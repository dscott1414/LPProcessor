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

## Wave — question answering (`questionAnswering.cpp`, `QuestionAnswering.h`,
`questionProcessing.cpp`, `questionAnsweringWebSearch.cpp`)

- **[CRITICAL] questionAnsweringWebSearch.cpp:173 — hardcoded Google Custom Search API key** —
  `webSearchKey = L"AIzaSyDOCHy1bm-46kJkgV2hqPjFJ6Ce8FfR_AE"` is a live CSE key
  (also pasted in the comment at the REST example). Load from an env var /
  secret store and rotate the committed key.

- **[CRITICAL] questionAnsweringWebSearch.cpp:174 — hardcoded Bing v7 subscription key** —
  `BINGAccountKey = L"345820954c834fa08a227260862bbfe5"` is sent as
  `Ocp-Apim-Subscription-Key` on every search. Externalize and rotate.

- **[HIGH] questionAnswering.cpp:3028 — `cProximityEntry` uses `childObject` before assigning `co`** —
  the 5-arg ctor inherits `childObject = 0` (narrator) from the default ctor,
  then types/describes every proximity neighbour as object 0. Assign
  `childObject = co` first.

- **[HIGH] questionAnswering.cpp:2866 — remapped question type is always 15** —
  `parentSRG->questionType = qt | typeQTMask` ORs the new type with
  `typeQTMask=15`, so later type checks miss. Should be
  `(questionType & ~typeQTMask) | qt`.

- **[HIGH] questionAnswering.cpp:1158 — meta-pattern answers are never recorded** —
  `processMetanameTagset` can return a valid `whereAnswer`, but
  `metaPatternMatch` ignores it and always `return -1`. Return the first
  positive `whereAnswer`.

- **[HIGH] questionAnsweringWebSearch.cpp:891 — yajl tree leaked on every Google parse** —
  `yajl_tree_parse` is never paired with `yajl_tree_free` (same bug at 973
  for Bing). Free `node` before each return.

- **[HIGH] questionAnsweringWebSearch.cpp:886 — `jsonBuffer[0]` read before empty check** —
  empty cache / failed fetch is UB. Check `empty()` first. Same pattern at
  968 (`extractBINGWebSites`).

- **[HIGH] questionProcessing.cpp:808 — `speakerGroups[sgAt]` can be `end()`** —
  the scan can leave `sgAt == speakerGroups.size()`. Guard before iterating
  `speakerGroups[sgAt].speakers`.

- **[HIGH] questionAnswering.cpp:4658 — proximity “BING” pass still searches Google** —
  `searchWebSearchQueries(..., true, lastGoogleResultPage)` repeats Google.
  Pass `false` and a Bing last-page flag.

- **[HIGH] questionAnswering.cpp:1874 — `wikiTableMap` entries are never deleted** —
  `new cWikipediaTableCandidateAnswers` is stored in a local map that goes
  out of scope without deleting. Own the pointers or store by value.

- **[HIGH] questionAnswering.cpp:115 — `stripWeb` on an empty URI is UB** —
  `name[0]` / `name.back()` with no empty check. Return early if
  `name.empty()`.

- **[MEDIUM] questionAnswering.cpp:3193 — `whereChildCandidateAnswer` written from the question source** —
  a question-source index is later used as `childCAS.source->m[...]`. Use
  the child SRG’s secondary prep object instead.

- **[MEDIUM] questionAnswering.cpp:4130 — table `columnIndex` is `iterator - columns.end()`** —
  always ≤ 0. Should be `columnIterator - tableIterator->columns.begin()`.

- **[MEDIUM] questionAnswering.cpp:3356 — transformed / rewritten SRGs are leaked** —
  `processTransformQuestionPattern` / `isQuestionPassive` `new` SRGs into
  `ssrg` / `lssri` with no owner.

- **[MEDIUM] questionAnsweringWebSearch.cpp:646 — `hashWebSiteURL` dereferences `end()` on an empty URL** —
  Check `empty()` before stripping `http://`.

- **[MEDIUM] questionAnsweringWebSearch.cpp:591 — `appendVerb` reads `m[where+1]` with no bounds check** —
  Require `where + 1 < (int)m.size()`.

- **[MEDIUM] questionProcessing.cpp:675 — `(imEOS + 1)` when the terminator is the last token** —
  can dereference `m.end()`. Same at 713. Test `imEOS + 1 != m.end()` first.

- **[MEDIUM] questionAnswering.cpp:715 — book-title strip can index `bookTitle[-1]`** —
  Check `!bookTitle.empty()` before trailing-quote / comma tests.

- **[MEDIUM] questionAnswering.cpp:302 — `matchAllSourcePositions` never writes `synonym`** —
  callers therefore never apply the synonym discount.

- **[MEDIUM] questionAnswering.cpp:140 — cache roots are compile-time `M:\caches`** —
  no runtime override in this TU; `questionTransforms.txt` is also a
  Windows-only relative path.

- **[MEDIUM] questionAnsweringWebSearch.cpp:693 — `scrapeWebSite` is unfinished and unused** —
  category-3 tags are a stub; full pages are parsed as raw HTML.

- **[LOW] questionAnswering.cpp:4346 — `matchAnswersOfPreviousQuestion` dereferences an empty set** —
  `*wherePossibleAnswers.begin()` with no `empty()` check; also always
  returns -1.

- **[LOW] questionAnswering.cpp:1003 — `appendSum` writes through `c_str()`** —
  `c_str()` is const; empty `str` is also UB.

- **[LOW] questionAnsweringWebSearch.cpp:210 — Bing `numWebSitesAskedFor` is ignored** —
  the parameter only gates a constant `&answerCount=10`.

- **[NIT] questionAnsweringWebSearch.cpp:894 — Google parse errors are logged as FreeBase** —
  copy-paste leftover from the old Freebase client.

---

## Later waves

*(to be appended as the remaining files are annotated)*
