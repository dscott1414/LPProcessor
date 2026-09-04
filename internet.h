/*
	internet.h - libcurl wrapper for HTTP GET, on-disk web cache, and the Jericho HTML-to-text launcher

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
		- readPage() - GET a URL into a lpwstring (decoded via mTW).
		- readBinaryPage() - GET a URL and write raw bytes to an fd.
		- cacheWebPath() / getWebPath() - cache-aside read of a URL.
		- LPInternetOpen() / closeConnection() - process-wide session handle.
		- runJavaJerichoHTML() - spawn "java ... RenderToText".

	Key data structures / globals:
		- hINet - one shared HINTERNET; reset to 0 on some failures and re-opened.
		- bandwidthControl - minimum milliseconds between requests (lastNetClock).
		- redirectUrl - last INTERNET_STATUS_REDIRECT target (written from the callback),
			and hINet itself are process-wide and unsynchronized across reader threads.
		- internetWebSearchRetryAttempts - retry cap for readPage.

	Notes / gotchas:
		- MAX_LEN is #defined here AND in source.h (both 2048).  Include order matters
			if anyone ever changes one of them.
		- getWebPath return codes overlap NET_ERR in word.h (GETWEBPATH_CANNOT_OPEN_PATH = -7).
		- InternetReadFile_Wait reports a timeout through its 'timedOut' out-parameter;
			on that path it has already closed the request handle, so the caller must not.
*/
#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#include <shared_mutex> // batch B3: totalInternetTimeWaitBandwidthControlSRWLock's type
#include <string>
// Batch B9: the WinINet client is gone, replaced by libcurl. The public surface
// below (readPage / readBinaryPage / cacheWebPath / getWebPath / LPInternetOpen /
// closeConnection / runJavaJerichoHTML) is unchanged, exactly as the port plan
// requires, so none of the ~10 consumer files needed a signature change.
//
// What went away with WinINet, and why nothing replaces it:
//   - InetOption / InternetStatusCallback: WinINet-specific option and status
//     plumbing. libcurl's equivalents are set directly in LPInternetOpen.
//   - InternetReadFile_Wait / InternetReadFile_Child / tIRFW: an entire worker
//     thread plus handle-closing dance existed only to put a timeout on a read
//     that WinINet could not time out itself. CURLOPT_TIMEOUT does that natively,
//     so the thread (the sole CreateThread in the codebase) is deleted rather
//     than ported -- a net simplification, and it removes the only place where
//     two threads shared the log handles.
//   - PrepAndLaunchRedirectedChild / ReadAndHandleOutput(HANDLE): CreateProcess
//     with inherited pipe handles, now posix_spawn with a pipe and file actions.
class cInternet
{
public:
	static int readPage(const lpchar_t *str, lpwstring &buffer);
	static bool LPInternetOpen(int timer);
	static int readPage(const lpchar_t *str, lpwstring &buffer, lpwstring &headers);
	static int readBinaryPage(lpchar_t *str, int destfile, int &total);
	static bool closeConnection(void);
	static int cacheWebPath(lpwstring webAddress, lpwstring &buffer, lpwstring epath, lpwstring cacheTypePath, bool forceWebReread, bool &networkAccessed, lpwstring &diskPath);
	// The process-wide libcurl easy handle (void* so this header does not have to
	// pull in curl/curl.h; Internet.cpp casts it back to CURL*). Replaces hINet.
	static void *curlHandle;
	static int bandwidthControl;
	static lpwstring redirectUrl;
	static std::shared_mutex totalInternetTimeWaitBandwidthControlSRWLock; // batch B3: was a Win32 SRWLOCK, see general.h
	static int getWebPath(int where, lpwstring webAddress, lpwstring &buffer, lpwstring epath, lpwstring cacheTypePath, lpwstring &filePathOut, lpwstring &headers, int index, bool clean, bool readInfoBuffer, bool forceWebReread=false);
	// Reads everything from a pipe read-end into outbuf. Batch B9: an int fd now.
	static void ReadAndHandleOutput(int pipeReadFd, std::string &outbuf);
	static int runJavaJerichoHTML(lpwstring webAddress, lpwstring outputPath, std::string &outbuf);

	enum {
		GETWEBPATH_CANNOT_OPEN_PATH = -7, INTERNET_OPEN_FAILED = -13, INTERNET_OPEN_URL_FAILED = -14,
		GETPAGE_CANNOT_CREATE = -15
	};
	#define MAX_LEN 2048
	static int internetWebSearchRetryAttempts;
};

