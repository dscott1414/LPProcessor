# Proposed fixes — undefined behaviour and logic inversions

Concrete remediation for categories 2 (undefined behaviour / memory safety) and
3 (logic inversions) of `CODE_REVIEW.md`. Categories 1 (secrets), 4 (SQL/HTTP
concatenation) and 5 (process control) are out of scope here.

**Nothing in this document has been applied.** The branch remains
comments-only. Each entry gives the verified current code and a minimal patch,
so a maintainer with a build can apply them in the stated order.

The project cannot be compiled in this environment (MSVC, `windows.h`, MySQL),
so every claim below is justified by reading the code. Where a claim could be
checked mechanically, the logic was extracted into a standalone harness and
run; those cases are marked **verified by execution** and include the output.

## How to read this

Each entry carries a verdict and a risk label.

| Verdict | Meaning |
| --- | --- |
| CONFIRMED | The defect is real and reachable. |
| CONFIRMED-LATENT | The code is wrong but no current caller reaches the bad path. |
| FALSE POSITIVE | The original review finding was wrong. |
| UNCERTAIN | Needs author intent to settle. |

| Risk | Meaning |
| --- | --- |
| Safe | Cannot change output on any input. |
| Behaviour-changing (intended) | The parser will take a path it never took before. Watch the named trace. |
| Needs author decision | Correct behaviour is ambiguous. |

Ordering is `DO-FIRST` / `NORMAL` / `DEFER` from the point of view of getting
correctness wins with the least risk.

---

## Correction to CODE_REVIEW.md: the stack-buffer findings were overstated

`CODE_REVIEW.md` claims several multi-megabyte stack arrays "will overflow as
soon as [the function] runs", reasoning from MSVC's 1 MB default. That default
does not apply here. Both project files set an explicit stack:

```1:1:lp.vcxproj
      <StackReserveSize>21097152</StackReserveSize>
      <StackCommitSize>21097152</StackCommitSize>
```

`specials.vcxproj` sets the same values. 21,097,152 bytes is 20.12 MiB, and it
looks deliberately chosen: the largest buffer in the tree is 20,000,000 bytes
(19.07 MiB), leaving ~1.05 MB of headroom. `Internet.cpp:459` is the only
`CreateThread` and passes 0 for the stack size, so worker threads inherit the
same reserve from the PE header.

So the accurate finding is not "guaranteed overflow" but:

1. These functions only work because of a non-obvious, non-default linker
   setting. Nothing in the source explains the dependency.
2. The largest two leave about 1 MB for the entire rest of the call chain.
3. `StackCommitSize` equals `StackReserveSize`, so the full 20 MB is *committed*
   per thread rather than grown on demand. In `-mp` mode with N child processes
   that is N × 20 MB of committed stack, plus 20 MB for the WinINet reader
   thread in each.

Moving the two 20 MB buffers to the heap would let the stack reservation drop
back toward the default and reclaim that memory. That is the real payoff — not
crash avoidance.

### Full inventory of oversized stack arrays

Repo-wide sweep for stack arrays sized by a `MAX_BUF`-family macro, with the
macro resolved. The review found five; there are thirteen.

| Location | Declaration | Size | In review? |
| --- | --- | --- | --- |
| `createOntology.cpp:2235` | `char buffer[MAX_BUF * 10]` | 19.07 MiB | yes |
| `getWikipedia.cpp:793` | `char buffer[EMAX_BUF]` | 19.07 MiB | yes |
| `source.cpp:2302` | `char buffer[MAX_BUF]` | 10 MiB | **no** |
| `source.cpp:2349` | `char buffer[MAX_BUF]` | 10 MiB | **no** |
| `source.cpp:3559` | `char buffer[MAX_BUF]` | 10 MiB | **no** |
| `getWordNetMaps.cpp:151` | `char buffer[MAX_BUF]` | 9.77 MiB | yes |
| `createOntology.cpp:1045` | `char fileBuffer[MAXYAGOBUF + 1]` | 4.77 MiB | yes |
| `createOntology.cpp:328` | `wchar_t buffer[MAX_BUF]` | 3.81 MiB | yes |
| `getWikipedia.cpp:535` | `wchar_t cBuffer[MAX_BUF]` | 3.81 MiB | **no** |
| `getMusicBrainz.cpp:74` | `wchar_t cBuffer[MAX_BUF]` | 2 MiB | **no** |
| `createOntology.cpp:1386` | `char buffer[MAX_BUF]` | 1.91 MiB | **no** |
| `getWikipedia.cpp:1397` | `char buffer[MAX_BUF]` | 1.91 MiB | **no** |
| `getWikipedia.cpp:1409` | `char buffer[MAX_BUF]` | 1.91 MiB | **no** |

`checkTypes/checkTypes.cpp:107` declares `char buffer[MAX_BUF_LEN]` (10 MB) at
file scope, so it is static storage, not stack — not a defect.

The two 19 MiB buffers are in separate call trees
(`cOntology::getRDFTypesMaster` → `writeRDFTypes`, and
`cSource::getExtendedRDFTypesMaster` → `writeExtendedRDFTypes`), so they do not
nest today. Two of them on one stack would exceed the reservation.

**Verdict:** CONFIRMED-LATENT (fragility and committed-memory cost, not a
crash). **Risk:** Safe. **Ordering:** NORMAL — do the two 19 MiB ones and then
lower the linker reservation; the rest are cleanup.

Recommended shape for each, using the tracked allocator already in the file so
the memory shows up in `memoryAllocated`:

```
--- a/createOntology.cpp   (line 2235, cOntology::writeRDFTypes)
- 	char buffer[MAX_BUF * 10];
+ 	char *buffer = (char *)tmalloc(MAX_BUF * 10);
+ 	if (!buffer) return -1;
```

with a matching `tfree(MAX_BUF * 10, buffer)` on **every** return path. Audit
each `return` in the function before applying; `writeRDFTypes` has several.

---

## 1. `intArray.h` — `decode()` shifts by a negative count

**Verdict:** CONFIRMED, **verified by execution**. **Risk:** Safe.
**Ordering:** DO-FIRST.

```345:359:intArray.h
	void decode(unsigned int val)
	{
		int lastNonZero = 0;
		for (int tc = 0, bitFieldCount = TOTAL_BITS; bitFieldCount >= 0; bitFieldCount -= BITS_PER_RULE, tc++)
		{
			assign(tc, (val >> (bitFieldCount - BITS_PER_RULE)) & ((1 << BITS_PER_RULE) - 1));
```

`TOTAL_BITS` is 30 and `BITS_PER_RULE` is 10, so `bitFieldCount` runs
30, 20, 10, 0 and the shift is 20, 10, 0, **−10**. `encode()` packs only three
fields, so the fourth iteration is spurious as well as undefined.

This is not merely theoretical. On x86 a negative shift count is masked to
`-10 & 31 == 22`, so the fourth field decodes as `val >> 22`, which is usually
nonzero — and because it is nonzero it also sets `lastNonZero`, so `count`
comes back as 4 instead of 3. `encode`/`decode` is therefore not round-trip
safe today. Extracting both functions verbatim into a standalone harness:

```
input            encoded      decode_current               decode_fixed
{   1,   2,   3} 0x00100803  count=3 [1,2,3,0]            count=3 [1,2,3,0]
{   5,   0,   0} 0x00500000  count=4 [5,0,0,1]            count=1 [5,0,0,0]
{1023,   1,   7} 0x3ff00407  count=4 [-1,1,7,255]         count=3 [-1,1,7,0]
{ 100, 200, 300} 0x0643212c  count=4 [100,200,300,25]     count=3 [100,200,300,0]
{   0,   0,   1} 0x00000001  count=3 [0,0,1,0]            count=3 [0,0,1,0]
```

Fix — stop before the shift goes negative:

```
--- a/intArray.h   (line 348, cIntArray::decode)
- 		for (int tc = 0, bitFieldCount = TOTAL_BITS; bitFieldCount >= 0; bitFieldCount -= BITS_PER_RULE, tc++)
+ 		for (int tc = 0, bitFieldCount = TOTAL_BITS; bitFieldCount >= BITS_PER_RULE; bitFieldCount -= BITS_PER_RULE, tc++)
```

Callers that consumed the phantom fourth element will now see `count` one
lower on those inputs. That is the correction, not a regression: `encode()`
never stored a fourth field.

---

## 2. `pattern.h` — usage counters are `reserve()`d but indexed

**Verdict:** CONFIRMED. **Risk:** Safe for the parse; changes the
`.patternUsage` side-file. **Ordering:** DO-FIRST.

```181:187:pattern.h
		void initializeUsage()
		{
			usageFormFinalMatch.reserve(formIndexes.size());
			usageFormEverMatched.reserve(formIndexes.size());
			usagePatternFinalMatch.reserve(patternIndexes.size());
			usagePatternEverMatched.reserve(patternIndexes.size());
		}
```

`reserve` sets capacity, not `size()`, so all four vectors stay `size() == 0`.
They are then indexed on the hot path — `cPattern::incrementUse`
(`pattern.h:500`/`502`, called from `PEMA::consolidateWinners`) and
`fillPattern` (`pattern.cpp:829`/`831`) both do `v[index]++`.

I searched every reader and writer of the four vectors. Nothing ever resizes
them:

- `cPatternElement::zeroUsage` (`pattern.cpp:2120`) uses
  `std::fill(v.begin(), v.end(), 0)` — a no-op on an empty vector.
- `cPatternElement::copyUsage` (`pattern.cpp:2110`) serializes them, so it
  writes four **zero-length** arrays.
- `printPatternStatistics` (`pattern.cpp:2137`/`2144`) reads `v[J]`, i.e. also
  out of bounds.

Because `reserve` really did allocate the capacity and nothing ever
`push_back`s (so no reallocation moves the buffer), the increments land in
allocated-but-unconstructed storage and the program "works". The observable
consequence is that `cSource::writePatternUsage` (`source.cpp:2293`) writes a
`.patternUsage` file containing no counts at all, and `zeroUsage` never resets
anything.

Fix:

```
--- a/pattern.h   (line 183, cPatternElement::initializeUsage)
- 			usageFormFinalMatch.reserve(formIndexes.size());
- 			usageFormEverMatched.reserve(formIndexes.size());
- 			usagePatternFinalMatch.reserve(patternIndexes.size());
- 			usagePatternEverMatched.reserve(patternIndexes.size());
+ 			usageFormFinalMatch.assign(formIndexes.size(), 0);
+ 			usageFormEverMatched.assign(formIndexes.size(), 0);
+ 			usagePatternFinalMatch.assign(patternIndexes.size(), 0);
+ 			usagePatternEverMatched.assign(patternIndexes.size(), 0);
```

`assign` rather than `resize` because these are counters that must start at
zero and `initializeUsage` may be called on a reused object.

Call ordering is already correct: `cPattern::initializeUsage`
(`pattern.cpp:2297`) is driven from `pattern.cpp:2385`, after `create()` has
resolved `patternReferences`, so `patternIndexes.size()` is final.

Also update the two comments added by the annotation pass that document the
old behaviour (`pattern.h:177-180`, `pattern.h:494-496`, `pattern.cpp:46`,
`pattern.cpp:1680`).

**After this fix `.patternUsage` files change size and layout.** There is no
reader of that file in the tree, so nothing breaks, but previously written
files become inconsistent with new ones — delete them when deploying.

---

## 3. `agreement.cpp` — `minSeparatorCost` is `reserve()`d but indexed

**Verdict:** CONFIRMED. **Risk:** Safe. **Ordering:** DO-FIRST.

```4342:4344:agreement.cpp
	minSeparatorCost.reserve(end - begin + 1); // reserve does not grow size; [I] is out-of-range
	for (unsigned int I = 0; I < end - begin; I++)
		minSeparatorCost[I] = m[begin + I].word->second.lowestSeparatorCost();
```

Same defect as above, on the winnow path that runs for every sentence. The
slots are written immediately after, and read as `minSeparatorCost[bp - begin]`
at lines 333, 4098, 4207 and 4273 — all using the same `- begin` convention, so
sizing the vector once is sufficient.

Fix:

```
--- a/agreement.cpp   (line 4342, cSource::eliminateLoserPatterns)
- 	minSeparatorCost.reserve(end - begin + 1); // reserve does not grow size; [I] is out-of-range
+ 	minSeparatorCost.resize(end - begin + 1);
```

`resize` is right here (not `assign`) because the loop immediately overwrites
every element it will read. Note the vector is sized `end - begin + 1` while
the loop fills `end - begin`; the extra trailing slot is never read by the
four call sites above, but with `resize` it is at least value-initialized
rather than indeterminate.

Drop the trailing comment from the annotation pass when applying.

---

## 4. `timeRelations.cpp` — `months_abb` is not index-parallel to `months`

**Verdict:** CONFIRMED, **verified by execution**. **Risk:**
Behaviour-changing (intended). **Ordering:** DO-FIRST.

```2244:2261:timeRelations.cpp
Inflections months[] = { {L"january",SINGULAR},{L"february",SINGULAR},{L"march",SINGULAR},{L"april",SINGULAR},{L"may",SINGULAR},
{L"june",SINGULAR},{L"july",SINGULAR},{L"august",SINGULAR},{L"september",SINGULAR},{L"october",SINGULAR},
{L"november",SINGULAR},{L"december",SINGULAR},{NULL,0} };
const wchar_t* months_abb[] = { L"jan",L"feb",L"mar",L"apr",L"aug",L"sept",L"oct",L"nov",L"dec",NULL };
int whichMonth(wstring w)
{
	LFS
		for (int I = 0; months[I].inflection; I++)
			if (w == months[I].word)
				return I;
	for (int I = 0; months_abb[I]; I++)
		if (w == months_abb[I])
			return I;
	return -1;
}
```

`whichMonth` returns the abbreviation's position in `months_abb`, but callers
treat the result as a `months[]` index (`t.absMonth`, set at
`timeRelations.cpp:484`, `488`, `864`, `909`). Because `may`, `jun` and `jul`
are missing from the abbreviation table, everything from `aug` onward is off by
three. Running the real tables through the real lookup:

```
input    cur    cur means      fix    fix means
aug      4      may            7      august           <-- CHANGED
sept     5      june           8      september        <-- CHANGED
oct      6      july           9      october          <-- CHANGED
nov      7      august         10     november         <-- CHANGED
dec      8      september      11     december         <-- CHANGED
jun      -1     (not a month)  -1     (not a month)
jul      -1     (not a month)  -1     (not a month)
sep      -1     (not a month)  -1     (not a month)
```

Five of the nine abbreviations resolve to the wrong month; `jun`, `jul` and
`sep` are not recognised at all. `daysOfWeek_abb` **is** index-parallel to
`daysOfWeek`, which is good evidence that parallelism was the intent here too.

There is a complication that rules out the obvious fix. `months_abb` is not
only a lookup table — it is also registered in the lexicon:

```2394:2397:timeRelations.cpp
	predefineWords(months, L"month", L"month", L"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, months);
	predefineWords(months_abb, L"month", L"month", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, months_abb);
```

Note the abbreviation call uses a different `predefineWords` overload than
`months` (no `L"noun"` argument). Simply adding `L"may"`, `L"jun"`, `L"jul"` to
the array would therefore also register **"may" as a month word through a
different code path than the one that already registers it** — and "may" is a
very common modal verb. That is a lexicon change with parser-wide blast radius,
smuggled in under a date-handling fix.

**Recommended fix — leave the lexicon alone, correct only the mapping:**

```
--- a/timeRelations.cpp   (line 2248)
  const wchar_t* months_abb[] = { L"jan",L"feb",L"mar",L"apr",L"aug",L"sept",L"oct",L"nov",L"dec",NULL };
+ // months[] index for each months_abb[] entry; may/jun/jul have no abbreviation here.
+ static const int months_abb_index[] = { 0, 1, 2, 3, 7, 8, 9, 10, 11 };

--- a/timeRelations.cpp   (line 2257, whichMonth)
  	for (int I = 0; months_abb[I]; I++)
  		if (w == months_abb[I])
- 			return I;
+ 			return months_abb_index[I];
```

This is the whole correctness fix, with zero lexicon impact. Keep the two
arrays adjacent and comment that they must stay the same length.

Recognising `jun`, `jul` and `sep` is a **separate, optional** change: add them
to `months_abb` *and* `months_abb_index`, and decide deliberately whether
`may` should be registered as a month abbreviation (recommendation: no).

What to watch after applying: any log line printing `absMonth`, and dates in
`timeRelations` traces for texts using abbreviated months. Events dated
August–December were previously being placed three to five months earlier, so
temporal ordering for those documents changes.

---

## Remaining areas

Verification and patches for the rest of categories 2 and 3 — the pattern
engine and agreement costing, object and speaker resolution, names and
temporal/syntactic relations, the infra headers and ontology/HMM, and the
QA/acquisition paths — are being prepared and will be appended here.
