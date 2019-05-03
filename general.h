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
#define CACHEDIR L"J:\\caches"
#define TEXTDIR L"J:\\caches"
#define MAX_COST 100000
#define CONFIDENCE_NOMATCH 100000
#define UMBEL_CACHE_VERSION L'J'
#define RDFLIBRARYTYPE_VERSION L'J'
#define RDFTYPE_VERSION L'J'
#define EXTENDED_RDFTYPE_VERSION L'O'

// When this enum is changed, inflection calculations must also change in net.cpp
enum InflectionTypes {
  SINGULAR=1,PLURAL=2,SINGULAR_OWNER=4,PLURAL_OWNER=8,
  VERB_PAST=16, VERB_PAST_PARTICIPLE=32,VERB_PRESENT_PARTICIPLE=64,
  VERB_PRESENT_THIRD_SINGULAR=128,VERB_PRESENT_FIRST_SINGULAR=256, // These 5 verb forms must remain in sequence
  VERB_PAST_THIRD_SINGULAR=512,VERB_PAST_PLURAL=1024,VERB_PRESENT_PLURAL=2048,VERB_PRESENT_SECOND_SINGULAR=4096,
  ADJECTIVE_NORMATIVE=8192,ADJECTIVE_COMPARATIVE=16384,ADJECTIVE_SUPERLATIVE=32768,
  ADVERB_NORMATIVE=65536,ADVERB_COMPARATIVE=131072,ADVERB_SUPERLATIVE=262144,
  MALE_GENDER=_MIL*1,FEMALE_GENDER=_MIL*2,NEUTER_GENDER=_MIL*4,
  FIRST_PERSON=_MIL*8 /*ME*/,SECOND_PERSON=_MIL*16 /*YOU*/,THIRD_PERSON=_MIL*32, /*THEM*/
  NO_OWNER=_MIL*64,VERB_NO_PAST=_MIL*64, // special case to match only nouns and Proper Nouns that do not have 's / and verbs that should only be past participles

  OPEN_INFLECTION=_MIL*128,CLOSE_INFLECTION=_MIL*256, // overlap
	MALE_GENDER_ONLY_CAPITALIZED=_MIL*512,FEMALE_GENDER_ONLY_CAPITALIZED=_MIL*1024,
	ONLY_CAPITALIZED=(MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED)
};

class bitObject
{
public:
  enum { sizeOfInteger=32 };
  bitObject(void)
  {
    memset(bits,0,sizeof(bits));
    byteIndex=bitIndex=-1;
  }
  void clear(void)
  {
    memset(bits,0,sizeof(bits));
  }
  bool check(wchar_t *type,int limit)
  {
    if (limit>=(sizeof(bits)<<3))
      lplog(LOG_FATAL_ERROR,L"FATAL ERROR: bit storage for %s exceeded - size %d cannot fit into bit storage for %d.",type,limit,(sizeof(bits)<<3)-1);
    return true;
  }
  inline void set(int bit)
  {
    bits[bit>>5]|=1<<(bit&(sizeOfInteger-1));
  }
  inline void reset(int bit)
  {
    bits[bit>>5]&=~(1<<(bit&(sizeOfInteger-1)));
  }
  inline bool isSet(int bit)
  {
    return (bits[bit>>5]&(1<<(bit&(sizeOfInteger-1))))!=0;
  }
  bool operator != (const bitObject &o)
  {
    for (unsigned int I=0; I<sizeof(bits)/sizeof(*bits); I++)
      if (bits[I]!=o.bits[I]) return true;
    return false;
  }
  bool operator == (const bitObject &o)
  {
    for (unsigned int I=0; I<sizeof(bits)/sizeof(*bits); I++)
      if (bits[I]!=o.bits[I]) return false;
    return true;
  }
  void operator |= (const bitObject &o)
  {
    for (unsigned int I=0; I<sizeof(bits)/sizeof(*bits); I++)
      bits[I]|=o.bits[I];
  }
  void operator &= (const bitObject &o)
  {
    for (unsigned int I=0; I<sizeof(bits)/sizeof(*bits); I++)
      bits[I]&=o.bits[I];
  }
  void operator -= (const bitObject &o)
  {
    for (unsigned int I=0; I<sizeof(bits)/sizeof(*bits); I++)
      bits[I]&=~o.bits[I];
  }
  int first(void)
  {
    byteIndex=0;
    bitIndex=-1;
    return next();
  }
  int next(void)
  {
    if (++bitIndex>=sizeOfInteger)
    {
      byteIndex++;
      bitIndex=0;
    }
    for (; byteIndex<sizeof(bits)/sizeof(*bits); byteIndex++)
    {
      for (int b=bits[byteIndex]; bitIndex<sizeOfInteger; bitIndex++)
        if (b&(1<<bitIndex))
          return bitIndex+byteIndex*sizeOfInteger;
      bitIndex=0;
    }
    byteIndex=0;
    bitIndex=-1;
    return -1;
  }
  int firstNotIn(const bitObject &o)
  {
    byteIndex=0;
    bitIndex=-1;
    return nextNotIn(o);
  }
  int nextNotIn(const bitObject &o)
  {
    if (++bitIndex>=sizeOfInteger)
    {
      byteIndex++;
      bitIndex=0;
    }
    for (; byteIndex<sizeof(bits)/sizeof(*bits); byteIndex++)
    {
      for (int b=bits[byteIndex]&~o.bits[byteIndex]; bitIndex<sizeOfInteger; bitIndex++)
        if (b&(1<<bitIndex))
          return bitIndex+byteIndex*sizeOfInteger;
      bitIndex=0;
    }
    byteIndex=0;
    bitIndex=-1;
    return -1;
  }
  // bitObject must find all of itself in o
  // patterns[p]->mandatoryPatterns.hasAll(matchedPatterns)
  bool hasAll(const bitObject &o)
  {
    for (unsigned int I=0; I<sizeof(bits)/sizeof(*bits); I++)
      if ((bits[I]&o.bits[I])!=bits[I])
        return false;
    return true;
  }
  bool isEmpty(void)
  {
    unsigned int I;
    for (I=0; I<sizeof(bits)/sizeof(*bits) && !bits[I]; I++);
    return I==sizeof(bits)/sizeof(*bits);
  }
  bool read(char *buffer,int &where,unsigned int limit)
  {
		if (where+sizeof(bits)>limit) 
		{
			lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached (1)!",limit);
		  return false;
		}
    memcpy(bits,buffer+where,sizeof(bits));
    where+=sizeof(bits);
    byteIndex=bitIndex=-1;
    return true;
  }
  bool write(void *buffer,int &where,int limit)
  {
    memcpy(((char *)buffer)+where,bits,sizeof(bits));
    where+=sizeof(bits);
		if (where>limit) 
			lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached (1)!",limit);
    return true;
  }
private:
  unsigned int bits[16];
  unsigned short byteIndex,bitIndex;
};

void *tmalloc(size_t num);
void *tcalloc(size_t num,size_t SizeOfElements);
void *trealloc(int from,void *original,unsigned int oldbytes,unsigned int newbytes);
void tfree(size_t oldbytes,void *original);
int getCounter(wchar_t *counter,unsigned long &dwValue);

#define IOHANDLE int

void itos(wchar_t *before,int i,wstring &concat,wchar_t *after);
void itos(wchar_t *before,int i,wstring &concat,wstring after);
wstring itos(int i,wstring &tmp);
wstring dtos(double fl,wstring &tmp);
char *wTM(wstring inString,string &outString,int codePage=CP_UTF8);
const wchar_t *mTW(string inString, wstring &outString);
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

// command line parameters set globally
extern bool TSROverride,flipTOROverride,flipTNROverride,flipTMSOverride,flipTUMSOverride;
extern bool traceParseInfo;
extern bool preTaggedSource;
extern short logCache;
extern int bandwidthControl;

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
const wchar_t *allFlags(int inflectionFlags, wstring &sFlags);
const wchar_t *allWordFlags(int inflectionFlags, wstring &sFlags);
void readVBNet(void);
wstring relationString(int r);
void *WideCharToMultiByte(wchar_t *q,int &queryLength,void *&buffer,unsigned int &bufSize);
void distributeToSubDirectories(wchar_t *path,int pathlen,bool createDirs);
void convertIllegalChars(wchar_t *path); // from getWikipedia
void deleteIllegalChars(char *path); // from getWikipedia
extern int memoryAllocated;
extern unordered_map <wstring,set < wstring > > nounVerbMap;
wstring vectorString(vector <wstring> &vstr,wstring &tmpstr,wstring separator);
wstring vectorString(vector < vector <wstring> > &vstr,wstring &tmpstr,wstring separator);
wstring setString(set <wstring> &sstr,wstring &tmpstr,wchar_t *separator);
string setString(set <string> &sstr,string &tmpstr,char *separator);
int getWebPath(int where,wstring webAddress,wstring &buffer,wstring epath,wstring cacheTypePath,wstring &filePathOut,wstring &headers,int searchIndex,bool clean,bool readInfoBuffer, bool forceWebReread=false);
int testWebPath(int where,wstring webAddress,wstring epath,wstring cacheTypePath,wstring &filePathOut,wstring &headers);
const wchar_t *ontologyTypeString(int ontologyType,int resourceType, wstring &Btmpstr);
#define SEPARATOR L"SEPARATOR|||"
int initializeDatabaseHandle(MYSQL &mysql,wchar_t *where,bool &alreadyConnected);
wstring lastErrorMsg();

// google API used for custom search and freebase
extern wstring webSearchKey;
char * LastErrorStr(void);
extern SRWLOCK rdfTypeMapSRWLock,mySQLTotalTimeSRWLock,totalInternetTimeWaitBandwidthControlSRWLock,mySQLQueryBufferSRWLock,orderedHyperNymsMapSRWLock;
void positionConsole(bool controller);
	