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
enum LogLevels { LOG_INFO=1, LOG_ERROR=2, LOG_RESOLUTION=4, LOG_NOTMATCHED=8, LOG_WHERE=128, LOG_RESCHECK=256, 
								 LOG_SG=512, LOG_WORDNET=1024, LOG_WIKIPEDIA=2048, LOG_WEBSEARCH=4096,
								 LOG_ROLE=8192, LOG_WCHECK=16384, LOG_TIME=32768, LOG_DICTIONARY=65536, LOG_FATAL_ERROR=131072, LOG_PROFILER=262144, 
								 LOG_QCHECK=LOG_PROFILER<<1, LOG_STDOUT = LOG_QCHECK << 1
};
//#define LOG_WORD_FLOW
extern int logDatabaseDetails;
int lplog(void);
int lplog(const wchar_t *format,...);
int lplog(int logLevel,const wchar_t *format,...);
int lplogNR(int logLevel,const wchar_t *format,...);
int logstring(int logLevel,const wchar_t *s);
#define SCREEN_WIDTH 280
struct sTrace 
{
	bool traceTime,traceTags,traceWhere;
	bool traceWikipedia,traceWebSearch,traceQCheck;
	bool printBeforeElimination,traceSubjectVerbAgreement, traceTestSubjectVerbAgreement, traceEVALObjects,traceAnaphors,traceRelations,traceSpeakerResolution,traceNyms,traceRole;
	bool traceObjectResolution,traceVerbObjects,traceDeterminer,traceBNCPreferences,tracePatternElimination,traceNameResolution;
	bool traceSecondaryPEMACosting,traceMatchedSentences,traceUnmatchedSentences,traceIncludesPEMAIndex;
	bool traceTransitoryQuestion,traceConstantQuestion,traceMapQuestion,traceCommonQuestion,traceQuestionPatternMap;
	bool traceTagSetCollection,collectPerSentenceStats,traceParseInfo, tracePreposition, tracePatternMatching;
};

extern int logQuestionProfileTime;
extern int logSynonymDetail;
extern int logTableDetail;
extern int equivalenceLogDetail;
extern int logDetail;
extern int logSemanticMap;
extern int rdfDetail;
extern bool log_net;  
extern bool logTraceOpen;

extern __declspec(thread) wstring logFileExtension; // parallel processing will overload this variable 
extern __declspec(thread) int multiProcess; // initialized
