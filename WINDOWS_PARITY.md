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

## Current state

| | macOS | Windows |
|---|---|---|
| size | 67,349,081 | 70,569,669 |
| `sourceVersion` | 9 | 8 |
| word matches | 100,633 | 100,657 |

`SOURCE_VERSION` was bumped 8→9 in the port commit `a6cd92c`. Since the format is
unchanged, that bump was defensive rather than required; it will need to go back
to 8 for byte-identity, but **not until the content matches**, or it would just
hide the real differences.

## First divergence

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
- **The 24 missing word matches.** macOS has 100,633 against Windows' 100,657.
  That divergence is somewhere after record 1 and has not been located yet.

## How to reproduce

```sh
cd /Users/davidscott/lp/source
export LP_DB_PASSWORD=...            # from ~/.my.cnf
export WNSEARCHDIR=/Users/davidscott/lp/WordNet/2.1/dict
./build/bin/lp -book 2 3 -BC 0 -SW -forceSourceReread -retry
```

**Send the logs to /dev/null first.** `Secret Adversary.txt` has 23 `~~` trace
directives immediately after `PROLOGUE`, and an unredirected run wrote 39 GB into
`main.lplog` in four minutes and kept growing. `ln -s /dev/null main.lplog`
(and resolution, where, rescheck, …) discards the output without changing the
parse — the traces cannot simply be turned off, because `cWordMatch::writeFlags`
serializes them into the cache and they are part of the bytes being compared.

Compare with `scratchpad/cmp_cache.py`, which decodes both headers and reports
the first differing byte and its offset into the record stream.
