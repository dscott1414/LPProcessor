#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <winsock.h>
#include "Winhttp.h"
using namespace std;
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "time.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "vcXML.h"
#include <wn.h>
#include "profile.h"
#include "ontology.h"
#include "strsafe.h"
#include "QuestionAnswering.h"

int questionProgress=-1; // shared but update is not important

int flushString(wstring &buffer,wchar_t *path)
{ LFS
	//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path, __FUNCTIONW__);
	int fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE); 
	if (fd<0)
	{
		lplog(LOG_ERROR,L"ERROR:Cannot create path %s - %S (9).",path,sys_errlist[errno]);
		return -1;
	}
	wchar_t ch=0xFEFF;
	_write(fd,&ch,sizeof(ch));
	_write(fd,buffer.c_str(),buffer.length()*sizeof(buffer[0]));
	_close(fd);
	return 0;
 }

// <http://rdf.freebase.com/ns/m.0zcqcv2>
wstring stripWeb(wstring &name)
{
	if (name[0] == '<') 
		name = name.substr(1, name.length() - 1);
	if (name[name.length() - 1] == '>') 
		name = name.substr(0, name.length() - 1);
	int hcut = name.find(L"://");
	if (hcut != wstring::npos)
		name = name.substr(hcut + 3, name.length() - hcut - 3);
	int rdfcut = name.find(L"rdf.freebase.com/");
	if (rdfcut != wstring::npos)
		name = name.substr(rdfcut + 17, name.length() - rdfcut - 17);
	int nscut = name.find(L"ns/");
	if (nscut != wstring::npos)
		name = name.substr(nscut + 3, name.length() - nscut - 3);
	return name;
}

//		unknownQTFlag=1,whichQTFlag=2,whereQTFlag=3,whatQTFlag=4,whoseQTFlag=5,howQTFlag=6,whenQTFlag=7,whomQTFlag=8,
//		referencingObjectQTFlag=16,subjectQTFlag=17,objectQTFlag=18,secondaryObjectQTFlag=19,prepObjectQTFlag=20,
//		referencingObjectQTAFlag=32,subjectQTAFlag=33,objectQTAFlag=34,secondaryObjectQTAFlag=35,prepObjectQTAFlag=36 
#define MAX_LEN 2048
int cQuestionAnswering::processAbstract(Source *questionSource,cTreeCat *rdfType,Source *&source,bool parseOnly)
{ LFS
  wchar_t path[1024];
	int pathlen=_snwprintf(path,MAX_LEN,L"%s\\dbPediaCache",CACHEDIR)+1;
	_wmkdir(path);
	wstring typeObject(stripWeb(rdfType->typeObject));
	_snwprintf(path,MAX_LEN,L"%s\\dbPediaCache\\_%s.abstract.txt",CACHEDIR,typeObject.c_str());
	convertIllegalChars(path+pathlen);
	distributeToSubDirectories(path,pathlen,true);
	if (logTraceOpen)
		lplog(LOG_WHERE, L"TRACEOPEN %s %S", path, __FUNCTION__);
	if (_waccess(path, 0) && flushString(rdfType->abstract, path)<0)
		return -1;
	return processPath(questionSource,path,source,Source::WEB_SEARCH_SOURCE_TYPE,1,parseOnly);
}

int cQuestionAnswering::processSnippet(Source *questionSource, wstring snippet,wstring object,Source *&source,bool parseOnly)
{ LFS
  wchar_t path[1024];
	int pathlen=_snwprintf(path,MAX_LEN,L"%s\\webSearchCache", WEBSEARCH_CACHEDIR)+1;
	_wmkdir(path);
	_snwprintf(path,MAX_LEN,L"%s\\webSearchCache\\_%s",WEBSEARCH_CACHEDIR,object.c_str());
	convertIllegalChars(path+pathlen);
	distributeToSubDirectories(path,pathlen,true);
	path[MAX_PATH-28]=0; // extensions
	wcscat(path,L".snippet.txt");
	if (logTraceOpen)
		lplog(LOG_WHERE, L"TRACEOPEN %s %S", path, __FUNCTION__);
	if (_waccess(path, 0) && flushString(snippet, path)<0)
		return -1;
	return processPath(questionSource,path,source,Source::WEB_SEARCH_SOURCE_TYPE,50,parseOnly);
}

int cQuestionAnswering::processWikipedia(Source *questionSource, int principalWhere,Source *&source,vector <wstring> &wikipediaLinks,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases,bool parseOnly, set <wstring> &wikipediaLinksAlreadyScanned)
{ LFS
  wchar_t path[1024];
	vector <wstring> lookForSubject;
	if (questionSource->getWikipediaPath(principalWhere, wikipediaLinks, path, lookForSubject, includeNonMixedCaseDirectlyAttachedPrepositionalPhrases) < 0)
		return -1;
	if (wikipediaLinksAlreadyScanned.find(path) != wikipediaLinksAlreadyScanned.end())
		return -1;
	wikipediaLinksAlreadyScanned.insert(path);
	return processPath(questionSource,path,source,Source::WIKIPEDIA_SOURCE_TYPE,2,parseOnly);
}

bool cQuestionAnswering::matchObjects(Source *parentSource,vector <cObject>::iterator parentObject,Source *childSource,vector <cObject>::iterator childObject,bool &namedNoMatch,sTrace debugTrace)
{ LFS
	bool match=false;
	namedNoMatch=false;
	if (parentObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
		match=parentObject->nameAnyNonGenderedMatch(childSource->m,*childObject);
	else if (parentObject->objectClass==NAME_OBJECT_CLASS && childObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
		match=parentObject->nameAnyNonGenderedMatch(childSource->m,*childObject);
	else if (parentObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass==NAME_OBJECT_CLASS)
		match=childObject->nameAnyNonGenderedMatch(parentSource->m,*parentObject);
	else if (parentObject->objectClass==NAME_OBJECT_CLASS && childObject->objectClass==NAME_OBJECT_CLASS)
		match=!childObject->name.isNull() && !parentObject->name.isNull() && childObject->nameMatch(*parentObject,debugTrace);
	else
		return match;
	namedNoMatch=(match==false) && !childObject->name.isNull() && !parentObject->name.isNull();
	return match;
}

bool cQuestionAnswering::matchObjectsExact(vector <cObject>::iterator parentObject,vector <cObject>::iterator childObject,bool &namedNoMatch)
{ LFS
	bool match=false;
	namedNoMatch=false;
	if (parentObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
		match=parentObject->nameMatchExact(*childObject);
	else if (parentObject->objectClass==NAME_OBJECT_CLASS && childObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
		match=parentObject->nameMatchExact(*childObject);
	else if (parentObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass==NAME_OBJECT_CLASS)
		match=childObject->nameMatchExact(*parentObject);
	else if (parentObject->objectClass==NAME_OBJECT_CLASS && childObject->objectClass==NAME_OBJECT_CLASS)
		match=childObject->nameMatchExact(*parentObject);
	else
		return false;
	namedNoMatch=(match==false);
	return match;
}

bool Source::matchChildSourcePositionSynonym(tIWMM parentWord, Source *childSource, int childWhere)
{
	LFS
	if (parentWord == Words.end())
	{
		lplog(LOG_ERROR, L"parent word is not in dictionary!");
		return false;
	}
	set <wstring> childSynonyms,parentSynonyms;
	getSynonyms(parentWord->first,parentSynonyms, NOUN);
	tIWMM parentME=parentWord->second.mainEntry;
	if (parentME==wNULL)
		parentME=parentWord;
  if (childSource->m[childWhere].objectMatches.empty())
	{
		if (childSource->m[childWhere].endObjectPosition>=0)
			childWhere=childSource->m[childWhere].endObjectPosition-1;
		wstring childWord=childSource->m[childWhere].getMainEntry()->first;
		if (childWord.find('%')!=wstring::npos)
			return false;
		wstring tmpstr;
		if (parentWord->first==childWord || parentME->first==childWord)
		{
			if (logSynonymDetail && childSource->debugTrace.traceWhere)
				lplog(LOG_WHERE,L"TSYM [1] comparing PARENT %s[%s] against CHILD [%s]",parentWord->first.c_str(),parentME->first.c_str(),childWord.c_str());
			return true;
		}
		if (logSynonymDetail &&childSource->debugTrace.traceWhere)
			lplog(LOG_WHERE, L"TSYM [1] comparing CHILD %s against synonyms [%s]%s", childWord.c_str(), parentWord->first.c_str(), setString(parentSynonyms, tmpstr, L"|").c_str());
		if (parentSynonyms.find(childWord)!=parentSynonyms.end())
			return true;
		getSynonyms(childWord,childSynonyms, NOUN);
		if (logSynonymDetail && childSource->debugTrace.traceWhere)
			lplog(LOG_WHERE, L"TSYM [1] comparing PARENT %s against synonyms [%s]%s", parentME->first.c_str(), childWord.c_str(), setString(childSynonyms, tmpstr, L"|").c_str());
		if (childSynonyms.find(parentME->first)!=childSynonyms.end())
			return true;
	}
	for (unsigned int mo=0; mo<childSource->m[childWhere].objectMatches.size(); mo++)
	{
		int cw,ownerWhere;
		if ((cw=childSource->objects[childSource->m[childWhere].objectMatches[mo].object].originalLocation)>=0 && !childSource->isDefiniteObject(cw,L"CHILD MATCHED",ownerWhere,false))
		{
			if (childSource->m[cw].endObjectPosition>=0)
				cw=childSource->m[cw].endObjectPosition-1;
			wstring tmpstr;
			getSynonyms(childSource->m[cw].word->first,childSynonyms, NOUN);
			bool childFound=childSynonyms.find(parentWord->first)!=childSynonyms.end();
			if (logSynonymDetail && childSource->debugTrace.traceWhere)
				lplog(LOG_WHERE, L"TSYM [2CHILD] comparing %s against synonyms [%s]%s (%s)", parentWord->first.c_str(), childSource->m[cw].word->first.c_str(), setString(childSynonyms, tmpstr, L"|").c_str(), (childFound) ? L"true" : L"false");
			if (childFound)
				return true;
			bool parentFound=parentSynonyms.find(childSource->m[cw].word->first)!=parentSynonyms.end();
			if (logSynonymDetail && childSource->debugTrace.traceWhere)
				lplog(LOG_WHERE, L"TSYM [2PARENT] comparing %s against synonyms [%s]%s (%s)", childSource->m[cw].word->first.c_str(), parentWord->first.c_str(), setString(parentSynonyms, tmpstr, L"|").c_str(), (parentFound) ? L"true" : L"false");
			if (parentFound)
				return true;
		}
	}
	return false;
}

bool cQuestionAnswering::matchChildSourcePosition(Source *parentSource,vector <cObject>::iterator parentObject,Source *childSource,int childWhere,bool &namedNoMatch,sTrace &debugTrace)
{ LFS
	namedNoMatch=false;
  if (childSource->m[childWhere].objectMatches.empty())
	{
		namedNoMatch=childSource->m[childWhere].getObject()<0;
		return childSource->m[childWhere].getObject()>=0 &&
			matchObjects(parentSource,parentObject,childSource,childSource->objects.begin()+childSource->m[childWhere].getObject(),namedNoMatch,debugTrace);
	}
	bool allNoMatch=true;
	for (unsigned int mo=0; mo<childSource->m[childWhere].objectMatches.size(); mo++)
	{
		namedNoMatch=false;
		if (matchObjects(parentSource,parentObject,childSource,childSource->objects.begin()+childSource->m[childWhere].objectMatches[mo].object,namedNoMatch,debugTrace))
		{
			if (logQuestionDetail)
			{
				wstring tmpstr1,tmpstr2;
				lplog(LOG_WHERE,L"matchChildSourcePosition:parentObject %s against child object %s succeeded",parentSource->objectString(parentObject,tmpstr1,true).c_str(),childSource->objectString(childSource->m[childWhere].objectMatches[mo].object,tmpstr2,true).c_str());
			}
			return true;
		}
		allNoMatch=allNoMatch && namedNoMatch;
	}
	namedNoMatch=allNoMatch;
	return false;
}

// does this represent a definite noun, like Sting or MIT?  If so, disallow synonyms (or perhaps use another kind of synonym which is not yet supported)
// also people are not allowed
// also include subjects which follow immediately after relativizers which have resolved to an object.
bool Source::isDefiniteObject(int where,wchar_t *definiteObjectType,int &ownerWhere,bool recursed)
{ LFS
	int object=m[where].getObject();
	if (object<0) return false;
	wstring tmpstr;
	int bp=m[where].beginObjectPosition;
	switch (objects[object].objectClass)
	{
	case PRONOUN_OBJECT_CLASS:
		if (m[bp].queryWinnerForm(indefinitePronounForm)>=0)
		{
			if (logSynonymDetail)
				lplog(LOG_WHERE,L"%06d:TSYM %s %s is %sa definite object [byIndefiniteClass].",where,definiteObjectType,objectString(object,tmpstr,false).c_str(),L"NOT ");
			return false;
		}
	case REFLEXIVE_PRONOUN_OBJECT_CLASS:
	case RECIPROCAL_PRONOUN_OBJECT_CLASS:
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%06d:TSYM %s %s is %sa definite object [byClassGender].",where,definiteObjectType,objectString(object,tmpstr,false).c_str(),(objects[object].neuter && !objects[object].male && !objects[object].female) ? L"NOT ":L"");
		return !objects[object].neuter || objects[object].male || objects[object].female;
	case GENDERED_DEMONYM_OBJECT_CLASS: // the Italians
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%06d:TSYM %s %s is %sa definite object [byDemonym].",where,definiteObjectType,objectString(object,tmpstr,false).c_str(),(m[bp].word->first==L"a" || m[bp].word->first==L"an") ? L"":L"NOT ");
		return (m[bp].word->first==L"a" || m[bp].word->first==L"an");
	case PLEONASTIC_OBJECT_CLASS: // it, which is not matched and so is meaningless
		return false;
	case NON_GENDERED_BUSINESS_OBJECT_CLASS: // G.M.
	case GENDERED_RELATIVE_OBJECT_CLASS: // sister ; brother / these have different kinds of synonyms which are more like isKindOf
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%06d:TSYM %s %s is a definite object [byClass].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
		return true;
	case VERB_OBJECT_CLASS: // the news I brought / running -- objects that contain verbs
	case GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS: // occupation=plumber;role=leader;activity=runner
	case META_GROUP_OBJECT_CLASS: // different kinds of friend words friendForm/groupJoiner/etc
	case BODY_OBJECT_CLASS: // arm/leg
	case GENDERED_GENERAL_OBJECT_CLASS: // the man/the woman
	case NON_GENDERED_GENERAL_OBJECT_CLASS: // anything else that is not capitalized and not in any other category
		// check for an 'owned' object: his plumber / my specialty / her ship // this organization
		// his rabbit hole / the baby's name
		// my academic [S My academic specialty[17-20]{OWNER:UNK_M_OR_F}[19][nongen][N][OGEN]]  is international constitutional [O international constitutional law[21-24][23][nongen][N]]for   [PO 20[28-29][28][nongen][N]].
		if (m[where].endObjectPosition-bp>1 && 
			  (m[ownerWhere=bp].queryWinnerForm(possessiveDeterminerForm)>=0 || 
			   m[bp].queryWinnerForm(demonstrativeDeterminerForm)>=0 || 
				(m[bp].word->second.inflectionFlags&(SINGULAR_OWNER|PLURAL_OWNER)) || 
				(m[bp].flags&WordMatch::flagNounOwner) ||
				(m[ownerWhere=where].endObjectPosition-bp>2 && 
				 (m[m[where].endObjectPosition-2].word->second.inflectionFlags&(SINGULAR_OWNER|PLURAL_OWNER)) || 
				 (m[m[where].endObjectPosition-2].flags&WordMatch::flagNounOwner))))
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE,L"%06d:TSYM %s %s is a definite object [byOwner].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
			return true;
		}
		if (bp>0 && m[bp-1].queryWinnerForm(relativizerForm)!=-1 && m[bp-1].objectMatches.size()>0)
		{
			ownerWhere = bp - 1;
			wstring tmpstr2;
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byRelativizer - %s].", where, definiteObjectType, objectString(object, tmpstr, false).c_str(), objectString(m[bp - 1].objectMatches[0].object, tmpstr2, false).c_str());
			return true;
		}
		ownerWhere=-1;
		{
			bool accumulatedDefiniteObject=false,accumulatedNotDefiniteObject=false;
			int omsize=m[where].objectMatches.size();
			for (int I=0; I<omsize; I++)
				if (m[where].objectMatches[I].object!=object && objects[m[where].objectMatches[I].object].originalLocation!=where && !recursed)
				{
					if (isDefiniteObject(objects[m[where].objectMatches[I].object].originalLocation,definiteObjectType,ownerWhere,true))
						accumulatedDefiniteObject|=true;
					else
						accumulatedNotDefiniteObject|=true;
				}
			if (logSynonymDetail)
			{
				if (omsize>0)
					lplog(LOG_WHERE,L"%06d:TSYM %s %s is%s a definite object [byClass (2)].",where,definiteObjectType,objectString(object,tmpstr,false).c_str(),(accumulatedDefiniteObject) ? L"":L" NOT");
				else
					lplog(LOG_WHERE,L"%06d:TSYM %s %s is NOT a definite object [byClass].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
			}
			if (omsize && accumulatedDefiniteObject)
				return true;
		}
		return false;
	case NAME_OBJECT_CLASS:
	case NON_GENDERED_NAME_OBJECT_CLASS:
	default:;
	}
	if (!objects[object].dbPediaAccessed)
		identifyISARelation(where,false);
	if (objects[object].isWikiBusiness || objects[object].isWikiPerson || objects[object].isWikiPlace || objects[object].isWikiWork)
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%06d:TSYM %s %s is a definite object [byWiki].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
		return true;
	}
	if (objects[object].name.hon!=wNULL)
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%06d:TSYM %s %s is a definite object [byHonorific].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
		return true;
	}
	switch (objects[object].getSubType())
	{
		case CANADIAN_PROVINCE_CITY: 
		case COUNTRY: 
		case ISLAND: 
		case MOUNTAIN_RANGE_PEAK_LANDFORM: 
		case OCEAN_SEA: 
		case PARK_MONUMENT: 
		case REGION: 
		case RIVER_LAKE_WATERWAY: 
		case US_CITY_TOWN_VILLAGE:
		case US_STATE_TERRITORY_REGION:
		case WORLD_CITY_TOWN_VILLAGE: 
			if (logSynonymDetail)
				lplog(LOG_WHERE,L"%06d:TSYM %s %s is a definite object [bySubType].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
			return true;
	}
	if (m[where].endObjectPosition-m[where].beginObjectPosition>1 && 
			(m[m[where].beginObjectPosition].queryWinnerForm(possessiveDeterminerForm)>=0 || 
			(m[m[where].beginObjectPosition].word->second.inflectionFlags&(SINGULAR_OWNER|PLURAL_OWNER)) || 
			(m[m[where].beginObjectPosition].flags&WordMatch::flagNounOwner)))
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%06d:TSYM %s %s is a definite object [byOwner].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
		ownerWhere=m[where].beginObjectPosition;
		return true;
	}
	// the Nobel Prize
	if (m[m[where].beginObjectPosition].word->first==L"the")
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%06d:TSYM %s %s is a definite object [byThe].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
		return true;
	}
	if (logSynonymDetail)
		lplog(LOG_WHERE,L"%06d:TSYM %s %s is NOT a definite object [default].",where,definiteObjectType,objectString(object,tmpstr,false).c_str());
	return false;
}

// Curveball defected
//000009:TSYM PARENT year[9-10][9][nongen][N][TimeObject] is NOT a definite object [byClass].
//000008:TSYM CHILD 1999[8-9][8][nongen][N][TimeObject] is NOT a definite object [byClass].
bool cQuestionAnswering::matchTimeObjects(Source *parentSource,int parentWhere,Source *childSource,int childWhere)
{ LFS
	vector <cSpaceRelation>::iterator parentSR=parentSource->findSpaceRelation(parentWhere);
	if (parentSR==parentSource->spaceRelations.end())
		return false;
	wstring tmpstr;
	for (unsigned int t2=0; t2<parentSR->timeInfo.size(); t2++)
		lplog(LOG_WHERE,L"%d:MTO parentSR %s",parentWhere,parentSR->timeInfo[t2].toString(parentSource->m,tmpstr).c_str());
	vector <cSpaceRelation>::iterator childSR=childSource->findSpaceRelation(childWhere);
	if (childSR==childSource->spaceRelations.end())
		return false;
	for (unsigned int t=0; t<childSR->timeInfo.size(); t++)
		lplog(LOG_WHERE,L"%d:MTO childSR %s",childWhere,childSR->timeInfo[t].toString(childSource->m,tmpstr).c_str());
	for (unsigned int pt=0; pt<parentSR->timeInfo.size(); pt++)
		for (unsigned int ct=0; ct<childSR->timeInfo.size(); ct++)
			if (parentSR->timeInfo[pt].timeCapacity==childSR->timeInfo[ct].timeCapacity)
				return true;
	return false;
}

// adjectivalMatch does NOT include owner objects (Paul's car)
// adjectivalMatch = -1 if the match operation was not performed
//                   0 if no adjectives to match
//                   1 if adjectives match
//                   2 if adjectives synonyms match
//                   3 if adjectives don't match
bool cQuestionAnswering::matchSourcePositions(Source *parentSource, int parentWhere, Source *childSource, int childWhere, bool &namedNoMatch, bool &synonym, bool parentInQuestionObject, int &semanticMismatch, int &adjectivalMatch, sTrace &debugTrace)
{	LFS
	adjectivalMatch = -1;
	vector <WordMatch>::iterator imChild=childSource->m.begin()+childWhere;
  // an unmatched pronoun matches nothing
	if (imChild->objectMatches.empty() && (imChild->queryWinnerForm(nomForm)>=0 || imChild->queryWinnerForm(personalPronounAccusativeForm)>=0 ||
      imChild->queryWinnerForm(personalPronounForm)>=0 || imChild->queryWinnerForm(quantifierForm)>=0 ||
      imChild->queryWinnerForm(possessivePronounForm)>=0 ||
      imChild->queryWinnerForm(indefinitePronounForm)>=0 || imChild->queryWinnerForm(pronounForm)>=0 ||
			imChild->queryWinnerForm(demonstrativeDeterminerForm)>=0 ||
			imChild->queryWinnerForm(reflexivePronounForm)>=0 ||
			imChild->queryWinnerForm(reciprocalPronounForm)>=0))
			return false;
  // narrator==0 or audience==1 objects
	if (imChild->objectMatches.size()>0 && imChild->objectMatches[0].object<2)
			return false;
	int childObject=(imChild->objectMatches.empty()) ? imChild->getObject() : imChild->objectMatches[0].object;
	namedNoMatch=false;
	if (parentSource->m[parentWhere].objectMatches.empty() && parentSource->m[parentWhere].getObject()>=0 &&
		  matchChildSourcePosition(parentSource,parentSource->objects.begin()+parentSource->m[parentWhere].getObject(),childSource,childWhere,namedNoMatch,debugTrace)) 
			return true;
	bool allNoMatch=true;
	for (unsigned int mo=0; mo<parentSource->m[parentWhere].objectMatches.size(); mo++)
	{
		namedNoMatch=false;
		if (matchChildSourcePosition(parentSource,parentSource->objects.begin()+parentSource->m[parentWhere].objectMatches[mo].object,childSource,childWhere,namedNoMatch,debugTrace))
			return true;
		allNoMatch=allNoMatch && namedNoMatch;
	}
	if (!parentSource->m[parentWhere].objectMatches.empty())
		namedNoMatch=allNoMatch;
	// compare by synonym
	if (namedNoMatch) return false;
	int parentOwnerWhere=-1,childOwnerWhere=-1;
	bool parentIsDefiniteObject=parentSource->isDefiniteObject(parentWhere,L"PARENT",parentOwnerWhere,false);
	bool childIsDefiniteObject=childSource->isDefiniteObject(childWhere,L"CHILD",childOwnerWhere,false);
	bool matched=(!parentIsDefiniteObject && !childIsDefiniteObject);
	if (parentIsDefiniteObject && childIsDefiniteObject && parentOwnerWhere>=0 && childOwnerWhere>=0 && (parentOwnerWhere!=parentWhere && childOwnerWhere!=childWhere))
	{
		wstring tmpstr,tmpstr2;
		matched=matchSourcePositions(parentSource,parentOwnerWhere,childSource,childOwnerWhere,namedNoMatch,synonym,parentInQuestionObject,semanticMismatch,adjectivalMatch,debugTrace);
	  // if they are definite objects, byOwner, and the owners match
		if (logQuestionDetail)
			lplog(LOG_WHERE,L"%d:definite owners: parent %d:%s child %d:%s matched=%s namedNoMatch=%s",parentWhere,parentOwnerWhere,parentSource->whereString(parentOwnerWhere,tmpstr,false).c_str(),childOwnerWhere,childSource->whereString(childOwnerWhere,tmpstr2,false).c_str(),(matched) ? L"true":L"false",(namedNoMatch) ? L"true":L"false");
	}
	// parent=the Nobel Peace Prize / child=the prize
	// parent=Jay-z [31][name][M][A:Jay-z ][WikiPerson] / child=4082:a story[4081-4083][4082][nongen][N]
	if (parentIsDefiniteObject && !childIsDefiniteObject)
	{
		if (parentOwnerWhere>=0)
		{
			int parentOwnerObject=(parentSource->m[parentOwnerWhere].objectMatches.size()>0) ? parentSource->m[parentOwnerWhere].objectMatches[0].object:parentSource->m[parentOwnerWhere].getObject();
			if (parentOwnerWhere>=0 && (parentSource->objects[parentOwnerObject].objectClass==NAME_OBJECT_CLASS || (parentSource->m[parentOwnerWhere].flags&WordMatch::flagNounOwner)))
				return false;
		}
		if (parentSource->m[parentSource->m[parentWhere].endObjectPosition-1].word->first==imChild->word->first)
			return true;
		int confidence=-1;
		if ((confidence=childSource->checkParticularPartSemanticMatch(LOG_WHERE,childWhere,parentSource,parentWhere,-1,synonym,semanticMismatch))<CONFIDENCE_NOMATCH)
		{
			if (imChild->beginObjectPosition>=0 && childSource->m[imChild->beginObjectPosition].word->first==L"a")
			{
				if (confidence>1 || synonym) return false;
				synonym=true; // decrease match
			}
			return true;
		}
		return false;
	}
	// parent=the prize / child=the Nobel Peace Prize
	if (!parentIsDefiniteObject && childIsDefiniteObject)
	{
		if (!childSource->objects[childObject].isWikiPerson &&
			childSource->m[imChild->endObjectPosition-1].word->first==parentSource->m[parentWhere].word->first)
			return true;
		// parent=persons / child = Jared Loughner
		if (childSource->objects[childObject].isWikiPerson &&
			parentSource->matchChildSourcePositionSynonym(Words.query(L"person"),parentSource,parentWhere))
			return true;
		if (childSource->objects[childObject].isWikiPlace && 
			parentSource->matchChildSourcePositionSynonym(Words.query(L"place"),parentSource,parentWhere))
			return true;
		if (childSource->objects[childObject].isWikiBusiness &&
			parentSource->matchChildSourcePositionSynonym(Words.query(L"business"),parentSource,parentWhere))
			return true;
		if (childSource->objects[childObject].isWikiWork &&
				(parentSource->matchChildSourcePositionSynonym(Words.query(L"book"),parentSource,parentWhere) ||
					parentSource->matchChildSourcePositionSynonym(Words.query(L"album"),parentSource,parentWhere) ||
					parentSource->matchChildSourcePositionSynonym(Words.query(L"song"),parentSource,parentWhere)))
			return true;
		// parent=US government officials / child=President George W . Bush
		int parentObject= parentSource->m[parentWhere].getObject();
		if (parentSource->m[parentWhere].objectMatches.size()>0)
			parentObject= parentSource->m[parentWhere].objectMatches[0].object;
		if (parentObject>=0 && (parentSource->objects[parentObject].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			parentSource->objects[parentObject].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
			parentSource->objects[parentObject].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
			parentSource->objects[parentObject].objectClass==GENDERED_RELATIVE_OBJECT_CLASS) &&
				(!childSource->objects[childObject].isWikiPerson || (childSource->objects[childObject].neuter && !childSource->objects[childObject].female && !childSource->objects[childObject].male)))
				return false;
		if (childSource->objects[childObject].isWikiPerson && childSource->objects[childObject].objectClass==NAME_OBJECT_CLASS)
			return checkParentGroup(parentSource,parentWhere,childSource,childWhere,childObject,synonym,semanticMismatch)<CONFIDENCE_NOMATCH;
		if (parentSource->checkParticularPartSemanticMatch(LOG_WHERE,parentWhere,childSource,childWhere,childObject,synonym,semanticMismatch)<CONFIDENCE_NOMATCH)
			return true;
		return false;
	}
	if (matched)
	{
		// but not 'which prize' and 'the prize'
		if (parentInQuestionObject && !childIsDefiniteObject && imChild->queryWinnerForm(nounForm)>=0 && parentSource->m[parentWhere].queryWinnerForm(nounForm)>=0)
		{
			bool isGeneric=imChild->beginObjectPosition<0;
			if (!isGeneric && imChild->objectMatches.empty())
			{
				isGeneric=(imChild->beginObjectPosition>=0 && imChild->endObjectPosition>=0 && 
						(imChild->endObjectPosition-imChild->beginObjectPosition)==2 &&
							(childSource->m[imChild->beginObjectPosition].queryWinnerForm(determinerForm)>=0 ||
							childSource->m[imChild->beginObjectPosition].queryWinnerForm(possessiveDeterminerForm)>=0 ||
							childSource->m[imChild->beginObjectPosition].queryWinnerForm(demonstrativeDeterminerForm)>=0));
				// Darrell Hammond went to the same university as me. // althouse.blogspot.com/2011/.../snls-darrell-hammond-was-victim-of.htm...‎
				isGeneric|=(imChild->beginObjectPosition>=0 && imChild->endObjectPosition>=0 && 
						(imChild->endObjectPosition-imChild->beginObjectPosition)==3 &&
							(childSource->m[imChild->beginObjectPosition].queryWinnerForm(determinerForm)>=0 ||
							childSource->m[imChild->beginObjectPosition].queryWinnerForm(possessiveDeterminerForm)>=0 ||
							childSource->m[imChild->beginObjectPosition].queryWinnerForm(demonstrativeDeterminerForm)>=0) &&
							childSource->m[imChild->beginObjectPosition+1].word->first==L"same");
			}
			if (!isGeneric && imChild->objectMatches.size()!=0)
			{ 
				int cwo=imChild->objectMatches[0].object;
				if (cwo>=0 && childSource->objects[cwo].originalLocation>=0)
				{
					int cw=childSource->objects[cwo].originalLocation;
					isGeneric=(childSource->m[cw].beginObjectPosition>=0 && childSource->m[cw].endObjectPosition>=0 && 
						(childSource->m[cw].endObjectPosition-childSource->m[cw].beginObjectPosition)==2 &&
							(childSource->m[childSource->m[cw].beginObjectPosition].queryWinnerForm(determinerForm)>=0 ||
							childSource->m[childSource->m[cw].beginObjectPosition].queryWinnerForm(possessiveDeterminerForm)>=0 ||
							childSource->m[childSource->m[cw].beginObjectPosition].queryWinnerForm(demonstrativeDeterminerForm)>=0));
				}
			}
			if (isGeneric)
			{
				if (logQuestionDetail)
				{
					wstring tmpstr,tmpstr2;
					lplog(LOG_WHERE,L"rejected generic answer: parent %s and child %s",parentSource->whereString(parentWhere,tmpstr,false).c_str(),childSource->whereString(childWhere,tmpstr2,false).c_str(),(matched) ? L"true":L"false");
				}
				namedNoMatch=true;
				return false;
			}
		}
		if (imChild->word->first==parentSource->m[parentWhere].word->first && imChild->queryWinnerForm(nounForm)>=0 && parentSource->m[parentWhere].queryWinnerForm(nounForm)>=0)
		{
			wstring tmpstr,tmpstr2;
			lplog(LOG_WHERE,L"matchSourcePositions:parentObject %s against child object %s succeeded (match primary word %s)",
				parentSource->whereString(parentWhere,tmpstr,true).c_str(),childSource->whereString(childWhere,tmpstr2,true).c_str(),imChild->word->first.c_str());
			adjectivalMatch = 0;
			if (childSource->m[childWhere].beginObjectPosition < childWhere && parentSource->m[parentWhere].beginObjectPosition < parentWhere)
			{
				if (childSource->m[childWhere - 1].word->first == parentSource->m[parentWhere - 1].word->first)
					adjectivalMatch = 1;
				else
				{
					set <wstring> synonyms;
					childSource->getSynonyms(childSource->m[childWhere - 1].word->first, synonyms, NOUN);
					if (logSynonymDetail)
						lplog(LOG_WHERE, L"TSYM [1-1] comparing %s against synonyms [%s]%s", parentSource->m[parentWhere - 1].getMainEntry()->first.c_str(), childSource->m[childWhere - 1].word->first.c_str(), setString(synonyms, tmpstr, L"|").c_str());
					if (synonyms.find(parentSource->m[parentWhere - 1].getMainEntry()->first) != synonyms.end())
						adjectivalMatch = 2;
					else
						adjectivalMatch = 3;
				}
			}
			return true;
		}
		if (parentSource->m[parentWhere].beginObjectPosition<0 && imChild->beginObjectPosition<0)
		{
			set <wstring> synonyms;
			wstring tmpstr;
			childSource->getSynonyms(imChild->word->first,synonyms, NOUN);
			if (logSynonymDetail)
				lplog(LOG_WHERE, L"TSYM [1-1] comparing %s against synonyms [%s]%s", parentSource->m[parentWhere].getMainEntry()->first.c_str(), imChild->word->first.c_str(), setString(synonyms, tmpstr, L"|").c_str());
  		if (synonyms.find(parentSource->m[parentWhere].getMainEntry()->first)!=synonyms.end())
				return synonym=true;
		}
		// time objects?
		if (parentSource->m[parentWhere].getObject()>=0 && parentSource->objects[parentSource->m[parentWhere].getObject()].isTimeObject && 
				childSource->m[childWhere].getObject()>=0 && childSource->objects[childSource->m[childWhere].getObject()].isTimeObject && 
				matchTimeObjects(parentSource,parentWhere,childSource,childWhere))
			return true;
		if (parentSource->m[parentWhere].objectMatches.empty() && parentSource->matchChildSourcePositionSynonym(parentSource->m[parentWhere].word,childSource,childWhere))
				return synonym=true;
		for (unsigned int mo=0; mo<parentSource->m[parentWhere].objectMatches.size(); mo++)
		{
			int pw=-2,pmo=parentSource->m[parentWhere].objectMatches[mo].object;
			if (pmo>=(signed)parentSource->objects.size() || (pw=parentSource->objects[pmo].originalLocation)<0 || pw>=(signed)parentSource->m.size())
				lplog(LOG_WHERE,L"ERROR: %d:offset illegal! %d %d %d %d",parentWhere,pmo,parentSource->objects.size(),pw,parentSource->m.size());
			else if	((parentSource->m[pw].endObjectPosition-parentSource->m[pw].beginObjectPosition)==1 &&
				parentSource->matchChildSourcePositionSynonym(parentSource->m[pw].word,childSource,childWhere))
				return synonym=true;
		}
	}
	return false;
}

int cQuestionAnswering::sriMatch(Source *questionSource,Source *childSource, int parentWhere, int childWhere, int whereQuestionType, __int64 questionType, bool &totalMatch, int cost)
{
	LFS
	totalMatch = false;
	if (parentWhere<0)
		return 0;
	bool inQuestionObject= questionSource->inObject(parentWhere,whereQuestionType);
  if (childWhere<0)
		return (inQuestionObject) ? -cost : 0;
	vector <WordMatch>::iterator imChild=childSource->m.begin()+childWhere;
	bool childIsPronoun=(imChild->objectMatches.empty() && (imChild->queryWinnerForm(nomForm)>=0 || imChild->queryWinnerForm(personalPronounAccusativeForm)>=0 ||
      imChild->queryWinnerForm(personalPronounForm)>=0 || imChild->queryWinnerForm(quantifierForm)>=0 ||
      imChild->queryWinnerForm(possessivePronounForm)>=0 ||
      imChild->queryWinnerForm(indefinitePronounForm)>=0 || imChild->queryWinnerForm(pronounForm)>=0 ||
			imChild->queryWinnerForm(demonstrativeDeterminerForm)>=0 ||
			imChild->queryWinnerForm(reflexivePronounForm)>=0 ||
			imChild->queryWinnerForm(reciprocalPronounForm)>=0));
  if (childIsPronoun)
		return (inQuestionObject) ? -cost : 0;
	if (inQuestionObject && !(questionType&QTAFlag) && questionType!=unknownQTFlag) // if in subquery, wait to see if it is a match, if so, boost
			return cost>>1;
	bool namedNoMatch=false,synonym=false;
	int semanticMismatch=0, adjectivalMatch=-1;
	//if (inQuestionObject && questionType==unknownQTFlag) // in subquery - boost
	//		cost<<=1;
	if (matchSourcePositions(questionSource,parentWhere,childSource,childWhere,namedNoMatch,synonym,inQuestionObject,semanticMismatch,adjectivalMatch, questionSource->debugTrace))
	{
		//if (inQuestionObject && questionType==unknownQTFlag) // in subquery - boost
		//	lplog(LOG_WHERE,L"%d:Boost found subquery match position - cost=%d!",childWhere,(synonym) ? cost*3/4 : cost);
		if (semanticMismatch)
			return -cost;
		totalMatch = !synonym;
		if (synonym)
			return cost * 3 / 4;
		// adjectives match exactly
		if (adjectivalMatch == 1)
			return cost * 2;
		// adjectives are synonyms
		if (adjectivalMatch == 2)
			return cost * 1.3;
		// adjectives do not match
		if (adjectivalMatch == 3)
			return cost * 3/4;
		return cost;
	}
	//if (inQuestionObject && questionType==unknownQTFlag) // in subquery
	//		return cost>>2;
	if (namedNoMatch)
		return -cost;
	return 0;
}

int cQuestionAnswering::sriVerbMatch(Source *parentSource,Source *childSource,int parentWhere,int childWhere,int cost)
{ LFS
	if (parentWhere<0 || childWhere<0)
		return 0;
	tIWMM childWord,parentWord;
	if ((childWord=childSource->m[childWhere].getMainEntry())==(parentWord=parentSource->m[parentWhere].getMainEntry()))
		return cost;
	set <wstring> childSynonyms;
	wstring tmpstr;
	childSource->getSynonyms(childWord->first,childSynonyms, VERB);
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"TSYM [VERB] comparing PARENT %s against synonyms [%s]%s", parentWord->first.c_str(), childWord->first.c_str(), setString(childSynonyms, tmpstr, L"|").c_str());
	if (childSynonyms.find(parentWord->first)!=childSynonyms.end())
		return cost*3/4;
	set <wstring> parentSynonyms;
	childSource->getSynonyms(parentWord->first,parentSynonyms, VERB);
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"TSYM [VERB] comparing CHILD %s against synonyms [%s]%s", childWord->first.c_str(), parentWord->first.c_str(), setString(parentSynonyms, tmpstr, L"|").c_str());
	if (parentSynonyms.find(childWord->first)!=parentSynonyms.end())
		return cost*3/4;
	if ((childWord->first==L"am" && (parentWord->second.timeFlags&31)==T_START) || (parentWord->first==L"am" && (childWord->second.timeFlags&31)==T_START))
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"TSYM [VERBBEGINBEING] matched CHILD %s against PARENT %s",childWord->first.c_str(),parentWord->first.c_str());
	  return cost/2;
	}
	return 0;
}

int cQuestionAnswering::sriPrepMatch(Source *parentSource, Source *childSource,int parentWhere,int childWhere,int cost)
{ LFS
	if (parentWhere<0 || childWhere<0)
		return 0;
	if (childSource->m[childWhere].word== parentSource->m[parentWhere].word)
		return cost;
	return 0;
}

// transform childSRI into another form and match against parent
// equivalence class 1:
//    if subject matches and verb is a 'to BE' verb AND the object is a profession or an agentive nominalization (a noun derived from a verb [runner])
//      and the equivalent verb can be identified, match parent against equivalent verb and any prep phrases from child.
int cQuestionAnswering::equivalenceClassCheck(Source *questionSource,Source *childSource,vector <cSpaceRelation>::iterator childSRI,cSpaceRelation* parentSRI,int whereChildSpecificObject,int &equivalenceClass,int matchSum)
{ LFS
	// 'to be' form
	if (childSRI->whereVerb<0 || !childSource->m[childSRI->whereVerb].forms.isSet(isForm)) return 0;
	// object must be a profession 'professor' or an agentive nominalization or an honorific 'general'
	if (whereChildSpecificObject<0) return 0;
	// object length is longer than 1 word - check the last word in "Centenary Professor", not the first one.
	if (childSource->m[whereChildSpecificObject].getObject()>0 && childSource->m[whereChildSpecificObject].endObjectPosition>=0 && 
			(childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass==NAME_OBJECT_CLASS || 
			 childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
		whereChildSpecificObject=childSource->m[whereChildSpecificObject].endObjectPosition-1;
	unordered_map <wstring,set < wstring > >::iterator nvi=nounVerbMap.find(childSource->m[whereChildSpecificObject].word->first);
	// professor -> profess
	if (nvi==nounVerbMap.end()) 
	{
		if (logEquivalenceDetail)
			lplog(LOG_WHERE,L"%d:noun %s not found",whereChildSpecificObject,childSource->m[whereChildSpecificObject].word->first.c_str());
		return 0;
	}
	tIWMM childConvertedVerb=Words.query(*nvi->second.begin());
	if (childConvertedVerb==Words.end()) 
	{
		if (logEquivalenceDetail)
			lplog(LOG_WHERE,L"%d:verb %s from noun %s not found",whereChildSpecificObject,nvi->second.begin()->c_str(),childSource->m[whereChildSpecificObject].word->first.c_str());
		return 0;
	}
	tIWMM childConvertedVerbME=(childConvertedVerb->second.mainEntry==wNULL) ? childConvertedVerb : childConvertedVerb->second.mainEntry;
	wstring tmpstr;
	if (childConvertedVerbME!= questionSource->m[parentSRI->whereVerb].getMainEntry())
	{
		bool match=false;
		// profess synonyms = teach...
		set <wstring> synonyms;
		childSource->getSynonyms(childConvertedVerbME->first,synonyms,VERB);
		// does the verb match any synonym?
		for (set <wstring>::iterator si=synonyms.begin(),siEnd=synonyms.end(); si!=siEnd && !match; si++)
		{
			tIWMM childConvertedSynonym=Words.query(*si);
			tIWMM childConvertedSynonymME=(childConvertedSynonym==Words.end() || childConvertedSynonym->second.mainEntry==wNULL) ? childConvertedSynonym : childConvertedSynonym->second.mainEntry;
			match=(childConvertedSynonymME!=Words.end() && childConvertedSynonymME==questionSource->m[parentSRI->whereVerb].getMainEntry());
		}
		if (!match) 
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE,L"%d:%s:convertedVerb %s[ME %s] nor synonyms (%s) match parentVerb %s[ME %s].",whereChildSpecificObject,childSource->m[whereChildSpecificObject].word->first.c_str(),
				childConvertedVerb->first.c_str(), childConvertedVerbME->first.c_str(), setString(synonyms, tmpstr, L"|").c_str(), questionSource->m[parentSRI->whereVerb].word->first.c_str(), questionSource->m[parentSRI->whereVerb].getMainEntry()->first.c_str());
			return 0;
		}
		if (logQuestionDetail)
			lplog(LOG_WHERE,L"%d:equivalence class check succeeded:"
									L"%s:convertedVerb %s[ME %s] synonyms (%s) matched parentVerb %s[ME %s].",whereChildSpecificObject,childSource->m[whereChildSpecificObject].word->first.c_str(),
									childConvertedVerb->first.c_str(), childConvertedVerbME->first.c_str(), setString(synonyms, tmpstr, L"|").c_str(), questionSource->m[parentSRI->whereVerb].word->first.c_str(), questionSource->m[parentSRI->whereVerb].getMainEntry()->first.c_str());
	}
	else if (logQuestionDetail)
		lplog(LOG_WHERE,L"%d:equivalence class check succeeded:"
									L"%s:convertedVerb %s[ME %s] matched parentVerb %s[ME %s].",whereChildSpecificObject,childSource->m[whereChildSpecificObject].word->first.c_str(),
					childConvertedVerb->first.c_str(),childConvertedVerbME->first.c_str(), questionSource->m[parentSRI->whereVerb].word->first.c_str(), questionSource->m[parentSRI->whereVerb].getMainEntry()->first.c_str());
	equivalenceClass=1;
	return matchSum;
}

int cQuestionAnswering::equivalenceClassCheck2(Source *questionSource, Source *childSource,vector <cSpaceRelation>::iterator childSRI,cSpaceRelation* parentSRI,int whereChildSpecificObject,int &equivalenceClass,int matchSum)
{ LFS
	// 'to be' form
	if (childSRI->whereVerb<0 || !childSource->m[childSRI->whereVerb].forms.isSet(isForm)) return 0;
	// object must be a profession 'professor' or an agentive nominalization or an honorific 'general'
	if (whereChildSpecificObject<0) return 0;
	// object length is longer than 1 word - check the last word in "Centenary Professor", not the first one.
	if (childSource->m[whereChildSpecificObject].getObject()>0 && childSource->m[whereChildSpecificObject].endObjectPosition>=0 && 
			(childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass==NAME_OBJECT_CLASS || 
			 childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
		whereChildSpecificObject=childSource->m[whereChildSpecificObject].endObjectPosition-1;
	set <wstring> synonyms;
	childSource->getSynonyms(childSource->m[whereChildSpecificObject].word->first,synonyms,NOUN);
	for (set <wstring>::iterator si=synonyms.begin(),siEnd=synonyms.end(); si!=siEnd; si++)
	{
		unordered_map <wstring,set < wstring > >::iterator nvi=nounVerbMap.find(*si);
		// teacher -> teach
		if (nvi==nounVerbMap.end()) 
		{
			if (logSynonymDetail)
				lplog(LOG_WHERE,L"%d:TSYM synonym noun %s not found",whereChildSpecificObject,si->c_str());
			continue;
		}
		tIWMM childConvertedVerb=Words.query(*nvi->second.begin());
		if (childConvertedVerb==Words.end()) 
		{
			//const wchar_t *ch=nvi->second.begin()->c_str();
			if (logSynonymDetail)
				lplog(LOG_WHERE,L"%d:SYNONYM of %s(%s):verb %s not found",whereChildSpecificObject,childSource->m[whereChildSpecificObject].word->first.c_str(),si->c_str(),nvi->second.begin()->c_str());
			continue;
		}
		tIWMM childConvertedVerbME=(childConvertedVerb->second.mainEntry==wNULL) ? childConvertedVerb : childConvertedVerb->second.mainEntry;
		if (childConvertedVerbME!= questionSource->m[parentSRI->whereVerb].getMainEntry())
		{
			if (logEquivalenceDetail)
				lplog(LOG_WHERE,L"%d:convertedVerb %s[ME %s] doesn't match parentVerb %s[ME %s].",whereChildSpecificObject,
					childConvertedVerb->first.c_str(),childConvertedVerbME->first.c_str(), questionSource->m[parentSRI->whereVerb].word->first.c_str(), questionSource->m[parentSRI->whereVerb].getMainEntry()->first.c_str());
			continue;
		}
		lplog(LOG_WHERE,L"%d:equivalence class (2) check succeeded (verb %s from synonym noun %s):",whereChildSpecificObject,nvi->second.begin()->c_str(),si->c_str());
		equivalenceClass=2;
		return matchSum;
	}
	return 0;
}

void appendSum(int sum,wchar_t *str,wstring &matchInfo)
{ LFS
	if (sum==0) return;
	wstring writableStr=str;
	if (sum<0) writableStr[0]=L'-';
	if (sum>0) writableStr[0]=L'+';
	itos((wchar_t *)writableStr.c_str(),sum,matchInfo,L"]");
}

void cQuestionAnswering::enterAnswer(Source *questionSource,wchar_t *derivation,wstring childSourceType,Source *childSource,cSpaceRelation* parentSRI,vector < cAS > &answerSRIs,int &maxAnswer,int rejectDuplicatesFrom,int matchSum,
	                       wstring matchInfo,vector <cSpaceRelation>::iterator childSRI,int equivalenceClass,int ws,int wo,int wp,int &answersContainedInSource)
{ LFS
	cAS as(childSourceType,childSource,-1,matchSum,matchInfo,&(*childSRI),equivalenceClass,ws,wo,wp);				
	bool identical=false,identicalWithinOwnSource=false;
	for (unsigned int I=0; I<answerSRIs.size() && !identicalWithinOwnSource; I++)
	{ LFSL
		if ((identical=checkIdentical(questionSource,parentSRI,answerSRIs[I],as) && as.equivalenceClass==answerSRIs[I].equivalenceClass))
		{
			if (!(identicalWithinOwnSource=(int)I>=rejectDuplicatesFrom)) 
				continue;
			lplog(LOG_WHERE,L"%s:%s with %d:Source:%s",derivation,(answerSRIs[I].matchSum<as.matchSum) ? L"substituted":L"identical",I,answerSRIs[I].sourceType.c_str());
			wstring ps;
			answerSRIs[I].source->prepPhraseToString(answerSRIs[I].wp,ps);
			answerSRIs[I].source->printSRI(L"    [identical] ",answerSRIs[I].sri,0,answerSRIs[I].ws,answerSRIs[I].wo,ps,false,answerSRIs[I].matchSum,answerSRIs[I].matchInfo);
			if (answerSRIs[I].matchSum<as.matchSum)
			{
				answersContainedInSource++;
				answerSRIs[I]=as;
			}
		}
	}
	if (!identicalWithinOwnSource)
	{
		maxAnswer=max(maxAnswer,matchSum);
		answerSRIs.push_back(as);
		as.identicalWithAnswerFromAnotherSource=identical;
		lplog(LOG_WHERE,L"%s:entered as answer %d:[%s]:Source:%s",derivation,answerSRIs.size()-1,as.matchInfo.c_str(),as.sourceType.c_str());
		answersContainedInSource++;
		wstring ps;
		as.source->prepPhraseToString(as.wp,ps);
		as.source->printSRI(L"    [entered as answer] ",as.sri,0,as.ws,as.wo,ps,false,matchSum,matchInfo);
	}
}

/* 
ANSWER
9:[] root=0 CONTAINS_FUTURE_REFERENCE INDIRECT_FUTURE_REFERENCE {_QUESTION}
ancestors:
DESCENDANT TAGS:_VERB_SENSE _AGREEMENT _SUBJECT _SPECIFIC_ANAPHOR _NOUN_DETERMINER _OBJECTS _ROLE _SUBJECT_VERB_RELATION _NAME _META_NAME_EQUIVALENCE _META_SPEAKER _BNC_PREFERENCES_TAGSET _N_AGREE_TAGSET _EVAL_TAGSET _NOTBUT_TAGSET _MOBJECT_TAGSET _NDP _TIME 
location 51 mapped to variable G.
location 52 mapped to variable A.
location 53 mapped to variable X.
0:1-1  _META_NAME_EQUIVALENCE[1]*0|0|0 _META_NAME_EQUIVALENCE[2]*0|0|0 _META_NAME_EQUIVALENCE[T]*0|0|0 _META_NAME_EQUIVALENCE[3]*0|0|0 _META_NAME_EQUIVALENCE[4]*0|0|0 _META_NAME_EQUIVALENCE[5]*0|0|0 _META_NAME_EQUIVALENCE[6]*0|0|0 _META_NAME_EQUIVALENCE[7]*0|0|0 _META_NAME_EQUIVALENCE[8]*0|0|0 _META_NAME_EQUIVALENCE[9]*0|0|0 _META_NAME_EQUIVALENCE[R]*0|0|0 _META_NAME_EQUIVALENCE[Q]*0|0|0 _META_NAME_EQUIVALENCE[A]*0|0|0 _META_NAME_EQUIVALENCE[B]*0|0|0 _META_NAME_EQUIVALENCE[C]*0|0|0 _META_NAME_EQUIVALENCE[D]*0|0|0 _META_NAME_EQUIVALENCE[E]*0|0|0 _META_NAME_EQUIVALENCE[F]*0|0|0 _META_NAME_EQUIVALENCE[G]*0|0|0 _META_NAME_EQUIVALENCE[R]*0|0|0 _META_NAME_EQUIVALENCE[H]*0|0|0 _META_NAME_EQUIVALENCE[J]*0|0|0 _META_NAME_EQUIVALENCE[K]*0|0|0 _META_NAME_EQUIVALENCE[M]*0|0|0 _META_NAME_EQUIVALENCE[N]*0|0|0 _META_NAME_EQUIVALENCE[P]*0|0|0 _META_NAME_EQUIVALENCE[S]*0|0|0 
1:1-1  noun|NAME_PRIMARY*0|0|0 
2:1-1  noun|NAME_SECONDARY*0|0|0 

QUESTION
8:[] root=0 CONTAINS_FUTURE_REFERENCE {_QUESTION}
ancestors:
DESCENDANT TAGS:_VERB_SENSE _NAME _BNC_PREFERENCES_TAGSET _NOTBUT_TAGSET _TIME 
variable B mapped to location 9.
variable X mapped to location 10.
variable B mapped to length 1.
variable X mapped to length 1.
location 9 mapped to variable B.
location 10 mapped to variable X.
0:1-1  relativizer|what*0|0|0 
1:1-1  is|is*0|0|0 is|was*0|0|0 
2:1-1  _NAMEOWNER[]*0|0|0 
3:1-1  adjective|real*0|0|0 
4:1-1  noun|name*0|0|0 
*/
int cQuestionAnswering::metaPatternMatch(Source *questionSource,Source *childSource,vector <cSpaceRelation>::iterator childSRI,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion)
{ LFS
	// match childSource, at childSRI->where to pattern defined by mapPatternAnswer[0]
	int element,maxEnd,nameEnd=-1,lastWhere=-1; // idExemption=-1,
	for (int mi=childSRI->printMin; mi<childSRI->printMax; mi++)
	{
		if ((element=childSource->queryPattern(mi,mapPatternAnswer->getElement(0)->patternStr[0],maxEnd))==-1)
			continue;
		int whereMNE=mi+childSource->pema[element].begin;
		if ((element=childSource->m[whereMNE].pma.queryPattern(mapPatternAnswer->getElement(0)->patternStr[0],nameEnd))==-1) 
			continue;
		if (lastWhere==whereMNE)
			continue;
		mapPatternAnswer->lplog();
		mapPatternQuestion->lplog();
		lastWhere=whereMNE;
		vector < vector <tTagLocation> > tagSets;
		if (childSource->startCollectTags(true,metaNameEquivalenceTagSet,whereMNE,childSource->m[whereMNE].pma[element&~matchElement::patternFlag].pemaByPatternEnd,tagSets,false,true,L"meta pattern match")>0)
			for (unsigned int J=0; J<tagSets.size(); J++)
			{
				
				childSource->printTagSet(LOG_WHERE,L"MNE",J,tagSets[J],whereMNE,childSource->m[whereMNE].pma[element&~matchElement::patternFlag].pemaByPatternEnd);
				// collect tag for each of the rest of the elements
				unordered_map <int,wstring>::iterator lvmi=mapPatternAnswer->locationToVariableMap.begin();
				lvmi++;
				int whereAnswer=-1,matchingElements=0,answerTagLen=-1;
				vector <wstring> parameters;
				for (unsigned int e=1; e<mapPatternAnswer->numElements(); e++,lvmi++)
				{
					wstring varName=lvmi->second;
					int tag=findOneTag(tagSets[J],(wchar_t *)mapPatternAnswer->getElement(e)->specificWords[0].c_str(),-1);
					if (tag<0) continue;
					int whereTag=tagSets[J][tag].sourcePosition;
					if (tagSets[J][tag].len>1 && childSource->m[whereTag].principalWherePosition>=0) // could be an adjective
						whereTag=childSource->m[whereTag].principalWherePosition;
					wstring childVarNameValue;
					childSource->whereString(whereTag,childVarNameValue,true);
					if (varName!=L"A")
					{
						// find location of mapPatternQuestion
						int parentWhere=mapPatternQuestion->variableToLocationMap[varName];
						wstring parentVarNameValue;
						questionSource->whereString(parentWhere,parentVarNameValue,true);
						if (childVarNameValue!=parentVarNameValue)
							continue;
						matchingElements++;
						wstring tmpstr;
						lplog(LOG_WHERE,L"%d:meta matched parameter [%s]=parent [%d:%s] = child [%d:%s].",whereMNE,varName.c_str(),parentWhere,parentVarNameValue.c_str(),whereTag,childVarNameValue.c_str());
						parameters.push_back(childVarNameValue);
					}
					else
					{
						whereAnswer=whereTag;
						answerTagLen=tagSets[J][tag].len;
					}
				}
				if (whereAnswer!=-1 && matchingElements>0 && childSource->m[whereAnswer].getObject()>=0 && answerTagLen==childSource->m[whereAnswer].endObjectPosition-childSource->m[whereAnswer].beginObjectPosition)
				{
					wstring childVarNameValue;
					childSource->whereString(whereAnswer,childVarNameValue,true);
					bool parameterMatchesAnswer=false;
					for (vector <wstring>::iterator pi=parameters.begin(),piEnd=parameters.end(); pi!=piEnd && !parameterMatchesAnswer; pi++)
						parameterMatchesAnswer=*pi==childVarNameValue;
					if (parameterMatchesAnswer)
						continue;
					// test only for patterns diff 8,9,G
					vector <tIWMM> parentWords;
					wstring diff=patterns[childSource->m[whereMNE].pma[element&~matchElement::patternFlag].getPattern()]->differentiator;
					lplog(LOG_WHERE,L"%d:meta matched pattern %s[%s]",whereMNE,patterns[childSource->m[whereMNE].pma[element&~matchElement::patternFlag].getPattern()]->name.c_str(),patterns[childSource->m[whereMNE].pma[element&~matchElement::patternFlag].getPattern()]->differentiator.c_str());
					vector <wchar_t *> checkVerbs;
					if (diff==L"8")
					{
						wchar_t *checkVerbsArray[]={ L"know",L"call",L"name",0 };
						for (int w=0; checkVerbsArray[w]; w++)
							checkVerbs.push_back(checkVerbsArray[w]);
					}
					if (diff==L"9")
					{
						wchar_t *checkVerbsArray[]={ L"look",L"appear",0 };
						for (int w=0; checkVerbsArray[w]; w++)
							checkVerbs.push_back(checkVerbsArray[w]);
					}
					if (diff==L"G")
					{
						wchar_t *checkVerbsArray[]={ L"recognize",0 };
						for (int w=0; checkVerbsArray[w]; w++)
							checkVerbs.push_back(checkVerbsArray[w]);
					}
					tIWMM childWord;
					wstring tmpstr;
					if (checkVerbs.size() && childSource->m[whereAnswer].getRelVerb()>=0)
						childWord=childSource->m[childSource->m[whereAnswer].getRelVerb()].getMainEntry();
					bool foundMatch=checkVerbs.empty();
					for (unsigned int cv=0; cv<checkVerbs.size() && !foundMatch; cv++)
					{
						tIWMM checkWord=Words.query(checkVerbs[cv]);
						if (checkWord!=Words.end())
						{
							if (!(foundMatch=(checkWord==childWord)))
							{
								set <wstring> checkSynonyms;
								questionSource->getSynonyms(checkWord->first,checkSynonyms, VERB);
								if (logSynonymDetail)
									lplog(LOG_WHERE, L"TSYM [VERB] comparing CHECK %s and synonyms [%s] against %s", checkWord->first.c_str(), setString(checkSynonyms, tmpstr, L"|").c_str(), childWord->first.c_str());
								foundMatch= (checkSynonyms.find(checkWord->first)!=checkSynonyms.end());
							}
						}
					}
					if (childSource->m[whereAnswer].getObject()>=0 && childSource->objects[childSource->m[whereAnswer].getObject()].objectClass==NAME_OBJECT_CLASS && foundMatch)
					{
						wstring tmpstr2;
						lplog(LOG_WHERE,L"%d:meta matched answer=%s.\n%s",
							whereMNE,childSource->whereString(whereAnswer,tmpstr,false).c_str(),
							childSource->phraseString(childSRI->printMin,childSRI->printMax,tmpstr2,false).c_str());
						return whereAnswer;
					}
					else
					{
						wstring tmpstr2;
						lplog(LOG_WHERE,L"%d:meta rejected answer=%s.\n%s",
							whereMNE,childSource->whereString(whereAnswer,tmpstr,false).c_str(),
							childSource->phraseString(childSRI->printMin,childSRI->printMax,tmpstr2,false).c_str());
					}
				}
			}
	}
	return -1;
}

// parse comment/abstract of rdfType.  
// For each sentence, 
//    match preferentially the subject/verb/object/prep?/prepObject, 
//    with special analysis match of the whereQuestionTargetSuggestionIndex, if sri->questionType is an adjective type.
//    answer is the special analysis match.
int cQuestionAnswering::analyzeQuestionFromSource(Source *questionSource,wchar_t *derivation,wstring childSourceType,Source *childSource, cSpaceRelation * parentSRI,vector < cAS > &answerSRIs,int &maxAnswer,int rejectDuplicatesFrom,bool eraseIfNoAnswers,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion)
{ LFS
	int answersContainedInSource=0,currentPercent,lastProgressPercent=-1,startClockTime=clock();
	if (logQuestionDetail)
		lplog(LOG_WHERE,L"********[%s used %d times:%d total sources] %s:",derivation,childSource->numSearchedInMemory,sourcesMap.size(),childSource->sourcePath.c_str());
	else
		lplog(LOG_WHERE,L"********[%s] %s:",derivation,childSource->sourcePath.c_str());
	childSource->parentSource=NULL; // this is set so we can investigate child sources through identification of ISA types
	accumulateSemanticMaps(questionSource,parentSRI,childSource,childSource->sourceType==Source::WIKIPEDIA_SOURCE_TYPE);
	if (!childSource->isFormsProcessed)
	{
		childSource->isFormsProcessed=true;
		int numSR=childSource->spaceRelations.size(),I=0;
		for (vector <cSpaceRelation>::iterator childSRI=childSource->spaceRelations.begin(); childSRI!=childSource->spaceRelations.end() && I<numSR; childSRI++,I++)
		{ LFSL
			if (childSRI->whereVerb>=0 && childSource->m[childSRI->whereVerb].queryWinnerForm(isForm)>=0 && childSRI->whereSubject>=0 && childSRI->whereObject>=0)
			{ LFSL
				if (logQuestionDetail)
					childSource->printSRI(L"IS SWITCH",&(*childSRI),0,childSRI->whereSubject,childSRI->whereObject,L"",false,-1,L"");
				vector <cSpaceRelation>::iterator endCopySRI=childSource->spaceRelations.insert(childSource->spaceRelations.end(),*childSRI);
				childSRI=childSource->spaceRelations.begin()+I;
				endCopySRI->whereSubject=childSRI->whereObject;
				endCopySRI->whereObject=childSRI->whereSubject;
			}
		}
	}
	bool questionTypeSubject= questionSource->inObject(parentSRI->whereSubject,parentSRI->whereQuestionType);
	bool questionTypeObject= questionSource->inObject(parentSRI->whereObject,parentSRI->whereQuestionType);
	bool questionTypePrepObject= questionSource->inObject(parentSRI->wherePrepObject,parentSRI->whereQuestionType);
	for (vector <cSpaceRelation>::iterator childSRI=childSource->spaceRelations.begin(), sriEnd=childSource->spaceRelations.end(); childSRI!=sriEnd; childSRI++)
	{ LFSL
	  if (logQuestionDetail)
		{
			wstring ps;
			childSource->prepPhraseToString(childSRI->wherePrep,ps);
			childSource->printSRI(L"    [printall] ",&(*childSRI),0,childSRI->whereSubject,childSRI->whereObject,ps,false,-1,L"");
		}
    if ((currentPercent=(childSRI-childSource->spaceRelations.begin())*100/childSource->spaceRelations.size())>lastProgressPercent)
    {
      wprintf(L"PROGRESS: %03d%% child relations processed with %04d seconds elapsed \r",currentPercent,(int)((clock()-startClockTime)/CLOCKS_PER_SEC));
      lastProgressPercent=currentPercent;
    }
		// child cannot be question
		bool inQuestion=(childSRI->whereSubject>=0 && (childSource->m[childSRI->whereSubject].flags&WordMatch::flagInQuestion));
		inQuestion|=(childSRI->whereObject>=0 && (childSource->m[childSRI->whereObject].flags&WordMatch::flagInQuestion));
		if (inQuestion) continue;
		// check for negation agreement
		if (childSRI->tft.negation ^ parentSRI->tft.negation)
			continue;
		// check for who / who his real name is / who is object in relative clause / SRI is constructed dynamically
		if (childSRI->whereObject>=0 && childSRI->whereSubject>=0 && childSRI->whereObject<childSRI->whereSubject &&
			  childSource->m[childSRI->whereSubject].getRelObject()<0 && 
				(childSource->m[childSRI->where].objectRole&SENTENCE_IN_REL_ROLE) && childSource->m[childSRI->whereSubject].beginObjectPosition>0 && 
				childSource->m[childSource->m[childSRI->whereSubject].beginObjectPosition-1].queryWinnerForm(relativizerForm)>=0 && 
				(childSource->m[childSource->m[childSRI->whereSubject].beginObjectPosition-1].flags&WordMatch::flagRelativeObject) &&
				childSRI->whereObject==childSource->m[childSource->m[childSRI->whereSubject].beginObjectPosition-1].getRelObject())
			continue;
		int whereMetaPatternAnswer=-1;
		if (mapPatternAnswer!=NULL && (whereMetaPatternAnswer=metaPatternMatch(questionSource,childSource,childSRI,mapPatternAnswer,mapPatternQuestion))>=0)
				enterAnswer(questionSource,derivation,childSourceType,childSource,parentSRI,answerSRIs,maxAnswer,rejectDuplicatesFrom,22,L"META_PATTERN",childSRI,0,whereMetaPatternAnswer,-1,-1,answersContainedInSource);
		for (int ws=childSRI->whereSubject; true; ws=childSource->m[ws].nextCompoundPartObject)
		{ LFSL
			if ((ws != childSRI->whereSubject && ws<0) || ws >= (int)childSource->m.size()) break;
			int matchSumSubject;
			wstring matchInfoDetailSubject;
			if (parentSRI->subQuery)
			{
				matchSumSubject=(checkParticularPartIdentical(questionSource,childSource,parentSRI->whereSubject,ws)) ? 8 : -1;
				matchInfoDetailSubject=(matchSumSubject<0) ? L"[SUBQUERY_NOT_IDENTICAL]":L"[SUBQUERY_IDENTICAL]";
			}
			else
				matchSumSubject = sriMatch(questionSource,childSource, parentSRI->whereSubject, ws, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticSubjectTotalMatch, 8);
			if (parentSRI->whereQuestionInformationSourceObjects.find(parentSRI->whereSubject)!=parentSRI->whereQuestionInformationSourceObjects.end() && matchSumSubject<8)
			{
				matchSumSubject=0;
				matchInfoDetailSubject+=L"[QUERY_INFORMATION_SOURCE_NOT_IDENTICAL_MATCH]";
			}
			if (childSRI->whereSubject >= 0 && childSource->m[childSRI->whereSubject].objectMatches.size()>1 && matchSumSubject>=0)
			{
				matchSumSubject/=childSource->m[childSRI->whereSubject].objectMatches.size();
				wstring subjectString;
				childSource->whereString(childSRI->whereSubject, subjectString, false);
				matchInfoDetailSubject += L"[MULTIPLE_CHILD_MATCH_SUBJ("+subjectString+L")]";
			}
			if (matchSumSubject<0) 
			{
				if (ws<0) break;
				continue;
			}
			int verbMatch=sriVerbMatch(questionSource,childSource,parentSRI->whereVerb,childSRI->whereVerb,8);
			bool subjectMatch=matchSumSubject>0;
			bool inSubQuery=parentSRI->questionType==unknownQTFlag;
			if (verbMatch<=0 && !subjectMatch && !questionTypeSubject && !inSubQuery)
			{
				if (ws<0 || childSource->m[ws].nextCompoundPartObject<0 || ws>=childSource->m.size()) break;
				continue;
			}
			//if (matchSumSubject>0 && inSubQuery)
			//{
			//	wstring tmp1,tmp2;
			//	lplog(LOG_WHERE,L"Comparing subjects %d [%d, %d] %s and %s.",matchSumSubject,parentSRI->whereSubject,ws,whereString(parentSRI->whereSubject,tmp1,false).c_str(),childSource->whereString(ws,tmp2,false).c_str());
			//}
			for (int wo=childSRI->whereObject; true; wo=childSource->m[wo].nextCompoundPartObject)
			{ LFSL
				if ((wo!=childSRI->whereObject && wo<0) || wo>=(int)childSource->m.size()) break;
				if (wo!=childSRI->whereObject && logQuestionDetail)
				{
					wstring ps;
					childSource->prepPhraseToString(childSRI->wherePrep,ps);
					childSource->printSRI(L"    [printallCO] ",&(*childSRI),0,ws,wo,ps,false,-1,L"");
				}
				set<int> whereAnswerMatchSubquery;
				wstring matchInfoDetail=matchInfoDetailSubject;
				int objectMatch=0,secondaryVerbMatch=0,secondaryObjectMatch=0;
				objectMatch = sriMatch(questionSource, childSource, parentSRI->whereObject, wo, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticObjectTotalMatch,8);
				if (parentSRI->subQuery && questionTypeObject && objectMatch>0)
					whereAnswerMatchSubquery.insert(childSRI->whereObject);

				secondaryVerbMatch=sriVerbMatch(questionSource,childSource,parentSRI->whereSecondaryVerb,childSRI->whereSecondaryVerb,4);
				if (parentSRI->whereSecondaryVerb>=0 && (secondaryVerbMatch==0 || childSRI->whereSecondaryVerb<0))
				{
					if (childSRI->whereSecondaryVerb>=0 || (secondaryVerbMatch=sriVerbMatch(questionSource,childSource,parentSRI->whereSecondaryVerb,childSRI->whereVerb,8))==0)
					{
						secondaryVerbMatch=-verbMatch;
						matchInfoDetail+=L"[SECONDARY_VERB_MATCH_FAILED]";
					}
					else
						verbMatch=0;
				}
				secondaryObjectMatch = sriMatch(questionSource, childSource, parentSRI->whereSecondaryObject, childSRI->whereSecondaryObject, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticSecondaryObjectTotalMatch, 4);
				if (parentSRI->subQuery && questionTypeObject && secondaryObjectMatch>0)
					whereAnswerMatchSubquery.insert(childSRI->whereSecondaryObject);
				if (parentSRI->whereSecondaryObject>=0 && (secondaryObjectMatch==0 || childSRI->whereSecondaryObject<0))
				{
					if (childSRI->whereSecondaryObject >= 0 || (secondaryObjectMatch = sriMatch(questionSource, childSource, parentSRI->whereSecondaryObject, wo, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticSecondaryObjectTotalMatch, 8)) == 0)
					{
						secondaryObjectMatch=-objectMatch;
						matchInfoDetail+=L"[SECONDARY_OBJECT_MATCH_FAILED]";
					}
					else
						objectMatch=0;
				}
				// PARENT: what     [S Darrell Hammond]  featured   on [PO Comedy Central program[12-15][14][nongen][N]]?
				// CHILD: [S Darrell Hammond]  was [O a featured cast member[67-71][70][nongen][N]] on   [PO Saturday Night Live[72-75][72][ngname][N][WikiWork]].
				int cob=-1,coe;
				if (verbMatch<=0 && parentSRI->whereVerb>=0 && childSRI->whereVerb>=0 && childSRI->whereObject>=0 &&
					childSource->m[childSRI->whereVerb].forms.isSet(isForm) && (cob=childSource->m[childSRI->whereObject].beginObjectPosition)>=0 && (coe=childSource->m[childSRI->whereObject].endObjectPosition)>=0 &&
					(coe-cob)>2 && childSource->m[cob].queryForm(determinerForm)>=0 && childSource->m[cob+1].getMainEntry()==questionSource->m[parentSRI->whereVerb].getMainEntry())
				{
					verbMatch=8;
					matchInfoDetail+=L"[MOVED_VERB_TO_OBJECT]";
				}
				set <int> relPreps;
				childSource->getAllPreps(&(*childSRI),relPreps,wo);
				if (relPreps.empty())
					relPreps.insert(-1);	
				for (set <int>::iterator rpi=relPreps.begin(),rpiEnd=relPreps.end(); rpi!=rpiEnd; rpi++)
				{ LFSL
				  wstring matchInfo=matchInfoDetail;
				  int prepObjectMatch=0,prepMatch=0;
					if (*rpi!=-1)
					{
						if (prepMatch=sriPrepMatch(questionSource,childSource,parentSRI->wherePrep,*rpi,2)==2) prepMatch=2;
						prepObjectMatch = sriMatch(questionSource, childSource, parentSRI->wherePrepObject, childSource->m[*rpi].getRelObject(), parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticPrepositionObjectTotalMatch, 4);
						if (parentSRI->subQuery && questionTypePrepObject && prepObjectMatch > 0)
							whereAnswerMatchSubquery.insert(childSource->m[*rpi].getRelObject());
						if (prepObjectMatch<=0 && questionSource->inObject(parentSRI->wherePrepObject,parentSRI->whereQuestionType))
							prepObjectMatch=-8;
					}
					else if (questionSource->inObject(parentSRI->wherePrepObject,parentSRI->whereQuestionType))
						prepObjectMatch=-8;
					int equivalenceClass=0,equivalenceMatch=0;
					if (verbMatch<=0 && (subjectMatch || (questionTypeSubject && !(parentSRI->questionType&QTAFlag))) && (*rpi==-1 || (prepMatch>0 && (prepObjectMatch>0 || questionSource->inObject(parentSRI->wherePrepObject,parentSRI->whereQuestionType)))))
					{
						if (equivalenceClassCheck(questionSource, childSource,childSRI,parentSRI,wo,equivalenceClass,8)==0)
							equivalenceMatch=equivalenceClassCheck2(questionSource,childSource,childSRI,parentSRI,wo,equivalenceClass,8);
					}
					int matchSum=matchSumSubject+verbMatch+objectMatch+secondaryVerbMatch+secondaryObjectMatch+prepMatch+prepObjectMatch+equivalenceMatch;
					if (logQuestionDetail || matchSum>=8+6)
					{
						appendSum(matchSumSubject,L"+SUBJ[",matchInfo);
						appendSum(verbMatch,L"+VERB[",matchInfo);
						appendSum(objectMatch,L"+OBJ[",matchInfo);
						appendSum(secondaryVerbMatch,L"+VERB2[",matchInfo);
						appendSum(secondaryObjectMatch,L"+OBJ2[",matchInfo);
						appendSum(prepMatch,L"+PREP[",matchInfo);
						appendSum(prepObjectMatch,L"+PREPOBJ[",matchInfo);
						appendSum(equivalenceMatch,L"+EM[",matchInfo);
					}
					if (matchSum>8+6 && ((parentSRI->subQuery && subjectMatch) || (subjectMatch && (verbMatch>0 || secondaryVerbMatch>0)))) // a single element match and 3/4 of another (sym match)
					{
						if (parentSRI->subQuery)
						{
							for (int wc: whereAnswerMatchSubquery)
							{
								bool match;
								if (match=checkParticularPartIdentical(questionSource,childSource,parentSRI->whereQuestionType,wc))
								{
										matchSum+=8;
										matchInfo+=L"ANSWER_MATCH[+8]";
								}
								int parentObject,childObject;
								if ((parentObject= questionSource->m[parentSRI->whereQuestionType].getObject())>=0 && (childObject=childSource->m[wc].getObject())>=0)
								{
									if (!childSource->objects[childObject].dbPediaAccessed)
										childSource->identifyISARelation(wc,false);
									bool areBothPlaces=false,wikiTypeMatch=false;
									if (wikiTypeMatch=(questionSource->objects[parentObject].isWikiBusiness && childSource->objects[childObject].isWikiBusiness) ||
											(questionSource->objects[parentObject].isWikiPerson && childSource->objects[childObject].isWikiPerson) ||
											(areBothPlaces=(questionSource->objects[parentObject].isWikiPlace && childSource->objects[childObject].isWikiPlace) ||
											(questionSource->objects[parentObject].isWikiWork && childSource->objects[childObject].isWikiWork)))
									{
											matchSum+=4;
											matchInfo+=L"ANSWER_MATCH_WIKITYPE[+4]";
									}
									if (areBothPlaces && questionSource->objects[parentObject].getSubType()==childSource->objects[childObject].getSubType() &&
										questionSource->objects[parentObject].getSubType()!=UNKNOWN_PLACE_SUBTYPE && questionSource->objects[parentObject].getSubType()>=0)
									{
											matchSum+=4;
											matchInfo+=L"ANSWER_MATCH_OBJECT_SUBTYPE[+4]";
									}
									if (wikiTypeMatch && !match && matchSum>16)
									{
											matchInfo+=L"WIKI_OTHER_ANSWER[0]";
									}
								}
							}
						}
						enterAnswer(questionSource,derivation,childSourceType,childSource,parentSRI,answerSRIs,maxAnswer,rejectDuplicatesFrom,matchSum,matchInfo,childSRI,equivalenceClass,ws,wo,*rpi,answersContainedInSource);
					}
					if (logQuestionDetail)
					{
						wstring ps;
						if (*rpi!=-1)
							childSource->prepPhraseToString(*rpi,ps);
						childSource->printSRI(L"", &(*childSRI),0,ws,wo,ps,false,matchSum,matchInfo);
					}
				}
				if (wo<0 || childSource->m[wo].nextCompoundPartObject<0 || wo>=childSource->m.size()) break;
			}
			if (ws<0 || childSource->m[ws].nextCompoundPartObject<0 || ws>=childSource->m.size()) break;
		}
	}
  wprintf(L"PROGRESS: 100%% child relations processed with %04d seconds elapsed \r",(int)((clock()-startClockTime)/CLOCKS_PER_SEC));
	if (answersContainedInSource==0 && childSource->numSearchedInMemory==1 && eraseIfNoAnswers)
	{
		unordered_map <wstring,Source *>::iterator smi=sourcesMap.find(childSource->sourcePath);
		if (smi!=sourcesMap.end())
		{
			Source *source=smi->second;
			sourcesMap.erase(smi);
			if (source->updateWordUsageCostsDynamically)
				WordClass::resetUsagePatternsAndCosts(source->debugTrace);
			else
				WordClass::resetCapitalizationAndProperNounUsageStatistics(source->debugTrace);
			source->clearSource();
			delete source;
		}
	}
	return answersContainedInSource;
 }

void cQuestionAnswering::analyzeQuestionFromRDFType(Source *questionSource,wchar_t *derivation,int whereQuestionContextSuggestion,cSpaceRelation *parentSRI,cTreeCat *rdfType,bool parseOnly,vector < cAS > &answerSRIs,int &maxAnswer,
		unordered_map <int,WikipediaTableCandidateAnswers *> &wikiTableMap,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion, set <wstring> &wikipediaLinksAlreadyScanned)
{ LFS
	if (rdfType!=NULL)
	{
		int whereQuestionTypeObject=(parentSRI->questionType==unknownQTFlag) ? parentSRI->whereSubject : getWhereQuestionTypeObject(questionSource, parentSRI);
		Source *abstractSource=NULL;
		int check=answerSRIs.size(),qMaxAnswer=-1;
		if (processAbstract(questionSource,rdfType,abstractSource,parseOnly)>=0)
		{
			analyzeQuestionFromSource(questionSource,derivation,L"abstract: "+abstractSource->sourcePath,abstractSource,parentSRI,answerSRIs,qMaxAnswer,check,false,mapPatternAnswer,mapPatternQuestion);
			maxAnswer=max(maxAnswer,qMaxAnswer);
			if (qMaxAnswer>24)
				return;
		}
		int maxPrepositionalPhraseNonMixMatch=-1,dlen=wcslen(derivation);
		questionSource->ppExtensionAvailable(whereQuestionContextSuggestion,maxPrepositionalPhraseNonMixMatch,true);
		int minPrepositionalPhraseNonMixMatch=(maxPrepositionalPhraseNonMixMatch==0) ? 0 : 1;
		for (int I=maxPrepositionalPhraseNonMixMatch; I>=minPrepositionalPhraseNonMixMatch; I--)
		{
			if (I!=0)
				StringCbPrintf(derivation+dlen,1024-dlen,L"P%d:",I);
			// also process wikipedia entry
			Source *wikipediaSource = NULL;
			if (processWikipedia(questionSource, whereQuestionContextSuggestion, wikipediaSource, rdfType->wikipediaLinks, I, parseOnly, wikipediaLinksAlreadyScanned) >= 0)
			{
				analyzeQuestionFromSource(questionSource,derivation, L"wikipedia:" + wikipediaSource->sourcePath, wikipediaSource, parentSRI, answerSRIs, qMaxAnswer, check, false, mapPatternAnswer, mapPatternQuestion);
				if (whereQuestionTypeObject >= 0)
				{
					vector < SourceTable > wikiTables;
					addTables(questionSource, whereQuestionTypeObject, wikipediaSource, wikiTables);
					if (!wikiTables.empty())
						wikiTableMap[whereQuestionContextSuggestion] = new WikipediaTableCandidateAnswers(wikipediaSource, wikiTables);
				}
				maxAnswer = max(maxAnswer, qMaxAnswer);
				if (qMaxAnswer>24)
					return;
			}
			Source *wikipediaLinkSource = NULL;
			if (processWikipedia(questionSource,-1, wikipediaLinkSource, rdfType->wikipediaLinks, I, parseOnly, wikipediaLinksAlreadyScanned) >= 0)
			{
				analyzeQuestionFromSource(questionSource,derivation, L"wikipediaLink:" + wikipediaLinkSource->sourcePath, wikipediaLinkSource, parentSRI, answerSRIs, qMaxAnswer, check, false, mapPatternAnswer, mapPatternQuestion);
				if (whereQuestionTypeObject >= 0)
				{
					vector < SourceTable > wikiTables;
					addTables(questionSource, whereQuestionTypeObject, wikipediaLinkSource, wikiTables);
					if (!wikiTables.empty())
						wikiTableMap[whereQuestionContextSuggestion] = new WikipediaTableCandidateAnswers(wikipediaLinkSource, wikiTables);
				}
				maxAnswer = max(maxAnswer, qMaxAnswer);
				if (qMaxAnswer>24)
					return;
			}
		}
	}
}

bool cQuestionAnswering::checkObjectIdentical(Source *source1,Source *source2,int object1,int object2)
{ LFS
	wstring tmpstr1,tmpstr2;
	//lplog(LOG_WHERE,L"Comparing %s against %s.",
  //  source1->objectString(source1->objects.begin()+object1,tmpstr1,false,false).c_str(),
	//  source2->objectString(source2->objects.begin()+object2,tmpstr2,false,false).c_str());
	int ca=source1->objects[object1].objectClass;
	if (ca==NON_GENDERED_NAME_OBJECT_CLASS || ca==NAME_OBJECT_CLASS)
	{
		if (source1->objects[object1].nameMatchExact(source2->objects[object2]))
		{
			//lplog(LOG_WHERE,L"Comparing %s against %s [checkObjectIdentical=TRUE].",
			//  source1->objectString(source1->objects.begin()+object1,tmpstr1,false,false).c_str(),
			//	source2->objectString(source2->objects.begin()+object2,tmpstr2,false,false).c_str());
			return true;
		}
		return false;
	}
	if (ca==PRONOUN_OBJECT_CLASS || ca==REFLEXIVE_PRONOUN_OBJECT_CLASS || ca==RECIPROCAL_PRONOUN_OBJECT_CLASS)
		return false;
	int ol1=source1->objects[object1].originalLocation,ol2=source2->objects[object2].originalLocation;
	if (ol1<0 || ol2<0) 
		return false;
	int beginPosition1=source1->m[ol1].beginObjectPosition,endPosition1=source1->m[ol1].endObjectPosition;
	int beginPosition2=source2->m[ol2].beginObjectPosition,endPosition2=source2->m[ol2].endObjectPosition;
	if (beginPosition1<0 || endPosition1<0 || beginPosition2<0 || endPosition2<0)
		return false;
	if ((endPosition1-beginPosition1)!=(endPosition2-beginPosition2))
		return false;
	/*
	GENDERED_GENERAL_OBJECT_CLASS=5,
	BODY_OBJECT_CLASS=6,
	GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS=7, // occupation=plumber;role=leader;activity=runner
	GENDERED_DEMONYM_OBJECT_CLASS=8,
	NON_GENDERED_GENERAL_OBJECT_CLASS=9,
	NON_GENDERED_BUSINESS_OBJECT_CLASS=10,
	PLEONASTIC_OBJECT_CLASS=11,
	VERB_OBJECT_CLASS=12, // the news I brought / running -- objects that contain verbs
	META_GROUP_OBJECT_CLASS=14,
	GENDERED_RELATIVE_OBJECT_CLASS=15
	*/
	for (int I=0; I<endPosition1-beginPosition1; I++)
		if (source1->m[beginPosition1+I].word!=source2->m[beginPosition2+I].word)
			return false;
	//lplog(LOG_WHERE,L"Comparing %s against %s [checkObjectIdentical=TRUE].",
  //  source1->objectString(source1->objects.begin()+object1,tmpstr1,false,false).c_str(),
	//  source2->objectString(source2->objects.begin()+object2,tmpstr2,false,false).c_str());
	return true;
}

bool cQuestionAnswering::checkParticularPartIdentical(Source *source1,Source *source2,int where1,int where2)
{ LFS
	if (where1<0 || where2<0) return false;
	if (source1->m[where1].getObject()<0 && source2->m[where2].getObject()<0)
	{
		lplog(LOG_WHERE,L"cas1:%d:%s cas2:%d:%s",where1,source1->m[where1].word->first.c_str(),where2,source2->m[where2].word->first.c_str());
		return source1->m[where1].word==source2->m[where2].word;
	}
	if (source1->m[where1].getObject()<0 || source2->m[where2].getObject()<0)
		return false;
	if (source1->m[where1].objectMatches.size()==0)
	{
		if (source2->m[where2].objectMatches.size() == 0)
			return checkObjectIdentical(source1, source2, source1->m[where1].getObject(), source2->m[where2].getObject());
		for (int om=0; om<source2->m[where2].objectMatches.size(); om++)
			if (checkObjectIdentical(source1,source2,source1->m[where1].getObject(),source2->m[where2].objectMatches[om].object))
				return true;
		return false;
	}
	if (source2->m[where2].objectMatches.size()==0)
	{
		for (int om=0; om<source1->m[where1].objectMatches.size(); om++)
			if (checkObjectIdentical(source1,source2,source1->m[where1].objectMatches[om].object,source2->m[where2].getObject()))
				return true;
		return false;
	}
	for (unsigned int cai=0; cai<source1->m[where1].objectMatches.size(); cai++)
		for (unsigned int cai2=0; cai2<source2->m[where2].objectMatches.size(); cai2++)
		  if (checkObjectIdentical(source1,source2,source1->m[where1].objectMatches[cai].object,source2->m[where2].objectMatches[cai2].object))
			  return true;
	return false;
}

int Source::determineKindBitFieldFromObject(Source *source,int object,int &wikiBitField)
{ LFS
	if (source->objects[object].isWikiBusiness) wikiBitField|=1;
	if (source->objects[object].isWikiPerson) wikiBitField|=2;
	if (source->objects[object].isWikiPlace) wikiBitField|=4;
	if (source->objects[object].isWikiWork) wikiBitField|=8;
	return 0;
}

int Source::determineKindBitField(Source *source,int where,int &wikiBitField)
{ LFS
	if (source->m[where].getObject()<0 && source->m[where].objectMatches.size()==0)
	{
		if (matchChildSourcePositionSynonym(Words.query(L"business"),source,where) || source->m[where].getMainEntry()->first==L"business")
			return wikiBitField|=1;
		if (matchChildSourcePositionSynonym(Words.query(L"person"),source,where) || source->m[where].getMainEntry()->first==L"person")
			return wikiBitField|=2;
		if (matchChildSourcePositionSynonym(Words.query(L"place"),source,where) || source->m[where].getMainEntry()->first==L"place")
			return wikiBitField|=4;
		return -1;
	}
	if (source->m[where].objectMatches.size()>=1)
	{
		for (unsigned int om=0; om<source->m[where].objectMatches.size(); om++)
			determineKindBitFieldFromObject(source,source->m[where].objectMatches[om].object,wikiBitField);
	}
	else
	{
		determineKindBitFieldFromObject(source,source->m[where].getObject(),wikiBitField);
		if (source->objects[source->m[where].getObject()].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS && 
				(source->m[where].endObjectPosition-source->m[where].beginObjectPosition==1 ||
				(source->m[where].endObjectPosition-source->m[where].beginObjectPosition==2 && source->m[source->m[where].beginObjectPosition].queryForm(determinerForm)!=-1)))
		{
			if (matchChildSourcePositionSynonym(Words.query(L"business"),source,where) || source->m[where].getMainEntry()->first==L"business")
				return wikiBitField|=1;
			if (matchChildSourcePositionSynonym(Words.query(L"person"),source,where) || source->m[where].getMainEntry()->first==L"person")
				return wikiBitField|=2;
			if (matchChildSourcePositionSynonym(Words.query(L"place"),source,where) || source->m[where].getMainEntry()->first==L"place")
				return wikiBitField|=4;
		}
	}
	return 0;
}

int Source::checkParticularPartQuestionTypeCheck(__int64 questionType,int childWhere,int childObject,int &semanticMismatch)
{ LFS
	wstring tmpstr;
	int oc=objects[childObject].objectClass;
	if (oc==PRONOUN_OBJECT_CLASS ||
			oc==REFLEXIVE_PRONOUN_OBJECT_CLASS ||
			oc==RECIPROCAL_PRONOUN_OBJECT_CLASS ||
			oc==PLEONASTIC_OBJECT_CLASS ||
			oc==META_GROUP_OBJECT_CLASS ||
			oc==VERB_OBJECT_CLASS)
	{
		semanticMismatch=1;
		return CONFIDENCE_NOMATCH;
	}
	if ((questionType==cQuestionAnswering::whereQTFlag || questionType== cQuestionAnswering::whoseQTFlag || questionType== cQuestionAnswering::whomQTFlag || questionType== cQuestionAnswering::wikiBusinessQTFlag || questionType== cQuestionAnswering::wikiWorkQTFlag) &&
		!objects[childObject].dbPediaAccessed)
		identifyISARelation(childWhere,true);
	bool wikiDetermined=
		(objects[childObject].isWikiPlace || objects[childObject].isWikiPerson ||	objects[childObject].isWikiBusiness ||	objects[childObject].isWikiWork);
	switch (questionType)
	{
		case cQuestionAnswering::whereQTFlag:
			if (oc==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
				  oc==GENDERED_DEMONYM_OBJECT_CLASS ||
					oc==GENDERED_RELATIVE_OBJECT_CLASS || 
					oc==BODY_OBJECT_CLASS ||
					oc==NON_GENDERED_BUSINESS_OBJECT_CLASS)
			{
				semanticMismatch=2;
				return CONFIDENCE_NOMATCH;
			}
			if (oc==GENDERED_GENERAL_OBJECT_CLASS && wikiDetermined)
			{
				semanticMismatch=3;
				return CONFIDENCE_NOMATCH;
			}
			if (oc==NON_GENDERED_GENERAL_OBJECT_CLASS ||
				  oc==NON_GENDERED_NAME_OBJECT_CLASS ||
					oc==NAME_OBJECT_CLASS)
			{
				if (objects[childObject].isLocationObject ||
					objects[childObject].isWikiPlace)
					return 1;
				if (objects[childObject].getSubType() == NOT_A_PLACE ||
				     objects[childObject].isTimeObject ||
				     objects[childObject].isWikiPerson ||
						 objects[childObject].isWikiWork ||
				     objects[childObject].isWikiBusiness)
				{
					semanticMismatch=4;
					return CONFIDENCE_NOMATCH;
				}
				// if object of [location preposition]
				if (m[childWhere].relPrep >= 0)
				{
					wstring prep = m[m[childWhere].relPrep].word->first;
					if (prepTypesMap[prep] == tprNEAR || prepTypesMap[prep] == tprIN || prepTypesMap[prep] == tprSPAT)
						return 1;
				}
			}
			break;
		case cQuestionAnswering::whoseQTFlag:
		case cQuestionAnswering::whomQTFlag:
			if (oc==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
				  oc==GENDERED_DEMONYM_OBJECT_CLASS ||
					oc==GENDERED_GENERAL_OBJECT_CLASS ||
					oc==GENDERED_RELATIVE_OBJECT_CLASS || 
					oc==BODY_OBJECT_CLASS)
				return 1;
			if ((oc==NON_GENDERED_GENERAL_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS) && wikiDetermined && !objects[childObject].isWikiPerson)
			{
				semanticMismatch=5;
				return CONFIDENCE_NOMATCH;
			}
			if (oc==NON_GENDERED_BUSINESS_OBJECT_CLASS && wikiDetermined && !objects[childObject].isWikiPerson)
			{
				semanticMismatch=6;
				return CONFIDENCE_NOMATCH;
			}
			if (oc == NAME_OBJECT_CLASS || oc == NON_GENDERED_NAME_OBJECT_CLASS)
			{
				if (objects[childObject].getSubType()!=NOT_A_PLACE && wikiDetermined && !objects[childObject].isWikiPerson)
				{
					semanticMismatch=7;
					return CONFIDENCE_NOMATCH;
				}
				if (wikiDetermined && !objects[childObject].isWikiPerson && (objects[childObject].isTimeObject || objects[childObject].isLocationObject || objects[childObject].isWikiPlace || objects[childObject].isWikiBusiness))
				{
					semanticMismatch=8;
					return CONFIDENCE_NOMATCH;
				}
				if (objects[childObject].isWikiPerson)
					return 1;
				if (oc == NAME_OBJECT_CLASS)
					return CONFIDENCE_NOMATCH / 2;
			}
			return CONFIDENCE_NOMATCH;
		case cQuestionAnswering::whenQTFlag:
			if (!objects[childObject].isTimeObject)
			{
				semanticMismatch=9;
				return CONFIDENCE_NOMATCH;
			}
			else
				return 1;
		case cQuestionAnswering::wikiBusinessQTFlag:
			if (oc == NON_GENDERED_BUSINESS_OBJECT_CLASS || objects[childObject].isWikiBusiness)
				return 1;
			break;
		case cQuestionAnswering::wikiWorkQTFlag:
			if (objects[childObject].isWikiWork)
				return 1;
			break;
	}
	return CONFIDENCE_NOMATCH/2;
}

void Source::checkParticularPartSemanticMatchWord(int logType,int parentWhere,bool &synonym,set <wstring> &parentSynonyms,wstring pw,wstring pwme,int &lowestConfidence,unordered_map <wstring ,int >::iterator ami)
{ LFS // DLFS
	//if (logQuestionDetail)
	//	lplog(logType,L"checkParticularPartSemanticMatchWord child=%s",ami->first.c_str());	
	bool rememberSynonym=false;
	if (ami->first==pw || ami->first==pwme || (rememberSynonym=parentSynonyms.find(ami->first)!=parentSynonyms.end()))
	{
		if (lowestConfidence>ami->second)
		{
			lowestConfidence=(ami->second);
			synonym=rememberSynonym;
			if (logQuestionDetail)
			{
				wstring tmpstr;
				lplog(logType,L"object %s:(primary match:%s[%s]) MATCH %s%s",whereString(parentWhere,tmpstr,false).c_str(),pw.c_str(),pwme.c_str(),ami->first.c_str(),(synonym) ? L" SYNONYM":L"");			
			}
		}
	}
}

// parent=US government officials / child=President George W . Bush
int cQuestionAnswering::checkParentGroup(Source *parentSource,int parentWhere,Source *childSource,int childWhere,int childObject,bool &synonym,int &semanticMismatch)
{ LFS
	if (parentSource->m[parentWhere].queryForm(commonProfessionForm)<0 && parentSource->m[parentWhere].getMainEntry()->second.query(commonProfessionForm)<0)
		return CONFIDENCE_NOMATCH;
  unordered_map<wstring,SemanticMatchInfo>::iterator csmpi;
	wstring childObjectString;
  if ((csmpi=questionGroupMap.find(childSource->whereString(childSource->objects[childObject].originalLocation,childObjectString,true)))!=questionGroupMap.end())
	{
		synonym=csmpi->second.synonym;
		semanticMismatch=csmpi->second.semanticMismatch;
		return csmpi->second.confidence;
	}
	// Does child belong to a group?
	// get rdfTypes
	vector <cTreeCat *> rdfTypes;
	unordered_map <wstring ,int > topHierarchyClassIndexes;
	childSource->getExtendedRDFTypesMaster(childSource->objects[childObject].originalLocation,-1,rdfTypes,topHierarchyClassIndexes,TEXT(__FUNCTION__));
	wstring tmpstr,tmpstr2,pw=parentSource->m[parentWhere].word->first,tmpstr3;
	transform (pw.begin (), pw.end (), pw.begin (), (int(*)(int)) tolower);
	wstring pwme=parentSource->m[parentWhere].getMainEntry()->first;
	set <wstring> parentSynonyms;
	parentSource->getSynonyms(pw,parentSynonyms, NOUN);
	parentSource->getSynonyms(pwme,parentSynonyms, NOUN);
	//logQuestionDetail= (tmpstr2.find(L"George W Bush")!=wstring::npos);
	if (true)
	{
		unordered_map <wstring ,int > associationMap;
		childSource->getAssociationMapMaster(childSource->objects[childObject].originalLocation,-1,associationMap,TEXT(__FUNCTION__));
		wstring associations;
		for (unordered_map <wstring ,int >::iterator ami=associationMap.begin(),amiEnd=associationMap.end(); ami!=amiEnd; ami++)
			associations+=ami->first+L" ";
		lplog(LOG_WHERE,L"checkParentGroup ld parent:%s[%s]\n  child:%s\n  parentSynonyms:%s\n  childRdfTypeAssociations %s",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
			childSource->whereString(childWhere, tmpstr2, false).c_str(), setString(parentSynonyms, tmpstr3, L"|").c_str(), associations.c_str());
	}
  int confidenceMatch=CONFIDENCE_NOMATCH;
	// first professions
	for (unsigned int r=0; r<rdfTypes.size(); r++)
	{
		if (logQuestionDetail)
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch))
				rdfTypes[r]->logIdentity(LOG_WHERE,L"checkParentGroup",false);
		for (unsigned int p=0; p<rdfTypes[r]->professionLinks.size(); p++)
		{
			// politician,businessman, etc
			vector < vector <string> > kindOfObjects;
			getAllOrderedHyperNyms(rdfTypes[r]->professionLinks[p],kindOfObjects);
			for (vector < vector <string> >::iterator oi=kindOfObjects.begin(),oiEnd=kindOfObjects.end(); oi!=oiEnd; oi++)
			{
				bool firstNewLevel=false;
				for (vector <string>::iterator soi=oi->begin(),soiEnd=oi->end(); soi!=soiEnd; soi++)
				{
					if (firstNewLevel)
					{
						bool levelContainsPerson=false;
						for (vector <string>::iterator soiLevel=soi; soiLevel!=soiEnd && !soi->empty() && !levelContainsPerson; soiLevel++)
							levelContainsPerson=(*soiLevel=="person");
						if (levelContainsPerson)
							break;
					}
					if (firstNewLevel=soi->empty())
						continue;
					wstring wsoi;
					mTW(*soi,wsoi);
					if (parentSynonyms.find(wsoi)!=parentSynonyms.end())
					{
						synonym=true;
						lplog(LOG_WHERE,L"checkParentGroup 1 parent:%s[%s] child:%s parentSynonyms:%s %s",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
							childSource->whereString(childWhere, tmpstr2, false).c_str(), setString(parentSynonyms, tmpstr3, L"|").c_str(), wsoi.c_str());
						tmpstr.clear();
						tmpstr2.clear();
						tmpstr3.clear();
						confidenceMatch=min(confidenceMatch,CONFIDENCE_NOMATCH/2); // only synonym
					}
					if (pw==wsoi || pwme==wsoi)
					{
						lplog(LOG_WHERE,L"checkParentGroup 2 parent:%s[%s] child:%s",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
							childSource->whereString(childWhere,tmpstr2,false).c_str());
						tmpstr.clear();
						tmpstr2.clear();
						confidenceMatch=min(confidenceMatch,CONFIDENCE_NOMATCH/4); 
					}
					set <wstring> professionSynonyms;
					parentSource->getSynonyms(wsoi,professionSynonyms, NOUN);
					if (logQuestionDetail)
						lplog(LOG_WHERE,L"checkParentGroup ld parent:%s[%s] child:%s profession:%s[%s] professionSynonyms:%s",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
						childSource->whereString(childWhere, tmpstr2, false).c_str(), rdfTypes[r]->professionLinks[p].c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
					if (professionSynonyms.find(pw)!=professionSynonyms.end() || professionSynonyms.find(pwme)!=professionSynonyms.end())
					{
						synonym=true;						
						lplog(LOG_WHERE,L"checkParentGroup 3 parent:%s[%s] child:%s profession:%s[%s] professionSynonyms:%s",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
							childSource->whereString(childWhere, tmpstr2, false).c_str(), rdfTypes[r]->professionLinks[p].c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
						confidenceMatch=min(confidenceMatch,CONFIDENCE_NOMATCH/2); 
						tmpstr.clear();
						tmpstr2.clear();
						tmpstr3.clear();
					}
				}
			}
		}
		if (!Ontology::cacheRdfTypes)
		  delete (rdfTypes[r]); // only delete if not caching them
	}
	// honorific?
	if (confidenceMatch>=CONFIDENCE_NOMATCH && childSource->objects[childObject].name.hon!=wNULL)
	{
		vector < vector <string> > kindOfObjects;
		if (pw==childSource->objects[childObject].name.hon->first || pwme==childSource->objects[childObject].name.hon->first)
		{
			lplog(LOG_WHERE,L"checkParentGroup 4 parent:%s[%s] child:%s profession:%s [honorific original]",pw.c_str(),parentSource->whereString(parentWhere,tmpstr,false).c_str(),
				childSource->whereString(childWhere,tmpstr2,false).c_str(),childSource->objects[childObject].name.hon->first.c_str());
			confidenceMatch=CONFIDENCE_NOMATCH/4;
		}
		if (confidenceMatch==CONFIDENCE_NOMATCH)
			getAllOrderedHyperNyms(childSource->objects[childObject].name.hon->first,kindOfObjects);
		for (vector < vector <string> >::iterator oi=kindOfObjects.begin(),oiEnd=kindOfObjects.end(); oi!=oiEnd && confidenceMatch==CONFIDENCE_NOMATCH; oi++)
		{
			bool firstNewLevel=false;
			for (vector <string>::iterator soi=oi->begin(),soiEnd=oi->end(); soi!=soiEnd && confidenceMatch==CONFIDENCE_NOMATCH; soi++)
			{
				if (firstNewLevel)
				{
					bool levelContainsPerson=false;
					for (vector <string>::iterator soiLevel=soi; soiLevel!=soiEnd && !soi->empty() && !levelContainsPerson; soiLevel++)
						levelContainsPerson=(*soiLevel=="person");
					if (levelContainsPerson)
						break;
				}
				if (firstNewLevel=soi->empty())
					continue;
				wstring wsoi;
				mTW(*soi,wsoi);
				if (parentSynonyms.find(wsoi)!=parentSynonyms.end())
				{
					lplog(LOG_WHERE,L"checkParentGroup 5 parent:%s[%s] child:%s parentSynonyms:%s %s [honorific]",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
						childSource->whereString(childWhere, tmpstr2, false).c_str(), setString(parentSynonyms, tmpstr3, L"|").c_str(), wsoi.c_str());
					synonym=true;					
					confidenceMatch=CONFIDENCE_NOMATCH/2;
					break;
				}
				if (pw==wsoi || pwme==wsoi)
				{
					lplog(LOG_WHERE,L"checkParentGroup 6 parent:%s[%s] child:%s profession:%s [honorific]",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
						childSource->whereString(childWhere,tmpstr2,false).c_str(),wsoi.c_str());
					confidenceMatch=CONFIDENCE_NOMATCH/4;
					break;
				}
				set <wstring> professionSynonyms;
				parentSource->getSynonyms(wsoi,professionSynonyms, NOUN);
				if (logQuestionDetail)
					lplog(LOG_WHERE,L"checkParentGroup ld parent:%s[%s] child:%s profession:%s professionSynonyms:%s",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
					childSource->whereString(childWhere, tmpstr2, false).c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
				if (professionSynonyms.find(pw)!=professionSynonyms.end() || professionSynonyms.find(pwme)!=professionSynonyms.end())
				{
					lplog(LOG_WHERE,L"checkParentGroup 7 parent:%s[%s] child:%s profession:%s professionSynonyms:%s [honorific]",pw.c_str(), parentSource->whereString(parentWhere,tmpstr,false).c_str(),
						childSource->whereString(childWhere, tmpstr2, false).c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
					synonym=true;
					confidenceMatch=CONFIDENCE_NOMATCH/2;
					break;
				}
			}
		}
	}
  questionGroupMap[childObjectString]=SemanticMatchInfo(synonym,semanticMismatch,confidenceMatch);
	return confidenceMatch;
}

// may be passed a childObject which is -1, in which case it will try to derive it from childWhere.  This is only recommended if the resolution of the object location is simple (no multiple matches).
int Source::checkParticularPartSemanticMatch(int logType,int parentWhere,Source *childSource,int childWhere,int childObject,bool &synonym,int &semanticMismatch)
{ LFS
	if (childWhere<0)
		return CONFIDENCE_NOMATCH;
	if (childObject<0)
		childObject=(childSource->m[childWhere].objectMatches.size()>0) ? childSource->m[childWhere].objectMatches[0].object : childSource->m[childWhere].getObject();
	if (childObject<0)
		return CONFIDENCE_NOMATCH;

	unordered_map <wstring ,int > associationMap;
	childSource->getAssociationMapMaster(childSource->objects[childObject].originalLocation,-1,associationMap,TEXT(__FUNCTION__));
	wstring tmpstr,tmpstr2,pw=m[parentWhere].word->first;
	transform (pw.begin (), pw.end (), pw.begin (), (int(*)(int)) tolower);
	wstring pwme=m[parentWhere].getMainEntry()->first;
	set <wstring> parentSynonyms;
	getSynonyms(pw,parentSynonyms, NOUN);
	getSynonyms(pwme,parentSynonyms, NOUN);
	int lowestConfidence=CONFIDENCE_NOMATCH;
	setString(parentSynonyms, tmpstr2, L"|");
	tmpstr.clear();
	for (unordered_map <wstring ,int >::iterator ami=associationMap.begin(),amiEnd=associationMap.end(); ami!=amiEnd; ami++)
		tmpstr+=ami->first+L"|";
	wstring tmp1, tmp2, tmp3;
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"Comparing [%d, %d] %s and %s(%s)\nassociationMap for %s: %s\nparentSynonyms for %s: %s.",
		parentWhere, childWhere, whereString(parentWhere, tmp1, false).c_str(), childSource->whereString(childWhere, tmp2, false).c_str(), childSource->whereString(childSource->objects[childObject].originalLocation, tmp3, false).c_str(),
		tmp2.c_str(), tmpstr.c_str(), pw.c_str(), tmpstr2.c_str());
	for (unordered_map <wstring, int >::iterator ami = associationMap.begin(), amiEnd = associationMap.end(); ami != amiEnd && lowestConfidence > 1; ami++)
		checkParticularPartSemanticMatchWord(logType, parentWhere, synonym, parentSynonyms, pw, pwme, lowestConfidence, ami);
	//if (childWhere==886)
	//{
	//	logQuestionDetail=0;
	//	if (saveConfidence>lowestConfidence)
	//		lplog(LOG_WHERE,L"Comparing [%d, %d] %s and %s(%s)\nassociationMap for %s: %s\nparentSynonyms for %s: %s.",
	//				parentWhere,childWhere,whereString(parentWhere,tmp1,false).c_str(),childSource->whereString(childWhere,tmp2,false).c_str(),childSource->whereString(childSource->objects[childObject].originalLocation,tmp3,false).c_str(),
	//				tmp2.c_str(),tmpstr.c_str(),pw.c_str(),tmpstr2.c_str());
	//}
	if (lowestConfidence==CONFIDENCE_NOMATCH)
	{
		int lastChildWhere=childSource->objects[childObject].originalLocation;
		if (childSource->objects[childObject].objectClass==NAME_OBJECT_CLASS || childSource->objects[childObject].objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
			lastChildWhere=childSource->m[childSource->objects[childObject].originalLocation].endObjectPosition-1;
		wstring cw=childSource->m[lastChildWhere].word->first;
		transform (cw.begin (), cw.end (), cw.begin (), (int(*)(int)) tolower);
		wstring cwme=childSource->m[lastChildWhere].getMainEntry()->first;
		if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
			lplog(logType,L"checkParticularPartSemanticMatch child alternate=%s[%s]",cw.c_str(),cwme.c_str());			
		if (pw==cw || pwme==cwme)
		{
			if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"parent word %s:(primary match:%s[%s]) MATCH %s[%s]", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), cw.c_str(), cwme.c_str());
			lowestConfidence=CONFIDENCE_NOMATCH/4;
		}
		if (synonym=lowestConfidence==CONFIDENCE_NOMATCH && (parentSynonyms.find(cw)!=parentSynonyms.end() || parentSynonyms.find(cwme)!=parentSynonyms.end()))
		{
			if (logSynonymDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"word %s MATCH %s[%s] SYNONYM", whereString(parentWhere, tmpstr, false).c_str(), cw.c_str(), cwme.c_str());
			lowestConfidence=CONFIDENCE_NOMATCH/2;
		}
	}
	if (lowestConfidence==CONFIDENCE_NOMATCH)
	{
		int parentWikiBitField=0,childWikiBitField=0;
		determineKindBitField(this,parentWhere,parentWikiBitField);
		determineKindBitField(childSource,childWhere,childWikiBitField);
		if (parentWikiBitField && childWikiBitField && !(parentWikiBitField&childWikiBitField))
		{
			semanticMismatch=10;
			if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"object parent [%s]:(primary match:%s[%s]) child [%s] BitField mismatch", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), childSource->objectString(childObject, tmpstr2, false).c_str(), parentWikiBitField, childWikiBitField);
		}
		// profession
		wchar_t *professionLimitedSynonyms[]= { L"avocation", L"calling", L"career", L"employment", L"occupation", L"vocation", L"job", L"livelihood", L"profession", L"work", NULL };
		bool parentIsProfession=false;
		for (int I=0; professionLimitedSynonyms[I] && !parentIsProfession; I++)
			parentIsProfession|=m[parentWhere].word->first==professionLimitedSynonyms[I];
		if (parentIsProfession && childSource->m[childWhere].queryForm(commonProfessionForm)<0)
		{
			semanticMismatch=11;
			if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"object parent [%s]:(primary match:%s[%s]) child [%s] profession mismatch", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), childSource->objectString(childObject, tmpstr2, false).c_str(), parentWikiBitField, childWikiBitField);
		}
		if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
			lplog(logType, L"object parent [%s]:(primary match:%s[%s]) child [%s] NO MATCH", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), childSource->objectString(childObject, tmpstr2, false).c_str(), parentWikiBitField, childWikiBitField);
	}
	else if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
		lplog(logType,L"object parent [%s]:(primary match:%s[%s]) child [%s] lowest confidence %d",whereString(parentWhere,tmpstr,false).c_str(),pw.c_str(),pwme.c_str(),childSource->objectString(childObject,tmpstr2,false).c_str(),lowestConfidence);			
	return lowestConfidence;
}

// transformSource is the transformQuestion source.
// originalQuestionSRI is the source sri that is mapped to the source (this).
// constantQuestionSRI is the transformation sri mapped to the constant question in transformSource.
// originalQuestionPattern is the pattern matched to the source (this).
// constantQuestionPattern is a pattern matched to transformSource.
// sourceMap maps each location in the original source to the source (this).
//
// copy all of transformedSRI which is stored in the transformSource to Source (this).
// patterns has three maps:
//         map < variable name, int where > variableToLocationMap 
//         map < variable name, int length > variableToLengthMap 
//         map < int where, variable name > locationToVariableMap;
// 
void cQuestionAnswering::copySource(Source *toSource,cSpaceRelation *constantQuestionSRI,cPattern *originalQuestionPattern,cPattern *constantQuestionPattern, unordered_map <int,int> &sourceMap, unordered_map <wstring, wstring> &parseVariables)
{ LFS
	sourceMap[-1]=-1;
	for (int I=constantQuestionSRI->printMin; I<constantQuestionSRI->printMax+1 && I<(signed)transformSource->m.size(); I++)
	{
		unordered_map < int, wstring >::iterator ivMap=constantQuestionPattern->locationToVariableMap.find(I);
		sourceMap[I]=toSource->m.size();
		wstring temp;
		if (ivMap==constantQuestionPattern->locationToVariableMap.end())
		{
			lplog(LOG_WHERE,L"[%s] location %d (mapped to %d) not found in pattern %d for variable.",transformSource->getOriginalWord(I,temp,false,false),I,toSource->m.size(),originalQuestionPattern->num);
			toSource->copyChildIntoParent(transformSource,I);
		}
		else
		{
			// look up variable (ivmap->second) within originalQuestionPattern
			unordered_map < wstring, int >::iterator ilMap=originalQuestionPattern->variableToLocationMap.find(ivMap->second);
			unordered_map < wstring, int >::iterator ilenMap=originalQuestionPattern->variableToLengthMap.find(ivMap->second);
			if (ilMap==originalQuestionPattern->variableToLocationMap.end() || ilenMap==originalQuestionPattern->variableToLengthMap.end())
			{
				wstring tmpstr;
				lplog(LOG_WHERE|LOG_ERROR,L"variable %s not found in question transformation, pushing word [%s (elements=%s) (suggestedElements=%s)].",
					ivMap->second.c_str(),transformSource->getOriginalWord(I,temp,false,false),transformSource->m[I].patternWinnerFormString(tmpstr).c_str(),parseVariables[ivMap->second].c_str());
				toSource->m.push_back(transformSource->m[I]);
				// optional lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms form
				toSource->m[toSource->m.size()-1].questionTransformationSuggestedPattern=parseVariables[ivMap->second];
				continue;
			}
			int where=ilMap->second,length=ilenMap->second;
			for (int w=where; w<where+length; w++)
			{
				::lplog(LOG_WHERE,L"[%s] location %d (mapped to %d) used variable %s", toSource->getOriginalWord(w,temp,false,false),w, toSource->m.size(),ivMap->second.c_str());
				toSource->m.push_back(toSource->m[w]);
				if (w==where+length-1 && (toSource->m[toSource->m.size()-1].flags&WordMatch::flagNounOwner))
				{
					toSource->m[toSource->m.size()-1].flags&=~(WordMatch::flagNounOwner|WordMatch::flagAdjectivalObject);
					toSource->getOriginalWord(toSource->m.size()-1,temp,false,false);
					lplog(LOG_WHERE|LOG_ERROR,L"word transformed %s",temp.c_str());
				}
				toSource->adjustOffsets(w,true);
			}
		}
	}
}

void Source::copySource(Source *childSource,int begin,int end)
{ LFS
	for (int I=begin; I<end; I++)
	{
		m.push_back(childSource->m[I]);
		adjustOffsets(I);
	}
}

int Source::copyDirectlyAttachedPrepositionalPhrase(Source *childSource,int relPrep)
{ LFS
	int relObject=childSource->m[relPrep].getRelObject();
	if (relObject>=relPrep && childSource->m[relObject].endObjectPosition>=0)
	{
		copySource(childSource,relPrep,childSource->m[relObject].endObjectPosition);
		return childSource->m[relObject].endObjectPosition-relPrep;
	}
	return 0;
}

bool Source::isObjectCapitalized(int where)
{ LFS
	if (where<0)
		return false;
	int lastWord=m[where].endObjectPosition-1;
	return (lastWord>=0 && (m[lastWord].queryForm(PROPER_NOUN_FORM_NUM)>=0 || (m[lastWord].flags&WordMatch::flagFirstLetterCapitalized) || (m[lastWord].flags&WordMatch::flagAllCaps)));
}

// if getUntilNumPP==-1, this gets the maximum number of available directly attached non-mixed case prepositional phrases
bool Source::ppExtensionAvailable(int where,int &getUntilNumPP,bool nonMixed)
{ LFS
	if (getUntilNumPP==0) return true;
	int relPrep=m[where].endObjectPosition;
	if (where>=0 && where<(signed)m.size() && relPrep>=0 && relPrep<(signed)m.size() && m[relPrep].queryWinnerForm(prepositionForm)>=0)
	{
		if (m[relPrep].nextQuote==-1)
			m[relPrep].nextQuote=where;
		if (m[relPrep].nextQuote==where || m[relPrep].relNextObject==where)
		{
			bool isCapitalized=isObjectCapitalized(where);
			int numPP;
			for (numPP=0; relPrep>=0 && m[relPrep].getRelObject()>=0 && m[relPrep].getRelObject()<(signed)m.size() && (m[relPrep].nextQuote==where || m[relPrep].relNextObject==where) && (numPP<getUntilNumPP || getUntilNumPP==-1); numPP++)
			{
				if ((getUntilNumPP==-1 || numPP<getUntilNumPP) && nonMixed && (isObjectCapitalized(m[relPrep].getRelObject()) ^ isCapitalized))
				{
					if (getUntilNumPP==-1)
						getUntilNumPP=numPP;
					return false;
				}
				relPrep=m[relPrep].relPrep;
			}
			if (getUntilNumPP==-1)
				getUntilNumPP=numPP;
			return numPP>=getUntilNumPP;
		}
	}
	if (getUntilNumPP==-1)
		getUntilNumPP=0;
	return false;
}

int Source::copyDirectlyAttachedPrepositionalPhrases(int whereParentObject,Source *childSource,int whereChild)
{ LFS
	int relPrep=childSource->m[whereChild].endObjectPosition;
	if (relPrep<0 || relPrep>=(signed)childSource->m.size() || childSource->m[relPrep].queryWinnerForm(prepositionForm)<0 || childSource->m[relPrep].nextQuote!=whereChild) return 0;
	m[whereParentObject].relPrep=m.size();
	while (relPrep>=0 && (childSource->m[relPrep].nextQuote==whereChild || childSource->m[relPrep].relNextObject==whereChild))
	{
		copyDirectlyAttachedPrepositionalPhrase(childSource,relPrep);
		relPrep=childSource->m[relPrep].relPrep;
	}
	return m.size()-m[whereParentObject].relPrep;
}

void Source::adjustOffsets(int childWhere,bool keepObjects)
{ LFS
	int parentWhere=m.size()-1;
	int offset=parentWhere-childWhere;
	if (m[parentWhere].beginObjectPosition>=0)
		m[parentWhere].beginObjectPosition+=offset;
	if (m[parentWhere].endObjectPosition>=0)
		m[parentWhere].endObjectPosition+=offset;
	if (m[parentWhere].relPrep>=0)
		m[parentWhere].relPrep+=offset;
	if (m[parentWhere].getRelObject()>=0)
		m[parentWhere].setRelObject(m[parentWhere].getRelObject()+offset);
	if (m[parentWhere].nextQuote>=0)
		m[parentWhere].nextQuote+=offset;
	if (m[parentWhere].principalWherePosition>=0)
		m[parentWhere].principalWherePosition+=offset;
	m[parentWhere].beginPEMAPosition=-1;
	m[parentWhere].endPEMAPosition=-1;
	if (!keepObjects)
	{
		m[parentWhere].setObject(-1);
		m[parentWhere].objectMatches.clear();
	}
	if (logQuestionDetail)
		lplog(LOG_WHERE,L"parentWhere %d:COPY CHILD->PARENT %s beginObjectPosition=%d endObjectPosition=%d relPrep=%d relObject=%d nextQuote=%d offset=%d",
			parentWhere,m[parentWhere].word->first.c_str(),m[parentWhere].beginObjectPosition,m[parentWhere].endObjectPosition,m[parentWhere].relPrep,m[parentWhere].getRelObject(),m[parentWhere].nextQuote,offset);
}

// copy the answer into the parent
int Source::copyChildIntoParent(Source *childSource,int whereChild)
{ LFS
	if (childSource->m[whereChild].getObject()<0 || childSource->m[whereChild].beginObjectPosition<0 || childSource->m[whereChild].endObjectPosition<0)
	{
		m.push_back(childSource->m[whereChild]);
		adjustOffsets(whereChild);
		return m.size()-1;
	}
	else
	{
		int copyChildObject=-1,whereObject=-1;
		if (childSource->m[whereChild].objectMatches.size()>0)
		{
			copyChildObject=childSource->m[whereChild].objectMatches[0].object;
			whereObject=childSource->objects[copyChildObject].originalLocation;
			copySource(childSource,childSource->m[whereObject].beginObjectPosition,childSource->m[whereObject].endObjectPosition);
		}
		else
		{
			copyChildObject=childSource->m[whereChild].getObject();
			whereObject=whereChild;
			copySource(childSource,childSource->m[whereChild].beginObjectPosition,childSource->m[whereChild].endObjectPosition);
		}
		// copy childObject
		objects.push_back(childSource->objects[copyChildObject]);
		int whereParentObject=m.size()-childSource->m[whereObject].endObjectPosition+whereObject;
		int parentObject=objects.size()-1;
		m[whereParentObject].setObject(parentObject);
		m[whereParentObject].objectMatches.clear();
		if (childSource->objects[copyChildObject].getOwnerWhere()>=0)
		{
			m.push_back(childSource->m[childSource->objects[copyChildObject].getOwnerWhere()]);
			adjustOffsets(childSource->objects[copyChildObject].getOwnerWhere());
			objects[parentObject].setOwnerWhere(m.size()-1);
		}
		objects[parentObject].begin=m[whereParentObject].beginObjectPosition=whereParentObject-(whereObject-childSource->m[whereObject].beginObjectPosition);
		objects[parentObject].end=m[whereParentObject].endObjectPosition=whereParentObject+(childSource->m[whereObject].endObjectPosition-whereObject);
		objects[parentObject].originalLocation=whereParentObject;
		objects[parentObject].relativeClausePM=-1; // should copy later
		objects[parentObject].whereRelativeClause=-1; // should copy later
		objects[parentObject].locations.clear();
		objects[parentObject].replacedBy=-1;
		objects[parentObject].duplicates.clear();
		objects[parentObject].eliminated=false;
		copyDirectlyAttachedPrepositionalPhrases(whereParentObject,childSource,whereObject);
		wstring tmpstr,tmpstr2;
		lplog(LOG_WHERE,L"Transferred %d:%s to %d:%s",whereChild,childSource->whereString(whereChild,tmpstr,true).c_str(),whereParentObject,whereString(whereParentObject,tmpstr2,true).c_str());
		return whereParentObject;
	}
}

int	cQuestionAnswering::parseSubQueriesParallel(Source *questionSource,Source *childSource, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer, set <wstring> &wikipediaLinksAlreadyScanned)
{
	LFS
	for (vector <cSpaceRelation>::iterator sqi = subQueries.begin(), sqiEnd = subQueries.end(); sqi != sqiEnd; sqi++)
	{
		// add the answer as a point of interest.
		// copy the answer into the parent
		sqi->whereSubject = questionSource->copyChildIntoParent(childSource, whereChildCandidateAnswer);
		set<int> saveQISO = sqi->whereQuestionInformationSourceObjects;
		sqi->whereQuestionInformationSourceObjects.insert(sqi->whereSubject);
		sqi->subQuery = true;
		for (set <int>::iterator si = sqi->whereQuestionInformationSourceObjects.begin(), siEnd = sqi->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
		{
			if (questionSource->m[*si].getObject() < 0)
				continue;
			vector <cTreeCat *> rdfTypes;
			unordered_map <wstring, int > topHierarchyClassIndexes;
			questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__));
			set<wstring> abstractTypes;
			for (unsigned int r = 0; r < rdfTypes.size(); r++)
				if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch) && abstractTypes.find(rdfTypes[r]->typeObject) == abstractTypes.end())
				{
					abstractTypes.insert(rdfTypes[r]->typeObject);
					Source *abstractSource = NULL;
					processAbstract(questionSource,rdfTypes[r], abstractSource, true);
					int maxPrepositionalPhraseNonMixMatch = -1;
					questionSource->ppExtensionAvailable(*si, maxPrepositionalPhraseNonMixMatch, true);
					int minPrepositionalPhraseNonMixMatch = (maxPrepositionalPhraseNonMixMatch == 0) ? 0 : 1;
					for (int I = maxPrepositionalPhraseNonMixMatch; I >= minPrepositionalPhraseNonMixMatch; I--)
					{
						// also process wikipedia entry
						Source *wikipediaSource = NULL;
						processWikipedia(questionSource,*si, wikipediaSource, rdfTypes[r]->wikipediaLinks, I, true, wikipediaLinksAlreadyScanned);
						Source *wikipediaLinkSource = NULL;
						processWikipedia(questionSource,-1, wikipediaLinkSource, rdfTypes[r]->wikipediaLinks, I, true, wikipediaLinksAlreadyScanned);
					}
				}
			if (!Ontology::cacheRdfTypes)
				for (unsigned int r = 0; r < rdfTypes.size(); r++)
					delete rdfTypes[r]; // now caching them
		}
		sqi->whereQuestionInformationSourceObjects = saveQISO;
	}
	return 0;
}

bool cQuestionAnswering::analyzeRDFTypes(Source *questionSource, vector <cSpaceRelation>::iterator sri, cSpaceRelation *ssri, wstring derivation,vector < cAS > &answerSRIs, int &maxAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, unordered_map <int, WikipediaTableCandidateAnswers * > &wikiTableMap,bool subQueryFlag)
{
	wchar_t sqderivation[1024];
	wstring tmpstr;
	bool whereQuestionInformationSourceObjectsSkipped=true;
	for (set <int>::iterator si = sri->whereQuestionInformationSourceObjects.begin(), siEnd = sri->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
	{
		if (questionSource->m[*si].getObject() < 0 && questionSource->m[*si].objectMatches.empty())
		{
			lplog(LOG_WHERE, L"%s: Information Source Object is null!", derivation.c_str());
			continue;
		}
		vector <cTreeCat *> rdfTypes;
		unordered_map <wstring, int > topHierarchyClassIndexes;
		questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__));
		if (rdfTypes.empty() && sri->wherePrep >= 0)
		{
			questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__), 1);
			if (rdfTypes.empty() && sri->wherePrep >= 0 && questionSource->m[sri->wherePrep].relPrep >= 0)
				questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__), 2);
		}
		Ontology::setPreferred(topHierarchyClassIndexes, rdfTypes);
		set<wstring> preferredTypes;
		set <wstring> wikipediaLinksAlreadyScanned;
		for (unsigned int r = 0; r < rdfTypes.size(); r++)
		{
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch) && preferredTypes.find(rdfTypes[r]->typeObject) == preferredTypes.end())
			{
				preferredTypes.insert(rdfTypes[r]->typeObject);
				rdfTypes[r]->logIdentity(LOG_WHERE, (subQueryFlag) ? L"subQueries":L"processQuestionSource", false);
				// find subject or object without question
				int numWords;
				wstring tmpstr2;
				StringCbPrintf(sqderivation, 1024 * sizeof(wchar_t), L"%s:%06d: informationSourceObject %s:rdfType %d:%s:", derivation.c_str(), sri->where, questionSource->whereString(*si, tmpstr, false, 6, L" ", numWords).c_str(), r, rdfTypes[r]->toString(tmpstr2).c_str());
				analyzeQuestionFromRDFType(questionSource, sqderivation, *si, ssri, rdfTypes[r], false, answerSRIs, maxAnswer, wikiTableMap, mapPatternAnswer, mapPatternQuestion, wikipediaLinksAlreadyScanned);
				whereQuestionInformationSourceObjectsSkipped = false;
			}
			else 
				rdfTypes[r]->logIdentity(LOG_WHERE, (subQueryFlag) ? L"subQueriesNP":L"processQuestionSourceNP", false);
		}
		if (preferredTypes.empty() && rdfTypes.size() > 0)
		{
			lplog(LOG_WHERE, L"%s:SKIPPED - NO PREFERRED RDF TYPES (%d)", sqderivation, rdfTypes.size());
			for (unsigned int r = 0; r < rdfTypes.size(); r++)
				if (rdfTypes[r]->cli->first != SEPARATOR)
					rdfTypes[r]->logIdentity(LOG_WHERE, L"NO PREFERRED RDF:", false);
		}
		if (!Ontology::cacheRdfTypes)
			for (unsigned int r = 0; r < rdfTypes.size(); r++)
				delete rdfTypes[r]; // now caching them
		// send to web search for scraping and parsing of open domain
	}
	return whereQuestionInformationSourceObjectsSkipped;
}

// qualify a answer with a subquery derived from the original question
// 
// original question - What prize which originated in Spain has Krugman won?
// derived main question answered - Paul Krugman won prize?
//         [S Paul Krugman] won  [O prize[which originated in Spain]]? - (the relative phrase is ignored for the purposes of matching the derived question.)
// subquery - prize originated in spain? 
//         [S prize[10-11][10][nongen][N][which originated in Spain[11-15]]] originated  in   [PO Spain[14-15][14][ngname][N][WikiBusiness][WikiPlace][country]]?

//  for each subquery
//    create a dynamic query by 
//           making the candidate answer the subject (A. Jenkins B. Nobel Prize)
//           the verb of the infinitive or relative clause (A. ran B. originated)
//           the object if definite object OR object of prep if definite object is 'what' (A. Jenkins ran in the what? B. The Nobel Prize originated in what?)
//    The answer [object or object of prep] of the dynamic query must match the object or object of prep of the relative clause/infinitive
//       A. Olympics
//       B. Spain
// derivation [input] - how LP arrived at this point in processing
// childSource [input] - contains child candidate answer to be matched against subquery
// anySemanticMismatch [output] - if the candidate answer has any matches against a particular subquery which do not correspond with the desired answer .
// subQueryNoMatch [output] - if the candidate answer did not provide any match against the desired answer.
// subQueries [input] - question derived from the original question which further qualifies a potential answer (these answers have already been calculated from the derived main question)
//    each subquery has a set of question information sources that are used to search dbpedia (rdfTypes) and do a wikipedia search.
// whereChildCandidateAnswer [input] - the beginning of the potential answer
// whereChildCandidateAnswerEnd [input] - the end of the potential answer
// numConsideredParentAnswer [input] - tracking number for potential answer
// semMatchValue [input] - used for return value if match was consistent
// mapPatternAnswer [input] - used for meta transformation of answers 
// mapPatternQuestion [input] - used for meta transformation of questions
// useParallelQuery [input] - will the web sources be parsed in parallel or not
int	cQuestionAnswering::matchSubQueries(Source *questionSource,wstring derivation,Source *childSource,int &anySemanticMismatch,bool &subQueryNoMatch,vector <cSpaceRelation> &subQueries,
	int whereChildCandidateAnswer,int whereChildCandidateAnswerEnd,int numConsideredParentAnswer,int semMatchValue,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,bool useParallelQuery)
{ LFS
	wstring childWhereString;
	int numWords=0;
	if (whereChildCandidateAnswerEnd < 0)
		childSource->whereString(whereChildCandidateAnswer, childWhereString, true, 6, L" ", numWords);
	else
	{
		childSource->phraseString(whereChildCandidateAnswer, whereChildCandidateAnswerEnd, childWhereString, true);
		numWords = whereChildCandidateAnswerEnd - whereChildCandidateAnswer;
	}
	if (childCandidateAnswerMap.find(childWhereString)!=childCandidateAnswerMap.end())
	{
		anySemanticMismatch=childCandidateAnswerMap[childWhereString].anySemanticMismatch;
		subQueryNoMatch=childCandidateAnswerMap[childWhereString].subQueryNoMatch;
		lplog(LOG_WHERE, L"parent considered answer %d:child subject=%s CACHED [%d]", numConsideredParentAnswer, childWhereString.c_str(), childCandidateAnswerMap[childWhereString].confidence);
		return childCandidateAnswerMap[childWhereString].confidence;
	}
	anySemanticMismatch = 0;
	subQueryNoMatch = false;
	//if (childWhereString==L"Prince of Asturias Awards in Social Sciences")
	//{
	//	logQuestionProfileTime=1;
	//	logSynonymDetail=1;
	//	logTableDetail=1;
	//	equivalenceLogDetail=1;
	//	logQuestionDetail=1;
	//}
	wstring tmpstr;
	bool allSubQueriesMatch=true,headerPrinted=false;
	for (vector <cSpaceRelation>::iterator sqi=subQueries.begin(),sqiEnd=subQueries.end(); sqi!=sqiEnd; sqi++)
	{
		// add the answer as a point of interest.
		// copy the answer into the parent
		if (subQueries.size()>1)
			lplog(LOG_WHERE,L"%s:subquery %d:child subject=%s",derivation.c_str(),sqi-subQueries.begin(),childWhereString.c_str());
		else
			lplog(LOG_WHERE,L"%s:subquery child subject=%s",derivation.c_str(),childWhereString.c_str());
		sqi->whereSubject= questionSource->copyChildIntoParent(childSource,whereChildCandidateAnswer);
		set<int> saveQISO=sqi->whereQuestionInformationSourceObjects;
		sqi->whereQuestionInformationSourceObjects.insert(sqi->whereSubject);
		sqi->subQuery=true;
		vector < cAS > answerSRIs;
		int maxAnswer=-1;
		wstring ps,tmpMatchInfo;
		questionSource->prepPhraseToString(sqi->wherePrep,ps);
		wchar_t sqderivation[1024];
		lplog(LOG_WHERE,L"parent considered answer %d:child subject=%s BEGIN",numConsideredParentAnswer,childWhereString.c_str());
		StringCbPrintf(sqderivation,1024*sizeof(wchar_t),L"%s:SUBQUERY #%d",derivation.c_str(),sqi-subQueries.begin());
		questionSource->printSRI(sqderivation,&(*sqi),0,sqi->whereSubject,sqi->whereObject,ps,false,-1,tmpMatchInfo);
		unordered_map <int, WikipediaTableCandidateAnswers * > wikiTableMap;
		bool whereQuestionInformationSourceObjectsSkipped=analyzeRDFTypes(questionSource, sqi, &(*sqi),derivation, answerSRIs, maxAnswer, mapPatternAnswer, mapPatternQuestion, wikiTableMap,true);
		lplog(LOG_WHERE,L"%s:SEARCHING WEB%s ************************************************************",derivation.c_str(),(whereQuestionInformationSourceObjectsSkipped) ? L" (SKIPPED ALL informationSourceObjects)":L"");
		sqi->whereQuestionInformationSourceObjects=saveQISO;
		vector <wstring> webSearchQueryStrings;
		getWebSearchQueries(questionSource,&(*sqi),webSearchQueryStrings);
		for (int webSearchOffset = 0; webSearchOffset < (signed)webSearchQueryStrings.size(); webSearchOffset++)
		{
			if (useParallelQuery)
				webSearchForQueryParallel(questionSource,(wchar_t *)derivation.c_str(), &(*sqi), false, answerSRIs, maxAnswer, 10, 1, true, webSearchQueryStrings, webSearchOffset, mapPatternAnswer, mapPatternQuestion);
			else
				webSearchForQuerySerial(questionSource,(wchar_t *)derivation.c_str(), &(*sqi), false, answerSRIs, maxAnswer, 10, 1, true, webSearchQueryStrings, webSearchOffset, mapPatternAnswer, mapPatternQuestion);
		}
		bool anyMatch=false;
		for (unsigned int I=0; I<answerSRIs.size(); I++)
		{
			if (answerSRIs[I].matchSum==maxAnswer || answerSRIs[I].matchSum >= 18)
			{
				StringCbPrintf(sqderivation,1024*sizeof(wchar_t),L"%s:SUBQUERY #%d answer #%d",derivation.c_str(),sqi-subQueries.begin(),I);
				int semanticMismatch=(answerSRIs[I].matchInfo.find(L"WIKI_OTHER_ANSWER")!=wstring::npos) ? 12 : 0;
				bool match=answerSRIs[I].matchInfo.find(L"ANSWER_MATCH[")!=wstring::npos;
				wchar_t *wm=L"";
				if (semanticMismatch)
					wm=L"MISMATCH";
				if (match)
					wm=L"MATCH";
				ps.clear();
				answerSRIs[I].source->prepPhraseToString(answerSRIs[I].wp,ps);
				answerSRIs[I].source->printSRI(L"["+wstring(sqderivation)+L"]",answerSRIs[I].sri,0,answerSRIs[I].ws,answerSRIs[I].wo,ps,false,answerSRIs[I].matchSum,answerSRIs[I].matchInfo);
				wstring tmpstr2;
				if (!headerPrinted)
					lplog(LOG_WHERE,L"%s:SUBQUERY ANSWER LIST %s ************************************************************",derivation.c_str(),answerSRIs[I].source->whereString(answerSRIs[I].sri->whereObject,tmpstr2,false).c_str());
				if (questionSource->inObject(sqi->whereObject,sqi->whereQuestionType))
					lplog(LOG_WHERE,L"[subquery answer %d]:%s OBJECT expected child answer=%s (source=%s).",I,wm, questionSource->whereString(sqi->whereObject,tmpstr,false).c_str(),answerSRIs[I].source->sourcePath.c_str());
				else if (questionSource->inObject(sqi->wherePrepObject,sqi->whereQuestionType))
					lplog(LOG_WHERE,L"[subquery answer %d]:%s PREPOBJECT expected child answer=%s (source=%s).",I,wm, questionSource->whereString(sqi->wherePrepObject,tmpstr,false).c_str(),answerSRIs[I].source->sourcePath.c_str());
				else
					lplog(LOG_WHERE,L"[subquery answer %d]:NOT OBJECT OR PREPOBJECT %s (answer source=%s).",I,wm,answerSRIs[I].source->sourcePath.c_str());
				headerPrinted=true;
				anyMatch|=match;
				anySemanticMismatch|=semanticMismatch;
			}
		}
		if (!anyMatch)
			lplog(LOG_WHERE, L"%s:NO SUBQUERY ANSWERS FOUND", sqderivation);
		allSubQueriesMatch=allSubQueriesMatch && anyMatch;
		answerSRIs.clear();
		lplog(LOG_WHERE, L"parent considered answer %d:child subject=%s [%d] END",
			numConsideredParentAnswer, childWhereString.c_str(), (allSubQueriesMatch) ? semMatchValue : CONFIDENCE_NOMATCH);
	}
	//if (childWhereString==L"Prince of Asturias Awards in Social Sciences")
	//{
	//	logQuestionProfileTime=0;
	//	logSynonymDetail=0;
	//	logTableDetail=0;
	//	equivalenceLogDetail=0;
	//	logQuestionDetail=0;
	//}
	childCandidateAnswerMap[childWhereString].anySemanticMismatch=anySemanticMismatch;
	subQueryNoMatch=childCandidateAnswerMap[childWhereString].subQueryNoMatch=!allSubQueriesMatch;
	return childCandidateAnswerMap[childWhereString].confidence=(allSubQueriesMatch) ? semMatchValue : CONFIDENCE_NOMATCH;
}

int cQuestionAnswering::questionTypeCheck(Source *questionSource,wstring derivation, cSpaceRelation* parentSRI, cAS &childCAS, int &semanticMismatch, bool &unableToDoquestionTypeCheck,
	bool &subQueryNoMatch, vector <cSpaceRelation> &subQueries, int numConsideredParentAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion,bool useParallelQuery)
{
	LFS
	unableToDoquestionTypeCheck = true;
	int qt=parentSRI->questionType&typeQTMask;
	if ((qt == whatQTFlag || qt == whichQTFlag) && (parentSRI->questionType&QTAFlag) && parentSRI->whereQuestionTypeObject > 0)
	{
		if (questionSource->matchChildSourcePositionSynonym(Words.query(L"person"), questionSource, parentSRI->whereQuestionTypeObject))
			qt = whomQTFlag;
		else if (questionSource->matchChildSourcePositionSynonym(Words.query(L"place"), questionSource, parentSRI->whereQuestionTypeObject))
			qt = whereQTFlag;
		else if (questionSource->matchChildSourcePositionSynonym(Words.query(L"business"), questionSource, parentSRI->whereQuestionTypeObject))
			qt = wikiBusinessQTFlag;
		else if (questionSource->matchChildSourcePositionSynonym(Words.query(L"book"), questionSource, parentSRI->whereQuestionTypeObject) ||
			questionSource->matchChildSourcePositionSynonym(Words.query(L"album"), questionSource, parentSRI->whereQuestionTypeObject) ||
			questionSource->matchChildSourcePositionSynonym(Words.query(L"song"), questionSource, parentSRI->whereQuestionTypeObject))
			qt = wikiWorkQTFlag;
		else if ((questionSource->m[parentSRI->whereQuestionTypeObject].word->second.timeFlags&T_UNIT))
			qt = whenQTFlag;
		else
			return CONFIDENCE_NOMATCH;
	}
	else if (qt != whereQTFlag && qt != whoseQTFlag && qt != whenQTFlag && qt != whomQTFlag)
		return CONFIDENCE_NOMATCH;
	unableToDoquestionTypeCheck = false;
	int semMatchValue = 1;
	if (childCAS.sri->whereChildCandidateAnswer<0)
	{
		lplog(LOG_WHERE,L"whereChildCandidateAnswer not found!");
		semanticMismatch=13;
		return CONFIDENCE_NOMATCH;
	}
	int childWhere=childCAS.sri->whereChildCandidateAnswer,childObject=childCAS.source->m[childWhere].getObject();
	if (childCAS.source->m[childWhere].objectMatches.size()>0)
		childObject=childCAS.source->m[childWhere].objectMatches[0].object;
	if (childObject<0)
		return CONFIDENCE_NOMATCH;
	//noise reduction - Jay-Z and Beyonce WedIn a private ceremony, Jay-Z married his ... snippet as an incomplete sentence that is parsed incorrectly
	if (childCAS.source->m[childWhere].queryWinnerForm(possessivePronounForm)>=0) // his/her/its - don't allow match to an adjective, even though it is marked as an object.
	{
		semanticMismatch=14;
		return CONFIDENCE_NOMATCH;
	}
	semMatchValue=childCAS.source->checkParticularPartQuestionTypeCheck(qt,childWhere,childObject,semanticMismatch);
	wstring tmpstr;
	if (logQuestionDetail)
		lplog(LOG_WHERE, L"checkParticularPartQuestionTypeCheck: %d compared with %s yields matchValue %d", qt, questionSource->objectString(childObject, tmpstr, false).c_str(),semMatchValue);
	if (semanticMismatch)
	{
		wstring ps;
		childCAS.source->prepPhraseToString(childCAS.wp,ps);
		childCAS.source->printSRI(L"questionTypeCheck semanticMismatch",childCAS.sri,0,childCAS.ws,childCAS.wo,ps,false,-1,L"");
	}
	if (semanticMismatch || subQueries.empty())
		return semMatchValue;
	int subQueryConfidenceMatch=matchSubQueries(questionSource,derivation,childCAS.source,semanticMismatch,subQueryNoMatch,subQueries,childCAS.sri->whereChildCandidateAnswer,-1,numConsideredParentAnswer,semMatchValue,mapPatternAnswer,mapPatternQuestion,useParallelQuery);
	wstring tmpstr1,tmpstr2;
	int numWords=0;
	lplog(LOG_WHERE,L"%d:subquery comparison whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d (1)",numConsideredParentAnswer-1,
		childCAS.sri->whereChildCandidateAnswer,childCAS.source->whereString(childCAS.sri->whereChildCandidateAnswer,tmpstr2,false,6,L" ",numWords).c_str(),
		semanticMismatch,(subQueryNoMatch) ? L"true":L"false",subQueryConfidenceMatch);
	return subQueryConfidenceMatch;
}

void cQuestionAnswering::verbTenseMatch(Source *questionSource, cSpaceRelation* parentSRI, cAS &childCAS,bool &tenseMismatch)
{
	int pvs=0, cvs=0;
	tenseMismatch = (parentSRI->whereVerb >= 0 && childCAS.sri->whereVerb >= 0 &&
		(pvs = questionSource->m[parentSRI->whereVerb].verbSense&~VT_POSSIBLE) != (cvs = childCAS.source->m[childCAS.sri->whereVerb].verbSense));
	if (parentSRI->isConstructedRelative)
		tenseMismatch = cvs != VT_PAST;
	if (tenseMismatch)
	{
		// has won and won are the same in this context
		// (pvs==VT_PRESENT && cvs==VT_PRESENT_PERFECT) - parent is On what TV show does Hammond regularly appear child is 'appeared eight  [O eight times[109-111]{WO:eight}[110][nongen][N][PL][OGEN]]on   [PO (Saturday Night Live[26-29][26][ngname][N][WikiWork]-4620) show]'
		// if the questions are in the past, and the web search is in the present, then 
		if ((pvs == VT_PRESENT_PERFECT && cvs == VT_PAST) || (cvs == VT_PRESENT_PERFECT && pvs == VT_PAST) || (questionSource->sourceInPast && pvs == VT_PRESENT && cvs == VT_PRESENT_PERFECT))
			tenseMismatch = false;
		// PARENT: what     [S Darrell Hammond]  featured   on [PO Comedy Central program[12-15][14][nongen][N]]?
		// CHILD: [S Darrell Hammond]  was [O a featured cast member[67-71][70][nongen][N]] on   [PO Saturday Night Live[72-75][72][ngname][N][WikiWork]].
		else if (childCAS.matchInfo.find(L"MOVED_VERB_TO_OBJECT") != wstring::npos)
			tenseMismatch = false;
		else
		{
			wstring tmpstr1, tmpstr2;
			lplog(LOG_WHERE, L"tense mismatch between parent=%d:%s and child=%d:%s.",
				parentSRI->whereVerb, senseString(tmpstr1, questionSource->m[parentSRI->whereVerb].verbSense).c_str(), childCAS.sri->whereVerb, senseString(tmpstr2, childCAS.source->m[childCAS.sri->whereVerb].verbSense).c_str());
		}
	}
}

int cQuestionAnswering::semanticMatch(Source *questionSource, wstring derivation,cSpaceRelation* parentSRI,cAS &childCAS,int &semanticMismatch,bool &subQueryNoMatch,
	                        vector <cSpaceRelation> &subQueries,int numConsideredParentAnswer,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,bool useParallelQuery)
{ LFS
	// check if verb tenses match!
	int semMatchValue=1;
	bool synonym=false;
	setWhereChildCandidateAnswer(questionSource,childCAS, parentSRI);
	if (childCAS.matchInfo==L"META_PATTERN" || childCAS.matchInfo==L"SEMANTIC_MAP")
	{
		parentSRI->whereQuestionTypeObject=parentSRI->whereSubject;
		if (!childCAS.sri->nonSemanticSubjectTotalMatch)
			semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRI->whereSubject, childCAS.source, childCAS.sri->whereChildCandidateAnswer, -1, synonym, semanticMismatch);
	}
	else
	{
		parentSRI->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, parentSRI);
		if (questionSource->inObject(parentSRI->whereSubject,parentSRI->whereQuestionType))
		{
			if (!childCAS.sri->nonSemanticSubjectTotalMatch)
				semMatchValue= questionSource->checkParticularPartSemanticMatch(LOG_WHERE,parentSRI->whereSubject,childCAS.source,childCAS.sri->whereChildCandidateAnswer,-1,synonym,semanticMismatch);
		}
		else if (questionSource->inObject(parentSRI->whereObject,parentSRI->whereQuestionType))
		{
			if (!childCAS.sri->nonSemanticObjectTotalMatch)
				semMatchValue= questionSource->checkParticularPartSemanticMatch(LOG_WHERE,parentSRI->whereObject,childCAS.source,childCAS.sri->whereChildCandidateAnswer,-1,synonym,semanticMismatch);
			else 
				// Curveball's Profession is what? - if the question word is not adjectival, and verb is the identity verb (is) then check subject 
				// What was Curveball (commonProfession)
				if (!(parentSRI->questionType&QTAFlag) && parentSRI->whereSubject>=0 && (questionSource->m[parentSRI->whereSubject].objectRole&IS_OBJECT_ROLE))
				{
					wstring tmpstr;
					if (questionSource->m[parentSRI->whereObject].questionTransformationSuggestedPattern.length())
					{
						semanticMismatch=(childCAS.source->m[childCAS.sri->whereObject].queryForm(questionSource->m[parentSRI->whereObject].questionTransformationSuggestedPattern)==-1) ? 14 : 0;
						if (logQuestionDetail)
							lplog(LOG_WHERE,L"semanticMismatch %s questionTransformationSuggestedPattern=%d:%s [form %s]",
							(semanticMismatch) ? L"NO MATCH":L"MATCH",childCAS.sri->whereObject,childCAS.source->whereString(childCAS.sri->whereObject,tmpstr,false).c_str(), questionSource->m[parentSRI->whereObject].questionTransformationSuggestedPattern.c_str());
					}
					else
						semMatchValue= questionSource->checkParticularPartSemanticMatch(LOG_WHERE,parentSRI->whereSubject,childCAS.source,childCAS.sri->whereChildCandidateAnswer,-1,synonym,semanticMismatch);
				}
		}
		else if (questionSource->inObject(parentSRI->wherePrepObject,parentSRI->whereQuestionType) && childCAS.wp>=0)
		{
			if (!childCAS.sri->nonSemanticPrepositionObjectTotalMatch)
				semMatchValue= questionSource->checkParticularPartSemanticMatch(LOG_WHERE,parentSRI->wherePrepObject,childCAS.source,childCAS.sri->whereChildCandidateAnswer,-1,synonym,semanticMismatch);
		}
		else if (questionSource->inObject(parentSRI->whereSecondaryObject,parentSRI->whereQuestionType))
		{
			if (!childCAS.sri->nonSemanticSecondaryObjectTotalMatch)
				semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRI->whereSecondaryObject, childCAS.source, childCAS.sri->whereChildCandidateAnswer, -1, synonym, semanticMismatch);
		}
		else
			return CONFIDENCE_NOMATCH;
	}
	if (childCAS.sri->whereChildCandidateAnswer<0)
	{
		lplog(LOG_WHERE,L"whereChildCandidateAnswer not found!");
		semanticMismatch=15;
		return CONFIDENCE_NOMATCH;
	}
	if (subQueries.empty())
		return semMatchValue;
	int subQueryConfidenceMatch=matchSubQueries(questionSource, derivation,childCAS.source,semanticMismatch,subQueryNoMatch,subQueries,childCAS.sri->whereChildCandidateAnswer,-1,numConsideredParentAnswer,semMatchValue,mapPatternAnswer,mapPatternQuestion,useParallelQuery);
	wstring tmpstr1,tmpstr2;
	int numWords=0;
	lplog(LOG_WHERE,L"%d:subquery comparison between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d (2)",numConsideredParentAnswer-1,
		parentSRI->whereQuestionTypeObject,questionSource->whereString(parentSRI->whereQuestionTypeObject,tmpstr1,false).c_str(),childCAS.sri->whereChildCandidateAnswer,childCAS.source->whereString(childCAS.sri->whereChildCandidateAnswer,tmpstr2,false,6,L" ",numWords).c_str(),
		semanticMismatch,(subQueryNoMatch) ? L"true":L"false",subQueryConfidenceMatch);
	return subQueryConfidenceMatch;
}

// this is called from the parent
int cSemanticMap::cSemanticEntry::semanticCheck(cQuestionAnswering &qa,cSpaceRelation* parentSRI,Source *parentSource)
{ LFS
	if (confidenceCheck && childSource==0)
	{
		qa.processPath(parentSource,lastChildSourcePath.c_str(),childSource,Source::WEB_SEARCH_SOURCE_TYPE,1,false);
		bool namedNoMatch=false,synonym=false;
		int parentSemanticMismatch=0, adjectivalMatch=-1;
		// President George W . Bush[President George W Bush [509][name][M][H1:President F:George M1:W L:Bush [91]][WikiPerson]]
		{
			wstring tmpstr;
			childSource->whereString(childWhere2,tmpstr,false);
			static int cttmp=0;
			//if (tmpstr.find(L"President Bush")==0 && cttmp<10) //  && tmpstr.find(L"Person")!=wstring::npos
			//{
			//	lplog(LOG_WHERE,L"%d:%s",parentSRI->whereSubject,tmpstr.c_str());
			//	cttmp++;
				//logSynonymDetail=logTableDetail=equivalenceLogDetail=logQuestionDetail=1;
			//}
		}

		if (qa.matchSourcePositions(parentSource,parentSRI->whereSubject,childSource,childWhere2,namedNoMatch,synonym,true,parentSemanticMismatch,adjectivalMatch,parentSource->debugTrace))
		{
			if (parentSemanticMismatch)
				return confidenceSE=CONFIDENCE_NOMATCH;
			return confidenceSE=(synonym) ? CONFIDENCE_NOMATCH*3/4 : 0;
		}
		if (namedNoMatch)
			return confidenceSE=CONFIDENCE_NOMATCH;
//		confidence=parentSource->semanticMatchSingle(L"accumulateSemanticEntry",parentSRI,childSource,childWhere2,childObject,semanticMismatch,subQueryNoMatch,subQueries,-1,mapPatternAnswer,mapPatternQuestion);
	}
	return confidenceSE=CONFIDENCE_NOMATCH;
}

void cSemanticMap::cSemanticEntry::printDirectRelations(cQuestionAnswering &qa, int logType,Source *parentSource,wstring &path,int where)
{ LFS
	if (qa.processPath(parentSource,path.c_str(),childSource,Source::WEB_SEARCH_SOURCE_TYPE,1,false)>=0)
	{
		vector <cSpaceRelation>::iterator sri=childSource->findSpaceRelation(where);
		if (sri==childSource->spaceRelations.end() || sri->printMin>where || sri->printMax<where)
		{
			wstring tmpstr,tmpstr2,tmpstr3;
			::lplog(logType,L"%s:%d:SM %s:SUBJ[%s] VERB[%s] OBJECT[%s]",path.c_str(),where,childSource->m[where].word->first.c_str(),
				(childSource->m[where].relSubject>=0) ? childSource->whereString(childSource->m[where].relSubject,tmpstr,true).c_str():L"",
				(childSource->m[where].getRelVerb()>=0) ? childSource->whereString(childSource->m[where].getRelVerb(),tmpstr2,true).c_str():L"",
				(childSource->m[where].getRelObject()>=0) ? childSource->whereString(childSource->m[where].getRelObject(),tmpstr3,true).c_str():L"");
		}
		else
			childSource->printSRI(path.c_str(),&(*sri),0,sri->whereSubject,sri->whereObject,sri->wherePrep,false,-1,L"DIRECT_RELATION",logType|LOG_QCHECK);
	}
}

int cQuestionAnswering::semanticMatchSingle(Source *questionSource, wstring derivation,cSpaceRelation* parentSRI,Source *childSource,int whereChild,int childObject,int &semanticMismatch,bool &subQueryNoMatch,
	                        vector <cSpaceRelation> &subQueries,int numConsideredParentAnswer,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,bool useParallelQuery)
{ LFS
	int semMatchValue=1;
	bool synonym=false;
	semMatchValue= questionSource->checkParticularPartSemanticMatch(LOG_WHERE,parentSRI->whereSubject,childSource,whereChild,childObject,synonym,semanticMismatch);
	if (subQueries.empty())
		return semMatchValue;
	int subQueryConfidenceMatch=matchSubQueries(questionSource,derivation,childSource,semanticMismatch,subQueryNoMatch,subQueries,whereChild,-1,numConsideredParentAnswer,semMatchValue,mapPatternAnswer,mapPatternQuestion,useParallelQuery);
	wstring tmpstr1,tmpstr2;
	int numWords=0;
	lplog(LOG_WHERE,L"%d:subquery comparison between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d (3)",numConsideredParentAnswer-1,
		parentSRI->whereQuestionTypeObject, questionSource->whereString(parentSRI->whereQuestionTypeObject,tmpstr1,false).c_str(),whereChild,childSource->whereString(whereChild,tmpstr2,false,6,L" ",numWords).c_str(),
		semanticMismatch,(subQueryNoMatch) ? L"true":L"false",subQueryConfidenceMatch);
	return subQueryConfidenceMatch;
}

// do cas1 and cas2 give the same answer?
bool cQuestionAnswering::checkIdentical(Source *questionSource, cSpaceRelation* sri,cAS &cas1,cAS &cas2)
{ LFS
	if (questionSource->inObject(sri->whereSubject,sri->whereQuestionType))
		return checkParticularPartIdentical(cas1.source,cas2.source,cas1.ws,cas2.ws);
	if (questionSource->inObject(sri->whereObject,sri->whereQuestionType))
		return checkParticularPartIdentical(cas1.source,cas2.source,cas1.wo,cas2.wo);
	if (questionSource->inObject(sri->wherePrepObject,sri->whereQuestionType) && cas1.wp>=0 && cas2.wp>=0)
		return checkParticularPartIdentical(cas1.source,cas2.source,cas1.source->m[cas1.wp].getRelObject(),cas2.source->m[cas2.wp].getRelObject());
	if (questionSource->inObject(sri->whereSecondaryObject,sri->whereQuestionType))
		return checkParticularPartIdentical(cas1.source,cas2.source,cas1.sri->whereSecondaryObject,cas2.sri->whereSecondaryObject);
	if (sri->whereSecondaryPrep>=0 && cas1.sri->whereSecondaryPrep>=0 && cas2.sri->whereSecondaryPrep>=0 && questionSource->inObject(questionSource->m[sri->whereSecondaryPrep].getRelObject(),sri->whereQuestionType))
		return checkParticularPartIdentical(cas1.source,cas2.source,cas1.source->m[cas1.sri->whereSecondaryPrep].getRelObject(),cas2.source->m[cas2.sri->whereSecondaryPrep].getRelObject());
	return false;
}

void cQuestionAnswering::setWhereChildCandidateAnswer(Source *questionSource,cAS &childCAS, cSpaceRelation* parentSRI)
{
	childCAS.sri->whereChildCandidateAnswer = -1;
	if (childCAS.matchInfo == L"META_PATTERN" || childCAS.matchInfo == L"SEMANTIC_MAP")
		childCAS.sri->whereChildCandidateAnswer = childCAS.ws;
	else if (questionSource->inObject(parentSRI->whereSubject, parentSRI->whereQuestionType))
		childCAS.sri->whereChildCandidateAnswer = childCAS.sri->whereSubject;
	else if (questionSource->inObject(parentSRI->whereObject, parentSRI->whereQuestionType))
		childCAS.sri->whereChildCandidateAnswer = childCAS.sri->whereObject;
	else if (questionSource->inObject(parentSRI->wherePrepObject, parentSRI->whereQuestionType) && childCAS.wp >= 0)
		childCAS.sri->whereChildCandidateAnswer = childCAS.source->m[childCAS.wp].getRelObject();
	else if (questionSource->inObject(parentSRI->whereSecondaryObject, parentSRI->whereQuestionType))
	{
		if ((childCAS.sri->whereChildCandidateAnswer = childCAS.sri->whereSecondaryObject)<0)
			childCAS.sri->whereChildCandidateAnswer = childCAS.sri->whereObject;
	}
	else if (parentSRI->whereSecondaryPrep >= 0 && questionSource->inObject(questionSource->m[parentSRI->whereSecondaryPrep].getRelObject(), parentSRI->whereQuestionType))
		childCAS.sri->whereChildCandidateAnswer = questionSource->m[parentSRI->whereSecondaryPrep].getRelObject();
}

int cQuestionAnswering::getWhereQuestionTypeObject(Source *questionSource,cSpaceRelation* sri)
{
	LFS
	int collectionWhere = -1;
	if (questionSource->inObject(sri->whereSubject, sri->whereQuestionType))
		collectionWhere = sri->whereSubject;
	else if (questionSource->inObject(sri->whereObject, sri->whereQuestionType))
		collectionWhere = sri->whereObject;
	else if (questionSource->inObject(sri->wherePrepObject, sri->whereQuestionType))
		collectionWhere = sri->wherePrepObject;
	else if (questionSource->inObject(sri->whereSecondaryObject, sri->whereQuestionType))
	{
		if ((collectionWhere = sri->whereSecondaryObject)<0)
			collectionWhere = sri->whereObject;
	}
	else if (sri->whereSecondaryPrep>=0 && questionSource->inObject(questionSource->m[sri->whereSecondaryPrep].getRelObject(),sri->whereQuestionType))
		collectionWhere= questionSource->m[sri->whereSecondaryPrep].getRelObject();
	return collectionWhere;
}

bool cQuestionAnswering::processPathToPattern(Source *questionSource,const wchar_t *path,Source *&source)
{ LFS
		source=new Source(&questionSource->mysql,Source::PATTERN_TRANSFORM_TYPE,0);
		source->numSearchedInMemory=1;
		source->isFormsProcessed=false;
		source->processOrder= questionSource->processOrder++;
		source->multiWordStrings= questionSource->multiWordStrings;
		source->multiWordObjects= questionSource->multiWordObjects;
		int repeatStart=0;
		wprintf(L"\nParsing pattern file %s...\n",path);
		unsigned int unknownCount=0,quotationExceptions=0,totalQuotations=0;
		int globalOverMatchedPositionsTotal=0;
		// id, path, start, repeatStart, etext, author, title from sources 
		//char *sqlrow[]={ "1", (char *)cPath.c_str(), "~~BEGIN","1","","",(char *)cPath.c_str() };
		wstring wpath = path, start = L"~~BEGIN",title,etext;
		source->tokenize(title,etext,wpath,L"",start,repeatStart,unknownCount);
		source->doQuotesOwnershipAndContractions(totalQuotations);
		source->printSentences(false,unknownCount,quotationExceptions,totalQuotations,globalOverMatchedPositionsTotal);
		source->addWNExtensions();
		source->identifyObjects();
		source->analyzeWordSenses();
		source->narrativeIsQuoted = false;
		source->syntacticRelations();
		source->identifySpeakerGroups(); 
		vector <int> secondaryQuotesResolutions;
		source->resolveSpeakers(secondaryQuotesResolutions);
		source->resolveFirstSecondPersonPronouns(secondaryQuotesResolutions);
		//source->resolveWordRelations(); // unnecessary because we are no longer dynamically updating word relations in DB and we are erasing updated word relations with every source read.
		return !source->m.empty();
}

// process source\lists\questionTransforms.txt
// create transformationMatchPatterns
void cQuestionAnswering::initializeTransformations(Source *questionSource,unordered_map <wstring, wstring> &parseVariables)
{ LFS
	if (transformationPatternMap.size())
		return;
	vector <cPattern *> patternsForAssignment,transformPatterns;
	int patternNum=0,existingReferences=patternReferences.size();
	if (processPathToPattern(questionSource,L"source\\lists\\questionTransforms.txt",transformSource))
	{
		for (int *I=transformSource->sentenceStarts.begin(); I!=transformSource->sentenceStarts.end(); I++)
		{
			if ((I+1)==transformSource->sentenceStarts.end()) break;
			transformPatterns.push_back(cPattern::create(transformSource,L"",patternNum++,*I,*(I+1)-1,parseVariables,transformSource->m[*I].t.traceCommonQuestion || transformSource->m[*I].t.traceConstantQuestion || transformSource->m[*I].t.traceQuestionPatternMap)); 
			transformPatterns[transformPatterns.size()-1]->metaPattern=transformSource->m[*I].t.traceQuestionPatternMap;
			patternsForAssignment.push_back(transformPatterns[transformPatterns.size()-1]);
			if (transformSource->m[*I].t.traceConstantQuestion || transformSource->m[*I].t.traceCommonQuestion || transformSource->m[*I].t.traceQuestionPatternMap)
			{
				vector <cSpaceRelation>::iterator sri=transformSource->findSpaceRelation(*I);
				if (sri!=transformSource->spaceRelations.end())
				{
					transformSource->printSRI(L"TRANSFORM",&(*sri),-1,sri->whereSubject,sri->whereObject,sri->wherePrep,false,-1,L"",LOG_INFO);
					transformationPatternMap[sri]=patternsForAssignment;
					patternsForAssignment.clear();
				}
			}
			if (transformSource->m[*I].t.traceTransitoryQuestion || transformSource->m[*I].t.traceMapQuestion)
			{
			}
		}
		bool patternError;
	  for (unsigned int r=existingReferences; r<patternReferences.size(); r++)
	    patternReferences[r]->resolve(transformPatterns,patternError);
	}
}

// How old is Darrell Hammond?
// What is the age of Darrell Hammond?
// What is Darrell Hammond's age?
// How many years old is Darrell Hammond?
// --> When was Darrell Hammond born?
//     How long since you have finished your degree?
//     How long have you been working?
// 
// this question is asking for an answer which is constantly changing.
// for best results, we must change the question into one that has a constant answer
bool cQuestionAnswering::detectTransitoryAnswer(Source *questionSource,cSpaceRelation* sri,cSpaceRelation * &ssri,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion)
{ LFS
	unordered_map <wstring, wstring> parseVariables;
	initializeTransformations(questionSource,parseVariables);
	for (map <vector <cSpaceRelation>::iterator,vector <cPattern *> >::iterator itPM=transformationPatternMap.begin(),tPMEnd=transformationPatternMap.end(); itPM!=tPMEnd; itPM++)
	{
		vector <cPattern *>::iterator constantPattern=itPM->second.end()-1;
		for (vector <cPattern *>::iterator ip=itPM->second.begin(),ipEnd=itPM->second.end(); ip!=ipEnd; ip++)
		{
			if ((*ip)->destinationPatternType) 
				continue;
			if (questionSource->matchPattern(*ip,sri->printMin,sri->printMin+2,false))
			{
				if ((*constantPattern)->metaPattern)
				{
					mapPatternAnswer=*constantPattern;
					mapPatternQuestion=*ip;
				}
				else
				{
					unordered_map <int,int> sourceMap;
					copySource(questionSource,&(*itPM->first),*ip,*constantPattern,sourceMap,parseVariables);
					ssri=new cSpaceRelation(itPM->first,sourceMap);
				}
				return true;
			}
		}
	}
	return false;
}

// if no proper noun is in the primary query, and a proper noun is in the subquery, answer the subquery first.
// What are titles of books written by Krugman? (1)
// What is the first computer manufactured by Apple? (1)
// What color was the first car manufactured by Ford? (2)

// 1. reorder if using Q1PASSIVE matched at _Q2+1 and a by clause
//    a. each answer to the subclause -> subject of main clause 
//    b. _IS [Q1PASSIVE] -> verb of the main clause
//    c. object-> object of main clause [what]
//    d. object of by clause -> subject of subclause [Krugman]
//    e. verb -> verb of subclause       [written]
//    f. take all prepositional objects and subject (titles): the nearest before the verb -> object of subclause [books]
// 2. reorder if the object is S_IN_REL and a by clause and controller exists
//    a. each answer to the subclause -> subject of main clause 
//    b. controller verb->verb of main clause [was]
//    c. controller subject -> object of main clause [color]
//    d. object of by clause -> by clause of subclause [by Ford]
//    e. verb -> verb of subclause [manufactured] 
//    f. subject -> subject of subclause [the first car]
// original interpretations
// What are titles of books written by Krugman?
// what     [S titles[37-38][37][nongen][N][PL]]  written   [O what]of   [PO books[39-40][39][nongen][N][PL]]?
// What is the first computer manufactured by Apple?
// what   first  [S the first computer[46-49]{WO:first}[48][nongen][N][OGEN]]  manufactured   [O what]by   [PO apple [51][name][M][F][A:apple ][unknown(place)]]?
// What color was the first car manufactured by Ford ?
// what[controller color[54-55][54][nongen][N]] was  first  [S the first car[56-59]{WO:first}[58][nongen][N][OGEN][geographical urban subfeature]]  manufactured   [O color[54-55][54][nongen][N]]geographical urban subfeatureby   [PO (Ford[61-62][61][ngbus][N][WikiBusiness]) ford]?
/*
What(35)[-500]        are(36)[-500]                      titles(37)[-500]                      of(38)[-500]                   books(39)[-500]                written(40)[-500]           by(41)[-500]                Krugman[PN][OPN](42)[-500]         ?(43)[
                      is PRES_PLURAL                     n P*0                                 prep*0                         n P*0                          v PAST_PART                                                                                ?*0
rel*0                                                                                                                                                                                                                Prop S
_Q2[F](35,43)*-6 rel  _IS[1](36,37)*0 is                 __N1[](37,38)*0 n                     _PP[2](38,40)*0 prep           __N1[](39,40)*0 n              _Q1PASSIVE[1](36,41)*0 v    _PP[2](41,43)*0 prep        __NAME[B](42,43)*0 Prop
                      _Q1PASSIVE[1](36,41)*0 _IS[*](37)  __NOUN[2](37,38)*0 __N1[*](38)        __NOUN[9](37,40)*0 _PP[*](40)  __NOUN[2](39,40)*0 __N1[*](40)                             _Q2[F](35,43)*-6 _PP[*](43) _NAME[1](42,43)*0 __NAME[*](43)
                      _Q2[F](35,43)*-6 _Q1PASSIVE[*](41) __NOUN[9](37,40)*0 __NOUN[*](38)                                     _PP[2](38,40)*0 __NOUN[*](40)                              _META_PP[](41,43)*0 prep    __NOUN[2](42,43)*0 _NAME[*](43)
                                                         _Q1PASSIVE[1](36,41)*0 __NOUN[*](40)                                                                                                                        _PP[2](41,43)*0 __NOUN[*](43)
																												                                                                                                                                                             _META_PP[](41,43)*0 __NOUN[*](43)

What(44)[-500]        is(45)[-500]                       the(46)[-500]                         first(47)[-500]                            computer(48)[-500]             manufactured(49)[-500]     by(50)[-500]                  Apple[PN][OPN](51)[-500]           ?(52)[
                      is PRES_3RD                        det*0                                 ord*0                                      n S                            v PAST PAST_PART           prep*0                        Prop S                             ?*0
rel*0                                                                                                                                                                                                                            
_Q2[F](44,52)*-5 rel  _IS[1](45,46)*0 is                 __NOUN[2](46,49)*0 det                __ADJECTIVE[2](47,48)*0 ord                __N1[](48,49)*0 n              _Q1PASSIVE[1](45,50)*0 v   _PP[2](50,52)*0 prep          __NAME[B](51,52)*0 Prop
                      _Q1PASSIVE[1](45,50)*0 _IS[*](46)  _Q1PASSIVE[1](45,50)*0 __NOUN[*](49)  _ADJECTIVE[2](47,48)*0 __ADJECTIVE[*](48)  __NOUN[2](46,49)*0 __N1[*](49)                            _Q2[F](44,52)*-5 _PP[*](52)   _NAME[1](51,52)*0 __NAME[*](52)
                      _Q2[F](44,52)*-5 _Q1PASSIVE[*](50)                                       __NOUN[2](46,49)*0 _ADJECTIVE[*](48)                                                                 _META_PP[](50,52)*0 prep      __NOUN[2](51,52)*0 _NAME[*](52)
											                                                                                                                                                                                                            _PP[2](50,52)*0 __NOUN[*](52)
                                                                                                                                                                                                                                  _META_PP[](50,52)*0 __NOUN[*](52)

What(53)[-222]         color(54)[-222]                     was(55)[-222]                         the(56)[-222]                               first(57)[-222]                            car(58)[-222]
                       n S*0                               is PAST                               det*0                                       ord*0
                                                                                                                                                                                        n S*0
rel*0
_Q2[J](53,62)*-3 rel   __N1[](54,55)*0 n                   _IS[2](55,56)*0 is                    __NOUN[2](56,59)*0 det                      __ADJECTIVE[2](57,58)*0 ord                __N1[](58,59)*0 n
_RELQ[](53,62)*-3 rel  __NOUN[2](54,55)*0 __N1[*](55)      _VERBPAST[](55,56)*0 is               _Q1PASSIVE[1](55,60)*0 __NOUN[*](59)        _ADJECTIVE[2](57,58)*0 __ADJECTIVE[*](58)  __NOUN[2](56,59)*0 __N1[*](59)
                       _Q2[J](53,62)*-3 __NOUN[*](55)      __ALLVERB[](55,56)*0 _VERBPAST[*](56) __S1[1](54,62)*0 __ALLOBJECTS_1[*](62)      __NOUN[2](56,59)*0 _ADJECTIVE[*](58)
                       __C1__S1[1](54,55)*0 __NOUN[*](55)  _Q1PASSIVE[1](55,60)*0 _IS[*](56)     __NOUNRU[1](56,62)*2 __NOUN[*](59)
                       __S1[1](54,62)*0 __C1__S1[*](55)    _Q2[J](53,62)*-3 _Q1PASSIVE[*](60)    __ALLOBJECTS_1[1](56,62)*2 __NOUNRU[*](62)
                       _RELQ[](53,62)*-3 __S1[*](62)       __S1[1](54,62)*0 __ALLVERB[*](56)

manufactured(59)[-222]                    by(60)[-222]                     Ford[OPN](61)[-222]                ?(62)[
v PAST PAST_PART                          prep*0                           Prop S
                                                                                                              ?*0
_VERBPASTPART[](59,60)*0 v                _PP[2](60,62)*0 prep             __NAME[B](61,62)*0 Prop
_Q1PASSIVE[1](55,60)*0 v                  _Q2[J](53,62)*-3 _PP[*](62)      _NAME[1](61,62)*0 __NAME[*](62)
__NOUNRU[1](56,62)*2 _VERBPASTPART[*](60) __NOUNRU[1](56,62)*2 _PP[*](62)  __NOUN[2](61,62)*0 _NAME[*](62)
                                          _META_PP[](60,62)*0 prep         _PP[2](60,62)*0 __NOUN[*](62)
                                                                           _META_PP[](60,62)*0 __NOUN[*](62)
*/

// 1. reorder if using Q1PASSIVE matched at _Q2+1 and a by clause
//    a. object of by clause -> subject of subclause [Krugman]
//    b. verb -> verb of subclause       [written]
//    c. take all prepositional objects and subject (titles): the nearest before the verb -> object of subclause [books]
//    d. each answer to the subclause -> subject of main clause 
//    e. _IS [Q1PASSIVE] -> verb of the main clause
//    f. object-> object of main clause [what]
// What are titles of books written by Krugman? (1)
// what     [S titles[37-38][37][nongen][N][PL]]  written   [O what]of   [PO books[39-40][39][nongen][N][PL]]?
// What is the first computer manufactured by Apple? (1)
// What are titles of albums featuring Jay-Z?
void cQuestionAnswering::detectByClausePassive(Source *questionSource,vector <cSpaceRelation>::iterator sri,cSpaceRelation * &ssri)
{ LFS
		//cSpaceRelation(int _where,int _o,int _whereControllingEntity,int _whereSubject,int _whereVerb,int _wherePrep,int _whereObject,
		//             int _wherePrepObject,int _movingRelativeTo,int _relationType,
		//							bool _genderedEntityMove,bool _genderedLocationRelation,int _objectSubType,int _prepObjectSubType,bool _physicalRelation)
	ssri=&(*sri);
	// detect Q1PASSIVE
	int maxEnd;
	if (sri->whereQuestionType<0)
		return;
	if (questionSource->queryPattern(sri->whereQuestionType,L"_Q2",maxEnd)!=-1 && questionSource->queryPattern(sri->whereQuestionType+1,L"_Q1PASSIVE",maxEnd)!=-1)
	{
		set <int> relPreps;
		questionSource->getAllPreps(&(*sri),relPreps);
		int subclausePrep=-1,nearestObject=-1;
		if (sri->whereSubject<sri->whereVerb)
			nearestObject=sri->whereSubject;
		for (set <int>::iterator rp=relPreps.begin(),rpEnd=relPreps.end(); rp!=rpEnd; rp++)
		{
			lplog(LOG_WHERE,L"%d:SUBST %d nearestObject=%d whereVerb=%d",sri->where,*rp,nearestObject,sri->whereVerb);
			if (questionSource->m[*rp].getRelObject()>0 && nearestObject<*rp && *rp<sri->whereVerb)
				nearestObject= questionSource->m[*rp].getRelObject();
			if (questionSource->m[*rp].word->first==L"by")
				subclausePrep=*rp;
		}
		if (subclausePrep<0)
			return;
		ssri=new cSpaceRelation(sri->where, questionSource->m[sri->where].getObject(),-1, questionSource->m[subclausePrep].getRelObject(),sri->whereVerb, questionSource->m[subclausePrep].relPrep,nearestObject,(questionSource->m[subclausePrep].relPrep>=0) ? questionSource->m[questionSource->m[subclausePrep].relPrep].getRelObject() : -1,-1,stNORELATION,false,false,-1,-1,false);
		ssri->whereQuestionType=nearestObject;
		ssri->whereQuestionTypeObject = sri->whereQuestionTypeObject;
		ssri->questionType=sri->questionType|QTAFlag;
		ssri->isConstructedRelative=true;
		ssri->changeStateAdverb=false;
		ssri->whereQuestionInformationSourceObjects=sri->whereQuestionInformationSourceObjects;
		questionSource->printSRI(L"SUBSTITUTE",ssri,0,ssri->whereSubject,ssri->whereObject,L"",false,-1,L"");
	}
	// What are titles of albums featuring Jay-Z? --> what albums featured Jay-Z?
	int verbPhraseElement=-1;
	if (questionSource->queryPattern(sri->whereQuestionType,L"__SQ",maxEnd)!=-1 && sri->whereSubject>=0 && questionSource->m[sri->whereSubject].getRelVerb()>=0 && questionSource->m[questionSource->m[sri->whereSubject].getRelVerb()].queryWinnerForm(isForm)>=0 &&
		  (verbPhraseElement= questionSource->m[sri->whereSubject].pma.queryPatternDiff(L"__NOUN",L"F"))!=-1)
	{
		vector < vector <tTagLocation> > tagSets;
		questionSource->startCollectTags(false, subjectVerbRelationTagSet, sri->whereSubject, questionSource->m[sri->whereSubject].pma[verbPhraseElement&~matchElement::patternFlag].pemaByPatternEnd, tagSets, true, true,L"passive clause detection");
		// not reachable?
			//for (unsigned int J=0; J<tagSets.size(); J++)
			//{
			//	printTagSet(LOG_WHERE,L"QR",J,tagSets[J],sri->whereSubject,m[sri->whereSubject].pma[verbPhraseElement&~matchElement::patternFlag].pemaByPatternEnd);
			//	int nextSubjectTag=-1,whereSubjectTag=findTag(tagSets[J],L"SUBJECT",nextSubjectTag);
			//	int nextVerbTag=-1,whereVerbTag=findTag(tagSets[J],L"VERB",nextVerbTag);
			//	int nextObjectTag=-1,whereObjectTag=findTag(tagSets[J],L"OBJECT",nextObjectTag);
			//	if (whereSubjectTag>=0 && whereVerbTag>=0 && whereObjectTag>=0)
			//	{
			//		int whereSubject=tagSets[J][whereSubjectTag].sourcePosition;
			//		if ((m[whereSubject].getMainEntry()->first==L"name" || m[whereSubject].getMainEntry()->first==L"title") &&
			//			  m[whereSubject].relPrep>=0 && m[m[whereSubject].relPrep].word->first==L"of" &&
			//				m[m[whereSubject].relPrep].getRelObject()>=0 && m[m[whereSubject].relPrep].getRelObject()>whereSubject)
			//			whereSubject=m[m[whereSubject].relPrep].getRelObject();
			//		//cSpaceRelation(int _where,int _o,int _whereControllingEntity,int _whereSubject,int _whereVerb,int _wherePrep,int _whereObject,
			//		//             int _wherePrepObject,int _movingRelativeTo,int _relationType,
			//		//							bool _genderedEntityMove,bool _genderedLocationRelation,int _objectSubType,int _prepObjectSubType,bool _physicalRelation)
			//		ssri=new cSpaceRelation(whereSubject,m[whereSubject].getObject(),-1,whereSubject,tagSets[J][whereVerbTag].sourcePosition,-1,tagSets[J][whereObjectTag].sourcePosition,-1,-1,stNORELATION,false,false,-1,-1,false);
			//		ssri->whereQuestionType=whereSubject;
			//		ssri->questionType=sri->questionType|QTAFlag;
			//		ssri->isConstructedRelative=true;
			//		ssri->changeStateAdverb=false;
			//		ssri->whereQuestionInformationSourceObjects=sri->whereQuestionInformationSourceObjects;
			//		printSRI(L"SUBSTITUTE",ssri,0,ssri->whereSubject,ssri->whereObject,L"",false,-1,L"");
			//	}
			//	return;
			//}
	}
}

int Source::detectAttachedPhrase(vector <cSpaceRelation>::iterator sri,int &relVerb)
{ LFS
	int collectionWhere=sri->whereQuestionTypeObject;
	if (m[collectionWhere].beginObjectPosition>=0 && m[m[collectionWhere].beginObjectPosition].pma.queryPatternDiff(L"__NOUN",L"F")!=-1 && (relVerb=m[collectionWhere].getRelVerb())>=0 && m[relVerb].relSubject==collectionWhere)
		return 0;
	return -1;
}

//   A. Which old man who ran in the Olympics won the prize? [Jenkins ran in what?] candidate answer Jenkins what==Olympics
//   B. Which prize originating in Spain did he win? [The prize originated in where? (the verb tense matches the main verb tense)] where=Spain
// 1. detect subqueries (after each object at whereQuestionType, detect a infinitive or a relative clause)
//      A. who ran in the Olympics
//      B. originating in Spain
// 2. collect subqueries
void cQuestionAnswering::detectSubQueries(Source *questionSource, vector <cSpaceRelation>::iterator sri,vector <cSpaceRelation> &subQueries)
{ LFS
	//A. Which old man who ran in the Olympics won the prize?
	// B. Which prize originating in Spain did he win? [The prize originated in where? (the verb tense matches the main verb tense)] where=Spain
	int collectionWhere = sri->whereQuestionTypeObject;
	if (collectionWhere<0) return;
	int rh= questionSource->m[collectionWhere].endObjectPosition,relVerb,relPrep,relObject;
	if (rh>=0 && (((questionSource->m[rh].flags&WordMatch::flagRelativeHead) && (relVerb= questionSource->m[rh].getRelVerb())>=0) || questionSource->detectAttachedPhrase(sri,relVerb)>=0))
	{
		questionSource->printSRI(L"[collecting subqueries of]:",&(*sri),0,sri->whereSubject,sri->whereObject,sri->wherePrep,false,-1,L"");
		// old man ran in the Olympics
		// prize originating in Spain
		relPrep= questionSource->m[relVerb].relPrep;
		questionSource->m.push_back(questionSource->m[relVerb]);
		//adjustOffsets(relVerb); // not applicable
		relVerb= questionSource->m.size()-1;
		questionSource->m[relVerb].verbSense= questionSource->m[sri->whereVerb].verbSense;
		questionSource->m[relVerb].setQuoteForwardLink(questionSource->m[sri->whereVerb].getQuoteForwardLink());
		questionSource->m[relVerb].word= questionSource->getTense(questionSource->m[relVerb].word, questionSource->m[collectionWhere].word, questionSource->m[sri->whereVerb].word->second.inflectionFlags);
		//cSpaceRelation(int _where, int _o, int _whereControllingEntity, int _whereSubject, int _whereVerb, int _wherePrep, int _whereObject,
		//	int _wherePrepObject, int _movingRelativeTo, int _relationType,
		//	bool _genderedEntityMove, bool _genderedLocationRelation, int _objectSubType, int _prepObjectSubType, bool _physicalRelation)
		cSpaceRelation csr(collectionWhere, questionSource->m[collectionWhere].getObject(),-1,collectionWhere,relVerb,relPrep,relObject= questionSource->m[relVerb].getRelObject(),(relPrep>=0) ? questionSource->m[relPrep].getRelObject() : -1,-1,-1,false,false,-1,-1,false);
		csr.questionType=unknownQTFlag;
		int whereQuestionTypeFlags;
		if (relPrep>=0 && questionSource->m[relPrep].getRelObject()>=0)
			questionSource->testQuestionType(questionSource->m[relPrep].getRelObject(),csr.whereQuestionType,whereQuestionTypeFlags,prepObjectQTFlag,csr.whereQuestionInformationSourceObjects);
		questionSource->testQuestionType(relObject,csr.whereQuestionType,whereQuestionTypeFlags, objectQTFlag,csr.whereQuestionInformationSourceObjects);
		if (relObject>=0)
			questionSource->testQuestionType(questionSource->m[relObject].relNextObject,csr.whereQuestionType,whereQuestionTypeFlags, secondaryObjectQTFlag,csr.whereQuestionInformationSourceObjects);
		questionSource->testQuestionType(questionSource->m[rh].getRelVerb() - 1, csr.whereQuestionType, whereQuestionTypeFlags, 0, csr.whereQuestionInformationSourceObjects);
		questionSource->testQuestionType(questionSource->m[rh].getRelVerb() - 2, csr.whereQuestionType, whereQuestionTypeFlags, 0, csr.whereQuestionInformationSourceObjects);
		if (csr.whereQuestionInformationSourceObjects.size()>0)
			csr.whereQuestionType=*csr.whereQuestionInformationSourceObjects.begin();
		wstring ps;
		questionSource->prepPhraseToString(csr.wherePrep,ps);
		questionSource->printSRI(L"collected subquery:",&csr,0,csr.whereSubject,csr.whereObject,ps,false,-1,L"");
		subQueries.push_back(csr);
	}
}

/*
From dbPedia:
Paul Robin Krugman is an American economist, professor of Economics and International Affairs at the Woodrow Wilson School of Public and International Affairs at Princeton University
From wikipedia:
He taught at Yale University, MIT, UC Berkeley, the London School of Economics, and Stanford University before joining Princeton University in 2000 as professor of economics and international affairs.
*/
void cQuestionAnswering::matchAnswersToQuestionType(Source *questionSource,cSpaceRelation*  sri,vector < cAS > &answerSRIs,int maxAnswer,vector <cSpaceRelation> &subQueries,
	   vector <int> &uniqueAnswers,vector <int> &uniqueAnswersPopularity,vector <int> &uniqueAnswersConfidence,
		 int &highestPopularity,int &lowestConfidence,int &lowestSourceConfidence,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,bool useParallelQuery)
{ LFS
	while (maxAnswer>-1)
	{
		lplog(LOG_WHERE,L"(maxCertainty=%d):",maxAnswer);
		lowestConfidence=1000000;
		highestPopularity=-1;
		lowestSourceConfidence=1000000;
		int maxAlternativeAnswer=-1;
		for (unsigned int I=0; I<answerSRIs.size(); I++)
			if (maxAnswer==answerSRIs[I].matchSum)
			{
				vector <cAS>::iterator as=answerSRIs.begin()+I;
				int popularity=1;
				bool identical=false;
				for (unsigned int J=0; J<uniqueAnswers.size() && !identical; J++)
				{
					if (identical=checkIdentical(questionSource,&(*sri),answerSRIs[uniqueAnswers[J]],answerSRIs[I]))
					{
						uniqueAnswersPopularity[J]++;
						highestPopularity=max(highestPopularity,uniqueAnswersPopularity[J]);
						as->identityWith=J;
						if (answerSRIs[uniqueAnswers[J]].equivalenceClass!=answerSRIs[I].equivalenceClass) 
						{
							identical=false;
							lplog(LOG_WHERE,L"Identical to [%d] (EQ %d!=%d)",J,answerSRIs[uniqueAnswers[J]].equivalenceClass,answerSRIs[I].equivalenceClass);
							break;
						}
						else
							lplog(LOG_WHERE,L"Identical to [%d]",J);
					}
				}
				bool tenseMismatch = false, subQueryNoMatch = false, unableToDoQuestionTypeCheck=true;
				int semanticMismatch=0;
				wchar_t derivation[1024];
				StringCbPrintf(derivation,1024*sizeof(wchar_t),L"PW %06d:child answerSRI %d",sri->where,answerSRIs[I].sri->where); 
				answerSRIs[I].confidence = CONFIDENCE_NOMATCH;
				if (!identical && answerSRIs[I].matchSum > 16)
				{
					setWhereChildCandidateAnswer(questionSource,answerSRIs[I], &(*sri));
					answerSRIs[I].confidence = questionTypeCheck(questionSource, derivation, &(*sri), answerSRIs[I], semanticMismatch, unableToDoQuestionTypeCheck, subQueryNoMatch, subQueries, I, mapPatternAnswer, mapPatternQuestion,useParallelQuery);
					verbTenseMatch(questionSource, &(*sri), answerSRIs[I], tenseMismatch);
					// if questionTypeCheck said:
					// CONFIDENCE_NOMATCH: no chance this could be the answer
					// 1: definitely matches the type of the answer
					// !QTAFlag - no semantic match to a where or who because where or who has nothing it is modifying so questionTypeCheck has done all the work possible
					if ((sri->questionType&QTAFlag) && (unableToDoQuestionTypeCheck || (answerSRIs[I].confidence != CONFIDENCE_NOMATCH && (answerSRIs[I].confidence != 1 || sri->questionType == unknownQTFlag))))
						answerSRIs[I].confidence = semanticMatch(questionSource, derivation, &(*sri), answerSRIs[I], semanticMismatch, subQueryNoMatch, subQueries, I, mapPatternAnswer, mapPatternQuestion,useParallelQuery);
					else
						if (unableToDoQuestionTypeCheck)
							answerSRIs[I].confidence = CONFIDENCE_NOMATCH / 2; // unable to do any semantic checking
					if (isModifiedGeneric(answerSRIs[I]))
						answerSRIs[I].confidence = CONFIDENCE_NOMATCH / 2;
				}
				if (answerSRIs[I].confidence < CONFIDENCE_NOMATCH && (!tenseMismatch || answerSRIs[I].confidence==1 || answerSRIs[I].matchSum>20) && !semanticMismatch && !subQueryNoMatch)
				{
					if (tenseMismatch)
						answerSRIs[I].confidence = CONFIDENCE_NOMATCH / 2;
					lplog(LOG_WHERE, L"ANSWER %d:Equivalence Class:%d:Source:%s", I, as->equivalenceClass, as->sourceType.c_str());
					uniqueAnswers.push_back(I);
					uniqueAnswersPopularity.push_back(popularity);
					uniqueAnswersConfidence.push_back(answerSRIs[I].confidence);
					lowestConfidence = min(lowestConfidence, answerSRIs[I].confidence);
					lowestSourceConfidence = min(lowestSourceConfidence, as->source->sourceConfidence);
				}
				else
				{
					lplog(LOG_WHERE, L"    REJECTED %d[%s%s && (%s || %s || confidence=%d) && %s && %s && %s]:Equivalence Class:%d:Source:%s", I,
						(answerSRIs[I].matchSum <= 16) ? L"matchSum too low " : L"",
						(identical) ? L"identical" : L"not identical",
						(sri->questionType&QTAFlag) ? L"adjectival" : L"not adjectival",
						(sri->questionType == unknownQTFlag) ? L"unknown QT" : L"known QT",
						answerSRIs[I].confidence,
						(tenseMismatch) ? L"tense Mismatch" : L"tense match",
						(semanticMismatch) ? L"semantic Mismatch" : L"semantic match",
						(subQueryNoMatch) ? L"subQuery NoMatch" : L"subQuery match",
						as->equivalenceClass, as->sourceType.c_str());
					if (uniqueAnswers.empty())
					{
						as->source->printSRI(L"REJECTED[SRI] ", as->sri, 0, as->ws, as->wo, as->wp, false, as->matchSum, as->matchInfo, LOG_WHERE);
						answerSRIs[I].matchSum = 0;
						maxAnswer = -1;
						for (unsigned int J = 0; J<answerSRIs.size(); J++)
							maxAnswer = max(maxAnswer, answerSRIs[J].matchSum);
						maxAlternativeAnswer = maxAnswer;
						I = 0;
						if (maxAnswer == 0) break;
						continue;
					}
				}
				as->source->printSRI(L"    ",as->sri,0,as->ws,as->wo,as->wp,false,as->matchSum,as->matchInfo,LOG_WHERE);
			}
			else 
			{
				if (maxAnswer>answerSRIs[I].matchSum)
					maxAlternativeAnswer=max(maxAlternativeAnswer,answerSRIs[I].matchSum);
				lplog(LOG_WHERE,L"OTHER %d[%d<%d]:Equivalence Class:%d:Source:%s [%s]",I,answerSRIs[I].matchSum,maxAnswer,answerSRIs[I].equivalenceClass,answerSRIs[I].sourceType.c_str(),answerSRIs[I].source->sourcePath.c_str());
				wstring ps;
				answerSRIs[I].source->prepPhraseToString(answerSRIs[I].wp,ps);
				answerSRIs[I].source->printSRI(L"    ",answerSRIs[I].sri,0,answerSRIs[I].ws,answerSRIs[I].wo,ps,false,answerSRIs[I].matchSum,answerSRIs[I].matchInfo);
			}
		if (uniqueAnswers.size()>0 || maxAlternativeAnswer<16 || maxAnswer==0) // even though answers are considered until 14, if there has already been a higher round, break.
			break;
		maxAnswer=maxAlternativeAnswer;
	}
}

bool cQuestionAnswering::isModifiedGeneric(cAS &as)
{
	if (as.sri!=0 && as.sri->whereChildCandidateAnswer >= 0 && as.source->m[as.sri->whereChildCandidateAnswer].objectMatches.empty() &&
		as.source->m[as.sri->whereChildCandidateAnswer].beginObjectPosition>=0 && as.source->m[as.source->m[as.sri->whereChildCandidateAnswer].beginObjectPosition].word->first == L"a")
		return true;
	return false;
}

int cQuestionAnswering::printAnswers(cSpaceRelation*  sri,vector < cAS > &answerSRIs,vector <int> &uniqueAnswers,vector <int> &uniqueAnswersPopularity,vector <int> &uniqueAnswersConfidence,
	                        int highestPopularity,int lowestConfidence,int lowestSourceConfidence)
{ LFS
	bool finalAnswerListed=false;
	int numWords;
	if (uniqueAnswers.size()>0)
	{
		bool highCertaintyAnswer=false;
		for (unsigned int J=0; J<uniqueAnswers.size(); J++)
			highCertaintyAnswer |= uniqueAnswersPopularity[J] == highestPopularity && uniqueAnswersConfidence[J] == lowestConfidence && 
			  answerSRIs[uniqueAnswers[J]].source->sourceConfidence == lowestSourceConfidence && lowestConfidence<CONFIDENCE_NOMATCH/2 && 
			  (!answerSRIs[uniqueAnswers[J]].subQueryExisted || answerSRIs[uniqueAnswers[J]].subQueryMatch) &&
				!isModifiedGeneric(answerSRIs[uniqueAnswers[J]]);
		lplog(LOG_WHERE,L"P%06d:ANSWER LIST [%d-%d%s]  ************************************************************",sri->where,0,uniqueAnswers.size(),(highCertaintyAnswer) ? L" high certainty found":L"");
		for (unsigned int J=0; J<uniqueAnswers.size(); J++)
		{
			vector <cAS>::iterator as=answerSRIs.begin()+uniqueAnswers[J];
			if (as->finalAnswer=(highCertaintyAnswer && as->source->sourceConfidence==lowestSourceConfidence &&
						uniqueAnswersPopularity[J]==highestPopularity && uniqueAnswersConfidence[J]==lowestConfidence) ||
					(!highCertaintyAnswer && (uniqueAnswersPopularity[J]==highestPopularity || uniqueAnswersConfidence[J]==lowestConfidence)))
			{
				lplog(LOG_WHERE|LOG_QCHECK,L"    ANSWER %d:Popularity:%d:Object Match Confidence:%d:Source:%s:Source Confidence:%d",J,uniqueAnswersPopularity[J],uniqueAnswersConfidence[J],as->sourceType.c_str(),as->source->sourceConfidence);
				if (as->sri)
				{
					wstring ps;
					as->source->prepPhraseToString(as->wp,ps);
					as->source->printSRI(L"      ",as->sri,0,as->ws,as->wo,ps,false,as->matchSum,as->matchInfo,LOG_WHERE|LOG_QCHECK);
				}
				else
				{
					wstring tmpstr;
					lplog(LOG_WHERE|LOG_QCHECK,L"    TABLE %d:%s",as->ws,as->source->whereString(as->ws,tmpstr,false,6,L" ",numWords).c_str());
				}
				if (uniqueAnswersConfidence[J]<CONFIDENCE_NOMATCH/2) // tenseMismatch sets confidence at CONFIDENCE_NOMATCH/2 - look for other answers
					finalAnswerListed = uniqueAnswersConfidence[J] >= 0;
			}
		}
		for (unsigned int J=0; J<uniqueAnswers.size(); J++)
		{
			vector <cAS>::iterator as=answerSRIs.begin()+uniqueAnswers[J];
			if (!as->finalAnswer)
			{
				if (as->identityWith>=0)
				{
					bool identicalWithFinalAnswer=false;
					for (unsigned int K=0; K<uniqueAnswers.size() && !identicalWithFinalAnswer; K++)
						identicalWithFinalAnswer=((answerSRIs[uniqueAnswers[K]].identityWith==as->identityWith || as->identityWith==K) && answerSRIs[uniqueAnswers[K]].finalAnswer);
					if (identicalWithFinalAnswer)
						continue;
				}
				wstring tmpstr1,tmpstr2;
				if (uniqueAnswersPopularity[J]<highestPopularity) as->rejectAnswer=L"low popularity "+itos(uniqueAnswersPopularity[J],tmpstr1)+L"<"+itos(highestPopularity,tmpstr2)+L" ";
				if (uniqueAnswersConfidence[J]>lowestConfidence) as->rejectAnswer+=L"confidence "+itos(uniqueAnswersConfidence[J],tmpstr1)+L">"+itos(lowestConfidence,tmpstr2)+L" ";
				if (as->rejectAnswer.empty())
					as->rejectAnswer=L"source confidence "+itos(answerSRIs[uniqueAnswers[J]].source->sourceConfidence,tmpstr1)+L">"+itos(lowestSourceConfidence,tmpstr2)+L" ";
				lplog(LOG_WHERE,L"  REJECTED (%s) %d:Popularity:%d:Object Match Confidence:%d:Source:%s:Source Confidence:%d",
					as->rejectAnswer.c_str(),J,uniqueAnswersPopularity[J],uniqueAnswersConfidence[J],as->sourceType.c_str(),as->source->sourceConfidence);
				wstring ps;
				as->source->prepPhraseToString(as->wp,ps);
				as->source->printSRI(L"      ",as->sri,0,as->ws,as->wo,ps,false,as->matchSum,as->matchInfo);
			}
		}
	}
	else
		lplog(LOG_WHERE, L"P%06d:ANSWER LIST EMPTY! lowest confidence %d out of %d answers ************************************************************", sri->where, lowestConfidence, answerSRIs.size());
	if (!finalAnswerListed && (sri->questionType&QTAFlag))
		return -1;
	return uniqueAnswers.empty() ? -1 : 0;
}

int	cQuestionAnswering::searchTableForAnswer(Source *questionSource,wchar_t derivation[1024],cSpaceRelation* sri, unordered_map <int,WikipediaTableCandidateAnswers * > &wikiTableMap,
	                               vector <cSpaceRelation> &subQueries,vector < cAS > &answerSRIs,int &minConfidence,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,bool useParallelQuery)
{ LFS
	// look for the questionType object and its synonyms in the freebase properties of the questionInformationSourceObject.
	int whereQuestionTypeObject=sri->whereQuestionTypeObject,numConsideredParentAnswer=0;
	if (whereQuestionTypeObject<0) return -1;
	wstring tmpstr;
	int numTableAttempts=0,numWords;
	unordered_map <int,WikipediaTableCandidateAnswers * > ::iterator wtmi;
	for (set <int>::iterator si=sri->whereQuestionInformationSourceObjects.begin(),siEnd=sri->whereQuestionInformationSourceObjects.end(); si!=siEnd; si++)
	{
		if ((wtmi=wikiTableMap.find(*si))!=wikiTableMap.end())
		{
			// if found, for each value of the property:
			for (vector < SourceTable >::iterator wtvi=wtmi->second->wikiQuestionTypeObjectAnswers.begin(),wtviEnd=wtmi->second->wikiQuestionTypeObjectAnswers.end(); wtvi!=wtviEnd; wtvi++)
			{
				numTableAttempts+=wtvi->columns.size();
			}
		}
		if (logTableDetail)
			lplog(LOG_WHERE,L"*No tables for answer matching %s:", questionSource->whereString(*si,tmpstr,true).c_str());
	}
	if (!numTableAttempts) 
	{
		for (unordered_map <int,WikipediaTableCandidateAnswers * >::iterator wm=wikiTableMap.begin(),wmEnd=wikiTableMap.end(); wm!=wmEnd; wm++)
			lplog(LOG_WHERE,L"*No answers for table mapping to whereQuestionInformationSourceObject %s", questionSource->whereString(wm->first,tmpstr,true).c_str());
		return -1;
	}
	lplog(LOG_WHERE,L"*Searching %d tables for answer:",numTableAttempts);
	wstring ps;
	questionSource->prepPhraseToString(sri->wherePrep,ps);
	questionSource->printSRI(L"",&(*sri),0,sri->whereSubject,sri->whereObject,ps,false,-1,L"");
	int numTableAttemptsTotal=numTableAttempts,lastProgressPercent=0,startTime=clock();
	numTableAttempts=0;
	// which prize did Paul Krugman win?
	// if no answers and whereQuestionType is adjectival
	// for each of the questionInformationSourceObjects:
	for (set <int>::iterator si=sri->whereQuestionInformationSourceObjects.begin(),siEnd=sri->whereQuestionInformationSourceObjects.end(); si!=siEnd; si++)
	{
		if ((wtmi=wikiTableMap.find(*si))!=wikiTableMap.end())
		{
			// if found, for each table in source:
			for (vector < SourceTable >::iterator wtvi=wtmi->second->wikiQuestionTypeObjectAnswers.begin(),wtviEnd=wtmi->second->wikiQuestionTypeObjectAnswers.end(); wtvi!=wtviEnd; wtvi++)
			{
				int answersFromOneEntry=0,whereLastEntryEnd=-1;
				wstring tableTitle = wtmi->second->wikipediaSource->phraseString(wtvi->tableTitleEntry.begin, wtvi->tableTitleEntry.begin+ wtvi->tableTitleEntry.numWords,tmpstr,false);
				wtvi->tableTitleEntry.logEntry(LOG_WHERE, wtvi->num.c_str(),-1,-1, wtvi->source);
				// for each column in table
				for (vector <Column>::iterator wtci = wtvi->columns.begin(), wtciEnd = wtvi->columns.end(); wtci != wtciEnd; wtci++)
				{
					if (wtci->coherencyPercentage < 50)
					{
						wtci->logColumn(LOG_WHERE, L"REJECTED - COHERENCY", tableTitle.c_str());
						continue;
					}
					wtci->logColumn(LOG_WHERE,L"INITIALIZE", tableTitle);
					for (vector < vector <Column::Entry> >::iterator wtri = wtci->rows.begin(), wtriEnd = wtci->rows.end(); wtri != wtriEnd; wtri++)
					{
						if (wtri == wtci->rows.begin())
						{
							continue; // skip header of column
						}
						for (vector <Column::Entry>::iterator wti = wtri->begin(), wtiEnd = wtri->end(); wti != wtiEnd; wti++)
						{
							wstring tmpstr1, tmpstr2, tmpstr3;
							int whereChildCandidateAnswer = wti->begin;
							whereLastEntryEnd = wti->begin + wti->numWords;
							if (whereChildCandidateAnswer < 0)
							{
								answersFromOneEntry = 0;
								// Fundación Príncipe de Asturias (Spain), Prince of Asturias Awards in Social Sciences
								whereLastEntryEnd = -1; // accept entries that are restatements of the previous entry, so no words are accepted between the entries (except punctuation)
								continue;
							}
							answersFromOneEntry++;
							if ((int)(++numTableAttempts * 100 / numTableAttemptsTotal) > lastProgressPercent)
							{
								lastProgressPercent = (int)numTableAttempts * 100 / numTableAttemptsTotal;
								wprintf(L"PROGRESS: %03d%% table targets with %d seconds elapsed \r", lastProgressPercent, (int)((clock() - startTime) / CLOCKS_PER_SEC));
							}
							bool synonym = false;
							int semanticMismatch = 0;
							lplog(LOG_WHERE, L"processing table %s: Q%d %s semantic comparison START whereChildCandidateAnswer=%d:%s:", 
								wtvi->num.c_str(), numConsideredParentAnswer, (wtvi->tableTitleEntry.lastWordFoundInTitleSynonyms) ? L" [matches question object]":L"",whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str());
							wti->logEntry(LOG_WHERE, wtvi->num.c_str(),wtri-wtci->rows.begin(),wti-wtri->begin(),wtvi->source);
							int confidence = CONFIDENCE_NOMATCH;
							if (wti->lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms && (wtvi->tableTitleEntry.matchedQuestionObject.size() || wtvi->tableTitleEntry.synonymMatchedQuestionObject.size()))
								confidence = (wtvi->tableTitleEntry.matchedQuestionObject.size()) ? 1 : 2;
							else
								confidence = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, whereQuestionTypeObject, wtmi->second->wikipediaSource, whereChildCandidateAnswer, -1, synonym, semanticMismatch);
							lplog(LOG_WHERE, L"processing table %s: Q%d semantic comparison FINISHED between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields confidence %d [%d].", 
								wtvi->num.c_str(), numConsideredParentAnswer++, whereQuestionTypeObject, questionSource->whereString(whereQuestionTypeObject, tmpstr1, false).c_str(), whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str(), confidence, answersFromOneEntry);
							bool secondarySentencePosition = (answersFromOneEntry > 1 && (wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].objectRole&(OBJECT_ROLE | PREP_OBJECT_ROLE)));
							if (secondarySentencePosition)
								confidence += CONFIDENCE_NOMATCH / 2;
							if (confidence < CONFIDENCE_NOMATCH)
							{
								cAS as(L"TABLE", wtmi->second->wikipediaSource, confidence, 1, L"CONFIDENCE_MATCH[" + itos(confidence, tmpstr1) + L"]", NULL, 1, whereChildCandidateAnswer, 0, 0);
								if (subQueries.empty())
								{
									minConfidence = min(minConfidence, confidence);
									answerSRIs.push_back(as);
									continue;
								}
								semanticMismatch = 0;
								bool subQueryNoMatch = false;
								wchar_t sqDerivation[2048];
								StringCbPrintf(sqDerivation, 2048 * sizeof(wchar_t), L"%s whereQuestionInformationSourceObject(%d:%s) wikiQuestionTypeObjectAnswer(%d:%s)", derivation, *si, questionSource->whereString(*si, tmpstr2, true, 6, L" ", numWords).c_str(),
									whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr3, false).c_str());
								// run a subquery which substitutes the value for the object in a part of the sentence.
								matchSubQueries(questionSource, sqDerivation, wtmi->second->wikipediaSource, semanticMismatch, subQueryNoMatch, subQueries, whereChildCandidateAnswer, whereLastEntryEnd, numConsideredParentAnswer, confidence, mapPatternAnswer, mapPatternQuestion,useParallelQuery);
								as.subQueryExisted = subQueries.size() > 0;
								as.subQueryMatch = !subQueryNoMatch;
								if (subQueryNoMatch)
								{
									confidence=as.confidence += 1000;
									as.matchInfo += L"[subQueryNoMatch +1000]";
								}
								if (secondarySentencePosition)
									as.matchInfo += L"[secondarySentencePosition[" + itos(answersFromOneEntry, tmpstr) + L"+" + itos(CONFIDENCE_NOMATCH / 2, tmpstr2) + L"]";
								lplog(LOG_WHERE, L"%d:table subquery comparison between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d[%s]", numConsideredParentAnswer - 1,
									whereQuestionTypeObject, questionSource->whereString(whereQuestionTypeObject, tmpstr1, false).c_str(), whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str(),
									semanticMismatch, (subQueryNoMatch) ? L"true" : L"false", confidence, as.matchInfo.c_str());
								minConfidence = min(minConfidence, confidence);
								answerSRIs.push_back(as);
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

int cQuestionAnswering::findConstrainedAnswers(Source *questionSource, vector < cAS > &answerSRIs,vector <int> &wherePossibleAnswers)
{ LFS
	int whereChild=-1;
	set <wstring> constrainedAnswers;
	for (vector < cAS >::iterator as=answerSRIs.begin(),asEnd=answerSRIs.end(); as!=asEnd; as++)
	{
		if (as->finalAnswer)
		{
			wstring tmpstr;
			cAS *transferAS=0;
			if (as->sri)
			{
				if (as->sri->whereChildCandidateAnswer>=0)
				{
					as->source->whereString(whereChild=as->sri->whereChildCandidateAnswer,tmpstr,false);
					if (constrainedAnswers.find(tmpstr)==constrainedAnswers.end())
					{
						lplog(LOG_WHERE|LOG_QCHECK,L"    CONSTRAINED ANSWER Source:%s:Source Confidence:%d: %s",as->sourceType.c_str(),as->source->sourceConfidence,tmpstr.c_str());
						transferAS=&(*as);
						constrainedAnswers.insert(tmpstr);
					}
					else
						lplog(LOG_WHERE|LOG_QCHECK,L"    CONSTRAINED DUPLICATE Source:%s:Source Confidence:%d: %s",as->sourceType.c_str(),as->source->sourceConfidence,tmpstr.c_str());
				}
				else
					lplog(LOG_WHERE|LOG_QCHECK,L"    NO CONSTRAINED ANSWER available.");
			}
			else
			{
				int numWords;
				as->source->whereString(whereChild=as->ws,tmpstr,false,6,L" ",numWords);
				if (constrainedAnswers.find(tmpstr)==constrainedAnswers.end())
				{
					lplog(LOG_WHERE|LOG_QCHECK,L"    CONSTRAINED TABLE ANSWER %d:%s",as->ws,tmpstr.c_str());
					transferAS= &(*as);
				}
				else
					lplog(LOG_WHERE|LOG_QCHECK,L"    CONSTRAINED TABLE DUPLICATE %d:%s",as->ws,tmpstr.c_str());
			}
			if (transferAS)
				wherePossibleAnswers.push_back(questionSource->copyChildIntoParent(transferAS->source,whereChild));
		}
	}
	return wherePossibleAnswers.size();
}

bool Source::compareObjectString(int whereObject1,int whereObject2)
{ LFS
	wstring whereObject1Str,whereObject2Str;
	whereString(whereObject1,whereObject1Str,true);
	whereString(whereObject2,whereObject2Str,true);
	return whereObject1Str==whereObject2Str;
}

bool Source::objectContainedIn(int whereObject,set <int> whereObjects)
{ LFS
	for (set <int>::iterator woi=whereObjects.begin(),woiEnd=whereObjects.end(); woi!=woiEnd; woi++)
	{
		if (compareObjectString(whereObject,*woi))
			return true;
		for (unsigned int I=0; I<m[*woi].objectMatches.size(); I++)
			if (compareObjectString(whereObject,objects[m[*woi].objectMatches[I].object].originalLocation))
				return true;
	}
	return false;
}

bool cQuestionAnswering::matchParticularAnswer(Source *questionSource,cSpaceRelation *ssri,int whereMatch,int wherePossibleAnswer,set <int> &addWhereQuestionInformationSourceObjects)
{ LFS
	if (whereMatch<0 || wherePossibleAnswer<0 || questionSource->inObject(whereMatch,ssri->whereQuestionType))
		return false;
	bool legalReference=false,notAlreadySelected=true,objectClass=true;
	int semanticMismatch=0;
	int oc;
	wstring tmpstr,tmpstr2,tmpstr3;
	if ((legalReference=whereMatch>=0 && questionSource->m[wherePossibleAnswer].getObject()>=0) &&
		  (notAlreadySelected=!questionSource->objectContainedIn(wherePossibleAnswer,ssri->whereQuestionInformationSourceObjects) &&
			!questionSource->objectContainedIn(wherePossibleAnswer,addWhereQuestionInformationSourceObjects)) &&
			(objectClass=(oc= questionSource->objects[questionSource->m[wherePossibleAnswer].getObject()].objectClass)==NAME_OBJECT_CLASS || oc==GENDERED_DEMONYM_OBJECT_CLASS || oc==NON_GENDERED_BUSINESS_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS))
		  //(confidenceMatch=checkParticularPartSemanticMatch(LOG_WHERE,whereMatch,this,wherePossibleAnswer,synonym,semanticMismatch)<CONFIDENCE_NOMATCH) && 
			//!semanticMismatch)
	{
		lplog(LOG_WHERE,L"%d:MAOPQ matchParticularAnswer SUCCESS: Pushing object %s to location %d:%s.",
			ssri->where, questionSource->whereString(wherePossibleAnswer,tmpstr,true).c_str(),whereMatch, questionSource->whereString(whereMatch,tmpstr2,true).c_str());
		questionSource->m[whereMatch].objectMatches.push_back(cOM(questionSource->m[wherePossibleAnswer].getObject(),0));
		addWhereQuestionInformationSourceObjects.insert(whereMatch);
		return true;
	}
	else 
		lplog(LOG_WHERE,L"%d:MAOPQ matchParticularAnswer REJECTED (%s%s%s%s):  %d:%s  =  %d:%s?",ssri->where,
		  (legalReference) ? L"":L"illegal",(notAlreadySelected) ? L"":L"alreadySelected",(semanticMismatch) ? L"semanticMismatch":L"",(objectClass) ? L"":L"objectClass",
			whereMatch, questionSource->whereString(whereMatch,tmpstr2,true).c_str(),
			wherePossibleAnswer, questionSource->whereString(wherePossibleAnswer,tmpstr3,true).c_str());
	return false;
}

bool cQuestionAnswering::matchAnswerSourceMatch(Source *questionSource, cSpaceRelation *ssri,int whereMatch,int wherePossibleAnswer,set <int> &addWhereQuestionInformationSourceObjects)
{ LFS
	if (whereMatch<0 || wherePossibleAnswer<0 || questionSource->inObject(whereMatch,ssri->whereQuestionType))
		return false;
	bool legalReference=false,notAlreadySelected=true,wordMatch=true,objectClass=true;
	wstring tmpstr,tmpstr2;
	int oc;
	if ((legalReference=whereMatch>=0 && questionSource->m[wherePossibleAnswer].getObject()>=0) &&
		  (notAlreadySelected=!questionSource->objectContainedIn(wherePossibleAnswer,ssri->whereQuestionInformationSourceObjects) &&
			!questionSource->objectContainedIn(wherePossibleAnswer,addWhereQuestionInformationSourceObjects)) &&
		  (wordMatch= questionSource->m[whereMatch].getMainEntry()== questionSource->m[wherePossibleAnswer].getMainEntry()))
	{
		if (questionSource->m[wherePossibleAnswer].objectMatches.empty() && (objectClass=(oc= questionSource->objects[questionSource->m[wherePossibleAnswer].getObject()].objectClass)==NAME_OBJECT_CLASS || oc==GENDERED_DEMONYM_OBJECT_CLASS || oc==NON_GENDERED_BUSINESS_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS))
		{
			lplog(LOG_WHERE,L"%d:MAOPQ matchAnswerSourceMatch SUCCESS: Pushing object %s to location %d:%s.",ssri->where, questionSource->whereString(wherePossibleAnswer,tmpstr,true).c_str(),whereMatch, questionSource->m[whereMatch].word->first.c_str());
			questionSource->m[whereMatch].objectMatches.push_back(cOM(questionSource->m[wherePossibleAnswer].getObject(),0));
			addWhereQuestionInformationSourceObjects.insert(whereMatch);
		}
		else
			lplog(LOG_WHERE,L"%d:MAOPQ matchAnswerSourceMatch SUCCESS: Matched object %s.",ssri->where, questionSource->whereString(wherePossibleAnswer,tmpstr,true).c_str());
		return true;
	}
	else 
		lplog(LOG_WHERE,L"%d:MAOPQ matchAnswerSourceMatch REJECTED (%s%s%s%s):%d:object=%s %s=%s?",ssri->where,
		  (legalReference) ? L"":L"illegal",(notAlreadySelected) ? L"":L"alreadySelected",(wordMatch) ? L"":L"noWordMatch",(objectClass) ? L"":L"objectClass",
		  whereMatch, questionSource->whereString(wherePossibleAnswer,tmpstr2,true).c_str(), questionSource->m[whereMatch].word->first.c_str(), questionSource->m[wherePossibleAnswer].word->first.c_str());
	return false;
}

int cQuestionAnswering::matchAnswersOfPreviousQuestion(Source *questionSource, cSpaceRelation *ssri,vector <int> &wherePossibleAnswers)
{ LFS
	set <int> addWhereQuestionInformationSourceObjects;
	wstring tmpstr;
	lplog(LOG_WHERE,L"%d:MAOPQ BEGIN matchAnswersOfPreviousQuestion (%s)",ssri->where, questionSource->whereString(wherePossibleAnswers,tmpstr).c_str());
	// match the general object that the answers were linked to
	if (matchAnswerSourceMatch(questionSource, ssri,ssri->whereSubject,wherePossibleAnswers[0],addWhereQuestionInformationSourceObjects))
		for (vector <int>::iterator wpai=wherePossibleAnswers.begin()+1,wpaiEnd=wherePossibleAnswers.end(); wpai!=wpaiEnd; wpai++)
			matchParticularAnswer(questionSource, ssri,ssri->whereSubject,*wpai,addWhereQuestionInformationSourceObjects);
	if (matchAnswerSourceMatch(questionSource, ssri,ssri->whereObject,wherePossibleAnswers[0],addWhereQuestionInformationSourceObjects))
		for (vector <int>::iterator wpai=wherePossibleAnswers.begin()+1,wpaiEnd=wherePossibleAnswers.end(); wpai!=wpaiEnd; wpai++)
			matchParticularAnswer(questionSource, ssri,ssri->whereObject,*wpai,addWhereQuestionInformationSourceObjects);
	if (matchAnswerSourceMatch(questionSource, ssri,ssri->wherePrepObject,wherePossibleAnswers[0],addWhereQuestionInformationSourceObjects))
		for (vector <int>::iterator wpai=wherePossibleAnswers.begin()+1,wpaiEnd=wherePossibleAnswers.end(); wpai!=wpaiEnd; wpai++)
			matchParticularAnswer(questionSource, ssri,ssri->wherePrepObject,*wpai,addWhereQuestionInformationSourceObjects);
	if (addWhereQuestionInformationSourceObjects.size())
	{
		lplog(LOG_WHERE,L"%d:MAOPQ END matchAnswersOfPreviousQuestion adding: %s",ssri->where, questionSource->whereString(addWhereQuestionInformationSourceObjects,tmpstr).c_str());
		ssri->whereQuestionInformationSourceObjects.insert(addWhereQuestionInformationSourceObjects.begin(),addWhereQuestionInformationSourceObjects.end());
	}
	else
		lplog(LOG_WHERE,L"%d:MAOPQ END matchAnswersOfPreviousQuestion no answers added",ssri->where, questionSource->whereString(addWhereQuestionInformationSourceObjects,tmpstr).c_str());
	return -1;
}

extern int limitProcessingForProfiling;
int cQuestionAnswering::searchWebSearchQueries(Source *questionSource,wchar_t derivation[1024],cSpaceRelation* ssri, unordered_map <int,WikipediaTableCandidateAnswers * > &wikiTableMap,
	                               vector <cSpaceRelation> &subQueries,vector < cAS > &answerSRIs,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,
																 vector <wstring> &webSearchQueryStrings,bool parseOnly,bool answerPluralSpecification,int &finalAnswer,int &maxAnswer,bool useParallelQuery)
{ LFS
	bool lastSearchPage=false,googleSearch=true,tablesNotChecked=true;
	for (int trySearchIndex = 1; true; trySearchIndex += 10)
	{
		StringCbPrintf(derivation,1024*sizeof(wchar_t),L"PW %06d",ssri->where);
		int webSearchOffset=0;
		if (useParallelQuery)
			lastSearchPage = webSearchForQueryParallel(questionSource, derivation, ssri, parseOnly, answerSRIs, maxAnswer, 10, trySearchIndex, googleSearch, webSearchQueryStrings, webSearchOffset, mapPatternAnswer, mapPatternQuestion)<10;
		else
			lastSearchPage = webSearchForQuerySerial(questionSource, derivation, ssri, parseOnly, answerSRIs, maxAnswer, 10, trySearchIndex, googleSearch, webSearchQueryStrings, webSearchOffset, mapPatternAnswer, mapPatternQuestion) < 10;
		if (limitProcessingForProfiling)
			return 0;
		vector <int> uniqueAnswers,uniqueAnswersPopularity,uniqueAnswersConfidence;
		int lowestConfidence=100000-1,highestPopularity=-1,lowestSourceConfidence=10000;
		matchAnswersToQuestionType(questionSource, ssri,answerSRIs,maxAnswer,subQueries,uniqueAnswers,uniqueAnswersPopularity,uniqueAnswersConfidence,highestPopularity,lowestConfidence,lowestSourceConfidence,mapPatternAnswer,mapPatternQuestion,useParallelQuery);
		finalAnswer=printAnswers(ssri,answerSRIs,uniqueAnswers,uniqueAnswersPopularity,uniqueAnswersConfidence,highestPopularity,lowestConfidence,lowestSourceConfidence);
		if (finalAnswer >= 0 && (!answerPluralSpecification || uniqueAnswers.size() >= 1))
			break;
		if (tablesNotChecked && (finalAnswer<0 || (answerPluralSpecification && uniqueAnswers.size()<=1)))
		{
			answerSRIs.clear();
			uniqueAnswers.clear();
			uniqueAnswersPopularity.clear();
			uniqueAnswersConfidence.clear();
			if (ssri->whereQuestionInformationSourceObjects.empty())
				lplog(LOG_WHERE,L"*No contextual suggestions for:");
			else
			{
				lowestConfidence=100000-1;
				highestPopularity=1;
				searchTableForAnswer(questionSource, derivation,ssri,wikiTableMap,subQueries,answerSRIs,lowestConfidence,mapPatternAnswer,mapPatternQuestion,useParallelQuery);
				for (unsigned int I=0; I<answerSRIs.size(); I++)
					if (answerSRIs[I].confidence==lowestConfidence)
					{
						uniqueAnswers.push_back(I);
						uniqueAnswersPopularity.push_back(1);
						uniqueAnswersConfidence.push_back(lowestConfidence);
					}
				if ((finalAnswer=printAnswers(ssri,answerSRIs,uniqueAnswers,uniqueAnswersPopularity,uniqueAnswersConfidence,highestPopularity,lowestConfidence,lowestSourceConfidence))>=0)
					break;
			}
			tablesNotChecked=false;
		}
		if (finalAnswer>=0) 
			break;
		answerSRIs.clear();
		if (lastSearchPage || trySearchIndex>40)
		{
			if (!googleSearch)
				break;
			googleSearch=false;
			trySearchIndex=-10;
		}
	}
	return finalAnswer;
}

extern int limitProcessingForProfiling;
int cQuestionAnswering::processQuestionSource(Source *questionSource,bool parseOnly,bool useParallelQuery)
{ LFS
	int lastProgressPercent=-1,where;
	vector <int> wherePossibleAnswers;
	wstring derivation;
	for (vector <cSpaceRelation>::iterator sri= questionSource->spaceRelations.begin(), sriEnd= questionSource->spaceRelations.end(); sri!=sriEnd; sri++)
	{
		if (!sri->questionType || sri->skip)
			continue;
			// memory optimization
		for (unordered_map <wstring, Source *>::iterator smi = sourcesMap.begin(); smi != sourcesMap.end(); )
		{
			Source *source = smi->second;
			source->clearSource();
			if (source->updateWordUsageCostsDynamically)
				WordClass::resetUsagePatternsAndCosts(source->debugTrace);
			else
				WordClass::resetCapitalizationAndProperNounUsageStatistics(source->debugTrace);
			delete source;
			sourcesMap.erase(smi);
			smi = sourcesMap.begin();
		}
		childCandidateAnswerMap.clear();
    if ((where=(sri- questionSource->spaceRelations.begin())*100/ questionSource->spaceRelations.size())>lastProgressPercent)
    {
      wprintf(L"PROGRESS: %03d%% questions processed with %04d seconds elapsed \r",where,clocksec());
      lastProgressPercent=where;
			questionProgress=lastProgressPercent;
    }
		sri->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, &(*sri));
		cSpaceRelation *ssri;
		// For which newspaper does Krugman write?
		detectByClausePassive(questionSource,sri,ssri);
		ssri->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, ssri);
		// detect How and transitory answers
		cPattern *mapPatternAnswer=NULL,*mapPatternQuestion=NULL;
		detectTransitoryAnswer(questionSource,&(*sri),ssri,mapPatternAnswer,mapPatternQuestion);
		wstring ps, parentNum,tmpstr,tmpstr2;
		itos(ssri->where, parentNum);
		//if (ssri->where==69) 
			//logDatabaseDetails = logQuestionProfileTime = logSynonymDetail = logTableDetail = equivalenceLogDetail = logQuestionDetail = logSemanticMap = 1;

		parentNum +=L":Q ";
		questionSource->prepPhraseToString(ssri->wherePrep,ps);
		questionSource->printSRI(parentNum,ssri,-1,ssri->whereSubject,ssri->whereObject,ps,false,-1,L"QUESTION",(ssri->questionType) ? LOG_WHERE|LOG_QCHECK : LOG_WHERE);
		if (wherePossibleAnswers.size())
		{
			matchAnswersOfPreviousQuestion(questionSource,ssri,wherePossibleAnswers);
			wherePossibleAnswers.clear();
		}
		int maxAnswer=-1;
		vector < cAS > answerSRIs;
		unordered_map <int, WikipediaTableCandidateAnswers * > wikiTableMap;
		analyzeRDFTypes(questionSource, sri, ssri, derivation, answerSRIs, maxAnswer, mapPatternAnswer, mapPatternQuestion, wikiTableMap, false);
		wchar_t sqderivation[4096];
		if (ssri->whereQuestionTypeObject < 0)
			continue;
		bool answerPluralSpecification = (questionSource->m[ssri->whereQuestionTypeObject].word->second.inflectionFlags&PLURAL) == PLURAL;
		// detect subqueries
		vector <cSpaceRelation> subQueries;
		if (&(*sri) == ssri)
		{
			questionSource->printSRI(parentNum, &(*sri), 0, sri->whereSubject, sri->whereObject, ps, false, -1, L"");
			detectSubQueries(questionSource, sri, subQueries);
		}
		else
		{
			lplog(LOG_WHERE,L"Transformed:");
			questionSource->printSRI(parentNum,ssri,0,ssri->whereSubject,ssri->whereObject,ps,false,-1,L"");
		}
		int finalAnswer=-1;
		matchOwnershipDbQuery(questionSource, sqderivation,ssri);
		int lastAnswer=answerSRIs.size();
		vector <int> uniqueAnswers,uniqueAnswersPopularity,uniqueAnswersConfidence;
		int lowestConfidence=100000-1,highestPopularity=-1,lowestSourceConfidence=10000;
		if (dbSearchForQuery(questionSource, sqderivation,ssri,answerSRIs))
		{
			lowestSourceConfidence=1;
			for (unsigned int a=lastAnswer; a<answerSRIs.size(); a++)
			{
				answerSRIs[a].source->sourceConfidence=1;
				uniqueAnswers.push_back(a);
				uniqueAnswersPopularity.push_back(highestPopularity=10);
				uniqueAnswersConfidence.push_back(lowestConfidence=1);
			}
		}
		else
			matchAnswersToQuestionType(questionSource,ssri,answerSRIs,maxAnswer,subQueries,uniqueAnswers,uniqueAnswersPopularity,uniqueAnswersConfidence,highestPopularity,lowestConfidence,lowestSourceConfidence,mapPatternAnswer,mapPatternQuestion,useParallelQuery);
		finalAnswer=printAnswers(ssri,answerSRIs,uniqueAnswers,uniqueAnswersPopularity,uniqueAnswersConfidence,highestPopularity,lowestConfidence,lowestSourceConfidence);
		vector < cAS > saveAnswerSRIs;
		vector <wstring> webSearchQueryStrings;
		if (finalAnswer<0 || (answerPluralSpecification && uniqueAnswers.size() <= 1))
		{
			getWebSearchQueries(questionSource, ssri,webSearchQueryStrings);
			searchWebSearchQueries(questionSource, sqderivation,ssri,wikiTableMap,subQueries,saveAnswerSRIs,mapPatternAnswer,mapPatternQuestion,
																webSearchQueryStrings,parseOnly,answerPluralSpecification,finalAnswer,maxAnswer,useParallelQuery);
			if (limitProcessingForProfiling)
				return 0;
			if (saveAnswerSRIs.empty())
			{
				lplog(LOG_WHERE|LOG_QCHECK,L"    *****Trying semantic map.");
				for (set <int>::iterator si=ssri->whereQuestionInformationSourceObjects.begin(),siEnd=ssri->whereQuestionInformationSourceObjects.end(); si!=siEnd; si++)
				{
					unordered_map <int,cSemanticMap *>::iterator msi=ssri->semanticMaps.find(*si);
					if (msi!=ssri->semanticMaps.end())
					{
						msi->second->sortAndCheck(*this,ssri,questionSource);
						msi->second->lplogSM(*this, LOG_WHERE, questionSource,false);
						set < unordered_map <wstring,cSemanticMap::cSemanticEntry>::iterator,cSemanticMap::semanticSetCompare > suggestedAnswers=msi->second->suggestedAnswers;
						for (set < unordered_map <wstring,cSemanticMap::cSemanticEntry>::iterator,cSemanticMap::semanticSetCompare >::iterator sai=suggestedAnswers.begin(),saiEnd=suggestedAnswers.end(); sai!=saiEnd; sai++)
						{
							enhanceWebSearchQueries(webSearchQueryStrings,(*sai)->first);
							searchWebSearchQueries(questionSource, sqderivation,ssri,wikiTableMap,subQueries,saveAnswerSRIs,mapPatternAnswer,mapPatternQuestion,
																				webSearchQueryStrings,parseOnly,answerPluralSpecification,finalAnswer,maxAnswer,useParallelQuery);
							if (limitProcessingForProfiling)
								return 0;
							lplog(LOG_WHERE|LOG_QCHECK,L"    *****Enhanced semantic map.");
							msi->second->sortAndCheck(*this,ssri, questionSource);
							msi->second->lplogSM(*this,LOG_WHERE, questionSource,true);
						}
					}
					else
						lplog(LOG_WHERE|LOG_QCHECK,L"    No entries in semantic map for %s.", questionSource->whereString(*si,tmpstr,true).c_str());
				}
				if (saveAnswerSRIs.empty())
					lplog(LOG_WHERE|LOG_QCHECK,L"    *****No answers found.");
			}
		}
		else
		{
			saveAnswerSRIs.insert(saveAnswerSRIs.end(),answerSRIs.begin(),answerSRIs.end());
		}
		findConstrainedAnswers(questionSource,saveAnswerSRIs, wherePossibleAnswers);
		if (wherePossibleAnswers.size()>0 && ssri->whereQuestionTypeObject >= 0 && ssri->whereQuestionTypeObject<(int)questionSource->m.size())
			wherePossibleAnswers.insert(wherePossibleAnswers.begin(), ssri->whereQuestionTypeObject);
		else
			wherePossibleAnswers.clear();
	}
	wprintf(L"\n%d total sources processed", (int)sourcesMap.size());
	return 0;
}

// no answers?
//   for each object that has a synonym (subject, object, propobject)
//     substitute object with synonym.  
//     run constructed query with webQueries only.
//     any more answers?
