#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <winsock.h>
#include "Winhttp.h"
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "time.h"
#include "source.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "tinyxml2.h"
#include <typeinfo>
extern wchar_t *cacheDir; // initialized and then not changed
#define MAX_BUF 1024*1024
#define MB_BASE L"http://www.musicbrainz.org/ws/2/%s/?query=%s:%s"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
using namespace std;
#include "profile.h"
#include "getMusicBrainz.h"
#include "internet.h"


int getMusicBrainzPage(wstring entitySearchedFor,wstring entityTypeReturned,wstring entity,wstring &buffer)
{ 
	wchar_t path[4096];
	wsprintf(path,L"%s\\musicBrainzCache\\_%s.%s.%s.xml",cacheDir,entitySearchedFor.c_str(),entityTypeReturned.c_str(),entity.c_str());
	convertIllegalChars(path+wcslen(cacheDir)+wcslen(L"\\musicBrainzCache\\"));
	distributeToSubDirectories(path,wcslen(cacheDir)+wcslen(L"\\musicBrainzCache\\"),false);
	buffer.clear();
	wchar_t cBuffer[MAX_BUF];
	int actualLenInBytes;
	if (!getPath(path,cBuffer,MAX_BUF,actualLenInBytes))
	{
		cBuffer[actualLenInBytes/sizeof(cBuffer[0])]=0;
		buffer=cBuffer;
		return 0;
	}
	wchar_t str[1024];
	// http://www.musicbrainz.org/ws/2/release/?query=artist:Jay-Z
	wsprintf(str,MB_BASE,entitySearchedFor.c_str(),entityTypeReturned.c_str(),entity.c_str());
	int ret;
	if (ret= Internet::readPage(str,buffer)) return ret;
	//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path, __FUNCTIONW__);
	int fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE); 
	if (fd<0)
	{
		path[wcslen(cacheDir)+wcslen(L"\\musicBrainzCache\\")+1]=0;
		_wmkdir(path);
		path[wcslen(cacheDir)+wcslen(L"\\musicBrainzCache\\")+1]='\\';
		path[wcslen(cacheDir)+wcslen(L"\\musicBrainzCache\\")+3]=0;
		_wmkdir(path);
		path[wcslen(cacheDir)+wcslen(L"\\musicBrainzCache\\")+3]='\\';
		if ((fd=_wopen(path,O_CREAT|O_RDWR|O_BINARY,_S_IREAD | _S_IWRITE ))<0) 
		{
			lplog(LOG_ERROR,L"ERROR:Cannot create path %s - %S (10).",path,sys_errlist[errno]);
			return Internet::GETPAGE_CANNOT_CREATE;
		}
	}
	_write(fd,buffer.c_str(),buffer.length()*sizeof(buffer[0]));
	_close(fd);
	return 0;
}


void printElement(int offset,tinyxml2::XMLElement *element,bool terminate)
{ 
	LFS
	printf("%*s<%s ",offset*2," ",element->Value());
	for (const tinyxml2::XMLAttribute *a = element->FirstAttribute(); a!=NULL; a=a->Next())
			printf("%s=\"%s\" ",a->Name(),a->Value());
	if (terminate)
		printf("/>\n");
	else
		printf(">\n");
}

void printNode(tinyxml2::XMLNode *node,int offset,bool childNodeAlreadyPrinted)
{ LFS
	for (; node!=NULL; node=node->NextSibling())
	{
		tinyxml2::XMLNode *firstChild=node->FirstChild();
		if (!childNodeAlreadyPrinted)
		{
			if (firstChild && firstChild->NextSibling()==NULL && typeid(*firstChild)==typeid(tinyxml2::XMLText))
			{
				printf("%*s<%s>%s</%s>\n",offset*2," ",node->Value(),firstChild->Value(),node->Value());
				continue;
			}
			else if (typeid(*node)==typeid(tinyxml2::XMLElement) && (((tinyxml2::XMLElement *) node)->FirstAttribute()))
				printElement(offset,(tinyxml2::XMLElement *)node,firstChild==NULL);
			else
				printf("%*s<%s>\n",offset*2," ",node->Value());
		}
		childNodeAlreadyPrinted=false;
		bool firstNodePrinted=false;
		if (firstNodePrinted=firstChild && firstChild==node->FirstChildElement() && ((tinyxml2::XMLElement *) firstChild)->FirstAttribute())
			printElement(offset,(tinyxml2::XMLElement *) firstChild,firstChild->FirstChild()==NULL && firstChild->LastChild()==NULL);
		if (node->FirstChildElement()!=NULL)
		{
			printNode(firstChild,offset+2,firstNodePrinted);
			printf("%*s</%s>\n",offset*2," ",node->Value());
		}
	}
}

void structurePrintNode(tinyxml2::XMLNode *node,int offset)
{ LFS
	for (; node!=NULL; node=node->NextSibling())
	{
		printf("%*s<%s>\n",offset*2," ",node->Value());
		structurePrintNode(node->FirstChild(),offset+2);
		if (typeid(*node)==typeid(tinyxml2::XMLElement))
			for (const tinyxml2::XMLAttribute *a = ((tinyxml2::XMLElement *)node)->FirstAttribute(); a!=NULL; a=a->Next())
				printf("%s=\"%s\" ",a->Name(),a->Value());
	}
}

wstring mTWNull(const char *str,wstring &t)
{ LFS
	if (str!=NULL)
		return mTW(str,t);
	else
		return L"";
}

bool isDuplicateByName(mbInfoReleaseType &t1,mbInfoReleaseType &t2)
{ LFS
	return (t1.title==t2.title);
}

void absorbReleases(tinyxml2::XMLHandle &releaseListHandle,vector <mbInfoReleaseType> &mbs,bool filterNameDuplicates)
{ LFS
	for (tinyxml2::XMLHandle node=releaseListHandle.FirstChildElement("release"); node.ToNode()!=NULL; node=node.NextSiblingElement("release"))
	{
		mbInfoReleaseType mb;
		wstring t;
		mb.releaseId=mTWNull(node.ToElement()->FindAttribute("id")->Value(),t);
		mb.title=mTWNull(node.FirstChildElement("title").ToElement()->GetText(),t);
		mb.status=mTWNull(node.FirstChildElement("status").ToElement()->GetText(),t);
		mb.artistId=mTWNull(node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").ToElement()->FindAttribute("id")->Value(),t);
		mb.artistName=mTWNull(node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").FirstChildElement("name").ToElement()->GetText(),t);
		mb.releaseGroupId=mTWNull(node.FirstChildElement("release-group").ToElement()->FindAttribute("id")->Value(),t);
		mb.releaseGroupType=mTWNull(node.FirstChildElement("release-group").ToElement()->FindAttribute("type")->Value(),t);
		mb.date=mTWNull(node.FirstChildElement("date").ToElement()->GetText(),t);
		mb.country=mTWNull(node.FirstChildElement("country").ToElement()->GetText(),t);
		mb.labelId=mTWNull(node.FirstChildElement("label-info-list").FirstChildElement("label-info").FirstChildElement("label").ToElement()->FindAttribute("id")->Value(),t);
		mb.labelName=mTWNull(node.FirstChildElement("label-info-list").FirstChildElement("label-info").FirstChildElement("label").FirstChildElement("name").ToElement()->GetText(),t);
		if (mbs.empty() || !filterNameDuplicates || !isDuplicateByName(mbs[mbs.size()-1],mb))
			mbs.push_back(mb);
	}
}

int getReleases(wstring byWhatType,wstring what,vector <mbInfoReleaseType> &mb,bool filterNameDuplicates)
{ LFS
	wstring buffer;
	getMusicBrainzPage(L"release",byWhatType.c_str(),what,buffer); // int retCode=
  tinyxml2::XMLDocument doc;
	string cbuffer;
	wTM(buffer,cbuffer);
  int errorCode=doc.Parse( cbuffer.c_str() );
	if (errorCode == 0)
	{
		tinyxml2::XMLHandle docHandle(&doc);
		tinyxml2::XMLHandle firstChildElement = docHandle.FirstChildElement("metadata").FirstChildElement("release-list");
		absorbReleases(firstChildElement,mb,filterNameDuplicates);
	}
	return 0;
}

bool isDuplicateByName(mbInfoArtistType &t1,mbInfoArtistType &t2)
{ LFS
	return (t1.artistId==t2.artistId);
}

int getArtists(wstring byWhatType,wstring what,vector <mbInfoArtistType> &mbs,bool filterNameDuplicates)
{ LFS
	wstring buffer;
	getMusicBrainzPage(L"artist",byWhatType.c_str(),what,buffer); // int retCode=
  tinyxml2::XMLDocument doc;
	string cbuffer;
	wTM(buffer,cbuffer);
  doc.Parse( cbuffer.c_str() ); // int errorCode=
	tinyxml2::XMLHandle docHandle( &doc );
	for (tinyxml2::XMLHandle node=docHandle.FirstChildElement("metadata").FirstChildElement("artist-list").FirstChildElement("artist"); node.ToNode()!=NULL; node=node.NextSiblingElement("artist"))
	{
		wstring t;
		mbInfoArtistType mb;
		mb.artistType=mTWNull(node.ToElement()->FindAttribute("type")->Value(),t);
		mb.artistId=mTWNull(node.ToElement()->FindAttribute("id")->Value(),t);
		mb.artistName=mTWNull(node.FirstChildElement("name").ToElement()->GetText(),t);
		for (tinyxml2::XMLHandle anode=node.FirstChildElement("alias-list").FirstChildElement("alias"); anode.ToNode()!=NULL; anode=anode.NextSiblingElement("alias"))
			mb.aliases.push_back(mTWNull(anode.ToElement()->GetText(),t));
		if (mbs.empty() || !filterNameDuplicates || !isDuplicateByName(mbs[mbs.size()-1],mb))
			mbs.push_back(mb);
	}
	return 0;
}

int getReleaseGroup(wstring releaseGroup)
{ LFS
	return 0;
}

bool isDuplicateByName(mbInfoRecordingType &t1,mbInfoRecordingType &t2)
{ LFS
	return (t1.title==t2.title);
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
int getRecordings(wstring byWhatType,wstring what,vector <mbInfoRecordingType> &mbs,bool filterNameDuplicates)
{ LFS
	wstring buffer;
	getMusicBrainzPage(L"recording",byWhatType.c_str(),what,buffer); // int retCode=
  tinyxml2::XMLDocument doc;
	string cbuffer;
	wTM(buffer,cbuffer);
  doc.Parse( cbuffer.c_str() ); // int errorCode=
	tinyxml2::XMLHandle docHandle( &doc );
	for (tinyxml2::XMLHandle node=docHandle.FirstChildElement("metadata").FirstChildElement("recording-list").FirstChildElement("recording"); node.ToNode()!=NULL; node=node.NextSiblingElement("recording"))
	{
		mbInfoRecordingType mb;
		wstring t;
		mb.recordingId=mTWNull(node.ToElement()->FindAttribute("id")->Value(),t);
		mb.title=mTWNull(node.FirstChildElement("title").ToElement()->GetText(),t);
		mb.artistId=mTWNull(node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").ToElement()->FindAttribute("id")->Value(),t);
		mb.artistName=mTWNull(node.FirstChildElement("artist-credit").FirstChildElement("name-credit").FirstChildElement("artist").FirstChildElement("name").ToElement()->GetText(),t);
		tinyxml2::XMLHandle firstChildElement = docHandle.FirstChildElement("metadata").FirstChildElement("release-list");
		absorbReleases(firstChildElement,mb.releases,filterNameDuplicates);
		if (mbs.empty() || !filterNameDuplicates || !isDuplicateByName(mbs[mbs.size()-1],mb))
			mbs.push_back(mb);
	}
	return 0;
}

bool isDuplicateByName(mbInfoLabelType &t1,mbInfoLabelType &t2)
{ LFS
	return (t1.labelName==t2.labelName);
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
int getLabels(wstring byWhatType,wstring what,vector <mbInfoLabelType> &mbs,bool filterNameDuplicates)
{ LFS
	wstring buffer;
	getMusicBrainzPage(L"label",byWhatType.c_str(),what,buffer); // int retCode=
  tinyxml2::XMLDocument doc;
	string cbuffer;
	wTM(buffer,cbuffer);
  doc.Parse( cbuffer.c_str() ); // int errorCode=
	tinyxml2::XMLHandle docHandle( &doc );
	for (tinyxml2::XMLHandle node=docHandle.FirstChildElement("metadata").FirstChildElement("label-list").FirstChildElement("label"); node.ToNode()!=NULL; node=node.NextSiblingElement("label"))
	{
		mbInfoLabelType mb;
		wstring t;
		mb.labelType=mTWNull(node.ToElement()->FindAttribute("type")->Value(),t);
		mb.labelId=mTWNull(node.ToElement()->FindAttribute("id")->Value(),t);
		mb.labelName=mTWNull(node.FirstChildElement("name").ToElement()->GetText(),t);
		for (tinyxml2::XMLHandle anode=node.FirstChildElement("alias-list").FirstChildElement("alias"); anode.ToNode()!=NULL; anode=anode.NextSiblingElement("alias"))
			mb.aliases.push_back(mTWNull(anode.ToElement()->GetText(),t));
		if (mbs.empty() || !filterNameDuplicates || !isDuplicateByName(mbs[mbs.size()-1],mb))
			mbs.push_back(mb);
	}
	return 0;
}

int getWork(wstring work)
{ LFS
	return 0;
}


bool Source::pushWhereEntities(wchar_t *derivation,int where,wstring matchEntityType,wstring byWhatType,int whatWhere,bool filterNameDuplicates)
{ LFS
	unordered_map <wstring,int > mapCount;
	wstring logres;
	if (m[whatWhere].objectMatches.empty())
		return pushEntities(derivation,where,matchEntityType,byWhatType,whereString(whatWhere,logres,true),filterNameDuplicates);
	OC defaultClass= NAME_OBJECT_CLASS;
	for (unsigned int om=0; om<m[whatWhere].objectMatches.size(); om++)
	{
		vector <mbInfoReleaseType> mbs;
		getReleases(byWhatType,objectString(m[whatWhere].objectMatches[om].object,logres,true),mbs,filterNameDuplicates);
		if (matchEntityType==L"artist")
		{
			defaultClass=NAME_OBJECT_CLASS;
			for (unsigned int I=0; I<mbs.size(); I++)
				mapCount[mbs[I].artistName]++;
		}
		if (matchEntityType==L"label")
		{
			defaultClass=NON_GENDERED_BUSINESS_OBJECT_CLASS;
			for (unsigned int I=0; I<mbs.size(); I++)
				mapCount[mbs[I].labelName]++;
		}
		if (matchEntityType == L"release")
		{
			defaultClass = NON_GENDERED_NAME_OBJECT_CLASS;
			for (unsigned int I = 0; I < mbs.size(); I++)
				mapCount[mbs[I].title]++;
		}
		else
			return 0;
	}
	vector <wstring> winners;
	int gc=0;
	for (unordered_map <wstring,int >::iterator mi=mapCount.begin(),miEnd=mapCount.end(); mi!=miEnd; mi++)
	{
		if (gc<mi->second && mi->first.length()>0)
		{
			winners.clear();
			gc=mi->second;
		}
		if (gc==mi->second)
			winners.push_back(mi->first);
	}
	for (unsigned int w=0; w<winners.size(); w++)
		if (winners[w].size()>0)
			m[where].objectMatches.push_back(createObject(derivation,winners[w],defaultClass));
	return !winners.empty();
}

bool Source::pushEntities(wchar_t *derivation, int where, wstring matchEntityType, wstring byWhatType, wstring what, bool filterNameDuplicates)
{
	LFS
	unsigned int originalSize=m[where].objectMatches.size();
	vector <mbInfoReleaseType> mbs;
	getReleases(byWhatType,what,mbs,filterNameDuplicates);
	if (matchEntityType == L"artist")
	{
		for (unsigned int I = 0; I < mbs.size(); I++)
		{
			cOM object = createObject(derivation, mbs[I].artistName, NAME_OBJECT_CLASS);
			m[where].objectMatches.push_back(object);
		}
	}
		if (matchEntityType == L"label")
		{
			for (unsigned int I = 0; I < mbs.size(); I++)
			{
				cOM object = createObject(derivation, mbs[I].labelName, NON_GENDERED_BUSINESS_OBJECT_CLASS);
				m[where].objectMatches.push_back(object);
			}
		}
	if (matchEntityType==L"release")
	{
		for (unsigned int I = 0; I < mbs.size(); I++)
		{
			cOM object = createObject(derivation, mbs[I].title, NON_GENDERED_NAME_OBJECT_CLASS);
			m[where].objectMatches.push_back(object);
		}
	}
	return originalSize<m[where].objectMatches.size();
}

const wchar_t *labelMatchList[]={L"label",L"company",L"studio",L"conglomerate",NULL };
const wchar_t *artistMatchList[]={L"artist",L"singer",L"songwriter",L"lyricist",L"composer",L"lyrist",L"musician",L"songsmith",NULL  };
const wchar_t *releaseMatchList[]={L"release",L"record",L"CD",L"album",L"recording",L"song",L"rap",L"CD",L"compilation",L"track",L"disc",NULL  };

bool Source::dbSearchMusicBrainzSearchType(wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs,
	int firstWhere, wstring firstMatchListType, int secondWhere, wstring secondMatchListType, const wchar_t *matchVerbsList[])
{
	LFS
	bool foundMatch = false;
	const wchar_t **firstMatchList;
	int firstObjectClass;
	if (firstMatchListType == L"artist")
	{
		firstMatchList = artistMatchList;
		firstObjectClass = NAME_OBJECT_CLASS;
	}
	else if (firstMatchListType==L"label")
	{
		firstMatchList = labelMatchList;
		firstObjectClass = NON_GENDERED_BUSINESS_OBJECT_CLASS;
	}
	else if (firstMatchListType == L"release")
	{
		firstMatchList = releaseMatchList;
		firstObjectClass = NON_GENDERED_NAME_OBJECT_CLASS;
	}
	else
		return false;
	const wchar_t **secondMatchList;
	int secondObjectClass;
	if (secondMatchListType == L"artist")
	{
		secondMatchList = artistMatchList;
		secondObjectClass = NAME_OBJECT_CLASS;
	}
	else if (secondMatchListType == L"label")
	{
		secondMatchList = labelMatchList;
		secondObjectClass = NON_GENDERED_BUSINESS_OBJECT_CLASS;
	}
	else if (secondMatchListType == L"release")
	{
		secondMatchList = releaseMatchList;
		secondObjectClass = NON_GENDERED_NAME_OBJECT_CLASS;
	}
	else
		return false;
	if (matchedList(firstMatchList, firstWhere,firstObjectClass) && matchedList(secondMatchList, secondWhere,secondObjectClass) &&
			matchedList(matchVerbsList,parentSRI->whereVerb,-1))
	{
		wstring logres;
		if (inObject(firstWhere,parentSRI->whereQuestionType) && pushWhereEntities(derivation,firstWhere,firstMatchListType,secondMatchListType,secondWhere,true))
		{
			cAS cas(L"dbMusicBrainz",this,0,1000,firstMatchListType,NULL,0,firstWhere,0,0);
			answerSRIs.push_back(cas);
			foundMatch=true;
		}
		if (inObject(secondWhere,parentSRI->whereQuestionType) && pushWhereEntities(derivation,secondWhere,secondMatchListType,firstMatchListType,firstWhere,true))
		{
			cAS cas(L"dbMusicBrainz",this,0,1000,secondMatchListType,NULL,0,secondWhere,0,0);
			answerSRIs.push_back(cas);
			foundMatch=true;
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

bool Source::matchedList(const wchar_t *matchList[],int where,int objectClass)
{ LFS
	if (where<0)
		return false;
	for (int I=0; matchList[I]; I++)
		if (m[where].getMainEntry()->first==matchList[I])
			return true;
	// or Proper Noun, which can match artist, compactLabel or release. - non gendered business objects may not be capitalized (fix?)
	int oc;
	return m[where].getObject() >= 0 && ((oc = objects[m[where].getObject()].objectClass) == objectClass);
}

cOM Source::createObject(wstring derivation,wstring wordstr,OC objectClass)
{ LFS
	unsigned int unknownCount=0,originalSize=m.size();
	bookBuffer=(wchar_t *)wordstr.c_str();
	bufferLen=wordstr.length();
	bufferScanLocation=0;
	int ret = parseBuffer(derivation, unknownCount);
	if (ret<0 || originalSize == m.size())
		return cOM(ret,-1);
	int whereObject=m.size()-1;
	cObject newObject;
	newObject.objectClass=objectClass;
	newObject.begin=originalSize;
	newObject.end=m.size();
	if (objectClass==NAME_OBJECT_CLASS)
	{
		if (newObject.end-newObject.begin==1)
			newObject.name.any=m[whereObject].word;
		else if (newObject.end-newObject.begin==2)
		{
			newObject.name.first=m[whereObject-1].word;
			newObject.name.last=m[whereObject].word;
		}
		else 
		{
			newObject.name.first=m[originalSize].word;
			newObject.name.middle=m[originalSize+1].word;
			newObject.name.last=m[whereObject].word;
		}
	}
	newObject.originalLocation=whereObject;
	newObject.setSubType(UNKNOWN_PLACE_SUBTYPE);
	objects.push_back(newObject);
	m[m.size()-1].setObject(objects.size()-1);
	m[whereObject].beginObjectPosition=originalSize;
	m[whereObject].endObjectPosition=m.size();
  m[whereObject].principalWherePosition=whereObject;
	return cOM(objects.size() - 1, 0);
}

// add to objects if ownership of trigger
bool Source::matchOwnershipDbMusicBrainzObject(wchar_t *derivation,int whereObject)
{ LFS
	if (whereObject<0)
		return false;
	int o=m[whereObject].getObject(),ow;
	bool ownershipMatched=false;
	if (o>=0 && m[whereObject].objectMatches.empty() && (ow=objects[o].ownerWhere)>=0 && (objects[o].ownerFemale || objects[o].ownerMale) && 
		matchedList(releaseMatchList, whereObject, NON_GENDERED_NAME_OBJECT_CLASS) &&
			m[ow].queryWinnerForm(possessiveDeterminerForm)>=0 && m[ow].objectMatches.size()>0)
	{
		wstring logres;
		for (unsigned int om=0; om<m[ow].objectMatches.size(); om++)
			ownershipMatched|=pushEntities(derivation,whereObject,L"release",L"artist",objectString(m[ow].objectMatches[om].object,logres,true),true);
	}
	return ownershipMatched;
}

bool Source::matchOwnershipDbMusicBrainz(wchar_t *derivation,cSpaceRelation* parentSRI)
{ LFS
	return matchOwnershipDbMusicBrainzObject(derivation,parentSRI->whereControllingEntity) ||
		matchOwnershipDbMusicBrainzObject(derivation,parentSRI->whereSubject) ||
		matchOwnershipDbMusicBrainzObject(derivation,parentSRI->whereObject) ||
		matchOwnershipDbMusicBrainzObject(derivation,parentSRI->wherePrepObject) ||
		matchOwnershipDbMusicBrainzObject(derivation,parentSRI->whereSecondaryObject) ||
		matchOwnershipDbMusicBrainzObject(derivation,parentSRI->whereNextSecondaryObject);
}

// example:what companies produce his records?
//   
bool Source::dbSearchMusicBrainz(wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs)
{
	LFS
	wstring logres;
	// artist: subject 
	//    compactLabel:prepobject verbs/prep: belong to/signed with   
  const wchar_t *artistLabelVerbs[]={ L"belong",L"signed",NULL };
	if (dbSearchMusicBrainzSearchType(derivation,parentSRI,answerSRIs,parentSRI->whereSubject,L"artist",parentSRI->wherePrepObject,L"label",artistLabelVerbs))
		return true;
	// artist: subject 
	//    release:object?  verbs: wrote/created/made + synonyms
  const wchar_t *artistReleaseVerbs[]={ L"wrote",L"create",L"made",NULL };
	if (dbSearchMusicBrainzSearchType(derivation,parentSRI,answerSRIs,parentSRI->whereSubject,L"artist",parentSRI->whereObject,L"release",artistReleaseVerbs))
		return true;
	// release: subject
	//    artist:prepobject  verbs/prep: written/created/made by      
  const wchar_t *releaseArtistVerbs[]={ L"write",L"create",L"make",NULL };
	if (dbSearchMusicBrainzSearchType(derivation,parentSRI,answerSRIs,parentSRI->whereSubject,L"release",parentSRI->wherePrepObject,L"artist",releaseArtistVerbs))
		return true;
	// release: subject
	//    artist:object  verbs: featured      
  const wchar_t *releaseArtist2Verbs[]={ L"feature",NULL };
	if (dbSearchMusicBrainzSearchType(derivation,parentSRI,answerSRIs,parentSRI->whereSubject,L"release",parentSRI->whereObject,L"artist",releaseArtist2Verbs))
		return true;
	// release: subject
	//    compactLabel:prepobject verbs/prep: owned by/distributed by/released by/released on/produce by  
  const wchar_t *releaseLabelVerbs[]={ L"own",L"distribute",L"release",L"produce",NULL };
	if (dbSearchMusicBrainzSearchType(derivation,parentSRI,answerSRIs,parentSRI->whereSubject,L"release",parentSRI->wherePrepObject,L"label",releaseLabelVerbs))
		return true;
	// compactLabel: subject
	//    artist: object verbs: owned/signed
  const wchar_t *labelArtistVerbs[]={ L"own",L"sign",NULL };
	if (dbSearchMusicBrainzSearchType(derivation,parentSRI,answerSRIs,parentSRI->whereSubject,L"label",parentSRI->whereObject,L"artist",labelArtistVerbs))
		return true;
	// compactLabel: subject
	//    release: object verbs: owns/distributes/produces
  const wchar_t *labelReleaseVerbs[]={ L"own",L"distribute",L"produce",NULL };
	if (dbSearchMusicBrainzSearchType(derivation,parentSRI,answerSRIs,parentSRI->whereSubject,L"label",parentSRI->whereObject,L"release",labelReleaseVerbs))
		return true;
  return false;
}

