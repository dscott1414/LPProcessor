from COM import COM

class Section:

    def __init__(self, rs):
        self.begin = rs.readInteger();
        self.endHeader = rs.readInteger();
        count = rs.readInteger();
        self.subHeadings = {}
        for I in range(count):
            self.subHeadings[I] = rs.readInteger();
        count = rs.readInteger();
        self.definiteSpeakerObjects = {}
        for I in range(count):
            self.definiteSpeakerObjects[I] = COM(rs);
        count = rs.readInteger();
        self.speakerObjects = {}
        for I in range(count):
            self.speakerObjects[I] = COM(rs);
        count = rs.readInteger();
        self.objectsSpokenAbout = {}
        for I in range(count):
            self.objectsSpokenAbout[I] = COM(rs);
        count = rs.readInteger();
        self.objectsInNarration = {}
        for I in range(count):
            self.objectsInNarration[I] = COM(rs);
        self.speakersMatched = 0
        self.speakersNotMatched = 0
        self.counterSpeakersMatched = 0
        self.counterSpeakersNotMatched = 0