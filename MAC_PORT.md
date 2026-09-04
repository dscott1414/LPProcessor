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
```

**The port is complete.** All 48 `lpcore` files compile, `liblpcore.a` archives, and
both `lp` and `CorpusAnalysis` link and run as arm64 binaries. The three smoke-test
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

**There are two MySQL installations on this machine — do not mix them.**

- `/opt/homebrew/opt/mysql-client` — Connector/C 26.7.0, **arm64**, client
  libraries only. This is what CMake finds and what the binaries link against;
  `MYSQL_LIBRARY` in `build/CMakeCache.txt` should always point here.
- `/usr/local/mysql` → `/usr/local/mysql-8.0.23-macos10.15-x86_64` — a full
  MySQL 8.0.23 **server**, x86_64, installed 2021. Its `libmysqlclient.dylib` is
  x86_64 and would fail to link against an arm64 target with a confusing
  architecture error. It is not on any CMake search path, and must not be added
  to one.

The split is fine and in fact convenient: link the arm64 client, and let B16
connect over TCP to the x86_64 server (which runs under Rosetta) — a client and
server of different architectures talk to each other normally. The server was not
running as of 2026-09-04, so B16 will need it started.

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
   `getBingSubscriptionKey()` and `getMerriamWebsterKey()` all handed back the
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
All three smoke tests pass: `lpchar_smoketest` (95 checks over the printf engine,
UTF codec and string primitives), `lpprocess_smoketest` (40 checks that spawn,
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
   Merriam-Webster syllable separator. **This needs the author's decision**; it was
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
