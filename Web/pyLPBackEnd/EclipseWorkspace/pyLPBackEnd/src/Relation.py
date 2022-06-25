from TimeInfo import TimeInfo

class Relation:

    def __init__(self, rs):
        self.where = rs.readInteger()
        self.o = rs.readInteger()
        self.whereControllingEntity = rs.readInteger()
        self.whereSubject = rs.readInteger()
        self.whereVerb = rs.readInteger()
        self.wherePrep = rs.readInteger()
        self.whereObject = rs.readInteger()
        self.wherePrepObject = rs.readInteger()
        self.whereSecondaryVerb = rs.readInteger()
        self.whereSecondaryObject = rs.readInteger()
        self.whereSecondaryPrep = rs.readInteger()
        self.whereNextSecondaryObject = rs.readInteger()
        self.movingRelativeTo = rs.readInteger()
        self.relationType = rs.readInteger()
        self.objectSubType = rs.readInteger()
        self.prepSubType = rs.readInteger()
        self.timeProgression = rs.readInteger()
        self.questionType = rs.readLong()
        self.whereQuestionType = rs.readInteger()
        self.sentenceNum = rs.readInteger()
        # flags
        flags = rs.readLong()
        self.changeStateAdverb=(flags & 1) == 1; flags>>=1;
        self.skip=(flags & 1) == 1; flags>>=1;
        self.physicalRelation=(flags & 1) == 1; flags>>=1;
        self.timeInfoSet=(flags & 1) == 1; flags>>=1;
        self.agentLocationRelationSet = (flags & 1) == 1;            flags >>= 1;
        self.negation = (flags & 1) == 1;            flags >>= 1;
        self.futureInPastHappening = (flags & 1) == 1;            flags >>= 1;
        self.futureHappening = (flags & 1) == 1;            flags >>= 1;
        self.presentHappening = (flags & 1) == 1;            flags >>= 1;
        self.pastHappening = (flags & 1) == 1;            flags >>= 1;
        self.beforePastHappening = (flags & 1) == 1;            flags >>= 1;
        self.nonPresentTimeTransition = (flags & 1) == 1;            flags >>= 1;
        self.timeTransition = (flags & 1) == 1;            flags >>= 1;
        #if (timeTransition)
        #    System.out.print(where+":timeTransition true!\n");
        self.story = (flags & 1) == 1;            flags >>= 1;
        self.beyondLocalSpace = (flags & 1) == 1;            flags >>= 1;
        self.significantRelation = (flags & 1) == 1;            flags >>= 1;
        self.speakerQuestionToAudience = (flags & 1) == 1;            flags >>= 1;
        self.speakerCommand = (flags & 1) == 1;            flags >>= 1;
        self.speakerContinuation = (flags & 1) == 1;            flags >>= 1;
        self.futureLocation = (flags & 1) == 1;    flags >>= 1;
        self.establishingLocation = (flags & 1) == 1;  flags >>= 1;
        self.genderedLocationRelation = (flags & 1) == 1;    flags >>= 1;
        self.genderedEntityMove = (flags & 1) == 1;    
        self.lastOpeningPrimaryQuote = rs.readInteger();
        self.duplicateTimeTransitionFromWhere = rs.readInteger();
        self.presType=rs.readString();
        self.description=rs.readString();
        self.nextSPR=rs.readInteger();
        count = rs.readInteger();
        self.timeInfo = {}
        for I in range(count):
            self.timeInfo[I] = TimeInfo(rs);
