/*
	logging.cpp - lplog / logstring implementation: per-level UTF-8 log files and FATAL exit

	Overview:
		logstring() is the sink.  It peels bits off logLevel, opens (or reuses) a
		FILE* named <level><logFileExtension>.lplog (under "multiprocessor logs/"
		when the extension is non-empty), and fputs the message encoded as UTF-8.
		FILE*s stay open for logCache seconds (or forever if logCache is large).
		lplog() formats into a LOG_BUFFER_SIZE lpchar_t stack buffer, appends a
		newline, and calls logstring.  lplogNR skips the newline.

	Pipeline position:
		Used from the first initializeDatabaseHandle through QA.  Every other
		translation unit calls into here.

	Key entry points:
		- logstring() - write / flush / FATAL-exit.
		- lplog() x3 - format (or flush-all when format==NULL).
		- lplogNR() - format without a trailing newline.

	Key data structures / globals:
		- last*Clock - TLS, last time that level's FILE* was opened.
		- log*File - TLS FILE* (LOG_BUFFER, the default) or TLS int fd (#else);
			each thread that logs opens/closes/reopens only its own handles.
		- logFileExtension / multiProcess - TLS, set by child workers.
		- logCache - seconds a FILE* is kept (40).

	Notes / gotchas:
		- LOG_FATAL_ERROR is routed first as LOG_INFO (main.lplog).  lplog() also
			ORs LOG_ERROR, but logstring() sees FATAL after the info write and
			terminates, so error.lplog never gets the fatal line.
		- The FATAL path goes through fatalExit(): exits EXIT_FAILURE, and waits for
			a keypress only when interactive (multiProcess==0 and stdin is a tty).
		- log*File used to be process-wide statics while logFileExtension/lastClock
			were already TLS, so two threads sharing a level (even with identical
			logFileExtension) could race on open/write/fclose of the same FILE*.
			log*File is now TLS too; the main use of parallelism (-mp) is separate
			OS processes anyway (no shared memory), so the only in-process case this
			ever mattered for is a worker thread (e.g. the WinINet reader thread in
			Internet.cpp) logging concurrently with the main thread.
		- logFilename[1024] is now built with snprintf, so an over-long
			logFileExtension truncates the filename instead of overflowing the
			buffer (it was sprintf, and unbounded, before batch B3).
*/
// Batch B3: windows.h / io.h / winhttp.h are gone (this file used windows.h only
// for GetLastError, and io.h only for isatty/fileno/::open -- all replaced with
// their POSIX spellings below; winhttp.h was never used here at all). ontology.h,
// source.h and QuestionAnswering.h are also gone: this file references nothing
// from any of the three. What is genuinely needed is word.h (for LOG_BUFFER_SIZE),
// profile.h (cProfile::accumulateNetworkTime and the LFS macro) and utfConvert.h
// (the UTF-8 encoder that replaces MSVC's "ccs=UTF-8" lp_fputws, see logstring).
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "word.h"
#include "time.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "profile.h"
#include "utfConvert.h"

thread_local static int lastInfoClock = 0, lastErrorClock = 0, lastNomatchClock = 0, lastResolutionClock = 0, lastWhereClock = 0, lastResCheckClock = 0, lastSGClock = 0, lastWNClock = 0, lastWPClock = 0, lastWSClock = 0, lastRoleClock = 0, lastWCClock = 0, lastTimeClock = 0, lastDictionaryClock = 0, lastQCClock = 0; 		// per thread
#ifdef LOG_BUFFER
// TLS, matching logFileExtension/lastClock above: previously process-wide, so any two
// threads sharing a level (even with the same logFileExtension) could race on the same
// FILE* - one thread's logCache-driven fclose()/reopen could run while another thread
// was still lp_fputws'ing into it.  Each thread now owns its own handles; the CRT closes
// (and flushes) all of them at process exit regardless of which thread opened them.
thread_local static FILE* logInfoFile, * logErrorFile, * logNomatchFile, * logResolutionFile, * logResCheckFile, * logSGFile, * logWNFile, * logWPFile;
thread_local static FILE* logWSFile, * logWhereFile, * logRoleFile, * logWCFile, * logTimeFile, * logDictionaryFile, * logQCFile;
#else
thread_local static int logInfoFile = -1, logErrorFile = -1, logNomatchFile = -1, logResolutionFile = -1, logResCheckFile = -1, logSGFile = -1, logWNFile = -1, logWPFile = -1, logWSFile = -1, logWhereFile = -1, logRoleFile = -1, logWCFile = -1, logTimeFile = -1, logDictionaryFile = -1, logQCFile = -1; // per thread
#endif
thread_local lpwstring logFileExtension; // parallel processing will overload this variable 
thread_local int multiProcess = 0; // initialized
#define LOG_MASK (LOG_INFO|LOG_ERROR|LOG_NOTMATCHED|LOG_RESOLUTION|LOG_WHERE|LOG_RESCHECK|LOG_SG|LOG_WORDNET|LOG_WIKIPEDIA|LOG_WEBSEARCH|LOG_ROLE|LOG_WCHECK|LOG_TIME|LOG_DICTIONARY|LOG_QCHECK|LOG_FATAL_ERROR)
short logCache = 40; // initialized

int logDatabaseDetails = 0;
int logQuestionProfileTime = 0;
int logSynonymDetail = 0;
int logTableDetail = 0;
int logTableCoherenceDetail = 0;
int logEquivalenceDetail = 0;
int logOntologyDetail = 0;
int logDetail = 0;
int logQuestionDetail = 1;
int logRDFDetail = 1;
int logProximityMap = 0;
bool logTraceOpen = false;
bool log_net = false;

// Terminate on a fatal error.  Exits non-zero so a parent (or a shell) can tell a
// crashed run from a clean one, and only waits for a keypress when a human is
// actually watching: a child under -mp has no console input and would hang forever.
static void fatalExit(void)
{
	if (multiProcess == 0 && isatty(fileno(stdin)))
	{
		lp_wprintf(u"\nPress Enter to close.\n");
		char buf[16];
		if (!fgets(buf, sizeof(buf), stdin))
			clearerr(stdin);
	}
	exit(EXIT_FAILURE);
}

// Write 's' to every log file whose bit is set in logLevel.  s==NULL closes
// those files (used as a flush).  LOG_FATAL_ERROR writes main.lplog then calls
// fatalExit(), so this function does not return for a fatal level.
// Returns 0, or -1 if fopen/lp_fputws failed (FATAL still exits first).
int logstring(int logLevel, const lpchar_t* s)
{
	LFS
		while (logLevel & LOG_MASK)
		{
			int* lastClock = &lastInfoClock;
#ifdef LOG_BUFFER
			FILE** logFile = &logInfoFile; // give it a default so compiler doesn't complain
#else
			int* logFile = &logInfoFile;
#endif
			char logFilename[1024];
			// Batch B3: the log filename used to be built with MSVC's narrow-printf
			// "%S" (= "the argument is a WIDE string"), which has no portable
			// meaning -- on POSIX %S means wchar_t*, and logFileExtension is now a
			// char16_t string, so the old spelling would have read the wrong type
			// entirely. Convert once, up front, and use plain %s below. snprintf
			// also replaces sprintf here, closing the unbounded-logFileExtension
			// overflow this file's own header comment already documented as a
			// known gotcha.
			const std::string logExtension = lp_utf16_to_utf8(logFileExtension);
			snprintf(logFilename, sizeof(logFilename), "main%s.lplog", logExtension.c_str()); // give it a default so compiler doesn't complain
			if ((logLevel & LOG_INFO) || (logLevel & LOG_FATAL_ERROR))
			{
				lastClock = &lastInfoClock; logFile = &logInfoFile;
				snprintf(logFilename, sizeof(logFilename), "main%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_INFO;
			}
			else if (logLevel & LOG_ERROR)
			{
				lastClock = &lastErrorClock; logFile = &logErrorFile;
				snprintf(logFilename, sizeof(logFilename), "error%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_ERROR;
			}
			else if (logLevel & LOG_NOTMATCHED)
			{
				lastClock = &lastNomatchClock; logFile = &logNomatchFile;
				snprintf(logFilename, sizeof(logFilename), "nomatch%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_NOTMATCHED;
			}
			else if (logLevel & LOG_RESOLUTION)
			{
				lastClock = &lastResolutionClock; logFile = &logResolutionFile;
				snprintf(logFilename, sizeof(logFilename), "resolution%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_RESOLUTION;
			}
			else if (logLevel & LOG_WHERE)
			{
				lastClock = &lastWhereClock; logFile = &logWhereFile;
				snprintf(logFilename, sizeof(logFilename), "where%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_WHERE;
			}
			else if (logLevel & LOG_RESCHECK)
			{
				lastClock = &lastResCheckClock; logFile = &logResCheckFile;
				snprintf(logFilename, sizeof(logFilename), "rescheck%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_RESCHECK;
			}
			else if (logLevel & LOG_SG)
			{
				lastClock = &lastSGClock; logFile = &logSGFile;
				snprintf(logFilename, sizeof(logFilename), "speakerGroup%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_SG;
			}
			else if (logLevel & LOG_WORDNET)
			{
				lastClock = &lastWNClock; logFile = &logWNFile;
				snprintf(logFilename, sizeof(logFilename), "wordNet%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_WORDNET;
			}
			else if (logLevel & LOG_WEBSEARCH)
			{
				lastClock = &lastWSClock; logFile = &logWSFile;
				snprintf(logFilename, sizeof(logFilename), "webSearch%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_WEBSEARCH;
			}
			else if (logLevel & LOG_QCHECK)
			{
				lastClock = &lastQCClock; logFile = &logQCFile;
				snprintf(logFilename, sizeof(logFilename), "questionAnswerCheck%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_QCHECK;
			}
			else if (logLevel & LOG_WIKIPEDIA)
			{
				lastClock = &lastWPClock; logFile = &logWPFile;
				snprintf(logFilename, sizeof(logFilename), "wikipedia%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_WIKIPEDIA;
			}
			else if (logLevel & LOG_ROLE)
			{
				lastClock = &lastRoleClock; logFile = &logRoleFile;
				snprintf(logFilename, sizeof(logFilename), "role%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_ROLE;
			}
			else if (logLevel & LOG_WCHECK)
			{
				lastClock = &lastWCClock; logFile = &logWCFile;
				snprintf(logFilename, sizeof(logFilename), "wcheck%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_WCHECK;
			}
			else if (logLevel & LOG_TIME)
			{
				lastClock = &lastTimeClock; logFile = &logTimeFile;
				snprintf(logFilename, sizeof(logFilename), "time%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_TIME;
			}
			else if (logLevel & LOG_DICTIONARY)
			{
				lastClock = &lastDictionaryClock; logFile = &logDictionaryFile;
				snprintf(logFilename, sizeof(logFilename), "dictionary%s.lplog", logExtension.c_str());
				logLevel &= ~LOG_DICTIONARY;
			}
			if (logFileExtension[0])
			{
				// Batch B3: '/' rather than Windows' '\'. The directory name itself
				// ("multiprocessor logs") is unchanged, so an existing log tree
				// copied over from a Windows run is still found.
				char logFilenameTmp[1024];
				snprintf(logFilenameTmp, sizeof(logFilenameTmp), "multiprocessor logs/%s", logFilename);
				strcpy(logFilename, logFilenameTmp);
			}
			if (s == NULL)
			{
#ifdef LOG_BUFFER
				if (*logFile) fclose(*logFile);
				*logFile = NULL;
#else
				if (*logFile >= 0) close(*logFile);
				*logFile = -1;
#endif
				continue;
			}
			if (clock() - *lastClock > logCache * CLOCKS_PER_SEC)
			{
#ifdef LOG_BUFFER
				if (*logFile) fclose(*logFile);
				*logFile = NULL;
#else
				if (*logFile >= 0) close(*logFile);
				*logFile = -1;
#endif
				* lastClock = clock();
			}
#ifdef LOG_BUFFER
			if (*logFile == NULL)
			{
				// write pure unicode log
				//bool writeBOM=access(logFilename,0)<0; 
				//*logFile=fopen(logFilename,"a+b");
				//if (writeBOM)
				//	fputs("\xFF\xFE",*logFile);
				// Batch B3: the mode string was "a+t,ccs=UTF-8" -- MSVC's extension
				// that puts the stream in wide-character mode and transcodes what
				// lp_fputws writes into UTF-8 on the way out. No other CRT understands
				// it, and lp_fputws itself takes a real wchar_t* (4 bytes here), not
				// lpchar_t. Both go away together: encode to UTF-8 explicitly (see
				// the fputs below) and open the file as a plain append stream. The
				// bytes on disk are the same UTF-8 the Windows build produced.
				*logFile = fopen(logFilename, "a");
				if (*logFile) setvbuf(*logFile, NULL, _IOFBF, 1024 * 1024);
			}
			if (!*logFile) return -1;
			if (fputs(lp_utf16_to_utf8(lpwstring(s)).c_str(), *logFile) == EOF)
				printf("Error in fputs - %s\n", strerror(errno));
			if (!logCache)
			{
				fclose(*logFile);
				*logFile = NULL;
			}
			if (logLevel & LOG_FATAL_ERROR)
			{
				cProfile::accumulateNetworkTime(u"", 0, 0);
				lp_wprintf(u"%s", s);
				if (*logFile != NULL)
					fclose(*logFile);
				*logFile = NULL;
				fatalExit();
			}
#else
			// Batch B3: POSIX open() rather than MSVC's ::open. There is no O_BINARY
			// equivalent because there is no text mode to opt out of on macOS.
			// (This whole branch is inert -- LOG_BUFFER is defined in logging.h --
			// but it is ported rather than left as a landmine for whoever turns
			// LOG_BUFFER off.)
			if (*logFile < 0)
				*logFile = open(logFilename, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
			if (*logFile < 0) return -1;
			int queryLength;
			thread_local static void* buffer = 0; // per thread
			thread_local static unsigned int bufSize = 0; // per thread
			WideCharToMultiByte((lpchar_t*)s, queryLength, buffer, bufSize);
			//int len=WideCharToMultiByte( CP_ACP, 0, s, -1,buffer, LOG_BUFFER_SIZE, NULL, NULL )-1;
			write(*logFile, buffer, queryLength - 1);
			if (logLevel & LOG_STDOUT)
				printf("%s\n", (char*)buffer);
			if (!logCache)
			{
				close(*logFile);
				*logFile = -1;
			}
			if (logLevel & LOG_FATAL_ERROR)
			{
				cProfile::accumulateNetworkTime(u"", 0, 0);
				lp_wprintf(u"%s", s);
				if (*logFile != -1)
					close(*logFile);
				*logFile = -1;
				fatalExit();
			}
#endif
		}
	return 0;
}

// Close every non-FATAL log FILE* (flush).  Omits LOG_FATAL_ERROR so this
// cannot accidentally abort.
int lplog(void)
{
	LFS
		return logstring(LOG_MASK & ~LOG_FATAL_ERROR, NULL);
}

// Format at LOG_INFO and append a newline.  format==NULL flushes ALL levels
// including FATAL in the mask (but s==NULL so FATAL's exit path is not taken).
int lplog(const lpchar_t* format, ...)
{
	LFS
		if (format == NULL) return logstring(LOG_MASK, NULL);
	// construct var string
	lpchar_t buf[LOG_BUFFER_SIZE];
	va_list marker;
	va_start(marker, format);
	if (lp_vsnprintf(buf, LOG_BUFFER_SIZE - 3, format, marker) < 0)
		buf[LOG_BUFFER_SIZE - 2] = 0;
	va_end(marker);
	lp_strcpy((buf) + lp_strlen(buf), u"\n");
	logstring(LOG_INFO, buf);
	return 0;
}

// Format at logLevel, append a newline, OR LOG_ERROR in if FATAL is set, then
// logstring.  Does not return when logLevel includes LOG_FATAL_ERROR.
// format==NULL flushes every non-FATAL level.
int lplog(int logLevel, const lpchar_t* format, ...)
{
	LFS
		if (format == NULL) return logstring(LOG_MASK & ~LOG_FATAL_ERROR, NULL);
	//if (logLevel==LOG_RESOLUTION && !traceSpeakerResolution)
	//	return 0;
	//if (logLevel==LOG_DICTIONARY) // turn this on later
	//	return 0;
	//if ((logLevel==LOG_WIKIPEDIA) && !traceWikipedia)
	//	return 0;
	//if ((logLevel==LOG_WEBSEARCH) && !traceWebSearch)
	//	return 0;
	//if ((logLevel==LOG_QCHECK) && !traceQCheck)
	//	return 0;
	// construct var string
	lpchar_t buf[LOG_BUFFER_SIZE];
	va_list marker;
	va_start(marker, format);
	if (lp_vsnprintf(buf, LOG_BUFFER_SIZE - 3, format, marker) < 0)
		buf[LOG_BUFFER_SIZE - 2] = 0;
	va_end(marker);
	lp_strcpy((buf) + lp_strlen(buf), u"\n");
	if (logLevel & LOG_FATAL_ERROR) logLevel |= LOG_ERROR;
	logstring(logLevel, buf);
	return 0;
}

// Like lplog(logLevel,...) but does not append a newline ("NR" = no return-
// character, not "no return").  The FATAL block after logstring is unreachable
// because logstring() already called fatalExit().
int lplogNR(int logLevel, const lpchar_t* format, ...)
{
	LFS
		if (format == NULL) return logstring(LOG_MASK & ~LOG_FATAL_ERROR, NULL);
	// construct var string
	lpchar_t buf[LOG_BUFFER_SIZE];
	va_list marker;
	va_start(marker, format);
	if (lp_vsnprintf(buf, LOG_BUFFER_SIZE - 3, format, marker) < 0)
		buf[LOG_BUFFER_SIZE - 2] = 0;
	va_end(marker);
	if (logLevel & LOG_FATAL_ERROR) logLevel |= LOG_ERROR;
	logstring(logLevel, buf);
	if (logLevel & LOG_FATAL_ERROR)
	{
		logstring(LOG_MASK, NULL);
		lp_wprintf(u"%s", buf); // buf is runtime data, never a format string
		fatalExit();
	}
	return 0;
}
