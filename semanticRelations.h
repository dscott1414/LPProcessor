#pragma once
#include "timeRelations.h"
// accumulateProximityMaps
// accumulateProximityEntry
// semanticCheck
class cQuestionAnswering;
class cProximityMap
{
public:
	wstring SMPrincipalObject;
	set <wstring> sourcePaths;
	class cProximityEntry
	{
	public:
		int inSource;
		int totalDistanceFromObject;
		int directRelation;
		int confidentInSource;
		int confidentTotalDistanceFromObject;
		int confidentDirectRelation;
		int confidenceSE;
		wstring fullDescriptor;
		int semanticMismatch;
		bool subQueryNoMatch, tenseMismatch, confidenceCheck;
		cSource *childSource;
		set <wstring> childSourcePaths;
		vector <wstring> relationSourcePaths;
		vector <int> relationWheres;
		wstring lastChildSourcePath;
		int childWhere2;
		int childObject;
		float score;
		// this is called from the parent
		int semanticCheck(cQuestionAnswering &qa, cSpaceRelation* parentSRI, cSource *parentSource);
		void printDirectRelations(cQuestionAnswering &qa, int logType, cSource *parentSource, wstring &path, int where);
		cProximityEntry();
		cProximityEntry(cSource *childSource, unsigned int childSourceIndex, int childObject, cSpaceRelation* parentSRI);
		void lplogSM(int logType, wstring objectStr)
		{
			wstring tmpstr;
			::lplog(logType, L"SM object: %s: score=%f inSource=%d totalDistanceFromObject=%d directRelation=%d confidentInSource=%d confidentTotalDistanceFromObject=%d confidentDirectRelation=%d confidence=%d semanticMismatch=%d subQueryNoMatch=%s tenseMismatch=%s confidenceCheck=%s numSources=%d",
				objectStr.c_str(), score, inSource, totalDistanceFromObject, directRelation, confidentInSource, confidentTotalDistanceFromObject, confidentDirectRelation, confidenceSE,
				semanticMismatch, (subQueryNoMatch) ? L"true" : L"false", (tenseMismatch) ? L"true" : L"false", (confidenceCheck) ? L"true" : L"false", childSourcePaths.size());
		}
		void calculateScore()
		{
			int occurrence = (inSource + confidentInSource * 2 + directRelation * 2 + confidentDirectRelation * 4);
			if (childSourcePaths.size() == 1)
				occurrence >>= 1;
			if (totalDistanceFromObject + confidentTotalDistanceFromObject)
				score = (float)((occurrence*occurrence)*1.0 / (totalDistanceFromObject + confidentTotalDistanceFromObject));
		}
	};
	struct semanticSetCompare
	{
		bool operator()(unordered_map <wstring, cProximityEntry>::iterator lhs, unordered_map <wstring, cProximityEntry>::iterator rhs) const
		{
			if (lhs->second.confidentInSource + lhs->second.inSource == rhs->second.confidentInSource + rhs->second.inSource)
				return lhs->first < rhs->first;
			return lhs->second.confidentInSource + lhs->second.inSource > rhs->second.confidentInSource + rhs->second.inSource;
		}
	};
	struct proximityScoreCompare
	{
		bool operator()(unordered_map <wstring, cProximityEntry>::iterator lhs, unordered_map <wstring, cProximityEntry>::iterator rhs) const
		{
			return lhs->second.score > rhs->second.score;
		}
	};
	unordered_map <wstring, cProximityEntry> closestObjects;
	set < unordered_map <wstring, cProximityEntry>::iterator, semanticSetCompare> relativeObjectsSorted;
	set < unordered_map <wstring, cProximityEntry>::iterator, proximityScoreCompare> objectsSortedByProximityScore;
	set < unordered_map <wstring, cProximityEntry>::iterator, semanticSetCompare > suggestedAnswers;
	void sortAndCheck(cQuestionAnswering &qa,cSpaceRelation* parentSRI, cSource *parentSource)
	{
		relativeObjectsSorted.clear();
		objectsSortedByProximityScore.clear();
		for (unordered_map <wstring, cProximityEntry>::iterator roi = closestObjects.begin(), roiEnd = closestObjects.end(); roi != roiEnd; roi++)
		{
			roi->second.calculateScore();
			relativeObjectsSorted.insert(roi);
			objectsSortedByProximityScore.insert(roi);
		}
		int onlyTopResults = 0;
		for (set < unordered_map <wstring, cProximityEntry>::iterator, semanticSetCompare>::iterator sroi = relativeObjectsSorted.begin(), sroiEnd = relativeObjectsSorted.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			if ((*sroi)->second.semanticCheck(qa,parentSRI, parentSource) < CONFIDENCE_NOMATCH)
				suggestedAnswers.insert((*sroi));
		}
		onlyTopResults = 0;
		for (set < unordered_map <wstring, cProximityEntry>::iterator, proximityScoreCompare>::iterator sroi = objectsSortedByProximityScore.begin(), sroiEnd = objectsSortedByProximityScore.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			if ((*sroi)->second.semanticCheck(qa,parentSRI, parentSource) < CONFIDENCE_NOMATCH)
				suggestedAnswers.insert((*sroi));
		}
	}
	void lplogSM(cQuestionAnswering &qa, int logType, cSource *parentSource, bool enhanced)
	{
		::lplog(logType, L"SM%s SEMANTIC MAP %d objects %d sources principalObject %s ****************************************************************************",
			(enhanced) ? L"E" : L"", closestObjects.size(), sourcePaths.size(), SMPrincipalObject.c_str());
		extern int logDetail;
		if (logDetail)
			for (set <wstring>::iterator spi = sourcePaths.begin(), spiEnd = sourcePaths.end(); spi != spiEnd; spi++)
				::lplog(logType, L"SM%s sourcePath: %s", (enhanced) ? L"E" : L"", spi->c_str());
		int onlyTopResults = 0;
		::lplog(logType, L"SM%s by frequency ***************", (enhanced) ? L"E" : L"");
		for (set < unordered_map <wstring, cProximityEntry>::iterator, semanticSetCompare>::iterator sroi = relativeObjectsSorted.begin(), sroiEnd = relativeObjectsSorted.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			(*sroi)->second.lplogSM(logType, (*sroi)->first);
		}
		::lplog(logType, L"SM%s by score ***************", (enhanced) ? L"E" : L"");
		onlyTopResults = 0;
		for (set < unordered_map <wstring, cProximityEntry>::iterator, semanticSetCompare>::iterator sroi = objectsSortedByProximityScore.begin(), sroiEnd = objectsSortedByProximityScore.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			(*sroi)->second.lplogSM(logType, (*sroi)->first);
		}
		if (suggestedAnswers.empty())
			::lplog(logType, L"SM%s no suggested answers.", (enhanced) ? L"E" : L"");
		else
		{
			::lplog(logType, L"SM%s suggested answers ***************", (enhanced) ? L"E" : L"");
			for (set < unordered_map <wstring, cProximityEntry>::iterator, semanticSetCompare >::iterator sai = suggestedAnswers.begin(), saiEnd = suggestedAnswers.end(); sai != saiEnd; sai++)
			{
				closestObjects[(*sai)->first].lplogSM(LOG_WHERE, (*sai)->first);
				if (logProximityMap)
					for (unsigned int I = 0; I < (*sai)->second.relationSourcePaths.size(); I++)
						(*sai)->second.printDirectRelations(qa,logType, parentSource, (*sai)->second.relationSourcePaths[I], (*sai)->second.relationWheres[I]);
			}
		}
		::lplog(logType, L"SM%s END SEMANTIC MAP %d objects %d sources principalObject %s ****************************************************************************",
			(enhanced) ? L"E" : L"", closestObjects.size(), sourcePaths.size(), SMPrincipalObject.c_str());
	}
};

// stEXIT - something is exiting from the sight or knowledge of the entity with point of view 
// stENTERING - something is entering the scene
// stSTAY - something is NOT exiting but is specifically staying in one place.
// stESTABLISH - a location is being set for a subject or object.
// stMOVE - an object is moving itself from one location to another.
// stMOVE_OBJECT - an object is moving something or someone
// stMOVE_IN_PLACE - an object is moving but staying in place (turning, twisting or so forth)
// stCONTACT - two or more objects are in one place or next to each other (because they contacted each other)
// stNEAR - two or more objects are near one another (one is following the other)
// stLOCATION - locating an object but without any action associated with it
// stTIME - a time transition (two days later...)
enum st { stEXIT=1,stENTER=2,stSTAY=3,stESTABLISH=4,stMOVE=5,stMOVE_OBJECT=6,stMOVE_IN_PLACE=7,stMETAWQ=8,stCONTACT=9,stNEAR=10,stTRANSFER=11,stLOCATION=12,
          stPREPTIME=13,stPREPDATE=14,stSUBJDAYOFMONTHTIME=15,stABSTIME=16,stABSDATE=17,stADVERBTIME=18,
          stTHINK=19, stCOMMUNICATE=20, stSTART=21, stCHANGE_STATE=22, stBE, stHAVE, stMETAINFO, stMETAPROFESSION,stMETABELIEF,
					stTHINKOBJECT,stCHANGESTATE,stCONTIGUOUS,stCONTROL,stAGENTCHANGEOBJECTINTERNALSTATE,stSENSE,stCREATE,
					stCONSUME,stMETAFUTUREHAVE,stMETAFUTURECONTACT,stMETAIFTHEN,stMETACONTAINS,stMETADESIRE,stMETAROLE,stSPATIALORIENTATION,
					stLOCATIONRP,stESTAB,stIGNORE,stNORELATION,stOTHER,
					stLAST};


class cTimeFlowTense
{
public:
	bool speakerCommand;
	bool speakerQuestionToAudience;
	bool significantRelation;
	bool beyondLocalSpace; // movement beyond a room or a local area
	bool story; // also includes future stories
	bool timeTransition;
	bool nonPresentTimeTransition;
	int duplicateTimeTransitionFromWhere;
	// futureInPastHappening is talking about the future, while the action is in the past.
	bool beforePastHappening,pastHappening,presentHappening,futureHappening,futureInPastHappening;
	bool negation;
	int lastOpeningPrimaryQuote;
  wstring presType;
	cTimeFlowTense()
	{
		speakerCommand=false;
		speakerQuestionToAudience = false;
		significantRelation = false;
		beyondLocalSpace = false; // movement beyond a room or a local area
		story = false; // also includes future stories
		timeTransition = false;
		nonPresentTimeTransition = false;
		duplicateTimeTransitionFromWhere=0;
		// futureInPastHappening is talking about the future, while the action is in the past.
		beforePastHappening = false;
		pastHappening = false;
		presentHappening = false;
		futureHappening = false;
		futureInPastHappening = false;
		negation = false;
		lastOpeningPrimaryQuote=-1;
	}
};

class cSpaceRelation
{
public:
	int where;
	int o;
	int whereControllingEntity;
	int whereSubject;
	int whereVerb;
	int wherePrep;
	int whereObject;
	int wherePrepObject;
	int whereSecondaryVerb;
	int whereSecondaryObject;
	int whereSecondaryPrep;
	int whereNextSecondaryObject;
	int whereMovingRelativeTo;
	int relationType;
	int objectSubType;
	int prepObjectSubType;
	int timeProgression;
	__int64 questionType;
	int whereQuestionType;
	int whereQuestionTypeObject;
	int whereChildCandidateAnswer;
	int sentenceNum;
	int printMin;
	int printMax;
	set <int> whereQuestionInformationSourceObjects;
	unordered_map <int,cProximityMap *> proximityMaps;
	bool genderedEntityMove;
	bool genderedLocationRelation;
	bool establishingLocation;
	bool futureLocation;
	bool speakerContinuation; // wherePrepObject least one speaker appears as a speaker before and after the establishing location, so don't age speakers
	bool agentLocationRelationSet;
	bool timeInfoSet;
	bool physicalRelation;
	bool skip;
	bool changeStateAdverb; // On what date did the court begin screening potential jurors?
	bool subQuery;
	bool isConstructedRelative;
	bool prepositionUncertain;
	bool nonSemanticSubjectTotalMatch; // two phases of matching (minimize expense) - syntactic (word by word, names) and semantic (what is a thing=thing).  does this part of the proposed answer TOTALLY match the question in the first phase?
	bool nonSemanticObjectTotalMatch; 
	bool nonSemanticSecondaryObjectTotalMatch; 
	bool nonSemanticPrepositionObjectTotalMatch; 
	cTimeFlowTense tft;
	wstring description;
	int nextSPR;
	vector <cTimeInfo> timeInfo;

	cSpaceRelation(int _where, int _o, int _whereControllingEntity, int _whereSubject, int _whereVerb, int _wherePrep, int _whereObject,
		int _wherePrepObject, int _movingRelativeTo, int _relationType,
		bool _genderedEntityMove, bool _genderedLocationRelation, int _objectSubType, int _prepObjectSubType, bool _physicalRelation);
	bool operator != (const cSpaceRelation &z)
  {
    return where!=z.where || 
			o!=z.o || 
			whereControllingEntity!=z.whereControllingEntity || 
			whereSubject!=z.whereSubject || 
			whereVerb!=z.whereVerb || 
			wherePrep!=z.wherePrep || 
			whereObject!=z.whereObject || 
			wherePrepObject!=z.wherePrepObject || 
			whereMovingRelativeTo!=z.whereMovingRelativeTo || 
			relationType!=z.relationType;
  }
  bool operator == (const cSpaceRelation &z)
  {
    return where==z.where &&
			o==z.o && 
			whereControllingEntity==z.whereControllingEntity && 
			whereSubject==z.whereSubject && 
			whereVerb==z.whereVerb && 
			wherePrep==z.wherePrep && 
			whereObject==z.whereObject && 
			wherePrepObject==z.wherePrepObject && 
			whereMovingRelativeTo==z.whereMovingRelativeTo && 
			relationType==z.relationType;
  }
	bool canUpdate(cSpaceRelation &z);
	cSpaceRelation(char *buffer, int &w, unsigned int total, bool &error);
	int sanityCheck(int maxSourcePosition, int maxObjectIndex);
	void convertToFlags(__int64 flags);
	__int64 convertFlags(bool isQuestion, bool inPrimaryQuote, bool inSecondaryQuote, __int64 questionFlags);
	bool write(void *buffer, int &w, int limit);
	cSpaceRelation(vector <cSpaceRelation>::iterator sri, unordered_map <int, int> &sourceMap);
};

