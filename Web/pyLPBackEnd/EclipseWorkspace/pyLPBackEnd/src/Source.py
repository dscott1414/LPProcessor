from enum import Enum
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
import jsonpickle
from json import JSONEncoder
from VerbNet import VerbNet
from time import perf_counter
import re

class Source:

    wordOrderWords = [ "other", "another", "second", "first", "third", "former", "latter", "that", "this",
            "two", "three", "one", "four", "five", "six", "seven", "eight" ]

    OCSubTypeStrings = [ "canadian province city", "country", "island", "mountain range peak landform",
            "ocean sea", "park monument", "region", "river lake waterway", "us city town village",
            "us state territory region", "world city town village", "geographical natural feature",
            "geographical urban feature", "geographical urban subfeature", "geographical urban subsubfeature", "travel",
            "moving", "moving natural", "relative direction", "absolute direction", "by activity", "unknown(place)" ]
    currentEmbeddedSpeakerGroup = 0
    
    class SourceMapType(int, Enum):
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

        def set_underline(self, att):
            self.__setitem__(self.UNDERLINE, att)
            return self

        def set_font_size(self, att):
            self.__setitem__(self.FONTSIZE, att)
            return self

        def set_background(self, att):
            self.__setitem__(self.BACKGROUND, att)
            return self
    
        def remove_background(self):
            if self.BACKGROUND in self:
                self.__delitem__(self.BACKGROUND)
            return self
    
        def set_foreground(self, att):
            self.__setitem__(self.FOREGROUND, att)
            return self
    
        def set_bold(self, att):
            self.__setitem__(self.BOLD, att)
            return self
            
        def set_space_before(self, att):
            self.__setitem__(self.SPACEBEFORE, att)
            return self
            
        def set_space_after(self, att):
            self.__setitem__(self.SPACEAFTER, att)
            return self
            
    location = ""

    class WhereSource:

        def __init__(self, inIndex, inIndex2, inIndex3, inFlags):
            self.index = inIndex
            self.index2 = inIndex2;
            self.index3 = inIndex3;
            self.flags = inFlags;

    def initialize_class_strings(self):
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
        t_start = perf_counter()
        self.initialize_class_strings()
        self.vn = VerbNet()
        self.masterSpeakerList = {}
        self.version = rs.read_integer() 
        self.location = rs.read_string()
        print("Source(1) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer()
        self.m = []
        for I in range(count):
            self.m.append(WordMatch(rs))
        print("Source(2) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer()
        self.sentenceStarts = []
        for I in range(count):
            self.sentenceStarts.append(rs.read_integer())
        print("Source(3) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer()
        self.sections = []
        for I in range(count):
            self.sections.append(Section(rs))
        print("Source(4) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer()
        self.speakerGroups = []
        for I in range(count):
            self.speakerGroups.append(SpeakerGroup(rs))
        print("Source(5) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer()
        self.pema = []
        for I in range(count):
            self.pema.append(PatternElementMatch(rs))
        print("Source(6) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer()
        self.objects = []
        for I in range(count):
            self.objects.append(CObject(rs))
            if self.objects[I].masterSpeakerIndex >= 0:
                self.masterSpeakerList[self.objects[I].masterSpeakerIndex] = I
        print("Source(7) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer();
        self.relations = []
        for I in range(count):
            self.relations.append(Relation(rs))
        print("Source(8) Seconds = " + "{:.2f}".format(perf_counter() - t_start))
        t_start = perf_counter()
        count = rs.read_integer();
        self.timelineSegments = []
        for I in range(count):
            self.timelineSegments.append(TimelineSegment(rs))
        self.lineHeight = 16
        self.letterWidth = 7
        print("Source(9) Seconds = " + "{:.2f}".format(perf_counter() - t_start))


    def add_element(self, s, attrs, where, index2, index3, sourceMapType):
        if where not in self.batchDoc:
            self.batchDoc[where] = []
        self.batchDoc[where].append((s, copy.copy(attrs), where, index2, index3, sourceMapType))

    def get_original_word(self, wm):
        originalWord = wm.word;
        if (wm.query_form(SourceEnums.PROPER_NOUN_FORM_NUM) >= 0 or (wm.flags & WordMatch.flagFirstLetterCapitalized) != 0):
            originalWord = originalWord[0].upper() + originalWord[1:]
        if ((wm.flags & WordMatch.flagNounOwner) != 0):
            originalWord += "'s";
        if ((wm.flags & WordMatch.flagAllCaps) != 0):
            originalWord = originalWord.upper();
        return originalWord;

    def phrase_string(self, begin, end, shortFormat):
        tmp = ""
        for K in range(begin, end):
            tmp += self.get_original_word(self.m[K]) + " "
        if (begin != end and not shortFormat):
            tmp += "[" + str(begin) + "-" + str(end) + "]"
        return tmp

    def object_string(self, om, shortNameFormat, objectOwnerRecursionFlag):
        tmp = self.object_num_string(om.object, shortNameFormat, objectOwnerRecursionFlag)
        if (shortNameFormat):
            return tmp
        return tmp + str(om.salienceFactor)

    def objects_string(self, oms, shortNameFormat, objectOwnerRecursionFlag):
        tmp = "";
        for s in oms:
            tmp += self.object_string(s, shortNameFormat, objectOwnerRecursionFlag)
        return tmp

    def get_class(self, objectClass):
        return self.objectClassStrings[objectClass]

    def object_known_string(self, obj, shortFormat, objectOwnerRecursionFlag):
        if (obj == 0):
            return "Narrator";
        elif (obj == 1):
            return "Audience";
        if (self.objects[obj].objectClass == SourceEnums.NAME_OBJECT_CLASS):
            tmpstr = self.objects[obj].name.print(True)
        else:
            tmpstr = self.phrase_string(self.objects[obj].begin, self.objects[obj].end, shortFormat)
        ow = self.objects[obj].ownerWhere
        if (ow >= 0):
            selfOwn = obj == (self.m[ow].object)
            for om in self.m[ow].objectMatches:
                if obj == om.object:
                    tmpstr = "Self-owning obj!";
                    selfOwn = True;
            if not selfOwn and not objectOwnerRecursionFlag:
                if len(self.m[ow].objectMatches) > 0:
                    self.objects_string(self.m[ow].objectMatches, True, True)
                else:
                    tmpstr += self.object_num_string(self.m[ow].object, True, True)
            tmpstr += "{OWNER:" + tmpstr + "}"
        if (self.objects[obj].ownerWhere < -1):
            tmpstr += "{WO:" + self.wordOrderWords[-2 - self.objects[obj].ownerWhere] + "}"
        if not shortFormat:
            tmpstr += "[" + str(self.objects[obj].originalLocation) + "]"
            tmpstr += "[" + self.get_class(self.objects[obj].objectClass) + "]"
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
            tmpstr += "[" + self.phrase_string(self.objects[obj].whereRelativeClause,
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

    def object_num_string(self, obj, shortNameFormat, objectOwnerRecursionFlag):
        if obj < 0:
            if obj == SourceEnums.UNKNOWN_OBJECT: return "";
            if obj == SourceEnums.OBJECT_UNKNOWN_MALE: return "UNKNOWN_MALE";
            if obj == SourceEnums.OBJECT_UNKNOWN_FEMALE: return "UNKNOWN_FEMALE";
            if obj == SourceEnums.OBJECT_UNKNOWN_MALE_OR_FEMALE: return "UNKNOWN_MALE_OR_FEMALE";
            if obj == SourceEnums.OBJECT_UNKNOWN_NEUTER: return "UNKNOWN_NEUTER";
            if obj == SourceEnums.OBJECT_UNKNOWN_PLURAL: return "UNKNOWN_PLURAL";
            if obj == SourceEnums.OBJECT_UNKNOWN_ALL: return "ALL";
        return self.object_known_string(obj, shortNameFormat, objectOwnerRecursionFlag)

    def get_verb_sense_string(self, verbSense):
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

    def relation_string(self, r):
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

    master_speaker_colors = [
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
    
    def set_attributes(self, spaceRelations):
        keyWord = self.HTMLStyles()
        if (spaceRelations[self.spr].agentLocationRelationSet):
            # is the subject moving within a local space, like a room, or is the
            # subject moving from a space to another, like from the building to the
            # street?
            # G + (self.relations[originalSPRI].beyondLocalSpace) ? "":"S" + SRType + "SR"
            if (spaceRelations[self.spr].beyondLocalSpace):
                keyWord.set_underline(True)
                keyWord.set_font_size(12)
            else:
                keyWord.set_font_size(8)
            if (spaceRelations[self.spr].pastHappening):
                keyWord.set_background((170, 0, 241))
            elif (spaceRelations[self.spr].futureHappening):
                keyWord.set_background((200, 200, 50))
            elif (spaceRelations[self.spr].beforePastHappening):
                keyWord.set_background((104, 0, 225))
            elif (spaceRelations[self.spr].futureInPastHappening):
                keyWord.set_background((200, 100, 50))
            else:
                keyWord.set_background((100, 224, 225))
        else:
            if (spaceRelations[self.spr].genderedLocationRelation):
                keyWord.set_underline(True)
                keyWord.set_background((100, 224, 225))
            else:
                keyWord.set_background((200, 200, 200))
        return keyWord;

    def should_print_relation(self, where, preferences):
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

    def print_relation(self, where, preferences):
        if (self.spr < len(self.relations) and self.relations[self.spr].where == where):
            if where<20: print(str(where) + ": print_relation 2")
            if (self.should_print_relation(where, preferences)):
                if where<20: print(str(where) + ": print_relation 3")
                self.add_element(self.relation_string(self.relations[self.spr].relationType), self.set_attributes(self.relations), where, self.spr, -1,
                        self.SourceMapType.relType);
        while (self.spr < len(self.relations) and self.relations[self.spr].where < where):
            self.spr = self.relations[self.spr].nextSPR;

    def advance_spr(self, where):
        while (self.spr < len(self.relations) and self.relations[self.spr].where < where):
            self.spr = self.relations[self.spr].nextSPR;

    def print_full_relation_string(self, originalSPRI, endSpeakerGroup):
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

    def evaluate_space_relation(self, where, endSpeakerGroup, spr, preferences):
        if (self.relations[spr].agentLocationRelationSet):
            originalSPRI = spr;
            spr = self.relations[spr].nextSPR;
            if (self.should_print_relation(where, originalSPRI, preferences)):
                w = self.print_full_relation_string(originalSPRI, endSpeakerGroup)
                self.add_element(w, self.set_attributes(self.relations, originalSPRI), where, spr, -1, self.SourceMapType.relType2)
                self.add_element("\n", self.set_attributes(self.relations, originalSPRI), where, spr, -1, self.SourceMapType.relType2)
            if (self.relations[originalSPRI].whereControllingEntity != -1 and self.relations[spr].whereControllingEntity == -1
                    and self.relations[originalSPRI].whereSubject == self.relations[spr].whereSubject
                    and self.relations[originalSPRI].whereVerb == self.relations[spr].whereVerb):
                spr += 1
        else:
            spr += 1
        return spr

    def master_speaker_html(self, ms):
        html = {}
        html['style'] = "font-weight: bold; "
        msindex = ms % len(self.master_speaker_colors)
        html['style'] += "color: rgb(" + json.dumps(self.master_speaker_colors[msindex])[1:-1] + "); "
        # html['style'] = "display:block; "
        # html['mouseover'] = str(line)
        masterSpeakerObject = self.masterSpeakerList[ms]  
        s = self.object_num_string(masterSpeakerObject, False, False)
        s += " ds[" + str(self.objects[masterSpeakerObject].numDefinitelyIdentifiedAsSpeaker) + "] s[" \
                + str(self.objects[masterSpeakerObject].numIdentifiedAsSpeaker) + "]";
        html['text'] = s  
        self.positionToAgent[ms] = masterSpeakerObject
        self.agentToPosition[masterSpeakerObject] = ms
        return html

    def print_speaker_group(self, I, preferences):
        while (self.currentSpeakerGroup < len(self.speakerGroups) and I == self.speakerGroups[self.currentSpeakerGroup].begin):
            if (self.speakerGroups[self.currentSpeakerGroup].tlTransition):
                keyWord = self.HTMLStyles()
                keyWord.set_bold(True)
                keyWord.set_foreground((0xFF, 0x00, 0x00))
                self.add_element("------------------TRANSITION------------------", keyWord, I, self.currentSpeakerGroup, -1,
                        self.SourceMapType.transitionSpeakerGroupType)
            self.add_element("\n", None, I, self.currentSpeakerGroup, -1, self.SourceMapType.speakerGroupType)
            for mo in self.speakerGroups[self.currentSpeakerGroup].speakers:
                isPOV = mo in self.speakerGroups[self.currentSpeakerGroup].povSpeakers
                isObserver = mo in self.speakerGroups[self.currentSpeakerGroup].observers
                isGrouped = mo in self.speakerGroups[self.currentSpeakerGroup].groupedSpeakers
                msindex = self.objects[mo].masterSpeakerIndex % 8 #len(self.master_speaker_colors)
                keyWord = self.HTMLStyles()
                keyWord.set_bold(True)
                if (msindex < len(self.master_speaker_colors) and msindex >= 0):
                    keyWord.set_foreground(self.master_speaker_colors[msindex])
                if (isObserver):
                    keyWord.set_background((0, 0, 0))
                elif (isPOV):
                    keyWord.set_background((130, 130, 130))
                oStr = ""
                if (isObserver):
                    oStr += "[obs]" 
                elif (isPOV):
                    oStr += "[pov]"
                if (isGrouped):
                    oStr += "[grouped]"
                oStr += self.object_num_string(mo, False, False)
                self.add_element(oStr, keyWord, I, self.currentSpeakerGroup, mo, self.SourceMapType.speakerGroupType)
                self.add_element("\nbr", None, I, -1, -1, self.SourceMapType.speakerGroupType)
            self.add_element("\n", None, I, self.currentSpeakerGroup, -1, self.SourceMapType.speakerGroupType)
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
                    spri = self.evaluate_space_relation(I, self.speakerGroups[self.currentSpeakerGroup].begin, spri, preferences)
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
                msindex = self.objects[mo].masterSpeakerIndex % 8 # len(self.master_speaker_colors)
                if (msindex < 0):
                    msindex = 0
                keyWord = self.HTMLStyles()
                keyWord.set_foreground(self.master_speaker_colors[msindex])
                keyWord.set_bold(True)
                keyWord.set_font_size(12)
                if (isObserver):
                    keyWord.set_background((0, 0, 0))
                elif (isPOV):
                    keyWord.set_background((192, 192, 192))
                oStr = ""
                if (isObserver):
                    oStr += "[obs]"
                elif (isPOV):
                    oStr += "[pov]"
                if (isGrouped):
                    oStr += "[grouped]"
                oStr += self.object_num_string(mo, False, False)
                self.add_element(oStr, keyWord, I, self.currentSpeakerGroup - 1, self.currentEmbeddedSpeakerGroup,
                        self.SourceMapType.speakerGroupType)
                self.add_element("\nbr", None, I, -1 , -1,
                                self.SourceMapType.speakerGroupType)
            self.add_element("\n", None, I, -1 , -1,
                            self.SourceMapType.speakerGroupType)
            self.currentEmbeddedSpeakerGroup += 1
        if (self.currentEmbeddedSpeakerGroup >= 0 and self.currentSpeakerGroup > 0
                and self.currentEmbeddedSpeakerGroup < len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups)
                and I == self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].end):
            self.add_element("END", None, I, self.currentSpeakerGroup, self.currentEmbeddedSpeakerGroup,
                    self.SourceMapType.speakerGroupType);
            self.currentEmbeddedSpeakerGroup = -1;

    def advance_current_speaker_group(self, I):
        while (self.currentSpeakerGroup < len(self.speakerGroups) and I == self.speakerGroups[self.currentSpeakerGroup].begin):
            self.currentSpeakerGroup += 1
            if (self.currentSpeakerGroup < len(self.speakerGroups)):
                if (len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups) > 0):
                    self.currentEmbeddedSpeakerGroup = 0;
                else:
                    self.currentEmbeddedSpeakerGroup = -1;
        while (self.currentEmbeddedSpeakerGroup >= 0 and self.currentSpeakerGroup > 0
                and self.currentEmbeddedSpeakerGroup < len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups)
                and I == self.speakerGroups[self.currentSpeakerGroup
                        -1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].begin):
            self.currentEmbeddedSpeakerGroup += 1
        if (self.currentEmbeddedSpeakerGroup >= 0 and self.currentSpeakerGroup > 0
                and self.currentEmbeddedSpeakerGroup < len(self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups)
                and I == self.speakerGroups[self.currentSpeakerGroup - 1].embeddedSpeakerGroups[self.currentEmbeddedSpeakerGroup].end):
            self.currentEmbeddedSpeakerGroup = -1;

    def set_time_color_attributes(self, timeColor, keyWord):
        if (timeColor > 0):
            keyWord.set_background((255, 255, 0));
            if ((timeColor & SourceEnums.T_UNIT) > 0):
                keyWord.set_foreground((0, 0, 0));
            elif ((timeColor & SourceEnums.T_RECURRING) == SourceEnums.T_RECURRING):
                keyWord.set_foreground((204, 120, 204));
                keyWord.set_underline(True);
            elif ((timeColor & SourceEnums.T_BEFORE) == SourceEnums.T_BEFORE):
                keyWord.set_underline(True);
                keyWord.set_foreground((60, 255, 60));
            elif ((timeColor & SourceEnums.T_AFTER) == SourceEnums.T_AFTER):
                keyWord.set_underline(True);
                keyWord.set_foreground((0, 205, 0));
            elif ((timeColor & SourceEnums.T_VAGUE) == SourceEnums.T_VAGUE):
                keyWord.set_underline(True);
                keyWord.set_foreground((0, 50, 0));
            elif ((timeColor & SourceEnums.T_AT) == SourceEnums.T_AT):
                keyWord.set_underline(True);
                keyWord.set_foreground((0, 102, 204));
            elif ((timeColor & SourceEnums.T_ON) == SourceEnums.T_ON):
                keyWord.set_underline(True);
                keyWord.set_foreground((102, 102, 204));
            # elif ((timeColor & T_PRESENT) == T_PRESENT):
                # keyWord.set_foreground((102,102,204));
            # elif ((timeColor & T_THROUGHOUT) == T_THROUGHOUT): 
                # keyWord.set_foreground((102,102,204));
            elif ((timeColor & SourceEnums.T_MIDWAY) == SourceEnums.T_MIDWAY):
                keyWord.set_underline(True);
                keyWord.set_foreground((204, 120, 204));
            elif ((timeColor & SourceEnums.T_IN) == SourceEnums.T_IN):
                keyWord.set_underline(True);
                keyWord.set_foreground((60, 255, 60));
            elif ((timeColor & SourceEnums.T_INTERVAL) == SourceEnums.T_INTERVAL):
                keyWord.set_underline(True);
                keyWord.set_foreground((0, 205, 0));
            elif ((timeColor & SourceEnums.T_START) == SourceEnums.T_START):
                keyWord.set_underline(True);
                keyWord.set_foreground((0, 50, 0));
            elif ((timeColor & SourceEnums.T_STOP) == SourceEnums.T_STOP):
                keyWord.set_underline(True);
                keyWord.set_foreground((0, 102, 204));
            elif ((timeColor & SourceEnums.T_RESUME) == SourceEnums.T_RESUME):
                keyWord.set_underline(True);
                keyWord.set_foreground((102, 102, 204));
            elif ((timeColor & SourceEnums.T_RANGE) == SourceEnums.T_RANGE):
                keyWord.set_underline(True);
                keyWord.set_foreground((152, 152, 204));

    def set_verb_sense_attributes(self, verbSense, keyWord):
        if (verbSense != 0):
            if ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PRESENT):
                keyWord.set_background((91, 193, 255));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PRESENT_PERFECT):
                keyWord.set_background((91, 255, 193));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PAST):
                keyWord.set_background((255, 193, 91));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_PAST_PERFECT):
                keyWord.set_background((255, 91, 193));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_FUTURE):
                keyWord.set_background((193, 91, 255));
            elif ((verbSense & SourceEnums.VT_TENSE_MASK) == SourceEnums.VT_FUTURE_PERFECT):
                keyWord.set_background((193, 193, 51));
            if ((verbSense & SourceEnums.VT_EXTENDED) == SourceEnums.VT_EXTENDED):
                keyWord.set_underline(True);
            if ((verbSense & SourceEnums.VT_PASSIVE) == SourceEnums.VT_PASSIVE):
                keyWord.set_foreground((90, 90, 90));
            if ((verbSense & SourceEnums.VT_POSSIBLE) == SourceEnums.VT_POSSIBLE):
                keyWord.set_foreground((160, 160, 160));
            if ((verbSense & (SourceEnums.VT_PASSIVE | SourceEnums.VT_POSSIBLE)) == (SourceEnums.VT_PASSIVE | SourceEnums.VT_POSSIBLE)):
                # u=";border-style: solid; border-width: thin";
                if ((verbSense & SourceEnums.VT_EXTENDED) == SourceEnums.VT_EXTENDED):
                    keyWord.set_foreground((90, 90, 90));

    def get_verb_classes(self, whereVerb):
        baseVerb = self.m[whereVerb].baseVerb;
        # map <wstring, set <int> >::iterator lvtoCi;
        vms = self.vbNetVerbToClassMap.get(baseVerb);
        # get_out is very different from get by itself
        if (whereVerb + 1 < len(self.m) and (self.m[whereVerb + 1].query_winner_form(Form.adverbForm) >= 0  
                or self.m[whereVerb + 1].query_winner_form(Form.prepositionForm) >= 0  
                or self.m[whereVerb + 1].query_winner_form(Form.nounForm) >= 0)):  
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
    def generate_per_element_state(self):
        self.chapters = []
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
        for wm in self.m:
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
                    self.chapters.append({ 'header': sectionHeader, 'where': str(I) + ".0",  'whereEndHeader': si})
            if (wm.object >= 0 and len(self.objects) > wm.object):
                inObject = wm.object;
                if (self.objects[inObject].masterSpeakerIndex < 0 and len(self.objects[inObject].aliases) > 0):
                    for ai in self.objects[inObject].aliases:
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
            states.append([self.spr, inSection, self.section, inObject, principalWhere, inObjectSubType, numCurrentColumn, numCurrentRow, numTotalColumns, self.currentSpeakerGroup])
            self.advance_spr(I);
            self.advance_current_speaker_group(I);
            if not wm.word == "|||":
                if wm.word == "lpendcolumnheaders":
                    numTotalColumns = numCurrentColumn;
                    numCurrentColumn = 0;
                elif (I > 3 and self.m[I - 2].word == "lptable") or wm.word == "lpendcolumn":
                    if wm.word == "lpendcolumn" and (I + 2 < len(self.m) and not self.m[I + 2].word == "lpendcolumnheaders" and not self.m[I + 2].word == "lptable"):
                        numCurrentColumn += 1
                        if (numCurrentColumn > numTotalColumns and numTotalColumns > 0):
                            numCurrentColumn = 0;
                            numCurrentRow += 1
                elif I > 1 and (self.m[I - 1].word == "lpendcolumn"
                        or (I + 1 < len(self.m) and self.m[I + 1].word == "lpendcolumn")):
                    pass
                elif wm.word == "lptable":
                    numCurrentColumn = 0
            if wm.word != "|||" and (self.section < len(self.sections) - 1 and I >= self.sections[self.section].endHeader):
                self.section += 1
        return states

    """
    batchDoc is a map
    key is the source index
    value is the HTML elements tied to that source
    """
    def initialize_source_elements(self):
        self.positionToAgent = {}
        self.agentToPosition = {}
        self.batchDoc = {} 
        self.chapters = []
        self.section = 0;
        self.spr = 0;
        
    def generate_source_element(self, preferences, I, states):
        if I in self.batchDoc:
            return self.batchDoc[I]
        wm = self.m[I]
        self.spr, inSection, self.section, inObject, principalWhere, inObjectSubType, numCurrentColumn, numCurrentRow, numTotalColumns, self.currentSpeakerGroup = states[I]
        keyWord = self.HTMLStyles();
        if inSection:
            keyWord.set_background((122, 224, 255))
            keyWord.set_font_size(22)
            keyWord.set_bold(True)
        if (inObject >= 0):
            keyWord.set_foreground((31, 82, 0))
        if (I == principalWhere or (inObject >= 0 and self.objects[inObject].objectClass == SourceEnums.NAME_OBJECT_CLASS)):
            keyWord.set_foreground((31, 134, 0))
            if ((wm.objectRole & CObject.POV_OBJECT_ROLE) != 0):  
                keyWord.set_background((0, 0, 0))
        if (inObjectSubType >= 0):
            keyWord.set_background((204, 0, 255));
            keyWord.set_foreground((255, 255, 255));
        self.set_time_color_attributes(wm.timeColor, keyWord)
        self.set_verb_sense_attributes(wm.verbSense, keyWord)
        if (wm.verbSense == 0 and inObject >= 0 and self.objects[inObject].masterSpeakerIndex >= 0):
            msindex = self.objects[inObject].masterSpeakerIndex % 8 #len(self.master_speaker_colors);
            keyWord.set_foreground(self.master_speaker_colors[msindex]);
        if wm.word == "|||":
            self.add_element("\n", None, I, -1, -1, self.SourceMapType.wordType);
        self.print_relation(I, preferences);
        self.print_speaker_group(I, preferences);
        if not wm.word == "|||":
            smt = self.SourceMapType.wordType;
            if (inSection):
                smt = self.SourceMapType.headerType;
            if wm.word == "lpendcolumnheaders":
                numTotalColumns = numCurrentColumn;
                numCurrentColumn = 0;
            elif (I > 3 and self.m[I - 2].word == "lptable") or wm.word == "lpendcolumn":
                self.add_element("\n", None, I, self.section, -1, self.SourceMapType.headerType);
                if wm.word == "lpendcolumn" and (I + 2 < len(self.m) and not self.m[I + 2].word == "lpendcolumnheaders" and not self.m[I + 2].word == "lptable"):
                    numCurrentColumn += 1
                    if (numCurrentColumn > numTotalColumns and numTotalColumns > 0):
                        numCurrentColumn = 0;
                        numCurrentRow += 1
                    keyWord.set_foreground((255, 165, 0));  
                    if (numCurrentRow == 0 and numTotalColumns == 0):
                        self.add_element("Column Title." + str(numCurrentColumn) + ":", keyWord, I,
                                self.section, -1, self.SourceMapType.headerType);
                    else:
                        self.add_element(str(numCurrentRow) + "." + str(numCurrentColumn)
                                +":", keyWord, I, self.section, -1, self.SourceMapType.headerType);
            elif I <= 1 or (self.m[I - 1].word != "lpendcolumn"
                    and (I + 1 >= len(self.m) or self.m[I + 1].word != "lpendcolumn")):
                if I > 1 and self.m[I - 1].word == "lptable":
                    keyWord.set_foreground((255, 0, 0))  
                    keyWord.set_font_size(25)
                    keyWord.set_bold(True)
                    keyWord.set_underline(True)
                    self.add_element(str(int(wm.word) + 1), keyWord, I, self.section, -1, smt)
                elif wm.word == "lptable":
                    self.add_element("\n", None, I, self.section, -1, self.SourceMapType.headerType)
                    self.add_element("\n", None, I, self.section, -1, self.SourceMapType.headerType)
                    smt = self.SourceMapType.tableType
                    keyWordTable = self.HTMLStyles()
                    keyWordTable.set_foreground((255, 0, 0))  
                    keyWordTable.set_bold(True)
                    keyWordTable.set_font_size(25)
                    keyWordTable.set_underline(True)
                    keyWord = keyWordTable
                    numCurrentColumn = 0
                    self.add_element("TABLE ", keyWord, I, self.section, -1, smt)
                else:
                    originalWord = self.get_original_word(wm)
                    if I+1<len(self.m) and self.m[I + 1] != '.' and self.m[I + 1] != "|||":
                        originalWord += " "     
                    self.add_element(originalWord, keyWord, I, self.section, -1, smt)
        if wm.word == "“" or wm.word == "‘":
            # if (wm.word.equals("“"))
            # lastOpeningPrimaryQuote = I;
            if ((wm.flags & WordMatch.flagQuotedString) != 0):
                keyWordQS = self.HTMLStyles();
                keyWordQS.set_foreground((0, 255, 0));  
                self.add_element("QS", keyWordQS, I, -1, -1, self.SourceMapType.QSWordType);  # green
            elif (len(wm.objectMatches) != 1 or len(wm.audienceObjectMatches) == 0
                    or (self.currentSpeakerGroup < len(self.speakerGroups)
                            and len(wm.audienceObjectMatches) >= len(self.speakerGroups[self.currentSpeakerGroup].speakers)
                            and len(self.speakerGroups[self.currentSpeakerGroup].speakers) > 1)):
                keyWordRED = self.HTMLStyles();
                keyWordRED.set_foreground((255, 0, 0));  
                self.add_element("Unresolved", keyWordRED, I, -1, -1, self.SourceMapType.URWordType);  # green
            # only mark stories that have a speaker relating a tale which does
            # not involve the person being spoken to
            # so 'we' does not mean the speaker + the audience, if there are any
            # other agents mentioned in the story before
            if ((wm.flags & WordMatch.flagEmbeddedStoryResolveSpeakers) != 0):  
                keyWordRED = self.HTMLStyles()
                keyWordRED.set_foreground((255, 0, 0));  
                ESL = "ES" + ('1' if (wm.flags & WordMatch.flagFirstEmbeddedStory) != 0 else ' ') + ('2' if (wm.flags & WordMatch.flagSecondEmbeddedStory) != 0 else ' ')  
                self.add_element(ESL, keyWordRED, I, -1, -1, self.SourceMapType.ESType);
        if not inSection:
            keyWord.remove_background()
        J = 0  
        for om in wm.objectMatches:
            mObject = om.object;
            objectWhere = self.objects[mObject].originalLocation;
            # skip honorific
            if ((self.m[objectWhere].query_winner_form(Form.honorificForm) >= 0  
                    or self.m[objectWhere].query_winner_form(Form.honorificAbbreviationForm) >= 0)  
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
                msindex = self.objects[mObject].masterSpeakerIndex % len(self.master_speaker_colors)
                keyWord.set_foreground(self.master_speaker_colors[msindex]);
                self.add_element(S, keyWord, I, mObject, J, self.SourceMapType.matchingSpeakerType);
            else:
                self.add_element(S, None, I, mObject, J, self.SourceMapType.matchingObjectType)
                if (J > 2):
                    self.add_element("...", None, I, mObject, J, self.SourceMapType.matchingObjectType)
                    break
            J += 1
        if (len(wm.objectMatches) > 0 and len(wm.audienceObjectMatches) > 0):
            self.add_element(":", None, I, -1, -1, self.SourceMapType.matchingType)
        if (len(wm.objectMatches) > 0 and len(wm.audienceObjectMatches) == 0):
            self.add_element("]", None, I, -1, -1, self.SourceMapType.matchingType)
        if (len(wm.objectMatches) == 0 and len(wm.audienceObjectMatches) > 0):
            self.add_element("[", None, I, -1, -1, self.SourceMapType.matchingType)
        J = 0
        for aom in wm.audienceObjectMatches:
            keyWord.set_foreground((102, 204, 255));
            mObject = aom.object;
            if (mObject == 0):
                w = "Narrator";
            elif (mObject == 1):
                w = "Audience";
            else:
                w = self.m[self.objects[mObject].originalLocation].word;
            self.add_element(w + ("," if (J < len(wm.audienceObjectMatches) - 1) else "]"), None, I,
                    aom.object, J, self.SourceMapType.audienceMatchingType);
            if (J > 2):
                self.add_element("...]", None, I, aom.object, J,
                        self.SourceMapType.audienceMatchingType)
                break
            J += 1
        newHeader = self.section < len(self.sections) and I == self.sections[self.section].endHeader
        if (newHeader):
            self.add_element("\n", None, I, self.section, -1, self.SourceMapType.headerType)
        return self.batchDoc[I]

    def get_style(self, attr):
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
            
    def print_html_per_element(self, e, htmlElementPosition):
        #e(s, attrs, where, index2, index3, sourceMapType)
        html = {}
        html['mouseover'] = str(htmlElementPosition)  
        if e[0] == "\n":
            html['style'] = "display:block; margin-bottom: 10px; "
        elif e[0] == "\nbr":
            html['style'] = "display:block; "
        else:
            html['style'] = self.get_style(e[1])
            html['text'] = e[0]
        return html

    def advance_screen_position(self, htmlElement, maxWidth, currentWidth, currentHeight):        
        length = 0 if 'text' not in htmlElement else len(htmlElement['text']) * self.letterWidth
        if length + currentWidth > maxWidth:
            currentHeight += self.lineHeight
            currentWidth = length
            # if 'text' in htmlElement:
            #    print("\n" + htmlElement['text'], end='')
        else:
            currentWidth += length
            # if 'text' in htmlElement:
            #    print(htmlElement['text'], end='')
        if "display:block" in htmlElement['style']:
            #print("\n")
            #if 'text' in htmlElement:
            #    print(htmlElement['text'], end='')
            currentWidth = 0
            currentHeight += self.lineHeight + 10
        return currentWidth, currentHeight
        
    def print_html(self, preferences, fromSourcePosition, currentWidth, currentHeight, maxWidth, maxHeight):
        where = fromSourcePosition
        htmlElements = []
        numPages = 2 # before they have to scroll!
        maxHeight = maxHeight * numPages 
        while currentHeight < maxHeight and where < len(self.m):
            self.generate_source_element(preferences, where, self.states)
            for esn in range (0, len(self.batchDoc[where])):
                htmlElement = self.print_html_per_element(self.batchDoc[where][esn], str(where) + '.' + str(esn))
                htmlElements.append(htmlElement)
                currentWidth, currentHeight = self.advance_screen_position(htmlElement, maxWidth, currentWidth, currentHeight)
            where += 1
        return htmlElements    
    
    # for determining whether CSS Classes can be enumerated separately
    def generate_css_classes(self):
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
    
    def get_info_panel(self, infoId):
        idSplit = infoId.split('.')
        where = int(idSplit[0])
        htmlElementPosition = int(idSplit[1])
        where2 = self.batchDoc[where][htmlElementPosition][2]
        where3 = self.batchDoc[where][htmlElementPosition][3]
        where4 = self.batchDoc[where][htmlElementPosition][4]
        sourceMapType = self.batchDoc[where][htmlElementPosition][5]
        wordInfo = ""
        toolTip = ""
        roleInfo = ""
        relationsInfo = ""
        if sourceMapType == self.SourceMapType.wordType:
            wordInfo = str(where) + ": " + self.m[where].get_winner_forms() + self.m[where].word
            toolTip = self.m[where].word
            roleInfo = self.m[where].role_string()
            relationsInfo = self.m[where].relations()
            if len(self.m[where].baseVerb) > 0:
                roleInfo = self.vn.get_class_names_2(self.m[where].baseVerb)
                verbSenseStr = self.get_verb_sense_string(self.m[where].quoteForwardLink);
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
            roleInfo = TimeInfo.to_string(self, r)
        if sourceMapType == self.SourceMapType.relType2:
            # w, self.set_attributes(self.relations, originalSPRI), where, spr, -1, self.SourceMapType.relType2
            wordInfo = str(where) + ": space relation #=" + str(where3) 
        if sourceMapType == self.SourceMapType.transitionSpeakerGroupType or \
            sourceMapType == self.SourceMapType.speakerGroupType:
            wordInfo = str(where) + ": speakerGroup #=" + str(where2) 
            if where3>=0:
                wordInfo += " embedded speakerGroup #=" + str(where3)
            if where4>=0:
                wordInfo += " object=" + self.object_num_string(where4, False, False)
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
                wordInfo = str(where) + ": matching object #" + str(where4) + "=" + self.object_num_string(where3, False, False) + " " + str(sourceMapType).split('.')[1]

        if sourceMapType == self.SourceMapType.beType:
            beObject = self.m[where].object;
            if self.m[where].objectMatches.len() == 1:
                beObject = self.m[where].objectMatches[0].object;
            wordInfo = where + ":" + self.object_string(beObject, False, False)
            relationsInfo = "acquired through BE relation:"
            if object >= 0:
                for I in self.objects[object].associatedAdjectives:
                    roleInfo += I;
        if sourceMapType == self.SourceMapType.possessionType:
            posObject = self.m[where].object;
            if self.m[where].objectMatches.len() == 1:
                posObject = self.m[where].objectMatches[0].object;
            wordInfo = where + ":" + self.object_string(posObject, False, False)
            relationsInfo = "HAS-A relation:"
            if posObject >= 0:
                for I in self.objects[posObject].possessions:
                    roleInfo += I;
        return wordInfo, roleInfo, relationsInfo, toolTip 

    def get_word_info(self,fromIndex, toIndex, pageNumber, pageSize):
        fromIndex = int(fromIndex)
        toIndex = int(toIndex)
        wordInfo = []
        for I in range(fromIndex,toIndex + 1):
            word = str(I) + ":" + self.m[I].get_winner_forms() + self.m[I].word
            roleVerbClass = self.m[I].role_string()
            relations = self.m[I].relations()
            objectInfo = self.object_num_string(self.m[I].object, False, False)
            matchingObjects = self.objects_string(self.m[I].objectMatches, False, True)
            # locations are accumulated from processing source - not read from file
            # if self.m[I].object >= 0 and self.objects[self.m[I].object].locations != None:
            #    locations = self.objects[self.m[I].object].locations
            flags = self.m[I].print_flags()
            if len(self.m[I].baseVerb) > 0 and len(roleVerbClass) == 0:
                flags = self.vn.get_class_names(self.m[I].baseVerb)
            wordInfo.append({ 'word': word, 'roleVerbClass':roleVerbClass, 'relations': relations, 'objectInfo':objectInfo, 'matchingObjects':matchingObjects, 'flags':flags})
        return wordInfo

    def get_timeline_segments(self, pageNumber, pageSize):
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
                            s += self.object_num_string(mo, True, False).strip() + ","
                    tsg += 1
            else:
                for esg in self.speakerGroups[tl.speakerGroup].embeddedSpeakerGroups:
                    if tl.begin == esg.begin:
                        for mo in esg.speakers:
                            if not mo in speakers:
                                speakers.add(mo);
                                s += self.object_num_string(mo, True, False).strip() + ","
                        wm = self.m[tl.begin]
                        if (wm.flags & WordMatch.flagEmbeddedStoryResolveSpeakers) != 0:
                            if (wm.flags & WordMatch.flagFirstEmbeddedStory) != 0:
                                for mo in wm.objectMatches:
                                    if not mo.object in speakers:
                                        speakers.add(mo.object);
                                        s += self.object_num_string(mo.object, True, False).strip() + ","
                            if (wm.flags & WordMatch.flagSecondEmbeddedStory) != 0:
                                for mo in wm.audienceObjectMatches:
                                    if not mo.object in speakers:
                                        speakers.add(mo.object);
                                        s += self.object_num_string(mo.object, True, False).strip() + ","
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
    
    def get_relations_in_timeline(self, timelineIndex):
        relations = []
        for tl in self.timelineSegment[timelineIndex].timeTransitions:
            if self.relations[tl].timeTransition:
                color = (0x00, 0x00, 0xFF)
            elif self.relations[tl].nonPresentTimeTransition:
                color = (0x20, 0x20, 0xDF)
            else:
                color = (0x20, 0x20, 0x20)
            s = self.print_full_relation_string(tl, -1) \
                    + TimeInfo.determine_time_progression(self, self.relations[tl])
            relations.append((s,color))
        return relations

    def add_surrounding_words(self, masterIndex):
        stringLengthLimit = 100
        # gather 10 previous words and 10 following words
        fromIndex = max(masterIndex - 10,0)
        toIndex = min(masterIndex + 10,len(self.m))
        resultStr = ""
        midpoint = -1
        beginWord = -1
        endWord = -1
        for I in range(fromIndex,toIndex):
            word = self.get_original_word(self.m[I])
            if I == masterIndex:
                resultStr += word + " "
                midpoint = len(resultStr) - int((len(word) + 1) / 2)
                beginWord =  len(resultStr) - (len(word) + 1)
                endWord = len(resultStr)
            else:
                resultStr += word + " "
        # maximum length of string result is 100 characters.
        if len(resultStr) > stringLengthLimit:
            ssl = int(stringLengthLimit / 2)
            # 3 cases
            # 1. before string is < 50
            # 2. after string is < 50
            # 3. before and after string >=50
            if midpoint > ssl and len(resultStr) - midpoint > ssl:
                # begin and end will be even, word will be approximately in the middle
                # print('1',resultStr,midpoint,ssl,beginWord,endWord)
                diff = midpoint - ssl
                resultStr = resultStr[diff : midpoint + ssl]
                beginWord -= diff
                endWord -= diff
            elif midpoint > ssl:
                # print('2',resultStr,midpoint,ssl,beginWord,endWord)
                diff = len(resultStr) - stringLengthLimit
                resultStr = resultStr[diff:]
                beginWord -= diff
                endWord -= diff
            else:
                # print('3',resultStr,midpoint,ssl,beginWord,endWord)
                resultStr = resultStr[: stringLengthLimit]
        return resultStr[: beginWord], resultStr[beginWord: endWord], resultStr[endWord:]
          
    
    def get_instances_of_word(self, searchString):
        if len(searchString)<3:
            return {}
        print("searching for :" + searchString)
        search_results = {}
        p = re.compile(searchString + '.*', re.IGNORECASE)
        I = 0
        for wm in self.m:
            if p.match(wm.word):
                search_results[I] = self.add_surrounding_words(I)
                if len(search_results) > 30:
                    break
            I += 1
        return search_results
                
    def find_paragraph_start(self, somewhereInParagraph):
        print("searching for beginning of:" + somewhereInParagraph)
        idSplit = somewhereInParagraph.split('.')
        where = int(idSplit[0])
        for I in range(where,0,-1):
            if self.m[I].word == "|||":
                return str(I) + ".0"
        return "0.0"
                
            
    def get_element_type(self, elementId):
        print("getting type for element id:" + elementId)
        idSplit = elementId.split('.')
        where = int(idSplit[0])
        whichHtmlElement = int(idSplit[1])
        print(self.batchDoc[where][whichHtmlElement][5]);
        return self.batchDoc[where][whichHtmlElement][5]
                
    def get_object_name(self, mObject):
        objectWhere = self.objects[mObject].originalLocation;
        # skip honorific
        if ((self.m[objectWhere].query_winner_form(Form.honorificForm) >= 0  
                or self.m[objectWhere].query_winner_form(Form.honorificAbbreviationForm) >= 0)  
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
        return w


    def get_matching_objects(self, elementId, matchingType):
        print("getting matching objects element id:" + elementId + "matchingType = " + matchingType)
        idSplit = elementId.split('.')
        where = int(idSplit[0])
        oms = []
        ret_objects = []
        oms = self.m[where].objectMatches
        for om in oms:
            ret_objects.append({ 'type': self.SourceMapType.matchingObjectType, 'id':om.object, 'name':self.get_object_name(om.object)})
        oms = self.m[where].audienceObjectMatches
        for om in oms:
            ret_objects.append({ 'type': self.SourceMapType.audienceMatchingType, 'id':om.object, 'name':self.get_object_name(om.object)})
        return ret_objects
                
    def save_matching_objects(self, elementId, objects):
        idSplit = elementId.split('.')
        where = int(idSplit[0])
        objects = []
        objectMatches = []
        audientObjectMatches = []
        for o in objects:
            if o.type == self.SourceMapType.matchingObjectType:
                objectMatches.add(o.id);
            else:
                audientObjectMatches.add(o.id);
        print("setting matching objects element id:" + elementId, objects, objectMatches, audientObjectMatches)
        self.m[where].objectMatches = objectMatches
        self.m[where].audientObjectMatches = audientObjectMatches
        return 0
                
    def get_surrounding_objects(self, elementId, matchingType):
        idSplit = elementId.split('.')
        where = int(idSplit[0])
        ret_objects = []
        o = self.m[where].object;
        if o > 0:
            print("getting surrounding objects element id:" + elementId + \
                   " matchingType = " + matchingType + \
                   " in range " + str(max(0,o - 10)) + " to " + str(o))
            for index in range(max(0,o - 10), o):
                ret_objects.append({ 'id':index, 'name':self.get_object_name(index) })
            return ret_objects
        else:
            print("getting surrounding objects element id:" + elementId + \
                   " matchingType = " + matchingType + \
                   " in position range " + str(max(0,where - 500)) + " to " + str(where))
            objects = set()
            for index in range(max(0,where - 500), where):
                if self.m[index].object > 0:
                    objects.add(self.m[index].object)
            for o in objects:
                ret_objects.append({ 'id':o, 'name':self.get_object_name(o) })
            return ret_objects
            
                
       
