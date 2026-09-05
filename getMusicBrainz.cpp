/*
	getMusicBrainz.cpp - MusicBrainz WS/2 fetch/parse and QA binding

	Overview:
		Caches WS/2 XML under cacheDir\musicBrainzCache, parses it with tinyxml2 into
		mbInfo* structs, then cQuestionAnswering::dbSearchMusicBrainz matches
		artist/label/release roles and verb triggers in a question against those
		hits and pushes winner names as cSource objects.

	Pipeline position:
		On-demand during question answering (not novel initialization).

	Key entry points:
		- getMusicBrainzPage - cache or HTTP GET, write cache
		- getReleases / getArtists / getRecordings / getLabels - XML -> vectors
		- absorbReleases - walk <release-list>
		- filter / pushWhereEntities / pushEntities - bind hits to a source position
		- dbSearchMusicBrainz / matchOwnershipDbMusicBrainz - QA triggers
		- createObject overloads - synthesize objects from MusicBrainz names

	Dependencies:
		tinyxml2, cInternet::readPage, cacheDir, musicBrainzCache on disk.

	Notes / gotchas:
		Uses https:// and encodeURL (defined in createOntology.cpp) escapes the free-text
		search value before it is interpolated into the query string; the fixed field-name
		parameters (byWhatType etc.) are not encoded since callers only ever pass literals.
		pushWhereEntities still takes mbs by value (not a reference) on purpose: it mutates
		its local copy via filter()/getReleases() as scratch state without writing back into
		the caller's (parentSRG->mbs); switching to a reference would start persisting
		fetched/filtered results back into the caller across calls, which is a behavior
		change that needs author intent, not a mechanical fix - see the function comment.
		getReleaseGroup/getWork are stubs returning 0.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "time.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "tinyxml2.h"
#include <typeinfo>
extern const lpchar_t* cacheDir; // initialized and then not changed
void encodeURL(lpwstring winput, lpwstring& wencodedURL); // defined in createOntology.cpp
#define MAX_BUF 1024*1024
#define MB_BASE u"https://www.musicbrainz.org/ws/2/%s/?query=%s:%s"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
using namespace std;
#include "profile.h"
#include "getMusicBrainz.h"
#include "internet.h"
#include "QuestionAnswering.h"

// Loads WS/2 XML for entitySearchedFor/?query=entityTypeReturned:entity into buffer,
// preferring the musicBrainzCache file. Returns 0 on cache or successful fetch;
// GETPAGE_CANNOT_CREATE if the cache file cannot be opened after mkdir attempts.
int getMusicBrainzPage(lpwstring entitySearchedFor, lpwstring entityTypeReturned, lpwstring entity, lpwstring& buffer)
{
	lpchar_t path[4096];
	lp_wsprintf(path, u"%s\\musicBrainzCache\\_%s.%s.%s.xml", cacheDir, entitySearchedFor.c_str(), entityTypeReturned.c_str(), entity.c_str());
	convertIllegalChars(path + lp_strlen(cacheDir) + lp_strlen(u"\\musicBrainzCache\\"));
	distributeToSubDirectories(path, lp_strlen(cacheDir) + lp_strlen(u"\\musicBrainzCache\\"), false);
	buffer.clear();
	// heap-allocated (tmalloc/tfree) rather than a ~2 MiB stack array
	lpchar_t* cBuffer = (lpchar_t*)tmalloc(MAX_BUF * sizeof(lpchar_t));
	if (!cBuffer)
		return cInternet::GETPAGE_CANNOT_CREATE;
	int actualLenInBytes;
	bool cacheHit = !getPath(path, cBuffer, MAX_BUF, actualLenInBytes);
	if (cacheHit)
	{
		cBuffer[actualLenInBytes / sizeof(cBuffer[0])] = 0;
		buffer = cBuffer;
	}
	tfree(MAX_BUF * sizeof(lpchar_t), cBuffer);
	if (cacheHit)
	{
		lplog(LOG_WHERE, u"MUSICBRAINZ:searchEntity=%s returnEntityType=%s entity=%s:\n%.600s", entitySearchedFor.c_str(), entityTypeReturned.c_str(), entity.c_str(), buffer.c_str());
		return 0;
	}
	lpchar_t str[1024];
	lpwstring uentity;
	encodeURL(entity, uentity); // entity is a free-text search value (may contain spaces/'&'/etc); entitySearchedFor/entityTypeReturned are always fixed literal field names
	// https://www.musicbrainz.org/ws/2/release/?query=artist:Jay-Z
	lp_wsprintf(str, MB_BASE, entitySearchedFor.c_str(), entityTypeReturned.c_str(), uentity.c_str());
	int ret;
	if (ret = cInternet::readPage(str, buffer)) return ret;
	//lplog(LOG_WHERE, u"TRACEOPEN %s %s", path, LP_TEXT(__func__).c_str());
	int fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0)
	{
		path[lp_strlen(cacheDir) + lp_strlen(u"\\musicBrainzCache\\") + 1] = 0;
		if (lp_wmkdir(path) < 0 && errno == ENOENT)
			lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
		path[lp_strlen(cacheDir) + lp_strlen(u"\\musicBrainzCache\\") + 1] = '\\';
		path[lp_strlen(cacheDir) + lp_strlen(u"\\musicBrainzCache\\") + 3] = 0;
		if (lp_wmkdir(path) < 0 && errno == ENOENT)
			lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
		path[lp_strlen(cacheDir) + lp_strlen(u"\\musicBrainzCache\\") + 3] = '\\';
		if ((fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) < 0)
		{
			lplog(LOG_ERROR, u"ERROR:Cannot create path %s - %S (10).", path, sys_errlist[errno]);
			return cInternet::GETPAGE_CANNOT_CREATE;
		}
	}
	::write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
	::close(fd);
	lplog(LOG_WHERE, u"MUSICBRAINZ:searchEntity=%s returnEntityType=%s entity=%s:\n%s", entitySearchedFor.c_str(), entityTypeReturned.c_str(), entity.c_str(), buffer.c_str());
	return 0;
}


// Debug-prints one XML element and its attributes at indent offset; terminate true emits />.
void printElement(int offset, tinyxml2::XMLElement* element, bool terminate)
{
	LFS
		printf("%*s<%s ", offset * 2, " ", element->Value());
	for (const tinyxml2::XMLAttribute* a = element->FirstAttribute(); a != NULL; a = a->Next())
		printf("%s=\"%s\" ", a->Name(), a->Value());
	if (terminate)
		printf("/>\n");
	else
		printf(">\n");
}

// Recursively debug-prints an XML subtree. childNodeAlreadyPrinted skips reprinting a
// first child that printElement already emitted (attribute-bearing elements).
void printNode(tinyxml2::XMLNode* node, int offset, bool childNodeAlreadyPrinted)
{
	LFS
		for (; node != NULL; node = node->NextSibling())
		{
			tinyxml2::XMLNode* firstChild = node->FirstChild();
			if (!childNodeAlreadyPrinted)
			{
				if (firstChild && firstChild->NextSibling() == NULL && typeid(*firstChild) == typeid(tinyxml2::XMLText))
				{
					printf("%*s<%s>%s</%s>\n", offset * 2, " ", node->Value(), firstChild->Value(), node->Value());
					continue;
				}
				else if (typeid(*node) == typeid(tinyxml2::XMLElement) && (((tinyxml2::XMLElement*)node)->FirstAttribute()))
					printElement(offset, (tinyxml2::XMLElement*)node, firstChild == NULL);
				else
					printf("%*s<%s>\n", offset * 2, " ", node->Value());
			}
			childNodeAlreadyPrinted = false;
			bool firstNodePrinted = false;
			if (firstNodePrinted = firstChild && firstChild == node->FirstChildElement() && ((tinyxml2::XMLElement*)firstChild)->FirstAttribute())
				printElement(offset, (tinyxml2::XMLElement*)firstChild, firstChild->FirstChild() == NULL && firstChild->LastChild() == NULL);
			if (node->FirstChildElement() != NULL)
			{
				printNode(firstChild, offset + 2, firstNodePrinted);
				printf("%*s</%s>\n", offset * 2, " ", node->Value());
			}
		}
}

// Debug-prints tag names and (after children) attributes; used to dump WS/2 shape.
void structurePrintNode(tinyxml2::XMLNode* node, int offset)
{
	LFS
		for (; node != NULL; node = node->NextSibling())
		{
			printf("%*s<%s>\n", offset * 2, " ", node->Value());
			structurePrintNode(node->FirstChild(), offset + 2);
			if (typeid(*node) == typeid(tinyxml2::XMLElement))
				for (const tinyxml2::XMLAttribute* a = ((tinyxml2::XMLElement*)node)->FirstAttribute(); a != NULL; a = a->Next())
					printf("%s=\"%s\" ", a->Name(), a->Value());
		}
}

// mTW(str, t) if str is non-NULL, else u"". t is the conversion scratch buffer.
lpwstring mTWNull(const char* str, lpwstring& t)
{
	LFS
		if (str != NULL)
			return mTW(str, t);
		else
			return u"";
}

// Adjacent-release dedup key: titles are equal.
bool isDuplicateByName(mbInfoReleaseType& t1, mbInfoReleaseType& t2)
{
	LFS
		return (t1.title == t2.title);
}

// Walks each <release> under releaseListHandle into mbs. Skips a hit whose title equals
// the last pushed title when filterNameDuplicates is set.
void absorbReleases(tinyxml2::XMLHandle& releaseListHandle, vector <mbInfoReleaseType>& mbs, bool filterNameDuplicates)
{
	LFS
		for (tinyxml2::XMLHandle node = releaseListHandle.FirstChildElement("release"); node.ToNode() != NULL; node = node.NextSiblingElement("release"))
		{
			mbInfoReleaseType mb;
			lpwstring t;
			if (node.ToElement()->FindAttribute("id"))
				mb.releaseId = mTWNull(node.ToElement()->FindAttribute("id")->Value(), t);
			if (node.FirstChildElement("title").ToElement())
				mb.title = mTWNull(node.FirstChildElement("title").ToElement()->GetText(), t);
			if (node.FirstChildElement("status").ToElement())
				mb.status = mTWNull(node.FirstChildElement("status").ToElement()->GetText(), t);
			if (node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").ToElement() &&
				node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").ToElement()->FindAttribute("id"))
				mb.artistId = mTWNull(node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").ToElement()->FindAttribute("id")->Value(), t);
			if (node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").FirstChildElement("name").ToElement())
				mb.artistName = mTWNull(node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").FirstChildElement("name").ToElement()->GetText(), t);
			if (node.FirstChildElement("release-group").ToElement())
			{
				if (node.FirstChildElement("release-group").ToElement()->FindAttribute("id"))
					mb.releaseGroupId = mTWNull(node.FirstChildElement("release-group").ToElement()->FindAttribute("id")->Value(), t);
				if (node.FirstChildElement("release-group").ToElement()->FindAttribute("type"))
					mb.releaseGroupType = mTWNull(node.FirstChildElement("release-group").ToElement()->FindAttribute("type")->Value(), t);
			}
			if (node.FirstChildElement("date").ToElement())
				mb.date = mTWNull(node.FirstChildElement("date").ToElement()->GetText(), t);
			if (node.FirstChildElement("country").ToElement())
				mb.country = mTWNull(node.FirstChildElement("country").ToElement()->GetText(), t);
			if (node.FirstChildElement("label-info-list").FirstChildElement("label-info").FirstChildElement("label").ToElement() &&
				node.FirstChildElement("label-info-list").FirstChildElement("label-info").FirstChildElement("label").ToElement()->FindAttribute("id"))
				mb.labelId = mTWNull(node.FirstChildElement("label-info-list").FirstChildElement("label-info").FirstChildElement("label").ToElement()->FindAttribute("id")->Value(), t);
			if (node.FirstChildElement("label-info-list").FirstChildElement("label-info").FirstChildElement("label").FirstChildElement("name").ToElement())
				mb.labelName = mTWNull(node.FirstChildElement("label-info-list").FirstChildElement("label-info").FirstChildElement("label").FirstChildElement("name").ToElement()->GetText(), t);
			if (mbs.empty() || !filterNameDuplicates || !isDuplicateByName(mbs[mbs.size() - 1], mb))
				mbs.push_back(mb);
		}
}

// Fetches /release/?query=byWhatType:what and fills mb. HTTP/parse errors are ignored (returns 0).
int getReleases(lpwstring byWhatType, lpwstring what, vector <mbInfoReleaseType>& mb, bool filterNameDuplicates)
{
	LFS
		lpwstring buffer;
	getMusicBrainzPage(u"release", byWhatType.c_str(), what, buffer); // int retCode=
	tinyxml2::XMLDocument doc;
	string cbuffer;
	wTM(buffer, cbuffer);
	int errorCode = doc.Parse(cbuffer.c_str());
	if (errorCode == 0)
	{
		tinyxml2::XMLHandle docHandle(&doc);
		tinyxml2::XMLHandle firstChildElement = docHandle.FirstChildElement("metadata").FirstChildElement("release-list");
		absorbReleases(firstChildElement, mb, filterNameDuplicates);
	}
	return 0;
}

// Adjacent-artist dedup key: MusicBrainz artist IDs are equal (name of the helper is misleading).
bool isDuplicateByName(mbInfoArtistType& t1, mbInfoArtistType& t2)
{
	LFS
		return (t1.artistId == t2.artistId);
}



// Adjacent-recording dedup key: titles are equal.
bool isDuplicateByName(mbInfoRecordingType& t1, mbInfoRecordingType& t2)
{
	LFS
		return (t1.title == t2.title);
}

/*
<metadata xmlns="http://musicbrainz.org/ns/mmd-2.0#" xmlns:ext="http://musicbrainz.org/ns/ext#-2.0">
		<recording-list offset="0" count="1">
				<recording id="0b382a13-32f0-4743-9248-ba5536a6115e" ext:score="100">
						<title>King Fred</title>
						<length>160000</length>
						<artist-credit>
								<name-credit>
										<artist id="f52f7a92-d495-4d32-89e7-8b1e5b8541c8">
												<name>Too Much Joy</name>
												<sort-name>Too Much Joy</sort-name>
										</artist>
								</name-credit>
						 </artist-credit>
						 <release-list>
						 </release-list>
						<puid-list>
								<puid id="1d9e8ed6-3893-4d3b-aa7d-72e79609e386"/>
						</puid-list>
				</recording>
		</recording-list>
</metadata>
*/

// Adjacent-label dedup key: labelName strings are equal.
bool isDuplicateByName(mbInfoLabelType& t1, mbInfoLabelType& t2)
{
	LFS
		return (t1.labelName == t2.labelName);
}

/*
<metadata xmlns="http://musicbrainz.org/ns/mmd-2.0#" xmlns:ext="http://musicbrainz.org/ns/ext#-2.0">
		<compactLabel-list offset="0" count="1">
				<compactLabel type="original production" id="d2c296e3-10a4-4ba9-97b9-5620ff8a3ce0">
				<name>Devil's Records</name>
				<sort-name>Devil's Records</sort-name>
				<alias-list>
						<alias>Devils Records</alias>
						<alias>Devil Records</alias>
				</alias-list>
				</compactLabel>
	 </compactLabel-list>
</metadata>
*/


// Logs every field of one release hit at LOG_WHERE.
void printMBInfo(mbInfoReleaseType& mbi)
{
	lplog(LOG_WHERE, u"MUSICBRAINZ  releaseId=%s title=%s status=%s artistId=%s artistName=%s releaseGroupId=%s releaseGroupType=%s date=%s country=%s labelName=%s labelId=%s",
		mbi.releaseId.c_str(), mbi.title.c_str(), mbi.status.c_str(), mbi.artistId.c_str(), mbi.artistName.c_str(), mbi.releaseGroupId.c_str(), mbi.releaseGroupType.c_str(), mbi.date.c_str(), mbi.country.c_str(), mbi.labelName.c_str(), mbi.labelId.c_str());
}


// Erases hits whose artistName/labelName/title (per byWhatType) is not exactly 'what'.
// Returns the surviving count (0 = caller should re-query).
int filter(lpwstring byWhatType, lpwstring what, vector <mbInfoReleaseType>& mbs)
{
	for (vector <mbInfoReleaseType>::iterator mbi = mbs.begin(); mbi != mbs.end(); )
	{
		lpwstring filterName;
		if (byWhatType == u"artist")
			filterName = mbi->artistName;
		else if (byWhatType == u"label")
			filterName = mbi->labelName;
		else if (byWhatType == u"release")
			filterName = mbi->title;
		//lplog(LOG_WHERE, u"filtering musicBrainz results: %s: %s=%s?", byWhatType.c_str(),what.c_str(), filterName.c_str());
		if (filterName != what)
			mbi = mbs.erase(mbi);
		else
			mbi++;
	}
	lplog(LOG_WHERE, u"filtering musicBrainz results: %s: %s [%s]", byWhatType.c_str(), what.c_str(), (mbs.empty()) ? u"NOT FOUND" : u"FOUND");
	return mbs.size();
}

// Binds MusicBrainz hits to m[where]. If whatWhere has no objectMatches, filters/fetches
// by the surface string and pushEntities. Otherwise votes across each match's hits and
// creates winner-name objects of the default class for matchEntityType.
// mbs is intentionally taken by value, not const&: filter()/getReleases() mutate this local
// copy in place as scratch state (erase non-matches, append fresh fetches), and that is not
// meant to write back into the caller's vector (parentSRG->mbs). Passing by value costs a
// copy per call, but switching to a plain reference would start persisting per-call
// filtering/fetches back into parentSRG->mbs across the multiple calls in
// dbSearchMusicBrainzSearchType, which is a behavior change - needs author intent, not a
// blind "pass by reference" mechanical fix.
bool cSource::pushWhereEntities(lpchar_t* derivation, int where, lpwstring matchEntityType, lpwstring byWhatType, int whatWhere, bool filterNameDuplicates, vector <mbInfoReleaseType> mbs)
{
	LFS
		unordered_map <lpwstring, int > mapCount;
	lpwstring logres;
	if (m[whatWhere].objectMatches.empty())
	{
		if (filter(byWhatType, whereString(whatWhere, logres, true), mbs) <= 0)
			getReleases(byWhatType, logres, mbs, filterNameDuplicates);
		for (auto mbi : mbs)
			printMBInfo(mbi);
		return pushEntities(derivation, where, matchEntityType, mbs);
	}
	OC defaultClass = NAME_OBJECT_CLASS;
	for (unsigned int om = 0; om < m[whatWhere].objectMatches.size(); om++)
	{
		vector <mbInfoReleaseType> filteredMbs = mbs;
		if (filter(byWhatType, objectString(m[whatWhere].objectMatches[om].object, logres, true), filteredMbs) <= 0)
			getReleases(byWhatType, objectString(m[whatWhere].objectMatches[om].object, logres, true), filteredMbs, filterNameDuplicates);
		for (auto mbi : filteredMbs)
			printMBInfo(mbi);
		if (matchEntityType == u"artist")
		{
			defaultClass = NAME_OBJECT_CLASS;
			for (unsigned int I = 0; I < filteredMbs.size(); I++)
				mapCount[filteredMbs[I].artistName]++;
		}
		else if (matchEntityType == u"label")
		{
			defaultClass = NON_GENDERED_BUSINESS_OBJECT_CLASS;
			for (unsigned int I = 0; I < filteredMbs.size(); I++)
				mapCount[filteredMbs[I].labelName]++;
		}
		else if (matchEntityType == u"release")
		{
			defaultClass = NON_GENDERED_NAME_OBJECT_CLASS;
			for (unsigned int I = 0; I < filteredMbs.size(); I++)
				mapCount[filteredMbs[I].title]++;
		}
		else
			continue;
	}
	vector <lpwstring> winners;
	int gc = 0;
	for (unordered_map <lpwstring, int >::iterator mi = mapCount.begin(), miEnd = mapCount.end(); mi != miEnd; mi++)
	{
		if (gc < mi->second && mi->first.length()>0)
		{
			winners.clear();
			gc = mi->second;
		}
		if (gc == mi->second)
			winners.push_back(mi->first);
	}
	for (unsigned int w = 0; w < winners.size(); w++)
		if (winners[w].size() > 0)
			m[where].objectMatches.push_back(createObject(derivation, winners[w], defaultClass));
	return winners.size() > 0;
}

// Creates one object per hit (artistName / labelName / title) and appends to m[where].objectMatches.
// Returns true if any object was added.
bool cSource::pushEntities(lpchar_t* derivation, int where, lpwstring matchEntityType, vector <mbInfoReleaseType>& mbs)
{
	LFS
		unsigned int originalSize = m[where].objectMatches.size();
	if (matchEntityType == u"artist")
	{
		for (unsigned int I = 0; I < mbs.size(); I++)
		{
			cOM object = createObject(derivation, mbs[I].artistName, NAME_OBJECT_CLASS);
			m[where].objectMatches.push_back(object);
		}
	}
	if (matchEntityType == u"label")
	{
		for (unsigned int I = 0; I < mbs.size(); I++)
		{
			cOM object = createObject(derivation, mbs[I].labelName, NON_GENDERED_BUSINESS_OBJECT_CLASS);
			m[where].objectMatches.push_back(object);
		}
	}
	if (matchEntityType == u"release")
	{
		for (unsigned int I = 0; I < mbs.size(); I++)
		{
			cOM object = createObject(derivation, mbs[I].title, NON_GENDERED_NAME_OBJECT_CLASS);
			m[where].objectMatches.push_back(object);
		}
	}
	return originalSize < m[where].objectMatches.size();
}

set <lpwstring> labelMatchList = { u"label",u"company",u"studio",u"conglomerate" };
set <lpwstring> artistMatchList = { u"artist",u"singer",u"songwriter",u"lyricist",u"composer",u"lyrist",u"musician",u"songsmith" };
set <lpwstring> releaseMatchList = { u"release",u"record",u"CD",u"album",u"recording",u"song",u"rap",u"compilation",u"track",u"disc",u"title" };

// If firstWhere/secondWhere match the artist|label|release word lists (or object class)
// and the verb is in matchVerbsList, pushWhereEntities on whichever side is the question
// type and record a dbMusicBrainz answer. Returns true if any side produced a match.
bool cQuestionAnswering::dbSearchMusicBrainzSearchType(cSource* questionSource, lpchar_t* derivation, cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs,
	int firstWhere, lpwstring firstMatchListType, int secondWhere, lpwstring secondMatchListType, set <lpwstring>& matchVerbsList)
{
	LFS
		bool foundMatch = false;
	set <lpwstring> firstMatchList;
	int firstObjectClass;
	if (firstMatchListType == u"artist")
	{
		firstMatchList = artistMatchList;
		firstObjectClass = NAME_OBJECT_CLASS;
	}
	else if (firstMatchListType == u"label")
	{
		firstMatchList = labelMatchList;
		firstObjectClass = NON_GENDERED_BUSINESS_OBJECT_CLASS;
	}
	else if (firstMatchListType == u"release")
	{
		firstMatchList = releaseMatchList;
		firstObjectClass = NON_GENDERED_NAME_OBJECT_CLASS;
	}
	else
		return false;
	set <lpwstring> secondMatchList;
	int secondObjectClass;
	if (secondMatchListType == u"artist")
	{
		secondMatchList = artistMatchList;
		secondObjectClass = NAME_OBJECT_CLASS;
	}
	else if (secondMatchListType == u"label")
	{
		secondMatchList = labelMatchList;
		secondObjectClass = NON_GENDERED_BUSINESS_OBJECT_CLASS;
	}
	else if (secondMatchListType == u"release")
	{
		secondMatchList = releaseMatchList;
		secondObjectClass = NON_GENDERED_NAME_OBJECT_CLASS;
	}
	else
		return false;
	if (questionSource->matchedList(firstMatchList, firstWhere, firstObjectClass, u"first") && questionSource->matchedList(secondMatchList, secondWhere, secondObjectClass, u"second") &&
		questionSource->matchedList(matchVerbsList, parentSRG->whereVerb, -1, u"verb"))
	{
		lpwstring logres;
		if (questionSource->inObject(firstWhere, parentSRG->whereQuestionType) && questionSource->pushWhereEntities(derivation, firstWhere, firstMatchListType, secondMatchListType, secondWhere, true, parentSRG->mbs))
		{
			answerSRGs.push_back(cAS(u"dbMusicBrainz", questionSource, 1, 1000, firstMatchListType, NULL, 0, firstWhere, 0, 0, false, false, u"", u"", 0, 0, 0, NULL));
			answerSRGs[answerSRGs.size() - 1].finalAnswer = foundMatch = true;
		}
		if (questionSource->inObject(secondWhere, parentSRG->whereQuestionType) && questionSource->pushWhereEntities(derivation, secondWhere, secondMatchListType, firstMatchListType, firstWhere, true, parentSRG->mbs))
		{
			answerSRGs.push_back(cAS(u"dbMusicBrainz", questionSource, 1, 1000, secondMatchListType, NULL, 0, secondWhere, 0, 0, false, false, u"", u"", 0, 0, 0, NULL));
			answerSRGs[answerSRGs.size() - 1].finalAnswer = foundMatch = true;
		}
	}
	return foundMatch;
}

// triggers: as any object
//    compactLabel/production company/studio/music studio/media conglomerate
//    artist/singer/songwriter/lyricist/composer/lyrist/musician/songsmith
//    release/record/CD/album/recording/song/rap/CD/compilation/track/disc
// triggers: verb
//    signed/wrote/produced/distributed/released/written/created/made
// bindings:
// artist: subject 
//    compactLabel:prepobject verbs/prep: belong to/signed with   
//    release:object?  verbs: wrote/created/made + synonyms
// no ownership of artist known within this KB
// release: subject
//    artist:prepobject  verbs/prep: written/created/made by      
//    compactLabel:prepobject verbs/prep: owned by/distributed by/released by/released on/produce by  
// ownership of release: subject or object or prepobject:
//    artist:owner his/her releases (or any trigger synonym above)
//    compactLabel:owner its releases/etc
// compactLabel: subject
//    artist: object verbs: owned/signed
//    release: object verbs: owns/distributes/produces
// no ownership of compactLabel known within this KB

// True if m[where]'s mainEntry is in matchList or its object has objectClass. Logs the test.
// Returns false if where is out of range.
bool cSource::matchedList(set <lpwstring>& matchList, int where, int objectClass, const lpchar_t* fromWhere)
{
	LFS
		if (where < 0 || where >= m.size())
			return false;
	lpwstring tmpstr;
	// or Proper Noun, which can match artist, compactLabel or release. - non gendered business objects may not be capitalized (fix?)
	bool matched = matchList.find(m[where].getMainEntry()->first) != matchList.end() || (m[where].getObject() >= 0 && (objects[m[where].getObject()].objectClass == objectClass));
	lplog(LOG_WHERE, u"matchedDbBrainzList: %s in [%s] or class %s = %s [%s match type] - %s", m[where].getMainEntry()->first.c_str(), setString(matchList, tmpstr, u",").c_str(),
		(m[where].getObject() >= 0) ? getClass(objects[m[where].getObject()].objectClass).c_str() : u"-1", (objectClass >= 0) ? getClass(objectClass).c_str() : u"-1", fromWhere, (matched) ? u"matched" : u"NOT matched");
	return matched;
}

// Appends a copy of object, clears owner, and points m[object.begin] at the new object index.
void cSource::createObject(cObject object)
{
	objects.push_back(object);
	objects[objects.size() - 1].originalLocation = object.begin;
	objects[objects.size() - 1].setOwnerWhere(-1);
	m[object.begin].setObject(objects.size() - 1);
	m[object.begin].beginObjectPosition = object.begin;
	m[object.begin].endObjectPosition = object.end;
}

// Parses descriptor "w1|w2|...&...&principalWhereOffset" into the token stream via parseBuffer
// and wraps those tokens in a new cObject. Returns the new object index, or -1 if parseBuffer fails.
// string separated by |, followed by forms separated by &
int cSource::createObject(lpwstring derivation, lpwstring descriptor)
{
	vector <lpwstring> forms = splitString(descriptor, u'&'); // last entry is the principalWhereOffset
	lpwstring wordstr;
	for (auto word : splitString(forms[0], u'|'))
		wordstr += word + u" ";
	int begin = m.size();
	unsigned int unknownCount = 0;
	bookBuffer = (lpchar_t*)wordstr.c_str();
	bufferLen = wordstr.length();
	bufferScanLocation = 0;
	if (parseBuffer(derivation, unknownCount) < 0)
		return -1;
	int principalWhereOffset = lp_wtoi(forms[forms.size() - 1].c_str());
	cObject object;
	object.begin = begin;
	object.end = m.size();
	object.originalLocation = begin + principalWhereOffset;
	object.setOwnerWhere(-1);
	objects.push_back(object);
	m[object.originalLocation].beginObjectPosition = begin;
	m[object.originalLocation].endObjectPosition = m.size();
	m[object.originalLocation].setObject(objects.size() - 1);
	if (begin != object.originalLocation)
		m[begin].principalWherePosition = object.originalLocation;
	lpwstring logres;
	lplog(LOG_WHERE, u"%s:created object %s.", derivation.c_str(), objectString(objects.size() - 1, logres, false, false).c_str());
	return objects.size() - 1;
}

// Tokenizes wordstr with parseBuffer and builds a NAME/business/title object. For NAME_OBJECT_CLASS
// fills first/middle/last from the 1/2/3+ tokens. Returns cOM(objectIndex, 0), or cOM(ret, -1)
// if parseBuffer fails or produced no tokens.
cOM cSource::createObject(lpwstring derivation, lpwstring wordstr, OC objectClass)
{
	LFS
		unsigned int unknownCount = 0, originalSize = m.size();
	bookBuffer = (lpchar_t*)wordstr.c_str();
	bufferLen = wordstr.length();
	bufferScanLocation = 0;
	int ret = parseBuffer(derivation, unknownCount);
	if (ret < 0 || originalSize == m.size())
		return cOM(ret, -1);
	int whereObject = m.size() - 1;
	cObject newObject;
	newObject.objectClass = objectClass;
	newObject.begin = originalSize;
	newObject.end = m.size();
	if (objectClass == NAME_OBJECT_CLASS)
	{
		if (newObject.end - newObject.begin == 1)
			newObject.name.any = m[whereObject].word;
		else if (newObject.end - newObject.begin == 2)
		{
			newObject.name.first = m[whereObject - 1].word;
			newObject.name.last = m[whereObject].word;
		}
		else
		{
			newObject.name.first = m[originalSize].word;
			newObject.name.middle = m[originalSize + 1].word;
			newObject.name.last = m[whereObject].word;
		}
	}
	newObject.originalLocation = whereObject;
	newObject.setSubType(UNKNOWN_PLACE_SUBTYPE);
	objects.push_back(newObject);
	m[m.size() - 1].setObject(objects.size() - 1);
	m[whereObject].beginObjectPosition = originalSize;
	m[whereObject].endObjectPosition = m.size();
	m[whereObject].principalWherePosition = whereObject;
	return cOM(objects.size() - 1, 0);
}

// If whereObject is a gendered-owned "release/album/…" with possessive-determiner owners,
// queries MusicBrainz by each owner as artist and pushEntities the releases. Returns true
// if any owner produced a release object.
// add to objects if ownership of trigger
bool cQuestionAnswering::matchOwnershipDbMusicBrainzObject(cSource* questionSource, lpchar_t* derivation, int whereObject, vector <mbInfoReleaseType>& mbs)
{
	LFS
		if (whereObject < 0)
			return false;
	int o = questionSource->m[whereObject].getObject(), ow;
	bool ownershipMatched = false;
	if (o >= 0 && questionSource->m[whereObject].objectMatches.empty() && (ow = questionSource->objects[o].getOwnerWhere()) >= 0 && (questionSource->objects[o].ownerFemale || questionSource->objects[o].ownerMale) &&
		questionSource->matchedList(releaseMatchList, whereObject, NON_GENDERED_NAME_OBJECT_CLASS, u"ownership") &&
		questionSource->m[ow].queryWinnerForm(possessiveDeterminerForm) >= 0 && questionSource->m[ow].objectMatches.size() > 0)
	{
		// query musicBrainz for each owning object (Jay-Z), as an artist
		// his records - Jay-Z's records
		for (unsigned int om = 0; om < questionSource->m[ow].objectMatches.size(); om++)
		{
			lpwstring ownershipObject, lookingForObject;
			questionSource->whereString(whereObject, lookingForObject, true);
			questionSource->objectString(questionSource->m[ow].objectMatches[om].object, ownershipObject, true);
			lplog(LOG_WHERE, u"matchOwnershipDbMusicBrainzObject: artist: %s has what release: %s?", ownershipObject.c_str(), lookingForObject.c_str());
			getReleases(u"artist", ownershipObject, mbs, true);
			for (auto mbi : mbs)
				printMBInfo(mbi);
			// lpwstring matchEntityType, lpwstring byWhatType, lpwstring what
			ownershipMatched |= questionSource->pushEntities(derivation, whereObject, u"release", mbs);
		}
	}
	return ownershipMatched;
}

// Tries matchOwnershipDbMusicBrainzObject on every SRG role slot (controlling/subject/object/prep/…).
bool cQuestionAnswering::matchOwnershipDbMusicBrainz(cSource* questionSource, lpchar_t* derivation, cSyntacticRelationGroup* parentSRG)
{
	LFS
		return matchOwnershipDbMusicBrainzObject(questionSource, derivation, parentSRG->whereControllingEntity, parentSRG->mbs) ||
		matchOwnershipDbMusicBrainzObject(questionSource, derivation, parentSRG->whereSubject, parentSRG->mbs) ||
		matchOwnershipDbMusicBrainzObject(questionSource, derivation, parentSRG->whereObject, parentSRG->mbs) ||
		matchOwnershipDbMusicBrainzObject(questionSource, derivation, parentSRG->wherePrepObject, parentSRG->mbs) ||
		matchOwnershipDbMusicBrainzObject(questionSource, derivation, parentSRG->whereSecondaryObject, parentSRG->mbs) ||
		matchOwnershipDbMusicBrainzObject(questionSource, derivation, parentSRG->whereNextSecondaryObject, parentSRG->mbs);
}

// Tries each artist/label/release role+verb pattern (belong/signed, wrote/create, …) via
// dbSearchMusicBrainzSearchType. Returns true on the first pattern that produces an answer.
// example:what companies produce his records?
//   
bool cQuestionAnswering::dbSearchMusicBrainz(cSource* questionSource, lpchar_t* derivation, cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs)
{
	LFS
		lpwstring logres;
	// artist: subject 
	//    compactLabel:prepobject verbs/prep: belong to/signed with   
	set <lpwstring> artistLabelVerbs = { u"belong",u"signed" };
	if (dbSearchMusicBrainzSearchType(questionSource, derivation, parentSRG, answerSRGs, parentSRG->whereSubject, u"artist", parentSRG->wherePrepObject, u"label", artistLabelVerbs))
		return true;
	// artist: subject 
	//    release:object?  verbs: wrote/created/made + synonyms
	set <lpwstring> artistReleaseVerbs = { u"wrote",u"create",u"made" };
	if (dbSearchMusicBrainzSearchType(questionSource, derivation, parentSRG, answerSRGs, parentSRG->whereSubject, u"artist", parentSRG->whereObject, u"release", artistReleaseVerbs))
		return true;
	// release: subject
	//    artist:prepobject  verbs/prep: written/created/made by      
	set <lpwstring> releaseArtistVerbs = { u"write",u"create",u"make" };
	if (dbSearchMusicBrainzSearchType(questionSource, derivation, parentSRG, answerSRGs, parentSRG->whereSubject, u"release", parentSRG->wherePrepObject, u"artist", releaseArtistVerbs))
		return true;
	// 
	// release: subject
	// verbs: featured
	// artist:object  
	set <lpwstring> releaseArtist2Verbs = { u"feature" };
	if (dbSearchMusicBrainzSearchType(questionSource, derivation, parentSRG, answerSRGs, parentSRG->whereSubject, u"release", parentSRG->whereObject, u"artist", releaseArtist2Verbs))
		return true;
	// what song [release] was produced by George Martin [label]?
	// release: subject
	//    verbs/prep: owned by/distributed by/released by/released on/produce by  
	// compactLabel:prepobject 
	set <lpwstring> releaseLabelVerbs = { u"own",u"distribute",u"release",u"produce" };
	if (dbSearchMusicBrainzSearchType(questionSource, derivation, parentSRG, answerSRGs, parentSRG->whereSubject, u"release", parentSRG->wherePrepObject, u"label", releaseLabelVerbs))
		return true;
	// what company signed Elton John?
	// compactLabel: subject
	// verbs: owned/signed
	// artist: object 
	set <lpwstring> labelArtistVerbs = { u"own",u"sign" };
	if (dbSearchMusicBrainzSearchType(questionSource, derivation, parentSRG, answerSRGs, parentSRG->whereSubject, u"label", parentSRG->whereObject, u"artist", labelArtistVerbs))
		return true;
	// What company [label] produces his records [release]?
	// compactLabel: subject 
	// verbs: owns/distributes/produces
	// release: object 
	set <lpwstring> labelReleaseVerbs = { u"own",u"distribute",u"produce" };
	if (dbSearchMusicBrainzSearchType(questionSource, derivation, parentSRG, answerSRGs, parentSRG->whereSubject, u"label", parentSRG->whereObject, u"release", labelReleaseVerbs))
		return true;
	return false;
}

