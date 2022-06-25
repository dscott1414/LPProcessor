class TimelineSegment:
    parentTimeline = 0
    begin = 0
    end = 0
    speakerGroup = 0
    location = 0
    linkage = 0
    sr = 0
    timeTransitions = {}
    locationTransitions = {}
    subTimelines = {}
    
    def __init__(self, rs):
        self.parentTimeline = rs.readInteger()
        self.begin = rs.readInteger()
        self.end = rs.readInteger()
        self.speakerGroup = rs.readInteger()
        self.location = rs.readInteger()
        self.linkage = rs.readInteger()
        self.sr = rs.readInteger()
        count = rs.readInteger()
        self.timeTransitions = {}
        for I in range(count):
            self.timeTransitions[I]=rs.readInteger()
        count = rs.readInteger()
        self.locationTransitions = {}
        for I in range(count):
            self.locationTransitions[I]=rs.readInteger()
        count = rs.readInteger()
        self.subTimelines = {}
        for I in range(count):
            self.subTimelines[I]=rs.readInteger()