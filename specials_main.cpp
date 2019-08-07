#include <windows.h>
#define _WINSOCKAPI_ /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "source.h"
#include "time.h"
#include <fcntl.h>
#include "bncc.h"
#include "mysql.h"
#include <direct.h>
#include <sys/stat.h>
#include "ontology.h"
#include <crtdbg.h>
	extern "C" {
#include <yajl_tree.h>
	}
#include "getMusicBrainz.h"
#include "profile.h"
#include "mysqldb.h"
#include "mysqld_error.h"
#include "internet.h"
#include<jni.h>
#include "hmm.h"


// needed for _STLP_DEBUG - these must be set to a legal, unreachable yet never changing value
unordered_map <wstring,tFI> static_wordMap;
tIWMM wNULL=static_wordMap.end();
map<tIWMM, tFI::cRMap::tRelation, Source::wordMapCompare> static_tIcMap;
tFI::cRMap::tIcRMap tNULL=(tFI::cRMap::tIcRMap)static_tIcMap.begin();
vector <cLocalFocus> static_cLocalFocus;
vector <cLocalFocus>::iterator cNULL=static_cLocalFocus.begin();
vector <Source::cSpeakerGroup> static_cSpeakerGroup;
vector <Source::cSpeakerGroup>::iterator sgNULL = (vector <Source::cSpeakerGroup>::iterator)static_cSpeakerGroup.begin();
vector <WordMatch> static_wm;
vector <WordMatch>::iterator wmNULL=static_wm.begin();
set<int> static_setInt;
set<int>::iterator sNULL= static_setInt.begin();
SRWLOCK rdfTypeMapSRWLock, mySQLTotalTimeSRWLock, totalInternetTimeWaitBandwidthControlSRWLock, mySQLQueryBufferSRWLock, orderedHyperNymsMapSRWLock;

// profiling
__int64 cProfile::cb;
__int64 cProfile::accumulatedOverheadTime=0;
unordered_map <string ,__int64 > cProfile::counterMap;
unordered_map <string ,int > cProfile::counterNumMap;
unordered_map <string,cProfile::CP> cProfile::timeMapTotal;
__int64 cProfile::totalCount=0;
bool cProfile::lockInitialized=false;
string cProfile::functionPath;
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::timeSetCompare> cProfile::timeSort; // sort map by time taken by function
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::memorySetCompare> cProfile::memorySort; // sort map by memory allocated by function
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::countSetCompare> cProfile::countSort; // sort map by number of times function is called
__int64 cProfile::mySQLTotalTime=0;
struct _RTL_SRWLOCK cProfile::networkTimeSRWLock;
int cProfile::totalInternetTimeWaitBandwidthControl;
__int64 cProfile::accumulationNetworkProfileTimer;
__int64 cProfile::accumulateOnlyNetTimer;
__int64 cProfile::lastNetworkTimePrinted;
__int64 cProfile::accumulateNetworkTimeCount;
int cProfile::lastNetClock;
unordered_map < wstring, __int64 > cProfile::netAndSleepTimes,cProfile::onlyNetTimes,cProfile::numTimesPerURL;
int websterQueriedToday = 0;

bool exitNow = false, exitEventually = false;
int overallTime;
int initializeCounter(void);
void freeCounter(void);
bool TSROverride = false, flipTOROverride = false, flipTNROverride = false, flipTMSOverride = false, flipTUMSOverride = false;

void no_memory () {
	lplog(LOG_FATAL_ERROR,L"Out of memory (new/STL allocation).");
	exit (1);
}

int createLPProcess(int numProcess, HANDLE &processId, wchar_t *commandPath, wchar_t *processParameters)
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.wShowWindow = true;
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE | STARTF_USECOUNTCHARS | STARTF_USESHOWWINDOW;
	si.dwX = 60;
	si.dwY = 180 * numProcess;
	si.dwXSize = 300;
	si.dwYSize = 500;
	si.dwXCountChars = 180;
	si.dwYCountChars = 3000;
	si.wShowWindow = SW_SHOWNOACTIVATE; // don't continuously hijack focus
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcess(commandPath,
		processParameters, // Command line
		NULL, // Process handle not inheritable
		NULL, // Thread handle not inheritable
		FALSE, // Set handle inheritance to FALSE
		CREATE_NEW_CONSOLE,
		NULL, // Use parent's environment block
		NULL, // Use parent's starting directory 
		&si, // Pointer to STARTUPINFO structure
		&pi) // Pointer to PROCESS_INFORMATION structure
		)
	{
		wchar_t cwd[1024];
		printf("CreateProcess of %S failed (%d) %s in %S.\n", processParameters, (int)GetLastError(), LastErrorStr(), _wgetcwd(cwd, 1024));
		return -1;
	}
	processId = pi.hProcess;
	return 0;
}

int startProcesses(Source &source, int processKind, int step, int beginSource, int endSource, Source::sourceTypeEnum st, int maxProcesses, int numSourcesPerProcess, bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite,bool parseOnly)
{
	LFS
	chdir("source");
	//MYSQL_RES *result = NULL;
	HANDLE *handles = (HANDLE *)calloc(maxProcesses, sizeof(HANDLE));
	int numProcesses = 0, errorCode = 0;
	wstring tmpstr;
	while (!exitNow && !exitEventually && !errorCode)
	{
		unsigned int nextProcessIndex = numProcesses;
		if (numProcesses == maxProcesses)
		{
			nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, false, 1000 * 60 * 60, false);
			if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
				continue;
			if (nextProcessIndex == WAIT_FAILED)
				lplog(LOG_FATAL_ERROR, L"WaitForMultipleObjectsEx failed with error %s", getLastErrorMessage(tmpstr));
			if (nextProcessIndex < WAIT_OBJECT_0 + numProcesses) // nextProcessIndex >= WAIT_OBJECT_0 && 
			{
				nextProcessIndex -= WAIT_OBJECT_0;
				CloseHandle(handles[nextProcessIndex]);
				printf("\nClosing process %d:%p", nextProcessIndex, handles[nextProcessIndex]);
			}
			if (nextProcessIndex >= WAIT_ABANDONED_0 && nextProcessIndex < WAIT_ABANDONED_0 + numProcesses)
			{
				nextProcessIndex -= WAIT_ABANDONED_0;
				printf("\nClosing process %d:%p", nextProcessIndex, handles[nextProcessIndex]);
				CloseHandle(handles[nextProcessIndex]);
			}
		}
		wstring parseRequestPath;
		int id, repeatStart, prId;
		wstring start, path, encoding, etext, author, title, pathInCache;
		bool result;
		switch (processKind)
		{
		case 0:result = source.getNextUnprocessedParseRequest(prId, pathInCache); break;
		case 1:result = source.getNextUnprocessedSource(beginSource, endSource, st, false, id, path, encoding, start, repeatStart, etext, author, title); break;
		case 2:result = source.anymoreUnprocessedForUnknown(st, step); break;
		default:result = 0; break;
		}
		if (!result)
		{
			if (numProcesses == maxProcesses)
			{
				memmove(handles + nextProcessIndex, handles + nextProcessIndex + 1, (maxProcesses - nextProcessIndex - 1) * sizeof(handles[0]));
				numProcesses--;
			}
			if (numProcesses)
			{
				printf("\nNo more processes to be createDd. %d processes left to wait for.", numProcesses);
				while (true)
				{
					nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, true, 1000 * 60 * 60, false);
					if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
						continue;
					if (nextProcessIndex == WAIT_FAILED)
						lplog(LOG_FATAL_ERROR, L"\nWaitForMultipleObjectsEx failed with error %s", getLastErrorMessage(tmpstr));
					for (int I = 0; I < numProcesses; I++)
						CloseHandle(handles[I]);
					break;
				}
			}
			break;
		}
		HANDLE processId = 0;
		wchar_t processParameters[1024];
		switch (processKind)
		{
		case 0:
			wsprintf(processParameters, L"releasex64\\lp.exe -ParseRequest \"%s\" -cacheDir %s %s%s%s%s%s-log %d", pathInCache.c_str(), CACHEDIR,
				(forceSourceReread) ? L"forceSourceReread " : L"",
				(sourceWrite) ? L"-SW " : L"",
				(sourceWordNetRead) ? L"-SWNR " : L"",
				(sourceWordNetWrite) ? L"-SWNW " : L"",
				(parseOnly) ? L"-parseOnly " : L"",
				numProcesses);
			if (errorCode = createLPProcess(nextProcessIndex, processId, L"releasex64\\lp.exe", processParameters) < 0)
				break;
			break;
		case 1:
			wsprintf(processParameters, L"releasex64\\lp.exe -book 0 + -BC 0 -cacheDir %s %s%s%s%s%s-numSourceLimit %d -log %d", CACHEDIR,
				(forceSourceReread) ? L"forceSourceReread " : L"",
				(sourceWrite) ? L"-SW " : L"",
				(sourceWordNetRead) ? L"-SWNR " : L"",
				(sourceWordNetWrite) ? L"-SWNW " : L"",
				(parseOnly) ? L"-parseOnly " : L"",
				numSourcesPerProcess,
				numProcesses);
			if (errorCode = createLPProcess(nextProcessIndex, processId, L"releasex64\\lp.exe", processParameters) < 0)
				break;
			break;
		case 2:
			wsprintf(processParameters, L"releasex64\\CorpusAnalysis.exe -step %d -numSourceLimit %d -log %d", CACHEDIR, step, numSourcesPerProcess, numProcesses);
			if (errorCode = createLPProcess(nextProcessIndex, processId, L"releasex64\\CorpusAnalysis.exe", processParameters) < 0)
				break;
			break;
		default: break;
		}
		handles[nextProcessIndex] = processId;
		if (numProcesses < maxProcesses)
			numProcesses++;
		printf("\nCreated process %d:%p", nextProcessIndex, processId);
	}
	if (st != Source::REQUEST_TYPE)
	{
		freeCounter();
		_exit(0); // fast exit
	}
	free(handles);
	chdir("..");
	return 0;
}

void setConsoleWindowSize(int width,int height)
{
	HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);      // Get Handle 
	_COORD coord;
	coord.X = max(150, width);
	coord.Y = max(2000, height);
	if (!SetConsoleScreenBufferSize(Handle, coord))            // Set Buffer Size 
		printf("Cannot set console buffer info to (%d,%d) (%d) %s\n", coord.X, coord.Y, (int)GetLastError(), LastErrorStr());

	_SMALL_RECT Rect;
	Rect.Top = 0;
	Rect.Left = 0;
	Rect.Bottom = height - 1;
	Rect.Right = width - 1;

	if (!SetConsoleWindowInfo(Handle, TRUE, &Rect))            // Set Window Size 	SMALL_RECT srctWindow;
		printf("Cannot set console window info to (top=%d,left=%d,bottom=%d,right=%d) (%d) %s\n", 
			Rect.Top, Rect.Left, Rect.Bottom, Rect.Right,
			(int)GetLastError(), LastErrorStr());
}

bool MWRequestAllowed()
{
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	int day = timeinfo->tm_year * 365 + timeinfo->tm_yday;
	int numRequests = 0, lastDay, lastNumRequests;
	FILE *MWRequestToday = _wfopen(L"MWCheck", L"r");
	if (MWRequestToday)
	{
		fscanf(MWRequestToday, "%d %d", &lastDay, &lastNumRequests);
		fclose(MWRequestToday);
		if (lastDay == day)
			numRequests = lastNumRequests;
	}
	numRequests++;
	MWRequestToday = _wfopen(L"MWCheck", L"w");
	fwprintf(MWRequestToday, L"%d %d", day, numRequests);
	fclose(MWRequestToday);
	websterQueriedToday= numRequests;
	return numRequests < 2000;
}


bool getMerriamWebsterDictionaryAPIForms(wstring sWord, set <int> &posSet, bool &plural, bool &networkAccessed, bool logEverything);
bool existsInDictionaryDotCom(MYSQL *mysql,wstring word, bool &networkAccessed);
bool detectNonEuropeanWord(wstring word);
int cacheWebPath(wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, bool forceWebReread, bool &networkAccessed);
string lookForPOS(string originalWord, yajl_val node, bool logEverything,int &inflection, string &referWord);
int discoverInflections(set <int> posSet, bool plural, wstring word);

int getWordPOS(MYSQL *mysql,wstring word, set <int> &posSet, int &inflections, bool print, bool &isNonEuropean, int &dictionaryComQueried, int &dictionaryComCacheQueried, bool &websterAPIRequestsExhausted,bool logEverything)
{
	LFS
	if (isNonEuropean = detectNonEuropeanWord(word))
		return 0;
	bool networkAccessed, existsDM = existsInDictionaryDotCom(mysql, word, networkAccessed);
	if (networkAccessed)
		dictionaryComQueried++;
	else
		dictionaryComCacheQueried++;
	if (!existsDM || websterAPIRequestsExhausted)
		return 0;
	networkAccessed = false;
	bool plural;
	getMerriamWebsterDictionaryAPIForms(word, posSet, plural, networkAccessed,logEverything);
	inflections = discoverInflections(posSet, plural, word);
	if (networkAccessed && !MWRequestAllowed())
	{
		websterAPIRequestsExhausted = true;
	}
	return 0;
}

int testDisInclineAndSplit(MYSQL *mysql, Source &source, int sourceId, WordMatch &word, bool capitalized, int totalFrequency, int capitalizedFrequency, int allCapsFrequency, set <int> &posSet,
	int &inflections, bool &isNonEuropean, bool &queryOnLowerCase, int &dictionaryComQueried, int &dictionaryComCacheQueried, bool &websterAPIRequestsExhausted)
{
	if (queryOnLowerCase = (totalFrequency > 5 && ((capitalizedFrequency + allCapsFrequency)*100.0 / totalFrequency) > 95.0))
		posSet.insert(FormsClass::gFindForm(L"noun"));
	else
		getWordPOS(mysql,word.word->first, posSet, inflections, false, isNonEuropean, dictionaryComQueried, dictionaryComCacheQueried, websterAPIRequestsExhausted,false);
	bool caps = word.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (word.flags&WordMatch::flagFirstLetterCapitalized) || (word.flags&WordMatch::flagAllCaps);
	if (posSet.empty())
	{
		tIWMM iWord = Words.end();
		int ret;
		if (capitalized || (ret = Words.attemptDisInclination(mysql, iWord, word.word->first, sourceId)))
		{
			if ((ret = Words.splitWord(mysql, iWord, word.word->first, sourceId)) && word.word->first.find(L'-')!=wstring::npos)
			{
				wstring sWord= word.word->first;
				sWord.erase(std::remove(sWord.begin(), sWord.end(), L'-') , sWord.end());
				if ((iWord=Words.query(sWord))==Words.end())
					ret = Words.attemptDisInclination(mysql, iWord, sWord, sourceId);
			}
		}
		if (iWord!=Words.end())
		{
			for (unsigned int f=0; f<iWord->second.formsSize(); f++)
			{
				posSet.insert(iWord->second.forms()[f]);
			}
		}
	}
	return posSet.size();
}

int createWordInDBIfNecessary(MYSQL mysql, int sourceId, int &wordId, wstring word, bool actuallyExecuteAgainstDB, bool logEverything)
{
	LFS
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numWordsInserted = 0;
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id from words where word=\"%s\"", word.c_str());
	if (myquery(&mysql, qt, result)) // if result is null, also returns false
	{
		if (sqlrow = mysql_fetch_row(result))
		{
			wordId = atoi(sqlrow[0]);
			mysql_free_result(result);
			return 0; // did not create word
		}
	}
	wcscpy(qt, L"INSERT IGNORE INTO words (word,inflectionFlags,flags,timeFlags,mainEntryWordId,derivationRules,sourceId) VALUES "); // IGNORE is necessary because C++ treats unicode strings as different, but MySQL treats them as the same
	int len = wcslen(qt) , inflectionFlags = 0, flags = 0, timeFlags = 0, derivationRules = 0;
	_snwprintf(qt + len, QUERY_BUFFER_LEN - len, L"(\"%s\",%d,%d,%d,%d,%d,%d)",
		word.c_str(), inflectionFlags, flags, timeFlags, -1, derivationRules, sourceId);
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&mysql, qt))
			return -1;
		if (!myquery(&mysql, L"SELECT LAST_INSERT_ID()", result))
			return -1;
		if (sqlrow = mysql_fetch_row(result))
			wordId = atoi(sqlrow[0]);
		mysql_free_result(result);
	}
	else if (logEverything)
		lplog(LOG_INFO, L"DB statement [%s create word]: %s", word.c_str(), qt);
	return 1; // created word
}

int	analyzeFormsUsageVSNewForms(MYSQL mysql, int wordId, wstring word, bool properNoun, bool existingWord, set <int> posSetDB, set <int> &remove, set<int> &add, set<int> &keep, int &maxcount,bool logEverything)
{
	bool properNounFormFound = false;
	maxcount = 127; // maximum
	if (existingWord)
	{
		wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select formId,count from wordforms where wordId=%d and (formId<=%d or formId=%d)", wordId, tFI::patternFormNumOffset,
			tFI::PROPER_NOUN_USAGE_PATTERN - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset); // formId in (1,101,102,103,104)
		MYSQL_RES * result;
		MYSQL_ROW sqlrow = NULL;
		if (!myquery(&mysql, qt, result))
			return -1;
		while (sqlrow = mysql_fetch_row(result))
		{
			int formId = atoi(sqlrow[0]);
			int count = atoi(sqlrow[1]);
			// record maximum usage count
			if (formId == tFI::patternFormNumOffset)
			{
				maxcount = count;
				continue;
			}
			// keep proper noun usage count if set as proper noun
			if (formId == tFI::PROPER_NOUN_USAGE_PATTERN - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset)
			{
				properNounFormFound = true;
				if (properNoun)
					keep.insert(formId);
				else
					remove.insert(formId);
				continue;
			}
			bool formUnknownCombinationOrOpen = (formId == (UNDEFINED_FORM_NUM + 1) || formId == (nounForm + 1) || formId == (adjectiveForm + 1) || formId == (verbForm+1) || formId == (adverbForm+1) || formId == (COMBINATION_FORM_NUM + 1));
			// forms to be set (in posSetDB) are assumed to be only open classes (noun,verb,adjective,adverb) or classes returned from the webster API
			// form to keep in DB - that is not unknown, and not combination and not open, and needs to be set
			bool formToKeepInDB = posSetDB.find(formId) != posSetDB.end() || !formUnknownCombinationOrOpen;
			// don't print unknown, combination, open forms or abbreviations or proper nouns, if this current word has been determined to be a proper noun
			if (!properNoun || !(formUnknownCombinationOrOpen || formId==(abbreviationForm+1) || formId==(PROPER_NOUN_FORM_NUM+1)) || logEverything)
				lplog(LOG_INFO, L"%s:original form: %s [%d] %s", word.c_str(), Forms[formId - 1]->name.c_str(), count, (formToKeepInDB) ? L"WILL BE KEPT" : L"WILL BE REMOVED");
			// noun=101, adjective=102,verb=103,adverb=104
			if (formToKeepInDB)
			{
				posSetDB.erase(formId);
				keep.insert(formId);
				if (formId == nounForm+1)
				{
					keep.insert(tFI::SINGULAR_NOUN_HAS_DETERMINER - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
					keep.insert(tFI::SINGULAR_NOUN_HAS_NO_DETERMINER - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
				}
				if (formId == verbForm+1)
				{
					keep.insert(tFI::VERB_HAS_0_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
					keep.insert(tFI::VERB_HAS_1_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
					keep.insert(tFI::VERB_HAS_2_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
				}
			}
			else
			{
				remove.insert(formId);
				if (formId == nounForm + 1)
				{
					remove.insert(tFI::SINGULAR_NOUN_HAS_DETERMINER - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
					remove.insert(tFI::SINGULAR_NOUN_HAS_NO_DETERMINER - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
				}
				if (formId == verbForm + 1)
				{
					remove.insert(tFI::VERB_HAS_0_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
					remove.insert(tFI::VERB_HAS_1_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
					remove.insert(tFI::VERB_HAS_2_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
				}
			}
		}
	}
	else
		add.insert(tFI::patternFormNumOffset);
	if (properNoun && !properNounFormFound)
	{
		add.insert(tFI::PROPER_NOUN_USAGE_PATTERN - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
	}
	for (int formId : posSetDB)
	{
		if (!properNoun || logEverything || formId!=nounForm+1)
			lplog(LOG_INFO, L"%s:new form: %s WILL BE ADDED", word.c_str(), Forms[formId-1]->name.c_str());
		add.insert(formId);
		if (formId == nounForm + 1)
		{
			add.insert(tFI::SINGULAR_NOUN_HAS_DETERMINER - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
			add.insert(tFI::SINGULAR_NOUN_HAS_NO_DETERMINER - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
		}
		if (formId == verbForm + 1)
		{
			add.insert(tFI::VERB_HAS_0_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
			add.insert(tFI::VERB_HAS_1_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
			add.insert(tFI::VERB_HAS_2_OBJECTS - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
		}
	}
	//LOWER_CASE_USAGE_PATTERN = 32756
	return 0;
}

int overwriteWordFormsInDB(MYSQL mysql, int wordId, wstring word, set <int> &posSetDB, __int64 &formsDeleted,bool properNoun,bool existingWord, bool actuallyExecuteAgainstDB,bool logEverything)
{
	LFS
	set <int> remove, add, keep;
	int maxcount;
	analyzeFormsUsageVSNewForms(mysql, wordId, word, properNoun, existingWord, posSetDB, remove, add, keep,maxcount, logEverything);
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (existingWord)
	{
		// erase all open wordforms and unknown form associated with wordId in wordforms
		// keep all others - this will allow forms like demonym to be kept.
		remove.insert(1); // unknown
		wstring removeFormQueryString,removeFormString;
		for (int r : remove)
		{
			removeFormQueryString += std::to_wstring(r) + L",";
			if (r < 32750 && (logEverything || !properNoun || (r != (UNDEFINED_FORM_NUM+1) && r != (adjectiveForm+1) && r != (verbForm + 1) && r != (adverbForm + 1) && r !=(COMBINATION_FORM_NUM+1))))
				removeFormString += Forms[r - 1]->name + L" ";
		}
		_snwprintf(qt, QUERY_BUFFER_LEN, L"delete from wordforms where formId in (%s) and wordId=%d", removeFormQueryString.substr(0, removeFormQueryString.length() - 1).c_str(),wordId);
		if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
			return -1;
		else if (removeFormString.length()>0)
			lplog(LOG_INFO, L"DB statement [%s delete]: %s", word.c_str(), removeFormString.c_str());
		formsDeleted = mysql_affected_rows(&mysql);
		if (keep.find(tFI::PROPER_NOUN_USAGE_PATTERN - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset) != keep.end())
		{
			_snwprintf(qt, QUERY_BUFFER_LEN, L"update wordforms set count=%d where wordId=%d and formId=%d", maxcount,wordId, tFI::PROPER_NOUN_USAGE_PATTERN - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset);
			if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
				return -1;
			else if (logEverything || !maxcount)
				lplog(LOG_INFO, L"DB statement [%s update counts]: %s", word.c_str(), qt);
		}
	}
	if (add.size() > 0)
	{
		wcscpy(qt, L"insert wordForms(wordId,formId,count) VALUES ");
		int len = wcslen(qt);
		// insert wordForms(wordId, formId, count) VALUES(2, 33, 4), (5, 6, 7)
		for (int form : add)
		{
			// if transfer count (tFI::patternFormNumOffset) or tFI::PROPER_NOUN_USAGE_PATTERN, set this to maxcount
			// else set to 0.
			if (form == tFI::patternFormNumOffset || form == tFI::PROPER_NOUN_USAGE_PATTERN - tFI::MAX_FORM_USAGE_PATTERNS + tFI::patternFormNumOffset)
				len += wsprintf(qt + len, L"(%d,%d,%d),", wordId, form, maxcount);
			else
				len += wsprintf(qt + len, L"(%d,%d,%d),", wordId, form, 0);
		}
		qt[len - 1] = 0;
		wcscat(qt, L" ON DUPLICATE KEY UPDATE count=VALUES(count)");
		if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
			return -1;
		else
		{
			wstring forms;
			for (int form : add)
				if (form < tFI::patternFormNumOffset && (!properNoun || form!=(nounForm+1)))
					forms += Forms[form-1]->name + L" ";
			if (forms.length()>0)
				lplog(LOG_INFO, L"DB statement [%s forms insert]: %s (%s)", word.c_str(), forms.c_str(), qt);
		}
	}
	return 0;
}

// queryOnLowerCase will query for word forms  the next time the word is encountered in all lower case.
// queryOnLowerCase = 4
int overwriteWordFlagsInDB(MYSQL mysql, wstring word, bool actuallyExecuteAgainstDB,bool logEverything)
{
	LFS
	// erase all wordforms associated with wordId in wordforms
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"update words set flags=flags|%d where word=\"%s\"", tFI::queryOnLowerCase, word.c_str());
	if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
		return -1;
	else if (logEverything)
		lplog(LOG_INFO, L"DB statement [%s add queryOnLowerCase flag]: %s", word.c_str(), qt);
	return 0;
}

// PLURAL refers to noun plural form.
int overwriteWordInflectionFlagsInDB(MYSQL mysql, wstring word, int inflections, bool actuallyExecuteAgainstDB)
{
	LFS
	// erase all wordforms associated with wordId in wordforms
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"update words set inflectionFlags=inflectionFlags+%d where word=\"%s\"", inflections,word.c_str());
	if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
		return -1;
	else
	{
		wstring sFlags;
		lplog(LOG_INFO, L"DB statement [%s add inflection flag]:%s", word.c_str(), allFlags(inflections, sFlags));
	}
	return 0;
}

/*
(Stem) word topsails: Added Inflection PLURAL (Form noun, definitionEntry topsail)
(Original) word inappreciable: Added Inflection ADJECTIVE_NORMATIVE (Form adjective, definitionEntry appreciable)
(SW) word calash: Added Inflection VERB_PRESENT_FIRST_SINGULAR (Form verb, definitionEntry lash)
ERROR:Word baiocco was not found
ERROR:Suffix rule #369 with word présence (root prés) has no suffix form.
modified stemmer code and rules using 
*/
void testDisinclination()
{

}

// this is to accumulate how webster describes word forms in its "fl" field, to properly map to lp forms.
	/* test webster
	unordered_set<wstring> pos;
	int numFilesProcessed = 0;
	scanAllWebsterEntries(L"J:\\caches\\webster", pos, numFilesProcessed);
	for (wstring psi : pos)
	{
		bool plural = false;
		set <int> posSet;
		wstring forms;
		identifyFormClass(posSet, psi, plural);
		for (int form : posSet)
			forms += Forms[form]->name + L" ";
		printf("%S:%S %S\n", psi.c_str(), forms.c_str(), (plural) ? L"PLURAL" : L"");
	}
	 end test webster
	*/
void scanAllWebsterEntries(wchar_t *basepath, unordered_set<wstring> &pos, int &numFilesProcessed)
{
	WIN32_FIND_DATA FindFileData;
	wchar_t path[1024];
	wsprintf(path, L"%s\\*.*", basepath);
	HANDLE hFind = FindFirstFile(path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		wprintf(L"FindFirstFile failed on directory %s (%d)\r", path, (int)GetLastError());
		return;
	}
	do
	{
		if (FindFileData.cFileName[0] == '.') continue;
		wchar_t completePath[1024];
		wsprintf(completePath, L"%s\\%s", basepath, FindFileData.cFileName);
		if ((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			scanAllWebsterEntries(completePath, pos, numFilesProcessed);
		else
		{
			printf("%d:%100S\r", numFilesProcessed, completePath);
			int fd;
			if ((fd = _wopen(completePath, O_RDWR | O_BINARY)) < 0)
				printf("cacheWebPath:Cannot read path %S - %s.\n", completePath, sys_errlist[errno]);
			else
			{
				numFilesProcessed++;
				int bufferlen = filelength(fd);
				void *tbuffer = (void *)tcalloc(bufferlen + 10, 1);
				_read(fd, tbuffer, bufferlen);
				_close(fd);
				wstring buffer = (wchar_t *)tbuffer;
				tfree(bufferlen + 10, tbuffer);
				char errbuf[1024];
				errbuf[0] = 0;
				string jsonBuffer;
				wTM(buffer, jsonBuffer);
				yajl_val node = yajl_tree_parse((const char *)jsonBuffer.c_str(), errbuf, sizeof(errbuf));
				/* parse error handling */
				if (node == NULL) {
					lplog(LOG_ERROR, L"Parse error:%s\n %S\n", jsonBuffer.c_str(), errbuf);
					return;
				}
				if (node->type == yajl_t_array)
					for (unsigned int docNum = 0; docNum < node->u.array.len; docNum++)
					{
						yajl_val doc = node->u.array.values[docNum];
						wstring temppos;
						int inflection;
						string referWord;
						mTW(lookForPOS(NULL, doc, true,inflection, referWord), temppos);
						if (temppos.length() > 0)
						{
							pos.insert(temppos);
						}
					}

			}
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);
	FindClose(hFind);
	return;
}

void eraseOldRDFTypeFiles(wstring completePath, int &removeErrors)
{
}

void scanAllRDFTypes(MYSQL mysql, wchar_t *startPath, bool &startHit, wchar_t *basepath, int &numFilesProcessed, int &numNotOpenable, int &numNewestVersion, int &numOldVersion, int &removeErrors, int &populatedRDFs,int &numERDFRemoved,
	unordered_map<wstring,int> &extensions, unordered_map<wstring, __int64> &extensionSpace)
{
	WIN32_FIND_DATA FindFileData;
	wchar_t path[1024];
	wsprintf(path, L"%s\\*.*", basepath);
	HANDLE hFind = FindFirstFile(path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		wprintf(L"\nscanAllRDFTypes:FindFirstFile failed on directory %s (%d)\n", path, (int)GetLastError());
		return;
	}
	do
	{
		ULARGE_INTEGER ul;
		ul.LowPart = FindFileData.nFileSizeLow;
		ul.HighPart = FindFileData.nFileSizeHigh;
		wchar_t *ext = wcsrchr(FindFileData.cFileName, L'.');
		if (ext && ext[1]!=0)
		{

			extensions[ext]++;
			extensionSpace[ext] += ((ULONGLONG)ul.QuadPart);
		}
		bool isRDFType = false, isERDFType = false, isJSON=false ;
		// must be an rdfTypes extension
		if (FindFileData.cFileName[0] == '.' || (wcslen(FindFileData.cFileName) > 10 && 
			(!(isRDFType= wcsicmp(FindFileData.cFileName + wcslen(FindFileData.cFileName) - 9, L".rdfTypes") == 0) && 
			 !(isJSON=    wcsicmp(FindFileData.cFileName + wcslen(FindFileData.cFileName) - 5, L".json") == 0) &&
			 !(isERDFType=wcsicmp(FindFileData.cFileName + wcslen(FindFileData.cFileName) - 10, L".erdfTypes") == 0))))
			continue;
		numFilesProcessed++;
		wchar_t completePath[1024];
		wsprintf(completePath, L"%s\\%s", basepath, FindFileData.cFileName);
		if (!startHit && wcscmp(startPath, completePath) == 0)
			startHit = true;
		if ((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		{
			if (startHit || wcsncmp(startPath, completePath,wcslen(completePath)) == 0)
				scanAllRDFTypes(mysql, startPath, startHit, completePath, numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs, numERDFRemoved,extensions,extensionSpace);
		}
		else
		{
			if ((numFilesProcessed & 63) == 0)
			{
				wstring extstr;
				for (auto const&[ext, num] : extensions)
				{
					if (num > 1)
					{
						wchar_t buf[1024];
						wsprintf(buf, L"%s:%d[%I64d] ", ext.c_str(), num, extensionSpace[ext]);
						extstr += buf;
					}
				}
				printf("%08d:unopenable=%02d newest=%08d old=%08d cannot remove=%08d populated=%07d numERDFRemoved=%07d [%s][%S]\r", numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs, numERDFRemoved, (startHit) ? "HIT" : "NOT HIT",extstr.c_str());
			}

			if (isERDFType && (((ULONGLONG)ul.QuadPart) == 10))
			{
				wchar_t qt[2048];
				FindFileData.cFileName[wcslen(FindFileData.cFileName) - 10] = 0;
				wsprintf(qt, L"INSERT INTO noERDFTypes VALUES ('%s')", FindFileData.cFileName);
				if (wcslen(FindFileData.cFileName) > 127 || myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY)
				{
					if (_wremove(completePath))
					{
						wprintf(L"\nremove failed on path %s (%d)\n", completePath, (int)GetLastError());
						removeErrors++;
					}
					else
						numERDFRemoved++;
					continue;
				}
			}
			if (isRDFType && ((ULONGLONG)ul.QuadPart) == 2)
			{
				wchar_t qt[2048];
				FindFileData.cFileName[wcslen(FindFileData.cFileName) - 9] = 0;
				wsprintf(qt, L"INSERT INTO noRDFTypes VALUES ('%s')", FindFileData.cFileName);
				if (wcslen(FindFileData.cFileName) > 127 || myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY)
				{
					if (_wremove(completePath))
					{
						wprintf(L"\nremove failed on path %s (%d)\n", completePath, (int)GetLastError());
						removeErrors++;
					}
					continue;
				}
			}
			int fd;
			if ((fd = _wopen(completePath, O_RDWR | O_BINARY)) < 0)
			{
				numNotOpenable++;
				printf("\nscanAllRDFTypes:Cannot read path %S - %s.\n", completePath, sys_errlist[errno]);
				continue;
			}
			wchar_t version;
			::read(fd, &version, sizeof(version));
			_close(fd);
			if ((isRDFType && version != RDFLIBRARYTYPE_VERSION) || (isERDFType && version != EXTENDED_RDFTYPE_VERSION))
			{
				numOldVersion++;
				if (_wremove(completePath))
				{
					wprintf(L"\nremove failed on path %s (%d)\n", completePath, (int)GetLastError());
					removeErrors++;
				}
				else if (isERDFType)
					numERDFRemoved++;
				continue;
			}
			populatedRDFs++;
			if (startHit && (populatedRDFs & 63) == 0)
			{
				FILE *progressFile = _wfopen(L"RDFTypesScanProgress.txt", L"w");
				if (progressFile)
				{
					fputws(completePath, progressFile);
					fclose(progressFile);
				}
			}
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);
	FindClose(hFind);
	return;
}

void removeIllegalNames(wchar_t *basepath)
{
	WIN32_FIND_DATA FindFileData;
	wchar_t path[1024];
	wsprintf(path, L"%s\\*.*", basepath);
	HANDLE hFind = FindFirstFile(path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		wprintf(L"\removeIllegalNames:FindFirstFile failed on directory %s (%d)\n", path, (int)GetLastError());
		return;
	}
	do
	{
		// must be an rdfTypes extension
		if (FindFileData.cFileName[0] == '.') // || (wcslen(FindFileData.cFileName) > 10 && wcscmp(FindFileData.cFileName + wcslen(FindFileData.cFileName) - 10, L".erdfTypes") != 0))
			continue;
		wchar_t completePath[1024];
		wsprintf(completePath, L"%s\\%s", basepath, FindFileData.cFileName);
		if ((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			removeIllegalNames(completePath);
		bool isIllegal = false;
		for (int I = 0; FindFileData.cFileName[I] && !isIllegal; I++)
			if (!iswascii(FindFileData.cFileName[I]) && !iswdigit(FindFileData.cFileName[I]) && FindFileData.cFileName[I] != L'-' && FindFileData.cFileName[I] != L'_')
				isIllegal = true;
		if (isIllegal)
		{
			_wremove(completePath);
			wprintf(L"removed %-200.200s\r", completePath);
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);
	FindClose(hFind);
}

/*
void scanAllERDFTypes(MYSQL mysql, wchar_t *basepath, int &numFilesProcessed, int &numNotOpenable, int &numNewestVersion, int &numOldVersion, int &removeErrors, int &populatedRDFs)
		wchar_t qt[2048];
		if ((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			scanAllERDFTypes(mysql, completePath, numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs);
		else
		{
			if ((numFilesProcessed & 31) == 0)
				printf("%08d:unopenable=%02d newest=%08d old=%08d cannot remove=%08d populated=%07d [%s]\r", numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs, (startHit) ? "HIT" : "NOT HIT");
			ULARGE_INTEGER ul;
			ul.LowPart = FindFileData.nFileSizeLow;
			ul.HighPart = FindFileData.nFileSizeHigh;
			if (((ULONGLONG)ul.QuadPart) != 10)
			{
				populatedRDFs++;
				continue;
			}
			int fd;
			if ((fd = _wopen(completePath, O_RDWR | O_BINARY)) < 0)
			{
				numNotOpenable++;
				printf("\nscanAllERDFTypes:Cannot read path %S - %s.\n", completePath, sys_errlist[errno]);
			}
			else
			{
				char buffer[12];
				int bufferlen = filelength(fd);
				::read(fd, buffer, bufferlen);
				_close(fd);
				if (*((wchar_t *)buffer) != EXTENDED_RDFTYPE_VERSION) // version
				{
					numOldVersion++;
					if (_wremove(completePath))
					{
						wprintf(L"\nremove failed on path %s (%d)\n", completePath, (int)GetLastError());
						removeErrors++;
					}
				}
				int rdfTypeCount=*((int *)(buffer+sizeof(wchar_t)));
				int topHierarchyClassIndexesCount= *((int *)(buffer + sizeof(wchar_t)+sizeof(rdfTypeCount)));
				if (!rdfTypeCount || !topHierarchyClassIndexesCount)
				{
					numNewestVersion++;
					FindFileData.cFileName[wcslen(FindFileData.cFileName) - 9] = 0;
					wsprintf(qt, L"INSERT INTO noERDFTypes VALUES ('%s')", FindFileData.cFileName);
					if (wcslen(FindFileData.cFileName) > 127 || myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY)
					{
						if (_wremove(completePath))
						{
							wprintf(L"\nremove failed on path %s (%d)\n", completePath, (int)GetLastError());
							removeErrors++;
						}
					}
				}
			}
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);
	FindClose(hFind);
	return;
}
*/
void scanAllDictionaryDotCom(MYSQL mysql, wchar_t *basepath, int &numFilesProcessed, int &numNotOpenable, int &filesRemoved,int &removeErrors)
{
	WIN32_FIND_DATA FindFileData;
	wchar_t path[1024];
	wsprintf(path, L"%s\\*.*", basepath);
	HANDLE hFind = FindFirstFile(path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		wprintf(L"\nscanAllDictionaryDotCom:FindFirstFile failed on directory %s (%d)\n", path, (int)GetLastError());
		return;
	}
	do
	{
		// must be an rdfTypes extension
		if (FindFileData.cFileName[0] == '.')
			continue;
		numFilesProcessed++;
		wchar_t completePath[1024];
		wsprintf(completePath, L"%s\\%s", basepath, FindFileData.cFileName);
		if ((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			scanAllDictionaryDotCom(mysql, completePath, numFilesProcessed, numNotOpenable, filesRemoved, removeErrors);
		else
		{
			if ((numFilesProcessed & 31) == 0)
				printf("%08d:unopenable=%02d removed=%08d cannot remove=%08d\r", numFilesProcessed, numNotOpenable, filesRemoved, removeErrors);
			int fd;
			if ((fd = _wopen(completePath, O_RDWR | O_BINARY)) < 0)
			{
				numNotOpenable++;
				printf("\nscanAllDictionaryDotCom:Cannot read path %S - %s.\n", completePath, sys_errlist[errno]);
			}
			else
			{
				int bufferlen = filelength(fd);
				wchar_t *tbuffer = (wchar_t *)tcalloc(bufferlen + 10, 1);
				_read(fd, tbuffer, bufferlen);
				_close(fd);
				bool putInTable = !wcsstr(tbuffer, L"No results found") || !wcsstr(tbuffer, L"dcom-no-result");
				tfree(bufferlen + 10, tbuffer);
				if (putInTable)
				{
					if (wcslen(FindFileData.cFileName) > 31)
						continue;
					wchar_t qt[2048];
					wsprintf(qt, L"INSERT INTO notwords VALUES ('%s')", FindFileData.cFileName);
					if (!myquery(&mysql, qt, true) && mysql_errno(&mysql) != ER_DUP_ENTRY)
						removeErrors++;
					else
					{
						if (_wremove(completePath))
						{
							removeErrors++;
							wprintf(L"\nremove failed on path %s (%d)\n", completePath, (int)GetLastError());
						}
						else
							filesRemoved++;
					}
				}
			}
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);
	FindClose(hFind);
	return;
}

class wordInfo
{
public:
	int totalFrequency;
	int unknownFrequency;
	int unknownCapitalizedFrequency;
	int unknownAllCapsFrequency;
	bool nonEuropeanWord;
	bool number; // has NUMBER_FORM_NUM
	bool cardinal; // has cardinalOrdinalForm
	bool ordinal; // has numeralOrdinalForm
	bool roman; // has romanNumeralForm
	bool date; // has dateForm
	bool time; // has timeForm
	bool telephone; // has telephoneNumberForm
	bool money; // has moneyForm
	bool webaddress; // has webAddressForm
	wordInfo()
	{
		totalFrequency = 0;
		unknownFrequency = 0;
		unknownCapitalizedFrequency = 0;
		unknownAllCapsFrequency = 0;
		nonEuropeanWord = false;
		number = false; // has NUMBER_FORM_NUM
		cardinal = false; // has cardinalOrdinalForm
		ordinal = false; // has numeralOrdinalForm
		roman = false; // has romanNumeralForm
		date = false; // has dateForm
		time = false; // has timeForm
		telephone = false; // has telephoneNumberForm
		money = false; // has moneyForm
		webaddress = false; // has webAddressForm
	}
};



void writeSourceWordFrequency(MYSQL *mysql,unordered_map<wstring, wordInfo> wf, wstring etext)
{
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int currentEntry = 0, totalEntries = wf.size(), percent = 0,len=0;
	int lastSourceEtext = _wtoi(etext.c_str());
	for (auto const&[word, wi] : wf)
	{
		if (len == 0)
			len += _snwprintf(qt, QUERY_BUFFER_LEN, L"INSERT INTO wordfrequencymemory (word,totalFrequency,unknownFrequency,capitalizedFrequency,allCapsFrequency,lastSourceEtext,nonEuropeanFlag,numberFlag,cardinalFlag,ordinalFlag,romanFlag,dateFlag,timeFlag,telephoneFlag,moneyFlag,webaddressFlag) VALUES");
		len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L" (\"%s\",%d,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)", word.c_str(), wi.totalFrequency, wi.unknownFrequency, wi.unknownCapitalizedFrequency, wi.unknownAllCapsFrequency, lastSourceEtext,
			(wi.nonEuropeanWord) ? L"true" : L"false",
			(wi.number) ? L"true" : L"false",
			(wi.cardinal) ? L"true" : L"false",
			(wi.ordinal) ? L"true" : L"false",
			(wi.roman) ? L"true" : L"false",
			(wi.date) ? L"true" : L"false",
			(wi.time) ? L"true" : L"false",
			(wi.telephone) ? L"true" : L"false",
			(wi.money) ? L"true" : L"false",
			(wi.webaddress) ? L"true" : L"false");
		if (len > QUERY_BUFFER_LEN_UNDERFLOW)
		{
			len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L" ON DUPLICATE KEY UPDATE totalFrequency=totalFrequency+VALUES(totalFrequency),unknownFrequency=unknownFrequency+VALUES(unknownFrequency),capitalizedFrequency=capitalizedFrequency+VALUES(capitalizedFrequency),allCapsFrequency=allCapsFrequency+VALUES(allCapsFrequency),lastSourceEtext=VALUES(lastSourceEtext)");
			if (!myquery(mysql, qt)) return;
			len = 0;
		}
		else
			qt[len++] = L',';
	}
	if (len > 0)
	{
		qt[--len] = L' ';
		len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L" ON DUPLICATE KEY UPDATE totalFrequency=totalFrequency+VALUES(totalFrequency),unknownFrequency=unknownFrequency+VALUES(unknownFrequency),capitalizedFrequency=capitalizedFrequency+VALUES(capitalizedFrequency),allCapsFrequency=allCapsFrequency+VALUES(allCapsFrequency),lastSourceEtext=VALUES(lastSourceEtext)");
		if (!myquery(mysql, qt)) return;
	}
}

int analyzeEnd(Source source, int sourceId, wstring path, wstring etext, wstring title,bool &reprocess,bool &nosource)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) 
		return -1;
	if (Words.readWithLock(source.mysql, sourceId, path, false, false, false) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, true))
	{
		bool multipleEnds = false;
		for (unsigned int I = 0; I < source.sentenceStarts.size(); I++)
		{
			int s = source.sentenceStarts[I], sEnd = (I == source.sentenceStarts.count - 1) ? source.m.size() : source.sentenceStarts[I + 1];
			if (source.analyzeEnd(path, s, sEnd, multipleEnds))
			{
				wstring tempPhrase;
				if (reprocess = (s * 100 / source.m.size() < 99))
					wprintf(L"%50.50s (%02I64d): End detected at position %07d (totalWords=%07I64d) - %s\n", title.c_str(), s * 100 / source.m.size(), s, source.m.size(), source.phraseString(s, sEnd, tempPhrase, true, L" ").c_str());
				break;
			}
		}
	}
	else
		nosource = true;
	if (!myquery(&source.mysql, L"UNLOCK TABLES"))
		return -1;
	return 0;
}

int writeWordFormsFromCorpusWideAnalysis(MYSQL mysql,bool actuallyExecuteAgainstDB)
{
	int definedUnknownWord = 0, dictionaryComQueried = 0, dictionaryComCacheQueried = 0;
	int I = 0, sumTotalFrequency = 0, sumProcessedTotalFrequency = 0;
	bool websterAPIRequestsExhausted = false;
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	wstring word;
	if (!myquery(&mysql, L"LOCK TABLES wordfrequencymemory READ"))
		return -1;
	if (!myquery(&mysql, L"select SUM(totalFrequency) from wordfrequencymemory where unknownFrequency*100/totalFrequency>95 and"
		L" nonEuropeanFlag =false and numberFlag = false and cardinalFlag = false and	ordinalFlag = false and	romanFlag = false and dateFlag = false and timeFlag = false and	telephoneFlag = false and	moneyFlag = false and	webaddressFlag = false order by unknownFrequency desc", result))
		return -1;
	if (sqlrow = mysql_fetch_row(result))
		sumTotalFrequency = atoi(sqlrow[0]);
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select word, totalFrequency, unknownFrequency, capitalizedFrequency,allCapsFrequency,lastSourceEtext from wordfrequencymemory where unknownFrequency*100/totalFrequency>95 and"
		L" nonEuropeanFlag =false and numberFlag = false and cardinalFlag = false and	ordinalFlag = false and	romanFlag = false and dateFlag = false and timeFlag = false and	telephoneFlag = false and	moneyFlag = false and	webaddressFlag = false order by unknownFrequency desc");
	if (!myquery(&mysql, qt, result))
		return -1;
	my_ulonglong totalWords = mysql_num_rows(result), totalFormsDeleted=0;
	int numWordsProcessed = 0;
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&mysql, L"LOCK TABLES words WRITE,wordforms WRITE"))
			return -1;
	}
	else if (!myquery(&mysql, L"LOCK TABLES words READ,wordforms READ"))
			return -1;
	for (int row=0; sqlrow = mysql_fetch_row(result); row++)
	{
		int totalFrequency = 0, capitalizedFrequency = 0, allCapsFrequency = 0, unknownFrequency = 0, sourceId = 0;
		mTW(sqlrow[0], word);
		bool logEverything = true;
		totalFrequency = atoi(sqlrow[1]);
		sumProcessedTotalFrequency += totalFrequency;
		unknownFrequency = atoi(sqlrow[2]);
		capitalizedFrequency = atoi(sqlrow[3]);
		allCapsFrequency = atoi(sqlrow[4]);
		sourceId = atoi(sqlrow[5]);
		if (word.find_first_of(L"ãâäáàæçêéèêëîíïñôóòöõôûüùú\'") != wstring::npos && (totalFrequency < 25 || capitalizedFrequency * 100 / totalFrequency < 99))
			continue;
		// find definition in webster.
		// if exists, erase all forms associated with this word and write the new forms (return true)
		// else definition could not be found (return false)
		set <int> posSet;
		bool isNonEuropean, setProperNoun;
		int inflections=0;
		if (setProperNoun = (totalFrequency > 4 && ((capitalizedFrequency + allCapsFrequency)*100.0 / totalFrequency) > 95.0))
			posSet.insert(FormsClass::gFindForm(L"noun"));
		else
			getWordPOS(&mysql,word, posSet, inflections, false, isNonEuropean, dictionaryComQueried, dictionaryComCacheQueried, websterAPIRequestsExhausted,true);
		if (posSet.size() > 0)
		{
			int wordId;
			// convert posSet to DB form values
			set<int> posSetDB;
			for (int f : posSet)
				posSetDB.insert(f + 1);
			int ret = createWordInDBIfNecessary(mysql, sourceId, wordId, word, actuallyExecuteAgainstDB,logEverything);
			// remove all wordforms associated with this word and create new wordforms
			if ( ret< 0)
				lplog(LOG_FATAL_ERROR, L"DB error unable to create word %s", word.c_str());
			__int64 formsDeleted=0;
			if (overwriteWordFormsInDB(mysql, wordId, word, posSetDB, formsDeleted, setProperNoun, ret==0, actuallyExecuteAgainstDB, logEverything) < 0)
				lplog(LOG_FATAL_ERROR, L"DB error setting word forms with word %s", word.c_str());
			totalFormsDeleted += formsDeleted;
			if (setProperNoun && totalFrequency < 25 && overwriteWordFlagsInDB(mysql, word, actuallyExecuteAgainstDB,false) < 0)
				lplog(LOG_FATAL_ERROR, L"DB error setting word flags with word %s", word.c_str());
			if (inflections > 0 && overwriteWordInflectionFlagsInDB(mysql, word, inflections, actuallyExecuteAgainstDB) < 0)
				lplog(LOG_FATAL_ERROR, L"DB error setting word inflection flags with word %s", word.c_str());
			definedUnknownWord++;
		}
		numWordsProcessed++;
		// remember word for further sources
		wprintf(L"%03I64d:%15.15s:unknown=%06d/%06d webster=%06d dictionaryCom(%06d,cache=%06d) [UpperCase=%05.1f] [totalFormsDeleted=%08I64d] frequency %I64d%% done\r",
			numWordsProcessed*100/totalWords,word.c_str(), definedUnknownWord, numWordsProcessed, websterQueriedToday, dictionaryComQueried, dictionaryComCacheQueried,
			((capitalizedFrequency + allCapsFrequency)*100.0 / totalFrequency), totalFormsDeleted, ((__int64)sumProcessedTotalFrequency)*100/ sumTotalFrequency);
	}
	mysql_free_result(result);
	if (!myquery(&mysql, L"UNLOCK TABLES"))
		return -1;
	return 0;
}

int populateWordFrequencyTableFromSource(Source source, int sourceId,wstring path, wstring etext)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) 
		return -20;
	if (Words.readWithLock(source.mysql, sourceId, path, false, false, false) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	bool parsedOnly=false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, true))
	{
		unordered_map<wstring, wordInfo> wf;
		wf.reserve(source.m.size());
		for (WordMatch &im : source.m)
		{
			wstring word = im.word->first;
			if (word.empty() || word[word.length() - 1] == L'\\' || word.length()>32)
				continue;
			auto wfi=wf.find(word);
			if (wfi == wf.end())
			{
				wordInfo wi;
				wi.totalFrequency = 0;
				wi.unknownAllCapsFrequency = 0;
				wi.unknownCapitalizedFrequency = 0;
				wi.unknownFrequency = 0;
				wi.nonEuropeanWord = detectNonEuropeanWord(word);
				wi.number = im.queryForm(NUMBER_FORM_NUM) >= 0;
				wi.cardinal = im.queryForm(numeralCardinalForm) >= 0;
				wi.ordinal = im.queryForm(numeralOrdinalForm) >= 0;
				wi.roman = im.queryForm(romanNumeralForm) >= 0;
				wi.date = im.queryForm(dateForm) >= 0;
				wi.time = im.queryForm(timeForm) >= 0;
				wi.telephone = im.queryForm(telephoneNumberForm) >= 0;
				wi.money = im.queryForm(moneyForm) >= 0;
				wi.webaddress = im.queryForm(webAddressForm) >= 0;
				wfi = wf.insert({ word,wi }).first;
				if (im.word->second.isUnknown())
				{
					int numCharsIncorrect = 0;
					for (wchar_t c : word)
						if (!iswalnum(c) && !WordClass::isDoubleQuote(c) && !WordClass::isSingleQuote(c) && !WordClass::isDash(c) && c != '.' && c != ' ')
							numCharsIncorrect++;
					if (numCharsIncorrect >= 1)
					{
						lplog(LOG_INFO, L"%s: word %s is suspicious - rejecting source.", path.c_str(), word.c_str());
						numIllegalWords++;
					}
					if (im.flags&WordMatch::flagFirstLetterCapitalized)
					{
						if (!myquery(&source.mysql, L"LOCK TABLES words w READ,wordforms wf READ"))
							return -1;
						MYSQL_RES * result;
						MYSQL_ROW sqlrow = NULL;
						wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
						escapeSingleQuote(word);
						_snwprintf(qt, QUERY_BUFFER_LEN, L"select wf.formId from words w,wordforms wf where w.id=wf.wordId and w.word='%s'", word.c_str());
						int form = -1;
						if (myquery(&source.mysql, qt, result, true))
						{
							if (mysql_num_rows(result) == 1 && (sqlrow = mysql_fetch_row(result)))
								form = atoi(sqlrow[0]);
							mysql_free_result(result);
						}
						source.unlockTables();
						if (form == 4)
							return -2;
					}
				}
			}
			wfi->second.totalFrequency++;
			if (im.word->second.isUnknown())
			{
				wfi->second.unknownFrequency++;
				if (im.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (im.flags&WordMatch::flagFirstLetterCapitalized))
					wfi->second.unknownCapitalizedFrequency++;
				if (im.flags&WordMatch::flagAllCaps)
					wfi->second.unknownAllCapsFrequency++;
			}
		}
		if (numIllegalWords==0)
			writeSourceWordFrequency(&source.mysql, wf, etext);
	}
	else
	{
		wprintf(L"Unable to read source %d:%s\n", sourceId,path.c_str());
		return 0;
	}
	return (numIllegalWords==0) ? 2 : -(numIllegalWords+10);
}

int populateWordFrequencyTableMP(Source source)
{
	int step = 1;
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum Source::sourceTypeEnum st = Source::GUTENBERG_SOURCE_TYPE;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select COUNT(*) from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**'", st);
	__int64 totalSource;
	if (myquery(&source.mysql, qt, result))
	{
		sqlrow = mysql_fetch_row(result);
		totalSource = atoi(sqlrow[0]);
		mysql_free_result(result);
	}
	bool websterAPIRequestsExhausted = false;
	int startTime = clock();
	while (true)
	{
		int sourcesLeft = 0;
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select COUNT(*) from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d", st, step);
		if (myquery(&source.mysql, qt, result))
		{
			sqlrow = mysql_fetch_row(result);
			sourcesLeft = atoi(sqlrow[0]);
			mysql_free_result(result);
		}
		if (!myquery(&source.mysql, L"START TRANSACTION"))
			return -1;
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id limit 1 FOR UPDATE SKIP LOCKED", st, step);
		if (!myquery(&source.mysql, qt, result) || mysql_num_rows(result) != 1)
			break;
		wstring path, etext, title;
		sqlrow = mysql_fetch_row(result);
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = L"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		mysql_free_result(result);
		path.insert(0, L"\\").insert(0, CACHEDIR);
		/*
		bool reprocess = false, nosource = false;
		if (analyzeEnd(source, sourceId, path, etext,title,reprocess,nosource) >= 0)
		{
			int setStep = step + 1;
			if (reprocess)
				setStep = step - 1;
			if (nosource)
				setStep = 0;
			_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set proc2=%d where id=%d", setStep, sourceId);
			if (!myquery(&source.mysql, qt))
				break;
		}
		*/
		int setStep = populateWordFrequencyTableFromSource(source, sourceId, path, etext);
		_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		if (!myquery(&source.mysql, L"COMMIT"))
			return -1;
		source.clearSource();
		wchar_t buffer[1024];
		__int64 processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		int numSourcesProcessedNow = (int)(totalSource - (sourcesLeft - 1));
		if (processingSeconds)
			wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d source in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow - 1, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		SetConsoleTitle(buffer);
	}
	return 0;
}

int populateWordFrequencyTable(Source source, int step)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum Source::sourceTypeEnum st = Source::GUTENBERG_SOURCE_TYPE;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	bool websterAPIRequestsExhausted = false;
	int startTime = clock(), numSourcesProcessedNow = 0;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		wstring path, etext, title;
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = L"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		path.insert(0, L"\\").insert(0, CACHEDIR);
		int setStep = populateWordFrequencyTableFromSource(source, sourceId, path, etext);
		_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		wchar_t buffer[1024];
		__int64 processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
			wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		SetConsoleTitle(buffer);
	}
	mysql_free_result(result);
	return 0;
}

// delete files associated with sources that have been marked skipped
// update sources set proc2 = 0;
// update sources set proc2 = 6 where start = '**SKIP**';
int removeOldCacheFiles(Source source)
{
	int step = 6;
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select COUNT(*) from sources where proc2=%d",step);
	__int64 totalSource;
	if (myquery(&source.mysql, qt, result))
	{
		sqlrow = mysql_fetch_row(result);
		totalSource = atoi(sqlrow[0]);
		mysql_free_result(result);
	}
	int startTime = clock();
	while (true)
	{
		int sourcesLeft = 0;
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select COUNT(*) from sources where proc2=%d", step);
		if (myquery(&source.mysql, qt, result))
		{
			sqlrow = mysql_fetch_row(result);
			sourcesLeft = atoi(sqlrow[0]);
			mysql_free_result(result);
		}
		if (!myquery(&source.mysql, L"START TRANSACTION"))
			return -1;
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select id,path from sources where proc2=%d order by id limit 1 FOR UPDATE SKIP LOCKED", step);
		if (!myquery(&source.mysql, qt, result) || mysql_num_rows(result) != 1)
			break;
		wstring path;
		sqlrow = mysql_fetch_row(result);
		int sourceId = atoi(sqlrow[0]);
		mTW(sqlrow[1], path);
		mysql_free_result(result);
		path.insert(0, L"\\").insert(0, CACHEDIR);
		wstring temp = path;
		temp += L".SourceCache";
		_wremove(temp.c_str());
		temp = path;
		temp += L".WNCache";
		_wremove(temp.c_str());
		temp = path;
		temp += L".WordCacheFile";
		_wremove(temp.c_str());
		_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set proc2=%d where id=%d", step + 1, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		if (!myquery(&source.mysql, L"COMMIT"))
			return -1;

		source.clearSource();
		wchar_t buffer[1024];
		__int64 processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		int numSourcesProcessedNow = (int)(totalSource - (sourcesLeft - 1));
		if (processingSeconds)
			wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d source in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow - 1, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, path.c_str());
		SetConsoleTitle(buffer);
	}
	return 0;
}

void testRDFType(Source &source)
{
	int sourceId = 25291;
	wstring path = L"J:\\caches\\texts\\Schoonover, Frank E\\Jules of the Great Heart  Free Trapper and Outlaw in the Hudson Bay Region in the Early Days.txt";
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) return;
	if (Words.readWithLock(source.mysql, sourceId, path, false, false, false) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	vector <cTreeCat *> rdfTypes;
	Ontology::rdfIdentify(L"clackamas", rdfTypes, L"Z", true);
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, true))
	{
		int where = 0;
		for (auto im : source.m)
		{
			if (im.word->first == L"maac")
			{
				vector <cTreeCat *> rdfTypes;
				source.getRDFTypes(where, rdfTypes, L"Z", 1000, false, false);
			}
			where++;
		}
	}
}

struct {
	wchar_t *commonWord;
	wchar_t *replace;
} doubleReplace[] = {
	{ L"d'ye", L"do you" },
	{ L"more'n", L"more than" },
	{ L"d'you", L"do you" },
	{ L"t'other", L"the other" },
	{ L"dinna", L"didn't you" },
};

struct {
	wchar_t *commonWord;
	wchar_t *replace;
} singleReplace[] = {
{ L"thar", L"there" },
{ L"sez", L"says" },
{ L"cap'n", L"captain" },
{ L"s'pose", L"suppose" },
{ L"thet", L"that" },
{ L"mebbe", L"maybe" },
{ L"ther", L"there" },
{ L"ze", L"the" },
{ L"yuh", L"yes" },
{ L"hoss", L"boss" },
{ L"sah", L"sir" },
{ L"tak", L"take" },
{ L"dere", L"there" },
{ L"havin", L"having" },
{ L"h'm", L"him" },
{ L"suh", L"sir" },
{ L"livin", L"living" },
{ L"waitin", L"waiting" },
{ L"seein", L"seeing" },
{ L"purty", L"pretty" },
{ L"ag'in", L"again" },
{ L"leetle", L"little" },
{ L"haf", L"half" },
{ L"meetin", L"meeting" },
{ L"thim", L"them" },
{ L"shewn", L"shown" },
{ L"iz", L"is" },
{ L"gittin", L"getting" },
{ L"sho", L"sure" },
{ L"heah", L"hear" },
{ L"givin", L"giving" },
{ L"reg'lar", L"regular" },
{ L"mebby", L"maybe" },
{ L"on'y", L"only" },
{ L"humouredly", L"humoredly" },
{ L"wud", L"would" },
{ L"m'sieu", L"monsieur" },
{ L"p'raps", L"perhaps" },
{ L"wur", L"were" },
{ L"o’er", L"over" },
{ L"dont", L"don't" },
{ L"fightin", L"fighting" },
{ L"theer", L"there" },
{ L"f'r", L"for" },
{ L"gainst", L"against" },
{ L"iver", L"ever" },
{ L"ev'ry", L"every" },
{ L"bloomin", L"blooming" },
{ L"whut", L"what" },
{ L"lyin", L"lying" },
{ L"puttin", L"putting" },
{ L"worshiped", L"worshipped" },
{ L"didna", L"didn't" },
{ L"hangin", L"hanging" },
{ L"b'lieve", L"believe" },
{ L"lemme", L"let me" },
{ L"huntin", L"hunting" },
{ L"speakin", L"speaking" },
{ L"sunshiny", L"sunshiney" },
{ L"for'ard", L"forward" }
};

// L"thank",L"no",L"never",L"then",,L"so",L"number",L"as",L"only", L"van",L"von",L"also",L"not",L"more",L"eg",L"e.g.",L"p.o.",L"like",L" - only used in very specialized patterns
// 	L"p",L"m",L"le",L"de",L"f",L"c",L"k",L"o",L"b",L"!",L"?",
unordered_map<wstring, vector <wstring> > maxentAssociationMap =
{
	// amplification
	//{L"sectionheader", L"noun"},

	// similarity 
	//{ L"coordinator",{ L"conjunction"} },
	{ L"conjunction",{ L"coordinator",L"and",L"as",L"if",L"so" } },
	{ L"Proper Noun",{ L"honorific_abbreviation",L"honorific",L"roman_numeral",L"month",L"interjection",L"daysOfWeek",L"no",L"holiday" } },
	{ L"honorific noun",{ L"honorific",L"honorific_abbreviation" }},
	{ L"modal_auxiliary",{ L"future_modal_auxiliary",L"negation_modal_auxiliary",L"negation_future_modal_auxiliary"} },
	{ L"determiner",{ L"demonstrative_determiner",L"no",L"the"} },
	{ L"relativizer", { L"what" } },
	{ L"particle", { L"adverb" } }, // LP usually treats particles as adverbs, not sure whether this is strictly correct, but it makes sense to me.
	// include possible subclasses
	{ L"verb", { L"verbverb",L"think",L"have",L"is",L"does",L"be",L"does_negation",L"is_negation",L"been",L"negation_modal_auxiliary",L"modal_auxiliary"} }, // feel, see, watch, hear, tell etc // fancy, say (thinksay verbs)
	// stanford maxent apparently has no indefinite pronoun, so it classes them all as nouns.
	{ L"noun",{ L"dayUnit",L"timeUnit",L"quantifier",L"numeral_cardinal",L"indefinite_pronoun" } }, // all, some etc // something, everything
	{ L"adjective",{ L"quantifier",L"numeral_ordinal" }},  // many
	{ L"adverb",{ L"not",L"never",L"then" }},  // many
	{ L"to",{ L"preposition" }},
	{ L"of",{ L"preposition" }},
	{ L"and",{ L"coordinator" }},
	{ L"or",{ L"coordinator" }},
	{ L"if",{ L"conjunction" }},
	{ L"the",{ L"determiner" }},
	{ L"you",{ L"personal_pronoun" }},
	{ L"his",{ L"possessive_determiner",L"reflexive_pronoun" }},
	{ L"her",{ L"possessive_determiner",L"reflexive_pronoun" }},
	{ L"there",{ L"pronoun",L"adverb" }},
	{ L"once",{ L"predeterminer" }},
	{ L"twice",{ L"predeterminer" }},
	{ L"both",{ L"predeterminer" }},
	{ L"but",{ L"conjunction",L"preposition" }},
	{ L"than",{ L"conjunction",L"preposition" }},
	{ L"box",{ L"noun",L"verb" }},
	{ L"ex",{ L"adjective",L"adverb" }},
	{ L"which",{ L"interrogative_determiner",L"interrogative_pronoun",L"relativizer"}},
	{ L"what",{ L"interrogative_determiner",L"interrogative_pronoun",L"relativizer"}},
	{ L"who",{ L"interrogative_pronoun",L"relativizer"}},
	{ L"whose",{ L"interrogative_determiner",L"relativizer"}},
	{ L"how",{ L"relativizer",L"conjunction",L"adverb"}},
	{ L"less",{ L"pronoun" }}
};
// checks if the part of speech indicated in parse from the Stanford Maxent POS tagger matches the winner forms at wordSourceIndex.
// returns yes=0, no=1
int checkStanfordMaxentAgainstWinner(Source &source, int wordSourceIndex, wstring originalParse, wstring &parse, int &numPOSNotFound, unordered_map<wstring, int> &noMatchMap,bool inRelativeClause)
{
	if (!iswalpha(source.m[wordSourceIndex].word->first[0]))
		return 0;
	wstring originalWordSave;
	source.getOriginalWord(wordSourceIndex, originalWordSave, false, false);
	// tagger output:
	// ;_: and_CC bunny_NN ,_, and_CC bobtail_NN ,_, and_CC billy_NNP were_VBD always_RB doing_VBG something_NN 
	wstring originalWord = L" " + originalWordSave + L"_";
	if (parse.empty())
		return 1;
	if (parse[0] != L' ')
		parse = L" " + parse;
	size_t wow = parse.find(originalWord);
	if (wow == wstring::npos)
	{
		transform(originalWord.begin(), originalWord.end(), originalWord.begin(), (int(*)(int)) tolower);
		wow = parse.find(originalWord);
	}
	if (wow == wstring::npos)
		return 1;
	if (parse[parse.length() - 1] != L' ')
		parse += L" ";
	wow += originalWord.length();
	auto nextspace = parse.find(L' ', wow);
	if (nextspace == wstring::npos)
		return 1;
	wstring partofspeech = parse.substr(wow, nextspace - wow);
	parse.erase(0, nextspace);
	extern unordered_map<wstring, vector<wstring>> pennMapToLP;
	auto lpPOS = pennMapToLP.find(partofspeech);
	if (lpPOS == pennMapToLP.end())
	{
		lplog(LOG_ERROR, L"Part of Speech %s not found.", partofspeech.c_str());
		numPOSNotFound++;
		return 1;
	}
	vector<wstring> posList = lpPOS->second;
	for (auto pos : lpPOS->second)
	{
		auto imai = maxentAssociationMap.find(pos);
		if (imai != maxentAssociationMap.end())
		{
			for (auto ampi : imai->second)
				posList.push_back(ampi);
		}
	}
	vector <int> winnerForms;
	source.m[wordSourceIndex].getWinnerForms(winnerForms);
	for (int wf : winnerForms)
	{
		if (std::find(posList.begin(), posList.end(), Forms[wf]->name) != posList.end())
			return 0;
	}
	//////////////////////////////
	// corrections based on implementation/interpretation differences and statistical findings
	// with maxent, if LP thinks it is a ProperNoun, it is always correct, compared with maxent which thinks it is a noun.
	if (posList[0] == L"noun" && std::find(winnerForms.begin(), winnerForms.end(), PROPER_NOUN_FORM_NUM) != winnerForms.end())
		return 0;
	// Maxent sometimes thinks things are proper nouns, when they are not capitalized.  I have not found an example where this is the case.
	if (posList[0] == L"Proper Noun" &&
		//(std::find(winnerForms.begin(), winnerForms.end(), nounForm) != winnerForms.end() || std::find(winnerForms.begin(), winnerForms.end(), adjectiveForm) != winnerForms.end()) && 
		iswlower(originalWordSave[0]))
		return 0;
	// Maxent sometimes thinks things are proper nouns, when they are actually just other forms, even if theyare capitalized, usually when they are the first word.
	// I have not found an example where this is the case.
	vector <wstring> pnExceptionForms =
	{ L"pronoun", L"pronoun possessive_pronoun", L"conjunction", L"determiner",
		 L"does_negation", L"have_negation", L"indefinite_pronoun", L"is",
		 L"is_negation", L"le", L"modal_auxiliary", L"negation_future_modal_auxiliary",
		 L"negation_modal_auxiliary", L"numeral_ordinal", L"personal_pronoun_nominative",
		 L"polite_inserts", L"sectionheader", L"street_address", L"think",
		 L"trademark" };
	if (posList[0] == L"Proper Noun")
	{
		for (wstring form : pnExceptionForms)
		{
			if (std::find(winnerForms.begin(), winnerForms.end(), FormsClass::findForm(form)) != winnerForms.end())
				return 0;
		}
	}
	// that is always noted by LP as a demonstrative determiner, but is used in REL phrases, which is equivalent to an IN usage that is matched by maxent.
	if (originalWordSave == L"that" && inRelativeClause)
		return 0;
	// these cases have been proven by examination to be either maxent mistakes or Proper Nouns used as adjectives (which is more of an implementation difference)
	if (posList[0] == L"adjective" && std::find(winnerForms.begin(), winnerForms.end(), PROPER_NOUN_FORM_NUM) != winnerForms.end())
		return 0;
	/////////////////////////////
	wstring posListStr;
	for (auto pos : posList)
		posListStr += pos + L" ";
	wstring winnerFormsString;
	source.m[wordSourceIndex].winnerFormString(winnerFormsString, false);
	noMatchMap[posListStr + L"!= "+winnerFormsString]++;
	lplog(LOG_ERROR, L"%d:Stanford POS %s (%s) not found in winnerForms %s for word %s [%s].", wordSourceIndex, partofspeech.c_str(), posListStr.c_str(), winnerFormsString.c_str(), originalWordSave.c_str(), originalParse.c_str());
	return 1;
}

int maxentCheckFromSource(Source &source, int sourceId, wstring path, JavaVM *vm,JNIEnv *env, int &numNoMatch, int &numPOSNotFound, unordered_map<wstring, int> &noMatchMap)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	if (Words.readWithLock(source.mysql, sourceId, path, false, false, false) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	bool parsedOnly = false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, true))
	{
		lplog(LOG_INFO, L"source %d:%s", sourceId, path.c_str());
		lplog(LOG_ERROR, L"source %d:%s", sourceId, path.c_str());
		int lastPercent=-1;
		for (int wordSourceIndex = 0; wordSourceIndex < source.m.size(); )
		{
			int currentPercent = wordSourceIndex * 100 / source.m.size();
			if (lastPercent < currentPercent)
			{
				printf("maxentCheck %03d%% done - %09d out of %09I64d\r", currentPercent, wordSourceIndex, source.m.size());
				lastPercent = currentPercent;
			}
			int endIndex = wordSourceIndex;
			while (endIndex < source.m.size() && !source.isEOS(endIndex) && source.m[endIndex].word != Words.sectionWord)
				endIndex++;
			if (endIndex < source.m.size() && source.isEOS(endIndex))
				endIndex++;
			wstring sentence;
			for (int I = wordSourceIndex; I < endIndex; I++)
			{
				wstring originalIWord;
				source.getOriginalWord(I, originalIWord, false, false);
				sentence += originalIWord + L" ";
			}
			wstring parse;
			if (parseSentence(env, sentence, parse, false) < 0)
				return -1;
			wstring originalParse = parse;
			int start = wordSourceIndex, until = -1,pmaOffset=-1;
			bool inRelativeClause = false;
			for (; wordSourceIndex < endIndex; wordSourceIndex++)
			{
				if ((pmaOffset = source.scanForTag(wordSourceIndex, REL_TAG)) != -1 || (pmaOffset = source.scanForTag(wordSourceIndex, SENTENCE_IN_REL_TAG)) != -1)
				{
					inRelativeClause = true;
					until = wordSourceIndex+source.m[wordSourceIndex].pma[pmaOffset].len;
				}
				if (checkStanfordMaxentAgainstWinner(source, wordSourceIndex, originalParse, parse, numPOSNotFound, noMatchMap, inRelativeClause))
				{
					numNoMatch++;
					if (source.m[wordSourceIndex].word->first == L"that" && !inRelativeClause)
						source.printSentence(SCREEN_WIDTH, start, endIndex, true);
				}
				if (wordSourceIndex == until)
					inRelativeClause = false;
			}
			if (source.m[wordSourceIndex].word == Words.sectionWord)
				wordSourceIndex++;
		}
	}
	else
	{
		wprintf(L"Unable to read source %d:%s\n", sourceId, path.c_str());
		return -1;
	}
	return 10;
}

int maxentCheck(Source source, int step)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum Source::sourceTypeEnum st = Source::GUTENBERG_SOURCE_TYPE;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	bool websterAPIRequestsExhausted = false;
	int startTime = clock(), numSourcesProcessedNow = 0;
	if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE"))
		return -1;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	JavaVM *vm;
	JNIEnv *env;
	createJavaVM(vm, env);
	unordered_map<wstring, int> noMatchMap;
	int numNoMatch = 0, numPOSNotFound = 0,totalWords=0;
	my_ulonglong totalSource = mysql_num_rows(result);
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		wstring path, etext, title;
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = L"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		path.insert(0, L"\\").insert(0, CACHEDIR);
		int setStep = maxentCheckFromSource(source, sourceId, path, vm, env, numNoMatch, numPOSNotFound, noMatchMap);
		totalWords += source.m.size();
		_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		wchar_t buffer[1024];
		__int64 processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
			wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		SetConsoleTitle(buffer);
	}
	mysql_free_result(result);
	destroyJavaVM(vm);
	if (totalWords>0)
		lplog(LOG_ERROR, L"numNoMatch=%d %d%% numPOSNotFound=%d", numNoMatch, numNoMatch * 100 / totalWords, numPOSNotFound);
	map<int, wstring, std::greater<int>> noMatchReverseMap;
	for (auto const&[forms, count] : noMatchMap)
		noMatchReverseMap[count] = forms;
	for (auto const&[count, forms] : noMatchReverseMap)
		lplog(LOG_ERROR, L"forms %s [%d]", forms.c_str(), count);
	return 0;
}

// this would be easier if you had all the sources in memory at once!
int testViterbiHMMMultiSource(Source &source,wchar_t *databaseHost,int step)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum Source::sourceTypeEnum st = Source::GUTENBERG_SOURCE_TYPE;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numSourcesProcessedNow = 0;

	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	//Generate vocabulary
	Source childSource(databaseHost, st, false, false, true);
	childSource.initializeNounVerbMapping();
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -1;
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		int sourceId =atoi(sqlrow[0]);
		wstring path, title;
		mTW(sqlrow[1], path);
		mTW(sqlrow[2], title);
		path.insert(0, L"\\").insert(0, CACHEDIR);
		bool parsedOnly = false;
		if (Words.readWithLock(source.mysql, sourceId, path, false, false, false) < 0)
			lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
		Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
		//unordered_map <int, vector < vector <tTagLocation> > > emptyMap;
		//for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
		//	childSource.pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
		if (childSource.readSource(path, false, parsedOnly, false, true))
		{
			lplog(LOG_ERROR,L"Beginning child source %d:%s at offset %d.", sourceId,path.c_str(), source.m.size());
			source.copySource(&childSource, 0, childSource.m.size());
		}
		else
		{
			wprintf(L"Unable to read source %d:%s\n", sourceId,path.c_str());
		}
		childSource.clearSource();
		wchar_t buffer[1024];
		__int64 processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
			wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		SetConsoleTitle(buffer);
	}
	mysql_free_result(result);
	int wordNum = 0;
	for (WordMatch im : source.m)
	{
		if (im.formsSize() == 0)
		{
			lplog(LOG_ERROR, L"Viterbi force form %d:%s", wordNum, im.word->first.c_str());
			im.word->second.addForm(1, im.word->first);
		}
		if (im.getForms().size()==0)
		{
			if (im.flags&WordMatch::flagOnlyConsiderProperNounForms)
			{
				lplog(LOG_ERROR, L"Viterbi proper noun form mandatory added %d:%s", wordNum, im.word->first.c_str());
				im.word->second.addForm(PROPER_NOUN_FORM_NUM, im.word->first);
			}
			else
			{
				lplog(LOG_ERROR, L"Viterbi illegal proper noun form added noun form %d:%s", wordNum, im.word->first.c_str());
				im.word->second.addForm(nounForm, im.word->first);
			}
		}
		wordNum++;
	}
	wstring temp;
	source.sourcePath = L"M:\\caches\\texts\\hmmViterbiMultiSource." + itos((int)totalSource, temp);
	testViterbiFromSource(source);
	return 0;
}


int numSourceLimit = 0;
// to begin proc2 field in all sources must be set to 1
// step = 1 - accumulate word frequency statistics - will set to 2 when finished.
// step = 2 - evaluate statistics and create database statements to decrease the number of unknown words
void wmain(int argc,wchar_t *argv[])
{
	setConsoleWindowSize(150, 15);
	chdir("..");
	initializeCounter();
	cacheDir = CACHEDIR;
	wchar_t *databaseHost = L"localhost";
	enum Source::sourceTypeEnum st = Source::GUTENBERG_SOURCE_TYPE;
	Source source(databaseHost, st, false, false, true);
	source.initializeNounVerbMapping();
	initializePatterns();
	Internet::bandwidthControl = CLOCKS_PER_SEC / 2;
	unordered_map <int, vector < vector <tTagLocation> > > emptyMap;
	for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
		source.pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
	if (!myquery(&source.mysql, L"LOCK TABLES sources READ"))
		return;
	//testDisinclination();
	//writeFrequenciesToDB(source);
	//if (true)
	//	return;
	nounForm = FormsClass::gFindForm(L"noun");
	verbForm = FormsClass::gFindForm(L"verb");
	adjectiveForm = FormsClass::gFindForm(L"adjective");
	adverbForm = FormsClass::gFindForm(L"adverb");
	int step = _wtoi(argv[1]);
	switch (step)
	{
	case 1:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
	case 100:
	case 1000:
	case 10000:
		populateWordFrequencyTable(source,step);
		break;
	case 2:
		{
			bool actuallyExecuteAgainstDB = wstring(argv[2]) == L"executeAgainstDB";
			writeWordFormsFromCorpusWideAnalysis(source.mysql, actuallyExecuteAgainstDB);
		}
		break;
	case 3:
		{
			source.unlockTables();
			int numFilesProcessed = 0, numNotOpenable = 0, numNewestVersion = 0, numOldVersion = 0, removeErrors = 0, populatedRDFs = 0, numERDFRemoved = 0;
			if (!myquery(&source.mysql, L"LOCK TABLES noRDFTypes WRITE, noERDFTypes WRITE"))
				return;
			FILE *progressFile = _wfopen(L"RDFTypesScanProgress.txt", L"r");
			wchar_t startPath[2048];
			bool startHit = false;
			if (progressFile)
			{
				fgetws(startPath, 2048, progressFile);
				fclose(progressFile);
			}
			else
				startHit = true;
			unordered_map<wstring, int> extensions;
			unordered_map<wstring, __int64> extensionSpace;
			scanAllRDFTypes(source.mysql, startPath, startHit, L"M:\\dbPediaCache", numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs, numERDFRemoved, extensions, extensionSpace);
		}
		break;
	case 4:
		source.unlockTables();
		{
			int numFilesProcessed = 0, numNotOpenable = 0, filesRemoved = 0, removeErrors = 0;
			if (!myquery(&source.mysql, L"LOCK TABLES notwords WRITE"))
				return;
			scanAllDictionaryDotCom(source.mysql, L"J:\\caches\\DictionaryDotCom", numFilesProcessed, numNotOpenable, filesRemoved, removeErrors);
		}
		break;
	case 5:
		source.unlockTables();
		removeIllegalNames(L"M:\\dbPediaCache");
		break;
	case 6:
		source.unlockTables();
		removeOldCacheFiles(source);
		break;
	case 7:
		testRDFType(source);
		break;
	case 8:
		testViterbiHMMMultiSource(source,databaseHost,step);
		break;
	case 9: 
		maxentCheck(source,step);
		break;
	}
	source.unlockTables();
}


