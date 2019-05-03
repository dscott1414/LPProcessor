package show;
	public class tFI {
		public static final int MAX_USAGE_PATTERNS=16;
	  int inflectionFlags;
	  int flags;
		int timeFlags;
	  int index; // to DB
	  int sourceId; // where the word came from
	  byte[] usagePatterns; // usage counts for every class of this word
	  byte[] usageCosts;
	  byte[] deltaUsagePatterns;
		int numProperNounUsageAsAdjective;
	  int derivationRules;
	  String mainEntry;
		int[] relatedSubTypes;
		int[] relatedSubTypeObjects;
	  //cRMap *relationMaps[numRelationWOTypes];
	  int verbRelationIndex; // if not verb, this is -1
	  boolean changedSinceLastWordRelationFlush;
	  int[] forms;
	  int count;

	  boolean hasWinnerVerbForm(int winnerForms)
	  {
	    for (int I=0; I<count; I++)
	  		if (Form.forms[forms[I]].verbForm && (winnerForms==0 || ((1<<I)&winnerForms)!=0))
	  			return true;
	    return false;
	  }


		public tFI(LittleEndianDataInputStream rs) {
			count=rs.readInteger();
			forms=new int[count];
			for (int I=0; I<count; I++)
				forms[I]=rs.readInteger();
			inflectionFlags=rs.readInteger();
			flags=rs.readInteger();
			timeFlags=rs.readInteger();
			derivationRules=rs.readInteger();
			index=rs.readInteger();
			mainEntry=rs.readString();
			usagePatterns=new byte[MAX_USAGE_PATTERNS];
			for (int I=0; I<MAX_USAGE_PATTERNS; I++)
				usagePatterns[I]=rs.readByte();
			usageCosts=new byte[MAX_USAGE_PATTERNS];
			for (int I=0; I<MAX_USAGE_PATTERNS; I++)
				usageCosts[I]=rs.readByte();
		}
		public static final int topLevelSeparator=1; 
		public static final int ignoreFlag=2;
		public static final int queryOnLowerCase=4; 
		public static final int queryOnAnyAppearance=8; 
		public static final int updateMainInfo=32;
		public static final int updateMainEntry=64;
		public static final int insertNewForms=128; 
		public static final int isMainEntry=256;
		public static final int intersectionGroup=512; 
		public static final int wordRelationsRefreshed=1024; 
		public static final int newWordFlag=2048;
		public static final int inSourceFlag=4096; 
		public static final int alreadyTaken=8192*256; 
		public static final int physicalObjectByWN=8192*512; 
		public static final int notPhysicalObjectByWN=8192*1024;
		public static final int uncertainPhysicalObjectByWN=notPhysicalObjectByWN<<1;
		public static final int genericGenderIgnoreMatch=uncertainPhysicalObjectByWN<<1;
		public static final int prepMoveType=genericGenderIgnoreMatch<<1;
		public static final int genericAgeGender=prepMoveType<<1;
		public static final int stateVerb=genericAgeGender<<1;
		public static final int possibleStateVerb=stateVerb<<1;
		public static final int lastWordFlag=possibleStateVerb<<1;

		// enum InflectionTypes {
		public static final int _MIL=1024*1024;

		public static final int SINGULAR=1;
		public static final int PLURAL=2;
		public static final int SINGULAR_OWNER=4;
		public static final int PLURAL_OWNER=8;
		public static final int VERB_PAST=16;
		public static final int VERB_PAST_PARTICIPLE=32;
		public static final int VERB_PRESENT_PARTICIPLE=64;
		public static final int VERB_PRESENT_THIRD_SINGULAR=128;
		public static final int VERB_PRESENT_FIRST_SINGULAR=256;
		// These 5 verb forms must remain in sequence
		public static final int VERB_PAST_THIRD_SINGULAR=512;
		public static final int VERB_PAST_PLURAL=1024;
		public static final int VERB_PRESENT_PLURAL=2048;
		public static final int VERB_PRESENT_SECOND_SINGULAR=4096;
		public static final int ADJECTIVE_NORMATIVE=8192;
		public static final int ADJECTIVE_COMPARATIVE=16384;
		public static final int ADJECTIVE_SUPERLATIVE=32768;
		public static final int ADVERB_NORMATIVE=65536;
		public static final int ADVERB_COMPARATIVE=131072;
		public static final int ADVERB_SUPERLATIVE=262144;
		public static final int MALE_GENDER=_MIL*1;
		public static final int FEMALE_GENDER=_MIL*2;
		public static final int NEUTER_GENDER=_MIL*4;
		public static final int FIRST_PERSON=_MIL*8 /*ME*/;
		public static final int SECOND_PERSON=_MIL*16 /*YOU*/;
		public static final int THIRD_PERSON=_MIL*32 /*THEM*/;
		public static final int NO_OWNER=_MIL*64;
		public static final int VERB_NO_PAST=_MIL*64; // special case to match only nouns and Proper Nouns that do not have 's / and verbs that should only be past participles

		public static final int OPEN_INFLECTION=_MIL*128;
		public static final int CLOSE_INFLECTION=_MIL*256;
		// overlap
		public static final int MALE_GENDER_ONLY_CAPITALIZED=_MIL*512;
		public static final int FEMALE_GENDER_ONLY_CAPITALIZED=_MIL*1024;
		public static final int ONLY_CAPITALIZED=(MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED);
		

	}

