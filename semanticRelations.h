#include "timeRelations.h"
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
					stLOCATIONRP,stESTAB,stIGNORE,stNORELATION,stOTHER };


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
	unordered_map <int,cSemanticMap *> semanticMaps;
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

	cSpaceRelation(int _where,int _o,int _whereControllingEntity,int _whereSubject,int _whereVerb,int _wherePrep,int _whereObject,
		             int _wherePrepObject,int _movingRelativeTo,int _relationType,
									bool _genderedEntityMove,bool _genderedLocationRelation,int _objectSubType,int _prepObjectSubType,bool _physicalRelation)
	{
		where=_where;
		o=_o;
		whereControllingEntity=_whereControllingEntity;
		whereSubject=_whereSubject;
		whereVerb=_whereVerb;
		wherePrep=_wherePrep;
		whereObject=_whereObject;
		wherePrepObject=_wherePrepObject;
		whereMovingRelativeTo=_movingRelativeTo;
		relationType=_relationType;
		genderedEntityMove=_genderedEntityMove;
		genderedLocationRelation=_genderedLocationRelation;
		objectSubType=_objectSubType;
		prepObjectSubType=_prepObjectSubType;
		physicalRelation=_physicalRelation;
		establishingLocation=false;
		futureLocation=false;
		tft.speakerCommand=false;
		tft.speakerQuestionToAudience=false;
		tft.significantRelation=false;
		tft.beyondLocalSpace=false; // movement beyond a room or a local area
		tft.story=false; // also includes future stories
		tft.timeTransition=false;
		tft.nonPresentTimeTransition=false;
		tft.duplicateTimeTransitionFromWhere=-1;
		tft.beforePastHappening=false;
		tft.pastHappening=false;
		tft.presentHappening=false;
		tft.futureHappening=false;
		tft.futureInPastHappening=false;
		tft.negation=false;
		agentLocationRelationSet=false;
		timeInfoSet=false;
		isConstructedRelative=false;
		prepositionUncertain=false;
		tft.lastOpeningPrimaryQuote=-1;
		nextSPR=-1;
		whereSecondaryVerb=-1;
		whereSecondaryObject=-1;
		whereNextSecondaryObject=-1;
		whereSecondaryPrep=-1;
		whereQuestionType=-1;
		questionType=0;
		sentenceNum=-1;
		subQuery=false;
		skip=false;
		changeStateAdverb = false;
		nonSemanticObjectTotalMatch = false;
		nonSemanticPrepositionObjectTotalMatch = false;
		nonSemanticSecondaryObjectTotalMatch = false;
		nonSemanticSubjectTotalMatch = false;
		printMax = -1;
		printMin = -1;
		speakerContinuation = false;
		timeProgression = -1;
		whereChildCandidateAnswer = -1;
		whereQuestionTypeObject = -1;
	}
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
	bool canUpdate(cSpaceRelation &z)
	{
    return where==z.where &&
			(o==z.o || (z.o<0 && o>0)) &&
			whereControllingEntity==z.whereControllingEntity &&
			whereSubject==z.whereSubject &&
			whereVerb==z.whereVerb &&
			(wherePrep==z.wherePrep || (z.wherePrep<0 && wherePrep>0)) &&
			whereObject==z.whereObject &&
			(wherePrepObject==z.wherePrepObject || (z.wherePrepObject<0 && wherePrepObject>0)) &&
			whereMovingRelativeTo==z.whereMovingRelativeTo && 
			relationType==z.relationType;
	}
    cSpaceRelation(char *buffer,int &w,unsigned int total,bool &error)
    {
      if (error=!copy(where,buffer,w,total)) return;
      if (error=!copy(o,buffer,w,total)) return;
      if (error=!copy(whereControllingEntity,buffer,w,total)) return;
      if (error=!copy(whereSubject,buffer,w,total)) return;
      if (error=!copy(whereVerb,buffer,w,total)) return;
      if (error=!copy(wherePrep,buffer,w,total)) return;
      if (error=!copy(whereObject,buffer,w,total)) return;
      if (error=!copy(wherePrepObject,buffer,w,total)) return;
      if (error=!copy(whereSecondaryVerb,buffer,w,total)) return;
      if (error=!copy(whereSecondaryObject,buffer,w,total)) return;
      if (error=!copy(whereSecondaryPrep,buffer,w,total)) return;
      if (error=!copy(whereNextSecondaryObject,buffer,w,total)) return;
      if (error=!copy(whereMovingRelativeTo,buffer,w,total)) return;
      if (error=!copy(relationType,buffer,w,total)) return;
      if (error=!copy(objectSubType,buffer,w,total)) return;
      if (error=!copy(prepObjectSubType,buffer,w,total)) return;
      if (error=!copy(timeProgression,buffer,w,total)) return;
      if (error=!copy(questionType,buffer,w,total)) return;
      if (error=!copy(whereQuestionType,buffer,w,total)) return;
      if (error=!copy(sentenceNum,buffer,w,total)) return;
			// flags
			__int64 flags;
			if (error=!copy(flags,buffer,w,total)) return;
			convertToFlags(flags);
      if (error=!copy(tft.lastOpeningPrimaryQuote,buffer,w,total)) return;
      if (error=!copy(tft.duplicateTimeTransitionFromWhere,buffer,w,total)) return;
      if (error=!copy(tft.presType,buffer,w,total)) return;
      if (error=!copy(description,buffer,w,total)) return;
      if (error=!copy(nextSPR,buffer,w,total)) return;
			int count;
      if (error=!copy(count,buffer,w,total)) return;
			subQuery=false;
		  timeInfo.reserve(count);
		  while (count-- && !error && w<(signed)total)
		    timeInfo.emplace_back(buffer,w,total,error);
    }
		void convertToFlags(__int64 flags)
		{
		  /*bool inSecondaryQuote=flags&1; */flags>>=1;
		  /*bool inPrimaryQuote=flags&1;*/ flags>>=1;
		  /*bool isQuestion=flags&1;*/ flags>>=1;
		  changeStateAdverb=flags&1; flags>>=1;
		  skip=flags&1; flags>>=1;
			physicalRelation=flags&1; flags>>=1;
			timeInfoSet=flags&1; flags>>=1;
			agentLocationRelationSet=flags&1; flags>>=1;
			tft.negation=flags&1; flags>>=1;
			tft.futureInPastHappening=flags&1; flags>>=1;
			tft.futureHappening=flags&1; flags>>=1;
			tft.presentHappening=flags&1; flags>>=1;
			tft.pastHappening=flags&1; flags>>=1;
			tft.beforePastHappening=flags&1; flags>>=1;
			tft.nonPresentTimeTransition=flags&1; flags>>=1;
			tft.timeTransition=flags&1; flags>>=1;
			tft.story=flags&1; flags>>=1;
			tft.beyondLocalSpace=flags&1; flags>>=1;
			tft.significantRelation=flags&1; flags>>=1;
			tft.speakerQuestionToAudience=flags&1; flags>>=1;
			tft.speakerCommand=flags&1; flags>>=1;
			speakerContinuation=flags&1; flags>>=1;
			futureLocation=flags&1; flags>>=1;
			establishingLocation=flags&1; flags>>=1;
			genderedLocationRelation=flags&1; flags>>=1;
			genderedEntityMove=flags&1; 
			// 6 questionFlags bits (see convertFlags)
		}
		__int64 convertFlags(bool isQuestion,bool inPrimaryQuote,bool inSecondaryQuote,__int64 questionFlags)
		{
			__int64 flags=(questionFlags<<6); 
		  flags|=(genderedEntityMove) ? 1:0; flags<<=1;
		  flags|=(genderedLocationRelation) ? 1:0; flags<<=1;
		  flags|=(establishingLocation) ? 1:0; flags<<=1;
		  flags|=(futureLocation) ? 1:0; flags<<=1;
		  flags|=(speakerContinuation) ? 1:0; flags<<=1;
		  flags|=(tft.speakerCommand) ? 1:0; flags<<=1;
		  flags|=(tft.speakerQuestionToAudience) ? 1:0; flags<<=1;
		  flags|=(tft.significantRelation) ? 1:0; flags<<=1;
		  flags|=(tft.beyondLocalSpace) ? 1:0; flags<<=1;
		  flags|=(tft.story) ? 1:0; flags<<=1;
		  flags|=(tft.timeTransition) ? 1:0; flags<<=1;
		  flags|=(tft.nonPresentTimeTransition) ? 1:0; flags<<=1;
		  flags|=(tft.beforePastHappening) ? 1:0; flags<<=1;
		  flags|=(tft.pastHappening) ? 1:0; flags<<=1;
		  flags|=(tft.presentHappening) ? 1:0; flags<<=1;
		  flags|=(tft.futureHappening) ? 1:0; flags<<=1;
		  flags|=(tft.futureInPastHappening) ? 1:0; flags<<=1;
		  flags|=(tft.negation) ? 1:0; flags<<=1;
		  flags|=(agentLocationRelationSet) ? 1:0; flags<<=1;
		  flags|=(timeInfoSet) ? 1:0; flags<<=1;
		  flags|=(physicalRelation) ? 1:0; flags<<=1;
		  flags|=(skip) ? 1:0; flags<<=1;
		  flags|=(changeStateAdverb) ? 1:0; flags<<=1;
		  flags|=(isQuestion) ? 1:0; flags<<=1;
		  flags|=(inPrimaryQuote) ? 1:0; flags<<=1;
		  flags|=(inSecondaryQuote) ? 1:0; 
			return flags;
		}
    bool write(void *buffer,int &w,int limit)
    {
      if (!copy(buffer,where,w,limit)) return false;
      if (!copy(buffer,o,w,limit)) return false;
      if (!copy(buffer,whereControllingEntity,w,limit)) return false;
      if (!copy(buffer,whereSubject,w,limit)) return false;
      if (!copy(buffer,whereVerb,w,limit)) return false;
      if (!copy(buffer,wherePrep,w,limit)) return false;
      if (!copy(buffer,whereObject,w,limit)) return false;
      if (!copy(buffer,wherePrepObject,w,limit)) return false;
      if (!copy(buffer,whereSecondaryVerb,w,limit)) return false;
      if (!copy(buffer,whereSecondaryObject,w,limit)) return false;
      if (!copy(buffer,whereSecondaryPrep,w,limit)) return false;
      if (!copy(buffer,whereNextSecondaryObject,w,limit)) return false;
      if (!copy(buffer,whereMovingRelativeTo,w,limit)) return false;
      if (!copy(buffer,relationType,w,limit)) return false;
      if (!copy(buffer,objectSubType,w,limit)) return false;
      if (!copy(buffer,prepObjectSubType,w,limit)) return false;
      if (!copy(buffer,timeProgression,w,limit)) return false;
      if (!copy(buffer,questionType,w,limit)) return false;
      if (!copy(buffer,whereQuestionType,w,limit)) return false;
      if (!copy(buffer,sentenceNum,w,limit)) return false;
			// flags
			__int64 flags=convertFlags(false,false,false,0); // while on disk, questions can be queried by the flag WordMatch::inQuestion
      if (!copy(buffer,flags,w,limit)) return false;
      if (!copy(buffer,tft.lastOpeningPrimaryQuote,w,limit)) return false; 
      if (!copy(buffer,tft.duplicateTimeTransitionFromWhere,w,limit)) return false;
      if (!copy(buffer,tft.presType,w,limit)) return false;
      if (!copy(buffer,description,w,limit)) return false;
      if (!copy(buffer,nextSPR,w,limit)) return false;
			unsigned int count=timeInfo.size();
			if (!copy((void *)buffer,count,w,limit)) return false;
			for (unsigned int I=0; I<count; I++)
				if (!timeInfo[I].write(buffer,w,limit)) return false;
      return true;
    }
		cSpaceRelation(vector <cSpaceRelation>::iterator sri, unordered_map <int,int> &sourceMap)
		{ 
			where=sourceMap[sri->where];
			o=-1;
			whereControllingEntity=sourceMap[sri->whereControllingEntity];
			whereSubject=sourceMap[sri->whereSubject];
			whereVerb=sourceMap[sri->whereVerb];
			wherePrep=sourceMap[sri->wherePrep];
			whereObject=sourceMap[sri->whereObject];
			wherePrepObject=sourceMap[sri->wherePrepObject];
			whereMovingRelativeTo=sourceMap[sri->whereMovingRelativeTo];
			relationType=sri->relationType;
			genderedEntityMove=sri->genderedEntityMove;
			genderedLocationRelation=sri->genderedLocationRelation;
			objectSubType=sri->objectSubType;
			prepObjectSubType=sri->prepObjectSubType;
			physicalRelation=sri->physicalRelation;
			prepositionUncertain=sri->prepositionUncertain;
			tft.speakerCommand=sri->tft.speakerCommand;
			tft.speakerQuestionToAudience=sri->tft.speakerQuestionToAudience;
			tft.significantRelation=sri->tft.significantRelation;
			tft.beyondLocalSpace=sri->tft.beyondLocalSpace;
			tft.story=sri->tft.story;
			tft.timeTransition=sri->tft.timeTransition;
			tft.nonPresentTimeTransition=sri->tft.nonPresentTimeTransition;
			tft.duplicateTimeTransitionFromWhere=sri->tft.duplicateTimeTransitionFromWhere;
			tft.beforePastHappening=sri->tft.beforePastHappening;
			tft.pastHappening=sri->tft.pastHappening;
			tft.presentHappening=sri->tft.presentHappening;
			tft.futureHappening=sri->tft.futureHappening;
			tft.futureInPastHappening=sri->tft.futureInPastHappening;
			tft.negation=sri->tft.negation;
			tft.lastOpeningPrimaryQuote=sri->tft.lastOpeningPrimaryQuote;
			establishingLocation=sri->establishingLocation;
			futureLocation=sri->futureLocation;
			agentLocationRelationSet=sri->agentLocationRelationSet;
			timeInfoSet=sri->timeInfoSet;
			isConstructedRelative=sri->isConstructedRelative;
			nextSPR=sri->nextSPR;
			whereSecondaryVerb=sourceMap[sri->whereSecondaryVerb];
			whereSecondaryObject=sourceMap[sri->whereSecondaryObject];
			whereNextSecondaryObject=sourceMap[sri->whereNextSecondaryObject];
			whereSecondaryPrep=sourceMap[sri->whereSecondaryPrep];
			whereQuestionType=sourceMap[sri->whereQuestionType];
			questionType=sri->questionType;
			sentenceNum=sri->sentenceNum;
			subQuery=sri->subQuery;
			skip=sri->skip;
		}
};

