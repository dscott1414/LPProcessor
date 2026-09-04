/*
	logging.h - process-wide log-level flags, lplog entry points, and the sTrace diagnostic-switch bag

	Overview:
		Every translation unit includes this (usually via word.h) for the lplog() family and
		the LOG_* bit flags.  Feature #defines at the top of the file are compile-time
		switches that change what the rest of the program writes to the database and to
		disk caches; they are not log-routing flags.  The LogLevels enum IS the routing
		mask: each bit selects a destination file (main.lplog, error.lplog, ...).

	Pipeline position:
		Used from initialization through QA.  lplog is the only sanctioned way to write a
		diagnostic; LOG_FATAL_ERROR is the sanctioned way to abort.

	Key entry points:
		- lplog() / lplog(format,...) / lplog(logLevel,format,...) - format and write.
		- lplogNR() - same as lplog(logLevel,...) but does not append a newline.
		- logstring() - the sink: opens/buffers the per-level FILE*, writes, and on
			LOG_FATAL_ERROR calls fatalExit() (exits EXIT_FAILURE; waits for a
			keypress only when interactive).

	Key data structures / globals:
		- sTrace - per-document bag of bools that gate expensive resolution/pattern traces.
		- logFileExtension / multiProcess - TLS; child processes set the extension so each
			writes "main<N>.lplog" under "multiprocessor logs\\".
		- logCache - seconds a FILE* is kept open (40 by default); 0 means close after every write.

	Notes / gotchas:
		- lplog(LOG_FATAL_ERROR,...) aborts: logstring() calls fatalExit(), which exits
			EXIT_FAILURE and only waits for a keypress when interactive (multiProcess==0
			and stdin is a tty).  source.h, main.cpp, and DBUtility.cpp all document this
			correctly.  Unattended runs (a -mp child) exit immediately with a non-zero
			status; they do not hang waiting on stdin.
		- LOG_FATAL_ERROR is also treated as LOG_INFO for file routing (writes main.lplog)
			and lplog() ORs in LOG_ERROR, but logstring() exits before the error-file pass.
		- lplogNR is "no newline" (it skips the wcscat u"\\n"), not "no return" - both
			lplog and lplogNR abort on FATAL via logstring().
		- When LOG_BUFFER is defined (it is), the FILE* handles are TLS, same as
			logFileExtension, so two threads never share the same handle.
*/
#pragma once
// Batch B3: general.h now includes this header for sTrace, and several .cpp files
// already include it directly as well -- so it needs an include guard, which it
// never had (nothing included it twice before).
// Batch B2: logging.h has no #includes of its own and is (per its own header
// comment above) included from word.h BEFORE general.h -- so it cannot rely on
// general.h having already pulled in lpchar.h by the time lplog()'s own
// lpchar_t-typed declarations below are parsed. Self-sufficient fix: include it
// directly here rather than depend on caller include order (this is what
// actually surfaced the bug: envConfig.cpp reaches logging.h without going
// through word.h at all, and every OTHER file that reaches windows.h first
// masked the same latent issue behind a fatal "windows.h not found" error).
#include "lpchar.h"
//#define LOG_RELATIVE_LOCATION
//#define LOG_OLD_MATCH
#define LOG_BUFFER
//#define LOG_RELATION_GROUPING
//#define LOG_BNC_PATTERNS_CHECK
//#define LOG_FORM_USAGE_COSTS
//#define LOG_UNUSED_TAGS
//#define LOG_DICTIONARY

// diagnostic messages about patterns as they are matched
#define LOG_PATTERN_MATCHING
// log patterns before they are used
//#define LOG_PATTERNS
//#define LOG_OVERMATCHED
//#define LOG_QUOTATIONS
// verify dictionary routines 
//#define CHECK_WORD_CACHE
// cost reductions from patterns matching in the 2nd and subsequent parsing phases
//#define LOG_PATTERN_COST_CHECK
// tracks when words go in and out of the database especially with respect to mainEntryIds
// useful when dictionary becomes corrupt or in parallel processing
//#define LOG_WORD_FLOW
// all array classes check their indicies
//#define INDEX_CHECK
// debug better way of recursively matching patterns
//#define LOG_IPATTERN
// print chains within PEMA structure as they are built
//#define LOG_PATTERN_CHAINS
// update database with new objects
//#define WRITE_OBJECTS_TO_DB
// update database with new forms, words, wordforms
//#define WRITE_WORDS_AND_FORMS_TO_DB
// update database with word relations etc  (if database is written, test results are not reproducible)
//#define WRITE_WORD_RELATIONS
// update database with multi word relations etc  (if database is written, multiple copies of the data will be written)
//#define WRITE_MULTI_WORD_RELATIONS
// use wordFormCache file in main directory
#define USE_TEST_CACHE
// write wordCache file for each source file
#define WRITE_WORD_CACHE
// exit without checking for next source (avoids useless updating after processing test document)
#if defined(USE_TEST_CACHE) && !defined(WRITE_WORDS_AND_FORMS_TO_DB) && !defined(WRITE_OBJECTS_TO_DB) && !defined(WRITE_WORD_RELATIONS) && !defined(WRITE_WORD_CACHE)
    #define EXIT_WITHOUT_UPDATE
#endif
// group words based on their word relations
//#define ACCUMULATE_GROUPS
// enables reading and writing of patterns based on ABNF
//#define ABNF
// checks to verify that the agreement check does not cause too much wasted processing
//#define LOG_AGREE_PATTERN_EVALUATION
//#define LOG_PATTERN_STATISTICS
// Only used when evaluating all the possible combinations of tagsets across all the patterns
// useful to detect when a pattern causes incorrect combinations of tags to be reflected upward
// like two VERBS or two SUBJECTS.
//#define LOG_GENERATE_PATTERNS
extern short logCache;
// Bitmask destinations for lplog.  A call may OR several bits; logstring() peels
// one bit per loop iteration and writes that level's file.  Values are not a
// dense sequence (LOG_WHERE starts at 128) because older bits were retired.
enum LogLevels { LOG_INFO=1, LOG_ERROR=2, LOG_RESOLUTION=4, LOG_NOTMATCHED=8, LOG_WHERE=128, LOG_RESCHECK=256, 
								 LOG_SG=512, LOG_WORDNET=1024, LOG_WIKIPEDIA=2048, LOG_WEBSEARCH=4096,
								 LOG_ROLE=8192, LOG_WCHECK=16384, LOG_TIME=32768, LOG_DICTIONARY=65536, LOG_FATAL_ERROR=131072, LOG_PROFILER=262144, 
								 LOG_QCHECK=LOG_PROFILER<<1, LOG_STDOUT = LOG_QCHECK << 1
};
//#define LOG_WORD_FLOW
extern int logDatabaseDetails;
int lplog(void);
int lplog(const lpchar_t *format,...);
int lplog(int logLevel,const lpchar_t *format,...);
int lplogNR(int logLevel,const lpchar_t *format,...);
int logstring(int logLevel,const lpchar_t *s);
#define SCREEN_WIDTH 280
// Per-document (copied onto cSource::debugTrace) switches that gate the expensive
// resolution / pattern / Wikipedia traces.  Default-constructed to all-false so a
// production parse is quiet; main.cpp flips subsets from the command line.
class sTrace 
{
public:
	bool traceTime,traceTags,traceWhere;
	bool traceWikipedia,traceWebSearch,traceQCheck;
	bool printBeforeElimination, traceSubjectVerbAgreement, traceTestSubjectVerbAgreement, traceEVALObjects, traceAnaphors, traceRelations,traceTestSyntacticRelations;
	bool traceSpeakerResolution, traceNyms, traceRole;
	bool traceObjectResolution,traceVerbObjects,traceDeterminer,traceBNCPreferences,tracePatternElimination,traceNameResolution;
	bool traceSecondaryPEMACosting,traceMatchedSentences,traceUnmatchedSentences,traceIncludesPEMAIndex;
	bool traceTransformDestinationQuestion,traceMapQuestion,traceQuestionPatternMap,traceLinkQuestion;
	bool traceTagSetCollection,collectPerSentenceStats,traceParseInfo, tracePreposition, tracePatternMatching;
	// Every flag starts false.  Add new bools here AND in this constructor.
	sTrace()
	{
		traceTime = false;
		traceTags = false;
		traceWhere = false;
		traceWikipedia = false;
		traceWebSearch = false;
		traceQCheck = false;
		printBeforeElimination = false;
		traceSubjectVerbAgreement = false;
		traceTestSubjectVerbAgreement = false;
		traceEVALObjects = false;
		traceAnaphors = false;
		traceRelations = false;
		traceTestSyntacticRelations = false;
		traceSpeakerResolution = false;
		traceNyms = false;
		traceRole = false;
		traceObjectResolution = false;
		traceVerbObjects = false;
		traceDeterminer = false;
		traceBNCPreferences = false;
		tracePatternElimination = false;
		traceNameResolution = false;
		traceSecondaryPEMACosting = false;
		traceMatchedSentences = false;
		traceUnmatchedSentences = false;
		traceIncludesPEMAIndex = false;
		traceTransformDestinationQuestion = false;
		traceMapQuestion = false;
		traceQuestionPatternMap = false;
		traceLinkQuestion = false;
		traceTagSetCollection = false;
		collectPerSentenceStats = false;
		traceParseInfo = false;
		tracePreposition = false;
		tracePatternMatching = false;
	}
};

extern int logQuestionProfileTime;
extern int logSynonymDetail;
extern int logTableDetail;
extern int logEquivalenceDetail;
extern int logDetail;
extern int logQuestionDetail;
extern int logTableCoherenceDetail;
extern int logProximityMap;
extern int logRDFDetail;
extern bool log_net;  
extern bool logTraceOpen;

extern thread_local lpwstring logFileExtension; // parallel processing will overload this variable
// 0 = single-process (write next to cwd).  Non-zero child workers also set
// logFileExtension so logstring() prefixes "multiprocessor logs\\".
extern thread_local int multiProcess; // initialized
