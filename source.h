#pragma warning (disable: 4503)
#include "syntacticRelations.h"
#include "names.h"

class cSpaceRelation;
#define MAX_LEN 2048

typedef struct {
	string mainEntry;
	string wordType;
	vector <string> primarySynonyms;
	vector <string> accumulatedSynonyms, accumulatedAntonyms;
	vector <int> concepts;
	vector <string> rest;
} sDefinition;

// accumulateSemanticMaps
// accumulateSemanticEntry
// semanticCheck
class cSemanticMap
{
public:
	wstring SMPrincipalObject;
	set <wstring> sourcePaths;
	class cSemanticEntry
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
		bool subQueryNoMatch,tenseMismatch,confidenceCheck;
		Source *childSource;
		set <wstring> childSourcePaths;
		vector <wstring> relationSourcePaths;
		vector <int> relationWheres;
		wstring lastChildSourcePath;
		int childWhere2;
		int childObject;
		float score;
		// this is called from the parent
		int semanticCheck(cSpaceRelation* parentSRI,Source *parentSource);
		//void cSemanticMap::cSemanticEntry::printDirectRelations(int logType,Source *parentSource,wstring &path,int where);
		void printDirectRelations(int logType,Source *parentSource,wstring &path,int where);
		cSemanticEntry()
		{
			inSource=0;
			totalDistanceFromObject=0;
			directRelation=0;
			confidentInSource=0;
			confidentTotalDistanceFromObject=0;
			confidentDirectRelation=0;
			confidenceSE=0;
			childWhere2=0;
			childSource=0;
			score=0.0;
			semanticMismatch=0;
			subQueryNoMatch=false;
			tenseMismatch=false;
			confidenceCheck=false;
		}
		void lplogSM(int logType,wstring objectStr)
		{
			wstring tmpstr;
			::lplog(logType,L"SM object: %s: score=%f inSource=%d totalDistanceFromObject=%d directRelation=%d confidentInSource=%d confidentTotalDistanceFromObject=%d confidentDirectRelation=%d confidence=%d semanticMismatch=%d subQueryNoMatch=%s tenseMismatch=%s confidenceCheck=%s numSources=%d",
				  objectStr.c_str(),score,inSource,totalDistanceFromObject,directRelation,confidentInSource,confidentTotalDistanceFromObject,confidentDirectRelation,confidenceSE,
					semanticMismatch,(subQueryNoMatch) ? L"true":L"false", (tenseMismatch) ? L"true":L"false",(confidenceCheck) ? L"true":L"false",childSourcePaths.size());
		}
		void calculateScore()
		{
			int occurrence=(inSource+confidentInSource*2+directRelation*2+confidentDirectRelation*4);
			if (childSourcePaths.size()==1)
				occurrence>>=1;
			if (totalDistanceFromObject+confidentTotalDistanceFromObject)
				score=(float)((occurrence*occurrence)*1.0/(totalDistanceFromObject+confidentTotalDistanceFromObject));
		}
	};
	struct semanticSetCompare
	{
		bool operator()(unordered_map <wstring,cSemanticEntry>::iterator lhs, unordered_map <wstring,cSemanticEntry>::iterator rhs) const
		{
			if (lhs->second.confidentInSource+lhs->second.inSource==rhs->second.confidentInSource+rhs->second.inSource)
				return lhs->first<rhs->first;
			return lhs->second.confidentInSource+lhs->second.inSource>rhs->second.confidentInSource+rhs->second.inSource;
		}
	};
	struct semanticSetCompare2
	{
		bool operator()(unordered_map <wstring,cSemanticEntry>::iterator lhs, unordered_map <wstring,cSemanticEntry>::iterator rhs) const
		{
			return lhs->second.score>rhs->second.score;
		}
	};
	unordered_map <wstring,cSemanticEntry> relativeObjects;
	set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare> relativeObjectsSorted;
	set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare2> relativeObjectsSorted2;
	set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare > suggestedAnswers;
	void sortAndCheck(cSpaceRelation* parentSRI,Source *parentSource)
	{
		relativeObjectsSorted.clear();
		relativeObjectsSorted2.clear();
		for (unordered_map <wstring,cSemanticEntry>::iterator roi=relativeObjects.begin(),roiEnd=relativeObjects.end(); roi!=roiEnd; roi++)
		{
			roi->second.calculateScore();
			relativeObjectsSorted.insert(roi);
			relativeObjectsSorted2.insert(roi);
		}
		int onlyTopResults=0;
		for (set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare>::iterator sroi=relativeObjectsSorted.begin(),sroiEnd=relativeObjectsSorted.end(); sroi!=sroiEnd && onlyTopResults<20; sroi++)
		{
				onlyTopResults++;
				if ((*sroi)->second.semanticCheck(parentSRI,parentSource)<CONFIDENCE_NOMATCH)
					suggestedAnswers.insert((*sroi));
		}
		onlyTopResults=0;
		for (set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare2>::iterator sroi=relativeObjectsSorted2.begin(),sroiEnd=relativeObjectsSorted2.end(); sroi!=sroiEnd && onlyTopResults<20; sroi++)
		{
			onlyTopResults++;
			if ((*sroi)->second.semanticCheck(parentSRI,parentSource)<CONFIDENCE_NOMATCH)
				suggestedAnswers.insert((*sroi));
		}
	}
	void lplogSM(int logType,Source *parentSource,bool enhanced)
	{
		::lplog(logType,L"SM%s SEMANTIC MAP %d objects %d sources principalObject %s ****************************************************************************",
			(enhanced) ? L"E":L"",relativeObjects.size(),sourcePaths.size(),SMPrincipalObject.c_str());
		extern int logDetail;
		if (logDetail)
			for (set <wstring>::iterator spi=sourcePaths.begin(),spiEnd=sourcePaths.end(); spi!=spiEnd; spi++)
				::lplog(logType,L"SM%s sourcePath: %s",(enhanced) ? L"E":L"",spi->c_str());
		int onlyTopResults=0;
		::lplog(logType,L"SM%s by frequency ***************",(enhanced) ? L"E":L"");
		for (set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare>::iterator sroi=relativeObjectsSorted.begin(),sroiEnd=relativeObjectsSorted.end(); sroi!=sroiEnd && onlyTopResults<20; sroi++)
		{
			onlyTopResults++;
			(*sroi)->second.lplogSM(logType,(*sroi)->first);
		}
		::lplog(logType,L"SM%s by score ***************",(enhanced) ? L"E":L"");
		onlyTopResults=0;
		for (set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare>::iterator sroi=relativeObjectsSorted2.begin(),sroiEnd=relativeObjectsSorted2.end(); sroi!=sroiEnd && onlyTopResults<20; sroi++)
		{
			onlyTopResults++;
			(*sroi)->second.lplogSM(logType,(*sroi)->first);
		}
		if (suggestedAnswers.empty())
			::lplog(logType,L"SM%s no suggested answers.",(enhanced) ? L"E":L"");
		else
		{
			::lplog(logType,L"SM%s suggested answers ***************",(enhanced) ? L"E":L"");
			for (set < unordered_map <wstring,cSemanticEntry>::iterator,semanticSetCompare >::iterator sai=suggestedAnswers.begin(),saiEnd=suggestedAnswers.end(); sai!=saiEnd; sai++)
			{
				relativeObjects[(*sai)->first].lplogSM(LOG_WHERE,(*sai)->first);
				if (logSemanticMap)
					for (unsigned int I=0; I<(*sai)->second.relationSourcePaths.size(); I++)
						(*sai)->second.printDirectRelations(logType,parentSource,(*sai)->second.relationSourcePaths[I],(*sai)->second.relationWheres[I]);
			}
		}
		::lplog(logType,L"SM%s END SEMANTIC MAP %d objects %d sources principalObject %s ****************************************************************************",
			(enhanced) ? L"E":L"",relativeObjects.size(),sourcePaths.size(),SMPrincipalObject.c_str());
	}
};

#include "semanticRelations.h"

#define MINIMUM_SALIENCE_WITH_MATCHED_ADJECTIVES -2500 // 2500 makes Irish Sinn Feiner work!
#define MORE_SALIENCE_PER_ADJECTIVE 0 // -100 // This only changed things slightly for the worse - 22695: The new-comer
#define DISALLOW_SALIENCE 200000

#define ILLEGAL_PATH_CHARS "\\/:*?\"<>|"
#define WCHAR_ILLEGAL_PATH_CHARS L"\\/:*?\"<>|"
#define IS_SALIENCE_BOOST 2000

void escapeStr(wstring &str);
unsigned long encodeEscape(MYSQL &mysql, wstring &to, wstring from);

class cOM
{
public:
	int object;
	int salienceFactor;
	cOM(int o,int sf) { object=o; salienceFactor=sf; };
	cOM(char *buffer,int &where,int limit)
	{
		if (!copy(object,buffer,where,limit)) return;
		if (!copy(salienceFactor,buffer,where,limit)) return;
	}
	bool write(void *buffer,int &where,int limit)
	{
		if (!copy(buffer,object,where,limit)) return false;
		if (!copy(buffer,salienceFactor,where,limit)) return false;
		return true;
	}
	cOM(void) { object=-1; salienceFactor=0; };
	bool operator == (const cOM& om)
	{  return object==om.object && salienceFactor==om.salienceFactor;  }
	bool operator != (const cOM& om)
	{  return object!=om.object; }
};

enum RENUM { R_SimplePresent=1,R_SimplePast=2,R_SimpleFuture=3,R_PresentPerfect=4,R_PastPerfect=5,R_FuturePerfect=6 };

class WordMatch
{
public:
	static const unsigned __int64 flagConstant=((unsigned __int64)1<<58);
	static const unsigned __int64 flagTransitory=((unsigned __int64)1<<57);
	static const unsigned __int64 flagNewLineBeforeHint=((unsigned __int64)1<<56);
	static const unsigned __int64 flagAlreadyTimeAnalyzed=((unsigned __int64)1<<55);
	static const unsigned __int64 flagVAnalysisVNOnly=((unsigned __int64)1<<54);
	static const unsigned __int64 flagUsedPossessionRelation=((unsigned __int64)1<<53);
	static const unsigned __int64 flagUsedBeRelation=((unsigned __int64)1<<52);
	static const unsigned __int64 flagInLingeringStatement=((unsigned __int64)1<<51);
	static const unsigned __int64 flagRelativeObject=((unsigned __int64)1<<50); 
	static const unsigned __int64 flagInInfinitivePhrase=((unsigned __int64)1<<49);
	static const unsigned __int64 flagResolvedByRecent=((unsigned __int64)1<<48); 
	static const unsigned __int64 flagLastContinuousQuote=((unsigned __int64)1<<47); 
	static const unsigned __int64 flagAlternateResolveForwardFromLastSubjectAudience=((unsigned __int64)1<<46); 
	static const unsigned __int64 flagAlternateResolveForwardFromLastSubjectSpeakers=((unsigned __int64)1<<45); 
	static const unsigned __int64 flagGenderIsAmbiguousResolveAudience=((unsigned __int64)1<<44); // when a definitely specified audience is a gendered pronoun with more than one speaker in the current speaker group having that gender
	static const unsigned __int64 flagRelativeHead=((unsigned __int64)1<<43); 
	static const unsigned __int64 flagQuoteContainsSpeaker=((unsigned __int64)1<<42); 
	static const unsigned __int64 flagResolveMetaGroupByGender=((unsigned __int64)1<<41); 
	static const unsigned __int64 flagEmbeddedStoryResolveSpeakersGap=((unsigned __int64)1<<40); 
	static const unsigned __int64 flagEmbeddedStoryBeginResolveSpeakers=((unsigned __int64)1<<39); // only set if aged
	static const unsigned __int64 flagAge=((unsigned __int64)1<<38); // only set if aged
	// set dynamically? 
	static const unsigned __int64 flagVAnalysis=((unsigned __int64)1<<37);
	static const unsigned __int64 flagTempVNReAnalysis=((unsigned __int64)1<<36);
	static const unsigned __int64 flagInPStatement=((unsigned __int64)1<<35);
	static const unsigned __int64 flagInQuestion=((unsigned __int64)1<<34);
	static const unsigned __int64 flagRelationsAlreadyEvaluated=((unsigned __int64)1<<32);
	static const unsigned __int64 flagNounOwner=((unsigned __int64)1<<31);
	enum { 
		// form flags
		flagAddProperNoun=(1<<30), flagOnlyConsiderProperNounForms=(1<<29),
		flagAllCaps=(1<<28), flagTopLevelPattern=(1<<27), flagPossiblePluralNounOwner=(1<<26),
		flagNotMatched=(1<<25),flagInsertedQuote=(1<<24),flagFirstLetterCapitalized=(1<<23),
		flagRefuseProperNoun=(1<<22), flagMetaData=(1<<21),
		flagOnlyConsiderOtherNounForms=(1<<20),
		// BNC patternPreferences (only used for nonQuotes)
		flagBNCPreferNounPatternMatch=(1<<19),         flagBNCPreferVerbPatternMatch=(1<<18),
		flagBNCPreferAdjectivePatternMatch=(1<<17),    flagBNCPreferAdverbPatternMatch=(1<<16),
		flagBNCPreferIgnore=(1<<15),                   flagBNCFormNotCertain=(1<<14), // tag was not set on word (because a tag was set for multiple words)

		// speaker resolution (only used for quotes)
		flagFromPreviousHailResolveSpeakers=(1<<23),
		flagAlphaBeforeHint=(1<<22),
		flagAlphaAfterHint=(1<<21),
		flagAlternateResolutionFinishedSpeakers=(1<<20),
		flagForwardLinkResolveAudience=(1<<19),
		flagForwardLinkResolveSpeakers=(1<<18),
		flagFromLastDefiniteResolveAudience=(1<<17),
		flagDefiniteResolveSpeakers=(1<<16),
		flagMostLikelyResolveAudience=(1<<15),
		flagMostLikelyResolveSpeakers=(1<<14),
		flagSpecifiedResolveAudience=(1<<13),
		flagAudienceFromSpeakerGroupResolveAudience=(1<<12),
		flagHailedResolveAudience=(1<<11),
		// if one speaker is known (flagDefiniteResolveSpeakers) then resolve others around it by assuming the speakers alternate
		flagAlternateResolveBackwardFromDefiniteAudience=(1<<10), // resolves unresolved speakers backwards from a known definite speaker
		flagAlternateResolveBackwardFromDefiniteSpeakers=(1<<9), // resolves unresolved speakers backwards from a known definite speaker
		flagForwardLinkAlternateResolveAudience=(1<<8),
		flagForwardLinkAlternateResolveSpeakers=(1<<7), 
		flagEmbeddedStoryResolveSpeakers=(1<<6), 
		flagAlternateResolveForwardAfterDefinite=(1<<5), // this flag is set for all alternative settings forward after a definite speaker
								// previously results from imposeMostLikely were only set with flagMostLikelyResolveSpeakers
								// flagAlternateResolveForwardAfterDefinite allows an alternate speaker imposed from below and an alternate speaker
								// imposed from above to stop running over each other
		// 1<<4 (flagObjectResolved) is zeroed for all positions after identifySpeakerGroups
		flagGenderIsAmbiguousResolveSpeakers=(1<<3), // when a definitely specified speaker is a gendered pronoun with more than one speaker in the current speaker group having that gender
		flagQuotedString=(1<<2), 
		flagSecondEmbeddedStory=(1<<1),
		flagFirstEmbeddedStory=(1<<0),
		// only for non-quotes
		flagObjectResolved=(1<<4), 
		flagAdjectivalObject=(1<<3), 
		flagUnresolvableObjectResolvedThroughSpeakerGroup=(1<<2), 
		flagObjectPleonastic=(1<<1), 
		flagIgnoreAsSpeaker=(1<<0), 
		};
	// flags considered more reliable:
#define moreReliableMatchedFlags (WordMatch::flagDefiniteResolveSpeakers|WordMatch::flagAudienceFromSpeakerGroupResolveAudience|WordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers|WordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers|WordMatch::flagEmbeddedStoryResolveSpeakers)
#define moreReliableNotMatchedFlags (WordMatch::flagFromLastDefiniteResolveAudience|WordMatch::flagSpecifiedResolveAudience|WordMatch::flagHailedResolveAudience|WordMatch::flagAlternateResolveBackwardFromDefiniteAudience|WordMatch::flagAlternateResolveForwardFromLastSubjectAudience)
	void setForm(void);
	void setPreferredForm(void);
	bool costable(void);
	WordMatch(tIWMM inWord,unsigned __int64 inAdjustedForms, sTrace &trace)
	{
		word=inWord;
		flags=inAdjustedForms;
		minAvgCostAfterAssessCost=1000000;
		maxLACAACMatch=0;
		maxMatch=0;
		maxLACMatch=0;
		lowestAverageCost=1000000;
		object=principalWherePosition=principalWhereAdjectivalPosition=originalObject=-1;
		beginObjectPosition=endObjectPosition=-1;
		relPrep=relObject=relSubject=relVerb=relNextObject=nextCompoundPartObject=previousCompoundPartObject=relInternalVerb=relInternalObject=-1;
		quoteForwardLink=-1;
		quoteBackLink=-1;
		embeddedStorySpeakerPosition=-1;
		audiencePosition=-1;
		speakerPosition=-1;
		skipResponse=-1;
		endQuote=-1;
		nextQuote=-1;
		previousQuote=-1;
		objectRole=0;
		verbSense=0;
		beginPEMAPosition=-1;
		endPEMAPosition=-1;
		PEMACount=0;
		timeColor=0;
		tmpWinnerForms=0;
		spaceRelation=false;
		andChainType=false;
		notFreePrep=false;
		hasVerbRelations=false;
		t.printBeforeElimination=trace.printBeforeElimination;
		t.traceSubjectVerbAgreement=trace.traceSubjectVerbAgreement;
		t.traceTestSubjectVerbAgreement = trace.traceTestSubjectVerbAgreement;
		t.traceEVALObjects=trace.traceEVALObjects;
		t.traceAnaphors=trace.traceAnaphors;
		t.traceRelations=trace.traceRelations;
		t.traceRole=trace.traceRole;
		t.traceSpeakerResolution=trace.traceSpeakerResolution;
		t.traceObjectResolution=trace.traceObjectResolution;
		t.traceNameResolution=trace.traceNameResolution;
		t.traceVerbObjects=trace.traceVerbObjects;
		t.traceDeterminer=trace.traceDeterminer;
		t.traceBNCPreferences=trace.traceBNCPreferences;
		t.tracePatternElimination=trace.tracePatternElimination;
		t.traceSecondaryPEMACosting=trace.traceSecondaryPEMACosting;
		t.traceIncludesPEMAIndex=trace.traceIncludesPEMAIndex;
		t.traceMatchedSentences=trace.traceMatchedSentences;
		t.traceUnmatchedSentences=trace.traceUnmatchedSentences;
		t.traceTagSetCollection=trace.traceTagSetCollection;
		t.traceTransitoryQuestion=trace.traceTransitoryQuestion;
		t.traceConstantQuestion=trace.traceConstantQuestion;
		t.traceMapQuestion=trace.traceMapQuestion;
		t.traceCommonQuestion=trace.traceCommonQuestion;
		t.traceQuestionPatternMap=trace.traceQuestionPatternMap;
		t.collectPerSentenceStats=trace.collectPerSentenceStats;
		logCache=::logCache;
	};
	tIWMM word;  // points to WMM array
	tIWMM getMainEntry(void)
	{
		return (word->second.mainEntry==wNULL) ? word : word->second.mainEntry;
	}
	tIWMM deriveMainEntry(int where,int fromWhere,bool isVerb,bool isNoun,wstring &lastNounNotFound,wstring &lastVerbNotFound)
	{
		if (!isVerb && !isNoun) return getMainEntry();
		int inflectionFlags=word->second.inflectionFlags;
		if ((isVerb && !(inflectionFlags&VERB_PRESENT_FIRST_SINGULAR)) || 
			  (isNoun && !(inflectionFlags&SINGULAR)))
		{
			if (word->second.mainEntry!=wNULL)
			{
				tIWMM me=word->second.mainEntry;
				if ((isVerb && (me->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR)) ||
					  (isNoun && (me->second.inflectionFlags&SINGULAR)))
					return me;
			}
			if (word->first.length()==1) return word;
			if (word->second.query(numeralCardinalForm)>=0 || word->second.query(NUMBER_FORM_NUM)>=0) return word;
			if (word->second.query(nomForm)>=0 || word->second.query(accForm)>=0 || word->second.query(personalPronounForm)>=0) return word;
			if (word->second.query(possessiveDeterminerForm)>=0 || word->second.query(possessivePronounForm)>=0 || word->second.query(reflexiveForm)>=0) return word;
			if (isNoun && word->second.query(nounForm)<0) return word;
			wstring in=word->first;
		::deriveMainEntry(where,fromWhere,in,inflectionFlags,isVerb,isNoun,lastNounNotFound,lastVerbNotFound);
		return Words.query(in);
		}
		else
			return word;
	}
	tIWMM getNounME(int where,int fromWhere,wstring &lastNounNotFound,wstring &lastVerbNotFound)
	{
		if (!(word->second.inflectionFlags&SINGULAR))
		{
			if (word->second.mainEntry!=wNULL)
			{
				tIWMM me=word->second.mainEntry;
				if (me->second.inflectionFlags&SINGULAR)
					return me;
			}
			if (word->first.length()==1) return word;
			if (word->second.query(numeralCardinalForm)>=0 || word->second.query(NUMBER_FORM_NUM)>=0) return word;
			if (word->second.query(nomForm)>=0 || word->second.query(accForm)>=0 || word->second.query(personalPronounForm)>=0) return word;
			if (word->second.query(possessiveDeterminerForm)>=0 || word->second.query(possessivePronounForm)>=0 || word->second.query(reflexiveForm)>=0) return word;
			if (word->second.query(nounForm)<0) return word;
			wstring in=word->first;
			int inflectionFlags=word->second.inflectionFlags; // avoid altering original inflectionFlags
			::deriveMainEntry(where,fromWhere,in,inflectionFlags,false,true,lastNounNotFound,lastVerbNotFound);
			return Words.query(in);
		}
		else
			return word;
	}
	tIWMM getVerbME(int where,int fromWhere,wstring &lastNounNotFound,wstring &lastVerbNotFound)
	{
		if (!(word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR))
		{
			if (word->second.mainEntry!=wNULL)
			{
				tIWMM me=word->second.mainEntry;
				if (me->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR)
					return me;
			}
			if (word->first.length()==1) return word;
			wstring in=word->first;
			int inflectionFlags=word->second.inflectionFlags; // avoid altering original inflectionFlags
			::deriveMainEntry(where,fromWhere,in,inflectionFlags,true,false,lastNounNotFound,lastVerbNotFound);
			return Words.query(in);
		}
		else
			return word;
	}
	unsigned __int64 flags;
	unsigned short maxMatch;
	int minAvgCostAfterAssessCost;
	int lowestAverageCost;

	// used in HMM Viterbi training and testing
	int originalPreferredViterbiForm;
	vector <int> preferredViterbiForms; // gives form # relative to Forms, not form offset relative to this word.
	double preferredViterbiProbability;
	double preferredViterbiMaximumProbability;
	int preferredViterbiPreviousTagOfHighestProbability;
	int preferredViterbiCurrentTagOfHighestProbability;

	unsigned short maxLACMatch;
	unsigned short maxLACAACMatch;
	unsigned __int64 objectRole;
	char verbSense;
	unsigned char timeColor;
	int beginPEMAPosition;
	int endPEMAPosition;
	unsigned int PEMACount;
	patternMatchArray pma;
	bitObject forms;
	//bitObject winnerForms; // used for BNC to remember tagged form
	bitObject patterns;

	// the man's shoes
	//  0   1     2
	// principalWherePosition at 0 = "the man's shoes"
	// object at 2 = "the man's shoes"
	// principalWhereAdjectivalPosition at 1 = "man"
	// object at 1 = "man"
	// principalWhere "the man's shoes"=2

	// the Dover Street Tube exit
	// 0   1     2      3    4
	// principalWherePosition at 0 = "The Dover Street Tube exit"
	// object at 4 = "The Dover Street Tube exit"
	// principalWhereAdjectivalPosition at 1 = "Dover Street Tube"
	// object at 3 = "Dover Street Tube"
	// principalWhere "the Dover Street Tube exit"=4
	// principalWhere "Dover Street Tube"=3

	// her socks
	// 0   1
	// principalWherePosition at 0 = "her socks"
	// object at 1 = "her socks"
	// principalWhere of "her socks"=1

	// Ben's socks
	// 0     1
	// principalWherePosition at 0 = "Ben's socks"
	// object at 0 = "Ben"
	// principalWhereAdjectivalPosition at 0 = "Ben"
	// object at 1 = "Ben's socks"
	// principalWhere of object "Ben's socks"=1
	int principalWherePosition; // principalWherePosition is set at the beginning location of an object - its value is the principalWhere of the object
	int principalWhereAdjectivalPosition; // principalWhereAdjectivalPosition is at the beginning of an object used as an adjective.
	int originalObject; // object that has been replaced by another object but that reflects the object originally found at this position
	// a position may be both subject and object
	int relNextObject; // if this is a subject, what is the next subject in the same sentence?  If object, what is the next object in the same sentence? (for pronoun disambiguation)
	                   // if this is an object of a preposition, this is equal to the immediately previous object position
	int nextCompoundPartObject; // subject or object, links compound objects together
	int previousCompoundPartObject; // subject or object, links compound objects together : also links an infinitive verb back to the mainVerb
	int relSubject; // if this is an object, what subject does it relate to? (for pronoun disambiguation)
	int relVerb; // for subjects and objects
	int relPrep; // subjects, objects and verbs should have this set to the prepositions of the prep phrases, 
							 // and prep phrases themselves have them set to the next prep phrase in the sentence
	int setRelPrep(int rp) { 
		relPrep=rp; 
		return rp;
	}
	// I passed two Johnnies in the street talking about ... / passed is 'two Johnnies' relVerb, relInternalVerb is set to 'talking'
	int relInternalVerb; // he wanted you to come to the dance.  relVerb of you is 'wanted' relInternalVerb of you is 'come'
	// relInternalObject is also used for another object of the same subject location but the object is of another verb.  This verb may be before the subject (_INTRO_S1)
	int relInternalObject; // I guess you have no right to ... relInternalObject of I is 'you' - this is to prevent LL routines 
	int beginObjectPosition,endObjectPosition; // where is the object defined on this position begin and end?
	vector <cOM> objectMatches;
	vector <cOM> audienceObjectMatches;
	// the next field is also used to store tsSense for verbs
	int quoteForwardLink; // next quote in same paragraph (or tsSense)
	int quoteBackLink; // previous quote in same paragraph 
	int nextQuote; // set at the beginning of the quote to the beginning of the next quote in a separate paragraph
	                    // if the word is a preposition, this points to the head of the preposition phrase
	                    //  Krugman earned his B.A. in economics from Yale University summa cum laude in 1974 and his PhD from the Massachusetts Institute of Technology (MIT) in 1977.
	                    // the head of 'in' economics,'from' Yale,'in' 1974 = his 'B.A.'
	                    // the head of 'from' Institute, 'in' 1977 = his 'PhD'
	int previousQuote; // set at the beginning of the quote to the beginning of the previous quote in a separate paragraph
	int endQuote; // matching end quote
	int tmpWinnerForms; 
	int skipResponse; // not saved
	int embeddedStorySpeakerPosition; // tracks who is speaking when they are relating an event that happened in the past
	int audiencePosition,speakerPosition;
	sTrace t;
	bool spaceRelation,hasVerbRelations,andChainType,notFreePrep;
	short logCache;
	wstring baseVerb;
	wstring questionTransformationSuggestedPattern;

	void setWinner(int form)
	{
		if (form>=sizeof(tmpWinnerForms)*8)
			lplog(LOG_FATAL_ERROR,L"overFlow on tmpWinnerForms (1)!");
		tmpWinnerForms|=(1<<form);
	}
	bool setPreferredViterbiForm(wstring form,double probability)
	{
		if (form == L"--s--")
		{
			preferredViterbiForms.push_back(-2);
			return true;
		}
		else
		{
			int preferredViterbiForm = queryForm(form);
			if (preferredViterbiForm < 0)
			{
				lplog(LOG_ERROR, L"Form %s not found as one of the forms for word %s in Viterbi processing.", form.c_str(), word->first.c_str());
				return false;
			}
			else
			{
				preferredViterbiForms.push_back(preferredViterbiForm);
				preferredViterbiProbability = probability;
			}
			return true;
		}
	}
	bool testPreferredViterbiForm(wstring form)
	{
		if (form != L"--s--" && queryForm(form) < 0)
		{
			lplog(LOG_ERROR, L"*Interim form %s not found as one of the forms for word %s in Viterbi processing.", form.c_str(), word->first.c_str());
			return false;
		}
		return true;
	}
	void setSeparatorWinner(void);
	bool maxWinner(int len,int avgCost,int lowestSeparatorCost);
	// if there is no winner, display every form
	bool isWinner(int form)
	{
		if (form>=sizeof(tmpWinnerForms)*8)
		{
			lplog(LOG_ERROR,L"overFlow on tmpWinnerForms form=%d tmpWinnerForms=%d (2)!",forms,tmpWinnerForms);
			return false;
		}
		return (tmpWinnerForms) ? ((1<<form)&tmpWinnerForms)!=0 : true;
	}
	bool isOnlyWinner(int form)
	{
		if (!tmpWinnerForms) return formsSize()==1;
		return (1<<form)==tmpWinnerForms;
	}
	wstring roleString(wstring &sRole);
	void logRelations(int where)
	{
		wstring rs;
		if (relNextObject==-1 && nextCompoundPartObject==-1 && previousCompoundPartObject==-1 && relInternalObject==-1 && 
				relSubject==-1 && relVerb==-1 && relObject==-1 && relPrep==-1 && relInternalVerb==-1)
		{
			if (objectRole)
				lplog(LOG_RESOLUTION,L"%06d:role=(%s)",where,roleString(rs).c_str());
			return;
		}
		if (t.traceSpeakerResolution)
		{
			wstring rs2;
			if (relNextObject==-1 && nextCompoundPartObject==-1 && previousCompoundPartObject==-1 && relInternalObject==-1)
				lplog(LOG_RESOLUTION,L"%06d:role=(%s) relSubject=%d,relVerb=%d,relObject=%d,relPrep=%d,relInternalVerb=%d",
					where,roleString(rs2).c_str(),relSubject,relVerb,relObject,relPrep,relInternalVerb);
			else
				lplog(LOG_RESOLUTION,L"%06d:role=(%s) relSubject=%d,relVerb=%d,relObject=%d,relNextObject=%d,nextCompoundPartObject=%d,previousCompoundPartObject=%d,relPrep=%d,relInternalVerb=%d,relInternalObject=%d",
					where,roleString(rs2).c_str(),relSubject,relVerb,relObject,relNextObject,nextCompoundPartObject,previousCompoundPartObject,relPrep,relInternalVerb,relInternalObject);
		}
	}
	bool isPPN(void);
	tIWMM resolveToClass();
	// in the case of a proper noun, it increases the size of the form by 1
	vector <int> getForms();
	int queryForm(int form);
	int queryForm(wstring sForm);
	int queryWinnerForm(int form);
	int queryWinnerForm(wstring sForm);
	wstring patternWinnerFormString(wstring &forms);
	wstring winnerFormString(wstring &formsString,bool withCost=true);
	void getWinnerForms(vector <int> &winnerForms);
	int getNumWinners();
	bool hasWinnerVerbForm(void);
	bool isWinnerSeparator(void);
	bool isPhysicalObject(void);
	bool isTopLevel(void);
	bool isNounType(void);
	bool isModifierType(void);
	unsigned int formsSize(void);
	unsigned int getFormNum(unsigned int line);
	bool updateMaxMatch(int len,int avgCost);
	bool updateMaxMatch(int len,int avgCost,int lowerAvgCost);
	bool compareCost(int AC1,int LEN1,int lowestSeparatorCost,bool alsoSet);
	unsigned int getShortFormInflectionEntry(int line,wchar_t *entry);
	unsigned int getShortAllFormAndInflectionLen(void);
	int getInflectionLength(int inflection,tInflectionMap *map);
	bool isGendered(void);
	bool isPossessivelyGendered(bool &possessivePronoun);
	bool updateFormUsagePatterns(void);
	void logFormUsageCosts(void);
	int getObject() { return object; }
	void setObject(int o)
	{
		object=o;
	}
	int getRelObject() { return relObject; }
	int setRelObject(int ro)
	{
		return relObject=ro;
	}
	bool writeRef(void *buffer,int &where,int limit);
	bool read(char *buffer,int &where,int limit);
	void accumulateStatistics(unordered_map<wstring, int> &defaultMap);
	WordMatch(char *buffer,int &where,int limit,bool &error)
	{
		error=!read(buffer,where,limit);
	}
	WordMatch(void)
	{
	}
private:
	int object; // this is an index into the objects array.  it is set at the principalWhere of an object
	int relObject; // if this is a subject, what object does it relate to? (for pronoun disambiguation) - also used if speaker explicitly names another speaker (flagQuoteContainsSpeaker)
};
extern vector <WordMatch>::iterator wmNULL;

class cLocalFocus
{
public:
	cOM om; // index into objects also a salience factor
	int numEncounters;
	int numIdentifiedAsSpeaker;
	int numDefinitelyIdentifiedAsSpeaker;
	int lastRoleSalience;
	int lastWhere;
	int previousWhere;
	int numMatchedAdjectives;
	int newPPAge; // the age of a new physically present entity - used with introductions
	int lastExit; // position in source of last exit of object - used in determining whether the exit was true
	int lastEntrance; // position in source of last explicit entrance of object 
	int whereBecamePhysicallyPresent; // position where the object last became physically present.
	bool notSpeaker;
	bool lastSubject;
	bool occurredInPrimaryQuote;
	bool occurredOutsidePrimaryQuote;
	bool occurredInSecondaryQuote;
	bool physicallyPresent;
	wstring res;
	void init(bool inPrimaryQuote,bool inSecondaryQuote)
	{
		unquotedAge=(inPrimaryQuote || inSecondaryQuote) ? -1:0;
		quotedAge=(!inPrimaryQuote && !inSecondaryQuote) ? -1:0;
		totalAge=0;
		totalPreviousAge=unquotedPreviousAge=quotedPreviousAge=-1;
		numEncounters=0;
		numIdentifiedAsSpeaker=0;
		numDefinitelyIdentifiedAsSpeaker=0;
		lastWhere=previousWhere=-1;
		lastRoleSalience=0;
		numMatchedAdjectives=0;
		lastExit=lastEntrance=-1;
		whereBecamePhysicallyPresent=-1;
		occurredInPrimaryQuote=inPrimaryQuote || inSecondaryQuote;
		occurredOutsidePrimaryQuote=!inPrimaryQuote && !inSecondaryQuote;
		occurredInSecondaryQuote=inSecondaryQuote;
		notSpeaker=false;
		lastSubject=false;
		physicallyPresent=false;
	};
	cLocalFocus(bool inPrimaryQuote,bool inSecondaryQuote)
	{
		init(inPrimaryQuote,inSecondaryQuote);
	}
	cLocalFocus(cOM lom,bool inPrimaryQuote,bool inSecondaryQuote,bool ns,bool pp)
	{
		init(inPrimaryQuote,inSecondaryQuote);
		om=lom;
		notSpeaker=ns;
		physicallyPresent=pp;
	};
	cLocalFocus(int object,bool inPrimaryQuote,bool inSecondaryQuote,bool ns,bool pp)
	{
		init(inPrimaryQuote,inSecondaryQuote);
		om.object=object;
		notSpeaker=ns;
		physicallyPresent=pp;
	};
	static bool salienceInQuote(bool objectToBeMatchedInQuote)
	{
		return objectToBeMatchedInQuote;
	}
	static bool salienceIndependent(bool quoteIndependentAge)
	{
		return quoteIndependentAge;
	}
	// this factor is set once for each object that is resolved.
	//static bool quoteIndependentAge,objectToBeMatchedInQuote;
	static bool setSalienceAgeMethod(bool inObjectToBeMatchedInQuote,bool objectToBeMatchedIsOnlyNeuter,bool &objectToBeMatchedInQuote,bool &quoteIndependentAge)
	{
		objectToBeMatchedInQuote=inObjectToBeMatchedInQuote;
		return quoteIndependentAge=objectToBeMatchedIsOnlyNeuter;
	}
	bool includeInSalience(bool objectToBeMatchedInQuote,bool quoteIndependentAge)
	{
		return occurredInPrimaryQuote==objectToBeMatchedInQuote ||
					 occurredOutsidePrimaryQuote!=objectToBeMatchedInQuote ||
					 // never bring speakers into focus inside quotes if they haven't been introduced inside them (in which case occurredInPrimaryQuote would be true)
					 // this is because speakers will easily win against any objects in quotes, when the inQuote objects need just one more boost.
					 // also speakers are unlikely to refer to themselves in the third person.
					 (quoteIndependentAge && ((!numIdentifiedAsSpeaker && !numDefinitelyIdentifiedAsSpeaker) || !physicallyPresent)); 
					 //quoteIndependentAge; 
	}
	void setInSalience(void)
	{
		occurredInPrimaryQuote=occurredOutsidePrimaryQuote=true;
	}
	void clear(void)
	{
		res.clear();
		numMatchedAdjectives=0;
		om.salienceFactor=-1;
	}
	int rAge(bool previous,bool speaker,bool objectToBeMatchedInQuote,bool quoteIndependentAge);
	int allAge(bool speaker,bool objectToBeMatchedInQuote,bool quoteIndependentAge);
	int getAge(bool previous,bool objectToBeMatchedInQuote,bool quoteIndependentAge);
	// used during salience to retry if no objects match
	void decreaseAge(bool objectToBeMatchedInQuote,bool quoteIndependentAge);
	void increaseAge(bool sentenceInPrimaryQuote,bool sentenceInSecondaryQuote,int amount);
	void increaseAge(int amount);
	// reset an object that was seen previously
	void resetAge(bool objectToBeMatchedInQuote);
	// reset a speaker at the beginning of a section
	void resetAgeBeginSection(bool isObserver);
	void saveAge(void);
	void restoreAge(void);
	int getQuotedAge(void) { return quotedAge; }
	int getQuotedPreviousAge(void) { return quotedPreviousAge; }
	int getUnquotedAge(void) { return unquotedAge; }
	int getUnquotedPreviousAge(void) { return unquotedPreviousAge; }
	int getTotalAge(void) { return totalAge; }
	int getTotalPreviousAge(void) { return totalPreviousAge; }

private:
	int unquotedAge; // age from last encounter outside quotes counting only sentences outside quotes
	int quotedAge; // age from last encounter inside quotes counting only sentences inside quotes
	int totalAge; // age from last encounter anywhere counting all sentences
	int unquotedPreviousAge,quotedPreviousAge,totalPreviousAge;

	int saveUnquotedAge; // age from last encounter outside quotes counting only sentences outside quotes
	int saveQuotedAge; // age from last encounter inside quotes counting only sentences inside quotes
	int saveTotalAge; // age from last encounter anywhere counting all sentences
	int saveUnquotedPreviousAge,saveQuotedPreviousAge,saveTotalPreviousAge;
};

extern vector <cLocalFocus>::iterator cNULL;

// include an animate flag?
enum OC {
	PRONOUN_OBJECT_CLASS=1,
	REFLEXIVE_PRONOUN_OBJECT_CLASS=2,
	RECIPROCAL_PRONOUN_OBJECT_CLASS=3,
	NAME_OBJECT_CLASS=4,
	GENDERED_GENERAL_OBJECT_CLASS=5,
	BODY_OBJECT_CLASS=6,
	GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS=7, // occupation=plumber;role=leader;activity=runner
	GENDERED_DEMONYM_OBJECT_CLASS=8,
	NON_GENDERED_GENERAL_OBJECT_CLASS=9,
	NON_GENDERED_BUSINESS_OBJECT_CLASS=10,
	PLEONASTIC_OBJECT_CLASS=11,
	//DEICTIC_OBJECT_CLASS=7, not necessary
	VERB_OBJECT_CLASS=12, // the news I brought / running -- objects that contain verbs
	NON_GENDERED_NAME_OBJECT_CLASS=13,
	META_GROUP_OBJECT_CLASS=14,
	GENDERED_RELATIVE_OBJECT_CLASS=15
};
// these first types must be kept in order with the list in defineWordsOfMultiWordObjects
enum OCSubType {
	CANADIAN_PROVINCE_CITY,
	COUNTRY,
	ISLAND,
	MOUNTAIN_RANGE_PEAK_LANDFORM,
	OCEAN_SEA,
	PARK_MONUMENT,
	REGION,
	RIVER_LAKE_WATERWAY,
	US_CITY_TOWN_VILLAGE,
	US_STATE_TERRITORY_REGION,
	WORLD_CITY_TOWN_VILLAGE,
	GEOGRAPHICAL_NATURAL_FEATURE, // lake mountain stream river pool air sea land space water
	GEOGRAPHICAL_URBAN_FEATURE, // city, town, suburbs, park, dam, buildings - definitely a different location
	GEOGRAPHICAL_URBAN_SUBFEATURE, // rooms within buildings - perhaps a different location (see spaceRelations)
	GEOGRAPHICAL_URBAN_SUBSUBFEATURE, // commonly interacted things within rooms - door, table, chair, desk - not a different location
	TRAVEL, // trip, journey, road, street, trail
	MOVING, // train, plane, automobile
	MOVING_NATURAL, // elephant, horse, _NAME (human)
	RELATIVE_DIRECTION, // front, rear, side, edge, behind
	ABSOLUTE_DIRECTION, // North, South, East, West
	BY_ACTIVITY, // lunch, dinner, breakfast, party, wedding
	UNKNOWN_PLACE_SUBTYPE,
	NUM_SUBTYPES,
	NOT_A_PLACE=-2
};
extern wchar_t *OCSubTypeStrings[];

class cLastVerbTenses
{
public:
	int lastVerb; // book position of main verb
	int lastTense; // tense of the entire verb
	cLastVerbTenses()
	{
		clear();
	};
	void clear()
	{
		lastVerb=-1;
		lastTense=-1;
	}
};

bool copy(void *buf,cLastVerbTenses &s,int &where,int limit);
bool copy(cLastVerbTenses &a,void *buf,int &where,int limit);
bool copy(void *buf,cName &a,int &where,int limit);
bool copy(cName &a,void *buf,int &where,int limit);

// locations start at 0 for each line
// the old man       (GENDERED_GENERAL_OBJECT_CLASS     begin=0 end=3 at=2 adjectival=false ownerGendered=false)
// Bill's wedding    (NAME_OBJECT_CLASS                 begin=0 end=1 at=0 adjectival=true  ownerGendered=false)
//                   (NON_GENDERED_GENERAL_OBJECT_CLASS begin=0 end=2 at=1 adjectival=false ownerGendered=true)
// my old father     (PRONOUN_OBJECT_CLASS              begin=0 end=1 at=0 adjectival=true  ownerGendered=false)
//                   (GENDERED_GENERAL_OBJECT_CLASS     begin=0 end=3 at=2 adjectival=false ownerGendered=false)
extern wchar_t *wordOrderWords[];
class cObject
{
public:
	int index;
	enum OC objectClass;
private:
	int subType;
public:
	int begin,end,originalLocation;
	int PMAElement;
	int numEncounters;
	int numIdentifiedAsSpeaker;
	int numDefinitelyIdentifiedAsSpeaker;
	int numEncountersInSection;
	int numSpokenAboutInSection;
	int numIdentifiedAsSpeakerInSection;
	int numDefinitelyIdentifiedAsSpeakerInSection;
	int PISSubject,PISHail,PISDefinite;
	int replacedBy;
	int ownerWhere;
	int firstLocation; // the very first location for this object including replacements
	int firstSpeakerGroup;
	int firstPhysicalManifestation; // used for matching against unresolvable objects
	int lastSpeakerGroup; // used for subgrouping
	int ageSinceLastSpeakerGroup;
	int masterSpeakerIndex;
	int htmlLinkCount;
	int relativeClausePM;
	int whereRelativeClause;
	int whereRelSubjectClause;
	int usedAsLocation,lastWhereLocation;
	vector <cLocalFocus>::iterator lsiOffset;
	vector <int> spaceRelations;
	set <int> duplicates; // objects that this object replaced
	vector <int> aliases; // objects that this object is the same as (but the information cannot be merged, as in duplicates)
	vector <tIWMM> associatedNouns; // man
	vector <tIWMM> associatedAdjectives; // young, old, strange
	vector <int> possessions;
	map <tIWMM,int,tFI::cRMap::wordMapCompare> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
	tIWMM mostMatchedGeneric; // girl, woman, boy, man, etc
	int genericAge[4];
	int objectGenericAge,mostMatchedAge;
	bool identified; // identified as a speaker by afterSpeaker or beforeSpeaker quotes
	bool plural; // not an identified name, with at least one term set to plural
	bool male;
	bool female;
	bool neuter;
	bool ownerPlural;
	bool ownerMale;
	bool ownerFemale;
	bool ownerNeuter;
	bool eliminated;
	bool multiSource;
	bool suspect,verySuspect,ambiguous;
	bool partialMatch;
	bool isNotAPlace;
	bool genderNarrowed;
	bool isKindOf;
	bool wikipediaAccessed;
	bool dbPediaAccessed;
	bool container; // object or subject of verb that in turn contains other objects
	bool isTimeObject;
	bool isLocationObject;
	bool isWikiPlace;
	bool isWikiPerson;
	bool isWikiBusiness;
	bool isWikiWork;

	cLastVerbTenses lastVerbTenses[VERB_HISTORY];

	cName name;

	class cLocation
	{
	public:
		int at;
		cLocation(int a) { at=a; };
		bool operator == (const cLocation& o)
		{
			return at==o.at;
		}
    bool operator<(cLocation other) const
    {
        return at < other.at;
    }

	};
	void setSubType(int st) { subType=st; }
	void resetSubType() { subType=-1; }
	int getSubType() { return subType; }
	vector <cLocation> locations; // every position in book where object is PLUS all positions where object is matched (to the object at that position)
	int len(void)
	{
		return end-begin;
	}
	int suspiciousVerbMerge()
	{
		 if (objectClass==NAME_OBJECT_CLASS && name.first!=wNULL && name.last!=wNULL)
		 {
 			 int flc;
			 if ((flc=name.last->second.getLowestCost())>=0 && name.last->second.Form(flc)->isVerbForm && !name.last->second.isUnknown()) return 1;
			 if (name.middle!=wNULL && (flc=name.middle->second.getLowestCost())>=0 && name.middle->second.Form(flc)->isVerbForm && !name.middle->second.isUnknown()) return 2;
		 }
		 return 0;
	}
	void updateFirstLocation(int location)
	{
		if (location<firstLocation) firstLocation=location;
	}
	// this does not include quotes where objectMatched means the object is a speaker within that quote
	set <int> speakerLocations; // quotes where objectMatches/audienceObjectMatches means the object is a speaker within that quote
	cObject(enum OC sc, cName nm,int b,int e,int a,int PMAE,int ow,bool m,bool f,bool n,bool pl,bool c)
	{
		objectClass=sc; name=nm; begin=b; end=e; firstLocation=originalLocation=a; PMAElement=PMAE; male=m; female=f; neuter=n; plural=pl; container=c;
		ownerMale=ownerFemale=ownerNeuter=ownerPlural=false;
		ownerWhere=ow;
		numEncounters=0; numIdentifiedAsSpeaker=0; numDefinitelyIdentifiedAsSpeaker=0; 
		numEncountersInSection=0; numIdentifiedAsSpeakerInSection=0; numSpokenAboutInSection=0; numDefinitelyIdentifiedAsSpeakerInSection=0;
		PISSubject=PISHail=PISDefinite=0;

		firstSpeakerGroup=-1;
		firstPhysicalManifestation=-1;
		lastSpeakerGroup=-1; // used for subgrouping
		ageSinceLastSpeakerGroup=0;
		usedAsLocation=0;
		lastWhereLocation=-1;
		replacedBy=-1;
		eliminated=false;
		multiSource=false;
		masterSpeakerIndex=-1;
		htmlLinkCount=0;
		suspect=verySuspect=ambiguous=false;
		lsiOffset=cNULL;
		subType=-1;
		partialMatch=false;
		isNotAPlace=false;
		genderNarrowed=false;
		isKindOf=false;
		wikipediaAccessed=false;
		isTimeObject=false;
		isLocationObject=false;
		isWikiPlace=false;
		isWikiPerson=false;
		isWikiBusiness=false;
		isWikiWork=false;
		wikipediaAccessed=false;
		dbPediaAccessed=false;
		relativeClausePM=-1;
		whereRelativeClause=-1;
		whereRelSubjectClause=-1;
		mostMatchedGeneric=wNULL; // girl, woman, boy, man, etc
		memset(genericAge,0,4*sizeof(*genericAge));
		objectGenericAge=-1;
		mostMatchedAge=-1;
	};
	cObject(void)
	{
		objectClass=NON_GENDERED_GENERAL_OBJECT_CLASS;
		identified=plural=false; male=female=neuter=false; eliminated=false;
		ownerMale=ownerFemale=ownerNeuter=ownerPlural=false;
		firstSpeakerGroup=-1;
		firstPhysicalManifestation=-1;
		lastSpeakerGroup=-1; // used for subgrouping
		ageSinceLastSpeakerGroup=0;
		usedAsLocation=0;
		lastWhereLocation=-1;
		ownerWhere=-1;
		PMAElement=begin=end=firstLocation=originalLocation=-1;
		replacedBy=-1;
		numEncounters=0; numIdentifiedAsSpeaker=0; numDefinitelyIdentifiedAsSpeaker=0; 
		numEncountersInSection=0; numIdentifiedAsSpeakerInSection=0; numSpokenAboutInSection=0; numDefinitelyIdentifiedAsSpeakerInSection=0;
		PISSubject=PISHail=PISDefinite=0;
		suspect=verySuspect=ambiguous=false;
		multiSource=false;
		masterSpeakerIndex=-1;
		htmlLinkCount=0;
		lsiOffset=cNULL;
		partialMatch=false;
		isKindOf=false;
		wikipediaAccessed=false;
		isTimeObject=false;
		isLocationObject=false;
		isWikiPlace=false;
		isWikiPerson=false;
		isWikiBusiness=false;
		isWikiWork=false;
		isNotAPlace=false;
		genderNarrowed=false;
		wikipediaAccessed=false;
		dbPediaAccessed=false;
		relativeClausePM=-1;
		whereRelativeClause=-1;
		whereRelSubjectClause=-1;
		mostMatchedGeneric=wNULL; // girl, woman, boy, man, etc
		memset(genericAge,0,4*sizeof(*genericAge));
		objectGenericAge=-1;
		mostMatchedAge=-1;
	};
	cObject(char *buffer,int &where,unsigned int total,bool &error)
	{
		if (error=!copy(index,buffer,where,total)) return; 
		int tmp;
		if (error=!copy(tmp,buffer,where,total)) return; 
		objectClass=(OC)tmp;
		if (error=!copy(subType,buffer,where,total)) return; 
		if (error=!copy(begin,buffer,where,total)) return; 
		if (error=!copy(end,buffer,where,total)) return; 
		if (error=!copy(originalLocation,buffer,where,total)) return; 
		if (error=!copy(PMAElement,buffer,where,total)) return; 
		if (error=!copy(numEncounters,buffer,where,total)) return; 
		if (error=!copy(numIdentifiedAsSpeaker,buffer,where,total)) return; 
		if (error=!copy(numDefinitelyIdentifiedAsSpeaker,buffer,where,total)) return; 
		if (error=!copy(numEncountersInSection,buffer,where,total)) return; 
		if (error=!copy(numSpokenAboutInSection,buffer,where,total)) return; 
		if (error=!copy(numIdentifiedAsSpeakerInSection,buffer,where,total)) return; 
		if (error=!copy(numDefinitelyIdentifiedAsSpeakerInSection,buffer,where,total)) return; 
		if (error=!copy(PISSubject,buffer,where,total)) return; 
		if (error=!copy(PISHail,buffer,where,total)) return; 
		if (error=!copy(PISDefinite,buffer,where,total)) return; 
		if (error=!copy(replacedBy,buffer,where,total)) return; 
		if (error=!copy(ownerWhere,buffer,where,total)) return; 
		if (error=!copy(firstLocation,buffer,where,total)) return; 
		if (error=!copy(firstSpeakerGroup,buffer,where,total)) return; 
		if (error=!copy(firstPhysicalManifestation,buffer,where,total)) return; 
		if (error=!copy(lastSpeakerGroup,buffer,where,total)) return; 
		if (error=!copy(ageSinceLastSpeakerGroup,buffer,where,total)) return; 
		if (error=!copy(masterSpeakerIndex,buffer,where,total)) return; 
		if (error=!copy(htmlLinkCount,buffer,where,total)) return; 
		if (error=!copy(relativeClausePM,buffer,where,total)) return; 
		if (error=!copy(whereRelativeClause,buffer,where,total)) return; 
		if (error=!copy(whereRelSubjectClause,buffer,where,total)) return; 
		if (error=!copy(usedAsLocation,buffer,where,total)) return; 
		if (error=!copy(lastWhereLocation,buffer,where,total)) return; 
		if (error=!copy(spaceRelations,buffer,where,total)) return;
		if (error=!copy(duplicates,buffer,where,total)) return;
		if (error=!copy(aliases,buffer,where,total)) return;
		int num;
		wstring str;
		if (error=!copy(num,buffer,where,total)) return; 
		for (int I=0; I<num; I++)
		{
			if (error=!copy(str,buffer,where,total)) return;
			associatedNouns.push_back(Words.query(str));
		}
		if (error=!copy(num,buffer,where,total)) return; 
		for (int I=0; I<num; I++)
		{
			if (error=!copy(str,buffer,where,total)) return;
			associatedAdjectives.push_back(Words.query(str));
		}
		if (error=!copy(possessions,buffer,where,total)) return;
		// read generic map
		// map <tIWMM,int,tFI::cRMap::wordMapCompare> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
		if (error=!copy(num,buffer,where,total)) return; 
		int frequency;
		for (int I=0; I<num; I++)
		{
			if (error=!copy(str,buffer,where,total)) return;
			tIWMM tStr=Words.query(str);
			if (error=!copy(frequency,buffer,where,total)) return;
			genericNounMap[tStr]=frequency;
		}
		if (error=!copy(str,buffer,where,total)) return;
		mostMatchedGeneric=(str.length()) ? Words.query(str) : wNULL;
		for (int I=0; I<4; I++)
			if (error=!copy(genericAge[I],buffer,where,total)) return;
		if (error=!copy(objectGenericAge,buffer,where,total)) return; 
		if (error=!copy(mostMatchedAge,buffer,where,total)) return; 
		__int64 flags;
		if (error=!copy(flags,buffer,where,total)) return;
		isWikiWork=flags&1; flags>>=1;
		isWikiBusiness=flags&1; flags>>=1;
		isWikiPerson=flags&1; flags>>=1;
		isWikiPlace=flags&1; flags>>=1;
		isLocationObject=flags&1; flags>>=1;
		isTimeObject=flags&1; flags>>=1;
		dbPediaAccessed=flags&1; flags>>=1;
		container=flags&1; flags>>=1;
		wikipediaAccessed=flags&1; flags>>=1;
		isKindOf=flags&1; flags>>=1;
		genderNarrowed=flags&1; flags>>=1;
		isNotAPlace=flags&1; flags>>=1;
		partialMatch=flags&1; flags>>=1;
		ambiguous=flags&1; flags>>=1;
		verySuspect=flags&1; flags>>=1;
		suspect=flags&1; flags>>=1;
		multiSource=flags&1; flags>>=1;
		eliminated=flags&1; flags>>=1;
		ownerNeuter=flags&1; flags>>=1;
		ownerFemale=flags&1; flags>>=1;
		ownerMale=flags&1; flags>>=1;
		ownerPlural=flags&1; flags>>=1;
		neuter=flags&1; flags>>=1;
		female=flags&1; flags>>=1;
		male=flags&1; flags>>=1;
		plural=flags&1; flags>>=1;
		identified=flags&1; 
		for (int I=0; I<VERB_HISTORY; I++)
			if (error=!copy(lastVerbTenses[I],buffer,where,total)) return;
		if (error=!copy(name,buffer,where,total)) return;
	}
	bool write(void *buffer,int &where,unsigned int limit)
	{
		if (!copy(buffer,index,where,limit)) return false; 
		if (!copy(buffer,(int)objectClass,where,limit)) return false; 
		if (!copy(buffer,subType,where,limit)) return false; 
		if (!copy(buffer,begin,where,limit)) return false; 
		if (!copy(buffer,end,where,limit)) return false; 
		if (!copy(buffer,originalLocation,where,limit)) return false; 
		if (!copy(buffer,PMAElement,where,limit)) return false; 
		if (!copy(buffer,numEncounters,where,limit)) return false; 
		if (!copy(buffer,numIdentifiedAsSpeaker,where,limit)) return false; 
		if (!copy(buffer,numDefinitelyIdentifiedAsSpeaker,where,limit)) return false; 
		if (!copy(buffer,numEncountersInSection,where,limit)) return false; 
		if (!copy(buffer,numSpokenAboutInSection,where,limit)) return false; 
		if (!copy(buffer,numIdentifiedAsSpeakerInSection,where,limit)) return false; 
		if (!copy(buffer,numDefinitelyIdentifiedAsSpeakerInSection,where,limit)) return false; 
		if (!copy(buffer,PISSubject,where,limit)) return false; 
		if (!copy(buffer,PISHail,where,limit)) return false; 
		if (!copy(buffer,PISDefinite,where,limit)) return false; 
		if (!copy(buffer,replacedBy,where,limit)) return false; 
		if (!copy(buffer,ownerWhere,where,limit)) return false; 
		if (!copy(buffer,firstLocation,where,limit)) return false; 
		if (!copy(buffer,firstSpeakerGroup,where,limit)) return false; 
		if (!copy(buffer,firstPhysicalManifestation,where,limit)) return false; 
		if (!copy(buffer,lastSpeakerGroup,where,limit)) return false; 
		if (!copy(buffer,ageSinceLastSpeakerGroup,where,limit)) return false; 
		if (!copy(buffer,masterSpeakerIndex,where,limit)) return false; 
		if (!copy(buffer,htmlLinkCount,where,limit)) return false; 
		if (!copy(buffer,relativeClausePM,where,limit)) return false; 
		if (!copy(buffer,whereRelativeClause,where,limit)) return false; 
		if (!copy(buffer,whereRelSubjectClause,where,limit)) return false; 
		if (!copy(buffer,usedAsLocation,where,limit)) return false; 
		if (!copy(buffer,lastWhereLocation,where,limit)) return false; 
		if (!copy(buffer,spaceRelations,where,limit)) return false;
		if (!copy(buffer,duplicates,where,limit)) return false;
		if (!copy(buffer,aliases,where,limit)) return false;
		if (!copy(buffer,(int)associatedNouns.size(),where,limit)) return false; 
		for (unsigned int I=0; I<associatedNouns.size(); I++)
			if (!copy(buffer,associatedNouns[I]->first,where,limit)) return false;
		if (!copy(buffer,(int)associatedAdjectives.size(),where,limit)) return false; 
		for (unsigned int I=0; I<associatedAdjectives.size(); I++)
			if (!copy(buffer,associatedAdjectives[I]->first,where,limit)) return false;
		if (!copy(buffer,possessions,where,limit)) return false; 
		// write generic map
		// map <tIWMM,int,tFI::cRMap::wordMapCompare> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
		if (!copy(buffer,(int)genericNounMap.size(),where,limit)) return false; 
		for (map <tIWMM,int,tFI::cRMap::wordMapCompare>::iterator gnm=genericNounMap.begin(),gnmEnd=genericNounMap.end(); gnm!=gnmEnd; gnm++)
		{
			if (!copy(buffer,gnm->first->first,where,limit)) return false;
			if (!copy(buffer,gnm->second,where,limit)) return false;
		}
		if (!copy(buffer,(mostMatchedGeneric!=wNULL) ? mostMatchedGeneric->first : L"",where,limit)) return false;
		for (int I=0; I<4; I++)
			if (!copy(buffer,genericAge[I],where,limit)) return false;
		if (!copy(buffer,objectGenericAge,where,limit)) return false; 
		if (!copy(buffer,mostMatchedAge,where,limit)) return false; 
		__int64 flags=0;
		flags|=(identified) ? 1:0; flags<<=1;
		flags|=(plural) ? 1:0; flags<<=1;
		flags|=(male) ? 1:0; flags<<=1;
		flags|=(female) ? 1:0; flags<<=1;
		flags|=(neuter) ? 1:0; flags<<=1;
		flags|=(ownerPlural) ? 1:0; flags<<=1;
		flags|=(ownerMale) ? 1:0; flags<<=1;
		flags|=(ownerFemale) ? 1:0; flags<<=1;
		flags|=(ownerNeuter) ? 1:0; flags<<=1;
		flags|=(eliminated) ? 1:0; flags<<=1;
		flags|=(multiSource) ? 1:0; flags<<=1;
		flags|=(suspect) ? 1:0; flags<<=1;
		flags|=(verySuspect) ? 1:0; flags<<=1;
		flags|=(ambiguous) ? 1:0; flags<<=1;
		flags|=(partialMatch) ? 1:0; flags<<=1;
		flags|=(isNotAPlace) ? 1:0; flags<<=1;
		flags|=(genderNarrowed) ? 1:0; flags<<=1;
		flags|=(isKindOf) ? 1:0; flags<<=1;
		flags|=(wikipediaAccessed) ? 1:0; flags<<=1;
		flags|=(container) ? 1:0; flags<<=1;
		flags|=(dbPediaAccessed) ? 1:0; flags<<=1;
		flags|=(isTimeObject) ? 1:0; flags<<=1;
		flags|=(isLocationObject) ? 1:0; flags<<=1;
		flags|=(isWikiPlace) ? 1:0; flags<<=1;
		flags|=(isWikiPerson) ? 1:0; flags<<=1;
		flags|=(isWikiBusiness) ? 1:0; flags<<=1;
		flags|=(isWikiWork) ? 1:0; 
		if (!copy(buffer,flags,where,limit)) return false;
		for (int I=0; I<VERB_HISTORY; I++)
			if (!copy(buffer,lastVerbTenses[I],where,limit)) return false;
		if (!copy(buffer,name,where,limit)) return false;
		return true;
	}

	void erase(void) { begin=end=firstLocation=originalLocation=-1; };
	bool isPronounLike(void) { return objectClass==PRONOUN_OBJECT_CLASS || objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS || objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS; }
	bool cataphoricMatch(cObject *obj);
	bool isAgent(bool permitGenderedPronouns)
	{
		if (objectClass==NAME_OBJECT_CLASS || objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
				objectClass==BODY_OBJECT_CLASS || objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || 
				objectClass==GENDERED_DEMONYM_OBJECT_CLASS || objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
				objectClass==META_GROUP_OBJECT_CLASS) return true;
		// if not permitGenderedPronouns, reject all pronouns.
		if (!permitGenderedPronouns) return false;
		return male || female || (end-begin==1 && (ownerMale || ownerFemale)); // no "it" or "they" or "I" but also "mine"
	}
	bool equals(const cObject& o, const vector <WordMatch> &m)
	{
		if ((end-begin)!=(o.end-o.begin) || o.objectClass!=objectClass) return false;
		for (int K=o.begin,K2=begin; K<o.end; K++,K2++)
			if (m[K].word!=m[K2].word) return false;
		return true;
	}
	bool wikiDefinition()
	{
		return isWikiBusiness || isWikiPerson || isWikiPlace || isWikiWork;
	}
	bool matchGender(const cObject& o,bool &unambiguousGenderFound)
	{
		bool sexUncertain=(!male && !female) || (male && female);
		bool otherSexUncertain=(!o.male && !o.female) || (o.male && o.female);
		unambiguousGenderFound=!(sexUncertain && otherSexUncertain);
		if (sexUncertain || otherSexUncertain) return true;
		return male==o.male;
	}
	bool matchGender(const cObject& o)
	{
		bool unambiguousGenderFound;
		return matchGender(o,unambiguousGenderFound);
	}
	bool matchGenderIncludingNeuter(const cObject& o,bool &ambiguousGenderMatch)
	{
		bool sexUncertain=(!male && !female && !neuter) || (male && female);
		bool otherSexUncertain=(!o.male && !o.female && !o.neuter) || (o.male && o.female);
		ambiguousGenderMatch=true;
		if (sexUncertain && otherSexUncertain) return true;
		if (sexUncertain && !o.neuter) return true;
		if (otherSexUncertain && !neuter) return true;
		ambiguousGenderMatch=false;
		if ((o.neuter && neuter) || (o.male && male) || (o.female && female)) return true;
		return male==o.male && female==o.female && neuter==o.neuter;
	}
	bool matchGenderIncludingNeuter(const cObject& o)
	{
		bool ambiguousGenderMatch;
		return matchGenderIncludingNeuter(o,ambiguousGenderMatch);
	}
	bool exactGenderMatch(const cObject& o)
	{
		if ((!male && !female) || (male && female)) return false;
		if ((!o.male && !o.female) || (o.male && o.female)) return false;
		return male==o.male;
	}
	bool confidentMatch(cObject &o,sTrace &t)
	{
		return (objectClass==NAME_OBJECT_CLASS || objectClass==NON_GENDERED_NAME_OBJECT_CLASS) && 
					 (o.objectClass==NAME_OBJECT_CLASS || o.objectClass==NON_GENDERED_NAME_OBJECT_CLASS) && 
					 name.confidentMatch(o.name,exactGenderMatch(o),t) && plural==o.plural && matchGender(o);
	}
	bool nameMatch(cObject &o,sTrace &t)
	{
		bool unambiguousGenderFound;
		return (o.objectClass==NAME_OBJECT_CLASS || o.objectClass==NON_GENDERED_NAME_OBJECT_CLASS) && 
			matchGender(o,unambiguousGenderFound) && name.like(o.name,t) && plural==o.plural;
	}
	bool nameAnyNonGenderedMatch(vector <WordMatch> &m,cObject &o)
	{
		if (name.first!=wNULL && name.last!=wNULL && o.name.first!=wNULL && o.name.last!=wNULL)
			return nameNonGenderedMatch(m,o);
		if (name.any!=wNULL && o.name.any!=wNULL)
			return name.any==o.name.any;
		return false;
	}
	// match between gendered and non-gendered objects
	bool nameNonGenderedMatch(vector <WordMatch> &m,cObject &o)
	{
		// both must have at least first and last names, otherwise a neuter word may improperly obtain male/female characteristics
		// charing cross may become male/female
		if (name.first==wNULL || name.last==wNULL || o.name.first==wNULL || o.name.last==wNULL)
			return false;
		bool unambiguousGenderFound;
		if (o.objectClass!=NON_GENDERED_NAME_OBJECT_CLASS || !matchGender(o,unambiguousGenderFound) || plural!=o.plural)
			return false;
		int where=o.originalLocation,first=-1,last=-1;
		for (int I=m[where].beginObjectPosition; I<m[where].endObjectPosition; I++)
			if (m[I].queryWinnerForm(PROPER_NOUN_FORM)>=0) 
			{
				if (first<0) first=last=I;
				else last=I;
			}
		if (first==-1) return false;
		if (first==last) 
		{
			if (name.any!=wNULL)
				return m[first].word==name.any;
			return m[first].word==name.last;
		}
		if (name.any!=wNULL)
			return m[last].word==name.any;
		return m[first].word==name.first && m[last].word==name.last;
	}
	bool nameMatchExact(cObject &o)
	{
		if (name.first!=o.name.first || name.last!=o.name.last || name.any!=o.name.any || 
			  name.middle!=o.name.middle || name.middle2!=o.name.middle2 || name.suffix!=o.name.suffix)
			return false;
		bool unambiguousGenderFound;
		if (!matchGender(o,unambiguousGenderFound) || plural!=o.plural)
			return false;
		return true;
	}
	bool hasAttribute(int where,vector <WordMatch> &m);
	bool hasAgeModifier(vector <WordMatch> &m,wchar_t *modifiers[]);
	int setGenericAge(vector <WordMatch> &m);
	bool updateGenericGender(int where,tIWMM w,int fromAge,wchar_t *fromWhere,sTrace &t);
	void updateGenericGenders(map <tIWMM,int,tFI::cRMap::wordMapCompare> &genericNounMap,int *replacedGenericAge);
	static int whichOrderWord(tIWMM word)
	{
		for (unsigned int J=0; wordOrderWords[J]; J++)
			if (word->first==wordOrderWords[J])
				return J;
		return -1;
	}
	int wordOrderSensitive(int at,vector <WordMatch> &m)
	{
		int tmp;
		for (int I=m[at].beginObjectPosition; I<m[at].endObjectPosition; I++)
			if ((tmp=whichOrderWord(m[I].word))>=0) 
				return tmp;
		return -1;
	}
	void setSex(tIWMM word)
	{
		if (word==wNULL || (male ^ female)) return; // male vs female already set
		int inflectionFlags=word->second.inflectionFlags;
		if (plural)
		{
			male|=(inflectionFlags&MALE_GENDER)!=0;
			female|=(inflectionFlags&FEMALE_GENDER)!=0;
		}
		else
		{
			male=(inflectionFlags&MALE_GENDER)!=0;
			female=(inflectionFlags&FEMALE_GENDER)!=0;
		}
	}
	void clear(bool webSearch)
	{
		//replacedBy=-1;  cannot clear this without clearing eliminated flag
		numEncounters=0; 
		numIdentifiedAsSpeaker=0; 
		numDefinitelyIdentifiedAsSpeaker=0; 
		numEncountersInSection=0; 
		numIdentifiedAsSpeakerInSection=0; 
		numSpokenAboutInSection=0; 
		numDefinitelyIdentifiedAsSpeakerInSection=0;
		associatedAdjectives.clear();
		associatedNouns.clear();
		genericNounMap.clear();
		mostMatchedGeneric=wNULL; // girl, woman, boy, man, etc
		memset(genericAge,0,4*sizeof(*genericAge));
		mostMatchedAge=-1;
		aliases.clear(); // re-analyze meta naming
		lastSpeakerGroup=-1;
		ageSinceLastSpeakerGroup=-1;
		if (!webSearch)
			isPossibleSubType(true);
		//usedAsLocation=0; this is set additionally at identify and at resolveSpeakers
		//lastWhereLocation=-1;
		if (genderNarrowed && objectClass!=NON_GENDERED_NAME_OBJECT_CLASS) male=female=true; 
		if (genderNarrowed && objectClass==NAME_OBJECT_CLASS && (male && female))
		{
			setSex(name.any);
			setSex(name.hon);
			setSex(name.hon2);
			setSex(name.hon3);
			setSex(name.first);
			setSex(name.middle);
			setSex(name.middle2);
			if (name.hon!=wNULL && name.first==wNULL && name.hon->first==L"st")
				setSex(name.last);
			if (!male && !female && !neuter)
				male=female=true;
		}
		genderNarrowed=false;
		for (int objectLastTense=0; objectLastTense<VERB_HISTORY; objectLastTense++)
			lastVerbTenses[objectLastTense].clear();
	}
	bool isPossibleSubType(bool st)
	{
		if (subType>=0) return true;
		if (subType<0 && (objectClass==NAME_OBJECT_CLASS || objectClass==NON_GENDERED_NAME_OBJECT_CLASS) && 
				!PISSubject && !PISHail && !PISDefinite && 
				name.hon==wNULL && !isNotAPlace && (name.any==wNULL || name.any->second.query(demonymForm)<0))// && !(m[I].flags&WordMatch::flagAdjectivalObject))
		{
			// if it is more than one word, and the last word is known, and is not a place-type, then don't make it a place
			if (((male && female) || (!male && !female)) && st)
			{
				subType=UNKNOWN_PLACE_SUBTYPE;
			}
			return true;
		}
		return false;
	}
};

enum eOBJECTS { UNKNOWN_OBJECT=-1,OBJECT_UNKNOWN_MALE=-2, OBJECT_UNKNOWN_FEMALE=-3,
								OBJECT_UNKNOWN_MALE_OR_FEMALE=-4, OBJECT_UNKNOWN_NEUTER=-5, OBJECT_UNKNOWN_PLURAL=-6,OBJECT_UNKNOWN_ALL=-7 };

extern unsigned int verbObjectsTagSet;
extern unsigned int iverbTagSet;
extern unsigned int nounDeterminerTagSet;
extern unsigned int subjectVerbAgreementTagSet;
extern unsigned int subjectTagSet;
extern unsigned int specificAnaphorTagSet;
extern unsigned int descendantAgreementTagSet;
extern unsigned int objectTagSet;
extern unsigned int subjectVerbRelationTagSet;
extern unsigned int verbObjectRelationTagSet;
extern unsigned int verbSenseTagSet;
extern unsigned int nameTagSet;
extern unsigned int metaNameEquivalenceTagSet;
extern unsigned int roleTagSet;
extern unsigned int prepTagSet;
extern unsigned int BNCPreferencesTagSet;
extern unsigned int nAgreeTagSet;
extern unsigned int EVALTagSet;
extern unsigned int idRelationTagSet;
extern unsigned int metaSpeakerTagSet;
extern unsigned int notbutTagSet;
extern unsigned int mobjectTagSet;
extern unsigned int ndPrepTagSet;
extern unsigned int timeTagSet;
extern unsigned int qtobjectTagSet;

extern unsigned int PREP_TAG,OBJECT_TAG,SUBOBJECT_TAG,REOBJECT_TAG,IOBJECT_TAG,SUBJECT_TAG,PREP_OBJECT_TAG,VERB_TAG,PLURAL_TAG,MPLURAL_TAG,GNOUN_TAG,FLOAT_TIME_TAG;
extern unsigned int MNOUN_TAG,PNOUN_TAG,VNOUN_TAG,HAIL_TAG,NAME_TAG,REL_TAG,SENTENCE_IN_REL_TAG;

static const int NUM_SIMPLE_TENSE=16;
static const int NUM_INF_TENSE=9;
class cTenseStat
{
public:
	int occurrence; // (includes passive)
	int passiveOccurrence;
	int followedBy[NUM_SIMPLE_TENSE]; // what tense is this tense followed by?
	int infinitive[NUM_INF_TENSE]; // what tense in an infinitive phrase (if any)
	cTenseStat()
	{
		occurrence=passiveOccurrence=0;
		memset(followedBy,0,sizeof(followedBy));
		memset(infinitive,0,sizeof(infinitive));
	};
};

bool frequencyCompare(const cOM &lhs,const cOM &rhs);
int mapNumeralCardinal(const wstring &word);
int mapNumeralOrdinal(const wstring &word);
#define SALIENCE_THRESHOLD 400
#define MAX_AGE 3
#define EOS_AGE 5 // age each object in localObjects by this when the end of a section is hit

class cTimelineSegment
{
public:
	int parentTimeline;
	int begin;
	int end;
	int speakerGroup;
	int location;
	int linkage;
	int sr;
	set <int> timeTransitions;
	vector <int> locationTransitions;
	vector <int> subTimelines;
	cTimelineSegment(int _parentTimeline,int _begin,int _end,int _speakerGroup,int _location,int _linkage,int _sr,vector <int> &_subTimelines)
	{
		parentTimeline=_parentTimeline;
		begin=_begin;
		end=_end;
		speakerGroup=_speakerGroup;
		location=_location;
		linkage=_linkage;
		sr=_sr;
		subTimelines=_subTimelines;
	}
	cTimelineSegment()
	{
		parentTimeline=-1;
		begin=-1;
		end=-1;
		speakerGroup=-1;
		location=-1;
		linkage=-1;
	}
	bool copy(void *buffer,int &w,int limit) // write
	{
		if (!::copy(buffer,parentTimeline,w,limit)) return false;
		if (!::copy(buffer,begin,w,limit)) return false;
		if (!::copy(buffer,end,w,limit)) return false;
		if (!::copy(buffer,speakerGroup,w,limit)) return false;
		if (!::copy(buffer,location,w,limit)) return false;
		if (!::copy(buffer,linkage,w,limit)) return false;
		if (!::copy(buffer,sr,w,limit)) return false;
		if (!::copy(buffer,timeTransitions,w,limit)) return false;
		if (!::copy(buffer,locationTransitions,w,limit)) return false;
		if (!::copy(buffer,subTimelines,w,limit)) return false;
		return true;
	}
	cTimelineSegment(char *buffer,int &w,unsigned int limit,bool &error)
	{
		if (error=!::copy(parentTimeline,buffer,w,limit)) return;
		if (error=!::copy(begin,buffer,w,limit)) return;
		if (error=!::copy(end,buffer,w,limit)) return;
		if (error=!::copy(speakerGroup,buffer,w,limit)) return;
		if (error=!::copy(location,buffer,w,limit)) return;
		if (error=!::copy(linkage,buffer,w,limit)) return;
		if (error=!::copy(sr,buffer,w,limit)) return;
		if (error=!::copy(timeTransitions,buffer,w,limit)) return;
		if (error=!::copy(locationTransitions,buffer,w,limit)) return;
		if (error=!::copy(subTimelines,buffer,w,limit)) return;
		error=w>(signed)limit;
	}
};

#define maxCategoryLength 1024
#define dbPedia_Ontology_Type 1
#define YAGO_Ontology_Type 2
#define UMBEL_Ontology_Type 3 // <http://umbel.org/umbel/rc/, <http://umbel.org/umbel#
#define OpenGIS_Ontology_Type 4

class dbs
{
public:
	wstring compactLabel;
	wstring infoPage;
	wstring abstractDescription;
	wstring commentDescription;
	int numLine; 
	int ontologyHierarchicalRank;
	int ontologyType; 
	int resourceType;
	int descriptionFilled;
	vector <wstring> superClasses;
	vector <int> superClassResourceTypes;
	dbs()
	{
		numLine=ontologyType= resourceType =-1;
		ontologyHierarchicalRank=100; // ignore
		descriptionFilled=-1; // number of rows found in ontology
	}
	wstring toString(wstring &tmpstr,wstring origin)
	{ 
		wstring tmpstr2,tmpstr3,tmpstr4,tmpstr5;
		tmpstr = origin + L":" + ontologyTypeString(ontologyType, resourceType,tmpstr5) + L":" + compactLabel;
		if (infoPage.length() > 0)
			tmpstr += L"\ninfoPage:" + infoPage;
		if (abstractDescription.length() > 0)
			tmpstr += L"\nabstract:" + abstractDescription;
		if (commentDescription.length() > 0)
			tmpstr += L"\ncomment:" + commentDescription;
		vectorString(superClasses, tmpstr2, L" ");
		if (tmpstr2.length()>0)
			tmpstr+=L"["+tmpstr2+L"]";
		if (ontologyHierarchicalRank>0)
			tmpstr+=L":ontologyHierarchicalRank "+itos(ontologyHierarchicalRank,tmpstr3);
		return tmpstr;
	}
  bool operator == (const dbs &o)
	{
		if (compactLabel!=o.compactLabel) return false;
		if (infoPage!=o.infoPage) return false;
		if (abstractDescription!=o.abstractDescription) return false;
		if (commentDescription!=o.commentDescription) return false;
		if (numLine!=o.numLine) return false;
		if (ontologyHierarchicalRank!=o.ontologyHierarchicalRank) return false;
		if (ontologyType!=o.ontologyType) return false;
		if (descriptionFilled!=o.descriptionFilled) return false;
		if (superClasses!=o.superClasses) return false;
		return true;
	}
  bool operator != (const dbs &o)
	{
		return !(*this==o);
	}
	void lplog(int whichLog,wstring origin)
	{ 
		wstring tmpstr;
		::lplog(whichLog,L"%s",toString(tmpstr,origin).c_str());
	}
};

bool copy(unordered_map <wstring, dbs>::iterator &hint,void *buf,int &where,int limit,unordered_map <wstring, dbs> &hm);

class cTreeCat
{
public:
	unordered_map <wstring, dbs>::iterator cli;
	wstring typeObject; 
	int confidence;
	wstring abstract;
	wstring qtype;
	wstring key;
	wstring top; // temporary used for back mapping of top class types
	wstring parentObject; 
	vector <wstring> wikipediaLinks;
	vector <wstring> professionLinks;
	bool preferred;
	bool exactMatch;
	bool preferredUnknownClass;

	cTreeCat(unordered_map <wstring, dbs>::iterator cli,wstring typeObject,wstring &parentObject,wstring qtype,int confidence,string &k,string &description,vector <wstring> &wikipediaLinks,vector <wstring> &professionLinks,bool exactMatch)
	{
		this->cli=cli;
		this->typeObject=typeObject;
		this->parentObject=parentObject;
		this->qtype=qtype;
		this->confidence=confidence;
		mTW(k,this->key);
		this->wikipediaLinks=wikipediaLinks;
		this->professionLinks=professionLinks;
		mTW(description,this->abstract);
		preferred=false;
		this->exactMatch=exactMatch;
		preferredUnknownClass=false;
	}
	cTreeCat(unordered_map <wstring, dbs>::iterator cli,wstring typeObject,wstring &parentObject,wstring qtype,int confidence)
	{
		this->cli=cli;
		this->typeObject=typeObject;
		this->parentObject=parentObject;
		this->qtype=qtype;
		this->confidence=confidence;
		preferred=false;
		exactMatch=false;
		preferredUnknownClass=false;
	}
	cTreeCat(unordered_map <wstring, dbs>::iterator cli)
	{
		this->cli=cli;
		preferred=false;
		exactMatch=false;
		preferredUnknownClass=false;
		confidence = 0;
	}
	cTreeCat()
	{
		//this->cli=(unordered_map <wstring, dbs>::iterator)((void *) 0);
		preferred=false;
		exactMatch=false;
		preferredUnknownClass=false;
		confidence = 0;
	}
	bool equals(const cTreeCat *o)
	{
		if (cli->first!=o->cli->first) return false;
		if (cli->second.compactLabel!=o->cli->second.compactLabel) return false;
		return true;
	}
  bool operator == (const cTreeCat &o)
  {
		if (cli->first!=o.cli->first) return false;
		if (cli->second!=o.cli->second) return false;
		if (typeObject!=o.typeObject) return false; 
		if (parentObject!=o.parentObject) return false; 
		if (qtype!=o.qtype) return false; 
		if (key!=o.key) return false; 
		if (wikipediaLinks!=o.wikipediaLinks) return false; 
		if (professionLinks!=o.professionLinks) return false; 
		if (confidence!=o.confidence) return false; 
		if (infoPage!=o.infoPage) return false; 
		if (abstract!=o.abstract) return false; 
		if (comment!=o.comment) return false; 
		if (preferred!=o.preferred) return false; 
		if (exactMatch!=o.exactMatch) return false; 
		if (preferredUnknownClass!=o.preferredUnknownClass) return false; 
		return true;
  }
  bool operator != (const cTreeCat &o)
	{
		return !(*this==o);
	}
	wstring toString(wstring &tmpstr);
	void lplogTC(int whichLog,wstring object);
	void assignDetails(wstring &a,wstring &c,wstring &ip)
	{
		this->abstract=a;
		this->comment=c;
		this->infoPage=ip;
	}

	bool copy(void *buf,dbs &dbsn,int &where,int limit)
	{ 
		if (!::copy(buf,dbsn.compactLabel,where,limit)) return false;
		if (!::copy(buf,dbsn.infoPage,where,limit)) return false;
		if (!::copy(buf,dbsn.abstractDescription,where,limit)) return false;
		if (!::copy(buf,dbsn.commentDescription,where,limit)) return false;
		if (!::copy(buf,dbsn.numLine,where,limit)) return false;
		if (!::copy(buf,dbsn.ontologyType,where,limit)) return false;
		if (!::copy(buf,dbsn.ontologyHierarchicalRank,where,limit)) return false;
		if (!::copy(buf, dbsn.superClasses, where, limit)) return false;
		if (!::copy(buf, dbsn.descriptionFilled, where, limit)) return false;
		return true;
	}

	bool copy(void *buf,unordered_map <wstring, dbs>::iterator dbsi,int &where,int limit)
	{
		if (!::copy(buf,dbsi->first,where,limit)) return false;
		if (!copy(buf,dbsi->second,where,limit)) return false;
		return true;
	}

	bool copy(unordered_map <wstring, dbs> &hm,void *buf,int &where,int limit)
	{ 
		if (!::copy(cli,buf,where,limit,hm)) return false;
		if (!::copy(typeObject,buf,where,limit)) return false;
		if (!::copy(parentObject,buf,where,limit)) return false;
		if (!::copy(qtype,buf,where,limit)) return false;
		if (!::copy(confidence,buf,where,limit)) return false;
		if (!::copy(infoPage,buf,where,limit)) return false;
		if (!::copy(abstract,buf,where,limit)) return false;
		if (!::copy(comment,buf,where,limit)) return false;
		if (!::copy(wikipediaLinks,buf,where,limit)) return false;
		if (!::copy(professionLinks,buf,where,limit)) return false;
		int flags;
		if (!::copy(flags,buf,where,limit)) return false;
		preferred=(flags&1)!=0;
		exactMatch=(flags&2)!=0;
		preferredUnknownClass=(flags&4)!=0;
		return true;
	}

	bool copy(void *buf,int &where,int limit)
	{ 
		if (!copy(buf,cli,where,limit)) return false;
		if (!::copy(buf,typeObject,where,limit)) return false;
		if (!::copy(buf,parentObject,where,limit)) return false;
		if (!::copy(buf,qtype,where,limit)) return false;
		if (!::copy(buf,confidence,where,limit)) return false;
		if (!::copy(buf,infoPage,where,limit)) return false;
		if (!::copy(buf,abstract,where,limit)) return false;
		if (!::copy(buf,comment,where,limit)) return false;
		if (!::copy(buf,wikipediaLinks,where,limit)) return false;
		if (!::copy(buf,professionLinks,where,limit)) return false;
		int flags=((preferred) ? 1:0) | (((exactMatch) ? 1:0)<<1) | ((preferredUnknownClass ? 1:0)<<2);
		if (!::copy(buf,flags,where,limit)) return false;
		return true;
	}

	void logIdentity(int logType,wstring object,bool printOnlyPreferred)
	{ 
		if (printOnlyPreferred && !preferred && !exactMatch) return;
		wstring tmpstr,tmpstr2,tmpstr3;
		::lplog(logType,L"%s%s%s%s[%d]ISTYPE[LI] %s:%s(%s):%s:rank %d (%s,%s)",
			object.c_str(), (preferred) ? L":PREFERRED " : L"", (exactMatch) ? L"EM " : L"", (preferredUnknownClass) ? L"PU " : L"", confidence,
			cli->first.c_str(),ontologyTypeString(cli->second.ontologyType, cli->second.resourceType,tmpstr3),qtype.c_str(),cli->second.compactLabel.c_str(),cli->second.ontologyHierarchicalRank,
			vectorString(cli->second.superClasses,tmpstr,L" ").c_str(),parentObject.c_str());
		::lplog(logType,L"    %s:%s:%s:%s",
			parentObject.c_str(),typeObject.c_str(),infoPage.c_str(),abstract.c_str());
	}

private:
	wstring infoPage;
	wstring comment;
};

class Source
{
public:
	Source(wchar_t *databaseServer,int _sourceType,bool generateFormStatistics,bool skipWordInitialization,bool printProgress);
	int beginClock;
	int pass;
	intArray reverseMatchElements;
	vector <WordMatch> m;
	patternElementMatchArray pema;
	bool parseNecessary(wchar_t *path);
	int readSourceBuffer(wstring title, wstring etext, wstring path, wstring encoding, wstring &start, int &repeatStart);
	int parseBuffer(wstring &path,unsigned int &unknownCount,bool newsBank);
	int tokenize(wstring title, wstring etext, wstring path, wstring encoding, wstring &start, int &repeatStart, unsigned int &unknownCount, bool newsBank);
	bool write(IOHANDLE file);
	bool read(char *buffer,int &where,unsigned int total, bool &parsedOnly, bool printProgress, bool readOnlyParsed);
	bool flush(int fd,void *buffer,int &where);
	bool FlushFile(HANDLE fd, void *buffer, int &where);
	bool writeCheck(wstring path);
	bool write(wstring file,bool S2, bool saveOld);
	bool findStart(wstring &buffer,wstring &start,int &repeatStart,wstring &title);
	bool retrieveText(wstring &path,wstring etext,wstring &start,int &repeatStart,wstring author,wstring title);
	bool readSource(wstring &path,bool checkOnly, bool &parsedOnly, bool printProgress,bool readOnlyParsed);
	Source *parentSource;
	vector <matchElement> whatMatched;
	struct wordMapCompare
	{
		bool operator()(const tIWMM &lhs, const tIWMM &rhs) const
		{
			return lhs->first<rhs->first;
		}
	};
	map < tIWMM,set<int>,wordMapCompare> relatedObjectsMap;
	unordered_map < int,wstring> temporaryPatternBuffer;

	int numSearchedInMemory,processOrder;
	bool isFormsProcessed;
	// For each tagset type defined
	//   there will be a map from pema to all the tagsets collected from that pema position downward through the tree.
	vector <unordered_map <int, vector < vector <tTagLocation> > > > pemaMapToTagSetsByPemaByTagSet;

	// new pattern detection
	void accumulateNewPatterns(void);

	void clearSource(void);

	int evaluateBNCPreferenceForPosition(int position,int patternPreference,int flag,bool remove);
	int evaluateBNCPreference(vector <tTagLocation> &tagSet,wchar_t *tag,int patternPreference,bool remove);
	int evaluateBNCPreferences(int position,int PEMAPosition,vector <tTagLocation> &tagSet);
	int BNCPatternViolation(int position,int PEMAPosition,vector < vector <tTagLocation> > &tagSets);

	enum speakerGroupTypes { CURRENT_SUBSET_SG=-2 };
	class cSpeakerGroup
	{
	public:
		int sgBegin,sgEnd,section;
		int previousSubsetSpeakerGroup; // speaker group index
		int saveNonNameObject; // speaker group index
		int conversationalQuotes;
		bool speakersAreNeverGroupedTogether; // speakers never appear anywhere in single group
		bool tlTransition; // a time/location/group of speakers transition
		set <int> speakers; // objects
		// 'a voice' may be resolved by using information from the next speakerGroup.  But the reader does not yet necessarily know
		// of this speaker.  So subsequent resolving may need to discard this future object bcause it really doesn't exist yet.
		// 
		set <int> fromNextSpeakerGroup; // objects resolved by speakers from the next speaker group
		vector <cOM> replacedSpeakers; // speakers replaced by other speakers (in speakergroup only, so replacedBy is still -1)
		set <int> singularSpeakers; 
		set <int> groupedSpeakers; 
		set <int> povSpeakers; // point-of-view
		set <int> dnSpeakers; // definitely named speakers
		// metaNameOthers is to prevent confusion when names which are not otherwise mentioned outside of quotes seem to be hailed - but only because the name is being talked about
		set <int> metaNameOthers; // people specifically named as being not present - a person being asked someone else's name 'her name is Marguerite'
		vector < cSpeakerGroup> embeddedSpeakerGroups;
		//set <int> embeddedPhysicallyPresent; // focused subjects embedded in a story
		set <int> observers;
		unordered_map < int,int > lastSGSpeakerMap;
		unordered_map < int,bool > pppMap;
		class cGroup
		{
		public:
			int where;
			vector <int> objects;
			cGroup(int w,vector <int> o)
			{
				where=w;
				objects=o;
			}
			bool copy(void *buffer,int &w,int limit) // write
			{
				if (!::copy(buffer,where,w,limit)) return false;
				if (!::copy(buffer,objects,w,limit)) return false;
				return true;
			}
			cGroup(char *buffer,int &w,unsigned int limit,bool &error)
			{
				if (error=!::copy(where,buffer,w,limit)) return;
				if (error=!::copy(objects,buffer,w,limit)) return;
				error=w>(signed)limit;
			}
		};
		vector < cGroup > groups; // subgroups of speakers grouped syntactically
		cSpeakerGroup(void);
		cSpeakerGroup(char *buffer,int &where,unsigned int limit,bool &error);
		bool copy(void *buffer,int &where,int limit);
		void clear(void)
		{
			sgBegin=sgEnd=section=previousSubsetSpeakerGroup=saveNonNameObject=conversationalQuotes=-1;
			speakersAreNeverGroupedTogether=false; 
			speakers.clear(); 
			fromNextSpeakerGroup.clear(); // objects resolved by speakers from the next speaker group
			replacedSpeakers.clear(); // speakers replaced by other speakers (in speakergroup only, so replacedBy is still -1)
			singularSpeakers.clear(); 
			groupedSpeakers.clear(); 
			povSpeakers.clear(); // point-of-view
			dnSpeakers.clear(); // definitely named speakers
			metaNameOthers.clear(); // people specifically named as being not present - a person being asked someone else's name 'her name is Marguerite'
			embeddedSpeakerGroups.clear();
			observers.clear();
			groups.clear(); // subgroups of speakers grouped syntactically
		}
		void removeSpeaker(int s)
		{
			speakers.erase(s);
			fromNextSpeakerGroup.erase(s); 
			singularSpeakers.erase(s); 
			groupedSpeakers.erase(s); 
			povSpeakers.erase(s); 
			dnSpeakers.erase(s);
			metaNameOthers.erase(s);
			observers.erase(s);
		}
	};
	const wchar_t *toText(cSpeakerGroup &sg,wstring &tmpstr)
	{
		wchar_t temp[128];
		_snwprintf(temp,128,L"%d-%d [section %03d] %d: ",sg.sgBegin,sg.sgEnd,sg.section,sg.conversationalQuotes);
		tmpstr=temp;
		wstring temp2,temp3;
		tmpstr+=objectString(sg.speakers,temp2)+L" ";
		if (sg.replacedSpeakers.size())
			tmpstr+=L"replacedSpeakers ("+objectString(sg.replacedSpeakers,temp2,true)+L") ";
		if (sg.singularSpeakers.size())
			tmpstr+=L"singularSpeakers ("+ objectString(sg.singularSpeakers,temp2)+L") ";
		if (sg.groupedSpeakers.size())
			tmpstr+=L"groupedSpeakers ("+objectString(sg.groupedSpeakers,temp2)+L") ";
		if (sg.povSpeakers.size())
			tmpstr+=L"povSpeakers ("+objectString(sg.povSpeakers,temp2)+L") ";
		if (sg.dnSpeakers.size())
			tmpstr+=L"dnSpeakers ("+objectString(sg.dnSpeakers,temp2)+L") ";
		if (sg.metaNameOthers.size())
			tmpstr+=L"metaNameOthers ("+objectString(sg.metaNameOthers,temp2)+L") ";
		if (sg.observers.size())
			tmpstr+=L"observers ("+objectString(sg.observers,temp2)+L") ";
		for (unsigned int I=0; I<sg.embeddedSpeakerGroups.size(); I++)
			tmpstr+=L"ESG#"+itos(I,temp2)+L"("+toText(sg.embeddedSpeakerGroups[I],temp3)+L") ";
		for (unsigned int I=0; I<sg.groups.size(); I++)
			tmpstr+=L"group#"+itos(I,temp2)+L"("+objectString(sg.groups[I],temp3)+L") ";
		tmpstr+=(sg.previousSubsetSpeakerGroup<0) ? L"[no Previous Subset SG]": (itos(sg.previousSubsetSpeakerGroup,temp2)+L" ");
		tmpstr+=(sg.saveNonNameObject<0) ? L"[no non-name object]":(itos(sg.saveNonNameObject,temp2)+L" ");
		tmpstr+=(sg.speakersAreNeverGroupedTogether) ? L"[speakers never grouped]":L"[speakers grouped]";
		tmpstr+=(sg.tlTransition) ? L"[transition]":L"[no transition]";
		return tmpstr.c_str();
	}
	wstring sourcePath;
	enum sourceTypeEnum { NO_SOURCE_TYPE,TEST_SOURCE_TYPE,GUTENBERG_SOURCE_TYPE, NEWS_BANK_SOURCE_TYPE,BNC_SOURCE_TYPE,SCRIPT_SOURCE_TYPE,
		WEB_SEARCH_SOURCE_TYPE, WIKIPEDIA_SOURCE_TYPE, INTERACTIVE_SOURCE_TYPE, PATTERN_TRANSFORM_TYPE, REQUEST_TYPE };
	//enum sourceType {NoType,TestType,BookType,NewsBankType,BNCType,ScriptType};
	int sourceType;
	bool sourceInPast;
	int sourceConfidence; // dbPedia/wikipedia sources have higher confidence
	unordered_map <wstring,Source *> sourcesMap;
	vector <cSpeakerGroup> speakerGroups;
	vector <int> povInSpeakerGroups; // keeps track of point-of-view objects throughout text to be dropped into speakerGroups
	vector <int> metaNameOthersInSpeakerGroups; // keeps track of objects named as a third person throughout text to be dropped into speakerGroups
	vector <int> definitelyIdentifiedAsSpeakerInSpeakerGroups; // keeps track of definitely identified speakers to be dropped into speakerGroups
	cSpeakerGroup tempSpeakerGroup; // only maintained during identifySpeakerGroups
	vector <int> nextNarrationSubjects; // only maintained during identifySpeakerGroups (no IS_OBJECTs)
	// lastISNarrationSubjects are IS_OBJECTS that have been accumulated over the last extendable speakerGroup
	vector <int> lastISNarrationSubjects; // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	vector <int> whereLastISNarrationSubjects; // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	// nextISNarrationSubjects are IS_OBJECTS that have been accumulated over the current temporary speakerGroup
	vector <int> nextISNarrationSubjects; // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	vector <int> whereNextISNarrationSubjects; // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	unordered_map <int,int> speakerAges; // associated with tempSpeakerGroup speakers
	vector <int> speakerSections;  // the sourcePosition of each speakerGroup piece encountered in the current speaker group
	vector <int> subjectsInPreviousUnquotedSection;
	int whereSubjectsInPreviousUnquotedSection;
	vector <int> introducedByReference; // objects that are introduced in a group that is non-gendered - The four paintings of Mephistopheles
	// subjectsInPreviousUnquotedSection is used by alternate speaker resolution.  In this case the following flag is not necessary.
	// It is also used by chooseBest with immediate resolution, and in such cases the subjects should only be used when
	// the speaker being resolved is right after the subjectsInPreviousUnquotedSection.
	bool subjectsInPreviousUnquotedSectionUsableForImmediateResolution;
	typedef pair <int, int> tSA;
	unsigned int currentTimelineSegment;
	int currentEmbeddedTimelineSegment;
	unsigned int currentSpeakerGroup;
	int currentEmbeddedSpeakerGroup;
	bool speakerGroupsEstablished;
	bool narrativeIsQuoted; // set only for texts where the entire text is an utterance (tests)
	// relations set
	set <int> nrr; // no repeat relations set

	// begin collectTags global section
	bool tagInFocus(int begin,int end);
	vector <int> collectTagsFocusPositions;
	class costPatternElementByTagSet
	{
	public:
		int position;
		int PEMAPosition;
		int childPEMAPosition; // may be -1, in which case child is not determined or applies to all children
		int tagSet;
		int element;
		int cost;
		int traceSource;
		costPatternElementByTagSet(int P,int PP,int cPP,int ts,int e)
		{
			position=P;
			PEMAPosition=PP;
			childPEMAPosition=cPP;
			tagSet=ts;
			element=e;
			cost=10000000;
			traceSource=-1;
		}
		bool operator == (const costPatternElementByTagSet& o)
		{
			return position==o.position &&
				PEMAPosition==o.PEMAPosition &&
				childPEMAPosition==o.childPEMAPosition &&
				tagSet==o.tagSet &&
				element==o.element &&
				cost==o.cost;
		}
		bool operator != (const costPatternElementByTagSet& o)
		{
			return position!=o.position ||
				PEMAPosition!=o.PEMAPosition ||
				childPEMAPosition!=o.childPEMAPosition ||
				tagSet!=o.tagSet ||
				element!=o.element ||
				cost!=o.cost;
		}
	};
	void identifyConversations();

	vector <costPatternElementByTagSet> secondaryPEMAPositions;
	int beginTime,timerForExit,desiredTagSetNum; // beginTime,timerForExit monitor time for collectTags
	bool exitTags,blocking,focused;
	sTrace debugTrace;
	// end collectTags global section

	int gTraceSource;
	bool lowestContainingPatternElement(int nextPatternElementPEMAPosition,int element,vector <int> &lowestCostPEMAPositions);
	bool tagSetAllIn(vector <costPatternElementByTagSet> &PEMAPositions,int I);
	void setChain(vector <patternElementMatchArray::tPatternElementMatch *> chainPEMAPositions,vector <costPatternElementByTagSet> &PEMAPositions,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int &traceSource,int &minOverallChainCost);
	void findAllChains(vector <costPatternElementByTagSet> &PEMAPositions,int PEMAPosition,int position,vector <patternElementMatchArray::tPatternElementMatch *> &chain,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int totalCost,int &traceSource,int &minOverallChainCost);
	void setChain2(vector <patternElementMatchArray::tPatternElementMatch *> &chainPEMAPositions,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int deltaCost);
	void findAllChains2(int PEMAPosition,int position,vector <patternElementMatchArray::tPatternElementMatch *> &chain,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int changedPosition,int rootPattern,int len,bool includesPatternMatch,int deltaCost);
	int cascadeUpToAllParents(bool recalculatePMCost,int basePosition,patternMatchArray::tPatternMatch *childPM,int traceSource,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet, wchar_t *fromWhere);
	void recalculateOCosts(bool &recalculatePMCost,vector<patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int start,int traceSource);
	int setSecondaryCosts(vector <costPatternElementByTagSet> &secondaryPEMAPositions,patternMatchArray::tPatternMatch *pm,int basePosition,wchar_t *fromWhere);
	int getEndRelativeSourcePosition(int PEMAPosition);
	void setPreviousElementsCostsAtIndex(vector <costPatternElementByTagSet> &PEMAPositions, int pp, int cost, int traceSource, int patternElementEndPosition, int pattern, int patternElement);
	void lowerPreviousElementCosts(vector <costPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, wchar_t *fromWhere);
	void lowerPreviousElementCostsLowerRegardlessOfPosition(vector <costPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, wchar_t *fromWhere);
	void lowerPreviousElementCostsOld(vector <costPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, wchar_t *fromWhere);
	bool assessEVALCost(tTagLocation &tl,int pattern,patternMatchArray::tPatternMatch *pm,int position);
	int assessCost(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,vector < vector <tTagLocation> > &tagSets);
	int eliminateLoserPatterns(unsigned int begin,unsigned int end);
	enum prepSetEnum { PREP_PREP_SET,PREP_OBJECT_SET,PREP_VERB_SET };
	void setRelPrep(int where,int relPrep,int fromWhere,int setType);
	wstring lastNounNotFound,lastVerbNotFound;

	void printObjects(void);
	void printSectionStatistics(void);
	void printResolutionCheck(vector <int> &badSpeakers);
	bool isSpeaker(int where,int esg,int tempCSG);
	void evaluateSpaceRelation(int where,int endSpeakerGroup,int &spri);
	void analyzeWordSenses(void);
	void printVerbFrequency();
	// speaker resolution
	void checkObject(vector <cObject>::iterator o);
	bool eraseWinnerFromRecalculatingAloneness(int I,patternMatchArray::tPatternMatch *pma);
	void removeWinnerFlag(int where, patternMatchArray::tPatternMatch *pma,int recursionSpaces);
	bool isAnySeparator(int where);
	bool addCostFromRecalculatingAloneness(int where,patternMatchArray::tPatternMatch *pma);
	void identifyObjects(void);
	int scanForSpeakers(int begin,int end,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,unsigned __int64 roleFlag);
	bool genderedBodyPartSubstitution(int where,int &speakerObject);
	void clearNextSection(int I,int section);
	void processNextSection(int I,int section);
	bool quoteIndependentAge,objectToBeMatchedInQuote;
	void ageSpeaker(int where,bool inPrimaryQuote,bool inSecondaryQuote,vector <cLocalFocus>::iterator &lfi,int amount);
	void ageSpeaker(int where,vector <cLocalFocus>::iterator &lfi,int amount);
	void ageSpeakerWithoutSpeakerInfo(int where,bool inPrimaryQuote,bool inSecondaryQuote,vector <cLocalFocus>::iterator &lfi,int amount);
	bool followObjectChain(int &o);
	bool mergableBySex(int o,set <int> &objects,bool &uniquelyMergable,set<int>::iterator &mergedObject);
	bool unMergable(int where,int o,vector <int> &speakers,bool &uniquelyMergable,bool insertObject,bool crossedSection,bool allowBothToBeSpeakers,bool checkUnmergableSpeaker,vector <int>::iterator &save);
	bool unMergable(int where,int o,set <int> &objects,bool &uniquelyMergable,bool insertObject,bool crossedSection,bool allowBothToBeSpeakers,bool checkUnmergableSpeaker,set <int>::iterator &save);
	void mergeName(int where,int &o,set <int> &speakers);
	void replaceSpeaker(int begin,int end,int fromObject,int toObject);
	void subtract(int o,vector <cOM> &objects);
	void subtract(set <int> &bigSet,set <int> &subtractSet,set <int> &resultSet);
	void subtract(vector <cOM> &bigSet,vector <cOM> &subtractSet);
	void subtract(vector <cOM> &bigSet,vector <int> &subtractSet);
	void subtract(vector <cOM> &bigSet,set <int> &subtractSet);
	int getLastSpeakerGroup(int o,int lastSG);
	void determineSubgroupFromGroups(cSpeakerGroup &sg);
	void determinePreviousSubgroup(int where,int whichSG,cSpeakerGroup *lastSG);
	void determineSpeakerRemoval(int where);
	void eliminateSpuriousHailSpeakers(int begin,int end,cSpeakerGroup &sg,bool speakerGroupCrossesSectionBoundary);
	int detectUnresolvableObjectsResolvableThroughSpeakerGroup(void);
	bool createSpeakerGroup(int begin,int end,bool endOfSection,int &lastSpeakerGroup);
	bool anyAcceptableLocations(int where,int object);
	int atBefore(int object,int where);
	int locationBefore(int object,int where);
	int ppLocationBetween(int object,int min,int max);
	int locationBeforeExceptFuture(int object,int where);
	int locationBefore(int object,int where,bool plural);
	int locationBefore(int object,int where,bool plural,bool notInQuestion);
	bool setPOVStatus(int where,bool inPrimaryQuote,bool inSecondaryQuote);
	bool notPhysicallyPresentByMissive(int where);
	bool isFocus(int where,bool inPrimaryQuote,bool inSecondaryQuote,int o,bool &isNotPhysicallyPresent,bool subjectAllowPrep);
	bool mergeFocus(bool inPrimaryQuote,bool inSecondaryQuote,int o,int where,vector <int> &lastSubjects,bool &clearBeforeSet);
	void mergeObjectIntoSpeakerGroup(int I,int speakerObject);
	bool isEOS(int where);
	void translateBodyObjects(cSpeakerGroup &sg);
	int detectMetaResponse(int I,int element);
	bool skipMetaResponse(int &I);
	void associateNyms(int where);
	void associatePossessions(int where);
	void moveNyms(int where,int toObject,int fromObject,wchar_t *fromWhere);
	bool implicitObject(int where);
	void distributePOV(void);
	bool invalidGroupObjectClass(int oc);
	void accumulateGroups(int where,vector <int> &groupedObjects,int &lastWhereMPluralGroupedObject);
	bool sameSpeaker(int sWhere1,int sWhere2);
	vector <int> subNarratives;
	void embeddedStory(int where,int &numPastSinceLastQuote,int &numNonPastSinceLastQuote,int &numSecondInQuote,
int &numFirstInQuote,
													 int &lastEmbeddedStory,int &lastEmbeddedImposedSpeakerPosition,int lastSpeakerPosition);
	void adjustHailRoleDuringScan(int where);
	void adjustToHailRole(int where);
	void syntacticRelations();
	bool replaceSubsequentMatches(set <int> &so,int sgEnd);
	bool replaceAliasesAndReplacements(set <int> &objects);
	bool eraseAliasesAndReplacementsInSpeakerGroup(vector <cSpeakerGroup>::iterator sg,bool eraseSubsequentMatches);
	void eraseAliasesAndReplacementsInEmbeddedSpeakerGroups	(void);
	void eraseAliasesAndReplacementsInSpeakerGroups(void);
	bool blockSpeakerGroupCreation(int endSection,bool quotesSeenSinceLastSentence,int nsAfter);
	bool rejectTimeWord(int where,int begin);
	bool resolveTimeRange(int where,int pmaOffset,vector <cSpaceRelation>::iterator csr);
	bool stopSearch(int I);
	void distributeTimeRelations(vector <cSpaceRelation>::iterator csr,vector <cSpaceRelation>::iterator previousRelation,int conjunctionPassed);
	void appendTime(vector <cSpaceRelation>::iterator csr);
	void detectTimeTransition(int where,vector <int> &lastSubjects);
	void copyTimeInfoNum(vector <cTimeInfo>::iterator previousTime,cTimeInfo &t,int num);
	void markTime(int where,int begin,int len);
	bool evaluateTimePattern(int beginObjectPosition,int &maxLen,cTimeInfo &t,cTimeInfo &rt,bool &rtSet);
	bool identifyDateTime(int where,vector <cSpaceRelation>::iterator csr,int &maxLen,int inMultiObject);
	bool detectTimeTransition(int where,vector <cSpaceRelation>::iterator csr,cTimeInfo &t);
	bool evaluateHOUR(int where,cTimeInfo &t);
	bool evaluateDateTime(vector <tTagLocation> &tagSet,cTimeInfo &t,cTimeInfo &rt,bool &rtSet);
	bool ageTransition(int where,bool timeTransition,bool &transitionSinceEOS,int duplicateFromWhere,int exceptWhere,vector <int> &lastSubjects,wchar_t *fromWhere);
	int primaryLocationLastMovingPosition,primaryLocationLastPosition;
	bool like(wstring str1,wstring str2);

	// where
	void getMaxWhereSR(vector <cSpaceRelation>::iterator csr,int &begin,int &end);
	vector <cSpaceRelation>::iterator findSpaceRelation(int where);
	const wchar_t *src(int where,wstring description,wstring &tmpstr);
	bool followerPOVToObserverConversion(vector <cSpaceRelation>::iterator sr,int sg);
	bool setTimeFlowTense(int where,int whereControllingEntity,int whereSubject,int whereVerb,int whereObject,
int wherePrepObject,
		int prepObjectSubType,int objectSubType,bool establishingLocation,bool futureLocation,bool genderedLocationRelation,cTimeFlowTense &tft);
	void srSetTimeFlowTense(int spri);
	bool isRelativeLocation(wstring word);
	void newSR(int where,int o,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int at,int whereMovingRelativeTo,int relationType,const wchar_t *whereType,bool physicalRelation);
	bool moveIdentifiedSubject(int where,bool inPrimaryQuote,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int at,int whereMovingRelativeTo,int spaceRelation,const wchar_t *whereType,bool physicalRelation);
	int findAnyLocationPrepObject(int whereVerb,int &wherePrep,bool &location,bool &timeUnit);
	bool rejectPrepPhrase(int wherePrep);
	bool adverbialPlace(int where);
	bool adjectivalExit(int where);
	bool whereSubType(int where);
	bool exitConversion(int whereObject,int whereSubject,int wherePrepObject);
	bool placeIdentification(int where,bool inPrimaryQuote,int whereControllingEntity,int whereSubject,int whereVerb,int vnClass);
	bool srMoveObject(int where,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int at,int whereMovingRelativeTo,int spaceRelation,wchar_t *whereType,bool physicalRelation);
	void defineObjectAsSpatial(int where);
	void evaluateMetaWhereQuery(int where,bool inPrimaryQuote,int &currentMetaWhereQuery);
	void srd(int where,wstring spd,wstring &description);
	wstring wsrToText(int where,wstring &description);
	wstring srToText(int &spr,wstring &description);
	void cancelSubType(int object);
	unordered_map <wstring, set <int> >::iterator getVerbClasses(int where,wstring &verb);
	wstring getBaseVerb(int where,int fromWhere,wstring &verb);
	bool isVerbClass(int where,int verbClass);
	bool isControlVerb(int where);
	bool isSpecialVerb(int where,bool moveOnly);
	bool isPhysicalActionVerb(int where);
	bool isSelfMoveVerb(int where,bool &exitOnly);
	bool isVerbClass(int where,wstring verbClass);
	bool isNounClass(int where,wstring group);
	bool locationMatched(int where);
	void detectSpaceLocation(int where,int lastBeginS1);
	bool isSpeakerContinued(int where,int o,int lastWherePP,bool &sgOccurredAfter,bool &audienceOccurredAfter,bool &speakerOccurredAfter);
	bool isSpatialSeparation(int whereVerb);
	void processExit(int where,vector <cSpaceRelation>::iterator sri,int backInitialPosition,vector <int> &lastSubjects);
	void detectSpaceRelation(int where,int backInitialPosition,vector <int> &lastSubjects);
	void logSpaceCheck(void);

	int getSpeakersToKeep(vector<cSpaceRelation>::iterator sr);
	void identifySpeakerGroups();
	void substituteGenderedObject(int where,int &object,set <int> &speakers);
	void mergeFocusResolution(int o,int I,vector <int> &lastSubjects,bool &clearBeforeSet);
	void preferSubgroupMatch(int where,int objectClass,int inflectionFlags,bool inQuote,int speakerObject,bool restrictToAlreadyMatched);
	bool matchNewHail(int where,vector <cSpeakerGroup>::iterator sg,int o);
	void accumulateSubjects(int I,int o,bool inPrimaryQuote,bool inSecondaryQuote,int &whereSubject,bool &accumulateMultipleSubjects,vector <int> &lastSubjects);
	bool isAgentObject(int object);
	bool hasAgentObjectOwner(int where,int &ownerWhere);
	void checkInfinitivePhraseForLocation(vector <tTagLocation> &tagSet,bool locationTense);
	unordered_map <wstring,int> prepTypesMap;
	void preparePrepMap(void);
	int getMovementPrepType(tIWMM prepWord);
	void accumulateLocation(int where,vector <tTagLocation> &tagSet,int subjectObject,bool locationTense);
	int determineSpeaker(int beginQuote,int endQuote,bool primary,bool noSpeakerAfterward,bool &definitelySpeaker,int &audienceObjectPosition);
	int accumulateLocationLastLocation;
	void accumulateAdjective(const wstring &fromWord,set <wstring> &words,vector <tIWMM> &validList,bool isAdjective,wstring &aa,bool &containsMale,bool &containsFemale);
	map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > wnSynonymsNounMap,wnSynonymsAdjectiveMap,wnAntonymsAdjectiveMap,wnAntonymsNounMap;
	map <tIWMM,int,tFI::cRMap::wordMapCompare> wnGenderAdjectiveMap,wnGenderNounMap;
	int readWNMap(map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &m,void *buffer,int &where,int bufferlen);
	bool readWNMaps(wstring path);
	bool writeWNMap(map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &m,void *buffer,int &where,int fd,int limit);
	bool writeWNMaps(wstring path);
	int readGWNMap(map <tIWMM,int,tFI::cRMap::wordMapCompare > &m,void *buffer,int &where,int bufferlen);
	void readGWNMaps(wstring path);
	bool writeGWNMap(map <tIWMM,int,tFI::cRMap::wordMapCompare > &m,void *buffer,int &where,int fd,int limit);
	void writeGWNMaps(wstring path);
	void fillWNMaps(int where,tIWMM word,bool isAdjective);
	bool WNMapsInitialized;
	void addWNExtensions(void);
	void addDefaultGenderedAssociatedNouns(int o);
	void accumulateAdjectives(int where);

	// question processing
	unordered_map <int,int> questionSubjectAgreementMap;  // maps the first subject occurring in the next nonquotedParagraph to the question before it
	bool questionAgreement(int where,int whereFirstSubjectInParagraph,int questionSpeakerLastParagraph,vector <cOM> &objectMatches,bool &subjectDefinitelyResolved,bool audience,wchar_t *fromWhere);
	void setSecondaryQuestion(vector <WordMatch>::iterator im);
	void setQuestion(vector <WordMatch>::iterator im,bool inQuote,int &questionSpeakerLastSentence,int &questionSpeaker,bool &currentIsQuestion);
	void correctBySpeakerInversionIfQuestion(int where,


					int whereFirstSubjectInParagraph);
	bool testQuestionType(int where,int &whereQuestionType,int &whereQuestionTypeFlags,int setType,set <int> &whereQuestionInformationSourceObjects);
	void processQuestion(int whereVerb,int whereReferencingObject,__int64 &questionType,int &whereQuestionType,set <int> &whereQuestionInformationSourceObjects);
	enum qtf { 
		unknownQTFlag = 1, whichQTFlag = 2, whereQTFlag = 3, whatQTFlag = 4, whoseQTFlag = 5, howQTFlag = 6, whenQTFlag = 7, whomQTFlag = 8, whyQTFlag = 9, wikiBusinessQTFlag=10, wikiWorkQTFlag=11, typeQTMask = (1 << 4) - 1,
		referencingObjectQTFlag=1<<4,subjectQTFlag=2<<4,objectQTFlag=3<<4,secondaryObjectQTFlag=4<<4,prepObjectQTFlag=5<<4,
		QTAFlag=1<<8
	};


	void resolveQuotedPOVObjects(int lastOpeningPrimaryQuote,int lastClosingPrimaryQuote);
	void setEmbeddedStorySpeaker(int where,int &lastDefiniteSpeaker);
	void ageIntoNewSpeakerGroup(int where);
	void resetObjects(void);
	bool setSecondaryQuoteString(int I,vector <int> &secondaryQuotesResolutions);
	// letters
	bool detectLetterAsObject(int where,int &lastLetterBegin);
	int letterDetectionBegin(int where,int &whereLetterTo,int &lastLetterBegin);
	bool letterDetectionEnd(int where,int whereLetterTo,int lastLetterBegin);

	void resolveMetaReference(int speakerPosition,int quotePosition,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	void insertSpaceRelations();
	void resolveSpeakers(vector <int> &secondaryQuotesResolutions);
	void resolveFirstSecondPersonPronouns(vector <int> &secondaryQuotesResolutions);
	void printTenseStatistic(cTenseStat &tenseStatistics,int sense,int numTotal);
	void printTenseStatistics(wchar_t *fromWhere,cTenseStat tenseStatistics[],int numTotal);
	void printTenseStatistics(wchar_t *fromWhere, unordered_map <int,cTenseStat> &tenseStatistics,int numTotal);
	bool determineTimelineSegmentLink();
	bool speakerGroupTransition(int where,int newSG,bool forward);
	void initializeTimelineSegments(void);
	void createTimelineSegment(int where);
	bool adjustWord(unsigned int q);
	void adjustWords(void);
	void eraseLastQuote(int &lastQuote,tIWMM quoteCloseWord,unsigned int &q);
	bool testConversionToDoubleQuotes();
	unsigned int doQuotesOwnershipAndContractions(unsigned int &quotations,bool newsBank);
	int reportUnmatchedElements(int begin,int end,bool logElements);
	void clearTagSetMaps(void);
	int readWordIndexesForWordRelationsRefresh(vector <int> &wordIds);
	int WRMemoryCheck();
	int refreshWordRelations();

	class testWordRelation
	{
	public:
		int id;
		int sourceId;
		int lastWhere;
		int fromWordId;
		int toWordId;
		int totalCount;
		int typeId;
		testWordRelation(int _id, int _sourceId, int _lastWhere, int _fromWordId, int _toWordId, int _totalCount, int _typeId)
		{
			id = _id;
			sourceId = _sourceId;
			lastWhere = _lastWhere;
			fromWordId = _fromWordId;
			toWordId = _toWordId;
			totalCount = _totalCount;
			typeId = _typeId;
		}
		bool operator==(const testWordRelation& tr) {
			return id == tr.id &&
				sourceId == tr.sourceId &&
				lastWhere == tr.lastWhere &&
				fromWordId == tr.fromWordId &&
				toWordId == tr.toWordId &&
				totalCount == tr.totalCount &&
				typeId == tr.typeId;
		}
	};


	int refreshWordRelationsFromSolr(vector <testWordRelation> &testWordRelations,int &overTime,int &solrTime);
	int printSentences(bool updateStatistics,unsigned int unknownCount,unsigned int quotationExceptions,unsigned int totalQuotations,int &globalOverMatchedPositionsTotal);
	int printSentencesCheck(bool skipCheck);
	void printTagSet(int logType,wchar_t *descriptor,int ts,vector <tTagLocation> &tagSet,int position,int PEMAPosition);
	void printTagSet(int logType,wchar_t *descriptor,int ts,vector <tTagLocation> &tagSet,int position,int PEMAPosition,vector <wstring> &words);

	class cAS
	{
	public:
		wstring sourceType;
		wstring rejectAnswer;
		int confidence;
		int matchSum;
		wstring matchInfo;
		Source *source;
		cSpaceRelation* sri;
		int equivalenceClass;
		int ws,wo,wp; // used to differentiate compound nouns
		int identityWith;
		bool finalAnswer;
		bool identicalWithAnswerFromAnotherSource;
		cAS(wstring _sourceType,Source *_source,int _confidence,int _matchSum,wstring _matchInfo,cSpaceRelation* _sri,int _equivalenceClass,int _ws,int _wo,int _wp) 
		{
			sourceType=_sourceType;
			source=_source;
			confidence=_confidence;
			matchSum=_matchSum;
			matchInfo=_matchInfo;
			sri=_sri;
			equivalenceClass=_equivalenceClass;
			ws=_ws;
			wo=_wo;
			wp=_wp;
			identityWith=-1;
			finalAnswer=false;
		}
	};

	// wikipedia
	Source(MYSQL *parentMysql,int _sourceType,int _sourceConfidence);
	void reduceLocalFreebase(wchar_t *path,wchar_t *filename);
	void getObjectString(int where,wstring &object,vector <wstring> &lookForSubject,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
	int getWikipediaPath(int principalWhere,vector <wstring> &wikipediaLinks,wchar_t *path,vector <wstring> &lookForSubject,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
	int evaluateISARelation(int parentSourceWhere,int where,vector <tTagLocation> &tagSet,vector <wstring> &lookForSubject);
	bool getISARelations(int parentSourceWhere,int where,vector < vector <tTagLocation> > &tagSets,vector <int> &OCTypes,vector <wstring> &lookForSubject);
	int getObjectRDFTypes(int object,vector <cTreeCat *> &rdfTypes,unordered_map <wstring ,int > &topHierarchyClassIndexes,wstring fromWhere);
	int getExtendedRDFTypes(int where, vector <cTreeCat *> &rdfTypes, unordered_map <wstring, int > &topHierarchyClassIndexes, wstring fromWhere, bool ignoreMatches=false, bool fileCaching=true);
	class extendedMapType
	{
	public:
		unordered_map <wstring,int> wordAssociationMap;
		vector <cTreeCat *> rdfTypes;
		unordered_map <wstring ,int > topHierarchyClassIndexes;
	};
	unordered_map<wstring, extendedMapType > extendedRdfTypeMap; 
	unordered_map<wstring, int > extendedRdfTypeNumMap; 
	int readExtendedRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes,unordered_map <wstring ,int > &topHierarchyClassIndexes);
	int writeExtendedRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes,unordered_map <wstring ,int > &topHierarchyClassIndexes);
	bool categoryMultiWord(wstring &childWord, wstring &lastWord);
	void getWordAssociationMap(wstring object, vector <cTreeCat *> &rdfTypes, unordered_map<wstring, int> &wordAssociationMap);
	int getAssociationMapMaster(int where, int numWords, unordered_map <wstring, int > &associationMap, wstring fromWhere);
	bool noRDFTypes();
	int getExtendedRDFTypesMaster(int where, int numWords, vector <cTreeCat *> &rdfTypes, unordered_map <wstring, int > &topHierarchyClassIndexes, wstring fromWhere, int extendNumPP = -1, bool fileCaching = true, bool ignoreMatches=false);
	void testWikipedia();
	int getRDFWhereString(int where, wstring &oStr, wchar_t *separator, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool ignoreMatches=false);
	int getRDFTypes(int where, vector <cTreeCat *> &rdfTypes, wstring fromWhere, int extendNumPP = -1, bool ignoreMatches=false, bool fileCaching=true);
	int identifyISARelation(int principalWhere,bool initialTenseOnly);
	bool checkForUppercaseSources(int questionInformationSourceObject);
	bool skipSentenceForUpperCase(unsigned int &sentenceEnd);
	void accumulateSemanticMaps(cSpaceRelation* parentSRI,Source *childSource,bool confidence);
	void accumulateSemanticEntry(unsigned int where,set <cObject::cLocation> &principalObjectLocations,set <cObject::cLocation>::iterator &polIndex,bool confidence,cSpaceRelation* parentSRI,cSemanticMap *semanticMap,unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings);
	bool mixedCaseObject(int begin,int len);
	bool capitalizationCheck(int begin, int len);
	bool rejectISARelation(int principalWhere);
	bool rejectPath(const wchar_t *path);
	int processPath(const wchar_t *path,Source *&source,Source::sourceTypeEnum st,int sourceConfidence,bool parseOnly);
	int identifyISARelationTextAnalysis(int principalWhere,bool parseOnly);
	int determineKindBitFieldFromObject(Source *source,int object,int &wikiBitField);
	int determineKindBitField(Source *source,int where,int &wikiBitField);
	void checkParticularPartSemanticMatchWord(int logType,int parentWhere,bool &synonym,set <wstring> &parentSynonyms,wstring pw,wstring pwme,int &lowestConfidence,unordered_map <wstring ,int >::iterator ami);
	class SemanticMatchInfo
	{
	public:
		bool synonym;
		int semanticMismatch;
		int confidence;
		SemanticMatchInfo(bool synonym,int semanticMismatch,int confidence)
		{
			this->synonym=synonym;
			this->semanticMismatch=semanticMismatch;
			this->confidence=confidence;
		};
		SemanticMatchInfo()
		{
			this->synonym=false;
			this->semanticMismatch=0;
			this->confidence=CONFIDENCE_NOMATCH;
		};
	};
	unordered_map<wstring,SemanticMatchInfo> questionGroupMap;
	int checkParentGroup(int parentWhere,Source *childSource,int childWhere,int childObject,bool &synonym,int &semanticMismatch);
	int checkParticularPartSemanticMatch(int logType,int parentWhere,Source *childSource,int childWhere,int childObject,bool &synonym,int &semanticMismatch);
	int checkParticularPartQuestionTypeCheck(__int64 questionType,int childWhere,int childObject,int &semanticMismatch);
	bool isObjectCapitalized(int where);
	bool ppExtensionAvailable(int where,int &numPPAvailable,bool nonMixed);
	void copySource(Source *childSource,int begin,int end);
	void copySource(cSpaceRelation *originalQuestionSRI,cPattern *originalQuestionPattern,cPattern *constantQuestionPattern, unordered_map <int,int> &sourceMap, unordered_map <wstring, wstring> &parseVariables);
	vector <cSpaceRelation>::iterator copySRI(Source *childSource,vector <cSpaceRelation>::iterator sri);
	int copyDirectlyAttachedPrepositionalPhrase(Source *childSource,int relPrep);
	int copyDirectlyAttachedPrepositionalPhrases(int whereParentObject,Source *childSource,int whereChild);
	void adjustOffsets(int childWhere,bool keepObjects=false);
	int copyChildIntoParent(Source *childSource,int whereChild);
	class cAnswerConfidence
	{
	public:
		int anySemanticMismatch;
		bool subQueryNoMatch;
		int confidence;
	};
	unordered_map <wstring, cAnswerConfidence> childCandidateAnswerMap;
	int	parseSubQueriesParallel(Source *childSource, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer);
	int	matchSubQueries(wstring derivation, Source *childSource, int &semanticMismatch, bool &subQueryNoMatch, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer, int whereChildCandidateAnswerEnd, int numConsideredParentAnswer, int semMatchValue, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int questionTypeCheck(wstring derivation, cSpaceRelation* parentSRI, cAS &childCAS, int &semanticMismatch, bool &unableToDoquestionTypeCheck,bool &subQueryNoMatch, vector <cSpaceRelation> &subQueries, int numConsideredParentAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	void verbTenseMatch(cSpaceRelation* parentSRI, cAS &childCAS, bool &tenseMismatch);
	int semanticMatch(wstring derivation,cSpaceRelation* parentSRI,cAS &childCAS,int &semanticMismatch,bool &subQueryNoMatch,vector <cSpaceRelation> &subQueries,int numConsideredParentAnswer,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	int semanticMatchSingle(wstring derivation,cSpaceRelation* parentSRI,Source *childSource,int whereChild,int childObject,int &semanticMismatch,bool &subQueryNoMatch,
	                        vector <cSpaceRelation> &subQueries,int numConsideredParentAnswer,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	bool checkIdentical(cSpaceRelation* sri,cAS &cas1,cAS &cas2);
	bool checkObjectIdentical(Source *source1,Source *source2,int object1,int object2);
	bool checkParticularPartIdentical(Source *source1,Source *source2,int where1,int where2);
	int getWhereQuestionTypeObject(cSpaceRelation* sri);
	bool processPathToPattern(const wchar_t *path,Source *&source);
	Source *transformSource;
	void initializeTransformations(unordered_map <wstring, wstring> &parseVariables);
	bool detectTransitoryAnswer(cSpaceRelation* sri,cSpaceRelation * &ssri,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	void detectByClausePassive(vector <cSpaceRelation>::iterator sri,cSpaceRelation * &substitute);
	int detectAttachedPhrase(vector <cSpaceRelation>::iterator sri,int &relVerb);
	void detectSubQueries(vector <cSpaceRelation>::iterator sri,vector <cSpaceRelation> &subQueries);
	bool hasProperty(int where,int whereQuestionTypeObject, unordered_map <int,vector < vector <int> > > &wikiTableMap,vector <wstring> &propertyValues);
	void setWhereChildCandidateAnswer(cAS &childCAS, cSpaceRelation* parentSRI);
	void matchAnswersToQuestionType(cSpaceRelation*  sri, vector < cAS > &answerSRIs, int maxAnswer, vector <cSpaceRelation> &subQueries,
		vector <int> &uniqueAnswers,vector <int> &uniqueAnswersPopularity,vector <int> &uniqueAnswersConfidence,
		int &highestPopularity,int &lowestConfidence,int &lowestSourceConfidence,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	bool isModifiedGeneric(cAS &sri);
	int printAnswers(cSpaceRelation*  sri, vector < cAS > &answerSRIs, vector <int> &uniqueAnswers, vector <int> &uniqueAnswersPopularity, vector <int> &uniqueAnswersConfidence,
		               int highestPopularity,int lowestConfidence,int lowestSourceConfidence);
	class Column
	{
	public:
		class CA
		{
		public:
			wstring word;
			int frequency;
			bool queryObjectMatch;
			bool titleObjectMatch;
			CA(wstring w, int f, bool qom = false, bool tom = false)
			{
				word = w;
				frequency = f;
				queryObjectMatch = qom;
				titleObjectMatch = tom;
			}
		};
		struct associationTypeMapCompare
		{
			bool operator()(CA lhs, CA rhs) const
			{
				if (lhs.frequency == rhs.frequency)
					return lhs.word > rhs.word;
				return lhs.frequency>rhs.frequency;
			}
		};
		class Entry
		{
		public:
			Entry(int b, int o, int n)
			{
				begin = b;
				adaptiveWhere = o;
				numWords = n;
				titleFound = 0;
				preferred = false;
				wordMatchesTitle = false;
			};
			int begin;
			int adaptiveWhere; // if an object, this is where the object is declared (not necessarily its beginning), if not an object then adaptiveWhere=begin
			int numWords;
			int titleFound;
			bool wordMatchesTitle;
			bool preferred;
			int queryAssociationsMatched;
			int titleAssociationsMatched;
		};

		vector <vector <Entry> > rows; // there are multiple entries for each row
		int invalidEntries;
		int emptyEntries;
		class AssociationType
		{
		public:
			int frequency;
			bool queryObjectMatch;
			bool titleObjectMatch;
		};
		unordered_map < wstring, AssociationType > commonAssociationTypeMap;
		set < CA, associationTypeMapCompare > mostCommonAssociationTypeSet;
		int numDefinite;
		int numNumerical;
		int numPunctuation;
		int numMaxEntries;
		int gMaxFrequency;
		int mostCommonRatio;
		bool matchedHeader;
		int queryAssociationsMatched;
		int titleAssociationsMatched;
		int preferredRowsFound;
		int rowsWithAnyIdentificationFound;
		int numCommonObjectAssociations;
		int coherencyPercentage;
		bool invalidColumn;
		Column()
		{
			numDefinite = 0;
			numNumerical = 0;
			numPunctuation = 0;
			gMaxFrequency = 1;
			mostCommonRatio = 0;
			numCommonObjectAssociations = 0;
			invalidEntries = 0;
			emptyEntries = 0;
			matchedHeader = false;
			queryAssociationsMatched = 0;
			titleAssociationsMatched = 0;
			preferredRowsFound = 0;
			rowsWithAnyIdentificationFound = 0;
		}
		void removeDomain(wchar_t * domainAssociations[])
		{
			for (int ma = 0; domainAssociations[ma][0]; ma++)
			{
				unordered_map < wstring, AssociationType >::iterator catmi = commonAssociationTypeMap.find(domainAssociations[ma]);
				if (catmi != commonAssociationTypeMap.end())
					catmi->second.frequency = 0;
			}
		}
		bool determineColumnCoherency(Source *wikipediaSource, int title, set <wstring> &titleSynonyms, bool keepMusicDomain, bool keepFilmDomain)
		{
			int sumMaxEntries = 0;
			for (int row = 0; row < rows.size(); row++)
				sumMaxEntries += rows[row].size();
			if (sumMaxEntries / rows.size() > 3) // if there are more than 3 entries, the number of possible combinations is too large to make sure it is actually a valid list.
			{
				lplog(LOG_WHERE, L"table coherency averageEntrySize=%d", sumMaxEntries / rows.size());
				//return false; // TMP DEBUG
			}
			// accumulate all the types of all the entries in the column together into commonAssociationTypeMap
			vector <int> noPreferences;
			accumulateColumnAssociationTypes(wikipediaSource, titleSynonyms, keepMusicDomain, keepFilmDomain, false);
			getMostCommonAssociationTypes(L"BEFORE");
			// prefer the entry in each row of each column that matches with the most common types OR title
			if (!testTitlePreference(wikipediaSource, titleSynonyms))
				setRowPreference(wikipediaSource);
			zeroColumnAssociationTypes();
			// accumulate all the types of ONLY the preferred entries in the table together
			accumulateColumnAssociationTypes(wikipediaSource, titleSynonyms, keepMusicDomain, keepFilmDomain, true);
			if (calculateColumnCoherence(wikipediaSource,title) < 90)
				return true; // TMP DEBUG
			// accumulate all the types of all the entries in the column together
			//getMostCommonAssociationTypes(queryWordAssociationMap, titleWordAssociationMap, L"AFTER");
			return true;
		}
		void accumulateColumnAssociationTypes(Source *wikipediaSource,set <wstring> &titleSynonyms, bool keepMusicDomain, bool keepFilmDomain, bool onlyPreferred)
		{
			wchar_t *musicAssociations[] = { L"single", L"recording", L"music", L"release", L"album", L"" };
			wchar_t *filmAssociations[] = { L"film", L"" };
			for (int row = 0; row < rows.size(); row++)
			{
				for (int entry = 0; entry < rows[row].size(); entry++)
					if (!onlyPreferred || rows[row][entry].preferred)
						accumulateColumnAssociationTypes(wikipediaSource, row, entry, titleSynonyms);
			}
			if (!keepMusicDomain)
				removeDomain(musicAssociations);
			if (!keepFilmDomain)
				removeDomain(filmAssociations);
		}
		void accumulateColumnAssociationTypes(Source *wikipediaSource, int row, int entry, set <wstring> &titleSynonyms)
		{
			rows[row][entry].queryAssociationsMatched = 0;
			rows[row][entry].titleAssociationsMatched = 0;
			unordered_map <wstring, int > wordAssociationMap;
			wikipediaSource->getAssociationMapMaster(rows[row][entry].adaptiveWhere, rows[row][entry].numWords, wordAssociationMap, TEXT(__FUNCTION__));
			for (unordered_map <wstring, int >::iterator ri = wordAssociationMap.begin(), riEnd = wordAssociationMap.end(); ri != riEnd; ri++)
			{
				if (ri->second == 1)
				{
					unordered_map < wstring, AssociationType >::iterator cmi = commonAssociationTypeMap.find(ri->first);
					if (cmi == commonAssociationTypeMap.end())
					{
						commonAssociationTypeMap[ri->first].frequency = 1;
						set <wstring >::iterator wami;
						if (commonAssociationTypeMap[ri->first].titleObjectMatch = (wami = titleSynonyms.find(ri->first)) != titleSynonyms.end())
							rows[row][entry].titleAssociationsMatched++;
					}
					else
						cmi->second.frequency++;
				}
			}
		}
		int getAccumulatedAssociationValue(Source *wikipediaSource, int row, int entry, int &maxFrequency, wstring &maxAssociation)
		{
			unordered_map <wstring, int > wordAssociationMapForEntry;
			wikipediaSource->getAssociationMapMaster(rows[row][entry].adaptiveWhere, rows[row][entry].numWords, wordAssociationMapForEntry, TEXT(__FUNCTION__));
			int accumulatedAssociationValue = 0;
			maxFrequency = 0;
			for (unordered_map <wstring, int >::iterator ri = wordAssociationMapForEntry.begin(), riEnd = wordAssociationMapForEntry.end(); ri != riEnd; ri++)
			{
				if (ri->second == 1)
				{
					unordered_map < wstring, AssociationType >::iterator cmi = commonAssociationTypeMap.find(ri->first);
					if (cmi == commonAssociationTypeMap.end())
						lplog(LOG_FATAL_ERROR, L"association for word %s not found in common column map!", ri->first.c_str());
					accumulatedAssociationValue += cmi->second.frequency;
					if (maxFrequency < cmi->second.frequency)
					{
						maxFrequency = cmi->second.frequency;
						maxAssociation = cmi->first;
					}
				}
			}
			return accumulatedAssociationValue;
		}
		void zeroColumnAssociationTypes()
		{
			commonAssociationTypeMap.clear();
		}
		bool testTitlePreference(Source *wikipediaSource, set <wstring> &titleSynonyms)
		{
			if (titleSynonyms.empty())
				return false;
			// if title exists, and is positively associated with 75% or more of the selected words in each row, make those the preferredEntries.
			for (int row = 0; row < rows.size(); row++)
			{
				int preferredEntriesFound = 0;
				int identificationFound = 0;
				int maxTitleFound = 0;
				int wordMatchesTitleFound = 0;
				for (int entry = 0; entry < rows[row].size(); entry++)
				{
					unordered_map <wstring, int > wordAssociationMapForEntry;
					wikipediaSource->getAssociationMapMaster(rows[row][entry].adaptiveWhere, rows[row][entry].numWords, wordAssociationMapForEntry, TEXT(__FUNCTION__));
					if (wordAssociationMapForEntry.size()>0)
						identificationFound++;
					wstring tmpstr2;
					int titleFound = 0;
					for (unordered_map <wstring, int >::iterator ri = wordAssociationMapForEntry.begin(), riEnd = wordAssociationMapForEntry.end(); ri != riEnd; ri++)
					{
						//if (ri->second == 1)
						{
							tmpstr2 += L" " + ri->first;
							set < wstring >::iterator cmi = titleSynonyms.find(ri->first);
							if (cmi != titleSynonyms.end())
								titleFound++;
						}
					}
					rows[row][entry].titleFound = titleFound;
					if (titleFound > maxTitleFound)
						maxTitleFound = titleFound;
					int lastWord = wikipediaSource->m[rows[row][entry].adaptiveWhere].endObjectPosition - 1;
					if (rows[row][entry].wordMatchesTitle = (titleSynonyms.find(wikipediaSource->m[lastWord].getMainEntry()->first) != titleSynonyms.end()))
						wordMatchesTitleFound++;
					lplog(LOG_WHERE, L"table coherency [%d] row=%d[%d] lastWord=%d[%s] wordMatchesTitle=%s", 
						rows[row][entry].adaptiveWhere, row, entry, lastWord, wikipediaSource->m[lastWord].word->first.c_str(), (rows[row][entry].wordMatchesTitle) ? L"true":L"false");
				}
				for (int entry = 0; entry < rows[row].size(); entry++)
				{
					if (rows[row][entry].preferred=(maxTitleFound>0 && rows[row][entry].titleFound >= maxTitleFound) || rows[row][entry].wordMatchesTitle)
						preferredEntriesFound++;
				}
				if (preferredEntriesFound > 0)
					preferredRowsFound++;
				if (identificationFound > 0)
					rowsWithAnyIdentificationFound++;
				for (int entry = 0; entry < rows[row].size(); entry++)
				{
					if (preferredEntriesFound > 1 && wordMatchesTitleFound>0)
						rows[row][entry].preferred = rows[row][entry].wordMatchesTitle;
					unordered_map <wstring, int > wordAssociationMapForEntry;
					wikipediaSource->getAssociationMapMaster(rows[row][entry].adaptiveWhere, rows[row][entry].numWords, wordAssociationMapForEntry, TEXT(__FUNCTION__));
					wstring tmpstr2,tmpstr4;
					for (unordered_map <wstring, int >::iterator ri = wordAssociationMapForEntry.begin(), riEnd = wordAssociationMapForEntry.end(); ri != riEnd; ri++)
					{
						if (ri->second == 1)
						{
							tmpstr2 += L" '" + ri->first + L"'";
						}
						tmpstr4 += L" '" + ri->first + L"'";
					}
					if (tmpstr2.empty())
						tmpstr2 = tmpstr4;
					wstring tmpstr3;
					wikipediaSource->phraseString(rows[row][entry].begin, rows[row][entry].begin + rows[row][entry].numWords, tmpstr3, false);
					lplog(LOG_WHERE, L"table coherency %s[%d] %s row=%d[%d] associatedWords=%s", tmpstr3.c_str(), rows[row][entry].adaptiveWhere, (rows[row][entry].preferred) ? L"PREFERRED" : L"", row, entry, tmpstr2.c_str());
				}
			}
			lplog(LOG_WHERE, L"title preference: %d entries preferred in %d[%d] rows.", preferredRowsFound, rowsWithAnyIdentificationFound,rows.size());
			return preferredRowsFound > rowsWithAnyIdentificationFound * 3 / 4;
		}
		void setRowPreference(Source *wikipediaSource)
		{
			for (int row = 0; row < rows.size(); row++)
			{
				int preferredEntry = -1;
				int maxOfMaxFrequency = -1;
				int maxAccumulatedAssociationValue = -1;
				wstring maxOfMaxAssociation;
				for (int entry = 0; entry < rows[row].size(); entry++)
				{
					wstring maxAssociation;
					int accumulatedAssociationValue, maxFrequency;
					accumulatedAssociationValue = getAccumulatedAssociationValue(wikipediaSource, row, entry, maxFrequency, maxAssociation);
					if (maxFrequency>maxOfMaxFrequency || (maxFrequency == maxOfMaxFrequency && accumulatedAssociationValue > maxAccumulatedAssociationValue))
					{
						maxAccumulatedAssociationValue = accumulatedAssociationValue;
						preferredEntry = entry;
						maxOfMaxFrequency = maxFrequency;
						maxOfMaxAssociation = maxAssociation;
					}
				}
				if (preferredEntry >= 0)
				{
					rows[row][preferredEntry].preferred = true;
					wstring tmpstr;
					wikipediaSource->phraseString(rows[row][preferredEntry].begin, rows[row][preferredEntry].begin + rows[row][preferredEntry].numWords, tmpstr, false);
					lplog(LOG_WHERE, L"table coherency setRowPreference %s row=%d preferredEntry=%d maxOfMaxFrequency=%d[%s] maxAssociationValue=%d", tmpstr.c_str(),
								row, preferredEntry, maxOfMaxFrequency, maxOfMaxAssociation.c_str(), maxAccumulatedAssociationValue);
				}
			}
		}
		// each preferred entry has two values:
		//   the average of the associationValue
		//   the number of values/the size of the 
		int calculateColumnCoherence(Source *wikipediaSource,int title)
		{
			int sumAssociationValue = 0, sumMaxFrequencies = 0;
			for (int row = 0; row < rows.size(); row++)
			{
				for (int entry = 0; entry < rows[row].size(); entry++)
				{
					if (rows[row][entry].preferred)
					{
						wstring maxAssociation;
						int maxFrequency=0, associationValue;
						associationValue = getAccumulatedAssociationValue(wikipediaSource, row, entry, maxFrequency, maxAssociation);
						if (!maxFrequency && title>=0)
						{
							maxFrequency = rows.size();
							maxAssociation = wikipediaSource->m[title].getMainEntry()->first;
						}
						wstring tmpstr;
						wikipediaSource->phraseString(rows[row][entry].begin, rows[row][entry].begin + rows[row][entry].numWords, tmpstr, false);
						lplog(LOG_WHERE, L"table coherency %s row=%d entry=%d associationValue=%d maxFrequency=%d[%s]", tmpstr.c_str(),
							row, entry, associationValue, maxFrequency, maxAssociation.c_str());
						sumAssociationValue += associationValue;
						sumMaxFrequencies += maxFrequency;
					}
				}
			}
			if (rows.size())
			{
				if (rowsWithAnyIdentificationFound < rows.size() && rowsWithAnyIdentificationFound>0)
					coherencyPercentage = 100 * sumMaxFrequencies / (rowsWithAnyIdentificationFound*rowsWithAnyIdentificationFound);
				else
					coherencyPercentage = 100 * sumMaxFrequencies / (rows.size()*rows.size());
				lplog(LOG_WHERE, L"table coherency coherence=%d%% [%d/%d]", coherencyPercentage, rowsWithAnyIdentificationFound, rows.size());
			}
			return coherencyPercentage;
		}
		void getMostCommonAssociationTypes(wchar_t *when)
		{
			mostCommonAssociationTypeSet.clear();
			numCommonObjectAssociations = 0;
			for (unordered_map < wstring, AssociationType >::iterator cmi = commonAssociationTypeMap.begin(), cmiEnd = commonAssociationTypeMap.end(); cmi != cmiEnd; cmi++)
			{
				if (cmi->second.frequency>rows.size() - 1)
				{
					lplog(LOG_WHERE, L"Processing table [table coherency] %s numRows= %d cmi->first=%s cmi->second=%d",
						when, rows.size(), cmi->first.c_str(), cmi->second);
					numCommonObjectAssociations++;
					mostCommonAssociationTypeSet.insert(CA(cmi->first, cmi->second.frequency, cmi->second.queryObjectMatch, cmi->second.titleObjectMatch));
					queryAssociationsMatched++;
					if (cmi->second.titleObjectMatch)
						titleAssociationsMatched++;
				}
				else if (cmi->second.titleObjectMatch && cmi->second.frequency > rows.size() / 2)
				{
					mostCommonAssociationTypeSet.insert(CA(cmi->first, cmi->second.frequency, false, cmi->second.titleObjectMatch));
					titleAssociationsMatched++;
				}
			}
			lplog(LOG_WHERE, L"Processing table [table coherency] %s numRows= %d numCommonObjectAssociations=%d queryAssociationsMatched=%d titleAssociationsMatched=%d",
				when, rows.size(), numCommonObjectAssociations, queryAssociationsMatched, titleAssociationsMatched);
			if (mostCommonAssociationTypeSet.size() > 0 && rows.size()>0)
				mostCommonRatio = ((mostCommonAssociationTypeSet.begin()))->frequency * 100 / rows.size();
			logColumn(LOG_WHERE,when);
		}
		void logColumn(int logType,wchar_t *when)
		{
			if (rows.size()>2 || (rows.size()>1 && matchedHeader))
				lplog(logType, L"Processing table [table coherency] %s numRows=%d numDefinite=%d maxFrequency=%d mostCommonRatio=%d%% matchedHeader=%s "
											L"table coherency [%d out of %d:%d%%] "
											L"table query associations matched=[%d out of %d:%d%%] "
											L"table title associations matched=[%d out of %d:%d%%]",
							when, (int)rows.size(), numDefinite, gMaxFrequency, mostCommonRatio, (matchedHeader) ? L"true" : L"false",
							numCommonObjectAssociations, (int)commonAssociationTypeMap.size(), (int)(numCommonObjectAssociations * 100 / ((commonAssociationTypeMap.size()) ? commonAssociationTypeMap.size() : 1)), 
							queryAssociationsMatched, numCommonObjectAssociations, queryAssociationsMatched * 100 / ((numCommonObjectAssociations) ? numCommonObjectAssociations : 1),
							titleAssociationsMatched, numCommonObjectAssociations, titleAssociationsMatched * 100 / ((numCommonObjectAssociations) ? numCommonObjectAssociations : 1));
			for (set < CA >::iterator si = mostCommonAssociationTypeSet.begin(), siEnd = mostCommonAssociationTypeSet.end(); si != siEnd; si++)
			{
				lplog(logType, L"Processing table [table coherency] %d:%s [queryMatch=%s,titleMatch=%s]", si->frequency, si->word.c_str(), (si->queryObjectMatch) ? L"true" : L"false", (si->titleObjectMatch) ? L"true" : L"false");
			}
		}
	};
	class SourceTable
	{
	public:
		vector <Column> columns;
		int title;
		int columnHeaderMatchTitle;
		vector <int> headers;
	};

	class WikipediaTableCandidateAnswers
	{
		public:
			vector < SourceTable > wikiQuestionTypeObjectAnswers;
			Source *wikipediaSource;
			WikipediaTableCandidateAnswers(Source *wikipediaSource, vector < SourceTable > wikiQuestionTypeObjectAnswers)
			{
				this->wikipediaSource=wikipediaSource;
				this->wikiQuestionTypeObjectAnswers=wikiQuestionTypeObjectAnswers;
			}
		};
	int searchWebSearchQueries(wchar_t derivation[1024],cSpaceRelation* ssri, unordered_map <int,WikipediaTableCandidateAnswers * > &wikiTableMap,
	                               vector <cSpaceRelation> &subQueries,vector < cAS > &answerSRIs,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,
																 vector <wstring> &webSearchQueryStrings,bool parseOnly,bool answerPluralSpecification,int &finalAnswer,int &maxAnswer);
	int matchBasicElements(bool parseOnly);
	int matchAnswersOfPreviousQuestion(cSpaceRelation *ssri,vector <int> &wherePossibleAnswers);
	int findConstrainedAnswers(vector < cAS > &answerSRIs,vector <int> &wherePossibleAnswers);
	bool compareObjectString(int whereObject1,int whereObject2);
	bool objectContainedIn(int whereObject,set <int> whereObjects);
	bool matchParticularAnswer(cSpaceRelation *ssri,int whereMatch,int wherePossibleAnswer,set <int> &addWhereQuestionInformationSourceObjects);
	bool matchAnswerSourceMatch(cSpaceRelation *ssri,int whereMatch,int wherePossibleAnswer,set <int> &addWhereQuestionInformationSourceObjects);
	static bool matchSourcePositions(Source *parentSource,int parentWhere,Source *childSource,int childWhere,bool &namedNoMatch,bool &synonym,bool parentInQuestionObject,int &semanticMismatch,sTrace debugTrace);

	void setForms(void);
	intArray sentenceStarts;
	vector < vector<cObject>::iterator > masterSpeakerList;
	bool usePIS; // use preIdentifiedSpeakerObjects
	class cSection
	{
	public:
		unsigned int begin,endHeader;
		int speakersMatched,speakersNotMatched,counterSpeakersMatched,counterSpeakersNotMatched;
		vector <cOM> definiteSpeakerObjects,speakerObjects,objectsSpokenAbout,objectsInNarration;
		intArray subHeadings;
		set <int> preIdentifiedSpeakerObjects; // temporary - used before identifySpeakers
		cSection(int inBegin,int inEndHeader) { begin=inBegin; endHeader=inEndHeader; speakersMatched=speakersNotMatched=counterSpeakersMatched=counterSpeakersNotMatched=0; }
		cSection(char *buffer,int &where,unsigned int total,bool &error)
		{
			if (error=!copy(begin,buffer,where,total)) return;
			if (error=!copy(endHeader,buffer,where,total)) return;
			if (error=!copy(subHeadings,buffer,where,total)) return;
			if (error=where>=(int)total) return;
			unsigned int count;
			if (error=!copy(count,buffer,where,total)) return;
			for (unsigned int I=0; I<count && where<(int)total; I++)
				definiteSpeakerObjects.push_back(cOM(buffer,where,total));
			if (error=where>=(int)total) return;
			if (error=!copy(count,buffer,where,total)) return;
			for (unsigned int I=0; I<count && where<(int)total; I++)
				speakerObjects.push_back(cOM(buffer,where,total));
			if (error=where>=(int)total) return;
			if (error=!copy(count,buffer,where,total)) return;
			for (unsigned int I=0; I<count && where<(int)total; I++)
				objectsSpokenAbout.push_back(cOM(buffer,where,total));
			if (error=where>=(int)total) return;
			if (error=!copy(count,buffer,where,total)) return;
			for (unsigned int I=0; I<count && where<(int)total; I++)
				objectsInNarration.push_back(cOM(buffer,where,total));
			if (error=where>=(int)total) return;
			error=false;
			speakersMatched=speakersNotMatched=counterSpeakersMatched=counterSpeakersNotMatched=0;
		}
		bool write(void *buffer,int &where,int limit)
		{
			if (!copy(buffer,begin,where,limit)) return false;
			if (!copy(buffer,endHeader,where,limit)) return false;
			if (!copy(buffer, subHeadings, where, limit)) return false;
			unsigned int count=definiteSpeakerObjects.size();
			if (!copy(buffer,count,where,limit)) return false;
			for (vector <cOM>::iterator mi=definiteSpeakerObjects.begin(),miEnd=definiteSpeakerObjects.end(); mi!=miEnd; mi++)
				mi->write(buffer,where,limit);
			count=speakerObjects.size();
			if (!copy(buffer,count,where,limit)) return false;
			for (vector <cOM>::iterator mi=speakerObjects.begin(),miEnd=speakerObjects.end(); mi!=miEnd; mi++)
				mi->write(buffer,where,limit);
			count=objectsSpokenAbout.size();
			if (!copy(buffer,count,where,limit)) return false;
			for (vector <cOM>::iterator mi=objectsSpokenAbout.begin(),miEnd=objectsSpokenAbout.end(); mi!=miEnd; mi++)
				mi->write(buffer,where,limit);
			count=objectsInNarration.size();
			if (!copy(buffer,count,where,limit)) return false;
			for (vector <cOM>::iterator mi=objectsInNarration.begin(),miEnd=objectsInNarration.end(); mi!=miEnd; mi++)
				mi->write(buffer,where,limit);
			return true;
		}
	};
	unsigned int section; // current section during identifySpeakers
	vector <cSection> sections;
	int speakersMatched,speakersNotMatched,counterSpeakersMatched,counterSpeakersNotMatched;
	wstring storageLocation;
	int lastSourcePositionSet;
	int makeRelationHash(int num1,int num2,int num3);
	class cRelationHistory
	{
	public:
		tFI::cRMap::tIcRMap r;
		int toWhere;
		int narrativeNum;
		int subject;
		int flags; // 0 if physically present, 1 if not
		cRelationHistory(tFI::cRMap::tIcRMap _r,int _toWhere,int _narrativeNum,int _subject,int _flags)
		{
			r=_r;
			toWhere=_toWhere;
			narrativeNum=_narrativeNum;
			subject=_subject;
			flags=_flags;
		}
	};
	wstring objectString(int object,wstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false);
	vector <cRelationHistory> relationHistory;
	class cMultiRelationHistory
	{
	public:
		int narrativeNum;
		int subject;
		cMultiRelationHistory(int _narrativeNum,int _subject)
		{
			narrativeNum=_narrativeNum;
			subject=_subject;
		}
	};
	vector <cMultiRelationHistory> multiRelationHistory;
	vector <int> delayedWordRelations;  // stores relations that should be delayed from being recorded in the syntactic phase because speakers are not resolved
	vector <int> delayedMultiWordRelations;  // stores relations that should be delayed from being recorded in the syntactic phase because speakers are not resolved
	void addDelayedMultiWordRelations(int whereSubject,int whereVerb,int whereObject, int wherePrep, int wherePrepObject);
	void addDelayedWordRelations(int where,int fromWhere,int toWhere,int relationType);
	void createProbableRelationsList();
	void reportProbableRelationsAccuracy();
	void resolveWordRelations();
	tFI::cRMap::tIcRMap addRelations(int where,tIWMM from,tIWMM to,int relationType);
	// multiWordStrings is read once from txt files in Source initialization.
	vector < vector < tmWS > > multiWordStrings;
	// after each readWithLock (after each book adds more words)
	// the multiWordStrings vector is scanned and any more objects which have all the words defined are moved to the multiWordObjects array,
	//   and the entry is removed from the multiWordStrings array
	vector < vector < vector <tIWMM> > > multiWordObjects;

	// MYSQL database
	MYSQL mysql;
	bool alreadyConnected;
	int sourceId;
	int writeSource();
	int getNumSources(enum Source::sourceTypeEnum st, bool left);
	int createDatabase(wchar_t *server);
	int insertWordRelationTypes(void);
	bool signalBeginProcessingSource(int sourceId);
	bool signalFinishedProcessingSource(int sourceId);
	bool updateSourceEncoding(int readBufferType, wstring sourceEncoding, wstring etext);
	bool updateSourceStart(wstring &start, int repeatStart, wstring &etext, __int64 actualLenInBytes);
	bool updateSource(wstring &path,wstring &start,int repeatStart,wstring &etext,int actualLenInBytes);
	bool getNextUnprocessedSource(int begin, int end, enum Source::sourceTypeEnum st, bool setUsed, int &id, wstring &path, wstring &encoding, wstring &start, int &repeatStart, wstring &etext, wstring &author, wstring &title);
	bool anymoreUnprocessedForUnknown(enum Source::sourceTypeEnum st, int step);
	bool getNextUnprocessedParseRequest(int &prId, wstring &pathInCache);
	int createThesaurusTables(void);
	int createGroupTables(void);
	int writeThesaurusEntry(sDefinition &d);
	bool resetAllSource(void);
	bool resetSource(int beginSource,int endSource);
	bool unlockTables(void);
	void resetProcessingFlags(void);
	void updateSourceStatistics(int numSentences, int matchedSentences, int numWords, int numUnknown,
		int numUnmatched,int numOvermatched, int numQuotations, int quotationExceptions, int numTicks, int averagePatternMatch);
	void updateSourceStatistics2(int sizeInBytes, int numWordRelations);
	void updateSourceStatistics3(int numMultiWordRelations);
	void logPatternChain(int sourcePosition,int insertionPoint,enum patternElementMatchArray::chainType patternChainType);
	int	flushWordRelationsHistory(MYSQL &mysql);	
	void printCAS(wstring logPrefix,cAS &childCAS);
	void printSRI(wstring logPrefix,cSpaceRelation* sri,int s,int ws,int wo,int ps,bool overWrote,int matchSum,wstring matchInfo,int logDestination=LOG_WHERE);
	void printSRI(wstring logPrefix,cSpaceRelation* sri,int s,int ws,int wo,wstring ps,bool overWrote,int matchSum,wstring matchInfo,int logDestination=LOG_WHERE);
	int checkInsertPrep(set <int> &relPreps,int wp,int wo);
	void getAllPreps(cSpaceRelation* sri,set <int> &relPreps,int wo=-1);
	int flushMultiWordRelations(set <int> &objects);
	int testStartCode();
	int initializeNounVerbMapping(void);
	void getSynonyms(wstring word, set <wstring> &synonyms, int synonymType);
	void getSynonyms(wstring word, vector <set <wstring> > &synonyms, int synonymType);
	void getWordNetSynonymsOnly(wstring word, vector <set <wstring> > &synonyms, int synonymType);
	
	// tense statistics
	cTenseStat narratorTenseStatistics[NUM_SIMPLE_TENSE];
	cTenseStat speakerTenseStatistics[NUM_SIMPLE_TENSE];
	unordered_map <int,cTenseStat> narratorFullTenseStatistics;
	unordered_map <int,cTenseStat> speakerFullTenseStatistics;
	int lastSense;
	int numTotalNarratorFullVerbTenses,numTotalNarratorVerbTenses;
	int numTotalSpeakerFullVerbTenses,numTotalSpeakerVerbTenses;
	cLastVerbTenses lastVerbTenses[VERB_HISTORY];
	void reduceParents(int position,vector <unsigned int> &insertionPoints,vector <int> &reducedCosts);
	vector <cSpaceRelation> spaceRelations;
	wstring phraseString(int where,int end,wstring &logres,bool shortFormat,wchar_t *separator=L" ");
	wstring whereString(int where,wstring &logres,bool shortFormat,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases,wchar_t *separator,int &numWords);
	wstring whereString(int where,wstring &logres,bool shortFormat);
	wstring whereString(vector <int> &where,wstring &logres);
	wstring whereString(set <int> &where,wstring &logres);
	wstring objectString(vector <cObject>::iterator object,wstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false);
	wstring objectString(vector <cOM> &oms,wstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false);
	wstring objectSortedString(vector <cOM> &objects,wstring &logres);
	wstring objectString(set <int> &objects,wstring &logres,bool shortNameFormat=true);
	wstring objectString(vector <int> &objects,wstring &logres);
	wstring objectString(cSpeakerGroup::cGroup &group,wstring &logres);
	wstring objectSortedString(vector <int> &objects,wstring &logres);
	wstring objectString(cOM om,wstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false);
	wstring wordString(vector <tIWMM> &words,wstring &logres);
	const wchar_t *getOriginalWord(int I, wstring &out, bool concat, bool mostCommon = false);
	bool analyzeEnd(wstring &path, int begin, int end, bool &multipleEnds);
	void writeWords(wstring oPath);
	int scanForPatternTag(int where, int tag);
	int scanForPatternElementTag(int where, int tag);
	int printSentence(unsigned int rowsize, unsigned int begin, unsigned int end, bool containsNotMatched);
	int getSubjectInfo(tTagLocation subjectTagset, int whereSubject, int &nounPosition, int &nameLastPosition, bool &restateSet, bool &singularSet, bool &pluralSet);
	bool evaluateSubjectVerbAgreement(int verbPosition, int whereSubject, bool &agreementTestable);
	int queryPattern(int position, wstring pattern, int &maxEnd);

private:
	wstring primaryQuoteType,secondaryQuoteType;

	// filled during other operations
	wchar_t *bookBuffer;
	__int64 bufferLen,bufferScanLocation;
	int lastPEMAConsolidationIndex;
	vector <int> printMaxSize;
	vector < vector <int> > rememberCompetingCompoundObjectPositions; // for multiple compound object processing

	bool compoundObjectSubChain(vector < int > &objectPositions);
	unsigned int getNumCompoundObjects(int where,int &combinantScore,wstring &combinantStr);
	void getCompoundPositions(int where,vector <tTagLocation> &mobjectTagSets,vector < int > &objectPositions);
	void markMultipleObjects(int where);
	bool setAdditionalRoleTags(int where,int &firstFreePrep,vector <int> &futureBoundPrepositions,bool inPrimaryQuote,bool inSecondaryQuote,
		bool &nextVerbInSeries,int &sense,int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int sentenceBegin,int sentenceEnd,vector < vector <tTagLocation> > &tagSets);
	//void collectTagSetsFromSentence(unsigned int begin,unsigned int end,int &lastOpeningPrimaryQuote,unsigned int &section);
	int replicate(int recursionLevel,int PEMAPosition,int position,vector <tTagLocation> &tagSet,vector < vector <tTagLocation> > &childTagSets,vector < vector <tTagLocation> > &tagSets, unordered_map <int, vector < vector <tTagLocation> > > &TagSetMap);
	int collectTags(int rLevel,int PEMAPosition,int position,vector <tTagLocation> &tagSet,vector < vector <tTagLocation> > &tagSets, unordered_map <int, vector < vector <tTagLocation> > > &TagSetMap);
	int getPEMAPosition(int position,int offset);
	int scanUntil(const wchar_t *start,int repeat,bool printError);

	// printing sentences
	//unsigned int getShortLen(int position);
	unsigned int getMaxDisplaySize(vector <WordMatch>::iterator &im,int numPosition);
	bool sumMaxLength(unsigned int begin,unsigned int end,unsigned int &matchedTripletSumTotal,int &matchedSentences,bool &containsUnmatchedElement);

	bool isSectionHeader(unsigned int begin,unsigned int end,unsigned int &sectionEnd);
	int updatePEMACosts(int PEMAPosition,int pattern,int begin,int end,int position,vector<patternElementMatchArray::tPatternElementMatch *> &ppema);
	void reduceParent(int position,unsigned int PMAOffset,int diffCost);

	bool matchPattern(cPattern *p,int begin,int end,bool fill);
	bool matchPatternAgainstSentence(cPattern *p,int s,bool fill);
	int matchIgnoredPatternsAgainstSentence(unsigned int s,unsigned int &patternsTried,bool fill);
	int matchPatternsAgainstSentence(unsigned int s,unsigned int &patternsTried);
	//void collectMatchedPatterns(wstring markType,int position,intArray &endPositions);

	void logOptimizedString(wchar_t *line,unsigned int &linepos);
	//unsigned int getPMAMinCost(unsigned int position,int parentPattern,int rootPattern,int childend);
	int getMinCost(patternElementMatchArray::tPatternElementMatch *pem);

	// speaker resolution   - resolving quotes, speakers and pronouns
	vector <cObject> objects; // each object has only one position within the text
	unordered_map <int,int> postProcessLinkedTimeExpressions;
	vector <cTimelineSegment> timelineSegments;
	vector <int> singularizedObjects; // objects created during resolve that are singular versions of plural objects
	// collected with most encountered speakers first, also
	// emptied after section header - also includes pronouns that were specifically identified as the speaker.
	vector <cLocalFocus> localObjects; // objects in current section
	bool setFlagAlternateResolveForwardAfterDefinite; // keeps track of whether speakers are alternating after a definite speaker is set
	vector <int> previousSpeakers,beforePreviousSpeakers;
	vector <int> unresolvedSpeakers;
	int firstQuote; // first conversational quote occurring in source
	int lastQuote; // last conversational quote - updated during identifySpeakerGroups
	int lastOpeningPrimaryQuote,lastOpeningSecondaryQuote; // updated during both identifySpeakerGroups and resolveSpeakers
	int previousPrimaryQuote; // updated during both identifySpeakerGroups and resolveSpeakers.  Valid only in-between quotes.
	int spaceRelationsIdEnd; // marks the split between the space relations discovered by identifySpeakers and relations discovered by resolveSpeakers
	void adjustSaliencesBySubjectRole(int where,int lastBeginS1);
	void scanForLocation(bool check,bool &relAsObject,int &whereRelClause,int &pmWhere,int checkEnd);
	bool assignRelativeClause(int where);
	//bool newPhysicallyPresentPosition(int where,int beginObjectPosition,bool &physicallyEvaluated);
	bool physicallyPresentPosition(int where,bool &physicallyEvaluated);
	bool accompanyingRolePP(int where);
	bool physicallyPresentPosition(int where,int beginObjectPosition,bool &physicallyEvaluated,bool ignoreTense);
	bool unResolvablePosition(int where);
	int checkSubsequent(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool resolveForSpeaker,bool avoidCurrentSpeaker,vector <cOM> &objectMatches);
	bool matchByAppositivity(int where,int nextPosition);
	bool eliminateExactMatchingLocalObject(int where);
	vector <cLocalFocus>::iterator ownerObjectInLocal(int o);
	void includeGenericGenderPreferences(int where,vector <cObject>::iterator object);
	int resolveAdjectivalObject(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,bool resolveForSpeaker);
	void unMatchObjects(int where,vector <cOM> &objectMatches,bool identifiedAsSpeaker);
	void unMatchObjects(int where,vector <int> &objects,bool identifiedAsSpeaker);
	bool intersect(int where,vector <cOM> &speakers,bool &allIn,bool &oneIn);
	bool intersect(int where,set <int> &speakers,bool &allIn,bool &oneIn);
	bool intersect(int where,vector <int> &objects,bool &allIn,bool &oneIn);
	bool intersect(vector <cOM> &matches,set <int> &speakers,bool &allIn,bool &oneIn);
	bool intersect(set <int> &speakers,set <int> &matches,bool &allIn,bool &oneIn);
	bool intersect(set <int> &speakers,vector <int> &matches,bool &allIn,bool &oneIn);
	bool intersect(vector <int> &matches,set <int> &speakers,bool &allIn,bool &oneIn);
	bool intersect(vector <cOM> &m1,vector <cOM> &m2,bool &allIn,bool &oneIn);
	bool intersect(vector <cOM> &m1,vector <int> &speakers,bool &allIn,bool &oneIn);
	bool intersect(vector <tIWMM> &m1,vector <tIWMM> &m2,bool &allIn,bool &oneIn);
	bool intersect(set <int> &speakers,vector <cOM> &matches,bool &allIn,bool &oneIn);
	bool intersect(vector <int> &speakers,vector <cOM> &matches,bool &allIn,bool &oneIn);
	bool intersect(vector <int> &o1,vector <int> &o2,bool &allIn,bool &oneIn);
	bool intersect(vector <tIWMM> &m1,wchar_t **a,bool &allIn,bool &oneIn);
	bool intersect(int where1,int where2);
	bool isSubsetOfSpeakers(int where,int ownerWhere,set <int> &speakers,bool inPrimaryQuote,bool &atLeastOneInSpeakerGroup);
	bool rejectSG(int ownerWhere,set <int> &speakers,bool inPrimaryQuote);
	void setMatched(int where,vector <int> &objects);

	void getPOVSpeakers(set <int> &povSpeakers);
	void getCurrentSpeakers(set <int> &speakers,set <int> &povSpeakers);
	int checkIfOne(int I,int latestObject,set <int> *speakers);
	bool resolveMetaGroupWordOrderedFutureObject(int where,vector <cOM> &objectMatches);
	bool resolveMetaGroupFormerLatter(int where,int previousS1,int latestOwnerWhere,vector <cOM> &objectMatches);
	bool resolveMetaGroupFirstSecondThirdWordOrderedObject(int where,int lastBeginS1,vector <cOM> &objectMatches,int latestOwnerWhere);
	bool resolveMetaGroupPlural(int latestOwnerWhere,bool inQuote,vector <cOM> &objectMatches);
	bool resolveMetaGroupSpecifiedOther(int where,int latestOwnerWhere,bool inQuote,vector <cOM> &objectMatches);
	bool resolveMetaGroupGenericOther(int where,int latestOwnerWhere,bool inQuote,vector <cOM> &objectMatches);
	bool resolveMetaGroupNonNameObject(int where,bool inQuote,vector <cOM> &objectMatches,int &latestOwnerWhere);
	bool resolveMetaGroupInLocalObjects(int where,int o,vector <cOM> &objectMatches,bool onlyFirst);
	bool resolveMetaGroupOne(int where,bool inPrimaryQuote,vector <cOM> &objectMatches,bool &chooseFromLocalFocus);
	bool resolveMetaGroupTwo(int where,bool inQuote,vector <cOM> &objectMatches);
	bool resolveMetaGroupJoiner(int where,vector <cOM> &objectMatches);
	bool resolveMetaGroupOther(int where,vector <cOM> &objectMatches);
	bool resolveMetaGroupByAssociation(int where,bool inPrimaryQuote,vector <cOM> &objectMatches,int latest);
	bool resolveMetaGroupSpecificObject(int where,bool inPrimaryQuote,bool inSecondaryQuote,bool definitelyResolveSpeaker,int lastBeginS1,int lastRelativePhrase,vector <cOM> &objectMatches,bool &chooseFromLocalFocus);
	bool resolveMetaGroupObject(int where,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,
													bool definitelySpeaker,bool resolveForSpeaker,bool avoidCurrentSpeaker,bool &mixedPlurality,bool limitTwo,vector <cOM> &objectMatches,bool &chooseFromLocalFocus);

	int preferWordOrder(int wordOrderSensitiveModifier,vector <int> &locations);
	vector <cSpeakerGroup>::iterator containingSpeakerGroup();
	bool resolveOccRoleActivityObject(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int wordOrderSensitiveModifier,bool physicallyPresent);
	void resolveRelativeObject(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int wordOrderSensitiveModifier);
	bool tryGenderedSubgroup(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int whereGenderedSubgroupCount,bool limitTwo);
	bool matchOtherObjects(vector <int> &otherGroupedObjects,int speakerGroup,vector <cOM> &objectMatches);
	bool scanFutureGenderedMatchingObjects(int where,bool inQuote,vector <cObject>::iterator object,vector <cOM> &objectMatches);
	void includeWordOrderPreferences(int where,int wordOrderSensitiveModifier);
	bool resolveGenderedObject(int where,bool definitelyResolveSpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,
vector <cOM> &objectMatches,
														 vector <cObject>::iterator object,
int wordOrderSensitiveModifier,
														 int &subjectCataRestriction,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated);
	void mixedPluralityUsageSubGroupEnhancement(int where);
	void processSubjectCataRestriction(int where,int subjectCataRestriction);
	void addPreviousDemonyms(int where);
	bool addNewNumberedSpeakers(int where,vector <cOM> &objectMatches);
	bool addNewSpeaker(int where,vector <cOM> &objectMatches);
	bool resolveBodyObjectClass(int where,int beginEntirePosition,vector <cObject>::iterator object,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool resolveForSpeaker,bool avoidCurrentSpeaker,int wordOrderSensitiveModifier,int subjectCataRestriction,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated,bool &changeClass,vector <cOM> &objectMatches);
	void excludeSpeakers(int where,bool inPrimaryQuote,bool inSecondaryQuote);
	void discouragePOV(int where,bool inQuote,bool definitelySpeaker);
	void excludeObservers(int where,bool inQuote,bool definitelySpeaker);
	void excludePOVSpeakers(int where,wchar_t *fromWhere);
	bool resolveWordOrderOfObject(int where,int wo,int ofObjectWhere,vector <cOM> &objectMatches);
	void setQuoteContainsSpeaker(int where,bool inPrimaryQuote);
	void setResolved(int where,vector <cLocalFocus>::iterator lsi,bool isPhysicallyPresent);
	void resolveObject(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool resolveForSpeaker,bool avoidCurrentSpeaker,bool limitTwo);
	bool quotedString(unsigned int beginQuote,unsigned int endQuote,bool &noTextBeforeOrAfter,bool &noSpeakerAfterward);
	int speakerBefore(int beginQuote,bool &previousParagraph);
	void addCataSpeaker(int position,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool definitelySpeaker);
	void setSameAudience(int whereVerb,int speakerObjectPosition,int &audienceObjectPosition);
	int scanForSpeaker(int position,bool &definitelySpeaker,bool &crossedSectionBoundary,int &audienceObjectPosition);
	int repeatReplaceObjectInSectionPosition;
	wstring repeatReplaceObjectInSectionFromWhere;
	bool replaceObjectInSection(int where,int replacementObject,int objectToBeReplaced,wchar_t *fromWhere);
	void associateNyms(int where,int replacementObject,int objectToBeReplaced,wchar_t *fromWhere);
	int getBodyObject(int o);
	unsigned int numMatchingGenderInSpeakerGroup(int o);
	void imposeSpeaker(int beginQuote,int endQuote,int &lastDefiniteSpeaker,int speakerObjectAt,bool definitelySpeaker,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool previousSpeakersUncertain,int audienceObjectPosition,vector <int> &lastUnQuotedSubjects,int whereLastUnQuotedSubjects);
	void imposeMostLikelySpeakers(unsigned int beginQuote,int &lastDefiniteSpeaker,int audienceObjectPosition);
	wstring speakerResolutionFlagsString(__int64 flags,wstring &tmpstr);
	void resolveSpeakersUsingPreviousSubject(int where);
	void setAudience(int where,int whereQuote,int currentSpeakerWhere);
	void resolveAudienceByAlternation(int currentSpeakerWhere,bool definitelySpeaker,int urs,bool forwards);
	bool matchedObjectsEqual(vector <cOM> &match1,vector <cOM> &match2);
	bool resolveMatchesByAlternation(int currentSpeakerWhere,bool definitelySpeaker,int urs,bool forwards);
	void printUnresolvedLocation(int urs);
	void resolveSpeakersByAlternationForwards(int where,int currentSpeakerWhere,bool definitelySpeaker);
	void resolvePreviousSpeakersByAlternationBackwards(int where,int currentSpeakerWhere,bool definitelySpeaker);
	bool quotationsImmediatelyBefore(int beginQuote);
	int flipSpeaker(int &previousSpeakers,int &beforePreviousSpeakers);
	int assignSecondarySpeaker(unsigned int beginQuote,unsigned int endQuote);
	void displayQuoteContext(unsigned int begin,unsigned int end);
	bool quoteTest(int q,unsigned int &quoteCount,int &lastQuote,tIWMM quoteType,tIWMM quoteOpenType,tIWMM quoteCloseType);
	void secondaryQuoteTest(int q,unsigned int &secondaryQuotations,int &lastSecondaryQuote,tIWMM secondaryQuoteWord,tIWMM secondaryQuoteOpenWord,tIWMM secondaryQuoteCloseWord);
	bool resolvePronoun(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,int beginEntirePosition,
															bool resolveForSpeaker,bool avoidCurrentSpeaker,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated,int &subjectCataRestriction,vector <cOM> &objectMatches);
	enum pronounResolutionSearchType { anyType,anyPersonType,anyMalePersonType,anyFemalePersonType,anyPluralPersonType };
	bool findNoun(int I,pronounResolutionSearchType prsType,vector <cLocalFocus> &ls,bool inQuote);
	void createSemanticPatterns(void);
	bool preferVerbRel(int position,unsigned int J,cPattern *p);
	bool preferS1(int position,unsigned int J);
	void consolidateWinners(int begin);
	void addSpeakerObjects(int position,bool toMatched,int where,vector <int> speakers,__int64 resolutionFlag);
	// exactly like PEMA but with position
	bool matchAliases(int where,int object,int aliasObject);
	bool matchAliases(int where,int object,set <int> &objects);
	bool matchAliases(int where,int object,vector <cOM> &objects);
	bool matchAliasLocation(int where,int &objectLocation, int &aliasLocation);
	bool matchAlias(int where,int object, int aliasObject);

	// agreement
	unsigned int getAllLocations(unsigned int position,int parentPattern,int rootp,int childLen,int parentLen,vector <unsigned int> &allLocations);
	int markChildren(patternElementMatchArray::tPatternElementMatch *pem,int position,int recursionLevel,int allRootsLowestCost);
	bool findLowCostTag(vector<tTagLocation> &tagSet,int &cost,wchar_t *tagName,tTagLocation &lowestCostTag,int parentCost,int &nextTag);
	int evaluateSubjectVerbAgreement(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,unsigned int parentPosition,unsigned int position,vector<tTagLocation> &tagSet,int &traceSource);
	// agreement section end

	void printSpeakerQueue(int where);
	bool evaluateName(vector <tTagLocation> &tagSet,cName &name,bool &isMale,bool &isFemale,bool &isPlural,bool &isBusiness);
	bool identifyName(int begin,int principalWhere,int end,int &nameElement,cName &name,bool &isMale,bool &isFemale,bool &isNeuter,bool &isPlural,bool &isBusiness,
bool &comparableName,
														bool &comparableNameAdjective,bool &requestWikiAgreement,OC &objectClass);
	bool identifyName(int where,int &element,cName &name,bool &isMale,bool &isFemale,bool &isPlural,bool &isBusiness);
	void foldNames(int where,cObject s,int speakerSet);
	bool findSpecificAnaphor(wstring tagName,int where,int element,int &specificWhere,bool &pluralNounOverride,bool &embeddedName);
	bool reEvaluateHailedObjects(int beginQuote,bool setMatched);
	void boostRecentSpeakers(int where,int beginQuote,int speakersConsidered,int speakersMentionedInLastParagraph,bool getMostLikelyAudience);
	void getMostLikelySpeakers(unsigned int beginQuote,unsigned int endQuote,int lastDefiniteSpeaker,bool previousSpeakersUncertain,int wherePreviousLastSubjects,vector <int> &previousLastSubjects,int rejectObjectPosition,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	int compareSpeakerEntry(int index1,int index2,vector <cLocalFocus> &ls);
	bool matchObjectByGenderAndNumber(int where,vector <cObject>::iterator object,vector <cOM> &objectMatches,vector <cLocalFocus> &ls,bool inQuote,bool failureRetry);
	bool isNonTransferrableAdjective(tIWMM word);
	bool isMetaGroupWord(int where);
	bool isInternalBodyPart(int where);
	bool isInternalDescription(int where);
	bool isPsychologicalFeature(int where);
	bool isExternalBodyPart(int where,bool &singular,bool pluralAllowed);
	bool isVoice(int where);
	bool isFace(int where);
	bool isFacialExpression(int where);
	bool isPossible(int where);
	bool isGroupJoiner(tIWMM word);
	bool isDelayedReceiver(tIWMM word);
	bool isKindOf(int where);
	bool isVision(int where);
	void getOriginalWords(int I,vector <wstring> &wsoStrs,bool notFirst);
	bool searchExactMatch(cObject &object,int position);
	wstring getClass(int objectClass);
	void printLocalFocusedObjects(int where,int matchObjectClass);
	bool isPleonastic(unsigned int where);
	bool isPleonastic(tIWMM w);
	bool disallowReference(int object,vector <int> &disallowedReferences);
	void scanTag(int position,int rObject,int idExemption,vector <int> &disallowedReferences);
	void checkForPreviousPP(int where,vector <int> &disallowedReferences);
	void coreferenceFilterLL5(int where,vector <int> &disallowedReferences);
	wchar_t *loopString(int where,wstring &tmpstr);
	int coreferenceFilterLL2345(int position,int rObject, vector <int> &disallowedReferences,int lastBeginS1,int lastRelativePhrase,int lastQ2,bool &mixedPlurality,int &subjectCataRestriction);
	int pronounCoreferenceFilterLL6(int P, int lastBeginS1,vector <int> &disallowedReferences);
	int reflexivePronounCoreference(int P, int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool inPrimaryQuote,bool inSecondaryQuote);
	int identifySubType(int principalWhere,bool &partialMatch);
	int identifyObject(int tag,int where,int element,bool adjectival,int previousOwnerWhere,int ownerBegin);
	void setRole(int position,patternElementMatchArray::tPatternElementMatch *pem);
	int getObjectEnd(vector <cObject>::iterator object);
	bool overlaps(vector <cObject>::iterator object,vector <cObject>::iterator matchingObject);
	int getLocationBefore(vector <cObject>::iterator object,int where);
	void localRoleBoost(vector <cLocalFocus>::iterator lsi,int I,unsigned __int64 objectRole,int age);
	void adjustSaliencesByParallelRoleAndPlurality(int where,bool inQuote,bool forSpeakerIdentification,int lastGenderedAge);

	void pushSpeaker(int where,int s, int sf,wchar_t *fromWhere);
	bool matchObjectToSpeakers(int I,vector <cOM> &currentSpeaker,vector <cOM> &previousSpeaker,int inflectionFlags,unsigned __int64 quoteFlags,int lastEmbeddedStoryBegin);
	void removeSpeakers(int I,vector <cOM> &speakers);
	void removeSpeakers(int I,set <int> &speakers);
	void resolveFirstSecondPersonPronoun(int where,unsigned __int64 flags,int lastEmbeddedStoryBegin,vector <cOM> &currentSpeaker,vector <cOM> &previousSpeaker);
	cOM mostRecentGenderedObject(int where,vector <cObject>::iterator object,vector <cOM> &currentMaleObjects,vector <cOM> &currentFemaleObjects);
	void testIfDeleteSingularObjects(vector <cOM> currentMaleObjects,vector <cOM> currentFemaleObjects,vector <cOM> currentGenderUnknownObjects);
	bool getHighestEncounters(int &highestDefinitelyIdentifiedEncounters,int &highestIdentifiedEncounters,int &highestEncounters);
	void getOwnerSex(int ownerObject,bool &matchMale,bool &matchFemale,bool &matchNeuter, bool &matchPlural);
	void printNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &wnMap1,
								 vector <tIWMM> &nyms2, map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &wnMap2,wchar_t *type,wchar_t *subtype,
								 int sharedMembers,wstring &logMatch);
	void printNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &wnMap,wchar_t *type,wchar_t *subtype,wchar_t *subsubtype);
	bool setNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &wnMap);
	void clearNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &wnMap);
	bool objectClassComparable(vector <cObject>::iterator o,vector <cObject>::iterator lso);
	bool hasDemonyms(vector <cObject>::iterator o);
	bool sharedDemonyms(int where,bool traceNymMatch,vector <cObject>::iterator o,vector <cObject>::iterator lso,tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch);
	bool nymNoMatch(vector <cObject>::iterator o,tIWMM adj);
	bool nymNoMatch(int where,vector <cObject>::iterator o,vector <cObject>::iterator lso,bool getFromMatch,bool traceThisMatch,wstring &logMatch,tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch,wchar_t *type);
	int limitedNymMatch(vector <cObject>::iterator o,vector <cObject>::iterator lso,bool traceNymMatch);
	int nymMatch(vector <cObject>::iterator o,vector <cObject>::iterator lso,bool getFromMatch,bool traceNymMatch,bool &explicitOccupationMatch,wstring &logMatch,tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch,wchar_t *type);
	int nymMapMatch(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &wnMap1,
							 vector <tIWMM> &nyms2, map <tIWMM,vector <tIWMM>,tFI::cRMap::wordMapCompare > &wnMap2,
							 bool mapOnly,bool getFromMatch,bool traceNymMatch,wstring &logMatch,
							 tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch,wchar_t *type,wchar_t *subtype);
	void adjustForPhysicalPresence();
	void adjustSaliencesByGenderNumberAndOccurrenceAgeAdjust(int where,int object,bool inPrimaryQuote,bool inSecondaryQuote,bool forSpeakerIdentification,int &lastGenderedAge,vector <int> &disallowedReferences,bool disallowOnlyNeuterMatches,bool isPhysicallyPresent,bool physicallyEvaluated);
	void adjustSaliencesByGenderNumberAndOccurrence(int where,int object,bool inPrimaryQuote,bool inSecondaryQuote,bool identifiedAsSpeaker,int &lastGenderedAge,bool isPhysicallyPresent);
	bool resolveNonGenderedGeneralObjectAgainstOneObject(int where,vector <cObject>::iterator object,vector <cOM> &objectMatches,int o,int sf,int lastWhere,int &mostRecentMatch);
	void resolveNonGenderedGeneralObject(int where,vector <cObject>::iterator &object,vector <cOM> &objectMatches,int wordOrderSensitiveModifier);
	bool potentiallyMergable(int where,vector <cObject>::iterator o1,vector <cObject>::iterator o2,bool allowBothToBeSpeakers,bool checkUnmergableSpeaker);
	void accumulateNameLikeStats(vector <cObject>::iterator &object,int o,bool &firstNameAmbiguous,bool &lastNameAmbiguous,tIWMM &ambiguousFirst,int &ambiguousNickName,tIWMM &ambiguousLast);
	bool resolveNameObject(int where,vector <cObject>::iterator &object,vector <cOM> &objectMatches,int forwardCallingObject);
	//vector <cLocalFocus>::iterator in(int o,bool inQuote,bool neuter);
	vector <cLocalFocus>::iterator in(int o);
	bool in(int o,int where);
	vector <cOM>::iterator in(int o,vector <cOM> &objects);
	bool replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,wchar_t *fromWhat);
	bool replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,vector <cObject::cLocation>::iterator ol);
	bool replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,int ol);
	void replaceObject(int replacementObject,int objectToBeReplaced,wchar_t * fromWhat);
	bool replaceObject(int where,int replacementObject, int objectToBeReplaced,vector <int> &objects, wchar_t *description,int begin,int end,wchar_t *fromWhat);
	bool replaceObject(int where,int replacementObject, int objectToBeReplaced,set <int> &objects, wchar_t *description,int begin,int end,wchar_t *fromWhat);
	bool replaceObjectInSpeakerGroup(int where,int replacementObject,int objectToBeReplaced,int sg,wchar_t *fromWhat);
	void replaceObjectWithObject(int where,vector <cObject>::iterator &object,int objectConfidentMatch,wchar_t *fromWhat);
	void resolveNameGender(int where,bool male,bool female);
	int getRoleSalience(unsigned __int64 role);
	tIWMM setSex(vector <tTagLocation> &tagSet,int where,bool &isMale,bool &isFemale,bool isPlural);
	bool evaluateNameAdjective(vector <tTagLocation> &tagSet,cName &name,bool &isMale,bool &isFemale);
	bool identifyNameAdjective(int where,cName &name,bool &isMale,bool &isFemale);
	bool recentExit(vector <cLocalFocus>::iterator lsi);
	void testLocalFocus(int where,vector <cLocalFocus>::iterator lsi);
	bool pushObjectIntoLocalFocus(int where,int matchingObject,bool identifiedAsSpeaker,bool notSpeaker,bool inPrimaryQuote,bool inSecondaryQuote,wchar_t *fromWhere, vector <cLocalFocus>::iterator &lsi);
	vector <cLocalFocus>::iterator substituteAlias(int where,vector <cLocalFocus>::iterator lsi);
	void pushLocalObjectOntoMatches(int where,vector <cLocalFocus>::iterator lsi,wchar_t *reason);
	void narrowGender(int where,int toObject);
	void eliminateBodyObjectRedundancy(int where,vector <cOM> &objectMatches);
	bool mixedPluralityInSameSentence(int where);
	bool evaluateCompoundObjectAsGroup(int where,bool &physicallyEvaluated);
	void matchAdditionalObjectsIfPlural(int where,bool isPlural,bool atLeastOneReference,
																						bool physicallyEvaluated,bool physicallyPresent,
																						bool inPrimaryQuote,bool inSecondaryQuote,
																						bool definitelySpeaker,bool mixedPlurality,
		vector <cLocalFocus>::iterator *highest,int numNeuterObjects,int numGenderedObjects,
		vector <int> &lsOffsets);
	void preferRelatedObjects(int where);
	bool chooseBest(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,bool resolveForSpeaker,bool mixedPlurality);
	void removeObjectsFromNotMatched(int where,int matchWhere);
	void removeObjectsFromMatched(int where,int matchWhere,vector <cLocalFocus> &ls);
	void setSpeakerMatchesFromPosition(int where,vector <cOM> &objectsToSet,int fromPosition,wchar_t *fromWhere,unsigned __int64 flag);
	void setObjectsFromMatchedAtPosition(int where,vector <cOM> &objectsSet,int matchWhere,bool identifiedAsSpeaker,unsigned __int64 flag);
	bool removeMatchedFromObjects(int where,vector <cOM> &removeFrom,int matchWhere,bool identifiedAsSpeaker);

	// relations
	bool tagIsCertain(int position);
	bool getVerb(vector <tTagLocation> &tagSet,int &tag);
	bool checkAmbiguousVerbTense(int whereVerb,int &sense,bool inQuote,tIWMM masterVerbWord);
	int getSimplifiedTense(int tense);
	char *tagSetTimeArray;
	unsigned int tagSetTimeArraySize;
	int getVerbTense(vector <tTagLocation> &tagSet,int verbTagIndex,bool &isId);
	bool getIVerb(vector <tTagLocation> &tagSet,int &tag);
	bool checkRelation(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,tIWMM verbWord,tIWMM objectWord,int relationType);
	int calculateVerbAfterVerbUsage(int whereVerb,unsigned int nextWord);
	int evaluateVerbObjects(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,vector <tTagLocation> &tagSet,bool infinitive,bool assessCost,int &voRelationsFound,int &traceSource);
	int properNounCheck(int &traceSource,int begin,int end,int whereDet);
	int evaluateNounDeterminer(vector <tTagLocation> &tagSet,bool assessCost,int &traceSource,int begin,int end);
	bool hasTimeObject(int where);
	int attachAdjectiveRelation(vector <tTagLocation> &tagSet,int whereObject);
	int attachAdverbRelation(vector <tTagLocation> &tagSet,int verbTagIndex,tIWMM verbWord);
	bool resolveObjectTagBeforeObjectResolution(vector <tTagLocation> &tagSet,int tag,tIWMM &word);
	tIWMM resolveToClass(int position);
	tIWMM resolveObjectToClass(int where,int o);
	tIWMM fullyResolveToClass(int position);
	bool forcePrepObject(vector <tTagLocation> &tagSet,int tag,int &object,int &whereObject,tIWMM &word);
	bool resolveTag(vector <tTagLocation> &tagSet,int tag,int &object,int &whereObject,tIWMM &word);
	void trackVerbTenses(int where,
vector <tTagLocation> &tagSet,
		bool inQuote,bool inQuotedString,
bool inSectionHeader,
		bool ambiguousSense,int sense,bool &tenseError);
	void recordVerbTenseRelations(int where,int sense,int subjectObject,int whereVerb);
	void evaluateSubjectRoleTag(int where,int which,vector <int> whereSubjects,int whereObject,int whereHObject,int whereVerb,int whereHVerb,vector <int> subjectObjects,int tsSense,
														bool ignoreSpeaker,bool isNot,bool isNonPast,bool isNonPresent,bool isId,bool subjectIsPleonastic,bool inPrimaryQuote,bool inSecondaryQuote,bool backwardsSubjects);
	bool skipQuote(int &where);
	void scanForSubjectsBackwardsInSentence(int where,int whereVerb,bool isId,bool &objectAsSubject,bool &subjectIsPleonastic,vector <tIWMM> &subjectWords,vector <int> &subjectObjects,vector <int> &whereSubjects,int tsSense,bool &multiSubject,bool preferInfinitive);
	void discoverSubjects(int where,vector <tTagLocation> &tagSet,int subjectTag,bool isId,bool &objectAsSubject,bool &subjectIsPleonastic,vector <tIWMM> &subjectWords,vector <int> &subjectObjects,vector <int> &whereSubjects);
	void markPrepositionalObjects(int where,int whereVerb,bool flagInInfinitivePhrase,bool subjectIsPleonastic,bool objectAsSubject,bool isId,bool inPrimaryQuote,bool inSecondaryQuote,bool isNot,bool isNonPast,bool isNonPresent,bool noObjects,bool delayedReceiver,int tsSense,vector <tTagLocation> &tagSet);
	void addRoleTagsAt(int where,int I,bool inRelativeClause,bool withinInfinitivePhrase,bool subjectIsPleonastic,bool isNot,bool objectNot,int tsSense,bool isNonPast,bool isNonPresent,bool objectAsSubject,bool isId,bool inPrimaryQuote,bool inSecondaryQuote,wchar_t *fromWhere);
	int findPrepRole(int whereLastPrep,int role,int rejectRole);
	int processInternalInfinitivePhrase(int where,int whereVerb,int whereParentObject,int iverbTag,int firstFreePrep,vector <int> &futureBoundPrepositions,bool inPrimaryQuote,bool inSecondaryQuote,
bool &nextVerbInSeries,

int &sense,
		int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int begin,int end,int infpElement,vector <tTagLocation> &tagSet);
	bool evaluateAdditionalRoleTags(int where,vector <tTagLocation> &tagSet,int len,int firstFreePrep,vector <int> &futureBoundPrepositions,bool inPrimaryQuote,bool inSecondaryQuote,bool &outsideQuoteTruth,bool &inQuoteTruth,bool withinInfinitivePhrase,bool internalInfinitivePhrase,
																	bool &nextVerbInSeries,int &sense,int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int begin,int end);
	int findObject(tTagLocation &tag,int &position);
	size_t startCollectTagsFromTag(bool inTrace,int tagSet,tTagLocation &tl,vector < vector <tTagLocation> > &tagSets,int rejectTag,bool collectParentTags);
	size_t startCollectTags(bool trace,int tagSet,int position,int PEMAPosition,vector < vector <tTagLocation> > &tagSets,bool obeyBlock,bool collectParentTags);
	void evaluateNounDeterminers(int PEMAPosition,int position,vector < vector <tTagLocation> > &tagSets);
	int evaluatePrepObjectRelation(vector <tTagLocation> &tagSet,int &pIndex,tIWMM &prepWord,int &object,int &wherePrepObject,tIWMM &objectWord);
	bool inTag(tTagLocation &innerTag,tTagLocation &outerTag);
	void equivocateObjects(int where,int eTo,int eFrom);
	void assignMetaQueryAudience(int beginQuote,int previousQuote,int primaryObject,int secondaryObject,int secondaryTag,vector <tTagLocation> &tagSet);
	bool processMetaSpeakerQueryAnswer(int beginQuote,int previousQuote,int lastQuery);
	bool evaluateMetaSpeaker(int where,vector <tTagLocation> &tagSet);
	bool identifyMetaSpeaker(int where,bool inQuote);
	bool evaluateAnnounce(int where,vector <tTagLocation> &tagSet);
	bool identifyAnnounce(int where,bool inQuote);
	bool evaluateMetaGroup(int where,vector <tTagLocation> &tagSet,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	bool identifyMetaGroup(int where,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	bool identifyMetaNameEquivalence(int where,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	bool ageDetection(int where,int primary,int secondary);
	bool evaluateMetaNameEquivalence(int where,vector <tTagLocation> &tagSet,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);

	int processSnippet(wstring snippet,wstring object,Source *&source,bool parseOnly);
	int processAbstract(cTreeCat *rdfType,Source *&source,bool parseOnly);	
	int processWikipedia(int principalWhere,Source *&source,vector <wstring> &wikipediaLinks,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases,bool parseOnly);	
	bool matchObjectsExact(vector <cObject>::iterator parentObject,vector <cObject>::iterator childObject,bool &namedNoMatch);
	static bool matchObjects(Source *parentSource,vector <cObject>::iterator parentObject,Source *childSource,vector <cObject>::iterator childObject,bool &namedNoMatch, sTrace debugTrace);
	bool matchChildSourcePositionSynonym(tIWMM parentWord,Source *childSource,int childWhere);
	static bool matchChildSourcePosition(Source *parentSource,vector <cObject>::iterator parentObject,Source *childSource,int childWhere,bool &namedNoMatch, sTrace debugTrace);
	bool isDefiniteObject(int where,wchar_t *definiteObjectType,int &ownerWhere,bool recursed);
	static bool matchTimeObjects(Source *parentSource,int parentWhere,Source *childSource,int childWhere);
	tIWMM getTense(tIWMM verb,tIWMM subject,int tenseDesired);
	wstring getTense(int where,wstring candidate,int preferredVerb);
	int appendVerb(vector <wstring> &objects,int where);
	int appendWord(vector <wstring> &objects,int where);
	bool inObject(int where,int whereQuestionType);
	bool appendPrepositionalPhrase(int where,vector <wstring> &prepPhraseStrings,int relPrep,bool nonMixed,bool lowerCase,wchar_t *separator,int atNumPP);
	int appendPrepositionalPhrases(int where,wstring &wsoStr,vector <wstring> &prepPhraseStrings,int &numWords,bool nonMixed,wchar_t *separator,int atNumPP);
	int getObjectStrings(int where,int object,vector <wstring> &wsoStrs,bool &alreadyDidPlainCopy);
	int appendObject(__int64 questionType, int whereQuestionType, vector <wstring> &objects, int where);
	int sriPrepMatch(Source *childSource,int parentWhere,int childWhere,int cost);
	int sriVerbMatch(Source *childSource,int parentWhere,int childWhere,int cost);
	int sriMatch(Source *source, int whereParentComponent, int whereChildComponent, int whereQuestionType, __int64 questionType, bool &totalMatch, int cost);
	int equivalenceClassCheck(Source *childSource,vector <cSpaceRelation>::iterator childSRI,cSpaceRelation* parentSRI,int whereChildSpecificObject,int &equivalenceClass,int matchSum);	
	int equivalenceClassCheck2(Source *childSource,vector <cSpaceRelation>::iterator childSRI,cSpaceRelation* parentSRI,int whereChildSpecificObject,int &equivalenceClass,int matchSum);
	void analyzeQuestionFromSpecificSource(wstring childSourceType,Source *childSource, cSpaceRelation * parentSRI,bool parseOnly,vector < cAS > &answerSRIs,int &maxAnswer,int rejectDuplicatesFrom,int ws,int wo);
	void enterAnswer(wchar_t *derivation,wstring childSourceType,Source *childSource,cSpaceRelation* parentSRI,vector < cAS > &answerSRIs,int &maxAnswer,int rejectDuplicatesFrom,int matchSum,
	                       wstring matchInfo,vector <cSpaceRelation>::iterator childSRI,int equivalenceClass,int ws,int wo,int wp,int &answersContainedInSource);
	int metaPatternMatch(Source *childSource,vector <cSpaceRelation>::iterator childSRI,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	int analyzeQuestionFromSource(wchar_t *derivation,wstring sourceType,Source *source, cSpaceRelation *parentSRI,vector < cAS > &answerSRIs,int &maxAnswer,int rejectDuplicatesFrom,bool eraseIfNoAnswers,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	int scanColumnEntry(int whereQuestionType,Source *wikipediaSource,int &I,bool &matchFound);

	int	searchTableForAnswer(wchar_t derivation[1024],cSpaceRelation* sri, unordered_map <int,WikipediaTableCandidateAnswers * > &wikiTableMap,vector <cSpaceRelation> &subQueries,vector < cAS > &answerSRIs,int &maxAnswer,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	bool analyzeTitle(unsigned int where,int &numWords,int &numPrepositions);
	bool isEntryInvalid(int tableNum, int beginColumn, vector <int> &wikiColumns);
	bool getTableFromSource(int I, int tableStart, int whereQuestionTypeObject, SourceTable &wikiTable);
	void addTable(int &I, int whereQuestionType, Source *wikipediaSource, SourceTable &wikiTable);
	void addTables(int whereQuestionType, Source *wikipediaSource, vector < SourceTable > &wikiTables);
	void getPrepositionalPhraseNonMixMatch(int whereQuestionContextSuggestion,int &minPrepositionalPhraseNonMixMatch,int &maxPrepositionalPhraseNonMixMatch);
	void analyzeQuestion(wchar_t *derivation,int whereQuestionContextSuggestion, cSpaceRelation * parentSRI,cTreeCat *rdfType,bool parseOnly,vector < cAS > &answerSRIs,int &maxAnswer, unordered_map <int,WikipediaTableCandidateAnswers *> &wikiTableMap,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	void enhanceWebSearchQueries(vector <wstring> &webSearchQueryStrings,wstring semanticSuggestion);
	void getWebSearchQueries(cSpaceRelation* parentSRI,vector <wstring> &objects);

	class searchSource
	{
	public:
		bool isSnippet; // snippet
		bool skipFullPath; // skip full path if snippet has good answer
		bool hasCorrespondingSnippet; // is a full article with a corresponding snippet in dbPedia
		int fullPathIndex; // location of corresponding fullPath
		wstring fullWebPath;
		wstring pathInCache;
	};
	int writeParseRequestToDatabase(vector <searchSource>::iterator pri);
	int spinParses(vector <searchSource> &accumulatedParseRequests);
	int accumulateParseRequests(cSpaceRelation* parentSRI, int webSitesAskedFor, int index, bool googleSearch, vector <wstring> &webSearchQueryStrings, int &offset, vector <searchSource> &accumulatedParseRequests);
	int analyzeAccumulatedRequests(wchar_t *derivation, cSpaceRelation *parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, vector <searchSource> &accumulatedParseRequests,
 cPattern *&mapPatternAnswer,
		cPattern *&mapPatternQuestion);
	int webSearchForQueryParallel(wchar_t *derivation,cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
 vector <wstring> &webSearchQueryStrings,
		int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int webSearchForQuerySerial(wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
 vector <wstring> &webSearchQueryStrings,
		int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int createParseRequestTable(void);

	bool matchOwnershipDbMusicBrainz(wchar_t *derivation, cSpaceRelation* parentSRI);
	bool pushWhereEntities(wchar_t *derivation,int where,wstring matchEntityType,wstring byWhatType,int whatWhere,bool filterNameDuplicates);
	bool pushEntities(wchar_t *derivation,int where,wstring matchEntityType,wstring byWhatType,wstring what,bool filterNameDuplicates);
	bool dbSearchMusicBrainzSearchType(wchar_t *derivation,cSpaceRelation* parentSRI,vector < cAS > &answerSRIs,
	                                        int firstWhere,wstring firstMatchListType,int secondWhere,wstring secondMatchListType,const wchar_t *matchVerbsList[]);
	bool matchedList(const wchar_t *matchList[], int where, int objectClass);
	cOM createObject(wstring derivation,wstring wordstr,OC objectClass);
	bool matchOwnershipDbMusicBrainzObject(wchar_t *derivation,int whereObject);
	bool dbSearchMusicBrainz(wchar_t *derivation, cSpaceRelation* parentSRI,vector < cAS > &answerSRIs);
	bool matchOwnershipDbQuery(wchar_t *derivation, cSpaceRelation* parentSRI);
	bool dbSearchForQuery(wchar_t *derivation, cSpaceRelation* parentSRI,vector < cAS > &answerSRIs);

	// MYSQL Database
	int createLocationTables(void);
	int createSentimentTables(void);
	int createObjectTables(void);
	int createTimeRelationTables(void);
	int createRelationTables(void);
	int flushObjectRelations();
	int alreadyExists(char *word);
	int readMultiSourceObjects(tIWMM *wordMap,int numWords);
	int flushObjects(set <int> &objectsToFlush);
	int flushGroups(int sourceId);
	bool abbreviationEquivalent(tIWMM w,tIWMM w2);
	bool accumulateRelatedObjects(int object,set <int> &relatedObjects);
	void addAssociatedNounsFromTitle(int o);
	void addWordAbbreviationMap(tIWMM w,vector <tIWMM> &nouns);
	void buildMap(void);
	int rti(int where);
	const wchar_t *wchr(int where);
	const wchar_t *wrti(int where,wchar_t *id,wstring &tmpstr,bool shortFormat=false);
	bool acceptableAdjective(int where);
	bool acceptableObjectPosition(int where);
	int getMSAdverb(int whereVerb,bool changeStateAdverb);
	int getOSAdjective(int whereVerb,int where);
	int getOSAdjective(int object);
	int getMSAdjective(int whereVerb,int where,int numOrder);
	int getMSAdjective(int object,int numOrder);
	const wchar_t *getWSAdverb(int whereVerb,bool changeStateAdverb);
	wstring getWOSAdjective(int whereVerb,int where,wstring &tmpstr);
	wstring getWOSAdjective(int where,wstring &tmpstr);
	wstring getWSAdjective(int whereVerb,int where,int numOrder,wstring &tmpstr);
	wstring getWSAdjective(int where,int numOrder);
	int getProfession(int object);
	int getVerbIndex(int whereVerb);
	int getMatchedObject(int where,set <int> &objects);
	int maxBackwards(int where);
	int getMinPosition(int where);
	int gmo(int wo);
	void getSRIMinMax(cSpaceRelation *sri);
	void prepPhraseToString(int wherePrep,wstring &ps);
	void insertPrepPhrase(vector <cSpaceRelation>::iterator sri,int where,int wherePrep,int whereSecondaryVerb,int whereSecondaryObject,set <int> &relPreps,wchar_t *pnqt,size_t &pnlen,set <int> &objects,wstring &ps);
	void testPFlag(int where,int wp,int &prepositionFlags,int flag,int &nearestLocation);
	void processPrepositionFlags(vector <cSpaceRelation>::iterator sri,int &prepositionFlags);
	void insertCompoundObjects(int wo,set <int> &relPreps, unordered_map <int,int> &principalObjectEndPoints);
	void correctSRIEntry(cSpaceRelation &sri);
	bool advanceSentenceNum(vector <cSpaceRelation>::iterator sri,unsigned int &s,int lastMin,int lastMax,bool inQuestion);
	// accumulate new patterns
	bool printPattern(int patternLength,int pc);
	bool printPatterns(int patternLength,unsigned int topLimit);
	void accumulateNewPattern(unsigned int w,int pc,int wordcount);
	bool primitiveMatch(vector<int> elements,int w,wstring &patternMatch);
	void printAccumulatedPatterns(void);
	map <vector <cSpaceRelation>::iterator,vector <cPattern *> > transformationPatternMap;
};

extern vector <Source::cSpeakerGroup>::iterator sgNULL;

