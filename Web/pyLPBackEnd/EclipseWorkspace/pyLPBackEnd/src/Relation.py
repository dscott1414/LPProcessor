"""Relation.py - One cSpaceRelation deserialized from an LP binary dump.

Overview:
    Unpacks the 100-byte packed header (17 ints, a 64-bit flags word,
    two more ints) then bit-walks `flags` into the same boolean fields
    the C++ cSpaceRelation uses (skip, timeTransition, speakerCommand,
    genderedEntityMove, …). Followed by presType/description strings,
    nextSPR, and a count-prefixed TimeInfo list.

Pipeline position:
    Loaded as part of Source relations after objects/speakers.

Key entry points:
    - __init__(rs) - unpack one relation from LPIO `rs`

Notes / gotchas:
    struct format '<17iq2iqii' must stay in lockstep with the C++
    write path; a field reorder there silently mis-parses every flag.
"""
from TimeInfo import TimeInfo
import struct 

class Relation:

        # Read the packed relation header, decode 24 flag bits LSB-first,
        # then presType, description, nextSPR, and TimeInfo[count].
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
