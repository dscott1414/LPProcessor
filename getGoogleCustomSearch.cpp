#include <windows.h>
#include <io.h>
#include "word.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "direct.h"
#include "time.h"
#include <errno.h>
#include <Winhttp.h>
#include <functional>
#include <share.h>
#include "profile.h"
#include "internet.h"

#define MAX_PATH_LEN 2048
#define MAX_BUF 2000000
bool myquery(MYSQL *mysql, wchar_t *q, bool allowFailure = false);

/* 
   base64.cpp 

   Copyright (C) 2004-2008 René Nyffenegger

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

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch


*/

//#include <iostream>

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";


static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

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

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
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
// this uses the key assigned to dscott's account
// using the custom search engine (cx) name created which ignores all information coming from wikipedia (since we already have freebase and dbPedia to reference directly)
// with the words WITH QUOTES "Paul Krugman writes for"
// https://www.googleapis.com/customsearch/v1?key=AIzaSyDOCHy1bm-46kJkgV2hqPjFJ6Ce8FfR_AE&cx=006746333365901280215:2cxb1obqj6u&q=%22Paul+Krugman+writes+for%22
wstring googleBaseWebSearchAddress=L"https://www.googleapis.com/customsearch/v1";
wstring BINGBaseWebSearchAddress=L"https://api.datamarket.azure.com/Bing/SearchWeb/Web";
wstring webSearchKey=L"AIzaSyDOCHy1bm-46kJkgV2hqPjFJ6Ce8FfR_AE";
string BINGAccountKey="joRjSURyb6u9B9XFGUb87mjTsvZ+Zhoq9437ARj8eKU=";
wstring cseContext=L"006746333365901280215:2cxb1obqj6u";

void encodeURL(wstring winput,wstring &wencodedURL);
int flushString(wstring &buffer,wchar_t *path);

/*
$top	Specifies the number of results to return.	&count=	50
(50 is the maximum)	https://api.datamarket.azure.com/Bing/Search/Web?Query=%27Xbox%27&$top=10

$skip	Specifies the offset requested for the starting point of results returned.	&offset=	0	https://api.datamarket.azure.com/Bing/Search/Web?Query=%27Xbox%27&$top=10&$skip=20

*/
int getBINGSearchJSON(int where,wstring object,wstring &buffer,wstring &filePathOut,int numWebSitesAskedFor,int index)
{ LFS
	wstring uobject,numWebSitesAskedForStr;
	//replace(object.begin(),object.end(),L' ',L'+');
	encodeURL(object,uobject);
	// %27Paul%20Krugman%27%20criticized%20in%20%27his%20op-ed%20columns%27
	// %22Paul%2BKrugman%22%2Bcriticized%2Bin%2B%22his%2Bop%2Ded%2Bcolumns%22
	// re-encode %2B as %20, %22 as %27
  for (size_t pos = 0; (pos = uobject.find(L"%2B", pos)) != std::string::npos; pos += 3)
     uobject.replace(pos, 3, L"%20");
	for (size_t pos = 0; (pos = uobject.find(L"%22", pos)) != std::string::npos; pos += 3)
		uobject.replace(pos, 3, L"%27");
	if (uobject.find(L"%27") == wstring::npos)
		uobject = L"%27" + uobject + L"%27";
	//https://api.datamarket.azure.com/Bing/SearchWeb/Web?$format=json&Query=%27Xbox%27
	wstring webAddress=BINGBaseWebSearchAddress+L"?$format=json&Query="+uobject+L"&$top="+itos(numWebSitesAskedFor,numWebSitesAskedForStr);
	if (index>1)
	{
		wstring start;
		itos(index,start);
		webAddress+=L"&$skip="+start;
	}
	string userPassword=BINGAccountKey+":"+BINGAccountKey;
	string encodedUserPassword=base64_encode((const unsigned char *)userPassword.c_str(),userPassword.length());
	wstring wEncodedUserPassword;
	mTW(encodedUserPassword,wEncodedUserPassword);
	wstring headers=L"Authorization: Basic "+wEncodedUserPassword;
	int errCode= Internet::getWebPath(where,webAddress,buffer,object+L"_BING",L"webSearchCache",filePathOut,headers,index,false,true);
	lplog(LOG_WEBSEARCH|LOG_WHERE|LOG_ERROR,L"searching BING: %s [%s] searchIndex=%d [%s]",object.c_str(),filePathOut.c_str(),index,webAddress.c_str());
	if (buffer == L"Parameter: Query is not of type String")
	{
		lplog(LOG_ERROR, L"Invalid query");
		return -1;
	}
	return errCode;
}

// cache google searches since this is faster and we are paying for them
// input object is not encoded, but should include quotes when needed.
int getGoogleSearchJSON(int where,wstring object,wstring &buffer,wstring &filePathOut,int numWebSitesAskedFor,int index)
{ LFS
	wstring uobject,numWebSitesAskedForStr;
	replace(object.begin(),object.end(),L' ',L'+');
	encodeURL(object,uobject);
	// https://www.googleapis.com/customsearch/v1?key=AIzaSyDOCHy1bm-46kJkgV2hqPjFJ6Ce8FfR_AE&cx=006746333365901280215:2cxb1obqj6u&q=%22Paul+Krugman+writes+for%22
	wstring start;
	if (index>1)
	{
		itos(index,start);
		start=L"&start="+start;
	}
	wstring webAddress=googleBaseWebSearchAddress+L"?key="+webSearchKey+start+L"&cx="+cseContext+L"&q="+uobject+L"&num="+itos(numWebSitesAskedFor,numWebSitesAskedForStr);
	wstring headers;
	int errCode= Internet::getWebPath(where,webAddress,buffer,object,L"webSearchCache",filePathOut,headers,index,false,true);
	lplog(LOG_WEBSEARCH|LOG_WHERE|LOG_ERROR,L"searching Google: %s [%s] searchIndex=%d [%s]",object.c_str(),filePathOut.c_str(),index,webAddress.c_str());
	lplog(LOG_WHERE, NULL);
	return errCode;
}

bool Source::inObject(int where,int whereQuestionType)
{ LFS
	if (where==whereQuestionType)
		return true;
	return (where>=0 && whereQuestionType>=0 && m[where].beginObjectPosition>=0 && m[where].endObjectPosition>=0 && 
		  whereQuestionType>=m[where].beginObjectPosition-1 && whereQuestionType<m[where].endObjectPosition);
}

// this is deliberately noncombinatorial - each prepositional phrase only has one predecessor
bool Source::appendPrepositionalPhrase(int where,vector <wstring> &prepPhraseStrings,int relPrep,bool nonMixedCase,bool lowerCase,wchar_t *separator,int atNumPP)
{ LFS
	int relObject=m[relPrep].getRelObject();
	if (relObject>=relPrep && m[relObject].endObjectPosition>=0 && (m[relPrep].nextQuote==where || m[relPrep].relNextObject==where))
	{
		wstring oStr;
		oStr=separator;
		int lctlen=1;
		getOriginalWord(relPrep,oStr,true);
		for (int I=relPrep+1; I<m[relObject].endObjectPosition; I++)
		{
			oStr+=separator;
			lctlen=oStr.length();
			getOriginalWord(I,oStr,true);
		}
		bool lc=iswlower(oStr[lctlen])!=0;
		if (!nonMixedCase || ((lowerCase && lc) || (!lowerCase && !lc)))
		{
			int originalSize=prepPhraseStrings.size();
			if (atNumPP<0) 
				prepPhraseStrings.push_back(prepPhraseStrings[originalSize-1]);
			else
				originalSize--;
			prepPhraseStrings[originalSize]+=oStr;
			return true;
		}
	}
	return false;
}

int Source::appendPrepositionalPhrases(int where,wstring &wsoStr,vector <wstring> &prepPhraseStrings,int &numWords,bool noMixedCase,wchar_t *separator,int atNumPP)
{ LFS
	int relPrep;
	numWords=0;
	if (where>=0 && where<(signed)m.size() && (relPrep=m[where].endObjectPosition)>=0 && relPrep<(signed)m.size() && m[relPrep].queryWinnerForm(prepositionForm)>=0 && (m[relPrep].nextQuote==where || m[relPrep].relNextObject==where))
	{
		prepPhraseStrings.push_back(wsoStr);
		bool lowerCase=iswlower(wsoStr[0])!=0;
		size_t lw=wsoStr.find_last_of(separator);
		int largestRelPrep=-1;
		if (lw!=wstring::npos) lowerCase=iswlower(wsoStr[lw+1])!=0;
		for (int I = 0; (I<atNumPP || atNumPP<0) && relPrep<(signed)m.size() && relPrep >= 0 && (m[relPrep].nextQuote == where || m[relPrep].relNextObject == where) && appendPrepositionalPhrase(where, prepPhraseStrings, relPrep, noMixedCase, lowerCase, separator, atNumPP); I++)
		{
			largestRelPrep=relPrep;
			relPrep=m[relPrep].relPrep;
		}
		if (largestRelPrep>=0 && m[largestRelPrep].getRelObject()>=0)
			numWords=m[m[largestRelPrep].getRelObject()].endObjectPosition-m[where].endObjectPosition;
	}
	return prepPhraseStrings.size();
}

int Source::getObjectStrings(int where,int object,vector <wstring> &wsoStrs,bool &alreadyDidPlainCopy)
{ LFS
	wstring wsoStr;
	if (objects[object].objectClass==NAME_OBJECT_CLASS && (m[where].endObjectPosition-m[where].beginObjectPosition>1 || objects[object].name.first!=wNULL))
	{
		objects[object].name.original(wsoStr,L'+',true);
		wsoStrs.push_back(wsoStr);
	}
	else
	{
		int oc=objects[object].objectClass;
		if (alreadyDidPlainCopy && 
			  oc!=NAME_OBJECT_CLASS && 
			  oc!=GENDERED_GENERAL_OBJECT_CLASS && 
			  oc!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && 
			  oc!=GENDERED_DEMONYM_OBJECT_CLASS && 
			  oc!=NON_GENDERED_BUSINESS_OBJECT_CLASS &&
				oc!=NON_GENDERED_NAME_OBJECT_CLASS)
			return -1;
		int begin=m[where].beginObjectPosition,len=m[where].endObjectPosition;
		for (int I=begin; I<len; I++)
			getOriginalWords(I,wsoStrs,I>begin);
		alreadyDidPlainCopy=true;
	}
	return 0;
}

int Source::appendObject(__int64 questionType,int whereQuestionType,vector <wstring> &objectStrings,int where)
{ LFS
	if (where<0) return 0;
	if (whereQuestionType == where || (whereQuestionType >= 0 && inObject(where, whereQuestionType)))
	{
		if (questionType&QTAFlag)
		{
			vector <wstring> saveStrings;
			saveStrings = objectStrings;
			int os = objectStrings.size();
			objectStrings.insert(objectStrings.end(), saveStrings.begin(), saveStrings.end());
			for (; os<objectStrings.size(); os++)
			{
				if (objectStrings[os].length()>0)
					objectStrings[os] += L"+" + m[where].word->first;
				else
					objectStrings[os] = m[where].word->first;
			}
			return 0;
		}
		return -1;
	}
	vector <int> appendObjects;
	int o=m[where].getObject(),oc;
	if (o>=0 && m[where].objectMatches.size()==0 && (oc=objects[o].objectClass)!=REFLEXIVE_PRONOUN_OBJECT_CLASS && oc!=RECIPROCAL_PRONOUN_OBJECT_CLASS && oc!=PRONOUN_OBJECT_CLASS)
		appendObjects.push_back(o);
	for (unsigned int oi = 0; oi<m[where].objectMatches.size(); oi++)
	{
		oc=objects[m[where].objectMatches[oi].object].objectClass;
		if (oc!=REFLEXIVE_PRONOUN_OBJECT_CLASS && oc!=RECIPROCAL_PRONOUN_OBJECT_CLASS && oc!=PRONOUN_OBJECT_CLASS)
			appendObjects.push_back(m[where].objectMatches[oi].object);
	}
	wstring logres;
	lplog(LOG_WHERE, L"appendObjects: %d:objectMatches.size=%d:%d:%s", where, m[where].objectMatches.size(),appendObjects.size(), objectString(appendObjects, logres).c_str());
	bool alreadyDidPlainCopy=false,copyNeeded=false;
	vector <wstring> saveStrings;
	saveStrings=objectStrings;
	for (vector <int>::iterator oi=appendObjects.begin(),oiEnd=appendObjects.end(); oi!=oiEnd; oi++)
	{
		vector <wstring> wsoStrs;
		if (getObjectStrings((o==*oi) ? where : objects[*oi].originalLocation,*oi,wsoStrs,alreadyDidPlainCopy)<0) 
			continue;
		unsigned int os=0;
		if (copyNeeded)
		{
			os=objectStrings.size();
			objectStrings.insert(objectStrings.end(),saveStrings.begin(),saveStrings.end());
		}
		copyNeeded=true;
		for (vector <wstring>::iterator wsoStr=wsoStrs.begin(),wsoStrEnd=wsoStrs.end(); wsoStr!=wsoStrEnd; wsoStr++)
		{
			//lplog(LOG_WHERE,L"%d:appendObjects: %d:%s",oi-appendObjects.begin(),wsoStr-wsoStrs.begin(),*wsoStr);
			for (; os<objectStrings.size(); os++)
			{
				if (objectStrings[os].length()>0)
					objectStrings[os]+=L"+" + *wsoStr;
				else
					objectStrings[os]=*wsoStr;
			}
			bool insertQuotes;
			if (insertQuotes=wsoStr->find(L'+')!=wstring::npos)
			{
				os=objectStrings.size();
				objectStrings.insert(objectStrings.end(),saveStrings.begin(),saveStrings.end());
				for (; os<objectStrings.size(); os++)
				{
					if (objectStrings[os].length()>0)
						objectStrings[os]+=L"+\"" + *wsoStr + L"\"";
					else
						objectStrings[os]=L"\"" + *wsoStr + L"\"";
				}
			}
			vector <wstring> prepPhraseStrings;
			int numWords;
			if (m[where].relPrep>=0 && m[where].relPrep<(signed)m.size() && m[m[where].relPrep].nextQuote==where && appendPrepositionalPhrases(where,*wsoStr,prepPhraseStrings,numWords,true,L"+",-1)>1)
			{
				for (unsigned int pp=1; pp<prepPhraseStrings.size(); pp++)
				{
					os=objectStrings.size();
					objectStrings.insert(objectStrings.end(),saveStrings.begin(),saveStrings.end());
					//lplog(LOG_WHERE,L"%d: relPrep=%d m.size()=%d nextQuote=%d inserting pp=%s objectStrings.size()=%d",where,m[where].relPrep,m.size(),(m[where].relPrep>=0 && m[where].relPrep<(signed)m.size()) ? m[m[where].relPrep].nextQuote : -1,prepPhraseStrings[pp].c_str(),os);
					for (; os<objectStrings.size(); os++)
					{
						if (objectStrings[os].length()>0)
							objectStrings[os]+=L"+"+prepPhraseStrings[pp];
						else
							objectStrings[os]=prepPhraseStrings[pp];
					}
					os=objectStrings.size();
					objectStrings.insert(objectStrings.end(),saveStrings.begin(),saveStrings.end());
					//lplog(LOG_WHERE,L"%d: relPrep=%d m.size()=%d nextQuote=%d inserting pp=%s objectStrings.size()=%d",where,m[where].relPrep,m.size(),(m[where].relPrep>=0 && m[where].relPrep<(signed)m.size()) ? m[m[where].relPrep].nextQuote : -1,prepPhraseStrings[pp].c_str(),os);
					for (; os<objectStrings.size(); os++)
					{
						if (objectStrings[os].length()>0)
							objectStrings[os]+=L"+\""+prepPhraseStrings[pp]+L"\"";
						else
							objectStrings[os]=L"\""+prepPhraseStrings[pp]+L"\"";
					}
				}
			}
		}
	}
	for (unsigned int I=0; I<objectStrings.size(); I++)
		lplog(LOG_WHERE,L"%d: objectString %d:%s",where,I,objectStrings[I].c_str());
	return 0;
}

// PAST:VERB_PAST, VERB_PAST_PARTICIPLE,VERB_PAST_THIRD_SINGULAR,VERB_PAST_PLURAL
// PRESENT:VERB_PRESENT_THIRD_SINGULAR=128,VERB_PRESENT_FIRST_SINGULAR=256, VERB_PRESENT_PLURAL=2048,VERB_PRESENT_SECOND_SINGULAR=4096,VERB_PRESENT_PARTICIPLE
tIWMM Source::getTense(tIWMM verb,tIWMM subject,int tenseDesired)
{ LFS
	int inflectionFlags=verb->second.inflectionFlags,desiredInflectionFlags=0;
	bool verbIsPast=((inflectionFlags&(VERB_PAST|VERB_PAST_PARTICIPLE|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL))>0);
	bool verbIsPresent=((inflectionFlags&(VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_PARTICIPLE))>0);
  bool tenseDesiredIsPast=((tenseDesired&(VERB_PAST|VERB_PAST_PARTICIPLE|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL))>0);
	bool tenseDesiredIsPresent=((tenseDesired&(VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_PARTICIPLE))>0);
	if ((verbIsPast && tenseDesiredIsPast) || (verbIsPresent && tenseDesiredIsPresent)) return verb;
	bool subjectIsSingular=subject==wNULL || (subject->second.inflectionFlags&SINGULAR);
	if (verbIsPast)
		desiredInflectionFlags=(subjectIsSingular) ? VERB_PRESENT_THIRD_SINGULAR:VERB_PRESENT_PLURAL;
	else
		desiredInflectionFlags=VERB_PAST;
	tIWMM verbMainEntry=(verb->second.mainEntry==wNULL) ? verb : verb->second.mainEntry;
	unordered_map <wstring, vector <tIWMM>>::iterator mEMI;
	// prefer third person verbs
	if (verbMainEntry!=wNULL && (mEMI=Words.mainEntryMap.find(verbMainEntry->first))!=Words.mainEntryMap.end())
	{
		for (vector <tIWMM>::iterator mei=mEMI->second.begin(),meiEnd=mEMI->second.end(); mei!=meiEnd; mei++)
			{
				if ((*mei)->second.inflectionFlags&desiredInflectionFlags)
				{
					return *mei;
				}
			}
	}
	return verb;
}

wstring Source::getTense(int where,wstring candidate,int preferredVerb)
{ LFS
	tIWMM mainEntry=m[where].getMainEntry();
	unordered_map <wstring, vector <tIWMM>>::iterator mEMI;
	// prefer third person verbs
	if (mainEntry!=wNULL && (mEMI=Words.mainEntryMap.find(mainEntry->first))!=Words.mainEntryMap.end())
	{
		for (unsigned int me=0; me<mEMI->second.size(); me++)
		{
			//int inflectionFlags=mEMI->second[me]->second.inflectionFlags;
			wstring verb=mEMI->second[me]->first;
			//lplog(L"%s:%d",verb.c_str(),inflectionFlags);
			if (mEMI->second[me]->second.inflectionFlags&preferredVerb)
			{
				if (preferredVerb==VERB_PRESENT_FIRST_SINGULAR)
					candidate=L"will+"+mEMI->second[me]->first;
				else
					candidate=mEMI->second[me]->first;
				break;
			}
		}
	}
	return candidate;
}
// possibly adjust tense
int Source::appendVerb(vector <wstring> &objectStrings,int where)
{ LFS
	wstring candidate=m[where].word->first;
	int fullTense=m[where].quoteForwardLink;
	int tense=fullTense&VT_TENSE_MASK;
	//int relPrep = m[where].relPrep;// , msize = m.size();
	bool isPassive=(fullTense&VT_PASSIVE)==VT_PASSIVE && (m[where].relPrep<0 || (m[where].relPrep>=0 && m[m[where].relPrep].word->first!=L"by"));
	int preferredVerb=0;
	// was born
	if (isPassive)
	{
		preferredVerb=tense;
		if  ((tense==VT_PAST) || (tense==VT_PAST_PERFECT) || (tense==VT_PRESENT_PERFECT))
		{
			candidate=L"was";
		}
		else if  ((tense==VT_FUTURE) || (tense==VT_FUTURE_PERFECT))
		{
			candidate=L"will+be";
		}
		else if  ((tense==VT_PRESENT))
		{
			candidate=L"is+being";
		}
		candidate+=L"+"+m[where].word->first;
	}
	else
	{
		if  ((tense==VT_PAST) || (tense==VT_PAST_PERFECT) || (tense==VT_PRESENT_PERFECT))
		{
			preferredVerb=VERB_PAST;
		}
		else if  ((tense==VT_FUTURE) || (tense==VT_FUTURE_PERFECT))
		{
			preferredVerb=VERB_PRESENT_FIRST_SINGULAR;
		}
		else if  ((tense==VT_PRESENT))
		{
			preferredVerb=VERB_PRESENT_THIRD_SINGULAR;
		}
		if (preferredVerb)
		{
			candidate=getTense(where,candidate,preferredVerb);
		}
	}
	wstring zcandidate=m[where+1].word->first;
	if (m[where+1].queryForm(prepositionForm)>=0 && 
  		(m[where+1].queryWinnerForm(prepositionForm)>=0 || m[where+1].queryWinnerForm(adverbForm)>=0) && 
	  	m[where+1].getRelObject()<0)
		candidate+=L"+"+m[where+1].word->first;
	unsigned int osSize=objectStrings.size();
	if (m[where].relVerb>=0 && (m[m[where].relVerb].flags&WordMatch::flagInInfinitivePhrase) && preferredVerb==VERB_PRESENT_THIRD_SINGULAR)
	{
		candidate+=L"+to+"+m[m[where].relVerb].word->first;
		vector <wstring> saveStrings=objectStrings;
		objectStrings.insert(objectStrings.end(),saveStrings.begin(),saveStrings.end());
	}
	for (unsigned int os=0; os<osSize; os++)
	{
		if (objectStrings[os].length()>0)
			objectStrings[os]+=L"+"+candidate;
		else
			objectStrings[os]=candidate;
	}
	if (osSize!=objectStrings.size())
	{
		candidate=m[m[where].relVerb].word->first;
		candidate=getTense(m[where].relVerb,candidate,VERB_PAST);
		for (unsigned int os=osSize; os<objectStrings.size(); os++)
		{
			if (objectStrings[os].length()>0)
				objectStrings[os]+=L"+"+candidate;
			else
				objectStrings[os]=candidate;
		}
	}
	return 0;
}

int Source::appendWord(vector <wstring> &objectStrings,int where)
{ LFS
	for (unsigned int os=0; os<objectStrings.size(); os++)
		if (objectStrings[os].length()>0)
			objectStrings[os]+=L"+"+m[where].word->first;
		else
			objectStrings[os]=m[where].word->first;
	return 0;
}

void hashWebSiteURL(wstring webSiteURL,wstring &epath)
{ LFS
	wchar_t tmp[8];
	wstring::iterator wsi=webSiteURL.begin();
	wchar_t *ba=L"http://";
	if (!wcsncmp(ba,&(*wsi),wcslen(ba)))
		wsi+=wcslen(ba);
	ba=L"www.";
	if (!wcsncmp(ba,&(*wsi),wcslen(ba)))
		wsi+=wcslen(ba);
	for (wstring::iterator wsiEnd=webSiteURL.end(); wsi!=wsiEnd; wsi++)
	{
		if (*wsi==L'/')
			epath+=L"_";
		else if (!iswalnum(*wsi) && *wsi!=L'.')
		{
			wsprintf(tmp,L"_%x_",*wsi);
			epath+=tmp;
		}
		else
			epath+=*wsi;
	}
}

wchar_t *StructureOfHtmlDocument[] = { L"html",L"head",L"body",L"div",L"span",NULL };
wchar_t *CreatingMetaTags[] = { L"DOCTYPE",L"title",L"link",L"meta",L"style",NULL };
wchar_t *TextLinkInHtml[] = { L"p",L"h1-h6",L"strong",L"em",L"abbr",L"acronym",L"address",L"bdo",L"blockquote",L"cite",L"q",L"code",L"ins",L"del",L"dfn",L"kbd",L"pre",L"samp",L"var",L"br",NULL };
wchar_t *ImagesAndObjects[] = { L"img",L"area",L"map",L"object",L"param",NULL };
wchar_t *CreatingWebForms[] = { L"form",L"input",L"textarea",L"select",L"option",L"optgroup",L"button",L"label",L"fieldset",L"legend",NULL };
wchar_t *TablesTagInHtml[] = { L"table",L"tr",L"td",L"th",L"tbody",L"thead",L"tfoot",L"col",L"colgroup",L"caption",NULL };
wchar_t *OrderedUnorderedLists[] = { L"ul",L"ol",L"li",L"dl",L"dt",L"dd",NULL };
wchar_t *Scripting[] = { L"script",L"noscript",NULL };

bool inCategory(wchar_t *tagList[],wstring tag)
{ LFS
	for (int tli=0; tagList[tli]; tli++)
		if (tag==tagList[tli])
			return true;
	return false;
}

void scrapeWebSite(wstring &webSiteBuffer)
{ LFS
	// category 1. remove all things between '<...>' and then treat everything between instance of these as separate texts.
	// category 2. remove all things between '<...>' and skip it as though it wasn't there.
	// category 3. remove all things between '<...>' and if it isn't closed by />, seek the next tag <...> and if it matches the closing tag, remove everything between the start and the closing tag.
	// remove all separate texts without at least one period.

	// Structure Of Html Document - treat as category 1
  //     html | head | body | div | span
	// Creating Meta Tags - treat as category 1
	//     DOCTYPE | title | link | meta | style 
	// Text Link In Html - treat as category 2
	//     p | h1-h6 | strong | em | abbr | acronym | address | bdo |
	//     blockquote | cite | q | code | ins | del | dfn | kbd | pre | samp | var | br
	// Images and Objects - category 1
	//     img | area | map | object | param
	// Creating Web Forms - treat as category 1
	//     form | input | textarea | select | option | optgroup | button | compactLabel | fieldset | legend	
	// Tables Tag In Html - treat as category 3
	//     table | tr | td | th | tbody | thead | tfoot | col | colgroup | caption
	// Ordered Unordered Lists - treat as category 2
	//     ul | ol | li | dl | dt | dd
	// Scripting - treat as category 3
	//     script | noscript
	wstring currentTextBlob;
	vector <wstring> textBlobs;
	for (unsigned int I=0; I<webSiteBuffer.size(); I++)
	{
		if (webSiteBuffer[I]==L'<')
		{
			wstring tag;
			if (webSiteBuffer[I+1]==L'/') I++; // skip closing
			for (unsigned int J=I+1; J<webSiteBuffer.size() && webSiteBuffer[J]!=L' '; J++)
				tag+=webSiteBuffer[J];
			// category 1
			if (inCategory(StructureOfHtmlDocument,tag) || inCategory(CreatingMetaTags,tag) || inCategory(ImagesAndObjects,tag) || inCategory(CreatingWebForms,tag))
			{
				if (currentTextBlob.length()>0)
					textBlobs.push_back(currentTextBlob);
				currentTextBlob.clear();
				for (; I<webSiteBuffer.size() && webSiteBuffer[I]!=L'>'; I++);
				continue;				
			}
			// category 2
			else if (inCategory(TextLinkInHtml,tag) || inCategory(OrderedUnorderedLists,tag))
			{
				for (; I<webSiteBuffer.size() && webSiteBuffer[I]!=L'>'; I++);
				continue;				
			}
			// category 3
			else if (inCategory(TablesTagInHtml,tag) || inCategory(Scripting,tag))
			{
			}
		}
		else
			currentTextBlob+=webSiteBuffer[I];
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
   "snippet": "Nov 9, 2011 ... Robert Bryce writes on NRO: Paul Krugman may be a Nobel Prize–winning   economist, but his most recent column in the New York Times, ...",
   "htmlSnippet": "Nov 9, 2011 \u003cb\u003e...\u003c/b\u003e Robert Bryce \u003cb\u003ewrites\u003c/b\u003e on NRO: \u003cb\u003ePaul Krugman\u003c/b\u003e may be a Nobel Prize–winning \u003cbr\u003e  economist, but his most recent column in the New York Times, \u003cb\u003e...\u003c/b\u003e",
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
      "title": "Paul Krugman’s Solar Eclipse",
      "url": "http://www.nationalreview.com/articles/282610/paul-krugman-s-solar-eclipse-robert-bryce",
      "image": "http://global.nationalreview.com/images/logo_NRO_facebook_square_110.jpg",
      "type": "article",
      "site_name": "NRO",
      "description": "Robert Bryce writes on NRO: Paul Krugman may be a Nobel Prize–winning economist, but his most recent column in the New York Times, which condemns hydraulic fracturing and praises solar energy,..."
     }
    ],
    "metatags": [
     {
      "fb:app_id": "129250807108374",
      "og:title": "Paul Krugman’s Solar Eclipse",
      "og:url": "http://www.nationalreview.com/articles/282610/paul-krugman-s-solar-eclipse-robert-bryce",
      "og:image": "http://global.nationalreview.com/images/logo_NRO_facebook_square_110.jpg",
      "og:type": "article",
      "og:site_name": "NRO",
      "og:description": "Robert Bryce writes on NRO: Paul Krugman may be a Nobel Prize–winning economist, but his most recent column in the New York Times, which condemns hydraulic fracturing and praises solar energy, displays an astounding disinterest in numbers and woeful ignorance of the facts.Without providing any sources, Krugman writes, “We know that [fracturing] produces toxic (and . . .",
      "title": "Paul Krugman’s Solar Eclipse - National Review Online",
      "fb_title": "Paul Krugman’s Solar Eclipse",
      "medium": "article"
     }
    ]
   }
  }
 ]
}
*/
int extractGoogleWebSites(wstring jsonBuffer,vector <wstring> &webSites,vector <wstring> &snippets)
{ LFS
	  /* null plug buffers */
	char errbuf[1024];
  errbuf[0] = 0;
	if (iswspace(jsonBuffer[0])) jsonBuffer.erase(jsonBuffer.begin());
	if (!jsonBuffer.length()) 
		return -1;
	string fileData;
	wTM(jsonBuffer,fileData);
  yajl_val node = yajl_tree_parse((const char *) fileData.c_str(), errbuf, sizeof(errbuf));
  /* parse error handling */
  if (node == NULL) {
		lplog(LOG_ERROR,L"FreeBase parse error (1):%s\n %S", jsonBuffer.c_str(),errbuf);
    return -1;
  }
  /* ... and extract a nested value from the config file */
	const char * path[] = { "items", (const char *) 0 };
	yajl_val v4 = yajl_tree_get(node, path, yajl_t_array);
	if (v4!=NULL)
	{
		// get snippet and link
		for (unsigned int i3=0; i3<v4->u.array.len; i3++)
		{
			yajl_val v5=v4->u.array.values[i3];
			if (v5->type==yajl_t_object)
			{
				const char * idpath[] = { "snippet", (const char *) 0 };
				yajl_val vid = yajl_tree_get(v5, idpath, yajl_t_string);
				wstring snippet;
				if (vid!=NULL && YAJL_IS_STRING(vid))
					mTW(YAJL_GET_STRING(vid),snippet);
				wstring link;
				const char * lpath[] = { "link", (const char *) 0 };
				vid = yajl_tree_get(v5, lpath, yajl_t_string);
				if (vid!=NULL && YAJL_IS_STRING(vid))
				{
					mTW(YAJL_GET_STRING(vid),link);
					if (link.length()>0)
					{
						snippets.push_back(snippet);
						webSites.push_back(link);
					}
				}
			}
		}
	}
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
*/
int extractBINGWebSites(wstring jsonBuffer,vector <wstring> &webSites,vector <wstring> &snippets)
{ LFS
	  /* null plug buffers */
	char errbuf[1024];
  errbuf[0] = 0;
	if (iswspace(jsonBuffer[0])) jsonBuffer.erase(jsonBuffer.begin());
	if (!jsonBuffer.length()) 
		return -1;
	string fileData;
	wTM(jsonBuffer,fileData);
  yajl_val node = yajl_tree_parse((const char *) fileData.c_str(), errbuf, sizeof(errbuf));
  /* parse error handling */
  if (node == NULL) {
		lplog(LOG_ERROR,L"FreeBase parse error (2):%s\n %S", jsonBuffer.c_str(),errbuf);
    return -1;
  }
  /* ... and extract a nested value from the config file */
	const char * path[] = { "d","results", (const char *) 0 };
	yajl_val v4 = yajl_tree_get(node, path, yajl_t_array);
	if (v4!=NULL)
	{
		// get snippet and link
		for (unsigned int i3=0; i3<v4->u.array.len; i3++)
		{
			yajl_val v5=v4->u.array.values[i3];
			if (v5->type==yajl_t_object)
			{
				const char * idpath[] = { "Description", (const char *) 0 };
				yajl_val vid = yajl_tree_get(v5, idpath, yajl_t_string);
				wstring snippet;
				if (vid!=NULL && YAJL_IS_STRING(vid))
					mTW(YAJL_GET_STRING(vid),snippet);
				wstring link;
				const char * upath[] = { "Url", (const char *) 0 };
				vid = yajl_tree_get(v5, upath, yajl_t_string);
				if (vid!=NULL && YAJL_IS_STRING(vid))
				{
					mTW(YAJL_GET_STRING(vid),link);
					if (link.length()>0)
					{
						snippets.push_back(snippet);
						webSites.push_back(link);
					}
				}
			}
		}
	}
	return 0;
}

int numWordsInQuotes(wstring &str)
{ LFS
	bool inQuote=false;
	int numWords=0;
	for (const wchar_t *ch=str.c_str(); *ch; ch++)
		if (*ch==L'\"') 
		{
			if (inQuote)
				numWords++;
			inQuote=!inQuote;
		}
		else
			if (*ch==L'+' && inQuote)
				numWords++;
	return numWords;
}

// sort longest objects first - this gives more information to Google
// if equal in length, the # of words in quotes wins.  This gives more specification.  
bool sortWebQueryStrings(wstring i,wstring j) { 
	if (i.length()!=j.length())
		return i.length()>j.length();
	return numWordsInQuotes(i)>numWordsInQuotes(j); 
}


void Source::enhanceWebSearchQueries(vector <wstring> &webSearchQueryStrings,wstring semanticSuggestion)
{ LFS
	wstring ss=L"+"+semanticSuggestion;
	for (vector <wstring>::iterator wqsi=webSearchQueryStrings.begin(),wqsiEnd=webSearchQueryStrings.end(); wqsi!=wqsiEnd; wqsi++)
	{
		wqsi->append(ss);
	}
}

void Source::getWebSearchQueries(cSpaceRelation* parentSRI,vector <wstring> &webSearchQueryStrings)
{ LFS
	webSearchQueryStrings.push_back(L"");
	appendObject(parentSRI->questionType,parentSRI->whereQuestionType, webSearchQueryStrings, parentSRI->whereSubject);
	if (parentSRI->whereVerb>=0)
		appendVerb(webSearchQueryStrings,parentSRI->whereVerb);
	bool noQuotes = (appendObject(parentSRI->questionType, parentSRI->whereQuestionType, webSearchQueryStrings, parentSRI->whereObject)<0);
	if (parentSRI->wherePrep>=0 && !parentSRI->prepositionUncertain && (parentSRI->whereObject<0 || m[parentSRI->whereObject].relPrep<0 || m[m[parentSRI->whereObject].relPrep].nextQuote!=parentSRI->whereObject))
	{
		appendWord(webSearchQueryStrings,parentSRI->wherePrep);
		appendObject(parentSRI->questionType, parentSRI->whereQuestionType, webSearchQueryStrings, parentSRI->wherePrepObject);
	}
	else
		noQuotes=false;
  if (!noQuotes)
	{
		int webSearchQueryStringsSize=webSearchQueryStrings.size();
		for (int I=0; I<webSearchQueryStringsSize; I++)
			if (webSearchQueryStrings[I].find(L"\"")==wstring::npos)
			  webSearchQueryStrings.push_back(L"\""+webSearchQueryStrings[I]+L"\"");
	}
	sort(webSearchQueryStrings.begin(),webSearchQueryStrings.end(),sortWebQueryStrings);
	for (unsigned int I=0; I<webSearchQueryStrings.size(); I++)
		lplog(LOG_WHERE,L"%d:webSearchQueryStrings %s",I,webSearchQueryStrings[I].c_str());

}

wstring quoteLess(wstring &qo,wstring &qlo)
{ LFS
	qlo=qo;
  qlo.erase(std::remove( qlo.begin(), qlo.end(), L'\"' ),qlo.end());
  return qlo;
}

int startProcesses(Source &source, int processKind, int step, int beginSource, int endSource, Source::sourceTypeEnum st, int maxProcesses, int numSourcesPerProcess, bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite, bool parseOnly);

int Source::spinParses(vector <searchSource> &accumulatedParseRequests)
{
	createParseRequestTable();
	for (vector <searchSource>::iterator pri = accumulatedParseRequests.begin(), priEnd = accumulatedParseRequests.end(); pri != priEnd; pri++)
	{
		wstring path = pri->pathInCache + L".SourceCache";
		bool processOldFile = false;
		if (!_waccess(path.c_str(), 0))
		{
			processOldFile = true;
			int fd, errorCode = _wsopen_s(&fd, path.c_str(), O_RDWR | O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
			if (errorCode == 0)
			{
				int sourceVersion = 0, numBytes = -2;
				if ((numBytes=::read(fd, &sourceVersion, sizeof(sourceVersion))) == 0)
					sourceVersion = SOURCE_VERSION;
				_close(fd);
				if (sourceVersion == SOURCE_VERSION)
					processOldFile = false;
				else
					lplog(LOG_WHERE, L"reparse of %s (%d) [3]?", path.c_str(), sourceVersion);
				if (sourceVersion == 0)
				{
					lplog(LOG_WHERE,L"reparse of %s (%d)?",path.c_str(),numBytes);
				}
			}
			else
			{
				lplog(LOG_WHERE, L"reparse of %s (%d) [2]?", path.c_str(), errno);
				if (errno == EACCES)
					_wremove(path.c_str());
			}
		}
		if (processOldFile || (_waccess(path.c_str(), 0) && !rejectPath(pri->pathInCache.c_str())))
			writeParseRequestToDatabase(pri);
	}
	return startProcesses(*this, 0,0,-1, -1, Source::REQUEST_TYPE, 5,5,false,true,true,true,false);
}

extern int limitProcessingForProfiling;
int Source::accumulateParseRequests(cSpaceRelation* parentSRI, int webSitesAskedFor, int index, bool googleSearch, vector <wstring> &webSearchQueryStrings, int &offset, vector <searchSource> &accumulatedParseRequests)
{
	LFS
	int maxWebSitesFound = -1;
	for (vector <wstring>::iterator oi = webSearchQueryStrings.begin() + offset, oiEnd = webSearchQueryStrings.end(); oi != oiEnd; oi++, offset++)
	{
		wstring object = *oi, jsonBuffer, filePathOut;
		vector <wstring> webSites, snippets;
		if (googleSearch)
		{
			getGoogleSearchJSON(parentSRI->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index);
			extractGoogleWebSites(jsonBuffer, webSites, snippets);
		}
		else
		{
			if (!getBINGSearchJSON(parentSRI->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index))
				extractBINGWebSites(jsonBuffer, webSites, snippets);
		}
		maxWebSitesFound = max(maxWebSitesFound, (signed)webSites.size());
		for (vector <wstring>::iterator ssi = snippets.begin(), wsi = webSites.begin(), ssiEnd = snippets.end(), wsiEnd = webSites.end(); ssi != ssiEnd; ssi++, wsi++)
		{
			wstring webSiteBuffer, epath, headers;
			hashWebSiteURL(*wsi, epath);
			searchSource pr;
			int snippetLocation=-1;
			if (!ssi->empty())
			{
				wchar_t path[1024];
				int pathlen = _snwprintf(path, MAX_LEN, L"%s\\webSearchCache", WEBSEARCH_CACHEDIR) + 1;
				_wmkdir(path);
				_snwprintf(path, MAX_LEN, L"%s\\webSearchCache\\_%s", WEBSEARCH_CACHEDIR, epath.c_str());
				convertIllegalChars(path + pathlen);
				distributeToSubDirectories(path, pathlen, true);
				path[MAX_PATH - 28] = 0; // extensions
				wcscat(path, L".snippet.txt");
				if (!_waccess(path, 0) || flushString(*ssi, path) >= 0)
				{
					pr.isSnippet = true;
					pr.pathInCache = path;
					pr.fullWebPath = *wsi;
					pr.skipFullPath = false;
					pr.hasCorrespondingSnippet = false;
					pr.fullWebPath += L" abstract";
					snippetLocation = accumulatedParseRequests.size();
					pr.fullPathIndex = -1;
					accumulatedParseRequests.push_back(pr);
				}
			}
			if (Internet::getWebPath(parentSRI->where, *wsi, webSiteBuffer, epath, L"webSearchCache", filePathOut, headers, index, true, false) == 0)
			{
				pr.isSnippet = false;
				pr.pathInCache = filePathOut;
				pr.fullWebPath = *wsi;
				pr.skipFullPath = false;
				if (pr.hasCorrespondingSnippet=snippetLocation >= 0)
					accumulatedParseRequests[snippetLocation].fullPathIndex = accumulatedParseRequests.size();
				accumulatedParseRequests.push_back(pr);
			}
		}
		if (webSites.size()>2)
		{
			wstring quoteLessObject, qlo;
			quoteLess(*oi, quoteLessObject);
			for (oi++; oi != oiEnd && quoteLessObject == quoteLess(*oi, qlo); oi++, offset++);
			oi--;
		}
	}
	return maxWebSitesFound;
}

int Source::analyzeAccumulatedRequests(wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, vector <searchSource> &accumulatedParseRequests,
 cPattern *&mapPatternAnswer,
	cPattern *&mapPatternQuestion)
{
	LFS
	int check = answerSRIs.size();
	for (vector <searchSource>::iterator oi = accumulatedParseRequests.begin(), oiEnd = accumulatedParseRequests.end(); oi != oiEnd; oi++)
	{
		if (oi->skipFullPath)
			continue;
		Source *source = NULL;
		int sMaxAnswer = -1;
		if (oi->isSnippet || !oi->hasCorrespondingSnippet)
			check = answerSRIs.size();
		if (processPath(oi->pathInCache.c_str(), source, Source::WEB_SEARCH_SOURCE_TYPE, (oi->isSnippet) ? 50 : 100, parseOnly) >= 0)
			analyzeQuestionFromSource(derivation, oi->fullWebPath, source, parentSRI, answerSRIs, sMaxAnswer, check, true, mapPatternAnswer, mapPatternQuestion);
		maxAnswer = max(maxAnswer, sMaxAnswer);
		if (sMaxAnswer >= 24 && oi->isSnippet && oi->fullPathIndex >= 0)
			accumulatedParseRequests[oi->fullPathIndex].skipFullPath = true;
	}
	return 0;
}

int Source::webSearchForQueryParallel(wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
 vector <wstring> &webSearchQueryStrings,
	int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion)
{
	vector <searchSource> accumulatedParseRequests;
	int maxWebSitesFound = accumulateParseRequests(parentSRI, webSitesAskedFor, index, googleSearch, webSearchQueryStrings, offset, accumulatedParseRequests);
	spinParses(accumulatedParseRequests);
	if (!myquery(&mysql, L"DROP table parseRequests"))
		return -1;
	analyzeAccumulatedRequests(derivation, parentSRI, parseOnly, answerSRIs, maxAnswer, accumulatedParseRequests, mapPatternAnswer, mapPatternQuestion);
	return maxWebSitesFound;
}

int Source::webSearchForQuerySerial(wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
 vector <wstring> &webSearchQueryStrings,
	int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion)
{
	LFS
	int maxWebSitesFound = -1;
	for (vector <wstring>::iterator oi = webSearchQueryStrings.begin() + offset, oiEnd = webSearchQueryStrings.end(); oi != oiEnd; oi++, offset++)
	{
		wstring object = *oi, jsonBuffer, filePathOut;
		vector <wstring> webSites, snippets;
		if (googleSearch)
		{
			getGoogleSearchJSON(parentSRI->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index);
			extractGoogleWebSites(jsonBuffer, webSites, snippets);
		}
		else
		{
			getBINGSearchJSON(parentSRI->where, object, jsonBuffer, filePathOut, webSitesAskedFor, index);
			extractBINGWebSites(jsonBuffer, webSites, snippets);
		}
		maxWebSitesFound = max(maxWebSitesFound, (signed)webSites.size());
		int I = 0;
		for (vector <wstring>::iterator ssi = snippets.begin(), wsi = webSites.begin(), ssiEnd = snippets.end(), wsiEnd = webSites.end(); ssi != ssiEnd; ssi++, wsi++, I++)
		{
			int sMaxAnswer = -1;
			wstring webSiteBuffer, epath, headers;
			hashWebSiteURL(*wsi, epath);
			int check = answerSRIs.size();
			if (!ssi->empty())
			{
				Source *source = NULL;
				if (processSnippet(*ssi, epath, source, parseOnly) >= 0)
				{
					analyzeQuestionFromSource(derivation, *wsi + L" abstract", source, parentSRI, answerSRIs, sMaxAnswer, check, true, mapPatternAnswer, mapPatternQuestion);
				}
			}
			maxAnswer = max(maxAnswer, sMaxAnswer);
			if (sMaxAnswer<24 && Internet::getWebPath(parentSRI->where, *wsi, webSiteBuffer, epath, L"webSearchCache", filePathOut, headers, index, true, false) == 0)
			{
				Source *source = NULL;
				if (processPath((wchar_t *)filePathOut.c_str(), source, Source::WEB_SEARCH_SOURCE_TYPE, 100, parseOnly) >= 0)
				{
					analyzeQuestionFromSource(derivation, *wsi, source, parentSRI, answerSRIs, maxAnswer, check, true, mapPatternAnswer, mapPatternQuestion);
					if (limitProcessingForProfiling)
						return maxWebSitesFound;
				}
			}
		}
		if (webSites.size()>2)
		{
			wstring quoteLessObject, qlo;
			quoteLess(*oi, quoteLessObject);
			for (oi++; oi != oiEnd && quoteLessObject == quoteLess(*oi, qlo); oi++, offset++);
			oi--;
		}
	}
	return maxWebSitesFound;
}
