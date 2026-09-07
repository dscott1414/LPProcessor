#!/usr/bin/env python3
"""Separate the port's mechanical rename from genuine changes.

The reference .SourceCache was written by 38aa63d (2022-05-09). Diffing that
against HEAD shows ~30k changed lines, but most of it is the port's mechanical
dialect shift (L"" -> u"", wchar_t -> lpchar_t, wcslen -> lp_strlen, ...) plus
comments this work added. Canonicalize BOTH sides into a neutral dialect, strip
comments and whitespace, and whatever still differs is a real change.

    semdiff.py                 rank every file by residual semantic difference
    semdiff.py <file.cpp>      show the residual hunks for one file
"""
import re, os, subprocess, sys, difflib

REPO = "/Users/davidscott/lp/source"
REF = os.environ.get("SD_REF", "38aa63dc8f39d400d623d472a3f7f8bd3426fe21")
HEADREV = os.environ.get("SD_HEAD", "HEAD")

# both spellings collapse to one neutral token
PAIRS = [
    (r"\bwcslen\b|\blp_strlen\b", "STRLEN"),
    (r"\bwcschr\b|\blp_strchr\b", "STRCHR"),
    (r"\bwcsrchr\b|\blp_strrchr\b", "STRRCHR"),
    (r"\bwcsstr\b|\blp_strstr\b", "STRSTR"),
    (r"\bwcscmp\b|\blp_strcmp\b", "STRCMP"),
    (r"\bwcsncmp\b|\blp_strncmp\b", "STRNCMP"),
    (r"\bwcscpy(_s)?\b|\blp_strcpy\b", "STRCPY"),
    (r"\bwcsncpy(_s)?\b|\blp_strncpy\b", "STRNCPY"),
    (r"\bwcsnlen\b|\blp_strnlen\b", "STRNLEN"),
    (r"\b_?wcsicmp\b|\bwcscasecmp\b|\blp_wcscasecmp\b", "STRICMP"),
    (r"\b_?wcsnicmp\b|\bwcsncasecmp\b|\blp_strncasecmp\b", "STRNICMP"),
    (r"\bwsprintf\b", "SNPRINTF"),
    (r"\bwcscat\b|\blp_strcat\b", "STRCAT"),
    (r"\bwmemset\b|\blp_memset\b", "MEMSET"),
    (r"\bwcstok(_s)?\b|\blp_wcstok\b", "STRTOK"),
    (r"\b_wtoi\b|\blp_wtoi\b|\bwtoi\b", "TOI"),
    (r"\b_wtoi64\b|\b_wtoll\b|\blp_wtoll\b|\bwtoll\b", "TOLL"),
    (r"\b_itow(_s)?\b|\blp_itow\b", "ITOW"),
    (r"\bswprintf(_s)?\b|\b_snwprintf(_s)?\b|\blp_snprintf\b|\blp_wsprintf\b", "SNPRINTF"),
    (r"\bvswprintf(_s)?\b|\b_vsnwprintf(_s)?\b|\blp_vsnprintf\b", "VSNPRINTF"),
    (r"\bwprintf\b|\blp_wprintf\b", "WPRINTF"),
    (r"\bfwprintf\b|\blp_fwprintf\b", "FWPRINTF"),
    (r"\bswscanf(_s)?\b|\blp_swscanf\b", "SSCANF"),
    (r"\bfgetws\b|\blp_fgetws16?\b", "FGETWS"),
    (r"\b_wfopen(_s)?\b|\blp_wfopen\b", "FOPEN"),
    (r"\b_wopen\b|\blp_wopen\b", "OPEN"),
    (r"\b_waccess(_s)?\b|\blp_waccess\b", "ACCESS"),
    (r"\b_wremove\b|\blp_wremove\b", "REMOVE"),
    (r"\b_wmkdir\b|\blp_wmkdir\b", "MKDIR"),
    (r"\b_filelength(i64)?\b|\blp_filelength\b", "FILELENGTH"),
    (r"\btowlower\b|\blp_towlower\b", "TOLOWER"),
    (r"\btowupper\b|\blp_towupper\b", "TOUPPER"),
    (r"\bwchar_t\b|\blpchar_t\b", "WCH"),
    (r"\bwstring\b|\blpwstring\b", "WSTR"),
    (r"\bwifstream\b|\blpwifstream\b", "WIFSTREAM"),
    (r"\b__int64\b|\bint64_t\b", "I64"),
    (r"\bunsigned __int64\b|\buint64_t\b", "U64"),
    (r'\bL"', 'Q"'), (r"\bL'", "Q'"),
    (r'\bu"', 'Q"'), (r"\bu'", "Q'"),
]


def strip_comments(t):
    out = []; i = 0; n = len(t); st = None
    while i < n:
        c = t[i]; nx = t[i + 1] if i + 1 < n else ""
        if st is None:
            if c == "/" and nx == "/": st = "line"; i += 2; continue
            if c == "/" and nx == "*": st = "block"; i += 2; continue
            out.append(c)
            if c == '"': st = "str"
            i += 1
        elif st == "line":
            if c == "\n": st = None; out.append(c)
            i += 1
        elif st == "block":
            if c == "*" and nx == "/": st = None; i += 2; continue
            if c == "\n": out.append(c)
            i += 1
        else:
            out.append(c)
            if c == "\\": out.append(nx); i += 2; continue
            if c == '"': st = None
            i += 1
    return "".join(out)


def canon(text):
    t = strip_comments(text)
    for pat, rep in PAIRS:
        t = re.sub(pat, rep, t)
    lines = []
    for ln in t.split("\n"):
        ln = re.sub(r"\s+", " ", ln).strip()
        if ln:
            lines.append(ln)
    return lines


def blob(rev, path):
    r = subprocess.run(["git", "show", f"{rev}:{path}"], cwd=REPO, capture_output=True)
    if r.returncode != 0:
        return None
    for enc in ("utf-8", "cp1252"):
        try:
            return r.stdout.decode(enc)
        except UnicodeDecodeError:
            continue
    return r.stdout.decode("utf-8", errors="replace")


def residual(path):
    a = blob(REF, path)
    if a is None:
        return None
    try:
        b = blob(HEADREV, path) if HEADREV != "HEAD" else open(f"{REPO}/{path}", encoding="utf-8", errors="replace").read()
        if b is None: return None
    except FileNotFoundError:
        return None
    ca, cb = canon(a), canon(b)
    sm = difflib.SequenceMatcher(None, ca, cb, autojunk=False)
    same = sum(bl.size for bl in sm.get_matching_blocks())
    return ca, cb, same, sm


def main():
    if len(sys.argv) > 1:
        path = sys.argv[1]
        r = residual(path)
        if r is None:
            print(f"{path}: not in {REF[:7]}"); return
        ca, cb, same, sm = r
        print(f"{path}: {len(ca)} -> {len(cb)} canonical lines, {same} identical\n")
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                continue
            print(f"  --- {tag} @ ref:{i1}-{i2} head:{j1}-{j2} ---")
            for x in ca[i1:i2][:14]:
                print(f"    2022| {x[:150]}")
            for x in cb[j1:j2][:14]:
                print(f"    HEAD| {x[:150]}")
            print()
        return

    files = subprocess.run(["git", "diff", "--name-only", REF, HEADREV, "--", "*.cpp", "*.h"],
                           cwd=REPO, capture_output=True).stdout.decode().split()
    rows = []
    for f in files:
        if "tinyxml" in f or "build" in f or "CMakeFiles" in f:
            continue
        r = residual(f)
        if r is None:
            continue
        ca, cb, same, _ = r
        diff = max(len(ca), len(cb)) - same
        rows.append((diff, f, len(ca), len(cb)))
    rows.sort(reverse=True)
    print(f"residual semantic difference, {REF[:7]} (2022-05-09) -> HEAD")
    print("after canonicalizing the port's dialect and stripping comments\n")
    print(f"  {'lines differing':>15}  {'2022':>6} {'HEAD':>6}  file")
    tot = 0
    for d, f, la, lb in rows:
        tot += d
        if d:
            print(f"  {d:>15,}  {la:>6} {lb:>6}  {f}")
    clean = [f for d, f, _, _ in rows if d == 0]
    print(f"\n  total residual: {tot:,} lines across {len([1 for d,_,_,_ in rows if d])} files")
    print(f"  identical after canonicalization: {len(clean)} files")


main()
