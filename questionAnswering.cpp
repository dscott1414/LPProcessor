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
#include "source.h"
#include "QuestionAnswering.h"

int questionProgress=-1; // shared but update is not important
bool isBookTitle(MYSQL &mysql, wstring proposedTitle);

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
int cQuestionAnswering::processAbstract(cSource *questionSource,cTreeCat *rdfType,cSource *&source,bool parseOnly)
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
	return processPath(questionSource,path,source,cSource::WEB_SEARCH_SOURCE_TYPE,1,parseOnly);
}

int cQuestionAnswering::processSnippet(cSource *questionSource, wstring snippet,wstring object,cSource *&source,bool parseOnly)
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
	return processPath(questionSource,path,source,cSource::WEB_SEARCH_SOURCE_TYPE,50,parseOnly);
}

int cQuestionAnswering::processWikipedia(cSource *questionSource, int principalWhere,cSource *&source,vector <wstring> &wikipediaLinks,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases,bool parseOnly, set <wstring> &wikipediaLinksAlreadyScanned)
{ LFS
  wchar_t path[1024];
	vector <wstring> lookForSubject;
	if (questionSource->getWikipediaPath(principalWhere, wikipediaLinks, path, lookForSubject, includeNonMixedCaseDirectlyAttachedPrepositionalPhrases) < 0)
		return -1;
	if (wikipediaLinksAlreadyScanned.find(path) != wikipediaLinksAlreadyScanned.end())
		return -1;
	wikipediaLinksAlreadyScanned.insert(path);
	return processPath(questionSource,path,source,cSource::WIKIPEDIA_SOURCE_TYPE,2,parseOnly);
}

bool cQuestionAnswering::matchObjects(cSource *parentSource,vector <cObject>::iterator parentObject,cSource *childSource,vector <cObject>::iterator childObject,bool &namedNoMatch,sTrace debugTrace)
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

bool cQuestionAnswering::matchChildSourcePosition(cSource *parentSource,vector <cObject>::iterator parentObject,cSource *childSource,int childWhere,bool &namedNoMatch,sTrace &debugTrace)
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

// Curveball defected
//000009:TSYM PARENT year[9-10][9][nongen][N][TimeObject] is NOT a definite object [byClass].
//000008:TSYM CHILD 1999[8-9][8][nongen][N][TimeObject] is NOT a definite object [byClass].
bool cQuestionAnswering::matchTimeObjects(cSource *parentSource,int parentWhere,cSource *childSource,int childWhere)
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
bool cQuestionAnswering::matchSourcePositions(cSource *parentSource, int parentWhere, cSource *childSource, int childWhere, bool &namedNoMatch, bool &synonym, bool parentInQuestionObject, int &semanticMismatch, int &adjectivalMatch, sTrace &debugTrace)
{	LFS
	adjectivalMatch = -1;
	vector <cWordMatch>::iterator imChild=childSource->m.begin()+childWhere;
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
	// parent=From which university did he receive his doctorate? / child=a Phd 
	if (parentIsDefiniteObject && !childIsDefiniteObject)
	{
		// if the parent has a definite owner, but if the child doesn't have a different owner, we should not return false?
		//if (parentOwnerWhere>=0)
		//{
		//	int parentOwnerObject=(parentSource->m[parentOwnerWhere].objectMatches.size()>0) ? parentSource->m[parentOwnerWhere].objectMatches[0].object:parentSource->m[parentOwnerWhere].getObject();
		//	if (parentOwnerWhere>=0 && (parentSource->objects[parentOwnerObject].objectClass==NAME_OBJECT_CLASS || (parentSource->m[parentOwnerWhere].flags&cWordMatch::flagNounOwner)))
		//		return false;
		//}
		if (parentSource->m[parentSource->m[parentWhere].endObjectPosition-1].word->first==imChild->word->first)
			return true;
		int confidence=-1;
		if ((confidence=childSource->checkParticularPartSemanticMatch(LOG_WHERE,childWhere,parentSource,parentWhere,-1,synonym,semanticMismatch))<CONFIDENCE_NOMATCH)
		{
			// why are we insisting on 'a'?
			//if (imChild->beginObjectPosition>=0 && childSource->m[imChild->beginObjectPosition].word->first==L"a")
			//{
			//	if (confidence>1 || synonym) return false;
			//	synonym=true; // decrease match
			//}
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

int cQuestionAnswering::sriMatch(cSource *questionSource,cSource *childSource, int parentWhere, int childWhere, int whereQuestionType, __int64 questionType, bool &totalMatch, wstring &matchInfoDetail, int cost, bool subQuery)
{
	LFS
	totalMatch = false;
	if (parentWhere<0)
		return 0;
	bool inQuestionObject= questionSource->inObject(parentWhere,whereQuestionType);
  if (childWhere<0)
		return (inQuestionObject) ? -cost : 0;
	vector <cWordMatch>::iterator imChild=childSource->m.begin()+childWhere;
	bool childIsPronoun=(imChild->queryWinnerForm(nomForm)>=0 || imChild->queryWinnerForm(personalPronounAccusativeForm)>=0 ||
      imChild->queryWinnerForm(personalPronounForm)>=0 || imChild->queryWinnerForm(quantifierForm)>=0 ||
      imChild->queryWinnerForm(possessivePronounForm)>=0 ||
      imChild->queryWinnerForm(indefinitePronounForm)>=0 || imChild->queryWinnerForm(pronounForm)>=0 ||
			imChild->queryWinnerForm(demonstrativeDeterminerForm)>=0 ||
			imChild->queryWinnerForm(reflexivePronounForm)>=0 ||
			imChild->queryWinnerForm(reciprocalPronounForm)>=0);
	// if childIsPronoun, and there are no matches, or the only matches are narrator and audience, then keep childIsPronoun = true.
	if (childIsPronoun && imChild->objectMatches.size())
	{
		if (imChild->objectMatches.size() > 2)
			childIsPronoun = false;
		else if (imChild->objectMatches[0].object != 0 && imChild->objectMatches[0].object != 1)
			childIsPronoun = false;
		else if (imChild->objectMatches.size() == 2 && imChild->objectMatches[1].object != 0 && imChild->objectMatches[1].object != 1)
			childIsPronoun = false;
	}
	if (childIsPronoun)
	{
		matchInfoDetail += L"[PRONOUN]";
		return (inQuestionObject) ? -cost : 0;
	}
	if (inQuestionObject)
	{
		// What are titles of books written by Krugman? - transformed into 'Krugman wrote what book?'
		// answer cannot be a book or a synonym of book 
		// however, in subquery, it is the opposite.  If it is the same, then it is good.
		// The Prize is awarded in [Spain].
		if (questionSource->m[parentWhere].getMainEntry() == childSource->m[childWhere].getMainEntry())
		{
			matchInfoDetail += L"[QUESTION_OBJECT_EXACT_MATCH]";
			return (subQuery) ? cost:-cost;
		}
		// questions with acceptable answers
		// which books did Krugman write?  'A Man for All Seasons'.  
		// these answers must come from highly local sources, of which we have none, so therefore these answers are tabled for now:
		// which books did you put on the table? The big books.  The white ones.
		// which book did you put on the table? A big book.
		set <wstring> bookSynonyms;
		questionSource->getSynonyms(L"book", bookSynonyms, NOUN);
		if (questionSource->m[parentWhere].getMainEntry()->first == L"book" || bookSynonyms.find(questionSource->m[parentWhere].getMainEntry()->first) != bookSynonyms.end())
		{
			int childObject = childSource->m[childWhere].getObject();
			if (childObject < 0)
			{
				matchInfoDetail += L"[QUESTION_OBJECT_NO_BOOK_MATCH_NO_OBJECT]";
				return -cost;
			}
			int begin = childSource->objects[childObject].begin, end= childSource->objects[childObject].end;
			bool isBeginQuote = (cWord::isDoubleQuote(childSource->m[begin].word->first[0]));
			if (begin>0)
				isBeginQuote |= (cWord::isDoubleQuote(childSource->m[begin-1].word->first[0]));
			bool isEndQuote= (cWord::isDoubleQuote(childSource->m[end-1].word->first[0]));
			if (end<childSource->m.size())
				isEndQuote |= (cWord::isDoubleQuote(childSource->m[end].word->first[0]));
			if (!isBeginQuote || !isEndQuote)
			{
				matchInfoDetail += L"[QUESTION_OBJECT_NO_BOOK_MATCH_NO_QUOTE]";
				return -cost;
			}
			wstring bookTitle;
			childSource->objectString(childObject,bookTitle,true);
			if (cWord::isDoubleQuote(bookTitle[0]))
				bookTitle = bookTitle.substr(1);
			if (cWord::isDoubleQuote(bookTitle[bookTitle.size() - 1]))
				bookTitle = bookTitle.substr(0, bookTitle.size() - 1);
			if (bookTitle[bookTitle.size() - 1]==L',')
				bookTitle = bookTitle.substr(0, bookTitle.size() - 1);
			if (isBookTitle(questionSource->mysql,bookTitle))
			{
				matchInfoDetail += L"[QUESTION_OBJECT_BOOK_MATCH]";
				lplog(LOG_WHERE, L"Found Title %s", bookTitle.c_str());
				return cost;
			}
			else
			{
				matchInfoDetail += L"[QUESTION_OBJECT_NO_BOOK_MATCH]";
				return -cost;
			}
		}
		set <wstring> synonyms;
		questionSource->getSynonyms(questionSource->m[parentWhere].getMainEntry()->first, synonyms, NOUN);
		if (synonyms.find(childSource->m[childWhere].getMainEntry()->first) != synonyms.end())
		{
			matchInfoDetail += L"[QUESTION_OBJECT_SYNONYM_MATCH]";
			return (subQuery) ? cost : -cost;
		}
		if (!(questionType&QTAFlag) && questionType != unknownQTFlag)
		{
			matchInfoDetail += L"[NOT_QTA (so half cost)]";
			return cost >> 1;
		}
	}
	bool namedNoMatch=false,synonym=false;
	int semanticMismatch=0, adjectivalMatch=-1;
	// if in subquery, wait to see if it is a match, if so, boost
	//if (inQuestionObject && questionType==unknownQTFlag) 
	//		cost<<=1;
	if (matchSourcePositions(questionSource, parentWhere, childSource, childWhere, namedNoMatch, synonym, inQuestionObject, semanticMismatch, adjectivalMatch, questionSource->debugTrace))
	{
		//if (inQuestionObject && questionType==unknownQTFlag) // in subquery - boost
		//	lplog(LOG_WHERE,L"%d:Boost found subquery match position - cost=%d!",childWhere,(synonym) ? cost*3/4 : cost);
		if (semanticMismatch)
		{
			matchInfoDetail += L"[SEMANTIC_MISMATCH]";
			return -cost;
		}
		if (synonym)
			matchInfoDetail += L"[MATCH_SYNONYM]";
		totalMatch = !synonym;
		if (synonym)
			return cost * 3 / 4;
		// adjectives match exactly
		if (adjectivalMatch == 1)
		{
			matchInfoDetail += L"[ADJECTIVE_EXACT_MATCH]";
			return cost * 2;
		}
		// adjectives are synonyms
		if (adjectivalMatch == 2)
			return (int)(cost * 1.3);
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

int cQuestionAnswering::sriVerbMatch(cSource *parentSource,cSource *childSource,int parentWhere,int childWhere,wstring &matchInfoDetailVerb, wstring verbTypeMatch, int cost)
{ LFS
	if (parentWhere < 0 || childWhere < 0)
	{
		matchInfoDetailVerb += L"["+ verbTypeMatch+L" where is negative]";
		return 0;
	}
	tIWMM childWord,parentWord;
	if ((childWord = childSource->m[childWhere].getMainEntry()) == (parentWord = parentSource->m[parentWhere].getMainEntry()))
	{
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb exact match]";
		return cost;
	}
	set <wstring> childSynonyms;
	wstring tmpstr;
	childSource->getSynonyms(childWord->first,childSynonyms, VERB);
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"TSYM [VERB] comparing PARENT %s against synonyms [%s]%s", parentWord->first.c_str(), childWord->first.c_str(), setString(childSynonyms, tmpstr, L"|").c_str());
	if (childSynonyms.find(parentWord->first) != childSynonyms.end())
	{
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb parent in child synonym match]";
		return cost * 3 / 4;
	}
	set <wstring> parentSynonyms;
	childSource->getSynonyms(parentWord->first,parentSynonyms, VERB);
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"%s TSYM [VERB] comparing CHILD %s against synonyms [%s]%s", verbTypeMatch.c_str(),childWord->first.c_str(), parentWord->first.c_str(), setString(parentSynonyms, tmpstr, L"|").c_str());
	if (parentSynonyms.find(childWord->first) != parentSynonyms.end())
	{
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb child in parent synonym match]";
		return cost * 3 / 4;
	}
	if ((childWord->first==L"am" && (parentWord->second.timeFlags&31)==T_START) || (parentWord->first==L"am" && (childWord->second.timeFlags&31)==T_START))
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE,L"%s TSYM [VERBBEGINBEING] matched CHILD %s against PARENT %s",verbTypeMatch.c_str(),childWord->first.c_str(),parentWord->first.c_str());
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb AM match]";
		return cost/2;
	}
	wstring parentVerb, parentVerbClassInfo, childVerb, childVerbClassInfo;
	unordered_map <wstring, set <int> >::iterator parentVerbClasses = parentSource->getVerbClasses(parentWhere, parentVerb);
	if (parentVerbClasses != vbNetVerbToClassMap.end())
	{
		parentVerbClassInfo = parentVerbClasses->first + L":";
		for (set <int>::iterator vbi = parentVerbClasses->second.begin(), vbiEnd = parentVerbClasses->second.end(); vbi != vbiEnd; vbi++)
			parentVerbClassInfo += vbNetClasses[*vbi].name() + L" ";
	}
	unordered_map <wstring, set <int> >::iterator childVerbClasses = childSource->getVerbClasses(childWhere, childVerb);
	if (childVerbClasses != vbNetVerbToClassMap.end())
	{
		childVerbClassInfo = childVerbClasses->first + L":";
		for (set <int>::iterator vbi = childVerbClasses->second.begin(), vbiEnd = childVerbClasses->second.end(); vbi != vbiEnd; vbi++)
			childVerbClassInfo += vbNetClasses[*vbi].name() + L" ";
	}
	lplog(LOG_WHERE, L"%s VerbNet comparing PARENT verb %s [class=%s] against CHILD verb %d:%s [class=%s]", verbTypeMatch.c_str(),parentVerb.c_str(), parentVerbClassInfo.c_str(), childWhere,childVerb.c_str(), childVerbClassInfo.c_str());
	if (parentVerbClasses != vbNetVerbToClassMap.end() && childVerbClasses != vbNetVerbToClassMap.end())
	{
		if (parentVerbClasses->first == childVerbClasses->first)
		{
			matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb match verb class]";
			return cost / 2;
		}
		if ((parentVerbClasses->first == L"receive" && childVerbClasses->first == L"pursue") ||
			(parentVerbClasses->first == L"pursue" && childVerbClasses->first == L"receive"))
		{
			matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb match verb class receive/pursue]";
			return cost / 2;
		}
	}
	return 0;
}

int cQuestionAnswering::sriPrepMatch(cSource *parentSource, cSource *childSource,int parentWhere,int childWhere,int cost)
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
int cQuestionAnswering::equivalenceClassCheck(cSource *questionSource,cSource *childSource,vector <cSpaceRelation>::iterator childSRI,cSpaceRelation* parentSRI,int whereChildSpecificObject,int &equivalenceClass,int matchSum)
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

int cQuestionAnswering::equivalenceClassCheck2(cSource *questionSource, cSource *childSource,vector <cSpaceRelation>::iterator childSRI,cSpaceRelation* parentSRI,int whereChildSpecificObject,int &equivalenceClass,int matchSum)
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
int cQuestionAnswering::metaPatternMatch(cSource *questionSource,cSource *childSource,vector <cSpaceRelation>::iterator childSRI,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion)
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
		vector < vector <cTagLocation> > tagSets;
		if (childSource->startCollectTags(true,metaNameEquivalenceTagSet,whereMNE,childSource->m[whereMNE].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd,tagSets,false,true,L"meta pattern match")>0)
			for (unsigned int J=0; J<tagSets.size(); J++)
			{
				
				childSource->printTagSet(LOG_WHERE,L"MNE",J,tagSets[J],whereMNE,childSource->m[whereMNE].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd);
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
					wstring diff=patterns[childSource->m[whereMNE].pma[element&~cMatchElement::patternFlag].getPattern()]->differentiator;
					lplog(LOG_WHERE,L"%d:meta matched pattern %s[%s]",whereMNE,patterns[childSource->m[whereMNE].pma[element&~cMatchElement::patternFlag].getPattern()]->name.c_str(),patterns[childSource->m[whereMNE].pma[element&~cMatchElement::patternFlag].getPattern()]->differentiator.c_str());
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

// process a child object in the child source as a string and return whether it should be processed by comparing it to the strings that have already been processed or illegal
//   input: childSource, childObject and whereQuestionInformationSourceObjectsStrings.
//   output: childObjectString
//     1. remove irrelevant object classes
//     2. remove determiners 'this' or 'that', 'there' or 'so'
//     3. skip determiners
//     4. create string with the rest of the words belonging to object.  
//     5. remove ownership
//     6. transform to all lower case.
//     7. if already in the considered list (whereQuestionInformationSourceObjectsStrings) return false.
//     8. log the context around certain illegal text words
//     9. if this string contains any of the others as a separate word string (whereQuestionInformationSourceObjectsStrings), return false.
bool cQuestionAnswering::processChildObjectIntoString(cSource *childSource, int childObject, unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings,wstring &childObjectString)
{
	// ****************************
	// process childObject into a string
	// ****************************
	// remove all classes of objects other than names including Narrator and Audience
	if (childObject < 2 ||
		childSource->objects[childObject].objectClass == PRONOUN_OBJECT_CLASS ||
		childSource->objects[childObject].objectClass == REFLEXIVE_PRONOUN_OBJECT_CLASS ||
		childSource->objects[childObject].objectClass == RECIPROCAL_PRONOUN_OBJECT_CLASS ||
		childSource->objects[childObject].objectClass == VERB_OBJECT_CLASS ||
		childSource->objects[childObject].objectClass == PLEONASTIC_OBJECT_CLASS ||
		childSource->objects[childObject].objectClass == META_GROUP_OBJECT_CLASS)
		return false;
	// remove determiners like 'this' or 'that', 'there' or 'so'
	if (childSource->objects[childObject].end - childSource->objects[childObject].begin == 1 &&
		(childSource->m[childSource->objects[childObject].begin].queryWinnerForm(demonstrativeDeterminerForm) >= 0 || childSource->m[childSource->objects[childObject].begin].queryWinnerForm(letterForm) >= 0 ||
			childSource->m[childSource->objects[childObject].begin].word->first == L"there" || childSource->m[childSource->objects[childObject].begin].word->first == L"so"))
		return false;
	unsigned int begin = childSource->m[childSource->objects[childObject].originalLocation].beginObjectPosition;
	// this increases the hit rate by not making a distinction with determiners.
	while (begin < childSource->m.size() && (childSource->m[begin].queryWinnerForm(determinerForm) >= 0 ||
		childSource->m[begin].queryWinnerForm(possessiveDeterminerForm) >= 0 ||
		childSource->m[begin].queryWinnerForm(demonstrativeDeterminerForm) >= 0 ||
		childSource->m[begin].queryWinnerForm(interrogativeDeterminerForm) >= 0 ||
		childSource->m[begin].queryWinnerForm(relativizerForm) >= 0 ||
		childSource->m[begin].queryWinnerForm(pronounForm) >= 0 ||
		childSource->m[begin].queryWinnerForm(quantifierForm) >= 0 ||
		childSource->m[begin].word->first == L"which"))
		begin++;
	if (begin == childSource->m[childSource->objects[childObject].originalLocation].endObjectPosition)
		return false;
	childSource->phraseString(begin, childSource->m[childSource->objects[childObject].originalLocation].endObjectPosition, childObjectString, true);
	// ownership objects are converted
	if (childObjectString.length() > 2 && childObjectString[childObjectString.length() - 2] == L'\'')
		childObjectString.erase(childObjectString.length() - 2);
	// exclude from semantic map any objects which include the whereQuestionInformationSourceObjects
	wstring objectStrLwr = childObjectString;
	transform(objectStrLwr.begin(), objectStrLwr.end(), objectStrLwr.begin(), (int(*)(int)) tolower);
	if (whereQuestionInformationSourceObjectsStrings.find(objectStrLwr) != whereQuestionInformationSourceObjectsStrings.end())
		return false;
	// log the context around certain illegal text words
	if (childObjectString == L"br" || (childObjectString == L"com" && begin > 0 && childSource->m[begin - 1].word->first == L".") || childObjectString == L"http" || childObjectString == L"href" || childObjectString == L"span" || childObjectString == L"div" ||
		childObjectString == L"html" || childObjectString == L"which" || childObjectString == L"that")
	{
		int end = childSource->m[childSource->objects[childObject].originalLocation].endObjectPosition + 10;
		if (end > childSource->m.size())
			end = childSource->m.size();
		childSource->phraseString((begin > 10) ? begin - 10 : 0, end, childObjectString, true);
		lplog(LOG_WHERE, L"accumulateProximityEntry[processChildObjectIntoString] context %s:%d:%s", childObjectString.c_str(), begin, childSource->sourcePath.c_str());
		return false;
	}
	// does this lowered string contain any other object string?  If so, continue
	// search for each string as a separate word 
	bool wqiFound = false;
	for (unordered_set <wstring>::iterator wqi = whereQuestionInformationSourceObjectsStrings.begin(), wqiEnd = whereQuestionInformationSourceObjectsStrings.end(); wqi != wqiEnd && !wqiFound; wqi++)
	{
		size_t pos = objectStrLwr.find(*wqi);
		wqiFound = (pos != wstring::npos && ((pos == 0 || !iswalpha(objectStrLwr[pos - 1])) && (pos + wqi->length() >= objectStrLwr.length() || !iswalpha(objectStrLwr[pos + wqi->length()]))));
	}
	return !wqiFound;
}

// record distance into the entry in proximity map (belonging to each QuestionInformationSourceObject in parentSRI) for childObjectString (closestObjectIterator) to how near it is to questionObjectMatchIndex (that matches the questionInformationObject in the child source)
//     2. find or create an entry in proximityMap relativeObjects corresponding to the object string.
//     4. get the closest principalWhere location to where and calculate distance
//     5. add the distance to the nearest principalWhere object to the sum in the relativeObjects entry corresponding to this lowered string (which is calculated from the object at where)
//     6. if the principalWhere and the where positions share a relative verb location, record in the number of direct relations, with a source path and which location (where)
void cQuestionAnswering::recordDistanceIntoProximityMap(cSource *childSource, unsigned int childSourceIndex, set <cObject::cLocation> &questionObjectMatchInChildSourceLocations,
	set <cObject::cLocation>::iterator &questionObjectMatchIndex, bool confidence, unordered_map <wstring, cProximityMap::cProximityEntry>::iterator closestObjectIterator)
{
	//lplog(LOG_WHERE,L"WSM %s:%d:%s [%d:%d]",sourcePath.c_str(),where,childObjectString.c_str(),roi->second.inSource,roi->second.confidentInSource);

	// get the closest principalWhere location to where and calculate distance
	// 1 4 8 CASES: where==0, where==1 where==2 where==4 where===6 where==8 where==9
	// index               0         0        1        1         2        2        3
	// calculate distance and minObjectWhere
	for (; questionObjectMatchIndex != questionObjectMatchInChildSourceLocations.end() && questionObjectMatchIndex->at < (int)childSourceIndex; questionObjectMatchIndex++);
	int distance = 0, minObjectWhere = -1;
	if (questionObjectMatchIndex == questionObjectMatchInChildSourceLocations.end())
	{
		set <cObject::cLocation>::iterator poliEnd2 = questionObjectMatchInChildSourceLocations.end();
		distance = childSourceIndex - (minObjectWhere = (--poliEnd2)->at);
	}
	else if (questionObjectMatchIndex == questionObjectMatchInChildSourceLocations.begin())
		distance = (minObjectWhere = questionObjectMatchInChildSourceLocations.begin()->at) - childSourceIndex;
	else
	{
		int d1 = questionObjectMatchIndex->at - childSourceIndex;
		set <cObject::cLocation>::iterator pi = questionObjectMatchIndex;
		pi--;
		int d2 = childSourceIndex - pi->at;
		minObjectWhere = (d1 < d2) ? questionObjectMatchIndex->at : pi->at;
		distance = min(d1, d2);
	}
	// add the distance to the nearest principalWhere object to the sum in the relativeObjects entry corresponding to this lowered string (which is calculated from the object at where)
	if (confidence)
		closestObjectIterator->second.confidentTotalDistanceFromObject += distance;
	else
		closestObjectIterator->second.totalDistanceFromObject += distance;
	// if the principalWhere and the where positions share a relative verb location, record in the number of direct relations, with a source path and which location (where)
	if (childSource->m[childSourceIndex].getRelVerb() == childSource->m[minObjectWhere].getRelVerb() && childSource->m[childSourceIndex].getRelVerb() != -1)
	{
		if (confidence)
			closestObjectIterator->second.confidentDirectRelation++;
		else
			closestObjectIterator->second.directRelation++;
		closestObjectIterator->second.relationSourcePaths.push_back(childSource->sourcePath);
		closestObjectIterator->second.relationWheres.push_back(childSourceIndex);
	}
	closestObjectIterator->second.childSourcePaths.insert(childSource->sourcePath);
}

// accumulate in the proximityMap a list of all 
// at the position 'childSourceIndex', for each object that has matched at this position:
//     1. process child object into a string.
//     2. record distance into the proximity map (the one for each QuestionInformationSourceObject in parentSRI) for childObjectString (closestObjectIterator) to how near it is to questionObjectMatchIndex (that matches the questionInformationObject in the child source)
void cQuestionAnswering::accumulateProximityEntry(cSource *childSource, unsigned int childSourceIndex, set <cObject::cLocation> &questionObjectMatchInChildSourceLocations, 
	set <cObject::cLocation>::iterator &questionObjectMatchIndex, bool confidence, cSpaceRelation* parentSRI, cProximityMap *proximityMap, unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings)
{
	LFS
		vector <cOM> childObjectMatches = childSource->m[childSourceIndex].objectMatches;
	if (childSource->m[childSourceIndex].objectMatches.empty())
		childObjectMatches.push_back(cOM(childSource->m[childSourceIndex].getObject(), -1));
	for (unsigned int I = 0; I < childObjectMatches.size(); I++)
	{
		wstring childObjectString;
		int childObject = childObjectMatches[I].object;
		if (processChildObjectIntoString(childSource, childObject, whereQuestionInformationSourceObjectsStrings, childObjectString))
		{
			// ****************************
			// record distance into the proximity map (the one for each QuestionInformationSourceObject in parentSRI) for childObjectString (closestObjectIterator) to how near it is to questionObjectMatchIndex (that matches the questionInformationObject in the child source)
			// ****************************
			// find or create an entry in proximityMap relativeObjects corresponding to the object string.
			unordered_map <wstring, cProximityMap::cProximityEntry>::iterator closestObjectIterator = proximityMap->closestObjects.find(childObjectString);
			if (closestObjectIterator == proximityMap->closestObjects.end())
			{
				proximityMap->closestObjects[childObjectString] = cProximityMap::cProximityEntry(childSource, childSourceIndex, childObject,parentSRI);
				closestObjectIterator = proximityMap->closestObjects.find(childObjectString);
			}
			if (confidence)
				closestObjectIterator->second.confidentInSource++;
			else
				closestObjectIterator->second.inSource++;
			recordDistanceIntoProximityMap(childSource, childSourceIndex, questionObjectMatchInChildSourceLocations, questionObjectMatchIndex, confidence, closestObjectIterator);
		}
	}
}

// accumulate additional words to use in web search strings to accumulate information to answer question.
// these additional words come from each child source, based on how often and how close these objects are to the original question information source objects.
// 1. for each question information object, 
//      create a string that is all lower case.  
//      If it was all upper case, and very short, make it into an acronym
// 2. for each question information object, 
//      create a map object   
//      3. If the source has already been scanned, skip this object.
//			4. accumulate objects pertaining to this question information source in parentObjects.
//			5. for each parent object
//					6. for every object in a child source
//							7. if the child source object matches the parentObject, accumulate the locations for this child source object in questionObjectMatchInChildSourceLocations.
//				8. for every object location in the child, call accumulateProximityEntry
void cQuestionAnswering::accumulateProximityMaps(cSource *questionSource, cSpaceRelation* parentSRI, cSource *childSource, bool confidence)
{
	LFS
		wstring tmpstr;
	unordered_set <wstring> whereQuestionInformationSourceObjectsStrings;
	// 1. for each question information object, 
	//      create a string that is all lower case.  
	//      If it was all upper case, and very short, make it into an acronym
	for (set <int>::iterator si = parentSRI->whereQuestionInformationSourceObjects.begin(), siEnd = parentSRI->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
	{
		questionSource->whereString(*si, tmpstr, true);
		bool isAllUpper = true, containsNonAlpha = false, containsPeriod = false;
		if (tmpstr.length() < 4)
		{
			for (int I = 0; I < tmpstr.length(); I++)
			{
				isAllUpper = isAllUpper && isupper(tmpstr[I]);
				containsNonAlpha |= !isalpha(tmpstr[I]);
				containsPeriod |= tmpstr[I] == L'.';
			}
		}
		transform(tmpstr.begin(), tmpstr.end(), tmpstr.begin(), (int(*)(int)) tolower);
		whereQuestionInformationSourceObjectsStrings.insert(tmpstr);
		if (tmpstr.length() < 4 && isAllUpper && !containsNonAlpha && !containsPeriod)
		{
			// assume this is a special acronym - convert to an acronym with periods.
			wstring tmpstr2;
			for (int I = 0; I < tmpstr.length(); I++)
			{
				tmpstr2 += tmpstr[I];
				tmpstr2 += L'.';
			}
			whereQuestionInformationSourceObjectsStrings.insert(tmpstr2);
		}
	}
	for (set <int>::iterator si = parentSRI->whereQuestionInformationSourceObjects.begin(), siEnd = parentSRI->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
	{
		// 2. for each question information object, create a semantic map.  
		if (parentSRI->proximityMaps.find(*si) == parentSRI->proximityMaps.end())
			parentSRI->proximityMaps[*si] = new cProximityMap();
		cProximityMap *proximityMap = parentSRI->proximityMaps[*si];
		if (proximityMap->SMPrincipalObject.empty())
			proximityMap->SMPrincipalObject = questionSource->whereString(*si, tmpstr, true);
		// 3. If the source has already been scanned, skip this object.
		if (proximityMap->sourcePaths.find(childSource->sourcePath) != proximityMap->sourcePaths.end())
			continue;
		proximityMap->sourcePaths.insert(childSource->sourcePath);
		// 4. accumulate objects pertaining to this question information source in parentObjects.
		int parentObject = questionSource->m[*si].getObject();
		vector <cOM> parentObjects = questionSource->m[*si].objectMatches;
		if (parentObjects.empty())
		{
			if (parentObject < 0)
				continue;
			parentObjects.push_back(cOM(parentObject, -1));
		}
		// 5. for each parent object
		for (vector <cOM>::iterator poi = parentObjects.begin(), poiEnd = parentObjects.end(); poi != poiEnd; poi++)
		{
			bool checkForUpperCase = (questionSource->checkForUppercaseSources(poi->object));
			int parentObjectClass = questionSource->objects[poi->object].objectClass;
			set <cObject::cLocation> questionObjectMatchInChildSourceLocations;
			bool namedNoMatch = false;
			if (logProximityMap)
				lplog(LOG_WHERE, L"%s:? SM", questionSource->objectString(*poi, tmpstr, false).c_str());
			// 6. for every object in a child source
			for (unsigned int I = 0; I < childSource->objects.size(); I++)
			{
				wstring tmpstr2;
				//if (logProximityMap)
				//	lplog(LOG_WHERE,L"==%s? SM",childSource->objectString(I,tmpstr2,false).c_str());
				// 7. if the child source object matches the parentObject, accumulate the locations for this child source object in primcipalObjectLocations.
				if (childSource->objects[I].objectClass == parentObjectClass && matchObjects(questionSource, questionSource->objects.begin() + poi->object, childSource, childSource->objects.begin() + I, namedNoMatch, questionSource->debugTrace))
				{
					questionObjectMatchInChildSourceLocations.insert(childSource->objects[I].locations.begin(), childSource->objects[I].locations.end());
					for (vector <int>::iterator oai = childSource->objects[I].aliases.begin(), oaiEnd = childSource->objects[I].aliases.end(); oai != oaiEnd; oai++)
						questionObjectMatchInChildSourceLocations.insert(childSource->objects[*oai].locations.begin(), childSource->objects[*oai].locations.end());
				}
			}
			if (questionObjectMatchInChildSourceLocations.empty())
				continue;
			// 8. for every object location in the child, call accumulateProximityEntry
			set <cObject::cLocation>::iterator polIndex = questionObjectMatchInChildSourceLocations.begin();
			for (unsigned int childSourceIndex = 0; childSourceIndex < childSource->m.size(); childSourceIndex++)
			{
				if (checkForUpperCase && childSource->isEOS(childSourceIndex) && childSource->skipSentenceForUpperCase(childSourceIndex))
					continue;
				if (childSource->m[childSourceIndex].getObject() < 0 && childSource->m[childSourceIndex].objectMatches.empty())
					continue;
				childSource->parentSource = questionSource;
				// how close is childSourcIndex to each of the questionObjectMatchInChildSourceLocations?
				accumulateProximityEntry(childSource, childSourceIndex, questionObjectMatchInChildSourceLocations, polIndex, confidence, parentSRI, proximityMap, whereQuestionInformationSourceObjectsStrings);
				childSource->parentSource = 0;
			}
		}
	}
}

// parse comment/abstract of rdfType.  
// For each sentence, 
//    match preferentially the subject/verb/object/prep?/prepObject, 
//    with special analysis match of the whereQuestionTargetSuggestionIndex, if sri->questionType is an adjective type.
//    answer is the special analysis match.
int cQuestionAnswering::analyzeQuestionFromSource(cSource *questionSource,wchar_t *derivation,wstring childSourceType,cSource *childSource, cSpaceRelation * parentSRI,vector < cAS > &answerSRIs,int &maxAnswer,bool eraseIfNoAnswers,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion)
{ LFS
	int currentPercent,lastProgressPercent=-1,startClockTime=clock();
	if (logQuestionDetail)
		lplog(LOG_WHERE,L"********[%s used %d times:%d total sources] %s:",derivation,childSource->numSearchedInMemory,sourcesMap.size(),childSource->sourcePath.c_str());
	else
		lplog(LOG_WHERE,L"********[%s] %s:",derivation,childSource->sourcePath.c_str());
	childSource->parentSource=NULL; // this is set so we can investigate child sources through identification of ISA types
	accumulateProximityMaps(questionSource,parentSRI,childSource,childSource->sourceType==cSource::WIKIPEDIA_SOURCE_TYPE);
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
		bool inQuestion=(childSRI->whereSubject>=0 && (childSource->m[childSRI->whereSubject].flags&cWordMatch::flagInQuestion));
		inQuestion|=(childSRI->whereObject>=0 && (childSource->m[childSRI->whereObject].flags&cWordMatch::flagInQuestion));
		if (inQuestion) continue;
		// check for negation agreement
		if (childSRI->tft.negation ^ parentSRI->tft.negation)
			continue;
		// check for who / who his real name is / who is object in relative clause / SRI is constructed dynamically
		if (childSRI->whereObject>=0 && childSRI->whereSubject>=0 && childSRI->whereObject<childSRI->whereSubject &&
			  childSource->m[childSRI->whereSubject].getRelObject()<0 && 
				(childSource->m[childSRI->where].objectRole&SENTENCE_IN_REL_ROLE) && childSource->m[childSRI->whereSubject].beginObjectPosition>0 && 
				childSource->m[childSource->m[childSRI->whereSubject].beginObjectPosition-1].queryWinnerForm(relativizerForm)>=0 && 
				(childSource->m[childSource->m[childSRI->whereSubject].beginObjectPosition-1].flags&cWordMatch::flagRelativeObject) &&
				childSRI->whereObject==childSource->m[childSource->m[childSRI->whereSubject].beginObjectPosition-1].getRelObject())
			continue;
		int whereMetaPatternAnswer=-1;
		if (mapPatternAnswer != NULL && (whereMetaPatternAnswer = metaPatternMatch(questionSource, childSource, childSRI, mapPatternAnswer, mapPatternQuestion)) >= 0)
			enterAnswerAccumulatingIdenticalAnswers(questionSource, parentSRI, cAS(childSourceType, childSource, -1, maxAnswer, L"META_PATTERN", &(*childSRI), 0, whereMetaPatternAnswer, -1, -1, false, L"", L"", 0, 0, 0, NULL), maxAnswer, answerSRIs);
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
				matchSumSubject = sriMatch(questionSource,childSource, parentSRI->whereSubject, ws, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticSubjectTotalMatch, matchInfoDetailSubject, 8, parentSRI->subQuery);
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
			for (int vi = childSRI->whereVerb; true; vi = childSource->m[vi].nextCompoundPartObject)
			{
				wstring matchInfoDetailVerb;
				int verbMatch=sriVerbMatch(questionSource,childSource,parentSRI->whereVerb,vi, matchInfoDetailVerb, L"PRIMARY", 8);
				bool subjectMatch=matchSumSubject>0;
				bool inSubQuery=parentSRI->questionType==unknownQTFlag;
				if (verbMatch<=0 && !subjectMatch && !questionTypeSubject && !inSubQuery)
				{
					if (ws<0 || childSource->m[ws].nextCompoundPartObject<0 || ws>=childSource->m.size()) break;
					if (vi < 0 || childSource->m[vi].nextCompoundPartObject < 0 || vi >= childSource->m.size()) break;
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
					wstring matchInfoDetail=matchInfoDetailSubject+ matchInfoDetailVerb;
					int objectMatch = 0, secondaryVerbMatch = 0, secondaryObjectMatch = 0;
					objectMatch = sriMatch(questionSource, childSource, parentSRI->whereObject, wo, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticObjectTotalMatch, matchInfoDetail, 8, parentSRI->subQuery);
					if (parentSRI->subQuery && questionTypeObject && objectMatch > 0)
						whereAnswerMatchSubquery.insert(childSRI->whereObject);

					secondaryVerbMatch = sriVerbMatch(questionSource, childSource, parentSRI->whereSecondaryVerb, childSRI->whereSecondaryVerb, matchInfoDetail, L"SECONDARY TO SECONDARY", 4);
					if (parentSRI->whereSecondaryVerb >= 0 && (secondaryVerbMatch == 0 || childSRI->whereSecondaryVerb < 0))
					{
						if (childSRI->whereSecondaryVerb >= 0 || (secondaryVerbMatch = sriVerbMatch(questionSource, childSource, parentSRI->whereSecondaryVerb, vi, matchInfoDetail, L"SECONDARY TO PRIMARY", 8)) == 0)
						{
							secondaryVerbMatch = -verbMatch;
							matchInfoDetail += L"[SECONDARY_VERB_MATCH_FAILED]";
						}
						else
							verbMatch = 0;
					}
					secondaryObjectMatch = sriMatch(questionSource, childSource, parentSRI->whereSecondaryObject, childSRI->whereSecondaryObject, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticSecondaryObjectTotalMatch, matchInfoDetail, 4, parentSRI->subQuery);
					if (parentSRI->subQuery && questionTypeObject && secondaryObjectMatch > 0)
						whereAnswerMatchSubquery.insert(childSRI->whereSecondaryObject);
					if (parentSRI->whereSecondaryObject >= 0 && (secondaryObjectMatch == 0 || childSRI->whereSecondaryObject < 0))
					{
						if (childSRI->whereSecondaryObject >= 0 || (secondaryObjectMatch = sriMatch(questionSource, childSource, parentSRI->whereSecondaryObject, wo, parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticSecondaryObjectTotalMatch, matchInfoDetail, 8, parentSRI->subQuery)) == 0)
						{
							secondaryObjectMatch = -objectMatch;
							matchInfoDetail += L"[SECONDARY_OBJECT_MATCH_FAILED]";
						}
						else
							objectMatch = 0;
					}
					// PARENT: what     [S Darrell Hammond]  featured   on [PO Comedy Central program[12-15][14][nongen][N]]?
					// CHILD: [S Darrell Hammond]  was [O a featured cast member[67-71][70][nongen][N]] on   [PO Saturday Night Live[72-75][72][ngname][N][WikiWork]].
					int cob = -1, coe;
					if (verbMatch <= 0 && parentSRI->whereVerb >= 0 && vi >= 0 && childSRI->whereObject >= 0 &&
						childSource->m[vi].forms.isSet(isForm) && (cob = childSource->m[childSRI->whereObject].beginObjectPosition) >= 0 && (coe = childSource->m[childSRI->whereObject].endObjectPosition) >= 0 &&
						(coe - cob) > 2 && childSource->m[cob].queryForm(determinerForm) >= 0 && childSource->m[cob + 1].getMainEntry() == questionSource->m[parentSRI->whereVerb].getMainEntry())
					{
						verbMatch = 8;
						matchInfoDetail += L"[MOVED_VERB_TO_OBJECT]";
					}
					set <int> relPreps;
					childSource->getAllPreps(&(*childSRI), relPreps, wo);
					if (relPreps.empty())
						relPreps.insert(-1);
					for (set <int>::iterator rpi = relPreps.begin(), rpiEnd = relPreps.end(); rpi != rpiEnd; rpi++)
					{
						LFSL
							wstring matchInfo = matchInfoDetail;
						int prepObjectMatch = 0, prepMatch = 0;
						if (*rpi != -1)
						{
							if (prepMatch = sriPrepMatch(questionSource, childSource, parentSRI->wherePrep, *rpi, 2) == 2) prepMatch = 2;
							prepObjectMatch = sriMatch(questionSource, childSource, parentSRI->wherePrepObject, childSource->m[*rpi].getRelObject(), parentSRI->whereQuestionType, parentSRI->questionType, childSRI->nonSemanticPrepositionObjectTotalMatch, matchInfoDetail, 4, parentSRI->subQuery);
							if (parentSRI->subQuery && questionTypePrepObject && prepObjectMatch > 0)
								whereAnswerMatchSubquery.insert(childSource->m[*rpi].getRelObject());
							if (prepObjectMatch <= 0 && questionSource->inObject(parentSRI->wherePrepObject, parentSRI->whereQuestionType))
								prepObjectMatch = -8;
						}
						else if (questionSource->inObject(parentSRI->wherePrepObject, parentSRI->whereQuestionType))
							prepObjectMatch = -8;
						int equivalenceClass = 0, equivalenceMatch = 0;
						if (verbMatch <= 0 && (subjectMatch || (questionTypeSubject && !(parentSRI->questionType&QTAFlag))) && (*rpi == -1 || (prepMatch > 0 && (prepObjectMatch > 0 || questionSource->inObject(parentSRI->wherePrepObject, parentSRI->whereQuestionType)))))
						{
							if (equivalenceClassCheck(questionSource, childSource, childSRI, parentSRI, wo, equivalenceClass, 8) == 0)
								equivalenceMatch = equivalenceClassCheck2(questionSource, childSource, childSRI, parentSRI, wo, equivalenceClass, 8);
						}
						int matchSum = matchSumSubject + verbMatch + objectMatch + secondaryVerbMatch + secondaryObjectMatch + prepMatch + prepObjectMatch + equivalenceMatch;
						if (logQuestionDetail || matchSum >= 8 + 6)
						{
							appendSum(matchSumSubject, L"+SUBJ[", matchInfo);
							appendSum(verbMatch, L"+VERB[", matchInfo);
							appendSum(objectMatch, L"+OBJ[", matchInfo);
							appendSum(secondaryVerbMatch, L"+VERB2[", matchInfo);
							appendSum(secondaryObjectMatch, L"+OBJ2[", matchInfo);
							appendSum(prepMatch, L"+PREP[", matchInfo);
							appendSum(prepObjectMatch, L"+PREPOBJ[", matchInfo);
							appendSum(equivalenceMatch, L"+EM[", matchInfo);
						}
						if (logQuestionDetail)
						{
							wstring ps;
							childSource->prepPhraseToString(childSRI->wherePrep, ps);
							childSource->printSRI(L"    [printall] " + matchInfo, &(*childSRI), 0, childSRI->whereSubject, childSRI->whereObject, ps, false, -1, L"");
						}
						if (matchSum > 8 + 6 && ((parentSRI->subQuery && subjectMatch) || (subjectMatch && (verbMatch > 0 || secondaryVerbMatch > 0)))) // a single element match and 3/4 of another (sym match)
						{
							if (parentSRI->subQuery)
							{
								for (int wc : whereAnswerMatchSubquery)
								{
									bool match;
									if (match = checkParticularPartIdentical(questionSource, childSource, parentSRI->whereQuestionType, wc))
									{
										matchSum += 8;
										matchInfo += L"ANSWER_MATCH[+8]";
									}
									int parentObject, childObject;
									if ((parentObject = questionSource->m[parentSRI->whereQuestionType].getObject()) >= 0 && (childObject = childSource->m[wc].getObject()) >= 0)
									{
										if (!childSource->objects[childObject].dbPediaAccessed)
											childSource->identifyISARelation(wc, false);
										bool areBothPlaces = false, wikiTypeMatch = false;
										if (wikiTypeMatch = (questionSource->objects[parentObject].isWikiBusiness && childSource->objects[childObject].isWikiBusiness) ||
											(questionSource->objects[parentObject].isWikiPerson && childSource->objects[childObject].isWikiPerson) ||
											(areBothPlaces = (questionSource->objects[parentObject].isWikiPlace && childSource->objects[childObject].isWikiPlace) ||
											(questionSource->objects[parentObject].isWikiWork && childSource->objects[childObject].isWikiWork)))
										{
											matchSum += 4;
											matchInfo += L"ANSWER_MATCH_WIKITYPE[+4]";
										}
										if (areBothPlaces && questionSource->objects[parentObject].getSubType() == childSource->objects[childObject].getSubType() &&
											questionSource->objects[parentObject].getSubType() != UNKNOWN_PLACE_SUBTYPE && questionSource->objects[parentObject].getSubType() >= 0)
										{
											matchSum += 4;
											matchInfo += L"ANSWER_MATCH_OBJECT_SUBTYPE[+4]";
										}
										if (wikiTypeMatch && !match && matchSum > 16)
										{
											matchInfo += L"WIKI_OTHER_ANSWER[0]";
										}
									}
								}
							}
							enterAnswerAccumulatingIdenticalAnswers(questionSource, parentSRI, cAS(childSourceType, childSource, -1, matchSum, matchInfo, &(*childSRI), equivalenceClass, ws, wo, *rpi, false, L"", L"", 0, 0, 0, NULL), maxAnswer, answerSRIs);
						}
						if (logQuestionDetail)
						{
							wstring ps;
							if (*rpi != -1)
								childSource->prepPhraseToString(*rpi, ps);
							childSource->printSRI(L"", &(*childSRI), 0, ws, wo, ps, false, matchSum, matchInfo);
						}
					}
					if (wo < 0 || childSource->m[wo].nextCompoundPartObject < 0 || wo >= childSource->m.size()) break;
				}
				break;
				//if (vi < 0 || childSource->m[vi].nextCompoundPartObject < 0 || vi >= childSource->m.size()) break;
			}
			if (ws<0 || childSource->m[ws].nextCompoundPartObject<0 || ws>=childSource->m.size()) break;
		}
	}
  wprintf(L"PROGRESS: 100%% child relations processed with %04d seconds elapsed \r",(int)((clock()-startClockTime)/CLOCKS_PER_SEC));
	if (childSource->answerContainedInSource==0 && childSource->numSearchedInMemory==1 && eraseIfNoAnswers)
	{
		unordered_map <wstring,cSource *>::iterator smi=sourcesMap.find(childSource->sourcePath);
		if (smi!=sourcesMap.end())
		{
			cSource *source=smi->second;
			sourcesMap.erase(smi);
			if (source->updateWordUsageCostsDynamically)
				cWord::resetUsagePatternsAndCosts(source->debugTrace);
			else
				cWord::resetCapitalizationAndProperNounUsageStatistics(source->debugTrace);
			source->clearSource();
			delete source;
		}
	}
	return childSource->answerContainedInSource;
 }

 // process RDFType abstract, then go through the wikipedia source and wikipedia links in the RDF type, also going through any tables in the sources.
void cQuestionAnswering::analyzeQuestionThroughAbstractAndWikipediaFromRDFType(cSource *questionSource,wchar_t *derivation,int whereQuestionContextSuggestion,cSpaceRelation *parentSRI,
	  cTreeCat *rdfType,bool parseOnly,vector < cAS > &answerSRIs,int &maxAnswer,
		unordered_map <int,cWikipediaTableCandidateAnswers *> &wikiTableMap,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion, set <wstring> &wikipediaLinksAlreadyScanned)
{ LFS
	if (rdfType!=NULL)
	{
		int whereQuestionTypeObject=(parentSRI->questionType==unknownQTFlag) ? parentSRI->whereSubject : getWhereQuestionTypeObject(questionSource, parentSRI);
		cSource *abstractSource=NULL;
		int qMaxAnswer=-1;
		if (processAbstract(questionSource,rdfType,abstractSource,parseOnly)>=0)
		{
			analyzeQuestionFromSource(questionSource,derivation,L"abstract: "+abstractSource->sourcePath,abstractSource,parentSRI,answerSRIs,qMaxAnswer,false,mapPatternAnswer,mapPatternQuestion);
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
			cSource *wikipediaSource = NULL;
			if (processWikipedia(questionSource, whereQuestionContextSuggestion, wikipediaSource, rdfType->wikipediaLinks, I, parseOnly, wikipediaLinksAlreadyScanned) >= 0)
			{
				analyzeQuestionFromSource(questionSource,derivation, L"wikipedia:" + wikipediaSource->sourcePath, wikipediaSource, parentSRI, answerSRIs, qMaxAnswer, false, mapPatternAnswer, mapPatternQuestion);
				if (whereQuestionTypeObject >= 0)
				{
					vector < cSourceTable > wikiTables;
					addTables(questionSource, whereQuestionTypeObject, wikipediaSource, wikiTables);
					if (!wikiTables.empty())
						wikiTableMap[whereQuestionContextSuggestion] = new cWikipediaTableCandidateAnswers(wikipediaSource, wikiTables);
				}
				maxAnswer = max(maxAnswer, qMaxAnswer);
				if (qMaxAnswer>24)
					return;
			}
			cSource *wikipediaLinkSource = NULL;
			if (processWikipedia(questionSource,-1, wikipediaLinkSource, rdfType->wikipediaLinks, I, parseOnly, wikipediaLinksAlreadyScanned) >= 0)
			{
				analyzeQuestionFromSource(questionSource,derivation, L"wikipediaLink:" + wikipediaLinkSource->sourcePath, wikipediaLinkSource, parentSRI, answerSRIs, qMaxAnswer, false, mapPatternAnswer, mapPatternQuestion);
				if (whereQuestionTypeObject >= 0)
				{
					vector < cSourceTable > wikiTables;
					addTables(questionSource, whereQuestionTypeObject, wikipediaLinkSource, wikiTables);
					if (!wikiTables.empty())
						wikiTableMap[whereQuestionContextSuggestion] = new cWikipediaTableCandidateAnswers(wikipediaLinkSource, wikiTables);
				}
				maxAnswer = max(maxAnswer, qMaxAnswer);
				if (qMaxAnswer>24)
					return;
			}
		}
	}
}

bool cQuestionAnswering::checkObjectIdentical(cSource *source1,cSource *source2,int object1,int object2)
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

bool cQuestionAnswering::checkParticularPartIdentical(cSource *source1,cSource *source2,int where1,int where2)
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

// parent=US government officials / child=President George W . Bush
int cQuestionAnswering::checkParentGroup(cSource *parentSource,int parentWhere,cSource *childSource,int childWhere,int childObject,bool &synonym,int &semanticMismatch)
{ LFS
	if (parentSource->m[parentWhere].queryForm(commonProfessionForm)<0 && parentSource->m[parentWhere].getMainEntry()->second.query(commonProfessionForm)<0)
		return CONFIDENCE_NOMATCH;
  unordered_map<wstring,cSemanticMatchInfo>::iterator csmpi;
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
	wstring rdfInfoPrinted;
	// first professions
	for (unsigned int r=0; r<rdfTypes.size(); r++)
	{
		if (logQuestionDetail)
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch))
				rdfTypes[r]->logIdentity(LOG_WHERE,L"checkParentGroup",false, rdfInfoPrinted);
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
		if (!cOntology::cacheRdfTypes)
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
  questionGroupMap[childObjectString]=cSemanticMatchInfo(synonym,semanticMismatch,confidenceMatch);
	return confidenceMatch;
}

// transformSource is the transformQuestion source.
// originalQuestionSRI is the source sri that is mapped to the source (this).
// constantQuestionSRI is the transformation sri mapped to the constant question in transformSource.
// originalQuestionPattern is the pattern matched to the source (this).
// constantQuestionPattern is a pattern matched to transformSource.
// sourceMap maps each location in the original source to the source (this).
//
// copy all of transformedSRI which is stored in the transformSource to cSource (this).
// patterns has three maps:
//         map < variable name, int where > variableToLocationMap 
//         map < variable name, int length > variableToLengthMap 
//         map < int where, variable name > locationToVariableMap;
// 
void cQuestionAnswering::copySource(cSource *toSource,cSpaceRelation *constantQuestionSRI,cPattern *originalQuestionPattern,cPattern *constantQuestionPattern, unordered_map <int,int> &sourceMap, unordered_map <wstring, wstring> &parseVariables)
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
				if (w==where+length-1 && (toSource->m[toSource->m.size()-1].flags&cWordMatch::flagNounOwner))
				{
					toSource->m[toSource->m.size()-1].flags&=~(cWordMatch::flagNounOwner|cWordMatch::flagAdjectivalObject);
					toSource->getOriginalWord(toSource->m.size()-1,temp,false,false);
					lplog(LOG_WHERE|LOG_ERROR,L"word transformed %s",temp.c_str());
				}
				toSource->adjustOffsets(w,true);
			}
		}
	}
}

int	cQuestionAnswering::parseSubQueriesParallel(cSource *questionSource,cSource *childSource, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer, set <wstring> &wikipediaLinksAlreadyScanned)
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
					cSource *abstractSource = NULL;
					processAbstract(questionSource,rdfTypes[r], abstractSource, true);
					int maxPrepositionalPhraseNonMixMatch = -1;
					questionSource->ppExtensionAvailable(*si, maxPrepositionalPhraseNonMixMatch, true);
					int minPrepositionalPhraseNonMixMatch = (maxPrepositionalPhraseNonMixMatch == 0) ? 0 : 1;
					for (int I = maxPrepositionalPhraseNonMixMatch; I >= minPrepositionalPhraseNonMixMatch; I--)
					{
						// also process wikipedia entry
						cSource *wikipediaSource = NULL;
						processWikipedia(questionSource,*si, wikipediaSource, rdfTypes[r]->wikipediaLinks, I, true, wikipediaLinksAlreadyScanned);
						cSource *wikipediaLinkSource = NULL;
						processWikipedia(questionSource,-1, wikipediaLinkSource, rdfTypes[r]->wikipediaLinks, I, true, wikipediaLinksAlreadyScanned);
					}
				}
			if (!cOntology::cacheRdfTypes)
				for (unsigned int r = 0; r < rdfTypes.size(); r++)
					delete rdfTypes[r]; // now caching them
		}
		sqi->whereQuestionInformationSourceObjects = saveQISO;
	}
	return 0;
}

bool cQuestionAnswering::analyzeRDFTypes(cSource *questionSource, vector <cSpaceRelation>::iterator sri, cSpaceRelation *ssri, wstring derivation,vector < cAS > &answerSRIs, int &maxAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, unordered_map <int, cWikipediaTableCandidateAnswers * > &wikiTableMap,bool subQueryFlag)
{
	wchar_t sqderivation[1024];
	wstring tmpstr;
	bool whereQuestionInformationSourceObjectsSkipped=true;
	for (set <int>::iterator si = ssri->whereQuestionInformationSourceObjects.begin(), siEnd = ssri->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
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
		cOntology::setPreferred(topHierarchyClassIndexes, rdfTypes);
		set<wstring> preferredTypes;
		set <wstring> wikipediaLinksAlreadyScanned;
		wstring rdfInfoPrinted;
		for (unsigned int r = 0; r < rdfTypes.size(); r++)
		{
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch) && preferredTypes.find(rdfTypes[r]->typeObject) == preferredTypes.end())
			{
				preferredTypes.insert(rdfTypes[r]->typeObject);
				rdfTypes[r]->logIdentity(LOG_WHERE, (subQueryFlag) ? L"subQueries":L"processQuestionSource", false, rdfInfoPrinted);
				// find subject or object without question
				int numWords;
				wstring tmpstr2;
				StringCbPrintf(sqderivation, 1024 * sizeof(wchar_t), L"%s:%06d: informationSourceObject %s:rdfType %d:%s:", derivation.c_str(), ssri->where, questionSource->whereString(*si, tmpstr, false, 6, L" ", numWords).c_str(), r, rdfTypes[r]->toString(tmpstr2).c_str());
				analyzeQuestionThroughAbstractAndWikipediaFromRDFType(questionSource, sqderivation, *si, ssri, rdfTypes[r], false, answerSRIs, maxAnswer, wikiTableMap, mapPatternAnswer, mapPatternQuestion, wikipediaLinksAlreadyScanned);
				whereQuestionInformationSourceObjectsSkipped = false;
			}
			else 
				rdfTypes[r]->logIdentity(LOG_WHERE, (subQueryFlag) ? L"subQueriesNP":L"processQuestionSourceNP", false, rdfInfoPrinted);
		}
		if (preferredTypes.empty() && rdfTypes.size() > 0)
		{
			lplog(LOG_WHERE, L"%s:SKIPPED - NO PREFERRED RDF TYPES (%d)", sqderivation, rdfTypes.size());
			for (unsigned int r = 0; r < rdfTypes.size(); r++)
				if (rdfTypes[r]->cli->first != SEPARATOR)
					rdfTypes[r]->logIdentity(LOG_WHERE, L"NO PREFERRED RDF:", false, rdfInfoPrinted);
		}
		if (!cOntology::cacheRdfTypes)
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
int	cQuestionAnswering::matchSubQueries(cSource *questionSource,wstring derivation,cSource *childSource,int &anySemanticMismatch,bool &subQueryNoMatch,vector <cSpaceRelation> &subQueries,
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
		unordered_map <int, cWikipediaTableCandidateAnswers * > wikiTableMap;
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



int cQuestionAnswering::questionTypeCheck(cSource *questionSource,wstring derivation, cSpaceRelation* parentSRI, cAS &childCAS, int &semanticMismatch, bool &unableToDoQuestionTypeCheck)
{
	LFS
	unableToDoQuestionTypeCheck = true;
	// attempt to realign question type (does question require a person, a place, a business, a book album or song, or a time)?
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
		parentSRI->questionType= qt|typeQTMask;
	}
	else if (qt != whereQTFlag && qt != whoseQTFlag && qt != whenQTFlag && qt != whomQTFlag)
		return CONFIDENCE_NOMATCH;
	unableToDoQuestionTypeCheck = false;
	// set whereChildCandidateAnswer and object
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
	// compare candidate answer to question type (if we are asking for a person, is the childCAS candidate answer a person or gendered object?)
	int confidence=childCAS.source->checkParticularPartQuestionTypeCheck(qt,childWhere,childObject,semanticMismatch);
	wstring tmpstr;
	if (logQuestionDetail)
		lplog(LOG_WHERE, L"checkParticularPartQuestionTypeCheck: %d compared with %s yields matchValue %d", qt, questionSource->objectString(childObject, tmpstr, false).c_str(), confidence);
	if (semanticMismatch)
	{
		wstring ps;
		childCAS.source->prepPhraseToString(childCAS.wp,ps);
		childCAS.source->printSRI(L"questionTypeCheck semanticMismatch",childCAS.sri,0,childCAS.ws,childCAS.wo,ps,false,-1,L"");
	}
	return confidence;
}

int cQuestionAnswering::semanticMatch(cSource *questionSource, wstring derivation,cSpaceRelation* parentSRI,cAS &childCAS,int &semanticMismatch)
{ LFS
	int semMatchValue=1;
	bool synonym=false;
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
	return semMatchValue;
}

// this is called from the parent
int cProximityMap::cProximityEntry::semanticCheck(cQuestionAnswering &qa,cSpaceRelation* parentSRI,cSource *parentSource)
{ LFS
	if (confidenceCheck && childSource==0)
	{
		qa.processPath(parentSource,lastChildSourcePath.c_str(),childSource,cSource::WEB_SEARCH_SOURCE_TYPE,1,false);
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
//		confidence=parentSource->semanticMatchSingle(L"accumulateProximityEntry",parentSRI,childSource,childWhere2,childObject,semanticMismatch,subQueryNoMatch,subQueries,-1,mapPatternAnswer,mapPatternQuestion);
	}
	return confidenceSE=CONFIDENCE_NOMATCH;
}

cProximityMap::cProximityEntry::cProximityEntry()
{
	inSource = 0;
	totalDistanceFromObject = 0;
	directRelation = 0;
	confidentInSource = 0;
	confidentTotalDistanceFromObject = 0;
	confidentDirectRelation = 0;
	confidenceSE = 0;
	childWhere2 = 0;
	childSource = 0;
	score = 0.0;
	semanticMismatch = 0;
	subQueryNoMatch = false;
	tenseMismatch = false;
	confidenceCheck = false;
}

cProximityMap::cProximityEntry::cProximityEntry(cSource *childSource, unsigned int childSourceIndex, int childObject, cSpaceRelation* parentSRI) : cProximityEntry()
{
	int qt = parentSRI->questionType&cQuestionAnswering::typeQTMask;
	bool parentQuestionTypeValid = ((parentSRI->questionType&cQuestionAnswering::QTAFlag) || (qt != cQuestionAnswering::whereQTFlag && qt != cQuestionAnswering::whoseQTFlag && qt != cQuestionAnswering::whenQTFlag && qt != cQuestionAnswering::whomQTFlag));
	bool questionTypeCheck = parentQuestionTypeValid && childSource->checkParticularPartQuestionTypeCheck(qt, childSourceIndex, childObject, semanticMismatch);
	confidenceCheck = (questionTypeCheck || parentSRI->questionType == cQuestionAnswering::unknownQTFlag);
	lastChildSourcePath = childSource->sourcePath;
	childWhere2 = childSourceIndex;
	childSource->objectString(childObject, fullDescriptor, false);
	if (childSource->objects[childObject].end - childSource->objects[childObject].begin == 1)
	{
		wstring formWinnerStr;
		fullDescriptor += childSource->m[childSource->objects[childObject].begin].winnerFormString(formWinnerStr);
	}
	childObject = childObject;
}

int cQuestionAnswering::verbTenseMatch(cSource *questionSource, cSpaceRelation* parentSRI, cAS &childCAS)
{
	if (parentSRI->whereVerb < 0 || childCAS.sri->whereVerb < 0)
		return 6;
	int questionVerbSense = questionSource->m[parentSRI->whereVerb].verbSense&~(VT_EXTENDED|VT_PASSIVE|VT_POSSIBLE|VT_VERB_CLAUSE), childVerbSense = childCAS.source->m[childCAS.sri->whereVerb].verbSense&~(VT_EXTENDED | VT_PASSIVE | VT_POSSIBLE | VT_VERB_CLAUSE);
	int tenseMatchReason = 0;
	if ((questionVerbSense&VT_NEGATION) ^ (childVerbSense&VT_NEGATION))
		childCAS.rejectAnswer += (questionVerbSense&VT_NEGATION) ? L"[question is negated and child is not]" : L"[question is not negated and child is negated]";
	// VT_PRESENT/VT_PAST/VT_PRESENT_PERFECT/VT_PAST_PERFECT/VT_FUTURE/VT_FUTURE_PERFECT
	// if child and parent are matched in tense
	else if ((parentSRI->isConstructedRelative) ? childVerbSense == VT_PAST : questionVerbSense == childVerbSense)
		tenseMatchReason = 1;
	// has won and won are the same in this context
	// (pvs==VT_PRESENT && cvs==VT_PRESENT_PERFECT) - parent is On what TV show does Hammond regularly appear child is 'appeared eight  [O eight times[109-111]{WO:eight}[110][nongen][N][PL][OGEN]]on   [PO (Saturday Night Live[26-29][26][ngname][N][WikiWork]-4620) show]'
	else if (((!questionSource->sourceInPast && !childCAS.source->sourceInPast) || (questionSource->sourceInPast && childCAS.source->sourceInPast)) && (questionVerbSense == VT_PRESENT_PERFECT && childVerbSense == VT_PAST) || (childVerbSense == VT_PRESENT_PERFECT && questionVerbSense == VT_PAST))
		tenseMatchReason = 2;
	// if questionSource in past, and childSource not in past, then the only problem would be if the child said it would be in the future.
	else if (questionSource->sourceInPast && !childCAS.source->sourceInPast && childVerbSense != VT_FUTURE && childVerbSense != VT_FUTURE_PERFECT)
		tenseMatchReason = 3;
	// if questionSource in present, and childSource in past, then the only problem would be if the parent said it would be in the future.
	else if (!questionSource->sourceInPast && childCAS.source->sourceInPast && questionVerbSense != VT_FUTURE && questionVerbSense != VT_FUTURE_PERFECT)
		tenseMatchReason = 4;
	// PARENT: what     [S Darrell Hammond]  featured   on [PO Comedy Central program[12-15][14][nongen][N]]?
	// CHILD: [S Darrell Hammond]  was [O a featured cast member[67-71][70][nongen][N]] on   [PO Saturday Night Live[72-75][72][ngname][N][WikiWork]].
	else if (childCAS.matchInfo.find(L"MOVED_VERB_TO_OBJECT") != wstring::npos)
		tenseMatchReason = 5;
	else
	{
		wstring tmpstr1, tmpstr2;
		lplog(LOG_WHERE, L"tense mismatch between parent=%d:%s and child=%d:%s.",
			parentSRI->whereVerb, senseString(tmpstr1, questionSource->m[parentSRI->whereVerb].verbSense).c_str(), childCAS.sri->whereVerb, senseString(tmpstr2, childCAS.source->m[childCAS.sri->whereVerb].verbSense).c_str());
		wstring questionVerbSenseString, childVerbSenseString;
		if (parentSRI->isConstructedRelative)
			childCAS.rejectAnswer += L"[QuestionIsConstructedRelative]";
		childCAS.rejectAnswer += (questionSource->sourceInPast) ? L"[question source in past]":L"";
		childCAS.rejectAnswer += (childCAS.source->sourceInPast) ? L"[child source in past]":L"";
		childCAS.rejectAnswer += L"[questionVerbSense=" + senseString(questionVerbSenseString, questionVerbSense) + L" childVerbSense=" + senseString(childVerbSenseString, childVerbSense)+L"]";
	}
	return tenseMatchReason;
}

void cProximityMap::cProximityEntry::printDirectRelations(cQuestionAnswering &qa, int logType,cSource *parentSource,wstring &path,int where)
{ LFS
	if (qa.processPath(parentSource,path.c_str(),childSource,cSource::WEB_SEARCH_SOURCE_TYPE,1,false)>=0)
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

int cQuestionAnswering::semanticMatchSingle(cSource *questionSource, wstring derivation,cSpaceRelation* parentSRI,cSource *childSource,int whereChild,int childObject,int &semanticMismatch,bool &subQueryNoMatch,
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
bool cQuestionAnswering::checkIdentical(cSource *questionSource, cSpaceRelation* sri,cAS &cas1,cAS &cas2)
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

void cQuestionAnswering::setWhereChildCandidateAnswer(cSource *questionSource,cAS &childCAS, cSpaceRelation* parentSRI)
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

int cQuestionAnswering::getWhereQuestionTypeObject(cSource *questionSource,cSpaceRelation* sri)
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

bool cQuestionAnswering::processPathToPattern(cSource *questionSource,const wchar_t *path,cSource *&source)
{ LFS
		source=new cSource(&questionSource->mysql,cSource::PATTERN_TRANSFORM_TYPE,0);
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
void cQuestionAnswering::initializeTransformations(cSource *questionSource,unordered_map <wstring, wstring> &parseVariables)
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
bool cQuestionAnswering::detectTransitoryAnswer(cSource *questionSource,cSpaceRelation* sri,cSpaceRelation * &ssri,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion)
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
bool cQuestionAnswering::isQuestionPassive(cSource *questionSource,vector <cSpaceRelation>::iterator sri,cSpaceRelation * &ssri)
{ LFS
		//cSpaceRelation(int _where,int _o,int _whereControllingEntity,int _whereSubject,int _whereVerb,int _wherePrep,int _whereObject,
		//             int _wherePrepObject,int _movingRelativeTo,int _relationType,
		//							bool _genderedEntityMove,bool _genderedLocationRelation,int _objectSubType,int _prepObjectSubType,bool _physicalRelation)
	ssri=&(*sri);
	// detect Q1PASSIVE
	int maxEnd;
	if (sri->whereQuestionType<0)
		return false;
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
			return false;
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
		vector < vector <cTagLocation> > tagSets;
		questionSource->startCollectTags(false, subjectVerbRelationTagSet, sri->whereSubject, questionSource->m[sri->whereSubject].pma[verbPhraseElement&~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true,L"passive clause detection");
		// not reachable?
			//for (unsigned int J=0; J<tagSets.size(); J++)
			//{
			//	printTagSet(LOG_WHERE,L"QR",J,tagSets[J],sri->whereSubject,m[sri->whereSubject].pma[verbPhraseElement&~cMatchElement::patternFlag].pemaByPatternEnd);
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
	return true;
}

//   A. Which old man who ran in the Olympics won the prize? [Jenkins ran in what?] candidate answer Jenkins what==Olympics
//   B. Which prize originating in Spain did he win? [The prize originated in where? (the verb tense matches the main verb tense)] where=Spain
// 1. detect subqueries (after each object at whereQuestionType, detect a infinitive or a relative clause)
//      A. who ran in the Olympics
//      B. originating in Spain
// 2. collect subqueries
void cQuestionAnswering::detectSubQueries(cSource *questionSource, vector <cSpaceRelation>::iterator sri,vector <cSpaceRelation> &subQueries)
{ LFS
	//A. Which old man who ran in the Olympics won the prize?
	// B. Which prize originating in Spain did he win? [The prize originated in where? (the verb tense matches the main verb tense)] where=Spain
	int collectionWhere = sri->whereQuestionTypeObject;
	if (collectionWhere<0) return;
	int rh= questionSource->m[collectionWhere].endObjectPosition,relVerb,relPrep,relObject;
	if (rh>=0 && (((questionSource->m[rh].flags&cWordMatch::flagRelativeHead) && (relVerb= questionSource->m[rh].getRelVerb())>=0) || questionSource->detectAttachedPhrase(sri,relVerb)>=0))
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

bool cQuestionAnswering::enterAnswerAccumulatingIdenticalAnswers(cSource *questionSource,cSpaceRelation *sri,cAS candidateAnswer, int &maxAnswer, vector < cAS > &answerSRIs)
{
	vector <int> identicalAnswers;
	for (unsigned int I = 0; I < answerSRIs.size(); I++)
		if (checkIdentical(questionSource, &(*sri), answerSRIs[I], candidateAnswer) && answerSRIs[I].equivalenceClass == candidateAnswer.equivalenceClass)
			identicalAnswers.push_back(I);
	for (unsigned int I = 0; I < identicalAnswers.size(); I++)
		answerSRIs[identicalAnswers[I]].numIdenticalAnswers = identicalAnswers.size();
	maxAnswer = max(maxAnswer, candidateAnswer.matchSum);
	answerSRIs.push_back(candidateAnswer);
	candidateAnswer.source->answerContainedInSource++;
	return true;
}

/*
From dbPedia:
Paul Robin Krugman is an American economist, professor of Economics and International Affairs at the Woodrow Wilson School of Public and International Affairs at Princeton University
From wikipedia:
He taught at Yale University, MIT, UC Berkeley, the London School of Economics, and Stanford University before joining Princeton University in 2000 as professor of economics and international affairs.
*/
int cQuestionAnswering::determineBestAnswers(cSource *questionSource, cSpaceRelation*  sri,vector < cAS > &answerSRIs,
	int maxAnswer, vector <cSpaceRelation> &subQueries, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery)
{
	LFS
	if (maxAnswer <= 14)
		return 0;
	lplog(LOG_WHERE, L"(maxCertainty=%d):", maxAnswer);
	int lowestConfidence = 1000000;
	int highestIdenticalAnswers = -1;
	int lowestSourceConfidence = 1000000;
	int maxRecomputedAnswer = -1, maxAlternativeAnswer = -1;
	for (unsigned int I = 0; I < answerSRIs.size(); I++)
	{
		vector <cAS>::iterator as = answerSRIs.begin() + I;
		as->matchSumWithConfidenceAndNumIdenticalAnswersScored = -1;
		setWhereChildCandidateAnswer(questionSource, *as, &(*sri));
		// ************************************
		// check all answers that are most likely
		// ************************************
		if (as->matchSum >= 14 && as->sri->whereChildCandidateAnswer >= 0)
		{
			bool unableToDoQuestionTypeCheck = true;
			int semanticMismatch = 0;
			wchar_t derivation[1024];
			StringCbPrintf(derivation, 1024 * sizeof(wchar_t), L"PW %06d:child answerSRI %d", sri->where, as->sri->where);
			as->confidence = questionTypeCheck(questionSource, derivation, &(*sri), *as, semanticMismatch, unableToDoQuestionTypeCheck);
			// if questionTypeCheck said:
			// CONFIDENCE_NOMATCH: no chance this could be the answer
			// 1: definitely matches the type of the answer
			// !QTAFlag - no semantic match to a where or who because where or who has nothing it is modifying so questionTypeCheck has done all the work possible
			if ((sri->questionType&QTAFlag) && (unableToDoQuestionTypeCheck || (as->confidence != CONFIDENCE_NOMATCH && (as->confidence != 1 || sri->questionType == unknownQTFlag))))
				as->confidence = semanticMatch(questionSource, derivation, &(*sri), *as, semanticMismatch);
			else
				if (unableToDoQuestionTypeCheck)
					as->confidence = CONFIDENCE_NOMATCH / 2; // unable to do any semantic checking
			bool subQueryNoMatch = false;
			if (!semanticMismatch)
			{
				as->confidence = matchSubQueries(questionSource, derivation, as->source, semanticMismatch, subQueryNoMatch, subQueries, as->sri->whereChildCandidateAnswer, -1, I, as->confidence, mapPatternAnswer, mapPatternQuestion, useParallelQuery);
				wstring tmpstr1, tmpstr2;
				int numWords = 0;
				lplog(LOG_WHERE, L"%d:subquery comparison whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d (1)", I - 1,
					as->sri->whereChildCandidateAnswer, as->source->whereString(as->sri->whereChildCandidateAnswer, tmpstr2, false, 6, L" ", numWords).c_str(),
					semanticMismatch, (subQueryNoMatch) ? L"true" : L"false", as->confidence);
			}
			int tenseMatchReason = verbTenseMatch(questionSource, &(*sri), *as);
			if (isModifiedGeneric(*as))
				as->confidence = CONFIDENCE_NOMATCH / 2;
			if (as->confidence < CONFIDENCE_NOMATCH && (tenseMatchReason>0 || as->confidence == 1 || as->matchSum > 20) && !semanticMismatch && !subQueryNoMatch)
			{
				if (tenseMatchReason==0)
					as->confidence = CONFIDENCE_NOMATCH / 2;
				lowestConfidence = min(lowestConfidence, as->confidence);
				lowestSourceConfidence = min(lowestSourceConfidence, as->source->sourceConfidence);
				highestIdenticalAnswers = max(highestIdenticalAnswers, as->numIdenticalAnswers);
				maxRecomputedAnswer = max(maxRecomputedAnswer, as->matchSum);
				lplog(LOG_WHERE, L"(maxRecomputedAnswer=%d [%d %d %d %d %d %s %s %d]):", maxRecomputedAnswer, answerSRIs.size(), as->confidence,tenseMatchReason,as->confidence,as->matchSum,(semanticMismatch) ? L"semanticMismatch":L"",(subQueryNoMatch) ? L"subQueryNoMatch":L"", tenseMatchReason);
			}
			else
			{
				if (as->matchSum <= 16) as->rejectAnswer += L"[matchSum too low]";
				as->rejectAnswer += (sri->questionType&QTAFlag) ? L"[adjectival]" : L"[not adjectival]";
				as->rejectAnswer += (sri->questionType == unknownQTFlag) ? L"[unknown QT]" : L"[known QT]";
				as->rejectAnswer += (tenseMatchReason) ? L"[tense Match]" : L"[tense mismatch]";
				as->rejectAnswer += (semanticMismatch) ? L"[semantic Mismatch]" : L"[semantic match]";
				as->rejectAnswer += (subQueryNoMatch) ? L"[subQuery NoMatch]" : L"[subQuery match]";
				maxAlternativeAnswer = max(maxAlternativeAnswer, as->matchSum);
				lplog(LOG_WHERE, L"(maxAlternativeAnswer=%d [%d %d %d %d %d %s %s]):", maxAlternativeAnswer, answerSRIs.size(), as->confidence, tenseMatchReason, as->confidence, as->matchSum, (semanticMismatch) ? L"semanticMismatch" : L"", (subQueryNoMatch) ? L"subQueryNoMatch" : L"");
			}
		}
		else
		{
			if (maxAnswer > as->matchSum)
				maxAlternativeAnswer = max(maxAlternativeAnswer, as->matchSum);
			as->rejectAnswer += L"[matchSum too low]";
			lplog(LOG_WHERE, L"(maxAlternativeAnswer (2)=%d [%d]):", maxAlternativeAnswer, answerSRIs.size());
		}
	}
	lplog(LOG_WHERE, L"(final maxRecomputedAnswer=%d [%d]):", maxRecomputedAnswer, answerSRIs.size());
	if (maxRecomputedAnswer < 0)
		return 0;
	bool highCertaintyAnswer = false;
	int maxRecomputedAnswerWithConfidenceAndNumIdenticalAnswersScored=-1;
	for (unsigned int I = 0; I < answerSRIs.size(); I++)
	{
		vector <cAS>::iterator as = answerSRIs.begin() + I;
		if (maxRecomputedAnswer == as->matchSum)
		{
			highCertaintyAnswer |= as->numIdenticalAnswers == highestIdenticalAnswers && as->confidence == lowestConfidence &&
				as->source->sourceConfidence == lowestSourceConfidence && lowestConfidence < CONFIDENCE_NOMATCH / 2 &&
				(!as->subQueryExisted || as->subQueryMatch) && !isModifiedGeneric(*as);
			as->matchSumWithConfidenceAndNumIdenticalAnswersScored = as->matchSum;
			if (as->numIdenticalAnswers == highestIdenticalAnswers)
				as->matchSumWithConfidenceAndNumIdenticalAnswersScored += 4;
			if (as->confidence == lowestConfidence)
				as->matchSumWithConfidenceAndNumIdenticalAnswersScored += 6;
			maxRecomputedAnswerWithConfidenceAndNumIdenticalAnswersScored = max(maxRecomputedAnswerWithConfidenceAndNumIdenticalAnswersScored, as->matchSumWithConfidenceAndNumIdenticalAnswersScored);
		}
	}
	int numFinalAnswers = 0;
	vector <int> finalAnswers;
	for (unsigned int I = 0; I < answerSRIs.size(); I++)
	{
		vector <cAS>::iterator as = answerSRIs.begin() + I;
		if (maxRecomputedAnswerWithConfidenceAndNumIdenticalAnswersScored == as->matchSumWithConfidenceAndNumIdenticalAnswersScored && (!highCertaintyAnswer || as->source->sourceConfidence == lowestSourceConfidence))
		{
			bool identicalFound = false;
			for (unsigned int fa = 0; fa < finalAnswers.size() && !identicalFound; fa++)
				if (checkIdentical(questionSource, &(*sri), answerSRIs[I], answerSRIs[finalAnswers[fa]]) && answerSRIs[I].equivalenceClass == answerSRIs[finalAnswers[fa]].equivalenceClass)
					identicalFound = true;
			if (!identicalFound)
			{
				as->finalAnswer = true;
				numFinalAnswers++;
				finalAnswers.push_back(I);
			}
			else
				as->rejectAnswer += L"identical final answer ";
		}
		else
		{
			wstring tmpstr1, tmpstr2;
			if (as->numIdenticalAnswers < highestIdenticalAnswers) 
				as->rejectAnswer += L"low identical answers " + itos(as->numIdenticalAnswers, tmpstr1) + L"<" + itos(highestIdenticalAnswers, tmpstr2) + L" ";
			if (as->confidence > lowestConfidence) 
				as->rejectAnswer += L"confidence " + itos(as->confidence, tmpstr1) + L">" + itos(lowestConfidence, tmpstr2) + L" ";
			if (as->matchSum< maxRecomputedAnswer)
				as->rejectAnswer += L"match low " + itos(as->matchSum, tmpstr1) + L"<" + itos(maxRecomputedAnswer, tmpstr2) + L" ";
			if (as->source->sourceConfidence > lowestSourceConfidence)
				as->rejectAnswer += L"source confidence " + itos(as->source->sourceConfidence, tmpstr1) + L">" + itos(lowestSourceConfidence, tmpstr2) + L" ";
		}
	}
	return numFinalAnswers;
}

bool cQuestionAnswering::isModifiedGeneric(cAS &as)
{
	if (as.sri!=0 && as.sri->whereChildCandidateAnswer >= 0 && as.source->m[as.sri->whereChildCandidateAnswer].objectMatches.empty() &&
		as.source->m[as.sri->whereChildCandidateAnswer].beginObjectPosition>=0 && as.source->m[as.source->m[as.sri->whereChildCandidateAnswer].beginObjectPosition].word->first == L"a")
		return true;
	return false;
}

int cQuestionAnswering::printAnswers(cSpaceRelation*  sri,vector < cAS > &answerSRIs)
{ LFS
	int numFinalAnswers=0;
	if (answerSRIs.size()>0)
	{
		lplog(LOG_WHERE,L"P%06d:ANSWER LIST  ************************************************************",sri->where);
		for (unsigned int J=0; J< answerSRIs.size(); J++)
		{
			vector <cAS>::iterator as = answerSRIs.begin() + J;
			if (as->finalAnswer)
			{
				lplog(LOG_WHERE | LOG_QCHECK, L"    ANSWER %d:Identical Answers:%d:Object Match Confidence:%d:Source:%s:Source Confidence:%d", J, answerSRIs[J].numIdenticalAnswers, answerSRIs[J].confidence, as->sourceType.c_str(), as->source->sourceConfidence);
				if (as->sri)
				{
					wstring ps;
					as->source->prepPhraseToString(as->wp, ps);
					as->source->printSRI(L"        ", as->sri, 0, as->ws, as->wo, ps, false, as->matchSum, as->matchInfo, LOG_WHERE | LOG_QCHECK);
				}
				else
				{
					wstring tmpstr;
					lplog(LOG_WHERE | LOG_QCHECK, L"       [TABLE %s %s:%d:%d:%d] %d:%s", as->tableNum.c_str(), as->tableName.c_str(), as->columnIndex, as->rowIndex, as->entryIndex, as->ws, as->source->phraseString(as->entry.begin, as->entry.begin+ as->entry.numWords, tmpstr, true).c_str());
				}
				numFinalAnswers++;
			}
		}
		for (unsigned int J = 0; J < answerSRIs.size(); J++)
		{
			vector <cAS>::iterator as=answerSRIs.begin()+J;
			if (!as->finalAnswer)
			{
				lplog(LOG_WHERE ,L"  REJECTED (%s%s) %d:Identical Answers:%d:Object Match Confidence:%d:Source:%s:Source Confidence:%d",
					as->rejectAnswer.c_str(),as->matchInfo.c_str(),J,as->numIdenticalAnswers,as->confidence,as->sourceType.c_str(),as->source->sourceConfidence);
				if (as->sri)
				{
					wstring ps;
					as->source->prepPhraseToString(as->wp, ps);
					as->source->printSRI(L"      ", as->sri, 0, as->ws, as->wo, ps, false, as->matchSum, as->matchInfo, LOG_WHERE );
				}
				else
				{
					wstring tmpstr;
					lplog(LOG_WHERE, L"       [TABLE %s %s:%d:%d:%d] %d:%s", as->tableNum.c_str(), as->tableName.c_str(), as->columnIndex, as->rowIndex, as->entryIndex, as->ws, as->source->phraseString(as->entry.begin, as->entry.begin + as->entry.numWords, tmpstr, true).c_str());
				}
			}
		}
	}
	else
		lplog(LOG_WHERE, L"P%06d:ANSWER LIST EMPTY! ************************************************************", sri->where);
	return numFinalAnswers;
}

int	cQuestionAnswering::searchTableForAnswer(cSource *questionSource,wchar_t derivation[1024],cSpaceRelation* sri, unordered_map <int,cWikipediaTableCandidateAnswers * > &wikiTableMap,
	                               vector <cSpaceRelation> &subQueries,vector < cAS > &answerSRIs,int &minConfidence,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,bool useParallelQuery)
{ LFS
	// look for the questionType object and its synonyms in the freebase properties of the questionInformationSourceObject.
	int whereQuestionTypeObject=sri->whereQuestionTypeObject,numConsideredParentAnswer=0;
	if (whereQuestionTypeObject<0) return -1;
	wstring tmpstr;
	int numTableAttempts=0,numWords,maxAnswer=-1;
	unordered_map <int,cWikipediaTableCandidateAnswers * > ::iterator wtmi;
	for (set <int>::iterator si=sri->whereQuestionInformationSourceObjects.begin(),siEnd=sri->whereQuestionInformationSourceObjects.end(); si!=siEnd; si++)
	{
		if ((wtmi=wikiTableMap.find(*si))!=wikiTableMap.end())
		{
			// if found, for each value of the property:
			for (vector < cSourceTable >::iterator wtvi=wtmi->second->wikiQuestionTypeObjectAnswers.begin(),wtviEnd=wtmi->second->wikiQuestionTypeObjectAnswers.end(); wtvi!=wtviEnd; wtvi++)
			{
				numTableAttempts+=wtvi->columns.size();
			}
		}
		if (logTableDetail)
			lplog(LOG_WHERE,L"*No tables for answer matching %s:", questionSource->whereString(*si,tmpstr,true).c_str());
	}
	if (!numTableAttempts) 
	{
		for (unordered_map <int,cWikipediaTableCandidateAnswers * >::iterator wm=wikiTableMap.begin(),wmEnd=wikiTableMap.end(); wm!=wmEnd; wm++)
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
			for (vector < cSourceTable >::iterator tableIterator=wtmi->second->wikiQuestionTypeObjectAnswers.begin(),wtviEnd=wtmi->second->wikiQuestionTypeObjectAnswers.end(); tableIterator!=wtviEnd; tableIterator++)
			{
				int answersFromOneEntry=0,whereLastEntryEnd=-1;
				wstring tableTitle = wtmi->second->wikipediaSource->phraseString(tableIterator->tableTitleEntry.begin, tableIterator->tableTitleEntry.begin+ tableIterator->tableTitleEntry.numWords,tmpstr,false);
				tableIterator->tableTitleEntry.logEntry(LOG_WHERE, tableIterator->num.c_str(),-1,-1, tableIterator->source);
				// for each column in table
				for (vector <cColumn>::iterator columnIterator = tableIterator->columns.begin(), wtciEnd = tableIterator->columns.end(); columnIterator != wtciEnd; columnIterator++)
				{
					if (columnIterator->coherencyPercentage < 50)
					{
						columnIterator->logColumn(LOG_WHERE, L"REJECTED - COHERENCY", tableTitle.c_str());
						continue;
					}
					columnIterator->logColumn(LOG_WHERE,L"INITIALIZE", tableTitle);
					for (vector < cColumn::cRow >::iterator rowIterator = columnIterator->rows.begin(), wtriEnd = columnIterator->rows.end(); rowIterator != wtriEnd; rowIterator++)
					{
						if (rowIterator == columnIterator->rows.begin())
						{
							continue; // skip header of column
						}
						for (vector <cColumn::cEntry>::iterator entryIterator = rowIterator->entries.begin(), wtiEnd = rowIterator->entries.end(); entryIterator != wtiEnd; entryIterator++)
						{
							wstring tmpstr1, tmpstr2, tmpstr3;
							int whereChildCandidateAnswer = entryIterator->begin;
							whereLastEntryEnd = entryIterator->begin + entryIterator->numWords;
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
								tableIterator->num.c_str(), numConsideredParentAnswer, (tableIterator->tableTitleEntry.lastWordFoundInTitleSynonyms) ? L" [matches question object]":L"",whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str());
							entryIterator->logEntry(LOG_WHERE, tableIterator->num.c_str(),(int)(rowIterator-columnIterator->rows.begin()),(int)(entryIterator-rowIterator->entries.begin()),tableIterator->source);
							int confidence = CONFIDENCE_NOMATCH;
							if (logQuestionDetail) 
								lplog(LOG_WHERE, L"processing table %s: Q%d	maxTitleFound=%d, entry RDFTypeSimplifiedToWordFoundInTitleSynonyms=%d entry lastWordFoundInTitleSynonyms=%d numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow=%d, numLastWordsFoundInTitleSynonymsInRow=%d, numSimplifiedRDFTypesFoundForRow=%d", tableIterator->num.c_str(), numConsideredParentAnswer,
									rowIterator->maxTitleFound, entryIterator->RDFTypeSimplifiedToWordFoundInTitleSynonyms, entryIterator->lastWordFoundInTitleSynonyms, 
									rowIterator->numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow, rowIterator->numLastWordsFoundInTitleSynonymsInRow, rowIterator->numSimplifiedRDFTypesFoundForRow);
							/*
									logic copied from testTitlePreference to help debug setting lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms
									rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = (rows[row].maxTitleFound > 0 && rows[row].entries[entry].RDFTypeSimplifiedToWordFoundInTitleSynonyms >= rows[row].maxTitleFound) || rows[row].entries[entry].lastWordFoundInTitleSynonyms
									if (rows[row].numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow > 1 && rows[row].numLastWordsFoundInTitleSynonymsInRow > 0)
										rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = rows[row].entries[entry].lastWordFoundInTitleSynonyms;
							*/
							wstring matchInfo;
							if (tableIterator->tableTitleEntry.matchedQuestionObject.size() || tableIterator->tableTitleEntry.synonymMatchedQuestionObject.size())
							{
								if (tableIterator->tableTitleEntry.matchedQuestionObject.size())
								{
									matchInfo += L"[title matches desired answer]";
									confidence = 3;
								}
								else if (tableIterator->tableTitleEntry.synonymMatchedQuestionObject.size())
								{
									matchInfo += L"[title matches desired answer synonym]";
									confidence = 4;
								};
								if (entryIterator->RDFTypeSimplifiedToWordFoundInTitleSynonyms)
								{
									matchInfo += L"[RDF type of entry matches title synonym]";
									confidence--;
								}
								if (entryIterator->lastWordFoundInTitleSynonyms)
								{
									matchInfo += L"[last word of entry matches title synonym]";
									confidence--;
								}
							}
							else if (tableIterator->tableTitleEntry.begin < 0)
							{
								matchInfo += L"[no table title - matching against desired answer]";
								// make it less confident than if we know the title of the table
								confidence = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, whereQuestionTypeObject, wtmi->second->wikipediaSource, whereChildCandidateAnswer, -1, synonym, semanticMismatch) + 4;
							}
							lplog(LOG_WHERE, L"processing table %s: Q%d semantic comparison FINISHED between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields confidence %d [%d] [title entry=%s].", 
								tableIterator->num.c_str(), numConsideredParentAnswer++, whereQuestionTypeObject, questionSource->whereString(whereQuestionTypeObject, tmpstr1, false).c_str(), whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str(), confidence, answersFromOneEntry, tableIterator->tableTitleEntry.sprint(tableIterator->source,tmpstr3).c_str());
							bool secondarySentencePosition = (answersFromOneEntry > 1 && (wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].objectRole&(OBJECT_ROLE | PREP_OBJECT_ROLE)));
							if (secondarySentencePosition)
							{
								matchInfo += L"[secondary sentence position]";
								confidence += CONFIDENCE_NOMATCH / 2;
							}
							if (confidence < CONFIDENCE_NOMATCH)
							{
								cAS as(L"TABLE", wtmi->second->wikipediaSource, confidence, 1, matchInfo, NULL, 1, whereChildCandidateAnswer, 0, 0, true, tableIterator->num, tableTitle, columnIterator-tableIterator->columns.end(),rowIterator-columnIterator->rows.begin(), entryIterator-rowIterator->entries.begin(),&(*entryIterator));
								if (subQueries.empty())
								{
									minConfidence = min(minConfidence, confidence);
									enterAnswerAccumulatingIdenticalAnswers(questionSource, sri, as, maxAnswer, answerSRIs);
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
								enterAnswerAccumulatingIdenticalAnswers(questionSource, sri, as, maxAnswer, answerSRIs);
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

int cQuestionAnswering::findConstrainedAnswers(cSource *questionSource, vector < cAS > &answerSRIs,vector <int> &wherePossibleAnswers)
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

bool cQuestionAnswering::matchParticularAnswer(cSource *questionSource,cSpaceRelation *ssri,int whereMatch,int wherePossibleAnswer,set <int> &addWhereQuestionInformationSourceObjects)
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

bool cQuestionAnswering::matchAnswerSourceMatch(cSource *questionSource, cSpaceRelation *ssri,int whereMatch,int wherePossibleAnswer,set <int> &addWhereQuestionInformationSourceObjects)
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

int cQuestionAnswering::matchAnswersOfPreviousQuestion(cSource *questionSource, cSpaceRelation *ssri,vector <int> &wherePossibleAnswers)
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
int cQuestionAnswering::searchWebSearchQueries(cSource *questionSource,wchar_t derivation[1024],cSpaceRelation* ssri,
	vector <cSpaceRelation> &subQueries,vector < cAS > &answerSRIs,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,
	vector <wstring> &webSearchQueryStrings,bool parseOnly,int &numFinalAnswers,int &maxAnswer,bool useParallelQuery,int &trySearchIndex,
	bool useGoogleSearch, bool &lastResultPage)
{ LFS
	StringCbPrintf(derivation,1024*sizeof(wchar_t),L"PW %06d",ssri->where);
	int webSearchQueryStringOffset =0;
	if (useParallelQuery)
		lastResultPage = webSearchForQueryParallel(questionSource, derivation, ssri, parseOnly, answerSRIs, maxAnswer, 10, trySearchIndex, useGoogleSearch, webSearchQueryStrings, webSearchQueryStringOffset, mapPatternAnswer, mapPatternQuestion)<10;
	else
		lastResultPage = webSearchForQuerySerial(questionSource, derivation, ssri, parseOnly, answerSRIs, maxAnswer, 10, trySearchIndex, useGoogleSearch, webSearchQueryStrings, webSearchQueryStringOffset, mapPatternAnswer, mapPatternQuestion) < 10;
	numFinalAnswers = determineBestAnswers(questionSource, ssri,answerSRIs,maxAnswer,subQueries,mapPatternAnswer,mapPatternQuestion,useParallelQuery);
	return numFinalAnswers;
}

void cQuestionAnswering::eraseSourcesMap()
{
	// memory optimization
	for (unordered_map <wstring, cSource *>::iterator smi = sourcesMap.begin(); smi != sourcesMap.end(); )
	{
		cSource *source = smi->second;
		source->clearSource();
		if (source->updateWordUsageCostsDynamically)
			cWord::resetUsagePatternsAndCosts(source->debugTrace);
		else
			cWord::resetCapitalizationAndProperNounUsageStatistics(source->debugTrace);
		delete source;
		sourcesMap.erase(smi);
		smi = sourcesMap.begin();
	}
}

extern int limitProcessingForProfiling;
int cQuestionAnswering::processQuestionSource(cSource *questionSource,bool parseOnly,bool useParallelQuery)
{ LFS
	int lastProgressPercent=-1,where;
	vector <int> wherePossibleAnswers;
	wstring derivation;
	for (vector <cSpaceRelation>::iterator sri= questionSource->spaceRelations.begin(), sriEnd= questionSource->spaceRelations.end(); sri!=sriEnd; sri++)
	{
		if (!sri->questionType || sri->skip)
			continue;
		eraseSourcesMap();
		childCandidateAnswerMap.clear();
		// **************************************************************
		// prepare question
		// **************************************************************
		sri->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, &(*sri));
		cSpaceRelation *ssri;
		// For which newspaper does Krugman write?
		isQuestionPassive(questionSource,sri,ssri);
		ssri->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, ssri);
		// detect How and transitory answers
		cPattern *mapPatternAnswer=NULL,*mapPatternQuestion=NULL;
		detectTransitoryAnswer(questionSource,&(*sri),ssri,mapPatternAnswer,mapPatternQuestion);
		// **************************************************************
		// log question
		// **************************************************************
		wstring ps, parentNum,tmpstr,tmpstr2;
		itos(ssri->where, parentNum);
		//if (ssri->where==69) 
			//logDatabaseDetails = logQuestionProfileTime = logSynonymDetail = logTableDetail = equivalenceLogDetail = logQuestionDetail = logProximityMap = 1;
		parentNum +=L":Q ";
		questionSource->prepPhraseToString(ssri->wherePrep,ps);
		questionSource->printSRI(parentNum,ssri,-1,ssri->whereSubject,ssri->whereObject,ps,false,-1,L"QUESTION",(ssri->questionType) ? LOG_WHERE|LOG_QCHECK : LOG_WHERE);
		// **************************************************************
		// check previous answers
		// **************************************************************
		if (wherePossibleAnswers.size())
		{
			matchAnswersOfPreviousQuestion(questionSource,ssri,wherePossibleAnswers);
			wherePossibleAnswers.clear();
		}
		// **************************************************************
		// check databases for answer.  
		// **************************************************************
		vector < cAS > answerSRIs;
		wchar_t sqderivation[4096];
		matchOwnershipDbQuery(questionSource, sqderivation, ssri);
		if (!dbSearchForQuery(questionSource, sqderivation, ssri, answerSRIs))
		{
			// **************************************************************
			// scan abstract and wikipedia articles from RDF types for the answer.
			// **************************************************************
			int maxAnswer = -1;
			unordered_map <int, cWikipediaTableCandidateAnswers * > wikiTableMap;
			analyzeRDFTypes(questionSource, sri, ssri, derivation, answerSRIs, maxAnswer, mapPatternAnswer, mapPatternQuestion, wikiTableMap, false);
			if (ssri->whereQuestionTypeObject < 0)
				continue;
			// **************************************************************
			// detect subqueries
			// **************************************************************
			vector <cSpaceRelation> subQueries;
			if (&(*sri) == ssri)
			{
				questionSource->printSRI(parentNum, &(*sri), 0, sri->whereSubject, sri->whereObject, ps, false, -1, L"");
				detectSubQueries(questionSource, sri, subQueries);
			}
			else
			{
				lplog(LOG_WHERE, L"Transformed:");
				questionSource->printSRI(parentNum, ssri, 0, ssri->whereSubject, ssri->whereObject, ps, false, -1, L"");
			}
			// **************************************************************
			// determine best answer and print them.
			// **************************************************************
			int numFinalAnswers = determineBestAnswers(questionSource, ssri, answerSRIs, maxAnswer, subQueries, mapPatternAnswer, mapPatternQuestion, useParallelQuery);
			// **************************************************************
			// if there are no answers or more than one answer is asked for and there is only one, search web first 10 results, then search the wikipredia tables (from analyzeRDFTypes), then search the rest of the web results.
			// **************************************************************
			bool answerPluralSpecification = (questionSource->m[ssri->whereQuestionTypeObject].word->second.inflectionFlags&PLURAL) == PLURAL;
			if (numFinalAnswers <= 0 || (answerPluralSpecification && numFinalAnswers <= 1))
			{
				vector <wstring> webSearchQueryStrings;
				getWebSearchQueries(questionSource, ssri, webSearchQueryStrings);
				bool lastGoogleResultPage = false, lastBINGResultPage = false, webSearchOrWikipediaTableSuccess = false;
				for (int trySearchIndex = 1; trySearchIndex<=40; trySearchIndex += 10)
				{
					vector < cAS > webSearchAnswerSRIs;
					// search google with webSearchQueryStrings, starting at trySearchIndex
					if (!lastGoogleResultPage)
					{
						searchWebSearchQueries(questionSource, sqderivation, ssri, subQueries, webSearchAnswerSRIs, mapPatternAnswer, mapPatternQuestion, webSearchQueryStrings,
							parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, true, lastGoogleResultPage);
						if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
							answerSRIs.insert(answerSRIs.end(), webSearchAnswerSRIs.begin(), webSearchAnswerSRIs.end());
						else
							printAnswers(ssri, webSearchAnswerSRIs); // print all rejected answers
						if (numFinalAnswers > 0 && (!answerPluralSpecification || numFinalAnswers >= 1))
							break;
						webSearchAnswerSRIs.clear();
					}
					// search BING with webSearchQueryStrings, starting at trySearchIndex
					if (!lastBINGResultPage)
					{
						searchWebSearchQueries(questionSource, sqderivation, ssri, subQueries, webSearchAnswerSRIs, mapPatternAnswer, mapPatternQuestion, webSearchQueryStrings,
							parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, false, lastBINGResultPage);
						if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
							answerSRIs.insert(answerSRIs.end(), webSearchAnswerSRIs.begin(), webSearchAnswerSRIs.end());
						else
							printAnswers(ssri, webSearchAnswerSRIs); // print all rejected answers
						if (numFinalAnswers > 0 && (!answerPluralSpecification || numFinalAnswers >= 1))
							break;
					}
					// search wikipedia tables after the first page of Google and BING results if no answers (or insufficient number of answers) are found.
					if (trySearchIndex == 1 && ssri->whereQuestionInformationSourceObjects.size() > 0)
					{
						vector < cAS > wikipediaTableAnswerSRIs;
						int lowestConfidence = 100000 - 1;
						searchTableForAnswer(questionSource, sqderivation, ssri, wikiTableMap, subQueries, wikipediaTableAnswerSRIs, lowestConfidence, mapPatternAnswer, mapPatternQuestion, useParallelQuery);
						bool atLeastOneFinalAnswer = false;
						for (unsigned int I = 0; I < wikipediaTableAnswerSRIs.size(); I++)
							if (wikipediaTableAnswerSRIs[I].confidence == lowestConfidence)
								atLeastOneFinalAnswer = wikipediaTableAnswerSRIs[I].finalAnswer = true;
							else
								wikipediaTableAnswerSRIs[I].rejectAnswer += L"[low confidence]";
						if (webSearchOrWikipediaTableSuccess = atLeastOneFinalAnswer)
						{
							answerSRIs.insert(answerSRIs.end(), wikipediaTableAnswerSRIs.begin(), wikipediaTableAnswerSRIs.end());
							break;
						}
						else
							printAnswers(ssri, wikipediaTableAnswerSRIs); // print all rejected answers
					}
				}
				// **************************************************************
				// if there are no answers or more than one answer is asked for and there is only one, search the first 10 results from Google and BING using more information sources from proximity mapping.
				// **************************************************************
				if (!webSearchOrWikipediaTableSuccess)
				{
					lplog(LOG_WHERE | LOG_QCHECK, L"    ***** proximity map");
					for (set <int>::iterator si = ssri->whereQuestionInformationSourceObjects.begin(), siEnd = ssri->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
					{
						unordered_map <int, cProximityMap *>::iterator msi = ssri->proximityMaps.find(*si);
						if (msi != ssri->proximityMaps.end())
						{
							msi->second->sortByFrequencyAndProximity(*this, ssri, questionSource);
							msi->second->lplogFrequentOrProximateObjects(*this, LOG_WHERE, questionSource, false);
							set < unordered_map <wstring, cProximityMap::cProximityEntry>::iterator, cProximityMap::semanticSetCompare > frequentOrProximateObjects = msi->second->frequentOrProximateObjects;
							for (set < unordered_map <wstring, cProximityMap::cProximityEntry>::iterator, cProximityMap::semanticSetCompare >::iterator sai = frequentOrProximateObjects.begin(), saiEnd = frequentOrProximateObjects.end(); sai != saiEnd; sai++)
							{
								vector < cAS > enhancedWebSearchAnswerSRIs;
								vector <wstring> enhancedWebSearchQueryStrings = webSearchQueryStrings;
								enhanceWebSearchQueries(enhancedWebSearchQueryStrings, (*sai)->first);
								int trySearchIndex = 1; 
								// google
								searchWebSearchQueries(questionSource, sqderivation, ssri, subQueries, enhancedWebSearchAnswerSRIs, mapPatternAnswer, mapPatternQuestion,enhancedWebSearchQueryStrings,
									parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, true,lastGoogleResultPage);
								if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
									answerSRIs.insert(answerSRIs.end(), enhancedWebSearchAnswerSRIs.begin(), enhancedWebSearchAnswerSRIs.end());
								else
									printAnswers(ssri, enhancedWebSearchAnswerSRIs); // print all rejected answers
								if (numFinalAnswers > 0 && (!answerPluralSpecification || numFinalAnswers >= 1))
									break;
								enhancedWebSearchAnswerSRIs.clear();
								// BING
								searchWebSearchQueries(questionSource, sqderivation, ssri, subQueries, enhancedWebSearchAnswerSRIs, mapPatternAnswer, mapPatternQuestion,enhancedWebSearchQueryStrings,
									parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, true, lastGoogleResultPage);
								if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
									answerSRIs.insert(answerSRIs.end(), enhancedWebSearchAnswerSRIs.begin(), enhancedWebSearchAnswerSRIs.end());
								else
									printAnswers(ssri, enhancedWebSearchAnswerSRIs); // print all rejected answers
								if (numFinalAnswers > 0 && (!answerPluralSpecification || numFinalAnswers >= 1))
									break;
							}
						}
						else
							lplog(LOG_WHERE | LOG_QCHECK, L"    No entries in proximity map for %s.", questionSource->whereString(*si, tmpstr, true).c_str());
					}
				}
			}
		}
		printAnswers(ssri, answerSRIs); 
		findConstrainedAnswers(questionSource,answerSRIs, wherePossibleAnswers);
		if (wherePossibleAnswers.size()>0 && ssri->whereQuestionTypeObject >= 0 && ssri->whereQuestionTypeObject<(int)questionSource->m.size())
			wherePossibleAnswers.insert(wherePossibleAnswers.begin(), ssri->whereQuestionTypeObject);
		else
			wherePossibleAnswers.clear();
		if ((where = (sri - questionSource->spaceRelations.begin()) * 100 / questionSource->spaceRelations.size()) > lastProgressPercent)
		{
			wprintf(L"PROGRESS: %03d%% questions processed with %04d seconds elapsed \r", where, clocksec());
			lastProgressPercent = where;
			questionProgress = lastProgressPercent;
		}
	}
	wprintf(L"\n%d total sources processed", (int)sourcesMap.size());
	return 0;
}

// no answers?
//   for each object that has a synonym (subject, object, propobject)
//     substitute object with synonym.  
//     run constructed query with webQueries only.
//     any more answers?
