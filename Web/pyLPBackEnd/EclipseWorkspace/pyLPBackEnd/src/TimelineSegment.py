"""TimelineSegment.py - One timeline span from an LP dump.

Overview:
    parentTimeline, begin/end, speakerGroup, location, linkage, sr
    (space-relation index), then three count-prefixed int arrays:
    timeTransitions, locationTransitions, subTimelines.

Pipeline position:
    Loaded with Source after timeRelations; used to draw the timeline.
"""
import struct

class TimelineSegment:
    
    # Unpack 8 ints (count is the last), then three int arrays.
    def __init__(self, rs):
        self.parentTimeline, self.begin, self.end, self.speakerGroup, self.location, \
        self.linkage, self.sr, count = struct.unpack('<8i', rs.f.read(32))
        self.timeTransitions = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
        count = rs.read_integer()
        self.locationTransitions = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
        count = rs.read_integer()
        self.subTimelines = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))