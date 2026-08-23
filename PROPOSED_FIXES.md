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

Line numbers were re-derived against `master` after the annotation PR merged
(that merge also re-encoded some high-bit comment characters and restored a
block comment in `pattern.h`, which shifted a few lines).

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

```313:314:lp.vcxproj
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
(`pattern.h:501`/`503`, called from `PEMA::consolidateWinners`) and
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

Also update the comments added by the annotation pass that document the old
behaviour (`pattern.h:166`, `pattern.h:178`, `pattern.h:497`, `pattern.cpp:46`,
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

## Pattern engine

### 5. `pattern.cpp:1807` `setMandatoryAncestorPatterns` ORs the wrong bitset

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** DO-FIRST.

The already-computed branch of `setAncestorPatterns` ORs into `ancestorPatterns`.
The mandatory walk copies the same structure but writes the *other* bitset:

```1806:1808:pattern.cpp
				if (patterns[p]->mandatoryAncestorsSet)
					ancestorPatterns |= patterns[p]->mandatoryAncestorPatterns;
				else
```

The only later test is `mandatoryAncestorPatterns.isSet(p)` (`pattern.cpp:2289`).
Grandparents of a mandatory parent are therefore never visible, and
mandatory-ancestor queries silently drop coverage. Init order at
`pattern.cpp:2366-2369` computes ancestors first, then mandatory, so this is
not rescued by the other walk.

```
--- a/pattern.cpp
 				if (patterns[p]->mandatoryAncestorsSet)
-					ancestorPatterns |= patterns[p]->mandatoryAncestorPatterns;
+					mandatoryAncestorPatterns |= patterns[p]->mandatoryAncestorPatterns;
 				else
```

Watch `mandatoryAncestorPatterns` traces after applying. Update the annotation
at `pattern.cpp:1797-1798` once the source is patched.

### 6. `definePatterns.cpp:2824` `{HAIL|OBJECT}` is one unused tag

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

`processForm` (`pattern.cpp:971-990`) splits `{...}` tags on `:` and `}`. The
alternative `_HAIL_OBJECT{HAIL|OBJECT}` therefore interns one unused tag
`"HAIL|OBJECT"` and never attaches `OBJECT`. Siblings in the same call use
`{HON:HAIL}`. `"HAIL|OBJECT"` has no other in-tree mention.

```
--- a/definePatterns.cpp
-		7, L"_NAME{HAIL}", ..., L"_HAIL_OBJECT{HAIL|OBJECT}", ...
+		7, L"_NAME{HAIL}", ..., L"_HAIL_OBJECT{HAIL:OBJECT}", ...
```

Hail closing patterns (`__CLOSING__S1` alternative 1) start seeing the `OBJECT`
tag. Watch hail / vocative traces.

### 7. `patternMatchArray.h:152` `queryTag` breaks on the first hit

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing if a caller uses the
returned slot rather than existence. **Order:** NORMAL.

The loop tracks `maxLen` then `break`s on the first `hasTag` match, so the
length walk is dead. Most in-tree callers only test `>= 0`
(`identifyObjects.cpp:157`). `timeRelations.cpp:1104` / `:1839` and
`getWikipedia.cpp:1333` use the returned PMA slot.

```
--- a/patternMatchArray.h   (queryTag)
 				gElement=PMAElement;
 				maxLen=content[PMAElement].len;
-				break;
 			}
```

If the author intended “first” rather than “longest”, delete `maxLen` instead
and keep the `break`. The header comment already documents the mismatch.

### 8. `pattern.cpp:945` `processForm` writes through `form.c_str()`

**Verdict:** CONFIRMED. **Risk:** Safe (create-time only; still UB). **Order:** NORMAL.

```944:947:pattern.cpp
		wchar_t saveEnd = *ch;
		*((wchar_t*)ch) = 0;
		specificWord = sword;
		*((wchar_t*)ch) = saveEnd;
```

`ch` is `form.c_str()`. C++ does not allow writing through that pointer.

```
--- a/pattern.cpp
-		wchar_t saveEnd = *ch;
-		*((wchar_t*)ch) = 0;
-		specificWord = sword;
-		*((wchar_t*)ch) = saveEnd;
+		specificWord.assign(sword, ch - sword);
```

### 9. `patternMatchArray.cpp:65` `clear()` does not NULL `content`

**Verdict:** CONFIRMED-LATENT. **Risk:** Safe. **Order:** NORMAL.

`tfree` then `allocated = 0`, but `content` is left dangling. The destructor
and PEMA `clear` both NULL it. The only in-tree caller is
`cSource::clearSource`, which then overwrites `m`. A later `push_back` would
`trealloc` the freed pointer.

```
--- a/patternMatchArray.cpp
 	allocated = 0;
+	content = NULL;
```

### 10. `patternMatchArray.cpp:116` `read()` bounds-checks the stale `count`

**Verdict:** CONFIRMED. **Risk:** Safe. **Order:** NORMAL.

The first check uses the *pre-read* `count` (usually 0) before
`copy(count, ...)`. A corrupt image therefore passes the guard and is only
caught by FATAL after the memcpy size is known.

```
--- a/patternMatchArray.cpp
-	if (where + sizeof(count) + count * sizeof(*content) > limit) return false;
-	if (!copy(count, buffer, where, limit)) return false;
-	allocated = count;
+	unsigned int newCount = 0;
+	int peek = where;
+	if (!copy(newCount, buffer, peek, limit)) return false;
+	if (peek + newCount * sizeof(*content) > limit) return false;
+	where = peek;
+	count = newCount;
+	allocated = count;
```

(Keep the existing FATAL as a belt-and-braces check after the assign, or drop
it once the pre-check is real.)

### 11. PMA / PEMA `operator=` self-assignment

**Verdict:** CONFIRMED. **Risk:** Safe (UAF only on `x = x`). **Order:** NORMAL.

Both `patternMatchArray.cpp:158` and `patternElementMatchArray.cpp:207` free
`content` then copy from `rhs`. `pma = pma` is use-after-free.

```
--- a/patternMatchArray.cpp   (same in patternElementMatchArray.cpp)
 cPatternMatchArray& cPatternMatchArray::operator=(const cPatternMatchArray& rhs)
 {
 	LFS
+	if (this == &rhs) return *this;
 	if (allocated) tfree(allocated * sizeof(*content), content);
```

### 12. `patternMatchArray.cpp:599` `1 << 31` signed overflow

**Verdict:** CONFIRMED. **Risk:** Safe on MSVC (the shift is well-known there)
but undefined in the language. **Order:** NORMAL.

```
--- a/patternMatchArray.cpp
-	int minPatternMatch = 1 << 31;
+	int minPatternMatch = INT_MIN;
```

Use the same sentinel in the comparison at `:603`. Include `<climits>` if the
TU does not already.

### 13. `patternMatchArray.cpp:414` `queryPattern(int, int& len)` does not init `len`

**Verdict:** CONFIRMED-LATENT. **Risk:** Safe for current callers. **Order:** NORMAL.

The wstring overload starts `maxLen = -1`. The int overload compares against
the caller’s `len`. No in-tree caller uses this overload (live sites go through
the wstring form). Still initialize, because leftover `len` would drop matches.

```
--- a/patternMatchArray.cpp
 int cPatternMatchArray::queryPattern(int pattern, int& len)
 {
 	LFS
+	len = -1;
 	int element = -1;
```

### 14. `patternMatchArray.cpp:440` `queryTagSet` can index `patternTagStrings[-1]`

**Verdict:** CONFIRMED. **Risk:** Safe if `hasTagInSet` never returns −1 when
`tagSetMemberInclusion` is set; otherwise UB. **Order:** NORMAL.

`tag` starts at −1. A later equal-length candidate does
`patternTagStrings[tag] == L"NAME"` before updating `tag`.

```
--- a/patternMatchArray.cpp
-			if (content[I].len == maxLen && patternTagStrings[tag] == L"NAME") continue;
+			if (content[I].len == maxLen && tag >= 0 && patternTagStrings[tag] == L"NAME") continue;
```

### 15. `patternElementMatchArray.cpp:600` `generatePEMACount` off-by-one

**Verdict:** CONFIRMED. **Risk:** Safe (turns a later OOB into FATAL). **Order:** NORMAL.

```
--- a/patternElementMatchArray.cpp
-		if (nextPosition > (signed)count)
+		if (nextPosition < 0 || nextPosition >= (signed)count)
```

(`nextPosition == -1` already ends the loop, so the `< 0` is optional.)

### 16. `patternElementMatchArray.cpp:145` format string missing `count`

**Verdict:** CONFIRMED. **Risk:** Safe (logging UB on a corrupt cache). **Order:** NORMAL.

```
--- a/patternElementMatchArray.cpp
-		lplog(LOG_ERROR, L"Illegal count of %d (>1000000) encountered!");
+		lplog(LOG_ERROR, L"Illegal count of %d (>1000000) encountered!", count);
```

---

## Agreement costing

### 17. `agreement.cpp:1376` `reduceCostIfRestate` takes `relationCost` by value

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** DO-FIRST.

The helper divides `relationCost` by subject length when the subject is a
restated object (`RE_OBJECT`). The caller at `:1478` then uses the original.
RE_OBJECT subjects therefore keep the full S/V cost.

```
--- a/agreement.cpp
-void cSource::reduceCostIfRestate(bool restateSet, int relationCost, int subjectTag, vector<cTagLocation>& tagSet)
+void cSource::reduceCostIfRestate(bool restateSet, int& relationCost, int subjectTag, vector<cTagLocation>& tagSet)
```

Update the declaration in `source.h` to match. Watch subject-verb traces for
restated objects.

### 18. `agreement.cpp:651` `markChildren` skips `allLocations[0]` after reassess

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** DO-FIRST.

On `localReassessParentCosts` the code resets `lc = 0`, then the `for`
increment does `lc++`, so slot 0 is never re-examined. A child that became a
winner only after the cost drop can stay rejected.

```
--- a/agreement.cpp
-						lc = 0; // for-loop then does lc++, so allLocations[0] is skipped
+						lc = (unsigned)-1; // for-loop increments to 0
```

A `while` loop that sets `lc = 0` and `continue`s is equivalent and easier to
read. Watch `tracePatternElimination` for the “WINNER REVERSED” path.

### 19. `agreement.cpp:1114` `substitutePrepObjectSomeOf` skips N_AGREE at index 0

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

The sibling NOUN find uses `>= 0`. N_AGREE uses `> 0`, so a tag at index 0 is
ignored. “some/any/none/all/most of NP” then fails to pick up the noun’s
number when N_AGREE is the first tag in the set.

```
--- a/agreement.cpp
-								if ((nounTag = findTag(ndTagSets[K], L"NOUN", nextNounTag)) >= 0 && (nAgreeTag = findTagConstrained(ndTagSets[K], L"N_AGREE", nextNAgreeTag, ndTagSets[K][nounTag])) > 0)
+								if ((nounTag = findTag(ndTagSets[K], L"NOUN", nextNounTag)) >= 0 && (nAgreeTag = findTagConstrained(ndTagSets[K], L"N_AGREE", nextNAgreeTag, ndTagSets[K][nounTag])) >= 0)
```

### 20. `agreement.cpp:2911` last-noun frequency bias is dead

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

```2909:2912:agreement.cpp
	if (numBeginRelations > 0 && numBeginFrequency > 0 && numLastFrequency / numBeginFrequency > 1)
		numBeginRelations = (numBeginRelations * numLastFrequency) / numBeginFrequency;
	else if (numLastRelations > 0 && numLastFrequency > 0 && numBeginFrequency / numLastFrequency < 1)
		numLastRelations = (numLastRelations * numBeginFrequency) / numLastFrequency;
```

Both tests are integer division. `a / b < 1` is never true when both are `> 0`
and the first branch already covers `numLastFrequency / numBeginFrequency > 1`
(the exact complement). The last-noun frequency bias never runs.

```
--- a/agreement.cpp
-	else if (numLastRelations > 0 && numLastFrequency > 0 && numBeginFrequency / numLastFrequency < 1)
+	else if (numLastRelations > 0 && numLastFrequency > 0 && numBeginFrequency < numLastFrequency)
```

(Equivalently, compare in `double`.) Watch `longSubjectBindingMismatch` for
subjects whose last noun is more frequent than the first.

### 21. `agreement.cpp:1618` / `:3949` BNC helpers bound-check PEMA, then index `m[]`

**Verdict:** CONFIRMED. **Risk:** Safe. **Order:** NORMAL.

`BNCPatternViolation` and `evaluateBNCPreferences` guard
`begin+position` / `end+position` against `pema.count`, then walk `m[I]`.

```
--- a/agreement.cpp   (both sites)
-		pema[PEMAPosition].begin + position >= (int)pema.count ||
-		pema[PEMAPosition].end + position >= (int)pema.count)
+		pema[PEMAPosition].begin + position >= (int)m.size() ||
+		pema[PEMAPosition].end + position > (int)m.size())
```

(`end` is exclusive in the `for`, so `>` not `>=` on the end test.)

### 22. `agreement.cpp:2093` / `:2151` / `:2354` unbound `m[where+1]`

**Verdict:** CONFIRMED. **Risk:** Safe. **Order:** NORMAL.

Sentence-final `her own` / `her best`, `his`/`her` as a prep object, and
verb-object adverb checks all read `m[n+1]` with no size test.

```
--- a/agreement.cpp:2093
-					if (numTagSets==0 && pema[nPEMAPosition].end == 1 && m[nPosition].word->first == L"her" && (m[nPosition + 1].word->first == L"own" || m[nPosition + 1].word->first == L"best"))
+					if (numTagSets==0 && pema[nPEMAPosition].end == 1 && m[nPosition].word->first == L"her" && nPosition + 1 < (int)m.size() && (m[nPosition + 1].word->first == L"own" || m[nPosition + 1].word->first == L"best"))

--- a/agreement.cpp:2150
+					nPosition already: prepObjectPosition + 1 < (int)m.size() &&
 					(nfindex = m[prepObjectPosition + 1].word->second.query(nounForm)) >= 0 &&

--- a/agreement.cpp:2354
+		if (whereVerb + 1 < (int)m.size()) before reading m[whereVerb + 1]
```

The `:2354` block also reads `m[whereVerb + 2]` — require `+ 2 < m.size()`
there as well.

### 23. `agreement.cpp:2136` `pma.find` can be nullptr; `setSecondaryCosts` dereferences it

**Verdict:** CONFIRMED. **Risk:** Safe. **Order:** NORMAL.

`evaluatePrepObjects` does not test `pm`. `setSecondaryCosts` then calls
`cascadeUpToAllParents(..., pm, ...)`. The noun-determiner path at `:2103`
already skips on nullptr.

```
--- a/agreement.cpp:2164
 		if (costs.size())
 		{
+			if (!pm) continue;
 			lowerPreviousElementCosts(...);
 			setSecondaryCosts(secondaryPEMAPositions, pm, position, false, L"prepObjects");
```

(Or return before the cost loop when `!pm`.)

### 24. `agreement.cpp:1367` `disagreementWithAmbiguousTense` indexes `tagSet[-1]`

**Verdict:** CONFIRMED. **Risk:** Safe. **Order:** NORMAL.

`verbAgreeTag` is checked `>= 0`. `mainVerbTag` is not. The question path via
`agreeVerbNotFoundOrQuestion` can leave it −1.

```
--- a/agreement.cpp
-	if (!agree && ambiguousTense && verbAgreeTag >= 0 && conditionalTag < 0 &&
+	if (!agree && ambiguousTense && verbAgreeTag >= 0 && mainVerbTag >= 0 && conditionalTag < 0 &&
```

### 25. `agreement.cpp:465` `compareCost` multiplies three `int`s

**Verdict:** CONFIRMED. **Risk:** Safe for typical spans; UB on a long
document. **Order:** NORMAL.

`AC2 * LEN2 * LEN2 >= AC1 * LEN1 * LEN1` overflows signed 32-bit when a span
is more than a few thousand tokens.

```
--- a/agreement.cpp
-		if (setInternal = (AC2 * LEN2 * LEN2 >= AC1 * LEN1 * LEN1))
+		if (setInternal = ((int64_t)AC2 * LEN2 * LEN2 >= (int64_t)AC1 * LEN1 * LEN1))
```

### 26. `agreement.cpp:735` `getAllLocations` returns `minCost` as `unsigned int`

**Verdict:** CONFIRMED as a type error; **latent on MSVC**. **Risk:** Safe on
the target compiler. **Order:** DEFER.

The only caller assigns the result to `int lowestCost` (`:610`). A commented
check at `:612` acknowledges that PMA costs can be negative. On MSVC two’s
complement the wrap-and-assign-back recovers the bit pattern, so winners do
not currently change. Still change the signature (and the declaration in
`source.h`) to `int` so a different compiler cannot promote a negative cost
into a huge unsigned comparison.

---

## Objects and speakers

### 27. `resolveObjects.cpp:732` `containingSpeakerGroup` compares the span to the loop index

**Verdict:** CONFIRMED. **Risk:** Needs author decision. **Order:** DEFER
until the occupation-scan filter is intentionally armed.

```727:735:resolveObjects.cpp
vector <cSource::cSpeakerGroup>::iterator cSource::containingSpeakerGroup()
{
	LFS
		for (int I = 0; I < (signed)speakerGroups.size(); I++)
			if (speakerGroups[I].sgBegin >= I && speakerGroups[I].sgEnd < I)
				return speakerGroups.begin() + I;
	return speakerGroups.end();
}
```

Two independent defects:

1. `I` is the group index, not a source position. `sgBegin`/`sgEnd` are
   source positions, so the body never succeeds and this always returns
   `end()`. The declaration in `source.h:3072` takes no `where`.
2. Even if `I` were a position, `sgBegin >= I && sgEnd < I` is an empty
   interval whenever `begin < end`. The intended test is
   `sgBegin <= where && where < sgEnd`.

The only caller is the occupation-scan filter in
`resolveOccRoleActivityObject` (`:884`) plus the log at `:892`. **Fixing this
arms a filter that has never run.** Distant “the doctor” / “the nurse”
matches that currently succeed would start being rejected when the two
speaker groups do not share a speaker.

Recommended shape, only after the author confirms the filter should exist:

```
vector <cSpeakerGroup>::iterator cSource::containingSpeakerGroup(int where)
{
	LFS
		for (int I = 0; I < (signed)speakerGroups.size(); I++)
			if (speakerGroups[I].sgBegin <= where && where < speakerGroups[I].sgEnd)
				return speakerGroups.begin() + I;
	return speakerGroups.end();
}
```

Pass `where` (and, at the log site, `oi->originalLocation` or `where` —
decide which position the overlap is supposed to test) from both call sites.
Update `source.h:3072`.

### 28. `identifySpeakerGroups.cpp:2934` `sameSpeaker` one-sided tests are inverted

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** DO-FIRST.

```2931:2934:identifySpeakerGroups.cpp
	if (m[sWhere1].objectMatches.empty())
		return in(m[sWhere1].getObject(), m[sWhere2].objectMatches) == m[sWhere2].objectMatches.end();
	if (m[sWhere2].objectMatches.empty())
		return in(m[sWhere2].getObject(), m[sWhere1].objectMatches) == m[sWhere1].objectMatches.end();
```

Both-empty and both-nonempty treat overlap as “same”. The one-sided arms
return true when the object is *not* in the other list. Used by
`embeddedStory` to decide whether a continuation quote belongs to the same
teller.

```
--- a/identifySpeakerGroups.cpp
-		return in(m[sWhere1].getObject(), m[sWhere2].objectMatches) == m[sWhere2].objectMatches.end();
+		return in(m[sWhere1].getObject(), m[sWhere2].objectMatches) != m[sWhere2].objectMatches.end();
-		return in(m[sWhere2].getObject(), m[sWhere1].objectMatches) == m[sWhere1].objectMatches.end();
+		return in(m[sWhere2].getObject(), m[sWhere1].objectMatches) != m[sWhere1].objectMatches.end();
```

Watch embedded-story continuation (a 1st/2nd-person story told across several
quotes). Quotes that were previously treated as the same speaker when they
were *not* will now split; quotes that were split when they *were* the same
will now continue.

### 29. `timeRelations.cpp:2832` `speakerGroupTransition` walks `I++` instead of `I--`

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** DO-FIRST.

```2832:2834:timeRelations.cpp
		for (int I = sg - 1; I >= 0 && lastSG < 0; I++)
			if (speakerGroups[I].speakers.find(*si) != speakerGroups[I].speakers.end())
				lastSG = I;
```

The loop is supposed to walk *prior* groups. `I++` steps into the current
group (`I == sg`) on the second iteration. `*si` is taken from
`speakerGroups[sg].speakers`, so that lookup always succeeds, `lastSG`
becomes `sg`, and `allNew` is **never** true. The “entirely new cast”
transition can fire only via `allNotPhysicallyPresent`.

```
--- a/timeRelations.cpp
-		for (int I = sg - 1; I >= 0 && lastSG < 0; I++)
+		for (int I = sg - 1; I >= 0 && lastSG < 0; I--)
```

Watch `SGT` / `tlTransition` logs. Groups that are actually a new cast of
characters will start being marked as time/location transitions.

### 30. `timeRelations.cpp:1181` `twsCapacity` inserts `morrow` and drops `NamedHoliday`

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** DO-FIRST.

`eCapacity` (`timeRelations.h:39-46`) is:

```
cTonight, cToday, cTomorrow, cYesterday,
cNamedMonth, cNamedDay, cNamedSeason, cNamedHoliday,
cUnspecified
```

`twsCapacity` inserts `morrow` after `tomorrow` and never lists
`NamedHoliday`. `whichCapacity` returns the table index and callers cast it
to `eCapacity`. Current mapping:

| word          | index | enum it hits      |
| ---           | ---   | ---               |
| tomorrow      | 24    | cTomorrow (ok)    |
| morrow        | 25    | cYesterday        |
| yesterday     | 26    | cNamedMonth       |
| NamedMonth    | 27    | cNamedDay         |
| NamedDay      | 28    | cNamedSeason      |
| NamedSeason   | 29    | cNamedHoliday     |
| unspecified   | 30    | cUnspecified (ok) |

`capacityString` prints the same shifted names. “yesterday” is therefore
stored and logged as a named month.

Do **not** add `cMorrow` to the enum — `morrow` is a synonym for tomorrow
(the pattern at `timeRelations.cpp:261` already tags `noun|morrow` as
`TIMECAPACITY`). Keep the enum, make the table index-parallel, and alias
`morrow` in the lookup:

```
--- a/timeRelations.cpp
-	 L"tonight",L"today",L"tomorrow",L"morrow",L"yesterday",
-	 L"NamedMonth",L"NamedDay",L"NamedSeason",
+	 L"tonight",L"today",L"tomorrow",L"yesterday",
+	 L"NamedMonth",L"NamedDay",L"NamedSeason",L"NamedHoliday",
 	 L"unspecified",NULL };

 int whichCapacity(wstring w)
 {
 	LFS
+		if (w == L"morrow") return cTomorrow;
 		for (int I = 0; twsCapacity[I]; I++)
```

This is the same shape as the `months_abb_index` fix above: lexicon
unchanged, mapping corrected. Watch any log that prints `timeCapacity` for
texts using “yesterday” or “morrow”.

### 31. `names.cpp:1784` “twentieth” maps to 0

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

```1784:1784:names.cpp
	{ L"twentieth", 0 }, // copy-paste: should be 20
```

`mapNumeralOrdinal` returns that value for age / date ordinals. “twentieth”
is treated as zeroth.

```
--- a/names.cpp
-	{ L"twentieth", 0 },
+	{ L"twentieth", 20 },
```

### 32. `names.cpp:471` `cName::notNull()` is inverted — do not flip it

**Verdict:** CONFIRMED as a name/implementation mismatch. **Risk:** Needs
author decision. **Order:** DEFER (rename, do not invert).

`notNull()` returns true when **every** part is `wNULL` — i.e. it implements
`isCompletelyNull` (and is slightly stricter than the existing
`isCompletelyNull()` in `resolveObjects.cpp:687`, which ignores `hon2` /
`hon3` / `middle` / `middle2` / `suffix`). The only caller is
`resolveSpeakers.cpp:185`:

```
if (object->objectClass==NAME_OBJECT_CLASS && !object->name.notNull() && (object->end-object->begin)>1)
    object->name.print(...)
```

`!notNull()` currently means “has some name part”, which is what the print
path wants. Flipping the body without flipping the caller would print
multi-word *empty* names and skip real ones.

Recommended: delete `notNull()` and use `!isCompletelyNull()` (or
`!isNull()` if honorifics-only names should also take the `print` path) at
the one call site. Do not invert the body in place.

### 33. `resolveFirstSecondPersonPronouns.cpp:196` erase-then-read

**Verdict:** CONFIRMED. **Risk:** Safe (the `break` limits the damage to one
invalid iterator deref). **Order:** NORMAL.

```195:200:resolveFirstSecondPersonPronouns.cpp
				m[where].objectMatches.erase(oi);
				objectClass = PRONOUN_OBJECT_CLASS;
				inflectionFlags = m[objects[oi->object].originalLocation].word->second.inflectionFlags;
				m[where].flags &= ~cWordMatch::flagObjectResolved;
				break;
```

Save `oi->object` before the erase.

```
--- a/resolveFirstSecondPersonPronouns.cpp
+				int erased = oi->object;
 				m[where].objectMatches.erase(oi);
 				objectClass = PRONOUN_OBJECT_CLASS;
-				inflectionFlags = m[objects[oi->object].originalLocation].word->second.inflectionFlags;
+				inflectionFlags = m[objects[erased].originalLocation].word->second.inflectionFlags;
```

### 34. `resolveSpeakers.cpp:286` `oStr[0]=0` on an empty `wstring`

**Verdict:** CONFIRMED. **Risk:** Safe. **Order:** NORMAL.

`wstring oStr;` then `oStr[0]=0` is `operator[]` on `size()==0`.

```
--- a/resolveSpeakers.cpp
 	wstring oStr;
-	oStr[0]=0;
 	if (notFirst)
```

### 35. `identifySpeakerGroups.cpp:1617` `isFocus` indexes `m[I]` before the bound

**Verdict:** CONFIRMED. **Risk:** Safe. **Order:** NORMAL.

```
for (I = where + 1; (m[I].beginObjectPosition < 0 || (m[I].flags & cWordMatch::flagAdjectivalObject)) && I < (signed)m.size(); I++);
```

When `I == m.size()` the `m[I]` conjunct is evaluated first.

```
--- a/identifySpeakerGroups.cpp
-			for (I = where + 1; (m[I].beginObjectPosition < 0 || (m[I].flags & cWordMatch::flagAdjectivalObject)) && I < (signed)m.size(); I++);
+			for (I = where + 1; I < (signed)m.size() && (m[I].beginObjectPosition < 0 || (m[I].flags & cWordMatch::flagAdjectivalObject)); I++);
```

### 36. `identifySpeakerGroups.cpp:938` hail-delete log dereferences `localObjects.end()`

**Verdict:** CONFIRMED. **Risk:** Safe (log-only; the delete still happens).
**Order:** NORMAL.

`lsi = in(*s)` can be `end()`. The delete condition can still be true via the
other `||` arms (`justHonorific`, `numEncountersInSection==0`, …). The log
then reads `lsi->lastWhere`.

```
--- a/identifySpeakerGroups.cpp
 			if (debugTrace.traceSpeakerResolution)
-				lplog(LOG_SG, L"... [LW=%d,PW=%d,%s,...]", ...,
-					lsi->lastWhere, lsi->previousWhere, (lsi->physicallyPresent) ? L"PP" : L"not PP", ...);
+				lplog(LOG_SG, L"... [LW=%d,PW=%d,%s,...]", ...,
+					(lsi != localObjects.end()) ? lsi->lastWhere : -1,
+					(lsi != localObjects.end()) ? lsi->previousWhere : -1,
+					(lsi != localObjects.end() && lsi->physicallyPresent) ? L"PP" : L"not PP", ...);
```

### 37. `identifySpeakerGroups.cpp:1289` `&speakerGroups[-1]` when empty

**Verdict:** CONFIRMED. **Risk:** Safe (empty-group path). **Order:** NORMAL.

```1289:1289:identifySpeakerGroups.cpp
	determinePreviousSubgroup(end, speakerGroups.size() - 1, &speakerGroups[speakerGroups.size() - 1]);
```

When `speakerGroups` is empty this is `size_t(-1)` and `&speakerGroups[-1]`.
The function already computes `lastSG = end()` in that case but does not use
it here.

```
--- a/identifySpeakerGroups.cpp
-	determinePreviousSubgroup(end, speakerGroups.size() - 1, &speakerGroups[speakerGroups.size() - 1]);
+	if (!speakerGroups.empty())
+		determinePreviousSubgroup(end, (int)speakerGroups.size() - 1, &speakerGroups.back());
```

Confirm `determinePreviousSubgroup` is optional on the first group (it should
be — there is no previous subgroup).

### 38. `identifySpeakerGroups.cpp:66` unbounded `copy(cOM&)`

**Verdict:** CONFIRMED. **Risk:** Safe (corrupt SourceCache). **Order:** NORMAL.

The counted `vector<cOM>` deserializer checks `limit` for the count, then
each element goes through the no-limit `copy(cOM&)`.

```
--- a/identifySpeakerGroups.cpp
 bool copy(cOM& num, char* buf, int& where)
+bool copy(cOM& num, char* buf, int& where, int limit)
 {
 	DLFS
+		if (where + (int)sizeof(cOM) > limit) return false;
 		num = *((cOM*)(buf + where));
```

Update the vector overload and every other `copy(cOM&)` caller to pass
`limit`. The write-then-check serialize overload in the same file has the
same “check after write” shape as `utilities.cpp copy()` (already in
`CODE_REVIEW.md`); fix that the same way as the scalar `copy()` overloads
when those are applied.

### 39. `resolveObjects.cpp:846` `preferWordOrder` erase without a size guard

**Verdict:** CONFIRMED. **Risk:** Safe if `preferWordOrder` never returns 0
on a 1-element list; otherwise UB. **Order:** NORMAL.

`tmp == 0` does `objectMatches.erase(objectMatches.begin() + 1)` with no
`size() >= 2` check. `adjustForWordOrderSensitiveModifier` does guard.

```
--- a/resolveObjects.cpp
 		if (tmp == 0)
+		{
+			if (objectMatches.size() < 2) return chooseFromLocalFocus;
 			objectMatches.erase(objectMatches.begin() + 1);
+		}
 		else if (tmp == 1)
+		{
+			if (objectMatches.empty()) return chooseFromLocalFocus;
 			objectMatches.erase(objectMatches.begin());
+		}
```

### 40. `resolveObjects.cpp:1600` `speakerGroups[currentSpeakerGroup + 1]` unchecked

**Verdict:** CONFIRMED. **Risk:** Safe (OOB when the current group is last).
**Order:** NORMAL.

```
--- a/resolveObjects.cpp
+	if (currentSpeakerGroup + 1 >= (int)speakerGroups.size()) return false;
 	set <int> speakers = speakerGroups[currentSpeakerGroup + 1].speakers;
```

### 41. `identifySpeakerGroups.cpp:960` empty-group early-out returns `false` (Narrator)

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing if the first document
ever hits this path. **Order:** NORMAL.

`detectUnresolvableObjectsResolvableThroughSpeakerGroup` is `int` and uses
`-1` as the no-match sentinel. `return false` is `0`, i.e. object 0
(Narrator).

```
--- a/identifySpeakerGroups.cpp
-		if (speakerGroups.empty()) return false;
+		if (speakerGroups.empty()) return -1;
```

### 42. `identifySpeakerGroups.cpp:2262` `intString` OOB on an empty POV range

**Verdict:** CONFIRMED. **Risk:** Safe (log-only). **Order:** NORMAL.

When `startPOVI >= povi` the loop writes nothing, then
`itos(povInSpeakerGroups[startPOVI], ...)` reads off the end of the vector.

```
--- a/identifySpeakerGroups.cpp
 	if (tmpstr.empty())
-		tmpstr = itos(povInSpeakerGroups[startPOVI], tmp);
+		return L"";
```

### 43. `identifySpeakerGroups.cpp:3308` `block && flags & 1` precedence

**Verdict:** CONFIRMED as a precedence bug. **Risk:** Needs author decision.
**Order:** DEFER.

`block && m[I].flags & 1` parses as `(block && m[I].flags) & 1`. `&&` yields
`bool`, so this is `block && flags != 0` — unblock whenever *any* flag is
set. Bit 0 is `flagFirstEmbeddedStory` (quotes) / `flagIgnoreAsSpeaker`
(non-quotes). Almost every token has some flag, so
`blockSpeakerGroupCreation` is nearly a no-op.

Minimal patch matching the written `& 1`:

```
-	if (block && m[I].flags & 1)
+	if (block && (m[I].flags & 1))
```

Ask whether the intended bit is `flagIgnoreAsSpeaker` or
`flagFirstEmbeddedStory` and write that named flag instead of `1`.

### 44. `resolveSpeakers.cpp:5289` `_VERBPAST` audience scan compares a position to a length

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

The `_VERBREL1` branch converts `end` to an absolute position. The
`_VERBPAST` branch leaves `end` as a pattern *length*, then:

```
speakerObjectPosition = end + where + 1;
for (...; audienceObjectPosition < end; ...)
```

`audienceObjectPosition` starts at `speakerObjectPosition + 1` which is
already `> end` for any realistic length, so the “said X to Y” scan never
iterates.

```
--- a/resolveSpeakers.cpp   (inside the _VERBPAST branch, after speakerObjectPosition is set)
+    int audienceLimit = end + where + 1;
+    if (extendedSayVerb) audienceLimit++;
     for (audienceObjectPosition=speakerObjectPosition+1,im++; audienceObjectPosition<audienceLimit; im++,audienceObjectPosition++)
     {
-        if (im->word->first==L"to" && audienceObjectPosition+1<end && ...
+        if (im->word->first==L"to" && audienceObjectPosition+1<audienceLimit && ...
```

Mirror the `_VERBREL1` conversion (`end += where + 1`) if that branch is the
intended template. Watch “said X to Y” after a simple past-tense verb.

### 45. `resolveObjects.cpp:389` plural non-gendered loop tests `localObjects[0]`

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

```
for (unsigned int s = 0; s < localObjects.size(); s++)
{
    if (localObjects[0].om.object <= 1) continue;
```

Intended: skip narrator/audience at slot `s`. As written, if `[0]` is
narrator the whole loop is a no-op; if not, narrator/audience later in the
list are never skipped.

```
--- a/resolveObjects.cpp
-		if (localObjects[0].om.object <= 1) continue;
+		if (localObjects[s].om.object <= 1) continue;
```

Watch plural non-gendered resolution (“the pictures” → a prior same-head
plural).

### 46. `resolveObjects.cpp:517` Num/address matcher returns after the first local object

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

The `om.object > 1` candidate loop `return true`s inside the first matching
`localObjects` slot, so later slots never contribute. Move `return true` to
after the `localObjects` loop (it already logs when `objectMatches.size()`).

### 47. `identifyObjects.cpp:1536` `getPrincipalWhereAndEndAndNameInfo` args swapped

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** DO-FIRST.

Declaration / definition take `(..., bool& plural, bool& embeddedName, ...)`.
The call passes `(..., embeddedName, plural, ...)`. Plurality is written into
`embeddedName` and an embedded-name detection is written into `plural`. That
feeds `identifyName` and the ALL-CAPS title walk.

```
--- a/identifyObjects.cpp
-		getPrincipalWhereAndEndAndNameInfo(tagName, where, element, principalWhere, embeddedName, plural, end, nameElement);
+		getPrincipalWhereAndEndAndNameInfo(tagName, where, element, principalWhere, plural, embeddedName, end, nameElement);
```

Watch NAME / NOUN object classification, especially ALL-CAPS titles and
embedded names inside a larger noun.

### 48. `identifyObjects.cpp:240` `isPleonastic` MEANS branch is off-by-one

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

The sibling “makes/finds it MA” test uses `m[where + 1]` / `m[where - 1]`, so
`where` is the pleonastic “it”. The MEANS comment says “It MEANS (that) S”
but the verb is read at `where + 2`. “it seems that S” therefore never
matches.

```
--- a/identifyObjects.cpp
-	for (I = 0; MEANS[I] && m[where + 2].word->first != MEANS[I]; I++);
-	if (MEANS[I] && (m[where + 3].pma.queryPattern(L"__S1") != -1 || m[where + 3].pma.queryPattern(L"_REL1") != -1)) return true;
+	for (I = 0; MEANS[I] && m[where + 1].word->first != MEANS[I]; I++);
+	if (MEANS[I] && where + 2 < (int)m.size() &&
+		(m[where + 2].pma.queryPattern(L"__S1") != -1 || m[where + 2].pma.queryPattern(L"_REL1") != -1 ||
+		 (m[where + 2].word->first == L"that" && where + 3 < (int)m.size() &&
+		  (m[where + 3].pma.queryPattern(L"__S1") != -1 || m[where + 3].pma.queryPattern(L"_REL1") != -1))))
+		return true;
```

The optional-`that` arm is an author decision; the `where + 1` verb lookup is
not.

### 49. `identifyObjects.cpp:1261` missing parens around `|| flagNounOwner`

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

```
if (identifyObject(...) >= 0 && m[I].getObject() >= 0 &&
    (m[I].word->second.inflectionFlags & (PLURAL_OWNER | SINGULAR_OWNER)) || (m[I].flags & cWordMatch::flagNounOwner))
    ownerWhere = I;
```

This is `(A && B && C) || D`. A `flagNounOwner` word sets `ownerWhere` even
when `identifyObject` failed or `getObject() < 0`.

```
--- a/identifyObjects.cpp
-				(m[I].word->second.inflectionFlags & (PLURAL_OWNER | SINGULAR_OWNER)) || (m[I].flags & cWordMatch::flagNounOwner))
+				((m[I].word->second.inflectionFlags & (PLURAL_OWNER | SINGULAR_OWNER)) || (m[I].flags & cWordMatch::flagNounOwner)))
```

### 50. `identifyObjects.cpp:267` `searchExactMatch` tests the new object’s `eliminated`

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

```
if (!object.eliminated && object.equals(objects[*s], m))
```

`object` is the newly built candidate (never eliminated). The test should
skip *already-merged* hits in `relatedObjects`.

```
--- a/identifyObjects.cpp
-		if (!object.eliminated && object.equals(objects[*s], m))
+		if (!objects[*s].eliminated && object.equals(objects[*s], m))
```

### 51. `names.cpp:555` `merge()` never replaces a single-letter name part

**Verdict:** CONFIRMED. **Risk:** Behaviour-changing (intended). **Order:** NORMAL.

```
if (w2 != wNULL && (w1 == wNULL || (w1->first[1] && w2->first[1]))) w1 = w2;
```

`first[1]` is the second character (0 when `length()==1`). A letter (`"J"`)
is therefore never replaced by a full first name. The function comment says
the opposite of what the code does (“merge … if we only have a letter and n
has more”).

```
--- a/names.cpp
-		if (w2 != wNULL && (w1 == wNULL || (w1->first[1] && w2->first[1]))) w1 = w2;
+		if (w2 != wNULL && (w1 == wNULL || ((w1->first.size() <= 1) && w2->first.size() > 1) || (w1->first.size() > 1 && w2->first.size() > 1)))
+			w1 = w2;
```

Ask whether a longer `w1` should ever be overwritten by a longer `w2` (the
current `w1->first[1] && w2->first[1]` branch). If the only intended upgrade
is letter → word, drop the last `||`.

---

## Still to append

Verification is still outstanding for the remaining category-2/3 items in
`CODE_REVIEW.md`:

- syntactic relations (`checkAmbiguousVerbTense`, `evaluateSubjects`,
  `testSyntacticRelations` `m[end]`, `findPrepRole`, SRG cache ctor,
  `getWSAdverb`, conversationContext identical `||` arms,
  `ADJECTIVE_INFLECTIONS_MASK`)
- time/names leftovers (`cTimeInfo::clear()`, `ageTransition`
  `objects.begin()+(-1)`, `detectPlaceTransition` `&&`/`||`)
- infra (`utilities.cpp copy()`, `bitObject.h`, `DIYDiskArray` dtor,
  `profile.h` const write, `paice.cpp` overlapping `memcpy`, `decodeURL`,
  hmm NaN / `[length-2]` / `_wfopen`, `generateBNCSources`,
  `readMultiSourceObjects`, `properties.find` argument order,
  `readOntologyList`)
- QA / acquisition (`stripWeb`, `cProximityEntry`, `qt | typeQTMask`,
  `metaPatternMatch`, yajl leak, `jsonBuffer[0]`, `speakerGroups[sgAt]`,
  Wikipedia `source` NULL, `vcXML` `aH`, specials Dictionary.com)
- unused `newPatternDetection.cpp` infinite PEMA loop (only if that TU is
  in a build)

Those will be appended here in the same format. Nothing above has been
applied.
