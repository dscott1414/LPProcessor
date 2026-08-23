/*
	main.cpp - process entry point: command line parsing, one-time initialization, multi-process orchestration and the top-level per-document loop.

	Overview:
		This translation unit owns everything that happens around the parser proper.  It
		(a) installs the crash / Ctrl-C / out-of-memory handlers, (b) parses the command
		line into the handful of booleans and the source type that drive a run,
		(c) constructs the single cSource object (which connects to MySQL and loads the
		lexicon), (d) either spawns and supervises N child lp.exe processes (controller
		mode, -mp) or itself loops over the sources claimed from the `sources` table, and
		(e) for each source calls the pipeline in order: readSource/tokenize -> print
		(match) sentences -> WordNet extensions -> identifyObjects -> analyzeWordSenses ->
		syntacticRelations -> identifySpeakerGroups -> resolveSpeakers ->
		resolveFirstSecondPersonPronouns -> identifyConversations -> (optionally)
		question answering.  It also holds the definitions of a number of program-wide
		globals (the "unreachable but stable" iterator sentinels, the cProfile statics,
		and the SRWLOCKs) that must exist in exactly one translation unit.

	Pipeline position:
		Stage 0 - runs before and around every other stage.  wmain() is the only caller of
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
		- wmain() - top level: initialize, parse arguments, build cSource, then either startProcesses() or loop over sources
		- processCommandArguments() - fills the run configuration and the source type from argv
		- initialize() - crash handler, console, locks, memory counter, working directory, cache directory check
		- processSource() - the whole per-document pipeline for one already-claimed source
		- startProcesses() / createLPProcess() / waitToSpawnMoreProcesses() / waitForSpawnedProcesses() - controller mode
		- WRMemoryCheck() - copies the wordRelations table into its MEMORY-engine mirror
		- createMinidump() / printStackTrace() / unhandled_handler() - crash diagnostics

	Key data structures / globals:
		- static_wordMap / wNULL, static_tIcMap / tNULL, static_cLocalFocus / cNULL,
		  static_cSpeakerGroup / sgNULL, static_wm / wmNULL, static_setInt / sNULL -
		  the "never dereferenced but always comparable" iterator sentinels.  They exist
		  because _STLP_DEBUG rejects default-constructed iterators; the containers must
		  stay empty and must never be mutated.
		- cProfile:: statics - the profiling accumulators used by the LFS macro.
		- exitNow / exitEventually - set by ConsoleHandler (a different thread) and polled
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
		under CACHEDIR, the source texts under TEXTDIR, dbghelp.dll (minidumps),
		WinHTTP / cInternet (acquisition), yajl and MusicBrainz headers, and the sibling
		build outputs QuestionAnsweringx64\lp.exe, ParseAllSourcesx64\lp.exe and
		x64\StanfordAllSources\CorpusAnalysis.exe which controller mode spawns.

	Notes / gotchas:
		- Windows only: CreateProcess, WaitForMultipleObjectsEx, SRWLOCK, console API,
		  minidumps, wsprintf (which is the Win32 unbounded wsprintfW, not swprintf).
		- LMAINDIR / CACHEDIR / TEXTDIR are compile-time absolute paths ("F:\lp",
		  "M:\caches") from general.h; initialize() fails fatally if CACHEDIR is absent
		  even when -cacheDir names a valid directory.
		- Working directory dance: initialize() does chdir(".."), startProcesses() does
		  chdir("source") and restores it on return; every relative path below (tests\,
		  the child .exe paths, the .lplog files) depends on this.
		- Initialization order is load bearing: cSource's constructor connects to MySQL
		  and reads the lexicon, initializePatterns() fills desiredTagSets, and
		  initializePemaMap() must run after it.  initializePatterns() is skipped in
		  controller mode (-mp), so desiredTagSets is empty there.
		- lplog(LOG_FATAL_ERROR,...) does not return: logging.cpp blocks on getchar()
		  and then exit(0).  Any "fatal" path here therefore both hangs an unattended
		  run and reports success to the parent process.
		- wmain() and startProcesses() end with _exit(0), so no destructor runs: the
		  cSource destructor, the MySQL close and any unflushed buffered log are skipped
		  deliberately ("fast exit") because tearing down the lexicon takes minutes.
		- multiProcess and logFileExtension are __declspec(thread) (logging.h); they are
		  set on the main thread only, so worker threads see multiProcess==0.
		- This file is largely duplicated by specials_main.cpp (the specials.vcxproj
		  entry point), including getNumSourcesProcessed() and the lock definitions.
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
#include "bncc.h"
#include "mysql.h"
#include <direct.h>
#include <sys/stat.h>
#include <crtdbg.h>
extern "C" {
#include <yajl_tree.h>
}
#include "getMusicBrainz.h"
#include "profile.h"
#include <Dbghelp.h>
#include "mysqldb.h"
#include "internet.h"
#include "stacktrace.h"
#include "QuestionAnswering.h"

// needed for _STLP_DEBUG - these must be set to a legal, unreachable yet never changing value
unordered_map <wstring, cSourceWordInfo> static_wordMap;
tIWMM wNULL = static_wordMap.begin();
unordered_map<wstring, cSourceWordInfo::cRMap::cRelation> static_tIcMap;
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
__int64 cProfile::cb;
__int64 cProfile::accumulatedOverheadTime = 0;
unordered_map <string, __int64 > cProfile::counterMap;
unordered_map <string, int > cProfile::counterNumMap;
unordered_map <string, cProfile::CP> cProfile::timeMapTotal;
__int64 cProfile::totalCount = 0;
bool cProfile::lockInitialized = false;
string cProfile::functionPath;
set <unordered_map <string, cProfile::CP>::iterator, cProfile::timeSetCompare> cProfile::timeSort; // sort map by time taken by function
set <unordered_map <string, cProfile::CP>::iterator, cProfile::memorySetCompare> cProfile::memorySort; // sort map by memory allocated by function
set <unordered_map <string, cProfile::CP>::iterator, cProfile::countSetCompare> cProfile::countSort; // sort map by number of times function is called
__int64 cProfile::mySQLTotalTime = 0;
struct _RTL_SRWLOCK cProfile::networkTimeSRWLock;
int cProfile::totalInternetTimeWaitBandwidthControl;
__int64 cProfile::accumulationNetworkProfileTimer;
__int64 cProfile::accumulateOnlyNetTimer;
__int64 cProfile::lastNetworkTimePrinted;
__int64 cProfile::accumulateNetworkTimeCount;
int cProfile::lastNetClock;
int cInternet::internetWebSearchRetryAttempts = 1;
bool cQuestionAnswering::fileCaching = true;  // fileCaching determines whether they are cached on disk.  cOntology::cacheRdfTypes determines whether rdfTypes are cached in memory.  
unordered_map < wstring, __int64 > cProfile::netAndSleepTimes, cProfile::onlyNetTimes, cProfile::numTimesPerURL;


typedef long long (FAR WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType, CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);
bool unlockTables(MYSQL& mysql);
bool preTaggedSource = false; // BNC

// Write a minidump of this process to LMAINDIR\core.dmp ("F:\lp\core.dmp") describing the
// exception in apExceptionInfo.  Called from the unhandled exception filter, so it runs on
// the faulting thread with the stack still intact.
// Side effects: loads dbghelp.dll, creates/truncates core.dmp (FILE_SHARE_WRITE so a
// concurrent lp.exe can also be writing it - the dumps of sibling processes overwrite
// each other because the name is fixed).
// Note: none of LoadLibrary / GetProcAddress / CreateFile is checked, so a missing
// dbghelp.dll turns the original crash into a null call through pDump; mhLib is never
// freed (harmless, the process is dying).
void createMinidump(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
	HMODULE mhLib = ::LoadLibrary(L"dbghelp.dll");
	MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(mhLib, "MiniDumpWriteDump");
	wchar_t corePath[1024];
	wsprintf(corePath, L"%s\\core.dmp", LMAINDIR);
	HANDLE  hFile = ::CreateFile(corePath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	_MINIDUMP_EXCEPTION_INFORMATION ExInfo;
	ExInfo.ThreadId = ::GetCurrentThreadId();
	ExInfo.ExceptionPointers = apExceptionInfo;
	ExInfo.ClientPointers = FALSE;

	pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL);
	::CloseHandle(hFile);
}

// Format the current call stack (via dbg::stack_trace() from stacktrace.h) as
// "0xADDRESS: name(line) in module" per frame and write it to the log at LOG_FATAL_ERROR.
// Because LOG_FATAL_ERROR is fatal in logging.cpp, this call does not return: it flushes,
// waits for a keypress on stdin and exits.  Used only from the unhandled exception filter.
void printStackTrace()
{
	std::stringstream buff;
	buff << ":  General Software Fault! \n";
	buff << "\n";

	std::vector<dbg::StackFrame> stack = dbg::stack_trace();
	buff << "Callstack: \n";
	for (unsigned int i = 0; i < stack.size(); i++)
	{
		buff << "0x" << std::hex << stack[i].address << ": " << stack[i].name << "(" << std::dec << stack[i].line << ") in " << stack[i].module << "\n";
	}
	::lplog(LOG_FATAL_ERROR, L"%S", buff.str().c_str());
}

// Process-wide last chance exception filter, installed by initialize() through
// SetUnhandledExceptionFilter.  Dumps core and logs the stack, then returns
// EXCEPTION_CONTINUE_SEARCH so the default handler still runs (WER / debugger attach).
// In practice printStackTrace() exits first, so the return value rarely matters.
LONG WINAPI unhandled_handler(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
	createMinidump(apExceptionInfo);
	printStackTrace();
	return EXCEPTION_CONTINUE_SEARCH;
}

// Bulk downloader for a hand-made list file: every line is "<url>|<destination path>",
// UTF-16 encoded (hence the "rb" + fgetws).  The extension of the URL is appended to the
// destination path, the page is fetched with cInternet::readBinaryPage and written to that
// path, then the loop sleeps 5s to stay polite.
// Returns 0 always - including when the list file cannot be opened.
// Side effects: creates/overwrites every destination file, network traffic, logging.
// NOTE: this function is currently dead code - nothing in the project calls it, and it has
// never been hardened: a line without '|' null-derefs, _wopen failure (-1) is treated as
// success by "if (destfile)", and both the BOM strip and the wcscat below can corrupt or
// overrun url[].
int acquireList(wchar_t* filename)
{
	LFS
		FILE* listfile = _wfopen(filename, L"rb"); // binary mode reads unicode
	if (listfile)
	{
		wchar_t url[2048], * path;
		int total = 0;
		while (fgetws(url, 2047, listfile))
		{
			// shift the string down one wchar_t to drop a leading byte order mark - note
			// the length is in characters but memcpy wants bytes, and the terminating null
			// is not copied
			if (url[0] == 0xFEFF) // detect BOM
				memcpy(url, url + 1, wcslen(url + 1));
			if (url[wcslen(url) - 1] == L'\n') url[wcslen(url) - 1] = 0;
			if (url[wcslen(url) - 1] == L'\r') url[wcslen(url) - 1] = 0;
			wchar_t* ch = wcschr(url, L'|');
			*ch = 0;
			path = ch + 1;
			// path points into url[] just past the '|', so appending the URL's extension
			// here writes past the end of the line inside the same 2048 wchar_t buffer
			wchar_t* period = wcsrchr(url, L'.');
			if (period) wcscat(path, period);
			wstring buffer;
			int destfile = _wopen(path, O_RDWR | O_BINARY | O_CREAT);
			if (destfile)
			{
				if (cInternet::readBinaryPage(url, destfile, total))
					lplog(LOG_ERROR, L"error retrieving %s.", url);
				close(destfile);
			}
			else
				lplog(LOG_ERROR, L"error opening %s.", path);
			wprintf(L"%s - total bytes = %dMB.\n", path, total / 1024 / 1024);
			Sleep(5000);
		}
		fclose(listfile);
	}
	return 0;
}

/*
int reportInfo(WMISupport &provider)
{ LFS
static __int64 lastVirtualBytes=0,lastWorkingSet=0;

IWbemClassObject *resourceNameInstance;
bool status=provider.getResourceObject(L"Win32_PerfFormattedData_PerfProc_Process.Name='lp'",resourceNameInstance);
__int64 percentProcessorTime,percentUserTime,percentPrivilegedTime,elapsedTime,virtualBytes,workingSet;
status = provider.getWMIKey(resourceNameInstance,L"ElapsedTime",elapsedTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentProcessorTime",percentProcessorTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentUserTime",percentUserTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentPrivilegedTime",percentPrivilegedTime);
status = provider.getWMIKey(resourceNameInstance,L"VirtualBytes",virtualBytes);
status = provider.getWMIKey(resourceNameInstance,L"WorkingSet",workingSet);
lplog(L"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
wprintf(L"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d\n",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
lastVirtualBytes=virtualBytes;
lastWorkingSet=workingSet;
return 0;
}
*/

bool exitNow = false, exitEventually = false;

// Console control handler (installed by initialize()) implementing the two stage
// interrupt: the first Ctrl-C/Break/Close sets exitEventually, which makes the source loop
// stop after the current document; a second one also sets exitNow, which abandons the
// current document and skips signalFinishedProcessingSource().  A system shutdown goes
// straight to exitNow.
// Returns TRUE ("handled") in every case, including CTRL_CLOSE_EVENT, where Windows still
// kills the process a few seconds later - so a console close usually loses the document.
// Runs on a handler thread injected by the OS: exitNow/exitEventually are plain bools
// shared with the main thread without any synchronization.
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	LFS
		if (exitEventually) exitNow = true;
	exitEventually = true;
	switch (CEvent)
	{
	case CTRL_C_EVENT:
		wprintf(L"\nCTRL+C received! Interrupting %s...\n", (exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_BREAK_EVENT:
		wprintf(L"\nCTRL+Break received! Interrupting %s...\n", (exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_CLOSE_EVENT:
		wprintf(L"\nClose received! Interrupting %s...\n", (exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_LOGOFF_EVENT:
		wprintf(L"\nUser is logging off! Interrupting %s...\n", (exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_SHUTDOWN_EVENT:
		exitNow = true;
		wprintf(L"\nSystem is shutting down! Interrupting %s...\n", (exitNow) ? L"immediately" : L"at end of this source");
		break;
	}
	return TRUE;
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
int getInterviewTranscript();
int getTwitterEntries(wchar_t* filter);
bool TSROverride = false, flipTOROverride = false, flipTNROverride = false, logMatchedSentences = false, logUnmatchedSentences = false;

// new_handler installed by initialize(): logs the allocation failure and terminates.
// The exit(1) is unreachable because lplog(LOG_FATAL_ERROR,...) exits (with status 0)
// first, so an out-of-memory run still looks successful to whoever spawned it.
void no_memory() {
	lplog(LOG_FATAL_ERROR, L"Out of memory (new/STL allocation).");
	exit(1);
}

// Give this console a large scrollback (at least 200 columns x 6000 rows, so that the
// per-sentence parse dumps can be scrolled back through) and resize the visible window to
// width x height characters.  Failures are printed but otherwise ignored - they are
// expected when stdout is redirected or the process has no console.
// Note the buffer is always at least 200x6000 regardless of the arguments; only the window
// rectangle actually honours width/height.
void setConsoleWindowSize(int width, int height)
{
	HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);      // Get Handle 
	_COORD coord;
	coord.X = max(200, width);
	coord.Y = max(6000, height);
	if (!SetConsoleScreenBufferSize(Handle, coord))            // Set Buffer Size 
		printf("Cannot set console buffer info to (%d,%d) (%d) %s\n", coord.X, coord.Y, (int)GetLastError(), LastErrorStr());

	//CONSOLE_SCREEN_BUFFER_INFOEX  csbiInfo;
	//csbiInfo.cbSize = sizeof(csbiInfo);
	// Get the current screen buffer size and window position. 
	//if (!GetConsoleScreenBufferInfoEx(Handle, &csbiInfo))
	//	printf("Cannot get console buffer info (%d) %s\n", (int)GetLastError(), LastErrorStr());
	//_COORD maxSize = GetLargestConsoleWindowSize(Handle);

	//printf("buffer sizex=%d buffer sizey=%d buffer cursor=(%d,%d) \nattributeflags=%d buffer window=(top=%d,left=%d,bottom=%d,right=%d) \nmax given buf=(%d,%d) max absolute=(%d,%d) popupAttributes=%d fullScreen=%d\n", 
	//	csbiInfo.dwSize.X, csbiInfo.dwSize.Y, csbiInfo.dwCursorPosition.X, csbiInfo.dwCursorPosition.Y,
	//	(int)csbiInfo.wAttributes, csbiInfo.srWindow.Top, csbiInfo.srWindow.Left, csbiInfo.srWindow.Bottom, csbiInfo.srWindow.Right,
	//	csbiInfo.dwMaximumWindowSize.X, csbiInfo.dwMaximumWindowSize.Y,
	//	maxSize.X,maxSize.Y,
	//	(int)csbiInfo.wPopupAttributes,(int)csbiInfo.bFullscreenSupported);

	//height = min(height, csbiInfo.dwMaximumWindowSize.Y);
	//width = min(width, csbiInfo.dwMaximumWindowSize.X);


	_SMALL_RECT Rect;
	Rect.Top = 0;
	Rect.Left = 0;
	Rect.Bottom = height - 1;
	Rect.Right = width - 1;

	if (!SetConsoleWindowInfo(Handle, TRUE, &Rect))            // Set Window Size 	SMALL_RECT srctWindow;
		printf("Cannot set console window info to (top=%d,left=%d,bottom=%d,right=%d) (%d) %s\n",
			Rect.Top, Rect.Left, Rect.Bottom, Rect.Right,
			(int)GetLastError(), LastErrorStr());
}

// Spawn one child worker in its own console window, tiled vertically by numProcess
// (x=60, y=180*numProcess) and shown without stealing focus, so a controller run with -mp
// leaves a readable stack of child windows.
// commandPath is the executable, processParameters the full command line (CreateProcess
// may modify it in place, hence the non-const pointer).
// Out: processHandle / threadHandle / processId of the new child.
// Returns 0 on success, -1 if CreateProcess failed (in which case the out parameters are
// left as the caller initialized them and the failure is printed with the current
// directory, since a wrong working directory is the usual cause).
// Ownership: the caller inherits both handles; threadHandle is never closed by any caller,
// which leaks one thread handle per child.
int createLPProcess(int numProcess, HANDLE& processHandle, HANDLE& threadHandle, DWORD& processId, const wchar_t* commandPath, wchar_t* processParameters)
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.wShowWindow = true;
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE | STARTF_USECOUNTCHARS | STARTF_USESHOWWINDOW;
	si.dwX = 60;
	si.dwY = 180 * numProcess;
	si.dwXSize = 300;
	si.dwYSize = 500;
	si.dwXCountChars = 180;
	si.dwYCountChars = 3000;
	si.wShowWindow = SW_SHOWNOACTIVATE; // don't continuously hijack focus
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcess(commandPath,
		processParameters, // Command line
		NULL, // Process handle not inheritable
		NULL, // Thread handle not inheritable
		FALSE, // Set handle inheritance to FALSE
		CREATE_NEW_CONSOLE,
		NULL, // Use parent's environment block
		NULL, // Use parent's starting directory 
		&si, // Pointer to STARTUPINFO structure
		&pi) // Pointer to PROCESS_INFORMATION structure
		)
	{
		wchar_t cwd[1024];
		printf("CreateProcess of %S failed (%d) %s in %S.\n", processParameters, (int)GetLastError(), LastErrorStr(), _wgetcwd(cwd, 1024));
		return -1;
	}
	processHandle = pi.hProcess;
	processId = pi.dwProcessId;
	threadHandle = pi.hThread;
	return 0;
}


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
// truncate to 32 bits even though the out parameters are __int64.
int getNumSourcesProcessed(MYSQL& mysql, int sourceType, int& numSourcesProcessed, __int64& wordsProcessed, __int64& sentencesProcessed)
{
	MYSQL_RES* result;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return -1;
	wsprintf(qt, L"select COUNT(id), SUM(numWords), SUM(numSentences) from sources where sourceType = %d and processed IS not NULL and processing IS NULL and start != '**SKIP**' and start != '**START NOT FOUND**'", sourceType);
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
	if (!myquery(&mysql, L"UNLOCK TABLES")) return -1;
	return 0;
}

// https://stackoverflow.com/questions/813086/can-i-send-a-ctrl-c-sigint-to-an-application-on-windows/1179124
// Inspired from http://stackoverflow.com/a/15281070/1529139
// and http://stackoverflow.com/q/40059902/1529139
// Deliver a console control event (used with CTRL_C_EVENT) to another process, so that a
// Ctrl-C in the controller makes each child stop at the end of its current document
// instead of being killed.  Windows offers no direct API for this: the controller must
// leave its own console, attach to the child's, disable its own Ctrl-C handling (which is
// deliberately never restored - restoring it would kill the controller too), raise the
// event for the whole attached console group, then reattach or allocate a fresh console.
// Returns true only if GenerateConsoleCtrlEvent succeeded.
bool signalCtrl(DWORD dwProcessId, DWORD dwCtrlEvent)
{
	bool success = false;
	DWORD thisConsoleId = GetCurrentProcessId();
	// Leave current console if it exists
	// (otherwise AttachConsole will return ERROR_ACCESS_DENIED)
	bool consoleDetached = (FreeConsole() != FALSE);

	if (AttachConsole(dwProcessId) != FALSE)
	{
		// Add a fake Ctrl-C handler for avoid instant kill is this console
		// WARNING: do not revert it or current program will be also killed
		SetConsoleCtrlHandler(nullptr, true);
		success = (GenerateConsoleCtrlEvent(dwCtrlEvent, 0) != FALSE);
		FreeConsole();
	}

	if (consoleDetached)
	{
		// Create a new console if previous was deleted by OS
		if (AttachConsole(thisConsoleId) == FALSE)
		{
			int errorCode = GetLastError();
			if (errorCode == 31) // 31=ERROR_GEN_FAILURE
			{
				AllocConsole();
			}
		}
	}
	return success;
}

// Drain phase of controller mode: no more sources are left to hand out, so wait for the
// numProcesses children still in handles[] to exit, closing each handle and compacting the
// array as they do, and refreshing the throughput line in the console title every three
// minutes (the wait timeout) so a long tail is visible.
// In/out: numProcesses (decremented to 0), handles[] (compacted), nextProcessIndex (left
// holding the last slot that was retired).  startTime is a clock() value; the
// *Originally counters are the totals sampled before this run started, so that the title
// shows only this run's progress.
// NOTE: processingSeconds is integer seconds and is used as a divisor, so a child that
// exits in the first second of the run divides by zero; and if WaitForMultipleObjectsEx
// returns WAIT_FAILED the memmove below would run with nextProcessIndex == 0xFFFFFFFF
// (only survivable because the LOG_FATAL_ERROR above terminates the process).
void waitForSpawnedProcesses(MYSQL& mysql, const int sourceType, int &numProcesses, HANDLE* handles, unsigned int &nextProcessIndex, const int startTime,
	const int numSourcesProcessedOriginally, const int wordsProcessedOriginally, const int sentencesProcessedOriginally, const int maxProcesses)
{
	if (numProcesses)
	{
		wstring tmpstr;
		printf("\nNo more processes to be created. %d processes left to wait for.", numProcesses);
		while (numProcesses)
		{
			nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, false, 1000 * 60 * 3, false);
			if (nextProcessIndex == WAIT_FAILED)
				lplog(LOG_FATAL_ERROR, L"\nWaitForMultipleObjectsEx failed with error %s", getLastErrorMessage(tmpstr));
			int numSourcesProcessedNow = 0;
			__int64 wordsProcessedNow = 0, sentencesProcessedNow = 0;
			getNumSourcesProcessed(mysql, sourceType, numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
			int processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
			wchar_t consoleTitle[1500];
			numSourcesProcessedNow -= numSourcesProcessedOriginally;
			wordsProcessedNow -= wordsProcessedOriginally;
			sentencesProcessedNow -= sentencesProcessedOriginally;
			wsprintf(consoleTitle, L"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].",
				numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, wordsProcessedNow * 3600 / processingSeconds);
			lplog(LOG_INFO | LOG_ERROR, L"%s", consoleTitle);
			SetConsoleTitle(consoleTitle);
			if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
				continue;
			if (nextProcessIndex < WAIT_OBJECT_0 + numProcesses) // nextProcessIndex >= WAIT_OBJECT_0 && 
			{
				nextProcessIndex -= WAIT_OBJECT_0;
				CloseHandle(handles[nextProcessIndex]);
				printf("\nClosing process %u", nextProcessIndex);
			}
			if (nextProcessIndex >= WAIT_ABANDONED_0 && nextProcessIndex < WAIT_ABANDONED_0 + numProcesses)
			{
				nextProcessIndex -= WAIT_ABANDONED_0;
				printf("\nClosing process %u [abandoned]", nextProcessIndex);
				CloseHandle(handles[nextProcessIndex]);
			}
			memmove(handles + nextProcessIndex, handles + nextProcessIndex + 1, (maxProcesses - nextProcessIndex - 1) * sizeof(handles[0]));
			numProcesses--;
		}
	}
}

// Build the command line for one child worker from this run's flags and start it.
// processKind selects both the executable and the mode:
//   0 - QuestionAnsweringx64\lp.exe -ParseRequest 0 +   (answer the queued requests)
//   1 - ParseAllSourcesx64\lp.exe   -book 0 + -BC 0     (parse Gutenberg books)
//   2 - x64\StanfordAllSources\CorpusAnalysis.exe -step <step>  (external corpus analysis)
// The boolean flags are passed straight through as the -forceSourceReread/-SW/-SWNR/-SWNW/
// -parseOnly/-MCSW/-logMatchedSentences/-logUnmatchedSentences switches; nextProcessIndex
// is used both to tile the child's window and to suffix its log files.
// Returns the child process handle, or 0 if the process could not be created or
// processKind is unknown - callers store that 0 in handles[] regardless, which later makes
// WaitForMultipleObjectsEx fail with the misleading "WaitForMultipleObjectsEx failed".
// NOTE: the paths are relative to the "source" directory startProcesses() chdir'd into, so
// controller mode only works from a full build tree.  Every command line is formatted with
// the unbounded Win32 wsprintf into a 1024 wchar_t buffer, and case 1 then wcscat's
// specialExtension onto it.  specialExtension - not logFileExtension - is what is passed
// to the child's -log, so the parent's -log suffix is not propagated.
HANDLE createLPProcess(const int processKind, const bool forceSourceReread, const bool sourceWrite, const bool sourceWordNetRead, const bool sourceWordNetWrite, const bool parseOnly, const bool makeCopyBeforeSourceWrite,
	const int numSourcesPerProcess, wstring specialExtension, const int nextProcessIndex, const int step)
{
	HANDLE processHandle = 0, threadHandle = 0;
	DWORD processId = 0;
	wchar_t processParameters[1024];
	int errorCode;
	switch (processKind)
	{
		case 0:
			wsprintf(processParameters, L"QuestionAnsweringx64\\lp.exe -ParseRequest 0 + -cacheDir %s %s%s%s%s%s%s%s%s-numSourceLimit %d -log %s.%u", CACHEDIR,
				(forceSourceReread) ? L"-forceSourceReread " : L"",
				(sourceWrite) ? L"-SW " : L"",
				(sourceWordNetRead) ? L"-SWNR " : L"",
				(sourceWordNetWrite) ? L"-SWNW " : L"",
				(parseOnly) ? L"-parseOnly " : L"",
				(makeCopyBeforeSourceWrite) ? L"-MCSW " : L"",
				(logMatchedSentences) ? L"-logMatchedSentences " : L"",
				(logUnmatchedSentences) ? L"-logUnmatchedSentences " : L"",
				numSourcesPerProcess,
				specialExtension.c_str(),
				nextProcessIndex);
			// note the precedence here (and in the two cases below): '<' binds tighter than
			// '=', so errorCode receives the comparison result, not the return code, and
			// the break only leaves the switch - failure is not propagated to the caller
			if (errorCode = createLPProcess(nextProcessIndex, processHandle, threadHandle, processId, L"QuestionAnsweringx64\\lp.exe", processParameters) < 0)
				break;
			break;
		case 1:
			wsprintf(processParameters, L"ParseAllSourcesx64\\lp.exe -book 0 + -BC 0 -cacheDir %s %s%s%s%s%s%s%s%s-numSourceLimit %d -log %s.%u", CACHEDIR,
				(forceSourceReread) ? L"-forceSourceReread " : L"",
				(sourceWrite) ? L"-SW " : L"",
				(sourceWordNetRead) ? L"-SWNR " : L"",
				(sourceWordNetWrite) ? L"-SWNW " : L"",
				(parseOnly) ? L"-parseOnly " : L"",
				(makeCopyBeforeSourceWrite) ? L"-MCSW " : L"",
				(logMatchedSentences) ? L"-logMatchedSentences " : L"",
				(logUnmatchedSentences) ? L"-logUnmatchedSentences " : L"",
				numSourcesPerProcess,
				specialExtension.c_str(),
				nextProcessIndex);
			if (specialExtension.length() > 0)
			{
				wcscat(processParameters, L" -specialExtension ");
				wcscat(processParameters, specialExtension.c_str());
			}
			if (errorCode = createLPProcess(nextProcessIndex, processHandle, threadHandle, processId, L"ParseAllSourcesx64\\lp.exe", processParameters) < 0)
				break;
			break;
		case 2:
			wsprintf(processParameters, L"x64\\StanfordAllSources\\CorpusAnalysis.exe -step %d -numSourceLimit %d -log %s.%u", step, numSourcesPerProcess, specialExtension.c_str(), nextProcessIndex);
			if (errorCode = createLPProcess(nextProcessIndex, processHandle, threadHandle, processId, L"x64\\StanfordAllSources\\CorpusAnalysis.exe", processParameters) < 0)
				break;
			break;
		default: break;
	}
	printf("\nCreated process %u:%d", nextProcessIndex, (int)processId);
	return processHandle;
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
int waitToSpawnMoreProcesses(MYSQL& mysql, const int sourceType, const int numProcesses, HANDLE* handles, unsigned int &nextProcessIndex, const int startTime,
	const int numSourcesProcessedOriginally, const int wordsProcessedOriginally, const int sentencesProcessedOriginally, const int maxProcesses, int &numSourcesLeft)
{
	if (numProcesses == maxProcesses)
	{
		wstring tmpstr;
		nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, false, 1000 * 60 * 5, false);
		if (nextProcessIndex == WAIT_FAILED)
		{
			if (!numProcesses)
				return -1;
			lplog(LOG_FATAL_ERROR, L"WaitForMultipleObjectsEx failed with error %s", getLastErrorMessage(tmpstr));
		}
		numSourcesLeft = 0;
		int numSourcesProcessedNow = 0;
		__int64 wordsProcessedNow = 0, sentencesProcessedNow = 0;
		getNumSourcesProcessed(mysql, sourceType, numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
		int processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
		wchar_t consoleTitle[1500];
		numSourcesProcessedNow -= numSourcesProcessedOriginally;
		wordsProcessedNow -= wordsProcessedOriginally;
		sentencesProcessedNow -= sentencesProcessedOriginally;
		wsprintf(consoleTitle, L"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].",
			numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, wordsProcessedNow * 3600 / processingSeconds);
		SetConsoleTitle(consoleTitle);

		if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
			return 1;
		if (nextProcessIndex < WAIT_OBJECT_0 + numProcesses) // nextProcessIndex >= WAIT_OBJECT_0 && 
		{
			nextProcessIndex -= WAIT_OBJECT_0;
			CloseHandle(handles[nextProcessIndex]);
			printf("\nClosing process %u", nextProcessIndex);
		}
		if (nextProcessIndex >= WAIT_ABANDONED_0 && nextProcessIndex < WAIT_ABANDONED_0 + numProcesses)
		{
			nextProcessIndex -= WAIT_ABANDONED_0;
			printf("\nClosing process %u [abandoned]", nextProcessIndex);
			CloseHandle(handles[nextProcessIndex]);
		}
	}
	return 0;
}

// Forward this controller's interrupt to every live child exactly once, so each finishes
// (or abandons) its current document and exits cleanly rather than being killed.
// In/out: sentBreakSignals - latch making the broadcast idempotent even though the source
// loop calls this on every iteration once exitEventually is set.
// Note the "Sending break signals" message is printed even on the repeat calls that do
// nothing.
void sendBreakSignals(bool & sentBreakSignals, const int numProcesses, HANDLE* handles)
{
	printf("\nSending break signals to children...\n");
	if (!sentBreakSignals)
	{
		for (int p = 0; p < numProcesses; p++)
		{
			int pid = GetProcessId(handles[p]);
			signalCtrl(pid, CTRL_C_EVENT);
		}
		sentBreakSignals = true;
	}
}

bool getNextUnprocessedSource(MYSQL& mysql, int begin, int end, int sourceType, bool setUsed, int& id, wstring& path, wstring& encoding, wstring& start, int& repeatStart, wstring& etext, wstring& author, wstring& title);
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
// the break paths; the calloc is unchecked; and because of the _exit(0) the free(handles)
// and the chdir("..") are unreachable in every mode except REQUEST_TYPE.
// NOTE: maxProcesses is not clamped to MAXIMUM_WAIT_OBJECTS (64); a larger -mp makes
// WaitForMultipleObjectsEx fail immediately and the run dies with an unrelated message.
int startProcesses(MYSQL& mysql, int sourceType, int processKind, int step, int beginSource, int endSource, cSource::sourceTypeEnum processSourceType, int maxProcesses, int numSourcesPerProcess,
	bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite, bool makeCopyBeforeSourceWrite, bool parseOnly, wstring specialExtension)
{
	LFS
		if (chdir("source") < 0)
			return -1;
	bool sentBreakSignals = false;
	int startTime = clock();
	HANDLE* handles = (HANDLE*)calloc(maxProcesses, sizeof(HANDLE));
	int numProcesses = 0, errorCode = 0, numSourcesProcessedOriginally = 0, numSourcesLeft;
	__int64 wordsProcessedOriginally = 0, sentencesProcessedOriginally = 0;
	if (processKind == 0)
		sourceType = cSource::REQUEST_TYPE;
	getNumSourcesProcessed(mysql, sourceType, numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally);
	numSourcesLeft = getNumSources(mysql, sourceType, true);
	maxProcesses = min(maxProcesses, numSourcesLeft);
	wstring tmpstr;
	while (!errorCode)
	{
		unsigned int nextProcessIndex = numProcesses;
		int breakContinueResult = waitToSpawnMoreProcesses(mysql, sourceType, numProcesses, handles, nextProcessIndex, startTime,
			numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally, maxProcesses, numSourcesLeft);
		if (breakContinueResult < 0) break;
		if (breakContinueResult > 0)  continue;
		int id, repeatStart;
		wstring start, path, encoding, etext, author, title, pathInCache;
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
				memmove(handles + nextProcessIndex, handles + nextProcessIndex + 1, (maxProcesses - nextProcessIndex - 1) * sizeof(handles[0]));
				numProcesses--;
			}
			waitForSpawnedProcesses(mysql, sourceType, numProcesses, handles, nextProcessIndex, startTime,
				numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally, maxProcesses);
			break;
		}
		if (exitNow || exitEventually)
			sendBreakSignals(sentBreakSignals, numProcesses, handles);
		else
		{
			HANDLE processHandle= createLPProcess(processKind, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite, parseOnly, makeCopyBeforeSourceWrite,
				numSourcesPerProcess, specialExtension, nextProcessIndex, step);
			handles[nextProcessIndex] = processHandle;
			if (numProcesses < maxProcesses)
				numProcesses++;
		}
	}
	if (processSourceType != cSource::REQUEST_TYPE)
	{
		freeCounter();
		_exit(0); // fast exit
	}
	free(handles);
	return chdir("..");
}

SRWLOCK rdfTypeMapSRWLock, mySQLTotalTimeSRWLock, totalInternetTimeWaitBandwidthControlSRWLock, mySQLQueryBufferSRWLock, orderedHyperNymsMapSRWLock;
// Initialize the process-wide reader/writer locks declared extern in general.h, before any
// worker thread can run.  Called once from initialize().
// NOTE: orderedHyperNymsMapSRWLock is declared above but not initialized here even though
// getWordNet.cpp acquires it; it only works because a zero-initialized global happens to
// match what InitializeSRWLock produces.
void createLocks(void)
{
	LFS
	InitializeSRWLock(&rdfTypeMapSRWLock);
	InitializeSRWLock(&mySQLTotalTimeSRWLock);
	InitializeSRWLock(&totalInternetTimeWaitBandwidthControlSRWLock);
	InitializeSRWLock(&mySQLQueryBufferSRWLock);
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
	if (!myquery(&mysql, L"LOCK TABLES wordRelationsMemory WRITE,wordRelations READ"))
		return -1;
	if (!myquery(&mysql, L"SELECT COUNT(sourceId) from wordRelationsMemory", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsInMemory = atoi(sqlrow[0]);
	else
		return -1;
	if (numRowsInMemory > 0)
	{
		myquery(&mysql, L"UNLOCK TABLES");
		return 0;
	}
	if (!myquery(&mysql, L"SELECT COUNT(sourceId) from wordRelations", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsOnDisk = atoi(sqlrow[0]);
	printf("Loading wordrelations into memory...\n");
	if (!myquery(&mysql, L"insert into wordrelationsmemory select id, sourceId, lastWhere, fromWordId, toWordId, typeId, totalCount from wordrelations"))
		return -1;
	printf("Finished wordrelations into memory...\n");
	if (!myquery(&mysql, L"SELECT COUNT(sourceId) from wordRelationsMemory", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsInMemory = atoi(sqlrow[0]);
	else
		return -1;
	myquery(&mysql, L"UNLOCK TABLES");
	return (numRowsInMemory == numRowsOnDisk) ? 0 : -1;
}

int numSourceLimit = 0;
void processCommandArguments(int argc, wchar_t* argv[],
	bool &forceSourceReread, bool& sourceWrite, bool& sourceWordNetRead, bool& sourceWordNetWrite, 
	bool &resetAllSource, bool &resetProcessingFlags, bool &generateFormStatistics, bool &retry,
	bool &parseOnly, bool &makeCopyBeforeSourceWrite,
	int &sourceArgs, int &numSourcesPerProcess, enum cSource::sourceTypeEnum &sourceType,
	wstring &specialExtension, wchar_t sourceHost[])
{
	int numCommandLineParameters = argc;
	for (int I = 0; I < argc; I++)
	{
		if (!_wcsicmp(argv[I], L"-server") && I < argc - 1)
			wcscpy(sourceHost, argv[++I]);
		else if (!_wcsicmp(argv[I], L"-log") && I < argc - 1)
			logFileExtension = argv[++I];
		else if (!_wcsicmp(argv[I], L"-mp") && I < argc - 1)
			multiProcess = _wtoi(argv[++I]);
		else if (!_wcsicmp(argv[I], L"-numSourcesPerProcess") && I < argc - 1)
			numSourcesPerProcess = _wtoi(argv[++I]);
		else if (!_wcsicmp(argv[I], L"-numSourceLimit") && I < argc - 1)
			numSourceLimit = _wtoi(argv[++I]);
		else if (!_wcsicmp(argv[I], L"-cacheDir") && I < argc - 1)
			cacheDir = argv[++I];
		else if (!_wcsicmp(argv[I], L"-resetAllSource"))
			resetAllSource = true;
		else if (!_wcsicmp(argv[I], L"-resetProcessingFlags"))
			resetProcessingFlags = true;
		else if (!_wcsicmp(argv[I], L"-generateFormStatistics"))
			generateFormStatistics = true;
		else if (!_wcsicmp(argv[I], L"-retry"))
			retry = true;
		else if (!_wcsicmp(argv[I], L"-parseOnly"))
			parseOnly = true;
		else if ((!_wcsicmp(argv[I], L"-LC") || !_wcsicmp(argv[I], L"-logCache")) && I < argc - 1)
			logCache = _wtoi(argv[I + 1]);
		else if ((!_wcsicmp(argv[I], L"-BC")) && I < argc - 1) // bandwidth control
			cInternet::bandwidthControl = _wtoi(argv[I + 1]);
		else if (!_wcsicmp(argv[I], L"-logMatchedSentences"))
			logMatchedSentences = true;
		else if (!_wcsicmp(argv[I], L"-logUnmatchedSentences"))
			logUnmatchedSentences = true;
		else if (!_wcsicmp(argv[I], L"-TSRO"))
			TSROverride = true;
		else if (!_wcsicmp(argv[I], L"-fTOR"))
			flipTOROverride = true;
		else if (!_wcsicmp(argv[I], L"-fTNR"))
			flipTNROverride = true;
		else if (!_wcsicmp(argv[I], L"-forceSourceReread"))
			forceSourceReread = true;
		else if (!_wcsicmp(argv[I], L"-SW"))
			sourceWrite = true;
		else if (!_wcsicmp(argv[I], L"-MCSW"))
			makeCopyBeforeSourceWrite = true;
		else if (!_wcsicmp(argv[I], L"-SWNR"))
			sourceWordNetRead = true;
		else if (!_wcsicmp(argv[I], L"-SWNW"))
			sourceWordNetWrite = true;
		else if (!_wcsicmp(argv[I], L"-specialExtension"))
			specialExtension = argv[++I];
		else
			continue;
		numCommandLineParameters = min(numCommandLineParameters, I);
	}
	sourceArgs = -1;
	sourceType = cSource::NO_SOURCE_TYPE;
	const wchar_t* where;
	for (int I = 1; I < numCommandLineParameters - 1; I++)
	{
		wstring arg = argv[I];
		std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
		if ((where = wcsstr(L"1-test 2-book 3-newsbank 4-bnc 5-script 6-websearch 7-wikipedia 8-interactive :-parserequest", arg.c_str())))
		{
			sourceType = (enum cSource::sourceTypeEnum)(where[-1] - '0');
			sourceArgs = I;
			break;
		}
	}
	if (sourceArgs == -1)
		lplog(LOG_FATAL_ERROR, L"Source type not found.");
}

int processPatternTransformTypeSource(cSource &source, wchar_t *path)
{
	wchar_t consoleTitle[1500];
#ifdef _DEBUG
	wchar_t displayDebugFlag = L'D';
#else
	wchar_t displayDebugFlag = L'R';
#endif
	wsprintf(consoleTitle, L"[%c] %s...", displayDebugFlag, path);
	_putws(consoleTitle);
	lplog(LOG_INFO | LOG_ERROR, L"%s\n", consoleTitle);
	SetConsoleTitle(consoleTitle);
	unlockTables(source.mysql);
	Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
	cSource* requestedSource;
	cQuestionAnswering qa;
	return qa.processPath(&source, path, requestedSource, cSource::WEB_SEARCH_SOURCE_TYPE, 1, false);
}

void processSource(cSource &source, bool forceSourceReread, bool sourceWordNetRead, bool sourceWordNetWrite, bool sourceWrite, bool viterbiTest, bool parseOnly, bool makeCopyBeforeSourceWrite,
	const wstring specialExtension, wstring title, wstring& encoding, wstring& start, int& repeatStart, wstring& etext,
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
					lplog(LOG_ERROR, L"ERROR:Unable to parse %s - %d (start=%s, repeatStart=%d).", source.sourcePath.c_str(), ret, start.c_str(), repeatStart);
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
			lplog(LOG_FATAL_ERROR, L"Cannot print sentences.");
		globalTotalUnmatched += totalUnmatched;
	}
	else
	{
		lplog(LOG_INFO, L"%s already parsed.", source.sourcePath.c_str());
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
		lplog(LOG_FATAL_ERROR, L"buffer overrun");
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
		source.printTenseStatistics(L"Narrator", source.narratorTenseStatistics, source.numTotalNarratorVerbTenses);
		source.printTenseStatistics(L"Speaker", source.speakerTenseStatistics, source.numTotalSpeakerVerbTenses);
		source.printTenseStatistics(L"Narrator All Tenses", source.narratorFullTenseStatistics, source.numTotalNarratorFullVerbTenses);
		source.printTenseStatistics(L"Speaker All Tenses", source.speakerFullTenseStatistics, source.numTotalSpeakerFullVerbTenses);
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

void initialize()
{
	// Create a dump file whenever this program crashes (only on windows)
	SetUnhandledExceptionFilter(unhandled_handler);
	setConsoleWindowSize(100, 8);
	createLocks();
	set_new_handler(no_memory);
	SetConsoleCtrlHandler(ConsoleHandler, true);
	initializeCounter();
	wchar_t dir[1024];
	GetCurrentDirectoryW(1024, dir);
	if (chdir(".."))
		exit(-1);
	cacheDir = CACHEDIR;
	if (_waccess(cacheDir, 0) < 0)
		lplog(LOG_FATAL_ERROR, L"Cache directory %s does not exist!", cacheDir);
}

void printWordMatchingStatistics(int numWordsOverAllSource, int globalTotalUnmatched, int globalOverMatchedPositionsTotal, int overallTime)
{
	if (numWordsOverAllSource)
	{
		wprintf(L"\n%d milliseconds elapsed (%d words, %d unmatched (%5.2f%%) %d overmatched (%5.2f%%)",
			(int)((clock() - overallTime) / (CLOCKS_PER_SEC / 1000)), numWordsOverAllSource,
			globalTotalUnmatched, (float)globalTotalUnmatched * 100 / numWordsOverAllSource, globalOverMatchedPositionsTotal, (float)globalOverMatchedPositionsTotal * 100 / numWordsOverAllSource);
		lplog(L"%d milliseconds elapsed (%d words, %d unmatched (%5.2f%%) %d overmatched (%5.2f%%)",
			(int)((clock() - overallTime) / (CLOCKS_PER_SEC / 1000)), numWordsOverAllSource,
			globalTotalUnmatched, (float)globalTotalUnmatched * 100 / numWordsOverAllSource, globalOverMatchedPositionsTotal, (float)globalOverMatchedPositionsTotal * 100 / numWordsOverAllSource);
		lplog();
	}
	else
		lplog(L"%d milliseconds elapsed. No words processed.", (clock() - overallTime) / (CLOCKS_PER_SEC / 1000));
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
  if (argc>1 && !wcscmp(argv[1],L"-tg"))
  {
  	cSource source2(L"localhost",0,false,true,true);
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
	getArtists(L"artist",L"Jay-Z",mbArtistsTypes);
	getArtists(L"compactLabel",L"Roc-A-Fella Records",mbArtistsTypes);
	getReleases(L"compactLabel",L"Roc-A-Fella Records",mbReleasesTypes);
	getRecordings(L"artist",L"Jay-Z",mbRecordingsTypes);
// TEST thesaurus
	// build thesaurus
	//source.createThesaurusTables();
	//extern vector <sDefinition> thesaurus;
	//for (int I = 0; I < thesaurus.size(); I++)
	//	source.writeThesaurusEntry(thesaurus[I]);
	// synonym testing
	//vector <set <wstring> > synonyms;
	//source.getWordNetSynonymsOnly(L"car", synonyms, 1);
	//for (int I = 0; I < synonyms.size(); I++)
		//for (set<wstring>::iterator ss = synonyms[I].begin(), ssEnd = synonyms[I].end(); ss != ssEnd; ss++)
			//printf("%d:%S\n", I, ss->c_str());

// TEST PATTERNS
	//source.accumulateNewPatterns();
	//source.printAccumulatedPatterns();

// NOUN/VERB class analysis (debugging)
	//bool measurableObject,notMeasurableObject,grouping;
	//analyzeNounClass(0,L"fish",0,measurableObject,notMeasurableObject,grouping,t);
	//wstring proposedSubstitute;
	//int inflectionFlags=0;
	//bool isNoun=false,isVerb=true,isAdjective=false,isAdverb=false;
	//analyzeSense(false,L"draft",proposedSubstitute,numIrregular,inflectionFlags,isNoun,isVerb,isAdjective,isAdverb);
*/
int wmain(int argc, wchar_t* argv[])
{
	initialize();
	bool viterbiTest = false;
	cProfile profile("");
	int overallTime = clock();
	wchar_t sourceHost[1024];
	wcscpy(sourceHost, L"localhost");
	bool forceSourceReread = false, sourceWrite = false, sourceWordNetRead = false, sourceWordNetWrite = false;
	bool resetAllSource = false, resetProcessingFlags = false, generateFormStatistics = false, retry = false;
	bool parseOnly = false, makeCopyBeforeSourceWrite = false;
	int sourceArgs = -1, numSourcesPerProcess = 5;
	enum cSource::sourceTypeEnum sourceType;
	wstring specialExtension;
	processCommandArguments(argc, argv, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite, 
		resetAllSource, resetProcessingFlags, generateFormStatistics, retry,
		parseOnly, makeCopyBeforeSourceWrite,	sourceArgs, numSourcesPerProcess, sourceType, specialExtension,	sourceHost);
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
		int beginSource = _wtoi(argv[sourceArgs + 1]), endSource;
		endSource = (argv[sourceArgs + 2][0] == '+') ? -1 : ((iswdigit(argv[sourceArgs + 2][0])) ? _wtoi(argv[sourceArgs + 2]) : beginSource + 1);
		if (retry)
		{
			wprintf(L"Resetting sources...               \r");
			source.resetSource(beginSource, endSource);
		}
		if (multiProcess > 0)
		{
			HWND consoleWindowHandle = GetConsoleWindow();
			SetWindowPos(consoleWindowHandle, HWND_NOTOPMOST, 900, 0, 700, 180, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
			startProcesses(source.mysql, source.sourceType, 1, 0, beginSource, endSource, sourceType, multiProcess, numSourcesPerProcess, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite, makeCopyBeforeSourceWrite, parseOnly, specialExtension);
			return 0;
		}
		wprintf(L"Getting number of sources to process...               \r");
		int numSources = getNumSources(source.mysql, source.sourceType, false);
		int numSourcesProcessed = 0, pid = GetCurrentProcessId();
		while (!exitNow && !exitEventually && (numSourceLimit == 0 || numSourcesProcessed++ < numSourceLimit))
		{
			int repeatStart;
			wstring path, encoding, etext, author, title, start;
			wprintf(L"Getting number of sources left...               \r");
			int numSourcesLeft = getNumSources(source.mysql, source.sourceType, true);
			if (!getNextUnprocessedSource(source.mysql, beginSource, endSource, source.sourceType, true, source.sourceId, path, encoding, start, repeatStart, etext, author, title))
				break;
			path.insert(0, L"\\");
			path = path.insert(0, TEXTDIR);
			wchar_t consoleTitle[1500];
			wsprintf(consoleTitle, L"[%03d:%03d-%03d:%03d%%]PID%05d %s '%s'...", source.sourceId, beginSource, numSources, (numSources - numSourcesLeft) * 100 / numSources, pid, (start == L"**SKIP**" || start == L"**START NOT FOUND**") ? L"Skipping" : L"", title.c_str());
			_putws(consoleTitle);
			lplog(LOG_INFO | LOG_ERROR, L"%s\n", consoleTitle);
			SetConsoleTitle(consoleTitle);
			unlockTables(source.mysql);
			if (start == L"**SKIP**")
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
		wstring start = L"~~BEGIN", title, etext, encoding = L"NOT FOUND";
		if (argv[sourceArgs + 2][0] == L'~')
			start = argv[sourceArgs + 2];
		int repeatStart = 1;
		source.sourcePath = L"tests\\" + std::wstring(argv[sourceArgs + 1]) + L".txt";
		source.sourceType = cSource::GUTENBERG_SOURCE_TYPE;
		processSource(source, forceSourceReread, sourceWordNetRead, sourceWordNetWrite, sourceWrite, viterbiTest, parseOnly, makeCopyBeforeSourceWrite,
			specialExtension, title, encoding, start, repeatStart, etext,
			numWordsOverAllSource, globalTotalUnmatched, globalOverMatchedPositionsTotal);
	}
	freeCounter();
	cProfile::lfprint(profile);
	_exit(0); // fast exit
}