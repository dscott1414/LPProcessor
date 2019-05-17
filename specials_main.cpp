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

int startProcesses(Source &source, int processKind, int step, int beginSource, int endSource, Source::sourceTypeEnum st, int maxProcesses, int numSourcesPerProcess, bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite)
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
		wstring start, path, etext, author, title, pathInCache;
		bool result;
		switch (processKind)
		{
		case 0:result = source.getNextUnprocessedParseRequest(prId, pathInCache); break;
		case 1:result = source.getNextUnprocessedSource(beginSource, endSource, st, false, id, path, start, repeatStart, etext, author, title); break;
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
		HANDLE processId = 0;
		wchar_t processParameters[1024];
		switch (processKind)
		{
		case 0:
			wsprintf(processParameters, L"releasex64\\lp.exe -ParseRequest \"%s\" -cacheDir %s %s%s%s%s-log %d", pathInCache.c_str(), CACHEDIR,
				(forceSourceReread) ? L"forceSourceReread " : L"",
				(sourceWrite) ? L"-SW " : L"",
				(sourceWordNetRead) ? L"-SWNR " : L"",
				(sourceWordNetWrite) ? L"-SWNW " : L"",
				numProcesses);
			if (errorCode = createLPProcess(nextProcessIndex, processId, L"releasex64\\lp.exe", processParameters) < 0)
				break;
			break;
		case 1:
			wsprintf(processParameters, L"releasex64\\lp.exe -book 0 + -BC 0 -cacheDir %s %s%s%s%s-numSourceLimit %d -log %d", CACHEDIR,
				(forceSourceReread) ? L"forceSourceReread " : L"",
				(sourceWrite) ? L"-SW " : L"",
				(sourceWordNetRead) ? L"-SWNR " : L"",
				(sourceWordNetWrite) ? L"-SWNW " : L"",
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
	coord.X = max(300, width);
	coord.Y = max(6000, height);
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
	return numRequests < 1000;
}

bool getMerriamWebsterDictionaryAPIForms(wstring sWord, set <int> &posSet, bool &plural, bool &networkAccessed);
bool existsInDictionaryDotCom(wstring word, bool &networkAccessed);
bool detectNonEuropeanWord(wstring word);
int cacheWebPath(wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, bool forceWebReread, bool &networkAccessed);
string lookForPOS(yajl_val node);
int discoverInflections(set <int> posSet, bool plural, wstring word);

int getWordPOS(wstring word, set <int> &posSet, bool &plural, bool print, bool &isNonEuropean, int &dictionaryComQueried, int &dictionaryComCacheQueried, bool &websterAPIRequestsExhausted)
{
	LFS
	if (isNonEuropean = detectNonEuropeanWord(word))
		return 0;
	bool networkAccessed, existsDM = existsInDictionaryDotCom(word, networkAccessed);
	if (networkAccessed)
		dictionaryComQueried++;
	else
		dictionaryComCacheQueried++;
	if (!existsDM || websterAPIRequestsExhausted)
		return 0;
	networkAccessed = false;
	getMerriamWebsterDictionaryAPIForms(word, posSet, plural, networkAccessed);
	if (networkAccessed && !MWRequestAllowed())
	{
		websterAPIRequestsExhausted = true;
	}
	return 0;
}

int getWordPOSInContext(MYSQL *mysql,Source &source, int sourceId, WordMatch &word, bool capitalized, int numUnknownWord, int numUnknownOriginalWord, int numUnknownAllCaps, set <int> &posSet,
	bool print, bool &plural, bool &isNonEuropean, bool &queryOnLowerCase, int &dictionaryComQueried, int &dictionaryComCacheQueried, bool &websterAPIRequestsExhausted)
{
	LFS
	// numUnknownAllCaps is zero if originalWord is already all caps.
	if (queryOnLowerCase = (numUnknownWord > 5 && ((numUnknownOriginalWord + numUnknownAllCaps)*100.0 / numUnknownWord) > 95.0 && capitalized))
		posSet.insert(FormsClass::gFindForm(L"noun"));
	else
		getWordPOS(word.word->first, posSet, plural, false, isNonEuropean, dictionaryComQueried, dictionaryComCacheQueried, websterAPIRequestsExhausted);
	if (posSet.size() > 0 && print)
	{
		wstring posStr;
		for (int form : posSet)
			posStr += L" " + Forms[form]->name;
	}
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

int createWordFormsInDBIfNecessary(Source source, int sourceId, int wordId, wstring word, bool actuallyExecuteAgainstDB)
{
	LFS
	wchar_t qt[query_buffer_len_overflow];
	if (wordId < 0)
	{
		int startTime = clock(), numWordsInserted = 0;
		wcscpy(qt, L"INSERT IGNORE INTO words VALUES "); // IGNORE is necessary because C++ treats unicode strings as different, but MySQL treats them as the same
		wstring word;
		wchar_t sourceStr[10];
		int len = 0, inflectionFlags=0, flags=0, timeFlags=0, derivationRules=0;
		_snwprintf(qt + len, query_buffer_len - len, L"(NULL,\"%s\",%d,%d,%d,%d,%d,%s,NULL),",
			word.c_str(), inflectionFlags, flags, timeFlags, -1, derivationRules,_itow(sourceId, sourceStr, 10));
		MYSQL_RES * result;
		if (actuallyExecuteAgainstDB)
		{
			if (!myquery(&source.mysql, L"LOCK TABLES words WRITE")) 
				return -1;
			if (!myquery(&source.mysql, qt, result))
				return -1;
			mysql_free_result(result);
		}
		else
			lplog(LOG_INFO, L"DB statement [%s create word]: %s", word.c_str(), qt);
	}
	return 0;
}


int overwriteWordFormsInDB(Source source, int wordId, wstring word, set <int> posSet, bool actuallyExecuteAgainstDB)
{
	LFS
	wchar_t qt[query_buffer_len_overflow];
	// erase all wordforms associated with wordId in wordforms
	_snwprintf(qt, query_buffer_len, L"delete wordforms where formId<%d and wordId=%d", tFI::patternFormNumOffset, wordId);
	MYSQL_RES * result;
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&source.mysql, L"LOCK TABLES wordforms WRITE"))
			return -1;
		if (!myquery(&source.mysql, qt, result))
			return -1;
		mysql_free_result(result);
	}
	else
		lplog(LOG_INFO, L"DB statement [%s delete]: %s", word.c_str(), qt);
	wcscpy(qt, L"insert wordForms(wordId,formId,count) VALUES ");
	int len = wcslen(qt);
	// insert wordForms(wordId, formId, count) VALUES(2, 33, 4), (5, 6, 7)
	for (int form : posSet)
		if (form < tFI::patternFormNumOffset)
			len += wsprintf(qt + len, L"(%d,%d,1)", wordId, form+1); // formId -1 yields form offset in memory - 
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&source.mysql, qt, result))
			return -1;
		mysql_free_result(result);
		source.unlockTables();
	}
	else
		lplog(LOG_INFO, L"DB statement [%s forms insert]: %s", word.c_str(), qt);
	return 0;
}

// queryOnLowerCase will query for word forms  the next time the word is encountered in all lower case.
// queryOnLowerCase = 4
int overwriteWordFlagsInDB(Source source, int wordId, wstring word, bool actuallyExecuteAgainstDB)
{
	LFS
		// erase all wordforms associated with wordId in wordforms
		wchar_t qt[query_buffer_len_overflow];
	_snwprintf(qt, query_buffer_len, L"update words where wordId=%d set flags=flags|4", wordId);
	MYSQL_RES * result;
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&source.mysql, L"LOCK TABLES words WRITE"))
			return -1;
		if (!myquery(&source.mysql, qt, result))
			return -1;
		mysql_free_result(result);
	}
	else
		lplog(LOG_INFO, L"DB statement [%s add queryOnLowerCase flag]: %s", word.c_str(), qt);
	return 0;
}

// PLURAL refers to noun plural form.
int overwriteWordInflectionFlagsInDB(Source source, int wordId, wstring word, int inflections, bool actuallyExecuteAgainstDB)
{
	LFS
	// erase all wordforms associated with wordId in wordforms
	wchar_t qt[query_buffer_len_overflow];
	_snwprintf(qt, query_buffer_len, L"update words where wordId=%d set inflectionFlags=inflectionFlags+inflections", wordId);
	MYSQL_RES * result;
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&source.mysql, L"LOCK TABLES words WRITE"))
			return -1;
		if (!myquery(&source.mysql, qt, result))
			return -1;
		mysql_free_result(result);
	}
	else
		lplog(LOG_INFO, L"DB statement [%s add inflection flag]: %s", word.c_str(), qt);
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
void scanAllWebsterEntries(wchar_t *basepath, unordered_set<wstring> &pos,int &numFilesProcessed)
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
				scanAllWebsterEntries(completePath, pos,numFilesProcessed);
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
							mTW(lookForPOS(doc), temppos);
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



void writeWordFrequencyTable(MYSQL *mysql,unordered_map<wstring, wordInfo> wf, wstring etext)
{
	wchar_t qt[query_buffer_len_overflow];
	int currentEntry = 0, totalEntries = wf.size(), percent = 0,len=0;
	int lastSource = _wtoi(etext.c_str());
	for (auto const&[word, wi] : wf)
	{
		if (len == 0)
			len += _snwprintf(qt, query_buffer_len, L"INSERT INTO wordFrequencyMemory (word,totalFrequency,unknownFrequency,capitalizedFrequency,allCapsFrequency,lastSourceId,nonEuropeanFlag,numberFlag,cardinalFlag,ordinalFlag,romanFlag,dateFlag,timeFlag,telephoneFlag,moneyFlag,webaddressFlag) VALUES");
		len += _snwprintf(qt + len, query_buffer_len - len, L" (\"%s\",%d,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)", word.c_str(), wi.totalFrequency, wi.unknownFrequency, wi.unknownCapitalizedFrequency, wi.unknownAllCapsFrequency, lastSource, 
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
		if (len > query_buffer_len_underflow)
		{
			len += _snwprintf(qt + len, query_buffer_len - len, L" ON DUPLICATE KEY UPDATE totalFrequency=totalFrequency+VALUES(totalFrequency),unknownFrequency=unknownFrequency+VALUES(unknownFrequency),capitalizedFrequency=capitalizedFrequency+VALUES(capitalizedFrequency),allCapsFrequency=allCapsFrequency+VALUES(allCapsFrequency),lastSourceId=VALUES(lastSourceId)");
			if (!myquery(mysql, qt)) return;
			len = 0;
		}
		else
			qt[len++] = L',';
	}
	if (len > 0)
	{
		qt[--len] = L' ';
		len += _snwprintf(qt + len, query_buffer_len - len, L" ON DUPLICATE KEY UPDATE totalFrequency=totalFrequency+VALUES(totalFrequency),unknownFrequency=unknownFrequency+VALUES(unknownFrequency),capitalizedFrequency=capitalizedFrequency+VALUES(capitalizedFrequency),allCapsFrequency=allCapsFrequency+VALUES(allCapsFrequency),lastSourceId=VALUES(lastSourceId)");
		if (!myquery(mysql, qt)) return;
	}
}

int analyzeEnd(Source source, int sourceId, wstring path, wstring etext, wstring title,bool &reprocess,bool &nosource)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) 
		return -1;
	if (Words.readWithLock(source.mysql, sourceId, path, false, false) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	if (source.readSource(path, false, false, false))
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

int processCorpusAnalysisForUnknownWords(Source source, int sourceId,wstring path, wstring etext, int step, bool actuallyExecuteAgainstDB)
{
	wchar_t qt[query_buffer_len_overflow];
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) return -1;
	if (Words.readWithLock(source.mysql, sourceId, path, false, false) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	if (source.readSource(path, false, false, false))
	{
		unordered_map<wstring, wordInfo> wf;
		set<wstring> unknownWordsCleared;
		int definedUnknownWord=0, dictionaryComQueried=0, dictionaryComCacheQueried=0;
		int I = 0;
		bool websterAPIRequestsExhausted = false;
		for (WordMatch &im : source.m)
		{
			wstring word = im.word->first;
			if (word.empty() || word[word.length() - 1] == L'\\' || word.find(L'"')!=wstring::npos || word.length()>32)
				continue;
			auto wfi=wf.find(word);
			if (wfi == wf.end())
			{
				wordInfo wi;
				wf[word] = wi;
				wfi = wf.find(word);
				wfi->second.nonEuropeanWord = detectNonEuropeanWord(word);
				wfi->second.number = im.queryForm(NUMBER_FORM_NUM)>=0;
				wfi->second.cardinal = im.queryForm(numeralCardinalForm) >= 0;
				wfi->second.ordinal = im.queryForm(numeralOrdinalForm) >= 0;
				wfi->second.roman = im.queryForm(romanNumeralForm) >= 0;
				wfi->second.date = im.queryForm(dateForm) >= 0;
				wfi->second.time = im.queryForm(timeForm) >= 0;
				wfi->second.telephone = im.queryForm(telephoneNumberForm) >= 0;
				wfi->second.money = im.queryForm(moneyForm) >= 0;
				wfi->second.webaddress = im.queryForm(webAddressForm) >= 0;
			}
			wfi->second.totalFrequency++;
			if (im.word->second.isUnknown())
			{
				if (step == 1)
				{
					wfi->second.unknownFrequency++;
					if (im.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (im.flags&WordMatch::flagFirstLetterCapitalized))
						wfi->second.unknownCapitalizedFrequency++;
					if (im.flags&WordMatch::flagAllCaps)
						wfi->second.unknownAllCapsFrequency++;
				}
				else
				{
					if (unknownWordsCleared.find(word) == unknownWordsCleared.end())
					{
						_snwprintf(qt, query_buffer_len, L"select totalFrequency, unknownFrequency, capitalizedFrequency,allCapsFrequency from wordFrequencyMemory where word=%s", word.c_str());
						MYSQL_RES * result;
						MYSQL_ROW sqlrow = NULL;
						int totalFrequency = 0, capitalizedFrequency = 0, allCapsFrequency = 0;
						if (myquery(&source.mysql, qt, result))
						{
							sqlrow = mysql_fetch_row(result);
							totalFrequency = atoi(sqlrow[0])+ atoi(sqlrow[1]);
							capitalizedFrequency = atoi(sqlrow[2]);
							allCapsFrequency= atoi(sqlrow[3]);
							mysql_free_result(result);
						}
						// find definition in webster.
						// if exists, erase all forms associated with this word and write the new forms (return true)
						// else definition could not be found (return false)
						set <int> posSet;
						bool isNonEuropean, queryOnLowerCase, plural;
						bool caps = im.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (im.flags&WordMatch::flagFirstLetterCapitalized) || (im.flags&WordMatch::flagAllCaps);
						if (getWordPOSInContext(&source.mysql, source, sourceId, im, caps, totalFrequency, capitalizedFrequency, allCapsFrequency, posSet, true, plural, isNonEuropean, queryOnLowerCase, dictionaryComQueried, dictionaryComCacheQueried, websterAPIRequestsExhausted) > 0)
						{
							// remove all wordforms associated with this word and create new wordforms
							if (createWordFormsInDBIfNecessary(source, sourceId, im.word->second.index, word, actuallyExecuteAgainstDB) < 0)
								lplog(LOG_FATAL_ERROR, L"DB error unable to create word %s", word.c_str());
							if (overwriteWordFormsInDB(source, im.word->second.index, word, posSet, actuallyExecuteAgainstDB) < 0)
								lplog(LOG_FATAL_ERROR, L"DB error setting word forms with word %s", word.c_str());
							if (queryOnLowerCase && overwriteWordFlagsInDB(source, im.word->second.index, word, actuallyExecuteAgainstDB) < 0)
								lplog(LOG_FATAL_ERROR, L"DB error setting word flags with word %s", word.c_str());
							int inflections = discoverInflections(posSet, plural, word);
							if (inflections > 0 && overwriteWordInflectionFlagsInDB(source, im.word->second.index, word, inflections, actuallyExecuteAgainstDB) < 0)
								lplog(LOG_FATAL_ERROR, L"DB error setting word inflection flags with word %s", word.c_str());
							definedUnknownWord++;
						}
						// remember word for further sources
						unknownWordsCleared.insert(word);
						wprintf(L"%15.15s:%05d unknown=%06d/%06I64d webster=%06d dictionaryCom(%06d,cache=%06d) [UpperCase=%05.1f] %03I64d%%\r",
							word.c_str(), sourceId, definedUnknownWord, unknownWordsCleared.size(), websterQueriedToday, dictionaryComQueried, dictionaryComCacheQueried,
							((capitalizedFrequency + allCapsFrequency)*100.0 / totalFrequency), I * 100 / source.m.size());
					}
				}
			}
			I++;
		}
		if (step == 1)
			writeWordFrequencyTable(&source.mysql, wf, etext);
	}
	else
	{
		wprintf(L"Unable to read source %s\n", path.c_str());
		return -1;
	}
	return 0;
}

int numSourceLimit = 0;
// to begin proc2 field in all sources must be set to 1
// step = 1 - accumulate word frequency statistics - will set to 2 when finished.
// step = 2 - evaluate statistics and create database statements to decrease the number of unknown words
void wmain(int argc,wchar_t *argv[])
{
	setConsoleWindowSize(200, 15);
	chdir("..");
	initializeCounter();
	cacheDir = CACHEDIR;
	wchar_t *sourceHost = L"localhost";
	enum Source::sourceTypeEnum st = Source::GUTENBERG_SOURCE_TYPE;
	Source source(sourceHost, st, false, false, true);
	source.initializeNounVerbMapping();
	initializePatterns();
	bandwidthControl = CLOCKS_PER_SEC / 2;
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
	bool actuallyExecuteAgainstDB = step==2 && wstring(argv[2])==L"executeAgainstDB";
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	wchar_t qt[query_buffer_len_overflow];
	_snwprintf(qt, query_buffer_len, L"select COUNT(*) from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**'", st);
	__int64 totalSource;
	if (myquery(&source.mysql, qt, result))
	{
		sqlrow = mysql_fetch_row(result);
		totalSource = atoi(sqlrow[0]);
		mysql_free_result(result);
	}
	bool websterAPIRequestsExhausted = false;
	while (true)
	{
		int sourcesLeft = 0;
		_snwprintf(qt, query_buffer_len, L"select COUNT(*) from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d", st,step);
		if (myquery(&source.mysql, qt, result))
		{
			sqlrow = mysql_fetch_row(result);
			sourcesLeft = atoi(sqlrow[0]);
			mysql_free_result(result);
		}
		if (!myquery(&source.mysql, L"START TRANSACTION"))
			return;
		_snwprintf(qt, query_buffer_len, L"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id limit 1 FOR UPDATE SKIP LOCKED", st, step);
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
			_snwprintf(qt, query_buffer_len, L"update sources set proc2=%d where id=%d", setStep, sourceId);
			if (!myquery(&source.mysql, qt))
				break;
		}
		*/
			
		
		if (processCorpusAnalysisForUnknownWords(source, sourceId, path, etext, step, actuallyExecuteAgainstDB)>=0)
		{ 
			_snwprintf(qt, query_buffer_len, L"update sources set proc2=%d where id=%d", step + 1, sourceId);
			if (!myquery(&source.mysql, qt))
				break;
		}
		
		if (!myquery(&source.mysql, L"COMMIT"))
			return;
		source.clearSource();
		wchar_t buffer[1024];
		wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d source (%-35.35s... finished)", (totalSource-(sourcesLeft-1)) * 100 / totalSource, (totalSource - (sourcesLeft - 1)) - 1, totalSource, title.c_str());
		SetConsoleTitle(buffer);
	}
	source.unlockTables();
}


