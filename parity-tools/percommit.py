#!/usr/bin/env python3
"""Which port-era commits changed BEHAVIOUR in cache-relevant code?

The reference .SourceCache was written by 38aa63d (2022-05-09). Everything since
a836a35 (2022-05-28, the last of the author's Windows commits) is port-era work.
Most of it is the mechanical dialect shift and added comments, which cannot
change the cache. This walks each port-era commit against its parent, canonicalizes
both sides the way semdiff.py does, and reports the residual -- so the commits that
actually changed behaviour stand out from the ones that only renamed things.
"""
import re, subprocess, sys, difflib, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("SD_REF", "x")
import importlib.util
spec = importlib.util.spec_from_file_location(
    "sd", os.path.join(os.path.dirname(os.path.abspath(__file__)), "semdiff.py"))

# reuse the canonicalizer without running semdiff's main()
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "semdiff.py")).read()
src = src.replace("\nmain()\n", "\n")
ns = {}
exec(compile(src, "semdiff.py", "exec"), ns)
canon, blob = ns["canon"], ns["blob"]

REPO = "/Users/davidscott/lp/source"
RELEVANT = set("""resolveMetaGroupObjects.cpp resolveSpeakers.cpp agreement.cpp
syntacticRelations.cpp source.cpp resolveObjects.cpp names.cpp word.cpp source.h
semanticRelations.cpp identifyObjects.cpp pattern.cpp definePatterns.cpp
identifySpeakerGroups.cpp tokenize.cpp patternMatchArray.cpp word.h pattern.h
patternElementMatchArray.cpp questionProcessing.cpp syntacticRelationGroups.cpp
timeRelations.cpp initializeDictionary.cpp""".split())


def residual_between(a_rev, b_rev, path):
    a, b = blob(a_rev, path), blob(b_rev, path)
    if a is None or b is None:
        return 0
    ca, cb = canon(a), canon(b)
    sm = difflib.SequenceMatcher(None, ca, cb, autojunk=False)
    same = sum(bl.size for bl in sm.get_matching_blocks())
    return max(len(ca), len(cb)) - same


commits = subprocess.run(
    ["git", "log", "--reverse", "--format=%h|%ad|%s", "--date=short", "a836a35..HEAD"],
    cwd=REPO, capture_output=True).stdout.decode().strip().split("\n")

rows = []
for line in commits:
    h, d, subj = line.split("|", 2)
    files = subprocess.run(["git", "diff", "--name-only", f"{h}^", h],
                           cwd=REPO, capture_output=True).stdout.decode().split()
    touched = [f for f in files if f in RELEVANT]
    if not touched:
        continue
    tot = 0
    per = {}
    for f in touched:
        r = residual_between(f"{h}^", h, f)
        if r:
            per[f] = r
        tot += r
    if tot:
        rows.append((tot, h, d, subj, per))

rows.sort(reverse=True)
print("port-era commits that changed BEHAVIOUR in cache-relevant code")
print("(canonicalized: renames, reformatting and comments removed)\n")
print(f"  {'lines':>6}  {'commit':<9} {'date':<11} subject")
print("  " + "-" * 76)
for tot, h, d, subj, per in rows:
    print(f"  {tot:>6}  {h:<9} {d:<11} {subj[:48]}")
    for f, n in sorted(per.items(), key=lambda x: -x[1])[:4]:
        print(f"          {n:>5}  {f}")
print("  " + "-" * 76)
print(f"  {sum(r[0] for r in rows):>6}  TOTAL across {len(rows)} commits")
