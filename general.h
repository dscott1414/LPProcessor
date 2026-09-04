/*
	general.h - shared constants, inflection bit flags, tracked allocators, and the binary-cache copy() overloads

	Overview:
		This is the kitchen-sink header every first-party translation unit eventually
		pulls in (via word.h).  It owns: parse-cost constants; the InflectionTypes bit
		enum (noun/verb/adjective/adverb/gender/person); the tmalloc/tcalloc/trealloc/
		tfree wrappers that keep memoryAllocated; the copy() serialize/deserialize
		overloads used by the binary source cache; and a pile of WordNet/VerbNet helpers
		declared here and defined in utilities.cpp / net.cpp.

	Pipeline position:
		Available from initialization onward.  InflectionTypes bits live on
		cSourceWordInfo::inflectionFlags and are consulted during tokenize, pattern
		costing, agreement, and stemming (paice.cpp).  copy() is the cache codec used
		by source.cpp read()/write().

	Key data structures / globals:
		- MAINDIR / LMAINDIR / CACHEDIR / WEBSEARCH_CACHEDIR / TEXTDIR - compile-time
			fallback paths (F:\\lp and M:\\caches), used only by envConfig.cpp when the
			corresponding LP_MAIN_DIR / LP_CACHE_DIR / LP_WEBSEARCH_CACHE_DIR /
			LP_TEXT_DIR environment variable is not set. Call getMainDir() /
			getCacheDir() / getWebSearchCacheDir() / getTextDir() (envConfig.h)
			instead of referencing these macros directly.
		- InflectionTypes - unsigned bit flags.  When this enum changes, net.cpp
			inflection calculations must change in lockstep (author note on the enum).
		- NO_OWNER and VERB_NO_PAST share the same bit (_MIL*64) by design.
		- memoryAllocated - process-wide counter mutated by tmalloc/tfree; comment
			in memoryStat.cpp says "protect with mutex" but there is no lock.
		- Google/Bing/Merriam-Webster credentials - read from the environment via
			envConfig.h's getGoogleCSEKey() / getGoogleCSEContext() /
			getBingSubscriptionKey() / getMerriamWebsterKey(), not stored as globals
			here (there used to be a hardcoded webSearchKey/BINGAccountKey/cseContext
			set of globals in questionAnsweringWebSearch.cpp; removed).
		- mySQLQueryBufferSRWLock / mySQLTotalTimeSRWLock / rdfTypeMapSRWLock /
			orderedHyperNymsMapSRWLock - the four process-wide locks used by DB and RDF.
			These are std::shared_mutex since batch B3 (see the declaration at the
			bottom of this file); the "SRWLock" in each name is historical.

	Notes / gotchas:
		- _MIL is 1024*1024 without parens; only use it as a factor, not in a larger
			unparenthesized expression.
		- MAX_INT / MIN_INT are computed from ~unsigned(0); they assume 32-bit int.
		- copy() 'where' is an in/out byte offset into buf; false means the buffer was
			truncated (the cache is corrupt).  The "if (error=!copy(...))" idiom in
			source.h is intentional assignment, not a == typo.
		- WideCharToMultiByte() declared here is the 4-arg UTF-8 helper in
			DBUtility.cpp, not the Win32 API.
*/
#pragma once
// Batch B3: general.h is included (directly or via word.h) by virtually every
// first-party translation unit, and until now depended entirely on each includer
// having already pulled in <vector>/<set>/<string>/<unordered_set>/
// <unordered_map>, logging.h (for sTrace), and a codepage constant for wTM's
// default argument -- an ordering contract that held only because every .cpp
// happened to include windows.h and the std headers first. Those dependencies are
// declared here now, so this header stands on its own.
//
// Bare (unqualified) `vector`/`set`/`string` in the declarations below are
// deliberately left alone: the codebase-wide convention is that
// `using namespace std;` is already active at every point general.h is reached
// (word.h establishes it before its own include of this file). Including the std
// headers here does not change that convention, it just stops this file from
// silently depending on someone else having included them.
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <shared_mutex>
#include <unistd.h> // batch B5/cleanup: ::close, for cScopedFd below
#include "lpchar.h" // batch B2: lpchar_t/lpwstring + lp_* string primitives, reachable from here for virtually every first-party file (see general.h's own file header)
#include "utfConvert.h" // batch B3: CP_UTF8, the default argument of wTM() declared below
#include "lpFile.h"    // batch B5: portable wide-path filesystem calls (lp_wfopen, MAX_PATH, ...)
#include "logging.h"    // batch B3: sTrace, used in several declarations below
#include "envConfig.h"
#define COST_PER_RELATION 1
#define NON_AGREEMENT_COST 10
#define MIN_SIGNED_SHORT -32768
#define MAX_INT (~unsigned (0) >> 1)          /* Most positive integer value. */
#define MIN_INT (~unsigned (0) ^ MAX_INT)     /* Most negative integer value. */
#define MAX_SIGNED_SHORT 32767
#define _MIL 1024*1024
#define MAINDIR "F:\\lp"
#define LMAINDIR u"F:\\lp"
#define CACHEDIR u"M:\\caches"
#define WEBSEARCH_CACHEDIR u"M:\\caches"
#define TEXTDIR u"M:\\caches"
#define MAX_COST 100000
#define CONFIDENCE_NOMATCH 100000
#define RDFLIBRARYTYPE_VERSION u'M'
#define RDFTYPE_VERSION u'M'
#define EXTENDED_RDFTYPE_VERSION u'R'

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
int getCounter(const lpchar_t *counter,unsigned long &dwValue);

#define IOHANDLE int

// ---------------------------------------------------------------------------
// RAII helpers over the two resources this codebase leaks most easily.
//
// The code review recommended heap-allocating the remaining multi-megabyte
// stack buffers "with a matching tfree() on EVERY return path -- audit each
// return in the function before applying". cSource::write() has 26 of them.
// Auditing 26 paths by hand is how a leak or a double free gets introduced, so
// the buffer owns itself instead: one declaration, correct on every path
// including ones added later. The memory is still tmalloc'd, so it still shows
// up in memoryAllocated exactly as that recommendation intended.
// ---------------------------------------------------------------------------
class cTrackedBuffer
{
	size_t bytes;
	void *memory;
public:
	explicit cTrackedBuffer(size_t byteCount) : bytes(byteCount), memory(tmalloc(byteCount)) {}
	~cTrackedBuffer() { if (memory) tfree(bytes, memory); }
	cTrackedBuffer(const cTrackedBuffer &) = delete;
	cTrackedBuffer &operator=(const cTrackedBuffer &) = delete;
	// tmalloc logs LOG_FATAL_ERROR and exits on exhaustion, so this is never
	// null in practice; checked anyway so a caller can be written defensively.
	bool valid() const { return memory != NULL; }
	operator char *() const { return (char *)memory; }
	char *get() const { return (char *)memory; }
};

// Closes a descriptor on every exit path. Same motivation: the functions that
// hold an fd across a long body were leaking it on their early-return arms.
class cScopedFd
{
	int fd;
public:
	explicit cScopedFd(int descriptor) : fd(descriptor) {}
	~cScopedFd() { if (fd >= 0) ::close(fd); }
	cScopedFd(const cScopedFd &) = delete;
	cScopedFd &operator=(const cScopedFd &) = delete;
	operator int() const { return fd; }
	int get() const { return fd; }
	// Hand ownership back for the paths that must close early and check the result.
	int release() { int r = fd; fd = -1; return r; }
};

void itos(const lpchar_t *before,int i,lpwstring &concat,lpchar_t *after);
void itos(const lpchar_t *before,int i,lpwstring &concat,lpwstring after);
lpwstring itos(int i, lpwstring &tmp);
lpwstring itos(int i, const lpchar_t *format, lpwstring &tmp);
lpwstring dtos(double fl,lpwstring &tmp);
char *wTM(lpwstring inString,string &outString,int codePage=CP_UTF8);
const lpchar_t *mTW(string inString, lpwstring &outString);
const lpchar_t *mTW(string inString, lpwstring &outString, int &codePage);
const lpchar_t *mTW(string inString, lpwstring &outString, int &codePage, bool &iso8859ControlCharactersFound);
const lpchar_t *mTWCodePage(string inString, lpwstring &outString, int codepage,int &error);
vector<lpwstring> splitString(lpwstring str, lpchar_t wc);

bool copy(void *buf,lpwstring str,int &where,int limit);
bool copy(void *buf,int num,int &where,int limit);
bool copy(void *buf,short num,int &where,int limit);
bool copy(void *buf,unsigned short num,int &where,int limit);
bool copy(void *buf,unsigned int num,int &where,int limit);
bool copy(void *buf,int64_t num,int &where,int limit);
bool copy(void *buf,uint64_t num,int &where,int limit);
bool copy(void *buf,char ch,int &where,int limit);
bool copy(void *buf,unsigned char ch,int &where,int limit);
bool copy(void *buf,set <int> &s,int &where,int limit);
bool copy(void *buf,vector <int> &s,int &where,int limit);
bool copy(void *buf,vector <lpwstring> &s,int &where,int limit);
bool copy(void *buf,vector <string> &s,int &where,int limit);
bool copy(void *buf,set <string> &s,int &where,int limit);
bool copy(void *buf, set <lpwstring> &s, int &where, int limit);
bool copy(void *buf, unordered_set <lpwstring> &s, int &where, int limit);

bool copy(const lpchar_t *str,void *buf,int &where,int limit);
bool copy(lpwstring &str,void *buf,int &where,int limit);
bool copy(int &num,void *buf,int &where,int limit);
bool copy(unsigned int &num,void *buf,int &where,int limit);
bool copy(int64_t &num,void *buf,int &where,int limit);
bool copy(uint64_t &num,void *buf,int &where,int limit);
bool copy(short &num,void *buf,int &where,int limit);
bool copy(unsigned short &num,void *buf,int &where,int limit);
bool copy(char &ch,void *buf,int &where,int limit);
bool copy(unsigned char &ch,void *buf,int &where,int limit);
bool copy(set <int> &s,void *buf,int &where,int limit);
bool copy(vector <int> &s,void *buf,int &where,int limit);
bool copy(vector <lpwstring> &s,void *buf,int &where,int limit);
bool copy(vector <string> &s,void *buf,int &where,int limit);
bool copy(set <string> &s,void *buf,int &where,int limit);
bool copy(set <lpwstring> &s, void *buf, int &where, int limit);
bool copy(unordered_set <lpwstring> &s, void *buf, int &where, int limit);

void escapeSingleQuote(lpwstring &lobject);
void removeSingleQuote(lpwstring &lobject);
void removeExcessSpaces(lpwstring &lobject);
void trim(lpwstring &str);

// command line parameters set globally
extern bool TSROverride,flipTOROverride,flipTNROverride,logMatchedSentences,logUnmatchedSentences;
extern bool preTaggedSource;
extern short logCache;

// One multi-word string plus a "already consumed" flag.  Used by the WordNet
// hyponym walk (addHyponyms / addCoords) to hand a preferred-sense phrase
// around without copying it twice.
class tmWS 
{
public:
	vector <lpwstring> ws;
	bool taken;
	tmWS(vector <lpwstring> &o)
	{
		ws=o;
		taken=false;
	}
};

void getAntonyms(lpwstring word,unordered_set <lpwstring> &synonyms,sTrace &t);
int getFamiliarity(lpwstring word,bool isAdjective);
int getHighestFamiliarity(lpwstring word);
bool hasHyperNym(lpwstring word,lpwstring hyperNym,bool &found,bool trace);
void splitMultiWord(lpwstring multiWord,vector <lpwstring> &words);
void splitMultiWord(string multiWord,vector <lpwstring> &words);
bool addHyponyms(lpchar_t *word,vector <tmWS > &objects,lpchar_t *preferredSense,set <string> &ignoreCategories,bool print);
bool addHyponyms(lpchar_t *word,set <lpwstring> &objects);
bool addCoords(lpchar_t *word,vector <tmWS > &objects,int wnClass,lpchar_t *preferredSense,int &numFirstSense,set <string> &ignoreCategories,bool print);
lpwstring getMostCommonSynonym(lpwstring in,lpwstring &out,bool isNoun,bool isVerb,bool isAdjective,bool isAdverb,sTrace &t);
void analyzeNounClass(int where,int fromWhere,lpwstring in,int inflectionFlags,bool &measurableObject,bool &notMeasurableObject,bool &grouping,sTrace &t,lpwstring &lastNounNotFound,lpwstring &lastVerbNotFound);
void analyzeVerbNetClass(int where,lpwstring in,lpwstring &proposedSubstitute,int &numIrregular,int inflectionFlags,sTrace &t,lpwstring &lastNounNotFound,lpwstring &lastVerbNotFound);
bool inWordNetClass(int where,lpwstring in,int inflectionFlags,string group,lpwstring &lastNounNotFound,lpwstring &lastVerbNotFound);
void getAllHyperNyms(lpwstring in,vector < set <string> > &objects);
void getAllOrderedHyperNyms(lpwstring in,vector < vector <string> > &objects);
void deriveMainEntry(int where,int fromWhere,lpwstring &in,int &inflectionFlags,bool isVerb,bool isNoun,lpwstring &lastNounNotFound,lpwstring &lastVerbNotFound);
const lpchar_t *inflectionFlagsToStr(int inflectionFlags, lpwstring &sFlags);
const lpchar_t *allWordFlags(int inflectionFlags, lpwstring &sFlags);
void readVBNet(void);
lpwstring relationString(int r);
void *WideCharToMultiByte(const lpchar_t * q,int &queryLength,void *&buffer,unsigned int &bufSize);
void distributeToSubDirectories(lpchar_t *path,int pathlen,bool createDirs);
void convertIllegalChars(lpchar_t *path); // from getWikipedia
void deleteIllegalChars(char *path); // from getWikipedia
extern int64_t memoryAllocated;
extern unordered_map <lpwstring,set < lpwstring > > nounVerbMap;
lpwstring vectorString(vector <lpwstring> &vstr,lpwstring &tmpstr,lpwstring separator);
lpwstring vectorString(vector < vector <lpwstring> > &vstr,lpwstring &tmpstr,lpwstring separator);
lpwstring setString(set <lpwstring> &sstr,lpwstring &tmpstr,const lpchar_t *separator);
lpwstring setString(unordered_set <lpwstring>& sstr, lpwstring& tmpstr, const lpchar_t* separator);
string setString(set <string> &sstr,string &tmpstr,const char *separator);
const lpchar_t *ontologyTypeString(int ontologyType,int resourceType, lpwstring &Btmpstr);
#define SEPARATOR u"SEPARATOR|||"
// Batch B3: forward-declared rather than #include <mysql.h> here. Only the
// reference parameter below needs the name, an incomplete type is enough for
// that, and pulling the whole Connector/C header into the ~50 translation units
// that reach general.h (only 8 of which actually talk to MySQL) would be a large
// compile-time cost for nothing. Matches mysql.h's own
// `typedef struct MYSQL {...} MYSQL;` -- redeclaring a typedef to the same type
// is legal, so this stays valid whichever of the two headers a .cpp sees first.
struct MYSQL;
typedef struct MYSQL MYSQL;
int initializeDatabaseHandle(MYSQL &mysql,const lpchar_t *where,bool &alreadyConnected);
lpwstring lastErrorMsg();

const char * LastErrorStr(void);
// Batch B3: these four were Win32 SRWLOCKs (a reader/writer lock initialized by
// InitializeSRWLock and taken via AcquireSRWLockShared/Exclusive). std::shared_mutex
// is the direct, portable equivalent -- same shared/exclusive semantics, and unlike
// SRWLOCK it needs no explicit initialization call, so main.cpp's/specials_main.cpp's
// InitializeSRWLock block is gone rather than ported. The "SRWLock" in each name is
// kept deliberately: it is what the guarded-by comments throughout this codebase
// (DBUtility.cpp, createOntology.cpp, getWordNet.cpp, profile.h, word.h, ...) name,
// and renaming would touch far more files than it would clarify.
extern std::shared_mutex rdfTypeMapSRWLock,mySQLTotalTimeSRWLock,mySQLQueryBufferSRWLock,orderedHyperNymsMapSRWLock;

