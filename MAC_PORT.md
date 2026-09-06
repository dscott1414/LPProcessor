# LPProcessor macOS port — plan and running status

This tracks the port of the first-party C++ engine (`lp` and `CorpusAnalysis`)
from Windows/MSVC to macOS/clang + CMake. It exists because the plan previously
lived only in a chat transcript; treat this file as the authoritative record of
what has been decided and what has been done.

Nothing here supersedes `CODE_REVIEW.md`, which covers a separate, already-closed
correctness/hygiene effort against the same tree. (That document now also absorbs
what used to be `PROPOSED_FIXES.md`; the two were merged and the second deleted.)

## How to build

```sh
cd source/build
cmake ..
cmake --build . --target lpcore -j 8 -- -k   # -k: keep going past the files that
                                             # later batches have not ported yet
cmake --build . --target lpchar_smoketest   && ./bin/lpchar_smoketest
cmake --build . --target lpprocess_smoketest && ./bin/lpprocess_smoketest
cmake --build . --target lp_smoketest        && ./bin/lp_smoketest
cmake --build . --target lpfile_smoketest    && (cd .. && ./build/bin/lpfile_smoketest)
```

**The port is complete.** All 48 `lpcore` files compile, `liblpcore.a` archives, and
both `lp` and `CorpusAnalysis` link and run as arm64 binaries. The four smoke-test
targets are deliberately independent of `lpcore` and must always pass.

## External dependencies — all resolved

| Dependency | Where it comes from | Status |
|---|---|---|
| yajl (JSON) | vendored, `lloyd-yajl-66cb08c/` | builds |
| tinyxml2 | vendored, `tinyxml2-master/`, compiled into `lpcore` | in the source list |
| WordNet 2.1 + `wn` wrapper | vendored outside the repo at `../wn/`, `../WordNet/2.1/` | builds (`libwn.a`) |
| MySQL Connector/C | `brew install mysql-client` → `/opt/homebrew/opt/mysql-client` | found (26.7.0) |
| JNI | Temurin 21 arm64 JDK via `/usr/libexec/java_home -v 21` | found |
| libcurl | system, 8.7.1 | found |

The Connector/C version gap is worth flagging: this code was written against a
much older Connector/C, so API drift (`my_bool`, retired `mysql_options` codes,
and so on) is a live possibility for batch B5 to discover.

**MySQL on this machine is now entirely arm64.**

- `/opt/homebrew/opt/mysql-client` — Connector/C 26.7.0, arm64, client libraries
  only. This is what CMake finds and what the binaries link against;
  `MYSQL_LIBRARY` in `build/CMakeCache.txt` should point here.
- `/opt/homebrew/opt/mysql@8.0` — MySQL **server** 8.0.46, arm64, running as a
  login service via `brew services`. Configured in `/opt/homebrew/etc/my.cnf`.

The x86_64 MySQL 8.0.23 that used to live at `/usr/local/mysql` (and ran under
Rosetta) has been removed, along with the architecture-mismatch trap it created.

The `lp` database is restored: 51 tables, 42.32 GB. One server setting is
load-bearing rather than a tuning choice — `max_heap_table_size = 4G`.
`WRMemoryCheck()` (`main.cpp`) mirrors all 25,040,632 rows of `wordRelations`
into the MEMORY-engine `wordRelationsMemory` at startup, which measures 2.04 GB;
under MySQL's 16 MB default only 206,388 rows fit before
`ERROR 1114: The table 'wordrelationsmemory' is full`, so the parser could not
start at all.

## Key design decisions (made once, in B0/B1 — later batches must not re-litigate)

- **`lpchar_t` / `lpwstring` replace `wchar_t` / `wstring` everywhere.** `wchar_t`
  is 2 bytes on Windows and 4 on macOS, and the binary source-cache format assumes
  2. `lpchar_t` is `char16_t`, `lpwstring` is `std::u16string`. See `lpchar.h`.
- **`lpchar.h` provides the printf engine** (`lp_snprintf`/`lp_vsnprintf`/
  `lp_wprintf`/`lp_fwprintf`) and the string primitives (`lp_strlen`, `lp_wtoi`,
  `lp_wcscasecmp`, ...). No platform ships a `char16_t` printf, so this is
  required regardless of target OS. `%I64d` is accepted, so MSVC-style format
  strings need no per-call-site edits.
- **`utfConvert.h` provides the UTF-8 ↔ UTF-16 codec**, preserving the original
  `mTW()` encoding-detection ladder exactly, because cached corpora were parsed
  under it. `wTM`/`mTW`/`mTWCodePage` get rewired onto it in B7.
- **Locks are `std::shared_mutex`**, taken with scoped `unique_lock`/`shared_lock`.
  The `...SRWLock` identifiers are kept as historical names.
- **`__int64`/`unsigned __int64` → `int64_t`/`uint64_t`**; `__declspec(thread)` →
  `thread_local`.
- Sibling-binary discovery uses `_NSGetExecutablePath`; console/progress UI
  collapses into a single `lpReportProgress()` helper. Both live in
  **`lpProcess.h`/`lpProcess.cpp`** (added in B4a) along with the rest of the
  portable process layer, so `main.cpp` and `specials_main.cpp` share one copy
  rather than drifting apart the way their existing duplicated code already has.

## Disk I/O — where it happens, and how paths are targeted

Every filesystem access in the built engine is inventoried here, because the
whole of it depends on one translation that is invisible when it breaks.

**One chokepoint.** All 178 filesystem call sites reach the OS through
`lpNarrowPath()` in `lpFile.cpp`. It encodes a wide path to UTF-8 *and translates
`\` to `/`*. The `lp_w*` family (`lp_wfopen`, `lp_wopen`, `lp_wremove`,
`lp_waccess`, `lp_wmkdir`, `lp_wrename`, `lp_wstat`, `lp_filelength`,
`lp_wIsDirectory`, `lpDirectoryEntries`) all go through it, which covers ~150 of
those sites. The rest are raw `open`/`access`/`mkdir` calls that narrow their own
path; they call `lpNarrowPath()` directly, and are in `source.cpp` (3),
`Internet.cpp` and `main.cpp`.

**Why the translation is needed.** Paths here are built with `\`: 123 string
literals of the form `u"%s\\dbPediaCache\\_%s.txt"`, plus
`distributeToSubDirectories()` (`getDictionary.cpp`), which writes `\` into
`path[1]` and `path[3]` by hand to build the two-level fan-out the caches use. On
Windows that was the separator. On macOS `\` is an ordinary filename character,
so without translation every constructed path names one long nonexistent file.

**Why translating unconditionally is safe.** No path component can contain a
backslash: `convertIllegalChars()` (`getWikipedia.cpp`) rewrites every character
of `WCHAR_ILLEGAL_PATH_CHARS` — which contains both `\` and `/` — to `!` before a
name is substituted into a path. So a `\` reaching `lpNarrowPath()` was always
put there by the code as a separator, never by data. The restored caches agree:
zero of their 3,775,294 files have a backslash in the name.

This is what lets the roots be POSIX while all 123 `\`-separated literals stay
untouched — the two spellings mix freely.

**The roots** (`general.h`, all overridable):

| Macro | Value | Env override | Also |
|---|---|---|---|
| `MAINDIR` / `LMAINDIR` | `/Users/davidscott/lp` | `LP_MAIN_DIR` | |
| `CACHEDIR` | `/Users/davidscott/lp/caches` | `LP_CACHE_DIR` | `-cacheDir` |
| `WEBSEARCH_CACHEDIR` | `/Users/davidscott/lp/caches` | `LP_WEBSEARCH_CACHE_DIR` | |
| `TEXTDIR` | `/Users/davidscott/lp/caches` | `LP_TEXT_DIR` | |

The three cache roots are one directory, as they were on Windows (`M:\caches`).
They stay separate macros because each has its own environment override.

**Working directory matters.** Roughly 32 reads use paths relative to the main
directory (`source\lists\...`, `tests\...`). `initialize()` does `chdir("..")`,
so **the binary must be launched from `<MAINDIR>/source`**; `startProcesses()`
does `chdir("source")` and restores it. Launched from anywhere else, every one of
those reads misses.

**Verify with** `lpfile_smoketest` (below), run from the main directory.

### Running the engine — the environment it needs

Established by actually running it; none of this is optional.

```sh
cd /Users/davidscott/lp/source          # initialize() does chdir("..") from here
export LP_DB_PASSWORD=...               # required; envConfig.h aborts without it
export WNSEARCHDIR=/Users/davidscott/lp/WordNet/2.1/dict
export WNHOME=/Users/davidscott/lp/WordNet/2.1
./build/bin/lp -test thatParsing -BC 0 -forceSourceReread -parseOnly
```

- **`WNSEARCHDIR`** is what `wninit()` reads (`wn/wnutil.c`: `WNSEARCHDIR`, else
  `WNHOME/dict`, else `DEFAULTPATH`). The Windows build got this from the
  registry, so nothing in the tree supplies it on macOS and initialization fails
  with "WordNet failed initialization!" until it is set.
- **Launch directory** — see "Working directory matters" above.
- The per-source result lands in `main.lplog` (`Matched sentences=...`), not on
  stdout; stdout carries only progress lines.

### What the caches contain, and what reads them

| Directory | Size / files | Layout | Read by |
|---|---|---|---|
| `dbPediaCache` | 154 GB / 1,164,362 | char fan-out | `createOntology.cpp`, `questionAnswering.cpp` |
| `texts` | 14 GB / 35,834 | by author | source loading (`TEXTDIR\texts`) |
| `wikipediaCache` | 3.6 GB / 1,325,736 | char fan-out | `getWikipedia.cpp` |
| `Webster` | *emptied* | — | nothing (deleted 2026-09-05) |
| `gutenbergCatalog` | 901 MB / 58,969 | flat | **nothing in the built tree** |
| `wordNetCache` | 434 MB / 111,204 | flat | `getWordNet.cpp` |
| `musicBrainzCache` | 3.6 MB / 83 | char fan-out | `getMusicBrainz.cpp` |

`caches/Webster` held the Merriam-Webster cache, orphaned when that integration
was removed (see `CODE_REVIEW.md` open item 1). Its 4.5 GB / 1,079,105 files were
deleted on 2026-09-05 at the author's request; the empty directory is left in
place. `caches/gutenbergCatalog` (901 MB) is named only in `README.md` and is
still present.

`webSearchCache` does not exist yet and does not need to: `cInternet::getWebPath`
`lp_wmkdir`s it before first use. `caches/DictionaryDotCom` never existed here and
is no longer referenced — the `-specials` step that swept it has been removed.

### Paths that still point at things this machine does not have

Not errors, but a run touching them will fail rather than silently degrade:

- `getWordNet.cpp` `initializeNounVerbMapping()` reads an external WordNet 2.1
  `index.noun`, which is not installed. It is only reached when
  `source/lists/nounVerbMapping` is missing — that file exists (452 KB), and the
  branch above returns first. Its `fopen` result used to be passed straight to
  `fgets()`, so a miss segfaulted; it is now checked, logs, and returns -1.
  `LP_WORDNET_DICT` overrides the path.
- `checkTypes/`, `correctRDF/` and `convertPDFTextToDatabase/` still contain
  `F:\`, `G:\` and `E:\` paths. None is in the CMake build.
- The `#ifdef TEST_CODE` block in `Internet.cpp` (~line 583) still has Windows
  separators and a one-argument `mkdir`; it has never compiled on any platform.

## Corpus status (2026-09-06)

**The engine parses real documents on macOS.** 14 of the 23 files in `tests/`
parse end to end -- roughly 26,900 positions -- at 98.15-100% matched sentences.

**These numbers are a snapshot, not a measurement: the parse is not reproducible
run to run.** Three consecutive runs of `VBGVBD incorrect` on an unchanged binary
and unchanged inputs gave 3,932 positions twice and 3,953 the third time, with
unknown words moving 213 -> 223. Matched-sentence percentage held at 98.15% in
all three, and the two `MS/word` figures that differ across runs are a timing
metric, but the position and unknown counts are a genuine difference in how the
text was tokenized. `wordFormCache` was byte-identical before each run, so it is
not the cause; the per-source `tests/*.wordCacheFile` were never rewritten
either. The likely class is iteration order over the engine's many
`unordered_map`s feeding back into decisions, but that is not established -- it
needs someone to actually chase it. **Until it is understood, treat any
comparison of these percentages between two runs as noise at the ~0.5% level on
position counts.**

| Test | Positions | Matched |
|---|---|---|
| VBGVBD incorrect | 3,953 | 98.15% |
| syntaxRelationFields | 3,460 | 99.09% |
| graded_sentences 1-5 | 13,650 | 99.54-100% |
| timeExpressions | 2,712 | 99.58% |
| verb object | 1,170 | 100% |
| time | 1,100 | 99.32% |
| tokenization | 388 | 84.62% |
| agreement | 338 | 100% |
| testParsing / thatParsing | 102 | 100% |

Four defects had to be fixed to get here (see the 2026-09-06 commits). Two stale
caches also had to be deleted, and both regenerate automatically: the 2022
`wordFormCache` -- its validity check compares the file's mtime against the DB
rows' timestamps, and the restored dump preserved 2022-era timestamps, so a stale
cache looks current -- and `tests/timeExpressions.txt.wordCacheFile`, which was
from 2010 and declared 196 forms where the database now has 209.

**The 9 that do not parse all fail the same way**, and none of them crashes: the
tokenizer cannot find the source's configured start marker, logs `Unable to find
start in tests/<name>.txt`, and skips the document (`cSource::scanUntil`,
`source.cpp`). The marker text is demonstrably present -- in `Adversary.txt` it
sits at character 96 of the decoded file and matches the logged marker exactly,
`\r\n` included -- so the file is being read correctly and the failure is in the
match. `scanUntil` requires the marker to be alone on a line (`aloneOnLine`) and
these markers span two lines, which is the likely cause. Whether the markers or
the matcher are wrong is an author decision, and the data is untidy either way:
`Adversary` and `Roman` have no `sources` row at all, and the row for `Usage`
says `tests\usage.txt` against a file named `Usage.txt`.

Affected: Adversary, Nameres, Roman, Usage, date-time-number, lappinl,
modification, `pattern matching`, resolution.

**No behavioural comparison against the Windows build has been made.** The
percentages look healthy, but nothing has checked them against what the same
documents scored on Windows, which is the only way to know the parse is
equivalent rather than merely successful. `tests/` is a corpus, not an oracle:
nothing asserts what a document *should* score, so a regression that lowered
match rates without crashing would pass unnoticed.

## Batch status

| Batch | Scope | Status |
|---|---|---|
| B0 | Environment + CMake skeleton, frozen file lists, `lp_smoketest` | done |
| B1 | `lpchar.h/.cpp`, `utfConvert.h/.cpp` + `lpchar_smoketest` (75 checks) | done |
| B2 | Global mechanical rename (types, `L""`→`u""`) | done |
| B3 | Root headers: `general.h`, `envConfig.*`, `logging.*`, `profile.h`, and every lock call site | done |
| B4a | `main.cpp` entry point + globals (`wmain`→`main`, process spawn, Ctrl-C, console) | done |
| B4b | `specials_main.cpp` equivalent | done |
| B5 | `DIYDiskArray.h`, `mysqldb.h`, `DB*.cpp`, `memoryStat.cpp`, low-level I/O | done |
| B6a | `word.h/.cpp`, `pattern.h/.cpp` (+ `SOURCE_VERSION` 8→9) | done |
| B6b | remaining pattern-engine files | done |
| B7 | `utilities.cpp` (`wTM`/`mTW` onto the B1 codec) + `tokenize.cpp` | done |
| B8 | Dictionary/WordNet cluster | done |
| B9 | `Internet.cpp`/`internet.h` → libcurl (highest external risk) | done |
| B10 | `get*.cpp` consumers, `FindFirstFile`→`opendir`, MAX_PATH sweep | done |
| B11 | `hmm.cpp`/`hmm.h` (JNI + Viterbi), Stanford jar classpath via `envConfig` | done |
| B12 | Business-logic cluster (~14 files; parallelizable) | done |
| B13 | QA cluster | done |
| B14 | First full `lpcore` compile + link (checkpoint 1) | done |
| B15 | Link `lp` and `CorpusAnalysis` (checkpoint 2) | done |
| B16 | Runtime smoke test | done |

### What B3 changed

- `general.h` is self-sufficient (its own std includes, `logging.h`, `utfConvert.h`)
  instead of depending on every includer having pulled `windows.h` and the std
  headers in first. `MYSQL` is forward-declared rather than dragging
  Connector/C's headers into ~50 translation units.
- `CP_UTF8`/`CP_ACP` moved to `utfConvert.h` with their original numeric values,
  so the codepage int threaded through the cache keeps meaning the same thing.
  `CP_ACP` has no macOS equivalent; its two real call sites are B9's to resolve.
- All six locks are `std::shared_mutex`; `createLocks()` is an empty no-op that
  B4a can delete along with its call site.
- `profile.h`: `QueryPerformanceCounter`→`std::chrono::steady_clock`,
  `QueryPerformanceFrequency`→a constant, Psapi's `GetProcessMemoryInfo`→
  `task_info(MACH_TASK_BASIC_INFO)`. Memory *deltas* stay comparable with the
  Windows runs; absolute totals do not (resident vs. private commit).
- `logging.cpp`: dropped `windows.h`/`io.h`/`winhttp.h` and three headers it never
  used; `_isatty`/`_fileno`→`isatty`/`fileno`; MSVC's `"a+t,ccs=UTF-8"`+`fputws`
  replaced with an explicit UTF-8 encode + `fputs` (same bytes on disk);
  `GetLastError`→`errno`; `sprintf`→`snprintf`; `multiprocessor logs\` → `/`.

Three bugs were found and fixed rather than ported faithfully:

1. **`envConfig.cpp` returned the wrong credential.** The `static` cache lived in
   the shared helper rather than per variable, so the first getter to run
   populated it and every later getter sharing that helper returned *its* value —
   `getDBHost()` handed back `LP_DB_USER`, and `getGoogleCSEContext()`,
   and `getBingSubscriptionKey()` both handed back the
   Google CSE key. Now cached per getter, and covered by a compiled-and-run check.
2. **`getWordNet.cpp`'s `getAllOrderedHyperNyms`** took the ordered-hypernym lock
   *shared* and then wrote the map under it (both the insert and the counter
   increment). Now exclusive, matching the structurally identical cache in
   `createOntology.cpp::getRDFTypesMaster`.
3. **`Internet.cpp`'s `readBinaryPage`** took `networkTimeSRWLock` *shared* to
   write `lastNetClock`, where `readPage`'s identical line correctly took it
   exclusively. Now exclusive.

### What B4a changed

`main.cpp` no longer includes `windows.h`, and has no Win32 surface left.

- **Entry point**: `wmain(int, wchar_t*[])` → a standard `main(int, char*[])` that
  widens argv once into never-freed statics and calls `lpMain()` with the same
  `lpchar_t*[]` shape. Every downstream signature is untouched, so the change is
  confined to the top of the file.
- **Process layer** (new `lpProcess.h`/`.cpp`): `CreateProcess` → `posix_spawn`
  with a real argument vector; `WaitForMultipleObjectsEx` →
  `lpWaitForAnyChildProcess` (polled `waitpid`, 250 ms, because a blocking
  `waitpid` takes no deadline and the only thing the deadline drives is a
  progress refresh every few minutes); `GenerateConsoleCtrlEvent` and its 40 lines
  of console-attach contortions → one `kill(pid, SIGINT)`.
- **Sibling discovery**: the three hardcoded relative paths
  (`QuestionAnsweringx64\lp.exe`, `ParseAllSourcesx64\lp.exe`,
  `x64\StanfordAllSources\CorpusAnalysis.exe`) were three Windows build outputs of
  only two programs, selected by arguments rather than by build. They are now
  `lp` and `CorpusAnalysis` found next to the running binary via
  `_NSGetExecutablePath`, so controller mode no longer requires being launched
  from the root of a full build tree.
- **Signals**: `SetUnhandledExceptionFilter` + `SetConsoleCtrlHandler` → one
  `sigaction` set. SIGINT/SIGTERM/SIGHUP drive the existing two-stage interrupt;
  SIGSEGV/SIGBUS/SIGFPE/SIGILL log a `backtrace()` and re-raise.
- **Deleted rather than ported**: `createMinidump` (macOS already writes a full
  crash report per abnormal exit, and the fixed `core.dmp` name meant concurrent
  children overwrote each other's anyway); `setConsoleWindowSize` and the
  `GetConsoleWindow`/`SetWindowPos` pair (a process cannot size or position the
  terminal it runs in on macOS); and the include of `stacktrace.h`, a vendored
  Win32-only dbghelp walker that `main.cpp` was the only consumer of — the file is
  left on disk for the unmaintained Windows project but nothing includes it now.
- **`exitNow`/`exitEventually`** became `volatile sig_atomic_t` (declaration in
  `word.h` updated to match), since a signal handler now writes them.

Four more bugs fixed rather than ported:

4. **Spawn failures were swallowed.** All three call sites read
   `if (errorCode = createLPProcess(...) < 0) break;` — `<` binds tighter than
   `=`, so `errorCode` got the comparison result, not the return code, and the
   `break` only left the `switch`. A failed spawn reached the caller as a zero
   handle and surfaced much later as a misleading wait error.
5. **Divide-by-zero in both throughput calculations.** `processingSeconds` is
   integer seconds used as a divisor, so a child exiting inside the first second
   divided by zero — a SIGFPE crash on macOS, where Windows raised a catchable
   exception. Both sites now clamp with `max(1, ...)`.
6. **`WAIT_FAILED` ran a `memmove` with an index of `0xFFFFFFFF`**, survivable
   only because the `LOG_FATAL_ERROR` above it terminated first. The replacement
   returns a distinct sentinel that is checked before anything is indexed.
7. **The `-mp` pool was silently capped at 64.** `maxProcesses` was never clamped
   to `MAXIMUM_WAIT_OBJECTS`, so a larger `-mp` made `WaitForMultipleObjectsEx`
   fail immediately and killed the run with an unrelated message. `waitpid` has
   no such limit, so the hazard is gone rather than papered over. The unchecked
   `calloc` next to it is now checked too.

Two behaviour changes worth knowing at runtime: children no longer each get their
own console window, so their stdout interleaves in the one terminal (their
per-child `.lplog` files are unaffected, and that is where the real output has
always gone); and the progress line that used to be the console title is now an
OSC 0 terminal-title escape, written only when stdout is a tty.

### What batches B4b-B16 changed

These are summarized together because they were carried out as one continuous
compile-fix-recompile pass, with the compiler as the oracle.

- **B4b (`specials_main.cpp`)**: same treatment as `main.cpp`, but reusing
  `lpProcess` rather than repeating it -- this file's own `CreateProcess` wrapper,
  `signalCtrl` and `setConsoleWindowSize` are deleted in favour of the shared ones.
  Its three `FindFirstFile` walks became `lpDirectoryEntries`.
- **B5 (low-level I/O)**: a new `lpFile.h`/`.cpp` holds the portable wide-path
  layer -- `lp_wfopen`/`lp_wremove`/`lp_waccess`/`lp_wmkdir`/`lp_wstat`/`lp_wopen`
  (76 call sites), `lp_filelength`, `lpDirectoryEntries` (replacing the whole
  `FindFirstFile`/`FindNextFile`/`FindClose` family), and the compatibility
  constants (`MAX_PATH`, `O_BINARY`, the `ERROR_*` codes mapped to `errno` values).
  `memoryStat.cpp`'s dead WMI/COM apparatus was deleted and replaced with a working
  Mach implementation (`task_info` / `host_statistics64` / `sysctl`).
- **B6 (word/pattern)**: mostly mixed-type `min`/`max` fallout from losing the
  untyped `windows.h` macros, each fixed with an explicit cast to the type the
  result is actually assigned to.
- **B7 (`utilities.cpp`, `tokenize.cpp`)**: `wTM`/`mTW`/`mTWCodePage` are now thin
  wrappers over `utfConvert`, preserving the encoding-detection ladder exactly
  (cached corpora were parsed under it). The Win32 two-call sizing protocol and its
  grow-only scratch buffers are gone.
- **B8/B10 (acquisition)**: remaining Win32 file APIs ported; the CP1252
  representability probe (`WideCharToMultiByte` with `WC_NO_BEST_FIT_CHARS`) became
  a direct check against the CP1252 repertoire.
- **B9 (`Internet.cpp`)**: WinINet replaced by libcurl behind an unchanged public
  API. `CURLOPT_TIMEOUT` made the entire `InternetReadFile_Wait` worker thread --
  the only `CreateThread` in the codebase -- unnecessary, so it is deleted rather
  than ported. `CURLOPT_FOLLOWLOCATION` + `CURLINFO_EFFECTIVE_URL` replace the
  redirect status callback. The Jericho Java helper uses `posix_spawn` with a pipe
  and file actions instead of `CreateProcess` plus five `DuplicateHandle`s.
- **B11 (`hmm.cpp`)**: the hardcoded `F:\lp\Stanford\...jar` classpath moved behind
  `envConfig`'s `getStanfordClasspath()` (`LP_STANFORD_CLASSPATH`), POSIX separators.
- **B14/B15**: `liblpcore.a` archives and both executables link.
- **B16**: both binaries start, initialize, parse arguments and write their
  `.lplog` files. `lp` with no arguments exits cleanly on its own "Source type not
  found" diagnostic; `CorpusAnalysis` reaches the database and reports a clean
  "Can't connect to local MySQL server" -- the expected result with no server
  running, and the natural hand-off point out of this plan.

New primitives added along the way, all covered by `lpchar_smoketest`:
`lp_itow`/`lp_i64tow` (array-size-deducing, which also fixed several `lpchar_t[10]`
buffers too small for `INT_MIN`), `lp_strncasecmp`, `lp_swscanf`/`lp_fwscanf`,
`lp_fgetws`/`lp_fputws`, and `lp_wsprintf`/`lp_wsprintf_at` -- the last being how
all 84 unbounded `wsprintf` call sites became bounded with no size argument for
anyone to get wrong.

### A build trap worth remembering

The first link attempt failed with a wall of "Undefined symbols for architecture
x86_64" although every dependency is arm64: the whole build had silently configured
itself x86_64. `CMakeLists.txt` now pins `CMAKE_OSX_ARCHITECTURES` to arm64 on
Apple. If it ever recurs, that misleading message is the symptom.

### Verification standard

There is no test suite. Each batch's claim is backed by an actual compile of the
files it owns, plus the two smoke-test binaries. Final state: from a clean tree, `cmake .. && cmake --build .` produces
`liblpcore.a`, `lp` and `CorpusAnalysis` with exit code 0 and no compile errors.
All four smoke tests pass: `lpchar_smoketest` (95 checks over the printf engine,
UTF codec and string primitives), `lpfile_smoketest` (10 checks that a
'\'-separated path literal joined to a POSIX root reaches the real file in
`caches/`; run it from the main directory), `lpprocess_smoketest` (40 checks that spawn,
signal and reap real processes, including a regression check that an argument
containing spaces survives as exactly one argument), and `lp_smoketest`
(toolchain, yajl, libcurl, JNI).

**What this standard does NOT cover**, and a maintainer should not assume it does:
nothing here exercises the parser on a real document. Everything verified is
"compiles, links, starts, fails cleanly". Behavioural equivalence with the Windows
build is argued from reading the code, not measured -- and several changes (the
encoding-detection ladder, the binary cache codec, the pattern engine's newly
bounded formatters) are exactly the kind a corpus run would catch and a startup
smoke test would not. Running the test corpus against a live MySQL is the first
thing to do next.

### Known follow-ups, and things a maintainer must decide

Genuinely open, in rough order of how likely they are to bite:

1. **Nothing has parsed a document.** See the verification standard above. Run the
   test corpus against a live MySQL before trusting any of this.
2. **The MySQL Connector/C version gap is unexercised.** The build links against
   26.7.0; the code was written against something far older. One symptom is already
   visible at runtime -- `MYSQL_OPT_RECONNECT is deprecated` on every connect --
   and that is only the part that warns. `mysql_options` codes and struct layouts
   are the risk.
3. **`getDictionary.cpp`'s `removeDots()` searches for U+FFFD**, the replacement
   character, because that is literally what the source bytes contain -- and did
   before this port (it is U+FFFD in git history too, and was a meaningless
   multi-character literal under MSVC as well). The function has therefore never
   removed what its name says. The likely intent is U+00B7 MIDDLE DOT, the
   syllable separator used in dictionary entries. **This needs the author's decision**; it was
   not guessed at.
4. **Windows path separators (`\`) remain throughout** outside the files that had
   to change. They are a runtime problem, not a compile-time one, so they will
   surface as "file not found" during the first real corpus run rather than now.
5. **`specials_main.cpp` could never have linked on Windows either**: it called
   `getNumSourcesProcessed`, which is defined only in `main.cpp`, a file
   `specials.vcxproj` does not compile. A definition was added here (and it null-
   checks the `SUM()` columns, which `main.cpp`'s copy does not -- that one calls
   `atol` on a null pointer on an empty corpus). Worth confirming that
   `CorpusAnalysis` was in fact unbuildable rather than built some other way.
6. **`totalInternetTimeWaitBandwidthControlSRWLock` still exists twice**, as a
   global in both entry-point files and as a `cInternet` static. Only the class
   static is referenced; the globals are dead and can be deleted.
7. **`#pragma warning(...)` lines remain in most files**; clang ignores them with a
   `-Wunknown-pragmas` warning. A mechanical sweep would quiet the build.
8. **`stacktrace.h` is now unused** -- a vendored Win32-only dbghelp walker with no
   remaining includer. Left on disk for the unmaintained Windows project.
9. **`-Wparentheses` dominates the build output**, from the codebase's deliberate
   `if (error = !copy(...))` idiom. Not a defect (`CODE_REVIEW.md` documents it as
   intentional), but it buries real warnings; `-Wno-parentheses` would be
   reasonable now that the port is done.
10. **`TreatWarningAsError` is still off**, per the original review's note. With the
    tree now compiling under `-Wall -Wextra`, triaging and enabling it is finally
    a realistic piece of work.
