# LPProcessor code review

Findings collected while annotating the first-party sources. Line numbers refer
to the files as they stand on this branch (after comment insertion). Severity
is `CRITICAL` / `HIGH` / `MEDIUM` / `LOW` / `NIT`.

This document is being filled as files are annotated. Vendored third-party
trees (`lloyd-yajl-66cb08c/`, `tinyxml2-master/`, `HMM/KenLM/`, `packages/`)
are out of scope.

---

## Wave 1 — core infra (`main.cpp`, `source.h`/`source.cpp`, `word.cpp`,
`tokenize.cpp`, `utilities.cpp`, `DBUtility.cpp`, `mysqldb.h`)

- **[HIGH] general.h:10-14 / main.cpp initialize() — hardcoded `F:\lp` / `M:\caches`** —
  `MAINDIR`, `LMAINDIR`, `CACHEDIR`, `WEBSEARCH_CACHEDIR` and `TEXTDIR` are
  compile-time absolute paths on a specific Windows machine. `initialize()`
  still fatally requires `CACHEDIR` to exist even when `-cacheDir` names a
  valid directory. Move these to configuration (or at least honour `-cacheDir`
  before the existence check).

- **[HIGH] logging.cpp / callers of `lplog(LOG_FATAL_ERROR, ...)` — fatal path
  hangs then `exit(0)`** — `logstring()` for `LOG_FATAL_ERROR` blocks on
  `getchar()` and then `exit(0)`. An unattended multiprocessor child therefore
  hangs on the first fatal, and if it is killed later the parent sees success
  (exit code 0). Abort with a non-zero status and without waiting for a
  console key. (Confirm the exact `getchar()`/`exit` pairing when
  `logging.cpp` is annotated; `source.h` currently documents the opposite
  behaviour, which is itself a documentation inconsistency.)

- **[HIGH] utilities.cpp copy() serialize overloads — write-then-check `limit`** —
  The scalar serialize overloads write into `buf` and only afterwards test
  `where` against `limit`. A truncated or corrupt cache file is therefore
  detected only after the overrun has already happened. Check `where + sizeof`
  first; treat `limit` as a real bound, not a diagnostic.

- **[HIGH] mysqldb.h:32-33 — `QUERY_BUFFER_LEN_OVERFLOW` / `_UNDERFLOW` not
  parenthesized** — they expand to `QUERY_BUFFER_LEN+1024` /
  `QUERY_BUFFER_LEN-1024`. Any use as `2*QUERY_BUFFER_LEN_OVERFLOW` silently
  mis-sizes the buffer. Parenthesize the macros.

- **[HIGH] DBUtility.cpp `myquery()` — `mySQLQueryBufferSRWLock` released
  before `mysql_real_query()`** — the shared `sqlQueryBuffer` pointer is still
  in use after the lock is dropped. Today the program is mostly
  single-threaded around SQL, but the lock as written does not actually
  protect the buffer for the duration of the query. Hold the lock until the
  query (and any copy of the statement text) is finished, or copy into a
  per-call buffer.

- **[MEDIUM] source.h `cOM::operator==` vs `operator!=` — not inverses** —
  `==` compares both `object` and `salienceFactor`; `!=` compares only
  `object`. `!(a==b)` is therefore not equivalent to `(a!=b)` when only
  salience differs. Make them inverses, or rename `!=` to something like
  `differentObject`.

- **[MEDIUM] source.h `cWordMatch::flags` — colliding flag vocabularies** —
  bits `>= 32` are global, the low 32 bits are an anonymous enum whose values
  mean different things on quote vs non-quote positions. Several enum values
  intentionally collide (e.g. `flagFirstLetterCapitalized` and
  `flagFromPreviousHailResolveSpeakers` are both `1<<23`). A flag tested on
  the wrong kind of position is a silent logic error. Split into two
  bitfields or two types.

- **[MEDIUM] utilities.cpp `itos()`/`dtos()` — unbounded format into a
  1024-wchar stack buffer** — `wsprintf` / `wcscpy` / `wcscat` with no length
  check. Callers that pass a long prefix/suffix or a wide format overrun the
  stack. Use `swprintf` with an explicit bound, or format into the
  destination `wstring` directly.

- **[MEDIUM] main.cpp `wmain()` / `startProcesses()` — `_exit(0)` skips
  destructors** — documented as deliberate because tearing down the lexicon
  takes minutes, but it also skips the MySQL close and any unflushed log
  buffer. Flush logs (and close MySQL) before `_exit`.

- **[MEDIUM] main.cpp / `specials_main.cpp` — duplicated process orchestration** —
  `specials_main.cpp` reimplements `wmain`, `getNumSourcesProcessed`, lock
  definitions and a large part of the controller loop. Drift between the two
  is already visible in the command-line table. Extract a shared
  `processMain.cpp`.

- **[LOW] README.md "Command Line" vs `processCommandArguments()` — stale
  documentation** — flags documented in the README (`-tg`, `-acquireNewsBank`,
  `-acquireMovieList`, `-acquireInterviewTranscript`, `-acquireTwitter`,
  `-flipTMSOverride`, `-flipTUMSOverride`, `-sourceRead`, `-sourceWrite`,
  `-C`) are not implemented in `main.cpp` (commented out or living under
  `unused source\`). Update the README from the actual argv parser.

- **[LOW] source.h binary cache — implicit format, no version tag** —
  `write()` and the `(char*,int&,int,bool&)` constructors must stay
  field-for-field in the same order or every previously written cache
  silently misparses. Add a version word at the start of the stream and
  reject unknown versions.

- **[NIT] source.h deserializing constructors — `if (error=!copy(...))`** —
  assignment-in-condition is intentional (not a `==` typo) but easy to
  misread. A named `ok` / early-return would be clearer.

---

## Later waves

*(to be appended as the remaining files are annotated)*
