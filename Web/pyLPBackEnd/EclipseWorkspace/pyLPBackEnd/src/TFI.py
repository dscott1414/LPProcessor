from Form import Form
import struct

class TFI:
    MAX_USAGE_PATTERNS=16

    def has_winner_verb_form(self, winnerForms):
        for I in range(self.count):
            if (Form.forms[self.forms[I]].verbForm and (winnerForms==0 or ((1<<I)&winnerForms)!=0)):
                return True
            return False


    def __init__(self,rs):
        self.count = rs.read_integer()
        self.forms = struct.unpack('<' + str(self.count) + 'i', rs.f.read(self.count * 4))
        rs.offset += self.count * 4
        self.inflectionFlags, self.flags, self.timeFlags, self.derivationRules, self.index = struct.unpack('<5i', rs.f.read(20))
        rs.offset += 20
        self.mainEntry = rs.read_string()
        self.usagePatterns = struct.unpack('<' + str(self.MAX_USAGE_PATTERNS) + 'b', rs.f.read(self.MAX_USAGE_PATTERNS))
        self.usageCosts = struct.unpack('<' + str(self.MAX_USAGE_PATTERNS) + 'b', rs.f.read(self.MAX_USAGE_PATTERNS))
        rs.offset += 2*self.MAX_USAGE_PATTERNS

    topLevelSeparator=1 
    ignoreFlag=2
    queryOnLowerCase=4 
    queryOnAnyAppearance=8 
    updateMainInfo=32
    updateMainEntry=64
    insertNewForms=128 
    isMainEntry=256
    intersectionGroup=512 
    wordRelationsRefreshed=1024 
    newWordFlag=2048
    inSourceFlag=4096 
    alreadyTaken=8192*256 
    physicalObjectByWN=8192*512 
    notPhysicalObjectByWN=8192*1024
    uncertainPhysicalObjectByWN=notPhysicalObjectByWN<<1
    genericGenderIgnoreMatch=uncertainPhysicalObjectByWN<<1
    prepMoveType=genericGenderIgnoreMatch<<1
    genericAgeGender=prepMoveType<<1
    stateVerb=genericAgeGender<<1
    possibleStateVerb=stateVerb<<1
    lastWordFlag=possibleStateVerb<<1

    _MIL=1024*1024
    
    SINGULAR=1
    PLURAL=2
    SINGULAR_OWNER=4
    PLURAL_OWNER=8
    VERB_PAST=16
    VERB_PAST_PARTICIPLE=32
    VERB_PRESENT_PARTICIPLE=64
    VERB_PRESENT_THIRD_SINGULAR=128
    VERB_PRESENT_FIRST_SINGULAR=256
    # These 5 verb forms must remain in sequence
    VERB_PAST_THIRD_SINGULAR=512
    VERB_PAST_PLURAL=1024
    VERB_PRESENT_PLURAL=2048
    VERB_PRESENT_SECOND_SINGULAR=4096
    ADJECTIVE_NORMATIVE=8192
    ADJECTIVE_COMPARATIVE=16384
    ADJECTIVE_SUPERLATIVE=32768
    ADVERB_NORMATIVE=65536
    ADVERB_COMPARATIVE=131072
    ADVERB_SUPERLATIVE=262144
    MALE_GENDER=_MIL*1
    FEMALE_GENDER=_MIL*2
    NEUTER_GENDER=_MIL*4
    FIRST_PERSON=_MIL*8 #/*ME*/
    SECOND_PERSON=_MIL*16 #/*YOU*/
    THIRD_PERSON=_MIL*32 #/*THEM*/
    NO_OWNER=_MIL*64
    VERB_NO_PAST=_MIL*64 # special case to match only nouns and Proper Nouns that do not have 's / and verbs that should only be past participles
    
    OPEN_INFLECTION=_MIL*128
    CLOSE_INFLECTION=_MIL*256
    # overlap
    MALE_GENDER_ONLY_CAPITALIZED=_MIL*512
    FEMALE_GENDER_ONLY_CAPITALIZED=_MIL*1024
    ONLY_CAPITALIZED=(MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED)