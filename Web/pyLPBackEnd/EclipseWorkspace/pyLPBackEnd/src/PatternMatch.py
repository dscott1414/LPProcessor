import struct

class PatternMatch:
    def __init__(self, rs):
        self.len, self.cost, self.flags, _, self.pemaByPatternEnd, self.pemaByChildPatternEnd, \
        self.descendantRelationships, self.pattern = struct.unpack('<4h2i2h', rs.f.read(20))
