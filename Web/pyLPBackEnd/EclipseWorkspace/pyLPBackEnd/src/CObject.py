import struct
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
		self.index, self.objectClass, self.subType, self.begin, self.end, self.originalLocation, \
		self.PMAElement, self.numEncounters, self.numIdentifiedAsSpeaker, self.numDefinitelyIdentifiedAsSpeaker, self.numEncountersInSection, \
		self.numSpokenAboutInSection, self.numIdentifiedAsSpeakerInSection, self.numDefinitelyIdentifiedAsSpeakerInSection, self.PISSubject, \
		self.PISHail, self.PISDefinite, self.replacedBy, self.ownerWhere, self.firstLocation, self.firstSpeakerGroup, self.firstPhysicalManifestation, \
		self.lastSpeakerGroup, self.ageSinceLastSpeakerGroup, self.masterSpeakerIndex, self.htmlLinkCount, self.relativeClausePM, self.whereRelativeClause, \
		self.whereRelSubjectClause, self.usedAsLocation, self.lastWhereLocation = struct.unpack('<31i', rs.f.read(124))

		count = rs.read_integer()
		self.spaceRelations = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
		count = rs.read_integer()
		self.duplicates = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
		count = rs.read_integer()
		self.aliases = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
		count = rs.read_integer()
		self.associatedNouns = []
		for I in range(count):
			self.associatedNouns.append(rs.read_string())
		count = rs.read_integer()
		self.associatedAdjectives = []
		for I in range(count):
			self.associatedAdjectives.append(rs.read_string())
		count = rs.read_integer()
		self.possessions = struct.unpack('<' + str(count) + 'i', rs.f.read(count * 4))
		count = rs.read_integer()
		if (count > 0):
			self.genericNounMap = {} 
		for I in range(count):
			s = rs.read_string()
			self.genericNounMap[s] = rs.read_integer() 
		self.mostMatchedGeneric = rs.read_string() 
		self.genericAge = {}
		for I in range(4):
			self.genericAge[I] = rs.read_integer()
		self.age = rs.read_integer()
		self.mostMatchedAge = rs.read_integer()
		flags = rs.read_long() 
		
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
			self.lastVerbTenses[I].lastTense = rs.read_integer()
			self.lastVerbTenses[I].lastVerb = rs.read_integer()
		self.name = CName(rs) 
	
