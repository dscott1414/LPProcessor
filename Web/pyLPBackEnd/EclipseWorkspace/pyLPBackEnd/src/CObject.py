from CName import CName


class CObject:
	VERB_HISTORY = 4 
	NO_ROLE = 0 
	SUBOBJECT_ROLE = 2 
	SUBJECT_ROLE = 4 
	OBJECT_ROLE = 8 
	META_NAME_EQUIVALENCE = 16 
	MPLURAL_ROLE = 32  # multiple subjects & objects
	HAIL_ROLE = 64  # speakers referred to in a quote from someone else
	IOBJECT_ROLE = 128 
	PREP_OBJECT_ROLE = 256 
	RE_OBJECT_ROLE = 512 
	IS_OBJECT_ROLE = 1024  # used to determine speaker status
	NONPAST_OBJECT_ROLE = 2048  # used to determine speaker status
	ID_SENTENCE_TYPE = 4096  # set at sentence beginning if containing an "IS" verb
	NO_ALT_RES_SPEAKER_ROLE = 8192  # Jill asked. ", Tom said. " said Jill. Ignore these as subjects for alternate resolution
	IS_ADJ_OBJECT_ROLE = 16384 
	NONPRESENT_OBJECT_ROLE = 32768 
	PLACE_OBJECT_ROLE = 65536 
	MOVEMENT_PREP_OBJECT_ROLE = 131072 
	NON_MOVEMENT_PREP_OBJECT_ROLE = 262144 

	SUBJECT_PLEONASTIC_ROLE = NON_MOVEMENT_PREP_OBJECT_ROLE << 1 
	IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE = SUBJECT_PLEONASTIC_ROLE << 1  # mark speakers referring to themselves within quotes
	UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE = IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE << 1 
	SENTENCE_IN_REL_ROLE = UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE << 1 
	PASSIVE_SUBJECT_ROLE = SENTENCE_IN_REL_ROLE << 1 
	POV_OBJECT_ROLE = PASSIVE_SUBJECT_ROLE << 1 
	MNOUN_ROLE = POV_OBJECT_ROLE << 1 
	SECONDARY_SPEAKER_ROLE = MNOUN_ROLE << 1 
	PRIMARY_SPEAKER_ROLE = SECONDARY_SPEAKER_ROLE << 1 
	DELAYED_RECEIVER_ROLE = PRIMARY_SPEAKER_ROLE << 1 
	FOCUS_EVALUATED = DELAYED_RECEIVER_ROLE << 1 
	IN_PRIMARY_QUOTE_ROLE = FOCUS_EVALUATED << 1 
	NOT_OBJECT_ROLE = IN_PRIMARY_QUOTE_ROLE << 1 
	IN_SECONDARY_QUOTE_ROLE = NOT_OBJECT_ROLE << 1 
	IN_EMBEDDED_STORY_OBJECT_ROLE = IN_SECONDARY_QUOTE_ROLE << 1 
	EXTENDED_OBJECT_ROLE = IN_EMBEDDED_STORY_OBJECT_ROLE << 1 
	PP_OBJECT_ROLE = EXTENDED_OBJECT_ROLE << 1 
	IN_QUOTE_REFERRING_AUDIENCE_ROLE = PP_OBJECT_ROLE << 1 
	NO_PP_PREP_ROLE = IN_QUOTE_REFERRING_AUDIENCE_ROLE << 1 
	POSSIBLE_ENCLOSING_ROLE = NO_PP_PREP_ROLE << 1 
	NONPRESENT_ENCLOSING_ROLE = POSSIBLE_ENCLOSING_ROLE << 1 
	NONPAST_ENCLOSING_ROLE = NONPRESENT_ENCLOSING_ROLE << 1 
	EXTENDED_ENCLOSING_ROLE = NONPAST_ENCLOSING_ROLE << 1 
	NOT_ENCLOSING_ROLE = EXTENDED_ENCLOSING_ROLE << 1 
	THINK_ENCLOSING_ROLE = NOT_ENCLOSING_ROLE << 1 
	IN_COMMAND_OBJECT_ROLE = THINK_ENCLOSING_ROLE << 1 
	SENTENCE_IN_ALT_REL_ROLE = IN_COMMAND_OBJECT_ROLE << 1 

	class CLastVerbTenses:
		lastVerb = 0  # book position of main verb
		lastTense = 0  # tense of the entire verb

	# identifySpeakers
	def __init__(self, rs):
		self.index = rs.readInteger()
		self.objectClass = rs.readInteger()
		self.subType = rs.readInteger()
		self.begin = rs.readInteger()
		self.end = rs.readInteger()
		self.originalLocation = rs.readInteger()
		self.PMAElement = rs.readInteger()
		self.numEncounters = rs.readInteger()
		self.numIdentifiedAsSpeaker = rs.readInteger()
		self.numDefinitelyIdentifiedAsSpeaker = rs.readInteger()
		self.numEncountersInSection = rs.readInteger()
		self.numSpokenAboutInSection = rs.readInteger()
		self.numIdentifiedAsSpeakerInSection = rs.readInteger()
		self.numDefinitelyIdentifiedAsSpeakerInSection = rs.readInteger()
		self.PISSubject = rs.readInteger()
		self.PISHail = rs.readInteger()
		self.PISDefinite = rs.readInteger()
		self.replacedBy = rs.readInteger()
		self.ownerWhere = rs.readInteger()
		self.firstLocation = rs.readInteger()
		self.firstSpeakerGroup = rs.readInteger()
		self.firstPhysicalManifestation = rs.readInteger()
		self.lastSpeakerGroup = rs.readInteger()
		self.ageSinceLastSpeakerGroup = rs.readInteger()
		self.masterSpeakerIndex = rs.readInteger()
		self.htmlLinkCount = rs.readInteger()
		self.relativeClausePM = rs.readInteger()
		self.whereRelativeClause = rs.readInteger()
		self.whereRelSubjectClause = rs.readInteger()
		self.usedAsLocation = rs.readInteger()
		self.lastWhereLocation = rs.readInteger()
		count = rs.readInteger()
		self.spaceRelations = {}
		for I in range(count):
			self.spaceRelations[I] = rs.readInteger()
		count = rs.readInteger()
		self.duplicates = {}
		for I in range(count):
			self.duplicates[I] = rs.readInteger()
		count = rs.readInteger()
		self.aliases = {}
		for I in range(count):
			self.aliases[I] = rs.readInteger()
		count = rs.readInteger()
		self.associatedNouns = {}
		for I in range(count):
			self.associatedNouns[I] = rs.readString() 
		count = rs.readInteger()
		self.associatedAdjectives = {}
		for I in range(count):
			self.associatedAdjectives[I] = rs.readString() 
		count = rs.readInteger()
		self.possessions = {}
		for I in range(count):
			self.possessions[I] = rs.readInteger()
		count = rs.readInteger()
		if (count > 0):
			self.genericNounMap = {} 
		for I in range(count):
			s = rs.readString()
			self.genericNounMap[s] = rs.readInteger() 
		self.mostMatchedGeneric = rs.readString() 
		self.genericAge = {}
		for I in range(4):
			self.genericAge[I] = rs.readInteger()
		self.age = rs.readInteger()
		self.mostMatchedAge = rs.readInteger()
		flags = rs.readLong() 
		
		self.isWikiBusiness = (flags & 1) == 1
		flags >>= 1 
		self.isWikiPerson = (flags & 1) == 1  
		flags >>= 1 
		self.isWikiPlace = (flags & 1) == 1  
		flags >>= 1 
		self.isLocationObject = (flags & 1) == 1  
		flags >>= 1 
		self.isTimeObject = (flags & 1) == 1  
		flags >>= 1 
		self.dbPediaAccessed = (flags & 1) == 1  
		flags >>= 1 
		self.container = (flags & 1) == 1  
		flags >>= 1 
		self.wikipediaAccessed = (flags & 1) == 1  
		flags >>= 1 
		self.isKindOf = (flags & 1) == 1  
		flags >>= 1 
		self.genderNarrowed = (flags & 1) == 1  
		flags >>= 1 
		self.isNotAPlace = (flags & 1) == 1  
		flags >>= 1 
		self.partialMatch = (flags & 1) == 1  
		flags >>= 1 
		self.ambiguous = (flags & 1) == 1  
		flags >>= 1 
		self.verySuspect = (flags & 1) == 1  
		flags >>= 1 
		self.suspect = (flags & 1) == 1  
		flags >>= 1 
		self.multiSource = (flags & 1) == 1  
		flags >>= 1 
		self.eliminated = (flags & 1) == 1  
		flags >>= 1 
		self.ownerNeuter = (flags & 1) == 1  
		flags >>= 1 
		self.ownerFemale = (flags & 1) == 1  
		flags >>= 1 
		self.ownerMale = (flags & 1) == 1  
		flags >>= 1 
		self.ownerPlural = (flags & 1) == 1  
		flags >>= 1 
		self.neuter = (flags & 1) == 1  
		flags >>= 1 
		self.female = (flags & 1) == 1  
		flags >>= 1 
		self.male = (flags & 1) == 1  
		flags >>= 1 
		self.plural = (flags & 1) == 1  
		flags >>= 1 
		self.identified = (flags & 1) == 1  
		flags >>= 1 
		
		self.lastVerbTenses = {}
		for I in range(self.VERB_HISTORY):
			self.lastVerbTenses[I] = self.CLastVerbTenses() 
			self.lastVerbTenses[I].lastTense = rs.readInteger()
			self.lastVerbTenses[I].lastVerb = rs.readInteger()
		self.name = CName(rs) 
	
