/*
	semanticRelations.h - st* relation enum, cTimeFlowTense, cSyntacticRelationGroup,
		and the QA proximity map used to rank candidate answers

	Overview:
		Declares the space/motion/state relation types produced by
		semanticRelations.cpp, the per-clause tense/flow flags, and the SRG
		that binds where* source-position slots to one relationType. Also
		declares cProximityMap, used later by question answering to score
		objects that co-occur with a principal entity across child sources.

	Pipeline position:
		Included from source.h. Instantiated during stage 5; consumed by
		speaker resolution (stage 7) and question answering (stage 8).

	Key data structures / globals:
		- enum st - ENTER/EXIT/MOVE/CONTACT/… plus TimeML-ish PREPTIME /
		  ABSTIME / ADVERBTIME; stLAST is the count sentinel
		- cTimeFlowTense - narration vs quote vs story, plus Reichenbach
		  happening bits (beforePast / past / present / future / futureInPast)
		- cSyntacticRelationGroup - one clause-level relation; where* index m,
		  o indexes objects, timeInfo is filled by timeRelations.cpp
		- cProximityMap - closestObjects keyed by printed object name; score
		  is occurrence^2 / distance (see calculateScore)

	Notes / gotchas:
		- stBE has no explicit enumerator value; it continues from
		  stCHANGE_STATE=22. Do not renumber without a cache bump.
		- -1 is “unset” on every where* / o field.
		- cProximityEntry::score is set to occurrence^2/distance when the
		  distance sum is non-zero, else explicitly 0.
*/
#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#include "timeRelations.h"
class cSyntacticRelationGroup;
// accumulateProximityMaps
// accumulateProximityEntry
// semanticCheck
class cQuestionAnswering;
class cProximityMap
{
public:
	lpwstring SMPrincipalObject;
	set <lpwstring> sourcePaths;
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
		lpwstring fullDescriptor;
		int semanticMismatch;
		bool subQueryNoMatch, tenseMismatch, confidenceCheck;
		cSource *childSource;
		set <lpwstring> childSourcePaths;
		vector <lpwstring> relationSourcePaths;
		vector <int> relationWheres;
		lpwstring lastChildSourcePath;
		int childWhere2;
		int childObject;
		float score;
		// this is called from the parent
		int semanticCheck(cQuestionAnswering &qa, cSyntacticRelationGroup* parentSRG, cSource *parentSource);
		void printDirectRelations(cQuestionAnswering &qa, int logType, cSource *parentSource, lpwstring &path, int where);
		cProximityEntry();
		cProximityEntry(cQuestionAnswering &qa, cSource *childSource, unsigned int childSourceIndex, int childObject, cSyntacticRelationGroup* parentSRG);
		// One-line LOG dump of this entry’s score / occurrence / mismatch flags.
		void lplogFrequentOrProximateObjects(int logType, lpwstring objectStr)
		{
			lpwstring tmpstr;
			::lplog(logType, u"SM object: %s: score=%f inSource=%d totalDistanceFromObject=%d directRelation=%d confidentInSource=%d confidentTotalDistanceFromObject=%d confidentDirectRelation=%d confidence=%d semanticMismatch=%d subQueryNoMatch=%s tenseMismatch=%s confidenceCheck=%s numSources=%d",
				objectStr.c_str(), score, inSource, totalDistanceFromObject, directRelation, confidentInSource, confidentTotalDistanceFromObject, confidentDirectRelation, confidenceSE,
				semanticMismatch, (subQueryNoMatch) ? u"true" : u"false", (tenseMismatch) ? u"true" : u"false", (confidenceCheck) ? u"true" : u"false", childSourcePaths.size());
		}
		// occurrence^2 / (totalDistance + confidentTotalDistance), or 0 when
		// both distances are 0. A single child source halves occurrence.
		void calculateScore()
		{
			int occurrence = (inSource + confidentInSource * 2 + directRelation * 2 + confidentDirectRelation * 4);
			if (childSourcePaths.size() == 1)
				occurrence >>= 1;
			if (totalDistanceFromObject + confidentTotalDistanceFromObject)
				score = (float)((occurrence*occurrence)*1.0 / (totalDistanceFromObject + confidentTotalDistanceFromObject));
			else
				score = 0;
		}
	};
	// Frequency order: more (confidentInSource + inSource) first; name tie-break.
	struct semanticSetCompare
	{
		bool operator()(unordered_map <lpwstring, cProximityEntry>::iterator lhs, unordered_map <lpwstring, cProximityEntry>::iterator rhs) const
		{
			if (lhs->second.confidentInSource + lhs->second.inSource == rhs->second.confidentInSource + rhs->second.inSource)
				return lhs->first < rhs->first;
			return lhs->second.confidentInSource + lhs->second.inSource > rhs->second.confidentInSource + rhs->second.inSource;
		}
	};
	// Higher calculateScore() first. Equal scores compare as equivalent.
	struct proximityScoreCompare
	{
		bool operator()(unordered_map <lpwstring, cProximityEntry>::iterator lhs, unordered_map <lpwstring, cProximityEntry>::iterator rhs) const
		{
			return lhs->second.score > rhs->second.score;
		}
	};
	unordered_map <lpwstring, cProximityEntry> closestObjects;
	set < unordered_map <lpwstring, cProximityEntry>::iterator, semanticSetCompare> objectsSortedByFrequency;
	set < unordered_map <lpwstring, cProximityEntry>::iterator, proximityScoreCompare> objectsSortedByProximityScore;
	set < unordered_map <lpwstring, cProximityEntry>::iterator, semanticSetCompare > frequentOrProximateObjects;
	// Rank closestObjects, semanticCheck the top 20 by frequency and by
	// score, and keep those with confidence < CONFIDENCE_NOMATCH.
	void sortByFrequencyAndProximity(cQuestionAnswering &qa,cSyntacticRelationGroup* parentSRG, cSource *parentSource)
	{
		objectsSortedByFrequency.clear();
		objectsSortedByProximityScore.clear();
		for (unordered_map <lpwstring, cProximityEntry>::iterator roi = closestObjects.begin(), roiEnd = closestObjects.end(); roi != roiEnd; roi++)
		{
			roi->second.calculateScore();
			objectsSortedByFrequency.insert(roi);
			objectsSortedByProximityScore.insert(roi);
		}
		int onlyTopResults = 0;
		for (set < unordered_map <lpwstring, cProximityEntry>::iterator, semanticSetCompare>::iterator sroi = objectsSortedByFrequency.begin(), sroiEnd = objectsSortedByFrequency.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			if ((*sroi)->second.semanticCheck(qa,parentSRG, parentSource) < CONFIDENCE_NOMATCH)
				frequentOrProximateObjects.insert((*sroi));
		}
		onlyTopResults = 0;
		for (set < unordered_map <lpwstring, cProximityEntry>::iterator, proximityScoreCompare>::iterator sroi = objectsSortedByProximityScore.begin(), sroiEnd = objectsSortedByProximityScore.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			if ((*sroi)->second.semanticCheck(qa,parentSRG, parentSource) < CONFIDENCE_NOMATCH)
				frequentOrProximateObjects.insert((*sroi));
		}
	}
	// Dump the map, the top-20 frequency/score lists, and suggested answers.
	void lplogFrequentOrProximateObjects(cQuestionAnswering &qa, int logType, cSource *parentSource, bool enhanced)
	{
		::lplog(logType, u"SM%s SEMANTIC MAP %d objects %d sources principalObject %s ****************************************************************************",
			(enhanced) ? u"E" : u"", closestObjects.size(), sourcePaths.size(), SMPrincipalObject.c_str());
		extern int logDetail;
		if (logDetail)
			for (set <lpwstring>::iterator spi = sourcePaths.begin(), spiEnd = sourcePaths.end(); spi != spiEnd; spi++)
				::lplog(logType, u"SM%s sourcePath: %s", (enhanced) ? u"E" : u"", spi->c_str());
		int onlyTopResults = 0;
		::lplog(logType, u"SM%s by frequency ***************", (enhanced) ? u"E" : u"");
		for (set < unordered_map <lpwstring, cProximityEntry>::iterator, semanticSetCompare>::iterator sroi = objectsSortedByFrequency.begin(), sroiEnd = objectsSortedByFrequency.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			(*sroi)->second.lplogFrequentOrProximateObjects(logType, (*sroi)->first);
		}
		::lplog(logType, u"SM%s by score ***************", (enhanced) ? u"E" : u"");
		onlyTopResults = 0;
		for (set < unordered_map <lpwstring, cProximityEntry>::iterator, proximityScoreCompare>::iterator sroi = objectsSortedByProximityScore.begin(), sroiEnd = objectsSortedByProximityScore.end(); sroi != sroiEnd && onlyTopResults < 20; sroi++)
		{
			onlyTopResults++;
			(*sroi)->second.lplogFrequentOrProximateObjects(logType, (*sroi)->first);
		}
		if (frequentOrProximateObjects.empty())
			::lplog(logType, u"SM%s no suggested answers.", (enhanced) ? u"E" : u"");
		else
		{
			::lplog(logType, u"SM%s suggested answers ***************", (enhanced) ? u"E" : u"");
			for (set < unordered_map <lpwstring, cProximityEntry>::iterator, semanticSetCompare >::iterator sai = frequentOrProximateObjects.begin(), saiEnd = frequentOrProximateObjects.end(); sai != saiEnd; sai++)
			{
				closestObjects[(*sai)->first].lplogFrequentOrProximateObjects(LOG_WHERE, (*sai)->first);
				if (logProximityMap)
					for (unsigned int I = 0; I < (*sai)->second.relationSourcePaths.size(); I++)
						(*sai)->second.printDirectRelations(qa,logType, parentSource, (*sai)->second.relationSourcePaths[I], (*sai)->second.relationWheres[I]);
			}
		}
		::lplog(logType, u"SM%s END SEMANTIC MAP %d objects %d sources principalObject %s ****************************************************************************",
			(enhanced) ? u"E" : u"", closestObjects.size(), sourcePaths.size(), SMPrincipalObject.c_str());
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
  lpwstring presType;
	// All bools false, lastOpeningPrimaryQuote = -1, duplicateTimeTransitionFromWhere = 0.
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

class cSyntacticRelationGroup
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
	int64_t questionType;
	int whereQuestionType;
	int whereQuestionTypeObject;
	int sentenceNum;
	int printMin;
	int printMax;
	set <int> whereQuestionInformationSourceObjects;
	unordered_map <int,cProximityMap *> proximityMaps;
	vector <mbInfoReleaseType> mbs;
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
	int transformedPrep;
	cTimeFlowTense tft;
	lpwstring description;
	int nextSPR;
	vector <cTimeInfo> timeInfo;
	cPattern* associatedPattern; // used only with question answering, particularly with verifying transformed questions
	cPattern* mapPatternAnswer;
	cPattern* mapPatternQuestion;
	cSyntacticRelationGroup(int _where, int _o, int _whereControllingEntity, int _whereSubject, int _whereVerb, int _wherePrep, int _whereObject,
		int _wherePrepObject, int _movingRelativeTo, int _relationType,
		bool _genderedEntityMove, bool _genderedLocationRelation, int _objectSubType, int _prepObjectSubType, bool _physicalRelation);
	// Inequality on the identity slots (where* + relationType), not tft/timeInfo.
	bool operator != (const cSyntacticRelationGroup &z)
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
	// Equality on the identity slots (where* + relationType), not tft/timeInfo.
  bool operator == (const cSyntacticRelationGroup &z)
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
	bool canUpdate(cSyntacticRelationGroup &z);
	cSyntacticRelationGroup(char *buffer, int &w, unsigned int total, bool &error);
	int sanityCheck(int maxSourcePosition, int maxObjectIndex);
	void convertToFlags(int64_t flags);
	int64_t convertFlags(bool isQuestion, bool inPrimaryQuote, bool inSecondaryQuote, int64_t questionFlags);
	bool write(void *buffer, int &w, int limit);
	cSyntacticRelationGroup(cSyntacticRelationGroup *srg, unordered_map <int, int> &sourceMap);
	bool adjustValue(int& val, int originalVal, lpwstring valString, unordered_map <int, int>& sourceIndexMap);
};

