/*
	questionAnsweringWebSearch.cpp - Bing / Google Custom Search fallback and the
		query-string builder that turns a question SRG into search phrases.

	Overview:
		When the novel / DBpedia / Wikipedia pass finds no (or too few) answers,
		cQuestionAnswering asks this TU to (1) assemble query strings from the
		question's subject + verb + object + prep, (2) hit Google Custom Search or
		Bing Web Search v7 (results cached under getWebSearchCacheDir()\webSearchCache),
		(3) parse the JSON with yajl, (4) write snippets to disk and optionally
		download the full page, then (5) parse those texts as WEB_SEARCH_SOURCE_TYPE
		child sources and score them with analyzeQuestionFromSource(). Parallel mode
		queues the paths as REQUEST_TYPE rows and spins child lp.exe processes via
		startProcesses(); serial mode parses in-process.

	Pipeline position:
		Called from answerQuestionInSourceWebWikiSearch() /
		answerQuestionInSourceProximityMapWebSearch() / matchSubQueries() after the
		RDF / Wikipedia pass. Also hosts cSource helpers (appendObject / appendVerb /
		inObject) used while building the query strings.

	Key entry points:
		- getWebSearchQueries() / enhanceWebSearchQueries() - build / extend queries
		- getGoogleSearchJSON() / getBINGSearchJSON() - REST call + disk cache
		- extractGoogleWebSites() / extractBINGWebSites() - yajl walk of items[]
		- webSearchForQueryParallel() / webSearchForQuerySerial() - fetch, parse, score
		- cSource::appendObject/appendVerb/appendWord() - combinatorial query builder

	Key data structures / globals:
		- getGoogleCSEKey() / getGoogleCSEContext() / getBingSubscriptionKey()
			(envConfig.h) - API credentials, read from the environment
		- googleBaseWebSearchAddress / BINGBaseWebSearchAddress - REST endpoints
		- cQuestionAnswering::cSearchSource - one snippet or full-page parse request

	Dependencies:
		cInternet::getWebPath (internet.h), yajl_tree, MySQL (generateParseRequestSources
		in another TU), getWebSearchCacheDir() (envConfig.h; LP_WEBSEARCH_CACHE_DIR env
		var, falling back to the general.h WEBSEARCH_CACHEDIR compile-time default), WinHTTP.

	Notes / gotchas:
		- Snippet paths truncate at MAX_PATH-28 on a MAX_LEN (2048) buffer.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "time.h"
#include <errno.h>
#include <functional>
#include "profile.h"
#include "internet.h"
#include "QuestionAnswering.h"

#define MAX_PATH_LEN 2048
#define MAX_BUF 2000000
bool myquery(MYSQL* mysql, lpchar_t* q, bool allowFailure = false);
int generateParseRequestSources(MYSQL& mysql, vector <cQuestionAnswering::cSearchSource>::iterator pri);
int deleteGeneratedParseRequests(MYSQL& mysql);

/*
	 base64.cpp

	 Copyright (C) 2004-2008 Ren� Nyffenegger

	 This source code is provided 'as-is', without any express or implied
	 warranty. In no event will the author be held liable for any damages
	 arising from the use of this software.

	 Permission is granted to anyone to use this software for any purpose,
	 including commercial applications, and to alter it and redistribute it
	 freely, subject to the following restrictions:

	 1. The origin of this source code must not be misrepresented; you must not
			claim that you wrote the original source code. If you use this source code
			in a product, an acknowledgment in the product documentation would be
			appreciated but is not required.

	 2. Altered source versions must be plainly marked as such, and must not be
			misrepresented as being the original source code.

	 3. This notice may not be removed or altered from any source distribution.

	 Ren� Nyffenegger rene.nyffenegger@adp-gmbh.ch


*/

//#include <iostream>

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";


static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

// Standard base64 (Nyffenegger). Present for the old Azure DataMarket Bing
// handshake; the v7 path below uses an Ocp-Apim-Subscription-Key header instead.
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';

	}

	return ret;

}

// key sites:
// http://code.google.com/apis/console
// https://groups.google.com/a/googleproductforums.com/forum/#!forum/customsearch
// http://code.google.com/apis/customsearch/v1/getting_started.html
// https://groups.google.com/group/google-ajax-search-api/browse_frm/month/2011-12
// http://code.google.com/apis/customsearch/docs/api.html
// https://code.google.com/apis/console-help/#UnderstandingTrafficControls

// example REST API using custom search engine:
// the custom search engine (cx) ignores all information coming from wikipedia
// (since we already have freebase and dbPedia to reference directly)
// with the words WITH QUOTES "Paul Krugman writes for"
// https://www.googleapis.com/customsearch/v1?key=<LP_GOOGLE_CSE_KEY>&cx=<LP_GOOGLE_CSE_CX>&q=%22Paul+Krugman+writes+for%22
// Credentials are read from the environment (envConfig.h getGoogleCSEKey() /
// getGoogleCSEContext() / getBingSubscriptionKey()), not stored here.
lpwstring googleBaseWebSearchAddress = u"https://www.googleapis.com/customsearch/v1";
lpwstring BINGBaseWebSearchAddress = u"https://api.cognitive.microsoft.com/bing/v7.0/search";

void encodeURL(lpwstring winput, lpwstring& wencodedURL);
int flushString(lpwstring& buffer, lpchar_t* path);

/*
$top	Specifies the number of results to return.	&count=	50
(50 is the maximum)	https://api.datamarket.azure.com/Bing/Search/Web?Query=%27Xbox%27&$top=10

$skip	Specifies the offset requested for the starting point of results returned.	&offset=	0	https://api.datamarket.azure.com/Bing/Search/Web?Query=%27Xbox%27&$top=10&$skip=20

*/
// GET Bing Web Search v7 for 'object' (spaces encoded, %22 rewritten to %27 so
// the query is wrapped in single quotes). index>1 becomes &offset=. Results are
// cached by cInternet::getWebPath under webSearchCache. Returns getWebPath's
// errCode, or -1 if Bing replies with the "Query is not of type String" body.
int getBINGSearchJSON(int where, lpwstring object, lpwstring& buffer, lpwstring& filePathOut, int numWebSitesAskedFor, int index)
{
	LFS
		lpwstring uobject, numWebSitesAskedForStr;
	//replace(object.begin(),object.end(),u' ',u'+');
	encodeURL(object, uobject);
	// %27Paul%20Krugman%27%20criticized%20in%20%27his%20op-ed%20columns%27
	// %22Paul%2BKrugman%22%2Bcriticized%2Bin%2B%22his%2Bop%2Ded%2Bcolumns%22
	// re-encode %2B as %20, %22 as %27
	for (size_t pos = 0; (pos = uobject.find(u"%2B", pos)) != std::string::npos; pos += 3)
		uobject.replace(pos, 3, u"%20");
	for (size_t pos = 0; (pos = uobject.find(u"%22", pos)) != std::string::npos; pos += 3)
		uobject.replace(pos, 3, u"%27");
	if (uobject.find(u"%27") == lpwstring::npos)
		uobject = u"%27" + uobject + u"%27";
	//https://api.datamarket.azure.com/Bing/SearchWeb/Web?$format=json&Query=%27Xbox%27
	lpwstring webAddress = BINGBaseWebSearchAddress + u"?q=" + uobject + u"&responseFilter=webPages,Entities,News,RelatedSearches&mkt=en-US&setLang=en-US&promote=Webpages";
	if (index > 1)
	{
		lpwstring start;
		itos(index, start);
		webAddress += u"&offset=" + start;
	}
	if (numWebSitesAskedFor >= 0)
	{
		webAddress += u"&answerCount=" + itos(numWebSitesAskedFor, numWebSitesAskedForStr);
	}
	lpwstring headers = u"Ocp-Apim-Subscription-Key:" + getBingSubscriptionKey();
	int errCode = cInternet::getWebPath(where, webAddress, buffer, object + u"_BING", u"webSearchCache", filePathOut, headers, index, false, true);
	lplog(LOG_WEBSEARCH | LOG_WHERE | LOG_ERROR, u"searching BING: %s [%s] searchIndex=%d [%s]", object.c_str(), filePathOut.c_str(), index, webAddress.c_str());
	if (buffer == u"Parameter: Query is not of type String")
	{
		lplog(LOG_ERROR, u"Invalid query");
		return -1;
	}
	return errCode;
}

// GET Google Custom Search for 'object' (spaces first turned into '+', then URL-
// encoded). index>1 becomes &start=. The key and cx come from envConfig.h.
// Cached under webSearchCache. Returns getWebPath's errCode.
// cache google searches since this is faster and we are paying for them
// input object is not encoded, but should include quotes when needed.
int getGoogleSearchJSON(int where, lpwstring object, lpwstring& buffer, lpwstring& filePathOut, int numWebSitesAskedFor, int index)
{
	LFS
		lpwstring uobject, numWebSitesAskedForStr;
	replace(object.begin(), object.end(), u' ', u'+');
	encodeURL(object, uobject);
	// https://www.googleapis.com/customsearch/v1?key=<LP_GOOGLE_CSE_KEY>&cx=<LP_GOOGLE_CSE_CX>&q=%22Paul+Krugman+writes+for%22
	lpwstring start;
	if (index > 1)
	{
		itos(index, start);
		start = u"&start=" + start;
	}
	lpwstring webAddress = googleBaseWebSearchAddress + u"?key=" + getGoogleCSEKey() + start + u"&cx=" + getGoogleCSEContext() + u"&q=" + uobject + u"&num=" + itos(numWebSitesAskedFor, numWebSitesAskedForStr);
	lpwstring headers;
	int errCode = cInternet::getWebPath(where, webAddress, buffer, object, u"webSearchCache", filePathOut, headers, index, false, true);
	lplog(LOG_WEBSEARCH | LOG_WHERE | LOG_ERROR, u"searching Google: %s [%s] searchIndex=%d [%s]", object.c_str(), filePathOut.c_str(), index, webAddress.c_str());
	lplog(LOG_WHERE, NULL);
	return errCode;
}

// True if whereQuestionType is the token 'where' itself or lies inside that
// token's [beginObjectPosition, endObjectPosition) span (the -1 on begin lets
// an adjectival WH-word sitting just before the object still count).
bool cSource::inObject(int where, int whereQuestionType)
{
	LFS
		if (whereQuestionType >= 0 && where == whereQuestionType)
			return true;
	return (where >= 0 && whereQuestionType >= 0 && m[where].beginObjectPosition >= 0 && m[where].endObjectPosition >= 0 &&
		whereQuestionType >= m[where].beginObjectPosition - 1 && whereQuestionType < m[where].endObjectPosition);
}

// Append one prep+object span (from relPrep through the object's end) onto the
// last (or atNumPP-th) string in prepPhraseStrings, if case matches. Returns
// true when something was appended. atNumPP<0 copies the last string first so
// the caller can keep a no-PP variant; that path assumes the vector is non-empty.
// this is deliberately noncombinatorial - each prepositional phrase only has one predecessor
bool cSource::appendPrepositionalPhrase(int where, vector <lpwstring>& prepPhraseStrings, int relPrep, bool nonMixedCase, bool lowerCase, const lpchar_t* separator, int atNumPP)
{
	LFS
		int relObject = m[relPrep].getRelObject();
	if (relObject >= relPrep && m[relObject].endObjectPosition >= 0 && (m[relPrep].nextQuote == where || m[relPrep].relNextObject == where))
	{
		lpwstring oStr;
		oStr = separator;
		int lctlen = 1;
		getOriginalWord(relPrep, oStr, true);
		for (int I = relPrep + 1; I < m[relObject].endObjectPosition; I++)
		{
			oStr += separator;
			lctlen = oStr.length();
			getOriginalWord(I, oStr, true);
		}
		bool lc = iswlower(oStr[lctlen]) != 0;
		if (!nonMixedCase || ((lowerCase && lc) || (!lowerCase && !lc)))
		{
			int originalSize = prepPhraseStrings.size();
			if (atNumPP < 0)
				prepPhraseStrings.push_back(prepPhraseStrings[originalSize - 1]);
			else
				originalSize--;
			prepPhraseStrings[originalSize] += oStr;
			return true;
		}
	}
	return false;
}

// If the token after the object at 'where' is a preposition that points back
// here, walk the relPrep chain and accumulate matching-case PP strings starting
// from wsoStr. Returns the size of prepPhraseStrings. numWords is the token
// span of the longest PP that was kept.
int cSource::appendPrepositionalPhrases(int where, lpwstring& wsoStr, vector <lpwstring>& prepPhraseStrings, int& numWords, bool noMixedCase, const lpchar_t* separator, int atNumPP)
{
	LFS
		int relPrep;
	numWords = 0;
	if (where >= 0 && where < (signed)m.size() && (relPrep = m[where].endObjectPosition) >= 0 && relPrep < (signed)m.size() && m[relPrep].queryWinnerForm(prepositionForm) >= 0 && (m[relPrep].nextQuote == where || m[relPrep].relNextObject == where))
	{
		prepPhraseStrings.push_back(wsoStr);
		bool lowerCase = iswlower(wsoStr[0]) != 0;
		size_t lw = wsoStr.find_last_of(separator);
		int largestRelPrep = -1;
		if (lw != lpwstring::npos) lowerCase = iswlower(wsoStr[lw + 1]) != 0;
		for (int I = 0; (I < atNumPP || atNumPP < 0) && relPrep < (signed)m.size() && relPrep >= 0 && (m[relPrep].nextQuote == where || m[relPrep].relNextObject == where) && appendPrepositionalPhrase(where, prepPhraseStrings, relPrep, noMixedCase, lowerCase, separator, atNumPP); I++)
		{
			largestRelPrep = relPrep;
			relPrep = m[relPrep].relPrep;
		}
		if (largestRelPrep >= 0 && m[largestRelPrep].getRelObject() >= 0)
			numWords = m[m[largestRelPrep].getRelObject()].endObjectPosition - m[where].endObjectPosition;
	}
	return prepPhraseStrings.size();
}

// Push one or more surface forms of objects[object] at 'where' into wsoStrs.
// Names use cName::original with '+' separators. Other classes copy the token
// span unless alreadyDidPlainCopy is set (then non-name classes return -1 so
// a later alias does not duplicate the plain copy). 0 on success.
int cSource::getObjectStrings(int where, int object, vector <lpwstring>& wsoStrs, bool& alreadyDidPlainCopy)
{
	LFS
		lpwstring wsoStr;
	if (objects[object].objectClass == NAME_OBJECT_CLASS && (m[where].endObjectPosition - m[where].beginObjectPosition > 1 || objects[object].name.first != wNULL))
	{
		objects[object].name.original(wsoStr, u'+', true);
		wsoStrs.push_back(wsoStr);
	}
	else
	{
		int oc = objects[object].objectClass;
		if (alreadyDidPlainCopy &&
			oc != NAME_OBJECT_CLASS &&
			oc != GENDERED_GENERAL_OBJECT_CLASS &&
			oc != GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
			oc != GENDERED_DEMONYM_OBJECT_CLASS &&
			oc != NON_GENDERED_BUSINESS_OBJECT_CLASS &&
			oc != NON_GENDERED_NAME_OBJECT_CLASS)
			return -1;
		int begin = m[where].beginObjectPosition, len = m[where].endObjectPosition;
		for (int I = begin; I < len; I++)
			getOriginalWords(I, wsoStrs, I > begin);
		alreadyDidPlainCopy = true;
	}
	return 0;
}

// Combinatorial append of the object at 'where' onto every string in
// objectStrings. Skips the WH-span itself (returns -1) unless QTAFlag is set,
// in which case the WH-word is concatenated. Pronouns are dropped. Multi-word
// names also generate a quoted variant; attached PPs generate still more copies.
// Returns 0, or -1 when the position is the question object (noQuotes signal).
int cSource::appendObject(int64_t questionType, int whereQuestionType, vector <lpwstring>& objectStrings, int where)
{
	LFS
		if (where < 0) return 0;
	if (whereQuestionType == where || (whereQuestionType >= 0 && inObject(where, whereQuestionType)))
	{
		if (questionType & cQuestionAnswering::QTAFlag)
		{
			vector <lpwstring> saveStrings;
			saveStrings = objectStrings;
			int os = objectStrings.size();
			objectStrings.insert(objectStrings.end(), saveStrings.begin(), saveStrings.end());
			for (; os < objectStrings.size(); os++)
			{
				if (objectStrings[os].length() > 0)
					objectStrings[os] += u"+" + m[where].word->first;
				else
					objectStrings[os] = m[where].word->first;
			}
			return 0;
		}
		return -1;
	}
	vector <int> appendObjects;
	int o = m[where].getObject(), oc;
	if (o >= 0 && m[where].objectMatches.size() == 0 && (oc = objects[o].objectClass) != REFLEXIVE_PRONOUN_OBJECT_CLASS && oc != RECIPROCAL_PRONOUN_OBJECT_CLASS && oc != PRONOUN_OBJECT_CLASS)
		appendObjects.push_back(o);
	for (unsigned int oi = 0; oi < m[where].objectMatches.size(); oi++)
	{
		oc = objects[m[where].objectMatches[oi].object].objectClass;
		if (oc != REFLEXIVE_PRONOUN_OBJECT_CLASS && oc != RECIPROCAL_PRONOUN_OBJECT_CLASS && oc != PRONOUN_OBJECT_CLASS)
			appendObjects.push_back(m[where].objectMatches[oi].object);
	}
	lpwstring logres;
	lplog(LOG_WHERE, u"appendObjects: %d:objectMatches.size=%d:%d:%s", where, m[where].objectMatches.size(), appendObjects.size(), objectString(appendObjects, logres).c_str());
	bool alreadyDidPlainCopy = false, copyNeeded = false;
	vector <lpwstring> saveStrings;
	saveStrings = objectStrings;
	for (vector <int>::iterator oi = appendObjects.begin(), oiEnd = appendObjects.end(); oi != oiEnd; oi++)
	{
		vector <lpwstring> wsoStrs;
		if (getObjectStrings((o == *oi) ? where : objects[*oi].originalLocation, *oi, wsoStrs, alreadyDidPlainCopy) < 0)
			continue;
		unsigned int os = 0;
		if (copyNeeded)
		{
			os = objectStrings.size();
			objectStrings.insert(objectStrings.end(), saveStrings.begin(), saveStrings.end());
		}
		copyNeeded = true;
		for (vector <lpwstring>::iterator wsoStr = wsoStrs.begin(), wsoStrEnd = wsoStrs.end(); wsoStr != wsoStrEnd; wsoStr++)
		{
			//lplog(LOG_WHERE,u"%d:appendObjects: %d:%s",oi-appendObjects.begin(),wsoStr-wsoStrs.begin(),*wsoStr);
			for (; os < objectStrings.size(); os++)
			{
				if (objectStrings[os].length() > 0)
					objectStrings[os] += u"+" + *wsoStr;
				else
					objectStrings[os] = *wsoStr;
			}
			bool insertQuotes;
			if (insertQuotes = wsoStr->find(u'+') != lpwstring::npos)
			{
				os = objectStrings.size();
				objectStrings.insert(objectStrings.end(), saveStrings.begin(), saveStrings.end());
				for (; os < objectStrings.size(); os++)
				{
					if (objectStrings[os].length() > 0)
						objectStrings[os] += u"+\"" + *wsoStr + u"\"";
					else
						objectStrings[os] = u"\"" + *wsoStr + u"\"";
				}
			}
			vector <lpwstring> prepPhraseStrings;
			int numWords;
			if (m[where].relPrep >= 0 && m[where].relPrep < (signed)m.size() && m[m[where].relPrep].nextQuote == where && appendPrepositionalPhrases(where, *wsoStr, prepPhraseStrings, numWords, true, u"+", -1)>1)
			{
				for (unsigned int pp = 1; pp < prepPhraseStrings.size(); pp++)
				{
					os = objectStrings.size();
					objectStrings.insert(objectStrings.end(), saveStrings.begin(), saveStrings.end());
					//lplog(LOG_WHERE,u"%d: relPrep=%d m.size()=%d nextQuote=%d inserting pp=%s objectStrings.size()=%d",where,m[where].relPrep,m.size(),(m[where].relPrep>=0 && m[where].relPrep<(signed)m.size()) ? m[m[where].relPrep].nextQuote : -1,prepPhraseStrings[pp].c_str(),os);
					for (; os < objectStrings.size(); os++)
					{
						if (objectStrings[os].length() > 0)
							objectStrings[os] += u"+" + prepPhraseStrings[pp];
						else
							objectStrings[os] = prepPhraseStrings[pp];
					}
					os = objectStrings.size();
					objectStrings.insert(objectStrings.end(), saveStrings.begin(), saveStrings.end());
					//lplog(LOG_WHERE,u"%d: relPrep=%d m.size()=%d nextQuote=%d inserting pp=%s objectStrings.size()=%d",where,m[where].relPrep,m.size(),(m[where].relPrep>=0 && m[where].relPrep<(signed)m.size()) ? m[m[where].relPrep].nextQuote : -1,prepPhraseStrings[pp].c_str(),os);
					for (; os < objectStrings.size(); os++)
					{
						if (objectStrings[os].length() > 0)
							objectStrings[os] += u"+\"" + prepPhraseStrings[pp] + u"\"";
						else
							objectStrings[os] = u"\"" + prepPhraseStrings[pp] + u"\"";
					}
				}
			}
		}
	}
	for (unsigned int I = 0; I < objectStrings.size(); I++)
		lplog(LOG_WHERE, u"%d: objectString %d:%s", where, I, objectStrings[I].c_str());
	return 0;
}

// Return an inflected form of 'verb' whose tense matches tenseDesired, preferring
// a third-person form from Words.mainEntryMap. If verb is already past/present as
// requested, it is returned unchanged. subject==wNULL is treated as singular.
// PAST:VERB_PAST, VERB_PAST_PARTICIPLE,VERB_PAST_THIRD_SINGULAR,VERB_PAST_PLURAL
// PRESENT:VERB_PRESENT_THIRD_SINGULAR=128,VERB_PRESENT_FIRST_SINGULAR=256, VERB_PRESENT_PLURAL=2048,VERB_PRESENT_SECOND_SINGULAR=4096,VERB_PRESENT_PARTICIPLE
tIWMM cSource::getTense(tIWMM verb, tIWMM subject, int tenseDesired)
{
	LFS
		int inflectionFlags = verb->second.inflectionFlags, desiredInflectionFlags = 0;
	bool verbIsPast = ((inflectionFlags & (VERB_PAST | VERB_PAST_PARTICIPLE | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL)) > 0);
	bool verbIsPresent = ((inflectionFlags & (VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_PARTICIPLE)) > 0);
	bool tenseDesiredIsPast = ((tenseDesired & (VERB_PAST | VERB_PAST_PARTICIPLE | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL)) > 0);
	bool tenseDesiredIsPresent = ((tenseDesired & (VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_PARTICIPLE)) > 0);
	if ((verbIsPast && tenseDesiredIsPast) || (verbIsPresent && tenseDesiredIsPresent)) return verb;
	bool subjectIsSingular = subject == wNULL || (subject->second.inflectionFlags & SINGULAR);
	if (verbIsPast)
		desiredInflectionFlags = (subjectIsSingular) ? VERB_PRESENT_THIRD_SINGULAR : VERB_PRESENT_PLURAL;
	else
		desiredInflectionFlags = VERB_PAST;
	tIWMM verbMainEntry = (verb->second.mainEntry == wNULL) ? verb : verb->second.mainEntry;
	unordered_map <lpwstring, vector <tIWMM>>::iterator mEMI;
	// prefer third person verbs
	if (verbMainEntry != wNULL && (mEMI = Words.mainEntryMap.find(verbMainEntry->first)) != Words.mainEntryMap.end())
	{
		for (vector <tIWMM>::iterator mei = mEMI->second.begin(), meiEnd = mEMI->second.end(); mei != meiEnd; mei++)
		{
			if ((*mei)->second.inflectionFlags & desiredInflectionFlags)
			{
				return *mei;
			}
		}
	}
	return verb;
}

// Same lookup as the tIWMM overload, but starting from m[where]'s main entry and
// returning a search-query token. preferredVerb==VERB_PRESENT_FIRST_SINGULAR is
// treated as future and prefixed with "will+". Falls back to 'candidate'.
lpwstring cSource::getTense(int where, lpwstring candidate, int preferredVerb)
{
	LFS
		tIWMM mainEntry = m[where].getMainEntry();
	unordered_map <lpwstring, vector <tIWMM>>::iterator mEMI;
	// prefer third person verbs
	if (mainEntry != wNULL && (mEMI = Words.mainEntryMap.find(mainEntry->first)) != Words.mainEntryMap.end())
	{
		for (unsigned int me = 0; me < mEMI->second.size(); me++)
		{
			//int inflectionFlags=mEMI->second[me]->second.inflectionFlags;
			lpwstring verb = mEMI->second[me]->first;
			//lplog(u"%s:%d",verb.c_str(),inflectionFlags);
			if (mEMI->second[me]->second.inflectionFlags & preferredVerb)
			{
				if (preferredVerb == VERB_PRESENT_FIRST_SINGULAR)
					candidate = u"will+" + mEMI->second[me]->first;
				else
					candidate = mEMI->second[me]->first;
				break;
			}
		}
	}
	return candidate;
}
// Append a tensed query form of m[where] to every string in objectStrings.
// Passive (and not "by") becomes was/will+be/is+being + verb. Future becomes
// will+bare. A following object-less prep/adverb is glued on. Infinitive
// complements duplicate the current strings with a "to+V" / past-tense variant.
// Always returns 0.
// possibly adjust tense
int cSource::appendVerb(vector <lpwstring>& objectStrings, int where)
{
	LFS
		lpwstring candidate = m[where].word->first;
	int fullTense = m[where].verbSense;
	int tense = fullTense & VT_TENSE_MASK;
	//int relPrep = m[where].relPrep;// , msize = m.size();
	bool isPassive = (fullTense & VT_PASSIVE) == VT_PASSIVE && (m[where].relPrep < 0 || (m[where].relPrep >= 0 && m[m[where].relPrep].word->first != u"by"));
	int preferredVerb = 0;
	// was born
	if (isPassive)
	{
		preferredVerb = tense;
		if ((tense == VT_PAST) || (tense == VT_PAST_PERFECT) || (tense == VT_PRESENT_PERFECT))
		{
			candidate = u"was";
		}
		else if ((tense == VT_FUTURE) || (tense == VT_FUTURE_PERFECT))
		{
			candidate = u"will+be";
		}
		else if ((tense == VT_PRESENT))
		{
			candidate = u"is+being";
		}
		candidate += u"+" + m[where].word->first;
	}
	else
	{
		if ((tense == VT_PAST) || (tense == VT_PAST_PERFECT) || (tense == VT_PRESENT_PERFECT))
		{
			preferredVerb = VERB_PAST;
		}
		else if ((tense == VT_FUTURE) || (tense == VT_FUTURE_PERFECT))
		{
			preferredVerb = VERB_PRESENT_FIRST_SINGULAR;
		}
		else if ((tense == VT_PRESENT))
		{
			preferredVerb = VERB_PRESENT_THIRD_SINGULAR;
		}
		if (preferredVerb)
		{
			candidate = getTense(where, candidate, preferredVerb);
		}
	}
	if (where + 1 < (int)m.size() && m[where + 1].queryForm(prepositionForm) >= 0 &&
		(m[where + 1].queryWinnerForm(prepositionForm) >= 0 || m[where + 1].queryWinnerForm(adverbForm) >= 0) &&
		m[where + 1].getRelObject() < 0)
		candidate += u"+" + m[where + 1].word->first;
	unsigned int osSize = objectStrings.size();
	if (m[where].getRelVerb() >= 0 && (m[m[where].getRelVerb()].flags & cWordMatch::flagInInfinitivePhrase) && preferredVerb == VERB_PRESENT_THIRD_SINGULAR)
	{
		candidate += u"+to+" + m[m[where].getRelVerb()].word->first;
		vector <lpwstring> saveStrings = objectStrings;
		objectStrings.insert(objectStrings.end(), saveStrings.begin(), saveStrings.end());
	}
	for (unsigned int os = 0; os < osSize; os++)
	{
		if (objectStrings[os].length() > 0)
			objectStrings[os] += u"+" + candidate;
		else
			objectStrings[os] = candidate;
	}
	if (osSize != objectStrings.size())
	{
		candidate = m[m[where].getRelVerb()].word->first;
		candidate = getTense(m[where].getRelVerb(), candidate, VERB_PAST);
		for (unsigned int os = osSize; os < objectStrings.size(); os++)
		{
			if (objectStrings[os].length() > 0)
				objectStrings[os] += u"+" + candidate;
			else
				objectStrings[os] = candidate;
		}
	}
	return 0;
}

// Append m[where]'s surface form to every query string (used for the preposition).
int cSource::appendWord(vector <lpwstring>& objectStrings, int where)
{
	LFS
		for (unsigned int os = 0; os < objectStrings.size(); os++)
			if (objectStrings[os].length() > 0)
				objectStrings[os] += u"+" + m[where].word->first;
			else
				objectStrings[os] = m[where].word->first;
	return 0;
}

// Fold a URL into a cache-file stem: strip http(s):// and www., map '/' to '_',
// and hex-escape any other non-alnum except '.'. Output is appended onto epath
// (caller must start empty).
void hashWebSiteURL(lpwstring webSiteURL, lpwstring& epath)
{
	LFS
		if (webSiteURL.empty())
			return;
	lpchar_t tmp[8];
	lpwstring::iterator wsi = webSiteURL.begin();
	const lpchar_t* ba = u"http://";
	if (!lp_strncmp(ba, &(*wsi), lp_strlen(ba)))
		wsi += lp_strlen(ba);
	ba = u"https://";
	if (!lp_strncmp(ba, &(*wsi), lp_strlen(ba)))
		wsi += lp_strlen(ba);
	ba = u"www.";
	if (!lp_strncmp(ba, &(*wsi), lp_strlen(ba)))
		wsi += lp_strlen(ba);
	for (lpwstring::iterator wsiEnd = webSiteURL.end(); wsi != wsiEnd; wsi++)
	{
		if (*wsi == u'/')
			epath += u"_";
		else if (!iswalnum(*wsi) && *wsi != u'.')
		{
			lp_wsprintf(tmp, u"_%x_", *wsi);
			epath += tmp;
		}
		else
			epath += *wsi;
	}
}

extern "C"
{
#include "yajl_tree.h"
}
/*
{ LFS
 "kind": "customsearch#search",
 "url": {
	"type": "application/json",
	"template": "https://www.googleapis.com/customsearch/v1?q={searchTerms}&num={count?}&start={startIndex?}&hr={language?}&safe={safe?}&cx={cx?}&cref={cref?}&sort={sort?}&filter={filter?}&gl={gl?}&cr={cr?}&googlehost={googleHost?}&alt=json"
 },
 "queries": {
	"nextPage": [
	 {
		"title": "Google Custom Search - Paul+Krugman+writes+for",
		"totalResults": "14100000",
		"searchTerms": "Paul+Krugman+writes+for",
		"count": 10,
		"startIndex": 11,
		"inputEncoding": "utf8",
		"outputEncoding": "utf8",
		"safe": "off",
		"cx": "006746333365901280215:2cxb1obqj6u"
	 }
	],
	"request": [
	 {
		"title": "Google Custom Search - Paul+Krugman+writes+for",
		"totalResults": "14100000",
		"searchTerms": "Paul+Krugman+writes+for",
		"count": 10,
		"startIndex": 1,
		"inputEncoding": "utf8",
		"outputEncoding": "utf8",
		"safe": "off",
		"cx": "006746333365901280215:2cxb1obqj6u"
	 }
	]
 },
 "context": {
	"title": "Hello"
 },
 "items": [
	{
	 "kind": "customsearch#result",
	 "title": "Economics and Politics by Paul Krugman - The Conscience of a ...",
	 "htmlTitle": "Economics and Politics by \u003cb\u003ePaul Krugman\u003c/b\u003e - The Conscience of a \u003cb\u003e...\u003c/b\u003e",
	 "link": "http://krugman.blogs.nytimes.com/",
	 "displayLink": "krugman.blogs.nytimes.com",
	 "snippet": "2 days ago ... The New York Times's Op-Ed columnist Paul Krugman, Nobel-Prize winner,   blogs about economics and politics.",
	 "htmlSnippet": "2 days ago \u003cb\u003e...\u003c/b\u003e The New York Times&#39;s Op-Ed columnist \u003cb\u003ePaul Krugman\u003c/b\u003e, Nobel-Prize winner, \u003cbr\u003e  blogs about economics and politics.",
	 "cacheId": "YP0zw1_aeG8J",
	 "pagemap": {
		"cse_image": [
		 {
			"src": "http://graphics8.nytimes.com/images/2012/01/29/opinion/012912krugman2/012912krugman2-blog480.jpg"
		 }
		],
		"cse_thumbnail": [
		 {
			"width": "253",
			"height": "199",
			"src": "https://encrypted-tbn2.google.com/images?q=tbn:ANd9GcSyaUeGdI0wxu9zJRGMiWtSNI4rBAnKXTsT00czgEqvkLrVHa4zI0RjfxW7"
		 }
		],
		"metatags": [
		 {
			"pt": "Blogs",
			"pst": "Blog Main",
			"cg": "Opinion",
			"scg": "krugman"
		 }
		]
	 }
	},
	{
	 "kind": "customsearch#result",
	 "title": "Paul Krugman's Solar Eclipse - Robert Bryce - National Review Online",
	 "htmlTitle": "\u003cb\u003ePaul Krugman&#39;s\u003c/b\u003e Solar Eclipse - Robert Bryce - National Review Online",
	 "link": "http://www.nationalreview.com/articles/282610/paul-krugman-s-solar-eclipse-robert-bryce",
	 "displayLink": "www.nationalreview.com",
	 "snippet": "Nov 9, 2011 ... Robert Bryce writes on NRO: Paul Krugman may be a Nobel Prize�winning   economist, but his most recent column in the New York Times, ...",
	 "htmlSnippet": "Nov 9, 2011 \u003cb\u003e...\u003c/b\u003e Robert Bryce \u003cb\u003ewrites\u003c/b\u003e on NRO: \u003cb\u003ePaul Krugman\u003c/b\u003e may be a Nobel Prize�winning \u003cbr\u003e  economist, but his most recent column in the New York Times, \u003cb\u003e...\u003c/b\u003e",
	 "cacheId": "sEuKW9Pq1vMJ",
	 "pagemap": {
		"cse_image": [
		 {
			"src": "http://www.nationalreview.com/sites/default/files/nfs/uploaded/page_2010_bryce_square.jpg"
		 }
		],
		"cse_thumbnail": [
		 {
			"width": "128",
			"height": "128",
			"src": "https://encrypted-tbn1.google.com/images?q=tbn:ANd9GcSDeZ3vnoCSmyRXJdIN0EFrwQ252enDow7HRe8DwqT7wwtremuLoKkV2pU"
		 }
		],
		"article": [
		 {
			"app_id": "129250807108374",
			"title": "Paul Krugman�s Solar Eclipse",
			"url": "http://www.nationalreview.com/articles/282610/paul-krugman-s-solar-eclipse-robert-bryce",
			"image": "http://global.nationalreview.com/images/logo_NRO_facebook_square_110.jpg",
			"type": "article",
			"site_name": "NRO",
			"description": "Robert Bryce writes on NRO: Paul Krugman may be a Nobel Prize�winning economist, but his most recent column in the New York Times, which condemns hydraulic fracturing and praises solar energy,..."
		 }
		],
		"metatags": [
		 {
			"fb:app_id": "129250807108374",
			"og:title": "Paul Krugman�s Solar Eclipse",
			"og:url": "http://www.nationalreview.com/articles/282610/paul-krugman-s-solar-eclipse-robert-bryce",
			"og:image": "http://global.nationalreview.com/images/logo_NRO_facebook_square_110.jpg",
			"og:type": "article",
			"og:site_name": "NRO",
			"og:description": "Robert Bryce writes on NRO: Paul Krugman may be a Nobel Prize�winning economist, but his most recent column in the New York Times, which condemns hydraulic fracturing and praises solar energy, displays an astounding disinterest in numbers and woeful ignorance of the facts.Without providing any sources, Krugman writes, �We know that [fracturing] produces toxic (and . . .",
			"title": "Paul Krugman�s Solar Eclipse - National Review Online",
			"fb_title": "Paul Krugman�s Solar Eclipse",
			"medium": "article"
		 }
		]
	 }
	}
 ]
}
*/
// Walk a Google Custom Search JSON document (yajl) and push items[].link /
// items[].snippet pairs. Returns -1 on empty / parse failure, 0 otherwise.
int extractGoogleWebSites(lpwstring jsonBuffer, vector <lpwstring>& webSites, vector <lpwstring>& snippets)
{
	LFS
		/* null plug buffers */
		char errbuf[1024];
	errbuf[0] = 0;
	if (jsonBuffer.empty())
		return -1;
	if (iswspace(jsonBuffer[0])) jsonBuffer.erase(jsonBuffer.begin());
	if (!jsonBuffer.length())
		return -1;
	string fileData;
	wTM(jsonBuffer, fileData);
	yajl_val node = yajl_tree_parse((const char*)fileData.c_str(), errbuf, sizeof(errbuf));
	/* parse error handling */
	if (node == NULL) {
		lplog(LOG_ERROR, u"Google Custom Search parse error (1):%s\n %S", jsonBuffer.c_str(), errbuf);
		return -1;
	}
	/* ... and extract a nested value from the config file */
	const char* path[] = { "items", (const char*)0 };
	yajl_val v4 = yajl_tree_get(node, path, yajl_t_array);
	if (v4 != NULL)
	{
		// get snippet and link
		for (unsigned int i3 = 0; i3 < v4->u.array.len; i3++)
		{
			yajl_val v5 = v4->u.array.values[i3];
			if (v5->type == yajl_t_object)
			{
				const char* idpath[] = { "snippet", (const char*)0 };
				yajl_val vid = yajl_tree_get(v5, idpath, yajl_t_string);
				lpwstring snippet;
				if (YAJL_IS_STRING(vid))
					mTW(YAJL_GET_STRING(vid), snippet);
				lpwstring link;
				const char* lpath[] = { "link", (const char*)0 };
				vid = yajl_tree_get(v5, lpath, yajl_t_string);
				if (YAJL_IS_STRING(vid))
				{
					mTW(YAJL_GET_STRING(vid), link);
					if (link.length() > 0)
					{
						snippets.push_back(snippet);
						webSites.push_back(link);
					}
				}
			}
		}
	}
	yajl_tree_free(node);
	return 0;
}

/*
{ LFS
	"d": {
		"results": [{
			"__metadata": {
				"uri": "https://api.datamarket.azure.com/Data.ashx/Bing/SearchWeb/Web?Query=\u0027Xbox\u0027&$skip=0&$top=1",
				"type": "WebResult"
			},
			"ID": "03ee8c91-5d95-49ef-8cb6-8dee7a68d4e8",
			"Title": "Xbox.com",
			"Description": "The official web site for all things on the system.",
			"DisplayUrl": "www.xbox.com",
			"Url": "http://www.xbox.com/"
		}],
		"__next": "https://api.datamarket.azure.com/Data.ashx/Bing/SearchWeb/Web?Query=\u0027Xbox\u0027&$skip=50"
	}
}
// easy testing code in main():
	chdir(".."); // so that log files end up in the right place
	int getBINGSearchJSON(int where, lpwstring object, lpwstring &buffer, lpwstring &filePathOut, int numWebSitesAskedFor, int index);
	int extractBINGWebSites(lpwstring jsonBuffer, vector <lpwstring> &webSites, vector <lpwstring> &snippets);
	int w=0, numWebSitesAskedFor=10, index=0;
	lpwstring object = u"Paul Krugman", jsonBuffer, filePathOut;
	vector <lpwstring> webSites, snippets;
	if (!getBINGSearchJSON(w, object, jsonBuffer, filePathOut, numWebSitesAskedFor, index))
		extractBINGWebSites(jsonBuffer, webSites, snippets);
	if (argc >= 0)
		return 0;
*/
// Walk a Bing v7 JSON document and push webPages.value[].url / .snippet pairs.
int extractBINGWebSites(lpwstring jsonBuffer, vector <lpwstring>& webSites, vector <lpwstring>& snippets)
{
	LFS
		/* null plug buffers */
		char errbuf[1024];
	errbuf[0] = 0;
	if (jsonBuffer.empty())
		return -1;
	if (iswspace(jsonBuffer[0])) jsonBuffer.erase(jsonBuffer.begin());
	if (!jsonBuffer.length())
		return -1;
	string fileData;
	wTM(jsonBuffer, fileData);
	yajl_val node = yajl_tree_parse((const char*)fileData.c_str(), errbuf, sizeof(errbuf));
	/* parse error handling */
	if (node == NULL) {
		lplog(LOG_ERROR, u"BING parse error:%s\n %S", jsonBuffer.c_str(), errbuf);
		return -1;
	}
	/* ... and extract a nested value from the config file */
	const char* path[] = { "webPages","value", (const char*)0 };
	yajl_val v4 = yajl_tree_get(node, path, yajl_t_array);
	if (v4 != NULL)
	{
		// get snippet and link
		for (unsigned int i3 = 0; i3 < v4->u.array.len; i3++)
		{
			yajl_val v5 = v4->u.array.values[i3];
			if (v5->type == yajl_t_object)
			{
				const char* idpath[] = { "snippet", (const char*)0 };
				yajl_val vid = yajl_tree_get(v5, idpath, yajl_t_string);
				lpwstring snippet;
				if (YAJL_IS_STRING(vid))
					mTW(YAJL_GET_STRING(vid), snippet);
				lpwstring link;
				const char* upath[] = { "url", (const char*)0 };
				vid = yajl_tree_get(v5, upath, yajl_t_string);
				if (YAJL_IS_STRING(vid))
				{
					mTW(YAJL_GET_STRING(vid), link);
					if (link.length() > 0)
					{
						snippets.push_back(snippet);
						webSites.push_back(link);
					}
				}
			}
		}
	}
	yajl_tree_free(node);
	return 0;
}

// Count space-separated tokens that sit inside double quotes in a query string
// (quotes toggle; '+' inside quotes counts as a word break). Used as a tie-break
// so more-specified (quoted) queries sort first.
int numWordsInQuotes(lpwstring& str)
{
	LFS
		bool inQuote = false;
	int numWords = 0;
	for (const lpchar_t* ch = str.c_str(); *ch; ch++)
		if (*ch == u'\"')
		{
			if (inQuote)
				numWords++;
			inQuote = !inQuote;
		}
		else
			if (*ch == u'+' && inQuote)
				numWords++;
	return numWords;
}

// sort longest objects first - this gives more information to Google
// if equal in length, the # of words in quotes wins.  This gives more specification.  
bool sortWebQueryStrings(lpwstring i, lpwstring j) {
	if (i.length() != j.length())
		return i.length() > j.length();
	return numWordsInQuotes(i) > numWordsInQuotes(j);
}


// Suffix every query with "+<semanticSuggestion>" (a proximity-map neighbour
// such as a show name) so the next Google/Bing pass is more specific.
void cQuestionAnswering::enhanceWebSearchQueries(vector <lpwstring>& webSearchQueryStrings, lpwstring semanticSuggestion)
{
	LFS
		lpwstring ss = u"+" + semanticSuggestion;
	for (vector <lpwstring>::iterator wqsi = webSearchQueryStrings.begin(), wqsiEnd = webSearchQueryStrings.end(); wqsi != wqsiEnd; wqsi++)
	{
		wqsi->append(ss);
	}
}

// Build the Google/Bing query list from the question SRG: start with "", then
// append subject, tensed verb, object, and (if the prep is certain) prep+object.
// Each appendObject call fans the vector out. Unquoted copies get a quoted
// twin unless the object was the WH-span (noQuotes). Sorted longest-first.
void cQuestionAnswering::getWebSearchQueries(cSource* questionSource, cSyntacticRelationGroup* parentSRG, vector <lpwstring>& webSearchQueryStrings)
{
	LFS
		webSearchQueryStrings.push_back(u"");
	questionSource->appendObject(parentSRG->questionType, parentSRG->whereQuestionType, webSearchQueryStrings, parentSRG->whereSubject);
	if (parentSRG->whereVerb >= 0)
		questionSource->appendVerb(webSearchQueryStrings, parentSRG->whereVerb);
	bool noQuotes = (questionSource->appendObject(parentSRG->questionType, parentSRG->whereQuestionType, webSearchQueryStrings, parentSRG->whereObject) < 0);
	if (parentSRG->wherePrep >= 0 && !parentSRG->prepositionUncertain && (parentSRG->whereObject < 0 || questionSource->m[parentSRG->whereObject].relPrep < 0 || questionSource->m[questionSource->m[parentSRG->whereObject].relPrep].nextQuote != parentSRG->whereObject))
	{
		questionSource->appendWord(webSearchQueryStrings, parentSRG->wherePrep);
		questionSource->appendObject(parentSRG->questionType, parentSRG->whereQuestionType, webSearchQueryStrings, parentSRG->wherePrepObject);
	}
	else
		noQuotes = false;
	if (!noQuotes)
	{
		int webSearchQueryStringsSize = webSearchQueryStrings.size();
		for (int I = 0; I < webSearchQueryStringsSize; I++)
			if (webSearchQueryStrings[I].find(u"\"") == lpwstring::npos)
				webSearchQueryStrings.push_back(u"\"" + webSearchQueryStrings[I] + u"\"");
	}
	sort(webSearchQueryStrings.begin(), webSearchQueryStrings.end(), sortWebQueryStrings);
	for (unsigned int I = 0; I < webSearchQueryStrings.size(); I++)
		lplog(LOG_WHERE, u"%d:webSearchQueryStrings %s", I, webSearchQueryStrings[I].c_str());

}

// Copy qo into qlo with every '"' removed. Used to collapse quoted/unquoted
// twins of the same query so we do not issue both once enough hits exist.
lpwstring quoteLess(lpwstring& qo, lpwstring& qlo)
{
	LFS
		qlo = qo;
	qlo.erase(std::remove(qlo.begin(), qlo.end(), u'\"'), qlo.end());
	return qlo;
}

int startProcesses(MYSQL& mysql, int sourceType, int processKind, int step, int beginSource, int endSource, cSource::sourceTypeEnum processSourceType, int maxProcesses, int numSourcesPerProcess,
	bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite, bool makeCopyBeforeSourceWrite, bool parseOnly, lpwstring specialExtension);

// For each queued path, skip it if a current-version SourceCache already exists;
// otherwise INSERT a REQUEST_TYPE row (generateParseRequestSources). If more than
// one request was generated, startProcesses() launches up to 6 child lp.exe
// workers. Returns startProcesses' result, or -1 when there is nothing to spawn.
// Stale caches with sourceVersion==0 still trigger a reparse.
int cQuestionAnswering::spinParses(MYSQL& mysql, vector <cSearchSource>& accumulatedParseRequests)
{
	deleteGeneratedParseRequests(mysql);
	int generatedRequests = 0;
	for (vector <cSearchSource>::iterator pri = accumulatedParseRequests.begin(), priEnd = accumulatedParseRequests.end(); pri != priEnd; pri++)
	{
		lpwstring path = pri->pathInCache + u".SourceCache";
		bool processOldFile = false;
		if (!lp_waccess(path.c_str(), 0))
		{
			processOldFile = true;
			int fd = lp_wopen(path.c_str(), O_RDONLY | O_BINARY, _S_IREAD | _S_IWRITE), errorCode = (fd < 0 ? errno : 0);
			if (errorCode == 0)
			{
				int sourceVersion = 0, numBytes = -2;
				if ((numBytes = ::read(fd, &sourceVersion, sizeof(sourceVersion))) == 0)
					sourceVersion = SOURCE_VERSION;
				::close(fd);
				if (sourceVersion == SOURCE_VERSION)
					processOldFile = false;
				else
					lplog(LOG_WHERE, u"reparse of %s (%d) [3]?", path.c_str(), sourceVersion);
				if (sourceVersion == 0)
				{
					lplog(LOG_WHERE, u"reparse of %s (%d)?", path.c_str(), numBytes);
				}
			}
			else
			{
				lplog(LOG_WHERE, u"reparse of %s (%d) [2]?", path.c_str(), errno);
				if (errno == EACCES)
					lp_wremove(path.c_str());
			}
		}
		if (processOldFile || (lp_waccess(path.c_str(), 0) && !rejectPath(pri->pathInCache.c_str())))
		{
			generateParseRequestSources(mysql, pri);
			generatedRequests++;
		}
	}
	if (generatedRequests > 1)
		return startProcesses(mysql, cSource::REQUEST_TYPE, 0, 0, -1, -1, cSource::REQUEST_TYPE, 6, 5, false, true, true, true, false, false, u"");
	else
		return -1;
}

extern int limitProcessingForProfiling;
// From webSearchQueryStringOffset onward, run Google or Bing, skip wikipedia.org
// hits, write each snippet to webSearchCache\_<hash>.snippet.txt and download
// the full page. Dedupes by path. After a query that returned >2 hits, skip
// remaining twins that differ only by quotes. Advances the offset. Returns the
// largest hit-list size seen (used as "last page?" when < 10).
int cQuestionAnswering::accumulateParseRequests(cSyntacticRelationGroup* parentSRG, int webSitesAskedFor, int index, bool googleSearch, vector <lpwstring>& webSearchQueryStrings, int& webSearchQueryStringOffset, vector <cSearchSource>& accumulatedParseRequests)
{
	LFS
		int maxWebSitesFound = -1;
	set <lpwstring> pathsAccumulated;
	for (vector <lpwstring>::iterator oi = webSearchQueryStrings.begin() + webSearchQueryStringOffset, oiEnd = webSearchQueryStrings.end(); oi != oiEnd; oi++, webSearchQueryStringOffset++)
	{
		lpwstring object = *oi, jsonBuffer, filePathOut;
		vector <lpwstring> webSites, snippets;
		if (googleSearch)
		{
			getGoogleSearchJSON(parentSRG->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index);
			extractGoogleWebSites(jsonBuffer, webSites, snippets);
		}
		else
		{
			if (!getBINGSearchJSON(parentSRG->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index))
				extractBINGWebSites(jsonBuffer, webSites, snippets);
		}
		maxWebSitesFound = max(maxWebSitesFound, (signed)webSites.size());
		for (vector <lpwstring>::iterator ssi = snippets.begin(), wsi = webSites.begin(), ssiEnd = snippets.end(), wsiEnd = webSites.end(); ssi != ssiEnd; ssi++, wsi++)
		{
			// prevent searching wikipedia as that has already been done.  This will also include canadian and other wikipedias
			if (wsi->find(u"wikipedia.org/") != lpwstring::npos)
				continue;
			lpwstring webSiteBuffer, epath, headers;
			hashWebSiteURL(*wsi, epath);
			cSearchSource pr;
			int snippetLocation = -1;
			if (!ssi->empty())
			{
				lpchar_t path[1024];
				int pathlen = lp_snprintf(path, (sizeof(path)/sizeof((path)[0])), u"%s\\webSearchCache", getWebSearchCacheDir().c_str()) + 1;
				if (lp_wmkdir(path) < 0 && errno == ENOENT)
					lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
				lp_snprintf(path, (sizeof(path)/sizeof((path)[0])), u"%s\\webSearchCache\\_%s", getWebSearchCacheDir().c_str(), epath.c_str());
				convertIllegalChars(path + pathlen);
				distributeToSubDirectories(path, pathlen, true);
				path[MAX_PATH - 28] = 0; // extensions
				lp_strcpy((path) + lp_strlen(path), u".snippet.txt");
				if ((!lp_waccess(path, 0) || flushString(*ssi, path) >= 0) && pathsAccumulated.find(path) == pathsAccumulated.end())
				{
					pr.isSnippet = true;
					pr.pathInCache = path;
					pr.fullWebPath = *wsi;
					pr.skipFullPath = false;
					pr.hasCorrespondingSnippet = false;
					pr.fullWebPath += u" abstract";
					snippetLocation = accumulatedParseRequests.size();
					pr.fullPathIndex = -1;
					accumulatedParseRequests.push_back(pr);
					pathsAccumulated.insert(path);
				}
			}
			if (cInternet::getWebPath(parentSRG->where, *wsi, webSiteBuffer, epath, u"webSearchCache", filePathOut, headers, index, true, false) == 0 && pathsAccumulated.find(filePathOut) == pathsAccumulated.end())
			{
				pr.isSnippet = false;
				pr.pathInCache = filePathOut;
				pr.fullWebPath = *wsi;
				pr.skipFullPath = false;
				if (pr.hasCorrespondingSnippet = snippetLocation >= 0)
					accumulatedParseRequests[snippetLocation].fullPathIndex = accumulatedParseRequests.size();
				accumulatedParseRequests.push_back(pr);
				pathsAccumulated.insert(filePathOut);
			}
		}
		if (webSites.size() > 2)
		{
			lpwstring quoteLessObject, qlo;
			quoteLess(*oi, quoteLessObject);
			for (oi++; oi != oiEnd && quoteLessObject == quoteLess(*oi, qlo); oi++, webSearchQueryStringOffset++);
			oi--;
		}
	}
	return maxWebSitesFound;
}

// processPath + analyzeQuestionFromSource each queued snippet/page. A snippet
// whose best matchSum is >= 24 marks its corresponding full page skipFullPath
// so we do not re-parse the article. Always returns 0.
int cQuestionAnswering::analyzeAccumulatedRequests(cSource* questionSource, lpchar_t* derivation, cSyntacticRelationGroup* parentSRG, bool parseOnly, vector < cAS >& answerSRGs, int& maxAnswer, vector <cSearchSource>& accumulatedParseRequests)
{
	LFS
		int check = answerSRGs.size();
	for (vector <cSearchSource>::iterator oi = accumulatedParseRequests.begin(), oiEnd = accumulatedParseRequests.end(); oi != oiEnd; oi++)
	{
		if (oi->skipFullPath)
			continue;
		cSource* source = NULL;
		int sMaxAnswer = -1;
		if (oi->isSnippet || !oi->hasCorrespondingSnippet)
			check = answerSRGs.size();
		if (processPath(questionSource, oi->pathInCache.c_str(), source, cSource::WEB_SEARCH_SOURCE_TYPE, (oi->isSnippet) ? 50 : 100, parseOnly) >= 0)
			analyzeQuestionFromSource(questionSource, derivation, oi->fullWebPath, source, parentSRG, answerSRGs, sMaxAnswer, true);
		maxAnswer = max(maxAnswer, sMaxAnswer);
		if (sMaxAnswer >= 24 && oi->isSnippet && oi->fullPathIndex >= 0)
			accumulatedParseRequests[oi->fullPathIndex].skipFullPath = true;
	}
	return 0;
}

// Parallel web-search pass: accumulateParseRequests, spin child parsers, then
// score the resulting caches. Returns the max hit-list size (see Serial).
int cQuestionAnswering::webSearchForQueryParallel(cSource* questionSource, lpchar_t* derivation, cSyntacticRelationGroup* parentSRG, bool parseOnly, vector < cAS >& answerSRGs, int& maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
	vector <lpwstring>& webSearchQueryStrings, int& webSearchQueryStringOffset)
{
	vector <cSearchSource> accumulatedParseRequests;
	int maxWebSitesFound = accumulateParseRequests(parentSRG, webSitesAskedFor, index, googleSearch, webSearchQueryStrings, webSearchQueryStringOffset, accumulatedParseRequests);
	spinParses(questionSource->mysql, accumulatedParseRequests);
	analyzeAccumulatedRequests(questionSource, derivation, parentSRG, parseOnly, answerSRGs, maxAnswer, accumulatedParseRequests);
	return maxWebSitesFound;
}

// In-process web-search pass: for each remaining query, fetch hits, parse the
// snippet immediately, and only download/parse the full page if the snippet
// scored below 24. Same quote-twin skip as the parallel path. limitProcessingForProfiling
// aborts after the first full page. Returns the max hit-list size.
int cQuestionAnswering::webSearchForQuerySerial(cSource* questionSource, lpchar_t* derivation, cSyntacticRelationGroup* parentSRG, bool parseOnly, vector < cAS >& answerSRGs, int& maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
	vector <lpwstring>& webSearchQueryStrings, int& webSearchQueryStringOffset)
{
	LFS
		int maxWebSitesFound = -1;
	for (vector <lpwstring>::iterator oi = webSearchQueryStrings.begin() + webSearchQueryStringOffset, oiEnd = webSearchQueryStrings.end(); oi != oiEnd; oi++, webSearchQueryStringOffset++)
	{
		lpwstring object = *oi, jsonBuffer, filePathOut;
		vector <lpwstring> webSites, snippets;
		if (googleSearch)
		{
			getGoogleSearchJSON(parentSRG->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index);
			extractGoogleWebSites(jsonBuffer, webSites, snippets);
		}
		else
		{
			getBINGSearchJSON(parentSRG->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index);
			extractBINGWebSites(jsonBuffer, webSites, snippets);
		}
		maxWebSitesFound = max(maxWebSitesFound, (signed)webSites.size());
		int I = 0;
		for (vector <lpwstring>::iterator ssi = snippets.begin(), wsi = webSites.begin(), ssiEnd = snippets.end(), wsiEnd = webSites.end(); ssi != ssiEnd; ssi++, wsi++, I++)
		{
			int sMaxAnswer = -1;
			lpwstring webSiteBuffer, epath, headers;
			hashWebSiteURL(*wsi, epath);
			//int check = answerSRGs.size();
			if (!ssi->empty())
			{
				cSource* source = NULL;
				if (processSnippet(questionSource, *ssi, epath, source, parseOnly) >= 0)
				{
					analyzeQuestionFromSource(questionSource, derivation, *wsi + u" abstract", source, parentSRG, answerSRGs, sMaxAnswer, true);
				}
			}
			maxAnswer = max(maxAnswer, sMaxAnswer);
			if (sMaxAnswer < 24 && cInternet::getWebPath(parentSRG->where, *wsi, webSiteBuffer, epath, u"webSearchCache", filePathOut, headers, index, true, false) == 0)
			{
				cSource* source = NULL;
				if (processPath(questionSource, (lpchar_t*)filePathOut.c_str(), source, cSource::WEB_SEARCH_SOURCE_TYPE, 100, parseOnly) >= 0)
				{
					analyzeQuestionFromSource(questionSource, derivation, *wsi, source, parentSRG, answerSRGs, maxAnswer, true);
					if (limitProcessingForProfiling)
						return maxWebSitesFound;
				}
			}
		}
		if (webSites.size() > 2)
		{
			lpwstring quoteLessObject, qlo;
			quoteLess(*oi, quoteLessObject);
			for (oi++; oi != oiEnd && quoteLessObject == quoteLess(*oi, qlo); oi++, webSearchQueryStringOffset++);
			oi--;
		}
	}
	return maxWebSitesFound;
}
