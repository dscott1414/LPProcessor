"""SpeakerGroup.py - One speaker-group span deserialized from an LP dump.

Overview:
    Mirrors C++ cSpeakerGroup: begin/end/section, speaker id arrays
    (singular/grouped/pov/dn/observers), replacedSpeakers (COM pairs),
    recursively nested embeddedSpeakerGroups, and per-position Group
    object sets. Two flag bits: speakersAreNeverGroupedTogether,
    tlTransition.

Pipeline position:
    Loaded with Source after resolveSpeakers. Used to colour quotes
    and list who is present.

Key entry points:
    - Group - {where, objects[]} nested type
    - __init__(rs) - recursive deserialize
"""
from COM import COM

class SpeakerGroup:
    class Group:
        where = 0
        objects = {}

    # Read scalars, speaker arrays, replacedSpeakers, nested
    # embeddedSpeakerGroups, groups[I], then two flag bits.
    def __init__(self, rs):
        self.begin = rs.read_integer()
        self.end = rs.read_integer()
        self.section = rs.read_integer()
        self.previousSubsetSpeakerGroup = rs.read_integer()
        self.saveNonNameObject = rs.read_integer()
        self.conversationalQuotes = rs.read_integer()
        self.speakers = rs.read_int_array()
        self.fromNextSpeakerGroup = rs.read_int_array()
        count = rs.read_integer()
        self.replacedSpeakers = []
        for I in range(count):
            self.replacedSpeakers.append(COM(rs))
        self.singularSpeakers = rs.read_int_array()
        self.groupedSpeakers = rs.read_int_array()
        self.povSpeakers = rs.read_int_array()
        self.dnSpeakers = rs.read_int_array()
        self.metaNameOthers = rs.read_int_array()
        self.observers = rs.read_int_array()
        count = rs.read_integer()
        self.embeddedSpeakerGroups = []
        for I in range(count):
            self.embeddedSpeakerGroups.append(SpeakerGroup(rs))
        count = rs.read_integer()
        self.groups = {}
        for I in range(count):
            self.groups[I]=self.Group()
            self.groups[I].where= rs.read_integer()
            self.groups[I].objects=rs.read_int_array()
        flags=rs.read_long()
        self.speakersAreNeverGroupedTogether = True if ((flags&1)>0) else False
        self.tlTransition = True if ((flags&2)>0) else False
        

