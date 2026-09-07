#!/usr/bin/env python3
"""Classify each port-era logic hunk by the kind of risk it carries.

Three classes, in ascending order of consequence for parity:

  GUARD      HEAD only adds a bounds/null test to an otherwise identical line.
             Changes behaviour only where the 2022 code was reading out of
             bounds, i.e. at the edges. Keep: these are the crash fixes.

  PRECEDENCE HEAD only adds or moves parentheses. Changes grouping, so it can
             change behaviour everywhere the expression is evaluated.

  BEHAVIOUR  The predicate, an index, a constant or a variable changed. These
             alter decisions on ordinary input and are the ones that can move
             object identification across the whole book.

Baseline a836a35 (the author's last Windows commit), so everything here is
port-era.
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

DECIDE = re.compile(r"\b(if|while|for|return|else)\b|[<>!=]=|&&|\|\||\[[^\]]*[+-][^\]]*\]")
# a clause that is purely a bounds/null test
GUARD = re.compile(
    r"^[\w.>\-\[\]()]*\s*(<|<=|>=|>|!=|==)\s*[\w.>\-\[\]()]*$|"
    r"^!?\w[\w.>\-]*\.(empty|size)\(\)$|^\w[\w.>\-]*\s*(>=|>)\s*0$")


def declause(line):
    """Split a condition into its && / || operands, normalized."""
    body = line
    m = re.search(r"\b(if|while)\s*\((.*)\)\s*$", line)
    if m:
        body = m.group(2)
    parts = re.split(r"&&|\|\|", body)
    return [p.strip().strip("()").strip() for p in parts if p.strip()]


WIN32IO = re.compile(r"\b(WriteFile|ReadFile|CreateFileW|CloseHandle|HANDLE|DWORD|"
                    r"INVALID_HANDLE_VALUE|GENERIC_(READ|WRITE)|FILE_(SHARE|ATTRIBUTE)_\w+|"
                    r"CREATE_ALWAYS|OPEN_EXISTING|dwBytesWritten|bytesWritten)\b")
POSIXIO = re.compile(r"\b(O_(RDONLY|WRONLY|CREAT|TRUNC|BINARY)|ssize_t|::(open|read|write))\b|"
                     r"\b(open|read|write)\s*\(")


def classify(old, new):
    o, n = " ".join(old), " ".join(new)
    if WIN32IO.search(o) and (POSIXIO.search(n) or not n.strip()):
        return "PORT_IO"
    if re.sub(r"[()]", "", o) == re.sub(r"[()]", "", n) and o != n:
        return "PRECEDENCE"
    # guard: every clause of old survives in new; new's extra clauses are all guards
    oc, nc = set(), set()
    for l in old:
        oc |= set(declause(l))
    for l in new:
        nc |= set(declause(l))
    added, removed = nc - oc, oc - nc
    if not removed and added and all(GUARD.match(a) for a in added):
        return "GUARD"
    # also a guard if new merely prefixes an early-return bounds test
    if len(new) == len(old) + 1 and new[1:] == old and re.match(
            r"^if\s*\(.*\)\s*return (false|true|-1|0);$", new[0]) and GUARD.match(
            declause(new[0])[0] if declause(new[0]) else ""):
        return "GUARD"
    return "BEHAVIOUR"


rows = {"BEHAVIOUR": [], "PRECEDENCE": [], "GUARD": [], "PORT_IO": []}
for path in RELEVANT:
    a = blob(BASE, path)
    try:
        b = open(f"/Users/davidscott/lp/source/{path}", encoding="utf-8", errors="replace").read()
    except FileNotFoundError:
        continue
    if a is None:
        continue
    ca, cb = canon(a), canon(b)
    sm = difflib.SequenceMatcher(None, ca, cb, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag != "replace":
            continue
        old, new = ca[i1:i2], cb[j1:j2]
        if not any(DECIDE.search(l) for l in old + new):
            continue
        rows[classify(old, new)].append((path, old, new))

for k in ("BEHAVIOUR", "PRECEDENCE", "GUARD", "PORT_IO"):
    print(f"\n{'#'*78}\n# {k}: {len(rows[k])} hunk(s)\n{'#'*78}")
    if k in ("GUARD", "PORT_IO"):
        from collections import Counter
        for f, c in Counter(p for p, _, _ in rows[k]).most_common():
            print(f"    {c:>3}  {f}")
        continue
    for path, old, new in rows[k]:
        print(f"\n  [{path}]")
        for x in old[:6]:
            print(f"    2022| {x[:140]}")
        for x in new[:6]:
            print(f"    HEAD| {x[:140]}")

print(f"\n\nTOTALS  behaviour={len(rows['BEHAVIOUR'])}  "
      f"precedence={len(rows['PRECEDENCE'])}  guard={len(rows['GUARD'])}  "
      f"port_io={len(rows['PORT_IO'])}")
