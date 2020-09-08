#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <winsock.h>
#include "Winhttp.h"
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
#include "profile.h"
#include "mysqldb.h"
int SRIDebugCounter = 0;
#define NULLWORD 187

const wchar_t *cSource::wchr(int where)
{
	LFS
		return (where < 0) ? L"" : m[where].word->first.c_str();
}

int cSource::gmo(int wo)
{
	LFS
		if (wo < 0) return wo;
	return (m[wo].endObjectPosition > wo) ? m[wo].endObjectPosition : wo;
}

const wchar_t *cSource::wrti(int where, wchar_t *id, wstring &tmpstr, bool shortFormat)
{
	LFS
	if (where < 0) return L"";
	if (where >= m.size())
	{
		tmpstr = L"Illegal!";
		return tmpstr.c_str();
	}
	wstring ws;
	whereString(where, ws, shortFormat);
	tIWMM w = fullyResolveToClass(where);
	wstring lcws = ws, lcw = w->first;
	std::transform(lcws.begin(), lcws.end(), lcws.begin(), ::tolower);
	std::transform(lcw.begin(), lcw.end(), lcw.begin(), ::tolower);
	if (w == wNULL || w->second.index < 0 || lcws.find(lcw) != wstring::npos)
		tmpstr = L"[" + wstring(id) + L" " + ws + L"]";
	else
		tmpstr = L"[" + wstring(id) + L" (" + ws + L") " + w->first + L"]";
	return tmpstr.c_str();
}

// get all prepositions, discard any prepositions that have attached objects as their head (subjects or objects of a verb) that are not the wo in question.
// Krugman earned his B.A. in economics from Yale University summa cum laude in 1974 and his PhD from the Massachusetts Institute of Technology (MIT) in 1977.
// please also see http://aclweb.org/anthology-new/J/J06/J06-3002.pdf
// 
void cSource::getAllPreps(cSpaceRelation* sri, set <int> &relPreps, int wo)
{
	LFS
		if (sri->whereVerb >= 0 && sri->wherePrep != m[sri->whereVerb].relPrep)
			for (int wp = m[sri->whereVerb].relPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
				if (checkInsertPrep(relPreps, wp, wo) < 0)
					break;
	for (int wp = sri->wherePrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
		if (checkInsertPrep(relPreps, wp, wo) < 0)
			break;
	for (int wp = sri->whereSecondaryPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
		if (checkInsertPrep(relPreps, wp, wo) < 0)
			break;
	if (wo >= 0 && m[wo].relPrep >= 0)
	{
		for (int wp = m[wo].relPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
			if (checkInsertPrep(relPreps, wp, wo) < 0)
				break;
	}
}

void cSource::prepPhraseToString(int wherePrep, wstring &ps)
{
	LFS
		if (wherePrep < 0) return;
	int wherePrepObject = m[wherePrep].getRelObject();
	if (wherePrepObject < 0) return;
	wstring tmpstr1, tmpstr2,ws;
	ps += wchr(wherePrep) + getWOSAdjective(wherePrepObject, tmpstr1) + L" " + getWSAdjective(wherePrepObject, 0) + L" " + getWSAdjective(wherePrepObject, 1) + L" " + itos(wherePrepObject,ws) + L":" + wrti(wherePrepObject, L"PO", tmpstr2);
	// compound nouns
	int compoundCount = 0;
	while (wherePrep >= 0 && wherePrepObject >= 0 && (wherePrepObject = m[wherePrepObject].nextCompoundPartObject) >= 0 && compoundCount++ < 10)
		ps += wchr(wherePrep) + getWOSAdjective(wherePrepObject, tmpstr1) + L" " + getWSAdjective(wherePrepObject, 0) + L" " + getWSAdjective(wherePrepObject, 1) + L" " + itos(wherePrepObject, ws) + L":" + wrti(wherePrepObject, L"POC", tmpstr2);
}

int cSource::checkInsertPrep(set <int> &relPreps, int wp, int wo)
{
	if (wp >= m.size() || relPreps.find(wp) != relPreps.end())
		return -1;
	if (wo < 0 || m[wp].nextQuote < 0 || m[wp].nextQuote == wo) // get only the prepositions attached to the wo
		relPreps.insert(wp);
	return 0;
}

int cSource::getProfession(int object)
{
	LFS
		if (object < 0) return NULLWORD;
	int where = m[object].principalWherePosition;
	if (where < 0) return NULLWORD;
	if (objects[object].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
		return m[where].word->second.index;
	if (m[where].objectMatches.size() == 1 && objects[object = m[where].objectMatches[0].object].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
		objects[object].originalLocation >= 0)
		return m[objects[object].originalLocation].word->second.index;
	return NULLWORD;
}

void cSource::getSRIMinMax(cSpaceRelation* sri)
{
	LFS
		sri->printMin = 10000;
	sri->printMax = -1;
	if (sri->whereControllingEntity >= 0)
	{
		sri->printMin = min(sri->printMin, sri->whereControllingEntity);
		if (m[sri->whereControllingEntity].getObject() >= 0)
			sri->printMin = min(sri->printMin, m[sri->whereControllingEntity].beginObjectPosition);
	}
	if (sri->whereSubject >= 0)
	{
		sri->printMin = min(sri->printMin, sri->whereSubject);
		if (m[sri->whereSubject].getObject() >= 0)
			sri->printMin = min(sri->printMin, m[sri->whereSubject].beginObjectPosition);
	}
	if (sri->whereVerb >= 0) sri->printMin = min(sri->printMin, sri->whereVerb);
	if (sri->whereObject >= 0) sri->printMin = min(sri->printMin, sri->whereObject);

	sri->printMax = max(sri->printMax, gmo(sri->whereControllingEntity));
	sri->printMax = max(sri->printMax, gmo(sri->whereSubject));
	sri->printMax = max(sri->printMax, sri->whereVerb);
	sri->printMax = max(sri->printMax, gmo(sri->whereObject));
	if (sri->whereObject >= 0) sri->printMax = max(sri->printMax, gmo(m[sri->whereObject].nextCompoundPartObject));
	if (sri->whereVerb >= 0) sri->printMax = max(sri->printMax, m[sri->whereVerb].getRelVerb());
	if (sri->whereVerb >= 0 && m[sri->whereVerb].getRelVerb() >= 0) sri->printMax = max(sri->printMax, gmo(m[m[sri->whereVerb].getRelVerb()].getRelObject()));
	set <int> relPreps;
	for (int wp = sri->wherePrep; wp >= 0 && wp < (int)m.size(); wp = m[wp].relPrep)
	{
		if (relPreps.find(wp) != relPreps.end())
			break;
		relPreps.insert(wp);
	}
	for (int wp = sri->whereSecondaryPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
	{
		if (relPreps.find(wp) != relPreps.end())
			break;
		relPreps.insert(wp);
	}
	for (set <int>::iterator rp = relPreps.begin(), rpEnd = relPreps.end(); rp != rpEnd; rp++)
	{
		if (*rp != sri->transformedPrep)
			sri->printMin = min(sri->printMin, *rp);
		if (m[*rp].getRelObject() >= 0) sri->printMin = min(sri->printMin, m[*rp].getRelObject());
		if (*rp != sri->transformedPrep)
			sri->printMax = max(sri->printMax, *rp);
		sri->printMax = max(sri->printMax, gmo(m[*rp].getRelObject()));
		if (m[*rp].getRelObject() >= 0) sri->printMax = max(sri->printMax, gmo(m[m[*rp].getRelObject()].nextCompoundPartObject));
	}
	if (sri->whereQuestionType >= 0) sri->printMin = min(sri->printMin, sri->whereQuestionType);
}

wstring cSource::getWSAdjective(int where, int numOrder)
{
	LFS
		if (where < 0) return L"";
	int object = m[where].getObject();
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1 && where > 0)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
			if (acceptableAdjective(I) && !acceptableObjectPosition(I))
			{
				if (numOrder == 0)
					return m[I].word->first;
				else
					numOrder--;
			}
	}
	return L"";
}

int cSource::getOSAdjective(int whereVerb, int where)
{
	LFS
		if (where >= 0)
			return getOSAdjective(where);
	if (whereVerb < 0) return -1;
	if (whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 1))
		return (m[whereVerb + 1].getObject() >= 0) ? m[whereVerb + 1].getObject() : m[whereVerb + 1].objectMatches[0].object;
	if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && m[whereVerb + 2].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 2))
		return (m[whereVerb + 2].getObject() >= 0) ? m[whereVerb + 2].getObject() : m[whereVerb + 2].objectMatches[0].object;
	return -1;
}

wstring cSource::getWOSAdjective(int whereVerb, int where, wstring &tmpstr)
{
	LFS
		getWOSAdjective(where, tmpstr);
	if (tmpstr.empty() && where < 0 && whereVerb >= 0)
	{
		if (whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 1))
			return whereString(whereVerb + 1, tmpstr, false);
		if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && m[whereVerb + 2].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 2))
			return whereString(whereVerb + 2, tmpstr, false);
	}
	return tmpstr;
}

int cSource::getMSAdjective(int whereVerb, int where, int numOrder)
{
	LFS
		if (where >= 0)
			return getMSAdjective(where, numOrder);
	if (numOrder != 0) return NULLWORD;
	int i;
	// How old is Darrell Hammond?
	//if (where<0 && whereVerb>=0 && (m[whereVerb].flags&cWordMatch::flagInQuestion) && m[whereVerb].relSubject<0 &&
	//	  m[whereVerb-1].queryWinnerForm(adjectiveForm)>=0 && m[whereVerb-1].getObject()<0)
	//	return ((i=m[whereVerb-1].word->second.index)<0) ? NULLWORD : i;
	if (whereVerb < 0 || (whereVerb + 1 >= where && where >= 0)) return NULLWORD;
	int maxEnd, q2IElement = queryPattern(whereVerb, L"_Q2", maxEnd);
	if (q2IElement >= 0 && patterns[pema[q2IElement].getParentPattern()]->differentiator == L"I" && m[whereVerb].relSubject >= 0 && (i = m[m[whereVerb].relSubject].endObjectPosition) >= 0 &&
		(m[i].queryWinnerForm(adjectiveForm) >= 0 || (m[i].queryWinnerForm(quoteForm) >= 0 && i + 1 < (signed)m.size() && m[i = i + 1].queryWinnerForm(adjectiveForm) >= 0))) // What is "WWE" short for?
		return ((i = m[i].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb + 1 < (signed)m.size() && acceptableAdjective(whereVerb + 1) && !acceptableObjectPosition(whereVerb + 1))
		return ((i = m[whereVerb + 1].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && acceptableAdjective(whereVerb + 2) && !acceptableObjectPosition(whereVerb + 2))
		return ((i = m[whereVerb + 2].word->second.index) < 0) ? NULLWORD : i;
	return NULLWORD;
}

wstring cSource::getWSAdjective(int whereVerb, int where, int numOrder, wstring &tmpstr)
{
	LFS
		tmpstr = getWSAdjective(where, numOrder);
	if (numOrder != 0)
		return tmpstr;
	if (tmpstr.empty() && where < 0 && whereVerb >= 0)
	{
		int maxEnd, i, q2IElement = queryPattern(whereVerb, L"_Q2", maxEnd);
		if (q2IElement >= 0 && patterns[pema[q2IElement].getParentPattern()]->differentiator == L"I" && m[whereVerb].relSubject >= 0 && (i = m[m[whereVerb].relSubject].endObjectPosition) >= 0 &&
			(m[i].queryWinnerForm(adjectiveForm) >= 0 || (m[i].queryWinnerForm(quoteForm) >= 0 && i + 1 < (signed)m.size() && m[i = i + 1].queryWinnerForm(adjectiveForm) >= 0))) // What is "WWE" short for?
			return m[i].word->first;
		if (whereVerb + 1 < (signed)m.size() && acceptableAdjective(whereVerb + 1) && !acceptableObjectPosition(whereVerb + 1))
			return m[whereVerb + 1].word->first;
		if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && acceptableAdjective(whereVerb + 2) && !acceptableObjectPosition(whereVerb + 2))
			return m[whereVerb + 2].word->first;
	}
	return tmpstr;
}

int cSource::getMSAdverb(int whereVerb, bool changeStateAdverb)
{
	LFS
		int i = -1;
	if (whereVerb >= 0 && whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0)
		return ((i = m[whereVerb + 1].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb > 0 && m[whereVerb - 1].queryWinnerForm(adverbForm) >= 0)
		return ((i = m[whereVerb - 1].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb > 0 && changeStateAdverb)
	{
		int timeFlag = (m[whereVerb - 1].word->second.timeFlags & 31);
		if (timeFlag == T_START || timeFlag == T_STOP || timeFlag == T_FINISH || timeFlag == T_RESUME)
			return ((i = m[whereVerb - 1].word->second.index) < 0) ? NULLWORD : i;
	}
	return i;
}

const wchar_t *cSource::getWSAdverb(int whereVerb, bool changeStateAdverb)
{
	LFS
		if (whereVerb > 0 && m[whereVerb - 1].queryWinnerForm(adverbForm) >= 0)
			return m[whereVerb - 1].word->first.c_str();
	if (whereVerb >= 0 && whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0)
		return m[whereVerb + 1].word->first.c_str();
	if (whereVerb > 0 && changeStateAdverb)
		return m[whereVerb - 1].word->first.c_str();
	return L"";
}

bool cSource::acceptableObjectPosition(int where)
{
	LFS
		return m[where].objectMatches.size() > 0 ||
		(m[where].getObject() >= 0 &&
		(m[where].endObjectPosition - m[where].beginObjectPosition > 1 || objects[m[where].getObject()].objectClass != NON_GENDERED_GENERAL_OBJECT_CLASS || objects[m[where].getObject()].numEncounters > 1));
}

bool cSource::acceptableAdjective(int where)
{
	LFS
		return m[where].queryWinnerForm(adjectiveForm) >= 0 || m[where].queryWinnerForm(nounForm) >= 0 || m[where].pma.queryPattern(L"__ADJECTIVE") != -1 ||
		m[where].queryWinnerForm(demonstrativeDeterminerForm) >= 0 || m[where].queryWinnerForm(possessiveDeterminerForm) >= 0 || m[where].queryWinnerForm(quantifierForm) >= 0;
}

// return the first adjective that is an object
int cSource::getOSAdjective(int where)
{
	LFS
		if (where < 0) return -1;
	int object = m[where].getObject();
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1 && where > 0)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
			if (m[I].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(I))
				return (m[I].getObject() >= 0) ? m[I].getObject() : m[I].objectMatches[0].object;
	}
	return -1;
}

wstring cSource::getWOSAdjective(int where, wstring &tmpstr)
{
	LFS
		if (where < 0) return L"";
	int object = m[where].getObject();
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1 && where > 0)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
			if (m[I].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(I))
				return whereString(I, tmpstr, false);
	}
	return L"";
}

// return the first adjective that is not an object
int cSource::getMSAdjective(int where, int numOrder)
{
	LFS
		if (where < 0) return NULLWORD;
	int object = m[where].getObject(), index;
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
		{
			if (acceptableAdjective(I) && !acceptableObjectPosition(I))
			{
				if (numOrder == 0)
					return ((index = m[I].word->second.index) < 0) ? NULLWORD : index;
				else
					numOrder--;
			}
		}
	}
	return NULLWORD;
}

void cSource::printSRI(wstring logPrefix, cSpaceRelation* sri, int s, int ws, int wo, int ps, bool overWrote, int matchSum, wstring matchInfo, int logDestination)
{
	LFS
		wstring tmpstr;
	prepPhraseToString(ps, tmpstr);
	printSRI(logPrefix, sri, s, ws, wo, tmpstr, overWrote, matchSum, matchInfo, logDestination);
}

void cSource::printSRI(wstring logPrefix, cSpaceRelation* sri, int s, int ws, int wo, wstring ps, bool overWrote, int matchSum, wstring matchInfo, int logDestination)
{
	LFS
		if (sri == NULL)
			return;
	wstring tmpstr, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6, tmpstr7, tmpstr8, tmpstr9, tmpstr10, tmpstr11, tmpstr12, tmpstr14;
	bool inQuestion = (sri->whereSubject >= 0 && (m[sri->whereSubject].flags&cWordMatch::flagInQuestion));
	inQuestion |= (sri->whereObject >= 0 && (m[sri->whereObject].flags&cWordMatch::flagInQuestion));
	//if (SRIDebugCounter==464)
	//{
	//	extern int logQuestionProfileTime,logSynonymDetail,logTableDetail,equivalenceLogDetail,logDetail;
	//	logQuestionProfileTime=logSynonymDetail=logTableDetail=equivalenceLogDetail=logDetail=1;
	//}
	if (matchSum >= 0)
	{
		itos(SRIDebugCounter++, tmpstr12);
		itos(matchSum, tmpstr14);
		if (logDestination&LOG_QCHECK)
			tmpstr14 = L"[" + tmpstr14 + L" " + matchInfo + L"]";
		else
			tmpstr14 = L"[" + tmpstr12 + L":" + tmpstr14 + L" " + matchInfo + L"]";
	}
	else if (s >= 0)
	{
		itos(s, tmpstr12);
		tmpstr14 = L"S" + tmpstr12;
	}
	const wchar_t *tmp1 = 0, *tmp2 = 0, *tmp3 = 0, *tmp4 = 0, *tmp5 = 0, *tmp6 = 0;
	bool shortFormat = (logDestination&LOG_QCHECK) != 0;
	wchar_t *f1 = L"%s:%06d:%s%s%s %s %s%s %s %s %s %s %d:%s%s [V %d:%s]%s%s%s %s %s %d:%s%s%s%s%s%s%s%s%s%s%s";
	lplog(logDestination, f1, 
		logPrefix.c_str(), // 1
		sri->where, // 2
		tmpstr14.c_str(), // 3
		(sri->whereQuestionType < 0 && sri->relationType != stLOCATION && sri->relationType != -stLOCATION && sri->relationType != stNORELATION && inQuestion) ? L"***" : L"", //4
		(overWrote) ? L" OVERWRITE" : L"", // 5
		relationString(sri->relationType).c_str(),  // 6
		(sri->whereQuestionType >= 0) ? m[sri->whereQuestionType].word->first.c_str() : L"", // 7
		wrti(sri->whereControllingEntity, L"controller", tmpstr, shortFormat),  // 8
		(sri->whereControllingEntity < 0) ? L"" : wchr(m[sri->whereControllingEntity].getRelVerb()), // 9
		getWOSAdjective(ws, tmpstr2).c_str(), // 10
		getWSAdjective(ws, 0).c_str(), // 11
		getWSAdjective(ws, 1).c_str(), // 12
		ws, // 13
		wrti(ws, L"S", tmpstr3, shortFormat), // 14
		(sri->tft.negation) ? L"[NOT]" : L"", // 15
		sri->whereVerb, // 16
		wchr(sri->whereVerb), // 17
		tmp1 = getWSAdverb(sri->whereVerb, sri->changeStateAdverb), // 18
		tmp2 = getWOSAdjective(sri->whereVerb, wo, tmpstr4).c_str(), // 19
		tmp3 = getWSAdjective(sri->whereVerb, wo, 0, tmpstr5).c_str(), // 20
		tmp4 = getWSAdjective(sri->whereVerb, wo, 1, tmpstr6).c_str(), // 21
		tmp5 = getWSAdjective(sri->whereVerb, wo, 2, tmpstr7).c_str(), // 22
		wo, // 23
		tmp6 = wrti(wo, L"O", tmpstr8, shortFormat), // 24
		wchr(sri->whereSecondaryVerb), // 25
		getWOSAdjective(sri->whereSecondaryObject, tmpstr9).c_str(), // 26
		getWSAdjective(sri->whereSecondaryObject, 0).c_str(), // 27
		getWSAdjective(sri->whereSecondaryObject, 1).c_str(), // 28
		wrti(sri->whereSecondaryObject, L"O2", tmpstr10, shortFormat), // 29
		wrti(sri->whereNextSecondaryObject, L"nextObject2", tmpstr11, shortFormat), // 30
		(sri->objectSubType >= 0) ? OCSubTypeStrings[sri->objectSubType] : L"", // 31
		(sri->whereObject < 0) ? L"" : wrti(m[sri->whereObject].relNextObject, L"nextObject", tmpstr12, shortFormat), // 32
		ps.c_str(), //33
		(inQuestion) ? L"?" : L"."); // 34
}

cSpaceRelation::cSpaceRelation(int _where, int _o, int _whereControllingEntity, int _whereSubject, int _whereVerb, int _wherePrep, int _whereObject,
	int _wherePrepObject, int _movingRelativeTo, int _relationType,
	bool _genderedEntityMove, bool _genderedLocationRelation, int _objectSubType, int _prepObjectSubType, bool _physicalRelation)
{
	where = _where;
	o = _o;
	whereControllingEntity = _whereControllingEntity;
	whereSubject = _whereSubject;
	whereVerb = _whereVerb;
	wherePrep = _wherePrep;
	whereObject = _whereObject;
	wherePrepObject = _wherePrepObject;
	whereMovingRelativeTo = _movingRelativeTo;
	relationType = _relationType;
	genderedEntityMove = _genderedEntityMove;
	genderedLocationRelation = _genderedLocationRelation;
	objectSubType = _objectSubType;
	prepObjectSubType = _prepObjectSubType;
	physicalRelation = _physicalRelation;
	establishingLocation = false;
	futureLocation = false;
	tft.speakerCommand = false;
	tft.speakerQuestionToAudience = false;
	tft.significantRelation = false;
	tft.beyondLocalSpace = false; // movement beyond a room or a local area
	tft.story = false; // also includes future stories
	tft.timeTransition = false;
	tft.nonPresentTimeTransition = false;
	tft.duplicateTimeTransitionFromWhere = -1;
	tft.beforePastHappening = false;
	tft.pastHappening = false;
	tft.presentHappening = false;
	tft.futureHappening = false;
	tft.futureInPastHappening = false;
	tft.negation = false;
	agentLocationRelationSet = false;
	timeInfoSet = false;
	isConstructedRelative = false;
	prepositionUncertain = false;
	tft.lastOpeningPrimaryQuote = -1;
	nextSPR = -1;
	whereSecondaryVerb = -1;
	whereSecondaryObject = -1;
	whereNextSecondaryObject = -1;
	whereSecondaryPrep = -1;
	whereQuestionType = -1;
	questionType = 0;
	sentenceNum = -1;
	subQuery = false;
	skip = false;
	changeStateAdverb = false;
	nonSemanticObjectTotalMatch = false;
	nonSemanticPrepositionObjectTotalMatch = false;
	nonSemanticSecondaryObjectTotalMatch = false;
	nonSemanticSubjectTotalMatch = false;
	printMax = -1;
	printMin = -1;
	speakerContinuation = false;
	timeProgression = -1;
	whereQuestionTypeObject = -1;
	transformedPrep = -1;
}

bool operator != (const cSpaceRelation &lhs, const cSpaceRelation &rhs)
{
	return lhs.where != rhs.where ||
		lhs.o != rhs.o ||
		lhs.whereControllingEntity != rhs.whereControllingEntity ||
		lhs.whereSubject != rhs.whereSubject ||
		lhs.whereVerb != rhs.whereVerb ||
		lhs.wherePrep != rhs.wherePrep ||
		lhs.whereObject != rhs.whereObject ||
		lhs.wherePrepObject != rhs.wherePrepObject ||
		lhs.whereMovingRelativeTo != rhs.whereMovingRelativeTo ||
		lhs.relationType != rhs.relationType;
}

bool operator == (const cSpaceRelation &lhs, const cSpaceRelation &rhs)
{
	return lhs.where == rhs.where &&
		lhs.o == rhs.o &&
		lhs.whereControllingEntity == rhs.whereControllingEntity &&
		lhs.whereSubject == rhs.whereSubject &&
		lhs.whereVerb == rhs.whereVerb &&
		lhs.wherePrep == rhs.wherePrep &&
		lhs.whereObject == rhs.whereObject &&
		lhs.wherePrepObject == rhs.wherePrepObject &&
		lhs.whereMovingRelativeTo == rhs.whereMovingRelativeTo &&
		lhs.relationType == rhs.relationType;
}

bool cSpaceRelation::canUpdate(cSpaceRelation &z)
{
	return where == z.where &&
		(o == z.o || (z.o < 0 && o>0)) &&
		whereControllingEntity == z.whereControllingEntity &&
		whereSubject == z.whereSubject &&
		whereVerb == z.whereVerb &&
		(wherePrep == z.wherePrep || (z.wherePrep < 0 && wherePrep>0)) &&
		whereObject == z.whereObject &&
		(wherePrepObject == z.wherePrepObject || (z.wherePrepObject < 0 && wherePrepObject>0)) &&
		whereMovingRelativeTo == z.whereMovingRelativeTo &&
		relationType == z.relationType;
}

cSpaceRelation::cSpaceRelation(char *buffer, int &w, unsigned int total, bool &error)
{
	if (error = !copy(where, buffer, w, total)) return;
	if (error = !copy(o, buffer, w, total)) return;
	if (error = !copy(whereControllingEntity, buffer, w, total)) return;
	if (error = !copy(whereSubject, buffer, w, total)) return;
	if (error = !copy(whereVerb, buffer, w, total)) return;
	if (error = !copy(wherePrep, buffer, w, total)) return;
	if (error = !copy(whereObject, buffer, w, total)) return;
	if (error = !copy(wherePrepObject, buffer, w, total)) return;
	if (error = !copy(whereSecondaryVerb, buffer, w, total)) return;
	if (error = !copy(whereSecondaryObject, buffer, w, total)) return;
	if (error = !copy(whereSecondaryPrep, buffer, w, total)) return;
	if (error = !copy(whereNextSecondaryObject, buffer, w, total)) return;
	if (error = !copy(whereMovingRelativeTo, buffer, w, total)) return;
	if (error = !copy(relationType, buffer, w, total)) return;
	if (error = !copy(objectSubType, buffer, w, total)) return;
	if (error = !copy(prepObjectSubType, buffer, w, total)) return;
	if (error = !copy(timeProgression, buffer, w, total)) return;
	if (error = !copy(questionType, buffer, w, total)) return;
	if (error = !copy(whereQuestionType, buffer, w, total)) return;
	if (error = !copy(sentenceNum, buffer, w, total)) return;
	// flags
	__int64 flags;
	if (error = !copy(flags, buffer, w, total)) return;
	convertToFlags(flags);
	if (error = !copy(tft.lastOpeningPrimaryQuote, buffer, w, total)) return;
	if (error = !copy(tft.duplicateTimeTransitionFromWhere, buffer, w, total)) return;
	if (error = !copy(tft.presType, buffer, w, total)) return;
	if (error = !copy(description, buffer, w, total)) return;
	if (error = !copy(nextSPR, buffer, w, total)) return;
	int count;
	if (error = !copy(count, buffer, w, total)) return;
	subQuery = false;
	timeInfo.reserve(count);
	while (count-- && !error && w < (signed)total)
		timeInfo.emplace_back(buffer, w, total, error);
	skip = false;
	changeStateAdverb = false;
	nonSemanticObjectTotalMatch = false;
	nonSemanticPrepositionObjectTotalMatch = false;
	nonSemanticSecondaryObjectTotalMatch = false;
	nonSemanticSubjectTotalMatch = false;
	transformedPrep = -1;
	printMax = -1;
	printMin = -1;
	speakerContinuation = false;
	whereQuestionTypeObject = -1;
}

int cSpaceRelation::sanityCheck(int maxSourcePosition, int maxObjectIndex)
{
	if (where < 0 || where >= maxSourcePosition) return 400;
	if (o < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || o >= maxObjectIndex) return 401;
	if (whereControllingEntity < -1 || whereControllingEntity >= maxSourcePosition) return 402;
	if (whereSubject < -1 || whereSubject >= maxSourcePosition) return 403;
	if (whereVerb < -1 || whereVerb >= maxSourcePosition) return 404;
	if (wherePrep < -1 || wherePrep >= maxSourcePosition) return 405;
	if (whereObject < -1 || whereObject >= maxSourcePosition) return 406;
	if (wherePrepObject < -1 || wherePrepObject >= maxSourcePosition) return 407;
	if (whereSecondaryVerb < -1 || whereSecondaryVerb >= maxSourcePosition) return 408;
	if (whereSecondaryObject < -1 || whereSecondaryObject >= maxSourcePosition) return 409;
	if (whereSecondaryPrep < -1 || whereSecondaryPrep >= maxSourcePosition) return 410;
	if (whereNextSecondaryObject < -1 || whereNextSecondaryObject >= maxSourcePosition) return 411;
	if (whereMovingRelativeTo < -1 || whereMovingRelativeTo >= maxSourcePosition) return 412;
	if (relationType >= stLAST) return 413;
	//if (objectSubType >= OCSubType::NUM_SUBTYPES) return false;
	//if (prepObjectSubType >= OCSubType::NUM_SUBTYPES) return false;
	if (whereQuestionType < -1 || whereQuestionType >= maxSourcePosition) return 414;
	return 0;
}

void cSpaceRelation::convertToFlags(__int64 flags)
{
	/*bool inSecondaryQuote=flags&1; */flags >>= 1;
	/*bool inPrimaryQuote=flags&1;*/ flags >>= 1;
	/*bool isQuestion=flags&1;*/ flags >>= 1;
	changeStateAdverb = flags & 1; flags >>= 1;
	skip = flags & 1; flags >>= 1;
	physicalRelation = flags & 1; flags >>= 1;
	timeInfoSet = flags & 1; flags >>= 1;
	agentLocationRelationSet = flags & 1; flags >>= 1;
	tft.negation = flags & 1; flags >>= 1;
	tft.futureInPastHappening = flags & 1; flags >>= 1;
	tft.futureHappening = flags & 1; flags >>= 1;
	tft.presentHappening = flags & 1; flags >>= 1;
	tft.pastHappening = flags & 1; flags >>= 1;
	tft.beforePastHappening = flags & 1; flags >>= 1;
	tft.nonPresentTimeTransition = flags & 1; flags >>= 1;
	tft.timeTransition = flags & 1; flags >>= 1;
	tft.story = flags & 1; flags >>= 1;
	tft.beyondLocalSpace = flags & 1; flags >>= 1;
	tft.significantRelation = flags & 1; flags >>= 1;
	tft.speakerQuestionToAudience = flags & 1; flags >>= 1;
	tft.speakerCommand = flags & 1; flags >>= 1;
	speakerContinuation = flags & 1; flags >>= 1;
	futureLocation = flags & 1; flags >>= 1;
	establishingLocation = flags & 1; flags >>= 1;
	genderedLocationRelation = flags & 1; flags >>= 1;
	genderedEntityMove = flags & 1;
	// 6 questionFlags bits (see convertFlags)
}

__int64 cSpaceRelation::convertFlags(bool isQuestion, bool inPrimaryQuote, bool inSecondaryQuote, __int64 questionFlags)
{
	__int64 flags = (questionFlags << 6);
	flags |= (genderedEntityMove) ? 1 : 0; flags <<= 1;
	flags |= (genderedLocationRelation) ? 1 : 0; flags <<= 1;
	flags |= (establishingLocation) ? 1 : 0; flags <<= 1;
	flags |= (futureLocation) ? 1 : 0; flags <<= 1;
	flags |= (speakerContinuation) ? 1 : 0; flags <<= 1;
	flags |= (tft.speakerCommand) ? 1 : 0; flags <<= 1;
	flags |= (tft.speakerQuestionToAudience) ? 1 : 0; flags <<= 1;
	flags |= (tft.significantRelation) ? 1 : 0; flags <<= 1;
	flags |= (tft.beyondLocalSpace) ? 1 : 0; flags <<= 1;
	flags |= (tft.story) ? 1 : 0; flags <<= 1;
	flags |= (tft.timeTransition) ? 1 : 0; flags <<= 1;
	flags |= (tft.nonPresentTimeTransition) ? 1 : 0; flags <<= 1;
	flags |= (tft.beforePastHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.pastHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.presentHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.futureHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.futureInPastHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.negation) ? 1 : 0; flags <<= 1;
	flags |= (agentLocationRelationSet) ? 1 : 0; flags <<= 1;
	flags |= (timeInfoSet) ? 1 : 0; flags <<= 1;
	flags |= (physicalRelation) ? 1 : 0; flags <<= 1;
	flags |= (skip) ? 1 : 0; flags <<= 1;
	flags |= (changeStateAdverb) ? 1 : 0; flags <<= 1;
	flags |= (isQuestion) ? 1 : 0; flags <<= 1;
	flags |= (inPrimaryQuote) ? 1 : 0; flags <<= 1;
	flags |= (inSecondaryQuote) ? 1 : 0;
	return flags;
}

bool cSpaceRelation::write(void *buffer, int &w, int limit)
{
	if (!copy(buffer, where, w, limit)) return false;
	if (!copy(buffer, o, w, limit)) return false;
	if (!copy(buffer, whereControllingEntity, w, limit)) return false;
	if (!copy(buffer, whereSubject, w, limit)) return false;
	if (!copy(buffer, whereVerb, w, limit)) return false;
	if (!copy(buffer, wherePrep, w, limit)) return false;
	if (!copy(buffer, whereObject, w, limit)) return false;
	if (!copy(buffer, wherePrepObject, w, limit)) return false;
	if (!copy(buffer, whereSecondaryVerb, w, limit)) return false;
	if (!copy(buffer, whereSecondaryObject, w, limit)) return false;
	if (!copy(buffer, whereSecondaryPrep, w, limit)) return false;
	if (!copy(buffer, whereNextSecondaryObject, w, limit)) return false;
	if (!copy(buffer, whereMovingRelativeTo, w, limit)) return false;
	if (!copy(buffer, relationType, w, limit)) return false;
	if (!copy(buffer, objectSubType, w, limit)) return false;
	if (!copy(buffer, prepObjectSubType, w, limit)) return false;
	if (!copy(buffer, timeProgression, w, limit)) return false;
	if (!copy(buffer, questionType, w, limit)) return false;
	if (!copy(buffer, whereQuestionType, w, limit)) return false;
	if (!copy(buffer, sentenceNum, w, limit)) return false;
	// flags
	__int64 flags = convertFlags(false, false, false, 0); // while on disk, questions can be queried by the flag cWordMatch::inQuestion
	if (!copy(buffer, flags, w, limit)) return false;
	if (!copy(buffer, tft.lastOpeningPrimaryQuote, w, limit)) return false;
	if (!copy(buffer, tft.duplicateTimeTransitionFromWhere, w, limit)) return false;
	if (!copy(buffer, tft.presType, w, limit)) return false;
	if (!copy(buffer, description, w, limit)) return false;
	if (!copy(buffer, nextSPR, w, limit)) return false;
	unsigned int count = timeInfo.size();
	if (!copy((void *)buffer, count, w, limit)) return false;
	for (unsigned int I = 0; I < count; I++)
		if (!timeInfo[I].write(buffer, w, limit)) return false;
	return true;
}

bool cSpaceRelation::adjustValue(int& val, int originalVal, wstring valString, unordered_map <int, int>& sourceIndexMap)
{
	if (val < 0)
		return false;
	if (sourceIndexMap.find(originalVal) != sourceIndexMap.end())
	{
		val = sourceIndexMap[originalVal];
		lplog(LOG_WHERE, L"Translated %s from %d to %d.", valString.c_str(), originalVal,val);
		return true;
	}
	else
	{
		lplog(LOG_WHERE, L"Unable to translate %s of %d.", valString.c_str(), originalVal);
		val = originalVal;
	}
	return false;
}

cSpaceRelation::cSpaceRelation(vector <cSpaceRelation>::iterator sri, unordered_map <int, int> &sourceIndexMap)
{
	o = -1;
	adjustValue(where, sri->where, L"where", sourceIndexMap);
	adjustValue(whereControllingEntity, sri->whereControllingEntity, L"whereControllingEntity", sourceIndexMap);
	adjustValue(whereSubject, sri->whereSubject, L"whereSubject", sourceIndexMap);
	adjustValue(whereVerb, sri->whereVerb, L"whereVerb", sourceIndexMap);
	adjustValue(wherePrep, sri->wherePrep, L"wherePrep", sourceIndexMap);
	adjustValue(whereObject, sri->whereObject, L"whereObject", sourceIndexMap);
	adjustValue(wherePrepObject, sri->wherePrepObject, L"wherePrepObject", sourceIndexMap);
	adjustValue(whereMovingRelativeTo, sri->whereMovingRelativeTo, L"whereMovingRelativeTo", sourceIndexMap);
	adjustValue(whereSecondaryVerb, sri->whereSecondaryVerb, L"whereSecondaryVerb", sourceIndexMap);
	adjustValue(whereSecondaryObject, sri->whereSecondaryObject, L"whereSecondaryObject", sourceIndexMap);
	adjustValue(whereNextSecondaryObject, sri->whereNextSecondaryObject, L"whereNextSecondaryObject", sourceIndexMap);
	adjustValue(whereSecondaryPrep, sri->whereSecondaryPrep, L"whereSecondaryPrep", sourceIndexMap);
	adjustValue(whereQuestionType, sri->whereQuestionType, L"whereQuestionType", sourceIndexMap);
	adjustValue(whereQuestionTypeObject, sri->whereQuestionTypeObject, L"whereQuestionTypeObject", sourceIndexMap);
	for (int wo : sri->whereQuestionInformationSourceObjects)
	{
		int whereQuestionInformationSourceObject;
		if (adjustValue(whereQuestionInformationSourceObject, wo, L"whereQuestionInformationSourceObject", sourceIndexMap))
			whereQuestionInformationSourceObjects.insert(sourceIndexMap[wo]);
	}
	adjustValue(transformedPrep, sri->transformedPrep, L"transformedPrep", sourceIndexMap);

	relationType = sri->relationType;
	genderedEntityMove = sri->genderedEntityMove;
	genderedLocationRelation = sri->genderedLocationRelation;
	objectSubType = sri->objectSubType;
	prepObjectSubType = sri->prepObjectSubType;
	physicalRelation = sri->physicalRelation;
	prepositionUncertain = sri->prepositionUncertain;
	tft.speakerCommand = sri->tft.speakerCommand;
	tft.speakerQuestionToAudience = sri->tft.speakerQuestionToAudience;
	tft.significantRelation = sri->tft.significantRelation;
	tft.beyondLocalSpace = sri->tft.beyondLocalSpace;
	tft.story = sri->tft.story;
	tft.timeTransition = sri->tft.timeTransition;
	tft.nonPresentTimeTransition = sri->tft.nonPresentTimeTransition;
	tft.duplicateTimeTransitionFromWhere = sri->tft.duplicateTimeTransitionFromWhere;
	tft.beforePastHappening = sri->tft.beforePastHappening;
	tft.pastHappening = sri->tft.pastHappening;
	tft.presentHappening = sri->tft.presentHappening;
	tft.futureHappening = sri->tft.futureHappening;
	tft.futureInPastHappening = sri->tft.futureInPastHappening;
	tft.negation = sri->tft.negation;
	tft.lastOpeningPrimaryQuote = sri->tft.lastOpeningPrimaryQuote;
	establishingLocation = sri->establishingLocation;
	futureLocation = sri->futureLocation;
	agentLocationRelationSet = sri->agentLocationRelationSet;
	timeInfoSet = sri->timeInfoSet;
	isConstructedRelative = sri->isConstructedRelative;
	nextSPR = sri->nextSPR;
	questionType = sri->questionType;
	sentenceNum = sri->sentenceNum;
	subQuery = sri->subQuery;
	skip = sri->skip;
	timeProgression = sri->timeProgression;
}
