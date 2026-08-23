"""Section.py - One document section (chapter/part) from an LP dump.

Overview:
    begin / endHeader, subHeading offsets, and four COM lists:
    definite speakers, speakers, objects spoken about, objects in
    narration. Match counters are initialized to 0 (not in the dump).

Pipeline position:
    Loaded with Source; used to page the web view by chapter.
"""
from COM import COM

class Section:

    # Deserialize section span, subHeadings, and four COM lists.
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
            self.definiteSpeakerObjects.append(COM(rs=rs))
        count = rs.read_integer();
        self.speakerObjects = []
        for _ in range(count):
            self.speakerObjects.append(COM(rs=rs))
        count = rs.read_integer()
        self.objectsSpokenAbout = []
        for _ in range(count):
            self.objectsSpokenAbout.append(COM(rs=rs))
        count = rs.read_integer()
        self.objectsInNarration = []
        for _ in range(count):
            self.objectsInNarration.append(COM(rs=rs))
        self.speakersMatched = 0
        self.speakersNotMatched = 0
        self.counterSpeakersMatched = 0
        self.counterSpeakersNotMatched = 0