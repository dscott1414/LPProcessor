class PatternMatch:
    def __init__(self, rs):
        self.len = rs.readShort()
        self.cost = rs.readShort()
        self.flags = rs.readShort()
        rs.readShort() # must read this because C++ compiled has 32 bit alignment
        self.pemaByPatternEnd = rs.readInteger()
        self.pemaByChildPatternEnd = rs.readInteger()
        self.descendantRelationships = rs.readShort()
        self.pattern = rs.readShort()