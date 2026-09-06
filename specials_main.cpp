/*
	specials_main.cpp - specials.vcxproj entry: corpus-maintenance / Stanford-check /
	HMM tools.  Near-duplicate of main.cpp's globals and process-spawning, but it
	is not the parser pipeline.

	Overview:
		Defines the same iterator sentinels, cProfile statics and SRWLOCKs as
		main.cpp (they cannot be linked in one binary).  wmain() does not parse
		main.cpp's -Book/-mp/... command line; it dispatches on -step N (and a
		few switches) to word-frequency harvest, DBpedia cache sweeps,
		pattern dumps, Stanford PCFG/Maxent agreement, and multi-source Viterbi.

	Pipeline position:
		Offline / specials only.  Does not call processSource().  Several helpers
		take cSource by value (full document + MYSQL copy).

	Drift vs main.cpp (do not treat these as the same program):
		- No initialize(): no crash filter, ConsoleHandler, createLocks(), or
		  CACHEDIR existence check.  The locks need no initialization call in
		  either binary since batch B3 made them std::shared_mutex (see general.h).
		- createLPProcess has no threadHandle out-param (main.cpp added one and
		  still leaks it); this copy just closes pi.hThread itself instead.
		  processParameters is const and cast to LPWSTR.
		- startProcesses is the old inlined waiter (main extracted
		  waitToSpawnMoreProcesses / sendBreakSignals / a createLPProcess wrapper).
		  processKind==0 does not force REQUEST_TYPE (unlike main.cpp's version).
		- wmain always builds a GUTENBERG cSource and uses proc2 as the work
		  queue, not sources.processed.
		- Includes hmm.h / JNI / thread-future headers that main.cpp does not.

	Key entry points:
		- wmain() - -step / -stanfordCheck / -executeAgainstDB dispatch
		- populateWordFrequencyTable*() / writeWordFormsFromCorpusWideAnalysis()
		- stanfordCheck() / stanfordCheckMP() / stanfordCheckTest()
		- testViterbiHMMMultiSource()
		- startProcesses() / createLPProcess() - leftover controller (rarely used
		  here; stanfordCheckMP spawns CorpusAnalysis.exe)

	Dependencies:
		MySQL (sources.proc2, wordfrequencymemory, words/wordforms, noRDFTypes,
		stanfordPCFGParsedSentences), JNI Stanford, caches under M:\ and J:\,

	Notes / gotchas:
		- Many paths LOCK TABLES and return without UNLOCK.
		- SQL is built by interpolating words/filenames throughout.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include <fcntl.h>
#include "bncc.h"
#include "mysql.h"
#include <sys/stat.h>
	extern "C" {
#include <yajl_tree.h>
	}
#include "getMusicBrainz.h"
#include "profile.h"
#include "mysqldb.h"
#include "mysqld_error.h"
#include "internet.h"
#include "lpProcess.h"
#include <execinfo.h>
#include <signal.h>
#include <sys/wait.h>
#include<jni.h>
#include "hmm.h"
#include "QuestionAnswering.h" // batch B4b: cQuestionAnswering::fileCaching is defined below
#include "thread"
#include "future"
#include "mutex"
#include <algorithm>


void getSentenceWithTags(cSource &source, int patternBegin, int patternEnd, int sentenceBegin, int sentenceEnd, int PEMAPosition, lpwstring &sentence);
bool unlockTables(MYSQL &mysql);

// needed for _STLP_DEBUG - these must be set to a legal, unreachable yet never changing value
unordered_map <lpwstring,cSourceWordInfo> static_wordMap;
tIWMM wNULL=static_wordMap.end();
unordered_map<lpwstring, cSourceWordInfo::cRMap::cRelation> static_tIcMap;
cSourceWordInfo::cRMap::tIcRMap tNULL=(cSourceWordInfo::cRMap::tIcRMap)static_tIcMap.begin();
vector <cLocalFocus> static_cLocalFocus;
vector <cLocalFocus>::iterator cNULL=static_cLocalFocus.begin();
vector <cSource::cSpeakerGroup> static_cSpeakerGroup;
vector <cSource::cSpeakerGroup>::iterator sgNULL = (vector <cSource::cSpeakerGroup>::iterator)static_cSpeakerGroup.begin();
vector <cWordMatch> static_wm;
vector <cWordMatch>::iterator wmNULL=static_wm.begin();
set<int> static_setInt;
set<int>::iterator sNULL= static_setInt.begin();
std::shared_mutex rdfTypeMapSRWLock, mySQLTotalTimeSRWLock, totalInternetTimeWaitBandwidthControlSRWLock, mySQLQueryBufferSRWLock, orderedHyperNymsMapSRWLock;

// profiling
int64_t cProfile::cb;
int64_t cProfile::accumulatedOverheadTime=0;
unordered_map <string ,int64_t > cProfile::counterMap;
unordered_map <string ,int > cProfile::counterNumMap;
unordered_map <string,cProfile::CP> cProfile::timeMapTotal;
int64_t cProfile::totalCount=0;
string cProfile::functionPath;
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::timeSetCompare> cProfile::timeSort; // sort map by time taken by function
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::memorySetCompare> cProfile::memorySort; // sort map by memory allocated by function
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::countSetCompare> cProfile::countSort; // sort map by number of times function is called
int64_t cProfile::mySQLTotalTime=0;
std::shared_mutex cProfile::networkTimeSRWLock;
int cProfile::totalInternetTimeWaitBandwidthControl;
int64_t cProfile::accumulationNetworkProfileTimer;
int64_t cProfile::accumulateOnlyNetTimer;
int64_t cProfile::lastNetworkTimePrinted;
int64_t cProfile::accumulateNetworkTimeCount;
int cProfile::lastNetClock;
unordered_map < lpwstring, int64_t > cProfile::netAndSleepTimes,cProfile::onlyNetTimes,cProfile::numTimesPerURL;

// Batch B4b: matches main.cpp and word.h's declaration -- these are written by a
// signal handler, where only volatile sig_atomic_t is safe to touch.
volatile sig_atomic_t exitNow = 0, exitEventually = 0;
// Batch B4b: two more definitions main.cpp owns and this binary needs its own copy
// of, for the same reason as cQuestionAnswering::fileCaching below -- lpcore
// references both, and the two entry-point files cannot be linked together.
int cInternet::internetWebSearchRetryAttempts = 1;
bool preTaggedSource = false; // BNC
int overallTime;
int initializeCounter(void);
void freeCounter(void);
bool TSROverride = false, flipTOROverride = false, flipTNROverride = false, logMatchedSentences=false, logUnmatchedSentences=false;

// Dump the current call stack via LOG_FATAL_ERROR (does not return).
// Batch B4b: backtrace()/backtrace_symbols() replace stacktrace.h's dbg::stack_trace(),
// identically to main.cpp's copy -- see the note there about file/line numbers.
void printStackTrace()
{
	std::stringstream buff;
	buff << ":  General Software Fault! \n";
	buff << "\n";

	void* frames[128];
	int numFrames = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
	buff << "Callstack: \n";
	char** symbols = backtrace_symbols(frames, numFrames);
	if (symbols)
	{
		for (int i = 0; i < numFrames; i++)
			buff << symbols[i] << "\n";
		free(symbols);
	}
	else
		for (int i = 0; i < numFrames; i++)
			buff << "0x" << std::hex << (uintptr_t)frames[i] << std::dec << "\n";
	::lplog(LOG_FATAL_ERROR, u"%S", buff.str().c_str());
}

// set_new_handler: log and exit(1).  Unlike main.cpp this is never installed
// because specials has no initialize().
void no_memory () {
	lplog(LOG_FATAL_ERROR,u"Out of memory (new/STL allocation).");
	exit (1);
}

// Batch B4b: this file's own CreateProcess wrapper and signalCtrl are deleted.
// Both are now shared with main.cpp through lpProcess.h (lpSpawnProcess /
// lpSignalInterrupt / lpWaitForAnyChildProcess), which is the whole point of that
// unit existing -- this file and main.cpp had two separately-drifting copies of the
// same controller plumbing, and the drift is documented in this file's own header.

bool getNextUnprocessedSource(MYSQL &mysql, int begin, int end, int sourceType, bool setUsed, int &id, lpwstring &path, lpwstring &encoding, lpwstring &start, int &repeatStart, lpwstring &etext, lpwstring &author, lpwstring &title);
int getNumSources(MYSQL &mysql, int sourceType, bool left);
bool anymoreUnprocessedForUnknown(MYSQL &mysql, int sourceType, int step);
// Batch B4b: getNumSourcesProcessed was CALLED by startProcesses below but defined
// only in main.cpp, which specials.vcxproj does not compile -- so CorpusAnalysis
// could never actually have linked on Windows either. Defined here now, matching
// this file's existing pattern of carrying its own copy of everything main.cpp owns
// (see the "Drift vs main.cpp" note in the file header).
// Batch B4b: another static main.cpp owns that this binary needs its own copy of
// (see the file header: the two entry points cannot be linked together).
bool cQuestionAnswering::fileCaching = true;

int getNumSourcesProcessed(MYSQL &mysql, int sourceType, int &numSourcesProcessed, int64_t &wordsProcessed, int64_t &sentencesProcessed)
{
	MYSQL_RES *result;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&mysql, u"LOCK TABLES sources WRITE")) return -1;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select COUNT(id), SUM(numWords), SUM(numSentences) from sources where sourceType = %d and processed IS not NULL and processing IS NULL and start != '**SKIP**' and start != '**START NOT FOUND**'", sourceType);
	if (myquery(&mysql, qt, result))
	{
		MYSQL_ROW sqlrow = NULL;
		if ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			// SUM() is SQL NULL on an empty corpus, so each column is checked before
			// being read -- main.cpp's copy does not, and calls atol on a null pointer.
			numSourcesProcessed = (sqlrow[0]) ? atoi(sqlrow[0]) : 0;
			wordsProcessed = (sqlrow[1]) ? atoll(sqlrow[1]) : 0;
			sentencesProcessed = (sqlrow[2]) ? atoll(sqlrow[2]) : 0;
		}
		mysql_free_result(result);
	}
	if (!myquery(&mysql, u"UNLOCK TABLES")) return -1;
	return 0;
}
// Old inlined controller (main.cpp split this into wait/spawn helpers).
// processKind 0/1 spawn releasex64\lp.exe; 2 spawns releasex64\CorpusAnalysis.exe.
// Returns -1 immediately if chdir("source") fails.  Non-REQUEST_TYPE ends in _exit(0).
int startProcesses(MYSQL &mysql, int sourceType, int processKind, int step, int beginSource, int endSource, cSource::sourceTypeEnum processSourceType, int maxProcesses, int numSourcesPerProcess,
	bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite, bool makeCopyBeforeSourceWrite, bool parseOnly, lpwstring specialExtension)
{
	LFS
		if (chdir("source") < 0)
			return -1;
	bool sentBreakSignals = false;
	int startTime = clock();
	// Batch B4b: pid_t table and the shared waiter, matching main.cpp.
	pid_t *childPids = (pid_t *)calloc(maxProcesses, sizeof(pid_t));
	if (!childPids)
		lplog(LOG_FATAL_ERROR, u"could not allocate the child process table for -mp %d", maxProcesses);
	int numProcesses = 0, errorCode = 0, numSourcesProcessedOriginally = 0;
	int64_t wordsProcessedOriginally = 0, sentencesProcessedOriginally = 0;
	getNumSourcesProcessed(mysql,sourceType,numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally);
	int numSourcesLeft = getNumSources(mysql,sourceType,true);
	maxProcesses = min(maxProcesses, numSourcesLeft);
	lpwstring tmpstr;
	while (!errorCode)
	{
		unsigned int nextProcessIndex = numProcesses;
		if (numProcesses == maxProcesses)
		{
			int exitStatus = 0;
			int exitedIndex = lpWaitForAnyChildProcess(childPids, numProcesses, 1000 * 60 * 5, exitStatus);
			numSourcesLeft = 0;
			int numSourcesProcessedNow = 0;
			int64_t wordsProcessedNow = 0, sentencesProcessedNow = 0;
			getNumSourcesProcessed(mysql,sourceType,numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
			int processingSeconds = max(1, (int)((clock() - startTime) / CLOCKS_PER_SEC)); // batch B4b: a divisor of zero is a SIGFPE crash here
			lpchar_t consoleTitle[1500];
			numSourcesProcessedNow -= numSourcesProcessedOriginally;
			wordsProcessedNow -= wordsProcessedOriginally;
			sentencesProcessedNow -= sentencesProcessedOriginally;
			lp_wsprintf(consoleTitle, u"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].",
				numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, wordsProcessedNow * 3600 / processingSeconds);
			lplog(LOG_INFO | LOG_ERROR, u"%s", consoleTitle);
			lpReportProgress(consoleTitle);

			if (exitedIndex == LP_WAIT_TIMEOUT)
				continue;
			if (exitedIndex == LP_WAIT_FAILED)
				lplog(LOG_FATAL_ERROR, u"waiting for child processes failed - %S", strerror(errno));
			nextProcessIndex = (unsigned int)exitedIndex;
			printf("\nClosing process %d", nextProcessIndex);
		}
		int id, repeatStart;
		lpwstring start, path, encoding, etext, author, title, pathInCache;
		bool result = true;
		if (processKind == 1 && numSourcesLeft > 0)
			numSourcesLeft--;
		else
		{
			switch (processKind)
			{
			case 0:
			case 1:result = getNextUnprocessedSource(mysql, beginSource, endSource, sourceType, false, id, path, encoding, start, repeatStart, etext, author, title); break;
			case 2:result = anymoreUnprocessedForUnknown(mysql, sourceType, step); break;
			default:result = false; break;
			}
		}
		if (!result)
		{
			if (numProcesses == maxProcesses)
			{
				memmove(childPids + nextProcessIndex, childPids + nextProcessIndex + 1, (maxProcesses - nextProcessIndex - 1) * sizeof(childPids[0]));
				numProcesses--;
			}
			if (numProcesses)
			{
				printf("\nNo more processes to be created. %d processes left to wait for.", numProcesses);
				// Batch B4b: the Win32 call here waited for ALL of them at once
				// (bWaitAll=true); lpWaitForAnyChildProcess reports one at a time,
				// so this drains them in a loop instead.
				for (int remaining = numProcesses; remaining > 0; remaining--)
				{
					int drainStatus = 0;
					if (lpWaitForAnyChildProcess(childPids, numProcesses, 1000 * 60 * 60, drainStatus) == LP_WAIT_FAILED)
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
					if (childPids[p] > 0)
						lpSignalInterrupt(childPids[p]);
				sentBreakSignals = true;
			}
		}
		else
		{
			pid_t processId = 0;
			std::vector<std::string> arguments;
			auto narrow = [](const lpwstring& wide) { return std::string(lp_utf16_to_utf8(wide)); };
			auto number = [](long long value) { return std::to_string(value); };
			// Batch B4b: argv vectors and sibling-executable discovery, matching
			// main.cpp. "releasex64\\lp.exe" and "releasex64\\CorpusAnalysis.exe" were
			// Windows build-output paths; the two binaries now live next to this one.
			switch (processKind)
			{
			case 0:
			case 1:
				arguments.push_back(lpSiblingExecutablePath("lp"));
				if (processKind == 0)
				{
					arguments.push_back("-ParseRequest");
					arguments.push_back(narrow(pathInCache));
				}
				else
				{
					arguments.push_back("-book"); arguments.push_back("0"); arguments.push_back("+");
					arguments.push_back("-BC"); arguments.push_back("0");
				}
				arguments.push_back("-cacheDir"); arguments.push_back(narrow(CACHEDIR));
				if (forceSourceReread)         arguments.push_back("-forceSourceReread");
				if (sourceWrite)               arguments.push_back("-SW");
				if (sourceWordNetRead)         arguments.push_back("-SWNR");
				if (sourceWordNetWrite)        arguments.push_back("-SWNW");
				if (parseOnly)                 arguments.push_back("-parseOnly");
				if (makeCopyBeforeSourceWrite) arguments.push_back("-MCSW");
				if (processKind == 1)
				{
					arguments.push_back("-numSourceLimit");
					arguments.push_back(number(numSourcesPerProcess));
				}
				arguments.push_back("-log"); arguments.push_back(number(nextProcessIndex));
				if (processKind == 1 && specialExtension.length() > 0)
				{
					arguments.push_back("-specialExtension");
					arguments.push_back(narrow(specialExtension));
				}
				break;
			case 2:
				arguments.push_back(lpSiblingExecutablePath("CorpusAnalysis"));
				arguments.push_back("-cacheDir"); arguments.push_back(narrow(CACHEDIR));
				arguments.push_back("-step"); arguments.push_back(number(step));
				arguments.push_back("-numSourceLimit"); arguments.push_back(number(numSourcesPerProcess));
				arguments.push_back("-log"); arguments.push_back(number(nextProcessIndex));
				break;
			default: break;
			}
			if (!arguments.empty() && lpSpawnProcess(processId, arguments) < 0)
				processId = 0;
			childPids[nextProcessIndex] = processId;
			if (numProcesses < maxProcesses)
				numProcesses++;
			printf("\nCreated process %d:%d", nextProcessIndex, (int)processId);
		}
	}
	if (processSourceType != cSource::REQUEST_TYPE)
	{
		freeCounter();
		_exit(0); // fast exit
	}
	free(childPids);
	chdir("..");
	return 0;
}

// Batch B4b: setConsoleWindowSize is deleted, exactly as in main.cpp -- console
// buffer and window dimensions are the user's terminal settings on macOS and
// cannot be imposed from inside the process.


bool detectNonEuropeanWord(lpwstring word);
int cacheWebPath(lpwstring webAddress, lpwstring &buffer, lpwstring epath, lpwstring cacheTypePath, bool forceWebReread, bool &networkAccessed);
int discoverInflections(set <int> posSet, bool plural, lpwstring word);

// Sets isNonEuropean and returns 0.  Nothing else: every parameter but `word` and
// `isNonEuropean` is now unused.
//
// NOTE: this no longer produces a part-of-speech set, and has no dictionary source
// of any kind.  It used to fill posSet from the Merriam-Webster Collegiate API and
// then check the word against dictionary.com; both integrations have been removed
// at the author's request.  posSet is left as the caller supplied it and
// `inflections` is untouched.  Callers that depend on POS discovery need a
// replacement dictionary source wired in here.
int getWordPOS(MYSQL *mysql,lpwstring word, set <int> &posSet, int &inflections, bool print, bool &isNonEuropean, bool logEverything)
{
	LFS
	(void)mysql; (void)posSet; (void)inflections; (void)print; (void)logEverything;
	if (isNonEuropean = detectNonEuropeanWord(word))
		return 0;
	return 0;
}


// SELECT/INSERT words.  MYSQL is passed by value (copies the connection).
// word is interpolated in quotes.  Returns 0 if existed, 1 if inserted, -1 on error.
int createWordInDBIfNecessary(MYSQL mysql, int sourceId, int &wordId, lpwstring word, bool actuallyExecuteAgainstDB, bool logEverything)
{
	LFS
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numWordsInserted = 0;
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id from words where word=\"%s\"", word.c_str());
	if (myquery(&mysql, qt, result)) // if result is null, also returns false
	{
		if (sqlrow = mysql_fetch_row(result))
		{
			wordId = atoi(sqlrow[0]);
			mysql_free_result(result);
			return 0; // did not create word
		}
	}
	lp_strcpy(qt, u"INSERT IGNORE INTO words (word,inflectionFlags,flags,timeFlags,mainEntryWordId,derivationRules,sourceId) VALUES "); // IGNORE is necessary because C++ treats unicode strings as different, but MySQL treats them as the same
	int len = lp_strlen(qt) , inflectionFlags = 0, flags = 0, timeFlags = 0, derivationRules = 0;
	lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u"(\"%s\",%d,%d,%d,%d,%d,%d)",
		word.c_str(), inflectionFlags, flags, timeFlags, -1, derivationRules, sourceId);
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&mysql, qt))
			return -1;
		if (!myquery(&mysql, u"SELECT LAST_INSERT_ID()", result))
			return -1;
		if (sqlrow = mysql_fetch_row(result))
			wordId = atoi(sqlrow[0]);
		mysql_free_result(result);
	}
	else if (logEverything)
		lplog(LOG_INFO, u"DB statement [%s create word]: %s", word.c_str(), qt);
	return 1; // created word
}

// Diff existing wordforms (DB formId = LP form+1) against posSetDB.  Closed-class
// forms are kept; open/unknown/combination are replaced.  Also syncs noun/verb
// usage-pattern formIds.  Out: remove/add/keep and maxcount (transfer count).
int	analyzeFormsUsageVSNewForms(MYSQL mysql, int wordId, lpwstring word, bool properNoun, bool existingWord, set <int> posSetDB, set <int> &remove, set<int> &add, set<int> &keep, int &maxcount,bool logEverything)
{
	bool properNounFormFound = false;
	maxcount = 127; // maximum
	if (existingWord)
	{
		lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"select formId,count from wordforms where wordId=%d and (formId<=%d or formId=%d)", wordId, cSourceWordInfo::patternFormNumOffset,
			cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset); // formId in (1,101,102,103,104)
		MYSQL_RES * result;
		MYSQL_ROW sqlrow = NULL;
		if (!myquery(&mysql, qt, result))
			return -1;
		while (sqlrow = mysql_fetch_row(result))
		{
			int formId = atoi(sqlrow[0]);
			int count = atoi(sqlrow[1]);
			// record maximum usage count
			if (formId == cSourceWordInfo::patternFormNumOffset)
			{
				maxcount = count;
				continue;
			}
			// keep proper noun usage count if set as proper noun
			if (formId == cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset)
			{
				properNounFormFound = true;
				if (properNoun)
					keep.insert(formId);
				else
					remove.insert(formId);
				continue;
			}
			bool formUnknownCombinationOrOpen = (formId == (UNDEFINED_FORM_NUM + 1) || formId == (nounForm + 1) || formId == (adjectiveForm + 1) || formId == (verbForm+1) || formId == (adverbForm+1) || formId == (COMBINATION_FORM_NUM + 1));
			// forms to be set (in posSetDB) are assumed to be only open classes (noun,verb,adjective,adverb) or classes returned from a dictionary lookup
			// form to keep in DB - that is not unknown, and not combination and not open, and needs to be set
			bool formToKeepInDB = posSetDB.find(formId) != posSetDB.end() || !formUnknownCombinationOrOpen;
			// don't print unknown, combination, open forms or abbreviations or proper nouns, if this current word has been determined to be a proper noun
			if (!properNoun || !(formUnknownCombinationOrOpen || formId==(abbreviationForm+1) || formId==(PROPER_NOUN_FORM_NUM+1)) || logEverything)
				lplog(LOG_INFO, u"%s:original form: %s [%d] %s", word.c_str(), Forms[formId - 1]->name.c_str(), count, (formToKeepInDB) ? u"WILL BE KEPT" : u"WILL BE REMOVED");
			// noun=101, adjective=102,verb=103,adverb=104
			if (formToKeepInDB)
			{
				posSetDB.erase(formId);
				keep.insert(formId);
				if (formId == nounForm+1)
				{
					keep.insert(cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
					keep.insert(cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
				}
				if (formId == verbForm+1)
				{
					keep.insert(cSourceWordInfo::VERB_HAS_0_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
					keep.insert(cSourceWordInfo::VERB_HAS_1_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
					keep.insert(cSourceWordInfo::VERB_HAS_2_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
				}
			}
			else
			{
				remove.insert(formId);
				if (formId == nounForm + 1)
				{
					remove.insert(cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
					remove.insert(cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
				}
				if (formId == verbForm + 1)
				{
					remove.insert(cSourceWordInfo::VERB_HAS_0_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
					remove.insert(cSourceWordInfo::VERB_HAS_1_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
					remove.insert(cSourceWordInfo::VERB_HAS_2_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
				}
			}
		}
	}
	else
		add.insert(cSourceWordInfo::patternFormNumOffset);
	if (properNoun && !properNounFormFound)
	{
		add.insert(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
	}
	for (int formId : posSetDB)
	{
		if (!properNoun || logEverything || formId!=nounForm+1)
			lplog(LOG_INFO, u"%s:new form: %s WILL BE ADDED", word.c_str(), Forms[formId-1]->name.c_str());
		add.insert(formId);
		if (formId == nounForm + 1)
		{
			add.insert(cSourceWordInfo::SINGULAR_NOUN_HAS_DETERMINER - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
			add.insert(cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
		}
		if (formId == verbForm + 1)
		{
			add.insert(cSourceWordInfo::VERB_HAS_0_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
			add.insert(cSourceWordInfo::VERB_HAS_1_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
			add.insert(cSourceWordInfo::VERB_HAS_2_OBJECTS - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
		}
	}
	//LOWER_CASE_USAGE_PATTERN = 32756
	return 0;
}

// DELETE remove-set formIds then INSERT add-set.  formsDeleted is
// mysql_affected_rows after delete (0 if not actuallyExecuteAgainstDB).
int overwriteWordFormsInDB(MYSQL mysql, int wordId, lpwstring word, set <int> &posSetDB, int64_t &formsDeleted,bool properNoun,bool existingWord, bool actuallyExecuteAgainstDB,bool logEverything)
{
	LFS
	set <int> remove, add, keep;
	int maxcount;
	analyzeFormsUsageVSNewForms(mysql, wordId, word, properNoun, existingWord, posSetDB, remove, add, keep,maxcount, logEverything);
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (existingWord)
	{
		// erase all open wordforms and unknown form associated with wordId in wordforms
		// keep all others - this will allow forms like demonym to be kept.
		remove.insert(1); // unknown
		lpwstring removeFormQueryString,removeFormString;
		for (int r : remove)
		{
			removeFormQueryString += lp_narrow_to_wide(std::to_string(r)) + u",";
			if (r < 32750 && (logEverything || !properNoun || (r != (UNDEFINED_FORM_NUM+1) && r != (adjectiveForm+1) && r != (verbForm + 1) && r != (adverbForm + 1) && r !=(COMBINATION_FORM_NUM+1))))
				removeFormString += Forms[r - 1]->name + u" ";
		}
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"delete from wordforms where formId in (%s) and wordId=%d", removeFormQueryString.substr(0, removeFormQueryString.length() - 1).c_str(),wordId);
		if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
			return -1;
		else if (removeFormString.length()>0)
			lplog(LOG_INFO, u"DB statement [%s delete]: %s", word.c_str(), removeFormString.c_str());
		formsDeleted = mysql_affected_rows(&mysql);
		if (keep.find(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset) != keep.end())
		{
			lp_snprintf(qt, QUERY_BUFFER_LEN, u"update wordforms set count=%d where wordId=%d and formId=%d", maxcount,wordId, cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset);
			if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
				return -1;
			else if (logEverything || !maxcount)
				lplog(LOG_INFO, u"DB statement [%s update counts]: %s", word.c_str(), qt);
		}
	}
	if (add.size() > 0)
	{
		lp_strcpy(qt, u"insert wordForms(wordId,formId,count) VALUES ");
		int len = lp_strlen(qt);
		// insert wordForms(wordId, formId, count) VALUES(2, 33, 4), (5, 6, 7)
		for (int form : add)
		{
			// if transfer count (cSourceWordInfo::patternFormNumOffset) or cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN, set this to maxcount
			// else set to 0.
			if (form == cSourceWordInfo::patternFormNumOffset || form == cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN - cSourceWordInfo::MAX_FORM_USAGE_PATTERNS + cSourceWordInfo::patternFormNumOffset)
				len += lp_wsprintf_at(qt, len, u"(%d,%d,%d),", wordId, form, maxcount);
			else
				len += lp_wsprintf_at(qt, len, u"(%d,%d,%d),", wordId, form, 0);
		}
		qt[len - 1] = 0;
		lp_strcpy((qt) + lp_strlen(qt), u" ON DUPLICATE KEY UPDATE count=VALUES(count)");
		if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
			return -1;
		else
		{
			lpwstring forms;
			for (int form : add)
				if (form < cSourceWordInfo::patternFormNumOffset && (!properNoun || form!=(nounForm+1)))
					forms += Forms[form-1]->name + u" ";
			if (forms.length()>0)
				lplog(LOG_INFO, u"DB statement [%s forms insert]: %s (%s)", word.c_str(), forms.c_str(), qt);
		}
	}
	return 0;
}

// queryOnLowerCase will query for word forms  the next time the word is encountered in all lower case.
// queryOnLowerCase = 4
// OR queryOnLowerCase into words.flags for word (interpolated).
int overwriteWordFlagsInDB(MYSQL mysql, lpwstring word, bool actuallyExecuteAgainstDB,bool logEverything)
{
	LFS
	// erase all wordforms associated with wordId in wordforms
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"update words set flags=flags|%d where word=\"%s\"", cSourceWordInfo::queryOnLowerCase, word.c_str());
	if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
		return -1;
	else if (logEverything)
		lplog(LOG_INFO, u"DB statement [%s add queryOnLowerCase flag]: %s", word.c_str(), qt);
	return 0;
}

// PLURAL refers to noun plural form.  inflectionFlags is a bitfield, so it is
// ORed (not added) with inflections, matching overwriteWordFlagsInDB above.
// word is interpolated.
int overwriteWordInflectionFlagsInDB(MYSQL mysql, lpwstring word, int inflections, bool actuallyExecuteAgainstDB)
{
	LFS
	// erase all wordforms associated with wordId in wordforms
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"update words set inflectionFlags=inflectionFlags|%d where word=\"%s\"", inflections,word.c_str());
	if (actuallyExecuteAgainstDB && !myquery(&mysql, qt))
		return -1;
	else
	{
		lpwstring sFlags;
		lplog(LOG_INFO, u"DB statement [%s add inflection flag]:%s", word.c_str(), inflectionFlagsToStr(inflections, sFlags));
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



// Walk dbPediaCache: empty .rdfTypes/.erdfTypes become noRDFTypes/noERDFTypes
// rows and are deleted; old version files are removed.  startPath/startHit
// resume from RDFTypesScanProgress.txt.  MYSQL by value.
void scanAllRDFTypes(MYSQL mysql, lpchar_t *startPath, bool &startHit, const lpchar_t *basepath, int &numFilesProcessed, int &numNotOpenable, int &numNewestVersion, 
	int &numOldVersion, int &removeErrors, int &populatedRDFs,int &numERDFRemoved,
	unordered_map<lpwstring,int> &extensions, unordered_map<lpwstring, int64_t> &extensionSpace)
{
	// Batch B4b: lpDirectoryEntries replaces the
	// FindFirstFile/FindNextFile/FindClose loop; lpEntry is the entry name,
	// which is what lpEntry held.
	lpwstring lpBase(basepath);
	for (const lpwstring &lpEntryString : lpDirectoryEntries(lpBase, u"*"))
	{
		const lpchar_t *lpEntry = lpEntryString.c_str();
		// Batch B4b: stat replaces WIN32_FIND_DATA's split 32-bit size fields.
		struct stat lpEntryStatus;
		int64_t lpEntrySize = (lp_wstat((lpBase + u"/" + lpEntryString).c_str(), &lpEntryStatus) == 0) ? (int64_t)lpEntryStatus.st_size : 0;
		const lpchar_t *ext = lp_strrchr(lpEntry, u'.');
		if (ext && ext[1]!=0)
		{

			extensions[ext]++;
			extensionSpace[ext] += lpEntrySize;
		}
		bool isRDFType = false, isERDFType = false, isJSON=false ;
		// must be an rdfTypes extension
		if (lpEntry[0] == '.' || (lp_strlen(lpEntry) > 10 && 
			(!(isRDFType= lp_wcscasecmp(lpEntry + lp_strlen(lpEntry) - 9, u".rdfTypes") == 0) && 
			 !(isJSON=    lp_wcscasecmp(lpEntry + lp_strlen(lpEntry) - 5, u".json") == 0) &&
			 !(isERDFType=lp_wcscasecmp(lpEntry + lp_strlen(lpEntry) - 10, u".erdfTypes") == 0))))
			continue;
		numFilesProcessed++;
		lpchar_t completePath[1024];
		lp_wsprintf(completePath, u"%s/%s", basepath, lpEntry);
		if (!startHit && lp_strcmp(startPath, completePath) == 0)
			startHit = true;
		if (lp_wIsDirectory(completePath))
		{
			if (startHit || lp_strncmp(startPath, completePath,lp_strlen(completePath)) == 0)
				scanAllRDFTypes(mysql, startPath, startHit, completePath, numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs, numERDFRemoved,extensions,extensionSpace);
		}
		else
		{
			if ((numFilesProcessed & 63) == 0)
			{
				lpwstring extstr;
				for (auto const&[ext, num] : extensions)
				{
					if (num > 1)
					{
						lpchar_t buf[1024];
						lp_wsprintf(buf, u"%s:%d[%I64d] ", ext.c_str(), num, extensionSpace[ext]);
						extstr += buf;
					}
				}
				printf("%08d:unopenable=%02d newest=%08d old=%08d cannot remove=%08d populated=%07d numERDFRemoved=%07d [%s][%S]\r", numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs, numERDFRemoved, (startHit) ? "HIT" : "NOT HIT",extstr.c_str());
			}

			if (isERDFType && (lpEntrySize == 10))
			{
				lpchar_t qt[2048];
				// Batch B4b: lpEntry points into the directory listing and is const;
				// take a copy before truncating the extension off it.
				lpwstring truncatedName(lpEntry);
				if (truncatedName.length() > 10) truncatedName.erase(truncatedName.length() - 10);
				lp_wsprintf(qt, u"INSERT INTO noERDFTypes VALUES ('%s')", truncatedName.c_str());
				if (lp_strlen(lpEntry) > 127 || myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY)
				{
					if (lp_wremove(completePath))
					{
						lp_wprintf(u"\nremove failed on path %s (%d)\n", completePath, (int)errno);
						removeErrors++;
					}
					else
						numERDFRemoved++;
					continue;
				}
			}
			if (isRDFType && lpEntrySize == 2)
			{
				lpchar_t qt[2048];
				// Batch B4b: see the noERDFTypes site above -- lpEntry is const.
				lpwstring truncatedName(lpEntry);
				if (truncatedName.length() > 9) truncatedName.erase(truncatedName.length() - 9);
				lp_wsprintf(qt, u"INSERT INTO noRDFTypes VALUES ('%s')", truncatedName.c_str());
				if (truncatedName.length() > 127 || myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY)
				{
					if (lp_wremove(completePath))
					{
						lp_wprintf(u"\nremove failed on path %s (%d)\n", completePath, (int)errno);
						removeErrors++;
					}
					continue;
				}
			}
			int fd;
			if ((fd = lp_wopen(completePath, O_RDWR | O_BINARY)) < 0)
			{
				numNotOpenable++;
				printf("\nscanAllRDFTypes:Cannot read path %S - %s.\n", completePath, sys_errlist[errno]);
				continue;
			}
			lpchar_t version;
			::read(fd, &version, sizeof(version));
			::close(fd);
			if ((isRDFType && version != RDFLIBRARYTYPE_VERSION) || (isERDFType && version != EXTENDED_RDFTYPE_VERSION))
			{
				numOldVersion++;
				if (lp_wremove(completePath))
				{
					lp_wprintf(u"\nremove failed on path %s (%d)\n", completePath, (int)errno);
					removeErrors++;
				}
				else if (isERDFType)
					numERDFRemoved++;
				continue;
			}
			populatedRDFs++;
			if (startHit && (populatedRDFs & 63) == 0)
			{
				FILE *progressFile = lp_wfopen(u"RDFTypesScanProgress.txt", "w");
				if (progressFile)
				{
					lp_fputws(completePath, progressFile);
					fclose(progressFile);
				}
			}
		}
	}
	return;
}

// Recursively lp_wremove files whose names contain a non-ASCII / non-digit / non-_- char.
void removeIllegalNames(const lpchar_t *basepath)
{
	// Batch B4b: lpDirectoryEntries replaces the
	// FindFirstFile/FindNextFile/FindClose loop; lpEntry is the entry name,
	// which is what lpEntry held.
	lpwstring lpBase(basepath);
	for (const lpwstring &lpEntryString : lpDirectoryEntries(lpBase, u"*"))
	{
		const lpchar_t *lpEntry = lpEntryString.c_str();
		// must be an rdfTypes extension
		if (lpEntry[0] == '.') // || (lp_strlen(lpEntry) > 10 && lp_strcmp(lpEntry + lp_strlen(lpEntry) - 10, u".erdfTypes") != 0))
			continue;
		lpchar_t completePath[1024];
		lp_wsprintf(completePath, u"%s/%s", basepath, lpEntry);
		if (lp_wIsDirectory(completePath))
			removeIllegalNames(completePath);
		bool isIllegal = false;
		for (int I = 0; lpEntry[I] && !isIllegal; I++)
			if (!iswascii(lpEntry[I]) && !iswdigit(lpEntry[I]) && lpEntry[I] != u'-' && lpEntry[I] != u'_')
				isIllegal = true;
		if (isIllegal)
		{
			lp_wremove(completePath);
			lp_wprintf(u"removed %-200.200s\r", completePath);
		}
	}
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
	// Per-word frequency / flag bag for wordfrequencymemory inserts.
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



// Batched INSERT ... ON DUPLICATE KEY UPDATE into wordfrequencymemory.
// wf is copied.  word is interpolated in double quotes (SQL injection / quote break).
void writeSourceWordFrequency(MYSQL *mysql,unordered_map<lpwstring, wordInfo> wf, lpwstring etext)
{
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int currentEntry = 0, totalEntries = wf.size(), percent = 0,len=0;
	int lastSourceEtext = lp_wtoi(etext.c_str());
	for (auto const&[word, wi] : wf)
	{
		if (len == 0)
			len += lp_snprintf(qt, QUERY_BUFFER_LEN, u"INSERT INTO wordfrequencymemory (word,totalFrequency,unknownFrequency,capitalizedFrequency,allCapsFrequency,lastSourceEtext,nonEuropeanFlag,numberFlag,cardinalFlag,ordinalFlag,romanFlag,dateFlag,timeFlag,telephoneFlag,moneyFlag,webaddressFlag) VALUES");
		len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u" (\"%s\",%d,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)", word.c_str(), wi.totalFrequency, wi.unknownFrequency, wi.unknownCapitalizedFrequency, wi.unknownAllCapsFrequency, lastSourceEtext,
			(wi.nonEuropeanWord) ? u"true" : u"false",
			(wi.number) ? u"true" : u"false",
			(wi.cardinal) ? u"true" : u"false",
			(wi.ordinal) ? u"true" : u"false",
			(wi.roman) ? u"true" : u"false",
			(wi.date) ? u"true" : u"false",
			(wi.time) ? u"true" : u"false",
			(wi.telephone) ? u"true" : u"false",
			(wi.money) ? u"true" : u"false",
			(wi.webaddress) ? u"true" : u"false");
		if (len > QUERY_BUFFER_LEN_UNDERFLOW)
		{
			len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u" ON DUPLICATE KEY UPDATE totalFrequency=totalFrequency+VALUES(totalFrequency),unknownFrequency=unknownFrequency+VALUES(unknownFrequency),capitalizedFrequency=capitalizedFrequency+VALUES(capitalizedFrequency),allCapsFrequency=allCapsFrequency+VALUES(allCapsFrequency),lastSourceEtext=VALUES(lastSourceEtext)");
			if (!myquery(mysql, qt)) return;
			len = 0;
		}
		else
			qt[len++] = u',';
	}
	if (len > 0)
	{
		qt[--len] = u' ';
		len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u" ON DUPLICATE KEY UPDATE totalFrequency=totalFrequency+VALUES(totalFrequency),unknownFrequency=unknownFrequency+VALUES(unknownFrequency),capitalizedFrequency=capitalizedFrequency+VALUES(capitalizedFrequency),allCapsFrequency=allCapsFrequency+VALUES(allCapsFrequency),lastSourceEtext=VALUES(lastSourceEtext)");
		if (!myquery(mysql, qt)) return;
	}
}

// Load the parsed source (by value) and scan for Gutenberg end-matter.
// reprocess is set if the end is before 99% of tokens.  Returns -1 on lock fail.
int analyzeEnd(cSource source, int sourceId, lpwstring path, lpwstring etext, lpwstring title,bool &reprocess,bool &nosource,lpwstring specialExtension)
{
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) 
		return -1;
	Words.readWords(path, sourceId, false,u"");
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, specialExtension))
	{
		bool multipleEnds = false;
		for (unsigned int I = 0; I < source.sentenceStarts.size(); I++)
		{
			int s = source.sentenceStarts[I], sEnd = (I == source.sentenceStarts.count - 1) ? source.m.size() : source.sentenceStarts[I + 1];
			if (source.analyzeEnd(path, s, sEnd, multipleEnds))
			{
				lpwstring tempPhrase;
				if (reprocess = (s * 100 / source.m.size() < 99))
					lp_wprintf(u"%50.50s (%02I64d): End detected at position %07d (totalWords=%07I64d) - %s\n", title.c_str(), s * 100 / source.m.size(), s, source.m.size(), source.phraseString(s, sEnd, tempPhrase, true, u" ").c_str());
				break;
			}
		}
	}
	else
		nosource = true;
	if (!myquery(&source.mysql, u"UNLOCK TABLES"))
		return -1;
	return 0;
}

// Step 2: words in wordfrequencymemory that are >95% unknown get DB forms.
// First SUM() result is not freed before the second SELECT.  LOCK TABLES words
// after the SELECT implicitly unlocks wordfrequencymemory while result is still open.
int writeWordFormsFromCorpusWideAnalysis(MYSQL mysql,bool actuallyExecuteAgainstDB)
{
	int definedUnknownWord = 0;
	int I = 0, sumTotalFrequency = 0, sumProcessedTotalFrequency = 0;
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	lpwstring word;
	if (!myquery(&mysql, u"LOCK TABLES wordfrequencymemory READ"))
		return -1;
	if (!myquery(&mysql, u"select SUM(totalFrequency) from wordfrequencymemory where unknownFrequency*100/totalFrequency>95 and"
		u" nonEuropeanFlag =false and numberFlag = false and cardinalFlag = false and	ordinalFlag = false and	romanFlag = false and dateFlag = false and timeFlag = false and	telephoneFlag = false and	moneyFlag = false and	webaddressFlag = false order by unknownFrequency desc", result))
		return -1;
	if (sqlrow = mysql_fetch_row(result))
		sumTotalFrequency = atoi(sqlrow[0]);
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select word, totalFrequency, unknownFrequency, capitalizedFrequency,allCapsFrequency,lastSourceEtext from wordfrequencymemory where unknownFrequency*100/totalFrequency>95 and"
		u" nonEuropeanFlag =false and numberFlag = false and cardinalFlag = false and	ordinalFlag = false and	romanFlag = false and dateFlag = false and timeFlag = false and	telephoneFlag = false and	moneyFlag = false and	webaddressFlag = false order by unknownFrequency desc");
	if (!myquery(&mysql, qt, result))
		return -1;
	my_ulonglong totalWords = mysql_num_rows(result), totalFormsDeleted=0;
	int numWordsProcessed = 0;
	if (actuallyExecuteAgainstDB)
	{
		if (!myquery(&mysql, u"LOCK TABLES words WRITE,wordforms WRITE"))
			return -1;
	}
	else if (!myquery(&mysql, u"LOCK TABLES words READ,wordforms READ"))
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
		if (word.find_first_of(u"ãâäáàæçêéèêëîíïñôóòöõôûüùú\'") != lpwstring::npos && (totalFrequency < 25 || capitalizedFrequency * 100 / totalFrequency < 99))
			continue;
		// find a definition for the word.
		// if exists, erase all forms associated with this word and write the new forms (return true)
		// else definition could not be found (return false)
		set <int> posSet;
		bool isNonEuropean, setProperNoun;
		int inflections=0;
		if (setProperNoun = (totalFrequency > 4 && ((capitalizedFrequency + allCapsFrequency)*100.0 / totalFrequency) > 95.0))
			posSet.insert(cForms::gFindForm(u"noun"));
		else
			getWordPOS(&mysql,word, posSet, inflections, false, isNonEuropean,true);
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
				lplog(LOG_FATAL_ERROR, u"DB error unable to create word %s", word.c_str());
			int64_t formsDeleted=0;
			if (overwriteWordFormsInDB(mysql, wordId, word, posSetDB, formsDeleted, setProperNoun, ret==0, actuallyExecuteAgainstDB, logEverything) < 0)
				lplog(LOG_FATAL_ERROR, u"DB error setting word forms with word %s", word.c_str());
			totalFormsDeleted += formsDeleted;
			if (setProperNoun && totalFrequency < 25 && overwriteWordFlagsInDB(mysql, word, actuallyExecuteAgainstDB,false) < 0)
				lplog(LOG_FATAL_ERROR, u"DB error setting word flags with word %s", word.c_str());
			if (inflections > 0 && overwriteWordInflectionFlagsInDB(mysql, word, inflections, actuallyExecuteAgainstDB) < 0)
				lplog(LOG_FATAL_ERROR, u"DB error setting word inflection flags with word %s", word.c_str());
			definedUnknownWord++;
		}
		numWordsProcessed++;
		// remember word for further sources
		lp_wprintf(u"%03I64d:%15.15s:unknown=%06d/%06d [UpperCase=%05.1f] [totalFormsDeleted=%08I64d] frequency %I64d%% done\r",
			numWordsProcessed*100/totalWords,word.c_str(), definedUnknownWord, numWordsProcessed,
			((capitalizedFrequency + allCapsFrequency)*100.0 / totalFrequency), totalFormsDeleted, ((int64_t)sumProcessedTotalFrequency)*100/ sumTotalFrequency);
	}
	mysql_free_result(result);
	if (!myquery(&mysql, u"UNLOCK TABLES"))
		return -1;
	return 0;
}

// Log unknown / UNDEFINED_FORM tokens.  Takes source by reference (avoids
// copying the live MYSQL connection) and UNLOCKs on every return path.
// Returns 21 on success, 20 if the source cache is missing, -20 on lock fail.
int printUnknownsFromSource(cSource &source, int sourceId, lpwstring path, lpwstring etext, lpwstring specialExtension)
{
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, u"");
	bool parsedOnly = false, firstIllegal = false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, specialExtension))
	{
		int wordIndex = 0;
		for (cWordMatch &im : source.m)
		{
			if (im.word->second.isUnknown())
			{
				lplog(LOG_INFO, u"SI%d: WI%d: %s", sourceId, wordIndex, im.word->first.c_str());
			}
			else if (im.word->second.query(UNDEFINED_FORM_NUM) > 0)
			{
				if (!firstIllegal)
				{
					lplog(LOG_INFO, u"REPARSE SI%d", sourceId);
					firstIllegal = true;
				}
				lplog(LOG_INFO, u"SI%d: WI%d: %s [ILLEGAL]", sourceId, wordIndex, im.word->first.c_str());
			}
			wordIndex++;
		}
	}
	else
	{
		lp_wprintf(u"Unable to read source %d:%s\n", sourceId, path.c_str());
		myquery(&source.mysql, u"UNLOCK TABLES");
		return 20;
	}
	myquery(&source.mysql, u"UNLOCK TABLES");
	return 21;
}

// matchType 0=pattern+diff, 1=winner form, 2=surface word, 3=flagNotMatched, 4=always.
// PMAOffset is only meaningful for type 0.
bool matchEntity(cSource &source, int wordIndex, int matchType, lpwstring patternOrWordName, lpwstring differentiator, int &PMAOffset)
{
	auto &im = source.m[wordIndex];
	return (matchType == 4 ||
		(matchType == 0 && (PMAOffset = im.pma.queryPatternDiff(patternOrWordName, differentiator)) != -1) ||
		(matchType == 1 && im.queryWinnerForm(patternOrWordName)!=-1) ||
		(matchType == 2 && im.word->first == patternOrWordName) ||
		(matchType == 3 && (im.flags&cWordMatch::flagNotMatched) != 0));
}

// Extra filter for pattern dumps.  Currently always true (the real checks are
// commented out), so every primary match is logged.
bool additionalMatchingLogic(cSource &source, int wordIndex, int primaryPMAOffset, int secondaryPMAOffset,lpwstring &logicResults)
{
	return true;
	//logicResults = u"";
	//// if __ALLOBJECTS_1 starts with one adverb which is one word long
	//int primaryPatternEnd = wordIndex + source.m[wordIndex].pma[primaryPMAOffset].len;
	////int secondaryPatternEnd = wordIndex + source.m[wordIndex].pma[secondaryPMAOffset].len;
	//if (source.m[wordIndex].pma[primaryPMAOffset].len<15)
	//	return false;
	//// compare the noun at the beginning and the noun at the end
	//// get positions
	//int beginObjectPosition = source.m[wordIndex].principalWherePosition;
	//int lastObjectPosition = primaryPatternEnd - 1;
	//if (beginObjectPosition<0)
	//{
	//	logicResults += u"NO_PRINCIPAL:";
	//}
	//else if (source.m[beginObjectPosition].queryForm(nounForm)==-1)
	//{
	//	logicResults += u"NO_BEGIN_NOUN:";
	//}
	//if (source.m[lastObjectPosition].queryForm(nounForm) == -1)
	//{
	//	logicResults += u"NO_END_NOUN:";
	//}
	//int verbPosition = source.m[source.m[wordIndex].principalWherePosition].getRelVerb();
	//if (verbPosition<0)
	//{
	//	logicResults += u"NO_VERB:";
	//}
	//if (logicResults.length())
	//	return true;
	//// get relations
	//tIWMM beginObjectWord = source.m[beginObjectPosition].getMainEntry();
	//tIWMM lastObjectWord = source.m[lastObjectPosition].getMainEntry();
	//tIWMM verbWord = source.m[verbPosition].getMainEntry();
	//if (verbWord->first==u"would" || verbWord->first == u"am" || verbWord->first == u"do" || verbWord->first == u"be" || verbWord->first == u"have")
	//{
	//	logicResults += u"COMMON_VERB:";
	//}
	//cSourceWordInfo::cRMap::tIcRMap tr = tNULL;
	//int numBeginRelations = beginObjectWord->second.scanAllRelations(verbWord);
	//int numLastRelations = lastObjectWord->second.scanAllRelations(verbWord);
	//lpwstring br, lr;
	//itos(numBeginRelations, br);
	//itos(numLastRelations, lr);
	//// the numRelations are a sum of all relations over all classes.  Therefore using TRANSFER_COUNT which is the total count over all classes.
	//int numBeginFrequency = beginObjectWord->second.wordFrequency;
	//int numLastFrequency = lastObjectWord->second.wordFrequency;
	//lpwstring bf, lf;
	//itos(numBeginFrequency, bf);
	//itos(numLastFrequency, lf);
	//// bias using word frequency but only for ruling out 
	//if (numBeginRelations > 0 && numBeginFrequency > 0 && numLastFrequency / numBeginFrequency>1)
	//	numBeginRelations = (numBeginRelations*numLastFrequency) / numBeginFrequency;
	//else if (numLastRelations > 0 && numLastFrequency > 0 && numBeginFrequency / numLastFrequency < 1)
	//	numLastRelations = (numLastRelations*numBeginFrequency) / numLastFrequency;
	//if (numLastRelations == 0)
	//{
	//	logicResults += u"LAST_ZERO:";
	//}
	//if (numBeginRelations > 0 && numLastRelations > 0)
	//{
	//	if (numBeginRelations > numLastRelations)
	//	{
	//		logicResults += u"BEGIN_IS_CORRECT:";
	//	}
	//	if (numLastRelations > numBeginRelations && numLastRelations*100/numBeginRelations<600)
	//	{
	//		logicResults += u"BEGIN_IS_NOT_CERTAIN:";
	//	}
	//}
	//if (verbPosition!=lastObjectPosition+1 || (source.m[verbPosition].word->second.inflectionFlags&VERB_PAST_PARTICIPLE) == 0)
	//{
	//	logicResults += u"VERB_WRONG_POSITION_OR_TENSE:";
	//}
	//if (logicResults.length())
	//	return true;
	//logicResults = u"BEGIN("+beginObjectWord->first+u")[" + br + u" f "+ bf + u"]LAST(" + lastObjectWord->first + u")[" + lr + u" f " + lf + u"]+VERB("+verbWord->first+u")";
	//return true;
	////return (source.m[wordIndex].pma[primaryPMAOffset].len == 1 || source.m[wordIndex + 1].pma.queryPattern(u"__INTERPPB") != -1 || source.m[wordIndex + 1].pma.queryPattern(u"__C1_IP") != -1);
}

// primaryType, secondaryType:
// 0: pattern - if differentiator is NOT specified, then any differentiator is ok.
// 1: form - any form class (noun/adjective)
// 2: word - the actual word.   
// 3: flagNotMatched is set
// 4: true (do not perform match)
// if both primaryMatchType AND secondaryMatchType>0, then the secondary match location is the NEXT word.
// if primaryMatchType == 3, then sentence highlight will encompass all words that have no match, and sentences will not be repeated.
// Scan one source for primary/secondary matchEntity hits and log the sentence.
// UNLOCKs on every return path.  Returns 22, or 21 if the cache is missing.
int patternOrWordAnalysisFromSource(cSource &source, int sourceId, lpwstring path, lpwstring etext, lpwstring primaryPatternOrWordName, lpwstring primaryDifferentiator, lpwstring secondaryPatternOrWordName, lpwstring secondaryDifferentiator, int primaryMatchType, int secondaryMatchType, lpwstring specialExtension)
{	LFS
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, u"");
	bool parsedOnly = false;
	int lastSentenceIndexPrinted = -1;
	if (source.readSource(path, false, parsedOnly, false, specialExtension))
	{
		// need ONLY if additionalMatchingLogic needs wordRelations - this takes LOTS of time
		//set <int> wordIds;
		//source.readWordIdsNeedingWordRelations(wordIds);
		//if (Words.initializeWordRelationsFromDB(source.mysql, wordIds, true, source.debugTrace.traceParseInfo) < 0)
		//	return -1;
		int wordIndex = 0;
		unsigned int ss = 1;
		for (cWordMatch &im : source.m)
		{
			int primaryPMAOffset,secondaryPMAOffset;
			if (matchEntity(source, wordIndex, primaryMatchType, primaryPatternOrWordName, primaryDifferentiator, primaryPMAOffset) &&
					matchEntity(source, (primaryMatchType>0 && secondaryMatchType>0) ? wordIndex+1:wordIndex, secondaryMatchType, secondaryPatternOrWordName, secondaryDifferentiator, secondaryPMAOffset))
			{
				primaryPMAOffset = primaryPMAOffset & ~cMatchElement::patternFlag;
				secondaryPMAOffset = secondaryPMAOffset & ~cMatchElement::patternFlag;
				lpwstring sentence;
				if (primaryMatchType==0)
				{
					int patternEnd = wordIndex + im.pma[primaryPMAOffset].len;
					lpwstring logicResults;
					/*additional logic begin*/
					if (additionalMatchingLogic(source,wordIndex, primaryPMAOffset, secondaryPMAOffset, logicResults))
					{
						/*additional logic end*/
						getSentenceWithTags(source, wordIndex, patternEnd, source.sentenceStarts[ss - 1], source.sentenceStarts[ss], im.pma[primaryPMAOffset].pemaByPatternEnd, sentence);
						lpwstring adiff = patterns[im.pma[primaryPMAOffset].getPattern()]->differentiator;
						lpwstring path = source.sourcePath.substr(16, source.sourcePath.length() - 20);
						if (primaryDifferentiator.find(u'*') == lpwstring::npos)
						{
							lplog(LOG_ERROR, u"%s", sentence.c_str());
							lplog(LOG_INFO, u"%s[%d-%d]:%s%s", path.c_str(), wordIndex, patternEnd, logicResults.c_str(),sentence.c_str());
						}
						else
						{
							lplog(LOG_ERROR, u"[%s]:%s", adiff.c_str(), sentence.c_str());
							lplog(LOG_INFO, u"%s[%s](%d-%d):%s%s", path.c_str(), adiff.c_str(), wordIndex, patternEnd, logicResults.c_str(),sentence.c_str());
						}
					}
				}
				else if (primaryMatchType!=3)
				{
					lpwstring originalIWord;
					for (int I = source.sentenceStarts[ss - 1]; I < source.sentenceStarts[ss]; I++)
					{
						source.getOriginalWord(I, originalIWord, false, false);
						if (I == wordIndex)
							originalIWord = u"*" + originalIWord + u"*";
						sentence += originalIWord + u" ";
					}
					lplog(LOG_ERROR, u"%s", sentence.c_str());
				}
				else
				{
					lpwstring originalIWord;
					if (lastSentenceIndexPrinted != source.sentenceStarts[ss - 1])
					{
						bool inNoMatch = false;
						for (int I = source.sentenceStarts[ss - 1]; I < source.sentenceStarts[ss]; I++)
						{
							source.getOriginalWord(I, originalIWord, false, false);
							if (I == wordIndex)
							{
								originalIWord = u"*" + originalIWord;
								inNoMatch = true;
							}
							if (inNoMatch && (I + 1 >= source.sentenceStarts[ss] || (source.m[I + 1].flags&cWordMatch::flagNotMatched) == 0))
							{
								originalIWord += u"*";
								inNoMatch = false;
							}
							sentence += originalIWord + u" ";
						}
						lplog(LOG_ERROR, u"%s", sentence.c_str());
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
		lp_wprintf(u"Unable to read source %d:%s\n", sourceId, path.c_str());
		myquery(&source.mysql, u"UNLOCK TABLES");
		return 21;
	}
	myquery(&source.mysql, u"UNLOCK TABLES");
	return 22;
}

// Log sentences that contain a flagNotMatched token.  Takes source by reference
// (avoids copying the live MYSQL connection) and UNLOCKs on every return path.
// Returns 62, or 61 if the cache is missing.
int syntaxCheckFromSource(cSource &source, int sourceId, lpwstring path, lpwstring etext, lpwstring specialExtension)
{
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, u"");
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, specialExtension))
	{
		int wordIndex = 0;
		unsigned int ss = 1,lastssprinted=-1;
		for (cWordMatch &im : source.m)
		{
			/*
			// verb, adverb, adverb, OBJECT_1 that is composed of a single noun that does not accept adjectives
			int pemaOffset= source.queryPatternDiff(wordIndex,u"__S1", u"1"),pmaOffset;
			if (pemaOffset != -1 && im.hasWinnerVerbForm() && im.getNumWinners() == 1 && wordIndex < source.m.size() - 3 &&
				(pmaOffset = source.m[wordIndex + 1].pma.queryPatternDiff(u"__ALLOBJECTS_1", u"1")) != -1)
			{
				pmaOffset &= ~cMatchElement::patternFlag;
				bool twoAdverbs =
					source.m[wordIndex + 1].pma[pmaOffset].len == 3 &&
					source.m[wordIndex + 1].isOnlyWinner(adverbForm) &&
					source.m[wordIndex + 2].isOnlyWinner(adverbForm) && source.m[wordIndex + 2].queryForm(prepositionForm)!=-1 &&
					(source.m[wordIndex + 3].isOnlyWinner(accForm) || source.m[wordIndex + 3].isOnlyWinner(personalPronounForm)) &&
					source.m[wordIndex + 3].word->first!=u"he" && source.m[wordIndex + 3].word->first != u"she";
				bool oneAdverb =
					source.m[wordIndex + 1].pma[pmaOffset].len == 2 &&
					source.m[wordIndex + 1].isOnlyWinner(adverbForm) && source.m[wordIndex + 1].queryForm(prepositionForm) != -1 &&
					(source.m[wordIndex + 2].isOnlyWinner(accForm) || source.m[wordIndex + 2].isOnlyWinner(personalPronounForm)) &&
					source.m[wordIndex + 2].word->first != u"he" && source.m[wordIndex + 2].word->first != u"she";
				if (oneAdverb || twoAdverbs)
				{
					int patternEnd = wordIndex + 1 + source.m[wordIndex + 1].pma[pmaOffset].len;
					lpwstring sentence, originalIWord;
					bool inPattern = false;
					for (int I = source.sentenceStarts[ss - 1]; I < source.sentenceStarts[ss]; I++)
					{
						source.getOriginalWord(I, originalIWord, false, false);
						if (I == wordIndex + 1)
						{
							sentence += u"**";
							inPattern = true;
						}
						sentence += originalIWord;
						if (I == patternEnd - 1)
						{
							sentence += u"**";
							inPattern = false;
						}
						sentence += u" ";
					}
					lpwstring path = source.sourcePath.substr(16, source.sourcePath.length() - 20);
					lplog(LOG_INFO, u"%s[%d-%d]:%s", path.c_str(), wordIndex, patternEnd, sentence.c_str());
					lplog(LOG_ERROR, u"%s", sentence.c_str());
				}
			}
		*/
			if ((im.flags&cWordMatch::flagNotMatched) && lastssprinted!=ss)
			{
				lpwstring sentence;
				source.phraseString(source.sentenceStarts[ss - 1], source.sentenceStarts[ss], sentence, false);
				lpwstring path = source.sourcePath.substr(16, source.sourcePath.length() - 20);
				lplog(LOG_INFO, u"%s:%d:%s", path.c_str(), wordIndex, sentence.c_str());
				lplog(LOG_ERROR, u"%s", sentence.c_str());
				lastssprinted = ss;
			}
			wordIndex++;
			while (ss < source.sentenceStarts.size() && source.sentenceStarts[ss] < wordIndex + 1)
				ss++;
		}
	}
	else
	{
		lp_wprintf(u"Unable to read source %d:%s\n", sourceId, path.c_str());
		myquery(&source.mysql, u"UNLOCK TABLES");
		return 61;
	}
	myquery(&source.mysql, u"UNLOCK TABLES");
	return 62;
}

// Count per-word frequencies / unknown / special-form flags and write
// wordfrequencymemory.  Rejects the source if an unknown token looks "illegal".
// form==4 (noun?) on a capitalized unknown aborts with -2.  Returns 2 or -(n+10).
// Takes source by reference (avoids copying the live MYSQL connection), matching
// the sibling *FromSource helpers above.
int populateWordFrequencyTableFromSource(cSource &source, int sourceId, lpwstring path, lpwstring etext, lpwstring specialExtension)
{
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, u"");
	bool parsedOnly = false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, specialExtension))
	{
		unordered_map<lpwstring, wordInfo> wf;
		int numUnknown = 0;
		wf.reserve(source.m.size());
		for (cWordMatch &im : source.m)
		{
			lpwstring word = im.word->first;
			if (word.empty() || word[word.length() - 1] == u'\\' || word.length() > 32)
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
					for (lpchar_t c : word)
						if (!iswalnum(c) && !cWord::isDoubleQuote(c) && !cWord::isSingleQuote(c) && !cWord::isDash(c) && c != '.' && c != ' ')
							numCharsIncorrect++;
					if (numCharsIncorrect >= 1)
					{
						lplog(LOG_INFO, u"%s: word %s is suspicious - rejecting source.", path.c_str(), word.c_str());
						numIllegalWords++;
					}
					if (im.flags&cWordMatch::flagFirstLetterCapitalized)
					{
						if (!myquery(&source.mysql, u"LOCK TABLES words w READ,wordforms wf READ"))
							return -1;
						MYSQL_RES * result;
						MYSQL_ROW sqlrow = NULL;
						lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
						escapeSingleQuote(word);
						lp_snprintf(qt, QUERY_BUFFER_LEN, u"select wf.formId from words w,wordforms wf where w.id=wf.wordId and w.word='%s'", word.c_str());
						int form = -1;
						if (myquery(&source.mysql, qt, result, true))
						{
							if (mysql_num_rows(result) == 1 && (sqlrow = mysql_fetch_row(result)))
								form = atoi(sqlrow[0]);
							mysql_free_result(result);
						}
						unlockTables(source.mysql);;
						if (form == 4)
							return -2;
					}
				}
			}
			wfi->second.totalFrequency++;
			if (im.word->second.query(UNDEFINED_FORM_NUM) > 0) // im.word->second.isUnknown())
			{
				wfi->second.unknownFrequency++;
				if (im.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (im.flags&cWordMatch::flagFirstLetterCapitalized))
					wfi->second.unknownCapitalizedFrequency++;
				if (im.flags&cWordMatch::flagAllCaps)
					wfi->second.unknownAllCapsFrequency++;
			}
		}
		if (numIllegalWords == 0)
			writeSourceWordFrequency(&source.mysql, wf, etext);
		if (!myquery(&source.mysql, u"LOCK TABLES sources WRITE")) return -1;
		lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"UPDATE sources SET numUnknown=%d,numIllegalWords=%d where id=%d",numUnknown, numIllegalWords,sourceId);
		myquery(&source.mysql, qt);
		unlockTables(source.mysql);;
	}
	else
	{
		lp_wprintf(u"Unable to read source %d:%s\n", sourceId, path.c_str());
		unlockTables(source.mysql);
		return 0;
	}
	return (numIllegalWords == 0) ? 2 : -(numIllegalWords + 10);
}


// Serial harvest of every source with proc2==step (used for steps 10..19).
int populateWordFrequencyTable(cSource source, int step, lpwstring specialExtension)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum cSource::sourceTypeEnum st = cSource::GUTENBERG_SOURCE_TYPE;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numSourcesProcessedNow = 0;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		lpwstring path, etext, title;
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = u"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		path.insert(0, u"\\").insert(0, CACHEDIR);
		int setStep = populateWordFrequencyTableFromSource(source, sourceId, path, etext,specialExtension);
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		lpchar_t buffer[1024];
		int64_t processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
			lp_wsprintf(buffer, u"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		lpReportProgress(buffer);
	}
	mysql_free_result(result);
	return 0;
}

// Walk proc2==step sources and printUnknownsFromSource each.
int printUnknowns(cSource source, int step, lpwstring specialExtension)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum cSource::sourceTypeEnum st = cSource::GUTENBERG_SOURCE_TYPE;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numSourcesProcessedNow = 0;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		lpwstring path, etext, title;
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = u"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		path.insert(0, u"\\").insert(0, CACHEDIR);
		int setStep = printUnknownsFromSource(source, sourceId, path, etext,specialExtension);
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		lpchar_t buffer[1024];
		int64_t processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
			lp_wsprintf(buffer, u"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		lpReportProgress(buffer);
	}
	mysql_free_result(result);
	return 0;
}
  
// Batch pattern/word dump over proc2==step.  Redirects logFileExtension to the
// pattern name.  Gutenberg paths are under CACHEDIR; TEST under LMAINDIR.
int patternOrWordAnalysis(cSource source, int step, lpwstring primaryPatternOrWordName, lpwstring primaryDifferentiator, lpwstring secondaryPatternOrWordName, lpwstring secondaryDifferentiator, enum cSource::sourceTypeEnum st, int primaryMatchType, int secondaryMatchType, lpwstring specialExtension)
{	LFS
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	if (!myquery(&source.mysql, u"LOCK TABLES sources WRITE")) return -1;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numSourcesProcessedNow = 0;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	lplog(LOG_INFO | LOG_ERROR | LOG_NOTMATCHED, NULL); // close all log files to change extension
	logFileExtension = specialExtension+ u"."+primaryPatternOrWordName;
	if (!primaryDifferentiator.empty() && primaryDifferentiator!=u"*")
		logFileExtension += u"[" + primaryDifferentiator + u"]";
	if (!secondaryPatternOrWordName.empty())
	{
		logFileExtension += u"." + secondaryPatternOrWordName;
		if (!secondaryDifferentiator.empty() && secondaryDifferentiator != u"*")
			logFileExtension += u"[" + secondaryDifferentiator + u"]";
	}
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		lpwstring path, etext, title;
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = u"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		if (st== cSource::GUTENBERG_SOURCE_TYPE)
			path.insert(0, u"\\").insert(0, CACHEDIR);
		else if (st == cSource::TEST_SOURCE_TYPE)
			path.insert(0, u"\\").insert(0, LMAINDIR);
		int setStep = patternOrWordAnalysisFromSource(source, sourceId, path, etext, primaryPatternOrWordName, primaryDifferentiator, secondaryPatternOrWordName, secondaryDifferentiator, primaryMatchType, secondaryMatchType, specialExtension);
		if (!myquery(&source.mysql, u"LOCK TABLES sources WRITE")) return -1;
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		lpchar_t buffer[1024];
		int64_t processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
		{
			lp_wsprintf(buffer, u"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
			lpReportProgress(buffer);
		}
	}
	mysql_free_result(result);
	return 0;
}

// Batch unmatched-sentence dump.  test=true uses TEST_SOURCE_TYPE + LMAINDIR.
int syntaxCheck(cSource source, int step, lpwstring specialExtension,bool test)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum cSource::sourceTypeEnum st = (test) ? cSource::TEST_SOURCE_TYPE : cSource::GUTENBERG_SOURCE_TYPE;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numSourcesProcessedNow = 0;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		lpwstring path, etext, title;
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = u"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		if (test)
			path.insert(0, u"\\").insert(0, LMAINDIR);
		else
			path.insert(0, u"\\").insert(0, CACHEDIR);
		int setStep = syntaxCheckFromSource(source, sourceId, path, etext, specialExtension);
		if (!myquery(&source.mysql, u"LOCK TABLES sources WRITE")) return -1;
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		lpchar_t buffer[1024];
		int64_t processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
			lp_wsprintf(buffer, u"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		lpReportProgress(buffer);
	}
	mysql_free_result(result);
	return 0;
}

// delete files associated with sources that have been marked skipped
// update sources set proc2 = 0;
// update sources set proc2 = 6 where start = '**SKIP**';
// Delete .SourceCache / .WNCache / .WordCacheFile for proc2==6 rows (skipped
// sources).  Same SKIP LOCKED / implicit-commit caveats as the MP harvester.
int removeOldCacheFiles(cSource source)
{
	int step = 6;
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select COUNT(*) from sources where proc2=%d",step);
	int64_t totalSource;
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
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"select COUNT(*) from sources where proc2=%d", step);
		if (myquery(&source.mysql, qt, result))
		{
			sqlrow = mysql_fetch_row(result);
			sourcesLeft = atoi(sqlrow[0]);
			mysql_free_result(result);
		}
		if (!myquery(&source.mysql, u"START TRANSACTION"))
			return -1;
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id,path from sources where proc2=%d order by id limit 1 FOR UPDATE SKIP LOCKED", step);
		if (!myquery(&source.mysql, qt, result) || mysql_num_rows(result) != 1)
			break;
		lpwstring path;
		sqlrow = mysql_fetch_row(result);
		int sourceId = atoi(sqlrow[0]);
		mTW(sqlrow[1], path);
		mysql_free_result(result);
		path.insert(0, u"\\").insert(0, CACHEDIR);
		lpwstring temp = path;
		temp += u".SourceCache";
		lp_wremove(temp.c_str());
		temp = path;
		temp += u".WNCache";
		lp_wremove(temp.c_str());
		temp = path;
		temp += u".WordCacheFile";
		lp_wremove(temp.c_str());
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"update sources set proc2=%d where id=%d", step + 1, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		if (!myquery(&source.mysql, u"COMMIT"))
			return -1;

		source.clearSource();
		lpchar_t buffer[1024];
		int64_t processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		int numSourcesProcessedNow = (int)(totalSource - (sourcesLeft - 1));
		if (processingSeconds)
			lp_wsprintf(buffer, u"%%%03I64d:%5d out of %05I64d source in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow - 1, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, path.c_str());
		lpReportProgress(buffer);
	}
	return 0;
}

// One-off: rdfIdentify("clackamas") then getRDFTypes at each "maac" token in
// a hardcoded Jules of the Great Heart path.
void testRDFType(cSource &source, lpwstring specialExtension)
{
	int sourceId = 25291;
	lpwstring path = TEXTDIR u"/texts/Schoonover, Frank E/Jules of the Great Heart  Free Trapper and Outlaw in the Hudson Bay Region in the Early Days.txt";
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE")) return;
	Words.readWords(path, sourceId, false, u"");
	vector <cTreeCat *> rdfTypes;
	cOntology::rdfIdentify(u"clackamas", rdfTypes, u"Z", true);
	bool parsedOnly = false;
	if (source.readSource(path, false, parsedOnly, false, specialExtension))
	{
		int where = 0;
		for (auto im : source.m)
		{
			if (im.word->first == u"maac")
			{
				vector <cTreeCat *> rdfTypes;
				source.getRDFTypes(where, rdfTypes, u"Z", 1000, false, false);
			}
			where++;
		}
	}
	myquery(&source.mysql, u"UNLOCK TABLES");
}

struct {
	const lpchar_t *commonWord;
	const lpchar_t *replace;
} doubleReplace[] = {
	{ u"d'ye", u"do you" },
	{ u"more'n", u"more than" },
	{ u"d'you", u"do you" },
	{ u"t'other", u"the other" },
	{ u"dinna", u"didn't you" },
};

struct {
	const lpchar_t *commonWord;
	const lpchar_t *replace;
} singleReplace[] = {
{ u"thar", u"there" },
{ u"sez", u"says" },
{ u"cap'n", u"captain" },
{ u"s'pose", u"suppose" },
{ u"thet", u"that" },
{ u"mebbe", u"maybe" },
{ u"ther", u"there" },
{ u"ze", u"the" },
{ u"yuh", u"yes" },
{ u"hoss", u"boss" },
{ u"sah", u"sir" },
{ u"tak", u"take" },
{ u"dere", u"there" },
{ u"havin", u"having" },
{ u"h'm", u"him" },
{ u"suh", u"sir" },
{ u"livin", u"living" },
{ u"waitin", u"waiting" },
{ u"seein", u"seeing" },
{ u"purty", u"pretty" },
{ u"ag'in", u"again" },
{ u"leetle", u"little" },
{ u"haf", u"half" },
{ u"meetin", u"meeting" },
{ u"thim", u"them" },
{ u"shewn", u"shown" },
{ u"iz", u"is" },
{ u"gittin", u"getting" },
{ u"sho", u"sure" },
{ u"heah", u"hear" },
{ u"givin", u"giving" },
{ u"reg'lar", u"regular" },
{ u"mebby", u"maybe" },
{ u"on'y", u"only" },
{ u"humouredly", u"humoredly" },
{ u"wud", u"would" },
{ u"m'sieu", u"monsieur" },
{ u"p'raps", u"perhaps" },
{ u"wur", u"were" },
{ u"o’er", u"over" },
{ u"dont", u"don't" },
{ u"fightin", u"fighting" },
{ u"theer", u"there" },
{ u"f'r", u"for" },
{ u"gainst", u"against" },
{ u"iver", u"ever" },
{ u"ev'ry", u"every" },
{ u"bloomin", u"blooming" },
{ u"whut", u"what" },
{ u"lyin", u"lying" },
{ u"puttin", u"putting" },
{ u"worshiped", u"worshipped" },
{ u"didna", u"didn't" },
{ u"hangin", u"hanging" },
{ u"b'lieve", u"believe" },
{ u"lemme", u"let me" },
{ u"huntin", u"hunting" },
{ u"speakin", u"speaking" },
{ u"sunshiny", u"sunshiney" },
{ u"for'ard", u"forward" }
};

// u"thank",u"no",u"never",u"then",u"so",u"number",u"as",u"only", u"van",u"von",u"also",u"not",u"more",u"eg",u"e.g.",u"p.o.",u"like",u" - only used in very specialized patterns
// 	u"p",u"m",u"le",u"de",u"f",u"c",u"k",u"o",u"b",u"!",u"?",
// expand ST choice of POS by these forms
unordered_map<lpwstring, vector <lpwstring> > maxentAssociationMap =
{
	// amplification
	//{u"sectionheader", u"noun"},

	// similarity 
	//{ u"coordinator",{ u"conjunction"} },
	{ u"conjunction",{ u"coordinator" } },
	{ u"Proper Noun",{ u"honorific_abbreviation",u"honorific",u"roman_numeral",u"month",u"interjection",u"daysOfWeek",u"no",u"holiday" } },
	{ u"honorific noun",{ u"honorific",u"honorific_abbreviation" }},
	{ u"modal_auxiliary",{ u"future_modal_auxiliary",u"negation_modal_auxiliary",u"negation_future_modal_auxiliary"} },
	{ u"determiner",{ u"demonstrative_determiner",u"no",u"quantifier",u"predeterminer"} },
	{ u"predeterminer",{ u"quantifier" } },
	{ u"interjection",{ u"no",u"politeness_discourse_marker"} },
	{ u"relativizer", { u"what",u"startquestion" } },
	{ u"particle", { u"adverb",u"preposition",u"quantifier" } }, // LP usually treats particles as adverbs, not sure whether this is strictly correct, but it makes sense to me.
	// include possible subclasses
	{ u"verb", { u"verbverb",u"SYNTAX:Accepts S as Object",u"have",u"have_negation",u"is",u"is_negation",u"does",u"does_negation",u"be",u"been",u"modal_auxiliary",u"negation_modal_auxiliary",u"future_modal_auxiliary",u"negation_future_modal_auxiliary",u"being"} }, // feel, see, watch, hear, tell etc // fancy, say (thinksay verbs)
	// stanford maxent apparently has no indefinite pronoun, so it classes them all as nouns.
	{ u"noun",{ u"uncertainDurationUnit",u"simultaneousUnit",u"dayUnit",u"timeUnit",u"quantifier",u"numeral_cardinal",u"indefinite_pronoun",u"season",u"time_abbreviation" } }, // all, some etc // something, everything
	{ u"adjective",{ u"quantifier",u"numeral_ordinal",u"numeral_cardinal" }},  // many / more
	{ u"adverb",{ u"not",u"never",u"there" }},  // many
	{ u"to",{ u"preposition" }},
	{ u"there",{ u"pronoun",u"adverb" }},
	{ u"no",{ u"adverb" }},
	{ u"which",{ u"interrogative_determiner",u"interrogative_pronoun",u"relativizer"}},
	{ u"what",{ u"interrogative_determiner",u"interrogative_pronoun",u"relativizer"}},
	{ u"who",{ u"interrogative_pronoun",u"relativizer"}},
	{ u"whose",{ u"interrogative_determiner",u"relativizer"}},
	{ u"how",{ u"relativizer",u"conjunction",u"adverb"}}
};

// checks if the part of speech indicated in parse from the Stanford Maxent POS tagger matches the winner forms at wordSourceIndex.
// returns yes=0, no=1
// Compare one Maxent `word_TAG` against LP winners (pennMapToLP +
// maxentAssociationMap).  Several documented LP-vs-ST implementation diffs
// are treated as agreement.  Returns 0 match / 1 mismatch.  Consumes parse.
int checkStanfordMaxentAgainstWinner(cSource &source, int wordSourceIndex, lpwstring originalParse, lpwstring &parse, int &numPOSNotFound, unordered_map<lpwstring, int> &formNoMatchMap, unordered_map<lpwstring, int> &wordNoMatchMap, bool inRelativeClause)
{
	if (!iswalpha(source.m[wordSourceIndex].word->first[0]))
		return 0;
	lpwstring originalWordSave;
	source.getOriginalWord(wordSourceIndex, originalWordSave, false, false);
	// tagger output:
	// ;_: and_CC bunny_NN ,_, and_CC bobtail_NN ,_, and_CC billy_NNP were_VBD always_RB doing_VBG something_NN 
	lpwstring originalWord = u" " + originalWordSave + u"_";
	if (parse.empty())
		return 1;
	if (parse[0] != u' ')
		parse = u" " + parse;
	size_t wow = parse.find(originalWord);
	if (wow == lpwstring::npos)
	{
		transform(originalWord.begin(), originalWord.end(), originalWord.begin(), (int(*)(int)) tolower);
		wow = parse.find(originalWord);
	}
	if (wow == lpwstring::npos)
		return 1;
	if (parse[parse.length() - 1] != u' ')
		parse += u" ";
	wow += originalWord.length();
	auto nextspace = parse.find(u' ', wow);
	if (nextspace == lpwstring::npos)
		return 1;
	lpwstring partofspeech = parse.substr(wow, nextspace - wow);
	parse.erase(0, nextspace);
	extern unordered_map<lpwstring, vector<lpwstring>> pennMapToLP;
	auto lpPOS = pennMapToLP.find(partofspeech);
	if (lpPOS == pennMapToLP.end())
	{
		lplog(LOG_ERROR, u"%d:Part of Speech %s not found.", wordSourceIndex,partofspeech.c_str());
		numPOSNotFound++;
		return 1;
	}
	std::set<lpwstring> posList(lpPOS->second.begin(), lpPOS->second.end());
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
	if (posList.find(u"noun")!=posList.end() && std::find(winnerForms.begin(), winnerForms.end(), PROPER_NOUN_FORM_NUM) != winnerForms.end())
		return 0;
	// Maxent sometimes thinks things are proper nouns, when they are not capitalized.  I have not found an example where this is the case.
	if (posList.find(u"Proper Noun") != posList.end() &&
		//(std::find(winnerForms.begin(), winnerForms.end(), nounForm) != winnerForms.end() || std::find(winnerForms.begin(), winnerForms.end(), adjectiveForm) != winnerForms.end()) && 
		iswlower(originalWordSave[0]))
		return 0;
	// Maxent sometimes thinks things are proper nouns, when they are actually just other forms, even if they are capitalized, usually when they are the first word.
	// I have not found an example where this is the case.
	vector <lpwstring> pnExceptionForms =
	{ u"pronoun", u"pronoun possessive_pronoun", u"conjunction", u"determiner",
		 u"does_negation", u"have_negation", u"indefinite_pronoun", u"is",
		 u"is_negation", u"le", u"modal_auxiliary", u"negation_future_modal_auxiliary",
		 u"negation_modal_auxiliary", u"numeral_ordinal", u"personal_pronoun_nominative",
		 u"polite_inserts", u"sectionheader", u"street_address", u"SYNTAX:Accepts S as Object",
		 u"trademark" };
	if (posList.find(u"Proper Noun") != posList.end())
	{
		for (lpwstring form : pnExceptionForms)
		{
			if (std::find(winnerForms.begin(), winnerForms.end(), cForms::findForm(form)) != winnerForms.end())
				return 0;
		}
	}
	// that is always noted by LP as a demonstrative determiner, but is used in REL phrases, which is equivalent to an IN usage that is matched by maxent.
	if (originalWordSave == u"that" && inRelativeClause)
		return 0;
	// these cases have been proven by examination to be either maxent mistakes or Proper Nouns used as adjectives (which is more of an implementation difference)
	if (posList.find(u"adjective") != posList.end() && std::find(winnerForms.begin(), winnerForms.end(), PROPER_NOUN_FORM_NUM) != winnerForms.end())
		return 0;
	// these cases have been proven by examination - Stanford guesses this to be a noun, but if it is capitalized, it is an honorific/honorific_abreviation
	if (posList.find(u"noun") != posList.end() && iswupper(originalWordSave[0]) && (std::find(winnerForms.begin(), winnerForms.end(), honorificForm) != winnerForms.end() || std::find(winnerForms.begin(), winnerForms.end(), honorificAbbreviationForm) != winnerForms.end()))
		return 0;
	// LP is always right about these negative forms (by examination)
	if (posList.find(u"noun") != posList.end() && (std::find(winnerForms.begin(), winnerForms.end(), doesNegationForm) != winnerForms.end() ||
		std::find(winnerForms.begin(), winnerForms.end(), negationModalAuxiliaryForm) != winnerForms.end() ||
		std::find(winnerForms.begin(), winnerForms.end(), isNegationForm) != winnerForms.end() ||
		std::find(winnerForms.begin(), winnerForms.end(), haveNegationForm) != winnerForms.end()))
		return 0;
	/////////////////////////////
	lpwstring posListStr;
	for (auto pos : posList)
		posListStr += pos + u" ";
	lpwstring winnerFormsString;
	source.m[wordSourceIndex].winnerFormString(winnerFormsString, false);
	formNoMatchMap[posListStr + u"!= " + winnerFormsString]++;
	wordNoMatchMap[source.m[wordSourceIndex].word->first]++;
	lplog(LOG_ERROR, u"%d:Stanford POS %s (%s) not found in winnerForms %s for word %s [%s].", wordSourceIndex, partofspeech.c_str(), posListStr.c_str(), winnerFormsString.c_str(), originalWordSave.c_str(), originalParse.c_str());
	return 1;
}

// If this token has usage costs (X,Y)==(costX,costY), bump comboCostFrequency
// and annotate partofspeech with the combo (plus verb tense hints).
void formMatrixTest(cSource &source, int wordSourceIndex, lpwstring X, lpwstring Y, int costX, int costY, unordered_map<lpwstring,int> &comboCostFrequency, lpwstring &partofspeech)
{
	if (source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(X)) != costX ||
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(Y)) != costY)
		return;
	lpwstring XCost, YCost;
	itos(costX, XCost);
	itos(costY, YCost);
	lpwstring tmp1,tmp2,combo = X + u"*" + itos(costX, tmp1) + u" " + Y + u"*"+ itos(costY, tmp2);
	lpwstring word = source.m[wordSourceIndex].word->first;
	if (Y == u"verb" || X == u"verb")
	{
		if (word.length() > 2 && word.substr(word.length() - 2) == u"ed" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PAST) == VERB_PAST)
		{
			combo += u" PAST";
		}
		if (word.length() > 3 && word.substr(word.length() - 3) == u"ing" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE)
		{
			combo += u" PARTICIPLE";
		}
		if ((source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_THIRD_SINGULAR) == VERB_PRESENT_THIRD_SINGULAR)
		{
			combo += u" 3rdSING";
		}
		if ((source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR)
		{
			combo += u" 1stSING";
		}
	}
	comboCostFrequency[combo]++;
	partofspeech += u"***|"+combo+u"|***";
}

// perform tests to make sure that the noun according to LP is not a verb (that ST says).
// that smells / those smell - these agree by verb
// True if wordDeterminerSourceIndex is a determiner-like winner that can
// support treating the next token as a noun (agreement-tested for demonstratives).
bool isStanfordDeterminerType(cSource &source, int wordNounVerbDisagreementSourceIndex, int wordDeterminerSourceIndex)
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
		(source.m[wordDeterminerSourceIndex].queryWinnerForm(interrogativeDeterminerForm) >= 0 && source.m[wordDeterminerSourceIndex].word->first!=u"which") || // which may be followed by a verb
		source.m[wordDeterminerSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0; // this / that / these / those AGREEMENT required
}

class FormDistribution
{
public:
	int agreeSTLP=0; // count of times word-POS agreed between ST and LP
	int disagreeSTLP = 0; // count of times word-POS disgreed between ST and LP
	int unaccountedForDisagreeSTLP = 0; // count of times word-POS disgreed between ST and LP
	map <lpwstring, int> STFormDistribution; // total count for each form match in ST
	map <lpwstring, int> LPFormDistribution; // total count for each form match in LP
	unordered_map <lpwstring, int> LPErrorFormDistribution; // count for each form error match in LP if none agree
	unordered_map <lpwstring, int> agreeFormDistribution; // total count for each form match agreed between ST and LP
	unordered_map <lpwstring, int> disagreeFormDistribution; // total count for each form match disagreed between ST and LP
	unordered_map <lpwstring, int> LPAlreadyAccountedFormDistribution; // total count for each form match already accounted for (already entered in the errorMap)
};
map <lpwstring, FormDistribution> formDistribution;

// Normalize originalWord to the token Stanford's PCFG tree would emit
// ('s / n't / cannot / gimme / ...).  Returns the " word)" search key.
lpwstring stTokenizeWord(lpwstring tokenizedWord,lpwstring &originalWord, unsigned long long flags,lpwstring parse,int &wspace)
{
	// pcfg output:
	// parse=(ROOT (PRN (: ;) (S (NP (NP (NP (QP (CC and) (CD Bunny))) (, ,) (CC and) (NP (NNP Bobtail)) (, ,)) (CC and) (NP (NNP Billy))) (VP (VBD were) (ADVP (RB always)) (VP (VBG doing) (NP (JJ *) (NN something)))))))
	// ben's
	if (originalWord.length() >= 2 && originalWord[originalWord.length() - 2] == u'\'' && towlower(originalWord[originalWord.length() - 1]) == u's')
		originalWord.erase(originalWord.length() - 2);
	// don't
	if (originalWord.length() >= 3 && towlower(originalWord[originalWord.length() - 3]) == u'n' && originalWord[originalWord.length() - 2] == u'\'' && towlower(originalWord[originalWord.length() - 1]) == u't')
		originalWord.erase(originalWord.length() - 3);
	// cannot, dunno
	if (tokenizedWord == u"cannot" || tokenizedWord == u"dunno")
		originalWord.erase(originalWord.length() - 3);
	// gimme, lemme
	if (tokenizedWord == u"gimme" || tokenizedWord == u"lemme")
		originalWord.erase(originalWord.length() - 2);
	// y'are
	if (tokenizedWord == u"y'are")
		originalWord.erase(originalWord.length() - 3);
	else if (tokenizedWord == u"y'r")
		originalWord.erase(originalWord.length() - 1);
	else if (tokenizedWord == u"ma’am")
		originalWord[2] = '\'';
	// o'brien, o'clock, b'ar o'sheen
	else if (tokenizedWord[0] == u'o' && tokenizedWord[1] == u'’')
		originalWord[1] = u'\'';
	else
	{
		//wer'n, better'n etc but not o'clock and ma'am, which are found by ST
		size_t findQuote = (originalWord.find(u'\''));
		if (findQuote != lpwstring::npos && parse.find(originalWord) == lpwstring::npos)
		{
			originalWord = originalWord.substr(0, findQuote);
		}
	}
	lpwstring lookFor;
	// all words in LP which have spaces in them are interpreted by ST separately
	wspace = originalWord.find(u' ');
	if (wspace != lpwstring::npos)
	{
		if (tokenizedWord == u"no one" || tokenizedWord == u"every one")
			lookFor = u" one)";
		else
			if (tokenizedWord == u"as if")
				lookFor = u" if)";
			else
				if (tokenizedWord == u"for ever")
					lookFor = u" ever)";
				else
					if (tokenizedWord == u"next to")
						lookFor = u" to)";
					else
						lookFor = u" " + originalWord.substr(0, wspace) + u")";
		if (lookFor.length() > 0 && (flags&cWordMatch::flagAllCaps))
			for (int len = 0; lookFor[len]; len++) lookFor[len] = towupper(lookFor[len]);
	}
	else
		lookFor = u" " + originalWord + u")";
	return lookFor;
}

// see if LP class can be corrected.
// return code:
//   -1: LP class corrected.  ST prefers something other than correct class, so LP is correct
//   -2: LP class corrected.  ST prefers correct class, so this entry should simply be removed from the output file.
//   -3: test whether to change to correct class - set to disagree, and add an arbitrary string to partofspeech to search for whatever string added as a test.
//    0: unable to determine whether class should be corrected, or the class has been corrected. Normal processing should continue.  
// Hand-written repairs of LP winners given the Stanford tag.  Returns
// -1 LP was right (keep, credit ST error), -2 ST already matches after repair
// (drop), -3 experimental, 0 continue into attributeErrors.
int ruleCorrectLPClass(lpwstring primarySTLPMatch, cSource &source, int wordSourceIndex, unordered_map<lpwstring, int> &errorMap, lpwstring &partofspeech, int startOfSentence, map<lpwstring,FormDistribution>::iterator fdi)
{
	if (wordSourceIndex + 1 >= source.m.size())
		return 0;
	int adverbFormOffset = source.m[wordSourceIndex].word->second.query(adverbForm);
	int adjectiveFormOffset = source.m[wordSourceIndex].word->second.query(adjectiveForm);
	int particleFormOffset = source.m[wordSourceIndex].word->second.query(particleForm);
	int nounPlusOneFormOffset = source.m[wordSourceIndex + 1].word->second.query(nounForm);
	int conjunctionFormOffset = source.m[wordSourceIndex].word->second.query(conjunctionForm);
	// RULE CHANGE - change an adjective to an adverb?
	if (source.m[wordSourceIndex].word->first != u"that" && // 'that' is very ambiguous
		source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) && 
		source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(u"Proper Noun") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(u"indefinite_pronoun") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(u"numeral_cardinal") < 0 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") < 0 && // only an adjective, not before a noun
		(nounPlusOneFormOffset <0 || source.m[wordSourceIndex+1].word->second.getUsageCost(nounPlusOneFormOffset)==4) &&
		(adverbFormOffset=source.m[wordSourceIndex].queryForm(u"adverb")) >= 0 && source.m[wordSourceIndex].word->second.getUsageCost(adverbFormOffset) < 2 &&
		source.m[wordSourceIndex].queryForm(u"interjection") < 0 && // interjection acts similarly to adverb
		(iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || wordSourceIndex == 0 || iswalpha(source.m[wordSourceIndex - 1].word->first[0])) && // not alone in the sentence
		(wordSourceIndex <= 0 || (source.m[wordSourceIndex - 1].queryForm(u"is") < 0 && source.m[wordSourceIndex - 1].word->first != u"be" && source.m[wordSourceIndex - 1].word->first != u"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 1 || (source.m[wordSourceIndex - 2].queryForm(u"is") < 0 && source.m[wordSourceIndex - 2].word->first != u"be" && source.m[wordSourceIndex - 2].word->first != u"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 2 || (source.m[wordSourceIndex - 3].queryForm(u"is") < 0 && source.m[wordSourceIndex - 3].word->first != u"be" && source.m[wordSourceIndex - 3].word->first != u"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 3 || (source.m[wordSourceIndex - 4].queryForm(u"is") < 0 && source.m[wordSourceIndex - 4].word->first != u"be" && source.m[wordSourceIndex - 4].word->first != u"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex < source.m.size()-1 || (source.m[wordSourceIndex + 1].queryForm(u"is") < 0 && source.m[wordSourceIndex + 1].word->first != u"be")) && // is/ishas before means it really is an adjective!
		(source.m[wordSourceIndex].queryForm(u"preposition") < 0) && (source.m[wordSourceIndex].queryForm(u"relativizer") < 0)) // || source.m[wordSourceIndex + 1].queryWinnerForm(u"numeral_cardinal") < 0)) // before one o'clock
	{
		bool isDeterminer = false;
		if (wordSourceIndex > 0)
		{
			vector<lpwstring> determinerTypes = { u"determiner",u"demonstrative_determiner",u"possessive_determiner",u"interrogative_determiner", u"quantifier", u"numeral_cardinal" };
			for (lpwstring dt : determinerTypes)
				if (isDeterminer = source.m[wordSourceIndex - 1].queryWinnerForm(dt) >= 0)
					break;
		}
		// The door that faced her stood *open*
		if (!isDeterminer && primarySTLPMatch != u"Proper Noun" && source.m[wordSourceIndex - 1].queryWinnerForm(u"verb") >= 0 && adverbFormOffset>=0 && adjectiveFormOffset>=0) // Proper Noun is already well controlled
		{
			source.m[wordSourceIndex].setWinner(adverbFormOffset);
			source.m[wordSourceIndex].unsetWinner(adjectiveFormOffset);
			if (primarySTLPMatch == u"adverb")
				return -2;
			errorMap[u"LP correct: adverb rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
			return -1;
		}
	}
	// RULE CHANGE - change an adverb to an adjective?
	if (source.m[wordSourceIndex].isOnlyWinner(adverbForm) && adjectiveFormOffset >= 0 && source.m[wordSourceIndex].word->second.getUsageCost(adverbFormOffset) - source.m[wordSourceIndex].word->second.getUsageCost(adjectiveFormOffset) >= 3 &&
		source.m[wordSourceIndex + 1].queryWinnerForm(determinerForm) >= 0 && source.m[wordSourceIndex - 1].queryWinnerForm(verbForm) < 0)
	{
		source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
		source.m[wordSourceIndex].unsetWinner(adverbFormOffset);
		if (primarySTLPMatch == u"adjective")
			return -2;
		errorMap[u"LP correct: adjective-adverb rule"]++;
		fdi->second.LPAlreadyAccountedFormDistribution[u"adjective"]++;
		return -1;
	}
	// cannot be preposition, conjunction, verb, determiner, particle
	if (source.m[wordSourceIndex].isOnlyWinner(adverbForm) && adjectiveFormOffset >= 0 && 
		(source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"dayUnit") >= 0) &&
		source.m[wordSourceIndex + 1].queryWinnerForm(adjectiveForm) < 0 &&
		source.m[wordSourceIndex - 1].queryWinnerForm(verbForm) < 0 &&
		source.m[wordSourceIndex].queryForm(prepositionForm)<0 &&
		source.m[wordSourceIndex].queryForm(conjunctionForm) < 0 &&
		source.m[wordSourceIndex].queryForm(verbForm) < 0 &&
		source.m[wordSourceIndex].queryForm(determinerForm) < 0 &&
		source.m[wordSourceIndex].queryForm(particleForm) < 0 && 
		source.m[wordSourceIndex + 2].word->first!=u"-" && // There is no getting in or out of them without the greatest difficulty , and a patient , slow navigation , which is *very* heart - rending .
		source.queryPattern(wordSourceIndex,u"_TIME")==-1) // An hour *later* supper was served . 
	{
		// LP correct - 1039
		// ST correct - 19 < 2%
		source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
		source.m[wordSourceIndex].unsetWinner(adverbFormOffset);
		if (primarySTLPMatch == u"adjective")
			return -2;
		errorMap[u"LP correct: adjective-adverb rule 2"]++;
		fdi->second.LPAlreadyAccountedFormDistribution[u"adjective"]++;
		return -1;
	}
	if (wordSourceIndex < source.m.size() - 3 && source.m[wordSourceIndex].word->first == u"most" &&
		(source.m[wordSourceIndex + 1].hasWinnerNounForm() ||
		(source.m[wordSourceIndex + 1].word->first == u"of" && 
			(source.m[wordSourceIndex + 2].word->first == u"the" || source.m[wordSourceIndex + 2].queryWinnerForm(demonstrativeDeterminerForm) != -1 || source.m[wordSourceIndex + 2].queryWinnerForm(possessiveDeterminerForm) != -1 || source.m[wordSourceIndex + 2].queryWinnerForm(interrogativeDeterminerForm) != -1))
			))
	{
		source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
		source.m[wordSourceIndex].unsetAllFormWinners();
		if (primarySTLPMatch == u"adjective")
			return -2;
		errorMap[u"LP correct: adjective-adverb 'most' rule"]++;
		fdi->second.LPAlreadyAccountedFormDistribution[u"adjective"]++;
		return -1;
	}
	if (source.m[wordSourceIndex].word->first == u"only")
	{
		if (source.m[wordSourceIndex + 1].pma.queryPattern(u"__S1") != -1)
		{
			if (wordSourceIndex == startOfSentence || wordSourceIndex == startOfSentence+1)
			{
				source.m[wordSourceIndex].setWinner(adverbFormOffset);
				source.m[wordSourceIndex].unsetAllFormWinners();
				if (primarySTLPMatch == u"adverb")
					return -2;
				errorMap[u"LP correct: adverb 'only' rule"]++;
				fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
				return -1;
			}
			else
			{
				source.m[wordSourceIndex].setWinner(conjunctionFormOffset);
				source.m[wordSourceIndex].unsetAllFormWinners();
				if (primarySTLPMatch == u"conjunction" || primarySTLPMatch == u"preposition or conjunction")
					return -2;
				errorMap[u"LP correct: conjunction 'only' rule"]++;
				fdi->second.LPAlreadyAccountedFormDistribution[u"conjunction"]++;
				return -1;

			}
		}
		else if (source.m[wordSourceIndex + 1].pma.queryPattern(u"__INFP") != -1)
		{
			source.m[wordSourceIndex].setWinner(adverbFormOffset);
			source.m[wordSourceIndex].unsetAllFormWinners();
			if (primarySTLPMatch == u"adverb")
				return -2;
			errorMap[u"LP correct: adverb 'only' rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
			return -1;
		}
		else if (source.m[wordSourceIndex + 1].queryWinnerForm(determinerForm) != -1)
		{
			source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
			source.m[wordSourceIndex].unsetAllFormWinners();
			if (primarySTLPMatch == u"adjective")
				return -2;
			errorMap[u"LP correct: adjective 'only' rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"adjective"]++;
			return -1;
		}
		return 0;
	}
	if (source.m[wordSourceIndex].word->first == u"better" || source.m[wordSourceIndex].word->first == u"further")
	{
		bool sentenceOfBeing =				// 4 words or less before the word must be an 'is' verb
			((wordSourceIndex <= 0 || (source.m[wordSourceIndex - 1].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 1].queryForm(u"be") >= 0)) || // is/ishas before means it really is an adjective!
			(wordSourceIndex <= 1 || (source.m[wordSourceIndex - 2].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 2].queryForm(u"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 2 || (source.m[wordSourceIndex - 3].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 3].queryForm(u"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 3 || (source.m[wordSourceIndex - 4].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 4].queryForm(u"be") >= 0))); // is/ishas before means it really is an adjective!
		if (source.m[wordSourceIndex].word->first == u"better" && sentenceOfBeing && source.m[wordSourceIndex + 1].queryWinnerForm(verbForm) == -1)
		{
			source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
			source.m[wordSourceIndex].unsetAllFormWinners();
			if (primarySTLPMatch == u"adjective")
				return -2;
			errorMap[u"LP correct: adjective 'only' rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"adjective"]++;
			return -1;
		}
		else if (source.m[wordSourceIndex].word->first == u"better" && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) == -1 && source.m[wordSourceIndex + 1].queryWinnerForm(determinerForm) == -1)
		{
			source.m[wordSourceIndex].setWinner(adverbFormOffset);
			source.m[wordSourceIndex].unsetAllFormWinners();
			if (primarySTLPMatch == u"adverb")
				return -2;
			errorMap[u"LP correct: adverb 'better' rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
			return -1;
		}
	}
	if (source.m[wordSourceIndex].isOnlyWinner(prepositionForm) && source.m[wordSourceIndex].getRelObject() < 0 && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))
	{
		int relVerb = source.m[wordSourceIndex].getRelVerb();
		bool sentenceOfBeing =				// 4 words or less before the word must be an 'is' verb
			((wordSourceIndex <= 0 || (source.m[wordSourceIndex - 1].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 1].queryForm(u"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 1 || (source.m[wordSourceIndex - 2].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 2].queryForm(u"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 2 || (source.m[wordSourceIndex - 3].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 3].queryForm(u"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 3 || (source.m[wordSourceIndex - 4].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 4].queryForm(u"be") >= 0))); // is/ishas before means it really is an adjective!
		if (adverbFormOffset < 0)
		{
			if (!(cWord::isSingleQuote(source.m[wordSourceIndex + 1].word->first[0]) || cWord::isDoubleQuote(source.m[wordSourceIndex + 1].word->first[0])) && primarySTLPMatch == u"to")
			{
				errorMap[u"LP correct: 'to' preposition rule"]++;
				fdi->second.LPAlreadyAccountedFormDistribution[u"preposition"]++;
				return -1;
			}
			if (source.m[wordSourceIndex].word->first == u"like" && sentenceOfBeing &&
					// the word before must NOT be a dash
					(wordSourceIndex <= 0 || !cWord::isDash((source.m[wordSourceIndex - 1].word->first[0]))))
			{
				source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
				source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
				if (primarySTLPMatch == u"adjective")
					return -2;
				errorMap[u"LP correct: adjective-like rule"]++;
				fdi->second.LPAlreadyAccountedFormDistribution[u"adjective"]++;
				return -1;
			}
			else
				return 0; // In other cases the STLPMatch is already a preposition, so ST and LP agree anyway
		}
		else if (primarySTLPMatch == u"adverb")
		{
			source.m[wordSourceIndex].setWinner(adverbFormOffset);
			source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
			if (primarySTLPMatch == u"adverb")
				return -2;
			errorMap[u"LP correct: adverb rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
			return -1;
		}
		else
		{
			lpwstring nextWord = source.m[wordSourceIndex + 1].word->first;
			if (nextWord == u"." || nextWord == u"," || nextWord == u";" || nextWord == u"--")
			{
				if (primarySTLPMatch == u"particle" && particleFormOffset>=0)
				{
					source.m[wordSourceIndex].setWinner(particleFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					return -2;
				}
				if (relVerb>=0 && (source.m[relVerb].queryForm(u"is")>=0 || source.m[relVerb].queryForm(u"be") >= 0))
				{
					if (adjectiveFormOffset < 0)
					{
						if (particleFormOffset > 0 && !cWord::isDash((source.m[wordSourceIndex + 1].word->first[0])))
						{
							source.m[wordSourceIndex].setWinner(particleFormOffset);
							source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
							if (primarySTLPMatch == u"particle")
								return -2;
							errorMap[u"LP correct: particle rule"]++;
							fdi->second.LPAlreadyAccountedFormDistribution[u"particle"]++;
							return -1;
						}
						if (adverbFormOffset >= 0)
						{
							source.m[wordSourceIndex].setWinner(adverbFormOffset);
							source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
							errorMap[u"LP correct: adverb rule"]++;
							fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
							return -1;
						}
						return 0;
					}
					//if (relVerb >= 0)
					//	partofspeech += source.m[relVerb].word->first;
					source.m[wordSourceIndex].setWinner(adjectiveFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					if (primarySTLPMatch == u"adjective")
						return -2;
					errorMap[u"LP correct: adjective-prep rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[u"adjective"]++;
					return -1;
				}
				else if (primarySTLPMatch==u"preposition or conjunction")
				{
					source.m[wordSourceIndex].setWinner(adverbFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					errorMap[u"LP correct: adverb rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
					return -1;
				}
				else
				{
					source.m[wordSourceIndex].setWinner(adverbFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					errorMap[u"LP correct: adverb rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[u"adverb"]++;
					return -1;
				}
			}
			else
			{
				if (particleFormOffset > 0 && !cWord::isDash((source.m[wordSourceIndex + 1].word->first[0])))
				{
					source.m[wordSourceIndex].setWinner(particleFormOffset);
					source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
					if (primarySTLPMatch == u"particle")
						return -2;
					errorMap[u"LP correct: particle rule"]++;
					fdi->second.LPAlreadyAccountedFormDistribution[u"particle"]++;
					return -1;
				}
				return 0;
			}
		}
		return 0;
	}
	if (source.m[wordSourceIndex].word->first == u"that" && (((source.m[wordSourceIndex].flags&cWordMatch::flagInQuestion) && wordSourceIndex > 0 && 
		(source.m[wordSourceIndex - 1].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 1].queryForm(u"is_negation") >= 0) &&
		source.m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm)>=0 && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) < 0) ||
		(source.m[wordSourceIndex].word->first == u"that" && !(source.m[wordSourceIndex].flags&cWordMatch::flagInQuestion) && wordSourceIndex > 0 && (source.m[wordSourceIndex + 1].queryForm(u"is") >= 0 || source.m[wordSourceIndex + 1].queryForm(u"is_negation") >= 0) &&
		(!iswalpha(source.m[wordSourceIndex - 1].word->first[0]) || wordSourceIndex == startOfSentence)) ||
		(source.m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0 && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))))
	{
		source.m[wordSourceIndex].setWinner(source.m[wordSourceIndex].queryForm(pronounForm));
		source.m[wordSourceIndex].unsetWinner(source.m[wordSourceIndex].queryForm(demonstrativeDeterminerForm));
	}
	// a word which LP thinks is an adverb, which is before a determiner, which ST thinks is a predeterminer and which has a predeterminer form
	if (source.m[wordSourceIndex].isOnlyWinner(adverbForm) && source.m[wordSourceIndex + 1].isOnlyWinner(determinerForm) && primarySTLPMatch == u"predeterminer" && source.m[wordSourceIndex].queryForm(predeterminerForm)!=-1)
	{
		source.m[wordSourceIndex].setWinner(source.m[wordSourceIndex].queryForm(predeterminerForm));
		source.m[wordSourceIndex].unsetWinner(adverbFormOffset);
		return -2;
	}
	int primaryPMAOffset = source.m[wordSourceIndex].pma.queryPattern(u"__ALLOBJECTS_1");
	int secondaryPMAOffset = source.m[wordSourceIndex].pma.queryPattern(u"_ADVERB");
	if (primaryPMAOffset !=-1 && secondaryPMAOffset !=-1)
	{
		primaryPMAOffset = primaryPMAOffset & ~cMatchElement::patternFlag;
		secondaryPMAOffset = secondaryPMAOffset & ~cMatchElement::patternFlag;
		set <lpwstring> particles = { u"down",u"out",u"off",u"up" };
		if (particles.find(source.m[wordSourceIndex].word->first) == particles.end() && source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(prepositionForm))<4 && source.m[wordSourceIndex].pma[secondaryPMAOffset].len == 1 && source.queryPattern(wordSourceIndex + 1, u"__NOUN") != -1 && source.m[wordSourceIndex].queryForm(prepositionForm) != -1)
		{
			source.m[wordSourceIndex].setWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
			source.m[wordSourceIndex].unsetWinner(adverbFormOffset);
			return 0;
		}
	}
	set <lpwstring> notObjects = { u"we",u"i",u"he",u"they" };
	if (wordSourceIndex < source.m.size() - 2 && source.m[wordSourceIndex + 1].hasWinnerNounForm() && source.m[wordSourceIndex].isOnlyWinner(adverbForm) &&
		source.m[wordSourceIndex].queryForm(prepositionForm) != -1 && source.m[wordSourceIndex].word->first != u"as" && source.m[wordSourceIndex + 1].queryWinnerForm(PROPER_NOUN_FORM) == -1)
	{
		if (notObjects.find(source.m[wordSourceIndex + 1].word->first) == notObjects.end() &&
			(!iswalpha(source.m[wordSourceIndex + 2].word->first[0]) || source.m[wordSourceIndex + 2].queryWinnerForm(coordinatorForm) != -1 || source.m[wordSourceIndex + 2].queryWinnerForm(determinerForm) != -1))
		{
			source.m[wordSourceIndex].setWinner(source.m[wordSourceIndex].queryForm(prepositionForm));
			source.m[wordSourceIndex].unsetWinner(adverbFormOffset);
			errorMap[u"LP correct: preposition/conjunction NOT adverb rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"preposition"]++;
			return -1;
		}
		int conjunctionFormOffset;
		if (notObjects.find(source.m[wordSourceIndex + 1].word->first) != notObjects.end() && (conjunctionFormOffset = source.m[wordSourceIndex].queryForm(conjunctionForm))!=-1)
		{
			source.m[wordSourceIndex].setWinner(conjunctionFormOffset);
			source.m[wordSourceIndex].unsetWinner(adverbFormOffset);
			errorMap[u"LP correct: preposition/conjunction NOT adverb rule"]++;
			fdi->second.LPAlreadyAccountedFormDistribution[u"conjunction"]++;
			return -1;
		}
	}
	int nounPMAIndex = -1;
	if (adverbFormOffset>=0 && wordSourceIndex > 0 && (nounPMAIndex = source.m[wordSourceIndex - 1].pma.queryPatternDiff(u"__NOUN", u"2")) != -1 && source.m[wordSourceIndex - 1].word->first == u"the" && source.m[wordSourceIndex - 1].pma[nounPMAIndex & ~cMatchElement::patternFlag].len == 3 &&
		source.m[wordSourceIndex + 1].pma.queryPattern(u"_ADJECTIVE_AFTER") != -1)
	{
		source.m[wordSourceIndex].unsetAllFormWinners();
		source.m[wordSourceIndex].setWinner(adverbFormOffset);
		return 0;
	}
	return 0;
}

// Classify an ST/LP disagreement into errorMap buckets (implementation diffs,
// speaking-verb quotes, cost-matrix noun/verb, ...).  Giant heuristic table;
// return 0 means "accounted for".
int attributeErrors(lpwstring primarySTLPMatch, cSource &source, int wordSourceIndex, unordered_map<lpwstring, int> &errorMap, unordered_map<lpwstring, int> &comboCostFrequency, lpwstring &partofspeech, int startOfSentence)
{
	lpwstring word = source.m[wordSourceIndex].word->first;
	//////////////////////////////
	// corrections based on implementation/interpretation differences and statistical findings
	// 1. LP has a NOUN[2] which allows a noun in what should be an adjective position
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && wordSourceIndex < source.m.size() - 1 && source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0)
	{
		errorMap[u"diff: noun in adjective position"]++;
		return 0;
	}
	// 2. where 'that' is a demonstrative_determiner, and matches a REL1 pattern, then we count that as correct parse for LP as this is a relative phrase, and the usage of 'that' is correctly understood within that pattern.
	if (primarySTLPMatch == u"preposition or conjunction" && wordSourceIndex < source.m.size() - 1 && source.m[wordSourceIndex].queryWinnerForm(u"demonstrative_determiner")>=0)
	{
		int maxEnd, pemaPosition = source.queryPattern(wordSourceIndex, u"_REL1", maxEnd);
		if (pemaPosition >= 0 && (//(source.pema[pemaPosition].begin == 0 && patterns[source.pema[pemaPosition].getPattern()]->differentiator == u"2") || // REL1[2] includes S1 
			(source.pema[pemaPosition].begin <= 0 && source.pema[pemaPosition].begin >= -5) || // must be the start of a relative clause
			source.scanForPatternElementTag(wordSourceIndex, SENTENCE_IN_REL_TAG) != -1))
		{
			errorMap[u"LP correct:" + word + u" start of relative phrase"]++;
			return 0;
		}
	}
	static set <lpwstring> speakingVerbs = { 
					u"added",u"agreed",u"announced",u"answered",u"approved",u"asked",u"aspirated",u"assented",u"barked",u"bawled",u"beamed",u"begged",
					u"bellowed",u"beseeched",u"blazed",u"blurted",u"brayed",u"burst",u"called",u"chaffed",u"chimed",u"chorused",u"chuckled",u"commanded",
					u"commended",u"commented",u"continued",u"cried",u"croaked",u"declaimed",u"demanded",u"dimpled",u"drawled",u"droned",u"ejaculated",u"enquired",
					u"exclaimed",u"expostulated",u"exulted",u"fell",u"flung",u"fumed",u"gasped",u"gazing",u"gibed",u"grinned",u"groaned",u"growled",
					u"grumbled",u"grunted",u"guffawed",u"hooted",u"howled",u"implored",u"inquired",u"interjected",u"interposed",u"interrupted",u"jeered",u"joked",
					u"jubilated",u"laughed",u"leered",u"moaned",u"mourned",u"murmured",u"mused",u"muttered",u"observed",u"panted",u"persisted",u"promised",
					u"proposed",u"propounded",u"protested",u"puffed",u"pursued",u"put",u"questioned",u"quizzed",u"raved",u"reminded",u"remonstrated",u"repeated",u"replied",
					u"responded",u"retaliated",u"retorted",u"roared",u"said",u"sang",u"scoffed",u"scorned",u"scowled",u"screamed",u"seconded",u"secure",
					u"shot",u"shouted",u"shrieked",u"shrilled",u"smirked",u"snapped",u"snarled",u"sneered",u"snorted",u"sobbed",u"soliloquized",u"spluttered",
					u"stammered",u"stuttered",u"suggested",u"surmised",u"sympathized",u"thought",u"turning",u"twitted",u"used",u"ventured",u"wailed",u"wheezed",
					u"whined",u"whispered",u"whistled",u"yawned",u"yawped",u"yelled"
	};
	// Stanford POS NN (noun) not found in winnerForms determiner for word the 0002542:[” asked *the* mother . ]
	if (wordSourceIndex > 1 && primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"determiner") >= 0 && source.m[wordSourceIndex - 2].queryForm(quoteForm) >= 0 &&
			find(speakingVerbs.begin(), speakingVerbs.end(), source.m[wordSourceIndex - 1].word->first) != speakingVerbs.end())
	{
		errorMap[u"LP correct: 'the' is not a noun"]++;
		return 0;
	}
	// 3. ST is always wrong when given a phrase like [(ROOT (S ('' '') (S (S (VP (VBD said))) (VP (VBZ Bobtail)))] - LP correctly tags 'Bobtail' as a proper noun
	if (wordSourceIndex>=2 && primarySTLPMatch == u"verb" && source.m[wordSourceIndex].queryWinnerForm(u"Proper Noun") >= 0 && source.m[wordSourceIndex - 2].queryForm(quoteForm) >= 0 &&
			speakingVerbs.find(source.m[wordSourceIndex - 1].word->first) != speakingVerbs.end())
	{
		errorMap[u"LP correct: speaker is proper noun (not verb)"]++;
		return 0;
	}
	// 3b. ST is always wrong when given a phrase like " she *exclaimed* - LP correctly tags 'exclaimed' as a verb
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && wordSourceIndex > 2 && source.m[wordSourceIndex - 2].queryForm(quoteForm) >= 0 &&
		(source.m[wordSourceIndex-1].queryWinnerForm(u"personal_pronoun") >= 0 || source.m[wordSourceIndex-1].queryWinnerForm(u"Proper Noun") >= 0) &&
		speakingVerbs.find(source.m[wordSourceIndex].word->first) != speakingVerbs.end())
	{
		errorMap[u"LP correct: verb of speaking is a verb"]++;
		return 0;
	}
	// 3c. ST is always wrong when given a phrase like " added his father - LP correctly tags 'added' as a verb
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && source.m[wordSourceIndex - 1].queryForm(quoteForm) >= 0 &&
		wordSourceIndex > 1 && speakingVerbs.find(source.m[wordSourceIndex].word->first) != speakingVerbs.end())
	{
		errorMap[u"LP correct: verb of speaking is a verb"]++;
		return 0;
	}
	// 4. ST is always wrong when given a phrase like [(ROOT (S ('' '') (S (S (VP (VBD said))) (VP (VBZ Bobtail)))] - LP correctly tags 'said' as a verb
	// Stanford POS JJ(adjective) not found in winnerForms verb for word ejaculated 0002658:[” *ejaculated* Mrs.Ross .]
	if ((primarySTLPMatch == u"adjective" || primarySTLPMatch == u"Proper Noun" || primarySTLPMatch == u"noun") &&
		(source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"SYNTAX:Accepts S as Object") >= 0) && wordSourceIndex > 1 &&
		source.m[wordSourceIndex - 1].queryForm(quoteForm) >= 0 &&
		find(speakingVerbs.begin(), speakingVerbs.end(), word) != speakingVerbs.end())
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(u"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].word->first == u"the" || source.m[wordSourceIndex + 1].word->first == u"a" || source.m[wordSourceIndex + 1].word->first == u"one" ||
			source.m[wordSourceIndex + 1].queryWinnerForm(u"honorific") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"honorific_abbreviation") >= 0)
		{
			errorMap[u"LP correct: speaking verb is not adjective, noun or Proper Noun"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].queryWinnerForm(u"preposition") >= 0 && (source.m[wordSourceIndex + 2].queryWinnerForm(u"Proper Noun") >= 0 || source.m[wordSourceIndex + 2].queryWinnerForm(u"honorific") >= 0 || source.m[wordSourceIndex + 2].queryWinnerForm(u"honorific_abbreviation") >= 0))
		{
			errorMap[u"LP correct: speaking verb is not adjective, noun or Proper Noun"]++;
			return 0;
		}
	}
	// 5. if ST thinks it is a verb, and LP thinks it is a noun, and it is preceded by a determiner separated only by up to 2 adjectives (that are not 'no'), unless it is a VBG and then it has to be immediately preceeded by a determiner
	//    examined 100 examples from gutenburg and 1 violated this rule.
	if ((primarySTLPMatch == u"verb") && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && wordSourceIndex > 2)
	{
		if (isStanfordDeterminerType(source, wordSourceIndex, wordSourceIndex - 1) || (partofspeech != u"VBG" &&
			((isStanfordDeterminerType(source, wordSourceIndex, wordSourceIndex - 2) && source.m[wordSourceIndex - 2].word->first != u"no" && (source.m[wordSourceIndex - 1].queryWinnerForm(adjectiveForm) >= 0)) ||
			(isStanfordDeterminerType(source, wordSourceIndex, wordSourceIndex - 3) && source.m[wordSourceIndex - 3].word->first != u"no" && source.m[wordSourceIndex - 2].queryWinnerForm(adjectiveForm) >= 0 && source.m[wordSourceIndex - 1].queryWinnerForm(adjectiveForm) >= 0))))
		{
			// further more, the 
			errorMap[u"LP correct: ST says verb when it is a noun (preceded by determiner)"]++;
			return 0;
		}
		// 6. if ST thinks it is a verb, and LP thinks it is a noun, and LP does not know of it having a verb form
		//    examined 100 examples from gutenburg and 0 violated this rule.
		else if (!source.m[wordSourceIndex].word->second.hasVerbForm())
		{
			errorMap[u"LP correct: ST says verb when it is a noun (no verb form possible)"]++;
			return 0;
		}
	}
	// 7. if ST thinks it is a noun, and LP does not know of it having a noun form
	//    examined 100 examples from gutenburg and X violated this rule.
	if (primarySTLPMatch == u"noun" && !source.m[wordSourceIndex].word->second.hasNounForm())
	{
		// However, if it is a present participle verb
		//    then this is only acceptable if LP has matched it to __N1.
		if (source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE)
		{
			if (source.m[wordSourceIndex].pma.queryPattern(u"__N1") != -1)
			{
				errorMap[u"diff: ST says noun when it is a present participle [acceptable]"]++;
				return 0; // ST and LP agree
			}
		}
		else
		{
			errorMap[u"LP correct: ST says noun when no noun form possible"]++;
			return 0; // ST and LP disagree and ST is wrong
		}
	}
	// 8. if ST thinks it is an adjective, and LP maps it to an __ADJECTIVE pattern
	//    examined 100 examples from gutenburg and 0 violated this rule.
	if ((primarySTLPMatch == u"adjective") && source.m[wordSourceIndex].queryForm(u"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PAST) == VERB_PAST && 
		  (source.m[wordSourceIndex].pma.queryPattern(u"__ADJECTIVE") != -1 || source.m[wordSourceIndex].pma.queryPattern(u"_ADJECTIVE_AFTER") != -1))
	{
		errorMap[u"diff: ST says adjective, LP says verb PAST matched to an _ADJECTIVE pattern"]++;
		return 0;
	}
	// 9. In LP rules, here is not an adverb.  It designates a place and therefore is a noun.
	//    examined 10 examples from gutenburg and 0 violated this rule.
	if ((primarySTLPMatch == u"adverb") && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && word == u"here")
	{
		errorMap[u"diff: ST says here is an adverb, LP says prefers to say a noun"]++;
		return 0;
	}
	// 10. out of 849 examples, 815 were marked LP correct,  10 for ST and 24 were neither.
	// this is because LP is able to use statistics regarding Proper Nouns which are not used in ST, and also all caps words are particularly marked as being Proper Nouns when they are hardly ever proper nouns.
	if ((primarySTLPMatch == u"Proper Noun") || source.m[wordSourceIndex].queryWinnerForm(u"Proper Noun") >= 0)
	{
		errorMap[u"LP correct: ST says Proper Noun when it is not or does not say it is a proper noun when it is"]++;
		return 0;
	}
	// 11. numeral_cardinal counts as an adjective for LP if it matches the pattern __ADJECTIVE or _TIME (with the first match being an adjective of how much time)
	if ((primarySTLPMatch == u"adjective") && source.m[wordSourceIndex].queryWinnerForm(u"numeral_cardinal") >= 0 &&
		(source.m[wordSourceIndex].pma.queryPattern(u"__ADJECTIVE") != -1 || source.m[wordSourceIndex].pma.queryPattern(u"_TIME") != -1))
	{
		errorMap[u"diff: ST says here is an adjective, LP says numeral_cardinal if matching _ADJECTIVE or _TIME"]++;
		return 0;
	}
	// 12. "one" is post processed and understood as a pronoun by LP, even if it is only matched as a numeral_cardinal. (10 examples examined)
	//   this is often encountered if the author is fond of speaking in the general person ('one')
	if (word == u"one" && (primarySTLPMatch == u"personal_pronoun_accusative") && source.m[wordSourceIndex].queryWinnerForm(u"numeral_cardinal") >= 0 &&
		(source.m[wordSourceIndex].pma.queryPattern(u"__ADJECTIVE") != -1 || source.m[wordSourceIndex].pma.queryPattern(u"__NOUN") != -1))
	{
		errorMap[u"LP correct: ST says noun when no noun form possible"]++;
		return 0;
	}
	// 13. numeral_ordinal counts as an noun for LP if it matches the pattern __NOUN
	if ((primarySTLPMatch == u"noun") && source.m[wordSourceIndex].queryWinnerForm(u"numeral_ordinal") >= 0 && source.m[wordSourceIndex].pma.queryPattern(u"__N1") != -1)
	{
		errorMap[u"diff: ST says noun, LP says numeral_ordinal matched to _N1 (noun subpattern)"]++;
		return 0;
	}
	// 14. LP does not have 'any' as a determiner but rather as a pronoun/adjective which is used internally as an indicator as to how to match the noun to other nouns.
	// out of 100 examples, 100% were interpreted correctly - added 'any' as a determiner when calculating noun/determiner agreement cost
	if (word == u"any" && primarySTLPMatch == u"determiner" &&
		(source.m[wordSourceIndex].pma.queryPattern(u"__ADJECTIVE") != -1 && source.m[wordSourceIndex].pma.queryPattern(u"__NOUN") != -1))
	{
		errorMap[u"diff: ST says determiner, LP says adjective which is used as a determiner in post-processing"]++;
		return 0;
	}
	// 15. in the event of __AS_AS pattern, which is as (adverb) followed by an adjective or adverb followed by as (preposition)
	// either adverb or preposition may be acceptable as both are incorporated into the pattern.  __AS_AS pattern was derived from Longman
	if (word == u"as" && (source.m[wordSourceIndex].pma.queryPattern(u"__AS_AS") != -1 || source.queryPatternDiff(wordSourceIndex,u"__PP",u"D")!=-1))
	{
		errorMap[u"diff: word 'as': ST says " + primarySTLPMatch + u", LP says __AS_AS (Longman adverbial clause) "]++;
		return 0;
	}
	if (source.m[wordSourceIndex].queryWinnerForm(u"does") >= 0)
	{
		errorMap[u"LP correct: word 'does': ST says " + primarySTLPMatch + u" LP says helper verb (does)"]++;
		return 0;
	}
	// 16. So as matched in the beginning of a phrase is a linking adverbial (Longman) but is usually marked as a preposition by Stanford.
	bool atStart = wordSourceIndex == startOfSentence || (wordSourceIndex == startOfSentence + 1 && cWord::isDoubleQuote(source.m[wordSourceIndex - 1].word->first[0]));
	if ((word == u"so" || word==u"either") && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 &&
		(primarySTLPMatch == u"preposition or conjunction" || primarySTLPMatch == u"conjunction") && atStart)
	{
		errorMap[u"LP correct: word 'so,either': ST says " + primarySTLPMatch + u" LP says adverb (Longman linking adverbial)"]++;
		return 0;
	}
	if ((word == u"up"  || word == u"off") && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 &&
		(primarySTLPMatch == u"preposition or conjunction" || primarySTLPMatch == u"conjunction") && atStart && source.m[wordSourceIndex+1].queryWinnerForm(u"preposition")>=0)
	{
		errorMap[u"LP correct: word 'up': ST says " + primarySTLPMatch + u" LP says adverb (Longman linking adverbial)"]++;
		return 0;
	}
	if (word == u"before" && primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && atStart)
	{
		errorMap[u"LP correct: word 'before' at the start of a sentence"]++;
		return 0;
	}
	bool tempstar = false;
	// 17. this is determined to be an adverb by LP, but this is wrong, it is actually a determiner for the preceding word
	//     over 100 items have been examined and 0 errors. - note this is also AFTER the POS processing has narrowed it down to disagreements with Stanford POS
	// case 1 is all after a plural noun.
	// case 2 is all before a determiner or 'right'. (39 out of 178 cases)
	//   
	int wallp = -1, qallp = (wordSourceIndex + 1 < source.m.size()) ? source.m[wordSourceIndex + 1].pma.queryPattern(u"__NOUN") : -1;
	if (qallp >= 0)
		wallp = wordSourceIndex + 1 + source.m[wordSourceIndex + 1].pma[qallp].len - 1;
	if (word == u"all" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && (primarySTLPMatch == u"determiner" || primarySTLPMatch == u"predeterminer") &&
		(source.m[wordSourceIndex - 1].queryWinnerForm(u"demonstrative_determiner") >= 0 || // case 1
		(source.m[wordSourceIndex + 1].queryForm(u"demonstrative_determiner") >= 0) || //  case 2- all these / all those / all this / all that
			(source.m[wordSourceIndex + 1].queryForm(u"possessive_determiner") >= 0) || // case 2- all my / all his / all her
			(source.m[wordSourceIndex + 1].queryForm(u"determiner") >= 0) || // case 2- all the ...
			(source.m[wordSourceIndex + 1].word->first == u"right") || // case 2- all right
			source.m[wordSourceIndex - 1].queryWinnerForm(u"personal_pronoun") >= 0 || // case 1
			source.m[wordSourceIndex - 1].word->first == u"them" || // case 1
			source.m[wordSourceIndex - 1].word->first == u"they" || // case 1
			source.m[wordSourceIndex - 1].word->first == u"we" || // case 1
			source.m[wordSourceIndex - 1].word->first == u"us" || // case 1
			((source.m[wordSourceIndex - 1].pma.queryPattern(u"__N1") != -1 || source.m[wordSourceIndex - 1].pma.queryPattern(u"__NOUN") != -1) && // case 1
			(source.m[wordSourceIndex - 1].word->second.inflectionFlags&PLURAL) == PLURAL) ||
				(tempstar = wallp >= 0 && (source.m[wallp].word->second.inflectionFlags&PLURAL) == PLURAL)
			)
		)
	{
		errorMap[u"ST correct: word 'all': [after plural noun or before a determiner or 'right'] ST says " + primarySTLPMatch + u" LP says adverb"]++;
		return 0;
	}
	else if (word == u"all" && source.m[wordSourceIndex].queryWinnerForm(u"predeterminer") >= 0 && primarySTLPMatch == u"adverb" && 
		       (source.m[wordSourceIndex + 1].queryWinnerForm(u"determiner") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"possessive_determiner") >= 0))
	{
		errorMap[u"ST correct: word 'all': [after determiner/possessive determiner] ST says " + primarySTLPMatch + u" LP says adverb"]++;
		return 0;
	}
	// 18. This 'All' should be classified as a subject (2 matches)
	else if (word == u"all" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 &&
		(source.m[wordSourceIndex + 1].word->first == u"are" || source.m[wordSourceIndex + 1].word->first == u"was"))
	{
		errorMap[u"ST correct: word 'all': [before 'are' or 'was'] ST says " + primarySTLPMatch + u" LP says adverb"]++;
		return 0;
	}
	// 19. this should be an adjective (all 12 examples checked)
	else if (word == u"all" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 &&
		(source.m[wordSourceIndex - 1].queryForm(u"is") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(u"modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(u"future_modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(u"negation_modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(u"negation_future_modal_auxiliary") >= 0 ||
			source.m[wordSourceIndex - 1].queryForm(u"is_negation") >= 0))
	{
		errorMap[u"ST correct: word 'all': [after 'is' or modal_auxiliary] ST says " + primarySTLPMatch + u" LP says adverb"]++;
		return 0;
	}
	// 19b. All immediately before verb or preposition is definitely an adverb (checked with 100 examples)
	else if (word == u"all" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 &&
		(source.m[wordSourceIndex + 1].queryWinnerForm(u"preposition") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(verbForm) >= 0))
	{
		errorMap[u"LP correct: word 'all': [before a verb or a preposition] ST says " + primarySTLPMatch + u" LP says adverb"]++;
		return 0;
	}
	// 20. each other seems to always be interpreted as a DET followed by an adjective.  For LP's purposes, this is a reciprocal pronoun!
	if (word == u"each other")
	{
		errorMap[u"LP correct: word 'each other': ST says DET adjective LP says reciprocal pronoun"]++;
		return 0;
	}
	// 21. one another is always be interpreted by ST as a CD (numeral_cardinal) followed by a DT (determiner!).  For LP's purposes, this is a reciprocal pronoun!
	if (word == u"one another")
	{
		errorMap[u"LP correct: word 'one another': ST says DET CD, LP says reciprocal pronoun"]++;
		return 0;
	}
	// 22. no one is always be interpreted by ST as a RB (adverb) followed by a CD (numeral_cardinal).  For LP's purposes, this is an indefinite pronoun!
	if (word == u"no one")
	{
		errorMap[u"LP correct: word 'no one': ST says RB CD, LP says indefinite pronoun"]++;
		return 0;
	}
	// 23. every one can be interpreted by ST as a DT (determiner) followed by a CD (numeral_cardinal) .  For LP's purposes, this is an indefinite pronoun!
	if (word == u"every one")
	{
		errorMap[u"LP correct: word 'every one': ST says DT CD, LP says indefinite pronoun"]++;
		return 0;
	}
	// 24. Stanford sometimes guesses that as an adverb as well (all 3 examples checked)
	if (word == u"that" && source.scanForPatternTag(wordSourceIndex, SENTENCE_IN_REL_TAG) != -1)
	{
		errorMap[u"LP correct: word 'that': ST says adverb, LP says relativizer [beginning of SENTENCE_IN_REL_TAG]"]++;
		return 0;
	}
	// 25. 'So' before _S1 is a linking adverbial, not a preposition (Longman - 891)
	if (word == u"so" && (primarySTLPMatch == u"preposition or conjunction" || primarySTLPMatch == u"conjunction") && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 &&
		wordSourceIndex + 1 < source.m.size() && (source.m[wordSourceIndex + 1].pma.queryPattern(u"__S1") != -1))
	{
		errorMap[u"LP correct: word 'so' before _S1 is a linking adverbial, not a preposition (Longman - 891)"]++;
		return 0;
	}
	// 26. 'So' before a period or a comma is a pro-form (derived from Longman), but ST says it is an adverb
	if (word == u"so" && primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(u"pronoun") >= 0 &&
		wordSourceIndex + 1 < source.m.size() &&
		(source.m[wordSourceIndex + 1].word->first == u"." || source.m[wordSourceIndex + 1].word->first == u"?" || source.m[wordSourceIndex + 1].word->first == u"!" ||
			source.m[wordSourceIndex + 1].word->first == u","))
	{
		errorMap[u"LP correct: word 'so' before a period or a comma is a pro-form (derived from Longman), but ST says it is an adverb"]++;
		return 0;
	}
	// 27. 'Such' or 'all' before a noun is a predeterminer (LP), not an adjective (ST)!
	if ((word == u"such" || word == u"all") &&
		(primarySTLPMatch == u"adjective") && source.m[wordSourceIndex].queryWinnerForm(u"predeterminer") >= 0)
	{
		errorMap[u"LP correct: word 'such or all': ST says adjective, LP says predeterminer (derived from Longman)"]++;
		return 0;
	}
	// 28. 'Dear' is misinterpreted to be an adverb or a verb (!), and in the 76 examples, dear is almost always correctly interpreted by LP to be an interjection or an adjective.
	if (word == u"dear")
	{
		errorMap[u"LP correct: word 'dear': ST says " + primarySTLPMatch + u" LP says interjection or adjective"]++;
		return 0;
	}
	// 29. 'Though' or 'though' when LP determines is a conjunction (equivalent of a Longman subordinator), Stanford still insists that it is an adverb (wrong)
	if ((word == u"though") && (primarySTLPMatch == u"adverb") && source.m[wordSourceIndex].queryWinnerForm(u"conjunction") >= 0)
	{
		errorMap[u"LP correct: word 'though': ST says adverb LP says conjunction"]++;
		return 0;
	}
	// 30. 'her' when followed by an adverb ending in an 'ly' OR a determiner (a, the) or a personal_pronoun_nominative (I, we) or a coordinator (and,or) or an indefinite_pronoun (everything, nothing) is a pronoun (ST wrong)
	if (word == u"her" && primarySTLPMatch == u"possessive_determiner" && source.m[wordSourceIndex].queryWinnerForm(u"personal_pronoun_accusative") >= 0 &&
		wordSourceIndex + 1 < source.m.size() &&
		(source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(u"determiner") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(u"possessive_determiner") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(u"personal_pronoun_nominative") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(u"coordinator") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(u"indefinite_pronoun") >= 0
			))
	{
		errorMap[u"LP correct: word 'her': [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"]++; // ST 28 LP 124
		return 0;
	}
	if (word == u"her" && primarySTLPMatch == u"possessive_determiner" && source.m[wordSourceIndex].queryWinnerForm(u"personal_pronoun_accusative") >= 0
		&& source.m[wordSourceIndex].pma.queryPatternDiff(u"__NOUN",u"C") != -1 && source.m[wordSourceIndex].pma.queryPattern(u"__ALLOBJECTS_2") != -1)
	{
		errorMap[u"LP correct: word 'her': [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"]++;
		return 0;
	}
	// 30. 'her' when in the beginning of a __NOUN[2]
	if (word == u"her" && source.m[wordSourceIndex].queryWinnerForm(u"possessive_determiner") >= 0 && source.m[wordSourceIndex].pma.queryPattern(u"__NOUN") != -1)
	{
		if (source.m[wordSourceIndex].pma.queryPatternDiff(u"__NOUN", u"COMING") != -1)
		{
			errorMap[u"LP correct: word 'her': [before 'COMING'] ST says personal_pronoun_accusative LP says possessive_determiner"]++; 
			return 0;
		}
		int nf;
		if ((nf = source.m[wordSourceIndex + 1].queryWinnerForm(nounForm)) >= 0)
		{
			int nounCost = source.m[wordSourceIndex + 1].word->second.getUsageCost(nf);
			lpwstring tmpstr;
			int af = source.m[wordSourceIndex + 1].queryForm(adverbForm);
			if (af >= 0)
			{
				int adverbCost = source.m[wordSourceIndex + 1].word->second.getUsageCost(af);
				if (nounCost == 0 && adverbCost > 0)
				{
					errorMap[u"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"]++; // probability 6 out of 130 are ST correct
					return 0;
				}
			}
			else if ((source.m[wordSourceIndex+1].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) != VERB_PRESENT_PARTICIPLE)
			{
				errorMap[u"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"]++; // probability 6 out of 130 are ST correct
				return 0;
			}
		}
	}
	// 31. 'that' when followed by an _S1 is a relativizer, not a preposition (ST wrong)
	if (word == u"that")
	{
		if (wordSourceIndex + 1 < source.m.size() && source.m[wordSourceIndex + 1].pma.queryPattern(u"__S1") != -1 && primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(u"demonstrative_determiner") >= 0)
		{
			for (int nextByPosition = source.m[wordSourceIndex].beginPEMAPosition; nextByPosition != -1; nextByPosition = source.pema[nextByPosition].nextByPosition)
			{
				cPattern *p = patterns[source.pema[nextByPosition].getParentPattern()];
				if (p->name == u"__S1" && p->differentiator == u"5" && (source.pema[nextByPosition].getElement() == 2 || source.pema[nextByPosition].getElement() == 3))
				{
					errorMap[u"LP correct: word 'that': [embedded in _S1[5]] ST says preposition LP says demonstrative_determiner (relativizer)"]++;
					return 0;
				}
			}
			errorMap[u"LP correct: word 'that': [immediately preceding _S1 otherwise unidentified (hidden in pattern, must be identified in post-processing)] ST says preposition LP says demonstrative_determiner (relativizer)"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"modal_auxiliary" && (source.m[wordSourceIndex].queryWinnerForm(u"verbverb") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"does") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"does_negation") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"have_negation") >= 0))
	{
		errorMap[u"diff: ST says modal_auxiliary and LP says verbverb, does, does_negation, have_negation"]++;
		return 0;
	}
	// 32. 'out' is an adverb particle (Longman 78,413), not a preposition (ST wrong)
	if (word == u"out" && primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
	{
		errorMap[u"LP correct: word 'out': [adverb particle] ST says preposition LP says adverb"]++;
		return 0;
	}
	// 33. 'at least' is an adverbial phrase (Longman 542), least for LP could be a pronoun or a quantifier (https://english.stackexchange.com/questions/107396/what-are-the-parts-of-speech-of-at-and-least-in-at-least)
	// most is a quantifier and a degree adverb, unlike least (Longman, 522)
	// the POS for at least is simply not well established.  LP has it as a pronoun because that works within the resolving logic
	if ((word == u"least" || word == u"most") &&
		primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"pronoun") >= 0)
	{
		errorMap[u"diff: word 'least': [part of adverbial phrase 'at least' or more of a noun 'the least'] ST says adjective"]++;
		return 0;
	}
	if (word==u"to-morrow" && primarySTLPMatch == u"noun")
	{
		errorMap[u"diff: word 'to-morrow': ST says noun, but this is always used as a time adverbial"]++;
		return 0;
	}
	// (adjective) not found in winnerForms modal_auxiliary for word wouldhad
	if (word == u"wouldhad" && source.m[wordSourceIndex].queryWinnerForm(u"modal_auxiliary") >= 0 && primarySTLPMatch == u"adjective")
	{
		errorMap[u"diff: word 'wouldhad': wouldhad special to LP"]++;
		return 0;
	}
	// 34. possessive pronoun section - ST likes to think of possessive pronouns as verbs.  This will have to be investigated at some point by examining the collection of statistics from the original source material
	if (word == u"mine" && source.m[wordSourceIndex].queryWinnerForm(u"possessive_pronoun") >= 0)
	{
		if (primarySTLPMatch == u"verb" || primarySTLPMatch == u"interjection")
		{
			errorMap[u"LP correct: word 'mine': [verb or interjection] ST says verb or interjection LP says possessive_pronoun, which is always correct (26 examples out of 5968058)"]++;
			return 0;
		}
		if ((primarySTLPMatch == u"noun" || primarySTLPMatch == u"personal_pronoun_accusative") && source.m[wordSourceIndex].queryWinnerForm(u"possessive_pronoun") >= 0)
		{
			errorMap[u"LP correct: word 'mine': ST says noun or personal_pronoun_accusative, LP says possessive_pronoun"]++;
			return 0;
		}
	}
	if (word == u"yours" && source.m[wordSourceIndex].queryWinnerForm(u"possessive_pronoun") >= 0 &&
		(primarySTLPMatch == u"verb" || primarySTLPMatch == u"interjection" || primarySTLPMatch == u"numeral_cardinal" || primarySTLPMatch == u"adjective" || primarySTLPMatch == u"adverb" || primarySTLPMatch == u"possessive_determiner"))
	{
		errorMap[u"LP correct: word 'yours': ST says verb, interjection, numeral_cardinal, adjective or possessive_determiner LP says possessive_pronoun, which is always correct"]++;
		return 0;
	}
	if (word == u"hers" && source.m[wordSourceIndex].queryWinnerForm(u"possessive_pronoun") >= 0 && (primarySTLPMatch == u"verb" || primarySTLPMatch == u"adjective"))
	{
		errorMap[u"LP correct: word 'hers': ST says verb, adjective LP says possessive_pronoun, which is always correct"]++;
		return 0;
	}
	// possessive pronoun section - end
	// 35. round is never a verb in the gutenberg corpus
	if (word == u"round")
	{
		if ((source.m[wordSourceIndex].queryWinnerForm(u"preposition") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0) && primarySTLPMatch == u"verb")
		{
			errorMap[u"LP correct: word 'round': ST says verb LP says preposition or adverb.  round as studied is never a verb in > 200 examples from corpus"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].isOnlyWinner(prepositionForm) && (primarySTLPMatch == u"noun" || primarySTLPMatch == u"adverb"))
		{
			int primaryPMAOffset = source.m[wordSourceIndex].pma.queryPattern(u"__ALLOBJECTS_1");
			int secondaryPMAOffset = source.m[wordSourceIndex].pma.queryPattern(u"_ADVERB");
			if (primaryPMAOffset != -1 && secondaryPMAOffset != -1)
			{
				primaryPMAOffset = primaryPMAOffset & ~cMatchElement::patternFlag;
				secondaryPMAOffset = secondaryPMAOffset & ~cMatchElement::patternFlag;
				if (source.m[wordSourceIndex].pma[secondaryPMAOffset].len == 1 && source.queryPattern(wordSourceIndex + 1, u"__NOUN") != -1 && source.m[wordSourceIndex].queryForm(prepositionForm) != -1)
				{
					errorMap[u"LP correct: word 'round': ST says adverb/noun LP says preposition."]++;  // ST correct 5, out of 126
					return 0;
				}
			}
		}
	}
	// 36. please is rarely a verb.  It is mostly used as a discourse politeness marker (Longman)
	if (word == u"please" && source.m[wordSourceIndex].queryWinnerForm(u"politeness_discourse_marker") >= 0 && (primarySTLPMatch == u"verb" || primarySTLPMatch == u"adverb" || primarySTLPMatch == u"adjective"))
	{
		errorMap[u"LP correct: word 'please': ST says verb LP says politeness_discourse_marker"]++;
		return 0;
	}
	// 37. less is never a conjunction.  (Longman)
	if (word == u"less" && primarySTLPMatch == u"conjunction")
	{
		errorMap[u"LP correct: word 'less': ST says conjunction LP says quantifier/adverb/adjective"]++;
		return 0;
	}
	// 38. I is always a personal_pronoun_nominative
	if (word == u"i" && source.m[wordSourceIndex].queryWinnerForm(u"personal_pronoun_nominative") >= 0 && (primarySTLPMatch == u"noun" || primarySTLPMatch == u"interjection"))
	{
		errorMap[u"LP correct: word 'I': ST says " + primarySTLPMatch + u" LP says personal_pronoun_nominative"]++;
		return 0;
	}
	if (word == u"i" && source.m[wordSourceIndex].queryWinnerForm(u"roman_numeral") >= 0 && (source.m[wordSourceIndex-1].word->first==u"chapter" || source.m[wordSourceIndex - 1].word->first == u"book"))
	{
		errorMap[u"LP correct: word 'I': ST says " + primarySTLPMatch + u" LP says roman_numeral after chapter or book"]++;
		return 0;
	}
	// 39. a is always a determiner
	if (word == u"a" && source.m[wordSourceIndex].queryWinnerForm(u"determiner") >= 0 && (primarySTLPMatch.empty() || primarySTLPMatch == u"noun" || primarySTLPMatch == u"symbol"))
	{
		errorMap[u"LP correct: word 'a': ST says " + primarySTLPMatch + u" LP says determiner"]++;
		return 0;
	}
	// 40. but is never a verb (reviewed all examples in corpus)
	if (word == u"but" && source.m[wordSourceIndex].queryWinnerForm(u"conjunction") >= 0 && primarySTLPMatch == u"verb")
	{
		errorMap[u"LP correct: word 'but': ST says " + primarySTLPMatch + u" LP says conjunction"]++;
		return 0;
	}
	if (word == u"no" && primarySTLPMatch == u"determiner" && source.m[wordSourceIndex].queryWinnerForm(u"interjection") >= 0 && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))
	{
		errorMap[u"LP correct: word 'no': determiner before nothing is incorrect"]++;
		return 0;
	}
	if (word == u"no" && primarySTLPMatch == u"adverb") 
	{
		if (source.m[wordSourceIndex].queryWinnerForm(u"no") >= 0)
		{
			if (wordSourceIndex > 1 && source.m[wordSourceIndex - 1].word->second.mainEntry->first == u"say")
				errorMap[u"LP correct: word 'no' which is literally saying no"]++;
			else
				errorMap[u"diff: word 'no': ST says " + primarySTLPMatch + u" LP says 'no'"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].queryWinnerForm(u"interjection") >= 0 && (atStart || !iswalpha(source.m[wordSourceIndex - 1].word->first[0] || source.m[wordSourceIndex-1].queryForm(u"interjection") >= 0 || source.m[wordSourceIndex - 1].word->first==u"but") && !iswalpha(source.m[wordSourceIndex + 1].word->first[0])))
		{
			errorMap[u"LP correct: word 'no' is interjection not adverb when alone"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].queryWinnerForm(u"determiner") >= 0 && source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0)
		{
			errorMap[u"LP correct: word 'no' is a determiner and not adverb when immediately before a noun"]++;
			return 0;
		}
	}
	// the is never anything but a determiner
	if (word == u"the" && source.m[wordSourceIndex].queryWinnerForm(u"determiner") >= 0)
	{
		errorMap[u"LP correct: word 'the': ST says " + primarySTLPMatch + u" LP says determiner"]++;
		return 0;
	}
	// worth while
	if (word == u"while" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && wordSourceIndex>=2 &&
		  ((cWord::isDash(source.m[wordSourceIndex-1].word->first[0]) && source.m[wordSourceIndex - 2].word->first==u"worth") || source.m[wordSourceIndex - 1].word->first==u"worth"))
	{
		errorMap[u"LP correct: word 'while': ST says " + primarySTLPMatch + u" LP says noun [worthwhile]"]++;
		return 0;
	}
	if (word == u"while" && source.m[wordSourceIndex].queryWinnerForm(u"uncertainDurationUnit") >= 0 && source.m[wordSourceIndex].pma.queryPattern(u"__INTRO_N") != -1)
	{
		errorMap[u"LP correct: word 'while': ST says " + primarySTLPMatch + u" LP says uncertainDurationUnit [__INTRO_N]"]++;
		return 0;
	}
	// 40. but is never a verb (reviewed all examples in corpus)
	if (word == u"want" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && primarySTLPMatch == u"verb" && 
		  (source.m[wordSourceIndex-1].word->first==u"in" || source.m[wordSourceIndex - 1].word->first == u"for" || source.m[wordSourceIndex - 1].word->first == u"from" || source.m[wordSourceIndex - 1].word->first == u"by" || source.m[wordSourceIndex - 1].word->first == u"of"))
	{
		errorMap[u"LP correct: word 'want': ST says " + primarySTLPMatch + u" LP says noun"]++;
		return 0;
	}
	// 41. in between two adverbs/adjectives, a verb and adverb/adjective or all/does/has and a verb.  100 examples in corpus with 100% correctness.
	if (wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
	{
		if (((source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"adjective") >= 0) &&
			   (source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0)) || 
				((source.m[wordSourceIndex - 1].queryWinnerForm(u"verb") >= 0) &&
				 (source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0)) || 
		    ((source.m[wordSourceIndex - 1].word->first == u"all" || source.m[wordSourceIndex - 1].queryWinnerForm(u"does") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"has") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"is") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"verbverb") >= 0) &&
				 (source.m[wordSourceIndex + 1].queryWinnerForm(u"verb") >= 0)))
		{
			set <lpwstring> wordsNotAdverbs = { u"dark",u"to-night",u"to-morrow",u"there" };
			if (wordsNotAdverbs.find(source.m[wordSourceIndex + 1].word->first)== wordsNotAdverbs.end())
				errorMap[u"LP correct: ST says " + primarySTLPMatch + u" LP says adverb [contextual]"]++;
			else
				errorMap[u"ST correct: ST says " + primarySTLPMatch + u" LP says adverb (next word [dark, to-night, etc] not adverb!)"]++;
			return 0;
		}
	}
	// 42. this is correct 95% of the time in the corpus (over 100 examples).  Only once was it perhaps a conjunction.
	if (word == u"but" && wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(u"preposition") >= 0)
	{
		errorMap[u"LP correct: word 'but': ST says " + primarySTLPMatch + u" LP says preposition"]++;
		return 0;
	}
	// 43. this is correct 100% of the time in the corpus (over 100 examples).  
	if (word == u"more" && wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
	{
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(u"verb") >= 0) ||
			(source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0 && source.m[wordSourceIndex + 1].queryForm(u"indefinite_pronoun") < 0) ||
			(source.m[wordSourceIndex - 1].word->first == u"the"))
		{
			errorMap[u"LP correct: word 'more': ST says " + primarySTLPMatch + u" LP says adverb"]++;
			return 0;
		}
	}
	if (word == u"more" && wordSourceIndex > 0 && source.m[wordSourceIndex].queryWinnerForm(u"quantifier") >= 0)
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0 && source.m[wordSourceIndex + 1].queryForm(u"verb")<0)
		{
			errorMap[u"LP correct: word 'more': ST says " + primarySTLPMatch + u" LP says quantifier"]++;
			return 0;
		}
	}
	// p80, Longman
	if (word == u"yet" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && primarySTLPMatch == u"conjunction")
	{
		errorMap[u"diff: word 'yet': can be either a conjunction or a linking adverbial"]++;
		return 0;
	}
	// never correct
	if (word == u"more" && (primarySTLPMatch == u"interjection" || primarySTLPMatch == u"verb"))
	{
		errorMap[u"LP correct: word 'more': ST says " + primarySTLPMatch]++;
		return 0;
	}
	// never correct
	if (word == u"once" && primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && (source.m[wordSourceIndex - 1].word->first==u"at" || source.m[wordSourceIndex - 1].word->first == u"for"))
	{
		errorMap[u"diff: word 'once': in saying 'at once' or 'for once'"]++;
		return 0;
	}
	// never correct
	if ((word == u"after" || word == u"besides") && primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 &&
		  (!iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || source.m[wordSourceIndex + 1].queryWinnerForm(u"verb")>=0))
	{
		errorMap[u"LP correct: word 'after, besides': ST says " + primarySTLPMatch + u"but LP says adverb"]++;
		return 0;
	}
	// Longman p85 subordinator 'as though' - subordinating conjunction
	if (word == u"as" && primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(u"conjunction") >= 0 && source.m[wordSourceIndex + 1].word->first==u"though")
	{
		errorMap[u"LP correct: word 'as': ST says adverb but LP says conjunction"]++;
		return 0;
	}
	if (word == u"as") // && source.m[wordSourceIndex + 1].word->first == u"to")
	{
		// 'as to stand outside in the wind' - 'as' is part of a subordinating conjunctive phrase and so therefore a conjunction
		if (source.m[wordSourceIndex].queryWinnerForm(u"conjunction") >= 0 && (source.m[wordSourceIndex + 1].pma.queryPattern(u"_INFP") != -1 || source.m[wordSourceIndex].pma.queryPattern(u"_INFP") != -1) && source.m[wordSourceIndex + 1].word->first!=u"if")
		{
			errorMap[u"LP correct: word 'as': ST says " + primarySTLPMatch + u" but LP says conjunction (complex subordinator)"]++;
			return 0;
		}
		// 'as to the romantic nonsense' - 'as' is a preposition - part of the complex preposition referred to in Longman
		if (source.m[wordSourceIndex].queryWinnerForm(u"preposition") >= 0 && (source.m[wordSourceIndex + 1].pma.queryPattern(u"_PP") != -1 || source.m[wordSourceIndex].pma.queryPattern(u"_PP") != -1))
		{
			errorMap[u"LP correct: word 'as': ST says " + primarySTLPMatch + u" but LP says preposition (complex preposition)"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].pma.queryPattern(u"__S1") != -1)
		{
			if (primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
			{
				errorMap[u"LP correct: word 'as': ST says preposition and LP says adverb"]++;
				return 0;
			}
			if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(u"conjunction") >= 0)
			{
				errorMap[u"diff: word 'as': ST says adverb and LP says conjunction"]++;
				return 0;
			}
		}
		// followed by an adjective? [2 instances where conjunction might be considered out of 100 observed]
		if (source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0)
		{
			errorMap[u"LP correct: word 'as': ST says preposition or conjunction and LP says adverb"]++;
			return 0;
		}
	}
	if ((primarySTLPMatch == u"adjective") && (source.m[wordSourceIndex].queryWinnerForm(u"honorific_abbreviation") !=-1)) // source.m[wordSourceIndex].queryWinnerForm(u"honorific") !=-1 || 
	{
		errorMap[u"LP correct: ST says adjective when LP says honorific/honorific_abbreviation"]++;
		return 0;
	}
	// 45. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == u"verb") && !source.m[wordSourceIndex].word->second.hasVerbForm())
	{
		errorMap[u"LP correct: ST says verb when no verb form possible"]++;
		return 0;
	}
	// 46. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == u"conjunction") && source.m[wordSourceIndex].queryForm(u"conjunction") < 0)
	{
		errorMap[u"LP correct: ST says conjunction when no conjunction form possible)"]++;
		return 0;
	}
	// 47. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == u"preposition or conjunction") && source.m[wordSourceIndex].queryForm(u"preposition") < 0)
	{
		errorMap[u"LP correct: ST says preposition when no preposition form possible)"]++;
		return 0;
	}
	// 48. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == u"adverb") && source.m[wordSourceIndex].queryForm(u"adverb") < 0 &&
		   (source.m[wordSourceIndex].queryWinnerForm(u"adjective") < 0 || word[word.length()-2]!=u'l' || word[word.length() - 1] != u'y')) // don't end in ly
	{
		errorMap[u"LP correct: ST says adverb when no adverb form possible)"]++;
		return 0;
	}
	// 49. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == u"interjection") && source.m[wordSourceIndex].queryForm(u"interjection") < 0)
	{
		errorMap[u"LP correct: ST says interjection when no interjection form possible)"]++;
		return 0;
	}
	// 50. this is correct 100% of the time in the corpus (over 100 examples).  
	if ((primarySTLPMatch == u"numeral_cardinal") && source.m[wordSourceIndex].queryForm(u"numeral_cardinal") < 0)
	{
		errorMap[u"LP correct: ST says numeral_cardinal when no numeral_cardinal form possible)"]++;
		return 0;
	}
	// 51. this is correct 100% of the time 
	if ((primarySTLPMatch == u""))
	{
		errorMap[u"LP correct: ST says interjection when no interjection form possible)"]++;
		return 0;
	}
	// 52. this is correct 100% of the time 
	if ((primarySTLPMatch == u"|||"))
	{
		errorMap[u"diff: ST says list but LP does not have that semantic category yet"]++;
		return 0;
	}
	// 53. this is correct 100% of the time 
	if ((primarySTLPMatch == u"symbol"))
	{
		errorMap[u"LP correct: ST says symbol when it is not a symbol"]++;
		return 0;
	}
	// 54. this is correct 100% of the time 
	if (primarySTLPMatch == u"determiner")
	{
		vector<lpwstring> determinerTypes = { u"determiner",u"demonstrative_determiner",u"possessive_determiner",u"interrogative_determiner", u"quantifier", u"numeral_cardinal" };
		bool detMatch = false;
		for (lpwstring dt : determinerTypes)
			if (detMatch = source.m[wordSourceIndex].queryWinnerForm(dt) >= 0)
				break;
		if (!detMatch)
		{
			if (word == u"both")
			{
				errorMap[u"LP correct: word 'both': ST says determiner when there is no following noun form (LP says pronoun)"]++;
				return 0;
			}
			if (word == u"a trifle" || word == u"a bit")
			{
				errorMap[u"LP correct: word 'a trifle' or 'a bit': ST says determiner when LP says adverb"]++;
				return 0;
			}
			if (word == u"another" && source.m[wordSourceIndex].queryWinnerForm(pronounForm) >= 0 && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) < 0 &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(determinerForm) >= 0 || !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) ||
					source.m[wordSourceIndex + 1].word->second.hasVerbForm() || source.m[wordSourceIndex + 1].queryWinnerForm(conjunctionForm) >= 0))
			{
				errorMap[u"LP correct: word 'another': ST says determiner when there is no following noun form (LP says pronoun)"]++;
				return 0;
			}
			if (word == u"any" && source.m[wordSourceIndex].queryWinnerForm(adverbForm) >= 0 &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(adjectiveForm) >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(adverbForm) >= 0))
			{
				errorMap[u"LP correct: word 'any': ST says determiner when there is only a following adverb or adjective form (LP says adverb)"]++;
				return 0;
			}
		}
	}
	if (word == u"to-night" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
	{
		errorMap[u"LP correct: word 'to-night': ST says " + primarySTLPMatch + u" but LP says adverb"]++;
		return 0;
	}
	if (word == u"well" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && source.m[wordSourceIndex + 1].word->first == u"-")
	{
		errorMap[u"LP correct: word 'well': ST says " + primarySTLPMatch + u" but LP says adverb"]++;
		return 0;
	}
	if (word == u"well" && source.m[wordSourceIndex].queryWinnerForm(u"interjection") >= 0)
	{
		errorMap[u"LP correct: word 'well': ST says " + primarySTLPMatch + u" but LP says interjection"]++;
		return 0;
	}
	if (word == u"either" && (source.m[wordSourceIndex].pma.queryPatternDiff(u"__NOUN", u"O") !=-1 || source.m[wordSourceIndex].pma.queryPatternDiff(u"_VERBPRESENTC", u"O") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(u"_VERBPAST", u"O") != -1))
	{
		errorMap[u"LP correct: word 'either': ST says " + primarySTLPMatch + u" but LP says quantifier"]++;
		return 0;
	}
	if (word == u"neither" && source.m[wordSourceIndex].pma.queryPatternDiff(u"__NOUN", u"P") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(u"_VERBPRESENTC", u"P") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(u"_VERBPAST", u"P") != -1)
	{
		errorMap[u"LP correct: word 'neither': ST says " + primarySTLPMatch + u" but LP says quantifier"]++;
		return 0;
	}
	// almost all examples studied from 995 low numUnknown sources
	if (word == u"neither" && primarySTLPMatch == u"determiner" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
	{
		errorMap[u"LP correct: word 'neither': ST says " + primarySTLPMatch + u" but LP says adverb"]++;
		return 0;
	}
	if (word == u"you" && primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"personal_pronoun") >= 0)
	{
		errorMap[u"LP correct: word 'you': ST says " + primarySTLPMatch + u" but LP says personal_pronoun"]++;
		return 0;
	}
	// Stanford POS JJ (adjective) not found in winnerForms adverb for word little 0000121:[With a sigh she pressed the pillow more firmly under her cheek , and lay looking a *little* wistfully at her maid , who , having drawn back the curtains at the window , stood now regarding her with the discreet and confidential smile which drew from her a protesting frown of irritation . ]
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && wordSourceIndex + 1 < source.m.size() && source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0)
	{
		errorMap[u"LP correct: Modifying an adverb ST says " + primarySTLPMatch + u" but LP says adverb"]++;
		return 0;
	}
	if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(u"preposition") >= 0 && wordSourceIndex + 1 < source.m.size() && source.m[wordSourceIndex + 1].pma.queryPattern(u"__NOUN")!=-1 && source.m[wordSourceIndex].pma.queryPattern(u"_PP") != -1)
	{
		errorMap[u"LP correct: ST says " + primarySTLPMatch + u" but LP says preposition - as the head of a _PP construct"]++;
		return 0;
	}
	bool wordBeforeIsIs = wordSourceIndex >0 && (source.m[wordSourceIndex - 1].queryWinnerForm(isForm) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(isNegationForm) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(beForm) != -1 || source.m[wordSourceIndex - 1].word->first == u"being");
	bool wordBeforeIsVerb = wordSourceIndex > 0 && source.m[wordSourceIndex - 1].hasWinnerVerbForm();
	bool word2BeforeIsIs = wordSourceIndex>1 && (source.m[wordSourceIndex - 2].queryWinnerForm(isForm) != -1 || source.m[wordSourceIndex - 2].queryWinnerForm(isNegationForm) != -1 || source.m[wordSourceIndex - 2].queryWinnerForm(beForm) != -1 || source.m[wordSourceIndex - 2].word->first == u"being");
	bool word2BeforeIsVerb = wordSourceIndex>1 && source.m[wordSourceIndex - 2].hasWinnerVerbForm();
	vector<lpwstring> determinerTypes = { u"determiner",u"demonstrative_determiner",u"possessive_determiner",u"interrogative_determiner", u"quantifier", u"numeral_cardinal" };
	bool wordBeforeIsDeterminer = false;
	if (wordSourceIndex > 0)
		for (lpwstring dt : determinerTypes)
			if (wordBeforeIsDeterminer = source.m[wordSourceIndex - 1].queryWinnerForm(dt) >= 0)
				break;
	bool word2BeforeIsDeterminer = false;
	if (wordSourceIndex>1)
		for (lpwstring dt : determinerTypes)
			if (word2BeforeIsDeterminer = source.m[wordSourceIndex - 2].queryWinnerForm(dt) >= 0)
				break;
	bool wordAfterIsDeterminer = false;
	if (wordSourceIndex+1 <source.m.size())
		for (lpwstring dt : determinerTypes)
			if (wordAfterIsDeterminer = source.m[wordSourceIndex + 1].queryWinnerForm(dt) >= 0)
				break;
	const lpchar_t *unmodifiableForms[] = { u"relativizer",u"preposition",u"coordinator",u"conjunction",u"quantifier", u"adverb",u"adjective",u"personal_pronoun_accusative",u"personal_pronoun_nominative",u"personal_pronoun",u"reflexive_pronoun" };
	bool wordAfterIsUnmodifiable = false;
	if (wordSourceIndex+1 < source.m.size())
		for (lpwstring unForm : unmodifiableForms)
			if (wordAfterIsUnmodifiable = source.m[wordSourceIndex + 1].queryWinnerForm(unForm) >= 0)
				break;
	wordAfterIsUnmodifiable |= !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || wordAfterIsDeterminer;
	vector<lpwstring> pronounTypes = { u"personal_pronoun_accusative",u"personal_pronoun_nominative",u"personal_pronoun",u"reflexive_pronoun",u"indefinite_pronoun" };
	bool wordBeforeIsPronoun = false;
	if (wordSourceIndex > 0)
		for (lpwstring pn : pronounTypes)
			if (wordBeforeIsPronoun = source.m[wordSourceIndex - 1].queryWinnerForm(pn) >= 0)
				break;
	if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0 && wordSourceIndex + 1 < source.m.size() && wordSourceIndex>3)
	{
		// investigate later!
		if (((wordBeforeIsVerb && wordBeforeIsIs) || (word2BeforeIsVerb && word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0)) && wordAfterIsUnmodifiable)
		{
			if (source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0)
				errorMap[u"ST correct: ST says adverb but LP says adjective, IS following a verb and followed by an adjective or adverb"]++;
			else
			  errorMap[u"LP correct: ST says adverb but LP says adjective, IS following a verb and followed by a relativizer, preposition or coordinator"]++;
			return 0;
		}
		// verb *ADJ* (relativizer OR preposition OR coordinator or ,)
		if (((wordBeforeIsVerb && !wordBeforeIsIs)|| (word2BeforeIsVerb && !word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0)) && wordAfterIsUnmodifiable)
		{
			errorMap[u"ST correct: ST says adverb but LP says adjective, following a verb and followed by a relativizer, preposition or coordinator"]++;
			return 0;
		}
		else
		{
			int adjectivePEMAOffset = source.queryPattern(wordSourceIndex, u"__ADJECTIVE");
			// an *even* and noiseless step
			if (wordBeforeIsDeterminer && source.m[wordSourceIndex + 1].queryWinnerForm(u"coordinator") >= 0 && source.m[wordSourceIndex + 2].queryWinnerForm(u"adjective") >= 0 && source.m[wordSourceIndex + 3].queryWinnerForm(u"noun") >= 0)
			{
				errorMap[u"LP correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			// how *much* trouble
			else if ((wordBeforeIsDeterminer || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"preposition") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(u"conjunction") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(u"coordinator") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(u"adjective") >= 0 ||
				source.queryPattern(wordSourceIndex - 1, u"__ADJECTIVE") != -1 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"relativizer") >= 0 || 
				!iswalpha(source.m[wordSourceIndex - 1].word->first[0])) &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"timeUnit") >= 0))
			{
				errorMap[u"LP correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			else if ((wordBeforeIsDeterminer || source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0 ) && (source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0 || source.queryPattern(wordSourceIndex + 1, u"__ADJECTIVE") != -1 || source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0))
			{
				errorMap[u"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			// much better looking - true except for IS verbs (
			else if (source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0 && source.m[wordSourceIndex + 1].hasWinnerVerbForm())
			{
				errorMap[u"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			// correct except for the rare IS verb or a noun restatement (you measly scrub!)
			else if ((wordBeforeIsPronoun || source.m[wordSourceIndex - 1].queryWinnerForm(u"Proper Noun") >= 0) && source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") < 0)
			{
				errorMap[u"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			if (source.m[wordSourceIndex + 1].queryWinnerForm(u"Proper Noun") >= 0)
			{
				errorMap[u"ST correct: ST says adverb but LP says adjective"]++;
				return 0;
			}
			else if (source.m[wordSourceIndex + 1].word->first == u"-")
			{
				if (source.m[wordSourceIndex + 2].queryWinnerForm(u"noun") >=0 && source.queryPattern(wordSourceIndex + 2, u"__ADJECTIVE") == -1)
				{
					errorMap[u"LP correct: ST says adverb but LP says adjective with dash and then a noun"]++;
					return 0;
				}
				else
				{
					errorMap[u"ST correct: ST says adverb but LP says adjective with dash and then a non-noun"]++;
					return 0;
				}
			}
			else if (adjectivePEMAOffset != -1 && source.queryPatternDiff(wordSourceIndex, u"__S1",u"7") != -1)
			{
				if (source.m[wordSourceIndex].queryWinnerForm(adjectiveForm) >= 0)
					errorMap[u"ST correct: ST says adverb but LP says adjective with __S1[7] before an adjective"]++;
				else
					errorMap[u"LP correct: ST says adverb but LP says adjective with __S1[7] alone"]++;
				return 0;
			}
			if (source.queryPattern(wordSourceIndex, u"_TIME") != -1)
			{
				errorMap[u"ST correct: ST says adverb but LP says adjective in _TIME structure"]++;
				return 0;
			}
		}
		if (word == u"o'clock" && primarySTLPMatch == u"adverb")
		{
			errorMap[u"diff: ST says adverb (which is correct by form) but LP says noun, from usage"]++;
			return 0;
		}
		if (word == u"but" && source.m[wordSourceIndex + 1].word->first==u"--")
		{
			errorMap[u"LP correct: 'but' before a double dash is a conjunction!"]++;
			return 0;
		}
		int maxlen = -1;
		if ((source.queryPattern(wordSourceIndex - 1, u"_BE", maxlen) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(u"is") >= 0) &&
			source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(u"verb") < 0)
		{
			errorMap[u"LP correct: ST says adverb but LP says adjective, following a being verb"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0)
	{
		int pemaOffset = source.queryPattern(wordSourceIndex, u"__NOUN");
		if (source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"4") != -1)
		{
			errorMap[u"diff: ST says " + primarySTLPMatch + u" but LP says adjective in the head of a __NOUN construction"]++;
			return 0;
		}
		// two incorrect parses lead to inaccuracy (ST is correct)
		if (source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"2") != -1 && source.m[wordSourceIndex].pma.queryPattern(u"__ADJECTIVE") != -1)
		{
			errorMap[u"LP correct: ST says " + primarySTLPMatch + u" but LP says adjective in an __ADJECTIVE construction"]++;
			return 0;
		}
		if (pemaOffset>=0 && cWord::isDash((source.m[wordSourceIndex + 1].word->first[0])) && source.m[wordSourceIndex + 1].word->first.length()==1)
		{
			errorMap[u"LP correct: ST says " + primarySTLPMatch + u" but LP says adjective before dash"]++;
			return 0;
		}
		if (word == u"right")
		{
			errorMap[u"LP correct: word 'right': ST says " + primarySTLPMatch + u" but LP says adjective"]++;
			return 0;
		}
	}
	if (wordSourceIndex + 2 < source.m.size() && cWord::isDash((source.m[wordSourceIndex + 1].word->first[0])) && source.m[wordSourceIndex + 1].word->first.length() == 1)
	{
		int pemaOffset = source.queryPattern(wordSourceIndex, u"__NOUN");
		bool adjectivePosition = (pemaOffset >= 0) ? (source.pema[pemaOffset].end > 1) : false, nounHeadPosition = (pemaOffset >= 0) ? (source.pema[pemaOffset].end == 1) : false;
		if (source.m[wordSourceIndex + 2].word->first != u"and" && source.m[wordSourceIndex + 2].word->first != u"to" && source.m[wordSourceIndex + 2].word->first != u"for" &&
			source.m[wordSourceIndex + 2].word->first != u"of" &&	source.m[wordSourceIndex + 2].queryWinnerForm(determinerForm) < 0 &&
			source.m[wordSourceIndex].queryWinnerForm(interjectionForm) < 0)
		{
			if ((source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"numeral_ordinal") >= 0) && 
				  (primarySTLPMatch == u"noun" || primarySTLPMatch == u"determiner" || primarySTLPMatch == u"predeterminer"))
			{
				errorMap[u"LP correct: ST says " + primarySTLPMatch + u" but LP says adjective before dash"]++;
				return 0;
			}
			else if (source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
			{
				errorMap[u"LP correct: ST says " + primarySTLPMatch + u" but LP says adjective/adverb before dash"]++;
				return 0;
			}
			else if ((source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0) && primarySTLPMatch == u"adjective" && (pemaOffset<0 || nounHeadPosition))
			{
				errorMap[u"ST correct: ST says " + primarySTLPMatch + u" but LP says noun before dash"]++;
				return 0;
			}
			else if (adjectivePosition)
			{
				errorMap[u"LP correct: ST says " + primarySTLPMatch + u" but LP says adjective position in __NOUN structure before dash"]++;
				return 0;
			}
		}
	}
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0)
	{
		int pemaPosition = -1;
		// two incorrect parses lead to inaccuracy (ST is correct)
		if ((pemaPosition=source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"2")) != -1)
		{
			if (source.m[wordSourceIndex].pma.queryPattern(u"__ADJECTIVE") != -1)
			{
				errorMap[u"diff: ST says adjective and LP says noun in an __ADJECTIVE construction"]++;
				return 0;
			}
			for (; pemaPosition != -1; pemaPosition = source.pema[pemaPosition].nextByPosition)
				if (patterns[source.pema[pemaPosition].getParentPattern()]->name == u"__NOUN" && patterns[source.pema[pemaPosition].getParentPattern()]->differentiator == u"2")
				{
					if (!source.pema[pemaPosition].isChildPattern() && source.m[wordSourceIndex].getFormNum(source.pema[pemaPosition].getChildForm()) == nounForm)
					{
						errorMap[u"diff: ST says adjective and LP says noun in an __NOUN(n) construction"]++;
						return 0;
					}
				}
		}
		if (source.queryPattern(wordSourceIndex, u"__ADJECTIVE") != -1)
		{
			if (cWord::isDash(source.m[wordSourceIndex + 1].word->first[0]))
			{
				errorMap[u"diff: ST says adjective and LP says noun in an __ADJECTIVE construction"]++;
				return 0;
			}
		}
	}
	if ((wordSourceIndex>0 && (source.m[wordSourceIndex - 1].queryWinnerForm(u"modal_auxiliary") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"future_modal_auxiliary") >= 0 ||
		source.m[wordSourceIndex - 1].queryWinnerForm(u"negation_modal_auxiliary") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"negation_future_modal_auxiliary") >= 0)) &&
		wordSourceIndex+1<source.m.size() && source.m[wordSourceIndex + 1].queryWinnerForm(u"verb") >= 0 && word == u"better" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
	{
		errorMap[u"LP correct: LP says adverb ST says "+ primarySTLPMatch]++;
		return 0;
	}
	// 100 examples checked - no errors
	if (primarySTLPMatch == u"preposition or conjunction" && wordAfterIsDeterminer && (source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"particle") >= 0))
	{
		errorMap[u"ST correct: LP says adverb or particle when ST says " + primarySTLPMatch]++;
		return 0;
	}
	// verb *ADJ* (relativizer OR preposition OR coordinator or ,)
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && wordSourceIndex + 1 < source.m.size() && wordSourceIndex > 3)
	{
		if (wordAfterIsUnmodifiable)
		{
			if ((wordBeforeIsVerb && wordBeforeIsIs) || (word2BeforeIsVerb && word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0))
			{
				errorMap[u"ST correct: LP says adverb but ST says adjective, IS following a verb and followed by a relativizer, preposition or coordinator"]++;
				return 0;
			}
			if ((wordBeforeIsVerb && !wordBeforeIsIs) || (word2BeforeIsVerb && !word2BeforeIsIs && source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0))
			{
				errorMap[u"LP correct: LP says adverb but ST says adjective, following a verb and followed by a relativizer, preposition or coordinator"]++;
				return 0;
			}
		}
		{
			// Still sipping his coffee , he regarded her with the blithe humour which lent so *great* a charm to his expression . 
			// The game was as old as the Garden of Eden, she had played it well or *ill* from her cradle, and at last she had begun to grow a trifle weary .

			// how *much* trouble
			if ((wordBeforeIsDeterminer || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"preposition") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0 ||
				source.m[wordSourceIndex - 1].queryWinnerForm(u"conjunction") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"coordinator") >= 0 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"adjective") >= 0 || 
				source.queryPattern(wordSourceIndex - 1, u"__ADJECTIVE") != -1 || 
				source.m[wordSourceIndex - 1].queryWinnerForm(u"relativizer") >= 0 || 
				!iswalpha(source.m[wordSourceIndex - 1].word->first[0])) &&
				(source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"timeUnit") >= 0))
			{
				errorMap[u"ST correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			else if ((wordBeforeIsDeterminer || source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") >= 0) && (source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0 || source.queryPattern(wordSourceIndex + 1, u"__ADJECTIVE") != -1 || source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") >= 0))
			{
				errorMap[u"LP correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			else if (wordBeforeIsDeterminer && source.m[wordSourceIndex + 1].queryWinnerForm(u"quantifier") >= 0 && source.queryPattern(wordSourceIndex + 1, u"_TIME") != -1)
			{
				errorMap[u"LP correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			// correct except for the rare IS verb or a noun restatement (you measly scrub!)
			else if ((wordBeforeIsPronoun || source.m[wordSourceIndex - 1].queryWinnerForm(u"Proper Noun") >= 0) && source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") < 0)
			{
				errorMap[u"LP correct: LP says adverb but ST says adjective"]++;
				return 0;
			}
			if (source.queryPattern(wordSourceIndex, u"__ADVERB") != -1 && source.queryPattern(wordSourceIndex, u"__CLOSING__S1") != -1)
			{
				enum ADVCL {ALWAYS_ADJECTIVE, ALWAYS_ADVERB, UNKNOWN};
				map <lpwstring,ADVCL> closingmap = { 
					{u"above",ALWAYS_ADJECTIVE },
					{u"asleep",ALWAYS_ADJECTIVE },
					{u"enough",ALWAYS_ADVERB },
					{u"much",ALWAYS_ADVERB },
					{u"pretty",ALWAYS_ADJECTIVE },
					{u"right",ALWAYS_ADJECTIVE },
					{u"more",ALWAYS_ADJECTIVE },
					{u"less",ALWAYS_ADJECTIVE }
				};
				auto cm = closingmap.find(word);
				if (cm != closingmap.end())
				{
					if (cm->second == ALWAYS_ADJECTIVE)
						errorMap[u"ST correct: LP says adverb but ST says adjective"]++;
					else
						errorMap[u"LP correct: LP says adverb but ST says adjective"]++;
					return 0;
				}
			}
			if (word == u"much")
			{
				// much money / much Nature / much of the road / how much he had gotten / he isn't much.
				if (source.m[wordSourceIndex + 1].queryWinnerForm(u"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0 ||
					source.m[wordSourceIndex + 1].word->first == u"of" || 
					source.m[wordSourceIndex - 1].queryWinnerForm(u"is") >= 0)
				{
					errorMap[u"ST correct 'much': LP says adverb but ST says adjective"]++;
					return 0;
				}
				else
				{
					errorMap[u"LP correct 'much': LP says adverb but ST says adjective"]++; // this includes the 'how much' case - may investigate this as how much does not act like adverb nor adjective
					return 0;
				}
			}
			else if (word == u"enough")
			{
				// much money / much Nature / much of the road / how much he had gotten / he isn't much.
				if (source.m[wordSourceIndex + 1].queryWinnerForm(u"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"noun") >= 0 ||
					source.m[wordSourceIndex + 1].queryWinnerForm(u"indefinite_pronoun") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"indefinite_pronoun") >= 0 ||
					source.m[wordSourceIndex + 1].word->first == u"of" ||
					source.m[wordSourceIndex - 1].queryWinnerForm(u"is") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"is_negation") >= 0 ||
					source.m[wordSourceIndex - 2].queryWinnerForm(u"is") >= 0 || source.m[wordSourceIndex - 2].queryWinnerForm(u"is_negation") >= 0)
				{
					errorMap[u"ST correct 'enough': LP says adverb but ST says adjective"]++;
					return 0;
				}
				else
				{
					errorMap[u"LP correct 'enough': LP says adverb but ST says adjective"]++; // this includes the 'how much' case - may investigate this as how much does not act like adverb nor adjective
					return 0;
				}
			}
			errorMap[u"LP correct: LP says adverb but ST says adjective"]++; // probabilistic ST correct 421 out of 1090 total (+ 7 temporal expressions not included)
			return 0;
		}
		int maxlen = -1;
		if ((source.queryPattern(wordSourceIndex - 1, u"_BE", maxlen) != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(u"is") >= 0) &&
			source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") < 0 && source.m[wordSourceIndex + 1].queryWinnerForm(u"verb") < 0)
		{
			errorMap[u"LP correct: ST says adverb but LP says adjective, following a being verb"]++;
			return 0;
		}
		else if (source.m[wordSourceIndex + 1].word->first == u"-")
		{
			if (source.m[wordSourceIndex + 2].queryWinnerForm(u"noun") >= 0 && source.queryPattern(wordSourceIndex + 2, u"__ADJECTIVE") == -1)
			{
				errorMap[u"ST correct: ST says adjective but LP says adverb with dash and then a noun"]++;
				return 0;
			}
			else
			{
				errorMap[u"LP correct: ST says adjective but LP says adverb with dash and then a non-noun"]++;
				return 0;
			}
		}
		if (source.queryPattern(wordSourceIndex, u"_TIME") != -1)
		{
			errorMap[u"LP correct: ST says adjective but LP says adverb in _TIME structure"]++;
			return 0;
		}
	}
	if (word == u"more")
	{
		// more money / more Nature / more of the road / how much more he had gotten / he isn't more wealthy.
		if (source.m[wordSourceIndex + 1].queryWinnerForm(u"Proper Noun") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0 ||
			source.m[wordSourceIndex + 1].queryWinnerForm(u"indefinite_pronoun") >= 0 ||
			(source.m[wordSourceIndex + 1].word->first == u"of" && source.m[wordSourceIndex - 1].word->first != u"no"))
		{
			if (source.m[wordSourceIndex ].queryWinnerForm(u"quantifier") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0)
				errorMap[u"LP correct 'more': LP says quantifier/adjective but ST says "+ primarySTLPMatch]++;
			else
				errorMap[u"ST correct 'more': ST says "+ primarySTLPMatch]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"verb") >= 0) && source.m[wordSourceIndex].queryWinnerForm(u"quantifier") >= 0 && primarySTLPMatch == u"adverb")
		{
			errorMap[u"ST correct 'more': LP says quantifier but ST says adverb"]++; // this includes the 'how much' case - may investigate this as how much does not act like adverb nor adjective
			return 0;
		}
		// no more!
		else if (source.m[wordSourceIndex - 1].word->first == u"no")
		{
			errorMap[u"diff: LP says quantifier but ST says adverb"]++; 
			return 0;
		}
		// more or less responsible is not included!
		else if ((source.m[wordSourceIndex - 1].queryWinnerForm(u"is") >= 0 || source.m[wordSourceIndex - 1].queryWinnerForm(u"is_negation") >= 0) && wordAfterIsUnmodifiable && source.m[wordSourceIndex + 1].word->first != u"or")
		{
			errorMap[u"LP correct 'more': LP says quantifier but ST says " + primarySTLPMatch]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && 
		source.m[wordSourceIndex + 1].queryWinnerForm(u"coordinator")==-1 && source.m[wordSourceIndex + 1].word->first!=u"," &&
		!cWord::isDash(source.m[wordSourceIndex + 1].word->first[0]) && !cWord::isDoubleQuote(source.m[wordSourceIndex + 1].word->first[0]) && !cWord::isSingleQuote(source.m[wordSourceIndex + 1].word->first[0]))
	{
		bool wordAfterIsVeryUnmodifiable = wordAfterIsUnmodifiable && source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb") == -1 && source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") == -1;
		int pemaOffset=source.queryPatternDiff(wordSourceIndex, u"__NOUN",u"2");
		if (wordBeforeIsDeterminer && source.m[wordSourceIndex + 1].queryForm(u"noun") == -1)
		{
			errorMap[u"LP correct: LP says noun but ST says " + primarySTLPMatch]++;
			return 0;
		}
		else if (source.m[wordSourceIndex - 1].queryWinnerForm(u"preposition") != -1 && wordAfterIsVeryUnmodifiable && source.m[wordSourceIndex - 1].word->first != u"than" && source.m[wordSourceIndex - 1].word->first != u"as")
		{
			errorMap[u"LP correct: LP says noun but ST says " + primarySTLPMatch]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex - 1].queryWinnerForm(u"is") != -1 || source.m[wordSourceIndex - 1].queryWinnerForm(u"is_negation") != -1) && wordAfterIsVeryUnmodifiable &&
			(source.m[wordSourceIndex].word->second.inflectionFlags&PLURAL) != PLURAL)
		{
			errorMap[u"ST correct: LP says noun but ST says adjective (after is, before unmodifiable)"]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex - 2].queryWinnerForm(u"is") != -1 || source.m[wordSourceIndex - 2].queryWinnerForm(u"is_negation") != -1) && wordAfterIsVeryUnmodifiable && source.m[wordSourceIndex - 1].queryWinnerForm(u"adverb") != -1 &&
			(source.m[wordSourceIndex].word->second.inflectionFlags&PLURAL) != PLURAL)
		{
			errorMap[u"ST correct: LP says noun but ST says adjective (after is, before unmodifiable)"]++;
			return 0;
		}
		else if ((source.m[wordSourceIndex].word->second.inflectionFlags&(SINGULAR|PLURAL)) == PLURAL)
		{
			errorMap[u"LP correct: LP says noun but ST says adjective (plural only)"]++;
			return 0;
		}
	}
	// over 100 examples checked and 99% correct except for 'only'
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && word.length() > 3 && word.substr(word.length() - 2) == u"ly" && word != u"only")
	{
		errorMap[u"LP correct: adverb of customary form (ending in -ly) ST says " + primarySTLPMatch + u" but LP says adverb"]++;
		return 0;
	}
	/*
	if (word == u"only")
	{
		if (source.m[wordSourceIndex+1].pma.queryPattern(u"__S1") != -1)
			partofspeech += u"**ONLYCONJUNCTION";
		else if (source.m[wordSourceIndex + 1].pma.queryPattern(u"__INFP") != -1)
			partofspeech += u"**ONLYADVERB";
		else if (source.m[wordSourceIndex + 1].queryWinnerForm(determinerForm) != -1)
			partofspeech += u"**ONLYADJECTIVE";
	}
	*/
	// POS JJ (adjective) not found in winnerForms verb for word annoyed 0006301:[Miss Farrar now was more than bored , she was *annoyed* . ]
	// in the future may attempt to correct ishas constuction which is actually ownership
	int maxEnd = -1;
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && source.queryPattern(wordSourceIndex, u"_VERBPASSIVE", maxEnd) != -1)
	{
		errorMap[u"ST correct: ST says " + primarySTLPMatch + u" but LP says a passive construction (may be classified as diff in future)"]++;
		return 0;
	}
	maxEnd = -1;
	if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0 && source.queryPattern(wordSourceIndex, u"__AS_AS", maxEnd) != -1)
	{
		errorMap[u"diff: ST says " + primarySTLPMatch + u" but LP says adjective embedded in an adverbial construction"]++;
		return 0;
	}
	// Stanford POS JJ***SPadjective(adjective) not found in winnerForms verb for word bellowing
	maxEnd = -1;
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && source.queryPattern(wordSourceIndex, u"__ADJECTIVE", maxEnd) != -1)
	{
		errorMap[u"diff: ST says " + primarySTLPMatch + u" but LP says verb embedded in an adjectival construction"]++;
		return 0;
	}
	if ((primarySTLPMatch == u"noun" || primarySTLPMatch == u"verb") && source.m[wordSourceIndex].queryWinnerForm(u"honorific") >= 0)
	{
		if (primarySTLPMatch == u"noun")
			errorMap[u"diff: ST says noun but LP says honorific"]++;
		if (primarySTLPMatch == u"verb")
			errorMap[u"LP correct: ST says verb but LP says honorific"]++;
		return 0;
	}
	if (word == u"his" && primarySTLPMatch == u"possessive_determiner" && source.m[wordSourceIndex].queryWinnerForm(u"possessive_pronoun") >= 0 &&
		(source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) >= 0 || !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) ||
			source.m[wordSourceIndex + 1].queryWinnerForm(conjunctionForm) >= 0 || source.m[wordSourceIndex + 1].word->second.hasVerbForm()))
	{
		errorMap[u"LP correct: word 'his': ST says " + primarySTLPMatch + u" but LP says possessive_pronoun"]++;
		return 0;
	}
	if (word == u"plenty" && primarySTLPMatch == u"adverb" && (source.m[wordSourceIndex].queryWinnerForm(u"quantifier") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0) &&
		source.m[wordSourceIndex + 1].word->first == u"of")
	{
		errorMap[u"LP correct: word 'plenty': ST says " + primarySTLPMatch + u" but LP says quantifier"]++;
		return 0;
	}
	if (word == u"little" && source.m[wordSourceIndex - 1].word->first == u"a")
	{
		if (source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
			errorMap[u"LP correct: word 'a little': ST says " + primarySTLPMatch + u" but LP says adverb"]++;
		else
			errorMap[u"diff: word 'a little': ST says " + primarySTLPMatch + u" and LP matches structural adverb"]++;
		return 0;
	}
	// 55. this is correct 100% of the time 
	if (source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE && source.m[wordSourceIndex].pma.queryPattern(u"_ADJECTIVE") != -1)
	{
		errorMap[u"diff: ST says adjective when LP says it is a present participle, matched to __ADJECTIVE pattern [acceptable]"]++;
		return 0; // ST and LP agree
	}
	// 56. incorrect 3 times out of 141 instances
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE && source.m[wordSourceIndex].pma.queryPattern(u"__N1") != -1)
	{
		errorMap[u"diff: ST says verb when LP says it is a noun but matching to a present participle and an __N1 pattern [acceptable]"]++;
		return 0; // ST and LP agree
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0)
	{
		if ((source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE && source.m[wordSourceIndex].pma.queryPattern(u"__N1") != -1)
		{
			errorMap[u"diff: ST says noun when LP says it is a verb but matching to a present participle and an __N1 pattern [acceptable]"]++;
			return 0; // ST and LP agree
		}
		if (wordSourceIndex >= 1 && source.m[wordSourceIndex - 1].word->first == u"to")
		{
			errorMap[u"LP correct: ST says noun when LP says it is a verb but before 'to'"]++;
			return 0;
		}
	}
	// Rollo met the policeman *walking* towards him
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE &&
		source.m[wordSourceIndex].pma.queryPattern(u"_VERBONGOING") != -1 &&
		(source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"F") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(u"__NOUN", u"D") != -1 || source.m[wordSourceIndex].pma.queryPatternDiff(u"_PP", u"3") != -1))
	{
		errorMap[u"diff: ST says noun when LP says it is a verb but matching to a present participle and an _NOUN[F], _NOUN[D] or _PP[3] pattern [acceptable]"]++;
		return 0; // ST and LP agree
	}
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE)
	{
		int maxLen = -1, pemaIndex;
		// It will be *surprising*
		if ((pemaIndex = source.queryPattern(wordSourceIndex, u"_VERB", maxLen)) != -1)
		{
			int verbBegin = source.pema[pemaIndex].begin + wordSourceIndex;
			// check for an 'is' or 'has' verb
			for (int wsi = wordSourceIndex - 1; wsi >= verbBegin; wsi--)
			{
				if (source.m[wsi].pma.queryPattern(u"_IS") != -1 || source.m[wsi].pma.queryPattern(u"_HAVE") != -1 || source.m[wsi].pma.queryPattern(u"_BE") != -1)
				{
					errorMap[u"ST correct: present participle after 'is' or 'has' ST says adjective LP says verb"]++;
					return 0;
				}
			}
		}
	}
	// checked with 100 examples and 1 was incorrect (misparse)
	if ((primarySTLPMatch == u"noun" || primarySTLPMatch == u"adjective") && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0 && word.length() > 3 && word.substr(word.length() - 3) == u"ing")
	{
		if (primarySTLPMatch == u"noun" && (source.queryPattern(wordSourceIndex, u"__INFP") != -1 ||
			source.queryPatternDiff(wordSourceIndex, u"_VERB", u"4") != -1 ||
			source.queryPatternDiff(wordSourceIndex, u"_VERB", u"8") != -1 ||
			source.queryPattern(wordSourceIndex, u"_ADJECTIVE_AFTER") != -1 ||
			source.queryPatternDiff(wordSourceIndex, u"__ADJECTIVE", u"2") != -1 ||
			(source.queryPattern(wordSourceIndex, u"_VERBONGOING") != -1 && source.queryPattern(wordSourceIndex, u"__MODAUX") != -1) ||
			(source.queryPattern(wordSourceIndex, u"_VERBONGOING") != -1 && source.queryPatternDiff(wordSourceIndex, u"_VERBREL2", u"1") != -1)))
		{
			errorMap[u"LP correct: ST says noun when LP says verb participle in a structure consonant with a verb"]++;
			return 0;
		}
		//int w;
		if (primarySTLPMatch == u"adjective" && (
			source.queryPattern(wordSourceIndex, u"__INFP") != -1 ||
			source.queryPatternDiff(wordSourceIndex, u"_VERB", u"4") != -1 ||
			source.queryPatternDiff(wordSourceIndex, u"_VERB", u"8") != -1 ||
			source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"D") != -1 ||
			//(w=source.queryPattern(wordSourceIndex, u"__NOUN", u"2")) != -1 && source.pema[w].end==1) ||
			(source.queryPattern(wordSourceIndex, u"_VERBONGOING") != -1 && source.queryPattern(wordSourceIndex, u"__MODAUX") != -1) ||
			(source.queryPattern(wordSourceIndex, u"_VERBONGOING") != -1 && source.queryPatternDiff(wordSourceIndex, u"_VERBREL2", u"1") != -1)))
		{
			errorMap[u"LP correct: ST says adjective when LP says verb participle in a structure consonant with a verb"]++;
			return 0;
		}
		if (primarySTLPMatch == u"noun" && source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"D") != -1)
		{
			errorMap[u"diff: ST says noun and LP says verb in a noun structure"]++;
			return 0;
		}
		if (primarySTLPMatch == u"adjective" && (source.queryPattern(wordSourceIndex, u"_ADJECTIVE_AFTER") != -1 ||
			source.queryPatternDiff(wordSourceIndex, u"__ADJECTIVE", u"2") != -1))
		{
			errorMap[u"diff: ST says adjective and LP says verb in a adjective structure"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0 &&
		source.m[wordSourceIndex].queryForm(u"verb") >= 0 && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PAST) == VERB_PAST &&
		source.queryPatternDiff(wordSourceIndex, u"__S1", u"7") != -1)
	{
		errorMap[u"ST correct: ST says verb and LP says adjective"]++;
		return 0;
	}
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0)
	{
		lpwstring nounCost, verbCost;
		itos(source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)), nounCost);
		itos(source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)), verbCost);
		// 115 out of 116 correct
		if (nounCost == u"0" && (verbCost == u"4" || verbCost == u"3" || verbCost == u"2") && partofspeech == u"VBN")
		{
			errorMap[u"LP correct: ST says verb VBN and LP says noun"]++;
			return 0;
		}
		// all of 183 examples
		if (nounCost == u"0" && (verbCost == u"4" || verbCost == u"3" || verbCost == u"2") && source.m[wordSourceIndex - 1].word->first == u"-") // not double dash!
		{
			if (word.length() > 3 && word.substr(word.length() - 3) == u"ing" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE &&
				source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0)
			{
				errorMap[u"diff: ST says verb and LP says noun - after -, and using ing (really adjective)"]++;
			}
			else
				errorMap[u"LP correct: ST says verb and LP says noun - after -"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"verb") >= 0)
		{
		lpwstring verbCost;
		itos(source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)), verbCost);
		int pemaOffset = source.queryPattern(wordSourceIndex, u"__NOUN");
		if (source.m[wordSourceIndex].isOnlyWinner(verbForm) && verbCost==u"0")
		{
			// letting her hands *fall*
			if (pemaOffset >= 0 && source.pema[pemaOffset].end == 1 && patterns[source.pema[pemaOffset].getParentPattern()]->differentiator==u"6")
			{
				errorMap[u"diff: LP says verb in head part of NOUN struct and ST says noun"]++;
				return 0;
			}
			// 3 out of 108 incorrect because the parse was wrong
			// for a *split* second .
			if (pemaOffset >= 0 && source.pema[pemaOffset].end > 1)
			{
				errorMap[u"LP correct: LP says verb in adjective part of NOUN struct and ST says noun"]++;
				return 0;
			}
		}
	}
	// this is correct exceot for rare parse structure: I would have done it *again* here had I thought you were coming to try to win her heart
	if (source.m[wordSourceIndex].queryWinnerForm(u"conjunction") >= 0 && source.m[wordSourceIndex + 1].pma.queryPattern(u"__S1") != -1)
	{
		errorMap[u"LP correct: ST says " + primarySTLPMatch + u" and LP says conjunction"]++;
		return 0;
	}
	if ((word == u"upstairs" || word == u"downstairs") && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && primarySTLPMatch == u"noun")
	{
		errorMap[u"LP correct: word 'upstairs' or 'downstairs': ST says noun LP says adverb"]++;
		return 0;
	}
	if (word == u"yer" || word == u"youse" || word == u"em" || word == u"ourselves")
	{
		errorMap[u"LP correct '"+word+u"': incorrect noun usage"]++;
		return 0;
	}
	if (partofspeech == u"VBG" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) != VERB_PRESENT_PARTICIPLE)
	{
		//CStanford POS VBG 52
		//WStanford POS VBG 9
	}
	if (partofspeech == u"VBD" && (source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PAST) != VERB_PAST)
	{
			//CStanford POS VBD 17
			//WStanford POS VBD 4
	}
	if (partofspeech == u"NN" && (source.m[wordSourceIndex].word->second.inflectionFlags&SINGULAR) != SINGULAR)
	{
		if (source.m[wordSourceIndex].queryWinnerForm(u"interjection") >= 0 && primarySTLPMatch == u"noun")
		{
			errorMap[u"LP correct: ST says noun LP says interjection"]++;
			return 0;
		}
	}
	if (partofspeech == u"NNS" && (source.m[wordSourceIndex].word->second.inflectionFlags&PLURAL) != PLURAL)
	{
		if (word == u"semi" || word == u"but" || word == u"oh" || word == u"hey" || word == u"yes" || source.m[wordSourceIndex].queryWinnerForm(u"reflexive_pronoun") >= 0)
		{
			errorMap[u"LP correct: incorrect noun usage"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && (word==u"half" || word==u"round"))
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective") >= 0 || source.m[wordSourceIndex + 1].queryWinnerForm(u"verb") >= 0 ||
			(source.m[wordSourceIndex + 1].queryForm(dashForm) != -1 && source.m[wordSourceIndex + 2].queryWinnerForm(u"adjective") >= 0))
		{
			errorMap[u"LP correct: adverb not noun"]++;
			return 0;
		}
	}
	if ((source.m[wordSourceIndex].flags&cWordMatch::flagFirstLetterCapitalized) && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]) && source.m[wordSourceIndex].queryWinnerForm(u"interjection") >= 0)
	{
		errorMap[u"LP correct: interjection not "+ primarySTLPMatch]++;
		return 0;
	}
	if ((source.queryPatternDiff(wordSourceIndex,u"__INTRO_N", u"C") != -1 || source.queryPatternDiff(wordSourceIndex,u"_ADVERB", u"T") != -1) && 
		  (source.m[wordSourceIndex].queryWinnerForm(u"dayUnit") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"timeUnit") >= 0 || source.m[wordSourceIndex].queryWinnerForm(u"uncertainDurationUnit") >= 0))
	{
		if (primarySTLPMatch ==u"adverb")
			errorMap[u"diff: TIME (adverb)"]++;
		else
			errorMap[u"LP correct: adverb not " + primarySTLPMatch]++;
		return 0;
	}
	if (word == u"on board")
	{
		errorMap[u"diff: on board (adverb)"]++;
		return 0;
	}
	if ((word == u"to-day" || word == u"to-morrow") && (primarySTLPMatch == u"noun" || primarySTLPMatch == u"adjective") && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0)
	{
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(u"preposition") >= 0 && primarySTLPMatch == u"noun"))
			errorMap[u"ST correct: 'to-day' or 'to-morrow' after preposition must be a noun"]++;
		else if ((source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") >= 0 && primarySTLPMatch == u"adjective"))
			errorMap[u"ST correct: 'to-day' or 'to-morrow' before noun must be an adjective"]++;
		else
		{
			errorMap[u"LP correct: 'to-day' or 'to-morrow' is in general an adverb of time"]++;
		}
		return 0;
	}
	// 102 examples checked, 101 correct.
	if (word==u"after" && primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0)
	{
		errorMap[u"LP correct: 'after' can be an adjective"]++;
		return 0;
	}
	if (word==u"doubt")
	{
		if (source.queryPatternDiff(wordSourceIndex, u"__INTRO_N", u"ID") != -1)
		{
			errorMap[u"LP correct: 'doubt' is a verb in 'I doubt if'"]++;
			return 0;
		}
		if (source.queryPatternDiff(wordSourceIndex, u"__ADVERB", u"ND") != -1)
		{
			errorMap[u"LP correct: 'doubt' is a noun in 'no doubt'"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].isOnlyWinner(nounForm) &&
		  source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(u"adjective")) == 4 &&
		  source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(u"noun")) == 0)
	{
		errorMap[u"LP correct: noun more probable than adjective"]++; // probabilistic - see distribute errors (19 out of 119 LP correct)
		return 0;
	}
	if (word == u"my" && source.m[wordSourceIndex].queryWinnerForm(u"possessive_determiner") >= 0)
	{
		errorMap[u"LP correct: 'my' is possessive_determiner (now that interjection has been added as a form)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].queryWinnerForm(u"adjective") >= 0)
	{
		errorMap[u"LP correct: adjective not verb"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(adverbForm) && source.m[wordSourceIndex-1].queryWinnerForm(u"preposition") < 0)
	{
		errorMap[u"LP correct: adverb not noun"]++; // probabilistic - see distribute errors
		return 0;
	}
	int pemaOffset = -1;
	if (source.queryPatternDiff(wordSourceIndex, u"_ADVERB", u"8") != -1)
	{
		errorMap[u"LP correct:little by little"]++; 
		return 0;
	}
	// *time* to time OR time to *time* / *face* to face
	if (source.m[wordSourceIndex].queryWinnerForm(u"noun") >= 0 && ((wordSourceIndex > 2 && source.m[wordSourceIndex - 1].word->first == u"to" && source.m[wordSourceIndex].word == source.m[wordSourceIndex - 2].word) ||
		(wordSourceIndex < source.m.size() - 1 && source.m[wordSourceIndex + 1].word->first == u"to" && source.m[wordSourceIndex].word == source.m[wordSourceIndex + 2].word)))
	{
		errorMap[u"LP correct:little by little"]++;
		return 0;
	}
	// from *head* to foot OR from head to *foot*
	if ((word == u"foot" && wordSourceIndex>3 && source.m[wordSourceIndex - 3].word->first == u"from" && source.m[wordSourceIndex - 2].word->first == u"head" &&source.m[wordSourceIndex - 1].word->first == u"to") ||
		  (word == u"head" && wordSourceIndex > 1 && source.m[wordSourceIndex - 1].word->first == u"from" && source.m[wordSourceIndex + 1].word->first == u"to" &&source.m[wordSourceIndex + 2].word->first == u"foot"))
	{
		errorMap[u"LP correct:from head to foot"]++;
		return 0;
	}

	if (word == u"hers" && primarySTLPMatch==u"noun" && source.m[wordSourceIndex].queryWinnerForm(u"pronoun") >= 0)
	{
		errorMap[u"LP correct:hers is better considered a pronoun/possessive, not a noun"]++;
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 4 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(adjectiveForm)) == 0)
	{
		errorMap[u"LP correct: (noun cost 4, adjective cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].isOnlyWinner(nounForm) &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 4 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 0)
	{
		errorMap[u"LP correct: (verb cost 4, noun cost 0 1SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].isOnlyWinner(nounForm) &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_THIRD_SINGULAR) == VERB_PRESENT_THIRD_SINGULAR &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 4 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 0)
	{
		errorMap[u"LP correct: (verb cost 4, noun cost 0 3SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(verbForm) && 
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 4 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)) == 0)
	{
		if ((source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR)
			errorMap[u"LP correct: (noun cost 4, verb cost 0 1SING)"]++; // probabilistic - see distribute errors
		else
			errorMap[u"LP correct: (noun cost 4, verb cost 0 REST OF TENSE)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(verbForm) &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 2 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)) == 0)
	{
		errorMap[u"LP correct: (noun cost 2, verb cost 0 1SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(verbForm) &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 0 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)) == 4)
	{
		errorMap[u"LP correct: (noun cost 0, verb cost 4 1SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(verbForm) &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 0 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)) == 3)
	{
		errorMap[u"LP correct: (noun cost 0, verb cost 3 1SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 4 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(adjectiveForm)) == 0)
	{
		errorMap[u"LP correct: (adverb cost 4, adjective cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 3 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(adjectiveForm)) == 0)
	{
		errorMap[u"LP correct: (adverb cost 3, adjective cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if ((word == u"in" || word == u"since" || word==u"beyond") && primarySTLPMatch == u"preposition or conjunction" && (!iswalpha(source.m[wordSourceIndex + 1].word->first[0]) || source.m[wordSourceIndex+1].isOnlyWinner(coordinatorForm)))
	{
		errorMap[u"LP correct: not preposition or conjunction before punctuation or coordinator"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (word == u"in" && primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].isOnlyWinner(adverbForm) &&
		(source.m[wordSourceIndex + 1].word->first == u"there" || source.m[wordSourceIndex + 1].word->first == u"silence"))
	{
		errorMap[u"ST correct: in is a preposition before 'silence' or 'there'"]++;
		return 0;
	}
	if (word == u"home" && primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].isOnlyWinner(nounForm))
	{
		errorMap[u"ST correct: home is an adverb when having no determiner (go home)"]++;
		return 0;
	}
	// all 32 examples are correct.
	if (word == u"fool" && source.m[wordSourceIndex].isOnlyWinner(nounForm))
	{
		errorMap[u"ST correct: fool is a noun (you fool!)"]++;
		return 0;
	}
	// 1 out of 109 examples was wrong (LP did not select the correct form)
	if (wordSourceIndex>0 && source.m[wordSourceIndex-1].word->first==u"wouldhad")
	{
		errorMap[u"diff: wouldhad is an LP construction, so Stanford will not do this correctly."]++;
		return 0;
	}
	// (noun) not found in winnerForms is for word ishas
	if (word == u"ishas" || word == u"wouldhad" || word == u"ishasdoes")
	{
		errorMap[u"diff: ishas/wouldhad/ishasdoes is a special word."]++;
		return 0;
	}
	if (primarySTLPMatch != u"preposition or conjunction" && source.m[wordSourceIndex].isOnlyWinner(prepositionForm) && source.m[wordSourceIndex].getRelObject()>=0)
	{
		if (source.m[wordSourceIndex + 1].word->first == u"to")
		{
			errorMap[u"LP correct: preposition preposition (to)"]++; 
			return 0;
		}
		if (source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) >= 0)
		{
			errorMap[u"ST correct: preposition preposition (other than to)"]++;
			return 0;
		}
		if (wordSourceIndex > 0 && (cWord::isDash(source.m[wordSourceIndex - 1].word->first[0]) || cWord::isDash(source.m[wordSourceIndex + 1].word->first[0])))
		{
			errorMap[u"ST correct: dash preposition"]++;
			return 0;
		}
		// 22 ST/163 LP
		errorMap[u"LP correct: preposition with relative object"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (source.m[wordSourceIndex].isOnlyWinner(prepositionForm) && source.m[wordSourceIndex].getRelObject() < 0)
	{
		if (source.m[wordSourceIndex - 1].queryWinnerForm(prepositionForm) >= 0 && source.m[wordSourceIndex - 1].getRelObject() >= 0 && word != u"as" && primarySTLPMatch != u"verb" && primarySTLPMatch != u"noun")
		{
			errorMap[u"ST correct: about is an adverb when used with this construction"]++; 
			return 0;
		}
	}
	// 'all' before an _S1 is an adverb not a determiner
	// u"relativizer|when", u"conjunction|before", u"conjunction|after", u"conjunction|as", u"conjunction|since", u"conjunction|until", u"conjunction|while", u"__AS_AS", u"quantifier|all*-1"
	if (source.m[wordSourceIndex].pma.queryPatternDiff(u"_ADVERB",u"AT8") != -1 && source.m[wordSourceIndex+1].pma.queryPattern(u"__S1")!=-1)
	{
		errorMap[u"LP correct: word:"+word+u" ST says "+ primarySTLPMatch+u", LP says AT8 match"]++;
		return 0;
	}
	// |noun*3 verb*0 1stSING|
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(verbForm) &&
		  source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 3 &&
			source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm))==0 &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR)
	{
		bool dashed = (wordSourceIndex > 0 && wordSourceIndex < source.m.size() - 1 &&
			(cWord::isDash(source.m[wordSourceIndex - 1].word->first[0]) || cWord::isDash(source.m[wordSourceIndex + 1].word->first[0])));
		bool previousWords = (source.m[wordSourceIndex - 2].word->first == u"a" || 
			source.m[wordSourceIndex - 1].word->first == u"that" || source.m[wordSourceIndex - 1].word->first == u"this");
		if (dashed || previousWords)
		{
			errorMap[u"ST correct: ST says noun and LP says verb (determined by dash or previous word)"]++;
			return 0;
		}
		// 73 ST/144 LP
		errorMap[u"LP correct: (noun cost 3, verb cost 0 1SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	// after *dark*
	if (source.m[wordSourceIndex].queryWinnerForm(u"dayUnit") >= 0 && source.m[wordSourceIndex - 1].queryWinnerForm(prepositionForm) >= 0)
	{
		errorMap[u"LP correct: dayUnit after preposition is correct"]++;
		return 0;
	}
	// to and fro
	if (word == u"fro")
	{
		errorMap[u"LP correct: if to is a preposition, fro must also be"]++;
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(verbForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 0 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(verbForm)) == 0 &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR)
	{
		errorMap[u"LP correct: (noun cost 0, verb cost 0 1SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 3 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(adjectiveForm)) == 0)
	{
		errorMap[u"LP correct: (noun cost 3, adjective cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 2 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(adjectiveForm)) == 0)
	{
		errorMap[u"LP correct: (noun cost 2, adjective cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	// this is for all tenses!
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].isOnlyWinner(nounForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 3 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 0)
	{
		errorMap[u"LP correct: (verb cost 3, noun cost 0)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].isOnlyWinner(nounForm) &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(primarySTLPMatch)) == 2 &&
		source.m[wordSourceIndex].word->second.getUsageCost(source.m[wordSourceIndex].queryForm(nounForm)) == 0 &&
		(source.m[wordSourceIndex].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) == VERB_PRESENT_FIRST_SINGULAR)
	{
		errorMap[u"LP correct: (verb cost 2, noun cost 0 1SING)"]++; // probabilistic - see distribute errors
		return 0;
	}
	if (wordSourceIndex > 0 && (source.m[wordSourceIndex - 1].flags&cWordMatch::flagNounOwner) && source.m[wordSourceIndex].isOnlyWinner(nounForm))
	{
		int pemaOffset = source.queryPattern(wordSourceIndex,u"__NOUN");
		if (pemaOffset >= 0 && source.pema[pemaOffset].end > 1 && primarySTLPMatch==u"verb")
		{
			errorMap[u"LP correct: ownership (verb infinitive)"]++; 
			return 0;
		}
		else
		{
			errorMap[u"LP correct: ownership of noun"]++; // probabilistic - see distribute errors ST=12 / LP=79
			return 0;
		}
	}
	if (word == u"that" && source.m[wordSourceIndex].queryWinnerForm(pronounForm) >= 0 && 
		(((source.m[wordSourceIndex].flags&cWordMatch::flagInQuestion) && wordSourceIndex > 0 && (source.m[wordSourceIndex - 1].queryForm(u"is") >= 0 || source.m[wordSourceIndex - 1].queryForm(u"is_negation") >= 0)) ||
		 (!(source.m[wordSourceIndex].flags&cWordMatch::flagInQuestion) && wordSourceIndex > 0 && (source.m[wordSourceIndex + 1].queryForm(u"is") >= 0 || source.m[wordSourceIndex + 1].queryForm(u"is_negation") >= 0) &&
			(!iswalpha(source.m[wordSourceIndex - 1].word->first[0]) || wordSourceIndex == startOfSentence)) ||
				!iswalpha(source.m[wordSourceIndex + 1].word->first[0])))
	{
		errorMap[u"LP correct: that after 'is' in a question OR before is not in a question is a pronoun OR before punctuation!"]++; // what is that? / “ Billy , *that* is exactly where you are wrong / “ It isn't *that* , ” she said .
		return 0;
	}
	if (word == u"that" && source.m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0 && 
		  wordSourceIndex<source.m.size()-2 && source.m[wordSourceIndex+1].queryWinnerForm(adjectiveForm) >= 0 && 
		source.m[wordSourceIndex].principalWherePosition==wordSourceIndex && // this is not acting as a determiner
		(source.m[wordSourceIndex + 2].word->first==u"." || source.m[wordSourceIndex + 2].word->first == u"," || source.m[wordSourceIndex + 2].queryWinnerForm(prepositionForm)>=0))
	{
		errorMap[u"ST correct: that before an adjective, not in an object and having passed all other tests (doesn't work for a rule)"]++; 
		return 0;
	}
	// never mind
	if (word == u"mind" && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0 && source.queryPatternDiff(wordSourceIndex, u"_COMMAND1", u"8") != -1)
	{
		errorMap[u"LP correct: never mind!"]++;
		return 0;
	}
	// their coming
	if ((word == u"coming" || word == u"being") && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0 && source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"COMING") != -1)
	{
		errorMap[u"LP correct: my coming!"]++;
		return 0;
	}
	// neither/either/or/nor
	if ((word == u"neither" || word == u"either" || word == u"or"  || word == u"nor" ) && 
		  (source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"O") != -1 || source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"P") != -1 || source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"7") != -1 || source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"A") != -1))
	{
		errorMap[u"LP correct: either/neither/or/nor!"]++;
		return 0;
	}
	if (word == u"kind" && source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0)
	{
		errorMap[u"ST correct: word 'kind' ST says adjective"]++;
		return 0;
	}
	if (word == u"kind" && source.m[wordSourceIndex].queryWinnerForm(adjectiveForm) >= 0)
	{
		errorMap[u"LP correct: word 'kind' ST says adjective"]++; // C 85 W 17
		return 0;
	}
	if (source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0)
	{
		// not foot by foot.  Also 'by' must have an object (Not 'he was close by')
		if (source.m[wordSourceIndex + 1].word->first == u"by" && source.m[wordSourceIndex + 2].word->first!= source.m[wordSourceIndex].word->first && source.m[wordSourceIndex + 1].getRelObject()>=0)
		{
			errorMap[u"LP correct: verb is passive not adjective"]++; 
			return 0;
		}
	}
	// if ST says adjective and we say verb...
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0 &&
		// and the verb is past or present participle
			(source.m[wordSourceIndex].word->second.inflectionFlags&(VERB_PAST | VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE)) && 
		// and it has a subject and an object
		  source.m[wordSourceIndex].getRelObject() >= 0 && source.m[wordSourceIndex].relSubject >= 0 &&
		// and the verb is supposed to have objects
			source.m[wordSourceIndex].word->second.getUsageCost(cSourceWordInfo::VERB_HAS_1_OBJECTS) < 4 && 
		// and the immediately preceding word is not a dash or that or her
			!cWord::isDash(source.m[wordSourceIndex - 1].word->first[0]) && source.m[wordSourceIndex - 1].word->first != u"that" && source.m[wordSourceIndex - 1].word->first != u"her")
	{
		errorMap[u"LP correct: ST says adjective, LP says verb(PAST/PRESENT_PARTICIPLE)"]++; // C 88 W 27
		return 0;
	}
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryForm(adjectiveForm) < 0)
	{
		if (source.m[wordSourceIndex].queryWinnerForm(verbForm) < 0 || !(source.m[wordSourceIndex].word->second.inflectionFlags&(VERB_PAST | VERB_PRESENT_PARTICIPLE)))
		{
			errorMap[u"LP correct: ST says adjective (wrong)"]++; // C 299 W 30
			return 0;
		}
		// VERB_PAST after be?
		if (source.m[wordSourceIndex].isOnlyWinner(verbForm) && source.m[wordSourceIndex - 1].word->first == u"be" && (source.m[wordSourceIndex].word->second.inflectionFlags&(VERB_PAST_PARTICIPLE | VERB_PAST)) == (VERB_PAST_PARTICIPLE | VERB_PAST))
		{
			errorMap[u"LP correct: ST says adjective (wrong) when verb past participle after be"]++;
			return 0;
		}
		if (source.queryPattern(wordSourceIndex, u"_Q1PASSIVE") != -1)
		{
			errorMap[u"LP correct: passive verb is not adjective"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0 && source.m[wordSourceIndex].getRelVerb() >= 0 && (source.m[source.m[wordSourceIndex].getRelVerb()].queryForm(isForm) >= 0 || source.m[source.m[wordSourceIndex].getRelVerb()].queryForm(isNegationForm) >= 0 ||
			source.m[source.m[wordSourceIndex].getRelVerb()].word->first == u"be" || source.m[source.m[wordSourceIndex].getRelVerb()].word->first == u"been"))
		{
			errorMap[u"LP correct: ST says adjective but word does not have adjective form used with IS/BE verb"]++;
			return 0;
		}
		if (!iswalpha(source.m[wordSourceIndex + 1].word->first[0]) && source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0)
		{
			errorMap[u"LP correct: LP says noun and ST says adjective but word does not have an adjective form"]++;
			return 0;
		}
		if (source.m[wordSourceIndex].queryWinnerForm(verbverbForm) >= 0 && (source.queryPattern(wordSourceIndex, u"_VERB_BARE_INF") != -1 && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0))
		{
			errorMap[u"LP correct: helper verbs are not adjectives"]++;
			return 0;
		}
		if (source.queryPattern(wordSourceIndex, u"_VERBPAST") != -1 && source.queryPattern(wordSourceIndex, u"__S1") != -1)
		{
			int relSubject = source.m[wordSourceIndex].relSubject;
			int relObject = source.m[wordSourceIndex].getRelObject();
			if (relSubject >= 0 && relObject < 0 && source.queryPattern(relSubject, u"__INFPT") >= 0)
			{
				errorMap[u"LP correct: INFPT verb is not adjective"]++;
				return 0;
			}
			if (relObject < 0)
			{
				errorMap[u"LP correct: past of verb is not adjective"]++; // probabilistic - see distribute errors ST=21 / LP=84
				return 0;
			}
			//for (int I = 0; I < source.m.size(); I++)
			//	lplog(LOG_INFO, u"%02d:%10s:relPrep = %02d,relObject = %02d,relSubject = %02d,relVerb = %02d,relNextObject = %02d,nextCompoundPartObject = %02d,previousCompoundPartObject = %02d,relInternalVerb = %02d,relInternalObject = %02d", 
			//		I, source.m[I].word->first.c_str(),source.m[I].relPrep, source.m[I].getRelObject(), source.m[I].relSubject, source.m[I].relVerb, source.m[I].relNextObject, source.m[I].nextCompoundPartObject, source.m[I].previousCompoundPartObject, source.m[I].relInternalVerb, source.m[I].relInternalObject);
			
			//errorMap[u"LP correct: passive verb is not adjective"]++;
			//return 0;
		}
		lpwstring excludeForms[] = { u"be", u"been", u"conjunction", u"coordinator", u"daysofweek", u"dayunit", u"future_modal_auxiliary", u"have", u"honorific", u"indefinite_pronoun", u"interjection", u"interrogative_determiner", u"interrogative_pronoun", u"is", u"modal_auxiliary", u"never", u"not",u"personal_pronoun",u"personal_pronoun_accusative",u"possessive_determiner",u"possessive_pronoun",u"relativizer"};
		for (auto ef : excludeForms)
		{
			if (source.m[wordSourceIndex].queryWinnerForm(ef) >= 0)
			{
				errorMap[u"LP correct: ST says adjective, LP says"+ef]++;
				return 0;
			}
		}
		if (source.m[wordSourceIndex].queryWinnerForm(nounForm) >= 0)
		{
			errorMap[u"LP correct: ST says adjective, LP says noun"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(verbForm) >= 0)
	{
		if (word == u"bent")
		{
			errorMap[u"LP correct: word 'bent': ST says adjective (wrong)"]++; 
			return 0;
		}
		if (!(source.m[wordSourceIndex].word->second.inflectionFlags&(VERB_PAST | VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE)))
		{
			// total 288: 155 LP correct.  114 ST correct. 12 ambiguous.  6 both wrong.
			errorMap[u"LP correct: ST says adjective, LP says non-past verb (highly ambiguous)"]++;
			return 0;
		}
		else
		{
			// total 1467: 125 LP correct.  106 ST correct. 89 ambiguous.  2 both wrong.
			errorMap[u"LP correct: ST says adjective, LP says verb (highly ambiguous)"]++;
			return 0;
		}

	}
	if (word == u"more" || word == u"less")
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(adverbForm) >= 0)
		{
			errorMap[u"ST correct: more or less before adverb is an adverb, not a quantifier"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].word->first == u"than")
		{
			if (source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1)
			{
				errorMap[u"diff: quantifier is preferred but ST pick of adverb for more or less than is acceptable."]++;
				return 0;
			}
			if (source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1 && source.m[wordSourceIndex + 2].pma.queryPattern(u"_TIME") == -1)
			{
				errorMap[u"ST correct: more/less than - adjective is preferred if not followed by a time."]++;
				return 0;
			}
		}
		if (source.queryPattern(wordSourceIndex, u"_MLT") != -1 && source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
		{
			errorMap[u"LP correct: more or less is an adverb when used with expressions of time."]++;
			return 0;
		}
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(u"noun") >= 0 && source.m[wordSourceIndex + 1].queryWinnerForm(u"noun") < 0))
		{
			if ((source.m[wordSourceIndex - 1].queryForm(u"uncertainDurationUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(u"simultaneousUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(u"dayUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(u"timeUnit") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(u"season") >= 0 ||
				source.m[wordSourceIndex - 1].queryForm(u"time_abbreviation") >= 0))
			{
				if (source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
				{
					errorMap[u"LP correct: more or less is an adverb when used with expressions of time."]++;
					return 0;
				}
			}
			else if (source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1)
			{
				errorMap[u"diff: more or less is an adjective BUT Stanford pegs it as an adverb and LP as a quantifier."]++;
				return 0;
			}
			else if (primarySTLPMatch == u"adjective")
			{
				errorMap[u"ST correct: more or less is an adjective after a noun and not before a noun."]++;
				return 0;
			}
		}
		if ((source.m[wordSourceIndex - 1].queryWinnerForm(u"uncertainDurationUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(u"simultaneousUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(u"dayUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(u"timeUnit") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(u"season") >= 0 ||
			source.m[wordSourceIndex - 1].queryWinnerForm(u"time_abbreviation") >= 0))
		{
			if (source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
			{
				errorMap[u"LP correct: more or less is an adverb when used with expressions of time."]++;
				return 0;
			}
		}
	}
	if (word == u"goodbye" || word == u"good-bye")
	{
		errorMap[u"diff: goodbye/good-bye is never an adjective."]++;
		return 0;
	}
	if (word == u"anyhow")
	{
		errorMap[u"LP correct: anyhow is always an adverb."]++;
		return 0;
	}
	if (word == u"but" && source.m[wordSourceIndex].queryWinnerForm(conjunctionForm) != -1)
	{
		errorMap[u"LP correct: but is a conjunction."]++; // 159 examples
		return 0;
	}
	if (source.m[wordSourceIndex].queryWinnerForm(reflexivePronounForm) != -1)
	{
		if (primarySTLPMatch == u"noun")
		{
			errorMap[u"diff: reflexive pronoun form can be considered a noun."]++;
			return 0;
		}
		if (primarySTLPMatch == u"adjective")
		{
			errorMap[u"diff: reflexive pronoun form cannot be considered an adjective."]++;
			return 0;
		}
	}
	if (word == u"now" && ((source.m[wordSourceIndex].relPrep >= 0 && source.queryPattern(wordSourceIndex,u"_PP") != -1) || (source.m[wordSourceIndex].flags&cWordMatch::flagNounOwner)!=0))
	{
		errorMap[u"LP correct: now in a PP is a noun."]++;
		return 0;
	}
	if ((primarySTLPMatch == u"preposition or conjunction" || word==u"but") && source.m[wordSourceIndex].queryWinnerForm(u"adverb") >= 0 && wordSourceIndex < source.m.size() - 1)
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(prepositionForm) != -1)
		{
			errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].hasWinnerVerbForm() && source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) == -1)
		{
			if (word == u"as")
			{
				errorMap[u"ST correct: word 'as': ST says preposition or conjunction and LP says adverb before verb"]++;
				return 0;
			}
			// *in* - most cases, the 'object' like 'in' hand, 'in' mind, 'in' turn, 'in' answer, 'in' order, 'in' love
			errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb before verb"]++; // LP Wrong 27 / LP Correct 258  distribute errors
			return 0;
		}
		if (source.m[wordSourceIndex + 1].queryWinnerForm(nounForm) != -1 && source.m[wordSourceIndex].isOnlyWinner(adverbForm) && !cWord::isDash(source.m[wordSourceIndex - 1].word->first[0]))
		{
			errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb before noun"]++; // ST Wrong 6 / ST Correct 171  distribute errors
			return 0;
		}
		/*
		set <lpwstring> notObjects = { u"we",u"i",u"he",u"they" };
		if (wordSourceIndex < source.m.size() - 2 && source.m[wordSourceIndex + 1].hasWinnerNounForm() && source.m[wordSourceIndex].isOnlyWinner(adverbForm) &&
			source.m[wordSourceIndex].queryForm(prepositionForm) != -1 && word != u"as" && source.m[wordSourceIndex + 1].queryWinnerForm(PROPER_NOUN_FORM)==-1)
		{
			if (notObjects.find(source.m[wordSourceIndex + 1].word->first) == notObjects.end() &&
				(!iswalpha(source.m[wordSourceIndex + 2].word->first[0]) || source.m[wordSourceIndex + 2].queryWinnerForm(coordinatorForm) != -1))
			{
				partofspeech += u"**ADVERBPREP?";
				//return 0;
			}
			if (notObjects.find(source.m[wordSourceIndex + 1].word->first) != notObjects.end())
			{
				partofspeech += u"**ADVERBCONJ?";
				//return 0;
			}
		}
		*/
		if (source.m[wordSourceIndex].pma.queryPattern(u"_INFP") != -1)
		{
			errorMap[u"ST correct: ST says preposition or conjunction and LP says adverb before INFP"]++; 
			return 0;
		}
		if (atStart || source.m[wordSourceIndex + 1].word->first == u"." || source.m[wordSourceIndex + 1].word->first == u"!" || source.m[wordSourceIndex + 1].word->first == u"?" || source.m[wordSourceIndex + 1].word->first == u";")
		{
			errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb as first or last word (next word may be 'there' or 'then' which may considered adverbs of time or place)"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].word->first == u"," || source.m[wordSourceIndex + 1].queryWinnerForm(conjunctionForm) != -1 || source.m[wordSourceIndex + 1].queryWinnerForm(coordinatorForm) != -1)
		{
			if (word == u"but")
			{
				errorMap[u"ST correct: but before a conjunction"]++;
				return 0;
			}
			if (word == u"as")
			{
				errorMap[u"ST correct: as before a conjunction or a comma"]++;
				return 0;
			}
			errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb before comma or conjunction/coordinator"]++;
			return 0;
		}
		int timePEMAOffset = -1;
		if ((timePEMAOffset = source.queryPattern(wordSourceIndex, u"_TIME")) != -1)
		{
			errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb in TIME expression"]++;
			return 0;
		}
		if (wordSourceIndex != startOfSentence && wordSourceIndex > 0 && cWord::isDash(source.m[wordSourceIndex - 1].word->first[0]) && source.m[wordSourceIndex - 1].word->first.length() == 1)
		{
			errorMap[u"diff: ST says preposition or conjunction and LP says adverb after a single dash - actually an adjective"]++;
			return 0;
		}
	}
	int adjThreePatternPEMAOffset = -1;
	if (primarySTLPMatch==u"noun" && source.m[wordSourceIndex].queryWinnerForm(verbForm) != -1 && (adjThreePatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"__ADJECTIVE", u"3")) != -1 &&
		  source.pema[adjThreePatternPEMAOffset].end==1)
	{
		errorMap[u"LP correct: ST says noun and LP says adjective a single dash"]++;
		return 0;
	}
	if (word == u"all" && source.m[wordSourceIndex].queryWinnerForm(quantifierForm)!=-1)
	{
		if (source.m[wordSourceIndex - 1].queryForm(u"is")!=-1 || source.m[wordSourceIndex - 1].word->first==u"be" || source.m[wordSourceIndex - 1].word->first == u"been")
		{
			if (source.m[wordSourceIndex + 1].queryWinnerForm(u"adjective")!=-1 || source.m[wordSourceIndex + 1].queryWinnerForm(u"adverb")!=-1 || source.m[wordSourceIndex + 1].word->first == u"right")
			{
				errorMap[u"ST correct: ST says adverb and LP says quantifier before an adjective, adverb after IS"]++;
				return 0;
			}
			else if (!source.m[wordSourceIndex + 1].hasWinnerVerbForm())
			{
				errorMap[u"diff: ST says adverb and LP says quantifier after IS"]++; // all is an adjective!
				return 0;
			}
		}
		if (source.m[wordSourceIndex - 1].word->first == u"at" && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))
		{
			errorMap[u"LP correct: ST says adverb and LP says quantifier 'not at all'"]++; 
			return 0;
		}
		if (source.m[wordSourceIndex + 1].queryWinnerForm(verbForm) != -1)
		{
			errorMap[u"ST correct: ST says adverb and LP says quantifier before an verb"]++;
			return 0;
		}
	}
	if ((word == u"either" || word == u"all") && source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1 && source.m[wordSourceIndex].pma.queryPattern(u"__ALLOBJECTS_1") != -1)
	{
		errorMap[u"LP correct: ST says adverb and LP says quantifier before or as object"]++;
		return 0;
	}
	if (word == u"as" && source.queryPattern(wordSourceIndex, u"__AS_AS") != -1)
	{
		errorMap[u"LP correct: LP AS_AS construction"]++;
		return 0;
	}
	if (word == u"grave" && source.m[wordSourceIndex].queryWinnerForm(adjectiveForm) != -1)
	{
		errorMap[u"LP correct: 'grave' as adjective"]++;
		return 0;
	}
	if ((primarySTLPMatch == u"adverb") && source.m[wordSourceIndex].queryWinnerForm(u"numeral_ordinal") >= 0)
	{
		if (source.queryPattern(wordSourceIndex, u"_PP") != -1)
		{
			errorMap[u"LP correct: ST says adverb, LP says numeral_ordinal matched to object of prep (_PP)"]++;
			return 0;
		}
		if (atStart || source.m[wordSourceIndex + 1].word->first == u"." || source.m[wordSourceIndex + 1].word->first == u"." || source.m[wordSourceIndex + 1].word->first == u"." ||
			  source.m[wordSourceIndex-1].hasWinnerVerbForm())
		{
		errorMap[u"ST correct: ST says adverb, LP says numeral_ordinal matched first word, last word or after verb"]++;
		return 0;
		}
	}
	if ((primarySTLPMatch == u"adverb") && source.m[wordSourceIndex].queryWinnerForm(u"preposition") >= 0 && word == u"before" && source.queryPatternDiff(wordSourceIndex, u"_ADVERB", u"AT13") != -1)
	{
		errorMap[u"diff: 'before' in _ADVERB[AT13] "]++;
		return 0;
	}
	if (word == u"no")
	{
		if (source.m[wordSourceIndex + 1].isOnlyWinner(nounForm) && source.m[wordSourceIndex].isOnlyWinner(determinerForm))
		{
			errorMap[u"LP correct: 'no' as determiner before noun"]++;
			return 0;
		}
		if (source.m[wordSourceIndex + 1].word->first == u"sooner" && source.m[wordSourceIndex].isOnlyWinner(adverbForm))
		{
			errorMap[u"LP correct: 'no' as adverb before 'sooner'"]++;
			return 0;
		}
	}
	int adverbPatternOffset;
	// AT8 not included because it has issues - look at AT8 in the future!
	set <lpwstring> adverbFixedWordPatterns = { u"8",u"T",u"ST",u"ST2",u"AT1",u"AT1b",u"AT1c",u"AT2",u"AT3",u"AT4",u"AT5",u"AT5b",u"AT5c",u"AT6",u"AT7",u"AT9",u"AT10",u"AT11",u"AT11m",u"AT11p",u"AT12",u"AT13",u"AT14",u"6",u"MT",u"B",u"M",u"u",u"TL",u"Y",u"AMONG",u"ND" };
	if ((adverbPatternOffset = source.queryPattern(wordSourceIndex, u"_ADVERB")) != -1 && adverbFixedWordPatterns.find(patterns[source.pema[adverbPatternOffset].getParentPattern()]->differentiator) != adverbFixedWordPatterns.end())
	{
		if (word != u"sun")
		{
			errorMap[u"diff: word matches specific word pattern ADVERB"]++;
			return 0;
		}
	}
	if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(adjectiveForm) != -1)
	{
		errorMap[u"LP correct: adjective not adverb"]++; // ST 305 out of total 771
		return 0;
	}
	//if (word == u"north" || word == u"south" || word == u"east" || word == u"west")
	//{
	//	if (source.m[wordSourceIndex].queryWinnerForm(nounForm) != -1 && source.m[wordSourceIndex].getRelVerb() >= 0 && source.m[wordSourceIndex].getRelVerb() < wordSourceIndex && source.m[source.m[wordSourceIndex].getRelVerb()].hasWinnerVerbForm())
	//		partofspeech += u"direction after verb is an adverb";
	//}
	//if (word == u"half")
	//{
	//	if (source.m[wordSourceIndex].queryWinnerForm(nounForm) != -1 && source.m[wordSourceIndex].getRelVerb() >= 0 && source.m[wordSourceIndex].getRelVerb() < wordSourceIndex)
	//		partofspeech += u"half object is an adverb";
	//}
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].queryWinnerForm(nounForm) != -1)
	{
		errorMap[u"LP correct: noun not verb"]++; // ST 655, Unknown 29 out of total 1739
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(verbForm) != -1)
	{
		errorMap[u"LP correct: verb not noun"]++; // ST 625, LP 526 (Both Wrong) 42 (Both Correct) 62 out of total 1256
		return 0;
	}
	if (primarySTLPMatch == u"noun" && source.m[wordSourceIndex].queryWinnerForm(adjectiveForm) != -1)
	{
		errorMap[u"LP correct: adjective not noun"]++; // ST 240, LP 364 (Both Wrong) 19 out of total 623
		return 0;
	}
	if (word == u"o'clock" && primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(nounForm) != -1)
	{
		errorMap[u"diff: o'clock may syntactically be considered a noun"]++;
		return 0;
	}
	if (primarySTLPMatch == u"adverb" && source.m[wordSourceIndex - 1].queryWinnerForm(prepositionForm) != -1)
	{
		if (word != u"as" && (source.m[wordSourceIndex].queryWinnerForm(nounForm) != -1 || source.m[wordSourceIndex].queryWinnerForm(pronounForm) != -1 || source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1))
		{
			errorMap[u"LP correct: may syntactically be considered a noun after a preposition"]++;
			return 0;
		}
	}
	if ((word == u"present" || word == u"particular") && primarySTLPMatch == u"adjective" && source.m[wordSourceIndex - 1].queryWinnerForm(prepositionForm) != -1)
	{
		errorMap[u"LP correct: present may syntactically be considered a noun after a preposition"]++;
		return 0;
	}
	if (primarySTLPMatch == u"adjective" && source.m[wordSourceIndex].queryWinnerForm(nounForm) != -1)
	{
		errorMap[u"LP correct: noun not adjective"]++; // ST 353, LP 334 (Both Wrong) 16 out of total 704
		return 0;
	}
	if (primarySTLPMatch == u"preposition or conjunction" && source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
	{
		if (source.m[wordSourceIndex + 1].queryWinnerForm(adverbForm) != -1)
		{
			errorMap[u"LP correct: adverb preceding adverb"]++; // out of 100, 4 were incorrect
			return 0;
		}
	}
	int quantifierExplicitPatternPEMAOffset;
	// _ADVERB[AT8], __ADJECTIVE[A], __INFPT[1], __NOUN[D2], __S1[5], _MS1[H], _REL1[2], _REL1[6]
	if (source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1 &&
		  ((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"_ADVERB", u"AT8")) != -1) ||
			((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"__ADJECTIVE", u"A")) != -1) ||
			((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"__INFPT", u"1")) != -1) ||
			((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"__NOUN", u"D2")) != -1) ||
			((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"__S1", u"5")) != -1) ||
			((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"_MS1", u"H")) != -1) ||
			((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"_REL1", u"2")) != -1) ||
			((quantifierExplicitPatternPEMAOffset = source.queryPatternDiff(wordSourceIndex, u"_REL1", u"6")) != -1))
	{
		lpwstring patternName = patterns[source.pema[quantifierExplicitPatternPEMAOffset].getParentPattern()]->name;
		lpwstring patternDiff = patterns[source.pema[quantifierExplicitPatternPEMAOffset].getParentPattern()]->differentiator;
		for (; quantifierExplicitPatternPEMAOffset != -1; quantifierExplicitPatternPEMAOffset = source.pema[quantifierExplicitPatternPEMAOffset].nextByPosition)
			if (patterns[source.pema[quantifierExplicitPatternPEMAOffset].getParentPattern()]->name == patternName && patterns[source.pema[quantifierExplicitPatternPEMAOffset].getParentPattern()]->differentiator == patternDiff)
			{
				if (!source.pema[quantifierExplicitPatternPEMAOffset].isChildPattern() && source.m[wordSourceIndex].getFormNum(source.pema[quantifierExplicitPatternPEMAOffset].getChildForm()) == quantifierForm)
				{
					errorMap[u"diff: LP says quantifier in an explicit construction"]++;
					return 0;
				}
			}
	}
	if (source.m[wordSourceIndex].queryWinnerForm(quantifierForm) != -1 && source.m[wordSourceIndex].pma.queryPatternDiff(u"__NOUN", u"9") != -1 && source.queryPattern(wordSourceIndex, u"__N1") != -1)
	{
		errorMap[u"diff: LP says quantifier in a __NOUN[9] construction"]++;
		return 0;
	}
	if (word == u"right" && source.m[wordSourceIndex - 1].word->first==u"no" && primarySTLPMatch == u"adverb" && source.m[wordSourceIndex].queryWinnerForm(nounForm) != -1)
	{
		errorMap[u"LP correct: right is a noun after 'no'"]++;
		return 0;
	}
	// ST 189, LP 232 out of total 421
	if (word == u"her" && primarySTLPMatch == u"possessive_determiner" && source.m[wordSourceIndex].queryWinnerForm(personalPronounAccusativeForm) != -1)
	{
		errorMap[u"LP correct: LP says 'her' is NOT a possessive"]++;
		return 0;
	}
	// ST 99, LP 310, (Both wrong) 13 out of total 422
	if (word != u"that" && primarySTLPMatch == u"determiner" && source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
	{
		errorMap[u"LP correct: LP says adverb not a determiner"]++;
		return 0;
	}
	// 
	if (primarySTLPMatch == u"personal_pronoun_accusative" && source.m[wordSourceIndex].queryWinnerForm(possessiveDeterminerForm) != -1)
	{
		if ((source.m[wordSourceIndex + 1].queryWinnerForm(verbForm) != -1 || source.m[wordSourceIndex + 1].word->first == u"being" || source.m[wordSourceIndex + 1].word->first == u"doing" || source.m[wordSourceIndex + 1].word->first == u"having") &&
			(source.m[wordSourceIndex + 1].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) == VERB_PRESENT_PARTICIPLE && !cWord::isDash(source.m[wordSourceIndex + 2].word->first[0]))
		{
			errorMap[u"ST correct: LP says determiner, ST says accusative.  It is a participial phrase that modifies the previous pronoun"]++;
			return 0;
		}
		errorMap[u"LP correct: LP says possessive not an accusative"]++;
		return 0;
	}
	// check for any pattern specified explicit word checks
	for (int pemaPosition=source.m[wordSourceIndex].beginPEMAPosition; pemaPosition != -1; pemaPosition = source.pema[pemaPosition].nextByPosition)
		if (!source.pema[pemaPosition].isChildPattern() && patterns[source.pema[pemaPosition].getParentPattern()]->getElement(source.pema[pemaPosition].getElement())->specificWords[source.pema[pemaPosition].getElementIndex()]==word)
		{
			if (source.m[wordSourceIndex].queryWinnerForm(predeterminerForm) != -1)
			{
				errorMap[u"LP correct: LP says predeterminer in an explicit construction"]++;
				return 0;
			}
			if (patterns[source.pema[pemaPosition].getParentPattern()]->name == u"_NOUN" && patterns[source.pema[pemaPosition].getParentPattern()]->differentiator == u"ANY")
			{
				errorMap[u"LP correct: LP says quantifier in an explicit construction"]++;
				return 0;
			}
			if (patterns[source.pema[pemaPosition].getParentPattern()]->name == u"_REL1")
			{
				errorMap[u"diff: LP says demonstrative determiner and ST says adverb to head a relative phrase"]++;
				return 0;
			}
			//partofspeech += u"EXPLICIT - "+ patterns[source.pema[pemaPosition].getPattern()]->name + u"["+ patterns[source.pema[pemaPosition].getPattern()]->differentiator +u"]";
			break; 
		}
	// ST 57, LP 145, (Both wrong) 14 out of total 216
	if (primarySTLPMatch == u"verb" && source.m[wordSourceIndex].queryWinnerForm(adverbForm) != -1)
	{
		errorMap[u"LP correct: LP says adverb not a verb"]++;
		return 0;
	}
	if (primarySTLPMatch == u"personal_pronoun_accusative" && source.m[wordSourceIndex].queryWinnerForm(possessivePronounForm) != -1)
	{
		errorMap[u"LP correct: LP says possessive pronoun NOT personal_pronoun_accusative"]++;
		return 0;
	}
	if (primarySTLPMatch == u"determiner" && source.m[wordSourceIndex].queryWinnerForm(pronounForm) != -1)
	{
		errorMap[u"LP correct: LP says pronoun NOT determiner"]++;
		return 0;
	}
	if (word == u"how" && !iswalpha(source.m[wordSourceIndex + 1].word->first[0]))
	{
		errorMap[u"LP correct: LP says 'how' is interjection"]++;
		return 0;
	}
	if (word == u"quite" && source.m[wordSourceIndex].queryWinnerForm(predeterminerForm) != -1)
	{
		errorMap[u"LP correct: LP says 'quite' is predeterminer"]++;
		return 0;
	}
	lpwstring winnerFormsString;
	source.m[wordSourceIndex].winnerFormString(winnerFormsString, false);
	// matrix analysis
	// combo list - primarySTLPMatch comes first!
	for (int I=0; I<5; I++)
		for (int J=0; J<5; J++)
			formMatrixTest(source, wordSourceIndex, primarySTLPMatch, winnerFormsString, I, J, comboCostFrequency, partofspeech);
	return -1;
}

// checks if the part of speech indicated in parse from the Stanford POS tagger matches the winner forms at wordSourceIndex.
// returns yes=0, no=1
// Locate originalWord in the PCFG tree via stTokenizeWord, map the Penn tag,
// then ruleCorrectLPClass / attributeErrors.  Updates formDistribution.
int checkStanfordPCFGAgainstWinner(cSource &source, int wordSourceIndex, int numTimesWordOccurred, lpwstring originalParse, lpwstring sentence, lpwstring &parse, int &numTotalDifferenceFromStanford, 
	unordered_map<lpwstring, int> &formNoMatchMap, unordered_map<lpwstring, int> &formMisMatchMap, unordered_map<lpwstring, int> &wordNoMatchMap, unordered_map<lpwstring, int> &VFTMap,
	bool inRelativeClause, unordered_map<lpwstring, int> &errorMap, unordered_map<lpwstring, int> &comboCostFrequency,int startOfSentence,int maxLength)
{
	lpwstring word = source.m[wordSourceIndex].word->first;
	if (!iswalpha(word[0]) || (word.length()<=1 && word[0]!='a' && word[0]!='i'))
		return 0;
	lpwstring originalWordSave;
	source.getOriginalWord(wordSourceIndex, originalWordSave, false, false);
	std::replace(originalWordSave.begin(), originalWordSave.end(), u'’', u'\'');
	lpwstring originalWord = originalWordSave;
	int wspace;
	lpwstring lookFor=stTokenizeWord(word,originalWord, source.m[wordSourceIndex].flags,parse,wspace);
	size_t wow = parse.find(lookFor);
	if (wow != lpwstring::npos)
	{
		auto firstparen = parse.rfind(u'(', wow);
		if (firstparen != lpwstring::npos)
		{
			lpwstring partofspeech = parse.substr(firstparen + 1, wow - firstparen - 1);
			parse.erase(0, wow+ lookFor.length());
			extern unordered_map<lpwstring, vector<lpwstring>> pennMapToLP;
			auto lpPOS = pennMapToLP.find(partofspeech);
			if (lpPOS != pennMapToLP.end())
			{
				std::set<lpwstring> posList(lpPOS->second.begin(), lpPOS->second.end());
				// ST incorrectly assigned a word a punctuation!
				if (posList.empty())
				{
					errorMap[u"ST Punctuation assignment"]++;
					return 0;
				}
				
				lpwstring primarySTLPMatch = lpPOS->second[0];
				if (partofspeech == u"IN")
					primarySTLPMatch += u" or " + lpPOS->second[1];
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
				lpwstring posListStr;
				for (auto pos : posList)
					posListStr += pos + u" ";
				lpwstring winnerFormsString;
				source.m[wordSourceIndex].winnerFormString(winnerFormsString, false);
				formNoMatchMap[posListStr + u"!= " + winnerFormsString]++;
				wordNoMatchMap[word]++;
				formMisMatchMap[primarySTLPMatch + u"!=" + winnerFormsString]++;
				lpwstring originalNextWord;
				source.getOriginalWord(wordSourceIndex + 1, originalNextWord, false, false);
				size_t pos = sentence.find(originalWord);
				while (pos != lpwstring::npos && numTimesWordOccurred>0)
				{
					size_t nextWordPos = pos + originalWord.length() + 1;
					bool wholeWord = (pos == 0 || sentence[pos - 1] == ' ') && (sentence.length() == pos + originalWord.length() || sentence[pos + originalWord.length()] == u' ' || sentence[pos + originalWord.length()] == u'\'');
					if (wholeWord && !--numTimesWordOccurred)
						sentence.replace(pos,originalWord.length(), u"*" + originalWord + u"*");
					pos = sentence.find(originalWord,pos+ originalWord.length()+2);
				}
				// not useful anymore
				//if (source.m[wordSourceIndex].flags&cWordMatch::flagFirstLetterCapitalized)
				//	partofspeech += u"**CAP**";
				lplog(LOG_ERROR, u"Stanford POS %s%s (%s) not found in winnerForms %s for word%s %s %07d:[%s]", partofspeech.c_str(), (sentence.length()<=maxLength && maxLength!=-1) ? u"SHORT":u"",primarySTLPMatch.c_str(), winnerFormsString.c_str(), (originalWord.find(u' ')==lpwstring::npos) ? u"":u"[space]", originalWord.c_str(), wordSourceIndex, sentence.c_str());
				for (int wf : winnerForms)
				{
					fdi->second.LPErrorFormDistribution[Forms[wf]->name]++;
				}
				fdi->second.unaccountedForDisagreeSTLP++;
				//formDistribution[word] = fd;
				if (originalWord.find(u' ') != lpwstring::npos &&
					word != u"no one" && word != u"every one" && word != u"as if" && word != u"for ever" && word != u"next to" && word != u"good by" && word != u"good bye" && word != u"a trifle" && word!=u"a" && word!=u"i" && word!=u"young 'un")
				{
					lookFor = originalWord.substr(wspace, originalWord.length()) + u")";
					parse = originalParse;
					wow = parse.find(lookFor);
					if (wow != lpwstring::npos)
					{
						auto firstparen = parse.rfind(u'(', wow);
						if (firstparen != lpwstring::npos)
						{
							lpwstring partofspeech = parse.substr(firstparen + 1, wow - firstparen - 1);
							parse.erase(0, wow + lookFor.length());
							extern unordered_map<lpwstring, vector<lpwstring>> pennMapToLP;
							auto lpPOS = pennMapToLP.find(partofspeech);
							if (lpPOS != pennMapToLP.end())
							{
								vector<lpwstring> posList = lpPOS->second;
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
									lpwstring posListStr;
									for (auto pos : posList)
										posListStr += pos + u" ";
									lplog(LOG_ERROR, u"FATAL! %07d:ALSO no winnerForm %s is found in ST POS list %s (2)", wordSourceIndex,  winnerFormsString.c_str(), posListStr.c_str());
								}
							}
						}
						else
							lplog(LOG_FATAL_ERROR, u"%d:Parenthesis not found in %s (from position %d) (2).", wordSourceIndex,parse.c_str(), wow);
					}
					else
						lplog(LOG_ERROR, u"%d:Word %s not found in parse %s (2).", wordSourceIndex,lookFor.c_str(),parse.c_str());
				}
			}
			else
				lplog(LOG_FATAL_ERROR, u"%d:Part of Speech %s not found looking for word %s in the parse %s.", wordSourceIndex,partofspeech.c_str(),originalWord.c_str(),originalParse.c_str());
		}
		else
			lplog(LOG_FATAL_ERROR, u"%d:Parenthesis not found in %s (from position %d).", wordSourceIndex,parse.c_str(),wow);
	}
	else if (parse.length()>1)
		lplog(LOG_ERROR, u"%d:FATAL! Word %s not found in parse %s [%s]", wordSourceIndex,originalWord.c_str(),parse.c_str(),originalParse.c_str());
	return 1;
}

//map <lpwstring, int> STFormDistribution; // total count for each form match in ST
//map <lpwstring, int> LPFormDistribution; // total count for each form match in LP
//map <lpwstring, int> agreeFormDistribution; // total count for each form match agreed between ST and LP
//map <lpwstring, int> disagreeFormDistribution; // total count for each form match disagreed between ST and LP
// Log one word's ST/LP form-agreement histogram.  Tracks the worst
// (high-frequency, low-agreement) form in maxWord/maxForm/maxDiff.
void printFormDistribution(lpwstring word, double adp, FormDistribution fd, lpwstring &maxWord, lpwstring &maxForm, int &maxDiff,int limit)
{
	if (fd.unaccountedForDisagreeSTLP == 0)
		return;
	if (limit<1000)
		lplog(LOG_ERROR, u"%s:%d %3.0f (%d/%d)", word.c_str(), fd.unaccountedForDisagreeSTLP, adp, fd.agreeSTLP, fd.agreeSTLP + fd.disagreeSTLP);
	int totalWordOccurrenceCount = fd.agreeSTLP + fd.disagreeSTLP;
	for (auto &&[form, formCount] : fd.LPFormDistribution)
	{
		// form name, total number of times form is a winner form for this word, % of times this form is the winner form for this word, % of times this form agrees with ST.
		if (limit<1000)
			lplog(LOG_ERROR, u"  LP %s:total=%d accounted=%d agree=%d disagree=%d error=%d %d%% %d%%", 
				form.c_str(), 
				formCount, fd.LPAlreadyAccountedFormDistribution[form], fd.agreeFormDistribution[form], fd.disagreeFormDistribution[form], fd.LPErrorFormDistribution[form],
				100 * formCount / totalWordOccurrenceCount, fd.agreeFormDistribution[form] * 100 / formCount);
		// commented out: look for forms that have a high percentage of LP winners, but a low percentage of agreement.
		// actually just look for the highest occurring forms with maximum poor agreement.
		int diff = formCount * (100 - (fd.agreeFormDistribution[form] * 100 / formCount));//count*count / totalWordOccurrenceCount*fd.agreeFormDistribution[form];
		if ((fd.LPErrorFormDistribution[form]>10 && (fd.agreeFormDistribution[form] * 100 / formCount) < 5 && fd.LPAlreadyAccountedFormDistribution[form]<5) ||
			fd.disagreeFormDistribution[form]*1000/formCount<=5)
		{
			lplog(LOG_ERROR, u"%05d *^*%s:%3.2f (%d/%d) %s:total=%d accounted=%d agree=%d disagree=%d error=%d %d%% %d%%", 
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
			lplog(LOG_ERROR, u"  ST %s:%d %d%% %d%%", form.c_str(), count, 100 * count / (fd.agreeSTLP + fd.disagreeSTLP), fd.agreeFormDistribution[form] * 100 / count);
}

// Read one parsed source, JNI-parse each sentence, and compare ST vs LP winners.
// lockPerSource takes a WRITE lock on stanfordPCFGParsedSentences for the whole
// document.  limitToWord / maxLength restrict the scan.  Returns the next proc2.
int stanfordCheckFromSource(cSource &source, int sourceId, lpwstring path, JavaVM *vm,JNIEnv *env, int &numNoMatch, int &numPOSNotFound, int &numTotalDifferenceFromStanford,unordered_map<lpwstring, int> &formNoMatchMap,
	                          unordered_map<lpwstring, int> &formMisMatchMap, unordered_map<lpwstring, int> &wordNoMatchMap, unordered_map<lpwstring, int> &VFTMap, 
	                          unordered_map<lpwstring, int> &errorMap, unordered_map<lpwstring, int> &comboCostFrequency, bool pcfg,lpwstring limitToWord,int maxLength, lpwstring specialExtension,bool lockPerSource)
{
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -20;
	Words.readWords(path, sourceId, false, u""); 
	bool parsedOnly = false,bearFound=false;
	int numIllegalWords = 0;
	if (source.readSource(path, false, parsedOnly, false, specialExtension))
	{
		lplog(LOG_INFO| LOG_ERROR, u"source*** %d:%s", sourceId, path.c_str());
		int lastPercent=-1;
		if (lockPerSource)
		{
			if (!myquery(&source.mysql, u"LOCK TABLES stanfordPCFGParsedSentences WRITE")) // moved out parseSentence (actually in foundParseSentence and setParsedSentence) for performance
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
			lpwstring sentence;
			int numLetters=0, numNumbers=0;
			for (int I = wordSourceIndex; I < endIndex; I++)
			{
				lpwstring originalIWord;
				source.getOriginalWord(I, originalIWord, false, false);
				if (source.m[I].word->first.length() == 1 && iswalpha(source.m[I].word->first[0]))
					numLetters++;
				if (source.m[I].word->second.query(NUMBER_FORM_NUM) >= 0)
					numNumbers++;
				sentence += originalIWord + u" ";
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
			lpwstring parse;
			if (parseSentence(source, env, sentence, parse, pcfg, !lockPerSource) < 0)
			{
				wordSourceIndex++;
				continue;
			}
			lpwstring originalParse = parse;
			int start = wordSourceIndex, until = -1,pmaOffset=-1;
			bool inRelativeClause = false, sentencePrinted = false;
			map <lpwstring, int> numTimesWordOccurred;
			for (; wordSourceIndex < endIndex; wordSourceIndex++)
			{
				lpwstring originalIWord;
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
		unlockTables(source.mysql);;
	}
	else
	{
		lplog(LOG_ERROR, u"Unable to read source %d:%s\n", sourceId, path.c_str());
		unlockTables(source.mysql);;
		return -1;
	}
	return 10;
}

// Re-split noun/verb cost-bucket counts using hand-audited ST-vs-LP fractions.
void distributeErrorsByCost(unordered_map<lpwstring, int> &errorMap)
{
	int numErrors;
	// VERB NOUN
	numErrors = errorMap[u"LP correct: (verb cost 4, noun cost 0 1SING)"]; // 32 ST correct out of 135 total
	errorMap[u"LP correct: (verb cost 4, noun cost 0 1SING)"] = numErrors * 103 / 135;
	errorMap[u"ST correct: (verb cost 4, noun cost 0 1SING)"] = numErrors * 32 / 135;

	numErrors = errorMap[u"LP correct: (verb cost 4, noun cost 0 3SING)"]; // 33 ST correct out of 425 total
	errorMap[u"LP correct: (verb cost 4, noun cost 0 3SING)"] = numErrors * 392 / 425;
	errorMap[u"ST correct: (verb cost 4, noun cost 0 3SING)"] = numErrors * 33 / 425;

	// this is all tenses
	numErrors = errorMap[u"LP correct: (verb cost 3, noun cost 0)"]; // 261 ST correct, 400 LP correct, 19 neither correct, 1 ambiguous out of 681 total
	errorMap[u"LP correct: (verb cost 3, noun cost 0)"] = numErrors * 400 / 681;
	errorMap[u"ST correct: (verb cost 3, noun cost 0)"] = numErrors * 261 / 681;
	errorMap[u"diff: (verb cost 3, noun cost 0)"] = numErrors * 20 / 681;

	numErrors = errorMap[u"LP correct: (verb cost 2, noun cost 0 1SING)"]; // 137 ST correct, 222 LP correct, out of 359 total
	errorMap[u"LP correct: (verb cost 2, noun cost 0 1SING)"] = numErrors * 222 / 359;
	errorMap[u"ST correct: (verb cost 2, noun cost 0 1SING)"] = numErrors * 137 / 359;

	// NOUN VERB
	numErrors = errorMap[u"LP correct: (noun cost 4, verb cost 0 1SING)"]; // 33 ST correct, 4 both wrong out of 203 total
	errorMap[u"LP correct: (noun cost 4, verb cost 0 1SING)"] = numErrors * 166 / 203;
	errorMap[u"ST correct: (noun cost 4, verb cost 0 1SING)"] = numErrors * 33 / 203;
	errorMap[u"diff: (noun cost 4, verb cost 0 1SING)"] = numErrors * 4 / 203;

	numErrors = errorMap[u"LP correct: (noun cost 4, verb cost 0 REST OF TENSE)"]; // 24 ST correct, 22 wrong or ambiguous out of 160 total
	errorMap[u"LP correct: (noun cost 4, verb cost 0) REST OF TENSE"] = numErrors * 114 / 160;
	errorMap[u"ST correct: (noun cost 4, verb cost 0) REST OF TENSE"] = numErrors * 24 / 160;
	errorMap[u"diff: (noun cost 4, verb cost 0) REST OF TENSE"] = numErrors * 22 / 160;

	numErrors = errorMap[u"LP correct: (noun cost 3, verb cost 0 1SING)"]; // 73 ST correct out of 217 total
	errorMap[u"LP correct: (noun cost 3, verb cost 0 1SING)"] = numErrors * 144 / 217;
	errorMap[u"ST correct: (noun cost 3, verb cost 0 1SING)"] = numErrors * 73 / 217;

	numErrors = errorMap[u"LP correct: (noun cost 2, verb cost 0 1SING)"]; // 178 ST correct, out of 429 total
	errorMap[u"LP correct: (noun cost 2, verb cost 0 1SING)"] = numErrors * 251 / 429;
	errorMap[u"ST correct: (noun cost 2, verb cost 0 1SING)"] = numErrors * 178 / 429;

	numErrors = errorMap[u"LP correct: (noun cost 0, verb cost 0 1SING)"]; // 232 ST correct out of 569 total
	errorMap[u"LP correct: (noun cost 0, verb cost 0 1SING)"] = numErrors * 337 / 569;
	errorMap[u"ST correct: (noun cost 0, verb cost 0 1SING)"] = numErrors * 232 / 569;

	numErrors = errorMap[u"LP correct: (noun cost 0, verb cost 4 1SING)"]; // 405 ST correct out of 525 total
	errorMap[u"LP correct: (noun cost 0, verb cost 4 1SING)"] = numErrors * 120 / 525;
	errorMap[u"ST correct: (noun cost 0, verb cost 4 1SING)"] = numErrors * 405 / 525;

	numErrors = errorMap[u"LP correct: (noun cost 0, verb cost 3 1SING)"]; // 98 ST correct out of 246 total
	errorMap[u"LP correct: (noun cost 0, verb cost 3 1SING)"] = numErrors * 148 / 246;
	errorMap[u"ST correct: (noun cost 0, verb cost 3 1SING)"] = numErrors * 98 / 246;

	// NOUN ADJECTIVE
	numErrors = errorMap[u"LP correct: (noun cost 4, adjective cost 0)"]; // out of 252 examples, 23 were incorrect (15 of those were the word 'safe'?)
	errorMap[u"LP correct: (noun cost 4, adjective cost 0)"] = numErrors * 90 / 100;
	errorMap[u"ST correct: (noun cost 4, adjective cost 0)"] = numErrors * 10 / 100;

	numErrors = errorMap[u"LP correct: (noun cost 3, adjective cost 0)"]; // 71 ST correct out of 403 total
	errorMap[u"LP correct: (noun cost 3, adjective cost 0)"] = numErrors * 332 / 403;
	errorMap[u"ST correct: (noun cost 3, adjective cost 0)"] = numErrors * 71 / 403;

	numErrors = errorMap[u"LP correct: (noun cost 2, adjective cost 0)"]; // 48 ST correct out of 122 total
	errorMap[u"LP correct: (noun cost 2, adjective cost 0)"] = numErrors * 74 / 122;
	errorMap[u"ST correct: (noun cost 2, adjective cost 0)"] = numErrors * 48 / 122;

	// ADVERB ADJECTIVE
	numErrors = errorMap[u"LP correct: (adverb cost 4, adjective cost 0)"]; // 51 ST correct, out of 261 total
	errorMap[u"LP correct: (adverb cost 4, adjective cost 0)"] = numErrors * 215 / 261;
	errorMap[u"ST correct: (adverb cost 4, adjective cost 0)"] = numErrors * 51 / 261;

	numErrors = errorMap[u"LP correct: (adverb cost 3, adjective cost 0)"]; // 98 ST correct, out of 243 total
	errorMap[u"LP correct: (adverb cost 3, adjective cost 0)"] = numErrors * 145 / 243;
	errorMap[u"ST correct: (adverb cost 3, adjective cost 0)"] = numErrors * 98 / 243;

	errorMap[u"LP correct: adjective not adverb"]++; // ST 305 out of total 771
	errorMap[u"LP correct: adjective not adverb"] = numErrors * 450 / 771;
	errorMap[u"ST correct: adjective not adverb"] = numErrors * 305 / 771;
	errorMap[u"diff: adjective not adverb"] = numErrors * 16 / 771;
}

// After distributeErrorsByCost, apply more hand-audited fractions to the
// remaining "LP correct: ..." buckets so the summary % is not 100% LP.
void distributeErrors(unordered_map<lpwstring, int> &errorMap)
{
	distributeErrorsByCost(errorMap);

	int numErrors = errorMap[u"LP correct: adjective not verb"];  // out of 215 examples studied, 12 were incorrect
	errorMap[u"LP correct: adjective not verb"] = numErrors * 94 / 100;
	errorMap[u"ST correct: adjective not verb"] = numErrors * 6 / 100;

	numErrors = errorMap[u"LP correct: adverb not noun"];  // out of 233 examples studied, 7 were incorrect
	errorMap[u"LP correct: adverb not noun"] = numErrors * 97 / 100;
	errorMap[u"ST correct: adverb not noun"] = numErrors * 3 / 100;

	numErrors=errorMap[u"LP correct: LP says adverb but ST says adjective"]; // ST correct 421 out of 1090 total (+ 7 temporal expressions not included)
	errorMap[u"LP correct: LP says adverb but ST says adjective"] = numErrors * 669 / 1090;
	errorMap[u"ST correct: LP says adverb but ST says adjective"] = numErrors * 421 / 1090;

	numErrors = errorMap[u"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"]; // probability 6 out of 130 are ST correct
	errorMap[u"LP correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"] = numErrors * 130 / 136;
	errorMap[u"ST correct: word 'her': [before a low cost noun] ST says personal_pronoun_accusative LP says possessive_determiner"] = numErrors * 6 / 136;
	
	numErrors = errorMap[u"LP correct: noun more probable than adjective"]; // 19 ST correct out of 119 total
	errorMap[u"LP correct: noun more probable than adjective"] = numErrors * 100 / 119;
	errorMap[u"ST correct: noun more probable than adjective"] = numErrors * 19 / 119;

	numErrors = errorMap[u"LP correct: preposition with relative object"]; // 22 ST correct out of 185 total
	errorMap[u"LP correct: preposition with relative object"] = numErrors * 163 / 185;
	errorMap[u"ST correct: preposition with relative object"] = numErrors * 22 / 185;
	
	numErrors = errorMap[u"LP correct: ownership of noun"]; // 12 ST correct out of 91 total 
	errorMap[u"LP correct: ownership of noun"] = numErrors * 79 / 91;
	errorMap[u"ST correct: ownership of noun"] = numErrors * 12 / 91;

	numErrors = errorMap[u"LP correct: past of verb is not adjective"]; // 21 ST correct out of 105 total  
	errorMap[u"LP correct: past of verb is not adjective"] = numErrors * 84 / 105;
	errorMap[u"ST correct: past of verb is not adjective"] = numErrors * 21 / 105;

	numErrors = errorMap[u"LP correct: word 'kind' ST says adjective"]; // 17 ST correct out of 102 total  
	errorMap[u"LP correct: word 'kind' ST says adjective"] = numErrors * 85 / 102;
	errorMap[u"ST correct: word 'kind' ST says adjective"] = numErrors * 17 / 102;

	numErrors = errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb before verb"]; // 27 ST correct out of 285 total
	errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb before verb"]=numErrors*258/(258+27);
	errorMap[u"ST correct: ST says preposition or conjunction and LP says adverb before verb"]=numErrors*27/(258+27);

	numErrors = errorMap[u"ST correct: ST says preposition or conjunction and LP says adverb before noun"]; // 171 ST correct out of 177 total
	errorMap[u"LP correct: ST says preposition or conjunction and LP says adverb before noun"] = numErrors * 6 / 177;
	errorMap[u"ST correct: ST says preposition or conjunction and LP says adverb before noun"] = numErrors * 171 / 177;

	numErrors = errorMap[u"LP correct: ST says adjective (wrong)"]; // 30 ST correct out of 329 total  
	errorMap[u"LP correct: ST says adjective (wrong)"] = numErrors * 299 / 329;
	errorMap[u"ST correct: ST says adjective (wrong)"] = numErrors * 30 / 329;

	numErrors = errorMap[u"LP correct : word 'her' : [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"]; // 28 ST correct out of 152 total
	errorMap[u"LP correct : word 'her' : [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"] = numErrors * 124 / 152;
	errorMap[u"ST correct : word 'her' : [before an adverb, determiner, personal_pronoun_nominative, coordinator, indefinite_pronoun] ST says possessive_determiner LP says personal_pronoun_accusative"] = numErrors * 28 / 152;
	
	numErrors = errorMap[u"LP correct: ST says adjective, LP says verb(PAST/PRESENT_PARTICIPLE)"]; // 27 ST correct out of 115 total
	errorMap[u"LP correct: ST says adjective, LP says verb(PAST/PRESENT_PARTICIPLE)"] = numErrors * 88 / 115;
	errorMap[u"ST correct: ST says adjective, LP says verb(PAST/PRESENT_PARTICIPLE)"] = numErrors * 27 / 115;

	numErrors = errorMap[u"LP correct: ST says adjective, LP says verb (highly ambiguous)"]; // 106 ST correct, 89 diff out of 320 total
	errorMap[u"LP correct: ST says adjective, LP says verb (highly ambiguous)"] = numErrors * 125 / 320;
	errorMap[u"ST correct: ST says adjective, LP says verb (highly ambiguous)"] = numErrors * 106 / 320;
	errorMap[u"diff: ST says adjective, LP says verb (highly ambiguous)"] = numErrors * 89 / 320;

	numErrors = errorMap[u"LP correct: ST says adjective, LP says non-past verb (highly ambiguous)"]; // 114 ST correct, 12 diff, 7 both wrong out of 288 total
	errorMap[u"LP correct: ST says adjective, LP says non-past verb (highly ambiguous)"] = numErrors * 155 / 320;
	errorMap[u"ST correct: ST says adjective, LP says non-past verb (highly ambiguous)"] = numErrors * 114 / 320;
	errorMap[u"diff: ST says adjective, LP says non - past verb(highly ambiguous)"] = numErrors * 12 / 320;

	numErrors= errorMap[u"LP correct: word 'round': ST says adverb/noun LP says preposition."];  // ST correct 5, out of 126
	errorMap[u"LP correct: word 'round': ST says adverb/noun LP says preposition."] = numErrors * 121 / 126;
	errorMap[u"ST correct: word 'round': ST says adverb/noun LP says preposition."] = numErrors * 5 / 126;

	numErrors = errorMap[u"LP correct: noun not verb"];  // ST 655, Unknown 29 out of total 1739
	errorMap[u"LP correct: noun not verb"] = numErrors * 1055 / 1739;
	errorMap[u"ST correct: noun not verb"] = numErrors * 655 / 1739;
	errorMap[u"diff: noun not verb"] = numErrors * 29 / 1739;

	// ST 625, LP 526 (Both Wrong) 42 (Both Correct) 62 out of total 1256
	numErrors = errorMap[u"LP correct: verb not noun"];  // ST 655, Unknown 29 out of total 1739
	errorMap[u"LP correct: verb not noun"] = numErrors * 526 / 1256;
	errorMap[u"ST correct: verb not noun"] = numErrors * 625 / 1256;
	errorMap[u"diff: verb not noun"] = numErrors * 62 / 1256;

	// ST 240, LP 364 (Both Wrong) 19 out of total 623
	numErrors = errorMap[u"LP correct: adjective not noun"];  
	errorMap[u"LP correct: adjective not noun"] = numErrors * 364 / 623;
	errorMap[u"ST correct: adjective not noun"] = numErrors * 240 / 623;
	errorMap[u"diff: adjective not noun"] = numErrors * 19 / 623;

	// ST 353, LP 334 (Both Wrong) 16 out of total 704
	numErrors = errorMap[u"LP correct: noun not adjective"];
	errorMap[u"LP correct: noun not adjective"] = numErrors * 334 / 704;
	errorMap[u"ST correct: noun not adjective"] = numErrors * 353 / 704;
	errorMap[u"diff: noun not adjective"] = numErrors * 16 / 704;

	// ST 189, LP 232 out of total 421
	numErrors = errorMap[u"LP correct: LP says 'her' is NOT a possessive"];
	errorMap[u"LP correct: LP says 'her' is NOT a possessive"] = numErrors * 232 / 421;
	errorMap[u"ST correct: LP says 'her' is NOT a possessive"] = numErrors * 189 / 421;

	// ST 99, LP 310, (Both wrong) 13 out of total 422
	numErrors = errorMap[u"LP correct: LP says adverb not a determiner"];
	errorMap[u"LP correct: LP says adverb not a determiner"] = numErrors * 310 / 422;
	errorMap[u"ST correct: LP says adverb not a determiner"] = numErrors * 99 / 422;
	errorMap[u"diff: LP says adverb not a determiner"] = numErrors * 13 / 422;

	// ST 49, LP 119 out of total 168
	numErrors = errorMap[u"LP correct: LP says possessive not an accusative"];
	errorMap[u"LP correct: LP says possessive not an accusative"] = numErrors * 119 / 168;
	errorMap[u"ST correct: LP says possessive not an accusative"] = numErrors * 49 / 168;

	// ST 57, LP 145, (Both wrong) 14 out of total 216
	numErrors = errorMap[u"LP correct: LP says adverb not a verb"];
	errorMap[u"LP correct: LP says adverb not a verb"] = numErrors * 145 / 216;
	errorMap[u"ST correct: LP says adverb not a verb"] = numErrors * 57 / 216;
	errorMap[u"diff: LP says adverb not a verb"] = numErrors * 14 / 216;

	// ST 25, LP 91 out of total 116
	numErrors = errorMap[u"LP correct: LP says pronoun NOT determiner"];
	errorMap[u"LP correct: LP says pronoun NOT determiner"] = numErrors * 91 / 116;
	errorMap[u"ST correct: LP says pronoun NOT determiner"] = numErrors * 25 / 116;
	
}

// Batch Stanford check over proc2==step (longest sources first).  Creates one
// JVM for the run.  Updates proc2 to stanfordCheckFromSource's return.
int stanfordCheck(cSource source, int step, bool pcfg, lpwstring specialExtension, bool lockPerSource)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum cSource::sourceTypeEnum st = cSource::GUTENBERG_SOURCE_TYPE;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numSourcesProcessedNow = 0;
	lpchar_t buffer[1024];
	lp_wsprintf(buffer, u"stanfordCheck %d", step);
	lpReportProgress(buffer);
	lplog(LOG_INFO | LOG_ERROR | LOG_NOTMATCHED, NULL); // close all log files to change extension
	logFileExtension = u".stanfordCheckErrors"+specialExtension;

	if (!myquery(&source.mysql, u"LOCK TABLES sources WRITE"))
		return -1;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by numWords desc", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	JavaVM *vm;
	JNIEnv *env;
	createJavaVM(vm, env);
	unordered_map<lpwstring, int> formNoMatchMap, formMisMatchMap, wordNoMatchMap,VFTMap,errorMap, comboCostFrequency;
	int numNoMatch = 0, numPOSNotFound = 0,totalWords=0, numTotalDifferenceFromStanford=0;
	my_ulonglong totalSource = mysql_num_rows(result);
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		lpwstring path, etext, title;
		int sourceId = atoi(sqlrow[0]);
		if (sqlrow[1] == NULL)
			etext = u"NULL";
		else
			mTW(sqlrow[1], etext);
		mTW(sqlrow[2], path);
		mTW(sqlrow[3], title);
		path.insert(0, u"\\").insert(0, CACHEDIR);
		lpchar_t buffer[1024];
		int64_t processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		lp_wsprintf(buffer, u"%%%03I64d:%d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s...)", numSourcesProcessedNow * 100 / totalSource, step, numSourcesProcessedNow, totalSource,
			processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, (processingSeconds) ? numSourcesProcessedNow * 3600 / processingSeconds : 0, title.c_str());
		lpReportProgress(buffer);
		int setStep = stanfordCheckFromSource(source, sourceId, path, vm, env, numNoMatch, numPOSNotFound, numTotalDifferenceFromStanford, formNoMatchMap, formMisMatchMap, wordNoMatchMap,VFTMap,errorMap, comboCostFrequency,pcfg,u"",-1,specialExtension,lockPerSource);
		totalWords += source.m.size();
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"update sources set proc2=%d where id=%d", setStep, sourceId);
		if (!myquery(&source.mysql, qt))
			break;
		source.clearSource();
		numSourcesProcessedNow++;
	}
	mysql_free_result(result);
	destroyJavaVM(vm);
	lplog(LOG_ERROR, u"WORD AGREE DISTRIBUTION ------------------------------------------------------------");
	map <int, lpwstring, std::greater<int>> agreeCountMap;
	for (auto &&[word, fd] : formDistribution)
	{
		agreeCountMap[fd.unaccountedForDisagreeSTLP] += word + u"*";
	}
	int limit = 0;
	int maxDiff = -1;
	lpwstring maxForm,maxWord;
	for (auto &&[adp, multiword] : agreeCountMap)
	{
		for (auto word : splitString(multiword, u'*'))
			if (formDistribution[word].agreeSTLP + formDistribution[word].disagreeSTLP > 100)
			{
				printFormDistribution(word, adp, formDistribution[word], maxWord, maxForm, maxDiff,limit);
				limit++;
			}
	}
	lplog(LOG_ERROR, u"maxWord=%s maxForm=%s maxDiff=%d", maxWord.c_str(), maxForm.c_str(), maxDiff);
	lplog(LOG_ERROR, u"FORMS ------------------------------------------------------------------------------");
	map<int, lpwstring, std::greater<int>> formNoMatchReverseMap, formMisMatchReverseMap, wordNoMatchReverseMap,VFTReverseMap,errorReverseMap, comboCostFrequencyReverseMap;
	for (auto const&[forms, count] : formNoMatchMap)
		formNoMatchReverseMap[count] += forms + u" *";
	for (auto const&[count, forms] : formNoMatchReverseMap)
		lplog(LOG_ERROR, u"forms %s [%d]", forms.c_str(), count);
	lplog(LOG_ERROR, u"FORM MAPPING ---------------------------------------------------------------------------");
	for (auto const&[forms, count] : formMisMatchMap)
		formMisMatchReverseMap[count] += forms + u" *";
	for (auto const&[count, forms] : formMisMatchReverseMap)
		lplog(LOG_ERROR, u"forms %s [%d]", forms.c_str(), count);
	if (!VFTMap.empty())
	{
		lplog(LOG_ERROR, u"VFT --------------------------------------------------------------------------------");
		for (auto const&[word, count] : VFTMap)
			VFTReverseMap[count] += word + u" *";
	for (auto const&[count, word] : VFTReverseMap)
		lplog(LOG_ERROR, u"VFT %s [%d]", word.c_str(), count);
	}
	lplog(LOG_ERROR, u"WORDS ------------------------------------------------------------------------------");
	for (auto const&[forms, count] : wordNoMatchMap)
		wordNoMatchReverseMap[count] += forms + u" *";
	int numListed = 0;
	for (auto const&[count, forms] : wordNoMatchReverseMap)
	{
		lplog(LOG_ERROR, u"words %s [%d]", forms.c_str(), count);
		if (numListed++ > 100)
			break;
	}
	lplog(LOG_ERROR, u"ComboExtremeCosts ------------------------------------------------------------------------------");
	for (auto const&[formCombo, frequency] : comboCostFrequency)
		comboCostFrequencyReverseMap[frequency] += formCombo + u" *";
	numListed = 0;
	for (auto const&[frequency, formCombo] : comboCostFrequencyReverseMap)
	{
		lplog(LOG_ERROR, u"combo %s [%d]", formCombo.c_str(), frequency);
		if (numListed++ > 100)
			break;
	}
	lplog(LOG_ERROR, u"DIFF ANALYSIS ------------------------------------------------------------------------");
	distributeErrors(errorMap);
	int LPErrors = 0, STErrors = 0, diff=0;
	for (auto const&[error, count] : errorMap)
	{
		if (error.find(u"LP correct") != lpwstring::npos)
			STErrors += count;
		else if (error.find(u"ST correct") != lpwstring::npos)
			LPErrors += count;
		else if (error.find(u"diff") != lpwstring::npos)
			diff += count;
		errorReverseMap[count] += error + u" *";
	}
	if (LPErrors + STErrors + diff > 0)
	{
		lplog(LOG_ERROR, u"LP errors: %d %d%% ST errors=%d %d%% diff=%d %d%%", LPErrors, 100 * LPErrors / (LPErrors + STErrors + diff), STErrors, 100 * STErrors / (LPErrors + STErrors + diff), diff, 100 * diff / (LPErrors + STErrors + diff));
		for (auto const&[count, multierror] : errorReverseMap)
		{
			for (auto LPSTErr:splitString(multierror, u'*'))
				lplog(LOG_ERROR, u"%07d:%s", count, LPSTErr.c_str());
		}
	}
	if (totalWords > 0)
		lplog(LOG_ERROR, u"numNoMatch=%d/%d %6.3f%% numTotalDifferenceFromStanford=%d", numNoMatch, totalWords, numNoMatch * 100.0 / totalWords, numTotalDifferenceFromStanford);
	return 0;
}

// must run update sources set proc2=100 for each source to multithread
// Shard proc2==step into MP buckets (proc2 = 101..100+MP) and spawn
// x64\StanfordParseMT\CorpusAnalysis.exe -step N.  The in-process
// std::async attempt is commented out.  pcfg is unused.
int stanfordCheckMP(cSource source, int step, bool pcfg, int MP)
{
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&source.mysql, u"LOCK TABLES sources WRITE, sources rs WRITE, sources rs2 WRITE"))
		return -1;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"update sources,(select id,ROW_NUMBER() over w rn from sources rs2 where proc2=%d WINDOW w AS (ORDER BY id)) rs set proc2=(rs.rn%%%d)+101 where sources.id=rs.id", step,MP);
	if (!myquery(&source.mysql, qt))
	{
		unlockTables(source.mysql);
		return -1;
	}
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
	unlockTables(source.mysql);
	if (chdir("source") < 0)
		return -1;
	// Batch B4b: same argv + pid_t + lpWaitForAnyChildProcess conversion as the
	// controller above; "x64\\StanfordParseMT\\CorpusAnalysis.exe" becomes the
	// CorpusAnalysis binary sitting next to this one.
	int numProcesses = MP;
	pid_t *childPids = (pid_t *)calloc(numProcesses, sizeof(pid_t));
	if (!childPids)
		lplog(LOG_FATAL_ERROR, u"could not allocate the child process table for -MP %d", numProcesses);
	for (int I = 0; I < numProcesses; I++)
	{
		std::vector<std::string> arguments = {
			lpSiblingExecutablePath("CorpusAnalysis"),
			"-step", std::to_string(I + step + 1)
		};
		pid_t processId = 0;
		if (lpSpawnProcess(processId, arguments) < 0)
			break;
		childPids[I] = processId;
	}
	while (numProcesses)
	{
		int exitStatus = 0;
		int exitedIndex = lpWaitForAnyChildProcess(childPids, numProcesses, 1000 * 60 * 5, exitStatus);
		if (exitedIndex == LP_WAIT_TIMEOUT)
			continue;
		if (exitedIndex == LP_WAIT_FAILED)
			break;
		printf("\nClosing process %d", exitedIndex);
		memmove(childPids + exitedIndex, childPids + exitedIndex + 1, (MP - exitedIndex - 1) * sizeof(childPids[0]));
		numProcesses--;
	}
	free(childPids);
	return 0;
}

// Single-path Stanford check (step 70: tests\thatParsing.txt).  Prints the
// same form/word/error summaries as stanfordCheck.
int stanfordCheckTest(cSource source, lpwstring path, int sourceId, bool pcfg,lpwstring limitToWord,int maxSentenceLimit, lpwstring specialExtension)
{
	if (limitToWord.length() > 0)
		printf("limited to %S!\n", limitToWord.c_str());
	JavaVM *vm;
	JNIEnv *env;
	createJavaVM(vm, env);
	unordered_map<lpwstring, int> formNoMatchMap, formMisMatchMap, wordNoMatchMap, VFTMap, errorMap, comboCostFrequency ;
	int numNoMatch = 0, numPOSNotFound = 0, numTotalDifferenceFromStanford=0;
	stanfordCheckFromSource(source, sourceId, path, vm, env, numNoMatch, numPOSNotFound, numTotalDifferenceFromStanford, formNoMatchMap, formMisMatchMap, wordNoMatchMap, VFTMap, errorMap, comboCostFrequency, pcfg,limitToWord,maxSentenceLimit,specialExtension,true);
	int totalWords = source.m.size();
	destroyJavaVM(vm);
	if (limitToWord.length() > 0)
		return 0;
	if (totalWords > 0)
		lplog(LOG_ERROR, u"numNoMatch=%d/%d %7.3f%% numPOSNotFound=%d", numNoMatch, totalWords, numNoMatch * 100.0 / totalWords, numPOSNotFound);
	lplog(LOG_ERROR, u"WORD AGREE DISTRIBUTION ------------------------------------------------------------");
	map <double, lpwstring> agreeCountMap;
	for (auto &&[word, fd] : formDistribution)
	{
		agreeCountMap[((double)fd.agreeSTLP) / (fd.agreeSTLP + fd.disagreeSTLP)] = word;
	}
	int limit = 0;
	int maxDiff = -1;
	lpwstring maxForm, maxWord;
	for (auto &&[adp, word] : agreeCountMap)
	{
		printFormDistribution(word, adp, formDistribution[word], maxWord, maxForm, maxDiff,limit);
		if (limit++ > 1000)
			break;
	}
	lplog(LOG_ERROR, u"FORMS ------------------------------------------------------------------------------");
	map<int, lpwstring, std::greater<int>> formNoMatchReverseMap, wordNoMatchReverseMap, VFTReverseMap, errorReverseMap;
	for (auto const&[forms, count] : formNoMatchMap)
		formNoMatchReverseMap[count] += forms + u" *";
	for (auto const&[count, forms] : formNoMatchReverseMap)
		lplog(LOG_ERROR, u"forms %s [%d]", forms.c_str(), count);
	lplog(LOG_ERROR, u"WORDS ------------------------------------------------------------------------------");
	for (auto const&[forms, count] : wordNoMatchMap)
		wordNoMatchReverseMap[count] += forms + u" *";
	int numListed = 0;
	for (auto const&[count, forms] : wordNoMatchReverseMap)
	{
		lplog(LOG_ERROR, u"words %s [%d]", forms.c_str(), count);
		if (numListed++ > 100)
			break;
	}
	lplog(LOG_ERROR, u"DIFF ANALYSIS ------------------------------------------------------------------------");
	int LPErrors = 0, STErrors = 0, diff = 0;
	for (auto const&[error, count] : errorMap)
	{
		if (error.find(u"LP correct") != lpwstring::npos)
			STErrors += count;
		else if (error.find(u"ST correct") != lpwstring::npos)
			LPErrors += count;
		else if (error.find(u"diff") != lpwstring::npos)
			diff += count;
		errorReverseMap[count] += error + u" *";
	}
	if (LPErrors + STErrors + diff > 0)
	{
		lplog(LOG_ERROR, u"LP errors: %d %d%% ST errors=%d %d%% diff=%d %d%%", LPErrors, 100 * LPErrors / (LPErrors + STErrors + diff), STErrors, 100 * STErrors / (LPErrors + STErrors + diff), diff, 100 * diff / (LPErrors + STErrors + diff));
		for (auto const&[count, multierror] : errorReverseMap)
		{
			for (auto LPSTErr : splitString(multierror, u'*'))
				lplog(LOG_ERROR, u"%07d:%s", count, LPSTErr.c_str());
		}
	}
	return 0;
}

// this would be easier if you had all the sources in memory at once!
// Concatenate every finished Gutenberg source with proc2==step into `source`
// (via copySource), then testViterbiFromSource.  childSource is a second
// cSource used only as a load buffer.
int testViterbiHMMMultiSource(cSource &source,const lpchar_t *databaseHost,int step, lpwstring specialExtension)
{
	MYSQL_RES * result;
	MYSQL_ROW sqlrow = NULL;
	enum cSource::sourceTypeEnum st = cSource::GUTENBERG_SOURCE_TYPE;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numSourcesProcessedNow = 0;

	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select id, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id", st, step);
	if (!myquery(&source.mysql, qt, result))
		return -1;
	my_ulonglong totalSource = mysql_num_rows(result);
	//Generate vocabulary
	cSource childSource(databaseHost, st, false, false, true);
	childSource.initializeNounVerbMapping();
	if (!myquery(&source.mysql, u"LOCK TABLES words WRITE, words w WRITE, words mw WRITE,wordForms wf WRITE"))
		return -1;
	for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
	{
		int sourceId =atoi(sqlrow[0]);
		lpwstring path, title;
		mTW(sqlrow[1], path);
		mTW(sqlrow[2], title);
		path.insert(0, u"\\").insert(0, CACHEDIR);
		bool parsedOnly = false;
		Words.readWords(path, sourceId, false, u"");
		//unordered_map <int, vector < vector <cTagLocation> > > emptyMap;
		//for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
		//	childSource.pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
		if (childSource.readSource(path, false, parsedOnly, false, specialExtension))
		{
			lplog(LOG_ERROR,u"Beginning child source %d:%s at offset %d.", sourceId,path.c_str(), source.m.size());
			unordered_map <int, int> sourceIndexMap;
			source.copySource(&childSource, 0, childSource.m.size(), sourceIndexMap);
		}
		else
		{
			lp_wprintf(u"Unable to read source %d:%s\n", sourceId,path.c_str());
		}
		childSource.clearSource();
		lpchar_t buffer[1024];
		int64_t processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		numSourcesProcessedNow++;
		if (processingSeconds)
			lp_wsprintf(buffer, u"%%%03I64d:%5d out of %05I64d sources in %02I64d:%02I64d:%02I64d [%d sources/hour] (%-35.35s... finished)", numSourcesProcessedNow * 100 / totalSource, numSourcesProcessedNow, totalSource,
				processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, title.c_str());
		lpReportProgress(buffer);
	}
	mysql_free_result(result);
	int wordNum = 0;
	for (cWordMatch im : source.m)
	{
		if (im.formsSize() == 0)
		{
			lplog(LOG_ERROR, u"Viterbi force form %d:%s", wordNum, im.word->first.c_str());
			im.word->second.addForm(1, im.word->first);
		}
		if (im.getForms().size()==0)
		{
			if (im.flags&cWordMatch::flagOnlyConsiderProperNounForms)
			{
				lplog(LOG_ERROR, u"Viterbi proper noun form mandatory added %d:%s", wordNum, im.word->first.c_str());
				im.word->second.addForm(PROPER_NOUN_FORM_NUM, im.word->first);
			}
			else
			{
				lplog(LOG_ERROR, u"Viterbi illegal proper noun form added noun form %d:%s", wordNum, im.word->first.c_str());
				im.word->second.addForm(nounForm, im.word->first);
			}
		}
		wordNum++;
	}
	lpwstring temp;
	source.sourcePath = TEXTDIR u"/texts/hmmViterbiMultiSource." + itos((int)totalSource, temp);
	testViterbiFromSource(source);
	return 0;
}


int numSourceLimit = 0;
// to begin proc2 field in all sources must be set to 1
// step = 1 - accumulate word frequency statistics - will set to 2 when finished.
// step = 2 - evaluate statistics and create database statements to decrease the number of unknown words
// specials entry.  Unlike main.cpp: no initialize()/crash handler, always
// GUTENBERG, dispatch on -step.  step>100 is stanfordCheck without per-source
// lock.
// Batch B4b: the same wmain->main conversion as main.cpp -- MSVC's wide entry point
// does not exist on POSIX. argv is widened once into never-freed statics and handed
// on with the same lpchar_t*[] shape, so nothing downstream changes.
// setConsoleWindowSize is gone with the rest of the console API (see above).
int lpSpecialsMain(int argc, lpchar_t *argv[]);

int main(int argc, char *argv[])
{
	static std::vector<lpwstring> wideArguments;
	static std::vector<lpchar_t*> wideArgv;
	wideArguments.reserve(argc);
	for (int i = 0; i < argc; i++)
		wideArguments.push_back(lpwstring(lp_narrow_to_wide(std::string(argv[i]))));
	wideArgv.reserve(argc + 1);
	for (int i = 0; i < argc; i++)
		wideArgv.push_back(&wideArguments[i][0]);
	wideArgv.push_back(nullptr);
	return lpSpecialsMain(argc, wideArgv.data());
}

int lpSpecialsMain(int argc,lpchar_t *argv[])
{
	chdir("..");
	initializeCounter();
	cacheDir = CACHEDIR;
	const lpchar_t *databaseHost = u"localhost";
	enum cSource::sourceTypeEnum st = cSource::GUTENBERG_SOURCE_TYPE;
	cSource source(databaseHost, st, false, false, true);
	source.initializeNounVerbMapping();
	initializePatterns();
	cInternet::bandwidthControl = CLOCKS_PER_SEC / 2;
	unordered_map <int, vector < vector <cTagLocation> > > emptyMap;
	for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
		source.pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
	if (!myquery(&source.mysql, u"LOCK TABLES sources READ"))
		return -1;
	lpwstring specialExtension = u"";
	//writeFrequenciesToDB(source);
	//if (true)
	//	return;
	nounForm = cForms::gFindForm(u"noun");
	verbForm = cForms::gFindForm(u"verb");
	adjectiveForm = cForms::gFindForm(u"adjective");
	adverbForm = cForms::gFindForm(u"adverb");
	int step=-1;
	bool actuallyExecuteAgainstDB=false;
	for (int I = 0; I < argc; I++)
	{
		if (!lp_wcscasecmp(argv[I], u"-step") && I < argc - 1)
			step = lp_wtoi(argv[++I]);
		else if (!lp_wcscasecmp(argv[I], u"-stanfordCheck") && I < argc - 1)
		{
			stanfordCheck(source, step, true, specialExtension,true);
			return 0;
		}
		else if (!lp_wcscasecmp(argv[I], u"-specialExtension"))
			specialExtension = argv[++I];
		else if (!lp_wcscasecmp(argv[I], u"-logFileExtension"))
			logFileExtension = argv[++I];
		else if (!lp_wcscasecmp(argv[I], u"-executeAgainstDB"))
			actuallyExecuteAgainstDB = true;
		else
			continue;
	}
	logFileExtension = specialExtension;
	if (step > 100)
	{
		stanfordCheck(source, step, true, specialExtension,false);
		return 0;
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
			unlockTables(source.mysql);;
			int numFilesProcessed = 0, numNotOpenable = 0, numNewestVersion = 0, numOldVersion = 0, removeErrors = 0, populatedRDFs = 0, numERDFRemoved = 0;
			if (!myquery(&source.mysql, u"LOCK TABLES noRDFTypes WRITE, noERDFTypes WRITE"))
				return -1;
			FILE *progressFile = lp_wfopen(u"RDFTypesScanProgress.txt", "r");
			lpchar_t startPath[2048];
			bool startHit = false;
			if (progressFile)
			{
				lp_fgetws(startPath, 2048, progressFile);
				fclose(progressFile);
			}
			else
				startHit = true;
			unordered_map<lpwstring, int> extensions;
			unordered_map<lpwstring, int64_t> extensionSpace;
			scanAllRDFTypes(source.mysql, startPath, startHit, CACHEDIR u"/dbPediaCache", numFilesProcessed, numNotOpenable, numNewestVersion, numOldVersion, removeErrors, populatedRDFs, numERDFRemoved, extensions, extensionSpace);
		}
		break;
	// (Step 4 removed: it swept the Dictionary.com cache, recording words the site
	// had no page for in the notwords table.)
	case 5:
		unlockTables(source.mysql);;
		removeIllegalNames(CACHEDIR u"/dbPediaCache");
		break;
	case 6:
		unlockTables(source.mysql);;
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
		// cSource::TEST_SOURCE_TYPE
		// 0: pattern - if differentiator is NOT specified, then any differentiator is ok.
		// 1: form - any formclass (string)
		// 2: word - the actual word.   
		// 3: flagNotMatched is set
		// 4: true (do not perform match)
		// if both primaryMatchType AND secondaryMatchType>0, then the secondary match location is the NEXT word.
		// if primaryMatchType == 3, then sentence highlight will encompass all words that have no match, and sentences will not be repeated.
		//patternOrWordAnalysis(source, step, u"__S1", u"R*", cSource::GUTENBERG_SOURCE_TYPE,true,specialExtension);
		//patternOrWordAnalysis(source, step, u"__ADJECTIVE", u"MTHAN", cSource::GUTENBERG_SOURCE_TYPE, true, specialExtension);
		//patternOrWordAnalysis(source, step, u"__NOUN", u"F", cSource::GUTENBERG_SOURCE_TYPE, true, specialExtension);
		//patternOrWordAnalysis(source, step, u"__S1", u"5", true);
		//patternOrWordAnalysis(source, step, u"__C1__S1", u"1", u"adjective", u"", cSource::GUTENBERG_SOURCE_TYPE, 0, 1, specialExtension);
		patternOrWordAnalysis(source, step, u"_Q2", u"F", u"__ALLOBJECTS_1", u"*", cSource::GUTENBERG_SOURCE_TYPE, 0, 0, specialExtension);
		//patternOrWordAnalysis(source, step, u"", u"", cSource::GUTENBERG_SOURCE_TYPE, false, specialExtension);
		// scans the test file for any unmatched sentences
		//patternOrWordAnalysis(source, step, u"", u"", u"", u"", cSource::TEST_SOURCE_TYPE, 3,4,u""); // TODO: testing weight change on _S1.
		break;
	case 60:
		stanfordCheck(source, step, true,specialExtension,true);
		break;
	case 61:
		// for test, set 27568 to 61
		syntaxCheck(source, step,specialExtension,true);
		break;
	case 70:
		stanfordCheckTest(source, LMAINDIR u"/tests/thatParsing.txt", 27568, true,u"",50,specialExtension);
		break;
	// (Step 71 removed: it was a Merriam-Webster plural-word probe.)
	case 100:
		stanfordCheckMP(source, step, true,12);
		break;
	}
	unlockTables(source.mysql);;
}


