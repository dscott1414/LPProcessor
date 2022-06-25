class PatternElementMatch:
    def __init__(self, rs):
        self.begin = rs.readShort()
        self.end = rs.readShort()
        self.__patternElement = rs.readByte()
        self.__patternElementIndex = rs.readByte()
        rs.readShort() # skip two bytes for alignment
        self.nextByPosition = rs.readInteger() # the next PEMA entry in the same
                                                                                # position - allows routines to
                                                                                # traverse downwards
        self.nextByPatternEnd = rs.readInteger() # the next PEMA entry in the same
                                                                                    # position with the same pattern
                                                                                    # and end
        # nextByPatternEnd is set negative if it points back to the beginning of
        # the loop
        self.nextByChildPatternEnd = rs.readInteger() # the next PEMA entry in the
                                                                                            # same position with the same
                                                                                            # childPattern and childEnd
        self.nextPatternElement = rs.readInteger() # the next PEMA entry in the next
                                                                                        # position with the same pattern
                                                                                        # and end - allows routines to
                                                                                        # skip forward by pattern
        self.origin = rs.readInteger() # beginning of the chain for the first element
                                                                # of the pattern
        self.cumulativeDeltaCost = rs.readShort() # accumulates costs or benefits of assessCost (from multiple setSecondaryCosts)
        self.tempCost = rs.readShort() # used for setSecondaryCosts
        self.PEMAElementMatchedSubIndex = rs.readInteger() # points to a pattern
        self.pattern = rs.readShort()
        self.flags = rs.readByte()
        rs.readByte() # skip byte for alignment
        self.cost = rs.readShort()
        self.iCost = rs.readShort() # lowest cost of PMA element