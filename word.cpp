#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "math.h"
#include "sys/stat.h"
#include "malloc.h"
#include "profile.h"
#include "paice.h"
#include "mysqldb.h"

tInflectionMap nounInflectionMap[]=
{
  { SINGULAR,L"SINGULAR"},
  { PLURAL,L"PLURAL"},
  { SINGULAR_OWNER,L"SINGULAR_OWNER"},
  { PLURAL_OWNER,L"PLURAL_OWNER"},
  { MALE_GENDER,L"MALE_GENDER"},
  { FEMALE_GENDER,L"FEMALE_GENDER"},
  { NEUTER_GENDER,L"NEUTER_GENDER"},
  { FIRST_PERSON,L"FIRST_PERSON"},
  { SECOND_PERSON,L"SECOND_PERSON"},
  { THIRD_PERSON,L"THIRD_PERSON"},
  { -1,NULL}
};

tInflectionMap verbInflectionMap[]=
{
  { VERB_PAST,L"VERB_PAST"},
  { VERB_PAST_PARTICIPLE,L"VERB_PAST_PARTICIPLE"},
  { VERB_PRESENT_PARTICIPLE,L"VERB_PRESENT_PARTICIPLE"},
  { VERB_PRESENT_FIRST_SINGULAR,L"VERB_PRESENT_FIRST_SINGULAR"},
  { VERB_PRESENT_SECOND_SINGULAR,L"VERB_PRESENT_SECOND_SINGULAR"}, // "are"
  { VERB_PRESENT_THIRD_SINGULAR,L"VERB_PRESENT_THIRD_SINGULAR"},
  { VERB_PAST_THIRD_SINGULAR,L"VERB_PAST_THIRD_SINGULAR"},
  { VERB_PAST_PLURAL,L"VERB_PAST_PLURAL"}, // special cases like "were"
  { VERB_PRESENT_PLURAL,L"VERB_PRESENT_PLURAL"}, // special cases like "are"
  { -1,NULL}
};

tInflectionMap adjectiveInflectionMap[]=
{
  { ADJECTIVE_NORMATIVE,L"ADJECTIVE_NORMATIVE"},
  { ADJECTIVE_COMPARATIVE,L"ADJECTIVE_COMPARATIVE"},
  { ADJECTIVE_SUPERLATIVE,L"ADJECTIVE_SUPERLATIVE"},
  { -1,NULL}
};

tInflectionMap adverbInflectionMap[]=
{
  { ADVERB_NORMATIVE,L"ADVERB_NORMATIVE"},
  { ADVERB_COMPARATIVE,L"ADVERB_COMPARATIVE"},
  { ADVERB_SUPERLATIVE,L"ADVERB_SUPERLATIVE"},
  { -1,NULL}
};

tInflectionMap quoteInflectionMap[]=
{
  { OPEN_INFLECTION,L"OPEN_QUOTE"},
  { CLOSE_INFLECTION,L"CLOSE_QUOTE"},
  { -1,NULL}
};

tInflectionMap bracketInflectionMap[]=
{
  { OPEN_INFLECTION,L"OPEN_BRACKET"},
  { CLOSE_INFLECTION,L"CLOSE_BRACKET"},
  { -1,NULL}
};

unsigned int *tFI::formsArray; // change possible but shut off
unsigned int tFI::allocated; // change possible but shut off
unsigned int tFI::fACount; // change possible but shut off
int tFI::uniqueNewIndex; // use to insure every word has a unique index, even though it hasn't been
int WordClass::lastWordWrittenClock; /// not used
unordered_map <wstring,int> nicknameEquivalenceMap; // initialized 
bool FormsClass::changedForms; // change possible but shut off
unordered_map <wstring,int> FormsClass::formMap; // change possible but shut off
wchar_t *cacheDir; // initialize
vector <wchar_t *> WordClass::multiElementWords;
vector <wchar_t *> WordClass::quotedWords;
vector <wchar_t *> WordClass::periodWords;
unordered_map <wstring, tFI> WordClass::WMM;
unordered_map <wstring, vector <tIWMM>> WordClass::mainEntryMap;
bool WordClass::changedWords;
bool WordClass::inCreateDictionaryPhase;
int WordClass::disinclinationRecursionCount;

// maximum forms for one word to have (to guard against corruption)
#define MAX_FORMS 30
// get inflection for form - remember to prepend a space
// if there is no inflection , return an empty string
const wchar_t *getInflectionName(int inflection,tInflectionMap *map,wstring &temp)
{ LFS
  temp.clear();
  for (int I=0; map[I].num>=0; I++)
    if (map[I].num&inflection)
      temp=temp+L" "+map[I].name;
  return temp.c_str();
}

// only print the inflection appropriate for the form
const wchar_t *getInflectionName(int inflection,int form,wstring &temp)
{ LFS
  if (form<0 || (unsigned int)form>=Forms.size())
    return L"ILLEGAL FORM";
  if (Forms[form]->inflectionsClass==L"noun") return getInflectionName(inflection&NOUN_INFLECTIONS_MASK,nounInflectionMap,temp);
  if (Forms[form]->inflectionsClass==L"verb") return getInflectionName(inflection&VERB_INFLECTIONS_MASK,verbInflectionMap,temp);
  if (Forms[form]->inflectionsClass==L"adjective") return getInflectionName(inflection&ADJECTIVE_INFLECTIONS_MASK,adjectiveInflectionMap,temp);
  if (Forms[form]->inflectionsClass==L"adverb") return getInflectionName(inflection&ADVERB_INFLECTIONS_MASK,adverbInflectionMap,temp);
  if (Forms[form]->inflectionsClass==L"quotes") return getInflectionName(inflection&INFLECTIONS_MASK,quoteInflectionMap,temp);
  if (Forms[form]->inflectionsClass==L"brackets") return getInflectionName(inflection&INFLECTIONS_MASK,bracketInflectionMap,temp);
  return L"";
}

vector <FormClass *> Forms;

FormClass::FormClass(int indexIn,wstring nameIn,wstring shortNameIn,wstring inflectionsClassIn,bool hasInflectionsIn,
										 bool properNounSubClassIn,bool isTopLevelIn,bool isIgnoreIn,bool verbFormIn,bool blockProperNounRecognitionIn,bool formCheckIn)
{ LFS
  index=indexIn;
  name=nameIn;
  shortName=shortNameIn;
  inflectionsClass=inflectionsClassIn;
	hasInflections=hasInflectionsIn;
  properNounSubClass=properNounSubClassIn;
  isTopLevel=isTopLevelIn;
	isIgnore=isIgnoreIn;
	isVerbForm=verbFormIn;
  blockProperNounRecognition=blockProperNounRecognitionIn;
	formCheck=formCheckIn;
	isCommonForm=false;
	isNonCachedForm = false;
}

int FormsClass::findForm(wstring sForm)
{ LFS
unordered_map <wstring,int>::iterator fmi=formMap.find(sForm);
	return (fmi==formMap.end()) ? -1 : fmi->second;
}

int FormsClass::gFindForm(wstring sForm)
{ LFS
  int f=findForm(sForm);
  if (f<0)
    lplog(LOG_FATAL_ERROR,L"Unable to find form %s.",sForm.c_str());
  return f;
}

int FormsClass::addNewForm(wstring sForm,wstring shortForm,bool message,bool properNounSubClass)
{ LFS
unordered_map <wstring,int>::iterator fmi=formMap.find(sForm);
	if (fmi==formMap.end())
  {
    unsigned int chi;
    bool searchAgain=true;
    if (sForm.find(L"adjective")!=wstring::npos)
      sForm=L"adjective";
    else if (sForm.find(L"adverb")!=wstring::npos)
      sForm=L"adverb";
    else if ((chi=sForm.find(L"verb"))!=wstring::npos && !iswalpha(sForm[chi+1]))
      sForm=L"verb";
    else if (sForm.find(L"past part")!=wstring::npos)
      sForm=L"verb";
    else if (sForm.find(L"plural in construction")!=wstring::npos)
      sForm=L"noun";
    else if (sForm.find(L"exclamation")!=wstring::npos)  // from Cambridge
      sForm=L"interjection";
    else if (sForm.find(L"foreign term")!=wstring::npos)
      sForm=L"noun";
    else
      searchAgain=false;
    if (searchAgain)
      fmi=formMap.find(sForm);
    if (fmi==formMap.end())
    {
      if (message) 
			{
				lplog(LOG_ERROR,L"Create new form attempt:%s",sForm.c_str());
				return 0; // new forms are not allowed
			}
			FormClass *newForm=new FormClass(-1,sForm,shortForm,L"",true,properNounSubClass); // iForm removed
      Forms.push_back(newForm);
      changedForms=true;
			return formMap[sForm]=Forms.size()-1;
    }
  }
  unsigned int iForm=fmi->second;
  // if this is changed abbreviations will no longer have properNounSubClass turned on!
  if (!Forms[iForm]->properNounSubClass && properNounSubClass)
  {
    Forms[iForm]->properNounSubClass=properNounSubClass;
    changedForms=true;
  }
  return iForm;
}

int FormsClass::createForm(wstring sForm,wstring shortName,bool inflectionsFlag,wstring inflectionsClass,bool properNounSubClass)
{ LFS
  FormClass *newForm;
  size_t iForm;
  if ((iForm=findForm(sForm))!=-1)
  {
    newForm=Forms[iForm];
    newForm->shortName=shortName;
    newForm->inflectionsClass=inflectionsClass;
    newForm->properNounSubClass=properNounSubClass;
  }
  else
  {
		newForm=new FormClass(-1,sForm,shortName,inflectionsClass,inflectionsFlag,properNounSubClass);
    Forms.push_back(newForm);
    iForm=Forms.size()-1;
		formMap[sForm]=iForm;
  }
	return iForm;
}

WordClass Words;

// this is coming from either WordClass::addCopy OR addNewOrModify.  Therefore, insertNewForms flag must be added.
tFI::tFI(int iForm,int iInflectionFlags,int iFlags,int iTimeFlags,int iDerivationRules,tIWMM iMainEntry,int iSourceId)
{ LFS
  count=1;
  int oldAllocated=allocated;
  if (allocated<=fACount+1)
    formsArray=(unsigned int *)trealloc(18,formsArray,oldAllocated*sizeof(*formsArray),(allocated=(allocated+1)*2)*sizeof(*formsArray));
  formsOffset=fACount;
  formsArray[fACount++]=iForm;
  inflectionFlags=iInflectionFlags;
  derivationRules=iDerivationRules;
  flags=iFlags|tFI::insertNewForms;
  timeFlags=iTimeFlags;
  mainEntry=iMainEntry;
  index=-uniqueNewIndex++;
  tmpMainEntryWordId=-1;
  memset(usagePatterns,0,MAX_USAGE_PATTERNS);
  memset(usageCosts,0,MAX_USAGE_PATTERNS);
  memset(deltaUsagePatterns,0,MAX_USAGE_PATTERNS);
  memset(relationMaps,0,numRelationWOTypes*sizeof(*relationMaps));
  preferVerbPresentParticiple();
  sourceId=iSourceId;
  changedSinceLastWordRelationFlush=false;
	localWordIsCapitalized=0;
	localWordIsLowercase=0;
}


// this is from disk.  The source has already been flushed to the database.  Don't add insertNewForms flag.
tFI::tFI(char *buffer,int &where,int limit,wstring &ME,int iSourceId)
{ LFS
  count=*((int *)(buffer+where));
  if (count>MAX_FORMS)
		::lplog(LOG_FATAL_ERROR,L"Illegal count of %d encountered in read buffer at location %d (1).",count,where);
  int oldAllocated=allocated;
  if (fACount+count>=allocated)
    formsArray=(unsigned int *)trealloc(16,formsArray,oldAllocated*sizeof(*formsArray),(allocated=(allocated+count)*2)*sizeof(*formsArray));
  where+=sizeof(count);
  memcpy(formsArray+fACount,(int *)(buffer+where),count*sizeof(int));
  where+=count*sizeof(int);
  formsOffset=fACount;
  fACount+=count;
  if (!copy(inflectionFlags,buffer,where,limit) || 
      !copy(timeFlags,buffer,where,limit) ||
      !copy(flags,buffer,where,limit) ||
      !copy(derivationRules,buffer,where,limit) ||
      !copy(index,buffer,where,limit) ||
			!copy(ME,buffer,where,limit))
		::lplog(LOG_FATAL_ERROR,L"Illegal count of %d encountered in read buffer at location %d (2).",count,where);
  memcpy(usagePatterns,buffer+where,MAX_USAGE_PATTERNS);
  where+=MAX_USAGE_PATTERNS;
  memcpy(usageCosts,buffer+where,MAX_USAGE_PATTERNS);
  where+=MAX_USAGE_PATTERNS;
  memset(deltaUsagePatterns,0,MAX_USAGE_PATTERNS);
  memset(relationMaps,0,numRelationWOTypes*sizeof(*relationMaps));
  mainEntry=wNULL;
  index=-uniqueNewIndex++;
  tmpMainEntryWordId=-1;
  changedSinceLastWordRelationFlush=false;
  sourceId=iSourceId;
	numProperNounUsageAsAdjective = 0;
	localWordIsCapitalized = 0;
	localWordIsLowercase = 0;
	//preferVerbPresentParticiple();
}

bool tFI::retrieveWordFromDatabase(wstring &sWord, MYSQL &mysql, tFI &dbWordInfo, unordered_map <int, int> &dbUsagePatterns,int &dbMainEntryWordId)
{
	MYSQL_RES * result = NULL;
	if (!myquery(&mysql, L"LOCK TABLES words w READ, wordforms wf READ, words READ")) return false;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select w.inflectionFlags,w.flags,w.timeFlags,IF( EXISTS(SELECT id FROM words WHERE id = w.mainEntryWordId), mainEntryWordId, null) mainEntryWordId,w.derivationRules,w.sourceId,wf.formId,wf.count from words w,wordforms wf where wf.wordId=w.id and word = '%s'", sWord.c_str());
	MYSQL_ROW sqlrow = NULL;
	dbWordInfo.inflectionFlags=-1;
	if (myquery(&mysql, qt, result))
	{
		while (sqlrow = mysql_fetch_row(result))
		{
			if (dbWordInfo.inflectionFlags == -1)
			{
				dbWordInfo.inflectionFlags = atoi(sqlrow[0]);
				dbWordInfo.flags = atoi(sqlrow[1]);
				dbWordInfo.timeFlags = atoi(sqlrow[2]);
				dbMainEntryWordId = (sqlrow[3]) ? atoi(sqlrow[3]) : -1;
				dbWordInfo.derivationRules = atoi(sqlrow[4]);
				dbWordInfo.sourceId = (sqlrow[5]) ? atoi(sqlrow[5]) : -1;
			}
			int dbFormId = atoi(sqlrow[6]);
			int dbCount = atoi(sqlrow[7]);
			dbUsagePatterns[dbFormId] = dbCount;
		}
	}
	mysql_free_result(result);
	myquery(&mysql, L"UNLOCK TABLES");
	return dbWordInfo.inflectionFlags != -1;
}

int mTD(int p)
{
	return p + tFI::patternFormNumOffset - tFI::TRANSFER_COUNT;
}

// copies the same logic as transferDBUsagePatternsToUsagePattern
// instead of transferring some flat array read rfom the database into an internal usagePatterns,
// this transforms the counts in a map parameter.
// transferDBUsagePatternsToUsagePattern(128, UPDB, 0, iCount);
// transferDBUsagePatternsToUsagePattern(64, UPDB, SINGULAR_NOUN_HAS_DETERMINER, 2);
// transferDBUsagePatternsToUsagePattern(64, UPDB, VERB_HAS_0_OBJECTS, 3);
// usagePatterns[LOWER_CASE_USAGE_PATTERN] = max(127, UPDB[LOWER_CASE_USAGE_PATTERN]);
// usagePatterns[PROPER_NOUN_USAGE_PATTERN] = max(127, UPDB[PROPER_NOUN_USAGE_PATTERN]);
void tFI::computeDBUsagePatternsToUsagePattern(unordered_map <int, int> &dbUsagePatterns)
{
	LFS
	int highest = -1, highestPatternCount=128;
	for (auto const&[pattern, dbCount] : dbUsagePatterns)
		if (pattern< patternFormNumOffset)
			highest = max(highest, (signed)dbCount);
	if (highest >= highestPatternCount)
		// 600 8 0 -> highest=600   128  1 0
		for (auto &dbup: dbUsagePatterns)
			if (dbup.first < patternFormNumOffset)
				dbup.second = (highestPatternCount*((unsigned int)dbup.second) / highest);

	highestPatternCount = 64;
	if (dbUsagePatterns.find(mTD(SINGULAR_NOUN_HAS_DETERMINER)) != dbUsagePatterns.end())
	{
		highest = max(dbUsagePatterns[mTD(SINGULAR_NOUN_HAS_DETERMINER)], dbUsagePatterns[mTD(SINGULAR_NOUN_HAS_NO_DETERMINER)]);
		if (highest >= highestPatternCount)
		{
			dbUsagePatterns[mTD(SINGULAR_NOUN_HAS_DETERMINER)] = (highestPatternCount*((unsigned int)dbUsagePatterns[mTD(SINGULAR_NOUN_HAS_DETERMINER)]) / highest);
			dbUsagePatterns[mTD(SINGULAR_NOUN_HAS_NO_DETERMINER)] = (highestPatternCount*((unsigned int)dbUsagePatterns[mTD(SINGULAR_NOUN_HAS_NO_DETERMINER)]) / highest);
		}
	}
	if (dbUsagePatterns.find(mTD(VERB_HAS_0_OBJECTS)) != dbUsagePatterns.end())
	{
		highest = max(dbUsagePatterns[mTD(VERB_HAS_0_OBJECTS)], dbUsagePatterns[mTD(VERB_HAS_1_OBJECTS)]);
		highest = max(highest, dbUsagePatterns[mTD(VERB_HAS_2_OBJECTS)]);
		if (highest >= highestPatternCount)
		{
			dbUsagePatterns[mTD(VERB_HAS_0_OBJECTS)] = (highestPatternCount*((unsigned int)dbUsagePatterns[mTD(VERB_HAS_0_OBJECTS)]) / highest);
			dbUsagePatterns[mTD(VERB_HAS_1_OBJECTS)] = (highestPatternCount*((unsigned int)dbUsagePatterns[mTD(VERB_HAS_1_OBJECTS)]) / highest);
			dbUsagePatterns[mTD(VERB_HAS_2_OBJECTS)] = (highestPatternCount*((unsigned int)dbUsagePatterns[mTD(VERB_HAS_2_OBJECTS)]) / highest);
		}
	}
	// these two fields are set to 0 for each word (because the values should be kept for each source), so we must copy that logic here.
	//if (dbUsagePatterns.find(mTD(LOWER_CASE_USAGE_PATTERN)) != dbUsagePatterns.end() && dbUsagePatterns[mTD(LOWER_CASE_USAGE_PATTERN)] > 127)
	//	dbUsagePatterns[mTD(LOWER_CASE_USAGE_PATTERN)] = 127;
	//if (dbUsagePatterns.find(mTD(PROPER_NOUN_USAGE_PATTERN)) != dbUsagePatterns.end() && dbUsagePatterns[mTD(PROPER_NOUN_USAGE_PATTERN)] > 127)
	//	dbUsagePatterns[mTD(PROPER_NOUN_USAGE_PATTERN)] = 127;
	if (dbUsagePatterns.find(mTD(LOWER_CASE_USAGE_PATTERN)) != dbUsagePatterns.end())
		dbUsagePatterns[mTD(LOWER_CASE_USAGE_PATTERN)] = 0;
	if (dbUsagePatterns.find(mTD(PROPER_NOUN_USAGE_PATTERN)) != dbUsagePatterns.end())
		dbUsagePatterns[mTD(PROPER_NOUN_USAGE_PATTERN)] = 0;
	// this is never transferred in transferFormsAndUsage - should be on a per source level
	if (dbUsagePatterns.find(mTD(TRANSFER_COUNT)) != dbUsagePatterns.end())
		dbUsagePatterns[mTD(TRANSFER_COUNT)] = 0;
}


wstring tFI::patternString(int p)
{
	switch (p)
	{
		case TRANSFER_COUNT: return L"transfer count";
		case SINGULAR_NOUN_HAS_DETERMINER: return L"has determiner";
		case SINGULAR_NOUN_HAS_NO_DETERMINER: return L"no determiner";
		case VERB_HAS_0_OBJECTS: return L"0objects";
		case VERB_HAS_1_OBJECTS: return L"1object";
		case VERB_HAS_2_OBJECTS: return L"2objects";
		case LOWER_CASE_USAGE_PATTERN: return L"lower case";
		case PROPER_NOUN_USAGE_PATTERN: return L"proper noun";
		default: return L"UNKNOWN";
	};
}

bool tFI::write(void *buffer,int &where,int limit)
{ LFS
  if (!copy(buffer,(int)count,where,limit)) return false;
  memcpy(((char *)buffer)+where,forms(),count*sizeof(int));
	// must write unknown form first!
	if (query(UNDEFINED_FORM_NUM) > 0)
		::lplog(LOG_FATAL_ERROR, L"write ILLEGAL Unknown nonzero!");
  where+=count*sizeof(int);
  if (!copy(buffer,inflectionFlags,where,limit)) return false;
	if (!copy(buffer,timeFlags,where,limit)) return false;
  if (!copy(buffer,flags,where,limit)) return false;
  if (!copy(buffer,derivationRules,where,limit)) return false;
  if (!copy(buffer,index,where,limit)) return false;
	if (mainEntry == wNULL)
	{
		if (query(nounForm) >= 0 ||
			query(verbForm) >= 0 ||
			query(adjectiveForm) >= 0 ||
			query(adverbForm) >= 0)
		{
			::lplog(LOG_INFO,L"QueryOnAnyAppearance added to word because it has no mainEntry and has open class!");
			flags |= tFI::queryOnAnyAppearance;
		}
		wstring empty;
		if (!copy(buffer, empty, where, limit)) return false;
	}
	else
		if (!copy(buffer, mainEntry->first, where, limit)) return false;
	if (where+tFI::MAX_USAGE_PATTERNS*2>limit) 
		::lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached (4)!",limit);
  memcpy(((char *)buffer)+where,usagePatterns,MAX_USAGE_PATTERNS);
  where+=MAX_USAGE_PATTERNS;
  memcpy(((char *)buffer)+where,usageCosts,MAX_USAGE_PATTERNS);
  where+=MAX_USAGE_PATTERNS;
	return true;
}

// this is called from readWords, which reads the wordCache file.  So this has also been committed/processed already.  No insertNewForms flag.
bool tFI::updateFromDisk(char *buffer,int &where,int limit,wstring &ME)
{ LFS
  int iCount=*((int *)(buffer+where));
	if (iCount > MAX_FORMS)
		::lplog(LOG_FATAL_ERROR, L"Illegal count of %d encountered in read buffer at location %d (3).", count, where);
	int iFlags=buffer[where+sizeof(iCount)+iCount*sizeof(int)+sizeof(inflectionFlags)];
  if ((!(flags&queryOnLowerCase) && !(flags&queryOnAnyAppearance)) || ((iFlags&queryOnLowerCase) || (iFlags&queryOnAnyAppearance))) // update rejected.
  {
		where+=sizeof(count)+iCount*sizeof(int)+sizeof(inflectionFlags)+sizeof(timeFlags)+sizeof(flags)+sizeof(derivationRules)+sizeof(index);
		if (!copy(ME, buffer, where, limit))
			return false;
    where+=MAX_USAGE_PATTERNS*2;
    return false;
  }
  count=iCount;
  int oldAllocated=allocated;
  if (fACount+count>=allocated)
    formsArray=(unsigned int *)trealloc(16,formsArray,oldAllocated*sizeof(*formsArray),(allocated=(allocated+count)*2)*sizeof(*formsArray));
  where+=sizeof(count);
  memcpy(formsArray+fACount,(int *)(buffer+where),count*sizeof(int));
  where+=count*sizeof(int);
  formsOffset=fACount;
  fACount+=count;
  if (!copy(inflectionFlags,buffer,where,limit) ||
      !copy(timeFlags,buffer,where,limit) ||
      !copy(flags,buffer,where,limit) ||
      !copy(derivationRules,buffer,where,limit))
    ::lplog(LOG_FATAL_ERROR,L"buffer overrun encountered in read buffer at location %d.",where);
  where+=sizeof(index); // skip database index in word cache - we already have the correct index in memory, and the one in the word cache may be wrong.
  //copy(index,buffer,where);
  if (!copy(ME,buffer,where,limit)) return false;
  memcpy(usagePatterns,buffer+where,MAX_USAGE_PATTERNS);
  where+=MAX_USAGE_PATTERNS;
  memcpy(usageCosts,buffer+where,MAX_USAGE_PATTERNS);
  where+=MAX_USAGE_PATTERNS;
  memset(deltaUsagePatterns,0,MAX_USAGE_PATTERNS);
  mainEntry=wNULL;
  changedSinceLastWordRelationFlush=false;
  return true;
}

bool tFI::operator==(tFI &other) const
{ LFS
  bool identical=true;
  if (inflectionFlags!=other.inflectionFlags)
  {
    identical=false;
    wstring tmp,inflectionName,otherInflectionName;
    getInflectionName(inflectionFlags,shortNounInflectionMap,inflectionName);
    getInflectionName(inflectionFlags,shortVerbInflectionMap,tmp); inflectionName+=tmp;
    getInflectionName(inflectionFlags,shortAdjectiveInflectionMap,tmp); inflectionName+=tmp;
    getInflectionName(inflectionFlags,shortAdverbInflectionMap,tmp); inflectionName+=tmp;
    getInflectionName(other.inflectionFlags,shortNounInflectionMap,otherInflectionName);
    getInflectionName(other.inflectionFlags,shortVerbInflectionMap,tmp); otherInflectionName+=tmp;
    getInflectionName(other.inflectionFlags,shortAdjectiveInflectionMap,tmp); otherInflectionName+=tmp;
    getInflectionName(other.inflectionFlags,shortAdverbInflectionMap,tmp); otherInflectionName+=tmp;
    ::lplog(L"original inflectionFlags:%u:%s reduced inflectionFlags:%u:%s",inflectionFlags,inflectionName.c_str(),other.inflectionFlags,otherInflectionName.c_str());
  }
  //if (flags!=other.flags) return false;
  //if (mainEntry!=other.mainEntry) return false;
  for (unsigned int *I=formsArray+formsOffset; I<formsArray+formsOffset+count; I++)
    if (other.query(*I)==-1)
    {
      identical=false;
      wchar_t tmp[10];
      wstring original;
      for (unsigned int *J=formsArray+formsOffset; J<formsArray+formsOffset+count; J++)
        original=original+ wstring(L" ") + wstring(_itow(*J,tmp,10)) + L":" + wstring(Forms[*J]->name.c_str());
      wstring reduced;
      for (unsigned int *J=other.forms(); J<other.forms()+other.formsSize(); J++)
        if (*J>Forms.size())
          reduced=reduced+ wstring(L" ") + wstring(_itow(*J,tmp,10)) + L": *ERROR*";
        else
          reduced=reduced+ wstring(L" ") + wstring(_itow(*J,tmp,10)) + L":" + wstring(Forms[*J]->name.c_str());
      ::lplog(L"original forms:%s reduced forms:%s",original.c_str(),reduced.c_str());
    }
    return identical;
}

void tFI::setTopLevel(void)
{ LFS
  for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count; f!=fend; f++)
    if (Forms[*f]->isTopLevel)
		{
			flags|=tFI::topLevelSeparator;
			break;
		}
}

void tFI::removeIllegalForms(void)
{ LFS
	for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count; f!=fend; f++)
		if (((int)*f)<0 || *f>=Forms.size())
		{
			remove(*f);
      break;
		}
}

void tFI::setIgnore(void)
{ LFS
  for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count; f!=fend; f++)
    if (Forms[*f]->isIgnore)
		{
			flags|=tFI::ignoreFlag;
      return;
		}
	flags &= ~tFI::ignoreFlag;
}

int tFI::query(int form)
{ LFS
  for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count; f!=fend; f++)
    if (*f==form)
      return (int)(f-(formsArray+formsOffset));
  return -1;
}

bool tFI::hasWinnerVerbForm(int winnerForms)
{
	LFS
		for (unsigned int *f = formsArray + formsOffset, *fend = formsArray + formsOffset + count, I = 0; f != fend; f++, I++)
			if (Forms[*f]->isVerbForm && (!winnerForms || ((1 << I)&winnerForms) != 0))
				return true;
	return false;
}

bool tFI::hasVerbForm()
{
	LFS
		for (unsigned int *f = formsArray + formsOffset, *fend = formsArray + formsOffset + count, I = 0; f != fend; f++, I++)
			if (Forms[*f]->isVerbForm)
				return true;
	return false;
}

bool tFI::hasNounForm()
{
	LFS
		for (unsigned int *f = formsArray + formsOffset, *fend = formsArray + formsOffset + count, I = 0; f != fend; f++, I++)
			if (Forms[*f]->isNounForm)
				return true;
	return false;
}

int tFI::query(wstring sForm)
{ LFS
  int form;
  if ((form=FormsClass::findForm(sForm))<=0) return -1;
  return query(form);
}

int tFI::queryForSeparator(void)
{ LFS
  for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count; f!=fend; f++)
    if (Forms[*f]->isTopLevel)
      return (int)(f-(formsArray+formsOffset));
  return -1;
}

int tFI::lowestSeparatorCost()
{ LFS
  int lowestCost=10000;
  for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count,I=0; f!=fend; f++,I++)
    if (Forms[*f]->isTopLevel && usageCosts[I]<lowestCost)
      lowestCost=usageCosts[I];
  if (lowestCost==10000) return -1;
  return 1000*lowestCost;
}

bool tFI::isLowestCost(int form)
{ LFS
  int lowestCost=10000,formCost=-1;
  for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count,I=0; f!=fend; f++,I++)
  {
    if (usageCosts[I]<lowestCost)
      lowestCost=usageCosts[I];
    if (*f==form)
      formCost=usageCosts[I];
  }
  return lowestCost!=10000 && formCost!=-1 && formCost==lowestCost;
}

bool tFI::isRareWord(void)
{ LFS
  bool containsUndefined=false,containsCombination=false;
  for (unsigned int *f=formsArray+formsOffset,*fend=formsArray+formsOffset+count; f!=fend; f++)
  {
    if (*f==UNDEFINED_FORM_NUM) containsUndefined=true;
    else if (*f==COMBINATION_FORM_NUM) containsCombination=true;
    else if (*f!=nounForm && *f!=adjectiveForm && *f!=verbForm && *f!=adverbForm)
      return false;
  }
  return ((containsUndefined || containsCombination) &&
    usagePatterns[TRANSFER_COUNT]<=1 &&
    (inflectionFlags&(MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER))==0);
}

bool tFI::remove(wchar_t *formName)
{ LFS
  int form=FormsClass::findForm(formName);
  if (form<0)
    ::lplog(LOG_FATAL_ERROR,L"Form %s not found.",formName);
  return remove(form);
}

bool tFI::remove(int form)
{ LFS
  unsigned int *f,*f2,*fend,whichOffset;
  for (f=formsArray+formsOffset,f2=f,fend=f+count; f!=fend; f++)
  {
    if (f2!=f)
      *f2=*f;
    if (*f!=form)
      f2++;
    else
    {
      whichOffset=(int)(f-(formsArray+formsOffset));
      memmove(usagePatterns+whichOffset,usagePatterns+whichOffset+1,count-whichOffset-1);
      memmove(usageCosts+whichOffset,usageCosts+whichOffset+1,count-whichOffset-1);
      memmove(deltaUsagePatterns+whichOffset,deltaUsagePatterns+whichOffset+1,count-whichOffset-1);
    }
  }
  if (f2!=f)
  {
    count=(int)(f2-(formsArray+formsOffset));
    return true;
  }
  return false;
}

bool tFI::isProperNounSubClass(void)
{ LFS
  for (unsigned int c=formsOffset; c<formsOffset+count; c++)
    if (Forms[formsArray[c]]->properNounSubClass)
      return true;
  return false;
}

bool tFI::blockProperNounRecognition(void)
{ LFS
  for (unsigned int c=formsOffset; c<formsOffset+count; c++)
    if (Forms[formsArray[c]]->blockProperNounRecognition)
      return true;
  return false;
}

FormClass *tFI::Form(unsigned int offset)
{
	LFS
	return Forms[getFormNum(offset)];
}

int tFI::getFormNum(unsigned int offset)
{
	LFS
		if (offset == count)
			return PROPER_NOUN_FORM_NUM;
	if (offset > count)
	{
		::lplog(LOG_ERROR, L"ERROR:form offset %d is illegal (>%d)!", offset, count);
		return 0;
	}
	if ((unsigned)(formsOffset + offset) >= fACount)
	{ 
		::lplog(LOG_ERROR, L"ERROR:form offset %d is illegal (>%d)!", formsOffset + offset, fACount);
		return 0;
	}
	if (formsArray[formsOffset + offset] >= Forms.size())
	{
		::lplog(LOG_ERROR, L"ERROR:form offset %d does not exist!", formsArray[formsOffset + offset]);
		return 0;
	}
	return formsArray[formsOffset + offset];
}

void tFI::eraseForms(void)
{ LFS
  count=0;
  formsOffset=fACount;
}

void tFI::cloneForms(tFI fromWord)
{
  // make sure this has enough forms space
	if (count < fromWord.count)
	{
		int oldAllocated = allocated;
		if (allocated <= fACount + fromWord.count)
			formsArray = (unsigned int *)trealloc(19, formsArray, oldAllocated * sizeof(int), (allocated = (allocated + fromWord.count) * 2) * sizeof(int));
		// if not already at the end
		if (formsOffset + fromWord.count != fACount)
		{
			formsOffset = fACount;
			fACount += fromWord.count;
		}
	}
	memmove(formsArray + formsOffset, formsArray + fromWord.formsOffset, fromWord.count * sizeof(int));
	count = fromWord.count;
}

// this is a dynamic addForm routine, called from BNC routines, addForm or addNewOrModify.  insertNewForms flag is necessary.
int tFI::addForm(int form,const wstring &word)
{ LFS
  if (count>=MAX_FORM_USAGE_PATTERNS)
  {
    ::lplog(LOG_INFO,L"Adding form %s was rejected for word %s because it already has a maximum of %d forms.",
      (form<patternFormNumOffset) ? Forms[form]->name.c_str() : L"UsagePattern",word.c_str(),count);
    return 0;
  }
  int oldAllocated=allocated;
  if (allocated<=fACount+1)
    formsArray=(unsigned int *)trealloc(19,formsArray,oldAllocated*sizeof(int),(allocated=(allocated+1)*2)*sizeof(int));
  if (formsOffset+count!=fACount)
  {
	  if (allocated<=fACount+count+1)
		  formsArray=(unsigned int *)trealloc(19,formsArray,oldAllocated*sizeof(int),(allocated=(allocated+count+1)*2)*sizeof(int));
		memmove(formsArray+fACount,formsArray+formsOffset,count*sizeof(int));
		formsOffset=fACount;
		fACount+=count;
  }
	flags|=tFI::insertNewForms;
  count++;
	if (form || count==1)
		formsArray[fACount++]=form;
	else
	{
		// make SURE unknownform is the first form to make isUnknown work properly
		int saveForm = formsArray[formsOffset];
		formsArray[formsOffset] = 0;
		formsArray[fACount++] = saveForm;
		::lplog(LOG_FATAL_ERROR, L"Test this - %d!", formsArray[formsOffset]);
	}
	::lplog(L"XXADDFORM form %d added to word %s!", form, word.c_str());
  return 0;
}

int tFI::adjustFormsInflections(wstring originalWord,unsigned __int64 &wmflags,bool isFirstWord,int nounOwner,bool allCaps,bool firstLetterCapitalized, bool log)
{ LFS
  //flags=(1<<count)-1;
  wmflags=0;
  if (nounOwner==2) wmflags|=WordMatch::flagNounOwner;
  else if (nounOwner==1) wmflags|=WordMatch::flagPossiblePluralNounOwner;
  // If it is capitalized and not a letter and not proper noun subclass and (not first word or never seen uncapitalized):
  // allow only a proper noun form
  if (firstLetterCapitalized && !allCaps && originalWord[1] && query(numeralOrdinalForm)==-1 && query(numeralCardinalForm)==-1 && !isFirstWord)
  {
    //(!isFirstWord || (flags&WordClass::queryOnLowerCase))) took out 5/17/2006 sentence starts out with unknown verb 'Loving is a good thing.'
		//::lplog(L"%s:DEBUG PNU %d %d",originalWord.c_str(),numProperNounUsageAsAdjective,(int)usagePatterns[tFI::PROPER_NOUN_USAGE_PATTERN]);
    if (blockProperNounRecognition())
		{
			if (query(nounForm)==-1) // don't block pronouns that can be used like nouns (What company owned the Sago Mine?)
				wmflags|=WordMatch::flagOnlyConsiderOtherNounForms;
		}
		else
		{
			wmflags |= WordMatch::flagOnlyConsiderProperNounForms;
		}
  }
  // if it is capitalized and is not already a proper noun
  // and also contains at least one "noun" form, then add the proper noun form.
  if (firstLetterCapitalized || allCaps)
  {
    int costingOffset=-1;
    if (!preTaggedSource && originalWord.length()>0 && !blockProperNounRecognition() && formsSize()<tFI::MAX_USAGE_PATTERNS && // lord is a very special word!  It is not only an interjection, but can be used as a title.  But let's not block Lord as a proper noun because of that!
      //formsArray[formsOffset]!=UNDEFINED_FORM_NUM && // DR. BAURSTEIN (a chapter heading)
      query(PROPER_NOUN_FORM_NUM)==-1 &&
      // if a proper noun is not all caps, and not the first word, it probably needs to be a noun, abbreviation or proper noun sub-class.
      ((!isFirstWord && !allCaps) || (costingOffset=query(nounForm))!=-1 || (allCaps && originalWord==L"us") || // allCaps || query(adjectiveForm)!=-1 || // allCaps removed && adjectiveForm removed 11/2/2006 - too many possibilities generated for a long sentence of all caps
      (costingOffset=query(abbreviationForm))!=-1 || (costingOffset=query(sa_abbForm))!=-1 || isProperNounSubClass())) // more general in the future?
      // Cambridge is classified as an adjective by Websters
    {
      wmflags|=WordMatch::flagAddProperNoun;
      usageCosts[formsSize()]=(costingOffset<0) ? 0 : usageCosts[costingOffset];
#ifdef LOG_PATTERN_COST_CHECK
      ::lplog(L"Added ProperNoun to word %s (form #%d) at cost %d.",originalWord.c_str(),formsSize(),usageCosts[formsSize()]);
#endif
    }
    if (((wmflags&WordMatch::flagAddProperNoun) || query(PROPER_NOUN_FORM_NUM)!=-1) && !allCaps && !isFirstWord && usagePatterns[PROPER_NOUN_USAGE_PATTERN]<255)
    {
      usagePatterns[PROPER_NOUN_USAGE_PATTERN]++;
      deltaUsagePatterns[PROPER_NOUN_USAGE_PATTERN]++;
    }
		if (!isFirstWord && !allCaps)
		{
			localWordIsCapitalized++;
		}
    if (firstLetterCapitalized) 
			wmflags|=WordMatch::flagFirstLetterCapitalized;
    if (allCaps) 
			wmflags|=WordMatch::flagAllCaps;
  }
	else
	{
		if (usagePatterns[LOWER_CASE_USAGE_PATTERN] < 255)
		{
			usagePatterns[LOWER_CASE_USAGE_PATTERN]++;
			deltaUsagePatterns[LOWER_CASE_USAGE_PATTERN]++;
		}
		if (log)
			::lplog(LOG_INFO, L"Increased localWordIsLowercase from %d to %d!", localWordIsLowercase, localWordIsLowercase+1);
		localWordIsLowercase++;
	}
	if (log)
	{
		::lplog(L"lower case=%d (%d) proper noun=%d (%d) localWordIsCapitalized=%d localWordIsLowercase=%d",
			usagePatterns[LOWER_CASE_USAGE_PATTERN], deltaUsagePatterns[LOWER_CASE_USAGE_PATTERN],
			usagePatterns[PROPER_NOUN_USAGE_PATTERN], deltaUsagePatterns[PROPER_NOUN_USAGE_PATTERN],
			localWordIsCapitalized, localWordIsLowercase);
	}
	return 0;
}

bool tFI::isUnknown(void)
{ LFS
  return count>0 && formsArray[formsOffset]==UNDEFINED_FORM_NUM;
}

bool tFI::isCommonWord(void)
{
	LFS
		unsigned int fscount = formsSize();
	for (unsigned int f = 0; f < fscount; f++)
		if (Form(f)->isCommonForm) return true;
	return false;
}

bool tFI::isNonCachedWord(void)
{
	LFS
		unsigned int fscount = formsSize();
	for (unsigned int f = 0; f < fscount; f++)
		if (Form(f)->isNonCachedForm) return true;
	return false;
}

bool tFI::isSeparator(void)
{ LFS
  return (flags&topLevelSeparator)!=0;
}

bool tFI::isIgnore(void)
{ LFS
  return (flags&ignoreFlag)!=0;
}

void tFI::preferVerbPresentParticiple(void)
{ LFS
  int whichForm;
  if (inflectionFlags&VERB_PRESENT_PARTICIPLE)
  {
    // Webster's dictionary tends to create noun and adjective forms that have parallel meanings for verbs that
    // already will match to noun and adjective forms through the patterns.  We will increase the cost of these matchings.
    if ((whichForm=query(verbForm))>=0 && whichForm<MAX_FORM_USAGE_PATTERNS)
      usagePatterns[whichForm]=8;
    if ((whichForm=query(nounForm))>=0 && whichForm<MAX_FORM_USAGE_PATTERNS)
    {
      usageCosts[whichForm]++;
      usagePatterns[whichForm]=deltaUsagePatterns[whichForm]=4;
    }
    if ((whichForm=query(adjectiveForm))>=0 && whichForm<MAX_FORM_USAGE_PATTERNS)
    {
      usageCosts[whichForm]++;
      usagePatterns[whichForm]=deltaUsagePatterns[whichForm]=4;
    }
  }
  // prepositions can often be used as adverbs, but not when there is an object after them,
  // so if something can be a preposition or an adverb, we should discourage the adverb.
  if (query(prepositionForm)>=0 && (whichForm=query(adverbForm))>=0 &&
    whichForm<MAX_FORM_USAGE_PATTERNS)
  {
    for (unsigned int I=0; I<count; I++)
      usagePatterns[I]=deltaUsagePatterns[I]=8;
    usagePatterns[whichForm]=deltaUsagePatterns[whichForm]=4;
    usageCosts[whichForm]++;
  }
}

void tFI::transferUsagePatternsToCosts(int highestCost, unsigned int upStart, unsigned int upLength)
{
	LFS
		int highest = -1, lowest = 255, K = 0;
	for (unsigned int up = upStart; up < upStart + upLength; up++)
	{
		highest = max(highest, usagePatterns[up]);
		lowest = min(lowest, usagePatterns[up]);
	}
	if (lowest && (K = highest / lowest) < 2) return;
	if (highest < 5) return;
	if (!lowest || K > highestCost)
	{
		// 8 4 0 -> K=2  0 2 4
		// 16 2 2 -> K=8 0 4 4
		// example: word trust
		//wordId | formId | count  cost
		//  7139 |    100 |   886  0    noun (singular)                      (4*(886-886))/886==0
		//  7139 |    101 |    16  3    adjective                            (4*(886- 16))/886==3
		//  7139 |    102 |   418  2    verb (present tense first person)    (4*(886-418))/886==2
		for (unsigned int up = upStart; up < upStart + upLength; up++)
			usageCosts[up] = (highestCost*(highest - ((unsigned int)usagePatterns[up]))) / highest;
	}
	else
	{
		// 8 4 4 -> K=2  0 1 1
		for (unsigned int up = upStart; up < upStart + upLength; up++)
			usageCosts[up] = K - (((unsigned int)usagePatterns[up]) / lowest);
	}
}

#define FORM_USAGE_PATTERN_HIGHEST_COST 4 // set from 32 to 4 6/21/2006
// get highest value (I) and divide by the lowest value (J).  K=I/J.
// If K>FORM_USAGE_PATTERN_HIGHEST_COST, for every value L, L2=FORM_USAGE_PATTERN_HIGHEST_COST-(FORM_USAGE_PATTERN_HIGHEST_COST*L/I).
// if K<2, exit (no clear advantage).
// if K>=2 && K<=FORM_USAGE_PATTERN_HIGHEST_COST, for every value L, L2=K-(L/J).
void tFI::transferFormUsagePatternsToCosts(int sameNameForm,int properNounForm,int iCount)
{ LFS
  unsigned int topAllowableUsageCount=min(iCount,MAX_USAGE_PATTERNS);
  int highest=-1,lowest=255,K=0;
  for (unsigned int f=0; f<topAllowableUsageCount; f++)
    if (f!=sameNameForm && f!=properNounForm)
    {
      highest=max(highest,usagePatterns[f]);
      lowest=min(lowest,usagePatterns[f]);
    }
    if (lowest && (K=highest/lowest)<2) return;
    if (highest<5) return;
    if (!lowest || K>FORM_USAGE_PATTERN_HIGHEST_COST)
    {
      // 8 4 0 -> K=2  0 2 4
      // 16 2 2 -> K=8 0 4 4
      for (unsigned int f=0; f<topAllowableUsageCount; f++)
        if (f!=sameNameForm && f!=properNounForm)
				{
					double moreAccurateCost=(1.0*FORM_USAGE_PATTERN_HIGHEST_COST)-(FORM_USAGE_PATTERN_HIGHEST_COST*(1.0*usagePatterns[f])/highest);
					// round moreAccurateCost to nearest integer
					double nearestFloor=floor(moreAccurateCost);
					if (moreAccurateCost-nearestFloor>0.5)
						nearestFloor++;
          usageCosts[f]=(int)nearestFloor;
				}
    }
    else
    {
      // 8 4 4 -> K=2  0 1 1
      for (unsigned int f=0; f<topAllowableUsageCount; f++)
        if (f!=sameNameForm && f!=properNounForm)
          usageCosts[f]=K-(((unsigned int)usagePatterns[f])/lowest);
    }
    //preferVerbPresentParticiple();
}

void tFI::resetUsagePatternsAndCosts(wstring sWord)
{
	localWordIsCapitalized = 0;
	localWordIsLowercase = 0;
	numProperNounUsageAsAdjective = 0;
	for (unsigned int f = 0; f < MAX_USAGE_PATTERNS; f++)
	{
		usagePatterns[f] -= deltaUsagePatterns[f];
		deltaUsagePatterns[f] = 0;
	}
	int sameNameFormIndex = -1, properNounFormIndex = -1;
	for (unsigned int f = 0; f < count; f++)
	{
		if (Form(f)->name == sWord)
			sameNameFormIndex = f;
		else if (forms()[f] == PROPER_NOUN_FORM_NUM)
			properNounFormIndex = f;
	}
	transferFormUsagePatternsToCosts(sameNameFormIndex, properNounFormIndex, count);
	transferUsagePatternsToCosts(HIGHEST_COST_OF_INCORRECT_NOUN_DET_USAGE, SINGULAR_NOUN_HAS_DETERMINER, 2);
	transferUsagePatternsToCosts(HIGHEST_COST_OF_INCORRECT_VERB_USAGE, VERB_HAS_0_OBJECTS, 3);
}

void tFI::logReset(wstring sWord)
{
	if (deltaUsagePatterns[LOWER_CASE_USAGE_PATTERN] || deltaUsagePatterns[PROPER_NOUN_USAGE_PATTERN] || localWordIsCapitalized || localWordIsLowercase)
		::lplog(L"resetting word %s to lower case=%d (=%d-%d) and proper noun=%d (=%d-%d) and localWordIsCapitalized=%d and localWordIsLowercase=%d", sWord.c_str(),
			usagePatterns[LOWER_CASE_USAGE_PATTERN] - deltaUsagePatterns[LOWER_CASE_USAGE_PATTERN], usagePatterns[LOWER_CASE_USAGE_PATTERN], deltaUsagePatterns[LOWER_CASE_USAGE_PATTERN],
			usagePatterns[PROPER_NOUN_USAGE_PATTERN] - deltaUsagePatterns[PROPER_NOUN_USAGE_PATTERN], usagePatterns[PROPER_NOUN_USAGE_PATTERN], deltaUsagePatterns[PROPER_NOUN_USAGE_PATTERN],
			localWordIsCapitalized, localWordIsLowercase);
}

void tFI::resetCapitalizationAndProperNounUsageStatistics()
{
	usagePatterns[LOWER_CASE_USAGE_PATTERN] -= deltaUsagePatterns[LOWER_CASE_USAGE_PATTERN];
	deltaUsagePatterns[LOWER_CASE_USAGE_PATTERN] = 0;
	usagePatterns[PROPER_NOUN_USAGE_PATTERN] -= deltaUsagePatterns[PROPER_NOUN_USAGE_PATTERN];
	deltaUsagePatterns[PROPER_NOUN_USAGE_PATTERN] = 0;
	localWordIsCapitalized = 0;
	localWordIsLowercase = 0;
	numProperNounUsageAsAdjective = 0;
}

// used during matching, printing, updatePEMA (cost reduction), updating usage patterns
// if flagOnlyConsiderProperNounForms, cost should not be considered in matching, printing, or updating
// if there is a possibility of proper noun (flagAddProperNoun), consider cost in matching, printing and updating PEMA
//    AND if the winner form is not a proper noun, update usage patterns.
bool WordMatch::costable(void)
{ LFS
  if (preTaggedSource)
    return word->second.formsSize()!=1 && !(flags&WordMatch::flagAddProperNoun);
  else
    return (word->second.formsSize()!=1 && !(flags&(WordMatch::flagOnlyConsiderProperNounForms|WordMatch::flagOnlyConsiderOtherNounForms))) ||
    word->first==L"i"; // || word->second.blockProperNounRecognition();
}

void WordMatch::setSeparatorWinner(void)
{ LFS
  int offset=word->second.queryForSeparator();
  if (offset>=0) setWinner(offset);
}

bool WordMatch::isWinnerSeparator(void)
{ LFS
  int offset=word->second.queryForSeparator();
	return (offset>=0) ? isWinner(offset) : word->second.isSeparator(); 
}

bool WordMatch::maxWinner(int len,int avgCost,int lowestSeparatorCost)
{ LFS
  if (lowestSeparatorCost>=0 && avgCost>=lowestSeparatorCost) return false; // prevent patterns that match only separators optimally
  return avgCost<lowestAverageCost || (avgCost==lowestAverageCost && len>=maxLACMatch);
}

//   if 1 out of 2, add 1.
//   if 1 out of 3, add 2.  if 2 out of 3, add 1.
//   if 1 out of 4, add 3.  if 2 out of 4, add 2.  if 3 out of 4, add 1.
//   if overflow, divide all usage pattern counts by 2.
// CMREADME017
bool WordMatch::updateFormUsagePatterns(void)
{ LFS
  if (!costable() || !preTaggedSource) // these words are capitalized whenever they are used (honorifics and 'I')
  {
		word->second.incrementTransferCount();
    return false;
  }
	return word->second.updateFormUsagePatterns(tmpWinnerForms,word->first);
}

void tFI::logFormUsageCosts(wstring w)
{ LFS
  wstring formStr;
  for (unsigned I=0; I<count; I++)
  {
    // don't count forms that are named the same as the word itself
    if (Forms[formsArray[formsOffset+I]]->name==w)
      continue;
    formStr+=Forms[formsArray[formsOffset+I]]->name.c_str();
		formStr+=L":";
    wchar_t tmp[10];
    _itow(usageCosts[I],tmp,10);
    formStr+=tmp;
    if (I<count-1) formStr+=L" ";
  }
  ::lplog(L"%s (%s)",w.c_str(),formStr.c_str());
}

tIWMM WordClass::addCopy(wstring sWord,tIWMM iWord,bool &added)
{ LFS
  tIWMM iWordCopy;
  int start=0;
  added=false;
  if ((iWordCopy=query(sWord))==WMM.end())
  {
    pair < tIWMM, bool > pr;
    pr=WMM.insert(tWFIMap(sWord,tFI(iWord->second.forms()[0],iWord->second.inflectionFlags,iWord->second.flags,iWord->second.timeFlags,iWord->second.derivationRules,iWord->second.mainEntry,iWord->second.sourceId)));
    added=pr.second;
    iWordCopy=pr.first;
    start=1; // already inserted the first form
  }
  for (unsigned int f=start; f<iWord->second.formsSize(); f++)
    if (iWordCopy->second.query(iWord->second.forms()[f])<0)
      iWordCopy->second.addForm(iWord->second.forms()[f],sWord);
  // derivation rules of variant may not apply to copy
  //iWordCopy->second.derivationRules=iWord->second.derivationRules;
  // flags like queryOnLowerCase should not be propagated to copy, which is already being queried
  //iWordCopy->second.flags=iWord->second.flags;
  iWordCopy->second.timeFlags|=iWord->second.timeFlags;
  iWordCopy->second.inflectionFlags|=iWord->second.inflectionFlags;
  if (iWordCopy->second.mainEntry==wNULL)
    iWordCopy->second.mainEntry=iWord->second.mainEntry;
  return iWordCopy;
}

tIWMM WordClass::query(wstring sWord,int form,int inflection,int &offset)
{ LFS
  for (unsigned int I=0; I<sWord.length(); I++) sWord[I]=towlower(sWord[I]);
  tIWMM iWMM;
  // make sure not already there
  offset=-1;
  if ((iWMM=WMM.find(sWord))!=WMM.end())
  {
    if ((iWMM->second.inflectionFlags&inflection)==inflection || !inflection)
      offset=iWMM->second.query(form);
  }
  return iWMM;
}

bool WordClass::remove(wstring sWord)
{ LFS
  vector<wchar_t *>::iterator index;
  if (wcschr(sWord.c_str(),L' '))
  {
    if ((index=lower_bound(multiElementWords.begin(),multiElementWords.end(),sWord))!=multiElementWords.end() && sWord==*index)
			multiElementWords.erase(index);
  }
  if (wcschr(sWord.c_str(),L'\'') || wcschr(sWord.c_str(), L'ʼ'))
  {
    if ((index=lower_bound(quotedWords.begin(),quotedWords.end(),sWord))!=quotedWords.end() && sWord==*index)
			quotedWords.erase(index);
  }
  if (wcschr(sWord.c_str(),L'.'))
  {
    if ((index=lower_bound(periodWords.begin(),periodWords.end(),sWord))!=periodWords.end() && sWord==*index)
			periodWords.erase(index);
  }
  /*
  if (wcschr(sWord.c_str(),'-'))
  {
  for (sw=dashWords.begin(),swend=dashWords.end(); sw!=swend && *sw!=sWord; sw++);
  if (sw!=swend) dashWords.erase(sw);
  }
  */
  tIWMM iWMM;
  if ((iWMM=WMM.find(sWord))!=WMM.end())
  {
    WMM.erase(iWMM);
    changedWords=true;
    return true;
  }
  return false;
}

void WordClass::resetUsagePatternsAndCosts()
{
	for (tIWMM w = begin(), wEnd = end(); w != wEnd; )
	{
		if (w->second.flags&tFI::deleteWordAfterSourceProcessing)
			w = WMM.erase(w);
		else
		{
			w->second.resetUsagePatternsAndCosts(w->first);
			w++;
		}
	}
}

void WordClass::resetCapitalizationAndProperNounUsageStatistics()
{
	// mainEntry processing must be completed before any words are deleted.
	for (tIWMM w = begin(), wEnd = end(); w != wEnd; w++)
	{
		if (w->second.mainEntry != wNULL && (w->second.mainEntry->second.flags&tFI::deleteWordAfterSourceProcessing) && !(w->second.flags&tFI::deleteWordAfterSourceProcessing))
		{
			lplog(LOG_INFO, L"Removing deletion of word %s because it is a main entry of another word which is not going to be deleted.", w->second.mainEntry->first.c_str());
			w->second.mainEntry->second.flags &= ~tFI::deleteWordAfterSourceProcessing;
		}
		if ((w->second.flags&tFI::deleteWordAfterSourceProcessing) && w->second.sourceId<0)
		{
			lplog(LOG_INFO, L"Removing deletion of word %s because it is has a NULL sourceId.", w->first.c_str());
			w->second.flags &= ~tFI::deleteWordAfterSourceProcessing;
		}
	}
	for (tIWMM w = begin(), wEnd = end(); w != wEnd; )
	{
		if (w->second.flags&tFI::deleteWordAfterSourceProcessing)
		{
			lplog(LOG_INFO, L"Deleting word %s post source processing", w->first.c_str());
			w = WMM.erase(w);
		}
		else
		{
			w->second.logReset(w->first);
			w->second.resetCapitalizationAndProperNounUsageStatistics();
			w++;
		}
	}
}

bool WordClass::removeInflectionFlag(wstring sWord,int flag)
{ LFS
  tIWMM iWMM;
  if ((iWMM=WMM.find(sWord))!=WMM.end() && (iWMM->second.inflectionFlags&flag)==flag)
  {
    iWMM->second.inflectionFlags&=~flag;
    changedWords=true;
    return true;
  }
  return false;
}

bool WordClass::addInflectionFlag(wstring sWord,int flag)
{ LFS
  tIWMM iWMM;
  if ((iWMM=WMM.find(sWord))!=WMM.end() && (iWMM->second.inflectionFlags&flag)!=flag)
  {
    iWMM->second.inflectionFlags|=flag;
    changedWords=true;
    return true;
  }
  return false;
}

tIWMM WordClass::addNewOrModify(MYSQL *mysql, wstring sWord, int flags, int form, int inflection, int derivationRules, wstring sME, int sourceId, bool &added)
{
	LFS
  bool firstLetterCapitalized=sWord[0]>0 && iswupper(sWord[0])!=0;
  for (unsigned int I=0; I<sWord.length(); I++) sWord[I]=towlower(sWord[I]);
  int offset;
  tIWMM iWMM=query(sWord,form,inflection,offset);
  if (!(added=(iWMM==WMM.end() || offset==-1 ||
    ((iWMM->second.inflectionFlags&inflection)!=inflection) ||
    ((iWMM->second.flags&flags)!=flags) ||
    iWMM->second.query(form)<0))) return iWMM; // already there
  if (sWord[0]<'0' || sWord[0]>'9')
    changedWords=true;  // if a digit, this is a number which will not be written out to disk anyway.
  tIWMM mainEntry=wNULL; // bool isFirstWord,bool firstLetterCapitalized,int nounOwner)
  if (iWMM==WMM.end() || iWMM->second.mainEntry ==wNULL)
  {
    for (unsigned int I=0; I<sME.length(); I++) sME[I]=towlower(sME[I]);
		disinclinationRecursionCount++;
    if (sME[0] && !equivalentIfIgnoreDashSpaceCase(sME,sWord) && (parseWord(mysql,sME,mainEntry,firstLetterCapitalized,false, sourceId) || mainEntry==wNULL))
    {
			disinclinationRecursionCount--;
      lplog(LOG_ERROR,L"ERROR:addNewOrModify failed finding %s as a main entry.",sME.c_str());
      return wNULL;
    }
		disinclinationRecursionCount--;
  }
  if (iWMM==WMM.end())
  {
    pair< tIWMM, bool > pr;
    pr=WMM.insert(tWFIMap(sWord,tFI(form,inflection,flags,0,derivationRules,mainEntry,sourceId)));
    if (Forms[form]->isTopLevel)
      pr.first->second.flags|=tFI::topLevelSeparator;
    if (equivalentIfIgnoreDashSpaceCase(sME,sWord)) pr.first->second.mainEntry=pr.first;
		if (mainEntry!=wNULL)
			mainEntryMap[mainEntry->first].push_back(pr.first);
		else
			pr.first->second.mainEntry = pr.first;
    return pr.first;
  }
  else
  {
    if (iWMM->second.query(form)<0) iWMM->second.addForm(form,sWord);
    iWMM->second.inflectionFlags|=inflection;
        // flags&=~tFI::queryOnAnyAppearance; // don't query if not a new word. - 5/3/2007 no way to tell...
    iWMM->second.flags|=flags|tFI::updateMainInfo;
    if (iWMM->second.mainEntry ==wNULL)
    {
      if (mainEntry==wNULL)
        iWMM->second.mainEntry=iWMM;
      else
        iWMM->second.mainEntry = mainEntry;
    }
		if (mainEntry!=wNULL)
			mainEntryMap[mainEntry->first].push_back(iWMM);
  }
  return iWMM;
}

tIWMM WordClass::query(wstring sWord)
{ LFS
  wcslwr((wchar_t *)sWord.c_str());
  // make sure not already there
  return WMM.find(sWord);
}

int WordClass::addWordToForm(wstring sWord,tIWMM &iWord,int flag,wstring sForm,wstring shortForm,int inflection,int derivationRules,wstring mainEntry,int sourceId,bool &added)
{ LFS
  unsigned int iForm=FormsClass::addNewForm(sForm,shortForm,true);
  iWord=addNewOrModify(NULL, sWord,flag,iForm,inflection,derivationRules,mainEntry,sourceId,added);
  return iForm;
}

bool WordClass::handleExtendedParseWords(wchar_t *word)
{ LFS
  if (!word[1]) return false;
  wchar_t *ch;
	vector <wchar_t *>::iterator index;
	if (wcschr(word,L' ') || wcschr(word, L'-') || wcschr(word, L'—'))
	{
		if (multiElementWords.size() && loosesort( multiElementWords[multiElementWords.size()-1], word ))
			index=multiElementWords.end();
		else
			index=lower_bound(multiElementWords.begin(),multiElementWords.end(),word,loosesort);
    multiElementWords.insert(index,wcsdup(word));
	}
  else if (wcschr(word,L'\''))
	{
		if (quotedWords.size() && loosesort( quotedWords[quotedWords.size()-1], word ))
			index=quotedWords.end();
		else
			index=lower_bound(quotedWords.begin(),quotedWords.end(),word,loosesort);
    quotedWords.insert(index,wcsdup(word));
	}
  else if (ch=wcschr(word,L'.'))
  {
    if (ch-word!=wcslen(word)-1) 
		{// the abbreviation must not have a period ONLY at the end!
			if (periodWords.size() && loosesort( periodWords[periodWords.size()-1], word ))
				index=periodWords.end();
			else
				index=lower_bound(periodWords.begin(),periodWords.end(),word,loosesort);
		  periodWords.insert(index,wcsdup(word));
		}
    else
    {
      //lplog(L"abbrevation word %s removed (will never be used)",word);
			//if (removalAllowed) // caused problems with usage of idToMap in mySQL.cpp, and most of the time is incorrect
			// because this routine is used for words that are predefined and so should not be deleted.
			//	return remove(word);
    }
  }
	return false;
}

int WordClass::markWordUndefined(tIWMM &iWord,wstring sWord,int flag,bool firstWordCapitalized,int nounOwner,int sourceId)
{ LFS
  bool added;
  tIWMM iW;
  addWordToForm(sWord,iWord,flag,UNDEFINED_FORM,L"",0,0,L"",sourceId,added);
  //if (iswupper(sWord[0])) // this is already done by other code
  //  addWordToForm(sWord,iW,flag,PROPER_NOUN_FORM,"",0,0,"",added);
  addWordToForm(sWord,iW,flag,L"noun",L"",SINGULAR,0,L"",sourceId,added);
	if (!nounOwner && !firstWordCapitalized)
  {
    if (sWord.length()>3 && sWord.substr(sWord.length()-3,3)==L"ing")
      addWordToForm(sWord,iW,flag,L"verb",L"",VERB_PRESENT_PARTICIPLE,0,L"",sourceId,added);
    else
    {
      addWordToForm(sWord,iW,flag,L"verb",L"",VERB_PAST,0,L"",sourceId,added);
      addWordToForm(sWord,iW,flag,L"verb",L"",VERB_PRESENT_FIRST_SINGULAR,0,L"",sourceId,added);
    }
    addWordToForm(sWord,iW,flag,L"adverb",L"",ADVERB_NORMATIVE,0,L"",sourceId,added);
    addWordToForm(sWord,iW,flag,L"adjective",L"",ADJECTIVE_NORMATIVE,0,L"",sourceId,added);
  }
  if (sWord.find_first_of(L"aeiou")==wstring::npos)
    addWordToForm(sWord,iW,flag,L"abbreviation",L"",0,0,L"",sourceId,added);
  if (!firstWordCapitalized) lplog(LOG_DICTIONARY,L"ERROR:Word %s was not found",sWord.c_str());
  return 0;
}

bool WordClass::isAllUpper(wstring &sWord)
{ LFS
  bool allNonAlpha=true;
  const wchar_t *ch;
  for (ch=sWord.c_str(); *ch; ch++)
    if (iswalpha(*ch))
    {
      allNonAlpha=false;
      if (iswlower(*ch)) return false;
    }
  return !allNonAlpha;
}

tIWMM WordClass::hasFormInflection(tIWMM iWord,wstring sForm,int inflection)
{ LFS
  if (iWord==WMM.end()) return iWord;
  int form=FormsClass::findForm(sForm);
  if (form>=0 && iWord->second.query(form)>=0 && (!inflection || (iWord->second.inflectionFlags&inflection)==inflection)) return iWord;
  if (form<0)
    lplog(LOG_ERROR,L"Dictionary lookup unable to find form %s.",sForm.c_str());
  return WMM.end();
}

// find sWord with prefix or suffix.
// if prefix, add all forms of base word to iWord.
// if suffix, add specific suffix form to iWord.
int WordClass::attemptDisInclination(MYSQL *mysql, tIWMM &iWord, wstring sWord, int sourceId)
{
	LFS
	if (mysql == NULL)
		return WORD_NOT_FOUND;
	vector <Stemmer::tSuffixRule> rulesUsed;
	intArray iaTrail;
	wstring inflectionName;
	if (Stemmer::stem(*mysql, sWord.c_str(), rulesUsed, iaTrail, -1))
	{
		vector <Stemmer::tSuffixRule>::iterator r;
		wstring trail;
		if (logDetail)
			for (r = rulesUsed.begin(); r != rulesUsed.end(); r++)
				lplog(LOG_DICTIONARY, L"stem rules %s%d on %s lead to %s form %s inflection%s",
					r->trail.concatToString(trail).c_str(), r->rulenum, sWord.c_str(), r->text.c_str(), r->form.c_str(), (r->inflection <= 0 || r->form == L"PREVIOUS") ? L" (None)" : getInflectionName(r->inflection, FormsClass::gFindForm(r->form), inflectionName));
		for (r = rulesUsed.begin(); r != rulesUsed.end(); r++)
		{
			if (iswdigit(r->text[r->text.length() - 1]) && iswdigit(r->text[r->text.length() - 2]))
				continue;
			// if stem is not found in memory and not in any dictionary, continue */
			tIWMM saveIWord=fullQuery(mysql, r->text, sourceId);
			// must not itself be undefined
			if (saveIWord == WMM.end() || saveIWord->second.query(UNDEFINED_FORM_NUM) >= 0)
				continue;
			/* make sure at least one form is one of the unlimited classes of words: adjective, adverb, noun, verb */
			int acceptClasses[] = { nounForm,verbForm,adjectiveForm,adverbForm, NULL };
			int a;
			for (a = 0; acceptClasses[a]; a++)
				if (saveIWord->second.query(acceptClasses[a]) >= 0)
					break;
			if (!acceptClasses[a]) continue;
			// if r->rulenum is 0, then PREFIX (not suffix) has been removed from original
			// if PREFIX, forms and inflections have already been added.
			// only when a SUFFIX does another form and inflection get imposed onto the stem's form and inflection.
			/* suffix only - prefix processing does not write anything */
			wstring form;
			int inflection = -1;
			Stemmer::findLastFormInflection(rulesUsed, r, form, inflection);
			int derivationRules = r->trail.encode();
			if (form != L"ORIGINAL")
			{
				// suffix
				if (!form[0])
				{
					lplog(LOG_DICTIONARY, L"ERROR:Suffix rule #%d with word %s (root %s) has no suffix form.", r->rulenum, sWord.c_str(), r->text.c_str());
					return WORD_NOT_FOUND;
				}
				if (inflection < 0)
				{
					lplog(LOG_DICTIONARY, L"ERROR:Suffix rule #%d with word %s (root %s) has no suffix inflection.", r->rulenum, sWord.c_str(), r->text.c_str());
					return SUFFIX_HAS_NO_INFLECTION;
				}
				checkAdd(L"Stem", iWord, sWord, 0, form, inflection, derivationRules, r->text, sourceId);
				lplog(LOG_DICTIONARY, L"WordPosMAP attemptDisInclination %s-->%s (%s)", sWord.c_str(), r->text.c_str(), form.c_str());
			}
			else
			{
				// prefix - copy only forms accepting prefix found in saveIWord to iWord
				for (a = 0; acceptClasses[a]; a++)
					if (saveIWord->second.query(acceptClasses[a]) >= 0)
					{
						checkAdd(L"Original", iWord, sWord, 0, Forms[acceptClasses[a]]->name.c_str(), saveIWord->second.inflectionFlags, derivationRules, r->text, sourceId);
						lplog(LOG_DICTIONARY, L"WordPosMAP attemptDisInclination %s-->%s (%s)", sWord.c_str(), r->text.c_str(), Forms[acceptClasses[a]]->name.c_str());
					}
			}
			return 0;
		}
	}
	return WORD_NOT_FOUND;
}

// returns a 0 if no error.  
// tries to find a word in the dictionary, in the DB, by stemming or splitting the word. 
// if the word is not found this still returns 0, but it will add all open word classes and mark the word as unknown.
int WordClass::parseWord(MYSQL *mysql, wstring sWord, tIWMM &iWord, bool firstLetterCapitalized, int nounOwner, int sourceId)
{
	LFS
	bool stopDisInclination = disinclinationRecursionCount>2;
	int ret= WORD_NOT_FOUND;
	bool wordComplete, dontMarkUndefined=false;
	if (wordComplete = (iWord = query(sWord)) != WMM.end())
	{
		if (!mysql) // can get here calling parseWord on a mainEntry as well.
			return 0;
		dontMarkUndefined = true;
		if (!firstLetterCapitalized && (iWord->second.flags&tFI::queryOnLowerCase) == tFI::queryOnLowerCase)
		{
			changedWords = true;
			iWord->second.flags &= ~tFI::queryOnLowerCase;
			iWord->second.flags |= tFI::updateMainInfo;
			//iWord->second.remove(PROPER_NOUN_FORM_NUM);
			wordComplete = false;
			if (iWord->second.isUnknown())
			{
				iWord->second.eraseForms();
				dontMarkUndefined = false;
			}
		}
		else if (iWord != end() && (iWord->second.flags&tFI::queryOnAnyAppearance) && inCreateDictionaryPhase == false)
		{
			changedWords = true;
			iWord->second.flags &= ~tFI::queryOnAnyAppearance;
			iWord->second.flags |= tFI::updateMainInfo;
			if (firstLetterCapitalized)
			{
				// this assumes a predefined word.  If encountered in an upper case, previously
				// this would go on and mark it undefined, like a normal Proper Noun.  However,
				// this is NOT undefined, because it already has a definition by virtue of it
				// having been predefined.
				iWord->second.flags |= tFI::queryOnLowerCase;
				return 0;
			}
			stopDisInclination = true;
			wordComplete = false;
			if (iWord->second.isUnknown())
			{
				iWord->second.eraseForms();
				dontMarkUndefined = false;
			}
		}
	}
	if (!wordComplete)
	{
		// if word is not already in words array, and mysql is 0 (which is from read a source cache file),
		// then the wordcache file was incorrectly written.
		// examine the logic in writeWords
		if (!mysql)
		{
			//wstring forms;
			//for (unsigned int f = 0; f < iSave->second.formsSize(); f++)
				//forms += iSave->second.Form(f)->name + L" ";
			lplog(LOG_INFO, L"Word not found on source read [%s].", sWord.c_str());
			return WORD_NOT_FOUND;
		}
		bool containsSingleQuote = false;
		int dashLocation = -1;
		if (sWord.length()>1 && !iswdigit(sWord[0]))
			for (int I = sWord.length() - 1; I >= 0; I--)
			{
				if (!iswalnum(sWord[I]) && sWord[I] != ' ' && sWord[I] != '.' && !isDash(sWord[I]) &&
					((sWord[0] != 'd' && sWord[0] != 'l') || !isSingleQuote(sWord[1])))
					lplog(LOG_DICTIONARY, L"Suspect character %c(ascii value %d) encountered in word %s.", sWord[I], (int)sWord[I], sWord.c_str());
				if (isDash(sWord[I]))
					dashLocation = I;
				if (isSingleQuote(sWord[I]))
					containsSingleQuote = true;
			}
		/* (s) check examples: finger(s) member(s) for now, chop off the (s) */
		int len = sWord.length() - 3;
		if (len>=0 && sWord[len] == '(' && sWord[len + 1] == 's' && sWord[len + 2] == ')')
			sWord.erase(len, 3);
		if (!iswalnum(sWord[0]) && !sWord[1] && sWord[0])
		{
			if (isDash(sWord[0]))
			{
				lplog(LOG_FATAL_ERROR, L"Dash should already exist!");
				//bool added;
				//iWord=addNewOrModify(NULL, sWord, 0, dashForm, 0, 0, sWord, -1, added);
			}
			else
				iWord = predefineWord((wchar_t *)sWord.c_str());
			return 0;
		}
		// search sql DB for word
		int wordId = -1;
		if (iWord == WMM.end() && findWordInDB(mysql, sWord, wordId, iWord) && !iWord->second.isUnknown())
			return 0;
		bool newWordIsUnknown = wordId != -1;
		if (firstLetterCapitalized)
		{
			if (dontMarkUndefined)
				return 0;
			markWordUndefined(iWord, sWord, tFI::queryOnLowerCase, firstLetterCapitalized, nounOwner, sourceId);
		}
		// make some attempt at getting past French words like d'affaires l'etat etc
		else if (sWord.length() > 1 && sWord[1] == '\'' && (sWord[0] == 'd' || sWord[0] == 'l'))
		{
			if (dontMarkUndefined)
				return 0;
			markWordUndefined(iWord, sWord, 0, firstLetterCapitalized, nounOwner, sourceId);
		}
		else 
		{
			// search online dictionaries for word
			if ((ret = getForms(mysql, iWord, sWord, sourceId,false)) && ret != WORD_NOT_FOUND && ret != NO_FORMS_FOUND) // getForms found word (ret>0)
				return 0;
			if (stopDisInclination) return 0;
			if (containsSingleQuote)
			{
				if (dontMarkUndefined)
					return 0;
				markWordUndefined(iWord, sWord, 0, firstLetterCapitalized, nounOwner, sourceId);
			}
			else if (firstLetterCapitalized) // don't try stemming or splitting if word is capitalized, but mark as reinvestigate if every encountered in lower case.
			{
				if (dontMarkUndefined)
					return 0;
				markWordUndefined(iWord, sWord, tFI::queryOnLowerCase, firstLetterCapitalized, nounOwner, sourceId);
			}
			else if (ret = attemptDisInclination(mysql,iWord, sWord, sourceId)) // returns 0 if found or WORD_NOT_FOUND if not found
			{
				if (dashLocation < 0 && ret != WORD_NOT_FOUND && ret != NO_FORMS_FOUND)
					return ret;
				ret = splitWord(mysql,iWord, sWord, sourceId);
				if (ret && dashLocation>=0)
				{
					wstring sWordNoDashes = sWord;
					sWordNoDashes.erase(std::remove(sWordNoDashes.begin(), sWordNoDashes.end(), L'-'), sWordNoDashes.end());
					sWordNoDashes.erase(std::remove(sWordNoDashes.begin(), sWordNoDashes.end(), L'—'), sWordNoDashes.end());
					tIWMM tiWord;
					if ((tiWord = Words.query(sWordNoDashes)) == Words.end() && !findWordInDB(mysql, sWordNoDashes, wordId, tiWord))
					{
						// all words found on a conditional that they are searched for only if lower case must be deleted after the source is finished
						// because when a single run parses multiple sources, if a capitalized version of the word is the only kind in a source, after the lower case version is found in a previous source,
						// the parse for the capitalized version is different than if the lower case version was never encountered previously, leading to 
						// a dependency on order of capitalization.  To get rid of this dependency and ensure that parses are identical no matter what order, this word must be deleted after source processing.
						if (!(ret = Words.attemptDisInclination(mysql, iWord, sWordNoDashes, sourceId))) // returns 0 if found or WORD_NOT_FOUND if not found
							iWord->second.flags |= tFI::deleteWordAfterSourceProcessing; 
					}
					else
					{
						if (iWord == end())
						{
							markWordUndefined(iWord, sWord, 0, firstLetterCapitalized, nounOwner, sourceId);
							iWord->second.mainEntry = tiWord;
						}
						iWord->second.cloneForms(tiWord->second);
						ret = 0; // don't mark undefined!
						iWord->second.flags |= tFI::deleteWordAfterSourceProcessing;
					}
				}
				if (ret && !dontMarkUndefined)
					ret = markWordUndefined(iWord, sWord, 0, firstLetterCapitalized, nounOwner, sourceId);
			}
			else if (newWordIsUnknown)
				iWord->second.flags |= tFI::deleteWordAfterSourceProcessing;

		}
		if (iWord!=WMM.end())
			iWord->second.preferVerbPresentParticiple();
	}
	return 0;
}

// 
int WordClass::parseWord(MYSQL *mysql,wstring sWord, tIWMM &iWord)
{
	LFS
	bool firstLetterCapitalized = false;
	int sourceId=-1;
	if ((iWord = query(sWord)) != WMM.end())
		return 0;
	bool containsSingleQuote = false;
	int dashLocation = -1;
	if (sWord.length()>1 && !iswdigit(sWord[0]))
		for (int I = sWord.length() - 1; I >= 0; I--)
		{
		if (!iswalnum(sWord[I]) && sWord[I] != ' ' && sWord[I] != '.' && !isDash(sWord[I]) &&
			((sWord[0] != 'd' && sWord[0] != 'l') || sWord[1] != '\''))
			lplog(LOG_DICTIONARY, L"Suspect character %c(ascii value %d) encountered in word %s.", sWord[I], (int)sWord[I], sWord.c_str());
		if (isDash(sWord[I]))
			dashLocation = I;
		if (sWord[I] == '\'')
			containsSingleQuote = true;
		}
	/* (s) check examples: finger(s) member(s) for now, chop off the (s) */
	int len = sWord.length() - 3;
	if (sWord[len] == '(' && sWord[len + 1] == 's' && sWord[len + 2] == ')')
		sWord.erase(len, 3);
	if (!iswalnum(sWord[0]) && !sWord[1] && sWord[0])
		return WORD_NOT_FOUND;
	if (firstLetterCapitalized)
		return WORD_NOT_FOUND;
	// make some attempt at getting past French words like d'affaires l'etat etc
	if (sWord[1] == '\'' && (sWord[0] == 'd' || sWord[0] == 'l'))
		return WORD_NOT_FOUND;
	if (containsSingleQuote)
		return WORD_NOT_FOUND;
	return attemptDisInclination(mysql,iWord, sWord, sourceId);
}


// any words that have punctuation (iswpunct) must be included here.
// also all section headers.
#define MAX_MULTI_WORD_LENGTH 20
int WordClass::continueParse(wchar_t *buffer,__int64 begincp,__int64 bufferLen,vector<wchar_t *> &multiWords)
{ LFS
  wchar_t temp[MAX_MULTI_WORD_LENGTH+2],*b=buffer+begincp;
  temp[0]=0;
	// eliminate multiple spaces (possibly a word split by a newline)
  bool wasSpace=false,isSpace=false,endAtPeriod=false;
	int I=1;
  for (; I<MAX_MULTI_WORD_LENGTH && begincp+I<bufferLen; b++)
  {
    if ((isSpace=(iswspace(*b)!=0)) && wasSpace) continue;
		// don't permit a period after more than one letter (will result in missed matches ex. 'can't.')
		// but permit abbreviations like 'Ph.D.' - permit two characters IF they are directly followed by a period, a letter and then a period.
		if (iswalpha(temp[I-1]) && iswalpha(*b) && !(begincp+I+3<bufferLen && b[1]==L'.' && iswalpha(b[2]) && b[3]==L'.')) endAtPeriod=true; 
		if (*b==L'.' && endAtPeriod)
			break;
    temp[I++]=(wasSpace=isSpace) ? L' ' : towlower(*b);
  }
	temp[I]=0;
	vector<wchar_t *>::iterator str=lower_bound(multiWords.begin( ), multiWords.end( ), (wchar_t *)temp , loosesort );
	int slen=-1,maxslen=-1;
	while (str!=multiWords.end() && !wcsncmp(*str,(const wchar_t *)temp+1,slen=wcslen(*str)))
	{
		if (!iswalnum(temp[slen+1]))
			maxslen=max(maxslen,slen);
		str++;
	}
  return maxslen;
}

// 7-8-90 OR 7/8/90
// 7-90 OR 7/90
// deleted 7-90 on 4/7/2006 on account of ambiguity
int WordClass::processDate(wstring &sWord,wchar_t *buffer,__int64 &cp,__int64 &bufferScanLocation)
{ LFS
  __int64 tempcp=cp+1;
	bool twoDigitYear=false;
  if (!iswdigit(buffer[tempcp++])) return -1; // 8 in 7-8-90
  if ((twoDigitYear=iswdigit(buffer[tempcp]))!=0) tempcp++; // 1 in 7-11-90
  if (buffer[tempcp]!='/' && !isDash(buffer[tempcp]))
  {
		int month=_wtoi(sWord.c_str());
		int year=_wtoi(buffer+cp+1);
    if (!iswalnum(buffer[tempcp]) && month>=1 && month<=12 && year>=1 && year<=100 && twoDigitYear) // 10-1590 may not be a date
    {
      sWord.append((const wchar_t *)(buffer+cp),(int)(tempcp-cp));
      bufferScanLocation=cp=tempcp;
      return 0;
    }
    return -1;
  }
  tempcp++;
  if (!iswdigit(buffer[tempcp])) return -1; // 9 in 7-11-90
  tempcp++;
  if (iswdigit(buffer[tempcp])) tempcp++; // 0 in 7-11-90
  if (iswdigit(buffer[tempcp])) tempcp++; // 8 in 7-11-1985
  if (iswdigit(buffer[tempcp])) tempcp++; // 5 in 7-11-1985
  sWord.append((const wchar_t *)(buffer+cp),(int)(tempcp-cp));
  bufferScanLocation=cp=tempcp;
  return 0;
}

// also include   '90s or '91 '60s mid-1989, mid-60s, mid-1860s, mid-90s, over-40s, under-14s, early 1960s, top?
// 7-8-90 OR 7/8/90
// 7-90 OR 7/90
// deleted 7-90 on 4/7/2006 on account of ambiguity
int WordClass::processDate(wstring sWord,short &year,char &month,char &dayOfMonth)
{ LFS
	// mid-1989
	if (sWord.find(L"mid")!=wstring::npos || sWord.find(L"over")!=wstring::npos || sWord.find(L"under")!=wstring::npos || sWord.find(L"early")!=wstring::npos)
	{
		int wi=sWord.find(L'-');
		if (wi!=wstring::npos)
			year=_wtoi(sWord.c_str()+wi+1);
		return 0;
	}
	// '90s or '91
	if (sWord[0]=='\'')
	{
		year=_wtoi(sWord.c_str()+1);
		return 0;
	}
	// 60s
	if (sWord[sWord.length()-1]=='s')
	{
		year=_wtoi(sWord.c_str());
		return 0;
	}
	// 7-8-90 or 7/8/90
	int intMonth=-1,intDayOfMonth=-1,intYear=-1,numArgs=-1;
	if ((numArgs=swscanf(sWord.c_str(),L"%d-%d-%d",&intMonth,&intDayOfMonth,&intYear))==3 || (numArgs=swscanf(sWord.c_str(),L"%d/%d/%d",&intMonth,&intDayOfMonth,&intYear))==3 ||
	    (numArgs=swscanf(sWord.c_str(),L"%d-%d",&intMonth,&intYear))==2 || (numArgs=swscanf(sWord.c_str(),L"%d/%d",&intMonth,&intYear))==2)
	{
		month=intMonth;
		if (numArgs==3)
			dayOfMonth=intDayOfMonth;
		year=intYear;
		return 0;
	}
	return -1;
}

// 3:45
int WordClass::processTime(wstring &sWord, wchar_t *buffer, __int64 &cp, __int64 &bufferScanLocation)
{
	LFS
	int hour = _wtoi(sWord.c_str());
	if (hour>24 || hour<0) return -1;
	__int64 tempcp = cp + 1;
	if (!iswdigit(buffer[tempcp])) return -1; // 1 in 7:15
	if (!iswdigit(buffer[tempcp + 1])) return -1; // 5 in 7:15
	if (iswdigit(buffer[tempcp + 2])) return -1;
	int minute = (buffer[tempcp] - L'0') * 10 + (buffer[tempcp + 1] - L'0');
	if (minute>60) return -1;
	tempcp += 2;
	sWord.append((const wchar_t *)(buffer + cp), (int)(tempcp - cp));
	bufferScanLocation = cp = tempcp;
	return 0;
}

// reference: http://tools.ietf.org/html/rfc3986#appendix-B
// = "//" [ userinfo "@" ] host [ ":" port ] *( "/" segment )
// / "/"[segment-nz *("/" segment)]
// / segment-nz *("/" segment)
// / 0<pchar>

//    userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
//         unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
//         pct-encoded   = "%" HEXDIG HEXDIG
//         sub-delims = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
//    host = IPv4address / reg-name // don't worry about IPv6 for now
//        IPv4address   = dec-octet "." dec-octet "." dec-octet "." dec-octet
//            dec-octet=0-255
//        reg-name      = *( unreserved / pct-encoded / sub-delims )
//    port = *DIGIT
//    segment       = *pchar
//        pchar = unreserved / pct-encoded / sub-delims / ":" / "@"
//    segment-nz = 1*pchar
//    gen-delims = ":" / "/" / "?" / "#" / "[" / "]" / "@"
// query=*( pchar / "/" / "?" )
// fragment= *( pchar / "/" / "?" )

// example: http://krugman.blogs.nytimes.com/2009/07/30/the-lessons-of-1979-82/
// example: http://www.krugmanonline.com/about.php
// { "ftp"|"http"|"https" }://
// may end in a /  
// cannot end in a period
// ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?
//  12            3  4          5       6  7        8 9
// scheme ":" hier-part [ "?" query ] [ "#" fragment ]
// ** does not handle web addresses split by whitespace
int WordClass::processWebAddress(wstring &sWord,wchar_t *buffer, __int64 &cp, __int64 bufferLen)
{
	LFS
		// non-space characters
		_int64 localLimit = min(cp + 1024, bufferLen);
	__int64 I = cp; 
	for (; buffer[I] && !iswspace(buffer[I]) && I< localLimit; I++);
	if (I>1 && buffer[I-1] == L',')
		I--;
	if (I > 1)
	{
		wchar_t ch = buffer[I - 1];
		if (ch == L'.' || ch == L'?' || ch == L'!' || ch == L')' || ch == L':')
			I--;
	}
	localLimit = min(I + 10, localLimit);
	wchar_t savech = buffer[localLimit];
	buffer[localLimit] = 0;
	wchar_t *wh = wcsstr(buffer + cp, L"http://");
	buffer[localLimit] = savech;
	// if http in string, must be at start or beyond the end of the current 'word'
	if ((wh == NULL || wh == (buffer + cp) || wh > (buffer+I)) && 
		buffer[cp] != '\'' && buffer[cp] != '(' &&  // cannot start with quote or parens
		I > cp && I > 4 && // sanity
		(!wcsncmp(buffer + I - 4, L".com", 4) || // ending with common endings
		!wcsncmp(buffer + I - 4, L".net", 4) ||
		!wcsncmp(buffer + I - 4, L".org", 4) ||
		!wcsncmp(buffer + I - 4, L".edu", 4) ||
		!wcsncmp(buffer + I - 4, L".htm", 4) ||
		!wcsncmp(buffer + I - 5, L".html", 5)))
	{
		for (; cp < I; cp++)
			sWord += buffer[cp];
		//lplog(LOG_QCHECK, L"WEBADDRESS(2) %s", sWord.c_str());
		return 0;
	}
	__int64  tempcp = cp;
	bool angleBracket = false;
	if (angleBracket = buffer[tempcp] == L'<')
		tempcp++;
	//<a href = "http://www.encyclopedia.com/doc/1G2-2586700295.html" title = "Facts 
	//	and information about Spain">Spain</a>
	// <a title=""What's up with the rag head?": When I starred in a Facebook ad"
	//    href = "http://www.salon.com/2014/12/10/whats_up_with_the_rag_head_when_i_starred_in_a_facebook_ad/"
	//    class = "plain gaTrackLinkEvent"
	//    data-ga-track-json = '["morerelatedstories", "click", ""What's up withthe rag head ? ": When I starred in a Facebook ad"]' > 
	//    <img
	//      class = "lazyLoad" alt = ""What's up with the rag head?": When I starred in a Facebook ad"
	//      data-source-standard = "http://media.salon.com/2014/12/vish_singh-150x150.jpg" />
	if (tempcp>0 && !wcsncmp(buffer + tempcp - 1, L"<a", wcslen(L"<a")))
	{
		wchar_t *ic = wcsstr(buffer + tempcp, L"</a>");
		if (ic && ic - (buffer + tempcp) < 120)
			tempcp =(ic-buffer)+4;
		else
		{
			ic = wcschr(buffer + tempcp, L'>');
			if (ic && ic - (buffer + tempcp) < 120)
				tempcp = (ic - buffer) + 1;
			else
				return -1;
		}
	}
	else
	{
		if (!wcsncmp(buffer + tempcp, L"https", wcslen(L"https")))
			tempcp += 5;
		else if (!wcsncmp(buffer + tempcp, L"http", wcslen(L"http")))
			tempcp += 4;
		else if (!wcsncmp(buffer + tempcp, L"ftp", wcslen(L"ftp")))
			tempcp += 3;
		if (tempcp <= cp + 1 || buffer[tempcp] != L':' || buffer[tempcp + 1] != '/')
			return -1;
		wchar_t *allowedCharacters = L"-._~%!$&'()*+,;=:/?";
		while (buffer[tempcp] && (iswalnum(buffer[tempcp]) || wcschr(allowedCharacters, buffer[tempcp])))
			tempcp++;
		if (angleBracket && buffer[tempcp] == L'>')
			tempcp++;
		// not part of URL - most likely part of sentence
		else
		{
			if (buffer[tempcp - 1] == L',' || buffer[tempcp - 1] == L';')
				tempcp--;
			if (tempcp > 1 && (buffer[tempcp - 1] == L'.' ||
				buffer[tempcp - 1] == L'?' ||
				buffer[tempcp - 1] == L'!' ||
				buffer[tempcp - 1] == L'\'' ||
				buffer[tempcp - 1] == L')'))
				tempcp--;
		}
	}
	for (; cp < tempcp; cp++)
		sWord += buffer[cp];
	//lplog(LOG_QCHECK, L"WEBADDRESS %s", sWord.c_str());
	return 0;
}

int WordClass::processTime(wstring sWord, char &hour, char &minute)
{ LFS
	hour=_wtoi(sWord.c_str());
	minute=-1;
	int p=1;
	if (sWord[0] && sWord[p]!=':') p++;
	if (sWord[p]!=':') return -1;
	p++;
	minute=_wtoi(sWord.c_str()+p);
	return (hour>0 && hour<24 && minute>=0 && minute<=59) ? 0 : -1;
}

int WordClass::processFootnote(wchar_t *buffer,__int64 bufferLen,__int64 &cp)
{ LFS
  while ((!wcsncmp(buffer+cp,L"[Footnote ",10) || buffer[cp]==L'^') && cp<bufferLen-10)
  {
    if (buffer[cp]=='^')
    {
      // scan for Footnote Reference - for Decline and Fall of the Roman Empire
      __int64 savecp=cp+1;
      while ((iswdigit(buffer[savecp]) || buffer[savecp]=='*' || buffer[savecp]=='!') && savecp<bufferLen) savecp++;
      if (savecp==bufferLen)
        return PARSE_EOF;
      if (savecp>cp+1 && iswspace(buffer[savecp]))
        cp=savecp;
      else
        break;
    }
    else
    {
      // scan for Footnote - for Decline and Fall of the Roman Empire
      wchar_t *colon=wcschr(buffer+cp,L':');
      if (!colon) return 0;
      *colon=0;
      wchar_t *lb=colon,*ch;
      // [... ]
      // [... [..] ]
      // [... [..] [..] ]
      while (true)
      {
        ch=wcschr(lb+1,L']');
        lb=wcschr(lb+1,L'[');
        if (!lb || lb>ch) break;
        lb=ch+1;
      }
      if (!ch)
      {
        *colon=L':';
        break;
      }
      //lplog(L"Footnote %s encountered - length %d bytes.",buffer+cp+1,(ch-buffer)-cp);
      cp=ch-buffer+1;
    }
    while (cp<bufferLen && (iswspace(buffer[cp]) || buffer[cp]==L'_' || buffer[cp]==L'*'))
      cp++;
  }
  return 0;
}

// is this actually part of a word, and not the start of a quote?
bool WordClass::evaluateIncludedSingleQuote(wchar_t *buffer,__int64 cp,__int64 begincp)
{ LFS
  if (cp<=begincp) // quote is the first character
    return false;
  switch (towlower(buffer[cp+1]))
  {
  case L'd':if (!iswalpha(buffer[cp+2])) return false; // 'd would contraction
  case L'l':if (!iswalpha(buffer[cp+3]) && towlower(buffer[cp+2])==L'l') return false; // 'll[1,verb] shall contraction
  case L'm':if (!iswalpha(buffer[cp+2])) return false; // 'm am contraction
  case L'r':
    if (iswalpha(buffer[cp+3])) break;
    if (towlower(buffer[cp+2])==L'e') return false; // 're are contraction
    else if (towlower(buffer[cp+2])==L't') return false; // 'rt art contraction
    break;
  case L's':
    if (!iswalpha(buffer[cp+3]) && towlower(buffer[cp+2])==L't') return false; // 'st hast contraction
    else if (!iswalpha(buffer[cp+2])) return false; // he she it there here see **hsit below
    break;
  case L'v':if (!iswalpha(buffer[cp+3]) && towlower(buffer[cp+2])==L'e') return false; // 've have contraction
  }
  // don't
  if (towlower(buffer[cp-1])==L'n' && towlower(buffer[cp+1])==L't' && !iswalpha(buffer[cp+2]))
    return false;
  return iswalpha(buffer[cp+1])!=0; // is there a letter right after this quote?
}

bool WordClass::parseMetaCommands(int where,wchar_t *buffer, int &endSymbol, wstring &comment, sTrace &t)
{
	LFS
	if (buffer[0] != '!' && !iswalpha(buffer[0]))
		return false;
	wchar_t *nl = wcschr(buffer, '\r');
	if (nl == 0)
		return false;
	bool setValue = (buffer[0] != L'!');
	int offset = (setValue) ? 0 : 1;
	*nl = 0;
	lplog(LOG_INFO, L"%d:setting meta %s to %d", where,buffer+offset, setValue);
	*nl = '\r';
	if (!wcsncmp(buffer,L"DUMPLOCALOBJECTS",wcslen(L"DUMPLOCALOBJECTS")))
	{
		endSymbol=PARSE_DUMP_LOCAL_OBJECTS;
    return true;
	}
  else if (!wcsncmp(buffer,L"END",wcslen(L"END")))
	{
		endSymbol=PARSE_EOF;
    return true;
	}
  else if (!wcsnicmp(buffer+offset,L"printBeforeElimination",wcslen(L"printBeforeElimination")))
    t.printBeforeElimination=setValue;
	else if (!wcsnicmp(buffer+offset, L"traceSubjectVerbAgreement", wcslen(L"traceSubjectVerbAgreement")))
		t.traceSubjectVerbAgreement = setValue;
	else if (!wcsnicmp(buffer+offset, L"traceTestSubjectVerbAgreement", wcslen(L"traceTestSubjectVerbAgreement")))
		t.traceTestSubjectVerbAgreement = setValue;
	else if (!wcsnicmp(buffer+offset,L"traceEVALObjects",wcslen(L"traceEVALObjects")))
    t.traceEVALObjects= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceAnaphors",wcslen(L"traceAnaphors")))
    t.traceAnaphors= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceRelations",wcslen(L"traceRelations")))
    t.traceRelations= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceSpeakerResolution",wcslen(L"traceSpeakerResolution")))
    t.traceSpeakerResolution= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceObjectResolution",wcslen(L"traceObjectResolution")))
    t.traceObjectResolution= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceNameResolution",wcslen(L"traceNameResolution")))
    t.traceNameResolution= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceVerbObjects",wcslen(L"traceVerbObjects")))
    t.traceVerbObjects= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceDeterminer",wcslen(L"traceDeterminer")))
    t.traceDeterminer= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceBNCPreferences",wcslen(L"traceBNCPreferences")))
    t.traceBNCPreferences= setValue;
  else if (!wcsnicmp(buffer+offset,L"tracePatternElimination",wcslen(L"tracePatternElimination")))
    t.tracePatternElimination= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceSecondaryPEMACosting",wcslen(L"traceSecondaryPEMACosting")))
    t.traceSecondaryPEMACosting= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceMatchedSentences",wcslen(L"traceMatchedSentences")))
    t.traceMatchedSentences= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceUnmatchedSentences",wcslen(L"traceUnmatchedSentences")))
    t.traceUnmatchedSentences= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceIncludesPEMAIndex",wcslen(L"traceIncludesPEMAIndex")))
    t.traceIncludesPEMAIndex= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceTagSetCollection",wcslen(L"traceTagSetCollection")))
    t.traceTagSetCollection= setValue;
  else if (!wcsnicmp(buffer+offset,L"collectPerSentenceStats",wcslen(L"collectPerSentenceStats")))
    t.collectPerSentenceStats= setValue;
  else if (!wcsnicmp(buffer+offset,L"logCache=",wcslen(L"logCache=")))
    logCache=_wtoi(buffer+wcslen(L"logCache="));
  else if (!wcsnicmp(buffer+offset,L"traceRole",wcslen(L"traceRole")))
    t.traceRole= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceWikipedia",wcslen(L"traceWikipedia")))
    t.traceWikipedia= setValue;
  else if (!wcsnicmp(buffer+offset,L"traceWebSearch",wcslen(L"traceWebSearch")))
    t.traceWebSearch= setValue;
	else if (!wcsnicmp(buffer + offset, L"traceQCheck", wcslen(L"traceQCheck")))
		t.traceQCheck = setValue;
	else if (!wcsnicmp(buffer + offset, L"traceParseInfo", wcslen(L"traceParseInfo")))
		t.traceParseInfo = setValue;
	else if (!wcsnicmp(buffer,L"traceTransitoryQuestion",wcslen(L"traceTransitoryQuestion")))
	{
    t.traceCommonQuestion=false;
    t.traceMapQuestion=false;
    t.traceConstantQuestion=false;
    t.traceTransitoryQuestion=true;
		t.traceQuestionPatternMap=false;
	}
  else if (!wcsnicmp(buffer,L"traceConstantQuestion",wcslen(L"traceConstantQuestion")))
	{
    t.traceCommonQuestion=false;
    t.traceMapQuestion=false;
    t.traceConstantQuestion=true;
    t.traceTransitoryQuestion=false;
		t.traceQuestionPatternMap=false;
	}
  else if (!wcsnicmp(buffer,L"traceMapQuestion",wcslen(L"traceMapQuestion")))
	{
    t.traceCommonQuestion=false;
    t.traceMapQuestion=true;
    t.traceConstantQuestion=false;
    t.traceTransitoryQuestion=false;
		t.traceQuestionPatternMap=false;
	}
  else if (!wcsnicmp(buffer,L"traceCommonQuestion",wcslen(L"traceCommonQuestion")))
	{
    t.traceCommonQuestion=true;
    t.traceMapQuestion=false;
    t.traceConstantQuestion=false;
    t.traceTransitoryQuestion=false;
		t.traceQuestionPatternMap=false;
	}
	else if (!wcsnicmp(buffer, L"traceQuestionPatternMap", wcslen(L"traceQuestionPatternMap")))
	{
		t.traceCommonQuestion = false;
		t.traceMapQuestion = false;
		t.traceConstantQuestion = false;
		t.traceTransitoryQuestion = false;
		t.traceQuestionPatternMap = true;
	}
	else if (!wcsnicmp(buffer, L"P ", wcslen(L"P ")))
	{
		wchar_t *endline = wcschr(buffer + 2, '\r');
		if (endline)
		{
			*endline = 0;
			comment=buffer+2;
			*endline = '\r';
		}
	}
	return false;
}

int readDate(wchar_t *buffer, __int64 bufferLen, __int64 &bufferScanLocation, __int64 cp,wstring &sWord)
{
	if (cp + 5 >= bufferLen)
		return 0;
	bool isDate = false;
	// '90s or '91
	isDate = (buffer[cp] == L'\'' && iswdigit(buffer[cp + 1]) && iswdigit(buffer[cp + 2]) && ((buffer[cp + 3] == L's' && (iswspace(buffer[cp + 4]) || iswpunct(buffer[cp + 4]))) || iswspace(buffer[cp + 3]) || iswpunct(buffer[cp + 3])));
	if (isDate)
	{
		for (int c = 0; c < 3; c++, cp++)
			sWord += buffer[cp];
		if (buffer[cp] == L's') 
			sWord += buffer[cp++];
		bufferScanLocation = cp;
		return PARSE_DATE;
	}
	// mid-1989, mid-60s, mid-1860s, mid-90s, late-40s, early-10s
	vector <wstring> dateStart = { L"mid",L"over",L"early",L"late" };
	for (wstring d : dateStart)
		if (!wcsnicmp(buffer + cp, d.c_str(), d.length()) && WordClass::isDash(buffer[cp+d.length()]) && iswdigit(buffer[cp+d.length()+1]) && iswdigit(buffer[cp + d.length() + 2]))
		{
			__int64 ocp = cp;
			ocp += d.length() + 3;
			wchar_t *ch = buffer + cp;
			// mid-1989
			if (iswdigit(ch[0]) && iswdigit(ch[1]) && (iswspace(ch[2]) || iswpunct(ch[2])))
				ocp += 2;
			// mid-1860s
			else if (iswdigit(ch[0]) && ch[1] == L'0' && ch[2] == L's' && (iswspace(ch[3]) || iswpunct(ch[3])))
				ocp += 3;
			// mid-60s
			else if (ch[0] == L's' && (iswspace(ch[1]) || iswpunct(ch[1])))
				ocp++;
			else
				return 0;
			for (; cp < ocp; cp++)
				sWord += buffer[cp];
			bufferScanLocation = cp;
			lplog(LOG_ERROR, L"TEMP DATE %s", sWord.c_str());
			return PARSE_DATE;
		}
	return 0;
}

/*
0097 included because of misencoding
&#151; is wrong. When you use numeric character references, the number refers to the Unicode codepoint. 
For numbers below 256 that is the same as the codepoint in ISO-8859-1. 
In 8859-1, character 151 is amongst the “C1 control codes”, and not a dash or any other visible character.
The confusion arises because character 151 is a dash in Windows code page 1252 (Western European). 
Many people think cp1252 is the same thing as ISO-8859-1, but in reality it's not: the characters in the C1 range (128 to 159) are different.
The first application is reading your “ASCII” file* as ISO-8859-1, but actually it's probably cp1252 and you'll need a way to clue the app in about what encoding it has to expect.
*/
bool WordClass::isDash(wchar_t ch)
{
	// \u expects 4 hexadecimal digits
	// 0097 encodes to 
	wchar_t dashes[] = { L'-',L'˗',L'۔',L'‐',L'‑',L'‒',L'–',L'⁃',L'−',L'➖',L'Ⲻ',L'﹘',L'—',L'\u0097',L'─' };
	for (wchar_t d : dashes)
		if (ch == d)
			return true;
	return false;
}

bool WordClass::isSingleQuote(wchar_t ch)
{
	// L'\x16F51', L'\x16F52', multicharacter does not for now work with VC++?
	wchar_t singleQuotes[] = { L'\'',L'`',L'´', L'ʹ', L'ʻ', L'ʼ', L'ʽ', L'ʾ', L'ˈ', L'ˊ', L'ˋ', L'˴', L'ʹ', L'΄', L'՚', L'՝', L'י', L'׳', L'ߴ', L'ߵ', L'ᑊ', L'ᛌ', L'᾽', L'᾿', L'`', L'´', L'῾', L'‘', L'’', L'‛', L'′',L'‵', L'ꞌ',L'\xFF07', L'\xFF40' };
	for (wchar_t sq : singleQuotes)
		if (ch == sq)
			return true;
	return false;
}

// from http://unicode.org/cldr/utility/confusables.jsp?a=%22&r=None
// \x22 \xff02 \x3003 \x2ee \x5f2 \x2033 \x5f4 \x2036 \x2f6 \x2ba \x201c \x201d \x2dd \x201f
bool WordClass::isDoubleQuote(wchar_t ch)
{
	wchar_t doubleQuotes[] = { L'"', L'＂',L'〃',L'ˮ',L'ײ',L'″',L'״',L'‶',L'˶',L'ʺ',L'“',L'”',L'˝',L'‟',L'᳓'};
	for (wchar_t dq : doubleQuotes)
		if (ch == dq)
			return true;
	return false;
}

int WordClass::readWord(wchar_t *buffer,__int64 bufferLen,__int64 &bufferScanLocation,
                        wstring &sWord,wstring &comment,int &nounOwner,bool scanForSection,bool webScrapeParse,sTrace &t, MYSQL *mysql,int sourceId)
{ LFS
  __int64 cp=bufferScanLocation;
  nounOwner=0;
  sWord.clear();
  int numlines=0;
  // ignore _ and *
  while (cp<bufferLen && (iswspace(buffer[cp]) || buffer[cp]=='_' || (buffer[cp]=='*' && !webScrapeParse)) && numlines<=1)
  {
    if (buffer[cp]==13) numlines++;
    cp++;
  }
	// ditch all lines that start with * or o - these are typically not part of the main text and cause lots of wasted processing
	// these are generated from the html through the screen scraping process
	if (webScrapeParse && (cp-bufferScanLocation)>1 && 
		  ((buffer[cp]=='*' && buffer[cp+1]==' ') || (buffer[cp]=='o' && buffer[cp+1]==' ')))
	{
		while (cp<bufferLen && buffer[cp]!=13)
			cp++;
		if (cp+1<bufferLen && buffer[cp]==13)
			cp++;
		bufferScanLocation=cp;
    return PARSE_END_PARAGRAPH;
	}
	// Pattern building parse
	if (buffer[cp]==L'{' && cp+3<bufferLen && iswupper(buffer[cp+1]) && iswalpha(buffer[cp+1]) && (buffer[cp+2]==L'=' || buffer[cp+2]==L'}'))
	{
		for (cp+=1; cp<bufferLen && buffer[cp]!=L'}'; cp++)
			sWord+=buffer[cp];
    bufferScanLocation=cp+1;
		return PARSE_PATTERN;
	}
  // NewsBank
  if (!wcsncmp(buffer+cp,L"<br>",4))
  {
    bufferScanLocation=cp+4;
    return PARSE_END_SECTION;
  }
  // NewsBank
  if (!wcsncmp(buffer+cp,L"~|~",3))
  {
    bufferScanLocation=cp+3;
    return PARSE_END_BOOK;
  }
	// web address processing
	if (processWebAddress(sWord,buffer, cp,bufferLen) == 0)
	{
		bufferScanLocation = cp;
		return PARSE_WEB_ADDRESS;
	}
	// remove italic html
	if (!wcsncmp(buffer + cp, L"<i>", 3))
	{
		bufferScanLocation = cp + 3;
		return readWord(buffer, bufferLen, bufferScanLocation, sWord, comment, nounOwner, scanForSection, webScrapeParse, t,mysql,sourceId);
	}
	if (!wcsncmp(buffer + cp, L"</i>", 4))
	{
		bufferScanLocation = cp + 4;
		return readWord(buffer, bufferLen, bufferScanLocation, sWord, comment, nounOwner, scanForSection, webScrapeParse, t,mysql,sourceId);
	}
	// NewsBank XML parsing
  if (buffer[cp]=='&')
  {
    if (!wcsncmp(buffer+cp,L"&lt;",4))
    {
      bufferScanLocation=cp+=4;
      sWord='<';
      return 0;
    }
    if (!wcsncmp(buffer+cp,L"&gt;",4))
    {
      bufferScanLocation=cp+=4;
      sWord='>';
      return 0;
    }
    if (!wcsncmp(buffer+cp,L"&amp;",5))
    {
      bufferScanLocation=cp+=5;
      sWord='&';
      return 0;
    }
  }
  if (numlines>1)
  {
    sWord.clear();
    bufferScanLocation=cp;
    return PARSE_END_PARAGRAPH;
  }
  // comments begin with ~~~, meta commands with ~~
  if (buffer[cp]=='~' && buffer[cp+1]=='~')
  {
		int endSymbol=0;
    parseMetaCommands((int)cp,buffer+cp+2,endSymbol,comment,t);
    while (cp<bufferLen && buffer[cp]!=13)
      cp++;
    if (cp<bufferLen) cp++;
    if (buffer[cp]==10) cp++;
    bufferScanLocation=cp;
		if (endSymbol) 
			return endSymbol;
		else
			return readWord(buffer,bufferLen,bufferScanLocation,sWord,comment,nounOwner,scanForSection,webScrapeParse,t,mysql,sourceId);
  }
  if (cp>=bufferLen)
    return PARSE_EOF;
  if (processFootnote(buffer,bufferLen,cp)<0) return PARSE_EOF;
	if (readDate(buffer, bufferLen, bufferScanLocation, cp,sWord))
		return PARSE_DATE;
	// any character that should be its own word
  if (iswpunct(buffer[cp]) ||   (!iswprint(buffer[cp]) && iswspace(buffer[cp+1])) ||
    isSingleQuote(buffer[cp]) || // slightly different quotes
    isDoubleQuote(buffer[cp]) || 
		isDash(buffer[cp]) || buffer[cp]==L'…' || buffer[cp]==L'│')
  {
    // contraction processing
    // <option>n't not contraction handled below as well to check for n
    if (isSingleQuote(buffer[cp]) && !iswspace(buffer[cp-1]))
    {
      switch (towlower(buffer[cp+1]))
      {
      case 'd':if (!iswalpha(buffer[cp+2])) { sWord=L"wouldhad"; cp+=2; } break; // 'd would contraction
      case 'l':if (!iswalpha(buffer[cp+3]) && towlower(buffer[cp+2])=='l') { sWord=L"shall"; cp+=3; } break; // 'll[1,verb] shall contraction
      case 'm':if (!iswalpha(buffer[cp+2])) { sWord=L"am"; cp+=2; } break; // 'm am contraction
      case 'r':
        if (iswalpha(buffer[cp+3])) break;
        if (towlower(buffer[cp+2])=='e') { sWord=L"are"; cp+=3; } // 're are contraction
        else if (towlower(buffer[cp+2])=='t') { sWord=L"art"; cp+=3; } // 'rt art contraction
        break;
      case 's':
        if (!iswalpha(buffer[cp+3]) && towlower(buffer[cp+2])=='t') { sWord=L"hast"; cp+=3; } // 'st hast contraction
        else if (!iswalpha(buffer[cp+2])) { sWord=L"ishas"; cp+=2; } // he she it there here see **hsit below
        break;
      case 'v':if (!iswalpha(buffer[cp+3]) && towlower(buffer[cp+2])=='e') { sWord=L"have"; cp+=3; } break; // 've have contraction
      }
    }
    if (sWord.length())
    {
      bufferScanLocation=cp;
      return 0;
    }
    // --- and ... processing
    if (!wcsncmp(buffer+cp,L"--",wcslen(L"--"))) sWord=L"--";
    else if (!wcsncmp(buffer+cp,L"...",wcslen(L"..."))) sWord=L"...";
    else sWord=buffer[cp];
    cp+=sWord.length();
    bufferScanLocation=cp;
    if (sWord==L"." || sWord==L"?" || sWord==L"!" || sWord==L"..." || sWord==L"…")
      return PARSE_END_SENTENCE;
		int extend;
		if (sWord.length() == 1 && isSingleQuote(sWord[0]) && (extend = continueParse(buffer, cp-1, bufferLen, quotedWords)) > 1)
		{
			for (int sqi = 0; sqi < extend-1; sqi++)
				sWord += buffer[cp++];
			bufferScanLocation = cp;
		}
		return 0;
  }
  else
  {
    __int64 begincp=cp;
    int numChars,extend=continueParse(buffer,begincp,bufferLen,multiElementWords);
    bool wasSpace=false,isSpace=false;
    while (true)
    {
      numChars=extend;
      for (wchar_t *b=buffer+cp; cp<bufferLen; cp++,b++)
        if ((!iswspace(*b) && !iswpunct(*b) && (iswprint(*b) || !iswspace(b[1])) &&
          !(isSingleQuote(*b) || isDoubleQuote(*b) || *b==L'…' || *b==L'_' || *b == L'*')
          ) || (isDash(*b) && iswalpha(b[1])) || // accept al-Jazeera as one word, but not a double dash or anything else
          cp-begincp<numChars)
        {
          if ((isSpace=(iswspace(*b)!=0)) && wasSpace) continue;
          if (wasSpace=isSpace)
            sWord+=L' ';
          else
            sWord+=*b;
        }
        else
          break;
      if (numChars>=0 && cp-begincp==numChars) break;
      if (isSingleQuote(buffer[cp]) && evaluateIncludedSingleQuote(buffer,cp,begincp))
      {
        extend=(int)(cp-begincp+1);
        continue;
      }
      if (isSingleQuote(buffer[cp]) && (extend=continueParse(buffer,begincp,bufferLen,quotedWords))>numChars)
        continue;
      else if (buffer[cp]=='.' && (extend=continueParse(buffer,begincp,bufferLen,periodWords))>numChars) continue;
      //else if (buffer[cp]=='-' && (extend=continueParse(buffer,begincp,dashWords))>numChars) continue;
      break;
    }
    // dangling - at the end of a line
    if (isDash(buffer[cp]) && buffer[cp+1]==13 && buffer[cp+2]==10 && fullQuery(mysql,sWord,sourceId)==end())
    {
      cp+=3;
      while (cp<bufferLen && iswspace(buffer[cp])) cp++;
      if (cp==bufferLen)
        return PARSE_EOF;
      for (; cp<bufferLen; cp++)
        if (!iswspace(buffer[cp]) && !iswpunct(buffer[cp]))
          sWord+=buffer[cp];
        else
          break;
    }
    // ownership (plural) too ambiguous on this level to figure out whether it is ownership or the end of a single quoted string
    if (isSingleQuote(buffer[cp]) && !iswalpha(buffer[cp+1]) && towlower(buffer[cp-1])=='s')
    {
      nounOwner=1;
      //  cp++;
    }
    // ownership (single)
    if (isSingleQuote(buffer[cp]) && bufferLen>cp+1 && towlower(buffer[cp+1])==L's' && (bufferLen<=cp+2 || !iswalpha(buffer[cp+2])) &&
      wcsicmp(sWord.c_str(),L"he") && wcsicmp(sWord.c_str(),L"she") && wcsicmp(sWord.c_str(),L"it") &&
      wcsicmp(sWord.c_str(),L"there") && wcsicmp(sWord.c_str(),L"here")) // **hsit
    {
      nounOwner=2;
      cp+=2;
    }
    // -n't not contraction - after processing done by next section
    if (isSingleQuote(buffer[cp]) && bufferLen>cp+1 && towlower(buffer[cp+1])==L't' && !wcsicmp(sWord.c_str(),L"n"))
    {
      sWord=L"not";
      cp+=2;
    }
    // -n't not contraction - cut off n, prepare for processing by previous section
    else if (sWord.length()>1 && numChars==-1 && isSingleQuote(buffer[cp]) && bufferLen>cp+1 && cp>0 && towlower(buffer[cp+1])==L't' && towlower(buffer[cp-1])==L'n')
    {
      sWord.erase(sWord.length()-1,1); // cut off n
      //sWord[sWord.length()-1]=0;
      cp--;
    }
    // dates, times, phone numbers (U.S. short form), money, 0th 1st 2nd 3rd 4th 5th 6th 7th 8th 9th processing
		// 2"	char *
		bool isAsciiMoney=false,isUnicodeMoney=false;
    if ((sWord[0]>0 && iswdigit(sWord[0])) || 
		    ((isAsciiMoney=(sWord[0]==L'$')) || (isUnicodeMoney=((sWord[0])==L'Â' && (sWord[1])==L'£'))))
    {
      int I=0;
			if (isAsciiMoney) I++;
			if (isUnicodeMoney) I+=2;
      while (iswdigit(sWord[I])) I++;
      if (!sWord[I] && I==3 && isDash(buffer[cp]) && iswdigit(buffer[cp+1]))
      {
        int J=0;
        while (iswdigit(buffer[J+cp+1]) && J+cp+1<bufferLen) J++;
        if (J==4)
        {
          for (J=0; J<5; J++) sWord+=buffer[cp++];
          bufferScanLocation=cp;
          return PARSE_TELEPHONE_NUMBER;
        }
      }
      // date processing
      if (!sWord[I] && (buffer[cp]==L'/' || isDash(buffer[cp])) && processDate(sWord,buffer,cp,bufferScanLocation)==0)
        return PARSE_DATE;
			// time processing
			if (!sWord[I] && (buffer[cp] == L':' || buffer[cp] == L'.') && processTime(sWord, buffer, cp, bufferScanLocation) == 0)
				return PARSE_TIME;
			if (buffer[cp] == L'.' && iswdigit(buffer[cp + 1]))
      {
        sWord+=buffer[cp++];
        while (cp<bufferLen && iswdigit((wchar_t)buffer[cp]))
          sWord+=buffer[cp++];
        bufferScanLocation=cp;
				return (isAsciiMoney || isUnicodeMoney) ? PARSE_MONEY_NUM : PARSE_NUM;
      }
      if (!sWord[I] && !nounOwner) // not 70's
      {
        bufferScanLocation=cp;
				return (isAsciiMoney || isUnicodeMoney) ? PARSE_MONEY_NUM : PARSE_NUM;
      }
      // include 1950s or it was in the low 50s today. The 70's
      if (nounOwner || (sWord[I]==L's' && !sWord[I+1]))
      {
        bufferScanLocation=cp;
        return PARSE_PLURAL_NUM;
      }
      wchar_t *numEnd[]={L"0th",L"1st",L"2nd",L"3rd",L"3d",L"4th",L"5th",L"6th",L"7th",L"8th",L"9th",
        L"1stly",L"2ndly",L"3rdly",L"4thly",L"5thly",L"6thly",L"7thly",L"8thly",L"9thly",NULL};
      int num;
      for (num=0; numEnd[num] && wcsicmp(sWord.c_str()+I-1,numEnd[num]); num++);
      if (!numEnd[num])
      {
        cp-=sWord.length()-I;
        sWord.erase(I,sWord.length()-I);
        bufferScanLocation=cp;
				return (isAsciiMoney || isUnicodeMoney) ? PARSE_MONEY_NUM : PARSE_NUM;
      }
      bufferScanLocation=cp;
      return (sWord[sWord.length()-1]=='y') ? PARSE_ADVERB_NUM : PARSE_ORD_NUM;
    }
    // handle numbers incorrectly appended to words 'He was in the category 18to25'
    else if (iswdigit(sWord[sWord.length()-1]))
    {
      int I=sWord.length()-1;
      while (iswdigit(sWord[I])) I--;
      I++;
      wstring tmp=sWord.substr(0,I);
      // if it is a word we recognize, uncouple the word from the number.
      if (fullQuery(mysql, tmp, sourceId)!=WMM.end())
      {
        cp-=sWord.length()-I;
        sWord=tmp;
      }
    }
		bufferScanLocation = cp;
  }
  if (sWord[0]==L'\'' || sWord[0]==L'’' || sWord[0]==L'‘')
    lplog(LOG_FATAL_ERROR,L"Illegal character");
  return 0;
}

WordClass::~WordClass()
{ LFS
  WMM.clear();
  for (unsigned int I=0; I<Forms.size(); I++)
    delete Forms[I];
  vector<wchar_t *>::iterator sw,swend;
  for (sw=multiElementWords.begin(),swend=multiElementWords.end(); sw!=swend; sw++) free((void *)(*sw));
  for (sw=quotedWords.begin(),swend=quotedWords.end(); sw!=swend; sw++) free((void *)(*sw));
  for (sw=periodWords.begin(),swend=periodWords.end(); sw!=swend; sw++) free((void *)(*sw));
  //for (sw=dashWords.begin(),swend=dashWords.end(); sw!=swend; sw++) free((void *)(*sw));
  tfree(tFI::allocated*sizeof(*tFI::formsArray),tFI::formsArray);
  tFI::allocated=0;
  tFI::fACount=0;
}

bool WordClass::findWordInDB(MYSQL *mysql, wstring &sWord, int &wordId, tIWMM &iWord)
{
	if (mysql == NULL)
		return false;
	if (!myquery(mysql, L"LOCK TABLES words w READ,wordforms wf READ")) return true;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	// don't drop order by formId!  this is necessary to ensure unknown form (0) is first, which is used in isUnknown processing!
	if (wordId == -1)
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select w.id,wf.formId,wf.count,w.inflectionFlags,w.flags,w.timeFlags,w.mainEntryWordId,w.derivationRules,w.sourceId from words w,wordForms wf where wf.wordId=w.id and word=\"%s\" order by wf.formId", sWord.c_str());
	else
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select w.word,wf.formId,wf.count,w.inflectionFlags,w.flags,w.timeFlags,w.mainEntryWordId,w.derivationRules,w.sourceId from words w,wordForms wf where wf.wordId=%d and w.id=%d order by wf.formId", wordId, wordId);
	MYSQL_RES *result = NULL;
	if (!myquery(mysql, qt, result) || mysql_num_rows(result) == 0)
	{
		if (result != NULL)
			mysql_free_result(result);
		myquery(mysql, L"UNLOCK TABLES");
		return false;
	}
	MYSQL_ROW sqlrow;
	unsigned int *wordForms = (unsigned int *)tmalloc(mysql_num_rows(result) * sizeof(int) * 2);
	int iInflectionFlags = 0, iFlags = 0, iTimeFlags = 0, iMainEntryWordId = 0, iDerivationRules = 0, iSourceId = 0, count = 0;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		if (count == 0)
		{
			if (wordId == -1)
				wordId = atoi(sqlrow[0]);
			else
				mTW(sqlrow[0], sWord);
			iInflectionFlags = atoi(sqlrow[3]);
			iFlags = atoi(sqlrow[4]);
			iTimeFlags = atoi(sqlrow[5]);
			iMainEntryWordId = atoi(sqlrow[6]);
			iDerivationRules = atoi(sqlrow[7]);
			iSourceId = atoi(sqlrow[8]);
		}
		wordForms[count++] = atoi(sqlrow[1]) - 1;
		wordForms[count++] = atoi(sqlrow[2]);
	}
	mysql_free_result(result);
	if (!myquery(mysql, L"UNLOCK TABLES"))
		return false;
	//tFI::tFI(unsigned int *forms, unsigned int iCount, int iInflectionFlags, int iFlags, int iTimeFlags, int mainEntryWordId, int iDerivationRules, int iSourceId, int formNum, wstring &word)
	int selfFormNum = FormsClass::findForm(sWord);
	if (Words.query(sWord) != Words.end())
	{
		lplog(LOG_FATAL_ERROR, L"word %s double insertion attempted", sWord.c_str());
	}
	iWord = Words.WMM.insert(WordClass::tWFIMap(sWord, tFI(wordForms, count / 2, iInflectionFlags, iFlags, iTimeFlags, iMainEntryWordId, iDerivationRules, iSourceId, selfFormNum, sWord))).first;
	Words.mapWordIdToWordStructure(wordId, iWord);
	if (Words.wordStructureGivenWordIdExists(iMainEntryWordId) == wNULL)
	{
		wstring sMainEntryWord;
		findWordInDB(mysql, sMainEntryWord, iMainEntryWordId, iWord->second.mainEntry);
	}
	else
		iWord->second.mainEntry = Words.wordStructureGivenWordIdExists(iMainEntryWordId);
	tfree(count * sizeof(int), wordForms);
	return iWord != Words.end();
}

tIWMM WordClass::fullQuery(MYSQL *mysql, wstring word, int sourceId)
{
	tIWMM iWord = Words.end();
	int wordId = -1;
	if ((iWord = Words.query(word)) == Words.end() && !findWordInDB(mysql, word, wordId, iWord))
		getForms(mysql,iWord, word, sourceId,false);
	return iWord;
}

