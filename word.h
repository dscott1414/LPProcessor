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

extern const wchar_t *cacheDir;
#define DBNAME "lp"
#define LDBNAME L"lp"
#define MAX_WORD_LENGTH 32

// STLPort 4.6.2 yields 3254 without ACCUMULATE, 5.0.2 yields 7539!
//#define ACCUMULATE_GROUPS 3686-3254
// without WORD_RELATIONS, yields 209 bytes/word
// with individually allocated relation maps, 402/word.
extern __int64 memoryAllocated;
extern bool exitNow;

#define LOG_BUFFER_SIZE 65536
#define READLEN 128
#define UNDEFINED_FORM L"Undefined"
#define UNDEFINED_SHORT_FORM L"Undef"
#define UNDEFINED_FORM_NUM 0
#define SECTION_FORM L"Section"
#define SECTION_SHORT_FORM L"Sec"
#define SECTION_FORM_NUM 1
#define COMBINATION_FORM L"Combination"
#define COMBINATION_SHORT_FORM L"Comb"
#define COMBINATION_FORM_NUM 2
#define PROPER_NOUN_FORM L"Proper_Noun"
#define PROPER_NOUN_SHORT_FORM L"Prop"
#define PROPER_NOUN_FORM_NUM 3
#define NUMBER_FORM L"Number"
#define NUMBER_SHORT_FORM L"Num"
#define NUMBER_FORM_NUM 4
#define HONORIFIC_FORM_NUM 5

// different subobject roles
#define SENTENCE_IN_ALT_REL_ROLE ((unsigned __int64) IN_COMMAND_OBJECT_ROLE<<1)
#define IN_COMMAND_OBJECT_ROLE ((unsigned __int64) THINK_ENCLOSING_ROLE<<1)
#define THINK_ENCLOSING_ROLE ((unsigned __int64) NOT_ENCLOSING_ROLE<<1)
#define NOT_ENCLOSING_ROLE ((unsigned __int64) EXTENDED_ENCLOSING_ROLE<<1)
#define EXTENDED_ENCLOSING_ROLE ((unsigned __int64) NONPAST_ENCLOSING_ROLE<<1)
#define NONPAST_ENCLOSING_ROLE ((unsigned __int64) NONPRESENT_ENCLOSING_ROLE<<1)
#define NONPRESENT_ENCLOSING_ROLE ((unsigned __int64) POSSIBLE_ENCLOSING_ROLE<<1)
#define POSSIBLE_ENCLOSING_ROLE ((unsigned __int64) NO_PP_PREP_ROLE<<1)
#define NO_PP_PREP_ROLE ((unsigned __int64) IN_QUOTE_REFERRING_AUDIENCE_ROLE<<1)
#define IN_QUOTE_REFERRING_AUDIENCE_ROLE ((unsigned __int64) PP_OBJECT_ROLE<<1)
#define PP_OBJECT_ROLE ((unsigned __int64) EXTENDED_OBJECT_ROLE<<1)
#define EXTENDED_OBJECT_ROLE ((unsigned __int64) IN_EMBEDDED_STORY_OBJECT_ROLE<<1)
#define IN_EMBEDDED_STORY_OBJECT_ROLE ((unsigned __int64) IN_SECONDARY_QUOTE_ROLE<<1)
#define IN_SECONDARY_QUOTE_ROLE ((unsigned __int64) NOT_OBJECT_ROLE<<1)
#define NOT_OBJECT_ROLE ((unsigned __int64) IN_PRIMARY_QUOTE_ROLE<<1)
#define IN_PRIMARY_QUOTE_ROLE ((unsigned __int64) FOCUS_EVALUATED<<1)
#define FOCUS_EVALUATED ((unsigned __int64) DELAYED_RECEIVER_ROLE<<1)
#define DELAYED_RECEIVER_ROLE ((unsigned __int64) PRIMARY_SPEAKER_ROLE<<1)
#define PRIMARY_SPEAKER_ROLE ((unsigned __int64) SECONDARY_SPEAKER_ROLE<<1)
#define SECONDARY_SPEAKER_ROLE ((unsigned __int64) MNOUN_ROLE<<1)
#define MNOUN_ROLE ((unsigned __int64) POV_OBJECT_ROLE<<1)
#define POV_OBJECT_ROLE ((unsigned __int64) PASSIVE_SUBJECT_ROLE<<1)
#define PASSIVE_SUBJECT_ROLE ((unsigned __int64) SENTENCE_IN_REL_ROLE<<1)
#define SENTENCE_IN_REL_ROLE ((unsigned __int64) UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE<<1)
#define UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE ((unsigned __int64) IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE<<1)
#define IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE ((unsigned __int64) SUBJECT_PLEONASTIC_ROLE<<1) // mark speakers referring to themselves within quotes
#define SUBJECT_PLEONASTIC_ROLE ((unsigned __int64) NON_MOVEMENT_PREP_OBJECT_ROLE<<1)
#define NON_MOVEMENT_PREP_OBJECT_ROLE ((unsigned __int64) 262144)
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

#define unknownWD L"source\\lists\\unknownWords.Websters.txt"
#define unknownCD L"source\\lists\\unknownWords.Cambridge.txt"

#define NOUN_INFLECTIONS_MASK (SINGULAR|PLURAL|SINGULAR_OWNER|PLURAL_OWNER|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER|FIRST_PERSON|SECOND_PERSON|THIRD_PERSON)
#define VERB_INFLECTIONS_MASK (VERB_PAST|VERB_PAST_PARTICIPLE|VERB_PRESENT_PARTICIPLE|VERB_PRESENT_THIRD_SINGULAR|\
                               VERB_PRESENT_FIRST_SINGULAR|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL|VERB_PRESENT_PLURAL|\
                               VERB_PRESENT_SECOND_SINGULAR)
#define ADJECTIVE_INFLECTIONS_MASK (ADJECTIVE_NORMATIVE|ADJECTIVE_COMPARATIVE|ADVERB_SUPERLATIVE)
#define ADVERB_INFLECTIONS_MASK (ADVERB_NORMATIVE|ADVERB_COMPARATIVE|ADVERB_SUPERLATIVE)
#define INFLECTIONS_MASK (OPEN_INFLECTION|CLOSE_INFLECTION)

unsigned int findTagSet(const wchar_t *tagSet);

typedef struct {
  int num;
  const wchar_t *name;
} tInflectionMap;

extern tInflectionMap nounInflectionMap[],verbInflectionMap[],adjectiveInflectionMap[],adverbInflectionMap[];
const wchar_t *getInflectionName(int inflection,int form,wstring &temp);
const wchar_t *getInflectionName(int inflection,tInflectionMap *map,wstring &temp);

const wchar_t *getLastErrorMessage(wstring &out);

typedef struct
{
  wchar_t word[64];
  int inflection;
} Inflections;

typedef struct
{
  wchar_t word[64];
  int inflection;
  wchar_t mainEntry[64];
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
  wstring name;
  wstring shortName;
  wstring inflectionsClass;
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
	cForm(int indexIn,wstring nameIn,wstring shortNameIn,wstring inflectionsClassIn,bool hasInflectionsIn,
										 bool properNounSubClassIn=false,bool isTopLevelIn=false,bool isIgnoreIn=false,bool verbFormIn=false,bool blockProperNounRecognitionIn=false,bool formCheckI=false);

	//cForm(int indexIn,wstring newForm,wstring shortForm,bool setInflections,wstring inflectionsClassIn=L"",bool properNounSubClass,bool isTopLevel=false,bool blockProperNounRecognition=false);
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
  static int findForm(wstring form);
  static int gFindForm(wstring form);
  static int addNewForm(wstring sForm,wstring shortForm,bool message,bool properNounSubClass=false);
	static int createForm(wstring sForm,wstring shortName,bool inflectionsFlag,wstring inflectionsClass,bool properNounSubClass);
  static bool changedForms;
	static unordered_map <wstring ,int > formMap;
};

class cWord;
extern cWord Words;
extern tInflectionMap shortNounInflectionMap[];
extern tInflectionMap shortVerbInflectionMap[];
extern tInflectionMap shortAdjectiveInflectionMap[];
extern tInflectionMap shortAdverbInflectionMap[];
class cSourceWordInfo;
typedef unordered_map <wstring,cSourceWordInfo>::iterator tIWMM;
extern tIWMM wNULL;
wchar_t *firstMatch(wchar_t *buffer, const wchar_t *beginString, const wchar_t *endString);
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
    typedef unordered_map<wstring,cRelation>::iterator tIcRMap;
    typedef unordered_map<wstring,cRelation> tcRMap;
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
	wstring patternString(int p);
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

  unsigned int *forms()
  {
    return formsArray+formsOffset;
  }
  void eraseForms(void);
  cForm *Form(unsigned int offset);
	int getFormNum(unsigned int offset);
  unsigned int formsSize()
  {
    return count;
  }
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
	bool illegal(int f,int maxForms,tIWMM word)
	{
		if (index<=0 || Form(f)->index<=0 || Form(f)->index>(signed)maxForms)
		{
      ::lplog(LOG_ERROR,L"Illegal index for word %s has index %d with form #%d having index %d (form offset %d)!",
          word->first.c_str(),index,f,Form(f)->index,forms()[f]);
			return true;
		}
		return false;
	}
	cSourceWordInfo(int iForm,int iInflectionFlags,int iFlags,int iTimeFlags,int derivationRules,tIWMM iMainEntry,int sourceId);
  cSourceWordInfo(char *buffer,int &where,int limit,wstring &ME,int sourceId);
  bool updateFromDisk(char *buffer,int &where,int limit,wstring &ME);
	void computeDBUsagePatternsToUsagePattern(unordered_map <int, int> &dbUsagePatterns);
	bool retrieveWordFromDatabase(wstring &sWord, MYSQL &mysql, cSourceWordInfo &dbWordInfo, unordered_map <int, int> &dbUsagePatterns,int &dbMainEntryWordId);
	bool write(void *buffer,int &where,int limit);
  // MYSQL database
  cSourceWordInfo(unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int mainEntryWordId,int iDerivationRules,int sourceId,int formNum,wstring &word);
  cSourceWordInfo(void);

  bool costEquivalentSubClass(int subclassForm,int parentForm);
	bool toLowestCostPreferForm(int form,int preferForm);
	bool toLowestCost(int form);
	bool setCost(int form,int cost);
  void mainEntryCheck(const wstring first,int where);
  void lplog(void);
  void transferFormsAndUsage(unsigned int *forms,unsigned int &iCount,int formNum,wstring &word);
  void transferDBUsagePatternsToUsagePattern(int highestCost,int *DBUsagePatterns,unsigned int upStart,unsigned int upLength);
  bool updateFromDB(int wordId,unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int mainEntryWordId,int iDerivationRules,int iSourceId,int formNum,wstring &word);
  bool isProperNounSubClass(void);
  bool blockProperNounRecognition(void);
  int query(int form);
	bool hasWinnerVerbForm(int winnerForms);
	bool hasVerbForm();
	bool hasWinnerNounForm(int winnerForms);
	bool hasNounForm();
	int query(wstring form);
  int lowestSeparatorCost();
    bool isLowestCost(int form);
  int queryForSeparator(void);
  bool remove(int form);
  bool remove(wchar_t *formName);
  int addForm(int form,const wstring &word,bool illegal=false);
	void cloneForms(cSourceWordInfo fromWord);
  int adjustFormsInflections(wstring originalWord,unsigned __int64 &flags,bool isFirstWord,int nounOwner,bool allCaps,bool firstLetterCapitalized, bool log);
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
  bool notCostable(wstring word,int flags);
  void logFormUsageCosts(wstring w);
  void createGroup(relationWOTypes relationType,tIWMM toWord);
	cSourceWordInfo::cRMap::tIcRMap addRelation(int where,int rType,tIWMM word);
  bool intersect(relationWOTypes relationType,tIWMM word,tIWMM self,tIWMM &fromWord,tIWMM &toWord);
  void transferUsagePatternsToCosts(int highestCost,unsigned int upStart,unsigned int upLength);
  void transferFormUsagePatternsToCosts(int sameNameForm,int properNounForm,int iCount);
	void resetUsagePatternsAndCosts(wstring sWord);
	void logReset(wstring sWord);
	void resetCapitalizationAndProperNounUsageStatistics();
	char getUsageCost(int formIndex) { return (formIndex>= MAX_USAGE_PATTERNS || formIndex<0) ? -1 : usageCosts[formIndex]; }
	char getUsagePattern(int formIndex) { return (formIndex >= MAX_USAGE_PATTERNS || formIndex<0) ? -1 : usagePatterns[formIndex]; }
	// controlled by the updateWordUsageCostsDynamically flag, currently globally set to false
	void incrementTransferCount()
	{
		deltaUsagePatterns[cSourceWordInfo::TRANSFER_COUNT]++;
		usagePatterns[cSourceWordInfo::TRANSFER_COUNT]++;
		changedSinceLastWordRelationFlush = true;
	}
	bool isWinner(int form,int tmpWinnerForms)
	{
		if (form >= sizeof(tmpWinnerForms) * 8)
		{
			return false;
		}
		return (tmpWinnerForms) ? ((1 << form)&tmpWinnerForms) != 0 : true;
	}
	// controlled by the updateWordUsageCostsDynamically flag, currently globally set to false
	bool updateFormUsagePatterns(int tmpWinnerForms,wstring sWord)
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
		unsigned int topAllowableUsageCount = min(count, cSourceWordInfo::MAX_USAGE_PATTERNS);
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
	void normalize(unsigned char *usages, int start, int len)
	{
		len += start;
		int I;
		for (I = start; I < len && usages[I] == 255; I++);
		if (I == len)
			for (int J = start; J < len; J++)
				usages[J] >>= 1;
	}
	void zeroNewProperNounCostIfUsedAllCaps() { usageCosts[formsSize()] = 0; }
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
	int getLowestTopLevelCost(void)
	{
			int lowestCost = 10000;
		for (unsigned int f = 0; f < count; f++)
			if (Form(f)->isTopLevel)
				lowestCost = min(lowestCost, usageCosts[f]);
		return lowestCost;
	}

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

bool loosesort( const wchar_t *s1, const wchar_t *s2 );

bool equivalentIfIgnoreDashSpaceCase(wstring sWord,wstring word2);
void removeDots(wstring &str);
int takeLastMatch(wstring &buffer,wstring begin_string,wstring end_string,wstring &match,bool include_begin_and_end);
size_t firstMatch(wstring &buffer,wstring begin_string,wstring end_string,size_t &beginPos,wstring &match,bool include_begin_and_end);
int firstMatchNonEmbedded(wstring &buffer,wstring beginString,wstring endString,size_t &beginPos,wstring &match,bool include_begin_and_end);
int getInflection(wstring sWord,wstring form,wstring mainEntry,wstring iform,vector <wstring> &allInflections);
int nextMatch(wstring &buffer,wstring begin_string,wstring end_string,size_t &begin_pos,wstring &match,bool include_begin_and_end);
int getPath(const wchar_t *pathname,void *buffer,int maxlen,int &actualLen);

class cWord
{
  friend class cSourceWordInfo;

public:
  cWord(void);
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
  static tIWMM end(void)
  {
    return WMM.end();
  }
	static unordered_map <wstring, vector <tIWMM>> mainEntryMap;
  static tIWMM query(wstring sWord);
  tIWMM gquery(wstring sWord);
  bool parseMetaCommands(int where,wchar_t *buffer,int &endSymbol, wstring &comment, sTrace &t);
  int ignoreSpecialContext(wchar_t* buffer, __int64 bufferLen, __int64& bufferScanLocation,
    wstring& sWord, wstring& comment, int& nounOwner, bool scanForSection, bool webScrapeParse, sTrace& t, MYSQL* mysql, int sourceId, int sourceType, __int64& cp);
  int readContraction(wchar_t* buffer, __int64 bufferLen, __int64& bufferScanLocation, wstring& sWord, __int64& cp);
  int readSpecialWords(wchar_t* buffer, __int64 bufferLen, __int64& bufferScanLocation, wstring& sWord, int nounOwner, __int64& cp);
  int readCharacters(wchar_t* buffer, __int64 bufferLen, wstring& sWord, __int64& cp);
  int readDanglingDash(wchar_t* buffer, __int64 bufferLen, wstring& sWord, MYSQL* mysql, int sourceId, __int64& cp);
  void readOwnership(wchar_t* buffer, __int64 bufferLen, wstring& sWord, int& nounOwner, __int64& cp);
  void readNTContraction(wchar_t* buffer, __int64 bufferLen, wstring& sWord, int numChars, __int64& cp);
  void readDigits(wstring& sWord, MYSQL* mysql, int sourceId, __int64& cp);
  int readWord(wchar_t *buffer,__int64 bufferLen,__int64 &bufferScanLocation,wstring &sWord, wstring &comment, int &nounOwner,bool scanForSection,bool webScrapeParse,sTrace &t,MYSQL *mysql,int sourceId, int sourceType);
  int processFootnote(wchar_t *buffer,__int64 bufferLen,__int64 &cp);
  void readContraction(wchar_t* buffer, __int64 bufferLen, wstring& sWord, int numChars, __int64& cp);
  int readSpecialWords(wchar_t* buffer, __int64 bufferLen, __int64& bufferScanLocation, wstring& sWord, __int64& cp);
	int parseWord(MYSQL *mysql, wstring sWord, tIWMM &iWord, bool log);
	static int attemptDisInclination(MYSQL *mysql, tIWMM &iWord, wstring sWord, int sourceId,bool log);
	static int parseWord(MYSQL *mysql, wstring sWord, tIWMM &iWord, bool firstLetterCapitalized, int nounOwner, int sourceId, bool log);
  static tIWMM addNewOrModify(MYSQL *mysql,wstring sWord,int flags,int form,int inflection,int derivationRules,wstring mainEntry,int sourceId,bool &added,bool markUndefined=false); // only used for adding a name
  // generic utilities
	static bool illegalWord(MYSQL *mysql, wstring sWord);
	bool isAllUpper(wstring &sWord);
  bool remove(wstring sWord);
	static void resetUsagePatternsAndCosts(sTrace debugTrace);
	static void resetCapitalizationAndProperNounUsageStatistics(sTrace debugTrace);
	int readFormsCache(char *buffer,int bufferlen,int &numReadForms);
  int readWords(wstring oPath,int sourceId, bool disqualifyWords, wstring specialExtension);
	int writeFormsCache(int fd);
  bool removeInflectionFlag(wstring sWord,int flag);
	static bool isDash(wchar_t ch);
	static bool isSingleQuote(wchar_t ch);
	static bool isDoubleQuote(wchar_t ch);

  // MYSQL database
  int lastReadfromDBTime;
  void readForms(MYSQL &mysql, wchar_t *qt);
	void mapWordIdToWordStructure(int wordId, tIWMM iWord);
	tIWMM wordStructureGivenWordIdExists(int wordId);
  bool acquireLock(MYSQL &mysql,bool persistent);
  void releaseLock(MYSQL &mysql);

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
  void addTimeFlag(int flag, const wchar_t *words[]);
  void usageCostToNoun(Inflections words[], const wchar_t * nounSubclass);
	void toLowestUsageCost(Inflections words[], const wchar_t * formClass);
  void usageCostToNoun(const wchar_t * words[], const wchar_t * nounSubclass);
  // the following are only used temporarily in BNCC and should be moved back to private usage
  int predefineVerbsFromFile(wstring form,wstring shortForm, const wchar_t * path,int flags);
  int predefineWords(Inflections words[],wstring form,wstring shortForm,wstring inflectionsClass=L"",int flags=0,bool properNounSubClass=false);
  int predefineWords(const wchar_t *words[],wstring form,wstring shortForm,int flags=0,bool properNounSubClass=false);
  static tIWMM predefineWord(const wchar_t *word,int flags=0);
	void extendedParseHolidays();
	int predefineHolidays();
	static int processTime(wstring sWord, char &hour, char &minute);
	static int processDate(wstring sWord, short &year, char &month, char &dayOfMonth);
	static int splitWord(MYSQL *mysql, tIWMM &iWord, wstring sWord, int sourceId,bool log);
	static tIWMM fullQuery(MYSQL *mysql, wstring word, int sourceId);
	static int writeWord(tIWMM iWord, void *buffer, int &where, int limit);
	int readWordsFromDB(MYSQL &mysql, bool generateFormStatistics, bool printProgress, bool skipWordInitialization);
	int initializeWordRelationsFromDB(MYSQL mysql, set <int> wordIds, bool inSourceFlagSet,bool log);

protected:
  bool appendToUnknownWordsMode;
  void moveFormOffsets(int formsOffset)
  {
    for (tIWMM I=WMM.begin(),WMMEnd=WMM.end(); I!=WMMEnd; I++)
      if (I->second.formsOffset>=formsOffset)
        I->second.formsOffset++;
  }

private:
	tIWMM *idToMap;
	int idsAllocated;
  typedef pair <wstring, cSourceWordInfo> tWFIMap;
  static int lastWordWrittenClock;
	bool isWordFormCacheValid(MYSQL &mysql);
	int readWordFormsFromDB(MYSQL &mysql,int maxWordId,wchar_t *qt,int *words,int *counts,int &numWordForms,unsigned int * &wordForms, bool printProgress, bool skipWordInitialization, wstring specialExtension);
	int initializeWordsFromDB(MYSQL &mysql, int *words, int *counts, unsigned int * &wordForms, int &numWordsInserted, int &numWordsModified, bool printProgress);
  int lastModifiedTime;
  int minimumLastWordWrittenClockDiff;
  static bool changedWords;
  static bool inCreateDictionaryPhase;
  vector <wstring> unknownWDWords;
  vector <wstring> unknownCDWords;
  void readUnknownWords(wchar_t *fileName,vector <wstring> &unknownWords);
  void writeUnknownWords(wchar_t *fileName,vector <wstring> &unknownWords);
  int write(void);

  // initialized
  static vector <wchar_t *> multiElementWords;
  static vector <wchar_t *> quotedWords;
  static vector <wchar_t *> periodWords;
  static unordered_map <wstring, cSourceWordInfo> WMM;
  // filled during program execution
	static int disinclinationRecursionCount;

  bool evaluateIncludedSingleQuote(wchar_t *buffer,__int64 cp,__int64 begincp);
  int addGenderedNouns(const wchar_t *genPath,int inflectionFlags,int wordForm);
	int addDemonyms(const wchar_t * demPath);
	bool readVerbClasses(void);
	bool readVerbClassNames(void);
  bool addPlaces(wstring pPath,vector <tmWS > &objects);
	void addTimeFlags();
  void createTimeCategories(bool normalize);
  static int getForms(MYSQL *mysql,tIWMM &iWord,wstring sWord,int sourceId, bool logEverything);
  tIWMM addCopy(wstring sWord,tIWMM iWord,bool &added);
  static tIWMM query(wstring sWord,int form,int inflection,int &offset);
  bool addInflectionFlag(wstring sWord,int flag);
  static tIWMM hasFormInflection(tIWMM iWord,wstring sForm,int inflection);
  static bool handleExtendedParseWords(const wchar_t * word);
  int continueParse(wchar_t *buffer,__int64 begincp,__int64 bufferLen,vector<wchar_t *> &multiWords);
  static int addWordToForm(wstring sWord,tIWMM &iWord,int flags,wstring sForm,wstring shortForm,int inflection,int derivationRules,wstring mainEntry,int sourceId,bool &added,bool markUndefined=false);
  int predefineWords(InflectionsRoot words[],wstring form,wstring shortForm,wstring inflectionsClass=L"",int flags=0,bool properNounSubClass=false);
  bool closeConnection(void);
  static int checkAdd(const wchar_t * fromWhere,tIWMM &iWord,wstring sWord,int flags,wstring sForm,int inflection,int derivationRules,wstring mainEntry,int sourceId,bool log);

  #ifdef CHECK_WORD_CACHE
    // test routines
    int checkWord(cWord &Words2,tIWMM originalIWord,tIWMM newWord,int ret);
  #endif

  int addProperNamesFile(wstring path);
  void addNickNames(const wchar_t * filePath);
	static int markWordUndefined(tIWMM &iWord,wstring sWord,int flags,bool firstWordCapitalized,int nounOwner,int sourceId);
  void generateFormStatistics(void);
  int processDate(wstring &sWord, wchar_t *buffer,__int64 &cp,__int64 &bufferScanLocation);
	int processTime(wstring &sWord, wchar_t *buffer, __int64 &cp, __int64 &bufferScanLocation);
	int processWebAddress(wstring &sWord, wchar_t *buffer, __int64 &cp, __int64 bufferLen);
	static bool findWordInDB(MYSQL *mysql, wstring &sWord, int &wordId, tIWMM &iWord);
};

#include "pattern.h"

class cWordMatch;

#include "patternElementMatchArray.h"
#include "patternMatchArray.h"

extern const wchar_t *OCSubTypeStrings[];
