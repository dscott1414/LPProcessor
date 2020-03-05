#pragma warning (disable: 4996)
#pragma warning(disable: 4267)
#define COST_PER_RELATION 1
#define NON_AGREEMENT_COST 10
#define MIN_SIGNED_SHORT -32768
#define MAX_INT (~unsigned (0) >> 1)          /* Most positive integer value. */
#define MIN_INT (~unsigned (0) ^ MAX_INT)     /* Most negative integer value. */
#define MAX_SIGNED_SHORT 32767
#define _MIL 1024*1024
#define MAINDIR "F:\\lp"
#define LMAINDIR L"F:\\lp"
#define CACHEDIR L"M:\\caches"
#define WEBSEARCH_CACHEDIR L"M:\\caches"
#define TEXTDIR L"M:\\caches"
#define MAX_COST 100000
#define CONFIDENCE_NOMATCH 100000
#define UMBEL_CACHE_VERSION L'J'
#define RDFLIBRARYTYPE_VERSION L'K'
#define RDFTYPE_VERSION L'K'
#define EXTENDED_RDFTYPE_VERSION L'P'

// When this enum is changed, inflection calculations must also change in net.cpp
enum InflectionTypes : unsigned int {
  SINGULAR=1,PLURAL=2,SINGULAR_OWNER=4,PLURAL_OWNER=8,
  VERB_PAST=16, VERB_PAST_PARTICIPLE=32,VERB_PRESENT_PARTICIPLE=64,
  VERB_PRESENT_THIRD_SINGULAR=128,VERB_PRESENT_FIRST_SINGULAR=256, // These 5 verb forms must remain in sequence
  VERB_PAST_THIRD_SINGULAR=512,VERB_PAST_PLURAL=1024,VERB_PRESENT_PLURAL=2048,VERB_PRESENT_SECOND_SINGULAR=4096,
  ADJECTIVE_NORMATIVE=8192,ADJECTIVE_COMPARATIVE=16384,ADJECTIVE_SUPERLATIVE=32768,
  ADVERB_NORMATIVE=65536,ADVERB_COMPARATIVE=131072,ADVERB_SUPERLATIVE=262144,
	NOUN_ONLY_UNCOUNTABLE=524288,
  MALE_GENDER=_MIL*1,FEMALE_GENDER=_MIL*2,NEUTER_GENDER=_MIL*4,
  FIRST_PERSON=_MIL*8 /*ME*/,SECOND_PERSON=_MIL*16 /*YOU*/,THIRD_PERSON=_MIL*32, /*THEM*/
  NO_OWNER=_MIL*64,VERB_NO_PAST=_MIL*64, // special case to match only nouns and Proper Nouns that do not have 's / and verbs that should only be past participles

  OPEN_INFLECTION=_MIL*128,CLOSE_INFLECTION=_MIL*256, // overlap
	MALE_GENDER_ONLY_CAPITALIZED=_MIL*512,FEMALE_GENDER_ONLY_CAPITALIZED=_MIL*1024,
	ONLY_CAPITALIZED=(MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED) ,
	NOUN_ALSO_UNCOUNTABLE = (unsigned)_MIL * 2048
};

/* These words should have NOUN_ALSO_UNCOUNTABLE set in DB but do not because that causes problems with the internal processing of inflectionFlags
art
time
age
light
understanding
experience
iron
work
advice
business
cake
coffee
education
love
pain
quality
room
success
vision
weight
youth
content
lack
marriage
food
friendship
paper
power
danger
failure
fire
injustice
painting
trade
hair
history
oil
philosophy
silence
soup
travel
trouble
production
environment
expense
fruit
glass
gossip
imagination
quantity
tea
wood
metal
beer
childhood
entertainment
noise
juice
meat
relaxation
tolerance
cheese
temperature
wine
currency
*/
void *tmalloc(size_t num);
void *tcalloc(size_t num,size_t SizeOfElements);
void *trealloc(int from,void *original,unsigned int oldbytes,unsigned int newbytes);
void tfree(size_t oldbytes,void *original);
int getCounter(wchar_t *counter,unsigned long &dwValue);

#define IOHANDLE int

void itos(wchar_t *before,int i,wstring &concat,wchar_t *after);
void itos(wchar_t *before,int i,wstring &concat,wstring after);
wstring itos(int i, wstring &tmp);
wstring itos(int i, wchar_t *format, wstring &tmp);
wstring dtos(double fl,wstring &tmp);
char *wTM(wstring inString,string &outString,int codePage=CP_UTF8);
const wchar_t *mTW(string inString, wstring &outString);
const wchar_t *mTW(string inString, wstring &outString,int &codePage);
const wchar_t *mTWCodePage(string inString, wstring &outString, int codepage,int &error);
vector<wstring> splitString(wstring str, wchar_t wc);

bool copy(void *buf,wstring str,int &where,int limit);
bool copy(void *buf,int num,int &where,int limit);
bool copy(void *buf,short num,int &where,int limit);
bool copy(void *buf,unsigned short num,int &where,int limit);
bool copy(void *buf,unsigned int num,int &where,int limit);
bool copy(void *buf,__int64 num,int &where,int limit);
bool copy(void *buf,unsigned __int64 num,int &where,int limit);
bool copy(void *buf,char ch,int &where,int limit);
bool copy(void *buf,unsigned char ch,int &where,int limit);
bool copy(void *buf,set <int> &s,int &where,int limit);
bool copy(void *buf,vector <int> &s,int &where,int limit);
bool copy(void *buf,vector <wstring> &s,int &where,int limit);
bool copy(void *buf,vector <string> &s,int &where,int limit);
bool copy(void *buf,set <string> &s,int &where,int limit);
bool copy(void *buf,set <wstring> &s,int &where,int limit);

bool copy(const wchar_t *str,void *buf,int &where,int limit);
bool copy(wstring &str,void *buf,int &where,int limit);
bool copy(int &num,void *buf,int &where,int limit);
bool copy(unsigned int &num,void *buf,int &where,int limit);
bool copy(__int64 &num,void *buf,int &where,int limit);
bool copy(unsigned __int64 &num,void *buf,int &where,int limit);
bool copy(short &num,void *buf,int &where,int limit);
bool copy(unsigned short &num,void *buf,int &where,int limit);
bool copy(char &ch,void *buf,int &where,int limit);
bool copy(unsigned char &ch,void *buf,int &where,int limit);
bool copy(set <int> &s,void *buf,int &where,int limit);
bool copy(vector <int> &s,void *buf,int &where,int limit);
bool copy(vector <wstring> &s,void *buf,int &where,int limit);
bool copy(vector <string> &s,void *buf,int &where,int limit);
bool copy(set <string> &s,void *buf,int &where,int limit);
bool copy(set <wstring> &s,void *buf,int &where,int limit);

void escapeSingleQuote(wstring &lobject);
void removeSingleQuote(wstring &lobject);
void removeExcessSpaces(wstring &lobject);
void trim(wstring &str);

// command line parameters set globally
extern bool TSROverride,flipTOROverride,flipTNROverride,logMatchedSentences,logUnmatchedSentences;
extern bool preTaggedSource;
extern short logCache;

class tmWS 
{
public:
	vector <wstring> ws;
	bool taken;
	tmWS(vector <wstring> &o)
	{
		ws=o;
		taken=false;
	}
};

void getSynonyms(wstring word,set <wstring> &synonyms, int synonymType,sTrace &t);
void getAntonyms(wstring word,set <wstring> &synonyms,sTrace &t);
int getFamiliarity(wstring word,bool isAdjective);
int getHighestFamiliarity(wstring word);
bool hasHyperNym(wstring word,wstring hyperNym,bool &found);
void splitMultiWord(wstring multiWord,vector <wstring> &words);
void splitMultiWord(string multiWord,vector <wstring> &words);
bool addHyponyms(wchar_t *word,vector <tmWS > &objects,wchar_t *preferredSense,set <string> &ignoreCategories,bool print);
bool addHyponyms(wchar_t *word,set <wstring> &objects);
bool addCoords(wchar_t *word,vector <tmWS > &objects,int wnClass,wchar_t *preferredSense,int &numFirstSense,set <string> &ignoreCategories,bool print);
wstring getMostCommonSynonym(wstring in,wstring &out,bool isNoun,bool isVerb,bool isAdjective,bool isAdverb,sTrace &t);
void analyzeNounClass(int where,int fromWhere,wstring in,int inflectionFlags,bool &measurableObject,bool &notMeasurableObject,bool &grouping,sTrace &t,wstring &lastNounNotFound,wstring &lastVerbNotFound);
void analyzeVerbNetClass(int where,wstring in,wstring &proposedSubstitute,int &numIrregular,int inflectionFlags,sTrace &t,wstring &lastNounNotFound,wstring &lastVerbNotFound);
bool inWordNetClass(int where,wstring in,int inflectionFlags,string group,wstring &lastNounNotFound,wstring &lastVerbNotFound);
void getAllHyperNyms(wstring in,vector < set <string> > &objects);
void getAllOrderedHyperNyms(wstring in,vector < vector <string> > &objects);
void deriveMainEntry(int where,int fromWhere,wstring &in,int &inflectionFlags,bool isVerb,bool isNoun,wstring &lastNounNotFound,wstring &lastVerbNotFound);
const wchar_t *inflectionFlagsToStr(int inflectionFlags, wstring &sFlags);
const wchar_t *allWordFlags(int inflectionFlags, wstring &sFlags);
void readVBNet(void);
wstring relationString(int r);
void *WideCharToMultiByte(wchar_t *q,int &queryLength,void *&buffer,unsigned int &bufSize);
void distributeToSubDirectories(wchar_t *path,int pathlen,bool createDirs);
void convertIllegalChars(wchar_t *path); // from getWikipedia
void deleteIllegalChars(char *path); // from getWikipedia
extern __int64 memoryAllocated;
extern unordered_map <wstring,set < wstring > > nounVerbMap;
wstring vectorString(vector <wstring> &vstr,wstring &tmpstr,wstring separator);
wstring vectorString(vector < vector <wstring> > &vstr,wstring &tmpstr,wstring separator);
wstring setString(set <wstring> &sstr,wstring &tmpstr,wchar_t *separator);
string setString(set <string> &sstr,string &tmpstr,char *separator);
const wchar_t *ontologyTypeString(int ontologyType,int resourceType, wstring &Btmpstr);
#define SEPARATOR L"SEPARATOR|||"
int initializeDatabaseHandle(MYSQL &mysql,wchar_t *where,bool &alreadyConnected);
wstring lastErrorMsg();

// google API used for custom search and freebase
extern wstring webSearchKey;
char * LastErrorStr(void);
extern SRWLOCK rdfTypeMapSRWLock,mySQLTotalTimeSRWLock,mySQLQueryBufferSRWLock,orderedHyperNymsMapSRWLock;
void positionConsole(bool controller);
	