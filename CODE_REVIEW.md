# LPProcessor code review

Findings collected while adding file-header and per-procedure comments to the
first-party sources. Line numbers refer to the files as they stand on this
branch (after comment insertion). Severity is `CRITICAL` / `HIGH` / `MEDIUM` /
`LOW` / `NIT`.

Vendored third-party trees (`lloyd-yajl-66cb08c/`, `tinyxml2-master/`,
`HMM/KenLM/`, `packages/`) are out of scope. This is a comments-only change;
no executable logic was intended to change.

## Cross-cutting conclusions

The parser is a single large Windows/MSVC/MySQL research codebase with a
clear pipeline (init → read → tokenize → pattern match → agreement winnow →
syntactic/semantic/time relations → objects → speakers → QA). The comments
now document that pipeline at the top of each first-party file.

The same defect classes recur across stages:

1. **Secrets in source.** MySQL `root`/`byron0` is copied through `DB.cpp`,
   `DBCreateSQLSchema.cpp`, `getThesaurus.cpp`, `processGutenbergRDFtoSQL`,
   `pyLP.py`, and `convertPDFTextToDatabase`. Live Google CSE, Bing, and
   Merriam-Webster keys sit in QA/acquisition TUs; a Twitter password and a
   NewsBank library card are in comments. Rotate all of these and load them
   from the environment.
2. **Undefined-behavior on the hot path.** Usage vectors and
   `minSeparatorCost` are `reserve`d then indexed (every successful parse
   match / every winnow). Several 4–20MB stack buffers will overflow a 1MB
   MSVC stack. Iterator erase-then-read, `wstring[0]` on empty, and
   `m[end]` when `end == m.size()` appear in object/speaker/syntax code.
3. **Logic inversions that silently drop linguistic coverage.**
   `containingSpeakerGroup` compares spans to the loop index;
   `speakerGroupTransition` increments instead of decrementing;
   `months_abb` skips May–July; `cName::notNull()` is inverted;
   `sameSpeaker` one-sided tests are inverted; several `&&`/`||` mixes
   parse as `(A && B) || C`.
4. **SQL and HTTP built by concatenation.** Titles, words, and RDF fields
   are interpolated into `INSERT`/`LIKE`/`VALUES('%s')` without escaping.
   Wikipedia/MusicBrainz/Twitter URLs are HTTP and unescaped.
5. **Process-control footguns.** `lplog(LOG_FATAL_ERROR)` waits on
   `getchar()` then `exit(0)` (unattended children hang, then look
   successful). Many `LOCK TABLES` paths never `UNLOCK`. `_exit(0)` skips
   log flush and MySQL close.

Highest-value first fixes: remove secrets, `resize` the usage /
`minSeparatorCost` vectors, parenthesize the query-buffer macros, make
FATAL abort with a non-zero status and no stdin wait, and correct the
inverted speaker/name/month predicates.

---

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

## Wave — object and pronoun resolution (`identifyObjects.cpp`,
`resolveObjects.cpp`, `resolveMetaGroupObjects.cpp`,
`resolveFirstSecondPersonPronouns.cpp`)

- **[HIGH] identifyObjects.cpp:1536 — `getPrincipalWhereAndEndAndNameInfo` args swapped** —
  the call passes `embeddedName, plural` against
  `(..., bool& pluralNounOverride, bool& embeddedName, ...)`. Swap the two
  arguments.

- **[HIGH] identifyObjects.cpp:240 — `isPleonastic` MEANS branch is off-by-one** —
  the loop tests `m[where + 2]` after the `it is ...` arm. The verb is at
  `where + 1`.

- **[HIGH] identifyObjects.cpp:1261 — missing parens around `|| flagNounOwner`** —
  `flagNounOwner` alone sets `ownerWhere = I` even when `identifyObject`
  failed. Parenthesize as the `ow >= 0` branch already does.

- **[HIGH] resolveFirstSecondPersonPronouns.cpp:196 — use-after-erase of `objectMatches` iterator** —
  `erase(oi)` then reads `oi->object`. Save `oi->object` before erase.

- **[HIGH] resolveObjects.cpp:732 — `containingSpeakerGroup` compares span to the loop index** —
  `sgBegin >= I && sgEnd < I` uses the group index, not a source position.
  Always returns `end()`. Take a `where` and test
  `sgBegin <= where && where < sgEnd`.

- **[HIGH] resolveObjects.cpp:389 — plural non-gendered loop tests `localObjects[0]` not `[s]`** —
  Use `localObjects[s].om.object <= 1`.

- **[HIGH] resolveObjects.cpp:517 — Num/address matcher returns after the first local object** —
  `return true` is inside the `for` body. Move it after the loop, or return
  only when `objectMatches` is non-empty.

- **[HIGH] resolveObjects.cpp:1600 — `speakerGroups[currentSpeakerGroup + 1]` unchecked** —
  last-group mentions are an out-of-bounds read. Guard like `addNewSpeaker`.

- **[HIGH] resolveObjects.cpp:844 — `preferWordOrder` erase has no size guard** —
  `erase(begin() + 1)` on an empty or singleton list is UB. Same at
  `resolveRelativeObject:1010`. Require `size() >= 2`.

- **[HIGH] resolveMetaGroupObjects.cpp:1692 — `previousPrimaryQuote` used without `>= 0`** —
  `m[previousPrimaryQuote].getQuoteForwardLink()` when the index is still
  -1. Guard with `previousPrimaryQuote >= 0`.

- **[HIGH] resolveMetaGroupObjects.cpp:617 — `*csg->povSpeakers.begin()` on a possibly empty set** —
  Test `!csg->povSpeakers.empty()` first.

- **[MEDIUM] identifyObjects.cpp:267 — `searchExactMatch` tests the new object’s `eliminated` flag** —
  always true for the freshly built `object`. Test `!objects[*s].eliminated`.

- **[MEDIUM] identifyObjects.cpp:204 — pleonastic-it requires five trailing tokens** —
  `where + 4 > m.size()` rejects shorter valid patterns. Require only as
  many tokens as the arm that fires.

- **[MEDIUM] resolveObjects.cpp:1371 — class-penalty `&&` / `||` precedence** —
  every business/verb object is penalized even when not in salience.
  Parenthesize the three class tests.

- **[MEDIUM] resolveMetaGroupObjects.cpp:1307 — `min(1, ownerMatches.size()) + 1` is not “one more than the owner”** —
  the expression is always 1 or 2. Use `size() + 1`.

- **[LOW] identifyObjects.cpp:1833 — `(%d%)` in `printObjects`** —
  a lone `%` before `)`. Use `%%`.

---

## Wave — pattern engine (`definePatterns.cpp`, `pattern.cpp`, `pattern.h`,
`patternMatchArray.cpp`/`patternMatchArray.h`,
`patternElementMatchArray.cpp`/`patternElementMatchArray.h`)

- **[HIGH] pattern.h:183 — `initializeUsage` only `reserve()`s usage counters** —
  `size()` stays 0; `incrementUse` / `fillPattern` then index those vectors
  on every match (UB). Use `resize(n, 0)`.

- **[HIGH] pattern.cpp:945 — `processForm` writes through `form.c_str()`** —
  mutates the `wstring` via the const pointer to split `|specificWord`.
  Copy the suffix without touching `*ch`.

- **[HIGH] pattern.cpp:1803 — `setMandatoryAncestorPatterns` updates the wrong bitset** —
  ORs into `ancestorPatterns` instead of `mandatoryAncestorPatterns`.
  Mandatory-ancestor queries then miss ancestors.

- **[HIGH] patternElementMatchArray.cpp:145 — format string missing its argument** —
  `L"Illegal count of %d ..."` has no `count`. Pass `count`.

- **[MEDIUM] patternMatchArray.cpp:65 — `PMA::clear` leaves a dangling `content`** —
  `tfree` then `allocated = 0` but `content` is not NULLed. Set
  `content = NULL`.

- **[MEDIUM] patternMatchArray.cpp:119 — `read()` bounds-check uses the stale `count`** —
  the check runs before `copy(count, ...)`. Parse count into a local first.

- **[MEDIUM] patternMatchArray.cpp:419 — `queryPattern(int, int& len)` does not initialize `len`** —
  Set `len = -1` on entry.

- **[MEDIUM] patternMatchArray.cpp:440 — `queryTagSet` can index `patternTagStrings[-1]`** —
  Guard `tag >= 0` before the NAME-precedence test.

- **[MEDIUM] patternMatchArray.cpp:599 — `1 << 31` is signed overflow** —
  Use `INT_MIN` or `1u << 31`.

- **[MEDIUM] patternMatchArray.h:152 — `queryTag` returns the first hit, not the longest** —
  `break`s on the first `hasTag` match. Remove the `break`.

- **[MEDIUM] patternMatchArray.cpp:158 — `operator=` is not self-assignment safe** —
  Same in `cPatternElementMatchArray::operator=`. Guard
  `if (this == &rhs)`.

- **[MEDIUM] definePatterns.cpp:2824 — `{HAIL|OBJECT}` is one tag name, not two** —
  interns unused tag `HAIL|OBJECT`. Change to `{HAIL:OBJECT}`.

- **[MEDIUM] pattern.cpp:2689 — `printPatternStatistics` header format/arg mismatch** —
  seven `%s` plus `%%` but nine string arguments. Align the format with
  the data columns.

- **[LOW] pattern.cpp:1162 — ABNF dump refers to `onlyAfterQuote`** —
  the live flag is `afterQuote`. Enabling `ABNF` will not compile.

- **[LOW] pattern.cpp:2700 — `printPatternStatistics` deletes the global pattern objects** —
  leaves dangling pointers in `patterns` / `patternReferences`.

- **[LOW] pattern.h / pattern.cpp — `findPattern` not-found conventions disagree** —
  some overloads return `patterns.size()`, one returns `(unsigned)-1`.

- **[LOW] patternMatchArray.cpp:147 / patternElementMatchArray.cpp:196 — `operator==` takes the other array by value** —
  Take `const T&`.

- **[LOW] patternElementMatchArray.cpp:600 — `generatePEMACount` bounds test is off by one** —
  `nextPosition == count` is accepted then used as an index. Use `>=`.

- **[NIT] patternMatchArray.h:96 / :110 — `querySingleNoun` and `findObjectElement` have no definition** —
  Remove the declarations or implement them.

- **[NIT] patternElementMatchArray.h:11 — `CHILDPATBITS` is both a shift and an eFlags enumerator** —
  `flagsStr` does `flagSet(CHILDPATBITS)` which tests `flags & 15`. Split
  the shift constant from the flag.

---

## Wave — semantic relations, time, names (`semanticRelations.cpp`/`semanticRelations.h`,
`timeRelations.cpp`/`timeRelations.h`, `names.cpp`/`names.h`)

- **[HIGH] timeRelations.cpp:2832 — `speakerGroupTransition` walks `I++` instead of `I--`** —
  supposed to find a previous speaker group; `I++` immediately walks into the
  current group, so `lastSG` becomes `sg` and `tlTransition` is wrong. Use
  `I--`.

- **[HIGH] timeRelations.cpp:2248 — `months_abb` skips May/June/July** —
  `{ jan, feb, mar, apr, aug, sept, oct, nov, dec }`. `whichMonth("aug")`
  returns May’s slot. Insert `may`/`jun`/`jul`.

- **[HIGH] timeRelations.cpp:1181 — `twsCapacity` inserts “morrow”, breaking `eCapacity`** —
  `whichCapacity("yesterday")` is not `cYesterday`. Drop `"morrow"` or add
  a matching enumerator.

- **[HIGH] timeRelations.h:324 — `cTimeInfo::clear()` leaves named-day / deictic fields uninitialized** —
  `write()` then persists stack garbage into the source cache. Zero them
  to `-1` in `clear()`.

- **[HIGH] names.cpp:1786 — “twentieth” maps to 0** —
  `numeralOrdinalMap` has `{ L"twentieth", 0 }`. Change the value to `20`.

- **[HIGH] names.cpp:557 — `merge()` never replaces a single-letter name part** —
  `first[1]` as “has a second character” is the opposite of the comment.
  Compare lengths; do not index `[1]` on an empty string.

- **[HIGH] names.cpp:471 — `cName::notNull()` is inverted** —
  returns true when *every* part is `wNULL`. Flip or rename;
  `isCompletelyNull()` in `resolveObjects.cpp` is the correct test.

- **[HIGH] semanticRelations.cpp:1453 — `getAfterVerb` indexes `m[afterVerb]` before the size check** —
  when `whereVerb` is the last token this is OOB. Test the bound first
  (same at 1456).

- **[HIGH] names.cpp:416 — unbounded `wcscat` in `cName::hn(wchar_t*)`** —
  no `maxbuf`. Use the `wstring` overload or `_snwprintf`.

- **[HIGH] timeRelations.cpp:1526 — `ageTransition` builds `objects.begin() + so` when `so` may be -1** —
  Only form the iterator when `so >= 0`.

- **[HIGH] semanticRelations.cpp:1672 — `detectPlaceTransition` `&&`/`||` precedence** —
  a physical/time prep-object is enough even when the other conjuncts
  fail. Parenthesize as `A && (B || C)` if both sides were required.

- **[MEDIUM] names.cpp:2024 — plurality reject compares secondary to itself** —
  always false. Compare primary vs secondary.

- **[MEDIUM] semanticRelations.cpp:1359 — “by hook or by crook” is unbounded** —
  Require `wherePrep + 4 < (int)m.size()`.

- **[MEDIUM] names.cpp:925 — `evaluateName` peeks `sourcePosition+1` without a bound** —
  Same at 1200. Check `sourcePosition + 1 < (int)m.size()`.

- **[MEDIUM] timeRelations.cpp:969 — `ti` is only set when `inMultiObject==2`** —
  an empty `timeInfo` makes `end()-1` invalid.

- **[MEDIUM] semanticRelations.h:82 — `calculateScore` leaves `score` unset when both distances are 0** —
  Set `score = 0` in the `else`.

- **[MEDIUM] names.cpp:868 — `insertSubSQL` `maxbuf-buflen` can go negative** —
  Clamp remaining to 0.

- **[MEDIUM] semanticRelations.cpp:223 — `setTimeFlowTense` comment vs return** —
  always returns `true`; negation is stored in `tft.negation`.

- **[LOW] timeRelations.cpp:2771 — `determineTimelineSegmentLink` is a stub** —
  always returns `false`.

- **[LOW] names.h:14 — `cNickName::operator==` takes `wstring` by value** —
  Take `const wstring&`.

- **[NIT] names.cpp:406 — `wchar_t*` print is labelled “optimized”** —
  it is the unsafe twin of the `wstring` overload.

---

## Wave — data acquisition (`getWikipedia.cpp`, `getWordNet.cpp`,
`getThesaurus.cpp`, `getDictionary.cpp`, `initializeDictionary.cpp`,
`getMusicBrainz.cpp`/`getMusicBrainz.h`, `getTwitter.cpp`,
`getWordNetMaps.cpp`, `tagOperations.cpp`, `vcXML.cpp`/`vcXML.h`,
`Adversary bugs.cpp`, `relationTypes.h`)

- **[CRITICAL] getDictionary.cpp:1209 — Merriam-Webster API key hardcoded in the URL** —
  `?key=ba4ac476-dac1-4b38-ad6b-fe36e8416e07`. Move to env/secrets and rotate.

- **[CRITICAL] getThesaurus.cpp:368 — MySQL root password hardcoded** —
  `mysql_real_connect(..., "root", "byron0", "lp", ...)` in `testThesaurus()`.

- **[CRITICAL] getWikipedia.cpp:1661 — NULL dereference after failed `processPath`** —
  `cSource* source = NULL` then `source->m.begin()`. Any failed Wikipedia
  child parse crashes.

- **[CRITICAL] vcXML.cpp:73 — NULL dereference in `aH` when `>` is missing** —
  `ech < ch` is true when `ech` is NULL; `*ch = 0` writes through NULL.

- **[HIGH] getTwitter.cpp:56 — account password committed in source** —
  comments record `password: builder!12`. Rotate and delete from history.

- **[HIGH] getWordNet.cpp:486 / getThesaurus.cpp:73 — SQL concatenated from the lookup word** —
  `query += word + L"'"`. Escape or bind. Also `wtmp[wtmp.length()-1]` on
  a possibly empty `wtmp`.

- **[HIGH] getMusicBrainz.cpp:198 — unchecked `FindAttribute("id")->Value()`** —
  a release/artist/label without `id` is a NULL deref (same at 205, 211,
  221, 272, 336, 338, 386).

- **[HIGH] getWordNetMaps.cpp:202 — `readWNMaps` leaks the file buffer on every failure** —
  `return false` without `tfree`. `writeWNMaps` also leaks `fd`.

- **[HIGH] getWikipedia.cpp:1260 — Wikipedia search URL is HTTP and unescaped** —
  spaces/`&` in object names corrupt the query. Same class in
  `getMusicBrainz.cpp:85` and `getTwitter.cpp:115`.

- **[HIGH] getWikipedia.cpp:322 — `firstMatchTableDeleteNested` ignores a successful `<li>` find** —
  `beginPos` is only updated in the `<li value=` fallback.

- **[HIGH] getWikipedia.cpp:1593 — `processPath` leaks the new `cSource` on empty tokenize** —
  `return -1` without `delete source`.

- **[HIGH] initializeDictionary.cpp:433 — `readWords` leaks the cache buffer** —
  `if (where < 0) return -1` skips `tfree`.

- **[HIGH] vcXML.cpp:472 — `readVBNet` leaks the `FindFirstFile` handle** —
  `if (fd < 0) return;` inside the loop never `FindClose`.

- **[HIGH] getWordNetMaps.cpp:151 / getWikipedia.cpp:793 — huge stack allocations** —
  10MB / 20MB stack buffers. Heap-allocate or stream.

- **[MEDIUM] getWordNet.cpp:239 — third-party API key left in a comment** —
  Big Huge Thesaurus key. Rotate if still live.

- **[MEDIUM] getMusicBrainz.h:70 — header/definition arity mismatch** —
  header has 3 args; `.cpp` has a 4th `filterNameDuplicates`.

- **[MEDIUM] getMusicBrainz.cpp:340 — `getRecordings` reads the wrong release-list** —
  walks metadata-level `release-list` instead of the current `<recording>`'s.

- **[MEDIUM] getWikipedia.cpp:301 — footnote scan reads past end** —
  `[` without `I+3 < length`. Same class in `convertFromWikilinkEscape`
  and `eliminateHTMLCharacterEntities`.

- **[MEDIUM] tagOperations.cpp:613 — `isPPN` uses logical `&&` on flags** —
  `flags && queryWinnerForm(...)` is true for any non-zero flags word.

- **[MEDIUM] tagOperations.cpp:192 — `found` is never set true** —
  the erase-and-continue branch is dead.

- **[MEDIUM] getThesaurus.cpp:674 — `_filelength(fd)` before `fd` is validated** —
  Check `fd` first; `malloc(fl+4)` is never freed.

- **[MEDIUM] getThesaurus.cpp:190 — `splitPrimarySynonyms` can divide by zero then `exit(0)`** —
  Guard `totalRows` and return instead of exiting.

- **[MEDIUM] getWordNet.cpp:1765 — `initializeNounVerbMapping` leaks on a corrupt cache** —
  `tmalloc` then `return -1` without `tfree`.

- **[LOW] getTwitter.cpp:129 — `lastId` is the last tweet seen, not the max** —
  Track the maximum ID.

- **[LOW] initializeDictionary.cpp:57 — `predefineWords` mutates caller storage** —
  writes through `Inflections` / `InflectionsRoot` (often literals).

- **[LOW] getMusicBrainz.cpp:438 — `pushWhereEntities` takes `mbs` by value** —
  Pass `const vector&`.

- **[NIT] vcXML.cpp:404 — error path names the wrong directory** —
  `VBNet` vs `VerbNet`.

- **[NIT] relationTypes.h:21 — `VerbWithNext1MainVerb` is computed from `VERB_HISTORY`** —
  inserting an enum value silently shifts stored `typeId`s.

- **[NIT] Adversary bugs.cpp — not compiled** —
  a single block comment (Secret Adversary gold bugs).

---

## Wave — unused source, satellite tools, pyLPBackEnd

- **[HIGH] processGutenbergRDFtoSQL/Program.cs:84 — hardcoded MySQL `root`/`byron0`** —
  also lines 106, 129, 230. Move to config/env.

- **[HIGH] processGutenbergRDFtoSQL/Program.cs:376 — SQL built by string concatenation** —
  RDF `creator`/`title` interpolated into INSERT. Use `MySqlParameter`.
  Same at line 143 for the path UPDATE.

- **[HIGH] Web/pyLPBackEnd/.../pyLP.py:74 — hardcoded DB password and Flask secret** —
  `password='byron0'` and `app.secret_key = "kjhasd@#$#@"`.

- **[HIGH] Web/pyLPBackEnd/.../pyLP.py:107 — SQL injection via LIKE concatenation** —
  `LOWER(author) like LOWER('%`+author+`%')` (also `search_author` /
  `get_path`). Use parameterized queries.

- **[HIGH] convertPDFTextToDatabase/.../Source.cpp:329 — hardcoded MySQL password** —
  `root`/`byron0` again.

- **[HIGH] unused source/getNewsbank.cpp:381 — library-card credential in source** —
  `libcard=80035000052216`. Remove/rotate.

- **[HIGH] unused source/relations.cpp:452 — `Source, ::` will not compile** —
  stray comma after the class name on every method. Same in
  `getFreebase.cpp:277`.

- **[HIGH] unused source/newPatternDetection.cpp:95 — PEMA walk never advances `p`** —
  increment expression is `pema[p].nextByPosition` (not assigned). Infinite
  loop. Should be `p = pema[p].nextByPosition`. Also trees are cleared
  before print (line 304).

- **[HIGH] unused source/DBMultiWordRelations.cpp:346 — LOCK TABLES leak on early return** —
  `checkFull` failure skips `UNLOCK TABLES`.

- **[MEDIUM] pyLP.py:81 — login always uses `uid=1`** —
  all browsers share one session. Use `uuid.uuid4()`.

- **[MEDIUM] LPIO.py:48 — short read on UTF-16 string** —
  `ch[0]` after a 0–1 byte EOF is `IndexError`.

- **[MEDIUM] TFI.py:33 — `return False` inside the for-loop** —
  `has_winner_verb_form` only inspects form 0.

- **[MEDIUM] VerbNet.py:61 / TimeInfo.py:90 / BitObject.py:20 — Java leftovers** —
  `.length()`, set-as-list indexing, mutating a tuple. These raise at
  runtime if called.

- **[MEDIUM] WordMatch.py:208 — `has_winner_verb_form` no null check** —
  AttributeError when the word is missing from `WordClass.words`.

- **[MEDIUM] convertPDFTextToDatabase/.../Source.cpp:91 — realloc result overwrites pointer** —
  leak + NULL deref on failure. Same at the local `WideCharToMultiByte`.

- **[MEDIUM] unused source/getInterviewTranscripts.cpp:164 — URL month is 0-based** —
  `tm_mon` used as the calendar month. Use `tm_mon+1`. NPR crawler
  (`while (true)`) also never exits.

- **[MEDIUM] unused source/readGutenbergWebstersDictionary.cpp:10 — `getLine` reads `buffer[where]` before the bound check** —
  Swap the tests. Also `dictionaryBuffer[bufferLen + 1] = 0` writes one
  past the payload (line 195).

- **[MEDIUM] unused source/getFreebase.cpp:508 — 200MB malloc unchecked** —
  then `memstr` on NULL. `vb[bbi+1]` also unguarded (line 557).

- **[MEDIUM] unused source/originalhmm.cpp:109 — `fopen` / newline assumptions** —
  unchecked `FILE*`; `line[strlen(line)-1]=0` on an empty last line.
  `--s--` may be missing from tags (line 405).

- **[MEDIUM] checkTypes/checkTypes.cpp:120 — unbounded `strcat` on a 1024-byte path** —
  recursive walk. Also `getPath` return ignored (line 97).

- **[MEDIUM] DirectoryAnalysis/Program.cs:47 — file count is the parent's, not the child's** —
  `GetFiles(path)` instead of `GetFiles(folder)`.

- **[MEDIUM] correctRDF/correctRDF.cpp:28 — 16348-byte buffer (likely 16384 typo)** —
  `pastFirstError` also drops every triple before the first overlong one.

- **[LOW] convertPDFTextToDatabase/.../Source.cpp:125 — unescaped word in SQL** —
  `mainEntry = '" + word + "'"`.

- **[LOW] unused source/getBNC.cpp:289 — overlapping `wcscpy` of entity substitute** —
  Use `wmemmove` or a second buffer.

- **[NIT] LPWeb/Startup.cs:48 — `UseAuthorization` without authentication** —
  a no-op until an auth scheme is registered.

---

## Wave — agreement and syntactic relations (`agreement.cpp`,
`syntacticRelations.cpp`, `syntacticRelationGroups.cpp`,
`syntacticRelations.h`)

- **[HIGH] agreement.cpp:4342 — `eliminateLoserPatterns` writes past `minSeparatorCost`** —
  `reserve` then `[I]=` on a size-0 vector is UB every winnow. Use `resize`.

- **[HIGH] agreement.cpp:1382 — `reduceCostIfRestate` discards its result** —
  `relationCost` is passed by value, so restated subjects keep the full
  relation cost. Take `int&`.

- **[HIGH] agreement.cpp:1367 — `disagreementWithAmbiguousTense` indexes `tagSet[-1]`** —
  `mainVerbTag` can be -1 on the question path. Guard with `mainVerbTag >= 0`.

- **[HIGH] agreement.cpp:651 — `markChildren` skips `allLocations[0]` after reassess** —
  `lc = 0` then the `for` increment does `lc++`. Set `lc = (unsigned)-1` or
  use a while-loop.

- **[HIGH] agreement.cpp:1618 / 3949 — BNC helpers bound-check PEMA, then index `m[]`** —
  Compare against `m.size()`.

- **[HIGH] agreement.cpp:2151 / 2354 / 2093 — unbound `m[where+1]` reads** —
  sentence-final `his`/`her`, verb, or `her own/best` OOBs. Require
  `+ 1 < m.size()`.

- **[HIGH] agreement.cpp:1773 — `setSecondaryCosts` dereferences a possibly-null `pm`** —
  `pma.find` can return nullptr. Skip the cascade when `pm == nullptr`.

- **[HIGH] syntacticRelations.cpp:615 — `checkAmbiguousVerbTense` `&&`/`||` mix** —
  `(A && B) || C` rewrites `sense` when `masterVerbWord` is NULL even if
  the incoming sense is not PRESENT/PAST. Parenthesize as `A && (B || C)`.

- **[HIGH] syntacticRelations.cpp:1607 — `evaluateSubjects` forward scan is not gated on empty** —
  `empty && A || B` runs the “did he?” scan even when subjects already
  exist, and `m[where+maxLen]` is unbound. Write `empty && (A || B)` and
  bound `where + maxLen + 1`.

- **[HIGH] syntacticRelations.cpp:3325 — `testSyntacticRelations` reads `m[end]` when `end==m.size()`** —
  Test `end < m.size()` first.

- **[HIGH] syntacticRelationGroups.cpp:701 — cache ctor wipes `skip` / `changeStateAdverb` after unpack** —
  those bits never survive a source-cache round-trip. Drop the two
  assignments, or persist the real flags (`write()` currently packs
  `convertFlags(false,false,false,0)`).

- **[MEDIUM] agreement.cpp:1114 — `substitutePrepObjectSomeOf` skips N_AGREE at index 0** —
  `findTagConstrained(...) > 0` should be `>= 0`.

- **[MEDIUM] agreement.cpp:465 — `compareCost` multiplies three ints** —
  overflows for long spans. Prefer `int64_t`.

- **[MEDIUM] agreement.cpp:735 — `getAllLocations` returns `minCost` as unsigned** —
  a negative PMA cost wraps to a huge value. Return `int`.

- **[MEDIUM] agreement.cpp:210 — `assessCost` `wsprintf` into 1024 wchars** —
  unbounded pattern names can overflow. Use `_snwprintf`.

- **[MEDIUM] agreement.cpp:2911 — `longSubjectBindingMismatch` else-if never fires** —
  integer division makes the last-noun frequency bias dead.

- **[MEDIUM] syntacticRelations.cpp:1476 — `findPrepRole` does not honor its -1 contract** —
  body does `m[whereLastPrep].relPrep` immediately. Return -1 up front.

- **[MEDIUM] syntacticRelations.cpp:2946 — `setRole` RE_OBJECT walk can scan the whole document** —
  `|| !isEOS` keeps the loop alive. Cap at `position-10`.

- **[MEDIUM] syntacticRelations.cpp:2499 — Watson special-case skips `getObject()>=0`** —
  `getObject()==-1` indexes `objects[-1]`.

- **[MEDIUM] syntacticRelations.cpp:1867 / 1173 — unbound `m[sourcePosition+1]` / `m[wp+1]`** —
  Require a size check; `markPrepositionalObjects` also uses `wp+1` when
  `pTag<0`.

- **[MEDIUM] syntacticRelations.cpp:317 / .h:48 — `cWordGroup` header vs `.cpp` type mismatch** —
  header has `vector<wstring>`; `#ifdef ACCUMULATE_GROUPS` body uses
  `tIWMM`. Enabling the ifdef will not compile. Also non-default ctors
  leave `index`/`otherFlag` uninitialized.

- **[MEDIUM] syntacticRelationGroups.cpp:886 / 609 / 839 / 380 — SRG copy/cache/adverb state** —
  remapping ctor drops tense/QA/adverb fields; live ctor leaves
  `tft.presType` uninitialized; `getWSAdverb(true)` has no time-flag
  filter.

- **[LOW] agreement.cpp:3047 / 3090 — debug logs read `m[begin-1]` / `m[end]` unbound** —
  Gate the logs.

- **[LOW] syntacticRelations.cpp:152 — `getRelStr` maps overflow types to the wrong `*1*` label** —
  Use `/ VERB_HISTORY` for the generation.

- **[NIT] agreement.cpp:2398 — here/there/home log has an extra `%s` argument** —

- **[NIT] syntacticRelationGroups.cpp:167 — `checkInsertPrep` compares signed `wp` to `m.size()`** —

---

## Wave — speaker groups and speaker resolution (`identifySpeakerGroups.cpp`,
`resolveSpeakers.cpp`)

- **[HIGH] identifySpeakerGroups.cpp:69 — unbounded `copy(cOM&)` deserializer** —
  no `limit` argument. A corrupted SourceCache count walks off the buffer.
  Refuse the read when `where + sizeof(cOM) > limit`.

- **[HIGH] identifySpeakerGroups.cpp:2934 — `sameSpeaker` inverted when only one side has `objectMatches`** —
  `return in(...) == matches.end()` is true when the object is *not* in
  the other side. Change `==` to `!=` on both one-sided tests.

- **[HIGH] identifySpeakerGroups.cpp:1619 — `isFocus` indexes `m[I]` before the bounds test** —
  Swap the conjuncts (`I < m.size() && ...`).

- **[HIGH] identifySpeakerGroups.cpp:940 — hail-delete log dereferences `localObjects.end()`** —
  Guard the log with `lsi != localObjects.end()`.

- **[HIGH] identifySpeakerGroups.cpp:1291 — `pushTemporarySpeakerGroupAndErase` takes `&speakerGroups[-1]` when empty** —
  Only take the address when `!speakerGroups.empty()`.

- **[HIGH] resolveSpeakers.cpp:286 — `oStr[0]=0` on a default-empty `wstring`** —
  `operator[]` on `size()==0` is UB. Delete the `oStr[0]=0` line.

- **[HIGH] resolveSpeakers.cpp:5286 — `_VERBPAST` audience scan compares a source position to a pattern length** —
  the loop `audienceObjectPosition < end` never iterates, so “said X to Y”
  after a past-tense verb is not picked up. Convert `end` to an absolute
  position as the `_VERBREL1` branch does.

- **[MEDIUM] identifySpeakerGroups.cpp:962 — empty-group early-out returns `false` (object 0, Narrator)** —
  an `int` function whose no-match sentinel is `-1`. Return `-1`.

- **[MEDIUM] identifySpeakerGroups.cpp:3310 — `block && m[I].flags & 1` operator precedence** —
  parsed as `(block && flags) & 1`. Write `block && (m[I].flags & 1)` if
  bit 0 was intended.

- **[MEDIUM] identifySpeakerGroups.cpp:2265 — `intString` OOB when the POV range is empty** —
  Return `L""` when `startPOVI >= povi`.

- **[MEDIUM] resolveSpeakers.cpp:3722 — `preferPreviousSpeaker` is a stub** —
  always returns `false`. Restore the rule or delete the calls.

- **[MEDIUM] resolveSpeakers.cpp:1059 — `increaseAge` ignores secondary-quote sentences** —
  nested-quote sentences never age local focus. Confirm against the
  quote-age design.

- **[LOW] identifySpeakerGroups.cpp:513 — extra `lplog` argument** —
  format has two conversions; the call also passes
  `speakerSections.size()`.

- **[LOW] resolveSpeakers.cpp:685 — `readStringVector` reads 99 wchar_ts into a 1024 buffer** —
  Pass `sizeof(buf)/sizeof(*buf)`.

- **[NIT] resolveSpeakers.cpp:3722 — dead helper left in the speaker-ranking chain** —
  Remove or implement `preferPreviousSpeaker`.
