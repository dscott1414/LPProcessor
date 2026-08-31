/*
	logging.cpp - lplog / logstring implementation: per-level UTF-8 log files and FATAL exit

	Overview:
		logstring() is the sink.  It peels bits off logLevel, opens (or reuses) a
		FILE* named <level><logFileExtension>.lplog (under "multiprocessor logs\\"
		when the extension is non-empty), and fputws the wide message as UTF-8.
		FILE*s stay open for logCache seconds (or forever if logCache is large).
		lplog() formats into a LOG_BUFFER_SIZE wchar_t stack buffer, appends a
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
		- log*File - process-wide FILE* when LOG_BUFFER is defined (the default);
			TLS int fds in the #else path.
		- logFileExtension / multiProcess - TLS, set by child workers.
		- logCache - seconds a FILE* is kept (40).

	Notes / gotchas:
		- LOG_FATAL_ERROR is routed first as LOG_INFO (main.lplog).  lplog() also
			ORs LOG_ERROR, but logstring() sees FATAL after the info write and
			terminates, so error.lplog never gets the fatal line.
		- The FATAL path goes through fatalExit(): exits EXIT_FAILURE, and waits for
			a keypress only when interactive (multiProcess==0 and stdin is a tty).

		- With LOG_BUFFER, FILE*s are shared across threads that have different
			logFileExtension values - a race, and one thread can fclose another's file.
		- sprintf into logFilename[1024] is unbounded in logFileExtension length.
*/
#include <windows.h>
#define _WINSOCKAPI_ /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "profile.h"
#include "QuestionAnswering.h"

__declspec(thread) static int lastInfoClock = 0, lastErrorClock = 0, lastNomatchClock = 0, lastResolutionClock = 0, lastWhereClock = 0, lastResCheckClock = 0, lastSGClock = 0, lastWNClock = 0, lastWPClock = 0, lastWSClock = 0, lastRoleClock = 0, lastWCClock = 0, lastTimeClock = 0, lastDictionaryClock = 0, lastQCClock = 0; 		// per thread
#ifdef LOG_BUFFER
static FILE* logInfoFile, * logErrorFile, * logNomatchFile, * logResolutionFile, * logResCheckFile, * logSGFile, * logWNFile, * logWPFile;
static FILE* logWSFile, * logWhereFile, * logRoleFile, * logWCFile, * logTimeFile, * logDictionaryFile, * logQCFile;
#else
__declspec(thread) static int logInfoFile = -1, logErrorFile = -1, logNomatchFile = -1, logResolutionFile = -1, logResCheckFile = -1, logSGFile = -1, logWNFile = -1, logWPFile = -1, logWSFile = -1, logWhereFile = -1, logRoleFile = -1, logWCFile = -1, logTimeFile = -1, logDictionaryFile = -1, logQCFile = -1; // per thread
#endif
__declspec(thread) wstring logFileExtension; // parallel processing will overload this variable 
__declspec(thread) int multiProcess = 0; // initialized
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
	if (multiProcess == 0 && _isatty(_fileno(stdin)))
	{
		wprintf(L"\nPress Enter to close.\n");
		char buf[16];
		if (!fgets(buf, sizeof(buf), stdin))
			clearerr(stdin);
	}
	exit(EXIT_FAILURE);
}

// Write 's' to every log file whose bit is set in logLevel.  s==NULL closes
// those files (used as a flush).  LOG_FATAL_ERROR writes main.lplog then calls
// fatalExit(), so this function does not return for a fatal level.
// Returns 0, or -1 if fopen/fputws failed (FATAL still exits first).
int logstring(int logLevel, const wchar_t* s)
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
			sprintf(logFilename, "main%S.lplog", logFileExtension.c_str()); // give it a default so compiler doesn't complain
			if ((logLevel & LOG_INFO) || (logLevel & LOG_FATAL_ERROR))
			{
				lastClock = &lastInfoClock; logFile = &logInfoFile;
				sprintf(logFilename, "main%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_INFO;
			}
			else if (logLevel & LOG_ERROR)
			{
				lastClock = &lastErrorClock; logFile = &logErrorFile;
				sprintf(logFilename, "error%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_ERROR;
			}
			else if (logLevel & LOG_NOTMATCHED)
			{
				lastClock = &lastNomatchClock; logFile = &logNomatchFile;
				sprintf(logFilename, "nomatch%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_NOTMATCHED;
			}
			else if (logLevel & LOG_RESOLUTION)
			{
				lastClock = &lastResolutionClock; logFile = &logResolutionFile;
				sprintf(logFilename, "resolution%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_RESOLUTION;
			}
			else if (logLevel & LOG_WHERE)
			{
				lastClock = &lastWhereClock; logFile = &logWhereFile;
				sprintf(logFilename, "where%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_WHERE;
			}
			else if (logLevel & LOG_RESCHECK)
			{
				lastClock = &lastResCheckClock; logFile = &logResCheckFile;
				sprintf(logFilename, "rescheck%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_RESCHECK;
			}
			else if (logLevel & LOG_SG)
			{
				lastClock = &lastSGClock; logFile = &logSGFile;
				sprintf(logFilename, "speakerGroup%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_SG;
			}
			else if (logLevel & LOG_WORDNET)
			{
				lastClock = &lastWNClock; logFile = &logWNFile;
				sprintf(logFilename, "wordNet%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_WORDNET;
			}
			else if (logLevel & LOG_WEBSEARCH)
			{
				lastClock = &lastWSClock; logFile = &logWSFile;
				sprintf(logFilename, "webSearch%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_WEBSEARCH;
			}
			else if (logLevel & LOG_QCHECK)
			{
				lastClock = &lastQCClock; logFile = &logQCFile;
				sprintf(logFilename, "questionAnswerCheck%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_QCHECK;
			}
			else if (logLevel & LOG_WIKIPEDIA)
			{
				lastClock = &lastWPClock; logFile = &logWPFile;
				sprintf(logFilename, "wikipedia%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_WIKIPEDIA;
			}
			else if (logLevel & LOG_ROLE)
			{
				lastClock = &lastRoleClock; logFile = &logRoleFile;
				sprintf(logFilename, "role%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_ROLE;
			}
			else if (logLevel & LOG_WCHECK)
			{
				lastClock = &lastWCClock; logFile = &logWCFile;
				sprintf(logFilename, "wcheck%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_WCHECK;
			}
			else if (logLevel & LOG_TIME)
			{
				lastClock = &lastTimeClock; logFile = &logTimeFile;
				sprintf(logFilename, "time%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_TIME;
			}
			else if (logLevel & LOG_DICTIONARY)
			{
				lastClock = &lastDictionaryClock; logFile = &logDictionaryFile;
				sprintf(logFilename, "dictionary%S.lplog", logFileExtension.c_str());
				logLevel &= ~LOG_DICTIONARY;
			}
			if (logFileExtension[0])
			{
				char logFilenameTmp[1024];
				sprintf(logFilenameTmp, "multiprocessor logs\\%s", logFilename);
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
				//bool writeBOM=_access(logFilename,0)<0; 
				//*logFile=fopen(logFilename,"a+b");
				//if (writeBOM)
				//	fputs("\xFF\xFE",*logFile);
				*logFile = fopen(logFilename, "a+t,ccs=UTF-8");
				setvbuf(*logFile, NULL, _IOFBF, 1024 * 1024);
			}
			if (!*logFile) return -1;
			if (fputws(s, *logFile) == WEOF)
				printf("Error in fputws - %d\n", (int)GetLastError());
			if (!logCache)
			{
				fclose(*logFile);
				*logFile = NULL;
			}
			if (logLevel & LOG_FATAL_ERROR)
			{
				cProfile::accumulateNetworkTime(L"", 0, 0);
				wprintf(L"%s", s);
				if (*logFile != NULL)
					fclose(*logFile);
				*logFile = NULL;
				fatalExit();
			}
#else
			if (*logFile < 0)
				*logFile = _open(logFilename, _O_CREAT | _O_WRONLY | _O_BINARY | _O_APPEND, _S_IREAD | _S_IWRITE);
			if (*logFile < 0) return -1;
			int queryLength;
			__declspec(thread) static void* buffer = 0; // per thread
			__declspec(thread) static unsigned int bufSize = 0; // per thread
			WideCharToMultiByte((wchar_t*)s, queryLength, buffer, bufSize);
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
				cProfile::accumulateNetworkTime(L"", 0, 0);
				wprintf(L"%s", s);
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
int lplog(const wchar_t* format, ...)
{
	LFS
		if (format == NULL) return logstring(LOG_MASK, NULL);
	// construct var string
	wchar_t buf[LOG_BUFFER_SIZE];
	va_list marker;
	va_start(marker, format);
	if (_vsnwprintf(buf, LOG_BUFFER_SIZE - 3, format, marker) < 0)
		buf[LOG_BUFFER_SIZE - 2] = 0;
	va_end(marker);
	wcscat(buf, L"\n");
	logstring(LOG_INFO, buf);
	return 0;
}

// Format at logLevel, append a newline, OR LOG_ERROR in if FATAL is set, then
// logstring.  Does not return when logLevel includes LOG_FATAL_ERROR.
// format==NULL flushes every non-FATAL level.
int lplog(int logLevel, const wchar_t* format, ...)
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
	wchar_t buf[LOG_BUFFER_SIZE];
	va_list marker;
	va_start(marker, format);
	if (_vsnwprintf(buf, LOG_BUFFER_SIZE - 3, format, marker) < 0)
		buf[LOG_BUFFER_SIZE - 2] = 0;
	va_end(marker);
	wcscat(buf, L"\n");
	if (logLevel & LOG_FATAL_ERROR) logLevel |= LOG_ERROR;
	logstring(logLevel, buf);
	return 0;
}

// Like lplog(logLevel,...) but does not append a newline ("NR" = no return-
// character, not "no return").  The FATAL block after logstring is unreachable
// because logstring() already called fatalExit().
int lplogNR(int logLevel, const wchar_t* format, ...)
{
	LFS
		if (format == NULL) return logstring(LOG_MASK & ~LOG_FATAL_ERROR, NULL);
	// construct var string
	wchar_t buf[LOG_BUFFER_SIZE];
	va_list marker;
	va_start(marker, format);
	if (_vsnwprintf(buf, LOG_BUFFER_SIZE - 3, format, marker) < 0)
		buf[LOG_BUFFER_SIZE - 2] = 0;
	va_end(marker);
	if (logLevel & LOG_FATAL_ERROR) logLevel |= LOG_ERROR;
	logstring(logLevel, buf);
	if (logLevel & LOG_FATAL_ERROR)
	{
		logstring(LOG_MASK, NULL);
		wprintf(L"%s", buf); // buf is runtime data, never a format string
		fatalExit();
	}
	return 0;
}
