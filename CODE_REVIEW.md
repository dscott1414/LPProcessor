# LPProcessor code review — open items

This is what remains of a code-review effort on this repository. The findings it
raised have been applied; the record of *what* changed and *why* is in `git log`
and `git diff`, which is a better place for it than prose, so the narrative that
used to fill this file has been removed rather than duplicated. What is kept here
is only the part that is still actionable: things a maintainer needs to know or
decide, none of which a commit message can tell you.

For the macOS port of the same tree, see `MAC_PORT.md`.

## Scope

Vendored third-party trees are out of scope and untouched: `lloyd-yajl-66cb08c/`,
`tinyxml2-master/`, `packages/`, and all of `HMM/`. `lists/` is runtime data, not
code. `stacktrace.h` is vendored third-party (Sean Farrell, MIT) and is now
unused — the port replaced its only consumer with `backtrace()` — and is kept on
disk only for the unmaintained Windows project.

## Open items

Four. Each needs a decision or an investigation that a review pass cannot make
on the author's behalf.

1. **`cWord::getForms()` has no dictionary source, so unknown words acquire no
   forms.** The most consequential item here. Form discovery used to come from the
   Merriam-Webster Collegiate API, which has been removed at the author's request;
   nothing replaced it, so the function now returns `WORD_NOT_FOUND` for every
   word. `discoverInflections`, `identifyFormClass` and `addNewOrModify` are all
   intact and still work — they simply have no part-of-speech set to work from.
   `getWNForms()` in the same file already returns WordNet parts of speech and has
   no callers, which makes it the obvious candidate, but wiring it in changes which
   words the parser will learn and is therefore a deliberate decision, not a
   drop-in. (It survived the dead-code sweep below for exactly this reason.) The
   same gap exists in `specials_main.cpp`'s `getWordPOS()`, which now does nothing
   but set `isNonEuropean` — its dictionary.com existence check has since been
   removed too.

2. **`ACCUMULATE_GROUPS` is an unfinished feature, not a bug.**
   In `syntacticRelations.h`/`.cpp`. `cSourceWordInfo::addRelation()`'s
   `cWordGroup(this, fromWord, word, toWord)` call matches no constructor in the
   class — `this` is a `cSourceWordInfo*`, not the `tIWMM` the constructors
   expect. The whole path is `#ifdef ACCUMULATE_GROUPS`'d out and has never been
   linked into any build on any platform. **Decision needed: finish the
   word-clustering feature, or delete it.** Designing a constructor for a feature
   nobody has specified is not something a review pass should guess at. Zero
   regression risk either way.

3. **`cSource::determineTimelineSegmentLink()` is a stub returning `false`.**
   `timeRelations.cpp`. Two live callers assign its result to `ts.linkage`, so
   timeline segments are currently never linked. Implementing it depends on what
   "linkage" is supposed to mean — author intent, not a fix.

4. **`LPWeb/Startup.cs` calls `UseAuthorization()` with no authentication
   middleware registered**, which makes it a no-op. `LPWeb` is a personal
   read-only viewer, so this may well be fine — it is recorded only because it
   stops being fine the moment the viewer is exposed beyond localhost.

## Standing security note

Credentials that were once hardcoded in source — the MySQL `root` password,
Google CSE and Bing API keys, and others — were removed from the working tree and
replaced by environment variables (`envConfig.h`/`.cpp`, with no hardcoded
default). A repo-wide grep for the literal values finds zero hits.

**They remain in git history**, which was out of scope to rewrite. If any of them
were ever live, treat them as compromised and rotate them on the actual services.
Removing a secret from the tip of a branch does not un-publish it.

## Removed integrations

Twitter, NewsBank, Merriam-Webster, thesaurus.com and dictionary.com support has
been removed from the tree entirely. Several consequences are worth knowing:

- `cSource::sourceTypeEnum` retains a `RETIRED_SOURCE_TYPE_3` placeholder where
  `NEWS_BANK_SOURCE_TYPE` used to be. **Do not delete or reorder it.** These
  ordinals are written into the `sources.sourceType` column and are baked into
  existing rows and binary source caches; removing the member would silently
  renumber every type after it.
- The Merriam-Webster removal is what created Open item 1 above.
- **`getSynonyms()` is now WordNet-only.** It used to fall back to a MySQL
  `thesaurus` table and then to scraping thesaurus.com. `getThesaurus.cpp` is
  deleted, along with `scrapeNewThesaurus`, `getSynonymsFromDB` and the
  `sDefinition` record they filled. A word absent from WordNet now yields no
  synonyms instead of reaching the network. The hand-maintained `synonymMap` and
  `synonymDeletionMap` overrides still apply.
- **`cWord::illegalWord()` no longer consults dictionary.com**, which means **more
  words now count as legal than did before.** This one has teeth: `illegalWord` is
  called from `identifyObjects.cpp` on the live parse path, so it changes what the
  parser accepts, not just what a batch scan records. Its `mysql` parameter is now
  unused and is kept only so the `word.h` signature is unchanged.
- `-specials` step 4 (the Dictionary.com cache sweep that populated `notwords`) is
  gone, matching how step 71 was retired earlier.
- `LP_DB_HOST` is gone. `getDBHost()` was never called by anything — the database
  host comes from the `-server` command-line flag, which defaults to `localhost`.
  The environment variable had simply never been wired up.

## Uncalled code

Every function in the tree that had no call site has been removed — 147 in total.
Method: enumerate defined symbols from the object files, classify every
occurrence of each name in the source as definition / declaration / call, and
remove only those with zero calls, hand-verifying each. Linker dead-strip maps
(built with inlining disabled, so inlined functions are not mistaken for dead
ones) were used to narrow the field. The sweep was repeated to a fixed point,
because removing a function usually strips its callees too — most of
`getThesaurus.cpp`'s helpers only died once their batch driver went.

Deliberately **kept**, because "no call site" does not mean unused:

- Functions passed as pointers rather than called — the `sort()` comparators
  (`oCompare`, `salienceCompare`, `frequencyCompare`, `compareTagLocation`,
  `sortRuleGreater`, `sectionCompareBySpeakerMatches`, `comparesr`, `ci_equal`,
  `masterCompare`, `sortWebQueryStrings`) and `no_memory`, which is handed to
  `set_new_handler`.
- Constructors, which are invoked implicitly.
- `getWNForms()`, which is the intended fix for Open item 1.
- `removeDots()`, which was corrected on request (see below) and is retained
  deliberately even though nothing calls it yet.

**A consequence worth knowing.** Removing the Merriam-Webster integration made
`cWord::getForms()` a stub, and that in turn made its whole support cast
uncalled: `discoverInflections` (which encoded how English inflects each open
word class), `identifyFormClass` (which mapped dictionary part-of-speech strings
onto form ids), and `getInflection`. All three are gone. They were real domain
logic, not scaffolding, so restoring unknown-word discovery now means writing
that layer again rather than re-pointing it at a new source — or recovering it
from git history on `buildable-on-mac`.

## removeDots

`getDictionary.cpp`'s `removeDots()` now removes dots. It previously searched for
U+FFFD REPLACEMENT CHARACTER, which is what the source bytes literally contained
— and had contained since before the macOS port; as a narrow multi-character
literal it had never had a meaningful value under MSVC either, so the function
had never once removed what its name and its own comment ("erases middots") said.

It now strips the three characters dictionary markup actually uses as syllable
separators inside a headword: U+00B7 MIDDLE DOT, U+2027 HYPHENATION POINT, and
U+2022 BULLET. The ASCII full stop is deliberately **not** removed — it is not a
syllable separator, and stripping it would corrupt abbreviations ("U.S.", "etc.")
and every sentence boundary in the surrounding text. Verified with a
compiled-and-run check covering each separator, all three mixed, the ASCII-period
negative case, and empty/separator-only input.

## Verification standard

The whole first-party engine compiles under clang with `-Wall -Wextra`,
`liblpcore.a` archives, and both `lp` and `CorpusAnalysis` link and run. So every
claim about this code now has the compiler behind it, which is what caught
several defects that reading passes had missed — a `cName::isNull()` declared
nowhere, a `getNumSourcesProcessed` that `CorpusAnalysis` referenced but no
linked translation unit defined, and a long tail of type errors that the untyped
`windows.h` `min`/`max` macros had been hiding.

Two limits on that, and they matter:

**The corpus now runs; nothing compares it to Windows.** 14 of the 23 documents in
`tests/` parse end to end at 98-100% matched sentences (see `MAC_PORT.md`,
"Corpus status"). Getting there required fixing four defects the run itself
exposed, which is exactly what it was for. But `tests/` is a corpus, not a test
suite: nothing asserts what a document *should* score, and no result has been
compared against the same document's score on Windows. A regression that lowered
match rates without crashing would still pass unnoticed.

**Not every finding was individually re-verified.** A broad sample across every
category was checked against the source and found correct; the remainder rests on
that hit rate. Treat the code, not any document, as authoritative.

The single highest-value thing anyone can do next is decide the start-marker
question in `MAC_PORT.md`'s "Corpus status" -- it is what stops the remaining 9
documents -- and then capture the Windows scores for these same documents so the
corpus becomes an oracle rather than a smoke test.
