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

## Wave — ontology, HMM, specials (`createOntology.cpp`, `ontology.h`,
`hmm.cpp`, `hmm.h`, `specials_main.cpp`)

- **[HIGH] createOntology.cpp:2235 — 20MB stack buffer in `writeRDFTypes`** —
  `char buffer[MAX_BUF * 10]` (~20MB) on a default 1MB MSVC stack. Heap-allocate
  or reuse the chunked write path.

- **[HIGH] createOntology.cpp:1045 — 5MB stack buffer in `readYAGOOntology`** —
  `char fileBuffer[MAXYAGOBUF + 1]`. Heap-allocate or memory-map the TTL file.

- **[HIGH] createOntology.cpp:328 — 4MB stack buffer in `readN3FileIntoTripletMap`** —
  `wchar_t buffer[MAX_BUF]` is 2e6 × 2 bytes on the stack.

- **[HIGH] createOntology.cpp:2385 — `rdfTypeNumMap` mutated under a shared SRWLOCK** —
  `AcquireSRWLockShared` then `rdfTypeNumMap[object] = 1`. Use an exclusive
  lock for the insert.

- **[HIGH] createOntology.cpp:2303 — unescaped SQL in noRDFTypes / noERDFTypes / Freebase** —
  `wsprintf(..., L"INSERT INTO noRDFTypes VALUES ('%s')", object)` (and
  matching SELECTs). Escape or parameterize.

- **[HIGH] createOntology.cpp:2828 — WRITE lock leaked on Open Library insert failure** —
  `return` after `LOCK TABLES ... WRITE` leaves the lock held. Unlock on
  every path.

- **[HIGH] createOntology.cpp:514 — `decodeURL` reads past the end of input** —
  `'%'` then `input[I+1]` / `input[I+2]` with no length check. Guard
  `I+2 < input.size()`.

- **[HIGH] createOntology.cpp:2050 — `find()` argument order is inverted** —
  `properties.find(whereName + 1, '{')` is `find(const char*, size_t)`.
  Intended: `properties.find('{', whereName + 1)`.

- **[HIGH] hmm.cpp:218 — Stanford parse cache INSERT is unescaped** —
  `VALUES('%s','%s',%I64d)` interpolates `parse` and `sentence` after only
  mapping some quote characters. Escape or bind parameters.

- **[HIGH] hmm.cpp:179 — cached PCFG lookup is hash-only** —
  `where sentencehash = %I64d` never compares the sentence text. Collisions
  return the wrong parse. Add `AND sentence = ...` or a stronger digest.

- **[HIGH] hmm.cpp:479 — `writeModelFile` / `readModelFile` do not check `_wfopen`** —
  a missing path is a null-pointer dereference.

- **[HIGH] hmm.cpp:516 — `readModelFile` writes before the line buffer on empty input** —
  `line[wcslen(line) - 1] = 0` underflows on an empty `fgetws`. Only strip
  if `wcslen(line) > 0`.

- **[HIGH] hmm.cpp:463 — `trainModelFromSource` never frees the wordforms result** —
  `MYSQL_RES*` is dropped; `wordsToAdd` is also interpolated unescaped.

- **[HIGH] hmm.cpp:311 — `findLPPOSEquivalents` indexes `[length-2]` unguarded** —
  `'s` stripping without `originalWord.length() >= 2`. Same pattern in
  `specials_main.cpp` `stTokenizeWord`.

- **[HIGH] hmm.cpp:785 — NaN test is never true** —
  `if (probMult == nan(NULL))` is always false. Use `std::isnan(probMult)`.

- **[HIGH] specials_main.cpp:399 — `startProcesses` case 2 format-string mismatch** —
  `wsprintf(..., L"... -log %d", CACHEDIR, step, ...)` passes a `wchar_t*`
  as the first `%d`. Use `%s` for `CACHEDIR`.

- **[HIGH] specials_main.cpp:6458 — `wmain` case 71 falls through into case 100** —
  the Webster plural-word probe has no `break` and then runs
  `stanfordCheckMP`. Add `break;`.

- **[HIGH] specials_main.cpp:1164 — Dictionary.com sweep inverts the miss test** —
  `!wcsstr(A) || !wcsstr(B)` is true unless *both* miss-markers are present,
  so almost every cache file is inserted into `notwords`. Intended:
  `wcsstr(A) || wcsstr(B)`.

- **[HIGH] specials_main.cpp:533 — hyphen-split branch is dead** —
  `if ((ret = Words.splitWord(...)) && word...find(L'-')!=npos, false)` :
  the comma operator makes the condition always `false`. Remove `, false`.

- **[HIGH] specials_main.cpp:1378 — several helpers take `cSource` by value and never UNLOCK** —
  `printUnknownsFromSource`, `patternOrWordAnalysisFromSource`,
  `syntaxCheckFromSource`, `testRDFType` lock and return without
  `UNLOCK TABLES`. Pass `cSource&` and unlock on every path.

- **[HIGH] specials_main.cpp:478 — `MWRequestAllowed` writes through a possibly-null `FILE*`** —
  `_wfopen(L"MWCheck", L"w")` is not checked before `fwprintf`/`fclose`.

- **[MEDIUM] createOntology.cpp:1530 — `readOntologyList` imports only one row** —
  `if ((sqlrow = mysql_fetch_row(result)))` is not a `while`.

- **[MEDIUM] createOntology.cpp:1044 — `GetFileSizeEx` failure leaks the YAGO HANDLE** —
  `return -1` without `CloseHandle(fd)`.

- **[MEDIUM] createOntology.cpp:2357 — `printRDFTypes` format/argument mismatch** —
  `L"END %s:%d %d"` is passed only two arguments.

- **[MEDIUM] createOntology.cpp:431 — `wcscmp` on a possibly-null extension** —
  `wcsrchr` then `wcscmp` without a null check.

- **[MEDIUM] ontology.h:86 — `cOntologyEntry::operator==` skips `resourceType` /
  `superClassResourceTypes`** — include both fields, or document that they
  are intentionally non-identity.

- **[MEDIUM] ontology.h:179 — default `cTreeCat` leaves `cli` uninitialized** —
  initialize `cli` to a sentinel in every constructor.

- **[MEDIUM] hmm.cpp:1209 — `tagFromSource` assumes a non-empty source** —
  `source.m[0]` with no empty check; vocab `operator[]` also inserts 0 for OOV.

- **[MEDIUM] hmm.h:21 — header signatures do not match `hmm.cpp`** —
  several declared arities will not link to the definitions.

- **[MEDIUM] specials_main.cpp:786 — inflection flags are added, not ORed** —
  `inflectionFlags=inflectionFlags+%d` corrupts the bitfield. Use `|`.

- **[MEDIUM] specials_main.cpp:203 — `getNumSourcesProcessed` dereferences `SUM()` NULL** —
  empty corpus makes `sqlrow[1]`/`sqlrow[2]` NULL. Guard NULL.

- **[MEDIUM] specials_main.cpp:376 — `createLPProcess` result assigned as a bool** —
  `if (errorCode = createLPProcess(...) < 0)` stores 0/1 and leaks
  `pi.hThread`. Parenthesize and close the thread handle.

- **[LOW] specials_main.cpp:1 — stale fork of `main.cpp`** —
  no `initialize()`, unchecked `chdir("source")`, duplicated lock/sentinel
  definitions. Port the `main.cpp` helpers or stop compiling this copy.

- **[LOW] hmm.cpp:82 — `createJavaVM` classpath and heap comments are stale** —
  option string is `F:\\lp\\Stanford\\...`; comments say 1MB/1GB while flags
  are `-Xms10m`/`-Xmx3g`.

- **[NIT] createOntology.cpp:478 — `readUMBELSuperClasses` always returns 0** —
  a `bool` that always returns false.

---

## Wave — DB layer and shared headers (`DB.cpp`, `DBCreateSQLSchema.cpp`,
`DBWordRelations.cpp`, `tableColumn.cpp`/`tableColumn.h`, `dbQuerySearch.cpp`,
`general.h`, `logging.cpp`/`logging.h`, `profile.h`, `stacktrace.h`,
`intArray.h`, `DIYDiskArray.h`, `bitObject.h`, `memoryStat.cpp`, `word.h`,
`paice.cpp`/`paice.h`, `conversationContext.cpp`, `Loebner.cpp`, `bncc.h`,
`Internet.cpp`/`internet.h`)

- **[CRITICAL] DB.cpp:366 — hardcoded MySQL root password** —
  `mysql_real_connect(..., "root", "byron0", DBNAME, ...)`. Move credentials
  to config/env and rotate `byron0`.

- **[CRITICAL] DBCreateSQLSchema.cpp:561 — same hardcoded credentials on schema create** —
  `mysql_real_connect(..., "root", "byron0", NULL, ...)` on the “database
  missing” path.

- **[CRITICAL] DBCreateSQLSchema.cpp:419 — `generateBNCSources` buffer overrun** —
  `tmalloc(actualLen + 1)` allocates bytes, then `buffer[actualLen] = 0`
  writes a `wchar_t` at byte offset `2*actualLen`. Allocate
  `actualLen + sizeof(wchar_t)` and NUL-terminate at
  `actualLen / sizeof(wchar_t)` if the file is UTF-16, or treat it as bytes.

- **[HIGH] logging.cpp:247 — `lplog(LOG_FATAL_ERROR)` getchar-then-`exit(0)`** —
  Confirms the Wave 1 finding. `source.h:83` is wrong (FATAL does abort);
  `main.cpp` / `DBUtility.cpp` are right. Exit 1 without waiting on stdin
  when not a TTY, and correct `source.h`.

- **[HIGH] DB.cpp:384 — `updateSourceStatistics` never `UNLOCK TABLES`** —
  same in `updateSourceStatistics2`/`3`. Every stats write leaves `sources`
  write-locked on this connection.

- **[HIGH] DB.cpp:326 — `getNumSources` lock leak on query failure** —
  `LOCK TABLES sources READ` then `return -1` without UNLOCK.

- **[HIGH] DB.cpp:1125 — `isBookTitle` interpolates title unescaped** —
  `where title = \"%s\"`. Escape or parameterize.

- **[HIGH] DB.cpp:429 — `readMultiSourceObjects` maps columns onto the wrong `cObject` fields** —
  `firstSpeakerGroup` is treated as the male BIT; the actual `plural` BIT
  is ignored. Pass `getFirstSpeakerGroup` separately and use
  `sqlrow[5..8][0]==1` for the four BIT columns.

- **[HIGH] DB.cpp:446 — `objectId` used as `objects[]` index** —
  a non-dense or large `objectId` is an out-of-range write. Map
  `dbIndex` → vector offset.

- **[HIGH] DB.cpp:846 — `isWordFormCacheValid` leaks the first `MYSQL_RES`** —
  the first `SELECT UNIX_TIMESTAMP` result is overwritten without
  `mysql_free_result`.

- **[HIGH] paice.cpp:176 — overlapping `memcpy` of the wrong size for UTF-16 BOM** —
  copies bytes not `wchar_t`s, drops the NUL, and overlaps. Use
  `memmove(s, s+1, (wcslen(s+1)+1)*sizeof(wchar_t))`. Same at line 245.

- **[HIGH] paice.cpp:307 — `isWordDBUnknown` interpolates word unescaped** —
  `word=\"%s\"`. Escape, or look up only in `Words`.

- **[HIGH] DIYDiskArray.h:189 — destructor never closes the fd** —
  `_wsopen_s` fds leak. Also `put`/`get` stride `saveSecond` while
  `initialize` sizes `(saveFirst+1)*(saveSecond+1)`, so the last column
  aliases the next row.

- **[HIGH] Internet.cpp:473 — timeout closes the request handle under a live reader thread** —
  `InternetCloseHandle` while `InternetReadFile_Child` may still be in
  `InternetReadFile`. Wait for the thread after cancel.

- **[HIGH] profile.h:346 — `accumulateNetworkTime` writes through a `const wchar_t*`** —
  mutates the URL to isolate the host. Copy into a local `wstring` first.

- **[HIGH] intArray.h:350 — `decode()` right-shifts by a negative count** —
  last iteration is `val >> (0-10)`. Stop at `bitFieldCount >= BITS_PER_RULE`.

- **[HIGH] bitObject.h:177 — `write()` memcpy before the limit check** —
  a short cache buffer is already overrun when FATAL fires. Check
  `where+sizeof(bits) > limit` first.

- **[HIGH] conversationContext.cpp:85 — identical intersect tests (copy-paste)** —
  both sides of the `||` compare against `m[previousQuote].objectMatches`
  only. The second clause should test `audienceObjectMatches`.

- **[MEDIUM] word.h:153 — `ADJECTIVE_INFLECTIONS_MASK` includes `ADVERB_SUPERLATIVE`** —
  copy-paste; should be `ADJECTIVE_SUPERLATIVE`.

- **[MEDIUM] logging.cpp:55 — log `FILE*` handles are process-wide while the filename is TLS** —
  concurrent workers can `fclose` each other's stream. Make the `FILE*` TLS
  or take a lock.

- **[MEDIUM] memoryStat.cpp:97 — WMI enumerator is never added** —
  `AddEnum` is commented out, so `getCounter` always fails.

- **[MEDIUM] tableColumn.cpp:374 — `determineColumnRDFTypeCoherency` always returns true** —
  the reject path is commented TEMP DEBUG, so incoherent Wikipedia columns
  are kept as QA answers.

- **[MEDIUM] tableColumn.cpp:434 — `sprint` prints the matched-object count twice** —
  uses `matchedQuestionObjectStr` instead of
  `synonymMatchedQuestionObjectStr`.

- **[MEDIUM] Internet.cpp:291 — SPARQL failure recurses with no depth cap** —
  retry in the existing `while (errors < internetWebSearchRetryAttempts)`
  loop.

- **[MEDIUM] Internet.cpp:514 — `getWebPath` truncates at `MAX_PATH-20` in a `MAX_LEN` buffer** —
  distinct long URLs collide on the same cache file. Truncate at
  `MAX_LEN-20`.

- **[MEDIUM] DBCreateSQLSchema.cpp:221 — `createTimeRelationTables` SQL is invalid** —
  `INDEX`/`FOREIGN KEY` on `relationId` but the column is `wordRelationId`;
  trailing comma. Same class of problems in `createRelationTables`.

- **[MEDIUM] paice.cpp:287 — assignment used as the `applyStemRule` condition** —
  `if (state = applyStemRule(...) == s_continue)` stores 0/1 in `state`.
  Write `if (applyStemRule(...) == s_continue)`.

- **[LOW] DB.cpp:1139 — `readWikiNominalizations` never `mysql_free_result`** —
  leaked on both success and failure-after-store paths.

- **[LOW] Loebner.cpp:1 — translation unit is comments only** —
  move the notes to a `.md` or implement the sketched model.

- **[NIT] source.h:83 vs logging.cpp — document FATAL in one place** —
  after fixing abort behaviour, delete the contradictory sentence in
  `source.h`.

---

## Later waves

*(to be appended as the remaining files are annotated)*
