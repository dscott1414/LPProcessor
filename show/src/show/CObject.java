package show;
import java.util.HashMap;
import java.util.Map;

	public class CObject{
	  public static final int VERB_HISTORY = 4;

		public static final long NO_ROLE=0;
		public static final long SUBOBJECT_ROLE=2;
		public static final long SUBJECT_ROLE=4;
		public static final long OBJECT_ROLE=8;
		public static final long META_NAME_EQUIVALENCE=16;
		public static final long MPLURAL_ROLE=32; // multiple subjects & objects
		public static final long HAIL_ROLE=64; // speakers referred to in a quote from someone else
		public static final long IOBJECT_ROLE=128;
		public static final long PREP_OBJECT_ROLE=256;
		public static final long RE_OBJECT_ROLE=512;
		public static final long IS_OBJECT_ROLE=1024; // used to determine speaker status
		public static final long NONPAST_OBJECT_ROLE=2048; // used to determine speaker status
		public static final long ID_SENTENCE_TYPE=4096; // set at sentence beginning if containing an "IS" verb
		public static final long NO_ALT_RES_SPEAKER_ROLE=8192; // Jill asked.  ", Tom said.  " said Jill.  Ignore these as subjects for alternate resolution
		public static final long IS_ADJ_OBJECT_ROLE=16384;
		public static final long NONPRESENT_OBJECT_ROLE=32768;
		public static final long PLACE_OBJECT_ROLE=65536;
		public static final long MOVEMENT_PREP_OBJECT_ROLE=131072;
		public static final long NON_MOVEMENT_PREP_OBJECT_ROLE=262144;
		
		public static final long SUBJECT_PLEONASTIC_ROLE=NON_MOVEMENT_PREP_OBJECT_ROLE<<1;
		public static final long IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE=SUBJECT_PLEONASTIC_ROLE<<1; // mark speakers referring to themselves within quotes
		public static final long UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE=IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE<<1;
		public static final long SENTENCE_IN_REL_ROLE=UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE<<1;
		public static final long PASSIVE_SUBJECT_ROLE=SENTENCE_IN_REL_ROLE<<1;
		public static final long POV_OBJECT_ROLE=PASSIVE_SUBJECT_ROLE<<1;
		public static final long MNOUN_ROLE=POV_OBJECT_ROLE<<1;
		public static final long SECONDARY_SPEAKER_ROLE=MNOUN_ROLE<<1;
		public static final long PRIMARY_SPEAKER_ROLE=SECONDARY_SPEAKER_ROLE<<1;
		public static final long DELAYED_RECEIVER_ROLE=PRIMARY_SPEAKER_ROLE<<1;
		public static final long FOCUS_EVALUATED=DELAYED_RECEIVER_ROLE<<1;
		public static final long IN_PRIMARY_QUOTE_ROLE=FOCUS_EVALUATED<<1;
		public static final long NOT_OBJECT_ROLE=IN_PRIMARY_QUOTE_ROLE<<1;
		public static final long IN_SECONDARY_QUOTE_ROLE=NOT_OBJECT_ROLE<<1;
		public static final long IN_EMBEDDED_STORY_OBJECT_ROLE=IN_SECONDARY_QUOTE_ROLE<<1;
		public static final long EXTENDED_OBJECT_ROLE=IN_EMBEDDED_STORY_OBJECT_ROLE<<1;
		public static final long PP_OBJECT_ROLE=EXTENDED_OBJECT_ROLE<<1;
		public static final long IN_QUOTE_REFERRING_AUDIENCE_ROLE=PP_OBJECT_ROLE<<1;
		public static final long NO_PP_PREP_ROLE=IN_QUOTE_REFERRING_AUDIENCE_ROLE<<1;
		public static final long POSSIBLE_ENCLOSING_ROLE=NO_PP_PREP_ROLE<<1;
		public static final long NONPRESENT_ENCLOSING_ROLE=POSSIBLE_ENCLOSING_ROLE<<1;
		public static final long NONPAST_ENCLOSING_ROLE=NONPRESENT_ENCLOSING_ROLE<<1;
		public static final long EXTENDED_ENCLOSING_ROLE=NONPAST_ENCLOSING_ROLE<<1;
		public static final long NOT_ENCLOSING_ROLE=EXTENDED_ENCLOSING_ROLE<<1;
		public static final long THINK_ENCLOSING_ROLE=NOT_ENCLOSING_ROLE<<1;
		public static final long IN_COMMAND_OBJECT_ROLE=THINK_ENCLOSING_ROLE<<1;
	  public static final long SENTENCE_IN_ALT_REL_ROLE=IN_COMMAND_OBJECT_ROLE<<1;

	  public int index;
	  public int objectClass;
	  public int subType;
	  public int begin,end,originalLocation;
	  public int PMAElement;
	  public int numEncounters;
	  public int numIdentifiedAsSpeaker;
	  public int numDefinitelyIdentifiedAsSpeaker;
	  public int numEncountersInSection;
	  public int numSpokenAboutInSection;
	  public int numIdentifiedAsSpeakerInSection;
	  public int numDefinitelyIdentifiedAsSpeakerInSection;
	  public int PISSubject,PISHail,PISDefinite;
	  public int replacedBy;
	  public int ownerWhere;
	  public int firstLocation; // the very first location for this object including replacements
	  public int firstSpeakerGroup;
	  public int firstPhysicalManifestation; // used for matching against unresolvable objects
	  public int lastSpeakerGroup; // used for subgrouping
	  public int ageSinceLastSpeakerGroup;
	  public int masterSpeakerIndex;
	  public int htmlLinkCount;
	  public int relativeClausePM;
	  public int whereRelativeClause;
	  public int whereRelSubjectClause;
	  public int usedAsLocation,lastWhereLocation;
	  public int[] spaceRelations;
	  public int[] duplicates; // objects that this object replaced
	  public int[] aliases; // objects that this object is the same as (but the information cannot be merged, as in duplicates)
	  public String[] associatedNouns; // man
	  public String[] associatedAdjectives; // young, old, strange
	  public int[] possessions; // objects that this object is the same as (but the information cannot be merged, as in duplicates)
	  public Map <String, Integer> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
	  public String mostMatchedGeneric; // girl, woman, boy, man, etc
	  public int[] genericAge;
	  public int age,mostMatchedAge;
	  public boolean identified; // identified as a speaker by afterSpeaker or beforeSpeaker quotes
	  public boolean plural; // not an identified name, with at least one term set to plural
	  public boolean male;
	  public boolean female;
	  public boolean neuter;
	  public boolean ownerPlural;
	  public boolean ownerMale;
	  public boolean ownerFemale;
	  public boolean ownerNeuter;
	  public boolean eliminated;
	  public boolean multiSource;
	  public boolean suspect,verySuspect,ambiguous;
	  public boolean partialMatch;
	  public boolean isNotAPlace;
	  public boolean genderNarrowed;
	  public boolean isKindOf;
	  public boolean wikipediaAccessed;
	  public boolean container; // object or subject of verb that in turn contains other objects
	  public boolean dbPediaAccessed;
	  public boolean isTimeObject;
	  public boolean isLocationObject;
	  public boolean isWikiPlace;
	  public boolean isWikiPerson;
	  public boolean isWikiBusiness;
	  public class cLastVerbTenses
	  {
		  int lastVerb; // book position of main verb
		  int lastTense; // tense of the entire verb
		};
		public cLastVerbTenses[] lastVerbTenses;

		public cName name;

		public int[] locations; // every position in book where object is PLUS all positions where object is matched (to the object at that position)
		public int[] speakerLocations; // quotes where objectMatches/audienceObjectMatches means the object is a speaker within that quote

		// identifySpeakers

		public CObject(LittleEndianDataInputStream rs) {
			index = rs.readInteger();
			objectClass = rs.readInteger();
			subType = rs.readInteger();
			begin = rs.readInteger();
			end = rs.readInteger();
			originalLocation = rs.readInteger();
			PMAElement = rs.readInteger();
			numEncounters = rs.readInteger();
			numIdentifiedAsSpeaker = rs.readInteger();
			numDefinitelyIdentifiedAsSpeaker = rs.readInteger();
			numEncountersInSection = rs.readInteger();
			numSpokenAboutInSection = rs.readInteger();
			numIdentifiedAsSpeakerInSection = rs.readInteger();
			numDefinitelyIdentifiedAsSpeakerInSection = rs.readInteger();
			PISSubject = rs.readInteger();
			PISHail = rs.readInteger();
			PISDefinite = rs.readInteger();
			replacedBy = rs.readInteger();
			ownerWhere = rs.readInteger();
			firstLocation = rs.readInteger();
			firstSpeakerGroup = rs.readInteger();
			firstPhysicalManifestation = rs.readInteger();
			lastSpeakerGroup = rs.readInteger();
			ageSinceLastSpeakerGroup = rs.readInteger();
			masterSpeakerIndex = rs.readInteger();
			htmlLinkCount = rs.readInteger();
			relativeClausePM = rs.readInteger();
			whereRelativeClause = rs.readInteger();
			whereRelSubjectClause = rs.readInteger();
			usedAsLocation = rs.readInteger();
			lastWhereLocation = rs.readInteger();
			int count = rs.readInteger();
			spaceRelations = new int[count];
			for (int I = 0; I < count; I++)
				spaceRelations[I] = rs.readInteger();
			count = rs.readInteger();
			duplicates = new int[count];
			for (int I = 0; I < count; I++)
				duplicates[I] = rs.readInteger();
			count = rs.readInteger();
			aliases = new int[count];
			for (int I = 0; I < count; I++)
				aliases[I] = rs.readInteger();
			count = rs.readInteger();
			associatedNouns = new String[count];
			for (int I=0; I<count; I++)
				associatedNouns[I]=rs.readString();
			count = rs.readInteger();
			associatedAdjectives = new String[count];
			for (int I=0; I<count; I++)
				associatedAdjectives[I]=rs.readString();
			count = rs.readInteger();
			possessions = new int[count];
			for (int I = 0; I < count; I++)
			{
				possessions[I] = rs.readInteger();
		    //System.err.println(possessions[I]+" ");
			}
			// map <tIWMM,int,tFI::cRMap::wordMapCompare> genericNounMap; // keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
			count = rs.readInteger();
			if (count>0) genericNounMap=new HashMap <String, Integer>();  
			for (int I=0; I<count; I++)
				genericNounMap.put(rs.readString(),rs.readInteger());
			mostMatchedGeneric=rs.readString();
			genericAge = new int[4];
			for (int I=0; I<4; I++)
				genericAge[I] = rs.readInteger();
			age = rs.readInteger();
			mostMatchedAge = rs.readInteger();
			long flags = rs.readLong();

			isWikiBusiness = (flags & 1) == 1; flags >>= 1;
			isWikiPerson = (flags & 1) == 1; flags >>= 1;
			isWikiPlace = (flags & 1) == 1; flags >>= 1;
			isLocationObject = (flags & 1) == 1; flags >>= 1;
			isTimeObject = (flags & 1) == 1; flags >>= 1;
			dbPediaAccessed = (flags & 1) == 1; flags >>= 1;
			container = (flags & 1) == 1; flags >>= 1;
			wikipediaAccessed = (flags & 1) == 1; flags >>= 1;
			isKindOf = (flags & 1) == 1; flags >>= 1;
			genderNarrowed = (flags & 1) == 1; flags >>= 1;
			isNotAPlace = (flags & 1) == 1; flags >>= 1;
			partialMatch = (flags & 1) == 1; flags >>= 1;
			ambiguous = (flags & 1) == 1; flags >>= 1;
			verySuspect = (flags & 1) == 1; flags >>= 1;
			suspect = (flags & 1) == 1; flags >>= 1;
			multiSource = (flags & 1) == 1; flags >>= 1;
			eliminated = (flags & 1) == 1; flags >>= 1;
			ownerNeuter = (flags & 1) == 1; flags >>= 1;
			ownerFemale = (flags & 1) == 1; flags >>= 1;
			ownerMale = (flags & 1) == 1; flags >>= 1;
			ownerPlural = (flags & 1) == 1; flags >>= 1;
			neuter = (flags & 1) == 1; flags >>= 1;
			female = (flags & 1) == 1; flags >>= 1;
			male = (flags & 1) == 1; flags >>= 1;
			plural = (flags & 1) == 1; flags >>= 1;
			identified = (flags & 1) == 1; flags >>= 1;

			lastVerbTenses=new cLastVerbTenses[VERB_HISTORY];
			for (int I=0; I<VERB_HISTORY; I++)
			{
				lastVerbTenses[I]=new cLastVerbTenses();
				lastVerbTenses[I].lastTense=rs.readInteger();
				lastVerbTenses[I].lastVerb=rs.readInteger();
			}
			name= new cName(rs);
		}
	}

