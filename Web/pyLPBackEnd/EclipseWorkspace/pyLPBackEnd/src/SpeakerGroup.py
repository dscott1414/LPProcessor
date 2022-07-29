from COM import COM

class SpeakerGroup:
    class Group:
        where = 0
        objects = {}

    def __init__(self, rs):
        self.begin = rs.readInteger()
        self.end = rs.readInteger()
        self.section = rs.readInteger()
        self.previousSubsetSpeakerGroup = rs.readInteger()
        self.saveNonNameObject = rs.readInteger()
        self.conversationalQuotes = rs.readInteger()
        self.speakers = rs.readIntArray()
        self.fromNextSpeakerGroup = rs.readIntArray()
        count = rs.readInteger()
        self.replacedSpeakers = []
        for I in range(count):
            self.replacedSpeakers.append(COM(rs))
        self.singularSpeakers = rs.readIntArray()
        self.groupedSpeakers = rs.readIntArray()
        self.povSpeakers = rs.readIntArray()
        self.dnSpeakers = rs.readIntArray()
        self.metaNameOthers = rs.readIntArray()
        self.observers = rs.readIntArray()
        count = rs.readInteger()
        self.embeddedSpeakerGroups = []
        for I in range(count):
            self.embeddedSpeakerGroups.append(SpeakerGroup(rs))
        count = rs.readInteger()
        self.groups = {}
        for I in range(count):
            self.groups[I]=self.Group()
            self.groups[I].where= rs.readInteger()
            self.groups[I].objects=rs.readIntArray()
        flags=rs.readLong()
        self.speakersAreNeverGroupedTogether = True if ((flags&1)>0) else False
        self.tlTransition = True if ((flags&2)>0) else False
        

