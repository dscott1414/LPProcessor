"""PatternElementMatch.py - One PEMA element (44 bytes) with chain links.

Overview:
    Mirrors C++ cPatternElementMatch. begin/end span, packed
    patternElement + index, then the four next* chain heads used to
    walk PEMA by position / pattern-end / child-pattern-end / next
    element, plus origin, costs, pattern id, flags.

Pipeline position:
    Loaded as Source.pema[]; WordMatch.beginPEMAPosition indexes in.
"""
import struct

class PatternElementMatch:
    def __init__(self, rs):
        # nextByPosition - the next PEMA entry in the same position - allows routines to traverse downwards
        # nextByPatternEnd - the next PEMA entry in the same position with the same pattern and end
        # nextByPatternEnd is set negative if it points back to the beginning of the loop
        # nextByChildPatternEnd - the next PEMA entry in the same position with the same childPattern and childEnd
        # nextPatternElement - the next PEMA entry in the next position with the same pattern and end - allows routines to skip forward by pattern
        # origin - beginning of the chain for the first element of the pattern
        # cumulativeDeltaCost - accumulates costs or benefits of assessCost (from multiple setSecondaryCosts)
        # tempCost - used for setSecondaryCosts
        # pattern - points to a pattern
        # iCost - lowest cost of PMA element
        # 44-byte PEMA record. nextByPatternEnd is negative when the chain
        # wraps to its origin. Underscored fields are packed bytes.
        self.begin, self.end, self.__patternElement, self.__patternElementIndex, _, self.nextByPosition, \
        self.nextByPatternEnd, self.nextByChildPatternEnd, self.nextPatternElement, self.origin, self.cumulativeDeltaCost, \
        self.tempCost, self.PEMAElementMatchedSubIndex, self.pattern, self.flags, _, self.cost, self.iCost = struct.unpack('<hhbbh5ihhihbbhh', rs.f.read(44))
