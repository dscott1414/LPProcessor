# Windows parity — making the macOS .SourceCache identical to the Windows one

Goal: `lp` on macOS should produce a `.SourceCache` byte-identical to
`caches/texts/Christie  Agatha  1890-1976/Secret Adversary.txt.from_windows.SourceCache`
(70,569,669 bytes, written by the Windows build on 2022-03-12).

This file records what has been established so far, so the hunt can be resumed
without repeating the groundwork.

## Byte-identity is achievable in principle

Four things had to be true, and all four are:

| Question | Answer | How it was checked |
|---|---|---|
| Did the port change the cache **format**? | **No** | The 50-field sequence written by `cWordMatch::writeRef`, `writeFlags` and `cSource::write` is identical before and after the port commit `a6cd92c`, once the mechanical `wchar_t`→`char16_t` rename is normalized away. |
| Did the **pattern set** change since 2022? | **Almost none** | 657 of 659 `cPattern::create` calls are identical to the 2022-03-12 revision (`b3833082`) after normalizing `L"`→`u"`. |
| Did the **dictionary** change? | **No** | `SELECT SUM(TS > '2022-03-12') FROM words WHERE sourceId IS NULL` returns 0; the newest row is from 2020-07-28. |
| Is anything in the format inherently non-reproducible (timestamps, addresses)? | **No** | The header is `sourceVersion`, `storageLocation`, `count`, then fixed records. |

So every remaining difference is a behavioural difference in the code, and is
therefore findable.

### The two pattern definitions that do differ

- `questionProcessing.cpp`, `_DISPLACED_OBJECT[1]`: `","` → `u","`. **Mine**, and
  almost certainly *toward* Windows: as a narrow literal read through
  `va_arg(_, lpchar_t*)` it was undefined behaviour, and the Windows build must
  have resolved it to something valid or its parse would have aborted the way
  macOS's did.
- `definePatterns.cpp`, `__CLOSING__S1[1]`: `_HAIL_OBJECT{HAIL|OBJECT}` →
  `{HAIL:OBJECT}`. Made by a **prior agent** in `8e1bd47` (2026-08-29), not by
  this work. `:` is the tag separator this codebase uses, so it is probably a
  genuine fix — but it is a behavioural change relative to the 2022 build and is
  a candidate if a `__CLOSING__S1` divergence shows up.

## Current state (macOS run 2)

| | macOS | Windows |
|---|---|---|
| size | 67,355,747 | 70,569,669 |
| `sourceVersion` | 9 | 8 |
| word matches | 100,627 | 100,657 |

`SOURCE_VERSION` was bumped 8→9 in the port commit `a6cd92c`. Since the format is
unchanged, that bump was defensive rather than required; it will need to go back
to 8 for byte-identity, but **not until the content matches**, or it would just
hide the real differences.

## Result so far: the word sequences are identical

Every one of the **100,627** words macOS produces matches the Windows reference
**exactly, word for word**, across the whole 905 KB novel. Windows has 30 extra
tokens and nothing else differs.

| | words |
|---|---|
| macOS, first run | 100,633 |
| macOS, second run | **100,627** |
| Windows reference | 100,657 |

Getting there resolved both classes of difference, and neither was what it looked
like:

**The 30 extra Windows tokens are a Windows bug, not a macOS omission.** They are
thirty consecutive `in` at the very end of the file. The book ends
`"And a damned good sport too," said Tommy.`, and macOS ends correctly with
`said tommy . ||| ||| |||`. Those thirty tokens are the tokenizer running past
the end of the decoded text — the same `bufferLen` defect fixed in `5b90003`,
which the 2022 Windows build still had. **Byte-identity with this reference would
require re-introducing that bug.**

**The six multi-word tokens were a cold-start artifact, not a port bug.** On the
first run macOS split `vice versa`, `au revoir` (×3) and `peche melba` (×2) into
pairs. All three are real dictionary entries (ids 19914, 57776, 141877) — easy to
miss because they carry `sourceId = 0`, not NULL, and the loader selects
`WHERE sourceId IS NULL`, so only 7,981 words preload and the rest are fetched on
demand *during* the parse. That is too late for `cWord::continueParse`, which
joins multi-word tokens by binary-searching `multiElementWords` at tokenization
time. The run then writes those words to `<source>.wordCacheFile`, so the *next*
run loads them upfront and joins correctly. That per-source cache did not exist
here — it was never shipped with the reference — which is why the first run
differed and the second did not.

**Practical consequence:** a first parse of any source will differ from a
warmed-up one. Compare only second-or-later runs, or ship the `.wordCacheFile`
alongside a reference cache.

## First divergence (measured on run 1 — re-check against run 2)

Record 0 (`|||`, the section separator) is **byte-identical**, 435 bytes on both.
Record 1 is the word `prologue`: 485 bytes on macOS, 445 on Windows. Decoded:

| field | macOS | Windows |
|---|---|---|
| `word` | `prologue` | `prologue` |
| `flags` | `0x0100000018800000` | `0x0100000010800000` |
| `maxMatch` | 1 | 0 |
| `minAvgCostAfterAssessCost` | 0 | 1000000 |
| `lowestAverageCost` | 0 | 3000 |
| `maxLACAACMatch` | 1 | 0 |
| `beginPEMAPosition` | 1 | −1 |
| `endPEMAPosition` | 2 | −1 |
| `PEMACount` | 2 | 0 |

The flags differ by exactly bit 27, `flagTopLevelPattern`, set at
`pattern.cpp:784` when `isTopLevelMatch()` succeeds.

**This is a pattern-elimination difference, not a tokenization one.** The word
text agrees, and `isTopLevelMatch` only requires separators on both sides —
`prologue` sits between two `|||` on both platforms. What differs is that Windows
finished with `PEMACount == 0` while still recording a `lowestAverageCost` of
3000: it found matches and then eliminated them. macOS kept two.

So the next step is `eliminateLoserPatterns` / `assessCost`, for a one-word
sentence between separators. Note `eliminateLoserPatternsPhase1` special-cases
short sentences:

```cpp
int paca = PRE_ASSESS_COST_ALLOWANCE * len / 5;
if (effectiveSentenceLength <= 2)
    paca = PRE_ASSESS_COST_ALLOWANCE;
```

## Unknowns worth pinning down

- **Which flags the Windows run used.** A `.patternUsage` file sits beside the
  cache with the same 2022-03-12 timestamp, so that run wrote pattern usage. The
  run reproduced here used `-book 2 3 -BC 0 -SW -forceSourceReread -retry`. A
  different flag set would produce a different cache regardless of code.
- **A macOS cache cannot be read back.** `cSource::read` consumes all 100,627
  records cleanly (`error=0`, `where=47,837,501` of `67,355,747`) and then fails
  in the trailing sections — sentenceStarts / sections / speakerGroups / pema.
  The file is not truncated; 19.5 MB follows the records. Writing a cache the
  reader rejects is a bug in its own right, independent of Windows parity.

## How to reproduce

```sh
cd /Users/davidscott/lp/source
export LP_DB_PASSWORD=...            # from ~/.my.cnf
export WNSEARCHDIR=/Users/davidscott/lp/WordNet/2.1/dict
./build/bin/lp -book 2 3 -BC 0 -SW -forceSourceReread -retry
```

**Send the logs to /dev/null first.** `Secret Adversary.txt` carried 23 `~~` trace
directives immediately after `PROLOGUE`, and an unredirected run wrote 39 GB into
`main.lplog` in four minutes and kept growing. (They were changed to `~~~`, which
disables them, on 2026-09-07 — note that this also zeroes the trace bits that
`writeFlags` stores in every record, so a cache built with them off will differ
from the Windows reference, whose dominant per-record mask is `0x07ffff`.) `ln -s /dev/null main.lplog`
(and resolution, where, rescheck, …) discards the output without changing the
parse — the traces cannot simply be turned off, because `cWordMatch::writeFlags`
serializes them into the cache and they are part of the bytes being compared.

Compare with `scratchpad/cmp_cache.py`, which decodes both headers and reports
the first differing byte and its offset into the record stream.

## The pipeline beyond parsing: three external dependencies

The parse phase is solved (see above). Getting a *complete* cache — one with
speaker groups and the S2 sections, as the Windows reference has — needs three
things the macOS machine did not have. Found by bisecting `processSource`:

1. **A cache the reader accepts.** `cSource::write` gates
   `syntacticRelationGroups`/`timelineSegments` behind its `S2` argument, but
   `cSource::read` read them unconditionally, so any cache written by the
   `S2 == false` call at `main.cpp:1448` was rejected and the source silently
   reparsed. **Fixed** in `a8d135a`; the existing cache now loads in 3 seconds
   and `main.lplog` reports `already parsed.`
2. **`dbpedia_downloads/2016-10/dbpedia_2016-10.nt`.**
   `cOntology::readDbPediaOntology` (`createOntology.cpp:1140`) treats its
   absence as `LOG_FATAL_ERROR`, which exits the process *after* a successful
   parse. **Supplied by the author on 2026-09-07** (4.1 MB, narrow UTF-8, which
   matches the `"rt"` + `lp_fgetws` reader it is opened with).
3. **A local Virtuoso SPARQL endpoint on `localhost:8890`.** Still missing.
   `cInternet::readPage` retries a failed SPARQL query
   `internetWebSearchRetryAttempts` times, sleeping 30 s and printing
   `restart virtuoso` between attempts, then gives up — and
   `identifySpeakerGroups` dies. Nothing is listening on 8890 and Virtuoso is not
   installed. The Windows machine evidently had it running.

`caches/dbPediaCache` covers this query type — 6,509 cached
`DISAMBIGUATE from REDIRECT(RESOURCES)` results — so most entities never reach
the network. The run dies on a genuine miss: `:the_Daily_Mail`.

**A lead worth checking before installing anything.** The cache holds
`d/a/_Daily_Mail_cR1_TYPES*.html` — keyed on `Daily_Mail`, without the article —
while the failing query asks for `the_Daily_Mail`. If the Windows build derived
the key without the leading article, it would have hit the cache and never
needed the server. That would make this another port defect rather than a
missing dependency, and is cheaper to investigate than standing up Virtuoso.

## Status after the resolution-phase fixes (2026-09-07)

The whole pipeline now runs: parse, speaker identification, speaker resolution and
object resolution, exit 0, **zero AddressSanitizer errors**, 26 s in the release
build.

| | macOS | Windows |
|---|---|---|
| cache size | 70,009,972 | 70,569,669 |
| word matches | 100,646 | 100,657 |

That is a 0.8% size gap, against 67,349,081 bytes and a missing speaker/S2
section before this work.

**Correction to an earlier entry in this file.** The 30 trailing `in` tokens in
the Windows cache were recorded above as an artifact of the `bufferLen` overrun
fixed in `5b90003`. That was wrong. The parse ends correctly on both platforms at
`… said tommy . ||| ||| |||` (position 100,627); the trailing `in` tokens are
appended afterwards, by `cSource::transformQuestionRelation`
(`questionProcessing.cpp:1039`), which pushes one synthetic `in` per *where*/*when*
question relation so that a web query reads "X was born in". They are deliberate.

macOS now appends **19** of them where Windows appended **30**, and the word
sequences are otherwise identical. So the remaining word-level gap is 11
where/when question relations that macOS does not detect — a semantic difference
in `getQuestionTypeAndQuestionInformationSourceObjects` / `inObject`, not a memory
error and not tokenization.

### The four memory errors that had to be fixed first

Each was hidden behind the previous one; all four are undefined behaviour that
Windows tolerated.

| # | site | error |
|---|---|---|
| 1 | `setTimeFlowTense`, semanticRelations.cpp:446 | `m[maxWO]` read before the `maxWO >= 0` guard two lines below; both inputs are -1 when unset |
| 2 | `detectUnresolvableObjectsResolvableThroughSpeakerGroup`, identifySpeakerGroups.cpp:988 | `futureSpeakers.erase(*si)` frees the node `si` points at, then the next line dereferences `si` |
| 3 | `scanForSpeaker`, resolveSpeakers.cpp:5199 | `m[m[…].principalWherePosition]` with no guard on an inner value that is -1 when unset |
| 4 | `preferRelatedObjects`, resolveSpeakers.cpp:3534 | `objectMatches.clear()` then `objectMatches[offsetWithFrequency]` to build its replacement |

`scratchpad/asan_cycle.sh` runs one iteration (~60 s) and prints the error type,
location and top frames.

### External dependencies, all now satisfied

Virtuoso 7.2.17 (`brew install virtuoso`; the formula has no `brew services`
definition, so start `virtuoso-t +configfile virtuoso.ini` from
`/opt/homebrew/opt/virtuoso/var/lib/virtuoso/db`). `NumberOfBuffers` raised from
10,000 to 2,720,000 and the data directory added to `DirsAllowed`; the original
ini is saved beside it as `virtuoso.ini.orig`. The 12 available DBpedia 2016-10
files bulk-loaded in about 90 seconds — 59,433,866 triples. Runs now report zero
`cannot read URL` failures.

Three of the fifteen datasets the README lists are still absent:
`yago_types.ttl`, `instance_types_transitive_en.ttl`, `interlanguage_links_en.ttl`.

A separate defect worth chasing: the engine asks DBpedia for `the_Daily_Mail`
where the resource is `The_Daily_Mail`. DBpedia resource names are case-sensitive,
so the query returns empty; `The_Daily_Mail` does resolve, redirecting to
`Daily_Mail`. That is a key-construction bug, and it is why the cache under
`caches/dbPediaCache` was being missed for this entity.

## The DBpedia key casing: investigated, not a defect

Recorded above as a lead: the engine queried `the_Daily_Mail` where DBpedia holds
`The_Daily_Mail`. Chasing it showed there is no bug.

`cSource::getObjectString` (`getWikipedia.cpp:1150`) takes a
`removePrecedingUncapitalizedWordsFromProperNouns` flag, and the comment at line
1253 states the intended usage: call once with it false and once with it true,
"so that if there are no preceding uncapitalized words, nothing is done".
`questionAnswering.cpp` does exactly that — `processWikipedia(..., false)` at line
1869 and `(..., true)` at line 1884. So the lowercase form is the *first of two
deliberate attempts*, and an empty result from it is expected.

The runs confirm it: they created `dbPediaCache/T/h/_The_Daily_Mail.rdfTypes`,
capitalised, and the cache holds 159 legitimate `_the_*` entries from the other
arm of the pair. The original failure was solely the unreachable SPARQL server.

## The remaining 11 word-level differences

macOS appends 19 synthetic `in` tokens where Windows appended 30. Reading both
caches through the engine and dumping each `in` token's `relObject` identifies
them exactly:

- Windows-only (12): word positions 846, 4145, 6117, 12072, 18117, 24985, 37605,
  47804, 49747 (twice), 63647 (twice)
- macOS-only (1): 26461

Every one is a *where* question — "where shall we go?", "where shall we meet? and
when?", "where does he come in?", "but if so, where was the girl…".

The chain, traced by instrumenting the decision:

1. `transformQuestionRelation` appends the token only when
   `inQuestion && srg.whereVerb >= 0`, and then only for a where/when question
   whose object passes `inObject`.
2. For the missing cases `inQuestion` is **false**. For position 12072 the SRG is
   logged as `where=12075 inQ=0 verb=12073 subj=12075 obj=12072` — obj is exactly
   the object Windows transformed.
3. `inQuestion` is the OR of `flagInQuestion` on the SRG's subject and object.
4. That flag is set by the backward walk from a `?` in
   `identifyObjects.cpp:1996-2026`, which stops at a quote
   (`imtmp->word->second.query(quoteForm) < 0`) and flags a word only when
   `imtmp->getObject() != -1 || imtmp->queryWinnerForm(relativizerForm) != -1`.

So the divergence is upstream of the question logic: it is a difference in
**object identification** for words inside quoted questions, which decides whether
the backward walk flags them at all. Of the 27 where-questions macOS does
classify, only 12 also pass `inObject`, so that predicate is worth examining in
the same pass.

This is a semantic difference, not a memory error, and the numbers are stable, so
it can be attacked with a diff of object identification between the two caches
rather than by crash-chasing.
