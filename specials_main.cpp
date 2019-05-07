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
tIWMM wNULL=static_wordMap.begin();
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

int acquireList(wchar_t *filename)
{ 
	LFS
	FILE *listfile=_wfopen(filename,L"rb"); // binary mode reads unicode
	if (listfile)
	{
		wchar_t url[2048],*path;
		int total=0;
		while (fgetws(url,2047,listfile))
		{
			if (url[0]==0xFEFF) // detect BOM
				memcpy(url,url+1,wcslen(url+1));
			if (url[wcslen(url)-1]==L'\n') url[wcslen(url)-1]=0;
			if (url[wcslen(url)-1]==L'\r') url[wcslen(url)-1]=0;
			wchar_t *ch=wcschr(url,L'|');
			*ch=0;
			path=ch+1;
			wchar_t *period=wcsrchr(url,L'.');
			if (period) wcscat(path,period);
			wstring buffer;
			int destfile=_wopen(path,O_RDWR|O_BINARY|O_CREAT);
			if (destfile)
			{
				if (Words.readBinaryPage(url,destfile,total))
					lplog(LOG_ERROR,L"error retrieving %s.",url);
				close(destfile);
			}
			else
				lplog(LOG_ERROR,L"error opening %s.",path);
			wprintf(L"%s - total bytes = %dMB.\n",path,total/1024/1024);
			Sleep(5000);
		}
		fclose(listfile);
	}
	return 0;
}

/*
int reportInfo(WMISupport &provider)
{ LFS
static __int64 lastVirtualBytes=0,lastWorkingSet=0;

IWbemClassObject *resourceNameInstance;
bool status=provider.getResourceObject(L"Win32_PerfFormattedData_PerfProc_Process.Name='lp'",resourceNameInstance);
__int64 percentProcessorTime,percentUserTime,percentPrivilegedTime,elapsedTime,virtualBytes,workingSet;
status = provider.getWMIKey(resourceNameInstance,L"ElapsedTime",elapsedTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentProcessorTime",percentProcessorTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentUserTime",percentUserTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentPrivilegedTime",percentPrivilegedTime);
status = provider.getWMIKey(resourceNameInstance,L"VirtualBytes",virtualBytes);
status = provider.getWMIKey(resourceNameInstance,L"WorkingSet",workingSet);
lplog(L"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
wprintf(L"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d\n",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
lastVirtualBytes=virtualBytes;
lastWorkingSet=workingSet;
return 0;
}
*/

bool exitNow=false,exitEventually=false;

BOOL WINAPI ConsoleHandler(DWORD CEvent)
{ LFS
	if (exitEventually) exitNow=true;
	exitEventually=true;
	switch(CEvent)
	{
	case CTRL_C_EVENT:
		wprintf(L"\nCTRL+C received! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_BREAK_EVENT:
		wprintf(L"\nCTRL+Break received! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_CLOSE_EVENT:
		wprintf(L"\nClose received! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_LOGOFF_EVENT:
		wprintf(L"\nUser is logging off! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_SHUTDOWN_EVENT:
		exitNow=true;
		wprintf(L"\nSystem is shutting down! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	}
	return TRUE;
}

int overallTime;
int initializeCounter(void);
void freeCounter(void);
void reportMemoryUsage(void);
int getInterviewTranscript();
int getTwitterEntries(wchar_t *filter);
bool TSROverride=false,flipTOROverride=false,flipTNROverride=false,flipTMSOverride=false,flipTUMSOverride=false;

void no_memory () {
	lplog(LOG_FATAL_ERROR,L"Out of memory (new/STL allocation).");
	exit (1);
}

void positionConsole(bool controller)
{
	LFS
		CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	// Get the current screen buffer size and window position. 
	if (!GetConsoleScreenBufferInfo(hStdout, &csbiInfo))
		printf("Cannot get console buffer info (%d) %s", (int)GetLastError(), LastErrorStr());
	else if (csbiInfo.dwSize.X < 150 || controller)
	{
		if (!controller)
		{
			csbiInfo.dwSize.X = 150;
			csbiInfo.dwSize.Y = 400;
			if (!SetConsoleScreenBufferSize(hStdout, csbiInfo.dwSize))
				printf("Cannot set console buffer info (%d) %s", (int)GetLastError(), LastErrorStr());
		}
		csbiInfo.srWindow.Bottom = 6;
		csbiInfo.srWindow.Right = 10;
		if (!SetConsoleWindowInfo(hStdout, true, &csbiInfo.srWindow))
			printf("Cannot set console window info (%d) %s", (int)GetLastError(), LastErrorStr());
	}
}

void setConsoleWindowSize(int width,int height)
{
	HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);      // Get Handle 
	_COORD coord;
	coord.X = max(300, width);
	coord.Y = max(6000, height);
	if (!SetConsoleScreenBufferSize(Handle, coord))            // Set Buffer Size 
		printf("Cannot set console buffer info to (%d,%d) (%d) %s\n", coord.X, coord.Y, (int)GetLastError(), LastErrorStr());

	//CONSOLE_SCREEN_BUFFER_INFOEX  csbiInfo;
	//csbiInfo.cbSize = sizeof(csbiInfo);
	// Get the current screen buffer size and window position. 
	//if (!GetConsoleScreenBufferInfoEx(Handle, &csbiInfo))
	//	printf("Cannot get console buffer info (%d) %s\n", (int)GetLastError(), LastErrorStr());
	//_COORD maxSize = GetLargestConsoleWindowSize(Handle);

	//printf("buffer sizex=%d buffer sizey=%d buffer cursor=(%d,%d) \nattributeflags=%d buffer window=(top=%d,left=%d,bottom=%d,right=%d) \nmax given buf=(%d,%d) max absolute=(%d,%d) popupAttributes=%d fullScreen=%d\n", 
	//	csbiInfo.dwSize.X, csbiInfo.dwSize.Y, csbiInfo.dwCursorPosition.X, csbiInfo.dwCursorPosition.Y,
	//	(int)csbiInfo.wAttributes, csbiInfo.srWindow.Top, csbiInfo.srWindow.Left, csbiInfo.srWindow.Bottom, csbiInfo.srWindow.Right,
	//	csbiInfo.dwMaximumWindowSize.X, csbiInfo.dwMaximumWindowSize.Y,
	//	maxSize.X,maxSize.Y,
	//	(int)csbiInfo.wPopupAttributes,(int)csbiInfo.bFullscreenSupported);

	//height = min(height, csbiInfo.dwMaximumWindowSize.Y);
	//width = min(width, csbiInfo.dwMaximumWindowSize.X);


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

int createLPProcess(wstring path, int numProcess, HANDLE &processId, int numSourceLimit, bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite)
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
	wchar_t processParameters[1024];
	if (!path.empty())
		wsprintf(processParameters, L"releasex64\\lp.exe -ParseRequest \"%s\" -cacheDir %s %s%s%s%s-log %d", path.c_str(), CACHEDIR,
		(forceSourceReread) ? L"forceSourceReread " : L"",
			(sourceWrite) ? L"-SW " : L"",
			(sourceWordNetRead) ? L"-SWNR " : L"",
			(sourceWordNetWrite) ? L"-SWNW " : L"",
			numProcess);
	else
		wsprintf(processParameters, L"releasex64\\lp.exe -book 0 + -BC 0 -cacheDir %s %s%s%s%s-numSourceLimit %d -log %d", CACHEDIR,
		(forceSourceReread) ? L"forceSourceReread " : L"",
			(sourceWrite) ? L"-SW " : L"",
			(sourceWordNetRead) ? L"-SWNR " : L"",
			(sourceWordNetWrite) ? L"-SWNW " : L"",
			numSourceLimit,
			numProcess);
	if (!CreateProcess(L"releasex64\\lp.exe",
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

int startProcesses(Source &source, int beginSource, int endSource, Source::sourceTypeEnum st, int maxProcesses, int numSourcesPerProcess, bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite)
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
		if (st == Source::REQUEST_TYPE)
			result = source.getNextUnprocessedParseRequest(prId, pathInCache);
		else
			result = source.getNextUnprocessedSource(beginSource, endSource, st, false, id, path, start, repeatStart, etext, author, title);

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
		if (errorCode = createLPProcess(((st == Source::REQUEST_TYPE) ? pathInCache : L""), nextProcessIndex, processId, numSourcesPerProcess, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite) < 0)
			break;
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

SRWLOCK rdfTypeMapSRWLock,mySQLTotalTimeSRWLock,totalInternetTimeWaitBandwidthControlSRWLock,mySQLQueryBufferSRWLock,orderedHyperNymsMapSRWLock;
void createLocks(void)
{ LFS
	InitializeSRWLock(&rdfTypeMapSRWLock);
	InitializeSRWLock(&mySQLTotalTimeSRWLock);
	InitializeSRWLock(&totalInternetTimeWaitBandwidthControlSRWLock);
	InitializeSRWLock(&mySQLQueryBufferSRWLock);
	InitializeSRWLock(&orderedHyperNymsMapSRWLock);
}

void reparseSource(Source &source)
{
	LFS
	if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE")) 
		return ;
	wchar_t qt[query_buffer_len_overflow];
	_snwprintf(qt, query_buffer_len, L"update sources set processed=false where id=%d ",source.sourceId);
	MYSQL_RES * result;
	myquery(&source.mysql, qt, result);
	mysql_free_result(result);
	source.unlockTables();
}

string lookForPOS(yajl_val node)
{
	const char * path[] = { "fl", (const char *)0 };
	yajl_val docs = yajl_tree_get(node, path, yajl_t_any);
	return (docs == NULL) ? "":YAJL_GET_STRING(docs);
}

int cacheWebPathExists(wstring epath, wstring cacheTypePath)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	_wmkdir(path);
	_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_PATH - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	return _waccess(path, 0) == 0;
}

int cacheWebPath(wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, bool forceWebReread,bool &networkAccessed)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	_wmkdir(path);
	_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_PATH - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	int exitCode = 0, ret, fd;
	if (networkAccessed = forceWebReread || _waccess(path, 0) < 0)
	{
		if (ret = readPage(webAddress.c_str(), buffer)) return ret;
		if ((fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) < 0)
		{
			lplog(LOG_ERROR, L"cacheWebPath:Cannot create path %s - %S.", path, sys_errlist[errno]);
			return GETPAGE_CANNOT_CREATE;
		}
		_write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
		_close(fd);
		return 0;
	}
	else
	{
		if ((fd = _wopen(path, O_RDWR | O_BINARY)) < 0)
			lplog(LOG_ERROR, L"cacheWebPath:Cannot read path %s - %S.", path, sys_errlist[errno]);
		else
		{
			int bufferlen = filelength(fd);
			void *tbuffer = (void *)tcalloc(bufferlen + 10, 1);
			_read(fd, tbuffer, bufferlen);
			_close(fd);
			buffer = (wchar_t *)tbuffer;
			tfree(bufferlen + 10, tbuffer);
		}
	}
	return 0;
}

map<wstring, wstring> websterFormMap = {
	{L"abbreviation",L"abbreviation" },
	{L"adjective",L"adjective"},
	{L"adverb",L"adverb"},
	{L"biographical name",L"Proper Noun"},
	{L"geographical name",L"Proper Noun"},
	{L"noun",L"noun"},
	{L"noun phrase",L"noun"},
	{L"verb",L"verb"},
	{L"interjection",L"interjection"}
};

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

void updateLastSource(int sourceId)
{
	FILE *lastSourceFP = _wfopen(L"lastSourceIdCorpusAnalysis", L"w");
	fwprintf(lastSourceFP, L"%d", sourceId);
	fclose(lastSourceFP);
}

int readLastSource()
{
	int lastSourceId = 0;
	FILE *lastSourceFP = _wfopen(L"lastSourceIdCorpusAnalysis", L"r");
	if (lastSourceFP)
	{
		fscanf(lastSourceFP, "%d", &lastSourceId);
		fclose(lastSourceFP);
	}
	return lastSourceId;
}

// returns false if not found by the site (or error)
bool existsInDictionaryDotCom(wstring word,bool &networkAccessed)
{
	wstring buffer;
	if (cacheWebPath(L"https://www.dictionary.com/browse/" + word, buffer, word, L"DictionaryDotCom", false, networkAccessed))
		return false;
	return buffer.find(L"No results found") == wstring::npos;
}

class WebstersEntry
{
public:
	wstring word, info;
	vector<wstring> wordClasses;
	vector <wstring> definitions;
	wstring getLine(wstring &buffer, int &where, int bufferLen)
	{
		wstring line;
		for (; buffer[where] != '\r' && where < bufferLen; where++)
			line += buffer[where];
		where++;
		if (buffer[where] == '\n')
			where++;
		return line;
	}

	wstring getUntilBlankLine(wstring &buffer, int &where, int bufferLen, bool &firstLineEntirelyUpperCase)
	{
		wstring paragraph;
		while (true)
		{
			wstring line = getLine(buffer, where, bufferLen);

			if (paragraph.empty())
			{
				bool oneLower = false;
				for (int where = 0; where < line.length() && line[where] != '\r'; where++)
					if (oneLower = iswlower(line[where]))
						break;
				firstLineEntirelyUpperCase= !oneLower;
			}

			if (line.empty())
				return paragraph;
			paragraph += line+L" ";
		}
	}

	bool isAllUpper(wstring line)
	{
		for (wchar_t ch : line)
			if (iswlower(ch))
				return false;
		return true;
	}

	int getWebsterEntryWord(wstring &buffer, int &where, wstring &word, int bufferLen)
	{
		wstring line;
		while ((line = getLine(buffer, where, bufferLen)).empty() && where<bufferLen);
		if (!line.empty() && isAllUpper(line))
		{
			word = line;
			return where;
		}
		return -1;
	}

	int getWebsterEntryWordClass(wstring &buffer, int &where, vector<wstring> &wordClasses, int bufferLen)
	{
		bool firstLineEntirelyUpperCase;
		wstring paragraph = getUntilBlankLine(buffer, where, bufferLen,firstLineEntirelyUpperCase);
		int whereComma = paragraph.find(',');
		int whereEtymology = paragraph.find(L"Etym:");
		if (whereEtymology != wstring::npos)
			paragraph.erase(whereEtymology);
		if (whereComma >= 0)
		{
			map<wstring, wstring> websterMapping = {
				{ L" vb.n.",L"verbal noun"},
				{ L" vb. n.",L"verbal noun"},
				{ L" n.",L"noun"},
				{ L" v. t.",L"transitive verb"},
				{ L" v.t.",L"transitive verb"},
				{ L" v.i.",L"intransitive verb"},
				{ L" a.",L"adjective"},
				{ L" adj.",L"adjective"},
				{ L" adv.",L"adverb"},
				{ L" p.p.",L"past participle"},
				{ L" p.a.",L"participial adjective"},
				{ L" p.pr.",L"present participle"},
				{ L" p.ple.",L"present participle"},
				{ L" p.",L"participle"},
				{ L" prep.",L"preposition"},
				{ L" imp.",L"imperfect"},
				{ L" inf.",L"infinitive"},
				{ L" interj.",L"interjection"},
				{ L" acc.",L"accusative"},
			};
			for (auto const&[webster, form] : websterMapping)
				if (paragraph.find(webster, whereComma) != wstring::npos)
				{
					wordClasses.push_back(form);
					break;
				}
		}
		return wordClasses.size();
	}

	bool getWebsterEntryStartsWith(wstring &buffer, wstring searchStr)
	{
		return buffer.substr(0, searchStr.length()) == searchStr;
	}

	bool getWebsterEntryStartsWithNum(wstring &buffer, int &num)
	{
		int I;
		for (I = 0; I < buffer.length() && iswdigit(buffer[I]); I++);
		if (I > 0 && buffer[I] == '.')
		{
			num = _wtoi(buffer.c_str());
			return true;
		}
		return false;
	}

	bool getWebsterEntryWordInfo(wstring &buffer, int &where, vector <wstring> &definitions, int bufferLen)
	{
		int saveWhere = where;
		bool firstLineEntirelyUpperCase;
		wstring paragraph = getUntilBlankLine(buffer, where, bufferLen, firstLineEntirelyUpperCase);
		int num = 1;
		if (getWebsterEntryStartsWith(paragraph, L"Defn:"))
		{
			wprintf(L"DEFN %d:%s\n", where, paragraph.c_str());
			definitions.push_back(paragraph);
			return true;
		}
		if (getWebsterEntryStartsWith(paragraph, L"Note:"))
		{
			wprintf(L"NOTE %d:%s\n", where, paragraph.c_str());
			return true;
		}
		if (getWebsterEntryStartsWith(paragraph, L"Syn."))
		{
			wprintf(L"SYN %d:%s\n", where, paragraph.c_str());
			//definitions.push_back(paragraph);
			return true;
		}
		if (getWebsterEntryStartsWithNum(paragraph, num))
		{
			wprintf(L"NUM %d:%s\n", where, paragraph.c_str());
			definitions.push_back(paragraph);
			return true;
		}
		if (!firstLineEntirelyUpperCase)
		{
			wprintf(L"ADD %d:%s\n", where, paragraph.c_str());
			return true;
		}
		//if (paragraph.find(L"Etym:") != wstring::npos)
		//{
		//	wprintf(L"ETYM %d:%s\n", where, paragraph.c_str());
		//	return true;
		//}
		where = saveWhere;
		return false;
	}

	WebstersEntry(wstring &buffer, int &where, int bufferLen, bool &reachedEnd)
	{
		// search for a line with all caps
		where = getWebsterEntryWord(buffer, where, word, bufferLen);
		if (reachedEnd=where < 0) return;
		getWebsterEntryWordClass(buffer, where, wordClasses, bufferLen);
		if (reachedEnd = where < 0) return;
		wprintf(L"\n*** %s %s\n", word.c_str(), (wordClasses.size() > 0) ? wordClasses[0].c_str() : L"X");
		while (getWebsterEntryWordInfo(buffer, where, definitions, bufferLen));
		reachedEnd = false;
	}
};

#define WebsterStart L"Produced by Graham Lawrence"
int readUnabridgedGutenbergWebstersDictionary()
{
	nounForm = 1;
	verbForm=2;
	adjectiveForm=3;
	adverbForm=4;
	prepositionForm=5;

	int bookhandle = _wopen(L"J:\\caches\\texts\\Webster, Noah\\Unabridged Dictionary.txt", O_RDWR | O_BINARY);
	if (bookhandle < 0)
	{
		lplog(LOG_ERROR, L"ERROR:Unable to open dictionary - %S", _sys_errlist[errno]);
		return -1;
	}
	int bufferLen;
	char *dictionaryBuffer = (char *)tmalloc((size_t)(bufferLen = _filelength(bookhandle)) + 10);
	bufferLen = _read(bookhandle, dictionaryBuffer, (unsigned int)bufferLen);
	_close(bookhandle);
	if (bufferLen < 0 || bufferLen == 0)
		return -1;
	dictionaryBuffer[bufferLen + 1] = 0;
	wstring buffer;
	mTW(dictionaryBuffer, buffer);
	int where=buffer.find(WebsterStart);
	if (where == wstring::npos)
		return -1;
	where += wcslen(WebsterStart);
	int numWordEntries = 0;
	bool reachedEnd = false;
	vector <WebstersEntry> entries;
	while (!reachedEnd)
	{
		entries.push_back(WebstersEntry(buffer, where, bufferLen,reachedEnd));
		numWordEntries++;
	}
	map <wstring, int> formCounts;
	for (auto we : entries)
	{
		if (we.wordClasses.size()>0)
			formCounts[we.wordClasses[0]]++;
		else
			formCounts[L"unknown"]++;
	}
	wprintf(L"Webster Entries found:%d\n", numWordEntries);
	for (auto const&[form, count] : formCounts)
		wprintf(L"Form %s:%d\n", form.c_str(), count);
	return 0;
}

bool detectNonEuropeanWord(wstring word, char *buffer, int bufferlen)
{
	BOOL usedDefaultChar;
	int ret = WideCharToMultiByte(
		1252,									//UINT CodePage,
		WC_NO_BEST_FIT_CHARS,//DWORD dwFlags,
		word.c_str(),				 //LPCWCH lpWideCharStr,
		word.length(),			 //int cchWideChar,
		buffer,              //LPSTR lpMultiByteStr,
		bufferlen,           // int cbMultiByte,
		NULL,                //LPCCH lpDefaultChar,
		&usedDefaultChar										 //LPBOOL lpUsedDefaultChar
	);
	return (ret == 0) || (usedDefaultChar);
	//if (!ret)
	//	wprintf(L"%s %d\n", word.c_str(), GetLastError());
	//else if (usedDefaultChar)
	//	wprintf(L"%s UDC\n", word.c_str());
	//else
	//	return false;
	//return true;
}

/*
// map to class/classes
Italian adverb
adjective or adverb
adverb or adjective
adjective
adjective combining form
German adjective
verb
phrasal verb
impersonal verb
Latin verb
pronoun, singular or plural in construction
noun
noun phrase
noun or adjective
adjective or noun
noun, plural in form but singular in construction
noun, plural in form but singular or plural in construction
French noun phrase
Latin noun phrase
plural noun
French noun
noun combining form
noun or intransitive verb
adverb
interjection
French interjection
abbreviation
Latin abbreviation
symbol
preposition
conjunction
trademark
pronoun
pronoun or adjective
honorific title

// ignore
prefix
noun suffix
adverb suffix
adjective suffix
verb suffix
plural noun suffix
Greek phrase
French phrase
Latin phrase
German phrase
Spanish phrase
Italian phrase
Irish phrase
Hawaiian phrase
proverbial saying
conventional saying
French quotation from ...
Latin quotation from ...
German quotation from ...
pronunciation spelling
script annotation
combining form

// custom
idiom
biographical name
geographical name
certification mark
communications code word
communications signal
*/
vector<wstring> ignoreBefore = { L"pronunciation spelling"};
vector<wstring> classes = { L"adjective",L"adverb",L"verb",L"noun",L"interjection",L"abbreviation",L"symbol",L"preposition",L"conjunction",L"trademark",L"pronoun",L"honorific"};
vector<wstring> ignoreAfter = { L"prefix",L"suffix",L"phrase",L"saying",L"quotation",L"pronunciation spelling",L"script annotation",L"combining form" }; // must be processed after classes

void identifyFormClass(set<int> &posSet, wstring pos,boolean &plural)
{
	plural = pos.find(L"noun") != wstring::npos && pos.find(L"plural") != wstring::npos;
	if (pos == L"idiom")
	{
		posSet.insert(FormsClass::gFindForm(L"noun"));
		posSet.insert(FormsClass::gFindForm(L"adjective"));
	}
	else if (pos.find(L"name") != wstring::npos)
	{
		posSet.insert(PROPER_NOUN_FORM_NUM);
	}
	else if (pos==L"certification mark")
	{
		posSet.insert(FormsClass::gFindForm(L"symbol"));
	}
	else if (pos.find(L"communications") != wstring::npos)
	{
posSet.insert(FormsClass::gFindForm(L"noun"));
	}
	for (auto c : ignoreBefore)
		if (pos.find(c.c_str()) != wstring::npos)
			return;
	for (auto c : classes)
	{
		int where;
		if ((where = pos.find(c.c_str())) != wstring::npos)
		{
			// this is enough to differentiate between noun, pronoun, verb and adverb
			if (where == 0 || iswspace(pos[where - 1]))
				posSet.insert(FormsClass::gFindForm(c.c_str()));
		}
	}
	for (auto c : ignoreAfter)
		if (pos.find(c.c_str()) != wstring::npos)
			return;
	if (posSet.empty())
		printf("form not recognized - %S", pos.c_str());
}

int getWordPOS(wstring word, set <int> &posSet, boolean &plural, boolean print, boolean &isNonEuropean, int &dictionaryComQueried, int &dictionaryComCacheQueried, boolean &websterAPIRequestsExhausted)
{
	LFS
		char temptransbuf[1024];
	if (isNonEuropean = detectNonEuropeanWord(word, temptransbuf, 1024))
		return 0;
	bool networkAccessed, existsDM = existsInDictionaryDotCom(word, networkAccessed);
	if (networkAccessed)
		dictionaryComQueried++;
	else
		dictionaryComCacheQueried++;
	if (!existsDM || websterAPIRequestsExhausted)
		return 0;
	networkAccessed = false;
	wstring pageURL = L"https://www.dictionaryapi.com/api/v3/references/collegiate/json/";
	pageURL += word + L"?key=ba4ac476-dac1-4b38-ad6b-fe36e8416e07";
	wstring jsonWideBuffer;
	if (!cacheWebPath(pageURL, jsonWideBuffer, word, L"Webster", false, networkAccessed))
	{
		char errbuf[1024];
		errbuf[0] = 0;
		string jsonBuffer;
		wTM(jsonWideBuffer, jsonBuffer);
		yajl_val node = yajl_tree_parse((const char *)jsonBuffer.c_str(), errbuf, sizeof(errbuf));
		/* parse error handling */
		if (node == NULL) {
			lplog(LOG_ERROR, L"Parse error:%s\n %S", jsonBuffer.c_str(), errbuf);
			return -1;
		}
		wstring posStr, pos;
		if (node->type == yajl_t_array)
			for (unsigned int docNum = 0; docNum < node->u.array.len; docNum++)
			{
				yajl_val doc = node->u.array.values[docNum];
				mTW(lookForPOS(doc), pos);
				if (pos.length() > 0)
					identifyFormClass(posSet, pos, plural);
			}
		if (print)
		{
			if (posStr.length() > 0)
				wprintf(L"%s:%s\n", word.c_str(), posStr.c_str());
			else if (posSet.empty())
			{
				wprintf(L"Unknown:%s\n", word.c_str());
				posSet.insert(UNDEFINED_FORM_NUM);
			}
		}
	}
	if (networkAccessed && !MWRequestAllowed())
	{
		websterAPIRequestsExhausted = true;
	}
	return 0;
}

int getWordPOSInContext(MYSQL *mysql,Source &source, int sourceId, WordMatch &word, wstring originalWord, int numUnknownWord, int numUnknownOriginalWord, int numUnknownAllCaps, set <int> &posSet,
	bool print, boolean &plural, boolean &isNonEuropean, boolean &queryOnLowerCase, int &dictionaryComQueried, int &dictionaryComCacheQueried, boolean &websterAPIRequestsExhausted)
{
	LFS
	// numUnknownAllCaps is zero if originalWord is already all caps.
	if (queryOnLowerCase = (numUnknownWord > 5 && ((numUnknownOriginalWord + numUnknownAllCaps)*100.0 / numUnknownWord) > 95.0 && iswupper(originalWord[0])))
	{
		if (print)
			lplog(LOG_INFO, L"Designated Proper Noun %s [numUnknownWord=%d][numUnknownOriginalWord=%d]", originalWord.c_str(), numUnknownWord, numUnknownOriginalWord);
		//posSet.insert(PROPER_NOUN_FORM_NUM);
		posSet.insert(FormsClass::gFindForm(L"noun"));
	}
	else
		getWordPOS(word.word->first, posSet, plural, false, isNonEuropean, dictionaryComQueried, dictionaryComCacheQueried, websterAPIRequestsExhausted);
	if (posSet.size() > 0 && print)
	{
		wstring posStr;
		for (int form : posSet)
			posStr += L" " + Forms[form]->name;
		lplog(LOG_INFO, L"Known Word %s: forms (%s)", originalWord.c_str(), posStr.c_str());
	}
	if (posSet.empty())
	{
		tIWMM iWord = Words.end();
		int ret;
		if (iswupper(originalWord[0]) || (ret = Words.attemptDisInclination(mysql, iWord, word.word->first, sourceId)))
		{
			if (ret = Words.splitWord(mysql, iWord, word.word->first, sourceId))
			{
				wstring sWord= word.word->first;
				sWord.erase(std::remove(sWord.begin(), sWord.end(), L'-') , sWord.end());
				if (Words.query(sWord)==Words.end())
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

int createWordFormsInDBIfNecessary(Source source, int sourceId, int wordId, wstring word, wstring originalWord, bool actuallyExecuteAgainstDB)
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
			lplog(LOG_INFO, L"DB statement [%s create word]: %s", originalWord.c_str(), qt);
	}
	return 0;
}


int overwriteWordFormsInDB(Source source, int wordId, wstring originalWord, set <int> posSet, bool actuallyExecuteAgainstDB)
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
		lplog(LOG_INFO, L"DB statement [%s delete]: %s", originalWord.c_str(), qt);
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
		lplog(LOG_INFO, L"DB statement [%s forms insert]: %s", originalWord.c_str(), qt);
	return 0;
}

// queryOnLowerCase will query for word forms  the next time the word is encountered in all lower case.
// queryOnLowerCase = 4
int overwriteWordFlagsInDB(Source source, int wordId, wstring originalWord, bool actuallyExecuteAgainstDB)
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
		lplog(LOG_INFO, L"DB statement [%s add queryOnLowerCase flag]: %s", originalWord.c_str(), qt);
	return 0;
}

// pass back these inflections:
// if NOUN in posSet:
// SINGULAR = 1, PLURAL = 2
// if VERB in posSet:
// VERB_PAST = 16, VERB_PAST_PARTICIPLE = 32, VERB_PRESENT_PARTICIPLE = 64,
// VERB_PRESENT_THIRD_SINGULAR = 128, VERB_PRESENT_FIRST_SINGULAR = 256,
// VERB_PAST_THIRD_SINGULAR = 512, VERB_PAST_PLURAL = 1024,
// VERB_PRESENT_PLURAL = 2048, VERB_PRESENT_SECOND_SINGULAR = 4096,
// if ADJECTIVE in posSet:
// ADJECTIVE_NORMATIVE = 8192, ADJECTIVE_COMPARATIVE = 16384,ADJECTIVE_SUPERLATIVE = 32768,
// if ADVERB in posSet:
// ADVERB_NORMATIVE = 65536, ADVERB_COMPARATIVE = 131072,ADVERB_SUPERLATIVE = 262144
int discoverInflections(set <int> posSet, boolean plural, wstring word)
{
	int nounForm = FormsClass::gFindForm(L"noun");
	int verbForm = FormsClass::gFindForm(L"verb");
	int adjectiveForm = FormsClass::gFindForm(L"adjective");
	int adverbForm = FormsClass::gFindForm(L"adverb");
	for (auto c : posSet)
	{
		if (c == adjectiveForm)
		{

		}
	}
	return 0;
}

// PLURAL refers to noun plural form.
int overwriteWordInflectionFlagsInDB(Source source, int wordId, wstring originalWord, int inflections, bool actuallyExecuteAgainstDB)
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
		lplog(LOG_INFO, L"DB statement [%s add inflection flag]: %s", originalWord.c_str(), qt);
	return 0;
}

wchar_t *commonUnknownWords[]=
{ 
L"pale-faces", L"chouette", L"huish", L"zikali", L"sambo", L"lanty", L"bounderby", L"bibber", L"pradelle",
L"loudon", L"belding", L"mitchy", L"babbalanja", L"shanter", L"wenceslas", L"deemster", L"whipple", L"cargrim",
L"marlow", L"purvis", L"finot", L"conroy", L"carden", L"marais", L"hadley", L"septimius", L"tête-à-tête",
L"didna", L"pale-face", L"williamson", L"long-lost", L"huns", L"kincaid", L"wyvern", L"hunterleys", L"westmore",
L"faversham", L"ludovico", L"girty", L"joses", L"amory", L"worshiped", L"byrne", L"merwyn", L"arf",
L"station-master", L"priam", L"gilman", L"rigby", L"edouard", L"decoud", L"fontainebleau", L"hippolyte", L"hurons",
L"ahab", L"ferrier", L"brookes", L"pasmer", L"bedad", L"readin", L"india-rubber", L"sharpe", L"magsie",
L"marthy", L"pendle", L"thirty-three", L"kidd", L"wuss", L"waldron", L"larsen", L"vandeford", L"lessingham",
L"grenfell", L"appleton", L"bloomin", L"ogram", L"kezia", L"angeles", L"swanhild", L"hathaway", L"derry",
L"totty", L"balfour", L"tubbs", L"suffolk", L"madre", L"half-mile", L"leyden", L"sunshiny", L"brutus",
L"zululand", L"hella", L"minchin", L"loire", L"ferrars", L"durand", L"watkins", L"ormond", L"dade",
L"walpole", L"flanagan", L"hawkeye", L"leverett", L"marsay", L"montalais", L"croesus", L"lawd", L"hawes",
L"pollnitz", L"kilmeny", L"thorold", L"apple-tree", L"rinkitink", L"northumberland", L"blackguards", L"grier", L"cochrane",
L"watering-place", L"garnache", L"balzac", L"seh", L"burge", L"haviland", L"kaunitz", L"raymonde", L"ravenel",
L"arrah", L"keziah", L"queen-mother", L"transvaal", L"ascott", L"ossaroo", L"leonidas", L"burnham", L"pilate",
L"vermont", L"oakdale", L"kennicott", L"dyck", L"bickley", L"so-and-so", L"dagaeoga", L"nebber", L"syme",
L"bridgenorth", L"seti", L"driscoll", L"dinwiddie", L"round-up", L"bevan", L"rojas", L"sechard", L"o'malley",
L"tonio", L"newhaven", L"glendinning", L"siegmund", L"ailsa", L"gustavus", L"anna-rose", L"country-house", L"povey",
L"goualeuse", L"tarrypin", L"hamil", L"lawry", L"murdock", L"kane", L"poindexter", L"greenland", L"dieppe",
L"gervase", L"irishmen", L"glastonbury", L"woodville", L"featherstone", L"dmitri", L"baines", L"knowles", L"peterborough",
L"cass", L"lensky", L"reception-room", L"hee", L"agen", L"ronicky", L"pyotr", L"seville", L"cadiz",
L"maxence", L"androvsky", L"bellingham", L"stannard", L"glyddyr", L"bavaria", L"wickham", L"farquhar", L"simpkins",
L"corentin", L"belfast", L"ransford", L"knee-deep", L"timéa", L"macgregor", L"bayliss", L"ulick", L"black-eyed",
L"breckenridge", L"hycy", L"copperfield", L"doone", L"severn", L"weems", L"wragge", L"wrandall", L"cardoville",
L"headley", L"thirty-eight", L"trevannion", L"bragelonne", L"hardwick", L"jervis", L"coulee", L"godefroid", L"foi",
L"wordsworth", L"lassiter", L"twere", L"spicer", L"jacopo", L"fontenoy", L"pavlovna", L"réné", L"tandakora",
L"canadians", L"nayland", L"d'arcy", L"gaspard", L"newgate", L"pearce", L"mcdonald", L"saint-aignan", L"feemy",
L"berwick", L"dysart", L"longstreet", L"athenians", L"geordie", L"sah-luma", L"yarmouth", L"sulaco", L"luxembourg",
L"ansell", L"lundie", L"meldrum", L"purcell", L"leroux", L"callin", L"bed-chamber", L"racksole", L"bram",
L"three-cornered", L"guard-house", L"kaffirs", L"shandon", L"flynn", L"manton", L"macleod", L"forty-two", L"osman",
L"ravenslee", L"able-bodied", L"crewe", L"mctee", L"baudraye", L"comanches", L"cayley", L"rylton", L"coupeau",
L"rhys", L"aphrodite", L"lidgerwood", L"wardour", L"ukridge", L"lawton", L"riccabocca", L"werry", L"stormont",
L"pilkington", L"self-interest", L"glaire", L"greene", L"mowgli", L"hurst", L"goesler", L"grandet", L"pepe",
L"mcguire", L"sunlocks", L"bianchon", L"emlyn", L"phelps", L"djalma", L"bruff", L"coorse", L"falkner",
L"neefit", L"lablache", L"whut", L"hephzy", L"nate", L"hamley", L"rossitur", L"farnham", L"carnaby",
L"tullia", L"bourdon", L"phonny", L"foker", L"timmendiquas", L"heinz", L"jeannette", L"selby", L"baisemeaux",
L"denbigh", L"miki", L"mayhew", L"dickson", L"netta", L"corbett", L"bowles", L"graustark", L"pertinax",
L"long-legged", L"uncas", L"nuttie", L"ninety-nine", L"hinpoha", L"richardson", L"maître", L"nottingham", L"wemyss",
L"vautrin", L"cephas", L"denas", L"dias", L"conde", L"woodhouse", L"crozier", L"anglo-saxon", L"galbraith",
L"johnston", L"stedman", L"ratcliffe", L"michigan", L"nassau", L"andre-louis", L"isaacs", L"gualtier", L"violante",
L"thayer", L"ingleborough", L"burgoyne", L"anna-felicitas", L"boyle", L"chahda", L"winn", L"jevons", L"tallente",
L"wodehouse", L"galway", L"lancashire", L"parlin", L"olympus", L"drennen", L"helbeck", L"custom-house", L"glenarm",
L"hanover", L"tuck'n", L"keswick", L"baltic", L"charteris", L"effendi", L"rhodes", L"betts", L"felicite",
L"rodman", L"patterson", L"eb", L"dashwood", L"alexandrovitch", L"cranston", L"moreau", L"wolfenden", L"roby",
L"hottentots", L"inga", L"rothesay", L"perth", L"lowington", L"musgrave", L"diaz", L"inger", L"brodie",
L"wycherly", L"parisians", L"umballa", L"thirlwell", L"lindau", L"byng", L"mutimer", L"logotheti", L"sanderson",
L"normans", L"old-world", L"ill-fated", L"whitefoot", L"pandora", L"grimaud", L"jeekie", L"bright-eyed", L"ongar",
L"steinmetz", L"paragot", L"copplestone", L"railsford", L"colonna", L"boat-house", L"cept", L"bartle", L"pett",
L"rosey", L"stapleton", L"budd", L"ostend", L"tanqueray", L"thrums", L"d'aigrigny", L"virginie", L"eugénie",
L"quisanté", L"nejdanov", L"kendric", L"co-operation", L"valois", L"neptune", L"sea-shore", L"tuyn", L"uz",
L"casaubon", L"marie-anne", L"wickersham", L"markland", L"jonesville", L"pompey", L"redmond", L"swinburne", L"leavenworth",
L"helmsley", L"lazarus", L"lytton", L"cuffy", L"croix", L"manor-house", L"marcello", L"perez", L"pinckney",
L"fischer", L"vargrave", L"minturn", L"langley", L"chupin", L"high-born", L"trenck", L"dupont", L"fentolin",
L"messire", L"persians", L"carker", L"señora", L"wabi", L"lewisham", L"speakin", L"bigley", L"kingozi",
L"hubbard", L"memphis", L"leddy", L"spence", L"arabin", L"montmartre", L"bimeby", L"jaffery", L"norgate",
L"gerty", L"byrd", L"staines", L"buckboard", L"ceylon", L"lovell", L"sapt", L"foley", L"seaforth",
L"holme", L"madelaine", L"lansing", L"tyrolese", L"hanson", L"wingfield", L"hendricks", L"hemstead", L"blois",
L"gunther", L"kennyfeck", L"saladin", L"ursus", L"guerchard", L"spouter", L"rousseau", L"dudleigh", L"ogden",
L"burleigh", L"farnsworth", L"connecticut", L"tappitt", L"cytherea", L"wilcox", L"duval", L"nostromo", L"bibbs",
L"nevill", L"shadrach", L"gotzkowsky", L"ruggles", L"l'estrange", L"beaumesnil", L"glyn", L"saratoga", L"vot",
L"gunson", L"thick-set", L"inca", L"waife", L"fête", L"hindu", L"endicott", L"vizard", L"ganimard",
L"dunham", L"winnipeg", L"mahommed", L"head-quarters", L"frewen", L"cornelli", L"heyward", L"beardsley", L"shelby",
L"self-love", L"popinot", L"bab", L"marneffe", L"furneaux", L"ravenswood", L"diogenes", L"broughton", L"mulford",
L"vaux", L"birkin", L"bellevite", L"slocum", L"ebenezer", L"nesta", L"frisco", L"phelim", L"tarling",
L"bar-room", L"herrick", L"corbet", L"mustapha", L"one-eyed", L"petrie", L"whitaker", L"wingrave", L"beppo",
L"loftus", L"wynn", L"worcester", L"o't", L"palestine", L"samoa", L"bathsheba", L"gudruda", L"indiana",
L"britons", L"monsignor", L"montcalm", L"meldon", L"californian", L"cerizet", L"festing", L"ranny", L"mullins",
L"bo'sun", L"faulkner", L"whitford", L"chinamen", L"marya", L"conyers", L"dumont", L"denys", L"merwell",
L"maryland", L"mdlle", L"rossi", L"maldon", L"chilian", L"latimer", L"colambre", L"ice-cream", L"corney",
L"make-up", L"wendover", L"tete-a-tete", L"pale-faced", L"d'harville", L"ayesha", L"self-contained", L"boarding-school", L"bargeton",
L"clif", L"northwick", L"masther", L"bonner", L"selingman", L"darsie", L"coryston", L"jeeves", L"hartmut",
L"home-made", L"rockies", L"tressady", L"bethune", L"hill-side", L"tuk", L"siward", L"cal'late", L"mohun",
L"rolfe", L"malipieri", L"nuremberg", L"ethelberta", L"thor", L"nehushta", L"phoebus", L"isaacson", L"vyner",
L"self-esteem", L"injin", L"dawes", L"mahomet", L"weber", L"lalage", L"t'ink", L"vatican", L"bengal",
L"yorke", L"grimes", L"maguire", L"faustina", L"hottentot", L"willem", L"ermengarde", L"dantes", L"kinney",
L"bridgie", L"mohawk", L"swartboy", L"four-and-twenty", L"coconnas", L"vandeloup", L"doria", L"tresler", L"lucetta",
L"bursley", L"boyne", L"reardon", L"school-master", L"baptist", L"grosset", L"haines", L"cassy", L"flavia",
L"tell-tale", L"pringle", L"vancouver", L"montagu", L"holbrook", L"counting-house", L"westy", L"briscoe", L"wery",
L"crosby", L"tressilian", L"brudenell", L"zenobia", L"villiers", L"dingaan", L"forrester", L"huguenot", L"twemlow",
L"unorna", L"imaginings", L"oregon", L"salem", L"hansei", L"tarrant", L"yasmini", L"isis", L"carnac",
L"bixiou", L"carmichael", L"puttin", L"hamel", L"elfride", L"francie", L"collingwood", L"harmer", L"table-cloth",
L"brighteyes", L"charnock", L"eversleigh", L"clo", L"newland", L"mayberry", L"hanny", L"bennington", L"caxton",
L"galusha", L"whitehall", L"blondet", L"blood-red", L"lemme", L"farrington", L"marsham", L"dobbin", L"nyoda",
L"merrifield", L"calyste", L"ingram", L"biggs", L"jewess", L"pericles", L"hagar", L"courcy", L"layin",
L"malchus", L"wide-awake", L"mariette", L"amsterdam", L"deringham", L"o'halloran", L"lodging-house", L"huntin", L"lorand",
L"fleur-de-marie", L"cabot", L"harman", L"morrel", L"gibbs", L"laverick", L"manisty", L"seagrave", L"hangin",
L"razumov", L"blakeney", L"davenport", L"rostov", L"hearn", L"alban", L"plato", L"brodrick", L"riddell",
L"lecount", L"faix", L"socrates", L"sellingworth", L"moya", L"ascher", L"tulliver", L"lil", L"acton",
L"goethe", L"brownlow", L"richelieu", L"mathieu", L"raskolnikov", L"buxton", L"honoria", L"allerdyke", L"rutherford",
L"caryll", L"sixty-five", L"selden", L"sherlock", L"daniels", L"griswold", L"peggotty", L"sowerby", L"herminie",
L"thackeray", L"charing", L"farley", L"dickenson", L"comstock", L"bosinney", L"victorine", L"ritz", L"shaddy",
L"smithson", L"emmeline", L"fosdick", L"bohemia", L"corydon", L"durham", L"senhor", L"dominey", L"lisbon",
L"stepan", L"heldre", L"modeste", L"japhet", L"hague", L"leave-taking", L"westover", L"wos", L"henley",
L"grandon", L"schmidt", L"genoese", L"henshaw", L"sacramento", L"fu-manchu", L"zealand", L"jacko", L"all-powerful",
L"maulevrier", L"henchard", L"tea-time", L"wagner", L"venters", L"gedge", L"mctaggart", L"lyin", L"florent",
L"sence", L"ulrich", L"out-of-doors", L"haynes", L"frying-pan", L"great-grandfather", L"burnett", L"isak", L"graeme",
L"ridley", L"ginevra", L"ashby", L"good-for-nothing", L"prayer-book", L"tembarom", L"didn", L"blacky", L"averil",
L"shepard", L"scipio", L"matthews", L"ralston", L"dunstan", L"courtland", L"henty", L"ferice", L"maraton",
L"laurent", L"rufford", L"aram", L"muller", L"lizzy", L"saracinesca", L"gowan", L"richling", L"ill-luck",
L"baree", L"jenks", L"porte", L"gascoigne", L"havana", L"siberia", L"une", L"stevenson", L"alwyn",
L"dios", L"greenwich", L"osborn", L"atter", L"romayne", L"horton", L"wyoming", L"gott", L"havre",
L"lannes", L"beautrelet", L"robespierre", L"rastignac", L"illinois", L"cobb", L"ivanovitch", L"selim", L"madelon",
L"b'lieve", L"algernon", L"mahony", L"duncombe", L"talboys", L"bostil", L"curdie", L"shawanoe", L"m'sieur",
L"drayton", L"athalie", L"tennyson", L"vavasor", L"gascon", L"nucingen", L"gardiner", L"wegg", L"ailie",
L"school-room", L"texans", L"han", L"gwynplaine", L"ritson", L"mukoki", L"marlowe", L"rienzi", L"almayer",
L"virgil", L"camusot", L"quintana", L"wilmet", L"zorzi", L"cross-roads", L"court-house", L"for'ard", L"lavretsky",
L"ojo", L"vronsky", L"south-east", L"goodwin", L"plummer", L"rowley", L"edmonstone", L"well-being", L"layton",
L"ransome", L"oui", L"thomson", L"o'neil", L"sicilian", L"danvers", L"townsend", L"wrayson", L"schomberg",
L"leighton", L"lorimer", L"hackney", L"bombay", L"mountjoy", L"goot", L"gillespie", L"guapo", L"noll",
L"thady", L"standish", L"alexey", L"marlborough", L"baird", L"pemberton", L"denry", L"tatham", L"bulstrode",
L"beresford", L"snuff-box", L"deerslayer", L"barby", L"ome", L"mormon", L"proudie", L"sholto", L"a'most",
L"huron",L"beasley",L"hobbs",L"ferrers",L"bartholomew",L"endymion",L"caldigate",L"pertell",L"mansfield",L"flemish",L"viner",L"ivo",L"lascelles",L"cesar",
L"bennet",L"red-headed",L"onondaga",L"jinny",L"lilias",L"lockwood",L"tyrol",L"bank-notes",
L"button-bright",L"euphemia",L"señorita",L"tavernake",L"wetzel",L"fightin",L"batch",L"dalrymple",
L"avenel",L"baden",L"dacre",L"mckay",L"white-faced",L"winslow",L"danglars",L"gaspar",
L"court-martial",L"charlton",L"carthage",L"elsmere",L"zane",L"christendom",L"danube",L"roche",
L"peru",L"thuillier",L"concha",L"claudius",L"niagara",L"ferrand",L"willett",L"dark-eyed",
L"barlow",L"hungary",L"amelius",L"mcchesney",L"tartarin",L"ball-room",L"guiche",L"sartin",
L"bellew",L"cox",L"crevel",L"coventry",L"dresden",L"hawtrey",L"blue-eyed",L"lettice",
L"larkin",L"ellinor",L"persia",L"school-house",L"absent-minded",L"hatton",L"squeers",L"langham",
L"earle",L"alaska",L"barchester",L"ripton",L"mysie",L"haldane",L"guillaume",L"antwerp",
L"dunlap",L"saxons",L"peterson",L"garcia",L"phillida",L"martindale",L"bayard",L"cain",
L"dinny",L"mainwaring",L"geoff",L"slone",L"staunton",L"portugal",L"simmons",L"underhill",
L"north-east",L"yates",L"arkwright",L"thim",L"etcetera",L"cocoa-nut",L"melbourne",L"wallingford",
L"chetwynde",L"wiggins",L"hickman",L"gude",L"vittoria",L"sunday-school",L"dubois",L"alvarez",
L"luttrell",L"capitola",L"davidson",L"falkirk",L"thirty-two",L"robarts",L"congo",L"saltash",
L"mitya",L"amabel",L"natur",L"jael",L"louisiana",L"packard",L"vanstone",L"dryfoos",
L"spargo",L"carstairs",L"hurd",L"europeans",L"varney",L"hughie",L"bingle",L"harvard",
L"ballard",L"kells",L"wimmen",L"armadale",L"knox",L"keene",L"raby",L"françois",
L"harrigan",L"rouen",L"sabine",L"goddard",L"wur",L"berenger",L"malays",L"macumazahn",
L"ill-will",L"aristide",L"abbott",L"dagobert",L"osmond",L"waddington",L"lydgate",L"meynell",
L"jeffreys",L"cibot",L"phipps",L"nevil",L"hamlin",L"lina",L"drawing-rooms",L"make-believe",
L"arsène",L"waring",L"ranjoor",L"reilly",L"exeter",L"petrovitch",L"clennam",L"alleyne",
L"hume",L"pacha",L"horatio",L"smithers",L"ivanovna",L"zara",L"gideon",L"texan",
L"algiers",L"weller",L"anson",L"sweetwater",L"rooney",L"crosbie",L"trap-door",L"eames",
L"wyllard",L"tennessee",L"rawdon",L"devereux",L"chichester",L"peyrade",L"malone",L"peabody",
L"yussuf",L"cadurcis",L"ellison",L"crocker",L"travilla",L"fontaine",L"gif",L"sahwah",
L"iver",L"greif",L"luc",L"surrey",L"agricola",L"templeton",L"apaches",L"stuyvesant",
L"passford",L"nickleby",L"furnival",L"graydon",L"swithin",L"belton",L"grandcourt",L"lynde",
L"dunstable",L"strether",L"crabtree",L"farm-house",L"kazan",L"donnegan",L"frankfort",L"fair-haired",
L"planchet",L"bellamy",L"matty",L"carbury",L"good-evening",L"warburton",L"gibraltar",L"mas'r",
L"kim",L"delaware",L"hendrik",L"cosette",L"rôle",L"maddy",L"gower",L"melrose",
L"canterbury",L"dorrit",L"lumley",L"pennington",L"aide-de-camp",L"injuns",L"doña",L"betsey",
L"baldwin",L"tuileries",L"keller",L"stirling",L"archy",L"south-west",L"schmucke",L"umslopogaas",
L"p'r'aps",L"hosses",L"mebby",L"pocket-handkerchief",L"barnaby",L"p'raps",L"sedley",L"wolfe",
L"thirty-six",L"kansas",L"galloway",L"sho",L"gascoyne",L"babbitt",L"beric",L"kearney",
L"vos",L"beaufort",L"rayner",L"white-haired",L"valliere",L"hampshire",L"renshaw",L"chiltern",
L"copley",L"erskine",L"pierrette",L"second-hand",L"syd",L"etienne",L"fenn",L"bathurst",
L"two-thirds",L"good-humour",L"jurgis",L"wilkinson",L"conniston",L"calvert",L"massachusetts",L"self-denial",
L"hallam",L"babylon",L"y'u",L"amherst",L"annele",L"frobisher",L"shep",L"thyrza",
L"houghton",L"esau",L"annesley",L"valjean",L"death-bed",L"roddy",L"rodd",L"davies",
L"hawthorne",L"belford",L"peveril",L"grosvenor",L"sheba",L"forsyte",L"berkeley",L"jethro",
L"olivier",L"gilmore",L"langdon",L"heah",L"allus",L"devonshire",L"stevens",L"richards",
L"putney",L"caspar",L"bartlett",L"fleetwood",L"lapham",L"calcutta",L"columbia",L"hazelton",
L"gittin",L"ruthven",L"beecher",L"villefort",L"boulogne",L"hulot",L"gwyn",L"vaughan",
L"bradshaw",L"one-half",L"rickman",L"beaton",L"grantly",L"breakfast-table",L"glencora",L"colville",
L"selwyn",L"norway",L"howland",L"navarre",L"twenty-seven",L"atherton",L"hayes",L"calais",
L"père",L"dutton",L"window-sill",L"bracy",L"ramsay",L"givin",L"bowen",L"lothair",
L"coningsby",L"trevelyan",L"o'grady",L"lancelot",L"lecoq",L"colbert",L"castlewood",L"southampton",
L"tita",L"tancred",L"odo",L"jemima",L"isoult",L"north-west",L"pierson",L"humouredly",
L"buckingham",L"tea-table",L"twenty-eight",L"levi",L"fyne",L"twenty-six",L"sandford",L"stratton",
L"cañon",L"poland",L"shif'less",L"baltimore",L"verena",L"godolphin",L"sicily",L"yit",
L"swinton",L"mclean",L"birotteau",L"artois",L"kemp",L"every-day",L"scarborough",L"livingstone",
L"armitage",L"chauvelin",L"austrians",L"sewell",L"c'est",L"cornish",L"normandy",L"walpurga",
L"pendleton",L"hôtel",L"heathcote",L"sullivan",L"old-time",L"bray",L"atkins",L"dorn",
L"shefford",L"note-book",L"wilmot",L"lousteau",L"theer",L"eyelashes",L"silverbridge",L"deronda",
L"stancy",L"lor",L"berners",L"self-sacrifice",L"neville",L"belgium",L"lufton",L"gresham",
L"elspeth",L"higgins",L"challoner",L"orsino",L"jabez",L"dulcie",L"dalzell",L"common-sense",
L"leetle",L"carrington",L"beatrix",L"norfolk",L"red-haired",L"gibbie",L"five-and-twenty",L"cristo",
L"jesuit",L"haf",L"darry",L"mowbray",L"calhoun",L"nasmyth",L"colorado",L"ripley",
L"alyosha",L"mazarin",L"marse",L"chicot",L"o'hara",L"prohack",L"alix",L"mildmay",
L"orme",L"sor",L"stanbury",L"brighton",L"egbert",L"commander-in-chief",L"reynolds",L"roden",
L"egyptians",L"fairfield",L"mannering",L"maltravers",L"melchior",L"ferguson",L"josè",L"zeb",
L"turnbull",L"smallbones",L"gloucester",L"anthea",L"kirkwood",L"pearson",L"venetia",L"on'y",
L"gwendolen",L"bedford",L"scotchman",L"m'sieu",L"gil",L"monmouth",L"waverley",L"zulus",
L"kenelm",L"lightfoot",L"purty",L"newport",L"melmotte",L"thyrsis",L"adolphe",L"sabin",
L"braddock",L"mid-day",L"wyndham",L"cuba",L"bentley",L"versailles",L"lingard",L"o'connor",
L"egremont",L"iroquois",L"rodin",L"courtenay",L"josé",L"fenton",L"ayala",L"glasgow",
L"alessandro",L"carew",L"ranald",L"barclay",L"pecksniff",L"zillah",L"halliday",L"darrow",
L"esq",L"good-day",L"grande",L"upton",L"apollo",L"marseilles",L"rochester",L"forty-eight",
L"wulf",L"churchill",L"hannibal",L"forster",L"heyst",L"o'reilly",L"starr",L"reg'lar",
L"i'se",L"salisbury",L"marilla",L"franks",L"alick",L"gwen",L"twenty-three",L"gaul",
L"martie",L"imogen",L"amyas",L"arundel",L"verloc",L"hervey",L"unquestionably",L"effingham",
L"georges",L"tak",L"abbé",L"boarding-house",L"kensington",L"egerton",L"meetin",L"summer-house",
L"gryce",L"capt",L"snow-white",L"dinna",L"boone",L"coralie",L"penrod",L"nugent",
L"brougham",L"hebrew",L"baas",L"granger",L"far-away",L"well-dressed",L"jenkins",L"brewster",
L"browne",L"virginian",L"psmith",L"lisbeth",L"public-house",L"jupiter",L"gaspare",L"dixon",
L"stanhope",L"lucilla",L"alaric",L"merrick",L"half-breed",L"prudy",L"waitin",L"gus",
L"rodolph",L"juno",L"market-place",L"armine",L"mustang",L"vous",L"eben",L"seventy-five",
L"hyar",L"cairo",L"lesbia",L"plymouth",L"orde",L"ludlow",L"levin",L"italians",
L"ashe",L"ames",L"mynheer",L"athens",L"gwynne",L"sonnenkamp",L"locke",L"camp-fire",
L"saul",L"overton",L"hereward",L"cavendish",L"loring",L"suh",L"jist",L"aggie",
L"lancaster",L"red-hot",L"deerfoot",L"katrine",L"seein",L"bristol",L"fleming",L"pendennis",
L"claus",L"montreal",L"beauchamp",L"hopkins",L"elfreda",L"markham",L"fulkerson",L"gould",
L"godwin",L"remus",L"putnam",L"cromwell",L"sylvie",L"clancy",L"arizona",L"dombey",
L"cleopatra",L"lovel",L"harcourt",L"cumberland",L"lenz",L"willet",L"bartley",L"flanders",
L"blount",L"pshaw",L"fräulein",L"stafford",L"marchmont",L"esmond",L"looking-glass",L"aylmer",
L"gorman",L"bessy",L"pollyanna",L"gridley",L"forty-five",L"portsmouth",L"arabian",L"powell",
L"conway",L"ag'in",L"daly",L"steele",L"w'at",L"crusoe",L"launcelot",L"potts",
L"digby",L"fraser",L"knowest",L"warwick",L"hawkins",L"dinsmore",L"briggs",L"harrington",
L"marius",L"somers",L"café",L"lopez",L"ohio",L"griggs",L"jolyon",L"naturedly",
L"tish",L"w'en",L"wilkins",L"ze",L"greece",L"grizel",L"livin",L"morrison",
L"palliser",L"good-will",L"marcy",L"fairfax",L"lyon",L"charmian",L"newcome",L"château",
L"neale",L"marston",L"dunbar",L"dunn",L"breton",L"barrington",L"fitzgerald",L"wingate",
L"fenwick",L"prussia",L"constantinople",L"evan",L"peterkin",L"mississippi",L"albany",L"leicester",
L"mordaunt",L"collins",L"cashel",L"linton",L"meredith",L"cecily",L"grandmamma",L"audley",
L"beltane",L"h'm",L"lena",L"mexicans",L"sioux",L"havin",L"montfort",L"à",
L"quarter-deck",L"switzerland",L"pee-wee",L"twenty-one",L"westminster",L"letty",L"dere",L"fitz",
L"noah",L"phillips",L"dodd",L"wa'n",L"zoe",L"sah",L"nic",L"pitt",
L"twenty-two",L"obed",L"philippa",L"fergus",L"francois",L"tavia",L"self-respect",L"everard",
L"simpson",L"thirty-five",L"forbes",L"cæsar",L"sinclair",L"sampson",L"hammond",L"jed",
L"lovelace",L"first-class",L"denham",L"fouquet",L"ozma",L"gervaise",L"maisie",L"greeks",
L"macdonald",L"greg",L"t'other",L"so-called",L"pickwick",L"harlowe",L"perkins",L"maitland",
L"clarke",L"lennox",L"singh",L"hoss",L"benson",L"half-a-dozen",L"dublin",L"edinburgh",
L"vanslyperken",L"wharton",L"boers",L"arabella",L"middleton",L"quixote",L"tayoga",L"pocket-book",
L"first-rate",L"russians",L"sancho",L"leon",L"bolton",L"somerset",L"d'you",L"marmaduke",
L"barnabas",L"stubbs",L"saunders",L"reade",L"humphrey",L"wiggily",L"gibson",L"winthrop",
L"drummond",L"more'n",L"jerusalem",L"reddy",L"philippe",L"o'brien",L"far-off",L"shakespeare",
L"good-morning",L"susy",L"arter",L"hampton",L"dressing-room",L"hermione",L"tristram",L"clive",
L"vere",L"aint",L"duc",L"yuh",L"mlle",L"armstrong",L"crawley",L"mebbe",
L"by-and-by",L"liverpool",L"warrington",L"well-nigh",L"texas",L"athos",L"wentworth",L"satan",
L"post-office",L"middle-aged",L"clavering",L"willoughby",L"austria",L"porthos",L"bassett",L"frenchmen",
L"sha'n",L"aramis",L"roberts",L"rosamund",L"self-control",L"arm-chair",L"señor",L"merriwell",
L"orleans",L"phineas",L"look-out",L"cuthbert",L"bonaparte",L"montague",L"ther",L"barnes",
L"milly",L"sophy",L"christians",L"mexico",L"ez",L"soames",L"fleda",L"bobbsey",
L"venice",L"ses",L"englishmen",L"hastings",L"d'ye",L"robinson",L"cap'n",L"baxter",
L"jem",L"hetty",L"california",L"half-way",L"chicago",L"s'pose",L"holmes",L"eustace",
L"germans",L"africa",L"sez",L"well-known",L"egypt",L"twenty-four",L"spaniards",L"good-natured",
L"thet",L"thar",L"prescott",L"d'artagnan",L"half-past",L"twenty-five",L"germany",L"bert",
L"good-by", L"ireland", L"old-fashioned", L"rollo", L"good-night", L"wuz", L"drawing-room", L"mary", L"law", L"english",
NULL };

void testWordList()
{
	map <int, int> posFrequency;
	int dictionaryComQueried=0, dictionaryComCacheQueried=0;
	for (int I = 0; commonUnknownWords[I]; I++)
	{
		set <int> posSet;
		boolean isNonEuropean,plural, websterAPIRequestsExhausted;
		getWordPOS(commonUnknownWords[I], posSet,plural,true, isNonEuropean, dictionaryComQueried, dictionaryComCacheQueried, websterAPIRequestsExhausted);
		for (int wordForm : posSet)
			posFrequency[wordForm]++;
	}
	for (auto const&[wordForm, wordFormFrequency] : posFrequency)
		wprintf(L"%30s:%d\n", Forms[wordForm]->name.c_str(), wordFormFrequency);
}

void writeMapReverseSorted(unordered_map<wstring, int> &words,wchar_t *outputFilePath)
{
	std::vector<std::pair<wstring, int>> pairs;
	for (auto itr = words.begin(); itr != words.end(); ++itr)
		pairs.push_back(*itr);

	sort(pairs.begin(), pairs.end(), [=](std::pair<wstring, int>& a, std::pair<wstring, int>& b)
	{
		return a.second < b.second;
	});
	FILE *fp = NULL;
	_wfopen_s(&fp, outputFilePath, L"w,ccs=UNICODE");
	if (fp)
	{
		for (std::pair<wstring, int> itr : pairs)
		{
			fwprintf(fp, L"%-30s:%d\n", itr.first.c_str(), itr.second);
		}
		fclose(fp);
	}
}

int readMap(wchar_t *mapFilePath, unordered_map<wstring, int> &frequencyMap)
{
	FILE *mapFile = NULL;
	_wfopen_s(&mapFile, mapFilePath, L"rb,ccs=UNICODE");
	if (mapFile)
	{
		wchar_t word[500], buffer[1024];
		int frequency,line=0;
		for (; !feof(mapFile); line++)
		{
			if (fgetws(buffer, 1024, mapFile) != buffer && ferror(mapFile))
			{
				printf("%d:%S\n", line,_wcserror(NULL));
				continue;
			}
			wchar_t *colon=wcschr(buffer, L':');
			if (colon)
			{
				wchar_t *num = colon + 1;
				colon--;
				while (iswspace(*colon)) colon--;
				colon++;
				wcsncpy(word, buffer, colon - buffer);
				word[colon - buffer] = 0;
				frequency = _wtoi(num);
				frequencyMap[word] = frequency;
			}
			else
				printf("%ls(%d):Invalid - %ls!\n", mapFilePath, line, buffer);
		}
		fclose(mapFile);
		printf("%I64d words found in %d lines in file %ls\n", frequencyMap.size(), line,mapFilePath);
		return 1;
	}
	return 0;
}

int readFrequencyMap(wchar_t *mapFilePath, unordered_map<wstring, int> &frequencyMap,bool writeReversedMap)
{
	FILE *mapFile=NULL;
	_wfopen_s(&mapFile,mapFilePath, L"rb,ccs=UNICODE");
	char transtempbuf[1024];
	unordered_map <wstring, int> sourceNonEuropean;
	if (mapFile)
	{
		wchar_t buffer[1024],lastSource[1024];
		for (int line=0; !feof(mapFile); line++)
		{
			if (fgetws(buffer, 1024, mapFile) != buffer && ferror(mapFile))
			{
				printf("%d:%S\n", line, _wcserror(NULL));
				continue;
			}
			bool error = true;
			wchar_t *lastSpace = wcsrchr(buffer, L' ');
			if (lastSpace)
			{
				lastSpace--;
				while (lastSpace!=buffer && *lastSpace != L' ')
					lastSpace--;
				if (lastSpace!=buffer)
				{
					error = false;
					int frequency = _wtoi(lastSpace + 1);
					if (frequency)
					{
						*lastSpace = 0;
						frequencyMap[buffer] = frequency;
						if (detectNonEuropeanWord(buffer, transtempbuf, 1024))
							sourceNonEuropean[lastSource]++;
					}
				}
			}
			if (error)
				printf("%ls[%d]:Invalid line - %ls\n", mapFilePath, line, buffer);
		}
		fclose(mapFile);
		printf("%I64d words found in file %ls\n", frequencyMap.size(), mapFilePath);
		writeMapReverseSorted(sourceNonEuropean,L"Non European Word Count by Source.txt");
		return 1;
	}
	return 0;
}

int writeFrequencyMap(wchar_t *mapFilePath, unordered_map<wstring, int> &frequencyMap, unordered_map<wstring, wstring> &lastSourceForUnknownWord)
{
	FILE *mapFile = NULL;
	_wfopen_s(&mapFile,mapFilePath, L"wb,ccs=UNICODE");
	if (mapFile)
	{
		for (auto const&[word, wordFrequency] : frequencyMap)
		{
			auto itr = lastSourceForUnknownWord.find(word);
			wstring lastSource = (itr== lastSourceForUnknownWord.end()) ? L"X" : itr->second;
			fwprintf(mapFile, L"%s %d %s\n", word.c_str(), wordFrequency,lastSource.c_str());
		}
		fclose(mapFile);
		return 1;
	}
	return 0;
}

void getWordCapitalization(WordMatch m,wstring &out)
{
	out = m.word->first;
	if (m.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m.flags&WordMatch::flagFirstLetterCapitalized))
		out[0] = towupper(out[0]);
	//if (m.flags&WordMatch::flagNounOwner)
	//	out += L"'s";
	if (m.flags&WordMatch::flagAllCaps)
		for (int len=0; len<out.length(); len++) out[len] = towupper(out[len]);
}

void wordAnalysis(unordered_map<wstring, int> &unknownOriginalWords, unordered_map<wstring, int> &unknownWords)
{
	map <wstring, double> designatedMap;
	for (auto word : unknownOriginalWords)
	{
		int numUnknownOriginalWord = unknownOriginalWords[word.first];
		wstring lw = word.first;
		for (int len = 0; len < lw.length(); len++) lw[len] = towlower(lw[len]);
		int numUnknownWord = unknownWords[lw];
		if (numUnknownWord > 30 && lw!=word.first)
		{
			//lplog(LOG_INFO, L"Designated Proper Noun %s [numUnknownWord=%d][numUnknownOriginalWord=%d]", word.first.c_str(), numUnknownWord, numUnknownOriginalWord);
			designatedMap[word.first] = (100.0*numUnknownOriginalWord) / numUnknownWord;
		}

	}
	std::vector<std::pair<wstring, double>> pairs;
	for (auto itr = designatedMap.begin(); itr != designatedMap.end(); ++itr)
		pairs.push_back(*itr);

	sort(pairs.begin(), pairs.end(), [=](std::pair<wstring, double>& a, std::pair<wstring, double>& b)
	{
		return a.second < b.second;
	});
	FILE *fp = NULL;
	_wfopen_s(&fp,L"Designated results.txt", L"w,ccs=UNICODE");
	if (fp)
	{
		for (std::pair<wstring, double> itr : pairs)
		{
			fwprintf(fp, L"%-30s:%f\n", itr.first.c_str(), itr.second);
		}
		fclose(fp);
	}
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

void writeFrequenciesToDB(Source source)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words WRITE"))
		return;
	unordered_map<wstring, int> frequencyMap;
	unordered_map<wstring, int> frequencyMapNotFound;
	if (readMap(L"word frequency.txt", frequencyMap))
	{
		__int64 wordsWritten=0;
		for (auto const&[word, wordFrequency] : frequencyMap)
		{
			if (word.find(L'"') == wstring::npos)
			{
				wchar_t qt[query_buffer_len_overflow];
				_snwprintf(qt, query_buffer_len, L"update words set frequency=%d where word=\"%s\"", wordFrequency, word.c_str());
				if (!myquery(&source.mysql, qt,true))
				{
					printf("ERROR: %S", qt);
				}
				__int64 rowsUpdated = mysql_affected_rows(&source.mysql);
				if (rowsUpdated != 1)
				{
					frequencyMapNotFound[word] = wordFrequency;
					if (wordFrequency>1417)
						printf("Not found:%-30S:%d\n", word.c_str(),wordFrequency);
				}
				else
					wordsWritten ++;
			}
		}
		printf("Words updated: %I64d - words not in dictionary: %I64d", wordsWritten,frequencyMapNotFound.size());
	}
}

// if a word has frequency<=1, and is unknown (select wordId from wordforms where formId=1), OR is not a european word, remove it.
// this must be done after the rewrite of the forms (overwriteWordFormsInDB).
void removeUnknownWordsFromDB(Source source)
{
	if (!myquery(&source.mysql, L"LOCK TABLES words READ"))
		return;
	//wchar_t qt[query_buffer_len_overflow];
	//_snwprintf(qt, query_buffer_len, L"select id, etext, path, title from sources where sourceType=%d and id>=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' order by id",
	//	st, lastSourceId);
	int dictionaryComQueried = 0, dictionaryComCacheQueried = 0;
	//if (myquery(&source.mysql, qt, result))
	//{

}

// this is to accumulate how webster describes word forms in its "fl" field, to properly map to lp forms.
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

int numSourceLimit = 0;
void main()
{
		//testDisinclination();
	setConsoleWindowSize(200, 15);
	//readUnabridgedGutenbergWebstersDictionary();
	chdir("..");
	initializeCounter();
	cacheDir = CACHEDIR;
	wchar_t *sourceHost = L"localhost";
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
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
	unordered_map<wstring, int> unknownWords, unknownOriginalWords,wordFrequency;
	unordered_map<wstring, wstring> lastSourceForUnknownWord;
	HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
	bool actuallyExecuteAgainstDB = false;
	if (std_out == INVALID_HANDLE_VALUE) {
		printf("stdout not available");
		return;
	}
	/* test webster
	unordered_set<wstring> pos;
	int numFilesProcessed = 0;
	scanAllWebsterEntries(L"J:\\caches\\webster", pos, numFilesProcessed);
	for (wstring psi : pos)
	{
		boolean plural = false;
		set <int> posSet;
		wstring forms;
		identifyFormClass(posSet, psi, plural);
		for (int form : posSet)
			forms += Forms[form]->name + L" ";
		printf("%S:%S %S\n", psi.c_str(), forms.c_str(), (plural) ? L"PLURAL" : L"");
	}
	 end test webster
	*/
	//writeFrequenciesToDB(source);
	//if (true)
	//	return;
	int loop = 0;
	int nonEuropeanWords = 0;
	if (readFrequencyMap(L"unknownSummaryResultsFileUW.txt", unknownWords,true) &&
		readFrequencyMap(L"unknownSummaryResultsFileOW.txt", unknownOriginalWords,false))
		loop = 1;
	for (; loop < 2; loop++)
	{
		wchar_t qt[query_buffer_len_overflow];
		int lastSourceId = (loop==0) ? 0 : readLastSource();
		_snwprintf(qt, query_buffer_len, L"select id, etext, path, title from sources where sourceType=%d and id>=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' order by id",
			st, lastSourceId);
		int dictionaryComQueried = 0, dictionaryComCacheQueried = 0;
		boolean websterAPIRequestsExhausted = false;
		if (myquery(&source.mysql, qt, result))
		{
			__int64 totalSource = mysql_num_rows(result)+lastSourceId;
			set<wstring> unknownWordsCleared, definedUnknownWord;
			set<int> sourcesToReparse;
			for (int is = 1; (sqlrow = mysql_fetch_row(result)); is++)
			{
				wstring path, etext, title;
				int sourceId = atoi(sqlrow[0]);
				if (loop==1)
					updateLastSource(sourceId);
				if (sqlrow[1] == NULL)
					etext = L"NULL";
				else
					mTW(sqlrow[1], etext);
				mTW(sqlrow[2], path);
				mTW(sqlrow[3], title);
				path.insert(0, L"\\").insert(0, CACHEDIR);
				if (!myquery(&source.mysql, L"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) return;
				if (Words.readWithLock(source.mysql, sourceId, path, false, false) < 0)
					lplog(LOG_FATAL_ERROR, L"Cannot read dictionary.");
				Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
				char transtempbuf[1024];
				if (source.readSource(path, false, false, false))
				{
					int I = 0;
					for (WordMatch &im : source.m)
					{
						wordFrequency[im.word->first]++;
						if (im.word->second.isUnknown())
						{
							if (loop==0)
							{
								detectNonEuropeanWord(im.word->first,transtempbuf,1024);
								unknownWords[im.word->first]++;
								wstring originalWord;
								getWordCapitalization(im, originalWord);
								unknownOriginalWords[originalWord]++;
								//wstring inflectionFlags, wordFlags;
								//allFlags(im.word->second.inflectionFlags, inflectionFlags);
								//allWordFlags(im.word->second.flags, wordFlags);
								//wstring owIF = originalWord + L":"+inflectionFlags + L":" + wordFlags;
								//owIF.erase(std::remove(owIF.begin(), owIF.end(), ' '), owIF.end()); 
								//unknownOriginalWordsWithInflectionsFlags[owIF]++;
								lastSourceForUnknownWord[im.word->first] = etext;
							}
							else
							{
								if (unknownWordsCleared.find(im.word->first) == unknownWordsCleared.end())
								{
									// find definition in webster.
									// if exists, erase all forms associated with this word and write the new forms (return true)
									// else definition could not be found (return false)
									int numUnknownWord = unknownWords[im.word->first];
									wstring originalWord;
									getWordCapitalization(im, originalWord);
									int numUnknownOriginalWord=unknownOriginalWords[originalWord];
									int numUnknownAllCaps = 0;
									if ((im.flags&WordMatch::flagFirstLetterCapitalized) && !(im.flags&WordMatch::flagAllCaps))
									{
										wstring allCaps = im.word->first;
										for (int len = 0; len < allCaps.length(); len++) allCaps[len] = towupper(allCaps[len]);
										auto aci = unknownOriginalWords.find(allCaps);
										if (aci!= unknownOriginalWords.end())
											numUnknownAllCaps = aci->second;
									}
									//wstring inflectionFlags, wordFlags;
									//allFlags(im.word->second.inflectionFlags, inflectionFlags);
									//allWordFlags(im.word->second.flags, wordFlags);
									//int numUnknownOriginalWordsWithInflectionsFlags=unknownOriginalWordsWithInflectionsFlags[originalWord + inflectionFlags + L" " + wordFlags];
									set <int> posSet;
									boolean isNonEuropean, queryOnLowerCase, plural;
									if (getWordPOSInContext(&source.mysql,source, sourceId, im, originalWord, numUnknownWord, numUnknownOriginalWord, numUnknownAllCaps, posSet,true, plural, isNonEuropean, queryOnLowerCase, dictionaryComQueried, dictionaryComCacheQueried, websterAPIRequestsExhausted)>0)
									{
										// remove all wordforms associated with this word and create new wordforms
										if (createWordFormsInDBIfNecessary(source, sourceId, im.word->second.index, im.word->first, originalWord, actuallyExecuteAgainstDB)<0)
											lplog(LOG_FATAL_ERROR, L"DB error unable to create word %s", originalWord.c_str());
										if (overwriteWordFormsInDB(source, im.word->second.index, originalWord, posSet, actuallyExecuteAgainstDB) < 0)
											lplog(LOG_FATAL_ERROR, L"DB error setting word forms with word %s",originalWord.c_str());
										if (queryOnLowerCase && overwriteWordFlagsInDB(source, im.word->second.index, originalWord, actuallyExecuteAgainstDB) < 0)
											lplog(LOG_FATAL_ERROR, L"DB error setting word flags with word %s", originalWord.c_str());
										int inflections = discoverInflections(posSet,plural,im.word->first);
										if (inflections>0 && overwriteWordInflectionFlagsInDB(source, im.word->second.index, originalWord, inflections, actuallyExecuteAgainstDB) < 0)
											lplog(LOG_FATAL_ERROR, L"DB error setting word inflection flags with word %s", originalWord.c_str());
										// sources containing any of these words must be reparsed
										definedUnknownWord.insert(im.word->first);
									}
									if (isNonEuropean)
										nonEuropeanWords++;
									// remember word for further sources
									unknownWordsCleared.insert(im.word->first);
									wprintf(L"%30.30s:numSources=(%05I64d,%05d) unknown=%06I64d out of %06I64d(nonEuropean=%05d) webster=%06d dictionaryCom(%06d,cache=%06d) [unknownUpperCase ratio=%08.4f]\r",
										originalWord.c_str(),sourcesToReparse.size(), sourceId, definedUnknownWord.size(), unknownWordsCleared.size(),nonEuropeanWords, websterQueriedToday, dictionaryComQueried, dictionaryComCacheQueried,
										((numUnknownOriginalWord+numUnknownAllCaps)*100.0 / numUnknownWord));
								}
								if (definedUnknownWord.find(im.word->first) != definedUnknownWord.end())
									sourcesToReparse.insert(sourceId);
							}
						}
						I++;
					}
				}
				source.clearSource();
				wchar_t buffer[1024];
				wsprintf(buffer, L"%%%03I64d:%5d out of %05I64d source (%-35.35s... finished)", (is+ lastSourceId) * 100 / totalSource, is+ lastSourceId, totalSource, title.c_str());
				SetConsoleTitle(buffer);
				//DWORD numWritten;
				//WriteConsole(std_out, buffer, wcslen(buffer), &numWritten, NULL);
			}
			if (loop == 0)
			{
				writeFrequencyMap(L"unknownSummaryResultsFileUW.txt", unknownWords, lastSourceForUnknownWord);
				writeFrequencyMap(L"unknownSummaryResultsFileOW.txt", unknownOriginalWords, lastSourceForUnknownWord);
				writeMapReverseSorted(unknownWords, L"unknown results.txt");
				writeMapReverseSorted(wordFrequency, L"word frequency.txt");
				wordAnalysis(unknownOriginalWords,unknownWords);
			}
		}
	}
	mysql_free_result(result);
	source.unlockTables();
}


