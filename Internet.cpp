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

const wchar_t *getLastErrorMessage(wstring &out);
void * Internet::hINet;
int Internet::bandwidthControl;
bool Internet::readTimeoutError;
struct _RTL_SRWLOCK Internet::totalInternetTimeWaitBandwidthControlSRWLock;
wstring Internet::redirectUrl;

int Internet::readPage(const wchar_t *str, wstring &buffer)
{
	LFS
		wstring headers;
	return readPage(str, buffer, headers);
}

bool Internet::InetOption(bool global, int option, wchar_t *description, unsigned long value)
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

void Internet::InternetStatusCallback(
	HINTERNET , // hInternet
	DWORD_PTR , // dwContext
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
			lplog(LOG_INFO, L"Request redirected to %s.", (wchar_t *)lpvStatusInformation);
		redirectUrl = (wchar_t *)lpvStatusInformation;
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

bool Internet::LPInternetOpen(int timer)
{
	if (!hINet)
	{
		hINet = InternetOpen(L"InetURL/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
		DWORD dwFlags;
		InternetGetConnectedState(&dwFlags, 0); // BOOL connState=
		if (dwFlags&INTERNET_CONNECTION_CONFIGURED)
			lplog(LOG_INFO, L"Local system has a valid connection to the Internet, but it might or might not be currently connected.");
		if (dwFlags&INTERNET_CONNECTION_LAN)
			lplog(LOG_INFO, L"Local system uses a local area network to connect to the Internet.");
		if (dwFlags&INTERNET_CONNECTION_MODEM)
			lplog(LOG_INFO, L"Local system uses a modem to connect to the Internet.");
		if (dwFlags&INTERNET_CONNECTION_MODEM_BUSY)
			lplog(LOG_INFO, L"No longer used.");
		if (dwFlags&INTERNET_CONNECTION_OFFLINE)
			lplog(LOG_INFO, L"Local system is in offline mode.");
		if (dwFlags&INTERNET_CONNECTION_PROXY)
			lplog(LOG_INFO, L"Local system uses a proxy server to connect to the Internet.");
		if (dwFlags&INTERNET_RAS_INSTALLED)
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
	InetOption(false, INTERNET_OPTION_CONNECT_TIMEOUT, L"Connect Timeout", 5000); // in milliseconds
	return true;
}

#define MAX_BUF 200000
int Internet::readPage(const wchar_t *str, wstring &buffer, wstring &headers)
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
#define MAX_ERRORS 3
	while (errors < MAX_ERRORS)
	{
		LPVOID hFile;
		if (hFile = InternetOpenUrl(hINet, str, headers.c_str(), headers.length(), 0, INTERNET_FLAG_NO_CACHE_WRITE))
		{
			if (log_net) lplog(L"Successfully opened URL %s.", str);
			DWORD dwRead;
			readTimeoutError = false;
			// does InternetReadFile in a child process to prevent lock
			while (InternetReadFile_Wait(hFile, cBuffer, MAX_BUF, &dwRead))
			{
				if (dwRead == 0)
					break;
				cBuffer[dwRead] = 0;
				wstring wb;
				buffer += mTW(cBuffer, wb);
			}
			InternetCloseHandle(hFile);
			if (!readTimeoutError)
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
				return readPage(str, buffer, headers);
			}
			if (lastError == ERROR_NO_UNICODE_TRANSLATION)
				break;
			if (!InternetCheckConnection(str, FLAG_ICC_FORCE_CONNECTION, 0))
				lplog(LOG_ERROR, L"ERROR:Cannot force URL %s - %s.\r", str, getLastErrorMessage(ioe));
			InternetCloseHandle(hINet);
			wprintf(L"\nrestarting internet connection for URL %s...\n",str);
			hINet = 0;
			LPInternetOpen(timer);
			lplog(LOG_ERROR, NULL);
			Sleep(2000);
			AcquireSRWLockExclusive(&cProfile::networkTimeSRWLock);
			cProfile::lastNetClock = clock();
			ReleaseSRWLockExclusive(&cProfile::networkTimeSRWLock);
		}
	}
	if (errors == MAX_ERRORS)
		lplog(LOG_ERROR, L"ERROR:%d:Terminating because we cannot read URL %s - %s.", errors, str, getLastErrorMessage(ioe)); // TMP DEBUG
	cProfile::accumulateNetworkTime(str, timer, cProfile::lastNetClock);
	return (errors) ? INTERNET_OPEN_URL_FAILED : 0;
}

int Internet::readBinaryPage(wchar_t *str, int destfile, int &total)
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
				::write(destfile, cBuffer, dwRead);
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

bool Internet::closeConnection(void)
{
	LFS
		if (hINet) InternetCloseHandle(hINet);
	return true;
}

int Internet::cacheWebPath(wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, bool forceWebReread, bool &networkAccessed,wstring &diskPath)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	_wmkdir(path);
	_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_PATH - 20] = 0; // make space for subdirectories and for file extensions
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
			void *tbuffer = (void *)tcalloc(bufferlen + 10, 1);
			_read(fd, tbuffer, bufferlen);
			_close(fd);
			buffer = (wchar_t *)tbuffer;
			tfree(bufferlen + 10, tbuffer);
		}
	}
	return 0;
}


DWORD WINAPI Internet::InternetReadFile_Child(void *vThreadParm)
{
	tIRFW *p = (tIRFW *)vThreadParm;
	if (!InternetReadFile(p->RequestHandle, p->buffer, p->bufsize, p->dwRead))
	{
		wstring lem;
		lplog(LOG_ERROR, L"InternetReadFile reports %s.", getLastErrorMessage(lem));
		return 1;
	}
	return 0;
}

bool Internet::InternetReadFile_Wait(HINTERNET RequestHandle, char *buffer, int bufsize, DWORD *dwRead)
{
	tIRFW p;
	readTimeoutError = false;
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

	// Wait for the call to InternetConnect in worker function to complete
	DWORD dwTimeout = 5 * 60 * 1000; // in milliseconds
	if (WaitForSingleObject(hThread, dwTimeout) == WAIT_TIMEOUT)
	{
		InternetCloseHandle(RequestHandle);
		CloseHandle(hThread);
		wprintf(L"\nRetry on document (InternetReadFile failure).\n");
		readTimeoutError = true;
		// Wait until the worker thread exits
		// WaitForSingleObject ( hThread, INFINITE );
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

int Internet::getWebPath(int where, wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, wstring &filePathOut, wstring &headers, int index, bool clean, bool readInfoBuffer, bool forceWebReread)
{
	LFS
		if (webAddress.find(L".pdf") != wstring::npos || webAddress.find(L".php") != wstring::npos) // Nobel Prize abstract is 2 bytes / also don't bother with pdf or php files for now
			return -1;
	if (logTraceOpen)
		lplog(LOG_WHERE, L"TRACEOPEN %s %s", epath.c_str(), __FUNCTIONW__);
	if (logQuestionDetail)
		lplog(LOG_WIKIPEDIA, L"accessing page: %s", epath.c_str());
	wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", (cacheTypePath==L"webSearchCache") ? WEBSEARCH_CACHEDIR:CACHEDIR, cacheTypePath.c_str());
	_wmkdir(path);
	if (index > 1)
		_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s.%d", epath.c_str(), index);
	else
		_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_PATH - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	filePathOut = path;
	int exitCode = 0;
	string spath;
	wTM(path, spath, CP_ACP);
	deleteIllegalChars((char *)spath.c_str() + pathlen + 5);
	spath[pathlen + 1] = spath[pathlen + 6];
	spath[pathlen + 3] = spath[pathlen + 7];
	spath[pathlen + 2] = 0;
	mkdir(spath.c_str());
	spath[pathlen + 2] = '\\';
	spath[pathlen + 4] = 0;
	mkdir(spath.c_str());
	spath[pathlen + 4] = '\\';
	if (wcsstr(path, L"http"))
		lplog(LOG_FATAL_ERROR, L"Please remove http addresses from web path to avoid overuse of the h/t directory!");
	if (forceWebReread || (_waccess(path, 0) < 0 && _access(spath.c_str(), 0) < 0))
	{
		if (!forceWebReread)
			lplog(LOG_WIKIPEDIA, L"getWebPath:failed to access page %s %S", path, spath.c_str());
		int ret, fd;
		if (ret = readPage(webAddress.c_str(), buffer, headers)) return ret;
		if ((fd = _wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) < 0)
		{
			lplog(LOG_ERROR, L"%06d:ERROR:getWebPath:Cannot create dbPedia path %s - %S.", where, path, sys_errlist[errno]);
			return Internet::GETPAGE_CANNOT_CREATE;
		}
		if (!clean)
		{
			_write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
			_close(fd);
			if (logRDFDetail)
				lplog(LOG_WIKIPEDIA, L"getWebPath:nonJava wrote page %s", path);
			return 0;
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
		void *tbuffer = (void *)tcalloc(bufferlen + 10, 1);
		_read(fd, tbuffer, bufferlen);
		_close(fd);
		buffer = (wchar_t *)tbuffer;
		tfree(bufferlen + 10, tbuffer);
	}
	return 0;
}

// ReadAndHandleOutput
// Monitors handle for input. Exits when child exits or pipe breaks.
void Internet::ReadAndHandleOutput(HANDLE hPipeRead, string &outbuf)
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

// PrepAndLaunchRedirectedChild
// Sets up STARTUPINFO structure, and launches redirected child.
HANDLE Internet::PrepAndLaunchRedirectedChild(wstring commandLine,
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

int Internet::runJavaJerichoHTML(wstring webAddress, wstring outputPath, string &outbuf)
{
	LFS
		TCHAR NPath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, NPath);
	_wchdir(LMAINDIR);
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
	_wchdir(NPath);
	return 0;
}

#ifdef TEST_CODE
int testWebPath(int where, wstring webAddress, wstring epath, wstring cacheTypePath, wstring &filePathOut, wstring &headers)
{
	LFS
		wchar_t path[MAX_LEN];
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	_wmkdir(path);
	_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", epath.c_str());
	path[MAX_PATH - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	filePathOut = path;
	//int exitCode=0;
	string spath;
	wTM(path, spath, CP_ACP);
	deleteIllegalChars((char *)spath.c_str() + pathlen + 5);
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
	void *tbuffer = (void *)tcalloc(bufferlen + 10, 1);
	_read(fd, tbuffer, bufferlen);
	_close(fd);
	wstring writeBufferToDisk = (wchar_t *)tbuffer;
	tfree(bufferlen + 10, tbuffer);
	if (readBufferFromDisk != writeBufferToDisk)
		lplog(L"MISMATCH!");
	return 0;
}

#endif