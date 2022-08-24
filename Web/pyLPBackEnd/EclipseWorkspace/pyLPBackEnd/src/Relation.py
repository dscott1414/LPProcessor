from TimeInfo import TimeInfo
import struct 

class Relation:

    def __init__(self, rs):
        self.where, self.o, self.whereControllingEntity, self.whereSubject, self.whereVerb, self.wherePrep, \
        self.whereObject, self.wherePrepObject, self.whereSecondaryVerb, self.whereSecondaryObject, self.whereSecondaryPrep, \
        self.whereNextSecondaryObject, self.movingRelativeTo, self.relationType, self.objectSubType, \
        self.prepSubType, self.timeProgression, self.questionType, self.whereQuestionType, self.sentenceNum, flags, \
        self.lastOpeningPrimaryQuote, self.duplicateTimeTransitionFromWhere = struct.unpack('<17iq2iqii', rs.f.read(100))

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

        self.presType = rs.read_string();
        self.description = rs.read_string();
        self.nextSPR = rs.read_integer();
        count = rs.read_integer();
        self.timeInfo = []
        for _ in range(count):
            self.timeInfo.append(TimeInfo(rs))
