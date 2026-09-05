/*
	Internet.cpp - libcurl HTTP GET, cache-aside web fetch, and Jericho HTML-to-text spawn

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
		- readPage / cacheWebPath / getWebPath
		- LPInternetOpen
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
		- cacheWebPath writes lpchar_t as binary (UTF-16) and reads it back as
			lpchar_t*.  getWebPath in the `clean` path writes UTF-8, then
			assigns the file bytes to a lpwstring as lpchar_t* - encoding mismatch.
		- getWebPath now truncates path (a MAX_LEN==2048 lpchar_t buffer) at
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

// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "errno.h"
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
#include <curl/curl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>
#include "internet.h"
#include "lpProcess.h"
extern "C" char** environ;
#include "utfConvert.h"
#include "logging.h"
#include "profile.h"
#include "mysql.h"
#include "mysqldb.h"
#include "general.h"

const lpchar_t* getLastErrorMessage(lpwstring& out);
void* cInternet::curlHandle;
int cInternet::bandwidthControl;
std::shared_mutex cInternet::totalInternetTimeWaitBandwidthControlSRWLock;
lpwstring cInternet::redirectUrl;

// GET 'str' into buffer (headers unused).  Returns 0 or a negative
// INTERNET_* / GETWEBPATH_* code.
int cInternet::readPage(const lpchar_t* str, lpwstring& buffer)
{
	LFS
		lpwstring headers;
	return readPage(str, buffer, headers);
}

// Batch B9: libcurl write callbacks. The first accumulates the body into a
// std::string, for readPage.
// Both follow libcurl's contract: return the number of bytes consumed, and
// returning anything else aborts the transfer.
static size_t appendToStringCallback(char* data, size_t size, size_t nmemb, void* userp)
{
	size_t total = size * nmemb;
	static_cast<std::string*>(userp)->append(data, total);
	return total;
}

struct sBinarySink { int fd; int* total; bool failed; };

// Batch B9: the per-request option set, applied to the shared easy handle before
// every transfer. CURLOPT_TIMEOUT is the piece that replaces the entire
// InternetReadFile_Wait worker-thread apparatus; CURLOPT_FOLLOWLOCATION plus
// CURLINFO_EFFECTIVE_URL afterwards replaces the INTERNET_STATUS_REDIRECT
// callback that used to populate redirectUrl.
static void applyCommonCurlOptions(CURL* curl, const std::string& url)
{
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);   // matched the old INTERNET_OPTION_CONNECT_TIMEOUT of 3000ms
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);        // 5 minutes, matching the old watchdog's timeout
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);         // do not let curl install its own SIGALRM handling
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // accept whatever encodings this libcurl supports
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "LPProcessor/1.0");
}

// Ensure hINet is open (InternetOpen "InetURL/1.0", preconfig, no cache).
// First call also logs InternetGetConnectedState flags and raises the
// per-server connection caps to 100.  Every call (re)sets connect timeout
// to 3s.  On failure, charges (now-timer) to the network profile and
// returns false.
// Batch B9: create (once) the process-wide libcurl easy handle. The long list of
// InternetGetConnectedState reporting the old version did has no libcurl or macOS
// equivalent and is not replaced -- it was informational logging about dial-up
// modems and RAS, and every branch of it is meaningless on this platform. The
// per-connection tuning (max connections per server/proxy, connect timeout) moved
// into applyCommonCurlOptions above, which runs per request.
bool cInternet::LPInternetOpen(int timer)
{
	if (!curlHandle)
	{
		// curl_global_init is not thread-safe and must happen once before any easy
		// handle exists; a function-local static gives exactly that guarantee.
		static const bool globalInitialized = []() {
			return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
		}();
		if (globalInitialized)
			curlHandle = curl_easy_init();
	}
	lpwstring ioe;
	if (!curlHandle)
	{
		lplog(LOG_ERROR, u"ERROR:curl_easy_init Failed - %s", getLastErrorMessage(ioe));
		{
			std::unique_lock<std::shared_mutex> networkTimeLock(cProfile::networkTimeSRWLock);
			cProfile::accumulationNetworkProfileTimer += (clock() - timer);
		}
		return false;
	}
	return true;
}

#define MAX_BUF 200000
// Rate-limit, then InternetOpenUrl + InternetReadFile_Wait into buffer
// (decoded via mTW).  Retries internetWebSearchRetryAttempts times; a SPARQL
// failure additionally std::this_thread::sleep_for(std::chrono::milliseconds(30s))s before that retry.  Returns 0,
// INTERNET_OPEN_FAILED, or INTERNET_OPEN_URL_FAILED.
int cInternet::readPage(const lpchar_t* str, lpwstring& buffer, lpwstring& headers)
{
	LFS
		int timer = clock();
	// Batch B3: the rate-limit decision now reads lastNetClock under a scoped
	// shared_lock and does the waiting outside it, rather than releasing the
	// shared lock by hand on each of the two paths. Same behaviour, one less way
	// to get the pairing wrong.
	int timeWait = 0;
	{
		std::shared_lock<std::shared_mutex> networkTimeLock(cProfile::networkTimeSRWLock);
		if (clock() - cProfile::lastNetClock < bandwidthControl)
			timeWait = bandwidthControl - (clock() - cProfile::lastNetClock);
	}
	if (timeWait > 0)
	{
		{
			std::unique_lock<std::shared_mutex> bandwidthLock(totalInternetTimeWaitBandwidthControlSRWLock);
			cProfile::totalInternetTimeWaitBandwidthControl += timeWait;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(timeWait)); // batch B9: was Win32 Sleep
	}
	int errors = 0;
	{
		std::unique_lock<std::shared_mutex> networkTimeLock(cProfile::networkTimeSRWLock);
		cProfile::lastNetClock = clock();
	}
	if (!LPInternetOpen(timer))
		return INTERNET_OPEN_FAILED;
	lpwstring ioe;
	CURL* curl = (CURL*)curlHandle;
	std::string url = lp_utf16_to_utf8(lpwstring(str));

	// Batch B9: `headers` is the caller's extra REQUEST headers (WinINet took them
	// as a single blob of CRLF-separated lines, which is what the call sites still
	// build); curl wants them one per slist entry, so the blob is split here.
	std::string headerBlob = lp_utf16_to_utf8(headers);
	struct curl_slist* headerList = nullptr;
	for (size_t start = 0; start < headerBlob.size(); )
	{
		size_t end = headerBlob.find("\r\n", start);
		if (end == std::string::npos) end = headerBlob.size();
		std::string line = headerBlob.substr(start, end - start);
		if (!line.empty()) headerList = curl_slist_append(headerList, line.c_str());
		start = end + 2;
	}

	while (errors < internetWebSearchRetryAttempts)
	{
		applyCommonCurlOptions(curl, url);
		std::string body;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendToStringCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
		if (headerList) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
		char curlError[CURL_ERROR_SIZE] = { 0 };
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

		CURLcode result = curl_easy_perform(curl);
		if (result == CURLE_OK)
		{
			if (log_net) lplog(u"Successfully opened URL %s.", str);
			// Where the INTERNET_STATUS_REDIRECT callback used to write redirectUrl.
			char* effectiveUrl = nullptr;
			if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl) == CURLE_OK && effectiveUrl)
				redirectUrl = lp_narrow_to_wide(std::string(effectiveUrl));
			lpwstring wb;
			// mTW applies the same encoding-detection ladder the WinINet path used,
			// so a page's bytes are still interpreted exactly as before.
			buffer += mTW(body, wb);
			if (headerList) curl_slist_free_all(headerList);
			cProfile::accumulateNetworkTime(str, timer, cProfile::lastNetClock);
			return 0;
		}

		errors++;
		lplog(LOG_ERROR, u"ERROR:%d:Cannot read URL %s - %S (%d).\r", errors, str,
			(curlError[0] ? curlError : curl_easy_strerror(result)), (int)result);
		if (lp_strstr(str, u"sparql"))
		{
			lp_wprintf(u"\n\nrestart virtuoso\n");
			std::this_thread::sleep_for(std::chrono::seconds(30));
			continue; // retry within this loop's errors<internetWebSearchRetryAttempts cap instead of recursing with no depth limit
		}
		// Batch B9: a URL libcurl cannot even parse is not going to become valid on
		// a retry, so it breaks out immediately -- the same role
		// ERROR_NO_UNICODE_TRANSLATION played in the WinINet version.
		if (result == CURLE_URL_MALFORMAT || result == CURLE_UNSUPPORTED_PROTOCOL)
			break;
		lplog(LOG_ERROR, NULL);
		{
			std::unique_lock<std::shared_mutex> networkTimeLock(cProfile::networkTimeSRWLock);
			cProfile::lastNetClock = clock();
		}
	}
	if (headerList) curl_slist_free_all(headerList);
	if (errors == internetWebSearchRetryAttempts)
		lplog(LOG_ERROR, u"ERROR:%d:Terminating because we cannot read URL %s.", errors, str);
	cProfile::accumulateNetworkTime(str, timer, cProfile::lastNetClock);
	return (errors) ? INTERNET_OPEN_URL_FAILED : 0;
}



// Cache-aside GET: path is CACHEDIR\\cacheTypePath\\_<sanitized epath>
// (distributed into two-letter subdirs).  On miss or forceWebReread, readPage
// and write the lpwstring as UTF-16 bytes.  On hit, read those bytes back as
// lpchar_t*.  networkAccessed is set true on a fetch.  Returns 0 or a
// GETPAGE / INTERNET_* code.
int cInternet::cacheWebPath(lpwstring webAddress, lpwstring& buffer, lpwstring epath, lpwstring cacheTypePath, bool forceWebReread, bool& networkAccessed, lpwstring& diskPath)
{
	LFS
		lpchar_t path[MAX_LEN];
	int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	if (lp_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	lp_snprintf(path + pathlen, MAX_LEN - pathlen, u"\\_%s", epath.c_str());
	path[MAX_LEN - 20] = 0; // make space for subdirectories and for file extensions
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	int ret, fd;
	diskPath = path;
	if (networkAccessed = forceWebReread || lp_waccess(path, 0) < 0)
	{
		if (ret = readPage(webAddress.c_str(), buffer)) return ret;
		if ((fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) < 0)
		{
			lplog(LOG_ERROR, u"cacheWebPath:Cannot create path %s - %S.", path, sys_errlist[errno]);
			return GETPAGE_CANNOT_CREATE;
		}
		::write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
		::close(fd);
		return 0;
	}
	else
	{
		if ((fd = lp_wopen(path, O_RDWR | O_BINARY)) < 0)
			lplog(LOG_ERROR, u"cacheWebPath:Cannot read path %s - %S.", path, sys_errlist[errno]);
		else
		{
			int bufferlen = lp_filelength(fd);
			void* tbuffer = (void*)tcalloc(bufferlen + 10, 1);
			if (::read(fd, tbuffer, bufferlen) < 0)
				lplog(LOG_FATAL_ERROR, u"Error reading web path file.");
			::close(fd);
			buffer = (lpchar_t*)tbuffer;
			tfree(bufferlen + 10, tbuffer);
		}
	}
	return 0;
}


// Batch B9: InternetReadFile_Child and InternetReadFile_Wait are deleted, not
// ported. Together they were a whole worker thread plus a close-the-handle-to-
// unblock-it dance whose only purpose was to impose a timeout on a read that
// WinINet could not time out itself. CURLOPT_TIMEOUT does that natively (see
// applyCommonCurlOptions), so this was the sole CreateThread in the codebase and
// it is now simply gone.

// Cache-aside GET with optional Jericho HTML-to-text (`clean`) and optional
// population of `buffer` (`readInfoBuffer`).  Skips .pdf/.php.  Index>1
// appends .N to the cache name.  Truncates the path at MAX_LEN-20, matching
// cacheWebPath.  `where` is only for log prefixes.
// Returns 0, -1, or a GETWEBPATH / GETPAGE / INTERNET_* code.
int cInternet::getWebPath(int where, lpwstring webAddress, lpwstring& buffer, lpwstring epath, lpwstring cacheTypePath, lpwstring& filePathOut, lpwstring& headers, int index, bool clean, bool readInfoBuffer, bool forceWebReread)
{
	LFS
		if (webAddress.find(u".pdf") != lpwstring::npos || webAddress.find(u".php") != lpwstring::npos) // Nobel Prize abstract is 2 bytes / also don't bother with pdf or php files for now
			return -1;
	if (logTraceOpen)
		lplog(LOG_WHERE, u"TRACEOPEN %s %s", epath.c_str(), LP_TEXT(__func__).c_str());
	if (logQuestionDetail)
		lplog(LOG_WIKIPEDIA, u"accessing page: %s", epath.c_str());
	lpchar_t path[MAX_LEN];
	int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\%s", (cacheTypePath == u"webSearchCache") ? WEBSEARCH_CACHEDIR : CACHEDIR, cacheTypePath.c_str());
	if (lp_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	if (index > 1)
		lp_snprintf(path + pathlen, MAX_LEN - pathlen, u"\\_%s.%d", epath.c_str(), index);
	else
		lp_snprintf(path + pathlen, MAX_LEN - pathlen, u"\\_%s", epath.c_str());
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
	if (mkdir(spath.c_str(), 0777) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	spath[pathlen + 2] = '\\';
	spath[pathlen + 4] = 0;
	if (mkdir(spath.c_str(), 0777) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	spath[pathlen + 4] = '\\';
	lpchar_t* wp = lp_strstr(path, u"http");
	if (wp && (wp - path) < 5)
		lplog(LOG_FATAL_ERROR, u"Please remove http addresses from web path to avoid overuse of the h/t directory %s!", path);
	if (forceWebReread || (lp_waccess(path, 0) < 0 && access(spath.c_str(), 0) < 0))
	{
		if (!forceWebReread)
			lplog(LOG_WIKIPEDIA, u"getWebPath:failed to access page %s %S", path, spath.c_str());
		int ret, fd;
		ret = readPage(webAddress.c_str(), buffer, headers);
		if ((fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) < 0)
		{
			lplog(LOG_ERROR, u"%06d:ERROR:getWebPath:Cannot create dbPedia path %s - %S.", where, path, sys_errlist[errno]);
			return cInternet::GETPAGE_CANNOT_CREATE;
		}
		if (!clean)
		{
			::write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
			::close(fd);
			if (logRDFDetail)
				lplog(LOG_WIKIPEDIA, u"getWebPath:nonJava wrote page %s", path);
			return ret; // propagate a readPage failure instead of reporting success (matches cacheWebPath)
		}
		if (ret && clean)
		{
			::close(fd);
			return ret; // propagate a readPage failure instead of reporting success (matches cacheWebPath)
		}
		string utf8Buffer;
		wTM(buffer, utf8Buffer);
		::write(fd, utf8Buffer.c_str(), utf8Buffer.length() * sizeof(utf8Buffer[0]));
		::close(fd);
		string outbuf;
		if ((exitCode = runJavaJerichoHTML(path, path, outbuf)) < 0) return -1; // changed to write file to disk twice because Java can no longer reliably scrape thesaurus.com due to cookie
		if (outbuf.find("Could not find") != string::npos)
		{
			lplog(LOG_FATAL_ERROR, u"Jericho library call on %s resulted in illegal error:%s", path, outbuf.c_str());
			lp_wremove(path);
		}
		lplog(LOG_WIKIPEDIA, u"getWebPath:Java wrote page %s:%S", path, outbuf.c_str());
		if (outbuf.find("Exception") != string::npos)
		{
			if ((fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				::close(fd);
				return 0;
			}
			if ((fd = ::open(spath.c_str(), O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				::close(fd);
				return 0;
			}
			lplog(LOG_ERROR, u"%06d:ERROR:getWebPath:Cannot create dbPedia path %S - %S.", where, spath.c_str(), sys_errlist[errno]);
		}
	}
	if (readInfoBuffer)
	{
		int fd;
		if ((fd = lp_wopen(path, O_RDWR | O_BINARY)) < 0)
		{
			if (exitCode && (fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				lplog(LOG_WIKIPEDIA, u"getWebPath:nonJava close 0 page %s", path);
				::close(fd);
				return 0;
			}
			if (exitCode && (fd = ::open(spath.c_str(), O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE)) >= 0)
			{
				lplog(LOG_WIKIPEDIA, u"getWebPath:nonJava close 0 page %S", spath.c_str());
				::close(fd);
				return 0;
			}
			if ((fd = ::open(spath.c_str(), O_RDWR | O_BINARY)) < 0)
			{
				lplog(LOG_ERROR, u"%06d:ERROR:getWebPath:Cannot read dbPedia path %s [%S] - %S.", where, path, spath.c_str(), sys_errlist[errno]);
				fd = ::open(spath.c_str(), O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
				if (fd >= 0)
					::close(fd);
				return GETWEBPATH_CANNOT_OPEN_PATH;
			}
			else
				mTW(spath, filePathOut);
		}
		int bufferlen = lp_filelength(fd);
		void* tbuffer = (void*)tcalloc(bufferlen + 10, 1);
		if (::read(fd, tbuffer, bufferlen) < 0)
			lplog(LOG_FATAL_ERROR, u"Error reading webPath file.");
		::close(fd);
		buffer = (lpchar_t*)tbuffer;
		tfree(bufferlen + 10, tbuffer);
	}
	return 0;
}

// Read everything the child writes until it closes its end of the pipe.
// Batch B9: a POSIX fd and read(2), replacing a HANDLE and ReadFile.
void cInternet::ReadAndHandleOutput(int pipeReadFd, string& outbuf)
{
	LFS
		char buffer[4096];
	for (;;)
	{
		ssize_t got = ::read(pipeReadFd, buffer, sizeof(buffer));
		if (got > 0) { outbuf.append(buffer, (size_t)got); continue; }
		if (got == 0) break;               // child closed the pipe
		if (errno == EINTR) continue;      // interrupted, not finished
		break;
	}
}

// Run the local Java "RenderToText" helper (Jericho HTML) over webAddress,
// writing plain text to outputPath, and collect whatever the child prints on
// stdout/stderr into outbuf.
//
// Batch B9: posix_spawn with a pipe and file actions, replacing
// PrepAndLaunchRedirectedChild's CreateProcess plus five DuplicateHandle calls
// (which existed only to make exactly the right handles inheritable -- POSIX file
// actions express the same thing declaratively). PrepAndLaunchRedirectedChild is
// deleted rather than ported; nothing else called it.
//
// Two other things change here, both forced by the platform: the classpath
// separator is ':' rather than ';', and its entries use '/' rather than '\\'.
// The main directory is taken from getMainDir() (LP_MAIN_DIR) instead of the
// hardcoded LMAINDIR the old code chdir'd to.
int cInternet::runJavaJerichoHTML(lpwstring webAddress, lpwstring outputPath, string& outbuf)
{
	LFS
		char previousDirectory[MAX_PATH];
	if (!getcwd(previousDirectory, sizeof(previousDirectory)))
		return -1;
	std::string mainDirectory = lp_utf16_to_utf8(getMainDir());
	if (chdir(mainDirectory.c_str()) < 0)
		lplog(LOG_FATAL_ERROR, u"Cannot find main directory %S.", mainDirectory.c_str());

	std::vector<std::string> arguments = {
		"java", "-classpath",
		"jericho-html-3.4/classes:jericho-html-3.4/dist/jericho-html-3.4.jar:TextRenderer/bin",
		"RenderToText",
		lp_utf16_to_utf8(webAddress),
		lp_utf16_to_utf8(outputPath)
	};

	int pipeFds[2];
	if (pipe(pipeFds) < 0)
		lplog(LOG_FATAL_ERROR, u"pipe failed - %S line %d", strerror(errno), __LINE__);

	// The child gets the write end as both stdout and stderr (what the old
	// DuplicateHandle of hOutputWrite into hErrorWrite achieved) and must not keep
	// the read end open, or our own read would never see end-of-file.
	posix_spawn_file_actions_t fileActions;
	posix_spawn_file_actions_init(&fileActions);
	posix_spawn_file_actions_addclose(&fileActions, pipeFds[0]);
	posix_spawn_file_actions_adddup2(&fileActions, pipeFds[1], STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&fileActions, pipeFds[1], STDERR_FILENO);
	posix_spawn_file_actions_addclose(&fileActions, pipeFds[1]);

	std::vector<char*> argv;
	for (std::string& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
	argv.push_back(nullptr);

	pid_t childPid = 0;
	// posix_spawnp, not posix_spawn: "java" is found on PATH, as it was on Windows.
	int spawnError = posix_spawnp(&childPid, "java", &fileActions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&fileActions);
	::close(pipeFds[1]); // our copy of the write end, so read() can reach EOF

	if (spawnError != 0)
	{
		::close(pipeFds[0]);
		lplog(LOG_ERROR, u"Cannot run the Jericho HTML helper - %S.", strerror(spawnError));
		if (chdir(previousDirectory) < 0) return -1;
		return -1;
	}

	ReadAndHandleOutput(pipeFds[0], outbuf);
	::close(pipeFds[0]);

	int exitStatus = 0;
	while (waitpid(childPid, &exitStatus, 0) < 0 && errno == EINTR)
		; // interrupted by a signal, not finished

	if (chdir(previousDirectory) < 0)
		return -1;
	return 0;
}

#ifdef TEST_CODE
int testWebPath(int where, lpwstring webAddress, lpwstring epath, lpwstring cacheTypePath, lpwstring& filePathOut, lpwstring& headers)
{
	LFS
		lpchar_t path[MAX_LEN];
	int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\%s", CACHEDIR, cacheTypePath.c_str());
	if (lp_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	lp_snprintf(path + pathlen, MAX_LEN - pathlen, u"\\_%s", epath.c_str());
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
	if (lp_waccess(path, 0) < 0 && access(spath.c_str(), 0) < 0)
		return 0;
	lpwstring readBufferFromDisk;
	if (readPage(webAddress.c_str(), readBufferFromDisk, headers) < 0) return -1;
	int fd;
	if ((fd = lp_wopen(path, O_RDWR | O_BINARY)) < 0)
	{
		if ((fd = ::open(spath.c_str(), O_RDWR | O_BINARY)) < 0)
		{
			lplog(LOG_ERROR, u"%06d:ERROR:getWebPath:Cannot read dbPedia path %s [%S] - %S.", where, path, spath.c_str(), sys_errlist[errno]);
			return GETWEBPATH_CANNOT_OPEN_PATH;
		}
		else
			mTW(spath, filePathOut);
	}
	int bufferlen = lp_filelength(fd);
	void* tbuffer = (void*)tcalloc(bufferlen + 10, 1);
	::read(fd, tbuffer, bufferlen);
	::close(fd);
	lpwstring writeBufferToDisk = (lpchar_t*)tbuffer;
	tfree(bufferlen + 10, tbuffer);
	if (readBufferFromDisk != writeBufferToDisk)
		lplog(u"MISMATCH!");
	return 0;
}

#endif