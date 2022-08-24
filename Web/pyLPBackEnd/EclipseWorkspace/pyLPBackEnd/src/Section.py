from COM import COM

class Section:

    def __init__(self, rs):
        self.begin = rs.read_integer();
        self.endHeader = rs.read_integer();
        count = rs.read_integer();
        self.subHeadings = []
        for _ in range(count):
            self.subHeadings.append(rs.read_integer())
        count = rs.read_integer();
        self.definiteSpeakerObjects = []
        for _ in range(count):
            self.definiteSpeakerObjects.append(COM(rs))
        count = rs.read_integer();
        self.speakerObjects = []
        for _ in range(count):
            self.speakerObjects.append(COM(rs))
        count = rs.read_integer()
        self.objectsSpokenAbout = []
        for _ in range(count):
            self.objectsSpokenAbout.append(COM(rs))
        count = rs.read_integer()
        self.objectsInNarration = []
        for _ in range(count):
            self.objectsInNarration.append(COM(rs))
        self.speakersMatched = 0
        self.speakersNotMatched = 0
        self.counterSpeakersMatched = 0
        self.counterSpeakersNotMatched = 0