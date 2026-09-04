/*
	word.h - global lexicon (cWord / Words), per-word info (cSourceWordInfo), and form catalog (cForm)

	Overview:
		The in-memory dictionary.  Words is an unordered_map<lpwstring,cSourceWordInfo>
		keyed by the surface form.  Each cSourceWordInfo holds the form-id list
		(indexes into the global Forms vector, stored in a process-wide formsArray),
		inflectionFlags (InflectionTypes bits), usagePatterns/usageCosts used by
		pattern costing, and an optional relationMaps[relationType] of
		cRMap::cRelation (from/to word co-occurrence counts, filled from
		wordRelationsMemory by DBWordRelations.cpp).

	Pipeline position:
		Loaded in stage 1 (cWord::readWordsFromDB / initialize) before tokenize.
		tokenize.cpp looks up or inserts every token via parseWord / addNewOrModify.
		Pattern costing reads usageCosts.  Relations and objects store tIWMM
		iterators into Words; those are serialized as the word string, not as a
		pointer (see source.h).

	Key entry points:
		- Words.query / gquery / fullQuery / parseWord / addNewOrModify - lookup/insert.
		- readWordsFromDB / initializeWordRelationsFromDB - hydrate from MySQL.
		- cSourceWordInfo::addForm / query / usage-cost helpers - form and cost API.
		- The predefined Words.PPN / NUM / DATE / TIME / LOCATION / TABLE / ...
			sentinels used as synthetic tokens.

	Key data structures / globals:
		- Words / WMM - the map.  tIWMM is its iterator; wNULL is "not found".
		- Forms - vector<cForm*>, form ids are indexes (UNDEFINED_FORM_NUM=0).
		- formsArray / allocated / fACount - one big int array of form ids; each
			word holds (formsOffset, count).  Must not move after init (no SRWLOCK).
		- DBNAME / LDBNAME - MySQL schema name "lp".
		- MAX_WORD_LENGTH 32 - matches words.word CHAR(32).
		- Role bit constants (SUBJECT_ROLE, OBJECT_ROLE, ... SENTENCE_IN_ALT_REL_ROLE)
			are uint64_t flags stored on objects/relations, not on words.
		- NET_ERR - negative codes shared by the stemmer, HTTP cache, and tokenizer.

	Notes / gotchas:
		- Form ids in the DB are 1-based; in memory they are 0-based (formId-1 on
			read).  patternFormNumOffset (32750) is the fake "form id" used to store
			usage-pattern counts in the same wordForms rows.
		- alreadyTaken = 8192*256 does not collide with deleteWordAfterSourceProcessing
			(=8192); the *256 is a shift by 8 extra bits.
		- A tIWMM is invalidated if WMM rehashes; callers that keep iterators across
			inserts (parse of a whole novel) rely on reserve() during DB load.
*/
#pragma warning (disable: 4503)
#pragma warning (disable: 4996)
#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
using namespace std;
#include "logging.h"
#include "mysql.h"
#include "general.h"
#include "relationTypes.h"
#include <stdio.h>
#include <signal.h> // batch B4a: sig_atomic_t, for the exitNow declaration below

extern const lpchar_t *cacheDir;
#define DBNAME "lp"
#define LDBNAME u"lp"
#define MAX_WORD_LENGTH 32

// STLPort 4.6.2 yields 3254 without ACCUMULATE, 5.0.2 yields 7539!
//#define ACCUMULATE_GROUPS 3686-3254
// without WORD_RELATIONS, yields 209 bytes/word
// with individually allocated relation maps, 402/word.
extern int64_t memoryAllocated;
// Batch B4a: written by main.cpp's SIGINT/SIGTERM/SIGHUP handler, hence
// volatile sig_atomic_t rather than bool (only that type is safe to touch from a
// signal handler). Still used purely as a boolean everywhere it is read.
extern volatile sig_atomic_t exitNow;

#define LOG_BUFFER_SIZE 65536
#define READLEN 128
#define UNDEFINED_FORM u"Undefined"
#define UNDEFINED_SHORT_FORM u"Undef"
#define UNDEFINED_FORM_NUM 0
#define SECTION_FORM u"Section"
#define SECTION_SHORT_FORM u"Sec"
#define SECTION_FORM_NUM 1
#define COMBINATION_FORM u"Combination"
#define COMBINATION_SHORT_FORM u"Comb"
#define COMBINATION_FORM_NUM 2
#define PROPER_NOUN_FORM u"Proper_Noun"
#define PROPER_NOUN_SHORT_FORM u"Prop"
#define PROPER_NOUN_FORM_NUM 3
#define NUMBER_FORM u"Number"
#define NUMBER_SHORT_FORM u"Num"
#define NUMBER_FORM_NUM 4
#define HONORIFIC_FORM_NUM 5

// different subobject roles
#define SENTENCE_IN_ALT_REL_ROLE ((uint64_t) IN_COMMAND_OBJECT_ROLE<<1)
#define IN_COMMAND_OBJECT_ROLE ((uint64_t) THINK_ENCLOSING_ROLE<<1)
#define THINK_ENCLOSING_ROLE ((uint64_t) NOT_ENCLOSING_ROLE<<1)
#define NOT_ENCLOSING_ROLE ((uint64_t) EXTENDED_ENCLOSING_ROLE<<1)
#define EXTENDED_ENCLOSING_ROLE ((uint64_t) NONPAST_ENCLOSING_ROLE<<1)
#define NONPAST_ENCLOSING_ROLE ((uint64_t) NONPRESENT_ENCLOSING_ROLE<<1)
#define NONPRESENT_ENCLOSING_ROLE ((uint64_t) POSSIBLE_ENCLOSING_ROLE<<1)
#define POSSIBLE_ENCLOSING_ROLE ((uint64_t) NO_PP_PREP_ROLE<<1)
#define NO_PP_PREP_ROLE ((uint64_t) IN_QUOTE_REFERRING_AUDIENCE_ROLE<<1)
#define IN_QUOTE_REFERRING_AUDIENCE_ROLE ((uint64_t) PP_OBJECT_ROLE<<1)
#define PP_OBJECT_ROLE ((uint64_t) EXTENDED_OBJECT_ROLE<<1)
#define EXTENDED_OBJECT_ROLE ((uint64_t) IN_EMBEDDED_STORY_OBJECT_ROLE<<1)
#define IN_EMBEDDED_STORY_OBJECT_ROLE ((uint64_t) IN_SECONDARY_QUOTE_ROLE<<1)
#define IN_SECONDARY_QUOTE_ROLE ((uint64_t) NOT_OBJECT_ROLE<<1)
#define NOT_OBJECT_ROLE ((uint64_t) IN_PRIMARY_QUOTE_ROLE<<1)
#define IN_PRIMARY_QUOTE_ROLE ((uint64_t) FOCUS_EVALUATED<<1)
#define FOCUS_EVALUATED ((uint64_t) DELAYED_RECEIVER_ROLE<<1)
#define DELAYED_RECEIVER_ROLE ((uint64_t) PRIMARY_SPEAKER_ROLE<<1)
#define PRIMARY_SPEAKER_ROLE ((uint64_t) SECONDARY_SPEAKER_ROLE<<1)
#define SECONDARY_SPEAKER_ROLE ((uint64_t) MNOUN_ROLE<<1)
#define MNOUN_ROLE ((uint64_t) POV_OBJECT_ROLE<<1)
#define POV_OBJECT_ROLE ((uint64_t) PASSIVE_SUBJECT_ROLE<<1)
#define PASSIVE_SUBJECT_ROLE ((uint64_t) SENTENCE_IN_REL_ROLE<<1)
#define SENTENCE_IN_REL_ROLE ((uint64_t) UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE<<1)
#define UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE ((uint64_t) IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE<<1)
#define IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE ((uint64_t) SUBJECT_PLEONASTIC_ROLE<<1) // mark speakers referring to themselves within quotes
#define SUBJECT_PLEONASTIC_ROLE ((uint64_t) NON_MOVEMENT_PREP_OBJECT_ROLE<<1)
#define NON_MOVEMENT_PREP_OBJECT_ROLE ((uint64_t) 262144)
#define MOVEMENT_PREP_OBJECT_ROLE 131072
#define PLACE_OBJECT_ROLE 65536
#define NONPRESENT_OBJECT_ROLE 32768
#define IS_ADJ_OBJECT_ROLE 16384
#define NO_ALT_RES_SPEAKER_ROLE 8192 // Jill asked.  ", Tom said.  " said Jill.  Ignore these as subjects for alternate resolution
#define ID_SENTENCE_TYPE 4096 // set at sentence beginning if containing an "IS" verb
#define NONPAST_OBJECT_ROLE 2048 // used to determine speaker status
#define IS_OBJECT_ROLE 1024 // used to determine speaker status
#define RE_OBJECT_ROLE 512
#define PREP_OBJECT_ROLE 256
#define IOBJECT_ROLE 128

#define HAIL_ROLE 64 // speakers referred to in a quote from someone else
#define MPLURAL_ROLE 32 // multiple subjects & objects
#define META_NAME_EQUIVALENCE 16
#define OBJECT_ROLE 8
#define SUBJECT_ROLE 4
#define SUBOBJECT_ROLE 2
#define NO_ROLE 0

#define unknownWD u"source\\lists\\unknownWords.Websters.txt"
#define unknownCD u"source\\lists\\unknownWords.Cambridge.txt"

#define NOUN_INFLECTIONS_MASK (SINGULAR|PLURAL|SINGULAR_OWNER|PLURAL_OWNER|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER|FIRST_PERSON|SECOND_PERSON|THIRD_PERSON)
#define VERB_INFLECTIONS_MASK (VERB_PAST|VERB_PAST_PARTICIPLE|VERB_PRESENT_PARTICIPLE|VERB_PRESENT_THIRD_SINGULAR|\
                               VERB_PRESENT_FIRST_SINGULAR|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL|VERB_PRESENT_PLURAL|\
                               VERB_PRESENT_SECOND_SINGULAR)
#define ADJECTIVE_INFLECTIONS_MASK (ADJECTIVE_NORMATIVE|ADJECTIVE_COMPARATIVE|ADJECTIVE_SUPERLATIVE)
#define ADVERB_INFLECTIONS_MASK (ADVERB_NORMATIVE|ADVERB_COMPARATIVE|ADVERB_SUPERLATIVE)
#define INFLECTIONS_MASK (OPEN_INFLECTION|CLOSE_INFLECTION)

unsigned int findTagSet(const lpchar_t *tagSet);

typedef struct {
  int num;
  const lpchar_t *name;
} tInflectionMap;

extern tInflectionMap nounInflectionMap[],verbInflectionMap[],adjectiveInflectionMap[],adverbInflectionMap[];
const lpchar_t *getInflectionName(int inflection,int form,lpwstring &temp);
const lpchar_t *getInflectionName(int inflection,tInflectionMap *map,lpwstring &temp);

const lpchar_t *getLastErrorMessage(lpwstring &out);

typedef struct
{
  lpchar_t word[64];
  int inflection;
} Inflections;

typedef struct
{
  lpchar_t word[64];
  int inflection;
  lpchar_t mainEntry[64];
} InflectionsRoot;

#include "intarray.h"

enum NET_ERR {
  TAKE_LAST_MATCH_BEGIN_NOT_FOUND=-1,TAKE_LAST_MATCH_END_NOT_FOUND=-2,
  INFLECTION_PROCESSING_FAILED=-3,PARSE_OPTION_FAILED=-4,GETPATH_INVALID_FILELENGTH1=-5,
  GETPATH_INVALID_FILELENGTH2=-6,GETPATH_CANNOT_OPEN_PATH=-7,GETPATH_GENERAL=-8,
  GETFORMS_CANNOT_OPEN_PATH=-9,GETFORMS_CANNOT_WRITE=-10,WORD_NOT_FOUND=-11,
  UNPARSABLE_PAGE=-12,

  PARSE_TRACE=-15,
  PARSE_EOF=-16,
  PARSE_END_WORD=-17,
  PARSE_END_SENTENCE=-18,
  PARSE_END_PARAGRAPH=-19,
  PARSE_END_SECTION=-20,
  PARSE_END_BOOK=-21,
  PARSE_DATE=-22,
  PARSE_TIME=-23,
  PARSE_NUM=-24,
  PARSE_ORD_NUM=-25,
  PARSE_ADVERB_NUM=-26,
  PARSE_PLURAL_NUM=-27,
  PARSE_TELEPHONE_NUMBER=-28,
	PARSE_MONEY_NUM=-29,

  WORD_NOT_FOUND_IN_FORMS=-30,FORMS_NUM_INFLECTIONS_NUM_DIFFERENT=-31,FORM_NOT_FOUND=-32,NO_FORMS_FOUND=-33,
  SUFFIX_HAS_NO_FORM=-34,SUFFIX_HAS_NO_INFLECTION=-35,
  SUFFIX_RULES_PARSE_ERROR=-36,PREFIX_RULES_PARSE_ERROR=-37,NO_PREFIX_RULES_FILE=-38,NO_SUFFIX_RULES_FILE=-39,
  NEXT_MATCH_BEGIN_NOT_FOUND=-40,NEXT_MATCH_END_NOT_FOUND=-41,BAD_CLD_DEFINITION=-42,
	PARSE_DUMP_LOCAL_OBJECTS=-43,PARSE_PATTERN=-44,

	PARSE_WEB_ADDRESS=-45
};

// in flags of cWordMatch
//enum formsAdjustmentFlags { flagNounOwner=(1<<31), flagAddProperNoun=(1<<30), flagOnlyConsiderProperNounForms=(1<<29),
//                            flagAllCaps=(1<<28), flagTopLevelPattern=(1<<27), flagPossiblePluralNounOwner=(1<<26),
//                            flagNotMatched=(1<<25)};

class cForm
{
public:
  lpwstring name;
  lpwstring shortName;
  lpwstring inflectionsClass;
  bool hasInflections;
  // classes that have this set: month, all place forms
  // this means that even though it is capitalized, and not seen uncapitalized, words having this form will not be considered
  // as ONLY a proper noun.  It will have a Proper_Noun as well as the form.
  bool properNounSubClass;
  bool isTopLevel;
	bool isIgnore;
	bool isCommonForm;
	bool isNonCachedForm;
	bool isVerbForm;
	bool isNounForm;
  // only honorific.  So if this word is capitalized, it will not be recogized as a Proper_Noun at all.
  bool blockProperNounRecognition;
	bool formCheck; // used only when checking dictionary entries
  int index; // to DB
	// Serialize name/shortName/inflectionsClass and the bool flags as shorts.
	// Returns false if any copy() ran out of buffer.  index is not written
	// (it is the Forms vector position on the read side).
	bool write(void *buffer,int &where,int limit)
	{
    if (!copy(buffer,name,where,limit)) return false;
    if (!copy(buffer,shortName,where,limit)) return false;
    if (!copy(buffer,inflectionsClass,where,limit)) return false;
    if (!copy(buffer,(short)hasInflections,where,limit)) return false;
    if (!copy(buffer,(short)properNounSubClass,where,limit)) return false;
    if (!copy(buffer,(short)isTopLevel,where,limit)) return false;
    if (!copy(buffer,(short)isIgnore,where,limit)) return false;
    if (!copy(buffer,(short)isVerbForm,where,limit)) return false;
    if (!copy(buffer,(short)blockProperNounRecognition,where,limit)) return false;
    if (!copy(buffer,(short)formCheck,where,limit)) return false;
		return true;
	}
	cForm(int indexIn,lpwstring nameIn,lpwstring shortNameIn,lpwstring inflectionsClassIn,bool hasInflectionsIn,
										 bool properNounSubClassIn=false,bool isTopLevelIn=false,bool isIgnoreIn=false,bool verbFormIn=false,bool blockProperNounRecognitionIn=false,bool formCheckI=false);

	//cForm(int indexIn,lpwstring newForm,lpwstring shortForm,bool setInflections,lpwstring inflectionsClassIn=u"",bool properNounSubClass,bool isTopLevel=false,bool blockProperNounRecognition=false);
};

extern vector <cForm *> Forms;
extern int commaForm,periodForm,reflexivePronounForm,nomForm,personalPronounAccusativeForm;
extern int nounForm,quoteForm,dashForm,bracketForm,conjunctionForm,demonstrativeDeterminerForm,possessiveDeterminerForm,interrogativeDeterminerForm;
extern int indefinitePronounForm,reciprocalPronounForm,pronounForm,numeralCardinalForm,numeralOrdinalForm,romanNumeralForm,adverbForm,adjectiveForm;
extern int verbForm,thinkForm,honorificForm,honorificAbbreviationForm,demonymForm,relativeForm,commonProfessionForm,businessForm,friendForm,internalStateForm;
extern int determinerForm,doesForm,doesNegationForm,possessivePronounForm,quantifierForm,dateForm,timeForm,telephoneNumberForm,coordinatorForm;
extern int verbverbForm,abbreviationForm,numberForm,beForm,haveForm,haveNegationForm,doForm,doNegationForm,interjectionForm,personalPronounForm,letterForm;
extern int isForm,isNegationForm,prepositionForm,telenumForm,sa_abbForm,toForm,relativizerForm,moneyForm,particleForm,webAddressForm,predeterminerForm;
extern int doForm,doNegationForm,monthForm,letterForm,modalAuxiliaryForm,futureModalAuxiliaryForm,negationModalAuxiliaryForm,negationFutureModalAuxiliaryForm;


class cForms
{
public:
  static int findForm(lpwstring form);
  static int gFindForm(lpwstring form);
  static int addNewForm(lpwstring sForm,lpwstring shortForm,bool message,bool properNounSubClass=false);
	static int createForm(lpwstring sForm,lpwstring shortName,bool inflectionsFlag,lpwstring inflectionsClass,bool properNounSubClass);
  static bool changedForms;
	static unordered_map <lpwstring ,int > formMap;
};

class cWord;
extern cWord Words;
extern tInflectionMap shortNounInflectionMap[];
extern tInflectionMap shortVerbInflectionMap[];
extern tInflectionMap shortAdjectiveInflectionMap[];
extern tInflectionMap shortAdverbInflectionMap[];
class cSourceWordInfo;
typedef unordered_map <lpwstring,cSourceWordInfo>::iterator tIWMM;
extern tIWMM wNULL;
lpchar_t *firstMatch(lpchar_t *buffer, const lpchar_t *beginString, const lpchar_t *endString);
char *firstMatch(char *buffer, const char *beginString, const char *endString);

class cSourceWordInfo
{
friend class cWord;
public:
  struct wordSetCompare
  {
    bool operator()(const tIWMM &lhs, const tIWMM &rhs) const
    {
      return lhs->first<rhs->first;
    }
  };
  class cRMap
  {
  public:
    class cRelation
    {
    public:
      int frequency;
      int deltaFrequency;
      //int index; // index into relations table in DB
      short sourceId; // index into sources
			int rlastWhere;
      // if fromDB is true, then this relation is being read from the database.
      // if not, it is generated from the text.
			// fromDB==true: frequency is the DB total and deltaFrequency stays 0
			// (already persisted).  fromDB==false: both frequency and deltaFrequency
			// start at iFrequency so a later flush can write only the delta.
			cRelation(short _sourceId,int _lastWhere,int iFrequency,bool fromDB)
      {
        if (fromDB)
        {
          deltaFrequency=0;
          frequency=iFrequency;
        }
        else
        {
          deltaFrequency=iFrequency;
          frequency=iFrequency;
        }
        //index=-1;
				sourceId=_sourceId;
				rlastWhere=_lastWhere;
      };
			// fromDB: replace frequency with count+deltaFrequency (DB is the new
			// baseline; in-memory delta is preserved).  Otherwise add count to both.
			void increaseCount(short _sourceId,int _lastWhere,int count,bool fromDB)
      {
        if (fromDB)
          frequency=count+deltaFrequency;
        else
        {
          frequency+=count;
          deltaFrequency+=count;
        }
				sourceId=_sourceId;
				rlastWhere=_lastWhere;
      }
      cRelation(void)
      {
        sourceId=0; // index into sources
        rlastWhere=0;
        deltaFrequency=frequency=0;
      };
    };
    struct wordMapCompare
    {
      bool operator()(const tIWMM &lhs, const tIWMM &rhs) const
      {
        return lhs->first<rhs->first;
      }
    };
    typedef unordered_map<lpwstring,cRelation>::iterator tIcRMap;
    typedef unordered_map<lpwstring,cRelation> tcRMap;
    tcRMap r;
    struct mapSequenceCompare
    {
      bool operator()(const tIcRMap &lhs, const tIcRMap &rhs) const
      {
        return lhs->second.frequency<rhs->second.frequency;
      }
    };
    //set <tIcRMap,mapSequenceCompare> bySequence; // constantly maintained map of most frequent relations - not currently used
		tIcRMap addRelation(int sourceId,int lastWhere,tIWMM toWord,bool &isNew,int count,bool fromDB);
		//tIcRMap addRelation(int sourceId,int lastWhere,tIWMM toWord,bool &isNew,int count,bool fromDB);
		// Drop every relation in this map.  The cRMap itself is not deleted
		// (cSourceWordInfo::clearRelationMaps does that).
		void clear()
		{
			r.clear();
		}
  };

  static const int MAX_FORM_USAGE_PATTERNS=8;
	// up to 32 bits
  enum eWordFlags { topLevelSeparator=1, ignoreFlag=2, queryOnLowerCase=4, queryOnAnyAppearance=8, updateMainInfo=32, updateMainEntry=64,
                    insertNewForms=128, isMainEntry=256, intersectionGroup=512, newWordFlag=2048, inSourceFlag=4096, 
										deleteWordAfterSourceProcessing=8192,
										alreadyTaken=8192*256, physicalObjectByWN=8192*512, notPhysicalObjectByWN=8192*1024,uncertainPhysicalObjectByWN=notPhysicalObjectByWN<<1,
										genericGenderIgnoreMatch=uncertainPhysicalObjectByWN<<1,prepMoveType=genericGenderIgnoreMatch<<1,
										genericAgeGender=prepMoveType<<1,stateVerb=genericAgeGender<<1,possibleStateVerb=stateVerb<<1,mainEntryErrorNoted=possibleStateVerb<<1,
										lastWordFlag=mainEntryErrorNoted<<1
  };
  static const int resetFlagsOnRead=updateMainInfo|insertNewForms|newWordFlag|inSourceFlag;
  static const int VERB_AFTER_VERB_COST_FLAG=65536;
  enum eUsagePatterns {
    TRANSFER_COUNT=MAX_FORM_USAGE_PATTERNS,
    SINGULAR_NOUN_HAS_DETERMINER,SINGULAR_NOUN_HAS_NO_DETERMINER,
    VERB_HAS_0_OBJECTS,VERB_HAS_1_OBJECTS,VERB_HAS_2_OBJECTS,
    LOWER_CASE_USAGE_PATTERN,PROPER_NOUN_USAGE_PATTERN,
    LAST_USAGE_PATTERN,
    MAX_USAGE_PATTERNS=16
  };
	lpwstring patternString(int p);
	static const int patternFormNumOffset=32750;
  static const int HIGHEST_COST_OF_INCORRECT_NOUN_DET_USAGE=4;
  static const int HIGHEST_COST_OF_INCORRECT_VERB_USAGE=4;
  static const int HIGHEST_COST_OF_INCORRECT_VERB_AFTER_VERB_USAGE=6;
  static const int COST_OF_INCORRECT_PROPER_NOUN=10;
  static const int COST_OF_INCORRECT_VERBAL_NOUN=10;
  unsigned int  inflectionFlags;
  int flags;
	int timeFlags;
  int index; // to DB
  int sourceId; // where the word came from
	int numProperNounUsageAsAdjective;
  int derivationRules;
  tIWMM mainEntry;
	vector <int> relatedSubTypes;
	vector <int> relatedSubTypeObjects;
	int localWordIsCapitalized;
	int localWordIsLowercase;
  void allocateMap(int relationType);
  cRMap *relationMaps[numRelationWOTypes];
  #ifdef ACCUMULATE_GROUPS
    vector <int> groupList[numRelationWOTypes];
  #endif
  int tmpMainEntryWordId; // stores main entry temporarily for DB routines
  bool changedSinceLastWordRelationFlush;
  bool operator==(cSourceWordInfo &other) const;

  // Pointer to this word's form-id slice inside the process-wide formsArray.
  // Valid for formsSize() ints.  Do not retain across a formsArray realloc.
  unsigned int *forms()
  {
    return formsArray+formsOffset;
  }
  void eraseForms(void);
  cForm *Form(unsigned int offset);
	int getFormNum(unsigned int offset);
  // Number of form ids in this word's forms() slice (not including the
  // extra "imposed proper noun" usageCosts[count] slot).
  unsigned int formsSize()
  {
    return count;
  }
	// Delete every non-NULL relationMaps[I] and null the slot.  Called before
	// initializeWordRelationsFromDB reloads the maps for words in this source.
	void clearRelationMaps()
	{
		for (int I=0; I<numRelationWOTypes; I++)
			if (relationMaps[I]) 
			{
				relationMaps[I]->clear();
				delete relationMaps[I];
				relationMaps[I] = NULL;
			}
	}
	// True (and LOG_ERROR) if this word's DB index or Form(f)->index is out of
	// range.  Used by the dictionary checker, not the parse hot path.
	bool illegal(int f,int maxForms,tIWMM word)
	{
		if (index<=0 || Form(f)->index<=0 || Form(f)->index>(signed)maxForms)
		{
      ::lplog(LOG_ERROR,u"Illegal index for word %s has index %d with form #%d having index %d (form offset %d)!",
          word->first.c_str(),index,f,Form(f)->index,forms()[f]);
			return true;
		}
		return false;
	}
	cSourceWordInfo(int iForm,int iInflectionFlags,int iFlags,int iTimeFlags,int derivationRules,tIWMM iMainEntry,int sourceId);
  cSourceWordInfo(char *buffer,int &where,int limit,lpwstring &ME,int sourceId);
  bool updateFromDisk(char *buffer,int &where,int limit,lpwstring &ME);
	void computeDBUsagePatternsToUsagePattern(unordered_map <int, int> &dbUsagePatterns);
	bool retrieveWordFromDatabase(lpwstring &sWord, MYSQL &mysql, cSourceWordInfo &dbWordInfo, unordered_map <int, int> &dbUsagePatterns,int &dbMainEntryWordId);
	bool write(void *buffer,int &where,int limit);
  // MYSQL database
  cSourceWordInfo(unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int mainEntryWordId,int iDerivationRules,int sourceId,int formNum,lpwstring &word);
  cSourceWordInfo(void);

  bool costEquivalentSubClass(int subclassForm,int parentForm);
	bool toLowestCostPreferForm(int form,int preferForm);
	bool toLowestCost(int form);
	bool setCost(int form,int cost);
  void mainEntryCheck(const lpwstring first,int where);
  void lplog(void);
  void transferFormsAndUsage(unsigned int *forms,unsigned int &iCount,int formNum,lpwstring &word);
  void transferDBUsagePatternsToUsagePattern(int highestCost,int *DBUsagePatterns,unsigned int upStart,unsigned int upLength);
  bool updateFromDB(int wordId,unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int mainEntryWordId,int iDerivationRules,int iSourceId,int formNum,lpwstring &word);
  bool isProperNounSubClass(void);
  bool blockProperNounRecognition(void);
  int query(int form);
	bool hasWinnerVerbForm(int winnerForms);
	bool hasVerbForm();
	bool hasWinnerNounForm(int winnerForms);
	bool hasNounForm();
	int query(lpwstring form);
  int lowestSeparatorCost();
    bool isLowestCost(int form);
  int queryForSeparator(void);
  bool remove(int form);
  bool remove(lpchar_t *formName);
  int addForm(int form,const lpwstring &word,bool illegal=false);
	void cloneForms(cSourceWordInfo fromWord);
  int adjustFormsInflections(lpwstring originalWord,uint64_t &flags,bool isFirstWord,int nounOwner,bool allCaps,bool firstLetterCapitalized, bool log);
  bool isUnknown(void);
  bool isCommonWord(void);
	bool isNonCachedWord(void);
	void setTopLevel(void);
  bool isSeparator(void);
	void removeIllegalForms(void);
	void setIgnore(void);
  bool isIgnore(void);
  bool isRareWord(void);
  void preferVerbPresentParticiple(void);
  bool notCostable(lpwstring word,int flags);
  void logFormUsageCosts(lpwstring w);
  void createGroup(relationWOTypes relationType,tIWMM toWord);
	cSourceWordInfo::cRMap::tIcRMap addRelation(int where,int rType,tIWMM word);
  bool intersect(relationWOTypes relationType,tIWMM word,tIWMM self,tIWMM &fromWord,tIWMM &toWord);
  void transferUsagePatternsToCosts(int highestCost,unsigned int upStart,unsigned int upLength);
  void transferFormUsagePatternsToCosts(int sameNameForm,int properNounForm,int iCount);
	void resetUsagePatternsAndCosts(lpwstring sWord);
	void logReset(lpwstring sWord);
	void resetCapitalizationAndProperNounUsageStatistics();
	// usageCosts[formIndex], or -1 if formIndex is out of 0..MAX_USAGE_PATTERNS.
	char getUsageCost(int formIndex) { return (formIndex>= MAX_USAGE_PATTERNS || formIndex<0) ? -1 : usageCosts[formIndex]; }
	char getUsagePattern(int formIndex) { return (formIndex >= MAX_USAGE_PATTERNS || formIndex<0) ? -1 : usagePatterns[formIndex]; }
	// controlled by the updateWordUsageCostsDynamically flag, currently globally set to false
	// Bump TRANSFER_COUNT (and its delta) and mark the word dirty for a
	// wordRelations flush.  Gated by updateWordUsageCostsDynamically (currently off).
	void incrementTransferCount()
	{
		deltaUsagePatterns[cSourceWordInfo::TRANSFER_COUNT]++;
		usagePatterns[cSourceWordInfo::TRANSFER_COUNT]++;
		changedSinceLastWordRelationFlush = true;
	}
	// True if bit 'form' is set in tmpWinnerForms, or if tmpWinnerForms is 0
	// (treat every form as a winner).  Forms past the width of an int are never winners.
	bool isWinner(int form,int tmpWinnerForms)
	{
		if (form >= sizeof(tmpWinnerForms) * 8)
		{
			return false;
		}
		return (tmpWinnerForms) ? ((1 << form)&tmpWinnerForms) != 0 : true;
	}
	// controlled by the updateWordUsageCostsDynamically flag, currently globally set to false
	// After a parse, add (count - numWinnerForms) to each winning form's usage
	// counter (halving first if any would exceed 255).  Same-name and Proper_Noun
	// forms are skipped.  Every 64 transfers, recomputes usageCosts and returns true.
	bool updateFormUsagePatterns(int tmpWinnerForms,lpwstring sWord)
	{
		int numWinnerForms = 0, sameNameForm = -1, properNounForm = -1;
		for (unsigned int f = 0; f < count; f++)
		{
			if (Form(f)->name == sWord)
			{
				sameNameForm = f;
				continue;
			}
			if (forms()[f] == PROPER_NOUN_FORM_NUM)
			{
				properNounForm = f;
				continue;
			}
			if (isWinner(f,tmpWinnerForms))
				numWinnerForms++;
		}
		// do not update usage pattern if proper noun has been imposed on the word because of capitalization and
		// this form won - numWinnerForms will be zero.
		int add = count - numWinnerForms;
		if (sameNameForm >= 0) add--;
		if (properNounForm >= 0) add--;
		if (!numWinnerForms || add <= 0)
		{
			deltaUsagePatterns[cSourceWordInfo::TRANSFER_COUNT]++;
			usagePatterns[cSourceWordInfo::TRANSFER_COUNT]++;
			changedSinceLastWordRelationFlush = true;
			return false;
		}
		// Batch B6a: the explicit cast is needed now that this is std::min rather
		// than the untyped windows.h min() macro -- count is unsigned int and
		// MAX_USAGE_PATTERNS is an eUsagePatterns enumerator, so template argument
		// deduction has two conflicting candidates for T.
		unsigned int topAllowableUsageCount = min(count, (unsigned int)cSourceWordInfo::MAX_USAGE_PATTERNS);
		bool reduce = false;
		for (unsigned int f = 0; f < topAllowableUsageCount; f++)
			if (f != sameNameForm && f != properNounForm)
				reduce |= ((add + usagePatterns[f]) > 255);
		if (reduce)
			for (unsigned int f = 0; f < topAllowableUsageCount; f++)
				if (f != sameNameForm && f != properNounForm)
					usagePatterns[f] >>= 1;
		bool reduceDelta = false;
		for (unsigned int f = 0; f < topAllowableUsageCount; f++)
			if (f != sameNameForm && f != properNounForm)
				reduceDelta |= ((add + deltaUsagePatterns[f]) > 255);
		if (reduceDelta)
			for (unsigned int f = 0; f < topAllowableUsageCount; f++)
				if (f != sameNameForm && f != properNounForm)
					deltaUsagePatterns[f] >>= 1;
		for (unsigned int f = 0; f < topAllowableUsageCount; f++)
		{
			if (f != sameNameForm && f != properNounForm && isWinner(f,tmpWinnerForms))
			{
				usagePatterns[f] += add;
				deltaUsagePatterns[f] += add;
			}
		}
		deltaUsagePatterns[cSourceWordInfo::TRANSFER_COUNT]++;
		changedSinceLastWordRelationFlush = true;
		if (usagePatterns[cSourceWordInfo::TRANSFER_COUNT]++ < 63)
			return false;
		transferFormUsagePatternsToCosts(sameNameForm, properNounForm, count);
		usagePatterns[cSourceWordInfo::TRANSFER_COUNT] = 1; // so writeUnknownWords knows this word is used more than once
		deltaUsagePatterns[cSourceWordInfo::TRANSFER_COUNT] = 1; // so writeUnknownWords knows this word is used more than once
		return true;

	}
	// If every slot in usages[start, start+len) is 255, right-shift them all so
	// the next increment does not wrap a unsigned char.
	void normalize(unsigned char *usages, int start, int len)
	{
		len += start;
		int I;
		for (I = start; I < len && usages[I] == 255; I++);
		if (I == len)
			for (int J = start; J < len; J++)
				usages[J] >>= 1;
	}
	// Clear the extra "imposed proper noun" cost slot (index == formsSize()).
	void zeroNewProperNounCostIfUsedAllCaps() { usageCosts[formsSize()] = 0; }
	// Copy the noun/abbreviation/sa_abb usageCost onto the extra "imposed
	// proper noun" slot at usageCosts[formsSize()], or 0 if none of those forms exist.
	void setProperNounUsageCost()
	{
		int costingOffset;
		if ((costingOffset = query(nounForm)) != -1 ||
			(costingOffset = query(abbreviationForm)) != -1 ||
			(costingOffset = query(sa_abbForm)) != -1)
			usageCosts[formsSize()] = usageCosts[costingOffset];
		else
			usageCosts[formsSize()] = 0;
	}
	// Count one SINGULAR_NOUN_HAS_[NO_]DETERMINER observation and, every 16
	// observations, fold the pair into usageCosts.
	void updateNounDeterminerUsageCost(bool hasDeterminer)
	{
		normalize(usagePatterns, cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER, 2);
		normalize(deltaUsagePatterns, cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER, 2);
		usagePatterns[(hasDeterminer) ? cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER : cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER]++;
		deltaUsagePatterns[(hasDeterminer) ? cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER : cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER]++;
		int transferTotal = 0;
		for (unsigned int I = cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER; I < cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER + 2; I++)
			transferTotal += usagePatterns[I];
		if ((transferTotal & 15) == 15)
			transferUsagePatternsToCosts(cSourceWordInfo::HIGHEST_COST_OF_INCORRECT_NOUN_DET_USAGE, cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER, 2);
	}
	// Count one VERB_HAS_0/1/2_OBJECTS observation.  numObjects is used as an
	// index addend - values outside 0..2 write past the three-slot window.
	void updateVerbObjectsUsageCost(int numObjects)
	{
		usagePatterns[cSourceWordInfo::VERB_HAS_0_OBJECTS + numObjects]++;
		deltaUsagePatterns[cSourceWordInfo::VERB_HAS_0_OBJECTS + numObjects]++;
		int transferTotal = 0;
		for (unsigned int I = cSourceWordInfo::VERB_HAS_0_OBJECTS; I < cSourceWordInfo::VERB_HAS_0_OBJECTS + 3; I++)
			transferTotal += usagePatterns[I];
		if ((transferTotal & 15) == 15)
			transferUsagePatternsToCosts(cSourceWordInfo::HIGHEST_COST_OF_INCORRECT_VERB_USAGE, cSourceWordInfo::VERB_HAS_0_OBJECTS, 3);
	}
	// Minimum usageCosts[f] among forms whose cForm::isTopLevel is set, or 10000
	// if the word has no top-level form.
	int getLowestTopLevelCost(void)
	{
			int lowestCost = 10000;
		for (unsigned int f = 0; f < count; f++)
			if (Form(f)->isTopLevel)
				lowestCost = min(lowestCost, (int)usageCosts[f]); // batch B6a: int vs unsigned char, see above
		return lowestCost;
	}

	// Index of the lowest-cost form, breaking ties by preferring a non-Proper_Noun.
	// Returns -1 if count==0.
	int getLowestCost(void)
	{
			int lowestCost = 10000, offset = -1;
		for (unsigned int f = 0; f < count; f++)
			if (lowestCost > usageCosts[f] || (lowestCost == usageCosts[f] && forms()[f] != PROPER_NOUN_FORM_NUM))
			{
				lowestCost = usageCosts[f];
				offset = f;
			}
		return offset;
	}


	int scanAllRelations(tIWMM verbWord);
	unsigned int wordFrequency;
protected:
  static unsigned int *formsArray; // must not change after initialization or this must be protected by SRWLock
  static unsigned int allocated; // must not change after initialization or this must be protected by SRWLock
  static unsigned int fACount; // must not change after initialization or this must be protected by SRWLock
  int formsOffset;
private:
  unsigned int count;
  static int uniqueNewIndex; // use to insure every word has a unique index, even though it hasn't been consigned to the database yet.  
	unsigned char usagePatterns[cSourceWordInfo::MAX_USAGE_PATTERNS]; // usage counts for every class of this word
	unsigned char usageCosts[cSourceWordInfo::MAX_USAGE_PATTERNS];
	unsigned char deltaUsagePatterns[cSourceWordInfo::MAX_USAGE_PATTERNS];
	// must not change after initialization or this must be protected by SRWLock 
};

extern cSourceWordInfo::cRMap::tIcRMap tNULL;

bool loosesort( const lpchar_t *s1, const lpchar_t *s2 );

bool equivalentIfIgnoreDashSpaceCase(lpwstring sWord,lpwstring word2);
void removeDots(lpwstring &str);
int takeLastMatch(lpwstring &buffer,lpwstring begin_string,lpwstring end_string,lpwstring &match,bool include_begin_and_end);
size_t firstMatch(lpwstring &buffer,lpwstring begin_string,lpwstring end_string,size_t &beginPos,lpwstring &match,bool include_begin_and_end);
int firstMatchNonEmbedded(lpwstring &buffer,lpwstring beginString,lpwstring endString,size_t &beginPos,lpwstring &match,bool include_begin_and_end);
int getInflection(lpwstring sWord,lpwstring form,lpwstring mainEntry,lpwstring iform,vector <lpwstring> &allInflections);
int nextMatch(lpwstring &buffer,lpwstring begin_string,lpwstring end_string,size_t &begin_pos,lpwstring &match,bool include_begin_and_end);
int getPath(const lpchar_t *pathname,void *buffer,int maxlen,int &actualLen);

class cWord
{
  friend class cSourceWordInfo;

public:
  cWord(void);
  void findPredefinedVerb();
  void findPredefinedPronoun();
  void findPredefinedDeterminer();
  void findPredefinedForms();
  void adjustUsages();
  void initializeCosts();
  void initialize();
	void initializeChangeStateVerbs();
  ~cWord();
  tIWMM sectionWord; // special word only for section breaks
  tIWMM PPN; // special word only for personal/gendered proper nouns (relations)
  tIWMM TELENUM; // special word only for telephone numbers
  tIWMM NUM; // special word only for numbers (relations)
  tIWMM DATE; // special word only for dates (relations)
  tIWMM TIME; // special word only for times (relations)
	tIWMM LOCATION; // special word only for locations
	tIWMM TABLE; // used to start the table section which is extracted from <table> and table-like constructions in HTML
	tIWMM END_COLUMN; // used to end each column string which is extracted from <table> and table-like constructions in HTML
	tIWMM END_COLUMN_HEADERS; // used to end the column header section in the table section which is extracted from <table> and table-like constructions in HTML
	tIWMM TOC_HEADER;
	tIWMM MISSING_COLUMN; // used to mark missing columns in the table section which is extracted from <table> and table-like constructions in HTML
  static tIWMM begin(void)
  {
    return WMM.begin();
  }
  // Past-the-end iterator of WMM.  Compare to this, never dereference it.
  // wNULL is a separate "not found" sentinel, not equal to end().
  static tIWMM end(void)
  {
    return WMM.end();
  }
	static unordered_map <lpwstring, vector <tIWMM>> mainEntryMap;
  static tIWMM query(lpwstring sWord);
  tIWMM gquery(lpwstring sWord);
  bool parseMetaCommands(int where,lpchar_t *buffer,int &endSymbol, lpwstring &comment, sTrace &t);
  int ignoreSpecialContext(lpchar_t* buffer, int64_t bufferLen, int64_t& bufferScanLocation,
    lpwstring& sWord, lpwstring& comment, int& nounOwner, bool scanForSection, bool webScrapeParse, sTrace& t, MYSQL* mysql, int sourceId, int sourceType, int64_t& cp);
  int readContraction(lpchar_t* buffer, int64_t bufferLen, int64_t& bufferScanLocation, lpwstring& sWord, int64_t& cp);
  int readSpecialWords(lpchar_t* buffer, int64_t bufferLen, int64_t& bufferScanLocation, lpwstring& sWord, int nounOwner, int64_t& cp);
  int readCharacters(lpchar_t* buffer, int64_t bufferLen, lpwstring& sWord, int64_t& cp);
  int readDanglingDash(lpchar_t* buffer, int64_t bufferLen, lpwstring& sWord, MYSQL* mysql, int sourceId, int64_t& cp);
  void readOwnership(lpchar_t* buffer, int64_t bufferLen, lpwstring& sWord, int& nounOwner, int64_t& cp);
  void readNTContraction(lpchar_t* buffer, int64_t bufferLen, lpwstring& sWord, int numChars, int64_t& cp);
  void readDigits(lpwstring& sWord, MYSQL* mysql, int sourceId, int64_t& cp);
  int readWord(lpchar_t *buffer,int64_t bufferLen,int64_t &bufferScanLocation,lpwstring &sWord, lpwstring &comment, int &nounOwner,bool scanForSection,bool webScrapeParse,sTrace &t,MYSQL *mysql,int sourceId, int sourceType);
  int processFootnote(lpchar_t *buffer,int64_t bufferLen,int64_t &cp);
  void readContraction(lpchar_t* buffer, int64_t bufferLen, lpwstring& sWord, int numChars, int64_t& cp);
  int readSpecialWords(lpchar_t* buffer, int64_t bufferLen, int64_t& bufferScanLocation, lpwstring& sWord, int64_t& cp);
	int parseWord(MYSQL *mysql, lpwstring sWord, tIWMM &iWord, bool log);
	static int attemptDisInclination(MYSQL *mysql, tIWMM &iWord, lpwstring sWord, int sourceId,bool log);
	static int parseWord(MYSQL *mysql, lpwstring sWord, tIWMM &iWord, bool firstLetterCapitalized, int nounOwner, int sourceId, bool log);
  static tIWMM addNewOrModify(MYSQL *mysql,lpwstring sWord,int flags,int form,int inflection,int derivationRules,lpwstring mainEntry,int sourceId,bool &added,bool markUndefined=false); // only used for adding a name
  // generic utilities
	static bool illegalWord(MYSQL *mysql, lpwstring sWord);
	bool isAllUpper(lpwstring &sWord);
  bool remove(lpwstring sWord);
	static void resetUsagePatternsAndCosts(sTrace debugTrace);
	static void resetCapitalizationAndProperNounUsageStatistics(sTrace debugTrace);
	int readFormsCache(char *buffer,int bufferlen,int &numReadForms);
  int readWords(lpwstring oPath,int sourceId, bool disqualifyWords, lpwstring specialExtension);
	int writeFormsCache(int fd);
  bool removeInflectionFlag(lpwstring sWord,int flag);
	static bool isDash(lpchar_t ch);
	static bool isSingleQuote(lpchar_t ch);
	static bool isDoubleQuote(lpchar_t ch);

  // MYSQL database
  int lastReadfromDBTime;
  void readForms(MYSQL &mysql, lpchar_t *qt);
	void mapWordIdToWordStructure(int wordId, tIWMM iWord);
	tIWMM wordStructureGivenWordIdExists(int wordId);
  bool acquireLock(MYSQL &mysql,bool persistent);
  void releaseLock(MYSQL &mysql);

  // Current lexicon size (WMM.size()), including sentinels and unknowns.
  size_t numWords(void) { return WMM.size(); }
  // if word is a new word discovered since last flush, the index in cSourceWordInfo will be -1.
	bool readWordsOfMultiWordObjects(vector < vector < tmWS > > &multiWordStrings,vector < vector < vector <tIWMM> > > &multiWordObjects);
	void addMultiWordObjects(vector < vector < tmWS > > &multiWordStrings,vector < vector < vector <tIWMM> > > &multiWordObjects);
  int wordCheck(void);
  void createHonorificWordCategories();
  void createAbbreviationWordCategories();
  void createPronounCategories();
  void createVerbAssociatedCategories();
  void createAdverbCategories();
  void createNounCategories();
  void createNumberCategories();
  void createLetterCategory();
  void createNonLetterWordCategories();
  void createAdjectiveCategories();
  void createPrepositionCategories();
  void createInterjectionCategory();
  void createRelativizerCategory();
  void defineDashedPrefixes();
  int createWordCategories();

  void addTimeFlag(int flag,Inflections words[]);
  void addTimeFlag(int flag, const lpchar_t *words[]);
  void usageCostToNoun(Inflections words[], const lpchar_t * nounSubclass);
	void toLowestUsageCost(Inflections words[], const lpchar_t * formClass);
  void usageCostToNoun(const lpchar_t * words[], const lpchar_t * nounSubclass);
  // the following are only used temporarily in BNCC and should be moved back to private usage
  int predefineVerbsFromFile(lpwstring form,lpwstring shortForm, const lpchar_t * path,int flags);
  int predefineWords(Inflections words[],lpwstring form,lpwstring shortForm,lpwstring inflectionsClass=u"",int flags=0,bool properNounSubClass=false);
  int predefineWords(const lpchar_t *words[],lpwstring form,lpwstring shortForm,int flags=0,bool properNounSubClass=false);
  static tIWMM predefineWord(const lpchar_t *word,int flags=0);
	void extendedParseHolidays();
	int predefineHolidays();
	static int processTime(lpwstring sWord, char &hour, char &minute);
	static int processDate(lpwstring sWord, short &year, char &month, char &dayOfMonth);
	static int splitWord(MYSQL *mysql, tIWMM &iWord, lpwstring sWord, int sourceId,bool log);
	static tIWMM fullQuery(MYSQL *mysql, lpwstring word, int sourceId);
	static int writeWord(tIWMM iWord, void *buffer, int &where, int limit);
	int readWordsFromDB(MYSQL &mysql, bool generateFormStatistics, bool printProgress, bool skipWordInitialization);
	int initializeWordRelationsFromDB(MYSQL mysql, set <int> wordIds, bool inSourceFlagSet,bool log);

protected:
  bool appendToUnknownWordsMode;
  // After an insertion into formsArray at 'formsOffset', bump every word whose
  // slice starts at or after that index.  O(Words) - only used while building
  // the dictionary, before parse.
  void moveFormOffsets(int formsOffset)
  {
    for (tIWMM I=WMM.begin(),WMMEnd=WMM.end(); I!=WMMEnd; I++)
      if (I->second.formsOffset>=formsOffset)
        I->second.formsOffset++;
  }

private:
	tIWMM *idToMap;
	int idsAllocated;
  typedef pair <lpwstring, cSourceWordInfo> tWFIMap;
  static int lastWordWrittenClock;
	bool isWordFormCacheValid(MYSQL &mysql);
	int readWordFormsFromDB(MYSQL &mysql,int maxWordId,lpchar_t *qt,int *words,int *counts,int &numWordForms,unsigned int * &wordForms, bool printProgress, bool skipWordInitialization, lpwstring specialExtension);
	int initializeWordsFromDB(MYSQL &mysql, int *words, int *counts, unsigned int * &wordForms, int &numWordsInserted, int &numWordsModified, bool printProgress);
  int lastModifiedTime;
  int minimumLastWordWrittenClockDiff;
  static bool changedWords;
  static bool inCreateDictionaryPhase;
  vector <lpwstring> unknownWDWords;
  vector <lpwstring> unknownCDWords;
  void readUnknownWords(lpchar_t *fileName,vector <lpwstring> &unknownWords);
  void writeUnknownWords(lpchar_t *fileName,vector <lpwstring> &unknownWords);
  int write(void);

  // initialized
  static vector <lpchar_t *> multiElementWords;
  static vector <lpchar_t *> quotedWords;
  static vector <lpchar_t *> periodWords;
  static unordered_map <lpwstring, cSourceWordInfo> WMM;
  // filled during program execution
	static int disinclinationRecursionCount;

  bool evaluateIncludedSingleQuote(lpchar_t *buffer,int64_t cp,int64_t begincp);
  int addGenderedNouns(const lpchar_t *genPath,int inflectionFlags,int wordForm);
	int addDemonyms(const lpchar_t * demPath);
	bool readVerbClasses(void);
	bool readVerbClassNames(void);
  bool addPlaces(lpwstring pPath,vector <tmWS > &objects);
	void addTimeFlags();
  void createTimeCategories(bool normalize);
  static int getForms(MYSQL *mysql,tIWMM &iWord,lpwstring sWord,int sourceId, bool logEverything);
  tIWMM addCopy(lpwstring sWord,tIWMM iWord,bool &added);
  static tIWMM query(lpwstring sWord,int form,int inflection,int &offset);
  bool addInflectionFlag(lpwstring sWord,int flag);
  static tIWMM hasFormInflection(tIWMM iWord,lpwstring sForm,int inflection);
  static bool handleExtendedParseWords(const lpchar_t * word);
  int continueParse(lpchar_t *buffer,int64_t begincp,int64_t bufferLen,vector<lpchar_t *> &multiWords);
  static int addWordToForm(lpwstring sWord,tIWMM &iWord,int flags,lpwstring sForm,lpwstring shortForm,int inflection,int derivationRules,lpwstring mainEntry,int sourceId,bool &added,bool markUndefined=false);
  int predefineWords(InflectionsRoot words[],lpwstring form,lpwstring shortForm,lpwstring inflectionsClass=u"",int flags=0,bool properNounSubClass=false);
  bool closeConnection(void);
  static int checkAdd(const lpchar_t * fromWhere,tIWMM &iWord,lpwstring sWord,int flags,lpwstring sForm,int inflection,int derivationRules,lpwstring mainEntry,int sourceId,bool log);

  #ifdef CHECK_WORD_CACHE
    // test routines
    int checkWord(cWord &Words2,tIWMM originalIWord,tIWMM newWord,int ret);
  #endif

  int addProperNamesFile(lpwstring path);
  void addNickNames(const lpchar_t * filePath);
	static int markWordUndefined(tIWMM &iWord,lpwstring sWord,int flags,bool firstWordCapitalized,int nounOwner,int sourceId);
  void generateFormStatistics(void);
  int processDate(lpwstring &sWord, lpchar_t *buffer,int64_t &cp,int64_t &bufferScanLocation);
	int processTime(lpwstring &sWord, lpchar_t *buffer, int64_t &cp, int64_t &bufferScanLocation);
	int processWebAddress(lpwstring &sWord, lpchar_t *buffer, int64_t &cp, int64_t bufferLen);
	static bool findWordInDB(MYSQL *mysql, lpwstring &sWord, int &wordId, tIWMM &iWord);
};

#include "pattern.h"

class cWordMatch;

#include "patternElementMatchArray.h"
#include "patternMatchArray.h"

extern const lpchar_t *OCSubTypeStrings[];
