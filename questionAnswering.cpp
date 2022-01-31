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
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "vcXML.h"
#include <wn.h>
#include "profile.h"
#include "strsafe.h"
#include "source.h"
#include "QuestionAnswering.h"

int questionProgress = -1; // shared but update is not important
bool isBookTitle(MYSQL& mysql, wstring proposedTitle);

int flushString(wstring& buffer, wchar_t* path)
{
	LFS
		//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path, __FUNCTIONW__);
		int fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0)
	{
		lplog(LOG_ERROR, L"ERROR:Cannot create path %s - %S (9).", path, sys_errlist[errno]);
		return -1;
	}
	wchar_t ch = 0xFEFF;
	_write(fd, &ch, sizeof(ch));
	_write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
	_close(fd);
	return 0;
}

// <http://rdf.freebase.com/ns/m.0zcqcv2>
wstring stripWeb(wstring& name)
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
int cQuestionAnswering::processAbstract(cSource* questionSource, cTreeCat* rdfType, cSource*& source, bool parseOnly)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\dbPediaCache", CACHEDIR) + 1;
	if (_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, L"Cannot create directory %s.", path);
	wstring typeObject(stripWeb(rdfType->typeObject));
	_snwprintf(path, MAX_LEN, L"%s\\dbPediaCache\\_%s.abstract.txt", CACHEDIR, typeObject.c_str());
	convertIllegalChars(path + pathlen);
	distributeToSubDirectories(path, pathlen, true);
	if (logTraceOpen)
		lplog(LOG_WHERE, L"TRACEOPEN %s %S", path, __FUNCTION__);
	if (_waccess(path, 0) && flushString(rdfType->abstract, path) < 0)
		return -1;
	return processPath(questionSource, path, source, cSource::WEB_SEARCH_SOURCE_TYPE, 1, parseOnly);
}

int cQuestionAnswering::processSnippet(cSource* questionSource, wstring snippet, wstring object, cSource*& source, bool parseOnly)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\webSearchCache", WEBSEARCH_CACHEDIR) + 1;
	if (_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, L"Cannot create directory %s.", path);
	_snwprintf(path, MAX_LEN, L"%s\\webSearchCache\\_%s", WEBSEARCH_CACHEDIR, object.c_str());
	convertIllegalChars(path + pathlen);
	distributeToSubDirectories(path, pathlen, true);
	path[MAX_PATH - 28] = 0; // extensions
	wcscat(path, L".snippet.txt");
	if (logTraceOpen)
		lplog(LOG_WHERE, L"TRACEOPEN %s %S", path, __FUNCTION__);
	if (_waccess(path, 0) && flushString(snippet, path) < 0)
		return -1;
	return processPath(questionSource, path, source, cSource::WEB_SEARCH_SOURCE_TYPE, 50, parseOnly);
}

int cQuestionAnswering::processWikipedia(cSource* questionSource, int principalWhere, cSource*& source, vector <wstring>& wikipediaLinks, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool parseOnly, set <wstring>& wikipediaLinksAlreadyScanned, bool removePrecedingUncapitalizedWordsFromProperNouns)
{
	LFS
		wchar_t path[1024];
	vector <wstring> lookForSubject;
	if (questionSource->getWikipediaPath(principalWhere, wikipediaLinks, path, lookForSubject, includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, removePrecedingUncapitalizedWordsFromProperNouns) < 0)
		return -1;
	if (wikipediaLinksAlreadyScanned.find(path) != wikipediaLinksAlreadyScanned.end())
		return -1;
	wikipediaLinksAlreadyScanned.insert(path);
	return processPath(questionSource, path, source, cSource::WIKIPEDIA_SOURCE_TYPE, 2, parseOnly);
}

bool cQuestionAnswering::matchObjectsByName(cSource* parentSource, vector <cObject>::iterator parentObject, cSource* childSource, vector <cObject>::iterator childObject, bool& namedNoMatch, sTrace debugTrace)
{
	LFS
		bool match = false;
	namedNoMatch = false;
	if (parentObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
		match = parentObject->nameAnyNonGenderedMatch(childSource->m, *childObject);
	else if (parentObject->objectClass == NAME_OBJECT_CLASS && childObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
		match = parentObject->nameAnyNonGenderedMatch(childSource->m, *childObject);
	else if (parentObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass == NAME_OBJECT_CLASS)
		match = childObject->nameAnyNonGenderedMatch(parentSource->m, *parentObject);
	else if (parentObject->objectClass == NAME_OBJECT_CLASS && childObject->objectClass == NAME_OBJECT_CLASS)
		match = !childObject->name.isCompletelyNull() && !parentObject->name.isCompletelyNull() && childObject->nameMatch(*parentObject, debugTrace);
	else
		return match;
	namedNoMatch = (match == false) && !childObject->name.isNull() && !parentObject->name.isNull();
	return match;
}

bool cQuestionAnswering::matchObjectsExactByName(vector <cObject>::iterator parentObject, vector <cObject>::iterator childObject, bool& namedNoMatch)
{
	LFS
		bool match = false;
	namedNoMatch = false;
	if (parentObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
		match = parentObject->nameMatchExact(*childObject);
	else if (parentObject->objectClass == NAME_OBJECT_CLASS && childObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
		match = parentObject->nameMatchExact(*childObject);
	else if (parentObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS && childObject->objectClass == NAME_OBJECT_CLASS)
		match = childObject->nameMatchExact(*parentObject);
	else if (parentObject->objectClass == NAME_OBJECT_CLASS && childObject->objectClass == NAME_OBJECT_CLASS)
		match = childObject->nameMatchExact(*parentObject);
	else
		return false;
	namedNoMatch = (match == false);
	return match;
}

bool cQuestionAnswering::matchChildSourcePositionByName(cSource* parentSource, vector <cObject>::iterator parentObject, cSource* childSource, int childWhere, bool& namedNoMatch, sTrace& debugTrace)
{
	LFS
		namedNoMatch = false;
	if (childSource->m[childWhere].objectMatches.empty())
	{
		namedNoMatch = childSource->m[childWhere].getObject() < 0;
		return childSource->m[childWhere].getObject() >= 0 &&
			matchObjectsByName(parentSource, parentObject, childSource, childSource->objects.begin() + childSource->m[childWhere].getObject(), namedNoMatch, debugTrace);
	}
	bool allNoMatch = true;
	for (unsigned int mo = 0; mo < childSource->m[childWhere].objectMatches.size(); mo++)
	{
		namedNoMatch = false;
		if (matchObjectsByName(parentSource, parentObject, childSource, childSource->objects.begin() + childSource->m[childWhere].objectMatches[mo].object, namedNoMatch, debugTrace))
		{
			if (logQuestionDetail)
			{
				wstring tmpstr1, tmpstr2;
				lplog(LOG_WHERE, L"matchChildSourcePositionByName:parentObject %s against child object %s succeeded", parentSource->objectString(parentObject, tmpstr1, true).c_str(), childSource->objectString(childSource->m[childWhere].objectMatches[mo].object, tmpstr2, true).c_str());
			}
			return true;
		}
		allNoMatch = allNoMatch && namedNoMatch;
	}
	namedNoMatch = allNoMatch;
	return false;
}

// Curveball defected
//000009:TSYM PARENT year[9-10][9][nongen][N][TimeObject] is NOT a definite object [byClass].
//000008:TSYM CHILD 1999[8-9][8][nongen][N][TimeObject] is NOT a definite object [byClass].
bool cQuestionAnswering::matchTimeObjects(cSource* parentSource, int parentWhere, cSource* childSource, int childWhere)
{
	LFS
		vector <cSyntacticRelationGroup>::iterator parentSR = parentSource->findSyntacticRelationGroup(parentWhere);
	if (parentSR == parentSource->syntacticRelationGroups.end())
		return false;
	wstring tmpstr;
	for (unsigned int t2 = 0; t2 < parentSR->timeInfo.size(); t2++)
		lplog(LOG_WHERE, L"%d:MTO parentSR %s", parentWhere, parentSR->timeInfo[t2].toString(parentSource->m, tmpstr).c_str());
	vector <cSyntacticRelationGroup>::iterator childSR = childSource->findSyntacticRelationGroup(childWhere);
	if (childSR == childSource->syntacticRelationGroups.end())
		return false;
	for (unsigned int t = 0; t < childSR->timeInfo.size(); t++)
		lplog(LOG_WHERE, L"%d:MTO childSR %s", childWhere, childSR->timeInfo[t].toString(childSource->m, tmpstr).c_str());
	for (unsigned int pt = 0; pt < parentSR->timeInfo.size(); pt++)
		for (unsigned int ct = 0; ct < childSR->timeInfo.size(); ct++)
			if (parentSR->timeInfo[pt].timeCapacity == childSR->timeInfo[ct].timeCapacity)
				return true;
	return false;
}

// compare using multiple cases: parent and child may not be an object or may be an object with or without objectMatches.
// 1. parent is not an object:
//    a. compare against child not an object.
//    b. compare against a child object and its matched objects (if any).
// 2. parent is an object:
//   a. compare 
bool cQuestionAnswering::matchAllSourcePositions(cSource* parentSource, int parentWhere, cSource* childSource, int childWhere, bool& namedNoMatch, bool& synonym, bool parentInQuestionObject, int& semanticMismatch, int& adjectivalMatch, sTrace& debugTrace)
{
	class cMatchQuality
	{
	public:
		int parentWhere;
		int childWhere;
		bool namedNoMatch;
		bool synonym;
		int semanticMismatch;
		int adjectivalMatch;
		bool overallMatch;
		cMatchQuality(int pw, int cw)
		{
			parentWhere = pw;
			childWhere = cw;
			overallMatch = namedNoMatch = synonym = false;
			semanticMismatch = 0;
			adjectivalMatch = -1;
		}
	};
	vector <cMatchQuality> mq;
	mq.push_back(cMatchQuality(parentWhere, childWhere));
	for (cOM po : parentSource->m[parentWhere].objectMatches)
		mq.push_back(cMatchQuality(parentSource->objects[po.object].originalLocation, childWhere));
	vector <cMatchQuality> onlyParentMQ = mq;
	for (auto& mqi : onlyParentMQ)
	{
		for (cOM co : childSource->m[childWhere].objectMatches)
			mq.push_back(cMatchQuality(mqi.parentWhere, childSource->objects[co.object].originalLocation));
	}
	for (auto& mqi : mq)
	{
		mqi.overallMatch = matchSourcePositions(parentSource, mqi.parentWhere, childSource, mqi.childWhere, mqi.namedNoMatch, mqi.synonym, parentInQuestionObject, mqi.semanticMismatch, mqi.adjectivalMatch, debugTrace);
		if (logQuestionDetail)
		{
			wstring ws, cs, am, sm;
			lplog(LOG_WHERE, L"matchAllSourcePositions: parent %d:%s child %d:%s %s %s %s %s %s",
				mqi.parentWhere, parentSource->whereString(mqi.parentWhere, ws, false).c_str(), mqi.childWhere, childSource->whereString(mqi.childWhere, cs, false).c_str(),
				(mqi.overallMatch) ? L"overallMatch" : L"",
				(mqi.namedNoMatch) ? L"namedNoMatch" : L"",
				(mqi.semanticMismatch) ? L"semanticMismatch" : L"", (mqi.semanticMismatch) ? itos(mqi.semanticMismatch, sm).c_str() : L"",
				(mqi.adjectivalMatch >= 0) ? L"adjectivalMatch" : L"", (mqi.adjectivalMatch >= 0) ? itos(mqi.adjectivalMatch, am).c_str() : L"",
				(mqi.synonym) ? L"synonym" : L"");
		}
	}
	vector <cMatchQuality> paMQ;
	bool atLeastOneNamedMatch = false;
	bool atLeastOneSemanticMatch = false;
	int saveSemanticMismatch = 0;
	int atLeastOneAdjectivalMatch = -1;
	for (auto& mqi : mq)
	{
		if (mqi.overallMatch)
		{
			if (mqi.adjectivalMatch >= 0)
				atLeastOneAdjectivalMatch = mqi.adjectivalMatch;
			paMQ.push_back(mqi);
		}
		else
		{
			if (!mqi.namedNoMatch)
				atLeastOneNamedMatch = true;
			if (!mqi.semanticMismatch)
				atLeastOneSemanticMatch = true;
			else
				saveSemanticMismatch = mqi.semanticMismatch;
		}
	}
	if (paMQ.empty())
	{
		namedNoMatch = !atLeastOneNamedMatch;
		semanticMismatch = !atLeastOneSemanticMatch;
		if (logQuestionDetail)
		{
			wstring sm;
			lplog(LOG_WHERE, L"matchAllSourcePositions: OVERALL %s %s %s", (namedNoMatch) ? L"namedNoMatch" : L"", (semanticMismatch) ? L"semanticMismatch" : L"", (semanticMismatch) ? itos(saveSemanticMismatch, sm).c_str() : L"");
		}
		return false;
	}
	if (atLeastOneAdjectivalMatch)
		adjectivalMatch = atLeastOneAdjectivalMatch;
	if (logQuestionDetail)
	{
		wstring am;
		lplog(LOG_WHERE, L"matchAllSourcePositions: OVERALL overallMatch %s %s", (adjectivalMatch >= 0) ? L"adjectivalMatch" : L"", (adjectivalMatch >= 0) ? itos(adjectivalMatch, am).c_str() : L"", (synonym) ? L"synonym" : L"");
	}
	return true;
}

// adjectivalMatch does NOT include owner objects (Paul's car)
// adjectivalMatch = -1 if the match operation was not performed
//                   0 if no adjectives to match
//                   1 if adjectives match
//                   2 if adjectives synonyms match
//                   3 if adjectives don't match
bool cQuestionAnswering::matchSourcePositions(cSource* parentSource, int parentWhere, cSource* childSource, int childWhere, bool& namedNoMatch, bool& synonym, bool parentInQuestionObject, int& semanticMismatch, int& adjectivalMatch, sTrace& debugTrace)
{
	LFS
		adjectivalMatch = -1;
	vector <cWordMatch>::iterator imChild = childSource->m.begin() + childWhere;
	// an unmatched pronoun matches nothing
	if (imChild->objectMatches.empty() && (imChild->queryWinnerForm(nomForm) >= 0 || imChild->queryWinnerForm(personalPronounAccusativeForm) >= 0 ||
		imChild->queryWinnerForm(personalPronounForm) >= 0 || imChild->queryWinnerForm(quantifierForm) >= 0 ||
		imChild->queryWinnerForm(possessivePronounForm) >= 0 ||
		imChild->queryWinnerForm(indefinitePronounForm) >= 0 || imChild->queryWinnerForm(pronounForm) >= 0 ||
		imChild->queryWinnerForm(demonstrativeDeterminerForm) >= 0 ||
		imChild->queryWinnerForm(reflexivePronounForm) >= 0 ||
		imChild->queryWinnerForm(reciprocalPronounForm) >= 0))
		return false;
	// narrator==0 or audience==1 objects
	if (imChild->objectMatches.size() > 0 && imChild->objectMatches[0].object < 2)
		return false;
	int childObject = (imChild->objectMatches.empty()) ? imChild->getObject() : imChild->objectMatches[0].object;
	namedNoMatch = false;
	if (parentSource->m[parentWhere].objectMatches.empty() && parentSource->m[parentWhere].getObject() >= 0 &&
		matchChildSourcePositionByName(parentSource, parentSource->objects.begin() + parentSource->m[parentWhere].getObject(), childSource, childWhere, namedNoMatch, debugTrace))
		return true;
	bool allNoMatch = true;
	for (unsigned int mo = 0; mo < parentSource->m[parentWhere].objectMatches.size(); mo++)
	{
		namedNoMatch = false;
		if (matchChildSourcePositionByName(parentSource, parentSource->objects.begin() + parentSource->m[parentWhere].objectMatches[mo].object, childSource, childWhere, namedNoMatch, debugTrace))
			return true;
		allNoMatch = allNoMatch && namedNoMatch;
	}
	if (!parentSource->m[parentWhere].objectMatches.empty())
		namedNoMatch = allNoMatch;
	// compare by synonym
	if (namedNoMatch) return false;
	int parentOwnerWhere = -1, childOwnerWhere = -1;
	bool parentIsDefiniteObject = parentSource->isDefiniteObject(parentWhere, L"PARENT", parentOwnerWhere, false);
	bool childIsDefiniteObject = childSource->isDefiniteObject(childWhere, L"CHILD", childOwnerWhere, false);
	bool matched = (!parentIsDefiniteObject && !childIsDefiniteObject);
	if (matched && logQuestionDetail)
	{
		wstring tmpstr, tmpstr2;
		lplog(LOG_WHERE, L"%d:both indefinite [no owners]: parent %d:%s child %d:%s matched=%s namedNoMatch=%s", parentWhere, parentWhere, parentSource->whereString(parentWhere, tmpstr, false).c_str(), childWhere, childSource->whereString(childWhere, tmpstr2, false).c_str(), (matched) ? L"true" : L"false", (namedNoMatch) ? L"true" : L"false");
	}
	if (parentIsDefiniteObject && childIsDefiniteObject && parentOwnerWhere >= 0 && childOwnerWhere >= 0 && (parentOwnerWhere != parentWhere && childOwnerWhere != childWhere))
	{
		wstring tmpstr, tmpstr2;
		matched = matchSourcePositions(parentSource, parentOwnerWhere, childSource, childOwnerWhere, namedNoMatch, synonym, parentInQuestionObject, semanticMismatch, adjectivalMatch, debugTrace);
		// if they are definite objects, byOwner, and the owners match
		if (logQuestionDetail)
			lplog(LOG_WHERE, L"%d:definite owners: parent %d:%s child %d:%s matched=%s namedNoMatch=%s", parentWhere, parentOwnerWhere, parentSource->whereString(parentOwnerWhere, tmpstr, false).c_str(), childOwnerWhere, childSource->whereString(childOwnerWhere, tmpstr2, false).c_str(), (matched) ? L"true" : L"false", (namedNoMatch) ? L"true" : L"false");
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
		if (parentSource->m[parentWhere].endObjectPosition > 0 && parentSource->m[parentSource->m[parentWhere].endObjectPosition - 1].word->first == imChild->word->first)
			return true;
		int confidence = -1;
		if ((confidence = childSource->checkParticularPartSemanticMatch(LOG_WHERE, childWhere, parentSource, parentWhere, -1, synonym, semanticMismatch, fileCaching)) < CONFIDENCE_NOMATCH)
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
			childSource->m[imChild->endObjectPosition - 1].word->first == parentSource->m[parentWhere].word->first)
			return true;
		// parent=persons / child = Jared Loughner
		if (childSource->objects[childObject].isWikiPerson &&
			parentSource->matchChildSourcePositionSynonym(Words.query(L"person"), parentSource, parentWhere))
			return true;
		if (childSource->objects[childObject].isWikiPlace &&
			parentSource->matchChildSourcePositionSynonym(Words.query(L"place"), parentSource, parentWhere))
			return true;
		if (childSource->objects[childObject].isWikiBusiness &&
			parentSource->matchChildSourcePositionSynonym(Words.query(L"business"), parentSource, parentWhere))
			return true;
		if (childSource->objects[childObject].isWikiWork &&
			(parentSource->matchChildSourcePositionSynonym(Words.query(L"book"), parentSource, parentWhere) ||
				parentSource->matchChildSourcePositionSynonym(Words.query(L"album"), parentSource, parentWhere) ||
				parentSource->matchChildSourcePositionSynonym(Words.query(L"song"), parentSource, parentWhere)))
			return true;
		// parent=US government officials / child=President George W . Bush
		int parentObject = parentSource->m[parentWhere].getObject();
		if (parentSource->m[parentWhere].objectMatches.size() > 0)
			parentObject = parentSource->m[parentWhere].objectMatches[0].object;
		if (parentObject >= 0 && (parentSource->objects[parentObject].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			parentSource->objects[parentObject].objectClass == GENDERED_DEMONYM_OBJECT_CLASS ||
			parentSource->objects[parentObject].objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
			parentSource->objects[parentObject].objectClass == GENDERED_RELATIVE_OBJECT_CLASS) &&
			(!childSource->objects[childObject].isWikiPerson || (childSource->objects[childObject].neuter && !childSource->objects[childObject].female && !childSource->objects[childObject].male)))
			return false;
		if (childSource->objects[childObject].isWikiPerson && childSource->objects[childObject].objectClass == NAME_OBJECT_CLASS)
			return checkParentGroup(parentSource, parentWhere, childSource, childWhere, childObject, synonym, semanticMismatch) < CONFIDENCE_NOMATCH;
		if (parentSource->checkParticularPartSemanticMatch(LOG_WHERE, parentWhere, childSource, childWhere, childObject, synonym, semanticMismatch, fileCaching) < CONFIDENCE_NOMATCH)
			return true;
		return false;
	}
	if (matched)
	{
		// but not 'which prize' and 'the prize'
		if (parentInQuestionObject && !childIsDefiniteObject && imChild->queryWinnerForm(nounForm) >= 0 && parentSource->m[parentWhere].queryWinnerForm(nounForm) >= 0)
		{
			bool isGeneric = imChild->beginObjectPosition < 0;
			if (!isGeneric && imChild->objectMatches.empty())
			{
				isGeneric = (imChild->beginObjectPosition >= 0 && imChild->endObjectPosition >= 0 &&
					(imChild->endObjectPosition - imChild->beginObjectPosition) == 2 &&
					(childSource->m[imChild->beginObjectPosition].queryWinnerForm(determinerForm) >= 0 ||
						childSource->m[imChild->beginObjectPosition].queryWinnerForm(possessiveDeterminerForm) >= 0 ||
						childSource->m[imChild->beginObjectPosition].queryWinnerForm(demonstrativeDeterminerForm) >= 0));
				// Darrell Hammond went to the same university as me. // althouse.blogspot.com/2011/.../snls-darrell-hammond-was-victim-of.htm...‎
				isGeneric |= (imChild->beginObjectPosition >= 0 && imChild->endObjectPosition >= 0 &&
					(imChild->endObjectPosition - imChild->beginObjectPosition) == 3 &&
					(childSource->m[imChild->beginObjectPosition].queryWinnerForm(determinerForm) >= 0 ||
						childSource->m[imChild->beginObjectPosition].queryWinnerForm(possessiveDeterminerForm) >= 0 ||
						childSource->m[imChild->beginObjectPosition].queryWinnerForm(demonstrativeDeterminerForm) >= 0) &&
					childSource->m[imChild->beginObjectPosition + 1].word->first == L"same");
			}
			if (!isGeneric && imChild->objectMatches.size() != 0)
			{
				int cwo = imChild->objectMatches[0].object;
				if (cwo >= 0 && childSource->objects[cwo].originalLocation >= 0)
				{
					int cw = childSource->objects[cwo].originalLocation;
					isGeneric = (childSource->m[cw].beginObjectPosition >= 0 && childSource->m[cw].endObjectPosition >= 0 &&
						(childSource->m[cw].endObjectPosition - childSource->m[cw].beginObjectPosition) == 2 &&
						(childSource->m[childSource->m[cw].beginObjectPosition].queryWinnerForm(determinerForm) >= 0 ||
							childSource->m[childSource->m[cw].beginObjectPosition].queryWinnerForm(possessiveDeterminerForm) >= 0 ||
							childSource->m[childSource->m[cw].beginObjectPosition].queryWinnerForm(demonstrativeDeterminerForm) >= 0));
				}
			}
			if (isGeneric)
			{
				if (logQuestionDetail)
				{
					wstring tmpstr, tmpstr2;
					lplog(LOG_WHERE, L"rejected generic answer: parent %s and child %s", parentSource->whereString(parentWhere, tmpstr, false).c_str(), childSource->whereString(childWhere, tmpstr2, false).c_str(), (matched) ? L"true" : L"false");
				}
				namedNoMatch = true;
				return false;
			}
		}
		if (imChild->word->first == parentSource->m[parentWhere].word->first && imChild->queryWinnerForm(nounForm) >= 0 && parentSource->m[parentWhere].queryWinnerForm(nounForm) >= 0)
		{
			wstring tmpstr, tmpstr2;
			lplog(LOG_WHERE, L"matchSourcePositions:parentObject %s against child object %s succeeded (match primary word %s)",
				parentSource->whereString(parentWhere, tmpstr, true).c_str(), childSource->whereString(childWhere, tmpstr2, true).c_str(), imChild->word->first.c_str());
			adjectivalMatch = 0;
			if (childSource->m[childWhere].beginObjectPosition < childWhere && parentSource->m[parentWhere].beginObjectPosition < parentWhere)
			{
				if (childSource->m[childWhere - 1].word->first == parentSource->m[parentWhere - 1].word->first)
					adjectivalMatch = 1;
				else
				{
					unordered_set <wstring> synonyms;
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
		if (parentSource->m[parentWhere].beginObjectPosition < 0 && imChild->beginObjectPosition < 0)
		{
			unordered_set <wstring> synonyms;
			wstring tmpstr;
			childSource->getSynonyms(imChild->word->first, synonyms, NOUN);
			if (logSynonymDetail)
				lplog(LOG_WHERE, L"TSYM [1-1] comparing %s against synonyms [%s]%s", parentSource->m[parentWhere].getMainEntry()->first.c_str(), imChild->word->first.c_str(), setString(synonyms, tmpstr, L"|").c_str());
			if (synonyms.find(parentSource->m[parentWhere].getMainEntry()->first) != synonyms.end())
				return synonym = true;
		}
		// time objects?
		if (parentSource->m[parentWhere].getObject() >= 0 && parentSource->objects[parentSource->m[parentWhere].getObject()].getIsTimeObject() &&
			childSource->m[childWhere].getObject() >= 0 && childSource->objects[childSource->m[childWhere].getObject()].getIsTimeObject() &&
			matchTimeObjects(parentSource, parentWhere, childSource, childWhere))
			return true;
		if (parentSource->m[parentWhere].objectMatches.empty() && parentSource->matchChildSourcePositionSynonym(parentSource->m[parentWhere].word, childSource, childWhere))
		{
			synonym = true;
			return synonym;
		}
		for (unsigned int mo = 0; mo < parentSource->m[parentWhere].objectMatches.size(); mo++)
		{
			int pw = -2, pmo = parentSource->m[parentWhere].objectMatches[mo].object;
			if (pmo >= (signed)parentSource->objects.size() || (pw = parentSource->objects[pmo].originalLocation) < 0 || pw >= (signed)parentSource->m.size())
				lplog(LOG_WHERE, L"ERROR: %d:offset illegal! %d %d %d %d", parentWhere, pmo, parentSource->objects.size(), pw, parentSource->m.size());
			else if ((parentSource->m[pw].endObjectPosition - parentSource->m[pw].beginObjectPosition) == 1 &&
				parentSource->matchChildSourcePositionSynonym(parentSource->m[pw].word, childSource, childWhere))
			{
				synonym = true;
				return synonym;
			}
		}
	}
	return false;
}

int cQuestionAnswering::srgMatch(cSource* questionSource, cSource* childSource, int parentWhere, int childWhere, int whereQuestionType, __int64 questionType, bool& totalMatch, wstring& matchInfoDetail, int cost, bool subQuery)
{
	LFS
		totalMatch = false;
	if (parentWhere < 0)
		return 0;
	bool inQuestionObject = questionSource->inObject(parentWhere, whereQuestionType);
	if (childWhere < 0)
		return (inQuestionObject) ? -cost : 0;
	vector <cWordMatch>::iterator imChild = childSource->m.begin() + childWhere;
	bool childIsPronoun = (imChild->queryWinnerForm(nomForm) >= 0 || imChild->queryWinnerForm(personalPronounAccusativeForm) >= 0 ||
		imChild->queryWinnerForm(personalPronounForm) >= 0 || imChild->queryWinnerForm(quantifierForm) >= 0 ||
		imChild->queryWinnerForm(possessivePronounForm) >= 0 ||
		imChild->queryWinnerForm(indefinitePronounForm) >= 0 || imChild->queryWinnerForm(pronounForm) >= 0 ||
		imChild->queryWinnerForm(demonstrativeDeterminerForm) >= 0 ||
		imChild->queryWinnerForm(reflexivePronounForm) >= 0 ||
		imChild->queryWinnerForm(reciprocalPronounForm) >= 0);
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
			return (subQuery) ? cost : -cost;
		}
		// questions with acceptable answers
		// which books did Krugman write?  'A Man for All Seasons'.  
		// these answers must come from highly local sources, of which we have none, so therefore these answers are tabled for now:
		// which books did you put on the table? The big books.  The white ones.
		// which book did you put on the table? A big book.
		unordered_set <wstring> bookSynonyms;
		questionSource->getSynonyms(L"book", bookSynonyms, NOUN);
		if (questionSource->m[parentWhere].getMainEntry()->first == L"book" || bookSynonyms.find(questionSource->m[parentWhere].getMainEntry()->first) != bookSynonyms.end())
		{
			int childObject = childSource->m[childWhere].getObject();
			if (childObject < 0)
			{
				matchInfoDetail += L"[QUESTION_OBJECT_NO_BOOK_MATCH_NO_OBJECT]";
				return -cost;
			}
			int begin = childSource->objects[childObject].begin, end = childSource->objects[childObject].end;
			bool isBeginQuote = (cWord::isDoubleQuote(childSource->m[begin].word->first[0]));
			if (begin > 0)
				isBeginQuote |= (cWord::isDoubleQuote(childSource->m[begin - 1].word->first[0]));
			bool isEndQuote = (cWord::isDoubleQuote(childSource->m[end - 1].word->first[0]));
			if (end < childSource->m.size())
				isEndQuote |= (cWord::isDoubleQuote(childSource->m[end].word->first[0]));
			if (!isBeginQuote || !isEndQuote)
			{
				matchInfoDetail += L"[QUESTION_OBJECT_NO_BOOK_MATCH_NO_QUOTE]";
				return -cost;
			}
			wstring bookTitle;
			childSource->objectString(childObject, bookTitle, true);
			if (cWord::isDoubleQuote(bookTitle[0]))
				bookTitle = bookTitle.substr(1);
			if (cWord::isDoubleQuote(bookTitle[bookTitle.size() - 1]))
				bookTitle = bookTitle.substr(0, bookTitle.size() - 1);
			if (bookTitle[bookTitle.size() - 1] == L',')
				bookTitle = bookTitle.substr(0, bookTitle.size() - 1);
			if (isBookTitle(questionSource->mysql, bookTitle))
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
		unordered_set <wstring> synonyms;
		questionSource->getSynonyms(questionSource->m[parentWhere].getMainEntry()->first, synonyms, NOUN);
		if (synonyms.find(childSource->m[childWhere].getMainEntry()->first) != synonyms.end())
		{
			matchInfoDetail += L"[QUESTION_OBJECT_SYNONYM_MATCH]";
			return (subQuery) ? cost : -cost;
		}
		if (!(questionType & QTAFlag) && questionType != unknownQTFlag)
		{
			matchInfoDetail += L"[NOT_QTA (so half cost)]";
			return cost >> 1;
		}
	}
	bool namedNoMatch = false, synonym = false;
	int semanticMismatch = 0, adjectivalMatch = -1;
	// if in subquery, wait to see if it is a match, if so, boost
	//if (inQuestionObject && questionType==unknownQTFlag) 
	//		cost<<=1;
	if (matchAllSourcePositions(questionSource, parentWhere, childSource, childWhere, namedNoMatch, synonym, inQuestionObject, semanticMismatch, adjectivalMatch, questionSource->debugTrace))
	{
		//if (inQuestionObject && questionType==unknownQTFlag) // in subquery - boost
		//	lplog(LOG_WHERE,L"%d:Boost found subquery match position - cost=%d!",childWhere,(synonym) ? cost*3/4 : cost);
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
			return cost * 3 / 4;
		return cost;
	}
	//if (inQuestionObject && questionType==unknownQTFlag) // in subquery
	//		return cost>>2;
	if (namedNoMatch)
		return -cost;
	if (semanticMismatch)
	{
		matchInfoDetail += L"[SEMANTIC_MISMATCH]";
		return -cost;
	}
	return 0;
}

int cQuestionAnswering::sriVerbMatch(cSource* parentSource, cSource* childSource, int parentWhere, int childWhere, wstring& matchInfoDetailVerb, wstring verbTypeMatch, int cost)
{
	LFS
		if (parentWhere < 0 || childWhere < 0)
		{
			matchInfoDetailVerb += L"[" + verbTypeMatch + L" where is negative]";
			return 0;
		}
	tIWMM childWord, parentWord;
	if ((childWord = childSource->m[childWhere].getMainEntry()) == (parentWord = parentSource->m[parentWhere].getMainEntry()))
	{
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb exact match]";
		return cost;
	}
	unordered_set <wstring> childSynonyms;
	wstring tmpstr;
	childSource->getSynonyms(childWord->first, childSynonyms, VERB);
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"TSYM [VERB] comparing PARENT %s against synonyms [%s]%s", parentWord->first.c_str(), childWord->first.c_str(), setString(childSynonyms, tmpstr, L"|").c_str());
	if (childSynonyms.find(parentWord->first) != childSynonyms.end())
	{
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb parent in child synonym match]";
		return cost * 3 / 4;
	}
	unordered_set <wstring> parentSynonyms;
	childSource->getSynonyms(parentWord->first, parentSynonyms, VERB);
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"%s TSYM [VERB] comparing CHILD %s against synonyms [%s]%s", verbTypeMatch.c_str(), childWord->first.c_str(), parentWord->first.c_str(), setString(parentSynonyms, tmpstr, L"|").c_str());
	if (parentSynonyms.find(childWord->first) != parentSynonyms.end())
	{
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb child in parent synonym match]";
		return cost * 3 / 4;
	}
	if ((childWord->first == L"am" && (parentWord->second.timeFlags & 31) == T_START) || (parentWord->first == L"am" && (childWord->second.timeFlags & 31) == T_START))
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%s TSYM [VERBBEGINBEING] matched CHILD %s against PARENT %s", verbTypeMatch.c_str(), childWord->first.c_str(), parentWord->first.c_str());
		matchInfoDetailVerb += L"[" + verbTypeMatch + L" verb AM match]";
		return cost / 2;
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
	lplog(LOG_WHERE, L"%s VerbNet comparing PARENT verb %s [class=%s] against CHILD verb %d:%s [class=%s]", verbTypeMatch.c_str(), parentVerb.c_str(), parentVerbClassInfo.c_str(), childWhere, childVerb.c_str(), childVerbClassInfo.c_str());
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

int cQuestionAnswering::sriPrepMatch(cSource* parentSource, cSource* childSource, int parentWhere, int childWhere, int cost)
{
	LFS
		if (parentWhere < 0 || childWhere < 0)
			return 0;
	if (childSource->m[childWhere].word == parentSource->m[parentWhere].word)
		return cost;
	return 0;
}

// transform childSRG into another form and match against parent
// equivalence class 1:
//    if subject matches and verb is a 'to BE' verb AND the object is a profession or an agentive nominalization (a noun derived from a verb [runner])
//      and the equivalent verb can be identified, match parent against equivalent verb and any prep phrases from child.
int cQuestionAnswering::equivalenceClassCheck(cSource* questionSource, cSource* childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cSyntacticRelationGroup* parentSRG, int whereChildSpecificObject, int& equivalenceClass, int matchSum)
{
	LFS
		// 'to be' form
		if (childSRG->whereVerb < 0 || !childSource->m[childSRG->whereVerb].forms.isSet(isForm)) return 0;
	// object must be a profession 'professor' or an agentive nominalization or an honorific 'general'
	if (whereChildSpecificObject < 0) return 0;
	// object length is longer than 1 word - check the last word in "Centenary Professor", not the first one.
	if (childSource->m[whereChildSpecificObject].getObject() > 0 && childSource->m[whereChildSpecificObject].endObjectPosition >= 0 &&
		(childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass == NAME_OBJECT_CLASS ||
			childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass == NON_GENDERED_NAME_OBJECT_CLASS))
		whereChildSpecificObject = childSource->m[whereChildSpecificObject].endObjectPosition - 1;
	unordered_map <wstring, set < wstring > >::iterator nvi = nounVerbMap.find(childSource->m[whereChildSpecificObject].word->first);
	// professor -> profess
	if (nvi == nounVerbMap.end())
	{
		if (logEquivalenceDetail)
			lplog(LOG_WHERE, L"%d:noun %s not found", whereChildSpecificObject, childSource->m[whereChildSpecificObject].word->first.c_str());
		return 0;
	}
	tIWMM childConvertedVerb = Words.query(*nvi->second.begin());
	if (childConvertedVerb == Words.end())
	{
		if (logEquivalenceDetail)
			lplog(LOG_WHERE, L"%d:verb %s from noun %s not found", whereChildSpecificObject, nvi->second.begin()->c_str(), childSource->m[whereChildSpecificObject].word->first.c_str());
		return 0;
	}
	tIWMM childConvertedVerbME = (childConvertedVerb->second.mainEntry == wNULL) ? childConvertedVerb : childConvertedVerb->second.mainEntry;
	wstring tmpstr;
	if (childConvertedVerbME != questionSource->m[parentSRG->whereVerb].getMainEntry())
	{
		bool match = false;
		// profess synonyms = teach...
		unordered_set <wstring> synonyms;
		childSource->getSynonyms(childConvertedVerbME->first, synonyms, VERB);
		// does the verb match any synonym?
		for (auto si = synonyms.begin(), siEnd = synonyms.end(); si != siEnd && !match; si++)
		{
			tIWMM childConvertedSynonym = Words.query(*si);
			tIWMM childConvertedSynonymME = (childConvertedSynonym == Words.end() || childConvertedSynonym->second.mainEntry == wNULL) ? childConvertedSynonym : childConvertedSynonym->second.mainEntry;
			match = (childConvertedSynonymME != Words.end() && childConvertedSynonymME == questionSource->m[parentSRG->whereVerb].getMainEntry());
		}
		if (!match)
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"%d:%s:convertedVerb %s[ME %s] nor synonyms (%s) match parentVerb %s[ME %s].", whereChildSpecificObject, childSource->m[whereChildSpecificObject].word->first.c_str(),
					childConvertedVerb->first.c_str(), childConvertedVerbME->first.c_str(), setString(synonyms, tmpstr, L"|").c_str(), questionSource->m[parentSRG->whereVerb].word->first.c_str(), questionSource->m[parentSRG->whereVerb].getMainEntry()->first.c_str());
			return 0;
		}
		if (logQuestionDetail)
			lplog(LOG_WHERE, L"%d:equivalence class check succeeded:"
				L"%s:convertedVerb %s[ME %s] synonyms (%s) matched parentVerb %s[ME %s].", whereChildSpecificObject, childSource->m[whereChildSpecificObject].word->first.c_str(),
				childConvertedVerb->first.c_str(), childConvertedVerbME->first.c_str(), setString(synonyms, tmpstr, L"|").c_str(), questionSource->m[parentSRG->whereVerb].word->first.c_str(), questionSource->m[parentSRG->whereVerb].getMainEntry()->first.c_str());
	}
	else if (logQuestionDetail)
		lplog(LOG_WHERE, L"%d:equivalence class check succeeded:"
			L"%s:convertedVerb %s[ME %s] matched parentVerb %s[ME %s].", whereChildSpecificObject, childSource->m[whereChildSpecificObject].word->first.c_str(),
			childConvertedVerb->first.c_str(), childConvertedVerbME->first.c_str(), questionSource->m[parentSRG->whereVerb].word->first.c_str(), questionSource->m[parentSRG->whereVerb].getMainEntry()->first.c_str());
	equivalenceClass = 1;
	return matchSum;
}

int cQuestionAnswering::equivalenceClassCheck2(cSource* questionSource, cSource* childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cSyntacticRelationGroup* parentSRG, int whereChildSpecificObject, int& equivalenceClass, int matchSum)
{
	LFS
		// 'to be' form
		if (childSRG->whereVerb < 0 || !childSource->m[childSRG->whereVerb].forms.isSet(isForm)) return 0;
	// object must be a profession 'professor' or an agentive nominalization or an honorific 'general'
	if (whereChildSpecificObject < 0) return 0;
	// object length is longer than 1 word - check the last word in "Centenary Professor", not the first one.
	if (childSource->m[whereChildSpecificObject].getObject() > 0 && childSource->m[whereChildSpecificObject].endObjectPosition >= 0 &&
		(childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass == NAME_OBJECT_CLASS ||
			childSource->objects[childSource->m[whereChildSpecificObject].getObject()].objectClass == NON_GENDERED_NAME_OBJECT_CLASS))
		whereChildSpecificObject = childSource->m[whereChildSpecificObject].endObjectPosition - 1;
	unordered_set <wstring> synonyms;
	childSource->getSynonyms(childSource->m[whereChildSpecificObject].word->first, synonyms, NOUN);
	for (auto si = synonyms.begin(), siEnd = synonyms.end(); si != siEnd; si++)
	{
		unordered_map <wstring, set < wstring > >::iterator nvi = nounVerbMap.find(*si);
		// teacher -> teach
		if (nvi == nounVerbMap.end())
		{
			if (logSynonymDetail)
				lplog(LOG_WHERE, L"%d:TSYM synonym noun %s not found", whereChildSpecificObject, si->c_str());
			continue;
		}
		tIWMM childConvertedVerb = Words.query(*nvi->second.begin());
		if (childConvertedVerb == Words.end())
		{
			//const wchar_t *ch=nvi->second.begin()->c_str();
			if (logSynonymDetail)
				lplog(LOG_WHERE, L"%d:SYNONYM of %s(%s):verb %s not found", whereChildSpecificObject, childSource->m[whereChildSpecificObject].word->first.c_str(), si->c_str(), nvi->second.begin()->c_str());
			continue;
		}
		tIWMM childConvertedVerbME = (childConvertedVerb->second.mainEntry == wNULL) ? childConvertedVerb : childConvertedVerb->second.mainEntry;
		if (childConvertedVerbME != questionSource->m[parentSRG->whereVerb].getMainEntry())
		{
			if (logEquivalenceDetail)
				lplog(LOG_WHERE, L"%d:convertedVerb %s[ME %s] doesn't match parentVerb %s[ME %s].", whereChildSpecificObject,
					childConvertedVerb->first.c_str(), childConvertedVerbME->first.c_str(), questionSource->m[parentSRG->whereVerb].word->first.c_str(), questionSource->m[parentSRG->whereVerb].getMainEntry()->first.c_str());
			continue;
		}
		lplog(LOG_WHERE, L"%d:equivalence class (2) check succeeded (verb %s from synonym noun %s):", whereChildSpecificObject, nvi->second.begin()->c_str(), si->c_str());
		equivalenceClass = 2;
		return matchSum;
	}
	return 0;
}

void appendSum(int sum, const wchar_t* str, wstring& matchInfo)
{
	LFS
		if (sum == 0) return;
	wstring writableStr = str;
	if (sum < 0) writableStr[0] = L'-';
	if (sum > 0) writableStr[0] = L'+';
	itos((wchar_t*)writableStr.c_str(), sum, matchInfo, L"]");
}

int cQuestionAnswering::processMetanameTagset(vector <cTagLocation>& tagSet, int whereMNE, int element, cSource* questionSource, cSource* childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cPattern*& mapPatternAnswer, cPattern*& mapPatternQuestion)
{
	childSource->printTagSet(LOG_WHERE, L"MNE", 0, tagSet, whereMNE, childSource->m[whereMNE].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
	// collect tag for each of the rest of the elements
	unordered_map <int, wstring>::iterator lvmi = mapPatternAnswer->locationToVariableMap.begin();
	lvmi++;
	int whereAnswer = -1, matchingElements = 0, answerTagLen = -1;
	vector <wstring> parameters;
	for (unsigned int e = 1; e < mapPatternAnswer->numElements(); e++, lvmi++)
	{
		if (mapPatternAnswer->getElement(e)->specificWords[0] == L".") // end with . because of sentence processing, but this is not significant.
			continue;
		if (lvmi == mapPatternAnswer->locationToVariableMap.end())
			lplog(LOG_FATAL_ERROR, L"locationToVariableMap overflow for pattern %s[%s], element %d", mapPatternAnswer->name.c_str(), mapPatternAnswer->differentiator.c_str(), e);
		int tag = findOneTag(tagSet, (wchar_t*)mapPatternAnswer->getElement(e)->specificWords[0].c_str(), -1);
		if (tag < 0) continue;
		int whereTag = tagSet[tag].sourcePosition;
		if (tagSet[tag].len > 1 && childSource->m[whereTag].principalWherePosition >= 0) // could be an adjective
			whereTag = childSource->m[whereTag].principalWherePosition;
		wstring childVarNameValue;
		childSource->whereString(whereTag, childVarNameValue, true);
		wstring varName = lvmi->second;
		if (varName != L"A")
		{
			// find location of mapPatternQuestion
			int parentWhere = mapPatternQuestion->variableToLocationMap[varName];
			wstring parentVarNameValue;
			questionSource->whereString(parentWhere, parentVarNameValue, true);
			if (childVarNameValue != parentVarNameValue)
				continue;
			matchingElements++;
			wstring tmpstr;
			lplog(LOG_WHERE, L"%d:meta matched parameter [%s]=parent [%d:%s] = child [%d:%s].", whereMNE, varName.c_str(), parentWhere, parentVarNameValue.c_str(), whereTag, childVarNameValue.c_str());
			parameters.push_back(childVarNameValue);
		}
		else
		{
			whereAnswer = whereTag;
			answerTagLen = tagSet[tag].len;
		}
	}
	if (whereAnswer != -1 && matchingElements > 0 && childSource->m[whereAnswer].getObject() >= 0 && answerTagLen == childSource->m[whereAnswer].endObjectPosition - childSource->m[whereAnswer].beginObjectPosition)
	{
		wstring childVarNameValue;
		childSource->whereString(whereAnswer, childVarNameValue, true);
		bool parameterMatchesAnswer = false;
		for (vector <wstring>::iterator pi = parameters.begin(), piEnd = parameters.end(); pi != piEnd && !parameterMatchesAnswer; pi++)
			parameterMatchesAnswer = *pi == childVarNameValue;
		if (parameterMatchesAnswer)
			return 0;
		// test only for patterns diff 8,9,G
		wstring diff = patterns[childSource->m[whereMNE].pma[element & ~cMatchElement::patternFlag].getPattern()]->differentiator;
		lplog(LOG_WHERE, L"%d:meta matched pattern %s[%s]", whereMNE, patterns[childSource->m[whereMNE].pma[element & ~cMatchElement::patternFlag].getPattern()]->name.c_str(), patterns[childSource->m[whereMNE].pma[element & ~cMatchElement::patternFlag].getPattern()]->differentiator.c_str());
		set <wstring> checkVerbs;
		switch (diff[0])
		{
		case '8':checkVerbs = { L"know",L"call",L"name" }; break;
		case '9':checkVerbs = { L"look",L"appear" }; break;
		case 'G':checkVerbs = { L"recognize" }; break;
		}
		wstring tmpstr;
		bool foundMatch = checkVerbs.empty();
		if (checkVerbs.size() && childSource->m[whereAnswer].getRelVerb() >= 0)
		{
			tIWMM childWord = childSource->m[childSource->m[whereAnswer].getRelVerb()].getMainEntry();
			for (auto& cv : checkVerbs)
			{
				if (foundMatch = (Words.gquery(cv) == childWord))
					break;
				unordered_set <wstring> checkSynonyms;
				questionSource->getSynonyms(cv, checkSynonyms, VERB);
				if (logSynonymDetail)
					lplog(LOG_WHERE, L"TSYM [VERB] comparing CHECK %s and synonyms [%s] against %s", cv.c_str(), setString(checkSynonyms, tmpstr, L"|").c_str(), childWord->first.c_str());
				if (foundMatch = (checkSynonyms.find(childWord->first) != checkSynonyms.end()))
					break;
			}
		}
		if (childSource->m[whereAnswer].getObject() >= 0 && childSource->objects[childSource->m[whereAnswer].getObject()].objectClass == NAME_OBJECT_CLASS && foundMatch)
		{
			wstring tmpstr2;
			lplog(LOG_WHERE, L"%d:meta matched answer=%s.\n%s",
				whereMNE, childSource->whereString(whereAnswer, tmpstr, false).c_str(),
				childSource->phraseString(childSRG->printMin, childSRG->printMax, tmpstr2, false).c_str());
			return whereAnswer;
		}
		else
		{
			wstring tmpstr2;
			lplog(LOG_WHERE, L"%d:meta rejected answer=%s.\n%s",
				whereMNE, childSource->whereString(whereAnswer, tmpstr, false).c_str(),
				childSource->phraseString(childSRG->printMin, childSRG->printMax, tmpstr2, false).c_str());
		}
	}
	return 0;
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
int cQuestionAnswering::metaPatternMatch(cSource* questionSource, cSource* childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cPattern*& mapPatternAnswer, cPattern*& mapPatternQuestion)
{
	LFS
		// match childSource, at childSRG->where to pattern defined by mapPatternAnswer[0]
		int element, maxEnd, nameEnd = -1, lastWhere = -1; // idExemption=-1,
	for (int mi = childSRG->printMin; mi < childSRG->printMax; mi++)
	{
		if ((element = childSource->queryPattern(mi, mapPatternAnswer->getElement(0)->patternStr[0], maxEnd)) == -1)
			continue;
		int whereMNE = mi + childSource->pema[element].begin;
		if ((element = childSource->m[whereMNE].pma.queryPattern(mapPatternAnswer->getElement(0)->patternStr[0], nameEnd)) == -1)
			continue;
		if (lastWhere == whereMNE)
			continue;
		mapPatternAnswer->lplogShort(L"AnswerPattern", LOG_INFO);
		mapPatternQuestion->lplogShort(L"QuestionPattern", LOG_INFO);
		lastWhere = whereMNE;
		vector < vector <cTagLocation> > tagSets;
		// L"_META_NAME_EQUIVALENCE",-3,L"NAME_PRIMARY",L"NAME_SECONDARY",L"NAME_ABOUT",NULL
		if (childSource->startCollectTags(true, metaNameEquivalenceTagSet, whereMNE, childSource->m[whereMNE].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, false, true, L"meta pattern match") > 0)
			for (auto& tagSet : tagSets)
			{
				processMetanameTagset(tagSet, whereMNE, element, questionSource, childSource, childSRG, mapPatternAnswer, mapPatternQuestion);
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
bool cQuestionAnswering::processChildObjectIntoString(cSource* childSource, int childObject, unordered_set <wstring>& whereQuestionInformationSourceObjectsStrings, wstring& childObjectString)
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

// record distance into the entry in proximity map (belonging to each QuestionInformationSourceObject in parentSRG) for childObjectString (closestObjectIterator) to how near it is to questionObjectMatchIndex (that matches the questionInformationObject in the child source)
//     2. find or create an entry in proximityMap relativeObjects corresponding to the object string.
//     4. get the closest principalWhere location to where and calculate distance
//     5. add the distance to the nearest principalWhere object to the sum in the relativeObjects entry corresponding to this lowered string (which is calculated from the object at where)
//     6. if the principalWhere and the where positions share a relative verb location, record in the number of direct relations, with a source path and which location (where)
void cQuestionAnswering::recordDistanceIntoProximityMap(cSource* childSource, unsigned int childSourceIndex, set <cObject::cLocation>& questionObjectMatchInChildSourceLocations,
	set <cObject::cLocation>::iterator& questionObjectMatchIndex, bool confidence, unordered_map <wstring, cProximityMap::cProximityEntry>::iterator closestObjectIterator)
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
//     2. record distance into the proximity map (the one for each QuestionInformationSourceObject in parentSRG) for childObjectString (closestObjectIterator) to how near it is to questionObjectMatchIndex (that matches the questionInformationObject in the child source)
void cQuestionAnswering::accumulateProximityEntry(cSource* childSource, unsigned int childSourceIndex, set <cObject::cLocation>& questionObjectMatchInChildSourceLocations,
	set <cObject::cLocation>::iterator& questionObjectMatchIndex, bool confidence, cSyntacticRelationGroup* parentSRG, cProximityMap* proximityMap, unordered_set <wstring>& whereQuestionInformationSourceObjectsStrings)
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
			// record distance into the proximity map (the one for each QuestionInformationSourceObject in parentSRG) for childObjectString (closestObjectIterator) to how near it is to questionObjectMatchIndex (that matches the questionInformationObject in the child source)
			// ****************************
			// find or create an entry in proximityMap relativeObjects corresponding to the object string.
			unordered_map <wstring, cProximityMap::cProximityEntry>::iterator closestObjectIterator = proximityMap->closestObjects.find(childObjectString);
			if (closestObjectIterator == proximityMap->closestObjects.end())
			{
				proximityMap->closestObjects[childObjectString] = cProximityMap::cProximityEntry(*this, childSource, childSourceIndex, childObject, parentSRG);
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
void cQuestionAnswering::accumulateProximityMaps(cSource* questionSource, cSyntacticRelationGroup* parentSRG, cSource* childSource, bool confidence)
{
	LFS
		wstring tmpstr;
	unordered_set <wstring> whereQuestionInformationSourceObjectsStrings;
	// 1. for each question information object, 
	//      create a string that is all lower case.  
	//      If it was all upper case, and very short, make it into an acronym
	for (set <int>::iterator si = parentSRG->whereQuestionInformationSourceObjects.begin(), siEnd = parentSRG->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
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
	for (set <int>::iterator si = parentSRG->whereQuestionInformationSourceObjects.begin(), siEnd = parentSRG->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
	{
		// 2. for each question information object, create a semantic map.  
		if (parentSRG->proximityMaps.find(*si) == parentSRG->proximityMaps.end())
			parentSRG->proximityMaps[*si] = new cProximityMap();
		cProximityMap* proximityMap = parentSRG->proximityMaps[*si];
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
				if (childSource->objects[I].objectClass == parentObjectClass && matchObjectsByName(questionSource, questionSource->objects.begin() + poi->object, childSource, childSource->objects.begin() + I, namedNoMatch, questionSource->debugTrace))
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
				accumulateProximityEntry(childSource, childSourceIndex, questionObjectMatchInChildSourceLocations, polIndex, confidence, parentSRG, proximityMap, whereQuestionInformationSourceObjectsStrings);
				childSource->parentSource = 0;
			}
		}
	}
}

// parse comment/abstract of rdfType.  
// For each sentence, 
//    match preferentially the subject/verb/object/prep?/prepObject, 
//    with special analysis match of the whereQuestionTargetSuggestionIndex, if srg->questionType is an adjective type.
//    answer is the special analysis match.
int cQuestionAnswering::analyzeQuestionFromSource(cSource* questionSource, wchar_t* derivation, wstring childSourceType, cSource* childSource, cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs, int& maxAnswer, bool eraseIfNoAnswers)
{
	LFS
		int currentPercent, lastProgressPercent = -1, startClockTime = clock();
	if (logQuestionDetail)
		lplog(LOG_WHERE, L"********[%s used %d times:%d total sources] %s:", derivation, childSource->numSearchedInMemory, sourcesMap.size(), childSource->sourcePath.c_str());
	else
		lplog(LOG_WHERE, L"********[%s] %s:", derivation, childSource->sourcePath.c_str());
	childSource->parentSource = NULL; // this is set so we can investigate child sources through identification of ISA types
	accumulateProximityMaps(questionSource, parentSRG, childSource, childSource->sourceType == cSource::WIKIPEDIA_SOURCE_TYPE);
	if (!childSource->isFormsProcessed)
	{
		childSource->isFormsProcessed = true;
		int numSR = childSource->syntacticRelationGroups.size(), I = 0;
		for (vector <cSyntacticRelationGroup>::iterator childSRG = childSource->syntacticRelationGroups.begin(); childSRG != childSource->syntacticRelationGroups.end() && I < numSR; childSRG++, I++)
		{
			LFSL
				if (childSRG->whereVerb >= 0 && childSource->m[childSRG->whereVerb].queryWinnerForm(isForm) >= 0 && childSRG->whereSubject >= 0 && childSRG->whereObject >= 0)
				{
					LFSL
						if (logQuestionDetail)
							childSource->printSRG(L"IS SWITCH", &(*childSRG), 0, childSRG->whereSubject, childSRG->whereObject, L"", false, -1, L"");
					vector <cSyntacticRelationGroup>::iterator endCopySRI = childSource->syntacticRelationGroups.insert(childSource->syntacticRelationGroups.end(), *childSRG);
					childSRG = childSource->syntacticRelationGroups.begin() + I;
					endCopySRI->whereSubject = childSRG->whereObject;
					endCopySRI->whereObject = childSRG->whereSubject;
				}
		}
	}
	bool questionTypeSubject = questionSource->inObject(parentSRG->whereSubject, parentSRG->whereQuestionType);
	bool questionTypeObject = questionSource->inObject(parentSRG->whereObject, parentSRG->whereQuestionType);
	bool questionTypePrepObject = questionSource->inObject(parentSRG->wherePrepObject, parentSRG->whereQuestionType);
	for (vector <cSyntacticRelationGroup>::iterator childSRG = childSource->syntacticRelationGroups.begin(), sriEnd = childSource->syntacticRelationGroups.end(); childSRG != sriEnd; childSRG++)
	{
		LFSL
			if (logQuestionDetail)
			{
				wstring ps;
				childSource->prepPhraseToString(childSRG->wherePrep, ps);
				childSource->printSRG(L"    [printall] ", &(*childSRG), 0, childSRG->whereSubject, childSRG->whereObject, ps, false, -1, L"");
			}
		if ((currentPercent = (childSRG - childSource->syntacticRelationGroups.begin()) * 100 / childSource->syntacticRelationGroups.size()) > lastProgressPercent)
		{
			wprintf(L"PROGRESS: %03d%% child relations processed with %04d seconds elapsed \r", currentPercent, (int)((clock() - startClockTime) / CLOCKS_PER_SEC));
			lastProgressPercent = currentPercent;
		}
		// child cannot be question
		bool inQuestion = (childSRG->whereSubject >= 0 && (childSource->m[childSRG->whereSubject].flags & cWordMatch::flagInQuestion));
		inQuestion |= (childSRG->whereObject >= 0 && (childSource->m[childSRG->whereObject].flags & cWordMatch::flagInQuestion));
		if (inQuestion) continue;
		// check for negation agreement
		if (childSRG->tft.negation ^ parentSRG->tft.negation)
			continue;
		// why do we want to block this?  There are many cases where a relative clause could be useful!  where can be matched to University, which is a fact that he graduated from the university.
		// KEEP: He went on to attend the University of Florida, *where he  graduated in 1978 with a degree in advertising and a 2.1 GPA.*
		// check for who / who his real name is / who is object in relative clause / SRI is constructed dynamically
		//if (childSRG->whereObject>=0 && childSRG->whereSubject>=0 && childSRG->whereObject<childSRG->whereSubject &&
		//	  childSource->m[childSRG->whereSubject].getRelObject()<0 && 
		//		(childSource->m[childSRG->where].objectRole&SENTENCE_IN_REL_ROLE) && childSource->m[childSRG->whereSubject].beginObjectPosition>0 && 
		//		childSource->m[childSource->m[childSRG->whereSubject].beginObjectPosition-1].queryWinnerForm(relativizerForm)>=0 && 
		//		(childSource->m[childSource->m[childSRG->whereSubject].beginObjectPosition-1].flags&cWordMatch::flagRelativeObject) &&
		//		childSRG->whereObject==childSource->m[childSource->m[childSRG->whereSubject].beginObjectPosition-1].getRelObject())
		//	continue;
		int whereMetaPatternAnswer = -1;
		if (parentSRG->mapPatternAnswer != NULL && (whereMetaPatternAnswer = metaPatternMatch(questionSource, childSource, childSRG, parentSRG->mapPatternAnswer, parentSRG->mapPatternQuestion)) >= 0)
			enterAnswerAccumulatingIdenticalAnswers(questionSource, parentSRG, cAS(childSourceType, childSource, -1, maxAnswer, L"META_PATTERN", &(*childSRG), 0, whereMetaPatternAnswer, -1, -1, false, false, L"", L"", 0, 0, 0, NULL), maxAnswer, answerSRGs);
		int compoundPartLoopsSubject = 0;
		for (int ws = childSRG->whereSubject; compoundPartLoopsSubject < 4; ws = childSource->m[ws].nextCompoundPartObject, compoundPartLoopsSubject++)
		{
			LFSL
				if ((ws != childSRG->whereSubject && ws < 0) || ws >= (int)childSource->m.size()) break;
			int matchSumSubject;
			wstring matchInfoDetailSubject;
			if (parentSRG->subQuery)
			{
				matchSumSubject = (checkParticularPartIdentical(questionSource, childSource, parentSRG->whereSubject, ws)) ? 8 : -1;
				matchInfoDetailSubject = (matchSumSubject < 0) ? L"[SUBQUERY_NOT_IDENTICAL]" : L"[SUBQUERY_IDENTICAL]";
			}
			else
				matchSumSubject = srgMatch(questionSource, childSource, parentSRG->whereSubject, ws, parentSRG->whereQuestionType, parentSRG->questionType, childSRG->nonSemanticSubjectTotalMatch, matchInfoDetailSubject, 8, parentSRG->subQuery);
			if (parentSRG->whereQuestionInformationSourceObjects.find(parentSRG->whereSubject) != parentSRG->whereQuestionInformationSourceObjects.end() && matchSumSubject < 8)
			{
				matchSumSubject = 0;
				matchInfoDetailSubject += L"[QUERY_INFORMATION_SOURCE_NOT_IDENTICAL_MATCH]";
			}
			if (childSRG->whereSubject >= 0 && childSource->m[childSRG->whereSubject].objectMatches.size() > 1 && matchSumSubject >= 0)
			{
				matchSumSubject /= childSource->m[childSRG->whereSubject].objectMatches.size();
				wstring subjectString;
				childSource->whereString(childSRG->whereSubject, subjectString, false);
				matchInfoDetailSubject += L"[MULTIPLE_CHILD_MATCH_SUBJ(" + subjectString + L")]";
			}
			if (matchSumSubject < 0)
			{
				if (ws < 0) break;
				continue;
			}
			int compoundPartLoops = 0;
			for (int vi = childSRG->whereVerb; compoundPartLoops < 4; vi = childSource->m[vi].nextCompoundPartObject, compoundPartLoops++)
			{
				wstring matchInfoDetailVerb;
				int verbMatch = sriVerbMatch(questionSource, childSource, parentSRG->whereVerb, vi, matchInfoDetailVerb, L"PRIMARY", 8);
				bool subjectMatch = matchSumSubject > 0;
				bool inSubQuery = parentSRG->questionType == unknownQTFlag;
				if (verbMatch <= 0 && !subjectMatch && !questionTypeSubject && !inSubQuery)
				{
					if (ws < 0 || childSource->m[ws].nextCompoundPartObject < 0 || ws >= childSource->m.size()) break;
					if (vi < 0 || childSource->m[vi].nextCompoundPartObject < 0 || vi >= childSource->m.size()) break;
					continue;
				}
				//if (matchSumSubject>0 && inSubQuery)
				//{
				//	wstring tmp1,tmp2;
				//	lplog(LOG_WHERE,L"Comparing subjects %d [%d, %d] %s and %s.",matchSumSubject,parentSRG->whereSubject,ws,whereString(parentSRG->whereSubject,tmp1,false).c_str(),childSource->whereString(ws,tmp2,false).c_str());
				//}
				for (int wo = childSRG->whereObject; true; wo = childSource->m[wo].nextCompoundPartObject)
				{
					LFSL
						if ((wo != childSRG->whereObject && wo < 0) || wo >= (int)childSource->m.size()) break;
					if (wo != childSRG->whereObject && logQuestionDetail)
					{
						wstring ps;
						childSource->prepPhraseToString(childSRG->wherePrep, ps);
						childSource->printSRG(L"    [printallCO] ", &(*childSRG), 0, ws, wo, ps, false, -1, L"");
					}
					set<int> whereAnswerMatchSubquery;
					wstring matchInfoDetail = matchInfoDetailSubject + matchInfoDetailVerb;
					int objectMatch = 0, secondaryVerbMatch = 0, secondaryObjectMatch = 0;
					objectMatch = srgMatch(questionSource, childSource, parentSRG->whereObject, wo, parentSRG->whereQuestionType, parentSRG->questionType, childSRG->nonSemanticObjectTotalMatch, matchInfoDetail, 8, parentSRG->subQuery);
					if (parentSRG->subQuery && questionTypeObject && objectMatch > 0)
						whereAnswerMatchSubquery.insert(childSRG->whereObject);

					secondaryVerbMatch = sriVerbMatch(questionSource, childSource, parentSRG->whereSecondaryVerb, childSRG->whereSecondaryVerb, matchInfoDetail, L"SECONDARY TO SECONDARY", 4);
					if (parentSRG->whereSecondaryVerb >= 0 && (secondaryVerbMatch == 0 || childSRG->whereSecondaryVerb < 0))
					{
						if (childSRG->whereSecondaryVerb >= 0 || (secondaryVerbMatch = sriVerbMatch(questionSource, childSource, parentSRG->whereSecondaryVerb, vi, matchInfoDetail, L"SECONDARY TO PRIMARY", 8)) == 0)
						{
							secondaryVerbMatch = -verbMatch;
							matchInfoDetail += L"[SECONDARY_VERB_MATCH_FAILED]";
						}
						else
							verbMatch = 0;
					}
					secondaryObjectMatch = srgMatch(questionSource, childSource, parentSRG->whereSecondaryObject, childSRG->whereSecondaryObject, parentSRG->whereQuestionType, parentSRG->questionType, childSRG->nonSemanticSecondaryObjectTotalMatch, matchInfoDetail, 4, parentSRG->subQuery);
					if (parentSRG->subQuery && questionTypeObject && secondaryObjectMatch > 0)
						whereAnswerMatchSubquery.insert(childSRG->whereSecondaryObject);
					if (parentSRG->whereSecondaryObject >= 0 && (secondaryObjectMatch == 0 || childSRG->whereSecondaryObject < 0))
					{
						if (childSRG->whereSecondaryObject >= 0 || (secondaryObjectMatch = srgMatch(questionSource, childSource, parentSRG->whereSecondaryObject, wo, parentSRG->whereQuestionType, parentSRG->questionType, childSRG->nonSemanticSecondaryObjectTotalMatch, matchInfoDetail, 8, parentSRG->subQuery)) == 0)
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
					if (verbMatch <= 0 && parentSRG->whereVerb >= 0 && vi >= 0 && childSRG->whereObject >= 0 &&
						childSource->m[vi].forms.isSet(isForm) && (cob = childSource->m[childSRG->whereObject].beginObjectPosition) >= 0 && (coe = childSource->m[childSRG->whereObject].endObjectPosition) >= 0 &&
						(coe - cob) > 2 && childSource->m[cob].queryForm(determinerForm) >= 0 && childSource->m[cob + 1].getMainEntry() == questionSource->m[parentSRG->whereVerb].getMainEntry())
					{
						verbMatch = 8;
						matchInfoDetail += L"[MOVED_VERB_TO_OBJECT]";
					}
					set <int> relPreps;
					childSource->getAllPreps(&(*childSRG), relPreps, wo);
					int relativizerAsPrepMatch = 0;
					// where you went -- You went TO Prague
					// when you went -- You went AT 12:30.
					unordered_set<wstring> relativizerObjectsActLikePrepObjects = { L"where",L"when" };
					if (objectMatch == 0 && wo >= 0 && childSource->m[wo].queryWinnerForm(relativizerForm) != -1 && parentSRG->wherePrep >= 0 && relativizerObjectsActLikePrepObjects.find(childSource->m[wo].word->first) != relativizerObjectsActLikePrepObjects.end())
					{
						relativizerAsPrepMatch = srgMatch(questionSource, childSource, parentSRG->wherePrepObject, wo, parentSRG->whereQuestionType, parentSRG->questionType, childSRG->nonSemanticPrepositionObjectTotalMatch, matchInfoDetail, 4, parentSRG->subQuery);
						//if (parentSRG->subQuery && questionTypePrepObject && relativizerAsPrepMatch > 0)
						//	whereAnswerMatchSubquery.insert(wo);
					}
					if (relPreps.empty())
						relPreps.insert(-1);
					for (int po : relPreps)
					{
						LFSL
							wstring matchInfo = matchInfoDetail;
						int prepObjectMatch = 0, prepMatch = 0;
						if (po != -1)
						{
							if (prepMatch = sriPrepMatch(questionSource, childSource, parentSRG->wherePrep, po, 2) == 2) prepMatch = 2;
							prepObjectMatch = srgMatch(questionSource, childSource, parentSRG->wherePrepObject, childSource->m[po].getRelObject(), parentSRG->whereQuestionType, parentSRG->questionType, childSRG->nonSemanticPrepositionObjectTotalMatch, matchInfoDetail, 4, parentSRG->subQuery);
							if (parentSRG->subQuery && questionTypePrepObject && prepObjectMatch > 0)
								whereAnswerMatchSubquery.insert(childSource->m[po].getRelObject());
							if (prepObjectMatch <= 0 && questionSource->inObject(parentSRG->wherePrepObject, parentSRG->whereQuestionType))
								prepObjectMatch = -8;
						}
						else if (questionSource->inObject(parentSRG->wherePrepObject, parentSRG->whereQuestionType))
							prepObjectMatch = -8;
						if (prepObjectMatch <= 0 && relativizerAsPrepMatch > 0)
						{
							prepMatch = 2;
							prepObjectMatch = relativizerAsPrepMatch;
							po = wo;
							matchInfo += L"[RELATIVIZER_AS_PREPOBJECT]";
						}
						int equivalenceClass = 0, equivalenceMatch = 0;
						if (verbMatch <= 0 && (subjectMatch || (questionTypeSubject && !(parentSRG->questionType & QTAFlag))) && (po == -1 || (prepMatch > 0 && (prepObjectMatch > 0 || questionSource->inObject(parentSRG->wherePrepObject, parentSRG->whereQuestionType)))))
						{
							if (equivalenceClassCheck(questionSource, childSource, childSRG, parentSRG, wo, equivalenceClass, 8) == 0)
								equivalenceMatch = equivalenceClassCheck2(questionSource, childSource, childSRG, parentSRG, wo, equivalenceClass, 8);
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
							childSource->prepPhraseToString(childSRG->wherePrep, ps);
							childSource->printSRG(L"    [printall] " + matchInfo, &(*childSRG), 0, childSRG->whereSubject, childSRG->whereObject, ps, false, -1, L"");
						}
						if (matchSum > 8 + 6 && ((parentSRG->subQuery && subjectMatch) || (subjectMatch && (verbMatch > 0 || secondaryVerbMatch > 0)))) // a single element match and 3/4 of another (sym match)
						{
							if (parentSRG->subQuery)
							{
								for (int wc : whereAnswerMatchSubquery)
								{
									bool match;
									if (match = checkParticularPartIdentical(questionSource, childSource, parentSRG->whereQuestionType, wc))
									{
										matchSum += 8;
										matchInfo += L"ANSWER_MATCH[+8]";
									}
									int parentObject, childObject;
									if ((parentObject = questionSource->m[parentSRG->whereQuestionType].getObject()) >= 0 && (childObject = childSource->m[wc].getObject()) >= 0)
									{
										if (!childSource->objects[childObject].dbPediaAccessed)
											childSource->identifyISARelation(wc, false, fileCaching);
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
							enterAnswerAccumulatingIdenticalAnswers(questionSource, parentSRG, cAS(childSourceType, childSource, -1, matchSum, matchInfo, &(*childSRG), equivalenceClass, ws, wo, po, false, false, L"", L"", 0, 0, 0, NULL), maxAnswer, answerSRGs);
						}
						if (logQuestionDetail)
						{
							wstring ps;
							if (po != -1)
								childSource->prepPhraseToString(po, ps);
							childSource->printSRG(L"", &(*childSRG), 0, ws, wo, ps, false, matchSum, matchInfo);
						}
					}
					if (wo < 0 || childSource->m[wo].nextCompoundPartObject < 0 || wo >= childSource->m.size()) break;
				}
				break;
				//if (vi < 0 || childSource->m[vi].nextCompoundPartObject < 0 || vi >= childSource->m.size()) break;
			}
			if (ws < 0 || childSource->m[ws].nextCompoundPartObject < 0 || ws >= childSource->m.size()) break;
		}
	}
	wprintf(L"PROGRESS: 100%% child relations processed with %04d seconds elapsed \r", (int)((clock() - startClockTime) / CLOCKS_PER_SEC));
	if (childSource->answerContainedInSource == 0 && childSource->numSearchedInMemory == 1 && eraseIfNoAnswers)
	{
		unordered_map <wstring, cSource*>::iterator smi = sourcesMap.find(childSource->sourcePath);
		if (smi != sourcesMap.end())
		{
			cSource* source = smi->second;
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
void cQuestionAnswering::analyzeQuestionThroughAbstractAndWikipediaFromRDFType(cSource* questionSource, wchar_t* derivation, int whereQuestionContextSuggestion, cSyntacticRelationGroup* parentSRG,
	cTreeCat* rdfType, bool parseOnly, vector < cAS >& answerSRGs, int& maxAnswer,
	unordered_map <int, cWikipediaTableCandidateAnswers*>& wikiTableMap, set <wstring>& wikipediaLinksAlreadyScanned)
{
	LFS
		if (rdfType != NULL)
		{
			int whereQuestionTypeObject = (parentSRG->questionType == unknownQTFlag) ? parentSRG->whereSubject : getWhereQuestionTypeObject(questionSource, parentSRG);
			cSource* abstractSource = NULL;
			int qMaxAnswer = -1;
			lplog(LOG_WHERE, L"birthDate=%s,birthPlace=%s,occupation=%s", rdfType->birthDate.c_str(), rdfType->birthPlace.c_str(), rdfType->occupation.c_str());
			if (processAbstract(questionSource, rdfType, abstractSource, parseOnly) >= 0)
			{
				analyzeQuestionFromSource(questionSource, derivation, L"abstract: " + abstractSource->sourcePath, abstractSource, parentSRG, answerSRGs, qMaxAnswer, false);
				maxAnswer = max(maxAnswer, qMaxAnswer);
				if (qMaxAnswer > 24)
					return;
			}
			int maxPrepositionalPhraseNonMixMatch = -1, dlen = wcslen(derivation);
			questionSource->ppExtensionAvailable(whereQuestionContextSuggestion, maxPrepositionalPhraseNonMixMatch, true);
			int minPrepositionalPhraseNonMixMatch = (maxPrepositionalPhraseNonMixMatch == 0) ? 0 : 1;
			for (int I = maxPrepositionalPhraseNonMixMatch; I >= minPrepositionalPhraseNonMixMatch; I--)
			{
				if (I != 0)
					StringCbPrintf(derivation + dlen, 1024 - dlen, L"P%d:", I);
				// process wikipedia entry - including any lower case words preceding a proper noun.
				cSource* wikipediaSource = NULL;
				if (processWikipedia(questionSource, whereQuestionContextSuggestion, wikipediaSource, rdfType->wikipediaLinks, I, parseOnly, wikipediaLinksAlreadyScanned, false) >= 0)
				{
					analyzeQuestionFromSource(questionSource, derivation, L"wikipedia:" + wikipediaSource->sourcePath, wikipediaSource, parentSRG, answerSRGs, qMaxAnswer, false);
					if (whereQuestionTypeObject >= 0)
					{
						vector < cSourceTable > wikiTables;
						addTables(questionSource, whereQuestionTypeObject, wikipediaSource, wikiTables);
						if (!wikiTables.empty())
							wikiTableMap[whereQuestionContextSuggestion] = new cWikipediaTableCandidateAnswers(wikipediaSource, wikiTables);
					}
					maxAnswer = max(maxAnswer, qMaxAnswer);
					if (qMaxAnswer > 24)
						return;
				}
				// process wikipedia entry NOT including any lower case words preceding a proper noun.
				if (processWikipedia(questionSource, whereQuestionContextSuggestion, wikipediaSource, rdfType->wikipediaLinks, I, parseOnly, wikipediaLinksAlreadyScanned, true) >= 0)
				{
					analyzeQuestionFromSource(questionSource, derivation, L"wikipediaNPUW:" + wikipediaSource->sourcePath, wikipediaSource, parentSRG, answerSRGs, qMaxAnswer, false);
					if (whereQuestionTypeObject >= 0)
					{
						vector < cSourceTable > wikiTables;
						addTables(questionSource, whereQuestionTypeObject, wikipediaSource, wikiTables);
						if (!wikiTables.empty())
							wikiTableMap[whereQuestionContextSuggestion] = new cWikipediaTableCandidateAnswers(wikipediaSource, wikiTables);
					}
					maxAnswer = max(maxAnswer, qMaxAnswer);
					if (qMaxAnswer > 24)
						return;
				}
				// process wikipedia with any wikipedia links explicitly included in the RDFType (not the object in the text, as whereQuestionContextSuggestion=-1).
				cSource* wikipediaLinkSource = NULL;
				if (processWikipedia(questionSource, -1, wikipediaLinkSource, rdfType->wikipediaLinks, I, parseOnly, wikipediaLinksAlreadyScanned, false) >= 0)
				{
					analyzeQuestionFromSource(questionSource, derivation, L"wikipediaLink:" + wikipediaLinkSource->sourcePath, wikipediaLinkSource, parentSRG, answerSRGs, qMaxAnswer, false);
					if (whereQuestionTypeObject >= 0)
					{
						vector < cSourceTable > wikiTables;
						addTables(questionSource, whereQuestionTypeObject, wikipediaLinkSource, wikiTables);
						if (!wikiTables.empty())
							wikiTableMap[whereQuestionContextSuggestion] = new cWikipediaTableCandidateAnswers(wikipediaLinkSource, wikiTables);
					}
					maxAnswer = max(maxAnswer, qMaxAnswer);
					if (qMaxAnswer > 24)
						return;
				}
			}
		}
}

bool cQuestionAnswering::checkObjectIdentical(cSource* source1, cSource* source2, int object1, int object2)
{
	LFS
		wstring tmpstr1, tmpstr2;
	//lplog(LOG_WHERE,L"Comparing %s against %s.",
	//  source1->objectString(source1->objects.begin()+object1,tmpstr1,false,false).c_str(),
	//  source2->objectString(source2->objects.begin()+object2,tmpstr2,false,false).c_str());
	int ca = source1->objects[object1].objectClass;
	if (ca == NON_GENDERED_NAME_OBJECT_CLASS || ca == NAME_OBJECT_CLASS)
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
	if (ca == PRONOUN_OBJECT_CLASS || ca == REFLEXIVE_PRONOUN_OBJECT_CLASS || ca == RECIPROCAL_PRONOUN_OBJECT_CLASS)
		return false;
	int ol1 = source1->objects[object1].originalLocation, ol2 = source2->objects[object2].originalLocation;
	if (ol1 < 0 || ol2 < 0)
		return false;
	int beginPosition1 = source1->m[ol1].beginObjectPosition, endPosition1 = source1->m[ol1].endObjectPosition;
	int beginPosition2 = source2->m[ol2].beginObjectPosition, endPosition2 = source2->m[ol2].endObjectPosition;
	if (beginPosition1 < 0 || endPosition1 < 0 || beginPosition2 < 0 || endPosition2 < 0)
		return false;
	if ((endPosition1 - beginPosition1) != (endPosition2 - beginPosition2))
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
	for (int I = 0; I < endPosition1 - beginPosition1; I++)
		if (source1->m[beginPosition1 + I].word != source2->m[beginPosition2 + I].word)
			return false;
	//lplog(LOG_WHERE,L"Comparing %s against %s [checkObjectIdentical=TRUE].",
	//  source1->objectString(source1->objects.begin()+object1,tmpstr1,false,false).c_str(),
	//  source2->objectString(source2->objects.begin()+object2,tmpstr2,false,false).c_str());
	return true;
}

bool cQuestionAnswering::checkParticularPartIdentical(cSource* source1, cSource* source2, int where1, int where2)
{
	LFS
		if (where1 < 0 || where2 < 0) return false;
	if (source1->m[where1].getObject() < 0 && source2->m[where2].getObject() < 0)
	{
		lplog(LOG_WHERE, L"cas1:%d:%s cas2:%d:%s", where1, source1->m[where1].word->first.c_str(), where2, source2->m[where2].word->first.c_str());
		return source1->m[where1].word == source2->m[where2].word;
	}
	if (source1->m[where1].getObject() < 0 || source2->m[where2].getObject() < 0)
		return false;
	if (source1->m[where1].objectMatches.size() == 0)
	{
		if (source2->m[where2].objectMatches.size() == 0)
			return checkObjectIdentical(source1, source2, source1->m[where1].getObject(), source2->m[where2].getObject());
		for (int om = 0; om < source2->m[where2].objectMatches.size(); om++)
			if (checkObjectIdentical(source1, source2, source1->m[where1].getObject(), source2->m[where2].objectMatches[om].object))
				return true;
		return false;
	}
	if (source2->m[where2].objectMatches.size() == 0)
	{
		for (int om = 0; om < source1->m[where1].objectMatches.size(); om++)
			if (checkObjectIdentical(source1, source2, source1->m[where1].objectMatches[om].object, source2->m[where2].getObject()))
				return true;
		return false;
	}
	for (unsigned int cai = 0; cai < source1->m[where1].objectMatches.size(); cai++)
		for (unsigned int cai2 = 0; cai2 < source2->m[where2].objectMatches.size(); cai2++)
			if (checkObjectIdentical(source1, source2, source1->m[where1].objectMatches[cai].object, source2->m[where2].objectMatches[cai2].object))
				return true;
	return false;
}

// parent=US government officials / child=President George W . Bush
int cQuestionAnswering::checkParentGroup(cSource* parentSource, int parentWhere, cSource* childSource, int childWhere, int childObject, bool& synonym, int& semanticMismatch)
{
	LFS
		if (parentSource->m[parentWhere].queryForm(commonProfessionForm) < 0 && parentSource->m[parentWhere].getMainEntry()->second.query(commonProfessionForm) < 0)
			return CONFIDENCE_NOMATCH;
	unordered_map<wstring, cSemanticMatchInfo>::iterator csmpi;
	wstring childObjectString;
	if ((csmpi = questionGroupMap.find(childSource->whereString(childSource->objects[childObject].originalLocation, childObjectString, true))) != questionGroupMap.end())
	{
		synonym = csmpi->second.synonym;
		semanticMismatch = csmpi->second.semanticMismatch;
		return csmpi->second.confidence;
	}
	// Does child belong to a group?
	// get rdfTypes
	vector <cTreeCat*> rdfTypes;
	unordered_map <wstring, int > topHierarchyClassIndexes;
	childSource->getExtendedRDFTypesMaster(childSource->objects[childObject].originalLocation, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__));
	wstring tmpstr, tmpstr2, pw = parentSource->m[parentWhere].word->first, tmpstr3;
	transform(pw.begin(), pw.end(), pw.begin(), (int(*)(int)) tolower);
	wstring pwme = parentSource->m[parentWhere].getMainEntry()->first;
	unordered_set <wstring> parentSynonyms;
	parentSource->getSynonyms(pw, parentSynonyms, NOUN);
	parentSource->getSynonyms(pwme, parentSynonyms, NOUN);
	//logQuestionDetail= (tmpstr2.find(L"George W Bush")!=wstring::npos);
	if (true)
	{
		unordered_map <wstring, int > associationMap;
		childSource->getAssociationMapMaster(childSource->objects[childObject].originalLocation, -1, associationMap, TEXT(__FUNCTION__), fileCaching);
		wstring associations;
		for (unordered_map <wstring, int >::iterator ami = associationMap.begin(), amiEnd = associationMap.end(); ami != amiEnd; ami++)
			associations += ami->first + L" ";
		lplog(LOG_WHERE, L"checkParentGroup ld parent:%s[%s]\n  child:%s\n  parentSynonyms:%s\n  childRdfTypeAssociations %s", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
			childSource->whereString(childWhere, tmpstr2, false).c_str(), setString(parentSynonyms, tmpstr3, L"|").c_str(), associations.c_str());
	}
	int confidenceMatch = CONFIDENCE_NOMATCH;
	wstring rdfInfoPrinted;
	// first professions
	for (unsigned int r = 0; r < rdfTypes.size(); r++)
	{
		if (logQuestionDetail)
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch))
				rdfTypes[r]->logIdentity(LOG_WHERE, L"checkParentGroup", false, rdfInfoPrinted);
		for (unsigned int p = 0; p < rdfTypes[r]->professionLinks.size(); p++)
		{
			// politician,businessman, etc
			vector < vector <string> > kindOfObjects;
			getAllOrderedHyperNyms(rdfTypes[r]->professionLinks[p], kindOfObjects);
			for (vector < vector <string> >::iterator oi = kindOfObjects.begin(), oiEnd = kindOfObjects.end(); oi != oiEnd; oi++)
			{
				bool firstNewLevel = false;
				for (vector <string>::iterator soi = oi->begin(), soiEnd = oi->end(); soi != soiEnd; soi++)
				{
					if (firstNewLevel)
					{
						bool levelContainsPerson = false;
						for (vector <string>::iterator soiLevel = soi; soiLevel != soiEnd && !soi->empty() && !levelContainsPerson; soiLevel++)
							levelContainsPerson = (*soiLevel == "person");
						if (levelContainsPerson)
							break;
					}
					if (firstNewLevel = soi->empty())
						continue;
					wstring wsoi;
					mTW(*soi, wsoi);
					if (parentSynonyms.find(wsoi) != parentSynonyms.end())
					{
						synonym = true;
						lplog(LOG_WHERE, L"checkParentGroup 1 parent:%s[%s] child:%s parentSynonyms:%s %s", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
							childSource->whereString(childWhere, tmpstr2, false).c_str(), setString(parentSynonyms, tmpstr3, L"|").c_str(), wsoi.c_str());
						tmpstr.clear();
						tmpstr2.clear();
						tmpstr3.clear();
						confidenceMatch = min(confidenceMatch, CONFIDENCE_NOMATCH / 2); // only synonym
					}
					if (pw == wsoi || pwme == wsoi)
					{
						lplog(LOG_WHERE, L"checkParentGroup 2 parent:%s[%s] child:%s", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
							childSource->whereString(childWhere, tmpstr2, false).c_str());
						tmpstr.clear();
						tmpstr2.clear();
						confidenceMatch = min(confidenceMatch, CONFIDENCE_NOMATCH / 4);
					}
					unordered_set <wstring> professionSynonyms;
					parentSource->getSynonyms(wsoi, professionSynonyms, NOUN);
					if (logQuestionDetail)
						lplog(LOG_WHERE, L"checkParentGroup ld parent:%s[%s] child:%s profession:%s[%s] professionSynonyms:%s", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
							childSource->whereString(childWhere, tmpstr2, false).c_str(), rdfTypes[r]->professionLinks[p].c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
					if (professionSynonyms.find(pw) != professionSynonyms.end() || professionSynonyms.find(pwme) != professionSynonyms.end())
					{
						synonym = true;
						lplog(LOG_WHERE, L"checkParentGroup 3 parent:%s[%s] child:%s profession:%s[%s] professionSynonyms:%s", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
							childSource->whereString(childWhere, tmpstr2, false).c_str(), rdfTypes[r]->professionLinks[p].c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
						confidenceMatch = min(confidenceMatch, CONFIDENCE_NOMATCH / 2);
						tmpstr.clear();
						tmpstr2.clear();
						tmpstr3.clear();
					}
				}
			}
		}
	}
	// honorific?
	if (confidenceMatch >= CONFIDENCE_NOMATCH && childSource->objects[childObject].name.hon != wNULL)
	{
		vector < vector <string> > kindOfObjects;
		if (pw == childSource->objects[childObject].name.hon->first || pwme == childSource->objects[childObject].name.hon->first)
		{
			lplog(LOG_WHERE, L"checkParentGroup 4 parent:%s[%s] child:%s profession:%s [honorific original]", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
				childSource->whereString(childWhere, tmpstr2, false).c_str(), childSource->objects[childObject].name.hon->first.c_str());
			confidenceMatch = CONFIDENCE_NOMATCH / 4;
		}
		if (confidenceMatch == CONFIDENCE_NOMATCH)
			getAllOrderedHyperNyms(childSource->objects[childObject].name.hon->first, kindOfObjects);
		for (vector < vector <string> >::iterator oi = kindOfObjects.begin(), oiEnd = kindOfObjects.end(); oi != oiEnd && confidenceMatch == CONFIDENCE_NOMATCH; oi++)
		{
			bool firstNewLevel = false;
			for (vector <string>::iterator soi = oi->begin(), soiEnd = oi->end(); soi != soiEnd && confidenceMatch == CONFIDENCE_NOMATCH; soi++)
			{
				if (firstNewLevel)
				{
					bool levelContainsPerson = false;
					for (vector <string>::iterator soiLevel = soi; soiLevel != soiEnd && !soi->empty() && !levelContainsPerson; soiLevel++)
						levelContainsPerson = (*soiLevel == "person");
					if (levelContainsPerson)
						break;
				}
				if (firstNewLevel = soi->empty())
					continue;
				wstring wsoi;
				mTW(*soi, wsoi);
				if (parentSynonyms.find(wsoi) != parentSynonyms.end())
				{
					lplog(LOG_WHERE, L"checkParentGroup 5 parent:%s[%s] child:%s parentSynonyms:%s %s [honorific]", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
						childSource->whereString(childWhere, tmpstr2, false).c_str(), setString(parentSynonyms, tmpstr3, L"|").c_str(), wsoi.c_str());
					synonym = true;
					confidenceMatch = CONFIDENCE_NOMATCH / 2;
					break;
				}
				if (pw == wsoi || pwme == wsoi)
				{
					lplog(LOG_WHERE, L"checkParentGroup 6 parent:%s[%s] child:%s profession:%s [honorific]", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
						childSource->whereString(childWhere, tmpstr2, false).c_str(), wsoi.c_str());
					confidenceMatch = CONFIDENCE_NOMATCH / 4;
					break;
				}
				unordered_set <wstring> professionSynonyms;
				parentSource->getSynonyms(wsoi, professionSynonyms, NOUN);
				if (logQuestionDetail)
					lplog(LOG_WHERE, L"checkParentGroup ld parent:%s[%s] child:%s profession:%s professionSynonyms:%s", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
						childSource->whereString(childWhere, tmpstr2, false).c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
				if (professionSynonyms.find(pw) != professionSynonyms.end() || professionSynonyms.find(pwme) != professionSynonyms.end())
				{
					lplog(LOG_WHERE, L"checkParentGroup 7 parent:%s[%s] child:%s profession:%s professionSynonyms:%s [honorific]", pw.c_str(), parentSource->whereString(parentWhere, tmpstr, false).c_str(),
						childSource->whereString(childWhere, tmpstr2, false).c_str(), wsoi.c_str(), setString(professionSynonyms, tmpstr3, L"|").c_str());
					synonym = true;
					confidenceMatch = CONFIDENCE_NOMATCH / 2;
					break;
				}
			}
		}
	}
	questionGroupMap[childObjectString] = cSemanticMatchInfo(synonym, semanticMismatch, confidenceMatch);
	return confidenceMatch;
}

// transformSource (class variable) is the transformQuestion source.
// originalQuestionSRI is the source srg that is mapped to the questionSource.
// destinationQuestionSRI is the transformation srg mapped to the constant question in transformSource.
// originalQuestionPattern is the pattern matched to the questionSource.
// destinationQuestionPattern is a pattern matched to transformSource.
// transformSourceToQuestionSourceMap maps each location in the original source to the questionSource.
//
// copy all of transformedSRI which is stored in the transformSource to questionSource.
// patterns have three maps:
//         map < int where, variable name > locationToVariableMap - used to find whether a location has a variable within the destinationPattern.
//         map < variable name, int where > variableToLocationMap - used to find the corresponding location for the variable within the originalQuestionPattern.
//         map < variable name, int length > variableToLengthMap - used to find the corresponding word length for the variable within the originalQuestionPattern.
// 
// ** transformedPrep is a special field which indicates the space relation was transformed by adding an 'in' to the relation to make it convertable from a question into a pattern for an answer.
//    When was Darrell Hammond born?
//    will have 'in' added to it:
//    When was Darrell Hammond born in - so the web search query will be 'Darrell Hammond born in ' - and the answer will be the object of the preposition 'in' which did not exist before the transformation.
void cQuestionAnswering::copySource(cSource* questionSource, cSyntacticRelationGroup* destinationQuestionSRI, cPattern* originalQuestionPattern, cPattern* destinationQuestionPattern, unordered_map <int, int>& transformSourceToQuestionSourceMap, unordered_map <wstring, wstring>& parseVariables)
{
	LFS
		transformSourceToQuestionSourceMap[-1] = -1;
	wstring phrase;
	transformSource->phraseString(destinationQuestionSRI->printMin, destinationQuestionSRI->printMax + 1, phrase, false);
	if (destinationQuestionSRI->transformedPrep >= 0)
		phrase += L" " + transformSource->m[destinationQuestionSRI->transformedPrep].word->first;
	lplog(LOG_WHERE | LOG_INFO, L"*** copying transformation pattern %s, with these variables:", phrase.c_str());
	for (int I = destinationQuestionSRI->printMin; I < destinationQuestionSRI->printMax + 1 && I < (signed)transformSource->m.size(); I++)
	{
		unordered_map < int, wstring >::iterator ivMap = destinationQuestionPattern->locationToVariableMap.find(I);
		if (ivMap != destinationQuestionPattern->locationToVariableMap.end())
			lplog(LOG_WHERE | LOG_INFO, L"      %d:variable %s", ivMap->first, ivMap->second.c_str());
	}
	for (int I = destinationQuestionSRI->printMin; I < destinationQuestionSRI->printMax + 1 && I < (signed)transformSource->m.size(); I++)
	{
		unordered_map < int, wstring >::iterator ivMap = destinationQuestionPattern->locationToVariableMap.find(I);
		wstring temp;
		// does a variable exist in the destination pattern?
		if (ivMap == destinationQuestionPattern->locationToVariableMap.end())
		{
			lplog(LOG_WHERE, L"[%s] location %d (mapped to %d) not found in pattern %d for variable.", transformSource->getOriginalWord(I, temp, false, false), I, questionSource->m.size(), originalQuestionPattern->num);
			questionSource->copyChildrenIntoParent(transformSource, I, transformSourceToQuestionSourceMap, true);
		}
		else
		{
			// if so, look up the variable (ivmap->second) within originalQuestionPattern
			unordered_map < wstring, int >::iterator ilMap = originalQuestionPattern->variableToLocationMap.find(ivMap->second);
			unordered_map < wstring, int >::iterator ilenMap = originalQuestionPattern->variableToLengthMap.find(ivMap->second);
			if ((ivMap->second != L"$" || parseVariables.find(L"$") == parseVariables.end()) && (ilMap == originalQuestionPattern->variableToLocationMap.end() || ilenMap == originalQuestionPattern->variableToLengthMap.end()))
			{
				// carry forward where answer should be to inform location of whereQuestionTypeObject.
				if (ivMap->second == L"A") // Answer - where the answer should be located! (He earns A), the destination answer to How much does he earn?
				{
					lplog(LOG_WHERE | LOG_ERROR, L"%d:variable %s [Desired Answer Position] found in question transformation.  Setting whereQuestionTypeObject=%d in destination question relation.", I, ivMap->second.c_str(), I);
					destinationQuestionSRI->whereQuestionTypeObject = I;
				}
				else
				{
					wstring tmpstr;
					lplog(LOG_WHERE | LOG_ERROR, L"%d:variable %s not found in question transformation, pushing word [%s (elements=%s) (suggestedElements=%s)].", I,
						ivMap->second.c_str(), transformSource->getOriginalWord(I, temp, false, false), transformSource->m[I].patternWinnerFormString(tmpstr).c_str(), parseVariables[ivMap->second].c_str());
				}
				transformSourceToQuestionSourceMap[I] = questionSource->m.size();
				questionSource->m.push_back(transformSource->m[I]);
				// optional lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms form
				questionSource->m[questionSource->m.size() - 1].questionTransformationSuggestedPattern = parseVariables[ivMap->second];
				continue;
			}
			int where, length;
			if (ivMap->second == L"$")
			{
				int previousAnswerObject = _wtoi(parseVariables[L"$"].c_str());
				where = questionSource->objects[previousAnswerObject].begin;
				length = questionSource->objects[previousAnswerObject].end - questionSource->objects[previousAnswerObject].begin;
			}
			else
			{
				where = ilMap->second;
				length = ilenMap->second;
			}
			// copy from the originalQuestionPattern source (questionSource), to destinationPattern (also in questionSource)
			for (int w = where; w < where + length; w++)
			{
				::lplog(LOG_WHERE, L"[%s] location %d (mapped to %d) used variable %s.  Mapping %d in transformSource to %d in questionSource and marking %d in questionSource with sameSourceCopy.",
					questionSource->getOriginalWord(w, temp, false, false), w, questionSource->m.size(), ivMap->second.c_str(), I, questionSource->m.size(), questionSource->m.size());
				transformSourceToQuestionSourceMap[I] = questionSource->m.size(); // do not change to w!
				questionSource->m.push_back(questionSource->m[w]);
				questionSource->m[questionSource->m.size() - 1].sameSourceCopy = w; // don't adjust reference for this position (see usage of sameSourceCopy at end of this procedure)
				questionSource->m[questionSource->m.size() - 1].adjustReferences(questionSource->m.size() - 1, questionSource->m.size() - 1 - w);
				if (questionSource->m[questionSource->m.size() - 1].objectMatches.size() > 0)
				{
					wstring tmpstr;
					lplog(LOG_WHERE, L"%d: copied location still has %d object matches:%s", questionSource->m.size() - 1, questionSource->m[questionSource->m.size() - 1].objectMatches.size(), questionSource->whereString(questionSource->m.size() - 1, tmpstr, false).c_str());
				}
				if (w == where + length - 1 && (questionSource->m[questionSource->m.size() - 1].flags & cWordMatch::flagNounOwner))
				{
					questionSource->m[questionSource->m.size() - 1].flags &= ~(cWordMatch::flagNounOwner | cWordMatch::flagAdjectivalObject);
					questionSource->getOriginalWord(questionSource->m.size() - 1, temp, false, false);
					lplog(LOG_WHERE | LOG_ERROR, L"%d: copySource: word transformed %s", questionSource->m.size() - 1, temp.c_str());
				}
				if (questionSource->m[w].getObject() >= 0 || questionSource->m[w].objectMatches.size() > 0 || length == 1)
				{
					::lplog(LOG_WHERE, L"Inserting whereQuestionInformationSourceObject using variable %d:%s in originalQuestionPattern to position %d:%s",
						ivMap->first, ivMap->second.c_str(), I, questionSource->m[questionSource->m.size() - 1].word->first.c_str());
					destinationQuestionSRI->whereQuestionInformationSourceObjects.insert(I);
				}
			}
		}
	}
	if (destinationQuestionSRI->transformedPrep >= 0)
	{
		wstring temp;
		lplog(LOG_WHERE, L"[%s] location %d (mapped to %d) transformPrep", transformSource->getOriginalWord(destinationQuestionSRI->transformedPrep, temp, false, false), destinationQuestionSRI->transformedPrep, questionSource->m.size(), originalQuestionPattern->num);
		questionSource->copyChildrenIntoParent(transformSource, destinationQuestionSRI->transformedPrep, transformSourceToQuestionSourceMap, false);
	}
	// adjust all referenced locations
	for (auto& pair : transformSourceToQuestionSourceMap)
	{
		if (pair.second < 0)
			continue;
		else if (questionSource->m[pair.second].sameSourceCopy < 0)
		{
			questionSource->m[pair.second].adjustReferences(pair.second, false, transformSourceToQuestionSourceMap);
			if (questionSource->m[pair.second].getObject() < -1000)
			{// see copyChildrenIntoParent
				lplog(LOG_WHERE, L"Setting negative object positive in adjust references - %d->%d object %d->%d!", pair.first, pair.second, questionSource->m[pair.second].getObject(), -questionSource->m[pair.second].getObject() - 1000); // SON[setObjectNegative]
				questionSource->m[pair.second].setObject(-questionSource->m[pair.second].getObject() - 1000);
			}
			else
				questionSource->m[pair.second].setObject(-1);
			questionSource->m[pair.second].objectMatches.clear();
		}
		else
			questionSource->m[pair.second].sameSourceCopy = -1;
	}
	lplog(LOG_WHERE | LOG_INFO, L"*** END copying transformation pattern %s:", phrase.c_str());
}

int	cQuestionAnswering::parseSubQueriesParallel(cSource* questionSource, cSource* childSource, vector <cSyntacticRelationGroup>& subQueries, int whereChildCandidateAnswer, set <wstring>& wikipediaLinksAlreadyScanned)
{
	LFS
		for (vector <cSyntacticRelationGroup>::iterator sqi = subQueries.begin(), sqiEnd = subQueries.end(); sqi != sqiEnd; sqi++)
		{
			// add the answer as a point of interest.
			// copy the answer into the parent
			unordered_map <int, int> transformSourceToQuestionSourceMap;
			if (childSource->m[whereChildCandidateAnswer].getObject() < 0)
				lplog(LOG_FATAL_ERROR, L"Subject not object!");
			for (int I = childSource->m[whereChildCandidateAnswer].beginObjectPosition; I < childSource->m[whereChildCandidateAnswer].endObjectPosition; I++)
				questionSource->copyChildrenIntoParent(childSource, I, transformSourceToQuestionSourceMap, false);
			if (transformSourceToQuestionSourceMap.find(whereChildCandidateAnswer) == transformSourceToQuestionSourceMap.end())
				lplog(LOG_FATAL_ERROR, L"Subject not found in translation map!");
			sqi->whereSubject = transformSourceToQuestionSourceMap[whereChildCandidateAnswer];
			for (auto& pair : transformSourceToQuestionSourceMap)
			{
				if (pair.second < 0)
					continue;
				else if (questionSource->m[pair.second].sameSourceCopy < 0)
				{
					questionSource->m[pair.second].adjustReferences(pair.second, false, transformSourceToQuestionSourceMap);
					if (questionSource->m[pair.second].getObject() < -1000)
					{// see copyChildrenIntoParent
						lplog(LOG_WHERE, L"Setting negative object positive in adjust references - %d->%d object %d->%d!", pair.first, pair.second, questionSource->m[pair.second].getObject(), -questionSource->m[pair.second].getObject() - 1000); // SON[setObjectNegative]
						questionSource->m[pair.second].setObject(-questionSource->m[pair.second].getObject() - 1000);
					}
					else
						questionSource->m[pair.second].setObject(-1);
					questionSource->m[pair.second].objectMatches.clear();
				}
				else
					questionSource->m[pair.second].sameSourceCopy = -1;
			}
			set<int> saveQISO = sqi->whereQuestionInformationSourceObjects;
			sqi->whereQuestionInformationSourceObjects.insert(sqi->whereSubject);
			sqi->subQuery = true;
			for (set <int>::iterator si = sqi->whereQuestionInformationSourceObjects.begin(), siEnd = sqi->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
			{
				if (questionSource->m[*si].getObject() < 0)
					continue;
				vector <cTreeCat*> rdfTypes;
				unordered_map <wstring, int > topHierarchyClassIndexes;
				questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__));
				set<wstring> abstractTypes;
				for (unsigned int r = 0; r < rdfTypes.size(); r++)
					if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch) && abstractTypes.find(rdfTypes[r]->typeObject) == abstractTypes.end())
					{
						abstractTypes.insert(rdfTypes[r]->typeObject);
						cSource* abstractSource = NULL;
						processAbstract(questionSource, rdfTypes[r], abstractSource, true);
						int maxPrepositionalPhraseNonMixMatch = -1;
						questionSource->ppExtensionAvailable(*si, maxPrepositionalPhraseNonMixMatch, true);
						int minPrepositionalPhraseNonMixMatch = (maxPrepositionalPhraseNonMixMatch == 0) ? 0 : 1;
						for (int I = maxPrepositionalPhraseNonMixMatch; I >= minPrepositionalPhraseNonMixMatch; I--)
						{
							// also process wikipedia entry
							cSource* wikipediaSource = NULL;
							processWikipedia(questionSource, *si, wikipediaSource, rdfTypes[r]->wikipediaLinks, I, true, wikipediaLinksAlreadyScanned, false);
							// also process wikipedia entry with no preceding uncapitalizedWords 
							cSource* wikipediaSourceNUW = NULL;
							processWikipedia(questionSource, *si, wikipediaSourceNUW, rdfTypes[r]->wikipediaLinks, I, true, wikipediaLinksAlreadyScanned, true);
							// also process wikipedia entry with just the wikipedia links in the RDFType
							cSource* wikipediaLinkSource = NULL;
							processWikipedia(questionSource, -1, wikipediaLinkSource, rdfTypes[r]->wikipediaLinks, I, true, wikipediaLinksAlreadyScanned, false);
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

bool cQuestionAnswering::analyzeRDFTypeBirthDate(cSource* questionSource, cSyntacticRelationGroup* ssri, wstring derivation, vector < cAS >& answerSRGs, int& maxAnswer, wstring birthDate)
{
	// question type must be "when"
	// verb must be "born"
	// this is in reference to 'When was {X} born?' in questionTransforms.txt
	if ((ssri->questionType & typeQTMask) == unknownQTFlag && ssri->whereVerb >= 0 && questionSource->m[ssri->whereVerb].word->first == L"born")
	{
		wstring ps;
		questionSource->prepPhraseToString(ssri->wherePrep, ps);
		questionSource->printSRG(L":analyzeRDFTypeBirthDate ", ssri, -1, ssri->whereSubject, ssri->whereObject, ps, false, -1, L"analyzeRDFTypeBirthDate", (ssri->questionType) ? LOG_WHERE | LOG_QCHECK : LOG_WHERE);
		cOM object = questionSource->createObject(derivation, birthDate, NON_GENDERED_GENERAL_OBJECT_CLASS);
		questionSource->objects[object.object].setIsTimeObject(true);
		// cAS(wstring _sourceType, cSource *_source, int _confidence, int _matchSum, wstring _matchInfo, cSyntacticRelationGroup* _sri, int _equivalenceClass, int _ws, int _wo, int _wp, bool _fromTable, wstring _tableNum, wstring _tableName, int _columnIndex, int _rowIndex, int _entryIndex, cColumn::cEntry *_entry)
		if (ssri->wherePrepObject >= 0)
			questionSource->m[ssri->wherePrepObject].objectMatches.push_back(object);
		answerSRGs.push_back(cAS(L"wikipediaInfoBox", questionSource, 1, 1000, L"[wikipediaInfoBox]", ssri, 0, 0, 0, ssri->wherePrep, true, false, L"", L"", 0, 0, 0, NULL));
		answerSRGs[answerSRGs.size() - 1].finalAnswer = true;
		maxAnswer = max(maxAnswer, 1000);
		return true;
	}
	return false;
}

bool cQuestionAnswering::analyzeRDFTypeOccupation(cSource* questionSource, cSyntacticRelationGroup* ssri, wstring derivation, vector < cAS >& answerSRGs, int& maxAnswer, wstring occupation)
{
	// question type must be "what"
	// This is in reference to 'What is {M} profession?' which transforms to '{Y} works as a {A=profession:commonProfession,noun}', both in questionTransforms.txt
	if ((ssri->questionType & typeQTMask) == unknownQTFlag && ssri->whereVerb >= 0 && questionSource->m[ssri->whereVerb].word->first == L"works" && ssri->wherePrepObject >= 0 && questionSource->m[ssri->wherePrepObject].word->first == L"profession")
	{
		for (auto& occ : splitString(occupation, L','))
		{
			cOM object = questionSource->createObject(derivation, occ, NON_GENDERED_GENERAL_OBJECT_CLASS);
			// cAS(wstring _sourceType, cSource *_source, int _confidence, int _matchSum, wstring _matchInfo, cSyntacticRelationGroup* _sri, int _equivalenceClass, int _ws, int _wo, int _wp, bool _fromTable, wstring _tableNum, wstring _tableName, int _columnIndex, int _rowIndex, int _entryIndex, cColumn::cEntry *_entry)
			questionSource->m[ssri->wherePrepObject].objectMatches.push_back(object);
			answerSRGs.push_back(cAS(L"wikipediaInfoBox", questionSource, 1, 1000, L"[wikipediaInfoBox]", ssri, 0, 0, ssri->wherePrepObject, 0, true, false, L"", L"", 0, 0, 0, NULL));
			answerSRGs[answerSRGs.size() - 1].finalAnswer = true;
			answerSRGs[answerSRGs.size() - 1].object = object.object;
		}
		wstring ps;
		questionSource->prepPhraseToString(ssri->wherePrep, ps);
		questionSource->printSRG(L":analyzeRDFTypeOccupation ", ssri, -1, ssri->whereSubject, ssri->whereObject, ps, false, -1, L"analyzeRDFTypeOccupation", (ssri->questionType) ? LOG_WHERE | LOG_QCHECK : LOG_WHERE);
		maxAnswer = max(maxAnswer, 1000);
		return true;
	}
	return false;
}

bool cQuestionAnswering::analyzeRDFTypes(cSource* questionSource, cSyntacticRelationGroup* srg, cSyntacticRelationGroup* ssri, wstring derivation, vector < cAS >& answerSRGs, int& maxAnswer, unordered_map <int, cWikipediaTableCandidateAnswers* >& wikiTableMap, bool subQueryFlag)
{
	wchar_t sqderivation[1024];
	wstring tmpstr;
	bool whereQuestionInformationSourceObjectsSkipped = true;
	for (set <int>::iterator si = ssri->whereQuestionInformationSourceObjects.begin(), siEnd = ssri->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
	{
		if (*si >= questionSource->m.size())
		{
			lplog(LOG_WHERE | LOG_FATAL_ERROR, L"%s: Illegal whereQuestionInformationSourceObject - %d!", derivation.c_str(), *si);
			continue;
		}
		if (questionSource->m[*si].getObject() < 0 && questionSource->m[*si].objectMatches.empty())
		{
			lplog(LOG_WHERE, L"%s: Information Source Object is null!", derivation.c_str());
			continue;
		}
		vector <cTreeCat*> rdfTypes;
		unordered_map <wstring, int > topHierarchyClassIndexes;
		questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__), -1, fileCaching);
		if (rdfTypes.empty() && srg->wherePrep >= 0)
		{
			questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__), 1, fileCaching);
			if (rdfTypes.empty() && srg->wherePrep >= 0 && questionSource->m[srg->wherePrep].relPrep >= 0)
				questionSource->getExtendedRDFTypesMaster(*si, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__), 2, fileCaching);
		}
		cOntology::setPreferred(topHierarchyClassIndexes, rdfTypes);
		set<wstring> preferredTypes;
		set <wstring> wikipediaLinksAlreadyScanned;
		wstring rdfInfoPrinted;
		// birthdate, occupation and other infobox info are copied to every rdfType, therefore only examining the first one.
		if (rdfTypes.size() > 0 && !rdfTypes[0]->birthDate.empty() && analyzeRDFTypeBirthDate(questionSource, ssri, derivation, answerSRGs, maxAnswer, rdfTypes[0]->birthDate))
			return true;
		if (rdfTypes.size() > 0 && !rdfTypes[0]->occupation.empty() && analyzeRDFTypeOccupation(questionSource, ssri, derivation, answerSRGs, maxAnswer, rdfTypes[0]->occupation))
			return true;
		for (unsigned int r = 0; r < rdfTypes.size(); r++)
		{
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch) && preferredTypes.find(rdfTypes[r]->typeObject) == preferredTypes.end())
			{
				preferredTypes.insert(rdfTypes[r]->typeObject);
				rdfTypes[r]->logIdentity(LOG_WHERE, (subQueryFlag) ? L"subQueries" : L"answerAllQuestionsInSource", false, rdfInfoPrinted);
				// find subject or object without question
				int numWords;
				wstring tmpstr2;
				StringCbPrintf(sqderivation, 1024 * sizeof(wchar_t), L"%s:%06d: informationSourceObject %s:rdfType %u:%s:", derivation.c_str(), ssri->where, questionSource->whereString(*si, tmpstr, false, 6, L" ", numWords).c_str(), r, rdfTypes[r]->toString(tmpstr2).c_str());
				analyzeQuestionThroughAbstractAndWikipediaFromRDFType(questionSource, sqderivation, *si, ssri, rdfTypes[r], false, answerSRGs, maxAnswer, wikiTableMap, wikipediaLinksAlreadyScanned);
				whereQuestionInformationSourceObjectsSkipped = false;
			}
			else
				rdfTypes[r]->logIdentity(LOG_WHERE, (subQueryFlag) ? L"subQueriesNP" : L"processQuestionSourceNP", false, rdfInfoPrinted);
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
int	cQuestionAnswering::matchSubQueries(cSource* questionSource, wstring derivation, cSource* childSource, int& anySemanticMismatch, bool& subQueryNoMatch, vector <cSyntacticRelationGroup>& subQueries,
	int whereChildCandidateAnswer, int whereChildCandidateAnswerEnd, int numConsideredParentAnswer, int semMatchValue, bool useParallelQuery)
{
	LFS
		if (childSource->m[whereChildCandidateAnswer].objectMatches.size() > 0)
		{
			lplog(LOG_FATAL_ERROR, L"%d:whereChildCandidateAnswer has %d object matches.  Ambiguous.", whereChildCandidateAnswer, childSource->m[whereChildCandidateAnswer].objectMatches.size());
		}
	wstring childWhereString;
	int numWords = 0;
	if (whereChildCandidateAnswerEnd < 0)
		whereChildCandidateAnswerEnd = childSource->m[whereChildCandidateAnswer].endObjectPosition + childSource->numWordsOfDirectlyAttachedPrepositionalPhrases(whereChildCandidateAnswer);
	childSource->phraseString(whereChildCandidateAnswer, whereChildCandidateAnswerEnd, childWhereString, true);
	numWords = whereChildCandidateAnswerEnd - whereChildCandidateAnswer;
	if (childCandidateAnswerMap.find(childWhereString) != childCandidateAnswerMap.end())
	{
		anySemanticMismatch = childCandidateAnswerMap[childWhereString].anySemanticMismatch;
		subQueryNoMatch = childCandidateAnswerMap[childWhereString].subQueryNoMatch;
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
	bool allSubQueriesMatch = true, headerPrinted = false;
	for (vector <cSyntacticRelationGroup>::iterator sqi = subQueries.begin(), sqiEnd = subQueries.end(); sqi != sqiEnd; sqi++)
	{
		// add the answer as a point of interest.
		// copy the answer into the parent
		if (subQueries.size() > 1)
			lplog(LOG_WHERE, L"%s:subquery %d:child subject=%s", derivation.c_str(), sqi - subQueries.begin(), childWhereString.c_str());
		else
			lplog(LOG_WHERE, L"%s:subquery child subject=%s", derivation.c_str(), childWhereString.c_str());
		unordered_map <int, int> transformSourceToQuestionSourceMap;
		if (childSource->m[whereChildCandidateAnswer].getObject() < 0)
			lplog(LOG_FATAL_ERROR, L"Subject not object!");
		for (int I = childSource->m[whereChildCandidateAnswer].beginObjectPosition; I < whereChildCandidateAnswerEnd; I++)
			questionSource->copyChildrenIntoParent(childSource, I, transformSourceToQuestionSourceMap, true);
		if (transformSourceToQuestionSourceMap.find(whereChildCandidateAnswer) == transformSourceToQuestionSourceMap.end())
			lplog(LOG_FATAL_ERROR, L"Subject not found in translation map!");
		sqi->whereSubject = transformSourceToQuestionSourceMap[whereChildCandidateAnswer];
		for (auto& pair : transformSourceToQuestionSourceMap)
		{
			if (pair.second < 0 || pair.second >= questionSource->m.size())
				continue;
			else if (questionSource->m[pair.second].sameSourceCopy < 0)
			{
				questionSource->m[pair.second].adjustReferences(pair.second, false, transformSourceToQuestionSourceMap);
				if (questionSource->m[pair.second].getObject() < -1000)
				{// see copyChildrenIntoParent
					lplog(LOG_WHERE, L"Setting negative object positive in adjust references - %d->%d object %d->%d!", pair.first, pair.second, questionSource->m[pair.second].getObject(), -questionSource->m[pair.second].getObject() - 1000); // SON[setObjectNegative]
					questionSource->m[pair.second].setObject(-questionSource->m[pair.second].getObject() - 1000);
				}
				else
					questionSource->m[pair.second].setObject(-1);
				questionSource->m[pair.second].objectMatches.clear();
			}
			else
				questionSource->m[pair.second].sameSourceCopy = -1;
		}
		for (auto& pair : transformSourceToQuestionSourceMap)
		{
			if (questionSource->m[pair.second].getObject() < -1000) // see copyChildrenIntoParent
				lplog(LOG_FATAL_ERROR, L"Negative object survivor after adjust references - %d->%d!", pair.first, pair.second);
		}
		set<int> saveQISO = sqi->whereQuestionInformationSourceObjects;
		sqi->whereQuestionInformationSourceObjects.insert(sqi->whereSubject);
		sqi->subQuery = true;
		vector < cAS > answerSRGs;
		int maxAnswer = -1;
		wstring ps, tmpMatchInfo;
		questionSource->prepPhraseToString(sqi->wherePrep, ps);
		wchar_t sqderivation[1024];
		lplog(LOG_WHERE, L"parent considered answer %d:child subject=%s BEGIN", numConsideredParentAnswer, childWhereString.c_str());
		StringCbPrintf(sqderivation, 1024 * sizeof(wchar_t), L"%s:SUBQUERY #%d", derivation.c_str(), (int)(sqi - subQueries.begin()));
		questionSource->printSRG(sqderivation, &(*sqi), 0, sqi->whereSubject, sqi->whereObject, ps, false, -1, tmpMatchInfo);
		unordered_map <int, cWikipediaTableCandidateAnswers* > wikiTableMap;
		bool whereQuestionInformationSourceObjectsSkipped = analyzeRDFTypes(questionSource, &(*sqi), &(*sqi), derivation, answerSRGs, maxAnswer, wikiTableMap, true);
		lplog(LOG_WHERE, L"%s:SEARCHING WEB%s ************************************************************", derivation.c_str(), (whereQuestionInformationSourceObjectsSkipped) ? L" (SKIPPED ALL informationSourceObjects)" : L"");
		sqi->whereQuestionInformationSourceObjects = saveQISO;
		vector <wstring> webSearchQueryStrings;
		getWebSearchQueries(questionSource, &(*sqi), webSearchQueryStrings);
		for (int webSearchOffset = 0; webSearchOffset < (signed)webSearchQueryStrings.size(); webSearchOffset++)
		{
			if (useParallelQuery)
				webSearchForQueryParallel(questionSource, (wchar_t*)derivation.c_str(), &(*sqi), false, answerSRGs, maxAnswer, 10, 1, true, webSearchQueryStrings, webSearchOffset);
			else
				webSearchForQuerySerial(questionSource, (wchar_t*)derivation.c_str(), &(*sqi), false, answerSRGs, maxAnswer, 10, 1, true, webSearchQueryStrings, webSearchOffset);
		}
		bool anyMatch = false;
		for (unsigned int I = 0; I < answerSRGs.size(); I++)
		{
			if (answerSRGs[I].matchSum == maxAnswer || answerSRGs[I].matchSum >= 18)
			{
				StringCbPrintf(sqderivation, 1024 * sizeof(wchar_t), L"%s:SUBQUERY #%d answer #%u", derivation.c_str(), (int)(sqi - subQueries.begin()), I);
				int semanticMismatch = (answerSRGs[I].matchInfo.find(L"WIKI_OTHER_ANSWER") != wstring::npos) ? 12 : 0;
				bool match = answerSRGs[I].matchInfo.find(L"ANSWER_MATCH[") != wstring::npos;
				const wchar_t* wm = L"";
				if (semanticMismatch)
					wm = L"MISMATCH";
				if (match)
					wm = L"MATCH";
				ps.clear();
				answerSRGs[I].source->prepPhraseToString(answerSRGs[I].wp, ps);
				answerSRGs[I].source->printSRG(L"[" + wstring(sqderivation) + L"]", answerSRGs[I].srg, 0, answerSRGs[I].ws, answerSRGs[I].wo, ps, false, answerSRGs[I].matchSum, answerSRGs[I].matchInfo);
				wstring tmpstr2;
				if (!headerPrinted)
					lplog(LOG_WHERE, L"%s:SUBQUERY ANSWER LIST %s ************************************************************", derivation.c_str(), answerSRGs[I].source->whereString(answerSRGs[I].srg->whereObject, tmpstr2, false).c_str());
				if (questionSource->inObject(sqi->whereObject, sqi->whereQuestionType))
					lplog(LOG_WHERE, L"[subquery answer %d]:%s OBJECT expected child answer=%s (source=%s).", I, wm, questionSource->whereString(sqi->whereObject, tmpstr, false).c_str(), answerSRGs[I].source->sourcePath.c_str());
				else if (questionSource->inObject(sqi->wherePrepObject, sqi->whereQuestionType))
					lplog(LOG_WHERE, L"[subquery answer %d]:%s PREPOBJECT expected child answer=%s (source=%s).", I, wm, questionSource->whereString(sqi->wherePrepObject, tmpstr, false).c_str(), answerSRGs[I].source->sourcePath.c_str());
				else
					lplog(LOG_WHERE, L"[subquery answer %d]:NOT OBJECT OR PREPOBJECT %s (answer source=%s).", I, wm, answerSRGs[I].source->sourcePath.c_str());
				headerPrinted = true;
				anyMatch |= match;
				anySemanticMismatch |= semanticMismatch;
			}
		}
		if (!anyMatch)
			lplog(LOG_WHERE, L"%s:NO SUBQUERY ANSWERS FOUND", sqderivation);
		allSubQueriesMatch = allSubQueriesMatch && anyMatch;
		answerSRGs.clear();
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
	childCandidateAnswerMap[childWhereString].anySemanticMismatch = anySemanticMismatch;
	subQueryNoMatch = childCandidateAnswerMap[childWhereString].subQueryNoMatch = !allSubQueriesMatch;
	return childCandidateAnswerMap[childWhereString].confidence = (allSubQueriesMatch) ? semMatchValue : CONFIDENCE_NOMATCH;
}

int cQuestionAnswering::checkParticularPartQuestionTypeCheck(cSource* questionSource, __int64 questionType, int childWhere, int childObjectIndex, int& semanticMismatch)
{
	LFS
		wstring tmpstr;
	auto childObject = questionSource->objects.begin() + childObjectIndex;
	int oc = childObject->objectClass;
	if (oc == PRONOUN_OBJECT_CLASS ||
		oc == REFLEXIVE_PRONOUN_OBJECT_CLASS ||
		oc == RECIPROCAL_PRONOUN_OBJECT_CLASS ||
		oc == PLEONASTIC_OBJECT_CLASS ||
		oc == META_GROUP_OBJECT_CLASS ||
		oc == VERB_OBJECT_CLASS)
	{
		semanticMismatch = 1;
		return CONFIDENCE_NOMATCH;
	}
	if ((questionType == cQuestionAnswering::whereQTFlag || questionType == cQuestionAnswering::whoseQTFlag || questionType == cQuestionAnswering::whomQTFlag || questionType == cQuestionAnswering::wikiBusinessQTFlag || questionType == cQuestionAnswering::wikiWorkQTFlag) &&
		!childObject->dbPediaAccessed)
		questionSource->identifyISARelation(childWhere, true, fileCaching);
	bool wikiDetermined =
		(childObject->isWikiPlace || childObject->isWikiPerson || childObject->isWikiBusiness || childObject->isWikiWork);
	switch (questionType)
	{
	case cQuestionAnswering::whereQTFlag:
		if (oc == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			oc == GENDERED_DEMONYM_OBJECT_CLASS ||
			oc == GENDERED_RELATIVE_OBJECT_CLASS ||
			oc == BODY_OBJECT_CLASS ||
			oc == NON_GENDERED_BUSINESS_OBJECT_CLASS)
		{
			semanticMismatch = 2;
			return CONFIDENCE_NOMATCH;
		}
		if (oc == GENDERED_GENERAL_OBJECT_CLASS && wikiDetermined)
		{
			semanticMismatch = 3;
			return CONFIDENCE_NOMATCH;
		}
		if (oc == NON_GENDERED_GENERAL_OBJECT_CLASS ||
			oc == NON_GENDERED_NAME_OBJECT_CLASS ||
			oc == NAME_OBJECT_CLASS)
		{
			if (childObject->isLocationObject ||
				childObject->isWikiPlace)
				return 1;
			if (childObject->getSubType() == NOT_A_PLACE ||
				childObject->getIsTimeObject() ||
				childObject->isWikiPerson ||
				childObject->isWikiWork ||
				childObject->isWikiBusiness)
			{
				semanticMismatch = 4;
				return CONFIDENCE_NOMATCH;
			}
			// if object of [location preposition]
			if (questionSource->m[childWhere].relPrep >= 0)
			{
				wstring prep = questionSource->m[questionSource->m[childWhere].relPrep].word->first;
				if (questionSource->prepTypesMap[prep] == tprNEAR || questionSource->prepTypesMap[prep] == tprIN || questionSource->prepTypesMap[prep] == tprSPAT)
					return 1;
			}
		}
		break;
	case cQuestionAnswering::whoseQTFlag:
	case cQuestionAnswering::whomQTFlag:
		if (oc == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			oc == GENDERED_DEMONYM_OBJECT_CLASS ||
			oc == GENDERED_GENERAL_OBJECT_CLASS ||
			oc == GENDERED_RELATIVE_OBJECT_CLASS ||
			oc == BODY_OBJECT_CLASS)
			return 1;
		if ((oc == NON_GENDERED_GENERAL_OBJECT_CLASS || oc == NON_GENDERED_NAME_OBJECT_CLASS) && wikiDetermined && !childObject->isWikiPerson)
		{
			semanticMismatch = 5;
			return CONFIDENCE_NOMATCH;
		}
		if (oc == NON_GENDERED_BUSINESS_OBJECT_CLASS && wikiDetermined && !childObject->isWikiPerson)
		{
			semanticMismatch = 6;
			return CONFIDENCE_NOMATCH;
		}
		if (oc == NAME_OBJECT_CLASS || oc == NON_GENDERED_NAME_OBJECT_CLASS)
		{
			if (childObject->getSubType() != NOT_A_PLACE && wikiDetermined && !childObject->isWikiPerson)
			{
				semanticMismatch = 7;
				return CONFIDENCE_NOMATCH;
			}
			if (wikiDetermined && !childObject->isWikiPerson && (childObject->getIsTimeObject() || childObject->isLocationObject || childObject->isWikiPlace || childObject->isWikiBusiness))
			{
				semanticMismatch = 8;
				return CONFIDENCE_NOMATCH;
			}
			if (childObject->isWikiPerson)
				return 1;
			if (oc == NAME_OBJECT_CLASS)
				return CONFIDENCE_NOMATCH / 2;
		}
		return CONFIDENCE_NOMATCH;
	case cQuestionAnswering::whenQTFlag:
		if (!childObject->getIsTimeObject())
		{
			semanticMismatch = 9;
			return CONFIDENCE_NOMATCH;
		}
		else
			return 1;
	case cQuestionAnswering::wikiBusinessQTFlag:
		if (oc == NON_GENDERED_BUSINESS_OBJECT_CLASS || childObject->isWikiBusiness)
			return 1;
		break;
	case cQuestionAnswering::wikiWorkQTFlag:
		if (childObject->isWikiWork)
			return 1;
		break;
	}
	return CONFIDENCE_NOMATCH / 2;
}

int cQuestionAnswering::questionTypeCheck(cSource* questionSource, wstring derivation, cSyntacticRelationGroup* parentSRG, cAS& childCAS, int& semanticMismatch, bool& unableToDoQuestionTypeCheck)
{
	LFS
		unableToDoQuestionTypeCheck = true;
	if (parentSRG->associatedPattern != NULL)
	{
		bool destinationTransformationPatternMatched = (parentSRG->associatedPattern->matchPatternPosition(*questionSource, parentSRG->where, false, questionSource->debugTrace));
		if (logQuestionDetail)
		{
			parentSRG->associatedPattern->lplogShort(L"questionTypeCheck", LOG_WHERE);
			lplog(LOG_WHERE, L"questionTypeCheck: destinationTransformationPatternMatched=%s", (destinationTransformationPatternMatched) ? L"true" : L"false");
		}
	}
	// attempt to realign question type (does question require a person, a place, a business, a book album or song, or a time)?
	int qt = parentSRG->questionType & typeQTMask;
	if ((qt == whatQTFlag || qt == whichQTFlag) && (parentSRG->questionType & QTAFlag) && parentSRG->whereQuestionTypeObject > 0)
	{
		if (questionSource->matchChildSourcePositionSynonym(Words.query(L"person"), questionSource, parentSRG->whereQuestionTypeObject))
			qt = whomQTFlag;
		else if (questionSource->matchChildSourcePositionSynonym(Words.query(L"place"), questionSource, parentSRG->whereQuestionTypeObject))
			qt = whereQTFlag;
		else if (questionSource->matchChildSourcePositionSynonym(Words.query(L"business"), questionSource, parentSRG->whereQuestionTypeObject))
			qt = wikiBusinessQTFlag;
		else if (questionSource->matchChildSourcePositionSynonym(Words.query(L"book"), questionSource, parentSRG->whereQuestionTypeObject) ||
			questionSource->matchChildSourcePositionSynonym(Words.query(L"album"), questionSource, parentSRG->whereQuestionTypeObject) ||
			questionSource->matchChildSourcePositionSynonym(Words.query(L"song"), questionSource, parentSRG->whereQuestionTypeObject))
			qt = wikiWorkQTFlag;
		else if ((questionSource->m[parentSRG->whereQuestionTypeObject].word->second.timeFlags & T_UNIT))
			qt = whenQTFlag;
		else
			return CONFIDENCE_NOMATCH;
		parentSRG->questionType = qt | typeQTMask;
	}
	else if (qt != whereQTFlag && qt != whoseQTFlag && qt != whenQTFlag && qt != whomQTFlag)
		return CONFIDENCE_NOMATCH;
	unableToDoQuestionTypeCheck = false;
	// set whereChildCandidateAnswer and object
	int childWhere = childCAS.whereChildCandidateAnswer, childObject = childCAS.source->m[childWhere].getObject();
	if (childCAS.source->m[childWhere].objectMatches.size() > 0)
		childObject = childCAS.source->m[childWhere].objectMatches[0].object;
	if (childObject < 0)
		return CONFIDENCE_NOMATCH;
	//noise reduction - Jay-Z and Beyonce WedIn a private ceremony, Jay-Z married his ... snippet as an incomplete sentence that is parsed incorrectly
	if (childCAS.source->m[childWhere].queryWinnerForm(possessivePronounForm) >= 0) // his/her/its - don't allow match to an adjective, even though it is marked as an object.
	{
		semanticMismatch = 14;
		return CONFIDENCE_NOMATCH;
	}
	// compare candidate answer to question type (if we are asking for a person, is the childCAS candidate answer a person or gendered object?)
	int confidence = checkParticularPartQuestionTypeCheck(childCAS.source, qt, childWhere, childObject, semanticMismatch);
	wstring tmpstr;
	if (logQuestionDetail)
		lplog(LOG_WHERE, L"checkParticularPartQuestionTypeCheck: %d compared with %s yields matchValue %d", qt, childCAS.source->objectString(childObject, tmpstr, false).c_str(), confidence);
	if (semanticMismatch)
	{
		wstring ps;
		childCAS.source->prepPhraseToString(childCAS.wp, ps);
		childCAS.source->printSRG(L"questionTypeCheck semanticMismatch", childCAS.srg, 0, childCAS.ws, childCAS.wo, ps, false, -1, L"");
	}
	return confidence;
}

int cQuestionAnswering::semanticMatch(cSource* questionSource, wstring derivation, cSyntacticRelationGroup* parentSRG, cAS& childCAS, int& semanticMismatch)
{
	LFS
		int semMatchValue = 1;
	bool synonym = false;
	if (childCAS.matchInfo == L"META_PATTERN" || childCAS.matchInfo == L"SEMANTIC_MAP")
	{
		parentSRG->whereQuestionTypeObject = parentSRG->whereSubject;
		if (!childCAS.srg->nonSemanticSubjectTotalMatch)
			semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRG->whereSubject, childCAS.source, childCAS.whereChildCandidateAnswer, -1, synonym, semanticMismatch, fileCaching);
	}
	else
	{
		parentSRG->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, parentSRG);
		if (questionSource->inObject(parentSRG->whereSubject, parentSRG->whereQuestionType))
		{
			if (!childCAS.srg->nonSemanticSubjectTotalMatch)
				semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRG->whereSubject, childCAS.source, childCAS.whereChildCandidateAnswer, -1, synonym, semanticMismatch, fileCaching);
		}
		else if (questionSource->inObject(parentSRG->whereObject, parentSRG->whereQuestionType))
		{
			if (!childCAS.srg->nonSemanticObjectTotalMatch)
				semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRG->whereObject, childCAS.source, childCAS.whereChildCandidateAnswer, -1, synonym, semanticMismatch, fileCaching);
			else
				// Curveball's Profession is what? - if the question word is not adjectival, and verb is the identity verb (is) then check subject 
				// What was Curveball (commonProfession)
				if (!(parentSRG->questionType & QTAFlag) && parentSRG->whereSubject >= 0 && (questionSource->m[parentSRG->whereSubject].objectRole & IS_OBJECT_ROLE))
				{
					wstring tmpstr;
					if (questionSource->m[parentSRG->whereObject].questionTransformationSuggestedPattern.length())
					{
						semanticMismatch = (childCAS.source->m[childCAS.srg->whereObject].queryForm(questionSource->m[parentSRG->whereObject].questionTransformationSuggestedPattern) == -1) ? 14 : 0;
						if (logQuestionDetail)
							lplog(LOG_WHERE, L"semanticMismatch %s questionTransformationSuggestedPattern=%d:%s [form %s]",
								(semanticMismatch) ? L"NO MATCH" : L"MATCH", childCAS.srg->whereObject, childCAS.source->whereString(childCAS.srg->whereObject, tmpstr, false).c_str(), questionSource->m[parentSRG->whereObject].questionTransformationSuggestedPattern.c_str());
					}
					else
						semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRG->whereSubject, childCAS.source, childCAS.whereChildCandidateAnswer, -1, synonym, semanticMismatch, fileCaching);
				}
		}
		else if (questionSource->inObject(parentSRG->wherePrepObject, parentSRG->whereQuestionType) && childCAS.wp >= 0)
		{
			if (!childCAS.srg->nonSemanticPrepositionObjectTotalMatch)
				semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRG->wherePrepObject, childCAS.source, childCAS.whereChildCandidateAnswer, -1, synonym, semanticMismatch, fileCaching);
		}
		else if (questionSource->inObject(parentSRG->whereSecondaryObject, parentSRG->whereQuestionType))
		{
			if (!childCAS.srg->nonSemanticSecondaryObjectTotalMatch)
				semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRG->whereSecondaryObject, childCAS.source, childCAS.whereChildCandidateAnswer, -1, synonym, semanticMismatch, fileCaching);
		}
		else
			return CONFIDENCE_NOMATCH;
	}
	return semMatchValue;
}

// this is called from the parent
int cProximityMap::cProximityEntry::semanticCheck(cQuestionAnswering& qa, cSyntacticRelationGroup* parentSRG, cSource* parentSource)
{
	LFS
		if (confidenceCheck && childSource == 0)
		{
			qa.processPath(parentSource, lastChildSourcePath.c_str(), childSource, cSource::WEB_SEARCH_SOURCE_TYPE, 1, false);
			bool namedNoMatch = false, synonym = false;
			int parentSemanticMismatch = 0, adjectivalMatch = -1;
			// President George W . Bush[President George W Bush [509][name][M][H1:President F:George M1:W L:Bush [91]][WikiPerson]]
			{
				wstring tmpstr;
				childSource->whereString(childWhere2, tmpstr, false);
				static int cttmp = 0;
				//if (tmpstr.find(L"President Bush")==0 && cttmp<10) //  && tmpstr.find(L"Person")!=wstring::npos
				//{
				//	lplog(LOG_WHERE,L"%d:%s",parentSRG->whereSubject,tmpstr.c_str());
				//	cttmp++;
					//logSynonymDetail=logTableDetail=equivalenceLogDetail=logQuestionDetail=1;
				//}
			}
			if (parentSRG->whereSubject < 0 || childWhere2 < 0)
				return confidenceSE = CONFIDENCE_NOMATCH;
			if (qa.matchAllSourcePositions(parentSource, parentSRG->whereSubject, childSource, childWhere2, namedNoMatch, synonym, true, parentSemanticMismatch, adjectivalMatch, parentSource->debugTrace))
			{
				return confidenceSE = (synonym) ? CONFIDENCE_NOMATCH * 3 / 4 : 0;
			}
			if (namedNoMatch)
				return confidenceSE = CONFIDENCE_NOMATCH;
			if (parentSemanticMismatch)
				return confidenceSE = CONFIDENCE_NOMATCH;
			//		confidence=parentSource->semanticMatchSingle(L"accumulateProximityEntry",parentSRG,childSource,childWhere2,childObject,semanticMismatch,subQueryNoMatch,subQueries,-1,mapPatternAnswer,mapPatternQuestion);
		}
	return confidenceSE = CONFIDENCE_NOMATCH;
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
	childObject = 0;
}

cProximityMap::cProximityEntry::cProximityEntry(cQuestionAnswering& qa, cSource* childSource, unsigned int childSourceIndex, int co, cSyntacticRelationGroup* parentSRG) : cProximityEntry()
{
	int qt = parentSRG->questionType & cQuestionAnswering::typeQTMask;
	bool parentQuestionTypeValid = ((parentSRG->questionType & cQuestionAnswering::QTAFlag) || (qt != cQuestionAnswering::whereQTFlag && qt != cQuestionAnswering::whoseQTFlag && qt != cQuestionAnswering::whenQTFlag && qt != cQuestionAnswering::whomQTFlag));
	bool questionTypeCheck = parentQuestionTypeValid && qa.checkParticularPartQuestionTypeCheck(childSource, qt, childSourceIndex, childObject, semanticMismatch);
	confidenceCheck = (questionTypeCheck || parentSRG->questionType == cQuestionAnswering::unknownQTFlag);
	lastChildSourcePath = childSource->sourcePath;
	childWhere2 = childSourceIndex;
	childSource->objectString(childObject, fullDescriptor, true, false);
	if (fullDescriptor[fullDescriptor.length() - 1] != L'|')
		fullDescriptor += L"|";
	fullDescriptor += L"&";
	for (int f = childSource->objects[childObject].begin; f < childSource->objects[childObject].end; f++)
	{
		wstring formWinnerStr;
		fullDescriptor += childSource->m[f].winnerFormString(formWinnerStr);
		fullDescriptor += L"&";
	}
	wstring principalWhereOffset;
	itos(childWhere2 - childSource->objects[childObject].begin, principalWhereOffset);
	fullDescriptor = principalWhereOffset;
	childObject = co;
}

int cQuestionAnswering::verbTenseMatch(cSource* questionSource, cSyntacticRelationGroup* parentSRG, cAS& childCAS)
{
	if (parentSRG->whereVerb < 0 || childCAS.srg->whereVerb < 0)
		return 6;
	int questionVerbSense = questionSource->m[parentSRG->whereVerb].verbSense & ~(VT_EXTENDED | VT_PASSIVE | VT_POSSIBLE | VT_VERB_CLAUSE), childVerbSense = childCAS.source->m[childCAS.srg->whereVerb].verbSense & ~(VT_EXTENDED | VT_PASSIVE | VT_POSSIBLE | VT_VERB_CLAUSE);
	int tenseMatchReason = 0;
	if ((questionVerbSense & VT_NEGATION) ^ (childVerbSense & VT_NEGATION))
		childCAS.rejectAnswer += (questionVerbSense & VT_NEGATION) ? L"[question is negated and child is not]" : L"[question is not negated and child is negated]";
	// VT_PRESENT/VT_PAST/VT_PRESENT_PERFECT/VT_PAST_PERFECT/VT_FUTURE/VT_FUTURE_PERFECT
	// if child and parent are matched in tense
	else if ((parentSRG->isConstructedRelative) ? childVerbSense == VT_PAST : questionVerbSense == childVerbSense)
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
			parentSRG->whereVerb, senseString(tmpstr1, questionSource->m[parentSRG->whereVerb].verbSense).c_str(), childCAS.srg->whereVerb, senseString(tmpstr2, childCAS.source->m[childCAS.srg->whereVerb].verbSense).c_str());
		wstring questionVerbSenseString, childVerbSenseString;
		if (parentSRG->isConstructedRelative)
			childCAS.rejectAnswer += L"[QuestionIsConstructedRelative]";
		childCAS.rejectAnswer += (questionSource->sourceInPast) ? L"[question source in past]" : L"";
		childCAS.rejectAnswer += (childCAS.source->sourceInPast) ? L"[child source in past]" : L"";
		childCAS.rejectAnswer += L"[questionVerbSense=" + senseString(questionVerbSenseString, questionVerbSense) + L" childVerbSense=" + senseString(childVerbSenseString, childVerbSense) + L"]";
	}
	return tenseMatchReason;
}

void cProximityMap::cProximityEntry::printDirectRelations(cQuestionAnswering& qa, int logType, cSource* parentSource, wstring& path, int where)
{
	LFS
		if (qa.processPath(parentSource, path.c_str(), childSource, cSource::WEB_SEARCH_SOURCE_TYPE, 1, false) >= 0)
		{
			vector <cSyntacticRelationGroup>::iterator srg = childSource->findSyntacticRelationGroup(where);
			if (srg == childSource->syntacticRelationGroups.end() || srg->printMin > where || srg->printMax < where)
			{
				wstring tmpstr, tmpstr2, tmpstr3;
				::lplog(logType, L"%s:%d:SM %s:SUBJ[%s] VERB[%s] OBJECT[%s]", path.c_str(), where, childSource->m[where].word->first.c_str(),
					(childSource->m[where].relSubject >= 0) ? childSource->whereString(childSource->m[where].relSubject, tmpstr, true).c_str() : L"",
					(childSource->m[where].getRelVerb() >= 0) ? childSource->whereString(childSource->m[where].getRelVerb(), tmpstr2, true).c_str() : L"",
					(childSource->m[where].getRelObject() >= 0) ? childSource->whereString(childSource->m[where].getRelObject(), tmpstr3, true).c_str() : L"");
			}
			else
				childSource->printSRG(path.c_str(), &(*srg), 0, srg->whereSubject, srg->whereObject, srg->wherePrep, false, -1, L"DIRECT_RELATION", logType | LOG_QCHECK);
		}
}

int cQuestionAnswering::semanticMatchSingle(cSource* questionSource, wstring derivation, cSyntacticRelationGroup* parentSRG, cSource* childSource, int whereChild, int childObject, int& semanticMismatch, bool& subQueryNoMatch,
	vector <cSyntacticRelationGroup>& subQueries, int numConsideredParentAnswer, bool useParallelQuery)
{
	LFS
		int semMatchValue = 1;
	bool synonym = false;
	semMatchValue = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, parentSRG->whereSubject, childSource, whereChild, childObject, synonym, semanticMismatch, fileCaching);
	if (subQueries.empty())
		return semMatchValue;
	int subQueryConfidenceMatch = matchSubQueries(questionSource, derivation, childSource, semanticMismatch, subQueryNoMatch, subQueries, whereChild, -1, numConsideredParentAnswer, semMatchValue, useParallelQuery);
	wstring tmpstr1, tmpstr2;
	int numWords = 0;
	lplog(LOG_WHERE, L"%d:subquery comparison between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d (3)", numConsideredParentAnswer - 1,
		parentSRG->whereQuestionTypeObject, questionSource->whereString(parentSRG->whereQuestionTypeObject, tmpstr1, false).c_str(), whereChild, childSource->whereString(whereChild, tmpstr2, false, 6, L" ", numWords).c_str(),
		semanticMismatch, (subQueryNoMatch) ? L"true" : L"false", subQueryConfidenceMatch);
	return subQueryConfidenceMatch;
}

// do cas1 and cas2 give the same answer?
bool cQuestionAnswering::checkIdentical(cSource* questionSource, cSyntacticRelationGroup* srg, cAS& cas1, cAS& cas2)
{
	LFS
		if (questionSource->inObject(srg->whereSubject, srg->whereQuestionType))
			return checkParticularPartIdentical(cas1.source, cas2.source, cas1.ws, cas2.ws);
	if (questionSource->inObject(srg->whereObject, srg->whereQuestionType))
		return checkParticularPartIdentical(cas1.source, cas2.source, cas1.wo, cas2.wo);
	if (questionSource->inObject(srg->wherePrepObject, srg->whereQuestionType) && cas1.wp >= 0 && cas2.wp >= 0)
		return checkParticularPartIdentical(cas1.source, cas2.source, cas1.source->m[cas1.wp].getRelObject(), cas2.source->m[cas2.wp].getRelObject());
	if (questionSource->inObject(srg->whereSecondaryObject, srg->whereQuestionType))
		return checkParticularPartIdentical(cas1.source, cas2.source, cas1.srg->whereSecondaryObject, cas2.srg->whereSecondaryObject);
	if (srg->whereSecondaryPrep >= 0 && cas1.srg->whereSecondaryPrep >= 0 && cas2.srg->whereSecondaryPrep >= 0 && questionSource->inObject(questionSource->m[srg->whereSecondaryPrep].getRelObject(), srg->whereQuestionType))
		return checkParticularPartIdentical(cas1.source, cas2.source, cas1.source->m[cas1.srg->whereSecondaryPrep].getRelObject(), cas2.source->m[cas2.srg->whereSecondaryPrep].getRelObject());
	return false;
}

void cQuestionAnswering::setWhereChildCandidateAnswer(cSource* questionSource, cAS& childCAS, cSyntacticRelationGroup* parentSRG)
{
	int qt = parentSRG->whereQuestionType;
	if (qt < 0)
		qt = parentSRG->whereQuestionTypeObject;
	if (qt < 0)
	{
		lplog(LOG_WHERE, L"cannot set whereChildCandidateAnswer: whereQuestionType and whereQuestionTypeObject negative!", childCAS.whereChildCandidateAnswer, qt);
		return;
	}
	wstring cps, pps;
	childCAS.source->prepPhraseToString(childCAS.wp, cps);
	childCAS.source->printSRG(L"CHILD", childCAS.srg, 0, childCAS.ws, childCAS.wo, cps, false, -1, L"setWhereChildCandidateAnswer", LOG_WHERE);
	questionSource->prepPhraseToString(parentSRG->wherePrep, pps);
	questionSource->printSRG(L"PARENT", parentSRG, 0, parentSRG->whereSubject, parentSRG->whereObject, pps, false, -1, L"setWhereChildCandidateAnswer", LOG_WHERE);
	childCAS.whereChildCandidateAnswer = -1;
	if (childCAS.matchInfo == L"META_PATTERN" || childCAS.matchInfo == L"SEMANTIC_MAP")
		childCAS.whereChildCandidateAnswer = childCAS.ws;
	else if (questionSource->inObject(parentSRG->whereSubject, qt))
		childCAS.whereChildCandidateAnswer = childCAS.srg->whereSubject;
	else if (questionSource->inObject(parentSRG->whereObject, qt))
		childCAS.whereChildCandidateAnswer = childCAS.srg->whereObject;
	else if (questionSource->inObject(parentSRG->wherePrepObject, qt) && childCAS.wp >= 0)
	{
		if (childCAS.source->m[childCAS.wp].getRelObject() >= 0)
			childCAS.whereChildCandidateAnswer = childCAS.source->m[childCAS.wp].getRelObject();
		else
			childCAS.whereChildCandidateAnswer = childCAS.srg->wherePrepObject;
	}
	else if (questionSource->inObject(parentSRG->whereSecondaryObject, qt))
	{
		if ((childCAS.whereChildCandidateAnswer = childCAS.srg->whereSecondaryObject) < 0)
			childCAS.whereChildCandidateAnswer = childCAS.srg->whereObject;
	}
	else if (parentSRG->whereSecondaryPrep >= 0 && questionSource->inObject(questionSource->m[parentSRG->whereSecondaryPrep].getRelObject(), qt))
		childCAS.whereChildCandidateAnswer = questionSource->m[parentSRG->whereSecondaryPrep].getRelObject();
	lplog(LOG_WHERE, L"setWhereChildCandidateAnswer: set to %d from whereQuestionType=%d, whereQuestionTypeObject=%d.", childCAS.whereChildCandidateAnswer, parentSRG->whereQuestionType, parentSRG->whereQuestionTypeObject);
}

int cQuestionAnswering::getWhereQuestionTypeObject(cSource* questionSource, cSyntacticRelationGroup* srg)
{
	LFS
		if (srg->whereQuestionTypeObject >= 0)
			return srg->whereQuestionTypeObject;
	int collectionWhere = -1;
	if (questionSource->inObject(srg->whereSubject, srg->whereQuestionType))
		collectionWhere = srg->whereSubject;
	else if (questionSource->inObject(srg->whereObject, srg->whereQuestionType))
		collectionWhere = srg->whereObject;
	else if (questionSource->inObject(srg->wherePrepObject, srg->whereQuestionType))
		collectionWhere = srg->wherePrepObject;
	else if (questionSource->inObject(srg->whereSecondaryObject, srg->whereQuestionType))
	{
		if ((collectionWhere = srg->whereSecondaryObject) < 0)
			collectionWhere = srg->whereObject;
	}
	else if (srg->whereSecondaryPrep >= 0 && questionSource->inObject(questionSource->m[srg->whereSecondaryPrep].getRelObject(), srg->whereQuestionType))
		collectionWhere = questionSource->m[srg->whereSecondaryPrep].getRelObject();
	if (collectionWhere < 0)
	{
		lplog(LOG_WHERE | LOG_INFO, L"Unable to map whereQuestionType %d: S=%d, O=%d, PO=%d, SO=%d, SPO=%d",
			srg->whereQuestionType, srg->whereSubject, srg->whereObject, srg->wherePrepObject, srg->whereSecondaryObject, (srg->whereSecondaryPrep >= 0) ? questionSource->m[srg->whereSecondaryPrep].getRelObject() : -1);
		wstring phrase;
		lplog(LOG_WHERE | LOG_INFO, questionSource->phraseString(srg->printMin, srg->printMax, phrase, false).c_str());
	}
	else
	{
		lplog(LOG_WHERE | LOG_INFO, L"Successfully mapped getWhereQuestionTypeObject to %d: QuestionType[%d:%s] S=%d, O=%d, PO=%d, SO=%d, SPO=%d",
			collectionWhere,
			srg->whereQuestionType, questionSource->m[srg->whereQuestionType].word->first.c_str(),
			srg->whereSubject, srg->whereObject, srg->wherePrepObject, srg->whereSecondaryObject, (srg->whereSecondaryPrep >= 0) ? questionSource->m[srg->whereSecondaryPrep].getRelObject() : -1);
	}
	return collectionWhere;
}

bool cQuestionAnswering::processPathToPattern(cSource* questionSource, const wchar_t* path, cSource*& source)
{
	LFS
		source = new cSource(&questionSource->mysql, cSource::PATTERN_TRANSFORM_TYPE, 0);
	source->numSearchedInMemory = 1;
	source->isFormsProcessed = false;
	source->processOrder = questionSource->processOrder++;
	source->multiWordStrings = questionSource->multiWordStrings;
	source->multiWordObjects = questionSource->multiWordObjects;
	int repeatStart = 0;
	wprintf(L"\nParsing pattern file %s...\n", path);
	unsigned int unknownCount = 0, quotationExceptions = 0, totalQuotations = 0;
	int globalOverMatchedPositionsTotal = 0;
	// id, path, start, repeatStart, etext, author, title from sources 
	//char *sqlrow[]={ "1", (char *)cPath.c_str(), "~~BEGIN","1","","",(char *)cPath.c_str() };
	wstring wpath = path, start = L"~~BEGIN", title, etext;
	source->tokenize(title, etext, wpath, L"", start, repeatStart, unknownCount);
	source->doQuotesOwnershipAndContractions(totalQuotations);
	source->printSentences(false, unknownCount, quotationExceptions, totalQuotations, globalOverMatchedPositionsTotal);
	source->addWNExtensions();
	source->identifyObjects();
	source->analyzeWordSenses();
	source->narrativeIsQuoted = false;
	source->syntacticRelations();
	source->identifySpeakerGroups();
	vector <int> secondaryQuotesResolutions;
	source->resolveSpeakers(secondaryQuotesResolutions);
	source->resolveFirstSecondPersonPronouns(secondaryQuotesResolutions);
	source->write(path, false, false, L"");
	source->writeWords(path, L"");
	source->writePatternUsage(path, true);
	return !source->m.empty();
}

// process source\lists\questionTransforms.txt
// create transformationMatchPatterns
void cQuestionAnswering::initializeTransformations(cSource* questionSource, unordered_map <wstring, wstring>& parseVariables)
{
	LFS
		if (transformationPatternMap.size())
			return;
	wstring currentPatternName;
	parseVariables[L"$"] = L"noun|answer";
	vector <cPattern*> sourcePatterns, transformPatterns, linkPatterns;
	vector <cSyntacticRelationGroup*> linkSyntacticRelationGroups;
	int patternNum = 0, existingReferences = patternReferences.size(), patternNumInGroup = 0;
	if (processPathToPattern(questionSource, L"source\\lists\\questionTransforms.txt", transformSource))
	{
		for (int* I = transformSource->sentenceStarts.begin(); I != transformSource->sentenceStarts.end(); I++)
		{
			if ((I + 1) == transformSource->sentenceStarts.end()) break;
			vector <cSyntacticRelationGroup>::iterator srg = transformSource->findSyntacticRelationGroup(*I);
			if (srg == transformSource->syntacticRelationGroups.end())
				continue;
			bool inQuestion = (srg->whereSubject >= 0 && (transformSource->m[srg->whereSubject].flags & cWordMatch::flagInQuestion)) || (srg->whereObject >= 0 && (transformSource->m[srg->whereObject].flags & cWordMatch::flagInQuestion));
			if (!inQuestion && srg->whereVerb >= 0)
				transformSource->getQuestionTypeAndQuestionInformationSourceObjects(srg->whereVerb, srg->whereControllingEntity, srg->questionType, srg->whereQuestionType, srg->whereQuestionInformationSourceObjects);
			transformSource->printSRG(L"initializeTransformations", &(*srg), -1, srg->whereSubject, srg->whereObject, srg->wherePrep, false, -1, L"", LOG_INFO);
			cPattern* pattern = cPattern::create(transformSource, L"", patternNum++, *I, *(I + 1) - 1, parseVariables);
			pattern->metaPattern = transformSource->m[*I].t.traceQuestionPatternMap;
			for (int w = *I; w < *(I + 1) - 1; w++)
			{
				if (transformSource->metaCommandsEmbeddedInSource.find(w) != transformSource->metaCommandsEmbeddedInSource.end())
				{
					currentPatternName = transformSource->metaCommandsEmbeddedInSource[w];
					patternNumInGroup = 0;
				}
				if (pattern->getElement(w - *I)->variable.size() > 0)
				{
					pattern->locationToVariableMap[w] = pattern->getElement(w - *I)->variable;
					//::lplog(LOG_WHERE, L"pattern %s[%s] mapped location %d to variable %s", pattern->name.c_str(),pattern->differentiator.c_str(), w, pattern->getElement(w - *I)->variable.c_str());
				}
			}
			pattern->name = currentPatternName;
			itos(patternNumInGroup++, pattern->differentiator);
			transformPatterns.push_back(pattern);
			// traceTransformDestinationQuestion or traceQuestionPatternMap must be the last pattern of each group
			if (transformSource->m[*I].t.traceTransformDestinationQuestion || transformSource->m[*I].t.traceQuestionPatternMap)
			{
				transformationPatternMap[srg] = cTransformPatterns(sourcePatterns, pattern, linkPatterns, linkSyntacticRelationGroups);
				sourcePatterns.clear();
				linkPatterns.clear();
				linkSyntacticRelationGroups.clear();
			}
			else if (transformSource->m[*I].t.traceLinkQuestion)
			{
				linkPatterns.push_back(pattern);
				linkSyntacticRelationGroups.push_back(&(*srg));
			}
			else
				sourcePatterns.push_back(pattern);
		}
		bool patternError;
		for (unsigned int r = existingReferences; r < patternReferences.size(); r++)
			patternReferences[r]->resolve(transformPatterns, patternError);
	}
}

bool cQuestionAnswering::processTransformQuestionPattern(cSource* questionSource, wstring patternType, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& lssri, cPattern* sourcePattern, cPattern* destinationPattern, cSyntacticRelationGroup* destinationSyntacticRelationGroup, unordered_map <wstring, wstring>& parseVariables)
{
	unordered_map <int, int> transformSourceToQuestionSourceMap;
	// copy transformation destination space relation to questionSource
	//void cQuestionAnswering::copySource(cSource * questionSource, cSyntacticRelationGroup * destinationQuestionSRI, cPattern * originalQuestionPattern, cPattern * destinationQuestionPattern, unordered_map <int, int> & transformSourceToQuestionSourceMap, unordered_map <wstring, wstring> & parseVariables)
	copySource(questionSource, destinationSyntacticRelationGroup, sourcePattern, destinationPattern, transformSourceToQuestionSourceMap, parseVariables);
	for (auto& tsqm : transformSourceToQuestionSourceMap)
		if (tsqm.first >= 0)
			lplog(LOG_INFO | LOG_WHERE, L"LINK %s[%s] transformSourceToQuestionSourceMap %d:%s %d:%s", destinationPattern->name.c_str(), destinationPattern->differentiator.c_str(), tsqm.first, transformSource->m[tsqm.first].word->first.c_str(), tsqm.second, questionSource->m[tsqm.second].word->first.c_str());
	lssri = new cSyntacticRelationGroup(destinationSyntacticRelationGroup, transformSourceToQuestionSourceMap);
	lssri->transformedPrep = -1;
	questionSource->getSRIMinMax(lssri);
	wstring psi;
	transformSource->prepPhraseToString(destinationSyntacticRelationGroup->wherePrep, psi);
	wstring tq = patternType + destinationPattern->name + L"[" + destinationPattern->differentiator + L"]";
	transformSource->printSRG(tq + L" TransformQuestion - INPUT", destinationSyntacticRelationGroup, 0, destinationSyntacticRelationGroup->whereSubject, destinationSyntacticRelationGroup->whereObject, psi, false, -1, L"", LOG_INFO | LOG_WHERE);
	wstring ps;
	questionSource->prepPhraseToString(srg->wherePrep, ps);
	questionSource->printSRG(tq + L" TransformQuestion - ORIGINAL", srg, 0, srg->whereSubject, srg->whereObject, ps, false, -1, L"", LOG_INFO | LOG_WHERE);
	wstring pss;
	questionSource->prepPhraseToString(lssri->wherePrep, pss);
	questionSource->printSRG(tq + L" TransformQuestion - OUTPUT", lssri, 0, lssri->whereSubject, lssri->whereObject, pss, false, -1, L"", LOG_INFO | LOG_WHERE);
	sourcePattern->lplogShort(L"SourcePattern", LOG_WHERE);
	destinationPattern->lplogShort(L"DestinationPattern", LOG_WHERE);
	return true;
}

set <wstring> cQuestionAnswering::createAnswerListAsStrings(cSource* questionSource, set <int>& wherePossibleAnswers)
{
	int numWords;
	set <wstring> answers;
	for (auto wpa : wherePossibleAnswers)
	{
		wstring answer;
		if (questionSource->m[wpa].objectMatches.empty())
		{
			questionSource->whereString(wpa, answer, true, 6, L" ", numWords);
			answers.insert(answer);
		}
		else
			for (auto& om : questionSource->m[wpa].objectMatches)
			{
				questionSource->objectString(om, answer, true);
				answers.insert(answer);
			}
	}
	return answers;
}

bool cQuestionAnswering::followQuestionLink(int link, vector <cPattern*>& linkPatterns, cSource* questionSource, cSyntacticRelationGroup* srg, cPattern* sourcePattern, vector <cSyntacticRelationGroup*>& linkSyntacticRelationGroups, unordered_map <wstring, wstring>& parseVariables, bool parseOnly, bool useParallelQuery, bool disableWebSearch, vector < cTrackDescendantAnswers>& ancestorAnswers)
{
	if (link >= linkPatterns.size())
		return true;
	cPattern* linkPattern = linkPatterns[link];
	cTrackDescendantAnswers ancestorAnswer = ancestorAnswers[ancestorAnswers.size() - 1];
	wstring temp;
	parseVariables[L"$"] = itos(ancestorAnswer.inputAnswerObject, temp);
	cSyntacticRelationGroup* lssri;
	processTransformQuestionPattern(questionSource, L"LINK", srg, lssri, sourcePattern, linkPattern, linkSyntacticRelationGroups[link], parseVariables);
	vector < cTrackDescendantAnswers> descendantAnswers;
	if (answerQuestionInSource(questionSource, parseOnly, useParallelQuery, lssri, lssri, descendantAnswers, disableWebSearch) >= 0)
	{
		for (auto& wpa : descendantAnswers)
		{
			if (wpa.destinationAnswer)
			{
				wstring outputAnswer;
				questionSource->objectString(wpa.inputAnswerObject, outputAnswer, true);
				ancestorAnswers.push_back(cTrackDescendantAnswers(wpa.inputAnswerObject, ancestorAnswer.ancestorAnswersTrackingString + L"|" + outputAnswer, link, false));
				followQuestionLink(link + 1, linkPatterns, questionSource, srg, sourcePattern, linkSyntacticRelationGroups, parseVariables, parseOnly, useParallelQuery, disableWebSearch, ancestorAnswers);
			}
		}
	}
	return true;
}

int cQuestionAnswering::findMetanamePatterns(cSource* questionSource, cSyntacticRelationGroup* srg)
{
	// for each destination space relation (When was X born?), there are multiple source patterns that map into it (How old is Darrell Hammond?, etc).
	for (auto& itPM : transformationPatternMap)
	{
		cPattern* destinationPattern = itPM.second.destinationPattern;
		if (!destinationPattern->metaPattern)
			continue;
		for (auto ip : itPM.second.sourcePatterns)
		{
			if (questionSource->matchPattern(ip, srg->printMin, srg->printMin + 2, false))
			{
				srg->mapPatternAnswer = destinationPattern;
				srg->mapPatternQuestion = ip;
				return 1;
			}
		}
	}
	return 0;
}

// origin transformation patterns - does the question look like:
//   How old is Darrell Hammond?
//   What is the age of Darrell Hammond?
//   What is Darrell Hammond's age?
//   How many years old is Darrell Hammond?
// destination transformation pattern (which is also a space relation)
//   When was Darrell Hammond born?
// 
// this question is asking for an answer which is constantly changing.
// for best results, we must change the question into one that has a constant answer
int cQuestionAnswering::transformQuestion(cSource* questionSource, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& tsrg, vector < cTrackDescendantAnswers>& descendantAnswers, bool parseOnly, bool useParallelQuery, bool disableWebSearch)
{
	LFS
		unordered_map <wstring, wstring> parseVariables;
	initializeTransformations(questionSource, parseVariables);
	bool transformProcessed = false;
	vector < cTrackDescendantAnswers> finalAnswers;
	// for each destination space relation (When was X born?), there are multiple source patterns that map into it (How old is Darrell Hammond?, etc).
	for (auto& itPM : transformationPatternMap)
	{
		cPattern* destinationPattern = itPM.second.destinationPattern;
		if (destinationPattern->metaPattern)
			continue;
		for (auto ip : itPM.second.sourcePatterns)
		{
			if (questionSource->matchPattern(ip, srg->printMin, srg->printMin + 2, false))
			{
				descendantAnswers.push_back(cTrackDescendantAnswers(-1, L"", -1, false));
				followQuestionLink(0, itPM.second.linkPatterns, questionSource, srg, ip, itPM.second.linkSyntacticRelationGroups, parseVariables, parseOnly, useParallelQuery, disableWebSearch, descendantAnswers);
				for (auto& answer : descendantAnswers)
				{
					if (answer.link == itPM.second.linkPatterns.size() - 1)
					{
						wstring tmpstr;
						parseVariables[L"$"] = itos(answer.inputAnswerObject, tmpstr);
						processTransformQuestionPattern(questionSource, L"DESTINATION", srg, tsrg, ip, destinationPattern, &(*itPM.first), parseVariables);
						tsrg->questionType = true;
						if (tsrg->whereQuestionType < 0)
							tsrg->whereQuestionType = tsrg->whereQuestionTypeObject;
						tsrg->associatedPattern = destinationPattern;
						vector < cTrackDescendantAnswers> destinationAnswers;
						answerQuestionInSource(questionSource, parseOnly, useParallelQuery, tsrg, tsrg, destinationAnswers, disableWebSearch);
						for (auto& wpa : destinationAnswers)
						{
							if (wpa.destinationAnswer)
							{
								wstring outputAnswer;
								questionSource->objectString(wpa.inputAnswerObject, outputAnswer, true);
								finalAnswers.push_back(cTrackDescendantAnswers(wpa.inputAnswerObject, answer.ancestorAnswersTrackingString + L"|" + outputAnswer, itPM.second.linkPatterns.size(), true));
							}
						}
						transformProcessed = true;
					}
				}
			}
		}
	}
	descendantAnswers.insert(descendantAnswers.end(), finalAnswers.begin(), finalAnswers.end());
	wstring question;
	if (!transformProcessed)
	{
		lplog(LOG_WHERE, L"transform failed pattern match for %s", questionSource->phraseString(srg->printMin, srg->printMax + 1, question, true).c_str());
		return -1;
	}
	else
	{
		lplog(LOG_WHERE, L"transform pattern match %s resulted in %d answers.", questionSource->phraseString(srg->printMin, srg->printMax + 1, question, true).c_str(), finalAnswers.size());
		return finalAnswers.size();
	}
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
bool cQuestionAnswering::isQuestionPassive(cSource* questionSource, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& ssrg)
{
	LFS
		//cSyntacticRelationGroup(int _where,int _o,int _whereControllingEntity,int _whereSubject,int _whereVerb,int _wherePrep,int _whereObject,
		//             int _wherePrepObject,int _movingRelativeTo,int _relationType,
		//							bool _genderedEntityMove,bool _genderedLocationRelation,int _objectSubType,int _prepObjectSubType,bool _physicalRelation)
		ssrg = &(*srg);
	// detect Q1PASSIVE
	int maxEnd;
	if (srg->whereQuestionType < 0)
		return false;
	if (questionSource->queryPattern(srg->whereQuestionType, L"_Q2", maxEnd) != -1 && questionSource->queryPattern(srg->whereQuestionType + 1, L"_Q1PASSIVE", maxEnd) != -1)
	{
		set <int> relPreps;
		questionSource->getAllPreps(&(*srg), relPreps);
		int subclausePrep = -1, nearestObject = -1;
		if (srg->whereSubject < srg->whereVerb)
			nearestObject = srg->whereSubject;
		for (set <int>::iterator rp = relPreps.begin(), rpEnd = relPreps.end(); rp != rpEnd; rp++)
		{
			lplog(LOG_WHERE, L"%d:SUBST %d nearestObject=%d whereVerb=%d", srg->where, *rp, nearestObject, srg->whereVerb);
			if (questionSource->m[*rp].getRelObject() > 0 && nearestObject < *rp && *rp < srg->whereVerb)
				nearestObject = questionSource->m[*rp].getRelObject();
			if (questionSource->m[*rp].word->first == L"by")
				subclausePrep = *rp;
		}
		if (subclausePrep < 0)
			return false;
		ssrg = new cSyntacticRelationGroup(srg->where, questionSource->m[srg->where].getObject(), -1, questionSource->m[subclausePrep].getRelObject(), srg->whereVerb, questionSource->m[subclausePrep].relPrep, nearestObject, (questionSource->m[subclausePrep].relPrep >= 0) ? questionSource->m[questionSource->m[subclausePrep].relPrep].getRelObject() : -1, -1, stNORELATION, false, false, -1, -1, false);
		ssrg->whereQuestionType = nearestObject;
		ssrg->whereQuestionTypeObject = -1;
		ssrg->questionType = srg->questionType | QTAFlag;
		ssrg->isConstructedRelative = true;
		ssrg->changeStateAdverb = false;
		ssrg->whereQuestionInformationSourceObjects = srg->whereQuestionInformationSourceObjects;
		questionSource->printSRG(L"SUBSTITUTE", ssrg, 0, ssrg->whereSubject, ssrg->whereObject, L"", false, -1, L"");
	}
	// What are titles of albums featuring Jay-Z? --> what albums featured Jay-Z?
	/*
	~~~ what: relVerb='are'
	~~~ are: relSubject='titles' relVerb='are' relObject='what' relPrep='of'
	~~~ titles: relVerb='are' relObject='what' relPrep='of'
	~~~ of: relVerb='are' relObject='albums'
	~~~ albums: relVerb='featuring' relObject='jay-z' relPrep='of' ///// relObjectOfPrep
	~~~ featuring: relSubject='albums' relObject='jay-z'
	~~~ jay-z: relSubject='albums' relVerb='featuring'
	*/
	int verbPhraseElement = -1, relVerb = -1, relPrep = -1, relObjectOfPrep = -1, whereSubject = srg->whereSubject, whereTransformedObject = -1;
	if (questionSource->queryPatternDiff(srg->whereQuestionType, L"_Q2", L"F") != -1 &&
		whereSubject >= 0 && (relVerb = questionSource->m[whereSubject].getRelVerb()) >= 0 &&
		questionSource->m[relVerb].queryWinnerForm(isForm) >= 0 &&
		(relPrep = questionSource->m[whereSubject].relPrep) >= 0 &&
		(relObjectOfPrep = questionSource->m[relPrep].getRelObject()) >= 0 && //// albums
		(whereTransformedObject = questionSource->m[relObjectOfPrep].getRelObject()) >= 0 && //// Jay-Z
		(verbPhraseElement = questionSource->m[relObjectOfPrep].pma.queryPatternDiff(L"__NOUN", L"F")) != -1 &&
		(questionSource->m[whereSubject].getMainEntry()->first == L"name" || questionSource->m[whereSubject].getMainEntry()->first == L"title") &&
		questionSource->m[relPrep].word->first == L"of" && relObjectOfPrep > whereSubject)
	{
		//cSyntacticRelationGroup(int _where,int _o,int _whereControllingEntity,int _whereSubject,int _whereVerb,int _wherePrep,int _whereObject,
		//             int _wherePrepObject,int _movingRelativeTo,int _relationType,
		//							bool _genderedEntityMove,bool _genderedLocationRelation,int _objectSubType,int _prepObjectSubType,bool _physicalRelation)
		ssrg = new cSyntacticRelationGroup(whereSubject, whereTransformedObject, -1, relObjectOfPrep, questionSource->m[relObjectOfPrep].getRelVerb(), -1, whereTransformedObject, -1, -1, stNORELATION, false, false, -1, -1, false);
		ssrg->whereQuestionType = relObjectOfPrep;
		ssrg->whereQuestionTypeObject = -1;
		ssrg->questionType = srg->questionType | QTAFlag;
		ssrg->isConstructedRelative = true;
		ssrg->changeStateAdverb = false;
		ssrg->whereQuestionInformationSourceObjects = srg->whereQuestionInformationSourceObjects;
		questionSource->printSRG(L"SUBSTITUTE", ssrg, 0, ssrg->whereSubject, ssrg->whereObject, L"", false, -1, L"");
	}
	return true;
}

//   A. Which old man who ran in the Olympics won the prize? [Jenkins ran in what?] candidate answer Jenkins what==Olympics
//   B. Which prize originating in Spain did he win? [The prize originated in where? (the verb tense matches the main verb tense)] where=Spain
// 1. detect subqueries (after each object at whereQuestionType, detect a infinitive or a relative clause)
//      A. who ran in the Olympics
//      B. originating in Spain
// 2. collect subqueries
void cQuestionAnswering::detectSubQueries(cSource* questionSource, cSyntacticRelationGroup* srg, vector <cSyntacticRelationGroup>& subQueries)
{
	LFS
		//A. Which old man who ran in the Olympics won the prize?
		// B. Which prize originating in Spain did he win? [The prize originated in where? (the verb tense matches the main verb tense)] where=Spain
		int collectionWhere = srg->whereQuestionTypeObject;
	if (collectionWhere < 0) return;
	int rh = questionSource->m[collectionWhere].endObjectPosition, relVerb = -1, relPrep = -1, relObject = -1;
	if (rh >= 0 && (((questionSource->m[rh].flags & cWordMatch::flagRelativeHead) && (relVerb = questionSource->m[rh].getRelVerb()) >= 0) || questionSource->detectAttachedPhrase(srg, relVerb) >= 0))
	{
		questionSource->printSRG(L"[collecting subqueries of]:", &(*srg), 0, srg->whereSubject, srg->whereObject, srg->wherePrep, false, -1, L"");
		// old man ran in the Olympics
		// prize originating in Spain
		relPrep = questionSource->m[relVerb].relPrep;
		questionSource->m.push_back(questionSource->m[relVerb]);
		//adjustReferences(relVerb); // not applicable
		relVerb = questionSource->m.size() - 1;
		questionSource->m[relVerb].verbSense = questionSource->m[srg->whereVerb].verbSense;
		questionSource->m[relVerb].setQuoteForwardLink(questionSource->m[srg->whereVerb].getQuoteForwardLink());
		questionSource->m[relVerb].word = questionSource->getTense(questionSource->m[relVerb].word, questionSource->m[collectionWhere].word, questionSource->m[srg->whereVerb].word->second.inflectionFlags);
		//cSyntacticRelationGroup(int _where, int _o, int _whereControllingEntity, int _whereSubject, int _whereVerb, int _wherePrep, int _whereObject,
		//	int _wherePrepObject, int _movingRelativeTo, int _relationType,
		//	bool _genderedEntityMove, bool _genderedLocationRelation, int _objectSubType, int _prepObjectSubType, bool _physicalRelation)
		cSyntacticRelationGroup csr(collectionWhere, questionSource->m[collectionWhere].getObject(), -1, collectionWhere, relVerb, relPrep, relObject = questionSource->m[relVerb].getRelObject(), (relPrep >= 0) ? questionSource->m[relPrep].getRelObject() : -1, -1, -1, false, false, -1, -1, false);
		csr.questionType = unknownQTFlag;
		int whereQuestionTypeFlags;
		if (relPrep >= 0 && questionSource->m[relPrep].getRelObject() >= 0)
			questionSource->testQuestionType(questionSource->m[relPrep].getRelObject(), csr.whereQuestionType, whereQuestionTypeFlags, prepObjectQTFlag, csr.whereQuestionInformationSourceObjects);
		questionSource->testQuestionType(relObject, csr.whereQuestionType, whereQuestionTypeFlags, objectQTFlag, csr.whereQuestionInformationSourceObjects);
		if (relObject >= 0)
			questionSource->testQuestionType(questionSource->m[relObject].relNextObject, csr.whereQuestionType, whereQuestionTypeFlags, secondaryObjectQTFlag, csr.whereQuestionInformationSourceObjects);
		questionSource->testQuestionType(questionSource->m[rh].getRelVerb() - 1, csr.whereQuestionType, whereQuestionTypeFlags, 0, csr.whereQuestionInformationSourceObjects);
		questionSource->testQuestionType(questionSource->m[rh].getRelVerb() - 2, csr.whereQuestionType, whereQuestionTypeFlags, 0, csr.whereQuestionInformationSourceObjects);
		if (csr.whereQuestionInformationSourceObjects.size() > 0)
			csr.whereQuestionType = *csr.whereQuestionInformationSourceObjects.begin();
		wstring ps;
		questionSource->prepPhraseToString(csr.wherePrep, ps);
		questionSource->printSRG(L"collected subquery:", &csr, 0, csr.whereSubject, csr.whereObject, ps, false, -1, L"");
		subQueries.push_back(csr);
	}
}

bool cQuestionAnswering::enterAnswerAccumulatingIdenticalAnswers(cSource* questionSource, cSyntacticRelationGroup* srg, cAS candidateAnswer, int& maxAnswer, vector < cAS >& answerSRGs)
{
	vector <int> identicalAnswers;
	for (unsigned int I = 0; I < answerSRGs.size(); I++)
		if (checkIdentical(questionSource, &(*srg), answerSRGs[I], candidateAnswer) && answerSRGs[I].equivalenceClass == candidateAnswer.equivalenceClass)
			identicalAnswers.push_back(I);
	for (unsigned int I = 0; I < identicalAnswers.size(); I++)
		answerSRGs[identicalAnswers[I]].numIdenticalAnswers = identicalAnswers.size();
	maxAnswer = max(maxAnswer, candidateAnswer.matchSum);
	answerSRGs.push_back(candidateAnswer);
	candidateAnswer.source->answerContainedInSource++;
	return true;
}

/*
From dbPedia:
Paul Robin Krugman is an American economist, professor of Economics and International Affairs at the Woodrow Wilson School of Public and International Affairs at Princeton University
From wikipedia:
He taught at Yale University, MIT, UC Berkeley, the London School of Economics, and Stanford University before joining Princeton University in 2000 as professor of economics and international affairs.
*/
int cQuestionAnswering::determineBestAnswers(cSource* questionSource, cSyntacticRelationGroup* srg, vector < cAS >& answerSRGs,
	int maxAnswer, vector <cSyntacticRelationGroup>& subQueries, bool useParallelQuery)
{
	LFS
		if (maxAnswer <= 14)
			return 0;
	lplog(LOG_WHERE, L"(maxCertainty=%d):", maxAnswer);
	int lowestConfidence = 1000000;
	int highestIdenticalAnswers = -1;
	int lowestSourceConfidence = 1000000;
	int maxRecomputedAnswer = -1, maxAlternativeAnswer = -1;
	for (unsigned int I = 0; I < answerSRGs.size(); I++)
	{
		vector <cAS>::iterator as = answerSRGs.begin() + I;
		as->matchSumWithConfidenceAndNumIdenticalAnswersScored = -1;
		setWhereChildCandidateAnswer(questionSource, *as, &(*srg));
		// ************************************
		// check all answers that are most likely
		// ************************************
		if (as->matchSum >= 14 && as->whereChildCandidateAnswer >= 0)
		{
			bool unableToDoQuestionTypeCheck = true;
			int semanticMismatch = 0;
			wchar_t derivation[1024];
			StringCbPrintf(derivation, 1024 * sizeof(wchar_t), L"PW %06d:child answerSRG %d", srg->where, as->srg->where);
			as->confidence = questionTypeCheck(questionSource, derivation, &(*srg), *as, semanticMismatch, unableToDoQuestionTypeCheck);
			// if questionTypeCheck said:
			// CONFIDENCE_NOMATCH: no chance this could be the answer
			// 1: definitely matches the type of the answer
			// !QTAFlag - no semantic match to a where or who because where or who has nothing it is modifying so questionTypeCheck has done all the work possible
			if ((srg->questionType & QTAFlag) && (unableToDoQuestionTypeCheck || (as->confidence != CONFIDENCE_NOMATCH && (as->confidence != 1 || srg->questionType == unknownQTFlag))))
				as->confidence = semanticMatch(questionSource, derivation, &(*srg), *as, semanticMismatch);
			else
				if (unableToDoQuestionTypeCheck)
					as->confidence = CONFIDENCE_NOMATCH / 2; // unable to do any semantic checking
			bool subQueryNoMatch = false;
			if (!semanticMismatch)
			{
				if (as->source->m[as->whereChildCandidateAnswer].objectMatches.size() > 0)
				{
					if (as->object < 0 && as->source->m[as->whereChildCandidateAnswer].objectMatches.size() == 1)
						as->object = as->source->m[as->whereChildCandidateAnswer].objectMatches[0].object;
					if (as->object < 0 && as->source->m[as->whereChildCandidateAnswer].objectMatches.size()>1)
					{
						as->object = as->source->m[as->whereChildCandidateAnswer].objectMatches[0].object;
						lplog(LOG_ERROR, L"***Answer is not split!");
					}
					wstring tmpstr1, tmpstr2;
					int numWords = 0;
					lplog(LOG_WHERE, L"%d:subquery comparison whereChildCandidateAnswer=%d:%s moved to %d:%s", I,
						as->whereChildCandidateAnswer, as->source->whereString(as->whereChildCandidateAnswer, tmpstr1, false, 6, L" ", numWords).c_str(),
						as->source->objects[as->object].originalLocation, as->source->whereString(as->source->objects[as->object].originalLocation, tmpstr2, false, 6, L" ", numWords).c_str());
					as->whereChildCandidateAnswer = as->source->objects[as->object].originalLocation;
				}
				as->confidence = matchSubQueries(questionSource, derivation, as->source, semanticMismatch, subQueryNoMatch, subQueries, as->whereChildCandidateAnswer, -1, I, as->confidence, useParallelQuery);
				wstring tmpstr1, tmpstr2;
				int numWords = 0;
				lplog(LOG_WHERE, L"%d:subquery comparison whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d (1)", I,
					as->whereChildCandidateAnswer, as->source->whereString(as->whereChildCandidateAnswer, tmpstr2, false, 6, L" ", numWords).c_str(),
					semanticMismatch, (subQueryNoMatch) ? L"true" : L"false", as->confidence);
			}
			int tenseMatchReason = verbTenseMatch(questionSource, &(*srg), *as);
			if (isModifiedGeneric(*as))
				as->confidence = CONFIDENCE_NOMATCH / 2;
			if (as->confidence < CONFIDENCE_NOMATCH && (tenseMatchReason > 0 || as->confidence == 1 || as->matchSum > 20) && !semanticMismatch && !subQueryNoMatch)
			{
				if (tenseMatchReason == 0)
					as->confidence = CONFIDENCE_NOMATCH / 2;
				lowestConfidence = min(lowestConfidence, as->confidence);
				lowestSourceConfidence = min(lowestSourceConfidence, as->source->sourceConfidence);
				highestIdenticalAnswers = max(highestIdenticalAnswers, as->numIdenticalAnswers);
				maxRecomputedAnswer = max(maxRecomputedAnswer, as->matchSum);
				lplog(LOG_WHERE, L"(maxRecomputedAnswer=%d [%d %d %d %d %d %s %s %d]):", maxRecomputedAnswer, answerSRGs.size(), as->confidence, tenseMatchReason, as->confidence, as->matchSum, (semanticMismatch) ? L"semanticMismatch" : L"", (subQueryNoMatch) ? L"subQueryNoMatch" : L"", tenseMatchReason);
			}
			else
			{
				if (as->matchSum <= 16) as->rejectAnswer += L"[matchSum too low]";
				as->rejectAnswer += (srg->questionType & QTAFlag) ? L"[adjectival]" : L"[not adjectival]";
				as->rejectAnswer += (srg->questionType == unknownQTFlag) ? L"[unknown QT]" : L"[known QT]";
				as->rejectAnswer += (tenseMatchReason) ? L"[tense Match]" : L"[tense mismatch]";
				as->rejectAnswer += (semanticMismatch) ? L"[semantic Mismatch]" : L"[semantic match]";
				as->rejectAnswer += (subQueryNoMatch) ? L"[subQuery NoMatch]" : L"[subQuery match]";
				maxAlternativeAnswer = max(maxAlternativeAnswer, as->matchSum);
				lplog(LOG_WHERE, L"(maxAlternativeAnswer=%d [%d %d %d %d %d %s %s]):", maxAlternativeAnswer, answerSRGs.size(), as->confidence, tenseMatchReason, as->confidence, as->matchSum, (semanticMismatch) ? L"semanticMismatch" : L"", (subQueryNoMatch) ? L"subQueryNoMatch" : L"");
			}
		}
		else
		{
			if (maxAnswer > as->matchSum)
				maxAlternativeAnswer = max(maxAlternativeAnswer, as->matchSum);
			as->rejectAnswer += L"[matchSum too low]";
			lplog(LOG_WHERE, L"(maxAlternativeAnswer (2)=%d [%d]):", maxAlternativeAnswer, answerSRGs.size());
		}
	}
	lplog(LOG_WHERE, L"(final maxRecomputedAnswer=%d [%d]):", maxRecomputedAnswer, answerSRGs.size());
	if (maxRecomputedAnswer < 0)
		return 0;
	bool highCertaintyAnswer = false;
	int maxRecomputedAnswerWithConfidenceAndNumIdenticalAnswersScored = -1;
	for (unsigned int I = 0; I < answerSRGs.size(); I++)
	{
		vector <cAS>::iterator as = answerSRGs.begin() + I;
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
	for (unsigned int I = 0; I < answerSRGs.size(); I++)
	{
		vector <cAS>::iterator as = answerSRGs.begin() + I;
		if (maxRecomputedAnswerWithConfidenceAndNumIdenticalAnswersScored == as->matchSumWithConfidenceAndNumIdenticalAnswersScored && (!highCertaintyAnswer || as->source->sourceConfidence == lowestSourceConfidence))
		{
			bool identicalFound = false;
			for (unsigned int fa = 0; fa < finalAnswers.size() && !identicalFound; fa++)
				if (checkIdentical(questionSource, &(*srg), answerSRGs[I], answerSRGs[finalAnswers[fa]]) && answerSRGs[I].equivalenceClass == answerSRGs[finalAnswers[fa]].equivalenceClass)
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
			if (as->matchSum < maxRecomputedAnswer)
				as->rejectAnswer += L"match low " + itos(as->matchSum, tmpstr1) + L"<" + itos(maxRecomputedAnswer, tmpstr2) + L" ";
			if (as->source->sourceConfidence > lowestSourceConfidence)
				as->rejectAnswer += L"source confidence " + itos(as->source->sourceConfidence, tmpstr1) + L">" + itos(lowestSourceConfidence, tmpstr2) + L" ";
		}
	}
	return numFinalAnswers;
}

bool cQuestionAnswering::isModifiedGeneric(cAS& as)
{
	if (as.srg != 0 && as.whereChildCandidateAnswer >= 0 && as.source->m[as.whereChildCandidateAnswer].objectMatches.empty() &&
		as.source->m[as.whereChildCandidateAnswer].beginObjectPosition >= 0 && as.source->m[as.source->m[as.whereChildCandidateAnswer].beginObjectPosition].word->first == L"a")
		return true;
	return false;
}

int cQuestionAnswering::printAnswers(cSyntacticRelationGroup* srg, vector < cAS >& answerSRGs)
{
	LFS
		int numFinalAnswers = 0;
	if (answerSRGs.size() > 0)
	{
		lplog(LOG_WHERE, L"P%06d:ANSWER LIST  ************************************************************", srg->where);
		for (unsigned int J = 0; J < answerSRGs.size(); J++)
		{
			vector <cAS>::iterator as = answerSRGs.begin() + J;
			if (as->finalAnswer)
			{
				lplog(LOG_WHERE | LOG_QCHECK, L"    ANSWER %d:Identical Answers:%d:Object Match Confidence:%d:Source:%s:Source Confidence:%d", J, answerSRGs[J].numIdenticalAnswers, answerSRGs[J].confidence, as->sourceType.c_str(), as->source->sourceConfidence);
				if (as->srg && !as->fromWikipediaInfoBox)
				{
					wstring ps;
					as->source->prepPhraseToString(as->wp, ps);
					as->source->printSRG(L"        ", as->srg, 0, as->ws, as->wo, ps, false, as->matchSum, as->matchInfo, LOG_WHERE | LOG_QCHECK);
				}
				else if (as->fromTable)
				{
					wstring tmpstr;
					lplog(LOG_WHERE | LOG_QCHECK, L"       [TABLE %s %s:%d:%d:%d] %d:%s", as->tableNum.c_str(), as->tableName.c_str(), as->columnIndex, as->rowIndex, as->entryIndex, as->ws, as->source->phraseString(as->entry.begin, as->entry.begin + as->entry.numWords, tmpstr, true).c_str());
				}
				numFinalAnswers++;
			}
		}
		for (unsigned int J = 0; J < answerSRGs.size(); J++)
		{
			vector <cAS>::iterator as = answerSRGs.begin() + J;
			if (!as->finalAnswer)
			{
				lplog(LOG_WHERE, L"  REJECTED (%s%s) %d:Identical Answers:%d:Object Match Confidence:%d:Source:%s:Source Confidence:%d",
					as->rejectAnswer.c_str(), as->matchInfo.c_str(), J, as->numIdenticalAnswers, as->confidence, as->sourceType.c_str(), as->source->sourceConfidence);
				if (as->srg && !as->fromWikipediaInfoBox)
				{
					wstring ps;
					as->source->prepPhraseToString(as->wp, ps);
					as->source->printSRG(L"      ", as->srg, 0, as->ws, as->wo, ps, false, as->matchSum, as->matchInfo, LOG_WHERE);
				}
				else if (as->fromTable)
				{
					wstring tmpstr;
					lplog(LOG_WHERE, L"       [TABLE %s %s:%d:%d:%d] %d:%s", as->tableNum.c_str(), as->tableName.c_str(), as->columnIndex, as->rowIndex, as->entryIndex, as->ws, as->source->phraseString(as->entry.begin, as->entry.begin + as->entry.numWords, tmpstr, true).c_str());
				}
			}
		}
	}
	else
		lplog(LOG_WHERE, L"P%06d:ANSWER LIST EMPTY! ************************************************************", srg->where);
	return numFinalAnswers;
}

int	cQuestionAnswering::searchTableForAnswer(cSource* questionSource, wchar_t derivation[1024], cSyntacticRelationGroup* srg, unordered_map <int, cWikipediaTableCandidateAnswers* >& wikiTableMap,
	vector <cSyntacticRelationGroup>& subQueries, vector < cAS >& answerSRGs, int& minConfidence, bool useParallelQuery)
{
	LFS
		// look for the questionType object and its synonyms in the freebase properties of the questionInformationSourceObject.
		int whereQuestionTypeObject = srg->whereQuestionTypeObject, numConsideredParentAnswer = 0;
	if (whereQuestionTypeObject < 0) return -1;
	wstring tmpstr;
	int numTableAttempts = 0, numWords, maxAnswer = -1;
	unordered_map <int, cWikipediaTableCandidateAnswers* > ::iterator wtmi;
	for (set <int>::iterator si = srg->whereQuestionInformationSourceObjects.begin(), siEnd = srg->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
	{
		if ((wtmi = wikiTableMap.find(*si)) != wikiTableMap.end())
		{
			// if found, for each value of the property:
			for (vector < cSourceTable >::iterator wtvi = wtmi->second->wikiQuestionTypeObjectAnswers.begin(), wtviEnd = wtmi->second->wikiQuestionTypeObjectAnswers.end(); wtvi != wtviEnd; wtvi++)
			{
				numTableAttempts += wtvi->columns.size();
			}
		}
		if (logTableDetail)
			lplog(LOG_WHERE, L"*No tables for answer matching %s:", questionSource->whereString(*si, tmpstr, true).c_str());
	}
	if (!numTableAttempts)
	{
		for (unordered_map <int, cWikipediaTableCandidateAnswers* >::iterator wm = wikiTableMap.begin(), wmEnd = wikiTableMap.end(); wm != wmEnd; wm++)
			lplog(LOG_WHERE, L"*No answers for table mapping to whereQuestionInformationSourceObject %s", questionSource->whereString(wm->first, tmpstr, true).c_str());
		return -1;
	}
	lplog(LOG_WHERE, L"*Searching %d tables for answer:", numTableAttempts);
	wstring ps;
	questionSource->prepPhraseToString(srg->wherePrep, ps);
	questionSource->printSRG(L"", &(*srg), 0, srg->whereSubject, srg->whereObject, ps, false, -1, L"");
	int numTableAttemptsTotal = numTableAttempts, lastProgressPercent = 0, startTime = clock();
	numTableAttempts = 0;
	// which prize did Paul Krugman win?
	// if no answers and whereQuestionType is adjectival
	// for each of the questionInformationSourceObjects:
	for (set <int>::iterator si = srg->whereQuestionInformationSourceObjects.begin(), siEnd = srg->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
	{
		if ((wtmi = wikiTableMap.find(*si)) != wikiTableMap.end())
		{
			// if found, for each table in source:
			for (vector < cSourceTable >::iterator tableIterator = wtmi->second->wikiQuestionTypeObjectAnswers.begin(), wtviEnd = wtmi->second->wikiQuestionTypeObjectAnswers.end(); tableIterator != wtviEnd; tableIterator++)
			{
				int answersFromOneEntry = 0, whereLastEntryEnd = -1;
				wstring tableTitle = wtmi->second->wikipediaSource->phraseString(tableIterator->tableTitleEntry.begin, tableIterator->tableTitleEntry.begin + tableIterator->tableTitleEntry.numWords, tmpstr, false);
				tableIterator->tableTitleEntry.logEntry(LOG_WHERE, tableIterator->num.c_str(), -1, -1, tableIterator->source);
				// for each column in table
				for (vector <cColumn>::iterator columnIterator = tableIterator->columns.begin(), wtciEnd = tableIterator->columns.end(); columnIterator != wtciEnd; columnIterator++)
				{
					if (columnIterator->coherencyPercentage < 50)
					{
						columnIterator->logColumn(LOG_WHERE, L"REJECTED - COHERENCY", tableTitle.c_str());
						continue;
					}
					columnIterator->logColumn(LOG_WHERE, L"INITIALIZE", tableTitle);
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
								tableIterator->num.c_str(), numConsideredParentAnswer, (tableIterator->tableTitleEntry.lastWordFoundInTitleSynonyms) ? L" [matches question object]" : L"", whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str());
							entryIterator->logEntry(LOG_WHERE, tableIterator->num.c_str(), (int)(rowIterator - columnIterator->rows.begin()), (int)(entryIterator - rowIterator->entries.begin()), tableIterator->source);
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
								confidence = questionSource->checkParticularPartSemanticMatch(LOG_WHERE, whereQuestionTypeObject, wtmi->second->wikipediaSource, whereChildCandidateAnswer, -1, synonym, semanticMismatch, fileCaching) + 4;
							}
							lplog(LOG_WHERE, L"processing table %s: Q%d semantic comparison FINISHED between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields confidence %d [%d] [title entry=%s].",
								tableIterator->num.c_str(), numConsideredParentAnswer++, whereQuestionTypeObject, questionSource->whereString(whereQuestionTypeObject, tmpstr1, false).c_str(), whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str(), confidence, answersFromOneEntry, tableIterator->tableTitleEntry.sprint(tableIterator->source, tmpstr3).c_str());
							bool secondarySentencePosition = (answersFromOneEntry > 1 && (wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].objectRole & (OBJECT_ROLE | PREP_OBJECT_ROLE)));
							if (secondarySentencePosition)
							{
								matchInfo += L"[secondary sentence position]";
								confidence += CONFIDENCE_NOMATCH / 2;
							}
							if (confidence < CONFIDENCE_NOMATCH)
							{
								// do not consider any matching objects for the purposes of considering this object as an answer for the subquery.
								if (wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].getObject() < 0)
								{
									wtmi->second->wikipediaSource->objects.push_back(cObject(OC::NON_GENDERED_GENERAL_OBJECT_CLASS, cName(), entryIterator->begin, entryIterator->begin + entryIterator->numWords, whereChildCandidateAnswer, -1, -1, false, false, false, false, false));
									wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].setObject(wtmi->second->wikipediaSource->objects.size() - 1);
									wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].beginObjectPosition = entryIterator->begin;
									wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].endObjectPosition = entryIterator->begin + entryIterator->numWords;
								}
								if (wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].objectMatches.size() > 0)
									wtmi->second->wikipediaSource->m[whereChildCandidateAnswer].objectMatches.clear();
								cAS as(L"TABLE", wtmi->second->wikipediaSource, confidence, 1, matchInfo, NULL, 1, whereChildCandidateAnswer, 0, 0, false, true, tableIterator->num, tableTitle, columnIterator - tableIterator->columns.end(), rowIterator - columnIterator->rows.begin(), (int)(entryIterator - rowIterator->entries.begin()), &(*entryIterator));
								if (subQueries.empty())
								{
									minConfidence = min(minConfidence, confidence);
									enterAnswerAccumulatingIdenticalAnswers(questionSource, srg, as, maxAnswer, answerSRGs);
									continue;
								}
								semanticMismatch = 0;
								bool subQueryNoMatch = false;
								wchar_t sqDerivation[2048];
								StringCbPrintf(sqDerivation, 2048 * sizeof(wchar_t), L"%s whereQuestionInformationSourceObject(%d:%s) wikiQuestionTypeObjectAnswer(%d:%s)", derivation, *si, questionSource->whereString(*si, tmpstr2, true, 6, L" ", numWords).c_str(),
									whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr3, false).c_str());
								// run a subquery which substitutes the value for the object in a part of the sentence.
								matchSubQueries(questionSource, sqDerivation, wtmi->second->wikipediaSource, semanticMismatch, subQueryNoMatch, subQueries, whereChildCandidateAnswer, whereLastEntryEnd, numConsideredParentAnswer, confidence, useParallelQuery);
								as.subQueryExisted = subQueries.size() > 0;
								as.subQueryMatch = !subQueryNoMatch;
								if (subQueryNoMatch)
								{
									confidence = as.confidence += 1000;
									as.matchInfo += L"[subQueryNoMatch +1000]";
								}
								if (secondarySentencePosition)
									as.matchInfo += L"[secondarySentencePosition[" + itos(answersFromOneEntry, tmpstr) + L"+" + itos(CONFIDENCE_NOMATCH / 2, tmpstr2) + L"]";
								lplog(LOG_WHERE, L"%d:table subquery comparison between whereQuestionTypeObject=%d:%s and whereChildCandidateAnswer=%d:%s yields semanticMismatch=%d subQueryNoMatch=%s confidence=%d[%s]", numConsideredParentAnswer - 1,
									whereQuestionTypeObject, questionSource->whereString(whereQuestionTypeObject, tmpstr1, false).c_str(), whereChildCandidateAnswer, wtmi->second->wikipediaSource->phraseString(whereChildCandidateAnswer, whereLastEntryEnd, tmpstr2, false).c_str(),
									semanticMismatch, (subQueryNoMatch) ? L"true" : L"false", confidence, as.matchInfo.c_str());
								minConfidence = min(minConfidence, confidence);
								enterAnswerAccumulatingIdenticalAnswers(questionSource, srg, as, maxAnswer, answerSRGs);
							}
						}
					}
				}
			}
		}
	}
	return 0;
}


int cQuestionAnswering::findConstrainedAnswers(cSource* questionSource, vector < cAS >& answerSRGs, vector < cTrackDescendantAnswers>& descendantAnswers)
{
	LFS
		int whereChild = -1;
	set <wstring> constrainedAnswers;
	for (vector < cAS >::iterator as = answerSRGs.begin(), asEnd = answerSRGs.end(); as != asEnd; as++)
	{
		if (as->finalAnswer)
		{
			wstring tmpstr;
			cAS* transferAS = 0;
			int numWords;
			wstring kind;
			if (as->srg)
				kind = L"";
			else if (as->fromTable)
				kind = L"TABLE ";
			else
				kind = L"DB ";
			whereChild = (as->srg) ? as->whereChildCandidateAnswer : as->ws;
			for (cOM o : as->source->m[whereChild].objectMatches)
			{
				as->source->objectString(o.object, tmpstr, true);
				if (constrainedAnswers.find(tmpstr) == constrainedAnswers.end())
				{
					lplog(LOG_WHERE | LOG_QCHECK, L"    CONSTRAINED %sANSWER Source:%s:Source Confidence:%d: %s", kind.c_str(), as->sourceType.c_str(), as->source->sourceConfidence, tmpstr.c_str());
					transferAS = &(*as);
					constrainedAnswers.insert(tmpstr);
				}
				else
					lplog(LOG_WHERE | LOG_QCHECK, L"    CONSTRAINED %sDUPLICATE Source:%s:Source Confidence:%d: %s", kind.c_str(), as->sourceType.c_str(), as->source->sourceConfidence, tmpstr.c_str());
			}
			if (as->source->m[whereChild].objectMatches.empty())
			{
				as->source->whereString(whereChild, tmpstr, false, 6, L" ", numWords);
				if (constrainedAnswers.find(tmpstr) == constrainedAnswers.end())
				{
					lplog(LOG_WHERE | LOG_QCHECK, L"    CONSTRAINED %sANSWER Source:%s:Source Confidence:%d: %s", kind.c_str(), as->sourceType.c_str(), as->source->sourceConfidence, tmpstr.c_str());
					transferAS = &(*as);
					constrainedAnswers.insert(tmpstr);
				}
				else
					lplog(LOG_WHERE | LOG_QCHECK, L"    CONSTRAINED %sDUPLICATE Source:%s:Source Confidence:%d: %s", kind.c_str(), as->sourceType.c_str(), as->source->sourceConfidence, tmpstr.c_str());
			}
			if (transferAS)
			{
				unordered_map <int, int> transformSourceToQuestionSourceMap;
				if (as->source->m[whereChild].objectMatches.empty() && transferAS->source->m[whereChild].getObject() >= 0)
				{
					descendantAnswers.push_back(cTrackDescendantAnswers(transferAS->source->m[whereChild].getObject(), L"", -1, true));
					for (int I = transferAS->source->m[whereChild].beginObjectPosition; I < transferAS->source->m[whereChild].endObjectPosition; I++)
					{
						questionSource->copyChildrenIntoParent(transferAS->source, I, transformSourceToQuestionSourceMap, false);
					}
				}
				else if (as->source->m[whereChild].objectMatches.size() > 0)
				{
					for (cOM o : as->source->m[whereChild].objectMatches)
					{
						int wo = transferAS->source->objects[o.object].originalLocation;
						descendantAnswers.push_back(cTrackDescendantAnswers(o.object, L"", -1, true));
						for (int I = transferAS->source->m[wo].beginObjectPosition; I < transferAS->source->m[wo].endObjectPosition; I++)
							questionSource->copyChildrenIntoParent(transferAS->source, I, transformSourceToQuestionSourceMap, false);
					}
				}
				else
				{
					lplog(LOG_FATAL_ERROR, L"constrained answer object not found!");
					//for (int wpa : questionSource->copyChildrenIntoParent(transferAS->source, whereChild, transformSourceToQuestionSourceMap, false))
					//	wherePossibleAnswers.insert(wherePossibleAnswers.end(), wpa);
				}
				for (auto& pair : transformSourceToQuestionSourceMap)
				{
					if (pair.second < 0)
						continue;
					else if (questionSource->m[pair.second].sameSourceCopy < 0)
					{
						questionSource->m[pair.second].adjustReferences(pair.second, false, transformSourceToQuestionSourceMap);
						if (questionSource->m[pair.second].getObject() < -1000)
						{// see copyChildrenIntoParent
							lplog(LOG_WHERE, L"Setting negative object positive in adjust references - %d->%d object %d->%d!", pair.first, pair.second, questionSource->m[pair.second].getObject(), -questionSource->m[pair.second].getObject() - 1000); // SON[setObjectNegative]
							questionSource->m[pair.second].setObject(-questionSource->m[pair.second].getObject() - 1000);
						}
						else
							questionSource->m[pair.second].setObject(-1);
						questionSource->m[pair.second].objectMatches.clear();
					}
					else
						questionSource->m[pair.second].sameSourceCopy = -1;
				}
			}
		}
	}
	return descendantAnswers.size();
}

bool cQuestionAnswering::matchParticularAnswer(cSource* questionSource, cSyntacticRelationGroup* ssri, int whereMatch, int wherePossibleAnswer, set <int>& addWhereQuestionInformationSourceObjects)
{
	LFS
		if (whereMatch < 0 || wherePossibleAnswer < 0 || questionSource->inObject(whereMatch, ssri->whereQuestionType))
			return false;
	bool legalReference = false, notAlreadySelected = true, objectClass = true;
	int semanticMismatch = 0;
	int oc;
	wstring tmpstr, tmpstr2, tmpstr3;
	if ((legalReference = whereMatch >= 0 && questionSource->m[wherePossibleAnswer].getObject() >= 0) &&
		(notAlreadySelected = !questionSource->objectContainedIn(wherePossibleAnswer, ssri->whereQuestionInformationSourceObjects) &&
			!questionSource->objectContainedIn(wherePossibleAnswer, addWhereQuestionInformationSourceObjects)) &&
		(objectClass = (oc = questionSource->objects[questionSource->m[wherePossibleAnswer].getObject()].objectClass) == NAME_OBJECT_CLASS || oc == GENDERED_DEMONYM_OBJECT_CLASS || oc == NON_GENDERED_BUSINESS_OBJECT_CLASS || oc == NON_GENDERED_NAME_OBJECT_CLASS))
		//(confidenceMatch=checkParticularPartSemanticMatch(LOG_WHERE,whereMatch,this,wherePossibleAnswer,synonym,semanticMismatch)<CONFIDENCE_NOMATCH) && 
		//!semanticMismatch)
	{
		lplog(LOG_WHERE, L"%d:MAOPQ matchParticularAnswer SUCCESS: Pushing object %s to location %d:%s.",
			ssri->where, questionSource->whereString(wherePossibleAnswer, tmpstr, true).c_str(), whereMatch, questionSource->whereString(whereMatch, tmpstr2, true).c_str());
		questionSource->m[whereMatch].objectMatches.push_back(cOM(questionSource->m[wherePossibleAnswer].getObject(), 0));
		addWhereQuestionInformationSourceObjects.insert(whereMatch);
		return true;
	}
	else
		lplog(LOG_WHERE, L"%d:MAOPQ matchParticularAnswer REJECTED (%s%s%s%s):  %d:%s  =  %d:%s?", ssri->where,
			(legalReference) ? L"" : L"illegal", (notAlreadySelected) ? L"" : L"alreadySelected", (semanticMismatch) ? L"semanticMismatch" : L"", (objectClass) ? L"" : L"objectClass",
			whereMatch, questionSource->whereString(whereMatch, tmpstr2, true).c_str(),
			wherePossibleAnswer, questionSource->whereString(wherePossibleAnswer, tmpstr3, true).c_str());
	return false;
}

bool cQuestionAnswering::matchAnswerSourceMatch(cSource* questionSource, cSyntacticRelationGroup* ssri, int whereMatch, int wherePossibleAnswer, set <int>& addWhereQuestionInformationSourceObjects)
{
	LFS
		if (whereMatch < 0 || wherePossibleAnswer < 0 || questionSource->inObject(whereMatch, ssri->whereQuestionType))
			return false;
	bool legalReference = false, notAlreadySelected = true, wordMatch = true, objectClass = true;
	wstring tmpstr, tmpstr2;
	int oc;
	if ((legalReference = whereMatch >= 0 && questionSource->m[wherePossibleAnswer].getObject() >= 0) &&
		(notAlreadySelected = !questionSource->objectContainedIn(wherePossibleAnswer, ssri->whereQuestionInformationSourceObjects) &&
			!questionSource->objectContainedIn(wherePossibleAnswer, addWhereQuestionInformationSourceObjects)) &&
		(wordMatch = questionSource->m[whereMatch].getMainEntry() == questionSource->m[wherePossibleAnswer].getMainEntry()))
	{
		if (questionSource->m[wherePossibleAnswer].objectMatches.empty() && (objectClass = (oc = questionSource->objects[questionSource->m[wherePossibleAnswer].getObject()].objectClass) == NAME_OBJECT_CLASS || oc == GENDERED_DEMONYM_OBJECT_CLASS || oc == NON_GENDERED_BUSINESS_OBJECT_CLASS || oc == NON_GENDERED_NAME_OBJECT_CLASS))
		{
			lplog(LOG_WHERE, L"%d:MAOPQ matchAnswerSourceMatch SUCCESS: Pushing object %s to location %d:%s.", ssri->where, questionSource->whereString(wherePossibleAnswer, tmpstr, true).c_str(), whereMatch, questionSource->m[whereMatch].word->first.c_str());
			questionSource->m[whereMatch].objectMatches.push_back(cOM(questionSource->m[wherePossibleAnswer].getObject(), 0));
		}
		else
			lplog(LOG_WHERE, L"%d:MAOPQ matchAnswerSourceMatch SUCCESS: Matched object %s.", ssri->where, questionSource->whereString(wherePossibleAnswer, tmpstr, true).c_str());
		addWhereQuestionInformationSourceObjects.insert(whereMatch);
		return true;
	}
	else
		lplog(LOG_WHERE, L"%d:MAOPQ matchAnswerSourceMatch REJECTED (%s%s%s%s):%d:object=%s %s=%s?", ssri->where,
			(legalReference) ? L"" : L"illegal", (notAlreadySelected) ? L"" : L"alreadySelected", (wordMatch) ? L"" : L"noWordMatch", (objectClass) ? L"" : L"objectClass",
			whereMatch, questionSource->whereString(wherePossibleAnswer, tmpstr2, true).c_str(), questionSource->m[whereMatch].word->first.c_str(), questionSource->m[wherePossibleAnswer].word->first.c_str());
	return false;
}

int cQuestionAnswering::matchAnswersOfPreviousQuestion(cSource* questionSource, cSyntacticRelationGroup* ssri, set <int>& wherePossibleAnswers)
{
	LFS
		set <int> addWhereQuestionInformationSourceObjects;
	wstring tmpstr;
	lplog(LOG_WHERE, L"%d:MAOPQ BEGIN matchAnswersOfPreviousQuestion (%s)", ssri->where, questionSource->whereString(wherePossibleAnswers, tmpstr).c_str());
	// match the general object that the answers were linked to
	if (matchAnswerSourceMatch(questionSource, ssri, ssri->whereSubject, *wherePossibleAnswers.begin(), addWhereQuestionInformationSourceObjects))
		for (int wpai : wherePossibleAnswers)
			matchParticularAnswer(questionSource, ssri, ssri->whereSubject, wpai, addWhereQuestionInformationSourceObjects);
	if (matchAnswerSourceMatch(questionSource, ssri, ssri->whereObject, *wherePossibleAnswers.begin(), addWhereQuestionInformationSourceObjects))
		for (int wpai : wherePossibleAnswers)
			matchParticularAnswer(questionSource, ssri, ssri->whereObject, wpai, addWhereQuestionInformationSourceObjects);
	if (matchAnswerSourceMatch(questionSource, ssri, ssri->wherePrepObject, *wherePossibleAnswers.begin(), addWhereQuestionInformationSourceObjects))
		for (int wpai : wherePossibleAnswers)
			matchParticularAnswer(questionSource, ssri, ssri->wherePrepObject, wpai, addWhereQuestionInformationSourceObjects);
	if (addWhereQuestionInformationSourceObjects.size())
	{
		lplog(LOG_WHERE, L"%d:MAOPQ END matchAnswersOfPreviousQuestion adding: %s", ssri->where, questionSource->whereString(addWhereQuestionInformationSourceObjects, tmpstr).c_str());
		ssri->whereQuestionInformationSourceObjects.insert(addWhereQuestionInformationSourceObjects.begin(), addWhereQuestionInformationSourceObjects.end());
	}
	else
		lplog(LOG_WHERE, L"%d:MAOPQ END matchAnswersOfPreviousQuestion no answers added", ssri->where, questionSource->whereString(addWhereQuestionInformationSourceObjects, tmpstr).c_str());
	return -1;
}

extern int limitProcessingForProfiling;
int cQuestionAnswering::searchWebSearchQueries(cSource* questionSource, wchar_t derivation[1024], cSyntacticRelationGroup* ssri,
	vector <cSyntacticRelationGroup>& subQueries, vector < cAS >& answerSRGs,
	vector <wstring>& webSearchQueryStrings, bool parseOnly, int& numFinalAnswers, int& maxAnswer, bool useParallelQuery, int& trySearchIndex,
	bool useGoogleSearch, bool& lastResultPage)
{
	LFS
		StringCbPrintf(derivation, 1024 * sizeof(wchar_t), L"PW %06d", ssri->where);
	int webSearchQueryStringOffset = 0;
	if (useParallelQuery)
		lastResultPage = webSearchForQueryParallel(questionSource, derivation, ssri, parseOnly, answerSRGs, maxAnswer, 10, trySearchIndex, useGoogleSearch, webSearchQueryStrings, webSearchQueryStringOffset) < 10;
	else
		lastResultPage = webSearchForQuerySerial(questionSource, derivation, ssri, parseOnly, answerSRGs, maxAnswer, 10, trySearchIndex, useGoogleSearch, webSearchQueryStrings, webSearchQueryStringOffset) < 10;
	numFinalAnswers = determineBestAnswers(questionSource, ssri, answerSRGs, maxAnswer, subQueries, useParallelQuery);
	return numFinalAnswers;
}

void cQuestionAnswering::eraseSourcesMap()
{
	// memory optimization
	for (unordered_map <wstring, cSource*>::iterator smi = sourcesMap.begin(); smi != sourcesMap.end(); )
	{
		cSource* source = smi->second;
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

// When a question information source object owns another object, disambiguate this by seeing what objects matching the same type that are also very close and common to the question information source object, and return all these matching objects.
// This section is about impressionist Darrell Hammond.
// His shows appear on which network ?
int cQuestionAnswering::getProximateObjectsMatchingOwnedItemType(cSource* questionSource, int si, cSyntacticRelationGroup*& ssrg, set <int>& proximityOwnedObjects)
{
	// adjective, possessive determiner or _NAMEOWNER pattern, or SINGULAR_OWNER/PLURAL_OWNER inflection flag?
	if (questionSource->m[si].queryWinnerForm(adjectiveForm) >= 0 || questionSource->m[si].queryWinnerForm(possessiveDeterminerForm) >= 0 ||
		questionSource->queryPattern(si, L"_NAMEOWNER") != -1 || (questionSource->m[si].word->second.inflectionFlags & (SINGULAR_OWNER | PLURAL_OWNER)) != 0 &&
		questionSource->m[si].principalWherePosition > si)
	{
		int principalWherePosition = questionSource->m[si].principalWherePosition;
		int objectNum = questionSource->m[principalWherePosition].getObject();
		int objectClass = questionSource->objects[objectNum].objectClass;
		// object class must be a type or group not a specific object or person.
		if (objectClass == NAME_OBJECT_CLASS && !questionSource->objects[objectNum].neuter)
			return 0;
		set < unordered_map <wstring, cProximityMap::cProximityEntry>::iterator, cProximityMap::semanticSetCompare> localProximityMap;
		unordered_map <int, cProximityMap*>::iterator msi = ssrg->proximityMaps.find(si);
		if (msi != ssrg->proximityMaps.end())
		{
			msi->second->sortByFrequencyAndProximity(*this, ssrg, questionSource);
			set<int> checkObjects;
			int onlyTopResults = 0;
			for (auto sroi = msi->second->objectsSortedByFrequency.begin(), sroiEnd = msi->second->objectsSortedByFrequency.end(); sroi != sroiEnd && onlyTopResults < 10; sroi++, onlyTopResults++)
				localProximityMap.insert(*sroi);
			onlyTopResults = 0;
			for (auto sroi = msi->second->objectsSortedByProximityScore.begin(), sroiEnd = msi->second->objectsSortedByProximityScore.end(); sroi != sroiEnd && onlyTopResults < 10; sroi++, onlyTopResults++)
				localProximityMap.insert(*sroi);
			wstring pw = questionSource->m[questionSource->m[si].principalWherePosition].word->first, tmpstr3;
			transform(pw.begin(), pw.end(), pw.begin(), (int(*)(int)) tolower);
			wstring pwme = questionSource->m[principalWherePosition].getMainEntry()->first;
			unordered_set <wstring> parentSynonyms;
			questionSource->getSynonyms(pw, parentSynonyms, NOUN);
			if (pw != pwme)
				questionSource->getSynonyms(pwme, parentSynonyms, NOUN);
			wstring psStr;
			lplog(LOG_WHERE, L"Ownership proximity looking for: %s:%s", pwme.c_str(), setString(parentSynonyms, psStr, L" ").c_str());
			for (auto& po : localProximityMap)
			{
				po->second.childObject = questionSource->createObject(L"proximity", po->first);
				vector <cTreeCat*> rdfTypes;
				unordered_map <wstring, int > topHierarchyClassIndexes;
				questionSource->getExtendedRDFTypesMaster(questionSource->objects[po->second.childObject].originalLocation, -1, rdfTypes, topHierarchyClassIndexes, TEXT(__FUNCTION__));
				po->second.confidenceSE = -1;
				for (vector <cTreeCat*>::iterator ri = rdfTypes.begin(), riEnd = rdfTypes.end(); ri != riEnd; ri++)
				{
					if (logDetail)
						(*ri)->lplogTC(LOG_WHERE, L"whereQuestionInformationSourceObjects");
					int confidenceSE = -1;
					if ((*ri)->cli->first == pwme)
						confidenceSE = 1;
					if ((*ri)->cli->second.superClasses.find(pwme) != (*ri)->cli->second.superClasses.end())
					{
						confidenceSE = 2;
					}
					if (parentSynonyms.find((*ri)->cli->first) != parentSynonyms.end())
					{
						confidenceSE = 3;
					}
					// find intersection between parentSynonyms and super classes
					vector <wstring> commonWords;
					if (parentSynonyms.size() < (*ri)->cli->second.superClasses.size())
					{
						for (auto& psw : parentSynonyms)
							if ((*ri)->cli->second.superClasses.find(psw) != (*ri)->cli->second.superClasses.end())
								commonWords.push_back(psw);
					}
					else
					{
						for (auto& scw : (*ri)->cli->second.superClasses)
							if (parentSynonyms.find(scw) != parentSynonyms.end())
								commonWords.push_back(scw);
					}
					if (commonWords.size() > 0)
						confidenceSE = 4;
					if (confidenceSE >= 0 && confidenceSE >= po->second.confidenceSE)
					{
						po->second.confidenceSE = confidenceSE;
						auto p = proximityOwnedObjects.insert(po->second.childObject);
						if (p.second)
							lplog(LOG_WHERE, L"Ownership proximity FOUND [%d]: %s - %s", po->second.confidenceSE, pwme.c_str(), po->first.c_str());
					}
				}
			}
		}
	}
	return proximityOwnedObjects.size();
}

extern int limitProcessingForProfiling;
int cQuestionAnswering::answerQuestionInSource(cSource* questionSource, bool parseOnly, bool useParallelQuery, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& ssrg, vector < cTrackDescendantAnswers>& descendantAnswers, bool disableWebSearch)
{
	if (!srg->questionType || srg->skip)
		return -1;
	wstring derivation;
	eraseSourcesMap();
	childCandidateAnswerMap.clear();
	// **************************************************************
	// prepare question
	// **************************************************************
	srg->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, &(*srg));
	// For which newspaper does Krugman write?
	isQuestionPassive(questionSource, srg, ssrg);
	// if it fits a traceQuestionPatternMap pattern found in questionTransforms, set question and answer patterns within srg.
	findMetanamePatterns(questionSource, srg);
	// for all questions matching patterns, there may be multiple answers for a linked pattern group, therefore transformQuestion handles answering the question internally.
	if (transformQuestion(questionSource, &(*srg), ssrg, descendantAnswers, parseOnly, useParallelQuery, disableWebSearch) != -1)
		return 0;
	ssrg->whereQuestionTypeObject = getWhereQuestionTypeObject(questionSource, ssrg);
	// **************************************************************
	// log question
	// **************************************************************
	wstring ps, parentNum, tmpstr, tmpstr2;
	itos(ssrg->where, parentNum);
	//if (tsrg->where==69) 
		//logDatabaseDetails = logQuestionProfileTime = logSynonymDetail = logTableDetail = equivalenceLogDetail = logQuestionDetail = logProximityMap = 1;
	parentNum += L":Q ";
	questionSource->prepPhraseToString(ssrg->wherePrep, ps);
	questionSource->printSRG(parentNum, ssrg, -1, ssrg->whereSubject, ssrg->whereObject, ps, false, -1, L"QUESTION", (ssrg->questionType) ? LOG_WHERE | LOG_QCHECK : LOG_WHERE);
	// **************************************************************
	// check databases for answer.  
	// **************************************************************
	vector < cAS > answerSRGs;
	wchar_t sqderivation[4096];
	matchOwnershipDbQuery(questionSource, sqderivation, ssrg);
	if (!dbSearchForQuery(questionSource, sqderivation, ssrg, answerSRGs))
	{
		// **************************************************************
		// scan abstract and wikipedia articles from RDF types for the answer.
		// **************************************************************
		int maxAnswer = -1;
		unordered_map <int, cWikipediaTableCandidateAnswers* > wikiTableMap;
		analyzeRDFTypes(questionSource, srg, ssrg, derivation, answerSRGs, maxAnswer, wikiTableMap, false);
		if (ssrg->whereQuestionTypeObject < 0)
		{
			lplog(LOG_WHERE, L"whereQuestionTypeObject is negative.  Skipping rest of answer processing.");
			return -1;
		}
		// **************************************************************
		// detect subqueries
		// **************************************************************
		vector <cSyntacticRelationGroup> subQueries;
		if (&(*srg) == ssrg)
		{
			questionSource->printSRG(parentNum, &(*srg), 0, srg->whereSubject, srg->whereObject, ps, false, -1, L"");
			detectSubQueries(questionSource, srg, subQueries);
		}
		else
		{
			lplog(LOG_WHERE, L"Transformed:");
			questionSource->printSRG(parentNum, ssrg, 0, ssrg->whereSubject, ssrg->whereObject, ps, false, -1, L"");
		}
		// **************************************************************
		// determine best answer and print them.
		// **************************************************************
		int numFinalAnswers = determineBestAnswers(questionSource, ssrg, answerSRGs, maxAnswer, subQueries, useParallelQuery);
		// **************************************************************
		// if there are no answers or more than one answer is asked for and there is only one, search web first 10 results, then search the wikipredia tables (from analyzeRDFTypes), then search the rest of the web results.
		// **************************************************************
		bool answerPluralSpecification = (questionSource->m[ssrg->whereQuestionTypeObject].word->second.inflectionFlags & PLURAL) == PLURAL;
		if (numFinalAnswers <= 0 || (answerPluralSpecification && numFinalAnswers <= 1))
		{
			vector <wstring> webSearchQueryStrings;
			getWebSearchQueries(questionSource, ssrg, webSearchQueryStrings);
			bool lastGoogleResultPage = disableWebSearch, lastBINGResultPage = disableWebSearch, webSearchOrWikipediaTableSuccess = false;
			for (int trySearchIndex = 1; trySearchIndex <= 20; trySearchIndex += 10)
			{
				vector < cAS > webSearchAnswerSRIs;
				// search google with webSearchQueryStrings, starting at trySearchIndex
				if (!lastGoogleResultPage)
				{
					searchWebSearchQueries(questionSource, sqderivation, ssrg, subQueries, webSearchAnswerSRIs, webSearchQueryStrings,
						parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, true, lastGoogleResultPage);
					if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
						answerSRGs.insert(answerSRGs.end(), webSearchAnswerSRIs.begin(), webSearchAnswerSRIs.end());
					else
						printAnswers(ssrg, webSearchAnswerSRIs); // print all rejected answers
					if (numFinalAnswers > 0 && (!answerPluralSpecification || numFinalAnswers >= 1))
						break;
					webSearchAnswerSRIs.clear();
				}
				// search BING with webSearchQueryStrings, starting at trySearchIndex
				if (!lastBINGResultPage)
				{
					searchWebSearchQueries(questionSource, sqderivation, ssrg, subQueries, webSearchAnswerSRIs, webSearchQueryStrings,
						parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, false, lastBINGResultPage);
					if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
						answerSRGs.insert(answerSRGs.end(), webSearchAnswerSRIs.begin(), webSearchAnswerSRIs.end());
					else
						printAnswers(ssrg, webSearchAnswerSRIs); // print all rejected answers
					if (numFinalAnswers > 0 && (!answerPluralSpecification || numFinalAnswers >= 1))
						break;
				}
				// search wikipedia tables after the first page of Google and BING results if no answers (or insufficient number of answers) are found.
				if (trySearchIndex == 1 && ssrg->whereQuestionInformationSourceObjects.size() > 0)
				{
					vector < cAS > wikipediaTableAnswerSRIs;
					int lowestConfidence = 100000 - 1;
					searchTableForAnswer(questionSource, sqderivation, ssrg, wikiTableMap, subQueries, wikipediaTableAnswerSRIs, lowestConfidence, useParallelQuery);
					bool atLeastOneFinalAnswer = false;
					for (unsigned int I = 0; I < wikipediaTableAnswerSRIs.size(); I++)
						if (wikipediaTableAnswerSRIs[I].confidence == lowestConfidence)
							atLeastOneFinalAnswer = wikipediaTableAnswerSRIs[I].finalAnswer = true;
						else
							wikipediaTableAnswerSRIs[I].rejectAnswer += L"[low confidence]";
					if (webSearchOrWikipediaTableSuccess = atLeastOneFinalAnswer)
					{
						answerSRGs.insert(answerSRGs.end(), wikipediaTableAnswerSRIs.begin(), wikipediaTableAnswerSRIs.end());
						break;
					}
					else
						printAnswers(ssrg, wikipediaTableAnswerSRIs); // print all rejected answers
				}
			}
			// **************************************************************
			// ownership query - replace the items owned by the information source with a specific item using rdfTypes.  Then retry query.
			// This section is about impressionist Darrell Hammond.
			// 'His shows appear on which network?' --> 'Saturday Night Live appear on which network?'
			// **************************************************************
			if (!webSearchOrWikipediaTableSuccess && !disableWebSearch)
			{
				// for each information object (ex. Darrell Hammond)
				for (int si : ssrg->whereQuestionInformationSourceObjects)
				{
					set <wstring> proximityOwnedItems;
					// When a question information source object owns another object, disambiguate this by seeing 
					// what objects matching the same type that are also very close and common to the question information source object, 
					// and return all these matching objects.
					set <int> proximityOwnedObjects;
					if (getProximateObjectsMatchingOwnedItemType(questionSource, si, ssrg, proximityOwnedObjects))
					{
						for (int proximityOwnedObject : proximityOwnedObjects)
						{
							//construct query from ssrg but substituting proximityOwnedItems for the owned object
							cSyntacticRelationGroup psrg = *ssrg, * ppsrg = &psrg;
							int replacementWhere = -1;
							if (questionSource->inObject(ssrg->whereSubject, si))
								replacementWhere = psrg.whereSubject;
							else if (questionSource->inObject(srg->whereObject, si))
								replacementWhere = psrg.whereObject;
							else if (questionSource->inObject(srg->wherePrepObject, si))
								replacementWhere = psrg.wherePrepObject;
							else if (questionSource->inObject(srg->whereSecondaryObject, si))
								replacementWhere = psrg.whereSecondaryObject;
							else if (srg->whereSecondaryPrep >= 0 && questionSource->inObject(questionSource->m[srg->whereSecondaryPrep].getRelObject(), srg->whereQuestionType))
							{
								lplog(LOG_FATAL_ERROR, L"ProximityOwnershipQuery:Unable to replace this object to a secondary prep.");
							}
							else
							{
								lplog(LOG_WHERE | LOG_INFO, L"ProximityOwnershipQuery:Cannot find owned item type %d in these fields: S=%d, O=%d, PO=%d, SO=%d",
									si, srg->whereSubject, srg->whereObject, srg->wherePrepObject, srg->whereSecondaryObject);
								continue;
							}
							questionSource->m[replacementWhere].objectMatches.clear();
							questionSource->m[replacementWhere].objectMatches.push_back(cOM(proximityOwnedObject, 1000));
							psrg.whereQuestionInformationSourceObjects.erase(si);
							psrg.whereQuestionInformationSourceObjects.insert(questionSource->objects[proximityOwnedObject].originalLocation);
							lplog(LOG_WHERE | LOG_INFO, L"ProximityOwnershipQuery:Successfully replaced owned item type at %d with specific object %s: S=%d, O=%d, PO=%d, SO=%d, whereQuestionTypeObject=%d",
								replacementWhere, questionSource->whereString(replacementWhere, tmpstr, true).c_str(), srg->whereSubject, srg->whereObject, srg->wherePrepObject, srg->whereSecondaryObject, srg->whereQuestionTypeObject);
							questionSource->printSRG(L"ProximityOwnershipQuery", &psrg, 0, srg->whereSubject, srg->whereObject, psrg.wherePrep, false, -1, L"", LOG_WHERE);
							if (answerQuestionInSource(questionSource, parseOnly, useParallelQuery, &psrg, ppsrg, descendantAnswers, disableWebSearch) >= 0)
							{
								for (auto& wpa : descendantAnswers)
								{
									wstring outputAnswer;
									questionSource->objectString(wpa.inputAnswerObject, outputAnswer, true);
									answerSRGs.push_back(cAS(L"ProximityOwnershipQuery", questionSource, 1, 1000, outputAnswer, ppsrg, 0, srg->whereSubject, srg->whereObject, srg->wherePrep, false, false, L"", L"", 0, 0, 0, NULL));
									answerSRGs[answerSRGs.size() - 1].object = wpa.inputAnswerObject;
									webSearchOrWikipediaTableSuccess = true;
								}
							}
						}
					}
				}
			}
			// **************************************************************
			// if there are no answers or more than one answer is asked for and there is only one, search the first 10 results from Google and BING using more information sources from proximity mapping.
			// **************************************************************
			if (!webSearchOrWikipediaTableSuccess && !disableWebSearch)
			{
				lplog(LOG_WHERE | LOG_QCHECK, L"    ***** proximity map");
				for (set <int>::iterator si = ssrg->whereQuestionInformationSourceObjects.begin(), siEnd = ssrg->whereQuestionInformationSourceObjects.end(); si != siEnd; si++)
				{
					unordered_map <int, cProximityMap*>::iterator msi = ssrg->proximityMaps.find(*si);
					if (msi != ssrg->proximityMaps.end())
					{
						msi->second->sortByFrequencyAndProximity(*this, ssrg, questionSource);
						msi->second->lplogFrequentOrProximateObjects(*this, LOG_WHERE, questionSource, false);
						set < unordered_map <wstring, cProximityMap::cProximityEntry>::iterator, cProximityMap::semanticSetCompare > frequentOrProximateObjects = msi->second->frequentOrProximateObjects;
						for (set < unordered_map <wstring, cProximityMap::cProximityEntry>::iterator, cProximityMap::semanticSetCompare >::iterator sai = frequentOrProximateObjects.begin(), saiEnd = frequentOrProximateObjects.end(); sai != saiEnd; sai++)
						{
							vector < cAS > enhancedWebSearchAnswerSRIs;
							vector <wstring> enhancedWebSearchQueryStrings = webSearchQueryStrings;
							enhanceWebSearchQueries(enhancedWebSearchQueryStrings, (*sai)->first);
							int trySearchIndex = 1;
							// google
							searchWebSearchQueries(questionSource, sqderivation, ssrg, subQueries, enhancedWebSearchAnswerSRIs, enhancedWebSearchQueryStrings,
								parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, true, lastGoogleResultPage);
							if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
								answerSRGs.insert(answerSRGs.end(), enhancedWebSearchAnswerSRIs.begin(), enhancedWebSearchAnswerSRIs.end());
							else
								printAnswers(ssrg, enhancedWebSearchAnswerSRIs); // print all rejected answers
							if (numFinalAnswers > 0 && (!answerPluralSpecification || numFinalAnswers >= 1))
								break;
							enhancedWebSearchAnswerSRIs.clear();
							// BING
							searchWebSearchQueries(questionSource, sqderivation, ssrg, subQueries, enhancedWebSearchAnswerSRIs, enhancedWebSearchQueryStrings,
								parseOnly, numFinalAnswers, maxAnswer, useParallelQuery, trySearchIndex, true, lastGoogleResultPage);
							if (webSearchOrWikipediaTableSuccess = numFinalAnswers > 0)
								answerSRGs.insert(answerSRGs.end(), enhancedWebSearchAnswerSRIs.begin(), enhancedWebSearchAnswerSRIs.end());
							else
								printAnswers(ssrg, enhancedWebSearchAnswerSRIs); // print all rejected answers
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
	printAnswers(ssrg, answerSRGs);
	findConstrainedAnswers(questionSource, answerSRGs, descendantAnswers);
	return 0;
}

int cQuestionAnswering::answerAllQuestionsInSource(cSource* questionSource, bool parseOnly, bool useParallelQuery)
{
	LFS
		int lastProgressPercent = -1, where;
	bool disableWebSearch = false;
	for (vector <cSyntacticRelationGroup>::iterator srg = questionSource->syntacticRelationGroups.begin(), sriEnd = questionSource->syntacticRelationGroups.end(); srg != sriEnd; srg++)
	{
		cSyntacticRelationGroup* ssri = NULL;
		vector < cTrackDescendantAnswers> descendantAnswers;
		answerQuestionInSource(questionSource, parseOnly, useParallelQuery, &(*srg), ssri, descendantAnswers, disableWebSearch);
		if ((where = (srg - questionSource->syntacticRelationGroups.begin()) * 100 / questionSource->syntacticRelationGroups.size()) > lastProgressPercent)
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
