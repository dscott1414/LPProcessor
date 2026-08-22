/*
	internet.h - WinINet wrapper for HTTP GET, on-disk web cache, and the Jericho HTML-to-text launcher

	Overview:
		cInternet is the only first-party HTTP client.  readPage() opens a process-wide
		HINTERNET, rate-limits on bandwidthControl, and reads the body on a worker
		thread so a hung InternetReadFile can be timed out.  cacheWebPath / getWebPath
		hash a URL onto CACHEDIR (M:\\caches) and only hit the network on a miss (or
		forceWebReread).  runJavaJerichoHTML() shells out to a local Java helper to
		strip tags from a cached page.

	Pipeline position:
		Stage 8 (question answering / web search) and the get*.cpp acquisition tools.
		Also used to scrape Wikipedia tables and SPARQL/Virtuoso.

	Key entry points:
		- readPage() - GET a URL into a wstring (decoded via mTW).
		- readBinaryPage() - GET a URL and write raw bytes to an fd.
		- cacheWebPath() / getWebPath() - cache-aside read of a URL.
		- LPInternetOpen() / closeConnection() - process-wide session handle.
		- runJavaJerichoHTML() - spawn "java ... RenderToText".

	Key data structures / globals:
		- hINet - one shared HINTERNET; reset to 0 on some failures and re-opened.
		- bandwidthControl - minimum milliseconds between requests (lastNetClock).
		- redirectUrl - last INTERNET_STATUS_REDIRECT target (written from the callback).
		- readTimeoutError - set by InternetReadFile_Wait on a 5-minute timeout.
		- internetWebSearchRetryAttempts - retry cap for readPage.

	Notes / gotchas:
		- MAX_LEN is #defined here AND in source.h (both 2048).  Include order matters
			if anyone ever changes one of them.
		- getWebPath return codes overlap NET_ERR in word.h (GETWEBPATH_CANNOT_OPEN_PATH = -7).
		- The worker-thread timeout path closes the request handle while the child
			may still be inside InternetReadFile (see Internet.cpp).
*/
#pragma once
class cInternet
{

	typedef struct
	{
		HINTERNET RequestHandle;
		char *buffer;
		int bufsize;
		DWORD *dwRead;
	} tIRFW;

public:
	static int readPage(const wchar_t *str, wstring &buffer);
	static bool InetOption(bool global, int option, const wchar_t * description, unsigned long value);
	static void InternetStatusCallback(HINTERNET hInternet,DWORD_PTR dwContext,DWORD dwInternetStatus,LPVOID lpvStatusInformation,DWORD dwStatusInformationLength);
	static bool LPInternetOpen(int timer);
	static int readPage(const wchar_t *str, wstring &buffer, wstring &headers);
	static int readBinaryPage(wchar_t *str, int destfile, int &total);
	static bool closeConnection(void);
	static int cacheWebPath(wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, bool forceWebReread, bool &networkAccessed, wstring &diskPath);
	static DWORD WINAPI InternetReadFile_Child(void *vThreadParm);
	static bool InternetReadFile_Wait(HINTERNET RequestHandle, char *buffer, int bufsize, DWORD *dwRead);
	static HINTERNET hINet;
	static int bandwidthControl;
	static wstring redirectUrl;
	static bool readTimeoutError;
	static SRWLOCK totalInternetTimeWaitBandwidthControlSRWLock;
	static int getWebPath(int where, wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, wstring &filePathOut, wstring &headers, int index, bool clean, bool readInfoBuffer, bool forceWebReread=false);
	static void ReadAndHandleOutput(HANDLE hPipeRead, string &outbuf);
	static HANDLE PrepAndLaunchRedirectedChild(wstring commandLine,HANDLE hChildStdOut, HANDLE hChildStdIn, HANDLE hChildStdErr);
	static int runJavaJerichoHTML(wstring webAddress, wstring outputPath, string &outbuf);

	enum {
		GETWEBPATH_CANNOT_OPEN_PATH = -7, INTERNET_OPEN_FAILED = -13, INTERNET_OPEN_URL_FAILED = -14,
		GETPAGE_CANNOT_CREATE = -15
	};
	#define MAX_LEN 2048
	static int internetWebSearchRetryAttempts;
};

