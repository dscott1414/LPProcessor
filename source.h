/*
	source.h - declaration of cSource, the in-memory representation of one entire parsed document (novel)

	Overview:
		This header declares the central document object of LPProcessor and the small
		value types that every pipeline stage reads and writes:
			cWordMatch  - one token ("source position") of the document, with its form/pattern
			              bitmaps, pattern-match arrays, syntactic-relation links, object
			              assignment and speaker/audience resolution state.
			cObject     - one entity (person, place, thing, meta-group) discovered in the text.
			cLocalFocus - one entity's "salience" record while it is in local focus (the
			              attention window used for pronoun/speaker resolution).
			cOM         - an (object index, salience factor) pair; the unit of a match list.
			cSource     - the document: the token array m, the object array objects, the
			              speaker groups, sections, timelines, MySQL handle, plus the several
			              hundred member functions implementing stages 2-8 of the pipeline.
		Nearly all cross-references inside these structures are plain ints that index one of
		three parallel arrays, and -1 is the universal "unset" sentinel (see "Index
		conventions" below).

	Pipeline position:
		Everything after initialization operates on a cSource. tokenize.cpp fills m;
		pattern.cpp / patternMatchArray.cpp / patternElementMatchArray.cpp fill m[].pma and
		pema; syntacticRelations.cpp fills the rel* links; identifyObjects.cpp /
		resolveObjects.cpp fill objects and m[].objectMatches; identifySpeakerGroups.cpp /
		resolveSpeakers.cpp fill speakerGroups, m[].speakerPosition and m[].audiencePosition;
		questionAnswering.cpp consumes the finished structure. source.cpp implements reading,
		tokenizing, and the binary cache read()/write() of the whole object.

	Key entry points (defined in source.cpp and the stage .cpp files):
		- readSourceBuffer() / retrieveText() / findStart() - load a document and skip
		  Project Gutenberg boilerplate.
		- tokenize() / parseBuffer() - build m.
		- eliminateLoserPatterns() - cost-based winnowing of competing pattern matches.
		- syntacticRelations() / identifyObjects() / resolveObjects() - relations and entities.
		- identifySpeakerGroups() / resolveSpeakers() - who speaks to whom.
		- read() / write() - binary cache of a fully parsed source; sanityCheck() validates
		  every index in a cache that was just read.

	Key data structures / globals:
		- m               - vector <cWordMatch>, the token array. A "source position",
		                    "where", or "position" anywhere in this codebase is an index
		                    into m unless stated otherwise.
		- objects         - vector <cObject>, one entry per discovered entity; an "object"
		                    int is an index into it.
		- localObjects    - vector <cLocalFocus>, the current attention window; cObject
		                    ::lsiOffset and the "lsi" iterators point into it.
		- speakerGroups   - vector <cSpeakerGroup>, conversation participants over spans of m.
		- sections        - vector <cSection>, chapter/section boundaries in m.
		- pema            - cPatternElementMatchArray shared by all positions; m[].pma holds
		                    per-position pattern matches.
		- wmNULL, cNULL, sgNULL - "null iterator" sentinels for m, localObjects and
		                    speakerGroups respectively (declared at the end of the
		                    corresponding class).

	Index conventions (the single most important thing to know here):
		- int fields named where*, *Position, begin/end, lastWhere, at, ... index m.
		- int fields named object, replacedBy, o, speakers members, ... index objects.
		- *SpeakerGroup fields index speakerGroups; section indexes sections;
		  PMAElement/PEMAPosition index m[x].pma and pema respectively.
		- -1 means "not set"/"none" almost everywhere; a few fields overload additional
		  negative values (cObject::eOBJECTS, CURRENT_SUBSET_SG, cSpeakerGroup::sgEnd -2/-3,
		  cObject::ownerWhere negative word-order encodings). Those are called out at the
		  field.
		- end offsets are exclusive: an object or phrase covers [begin,end).

	Dependencies:
		syntacticRelations.h / semanticRelations.h / names.h / tableColumn.h / bitObject.h /
		getMusicBrainz.h / vcXML.h, the global word lexicon Words (word.h) and the tIWMM
		iterators into it, MySQL (member mysql), and the copy()/lplog() helpers from
		general.h / logging.h.

	Notes / gotchas:
		- tIWMM values are unordered_map iterators into the global Words map; they are
		  serialized as the word string, not as a pointer, and are re-queried on read (so a
		  word missing from the lexicon deserializes to wNULL - callers must check).
		- The binary cache format is defined implicitly by the matched pairs of write() and
		  the (char *buffer,int &where,...) constructors in this file. Any field added to one
		  side must be added to the other in the same order.  cSource::write()/read()
		  (source.cpp) do write and check a leading SOURCE_VERSION int (pattern.h) that is
		  rejected as "old, reparse" on mismatch, so a version bump makes an incompatible
		  cache fail safely instead of misparsing - but that safety net only works if
		  SOURCE_VERSION is actually bumped alongside the field-order change; forgetting to
		  bump it still silently misparses every previously written cache.
		- The (char*,int&,...) deserializing constructors use the "if (error=!copy(...))"
		  assignment-in-condition idiom throughout; that is intentional, not a == typo.
		- lplog(LOG_FATAL_ERROR,...) does NOT return: logstring() terminates the process
		  (see logging.cpp fatalExit).  Code after such a check is unreachable.
		- Several classes have more than one constructor that initialize different subsets of
		  members (cWordMatch and cObject in particular); see the comments there before
		  relying on a default-constructed instance.
*/
#pragma warning (disable: 4503)
#pragma once
// Batch B12: source.h declares members taking vector<cTreeCat*>, so it needs
// ontology.h. It used to rely on every including .cpp listing ontology.h first,
// which held only because they all did -- getDictionary.cpp reaches source.h
// through word.h without ever naming ontology.h, and that is what surfaced it.
#include "ontology.h"
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#include "syntacticRelations.h"
#include "names.h"
#include "bitObject.h"
#include "tableColumn.h"
#include "getMusicBrainz.h"
#include "vcXML.h"

class cSyntacticRelationGroup;
#define MAX_LEN 2048

#include "semanticRelations.h"

#define MINIMUM_SALIENCE_WITH_MATCHED_ADJECTIVES -2500 // 2500 makes Irish Sinn Feiner work!
#define MORE_SALIENCE_PER_ADJECTIVE 0 // -100 // This only changed things slightly for the worse - 22695: The new-comer
#define DISALLOW_SALIENCE 200000

#define ILLEGAL_PATH_CHARS "\\/:*?\"<>|"
#define WCHAR_ILLEGAL_PATH_CHARS u"\\/:*?\"<>|"
#define IS_SALIENCE_BOOST 2000

void escapeStr(lpwstring &str);
lpwstring escaped(const lpwstring &str);

// "Object Match": an entity plus the confidence/preference score with which it was matched at
// some source position.  This is the element type of every match list in the system
// (cWordMatch::objectMatches, cSpeakerGroup::replacedSpeakers, cSection::speakerObjects, ...).
//   object         - index into cSource::objects, or -1 for "none".  May also hold one of the
//                    negative cObject::eOBJECTS pseudo-objects (OBJECT_UNKNOWN_MALE etc) in
//                    the lists that allow unresolved placeholders.
//   salienceFactor - accumulated salience/preference score computed during resolution; higher
//                    is a better match.  DISALLOW_SALIENCE (200000) is used as a poison value
//                    to keep a candidate out of the running, and MINIMUM_SALIENCE_WITH_MATCHED
//                    _ADJECTIVES is the floor applied when adjectives matched.  cLocalFocus
//                    ::clear() resets it to -1.
// Note the asymmetry between the comparison operators: == compares both fields but != compares
// only 'object', so !(a==b) is not equivalent to (a!=b) when only the salience differs.  Code
// that wants set-membership semantics must use != (or in()), and code that wants exact identity
// must use ==.
class cOM
{
public:
	int object;
	int salienceFactor;
	cOM(int o,int sf) { object=o; salienceFactor=sf; };
	// Deserialize from the binary source cache.  'where' is advanced past the two ints; 'error'
	// is set true on a truncated buffer (and the object is left partially filled).
	cOM(char *buffer,int &where,int limit, bool &error)
	{
		error = true;
		if (!copy(object,buffer,where,limit)) return;
		if (!copy(salienceFactor,buffer,where,limit)) return;
		error = false;
	}
	// Serialize to the binary source cache; must stay in the same field order as the
	// deserializing constructor above.  Returns false if the copy helper reported failure.
	bool write(void *buffer,int &where,int limit)
	{
		if (!copy(buffer,object,where,limit)) return false;
		if (!copy(buffer,salienceFactor,where,limit)) return false;
		return true;
	}
	cOM(void) { object=-1; salienceFactor=0; };
	bool operator == (const cOM& om)
	{  return object==om.object && salienceFactor==om.salienceFactor;  }
	// Deliberately compares only the object index (salience is ignored), so this is "a
	// different entity", not "a different value" - see the class comment.
	bool operator != (const cOM& om)
	{  return object!=om.object; }
};

// Coarse tense buckets used when a full tense index (NUM_SIMPLE_TENSE, see cTenseStat) is not
// needed - e.g. cSource::getTense(...,tenseDesired).  Values start at 1 so that 0 can mean
// "unspecified".
enum RENUM { R_SimplePresent=1,R_SimplePast=2,R_SimpleFuture=3,R_PresentPerfect=4,R_PastPerfect=5,R_FuturePerfect=6 };

// One token of the document: cSource::m[where] for a source position 'where'.  A cWordMatch is
// created per word (and per punctuation mark, quote, and synthetic section marker) by
// tokenize.cpp and then progressively annotated by every later stage, so most members below are
// meaningless until the stage that owns them has run.
//
// Roughly, the members group into:
//   - the lexicon link (word) and the form/inflection bitmaps (forms, flags, tmpWinnerForms);
//   - the pattern-matching state (pma, patterns, maxMatch/maxLAC*/lowestAverageCost costs,
//     beginPEMAPosition/endPEMAPosition/PEMACount, PMAWinners/PEMAWinners);
//   - the syntactic-relation links (relSubject/relVerb/relObject/relPrep/relNextObject/
//     relInternal*/*CompoundPartObject) - every one of these is a source position, i.e. an
//     index into the same m array, or -1;
//   - the entity assignment (object/originalObject/principalWhere*/begin-endObjectPosition/
//     objectMatches/audienceObjectMatches) - 'object' indexes cSource::objects;
//   - the quote and speaker state (endQuote/nextQuote/previousQuote/quoteForwardLink/
//     quoteBackLink/speakerPosition/audiencePosition/embeddedStorySpeakerPosition).
//
// The 64-bit 'flags' field is shared by two disjoint flag vocabularies: bits >= 32 (the
// static const flagXxx members) are global, while the low 32 bits are declared in the anonymous
// enum below and are interpreted differently depending on whether the position is a quote
// (speaker-resolution flags) or not (form/BNC/object flags).  Several enum values therefore
// intentionally collide - e.g. flagFirstLetterCapitalized and flagFromPreviousHailResolveSpeakers
// are both 1<<23.  Never test a quote-only flag on a non-quote position or vice versa.
class cWordMatch
{
public:
	static const uint64_t flagConstant=((uint64_t)1<<58);
	static const uint64_t flagTransitory=((uint64_t)1<<57);
	static const uint64_t flagNewLineBeforeHint=((uint64_t)1<<56);
	static const uint64_t flagAlreadyTimeAnalyzed=((uint64_t)1<<55);
	static const uint64_t flagVAnalysisVNOnly=((uint64_t)1<<54);
	static const uint64_t flagUsedPossessionRelation=((uint64_t)1<<53);
	static const uint64_t flagUsedBeRelation=((uint64_t)1<<52);
	static const uint64_t flagInLingeringStatement=((uint64_t)1<<51);
	static const uint64_t flagRelativeObject=((uint64_t)1<<50); 
	static const uint64_t flagInInfinitivePhrase=((uint64_t)1<<49);
	static const uint64_t flagResolvedByRecent=((uint64_t)1<<48); 
	static const uint64_t flagLastContinuousQuote=((uint64_t)1<<47); 
	static const uint64_t flagAlternateResolveForwardFromLastSubjectAudience=((uint64_t)1<<46); 
	static const uint64_t flagAlternateResolveForwardFromLastSubjectSpeakers=((uint64_t)1<<45); 
	static const uint64_t flagGenderIsAmbiguousResolveAudience=((uint64_t)1<<44); // when a definitely specified audience is a gendered pronoun with more than one speaker in the current speaker group having that gender
	static const uint64_t flagRelativeHead=((uint64_t)1<<43); 
	static const uint64_t flagQuoteContainsSpeaker=((uint64_t)1<<42); 
	static const uint64_t flagResolveMetaGroupByGender=((uint64_t)1<<41); 
	static const uint64_t flagEmbeddedStoryResolveSpeakersGap=((uint64_t)1<<40); 
	static const uint64_t flagEmbeddedStoryBeginResolveSpeakers=((uint64_t)1<<39); // only set if aged
	static const uint64_t flagAge=((uint64_t)1<<38); // only set if aged
	// set dynamically? 
	static const uint64_t flagVAnalysis=((uint64_t)1<<37);
	static const uint64_t flagTempVNReAnalysis=((uint64_t)1<<36);
	static const uint64_t flagInPStatement=((uint64_t)1<<35);
	static const uint64_t flagInQuestion=((uint64_t)1<<34);
	static const uint64_t flagRelationsAlreadyEvaluated=((uint64_t)1<<32);
	static const uint64_t flagNounOwner=((uint64_t)1<<31);
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
#define moreReliableMatchedFlags (cWordMatch::flagDefiniteResolveSpeakers|cWordMatch::flagAudienceFromSpeakerGroupResolveAudience|cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers|cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers|cWordMatch::flagEmbeddedStoryResolveSpeakers)
#define moreReliableNotMatchedFlags (cWordMatch::flagFromLastDefiniteResolveAudience|cWordMatch::flagSpecifiedResolveAudience|cWordMatch::flagHailedResolveAudience|cWordMatch::flagAlternateResolveBackwardFromDefiniteAudience|cWordMatch::flagAlternateResolveForwardFromLastSubjectAudience)
	void setForm(void);
	void setPreferredForm(void);
	bool costable(void);
	// Primary constructor, used by tokenize.cpp for every token it appends to cSource::m.
	//   inWord         - lexicon entry for this token (never wNULL in normal use).
	//   inAdjustedForms- initial value of 'flags' (the form/hint flags the tokenizer already
	//                    knows: capitalization, all-caps, newline-before, inserted quote, ...).
	//   trace          - the source-wide trace settings, copied field by field into t so that
	//                    per-position tracing can be toggled later without touching the source.
	// All index members are initialized to -1 ("unset") and the two cost accumulators to
	// 1000000 ("no cost known yet, anything is cheaper").  Contrast with the default
	// constructor at the bottom of this class, which zeroes them instead.
	cWordMatch(tIWMM inWord,uint64_t inAdjustedForms, sTrace &trace)
	{
		word=inWord;
		flags=inAdjustedForms;
		minAvgCostAfterAssessCost=1000000;
		maxLACAACMatch=0;
		lastWinnerLACAACMatchPMAOffset = -1;
		whereLastWinnerLACAACMatchPMAOffset = -1;
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
		hasSyntacticRelationGroup=false;
		andChainType=false;
		notFreePrep=false;
		hasVerbRelations=false;
		t.printBeforeElimination=trace.printBeforeElimination;
		t.traceSubjectVerbAgreement=trace.traceSubjectVerbAgreement;
		t.traceTestSubjectVerbAgreement = trace.traceTestSubjectVerbAgreement;
		t.traceEVALObjects=trace.traceEVALObjects;
		t.traceAnaphors=trace.traceAnaphors;
		t.traceRelations=trace.traceRelations;
		t.traceTestSyntacticRelations = trace.traceTestSyntacticRelations;
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
		t.traceTransformDestinationQuestion = trace.traceTransformDestinationQuestion;
		t.traceLinkQuestion = trace.traceLinkQuestion;
		t.traceMapQuestion=trace.traceMapQuestion;
		t.traceQuestionPatternMap=trace.traceQuestionPatternMap;
		t.collectPerSentenceStats=trace.collectPerSentenceStats;
		t.traceParseInfo = trace.traceParseInfo;
		t.tracePreposition = trace.tracePreposition;
		t.tracePatternMatching = trace.tracePatternMatching;
		t.traceTime = trace.traceTime;
		logCache=::logCache;
		originalPreferredViterbiForm = 0;
		preferredViterbiCurrentTagOfHighestProbability = 0;
		preferredViterbiMaximumProbability = 0;
		preferredViterbiPreviousTagOfHighestProbability = 0;
		preferredViterbiProbability = 0;
		sameSourceCopy = -1;
	};
	tIWMM word;  // points to WMM array
	// The lexicon entry this token should be counted under: the word's own main (dictionary)
	// entry if it has one, otherwise the word itself.  Never returns wNULL for a valid word.
	tIWMM getMainEntry(void)
	{
		return (word->second.mainEntry==wNULL) ? word : word->second.mainEntry;
	}
	// Return the lexicon entry for the uninflected form of this token, deriving it
	// morphologically if the lexicon does not already record a suitable mainEntry.
	//   isVerb/isNoun - which category to normalize to (first person singular present for a
	//                   verb, singular for a noun).  If neither is set, falls back to
	//                   getMainEntry().
	//   where/fromWhere            - source position and caller id, for logging only.
	//   lastNounNotFound/lastVerbNotFound - in/out cache of the last failed derivation, used by
	//                   ::deriveMainEntry to avoid repeating dictionary/DB lookups.
	// Single-character words, numerals, pronouns/determiners and non-nouns are returned
	// unchanged (they have no useful main entry).  On the derivation path the result is
	// Words.query(derived) which can be wNULL if the derived form is not in the lexicon, so
	// callers must check.
	tIWMM deriveMainEntry(int where,int fromWhere,bool isVerb,bool isNoun,lpwstring &lastNounNotFound,lpwstring &lastVerbNotFound)
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
			if (word->second.query(nomForm)>=0 || word->second.query(personalPronounAccusativeForm)>=0 || word->second.query(personalPronounForm)>=0) return word;
			if (word->second.query(possessiveDeterminerForm)>=0 || word->second.query(possessivePronounForm)>=0 || word->second.query(reflexivePronounForm)>=0) return word;
			if (isNoun && word->second.query(nounForm)<0) return word;
			lpwstring in=word->first;
		::deriveMainEntry(where,fromWhere,in,inflectionFlags,isVerb,isNoun,lastNounNotFound,lastVerbNotFound);
		return Words.query(in);
		}
		else
			return word;
	}
	// Noun-only specialization of deriveMainEntry: returns the singular lexicon entry for this
	// token.  Returns the token itself when it is already singular, is one word long, is a
	// numeral/pronoun/determiner, or has no noun form at all; otherwise returns
	// Words.query(singularized form), which may be wNULL if that form is unknown.
	tIWMM getNounME(int where,int fromWhere,lpwstring &lastNounNotFound,lpwstring &lastVerbNotFound)
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
			if (word->second.query(nomForm)>=0 || word->second.query(personalPronounAccusativeForm)>=0 || word->second.query(personalPronounForm)>=0) return word;
			if (word->second.query(possessiveDeterminerForm)>=0 || word->second.query(possessivePronounForm)>=0 || word->second.query(reflexivePronounForm)>=0) return word;
			if (word->second.query(nounForm)<0) return word;
			lpwstring in=word->first;
			int inflectionFlags=word->second.inflectionFlags; // avoid altering original inflectionFlags
			::deriveMainEntry(where,fromWhere,in,inflectionFlags,false,true,lastNounNotFound,lastVerbNotFound);
			return Words.query(in);
		}
		else
			return word;
	}
	// Verb-only specialization of deriveMainEntry: returns the first-person-singular-present
	// lexicon entry for this token, or the token itself if it is already in that form or is a
	// single character.  May return wNULL when the derived form is not in the lexicon.
	// Unlike getNounME this does not screen out pronouns/numerals, because it is only called
	// from positions already known to carry a verb form.
	tIWMM getVerbME(int where,int fromWhere,lpwstring &lastNounNotFound,lpwstring &lastVerbNotFound)
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
			lpwstring in=word->first;
			int inflectionFlags=word->second.inflectionFlags; // avoid altering original inflectionFlags
			::deriveMainEntry(where,fromWhere,in,inflectionFlags,true,false,lastNounNotFound,lastVerbNotFound);
			return Words.query(in);
		}
		else
			return word;
	}
	uint64_t flags;                 // see the class comment: high bits global, low 32 bits context dependent
	unsigned short maxMatch;                // length (in positions) of the longest pattern match starting here
	int minAvgCostAfterAssessCost;          // lowest average cost seen after assessCost ran; 1000000 = none yet
	int lowestAverageCost;                   // lowest average cost of any match starting here; 1000000 = none yet

	// used in HMM Viterbi training and testing
	int originalPreferredViterbiForm;
	vector <int> preferredViterbiForms; // gives form # relative to Forms, not form offset relative to this word.
	                                   // -2 is pushed for the sentence-boundary pseudo-tag "--s--".
	double preferredViterbiProbability;
	double preferredViterbiMaximumProbability;
	int preferredViterbiPreviousTagOfHighestProbability;
	int preferredViterbiCurrentTagOfHighestProbability;

	unsigned short maxLACMatch;    // longest match among the lowest-average-cost matches
	unsigned short maxLACAACMatch; // longest match among lowest-average-cost/lowest-added-average-cost matches
	short lastWinnerLACAACMatchPMAOffset; // only used during tracing
	int whereLastWinnerLACAACMatchPMAOffset; // only used during tracing
	uint64_t objectRole;   // bitfield of syntactic/semantic roles assigned to this position (see relationTypes.h)
	int verbSense;                 // verb sense id for verb positions; 0 = unset
	unsigned char timeColor;       // time-segment shading used by timeRelations.cpp / logging
	int beginPEMAPosition;         // first entry in cSource::pema belonging to this position, -1 = none
	int endPEMAPosition;           // one past the last pema entry for this position, -1 = none
	unsigned int PEMACount;        // number of pema entries for this position
	cPatternMatchArray pma;        // competing pattern matches that start at this position
	cBitObject<> forms;            // bitmap of the word forms (parts of speech) allowed here; default template
	                               // parameters give 16 x 32 = 512 bits, indexed by form number
	//bitObject winnerForms; // used for BNC to remember tagged form
	cBitObject<32, 5, unsigned int, 32> patterns; // bitmap of pattern ids matched at this position (32 x 32 = 1024 bits)

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
	int sameSourceCopy; // when a phrase is duplicated within this same source (question transformation /
	                    // copySource), the position in m this position was copied from; -1 if not a copy
	// Source position of the verb this subject/object belongs to, or -1.
	int getRelVerb()
	{
		return relVerb;
	}
	// Set the verb link.  Only -1 (unset) or a real source position is legal; anything
	// below -1 is fatal.
	void setRelVerb(int rv)
	{
		if (rv < -1)
			lplog(LOG_FATAL_ERROR, u"Illegal relVerb.");
		relVerb = rv;
	}
	int relPrep; // subjects, objects and verbs should have this set to the prepositions of the prep phrases, 
							 // and prep phrases themselves have them set to the next prep phrase in the sentence
	// Set the preposition link and echo it back, so callers can write "x=setRelPrep(y)".
	// The interesting logic lives in cSource::setRelPrep(where,...), which decides what to link.
	int setRelPrep(int rp) { 
		relPrep=rp; 
		return rp;
	}
	// I passed two Johnnies in the street talking about ... / passed is 'two Johnnies' relVerb, relInternalVerb is set to 'talking'
	int relInternalVerb; // he wanted you to come to the dance.  relVerb of you is 'wanted' relInternalVerb of you is 'come'
	// relInternalObject is also used for another object of the same subject location but the object is of another verb.  This verb may be before the subject (_INTRO_S1)
	int relInternalObject; // I guess you have no right to ... relInternalObject of I is 'you' - this is to prevent LL routines 
	int beginObjectPosition,endObjectPosition; // where is the object defined on this position begin and end?
	                                           // half-open range [begin,end) of positions in m; -1 = no object here
	vector <cOM> objectMatches;         // entities this position resolves to (pronoun/name coreference result).
	                                    // For a quote position these are the speakers of the quote.
	vector <cOM> audienceObjectMatches; // for a quote position, the entities being addressed
	// Next quote in the same paragraph, or -1.  For a verb position this same storage holds
	// tsSense instead (see the private declaration), so only read it on quote positions.
	int getQuoteForwardLink() { return quoteForwardLink; }
	void setQuoteForwardLink(int qfl) 
	{ 
		quoteForwardLink = qfl; 
	}
	int quoteBackLink; // previous quote in same paragraph 
	int nextQuote; // set at the beginning of the quote to the beginning of the next quote in a separate paragraph
	                    // if the word is a preposition, this points to the head of the preposition phrase
	                    //  Krugman earned his B.A. in economics from Yale University summa cum laude in 1974 and his PhD from the Massachusetts Institute of Technology (MIT) in 1977.
	                    // the head of 'in' economics,'from' Yale,'in' 1974 = his 'B.A.'
	                    // the head of 'from' Institute, 'in' 1977 = his 'PhD'
	int previousQuote; // set at the beginning of the quote to the beginning of the previous quote in a separate paragraph
	int endQuote; // matching end quote
	int tmpWinnerForms; // bitmap (by form offset within this word, not global form number) of the forms
	                    // that survived pattern winnowing.  0 means "no winnowing yet" and is treated
	                    // by isWinner() as "every form is still a winner".  Only 31 usable bits.
	int skipResponse; // not saved
	int embeddedStorySpeakerPosition; // tracks who is speaking when they are relating an event that happened in the past
	int audiencePosition,speakerPosition; // positions in m of the audience/speaker resolved for this quote; -1 = unresolved
	sTrace t;                             // per-position copy of the trace flags (see the constructor)
	bool hasSyntacticRelationGroup,hasVerbRelations,andChainType,notFreePrep;
	short logCache;
	lpwstring baseVerb;                            // uninflected verb string cached by getBaseVerb
	lpwstring questionTransformationSuggestedPattern;
	vector <int> PEMAWinners;  // pema offsets that won at this position
	vector <int> PMAWinners;   // pma offsets that won at this position
	
	// Mark form offset 'form' (relative to this word's form list) as a surviving winner.
	// An out-of-range form number is fatal (lplog(LOG_FATAL_ERROR) terminates).
	void setWinner(int form)
	{
		if (form >= sizeof(tmpWinnerForms) * 8)
			lplog(LOG_FATAL_ERROR, u"overFlow on tmpWinnerForms (1)!");
		tmpWinnerForms |= (1 << form);
	}
	// Clear the winner bit for form offset 'form'.
	void unsetWinner(int form)
	{
		if (form >= sizeof(tmpWinnerForms) * 8)
			lplog(LOG_FATAL_ERROR, u"overFlow on tmpWinnerForms (1)!");
		tmpWinnerForms &= ~(1 << form);
	}
	// Forget all winnowing decisions for this position.  Because 0 doubles as "not winnowed",
	// this makes isWinner() answer true for every form again.
	void unsetAllFormWinners()
	{
		tmpWinnerForms = 0;
	}
	// Record the HMM/Viterbi-preferred tag for this token.
	//   form        - form name; the special value u"--s--" is the sentence-boundary tag and is
	//                 stored as -2 in preferredViterbiForms.
	//   probability - Viterbi probability of that tag, stored only on the success path.
	// Returns false (and logs) if 'form' is not one of the forms this word can take; the
	// preferred-form list is then left unchanged.
	bool setPreferredViterbiForm(lpwstring form,double probability)
	{
		if (form == u"--s--")
		{
			preferredViterbiForms.push_back(-2);
			return true;
		}
		else
		{
			int preferredViterbiForm = queryForm(form);
			if (preferredViterbiForm < 0)
			{
				lplog(LOG_ERROR, u"Form %s not found as one of the forms for word %s in Viterbi processing.", form.c_str(), word->first.c_str());
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
	// Dry run of setPreferredViterbiForm: returns true if 'form' is the sentence-boundary tag or
	// is a legal form for this word, false (with a log line) otherwise.  Stores nothing.
	bool testPreferredViterbiForm(lpwstring form)
	{
		if (form != u"--s--" && queryForm(form) < 0)
		{
			lplog(LOG_ERROR, u"*Interim form %s not found as one of the forms for word %s in Viterbi processing.", form.c_str(), word->first.c_str());
			return false;
		}
		return true;
	}
	void setSeparatorWinner(void);
	bool maxWinner(int len,int avgCost,int lowestSeparatorCost);
	// if there is no winner, display every form
	// True if form offset 'form' survived winnowing.  When tmpWinnerForms is 0 (nothing has been
	// winnowed yet) every form is reported as a winner, which is what the printing code wants.
	// Out-of-range offsets return false after logging (note the log line prints the 'forms'
	// bitmap object rather than the offending 'form' value).
	bool isWinner(int form)
	{
		if (form>=sizeof(tmpWinnerForms)*8)
		{
			lplog(LOG_ERROR,u"overFlow on tmpWinnerForms form=%d tmpWinnerForms=%d (2)!",forms,tmpWinnerForms);
			return false;
		}
		return (tmpWinnerForms) ? ((1<<form)&tmpWinnerForms)!=0 : true;
	}
	// True if 'form' (a global form number, unlike isWinner's offset) is the single surviving
	// form at this position: either nothing was winnowed and the word has exactly one form, or
	// the winner bitmap contains exactly that form's bit.  Note the query() result is not
	// checked for -1 on the second path, so calling this with a form the word cannot take
	// evaluates a negative shift.
	bool isOnlyWinner(int form)
	{
		int formIndex = word->second.query(form);
		if (formIndex!=-1 && !tmpWinnerForms) return formsSize() == 1;
		return (1<< formIndex)==tmpWinnerForms;
	}
	lpwstring roleString(lpwstring &sRole);
	// Log this position's syntactic-relation links at LOG_RESOLUTION.  If no relation link is
	// set, only the role bitfield is printed (and nothing at all if there is no role either);
	// the detailed link dump is gated on t.traceSpeakerResolution.  'where' is this position,
	// printed as the %06d line prefix.
	void logRelations(int where)
	{
		lpwstring rs;
		if (relNextObject==-1 && nextCompoundPartObject==-1 && previousCompoundPartObject==-1 && relInternalObject==-1 && 
				relSubject==-1 && relVerb==-1 && relObject==-1 && relPrep==-1 && relInternalVerb==-1)
		{
			if (objectRole)
				lplog(LOG_RESOLUTION,u"%06d:role=(%s)",where,roleString(rs).c_str());
			return;
		}
		if (t.traceSpeakerResolution)
		{
			lpwstring rs2;
			if (relNextObject==-1 && nextCompoundPartObject==-1 && previousCompoundPartObject==-1 && relInternalObject==-1)
				lplog(LOG_RESOLUTION,u"%06d:role=(%s) relSubject=%d,relVerb=%d,relObject=%d,relPrep=%d,relInternalVerb=%d",
					where,roleString(rs2).c_str(),relSubject,relVerb,relObject,relPrep,relInternalVerb);
			else
				lplog(LOG_RESOLUTION,u"%06d:role=(%s) relSubject=%d,relVerb=%d,relObject=%d,relNextObject=%d,nextCompoundPartObject=%d,previousCompoundPartObject=%d,relPrep=%d,relInternalVerb=%d,relInternalObject=%d",
					where,roleString(rs2).c_str(),relSubject,relVerb,relObject,relNextObject,nextCompoundPartObject,previousCompoundPartObject,relPrep,relInternalVerb,relInternalObject);
		}
	}
	// Reset the links that only make sense in the position's original context, after this
	// cWordMatch has been copied into another source (copySource / question transformation).
	// Everything that points at a position or object index is set back to -1 and the resolved
	// object matches are dropped, so the copy can be re-resolved from scratch in its new
	// document.  Note that audienceObjectMatches, quoteBackLink, principalWhere*Position and
	// the PEMA/PMA bookkeeping are deliberately or accidentally left untouched here.
	void clearAfterCopy()
	{
		originalObject = relNextObject = nextCompoundPartObject = previousCompoundPartObject = relSubject = sameSourceCopy = relPrep = relInternalVerb = relInternalObject = nextQuote = previousQuote = endQuote = embeddedStorySpeakerPosition = audiencePosition = speakerPosition = -1;
		setRelObject(-1);
		setRelVerb(-1);
		setQuoteForwardLink(-1);
		beginObjectPosition = -1;
		endObjectPosition = -1;
		setObject(-1);
		objectMatches.clear();
	}
	void adjustReferences(int index,bool keepObjects, unordered_map <int, int>& transformSourceToQuestionSourceMap);
	void adjustReferences(int index, int offset);
	void adjustValue(int& val, lpwstring valString, unordered_map <int, int>& sourceIndexMap);
	bool isPPN(void);
	tIWMM resolveToClass();
	// in the case of a proper noun, it increases the size of the form by 1
	vector <int> getForms();
	int queryForm(int form);
	int queryForm(lpwstring sForm);
	int queryWinnerForm(int form);
	int queryWinnerForm(lpwstring sForm);
	lpwstring patternWinnerFormString(lpwstring &forms);
	lpwstring winnerFormString(lpwstring &formsString,bool withCost=true);
	void getWinnerForms(vector <int> &winnerForms);
	int getNumWinners();
	bool hasWinnerVerbForm(void);
	bool hasWinnerNounForm(void);
	bool isWinnerSeparator(void);
	bool isPhysicalObject(void);
	bool isTopLevel(void);
	bool isNounType(void);
	bool isModifierType(void);
	unsigned int formsSize(void);
	unsigned int getFormNum(unsigned int line);
	bool updateMaxMatch(int len,int avgCost);
	bool updateMaxMatch(int len,int avgCost,int lowerAvgCost);
	bool compareCost(int AC1,int LEN1,int lowestSeparatorCost,int pmaOffset, int fromWhere, int &reason, bool alsoSet);
	unsigned int getShortFormInflectionEntry(int line,lpchar_t *entry,size_t entryCount); // batch B5: entryCount
	unsigned int getShortAllFormAndInflectionLen(void);
	int getInflectionLength(int inflection,tInflectionMap *map);
	bool isGendered(void);
	bool isPossessivelyGendered(bool &possessivePronoun);
	bool updateFormUsagePatterns(void);
	void logFormUsageCosts(void);
	// Index into cSource::objects of the entity that starts (has its principalWhere) at this
	// position, or -1.  Accessors exist only so that the assignment can be traced/breakpointed.
	int getObject() { return object; }
	void setObject(int o)
	{
		object=o;
	}
	// Source position of the object related to this subject (or of the explicitly named other
	// speaker, when flagQuoteContainsSpeaker is set); -1 if none.  Returns the value assigned.
	int getRelObject() { return relObject; }
	int setRelObject(int ro)
	{
		return relObject=ro;
	}
	// True if this token is an opening/closing single or double quotation mark.  Only the first
	// character is examined, which is enough because the tokenizer emits quotes as single-
	// character tokens.
	bool isQuote()
	{
		return cWord::isSingleQuote(word->first[0]) || cWord::isDoubleQuote(word->first[0]);
	}
	bool writeFlags(void* buffer, int& where, int limit);
	bool writeRef(void *buffer,int &where,int limit);
	bool readWord(const lpwstring temp, int sourceType);
	bool readFlags(char* buffer, int& where, int limit);
	bool read(char *buffer,int &where,int limit,int sourceType);
	void accumulateStatistics(unordered_map<lpwstring, int> &defaultMap);
	// Deserialize one token from the binary source cache; 'error' is set true if read() failed
	// (truncated buffer or a word that is no longer in the lexicon).  'sourceType' selects the
	// cSource::sourceTypeEnum-specific parts of the format.
	cWordMatch(char *buffer,int &where,int limit,int sourceType,bool &error)
	{
		error=!read(buffer,where,limit,sourceType);
	}
	// Default constructor, needed so cWordMatch can live in a vector.  Beware: unlike the
	// tokenizer constructor above it zeroes every index and cost field instead of using -1 /
	// 1000000, so a default-constructed entry claims to reference object 0 and position 0 and
	// to have zero cost; 'word' is also left as a default-constructed (invalid) lexicon
	// iterator rather than wNULL.  Only use it as a placeholder that is immediately overwritten.
	cWordMatch(void)
	{
			PEMACount=0;
			andChainType=0;
			audiencePosition=0;
			beginObjectPosition=0;
			beginPEMAPosition=0;
			embeddedStorySpeakerPosition=0;
			endObjectPosition=0;
			endPEMAPosition=0;
			endQuote=0;
			flags=0;
			hasVerbRelations=0;
			lastWinnerLACAACMatchPMAOffset=0;
			logCache=0;
			lowestAverageCost=0;
			maxLACAACMatch=0;
			maxLACMatch=0;
			maxMatch=0;
			minAvgCostAfterAssessCost=0;
			nextCompoundPartObject=0;
			nextQuote=0;
			notFreePrep=0;
			object=0;
			objectRole=0;
			originalObject=0;
			originalPreferredViterbiForm=0;
			preferredViterbiCurrentTagOfHighestProbability=0;
			preferredViterbiMaximumProbability=0;
			preferredViterbiPreviousTagOfHighestProbability=0;
			preferredViterbiProbability=0;
			previousCompoundPartObject=0;
			previousQuote=0;
			principalWhereAdjectivalPosition=0;
			principalWherePosition=0;
			quoteBackLink=0;
			quoteForwardLink=0;
			relInternalObject=0;
			relInternalVerb=0;
			relNextObject=0;
			relObject=0;
			relPrep=0;
			relSubject=0;
			relVerb=0;
			skipResponse=0;
			hasSyntacticRelationGroup=0;
			speakerPosition=0;
			timeColor=0;
			tmpWinnerForms=0;
			verbSense=0;
			whereLastWinnerLACAACMatchPMAOffset=0;
			sameSourceCopy = -1;
	}
private:
	int object; // this is an index into the objects array.  it is set at the principalWhere of an object
	int relObject; // if this is a subject, what object does it relate to? (for pronoun disambiguation) - also used if speaker explicitly names another speaker (flagQuoteContainsSpeaker)
	int relVerb; // for subjects and objects
	// the next field is also used to store tsSense for verbs
	int quoteForwardLink; // next quote in same paragraph (or tsSense)
};
extern vector <cWordMatch>::iterator wmNULL; // "null" token iterator; compare against it instead of using NULL

// One entity currently in local focus, i.e. a candidate for the next pronoun/speaker
// resolution.  cSource::localObjects is the focus window; entries are pushed as entities are
// encountered, aged as sentences go by (increaseAge/decreaseAge/resetAge), and cleared at
// section boundaries.  cObject::lsiOffset points back at this entry for the object.
//
// The three age counters exist because salience decays differently inside and outside quotes:
// text inside quotes and text outside quotes are effectively two interleaved narratives, and a
// mention in one should not fully refresh the other.  Which counter is consulted is decided by
// setSalienceAgeMethod/salienceInQuote/salienceIndependent.
class cLocalFocus
{
public:
	cOM om; // index into objects also a salience factor
	int numEncounters;                      // total mentions of this entity while in focus
	int numIdentifiedAsSpeaker;             // mentions where it was taken to be the speaker
	int numDefinitelyIdentifiedAsSpeaker;   // subset of the above that were unambiguous ("said Bill")
	int lastRoleSalience;                   // salience contributed by the syntactic role of the last mention
	int lastWhere;                          // position in m of the most recent mention, -1 = none yet
	int previousWhere;                      // position of the mention before that, -1 = none
	int numMatchedAdjectives;               // adjectives that matched between mention and entity (raises salience)
	int newPPAge; // the age of a new physically present entity - used with introductions
	int lastExit; // position in source of last exit of object - used in determining whether the exit was true
	int lastEntrance; // position in source of last explicit entrance of object 
	int whereBecamePhysicallyPresent; // position where the object last became physically present.
	bool notSpeaker;                  // known not to be a speaker (e.g. mentioned as absent)
	bool lastSubject; // // last subject preference used in chooseBest
	bool occurredInPrimaryQuote;      // has been mentioned inside a primary (outermost) quote
	bool occurredOutsidePrimaryQuote; // has been mentioned in narration
	bool occurredInSecondaryQuote;    // has been mentioned inside a quote within a quote
	bool physicallyPresent;           // believed to be present in the current scene
	lpwstring res;                      // human-readable trace of how salience was accumulated (logging only)
	// (Re)initialize every counter for a fresh appearance of this entity.
	//   inPrimaryQuote/inSecondaryQuote - whether the mention that created this focus entry was
	//   inside a quote.  The age counter for the *other* context is set to -1, meaning "never
	//   seen there", so that the entity does not appear artificially recent in that context.
	// Does not touch om, so the caller's object/salience survive a re-init.
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
	// Focus entry with no entity attached yet (om.object stays -1).
	cLocalFocus(bool inPrimaryQuote,bool inSecondaryQuote)
	{
		init(inPrimaryQuote,inSecondaryQuote);
	}
	// Focus entry for an already-scored match.  ns = known not to be a speaker,
	// pp = believed physically present.
	cLocalFocus(cOM lom,bool inPrimaryQuote,bool inSecondaryQuote,bool ns,bool pp)
	{
		init(inPrimaryQuote,inSecondaryQuote);
		om=lom;
		notSpeaker=ns;
		physicallyPresent=pp;
	};
	// Focus entry for an object index, leaving om.salienceFactor at its default 0.
	cLocalFocus(int object,bool inPrimaryQuote,bool inSecondaryQuote,bool ns,bool pp)
	{
		init(inPrimaryQuote,inSecondaryQuote);
		om.object=object;
		notSpeaker=ns;
		physicallyPresent=pp;
	};
	// Trivial named readers for the two salience-mode flags that resolution passes around; they
	// exist so the call sites document which mode they are testing (the flags used to be static
	// members of this class - see the commented-out declaration below).
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
	// Decide, once per entity being resolved, which age counter drives salience.
	//   inObjectToBeMatchedInQuote      - is the position being resolved inside a quote?
	//   objectToBeMatchedIsOnlyNeuter   - neuter-only entities (things, not people) ignore the
	//                                     quote/narration split entirely, because "it" refers
	//                                     across that boundary freely.
	// Both decisions are written to the out parameters (which the caller keeps for the whole
	// resolution) and the quote-independent decision is also returned.
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
	GEOGRAPHICAL_URBAN_SUBFEATURE, // rooms within buildings - perhaps a different location (see syntacticRelationGroups)
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
extern const lpchar_t *OCSubTypeStrings[];

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
class cObject
{
public:
	int dbIndex;
	enum OC objectClass;
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
	int firstLocation; // the very first location for this object including replacements
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
	vector <int> syntacticRelationGroups;
	set <int> duplicates; // objects that this object replaced
	vector <int> aliases; // objects that this object is the same as (but the information cannot be merged, as in duplicates)
	vector <tIWMM> associatedNouns; // man
	vector <tIWMM> associatedAdjectives; // young, old, strange
	vector <int> possessions;
	map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
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
	bool ito;
	bool getIsTimeObject() {
		return ito;
	}
	bool setIsTimeObject(bool setITO)
	{
		lplog(LOG_RESOLUTION, u"set object at where = %d to %s", begin, (setITO) ? u"true" : u"false");
		ito = setITO;
		return ito;
	}
	bool isLocationObject;
	bool isWikiPlace;
	bool isWikiPerson;
	bool isWikiBusiness;
	bool isWikiWork;

	cLastVerbTenses lastVerbTenses[VERB_HISTORY];
	inline static vector <lpwstring> wordOrderWords = { u"other", u"another", u"second", u"first", u"third", u"former", u"latter", u"that", u"this", u"two", u"three", u"one", u"four", u"five", u"six", u"seven", u"eight" };
	enum eOBJECTS {
		UNKNOWN_OBJECT = -1, OBJECT_UNKNOWN_MALE = -2, OBJECT_UNKNOWN_FEMALE = -3,
		OBJECT_UNKNOWN_MALE_OR_FEMALE = -4, OBJECT_UNKNOWN_NEUTER = -5, OBJECT_UNKNOWN_PLURAL = -6, OBJECT_UNKNOWN_ALL = -7
	};

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
	int getFirstSpeakerGroup() { return firstSpeakerGroup; }
	void setFirstSpeakerGroup(int fsg) { firstSpeakerGroup = fsg; }
	int sanityCheck(int maxSourcePosition, int maxObjectIndex, int maxSpeakerGroupsIndex, vector <cWordMatch> &m)
	{
		if (objectClass< PRONOUN_OBJECT_CLASS || objectClass>GENDERED_RELATIVE_OBJECT_CLASS) return 300;
		if (subType >= NUM_SUBTYPES) return 301;
		if (begin < 0 || begin >= maxSourcePosition) 
			return 302;
		if (end < 0 || end > maxSourcePosition) return 303; // the end of an object may = m.size()
		if (originalLocation < 0 || originalLocation >= maxSourcePosition) return 304;
		if (PMAElement < -1 || PMAElement >= (signed)m[begin].pma.count) return 305;
		if (replacedBy < -1 || replacedBy >= maxObjectIndex) return 306;
		if (ownerWhere < -2 - (signed)cObject::wordOrderWords.size() || ownerWhere >= maxSourcePosition) return 307;
		if (firstLocation < -1 || firstLocation >= maxSourcePosition) return 308;
		if (firstSpeakerGroup < -1 || firstSpeakerGroup >= maxSpeakerGroupsIndex) return 309;
		if (firstPhysicalManifestation < -1 || firstPhysicalManifestation >= maxSourcePosition) return 310;
		if (lastSpeakerGroup < -1 || lastSpeakerGroup >= maxSpeakerGroupsIndex) return 311;
		if (whereRelativeClause < -1 || whereRelativeClause >= maxSourcePosition) return 312;
		if (relativeClausePM < -1 || (relativeClausePM>=0 && relativeClausePM >= (signed)m[whereRelativeClause].pma.count)) return 313;
		if (whereRelSubjectClause < -1 || whereRelSubjectClause >= maxSourcePosition) return 314;
		if (lastWhereLocation < -1 || lastWhereLocation >= maxSourcePosition) return 315;
		for (int di : duplicates)
			if (di < 0 || di >= maxObjectIndex) return 316;
		for (int di : aliases)
			if (di < 0 || di >= maxObjectIndex) return 317;
		/*
		for (tIWMM n : associatedNouns)
		{
			if (n != wNULL && (n - beginWord) >= maxWordIndex) return false;
		}
		for (tIWMM n : associatedAdjectives)
		{
			if (n != wNULL && (n - beginWord) >= maxWordIndex) return false;
		}
		for (map <tIWMM, int, cSourceWordInfo::cRMap::wordMapCompare>::iterator gnm = genericNounMap.begin(), gnmEnd = genericNounMap.end(); gnm != gnmEnd; gnm++)
			if (gnm->first != wNULL && (gnm->first - beginWord) >= maxWordIndex) return false;
			*/
		return 0;
	}
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
		setIsTimeObject(false);
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
		subType = -1;
		partialMatch=false;
		isKindOf=false;
		wikipediaAccessed=false;
		setIsTimeObject(false);
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
		clear(true);
		if (error=!copy(dbIndex,buffer,where,total)) return; 
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
		if (error=!copy(syntacticRelationGroups,buffer,where,total)) return;
		if (error=!copy(duplicates,buffer,where,total)) return;
		if (error=!copy(aliases,buffer,where,total)) return;
		int num;
		lpwstring str;
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
		// map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
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
		int64_t flags;
		if (error=!copy(flags,buffer,where,total)) return;
		readFlags(flags);
		for (int I=0; I<VERB_HISTORY; I++)
			if (error=!copy(lastVerbTenses[I],buffer,where,total)) return;
		if (error=!copy(name,buffer,where,total)) return;
	}
	void readFlags(int64_t flags)
	{
		isWikiWork = flags & 1; flags >>= 1;
		isWikiBusiness = flags & 1; flags >>= 1;
		isWikiPerson = flags & 1; flags >>= 1;
		isWikiPlace = flags & 1; flags >>= 1;
		isLocationObject = flags & 1; flags >>= 1;
		setIsTimeObject(flags & 1); flags >>= 1;
		dbPediaAccessed = flags & 1; flags >>= 1;
		container = flags & 1; flags >>= 1;
		wikipediaAccessed = flags & 1; flags >>= 1;
		isKindOf = flags & 1; flags >>= 1;
		genderNarrowed = flags & 1; flags >>= 1;
		isNotAPlace = flags & 1; flags >>= 1;
		partialMatch = flags & 1; flags >>= 1;
		ambiguous = flags & 1; flags >>= 1;
		verySuspect = flags & 1; flags >>= 1;
		suspect = flags & 1; flags >>= 1;
		multiSource = flags & 1; flags >>= 1;
		eliminated = flags & 1; flags >>= 1;
		ownerNeuter = flags & 1; flags >>= 1;
		ownerFemale = flags & 1; flags >>= 1;
		ownerMale = flags & 1; flags >>= 1;
		ownerPlural = flags & 1; flags >>= 1;
		neuter = flags & 1; flags >>= 1;
		female = flags & 1; flags >>= 1;
		male = flags & 1; flags >>= 1;
		plural = flags & 1; flags >>= 1;
		identified = flags & 1;
	}
	bool writeFlags(void* buffer, int& where, unsigned int limit)
	{
		int64_t flags = 0;
		flags |= (identified) ? 1 : 0; flags <<= 1;
		flags |= (plural) ? 1 : 0; flags <<= 1;
		flags |= (male) ? 1 : 0; flags <<= 1;
		flags |= (female) ? 1 : 0; flags <<= 1;
		flags |= (neuter) ? 1 : 0; flags <<= 1;
		flags |= (ownerPlural) ? 1 : 0; flags <<= 1;
		flags |= (ownerMale) ? 1 : 0; flags <<= 1;
		flags |= (ownerFemale) ? 1 : 0; flags <<= 1;
		flags |= (ownerNeuter) ? 1 : 0; flags <<= 1;
		flags |= (eliminated) ? 1 : 0; flags <<= 1;
		flags |= (multiSource) ? 1 : 0; flags <<= 1;
		flags |= (suspect) ? 1 : 0; flags <<= 1;
		flags |= (verySuspect) ? 1 : 0; flags <<= 1;
		flags |= (ambiguous) ? 1 : 0; flags <<= 1;
		flags |= (partialMatch) ? 1 : 0; flags <<= 1;
		flags |= (isNotAPlace) ? 1 : 0; flags <<= 1;
		flags |= (genderNarrowed) ? 1 : 0; flags <<= 1;
		flags |= (isKindOf) ? 1 : 0; flags <<= 1;
		flags |= (wikipediaAccessed) ? 1 : 0; flags <<= 1;
		flags |= (container) ? 1 : 0; flags <<= 1;
		flags |= (dbPediaAccessed) ? 1 : 0; flags <<= 1;
		flags |= (getIsTimeObject()) ? 1 : 0; flags <<= 1;
		flags |= (isLocationObject) ? 1 : 0; flags <<= 1;
		flags |= (isWikiPlace) ? 1 : 0; flags <<= 1;
		flags |= (isWikiPerson) ? 1 : 0; flags <<= 1;
		flags |= (isWikiBusiness) ? 1 : 0; flags <<= 1;
		flags |= (isWikiWork) ? 1 : 0;
		if (!copy(buffer, flags, where, limit)) return false;
		return true;
	}
	bool write(void *buffer,int &where,unsigned int limit)
	{
		if (!copy(buffer,dbIndex,where,limit)) return false; 
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
		if (!copy(buffer,syntacticRelationGroups,where,limit)) return false;
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
		// map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
		if (!copy(buffer,(int)genericNounMap.size(),where,limit)) return false; 
		for (map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare>::iterator gnm=genericNounMap.begin(),gnmEnd=genericNounMap.end(); gnm!=gnmEnd; gnm++)
		{
			if (!copy(buffer,gnm->first->first,where,limit)) return false;
			if (!copy(buffer,gnm->second,where,limit)) return false;
		}
		if (!copy(buffer,(mostMatchedGeneric!=wNULL) ? mostMatchedGeneric->first : u"",where,limit)) return false;
		for (int I=0; I<4; I++)
			if (!copy(buffer,genericAge[I],where,limit)) return false;
		if (!copy(buffer,objectGenericAge,where,limit)) return false; 
		if (!copy(buffer,mostMatchedAge,where,limit)) return false; 
		if (!writeFlags(buffer, where, limit)) return false;
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
	bool equals(const cObject& o, const vector <cWordMatch> &m)
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
	bool nameAnyNonGenderedMatch(vector <cWordMatch> &m,cObject &o)
	{
		if (name.first!=wNULL && name.last!=wNULL && o.name.first!=wNULL && o.name.last!=wNULL)
			return nameNonGenderedMatch(m,o);
		if (name.any!=wNULL && o.name.any!=wNULL)
			return name.any==o.name.any;
		return false;
	}
	// match between gendered and non-gendered objects
	bool nameNonGenderedMatch(vector <cWordMatch> &m,cObject &o)
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
		if (name.isCompletelyNull() || o.name.isCompletelyNull())
			return false;
		if (name.first!=o.name.first || name.last!=o.name.last || name.any!=o.name.any || 
			  name.middle!=o.name.middle || name.middle2!=o.name.middle2 || name.suffix!=o.name.suffix)
			return false;
		bool unambiguousGenderFound;
		if (!matchGender(o,unambiguousGenderFound) || plural!=o.plural)
			return false;
		return true;
	}
	bool hasAttribute(int where,vector <cWordMatch> &m);
	bool hasAgeModifier(vector <cWordMatch> &m, const lpchar_t *modifiers[]);
	int setGenericAge(vector <cWordMatch> &m);
	bool updateGenericGender(int where,tIWMM w,int fromAge, const lpchar_t * fromWhere,sTrace &t);
	void updateGenericGenders(map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare> &genericNounMap,int *replacedGenericAge);

	static int whichOrderWord(tIWMM word)
	{
		auto f = find(wordOrderWords.begin(),wordOrderWords.end(),word->first);
		if (f == wordOrderWords.end())
			return -1;
		return (int)(f-wordOrderWords.begin());
	}
	int wordOrderSensitive(int at,vector <cWordMatch> &m)
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
		subType = -1;
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
			if (name.hon!=wNULL && name.first==wNULL && name.hon->first==u"st")
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
				name.hon==wNULL && !isNotAPlace && (name.any==wNULL || name.any->second.query(demonymForm)<0))// && !(m[I].flags&cWordMatch::flagAdjectivalObject))
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
	int getOwnerWhere() { return ownerWhere; }
	void setOwnerWhere(int ow) { ownerWhere = ow; }
private:
	int subType;
	int ownerWhere;
	int firstSpeakerGroup;
};

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
extern unsigned int twoObjectTestTagSet;

extern unsigned int PREP_TAG,OBJECT_TAG,SUBOBJECT_TAG,REOBJECT_TAG,IOBJECT_TAG,SUBJECT_TAG,PREP_OBJECT_TAG,VERB_TAG,IVERB_TAG,PLURAL_TAG,MPLURAL_TAG,GNOUN_TAG,FLOAT_TIME_TAG;
extern unsigned int MNOUN_TAG,PNOUN_TAG,VNOUN_TAG,HAIL_TAG,NAME_TAG,REL_TAG,SENTENCE_IN_REL_TAG,NOUN_TAG;

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
int mapNumeralCardinal(const lpwstring &word);
int mapNumeralOrdinal(const lpwstring &word);
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
	void clear()
	{
		parentTimeline = -1;
		begin = -1;
		end = -1;
		speakerGroup = -1;
		location = -1;
		linkage = -1;
	}
	cTimelineSegment()
	{
		clear();
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
		clear();
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

class cSource
{
public:
	cSource(const lpchar_t * databaseServer,int _sourceType,bool generateFormStatistics,bool skipWordInitialization,bool printProgress);
	int beginClock;
	int pass;
	bool RDFFileCaching; // sets whether rdfTypes are read from disk.  They may still be cached in memory! (cOntology::cacheRdfTypes determines that).  This is different than cQuestionAnswering::fileCaching.
	// These ordinals are persisted: they are written to the `sources.sourceType`
	// column and are baked into existing rows and binary source caches.  Do not
	// reorder or delete a member -- RETIRED_SOURCE_TYPE_3 is a retired NewsBank
	// slot, kept purely so every member after it keeps its stored value.
	enum sourceTypeEnum {
		NO_SOURCE_TYPE, TEST_SOURCE_TYPE, GUTENBERG_SOURCE_TYPE, RETIRED_SOURCE_TYPE_3, BNC_SOURCE_TYPE, SCRIPT_SOURCE_TYPE,
		WEB_SEARCH_SOURCE_TYPE, WIKIPEDIA_SOURCE_TYPE, INTERACTIVE_SOURCE_TYPE, PATTERN_TRANSFORM_TYPE, REQUEST_TYPE
	};
	inline static bool updateWordUsageCostsDynamically=false;  // this is turned of by default.  Do NOT turn this back on unless a great deal of testing is done, as this will
																// upset carefully defined weights of word forms usages, and it will also accumulate usages into costs which will make results nonreproducible
	                              // from actual to test because the exact parsing history from the beginning of Source initialization must be followed
	                              // so testing of one sentence in the middle of a book will not necessarily yield the same results as the sentence buried inside of the book
	                              // because previous words have accumulated usages of the words in the sentence which have in turn been rolled over into the costs.
	                              // ALSO processing of multiple books with a single source object will make this much more likely, unless the following procedure is also 
	                              // enabled (this procedure has not been tested!)
																// PLEASE NOTE this does not turn off accumulation of proper noun and lower case statistics as these are used for proper noun processing and
																// not for parsing costs, and will mess up proper noun identification if these statistics are also reset.  These statistics are reset individually
																// in resetCapitalizationAndProperNounUsageStatistics.  All statistics including these proper noun/lower case statistics are reset in resetUsagePatternsAndCosts.
	cIntArray reverseMatchElements;
	vector <cWordMatch> m;
	unordered_map <unsigned int, lpwstring> metaCommandsEmbeddedInSource;
	cPatternElementMatchArray pema;
	int readSourceBuffer(lpwstring title, lpwstring etext, lpwstring path, lpwstring encoding, lpwstring &start, int &repeatStart);
	int parseBuffer(lpwstring &path,unsigned int &unknownCount);
	void parsePattern(unordered_map <lpwstring, lpwstring> &parseVariables, const lpwstring lastMetaCommandEmbeddedInSource, lpwstring& sWord, int& nounOwner);
	void processDash(lpwstring& sWord, bool& firstLetterCapitalized, const int result);
	bool processEndSentence(const lpwstring path, bool endSentence, size_t& lastSentenceEnd, tIWMM iWord, const lpwstring sWord, int& lastProgressPercent, int& runOnSentences, bool& multipleEnds, bool & alreadyAtEnd);
	void webScrapeFields(size_t& lastSentenceEnd);
	void adjustFormsInflections(tIWMM iWord, lpwstring sWord, uint64_t& flags, int nounOwner, int lastSentenceEnd, bool allCaps,
		bool firstLetterCapitalized, bool flagAlphaBeforeHint, bool flagAlphaAfterHint, bool flagNewLineBeforeHint, bool & previousIsProperNoun);
	bool addSpecialWord(int result, const lpwstring sWord, tIWMM& iWord);
	int tokenize(lpwstring title, lpwstring etext, lpwstring path, lpwstring encoding, lpwstring &start, int &repeatStart, unsigned int &unknownCount);
	bool write(IOHANDLE file);
	int checkPEMAPositions(cWordMatch& mi, int& generalizedIndex);
	int sanityCheckSourcePosition(cWordMatch& mi, int & generalizedIndex);
	int sanityCheck(int &wordIndex);
	bool read(char *buffer,int &where,unsigned int total, bool &parsedOnly, bool printProgress, lpwstring specialExtension);
	bool flush(int fd,void *buffer,int &where);
	bool FlushFile(int fd, void *buffer, int &where); // batch B5: POSIX fd, was a Win32 HANDLE
	bool writePatternUsage(lpwstring path, bool zeroOutPatternUsage);
	bool write(lpwstring file,bool S2, bool saveOld, lpwstring specialExtension);
	bool findStart(lpwstring &buffer,lpwstring &start,int &repeatStart,lpwstring &title);
	bool retrieveText(lpwstring &path,lpwstring etext,lpwstring &start,int &repeatStart,lpwstring author,lpwstring title);
	bool readSource(lpwstring &path,bool checkOnly, bool &parsedOnly, bool printProgress,lpwstring specialExtension);
	cSource *parentSource;
	int answerContainedInSource;
	vector <cMatchElement> whatMatched;
	struct wordMapCompare
	{
		bool operator()(const tIWMM &lhs, const tIWMM &rhs) const
		{
			return lhs->first<rhs->first;
		}
	};
	map < tIWMM,set<int>,wordMapCompare> relatedObjectsMap;
	unordered_map < int,lpwstring> positionToTransformationPatternVariableMap;

	int numSearchedInMemory,processOrder;
	bool isFormsProcessed;
	// For each tagset type defined
	//   there will be a map from pema to all the tagsets collected from that pema position downward through the tree.
	vector <unordered_map <int, vector < vector <cTagLocation> > > > pemaMapToTagSetsByPemaByTagSet;

	// new pattern detection
	void accumulateNewPatterns(void);

	void clearSource(void);

	int evaluateBNCPreferenceForPosition(int position,int patternPreference,int flag,bool remove);
	int evaluateBNCPreference(vector <cTagLocation> &tagSet, const lpchar_t * tag,int patternPreference,bool remove);
	int evaluateBNCPreferences(int position,int PEMAPosition,vector <cTagLocation> &tagSet);
	int BNCPatternViolation(int position,int PEMAPosition,vector < vector <cTagLocation> > &tagSets);

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
		//cSpeakerGroup(const cSpeakerGroup& obj);

		bool copy(void *buffer,int &where,int limit);
		void clear(void)
		{
			sgBegin=sgEnd=section=previousSubsetSpeakerGroup=saveNonNameObject=conversationalQuotes=-1;
			speakersAreNeverGroupedTogether=false; 
			tlTransition = false;
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
		void clearTemp(int end)
		{
			speakers.clear();
			groupedSpeakers.clear();
			singularSpeakers.clear();
			povSpeakers.clear();
			metaNameOthers.clear();
			sgBegin = end;
			sgEnd = end;
			groups.clear();
			conversationalQuotes = 0;
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
		int sanityCheck(int maxSourcePosition,int maxSection,int maxObjectIndex, int maxSpeakerGroupsIndex)
		{
			if (sgBegin < 0 || sgBegin >= maxSourcePosition) return 100;
			if ((sgEnd < 0 && sgEnd!=-2 && sgEnd!=-3) || sgEnd > maxSourcePosition) 
				return 101; // in embedded speaker groups, sgEnd may also be set to -2 or -3 or = m.size()
			if (section < -1 || section >= maxSection) return 102;
			if (previousSubsetSpeakerGroup < CURRENT_SUBSET_SG || previousSubsetSpeakerGroup >= maxSpeakerGroupsIndex) return 103;  // CURRENT_SUBSET_SG = -2
			if (saveNonNameObject < -1 || saveNonNameObject >= maxObjectIndex) return 104;
			for (int si : speakers)
				if (si < 0 || si >= maxObjectIndex) 
					return 105;
			for (int si : fromNextSpeakerGroup)
				if (si < 0 || si >= maxObjectIndex) return 106;
			for (unsigned int si = 0; si < replacedSpeakers.size(); si++)
				if (replacedSpeakers[si].object < 0 || replacedSpeakers[si].object >= maxObjectIndex) return 107;
			for (int si : singularSpeakers)
				if (si < 0 || si >= maxObjectIndex) return 108;
			for (int si : groupedSpeakers)
				if (si < 0 || si >= maxObjectIndex) return 109;
			for (int si : povSpeakers)
				if (si < 0 || si >= maxObjectIndex) return 110;
			for (int si : dnSpeakers)
				if (si < 0 || si >= maxObjectIndex) return 111;
			for (int si : metaNameOthers)
				if (si < 0 || si >= maxObjectIndex) return 112;
			for (int si : observers)
				if (si < 0 || si >= maxObjectIndex) return 113;
			for (auto &esg : embeddedSpeakerGroups)
			{
				int sanityCheckReturnCode = 0;
				if (sanityCheckReturnCode=esg.sanityCheck(maxSourcePosition, maxSection, maxObjectIndex, maxSpeakerGroupsIndex))
					return sanityCheckReturnCode+20;
			}
			for (auto &g : groups)
			{
				if (g.where < 0 || g.where >= maxSourcePosition) return 115;
				for (int oi : g.objects)
					if (oi < 0 || oi >= maxObjectIndex)	return 116;
			}
			return 0;
		}

	};
	const lpchar_t *toText(cSpeakerGroup &sg,lpwstring &tmpstr)
	{
		lpchar_t temp[128];
		lp_snprintf(temp,128,u"%d-%d [section %03d] %d: ",sg.sgBegin,sg.sgEnd,sg.section,sg.conversationalQuotes);
		tmpstr=temp;
		lpwstring temp2,temp3;
		tmpstr+=objectString(sg.speakers,temp2)+u" ";
		if (sg.replacedSpeakers.size())
			tmpstr+=u"replacedSpeakers ("+objectString(sg.replacedSpeakers,temp2,true)+u") ";
		if (sg.singularSpeakers.size())
			tmpstr+=u"singularSpeakers ("+ objectString(sg.singularSpeakers,temp2)+u") ";
		if (sg.groupedSpeakers.size())
			tmpstr+=u"groupedSpeakers ("+objectString(sg.groupedSpeakers,temp2)+u") ";
		if (sg.povSpeakers.size())
			tmpstr+=u"povSpeakers ("+objectString(sg.povSpeakers,temp2)+u") ";
		if (sg.dnSpeakers.size())
			tmpstr+=u"dnSpeakers ("+objectString(sg.dnSpeakers,temp2)+u") ";
		if (sg.metaNameOthers.size())
			tmpstr+=u"metaNameOthers ("+objectString(sg.metaNameOthers,temp2)+u") ";
		if (sg.observers.size())
			tmpstr+=u"observers ("+objectString(sg.observers,temp2)+u") ";
		for (unsigned int I=0; I<sg.embeddedSpeakerGroups.size(); I++)
			tmpstr+=u"ESG#"+itos(I,temp2)+u"("+toText(sg.embeddedSpeakerGroups[I],temp3)+u") ";
		for (unsigned int I=0; I<sg.groups.size(); I++)
			tmpstr+=u"group#"+itos(I,temp2)+u"("+objectString(sg.groups[I],temp3)+u") ";
		tmpstr+=(sg.previousSubsetSpeakerGroup<0) ? u"[no Previous Subset SG]": (itos(sg.previousSubsetSpeakerGroup,temp2)+u" ");
		tmpstr+=(sg.saveNonNameObject<0) ? u"[no non-name object]":(itos(sg.saveNonNameObject,temp2)+u" ");
		tmpstr+=(sg.speakersAreNeverGroupedTogether) ? u"[speakers never grouped]":u"[speakers grouped]";
		tmpstr+=(sg.tlTransition) ? u"[transition]":u"[no transition]";
		return tmpstr.c_str();
	}
	lpwstring sourcePath;
	int sourceType;
	bool sourceInPast;
	int sourceConfidence; // dbPedia/wikipedia sources have higher confidence
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
	class cCostPatternElementByTagSet
	{
	public:
		cCostPatternElementByTagSet(int P, int PP, int cPP, int ts, int e)
		{
			sourcePosition = P;
			PEMAPosition = PP;
			childPEMAPosition = cPP;
			tagSet = ts;
			cPatternElement = e;
			cost = 10000000;
			traceSource = -1;
		}
		void initialize(int P, int tso, int ts, int tmpVOCost)
		{
			sourcePosition = P;
			PEMAPosition = -1;
			childPEMAPosition = -1;
			tagSet = tso;
			cPatternElement = -1;
			cost = tmpVOCost;
			traceSource = ts;
		}
		cCostPatternElementByTagSet()
		{
			sourcePosition = 0;
			PEMAPosition = 0;
			childPEMAPosition = 0;
			tagSet = 0;
			cPatternElement = 0;
			cost = 0;
			traceSource = 0;
		}
		bool operator == (const cCostPatternElementByTagSet& o)
		{
			return sourcePosition ==o.sourcePosition &&
				PEMAPosition==o.PEMAPosition &&
				childPEMAPosition==o.childPEMAPosition &&
				tagSet==o.tagSet &&
				cPatternElement ==o.cPatternElement &&
				cost==o.cost;
		}
		bool operator != (const cCostPatternElementByTagSet& o)
		{
			return sourcePosition !=o.sourcePosition ||
				PEMAPosition!=o.PEMAPosition ||
				childPEMAPosition!=o.childPEMAPosition ||
				tagSet!=o.tagSet ||
				cPatternElement !=o.cPatternElement ||
				cost!=o.cost;
		}
		int getSourcePosition() { return sourcePosition; }
		int getPEMAPosition() { return PEMAPosition; }
		int getChildPEMAPosition() { return childPEMAPosition; }
		int getTagSet() { return tagSet; }
		void setTagSet(int ts) { tagSet=ts; }
		int getElement() { return cPatternElement; }
		int getCost() { return cost; }
		void setCost(int c) { cost = c; }
		int getTraceSource() { return traceSource; }
		void setTraceSource(int ts) { traceSource = ts; }
	private:
		int sourcePosition;
		int PEMAPosition;
		int childPEMAPosition; // may be -1, in which case child is not determined or applies to all children
		int tagSet;
		int cPatternElement;
		int cost;
		int traceSource;
	};
	void identifyConversations();

	vector <cCostPatternElementByTagSet> secondaryPEMAPositions;
	int beginTime,timerForExit,desiredTagSetNum; // beginTime,timerForExit monitor time for collectTags
	bool exitTags,blocking,focused;
	sTrace debugTrace;
	// end collectTags global section

	int gTraceSource;
	bool lowestContainingPatternElement(int nextPatternElementPEMAPosition,int element,vector <int> &lowestCostPEMAPositions);
	bool tagSetAllIn(vector <cCostPatternElementByTagSet> &PEMAPositions,int I);
	void setChain(vector <cPatternElementMatchArray::tPatternElementMatch *> chainPEMAPositions,vector <cCostPatternElementByTagSet> &PEMAPositions,vector <cPatternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int &traceSource,int &minOverallChainCost);
	void findAllChains(vector <cCostPatternElementByTagSet> &PEMAPositions,int PEMAPosition,vector <cPatternElementMatchArray::tPatternElementMatch *> &chain,vector <cPatternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int &traceSource,int &minOverallChainCost);
	void setChain2(vector <cPatternElementMatchArray::tPatternElementMatch *> &chainPEMAPositions,vector <cPatternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int deltaCost);
	void findAllChains2(int PEMAPosition,int position,vector <cPatternElementMatchArray::tPatternElementMatch *> &chain,vector <cPatternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int changedPosition,int rootPattern,int len,bool includesPatternMatch,int deltaCost);
	bool notFirstNounInMultiNounConstruction(int parentPosition,int parentPEMAOffset, int childPosition,int childEnd);
	int cascadeUpToAllParents(bool recalculatePMCost,int basePosition,cPatternMatchArray::tPatternMatch *childPM,int traceSource,vector <cPatternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet, bool stopCascadeWhenNDAlreadySet, const lpchar_t * fromWhere);
	void recalculateOCosts(bool &recalculatePMCost,vector<cPatternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int start,int traceSource);
	int setSecondaryCosts(vector <cCostPatternElementByTagSet> &secondaryPEMAPositions,cPatternMatchArray::tPatternMatch *pm,int basePosition, bool stopCascadeWhenNDAlreadySet, const lpchar_t * fromWhere);
	int getEndRelativeSourcePosition(int PEMAPosition);
	void setPreviousElementsCostsAtIndex(vector <cCostPatternElementByTagSet> &PEMAPositions, int pp, int cost, int traceSource, int patternElementEndPosition, int pattern, int cPatternElement);
	void lowerPreviousElementCosts(vector <cCostPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, const lpchar_t * fromWhere);
	void lowerPreviousElementCostsLowerRegardlessOfPosition(vector <cCostPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, lpchar_t *fromWhere);
	void lowerPreviousElementCostsOld(vector <cCostPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, lpchar_t *fromWhere);
	bool assessEVALCost(cTagLocation &tl,int pattern,cPatternMatchArray::tPatternMatch *pm,int position, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions,lpwstring purpose);
	void accumulateTertiaryPEMAPositions(int tagSetOffset,int traceSource,vector <cTagLocation>  &tagSet, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions, int tmpVOCost);
	void applyTertiaryPEMAPositions(unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions);
	void assessAgreementCost(cPatternMatchArray::tPatternMatch* parentpm, cPatternMatchArray::tPatternMatch* pm, const int parentPosition, const int position, vector < vector <cTagLocation> >& tagSets, lpwstring purpose);
	void assessVerbObjectCost(cPatternMatchArray::tPatternMatch* parentpm, cPatternMatchArray::tPatternMatch* pm, const int parentPosition, const int position, vector < vector <cTagLocation> >& tagSets, lpwstring purpose);
	int assessCost(cPatternMatchArray::tPatternMatch *parentpm, cPatternMatchArray::tPatternMatch *pm, int parentPosition, int position, vector < vector <cTagLocation> > &tagSets, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions,bool alternateNounDeterminerShortTry,lpwstring purpose);
	void evaluateExplicitNounDeterminerAgreement(int position, cPatternMatchArray::tPatternMatch *pm, vector < vector <cTagLocation> > &tagSets, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions);
	void evaluateExplicitSubjectVerbAgreement(int position, cPatternMatchArray::tPatternMatch *pm, vector < vector <cTagLocation> > &tagSets, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions);
	void eliminateLoserPatternsPhase1(unsigned int begin, unsigned int end, vector <int> &minSeparatorCost, vector < vector <unsigned int> > &winners, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions);
	void updateCost(unsigned int begin, unsigned int position, vector <int> &minSeparatorCost, int PMAOffset, vector <unsigned int> &preliminaryWinners,int phase);
	void eliminateLoserPatternsPhase2(unsigned int begin, unsigned int end, vector <int> &minSeparatorCost, vector < vector <unsigned int> > &winners);
	bool eliminateLoserPatternsPhase3OR5(unsigned int begin, unsigned int end, vector <int> &minSeparatorCost, vector < vector <unsigned int> > &winners,int &matchedPositions, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions,int phase);
	void eliminateLoserPatternsPhase4(unsigned int begin, unsigned int end, vector <int> &minSeparatorCost, vector < vector <unsigned int> > &winners);
	int eliminateLoserPatterns(unsigned int begin,unsigned int end);
	enum prepSetEnum { PREP_PREP_SET,PREP_OBJECT_SET,PREP_VERB_SET };
	void setRelPrep(int where,int relPrep,int fromWhere,int setType, int whereVerb);
	lpwstring lastNounNotFound,lastVerbNotFound;

	void printSectionStatistics(void);
	void printResolutionCheck(vector <int> &badSpeakers);
	bool isSpeaker(int where,int esg,int tempCSG);
	bool appendPrepositionalPhrase(int where, vector <lpwstring> &prepPhraseStrings, int relPrep, bool nonMixed, bool lowerCase, const lpchar_t *separator, int atNumPP);
	int appendPrepositionalPhrases(int where, lpwstring &wsoStr, vector <lpwstring> &prepPhraseStrings, int &numWords, bool nonMixed, const lpchar_t *separator, int atNumPP);
	int getObjectStrings(int where, int object, vector <lpwstring> &wsoStrs, bool &alreadyDidPlainCopy);
	int appendVerb(vector <lpwstring> &objects, int where);
	int appendWord(vector <lpwstring> &objects, int where);
	int appendObject(int64_t questionType, int whereQuestionType, vector <lpwstring> &objects, int where);
	tIWMM getTense(tIWMM verb, tIWMM subject, int tenseDesired);
	lpwstring getTense(int where, lpwstring candidate, int preferredVerb);
	void analyzeWordSenses(void);
	// speaker resolution
	bool eraseWinnerFromRecalculatingAloneness(int I,cPatternMatchArray::tPatternMatch *pma);
	bool removeWinnerFlag(int where, cPatternMatchArray::tPatternMatch *pma,int recursionSpaces, vector <cPatternMatchArray::tPatternMatch *> &PMAToRemoveWinner, vector <int> &parentPEMAToRemoveWinner);
	bool isAnySeparator(int where);
	bool addCostFromRecalculatingAloneness(int where,cPatternMatchArray::tPatternMatch *pma);
	void identifyObjects(void);
	int scanForSpeakers(int begin,int end,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,uint64_t roleFlag);
	bool substituteGenderedBodyObject(int where,int &speakerObject);
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
	int determineIfSpeakerMoved(int begin, int end, bool endOfSection);
	void insertPreviousUnquotedSpeakers(int begin, int end);
	void mergeTempSpeakerGroupWithLastSG(int begin, int end, int& speakersNotMergable, int& onlyHailSpeakers, vector <cSpeakerGroup>::iterator lastSG, int resolvableByFutureSpeaker, bool speakerGroupCrossesSectionBoundary);
	bool mergeTempSpeakerGroupWithLastSG2(int begin, int end, vector <cSpeakerGroup>::iterator lastSG, int resolvableByFutureSpeaker, bool speakerGroupCrossesSectionBoundary);
	void extendLastSGToEndOfSection(int begin, int end, vector <cSpeakerGroup>::iterator lastSG, int resolvableByFutureSpeaker, int& lastSpeakerGroupOfPreviousSection, bool speakerGroupCrossesSectionBoundary, bool endOfSection);
	void pushTemporarySpeakerGroupAndErase(int begin, int end, bool endOfSection, int& lastSpeakerGroupOfPreviousSection);
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
	void associateNyms(int where);
	void associatePossessions(int where);
	void moveNyms(int where,int toObject,int fromObject, const lpchar_t * fromWhere);
	bool implicitObject(int where);
	void dropPOVIntoSpeakerGroup(const int sgi, int &povi, const int maleSpeakers, const int femaleSpeakers);
	void dropDefinitelyIdentifiedSpeakersIntoSpeakerGroup(const int sgi, int &dni, const int maleSpeakers, const int femaleSpeakers);
	void determineObserverStatus(const int sgi, int& nonObserver, const bool conversationRestricted, bool &isAnyNonObserverSpeakerNew, bool &isAnyObserverSpeakerNew);
	bool determineObserverContinuing(const int sgi, const int nonObserver, const bool conversationRestricted);
	void addPreviousObserver(const int sgi, const bool previousSpeakerGroupInSameSection);
	void addPreviousPOV(const int sgi, const bool previousSpeakerGroupInSameSection);
	void removeObserverAssociatedWithSpeakingGroup(const int sgi);
	void setConversationalQuotes(const int sgi, int &currentQuote);
	void distributeMetaNameOthers();
	void distributePOV(void);
	bool invalidGroupObjectClass(int oc);
	void accumulateGroups(int where,vector <int> &groupedObjects,int &lastWhereMPluralGroupedObject);
	bool sameSpeaker(int sWhere1,int sWhere2);
	vector <int> subNarratives;
	void embeddedStory(int where,int &numPastSinceLastQuote,int &numNonPastSinceLastQuote,int &numSecondInQuote,int &numFirstInQuote,
										 int &lastEmbeddedStory,int &lastEmbeddedImposedSpeakerPosition,int lastSpeakerPosition);
	void adjustHailRoleDuringScan(int where);
	void adjustToHailRole(int where);
	void setPrepVerbRelations(vector <int> &futureBoundPrepositions);
	void syntacticRelationsQuotes(vector <cWordMatch>::iterator im, const int I, bool& inPrimaryQuote, bool& inSecondaryQuote, bool& inQuotedString, int& lastVerb, int& firstFreePrep);
	void syntacticRelationsEvaluateRelations(vector <cWordMatch>::iterator im, const int I);
	void syntacticRelationsEOS(int I, int& lastBeginS1, int& lastRelativePhrase, int& lastQ2, int& lastVerb, int& firstFreePrep, int& whereLastVerb);
	void syntacticRelations();
	bool replaceSubsequentMatches(set <int> &so,int sgEnd);
	bool replaceAliasesAndReplacements(set <int> &objects);
	bool eraseAliasesAndReplacementsInSpeakerGroup(vector <cSpeakerGroup>::iterator sg,bool eraseSubsequentMatches);
	void eraseAliasesAndReplacementsInEmbeddedSpeakerGroups	(void);
	void eraseAliasesAndReplacementsInSpeakerGroups(void);
	bool blockSpeakerGroupCreation(int endSection,bool quotesSeenSinceLastSentence,int nsAfter);
	bool rejectTimeWord(int where,int begin);
	bool resolveTimeRange(int where,int pmaOffset,vector <cSyntacticRelationGroup>::iterator csr);
	bool stopSearch(int I);
	void distributeTimeRelations(vector <cSyntacticRelationGroup>::iterator csr,vector <cSyntacticRelationGroup>::iterator previousRelation,int conjunctionPassed);
	void appendTime(vector <cSyntacticRelationGroup>::iterator csr);
	void detectTimeTransition(int where,vector <int> &lastSubjects);
	void copyTimeInfoNum(vector <cTimeInfo>::iterator previousTime,cTimeInfo &t,int num);
	void markTime(int where,int begin,int len);
	bool evaluateTimePattern(int beginObjectPosition,int &maxLen,cTimeInfo &t,cTimeInfo &rt,bool &rtSet);
	bool identifyTimePattern(const int where, vector <cSyntacticRelationGroup>::iterator csr, const int beginObjectPosition, int& maxLen);
	bool identifyYear(const int where, vector <cSyntacticRelationGroup>::iterator csr, const int beginObjectPosition);
	bool identifyTimeType(const int where, cTimeInfo& t);
	bool processModifierTime(const int where, cTimeInfo& t);
	void interpretNumberAsDateTime(const int where, vector <cSyntacticRelationGroup>::iterator csr, cTimeInfo& t, const int beginObjectPosition, const int inMultiObject);
	void setTimeModifier(const int where, cTimeInfo& t, const int beginObjectPosition);
	bool cancelTimeDateIdentification(const int where, cTimeInfo& t, const int inMultiObject);
	bool identifyDateTime(int where,vector <cSyntacticRelationGroup>::iterator csr,int &maxLen,int inMultiObject);
	bool detectTimeTransition(int where,vector <cSyntacticRelationGroup>::iterator csr,cTimeInfo &t);
	bool evaluateHOUR(int where,cTimeInfo &t);
	void evaluateDateTimeTimeSpec(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet);
	void evaluateDateTimeDateSpec(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet);
	void evaluateDateTimeModifier(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet);
	void evaluateDateTimeCapacity(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet);
	void evaluateDateTimeType(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet);
	bool evaluateDateTimeHour(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool &rtSet);
	void evaluateDateTimeDayMonth(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet);
	bool evaluateDateTimeDayMinute(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet);
	void evaluateDateTimeMonth(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet);
	void evaluateDateTimeSeason(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet);
	void evaluateDateTimeHoliday(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet);
	void evaluateDateTimeDayWeek(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet);
	void evaluateDateTimeMinute(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& rtSet);
	bool evaluateDateTime(vector <cTagLocation> &tagSet,cTimeInfo &t,cTimeInfo &rt,bool &rtSet);
	bool ageTransition(int where,bool timeTransition,bool &transitionSinceEOS,int duplicateFromWhere,int exceptWhere,vector <int> &lastSubjects, const lpchar_t * fromWhere);
	int primaryLocationLastMovingPosition,primaryLocationLastPosition;
	bool like(lpwstring str1,lpwstring str2);

	// where
	vector <cSyntacticRelationGroup>::iterator findSyntacticRelationGroup(int where);
	const lpchar_t *src(int where,lpwstring description,lpwstring &tmpstr);
	bool followerPOVToObserverConversion(vector <cSyntacticRelationGroup>::iterator sr,int sg);
	bool setTimeFlowTense(int where,int whereControllingEntity,int whereSubject,int whereVerb,int whereObject,
		int wherePrepObject,
		int prepObjectSubType,int objectSubType,bool establishingLocation,bool futureLocation,bool genderedLocationRelation,cTimeFlowTense &tft);
	void srSetTimeFlowTense(int spri);
	bool isRelativeLocation(lpwstring word);
	void setRelationTypeAndTimeFlow(int where, int whereSubject, vector <cSyntacticRelationGroup>::iterator location, int relationType);
	bool changeEnterToMoveIfPhysicallyPresent(int where, int relationType, int whereVerb, int whereSubject);
	bool determineSubjectGendered(int whereSubject, int whereVerb);
	bool determineControllerGendered(int whereControllingEntity);
	bool determineObjectIsAcceptable(int whereObject, int relationType, int o, int objectSubType);
	int setPrepSubType(const int po, const int wherePrepObject, int& relObject, int& relPrep, bool &prepTypeCancelled);
	bool defineGenderedLocationRelation(const int o, const int po, const int whereControllingEntity, int &whereSubject, const int whereVerb, const int whereObject, const int relationType, const int objectSubType,
		const bool prepObjectIsAcceptable, const bool objectIsAcceptable, const bool prepTypeCancelled, const bool convertToMove);
	void lookForwardToUpdateTimeInfo(int where, vector <cSyntacticRelationGroup>::iterator location);
	void insertOrUpdateNewSpeakerGroup(int where, int whereSubject, cSyntacticRelationGroup& sr, bool convertToMove, const lpchar_t* whereType);
	void determineAcceptabilityAndSubTypes(const int whereSubject, const int whereVerb, const int relationType,
		int& o, int& objectSubType, const int whereObject, bool& objectIsAcceptable,
		int& po, int& prepObjectSubType, int& wherePrepObject, bool& prepObjectIsAcceptable,
		int& wherePrep);
	void newSR(int where,int o,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int at,int whereMovingRelativeTo,int relationType,const lpchar_t *whereType,bool physicalRelation);
	void logSyntacticRelationGroup(cSyntacticRelationGroup &sr, const lpchar_t *whereType);
	bool moveIdentifiedSubject(int where,bool inPrimaryQuote,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int at,int whereMovingRelativeTo,int hasSyntacticRelationGroup,const lpchar_t *whereType,bool physicalRelation);
	int findAnyLocationPrepObject(int whereVerb,int &wherePrep,bool &location,bool &timeUnit);
	bool rejectPrepPhrase(int wherePrep);
	bool adverbialPlace(int where);
	bool adjectivalExit(int where);
	bool whereSubType(int where);
	bool exitConversion(int whereObject,int whereSubject,int wherePrepObject);
	int getAfterVerb(const int where, const int whereVerb, const int whereSubject);
	int getSubType(int whereVerb, int& whereObject);
	void adjustForStart(bool start, int &whereObject, int &whereVerb, int& wpd, int &st);
	bool determineIfPhysicalSubject(int where, int &whereSubject);
	bool detectPlaceTransition(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, cVerbNet &verbClass, lpwstring id, bool pr, bool inPrimaryQuote, int st, bool proLocation);
	bool identifyObjectAsPlace(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, cVerbNet& verbClass, lpwstring id, bool pr, bool inPrimaryQuote, int st, int wpd, bool acceptableVerbForm, bool prepLocation, bool prepMustBeLocation);
	int detectPlacePreposition(int where, const int whereControllingEntity, const int whereSubject, int& whereObject, const int whereVerb, cVerbNet& verbClass, lpwstring id, const bool pr, const bool inPrimaryQuote, const int wherePrep, 
		int &whereLastPrepObject, const bool acceptableVerbForm, const bool prepMustBeLocation, const bool objectMustBeLocation);
		int detectPlaceTransitionForPrep(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, cVerbNet& verbClass, lpwstring id, bool pr, int wherePrep, bool proLocation, bool physicalObject, bool prepMustBeLocation, bool objectMustBeLocation, bool acceptableVerbForm);
	bool detectAdverbialWhere(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, cVerbNet& verbClass, lpwstring id, bool pr, bool physicalObject, bool acceptableVerbForm);
	bool detectExit(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, int afterVerb, lpwstring id, bool pr, bool inPrimaryQuote);
	int detectObjectTransfer(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, cVerbNet& verbClass, lpwstring id, bool pr);
	bool detectMoveCommand(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, cVerbNet& verbClass, lpwstring id, bool pr);
	bool detectWhere(int where, int whereControllingEntity, int whereSubject, int whereObject, int whereVerb, lpwstring id, bool pr);
	bool detectPhysicalObject(int whereObject);
	bool placeIdentification(int where, bool inPrimaryQuote, int whereControllingEntity, int whereSubject, int whereVerb, int vnClass);
	bool srMoveObject(int where,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int at,int whereMovingRelativeTo,int hasSyntacticRelationGroup, const lpchar_t * whereType,bool physicalRelation);
	void defineObjectAsSpatial(int where);
	void detectTenseAndFirstPersonUsage(int where, int lastBeginS1, int lastRelativePhrase, int &numPastSinceLastQuote, int &numNonPastSinceLastQuote, int &numFirstInQuote, int &numSecondInQuote, bool inPrimaryQuote);
	void evaluateMetaWhereQuery(int where,bool inPrimaryQuote,int &currentMetaWhereQuery);
	void identifyHailObjects(int where, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, bool inPrimaryQuote, bool inSecondaryQuote);
	void processEndOfSentence(int where, int &lastBeginS1, int &lastRelativePhrase, int &lastCommand, int &lastSentenceEnd, int &uqPreviousToLastSentenceEnd, int &uqLastSentenceEnd,
		int &questionSpeakerLastSentence, int &questionSpeaker, bool &currentIsQuestion, bool inPrimaryQuote, bool inSecondaryQuote, bool &endOfSentence, bool &transitionSinceEOS,
		unsigned int &agingStructuresSeen, bool quotesSeenSinceLastSentence, vector <int> &lastSubjects, vector <int> &previousLastSubjects);
	void processEndOfPrimaryQuote(int where, int lastSentenceEndBeforeAndNotIncludingCurrentQuote,
		int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, int &lastSpeakerPosition, int &lastQuotedString, int &quotedObjectCounter,
		bool &inPrimaryQuote, bool &immediatelyAfterEndOfParagraph, bool &firstQuotedSentenceOfSpeakerGroupNotSeen, bool &quotesSeenSinceLastSentence,
		vector <int> &lastSubjects);
	void scanQuoteForAudience(int where);
	void markEndOfEmbeddedSpeakerGroup(int where);
	void removeObjectsHavingPrimaryQuote(unsigned int& quotedObjectCounter);
	void resolveAudience(int audienceObjectPosition, int speakerPosition, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, bool& audienceInSubQuote, bool& audienceFromSpeakerGroup);
	bool getSpeakerAudiencePositions(int where, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, bool noSpeakerAfterward, int& audienceObjectPosition, int& speakerPosition);
	void replaceSpeakerWithPreviousSpeaker(int where, int speakerPosition);
	void processEndOfPrimaryQuoteRS(int where, int lastSentenceEndBeforeAndNotIncludingCurrentQuote,
		int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, int &lastQuotedString,unsigned int &quotedObjectCounter, int &lastDefiniteSpeaker, int &lastClosingPrimaryQuote,
		int &paragraphsSinceLastSubjectWasSet, int wherePreviousLastSubjects,
		bool &inPrimaryQuote, bool &immediatelyAfterEndOfParagraph, bool &quotesSeenSinceLastSentence, bool &previousSpeakersUncertain,
		vector <int> &previousLastSubjects);
	void scanSentenceForSyntacticAndSpaceRelationsAndMetaQueries(int where, int lastBeginS1, int& currentMetaWhereQuery, int& uqPreviousToLastSentenceEnd, int& uqLastSentenceEnd, vector <int>& lastSubjects);
	void processUnquotedEnds(int where, int& currentMetaWhereQuery, int& uqPreviousToLastSentenceEnd, int& uqLastSentenceEnd, vector <int>& lastSubjects);
	void ageEmbeddedSpeakerGroups(int where, bool inPrimaryQuote);
	void processEndOfSentenceRS(int where,
		int& questionSpeakerLastSentence, int& questionSpeaker, bool& currentIsQuestion,
		int& lastClosingPrimaryQuote, int& paragraphsSinceLastSubjectWasSet, int& wherePreviousLastSubjects,		
		bool& inPrimaryQuote, bool& inSecondaryQuote,
		bool& quotesSeenSinceLastSentence, int whereSubject, vector <int>& lastSubjects,
		vector <int>& previousLastSubjects, int lastSectionWord, unsigned int& agingStructuresSeen);
	void srd(int where,lpwstring spd,lpwstring &description);
	lpwstring wsrToText(int where,lpwstring &description);
	lpwstring srToText(int &spr,lpwstring &description);
	unordered_map <lpwstring, set <int> >::iterator getVerbClasses(int where,lpwstring &verb);
	lpwstring getBaseVerb(int where,int fromWhere,lpwstring &verb);
	bool isVerbClass(int where,int verbClass);
	bool isSpecialVerb(int where,bool moveOnly);
	bool isPhysicalActionVerb(int where);
	bool isSelfMoveVerb(int where,bool &exitOnly);
	bool isVerbClass(int where,lpwstring verbClass);
	bool isNounClass(int where,lpwstring group);
	bool locationMatched(int where);
	void detectSpaceLocation(int where,int lastBeginS1);
	bool isSpeakerContinued(int where,int o,int lastWherePP,bool &sgOccurredAfter,bool &audienceOccurredAfter,bool &speakerOccurredAfter);
	bool isSpatialSeparation(int whereVerb);
	bool initiallyQualifySyntacticRelationExit(const int where, vector <cSyntacticRelationGroup>::iterator srg);
	bool cancelExitPOV(const int where, vector <cSyntacticRelationGroup>::iterator srg, bool &allPOVSpeakersInSubject, bool &subjectIsPhysicallyPresent, set <int> &povSpeakers);
	void disambiguateExitingSubject(int where, vector <cSyntacticRelationGroup>::iterator srg, set <int>& povSpeakers);
	void processExit(int where,vector <cSyntacticRelationGroup>::iterator srg,int backInitialPosition,vector <int> &lastSubjects);
	bool detectSubjectVerbForSyntacticRelationGroup(const int where, const bool inPrimaryQuote, int& whereSubject, int& whereVerb);
	void createLocationMoveSyntacticRelationGroup(const int where, const int whereSubject, const int whereVerb, bool &syntacticRelationGroupMovingDetected);
	bool createLocationByPrepSyntacticRelationGroup(const int where, const bool inPrimaryQuote, int& whereControllingEntity, int& whereSubject, int& whereVerb);
	void detectSyntacticRelationGroup(int where,int backInitialPosition,vector <int> &lastSubjects);
	void logSpaceCheck(void);
	int getSpeakersToKeep(vector<cSyntacticRelationGroup>::iterator sr);
	void beginSection(int &lastSpeakerGroupPositionConsidered, int &lastSpeakerGroupOfPreviousSection, int I, vector <int> &previousLastSubjects, vector <int> &lastSubjects);
	void endSection(int &questionSpeakerLastParagraph, int &questionSpeakerLastSentence, int &whereFirstSubjectInParagraph, int &lastSpeakerGroupPositionConsidered, int &lastSpeakerGroupOfPreviousSection, int I,
		bool &endOfSentence, bool &immediatelyAfterEndOfParagraph, bool &quotesSeenSinceLastSentence, bool inSecondaryQuote, bool inPrimaryQuote, bool &quotesSeen, bool &firstQuotedSentenceOfSpeakerGroupNotSeen,
		vector <int> previousLastSubjects);
	void ageSpeakersPerSentence(int where, int& endMetaResponse, unsigned int& agingStructuresSeen, int& lastBeginS1, const bool inPrimaryQuote, const bool inSecondaryQuote);
	void getWhereFirstSubjectInParagraph(int where, const bool inPrimaryQuote, const bool inSecondaryQuote, const bool currentIsQuestion, vector <int>& lastSubjects, int& whereFirstSubjectInParagraph);
	void updateSpeakerObjects(cSpeakerGroup& tempSpeakerGroup);
	void identifySpeakerGroups();
	void substituteGenderedObject(int where,int &object,set <int> &speakers);
	void mergeFocusResolution(int o,int I,vector <int> &lastSubjects,bool &clearBeforeSet);
	void preferSubgroupMatch(int where,int objectClass,int inflectionFlags,bool inQuote,int speakerObject,bool restrictToAlreadyMatched);
	bool matchNewHail(int where,vector <cSpeakerGroup>::iterator sg,int o);
	void accumulateSubjects(int I,int o,bool inPrimaryQuote,bool inSecondaryQuote,int &whereSubject,bool &accumulateMultipleSubjects,vector <int> &lastSubjects);
	bool isAgentObject(int object);
	bool hasAgentObjectOwner(int where,int &ownerWhere);
	unordered_map <lpwstring,int> prepTypesMap;
	void preparePrepMap(void);
	int getMovementPrepType(tIWMM prepWord);
	void accumulateLocation(int where,vector <cTagLocation> &tagSet,int subjectObject,bool locationTense);
	int determineSpeaker(int beginQuote,int endQuote,bool primary,bool noSpeakerAfterward,bool &definitelySpeaker,int &audienceObjectPosition);
	int accumulateLocationLastLocation;
	void accumulateAdjective(const lpwstring &fromWord,unordered_set <lpwstring> &words,vector <tIWMM> &validList,bool isAdjective,lpwstring &aa,bool &containsMale,bool &containsFemale);
	map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > wnSynonymsNounMap,wnSynonymsAdjectiveMap,wnAntonymsAdjectiveMap,wnAntonymsNounMap;
	map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare> wnGenderAdjectiveMap,wnGenderNounMap;
	int readWNMap(map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &m,void *buffer,int &where,int bufferlen);
	bool readWNMaps(lpwstring path);
	bool writeWNMap(map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &m,void *buffer,int &where,int fd,int limit);
	bool writeWNMaps(lpwstring path);
	int readGWNMap(map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare > &m,void *buffer,int &where,int bufferlen);
	void readGWNMaps(lpwstring path);
	bool writeGWNMap(map <tIWMM,int,cSourceWordInfo::cRMap::wordMapCompare > &m,void *buffer,int &where,int fd,int limit);
	void writeGWNMaps(lpwstring path);
	void fillWNMaps(int where,tIWMM word,bool isAdjective);
	void clearWNMaps();
	bool WNMapsInitialized;
	void addWNExtensions(void);
	void addDefaultGenderedAssociatedNouns(int o);
	void accumulateAdjectives(int where);

	// question processing
	unordered_map <int,int> questionSubjectAgreementMap;  // maps the first subject occurring in the next nonquotedParagraph to the question before it
	bool questionAgreement(int where,int whereFirstSubjectInParagraph,int questionSpeakerLastParagraph,vector <cOM> &objectMatches,bool &subjectDefinitelyResolved,bool audience, const lpchar_t * fromWhere);
	void setSecondaryQuestion(vector <cWordMatch>::iterator im);
	void setQuestion(vector <cWordMatch>::iterator im,bool inQuote,int &questionSpeakerLastSentence,int &questionSpeaker,bool &currentIsQuestion);
	void correctBySpeakerInversionIfQuestion(int where,int whereFirstSubjectInParagraph);
	bool matchChildSourcePositionSynonym(tIWMM parentWord, cSource *childSource, int childWhere);
	int determineKindBitField(cSource *source, int where, int &wikiBitField);
	int determineKindBitFieldFromObject(cSource *source, int object, int &wikiBitField);
	bool testQuestionType(int where,int &whereQuestionType,int &whereQuestionTypeFlags,int setType,set <int> &whereQuestionInformationSourceObjects);
	void getQuestionTypeAndQuestionInformationSourceObjects(int whereVerb,int whereReferencingObject,int64_t &questionType,int &whereQuestionType,set <int> &whereQuestionInformationSourceObjects);
	void transformQuestionRelation(cSyntacticRelationGroup& srg);

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
	void atTimeTransitionKeepOnlyPhysicallyPresentSpeaker(int I, bool &transitionSinceEOS, vector <int>& lastSubjects);
	void markLoneNameSpeaker(int I);
	void accumulateSubjects(int I, bool inPrimaryQuote, bool inSecondaryQuote, int& whereSubject, bool accumulateMultipleSubjects, bool currentIsQuestion, vector <int>& lastSubjects);
	bool setOpeningDoubleQuote(int I, bool& inPrimaryQuote, bool& quotesSeen, bool& quotesSeenSinceLastSentence, int& lastQuotedString, int& lastSentenceEndBeforeAndNotIncludingCurrentQuote, int lastSentenceEnd, int& lastSentenceMetaSpeakerQuery);
	void processEndOfSection(int I, bool& endOfSentence, bool& immediatelyAfterEndOfParagraph, bool& accumulateMultipleSubjects, bool inPrimaryQuote, bool& quotesSeen,
		int& lastSectionWord, bool & quotesSeenSinceLastSentence, int& lastLetterBegin, int& whereLetterTo, int& lastDefiniteSpeaker,
		int& questionSpeakerLastParagraph, int& questionSpeakerLastSentence, int& whereFirstSubjectInParagraph, int wherePreviousLastSubjects,
		vector <int>& previousLastSubjects);
	void processBeginSection(int I, int& beginSection, int& lastDefiniteSpeaker, int& whereSubject, int& wherePreviousLastSubjects,
		vector <int>& lastSubjects, vector <int>& previousLastSubjects, bool quotesSeenSinceLastSentence);
	void processMetaResponse(int I, int& endMetaResponse);
	void processBeginningOfSentence(int I, unsigned int& agingStructuresSeen, int& lastBeginS1, int endMetaResponse, bool inPrimaryQuote, bool inSecondaryQuote, bool& accumulateMultipleSubjects);
	void processOneWordSentence(int uqLastSentenceEnd, int uqPreviousToLastSentenceEnd, int whereLastObject, bool anySyntacticRelationGroup);
	void logSpeakerGroups();
	void followObjectReplacements();
	void assureAtLeastOneSpeakerGroup();
	void resolveSpeakers(vector <int>& secondaryQuotesResolutions);
	void matchSelfReferences(vector <int>& secondaryQuotesResolutions, const int where, const int sqr, vector <cWordMatch>::iterator lastOpeningSecondaryQuoteIM, bool& audienceFilled);
	void assignAudienceBasedOnHailOrLastAudienceOrEmbeddedSpeakers(vector <int>& secondaryQuotesResolutions, const int where, const int sqr, vector <cWordMatch>::iterator lastOpeningPrimaryQuoteIM, vector <cWordMatch>::iterator lastOpeningSecondaryQuoteIM);
	void handleQuotes(vector <cWordMatch>::iterator im, const int where, bool& inPrimaryQuote, bool& inSecondaryQuote, vector <cWordMatch>::iterator& lastOpeningPrimaryQuoteIM, vector <cWordMatch>::iterator& lastOpeningSecondaryQuoteIM, int &lastEmbeddedStoryBegin);
	void setMasterSpeakerList();
	void resolveFirstSecondMetaGroupObject(vector <cWordMatch>::iterator im, const int where, const bool inPrimaryQuote, const bool inSecondaryQuote, vector <cWordMatch>::iterator lastOpeningPrimaryQuoteIM, vector <cWordMatch>::iterator lastOpeningSecondaryQuoteIM,
		const int lastEmbeddedStoryBegin);
	void resolveUnquotedFirstSecondPronoun(vector <cWordMatch>::iterator im, const bool inPrimaryQuote, const bool inSecondaryQuote);
	void processSecondaryQuotes(const int where, int &sqr, vector <int>& secondaryQuotesResolutions, vector <cWordMatch>::iterator lastOpeningPrimaryQuoteIM, vector <cWordMatch>::iterator lastOpeningSecondaryQuoteIM,
		const int lastEmbeddedStoryBegin);
	void resolveFirstSecondPersonPronouns(vector <int> &secondaryQuotesResolutions);
	void printTenseStatistic(cTenseStat &tenseStatistics,int sense,int numTotal);
	void printTenseStatistics(const lpchar_t * fromWhere,cTenseStat tenseStatistics[],int numTotal);
	void printTenseStatistics(const lpchar_t * fromWhere, unordered_map <int,cTenseStat> &tenseStatistics,int numTotal);
	bool determineTimelineSegmentLink();
	bool speakerGroupTransition(int where,int newSG,bool forward);
	void initializeTimelineSegments(void);
	void createTimelineSegment(int where);
	bool adjustWord(unsigned int q);
	void eraseLastQuote(int &lastQuote,tIWMM quoteCloseWord,unsigned int &q);
	bool testConversionToDoubleQuotes();
	bool getFormFlags(int where, bool &maybeVerb, bool &maybeNoun, bool &maybeAdjective, bool &preferNoun);
	bool isFirstWordInSentence(unsigned int q, unsigned int begin);
	void checkProperNoun(unsigned int q, unsigned int begin, unsigned int end, bool firstWordInSentence);
	void adjustQuotationsIfOpen(unsigned int q, unsigned int& end, unsigned int& quotationExceptions, int lastPrimaryQuote, int lastSecondaryQuote,
		tIWMM primaryQuoteOpenWord, tIWMM primaryQuoteCloseWord,
		tIWMM secondaryQuoteOpenWord, tIWMM secondaryQuoteCloseWord,
		unsigned int& primaryQuotations, unsigned int& secondaryQuotations);
	void alterNounOwner(unsigned int q, unsigned int& begin, unsigned int& end, unsigned int s, unsigned int secondaryQuotations);
	bool convertNoOne(unsigned int q);
	void manageMissingQuotes(unsigned int& end, unsigned int& primaryQuotations, unsigned int& secondaryQuotations, unsigned int& quotationExceptions, unsigned int s, int lastSecondaryQuote,
		tIWMM primaryQuoteWord, tIWMM secondaryQuoteWord, tIWMM primaryQuoteCloseWord, tIWMM secondaryQuoteCloseWord, bool& endOfParagraph);
	void rationalizePrimarySecondaryQuotes();
	unsigned int doQuotesOwnershipAndContractions(unsigned int& quotations);
	int reportUnmatchedElements(int begin,int end,bool logElements);
	void clearTagSetMaps(void);
	int WRMemoryCheck();
	int readWordIdsNeedingWordRelations(set <int> &wordIds);

	class cTestWordRelation
	{
	public:
		int id;
		int sourceId;
		int lastWhere;
		int fromWordId;
		int toWordId;
		int totalCount;
		int typeId;
		cTestWordRelation(int _id, int _sourceId, int _lastWhere, int _fromWordId, int _toWordId, int _totalCount, int _typeId)
		{
			id = _id;
			sourceId = _sourceId;
			lastWhere = _lastWhere;
			fromWordId = _fromWordId;
			toWordId = _toWordId;
			totalCount = _totalCount;
			typeId = _typeId;
		}
		bool operator==(const cTestWordRelation& tr) {
			return id == tr.id &&
				sourceId == tr.sourceId &&
				lastWhere == tr.lastWhere &&
				fromWordId == tr.fromWordId &&
				toWordId == tr.toWordId &&
				totalCount == tr.totalCount &&
				typeId == tr.typeId;
		}
	};


	int printSentences(bool updateStatistics,unsigned int unknownCount,unsigned int quotationExceptions,unsigned int totalQuotations,int &globalOverMatchedPositionsTotal);
	int printSentencesCheck(bool skipCheck);
	void printTagSet(int logType, const lpchar_t * descriptor,int ts,vector <cTagLocation> &tagSet,int position,int PEMAPosition);
	void printTagSet(int logType, const lpchar_t * descriptor,int ts,vector <cTagLocation> &tagSet,int position,int PEMAPosition,vector <lpwstring> &words);

	// wikipedia
	cSource(MYSQL *parentMysql,int _sourceType,int _sourceConfidence);
	void reduceLocalFreebase(lpchar_t *path,lpchar_t *filename);
	void getObjectString(int where,lpwstring &object,vector <lpwstring> &lookForSubject,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool removePrecedingUncapitalizedWordsFromProperNouns=false);
	int getWikipediaPath(int principalWhere,vector <lpwstring> &wikipediaLinks,lpchar_t *path,vector <lpwstring> &lookForSubject,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases,bool removePrecedingUncapitalizedWordsFromProperNouns);
	int getObjectRDFTypes(int object,vector <cTreeCat *> &rdfTypes,unordered_map <lpwstring ,int > &topHierarchyClassIndexes,lpwstring fromWhere);
	int getExtendedRDFTypes(int where, vector <cTreeCat *> &rdfTypes, unordered_map <lpwstring, int > &topHierarchyClassIndexes, lpwstring fromWhere, bool ignoreMatches=false, bool fileCaching=true);
	class cExtendedMapType
	{
	public:
		unordered_map <lpwstring,int> RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
		vector <cTreeCat *> rdfTypes;
		unordered_map <lpwstring ,int > topHierarchyClassIndexes;
	};
	unordered_map<lpwstring, cExtendedMapType > extendedRdfTypeMap; 
	unordered_map<lpwstring, int > extendedRdfTypeNumMap; 
	int readExtendedRDFTypes(lpchar_t path[4096],vector <cTreeCat *> &rdfTypes,unordered_map <lpwstring ,int > &topHierarchyClassIndexes);
	int writeExtendedRDFTypes(lpchar_t path[4096],vector <cTreeCat *> &rdfTypes,unordered_map <lpwstring ,int > &topHierarchyClassIndexes);
	bool categoryMultiWord(lpwstring &childWord, lpwstring &lastWord);
	void getRDFTypeSimplificationToWordAssociationWithObjectMap(lpwstring object, vector <cTreeCat *> &rdfTypes, unordered_map<lpwstring, int> &wordAssociationMap);
	int getAssociationMapMaster(int where, int numWords, unordered_map <lpwstring, int > &associationMap, lpwstring fromWhere, bool fileCaching);
	bool noRDFTypes();
	int getExtendedRDFTypesMaster(int where, int numWords, vector <cTreeCat *> &rdfTypes, unordered_map <lpwstring, int > &topHierarchyClassIndexes, lpwstring fromWhere, int extendNumPP = -1, bool fileCaching = true, bool ignoreMatches=false);
	void testWikipedia();
	int getRDFWhereString(int where, lpwstring &oStr, const lpchar_t *separator, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool ignoreMatches=false);
	bool analyzeRDFTitle(unsigned int where, int &numWords, int &numPrepositions, lpwstring tableName);
	int getRDFTypes(int where, vector <cTreeCat *> &rdfTypes, lpwstring fromWhere, int extendNumPP = -1, bool ignoreMatches=false, bool fileCaching=true);
	int identifyISARelation(int principalWhere,bool initialTenseOnly, bool fileCaching);
	bool checkForUppercaseSources(int questionInformationSourceObject);
	bool skipSentenceForUpperCase(unsigned int &sentenceEnd);
	bool mixedCaseObject(int begin,int len);
	bool capitalizationCheck(int begin, int len);
	bool rejectISARelation(int principalWhere);
	bool isDefiniteObject(int where, const lpchar_t * definiteObjectType, int &ownerWhere, bool recursed);
	int checkParticularPartSemanticMatch(int logType, int parentWhere, cSource *childSource, int childWhere, int childObject, bool &synonym, int &semanticMismatch, bool fileCaching);
	void checkParticularPartSemanticMatchWord(int logType, int parentWhere, bool &synonym, unordered_set <lpwstring> &parentSynonyms, lpwstring pw, lpwstring pwme, int &lowestConfidence, unordered_map <lpwstring, int >::iterator ami);
	bool isObjectCapitalized(int where);
	bool ppExtensionAvailable(int where,int &numPPAvailable,bool nonMixed);
	void copySource(cSource* childSource, int begin, int end, unordered_map <int, int>& sourceIndexMap);
	vector <cSyntacticRelationGroup>::iterator copySRI(cSource *childSource,vector <cSyntacticRelationGroup>::iterator srg);
	int numWordsOfDirectlyAttachedPrepositionalPhrases(int whereChild);
	int copyDirectlyAttachedPrepositionalPhrase(cSource *childSource,int relPrep, unordered_map <int, int>& sourceIndexMap,bool clear);
	int copyDirectlyAttachedPrepositionalPhrases(int whereParentObject,cSource *childSource,int whereChild, unordered_map <int, int>& sourceIndexMap,bool clear);
	vector <int> copyChildrenIntoParent(cSource *childSource,int whereChild, unordered_map <int, int>& sourceIndexMap, bool enclosingLoop);
	int detectAttachedPhrase(cSyntacticRelationGroup *srg,int &relVerb);
	bool hasProperty(int where,int whereQuestionTypeObject, unordered_map <int,vector < vector <int> > > &wikiTableMap,vector <lpwstring> &propertyValues);
	bool compareObjectString(int whereObject1,int whereObject2);
	bool objectContainedIn(int whereObject,set <int> whereObjects);
	int ruleCorrectLPClassAdjectiveToAdverb(int wordSourceIndex);
	int ruleCorrectLPClassAdverbToAdjective(int wordSourceIndex);
	int ruleCorrectLPClassOnly(int wordSourceIndex, int startOfSentence);
	int ruleCorrectLPClassBetterFurther(int wordSourceIndex);
	int ruleCorrectLPClassMost(int wordSourceIndex);
	int ruleCorrectLPClassPrepositionAtEndOfSentence(int wordSourceIndex);
	int ruleCorrectLPClassPreferPrepositionOverAdverb(int wordSourceIndex);
	void ruleCorrectLPClassPreferPronounOverDemonstrativeDeterminer(int wordSourceIndex, int startOfSentence);

	int ruleCorrectLPClass(int wordSourceIndex, int startOfSentence);
	void initializePemaMap(size_t numTagSets);

	void setForms(void);
	cIntArray sentenceStarts;
	vector < vector<cObject>::iterator > masterSpeakerList;
	bool usePIS; // use preIdentifiedSpeakerObjects
	class cSection
	{
	public:
		unsigned int begin,endHeader;
		int speakersMatched,speakersNotMatched,counterSpeakersMatched,counterSpeakersNotMatched;
		vector <cOM> definiteSpeakerObjects,speakerObjects,objectsSpokenAbout,objectsInNarration;
		cIntArray subHeadings;
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
				definiteSpeakerObjects.push_back(cOM(buffer,where,total,error));
			if (error || (error=where>=(int)total)) return;
			if (error=!copy(count,buffer,where,total)) return;
			for (unsigned int I=0; I<count && where<(int)total; I++)
				speakerObjects.push_back(cOM(buffer,where,total,error));
			if (error || (error=where>=(int)total)) return;
			if (error=!copy(count,buffer,where,total)) return;
			for (unsigned int I=0; I<count && where<(int)total; I++)
				objectsSpokenAbout.push_back(cOM(buffer,where,total,error));
			if (error || (error=where>=(int)total)) return;
			if (error=!copy(count,buffer,where,total)) return;
			for (unsigned int I=0; I<count && where<(int)total; I++)
				objectsInNarration.push_back(cOM(buffer,where,total,error));
			if (error || (error=where>=(int)total)) return;
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
	lpwstring storageLocation;
	int lastSourcePositionSet;
	class cRelationHistory
	{
	public:
		cSourceWordInfo::cRMap::tIcRMap r;
		int toWhere;
		int narrativeNum;
		int subject;
		int flags; // 0 if physically present, 1 if not
		cRelationHistory(cSourceWordInfo::cRMap::tIcRMap _r,int _toWhere,int _narrativeNum,int _subject,int _flags)
		{
			r=_r;
			toWhere=_toWhere;
			narrativeNum=_narrativeNum;
			subject=_subject;
			flags=_flags;
		}
	};
	lpwstring objectString(int object,lpwstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false, const lpchar_t * separator=u" ");
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
	void addDelayedWordRelations(int where,int fromWhere,int toWhere,int relationType);
	void resolveWordRelations();
	cSourceWordInfo::cRMap::tIcRMap addRelations(int where,tIWMM from,tIWMM to,int relationType);
	// multiWordStrings is read once from txt files in Source initialization.
	vector < vector < tmWS > > multiWordStrings;
	// after each readWordsFromDB (after each book adds more words)
	// the multiWordStrings vector is scanned and any more objects which have all the words defined are moved to the multiWordObjects array,
	//   and the entry is removed from the multiWordStrings array
	vector < vector < vector <tIWMM> > > multiWordObjects;

	// MYSQL database
	MYSQL mysql;
	bool alreadyConnected;
	int sourceId;
	int createDatabase(const lpchar_t * server);
	int insertWordRelationTypes(void);
	bool signalFinishedProcessingSource(int sourceId);
	bool updateSourceEncoding(int readBufferType, lpwstring sourceEncoding, lpwstring etext);
	bool updateSourceStart(const lpwstring &start, int repeatStart, lpwstring &etext, int64_t actualLenInBytes);
	bool resetAllSource(void);
	bool resetSource(int beginSource,int endSource);
	void resetProcessingFlags(void);
	void updateSourceStatistics(int numSentences, int matchedSentences, int numWords, int numUnknown,
		int numUnmatched,int numOvermatched, int numQuotations, int quotationExceptions, int numTicks, int averagePatternMatch);
	void logPatternChain(int sourcePosition,int insertionPoint,enum cPatternElementMatchArray::chainType patternChainType);
	void printSRG(lpwstring logPrefix,cSyntacticRelationGroup* srg,int s,int ws,int wo,int ps,bool overWrote,int matchSum,lpwstring matchInfo,int logDestination=LOG_WHERE);
	void printSRG(lpwstring logPrefix,cSyntacticRelationGroup* srg,int s,int ws,int wo,lpwstring ps,bool overWrote,int matchSum,lpwstring matchInfo,int logDestination=LOG_WHERE);
	int checkInsertPrep(set <int> &relPreps,int wp,int wo);
	void getAllPreps(cSyntacticRelationGroup* srg,set <int> &relPreps,int wo=-1);
	int flushMultiWordRelations(set <int> &objects);
	int initializeNounVerbMapping(void);
	void getSynonyms(lpwstring word, unordered_set <lpwstring> &synonyms, int synonymType);
	void getSynonyms(lpwstring word, vector <unordered_set <lpwstring> > &synonyms, int synonymType);
	
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
	vector <cSyntacticRelationGroup> syntacticRelationGroups;
	lpwstring phraseString(int where,int end,lpwstring &logres,bool shortFormat,const lpchar_t *separator=u" ");
	lpwstring whereString(int where,lpwstring &logres,bool shortFormat,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, const lpchar_t * separator,int &numWords);
	lpwstring whereString(int where,lpwstring &logres,bool shortFormat);
	lpwstring whereString(vector <int> &where,lpwstring &logres);
	lpwstring whereString(set <int> &where,lpwstring &logres);
	lpwstring objectString(vector <cObject>::iterator object,lpwstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false, const lpchar_t * separator=u" ");
	lpwstring objectString(vector <cOM> &oms,lpwstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false);
	lpwstring objectSortedString(vector <cOM> &objects,lpwstring &logres);
	lpwstring objectString(set <int> &objects,lpwstring &logres,bool shortNameFormat=true);
	lpwstring objectString(vector <int> &objects,lpwstring &logres);
	lpwstring objectString(cSpeakerGroup::cGroup &group,lpwstring &logres);
	lpwstring objectSortedString(vector <int> &objects,lpwstring &logres);
	lpwstring objectString(cOM om,lpwstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag=false);
	lpwstring wordString(vector <tIWMM> &words,lpwstring &logres);
	const lpchar_t *getOriginalWord(int I, lpwstring &out, bool concat, bool mostCommon = false);
	bool analyzeEnd(const lpwstring path, int begin, int end, bool &multipleEnds);
	void writeWords(lpwstring oPath, lpwstring specialExtension);
	int scanForPatternTag(int where, int tag);
	int scanForPatternElementTag(int where, int tag);
	int printSentence(unsigned int rowsize, unsigned int begin, unsigned int end, bool containsNotMatched);
	int getSubjectInfo(cTagLocation subjectTagset, int whereSubject, int &nounPosition, int &nameLastPosition, bool &restateSet, bool &singularSet, bool &pluralSet, bool &adjectivalSet, bool &embeddedS1);
	bool longSubjectBindingMismatch(int wordIndex, int principalWherePosition, int primaryPMAOffset, int whereVerb);
	bool evaluateSubjectVerbAgreement(int verbPosition, int whereSubject, bool& agreementTestable);
	int queryPatternDiff(int position, lpwstring pattern, lpwstring differentiator);
	int queryPattern(int position, lpwstring pattern, int &maxEnd);
	int queryPattern(int position, lpwstring pattern);
	bool matchPattern(cPattern *p, int begin, int end, bool fill);
	vector <cObject> objects; // each object has only one position within the text

private:
	lpwstring primaryQuoteType,secondaryQuoteType;

	// filled during other operations
	lpchar_t *bookBuffer;
	int64_t bufferLen,bufferScanLocation;
	int lastPEMAConsolidationIndex;
	vector <int> printMaxSize;
	vector < vector <int> > rememberCompetingCompoundObjectPositions; // for multiple compound object processing

	bool compoundObjectSubChain(vector < int > &objectPositions);
	unsigned int getNumCompoundObjects(int where,int &combinantScore,lpwstring &combinantStr);
	void markMultipleObjects(int where);
	bool setAdditionalRoleTags(int where,int &firstFreePrep,vector <int> &futureBoundPrepositions,bool inPrimaryQuote,bool inSecondaryQuote,
		bool &nextVerbInSeries,int &sense,int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int sentenceBegin,int sentenceEnd,vector < vector <cTagLocation> > &tagSets);
	//void collectTagSetsFromSentence(unsigned int begin,unsigned int end,int &lastOpeningPrimaryQuote,unsigned int &section);
	int replicate(int recursionLevel,int PEMAPosition,int position,vector <cTagLocation> &tagSet,vector < vector <cTagLocation> > &childTagSets,vector < vector <cTagLocation> > &tagSets, unordered_map <int, vector < vector <cTagLocation> > > &TagSetMap);
	int collectTags(int rLevel,int PEMAPosition,int position,vector <cTagLocation> &tagSet,vector < vector <cTagLocation> > &tagSets, unordered_map <int, vector < vector <cTagLocation> > > &TagSetMap);
	int getPEMAPosition(int position,int offset);
	int scanUntil(const lpchar_t *start,int repeat,bool printError);

	// printing sentences
	//unsigned int getShortLen(int position);
	unsigned int getMaxDisplaySize(vector <cWordMatch>::iterator &im,int numPosition);
	bool sumMaxLength(unsigned int begin,unsigned int end,unsigned int &matchedTripletSumTotal,int &matchedSentences,bool &containsUnmatchedElement);

	bool isSectionHeader(unsigned int begin,unsigned int end,unsigned int &sectionEnd);
	int updatePEMACosts(int PEMAPosition,int pattern,int begin,int end,int position,vector<cPatternElementMatchArray::tPatternElementMatch *> &ppema);
	void reduceParent(int position,unsigned int PMAOffset,int diffCost);

	bool matchPatternAgainstSentence(cPattern *p,int s,bool fill);
	int matchIgnoredPatternsAgainstSentence(unsigned int s,unsigned int &patternsTried,bool fill);
	int matchPatternsAgainstSentence(unsigned int s,unsigned int &patternsTried);
	//void collectMatchedPatterns(lpwstring markType,int position,cIntArray &endPositions);

	void logOptimizedString(lpchar_t *line,unsigned int lineBufferLen,unsigned int &linepos);
	//unsigned int getPMAMinCost(unsigned int position,int parentPattern,int rootPattern,int childend);
	int getMinCost(cPatternElementMatchArray::tPatternElementMatch *pem, int &minPEMAOffset);

	// speaker resolution   - resolving quotes, speakers and pronouns
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
	int syntacticRelationGroupsIdEnd; // marks the split between the space relations discovered by identifySpeakers and relations discovered by resolveSpeakers
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
	bool intersect(vector <tIWMM> &m1, const lpchar_t ** a,bool &allIn,bool &oneIn);
	bool intersect(int where1,int where2);
	bool isSubsetOfSpeakers(int where,int ownerWhere,set <int> &speakers,bool inPrimaryQuote,bool &atLeastOneInSpeakerGroup);
	bool rejectSG(int ownerWhere,set <int> &speakers,bool inPrimaryQuote);
	void setMatched(int where,vector <int> &objects);

	void getPOVSpeakers(set <int> &povSpeakers);
	void getPOVSpeakers2(const int where, vector <cSyntacticRelationGroup>::iterator srg, set <int>& povSpeakers, bool& nonPOVSpeakerOverride);
	void getCurrentSpeakers(set <int> &speakers,set <int> &povSpeakers);
	int checkIfOne(int I,int latestObject,set <int> *speakers);
	bool resolveMetaGroupWordOrderedFutureObject(int where,vector <cOM> &objectMatches);
	bool resolveMetaGroupFormerLatter(int where,int previousS1,int latestOwnerWhere,vector <cOM> &objectMatches);
	bool resolveMetaGroupFirstSecondThirdWordOrderedObject(int where,int lastBeginS1,vector <cOM> &objectMatches,int latestOwnerWhere);
	bool resolveMetaGroupPlural(int latestOwnerWhere,bool inQuote,vector <cOM> &objectMatches);
	bool resolveMetaGroupSpecifiedOther(int where,int latestOwnerWhere,bool inQuote,vector <cOM> &objectMatches);
	void resolveMetaGroupGenericOtherTwoSpeaker(const int where, vector <cSpeakerGroup>::iterator sg, vector <cOM>& objectMatches);
	bool resolveMetaGroupGenericObserver(const int where, bool inQuote, int latestOwnerWhere, int latestObject, int o, int latestObjectWhere, vector <cSpeakerGroup>::iterator sg, vector <cOM>& objectMatches);
	void resolveMetaGroupGenericLatestSubGroupedSpeaker(const int where, int latestObject, int o, int latestObjectWhere, vector <cSpeakerGroup>::iterator sg, vector <cOM>& objectMatches);
	void resolveMetaGroupGenericLatestGroupedSpeaker(const int where, int latestObject, int o, int latestObjectWhere, vector <cSpeakerGroup>::iterator sg, vector <cOM>& objectMatches);
	void resolveMetaGroupMatchLatestSpeakerMatchingGender(const int where, const int o, int latestObjectWhere, vector <cSpeakerGroup>::iterator sg, const int latestObject, vector <cOM>& objectMatches);
	bool resolveMetaGroupGenericOtherOne(const int where, int latestObject, int wordsTraversed, int latestObjectWhere, bool crossQuotes, set <int>*& speakers, vector <cOM>& objectMatches);
	bool resolveMetaGroupGenericBackwardsMatch(const int where, int latestOwnerWhere, bool inQuote, vector <cSpeakerGroup>::iterator csg, vector <cOM>& objectMatches);
	void limitObjectMatchesToAudienceAndPreviousSpeakers(int where, vector <cSpeakerGroup>::iterator csg, vector <cOM>& objectMatches);
	void resolveMetaGroupGenericOtherGetObjectMatchesPreferLocalPhysicallyPresentSpeakers(int where, vector <cSpeakerGroup>::iterator csg, vector <cOM>& objectMatches);
	bool resolveMetaGroupGenericOther(int where,int latestOwnerWhere,bool inQuote,vector <cOM> &objectMatches);
	bool resolveMetaGroupNonNameObject(int where,bool inQuote,vector <cOM> &objectMatches,int &latestOwnerWhere);
	bool resolveMetaGroupInLocalObjects(int where,int o,vector <cOM> &objectMatches,bool onlyFirst);
	bool resolveMetaGroupOne(int where,bool inPrimaryQuote,vector <cOM> &objectMatches,bool &chooseFromLocalFocus);
	bool resolveMetaGroupTwo(int where,bool inQuote,vector <cOM> &objectMatches);
	bool resolveMetaGroupJoiner(int where,vector <cOM> &objectMatches);
	bool resolveMetaGroupOther(int where,vector <cOM> &objectMatches);
	bool resolveMetaGroupByAssociationAddSpeakerIfLatestOwnerWhereNotObserverSameGender(const int where, const int sg, vector <cOM>& objectMatches,
		const int latestOwnerWhere, const bool restrictSGToGrouped, const bool friendOfObserver, set <int>* speakers, int& numInSpeakers);
	bool temporarilyFill12PersonLatestOwnerWhere(const int where, const int latestOwnerWhere);
	bool findMinimallyAssociatedSpeakerGroup(const int where, const int latestOwnerWhere, const bool inPrimaryQuote, bool &restrictSGToGrouped, int &sg);
	bool resolveMetaGroupByAssociation(int where,bool inPrimaryQuote,vector <cOM> &objectMatches,int latest);
	bool resolveMetaGroupSpecificObject(int where,bool inPrimaryQuote,bool inSecondaryQuote,bool definitelyResolveSpeaker,int lastBeginS1,int lastRelativePhrase,vector <cOM> &objectMatches,bool &chooseFromLocalFocus);
	bool resolveMetaGroupObject(int where,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,
													bool definitelySpeaker,bool resolveForSpeaker,bool avoidCurrentSpeaker,bool &mixedPlurality,bool limitTwo,vector <cOM> &objectMatches,bool &chooseFromLocalFocus);

	int preferWordOrder(int wordOrderSensitiveModifier,vector <int> &locations);
	vector <cSpeakerGroup>::iterator containingSpeakerGroup(int where);
	bool resolveOccRoleActivityObject(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int wordOrderSensitiveModifier,bool physicallyPresent);
	void resolveRelativeObject(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int wordOrderSensitiveModifier);
	bool tryGenderedSubgroup(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int whereGenderedSubgroupCount,bool limitTwo);
	bool matchOtherObjects(vector <int> &otherGroupedObjects,int speakerGroup,vector <cOM> &objectMatches);
	bool scanFutureGenderedMatchingObjects(int where,bool inQuote,vector <cObject>::iterator object,vector <cOM> &objectMatches);
	void includeWordOrderPreferences(int where,int wordOrderSensitiveModifier);
	bool resolveGenderedObject(int where,bool definitelyResolveSpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,
					vector <cOM> &objectMatches,vector <cObject>::iterator object,int wordOrderSensitiveModifier,
					int &subjectCataRestriction,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated);
	void processSubjectCataRestriction(int where,int subjectCataRestriction);
	void addPreviousDemonyms(int where);
	bool addNewNumberedSpeakers(int where,vector <cOM> &objectMatches);
	bool addNewSpeaker(int where,vector <cOM> &objectMatches);
	bool resolveBodyObjectClass(int where,int beginEntirePosition,vector <cObject>::iterator object,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool resolveForSpeaker,bool avoidCurrentSpeaker,int wordOrderSensitiveModifier,int subjectCataRestriction,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated,bool &changeClass,vector <cOM> &objectMatches);
	void excludeSpeakers(int where,bool inPrimaryQuote,bool inSecondaryQuote);
	void discouragePOV(int where,bool inQuote,bool definitelySpeaker);
	void excludeObservers(int where,bool inQuote,bool definitelySpeaker);
	void excludePOVSpeakers(int where, const lpchar_t * fromWhere);
	bool resolveWordOrderOfObject(int where,int wo,int ofObjectWhere,vector <cOM> &objectMatches);
	void setQuoteContainsSpeaker(int where,bool inPrimaryQuote);
	void setResolved(int where,vector <cLocalFocus>::iterator lsi,bool isPhysicallyPresent);
	void setMatchesAndLocalResolved(int where, bool resolveForSpeaker, bool isPhysicallyPresent);
	void pushSpeakerGroupResolvedObjectToLocalFocus(int where, bool inPrimaryQuote, bool inSecondaryQuote);
	bool resolveSpecialPronounObjects(int where, bool inPrimaryQuote, bool inSecondaryQuote);
	bool getPresentAssertion(int where, bool inPrimaryQuote);
	bool unresolvableObject(int where, int beginEntirePosition, bool inPrimaryQuote, bool inSecondaryQuote, bool resolveForSpeaker, bool isPhysicallyPresent);
	void reclassifyGenderedOccupationalRoleActivityToNameObject(int where, int beginEntirePosition, bool inPrimaryQuote, bool inSecondaryQuote);
	bool resolveFirstPersonSecondPersonPronoun(int where, int person, bool inPrimaryQuote, bool inSecondaryQuote);
	void resolveGenderedOccupationalRoleActivityClass(int where, vector <cObject>::iterator object, vector <cOM>& objectMatches, const int wordOrderSensitiveModifier,
		bool& chooseFromLocalFocus, const bool isPhysicallyPresent, const bool physicallyEvaluated);
	bool resolveIsKindOf(int where, bool definitelySpeaker, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb,
		bool resolveForSpeaker, bool avoidCurrentSpeaker, bool limitTwo, vector <cOM>& objectMatches);
	void deleteCurrentSpeaker(int where);
	void avoidFollowingRelativeClauseSubject(int where, vector <cObject>::iterator object, bool chooseFromLocalFocus, vector <cOM>& objectMatches);
	bool definitelyNotSpeaker(int where, vector <cObject>::iterator object, bool definitelySpeaker);
	void reclassifyWikiPersonSubject(int where, vector <cObject>::iterator object);
	bool isNotLocallyMatched(int where, vector <cObject>::iterator object, int beginEntirePosition, int lastBeginS1, bool& notSpeaker);
	void pushObjectMatchesToLocalFocus(int where, vector <cObject>::iterator object, bool inPrimaryQuote, bool inSecondaryQuote, bool definitelySpeaker, bool notSpeaker, vector <cOM>& objectMatches);
	bool pleonasticIt(int where);
	void pushObjectMatchesIntoLocalFocusAndLocation(int where, vector <cObject>::iterator object, bool inPrimaryQuote, bool inSecondaryQuote, bool definitelySpeaker, bool notSpeaker, bool inSpeakerGroup, vector <cOM>& objectMatches);
	void cataphoricallyMatch(int where, int lastBeginS1, vector <cObject>::iterator object);
	bool resolveSpecificClassObject(const int where, const bool definitelySpeaker, const bool inPrimaryQuote, const bool inSecondaryQuote, const int lastBeginS1, const int lastRelativePhrase, const int lastQ2, const int lastVerb, const int beginEntirePosition, int& subjectCataRestriction,
		const bool resolveForSpeaker, const bool avoidCurrentSpeaker, const bool limitTwo, bool& chooseFromLocalFocus, const bool isPhysicallyPresent, const bool physicallyEvaluated, bool& mixedPlurality, vector <cOM>& objectMatches, vector <cObject>::iterator &object);
	void resolveObject(int where, bool definitelySpeaker, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, bool resolveForSpeaker, bool avoidCurrentSpeaker, bool limitTwo);
	bool quotedString(unsigned int beginQuote,unsigned int endQuote,bool &noTextBeforeOrAfter,bool &noSpeakerAfterward);
	int speakerBefore(int beginQuote,bool &previousParagraph);
	void setSameAudience(int whereVerb,int speakerObjectPosition,int &audienceObjectPosition);
	int scanForSpeaker(int position,bool &definitelySpeaker,bool &crossedSectionBoundary,int &audienceObjectPosition);
	int repeatReplaceObjectInSectionPosition;
	lpwstring repeatReplaceObjectInSectionFromWhere;
	bool replaceObjectInSection(int where,int replacementObject,int objectToBeReplaced, const lpchar_t * fromWhere);
	void associateNyms(int where,int replacementObject,int objectToBeReplaced,lpchar_t *fromWhere);
	int getBodyObject(int o);
	unsigned int numMatchingGenderInSpeakerGroup(int o);
	void setSpeakerAndAudience(const int beginQuote, const int speakerObjectAt, const bool definitelySpeaker, const int audienceObjectPosition);
	void setDefiniteSpeaker(const int speakerObjectAt, const int audienceObjectPosition, const int audienceObject, int &speakerObject);
	void resolvePreviousSpeakerAudience(const int beginQuote, const int endQuote, int &lastDefiniteSpeaker, const int speakerObjectAt, const bool definitelySpeaker,
		const int lastBeginS1, const int lastRelativePhrase, const int lastQ2, const int lastVerb, const bool previousSpeakersUncertain, const int audienceObjectPosition);
	void imposeSpeaker(int beginQuote,int endQuote,int &lastDefiniteSpeaker,int speakerObjectAt,bool definitelySpeaker,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool previousSpeakersUncertain,int audienceObjectPosition,vector <int> &lastUnQuotedSubjects,int whereLastUnQuotedSubjects);
	void imposeMostLikelySpeakers(unsigned int beginQuote,int &lastDefiniteSpeaker,int audienceObjectPosition);
	lpwstring speakerResolutionFlagsString(int64_t flags,lpwstring &tmpstr);
	void resolveSpeakersUsingPreviousSubject(int where);
	void setAudience(int where,int whereQuote,int currentSpeakerWhere);
	void resolveAudienceByAlternation(int currentSpeakerWhere,bool definitelySpeaker,int urs,bool forwards);
	bool matchedObjectsEqual(vector <cOM> &match1,vector <cOM> &match2);
	bool resolveMatchesByAlternation(int currentSpeakerWhere,bool definitelySpeaker,int urs,bool forwards);
	void printUnresolvedLocation(int urs);
	void resolveSpeakersByAlternationForwards(int where,int currentSpeakerWhere,bool definitelySpeaker);
	void resolvePreviousSpeakersByAlternationBackwards(int where,int currentSpeakerWhere,bool definitelySpeaker);
	int flipSpeaker(int &previousSpeakers,int &beforePreviousSpeakers);
	int assignSecondarySpeaker(unsigned int beginQuote,unsigned int endQuote);
	void displayQuoteContext(unsigned int begin,unsigned int end);
	bool quoteTest(int q,unsigned int &quoteCount,int &lastQuote,tIWMM quoteType,tIWMM quoteOpenType,tIWMM quoteCloseType);
	void secondaryQuoteTest(int q,unsigned int &secondaryQuotations,int &lastSecondaryQuote,tIWMM secondaryQuoteWord,tIWMM secondaryQuoteOpenWord,tIWMM secondaryQuoteCloseWord);
	bool resolvePronounSpecialCases(const int where, const bool definitelySpeaker, const bool inPrimaryQuote, const bool inSecondaryQuote,
		const int lastBeginS1, const int lastRelativePhrase, const int lastQ2, const int lastVerb, 
		const bool resolveForSpeaker, const bool avoidCurrentSpeaker, const bool limitTwo, vector <cOM>& objectMatches);
	bool resolveGenderAndNumberMatchedIsRolePronoun(const int where, const bool definitelySpeaker, const bool inPrimaryQuote, const bool inSecondaryQuote,
		const int lastBeginS1, const int lastRelativePhrase, const int lastQ2, const int lastVerb,
		const bool resolveForSpeaker, const bool avoidCurrentSpeaker, vector <cOM>& objectMatches);
	bool identifyPleonasticIt(const int where, vector <cOM>& objectMatches);
	bool resolvePronounIsPlace(const int where, vector <cOM>& objectMatches);
	bool resolveAdjectivalNonSubject(int where, int lastBeginS1, vector <cOM>& objectMatches);
	void disallowSpeakerOfPreviousQuestion(int where);
	void disallowPOV(int where, const bool inPrimaryQuote);
	void excludeMixedPlurality(int where);
	bool resolvePronoun(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,int beginEntirePosition,
															bool resolveForSpeaker,bool avoidCurrentSpeaker,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated,int &subjectCataRestriction,vector <cOM> &objectMatches);
	enum pronounResolutionSearchType { anyType,anyPersonType,anyMalePersonType,anyFemalePersonType,anyPluralPersonType };
	bool findNoun(int I,pronounResolutionSearchType prsType,vector <cLocalFocus> &ls,bool inQuote);
	void createSemanticPatterns(void);
	bool preferVerbRel(int position,unsigned int J,cPattern *p);
	void consolidateWinners(int begin);
	void addSpeakerObjects(int position,bool toMatched,int where,vector <int> speakers,int64_t resolutionFlag);
	// exactly like PEMA but with position
	bool matchAliases(int where,int object,int aliasObject);
	bool matchAliases(int where,int object,set <int> &objects);
	bool matchAliases(int where,int object,vector <cOM> &objects);
	bool matchAliasLocation(int where,int &objectLocation, int &aliasLocation);
	bool matchAlias(int where,int object, int aliasObject);

	// agreement
	int getAllLocations(unsigned int position,int parentPattern,int rootp,int childLen,int parentLen,vector <unsigned int> &allLocations, int recursionLevel,unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions,bool &reassessParentCosts);
	int markChildren(cPatternElementMatchArray::tPatternElementMatch *pem,int position,int recursionLevel,int allRootsLowestCost, unordered_map <int, cCostPatternElementByTagSet> &tertiaryPEMAPositions,bool &reassessParentCosts);
	bool findLowCostTag(vector<cTagLocation> &tagSet,int &cost, const lpchar_t * tagName,cTagLocation &lowestCostTag,int parentCost,int &nextTag);
	void switchSpecialSubjectWithObject(unsigned int position, cPatternMatchArray::tPatternMatch* pm, vector<cTagLocation>& tagSet, int subjectTag, int mainVerbTag);
	bool capitalizedVerbWithPeriod(int subjectTag, int verbAgreeTag, int position, vector<cTagLocation>& tagSet, int& traceSource);
	bool logLongSubject(int subjectTag, int verbAgreeTag, int position, int nounPosition, vector<cTagLocation>& tagSet, int& traceSource);
	bool adjectiveSubjectNotWithBeVerb(int subjectTag, int verbAgreeTag, int position, vector<cTagLocation>& tagSet, int& traceSource);
	bool possessiveDeterminerSubject(int subjectTag, int nextSubjectTag, int mainVerbTag, int nextVerbAgreeTag, int position, vector<cTagLocation>& tagSet, int& traceSource);
	bool theMostSubjectWithPastVerb(int subjectTag, int position, vector<cTagLocation>& tagSet, int& traceSource);
	bool agreeVerbNotFoundOrQuestion(int& conditionalTag, int& nextConditionalTag, int futureTag, int subjectTag, int nextSubjectTag, int mainVerbTag, int nextMainVerbTag,
		int& verbAgreeTag, int& nextVerbAgreeTag, int position, int nounPosition, vector<cTagLocation>& tagSet, int& traceSource);
	void decreaseSubjectVerbCostIfRelated(cPatternMatchArray::tPatternMatch* parentpm, cPatternMatchArray::tPatternMatch* pm, unsigned parentPosition, unsigned int position,
		vector<cTagLocation>& tagSet, int nounPosition, int verbPosition, int subjectTag, int mainVerbTag, int verbAgreeTag, int nextVerbAgreeTag, int& relationCost);
	void substitutePrepObjectSomeOf(int& nounPosition, bool& singularSet, bool& pluralSet);
	void determineSingularOrPlural(int nounPosition, int person, int position, int nameLastPosition, int inflectionFlags, bool& singularSet, bool& pluralSet);
	bool isSubjunctiveMood(int subjectTag, int position, int verbPosition, int person, vector<cTagLocation>& tagSet);
	bool agreeInPersonPluralityAndTense(int inflectionFlags, int verbPosition, int person, bool singularSet, bool pluralSet);
	void disagreementWithAmbiguousTense(bool agree, bool ambiguousTense, int verbAgreeTag, int conditionalTag, int mainVerbTag, int verbPosition, int position, vector<cTagLocation>& tagSet);
	void reduceCostIfRestate(bool restateSet, int &relationCost, int subjectTag, vector<cTagLocation>& tagSet);
	int evaluateSubjectVerbAgreement(cPatternMatchArray::tPatternMatch* parentpm, cPatternMatchArray::tPatternMatch* pm, unsigned int parentPosition, unsigned int position, vector<cTagLocation> tagSet, int& traceSource);
	// agreement section end

	void printSpeakerQueue(int where);
	bool evaluateName(vector <cTagLocation> &tagSet,cName &name,bool &isMale,bool &isFemale,bool &isPlural,bool &isBusiness);
	bool identifyName(int begin, int principalWhere, int end, int& nameElement, cName& name, bool& isMale, bool& isFemale, bool& isNeuter, bool& isPlural, bool& isBusiness,
		bool& comparableName, bool& comparableNameAdjective, bool& requestWikiAgreement, OC& objectClass);
	bool identifyName(int where,int &element,cName &name,bool &isMale,bool &isFemale,bool &isPlural,bool &isBusiness);
	void foldNames(int where,cObject s,int speakerSet);
	bool findSpecificAnaphor(lpwstring tagName,int where,int element,int &specificWhere,bool &pluralNounOverride,bool &embeddedName);
	bool reEvaluateHailedObjects(int beginQuote,bool setMatched);
	void boostRecentSpeakers(int where,int beginQuote,int speakersConsidered,int speakersMentionedInLastParagraph,bool getMostLikelyAudience);
	void scoreLocalObject(const vector <cLocalFocus>::iterator lsi, const unsigned int beginQuote, const unsigned int endQuote,
		const int rejectObjectPosition,	bool& atLeastOne, bool& objectsRejected, const bool getMostLikelyAudience,
		int& saveBlockedObject, int& speakersConsidered, int& speakersMentionedInLastParagraph,
		const tIWMM secondaryQuoteOpenWord, const tIWMM secondaryQuoteCloseWord);
	void setPreviousSpeakers(const unsigned int beginQuote,
		bool &previousSpeakersUncertain, const int wherePreviousLastSubjects, vector <int>& previousLastSubjects,
		bool &atLeastOne, const bool objectsRejected);
	void reresolveHailObjects(const unsigned int beginQuote, const unsigned int endQuote,
		const int lastBeginS1, const int lastRelativePhrase, const int lastQ2, const int lastVerb, int &saveBlockedObject);
	void putSpeakersInLocalSalience(const unsigned int beginQuote);
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
	void getOriginalWords(int I,vector <lpwstring> &wsoStrs,bool notFirst);
	bool searchExactMatch(cObject &object,int position);
	lpwstring getClass(int objectClass);
	void printLocalFocusedObjects(int where,int matchObjectClass);
	bool isPleonastic(unsigned int where);
	bool isPleonastic(tIWMM w);
	bool disallowReference(int object,vector <int> &disallowedReferences);
	void scanTag(int position,int rObject,int idExemption,vector <int> &disallowedReferences);
	void checkForPreviousPP(int where,vector <int> &disallowedReferences);
	void coreferenceFilterLL5(int where,vector <int> &disallowedReferences);
	lpchar_t *loopString(int where,lpwstring &tmpstr);
	int coreferenceFilterDetermineBeginAndEndS1(const int lastBeginS1, const int lastRelativePhrase, const int lastQ2, int& begin, int& endS1);
	bool noCoreferenceFound(const int where, const int endS1);
	void disallowCompoundReferences(const int where, vector <int>& disallowedReferences);
	void rule2DisallowObjects(const int where, const int directObject, const int indirectObject, const int indirectObjectPosition, vector <int>& disallowedReferences);
	void rule3DisallowPrepObject(const int where, const int whereVerb, const int endS1, const int directObjectPosition, vector <int>& disallowedReferences);
	void rule23ObjectDisallowSubjectOrPrepObject(const int where, const int whereVerb, const int whereSubject, const int rObject, const int subjectObject, const int directObject, const int directObjectPosition, vector <int>& disallowedReferences, int& subjectCataRestriction);
	void rule4MetaNameEquivalenceSameNounPattern(const int where, const int rObject, const int lastBeginS1, int& idExemption, vector <int>& disallowedReferences);
	bool getMixedPlurality(const int begin, const int endS1);
	void scanEntireCompoundObject(const int where, const int rObject, const int idExemption, vector <int>& disallowedReferences);
	int coreferenceFilterLL2345(int position,int rObject, vector <int> &disallowedReferences,int lastBeginS1,int lastRelativePhrase,int lastQ2,bool &mixedPlurality,int &subjectCataRestriction);
	int pronounCoreferenceFilterLL6(int P, int lastBeginS1,vector <int> &disallowedReferences);
	int reflexivePronounCoreference(int P, int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool inPrimaryQuote,bool inSecondaryQuote);
	int identifySubType(int principalWhere,bool &partialMatch);
	void getPrincipalWhereAndEndAndNameInfo(lpwstring tagName, int where, int element, int& specificWhere, bool& pluralNounOverride, bool& embeddedName, unsigned int& end, int& nameElement);
	bool identifyAdjectivalObjects(const int where, lpwstring tagName, const int principalWhere, const int end, int &ownerWhere, bool &isOwnerGendered, bool &isOwnerFemale, bool &isOwnerMale, bool &isOwnerPlural, bool& hasDeterminer, unsigned int &I);
	bool refineObjectClassAndGender(const int where, const int ownerWhere, const int principalWhere, const int begin, const int end,
		//const bool isOwnerMale, const bool isOwnerFemale, const bool isOwnerGendered, cName& name,
		enum OC& objectClass, bool& isFemale, bool& isMale, bool& isNeuter, bool& plural, const bool adjectival);
	void setOwnerGender(cObject& thisObject);
	void setRelatedObjects(cObject& thisObject);
	bool identifyAdjectiveObjectClassAndGender(const int where, const int ownerBegin, int& begin, int principalWhere, int& ownerWhere, enum OC& objectClass, bool& isMale, bool& isFemale, bool& plural);
	int determineNonOwnershipObjectInfo(int& where, int& element, const int begin, int& principalWhere, const int ownerWhere, unsigned int& end,
		cName& name, bool& isMale, bool& isFemale, bool& isNeuter, bool& plural, bool& isBusiness, const bool adjectival,
		OC& objectClass, const lpwstring tagName);
	bool determineIfBodyObject(const bool isOwnerGendered, const bool isOwnerMale, const bool isOwnerFemale, bool& isMale, bool& isFemale, bool& isNeuter, bool& singularBodyPart,
		const int principalWhere, const unsigned int begin, const unsigned int end, const int ownerWhere, enum OC& objectClass);
	int identifyObject(int tag, int where, int element, bool adjectival, int previousOwnerWhere, int ownerBegin);
	void setRole(int position,cPatternElementMatchArray::tPatternElementMatch *pem);
	int getObjectEnd(vector <cObject>::iterator object);
	bool overlaps(vector <cObject>::iterator object,vector <cObject>::iterator matchingObject);
	int getLocationBefore(vector <cObject>::iterator object,int where);
	void localRoleBoost(vector <cLocalFocus>::iterator lsi,int I,uint64_t objectRole,int age);
	void adjustSaliencesByParallelRoleAndPlurality(int where,bool inQuote,bool forSpeakerIdentification,int lastGenderedAge);
	void pushSpeaker(int where,int s, int sf, const lpchar_t * fromWhere);
	bool matchObjectToSpeakers(int I,vector <cOM> &currentSpeaker,vector <cOM> &previousSpeaker,int inflectionFlags,uint64_t quoteFlags,int lastEmbeddedStoryBegin);
	void removeSpeakers(int I,vector <cOM> &speakers);
	void removeSpeakers(int I,set <int> &speakers);
	void resolveFirstSecondPersonPronoun(int where,uint64_t flags,int lastEmbeddedStoryBegin,vector <cOM> &currentSpeaker,vector <cOM> &previousSpeaker);
	cOM mostRecentGenderedObject(int where,vector <cObject>::iterator object,vector <cOM> &currentMaleObjects,vector <cOM> &currentFemaleObjects);
	void testIfDeleteSingularObjects(vector <cOM> currentMaleObjects,vector <cOM> currentFemaleObjects,vector <cOM> currentGenderUnknownObjects);
	bool getHighestEncounters(int &highestDefinitelyIdentifiedEncounters,int &highestIdentifiedEncounters,int &highestEncounters);
	void getOwnerSex(int ownerObject,bool &matchMale,bool &matchFemale,bool &matchNeuter, bool &matchPlural);
	void printNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap1,
								 vector <tIWMM> &nyms2, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap2, const lpchar_t * type, const lpchar_t * subtype,
								 int sharedMembers,lpwstring &logMatch);
	void printNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap, const lpchar_t * type, const lpchar_t * subtype, const lpchar_t * subsubtype);
	bool setNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap);
	void clearNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap);
	bool hasDemonyms(vector <cObject>::iterator o);
	bool sharedDemonyms(int where,bool traceNymMatch,vector <cObject>::iterator o,vector <cObject>::iterator lso,tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch);
	bool nymNoMatch(vector <cObject>::iterator o,tIWMM adj);
	bool nymNoMatch(int where,vector <cObject>::iterator o,vector <cObject>::iterator lso,bool getFromMatch,bool traceThisMatch,lpwstring &logMatch,tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch, const lpchar_t * type);
	int limitedNymMatch(vector <cObject>::iterator o,vector <cObject>::iterator lso,bool traceNymMatch);
	int nymMatch(vector <cObject>::iterator o,vector <cObject>::iterator lso,bool getFromMatch,bool traceNymMatch,bool &explicitOccupationMatch,lpwstring &logMatch,tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch, const lpchar_t * type);
	int nymMapMatch(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap1,
							 vector <tIWMM> &nyms2, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap2,
							 bool mapOnly,bool getFromMatch,bool traceNymMatch,lpwstring &logMatch,
							 tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch, const lpchar_t * type, const lpchar_t * subtype);
	void adjustForPhysicalPresence();
	void adjustSaliencesByGenderNumberAndOccurrenceAgeAdjust(int where,int object,bool inPrimaryQuote,bool inSecondaryQuote,bool forSpeakerIdentification,int &lastGenderedAge,vector <int> &disallowedReferences,bool disallowOnlyNeuterMatches,bool isPhysicallyPresent,bool physicallyEvaluated);
	void setGender(int where, vector <cObject>::iterator o, bool& male, bool& matchMale, bool& female, bool& matchFemale, bool& neuter, bool& matchNeuter, bool& plural, bool& matchPlural);
	void setColocation(int where, vector <cObject>::iterator& oColocate, bool traceThisNym);
	int determineGenderAgreement(int& numUnambiguousSexMatch, vector <cLocalFocus>::iterator& unambiguousSexMatch, bool forSpeakerIdentification, vector <cLocalFocus>::iterator lsi,
		bool localMale, bool localFemale, bool localNeuter, bool matchMale, bool matchFemale, bool male, bool female, bool neuter);
	void fillTracingInfo(vector <cLocalFocus>::iterator lsi, int where, int object, bool inPrimaryQuote, bool inSecondaryQuote, bool forSpeakerIdentification, bool isPhysicallyPresent,
		vector <cObject>::iterator o, bool& male, bool& matchMale, bool& female, bool& matchFemale, bool& neuter, int highestDefinitelyIdentifiedEncounters, int highestIdentifiedEncounters, int highestEncounters,
		bool localNeuter, bool localMale, bool localFemale, bool ignorePlural, bool ignoreMPlural, bool allowedPlural,
		vector <cSpeakerGroup>::iterator sg, vector <cSpeakerGroup>::iterator esg);
	void adjustColocatedBodyObject(vector <cLocalFocus>::iterator lsi, int where, vector <cObject>::iterator oColocate, vector <cObject>::iterator lso, bool traceThisNym);
	void boostUnresolvableCloseGenderedObject(vector <cLocalFocus>::iterator lsi, int where, vector < vector <cLocalFocus>::iterator >& urps, bool forSpeakerIdentification, bool isPhysicallyPresent);
	void disallowSpeakerMatchToThirdPersonInPrimaryQuotes(vector <cLocalFocus>::iterator lsi, int where, int object, vector <cObject>::iterator o, bool inPrimaryQuote, vector <cSpeakerGroup>::iterator sg);
	void disallowSpeakerMatchToThirdPersonInSecondaryQuotes(vector <cLocalFocus>::iterator lsi, int where, int object, vector <cObject>::iterator o, bool inSecondaryQuote, vector <cSpeakerGroup>::iterator esg);
	void computePluralAgreement(vector <cLocalFocus>::iterator lsi, bool ignorePlural, bool ignoreMPlural, bool allowedPlural);
	void computeSalienceByAge(vector <cLocalFocus>::iterator lsi, bool male, bool female, bool neuter);
	void computerSalienceByNumEncounter(vector <cLocalFocus>::iterator lsi, int highestEncounters, int highestIdentifiedEncounters, int highestDefinitelyIdentifiedEncounters, bool forSpeakerIdentification);
	bool unknownBodyObject(vector <cObject>::iterator lso, bool neuter);
	void adjustLocalObjectSalienceByGenderNumberAndOccurrence(vector <cLocalFocus>::iterator lsi, int where, int object, bool inPrimaryQuote, bool inSecondaryQuote, bool forSpeakerIdentification, int& lastGenderedAge, bool isPhysicallyPresent,
		vector <cObject>::iterator o, bool& male, bool& matchMale, bool& female, bool& matchFemale, bool& neuter, bool& plural, bool traceThisNym, int& numUnambiguousSexMatch, 
		vector < vector <cLocalFocus>::iterator > &urps, int highestDefinitelyIdentifiedEncounters, int highestIdentifiedEncounters, int highestEncounters, vector <cLocalFocus>::iterator& unambiguousSexMatch, vector <cObject>::iterator oColocate);
	void reduceSalienceIfSpeakerIdentificationAndNotSpeaker(int where);
	void setUnresolvableObjectPreference(int where, vector < vector <cLocalFocus>::iterator >& urps);
	void adjustSaliencesByGenderNumberAndOccurrence(int where, int object, bool inPrimaryQuote, bool inSecondaryQuote, bool identifiedAsSpeaker, int& lastGenderedAge, bool isPhysicallyPresent);
	bool resolveNonGenderedGeneralObjectAgainstOneObject(int where,vector <cObject>::iterator object,vector <cOM> &objectMatches,int o,int sf,int lastWhere,int &mostRecentMatch);
	bool resolveNonGenderedGeneralObjectPlural(int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches);
	bool resolveNonGenderedGeneralObjectNumAddress(int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches);
	bool resolveNonGenderedGeneralObjectExpression(int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches);
	void narrowClassToGenderedIfIsProfession(int where);
	void resolveNonGenderedGeneralObjectSingularToPlural(int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches);
	void adjustForWordOrderSensitiveModifier(int where, vector <cOM>& objectMatches, int wordOrderSensitiveModifier);
	void resolveNonGenderedGeneralObject(int where,vector <cObject>::iterator &object,vector <cOM> &objectMatches,int wordOrderSensitiveModifier);
	bool potentiallyMergable(int where,vector <cObject>::iterator o1,vector <cObject>::iterator o2,bool allowBothToBeSpeakers,bool checkUnmergableSpeaker);
	void accumulateNameLikeStats(vector <cObject>::iterator &object,int o,bool &firstNameAmbiguous,bool &lastNameAmbiguous,tIWMM &ambiguousFirst,int &ambiguousNickName,tIWMM &ambiguousLast);
	void matchRelatedObjects(const int where, vector <cObject>::iterator &object, const int forwardCallingObject, const int objectToBeReplaced,
		bool& firstNameAmbiguous, bool& lastNameAmbiguous, tIWMM& ambiguousFirst, tIWMM& ambiguousLast, int& ambiguousNickName,
		set <int>& matchingObjects);
	void pushIntoObjectMatchesOrReplace(const int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches, const set <int>::iterator mo,
		const bool firstNameAmbiguous, const bool lastNameAmbiguous, const bool qualified, const bool globalSearch);
	void removeEliminatedObjects(set <int>& matchingObjects);
	void matchLocalObjectWithNameObject(vector <cLocalFocus>::iterator lsi, vector <cObject>::iterator& object,
		const int objectToBeReplaced, bool &unambiguousGenderFound,
		bool &firstNameAmbiguous, bool &lastNameAmbiguous, tIWMM& ambiguousFirst, tIWMM& ambiguousLast, int& ambiguousNickName,
		set <int> &matchingObjects);
	bool resolveNameObject(int where,vector <cObject>::iterator &object,vector <cOM> &objectMatches,int forwardCallingObject);
	//vector <cLocalFocus>::iterator in(int o,bool inQuote,bool neuter);
	vector <cLocalFocus>::iterator in(int o);
	bool in(int o,int where);
	vector <cOM>::iterator in(int o,vector <cOM> &objects);
	bool replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList, const lpchar_t * fromWhat);
	bool replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,vector <cObject::cLocation>::iterator ol);
	bool replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,int ol);
	void replaceObject(int replacementObject,int objectToBeReplaced, const lpchar_t * fromWhat);
	bool replaceObject(int where,int replacementObject, int objectToBeReplaced,vector <int> &objects, const lpchar_t * description,int begin,int end, const lpchar_t * fromWhat);
	bool replaceObject(int where,int replacementObject, int objectToBeReplaced,set <int> &objects, const lpchar_t * description,int begin,int end, const lpchar_t * fromWhat);
	bool replaceObjectInSpeakerGroup(int where,int replacementObject,int objectToBeReplaced,int sg, const lpchar_t * fromWhat);
	void replaceObjectWithObject(int where,vector <cObject>::iterator &object,int objectConfidentMatch, const lpchar_t * fromWhat);
	void resolveNameGender(int where,bool male,bool female);
	int getRoleSalience(uint64_t role);
	tIWMM setSex(vector <cTagLocation> &tagSet,int where,bool &isMale,bool &isFemale,bool isPlural);
	bool evaluateNameAdjective(vector <cTagLocation> &tagSet,cName &name,bool &isMale,bool &isFemale);
	bool identifyNameAdjective(int where,cName &name,bool &isMale,bool &isFemale);
	bool recentExit(vector <cLocalFocus>::iterator lsi);
	void testLocalFocus(int where,vector <cLocalFocus>::iterator lsi);
	bool pushObjectIntoLocalFocus(int where,int matchingObject,bool identifiedAsSpeaker,bool notSpeaker,bool inPrimaryQuote,bool inSecondaryQuote, const lpchar_t * fromWhere, vector <cLocalFocus>::iterator &lsi);
	vector <cLocalFocus>::iterator substituteAlias(int where,vector <cLocalFocus>::iterator lsi);
	void pushLocalObjectOntoMatches(int where,vector <cLocalFocus>::iterator lsi, const lpchar_t * reason);
	void narrowGender(int where,int toObject);
	void eliminateBodyObjectRedundancy(int where,vector <cOM> &objectMatches);
	bool evaluateCompoundObjectAsGroup(int where,bool &physicallyEvaluated);
	void matchAdditionalObjectsIfPlural(int where,bool isPlural,bool atLeastOneReference,
																						bool physicallyEvaluated,bool physicallyPresent,
																						bool inPrimaryQuote,bool inSecondaryQuote,
																						bool definitelySpeaker,bool mixedPlurality,
		vector <cLocalFocus>::iterator *highest,int numNeuterObjects,int numGenderedObjects,
		vector <int> &lsOffsets);
	void preferRelatedObjects(int where);
	void evaluateEachLocalObject(vector <cLocalFocus>::iterator lsi, int where, bool inPrimaryQuote, bool resolveForSpeaker, bool physicallyPresent, bool neuter,
		int object, bool& genericGender, bool& genericGenderOverride, vector <cLocalFocus>::iterator* highest);
	bool preferNonAudience(int where, vector <cLocalFocus>::iterator* highest, bool resolveForSpeaker);
	bool preferLastSubject(int where, vector <cLocalFocus>::iterator* highest, bool resolveForSpeaker);
	bool preferMatchingSubject(int where, vector <cLocalFocus>::iterator* highest);
	void resolveHailUsingSpeakers(int where, vector <cLocalFocus>::iterator* highest);
	void bodyObjectInheritsPreviousCompoundBodyMatch(int where, int object, vector <cLocalFocus>::iterator* highest);
	void resolveSpeakerSameGenderByAudienceOrLastSubject(int where, vector <cLocalFocus>::iterator* highest, bool resolveForSpeaker);
	void chooseBetweenMatches(int where, vector <cLocalFocus>::iterator* highest, bool resolveForSpeaker);
	void ifGroupJoinerPickMostRecentlyPhysicallyManifestedObject(int where, vector <cLocalFocus>::iterator* highest);
	void preferPhysicalPresentIfPhysicallyPresentPosition(int where, vector <cLocalFocus>::iterator* highest, bool physicallyEvaluated, bool physicallyPresent);
	void nullifyGenericGenderOverrideIfPhysicallyPresentAndSalientAlternateFound(int where, vector <cLocalFocus>::iterator* highest, bool genericGenderOverride);
	void preferMatchedAdjectives(int where, vector <cLocalFocus>::iterator* highest);
	void preferRecentPOVIfPronounAndSpeakersDefined(int where, vector <cLocalFocus>::iterator* highest);
	bool preferImplicitObjectMatchIfSingleAndUnowned(int where, vector <cLocalFocus>::iterator* highest, int I, int object, bool inPrimaryQuote, bool inSecondaryQuote, bool physicallyPresent);
	void substituteOwnerOfBodyObject(int where, int highestActualsf, bool isPlural);
	void adjustStatisticsForMatchedObjects(int where, vector <cOM>& objectMatches, bool definitelySpeaker);
	void insertPOVSpeaker(int where);
	void checkIfReplacedSpeaker(int where);
	void narrowToNeuterIfPlace(int where, vector <cLocalFocus>::iterator lsi, bool neuter);
	bool chooseBest(int where, bool definitelySpeaker, bool inPrimaryQuote, bool inSecondaryQuote, bool resolveForSpeaker, bool mixedPlurality);
	void removeObjectsFromNotMatched(int where,int matchWhere);
	void removeObjectsFromMatched(int where,int matchWhere,vector <cLocalFocus> &ls);
	void setSpeakerMatchesFromPosition(int where,vector <cOM> &objectsToSet,int fromPosition, const lpchar_t * fromWhere,uint64_t flag);
	void setObjectsFromMatchedAtPosition(int where,vector <cOM> &objectsSet,int matchWhere,bool identifiedAsSpeaker,uint64_t flag);
	bool removeMatchedFromObjects(int where,vector <cOM> &removeFrom,int matchWhere,bool identifiedAsSpeaker);

	// relations
	bool tagIsCertain(int position);
	bool getVerb(vector <cTagLocation> &tagSet,int &tag);
	bool getIVerb(vector <cTagLocation> &tagSet,int &tag);
	bool checkAmbiguousVerbTense(int whereVerb,int &sense,bool inQuote,tIWMM masterVerbWord);
	int getSimplifiedTense(int tense);
	char *tagSetTimeArray;
	unsigned int tagSetTimeArraySize;
	int getVerbTense(vector <cTagLocation> &tagSet,int verbTagIndex,bool &isId);
	bool checkRelation(cPatternMatchArray::tPatternMatch *parentpm,cPatternMatchArray::tPatternMatch *pm,int parentPosition,int position,tIWMM verbWord,tIWMM objectWord,int relationType);
	int calculateVerbAfterVerbUsage(int whereVerb,unsigned int nextWord, bool adverbialObject);
	void evaluateVerbObjectsInfo(cPatternMatchArray::tPatternMatch* pm, 
		vector <cTagLocation>& tagSet, bool assessCost, lpwstring purpose, int &whereObjectTag, int &nextObjectTag, unsigned int &numObjects,
		int& wo1, int& object1, tIWMM& object1Word, int& wo2, int& object2, tIWMM& object2Word, int verbTagIndex, tIWMM verbWord);
	int getAfterQuoteAttributionBenefit(cPatternMatchArray::tPatternMatch* pm, int whereVerb, int numObjects, int verbAfterVerbCost, int &verbObjectCost);
	int getVerbObjectCost(cPatternMatchArray::tPatternMatch* pm, vector <cTagLocation>& tagSet, int& voRelationsFound,
		const unsigned int whereVerb, const int verbTagIndex, const tIWMM verbWord, const int numObjects, int &objectDistanceCost,
		const int nextObjectTag, const tIWMM object1Word, const int object2, const int whereObjectTag);
	int getVerbAfterVerbCost(cPatternMatchArray::tPatternMatch* pm, vector <cTagLocation>& tagSet, lpwstring purpose,
		const unsigned int numObjects, const int verbTagIndex, tIWMM verbWord, const unsigned int whereVerb, const unsigned int nextWord, int advObjectTag);
	int evaluateVerbObjectsCost(cPatternMatchArray::tPatternMatch* parentpm, cPatternMatchArray::tPatternMatch* pm, const int parentPosition, const int position,
		vector <cTagLocation>& tagSet, int& voRelationsFound, int &traceSource, lpwstring purpose,
		const int whereObjectTag, const int nextObjectTag, unsigned int &numObjects,
		tIWMM object1Word, const int object2, tIWMM object2Word, const int verbTagIndex, tIWMM verbWord);
	int evaluateVerbObjects(cPatternMatchArray::tPatternMatch *parentpm,cPatternMatchArray::tPatternMatch *pm,int parentPosition,int position,vector <cTagLocation> &tagSet,bool infinitive,bool assessCost,int &voRelationsFound,int &traceSource,lpwstring purpose);
	void evaluateNounDeterminerAdjectiveVerbPresentParticiple(int begin, int end, int fromPEMAPosition, int& PNC);
	void evaluateNounDeterminerFromToOrToSame(int begin, int end, int fromPEMAPosition, int& PNC);
	void evaluateNounDeterminerPreferVerbAfterTo(int begin, int end, int fromPEMAPosition, int& PNC);
	void evaluateNounDeterminerIncorrectVerbalNoun(int& traceSource, int begin, int end, int fromPEMAPosition, int nAgreeTag, int& PNC);
	void evaluateNounDeterminerFromTo2OrPreferVerb(int begin, int end, int fromPEMAPosition, int& PNC);
	void evaluateNounDeterminerDetectSeparateTime(int begin, int end, int& PNC);
	void evaluateNounDeterminerDetectIncorrectOrderingDeterminersAfterHer(int begin, int end, int& PNC);
	void evaluateNounDeterminerDisallowPronounPrecededByNoun(int& traceSource, int begin, int end, int& PNC);
	void evaluateNounDeterminerDetectNounDeterminerMissedCost(vector <cTagLocation>& tagSet, int nounTag, int fromPEMAPosition);
	bool evaluateNounDeterminerSetHasDeterminer(vector <cTagLocation>& tagSet, int& traceSource, int begin, int nounTag, int nAgreeTag, int whereNAgree, tIWMM nounWord, bool& hasDeterminer);
	int evaluateNounDeterminer(vector <cTagLocation> &tagSet,bool assessCost,int &traceSource,int begin,int end, int fromPEMAPosition);
	bool hasTimeObject(int where);
	//int attachAdjectiveRelation(vector <cTagLocation> &tagSet,int whereObject);  see dynamicallyUpdateWordRelations.cpp
	//int attachAdverbRelation(vector <cTagLocation> &tagSet,int verbTagIndex,tIWMM verbWord); see dynamicallyUpdateWordRelations.cpp
	bool resolveObjectTagBeforeObjectResolution(vector <cTagLocation> &tagSet,int tag,tIWMM &word,lpwstring purpose);
	tIWMM resolveToClass(int position);
	tIWMM resolveObjectToClass(int where,int o);
	tIWMM fullyResolveToClass(int position);
	bool forcePrepObject(vector <cTagLocation> &tagSet,int tag,int &object,int &whereObject,tIWMM &word);
	bool resolveTag(vector <cTagLocation> &tagSet,int tag,int &object,int &whereObject,tIWMM &word);
	void trackVerbTenses(int where,vector <cTagLocation> &tagSet,
		bool inQuote,bool inQuotedString,bool inSectionHeader,
		bool ambiguousSense,int sense,bool &tenseError);
	void recordVerbTenseRelations(int where,int sense,int subjectObject,int whereVerb);
	void evaluateSubjectRoleTag(int where,int which,vector <int> whereSubjects,int whereObject,int whereHObject,int whereVerb,int whereHVerb,vector <int> subjectObjects,int tsSense,
														bool ignoreSpeaker,bool isNot,bool isNonPast,bool isNonPresent,bool isId,bool subjectIsPleonastic,bool inPrimaryQuote,bool inSecondaryQuote,bool backwardsSubjects);
	bool skipQuote(int &where);
	void scanForSubjectsBackwardsInSentence(int where,int whereVerb,bool isId,bool &objectAsSubject,bool &subjectIsPleonastic,vector <tIWMM> &subjectWords,vector <int> &subjectObjects,vector <int> &whereSubjects,int tsSense,bool &multiSubject,bool preferInfinitive);
	void discoverSubjects(int where,vector <cTagLocation> &tagSet,int subjectTag,bool isId,bool &objectAsSubject,bool &subjectIsPleonastic,vector <tIWMM> &subjectWords,vector <int> &subjectObjects,vector <int> &whereSubjects);
	void markPrepositionalObjects(int where,int whereVerb,bool flagInInfinitivePhrase,bool subjectIsPleonastic,bool objectAsSubject,bool isId,bool inPrimaryQuote,bool inSecondaryQuote,bool isNot,bool isNonPast,bool isNonPresent,bool noObjects,bool delayedReceiver,int tsSense,vector <cTagLocation> &tagSet);
	void addRoleTagsAt(int where, int I, bool inRelativeClause, bool withinInfinitivePhrase, bool subjectIsPleonastic, bool isNot, bool objectNot, int tsSense, bool isNonPast, bool isNonPresent, bool objectAsSubject, bool isId, bool inPrimaryQuote, bool inSecondaryQuote, const lpchar_t* fromWhere);
	void extendRolesThroughExtendedIdentitySentence(int where, int len, bool withinInfinitivePhrase, bool subjectIsPleonastic, bool isNot, int tsSense, bool isNonPast, bool isNonPresent, bool objectAsSubject, bool isId, bool inPrimaryQuote, bool inSecondaryQuote);
	int findPrepRole(int whereLastPrep,int role,int rejectRole);
	int processInternalInfinitivePhrase(int where,int whereVerb,int whereParentObject,int iverbTag,int firstFreePrep,vector <int> &futureBoundPrepositions,bool inPrimaryQuote,bool inSecondaryQuote,
		bool &nextVerbInSeries,int &sense,
		int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int begin,int end,int infpElement,vector <cTagLocation> &tagSet);
	void evaluateMultipleVerbs(vector <cTagLocation>& tagSet, int& whereLastVerb, int whereVerb,bool &isNot);
	void evaluateSubjects(int where, vector <cTagLocation>& tagSet,bool inPrimaryQuote, bool inSecondaryQuote, bool withinInfinitivePhrase,
		bool internalInfinitivePhrase, int whereVerb, vector <int>& whereSubjects,int tsSense,bool isId, bool &isNonPast, bool& objectAsSubject, bool &subjectIsPleonastic, bool& noObjects, vector <tIWMM>& subjectWords, vector <int>& subjectObjects,bool &backwardsSubjects);
	void bindFreePrepositions(int where,int firstFreePrep, int whereVerb, int whereHVerb);
	void processMultipleObjects(vector <cTagLocation>& tagSet, int& whereObject, const int tsSense, const int mnounTag, const int objectTag, vector <int>& whereMObjects);
	void processObjects(int where, vector <cTagLocation>& tagSet, int firstFreePrep, vector <int>& futureBoundPrepositions,
		bool inPrimaryQuote, bool inSecondaryQuote, bool withinInfinitivePhrase, bool& nextVerbInSeries, int& sense, int& whereLastVerb,
		bool& ambiguousSense, bool inQuotedString, bool inSectionHeader, int begin, int end,
		int& numObjects, const int whereHObject, int& whereObject, const int whereVerb, int& infpElement, const int tsSense,
		vector <int>& whereSubjects, vector <int>& subjectObjects, vector <tIWMM>& subjectWords,
		const bool isId, const bool isNonPast, const bool isNonPresent, const bool isNot, const bool subjectIsPleonastic, const bool objectAsSubject);
	void adjustAttachmentOfPrecedingRelativizer(int whereSubject);
	bool evaluateVerbRoleTags(int where, int& hverbTagIndex, int& verbTagIndex, int& whereHVerb, int& whereVerb, int& notTag, int& len, bool& nextVerbInSeries, int& sense, int& tsSense,
		int& whereLastVerb, int& begin, int& end, bool& isId, bool& isNot, bool& withinInfinitivePhrase, bool& isNonPast, bool& isNonPresent, bool& ambiguousSense, bool& inPrimaryQuote, bool& inQuotedString,
		bool& inSectionHeader, vector <cTagLocation>& tagSet, int& infpElement, int firstFreePrep, vector <int>& futureBoundPrepositions, bool inSecondaryQuote, int &whereIVerb);
	void overrideRelativeObject(int where,vector <cTagLocation>& tagSet, int whereVerb, int whereObject, const vector <int> whereSubjects,bool noObjects);
	void handleLeadingPreposition(const int where, const bool objectAsSubject, vector <int>& whereSubjects, const int whereVerb, int &whereObject);
	void setInfinitiveRelations(int whereVerb, int whereIVerb, const vector <int> whereSubjects);
	bool evaluateAdditionalRoleTags(int where, vector <cTagLocation>& tagSet, int len, int firstFreePrep, vector <int>& futureBoundPrepositions, bool inPrimaryQuote, bool inSecondaryQuote, bool& outsideQuoteTruth, bool& inQuoteTruth, bool withinInfinitivePhrase, bool internalInfinitivePhrase,
																	bool &nextVerbInSeries,int &sense,int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int begin,int end);
	int findObject(cTagLocation &tag,int &position);
	public: size_t startCollectTagsFromTag(bool inTrace,int tagSet,cTagLocation &tl,vector < vector <cTagLocation> > &tagSets,int rejectTag,bool obeyBlock, bool collectParentTags, lpwstring purpose);
	public: size_t startCollectTags(bool trace,int tagSet,int position,int PEMAPosition,vector < vector <cTagLocation> > &tagSets,bool obeyBlock,bool collectParentTags,lpwstring purpose);
	void sortTagLocations(vector < vector <cTagLocation> > &tagSets, vector <cTagLocation> &tagSetLocations);
	void evaluateNounDeterminersGNoun(const int nLen, int& nPEMAPosition, const int nPosition, const int p, int& traceSource, cPatternMatchArray::tPatternMatch* pma);
	int collectAndProcessNounDeterminerTags(const int nLen, int& nPEMAPosition, const int nPosition, const int p, int& traceSource, cPatternMatchArray::tPatternMatch* pma, lpwstring purpose);
	void collectAndProcessHerNonSeparableTags(const int nLen, int& nPEMAPosition, const int nPosition, const int p, int& traceSource, cPatternMatchArray::tPatternMatch* pma);
	void collectAndProcessNounDeterminerPattern(const int nLen, int& nPEMAPosition, const int nPosition, cPatternMatchArray::tPatternMatch* pma, lpwstring purpose);
	void evaluateNounDeterminers(int PEMAPosition,int position,vector < vector <cTagLocation> > &tagSets, bool alternateShortTry, lpwstring purpose);
	void evaluatePrepObjects(int PEMAPosition, int position, vector < vector <cTagLocation> > &tagSets, lpwstring purpose);
	int evaluatePrepObjectRelation(vector <cTagLocation> &tagSet,int &pIndex,tIWMM &prepWord,int &object,int &wherePrepObject,tIWMM &objectWord);
	void equivocateObjects(int where,int eTo,int eFrom);
	void assignMetaQueryAudience(int beginQuote,int previousQuote,int primaryObject,int secondaryObject,int secondaryTag,vector <cTagLocation> &tagSet);
	bool processMetaSpeakerQueryAnswer(int beginQuote,int previousQuote,int lastQuery);
	bool evaluateMetaSpeaker(int where,vector <cTagLocation> &tagSet);
	bool identifyMetaSpeaker(int where,bool inQuote);
	bool evaluateAnnounce(int where,vector <cTagLocation> &tagSet);
	bool identifyAnnounce(int where,bool inQuote);
	bool evaluateMetaGroup(int where,vector <cTagLocation> &tagSet,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	bool identifyMetaGroup(int where,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	bool identifyMetaNameEquivalence(int where,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);
	bool ageDetection(int where,int primary,int secondary);
	bool primaryIsMNoun(int where, int wherePrimary, cTagLocation& tag);
	void findLast(int where, int wherePrimary, int& tmpLastRelativePhrase, int& tmpLastBeginS1, int& tmpLastQ2, int& tmpLastVerb);
	void resolvePrimaryMetaNameObject(int where, int wherePrimary, int whereSecondary, int& tmpLastRelativePhrase, int lastRelativePhrase, int& tmpLastBeginS1, int lastBeginS1, int& tmpLastQ2, int lastQ2, int& tmpLastVerb, 
		bool inPrimaryQuote, bool inSecondaryQuote);
	bool findAndResolveSecondaryObjects(int& tmpLastRelativePhrase, int& tmpLastBeginS1, int& tmpLastQ2, int& tmpLastVerb, bool inPrimaryQuote, bool inSecondaryQuote, int secondaryTag, vector <cTagLocation>& tagSet, vector <int>& objectsResolved,
		vector <int>& secondaryNameObjects, vector <int>& eraseREObjects);
	void resolveSecondaryMetaNameObject(int whereSecondary,int primaryNameObject, int& tmpLastRelativePhrase, int& tmpLastBeginS1, int& tmpLastQ2, int& tmpLastVerb, bool inPrimaryQuote, bool inSecondaryQuote, vector <int>& objectsResolved,
		vector <int>& secondaryNameObjects, vector <int>& eraseREObjects);
	void insertSecondaryNameObjectInQuoteIntoSpeakers(int where, int wherePrimary, int whereSecondary, vector <int>& secondaryNameObjects, bool inPrimaryQuote);
	bool rejectSecondaryMetaNameEquivalence(int where, int sno, int wherePrimary, int whereSecondary, int primaryNameObject, vector <int>& objectsResolved, vector <int>& secondaryNameObjects, vector <int>& eraseREObjects);
	void switchToPreferPrimaryNameOrMetaGroup(int& wherePrimary, int& primaryNameObject, int& whereSecondary, int& secondaryNameObject);
	void removeHail(int where, int wherePrimary);
	void associateSecondaryAdjectivesAndGenderToPrimary(int where, int wherePrimary, int primaryNameObject, int whereSecondary, int secondaryNameObject);
	bool refuseIdentificationIfNotIdentifiedOrNotKnown(int where, int wherePrimary, int primaryNameObject, int whereSecondary, int secondaryNameObject);
	void removePlaceSetGenderAndReplacePrimaryWithSecondary(int where, bool inPrimaryQuote, bool inSecondaryQuote, int wherePrimary, int primaryNameObject, int whereSecondary, int secondaryNameObject);
	void equalizeGenderAdjectivesAndRelativeClauses(int where, int primaryNameObject, int secondaryNameObject);
	bool evaluateSecondaryMetaNameEquivalence(int where, vector <cTagLocation>& tagSet, bool inPrimaryQuote, bool inSecondaryQuote, int sno, int wherePrimary, int whereSecondary, int primaryNameObject, int secondaryTag, vector <int>& objectsResolved,
		vector <int>& secondaryNameObjects, vector <int>& eraseREObjects);
	bool evaluateMetaNameEquivalence(int where,vector <cTagLocation> &tagSet,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb);

	bool inObject(int where, int whereQuestionType);
	bool pushWhereEntities(lpchar_t *derivation,int where,lpwstring matchEntityType,lpwstring byWhatType,int whatWhere,bool filterNameDuplicates, vector <mbInfoReleaseType> mbs);
	bool pushEntities(lpchar_t *derivation,int where,lpwstring matchEntityType, vector <mbInfoReleaseType> &mbs);
	bool matchedList(set <lpwstring> &matchList, int where, int objectClass, const lpchar_t * fromWhere);
	void createObject(cObject object);
	cOM createObject(lpwstring derivation,lpwstring wordstr,OC objectClass);
	int createObject(lpwstring derivation, lpwstring descriptor);

	// MYSQL Database
	int createSentimentTables(void);
	int createObjectTables(void);
	int createRelationTables(void);
	int flushObjectRelations();
	int alreadyExists(char *word);
	int readMultiSourceObjects(tIWMM *wordMap,int numWords);
	int flushGroups(int sourceId);
	bool abbreviationEquivalent(tIWMM w,tIWMM w2);
	bool accumulateRelatedObjects(int object,set <int> &relatedObjects);
	void addAssociatedNounsFromTitle(int o);
	void addWordAbbreviationMap(tIWMM w,vector <tIWMM> &nouns);
	void buildMap(void);
	int rti(int where);
	const lpchar_t *wchr(int where);
	const lpchar_t *wrti(int where, const lpchar_t * id,lpwstring &tmpstr,bool shortFormat=false);
	bool acceptableAdjective(int where);
	bool acceptableObjectPosition(int where);
	const lpchar_t *getWSAdverb(int whereVerb,bool changeStateAdverb);
	lpwstring getWOSAdjective(int whereVerb,int where,lpwstring &tmpstr);
	lpwstring getWOSAdjective(int where,lpwstring &tmpstr);
	lpwstring getWSAdjective(int whereVerb,int where,int numOrder,lpwstring &tmpstr);
	lpwstring getWSAdjective(int where,int numOrder);
	int maxBackwards(int where);
	int getMinPosition(int where);
	int gmo(int wo);
	void getSRIMinMax(cSyntacticRelationGroup *srg);
	void prepPhraseToString(int wherePrep,lpwstring &ps);
	void insertCompoundObjects(int wo,set <int> &relPreps, unordered_map <int,int> &principalObjectEndPoints);
	void correctSRIEntry(cSyntacticRelationGroup &srg);
};

extern vector <cSource::cSpeakerGroup>::iterator sgNULL;

