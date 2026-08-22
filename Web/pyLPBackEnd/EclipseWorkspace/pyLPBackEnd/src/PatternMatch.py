"""PatternMatch.py - One PMA (pattern-match array) element, 20 bytes.

Overview:
    Unpacks len, cost, flags, two PEMA indexes (by pattern-end and
    child-pattern-end), descendantRelationships, and pattern id.
    Mirrors C++ cPatternMatch.

Pipeline position:
    Loaded as Source.pma[]; used to highlight winning patterns.
"""
import struct

class PatternMatch:
    # Unpack 4 shorts + 2 ints + 2 shorts (20 bytes). Fourth short ignored.
    def __init__(self, rs):
        self.len, self.cost, self.flags, _, self.pemaByPatternEnd, self.pemaByChildPatternEnd, \
        self.descendantRelationships, self.pattern = struct.unpack('<4h2i2h', rs.f.read(20))
