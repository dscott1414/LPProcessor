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
from TimeInfo import TimeInfo
import copy
import json
from VerbNet import VerbNet

try:
    profile  # throws an exception when profile isn't defined
except NameError:
    profile = lambda x: x   # if it's not defined simply ignore the decorator.

class Source:

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
        agentType = 17

    class HTMLStyles(dict):
        UNDERLINE = 0
        FONTSIZE = 1
        FOREGROUND = 2
        BACKGROUND = 3
        BOLD = 4
        SPACEBEFORE = 5
        SPACEAFTER = 6
        
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
            
        def setSpaceBefore(self, att):
            self.__setitem__(self.SPACEBEFORE, att)
            return self
            
        def setSpaceAfter(self, att):
            self.__setitem__(self.SPACEAFTER, att)
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
        self.vn = VerbNet()
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
        self.timelineSegments = []
        for I in range(count):
            self.timelineSegments.append(TimelineSegment(rs))

    @profile
    def addElement(self, s, attrs, where, index2, index3, sourceMapType):
        if where not in self.sourceToHTMLElementPosition:
            self.sourceToHTMLElementPosition[where] = []
        self.sourceToHTMLElementPosition[where].append(len(self.batchDoc))
        self.HTMLElementIdToSource[len(self.batchDoc)] = where
        self.batchDoc.append((s, copy.copy(attrs), where, index2, index3, sourceMapType))
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
        return tmp + str(om.salienceFactor)

    @profile
    def objectsString(self, oms, shortNameFormat, objectOwnerRecursionFlag):
        tmp = "";
        for s in oms:
            tmp += self.objectString(s, shortNameFormat, objectOwnerRecursionFlag)
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
            for om in self.m[ow].objectMatches:
                if obj == om.object:
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
            if ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PRESENT):
                verbSenseStr += "PRESENT "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PRESENT_PERFECT):
                verbSenseStr += "PRESENT_PERFECT "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PAST):
                verbSenseStr += "PAST "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PAST_PERFECT):
                verbSenseStr += "PAST_PERFECT "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_FUTURE):
                verbSenseStr += "FUTURE "
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_FUTURE_PERFECT):
                verbSenseStr += "FUTURE_PREFECT "
            if ((verbSense & SourceEnums.VT_EXTENDED) == SourceEnums.VT_EXTENDED):
                verbSenseStr += "EXTENDED "
            if ((verbSense & SourceEnums.VT_PASSIVE) == SourceEnums.VT_PASSIVE):
                verbSenseStr += "PASSIVE "
            if ((verbSense & SourceEnums.VT_NEGATION) == SourceEnums.VT_NEGATION):
                verbSenseStr += "NEGATION "
            if ((verbSense & SourceEnums.VT_IMPERATIVE) == SourceEnums.VT_IMPERATIVE):
                verbSenseStr += "POSSIBLE "
            if ((verbSense & SourceEnums.VT_POSSIBLE) == SourceEnums.VT_POSSIBLE):
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

    def shouldPrintRelation(self, where, spr, preferences):
        printAll = True
        printNone = True
        for p in preferences.values():
            if (not p):
                printAll = False
            if (p):
                printNone = False
        if (printAll):
            if where<20: print(str(where) + " 1: True")
            return True
        if (printNone):
            if where<20: print(str(where) + " 2: False")
            return False
        if (preferences[SourceEnums.INCLUDE_STORY] and self.relations[self.spr].story):
            if where<20: print(str(where) + " 3: True")
            return True
        if (preferences[SourceEnums.INCLUDE_QUOTES] and (self.m[self.relations[self.spr].where].objectRole & CObject.IN_PRIMARY_QUOTE_ROLE) != 0):  
            if where<20: print(str(where) + " 4: True")
            return True
        if (preferences[SourceEnums.INCLUDE_TIME_TRANSITION] and self.relations[self.spr].timeTransition):
            if where<20: print(str(where) + " 5: True")
            return True
        timeRelation = len(self.relations[self.spr].timeInfo) > 0 or (self.relations[self.spr].relationType == SourceEnums.stABSTIME
                or self.relations[self.spr].relationType == SourceEnums.stPREPTIME or self.relations[self.spr].relationType == SourceEnums.stPREPDATE
                or self.relations[self.spr].relationType == SourceEnums.stSUBJDAYOFMONTHTIME or self.relations[self.spr].relationType == SourceEnums.stABSTIME
                or self.relations[self.spr].relationType == SourceEnums.stABSDATE or self.relations[self.spr].relationType == SourceEnums.stADVERBTIME)
        if (preferences[SourceEnums.INCLUDE_ABSTRACT] and not (self.relations[self.spr].physicalRelation or timeRelation)):
            if where<20: print(str(where) + " 6: True")
            return True
        if (preferences[SourceEnums.INCLUDE_TIME] and timeRelation):
            if where<20: print(str(where) + " 7: True")
            return True
        if (preferences[SourceEnums.INCLUDE_SPACE] and self.relations[self.spr].physicalRelation):
            if where<20: print(str(where) + " 8: True")
            return True
        if where<20: print(str(where) + " 9: False")
        return False

    def printRelation(self, where, preferences):
        if (self.spr < len(self.relations) and self.relations[self.spr].where == where):
            if where<20: print(str(where) + ": printRelation 2")
            if (self.shouldPrintRelation(where, self.spr, preferences)):
                if where<20: print(str(where) + ": printRelation 3")
                self.addElement(self.relationString(self.relations[self.spr].relationType), self.setAttributes(self.relations, self.spr), where, self.spr, -1,
                        self.SourceMapType.relType);
        while (self.spr < len(self.relations) and self.relations[self.spr].where < where):
            self.spr = self.relations[self.spr].nextSPR;

    def printFullRelationString(self, originalSPRI, endSpeakerGroup):
        presType = "";
        r = self.relations[originalSPRI];
        if (r.futureHappening and r.wherePrepObject >= 0 and self.m[r.wherePrepObject].object >= 0
                and self.objects[self.m[r.wherePrepObject].object].subType == SourceEnums.BY_ACTIVITY):
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
        if (self.relations[spr].agentLocationRelationSet):
            originalSPRI = spr;
            spr = self.relations[spr].nextSPR;
            if (self.shouldPrintRelation(where, originalSPRI, preferences)):
                w = self.printFullRelationString(originalSPRI, endSpeakerGroup)
                self.addElement(w, self.setAttributes(self.relations, originalSPRI), where, spr, -1, self.SourceMapType.relType2)
                self.addElement("\n", self.setAttributes(self.relations, originalSPRI), where, spr, -1, self.SourceMapType.relType2)
            if (self.relations[originalSPRI].whereControllingEntity != -1 and self.relations[spr].whereControllingEntity == -1
                    and self.relations[originalSPRI].whereSubject == self.relations[spr].whereSubject
                    and self.relations[originalSPRI].whereVerb == self.relations[spr].whereVerb):
                spr += 1
        else:
            spr += 1
        return spr

    def masterSpeakerHtml(self, ms):
        html = {}
        html['style'] = "font-weight: bold; "
        msindex = ms % len(self.masterSpeakerColors)
        html['style'] += "color: rgb(" + json.dumps(self.masterSpeakerColors[msindex])[1:-1] + "); "
        # html['style'] = "display:block; "
        # html['mouseover'] = str(line)
        masterSpeakerObject = self.masterSpeakerList[ms]  
        s = self.objectNumString(masterSpeakerObject, False, False)
        s += " ds[" + str(self.objects[masterSpeakerObject].numDefinitelyIdentifiedAsSpeaker) + "] s[" \
                + str(self.objects[masterSpeakerObject].numIdentifiedAsSpeaker) + "]";
        html['text'] = s  
        self.positionToAgent[ms] = masterSpeakerObject
        self.agentToPosition[masterSpeakerObject] = ms
        return html

    @profile
    def printSpeakerGroup(self, I, preferences):
        while (self.currentSpeakerGroup < len(self.speakerGroups) and I == self.speakerGroups[self.currentSpeakerGroup].begin):
            if (self.speakerGroups[self.currentSpeakerGroup].tlTransition):
                keyWord = self.HTMLStyles()
                keyWord.setBold(True)
                keyWord.setForeground((0xFF, 0x00, 0x00))
                self.addElement("------------------TRANSITION------------------", keyWord, I, self.currentSpeakerGroup, -1,
                        self.SourceMapType.transitionSpeakerGroupType)
            self.addElement("\n", None, I, self.currentSpeakerGroup, -1, self.SourceMapType.speakerGroupType)
            for mo in self.speakerGroups[self.currentSpeakerGroup].speakers:
                isPOV = mo in self.speakerGroups[self.currentSpeakerGroup].povSpeakers
                isObserver = mo in self.speakerGroups[self.currentSpeakerGroup].observers
                isGrouped = mo in self.speakerGroups[self.currentSpeakerGroup].groupedSpeakers
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
                self.addElement(oStr, keyWord, I, self.currentSpeakerGroup, mo, self.SourceMapType.speakerGroupType)
                self.addElement("\nbr", None, I, -1, -1, self.SourceMapType.speakerGroupType)
            self.addElement("\n", None, I, self.currentSpeakerGroup, -1, self.SourceMapType.speakerGroupType)
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
            for mo in self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].speakers:
                isPOV = mo in self.speakerGroups[self.currentSpeakerGroup
                        -1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].povSpeakers
                isObserver = mo in self.speakerGroups[self.currentSpeakerGroup
                        -1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].observers
                isGrouped = mo in self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].groupedSpeakers
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
                self.addElement(oStr, keyWord, I, self.currentSpeakerGroup - 1, self.currentEmbeddedSpeakerGroup,
                        self.SourceMapType.speakerGroupType)
                self.addElement("\nbr", None, I, -1 , -1,
                                self.SourceMapType.speakerGroupType)
            self.addElement("\n", None, I, -1 , -1,
                            self.SourceMapType.speakerGroupType)
            self.currentEmbeddedSpeakerGroup += 1
        if (self.currentEmbeddedSpeakerGroup >= 0 and self.currentSpeakerGroup > 0
                and self.currentEmbeddedSpeakerGroup < len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups)
                and I == self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].end):
            self.addElement("END", None, I, self.currentSpeakerGroup, self.currentEmbeddedSpeakerGroup,
                    self.SourceMapType.speakerGroupType);
            self.currentEmbeddedSpeakerGroup = -1;

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

    """
    1. create state variables for each source position
    2. if source position is the same as the previous source position, do not create that (possible improvement)
    3. rerun generateSourceElements, and verify that batchDoc is identical
    4. THEN only run generateSourceElements when generating HTML elements.
    5. keep array by position. keep intervals.
    6. when new interval comes, | new interval with already generated intervals
    7. subtract the previous intervals.
    8. generate only what is left.
    9. save the sum as the new interval set.
    """
    

    @profile
    def generatePerElementState(self, words, preferences):
        self.chaptersCheck = []
        self.section = 0;
        self.spr = 0;
        I = -1
        # state variables
        inObject = -1 
        endObject = 0 
        principalWhere = -1 
        inObjectSubType = -1
        numCurrentColumn = 0
        numCurrentRow = 0
        numTotalColumns = 0
        self.currentSpeakerGroup = 0
        inSection = False
        states = []
        for wm in self.m.values():
            I += 1
            inSection = False;
            if (self.section < len(self.sections) and I >= self.sections[self.section].begin and I <= self.sections[self.section].endHeader):
                inSection = True;
                if (I == self.sections[self.section].begin):
                    sectionHeader = ""
                    si = self.sections[self.section].begin
                    if self.m[si].word == "chapter":
                        si += 1
                    while (si < self.sections[self.section].endHeader and len(sectionHeader) < 50):
                        if not self.m[si].word == "|||":
                            sectionHeader += self.m[si].word + " "
                        si += 1
                    self.chaptersCheck.append({ 'header': sectionHeader, 'where': I,  'whereEndHeader': si})
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
            self.printRelation(I, preferences);
            if not wm.word == "|||":
                if wm.word == "lpendcolumnheaders":
                    numTotalColumns = numCurrentColumn;
                    numCurrentColumn = 0;
                    continue;
                if (I > 3 and self.m[I - 2].word == "lptable") or wm.word == "lpendcolumn":
                    if wm.word == "lpendcolumn" and (I + 2 < len(self.m) and not self.m[I + 2].word == "lpendcolumnheaders" and not self.m[I + 2].word == "lptable"):
                        numCurrentColumn += 1
                        if (numCurrentColumn > numTotalColumns and numTotalColumns > 0):
                            numCurrentColumn = 0;
                            numCurrentRow += 1
                    continue;
                if I > 1 and (self.m[I - 1].word == "lpendcolumn"
                        or (I + 1 < len(self.m) and self.m[I + 1].word == "lpendcolumn")):
                    continue;
                if wm.word == "lptable":
                    numCurrentColumn = 0
                    continue
            if wm.word == "|||":
                continue
            if (self.section < len(self.sections) - 1 and I >= self.sections[self.section].endHeader):
                self.section += 1
            states.append([self.spr, inSection, self.section, inObject, endObject, principalWhere, inObjectSubType, numCurrentColumn, numCurrentRow, numTotalColumns, self.currentSpeakerGroup])
        return states

    @profile
    def generateSourceElements(self, words, preferences):
        states = self.generatePerElementState(words, preferences)
        self.sourceToHTMLElementPosition = {}
        self.HTMLElementIdToSource = {}
        self.positionToAgent = {}
        self.agentToPosition = {}
        self.batchDoc = []
        self.chapters = []
        self.section = 0;
        self.docPosition = 0;
        self.spr = 0;
        total = len(self.m)
        I = -1
        last = -1 # keep percentage
        # state variables
        inObject = -1 
        endObject = 0 
        principalWhere = -1 
        inObjectSubType = -1
        numCurrentColumn = 0
        numCurrentRow = 0
        numTotalColumns = 0
        self.currentSpeakerGroup = 0
        inSection = False
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
                    self.chapters.append({ 'header': sectionHeader, 'where': I,  'whereEndHeader': si})
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
            if wm.word == "|||":
                self.addElement("\n", None, I, -1, -1, self.SourceMapType.wordType);
            self.printRelation(I, preferences);
            self.printSpeakerGroup(I, preferences);
            if not wm.word == "|||":
                smt = self.SourceMapType.wordType;
                if (inSection):
                    smt = self.SourceMapType.headerType;
                if wm.word == "lpendcolumnheaders":
                    numTotalColumns = numCurrentColumn;
                    numCurrentColumn = 0;
                elif (I > 3 and self.m[I - 2].word == "lptable") or wm.word == "lpendcolumn":
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
                elif I <= 1 or (self.m[I - 1].word != "lpendcolumn"
                        and (I + 1 >= len(self.m) or self.m[I + 1].word != "lpendcolumn")):
                    if I > 1 and self.m[I - 1].word == "lptable":
                        keyWord.setForeground((255, 0, 0))  
                        keyWord.setFontSize(25)
                        keyWord.setBold(True)
                        keyWord.setUnderline(True)
                        self.addElement(str(int(wm.word) + 1), keyWord, I, self.section, -1, smt)
                    elif wm.word == "lptable":
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
                    else:
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
            J = 0  
            for om in wm.objectMatches:
                mObject = om.object;
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
                J += 1
            if (len(wm.objectMatches) > 0 and len(wm.audienceObjectMatches) > 0):
                self.addElement(":", None, I, -1, -1, self.SourceMapType.matchingType)
            if (len(wm.objectMatches) > 0 and len(wm.audienceObjectMatches) == 0):
                self.addElement("]", None, I, -1, -1, self.SourceMapType.matchingType)
            if (len(wm.objectMatches) == 0 and len(wm.audienceObjectMatches) > 0):
                self.addElement("[", None, I, -1, -1, self.SourceMapType.matchingType)
            J = 0
            for aom in wm.audienceObjectMatches:
                keyWord.setForeground((102, 204, 255));
                mObject = aom.object;
                if (mObject == 0):
                    w = "Narrator";
                elif (mObject == 1):
                    w = "Audience";
                else:
                    w = self.m[self.objects[mObject].originalLocation].word;
                self.addElement(w + ("," if (J < len(wm.audienceObjectMatches) - 1) else "]"), None, I,
                        aom.object, J, self.SourceMapType.audienceMatchingType);
                if (J > 2):
                    self.addElement("...]", None, I, aom.object, J,
                            self.SourceMapType.audienceMatchingType)
                    break
                J += 1
            newHeader = self.section < len(self.sections) and I == self.sections[self.section].endHeader
            if (newHeader):
                self.addElement("\n", None, I, self.section, -1, self.SourceMapType.headerType)
            if wm.word != "|||" and (self.section < len(self.sections) - 1 and I >= self.sections[self.section].endHeader):
                self.section += 1
            if states[I] == [self.spr, inSection, self.section, inObject, endObject, principalWhere, inObjectSubType, numCurrentColumn, numCurrentRow, numTotalColumns, self.currentSpeakerGroup]:
                print("GOOD:" + str(I))
            else:
                print("BAD:" + str(I))
                print(states[I])
                print([self.spr, inSection, self.section, inObject, endObject, principalWhere, inObjectSubType, numCurrentColumn, numCurrentRow, numTotalColumns, self.currentSpeakerGroup])
                break
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
        if self.HTMLStyles.SPACEBEFORE in attr:
            htmlStyle += "margin-top: " + str(attr[self.HTMLStyles.SPACEBEFORE]) + "px; "
        if self.HTMLStyles.SPACEAFTER in attr:
            htmlStyle += "margin-bottom: " + str(attr[self.HTMLStyles.SPACEAFTER]) + "px; "
        return htmlStyle
            
    def printHTMLPerElement(self, e, htmlElementPosition):
        #e(s, attrs, where, index2, index3, sourceMapType)
        html = {}
        html['mouseover'] = str(htmlElementPosition)  
        if e[0] == "\n":
            html['style'] = "display:block; margin-bottom: 10px; "
        elif e[0] == "\nbr":
            html['style'] = "display:block; "
        else:
            html['style'] = self.getStyle(e[1])
            html['text'] = e[0]
        return html
        
    def printHTML(self, fromWhere, width, maxHeight):
        where = fromWhere
        linePosition = 0
        height = 0
        lineHeight = 16
        letterWidth = 7
        htmlElements = []
        numPages = 2 # before they have to scroll!
        maxHeight = maxHeight * numPages 
        while height < maxHeight and where < len(self.batchDoc):
            # print(height,maxHeight,where,len(self.batchDoc))
            htmlElement = self.printHTMLPerElement(self.batchDoc[where], where)
            htmlElements.append(htmlElement)
            length = 0 if 'text' not in htmlElement else len(htmlElement['text'])*letterWidth
            if length + linePosition > width:
                height += lineHeight
                linePosition = length
                # if 'text' in htmlElement:
                #    print("\n" + htmlElement['text'], end='')
            else:
                linePosition += length
                # if 'text' in htmlElement:
                #    print(htmlElement['text'], end='')
            if "display:block" in htmlElement['style']:
                #print("\n")
                #if 'text' in htmlElement:
                #    print(htmlElement['text'], end='')
                linePosition=0
                height += lineHeight + 10
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
                if a == self.HTMLStyles.SPACEBEFORE:
                    css_output += "margin-top: " + str(attrsList[attr][a]) + "px;\n"
                if a == self.HTMLStyles.SPACEAFTER:
                    css_output += "margin-bottom: " + str(attrsList[attr][a]) + "px;\n"
            css_output += "}\n"
        return css_output
    
    def getInfoPanel(self, htmlElementPosition):
        where = self.HTMLElementIdToSource[htmlElementPosition]
        where2 = self.batchDoc[htmlElementPosition][2]
        where3 = self.batchDoc[htmlElementPosition][3]
        where4 = self.batchDoc[htmlElementPosition][4]
        sourceMapType = self.batchDoc[htmlElementPosition][5]
        wordInfo = ""
        toolTip = ""
        roleInfo = ""
        relationsInfo = ""
        if sourceMapType == self.SourceMapType.wordType:
            wordInfo = str(where) + ": " + self.m[where].getWinnerForms() + self.m[where].word
            toolTip = self.m[where].word
            roleInfo = self.m[where].roleString()
            relationsInfo = self.m[where].relations()
            if len(self.m[where].baseVerb) > 0:
                roleInfo = self.vn.getClassNames2(self.m[where].baseVerb)
                verbSenseStr = self.getVerbSenseString(self.m[where].quoteForwardLink);
                wordInfo = str(where) + ": " + self.m[where].word + "(" + verbSenseStr + ")"
        if sourceMapType == self.SourceMapType.relType:
            r = Source.relations[where2];
            if (r.beforePastHappening):
                wordInfo.setText("beforePast");
            if (r.futureHappening):
                wordInfo.setText("future");
            if (r.futureInPastHappening):
                wordInfo.setText("futureInPast");
            if (r.futureLocation):
                wordInfo.setText("futureLocation");
            if (r.pastHappening):
                wordInfo.setText("past");
            if (r.presentHappening):
                wordInfo.setText("present");
            firstMultiple = where2 - 1
            while firstMultiple >= 0 and Source.relations[firstMultiple].where == r.where:
                firstMultiple -= 1
            firstMultiple += 1
            lastMultiple = firstMultiple + 1
            while lastMultiple < Source.relations.len() and Source.relations[lastMultiple].where == r.where:
                lastMultiple += 1
            lastMultiple -= 1
            relationsInfo = ("M" if firstMultiple != lastMultiple else "") + ("TT" if r.timeTransition else "") + r.description
            roleInfo = TimeInfo.toString(self, r)
        if sourceMapType == self.SourceMapType.relType2:
            # w, self.setAttributes(self.relations, originalSPRI), where, spr, -1, self.SourceMapType.relType2
            wordInfo = str(where) + ": space relation #=" + str(where3) 
        if sourceMapType == self.SourceMapType.transitionSpeakerGroupType or \
            sourceMapType == self.SourceMapType.speakerGroupType:
            wordInfo = str(where) + ": speakerGroup #=" + str(where2) 
            if where3>=0:
                wordInfo += " embedded speakerGroup #=" + str(where3)
            if where4>=0:
                wordInfo += " object=" + self.objectNumString(where4, False, False)
        if sourceMapType == self.SourceMapType.QSWordType:
            wordInfo = str(where) + ": QUOTED STRING"
        if sourceMapType == self.SourceMapType.ESType:
            wordInfo = str(where) + ": EMBEDDED STORY"
        if sourceMapType == self.SourceMapType.URWordType:
            wordInfo = str(where) + ": UNRESOLVED"
        if sourceMapType == self.SourceMapType.headerType:
            wordInfo = str(where) + ": section#=" + str(where3) + " " + str(sourceMapType).split('.')[1]
        if  sourceMapType == self.SourceMapType.matchingSpeakerType or \
            sourceMapType == self.SourceMapType.matchingType or \
            sourceMapType == self.SourceMapType.audienceMatchingType or \
            sourceMapType == self.SourceMapType.matchingObjectType:
            if where3 == -1:
                wordInfo = str(where) + ": " + str(sourceMapType).split('.')[1]
            else:
                wordInfo = str(where) + ": matching object #" + str(where4) + "=" + self.objectNumString(where3, False, False) + " " + str(sourceMapType).split('.')[1]

        if sourceMapType == self.SourceMapType.beType:
            beObject = self.m[where].object;
            if self.m[where].objectMatches.len() == 1:
                beObject = self.m[where].objectMatches[0].object;
            wordInfo = where + ":" + self.objectString(beObject, False, False)
            relationsInfo = "acquired through BE relation:"
            if object >= 0:
                for I in self.objects[object].associatedAdjectives:
                    roleInfo += I;
        if sourceMapType == self.SourceMapType.possessionType:
            posObject = self.m[where].object;
            if self.m[where].objectMatches.len() == 1:
                posObject = self.m[where].objectMatches[0].object;
            wordInfo = where + ":" + self.objectString(posObject, False, False)
            relationsInfo = "HAS-A relation:"
            if posObject >= 0:
                for I in self.objects[posObject].possessions:
                    roleInfo += I;
        return wordInfo, roleInfo, relationsInfo, toolTip 

    def getWordInfo(self,fromIndex, toIndex, pageNumber, pageSize):
        fromIndex = self.HTMLElementIdToSource[int(fromIndex)]
        toIndex = self.HTMLElementIdToSource[int(toIndex)]
        wordInfo = []
        for I in range(fromIndex,toIndex + 1):
            word = str(I) + ":" + self.m[I].getWinnerForms() + self.m[I].word
            roleVerbClass = self.m[I].roleString()
            relations = self.m[I].relations()
            objectInfo = self.objectNumString(self.m[I].object, False, False)
            matchingObjects = self.objectsString(self.m[I].objectMatches, False, True)
            # locations are accumulated from processing source - not read from file
            # if self.m[I].object >= 0 and self.objects[self.m[I].object].locations != None:
            #    locations = self.objects[self.m[I].object].locations
            flags = self.m[I].printFlags()
            if len(self.m[I].baseVerb) > 0 and len(roleVerbClass) == 0:
                flags = self.vn.getClassNames(self.m[I].baseVerb)
            wordInfo.append({ 'word': word, 'roleVerbClass':roleVerbClass, 'relations': relations, 'objectInfo':objectInfo, 'matchingObjects':matchingObjects, 'flags':flags})
        return wordInfo

    def getTimelineSegments(self, pageNumber, pageSize):
        segments = []
        lastParent = None
        timelineIndex = 0
        for tl in self.timelineSegments:
            s = "[" + str(tl.begin) + "-" + str(tl.end) + "]";
            speakers = set()
            if tl.begin == self.speakerGroups[tl.speakerGroup].begin:
                tsg = tl.speakerGroup
                while tsg < len(self.speakerGroups) and self.speakerGroups[tsg].end < tl.end:
                    for mo in self.speakerGroups[tsg].speakers: 
                        if not mo in speakers:
                            speakers.add(mo)
                            s += self.objectNumString(mo, True, False).strip() + ","
                    tsg += 1
            else:
                for esg in self.speakerGroups[tl.speakerGroup].embeddedSpeakerGroups:
                    if tl.begin == esg.begin:
                        for mo in esg.speakers:
                            if not mo in speakers:
                                speakers.add(mo);
                                s += self.objectNumString(mo, True, False).strip() + ","
                        wm = self.m[tl.begin]
                        if (wm.flags & WordMatch.flagEmbeddedStoryResolveSpeakers) != 0:
                            if (wm.flags & WordMatch.flagFirstEmbeddedStory) != 0:
                                for mo in wm.objectMatches:
                                    if not mo.object in speakers:
                                        speakers.add(mo.object);
                                        s += self.objectNumString(mo.object, True, False).strip() + ","
                            if (wm.flags & WordMatch.flagSecondEmbeddedStory) != 0:
                                for mo in wm.audienceObjectMatches:
                                    if not mo.object in speakers:
                                        speakers.add(mo.object);
                                        s += self.objectNumString(mo.object, True, False).strip() + ","
                        break
            if s[-1] == ",":
                s = s[:-1]
            segment = {}
            segment['speakers'] = s
            segment['timeline'] = timelineIndex
            segment['children'] = []
            if tl.parentTimeline == -1 or lastParent == None:
                lastParent = segment
                segments.append(segment)
            else:
                lastParent['children'].append(segment);
            timelineIndex += 1
        return segments
    
    def getRelationsInTimeline(self, timelineIndex):
        relations = []
        for tl in self.timelineSegment[timelineIndex].timeTransitions:
            if self.relations[tl].timeTransition:
                color = (0x00, 0x00, 0xFF)
            elif self.relations[tl].nonPresentTimeTransition:
                color = (0x20, 0x20, 0xDF)
            else:
                color = (0x20, 0x20, 0x20)
            s = self.printFullRelationString(tl, -1) \
                    + TimeInfo.determineTimeProgression(self, self.relations[tl])
            relations.append((s,color))
        return relations

