#!/usr/bin/env python3
"""Enumerate every port-era LOGIC change in cache-relevant code.

semdiff.py canonicalizes away the port's dialect; this goes one step further and
keeps only hunks where the surviving difference is a *decision*: a changed
condition, index, comparison or constant. Pure deletions (dead-code sweeps) and
pure insertions (added bounds guards) are reported separately, because they are
a different kind of risk from a silently altered predicate.

Baseline is a836a35 (2022-05-28), the last of the author's Windows commits, so
everything reported here was introduced by port-era work.
"""
import re, difflib, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, "semdiff.py")).read().replace("\nmain()\n", "\n")
ns = {}
exec(compile(src, "semdiff.py", "exec"), ns)
canon, blob = ns["canon"], ns["blob"]

BASE = "a836a35"
RELEVANT = """identifyObjects.cpp resolveObjects.cpp resolveMetaGroupObjects.cpp
pattern.cpp patternMatchArray.cpp patternElementMatchArray.cpp definePatterns.cpp
source.cpp source.h tokenize.cpp word.cpp word.h names.cpp agreement.cpp
syntacticRelations.cpp syntacticRelationGroups.cpp semanticRelations.cpp
resolveSpeakers.cpp identifySpeakerGroups.cpp timeRelations.cpp
questionProcessing.cpp pattern.h initializeDictionary.cpp""".split()

# a line that encodes a decision
DECIDE = re.compile(r"\b(if|while|for|return|else)\b|[<>!=]=|&&|\|\||\[[^\]]*[+-][^\]]*\]")


def is_decision(lines):
    return any(DECIDE.search(l) for l in lines)


def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    n_logic = n_del = n_ins = 0
    for path in RELEVANT:
        if only and path != only:
            continue
        a, b = blob(BASE, path), None
        try:
            b = open(f"/Users/davidscott/lp/source/{path}", encoding="utf-8", errors="replace").read()
        except FileNotFoundError:
            pass
        if a is None or b is None:
            continue
        ca, cb = canon(a), canon(b)
        sm = difflib.SequenceMatcher(None, ca, cb, autojunk=False)
        hunks = []
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                continue
            old, new = ca[i1:i2], cb[j1:j2]
            if tag == "replace" and is_decision(old + new):
                hunks.append(("LOGIC", old, new))
            elif tag == "delete":
                hunks.append(("deleted", old, []))
            elif tag == "insert":
                hunks.append(("inserted", [], new))
            else:
                hunks.append(("other", old, new))
        logic = [h for h in hunks if h[0] == "LOGIC"]
        if not logic and not only:
            continue
        n_logic += len(logic)
        n_del += len([h for h in hunks if h[0] == "deleted"])
        n_ins += len([h for h in hunks if h[0] == "inserted"])
        print(f"\n{'='*78}\n{path}   {len(logic)} logic hunk(s), "
              f"{len([h for h in hunks if h[0]=='deleted'])} deletion(s), "
              f"{len([h for h in hunks if h[0]=='inserted'])} insertion(s)\n{'='*78}")
        for kind, old, new in (hunks if only else logic):
            if kind != "LOGIC" and not only:
                continue
            print(f"  --- {kind} ---")
            for x in old[:8]:
                print(f"    2022| {x[:145]}")
            for x in new[:8]:
                print(f"    HEAD| {x[:145]}")
            print()
    print(f"\n{'='*78}\nTOTAL: {n_logic} logic hunks, {n_del} deletions, {n_ins} insertions")


main()
