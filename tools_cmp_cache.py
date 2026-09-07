#!/usr/bin/env python3
"""Compare a macOS .SourceCache against the Windows reference.

Reports the header fields, then the first differing byte with context, and
locates that byte within the record stream so the divergence can be attributed
to a word position rather than an offset.

    cmp_cache.py <mac.SourceCache> [windows.SourceCache]
"""
import sys, struct, os

WIN = ("/Users/davidscott/lp/caches/texts/Christie  Agatha  1890-1976/"
       "Secret Adversary.txt.from_windows.SourceCache")

def header(d):
    """version, storageLocation, count, and the offset where records begin."""
    v = struct.unpack_from("<i", d, 0)[0]
    off = 4
    while struct.unpack_from("<H", d, off)[0] != 0:
        off += 2
    sl = d[4:off].decode("utf-16-le")
    off += 2
    count = struct.unpack_from("<I", d, off)[0]
    return v, sl, count, off + 4

def main():
    mac = sys.argv[1]
    win = sys.argv[2] if len(sys.argv) > 2 else WIN
    if not os.path.exists(mac):
        print(f"macOS cache not produced yet: {mac}")
        return 1
    a = open(mac, "rb").read()
    b = open(win, "rb").read()

    va, sla, ca, ra = header(a)
    vb, slb, cb, rb = header(b)

    print("=" * 72)
    print(f"{'':<18} {'macOS':>16} {'Windows':>16}")
    print("=" * 72)
    print(f"{'size':<18} {len(a):>16,} {len(b):>16,}   {'MATCH' if len(a)==len(b) else 'differ by %+d' % (len(a)-len(b))}")
    print(f"{'sourceVersion':<18} {va:>16} {vb:>16}   {'MATCH' if va==vb else 'DIFFER'}")
    print(f"{'storageLocation':<18} {sla!r:>16} {slb!r:>16}   {'MATCH' if sla==slb else 'DIFFER'}")
    print(f"{'word matches':<18} {ca:>16,} {cb:>16,}   {'MATCH' if ca==cb else 'differ by %+d' % (ca-cb)}")
    print(f"{'records begin at':<18} {ra:>16} {rb:>16}")
    print()

    if a == b:
        print("*** IDENTICAL ***")
        return 0

    # first differing byte
    n = min(len(a), len(b))
    i = next((k for k in range(n) if a[k] != b[k]), n)
    print(f"first difference at byte {i:,}"
          f"{' (only the version field)' if i < 4 else ''}")
    lo = max(0, i - 16)
    print(f"  macOS   {lo}: {a[lo:i+16].hex(' ')}")
    print(f"  Windows {lo}: {b[lo:i+16].hex(' ')}")
    print(f"  {' '*(len(str(lo))+11)}{'   '*(i-lo)}^^")
    print()

    # how much agrees, and where differences cluster
    same = sum(1 for k in range(n) if a[k] == b[k])
    print(f"bytes equal in the common prefix region: {same:,} / {n:,} ({same*100.0/n:.2f}%)")

    if i >= 4:
        print(f"\nfirst difference is {i - rb:,} bytes into the record stream")
    else:
        # skip the version and compare the rest, which is the meaningful question
        j = next((k for k in range(4, n) if a[k] != b[k]), n)
        if j >= n:
            print("\n*** IDENTICAL apart from the sourceVersion field ***")
        else:
            print(f"\nignoring the version field, first difference at byte {j:,}"
                  f" ({j - rb:,} into the record stream)")
            lo = max(0, j - 16)
            print(f"  macOS   {lo}: {a[lo:j+16].hex(' ')}")
            print(f"  Windows {lo}: {b[lo:j+16].hex(' ')}")
    return 0

sys.exit(main())
