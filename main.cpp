/*
	main.cpp - process entry point: command line parsing, one-time initialization, multi-process orchestration and the top-level per-document loop.

	Overview:
		This translation unit owns everything that happens around the parser proper.  It
		(a) installs the crash / Ctrl-C / out-of-memory handlers, (b) parses the command
		line into the handful of booleans and the source type that drive a run,
		(c) constructs the single cSource object (which connects to MySQL and loads the
		lexicon), (d) either spawns and supervises N child lp processes (controller
		mode, -mp) or itself loops over the sources claimed from the `sources` table, and
		(e) for each source calls the pipeline in order: readSource/tokenize -> print
		(match) sentences -> WordNet extensions -> identifyObjects -> analyzeWordSenses ->
		syntacticRelations -> identifySpeakerGroups -> resolveSpeakers ->
		resolveFirstSecondPersonPronouns -> identifyConversations -> (optionally)
		question answering.  It also holds the definitions of a number of program-wide
		globals (the "unreachable but stable" iterator sentinels, the cProfile statics,
		and the shared_mutex locks) that must exist in exactly one translation unit.

	Pipeline position:
		Stage 0 - runs before and around every other stage.  lpMain() is the only caller of
		processSource(), which is the linear script of the whole pipeline described in
		README.md "Parsing Processing Stages".

	Command line (as actually implemented by processCommandArguments(); note that
	README.md "Command Line" is stale in places - see the notes below):
		Mode selectors (must appear before any switch, and are matched case-insensitively
		as a substring of the internal table
		"1-test 2-book 3-newsbank 4-bnc 5-script 6-websearch 7-wikipedia 8-interactive :-parserequest",
		the leading digit being the cSource::sourceTypeEnum value):
			-Test   [name|#begin] [#end|+|~start]  tests\<name>.txt, or a numeric range of TEST_SOURCE_TYPE rows
			-Book   [#begin] [#end|+]              Project Gutenberg novels (GUTENBERG_SOURCE_TYPE)
			-Newsbank [#begin] [#end|+]            NewsBank articles
			-BNC    [#begin] [#end|+]              British National Corpus (pre-tagged; processing itself now lives in getBNC.cpp)
			-Script [#begin] [#end|+]              movie scripts
			-WebSearch [#begin] [#end|+]           web search fragments collected for question answering
			-Wikipedia [#begin] [#end|+]           Wikipedia articles
			-Interactive [#begin] [#end|+]         interactive question answering session
			-ParseRequest [#begin] [#end|+]        REQUEST_TYPE - the mode child processes are spawned with by createLPProcess(processKind==0)
		A '+' in the second position means "to the end of the table"; a number means
		"up to but not including it"; anything else means "just this one source".
		Switches (all optional, order independent, all must follow the mode selector):
			-server <host>            MySQL host (default localhost)
			-log <suffix>            suffix for every *.lplog file; also redirects them into "multiprocessor logs\"
			-mp <n>                  controller mode: run n child lp.exe processes instead of parsing here
			-numSourcesPerProcess <n> how many sources each child should take before exiting (default 5)
			-numSourceLimit <n>      stop after n sources (0 = no limit)
			-cacheDir <path>         root of the text/websearch/wikipedia/wordnet caches
			-resetAllSource          mark every source unprocessed and not in processing
			-resetProcessingFlags    clear only the 'processing' flags
			-generateFormStatistics  log word form statistics while reading the lexicon
			-retry                   reset the selected source range before processing it
			-parseOnly               stop after syntacticRelations (no speakers/conversations)
			-LC | -logCache <n>      seconds of log buffering before flush
			-BC <n>                  internet bandwidth control: minimum seconds between requests
			-logMatchedSentences     log sentences that got a fully enclosing parse
			-logUnmatchedSentences   log sentences that did not
			-TSRO                    force speaker resolution tracing (currently only read by unused source\relations.cpp)
			-fTOR                    flip object resolution tracing (identifyObjects.cpp)
			-fTNR                    flip name resolution tracing (currently unreferenced)
			-forceSourceReread       ignore the parsed-source cache file and retokenize/reparse
			-SW                      write the parsed source cache file
			-MCSW                    keep a copy of the previous cache file before overwriting it
			-SWNR / -SWNW            read / write the WordNet map cache file
			-specialExtension <ext>  extra suffix for the source/word cache files (also used as the child -log suffix)
		Documented in README.md but NOT implemented here: -tg, -acquireNewsBank,
		-acquireMovieList, -acquireInterviewTranscript, -acquireTwitter,
		-flipTMSOverride, -flipTUMSOverride, -sourceRead, -sourceWrite, -C.  The
		corresponding code is either commented out below or lives in "unused source\".

	Key entry points:
		- main() - widens argv, then lpMain(): initialize, parse arguments, validateCacheDir(), build cSource, then either startProcesses() or loop over sources
		- processCommandArguments() - fills the run configuration and the source type from argv
		- initialize() - signal handlers, locks, memory counter, working directory, default cache directory
		- validateCacheDir() - fatal existence check on whichever cacheDir ended up in effect (default, LP_CACHE_DIR, or -cacheDir); runs after processCommandArguments() so a CLI override is actually what gets checked
		- processSource() - the whole per-document pipeline for one already-claimed source
		- startProcesses() / createLPProcess() / waitToSpawnMoreProcesses() / waitForSpawnedProcesses() - controller mode
		- WRMemoryCheck() - copies the wordRelations table into its MEMORY-engine mirror
		- printStackTrace() / crashHandler() / interruptHandler() - crash and interrupt diagnostics
		- lpReportProgress() - the single progress-status helper (terminal title via OSC 0)
		- lpSiblingExecutablePath() - locates the lp / CorpusAnalysis binary next to this one

	Key data structures / globals:
		- static_wordMap / wNULL, static_tIcMap / tNULL, static_cLocalFocus / cNULL,
		  static_cSpeakerGroup / sgNULL, static_wm / wmNULL, static_setInt / sNULL -
		  the "never dereferenced but always comparable" iterator sentinels.  They exist
		  because _STLP_DEBUG rejects default-constructed iterators; the containers must
		  stay empty and must never be mutated.
		- cProfile:: statics - the profiling accumulators used by the LFS macro.
		- exitNow / exitEventually - set by interruptHandler (a signal handler) and polled
		  by the source loop and by processSource(); one Ctrl-C finishes the current
		  source, a second one abandons it.
		- rdfTypeMapSRWLock, mySQLTotalTimeSRWLock, totalInternetTimeWaitBandwidthControlSRWLock,
		  mySQLQueryBufferSRWLock, orderedHyperNymsMapSRWLock - the process-wide locks
		  declared extern in general.h; defined here, initialized by createLocks().
		- TSROverride / flipTOROverride / flipTNROverride / logMatchedSentences /
		  logUnmatchedSentences / preTaggedSource / numSourceLimit - command line flags
		  read by other translation units (general.h).

	Dependencies:
		MySQL (the `sources` table drives the work queue; wordRelations /
		wordRelationsMemory for the in-memory word relation mirror), the on-disk caches
		under CACHEDIR, the source texts under TEXTDIR, cInternet (acquisition), yajl
		and MusicBrainz headers, and the sibling build outputs `lp` and `CorpusAnalysis`
		(in this executable's own directory) which controller mode spawns.

	Notes / gotchas:
		- Batch B4a replaced the Win32 process/console/crash layer: CreateProcess ->
		  posix_spawn, WaitForMultipleObjectsEx -> waitForAnyChildProcess (waitpid),
		  GenerateConsoleCtrlEvent -> kill(SIGINT), SetConsoleTitle -> reportProgress,
		  SetUnhandledExceptionFilter/SetConsoleCtrlHandler -> sigaction, minidumps ->
		  the OS crash reporter plus a logged backtrace().
		- LMAINDIR / CACHEDIR / TEXTDIR (general.h) are only the fallback values used
		  by envConfig.h's getMainDir()/getCacheDir()/getTextDir() when LP_MAIN_DIR /
		  LP_CACHE_DIR / LP_TEXT_DIR are unset. cacheDir defaults to getCacheDir() in
		  initialize(), -cacheDir can still override it in processCommandArguments(),
		  and validateCacheDir() (called after argument parsing) is what actually does
		  the fatal existence check - it used to run inside initialize(), before argv
		  was parsed, so a valid -cacheDir was never the value being checked.
		- Working directory dance: initialize() does chdir(".."), startProcesses() does
		  chdir("source") and restores it on return; every relative path below (tests\,
		  the .lplog files) depends on this.  The child executable paths no longer do -
		  they are absolute since batch B4a.
		- Initialization order is load bearing: cSource's constructor connects to MySQL
		  and reads the lexicon, initializePatterns() fills desiredTagSets, and
		  initializePemaMap() must run after it.  initializePatterns() is skipped in
		  controller mode (-mp), so desiredTagSets is empty there.
		- lplog(LOG_FATAL_ERROR,...) does not return: logging.cpp exits EXIT_FAILURE,
		  waiting for a keypress only when interactive.  Child exit codes are reported
		  by reportChildExitCode() as each worker is reaped.
		- lpMain() and startProcesses() end with _exit(0), so no destructor runs: the
		  cSource destructor (which would tear down the in-memory lexicon, taking
		  minutes) is skipped deliberately ("fast exit").  Both call sites explicitly
		  lplog() (flush) and mysql_close() immediately before the _exit(0), though,
		  since those are cheap and _exit() - unlike exit() - does not flush stdio
		  buffers or close the DB socket on its own.
		- multiProcess and logFileExtension are thread_local (logging.h); they are
		  set on the main thread only, so worker threads see multiProcess==0.
		- This file is largely duplicated by specials_main.cpp (the specials.vcxproj
		  entry point), including getNumSourcesProcessed() and the lock definitions.
*/
// Batch B4a: windows.h, io.h, winhttp.h, direct.h, crtdbg.h and Dbghelp.h are gone,
// along with stacktrace.h -- that last one is a vendored Win32-only dbghelp stack
// walker, and main.cpp was its only consumer, so it is now unused by this build
// (the file is left on disk for the Windows project, which this port does not
// maintain). What replaces them: unistd.h/sys/wait.h/spawn.h/signal.h for the
// process and interrupt work, execinfo.h for the crash-time backtrace,
// mach-o/dyld.h for locating our own executable.
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <execinfo.h>
#include <mach-o/dyld.h>
#include <errno.h>
#include <limits.h>
#include <sstream> // batch B4a: printStackTrace's std::stringstream, previously reaching us via windows.h
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
#include "internet.h"
#include "QuestionAnswering.h"
#include "utfConvert.h"
#include "lpProcess.h"

// needed for _STLP_DEBUG - these must be set to a legal, unreachable yet never changing value
unordered_map <lpwstring, cSourceWordInfo> static_wordMap;
tIWMM wNULL = static_wordMap.begin();
unordered_map<lpwstring, cSourceWordInfo::cRMap::cRelation> static_tIcMap;
cSourceWordInfo::cRMap::tIcRMap tNULL = (cSourceWordInfo::cRMap::tIcRMap)static_tIcMap.begin();
vector <cLocalFocus> static_cLocalFocus;
vector <cLocalFocus>::iterator cNULL = static_cLocalFocus.begin();
vector <cSource::cSpeakerGroup> static_cSpeakerGroup;
vector <cSource::cSpeakerGroup>::iterator sgNULL = (vector <cSource::cSpeakerGroup>::iterator)static_cSpeakerGroup.begin();
vector <cWordMatch> static_wm;
vector <cWordMatch>::iterator wmNULL = static_wm.begin();
set<int> static_setInt;
set<int>::iterator sNULL = static_setInt.begin();

// profiling
int64_t cProfile::cb;
int64_t cProfile::accumulatedOverheadTime = 0;
unordered_map <string, int64_t > cProfile::counterMap;
unordered_map <string, int > cProfile::counterNumMap;
unordered_map <string, cProfile::CP> cProfile::timeMapTotal;
int64_t cProfile::totalCount = 0;
string cProfile::functionPath;
set <unordered_map <string, cProfile::CP>::iterator, cProfile::timeSetCompare> cProfile::timeSort; // sort map by time taken by function
set <unordered_map <string, cProfile::CP>::iterator, cProfile::memorySetCompare> cProfile::memorySort; // sort map by memory allocated by function
set <unordered_map <string, cProfile::CP>::iterator, cProfile::countSetCompare> cProfile::countSort; // sort map by number of times function is called
int64_t cProfile::mySQLTotalTime = 0;
std::shared_mutex cProfile::networkTimeSRWLock;
int cProfile::totalInternetTimeWaitBandwidthControl;
int64_t cProfile::accumulationNetworkProfileTimer;
int64_t cProfile::accumulateOnlyNetTimer;
int64_t cProfile::lastNetworkTimePrinted;
int64_t cProfile::accumulateNetworkTimeCount;
int cProfile::lastNetClock;
int cInternet::internetWebSearchRetryAttempts = 1;
bool cQuestionAnswering::fileCaching = true;  // fileCaching determines whether they are cached on disk.  cOntology::cacheRdfTypes determines whether rdfTypes are cached in memory.  
unordered_map < lpwstring, int64_t > cProfile::netAndSleepTimes, cProfile::onlyNetTimes, cProfile::numTimesPerURL;


bool unlockTables(MYSQL& mysql);
bool preTaggedSource = false; // BNC

// Batch B4a: createMinidump() is deleted rather than ported. It wrote a Windows
// .dmp through dbghelp; macOS already writes a full crash report for every
// abnormal termination to ~/Library/Logs/DiagnosticReports/, which is strictly
// more information than the fixed-name core.dmp this produced -- and because that
// name was fixed, concurrent -mp children overwrote each other's dumps anyway. If
// a core file is wanted as well, `ulimit -c unlimited` is the macOS answer, not
// application code.

// Format the current call stack and write it to the log at LOG_FATAL_ERROR.
// Because LOG_FATAL_ERROR is fatal in logging.cpp, this call does not return: it
// flushes, waits for a keypress on stdin (only when interactive) and exits.
//
// Batch B4a: backtrace()/backtrace_symbols() replace stacktrace.h's dbg::stack_trace().
// The frame format is what the platform gives us -- "module address symbol + offset"
// -- rather than the old "0xADDRESS: name(line) in module"; file and line numbers
// are not available without a symbolizer, so `atos -p <pid> <address>` (or running
// the binary under lldb) is how a frame becomes a source line now.
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
		// backtrace_symbols mallocs, which can itself fail in a crashed process;
		// raw addresses are still enough for `atos` to resolve after the fact.
		for (int i = 0; i < numFrames; i++)
			buff << "0x" << std::hex << (uintptr_t)frames[i] << std::dec << "\n";
	::lplog(LOG_FATAL_ERROR, u"%S", buff.str().c_str());
}

// Batch B4a: replaces the SetUnhandledExceptionFilter/unhandled_handler pair. The
// four signals below are the POSIX equivalents of the structured exceptions that
// filter existed to catch (access violation, bus error, integer division by zero,
// illegal instruction).
//
// Honest caveat, unchanged in spirit from the Windows original: printStackTrace()
// reaches lplog(), which allocates and uses stdio, and none of that is
// async-signal-safe. Doing it anyway is the deliberate trade every crash reporter
// makes -- the process is already dying, and a logged stack trace is worth far
// more than the guarantee we give up. The handler resets itself to SIG_DFL first,
// so a fault *inside* the handler terminates immediately instead of looping.
extern "C" void crashHandler(int signalNumber)
{
	signal(signalNumber, SIG_DFL);
	printStackTrace(); // does not return: LOG_FATAL_ERROR exits
	raise(signalNumber);
}

/*
int reportInfo(WMISupport &provider)
{ LFS
static int64_t lastVirtualBytes=0,lastWorkingSet=0;

IWbemClassObject *resourceNameInstance;
bool status=provider.getResourceObject(u"Win32_PerfFormattedData_PerfProc_Process.Name='lp'",resourceNameInstance);
int64_t percentProcessorTime,percentUserTime,percentPrivilegedTime,elapsedTime,virtualBytes,workingSet;
status = provider.getWMIKey(resourceNameInstance,u"ElapsedTime",elapsedTime);
status = provider.getWMIKey(resourceNameInstance,u"PercentProcessorTime",percentProcessorTime);
status = provider.getWMIKey(resourceNameInstance,u"PercentUserTime",percentUserTime);
status = provider.getWMIKey(resourceNameInstance,u"PercentPrivilegedTime",percentPrivilegedTime);
status = provider.getWMIKey(resourceNameInstance,u"VirtualBytes",virtualBytes);
status = provider.getWMIKey(resourceNameInstance,u"WorkingSet",workingSet);
lplog(u"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
lp_wprintf(u"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d\n",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
lastVirtualBytes=virtualBytes;
lastWorkingSet=workingSet;
return 0;
}
*/

// Batch B4a: these were plain bools written by a Windows handler thread. They are
// now written by a real signal handler, where only volatile sig_atomic_t is
// guaranteed safe to touch, so that is what they are. sig_atomic_t is an int, and
// every use of these is a boolean test, so no call site changes. (They are read
// once per iteration in some hot parse loops in agreement.cpp; a volatile int load
// is cheap, and correctness in the handler is worth more than eliding it.)
volatile sig_atomic_t exitNow = 0, exitEventually = 0;

// Two-stage interrupt handler (installed by initialize()), replacing the Windows
// ConsoleHandler: the first Ctrl-C sets exitEventually, which makes the source loop
// stop after the current document; a second one also sets exitNow, which abandons
// the current document and skips signalFinishedProcessingSource().
//
// SIGINT is Ctrl-C. SIGTERM and SIGHUP are handled the same way, standing in for the
// old CTRL_CLOSE/CTRL_LOGOFF/CTRL_SHUTDOWN events: SIGTERM is what `kill` and a
// system shutdown send, SIGHUP is what a closed terminal sends. Unlike Windows,
// which killed the process a few seconds after CTRL_CLOSE_EVENT regardless (so a
// console close usually lost the document), nothing here forces termination -- the
// document gets to finish.
//
// Everything in here must be async-signal-safe, which is why the message goes out
// through write(2) rather than lp_wprintf: the old handler's printf-family call was
// safe on Windows only because it ran on an ordinary injected thread, not in a
// signal context.
extern "C" void interruptHandler(int)
{
	if (exitEventually) exitNow = 1;
	exitEventually = 1;
	static const char immediately[] = "\nInterrupt received! Interrupting immediately...\n";
	static const char atEndOfSource[] = "\nInterrupt received! Interrupting at end of this source...\n";
	const char* message = exitNow ? immediately : atEndOfSource;
	ssize_t ignored = write(STDOUT_FILENO, message, strlen(message));
	(void)ignored; // nothing useful to do if the console write fails
}

/*

Amazon customer reviews:
http://www.amazon.com/gp/community-content-search/results?ie=UTF8&flatten=1&query=10%20amp&search-alias=community-reviews

1. finish processing american gutenberg and australian gutenberg
2. process definitions and create word relations with definition flag
3. finish using word relations for parsing speed-up and evaluation
4. Also fix word relations not found problem when writing word relation history
5. Use this to convert from
http://gill.readingroo.ms/1%20-%20GILL'S%20LIBRARY/Fiction-Biographies/
http://manual.calibre-ebook.com/faq.html#what-formats-does-app-support-conversion-to-from

1. PURPOSE - the purpose of the first conversation in the Adversary is to transfer the papers
2. DESIRE - Characters say 'I want to make money', I'd like to go to the park (etc)
3. SENTIMENT - Today I am happy.

1. record location of every relation in a separate table
each location can also be specific and generic (St. James Park is specific, park is generic)
2. create a linked list of narrative verbs throughout each narrative
3. divided into sections defined by changes in time and/or place
4. where the narrative linked list leads backwards into the start of the section
5. and each section has an associated time and place
6. and each section is linked to the speakergroup.
7. each section should also have associated generic objects, such that each location has associated generic objects (chair, desk, tree, etc)

look up a relation throughout all sources.
filter by generic place (park/kitchen/etc) not specific place
filter by previous action/future action

generic action cause/effect
for every entry in wordRelationHistory for a given wordRelation (and gwhere=given where)
query wordRelationHistory where where>gwhere-10 and where<gwhere [PAST]
OR query wordRelationHistory where where<gwhere+10 and where>gwhere [FUTURE]
and narrativeNum matches sort by frequency

object restrained action cause/effect
for every entry in wordRelationHistory for a given wordRelation (and gwhere=given where, and gobject=given associated object id)
query wordRelationHistory where where>gwhere-10 and where<gwhere and object id match [PAST]
OR query wordRelationHistory where where<gwhere+10 and where>gwhere and object id match [FUTURE]
sort by frequency

1. NOW at time expression timetransition check: 65091
2. adjust location tracking by time, to create a full timeline with location
3. [ sentence drives adjacency pair drives next sentence linked by relations]
identify all conversation pairs by type
relations
sentences
adjacency pairs
4. Question/Answer training - trace through all answers to questions from corpus
Determine all structures that are an answer to them. Analyze structure types and matching relations between the question and answer.
a. Who
b. When
c. Where
d. What
e. Why
f. How

Also, go by Google News. Get all articles about a person.
Analyze all articles to confirm they are about the same person.
Analyze whether the article is positive or negative about the person
Where they are
When they are
What are they doing

TODO:
multiple processes:
central decision
hunger
curiousity
secondary relations correlator (thinking about what was just said)
alternate goals resolution mechanism
current central conversation theme
alternate conversation themes

the use of language is driven by results!
study how things move in space - have a goal, scan for the goal being achieved and move backwards to find the language and context that enabled that action.

drive understanding through structure determined by adjacency pairs
at first, limit understanding to physical verbs and fundamental relations (work, play, etc)

constantly build a model of your audience
relations
where and when
locate the audience in context of the stories

drivers of conversation -
inform - non-opinion - driven by 'theme' or common topic or summary (money/Mother)
goal -driven conversation - how do we get this accomplished?
biographies/biographies show how one person lived. This will help give a person-context/image of the other.
search then in biographies (Gutenberg LoC Class = CT (151 books match)) for this particular person/as many context clues as given
then project forward in time to search for goal (stated by other speaker?)
find love
make lots of money
become famous
will have to search also for synonyms/meanings
will also have to drive question/answer adjacency pairs
find happiness? http://kidshealth.org/kid/feeling/index.html
inform with opinion - (editorials?)
first we must establish opinions to defend or persuade with
we must measure how much the other person agrees/disagrees with our opinion and
then based on which part of the opinion
counter it
feeling based conversation
mad/sad/glad/fear/shame/hurt - how to understand feelings?
type of conversation (inform/fix problem/persuasion) goal


different types of conversation (making some one else's internal state more consonant with yours):
persuasion: argument, debate
making some one else's internal state more consonant with yours:
why the U.S. should have more nuclear plants
why we should spend more money
Expressing agreement
Expressing criticism
Expressing support
Responding to criticism

fix problem - request for information or opinion
how to get more money [for us or for me], a common goal or a self-related or other-related goal
Asking a question
Disclosing personal information
Soliciting help
Soliciting comments
Putting out a wanted ad
Calling for action - Rallying support - Recruiting people - Giving a shout-out

inform -
about 'dry' issue like economics (teach) or
how to build a train
Answering a question
Making an observation
Acknowledging receipt of information
Advertising something or Giving a heads-up
Offering an opinion
Making a suggestion
Augmenting a previous post
internal issue like 'why I am mad at you'. 'I am sorry'.
Expressing surprise
Showing dismay
discuss without goal about common topic ("house", "children", "sports", "weather", "hobby", etc)
Making a joke


refresh relations in DB
analyze conversation and flow using
current/historical internal/external environment for each person and narrator
1. INFORMATION - what information does each person know?
2. What new information can be provided and still maintain coherency?

moods - happy/sad/mad etc
corpus derived (laughed/cried/etc)
motivations money/power/etc



1. check before/after etc being used as conjunctions rather than prepositions
2. treat better: during (for time expressions that are META, whose prep objects don't involve
time, or involving time conjunctions like when and after.
3. cut down the WordCache for wikipedia entries
4. single quote

1. Partners in Crime
2. N or M
3. BY the pricking of my thumbs
asher/lascarides - Logics of conversation
connectives - yet/hence/but
implicatures
amy ate some of the chocolate
also means amy did not eat all of the chocolate


eliminate these from Wikipedia search:
number -
anything with a , or a [ or a {
anything with an odd # of quotes
anything with the second word being a time unit, or a pronoun
action - hailing a taxi. Does that mean, if encountered at the end of an sg, that the character got into the car? Most likely.
go to lunch? going to dinner - look for places mentioned?
introduce location by activity - party, lunch, dinner etc.
phrases indicating future action
handle postProcessLinkedTimeExpressions.
in quote time expressions, either command
4611:call upon me tomorrow morning at 11 o'clock
7278: Come to - morrow at the same time
8771: Whittington was in a hurry to get rid of you[tuppence] this morning
9362: What are you[tuppence] doing this afternoon ?
10765: you[tuppence] would call round somewhere about lunch - time
16142: you[julius] don't know the name of the man who came this morning
17447: MOVE_OBJECT I[tuppence] got his[carter] reply this morning - in quote
31019: And to - day is Friday!
31919: - hadn't been here since Wednesday
32031: I[julius] haven't had one darned word from him[tommy] since we[julius,tuppence] parted at the depot on Wednesday .
35596: I[julius] shall be round in the car in half an hour .
38750: I[james] will call upon her[marguerite] about ten o'clock
39169: I[tuppence] shall meet you[julius] at the Ritz at seven .
47883: She[marguerite] took an overdose of chloral last night
47916: she[marguerite] was found dead this morning .
49831: To - day is Monday
27652 - morning post
Commands
involving an activity location:
07841:Let us go to Lunch.
09415:Let us do dinner and a show
10525: you[tommy,tuppence] could call and see me[carter] at the above address[carshalton] at eleven o'clock to - morrow morning
involving a future intention:
00819:Let us get out of it. "EXIT" GENDERED FULL
09558:Come up with me, Tuppence
09704:Come on, Tuppence.
Other phrases [ WHEN ]
future intention at a time:
4129:I must return to my suite at the hostel. [ UNKNOWN FUTURE ]
9126:I shall go alone tomorrow. [ TOMORROW ]
10106:He had been bound ... to repair to the National Gallery, where his colleague would meet him at ten o'clock.
future intention tied to an action:
9186:When I come out I shan't speak to you [ I come out ]
9212:when he comes out of the building I shall drop a handkerchief [ He comes out of the building ]
future intention in an iterative (story-like) fashion
10611:ushered into the presence of Mr. Carter. [ AFTER previous statement ]
10686:I rejoin you in the road outside [ AFTER previous statement ]
10695:we proceed to the next address... [ AFTER previous statement ]
Questions
Where shall we meet? METAWQ Piccadilly Tube station
You followed me here?
Where to? METAWQ Paris.
A pensionnat? METAWQ Madame Columbier in the Avenue de Neuilly
How about the Ritz?
I go where?
??
You would go in the character of my ward
I prefer the Picadilly.
rendezvous and place and address are both 'where' anaphors

Also, collect phrases for maintenance of current time line.

verify place for each gendered object throughout book
includes future and past objects, which also implies time relations
current: 63 - why is 'others' matched to children?
do 'new' location for an object - add speakergroup/aging
'He finished painting in the gallery'
'Back at the Ritz, he looked through the pictures'
improve meta object resolution
collect and organize comments
analyze through entire corpus
generate new conversation based on stat
add logic/motivation
add front-end

transcript sources
http://www.texaslegacy.org/m/transcripts/index.html
http://www.pbs.org/wgbh/nova/transcripts/
http://www.pbs.org/nbr/site/onair/transcripts/archive/
http://www.pbs.org/newshour/bb/
news.google.com/archives

She was going to have not taken a bath for three days.
St. James - should be masculine (st.'s last name is always gender dominant)
St. James shouldn't match because it is an adjective inside of another NAME which is a place!
single noun with determiner - accumulate in BNC or when every other word has a relation
verb objects - accumulate BNC when no objects after active verb, one object or two objects when one is a pronoun
accumulate non-BNC without infinitive phrases (is 'to' a prep or not)
Why - an account of the character's wants and desires and teh subsequent fulfillment thereof
include mental states
context analysis (conversation)
http://www.idiomconnection.com/
www.phrases.org.uk
http://www.idiomsite.com/
http://www.idioms-today.com/
extend thinksay and internal verbs through verbNet - mark counts of how many in each class
implement Paice & Husk for pleonastic it
so anaphora -
(1) “And with complete premeditation [they] resolved that His
Imperial Majesty Haile Selassie should be strangled because he was
head of the feudal system.” He was so strangled on Aug. 26, 1975,
in his bed most cruelly. (Chicago Tribune 12/15/94)
SYNTACTIC FORM AND DISCOURSE ACCESSIBILITY 3
(2) As an imperial statute the British North America Act could be
amended only by the British Parliament, which did so on several
occasions. (Groliers Encyclopedia)
zero anaphora - find the red blocks and stack up three
He found three blocks and she did also.
REMEMBER COPULAR VERBS 5.5 p.435 "to be" verbs
copular relations -
He is the driver.
He was voted the driver.
He became the driver.
He was appointed the president.
He was named the doctor.
Indefinite anaphora -
George bought some chocolates
A few are left.
attributes
NEWS corpora - changes in anaphora
Sophia Loren ....
The actress ...

if last name is resolved as a noun (not unknown), is not matched as speaker, and first name has no sex (not preidentified as a girl or boy name)
it is a NAME_OTHER object

example of how true reading is so heavily based on prediction and context that very little
information is needed (the first and last letters, and the rest out of order) to read:

"Cna yuo raed tihs? Olny 55 plepoe out of 100 can.

"i cdnuolt blveiee taht I cluod aulaclty uesdnatnrd waht I was rdanieg.
The phaonmneal pweor of the hmuan mnid, aoccdrnig to a rscheearch at
Cmabrigde Uinervtisy, it dseno't mtaetr in waht oerdr the ltteres in a
wrod are, the olny iproamtnt tihng is taht the frsit and lsat ltteer
be in the rghit pclae. The rset can be a taotl mses and you can sitll
raed it whotuit a pboerlm. Tihs is bcuseae the huamn mnid deos not
raed ervey lteter by istlef, but the wrod as a wlohe. Azanmig huh?
yaeh and I awlyas tghuhot slpeling was ipmorantt!"
does the verb 'to be' take any adverb?

does not include:
verbs attended by recurring time: ""

verbpart relation (bring him up)
remember adjectives, make different proper noun adjectives incompatible "Danver's Park, Coor's Park"
Jane as Tuppence should not be a name
clean up all present verbs still in narration

GIVER-RECEIVER MODEL
once time-flow is established for each speaker object
create giver receiver arrays, each entry has:
speaker object: Nancy
object: (animate object) Bill
role: (role from Nancy to Bill) Boss/Unknown
giver array
a noun: time #, Nancy gave him money
verb: time #, Nancy said to Bill: "Spy on Rachel. Steal the jewel.".
receiver array
a noun: time #, Nancy took the jewel.
verb: time #, Bill said to Nancy: "Now give me money."
object: Bart
role: Mother
giver array
a noun: time #, Nancy gave Bart a hug.
verb: time #, Nancy said to Bart: "Go to school".
receiver array
a noun: time #, Bart gave Nancy a kiss.

// don't attempt to estimate a verb tense appearing in the second position after another verb - this
// requires more sophisticated processing to get the tense of the verb immediately before this one - TODO
// incorporate verb tense I am going to do this, I am running to finish this, etc

make sure mainEntry isn't used to cross word-class boundries.
spoke noun mainEntry is speak - verb. Wrong!

include EVAL in costing for markChildren
~~~Willing to do anything, go anywhere. check 'willing'

update wordforms set count=50000 where wordid=30 and formid=62; // making sure 'but' is also a conjunction
update wordforms set count=7000 where wordid=7367 and formid=103; // making sure 'willing' is also a verb participle
James's is MALE and FEMALE? (4263)
ALSO count the embedded relative clause in __APPNOUN[2] as a relative clause, with appropriate costs and statistics
correct statistics of object relative clauses (IVOS)
*TEST also if the second object is sufficiently far away from the verb, it should incur a graduated cost as well.

word relations ratio
for each word relation present in sentence, wrr=(frequency of wr)/(all wr for word)
swrr=add wrr for each word in sentence/num words in sentence
when, who, where relations so that subjects can be tagged with time, person or place

do INFP phrases after verbs mean the verb has no more objects other than the INFP itself?

Why are there numbers in wordRelations?
also for a generic company (associated with all company names)
also for a generic geographic entity

verbs accepting two objects:
"accord","advance","afford","allot","allow","apportion","ask","assemble","assign","award","bake","bar","bear","begrudge","bequeath","bet","bid",
"blend","boil","brew","bring","build","buy","call","carve","cash","cast","catch","charter","chisel","chuck","churn","clean","clear","compile",
"cook","crochet","cut","dance","deal","deny","design","develop","dig","do","draw","drop","earn","embroider","ensure","envy","excuse","fashion",
"fetch","fill","fix","flash","fling","fold","forbid","foretell","forge","forgive","forward","fry","gain","gather","get","give","grant","grill",
"grind","grow","grudge","guarantee","hack","hammer","hand","hardboil","hatch","hire","hit","hum","iron","keep","knit","lay","lead","lease","leave",
"lend","light","loan","lose","mail","make","match","mint","mix","mold","occasion","offer","open","order","owe","paint","pass","pay","phone","pick",
"play","pluck","poach","pound","pour","prepare","procure","promise","pull","reach","read","recite","refuse","reimburse","remit","rent","reserve",
"roll","run","save","scramble","sculpt","secure","sell","send","set","sew","shape","shoot","show","sing","slaughter","softboil","spare","spin",
"steal","stitch","strike","take","teach","telegraph","tell","throw","tip","toss","vote","vouchsafe","wager","wash","weave","whistle","whittle",
"will","win","wire","wish","write",

prepositional verbs
barring bating concerning considering
disconcerning during enduring excusing
failing following induring inpending
lacking notwithstanding passing pending
regarding respecting saving selfregarding
touching transpassing unwanting wanting
barring excepting bating excepting

parse this sentence:
In the neighbourhood are: Alatri is divide into the following rioni (quarters): Chiappitto, Pacciano, Porpuro, Valle Santa Maria, Carvarola,
Capranica, Fontana Vecchia, Maddalena, Piedimonte, Madonna delle Grazie, Melegranate, Montecapraro, Vignola, Valle Carchera, Montesantangelo, Montelarena,
Pezza, Allegra, Basciano, Pignano, Castello, Collefreddo, Madonna del Pianto, Montelungo, Montereo, Monte San Marino, Pezzelle, Preturo, Sant'Antimo,
San Valentino, Vallecupa, Vallefredda, Valle Pantano, Vallesacco, Valle S.Matteo, Villa Magna, Cassiano, Castagneto, Fraschette, Seritico, Santa Caterina,
Vicero, Aiello, Canarolo, Collelavena, Costa San Vincenzo, Maranillo, Cavariccio, Colletraiano, Imbratto, Piano, S. Colomba, Scopigliette, Cucuruzzavolo,
le Grotte, Magione, Mole Santa Maria, San Pancrazio, Vallemiccina, Sant'Emidio, Canale, Prati Giuliani, Quarticciolo, Quarti di Tecchiena, Tecchiena,
Campello, Mole Bisleti, Cuione, Fontana Santo Stefano, Fontana Sistiliana, Frittola, S. Manno, Arillette, Collecuttrino, Colle del Papa, Laguccio,
Montelena, Quercia d'Orlando, San Mattia, Carano, Fontana Scurano, Magliano, Cellerano, Fiume, Fiura, Fontana Santa, Riano, Abbadia, Case Paolone,
Fontana Sambuco, Gaudo, Intignano, Colleprata.

enhance agreement with the examples in comment before evaluateSubjectVerbAgreement.
when she thought of him, she yelled "How!". (speaker related speech not in a conversation)
abb as pertains to plural measurements restricted 10 oz 11 in, etc.
fix multi-subject relations
test relations with examples from both grammar books (clear up usage of REL1, REL2 and VERBREL1, VERBREL2)

create process to detect misspelled words and replace them with the pointers to correctly spelled words
and also replace rarely used numbers with ranges 'over 100 and under 200' 'over 1000 and under 1000000' 'many' etc.
verify objects, verify object roles, 'Some' should be plural only, it should NOT be male or FEMALE.
enhance groups by grouping together only people who react warmly to each other (not growling, or saying 'I hate you' etc).

?fix 'to' verb vs 'to' noun, so that 'to' (single noun without determiner) is of greater cost, and encourages INFP.
fix VERBREL as an object for a prepositional phrase - EXCEPT where preposition is 'to'.
limit links to objects in relatedObjects array to main noun for GENERAL_OBJECT_CLASSes

more information sources:
check all words against WordNet and make sure we have all the words WordNet 2.1 does
(add company form test if Proper_Noun - uncertain as to implementation)
http://www.hoovers.com/free/search/simple/xmillion/index.xhtml?query_string=General+Motors&which=company&page=1
look for: Hoover's Company Name Matches
www.time.com (all archives are open and free)

open issues:
incorporate internal body parts as clues to set internal state objects
Correct abbreviation discovery by using abbreviation rules delineated in computational linguistics book:
abbreviations usually don't have any vowels in them followed by lower case words, or punctuation
France should be a neuter object!
addCoords and addHyponyms could also extract gender tags
Incorporate ANC (Second Release) http://americannationalcorpus.org/SecondRelease/encoding.html
populate new word relation WordWithPrepObjectWord
What [makes of vehicles] participated in the Challenge?

think patterns could be replaced by a tag procedure like agree which increases the cost of a pattern if the verb is not a think verb but has an _S1 as object
correct words that have spaces split by a new line are not recognized as being in the same word
add parents to patterns to have reduceParent look for parents of a child pattern in log n, not the child pattern in order n
if a pattern has a child which has all the tags for a particular tagset, then block all patterns for that child from that tagset. This
prevents a parent pattern from checking for a tagset when the child would also check for it. This is good for a noun - determiner usage,
but may not be optimal for agreement patterns.
add gendered animals (cow, bull)
add gendered objects (ship, countries)
add plural to collective nouns (team, committee)
create occupational subclass: educational - 1st-grader, middle-schooler, junior, under-graduate...
create occupational subclass: political - democratic, republican, independent...
appearance (already taken care of by associatedAdjectives): shorty, dwarf
create occupational subclass: class/caste - peasant, bourgousie
racial/regional (but not country): aborigine, indian, african, asian, european - add to demonyms
by hobby/activity other than occupation: bicyclist, runner,
attempt to combine pronoun definite selections with definite Proper_Noun's of the same gender and number.
attribute three-quarters, three-fifths to number
correct can't, don't etc contractions
keep female/male names as probabilities - so Robert has only a 1 % chance of being female.
meta language: the adjective "old" didn't describe them.
handle very long dashed words (by splitting them): kowabunga-mutant-ninja-turtle-most-unfortunately-named-clothing-company-of-the-century

/* feedback mechanism
run through BNC, noting correct class types. save in winnerForms

test BNC pattern violation and general BNC sentence parse correctness - eliminate multiple parses on one sentence as much as possible
run through matching process.
run eliminateLoserPatterns
for each position in sentence:
if no lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms forms, next position.
for beginPEMAPosition of m[position] till endPEMAPosition
if pema position returns isChildPattern false
if form is incorrect:
add all parents to 'incorrect' array. add last parent to incorrectLastParent if lastParent has isTopLevelMatch true for this position
if form is correct:
add all parents to 'correct' array.
if there are no parents in the correct array (unmatchable++), or incorrect array (correctlyMatched++), return.
for each parent in incorrect array that matches a parent in the correct array by begin, end, and parentElement,
increase the incorrectly matching child cost patternElementIndex by one.
if there are no matches, increase cost of each incorrectLastParent by one.
clear pma, pema
rerun

efficiency:
use Low Fragmentation heap by using HeapSetInformation
try not clearing TagSetMap in collectTags!
will use HANDLER interface to MyISAM, instead of SQL... faster?
ALSO think about converting forms to bit fields:
eliminating wordForms table, storing forms per word as a bit field within words table
make patterns more efficient:
{ LFS
computer 'first' of each pattern so that a set of patterns beginning with each form is created
compute first form and last form of each pattern
compute first and last strictly non-mandatory form of each pattern
collect all patterns which include a pattern - use for
for each pattern
for each element in pattern
for each index in pattern
if pattern index:
if first of pattern and lastSet has common members, log problem.
if form index:
if lastSet has form in it, log problem.
add up all the forms and the lasts of all the patterns of all the previous elements until a mandatory element is reached.
if the intersection of the first of the pattern and the set
compute overmatched by form and by pattern
}

debug hints:
use Win32 windbg
use application verifier
use this sympath:
.sympath c:\windows\system32;SRV*c:\localsymbols*http://msdl.microsoft.com/download/symbols;SRV*C:\WebSymb*http://msdl.microsoft.com/download/symbols
*/
int initializeCounter(void);
void freeCounter(void);
void reportMemoryUsage(void);
bool TSROverride = false, flipTOROverride = false, flipTNROverride = false, logMatchedSentences = false, logUnmatchedSentences = false;

// new_handler installed by initialize(): logs the allocation failure and terminates.
// The exit(1) below is unreachable because lplog(LOG_FATAL_ERROR,...) already exits
// the process first (via logging.cpp's fatalExit(), status EXIT_FAILURE).
void no_memory() {
	lplog(LOG_FATAL_ERROR, u"Out of memory (new/STL allocation).");
	exit(1);
}

// Batch B4a: setConsoleWindowSize() is deleted. It set the Windows console's
// screen-buffer and window rectangle -- both are properties the user owns on
// macOS (terminal size, scrollback depth are Terminal/iTerm preferences), and
// there is no API to impose them from inside the process. Nothing replaces it.

// Read the completed-work totals for one sourceType from the `sources` table: rows that
// are processed, not currently being processed, and not marked '**SKIP**' or
// '**START NOT FOUND**'.  Used by the controller to compute the sources/hour and words/hour
// figures shown in its console title.
// Out: numSourcesProcessed / wordsProcessed / sentencesProcessed - left untouched if the
// query returns no row.
// Returns 0 on success, -1 if the table could not be locked or unlocked (the callers all
// ignore this).  Side effects: takes and releases a WRITE lock on `sources`; frees the
// result set.
// NOTE: SUM() yields SQL NULL when no row matches, so on an empty/fresh corpus
// sqlrow[1]/sqlrow[2] are NULL and atol/atoi are called on a null pointer.  atol/atoi also
// truncate to 32 bits even though the out parameters are int64_t.
int getNumSourcesProcessed(MYSQL& mysql, int sourceType, int& numSourcesProcessed, int64_t& wordsProcessed, int64_t& sentencesProcessed)
{
	MYSQL_RES* result;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&mysql, u"LOCK TABLES sources WRITE")) return -1;
	// Batch B4a: lp_snprintf, bounded by QUERY_BUFFER_LEN per mysqldb.h's buffer
	// convention (qt is QUERY_BUFFER_LEN_OVERFLOW long, giving 1024 lpchar_t of
	// slack past the limit), replacing the Win32 unbounded lp_wsprintf.
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select COUNT(id), SUM(numWords), SUM(numSentences) from sources where sourceType = %d and processed IS not NULL and processing IS NULL and start != '**SKIP**' and start != '**START NOT FOUND**'", sourceType);
	if (myquery(&mysql, qt, result))
	{
		MYSQL_ROW sqlrow = NULL;
		if (sqlrow = mysql_fetch_row(result))
		{
			numSourcesProcessed = atoi(sqlrow[0]);
			wordsProcessed = atol(sqlrow[1]);
			sentencesProcessed = atoi(sqlrow[2]);
		}
		mysql_free_result(result);
	}
	if (!myquery(&mysql, u"UNLOCK TABLES")) return -1;
	return 0;
}

// Log a child's exit status.  A worker that hit LOG_FATAL_ERROR exits non-zero; without
// this the controller could not distinguish a crashed child from a completed one.
// Batch B4a: takes waitpid's raw status instead of calling GetExitCodeProcess on a
// handle, which also means a child killed by a signal is now reported as such
// rather than being folded into a numeric exit code.
static void reportChildExitCode(int exitStatus, unsigned int slot)
{
	if (WIFEXITED(exitStatus))
	{
		int exitCode = WEXITSTATUS(exitStatus);
		if (exitCode != 0)
			lplog(LOG_INFO | LOG_ERROR, u"ERROR:process %u exited with code %d - its sources may be incomplete.", slot, exitCode);
	}
	else if (WIFSIGNALED(exitStatus))
		lplog(LOG_INFO | LOG_ERROR, u"ERROR:process %u was killed by signal %d - its sources may be incomplete.", slot, WTERMSIG(exitStatus));
}

// Drain phase of controller mode: no more sources are left to hand out, so wait for the
// numProcesses children still in handles[] to exit, closing each handle and compacting the
// array as they do, and refreshing the throughput line in the console title every three
// minutes (the wait timeout) so a long tail is visible.
// In/out: numProcesses (decremented to 0), handles[] (compacted), nextProcessIndex (left
// holding the last slot that was retired).  startTime is a clock() value; the
// *Originally counters are the totals sampled before this run started, so that the title
// shows only this run's progress.
// Batch B4a: two documented hazards in the Windows version are gone rather than
// carried over. WAIT_FAILED used to leave nextProcessIndex at 0xFFFFFFFF and run
// the memmove below with it (survivable only because the LOG_FATAL_ERROR
// terminated first); waitForAnyChildProcess returns a distinct sentinel that is
// checked before any indexing. And processingSeconds is integer seconds used as a
// divisor, so a child exiting inside the first second divided by zero -- which on
// macOS is a SIGFPE crash, not a Windows exception -- hence the max(1, ...) below.
void waitForSpawnedProcesses(MYSQL& mysql, const int sourceType, int &numProcesses, pid_t* childPids, unsigned int &nextProcessIndex, const int startTime,
	const int numSourcesProcessedOriginally, const int wordsProcessedOriginally, const int sentencesProcessedOriginally, const int maxProcesses)
{
	if (numProcesses)
	{
		printf("\nNo more processes to be created. %d processes left to wait for.", numProcesses);
		while (numProcesses)
		{
			int exitStatus = 0;
			int exitedIndex = lpWaitForAnyChildProcess(childPids, numProcesses, 1000 * 60 * 3, exitStatus);
			if (exitedIndex == LP_WAIT_FAILED)
				lplog(LOG_FATAL_ERROR, u"\nwaiting for child processes failed - %S", strerror(errno));
			int numSourcesProcessedNow = 0;
			int64_t wordsProcessedNow = 0, sentencesProcessedNow = 0;
			getNumSourcesProcessed(mysql, sourceType, numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
			int processingSeconds = max(1, (int)((clock() - startTime) / CLOCKS_PER_SEC));
			lpchar_t consoleTitle[1500];
			numSourcesProcessedNow -= numSourcesProcessedOriginally;
			wordsProcessedNow -= wordsProcessedOriginally;
			sentencesProcessedNow -= sentencesProcessedOriginally;
			lp_snprintf(consoleTitle, 1500, u"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].",
				numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, wordsProcessedNow * 3600 / processingSeconds);
			lplog(LOG_INFO | LOG_ERROR, u"%s", consoleTitle);
			lpReportProgress(consoleTitle);
			if (exitedIndex == LP_WAIT_TIMEOUT)
				continue;
			nextProcessIndex = (unsigned int)exitedIndex;
			reportChildExitCode(exitStatus, nextProcessIndex);
			printf("\nClosing process %u", nextProcessIndex);
			memmove(childPids + nextProcessIndex, childPids + nextProcessIndex + 1, (maxProcesses - nextProcessIndex - 1) * sizeof(childPids[0]));
			numProcesses--;
		}
	}
}

// Build the argument list for one child worker from this run's flags and start it.
// processKind selects both the executable and the mode:
//   0 - lp -ParseRequest 0 +          (answer the queued requests)
//   1 - lp -book 0 + -BC 0            (parse Gutenberg books)
//   2 - CorpusAnalysis -step <step>   (external corpus analysis)
// The boolean flags are passed straight through as the -forceSourceReread/-SW/-SWNR/-SWNW/
// -parseOnly/-MCSW/-logMatchedSentences/-logUnmatchedSentences switches;
// nextProcessIndex suffixes the child's log files.
// Returns the child pid, or 0 if the process could not be created or processKind is
// unknown.
// NOTE: specialExtension - not logFileExtension - is what is passed to the child's
// -log, so the parent's -log suffix is not propagated.
//
// Batch B4a: the three hardcoded relative .exe paths became sibling lookups (see
// lpSiblingExecutablePath), so controller mode no longer depends on having been
// launched from the root of a full build tree; and the command line is built as a
// real argument vector instead of one lp_wsprintf'd string, so nothing here can
// truncate or word-split.
pid_t createLPProcess(const int processKind, const bool forceSourceReread, const bool sourceWrite, const bool sourceWordNetRead, const bool sourceWordNetWrite, const bool parseOnly, const bool makeCopyBeforeSourceWrite,
	const int numSourcesPerProcess, lpwstring specialExtension, const int nextProcessIndex, const int step)
{
	pid_t childPid = 0;
	// Every argument is narrow: this is what execve ultimately wants, and the child's
	// own main() widens argv back on the way in (see main() at the bottom of this file).
	auto narrow = [](const lpwstring& wide) { return std::string(lp_utf16_to_utf8(wide)); };
	auto number = [](long long value) { return std::to_string(value); };
	const std::string logSuffix = narrow(specialExtension) + "." + number(nextProcessIndex);

	std::vector<std::string> arguments;
	if (processKind == 0 || processKind == 1)
	{
		arguments.push_back(lpSiblingExecutablePath("lp"));
		if (processKind == 0)
		{
			arguments.push_back("-ParseRequest"); arguments.push_back("0"); arguments.push_back("+");
		}
		else
		{
			arguments.push_back("-book"); arguments.push_back("0"); arguments.push_back("+");
			arguments.push_back("-BC"); arguments.push_back("0");
		}
		arguments.push_back("-cacheDir"); arguments.push_back(narrow(cacheDir));
		if (forceSourceReread)          arguments.push_back("-forceSourceReread");
		if (sourceWrite)                arguments.push_back("-SW");
		if (sourceWordNetRead)          arguments.push_back("-SWNR");
		if (sourceWordNetWrite)         arguments.push_back("-SWNW");
		if (parseOnly)                  arguments.push_back("-parseOnly");
		if (makeCopyBeforeSourceWrite)  arguments.push_back("-MCSW");
		if (logMatchedSentences)        arguments.push_back("-logMatchedSentences");
		if (logUnmatchedSentences)      arguments.push_back("-logUnmatchedSentences");
		arguments.push_back("-numSourceLimit"); arguments.push_back(number(numSourcesPerProcess));
		arguments.push_back("-log"); arguments.push_back(logSuffix);
		if (processKind == 1 && specialExtension.length() > 0)
		{
			arguments.push_back("-specialExtension");
			arguments.push_back(narrow(specialExtension));
		}
	}
	else if (processKind == 2)
	{
		arguments.push_back(lpSiblingExecutablePath("CorpusAnalysis"));
		arguments.push_back("-step"); arguments.push_back(number(step));
		arguments.push_back("-numSourceLimit"); arguments.push_back(number(numSourcesPerProcess));
		arguments.push_back("-log"); arguments.push_back(logSuffix);
	}
	else
		return 0;

	// Batch B4a: the failure return is now actually propagated. The three original
	// call sites read `if (errorCode = createLPProcess(...) < 0) break;`, where '<'
	// binds tighter than '=', so errorCode received the comparison result rather
	// than the return code and the break only left the switch -- a spawn failure
	// reached the caller as a zero handle and surfaced much later as a misleading
	// wait error.
	if (lpSpawnProcess(childPid, arguments) < 0)
		return 0;
	printf("\nCreated process %u:%d", nextProcessIndex, (int)childPid);
	return childPid;
}

// Throttle for controller mode: if the pool is already full (numProcesses == maxProcesses)
// block until one child exits, close its handle and report its slot in nextProcessIndex so
// the caller can start a replacement there; also refresh the throughput console title.
// Returns 0 when a slot is free (or the pool was not full to begin with), 1 when the five
// minute wait expired or was interrupted by an APC and the caller should just loop again,
// and -1 when there is nothing to wait for (an empty pool) and the caller should stop.
// In/out: nextProcessIndex - the slot to reuse; numSourcesLeft is zeroed here because once
// the pool is full the pre-counted estimate is no longer used.
// NOTE: numProcesses is by value, so the pool size is deliberately not decremented - the
// freed slot is immediately overwritten by the caller.  Same integer-division-by-zero
// hazard on processingSeconds as waitForSpawnedProcesses().
int waitToSpawnMoreProcesses(MYSQL& mysql, const int sourceType, const int numProcesses, pid_t* childPids, unsigned int &nextProcessIndex, const int startTime,
	const int numSourcesProcessedOriginally, const int wordsProcessedOriginally, const int sentencesProcessedOriginally, const int maxProcesses, int &numSourcesLeft)
{
	if (numProcesses == maxProcesses)
	{
		int exitStatus = 0;
		int exitedIndex = lpWaitForAnyChildProcess(childPids, numProcesses, 1000 * 60 * 5, exitStatus);
		if (exitedIndex == LP_WAIT_FAILED)
		{
			if (!numProcesses)
				return -1;
			lplog(LOG_FATAL_ERROR, u"waiting for child processes failed - %S", strerror(errno));
		}
		numSourcesLeft = 0;
		int numSourcesProcessedNow = 0;
		int64_t wordsProcessedNow = 0, sentencesProcessedNow = 0;
		getNumSourcesProcessed(mysql, sourceType, numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
		// max(1,...): see the note on waitForSpawnedProcesses -- a divisor of zero
		// here is a SIGFPE crash on macOS.
		int processingSeconds = max(1, (int)((clock() - startTime) / CLOCKS_PER_SEC));
		lpchar_t consoleTitle[1500];
		numSourcesProcessedNow -= numSourcesProcessedOriginally;
		wordsProcessedNow -= wordsProcessedOriginally;
		sentencesProcessedNow -= sentencesProcessedOriginally;
		lp_snprintf(consoleTitle, 1500, u"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].",
			numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, wordsProcessedNow * 3600 / processingSeconds);
		lpReportProgress(consoleTitle);

		if (exitedIndex == LP_WAIT_TIMEOUT)
			return 1;
		nextProcessIndex = (unsigned int)exitedIndex;
		reportChildExitCode(exitStatus, nextProcessIndex);
		printf("\nClosing process %u", nextProcessIndex);
	}
	return 0;
}

// Forward this controller's interrupt to every live child exactly once, so each finishes
// (or abandons) its current document and exits cleanly rather than being killed.
// In/out: sentBreakSignals - latch making the broadcast idempotent even though the source
// loop calls this on every iteration once exitEventually is set.
// Note the "Sending break signals" message is printed even on the repeat calls that do
// nothing.
void sendBreakSignals(bool & sentBreakSignals, const int numProcesses, pid_t* childPids)
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

bool getNextUnprocessedSource(MYSQL& mysql, int begin, int end, int sourceType, bool setUsed, int& id, lpwstring& path, lpwstring& encoding, lpwstring& start, int& repeatStart, lpwstring& etext, lpwstring& author, lpwstring& title);
int getNumSources(MYSQL& mysql, int sourceType, bool left);
bool anymoreUnprocessedForUnknown(MYSQL& mysql, int sourceType, int step);
// Controller mode (-mp): keep up to maxProcesses child workers busy until the `sources`
// table has nothing left to hand out, then wait for the stragglers.
// The controller itself never parses; it only decides whether more work exists.  How that
// is decided depends on processKind: kind 1 (book parsing) simply counts down the
// pre-computed numSourcesLeft and lets each child claim its own rows, kind 0 peeks at the
// next unprocessed request row (setUsed=false, so it does not claim it), kind 2 asks
// whether any source still needs the given analysis `step`.
// beginSource/endSource bound the id range; numSourcesPerProcess is passed to each child
// as -numSourceLimit; the source*/parse* flags are forwarded verbatim.
// processSourceType is the sourceType of this (parent) run, and is only used to decide
// whether to _exit(0) at the end - anything other than REQUEST_TYPE never returns.
// Returns chdir("..")'s result (0 on success) or -1 if the initial chdir("source") failed.
// Side effects: changes the working directory to "source" for its whole duration, spawns
// and reaps processes, sets the console title, allocates handles[].
// NOTE: errorCode is never assigned inside the loop, so the loop only ends through one of
// the break paths; and because of the _exit(0) the free(childPids) and the chdir("..")
// are unreachable in every mode except REQUEST_TYPE.
// Batch B4a: the calloc is now checked, and the old "maxProcesses is not clamped to
// MAXIMUM_WAIT_OBJECTS (64), so a larger -mp makes WaitForMultipleObjectsEx fail
// immediately and the run dies with an unrelated message" hazard is simply gone --
// waitForAnyChildProcess has no such limit, so -mp is bounded only by what the
// machine can actually run.
int startProcesses(MYSQL& mysql, int sourceType, int processKind, int step, int beginSource, int endSource, cSource::sourceTypeEnum processSourceType, int maxProcesses, int numSourcesPerProcess,
	bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite, bool makeCopyBeforeSourceWrite, bool parseOnly, lpwstring specialExtension)
{
	LFS
		if (chdir("source") < 0)
			return -1;
	bool sentBreakSignals = false;
	int startTime = clock();
	pid_t* childPids = (pid_t*)calloc(maxProcesses, sizeof(pid_t));
	if (!childPids)
		lplog(LOG_FATAL_ERROR, u"could not allocate the child process table for -mp %d", maxProcesses);
	int numProcesses = 0, errorCode = 0, numSourcesProcessedOriginally = 0, numSourcesLeft;
	int64_t wordsProcessedOriginally = 0, sentencesProcessedOriginally = 0;
	if (processKind == 0)
		sourceType = cSource::REQUEST_TYPE;
	getNumSourcesProcessed(mysql, sourceType, numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally);
	numSourcesLeft = getNumSources(mysql, sourceType, true);
	maxProcesses = min(maxProcesses, numSourcesLeft);
	lpwstring tmpstr;
	while (!errorCode)
	{
		unsigned int nextProcessIndex = numProcesses;
		int breakContinueResult = waitToSpawnMoreProcesses(mysql, sourceType, numProcesses, childPids, nextProcessIndex, startTime,
			numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally, maxProcesses, numSourcesLeft);
		if (breakContinueResult < 0) break;
		if (breakContinueResult > 0)  continue;
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
			waitForSpawnedProcesses(mysql, sourceType, numProcesses, childPids, nextProcessIndex, startTime,
				numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally, maxProcesses);
			break;
		}
		if (exitNow || exitEventually)
			sendBreakSignals(sentBreakSignals, numProcesses, childPids);
		else
		{
			pid_t childPid = createLPProcess(processKind, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite, parseOnly, makeCopyBeforeSourceWrite,
				numSourcesPerProcess, specialExtension, nextProcessIndex, step);
			childPids[nextProcessIndex] = childPid;
			if (numProcesses < maxProcesses)
				numProcesses++;
		}
	}
	if (processSourceType != cSource::REQUEST_TYPE)
	{
		freeCounter();
		// Flush the buffered log FILE*s and close the MySQL connection before the fast
		// exit below: _exit() (unlike exit()) does not flush stdio buffers or run atexit
		// handlers, so without this the last few KB written to *.lplog since the last
		// logCache-driven flush would be silently lost, and the MySQL socket would be
		// dropped by the OS rather than closed cleanly.  This does not reintroduce the
		// "tearing down the lexicon takes minutes" cost the _exit() is here to avoid -
		// both calls are cheap.
		lplog();
		mysql_close(&mysql);
		_exit(0); // fast exit
	}
	free(childPids);
	return chdir("..");
}

std::shared_mutex rdfTypeMapSRWLock, mySQLTotalTimeSRWLock, totalInternetTimeWaitBandwidthControlSRWLock, mySQLQueryBufferSRWLock, orderedHyperNymsMapSRWLock;
// Batch B3: createLocks() is now empty. It called InitializeSRWLock on four of the
// five locks above; a default-constructed std::shared_mutex is already usable, so
// there is nothing left to initialize. The function is kept (rather than deleted
// along with its call in initialize()) purely so the call site stays a one-line
// no-op instead of this batch reaching into main.cpp's startup sequence, which is
// batch B4a's to restructure -- B4a can drop both together.
//
// This also closes the note that used to live here: orderedHyperNymsMapSRWLock was
// the one lock never passed to InitializeSRWLock even though getWordNet.cpp
// acquires it, and worked only because a zero-initialized global happened to equal
// SRWLOCK_INIT. That asymmetry no longer exists -- all five are constructed
// identically by the runtime.
void createLocks(void)
{
	LFS
}

// Make sure the MEMORY-engine mirror `wordRelationsMemory` is populated from the on-disk
// `wordRelations` table, so that the parser's word-relation lookups stay in RAM.  MySQL
// empties MEMORY tables on restart, so this runs once per controller / standalone process.
// Returns 0 if the mirror was already populated or was refilled with exactly as many rows
// as the disk table has, -1 on any query failure or on a row count mismatch (the single
// caller in wmain() ignores the result).
// Side effects: LOCKs both tables, inserts up to the whole word relation corpus, prints
// progress.
// NOTE: mysql is taken by value - a copy of the MYSQL connection object - unlike every
// other function here, which takes MYSQL&.  None of the early -1 returns unlocks the
// tables, and none of the three result sets is ever freed.
int WRMemoryCheck(MYSQL mysql)
{
	int numRowsOnDisk = 0, numRowsInMemory = 0;
	MYSQL_ROW sqlrow;
	MYSQL_RES* result = NULL;
	if (!myquery(&mysql, u"LOCK TABLES wordRelationsMemory WRITE,wordRelations READ"))
		return -1;
	if (!myquery(&mysql, u"SELECT COUNT(sourceId) from wordRelationsMemory", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsInMemory = atoi(sqlrow[0]);
	else
		return -1;
	if (numRowsInMemory > 0)
	{
		myquery(&mysql, u"UNLOCK TABLES");
		return 0;
	}
	if (!myquery(&mysql, u"SELECT COUNT(sourceId) from wordRelations", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsOnDisk = atoi(sqlrow[0]);
	printf("Loading wordrelations into memory...\n");
	if (!myquery(&mysql, u"insert into wordrelationsmemory select id, sourceId, lastWhere, fromWordId, toWordId, typeId, totalCount from wordrelations"))
		return -1;
	printf("Finished wordrelations into memory...\n");
	if (!myquery(&mysql, u"SELECT COUNT(sourceId) from wordRelationsMemory", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsInMemory = atoi(sqlrow[0]);
	else
		return -1;
	myquery(&mysql, u"UNLOCK TABLES");
	return (numRowsInMemory == numRowsOnDisk) ? 0 : -1;
}

int numSourceLimit = 0;
void processCommandArguments(int argc, lpchar_t* argv[],
	bool &forceSourceReread, bool& sourceWrite, bool& sourceWordNetRead, bool& sourceWordNetWrite, 
	bool &resetAllSource, bool &resetProcessingFlags, bool &generateFormStatistics, bool &retry,
	bool &parseOnly, bool &makeCopyBeforeSourceWrite,
	int &sourceArgs, int &numSourcesPerProcess, enum cSource::sourceTypeEnum &sourceType,
	lpwstring &specialExtension, lpchar_t sourceHost[])
{
	int numCommandLineParameters = argc;
	for (int I = 0; I < argc; I++)
	{
		if (!lp_wcscasecmp(argv[I], u"-server") && I < argc - 1)
			lp_strcpy(sourceHost, argv[++I]);
		else if (!lp_wcscasecmp(argv[I], u"-log") && I < argc - 1)
			logFileExtension = argv[++I];
		else if (!lp_wcscasecmp(argv[I], u"-mp") && I < argc - 1)
			multiProcess = lp_wtoi(argv[++I]);
		else if (!lp_wcscasecmp(argv[I], u"-numSourcesPerProcess") && I < argc - 1)
			numSourcesPerProcess = lp_wtoi(argv[++I]);
		else if (!lp_wcscasecmp(argv[I], u"-numSourceLimit") && I < argc - 1)
			numSourceLimit = lp_wtoi(argv[++I]);
		else if (!lp_wcscasecmp(argv[I], u"-cacheDir") && I < argc - 1)
			cacheDir = argv[++I];
		else if (!lp_wcscasecmp(argv[I], u"-resetAllSource"))
			resetAllSource = true;
		else if (!lp_wcscasecmp(argv[I], u"-resetProcessingFlags"))
			resetProcessingFlags = true;
		else if (!lp_wcscasecmp(argv[I], u"-generateFormStatistics"))
			generateFormStatistics = true;
		else if (!lp_wcscasecmp(argv[I], u"-retry"))
			retry = true;
		else if (!lp_wcscasecmp(argv[I], u"-parseOnly"))
			parseOnly = true;
		else if ((!lp_wcscasecmp(argv[I], u"-LC") || !lp_wcscasecmp(argv[I], u"-logCache")) && I < argc - 1)
			logCache = lp_wtoi(argv[I + 1]);
		else if ((!lp_wcscasecmp(argv[I], u"-BC")) && I < argc - 1) // bandwidth control
			cInternet::bandwidthControl = lp_wtoi(argv[I + 1]);
		else if (!lp_wcscasecmp(argv[I], u"-logMatchedSentences"))
			logMatchedSentences = true;
		else if (!lp_wcscasecmp(argv[I], u"-logUnmatchedSentences"))
			logUnmatchedSentences = true;
		else if (!lp_wcscasecmp(argv[I], u"-TSRO"))
			TSROverride = true;
		else if (!lp_wcscasecmp(argv[I], u"-fTOR"))
			flipTOROverride = true;
		else if (!lp_wcscasecmp(argv[I], u"-fTNR"))
			flipTNROverride = true;
		else if (!lp_wcscasecmp(argv[I], u"-forceSourceReread"))
			forceSourceReread = true;
		else if (!lp_wcscasecmp(argv[I], u"-SW"))
			sourceWrite = true;
		else if (!lp_wcscasecmp(argv[I], u"-MCSW"))
			makeCopyBeforeSourceWrite = true;
		else if (!lp_wcscasecmp(argv[I], u"-SWNR"))
			sourceWordNetRead = true;
		else if (!lp_wcscasecmp(argv[I], u"-SWNW"))
			sourceWordNetWrite = true;
		else if (!lp_wcscasecmp(argv[I], u"-specialExtension"))
			specialExtension = argv[++I];
		else
			continue;
		numCommandLineParameters = min(numCommandLineParameters, I);
	}
	sourceArgs = -1;
	sourceType = cSource::NO_SOURCE_TYPE;
	const lpchar_t* where;
	for (int I = 1; I < numCommandLineParameters - 1; I++)
	{
		lpwstring arg = argv[I];
		std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
		if ((where = lp_strstr(u"1-test 2-book 3-newsbank 4-bnc 5-script 6-websearch 7-wikipedia 8-interactive :-parserequest", arg.c_str())))
		{
			sourceType = (enum cSource::sourceTypeEnum)(where[-1] - '0');
			sourceArgs = I;
			break;
		}
	}
	if (sourceArgs == -1)
		lplog(LOG_FATAL_ERROR, u"Source type not found.");
}

int processPatternTransformTypeSource(cSource &source, lpchar_t *path)
{
	lpchar_t consoleTitle[1500];
#ifdef _DEBUG
	lpchar_t displayDebugFlag = u'D';
#else
	lpchar_t displayDebugFlag = u'R';
#endif
	lp_snprintf(consoleTitle, 1500, u"[%c] %s...", displayDebugFlag, path);
	lp_wprintf(u"%s\n", consoleTitle); // batch B4a: _putws is MSVC-only, and takes a real wchar_t*
	lplog(LOG_INFO | LOG_ERROR, u"%s\n", consoleTitle);
	lpReportProgress(consoleTitle);
	unlockTables(source.mysql);
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	cSource* requestedSource;
	cQuestionAnswering qa;
	return qa.processPath(&source, path, requestedSource, cSource::WEB_SEARCH_SOURCE_TYPE, 1, false);
}

void processSource(cSource &source, bool forceSourceReread, bool sourceWordNetRead, bool sourceWordNetWrite, bool sourceWrite, bool viterbiTest, bool parseOnly, bool makeCopyBeforeSourceWrite,
	const lpwstring specialExtension, lpwstring title, lpwstring& encoding, lpwstring& start, int& repeatStart, lpwstring& etext,
	int &numWordsOverAllSource, int &globalTotalUnmatched, int &globalOverMatchedPositionsTotal)
{
	int ret = 0;
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	unsigned int totalQuotations = 0, quotationExceptions = 0, unknownCount = 0;
	bool parsedOnly = false;
	Words.readWords(source.sourcePath, source.sourceId, false, specialExtension);
	if (forceSourceReread || !source.readSource(source.sourcePath, false, parsedOnly, true, specialExtension))
	{
		unknownCount = 0;
		switch (source.sourceType)
		{
			case cSource::TEST_SOURCE_TYPE:
			case cSource::GUTENBERG_SOURCE_TYPE:
			case cSource::WIKIPEDIA_SOURCE_TYPE:
			case cSource::INTERACTIVE_SOURCE_TYPE:
			case cSource::WEB_SEARCH_SOURCE_TYPE:
			case cSource::NEWS_BANK_SOURCE_TYPE:
			case cSource::REQUEST_TYPE:
				if ((ret = source.tokenize(title, etext, source.sourcePath, encoding, start, repeatStart, unknownCount)) < 0)
				{
					lplog(LOG_ERROR, u"ERROR:Unable to parse %s - %d (start=%s, repeatStart=%d).", source.sourcePath.c_str(), ret, start.c_str(), repeatStart);
					return;
				}
				quotationExceptions = source.doQuotesOwnershipAndContractions(totalQuotations);
				break;
			case cSource::BNC_SOURCE_TYPE:
			{
				// BNC processing has been removed for now - it is in getBNC.cpp
				// source.beginClock = clock();
				// bncc bnc;
				// bnc.process(source, source.sourceId, source.sourcePath);
				// source.adjustWords();
				// unknownCount = bnc.unknownCount;
				break;
			}
			case cSource::NO_SOURCE_TYPE:
			case cSource::SCRIPT_SOURCE_TYPE:
			case cSource::PATTERN_TRANSFORM_TYPE:
			default: break;
		}
		//int cap=source.m.capacity();
		source.m.shrink_to_fit(); // C++ 11 only
		//int cap2=source.m.capacity();
		int totalUnmatched = source.printSentences(true, unknownCount, quotationExceptions, totalQuotations, globalOverMatchedPositionsTotal);
		if (totalUnmatched < 0)
			lplog(LOG_FATAL_ERROR, u"Cannot print sentences.");
		globalTotalUnmatched += totalUnmatched;
	}
	else
	{
		lplog(LOG_INFO, u"%s already parsed.", source.sourcePath.c_str());
		source.m.shrink_to_fit(); // C++ 11 only
		source.printSentencesCheck(false);
	}
	numWordsOverAllSource += source.m.size();
	sourceWordNetWrite = (!sourceWordNetRead || !source.readWNMaps(source.sourcePath)) && sourceWordNetWrite;
	source.addWNExtensions();
	puts("");
	if (source.m.size())
	{
		source.identifyObjects();
		source.analyzeWordSenses();
		source.narrativeIsQuoted = source.sourceType != cSource::GUTENBERG_SOURCE_TYPE;
		source.syntacticRelations();
	}
	lplog();
	if (sourceWrite)
	{
		source.write(source.sourcePath, false, makeCopyBeforeSourceWrite, specialExtension);
		source.writeWords(source.sourcePath, specialExtension);
		source.writePatternUsage(source.sourcePath, true);
	}
	if (parseOnly || viterbiTest)
	{
		if (viterbiTest)
		{
			void testViterbiFromSource(cSource & source);
			testViterbiFromSource(source);
		}
		if (!exitNow) source.signalFinishedProcessingSource(source.sourceId);
		if (source.updateWordUsageCostsDynamically)
			cWord::resetUsagePatternsAndCosts(source.debugTrace);
		else
			cWord::resetCapitalizationAndProperNounUsageStatistics(source.debugTrace);
		source.clearSource();
		return;
	}
	if (source.m.size())
	{
		//source.printVerbFrequency();
		source.identifySpeakerGroups();
		vector <int> secondaryQuotesResolutions;
		source.resolveSpeakers(secondaryQuotesResolutions);
		source.resolveFirstSecondPersonPronouns(secondaryQuotesResolutions);
	}
	vector <cSyntacticRelationGroup>::iterator srg = source.syntacticRelationGroups.begin();
	//source.printObjects(); // only necessary if printing objects
	//source.resolveWordRelations(); // this resolves word relations to add to words - these will be erased unless future plans to update word relations dynamically.
	if (sourceWrite && !source.write(source.sourcePath, true, false, specialExtension))
		lplog(LOG_FATAL_ERROR, u"buffer overrun");
	vector <int> badSpeakers;
	source.printResolutionCheck(badSpeakers);
	source.logSpaceCheck();
	source.identifyConversations();
	//source.writeMarkup();
	// write or compare markup
	// read markup program will be written, along with a way to change the markup, keeping the most corrected markup and the last automated markup.
	// then the markup will be corrected, and the percentage correct noted.
	// whatever changes/improvements made will be scored according to the markup percentage changed/correct from the last automated run.
	// do 10 books.  create corrected markup against all of them and compute the automated score.
	// try to accumulate statistics on the best way to correct the automated run.
	// do 10 more.  and so on.  each time accumulating stats and correcting the automated run.
	//source.printResolutions(badSpeakers,false);
	if (sourceWordNetWrite)
		source.writeWNMaps(source.sourcePath);
	if (source.debugTrace.traceSpeakerResolution)
	{
		source.printTenseStatistics(u"Narrator", source.narratorTenseStatistics, source.numTotalNarratorVerbTenses);
		source.printTenseStatistics(u"Speaker", source.speakerTenseStatistics, source.numTotalSpeakerVerbTenses);
		source.printTenseStatistics(u"Narrator All Tenses", source.narratorFullTenseStatistics, source.numTotalNarratorFullVerbTenses);
		source.printTenseStatistics(u"Speaker All Tenses", source.speakerFullTenseStatistics, source.numTotalSpeakerFullVerbTenses);
		if (source.debugTrace.traceSpeakerResolution)
			source.printSectionStatistics();
	}
	lplog();
	if (source.sourceInPast = source.sourceType == cSource::INTERACTIVE_SOURCE_TYPE)
	{
		cQuestionAnswering qa;
		qa.answerAllQuestionsInSource(&source, parseOnly, true);
	}

	if (!exitNow) source.signalFinishedProcessingSource(source.sourceId);
	source.clearSource();
	if (source.updateWordUsageCostsDynamically)
		cWord::resetUsagePatternsAndCosts(source.debugTrace);
	else
		cWord::resetCapitalizationAndProperNounUsageStatistics(source.debugTrace);
}

// Batch B4a: installs one signal handler, replacing SetUnhandledExceptionFilter
// (the four fault signals) and SetConsoleCtrlHandler (the three interrupt signals).
// SA_RESTART keeps an interrupt from turning every in-flight read/write into a
// spurious EINTR failure in code that has never checked for it.
static void installSignalHandlers()
{
	struct sigaction interruptAction;
	memset(&interruptAction, 0, sizeof(interruptAction));
	interruptAction.sa_handler = interruptHandler;
	sigemptyset(&interruptAction.sa_mask);
	interruptAction.sa_flags = SA_RESTART;
	sigaction(SIGINT, &interruptAction, nullptr);
	sigaction(SIGTERM, &interruptAction, nullptr);
	sigaction(SIGHUP, &interruptAction, nullptr);

	struct sigaction crashAction;
	memset(&crashAction, 0, sizeof(crashAction));
	crashAction.sa_handler = crashHandler;
	sigemptyset(&crashAction.sa_mask);
	crashAction.sa_flags = 0;
	sigaction(SIGSEGV, &crashAction, nullptr);
	sigaction(SIGBUS, &crashAction, nullptr);
	sigaction(SIGFPE, &crashAction, nullptr);
	sigaction(SIGILL, &crashAction, nullptr);
}

void initialize()
{
	// Log a stack trace whenever this program crashes, and handle Ctrl-C.
	installSignalHandlers();
	createLocks();
	set_new_handler(no_memory);
	initializeCounter();
	// Batch B4a: the GetCurrentDirectoryW(1024, dir) that used to sit here is gone
	// along with its buffer -- `dir` was written and then never read.
	if (chdir(".."))
		exit(-1);
	cacheDir = getCacheDir().c_str();
}

// Fatally checks whichever cacheDir is actually in effect once command-line
// parsing has had a chance to apply -cacheDir. Must run after
// processCommandArguments(), not from initialize(): initialize() runs before
// argv is parsed, so checking there tested the default/env value even when
// -cacheDir named a valid directory.
void validateCacheDir()
{
	// Batch B4a: access(F_OK) is the POSIX spelling of lp_waccess(path, 0).
	if (access(lp_utf16_to_utf8(lpwstring(cacheDir)).c_str(), F_OK) < 0)
		lplog(LOG_FATAL_ERROR, u"Cache directory %s does not exist!", cacheDir);
}

void printWordMatchingStatistics(int numWordsOverAllSource, int globalTotalUnmatched, int globalOverMatchedPositionsTotal, int overallTime)
{
	if (numWordsOverAllSource)
	{
		lp_wprintf(u"\n%d milliseconds elapsed (%d words, %d unmatched (%5.2f%%) %d overmatched (%5.2f%%)",
			(int)((clock() - overallTime) / (CLOCKS_PER_SEC / 1000)), numWordsOverAllSource,
			globalTotalUnmatched, (float)globalTotalUnmatched * 100 / numWordsOverAllSource, globalOverMatchedPositionsTotal, (float)globalOverMatchedPositionsTotal * 100 / numWordsOverAllSource);
		lplog(u"%d milliseconds elapsed (%d words, %d unmatched (%5.2f%%) %d overmatched (%5.2f%%)",
			(int)((clock() - overallTime) / (CLOCKS_PER_SEC / 1000)), numWordsOverAllSource,
			globalTotalUnmatched, (float)globalTotalUnmatched * 100 / numWordsOverAllSource, globalOverMatchedPositionsTotal, (float)globalOverMatchedPositionsTotal * 100 / numWordsOverAllSource);
		lplog();
	}
	else
		lplog(u"%d milliseconds elapsed. No words processed.", (clock() - overallTime) / (CLOCKS_PER_SEC / 1000));
}

/*
  SleepConditionVariableSRW	Sleeps on the specified condition variable and releases the specified lock as an atomic operation.
  TryAcquireSRWLockExclusive	Attempts to acquire a slim reader/writer (SRW) lock in exclusive mode. If the call is successful, the calling thread takes ownership of the lock.
  TryAcquireSRWLockShared	Attempts to acquire a slim reader/writer (SRW) lock in shared mode. If the call is successful, the calling thread takes ownership of the lock.

// -test timeExpressions -retry -BC 0 -cacheDir M:\caches -forceSourceReread -parseOnly -SW -SWNR -SWNW 
// -test tokenization -BC 0 -cacheDir M:\caches -SW -SWNR -SWNW -forceSourceReread -retry
// -test thatParsing -BC 0 -cacheDir M:\caches -SW -SWNR -SWNW -forceSourceReread -retry
// parse one gutenberg book repeatedly:
// -book 3537 3538 -BC 0 -cacheDir M:\caches -SW -SWNR -SWNW -forceSourceReread -numSourcesPerProcess 15 -retry -parseOnly
// -book 0 -BC 0 -retry -cacheDir M:\caches -SR -SW -SWNR -SWNW -TNMS
// parse gutenberg books all at once parcelled out (parseOnly):
// -book 0 + -BC 0 -cacheDir M:\caches -SWNR -SWNW -SW -MCSW -logMatchedSentences -logUnmatchedSentences -numSourcesPerProcess 15 -forceSourceReread -parseOnly -retry -mp 8
// do TREC:
// -Interactive 0 + -BC 0 -cacheDir J:\caches -SR -SW -SWNR -SWNW -TNMS

// test gutenberg by generating usage statistics
  if (argc>1 && !lp_strcmp(argv[1],u"-tg"))
  {
  	cSource source2(u"localhost",0,false,true,true);
  	source2.testStartCode();
  	return 0;
//}
// TEST Ontology:
	//cOntology::fillOntologyList(true);
	//cOntology::maxFieldLengths();
	//cOntology::writeOntologyList();
	//cOntology ontology;
	//ontology.readOpenLibraryInternetArchiveWorksDump();
// Test MusicBrainz
	vector <mbInfoRecordingType> mbRecordingsTypes;
	vector <mbInfoReleaseType> mbReleasesTypes;
	vector <mbInfoArtistType> mbArtistsTypes;
	getArtists(u"artist",u"Jay-Z",mbArtistsTypes);
	getArtists(u"compactLabel",u"Roc-A-Fella Records",mbArtistsTypes);
	getReleases(u"compactLabel",u"Roc-A-Fella Records",mbReleasesTypes);
	getRecordings(u"artist",u"Jay-Z",mbRecordingsTypes);
// TEST thesaurus
	// build thesaurus
	//source.createThesaurusTables();
	//extern vector <sDefinition> thesaurus;
	//for (int I = 0; I < thesaurus.size(); I++)
	//	source.writeThesaurusEntry(thesaurus[I]);
	// synonym testing
	//vector <set <lpwstring> > synonyms;
	//source.getWordNetSynonymsOnly(u"car", synonyms, 1);
	//for (int I = 0; I < synonyms.size(); I++)
		//for (set<lpwstring>::iterator ss = synonyms[I].begin(), ssEnd = synonyms[I].end(); ss != ssEnd; ss++)
			//printf("%d:%S\n", I, ss->c_str());

// TEST PATTERNS
	//source.accumulateNewPatterns();
	//source.printAccumulatedPatterns();

// NOUN/VERB class analysis (debugging)
	//bool measurableObject,notMeasurableObject,grouping;
	//analyzeNounClass(0,u"fish",0,measurableObject,notMeasurableObject,grouping,t);
	//lpwstring proposedSubstitute;
	//int inflectionFlags=0;
	//bool isNoun=false,isVerb=true,isAdjective=false,isAdverb=false;
	//analyzeSense(false,u"draft",proposedSubstitute,numIrregular,inflectionFlags,isNoun,isVerb,isAdjective,isAdverb);
*/
// Batch B4a: MSVC's wmain(int, wchar_t*[]) entry point does not exist on any POSIX
// platform, so the real entry point is now a standard main() that widens argv once,
// up front, and hands the same shape to the same lpMain() body below.
//
// The conversion has to outlive lpMain (argv strings are stored and referenced for
// the whole run, e.g. processCommandArguments keeps `cacheDir = argv[++I]`), so the
// decoded strings live in function-local statics that are deliberately never freed
// -- the process-lifetime equivalent of what the CRT did for wmain's argv.
//
// The pointers handed on are non-const, matching wmain's own `lpchar_t*[]`, so that
// every downstream signature (processCommandArguments, processPatternTransformTypeSource
// and everything they reach) is untouched by this change. They point into the
// reserved-and-then-filled `wideArguments` strings, so they stay valid and are
// legitimately mutable. argv[argc] is NULL, matching the C standard's guarantee,
// which several call sites here rely on by reading argv[sourceArgs + 2].
int lpMain(int argc, lpchar_t* argv[]);

int main(int argc, char* argv[])
{
	static std::vector<lpwstring> wideArguments;
	static std::vector<lpchar_t*> wideArgv;
	wideArguments.reserve(argc); // no reallocation, so the pointers taken below stay valid
	for (int i = 0; i < argc; i++)
		wideArguments.push_back(lpwstring(lp_narrow_to_wide(std::string(argv[i]))));
	wideArgv.reserve(argc + 1);
	for (int i = 0; i < argc; i++)
		wideArgv.push_back(&wideArguments[i][0]);
	wideArgv.push_back(nullptr);
	return lpMain(argc, wideArgv.data());
}

int lpMain(int argc, lpchar_t* argv[])
{
	initialize();
	bool viterbiTest = false;
	cProfile profile("");
	int overallTime = clock();
	lpchar_t sourceHost[1024];
	lp_strcpy(sourceHost, u"localhost");
	bool forceSourceReread = false, sourceWrite = false, sourceWordNetRead = false, sourceWordNetWrite = false;
	bool resetAllSource = false, resetProcessingFlags = false, generateFormStatistics = false, retry = false;
	bool parseOnly = false, makeCopyBeforeSourceWrite = false;
	int sourceArgs = -1, numSourcesPerProcess = 5;
	enum cSource::sourceTypeEnum sourceType;
	lpwstring specialExtension;
	processCommandArguments(argc, argv, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite,
		resetAllSource, resetProcessingFlags, generateFormStatistics, retry,
		parseOnly, makeCopyBeforeSourceWrite,	sourceArgs, numSourcesPerProcess, sourceType, specialExtension,	sourceHost);
	validateCacheDir();
	cSource source(sourceHost, sourceType, generateFormStatistics, multiProcess > 0, true);
	if (multiProcess > 0 || numSourceLimit == 0) // controller or a single process not under control
		WRMemoryCheck(source.mysql);

	if (resetAllSource) source.resetAllSource();
	if (resetProcessingFlags) source.resetProcessingFlags();
	source.initializeNounVerbMapping();
	if (multiProcess == 0)
		initializePatterns();
	source.initializePemaMap(desiredTagSets.size());
	if (source.sourceType == cSource::sourceTypeEnum::PATTERN_TRANSFORM_TYPE)
		return processPatternTransformTypeSource(source, argv[sourceArgs + 1]);
	int globalTotalUnmatched = 0, globalOverMatchedPositionsTotal = 0, numWordsOverAllSource = 0;
	if (iswdigit(argv[sourceArgs + 1][0]))
	{
		int beginSource = lp_wtoi(argv[sourceArgs + 1]), endSource;
		endSource = (argv[sourceArgs + 2][0] == '+') ? -1 : ((iswdigit(argv[sourceArgs + 2][0])) ? lp_wtoi(argv[sourceArgs + 2]) : beginSource + 1);
		if (retry)
		{
			lp_wprintf(u"Resetting sources...               \r");
			source.resetSource(beginSource, endSource);
		}
		if (multiProcess > 0)
		{
			// Batch B4a: the GetConsoleWindow/SetWindowPos pair that moved the
			// controller's own console window out of the way of its children's
			// is deleted -- a process cannot position the terminal it runs in on
			// macOS, and with no per-child windows there is nothing to move for.
			startProcesses(source.mysql, source.sourceType, 1, 0, beginSource, endSource, sourceType, multiProcess, numSourcesPerProcess, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite, makeCopyBeforeSourceWrite, parseOnly, specialExtension);
			return 0;
		}
		lp_wprintf(u"Getting number of sources to process...               \r");
		int numSources = getNumSources(source.mysql, source.sourceType, false);
		int numSourcesProcessed = 0, pid = (int)getpid();
		while (!exitNow && !exitEventually && (numSourceLimit == 0 || numSourcesProcessed++ < numSourceLimit))
		{
			int repeatStart;
			lpwstring path, encoding, etext, author, title, start;
			lp_wprintf(u"Getting number of sources left...               \r");
			int numSourcesLeft = getNumSources(source.mysql, source.sourceType, true);
			if (!getNextUnprocessedSource(source.mysql, beginSource, endSource, source.sourceType, true, source.sourceId, path, encoding, start, repeatStart, etext, author, title))
				break;
			path.insert(0, u"\\");
			path = path.insert(0, getTextDir());
			lpchar_t consoleTitle[1500];
			lp_snprintf(consoleTitle, 1500, u"[%03d:%03d-%03d:%03d%%]PID%05d %s '%s'...", source.sourceId, beginSource, numSources, (numSources - numSourcesLeft) * 100 / numSources, pid, (start == u"**SKIP**" || start == u"**START NOT FOUND**") ? u"Skipping" : u"", title.c_str());
			lp_wprintf(u"%s\n", consoleTitle);
			lplog(LOG_INFO | LOG_ERROR, u"%s\n", consoleTitle);
			lpReportProgress(consoleTitle);
			unlockTables(source.mysql);
			if (start == u"**SKIP**")
				continue;
			source.sourcePath = path;
			processSource(source, forceSourceReread, sourceWordNetRead, sourceWordNetWrite, sourceWrite, viterbiTest, parseOnly, makeCopyBeforeSourceWrite, 
				specialExtension, title, encoding, start, repeatStart, etext, numWordsOverAllSource, globalTotalUnmatched, globalOverMatchedPositionsTotal);
		}
#ifdef LOG_PATTERNS
		cPattern::printPatternStatistics();
#endif
		printWordMatchingStatistics(numWordsOverAllSource, globalTotalUnmatched, globalOverMatchedPositionsTotal,overallTime);
	}
	else
	{
		lpwstring start = u"~~BEGIN", title, etext, encoding = u"NOT FOUND";
		if (argv[sourceArgs + 2][0] == u'~')
			start = argv[sourceArgs + 2];
		int repeatStart = 1;
		source.sourcePath = u"tests\\" + lpwstring(argv[sourceArgs + 1]) + u".txt";
		source.sourceType = cSource::GUTENBERG_SOURCE_TYPE;
		processSource(source, forceSourceReread, sourceWordNetRead, sourceWordNetWrite, sourceWrite, viterbiTest, parseOnly, makeCopyBeforeSourceWrite,
			specialExtension, title, encoding, start, repeatStart, etext,
			numWordsOverAllSource, globalTotalUnmatched, globalOverMatchedPositionsTotal);
	}
	freeCounter();
	cProfile::lfprint(profile);
	// See the matching comment in startProcesses(): flush the logs and close MySQL
	// before the fast exit so _exit() does not silently drop buffered log output or
	// leave the DB socket to be torn down by the OS instead of closed cleanly.
	lplog();
	mysql_close(&source.mysql);
	_exit(0); // fast exit
}