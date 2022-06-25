from enum import Enum, IntEnum
from CObject import CObject
from Form import Form
from WordMatch import WordMatch
from Section import Section
from SpeakerGroup import SpeakerGroup
from PatternElementMatch import PatternElementMatch
from Relation import Relation
from TimelineSegment import TimelineSegment
from SourceEnums import SourceEnums
import copy
import json

try:
    profile  # throws an exception when profile isn't defined
except NameError:
    profile = lambda x: x   # if it's not defined simply ignore the decorator.

class Source:

    batchDoc = []
    
    wordOrderWords = [ "other", "another", "second", "first", "third", "former", "latter", "that", "this",
            "two", "three", "one", "four", "five", "six", "seven", "eight" ]

    OCSubTypeStrings = [ "canadian province city", "country", "island", "mountain range peak landform",
            "ocean sea", "park monument", "region", "river lake waterway", "us city town village",
            "us state territory region", "world city town village", "geographical natural feature",
            "geographical urban feature", "geographical urban subfeature", "geographical urban subsubfeature", "travel",
            "moving", "moving natural", "relative direction", "absolute direction", "by activity", "unknown(place)" ]
    currentEmbeddedSpeakerGroup = 0
    
    class SourceMapType(Enum):
        relType = 1
        relType2 = 2
        speakerGroupType = 3
        transitionSpeakerGroupType = 4
        wordType = 5
        QSWordType = 6
        ESType = 7
        URWordType = 8
        matchingSpeakerType = 9
        matchingType = 10
        audienceMatchingType = 11
        matchingObjectType = 12
        headerType = 13
        beType = 14
        possessionType = 15
        tableType = 16

    class HTMLStyles(dict):
        UNDERLINE = 0
        FONTSIZE = 1
        FOREGROUND = 2
        BACKGROUND = 3
        BOLD = 4
        
        def __init__(self):
            pass

        def setUnderline(self, att):
            self.__setitem__(self.UNDERLINE, att)
            return self

        def setFontSize(self, att):
            self.__setitem__(self.FONTSIZE, att)
            return self

        def setBackground(self, att):
            self.__setitem__(self.BACKGROUND, att)
            return self
    
        def removeBackground(self):
            if self.BACKGROUND in self:
                self.__delitem__(self.BACKGROUND)
            return self
    
        def setForeground(self, att):
            self.__setitem__(self.FOREGROUND, att)
            return self
    
        def setBold(self, att):
            self.__setitem__(self.BOLD, att)
            return self
            
    location = ""

    class WhereSource:

        def __init__(self, inIndex, inIndex2, inIndex3, inFlags):
            self.index = inIndex
            self.index2 = inIndex2;
            self.index3 = inIndex3;
            self.flags = inFlags;

    def initializeClassStrings(self):
        self.objectClassStrings = {}
        self.objectClassStrings[SourceEnums.PRONOUN_OBJECT_CLASS] = "pron"
        self.objectClassStrings[SourceEnums.REFLEXIVE_PRONOUN_OBJECT_CLASS] =  "reflex"
        self.objectClassStrings[SourceEnums.RECIPROCAL_PRONOUN_OBJECT_CLASS] =  "recip"
        self.objectClassStrings[SourceEnums.NAME_OBJECT_CLASS] =  "name"
        self.objectClassStrings[SourceEnums.GENDERED_GENERAL_OBJECT_CLASS] =  "gender"
        self.objectClassStrings[SourceEnums.BODY_OBJECT_CLASS] =  "genbod"
        self.objectClassStrings[SourceEnums.GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS] =  "genocc"
        self.objectClassStrings[SourceEnums.GENDERED_DEMONYM_OBJECT_CLASS] =  "gendem"
        self.objectClassStrings[SourceEnums.GENDERED_RELATIVE_OBJECT_CLASS] =  "genre"
        self.objectClassStrings[SourceEnums.NON_GENDERED_GENERAL_OBJECT_CLASS] =  "nongen"
        self.objectClassStrings[SourceEnums.NON_GENDERED_BUSINESS_OBJECT_CLASS] =  "ngbus"
        self.objectClassStrings[SourceEnums.NON_GENDERED_NAME_OBJECT_CLASS] =  "ngname"
        self.objectClassStrings[SourceEnums.VERB_OBJECT_CLASS] =  "vb"
        self.objectClassStrings[SourceEnums.PLEONASTIC_OBJECT_CLASS] =  "pleona"
        self.objectClassStrings[SourceEnums.META_GROUP_OBJECT_CLASS] =  "mg"

    def __init__(self, words, rs):
        self.initializeClassStrings()
        self.masterSpeakerList = {}
        self.version = rs.readInteger() 
        self.location = rs.readString()
        count = rs.readInteger()
        self.m = {}
        for I in range(count):
            self.m[I] = WordMatch(words, rs)
        count = rs.readInteger()
        self.sentenceStarts = {}
        for I in range(count):
            self.sentenceStarts[I] = rs.readInteger()
        count = rs.readInteger()
        self.sections = {}
        for I in range(count):
            self.sections[I] = Section(rs)
        count = rs.readInteger()
        self.speakerGroups = {}
        for I in range(count):
            self.speakerGroups[I] = SpeakerGroup(rs)
        count = rs.readInteger()
        self.pema = {}
        for I in range(count):
            self.pema[I] = PatternElementMatch(rs)
        count = rs.readInteger()
        self.objects = {}
        for I in range(count):
            self.objects[I] = CObject(rs)
            if self.objects[I].masterSpeakerIndex >= 0:
                self.masterSpeakerList[self.objects[I].masterSpeakerIndex] = I
        count = rs.readInteger();
        self.relations = {}
        for I in range(count):
            self.relations[I] = Relation(rs);
        count = rs.readInteger();
        self.timelineSegments = {}
        for I in range(count):
            self.timelineSegments[I] = TimelineSegment(rs)

    @profile
    def addElement(self, s, attrs, where, index2, index3, sourceMapType):
        self.batchDoc.append((s, copy.copy(attrs), where, index2, index3, sourceMapType))
        # self.positionToSource[self.docPosition] = self.WhereSource(where, index2, index3, sourceMapType)
        #if sourceMapType == self.SourceMapType.headerType or sourceMapType == self.SourceMapType.wordType:
        #    self.sourceToPosition[where] = self.docPosition
        self.docPosition += len(s)

    @profile
    def getOriginalWord(self, wm):
        originalWord = wm.word;
        if (wm.queryForm(SourceEnums.PROPER_NOUN_FORM_NUM) >= 0 or (wm.flags & WordMatch.flagFirstLetterCapitalized) != 0):
            originalWord = originalWord[0].upper() + originalWord[1:]
        if ((wm.flags & WordMatch.flagNounOwner) != 0):
            originalWord += "'s";
        if ((wm.flags & WordMatch.flagAllCaps) != 0):
            originalWord = originalWord.upper();
        return originalWord;

    @profile
    def phraseString(self, begin, end, shortFormat):
        tmp = ""
        for K in range(begin, end):
            tmp += self.getOriginalWord(self.m[K]) + " "
        if (begin != end and not shortFormat):
            tmp += "[" + str(begin) + "-" + str(end) + "]"
        return tmp

    @profile
    def objectString(self, om, shortNameFormat, objectOwnerRecursionFlag):
        tmp = self.objectNumString(om.object, shortNameFormat, objectOwnerRecursionFlag)
        if (shortNameFormat):
            return tmp
        return tmp + om.salienceFactor

    @profile
    def objectsString(self, oms, shortNameFormat, objectOwnerRecursionFlag):
        tmp = "";
        for s in range(len(oms)):
            tmp += self.objectString(oms[s], shortNameFormat, objectOwnerRecursionFlag)
            if (s != len(oms) - 1):
                tmp += " "
        return tmp

    def getClass(self, objectClass):
        return self.objectClassStrings[objectClass]

    def objectKnownString(self, obj, shortFormat, objectOwnerRecursionFlag):
        if (obj == 0):
            return "Narrator";
        elif (obj == 1):
            return "Audience";
        if (self.objects[obj].objectClass == SourceEnums.NAME_OBJECT_CLASS):
            tmpstr = self.objects[obj].name.print(True)
        else:
            tmpstr = self.phraseString(self.objects[obj].begin, self.objects[obj].end, shortFormat)
        ow = self.objects[obj].ownerWhere
        if (ow >= 0):
            selfOwn = obj == (self.m[ow].object)
            for I in range(len(self.m[ow].objectMatches)):
                if obj == (self.m[ow].objectMatches[I].object):
                    tmpstr = "Self-owning obj!";
                    selfOwn = True;
            if not selfOwn and not objectOwnerRecursionFlag:
                if len(self.m[ow].objectMatches) > 0:
                    self.objectsString(self.m[ow].objectMatches, True, True)
                else:
                    tmpstr += self.objectNumString(self.m[ow].object, True, True)
            tmpstr += "{OWNER:" + tmpstr + "}"
        if (self.objects[obj].ownerWhere < -1):
            tmpstr += "{WO:" + self.wordOrderWords[-2 - self.objects[obj].ownerWhere] + "}"
        if not shortFormat:
            tmpstr += "[" + str(self.objects[obj].originalLocation) + "]"
            tmpstr += "[" + self.getClass(self.objects[obj].objectClass) + "]"
            if (self.objects[obj].male):
                tmpstr += "[M]"
            if (self.objects[obj].female):
                tmpstr += "[F]"
            if (self.objects[obj].neuter):
                tmpstr += "[N]"
            if (self.objects[obj].plural):
                tmpstr += "[PL]"
            if (self.objects[obj].ownerWhere != -1):
                tmpstr += "[OGEN]"
            if (self.objects[obj].objectClass == SourceEnums.NAME_OBJECT_CLASS):
                tmpstr += "[" + self.objects[obj].name.print(False) + "]";
            if (self.objects[obj].suspect):
                tmpstr += "[suspect]";
            if (self.objects[obj].verySuspect):
                tmpstr += "[verySuspect]";
            if (self.objects[obj].ambiguous):
                tmpstr += "[ambiguous]";
            if (self.objects[obj].eliminated):
                tmpstr += "[ELIMINATED]";
            if (self.objects[obj].subType >= 0):
                tmpstr += "[" + self.OCSubTypeStrings[self.objects[obj].subType] + "]";
        if self.objects[obj].relativeClausePM >= 0 and self.objects[obj].objectClass != SourceEnums.NAME_OBJECT_CLASS:
            tmpstr += "[" + self.phraseString(self.objects[obj].whereRelativeClause,
                    self.objects[obj].whereRelativeClause
                            +self.m[self.objects[obj].whereRelativeClause].pma[self.objects[obj].relativeClausePM].len,
                    shortFormat) + "]"
        if not shortFormat:
            if len(self.objects[obj].associatedAdjectives) > 0 and len(self.objects[obj].associatedAdjectives) < 4: 
                tmpstr += "ADJ:";
                for I in range(len(self.objects[obj].associatedAdjectives)):
                    tmpstr += self.objects[obj].associatedAdjectives[I] + " "
            if len(self.objects[obj].associatedNouns) > 0 and len(self.objects[obj].associatedNouns) < 4:
                tmpstr += "NOUN:"
                for I in range(len(self.objects[obj].associatedNouns)):
                    tmpstr += self.objects[obj].associatedNouns[I] + " "
        return tmpstr;

    def objectNumString(self, obj, shortNameFormat, objectOwnerRecursionFlag):
        if obj < 0:
            if obj == SourceEnums.UNKNOWN_OBJECT: return "";
            if obj == SourceEnums.OBJECT_UNKNOWN_MALE: return "UNKNOWN_MALE";
            if obj == SourceEnums.OBJECT_UNKNOWN_FEMALE: return "UNKNOWN_FEMALE";
            if obj == SourceEnums.OBJECT_UNKNOWN_MALE_OR_FEMALE: return "UNKNOWN_MALE_OR_FEMALE";
            if obj == SourceEnums.OBJECT_UNKNOWN_NEUTER: return "UNKNOWN_NEUTER";
            if obj == SourceEnums.OBJECT_UNKNOWN_PLURAL: return "UNKNOWN_PLURAL";
            if obj == SourceEnums.OBJECT_UNKNOWN_ALL: return "ALL";
        return self.objectKnownString(obj, shortNameFormat, objectOwnerRecursionFlag)

    def getVerbSenseString(self, verbSense):
        verbSenseStr = ""
        if verbSense != 0:
            if ((verbSense & SourceEnums.VT_TENSE_MASK) == self.VT_PRESENT):
                verbSenseStr += "PRESENT "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == self.VT_PRESENT_PERFECT):
                verbSenseStr += "PRESENT_PERFECT "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == self.VT_PAST):
                verbSenseStr += "PAST "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == self.VT_PAST_PERFECT):
                verbSenseStr += "PAST_PERFECT "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == self.VT_FUTURE):
                verbSenseStr += "FUTURE "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == self.VT_FUTURE_PERFECT):
                verbSenseStr += "FUTURE_PREFECT "
            if ((verbSense & SourceEnums.VT_EXTENDED) == self.VT_EXTENDED):
                verbSenseStr += "EXTENDED "
            if ((verbSense & SourceEnums.VT_PASSIVE) == self.VT_PASSIVE):
                verbSenseStr += "PASSIVE "
            if ((verbSense & SourceEnums.VT_NEGATION) == self.VT_NEGATION):
                verbSenseStr += "NEGATION "
            if ((verbSense & SourceEnums.VT_IMPERATIVE) == self.VT_IMPERATIVE):
                verbSenseStr += "POSSIBLE "
            if ((verbSense & SourceEnums.VT_POSSIBLE) == self.VT_POSSIBLE):
                verbSenseStr += "IMPERATIVE "
        return verbSenseStr;

    def relationString(self, r):
        relStrings = [ "", "EXIT", "ENTER", "STAY", "ESTAB", "MOVE", "MOVE_OBJECT", "MOVE_IN_PLACE", "METAWQ",
                "CONTACT", "NEAR", "TRANSFER", "LOCATION", "PREP TIME", "PREP DATE", "SUBJDAY TIME", "ABS TIME",
                "ABS DATE", "ADVERB TIME", "THINK", "COMMUNICATE", "START", "COS", "BE", "HAS-A", "METAINFO",
                "METAPROFESSION", "METABELIEF", "METACONTACT", "THINK_OBJECT", "stCHANGESTATE", "stCONTIGUOUS",
                "stCONTROL", "stAGENTCHANGEOBJECTINTERNALSTATE", "stSENSE", "stCREATE", "stCONSUME", "stMETAFUTUREHAVE",
                "stMETAFUTURECONTACT", "stMETAIFTHEN", "stMETACONTAINS", "stMETADESIRE", "stMETAROLE",
                "stSPATIALORIENTATION", "stIGNORE", "stNORELATION", "stOTHER", "47" ]
        if (r > 0):
            return relStrings[r]
        if (r == -SourceEnums.stLOCATION):
            return relStrings[-r]
        return "unknown"

    masterSpeakerColors = [
            # (0x00, 0xFF, 0xFF), # "Aqua" too light
            (0x60, 0xCC, 0x93),  # "Aquamarine"
            # (0x6F, 0xFF, 0xC3), # "Aquamarine(Light)" too light
            # (0xF6, 0xF6, 0xCC), # "Beige" too light
            # (0xFF, 0xF3, 0xC3), # "Bisque" too light
            # (0x00, 0x00, 0x00), # "Black"
            (0x00, 0x00, 0xFF),  # "Blue"
            (0x6F, 0x9F, 0x9F),  # "Blue(Cadet)"
            (0x33, 0x33, 0x6F),  # "Blue(CornFlower)"
            (0x00, 0x00, 0x9C),  # "Blue(Dark)"
            (0x3F, 0x33, 0xF3),  # "Blue(Midnight)"
            (0x33, 0x33, 0x9F),  # "Blue(Navy)"
            (0x3C, 0x3C, 0xFF),  # "Blue(Neon)"
            (0x33, 0x99, 0xCC),  # "Blue(Sky)"
            (0x33, 0x6C, 0x9F),  # "Blue(Steel)"
            (0x99, 0x3C, 0xF3),  # "Blue(Violet)"
            (0xC6, 0x96, 0x33),  # "Brass"
            (0x9C, 0x69, 0x63),  # "Bronze"
            (0x96, 0x39, 0x39),  # "Brown"
            (0x66, 0x30, 0x33),  # "Brown(Dark)"
            # (0xF6, 0xCC, 0xC0), # "Brown(Faded)"
            (0x33, 0xCC, 0x99),  # "Aquamarine(Med)" # moved to prevent
                                            # speakers from being to close in color
            (0x6F, 0x9F, 0x90),  # "Blue(Cadet3)"
            (0x63, 0x96, 0xFC),  # "Blue(CornFlower-Light)"
            (0x6C, 0x33, 0x9F),  # "Blue(DarkSlate)"
            (0x99, 0xCC, 0xFF),  # "Blue(Light)"
            (0x33, 0x33, 0xCC),  # "Blue(Med)"
            (0x6F, 0xFF, 0x00),  # "Chartruse"
            (0xC3, 0x69, 0x0F),  # "Chocolate"
            (0x6C, 0x33, 0x06),  # "Chocolate(Baker's)"
            (0x6C, 0x33, 0x36),  # "Chocolate(SemiSweet)"
            (0xC9, 0x63, 0x33),  # "Copper"
            (0x39, 0x66, 0x6F),  # "Copper(DarkGreen)"
            (0x63, 0x6F, 0x66),  # "Copper(Green)"
            (0xFF, 0x6F, 0x60),  # "Coral"
            (0xFF, 0x6F, 0x00),  # "Coral(Bright)"
            (0xFF, 0xF9, 0xCC),  # "Cornsilk"
            (0xCC, 0x03, 0x3C),  # "Crimson"
            (0x00, 0xFF, 0xFF),  # "Cyan"
            (0x00, 0x9C, 0x9C),  # "Cyan(Dark)"
            (0x9F, 0x33, 0x33),  # "Firebrick"
            (0xFF, 0x00, 0xFF),  # "Fushia"
            (0xFF, 0xCC, 0x00),  # "Gold"
            (0xC9, 0xC9, 0x09),  # "Gold(Bright)"
            (0xC6, 0xC6, 0x3C),  # "Gold(Old)"
            (0xCC, 0xCC, 0x60),  # "Goldenrod"
            (0xC9, 0x96, 0x0C),  # "Goldenrod(Dark)"
            # (0xF9, 0xF9, 0x9F), # "Goldenrod(Med)" not visible
            (0x90, 0x90, 0x90),  # "Gray"
            (0x3F, 0x3F, 0x3F),  # "Gray(DarkSlate)"
            (0x66, 0x66, 0x66),  # "Gray(Dark)"
            (0x99, 0x99, 0x99),  # "Gray(Light)"
            (0xC0, 0xC0, 0xC0),  # "Gray(Lighter)"
            (0x00, 0x90, 0x00),  # "Green"
            (0x00, 0x63, 0x00),  # "Green(Dark)"
            (0x3F, 0x3F, 0x3F),  # "Green(Forest)"
            (0x9F, 0xCC, 0x9F),  # "Green(Pale)"
            (0x99, 0xCC, 0x33),  # "Green(Yellow)"
            (0x9F, 0x9F, 0x6F),  # "Khaki"
            (0xCC, 0xC6, 0x6C),  # "Khaki(Light)"
            (0x00, 0xFF, 0x00),  # "Lime"
            (0xFF, 0x00, 0xFF),  # "Magenta"
            (0xCC, 0x00, 0x9C),  # "Magenta(Dark)"
            (0x90, 0x00, 0x00),  # "Maroon"
            (0x9F, 0x33, 0x6C),  # "Maroon3"
            (0x00, 0x00, 0x90),  # "Navy"
            (0x90, 0x90, 0x00),  # "Olive"
            (0x66, 0x6C, 0x3F),  # "Olive(Dark)"
            (0x3F, 0x3F, 0x3F),  # "Olive(Darker)"
            (0xFF, 0x6F, 0x00),  # "Orange"
            (0xFF, 0x9C, 0x00),  # "Orange(Light)"
            (0xCC, 0x60, 0xCC),  # "Orchid"
            (0x99, 0x33, 0xCC),  # "Orchid(Dark)"
            (0xF9, 0x9C, 0xF9),  # "Pink"
            (0xCC, 0x9F, 0x9F),  # "Pink(Dusty)"
            (0x90, 0x00, 0x90),  # "Purple"
            (0x96, 0x06, 0x69),  # "Purple(Dark)"
            (0xC9, 0xC9, 0xF3),  # "Quartz"
            (0xFF, 0x00, 0x00),  # "Red"
            (0x9C, 0x00, 0x00),  # "Red(Dark)"
            (0xFF, 0x33, 0x00),  # "Red(Orange)"
            (0x96, 0x63, 0x63),  # "Rose(Dusty)"
            (0x6F, 0x33, 0x33),  # "Salmon"
            (0x9C, 0x06, 0x06),  # "Scarlet"
            (0x9F, 0x6C, 0x33),  # "Sienna"
            (0xF6, 0xF9, 0xF9),  # "Silver"
            (0x39, 0xC0, 0xCF),  # "Sky(Summer)"
            (0xCC, 0x93, 0x60),  # "Tan"
            (0x96, 0x69, 0x3F),  # "Tan(Dark)"
            (0x00, 0x90, 0x90),  # "Teal"
            (0xC9, 0xCF, 0xC9),  # "Thistle"
            (0x9C, 0xF9, 0xF9),  # "Turquoise"
            (0x00, 0xCF, 0xC0),  # "Turquoise(Dark)"
            (0x99, 0x00, 0xFF),  # "Violet"
            (0x66, 0x00, 0xCC),  # "Violet(Blue)"
            (0xCC, 0x33, 0x99),  # "Violet(Red)"
            (0xFF, 0xFF, 0xFF),  # "White"
            (0xF9, 0xFC, 0xC6),  # "White(Antique)"
            (0xCF, 0xC9, 0x96),  # "Wood(Curly)"
            (0x96, 0x6F, 0x33),  # "Wood(Dark)"
            (0xF9, 0xC3, 0x96),  # "Wood(Light)"
            (0x96, 0x90, 0x63),  # "Wood(Med)"
            (0xFF, 0xFF, 0x00)  # "Yellow"
            ]
    
    def setAttributes(self, spaceRelations, spr):
        keyWord = self.HTMLStyles()
        if (spaceRelations[self.spr].agentLocationRelationSet):
            # is the subject moving within a local space, like a room, or is the
            # subject moving from a space to another, like from the building to the
            # street?
            # G + (self.relations[originalSPRI].beyondLocalSpace) ? "":"S" + SRType + "SR"
            if (spaceRelations[self.spr].beyondLocalSpace):
                keyWord.setUnderline(True)
                keyWord.setFontSize(12)
            else:
                keyWord.setFontSize(8)
            if (spaceRelations[self.spr].pastHappening):
                keyWord.setBackground((170, 0, 241))
            elif (spaceRelations[self.spr].futureHappening):
                keyWord.setBackground((200, 200, 50))
            elif (spaceRelations[self.spr].beforePastHappening):
                keyWord.setBackground((104, 0, 225))
            elif (spaceRelations[self.spr].futureInPastHappening):
                keyWord.setBackground((200, 100, 50))
            else:
                keyWord.setBackground((100, 224, 225))
        else:
            if (spaceRelations[self.spr].genderedLocationRelation):
                keyWord.setUnderline(True)
                keyWord.setBackground((100, 224, 225))
            else:
                keyWord.setBackground((200, 200, 200))
        return keyWord;

    def shouldPrintRelation(self, where, spr, preference):
        printAll = True
        for p in preference:
            if (not p):
                printAll = False
        if (printAll):
            return True
        timeRelation = len(self.relations[self.spr].timeInfo) > 0 or (self.relations[self.spr].relationType == self.stABSTIME
                or self.relations[self.spr].relationType == self.stPREPTIME or self.relations[self.spr].relationType == self.stPREPDATE
                or self.relations[self.spr].relationType == self.stSUBJDAYOFMONTHTIME or self.relations[self.spr].relationType == self.stABSTIME
                or self.relations[self.spr].relationType == self.stABSDATE or self.relations[self.spr].relationType == self.stADVERBTIME)
        if (not preference[self.INCLUDE_STORY] and self.relations[self.spr].story):
            return False
        if (not preference[self.INCLUDE_QUOTES] and (self.m[self.relations[self.spr].where].objectRole & CObject.IN_PRIMARY_QUOTE_ROLE) != 0):  
            return False
        if (not preference[self.INCLUDE_TIME_TRANSITION] and not self.relations[self.spr].timeTransition):
            return False
        if (preference[self.INCLUDE_ABSTRACT] and not (self.relations[self.spr].physicalRelation or timeRelation)):
            return True
        if (preference[self.INCLUDE_TIME]):
            return timeRelation
        if (preference[self.INCLUDE_SPACE] and not self.relations[self.spr].physicalRelation):
            return False
        return True

    def printRelation(self, where, preferences):
        if (self.spr < len(self.relations) and self.relations[self.spr].where == where):
            if (self.shouldPrintRelation(where, self.spr, preferences)):
                self.addElement(self.relationString(self.relations[self.spr].relationType), self.setAttributes(self.relations, self.spr), where, self.spr, -1,
                        self.SourceMapType.relType);
        while (self.spr < len(self.relations) and self.relations[self.spr].where < where):
            self.spr = self.relations[self.spr].nextSPR;

    def printFullRelationString(self, originalSPRI, endSpeakerGroup):
        presType = "";
        r = self.relations[originalSPRI];
        if (r.futureHappening and r.wherePrepObject >= 0 and self.m[r.wherePrepObject].object >= 0
                and self.objects[self.m[r.wherePrepObject].object].subType == self.BY_ACTIVITY):
            li = originalSPRI + 1
            while (abs(self.relations[li].relationType) == self.stLOCATION and li < len(self.relations)
                    and self.relations[li].where < endSpeakerGroup):
                li += 1
                if (self.relations[li].whereSubject >= 0):
                    presType += self.m[self.relations[li].whereSubject].word + " "
        Q = "\"" if ((self.m[r.where].objectRole & CObject.IN_PRIMARY_QUOTE_ROLE) != 0) else ""  
        w = r.where + ":" + Q + ("story " if (r.story) else "") + r.description + r.presType + presType + Q
        return w

    def evaluateSpaceRelation(self, where, endSpeakerGroup, spr, preferences):
        if (self.relations[self.spr].agentLocationRelationSet):
            originalSPRI = self.spr;
            self.spr = self.relations[self.spr].nextSPR;
            if (self.shouldPrintRelation(where, originalSPRI, preferences)):
                w = self.printFullRelationString(originalSPRI, endSpeakerGroup)
                self.addElement(w, self.setAttributes(self.relations, originalSPRI), where, self.spr, -1, self.SourceMapType.relType2)
                self.addElement("\n", self.setAttributes(self.relations, originalSPRI), where, self.spr, -1, self.SourceMapType.relType2)
            if (self.relations[originalSPRI].whereControllingEntity != -1 and self.relations[self.spr].whereControllingEntity == -1
                    and self.relations[originalSPRI].whereSubject == self.relations[self.spr].whereSubject
                    and self.relations[originalSPRI].whereVerb == self.relations[self.spr].whereVerb):
                self.spr += 1
        else:
            self.spr += 1
        return self.spr

    def fillAgents(self):
        docPosition = 0;
        for ms in range(0, self.masterSpeakerList.size()):
            msindex = ms % 8 #len(self.masterSpeakerColors)
            keyWord = self.HTMLStyles()
            keyWord.setBold(True)
            keyWord.setForeground(self.masterSpeakerColors[msindex])
            s = self.objectNumString(self.masterSpeakerList.get(ms), False, False)
            s += " ds[" + self.objects[self.masterSpeakerList.get(ms)].numDefinitelyIdentifiedAsSpeaker + "] s[" + \
                    self.objects[self.masterSpeakerList.get(ms)].numIdentifiedAsSpeaker + "]"
            self.batchDoc.append((s, keyWord))
            self.batchDoc.append(("\n", keyWord))
            self.positionToAgent[docPosition] = self.WhereSource(self.masterSpeakerList.get(ms), -1, -1, self.SourceMapType.matchingSpeakerType)
            self.agentToPosition[self.masterSpeakerList.get(ms)] = docPosition
            docPosition += len(s)
        return self.batchDoc

    @profile
    def printSpeakerGroup(self, I, preferences):
        while (self.currentSpeakerGroup < len(self.speakerGroups) and I == self.speakerGroups[self.currentSpeakerGroup].begin):
            if (self.speakerGroups[self.currentSpeakerGroup].tlTransition):
                keyWord = self.HTMLStyles()
                keyWord.setBold(True)
                keyWord.setForeground((0xFF, 0x00, 0x00))
                self.addElement("------------------TRANSITION------------------", keyWord, self.currentSpeakerGroup, I, -1,
                        self.SourceMapType.transitionSpeakerGroupType)
            self.addElement("\n", None, self.currentSpeakerGroup, -1, -1, self.SourceMapType.speakerGroupType)
            self.paragraphSpacingOn.append(len(self.batchDoc))
            for mo in self.speakerGroups[self.currentSpeakerGroup].speakers.values():
                isPOV = mo in self.speakerGroups[self.currentSpeakerGroup].povSpeakers.values()
                isObserver = mo in self.speakerGroups[self.currentSpeakerGroup].observers.values()
                isGrouped = mo in self.speakerGroups[self.currentSpeakerGroup].groupedSpeakers.values()
                msindex = self.objects[mo].masterSpeakerIndex % 8 #len(self.masterSpeakerColors)
                keyWord = self.HTMLStyles()
                keyWord.setBold(True)
                if (msindex < len(self.masterSpeakerColors) and msindex >= 0):
                    keyWord.setForeground(self.masterSpeakerColors[msindex])
                if (isObserver):
                    keyWord.setBackground((0, 0, 0))
                elif (isPOV):
                    keyWord.setBackground((130, 130, 130))
                oStr = ""
                if (isObserver):
                    oStr += "[obs]"
                elif (isPOV):
                    oStr += "[pov]"
                if (isGrouped):
                    oStr += "[grouped]"
                oStr += self.objectNumString(mo, False, False)
                self.addElement(oStr, keyWord, self.currentSpeakerGroup, -1, mo, self.SourceMapType.speakerGroupType)
                self.addElement("\n", keyWord, self.currentSpeakerGroup, -1, mo, self.SourceMapType.speakerGroupType)
            self.addElement("\n", None, self.currentSpeakerGroup, -1, -1, self.SourceMapType.speakerGroupType)
            self.currentSpeakerGroup += 1
            if (self.currentSpeakerGroup >= len(self.speakerGroups)):
                break;
            if (len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups) > 0):
                self.currentEmbeddedSpeakerGroup = 0;
            else:
                self.currentEmbeddedSpeakerGroup = -1;
            endSpr = self.spr
            while (endSpr < len(self.relations)
                    and self.relations[endSpr].where < self.speakerGroups[self.currentSpeakerGroup].begin):
                endSpr += 1
            if (endSpr > self.spr):
                spri = self.spr
                while (spri < len(self.relations)
                        and self.relations[spri].where < self.speakerGroups[self.currentSpeakerGroup].begin):
                    spri = self.evaluateSpaceRelation(I, self.speakerGroups[self.currentSpeakerGroup].begin, spri, preferences)
        while (self.currentEmbeddedSpeakerGroup >= 0 and self.currentSpeakerGroup > 0
                and self.currentEmbeddedSpeakerGroup < len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups)
                and I == self.speakerGroups[self.currentSpeakerGroup
                        -1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].begin):
            for mo in self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].speakers.values():
                isPOV = mo in self.speakerGroups[self.currentSpeakerGroup
                        -1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].povSpeakers.values()
                isObserver = mo in self.speakerGroups[self.currentSpeakerGroup
                        -1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].observers.values()
                isGrouped = mo in self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].groupedSpeakers.values()
                msindex = self.objects[mo].masterSpeakerIndex % 8 # len(self.masterSpeakerColors)
                if (msindex < 0):
                    msindex = 0
                keyWord = self.HTMLStyles()
                keyWord.setForeground(self.masterSpeakerColors[msindex])
                keyWord.setBold(True)
                keyWord.setFontSize(12)
                if (isObserver):
                    keyWord.setBackground((0, 0, 0))
                elif (isPOV):
                    keyWord.setBackground((192, 192, 192))
                oStr = ""
                if (isObserver):
                    oStr += "[obs]"
                elif (isPOV):
                    oStr += "[pov]"
                if (isGrouped):
                    oStr += "[grouped]"
                oStr += self.objectNumString(mo, False, False)
                self.addElement(oStr, keyWord, self.currentSpeakerGroup - 1, self.currentEmbeddedSpeakerGroup, mo,
                        self.SourceMapType.speakerGroupType)
                self.addElement("\n", keyWord, self.currentSpeakerGroup - 1, self.currentEmbeddedSpeakerGroup, mo,
                        self.SourceMapType.speakerGroupType)
            self.currentEmbeddedSpeakerGroup += 1
        if (self.currentEmbeddedSpeakerGroup >= 0 and self.currentSpeakerGroup > 0
                and self.currentEmbeddedSpeakerGroup < len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups)
                and I == self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].end):
            self.addElement("END", None, self.currentSpeakerGroup, self.currentEmbeddedSpeakerGroup, -1,
                    self.SourceMapType.speakerGroupType);
            self.currentEmbeddedSpeakerGroup = -1;

    def setParagraphSpacing(self, none):
        # Start with the current input attributes for the JTextPane. This
        # should ensure that we do not wipe out any existing attributes
        # (such as alignment or other paragraph attributes) currently
        # set on the text area.
        attributes = self.HTMLStyles()
        attributes.setSpaceBelow(0 if (none) else 5)
        # Replace the style for the entire document. We exceed the length
        # of the document by 1 so that text entered at the end of the
        # document uses the attributes.
        self.batchDoc.append("\n", attributes)

    def setTimeColorAttributes(self, timeColor, keyWord):
        if (timeColor > 0):
            keyWord.setBackground((255, 255, 0));
            if ((timeColor & SourceEnums.T_UNIT) > 0):
                keyWord.setForeground((0, 0, 0));
            elif ((timeColor & SourceEnums.T_RECURRING) == SourceEnums.T_RECURRING):
                keyWord.setForeground((204, 120, 204));
                keyWord.setUnderline(True);
            elif ((timeColor & SourceEnums.T_BEFORE) == SourceEnums.T_BEFORE):
                keyWord.setUnderline(True);
                keyWord.setForeground((60, 255, 60));
            elif ((timeColor & SourceEnums.T_AFTER) == SourceEnums.T_AFTER):
                keyWord.setUnderline(True);
                keyWord.setForeground((0, 205, 0));
            elif ((timeColor & SourceEnums.T_VAGUE) == SourceEnums.T_VAGUE):
                keyWord.setUnderline(True);
                keyWord.setForeground((0, 50, 0));
            elif ((timeColor & SourceEnums.T_AT) == SourceEnums.T_AT):
                keyWord.setUnderline(True);
                keyWord.setForeground((0, 102, 204));
            elif ((timeColor & SourceEnums.T_ON) == SourceEnums.T_ON):
                keyWord.setUnderline(True);
                keyWord.setForeground((102, 102, 204));
            # elif ((timeColor & T_PRESENT) == T_PRESENT):
                # keyWord.setForeground((102,102,204));
            # elif ((timeColor & T_THROUGHOUT) == T_THROUGHOUT): 
                # keyWord.setForeground((102,102,204));
            elif ((timeColor & SourceEnums.T_MIDWAY) == SourceEnums.T_MIDWAY):
                keyWord.setUnderline(True);
                keyWord.setForeground((204, 120, 204));
            elif ((timeColor & SourceEnums.T_IN) == SourceEnums.T_IN):
                keyWord.setUnderline(True);
                keyWord.setForeground((60, 255, 60));
            elif ((timeColor & SourceEnums.T_INTERVAL) == SourceEnums.T_INTERVAL):
                keyWord.setUnderline(True);
                keyWord.setForeground((0, 205, 0));
            elif ((timeColor & SourceEnums.T_START) == SourceEnums.T_START):
                keyWord.setUnderline(True);
                keyWord.setForeground((0, 50, 0));
            elif ((timeColor & SourceEnums.T_STOP) == SourceEnums.T_STOP):
                keyWord.setUnderline(True);
                keyWord.setForeground((0, 102, 204));
            elif ((timeColor & SourceEnums.T_RESUME) == SourceEnums.T_RESUME):
                keyWord.setUnderline(True);
                keyWord.setForeground((102, 102, 204));
            elif ((timeColor & SourceEnums.T_RANGE) == SourceEnums.T_RANGE):
                keyWord.setUnderline(True);
                keyWord.setForeground((152, 152, 204));

    def setVerbSenseAttributes(self, verbSense, keyWord):
        if (verbSense != 0):
            if ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PRESENT):
                keyWord.setBackground((91, 193, 255));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PRESENT_PERFECT):
                keyWord.setBackground((91, 255, 193));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PAST):
                keyWord.setBackground((255, 193, 91));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PAST_PERFECT):
                keyWord.setBackground((255, 91, 193));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_FUTURE):
                keyWord.setBackground((193, 91, 255));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_FUTURE_PERFECT):
                keyWord.setBackground((193, 193, 51));
            if ((verbSense & SourceEnums.VT_EXTENDED) == SourceEnums.VT_EXTENDED):
                keyWord.setUnderline(True);
            if ((verbSense & SourceEnums.VT_PASSIVE) == SourceEnums.VT_PASSIVE):
                keyWord.setForeground((90, 90, 90));
            if ((verbSense & SourceEnums.VT_POSSIBLE) == SourceEnums.VT_POSSIBLE):
                keyWord.setForeground((160, 160, 160));
            if ((verbSense & (SourceEnums.VT_PASSIVE | SourceEnums.VT_POSSIBLE)) == (SourceEnums.VT_PASSIVE | SourceEnums.VT_POSSIBLE)):
                # u=";border-style: solid; border-width: thin";
                if ((verbSense & SourceEnums.VT_EXTENDED) == SourceEnums.VT_EXTENDED):
                    keyWord.setForeground((90, 90, 90));

    def getVerbClasses(self, whereVerb):
        baseVerb = self.m[whereVerb].baseVerb;
        # map <wstring, set <int> >::iterator lvtoCi;
        vms = self.vbNetVerbToClassMap.get(baseVerb);
        # get_out is very different from get by itself
        if (whereVerb + 1 < len(self.m) and (self.m[whereVerb + 1].queryWinnerForm(Form.adverbForm) >= 0  
                or self.m[whereVerb + 1].queryWinnerForm(Form.prepositionForm) >= 0  
                or self.m[whereVerb + 1].queryWinnerForm(Form.nounForm) >= 0)):  
            verbParticiple = baseVerb + "_" + self.m[whereVerb + 1].word
            vmsParticiple = self.vbNetVerbToClassMap.get(verbParticiple)
            if (vmsParticiple != None):
                if (self.m[whereVerb].relObject == whereVerb + 1):
                    self.m[whereVerb].relObject = -1
                vms = vmsParticiple
        return vms

    @profile
    def generateSourceElements(self, words, preferences):
        self.positionToSource = {}
        self.sourceToPosition = {}
        self.positionToAgent = {}
        self.agentToPosition = {}
        self.paragraphSpacingOff = []
        self.paragraphSpacingOn = []
        self.section = 0;
        self.docPosition = 0;
        self.spr = 0;
        total = len(self.m)
        I = -1
        last = -1
        inObject = -1
        endObject = 0
        principalWhere = -1
        inObjectSubType = -1
        numCurrentColumn = 0
        numCurrentRow = 0
        numTotalColumns = 0
        self.currentSpeakerGroup = 0
        inSection = False
        self.chapters = []
        for wm in self.m.values():
            current = round(100 * I / total);
            I += 1
            if (current != last):
                last = current;
            inSection = False;
            keyWord = self.HTMLStyles();
            if (self.section < len(self.sections) and I >= self.sections[self.section].begin and I <= self.sections[self.section].endHeader):
                inSection = True;
                keyWord.setBackground((122, 224, 255))
                keyWord.setFontSize(22)
                keyWord.setBold(True)
                if (I == self.sections[self.section].begin):
                    sectionHeader = ""
                    si = self.sections[self.section].begin
                    if self.m[si].word == "chapter":
                        si += 1
                    while (si < self.sections[self.section].endHeader and len(sectionHeader) < 50):
                        if not self.m[si].word == "|||":
                            sectionHeader += self.m[si].word + " "
                        si += 1
                    self.chapters.append({ 'header': sectionHeader, 'where': si})
            if (wm.object >= 0 and len(self.objects) > wm.object):
                inObject = wm.object;
                if (self.objects[inObject].masterSpeakerIndex < 0 and len(self.objects[inObject].aliases) > 0):
                    for ai in self.objects[inObject].aliases.values():
                        if (self.objects[ai].masterSpeakerIndex >= 0):
                            inObject = ai
                            break
                inObjectSubType = self.objects[inObject].subType
                nextEndObject = wm.endObjectPosition
                if (endObject < nextEndObject):
                    endObject = nextEndObject;
                    principalWhere = I;
            if (I >= endObject):
                inObject = inObjectSubType = -1
            if (inObject >= 0):
                keyWord.setForeground((31, 82, 0))
            if (I == principalWhere or (inObject >= 0 and self.objects[inObject].objectClass == SourceEnums.NAME_OBJECT_CLASS)):
                keyWord.setForeground((31, 134, 0))
                if ((wm.objectRole & CObject.POV_OBJECT_ROLE) != 0):  
                    keyWord.setBackground((0, 0, 0))
            if (inObjectSubType >= 0):
                keyWord.setBackground((204, 0, 255));
                keyWord.setForeground((255, 255, 255));
            self.setTimeColorAttributes(wm.timeColor, keyWord)
            self.setVerbSenseAttributes(wm.verbSense, keyWord)
            if (wm.verbSense == 0 and inObject >= 0 and self.objects[inObject].masterSpeakerIndex >= 0):
                msindex = self.objects[inObject].masterSpeakerIndex % 8 #len(self.masterSpeakerColors);
                keyWord.setForeground(self.masterSpeakerColors[msindex]);
            self.paragraphSpacingOff.append(len(self.batchDoc))
            if wm.word == "|||":
                self.addElement("\n", None, I, -1, -1, self.SourceMapType.wordType);
            self.printRelation(I, preferences);
            self.printSpeakerGroup(I, preferences);
            if not wm.word == "|||":
                smt = self.SourceMapType.wordType;
                if (inSection):
                    smt = self.SourceMapType.headerType;
                tablesmart = True;
                if (tablesmart):
                    if wm.word == "lpendcolumnheaders":
                        numTotalColumns = numCurrentColumn;
                        numCurrentColumn = 0;
                        continue;
                    if (I > 3 and self.m[I - 2].word == "lptable") or wm.word == "lpendcolumn":
                        self.addElement("\n", None, I, self.section, -1, self.SourceMapType.headerType);
                        if wm.word == "lpendcolumn" and (I + 2 < len(self.m) and not self.m[I + 2].word == "lpendcolumnheaders" and not self.m[I + 2].word == "lptable"):
                            numCurrentColumn += 1
                            if (numCurrentColumn > numTotalColumns and numTotalColumns > 0):
                                numCurrentColumn = 0;
                                numCurrentRow += 1
                            keyWord.setForeground((255, 165, 0));  
                            if (numCurrentRow == 0 and numTotalColumns == 0):
                                self.addElement("Column Title." + str(numCurrentColumn) + ":", keyWord, I,
                                        self.section, -1, self.SourceMapType.headerType);
                            else:
                                self.addElement(str(numCurrentRow) + "." + str(numCurrentColumn)
                                        +":", keyWord, I, self.section, -1, self.SourceMapType.headerType);
                        continue;
                    if I > 1 and (self.m[I - 1].word == "lpendcolumn"
                            or (I + 1 < len(self.m) and self.m[I + 1].word == "lpendcolumn")):
                        continue;
                    if I > 1 and self.m[I - 1].word == "lptable":
                        keyWord.setForeground((255, 0, 0))  
                        keyWord.setFontSize(25)
                        keyWord.setBold(True)
                        keyWord.setUnderline(True)
                        self.addElement(str(int(wm.word) + 1), keyWord, I, self.section, -1, smt)
                        continue
                    if wm.word == "lptable":
                        self.addElement("\n", None, I, self.section, -1, self.SourceMapType.headerType)
                        self.addElement("\n", None, I, self.section, -1, self.SourceMapType.headerType)
                        smt = self.SourceMapType.tableType
                        keyWordTable = self.HTMLStyles()
                        keyWordTable.setForeground((255, 0, 0))  
                        keyWordTable.setBold(True)
                        keyWordTable.setFontSize(25)
                        keyWordTable.setUnderline(True)
                        keyWord = keyWordTable
                        numCurrentColumn = 0
                        self.addElement("TABLE ", keyWord, I, self.section, -1, smt)
                        # numTable++
                        continue
                originalWord = self.getOriginalWord(wm)
                if I+1<len(self.m) and self.m[I + 1] != '.' and self.m[I + 1] != "|||":
                    originalWord += " "     
                self.addElement(originalWord, keyWord, I, self.section, -1, smt)
            if wm.word == "“" or wm.word == "‘":
                # if (wm.word.equals("“"))
                # lastOpeningPrimaryQuote = I;
                if ((wm.flags & WordMatch.flagQuotedString) != 0):
                    keyWordQS = self.HTMLStyles();
                    keyWordQS.setForeground((0, 255, 0));  
                    self.addElement("QS", keyWordQS, I, -1, -1, self.SourceMapType.QSWordType);  # green
                elif (len(wm.objectMatches) != 1 or len(wm.audienceObjectMatches) == 0
                        or (self.currentSpeakerGroup < len(self.speakerGroups)
                                and len(wm.audienceObjectMatches) >= len(self.speakerGroups[self.currentSpeakerGroup].speakers)
                                and len(self.speakerGroups[self.currentSpeakerGroup].speakers) > 1)):
                    keyWordRED = self.HTMLStyles();
                    keyWordRED.setForeground((255, 0, 0));  
                    self.addElement("Unresolved", keyWordRED, I, -1, -1, self.SourceMapType.URWordType);  # green
                # only mark stories that have a speaker relating a tale which does
                # not involve the person being spoken to
                # so 'we' does not mean the speaker + the audience, if there are any
                # other agents mentioned in the story before
                if ((wm.flags & WordMatch.flagEmbeddedStoryResolveSpeakers) != 0):  
                    keyWordRED = self.HTMLStyles()
                    keyWordRED.setForeground((255, 0, 0));  
                    ESL = "ES" + ('1' if (wm.flags & WordMatch.flagFirstEmbeddedStory) != 0 else ' ') + ('2' if (wm.flags & WordMatch.flagSecondEmbeddedStory) != 0 else ' ')  
                    self.addElement(ESL, keyWordRED, I, -1, -1, self.SourceMapType.ESType);
            if not inSection:
                keyWord.removeBackground()  
            for J in range(len(wm.objectMatches)):
                mObject = wm.objectMatches[J].object;
                objectWhere = self.objects[mObject].originalLocation;
                # skip honorific
                if ((self.m[objectWhere].queryWinnerForm(Form.honorificForm) >= 0  
                        or self.m[objectWhere].queryWinnerForm(Form.honorificAbbreviationForm) >= 0)  
                        and objectWhere + 1 < self.m[objectWhere].endObjectPosition):
                    objectWhere += 1
                    if self.m[objectWhere].word == ".":
                        objectWhere += 1
                if mObject == 0:
                    w = "Narrator"
                elif mObject == 1:
                    w = "Audience"
                else:
                    w = self.m[objectWhere].word;
                S = ("[" if (J == 0) else "") + w + ("," if (J < len(wm.objectMatches) - 1) else "")
                if (self.objects[mObject].masterSpeakerIndex >= 0):
                    msindex = self.objects[mObject].masterSpeakerIndex % len(self.masterSpeakerColors)
                    keyWord.setForeground(self.masterSpeakerColors[msindex]);
                    self.addElement(S, keyWord, I, mObject, J, self.SourceMapType.matchingSpeakerType);
                else:
                    self.addElement(S, None, I, mObject, J, self.SourceMapType.matchingObjectType)
                    if (J > 2):
                        self.addElement("...", None, I, mObject, J, self.SourceMapType.matchingObjectType)
                        break
            if (len(wm.objectMatches) > 0 and len(wm.audienceObjectMatches) > 0):
                self.addElement(":", None, I, -1, -1, self.SourceMapType.matchingType)
            if (len(wm.objectMatches) > 0 and len(wm.audienceObjectMatches) == 0):
                self.addElement("]", None, I, -1, -1, self.SourceMapType.matchingType)
            if (len(wm.objectMatches) == 0 and len(wm.audienceObjectMatches) > 0):
                self.addElement("[", None, I, -1, -1, self.SourceMapType.matchingType)
            for J in range(len(wm.audienceObjectMatches)):
                keyWord.setForeground((102, 204, 255));
                mObject = wm.audienceObjectMatches[J].object;
                if (mObject == 0):
                    w = "Narrator";
                elif (mObject == 1):
                    w = "Audience";
                else:
                    w = self.m[self.objects[mObject].originalLocation].word;
                self.addElement(w + ("," if (J < len(wm.audienceObjectMatches) - 1) else "]"), None, I,
                        wm.audienceObjectMatches[J].object, J, self.SourceMapType.audienceMatchingType);
                if (J > 2):
                    self.addElement("...]", None, I, wm.audienceObjectMatches[J].object, J,
                            self.SourceMapType.audienceMatchingType)
                    break
            newHeader = self.section < len(self.sections) and I == self.sections[self.section].endHeader
            if (newHeader):
                self.addElement("\n", None, I, self.section, -1, self.SourceMapType.headerType)
            if wm.word == "|||":
                continue
            if (self.section < len(self.sections) - 1 and I >= self.sections[self.section].endHeader):
                self.section += 1
        return self.batchDoc

    def getStyle(self, attr):
        htmlStyle = ""
        if attr is None:
            return htmlStyle
        if self.HTMLStyles.UNDERLINE in attr:
            htmlStyle += "text-decoration: underline; "
        if self.HTMLStyles.BOLD in attr:
            htmlStyle += "font-weight: bold; "
        if self.HTMLStyles.FONTSIZE in attr:
            htmlStyle += "font-size: " + str(attr[self.HTMLStyles.FONTSIZE]) + "px; "
        if self.HTMLStyles.FOREGROUND in attr:
            htmlStyle += "color: rgb(" + json.dumps(attr[self.HTMLStyles.FOREGROUND])[1:-1] + "); "
        if self.HTMLStyles.BACKGROUND in attr:
            htmlStyle += "background-color: rgb(" + json.dumps(attr[self.HTMLStyles.BACKGROUND])[1:-1] + "); "
        return htmlStyle
            
    def printHTMLPerElement(self, e, where):
        #e(s, attrs, where, index2, index3, sourceMapType)
        html = {}
        html['mouseover'] = where
        if e[0] == "\n":
            html['style'] = "display:block"
        else:
            html['style'] = self.getStyle(e[1])
            html['text'] = e[0]
        return html
        
    def printHTML(self, fromWhere, width, maxHeight):
        where = fromWhere
        linePosition = 0
        height = 0
        lineHeight = 12
        htmlElements = []
        while height < maxHeight and where < len(self.batchDoc):
            # print(height,maxHeight,where,len(self.batchDoc))
            htmlElement = self.printHTMLPerElement(self.batchDoc[where], where)
            htmlElements.append(htmlElement)
            length = 0 if 'text' not in htmlElement else len(htmlElement['text'])
            if length + linePosition > width:
                height += lineHeight
                linePosition = length
            else:
                linePosition += length
            where += 1
        return htmlElements    
    
    # for determining whether CSS Classes can be enumerated separately
    def generateCSSClasses(self):
        global attrsList
        css_output = ""
        for attr in attrsList:
            style_name = str(attr)
            css_output += "s_" + style_name + " {\n"
            for a in attrsList[attr]:
                if a == self.HTMLStyles.UNDERLINE:
                    css_output += "text-decoration: underline;\n"
                if a == self.HTMLStyles.BOLD:
                    css_output += "font-weight: bold;\n"
                if a == self.HTMLStyles.FOREGROUND:
                    css_output += "color: rgb(" + json.dumps(attrsList[attr][a])[1:-1] + ");\n"
                if a == self.HTMLStyles.BACKGROUND:
                    css_output += "background-color: rgb(" + json.dumps(attrsList[attr][a])[1:-1] + ");\n"
                if a == self.HTMLStyles.FONTSIZE:
                    css_output += "font-size: " + str(attrsList[attr][a]) + "px;\n"
            css_output += "}\n"
        return css_output
