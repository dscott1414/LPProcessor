# LPProcessor code review — closing report

This document began as findings collected while adding file-header and
per-procedure comments to the first-party sources of a personal-research
C++/MSVC/MySQL NLP pipeline, plus several satellite tools (C#, Python, Java).
It went through an original review pass (~180 findings across ten "wave"
categories, `CRITICAL`/`HIGH`/`MEDIUM`/`LOW`/`NIT`), three early patch waves
referenced by commit, and then a twelve-batch remediation effort (internally
labelled A0 through K2) that worked through the whole codebase file-set by
file-set. This closing pass re-verified a broad, representative sample of the
original findings against the current source, corrected the vendored-code
exclusion list, scrubbed secret-shaped literals out of this document's own
text, ran a final repo-wide dead-code sweep and secrets grep, and checked the
Visual Studio project files for consistency. See "Open items" below for what,
if anything, is genuinely still unresolved.

Vendored third-party trees are out of scope for this review and were not
touched: `lloyd-yajl-66cb08c/`, `tinyxml2-master/`, `packages/`, and all of
`HMM/` (`HMM/KenLM/`, `HMM/HMM POS Tagger`, `HMM/HMM POS Tagger 2`,
`HMM/HMM POS Tagger 3`, `HMM/HMM2CTest` — these are third-party reference
implementations bundled for comparison, not code the project's author wrote).
`lists/` is runtime data, not code, and is likewise out of scope.

## Status — closing state

**The remediation effort is complete.** Twelve batches (A0, A1, B, C, D, E, F,
G, H, I, J, K1, K2) worked through the entire first-party C++ engine, its
shared headers, and every satellite tool tree, in file-set order roughly
matching the waves below. Each batch's diff was read and spot-checked by an
orchestrating session (not just trusted from an agent's self-report); several
of the highest-risk changes were hand-traced through their call sites. This
closing pass then independently re-sampled findings spread across every wave
and severity level (see "Verification methodology" below) and found no
regressions and no falsely-claimed fixes in the sample checked.

There is no compiler available in this environment (MSVC, `windows.h`,
MySQL client headers, WordNet, the Java toolchain for `show`/`TextRenderer`
are all Windows/host-specific and not installed here). Every claim in every
batch, and in this closing pass, is justified by reading the code and
reasoning about it, not by building or running it. A maintainer with an
actual Windows/MSVC/MySQL build environment should compile and run the test
corpus before treating this as final.

**Build note carried over from the original review:** the x64 configurations
compile with `EnableAllWarnings` and a minimal suppression list (previously
`/Wall` was negated by a large `DisableSpecificWarnings` list that hid every
narrowing/signed-unsigned conversion, assignment-in-conditional, and
null-dereference/buffer-overrun analysis warning). `TreatWarningAsError` is
still `false` so newly surfaced warnings can be triaged on a machine that can
actually build the project; re-enable it once they are cleared.

## Open items — read this first

After twelve remediation batches and this closing verification pass, the
substantive backlog from the original review is resolved. A small number of
items are genuinely still open. None of them are secrets, memory-safety bugs,
or SQL/HTTP injection — those categories are clear. What remains:

1. **`syntacticRelations.h`/`syntacticRelations.cpp` — `ACCUMULATE_GROUPS` is
   an unfinished, unused feature.** `cWordGroup`'s fields were retyped during
   remediation to match how `summary()`/`incorporateMapping()` actually use
   them, and every constructor now initializes `index`/`otherFlag`. But
   `cSourceWordInfo::addRelation()`'s `cWordGroup(this, fromWord, word,
   toWord)` call still does not match any constructor in the class (`this` is
   a `cSourceWordInfo*`, not the `tIWMM` the constructors expect) — this
   whole code path is `#ifdef ACCUMULATE_GROUPS`'d out and not linked into
   the default build, so it has zero effect on the shipped parser. It needs a
   real constructor design from whoever decides to finish the clustering
   feature; that is a design task, not a bug fix, so it was left alone
   deliberately rather than guessed at. Not a regression risk either way.

2. **`timeRelations.cpp` `cSource::determineTimelineSegmentLink()` is a
   stub that always returns `false`.** This was flagged LOW in the original
   review as a stub, not a logic bug, and remains one. Implementing it (if
   the timeline-segment-linkage feature is wanted) is new work, not a fix.

3. **`DBCreateSQLSchema.cpp`'s `createGroupTables()`/`createLocationTables()`/
   `createThesaurusTables()` remain broken-if-run** (trailing commas,
   foreign-key column names that don't match, tables that don't exist) —
   already documented in the file's own header comment as broken, but never
   fixed, because all three are dead: `createGroupTables`/
   `createLocationTables` have no caller anywhere, and
   `createThesaurusTables`'s only reference is a commented-out call in
   `main.cpp`. Left alone deliberately (fixing SQL logic in a file another
   batch already closely reviewed was out of scope for this pass), but
   flagged here because it is a landmine for whoever uncomments that call.

4. **`resolveSpeakers.cpp` `preferPreviousSpeaker()` is dead code, left in
   place deliberately.** Fully commented out, with a comment explaining that
   its free variables (`resolveForSpeaker`, `highest[]`, etc.) don't resolve
   to anything in the current scope and that `chooseBetweenMatches`'s own
   documented tie-break chain does not call it. This was a considered
   decision during remediation (K2), not an oversight — recorded here only
   so a future maintainer knows it was looked at.

5. **`LPWeb/Startup.cs` — `UseAuthorization()` is a no-op** because no
   authentication middleware is registered. This is now accurately documented
   in the file's own header comment (it wasn't before) rather than silently
   wrong, but no authentication was added — `LPWeb` is a personal read-only
   viewer, so this may be fine as-is; flagged here in case it is ever
   exposed beyond localhost.

6. **A handful of small, self-contained, zero-caller helper functions** were
   found during this closing pass's dead-code sweep and were judged not
   worth deleting outright (see "Dead-code sweep" below) because they read
   as intentional, symmetric API surface rather than debris — `NIT`, not a
   defect. They're listed in that section for a maintainer's awareness.

Everything else the original review raised — every `CRITICAL` and `HIGH`
finding, and the large majority of `MEDIUM`/`LOW`/`NIT` findings — was
confirmed fixed wherever this pass checked it. See "Waves and their closing
status" for the file-set breakdown and "Verification methodology" for exactly
what was re-checked in this closing pass.

## Cross-cutting conclusions

The original review identified five recurring defect classes across the
codebase. Their closing status:

1. **Secrets in source.** MySQL `root` credentials, Google CSE / Bing /
   Merriam-Webster API keys, a Twitter account password, and a NewsBank
   library card were hardcoded across `DB.cpp`, `DBCreateSQLSchema.cpp`,
   `getThesaurus.cpp`, `getDictionary.cpp`, `questionAnsweringWebSearch.cpp`,
   `getTwitter.cpp`, `processGutenbergRDFtoSQL`, `pyLP.py`,
   `convertPDFTextToDatabase`, and one deleted file
   (`unused source/getNewsbank.cpp`), plus a live Azure account password and
   both Bing access keys that were pasted in plaintext in `README.md`.
   **Resolved.** `envConfig.h`/`envConfig.cpp` centralize every credential
   and path behind environment variables (`LP_DB_USER`, `LP_DB_PASSWORD`,
   `LP_BING_KEY`, `LP_GOOGLE_CSE_KEY`, `LP_MERRIAM_WEBSTER_KEY`, etc.) with
   no hardcoded default password; the README's live secrets were scrubbed
   and replaced with rotation instructions; `unused source/` (which held the
   NewsBank credential) was deleted outright. This closing pass's own
   repo-wide grep for the literal secret values found zero hits in the
   working tree outside this document's own historical text (now scrubbed)
   — see "Final secrets grep" below.
2. **Undefined behavior on the hot path.** `reserve()`d-then-indexed usage
   vectors and `minSeparatorCost`, multi-megabyte stack buffers, iterator
   erase-then-read, `wstring[0]` on empty, `m[end]` when `end==m.size()`.
   **Resolved** everywhere this pass sampled: `resize`/`assign` replace the
   `reserve` bugs, oversized stack buffers in the two largest call trees
   (`writeRDFTypes`/`writeExtendedRDFTypes`) are heap-allocated with the
   tracked allocator, and the erase-then-read / empty-container / off-by-one
   patterns checked were all fixed with a bounds or emptiness guard placed
   before the access. (Note from the original review, still accurate: the
   4–20 MB stack buffers were never a *guaranteed* overflow — both
   `lp.vcxproj` and `specials.vcxproj` set `StackReserveSize`/
   `StackCommitSize` to ~20.1 MiB in every configuration, deliberately sized
   just above the largest buffer. The real payoff of heap-allocating them is
   reclaiming that committed-per-thread memory, not crash avoidance.)
3. **Logic inversions that silently drop linguistic coverage.**
   `containingSpeakerGroup` comparing spans to the loop index instead of a
   source position; `speakerGroupTransition` incrementing instead of
   decrementing; `months_abb` not being index-parallel to `months[]`
   (originally skipping May–July, then a later refactor regressed it further
   by dropping "aug" outright); `cName::notNull()` inverted;
   one-sided `sameSpeaker` tests inverted; several `&&`/`||` precedence bugs
   parsing as `(A && B) || C` instead of `A && (B || C)`. **Resolved.** The
   `months_abb` fix is worth calling out specifically: remediation restored
   the earlier, correct, lexicon-safe fix (an index-mapping table,
   `months_abb_index[]`, that keeps `months_abb[]` from also registering
   "may"/"jun"/"jul" as month nouns through a different lexicon code path)
   after discovering an intermediate refactor had silently reverted it to a
   worse state. Every `&&`/`||` precedence finding this pass re-checked is
   now parenthesized as originally intended.
4. **SQL and HTTP built by concatenation.** Titles, words, and RDF fields
   interpolated into `INSERT`/`LIKE`/`VALUES('%s')` without escaping;
   Wikipedia/MusicBrainz/Twitter URLs sent over HTTP unescaped. **Resolved**
   everywhere this pass sampled — escaping via `mysql_real_escape_string`
   (or the project's own `escapeStr`/`encodeEscape`) is now in place on the
   query-construction paths checked, and HTTPS + URL-encoding fixes landed
   in the acquisition layer (`getWikipedia.cpp`, `getMusicBrainz.cpp`,
   `getTwitter.cpp`).
5. **Process-control footguns.** `lplog(LOG_FATAL_ERROR)` used to block on
   `getchar()` then `exit(0)` — an unattended multiprocessor child would
   hang, and if later killed the parent would see a false success exit code.
   **Resolved.** `logging.cpp` now routes `LOG_FATAL_ERROR` through
   `fatalExit()`, which exits with `EXIT_FAILURE` and only waits for a
   keypress when interactive (`multiProcess==0` and stdin is a TTY);
   `source.h`'s contradictory documentation of this behavior was corrected
   to match. `LOCK TABLES`-without-`UNLOCK` leaks and unescaped
   `_exit(0)` skipping log flush were both in scope for individual batches
   and were fixed where sampled.

## Waves and their closing status

Each wave below names the file-set a remediation batch owned and a one-line
closing status. The original per-finding itemized lists (roughly 180 entries
across `CRITICAL` down to `NIT`) have been removed from this document — once
a finding is fixed, a line-by-line record of "was a bug, now isn't" has
little ongoing value, and `git log`/`git diff` against the pre-remediation
tree is the authoritative record of exactly what changed and why. What
remains here is the map of what was covered, so a maintainer knows where to
look for the history of a given file.

- **Core infra** (`main.cpp`, `source.h`/`source.cpp`, `word.cpp`,
  `tokenize.cpp`, `utilities.cpp`, `DBUtility.cpp`, `mysqldb.h`) — hardcoded
  paths moved behind `envConfig`, the FATAL exit path fixed, serialize
  bounds-checking fixed, the query-buffer macros parenthesized, the SQL
  buffer lock now held across the query. Closing pass also removed three
  small unexplained dead-code blocks from `source.cpp` and corrected six
  stale "store-then-check" comments in `utilities.cpp` that described the
  pre-fix ordering (see "Dead-code sweep").
- **Question answering** (`questionAnswering.cpp`, `QuestionAnswering.h`,
  `questionProcessing.cpp`, `questionAnsweringWebSearch.cpp`) — hardcoded
  Google/Bing keys removed, several leaks (yajl trees, `wikiTableMap`
  pointers, transformed SRGs) fixed, multiple off-by-one/bounds bugs and the
  always-15 question-type remap fixed.
- **Ontology, HMM, specials** (`createOntology.cpp`, `ontology.h`,
  `hmm.cpp`, `hmm.h`, `specials_main.cpp`) — oversized stack buffers
  heap-allocated, unescaped SQL fixed, several lock-leak and bounds bugs
  fixed, the `hmm.h`/`hmm.cpp` declared-vs-defined arity mismatches
  realigned.
- **DB layer and shared headers** (`DB.cpp`, `DBCreateSQLSchema.cpp`,
  `DBWordRelations.cpp`, `tableColumn.*`, `dbQuerySearch.cpp`, `general.h`,
  `logging.*`, `profile.h`, `stacktrace.h`, `intArray.h`, `DIYDiskArray.h`,
  `bitObject.h`, `memoryStat.cpp`, `word.h`, `paice.*`,
  `conversationContext.cpp`, `bncc.h`, `Internet.cpp`/`internet.h`) —
  hardcoded MySQL credentials removed, a data-correctness column-mapping bug
  in `readMultiSourceObjects` fixed, several lock leaks and a UTF-16 BOM
  overlapping-`memcpy` bug fixed, `Loebner.cpp` (a comments-only stub)
  deleted.
- **Object and pronoun resolution** (`identifyObjects.cpp`,
  `resolveObjects.cpp`, `resolveMetaGroupObjects.cpp`,
  `resolveFirstSecondPersonPronouns.cpp`) — swapped-argument bug, an
  off-by-one pleonastic-it branch, a use-after-erase iterator bug, the
  `containingSpeakerGroup` span-vs-index inversion, several unguarded-index
  bugs fixed.
- **Pattern engine** (`definePatterns.cpp`, `pattern.*`,
  `patternMatchArray.*`, `patternElementMatchArray.*`) — the
  `reserve`-not-`resize` usage-counter bug, the `setMandatoryAncestorPatterns`
  wrong-bitset bug, a `CHILDPATBITS` enum/shift-constant collision, several
  self-assignment and format-string bugs fixed; two dead `findPattern`
  overloads, two phantom declarations, and an infinite-loop dead function
  removed.
- **Semantic relations, time, names** (`semanticRelations.*`,
  `timeRelations.*`, `names.*`) — the `months_abb` regression correctly
  restored (see "Cross-cutting conclusions" above), `speakerGroupTransition`
  direction bug fixed, `cName::notNull()` inversion fixed, several
  unbounded-index and precedence bugs fixed.
- **Data acquisition** (`getWikipedia.cpp`, `getWordNet.cpp`,
  `getThesaurus.cpp`, `getDictionary.cpp`, `initializeDictionary.cpp`,
  `getMusicBrainz.*`, `getTwitter.cpp`, `getWordNetMaps.cpp`,
  `tagOperations.cpp`, `vcXML.*`, `relationTypes.h`) — the Merriam-Webster
  key removed, two NULL-dereference crashes fixed
  (`getWikipedia.cpp`/`vcXML.cpp`), HTTPS + URL-encoding fixes, several
  buffer/handle leaks fixed, the `getMusicBrainz.h`/`.cpp` arity mismatch
  fixed via a default argument. `Adversary bugs.cpp` confirmed to actually be
  referenced in `lp.vcxproj` (contrary to an earlier assumption) and left
  alone.
- **Satellite tools** (`processGutenbergRDFtoSQL`, `Web/pyLPBackEnd`,
  `convertPDFTextToDatabase`, `checkTypes`, `correctRDF`,
  `DirectoryAnalysis`, `LPWeb`) — hardcoded credentials and a Flask secret
  key removed, SQL injection via string concatenation fixed with
  parameterized queries, an unbounded `strcat` fixed with bounds checks, a
  buffer-size typo and an output-order bug fixed in `correctRDF`, a
  wrong-variable file-count bug fixed in `DirectoryAnalysis`. The entire
  `unused source/` tree (12 files, including the file that held the NewsBank
  credential and a `Source, ::` syntax error that would not compile) was
  deleted, along with `TextRendererWorkspace/` and stray tracked files under
  `workspace/`/`packages/`.
- **Two newly-discovered first-party Java trees**, `TextRenderer/` and
  `show/`, were not in the original review's scope but were found and fixed:
  `show/`'s binary-cache readers (`Relation.java`, `CObject.java`) had
  bit-offset bugs silently misreading flags by 1–3 bit positions on every
  cached read.
- **Agreement and syntactic relations** (`agreement.cpp`,
  `syntacticRelations.*`, `syntacticRelationGroups.cpp`) — the
  highest-traffic winnow-path `reserve`-not-`resize` bug fixed, an unbounded
  `setRole` backward walk capped, a Watson `objects[-1]` bug fixed, several
  `&&`/`||` precedence bugs fixed, a `getRelStr` enum-decomposition bug
  affecting generated DB schema fixed.
- **Speaker groups and speaker resolution**
  (`identifySpeakerGroups.cpp`, `resolveSpeakers.cpp`) —
  `sameSpeaker`'s inverted one-sided tests fixed, an unbounded deserializer
  fixed, and (`resolveSpeakers.cpp`, the largest file in the repository at
  ~10,500 lines) a subtle cross-container iterator bug where `set::find()`
  was compared against a *different* set's `.end()` — silently breaking
  previous-subject speaker attribution — plus seven other bugs.

## Verification methodology (this closing pass)

No individual batch's work was re-litigated (the `months_abb` restoration,
the SRG ownership fix, the cross-container iterator fix, etc. were already
carefully reasoned through and spot-checked at the time). This pass instead
re-verified a broad, independent sample against the current source — over 45
individual findings, spread across every wave and every severity level from
`CRITICAL` down to `NIT`, including (non-exhaustively): the `envConfig`
path/credential indirection; the FATAL-exit fix and its lock-held-across-query
counterpart in `DBUtility.cpp`; the Google/Bing key removal; `cProximityEntry`
and the question-type remap; the three oversized-buffer heap-allocations in
`createOntology.cpp`; the `hmm.cpp` NaN test and `specials_main.cpp`
case-71 fallthrough; hardcoded-credential removal in `DB.cpp`/`getThesaurus.cpp`;
the `readMultiSourceObjects` column mapping; the paice.cpp BOM `memmove`; the
`identifyObjects.cpp` swapped-argument bug; `containingSpeakerGroup`; the
`pattern.h` usage-counter fix; `months_abb`/`months_abb_index`; "twentieth"
mapping to 20; the Merriam-Webster key and both `getWikipedia.cpp`/`vcXML.cpp`
NULL-dereference fixes; secret removal across `processGutenbergRDFtoSQL` and
`pyLPBackEnd`; `minSeparatorCost.resize`; the `setRole` bounded walk; the
`sameSpeaker`, `oStr[0]`, `preferPreviousSpeaker`, and `_VERBPAST` audience-scan
items in the speaker-resolution files; a dozen further items across
`patternMatchArray.cpp`, `agreement.cpp`, `names.cpp`, `word.h`,
`timeRelations.h`, `createOntology.cpp`'s `decodeURL`/`find` bugs,
`DBCreateSQLSchema.cpp`, and the Python satellite tree
(`pyLP.py`'s per-session uid, `LPIO.py`'s short-read guard, the
Java-leftover fixes in `VerbNet.py`/`TimeInfo.py`/`BitObject.py`/`WordMatch.py`).

Every one of those checks confirmed the finding is fixed as described, with
one partial exception noted above (`cWordGroup`/`ACCUMULATE_GROUPS`, which
was improved but honestly left incomplete because finishing it is a design
task, not a bug fix) and zero cases of a finding being falsely marked fixed
or a fix regressing. Given that hit rate across a sample this size and this
spread, the remaining ~135 unsampled findings are treated as resolved on the
strength of the batch reports and the orchestrator's own contemporaneous
spot-checks, not re-verified individually here — see the "Open items"
section above for the exceptions.

## Dead-code sweep (this closing pass)

Beyond what individual batches removed within their own file sets, this
closing pass did a final cross-cutting sweep for (a) header-declared symbols
with no definition anywhere, (b) functions defined with zero callers
anywhere in the repo, and (c) unexplained commented-out code blocks. Scope
was `general.h`, `source.cpp`, `word.cpp`, `tokenize.cpp`, and the satellite
tool trees — the rest of the engine already got a thorough close-read from
its owning batch. Findings, all fixed except where noted as intentionally
kept:

- `general.h`: removed `positionConsole(bool controller)`, a declaration
  with no definition anywhere and no caller — console positioning is
  actually implemented inline in `main.cpp`. Removed a duplicate
  `setString(set<wstring>&, wstring&, const wchar_t*)` declaration
  (the same signature was declared twice). Removed a stale,
  unexplained commented-out declaration for an old free-function
  `getSynonyms(wstring, set<wstring>&, int, sTrace&)` signature, long since
  superseded by the current `cSource::getSynonyms` member functions.
- `source.cpp`: removed three unexplained commented-out blocks — a
  "TEMP WRITE" debug print gated on three specific words (`fewer`/`afore`/
  `thyself`) in `writeWords()`; a debug log gated on a magic source position
  (`childWhere==886`); and an orphaned `if (!keepObjects) { setObject(-1);
  objectMatches.clear(); }` block that directly contradicted the
  `keepObjects = false;` assignment immediately above it with no explanation
  of why it had been disabled. All three were pure comments (already inert)
  and removing them has zero behavior effect.
- `word.cpp`/`word.h`: identified six public member functions
  (`cSourceWordInfo::computeDBUsagePatternsToUsagePattern`, `isLowestCost`,
  `patternString`, `removeIllegalForms`; `cWord::addInflectionFlag`,
  `removeInflectionFlag`) that are declared and defined but have zero callers
  anywhere in the repo. **Not deleted** — each reads as deliberate,
  symmetric public API surface (an add/remove or is-X/set-X pair matching
  the pattern used throughout `cWord`/`cSourceWordInfo`, e.g.
  `addInflectionFlag`/`removeInflectionFlag` are a matched pair and both are
  currently unused) rather than debris, so removing working code on that
  basis felt like overreach for a documentation/hygiene pass. Flagged here
  (`NIT`) for a maintainer to judge; `pattern.cpp`'s `allWordFlags(int,
  wstring&)` (a complete,
  self-contained `cSourceWordInfo`-flag debug formatter, parallel to the
  actively-used `inflectionFlagsToStr`) is the same situation and was
  likewise left in place.
- `tokenize.cpp`: no dead code found.
- `show/` (Java): deleted `FindMenu.java` — a complete `JMenu` subclass
  implementing `ActionListener`/`MenuListener` that is never instantiated
  anywhere in the package, and which itself contained several more
  unexplained commented-out listener-stub methods (for interfaces the class
  doesn't even declare implementing). No build file enumerates source files
  by name (Maven `pom.xml` compiles the whole `src` directory), so no other
  file needed updating.
- `convertPDFTextToDatabase/.../Source.cpp`: removed a phantom
  `scrapeNewThesaurus(wstring, int, vector<sDefinition>&)` forward
  declaration (never defined anywhere in the file) and its orphaned,
  commented-out call site (`//scrapeNewThesaurus();`) — the live
  `scrapeThesaurus` (no "New") on the line above is what's actually called.
- Checked and found clean: `correctRDF.cpp`, `checkTypes.cpp`,
  `processGutenbergRDFtoSQL/Program.cs`, `DirectoryAnalysis/Program.cs`,
  `LPWeb/*.cs`, `TextRenderer/src/RenderToText.java`, and every module under
  `Web/pyLPBackEnd` (cross-referenced — every class/module is used by at
  least one other file).
- Repo-wide grep for dangling references to symbols named as removed in
  individual batch reports (`Loebner`, `getTimeStamp`, `skipMetaResponse`,
  `querySingleNoun`, `findObjectElement`) found zero live references. One
  hit for `getTimeStamp` inside an already-commented-out debug line in
  `DB.cpp` (itself dead, inert, and out of this pass's scope since `DB.cpp`
  already got a thorough close-read from its owning batch).

## Final secrets grep (this closing pass)

Fresh repo-wide grep (excluding `.git` and the vendored directories listed
above) for the specific hardcoded-secret literals named in the original
review: the MySQL root password, the Google CSE key, the Bing v7 key, the
Merriam-Webster key, the Twitter account password, and the NewsBank library
card. Also a generic `password\s*=\s*["']` / `secret_key\s*=\s*["']` /
`api[_-]?key\s*=\s*["']` sweep across every `.cs`/`.py`/`.java` file in the
satellite trees.

**Result: zero hits in the working tree**, outside of this document's own
historical text (now scrubbed to placeholders — see below) and
`PROPOSED_FIXES.md` (which never quoted the literals). The NewsBank library
card is confirmed gone entirely — it lived only in
`unused source/getNewsbank.cpp`, which was deleted in batch I.

All of the MySQL password, Google CSE key, Bing key, and Merriam-Webster key
literals **do** still appear in this repository's git history (pre-dating
the batches that removed them, plus this document's own earlier commits that
quoted them as findings). That is expected, out of scope for this effort,
and not something this pass attempted to fix — rewriting history was
explicitly out of bounds. If these credentials were ever live, treat them as
compromised regardless of what the working tree says now, and rotate them.

The three secret-shaped literals that were pasted directly into this
document's own text (as examples/quotes of the original findings) have been
replaced with placeholders in the rewrite above: the Google CSE key is now
referred to only as "the Google CSE key," the Bing key as "the Bing key,"
and the Merriam-Webster key as "the Merriam-Webster key," with no literal
value quoted anywhere in this file.

## Project-file consistency (this closing pass)

Checked every `<ClCompile Include="...">` / `<ClInclude Include="...">` in
`lp.vcxproj`, `lp.vcxproj.filters`, `specials.vcxproj`, and
`specials.vcxproj.filters` for two things: does the referenced file exist on
disk, and is there any leftover reference to a file a batch deleted
(`Loebner.cpp`, anything under the old `unused source/`,
`evaluateTagSetCombinatorials.cpp`, `getBNC.cpp`).

Found and fixed one mismatch: `lp.vcxproj` and `lp.vcxproj.filters` both
referenced `leethomason-tinyxml2-a9cf3f9\tinyxml2.h` for the tinyxml2
header — that directory doesn't exist (the vendored tree lives at
`tinyxml2-master\`, and the matching `ClCompile` entry for `tinyxml2.cpp`
already correctly pointed there). Both files now reference
`tinyxml2-master\tinyxml2.h`, matching where the file actually is.

No other mismatches: every other `Include` path in both project files (150
entries total) resolves to a real file, and there are zero leftover
references to any previously-deleted file. `envConfig.cpp`/`envConfig.h`
(new in batch A0) are correctly wired into both `lp.vcxproj` and
`specials.vcxproj`. Every top-level first-party `.cpp`/`.h` file in
`source/` is referenced by at least one of the two project files (checked
the reverse direction too — no orphaned-from-the-build source file).
