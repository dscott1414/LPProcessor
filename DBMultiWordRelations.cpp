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
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "vcXML.h"
#include "profile.h"
#include "mysqldb.h"

#define NULLWORD 187
bool checkFull(MYSQL *mysql,wchar_t *qt,size_t &len,bool flush,wchar_t *qualifier);

int Source::rti(int where)
{ LFS
	if (where<0) return NULLWORD;
	tIWMM w=fullyResolveToClass(where);
	if (w==wNULL || w->second.index<0) return NULLWORD;
	return w->second.index;
}

const wchar_t *Source::wrti(int where,wchar_t *id,wstring &tmpstr,bool shortFormat)
{ LFS
	if (where<0) return L"";
	wstring ws;
	whereString(where,ws,shortFormat);
	tIWMM w=fullyResolveToClass(where);
	wstring lcws=ws,lcw=w->first;
	std::transform(lcws.begin(), lcws.end(), lcws.begin(), ::tolower);
	std::transform(lcw.begin(), lcw.end(), lcw.begin(), ::tolower);
	if (w==wNULL || w->second.index<0 || lcws.find(lcw)!=wstring::npos) 
		tmpstr=L"["+wstring(id)+L" "+ws+L"]";
	else
		tmpstr=L"["+wstring(id)+L" ("+ws+L") "+w->first+L"]";
	return tmpstr.c_str();
}

const wchar_t *Source::wchr(int where)
{ LFS
	return (where<0) ? L"":m[where].word->first.c_str();
}

bool Source::acceptableObjectPosition(int where)
{ LFS
	return m[where].objectMatches.size()>0 || 
		     (m[where].getObject()>=0 && 
				  (m[where].endObjectPosition-m[where].beginObjectPosition>1 || objects[m[where].getObject()].objectClass!=NON_GENDERED_GENERAL_OBJECT_CLASS || objects[m[where].getObject()].numEncounters>1));
}

// return the first adjective that is an object
int Source::getOSAdjective(int where)
{ LFS
	if (where<0) return -1;
	int object=m[where].getObject();
	if (object>=0 && m[where].endObjectPosition-m[where].beginObjectPosition>1 && where>0)
	{
		for (int I=m[where].beginObjectPosition; I<m[where].endObjectPosition-1; I++)
			if (m[I].principalWhereAdjectivalPosition>=0 && acceptableObjectPosition(I))
				return (m[I].getObject()>=0) ? m[I].getObject() : m[I].objectMatches[0].object;
	}
	return -1;
}

wstring Source::getWOSAdjective(int where,wstring &tmpstr)
{ LFS
	if (where<0) return L"";
	int object=m[where].getObject();
	if (object>=0 && m[where].endObjectPosition-m[where].beginObjectPosition>1 && where>0)
	{
		for (int I=m[where].beginObjectPosition; I<m[where].endObjectPosition-1; I++)
			if (m[I].principalWhereAdjectivalPosition>=0 && acceptableObjectPosition(I))
				return whereString(I,tmpstr,false);
	}
	return L"";
}

bool Source::acceptableAdjective(int where)
{ LFS
  return m[where].queryWinnerForm(adjectiveForm)>=0 || m[where].queryWinnerForm(nounForm)>=0 || m[where].pma.queryPattern(L"__ADJECTIVE")!=-1 || 
		     m[where].queryWinnerForm(demonstrativeDeterminerForm)>=0 || m[where].queryWinnerForm(possessiveDeterminerForm)>=0 || m[where].queryWinnerForm(quantifierForm)>=0;
}

// return the first adjective that is not an object
int Source::getMSAdjective(int where,int numOrder)
{ LFS
	if (where<0) return NULLWORD;
	int object=m[where].getObject(),index;
	if (object>=0 && m[where].endObjectPosition-m[where].beginObjectPosition>1)
	{
		for (int I=m[where].beginObjectPosition; I<m[where].endObjectPosition-1; I++)
		{
			if (acceptableAdjective(I) && !acceptableObjectPosition(I))
			{
				if (numOrder==0)
					return ((index=m[I].word->second.index)<0) ? NULLWORD : index;
				else
					numOrder--;
			}
		}
	}
	return NULLWORD;
}

wstring Source::getWSAdjective(int where,int numOrder)
{ LFS
	if (where<0) return L"";
	int object=m[where].getObject();
	if (object>=0 && m[where].endObjectPosition-m[where].beginObjectPosition>1 && where>0)
	{
		for (int I=m[where].beginObjectPosition; I<m[where].endObjectPosition-1; I++)
			if (acceptableAdjective(I) && !acceptableObjectPosition(I))
			{
				if (numOrder==0)
					return m[I].word->first;
				else
					numOrder--;
			}
	}
	return L"";
}

int Source::getOSAdjective(int whereVerb,int where)
{ LFS
	if (where>=0)
		return getOSAdjective(where);
	if (whereVerb<0) return -1;
	if (whereVerb+1<(signed)m.size() && m[whereVerb+1].principalWhereAdjectivalPosition>=0 && acceptableObjectPosition(whereVerb+1))
		return (m[whereVerb+1].getObject()>=0) ? m[whereVerb+1].getObject() : m[whereVerb+1].objectMatches[0].object;
	if (whereVerb+2<(signed)m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0 && m[whereVerb+2].principalWhereAdjectivalPosition>=0 && acceptableObjectPosition(whereVerb+2))
		return (m[whereVerb+2].getObject()>=0) ? m[whereVerb+2].getObject() : m[whereVerb+2].objectMatches[0].object;
	return -1;
}

wstring Source::getWOSAdjective(int whereVerb,int where,wstring &tmpstr)
{ LFS
	getWOSAdjective(where,tmpstr);
	if (tmpstr.empty() && where<0 && whereVerb>=0)
	{
		if (whereVerb+1<(signed)m.size() && m[whereVerb+1].principalWhereAdjectivalPosition>=0 && acceptableObjectPosition(whereVerb+1))
			return whereString(whereVerb+1,tmpstr,false);
		if (whereVerb+2<(signed)m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0 && m[whereVerb+2].principalWhereAdjectivalPosition>=0 && acceptableObjectPosition(whereVerb+2))
			return whereString(whereVerb+2,tmpstr,false);
	}
	return tmpstr;
}

int Source::getMSAdjective(int whereVerb,int where,int numOrder)
{ LFS
	if (where>=0)
		return getMSAdjective(where,numOrder);
	if (numOrder!=0) return NULLWORD;
	int i;
	// How old is Darrell Hammond?
	//if (where<0 && whereVerb>=0 && (m[whereVerb].flags&WordMatch::flagInQuestion) && m[whereVerb].relSubject<0 &&
	//	  m[whereVerb-1].queryWinnerForm(adjectiveForm)>=0 && m[whereVerb-1].getObject()<0)
	//	return ((i=m[whereVerb-1].word->second.index)<0) ? NULLWORD : i;
	if (whereVerb<0 || (whereVerb+1>=where && where>=0)) return NULLWORD;
	int maxEnd,q2IElement=queryPattern(whereVerb,L"_Q2",maxEnd);
	if (q2IElement>=0 && patterns[pema[q2IElement].getPattern()]->differentiator==L"I" && m[whereVerb].relSubject>=0 && (i=m[m[whereVerb].relSubject].endObjectPosition)>=0 &&
		(m[i].queryWinnerForm(adjectiveForm)>=0 || (m[i].queryWinnerForm(quoteForm)>=0 && i+1<(signed)m.size() && m[i=i+1].queryWinnerForm(adjectiveForm)>=0))) // What is "WWE" short for?
		return ((i=m[i].word->second.index)<0) ? NULLWORD : i;
	if (whereVerb+1<(signed)m.size() && acceptableAdjective(whereVerb+1) && !acceptableObjectPosition(whereVerb+1))
		return ((i=m[whereVerb+1].word->second.index)<0) ? NULLWORD : i;
	if (whereVerb+2<(signed)m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0 && acceptableAdjective(whereVerb+2) && !acceptableObjectPosition(whereVerb+2))
		return ((i=m[whereVerb+2].word->second.index)<0) ? NULLWORD : i;
	return NULLWORD;
}

wstring Source::getWSAdjective(int whereVerb,int where,int numOrder,wstring &tmpstr)
{ LFS
	tmpstr=getWSAdjective(where,numOrder);
	if (numOrder!=0)
		return tmpstr;
	if (tmpstr.empty() && where<0 && whereVerb>=0)
	{
		int maxEnd,i,q2IElement=queryPattern(whereVerb,L"_Q2",maxEnd);
		if (q2IElement>=0 && patterns[pema[q2IElement].getPattern()]->differentiator==L"I" && m[whereVerb].relSubject>=0 && (i=m[m[whereVerb].relSubject].endObjectPosition)>=0 &&
			(m[i].queryWinnerForm(adjectiveForm)>=0 || (m[i].queryWinnerForm(quoteForm)>=0 && i+1<(signed)m.size() && m[i=i+1].queryWinnerForm(adjectiveForm)>=0))) // What is "WWE" short for?
			return m[i].word->first;
		if (whereVerb+1<(signed)m.size() && acceptableAdjective(whereVerb+1) && !acceptableObjectPosition(whereVerb+1))
			return m[whereVerb+1].word->first;
		if (whereVerb+2<(signed)m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0 && acceptableAdjective(whereVerb+2) && !acceptableObjectPosition(whereVerb+2))
			return m[whereVerb+2].word->first;
	}
	return tmpstr;
}

int Source::getMSAdverb(int whereVerb,bool changeStateAdverb)
{ LFS
	int i=-1;
	if (whereVerb>=0 && whereVerb+1<(signed)m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0)
		return ((i=m[whereVerb+1].word->second.index)<0) ? NULLWORD : i;
	if (whereVerb>0 && m[whereVerb-1].queryWinnerForm(adverbForm)>=0)
		return ((i=m[whereVerb-1].word->second.index)<0) ? NULLWORD : i;
	if (whereVerb>0 && changeStateAdverb)
	{ 
		int timeFlag=(m[whereVerb-1].word->second.timeFlags&31);
	  if (timeFlag==T_START || timeFlag==T_STOP || timeFlag==T_FINISH || timeFlag==T_RESUME)
			return ((i=m[whereVerb-1].word->second.index)<0) ? NULLWORD : i;
	}
	return i;
}

const wchar_t *Source::getWSAdverb(int whereVerb,bool changeStateAdverb)
{ LFS
	if (whereVerb>0 && m[whereVerb-1].queryWinnerForm(adverbForm)>=0)
		return m[whereVerb-1].word->first.c_str();
	if (whereVerb>=0 && whereVerb+1<(signed)m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0)
		return m[whereVerb+1].word->first.c_str();
	if (whereVerb>0 && changeStateAdverb)
		return m[whereVerb-1].word->first.c_str();
	return L"";
}

int Source::getProfession(int object)
{ LFS
	if (object<0) return NULLWORD;
	int where=m[object].principalWherePosition;
	if (where<0) return NULLWORD;
	if (objects[object].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
		return m[where].word->second.index;
	if (m[where].objectMatches.size()==1 && objects[object=m[where].objectMatches[0].object].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && 
		objects[object].originalLocation>=0)
		return m[objects[object].originalLocation].word->second.index;
	return NULLWORD;
}

int Source::getVerbIndex(int whereVerb)
{ LFS
	if (whereVerb<0) return NULLWORD;
	tIWMM v=m[whereVerb].getVerbME(whereVerb,1,lastNounNotFound,lastVerbNotFound);
	return (v!=wNULL && v->second.index>=0) ? v->second.index : NULLWORD;
}

int Source::getMatchedObject(int where,set <int> &mwObjects)
{ LFS
	if (where<0) return -1;
	int object=m[where].getObject();
	if (m[where].objectMatches.size()>0)
		object=m[where].objectMatches[0].object;
	if (object>=0) 
		mwObjects.insert(object);
	return object;
}

int Source::gmo(int wo)
{ LFS
	if (wo<0) return wo;
	return (m[wo].endObjectPosition>wo) ? m[wo].endObjectPosition : wo;
}

void Source::getSRIMinMax(cSpaceRelation* sri)
{ LFS
	sri->printMin=10000;
	sri->printMax=-1;
	if (sri->whereControllingEntity>=0) 
	{
		sri->printMin=min(sri->printMin,sri->whereControllingEntity);
		if (m[sri->whereControllingEntity].getObject()>=0)
			sri->printMin=min(sri->printMin,m[sri->whereControllingEntity].beginObjectPosition);
	}
	if (sri->whereSubject>=0)
	{
			sri->printMin=min(sri->printMin,sri->whereSubject);
			if (m[sri->whereSubject].getObject()>=0)
				sri->printMin=min(sri->printMin,m[sri->whereSubject].beginObjectPosition);
	}
	if (sri->whereVerb>=0) sri->printMin=min(sri->printMin,sri->whereVerb);
	if (sri->whereObject>=0) sri->printMin=min(sri->printMin,sri->whereObject);

	sri->printMax=max(sri->printMax,gmo(sri->whereControllingEntity));
	sri->printMax=max(sri->printMax,gmo(sri->whereSubject));
	sri->printMax=max(sri->printMax,sri->whereVerb);
	sri->printMax=max(sri->printMax,gmo(sri->whereObject));
	if (sri->whereObject>=0) sri->printMax=max(sri->printMax,gmo(m[sri->whereObject].nextCompoundPartObject));
	if (sri->whereVerb>=0) sri->printMax=max(sri->printMax,m[sri->whereVerb].getRelVerb());
	if (sri->whereVerb>=0 && m[sri->whereVerb].getRelVerb()>=0) sri->printMax=max(sri->printMax,gmo(m[m[sri->whereVerb].getRelVerb()].getRelObject()));
	set <int> relPreps;
	for (int wp=sri->wherePrep; wp>=0 && wp<(int)m.size(); wp=m[wp].relPrep)
	{
		if (relPreps.find(wp)!=relPreps.end())
			break;
		relPreps.insert(wp);
	}
	for (int wp=sri->whereSecondaryPrep; wp>=0 && wp+1<(int)m.size(); wp=m[wp].relPrep)
	{
		if (relPreps.find(wp)!=relPreps.end())
			break;
		relPreps.insert(wp);
	}
	for (set <int>::iterator rp=relPreps.begin(),rpEnd=relPreps.end(); rp!=rpEnd; rp++)
	{
		sri->printMin=min(sri->printMin,*rp);
		if (m[*rp].getRelObject()>=0) sri->printMin=min(sri->printMin,m[*rp].getRelObject());
		sri->printMax=max(sri->printMax,*rp);
		sri->printMax=max(sri->printMax,gmo(m[*rp].getRelObject()));
		if (m[*rp].getRelObject()>=0) sri->printMax=max(sri->printMax,gmo(m[m[*rp].getRelObject()].nextCompoundPartObject));
	}
	if (sri->whereQuestionType>=0) sri->printMin=min(sri->printMin,sri->whereQuestionType);
}

void Source::testPFlag(int where,int wp,int &prepositionFlags,int flag,int &nearestLocation)
{ LFS
	if (wp==where+1)
		prepositionFlags=flag;
	if (where>=0 && where<wp && where>nearestLocation)
		nearestLocation=where;
}

void Source::insertPrepPhrase(vector <cSpaceRelation>::iterator sri,int where,int wherePrep,int whereSecondaryVerb,int whereSecondaryObject,set <int> &relPreps,wchar_t *pnqt,size_t &pnlen,set <int> &mwObjects,wstring &ps)
{ LFS
	int wherePrepObject=m[wherePrep].getRelObject();
	int po=(wherePrepObject>=0) ? m[wherePrepObject].getObject() : -1;
	int prepObjectSubType=(po>=0) ? objects[po].getSubType() : -1;
	int prepBindingFlags=0,nearestObject=-1,nearestVerb=-1;
	if (prepObjectSubType<0 && po>=0 && objects[po].isPossibleSubType(false)) prepObjectSubType=UNKNOWN_PLACE_SUBTYPE;
	// prepWhereFlag [3 bits]
	// preposition previous word is referencingObject [0],subject [1],object [2], 2nd object [3], secondary object [4], secondary 2nd object [5], verb [6], secondary verb [7], prep objects [8+], none [-1]
	testPFlag(sri->whereControllingEntity,wherePrep,prepBindingFlags,0,nearestObject);
	testPFlag(sri->whereSubject,wherePrep,prepBindingFlags,1,nearestObject);
	testPFlag(sri->whereObject,wherePrep,prepBindingFlags,2,nearestObject);
	if (sri->whereObject>=0)
		testPFlag(m[sri->whereObject].relNextObject,wherePrep,prepBindingFlags,3,nearestObject);
	testPFlag(whereSecondaryObject,wherePrep,prepBindingFlags,4,nearestObject);
	if (whereSecondaryObject>=0)
		testPFlag(m[whereSecondaryObject].relNextObject,wherePrep,prepBindingFlags,5,nearestObject);
	testPFlag(sri->whereVerb,wherePrep,prepBindingFlags,6,nearestVerb);
	testPFlag(whereSecondaryVerb,wherePrep,prepBindingFlags,7,nearestVerb);
	int bp=0;
	for (set <int>::iterator rp=relPreps.begin(),rpEnd=relPreps.end(); rp!=rpEnd; rp++,bp++)
	{
		testPFlag(m[*rp].getRelObject(),wherePrep,prepBindingFlags,8+bp,nearestObject);
	}
	if (wherePrep>=0)
		pnlen+=_snwprintf(pnqt+pnlen,QUERY_BUFFER_LEN-pnlen,L"(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d),",
			sourceId, // 1
			where, // 2
			getVerbIndex(nearestVerb), // 3
			rti(nearestObject), // 4
			rti(m[wherePrep].nextQuote), // 5
			rti(m[wherePrep].relNextObject), // 6
			rti(wherePrep), // 7
			getOSAdjective(wherePrepObject), // 8
			getMSAdjective(wherePrepObject,0), // 9
			getMSAdjective(wherePrepObject,1), // 10
			(wherePrepObject<0) ? NULLWORD:m[wherePrepObject].word->second.index, // 11
			getMatchedObject(wherePrepObject,mwObjects), // 12
			prepObjectSubType, // 13
			prepBindingFlags); // 14
	wstring tmpstr1,tmpstr2;
	ps+=wchr(wherePrep)+getWOSAdjective(wherePrepObject,tmpstr1)+L" "+getWSAdjective(wherePrepObject,0)+L" "+getWSAdjective(wherePrepObject,1)+L" " + wrti(wherePrepObject,L"PO",tmpstr2);
	// compound nouns
	int compoundCount=0;
	set <int> whereCompoundObjects;
	while (wherePrep>=0 && wherePrepObject>=0 && (wherePrepObject=m[wherePrepObject].nextCompoundPartObject)>=0 && compoundCount++<10)
	{
		if (whereCompoundObjects.find(wherePrepObject)!=whereCompoundObjects.end())
			break;
		whereCompoundObjects.insert(wherePrepObject);
		po=m[wherePrepObject].getObject();
		prepObjectSubType=(po>=0) ? objects[po].getSubType() : -1;
		if (prepObjectSubType<0 && po>=0 && objects[po].isPossibleSubType(false)) prepObjectSubType=UNKNOWN_PLACE_SUBTYPE;
		pnlen+=_snwprintf(pnqt+pnlen,QUERY_BUFFER_LEN-pnlen,L"(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d),",
			sourceId, // 1
			where, // 2
			getVerbIndex(nearestVerb), // 3
			rti(nearestObject), // 4
			rti(m[wherePrep].nextQuote), // 5
			rti(m[wherePrep].relNextObject), // 6
			rti(wherePrep), // 7
			getOSAdjective(wherePrepObject), // 8
			getMSAdjective(wherePrepObject,0), // 9
			getMSAdjective(wherePrepObject,1), // 10
			(wherePrepObject<0) ? NULLWORD:m[wherePrepObject].word->second.index, // 11
			getMatchedObject(wherePrepObject,mwObjects), // 12
			prepObjectSubType, // 13
			prepBindingFlags); // 14
		ps+=wchr(wherePrep)+getWOSAdjective(wherePrepObject,tmpstr1)+L" "+getWSAdjective(wherePrepObject,0)+L" "+getWSAdjective(wherePrepObject,1)+L" " + wrti(wherePrepObject,L"POC",tmpstr2);
	}
}

void Source::prepPhraseToString(int wherePrep,wstring &ps)
{ LFS
	if (wherePrep<0) return;
	int wherePrepObject=m[wherePrep].getRelObject();
	if (wherePrepObject<0) return;
	wstring tmpstr1,tmpstr2;
	ps+=wchr(wherePrep)+getWOSAdjective(wherePrepObject,tmpstr1)+L" "+getWSAdjective(wherePrepObject,0)+L" "+getWSAdjective(wherePrepObject,1)+L" " + wrti(wherePrepObject,L"PO",tmpstr2);
	// compound nouns
	int compoundCount=0;
	while (wherePrep>=0 && wherePrepObject>=0 && (wherePrepObject=m[wherePrepObject].nextCompoundPartObject)>=0 && compoundCount++<10)
		ps+=wchr(wherePrep)+getWOSAdjective(wherePrepObject,tmpstr1)+L" "+getWSAdjective(wherePrepObject,0)+L" "+getWSAdjective(wherePrepObject,1)+L" " + wrti(wherePrepObject,L"POC",tmpstr2);
}

bool Source::advanceSentenceNum(vector <cSpaceRelation>::iterator sri,unsigned int &s,int lastMin,int lastMax,bool inQuestion)
{ LFS
		
		if (sri->printMin==sri->printMax || sri->printMin<0)
			return true;
		if (sri->printMin>=lastMin && sri->printMax<=lastMax)
		{
				if (sri->whereVerb<0 || sri==spaceRelations.begin() || sri->whereVerb==sri[-1].whereVerb)
					return true;
				if (m[sri->whereVerb].getRelVerb()==sri[-1].whereVerb)
					return true;
		}
		while (s+1<sentenceStarts.size() && sri->printMin>=sentenceStarts[s] && sri->printMin>=sentenceStarts[s+1])
		{
			if (s>0)
				lplog(LOG_WHERE,L"%d-%d sentence skipped (current printMin=%d printMax=%d).",sentenceStarts[s],sentenceStarts[s+1],sri->printMin,sri->printMax);
			s++;
		}
		if (s+1<sentenceStarts.size() && sri->printMin>=sentenceStarts[s] && sri->printMax<=sentenceStarts[s+1])
		{
			if ((sri->relationType==stLOCATION || sri->relationType==stNORELATION) && sri->whereVerb>=0)
			{
				wstring verb,verbClassWords;
				unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(sri->whereVerb,verb);
				if (lvtoCi!=vbNetVerbToClassMap.end())
					for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
						vbNetClasses[*vbi].incorporatedVerbClassString(verbClassWords);
				lplog(LOG_WHERE,L"%d-%d location alternate verb classes for %s:%s.",sentenceStarts[s],sentenceStarts[s+1],verb.c_str(),verbClassWords.c_str());
			}
			if ((sri->relationType!=stLOCATION  && sri->relationType!=stNORELATION) || (sri->whereVerb>=0 && sri->whereQuestionType>=0) || !inQuestion)
				s++;
		}
		sri->sentenceNum=s;
		return false;
}

int SRIDebugCounter=0;
void Source::printCAS(wstring logPrefix,cAS &childCAS)
{ LFS
		wstring ps;
		prepPhraseToString(childCAS.wp,ps);
		printSRI(logPrefix,childCAS.sri,0,childCAS.ws,childCAS.wo,ps,false,-1,L"");
}

void Source::printSRI(wstring logPrefix,cSpaceRelation* sri,int s,int ws,int wo,int ps,bool overWrote,int matchSum,wstring matchInfo,int logDestination)
{ LFS
		wstring tmpstr;
		prepPhraseToString(ps,tmpstr);
		printSRI(logPrefix,sri,s,ws,wo,tmpstr,overWrote,matchSum,matchInfo,logDestination);
}

void Source::printSRI(wstring logPrefix,cSpaceRelation* sri,int s,int ws,int wo,wstring ps,bool overWrote,int matchSum,wstring matchInfo,int logDestination)
{ LFS
	if (sri==NULL)
		return;
	wstring tmpstr,tmpstr2,tmpstr3,tmpstr4,tmpstr5,tmpstr6,tmpstr7,tmpstr8,tmpstr9,tmpstr10,tmpstr11,tmpstr12,tmpstr14;
	bool inQuestion=(sri->whereSubject>=0 && (m[sri->whereSubject].flags&WordMatch::flagInQuestion));
	inQuestion|=(sri->whereObject>=0 && (m[sri->whereObject].flags&WordMatch::flagInQuestion));
	//if (SRIDebugCounter==464)
	//{
	//	extern int logQuestionProfileTime,logSynonymDetail,logTableDetail,equivalenceLogDetail,logDetail;
	//	logQuestionProfileTime=logSynonymDetail=logTableDetail=equivalenceLogDetail=logDetail=1;
	//}
	if (matchSum>=0) 
	{
		itos(SRIDebugCounter++,tmpstr12);
		itos(matchSum,tmpstr14);
		if (logDestination&LOG_QCHECK)
			tmpstr14=L"["+tmpstr14+L" "+matchInfo+L"]";
		else
			tmpstr14=L"["+tmpstr12+L":"+tmpstr14+L" "+matchInfo+L"]";
	}
	else if (s>=0)
	{
		itos(s,tmpstr12);
		tmpstr14=L"S"+tmpstr12;
	}
	const wchar_t *tmp1=0,*tmp2=0,*tmp3=0,*tmp4=0,*tmp5=0,*tmp6=0;
	bool shortFormat=(logDestination&LOG_QCHECK)!=0;
	wchar_t *f1 = L"%s:%06d:%s%s%s %s %s%s %s %s %s %s %s%s %s%s%s%s %s %s%s%s%s%s%s%s%s%s%s%s%s";
	//wchar_t *f2 = L"%s:%06d:1%s2%s3%s 4%s 5%s6%s 7%s 8%s 9%s a%s b%sc%s d%se%sf%sg%s h%s i%sj%sk%sl%sm%sn%so%sp%sq%sr%ss%st%su";
	//0  1   2  3 4  5  6 7  8  9 10 11 1213 14151617 18 192021222324252627282930
	lplog(logDestination, f1, logPrefix.c_str(),
		sri->where, // 1
		tmpstr14.c_str(), // 2
		(sri->whereQuestionType<0 && sri->relationType!=stLOCATION && sri->relationType!=-stLOCATION && sri->relationType!=stNORELATION && inQuestion) ? L"***":L"", //3
		(overWrote) ? L" OVERWRITE":L"", // 4
		relationString(sri->relationType).c_str(),  // 5
		(sri->whereQuestionType>=0) ? m[sri->whereQuestionType].word->first.c_str() : L"", // 6
		wrti(sri->whereControllingEntity,L"controller",tmpstr,shortFormat),  // 7
		(sri->whereControllingEntity<0) ? L"":wchr(m[sri->whereControllingEntity].getRelVerb()), // 8
		getWOSAdjective(ws,tmpstr2).c_str(), // 9
		getWSAdjective(ws,0).c_str(), // 10
		getWSAdjective(ws,1).c_str(), // 11
		wrti(ws,L"S",tmpstr3,shortFormat), // 12
		(sri->tft.negation) ? L"[NOT]":L"", // 13
		wchr(sri->whereVerb), // 14
		tmp1=getWSAdverb(sri->whereVerb,sri->changeStateAdverb), // 15
		tmp2=getWOSAdjective(sri->whereVerb,wo,tmpstr4).c_str(), // 16
		tmp3=getWSAdjective(sri->whereVerb,wo,0,tmpstr5).c_str(), // 17
		tmp4=getWSAdjective(sri->whereVerb,wo,1,tmpstr6).c_str(), // 18
		tmp5=getWSAdjective(sri->whereVerb,wo,2,tmpstr7).c_str(), // 19
		tmp6=wrti(wo,L"O",tmpstr8,shortFormat), // 20
		wchr(sri->whereSecondaryVerb), // 21
		getWOSAdjective(sri->whereSecondaryObject,tmpstr9).c_str(), // 22
		getWSAdjective(sri->whereSecondaryObject,0).c_str(), // 23
		getWSAdjective(sri->whereSecondaryObject,1).c_str(), // 24
		wrti(sri->whereSecondaryObject,L"O2",tmpstr10,shortFormat), // 25
		wrti(sri->whereNextSecondaryObject,L"nextObject2",tmpstr11,shortFormat), // 26
		(sri->objectSubType>=0) ? OCSubTypeStrings[sri->objectSubType] :L"", // 27
		(sri->whereObject<0) ? L"" : wrti(m[sri->whereObject].relNextObject,L"nextObject",tmpstr12,shortFormat), // 28
		ps.c_str(), //29
		(inQuestion) ? L"?":L"."); // 30
}

int Source::checkInsertPrep(set <int> &relPreps,int wp,int wo)
{
	if (wp>=m.size() || relPreps.find(wp)!=relPreps.end())
		return -1;
	if (wo<0 || m[wp].nextQuote<0 || m[wp].nextQuote==wo) // get only the prepositions attached to the wo
		relPreps.insert(wp);
	return 0;
}

// get all prepositions, discard any prepositions that have attached objects as their head (subjects or objects of a verb) that are not the wo in question.
// Krugman earned his B.A. in economics from Yale University summa cum laude in 1974 and his PhD from the Massachusetts Institute of Technology (MIT) in 1977.
// please also see http://aclweb.org/anthology-new/J/J06/J06-3002.pdf
// 
void Source::getAllPreps(cSpaceRelation* sri,set <int> &relPreps,int wo)
{ LFS
	if (sri->whereVerb>=0 && sri->wherePrep!=m[sri->whereVerb].relPrep)
		for (int wp=m[sri->whereVerb].relPrep; wp>=0 && wp+1<(int)m.size(); wp=m[wp].relPrep)
			if (checkInsertPrep(relPreps,wp,wo)<0) 
				break;
	for (int wp=sri->wherePrep; wp>=0 && wp+1<(int)m.size(); wp=m[wp].relPrep)
		if (checkInsertPrep(relPreps,wp,wo)<0) 
			break;
	for (int wp=sri->whereSecondaryPrep; wp>=0 && wp+1<(int)m.size(); wp=m[wp].relPrep)
		if (checkInsertPrep(relPreps,wp,wo)<0) 
			break;
	if (wo>=0 && m[wo].relPrep>=0)
	{
		for (int wp=m[wo].relPrep; wp>=0 && wp+1<(int)m.size(); wp=m[wp].relPrep)
			if (checkInsertPrep(relPreps,wp,wo)<0) 
				break;
	}
}

int Source::flushMultiWordRelations(set <int> &mwObjects)
{ LFS
	if (!myquery(&mysql,L"LOCK TABLES multiWordRelations WRITE, prepPhraseMultiWordRelations WRITE")) return -1;
  int startTime=clock(),multiWordRelationsInserted=0,where,lastProgressPercent=-1;
  wchar_t nqt[QUERY_BUFFER_LEN_OVERFLOW],pnqt[QUERY_BUFFER_LEN_OVERFLOW];
	wcscpy(nqt,L"INSERT into multiWordRelations (sourceId,w,sentenceNum,narrativeNum,speaker,audience,referencingEntityLocal,referencingEntityMatched, referencingVerb, "
		L"subjectAdjectiveMatchedOwner, subjectAdjective, subjectAdjective2, subjectLocal, subjectMatched, verb, adverb, "
		L"objectAdjectiveMatchedOwner, objectAdjective, objectAdjective2, objectAdjective3, objectLocal, objectMatched, "
		L"nextObjectLocal, nextObjectMatched, secondaryVerb, "
		L"secondaryObjectAdjectiveMatchedOwner, secondaryObjectAdjective, secondaryObjectAdjective2, secondaryObjectLocal, secondaryObjectMatched, "
		L"nextSecondaryObjectLocal, nextSecondaryObjectMatched, "
		L"relationType, objectSubType, timeProgression, flags) VALUES ");
	wcscpy(pnqt,L"INSERT into prepPhraseMultiWordRelations (sourceId,w,nearestVerb,nearestObject,principalWord,immediatePrincipalWord,prep, prepObjectAdjectiveMatchedOwner, prepObjectAdjective, prepObjectAdjective2, prepObjectLocal, prepObjectMatched, prepObjectSubType, flags) VALUES ");
  size_t nlen=wcslen(nqt),lastNlen=-1;
  size_t pnlen=wcslen(pnqt),lastPNlen=-1;
	unsigned int sni=0;
	int totalMultiRelations=spaceRelations.size();
	int lastMin=-1,lastMax=-1;
	unsigned int s=0; 
	for (vector <cSpaceRelation>::iterator sri=spaceRelations.begin(), sriEnd=spaceRelations.end(); sri!=sriEnd; sri++)
	{
		int narrativeNum=(m[sri->where].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE)) ? -1:0,speaker=-1,audience=-1;
		if (narrativeNum<0 && sri->tft.lastOpeningPrimaryQuote>=0 && m[sri->tft.lastOpeningPrimaryQuote].objectMatches.size()>0)
		{
			speaker=m[sri->tft.lastOpeningPrimaryQuote].objectMatches[0].object;
			if (m[sri->tft.lastOpeningPrimaryQuote].audienceObjectMatches.size()>0)
				audience=m[sri->tft.lastOpeningPrimaryQuote].audienceObjectMatches[0].object;
			if (speaker>=0) mwObjects.insert(speaker);
			if (audience>=0) mwObjects.insert(audience);
		}
		while (sni+1<subNarratives.size() && subNarratives[sni+1]<=sri->where)
			sni++;
		if (m[sri->where].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE) narrativeNum=(signed)sni;
		if (advanceSentenceNum(sri,s,lastMin,lastMax,sri->whereQuestionType>=0))
			continue;
		bool overWrote;
		if (overWrote=sri->printMin<=lastMin && sri->printMax>=lastMax && (sri->whereVerb==sri[-1].whereVerb || sri[-1].whereVerb<0))
		{
			multiWordRelationsInserted--;
			nlen=lastNlen;
			pnlen=lastPNlen;
		}
		lastMin=sri->printMin;
		lastMax=sri->printMax;
		wstring tmpstr;
		if (sri->printMin!=sri->printMax && sri->printMin>=0 && sri->printMax<1000000)
			lplog(LOG_WHERE,L"%s",phraseString(sri->printMin,sri->printMax+1,tmpstr,true).c_str());
		bool inPrimaryQuote=(m[sri->where].objectRole&(IN_PRIMARY_QUOTE_ROLE))!=0;
		bool inSecondaryQuote=(m[sri->where].objectRole&(IN_SECONDARY_QUOTE_ROLE))!=0;
		for (int ws=sri->whereSubject; true; ws=m[ws].nextCompoundPartObject)
		{
			for (int wo=sri->whereObject; true; wo=m[wo].nextCompoundPartObject)
			{
				nlen+=_snwprintf(nqt+nlen,QUERY_BUFFER_LEN-nlen,L"(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%I64d),",
					sourceId, // 1
					sri->where, // 2
					s, // 3
					narrativeNum, // 4
					speaker, // 5
					audience, // 6
					(sri->whereControllingEntity<0) ? NULLWORD:m[sri->whereControllingEntity].word->second.index, // 7
					(sri->whereControllingEntity<0) ? -1:m[sri->whereControllingEntity].getObject(), // 8
					(sri->whereControllingEntity<0) ? NULLWORD:getVerbIndex(m[sri->whereControllingEntity].getRelVerb()), // 9
					getOSAdjective(ws), // 10
					getMSAdjective(ws,0), // 11
					getMSAdjective(ws,1), // 12
					(ws<0) ? NULLWORD:m[ws].word->second.index, // 13
					getMatchedObject(ws,mwObjects), // 14
					getVerbIndex(sri->whereVerb), // 15
					getMSAdverb(sri->whereVerb,sri->changeStateAdverb), // 16
					getOSAdjective(sri->whereVerb,wo), // 17
					getMSAdjective(sri->whereVerb,wo,0), // 18
					getMSAdjective(sri->whereVerb,wo,1), // 19
					getMSAdjective(sri->whereVerb,wo,2), // 20
					(wo<0) ? NULLWORD:m[wo].word->second.index, // 21
					getMatchedObject(wo,mwObjects), // 22
					(sri->whereObject<0 || m[sri->whereObject].relNextObject<0) ? NULLWORD:m[m[sri->whereObject].relNextObject].word->second.index, // 23
					(sri->whereObject<0) ? -1 : getMatchedObject(m[sri->whereObject].relNextObject,mwObjects), // 24
					getVerbIndex(sri->whereSecondaryVerb), // he ran to fetch her. // 25
					getOSAdjective(sri->whereSecondaryObject), // 26
					getMSAdjective(sri->whereSecondaryObject,0), // 27
					getMSAdjective(sri->whereSecondaryObject,1), // 28
					(sri->whereSecondaryObject<0) ? NULLWORD:m[sri->whereSecondaryObject].word->second.index, // 29
					getMatchedObject(sri->whereSecondaryObject,mwObjects), // I guess you have no right to ask such a thing! // 30
					(sri->whereNextSecondaryObject<0) ? NULLWORD:m[sri->whereNextSecondaryObject].word->second.index, // 31
					getMatchedObject(sri->whereNextSecondaryObject,mwObjects), // I guess you have no right to ask such a thing! // 32
					sri->relationType, // 33
					sri->objectSubType, // 34
					sri->timeProgression, // 35
					// skipping tft.lastOpeningPrimaryQuote && tft.duplicateTimeTransitionFromWhere
					sri->convertFlags(sri->whereQuestionType>=0,inPrimaryQuote,inSecondaryQuote,sri->questionType)); // skipping description and all other tft info except booleans that are already encoded in flags // 36
				set <int> relPreps;
				getAllPreps(&(*sri),relPreps,wo);
				wstring ps;
				for (set <int>::iterator rp=relPreps.begin(),rpEnd=relPreps.end(); rp!=rpEnd; rp++)
					insertPrepPhrase(sri,sri->where,*rp, sri->whereSecondaryVerb, sri->whereSecondaryObject, relPreps,pnqt,pnlen,mwObjects,ps);
				lastNlen=nlen;
				lastPNlen=pnlen;
				printSRI(L"FMWR",&(*sri),s,ws,wo,ps,overWrote,-1,L"");
				if (wo<0 || m[wo].nextCompoundPartObject<0) break;
			}
			if (ws<0 || m[ws].nextCompoundPartObject<0) break;
		}
		while (sri+1<sriEnd && sri[1].where<sri->printMax && sri[1].whereObject<sri->printMax && (sri->whereVerb==sri[1].whereVerb || sri[1].whereVerb<0)) 
		{
			sri++;
			sri->skip=true;
		}
		multiWordRelationsInserted++;
    if ((where=multiWordRelationsInserted*100/totalMultiRelations)>lastProgressPercent)
    {
      wprintf(L"PROGRESS: %03d%% multiword relations flushed with %04d seconds elapsed \r",where,clocksec());
      lastProgressPercent=where;
    }
		if (!checkFull(&mysql,nqt,nlen,false,NULL)) 
			return -1;
		if (!checkFull(&mysql,pnqt,pnlen,false,NULL)) 
			return -1;
  }
	if (!checkFull(&mysql,nqt,nlen,true,NULL)) 
		return -1;
	if (!checkFull(&mysql,pnqt,pnlen,true,NULL)) 
		return -1;
  if ((clock()-startTime)>CLOCKS_PER_SEC && logDatabaseDetails) // relationsInserted &&
    lplog(L"Inserting %d multiwordrelations took %d seconds.",multiWordRelationsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
	updateSourceStatistics3(multiWordRelationsInserted);
  wprintf(L"PROGRESS: 100%% multiword relations flushed with %04d seconds elapsed \n",clocksec());
	myquery(&mysql,L"UNLOCK TABLES");
	return 0;
}
