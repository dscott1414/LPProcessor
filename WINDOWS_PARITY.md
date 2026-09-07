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
