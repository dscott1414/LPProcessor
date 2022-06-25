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

void Source::printCAS(wstring logPrefix,cAS &childCAS)
{ LFS
		wstring ps;
		prepPhraseToString(childCAS.wp,ps);
		printSRI(logPrefix,childCAS.sri,0,childCAS.ws,childCAS.wo,ps,false,-1,L"");
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
