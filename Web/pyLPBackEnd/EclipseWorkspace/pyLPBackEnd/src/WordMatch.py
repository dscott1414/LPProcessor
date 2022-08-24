from WordClass import WordClass
from Form import Form 
from CObject import CObject
from BitObject import BitObject
from PatternMatch import PatternMatch
from COM import COM 
from SourceEnums import SourceEnums
import struct

class WordMatch:
    flags = 0
    
    flagUsedPossessionRelation = (1 << 53)
    flagUsedBeRelation = (1 << 52)
    flagInLingeringStatement = (1 << 51)
    flagRelativeObject = (1 << 50)
    flagInInfinitivePhrase = (1 << 49)
    flagResolvedByRecent = (1 << 48)
    flagLastContinuousQuote = (1 << 47)
    flagAlternateResolveForwardFromLastSubjectAudience = (1 << 46)
    flagAlternateResolveForwardFromLastSubjectSpeakers = (1 << 45)
    flagGenderIsAmbiguousResolveAudience = (1 << 44) # when a
    # definitely specified audience is a gendered pronoun with more than one
    # speaker in the current speaker group having that gender
    flagRelativeHead = (1 << 43)
    flagQuoteContainsSpeaker = (1 << 42)
    flagResolveMetaGroupByGender = (1 << 41)
    flagEmbeddedStoryResolveSpeakersGap = (1 << 40)
    flagEmbeddedStoryBeginResolveSpeakers = (1 << 39) # only set if
    # aged
    flagAge = (1 << 38) # only set if aged
    # set dynamically?
    # flagWNNotPhysicalObject=(1<<37)
    flagWNPhysicalObject = (1 << 36)
    flagInPStatement = (1 << 35)
    flagInQuestion = (1 << 34)
    flagRelationsAlreadyEvaluated = (1 << 32)
    flagNounOwner = (1 << 31)
    # form flags
    flagAddProperNoun = (1 << 30)
    flagOnlyConsiderProperNounForms = (1 << 29)
    flagAllCaps = (1 << 28)
    flagTopLevelPattern = (1 << 27)
    flagPossiblePluralNounOwner = (1 << 26)
    flagNotMatched = (1 << 25)
    flagInsertedQuote = (1 << 24)
    flagFirstLetterCapitalized = (1 << 23)
    flagRefuseProperNoun = (1 << 22)
    flagMetaData = (1 << 21)
    flagOnlyConsiderOtherNounForms = (1 << 20)
    # BNC patternPreferences (only used for nonQuotes)
    flagBNCPreferNounPatternMatch = (1 << 19)
    flagBNCPreferVerbPatternMatch = (1 << 18)
    flagBNCPreferAdjectivePatternMatch = (1 << 17)
    flagBNCPreferAdverbPatternMatch = (1 << 16)
    flagBNCPreferIgnore = (1 << 15)
    flagBNCFormNotCertain = (1 << 14) # tag was not set on word
    # (because a tag was set for
    # multiple words)

    # speaker resolution (only used for quotes)
    flagFromPreviousHailResolveSpeakers = (1 << 23)
    flagAlphaBeforeHint = (1 << 22)
    flagAlphaAfterHint = (1 << 21)
    flagAlternateResolutionFinishedSpeakers = (1 << 20)
    flagForwardLinkResolveAudience = (1 << 19)
    flagForwardLinkResolveSpeakers = (1 << 18)
    flagFromLastDefiniteResolveAudience = (1 << 17)
    flagDefiniteResolveSpeakers = (1 << 16)
    flagMostLikelyResolveAudience = (1 << 15)
    flagMostLikelyResolveSpeakers = (1 << 14)
    flagSpecifiedResolveAudience = (1 << 13)
    flagAudienceFromSpeakerGroupResolveAudience = (1 << 12)
    flagHailedResolveAudience = (1 << 11)
    # if one speaker is known (flagDefiniteResolveSpeakers) then resolve others
    # around it by assuming the speakers alternate
    flagAlternateResolveBackwardFromDefiniteAudience = (1 << 10) # resolves
    # unresolved speakers backwards from a known definite speaker
    flagAlternateResolveBackwardFromDefiniteSpeakers = (1 << 9) # resolves
    # unresolved speakers backwards from a    known definite speaker
    flagForwardLinkAlternateResolveAudience = (1 << 8)
    flagForwardLinkAlternateResolveSpeakers = (1 << 7)
    flagEmbeddedStoryResolveSpeakers = (1 << 6)
    flagAlternateResolveForwardAfterDefinite = (1 << 5) # this flag
    # is set for all alternative settings forward after a definite
    # speaker previously results from imposeMostLikely were only set with
    # flagMostLikelyResolveSpeakers flagAlternateResolveForwardAfterDefinite allows an alternate speaker
    # imposed from below and an alternate speaker
    # imposed from above to stop running over each other
    # 1<<4 (flagObjectResolved) is zeroed for all positions after
    # identifySpeakerGroups
    flagGenderIsAmbiguousResolveSpeakers = (1 << 3) # when a
    # definitely specified speaker is a gendered pronoun with more than one
    # speaker in the current speaker group having that gender
    flagQuotedString = (1 << 2)
    flagSecondEmbeddedStory = (1 << 1)
    flagFirstEmbeddedStory = (1 << 0)
    # only for non-quotes
    flagObjectResolved = (1 << 4)
    flagAdjectivalObject = (1 << 3)
    flagUnresolvableObjectResolvedThroughSpeakerGroup = (1 << 2)
    flagObjectPleonastic = (1 << 1)
    flagIgnoreAsSpeaker = (1 << 0)

    def __init__(self, rs):
        self.word = rs.read_string()
        self.baseVerb = rs.read_string()
        self.flags, self.maxMatch, self.minAvgCostAfterAssessCost, self.lowestAverageCost, self.maxLACMatch, \
        self.maxLACAACMatch, self.objectRole, self.verbSense, self.timeColor, self.beginPEMAPosition, \
        self.endPEMAPosition, self.PEMACount, count = struct.unpack('<qhiihhqibiiii', rs.f.read(51))
        self.pma = []
        for _ in range(count):
            self.pma.append(PatternMatch(rs))
        self.forms = BitObject(rs,16)
        self.winnerForms = BitObject(rs,16)
        self.patterns = BitObject(rs,32)
        self.object = rs.read_integer()
        self.principalWherePosition = rs.read_integer()
        self.principalWhereAdjectivalPosition = rs.read_integer()
        self.originalObject = rs.read_integer()
        count = rs.read_integer()
        self.objectMatches = []
        for _ in range(count):
            self.objectMatches.append(COM(rs))
        count = rs.read_integer()
        self.audienceObjectMatches = []
        for _ in range(count):
            self.audienceObjectMatches.append(COM(rs))
        self.quoteForwardLink, self.endQuote, self.nextQuote, self.previousQuote, self.relObject, \
        self.relSubject, self.relVerb, self.relPrep, self.beginObjectPosition, self.endObjectPosition, \
        self.tmpWinnerForms, self.embeddedStorySpeakerPosition, traceFlags = struct.unpack('<12iq', rs.f.read(56))
        self.collectPerSentenceStats = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceTagSetCollection = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceIncludesPEMAIndex = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceUnmatchedSentences = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceMatchedSentences = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceSecondaryPEMACosting = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.tracePatternElimination = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceBNCPreferences = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceDeterminer = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceVerbObjects = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceObjectResolution = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceNameResolution = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceSpeakerResolution = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceRelations = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceAnaphors = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceEVALObjects = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceAgreement = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.traceRole = (traceFlags & 1) == 1
        traceFlags >>= 1
        self.printBeforeElimination = (traceFlags & 1) == 1
        self.logCache, self.relNextObject, self.nextCompoundPartObject, self.previousCompoundPartObject, \
        self.relInternalVerb, self.relInternalObject, self.quoteForwardLink, self.quoteBackLink, self.speakerPosition, \
        self.audiencePosition = struct.unpack('<h9i', rs.f.read(38))
        self.skipResponse = -1
        self.spaceRelation = False
        self.andChainType = False
        self.notFreePrep = False
        self.hasVerbRelations = False

    def get_word_tfi(self):
        return WordClass.words.get(self.word)  

    def query_word_form(self,form):
        tfi=self.get_word_tfi()
        if (tfi==None): return -1
        for f in range(len(tfi.forms)):
            if (tfi.forms[f] == form):
                return f
        return -1
    
    def query_form(self, form):
        if (Form.forms==None):  
            return -1
        if ((self.flags & self.flagAddProperNoun) != 0 and form == SourceEnums.PROPER_NOUN_FORM_NUM):  
            tfi=self.get_word_tfi()
            if (tfi==None): return -1
            return len(tfi.forms)
        if ((self.flags & self.flagOnlyConsiderProperNounForms) != 0):
            if (form >= len(Form.forms)):  
                if (form != SourceEnums.PROPER_NOUN_FORM_NUM and not Form.forms[form].properNounSubClass):  
                    return -1
        elif (form == SourceEnums.PROPER_NOUN_FORM_NUM and ((self.flags & self.flagFirstLetterCapitalized) == 0 or (self.flags & self.flagRefuseProperNoun) != 0)):  
            return -1
        return self.query_word_form(form)

    def is_winner(self, form):
        return ((1 << form) & self.tmpWinnerForms) != 0 if (self.tmpWinnerForms > 0) else True

    def has_winner_verb_form(self):
        return self.get_word_tfi().has_winner_verb_form(self.tmpWinnerForms)

    def query_winner_form(self, form):
        if (self.get_word_tfi()==None or self.get_word_tfi().forms==None):
            return -1
        if ((self.flags & self.flagAddProperNoun) != 0 and form == SourceEnums.PROPER_NOUN_FORM_NUM):  
            picked = len(self.get_word_tfi().forms)
        elif ((self.flags & self.flagOnlyConsiderProperNounForms) != 0):
            if (form != SourceEnums.PROPER_NOUN_FORM_NUM and not Form.forms[form].properNounSubClass):  
                return -1
            picked = self.query_word_form(form)
        else:
            picked = self.query_word_form(form)
        if (picked >= 0 and self.is_winner(picked)):
            return picked
        return -1

    roles = [ CObject.SUBOBJECT_ROLE, CObject.SUBJECT_ROLE, CObject.OBJECT_ROLE, CObject.META_NAME_EQUIVALENCE, CObject.MPLURAL_ROLE,  
            CObject.HAIL_ROLE, CObject.IOBJECT_ROLE, CObject.PREP_OBJECT_ROLE, CObject.RE_OBJECT_ROLE, CObject.IS_OBJECT_ROLE,  
            CObject.NOT_OBJECT_ROLE, CObject.NONPAST_OBJECT_ROLE, CObject.ID_SENTENCE_TYPE, CObject.NO_ALT_RES_SPEAKER_ROLE,  
            CObject.IS_ADJ_OBJECT_ROLE, CObject.NONPRESENT_OBJECT_ROLE, CObject.PLACE_OBJECT_ROLE, CObject.MOVEMENT_PREP_OBJECT_ROLE,  
            CObject.NON_MOVEMENT_PREP_OBJECT_ROLE, CObject.SUBJECT_PLEONASTIC_ROLE, CObject.IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE,  
            CObject.UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE, CObject.SENTENCE_IN_REL_ROLE, CObject.PASSIVE_SUBJECT_ROLE, CObject.POV_OBJECT_ROLE,  
            CObject.MNOUN_ROLE, CObject.PRIMARY_SPEAKER_ROLE, CObject.SECONDARY_SPEAKER_ROLE, CObject.FOCUS_EVALUATED, CObject.ID_SENTENCE_TYPE,  
            CObject.DELAYED_RECEIVER_ROLE, CObject.IN_PRIMARY_QUOTE_ROLE, CObject.IN_SECONDARY_QUOTE_ROLE, CObject.IN_EMBEDDED_STORY_OBJECT_ROLE, 
            CObject.EXTENDED_OBJECT_ROLE, CObject.NOT_ENCLOSING_ROLE, CObject.EXTENDED_ENCLOSING_ROLE, CObject.NONPAST_ENCLOSING_ROLE,  
            CObject.NONPRESENT_ENCLOSING_ROLE, CObject.POSSIBLE_ENCLOSING_ROLE, CObject.THINK_ENCLOSING_ROLE ]  
    r_c = [ "SUBOBJ", "SUBJ", "OBJ", "META_EQUIV", "MP", "H", "IOBJECT", "PREP", "RE", "IS", "NOT", "NONPAST", "ID",
            "NO_ALT_RES_SPEAKER", "IS_ADJ", "NONPRESENT", "PL", "MOVE", "NON_MOVE", "PLEO", "INQ_SELF_REF_SPEAKER", "UNRES_FROM_IMPLICIT",
            "S_IN_REL", "PASS_SUBJ", "POV", "MNOUN", "SP", "SECONDARY_SP", "EVAL", "ID", "DELAY", "PRIM", "SECOND", "EMBED", "EXT", "NOT_ENC",
            "EXT_ENC", "NPAST_ENC", "NPRES_ENC", "POSS_ENC", "THINK_ENC" ]

    def role_string(self):
        role = ""
        I = 0
        for r in self.r_c:
            if ((self.objectRole & self.roles[I]) != 0):
                role += r + ", "
            I += 1
        if len(role) > 1:
            role=role[:-2]
        return role

    def relations(self):
        retMessage = ""
        if (self.relSubject!=-1): retMessage+="rSubj=" + str(self.relSubject)
        if (self.relVerb!=-1): retMessage+=" rVerb=" + str(self.relVerb)
        if (self.relObject!=-1): retMessage+=" rObj=" + str(self.relObject)
        if (self.relPrep!=-1): retMessage+=" rPrep=" + str(self.relPrep)
        if (self.relInternalVerb!=-1): retMessage+=" rInternalVerb=" + str(self.relInternalVerb)
        if (self.relNextObject!=-1): retMessage+=" rNextObject=" + str(self.relNextObject)
        if (self.nextCompoundPartObject!=-1): retMessage+=" nextCompoundPartObject=" + str(self.nextCompoundPartObject)
        if (self.previousCompoundPartObject!=-1): retMessage+=" previousCompoundPartObject=" + str(self.previousCompoundPartObject)
        if (self.relInternalObject!=-1): retMessage+=" rInternalObject=" + str(self.relInternalObject)
        if (self.nextQuote!=-1): retMessage+=" nextQuote=" + str(self.nextQuote)
        if (self.previousQuote!=-1): retMessage+=" previousQuote=" + str(self.previousQuote)
        if (len(retMessage) > 0 and retMessage[0] == ' '):
            retMessage = retMessage[1:]
        return retMessage

    def get_winner_forms(self):
        sForms="("
        for I in range(len(Form.forms)):  
            if (self.forms.is_set(I)):
                if (self.query_winner_form(I)>=0): sForms+="*"
                sForms+=Form.forms[I].shortName+" "
        return sForms.strip()+") "
    
    def print_flags(self):
        flagStr=""
        if ((self.flags&self.flagInQuestion)!=0): flagStr+=" Q"
        if ((self.flags&self.flagFirstLetterCapitalized)!=0): flagStr+=" C"
        if ((self.flags&self.flagUsedPossessionRelation)!=0): flagStr+=" UsedPossessionRelation" 
        if ((self.flags&self.flagUsedBeRelation)!=0): flagStr+=" UsedBeRelation"
        if ((self.flags&self.flagInLingeringStatement)!=0): flagStr+=" InLingeringStatement"
        if ((self.flags&self.flagRelativeObject)!=0): flagStr+=" RelativeObject"
        if ((self.flags&self.flagInInfinitivePhrase)!=0): flagStr+=" InInfinitivePhrase"
        if ((self.flags&self.flagResolvedByRecent)!=0): flagStr+=" ResolvedByRecent"
        if ((self.flags&self.flagLastContinuousQuote)!=0): flagStr+=" LastContinuousQuote"
        if ((self.flags&self.flagAlternateResolveForwardFromLastSubjectAudience)!=0): flagStr+=" AlternateResolveForwardFromLastSubjectAudience"
        if ((self.flags&self.flagAlternateResolveForwardFromLastSubjectSpeakers)!=0): flagStr+=" AlternateResolveForwardFromLastSubjectSpeakers"
        if ((self.flags&self.flagGenderIsAmbiguousResolveAudience)!=0): flagStr+=" GenderIsAmbiguousResolveAudience" # when a
        # definitely specified audience is a gendered pronoun with more than one
        # speaker in the current speaker group having that gender
        if ((self.flags&self.flagRelativeHead)!=0): flagStr+=" RelativeHead"
        if ((self.flags&self.flagQuoteContainsSpeaker)!=0): flagStr+=" QuoteContainsSpeaker"
        if ((self.flags&self.flagResolveMetaGroupByGender)!=0): flagStr+=" ResolveMetaGroupByGender"
        if ((self.flags&self.flagEmbeddedStoryResolveSpeakersGap)!=0): flagStr+=" EmbeddedStoryResolveSpeakersGap"
        if ((self.flags&self.flagEmbeddedStoryBeginResolveSpeakers)!=0): flagStr+=" EmbeddedStoryBeginResolveSpeakers" # only set if
        # aged
        if ((self.flags&self.flagAge)!=0): flagStr+="" # only set if aged
        # set dynamically?
        # if ((flags&flagWNNotPhysicalObject=(1<<37"
        if ((self.flags&self.flagWNPhysicalObject)!=0): flagStr+=" WNPhysical"
        if ((self.flags&self.flagInPStatement)!=0): flagStr+=" InPStatement"
        if ((self.flags & self.flagRelationsAlreadyEvaluated)!=0): flagStr+=" RelationsAlreadyEvaluated"
        if ((self.flags & self.flagNounOwner)!=0): flagStr+=" NounOwner"
        # form flags
        if ((self.flags & self.flagAddProperNoun)!=0): flagStr+=" AddPN"
        if ((self.flags & self.flagOnlyConsiderProperNounForms)!=0): flagStr+=" OnlyPN"
        if ((self.flags & self.flagAllCaps)!=0): flagStr+=" AllCaps"
        if ((self.flags & self.flagTopLevelPattern)!=0): flagStr+=""
        if ((self.flags & self.flagPossiblePluralNounOwner)!=0): flagStr+=" PossiblePluralNounOwner"
        if ((self.flags & self.flagNotMatched)!=0): flagStr+=" NotMatched"
        if ((self.flags & self.flagInsertedQuote)!=0): flagStr+=" InsertedQuote"
        if ((self.flags & self.flagRefuseProperNoun)!=0): flagStr+=" RefuseProperNoun"
        if ((self.flags & self.flagMetaData)!=0): flagStr+=" MetaData"
        if ((self.flags & self.flagOnlyConsiderOtherNounForms)!=0): flagStr+=" OnlyConsiderOtherNounForms"
        # BNC patternPreferences (only used for nonQuotes)
        if ((self.flags & self.flagBNCPreferNounPatternMatch)!=0): flagStr+=" BNCPreferNounPatternMatch"
        if ((self.flags & self.flagBNCPreferVerbPatternMatch)!=0): flagStr+=" BNCPreferVerbPatternMatch"
        if ((self.flags & self.flagBNCPreferAdjectivePatternMatch)!=0): flagStr+=" BNCPreferAdjectivePatternMatch"
        if ((self.flags & self.flagBNCPreferAdverbPatternMatch)!=0): flagStr+=" BNCPreferAdverbPatternMatch"
        if ((self.flags & self.flagBNCPreferIgnore)!=0): flagStr+=" BNCPreferIgnore"
        if ((self.flags & self.flagBNCFormNotCertain)!=0): flagStr+=" BNCFormNotCertain" # tag was not set on word
        # (because a tag was set for
        # multiple words)

        # speaker resolution (only used for quotes)
        if (self.word=="\""):
            if ((self.flags & self.flagFromPreviousHailResolveSpeakers)!=0): flagStr+=" FromPreviousHailResolveSpeakers"
            if ((self.flags & self.flagAlphaBeforeHint)!=0): flagStr+=" AlphaBeforeHint"
            if ((self.flags & self.flagAlphaAfterHint)!=0): flagStr+=" AlphaAfterHint"
            if ((self.flags & self.flagAlternateResolutionFinishedSpeakers)!=0): flagStr+=" AlternateResolutionFinishedSpeakers"
            if ((self.flags & self.flagForwardLinkResolveAudience)!=0): flagStr+=" ForwardLinkResolveAudience"
            if ((self.flags & self.flagForwardLinkResolveSpeakers)!=0): flagStr+=" ForwardLinkResolveSpeakers"
            if ((self.flags & self.flagFromLastDefiniteResolveAudience)!=0): flagStr+=" FromLastDefiniteResolveAudience"
            if ((self.flags & self.flagDefiniteResolveSpeakers)!=0): flagStr+=" DefiniteResolveSpeakers"
            if ((self.flags & self.flagMostLikelyResolveAudience)!=0): flagStr+=" MostLikelyResolveAudience"
            if ((self.flags & self.flagMostLikelyResolveSpeakers)!=0): flagStr+=" MostLikelyResolveSpeakers"
            if ((self.flags & self.flagSpecifiedResolveAudience)!=0): flagStr+=" SpecifiedResolveAudience"
            if ((self.flags & self.flagAudienceFromSpeakerGroupResolveAudience)!=0): flagStr+=" AudienceFromSpeakerGroupResolveAudience"
            if ((self.flags & self.flagHailedResolveAudience)!=0): flagStr+=" HailedResolveAudience"
            # if one speaker is known (flagDefiniteResolveSpeakers) then resolve others
            # around it by assuming the speakers alternate
            if ((self.flags & self.flagAlternateResolveBackwardFromDefiniteAudience)!=0): flagStr+=" AlternateResolveBackwardFromDefiniteAudience" # resolves
            # unresolved speakers backwards from a known definite speaker
            if ((self.flags & self.flagAlternateResolveBackwardFromDefiniteSpeakers)!=0): flagStr+=" AlternateResolveBackwardFromDefiniteSpeakers" # resolves
            # unresolved speakers backwards from a    known definite speaker
            if ((self.flags & self.flagForwardLinkAlternateResolveAudience)!=0): flagStr+=" ForwardLinkAlternateResolveAudience"
            if ((self.flags & self.flagForwardLinkAlternateResolveSpeakers)!=0): flagStr+=" ForwardLinkAlternateResolveSpeakers"
            if ((self.flags & self.flagEmbeddedStoryResolveSpeakers)!=0): flagStr+=" EmbeddedStoryResolveSpeakers"
            if ((self.flags & self.flagAlternateResolveForwardAfterDefinite)!=0): flagStr+=" AlternateResolveForwardAfterDefinite" # this flag
            # is set for all alternative settings forward after a definite
            # speaker previously results from imposeMostLikely were only set with
            # flagMostLikelyResolveSpeakers flagAlternateResolveForwardAfterDefinite allows an alternate speaker
            # imposed from below and an alternate speaker
            # imposed from above to stop running over each other
            # 1<<4 (flagObjectResolved) is zeroed for all positions after
            # identifySpeakerGroups
            if ((self.flags & self.flagGenderIsAmbiguousResolveSpeakers)!=0): flagStr+=" GenderIsAmbiguousResolveSpeakers" # when a
            # definitely specified speaker is a gendered pronoun with more than one
            # speaker in the current speaker group having that gender
            if ((self.flags & self.flagQuotedString)!=0): flagStr+=" QuotedString"
            if ((self.flags & self.flagSecondEmbeddedStory)!=0): flagStr+=" SecondEmbeddedStory"
            if ((self.flags & self.flagFirstEmbeddedStory)!=0): flagStr+=" FirstEmbeddedStory"
        # only for non-quotes
        if ((self.flags & self.flagObjectResolved)!=0): flagStr+=""
        if ((self.flags & self.flagAdjectivalObject)!=0): flagStr+=" AdjectivalObject"
        if ((self.flags & self.flagUnresolvableObjectResolvedThroughSpeakerGroup)!=0): flagStr+=" UnresolvableObjectResolvedThroughSpeakerGroup"
        if ((self.flags & self.flagObjectPleonastic)!=0): flagStr+=" Pleonastic"
        if ((self.flags & self.flagIgnoreAsSpeaker)!=0): flagStr+=" IgnoreAsSpeaker"

        return flagStr

