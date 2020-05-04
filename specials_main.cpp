#include <windows.h>
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
#include "thread"
#include "future"
#include "mutex"
#include "stacktrace.h"

void getSentenceWithTags(Source &source, int patternBegin, int patternEnd, int sentenceBegin, int sentenceEnd, int PEMAPosition, wstring &sentence);

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
bool TSROverride = false, flipTOROverride = false, flipTNROverride = false, logMatchedSentences=false, logUnmatchedSentences=false;

void printStackTrace()
{
	std::stringstream buff;
	buff << ":  General Software Fault! \n";
	buff << "\n";

	std::vector<dbg::StackFrame> stack = dbg::stack_trace();
	buff << "Callstack: \n";
	for (unsigned int i = 0; i < stack.size(); i++)
	{
		buff << "0x" << std::hex << stack[i].address << ": " << stack[i].name << "(" << std::dec << stack[i].line << ") in " << stack[i].module << "\n";
	}
	::lplog(LOG_FATAL_ERROR, L"%S", buff.str().c_str());
}

void no_memory () {
	lplog(LOG_FATAL_ERROR,L"Out of memory (new/STL allocation).");
	exit (1);
}

int createLPProcess(int numProcess, HANDLE &processHandle, DWORD &processId, wchar_t *commandPath, wchar_t *processParameters)
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
	processHandle = pi.hProcess;
	processId = pi.dwProcessId;
	return 0;
}

int getNumSourcesProcessed(Source &source, int &numSourcesProcessed, __int64 &wordsProcessed, __int64 &sentencesProcessed)
{
	MYSQL_RES * result;
	if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE")) return -1;
	if (myquery(&source.mysql, L"select COUNT(id), SUM(numWords), SUM(numSentences) from sources where sourceType = 2 and processed IS not NULL and processing IS NULL and start != '**SKIP**' and start != '**START NOT FOUND**'", result))
	{
		MYSQL_ROW sqlrow = NULL;
		if (sqlrow = mysql_fetch_row(result))
		{
			numSourcesProcessed = atoi(sqlrow[0]);
			wordsProcessed = atol(sqlrow[1]);
			sentencesProcessed = atoi(sqlrow[2]);
		}
		mysql_free_result(result);
	}
	if (!myquery(&source.mysql, L"UNLOCK TABLES")) return -1;
	return 0;
}

// https://stackoverflow.com/questions/813086/can-i-send-a-ctrl-c-sigint-to-an-application-on-windows/1179124
// Inspired from http://stackoverflow.com/a/15281070/1529139
// and http://stackoverflow.com/q/40059902/1529139
bool signalCtrl(DWORD dwProcessId, DWORD dwCtrlEvent)
{
	bool success = false;
	DWORD thisConsoleId = GetCurrentProcessId();
	// Leave current console if it exists
	// (otherwise AttachConsole will return ERROR_ACCESS_DENIED)
	bool consoleDetached = (FreeConsole() != FALSE);

	if (AttachConsole(dwProcessId) != FALSE)
	{
		// Add a fake Ctrl-C handler for avoid instant kill is this console
		// WARNING: do not revert it or current program will be also killed
		SetConsoleCtrlHandler(nullptr, true);
		success = (GenerateConsoleCtrlEvent(dwCtrlEvent, 0) != FALSE);
		FreeConsole();
	}

	if (consoleDetached)
	{
		// Create a new console if previous was deleted by OS
		if (AttachConsole(thisConsoleId) == FALSE)
		{
			int errorCode = GetLastError();
			if (errorCode == 31) // 31=ERROR_GEN_FAILURE
			{
				AllocConsole();
			}
		}
	}
	return success;
}

int startProcesses(Source &source, int processKind, int step, int beginSource, int endSource, Source::sourceTypeEnum st, int maxProcesses, int numSourcesPerProcess, bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite, bool makeCopyBeforeSourceWrite, bool parseOnly, wstring specialExtension)
{
	LFS
		chdir("source");
	bool sentBreakSignals = false;
	int startTime = clock();
	HANDLE *handles = (HANDLE *)calloc(maxProcesses, sizeof(HANDLE));
	int numProcesses = 0, errorCode = 0, numSourcesProcessedOriginally = 0;
	__int64 wordsProcessedOriginally = 0, sentencesProcessedOriginally = 0;
	getNumSourcesProcessed(source, numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally);
	int numSourcesLeft = source.getNumSources(st, true);
	maxProcesses = min(maxProcesses, numSourcesLeft);
	wstring tmpstr;
	while (!errorCode)
	{
		unsigned int nextProcessIndex = numProcesses;
		if (numProcesses == maxProcesses)
		{
			nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, false, 1000 * 60 * 5, false);
			numSourcesLeft = 0;
			int numSourcesProcessedNow = 0;
			__int64 wordsProcessedNow = 0, sentencesProcessedNow = 0;
			getNumSourcesProcessed(source, numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
			int processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
			wchar_t consoleTitle[1500];
			numSourcesProcessedNow -= numSourcesProcessedOriginally;
			wordsProcessedNow -= wordsProcessedOriginally;
			sentencesProcessedNow -= sentencesProcessedOriginally;
			wsprintf(consoleTitle, L"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].",
				numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, wordsProcessedNow * 3600 / processingSeconds);
			lplog(LOG_INFO | LOG_ERROR, L"%s", consoleTitle);
			SetConsoleTitle(consoleTitle);

			if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
				continue;
			if (nextProcessIndex == WAIT_FAILED)
				lplog(LOG_FATAL_ERROR, L"WaitForMultipleObjectsEx failed with error %s", getLastErrorMessage(tmpstr));
			if (nextProcessIndex < WAIT_OBJECT_0 + numProcesses) // nextProcessIndex >= WAIT_OBJECT_0 && 
			{
				nextProcessIndex -= WAIT_OBJECT_0;
				CloseHandle(handles[nextProcessIndex]);
				printf("\nClosing process %d", nextProcessIndex);
			}
			if (nextProcessIndex >= WAIT_ABANDONED_0 && nextProcessIndex < WAIT_ABANDONED_0 + numProcesses)
			{
				nextProcessIndex -= WAIT_ABANDONED_0;
				printf("\nClosing process %d [abandoned]", nextProcessIndex);
				CloseHandle(handles[nextProcessIndex]);
			}
		}
		int id, repeatStart, prId;
		wstring start, path, encoding, etext, author, title, pathInCache;
		bool result = true;
		if (processKind == 1 && numSourcesLeft > 0)
			numSourcesLeft--;
		else
		{
			switch (processKind)
			{
			case 0:result = source.getNextUnprocessedParseRequest(prId, pathInCache); break;
			case 1:result = source.getNextUnprocessedSource(beginSource, endSource, st, false, id, path, encoding, start, repeatStart, etext, author, title); break;
			case 2:result = source.anymoreUnprocessedForUnknown(st, step); break;
			default:result = false; break;
			}
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
				printf("\nNo more processes to be created. %d processes left to wait for.", numProcesses);
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
		if (exitNow || exitEventually)
		{
			printf("\nSending break signals to children...\n");
			if (!sentBreakSignals)
			{
				for (int p = 0; p < numProcesses; p++)
				{
					int pid = GetProcessId(handles[p]);
					signalCtrl(pid, CTRL_C_EVENT);
				}
				sentBreakSignals = true;
			}
		}
		else
		{
			HANDLE processHandle = 0;
			DWORD processId = 0;
			wchar_t processParameters[1024];
			switch (processKind)
			{
			case 0:
				wsprintf(processParameters, L"releasex64\\lp.exe -ParseRequest \"%s\" -cacheDir %s %s%s%s%s%s%s-log %d", pathInCache.c_str(), CACHEDIR,
					(forceSourceReread) ? L"-forceSourceReread " : L"",
					(sourceWrite) ? L"-SW " : L"",
					(sourceWordNetRead) ? L"-SWNR " : L"",
					(sourceWordNetWrite) ? L"-SWNW " : L"",
					(parseOnly) ? L"-parseOnly " : L"",
					(makeCopyBeforeSourceWrite) ? L"-MCSW " : L"",
					nextProcessIndex);
				if (errorCode = createLPProcess(nextProcessIndex, processHandle, processId, L"releasex64\\lp.exe", processParameters) < 0)
					break;
				break;
			case 1:
				wsprintf(processParameters, L"releasex64\\lp.exe -book 0 + -BC 0 -cacheDir %s %s%s%s%s%s%s-numSourceLimit %d -log %d", CACHEDIR,
					(forceSourceReread) ? L"-forceSourceReread " : L"",
					(sourceWrite) ? L"-SW " : L"",
					(sourceWordNetRead) ? L"-SWNR " : L"",
					(sourceWordNetWrite) ? L"-SWNW " : L"",
					(parseOnly) ? L"-parseOnly " : L"",
					(makeCopyBeforeSourceWrite) ? L"-MCSW " : L"",
					numSourcesPerProcess,
					nextProcessIndex);
				if (specialExtension.length() > 0)
				{
					wcscat(processParameters, L" -specialExtension ");
					wcscat(processParameters, specialExtension.c_str());
				}
				if (errorCode = createLPProcess(nextProcessIndex, processHandle, processId, L"releasex64\\lp.exe", processParameters) < 0)
					break;
				break;
			case 2:
				wsprintf(processParameters, L"releasex64\\CorpusAnalysis.exe -step %d -numSourceLimit %d -log %d", CACHEDIR, step, numSourcesPerProcess, nextProcessIndex);
				if (errorCode = createLPProcess(nextProcessIndex, processHandle, processId, L"releasex64\\CorpusAnalysis.exe", processParameters) < 0)
					break;
				break;
			default: break;
			}
			handles[nextProcessIndex] = processHandle;
			if (numProcesses < maxProcesses)
				numProcesses++;
			printf("\nCreated process %d:%d", nextProcessIndex, (int)processId);
		}
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
		if (capitalized || (ret = Words.attemptDisInclination(mysql, iWord, word.word->first, sourceId,false)))
		{
			if ((ret = Words.splitWord(mysql, iWord, word.word->first, sourceId,false)) && word.word->first.find(L'-')!=wstring::npos,false)
			{
				wstring sWord= word.word->first;
				sWord.erase(std::remove(sWord.begin(), sWord.end(), L'-') , sWord.end());
				if ((iWord=Words.query(sWord))==Words.end())
					ret = Words.attemptDisInclination(mysql, iWord, sWord, sourceId,false);
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
		lplog(LOG_INFO, L"DB statement [%s add inflection flag]:%s", word.c_str(), inflectionFlagsToStr(inflections, sFlags));
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

int analyzeEnd(Source source, int sourceId, wstring path, wstring etext, wstring title,bool &reprocess,bool &nosource,wstring specialExtension)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) 
		return -1;
	Words.readWords(path, sourceId, false,L"");
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, true,specialExtension))
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

int printUnknownsFromSource(Source source, int sourceId, wstring path, wstring etext, wstring specialExtension)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, L"");
	bool parsedOnly = false, firstIllegal = false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, true,specialExtension))
	{
		int wordIndex = 0;
		for (WordMatch &im : source.m)
		{
			if (im.word->second.isUnknown())
			{
				lplog(LOG_INFO, L"SI%d: WI%d: %s", sourceId, wordIndex, im.word->first.c_str());
			}
			else if (im.word->second.query(UNDEFINED_FORM_NUM) > 0)
			{
				if (!firstIllegal)
				{
					lplog(LOG_INFO, L"REPARSE SI%d", sourceId);
					firstIllegal = true;
				}
				lplog(LOG_INFO, L"SI%d: WI%d: %s [ILLEGAL]", sourceId, wordIndex, im.word->first.c_str());
			}
			wordIndex++;
		}
	}
	else
	{
		wprintf(L"Unable to read source %d:%s\n", sourceId, path.c_str());
		return 20;
	}
	return 21;
}

int patternOrWordAnalysisFromSource(Source &source, int sourceId, wstring path, wstring etext, wstring primaryPatternOrWordName, wstring primaryDifferentiator, wstring secondaryPatternOrWordName, wstring secondaryDifferentiator, bool isPrimaryPattern, bool isSecondaryPattern, wstring specialExtension)
{	LFS
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, L"");
	bool parsedOnly = false;
	int lastSentenceIndexPrinted = -1;
	if (source.readSource(path, false, parsedOnly, false, true,specialExtension))
	{
		int wordIndex = 0;
		unsigned int ss = 1;
		for (WordMatch &im : source.m)
		{
			int primaryPMAOffset;
			if (isPrimaryPattern && (primaryPMAOffset = (primaryDifferentiator == L"*") ? im.pma.queryPattern(primaryPatternOrWordName) : im.pma.queryPatternDiff(primaryPatternOrWordName, primaryDifferentiator))!=-1)
			{
				int secondaryPMAOffset;
				if (secondaryPatternOrWordName.empty() ||
					(isSecondaryPattern && (secondaryPMAOffset = (secondaryDifferentiator == L"*") ? im.pma.queryPattern(secondaryPatternOrWordName) : im.pma.queryPatternDiff(secondaryPatternOrWordName, secondaryDifferentiator)) != -1) ||
					(!isSecondaryPattern && im.word->first == secondaryPatternOrWordName) ||
					(!isSecondaryPattern && secondaryPatternOrWordName == L"" && (im.flags&WordMatch::flagNotMatched) != 0))
				{
					primaryPMAOffset = primaryPMAOffset & ~matchElement::patternFlag;
					secondaryPMAOffset = secondaryPMAOffset & ~matchElement::patternFlag;
					/*additional logic begin*/
					//if (im.pma[primaryPMAOffset].len != 1 || im.pma.queryPattern(L"__NOUN") != -1)
					//{
					//	wordIndex++;
					//	while (ss < source.sentenceStarts.size() && source.sentenceStarts[ss] < wordIndex + 1)
					//		ss++;
					//	continue;
					//}
					/*additional logic end*/
					int patternEnd = wordIndex + im.pma[primaryPMAOffset].len;
					wstring sentence;
					getSentenceWithTags(source, wordIndex, patternEnd, source.sentenceStarts[ss - 1], source.sentenceStarts[ss], im.pma[primaryPMAOffset].pemaByPatternEnd, sentence);
					wstring adiff = patterns[im.pma[primaryPMAOffset].getPattern()]->differentiator;
					wstring path = source.sourcePath.substr(16, source.sourcePath.length() - 20);
					if (primaryDifferentiator.find(L'*') == wstring::npos)
					{
						lplog(LOG_ERROR, L"%s", sentence.c_str());
						lplog(LOG_INFO, L"%s[%d-%d]:%s", path.c_str(), wordIndex, patternEnd, sentence.c_str());
					}
					else
					{
						lplog(LOG_ERROR, L"[%s]:%s", adiff.c_str(), sentence.c_str());
						lplog(LOG_INFO, L"%s[%s](%d-%d):%s", path.c_str(), adiff.c_str(), wordIndex, patternEnd, sentence.c_str());
					}
				}
			}
			else if (!isPrimaryPattern && im.word->first==primaryPatternOrWordName)
			{
				int secondaryPMAOffset;
				if (secondaryPatternOrWordName.empty() ||
					(isSecondaryPattern && (secondaryPMAOffset = (secondaryDifferentiator == L"*") ? im.pma.queryPattern(secondaryPatternOrWordName) : im.pma.queryPatternDiff(secondaryPatternOrWordName, secondaryDifferentiator)) != -1) ||
					(!isSecondaryPattern && im.word->first == secondaryPatternOrWordName) ||
					(!isSecondaryPattern && secondaryPatternOrWordName == L"" && (im.flags&WordMatch::flagNotMatched) != 0))
				{
					wstring sentence, originalIWord;
					if (lastSentenceIndexPrinted != source.sentenceStarts[ss - 1])
					{
						for (int I = source.sentenceStarts[ss - 1]; I < source.sentenceStarts[ss]; I++)
						{
							source.getOriginalWord(I, originalIWord, false, false);
							if (I == wordIndex)
								originalIWord = L"*" + originalIWord + L"*";
							sentence += originalIWord + L" ";
						}
						lplog(LOG_ERROR, L"%s", sentence.c_str());
						lastSentenceIndexPrinted = source.sentenceStarts[ss - 1];
					}
				}
			}
			else if (!isPrimaryPattern && primaryPatternOrWordName == L"" && (im.flags&WordMatch::flagNotMatched) != 0)
			{
				int secondaryPMAOffset;
				if (secondaryPatternOrWordName.empty() ||
					(isSecondaryPattern && (secondaryPMAOffset = (secondaryDifferentiator == L"*") ? im.pma.queryPattern(secondaryPatternOrWordName) : im.pma.queryPatternDiff(secondaryPatternOrWordName, secondaryDifferentiator)) != -1) ||
					(!isSecondaryPattern && im.word->first == secondaryPatternOrWordName) ||
					(!isSecondaryPattern && secondaryPatternOrWordName == L"" && (im.flags&WordMatch::flagNotMatched) != 0))
				{
					wstring sentence, originalIWord;
					if (lastSentenceIndexPrinted != source.sentenceStarts[ss - 1])
					{
						bool inNoMatch = false;
						for (int I = source.sentenceStarts[ss - 1]; I < source.sentenceStarts[ss]; I++)
						{
							source.getOriginalWord(I, originalIWord, false, false);
							if (I == wordIndex)
							{
								originalIWord = L"*" + originalIWord;
								inNoMatch = true;
							}
							if (inNoMatch && (I + 1 >= source.sentenceStarts[ss] || (source.m[I + 1].flags&WordMatch::flagNotMatched) == 0))
							{
								originalIWord += L"*";
								inNoMatch = false;
							}
							sentence += originalIWord + L" ";
						}
						lplog(LOG_ERROR, L"%s", sentence.c_str());
						lastSentenceIndexPrinted = source.sentenceStarts[ss - 1];
					}
				}
			}
			wordIndex++;
			while (ss < source.sentenceStarts.size() && source.sentenceStarts[ss] < wordIndex+1)
				ss++;
		}
	}
	else
	{
		wprintf(L"Unable to read source %d:%s\n", sourceId, path.c_str());
		return 21;
	}
	return 22;
}

int syntaxCheckFromSource(Source source, int sourceId, wstring path, wstring etext, wstring specialExtension)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, L"");
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, true,specialExtension))
	{
		int wordIndex = 0;
		unsigned int ss = 1;
		for (WordMatch &im : source.m)
		{
			// verb, adverb, adverb, OBJECT_1 that is composed of a single noun that does not accept adjectives
			int pemaOffset= source.queryPatternDiff(wordIndex,L"__S1", L"1"),pmaOffset;
			if (pemaOffset != -1 && im.hasWinnerVerbForm() && im.getNumWinners() == 1 && wordIndex < source.m.size() - 3 &&
				(pmaOffset = source.m[wordIndex + 1].pma.queryPatternDiff(L"__ALLOBJECTS_1", L"1")) != -1)
			{
				pmaOffset &= ~matchElement::patternFlag;
				bool twoAdverbs =
					source.m[wordIndex + 1].pma[pmaOffset].len == 3 &&
					source.m[wordIndex + 1].isOnlyWinner(adverbForm) &&
					source.m[wordIndex + 2].isOnlyWinner(adverbForm) && source.m[wordIndex + 2].queryForm(prepositionForm)!=-1 &&
					(source.m[wordIndex + 3].isOnlyWinner(accForm) || source.m[wordIndex + 3].isOnlyWinner(personalPronounForm)) &&
					source.m[wordIndex + 3].word->first!=L"he" && source.m[wordIndex + 3].word->first != L"she";
				bool oneAdverb =
					source.m[wordIndex + 1].pma[pmaOffset].len == 2 &&
					source.m[wordIndex + 1].isOnlyWinner(adverbForm) && source.m[wordIndex + 1].queryForm(prepositionForm) != -1 &&
					(source.m[wordIndex + 2].isOnlyWinner(accForm) || source.m[wordIndex + 2].isOnlyWinner(personalPronounForm)) &&
					source.m[wordIndex + 2].word->first != L"he" && source.m[wordIndex + 2].word->first != L"she";
				if (oneAdverb || twoAdverbs)
				{
					int patternEnd = wordIndex + 1 + source.m[wordIndex + 1].pma[pmaOffset].len;
					wstring sentence, originalIWord;
					bool inPattern = false;
					for (int I = source.sentenceStarts[ss - 1]; I < source.sentenceStarts[ss]; I++)
					{
						source.getOriginalWord(I, originalIWord, false, false);
						if (I == wordIndex + 1)
						{
							sentence += L"**";
							inPattern = true;
						}
						sentence += originalIWord;
						if (I == patternEnd - 1)
						{
							sentence += L"**";
							inPattern = false;
						}
						sentence += L" ";
					}
					wstring path = source.sourcePath.substr(16, source.sourcePath.length() - 20);
					lplog(LOG_INFO, L"%s[%d-%d]:%s", path.c_str(), wordIndex, patternEnd, sentence.c_str());
					lplog(LOG_ERROR, L"%s", sentence.c_str());
				}
			}
			wordIndex++;
			while (ss < source.sentenceStarts.size() && source.sentenceStarts[ss] < wordIndex + 1)
				ss++;
		}
	}
	else
	{
		wprintf(L"Unable to read source %d:%s\n", sourceId, path.c_str());
		return 61;
	}
	return 62;
}

int populateWordFrequencyTableFromSource(Source source, int sourceId, wstring path, wstring etext, wstring specialExtension)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, L"");
	bool parsedOnly = false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, true,specialExtension))
	{
		unordered_map<wstring, wordInfo> wf;
		int numUnknown = 0;
		wf.reserve(source.m.size());
		for (WordMatch &im : source.m)
		{
			wstring word = im.word->first;
			if (word.empty() || word[word.length() - 1] == L'\\' || word.length() > 32)
				continue;
			auto wfi = wf.find(word);
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
				if (im.word->second.query(UNDEFINED_FORM_NUM) >= 0) // im.word->second.isUnknown())
				{
					numUnknown++;
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
			if (im.word->second.query(UNDEFINED_FORM_NUM) > 0) // im.word->second.isUnknown())
			{
				wfi->second.unknownFrequency++;
				if (im.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (im.flags&WordMatch::flagFirstLetterCapitalized))
					wfi->second.unknownCapitalizedFrequency++;
				if (im.flags&WordMatch::flagAllCaps)
					wfi->second.unknownAllCapsFrequency++;
			}
		}
		if (numIllegalWords == 0)
			writeSourceWordFrequency(&source.mysql, wf, etext);
		if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE")) return -1;
		wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
		_snwprintf(qt, QUERY_BUFFER_LEN, L"UPDATE sources SET numUnknown=%d,numIllegalWords=%d where id=%d",numUnknown, numIllegalWords,sourceId);
		myquery(&source.mysql, qt);
		source.unlockTables();
	}
	else
	{
		wprintf(L"Unable to read source %d:%s\n", sourceId, path.c_str());
		return 0;
	}
	return (numIllegalWords == 0) ? 2 : -(numIllegalWords + 10);
}

int populateWordFrequencyTableMP(Source source, wstring specialExtension)
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
		int setStep = populateWordFrequencyTableFromSource(source, sourceId, path, etext,specialExtension);
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

int populateWordFrequencyTable(Source source, int step, wstring specialExtension)
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
		int setStep = populateWordFrequencyTableFromSource(source, sourceId, path, etext,specialExtension);
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

int printUnknowns(Source source, int step, wstring specialExtension)
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
		int setStep = printUnknownsFromSource(source, sourceId, path, etext,specialExtension);
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
  
int patternOrWordAnalysis(Source source, int step, wstring primaryPatternOrWordName, wstring primaryDifferentiator, wstring secondaryPatternOrWordName, wstring secondaryDifferentiator, enum Source::sourceTypeEnum st, bool isPrimaryPattern, bool isSecondaryPattern, wstring specialExtension)
{	LFS
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE")) return -1;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	bool websterAPIRequestsExhausted = false;
	int startTime = clock(), numSourcesProcessedNow = 0;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	lplog(LOG_INFO | LOG_ERROR | LOG_NOTMATCHED, NULL); // close all log files to change extension
	logFileExtension = specialExtension+ L"."+primaryPatternOrWordName;
	if (!primaryDifferentiator.empty() && primaryDifferentiator!=L"*")
		logFileExtension += L"[" + primaryDifferentiator + L"]";
	if (!secondaryPatternOrWordName.empty())
	{
		logFileExtension += L"." + secondaryPatternOrWordName;
		if (!secondaryDifferentiator.empty() && secondaryDifferentiator != L"*")
			logFileExtension += L"[" + secondaryDifferentiator + L"]";
	}
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
		if (st== Source::GUTENBERG_SOURCE_TYPE)
			path.insert(0, L"\\").insert(0, CACHEDIR);
		else if (st == Source::TEST_SOURCE_TYPE)
			path.insert(0, L"\\").insert(0, LMAINDIR);
		int setStep = patternOrWordAnalysisFromSource(source, sourceId, path, etext, primaryPatternOrWordName, primaryDifferentiator, secondaryPatternOrWordName, secondaryDifferentiator, isPrimaryPattern, isSecondaryPattern, specialExtension);
		if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE")) return -1;
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

int syntaxCheck(Source source, int step, wstring specialExtension)
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
		int setStep = syntaxCheckFromSource(source, sourceId, path, etext,specialExtension);
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

void testRDFType(Source &source, wstring specialExtension)
{
	int sourceId = 25291;
	wstring path = L"J:\\caches\\texts\\Schoonover, Frank E\\Jules of the Great Heart  Free Trapper and Outlaw in the Hudson Bay Region in the Early Days.txt";
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) return;
	Words.readWords(path, sourceId, false, L"");
	vector <cTreeCat *> rdfTypes;
	Ontology::rdfIdentify(L"clackamas", rdfTypes, L"Z", true);
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, true,specialExtension))
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

// L"thank",L"no",L"never",L"then",L"so",L"number",L"as",L"only", L"van",L"von",L"also",L"not",L"more",L"eg",L"e.g.",L"p.o.",L"like",L" - only used in very specialized patterns
// 	L"p",L"m",L"le",L"de",L"f",L"c",L"k",L"o",L"b",L"!",L"?",
// expand ST choice of POS by these forms
unordered_map<wstring, vector <wstring> > maxentAssociationMap =
{
	// amplification
	//{L"sectionheader", L"noun"},

	// similarity 
	//{ L"coordinator",{ L"conjunction"} },
	{ L"conjunction",{ L"coordinator" } },
	{ L"Proper Noun",{ L"honorific_abbreviation",L"honorific",L"roman_numeral",L"month",L"interjection",L"daysOfWeek",L"no",L"holiday" } },
	{ L"honorific noun",{ L"honorific",L"honorific_abbreviation" }},
	{ L"modal_auxiliary",{ L"future_modal_auxiliary",L"negation_modal_auxiliary",L"negation_future_modal_auxiliary"} },
	{ L"determiner",{ L"demonstrative_determiner",L"no",L"quantifier",L"predeterminer"} },
	{ L"predeterminer",{ L"quantifier" } },
	{ L"interjection",{ L"no",L"politeness_discourse_marker"} },
	{ L"relativizer", { L"what",L"startquestion" } },
	{ L"particle", { L"adverb",L"preposition",L"quantifier" } }, // LP usually treats particles as adverbs, not sure whether this is strictly correct, but it makes sense to me.
	// include possible subclasses
	{ L"verb", { L"verbverb",L"SYNTAX:Accepts S as Object",L"have",L"have_negation",L"is",L"is_negation",L"does",L"does_negation",L"be",L"been",L"modal_auxiliary",L"negation_modal_auxiliary",L"future_modal_auxiliary",L"negation_future_modal_auxiliary",L"being"} }, // feel, see, watch, hear, tell etc // fancy, say (thinksay verbs)
	// stanford maxent apparently has no indefinite pronoun, so it classes them all as nouns.
	{ L"noun",{ L"uncertainDurationUnit",L"simultaneousUnit",L"dayUnit",L"timeUnit",L"quantifier",L"numeral_cardinal",L"indefinite_pronoun",L"season",L"time_abbreviation" } }, // all, some etc // something, everything
	{ L"adjective",{ L"quantifier",L"numeral_ordinal",L"numeral_cardinal" }},  // many / more
	{ L"adverb",{ L"not",L"never",L"there" }},  // many
	{ L"to",{ L"preposition" }},
	{ L"there",{ L"pronoun",L"adverb" }},
	{ L"no",{ L"adverb" }},
	{ L"which",{ L"interrogative_determiner",L"interrogative_pronoun",L"relativizer"}},
	{ L"what",{ L"interrogative_determiner",L"interrogative_pronoun",L"relativizer"}},
	{ L"who",{ L"interrogative_pronoun",L"relativizer"}},
	{ L"whose",{ L"interrogative_determiner",L"relativizer"}},
	{ L"how",{ L"relativizer",L"conjunction",L"adverb"}}
};

// checks if the part of speech indicated in parse from the Stanford Maxent POS tagger matches the winner forms at wordSourceIndex.
// returns yes=0, no=1
int checkStanfordMaxentAgainstWinner(Source &source, int wordSourceIndex, wstring originalParse, wstring &parse, int &numPOSNotFound, unordered_map<wstring, int> &formNoMatchMap, unordered_map<wstring, int> &wordNoMatchMap, bool inRelativeClause)
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
		lplog(LOG_ERROR, L"%d:Part of Speech %s not found.", wordSourceIndex,partofspeech.c_str());
		numPOSNotFound++;
		return 1;
	}
	std::set<wstring> posList(lpPOS->second.begin(), lpPOS->second.end());
	for (auto pos : lpPOS->second)
	{
		auto imai = maxentAssociationMap.find(pos);
		if (imai != maxentAssociationMap.end())
		{
			posList.insert(imai->second.begin(), imai->second.end());
		}
	}
	vector <int> winnerForms;
	source.m[wordSourceIndex].getWinnerForms(winnerForms);
	for (int wf : winnerForms)
	{
		if (posList.find(Forms[wf]->name) != posList.end())
			return 0;
	}
	//////////////////////////////
	// corrections based on implementation/interpretation differences and statistical findings
	// with maxent, if LP thinks it is a ProperNoun, it is always correct, compared with maxent which thinks it is a noun.
	if (posList.find(L"noun")!=posList.end() && std::find(winnerForms.begin(), winnerForms.end(), PROPER_NOUN_FORM_NUM) != winnerForms.end())
		return 0;
	// Maxent sometimes thinks things are proper nouns, when they are not capitalized.  I have not found an example where this is the case.
	if (posList.find(L"Proper Noun") != posList.end() &&
		//(std::find(winnerForms.begin(), winnerForms.end(), nounForm) != winnerForms.end() || std::find(winnerForms.begin(), winnerForms.end(), adjectiveForm) != winnerForms.end()) && 
		iswlower(originalWordSave[0]))
		return 0;
	// Maxent sometimes thinks things are proper nouns, when they are actually just other forms, even if they are capitalized, usually when they are the first word.
	// I have not found an example where this is the case.
	vector <wstring> pnExceptionForms =
	{ L"pronoun", L"pronoun possessive_pronoun", L"conjunction", L"determiner",
		 L"does_negation", L"have_negation", L"indefinite_pronoun", L"is",
		 L"is_negation", L"le", L"modal_auxiliary", L"negation_future_modal_auxiliary",
		 L"negation_modal_auxiliary", L"numeral_ordinal", L"personal_pronoun_nominative",
		 L"polite_inserts", L"sectionheader", L"street_address", L"SYNTAX:Accepts S as Object",
		 L"trademark" };
	if (posList.find(L"Proper Noun") != posList.end())
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
	if (posList.find(L"adjective") != posList.end() && std::find(winnerForms.begin(), winnerForms.end(), PROPER_NOUN_FORM_NUM) != winnerForms.end())
		return 0;
	// these cases have been proven by examination - Stanford guesses this to be a noun, but if it is capitalized, it is an honorific/honorific_abreviation
	if (posList.find(L"noun") != posList.end() && iswupper(originalWordSave[0]) && (std::find(winnerForms.begin(), winnerForms.end(), honorificForm) != winnerForms.end() || std::find(winnerForms.begin(), winnerForms.end(), honorificAbbreviationForm) != winnerForms.end()))
		return 0;
	// LP is always right about these negative forms (by examination)
	if (posList.find(L"noun") != posList.end() && (std::find(winnerForms.begin(), winnerForms.end(), doesNegationForm) != winnerForms.end() ||
		std::find(winnerForms.begin(), winnerForms.end(), negationModalAuxiliaryForm) != winnerForms.end() ||
		std::find(winnerForms.begin(), winnerForms.end(), isNegationForm) != winnerForms.end() ||
		std::find(winnerForms.begin(), winnerForms.end(), haveNegationForm) != winnerForms.end()))
		return 0;
	/////////////////////////////
	wstring posListStr;
	for (auto pos : posList)
		posListStr += pos + L" ";
	wstring winnerFormsString;
	source.m[wordSourceIndex].winnerFormString(winnerFormsString, false);
	formNoMatchMap[posListStr + L"!= " + winnerFormsString]++;
	wordNoMatchMap[source.m[wordSourceIndex].word->first]++;
	lplog(LOG_ERROR, L"%d:Stanford POS %s (%s) not found in winnerForms %s for word %s [%s].", wordSourceIndex, partofspeech.c_str(), posListStr.c_str(), winnerFormsString.c_str(), originalWordSave.c_str(), originalParse.c_str());
	return 1;
}

void formMatrixTest(Source &source, int wordSourceIndex, wstring X, wstring Y, int costX, int costY, unordered_map<wstring,int> &comboCostFrequency, wstring &partofspeech)
{
	if (source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(X)) != costX ||
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(Y)) != costY)
		return;
	wstring XCost, YCost;
	itos(costX, XCost);
	itos(costY, YCost);
	wstring tmp1,tmp2,combo = X + L"*" + itos(costX, tmp1) + L" " + Y + L"*"+ itos(costY, tmp2);
	wstring word = source.m[wordSourceIndex].word->first;
	if (Y == L"verb" || X == L"verb")
	{
		if (word.length() > 2 && word.substr(word.length() - 2) == L"ed" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PAST) == VERB_PAST)
		{
			combo += L" PAST";
		}
		if (word.length() > 3 && word.substr(word.length() - 3) == L"ing" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE)
		{
			combo += L" PARTICIPLE";
		}
		if ((source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_THIRD_SINGULAR) == VERB_PRESENT_THIRD_SINGULAR)
		{
			combo += L" 3rdSING";
		}
		if ((source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR)
		{
			combo += L" 1stSING";
		}
	}
	comboCostFrequency[combo]++;
	partofspeech += L"***|"+combo+L"|***";
}

// perform tests to make sure that the noun according to LP is not a verb (that ST says).
// that smells / those smell - these agree by verb
bool isStanfordDeterminerType(Source &source, int wordNounVerbDisagreementSourceIndex, int wordDeterminerSourceIndex)
{
	// test for agreement - the two must agree (that smells / those smell) and not potentially disagree (ambiguousness)
	if (source.m[wordDeterminerSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0)
	{
		// evaluate as though Stanford was correct - does the determiner (which would then be the subject) and the verb agree (and was it testable)?
		if (source.m[wordNounVerbDisagreementSourceIndex].word->second.hasVerbForm())
		{
			bool agreementTestable;
			bool agree=source.evaluateSubjectVerbAgreement(wordNounVerbDisagreementSourceIndex, wordDeterminerSourceIndex, agreementTestable);
			// if not testable, or the purported verb and determiner/subject agree, reject.
			if (!agreementTestable || agree)
				return false;
		}
		// evaluate as through LP was correct - does the determiner and the noun agree?
		bool determinerPlural = source.m[wordDeterminerSourceIndex].word->second.inflectionFlags& PLURAL;
		bool determinerSingular = source.m[wordDeterminerSourceIndex].word->second.inflectionFlags&SINGULAR;
		bool nounPlural = source.m[wordNounVerbDisagreementSourceIndex].word->second.inflectionFlags& PLURAL;
		bool nounSingular = source.m[wordNounVerbDisagreementSourceIndex].word->second.inflectionFlags&SINGULAR;
		// be maximally certain - if either are plural and sungular or neither, then this test cannot be relied upon
		if ((determinerPlural && determinerSingular) || (nounPlural && nounSingular) || 
			(!determinerPlural && !determinerSingular) || (!nounPlural && !nounSingular))
			return false;
		// if they disagree, LP is probably not correct - reject.
		if ((determinerPlural && nounSingular) || (determinerSingular && nounPlural))
			return false;
		// allow further testing
	}

	return source.m[wordDeterminerSourceIndex].queryWinnerForm(determinerForm) >= 0 ||
		source.m[wordDeterminerSourceIndex].queryWinnerForm(possessiveDeterminerForm) >= 0 ||  // my / your / their AGREEMENT test possible but this determiner type cannot be a subject.
		(source.m[wordDeterminerSourceIndex].queryWinnerForm(interrogativeDeterminerForm) >= 0 && source.m[wordDeterminerSourceIndex].word->first!=L"which") || // which may be followed by a verb
		source.m[wordDeterminerSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0; // this / that / these / those AGREEMENT required
}

class FormDistribution
{
public:
	int agreeSTLP=0; // count of times word-POS agreed between ST and LP
	int disagreeSTLP = 0; // count of times word-POS disgreed between ST and LP
	int unaccountedForDisagreeSTLP = 0; // count of times word-POS disgreed between ST and LP
	map <wstring, int> STFormDistribution; // total count for each form match in ST
	map <wstring, int> LPFormDistribution; // total count for each form match in LP
	unordered_map <wstring, int> LPErrorFormDistribution; // count for each form error match in LP if none agree
	unordered_map <wstring, int> agreeFormDistribution; // total count for each form match agreed between ST and LP
	unordered_map <wstring, int> disagreeFormDistribution; // total count for each form match disagreed between ST and LP
	unordered_map <wstring, int> LPAlreadyAccountedFormDistribution; // total count for each form match already accounted for (already entered in the errorMap)
};
map <wstring, FormDistribution> formDistribution;

wstring stTokenizeWord(wstring tokenizedWord,wstring &originalWord, unsigned long long flags,wstring parse,int &wspace)
{
	// pcfg output:
	// parse=(ROOT (PRN (: ;) (S (NP (NP (NP (QP (CC and) (CD Bunny))) (, ,) (CC and) (NP (NNP Bobtail)) (, ,)) (CC and) (NP (NNP Billy))) (VP (VBD were) (ADVP (RB always)) (VP (VBG doing) (NP (JJ *) (NN something)))))))
	// ben's
	if (originalWord[originalWord.length() - 2] == L'\'' && towlower(originalWord[originalWord.length() - 1]) == L's')
		originalWord.erase(originalWord.length() - 2);
	// don't
	if (towlower(originalWord[originalWord.length() - 3]) == L'n' && originalWord[originalWord.length() - 2] == L'\'' && towlower(originalWord[originalWord.length() - 1]) == L't')
		originalWord.erase(originalWord.length() - 3);
	// cannot, dunno
	if (tokenizedWord == L"cannot" || tokenizedWord == L"dunno")
		originalWord.erase(originalWord.length() - 3);
	// gimme, lemme
	if (tokenizedWord == L"gimme" || tokenizedWord == L"lemme")
		originalWord.erase(originalWord.length() - 2);
	// y'are
	if (tokenizedWord == L"y'are")
		originalWord.erase(originalWord.length() - 3);
	else if (tokenizedWord == L"y'r")
		originalWord.erase(originalWord.length() - 1);
	else if (tokenizedWord == L"ma’am")
		originalWord[2] = '\'';
	// o'brien, o'clock, b'ar o'sheen
	else if (tokenizedWord[0] == L'o' && tokenizedWord[1] == L'’')
		originalWord[1] = L'\'';
	else
	{
		//wer'n, better'n etc but not o'clock and ma'am, which are found by ST
		size_t findQuote = (originalWord.find(L'\''));
		if (findQuote != wstring::npos && parse.find(originalWord) == wstring::npos)
		{
			originalWord = originalWord.substr(0, findQuote);
		}
	}
	wstring lookFor;
	// all words in LP which have spaces in them are interpreted by ST separately
	wspace = originalWord.find(L' ');
	if (wspace != wstring::npos)
	{
		if (tokenizedWord == L"no one" || tokenizedWord == L"every one")
			lookFor = L" one)";
		else
			if (tokenizedWord == L"as if")
				lookFor = L" if)";
			else
				if (tokenizedWord == L"for ever")
					lookFor = L" ever)";
				else
					if (tokenizedWord == L"next to")
						lookFor = L" to)";
					else
						lookFor = L" " + originalWord.substr(0, wspace) + L")";
		if (lookFor.length() > 0 && (flags&WordMatch::flagAllCaps))
			for (int len = 0; lookFor[len]; len++) lookFor[len] = towupper(lookFor[len]);
	}
	else
		lookFor = L" " + originalWord + L")";
	return lookFor;
}

// see if LP class can be corrected.
// return code:
//   -1: LP class corrected.  ST prefers something other than correct class, so LP is correct
//   -2: LP class corrected.  ST prefers correct class, so this entry should simply be removed from the output file.
//   -3: test whether to change to correct class - set to disagree, and add an arbitrary string to partofspeech to search for whatever string added as a test.
//    0: unable to determine whether class should be corrected, or the class has been corrected. Normal processing should continue.  
int ruleCorrectLPClass(wstring primarySTLPMatch, Source &source, int wordSourceIndex, unordered_map<wstring, int> &errorMap, wstring &partofspeech, int startOfSentence, map<wstring,FormDistribution>::iterator fdi)
{
	int adverbFormOffset = source.m[wordSourceIndex].queryForm(adverbForm);
	int adjectiveFormOffset = source.m[wordSourceIndex].queryForm(adjectiveForm);
	int particleFormOffset = source.m[wordSourceIndex].queryForm(particleForm);
	int nounPlusOneFormOffset = source.m[wordSourceIndex + 1].word->second.query(nounForm);
	// RULE CHANGE - change an adjective to an adverb?
	if (source.m[wordSourceIndex].word->first != L"that" && // 'that' is very ambiguous
		source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) && 
		source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(L"Proper Noun") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(L"indefinite_pronoun") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(L"numeral_cardinal") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") < 0 && // only an adjective, not before a noun
		(nounPlusOneFormOffset <0 || source.m[wordSourceIndex+1].word->second.getUsageCost(nounPlusOneFormOffset)==4) &&
		(adverbFormOffset=source.m[wordSourceIndex].queryForm(L"adverb")) >= 0 && source.m[wordSourceIndex].word->second.getUsageCost(adverbFormOffset) < 2 &&
		source.m[wordSourceIndex].queryForm(L"interjection") < 0 && // interjection acts similarly to adverb
		(iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || wordSourceIndex == 0 || iswalpha(source.m[wordSourceIndex - 1].word->first[0])) && // not alone in the sentence
		(wordSourceIndex <= 0 || (source.m[wordSourceIndex - 1].queryForm(L"is") < 0 && source.m[wordSourceIndex - 1].word->first != L"be" && source.m[wordSourceIndex - 1].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 1 || (source.m[wordSourceIndex - 2].queryForm(L"is") < 0 && source.m[wordSourceIndex - 2].word->first != L"be" && source.m[wordSourceIndex - 2].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 2 || (source.m[wordSourceIndex - 3].queryForm(L"is") < 0 && source.m[wordSourceIndex - 3].word->first != L"be" && source.m[wordSourceIndex - 3].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 3 || (source.m[wordSourceIndex - 4].queryForm(L"is") < 0 && source.m[wordSourceIndex - 4].word->first != L"be" && source.m[wordSourceIndex - 4].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex < source.m.size()-1 || (source.m[wordSourceIndex + 1].queryForm(L"is") < 0 && source.m[wordSourceIndex + 1].word->first != L"be")) && // is/ishas before means it really is an adjective!
		(source.m[wordSourceIndex].queryForm(L"preposition") < 0) && (source.m[wordSourceIndex].queryForm(L"relativizer") < 0)) // || source.m[wordSourceIndex + 1].queryWinnerForm(L"numeral_cardinal") < 0)) // before one o'clock
	{
		bool isDeterminer = false;
		if (wordSourceIndex > 0)
		{
			vector<wstring> determinerTypes = { L"determiner",L"demonstrative_determiner",L"possessive_determiner",L"interrogative_determiner", L"quantifier", L"numeral_cardinal" };
			for (wstring dt : determinerTypes)
				if (isDeterminer = source.m[wordSourceIndex - 1].queryWinnerForm(dt) >= 0)
					break;
		}
		// The door that faced her stood *open*
		if (!isDeterminer && primarySTLPMatch != L"Proper Noun" && source.m[wordSourceIndex - 1].queryWinnerForm(L"verb") >= 0) // Proper Noun is already well controlled
		{
			source.m[wordSourceIndex].setWinner(source.m[wordSourceIndex].queryForm(adverbForm));
			source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(adjectiveForm));
			if (primarySTLPMatch == L"adverb")
				return -2;
			errorMap[L"LP correct: adverb rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[L"adverb"]++;
			return -1;
		}
	}
	if (source.m[wordSourceIndex].isOnlyWinner(prepositionForm) && source.m[wordSourceIndex].getRelObject() < 0 && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))
	{
		int relVerb = source.m[wordSourceIndex].getRelVerb();
		bool sentenceOfBeing =				// 4 words or less before the word must be an 'is' verb
			((wordSourceIndex <= 0 || (source.m[wordSourceIndex - 1].queryForm(L"is") >= 0 || source.m[wordSourceIndex - 1].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 1 || (source.m[wordSourceIndex - 2].queryForm(L"is") >= 0 || source.m[wordSourceIndex - 2].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 2 || (source.m[wordSourceIndex - 3].queryForm(L"is") >= 0 || source.m[wordSourceIndex - 3].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 3 || (source.m[wordSourceIndex - 4].queryForm(L"is") >= 0 || source.m[wordSourceIndex - 4].queryForm(L"be") >= 0))); // is/ishas before means it really is an adjective!
		if (adverbFormOffset < 0)
		{
			if (!(WordClass::isSingleQuote(source.m[wordSourceIndex + 1].word->first[0]) || WordClass::isDoubleQuote(source.m[wordSourceIndex + 1].word->first[0])) && primarySTLPMatch == L"to")
			{
				errorMap[L"LP correct: 'to' preposition rule"]++;
				fdi->second.LPAlreadyAccountedFormDistribution[L"preposition"]++;
				return -1;
			}
			if (source.m[wordSourceIndex].word->first == L"like" && sentenceOfBeing &&
					// the word before must NOT be a dash
					(wordSourceIndex <= 0 || !WordClass::isDash((source.m[wordSourceIndex - 1].word->first[0]))))
			{
				source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
				source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
				if (primarySTLPMatch == L"adjective")
					return -2;
				errorMap[L"LP correct: adjective-like rule"]++;
				fdi->second.LPAlreadyAccountedFormDistribution[L"adjective"]++;
				return -1;
			}
			else
				return 0; // In other cases the STLPMatch is already a preposition, so ST and LP agree anyway
		}
		else if (primarySTLPMatch == L"adverb")
		{
			source.m[wordSourceIndex].setWinner(adverbFormOffset);
			source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
			if (primarySTLPMatch == L"adverb")
				return -2;
			errorMap[L"LP correct: adverb rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[L"adverb"]++;
			return -1;
		}
		else
		{
			wstring nextWord = source.m[wordSourceIndex + 1].word->first;
			if (nextWord == L"." || nextWord == L"," || nextWord == L";" || nextWord == L"--")
			{
				if (primarySTLPMatch == L"particle" && particleFormOffset>=0)
				{
					source.m[wordSourceIndex].setWinner(particleFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					return -2;
				}
				if (relVerb>=0 && (source.m[relVerb].queryForm(L"is")>=0 || source.m[relVerb].queryForm(L"be") >= 0))
				{
					if (adjectiveFormOffset < 0)
					{
						if (particleFormOffset > 0 && !WordClass::isDash((source.m[wordSourceIndex + 1].word->first[0])))
						{
							source.m[wordSourceIndex].setWinner(particleFormOffset);
							source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
							if (primarySTLPMatch == L"particle")
								return -2;
							errorMap[L"LP correct: particle rule"]++;
							fdi->second.LPAlreadyAccountedFormDistribution[L"particle"]++;
							return -1;
						}
						if (adverbFormOffset >= 0)
						{
							source.m[wordSourceIndex].setWinner(adverbFormOffset);
							source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
							errorMap[L"LP correct: adverb rule"]++;
							fdi->second.LPAlreadyAccountedFormDistribution[L"adverb"]++;
							return -1;
						}
						return 0;
					}
					//if (relVerb >= 0)
					//	partofspeech += source.m[relVerb].word->first;
					source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					if (primarySTLPMatch == L"adjective")
						return -2;
					errorMap[L"LP correct: adjective-prep rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[L"adjective"]++;
					return -1;
				}
				else if (primarySTLPMatch==L"preposition or conjunction")
				{
					source.m[wordSourceIndex].setWinner(adverbFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					errorMap[L"LP correct: adverb rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[L"adverb"]++;
					return -1;
				}
				else
				{
					source.m[wordSourceIndex].setWinner(adverbFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					errorMap[L"LP correct: adverb rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[L"adverb"]++;
					return -1;
				}
			}
			else
			{
				if (particleFormOffset > 0 && !WordClass::isDash((source.m[wordSourceIndex + 1].word->first[0])))
				{
					source.m[wordSourceIndex].setWinner(particleFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					if (primarySTLPMatch == L"particle")
						return -2;
					errorMap[L"LP correct: particle rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[L"particle"]++;
					return -1;
				}
				return 0;
			}
		}
		return -3;
	}
	if (source.m[wordSourceIndex].word->first == L"that" && (((source.m[wordSourceIndex].flags&WordMatch::flagInQuestion) && wordSourceIndex > 0 && 
		(source.m[wordSourceIndex - 1].queryForm(L"is") >= 0 || source.m[wordSourceIndex - 1].queryForm(L"is_negation") >= 0) &&
		source.m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm)>=0 && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) < 0) ||
		(source.m[wordSourceIndex].word->first == L"that" && !(source.m[wordSourceIndex].flags&WordMatch::flagInQuestion) && wordSourceIndex > 0 && (source.m[wordSourceIndex + 1].queryForm(L"is") >= 0 || source.m[wordSourceIndex + 1].queryForm(L"is_negation") >= 0) &&
		(!iswalpha(source.m[wordSourceIndex - 1].word->first[0]) || wordSourceIndex == startOfSentence)) ||
		(source.m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0 && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))))
	{
		source.m[wordSourceIndex].setWinner(source.m[wordSourceIndex].queryForm(pronounForm));
		source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(demonstrativeDeterminerForm));
	}
	// a word which LP thinks is an adverb, which is before a determiner, which ST thinks is a predeterminer and which has a predeterminer form
	if (source.m[wordSourceIndex].isOnlyWinner(adverbForm) && source.m[wordSourceIndex + 1].isOnlyWinner(determinerForm) && primarySTLPMatch == L"predeterminer" && source.m[wordSourceIndex].queryForm(predeterminerForm)!=-1)
	{
		source.m[wordSourceIndex].setWinner(source.m[wordSourceIndex].queryForm(predeterminerForm));
		source.m[wordSourceIndex].unsetWinner(adverbFormOffset);
		return -2;
	}
	return 0;
}

int attributeErrors(wstring primarySTLPMatch, Source &source, int wordSourceIndex, unordered_map<wstring, int> &errorMap, unordered_map<wstring, int> &comboCostFrequency, wstring &partofspeech, int startOfSentence)
{
	wstring word = source.m[wordSourceIndex].word->first;
	//////////////////////////////
	// corrections based on implementation/interpretation differences and statistical findings
	// 1. LP has a NOUN[2] which allows a noun in what should be an adjective position
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && wordSourceIndex < source.m.size() - 1 && source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0)
	{
		errorMap[L"diff: noun in adjective position"]++;
		return 0;
	}
	// 2. where 'that' is a demonstrative_determiner, and matches a REL1 pattern, then we count that as correct parse for LP as this is a relative phrase, and the usage of 'that' is correctly understood within that pattern.
	if (primarySTLPMatch == L"preposition or conjunction" && wordSourceIndex < source.m.size() - 1 && source.m[wordSourceIndex].queryWinnerForm(L"demonstrative_determiner")>=0)
	{
		int maxEnd, pemaPosition = source.queryPattern(wordSourceIndex, L"_REL1", maxEnd);
		if (pemaPosition >= 0 && (//(source.pema[pemaPosition].begin == 0 && patterns[source.pema[pemaPosition].getPattern()]->differentiator == L"2") || // REL1[2] includes S1 
			(source.pema[pemaPosition].begin <= 0 && source.pema[pemaPosition].begin >= -5) || // must be the start of a relative clause
			source.scanForPatternElementTag(wordSourceIndex, SENTENCE_IN_REL_TAG) != -1))
		{
			errorMap[L"LP correct:" + word + L" start of relative phrase"]++;
			return 0;
		}
	}
	static set <wstring> speakingVerbs = { 
					L"added",L"agreed",L"announced",L"answered",L"approved",L"asked",L"aspirated",L"assented",L"barked",L"bawled",L"beamed",L"begged",
					L"bellowed",L"beseeched",L"blazed",L"blurted",L"brayed",L"burst",L"called",L"chaffed",L"chimed",L"chorused",L"chuckled",L"commanded",
					L"commended",L"commented",L"continued",L"cried",L"croaked",L"declaimed",L"demanded",L"dimpled",L"drawled",L"droned",L"ejaculated",L"enquired",
					L"exclaimed",L"expostulated",L"exulted",L"fell",L"flung",L"fumed",L"gasped",L"gazing",L"gibed",L"grinned",L"groaned",L"growled",
					L"grumbled",L"grunted",L"guffawed",L"hooted",L"howled",L"implored",L"inquired",L"interjected",L"interposed",L"interrupted",L"jeered",L"joked",
					L"jubilated",L"laughed",L"leered",L"moaned",L"mourned",L"murmured",L"mused",L"muttered",L"observed",L"panted",L"persisted",L"promised",
					L"proposed",L"propounded",L"protested",L"puffed",L"pursued",L"put",L"questioned",L"quizzed",L"raved",L"reminded",L"remonstrated",L"repeated",L"replied",
					L"responded",L"retaliated",L"retorted",L"roared",L"said",L"sang",L"scoffed",L"scorned",L"scowled",L"screamed",L"seconded",L"secure",
					L"shot",L"shouted",L"shrieked",L"shrilled",L"smirked",L"snapped",L"snarled",L"sneered",L"snorted",L"sobbed",L"soliloquized",L"spluttered",
					L"stammered",L"stuttered",L"suggested",L"surmised",L"sympathized",L"thought",L"turning",L"twitted",L"used",L"ventured",L"wailed",L"wheezed",
					L"whined",L"whispered",L"whistled",L"yawned",L"yawped",L"yelled"
	};
	// Stanford POS NN (noun) not found in winnerForms determiner for word the 0002542:[” asked *the* mother . ]
	if (wordSourceIndex > 1 && primarySTLPMatch == L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"determiner") >= 0 && source.m[wordSourceIndex - 2].queryForm(quoteForm) >= 0 &&
			find(speakingVerbs.begin(), speakingVerbs.end(), source.m[wordSourceIndex - 1].word->first) != speakingVerbs.end())
	{
		errorMap[L"LP correct: 'the' is not a noun"]++;
		return 0;
	}
	// 3. ST is always wrong when given a phrase like [(ROOT (S ('' '') (S (S (VP (VBD said))) (VP (VBZ Bobtail)))] - LP correctly tags 'Bobtail' as a proper noun
	if (wordSourceIndex>=2 && primarySTLPMatch == L"verb" && source.m[wordSourceIndex].queryWinnerForm(L"Proper Noun") >= 0 && source.m[wordSourceIndex - 2].queryForm(quoteForm) >= 0 &&
			speakingVerbs.find(source.m[wordSourceIndex - 1].word->first) != speakingVerbs.end())
	{
		errorMap[L"LP correct: speaker is proper noun (not verb)"]++;
		return 0;
	}
	// 3b. ST is always wrong when given a phrase like " she *exclaimed* - LP correctly tags 'exclaimed' as a verb
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && wordSourceIndex > 2 && source.m[wordSourceIndex - 2].queryForm(quoteForm) >= 0 &&
		(source.m[wordSourceIndex-1].queryWinnerForm(L"personal_pronoun") >= 0 || source.m[wordSourceIndex-1].queryWinnerForm(L"Proper Noun") >= 0) &&
		speakingVerbs.find(source.m[wordSourceIndex].word->first) != speakingVerbs.end())
	{
		errorMap[L"LP correct: verb of speaking is a verb"]++;
		return 0;
	}
	// 3c. ST is always wrong when given a phrase like " added his father - LP correctly tags 'added' as a verb
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && source.m[wordSourceIndex - 1].queryForm(quoteForm) >= 0 &&
		wordSourceIndex > 1 && speakingVerbs.find(source.m[wordSourceIndex].word->first) != speakingVerbs.end())
	{
		errorMap[L"LP correct: verb of speaking is a verb"]++;
		return 0;
	}
	// 4. ST is always wrong when given a phrase like [(ROOT (S ('' '') (S (S (VP (VBD said))) (VP (VBZ Bobtail)))] - LP correctly tags 'said' as a verb
	// Stanford POS JJ(adjective) not found in winnerForms verb for word ejaculated 0002658:[” *ejaculated* Mrs.Ross .]
	if ((primarySTLPMatch == L"adjective" || primarySTLPMatch == L"Proper Noun" || primarySTLPMatch == L"noun") &&
		(source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"SYNTAX:Accepts S as Object") >= 0) && wordSourceIndex > 1 &&
		source.m[wordSourceIndex - 1].queryForm(quoteForm) >= 0 &&
		find(speakingVerbs.begin(), speakingVerbs.end(), word) != speakingVerbs.end())
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].word->first == L"the" || source.m[wordSourceIndex + 1].word->first == L"a" || source.m[wordSourceIndex + 1].word->first == L"one" ||
			source.m[wordSourceIndex + 1].queryWinnerForm(L"honorific") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"honorific_abbreviation") >= 0)
		{
			errorMap[L"LP correct: speaking verb is not adjective, noun or Proper Noun"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"preposition") >= 0 && (source.m[wordSourceIndex + 2].queryWinnerForm(L"Proper Noun") >= 0 || source.m[wordSourceIndex + 2].queryWinnerForm(L"honorific") >= 0 || source.m[wordSourceIndex + 2].queryWinnerForm(L"honorific_abbreviation") >= 0))
		{
			//partofspeech += L"SPV3";
			errorMap[L"LP correct: speaking verb is not adjective, noun or Proper Noun"]++;
			return 0;
		}
	}
	// 5. if ST thinks it is a verb, and LP thinks it is a noun, and it is preceded by a determiner separated only by up to 2 adjectives (that are not 'no'), unless it is a VBG and then it has to be immediately preceeded by a determiner
	//    examined 100 examples from gutenburg and 1 violated this rule.
	if ((primarySTLPMatch == L"verb") && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && wordSourceIndex > 2)
	{
		if (isStanfordDeterminerType(source, wordSourceIndex, wordSourceIndex - 1) || (partofspeech != L"VBG" &&
			((isStanfordDeterminerType(source, wordSourceIndex, wordSourceIndex - 2) && source.m[wordSourceIndex - 2].word->first != L"no" && (source.m[wordSourceIndex - 1].queryWinnerForm(adjectiveForm) >= 0)) ||
			(isStanfordDeterminerType(source, wordSourceIndex, wordSourceIndex - 3) && source.m[wordSourceIndex - 3].word->first != L"no" && source.m[wordSourceIndex - 2].queryWinnerForm(adjectiveForm) >= 0 && source.m[wordSourceIndex - 1].queryWinnerForm(adjectiveForm) >= 0))))
		{
			// further more, the 
			errorMap[L"LP correct: ST says verb when it is a noun (preceded by determiner)"]++;
			return 0;
		}
		// 6. if ST thinks it is a verb, and LP thinks it is a noun, and LP does not know of it having a verb form
		//    examined 100 examples from gutenburg and 0 violated this rule.
		else if (!source.m[wordSourceIndex].word->second.hasVerbForm())
		{
			errorMap[L"LP correct: ST says verb when it is a noun (no verb form possible)"]++;
			return 0;
		}
	}
	// 7. if ST thinks it is a noun, and LP does not know of it having a noun form
	//    examined 100 examples from gutenburg and X violated this rule.
	if (primarySTLPMatch == L"noun" && !source.m[wordSourceIndex].word->second.hasNounForm())
	{
		// However, if it is a present participle verb
		//    then this is only acceptable if LP has matched it to __N1.
		if (source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE)
		{
			if (source.m[wordSourceIndex].pma.queryPattern(L"__N1") != -1)
			{
				errorMap[L"diff: ST says noun when it is a present participle [acceptable]"]++;
				return 0; // ST and LP agree
			}
		}
		else
		{
			errorMap[L"LP correct: ST says noun when no noun form possible"]++;
			return 0; // ST and LP disagree and ST is wrong
		}
	}
	// 8. if ST thinks it is an adjective, and LP maps it to an __ADJECTIVE pattern
	//    examined 100 examples from gutenburg and 0 violated this rule.
	if ((primarySTLPMatch == L"adjective") && source.m[wordSourceIndex].queryForm(L"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PAST) == VERB_PAST && 
		  (source.m[wordSourceIndex].pma.queryPattern(L"__ADJECTIVE") != -1 || source.m[wordSourceIndex].pma.queryPattern(L"_ADJECTIVE_AFTER") != -1))
	{
		errorMap[L"diff: ST says adjective, LP says verb PAST matched to an _ADJECTIVE pattern"]++;
		return 0;
	}
	// 9. In LP rules, here is not an adverb.  It designates a place and therefore is a noun.
	//    examined 10 examples from gutenburg and 0 violated this rule.
	if ((primarySTLPMatch == L"adverb") && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && word == L"here")
	{
		errorMap[L"diff: ST says here is an adverb, LP says prefers to say a noun"]++;
		return 0;
	}
	// 10. out of 849 examples, 815 were marked LP correct,  10 for ST and 24 were neither.
	// this is because LP is able to use statistics regarding Proper Nouns which are not used in ST, and also all caps words are particularly marked as being Proper Nouns when they are hardly ever proper nouns.
	if ((primarySTLPMatch == L"Proper Noun") || source.m[wordSourceIndex].queryWinnerForm(L"Proper Noun") >= 0)
	{
		errorMap[L"LP correct: ST says Proper Noun when it is not or does not say it is a proper noun when it is"]++;
		return 0;
	}
	// 11. numeral_cardinal counts as an adjective for LP if it matches the pattern __ADJECTIVE or _TIME (with the first match being an adjective of how much time)
	if ((primarySTLPMatch == L"adjective") && source.m[wordSourceIndex].queryWinnerForm(L"numeral_cardinal") >= 0 &&
		(source.m[wordSourceIndex].pma.queryPattern(L"__ADJECTIVE") != -1 || source.m[wordSourceIndex].pma.queryPattern(L"_TIME") != -1))
	{
		errorMap[L"diff: ST says here is an adjective, LP says numeral_cardinal if matching _ADJECTIVE or _TIME"]++;
		return 0;
	}
	// 12. "one" is post processed and understood as a pronoun by LP, even if it is only matched as a numeral_cardinal. (10 examples examined)
	//   this is often encountered if the author is fond of speaking in the general person ('one')
	if (word == L"one" && (primarySTLPMatch == L"personal_pronoun_accusative") && source.m[wordSourceIndex].queryWinnerForm(L"numeral_cardinal") >= 0 &&
		(source.m[wordSourceIndex].pma.queryPattern(L"__ADJECTIVE") != -1 || source.m[wordSourceIndex].pma.queryPattern(L"__NOUN") != -1))
	{
		errorMap[L"LP correct: ST says noun when no noun form possible"]++;
		return 0;
	}
	// 13. numeral_ordinal counts as an noun for LP if it matches the pattern __NOUN
	if ((primarySTLPMatch == L"noun") && source.m[wordSourceIndex].queryWinnerForm(L"numeral_ordinal") >= 0 && source.m[wordSourceIndex].pma.queryPattern(L"__N1") != -1)
	{
		errorMap[L"diff: ST says noun, LP says numeral_ordinal matched to _N1 (noun subpattern)"]++;
		return 0;
	}
	// 14. LP does not have 'any' as a determiner but rather as a pronoun/adjective which is used internally as an indicator as to how to match the noun to other nouns.
	// out of 100 examples, 100% were interpreted correctly - added 'any' as a determiner when calculating noun/determiner agreement cost
	if (word == L"any" && primarySTLPMatch == L"determiner" &&
		(source.m[wordSourceIndex].pma.queryPattern(L"__ADJECTIVE") != -1 && source.m[wordSourceIndex].pma.queryPattern(L"__NOUN") != -1))
	{
		errorMap[L"diff: ST says determiner, LP says adjective which is used as a determiner in post-processing"]++;
		return 0;
	}
	// 15. in the event of __AS_AS pattern, which is as (adverb) followed by an adjective or adverb followed by as (preposition)
	// either adverb or preposition may be acceptable as both are incorporated into the pattern.  __AS_AS pattern was derived from Longman
	if (word == L"as" && (primarySTLPMatch == L"preposition or conjunction" || primarySTLPMatch == L"adverb"))
	{
		if (source.m[wordSourceIndex].pma.queryPattern(L"__AS_AS") != -1)
		{
			errorMap[L"diff: word 'as': ST says " + primarySTLPMatch + L", LP says __AS_AS (Longman adverbial clause) - first as"]++;
			return 0;
		}
		if (wordSourceIndex >= 2 && source.m[wordSourceIndex - 2].word->first == L"as" && source.m[wordSourceIndex - 2].pma.queryPattern(L"__AS_AS") != -1)
		{
			errorMap[L"diff: word 'as': ST says " + primarySTLPMatch + L", LP says __AS_AS (Longman adverbial clause) - second as"]++;
			return 0;
		}
	}
	if (source.m[wordSourceIndex].queryWinnerForm(L"does") >= 0)
	{
		errorMap[L"LP correct: word 'does': ST says " + primarySTLPMatch + L" LP says helper verb (does)"]++;
		return 0;
	}
	// 16. So as matched in the beginning of a phrase is a linking adverbial (Longman) but is usually marked as a preposition by Stanford.
	bool atStart = wordSourceIndex == startOfSentence || (wordSourceIndex == startOfSentence + 1 && WordClass::isDoubleQuote(source.m[wordSourceIndex - 1].word->first[0]));
	if ((word == L"so" || word==L"either") && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 &&
		(primarySTLPMatch == L"preposition or conjunction" || primarySTLPMatch == L"conjunction") && atStart)
	{
		errorMap[L"LP correct: word 'so,either': ST says " + primarySTLPMatch + L" LP says adverb (Longman linking adverbial)"]++;
		return 0;
	}
	if ((word == L"up"  || word == L"off") && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 &&
		(primarySTLPMatch == L"preposition or conjunction" || primarySTLPMatch == L"conjunction") && atStart && source.m[wordSourceIndex+1].queryWinnerForm(L"preposition")>=0)
	{
		errorMap[L"LP correct: word 'up': ST says " + primarySTLPMatch + L" LP says adverb (Longman linking adverbial)"]++;
		return 0;
	}
	if (word == L"before" && primarySTLPMatch == L"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && atStart)
	{
		errorMap[L"LP correct: word 'before' at the start of a sentence"]++;
		return 0;
	}
	bool tempstar = false;
	// 17. this is determined to be an adverb by LP, but this is wrong, it is actually a determiner for the preceding word
	//     over 100 items have been examined and 0 errors. - note this is also AFTER the POS processing has narrowed it down to disagreements with Stanford POS
	// case 1 is all after a plural noun.
	// case 2 is all before a determiner or 'right'. (39 out of 178 cases)
	//   
	int wallp = -1, qallp = (wordSourceIndex + 1 < source.m.size()) ? source.m[wordSourceIndex + 1].pma.queryPattern(L"__NOUN") : -1;
	if (qallp >= 0)
		wallp = wordSourceIndex + 1 + source.m[wordSourceIndex + 1].pma[qallp].len - 1;
	if (word == L"all" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && (primarySTLPMatch == L"determiner" || primarySTLPMatch == L"predeterminer") &&
		(source.m[wordSourceIndex - 1].queryWinnerForm(L"demonstrative_determiner") >= 0 || // case 1
		(source.m[wordSourceIndex + 1].queryForm(L"demonstrative_determiner") >= 0) || //  case 2- all these / all those / all this / all that
			(source.m[wordSourceIndex + 1].queryForm(L"possessive_determiner") >= 0) || // case 2- all my / all his / all her
			(source.m[wordSourceIndex + 1].queryForm(L"determiner") >= 0) || // case 2- all the ...
			(source.m[wordSourceIndex + 1].word->first == L"right") || // case 2- all right
			source.m[wordSourceIndex - 1].queryWinnerForm(L"personal_pronoun") >= 0 || // case 1
			source.m[wordSourceIndex - 1].word->first == L"them" || // case 1
			source.m[wordSourceIndex - 1].word->first == L"they" || // case 1
			source.m[wordSourceIndex - 1].word->first == L"we" || // case 1
			source.m[wordSourceIndex - 1].word->first == L"us" || // case 1
			((source.m[wordSourceIndex - 1].pma.queryPattern(L"__N1") != -1 || source.m[wordSourceIndex - 1].pma.queryPattern(L"__NOUN") != -1) && // case 1
			(source.m[wordSourceIndex - 1].word->second.inflectionFlags&PLURAL) == PLURAL) ||
				(tempstar = wallp >= 0 && (source.m[wallp].word->second.inflectionFlags&PLURAL) == PLURAL)
			)
		)
	{
		errorMap[L"ST correct: word 'all': [after plural noun or before a determiner or 'right'] ST says " + primarySTLPMatch + L" LP says adverb"]++;
		return 0;
	}
	else if (word == L"all" && source.m[wordSourceIndex].queryWinnerForm(L"predeterminer") >= 0 && primarySTLPMatch == L"adverb" && 
		       (source.m[wordSourceIndex + 1].queryWinnerForm(L"determiner") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"possessive_determiner") >= 0))
	{
		errorMap[L"ST correct: word 'all': [after determiner/possessive determiner] ST says " + primarySTLPMatch + L" LP says adverb"]++;
		return 0;
	}
	// 18. This 'All' should be classified as a subject (2 matches)
	else if (word == L"all" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 &&
		(source.m[wordSourceIndex + 1].word->first == L"are" || source.m[wordSourceIndex + 1].word->first == L"was"))
	{
		errorMap[L"ST correct: word 'all': [before 'are' or 'was'] ST says " + primarySTLPMatch + L" LP says adverb"]++;
		return 0;
	}
	// 19. this should be an adjective (all 12 examples checked)
	else if (word == L"all" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 &&
		(source.m[wordSourceIndex - 1].queryForm(L"is") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(L"modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(L"future_modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(L"negation_modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(L"negation_future_modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(L"is_negation") >= 0))
	{
		errorMap[L"ST correct: word 'all': [after 'is' or modal_auxiliary] ST says " + primarySTLPMatch + L" LP says adverb"]++;
		return 0;
	}
	// 19b. All immediately before verb or preposition is definitely an adverb (checked with 100 examples)
	else if (word == L"all" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 &&
		(source.m[wordSourceIndex + 1].queryWinnerForm(L"preposition") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(verbForm) >= 0))
	{
		errorMap[L"LP correct: word 'all': [before a verb or a preposition] ST says " + primarySTLPMatch + L" LP says adverb"]++;
		return 0;
	}
	// 20. each other seems to always be interpreted as a DET followed by an adjective.  For LP's purposes, this is a reciprocal pronoun!
	if (word == L"each other")
	{
		errorMap[L"LP correct: word 'each other': ST says DET adjective LP says reciprocal pronoun"]++;
		return 0;
	}
	// 21. one another is always be interpreted by ST as a CD (numeral_cardinal) followed by a DT (determiner!).  For LP's purposes, this is a reciprocal pronoun!
	if (word == L"one another")
	{
		errorMap[L"LP correct: word 'one another': ST says DET CD, LP says reciprocal pronoun"]++;
		return 0;
	}
	// 22. no one is always be interpreted by ST as a RB (adverb) followed by a CD (numeral_cardinal).  For LP's purposes, this is an indefinite pronoun!
	if (word == L"no one")
	{
		errorMap[L"LP correct: word 'no one': ST says RB CD, LP says indefinite pronoun"]++;
		return 0;
	}
	// 23. every one can be interpreted by ST as a DT (determiner) followed by a CD (numeral_cardinal) .  For LP's purposes, this is an indefinite pronoun!
	if (word == L"every one")
	{
		errorMap[L"LP correct: word 'every one': ST says DT CD, LP says indefinite pronoun"]++;
		return 0;
	}
	// 24. Stanford sometimes guesses that as an adverb as well (all 3 examples checked)
	if (word == L"that" && source.scanForPatternTag(wordSourceIndex, SENTENCE_IN_REL_TAG) != -1)
	{
		errorMap[L"LP correct: word 'that': ST says adverb, LP says relativizer [beginning of SENTENCE_IN_REL_TAG]"]++;
		return 0;
	}
	// 25. 'So' before _S1 is a linking adverbial, not a preposition (Longman - 891)
	if (word == L"so" && (primarySTLPMatch == L"preposition or conjunction" || primarySTLPMatch == L"conjunction") && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 &&
		wordSourceIndex + 1 < source.m.size() && (source.m[wordSourceIndex + 1].pma.queryPattern(L"__S1") != -1))
	{
		errorMap[L"LP correct: word 'so' before _S1 is a linking adverbial, not a preposition (Longman - 891)"]++;
		return 0;
	}
	// 26. 'So' before a period or a comma is a pro-form (derived from Longman), but ST says it is an adverb
	if (word == L"so" && primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].queryWinnerForm(L"pronoun") >= 0 &&
		wordSourceIndex + 1 < source.m.size() &&
		(source.m[wordSourceIndex + 1].word->first == L"." || source.m[wordSourceIndex + 1].word->first == L"?" || source.m[wordSourceIndex + 1].word->first == L"!" ||
			source.m[wordSourceIndex + 1].word->first == L","))
	{
		errorMap[L"LP correct: word 'so' before a period or a comma is a pro-form (derived from Longman), but ST says it is an adverb"]++;
		return 0;
	}
	// 27. 'Such' or 'all' before a noun is a predeterminer (LP), not an adjective (ST)!
	if ((word == L"such" || word == L"all") &&
		(primarySTLPMatch == L"adjective") && source.m[wordSourceIndex].queryWinnerForm(L"predeterminer") >= 0)
	{
		errorMap[L"LP correct: word 'such or all': ST says adjective, LP says predeterminer (derived from Longman)"]++;
		return 0;
	}
	// 28. 'Dear' is misinterpreted to be an adverb or a verb (!), and in the 76 examples, dear is almost always correctly interpreted by LP to be an interjection or an adjective.
	if (word == L"dear")
	{
		errorMap[L"LP correct: word 'dear': ST says " + primarySTLPMatch + L" LP says interjection or adjective"]++;
		return 0;
	}
	// 29. 'Though' or 'though' when LP determines is a conjunction (equivalent of a Longman subordinator), Stanford still insists that it is an adverb (wrong)
	if ((word == L"though") && (primarySTLPMatch == L"adverb") && source.m[wordSourceIndex].queryWinnerForm(L"conjunction") >= 0)
	{
		errorMap[L"LP correct: word 'though': ST says adverb LP says conjunction"]++;
		return 0;
	}
	// 30. 'her' when followed by an adverb ending in an 'ly' OR a determiner (a, the) or a personal_pronoun_nominative (I, we) or a coordinator (and,or) or an indefinite_pronoun (everything, nothing) is a pronoun (ST wrong)
	if (word == L"her" && primarySTLPMatch == L"possessive_determiner" && source.m[wordSourceIndex].queryWinnerForm(L"personal_pronoun_accusative") >= 0 &&
		wordSourceIndex + 1 < source.m.size() &&
		(source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(L"determiner") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(L"possessive_determiner") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(L"personal_pronoun_nominative") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(L"coordinator") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(L"indefinite_pronoun") >= 0
			))
	{
		errorMap[L"LP correct: word 'her': [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"]++;
		return 0;
	}
	if (word == L"her" && primarySTLPMatch == L"possessive_determiner" && source.m[wordSourceIndex].queryWinnerForm(L"personal_pronoun_accusative") >= 0
		&& source.m[wordSourceIndex].pma.queryPatternDiff(L"__NOUN",L"C") != -1 && source.m[wordSourceIndex].pma.queryPattern(L"__ALLOBJECTS_2") != -1)
	{
		partofspeech += L"***OBJECT2HER";
		//errorMap[L"LP correct: word 'her': [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"]++;
		//return 0;
	}
	// 30. 'her' when in the beginning of a __NOUN[2]
	if (word == L"her" && source.m[wordSourceIndex].queryWinnerForm(L"possessive_determiner") >= 0 && source.m[wordSourceIndex].pma.queryPattern(L"__NOUN") != -1)
	{
		int nf;
		if ((nf = source.m[wordSourceIndex + 1].queryWinnerForm(nounForm)) >= 0)
		{
			int nounCost = source.m[wordSourceIndex + 1].word->second.getUsageCost(nf);
			wstring tmpstr;
			partofspeech += L"***HERNOUNCOST"+itos(nounCost,tmpstr);
			int af = source.m[wordSourceIndex + 1].queryForm(adverbForm);
			if (af >= 0)
			{
				int adverbCost = source.m[wordSourceIndex + 1].word->second.getUsageCost(af);
				if (nounCost == 0 && adverbCost > 0)
				{
					errorMap[L"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"]++; // probability 6 out of 130 are ST correct
					return 0;
				}
			}
			else if ((source.m[wordSourceIndex+1].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) != VERB_PRESENT_PARTICIPLE)
			{
				errorMap[L"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"]++; // probability 6 out of 130 are ST correct
				return 0;
			}
		}
		else
			partofspeech += L"***HERNONOUN";
		//errorMap[L"LP correct: word 'her': [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"]++;
		//return 0;
	}
	// 31. 'that' when followed by an _S1 is a relativizer, not a preposition (ST wrong)
	if (word == L"that")
	{
		if (wordSourceIndex + 1 < source.m.size() && source.m[wordSourceIndex + 1].pma.queryPattern(L"__S1") != -1 && primarySTLPMatch == L"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(L"demonstrative_determiner") >= 0)
		{
			for (int nextByPosition = source.m[wordSourceIndex].beginPEMAPosition; nextByPosition != -1; nextByPosition = source.pema[nextByPosition].nextByPosition)
			{
				cPattern *p = patterns[source.pema[nextByPosition].getPattern()];
				if (p->name == L"__S1" && p->differentiator == L"5" && (source.pema[nextByPosition].getElement() == 2 || source.pema[nextByPosition].getElement() == 3))
				{
					errorMap[L"LP correct: word 'that': [embedded in _S1[5]] ST says preposition LP says demonstrative_determiner (relativizer)"]++;
					return 0;
				}
			}
			errorMap[L"LP correct: word 'that': [immediately preceding _S1 otherwise unidentified (hidden in pattern, must be identified in post-processing)] ST says preposition LP says demonstrative_determiner (relativizer)"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == L"modal_auxiliary" && (source.m[wordSourceIndex].queryWinnerForm(L"verbverb") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"does") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"does_negation") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"have_negation") >= 0))
	{
		errorMap[L"diff: ST says modal_auxiliary and LP says verbverb, does, does_negation, have_negation"]++;
		return 0;
	}
	// 32. 'out' is an adverb particle (Longman 78,413), not a preposition (ST wrong)
	if (word == L"out" && primarySTLPMatch == L"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		errorMap[L"LP correct: word 'out': [adverb particle] ST says preposition LP says adverb"]++;
		return 0;
	}
	// 33. 'at least' is an adverbial phrase (Longman 542), least for LP could be a pronoun or a quantifier (https://english.stackexchange.com/questions/107396/what-are-the-parts-of-speech-of-at-and-least-in-at-least)
	// most is a quantifier and a degree adverb, unlike least (Longman, 522)
	// the POS for at least is simply not well established.  LP has it as a pronoun because that works within the resolving logic
	if ((word == L"least" || word == L"most") &&
		primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"pronoun") >= 0)
	{
		errorMap[L"diff: word 'least': [part of adverbial phrase 'at least' or more of a noun 'the least'] ST says adjective"]++;
		return 0;
	}
	if (word==L"to-morrow" && primarySTLPMatch == L"noun")
	{
		errorMap[L"diff: word 'to-morrow': ST says noun, but this is always used as a time adverbial"]++;
		return 0;
	}
	// (adjective) not found in winnerForms modal_auxiliary for word wouldhad
	if (word == L"wouldhad" && source.m[wordSourceIndex].queryWinnerForm(L"modal_auxiliary") >= 0 && primarySTLPMatch == L"adjective")
	{
		errorMap[L"diff: word 'wouldhad': wouldhad special to LP"]++;
		return 0;
	}
	// 34. possessive pronoun section - ST likes to think of possessive pronouns as verbs.  This will have to be investigated at some point by examining the collection of statistics from the original source material
	if (word == L"mine" && source.m[wordSourceIndex].queryWinnerForm(L"possessive_pronoun") >= 0)
	{
		if (primarySTLPMatch == L"verb" || primarySTLPMatch == L"interjection")
		{
			errorMap[L"LP correct: word 'mine': [verb or interjection] ST says verb or interjection LP says possessive_pronoun, which is always correct (26 examples out of 5968058)"]++;
			return 0;
		}
		if ((primarySTLPMatch == L"noun" || primarySTLPMatch == L"personal_pronoun_accusative") && source.m[wordSourceIndex].queryWinnerForm(L"possessive_pronoun") >= 0)
		{
			errorMap[L"LP correct: word 'mine': ST says noun or personal_pronoun_accusative, LP says possessive_pronoun"]++;
			return 0;
		}
	}
	if (word == L"yours" && source.m[wordSourceIndex].queryWinnerForm(L"possessive_pronoun") >= 0 &&
		(primarySTLPMatch == L"verb" || primarySTLPMatch == L"interjection" || primarySTLPMatch == L"numeral_cardinal" || primarySTLPMatch == L"adjective" || primarySTLPMatch == L"adverb" || primarySTLPMatch == L"possessive_determiner"))
	{
		errorMap[L"LP correct: word 'yours': ST says verb, interjection, numeral_cardinal, adjective or possessive_determiner LP says possessive_pronoun, which is always correct"]++;
		return 0;
	}
	if (word == L"hers" && source.m[wordSourceIndex].queryWinnerForm(L"possessive_pronoun") >= 0 && (primarySTLPMatch == L"verb" || primarySTLPMatch == L"adjective"))
	{
		errorMap[L"LP correct: word 'hers': ST says verb, adjective LP says possessive_pronoun, which is always correct"]++;
		return 0;
	}
	// possessive pronoun section - end
	// 35. round is never a verb in the gutenberg corpus
	if (word == L"round" && (source.m[wordSourceIndex].queryWinnerForm(L"preposition") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0) && primarySTLPMatch == L"verb")
	{
		errorMap[L"LP correct: word 'round': ST says verb LP says preposition or adverb.  round as studied is never a verb in > 200 examples from corpus"]++;
		return 0;
	}
	// 36. please is rarely a verb.  It is mostly used as a discourse politeness marker (Longman)
	if (word == L"please" && source.m[wordSourceIndex].queryWinnerForm(L"politeness_discourse_marker") >= 0 && (primarySTLPMatch == L"verb" || primarySTLPMatch == L"adverb" || primarySTLPMatch == L"adjective"))
	{
		errorMap[L"LP correct: word 'please': ST says verb LP says politeness_discourse_marker"]++;
		return 0;
	}
	// 37. less is never a conjunction.  (Longman)
	if (word == L"less" && primarySTLPMatch == L"conjunction")
	{
		errorMap[L"LP correct: word 'less': ST says conjunction LP says quantifier/adverb/adjective"]++;
		return 0;
	}
	// 38. I is always a personal_pronoun_nominative
	if (word == L"i" && source.m[wordSourceIndex].queryWinnerForm(L"personal_pronoun_nominative") >= 0 && (primarySTLPMatch == L"noun" || primarySTLPMatch == L"interjection"))
	{
		errorMap[L"LP correct: word 'I': ST says " + primarySTLPMatch + L" LP says personal_pronoun_nominative"]++;
		return 0;
	}
	if (word == L"i" && source.m[wordSourceIndex].queryWinnerForm(L"roman_numeral") >= 0 && (source.m[wordSourceIndex-1].word->first==L"chapter" || source.m[wordSourceIndex - 1].word->first == L"book"))
	{
		errorMap[L"LP correct: word 'I': ST says " + primarySTLPMatch + L" LP says roman_numeral after chapter or book"]++;
		return 0;
	}
	// 39. a is always a determiner
	if (word == L"a" && source.m[wordSourceIndex].queryWinnerForm(L"determiner") >= 0 && (primarySTLPMatch.empty() || primarySTLPMatch == L"noun" || primarySTLPMatch == L"symbol"))
	{
		errorMap[L"LP correct: word 'a': ST says " + primarySTLPMatch + L" LP says determiner"]++;
		return 0;
	}
	// 40. but is never a verb (reviewed all examples in corpus)
	if (word == L"but" && source.m[wordSourceIndex].queryWinnerForm(L"conjunction") >= 0 && primarySTLPMatch == L"verb")
	{
		errorMap[L"LP correct: word 'but': ST says " + primarySTLPMatch + L" LP says conjunction"]++;
		return 0;
	}
	if (word == L"no" && primarySTLPMatch == L"determiner" && source.m[wordSourceIndex].queryWinnerForm(L"interjection") >= 0 && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))
	{
		errorMap[L"LP correct: word 'no': determiner before nothing is incorrect"]++;
		return 0;
	}
	if (word == L"no" && primarySTLPMatch == L"adverb") 
	{
		if (source.m[wordSourceIndex].queryWinnerForm(L"no") >= 0)
		{
			if (wordSourceIndex > 1 && source.m[wordSourceIndex - 1].word->second.mainEntry->first == L"say")
				errorMap[L"LP correct: word 'no' which is literally saying no"]++;
			else
				errorMap[L"diff: word 'no': ST says " + primarySTLPMatch + L" LP says 'no'"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].queryWinnerForm(L"interjection") >= 0 && (atStart || !iswalpha(source.m[wordSourceIndex - 1].word->first[0] || source.m[wordSourceIndex-1].queryForm(L"interjection") >= 0 || source.m[wordSourceIndex - 1].word->first==L"but") && !iswalpha(source.m[wordSourceIndex + 1].word->first[0])))
		{
			errorMap[L"LP correct: word 'no' is interjection not adverb when alone"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].queryWinnerForm(L"determiner") >= 0 && source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0)
		{
			errorMap[L"LP correct: word 'no' is a determiner and not adverb when immediately before a noun"]++;
			return 0;
		}
	}
	// the is never anything but a determiner
	if (word == L"the" && source.m[wordSourceIndex].queryWinnerForm(L"determiner") >= 0)
	{
		errorMap[L"LP correct: word 'the': ST says " + primarySTLPMatch + L" LP says determiner"]++;
		return 0;
	}
	// worth while
	if (word == L"while" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && wordSourceIndex>=2 &&
		  ((WordClass::isDash(source.m[wordSourceIndex-1].word->first[0]) && source.m[wordSourceIndex - 2].word->first==L"worth") || source.m[wordSourceIndex - 1].word->first==L"worth"))
	{
		errorMap[L"LP correct: word 'while': ST says " + primarySTLPMatch + L" LP says noun [worthwhile]"]++;
		return 0;
	}
	if (word == L"while" && source.m[wordSourceIndex].queryWinnerForm(L"uncertainDurationUnit") >= 0 && source.m[wordSourceIndex].pma.queryPattern(L"__INTRO_N") != -1)
	{
		errorMap[L"LP correct: word 'while': ST says " + primarySTLPMatch + L" LP says uncertainDurationUnit [__INTRO_N]"]++;
		return 0;
	}
	// 40. but is never a verb (reviewed all examples in corpus)
	if (word == L"want" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && primarySTLPMatch == L"verb" && 
		  (source.m[wordSourceIndex-1].word->first==L"in" || source.m[wordSourceIndex - 1].word->first == L"for" || source.m[wordSourceIndex - 1].word->first == L"from" || source.m[wordSourceIndex - 1].word->first == L"by" || source.m[wordSourceIndex - 1].word->first == L"of"))
	{
		errorMap[L"LP correct: word 'want': ST says " + primarySTLPMatch + L" LP says noun"]++;
		return 0;
	}
	// 41. in between two adverbs/adjectives, a verb and adverb/adjective or all/does/has and a verb.  100 examples in corpus with 100% correctness.
	if (wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		if (((source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"adjective") >= 0) &&
			   (source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0)) || 
				((source.m[wordSourceIndex - 1].queryWinnerForm(L"verb") >= 0) &&
				 (source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0)) || 
		    ((source.m[wordSourceIndex - 1].word->first == L"all" || source.m[wordSourceIndex - 1].queryWinnerForm(L"does") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"has") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"is") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"verbverb") >= 0) &&
				 (source.m[wordSourceIndex + 1].queryWinnerForm(L"verb") >= 0)))
		{
			set <wstring> wordsNotAdverbs = { L"dark",L"to-night",L"to-morrow",L"there" };
			if (wordsNotAdverbs.find(source.m[wordSourceIndex + 1].word->first)== wordsNotAdverbs.end())
				errorMap[L"LP correct: ST says " + primarySTLPMatch + L" LP says adverb [contextual]"]++;
			else
				errorMap[L"ST correct: ST says " + primarySTLPMatch + L" LP says adverb (next word [dark, to-night, etc] not adverb!)"]++;
			return 0;
		}
	}
	// 42. this is correct 95% of the time in the corpus (over 100 examples).  Only once was it perhaps a conjunction.
	if (word == L"but" && wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(L"preposition") >= 0)
	{
		errorMap[L"LP correct: word 'but': ST says " + primarySTLPMatch + L" LP says preposition"]++;
		return 0;
	}
	// 43. this is correct 100% of the time in the corpus (over 100 examples).  
	if (word == L"more" && wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(L"verb") >= 0) ||
			(source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0 && source.m[wordSourceIndex + 1].queryForm(L"indefinite_pronoun") < 0) ||
			(source.m[wordSourceIndex - 1].word->first == L"the"))
		{
			errorMap[L"LP correct: word 'more': ST says " + primarySTLPMatch + L" LP says adverb"]++;
			return 0;
		}
	}
	if (word == L"more" && wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(L"quantifier") >= 0)
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0 && source.m[wordSourceIndex + 1].queryForm(L"verb")<0)
		{
			errorMap[L"LP correct: word 'more': ST says " + primarySTLPMatch + L" LP says quantifier"]++;
			return 0;
		}
	}
	// p80, Longman
	if (word == L"yet" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && primarySTLPMatch == L"conjunction")
	{
		errorMap[L"diff: word 'yet': can be either a conjunction or a linking adverbial"]++;
		return 0;
	}
	// never correct
	if (word == L"more" && (primarySTLPMatch == L"interjection" || primarySTLPMatch == L"verb"))
	{
		errorMap[L"LP correct: word 'more': ST says " + primarySTLPMatch]++;
		return 0;
	}
	// never correct
	if (word == L"once" && primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && (source.m[wordSourceIndex - 1].word->first==L"at" || source.m[wordSourceIndex - 1].word->first == L"for"))
	{
		errorMap[L"diff: word 'once': in saying 'at once' or 'for once'"]++;
		return 0;
	}
	// never correct
	if ((word == L"after" || word == L"besides") && primarySTLPMatch == L"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 &&
		  (!iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || source.m[wordSourceIndex + 1].queryWinnerForm(L"verb")>=0))
	{
		errorMap[L"LP correct: word 'after, besides': ST says " + primarySTLPMatch + L"but LP says adverb"]++;
		return 0;
	}
	// Longman p85 subordinator 'as though' - subordinating conjunction
	if (word == L"as" && primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].queryWinnerForm(L"conjunction") >= 0 && source.m[wordSourceIndex + 1].word->first==L"though")
	{
		errorMap[L"LP correct: word 'as': ST says adverb but LP says conjunction"]++;
		return 0;
	}
	if (word == L"as") // && source.m[wordSourceIndex + 1].word->first == L"to")
	{
		// 'as to stand outside in the wind' - 'as' is part of a subordinating conjunctive phrase and so therefore a conjunction
		if (source.m[wordSourceIndex].queryWinnerForm(L"conjunction") >= 0 && (source.m[wordSourceIndex + 1].pma.queryPattern(L"_INFP") != -1 || source.m[wordSourceIndex].pma.queryPattern(L"_INFP") != -1) && source.m[wordSourceIndex + 1].word->first!=L"if")
		{
			errorMap[L"LP correct: word 'as': ST says " + primarySTLPMatch + L" but LP says conjunction (complex subordinator)"]++;
			return 0;
		}
		// 'as to the romantic nonsense' - 'as' is a preposition - part of the complex preposition referred to in Longman
		if (source.m[wordSourceIndex].queryWinnerForm(L"preposition") >= 0 && (source.m[wordSourceIndex + 1].pma.queryPattern(L"_PP") != -1 || source.m[wordSourceIndex].pma.queryPattern(L"_PP") != -1))
		{
			errorMap[L"LP correct: word 'as': ST says " + primarySTLPMatch + L" but LP says preposition (complex preposition)"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].pma.queryPattern(L"__S1") != -1)
		{
			if (primarySTLPMatch == L"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
			{
				errorMap[L"LP correct: word 'as': ST says preposition and LP says adverb"]++;
				return 0;
			}
			if (primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].queryWinnerForm(L"conjunction") >= 0)
			{
				errorMap[L"diff: word 'as': ST says adverb and LP says conjunction"]++;
				return 0;
			}
		}
		// followed by a preposition?
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"preposition") >= 0)
		{
			partofspeech += L"**ASPREP";
		}
		// followed by a preposition?
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"verb") >= 0)
		{
			partofspeech += L"**ASVERB";
		}
		// followed by an adverb?
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0 && source.m[wordSourceIndex].pma.queryPattern(L"__ADJECTIVE")!=-1)
		{
			partofspeech += L"**ASADVERB";
		}
		// followed by an adjective? [2 instances where conjunction might be considered out of 100 observed]
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0)
		{
			errorMap[L"LP correct: word 'as': ST says preposition or conjunction and LP says adverb"]++;
			return 0;
		}
		// followed by a noun?
		if (source.m[wordSourceIndex + 1].pma.queryPattern(L"__NOUN") != -1)
		{
			partofspeech += L"**ASNOUN";
		}
	}
	if ((primarySTLPMatch == L"adjective") && (source.m[wordSourceIndex].queryWinnerForm(L"honorific_abbreviation") !=-1)) // source.m[wordSourceIndex].queryWinnerForm(L"honorific") !=-1 || 
	{
		errorMap[L"LP correct: ST says adjective when LP says honorific/honorific_abbreviation"]++;
		return 0;
	}
	// 45. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == L"verb") && !source.m[wordSourceIndex].word->second.hasVerbForm())
	{
		errorMap[L"LP correct: ST says verb when no verb form possible"]++;
		return 0;
	}
	// 46. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == L"conjunction") && source.m[wordSourceIndex].queryForm(L"conjunction") < 0)
	{
		errorMap[L"LP correct: ST says conjunction when no conjunction form possible)"]++;
		return 0;
	}
	// 47. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == L"preposition or conjunction") && source.m[wordSourceIndex].queryForm(L"preposition") < 0)
	{
		errorMap[L"LP correct: ST says preposition when no preposition form possible)"]++;
		return 0;
	}
	// 48. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == L"adverb") && source.m[wordSourceIndex].queryForm(L"adverb") < 0 &&
		   (source.m[wordSourceIndex].queryWinnerForm(L"adjective") < 0 || word[word.length()-2]!=L'l' || word[word.length() - 1] != L'y')) // don't end in ly
	{
		errorMap[L"LP correct: ST says adverb when no adverb form possible)"]++;
		return 0;
	}
	// 49. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == L"interjection") && source.m[wordSourceIndex].queryForm(L"interjection") < 0)
	{
		//partofspeech += L"**INTERJ";
		errorMap[L"LP correct: ST says interjection when no interjection form possible)"]++;
		return 0;
	}
	// 50. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == L"numeral_cardinal") && source.m[wordSourceIndex].queryForm(L"numeral_cardinal") < 0)
	{
		errorMap[L"LP correct: ST says numeral_cardinal when no numeral_cardinal form possible)"]++;
		return 0;
	}
	// 51. this is correct 100% of the time 
	if ((primarySTLPMatch == L""))
	{
		//partofspeech += L"***FW";
		errorMap[L"LP correct: ST says interjection when no interjection form possible)"]++;
		return 0;
	}
	// 52. this is correct 100% of the time 
	if ((primarySTLPMatch == L"|||"))
	{
		errorMap[L"diff: ST says list but LP does not have that semantic category yet"]++;
		return 0;
	}
	// 53. this is correct 100% of the time 
	if ((primarySTLPMatch == L"symbol"))
	{
		errorMap[L"LP correct: ST says symbol when it is not a symbol"]++;
		return 0;
	}
	// 54. this is correct 100% of the time 
	if (primarySTLPMatch == L"determiner")
	{
		vector<wstring> determinerTypes = { L"determiner",L"demonstrative_determiner",L"possessive_determiner",L"interrogative_determiner", L"quantifier", L"numeral_cardinal" };
		bool detMatch = false;
		for (wstring dt : determinerTypes)
			if (detMatch = source.m[wordSourceIndex].queryWinnerForm(dt) >= 0)
				break;
		if (!detMatch)
		{
			if (word == L"both")
			{
				errorMap[L"LP correct: word 'both': ST says determiner when there is no following noun form (LP says pronoun)"]++;
				return 0;
			}
			if (word == L"a trifle" || word == L"a bit")
			{
				errorMap[L"LP correct: word 'a trifle' or 'a bit': ST says determiner when LP says adverb"]++;
				return 0;
			}
			if (word == L"another" && source.m[wordSourceIndex].queryWinnerForm(pronounForm) >= 0 && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) < 0 &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(determinerForm) >= 0 || !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) ||
					source.m[wordSourceIndex + 1].word->second.hasVerbForm() || source.m[wordSourceIndex + 1].queryWinnerForm(conjunctionForm) >= 0))
			{
				errorMap[L"LP correct: word 'another': ST says determiner when there is no following noun form (LP says pronoun)"]++;
				return 0;
			}
			if ((word == L"that" || word == L"this") && source.m[wordSourceIndex].queryWinnerForm(pronounForm) >= 0 && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) < 0 &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(determinerForm) >= 0 || !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) ||
					source.m[wordSourceIndex + 1].word->second.hasVerbForm() || source.m[wordSourceIndex + 1].queryWinnerForm(conjunctionForm) >= 0))
			{
				partofspeech += L"***PNPDET";
				//errorMap[L"LP correct: word 'another': ST says determiner when there is no following noun form (LP says pronoun)"]++;
				//return 0;
			}
			if (word == L"any" && source.m[wordSourceIndex].queryWinnerForm(adverbForm) >= 0 &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(adjectiveForm) >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(adverbForm) >= 0))
			{
				errorMap[L"LP correct: word 'any': ST says determiner when there is only a following adverb or adjective form (LP says adverb)"]++;
				return 0;
			}
			partofspeech += L"***DT";
		}
	}
	if (word == L"to-night" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		errorMap[L"LP correct: word 'to-night': ST says " + primarySTLPMatch + L" but LP says adverb"]++;
		return 0;
	}
	if (word == L"well" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && source.m[wordSourceIndex + 1].word->first == L"-")
	{
		errorMap[L"LP correct: word 'well': ST says " + primarySTLPMatch + L" but LP says adverb"]++;
		return 0;
	}
	if (word == L"well" && source.m[wordSourceIndex].queryWinnerForm(L"interjection") >= 0)
	{
		errorMap[L"LP correct: word 'well': ST says " + primarySTLPMatch + L" but LP says interjection"]++;
		return 0;
	}
	if (word == L"either" && (source.m[wordSourceIndex].pma.queryPatternDiff(L"__NOUN", L"O") !=-1 || source.m[wordSourceIndex].pma.queryPatternDiff(L"_VERBPRESENTC", L"O") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(L"_VERBPAST", L"O") != -1))
	{
		errorMap[L"LP correct: word 'either': ST says " + primarySTLPMatch + L" but LP says quantifier"]++;
		return 0;
	}
	if (word == L"neither" && source.m[wordSourceIndex].pma.queryPatternDiff(L"__NOUN", L"P") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(L"_VERBPRESENTC", L"P") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(L"_VERBPAST", L"P") != -1)
	{
		errorMap[L"LP correct: word 'either': ST says " + primarySTLPMatch + L" but LP says quantifier"]++;
		return 0;
	}
	if (word == L"you" && primarySTLPMatch == L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"personal_pronoun") >= 0)
	{
		errorMap[L"LP correct: word 'you': ST says " + primarySTLPMatch + L" but LP says personal_pronoun"]++;
		return 0;
	}
	// Stanford POS JJ (adjective) not found in winnerForms adverb for word little 0000121:[With a sigh she pressed the pillow more firmly under her cheek , and lay looking a *little* wistfully at her maid , who , having drawn back the curtains at the window , stood now regarding her with the discreet and confidential smile which drew from her a protesting frown of irritation . ]
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && wordSourceIndex + 1 < source.m.size() && source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0)
	{
		errorMap[L"LP correct: Modifying an adverb ST says " + primarySTLPMatch + L" but LP says adverb"]++;
		return 0;
	}
	if (primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].queryWinnerForm(L"preposition") >= 0 && wordSourceIndex + 1 < source.m.size() && source.m[wordSourceIndex + 1].pma.queryPattern(L"__NOUN")!=-1 && source.m[wordSourceIndex].pma.queryPattern(L"_PP") != -1)
	{
		errorMap[L"LP correct: ST says " + primarySTLPMatch + L" but LP says preposition - as the head of a _PP construct"]++;
		return 0;
		//partofspeech += L"***ADVPREP";
	}
	bool wordBeforeIsIs = wordSourceIndex >0 && (source.m[wordSourceIndex - 1].queryWinnerForm(isForm) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(isNegationForm) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(beForm) != -1 || source.m[wordSourceIndex - 1].word->first == L"being");
	bool wordBeforeIsVerb = wordSourceIndex > 0 && source.m[wordSourceIndex - 1].hasWinnerVerbForm();
	bool word2BeforeIsIs = wordSourceIndex>1 && (source.m[wordSourceIndex - 2].queryWinnerForm(isForm) != -1 || source.m[wordSourceIndex - 2].queryWinnerForm(isNegationForm) != -1 || source.m[wordSourceIndex - 2].queryWinnerForm(beForm) != -1 || source.m[wordSourceIndex - 2].word->first == L"being");
	bool word2BeforeIsVerb = wordSourceIndex>1 && source.m[wordSourceIndex - 2].hasWinnerVerbForm();
	vector<wstring> determinerTypes = { L"determiner",L"demonstrative_determiner",L"possessive_determiner",L"interrogative_determiner", L"quantifier", L"numeral_cardinal" };
	bool wordBeforeIsDeterminer = false;
	if (wordSourceIndex > 0)
		for (wstring dt : determinerTypes)
			if (wordBeforeIsDeterminer = source.m[wordSourceIndex - 1].queryWinnerForm(dt) >= 0)
				break;
	bool word2BeforeIsDeterminer = false;
	if (wordSourceIndex>1)
		for (wstring dt : determinerTypes)
			if (word2BeforeIsDeterminer = source.m[wordSourceIndex - 2].queryWinnerForm(dt) >= 0)
				break;
	bool wordAfterIsDeterminer = false;
	if (wordSourceIndex+1 <source.m.size())
		for (wstring dt : determinerTypes)
			if (wordAfterIsDeterminer = source.m[wordSourceIndex + 1].queryWinnerForm(dt) >= 0)
				break;
	wchar_t *unmodifiableForms[] = { L"relativizer",L"preposition",L"coordinator",L"conjunction",L"quantifier", L"adverb",L"adjective",L"personal_pronoun_accusative",L"personal_pronoun_nominative",L"personal_pronoun",L"reflexive_pronoun" };
	bool wordAfterIsUnmodifiable = false;
	if (wordSourceIndex+1 < source.m.size())
		for (wstring unForm : unmodifiableForms)
			if (wordAfterIsUnmodifiable = source.m[wordSourceIndex + 1].queryWinnerForm(unForm) >= 0)
				break;
	wordAfterIsUnmodifiable |= !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || wordAfterIsDeterminer;
	vector<wstring> pronounTypes = { L"personal_pronoun_accusative",L"personal_pronoun_nominative",L"personal_pronoun",L"reflexive_pronoun",L"indefinite_pronoun" };
	bool wordBeforeIsPronoun = false;
	if (wordSourceIndex > 0)
		for (wstring pn : pronounTypes)
			if (wordBeforeIsPronoun = source.m[wordSourceIndex - 1].queryWinnerForm(pn) >= 0)
				break;
	if (primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0 && wordSourceIndex + 1 < source.m.size() && wordSourceIndex>3)
	{
		// investigate later!
		if (((wordBeforeIsVerb && wordBeforeIsIs) || (word2BeforeIsVerb && word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0)) && wordAfterIsUnmodifiable)
		{
			if (source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0)
				errorMap[L"ST correct: ST says adverb but LP says adjective, IS following a verb and followed by an adjective or adverb"]++;
			else
			  errorMap[L"LP correct: ST says adverb but LP says adjective, IS following a verb and followed by a relativizer, preposition or coordinator"]++;
			return 0;
		}
		// verb *ADJ* (relativizer OR preposition OR coordinator or ,)
		if (((wordBeforeIsVerb && !wordBeforeIsIs)|| (word2BeforeIsVerb && !word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0)) && wordAfterIsUnmodifiable)
		{
			errorMap[L"ST correct: ST says adverb but LP says adjective, following a verb and followed by a relativizer, preposition or coordinator"]++;
			return 0;
		}
		else
		{
			int adjectivePEMAOffset = source.queryPattern(wordSourceIndex, L"__ADJECTIVE");
			// an *even* and noiseless step
			if (wordBeforeIsDeterminer && source.m[wordSourceIndex + 1].queryWinnerForm(L"coordinator") >= 0 && source.m[wordSourceIndex + 2].queryWinnerForm(L"adjective") >= 0 && source.m[wordSourceIndex + 3].queryWinnerForm(L"noun") >= 0)
			{
				errorMap[L"LP correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			// how *much* trouble
			else if ((wordBeforeIsDeterminer || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"preposition") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(L"conjunction") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(L"coordinator") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(L"adjective") >= 0 ||
				source.queryPattern(wordSourceIndex - 1, L"__ADJECTIVE") != -1 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"relativizer") >= 0 || 
				!iswalpha(source.m[wordSourceIndex - 1].word->first[0])) &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"timeUnit") >= 0))
			{
				errorMap[L"LP correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			else if ((wordBeforeIsDeterminer || source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0 ) && (source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0 || source.queryPattern(wordSourceIndex + 1, L"__ADJECTIVE") != -1 || source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0))
			{
				errorMap[L"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			// much better looking - true except for IS verbs (
			else if (source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0 && source.m[wordSourceIndex + 1].hasWinnerVerbForm())
			{
				errorMap[L"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			// correct except for the rare IS verb or a noun restatement (you measly scrub!)
			else if ((wordBeforeIsPronoun || source.m[wordSourceIndex - 1].queryWinnerForm(L"Proper Noun") >= 0) && source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") < 0)
			{
				errorMap[L"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			if (source.m[wordSourceIndex + 1].queryWinnerForm(L"Proper Noun") >= 0)
			{
				errorMap[L"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			else if (source.m[wordSourceIndex + 1].word->first == L"-")
			{
				if (source.m[wordSourceIndex + 2].queryWinnerForm(L"noun") >=0 && source.queryPattern(wordSourceIndex + 2, L"__ADJECTIVE") == -1)
				{
					errorMap[L"LP correct: ST says adverb but LP says adjective with dash and then a noun"]++;
					return 0;
				}
				else
				{
					errorMap[L"ST correct: ST says adverb but LP says adjective with dash and then a non-noun"]++;
					return 0;
				}
			}
			else if (adjectivePEMAOffset != -1 && source.queryPatternDiff(wordSourceIndex, L"__S1",L"7") != -1)
			{
				if (source.m[wordSourceIndex].queryWinnerForm(adjectiveForm) >= 0)
					errorMap[L"ST correct: ST says adverb but LP says adjective with __S1[7] before an adjective"]++;
				else
					errorMap[L"LP correct: ST says adverb but LP says adjective with __S1[7] alone"]++;
				return 0;
			}
			else if (adjectivePEMAOffset != -1 && source.queryPatternDiff(wordSourceIndex + source.pema[adjectivePEMAOffset].begin, L"__NOUN", L"2") != -1)
			{
				partofspeech += L"***ISADJECTIVENOTADVERBELSE?";
			}
			else
				partofspeech += L"***ISADVERBELSE";
		}
		if (word == L"o'clock" && primarySTLPMatch == L"adverb")
		{
			errorMap[L"diff: ST says adverb (which is correct by form) but LP says noun, from usage"]++;
			return 0;
		}
		if (word == L"but" && source.m[wordSourceIndex + 1].word->first==L"--")
		{
			errorMap[L"LP correct: 'but' before a double dash is a conjunction!"]++;
			return 0;
		}
		int maxlen = -1;
		if ((source.queryPattern(wordSourceIndex - 1, L"_BE", maxlen) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(L"is") >= 0) &&
			source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(L"verb") < 0)
		{
			errorMap[L"LP correct: ST says adverb but LP says adjective, following a being verb"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0)
	{
		int pemaOffset = source.queryPattern(wordSourceIndex, L"__NOUN");
		if (source.queryPatternDiff(wordSourceIndex, L"__NOUN", L"4") != -1)
		{
			errorMap[L"diff: ST says " + primarySTLPMatch + L" but LP says adjective in the head of a __NOUN construction"]++;
			return 0;
		}
		// two incorrect parses lead to inaccuracy (ST is correct)
		if (source.queryPatternDiff(wordSourceIndex, L"__NOUN", L"2") != -1 && source.m[wordSourceIndex].pma.queryPattern(L"__ADJECTIVE") != -1)
		{
			errorMap[L"LP correct: ST says " + primarySTLPMatch + L" but LP says adjective in an __ADJECTIVE construction"]++;
			return 0;
		}
		if (pemaOffset>=0 && WordClass::isDash((source.m[wordSourceIndex + 1].word->first[0])) && source.m[wordSourceIndex + 1].word->first.length()==1)
		{
			errorMap[L"LP correct: ST says " + primarySTLPMatch + L" but LP says adjective before dash"]++;
			return 0;
		}
		if (word == L"right")
		{
			errorMap[L"LP correct: word 'right': ST says " + primarySTLPMatch + L" but LP says adjective"]++;
			return 0;
		}
	}
	if (wordSourceIndex + 2 < source.m.size() && WordClass::isDash((source.m[wordSourceIndex + 1].word->first[0])) && source.m[wordSourceIndex + 1].word->first.length() == 1)
	{
		int pemaOffset = source.queryPattern(wordSourceIndex, L"__NOUN");
		bool adjectivePosition = (pemaOffset >= 0) ? (source.pema[pemaOffset].end > 1) : false, nounHeadPosition = (pemaOffset >= 0) ? (source.pema[pemaOffset].end == 1) : false;
		if (source.m[wordSourceIndex + 2].word->first != L"and" && source.m[wordSourceIndex + 2].word->first != L"to" && source.m[wordSourceIndex + 2].word->first != L"for" &&
			source.m[wordSourceIndex + 2].word->first != L"of" &&	source.m[wordSourceIndex + 2].queryWinnerForm(determinerForm) < 0 &&
			source.m[wordSourceIndex].queryWinnerForm(interjectionForm) < 0)
		{
			if ((source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"numeral_ordinal") >= 0) && 
				  (primarySTLPMatch == L"noun" || primarySTLPMatch == L"determiner" || primarySTLPMatch == L"predeterminer"))
			{
				errorMap[L"LP correct: ST says " + primarySTLPMatch + L" but LP says adjective before dash"]++;
				return 0;
			}
			else if (source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
			{
				errorMap[L"LP correct: ST says " + primarySTLPMatch + L" but LP says adjective/adverb before dash"]++;
				return 0;
			}
			else if ((source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0) && primarySTLPMatch == L"adjective" && (pemaOffset<0 || nounHeadPosition))
			{
				errorMap[L"ST correct: ST says " + primarySTLPMatch + L" but LP says noun before dash"]++;
				return 0;
			}
			else if (adjectivePosition)
			{
				errorMap[L"LP correct: ST says " + primarySTLPMatch + L" but LP says adjective position in __NOUN structure before dash"]++;
				return 0;
			}
		}
		//if (source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && source.m[wordSourceIndex + 2].word->first == L"and" && nounHeadPosition)
		//	partofspeech += L"***ADJNOUNDashCorrect?";
		//else 
		//	partofspeech += L"***ADJNOUNDashUnknown";
		//errorMap[L"LP correct: ST says " + primarySTLPMatch + L" but LP says adjective before dash"]++;
		//return 0;
	}
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0)
	{
		int pemaPosition = -1;
		// two incorrect parses lead to inaccuracy (ST is correct)
		if ((pemaPosition=source.queryPatternDiff(wordSourceIndex, L"__NOUN", L"2")) != -1)
		{
			if (source.m[wordSourceIndex].pma.queryPattern(L"__ADJECTIVE") != -1)
			{
				errorMap[L"diff: ST says adjective and LP says noun in an __ADJECTIVE construction"]++;
				return 0;
			}
			for (; pemaPosition != -1; pemaPosition = source.pema[pemaPosition].nextByPosition)
				if (patterns[source.pema[pemaPosition].getPattern()]->name == L"__NOUN" && patterns[source.pema[pemaPosition].getPattern()]->differentiator == L"2")
				{
					if (!source.pema[pemaPosition].isChildPattern() && source.m[wordSourceIndex].getFormNum(source.pema[pemaPosition].getChildForm()) == nounForm)
					{
						errorMap[L"diff: ST says adjective and LP says noun in an __NOUN(n) construction"]++;
						return 0;
					}
				}
		}
		if (source.queryPattern(wordSourceIndex, L"__ADJECTIVE") != -1)
		{
			if (WordClass::isDash(source.m[wordSourceIndex + 1].word->first[0]))
			{
				errorMap[L"diff: ST says adjective and LP says noun in an __ADJECTIVE construction"]++;
				return 0;
			}
		}
	}
	if ((wordSourceIndex>0 && (source.m[wordSourceIndex - 1].queryWinnerForm(L"modal_auxiliary") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"future_modal_auxiliary") >= 0 ||
		source.m[wordSourceIndex - 1].queryWinnerForm(L"negation_modal_auxiliary") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"negation_future_modal_auxiliary") >= 0)) &&
		wordSourceIndex+1<source.m.size() && source.m[wordSourceIndex + 1].queryWinnerForm(L"verb") >= 0 && word == L"better" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		errorMap[L"LP correct: LP says adverb ST says "+ primarySTLPMatch]++;
		return 0;
	}
	// 100 examples checked - no errors
	if (primarySTLPMatch == L"preposition or conjunction" && wordAfterIsDeterminer && (source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"particle") >= 0))
	{
		errorMap[L"ST correct: LP says adverb or particle when ST says " + primarySTLPMatch]++;
		return 0;
	}
	// verb *ADJ* (relativizer OR preposition OR coordinator or ,)
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && wordSourceIndex + 1 < source.m.size() && wordSourceIndex > 3)
	{
		if (wordAfterIsUnmodifiable)
		{
			if ((wordBeforeIsVerb && wordBeforeIsIs) || (word2BeforeIsVerb && word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0))
			{
				errorMap[L"ST correct: LP says adverb but ST says adjective, IS following a verb and followed by a relativizer, preposition or coordinator"]++;
				return 0;
			}
			if ((wordBeforeIsVerb && !wordBeforeIsIs) || (word2BeforeIsVerb && !word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0))
			{
				errorMap[L"LP correct: LP says adverb but ST says adjective, following a verb and followed by a relativizer, preposition or coordinator"]++;
				return 0;
			}
		}
		{
			// an *even* and noiseless step - 0 matches!
			//if (wordBeforeIsDeterminer && source.m[wordSourceIndex + 1].queryWinnerForm(L"coordinator") >= 0 && source.m[wordSourceIndex + 2].queryWinnerForm(L"adjective") >= 0 && source.m[wordSourceIndex + 3].queryWinnerForm(L"noun") >= 0)
			//{
			//	partofspeech += L"***ADJADV3";
			//	//errorMap[L"ST correct: LP says adverb but ST says adjective"]++;
			//	//return 0;
			//}
			// Still sipping his coffee , he regarded her with the blithe humour which lent so *great* a charm to his expression . 
			// The game was as old as the Garden of Eden, she had played it well or *ill* from her cradle, and at last she had begun to grow a trifle weary .

			// how *much* trouble
			if ((wordBeforeIsDeterminer || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"preposition") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(L"conjunction") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"coordinator") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"adjective") >= 0 || 
				source.queryPattern(wordSourceIndex - 1, L"__ADJECTIVE") != -1 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(L"relativizer") >= 0 || 
				!iswalpha(source.m[wordSourceIndex - 1].word->first[0])) &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"timeUnit") >= 0))
			{
				errorMap[L"ST correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			else if ((wordBeforeIsDeterminer || source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0) && (source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0 || source.queryPattern(wordSourceIndex + 1, L"__ADJECTIVE") != -1 || source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") >= 0))
			{
				errorMap[L"LP correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			else if (wordBeforeIsDeterminer && source.m[wordSourceIndex + 1].queryWinnerForm(L"quantifier") >= 0 && source.queryPattern(wordSourceIndex + 1, L"_TIME") != -1)
			{
				errorMap[L"LP correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			// much better looking - true except for IS verbs (only 58 matches)
			else if (source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") >= 0 && source.m[wordSourceIndex + 1].hasWinnerVerbForm())
			{
				//partofspeech += L"***ADJADV6";
				//errorMap[L"LP correct: LP says adverb but ST says adjective (5)"]++;
				//return 0;
			}
			// correct except for the rare IS verb or a noun restatement (you measly scrub!)
			else if ((wordBeforeIsPronoun || source.m[wordSourceIndex - 1].queryWinnerForm(L"Proper Noun") >= 0) && source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") < 0)
			{
				//partofspeech += L"***ADJADV7";
				errorMap[L"LP correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			if (source.queryPattern(wordSourceIndex, L"__ADVERB") != -1 && source.queryPattern(wordSourceIndex, L"__CLOSING__S1") != -1)
			{
				enum ADVCL {ALWAYS_ADJECTIVE, ALWAYS_ADVERB, UNKNOWN};
				map <wstring,ADVCL> closingmap = { 
					{L"above",ALWAYS_ADJECTIVE },
					{L"asleep",ALWAYS_ADJECTIVE },
					{L"enough",ALWAYS_ADVERB },
					{L"much",ALWAYS_ADVERB },
					{L"pretty",ALWAYS_ADJECTIVE },
					{L"right",ALWAYS_ADJECTIVE },
					{L"more",ALWAYS_ADJECTIVE },
					{L"less",ALWAYS_ADJECTIVE }
				};
				auto cm = closingmap.find(word);
				if (cm != closingmap.end())
				{
					if (cm->second == ALWAYS_ADJECTIVE)
						errorMap[L"ST correct: LP says adverb but ST says adjective"]++;
					else
						errorMap[L"LP correct: LP says adverb but ST says adjective"]++;
					return 0;
				}
				else
					partofspeech += L"***CLOSINGISADV(2)";
			}
			if (source.queryPattern(wordSourceIndex, L"__ADVERB") != -1 && source.queryPattern(wordSourceIndex, L"__INTRO_N") != -1)
			{
				partofspeech += L"***INTROISADV(2)";
			}
			else if (word == L"much")
			{
				// much money / much Nature / much of the road / how much he had gotten / he isn't much.
				if (source.m[wordSourceIndex + 1].queryWinnerForm(L"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0 ||
					source.m[wordSourceIndex + 1].word->first == L"of" || 
					source.m[wordSourceIndex - 1].queryWinnerForm(L"is") >= 0)
				{
					errorMap[L"ST correct 'much': LP says adverb but ST says adjective"]++;
					return 0;
				}
				else
				{
					errorMap[L"LP correct 'much': LP says adverb but ST says adjective"]++; // this includes the 'how much' case - may investigate this as how much does not act like adverb nor adjective
					return 0;
				}
			}
			else if (word == L"enough")
			{
				// much money / much Nature / much of the road / how much he had gotten / he isn't much.
				if (source.m[wordSourceIndex + 1].queryWinnerForm(L"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"noun") >= 0 ||
					source.m[wordSourceIndex + 1].queryWinnerForm(L"indefinite_pronoun") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"indefinite_pronoun") >= 0 ||
					source.m[wordSourceIndex + 1].word->first == L"of" ||
					source.m[wordSourceIndex - 1].queryWinnerForm(L"is") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"is_negation") >= 0 ||
					source.m[wordSourceIndex - 2].queryWinnerForm(L"is") >= 0 || source.m[wordSourceIndex - 2].queryWinnerForm(L"is_negation") >= 0)
				{
					errorMap[L"ST correct 'enough': LP says adverb but ST says adjective"]++;
					return 0;
				}
				else
				{
					//partofspeech += L"***ENOUGHISNOTADJECTIVE";
					errorMap[L"LP correct 'enough': LP says adverb but ST says adjective"]++; // this includes the 'how much' case - may investigate this as how much does not act like adverb nor adjective
					return 0;
				}
			}
		}
		int maxlen = -1;
		if ((source.queryPattern(wordSourceIndex - 1, L"_BE", maxlen) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(L"is") >= 0) &&
			source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(L"verb") < 0)
		{
			errorMap[L"LP correct: ST says adverb but LP says adjective, following a being verb"]++;
			return 0;
		}
		else if (source.m[wordSourceIndex + 1].word->first == L"-")
		{
			if (source.m[wordSourceIndex + 2].queryWinnerForm(L"noun") >= 0 && source.queryPattern(wordSourceIndex + 2, L"__ADJECTIVE") == -1)
			{
				errorMap[L"ST correct: ST says adjective but LP says adverb with dash and then a noun"]++;
				return 0;
			}
			else
			{
				errorMap[L"LP correct: ST says adjective but LP says adverb with dash and then a non-noun"]++;
				return 0;
			}
		}
	}
	if (word == L"more")
	{
		// more money / more Nature / more of the road / how much more he had gotten / he isn't more wealthy.
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(L"indefinite_pronoun") >= 0 ||
			(source.m[wordSourceIndex + 1].word->first == L"of" && source.m[wordSourceIndex - 1].word->first != L"no"))
		{
			//partofspeech += L"***MOREISADJECTIVEQUANTIFIER";
			if (source.m[wordSourceIndex ].queryWinnerForm(L"quantifier") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0)
				errorMap[L"LP correct 'more': LP says quantifier/adjective but ST says "+ primarySTLPMatch]++;
			else
				errorMap[L"ST correct 'more': ST says "+ primarySTLPMatch]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"verb") >= 0) && source.m[wordSourceIndex].queryWinnerForm(L"quantifier") >= 0 && primarySTLPMatch == L"adverb")
		{
			//partofspeech += L"***MOREISADVERB";
			errorMap[L"ST correct 'more': LP says quantifier but ST says adverb"]++; // this includes the 'how much' case - may investigate this as how much does not act like adverb nor adjective
			return 0;
		}
		// no more!
		else if (source.m[wordSourceIndex - 1].word->first == L"no")
		{
			errorMap[L"diff: LP says quantifier but ST says adverb"]++; 
			return 0;
		}
		// more or less responsible is not included!
		else if ((source.m[wordSourceIndex - 1].queryWinnerForm(L"is") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(L"is_negation") >= 0) && wordAfterIsUnmodifiable && source.m[wordSourceIndex + 1].word->first != L"or")
		{
			errorMap[L"LP correct 'more': LP says quantifier but ST says " + primarySTLPMatch]++;
			return 0;
			//partofspeech += L"***MOREISADJECTIVE()";
		}
		else
			partofspeech += L"***MOREIS()";

	}
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && 
		source.m[wordSourceIndex + 1].queryWinnerForm(L"coordinator")==-1 && source.m[wordSourceIndex + 1].word->first!=L"," &&
		!WordClass::isDash(source.m[wordSourceIndex + 1].word->first[0]) && !WordClass::isDoubleQuote(source.m[wordSourceIndex + 1].word->first[0]) && !WordClass::isSingleQuote(source.m[wordSourceIndex + 1].word->first[0]))
	{
		bool wordAfterIsVeryUnmodifiable = wordAfterIsUnmodifiable && source.m[wordSourceIndex + 1].queryWinnerForm(L"adverb") == -1 && source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") == -1;
		int pemaOffset=source.queryPatternDiff(wordSourceIndex, L"__NOUN",L"2");
		if (pemaOffset >= 0 && source.pema[pemaOffset].end >= 2)
			partofspeech += L"***ADJNOUNDET1";
		else if (wordBeforeIsDeterminer && source.m[wordSourceIndex + 1].queryForm(L"noun") == -1)
		{
			errorMap[L"LP correct: LP says noun but ST says " + primarySTLPMatch]++;
			return 0;
		}
		else if (source.m[wordSourceIndex - 1].queryWinnerForm(L"preposition") != -1 && wordAfterIsVeryUnmodifiable && source.m[wordSourceIndex - 1].word->first != L"than" && source.m[wordSourceIndex - 1].word->first != L"as")
		{
			errorMap[L"LP correct: LP says noun but ST says " + primarySTLPMatch]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex - 1].queryWinnerForm(L"is") != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(L"is_negation") != -1) && wordAfterIsVeryUnmodifiable &&
			(source.m[wordSourceIndex].word->second.inflectionFlags&PLURAL) != PLURAL)
		{
			errorMap[L"ST correct: LP says noun but ST says adjective (after is, before unmodifiable)"]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex - 2].queryWinnerForm(L"is") != -1 || source.m[wordSourceIndex - 2].queryWinnerForm(L"is_negation") != -1) && wordAfterIsVeryUnmodifiable && source.m[wordSourceIndex - 1].queryWinnerForm(L"adverb") != -1 &&
			(source.m[wordSourceIndex].word->second.inflectionFlags&PLURAL) != PLURAL)
		{
			errorMap[L"ST correct: LP says noun but ST says adjective (after is, before unmodifiable)"]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex].word->second.inflectionFlags&(SINGULAR|PLURAL)) == PLURAL)
		{
			errorMap[L"LP correct: LP says noun but ST says adjective (plural only)"]++;
			return 0;
		}
		else if (source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) >=4)
			partofspeech += L"***ADJNOUNDET2";
		else
			partofspeech += L"***ADJNOUNDET3";
		//errorMap[L"LP correct: adverb of customary form (ending in -ly) ST says " + primarySTLPMatch + L" but LP says adverb"]++;
		//return 0;
	}
	// over 100 examples checked and 99% correct except for 'only'
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && word.length() > 3 && word.substr(word.length() - 2) == L"ly" && word != L"only")
	{
		errorMap[L"LP correct: adverb of customary form (ending in -ly) ST says " + primarySTLPMatch + L" but LP says adverb"]++;
		return 0;
	}
	// POS JJ (adjective) not found in winnerForms verb for word annoyed 0006301:[Miss Farrar now was more than bored , she was *annoyed* . ]
	// in the future may attempt to correct ishas constuction which is actually ownership
	int maxEnd = -1;
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && source.queryPattern(wordSourceIndex, L"_VERBPASSIVE", maxEnd) != -1)
	{
		errorMap[L"ST correct: ST says " + primarySTLPMatch + L" but LP says a passive construction (may be classified as diff in future)"]++;
		return 0;
		//partofspeech += L"***VERBPASSIVE";
	}
	maxEnd = -1;
	if (primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0 && source.queryPattern(wordSourceIndex, L"__AS_AS", maxEnd) != -1)
	{
		errorMap[L"diff: ST says " + primarySTLPMatch + L" but LP says adjective embedded in an adverbial construction"]++;
		return 0;
		//partofspeech += L"***__AS_AS";
	}
	// Stanford POS JJ***SPadjective(adjective) not found in winnerForms verb for word bellowing
	maxEnd = -1;
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && source.queryPattern(wordSourceIndex, L"__ADJECTIVE", maxEnd) != -1)
	{
		errorMap[L"diff: ST says " + primarySTLPMatch + L" but LP says verb embedded in an adjectival construction"]++;
		return 0;
		//partofspeech += L"***__ADJECTIVE";
	}
	if ((primarySTLPMatch == L"noun" || primarySTLPMatch == L"verb") && source.m[wordSourceIndex].queryWinnerForm(L"honorific") >= 0)
	{
		if (primarySTLPMatch == L"noun")
			errorMap[L"diff: word '"+word+L"': ST says " + primarySTLPMatch + L" but LP says honorific"]++;
		if (primarySTLPMatch == L"verb")
			errorMap[L"LP correct: word '"+word+L"': ST says verb but LP says honorific"]++;
		return 0;
	}
	if (word == L"his" && primarySTLPMatch == L"possessive_determiner" && source.m[wordSourceIndex].queryWinnerForm(L"possessive_pronoun") >= 0 &&
		(source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) >= 0 || !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) ||
			source.m[wordSourceIndex + 1].queryWinnerForm(conjunctionForm) >= 0 || source.m[wordSourceIndex + 1].word->second.hasVerbForm()))
	{
		errorMap[L"LP correct: word 'his': ST says " + primarySTLPMatch + L" but LP says possessive_pronoun"]++;
		return 0;
	}
	if (word == L"plenty" && primarySTLPMatch == L"adverb" && (source.m[wordSourceIndex].queryWinnerForm(L"quantifier") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0) &&
		source.m[wordSourceIndex + 1].word->first == L"of")
	{
		errorMap[L"LP correct: word 'plenty': ST says " + primarySTLPMatch + L" but LP says quantifier"]++;
		return 0;
	}
	if (word == L"little" && source.m[wordSourceIndex - 1].word->first == L"a" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		errorMap[L"LP correct: word 'a little': ST says " + primarySTLPMatch + L" but LP says adverb"]++;
		return 0;
	}
	if (primarySTLPMatch == L"personal_pronoun_accusative")
	{
		vector<wstring> pronounTypes = { L"personal_pronoun_accusative",L"personal_pronoun_nominative",L"personal_pronoun",L"reflexive_pronoun" };
		bool pnMatch = false;
		for (wstring pn : pronounTypes)
			if (pnMatch = source.m[wordSourceIndex].queryWinnerForm(pn) >= 0)
				break;
		if (!pnMatch)
			partofspeech += L"***PN";
		//errorMap[L"LP correct: ST says interjection when no interjection form possible)"]++;
		//return 0;
	}
	if (primarySTLPMatch == L"possessive_determiner")
	{
		vector<wstring> pronounTypes = { L"possessive_determiner" };
		bool pnMatch = false;
		for (wstring pn : pronounTypes)
			if (pnMatch = source.m[wordSourceIndex].queryWinnerForm(pn) >= 0)
				break;
		if (!pnMatch)
			partofspeech += L"***PNPDET";
		//errorMap[L"LP correct: ST says interjection when no interjection form possible)"]++;
		//return 0;
	}
	// 55. this is correct 100% of the time 
	if (source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE && source.m[wordSourceIndex].pma.queryPattern(L"_ADJECTIVE") != -1)
	{
		errorMap[L"diff: ST says adjective when LP says it is a present participle, matched to __ADJECTIVE pattern [acceptable]"]++;
		return 0; // ST and LP agree
	}
	// 56. incorrect 3 times out of 141 instances
	if (primarySTLPMatch == L"verb" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE && source.m[wordSourceIndex].pma.queryPattern(L"__N1") != -1)
	{
		errorMap[L"diff: ST says verb when LP says it is a noun but matching to a present participle and an __N1 pattern [acceptable]"]++;
		return 0; // ST and LP agree
	}
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0)
	{
		if ((source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE && source.m[wordSourceIndex].pma.queryPattern(L"__N1") != -1)
		{
			errorMap[L"diff: ST says noun when LP says it is a verb but matching to a present participle and an __N1 pattern [acceptable]"]++;
			return 0; // ST and LP agree
		}
if (wordSourceIndex >= 1 && source.m[wordSourceIndex - 1].word->first == L"to")
{
	errorMap[L"LP correct: ST says noun when LP says it is a verb but before 'to'"]++;
	return 0;
}
	}
	// Rollo met the policeman *walking* towards him
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE &&
		source.m[wordSourceIndex].pma.queryPattern(L"_VERBONGOING") != -1 &&
		(source.queryPatternDiff(wordSourceIndex, L"__NOUN", L"F") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(L"__NOUN", L"D") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(L"_PP", L"3") != -1))
	{
		errorMap[L"diff: ST says noun when LP says it is a verb but matching to a present participle and an _NOUN[F], _NOUN[D] or _PP[3] pattern [acceptable]"]++;
		return 0; // ST and LP agree
	}
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE)
	{
		/*
		int maxLen=-1,pemaIndex=-1;
		if (source.m[wordSourceIndex].pma.queryPattern(L"__N1") != -1 && (pemaIndex = source.queryPattern(wordSourceIndex, L"__NOUN", maxLen)) != -1)
		{
			if (source.pema[pemaIndex].end > 1)
			{
				errorMap[L"diff: ST says adjective when LP says it is a verb but matching to a present participle and an __N1 inside of a __NOUN pattern [acceptable]"]++;
				partofspeech += L"***SPADJVERB";
			}
			else
			{
				errorMap[L"LP correct: ST says adjective when LP says it is a verb but matching to a present participle and an __N1 as the front of a __NOUN pattern"]++;
				partofspeech += L"***SPADJVERBNOUN";
			}
		}
		*/
		int maxLen = -1, pemaIndex;
		// It will be *surprising*
		if ((pemaIndex = source.queryPattern(wordSourceIndex, L"_VERB", maxLen)) != -1)
		{
			int verbBegin = source.pema[pemaIndex].begin + wordSourceIndex;
			// check for an 'is' or 'has' verb
			for (int wsi = wordSourceIndex - 1; wsi >= verbBegin; wsi--)
			{
				if (source.m[wsi].pma.queryPattern(L"_IS") != -1 || source.m[wsi].pma.queryPattern(L"_HAVE") != -1 || source.m[wsi].pma.queryPattern(L"_BE") != -1)
				{
					errorMap[L"ST correct: present participle after 'is' or 'has' ST says adjective LP says verb"]++;
					return 0;
				}
			}
		}
	}
	// checked with 100 examples and 1 was incorrect (misparse)
	if ((primarySTLPMatch == L"noun" || primarySTLPMatch == L"adjective") && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0 && word.length() > 3 && word.substr(word.length() - 3) == L"ing")
	{
		if (primarySTLPMatch == L"noun" && (source.queryPattern(wordSourceIndex, L"__INFP") != -1 ||
			source.queryPatternDiff(wordSourceIndex, L"_VERB", L"4") != -1 ||
			source.queryPatternDiff(wordSourceIndex, L"_VERB", L"8") != -1 ||
			source.queryPattern(wordSourceIndex, L"_ADJECTIVE_AFTER") != -1 ||
			source.queryPatternDiff(wordSourceIndex, L"__ADJECTIVE", L"2") != -1 ||
			(source.queryPattern(wordSourceIndex, L"_VERBONGOING") != -1 && source.queryPattern(wordSourceIndex, L"__MODAUX") != -1) ||
			(source.queryPattern(wordSourceIndex, L"_VERBONGOING") != -1 && source.queryPatternDiff(wordSourceIndex, L"_VERBREL2", L"1") != -1)))
		{
			errorMap[L"LP correct: ST says noun when LP says verb participle in a structure consonant with a verb"]++;
			return 0;
		}
		//int w;
		if (primarySTLPMatch == L"adjective" && (
			source.queryPattern(wordSourceIndex, L"__INFP") != -1 ||
			source.queryPatternDiff(wordSourceIndex, L"_VERB", L"4") != -1 ||
			source.queryPatternDiff(wordSourceIndex, L"_VERB", L"8") != -1 ||
			source.queryPatternDiff(wordSourceIndex, L"__NOUN", L"D") != -1 ||
			//(w=source.queryPattern(wordSourceIndex, L"__NOUN", L"2")) != -1 && source.pema[w].end==1) ||
			(source.queryPattern(wordSourceIndex, L"_VERBONGOING") != -1 && source.queryPattern(wordSourceIndex, L"__MODAUX") != -1) ||
			(source.queryPattern(wordSourceIndex, L"_VERBONGOING") != -1 && source.queryPatternDiff(wordSourceIndex, L"_VERBREL2", L"1") != -1)))
		{
			errorMap[L"LP correct: ST says adjective when LP says verb participle in a structure consonant with a verb"]++;
			return 0;
		}
		if (primarySTLPMatch == L"noun" && source.queryPatternDiff(wordSourceIndex, L"__NOUN", L"D") != -1)
		{
			errorMap[L"diff: ST says noun and LP says verb in a noun structure"]++;
			return 0;
		}
		if (primarySTLPMatch == L"adjective" && (source.queryPattern(wordSourceIndex, L"_ADJECTIVE_AFTER") != -1 ||
			source.queryPatternDiff(wordSourceIndex, L"__ADJECTIVE", L"2") != -1))
		{
			errorMap[L"diff: ST says adjective and LP says verb in a adjective structure"]++;
			return 0;
		}
		//partofspeech += L"***NAVING";
	}
	if (primarySTLPMatch == L"verb" && source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0 &&
		source.m[wordSourceIndex].queryForm(L"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PAST) == VERB_PAST &&
		source.queryPatternDiff(wordSourceIndex, L"__S1", L"7") != -1)
	{
		errorMap[L"ST correct: ST says verb and LP says adjective"]++;
		return 0;
	}
	if (primarySTLPMatch == L"verb" && source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0)
	{
		wstring nounCost, verbCost;
		itos(source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)), nounCost);
		itos(source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)), verbCost);
		// 115 out of 116 correct
		if (nounCost == L"0" && (verbCost == L"4" || verbCost == L"3" || verbCost == L"2") && partofspeech == L"VBN")
		{
			errorMap[L"LP correct: ST says verb VBN and LP says noun"]++;
			return 0;
		}
		// all of 183 examples
		if (nounCost == L"0" && (verbCost == L"4" || verbCost == L"3" || verbCost == L"2") && source.m[wordSourceIndex - 1].word->first == L"-") // not double dash!
		{
			if (word.length() > 3 && word.substr(word.length() - 3) == L"ing" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE &&
				source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0)
			{
				errorMap[L"diff: ST says verb and LP says noun - after -, and using ing (really adjective)"]++;
			}
			else
				errorMap[L"LP correct: ST says verb and LP says noun - after -"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"verb") >= 0)
		{
		wstring verbCost;
		itos(source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)), verbCost);
		int pemaOffset = source.queryPattern(wordSourceIndex, L"__NOUN");
		if (source.m[wordSourceIndex].isOnlyWinner(verbForm) && verbCost==L"0")
		{
			// letting her hands *fall*
			if (pemaOffset >= 0 && source.pema[pemaOffset].end == 1 && patterns[source.pema[pemaOffset].getPattern()]->differentiator==L"6")
			{
				errorMap[L"diff: LP says verb in head part of NOUN struct and ST says noun"]++;
				return 0;
			}
			// 3 out of 108 incorrect because the parse was wrong
			// for a *split* second .
			if (pemaOffset >= 0 && source.pema[pemaOffset].end > 1)
			{
				errorMap[L"LP correct: LP says verb in adjective part of NOUN struct and ST says noun"]++;
				return 0;
			}
		}
	}
	// this is correct exceot for rare parse structure: I would have done it *again* here had I thought you were coming to try to win her heart
	if (source.m[wordSourceIndex].queryWinnerForm(L"conjunction") >= 0 && source.m[wordSourceIndex + 1].pma.queryPattern(L"__S1") != -1)
	{
		errorMap[L"LP correct: ST says " + primarySTLPMatch + L" and LP says conjunction"]++;
		return 0;
	}
	if ((word == L"upstairs" || word == L"downstairs") && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && primarySTLPMatch == L"noun")
	{
		errorMap[L"LP correct: word 'upstairs' or 'downstairs': ST says noun LP says adverb"]++;
		return 0;
	}
	if (word == L"yer" || word == L"youse" || word == L"em" || word == L"ourselves")
	{
		errorMap[L"LP correct '"+word+L"': incorrect noun usage"]++;
		return 0;
	}
	if (partofspeech == L"VBG" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) != VERB_PRESENT_PARTICIPLE)
	{
		if (word == L"bleeding")
		{
			errorMap[L"LP correct '" + word + L"': incorrect verb usage"]++;
			return 0;
		}
	}
	if (partofspeech == L"NN" && (source.m[wordSourceIndex].word->second.inflectionFlags&SINGULAR) != SINGULAR)
	{
		if (source.m[wordSourceIndex].queryWinnerForm(L"interjection") >= 0 && primarySTLPMatch == L"noun")
		{
			errorMap[L"LP correct: ST says noun LP says interjection"]++;
			return 0;
		}
	}
	if (partofspeech == L"NNS" && (source.m[wordSourceIndex].word->second.inflectionFlags&PLURAL) != PLURAL)
	{
		if (word == L"semi" || word == L"but" || word == L"oh" || word == L"hey" || word == L"yes" || source.m[wordSourceIndex].queryWinnerForm(L"reflexive_pronoun") >= 0)
		{
			errorMap[L"LP correct '" + word + L"': incorrect noun usage"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0 && (word==L"half" || word==L"round"))
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(L"adjective") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(L"verb") >= 0 ||
			(source.m[wordSourceIndex + 1].queryForm(dashForm) != -1 && source.m[wordSourceIndex + 2].queryWinnerForm(L"adjective") >= 0))
		{
			errorMap[L"LP correct '" + word + L"': adverb not noun"]++;
			return 0;
		}
	}
	if ((source.m[wordSourceIndex].flags&WordMatch::flagFirstLetterCapitalized) && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) && source.m[wordSourceIndex].queryWinnerForm(L"interjection") >= 0)
	{
		errorMap[L"LP correct '" + word + L"': interjection not "+ primarySTLPMatch]++;
		return 0;
	}
	if ((source.queryPatternDiff(wordSourceIndex,L"__INTRO_N", L"C") != -1 || source.queryPatternDiff(wordSourceIndex,L"_ADVERB", L"T") != -1) && 
		  (source.m[wordSourceIndex].queryWinnerForm(L"dayUnit") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"timeUnit") >= 0 || source.m[wordSourceIndex].queryWinnerForm(L"uncertainDurationUnit") >= 0))
	{
		if (primarySTLPMatch ==L"adverb")
			errorMap[L"diff: TIME (adverb)"]++;
		else
			errorMap[L"LP correct: adverb not " + primarySTLPMatch]++;
		return 0;
	}
	if (word == L"on board")
	{
		errorMap[L"diff: on board (adverb)"]++;
		return 0;
	}
	if ((word == L"to-day" || word == L"to-morrow") && (primarySTLPMatch == L"noun" || primarySTLPMatch == L"adjective") && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(L"preposition") >= 0 && primarySTLPMatch == L"noun"))
			errorMap[L"ST correct: 'to-day' or 'to-morrow' after preposition must be a noun"]++;
		else if ((source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") >= 0 && primarySTLPMatch == L"adjective"))
			errorMap[L"ST correct: 'to-day' or 'to-morrow' before noun must be an adjective"]++;
		else
		{
			errorMap[L"LP correct: 'to-day' or 'to-morrow' is in general an adverb of time"]++;
		}
		return 0;
	}
	// 102 examples checked, 101 correct.
	if (word==L"after" && primarySTLPMatch == L"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0)
	{
		errorMap[L"LP correct: 'after' can be an adjective"]++;
		return 0;
	}
	if (word==L"doubt")
	{
		if (source.queryPatternDiff(wordSourceIndex, L"__INTRO_N", L"ID") != -1)
		{
			errorMap[L"LP correct: 'doubt' is a verb in 'I doubt if'"]++;
			return 0;
		}
		if (source.queryPatternDiff(wordSourceIndex, L"__ADVERB", L"ND") != -1)
		{
			errorMap[L"LP correct: 'doubt' is a noun in 'no doubt'"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].isOnlyWinner(nounForm) &&
		  source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(L"adjective")) == 4 &&
		  source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(L"noun")) == 0)
	{
		errorMap[L"LP correct: noun more probable than adjective"]++; // probabilistic - see distribute errors (19 out of 119 LP correct)
		return 0;
	}
	if (word == L"my" && source.m[wordSourceIndex].queryWinnerForm(L"possessive_determiner") >= 0)
	{
		errorMap[L"LP correct: 'my' is possessive_determiner (now that interjection has been added as a form)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == L"verb" && source.m[wordSourceIndex].queryWinnerForm(L"adjective") >= 0)
	{
		errorMap[L"LP correct: adjective not verb"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].isOnlyWinner(adverbForm) && source.m[wordSourceIndex-1].queryWinnerForm(L"preposition") < 0)
	{
		errorMap[L"LP correct: adverb not noun"]++; // probabilistic - see distribute errors
		return 0;
	}
	int pemaOffset = -1;
	if (wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(L"quantifier") >= 0 && (pemaOffset=source.queryPatternDiff(wordSourceIndex, L"__NOUN", L"2")) != -1)
	{
		if (source.pema[pemaOffset].end !=1)
		{
			partofspeech += L"**ISQUANTIFIERADJECTIVE?";
			//errorMap[L"LP correct: word 'more': ST says " + primarySTLPMatch + L" LP says quantifier"]++;
			//return 0;
		}
	}
	if (source.queryPatternDiff(wordSourceIndex, L"_ADVERB", L"8") != -1)
	{
		errorMap[L"LP correct:little by little"]++; 
		return 0;
	}
	// *time* to time OR time to *time* / *face* to face
	if (source.m[wordSourceIndex].queryWinnerForm(L"noun") >= 0 && ((wordSourceIndex > 2 && source.m[wordSourceIndex - 1].word->first == L"to" && source.m[wordSourceIndex].word == source.m[wordSourceIndex - 2].word) ||
		(wordSourceIndex < source.m.size() - 1 && source.m[wordSourceIndex + 1].word->first == L"to" && source.m[wordSourceIndex].word == source.m[wordSourceIndex + 2].word)))
	{
		errorMap[L"LP correct:little by little"]++;
		return 0;
	}
	// from *head* to foot OR from head to *foot*
	if ((word == L"foot" && wordSourceIndex>3 && source.m[wordSourceIndex - 3].word->first == L"from" && source.m[wordSourceIndex - 2].word->first == L"head" &&source.m[wordSourceIndex - 1].word->first == L"to") ||
		  (word == L"head" && wordSourceIndex > 1 && source.m[wordSourceIndex - 1].word->first == L"from" && source.m[wordSourceIndex + 1].word->first == L"to" &&source.m[wordSourceIndex + 2].word->first == L"foot"))
	{
		errorMap[L"LP correct:from head to foot"]++;
		return 0;
	}

	if (word == L"hers" && primarySTLPMatch==L"noun" && source.m[wordSourceIndex].queryWinnerForm(L"pronoun") >= 0)
	{
		errorMap[L"LP correct:hers is better considered a pronoun/possessive, not a noun"]++;
		return 0;
	}
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 4 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(adjectiveForm)) == 0)
	{
		errorMap[L"LP correct: adjective not noun (noun cost 4, adjective cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == L"verb" && source.m[wordSourceIndex].isOnlyWinner(nounForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 4 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 0)
	{
		errorMap[L"LP correct: noun not verb (verb cost 4, noun cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if ((word == L"in" || word == L"since" || word==L"beyond") && primarySTLPMatch == L"preposition or conjunction" && (!iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || source.m[wordSourceIndex+1].isOnlyWinner(coordinatorForm)))
	{
		errorMap[L"LP correct: not preposition or conjunction before punctuation or coordinator"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (word == L"in" && primarySTLPMatch == L"preposition or conjunction" && source.m[wordSourceIndex].isOnlyWinner(adverbForm) &&
		(source.m[wordSourceIndex + 1].word->first == L"there" || source.m[wordSourceIndex + 1].word->first == L"silence"))
	{
		errorMap[L"ST correct: in is a preposition before 'silence' or 'there'"]++;
		return 0;
	}
	if (word == L"home" && primarySTLPMatch == L"adverb" && source.m[wordSourceIndex].isOnlyWinner(nounForm))
	{
		errorMap[L"ST correct: home is an adverb when having no determiner (go home)"]++;
		return 0;
	}
	// all 32 examples are correct.
	if (word == L"fool" && source.m[wordSourceIndex].isOnlyWinner(nounForm))
	{
		errorMap[L"ST correct: fool is a noun (you fool!)"]++;
		return 0;
	}
	// 1 out of 109 examples was wrong (LP did not select the correct form)
	if (wordSourceIndex>0 && source.m[wordSourceIndex-1].word->first==L"wouldhad")
	{
		errorMap[L"diff: wouldhad is an LP construction, so Stanford will not do this correctly."]++;
		return 0;
	}
	// (noun) not found in winnerForms is for word ishas
	if (word == L"ishas" || word == L"wouldhad" || word == L"ishasdoes")
	{
		errorMap[L"diff: ishas/wouldhad/ishasdoes is a special word."]++;
		return 0;
	}
	if (primarySTLPMatch != L"preposition or conjunction" && source.m[wordSourceIndex].isOnlyWinner(prepositionForm) && source.m[wordSourceIndex].getRelObject()>=0)
	{
		if (source.m[wordSourceIndex + 1].word->first == L"to")
		{
			errorMap[L"LP correct: preposition preposition (to)"]++; 
			return 0;
		}
		if (source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) >= 0)
		{
			errorMap[L"ST correct: preposition preposition (other than to)"]++;
			return 0;
		}
		if (wordSourceIndex > 0 && (WordClass::isDash(source.m[wordSourceIndex - 1].word->first[0]) || WordClass::isDash(source.m[wordSourceIndex + 1].word->first[0])))
		{
			errorMap[L"ST correct: dash preposition"]++;
			return 0;
		}
		// 22 ST/163 LP
		errorMap[L"LP correct: preposition with relative object"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (source.m[wordSourceIndex].isOnlyWinner(prepositionForm) && source.m[wordSourceIndex].getRelObject() < 0)
	{
		if (source.m[wordSourceIndex - 1].queryWinnerForm(prepositionForm) >= 0 && source.m[wordSourceIndex - 1].getRelObject() >= 0 && word != L"as" && primarySTLPMatch != L"verb" && primarySTLPMatch != L"noun")
		{
			errorMap[L"ST correct: about is an adverb when used with this construction"]++; 
			return 0;
		}
	}
	// 'all' before an _S1 is an adverb not a determiner
	// L"relativizer|when", L"conjunction|before", L"conjunction|after", L"conjunction|as", L"conjunction|since", L"conjunction|until", L"conjunction|while", L"__AS_AS", L"quantifier|all*-1"
	if (source.m[wordSourceIndex].pma.queryPatternDiff(L"_ADVERB",L"AT8") != -1 && source.m[wordSourceIndex+1].pma.queryPattern(L"__S1")!=-1)
	{
		errorMap[L"LP correct: word:"+word+L" ST says "+ primarySTLPMatch+L", LP says AT8 match"]++;
		return 0;
	}
	// |noun*3 verb*0 1stSING|
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].isOnlyWinner(verbForm) &&
		  source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 3 &&
			source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm))==0 &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR)
	{
		bool dashed = (wordSourceIndex > 0 && wordSourceIndex < source.m.size() - 1 &&
			(WordClass::isDash(source.m[wordSourceIndex - 1].word->first[0]) || WordClass::isDash(source.m[wordSourceIndex + 1].word->first[0])));
		bool previousWords = (source.m[wordSourceIndex - 2].word->first == L"a" || 
			source.m[wordSourceIndex - 1].word->first == L"that" || source.m[wordSourceIndex - 1].word->first == L"this");
		if (dashed || previousWords)
		{
			errorMap[L"ST correct: ST says noun and LP says verb (determined by dash or previous word)"]++;
			return 0;
		}
		// 73 ST/144 LP
		errorMap[L"LP correct: (noun cost 3, verb cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	// after *dark*
	if (source.m[wordSourceIndex].queryWinnerForm(L"dayUnit") >= 0 && source.m[wordSourceIndex - 1].queryWinnerForm(prepositionForm) >= 0)
	{
		errorMap[L"LP correct: dayUnit after preposition is correct"]++;
		return 0;
	}
	// to and fro
	if (word == L"fro")
	{
		errorMap[L"LP correct: if to is a preposition, fro must also be"]++;
		return 0;
	}
	if (primarySTLPMatch == L"noun" && source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 3 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(adjectiveForm)) == 0)
	{
		errorMap[L"LP correct: (noun cost 3, adjective cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (wordSourceIndex > 0 && (source.m[wordSourceIndex - 1].flags&WordMatch::flagNounOwner) && source.m[wordSourceIndex].isOnlyWinner(nounForm))
	{
		int pemaOffset = source.queryPattern(wordSourceIndex,L"__NOUN");
		if (pemaOffset >= 0 && source.pema[pemaOffset].end > 1 && primarySTLPMatch==L"verb")
		{
			errorMap[L"LP correct: ownership (verb infinitive)"]++; 
			return 0;
		}
		else
		{
			errorMap[L"LP correct: ownership of noun"]++; // probabilistic - see distribute errors ST=12 / LP=79
			return 0;
		}
	}
	if (word == L"that" && source.m[wordSourceIndex].queryWinnerForm(pronounForm) >= 0 && 
		(((source.m[wordSourceIndex].flags&WordMatch::flagInQuestion) && wordSourceIndex > 0 && (source.m[wordSourceIndex - 1].queryForm(L"is") >= 0 || source.m[wordSourceIndex - 1].queryForm(L"is_negation") >= 0)) ||
		 (!(source.m[wordSourceIndex].flags&WordMatch::flagInQuestion) && wordSourceIndex > 0 && (source.m[wordSourceIndex + 1].queryForm(L"is") >= 0 || source.m[wordSourceIndex + 1].queryForm(L"is_negation") >= 0) &&
			(!iswalpha(source.m[wordSourceIndex - 1].word->first[0]) || wordSourceIndex == startOfSentence)) ||
				!iswalpha(source.m[wordSourceIndex + 1].word->first[0])))
	{
		errorMap[L"LP correct: that after 'is' in a question OR before is not in a question is a pronoun OR before punctuation!"]++; // what is that? / “ Billy , *that* is exactly where you are wrong / “ It isn't *that* , ” she said .
		return 0;
	}
	if (word == L"that" && source.m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0 && 
		  wordSourceIndex<source.m.size()-2 && source.m[wordSourceIndex+1].queryWinnerForm(adjectiveForm) >= 0 && 
		source.m[wordSourceIndex].principalWherePosition==wordSourceIndex && // this is not acting as a determiner
		(source.m[wordSourceIndex + 2].word->first==L"." || source.m[wordSourceIndex + 2].word->first == L"," || source.m[wordSourceIndex + 2].queryWinnerForm(prepositionForm)>=0))
	{
		errorMap[L"ST correct: that before an adjective, not in an object and having passed all other tests (doesn't work for a rule)"]++; 
		return 0;
	}
	// never mind
	if (word == L"mind" && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0 && source.queryPatternDiff(wordSourceIndex,L"_COMMAND1", L"8") != -1)
	{
		errorMap[L"LP correct: never mind!"]++;
		return 0;
	}
	// parallel structures
	if (wordSourceIndex > 2 && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0 && source.m[wordSourceIndex - 1].queryWinnerForm(coordinatorForm) >= 0 && source.m[wordSourceIndex - 2].queryWinnerForm(verbForm) >= 0 &&
		(source.queryPatternDiff(wordSourceIndex, L"__INFPSUB", L"") != -1 || source.queryPatternDiff(wordSourceIndex, L"__SUB_S2", L"") != -1 || source.queryPatternDiff(wordSourceIndex, L"_VERBPASTC", L"") != -1))
	{
		partofspeech += L"PARALLELVERB";
		if (source.queryPatternDiff(wordSourceIndex, L"__INFPSUB", L"") != -1)
			partofspeech += L"INFPSUB";
		if (source.queryPatternDiff(wordSourceIndex, L"__SUB_S2", L"") != -1)
			partofspeech += L"__SUB_S2";
		if (source.queryPatternDiff(wordSourceIndex, L"_VERBPASTC", L"") != -1)
			partofspeech += L"_VERBPASTC";
	}
	if (wordSourceIndex > 2 && source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0 && source.m[wordSourceIndex - 1].queryWinnerForm(coordinatorForm) >= 0 && source.m[wordSourceIndex - 2].queryWinnerForm(nounForm) >= 0)
	{
		partofspeech += L"PARALLELNOUN";
	}
	if (word == L"kind" && source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0)
	{
		errorMap[L"ST correct: word 'kind' ST says adjective"]++;
		return 0;
	}
	if (word == L"kind" && source.m[wordSourceIndex].queryWinnerForm(adjectiveForm) >= 0)
	{
		errorMap[L"LP correct: word 'kind' ST says adjective"]++; // C 85 W 17
		return 0;
	}
	if (primarySTLPMatch == L"adjective" && source.m[wordSourceIndex].queryForm(adjectiveForm) < 0)
	{
		if (source.m[wordSourceIndex].queryWinnerForm(verbForm) < 0 || !(source.m[wordSourceIndex].word->second.inflectionFlags&(VERB_PAST | VERB_PRESENT_PARTICIPLE)))
		{
			errorMap[L"LP correct: ST says adjective (wrong)"]++; // C 299 W 30
			return 0;
		}
		// VERB_PAST after be?
		if (source.m[wordSourceIndex].isOnlyWinner(verbForm) && source.m[wordSourceIndex - 1].word->first == L"be" && (source.m[wordSourceIndex].word->second.inflectionFlags&(VERB_PAST_PARTICIPLE | VERB_PAST)) == (VERB_PAST_PARTICIPLE | VERB_PAST))
		{
			errorMap[L"LP correct: ST says adjective (wrong) when verb past participle after be"]++; 
			return 0;
		}
		partofspeech += L"NOTADJECTIVE!";
		if (source.queryPattern(wordSourceIndex, L"_Q1PASSIVE") != -1)
		{
			errorMap[L"LP correct: passive verb is not adjective"]++;
			return 0;
		}
		// not specific enough and only matched 45 
		//if (source.queryPattern(wordSourceIndex, L"__ADJECTIVE") != -1)
		//{
		//	partofspeech += L"_ADJ!";
		//	//errorMap[L"LP correct: passive verb is not adjective"]++;
		//	//return 0;
		//}
		if (source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0 && source.m[wordSourceIndex].getRelVerb() >= 0 && (source.m[source.m[wordSourceIndex].getRelVerb()].queryForm(isForm) >= 0 || source.m[source.m[wordSourceIndex].getRelVerb()].queryForm(isNegationForm) >= 0 ||
			source.m[source.m[wordSourceIndex].getRelVerb()].word->first == L"be" || source.m[source.m[wordSourceIndex].getRelVerb()].word->first == L"been"))
		{
			errorMap[L"LP correct: ST says adjective but word does not have adjective form used with IS/BE verb"]++;
			return 0;
		}
		if (!iswalpha(source.m[wordSourceIndex + 1].word->first[0]) && source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0)
		{
			errorMap[L"LP correct: LP says noun and ST says adjective but word does not have an adjective form"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].queryWinnerForm(verbverbForm) >= 0 && (source.queryPattern(wordSourceIndex, L"_VERB_BARE_INF") != -1 && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0))
		{
			errorMap[L"LP correct: helper verbs are not adjectives"]++;
			return 0;
		}
		if (source.queryPattern(wordSourceIndex, L"_VERBPAST") != -1 && source.queryPattern(wordSourceIndex, L"__S1") != -1)
		{
			int relSubject = source.m[wordSourceIndex].relSubject;
			int relObject = source.m[wordSourceIndex].getRelObject();
			if (relSubject >= 0 && relObject < 0 && source.queryPattern(relSubject, L"__INFPT") >= 0)
			{
				errorMap[L"LP correct: INFPT verb is not adjective"]++;
				return 0;
			}
			if (relObject < 0)
			{
				errorMap[L"LP correct: past of verb is not adjective"]++; // probabilistic - see distribute errors ST=21 / LP=84
				return 0;
			}
			//for (int I = 0; I < source.m.size(); I++)
			//	lplog(LOG_INFO, L"%02d:%10s:relPrep = %02d,relObject = %02d,relSubject = %02d,relVerb = %02d,relNextObject = %02d,nextCompoundPartObject = %02d,previousCompoundPartObject = %02d,relInternalVerb = %02d,relInternalObject = %02d", 
			//		I, source.m[I].word->first.c_str(),source.m[I].relPrep, source.m[I].getRelObject(), source.m[I].relSubject, source.m[I].relVerb, source.m[I].relNextObject, source.m[I].nextCompoundPartObject, source.m[I].previousCompoundPartObject, source.m[I].relInternalVerb, source.m[I].relInternalObject);
			
			//errorMap[L"LP correct: passive verb is not adjective"]++;
			//return 0;
		}
		wstring excludeForms[] = { L"be", L"been", L"conjunction", L"coordinator", L"daysofweek", L"dayunit", L"future_modal_auxiliary", L"have", L"honorific", L"indefinite_pronoun", L"interjection", L"interrogative_determiner", L"interrogative_pronoun", L"is", L"modal_auxiliary", L"never", L"not",L"personal_pronoun",L"personal_pronoun_accusative",L"possessive_determiner",L"possessive_pronoun",L"relativizer"};
		for (auto ef : excludeForms)
		{
			if (source.m[wordSourceIndex].queryWinnerForm(ef) >= 0)
			{
				errorMap[L"LP correct: ST says adjective, LP says"+ef]++;
				return 0;
			}
		}
		if (source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0)
		{
			errorMap[L"LP correct: ST says adjective, LP says noun"]++;
			return 0;
		}
	}
	if (word == L"more" || word == L"less")
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(adverbForm) >= 0)
		{
			errorMap[L"ST correct: more or less before adverb is an adverb, not a quantifier"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].word->first == L"than")
		{
			if (source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1)
			{
				errorMap[L"diff: quantifier is preferred but ST pick of adverb for more or less than is acceptable."]++;
				return 0;
			}
			if (source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1 && source.m[wordSourceIndex + 2].pma.queryPattern(L"_TIME") == -1)
			{
				errorMap[L"ST correct: more/less than - adjective is preferred if not followed by a time."]++;
				return 0;
			}
		}
		if (source.queryPattern(wordSourceIndex, L"_MLT") != -1 && source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
		{
			errorMap[L"LP correct: more or less is an adverb when used with expressions of time."]++;
			return 0;
		}
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(L"noun") >= 0 && source.m[wordSourceIndex + 1].queryWinnerForm(L"noun") < 0))
		{
			if ((source.m[wordSourceIndex - 1].queryForm(L"uncertainDurationUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(L"simultaneousUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(L"dayUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(L"timeUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(L"season") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(L"time_abbreviation") >= 0))
			{
				if (source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
				{
					errorMap[L"LP correct: more or less is an adverb when used with expressions of time."]++;
					return 0;
				}
			}
			else if (source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1)
			{
				errorMap[L"diff: more or less is an adjective BUT Stanford pegs it as an adverb and LP as a quantifier."]++;
				return 0;
			}
			else if (primarySTLPMatch == L"adjective")
			{
				errorMap[L"ST correct: more or less is an adjective after a noun and not before a noun."]++;
				return 0;
			}
		}
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(L"uncertainDurationUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(L"simultaneousUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(L"dayUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(L"timeUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(L"season") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(L"time_abbreviation") >= 0))
		{
			if (source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
			{
				errorMap[L"LP correct: more or less is an adverb when used with expressions of time."]++;
				return 0;
			}
		}
	}
	if (word == L"goodbye" || word == L"good-bye")
	{
		errorMap[L"diff: goodbye/good-bye is never an adjective."]++;
		return 0;
	}
	if (word == L"anyhow")
	{
		errorMap[L"LP correct: anyhow is always an adverb."]++;
		return 0;
	}
	if (word == L"but" && source.m[wordSourceIndex].queryWinnerForm(conjunctionForm) != -1)
	{
		errorMap[L"LP correct: but is a conjunction."]++; // 159 examples
		return 0;
	}
	if (source.m[wordSourceIndex].queryWinnerForm(reflexivePronounForm) != -1)
	{
		if (primarySTLPMatch == L"noun")
		{
			errorMap[L"diff: reflexive pronoun form can be considered a noun."]++;
			return 0;
		}
		if (primarySTLPMatch == L"adjective")
		{
			errorMap[L"diff: reflexive pronoun form cannot be considered an adjective."]++;
			return 0;
		}
	}
	if (word == L"now" && ((source.m[wordSourceIndex].relPrep >= 0 && source.queryPattern(wordSourceIndex,L"_PP") != -1) || (source.m[wordSourceIndex].flags&WordMatch::flagNounOwner)!=0))
	{
		errorMap[L"LP correct: now in a PP is a noun."]++;
		return 0;
	}
	if ((primarySTLPMatch == L"preposition or conjunction" || word==L"but") && source.m[wordSourceIndex].queryWinnerForm(L"adverb") >= 0)
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) != -1)
		{
			errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].hasWinnerVerbForm() && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) == -1)
		{
			if (word == L"as")
			{
				errorMap[L"ST correct: word 'as': ST says preposition or conjunction and LP says adverb before verb"]++;
				return 0;
			}
			// *in* - most cases, the 'object' like 'in' hand, 'in' mind, 'in' turn, 'in' answer, 'in' order, 'in' love
			errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb before verb"]++; // LP Wrong 27 / LP Correct 258  distribute errors
			return 0;
		}
		if (source.m[wordSourceIndex + 1].queryWinnerForm(nounForm)!=-1 && source.m[wordSourceIndex].isOnlyWinner(adverbForm) && !WordClass::isDash(source.m[wordSourceIndex-1].word->first[0]))
		{
			errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb before noun"]++; // ST Wrong 6 / ST Correct 171  distribute errors
			return 0;
		}
		if (source.m[wordSourceIndex].pma.queryPattern(L"_INFP") != -1)
		{
			errorMap[L"ST correct: ST says preposition or conjunction and LP says adverb before INFP"]++; 
			return 0;
		}
		if (atStart || source.m[wordSourceIndex + 1].word->first == L"." || source.m[wordSourceIndex + 1].word->first == L"!" || source.m[wordSourceIndex + 1].word->first == L"?" || source.m[wordSourceIndex + 1].word->first == L";")
		{
			errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb as first or last word (next word may be 'there' or 'then' which may considered adverbs of time or place)"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].word->first == L"," || source.m[wordSourceIndex + 1].queryWinnerForm(conjunctionForm) != -1 || source.m[wordSourceIndex + 1].queryWinnerForm(coordinatorForm) != -1)
		{
			if (word == L"but")
			{
				errorMap[L"ST correct: but before a conjunction"]++;
				return 0;
			}
			if (word == L"as")
			{
				errorMap[L"ST correct: as before a conjunction or a comma"]++;
				return 0;
			}
			errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb before comma or conjunction/coordinator"]++;
			return 0;
		}
		int timePEMAOffset = -1;
		if ((timePEMAOffset = source.queryPattern(wordSourceIndex, L"_TIME")) != -1 && source.pema[timePEMAOffset].end == 1 && wordSourceIndex+ source.pema[timePEMAOffset].begin==startOfSentence)
		{
			errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb in TIME expression"]++;
			return 0;
		}
		if (wordSourceIndex != startOfSentence && wordSourceIndex > 0 && WordClass::isDash(source.m[wordSourceIndex - 1].word->first[0]) && source.m[wordSourceIndex - 1].word->first.length() == 1)
		{
			errorMap[L"diff: ST says preposition or conjunction and LP says adverb after a single dash - actually an adjective"]++;
			return 0;
		}
	}
	wstring winnerFormsString;
	source.m[wordSourceIndex].winnerFormString(winnerFormsString, false);
	// matrix analysis
	// combo list - primarySTLPMatch comes first!
	formMatrixTest(source, wordSourceIndex, primarySTLPMatch, winnerFormsString, 0, 4, comboCostFrequency, partofspeech);
	formMatrixTest(source, wordSourceIndex, primarySTLPMatch, winnerFormsString, 0, 3, comboCostFrequency, partofspeech);
	formMatrixTest(source, wordSourceIndex, primarySTLPMatch, winnerFormsString, 4, 0, comboCostFrequency, partofspeech);
	formMatrixTest(source, wordSourceIndex, primarySTLPMatch, winnerFormsString, 3, 0, comboCostFrequency, partofspeech);
	return -1;
}

// checks if the part of speech indicated in parse from the Stanford POS tagger matches the winner forms at wordSourceIndex.
// returns yes=0, no=1
int checkStanfordPCFGAgainstWinner(Source &source, int wordSourceIndex, int numTimesWordOccurred, wstring originalParse, wstring sentence, wstring &parse, int &numTotalDifferenceFromStanford, 
	unordered_map<wstring, int> &formNoMatchMap, unordered_map<wstring, int> &formMisMatchMap, unordered_map<wstring, int> &wordNoMatchMap, unordered_map<wstring, int> &VFTMap,
	bool inRelativeClause, unordered_map<wstring, int> &errorMap, unordered_map<wstring, int> &comboCostFrequency,int startOfSentence,int maxLength)
{
	wstring word = source.m[wordSourceIndex].word->first;
	if (!iswalpha(word[0]) || (word.length()<=1 && word[0]!='a' && word[0]!='i'))
		return 0;
	wstring originalWordSave;
	source.getOriginalWord(wordSourceIndex, originalWordSave, false, false);
	wstring originalWord = originalWordSave;
	int wspace;
	wstring lookFor=stTokenizeWord(word,originalWord, source.m[wordSourceIndex].flags,parse,wspace);
	size_t wow = parse.find(lookFor);
	if (wow != wstring::npos)
	{
		auto firstparen = parse.rfind(L'(', wow);
		if (firstparen != wstring::npos)
		{
			wstring partofspeech = parse.substr(firstparen + 1, wow - firstparen - 1);
			parse.erase(0, wow+ lookFor.length());
			extern unordered_map<wstring, vector<wstring>> pennMapToLP;
			auto lpPOS = pennMapToLP.find(partofspeech);
			if (lpPOS != pennMapToLP.end())
			{
				std::set<wstring> posList(lpPOS->second.begin(), lpPOS->second.end());
				// ST incorrectly assigned a word a punctuation!
				if (posList.empty())
				{
					errorMap[L"ST Punctuation assignment"]++;
					return 0;
				}
				
				wstring primarySTLPMatch = lpPOS->second[0];
				if (partofspeech == L"IN")
					primarySTLPMatch += L" or " + lpPOS->second[1];
				auto fdi = formDistribution.find(word);
				if (fdi == formDistribution.end())
				{
					FormDistribution fd;
					formDistribution[word] = fd;
					fdi = formDistribution.find(word);
				}
				for (auto pos : lpPOS->second)
				{
					fdi->second.STFormDistribution[pos]++;
					auto imai = maxentAssociationMap.find(pos);
					if (imai != maxentAssociationMap.end())
					{
						posList.insert(imai->second.begin(), imai->second.end());
					}
				}
				// ruleCode
				//   -1: LP class corrected.  ST prefers something other than correct class, so LP is correct
				//   -2: LP class corrected.  ST prefers correct class, so this entry should simply be removed from the output file (agree will be set to true, so there is no case statement for this case)
				//   -3: test whether to change to correct class - set to disagree (even if ST and LP forms agree)
				//    0: unable to determine whether class should be corrected.  Continue.
				int ruleCode = ruleCorrectLPClass(primarySTLPMatch, source, wordSourceIndex, errorMap, partofspeech, startOfSentence, fdi);
				vector <int> winnerForms;
				source.m[wordSourceIndex].getWinnerForms(winnerForms);
				bool agree = false;
				for (int wf : winnerForms)
				{
					fdi->second.LPFormDistribution[Forms[wf]->name]++;
					if (posList.find(Forms[wf]->name) != posList.end())
					{
						fdi->second.agreeFormDistribution[Forms[wf]->name]++;
						agree = true;
					}
					else
					{
						fdi->second.disagreeFormDistribution[Forms[wf]->name]++;
					}
				}
				if (agree)
					fdi->second.agreeSTLP++;
				else
				{
					fdi->second.disagreeSTLP++;
					numTotalDifferenceFromStanford++;
				}
				char tempdebugBuffer[4096];
				tempdebugBuffer[0] = 'c';
				if (ruleCode == -1)
				{
					// recording the error in errorMap already taken care of in rule procedure
					return 0;
				}
				else if (ruleCode != -3 && agree)
				{
					return 0;
				}
				else if (ruleCode != -3 && attributeErrors(primarySTLPMatch, source, wordSourceIndex, errorMap, comboCostFrequency, partofspeech, startOfSentence) == 0)
				{
					for (int wf : winnerForms)
					{
						fdi->second.LPAlreadyAccountedFormDistribution[Forms[wf]->name]++;
					}
					return 0;
				}
				//////////////////////////////
				wstring posListStr;
				for (auto pos : posList)
					posListStr += pos + L" ";
				wstring winnerFormsString;
				source.m[wordSourceIndex].winnerFormString(winnerFormsString, false);
				formNoMatchMap[posListStr + L"!= " + winnerFormsString]++;
				wordNoMatchMap[word]++;
				formMisMatchMap[primarySTLPMatch + L"!=" + winnerFormsString]++;
				wstring originalNextWord;
				source.getOriginalWord(wordSourceIndex + 1, originalNextWord, false, false);
				size_t pos = sentence.find(originalWord);
				while (pos != wstring::npos && numTimesWordOccurred>0)
				{
					size_t nextWordPos = pos + originalWord.length() + 1;
					bool wholeWord = (pos == 0 || sentence[pos - 1] == ' ') && (sentence.length() == pos + originalWord.length() || sentence[pos + originalWord.length()] == L' ' || sentence[pos + originalWord.length()] == L'\'');
					if (wholeWord && !--numTimesWordOccurred)
						sentence.replace(pos,originalWord.length(), L"*" + originalWord + L"*");
					pos = sentence.find(originalWord,pos+ originalWord.length()+2);
				}
				if (source.m[wordSourceIndex].flags&WordMatch::flagFirstLetterCapitalized)
					partofspeech += L"**CAP**";
				lplog(LOG_ERROR, L"Stanford POS %s%s (%s) not found in winnerForms %s for word%s %s %07d:[%s]", partofspeech.c_str(), (sentence.length()<=maxLength) ? L"SHORT":L"",primarySTLPMatch.c_str(), winnerFormsString.c_str(), (originalWord.find(L' ')==wstring::npos) ? L"":L"[space]", originalWord.c_str(), wordSourceIndex, sentence.c_str());
				for (int wf : winnerForms)
				{
					fdi->second.LPErrorFormDistribution[Forms[wf]->name]++;
				}
				fdi->second.unaccountedForDisagreeSTLP++;
				//formDistribution[word] = fd;
				if (originalWord.find(L' ') != wstring::npos &&
					word != L"no one" && word != L"every one" && word != L"as if" && word != L"for ever" && word != L"next to" && word != L"good by" && word != L"good bye" && word != L"a trifle" && word!=L"a" && word!=L"i" && word!=L"young 'un")
				{
					lookFor = originalWord.substr(wspace, originalWord.length()) + L")";
					parse = originalParse;
					wow = parse.find(lookFor);
					if (wow != wstring::npos)
					{
						auto firstparen = parse.rfind(L'(', wow);
						if (firstparen != wstring::npos)
						{
							wstring partofspeech = parse.substr(firstparen + 1, wow - firstparen - 1);
							parse.erase(0, wow + lookFor.length());
							extern unordered_map<wstring, vector<wstring>> pennMapToLP;
							auto lpPOS = pennMapToLP.find(partofspeech);
							if (lpPOS != pennMapToLP.end())
							{
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
								bool foundSecondaryForm = false;
								for (int wf : winnerForms)
								{
									if (std::find(posList.begin(), posList.end(), Forms[wf]->name) != posList.end())
										foundSecondaryForm = true;
								}
								if (!foundSecondaryForm)
								{
									wstring posListStr;
									for (auto pos : posList)
										posListStr += pos + L" ";
									lplog(LOG_ERROR, L"FATAL! %07d:ALSO no winnerForm %s is found in ST POS list %s (2)", wordSourceIndex,  winnerFormsString.c_str(), posListStr.c_str());
								}
							}
						}
						else
							lplog(LOG_FATAL_ERROR, L"%d:Parenthesis not found in %s (from position %d) (2).", wordSourceIndex,parse.c_str(), wow);
					}
					else
						lplog(LOG_FATAL_ERROR, L"%d:Word %s not found in parse %s (2).", wordSourceIndex,lookFor.c_str(),parse.c_str());
				}
			}
			else
				lplog(LOG_FATAL_ERROR, L"%d:Part of Speech %s not found looking for word %s in the parse %s.", wordSourceIndex,partofspeech.c_str(),originalWord.c_str(),originalParse.c_str());
		}
		else
			lplog(LOG_FATAL_ERROR, L"%d:Parenthesis not found in %s (from position %d).", wordSourceIndex,parse.c_str(),wow);
	}
	else
		lplog(LOG_ERROR, L"%d:FATAL! Word %s not found in parse %s [%s]", wordSourceIndex,originalWord.c_str(),parse.c_str(),originalParse.c_str());
	return 1;
}

//map <wstring, int> STFormDistribution; // total count for each form match in ST
//map <wstring, int> LPFormDistribution; // total count for each form match in LP
//map <wstring, int> agreeFormDistribution; // total count for each form match agreed between ST and LP
//map <wstring, int> disagreeFormDistribution; // total count for each form match disagreed between ST and LP
void printFormDistribution(wstring word, double adp, FormDistribution fd, wstring &maxWord, wstring &maxForm, int &maxDiff,int limit)
{
	if (fd.unaccountedForDisagreeSTLP == 0)
		return;
	if (limit<1000)
		lplog(LOG_ERROR, L"%s:%d %3.0f (%d/%d)", word.c_str(), fd.unaccountedForDisagreeSTLP, adp, fd.agreeSTLP, fd.agreeSTLP + fd.disagreeSTLP);
	int totalWordOccurrenceCount = fd.agreeSTLP + fd.disagreeSTLP;
	for (auto &&[form, formCount] : fd.LPFormDistribution)
	{
		// form name, total number of times form is a winner form for this word, % of times this form is the winner form for this word, % of times this form agrees with ST.
		if (limit<1000)
			lplog(LOG_ERROR, L"  LP %s:total=%d accounted=%d agree=%d disagree=%d error=%d %d%% %d%%", 
				form.c_str(), 
				formCount, fd.LPAlreadyAccountedFormDistribution[form], fd.agreeFormDistribution[form], fd.disagreeFormDistribution[form], fd.LPErrorFormDistribution[form],
				100 * formCount / totalWordOccurrenceCount, fd.agreeFormDistribution[form] * 100 / formCount);
		// commented out: look for forms that have a high percentage of LP winners, but a low percentage of agreement.
		// actually just look for the highest occurring forms with maximum poor agreement.
		int diff = formCount * (100 - (fd.agreeFormDistribution[form] * 100 / formCount));//count*count / totalWordOccurrenceCount*fd.agreeFormDistribution[form];
		if (fd.LPErrorFormDistribution[form]>10 && (fd.agreeFormDistribution[form] * 100 / formCount) < 5 && fd.LPAlreadyAccountedFormDistribution[form]<5)
		{
			lplog(LOG_ERROR, L"%05d **%s:%3.2f (%d/%d) %s:total=%d accounted=%d agree=%d disagree=%d error=%d %d%% %d%%", 
				fd.LPErrorFormDistribution[form],
				word.c_str(), adp, fd.agreeSTLP, totalWordOccurrenceCount, form.c_str(), 
				formCount, fd.LPAlreadyAccountedFormDistribution[form], fd.agreeFormDistribution[form], fd.disagreeFormDistribution[form], fd.LPErrorFormDistribution[form],
				100 * formCount / totalWordOccurrenceCount, fd.agreeFormDistribution[form] * 100 / formCount);
		}
		if (maxDiff < diff)
		{
			maxWord = word;
			maxDiff = diff;
			maxForm = form;
		}
	}
	if (limit < 1000)
		for (auto &&[form, count] : fd.STFormDistribution)
			lplog(LOG_ERROR, L"  ST %s:%d %d%% %d%%", form.c_str(), count, 100 * count / (fd.agreeSTLP + fd.disagreeSTLP), fd.agreeFormDistribution[form] * 100 / count);
}

int stanfordCheckFromSource(Source &source, int sourceId, wstring path, JavaVM *vm,JNIEnv *env, int &numNoMatch, int &numPOSNotFound, int &numTotalDifferenceFromStanford,unordered_map<wstring, int> &formNoMatchMap,
	                          unordered_map<wstring, int> &formMisMatchMap, unordered_map<wstring, int> &wordNoMatchMap, unordered_map<wstring, int> &VFTMap, 
	                          unordered_map<wstring, int> &errorMap, unordered_map<wstring, int> &comboCostFrequency, bool pcfg,wstring limitToWord,int maxLength, wstring specialExtension,bool lockPerSource)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, L""); 
	bool parsedOnly = false,bearFound=false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, true,specialExtension))
	{
		lplog(LOG_INFO| LOG_ERROR, L"source*** %d:%s", sourceId, path.c_str());
		int lastPercent=-1;
		if (lockPerSource)
		{
			if (!myquery(&source.mysql, L"LOCK TABLES stanfordPCFGParsedSentences WRITE")) // moved out parseSentence (actually in foundParseSentence and setParsedSentence) for performance
				return -20;
		}
		for (int wordSourceIndex = 0; wordSourceIndex < source.m.size(); )
		{
			int currentPercent = wordSourceIndex * 100 / source.m.size();
			if (lastPercent < currentPercent)
			{
				printf("%s %03d%% done - %09d out of %09I64d\r", (pcfg) ? "pcfgCheck":"maxentCheck",currentPercent, wordSourceIndex, source.m.size());
				lastPercent = currentPercent;
			}
			int endIndex = wordSourceIndex;
			while (endIndex < source.m.size() && !source.isEOS(endIndex) && source.m[endIndex].word != Words.sectionWord)
				endIndex++;
			if (endIndex < source.m.size() && source.isEOS(endIndex))
				endIndex++;
			wstring sentence;
			int numLetters=0, numNumbers=0;
			for (int I = wordSourceIndex; I < endIndex; I++)
			{
				wstring originalIWord;
				source.getOriginalWord(I, originalIWord, false, false);
				if (source.m[I].word->first.length() == 1 && iswalpha(source.m[I].word->first[0]))
					numLetters++;
				if (source.m[I].word->second.query(NUMBER_FORM_NUM) >= 0)
					numNumbers++;
				sentence += originalIWord + L" ";
			}
			if (sentence.empty())
			{
				wordSourceIndex++;
				continue;
			}
			// not a sentence
			if ((numNumbers + numLetters) > (endIndex - wordSourceIndex) / 2)
			{
				wordSourceIndex=endIndex; 
				continue;
			}
			wstring parse;
			if (parseSentence(source,env, sentence, parse, pcfg, !lockPerSource) < 0)
				return -1;
			wstring originalParse = parse;
			int start = wordSourceIndex, until = -1,pmaOffset=-1;
			bool inRelativeClause = false, sentencePrinted = false;
			map <wstring, int> numTimesWordOccurred;
			for (; wordSourceIndex < endIndex; wordSourceIndex++)
			{
				wstring originalIWord;
				source.getOriginalWord(wordSourceIndex, originalIWord, false, false);
				numTimesWordOccurred[originalIWord]++;
				if ((pmaOffset = source.scanForPatternTag(wordSourceIndex, REL_TAG)) != -1 || (pmaOffset = source.scanForPatternTag(wordSourceIndex, SENTENCE_IN_REL_TAG)) != -1)
				{
					inRelativeClause = true;
					until = wordSourceIndex+source.m[wordSourceIndex].pma[pmaOffset].len;
				}
				if (limitToWord.empty() || limitToWord == source.m[wordSourceIndex].word->first)
				{
					if (!pcfg)
					{
						if (checkStanfordMaxentAgainstWinner(source, wordSourceIndex, originalParse, parse, numPOSNotFound, formNoMatchMap, wordNoMatchMap, inRelativeClause))
						{
							numNoMatch++;
							if (!sentencePrinted)
								source.printSentence(SCREEN_WIDTH, start, endIndex, true);
							sentencePrinted = true;
						}
					}
					else
					{
						if (checkStanfordPCFGAgainstWinner(source, wordSourceIndex, numTimesWordOccurred[originalIWord], originalParse, sentence, parse, numTotalDifferenceFromStanford, formNoMatchMap, formMisMatchMap, wordNoMatchMap, VFTMap, inRelativeClause, errorMap, comboCostFrequency, start,maxLength))
						{
							numNoMatch++;
							if (!sentencePrinted)
								source.printSentence(SCREEN_WIDTH, start, endIndex, true);
							sentencePrinted = true;
						}
					}
				}
				if (wordSourceIndex == until)
					inRelativeClause = false;
			}
			if (source.m[wordSourceIndex].word == Words.sectionWord)
				wordSourceIndex++;
		}
		source.unlockTables();
	}
	else
	{
		lplog(LOG_FATAL_ERROR, L"Unable to read source %d:%s\n", sourceId, path.c_str());
		return -1;
	}
	return 10;
}

void distributeErrors(unordered_map<wstring, int> &errorMap)
{
	int numErrors=errorMap[L"LP correct: adjective not verb (94% probabilistic)"];  // out of 215 examples studied, 12 were incorrect
	errorMap[L"LP correct: adjective not verb (94% probabilistic)"] = numErrors * 94 / 100;
	errorMap[L"ST correct: adjective not verb (6% probabilistic)"] = numErrors * 6 / 100;

	numErrors = errorMap[L"LP correct: adverb not noun (97% probabilistic)"];  // out of 233 examples studied, 7 were incorrect
	errorMap[L"LP correct: adverb not noun (97% probabilistic)"] = numErrors * 97 / 100;
	errorMap[L"ST correct: adverb not noun (3% probabilistic)"] = numErrors * 3 / 100;

	numErrors = errorMap[L"LP correct: adjective not noun (noun cost 4, adjective cost 0)"]; // out of 252 examples, 23 were incorrect (15 of those were the word 'safe'?)
	errorMap[L"LP correct: adjective not noun (noun cost 4, adjective cost 0)"] = numErrors * 90 / 100;
	errorMap[L"ST correct: adjective not noun (noun cost 4, adjective cost 0)"] = numErrors * 10 / 100;

	numErrors = errorMap[L"LP correct: noun not verb (verb cost 4, noun cost 0)"]; // out of 135 examples, 32 were incorrect 
	errorMap[L"LP correct: noun not verb (verb cost 4, noun cost 0)"] = numErrors * 76 / 100;
	errorMap[L"ST correct: noun not verb (verb cost 4, noun cost 0)"] = numErrors * 24 / 100;

	numErrors = errorMap[L"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"]; // probability 6 out of 130 are ST correct
	errorMap[L"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"] = numErrors * 130 / 136;
	errorMap[L"ST correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"] = numErrors * 6 / 136;
	
	numErrors = errorMap[L"LP correct: noun more probable than adjective"]; // 19 ST correct out of 119 total
	errorMap[L"LP correct: noun more probable than adjective"] = numErrors * 100 / 119;
	errorMap[L"ST correct: noun more probable than adjective"] = numErrors * 19 / 119;

	numErrors = errorMap[L"LP correct: preposition with relative object"]; // 22 ST correct out of 185 total
	errorMap[L"LP correct: preposition with relative object"] = numErrors * 163 / 185;
	errorMap[L"ST correct: preposition with relative object"] = numErrors * 22 / 185;
	
	numErrors = errorMap[L"LP correct: (noun cost 3, verb cost 0)"]; // 73 ST correct out of 217 total
	errorMap[L"LP correct: (noun cost 3, verb cost 0)"] = numErrors * 144 / 217;
	errorMap[L"ST correct: (noun cost 3, verb cost 0)"] = numErrors * 73 / 217;

	numErrors = errorMap[L"LP correct: (noun cost 3, adjective cost 0)"]; // 71 ST correct out of 403 total
	errorMap[L"LP correct: (noun cost 3, adjective cost 0)"] = numErrors * 332 / 403;
	errorMap[L"ST correct: (noun cost 3, adjective cost 0)"] = numErrors * 71 / 403;

	numErrors = errorMap[L"LP correct: ownership of noun"]; // 12 ST correct out of 91 total 
	errorMap[L"LP correct: ownership of noun"] = numErrors * 79 / 91;
	errorMap[L"ST correct: ownership of noun"] = numErrors * 12 / 91;

	numErrors = errorMap[L"LP correct: past of verb is not adjective"]; // 21 ST correct out of 105 total  
	errorMap[L"LP correct: past of verb is not adjective"] = numErrors * 84 / 105;
	errorMap[L"ST correct: past of verb is not adjective"] = numErrors * 21 / 105;

	numErrors = errorMap[L"LP correct: word 'kind' ST says adjective"]; // 17 ST correct out of 102 total  
	errorMap[L"LP correct: word 'kind' ST says adjective"] = numErrors * 85 / 102;
	errorMap[L"ST correct: word 'kind' ST says adjective"] = numErrors * 17 / 102;

	// LP Wrong 27 / LP Correct 258  distribute errors
	numErrors = errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb before verb"];
	errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb before verb"]=numErrors*258/(258+27);
	errorMap[L"ST correct: ST says preposition or conjunction and LP says adverb before verb"]=numErrors*27/(258+27);

	numErrors = errorMap[L"ST correct: ST says preposition or conjunction and LP says adverb before noun"]; // LP Wrong 171 / ST Wrong 6  distribute errors
	errorMap[L"LP correct: ST says preposition or conjunction and LP says adverb before noun"] = numErrors * 6 / 177;
	errorMap[L"ST correct: ST says preposition or conjunction and LP says adverb before noun"] = numErrors * 171 / 177;

	numErrors = errorMap[L"LP correct: ST says adjective (wrong)"]; // 30 ST correct out of 329 total  
	errorMap[L"LP correct: ST says adjective (wrong)"] = numErrors * 299 / 329;
	errorMap[L"ST correct: ST says adjective (wrong)"] = numErrors * 30 / 329;

}

int stanfordCheck(Source source, int step, bool pcfg, wstring specialExtension, bool lockPerSource)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum Source::sourceTypeEnum st = Source::GUTENBERG_SOURCE_TYPE;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	bool websterAPIRequestsExhausted = false;
	int startTime = clock(), numSourcesProcessedNow = 0;
	wchar_t buffer[1024];
	wsprintf(buffer, L"stanfordCheck %d", step);
	SetConsoleTitle(buffer);
	lplog(LOG_INFO | LOG_ERROR | LOG_NOTMATCHED, NULL); // close all log files to change extension
	logFileExtension = L".stanfordCheckErrors"+specialExtension;

	if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE"))
		return -1;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by numWords desc", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	JavaVM *vm;
	JNIEnv *env;
	createJavaVM(vm, env);
	unordered_map<wstring, int> formNoMatchMap, formMisMatchMap, wordNoMatchMap,VFTMap,errorMap, comboCostFrequency;
	int numNoMatch = 0, numPOSNotFound = 0,totalWords=0, numTotalDifferenceFromStanford=0;
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
		wchar_t buffer[1024];
		__int64 processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s...)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
			processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, (processingSeconds) ? numSourcesProcessedNow * 3600 / processingSeconds : 0, title.c_str());
		SetConsoleTitle(buffer);
		int setStep = stanfordCheckFromSource(source, sourceId, path, vm, env, numNoMatch, numPOSNotFound, numTotalDifferenceFromStanford, formNoMatchMap, formMisMatchMap, wordNoMatchMap,VFTMap,errorMap, comboCostFrequency,pcfg,L"",60,specialExtension,lockPerSource);
		totalWords += source.m.size();
		_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		numSourcesProcessedNow++;
	}
	mysql_free_result(result);
	destroyJavaVM(vm);
	lplog(LOG_ERROR, L"WORD AGREE DISTRIBUTION ------------------------------------------------------------");
	map <int, wstring, std::greater<int>> agreeCountMap;
	for (auto &&[word, fd] : formDistribution)
	{
		agreeCountMap[fd.unaccountedForDisagreeSTLP] += word + L"*";
	}
	int limit = 0;
	int maxDiff = -1;
	wstring maxForm,maxWord;
	for (auto &&[adp, multiword] : agreeCountMap)
	{
		for (auto word : splitString(multiword, L'*'))
			if (formDistribution[word].agreeSTLP + formDistribution[word].disagreeSTLP > 500)
			{
				printFormDistribution(word, adp, formDistribution[word], maxWord, maxForm, maxDiff,limit);
				limit++;
			}
	}
	lplog(LOG_ERROR, L"maxWord=%s maxForm=%s maxDiff=%d", maxWord.c_str(), maxForm.c_str(), maxDiff);
	lplog(LOG_ERROR, L"FORMS ------------------------------------------------------------------------------");
	map<int, wstring, std::greater<int>> formNoMatchReverseMap, formMisMatchReverseMap, wordNoMatchReverseMap,VFTReverseMap,errorReverseMap, comboCostFrequencyReverseMap;
	for (auto const&[forms, count] : formNoMatchMap)
		formNoMatchReverseMap[count] += forms + L" *";
	for (auto const&[count, forms] : formNoMatchReverseMap)
		lplog(LOG_ERROR, L"forms %s [%d]", forms.c_str(), count);
	lplog(LOG_ERROR, L"FORM MAPPING ---------------------------------------------------------------------------");
	for (auto const&[forms, count] : formMisMatchMap)
		formMisMatchReverseMap[count] += forms + L" *";
	for (auto const&[count, forms] : formMisMatchReverseMap)
		lplog(LOG_ERROR, L"forms %s [%d]", forms.c_str(), count);
	if (!VFTMap.empty())
	{
		lplog(LOG_ERROR, L"VFT --------------------------------------------------------------------------------");
		for (auto const&[word, count] : VFTMap)
			VFTReverseMap[count] += word + L" *";
	for (auto const&[count, word] : VFTReverseMap)
		lplog(LOG_ERROR, L"VFT %s [%d]", word.c_str(), count);
	}
	lplog(LOG_ERROR, L"WORDS ------------------------------------------------------------------------------");
	for (auto const&[forms, count] : wordNoMatchMap)
		wordNoMatchReverseMap[count] += forms + L" *";
	int numListed = 0;
	for (auto const&[count, forms] : wordNoMatchReverseMap)
	{
		lplog(LOG_ERROR, L"words %s [%d]", forms.c_str(), count);
		if (numListed++ > 100)
			break;
	}
	lplog(LOG_ERROR, L"ComboExtremeCosts ------------------------------------------------------------------------------");
	for (auto const&[formCombo, frequency] : comboCostFrequency)
		comboCostFrequencyReverseMap[frequency] += formCombo + L" *";
	numListed = 0;
	for (auto const&[frequency, formCombo] : comboCostFrequencyReverseMap)
	{
		lplog(LOG_ERROR, L"combo %s [%d]", formCombo.c_str(), frequency);
		if (numListed++ > 100)
			break;
	}
	lplog(LOG_ERROR, L"DIFF ANALYSIS ------------------------------------------------------------------------");
	distributeErrors(errorMap);
	int LPErrors = 0, STErrors = 0, diff=0;
	for (auto const&[error, count] : errorMap)
	{
		if (error.find(L"LP correct") != wstring::npos)
			STErrors += count;
		else if (error.find(L"ST correct") != wstring::npos)
			LPErrors += count;
		else if (error.find(L"diff") != wstring::npos)
			diff += count;
		errorReverseMap[count] += error + L" *";
	}
	if (LPErrors + STErrors + diff > 0)
	{
		lplog(LOG_ERROR, L"LP errors: %d %d%% ST errors=%d %d%% diff=%d %d%%", LPErrors, 100 * LPErrors / (LPErrors + STErrors + diff), STErrors, 100 * STErrors / (LPErrors + STErrors + diff), diff, 100 * diff / (LPErrors + STErrors + diff));
		for (auto const&[count, multierror] : errorReverseMap)
		{
			for (auto LPSTErr:splitString(multierror, L'*'))
				lplog(LOG_ERROR, L"%07d:%s", count, LPSTErr.c_str());
		}
	}
	if (totalWords > 0)
		lplog(LOG_ERROR, L"numNoMatch=%d/%d %6.3f%% numTotalDifferenceFromStanford=%d", numNoMatch, totalWords, numNoMatch * 100.0 / totalWords, numTotalDifferenceFromStanford);
	return 0;
}

int stanfordCheckMP(Source source, int step, bool pcfg, int MP)
{
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE, sources rs WRITE, sources rs2 WRITE"))
		return -1;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources,(select id,ROW_NUMBER() over w rn from sources rs2 where proc2=%d WINDOW w AS (ORDER BY id)) rs set proc2=(rs.rn%%%d)+100 where sources.id=rs.id", step,MP);
	if (!myquery(&source.mysql, qt))
		return -1;
	/*
	vector <std::future<int>> threadResults;
	for (int I = 0; I < MP; I++)
	{
		threadResults.emplace_back(std::async(&stanfordCheck, std::ref(source), I, pcfg));
	}
	std::future_status status;
	while (true)
	{
		for (auto &f : threadResults)
		{
			status = f.wait_for(std::chrono::seconds(2));
			switch (status)
			{
			case std::future_status::deferred:
				printf("deferred\n");
				break;
			case std::future_status::timeout:
				//printf("timeout\n");
				break;
			case std::future_status::ready:
				printf("ready\n");
				break;
			}
		}
	} 
	*/
	source.unlockTables();
	chdir("source");
	wchar_t processParameters[1024];
	int numProcesses = MP;
	HANDLE *handles = (HANDLE *)calloc(numProcesses, sizeof(HANDLE));
	for (int I = 0; I < numProcesses; I++)
	{
		wsprintf(processParameters, L"CorpusAnalysis.exe -step %d", I+step+1);
		HANDLE processHandle = 0;
		DWORD processId = 0;
		int errorCode;
		if (errorCode = createLPProcess(I, processHandle, processId, L"x64\\StanfordParseMT\\CorpusAnalysis.exe", processParameters) < 0)
			break;
		handles[I] = processHandle;
	}
	wstring tmpstr;
	while (numProcesses)
	{
		unsigned int nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, false, 1000 * 60 * 5, false);
		if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
			continue;
		if (nextProcessIndex == WAIT_FAILED)
			lplog(LOG_FATAL_ERROR, L"\nWaitForMultipleObjectsEx [StanfordCheckMT] failed with error %s", getLastErrorMessage(tmpstr));
		if (nextProcessIndex < WAIT_OBJECT_0 + numProcesses) 
		{
			nextProcessIndex -= WAIT_OBJECT_0;
			printf("\nClosing process %d", nextProcessIndex);
			CloseHandle(handles[nextProcessIndex]);
			memmove(handles + nextProcessIndex, handles + nextProcessIndex + 1, (MP - nextProcessIndex - 1) * sizeof(handles[0]));
			numProcesses--;
		}
		if (nextProcessIndex >= WAIT_ABANDONED_0 && nextProcessIndex < WAIT_ABANDONED_0 + MP)
		{
			nextProcessIndex -= WAIT_ABANDONED_0;
			printf("\nClosing process %d [abandoned]", nextProcessIndex);
			CloseHandle(handles[nextProcessIndex]);
			memmove(handles + nextProcessIndex, handles + nextProcessIndex + 1, (MP - nextProcessIndex - 1) * sizeof(handles[0]));
			numProcesses--;
		}
	}
	free(handles);
	return 0;
}

int stanfordCheckTest(Source source, wstring path, int sourceId, bool pcfg,wstring limitToWord,int maxSentenceLimit, wstring specialExtension)
{
	if (limitToWord.length() > 0)
		printf("limited to %S!\n", limitToWord.c_str());
	JavaVM *vm;
	JNIEnv *env;
	createJavaVM(vm, env);
	unordered_map<wstring, int> formNoMatchMap, formMisMatchMap, wordNoMatchMap, VFTMap, errorMap, comboCostFrequency ;
	int numNoMatch = 0, numPOSNotFound = 0, numTotalDifferenceFromStanford=0;
	stanfordCheckFromSource(source, sourceId, path, vm, env, numNoMatch, numPOSNotFound, numTotalDifferenceFromStanford, formNoMatchMap, formMisMatchMap, wordNoMatchMap, VFTMap, errorMap, comboCostFrequency, pcfg,limitToWord,maxSentenceLimit,specialExtension,true);
	int totalWords = source.m.size();
	destroyJavaVM(vm);
	if (limitToWord.length() > 0)
		return 0;
	if (totalWords > 0)
		lplog(LOG_ERROR, L"numNoMatch=%d/%d %7.3f%% numPOSNotFound=%d", numNoMatch, totalWords, numNoMatch * 100.0 / totalWords, numPOSNotFound);
	lplog(LOG_ERROR, L"WORD AGREE DISTRIBUTION ------------------------------------------------------------");
	map <double, wstring> agreeCountMap;
	for (auto &&[word, fd] : formDistribution)
	{
		agreeCountMap[((double)fd.agreeSTLP) / (fd.agreeSTLP + fd.disagreeSTLP)] = word;
	}
	int limit = 0;
	int maxDiff = -1;
	wstring maxForm, maxWord;
	for (auto &&[adp, word] : agreeCountMap)
	{
		printFormDistribution(word, adp, formDistribution[word], maxWord, maxForm, maxDiff,limit);
		if (limit++ > 1000)
			break;
	}
	lplog(LOG_ERROR, L"FORMS ------------------------------------------------------------------------------");
	map<int, wstring, std::greater<int>> formNoMatchReverseMap, wordNoMatchReverseMap, VFTReverseMap, errorReverseMap;
	for (auto const&[forms, count] : formNoMatchMap)
		formNoMatchReverseMap[count] += forms + L" *";
	for (auto const&[count, forms] : formNoMatchReverseMap)
		lplog(LOG_ERROR, L"forms %s [%d]", forms.c_str(), count);
	lplog(LOG_ERROR, L"WORDS ------------------------------------------------------------------------------");
	for (auto const&[forms, count] : wordNoMatchMap)
		wordNoMatchReverseMap[count] += forms + L" *";
	int numListed = 0;
	for (auto const&[count, forms] : wordNoMatchReverseMap)
	{
		lplog(LOG_ERROR, L"words %s [%d]", forms.c_str(), count);
		if (numListed++ > 100)
			break;
	}
	lplog(LOG_ERROR, L"DIFF ANALYSIS ------------------------------------------------------------------------");
	int LPErrors = 0, STErrors = 0, diff = 0;
	for (auto const&[error, count] : errorMap)
	{
		if (error.find(L"LP correct") != wstring::npos)
			STErrors += count;
		else if (error.find(L"ST correct") != wstring::npos)
			LPErrors += count;
		else if (error.find(L"diff") != wstring::npos)
			diff += count;
		errorReverseMap[count] += error + L" *";
	}
	if (LPErrors + STErrors + diff > 0)
	{
		lplog(LOG_ERROR, L"LP errors: %d %d%% ST errors=%d %d%% diff=%d %d%%", LPErrors, 100 * LPErrors / (LPErrors + STErrors + diff), STErrors, 100 * STErrors / (LPErrors + STErrors + diff), diff, 100 * diff / (LPErrors + STErrors + diff));
		for (auto const&[count, multierror] : errorReverseMap)
		{
			for (auto LPSTErr : splitString(multierror, L'*'))
				lplog(LOG_ERROR, L"%07d:%s", count, LPSTErr.c_str());
		}
	}
	return 0;
}

// this would be easier if you had all the sources in memory at once!
int testViterbiHMMMultiSource(Source &source,wchar_t *databaseHost,int step, wstring specialExtension)
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
		Words.readWords(path, sourceId, false, L"");
		//unordered_map <int, vector < vector <tTagLocation> > > emptyMap;
		//for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
		//	childSource.pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
		if (childSource.readSource(path, false, parsedOnly, false, true,specialExtension))
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
	setConsoleWindowSize(85, 5);
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
	wstring specialExtension = L"";
	//testDisinclination();
	//writeFrequenciesToDB(source);
	//if (true)
	//	return;
	nounForm = FormsClass::gFindForm(L"noun");
	verbForm = FormsClass::gFindForm(L"verb");
	adjectiveForm = FormsClass::gFindForm(L"adjective");
	adverbForm = FormsClass::gFindForm(L"adverb");
	int step=-1;
	bool actuallyExecuteAgainstDB=false;
	for (int I = 0; I < argc; I++)
	{
		if (!_wcsicmp(argv[I], L"-step") && I < argc - 1)
			step = _wtoi(argv[++I]);
		else if (!_wcsicmp(argv[I], L"-stanfordCheck") && I < argc - 1)
		{
			stanfordCheck(source, step, true, specialExtension,true);
			return;
		}
		else if (!_wcsicmp(argv[I], L"-specialExtension"))
			specialExtension = argv[++I];
		else if (!_wcsicmp(argv[I], L"-logFileExtension"))
			logFileExtension = argv[++I];
		else if (!_wcsicmp(argv[I], L"-executeAgainstDB"))
			actuallyExecuteAgainstDB = true;
		else
			continue;
	}
	logFileExtension = specialExtension;
	if (step > 100)
	{
		stanfordCheck(source, step, true, specialExtension,false);
		return;
	}
	switch (step)
	{
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
		populateWordFrequencyTable(source,step,specialExtension);
		break;
	case 2:
		writeWordFormsFromCorpusWideAnalysis(source.mysql, actuallyExecuteAgainstDB);
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
		testRDFType(source,specialExtension);
		break;
	case 8:
		testViterbiHMMMultiSource(source,databaseHost,step,specialExtension);
		break;
	case 20:
		printUnknowns(source, step,specialExtension);
		break;
	case 21:
		// Source::TEST_SOURCE_TYPE
		//patternOrWordAnalysis(source, step, L"__S1", L"R*", Source::GUTENBERG_SOURCE_TYPE,true,specialExtension);
		//patternOrWordAnalysis(source, step, L"__ADJECTIVE", L"MTHAN", Source::GUTENBERG_SOURCE_TYPE, true, specialExtension);
		//patternOrWordAnalysis(source, step, L"__NOUN", L"F", Source::GUTENBERG_SOURCE_TYPE, true, specialExtension);
		//patternOrWordAnalysis(source, step, L"__S1", L"5", true);
		patternOrWordAnalysis(source, step, L"__C1__S1", L"1",L"__NOUN", L"9", Source::GUTENBERG_SOURCE_TYPE, true,true, specialExtension);
		//patternOrWordAnalysis(source, step, L"", L"", Source::GUTENBERG_SOURCE_TYPE, false, specialExtension);
		//patternOrWordAnalysis(source, step, L"worth", L"", Source::GUTENBERG_SOURCE_TYPE, false,L""); // TODO: testing weight change on _S1.
		break;
	case 60:
		stanfordCheck(source, step, true,specialExtension,true);
		break;
	case 61:
		syntaxCheck(source, step,specialExtension);
		break;
	case 70:
		stanfordCheckTest(source, L"F:\\lp\\tests\\thatParsing.txt", 27568, true,L"",50,specialExtension);
		break;
	case 71:
	{
		vector <wstring> words = { L"advertising",L"wishing",L"writing",L"yachting",L"yellowing" };
		if (!myquery(&source.mysql, L"LOCK TABLES words WRITE"))
			return;
		for (wstring sWord : words)
		{
			set <int> posSet;
			bool plural, networkAccessed, logEverything = true;
			getMerriamWebsterDictionaryAPIForms(sWord, posSet, plural, networkAccessed, logEverything);
			wstring sForms;
			bool hasNoun = false;
			for (int form : posSet)
			{
				sForms += Forms[form]->name + L" ";
				if (Forms[form]->name == L"noun")
					hasNoun = true;
			}
			if (!hasNoun)
				lplog(LOG_INFO, L"***%s: %s", sWord.c_str(), sForms.c_str());
			wstring pluralWord = sWord + L"s";
			wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
			int startTime = clock(), numWordsInserted = 0;
			MYSQL_RES * result;
			MYSQL_ROW sqlrow = NULL;
			_snwprintf(qt, QUERY_BUFFER_LEN, L"select id from words where word=\"%s\"", pluralWord.c_str());
			int wordId = -1;
			if (myquery(&source.mysql, qt, result)) // if result is null, also returns false
			{
				if (sqlrow = mysql_fetch_row(result))
				{
					wordId = atoi(sqlrow[0]);
					mysql_free_result(result);
				}
			}
			if (wordId < 0)
				lplog(LOG_INFO, L"***%s: plural %s not found.", sWord.c_str(), pluralWord.c_str());
		}
	}
	case 100:
		stanfordCheckMP(source, step, true,12);
		break;
	}
	source.unlockTables();
}


