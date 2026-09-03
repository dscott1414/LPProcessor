/*
	Internet.cpp - WinINet HTTP GET, cache-aside web fetch, and Jericho HTML-to-text spawn

	Overview:
		The only first-party HTTP client.  LPInternetOpen() creates one process-wide
		HINTERNET (preconfig proxy, 3s connect timeout, 100 conns/server) and
		installs InternetStatusCallback (which records redirectUrl).  readPage()
		rate-limits on bandwidthControl, GET-s the URL, and reads the body on a
		worker thread (InternetReadFile_Wait, 5-minute timeout) so a hung read
		can be abandoned.  cacheWebPath / getWebPath hash the URL onto
		CACHEDIR/WEBSEARCH_CACHEDIR and only hit the network on a miss.
		runJavaJerichoHTML() launches a local Java RenderToText helper via
		redirected pipes.

	Pipeline position:
		Stage 8 (QA web search / Wikipedia / SPARQL) and the get*.cpp scrapers.

	Key entry points:
		- readPage / readBinaryPage / cacheWebPath / getWebPath
		- LPInternetOpen / closeConnection / InetOption
		- InternetReadFile_Wait / InternetReadFile_Child
		- runJavaJerichoHTML / PrepAndLaunchRedirectedChild / ReadAndHandleOutput

	Notes / gotchas:
		- On timeout InternetReadFile_Wait closes the request handle to unblock the
			child, then waits for it to exit before returning; the child writes into
			the caller's stack, so it must not outlive the call.
		- SPARQL failures now `continue` the existing errors<internetWebSearchRetryAttempts
			loop after a 30s sleep instead of recursing into readPage with no depth
			cap; a Virtuoso outage now gives up after the normal retry budget instead
			of retrying forever / risking a stack overflow.
		- cacheWebPath writes wchar_t as binary (UTF-16) and reads it back as
			wchar_t*.  getWebPath in the `clean` path writes UTF-8, then
			assigns the file bytes to a wstring as wchar_t* - encoding mismatch.
		- getWebPath now truncates path (a MAX_LEN==2048 wchar_t buffer) at
			MAX_LEN-20, matching cacheWebPath; it used to truncate at MAX_PATH-20
			(240), so two distinct long URLs whose first 240 chars matched
			collided on the same cache file.  The #ifdef TEST_CODE testWebPath
			twin had the same bug; fixed there too.
		- Hardcoded LMAINDIR chdir for the Java helper; hINet is process-wide
			and reset to 0 on some errors without a lock.
		- getWebPath used to `return 0` unconditionally after a fresh fetch,
			in both the !clean and (ret && clean) paths, discarding readPage's
			result.  Several callers check the return value for ==0 / <0 to
			detect a failed fetch (createOntology.cpp, questionAnsweringWebSearch.cpp),
			so a network failure was being reported as success and could leave
			an empty file permanently cached (a later call sees the path exists
			and never re-fetches).  Now returns `ret`, matching cacheWebPath's
			`if (ret = readPage(...)) return ret;`.  Behaviour-changing: callers
			that previously always saw 0 from getWebPath on a fresh-fetch path
			now see the real readPage failure code.
*/
#pragma warning (disable: 4503)
#pragma warning (disable: 4996)

#include <errno.h>
#include <windows.h>
#include "WinInet.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "errno.h"
#include <direct.h>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
using namespace std;

#include <stdio.h>
#include "internet.h"
#include "logging.h"
#include "profile.h"
#include "mysql.h"
#include "mysqldb.h"
#include "general.h"

const wchar_t* getLastErrorMessage(wstring& out);
void* cInternet::hINet;
int cInternet::bandwidthControl;
struct _RTL_SRWLOCK cInternet::totalInternetTimeWaitBandwidthControlSRWLock;
wstring cInternet::redirectUrl;

// GET 'str' into buffer (headers unused).  Returns 0 or a negative
// INTERNET_* / GETWEBPATH_* code.
int cInternet::readPage(const wchar_t* str, wstring& buffer)
{
	LFS
		wstring headers;
	return readPage(str, buffer, headers);
}

// Query INTERNET_OPTION `option` and, if it is not already `value`, set it.
// global==true uses NULL (process default); false uses hINet.  Always
// returns true; failures are only logged.
bool cInternet::InetOption(bool global, int option, const wchar_t* description, unsigned long value)
{
	LFS
		HINTERNET hI = (global) ? 0 : hINet;
	unsigned long qValue = value;
	DWORD len = sizeof(qValue);
	wstring inett;
	if (!InternetQueryOption(hI, option, &qValue, &len))
		lplog(LOG_ERROR, L"ERROR:InternetQueryOption of (%s) Failed - %s", description, getLastErrorMessage(inett));
	if (value != qValue)
	{
		lplog(LOG_INFO, L"%s set to %d from %d.", description, value, qValue);
		qValue = value;
		if (!InternetSetOption(hI, option, &qValue, sizeof(qValue)))
			lplog(LOG_ERROR, L"ERROR:InternetSetOption of (%s) Failed - %s", description, getLastErrorMessage(inett));
	}
	return true;
}

// WinINet status callback.  The only live arm records INTERNET_STATUS_REDIRECT
// into the process-wide redirectUrl (and optionally logs it).
void cInternet::InternetStatusCallback(
	HINTERNET, // hInternet
	DWORD_PTR, // dwContext
	DWORD dwInternetStatus,
	LPVOID lpvStatusInformation,
	DWORD // dwStatusInformationLength
)
{
	switch (dwInternetStatus)
	{
		//case INTERNET_STATUS_CLOSING_CONNECTION:
		//	lplog(LOG_INFO, L"status:%s", L"Closing the connection to the server."); break;
		//case INTERNET_STATUS_CONNECTED_TO_SERVER:
		//	lplog(LOG_INFO, L"Successfully connected to the socket address(SOCKADDR) pointed to by lpvStatusInformation."); break;
		//case INTERNET_STATUS_CONNECTING_TO_SERVER:
		//	lplog(LOG_INFO, L"Connecting to the socket address(SOCKADDR) pointed to by lpvStatusInformation."); break;
		//case INTERNET_STATUS_CONNECTION_CLOSED:
		//	lplog(LOG_INFO, L"Successfully closed the connection to the server."); break;
		//case INTERNET_STATUS_COOKIE_HISTORY:
		//	lplog(LOG_INFO, L"Retrieving content from the cache.Contains data about past cookie events for the URL such as if cookies were accepted, rejected, downgraded, or leashed."); break;
		//case INTERNET_STATUS_COOKIE_RECEIVED:
		//	lplog(LOG_INFO, L"Indicates the number of cookies that were accepted, rejected, downgraded(changed from persistent to session cookies), or leashed(will be sent out only in 1st party context).The lpvStatusInformation parameter is a DWORD with the number of cookies received."); break;
		//case INTERNET_STATUS_COOKIE_SENT:
		//	lplog(LOG_INFO, L"Indicates the number of cookies that were either sent or suppressed, when a request is sent.The lpvStatusInformation parameter is a DWORD with the number of cookies sent or suppressed."); break;
		//case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
		//	lplog(LOG_INFO, L"Not implemented."); break;
		//case INTERNET_STATUS_DETECTING_PROXY:
		//	lplog(LOG_INFO, L"Notifies the client application that a proxy has been detected."); break;
		//case INTERNET_STATUS_HANDLE_CLOSING:
		//	lplog(LOG_INFO, L"This handle value has been terminated.pvStatusInformation contains the address of the handle being closed.The lpvStatusInformation parameter contains the address of the handle being closed."); break;
		//case INTERNET_STATUS_HANDLE_CREATED:
		//	lplog(LOG_INFO, L"Used by InternetConnect to indicate it has created the new handle.This lets the application call InternetCloseHandle from another thread, if the connect is taking too long.The lpvStatusInformation parameter contains the address of an HINTERNET handle."); break;
		//case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
		//	lplog(LOG_INFO, L"Received an intermediate(100 level) status code message from the server."); break;
		//case INTERNET_STATUS_NAME_RESOLVED:
		//	lplog(LOG_INFO, L"Successfully found the IP address of the name contained in lpvStatusInformation.The lpvStatusInformation parameter points to a PCTSTR containing the host name."); break;
		//case INTERNET_STATUS_P3P_HEADER:
		//	lplog(LOG_INFO, L"The response has a P3P header in it."); break;
		//case INTERNET_STATUS_P3P_POLICYREF:
		//	lplog(LOG_INFO, L"Not implemented."); break;
		//case INTERNET_STATUS_PREFETCH:
		//	lplog(LOG_INFO, L"Not implemented."); break;
		//case INTERNET_STATUS_PRIVACY_IMPACTED:
		//	lplog(LOG_INFO, L"Not implemented."); break;
		//case INTERNET_STATUS_RECEIVING_RESPONSE:
		//	lplog(LOG_INFO, L"Waiting for the server to respond to a request.The lpvStatusInformation parameter is NULL."); break;
	case INTERNET_STATUS_REDIRECT:
		if (logDetail)
			lplog(LOG_INFO, L"Request redirected to %s.", (wchar_t*)lpvStatusInformation);
		redirectUrl = (wchar_t*)lpvStatusInformation;
		break;
		//case INTERNET_STATUS_REQUEST_COMPLETE:
		//	lplog(LOG_INFO, L"An asynchronous operation has been completed.The lpvStatusInformation parameter contains the address of an INTERNET_ASYNC_RESULT structure."); break;
		//case INTERNET_STATUS_REQUEST_SENT:
		//	lplog(LOG_INFO, L"Successfully sent the information request to the server.The lpvStatusInformation parameter points to a DWORD value that contains the number of bytes sent."); break;
		//case INTERNET_STATUS_RESOLVING_NAME:
		//	lplog(LOG_INFO, L"Looking up the IP address of the name contained in lpvStatusInformation.The lpvStatusInformation parameter points to a PCTSTR containing the host name."); break;
		//case INTERNET_STATUS_RESPONSE_RECEIVED:
		//	lplog(LOG_INFO, L"Successfully received a response from the server."); break;
		//case INTERNET_STATUS_SENDING_REQUEST:
		//	lplog(LOG_INFO, L"Sending the information request to the server.The lpvStatusInformation parameter is NULL."); break;
		//case INTERNET_STATUS_STATE_CHANGE:
		//	lplog(LOG_INFO, L"Moved between a secure(HTTPS) and a nonsecure(HTTP) site.The user must be informed of this change; otherwise, the user is at risk of disclosing sensitive information involuntarily.When this flag is set, the lpvStatusInformation parameter points to a status DWORD that contains additional flags."); break;
		//default:;
	}
}

// Ensure hINet is open (InternetOpen "InetURL/1.0", preconfig, no cache).
// First call also logs InternetGetConnectedState flags and raises the
// per-server connection caps to 100.  Every call (re)sets connect timeout
// to 3s.  On failure, charges (now-timer) to the network profile and
// returns false.
bool cInternet::LPInternetOpen(int timer)
{
	if (!hINet)
	{
		hINet = InternetOpen(L"InetURL/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
		DWORD dwFlags;
		InternetGetConnectedState(&dwFlags, 0); // BOOL connState=
		if (dwFlags & INTERNET_CONNECTION_CONFIGURED)
			lplog(LOG_INFO, L"Local system has a valid connection to the Internet, but it might or might not be currently connected.");
		if (dwFlags & INTERNET_CONNECTION_LAN)
			lplog(LOG_INFO, L"Local system uses a local area network to connect to the Internet.");
		if (dwFlags & INTERNET_CONNECTION_MODEM)
			lplog(LOG_INFO, L"Local system uses a modem to connect to the Internet.");
		if (dwFlags & INTERNET_CONNECTION_MODEM_BUSY)
			lplog(LOG_INFO, L"No longer used.");
		if (dwFlags & INTERNET_CONNECTION_OFFLINE)
			lplog(LOG_INFO, L"Local system is in offline mode.");
		if (dwFlags & INTERNET_CONNECTION_PROXY)
			lplog(LOG_INFO, L"Local system uses a proxy server to connect to the Internet.");
		if (dwFlags & INTERNET_RAS_INSTALLED)
			lplog(LOG_INFO, L"Local system has RAS installed.");
		InetOption(true, INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER, L"Maximum connections per 1.0 server", 100);
		InetOption(true, INTERNET_OPTION_MAX_CONNS_PER_PROXY, L"Maximum connections per proxy", 100);
		InetOption(true, INTERNET_OPTION_MAX_CONNS_PER_SERVER, L"Maximum connections per server", 100);
		InternetSetStatusCallback(hINet, InternetStatusCallback);
	}
	wstring ioe;
	if (!hINet)
	{
		lplog(LOG_ERROR, L"ERROR:InternetOpen Failed - %s", getLastErrorMessage(ioe));
		AcquireSRWLockExclusive(&cProfile::networkTimeSRWLock);
		cProfile::accumulationNetworkProfileTimer += (clock() - timer);
		ReleaseSRWLockExclusive(&cProfile::networkTimeSRWLock);
		return false;
	}
	InetOption(false, INTERNET_OPTION_CONNECT_TIMEOUT, L"Connect Timeout", 3000); // in milliseconds
	return true;
}

#define MAX_BUF 200000
// Rate-limit, then InternetOpenUrl + InternetReadFile_Wait into buffer
// (decoded via mTW).  Retries internetWebSearchRetryAttempts times; a SPARQL
// failure additionally Sleep(30s)s before that retry.  Returns 0,
// INTERNET_OPEN_FAILED, or INTERNET_OPEN_URL_FAILED.
int cInternet::readPage(const wchar_t* str, wstring& buffer, wstring& headers)
{
	LFS
		int timer = clock();
	AcquireSRWLockShared(&cProfile::networkTimeSRWLock);
	if (clock() - cProfile::lastNetClock < bandwidthControl)
	{
		int timeWait = (bandwidthControl - (clock() - cProfile::lastNetClock));
		ReleaseSRWLockShared(&cProfile::networkTimeSRWLock);
		AcquireSRWLockExclusive(&totalInternetTimeWaitBandwidthControlSRWLock);
		cProfile::totalInternetTimeWaitBandwidthControl += timeWait;
		ReleaseSRWLockExclusive(&totalInternetTimeWaitBandwidthControlSRWLock);
		Sleep(timeWait);
	}
	else
		ReleaseSRWLockShared(&cProfile::networkTimeSRWLock);
	int errors = 0;
	AcquireSRWLockExclusive(&cProfile::networkTimeSRWLock);
	cProfile::lastNetClock = clock();
	ReleaseSRWLockExclusive(&cProfile::networkTimeSRWLock);
	char cBuffer[MAX_BUF + 4];
	if (!LPInternetOpen(timer))
		return INTERNET_OPEN_FAILED;
	wstring ioe;

	//INTERNET_OPTION_CONNECT_RETRIES
	while (errors < internetWebSearchRetryAttempts)
	{
		LPVOID hFile;
		if (hFile = InternetOpenUrl(hINet, str, headers.c_str(), headers.length(), 0, INTERNET_FLAG_NO_CACHE_WRITE))
		{
			if (log_net) lplog(L"Successfully opened URL %s.", str);
			DWORD dwRead;
			bool timedOut = false;
			// does InternetReadFile on a worker thread so a hung read can be timed out
			while (InternetReadFile_Wait(hFile, cBuffer, MAX_BUF, &dwRead, timedOut))
			{
				if (dwRead == 0)
					break;
				cBuffer[dwRead] = 0;
				wstring wb;
				buffer += mTW(cBuffer, wb);
			}
			if (!timedOut) // the timeout path already closed hFile to unblock the worker
				InternetCloseHandle(hFile);
			if (!timedOut)
			{
				cProfile::accumulateNetworkTime(str, timer, cProfile::lastNetClock);
				return 0;
			}
			errors++;
			lplog(LOG_ERROR, L"ERROR:%d:Timeout reading URL %s.", errors, str);
			if (!InternetCheckConnection(str, FLAG_ICC_FORCE_CONNECTION, 0))
				lplog(LOG_ERROR, L"ERROR:Cannot force URL %s - %s.\r", str, getLastErrorMessage(ioe));
			lplog(LOG_ERROR, NULL);
		}
		else
		{
			errors++;
			int lastError = GetLastError();
			lplog(LOG_ERROR, L"ERROR:%d:Cannot open URL %s - %s (%d).\r", errors, str, getLastErrorMessage(ioe), lastError);
			if (wcsstr(str, L"sparql"))
			{
				wprintf(L"\n\nrestart virtuoso\n");
				Sleep(30000);
				continue; // retry within this loop's errors<internetWebSearchRetryAttempts cap instead of recursing with no depth limit
			}
			if (lastError == ERROR_NO_UNICODE_TRANSLATION)
				break;
			if (!InternetCheckConnection(str, FLAG_ICC_FORCE_CONNECTION, 0))
				lplog(LOG_ERROR, L"ERROR:Cannot force URL %s - %s.\r", str, getLastErrorMessage(ioe));
			InternetCloseHandle(hINet);
			wprintf(L"\nrestarting internet connection for URL %s...\n", str);
			hINet = 0;
			LPInternetOpen(timer);
			lplog(LOG_ERROR, NULL);
			AcquireSRWLockExclusive(&cProfile::networkTimeSRWLock);
			cProfile::lastNetClock = clock();
			ReleaseSRWLockExclusive(&cProfile::networkTimeSRWLock);
		}
	}
	if (errors == internetWebSearchRetryAttempts)
		lplog(LOG_ERROR, L"ERROR:%d:Terminating because we cannot read URL %s - %s.", errors, str, getLastErrorMessage(ioe));
	cProfile::accumulateNetworkTime(str, timer, cProfile::lastNetClock);
	return (errors) ? INTERNET_OPEN_URL_FAILED : 0;
}

// GET 'str' and write raw bytes to destfile, adding the byte count to
// 'total'.  The `while (true)` never retries - the failure arm returns -1.
// FATAL if the write fails.  Returns 0, INTERNET_OPEN_FAILED, or -1.
int cInternet::readBinaryPage(wchar_t* str, int destfile, int& total)
{
	LFS
		AcquireSRWLockShared(&cProfile::networkTimeSRWLock);
	if (clock() - cProfile::lastNetClock < bandwidthControl)
	{
		ReleaseSRWLockShared(&cProfile::networkTimeSRWLock);
		int timeWait = (bandwidthControl - (clock() - cProfile::lastNetClock));
		AcquireSRWLockExclusive(&totalInternetTimeWaitBandwidthControlSRWLock);
		cProfile::totalInternetTimeWaitBandwidthControl += timeWait;
		ReleaseSRWLockExclusive(&totalInternetTimeWaitBandwidthControlSRWLock);
		Sleep(timeWait);
	}
	else
		ReleaseSRWLockShared(&cProfile::networkTimeSRWLock);
	AcquireSRWLockShared(&cProfile::networkTimeSRWLock);
	cProfile::lastNetClock = clock();
	ReleaseSRWLockShared(&cProfile::networkTimeSRWLock);
	char cBuffer[MAX_BUF + 4];
	wstring ioe;
	if (!LPInternetOpen(0))
	{
		lplog(LOG_ERROR, L"ERROR:LPInternetOpen Failed - %s", getLastErrorMessage(ioe));
		return INTERNET_OPEN_FAILED;
	}
	while (true)
	{
		HANDLE hFile;
		if (hFile = InternetOpenUrl(hINet, str, NULL, 0, 0, INTERNET_FLAG_NO_CACHE_WRITE))
		{
			if (log_net) lplog(L"Successfully opened URL %s.", str);
			DWORD dwRead;
			while (InternetReadFile(hFile, cBuffer, MAX_BUF, &dwRead))
			{
				if (dwRead == 0)
					break;
				total += dwRead;
				if (::write(destfile, cBuffer, dwRead) < 0)
				{
					lplog(LOG_FATAL_ERROR, L"Cannot write rdfTypes dbPediaCache - %S.", _sys_errlist[errno]);
					return -1;
				}
			}
			InternetCloseHandle(hFile);
			return 0;
		}
		else
		{
			lplog(LOG_ERROR, L"ERROR:Cannot open URL %s - %s.", str, getLastErrorMessage(ioe));
			lplog(LOG_ERROR, NULL);
			return -1;
		}
	}
	return 0;
}

// InternetCloseHandle(hINet) and null it.  Always returns true.
bool cInternet::closeConnection(void)
{
	LFS
		if (hINet) InternetCloseHandle(hINet);
	hINet = 0;
	return true;
}

// Cache-aside GET: path is CACHEDIR\\cacheTypePath\\_<sanitized epath>
// (distributed into two-letter subdirs).  On miss or forceWebReread, readPage
// and write the wstring as UTF-16 bytes.  On hit, read those bytes back as
// wchar_t*.  networkAccessed is set true on a fetch.  Returns 0 or a
// GETPAGE / INTERNET_* code.
int cInternet::cacheWebPath(wstring webAddress, wstring& buffer, wstring epath, wstring cacheTypePath, bool forceWebReread, bool& networkAccessed, wstring& diskPath)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	if (_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, L"Cannot create directory %s.", path);
	_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_LEN - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	int ret, fd;
	diskPath = path;
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
			void* tbuffer = (void*)tcalloc(bufferlen + 10, 1);
			if (_read(fd, tbuffer, bufferlen) < 0)
				lplog(LOG_FATAL_ERROR, L"Error reading web path file.");
			_close(fd);
			buffer = (wchar_t*)tbuffer;
			tfree(bufferlen + 10, tbuffer);
		}
	}
	return 0;
}


// Worker: InternetReadFile into the tIRFW pointed at by vThreadParm.
// Returns 0 on success, 1 if InternetReadFile failed (logged).
DWORD WINAPI cInternet::InternetReadFile_Child(void* vThreadParm)
{
	tIRFW* p = (tIRFW*)vThreadParm;
	if (!InternetReadFile(p->RequestHandle, p->buffer, p->bufsize, p->dwRead))
	{
		wstring lem;
		lplog(LOG_ERROR, L"InternetReadFile reports %s.", getLastErrorMessage(lem));
		return 1;
	}
	return 0;
}

// Spawn InternetReadFile_Child and wait up to 5 minutes.  On timeout,
// InternetCloseHandle unblocks the child and we then wait for it to exit before
// returning: 'p', 'buffer' and 'dwRead' are caller stack objects the child writes
// through, so returning while it still runs would corrupt the caller's frame.
// timedOut is per-call; a shared flag would race between concurrent reader threads.
bool cInternet::InternetReadFile_Wait(HINTERNET RequestHandle, char* buffer, int bufsize, DWORD* dwRead, bool& timedOut)
{
	tIRFW p;
	timedOut = false;
	p.buffer = buffer;
	p.bufsize = bufsize;
	p.RequestHandle = RequestHandle;
	p.dwRead = dwRead;
	*dwRead = 0;
	// Create a worker thread
	DWORD    dwThreadID;
	HANDLE hThread = CreateThread(
		NULL,            // Pointer to thread security attributes
		0,               // Initial thread stack size, in bytes
		InternetReadFile_Child,  // Pointer to thread function
		&p,     // The argument for the new thread
		0,               // Creation flags
		&dwThreadID      // Pointer to returned thread identifier
	);
	if (hThread == NULL)
		return false;
	// Wait for the call to InternetConnect in worker function to complete
	DWORD dwTimeout = 5 * 60 * 1000; // in milliseconds
	if (WaitForSingleObject(hThread, dwTimeout) == WAIT_TIMEOUT)
	{
		timedOut = true;
		// Closing the handle makes the pending InternetReadFile fail and return.
		InternetCloseHandle(RequestHandle);
		wprintf(L"\nRetry on document (InternetReadFile failure).\n");
		if (WaitForSingleObject(hThread, 60 * 1000) != WAIT_OBJECT_0)
			// The child still holds pointers into this frame; unwinding now corrupts it.
			lplog(LOG_FATAL_ERROR, L"InternetReadFile worker did not exit after the request handle was closed.");
		CloseHandle(hThread);
		return false;
	}
	// The state of the specified object (thread) is signaled
	DWORD   dwExitCode = 0;
	if (!GetExitCodeThread(hThread, &dwExitCode))
	{
		CloseHandle(hThread);
		return false;
	}
	CloseHandle(hThread);
	return dwExitCode == 0;
}

// Cache-aside GET with optional Jericho HTML-to-text (`clean`) and optional
// population of `buffer` (`readInfoBuffer`).  Skips .pdf/.php.  Index>1
// appends .N to the cache name.  Truncates the path at MAX_LEN-20, matching
// cacheWebPath.  `where` is only for log prefixes.
// Returns 0, -1, or a GETWEBPATH / GETPAGE / INTERNET_* code.
int cInternet::getWebPath(int where, wstring webAddress, wstring& buffer, wstring epath, wstring cacheTypePath, wstring& filePathOut, wstring& headers, int index, bool clean, bool readInfoBuffer, bool forceWebReread)
{
	LFS
		if (webAddress.find(L".pdf") != wstring::npos || webAddress.find(L".php") != wstring::npos) // Nobel Prize abstract is 2 bytes / also don't bother with pdf or php files for now
			return -1;
	if (logTraceOpen)
		lplog(LOG_WHERE, L"TRACEOPEN %s %s", epath.c_str(), __FUNCTIONW__);
	if (logQuestionDetail)
		lplog(LOG_WIKIPEDIA, L"accessing page: %s", epath.c_str());
	wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", (cacheTypePath == L"webSearchCache") ? WEBSEARCH_CACHEDIR : CACHEDIR, cacheTypePath.c_str());
	if (_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, L"Cannot create directory %s.", path);
	if (index > 1)
		_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s.%d", epath.c_str(), index);
	else
		_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_LEN - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	filePathOut = path;
	int exitCode = 0;
	string spath;
	wTM(path, spath, CP_ACP);
	deleteIllegalChars((char*)spath.c_str() + pathlen + 5);
	spath[pathlen + 1] = spath[pathlen + 6];
	spath[pathlen + 3] = spath[pathlen + 7];
	spath[pathlen + 2] = 0;
	if (mkdir(spath.c_str()) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, L"Cannot create directory %s.", path);
	spath[pathlen + 2] = '\\';
	spath[pathlen + 4] = 0;
	if (mkdir(spath.c_str()) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, L"Cannot create directory %s.", path);
	spath[pathlen + 4] = '\\';
	wchar_t* wp = wcsstr(path, L"http");
	if (wp && (wp - path) < 5)
		lplog(LOG_FATAL_ERROR, L"Please remove http addresses from web path to avoid overuse of the h/t directory %s!", path);
	if (forceWebReread || (_waccess(path, 0) < 0 && _access(spath.c_str(), 0) < 0))
	{
		if (!forceWebReread)
			lplog(LOG_WIKIPEDIA, L"getWebPath:failed to access page %s %S", path, spath.c_str());
		int ret, fd;
		ret = readPage(webAddress.c_str(), buffer, headers);
		if ((fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) < 0)
		{
			lplog(LOG_ERROR, L"%06d:ERROR:getWebPath:Cannot create dbPedia path %s - %S.", where, path, sys_errlist[errno]);
			return cInternet::GETPAGE_CANNOT_CREATE;
		}
		if (!clean)
		{
			_write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
			_close(fd);
			if (logRDFDetail)
				lplog(LOG_WIKIPEDIA, L"getWebPath:nonJava wrote page %s", path);
			return ret; // propagate a readPage failure instead of reporting success (matches cacheWebPath)
		}
		if (ret && clean)
		{
			_close(fd);
			return ret; // propagate a readPage failure instead of reporting success (matches cacheWebPath)
		}
		string utf8Buffer;
		wTM(buffer, utf8Buffer);
		_write(fd, utf8Buffer.c_str(), utf8Buffer.length() * sizeof(utf8Buffer[0]));
		_close(fd);
		string outbuf;
		if ((exitCode = runJavaJerichoHTML(path, path, outbuf)) < 0) return -1; // changed to write file to disk twice because Java can no longer reliably scrape thesaurus.com due to cookie
		if (outbuf.find("Could not find") != string::npos)
		{
			lplog(LOG_FATAL_ERROR, L"Jericho library call on %s resulted in illegal error:%s", path, outbuf.c_str());
			_wremove(path);
		}
		lplog(LOG_WIKIPEDIA, L"getWebPath:Java wrote page %s:%S", path, outbuf.c_str());
		if (outbuf.find("Exception") != string::npos)
		{
			if ((fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				_close(fd);
				return 0;
			}
			if ((fd = _open(spath.c_str(), O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				_close(fd);
				return 0;
			}
			lplog(LOG_ERROR, L"%06d:ERROR:getWebPath:Cannot create dbPedia path %S - %S.", where, spath.c_str(), sys_errlist[errno]);
		}
	}
	if (readInfoBuffer)
	{
		int fd;
		if ((fd = _wopen(path, O_RDWR | O_BINARY)) < 0)
		{
			if (exitCode && (fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				lplog(LOG_WIKIPEDIA, L"getWebPath:nonJava close 0 page %s", path);
				_close(fd);
				return 0;
			}
			if (exitCode && (fd = _open(spath.c_str(), O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				lplog(LOG_WIKIPEDIA, L"getWebPath:nonJava close 0 page %S", spath.c_str());
				_close(fd);
				return 0;
			}
			if ((fd = _open(spath.c_str(), O_RDWR | O_BINARY)) < 0)
			{
				lplog(LOG_ERROR, L"%06d:ERROR:getWebPath:Cannot read dbPedia path %s [%S] - %S.", where, path, spath.c_str(), sys_errlist[errno]);
				fd = _open(spath.c_str(), O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
				if (fd >= 0)
					_close(fd);
				return GETWEBPATH_CANNOT_OPEN_PATH;
			}
			else
				mTW(spath, filePathOut);
		}
		int bufferlen = filelength(fd);
		void* tbuffer = (void*)tcalloc(bufferlen + 10, 1);
		if (_read(fd, tbuffer, bufferlen) < 0)
			lplog(LOG_FATAL_ERROR, L"Error reading webPath file.");
		_close(fd);
		buffer = (wchar_t*)tbuffer;
		tfree(bufferlen + 10, tbuffer);
	}
	return 0;
}

// Drain hPipeRead into outbuf until ReadFile fails.  ERROR_BROKEN_PIPE is
// the expected end (child exited); anything else is logged.
// ReadAndHandleOutput
// Monitors handle for input. Exits when child exits or pipe breaks.
void cInternet::ReadAndHandleOutput(HANDLE hPipeRead, string& outbuf)
{
	LFS
		CHAR lpBuffer[257];
	DWORD nBytesRead;

	while (ReadFile(hPipeRead, lpBuffer, sizeof(lpBuffer) - 1, &nBytesRead, NULL) && nBytesRead > 0)
	{
		lpBuffer[nBytesRead] = 0;
		outbuf += lpBuffer;
	}
	if (GetLastError() != ERROR_BROKEN_PIPE)
		lplog(LOG_ERROR, L"%S:%d:%s", __FUNCTION__, __LINE__, lastErrorMsg().c_str());
}

// CreateProcess(commandLine) with the three std handles redirected, hidden
// window, CREATE_NEW_CONSOLE.  Returns the process handle (not checked for
// NULL if CreateProcess failed - pi is uninitialized on failure).
// PrepAndLaunchRedirectedChild
// Sets up STARTUPINFO structure, and launches redirected child.
HANDLE cInternet::PrepAndLaunchRedirectedChild(wstring commandLine,
	HANDLE hChildStdOut, HANDLE hChildStdIn, HANDLE hChildStdErr)
{
	LFS
		PROCESS_INFORMATION pi;
	STARTUPINFO si;

	// Set up the start up info struct.
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdOutput = hChildStdOut;
	si.hStdInput = hChildStdIn;
	si.hStdError = hChildStdErr;
	si.wShowWindow = SW_HIDE;
	if (!CreateProcess(NULL, (LPWSTR)commandLine.c_str(), NULL, NULL, TRUE,
		CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
		lplog(LOG_ERROR, L"%S:%d:%s", __FUNCTION__, __LINE__, lastErrorMsg().c_str());

	// Close any unnecessary handles.
	if (!CloseHandle(pi.hThread))
		lplog(LOG_ERROR, L"%S:%d:%s", __FUNCTION__, __LINE__, lastErrorMsg().c_str());
	return pi.hProcess;
}

// chdir LMAINDIR, spawn
// `java -classpath jericho-html-3.4\\... RenderToText <webAddress> <outputPath>`
// with stdout/stderr piped into outbuf, wait INFINITE, chdir back.
// Returns 0, or -1 if chdir-back fails (launch failure is FATAL first).
int cInternet::runJavaJerichoHTML(wstring webAddress, wstring outputPath, string& outbuf)
{
	LFS
		TCHAR NPath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, NPath);
	if (_wchdir(LMAINDIR) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot find main directory.");
	wstring baseCommandLine = L"java -classpath jericho-html-3.4\\classes;jericho-html-3.4\\dist\\jericho-html-3.4.jar;TextRenderer\\bin RenderToText ";
	wstring commandLine = baseCommandLine + webAddress + L" " + outputPath;

	HANDLE hOutputReadTmp, hOutputRead, hOutputWrite;
	HANDLE hInputWriteTmp, hInputRead, hInputWrite;
	HANDLE hErrorWrite;
	SECURITY_ATTRIBUTES sa;

	// Set up the security attributes struct.
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	// Create the child output pipe.
	if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0))
		lplog(LOG_FATAL_ERROR, L"CreatePipe %d line %d", GetLastError(), __LINE__);

	// Create a duplicate of the output write handle for the std error write handle. This is necessary 
	// in case the child application closes one of its std output handles.
	if (!DuplicateHandle(GetCurrentProcess(), hOutputWrite, GetCurrentProcess(), &hErrorWrite, 0, TRUE, DUPLICATE_SAME_ACCESS))
		lplog(LOG_FATAL_ERROR, L"DuplicateHandle %d line %d", GetLastError(), __LINE__);

	// Create the child input pipe.
	if (!CreatePipe(&hInputRead, &hInputWriteTmp, &sa, 0))
		lplog(LOG_FATAL_ERROR, L"CreatePipe %d line %d", GetLastError(), __LINE__);

	// Create new output read handle and the input write handles. Set the Properties to FALSE. 
	// Otherwise, the child inherits the properties and, as a result, non-closeable handles to the pipes
	// are created.
	if (!DuplicateHandle(GetCurrentProcess(), hOutputReadTmp, GetCurrentProcess(),
		&hOutputRead, // Address of new handle.
		0, FALSE, // Make it uninheritable.
		DUPLICATE_SAME_ACCESS))
		lplog(LOG_FATAL_ERROR, L"DuplicateHandle %d line %d", GetLastError(), __LINE__);

	if (!DuplicateHandle(GetCurrentProcess(), hInputWriteTmp,
		GetCurrentProcess(),
		&hInputWrite, // Address of new handle.
		0, FALSE, // Make it uninheritable.
		DUPLICATE_SAME_ACCESS))
		lplog(LOG_FATAL_ERROR, L"DuplicateHandle %d line %d", GetLastError(), __LINE__);

	// Close inheritable copies of the handles you do not want to be inherited.
	if (!CloseHandle(hOutputReadTmp))
		lplog(LOG_FATAL_ERROR, L"CloseHandle %d line %d", GetLastError(), __LINE__);
	if (!CloseHandle(hInputWriteTmp))
		lplog(LOG_FATAL_ERROR, L"CloseHandle %d line %d", GetLastError(), __LINE__);

	HANDLE hStdIn = NULL; // Handle to parents std input.

	// Get std input handle so you can close it and force the ReadFile to
	// fail when you want the input thread to exit.
	if ((hStdIn = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
		lplog(LOG_FATAL_ERROR, L"GetStdHandle %d line %d", GetLastError(), __LINE__);

	HANDLE hChildProcess = PrepAndLaunchRedirectedChild(commandLine, hOutputWrite, hInputRead, hErrorWrite);

	if (!hChildProcess)
	{
		wchar_t currentDirectory[4096];
		GetCurrentDirectory(4096, currentDirectory);
		lplog(LOG_FATAL_ERROR, L"Error launching %s in %s", commandLine.c_str(), currentDirectory);
		return -1;
	}
	// Close pipe handles (do not continue to modify the parent).
	// You need to make sure that no handles to the write end of the
	// output pipe are maintained in this process or else the pipe will
	// not close when the child process exits and the ReadFile will hang.
	if (!CloseHandle(hOutputWrite))
		lplog(LOG_FATAL_ERROR, L"CloseHandle %d line %d", GetLastError(), __LINE__);
	if (!CloseHandle(hInputRead))
		lplog(LOG_FATAL_ERROR, L"CloseHandle %d line %d", GetLastError(), __LINE__);
	if (!CloseHandle(hErrorWrite))
		lplog(LOG_FATAL_ERROR, L"CloseHandle %d line %d", GetLastError(), __LINE__);

	// Read the child's output.
	ReadAndHandleOutput(hOutputRead, outbuf);

	// Force the read on the input to return by closing the stdin handle.
	//if (!CloseHandle(hStdIn)) // error is very common - returns handle invalid
		//logLastError("CloseHandle",__LINE__);

	if (WaitForSingleObject(hChildProcess, INFINITE) == WAIT_FAILED)
		lplog(LOG_FATAL_ERROR, L"WaitForSingleObject %d line %d", GetLastError(), __LINE__);

	if (!CloseHandle(hOutputRead))
		lplog(LOG_FATAL_ERROR, L"CloseHandle %d line %d", GetLastError(), __LINE__);
	if (!CloseHandle(hInputWrite))
		lplog(LOG_FATAL_ERROR, L"CloseHandle %d line %d", GetLastError(), __LINE__);
	if (_wchdir(NPath) < 0)
		return -1;
	return 0;
}

#ifdef TEST_CODE
int testWebPath(int where, wstring webAddress, wstring epath, wstring cacheTypePath, wstring& filePathOut, wstring& headers)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	if (_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, L"Cannot create directory %s.", path);
	_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_LEN - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	filePathOut = path;
	//int exitCode=0;
	string spath;
	wTM(path, spath, CP_ACP);
	deleteIllegalChars((char*)spath.c_str() + pathlen + 5);
	spath[pathlen + 1] = spath[pathlen + 6];
	spath[pathlen + 3] = spath[pathlen + 7];
	spath[pathlen + 2] = 0;
	mkdir(spath.c_str());
	spath[pathlen + 2] = '\\';
	spath[pathlen + 4] = 0;
	mkdir(spath.c_str());
	spath[pathlen + 4] = '\\';
	if (_waccess(path, 0) < 0 && _access(spath.c_str(), 0) < 0)
		return 0;
	wstring readBufferFromDisk;
	if (readPage(webAddress.c_str(), readBufferFromDisk, headers) < 0) return -1;
	int fd;
	if ((fd = _wopen(path, O_RDWR | O_BINARY)) < 0)
	{
		if ((fd = _open(spath.c_str(), O_RDWR | O_BINARY)) < 0)
		{
			lplog(LOG_ERROR, L"%06d:ERROR:getWebPath:Cannot read dbPedia path %s [%S] - %S.", where, path, spath.c_str(), sys_errlist[errno]);
			return GETWEBPATH_CANNOT_OPEN_PATH;
		}
		else
			mTW(spath, filePathOut);
	}
	int bufferlen = filelength(fd);
	void* tbuffer = (void*)tcalloc(bufferlen + 10, 1);
	_read(fd, tbuffer, bufferlen);
	_close(fd);
	wstring writeBufferToDisk = (wchar_t*)tbuffer;
	tfree(bufferlen + 10, tbuffer);
	if (readBufferFromDisk != writeBufferToDisk)
		lplog(L"MISMATCH!");
	return 0;
}

#endif