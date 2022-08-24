import struct

class TimelineSegment:
    
    def __init__(self, rs):
        self.parentTimeline, self.begin, self.end, self.speakerGroup, self.location, \
        self.linkage, self.sr, count = struct.unpack('<8i', rs.f.read(32))
        self.timeTransitions = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
        count = rs.read_integer()
        self.locationTransitions = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
        count = rs.read_integer()
        self.subTimelines = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))