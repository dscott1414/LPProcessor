# LPProcessor code review — closing report

This is the merged, closed record of the code-review effort on this repository.
It combines what used to be two documents: `CODE_REVIEW.md` (the findings and
their closing status) and `PROPOSED_FIXES.md` (a per-entry catalogue of ~77
undefined-behaviour and logic-inversion patches). `PROPOSED_FIXES.md` has been
deleted and the parts of it that still carry information — its correction to
this document's overstated stack-buffer claim, and the handful of items it left
genuinely open — are folded in below.

**The per-entry patch catalogue is deliberately gone.** Every entry in it had
been applied; a line-by-line record of "was a bug, now isn't" has little ongoing
value, and `git log` / `git diff` against the pre-remediation tree is the
authoritative record of exactly what changed and why. What is kept here is the
map of what was covered, the conclusions worth remembering, and — the part that
actually matters day to day — the short list of things that are still open.

For the separate macOS port of the same tree, see `MAC_PORT.md`. It is not a
code-review document, but it changed enough of this one's assumptions (there is
now a working compiler) that the two should be read together.

## Status

**The remediation is complete.** An original review pass raised roughly 180
findings across ten "wave" categories (`CRITICAL` through `NIT`). Twelve
remediation batches (A0–K2) worked through the whole first-party tree; a closing
pass re-sampled findings across every wave and severity and found no regressions
and no falsely-claimed fixes. Everything in the "Open items" section below is
what genuinely remains — three items, none of them a memory-safety,
logic-inversion, or injection defect.

**The verification standard has changed, and improved.** This document used to
say "there is no compiler available in this environment ... every claim is
justified by reading the code, not by building or running it." That is no longer
true. Since the macOS port (`MAC_PORT.md`), the entire first-party engine
compiles under clang with `-Wall -Wextra`, `liblpcore.a` archives, and both `lp`
and `CorpusAnalysis` link and run. So every claim here now has at least the
compiler behind it — which is what caught several defects the reading passes had
missed, including a `cName::isNull()` declared nowhere, a `getNumSourcesProcessed`
that `CorpusAnalysis` referenced but no linked translation unit defined, and a
long tail of type errors that the untyped `windows.h` `min`/`max` macros had been
hiding.

What still has *not* happened: **nothing has parsed a real document.** No corpus
run, no test suite (there isn't one), no behavioural comparison against the
Windows build. "Compiles, links, starts, fails cleanly" is the ceiling of what
has been demonstrated.

**Build warnings.** The MSVC x64 configurations use `EnableAllWarnings` with a
minimal suppression list; the CMake build uses `-Wall -Wextra`. Neither treats
warnings as errors. The clang build is currently dominated by `-Wparentheses`
firing on this codebase's deliberate `if (error = !copy(...))` idiom — not a
defect, but it buries real warnings, so `-Wno-parentheses` is worth adding before
anyone tries to clear the rest and turn warnings into errors.

## Scope

Vendored third-party trees were not touched: `lloyd-yajl-66cb08c/`,
`tinyxml2-master/`, `packages/`, and all of `HMM/`. `lists/` is runtime data, not
code. `stacktrace.h` is vendored third-party (Sean Farrell, MIT) and is now
unused — the macOS port replaced its only consumer with `backtrace()`; the file
is left on disk for the unmaintained Windows project.

## Open items

Three. Each needs a decision only the author can make — that is why they are
still here rather than fixed.

1. **`ACCUMULATE_GROUPS` is an unfinished feature, not a bug.**
   In `syntacticRelations.h`/`.cpp`. `cWordGroup`'s fields were retyped during
   remediation to match how `summary()`/`incorporateMapping()` actually use them,
   and every constructor now initializes `index`/`otherFlag`. But
   `cSourceWordInfo::addRelation()`'s `cWordGroup(this, fromWord, word, toWord)`
   call still matches no constructor in the class — `this` is a
   `cSourceWordInfo*`, not the `tIWMM` the constructors expect. The whole path is
   `#ifdef ACCUMULATE_GROUPS`'d out and has never been linked into any build, on
   any platform. **Decision needed: finish the word-clustering feature, or delete
   it.** Designing a constructor for a feature nobody has specified is not
   something a review pass should guess at. Zero regression risk either way.

2. **`cSource::determineTimelineSegmentLink()` is a stub returning `false`.**
   `timeRelations.cpp`. Two live callers assign its result to `ts.linkage`, so
   timeline segments are currently never linked. Implementing it is new feature
   work that depends on what "linkage" is supposed to mean — again, author
   intent, not a fix.

3. **`LPWeb/Startup.cs` calls `UseAuthorization()` with no authentication
   middleware registered**, which makes it a no-op. This is now accurately
   documented in that file rather than silently wrong. `LPWeb` is a personal
   read-only viewer, so this may well be fine as-is — it is recorded only because
   it stops being fine the moment the viewer is exposed beyond localhost.

Everything else either landed or was resolved while merging these two documents;
see "Closed while merging" below.

## Cross-cutting conclusions

The five recurring defect classes the original review identified, and where they
ended up:

1. **Secrets in source.** MySQL `root` credentials, Google CSE / Bing /
   Merriam-Webster API keys, a Twitter password and a NewsBank library card were
   hardcoded across ten files, plus live secrets pasted into `README.md`.
   **Resolved.** `envConfig.h`/`.cpp` put every credential behind an environment
   variable with no hardcoded default; the README was scrubbed; `unused source/`
   (which held the NewsBank credential) was deleted. A fresh repo-wide grep for
   the literal values finds zero hits in the working tree.
   **They remain in git history**, which was out of scope to rewrite. If any of
   those credentials were ever live, treat them as compromised and rotate them.
2. **Undefined behaviour on the hot path.** `reserve()`d-then-indexed vectors,
   multi-megabyte stack buffers, iterator erase-then-read, `wstring[0]` on empty,
   `m[end]` at `end == m.size()`. **Resolved** — `resize`/`assign` replace the
   `reserve` bugs, every oversized stack buffer is now heap-allocated (see below),
   and the bounds/emptiness guards are in place at the sites sampled.
3. **Logic inversions that silently drop linguistic coverage.**
   `containingSpeakerGroup` comparing spans to a loop index; `speakerGroupTransition`
   incrementing instead of decrementing; `months_abb` not index-parallel to
   `months[]`; `cName::notNull()` inverted; one-sided `sameSpeaker` tests
   inverted; several `&&`/`||` precedence bugs parsing as `(A && B) || C`.
   **Resolved.** The `months_abb` fix is worth singling out: remediation restored
   an earlier, correct, lexicon-safe fix (the `months_abb_index[]` mapping table,
   which keeps `months_abb[]` from also registering "may"/"jun"/"jul" as month
   nouns through a different lexicon path) after finding that an intermediate
   refactor had silently reverted it to a worse state.
4. **SQL and HTTP built by concatenation.** **Resolved** at the sites sampled —
   escaping via `mysql_real_escape_string` / the project's own
   `escapeStr`/`encodeEscape`, and HTTPS + URL-encoding in the acquisition layer.
5. **Process-control footguns.** `lplog(LOG_FATAL_ERROR)` used to block on
   `getchar()` then `exit(0)`, so an unattended child would hang and, if killed,
   report success. **Resolved** — `logging.cpp` routes fatals through
   `fatalExit()`, which exits `EXIT_FAILURE` and only waits for a keypress when
   interactive.

## The stack-buffer finding, corrected and closed

This is the one place the original review was wrong, and it is worth preserving
because the correction is instructive.

The review claimed several multi-megabyte stack arrays "will overflow as soon as
the function runs", reasoning from MSVC's 1 MB default stack. That default did
not apply: both `.vcxproj` files set `StackReserveSize`/`StackCommitSize` to
21,097,152 bytes (20.12 MiB), deliberately sized just above the largest buffer in
the tree. So the accurate finding was never "guaranteed overflow" but three
milder things: the functions worked only because of a non-obvious linker setting
nothing in the source explained; the largest two left about 1 MB for the entire
rest of the call chain; and because commit equalled reserve, the full 20 MB was
committed *per thread* — under `-mp` with N children, N × 20 MB.

A repo-wide sweep found thirteen such arrays where the review had found five.

**Now closed.** Every one of them is heap-allocated through the tracked allocator
(so the memory still shows up in `memoryAllocated`), and the largest stack array
left anywhere in the tree is 100 KiB. The three 10 MiB buffers in `source.cpp`
and the 1.9 MiB one in `createOntology.cpp` were the last holdouts and went
during this merge.

They use a new `cTrackedBuffer` RAII wrapper in `general.h` rather than paired
`tmalloc`/`tfree` calls, because `PROPOSED_FIXES.md`'s own recommended shape —
"with a matching `tfree()` on **every** return path; audit each `return` before
applying" — is a trap: `cSource::write()` has 26 return statements. Letting the
buffer own itself is correct on all of them, and on any added later. A companion
`cScopedFd` fixed the descriptor leaks on `writePatternUsage`'s early-return arms
while it was being converted.

With the buffers gone, the oversized stack reservation had no reason to exist,
and reclaiming it was the *actual* payoff the correction identified. The CMake
build now asks for 8 MiB (macOS's own default, stated explicitly so it reads as a
decision) instead of 20 MiB. Both binaries still start and run cleanly on it.

## Waves — the map of what was covered

Each wave names the file-set a remediation batch owned, so a maintainer knows
where to look in `git log` for a given file's history.

- **Core infra** (`main.cpp`, `source.*`, `word.cpp`, `tokenize.cpp`,
  `utilities.cpp`, `DBUtility.cpp`, `mysqldb.h`) — hardcoded paths moved behind
  `envConfig`, FATAL exit path fixed, serialize bounds-checking fixed,
  query-buffer macros parenthesized, SQL buffer lock held across the query.
- **Question answering** (`questionAnswering.cpp`, `QuestionAnswering.h`,
  `questionProcessing.cpp`, `questionAnsweringWebSearch.cpp`) — Google/Bing keys
  removed, several leaks (yajl trees, `wikiTableMap` pointers, transformed SRGs)
  fixed, multiple off-by-one/bounds bugs and the always-15 question-type remap.
- **Ontology, HMM, specials** (`createOntology.cpp`, `ontology.h`, `hmm.*`,
  `specials_main.cpp`) — oversized buffers heap-allocated, unescaped SQL fixed,
  lock-leak and bounds bugs, `hmm.h`/`.cpp` arity mismatches realigned.
- **DB layer and shared headers** (`DB.cpp`, `DBCreateSQLSchema.cpp`,
  `DBWordRelations.cpp`, `tableColumn.*`, `general.h`, `logging.*`, `profile.h`,
  `intArray.h`, `DIYDiskArray.h`, `bitObject.h`, `paice.*`, `Internet.cpp`, …) —
  credentials removed, a column-mapping data-correctness bug in
  `readMultiSourceObjects` fixed, lock leaks and a UTF-16 BOM overlapping-`memcpy`
  bug fixed, `Loebner.cpp` (a comments-only stub) deleted.
- **Object and pronoun resolution** (`identifyObjects.cpp`, `resolveObjects.cpp`,
  `resolveMetaGroupObjects.cpp`, `resolveFirstSecondPersonPronouns.cpp`) —
  swapped-argument bug, off-by-one pleonastic-it branch, use-after-erase iterator
  bug, the `containingSpeakerGroup` span-vs-index inversion.
- **Pattern engine** (`definePatterns.cpp`, `pattern.*`, `patternMatchArray.*`,
  `patternElementMatchArray.*`) — the `reserve`-not-`resize` usage-counter bug,
  the `setMandatoryAncestorPatterns` wrong-bitset bug, a `CHILDPATBITS`
  enum/shift collision, self-assignment and format-string bugs; dead overloads
  and an infinite-loop function removed.
- **Semantic relations, time, names** (`semanticRelations.*`, `timeRelations.*`,
  `names.*`) — the `months_abb` restoration, `speakerGroupTransition` direction,
  `cName::notNull()` inversion, unbounded-index and precedence bugs.
- **Data acquisition** (`getWikipedia.cpp`, `getWordNet.cpp`, `getThesaurus.cpp`,
  `getDictionary.cpp`, `initializeDictionary.cpp`, `getMusicBrainz.*`,
  `getTwitter.cpp`, `tagOperations.cpp`, `vcXML.*`) — Merriam-Webster key
  removed, two NULL-dereference crashes fixed, HTTPS + URL-encoding, buffer and
  handle leaks.
- **Satellite tools** (`processGutenbergRDFtoSQL`, `Web/pyLPBackEnd`,
  `convertPDFTextToDatabase`, `checkTypes`, `correctRDF`, `DirectoryAnalysis`,
  `LPWeb`) — credentials and a Flask secret key removed, SQL injection fixed with
  parameterized queries, an unbounded `strcat` bounded, a buffer-size typo and an
  output-order bug in `correctRDF`, a wrong-variable file-count bug in
  `DirectoryAnalysis`. The `unused source/` tree (12 files, including one that
  would not compile) was deleted.
- **Two Java trees found mid-review**, `TextRenderer/` and `show/` — `show/`'s
  binary-cache readers (`Relation.java`, `CObject.java`) had bit-offset bugs
  silently misreading flags by 1–3 bit positions on every cached read.
- **Agreement and syntactic relations** (`agreement.cpp`, `syntacticRelations.*`,
  `syntacticRelationGroups.cpp`) — the highest-traffic winnow-path
  `reserve`-not-`resize` bug, an unbounded `setRole` backward walk capped, a
  Watson `objects[-1]` bug, precedence bugs, and a `getRelStr` enum-decomposition
  bug affecting the generated DB schema.
- **Speaker groups and speaker resolution** (`identifySpeakerGroups.cpp`,
  `resolveSpeakers.cpp`) — `sameSpeaker`'s inverted one-sided tests, an unbounded
  deserializer, and a subtle cross-container iterator bug where `set::find()` was
  compared against a *different* set's `.end()`, silently breaking
  previous-subject speaker attribution.

## Closed while merging these two documents

Each of these was an open recommendation in one of the two source documents. Each
was re-checked against the current source and then either done or retired, so
that nothing below is left as a standing recommendation.

- **The remaining oversized stack buffers** — done; see the section above.
- **Lowering the stack reservation** — done; 20 MiB → 8 MiB.
- **`DBCreateSQLSchema.cpp`'s "broken-if-run" table creators** — the claim was
  partly stale. Re-checked: `createThesaurusTables` and `createLocationTables`
  are valid SQL as they stand. `createGroupTables` really was broken, in two
  specific ways, both now fixed: `groups` declared `groupId` with no index on it
  (so both child tables' `FOREIGN KEY … REFERENCES groups(groupId)` would be
  rejected — InnoDB requires the referenced column to be indexed), and
  `subGroups` referenced a `groups(id)` column that does not exist. All three
  remain uncalled. The fix is reasoned from the schema, not executed against a
  server.
- **`resolveSpeakers.cpp`'s `preferPreviousSpeaker()` dead code** — retired as a
  document-level item. It is fully commented out inside a `/* */` block and
  carries an in-place explanation of why reviving it needs author input. The
  source comment is the right home for that; it does not need a second entry
  here.
- **Zero-caller helper functions** (`cSourceWordInfo::computeDBUsagePatternsToUsagePattern`,
  `isLowestCost`, `patternString`, `removeIllegalForms`, `cWord::addInflectionFlag`,
  `removeInflectionFlag`, `pattern.cpp`'s `allWordFlags`) — re-confirmed to have
  zero callers, and the recommendation ("a maintainer should judge whether to
  delete these") is now settled: **keep them.** Each is one half of a deliberate
  symmetric API pair — `addInflectionFlag`/`removeInflectionFlag` are a matched
  set and both are unused — so they read as intentional surface rather than
  debris. Deleting working code on a usage count alone is not worth it.
- **The "still to append" list** in `PROPOSED_FIXES.md` — every item on it was
  verified fixed, or moot because the file holding it was deleted, or is Open
  item 1 above.

## Verification

The remediation batches were each read and spot-checked by an orchestrating
session rather than trusted from a self-report, and several of the highest-risk
changes were hand-traced through their call sites. The closing pass independently
re-sampled more than 45 findings spread across every wave and severity, and found
every one fixed as described — zero regressions, zero falsely-claimed fixes.

For this merge, the claims that matter were re-checked directly against the
current source: the five cross-cutting classes (a fresh secrets grep returns
zero; escaping, `fatalExit`, `months_abb_index`, `cName::isNull`,
`minSeparatorCost.resize`, `pattern.h`'s `assign`, `decodeURL`'s bounds guard,
`paice`'s BOM `memmove`, `DIYDiskArray`'s destructor closing its fd, and the
`ADJECTIVE_INFLECTIONS_MASK` bit are all in the state claimed), plus every item
in "Open items" and "Closed while merging" above.

Two honest limits on all of this. First, the ~135 findings not individually
re-sampled are treated as resolved on the strength of the sample's hit rate and
the batch reports, not re-verified one by one. Second, and more important:
compiling is not running. The riskiest changes in this whole effort — the
encoding-detection ladder, the binary source-cache codec, the newly bounded
pattern-engine formatters — are exactly the kind a corpus run would catch and a
startup smoke test would not. Running the test corpus against a live MySQL is the
single highest-value thing anyone can do next, and until it happens this document
describes code that is *believed* correct, not code that is *known* to still
parse English the way it did.
