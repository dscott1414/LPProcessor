from COM import COM

class SpeakerGroup:
    class Group:
        where = 0
        objects = {}

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
        

