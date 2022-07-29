package show;

import java.awt.Color;
import java.awt.Dimension;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.Vector;

import javax.swing.text.BadLocationException;
import javax.swing.text.SimpleAttributeSet;
import javax.swing.text.StyleConstants;

public class Source {

	public static final int PRONOUN_OBJECT_CLASS = 1;
	public static final int REFLEXIVE_PRONOUN_OBJECT_CLASS = 2;
	public static final int RECIPROCAL_PRONOUN_OBJECT_CLASS = 3;
	public static final int NAME_OBJECT_CLASS = 4;
	public static final int GENDERED_GENERAL_OBJECT_CLASS = 5;
	public static final int BODY_OBJECT_CLASS = 6;
	public static final int GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS = 7; // occupation=plumber;role=leader;activity=runner
	public static final int GENDERED_DEMONYM_OBJECT_CLASS = 8;
	public static final int NON_GENDERED_GENERAL_OBJECT_CLASS = 9;
	public static final int NON_GENDERED_BUSINESS_OBJECT_CLASS = 10;
	public static final int PLEONASTIC_OBJECT_CLASS = 11;
	// DEICTIC_OBJECT_CLASS=7, not necessary
	public static final int VERB_OBJECT_CLASS = 12; // the news I brought /
	public static final int NON_GENDERED_NAME_OBJECT_CLASS = 13;
	public static final int META_GROUP_OBJECT_CLASS = 14;
	public static final int GENDERED_RELATIVE_OBJECT_CLASS = 15;

	public static final int UNDEFINED_FORM_NUM = 0;
	public static final int SECTION_FORM_NUM = 1;
	public static final int COMBINATION_FORM_NUM = 2;
	public static final int PROPER_NOUN_FORM_NUM = 3;
	public static final int NUMBER_FORM_NUM = 4;
	public static final int HONORIFIC_FORM_NUM = 5;

	public static final int T_ASSUME_SEQUENTIAL = 0;
	public static final int MTM = 1;
	// on the following lines, flags cannot be combined, only one may be active at
	// a time
	public static final int T_BEFORE = MTM;
	public static final int T_AFTER = MTM * 2;
	public static final int T_PRESENT = MTM * 3;
	public static final int T_THROUGHOUT = MTM * 4;
	public static final int T_RECURRING = MTM * 5;
	public static final int T_AT = MTM * 6;
	public static final int T_MIDWAY = MTM * 7;
	public static final int T_IN = MTM * 8;
	public static final int T_ON = MTM * 9;
	public static final int T_INTERVAL = MTM * 10;
	public static final int T_START = MTM * 11;
	public static final int T_STOP = MTM * 12;
	public static final int T_RESUME = MTM * 13;
	public static final int T_FINISH = MTM * 14;
	public static final int T_RANGE = MTM * 15;
	public static final int T_UNIT = MTM * 16;
	// on the following lines, flags can be combined
	public static final int T_TIME = MTM * 32;
	public static final int T_DATE = MTM * 64;
	public static final int T_TIMEDATE = T_TIME + T_DATE;
	public static final int T_VAGUE = MTM * 128;
	public static final int T_LENGTH = MTM * 256;
	public static final int T_CARDTIME = MTM * 512;

	public static final int UNKNOWN_OBJECT = -1;
	public static final int OBJECT_UNKNOWN_MALE = -2;
	public static final int OBJECT_UNKNOWN_FEMALE = -3;
	public static final int OBJECT_UNKNOWN_MALE_OR_FEMALE = -4;
	public static final int OBJECT_UNKNOWN_NEUTER = -5;
	public static final int OBJECT_UNKNOWN_PLURAL = -6;
	public static final int OBJECT_UNKNOWN_ALL = -7;

	String[] wordOrderWords = { "other", "another", "second", "first", "third", "former", "latter", "that", "this",
			"two", "three", "one", "four", "five", "six", "seven", "eight" };

	String[] OCSubTypeStrings = { "canadian province city", "country", "island", "mountain range peak landform",
			"ocean sea", "park monument", "region", "river lake waterway", "us city town village",
			"us state territory region", "world city town village", "geographical natural feature",
			"geographical urban feature", "geographical urban subfeature", "geographical urban subsubfeature", "travel",
			"moving", "moving natural", "relative direction", "absolute direction", "by activity", "unknown(place)" };
	public static final int CANADIAN_PROVINCE_CITY = 0;
	public static final int COUNTRY = 1;
	public static final int ISLAND = 2;
	public static final int MOUNTAIN_RANGE_PEAK_LANDFORM = 3;
	public static final int OCEAN_SEA = 4;
	public static final int PARK_MONUMENT = 5;
	public static final int REGION = 6;
	public static final int RIVER_LAKE_WATERWAY = 7;
	public static final int US_CITY_TOWN_VILLAGE = 8;
	public static final int US_STATE_TERRITORY_REGION = 9;
	public static final int WORLD_CITY_TOWN_VILLAGE = 10;
	public static final int GEOGRAPHICAL_NATURAL_FEATURE = 11; // lake mountain
	public static final int GEOGRAPHICAL_URBAN_FEATURE = 12; // city, town,
	public static final int GEOGRAPHICAL_URBAN_SUBFEATURE = 13; // rooms within
	public static final int GEOGRAPHICAL_URBAN_SUBSUBFEATURE = 14; // commonly
	public static final int TRAVEL = 15; // trip, journey, road, street, trail
	public static final int MOVING = 16; // train, plane, automobile
	public static final int MOVING_NATURAL = 17; // elephant, horse, _NAME (human)
	public static final int RELATIVE_DIRECTION = 18; // front, rear, side, edge,
	public static final int ABSOLUTE_DIRECTION = 19; // North, South, East, West
	public static final int BY_ACTIVITY = 20; // lunch, dinner, breakfast, party,
	public static final int UNKNOWN_PLACE_SUBTYPE = 21;

	// "time","space","all other","story","quote","non-time transition"
	public static final int INCLUDE_TIME = 0;
	public static final int INCLUDE_SPACE = 1;
	public static final int INCLUDE_ABSTRACT = 2;
	public static final int INCLUDE_STORY = 3;
	public static final int INCLUDE_QUOTES = 4;
	public static final int INCLUDE_TIME_TRANSITION = 5;

	public enum SourceMapType {
		relType, relType2, speakerGroupType, transitionSpeakerGroupType, wordType, QSWordType, ESType, URWordType,
		matchingSpeakerType, matchingType, audienceMatchingType, matchingObjectType, headerType, beType, possessionType,
		tableType
	}

	String location;
	WordMatch[] m;
	int[] sentenceStarts;
	List<Integer> chapters;
	static cSection[] sections;
	static cSpeakerGroup[] speakerGroups;
	Map<Integer, Integer> masterSpeakerList;
	patternElementMatch[] pema;
	CObject[] objects;
	static Relation[] relations;
	TimelineSegment[] timelineSegments;
	// StyledDocument doc;
	int docPosition = 0;
	BatchDocument batchDoc;
	List<Integer> paragraphSpacingOff;
	List<Integer> paragraphSpacingOn;

	public class WhereSource {
		public WhereSource(int inIndex, int inIndex2, int inIndex3, SourceMapType inFlags) {
			index = inIndex;
			index2 = inIndex2;
			index3 = inIndex3;
			flags = inFlags;
		}

		int index;
		int index2;
		int index3;
		SourceMapType flags;
	}

	TreeMap<Integer, WhereSource> positionToSource = null, positionToAgent = null;
	TreeMap<Integer, Integer> sourceToPosition = null, agentToPosition = null;
	int spr; // current relations counter
	int currentSpeakerGroup;
	int currentEmbeddedSpeakerGroup;

	public Source(WordClass Words, LittleEndianDataInputStream rs) {
		masterSpeakerList = new TreeMap<Integer, Integer>();
		if (rs.b == null) {
			m = new WordMatch[0];
			return;
		}
		int version = rs.readInteger();
		location = rs.readString();
		int count = rs.readInteger();
		m = new WordMatch[count];
		try {
			for (int I = 0; I < count; I++)
				m[I] = new WordMatch(Words, rs);
			count = rs.readInteger();
			sentenceStarts = new int[count];
			for (int I = 0; I < count; I++)
				sentenceStarts[I] = rs.readInteger();
			count = rs.readInteger();
			sections = new cSection[count];
			for (int I = 0; I < count; I++)
				sections[I] = new cSection(rs);
			count = rs.readInteger();
			speakerGroups = new cSpeakerGroup[count];
			for (int I = 0; I < count; I++)
				speakerGroups[I] = new cSpeakerGroup(rs);
			count = rs.readInteger();
			pema = new patternElementMatch[count];
			for (int I = 0; I < count; I++)
				pema[I] = new patternElementMatch(rs);
			if (rs.EndOfBufferReached())
			{
				objects = new CObject[0];
				relations = new Relation[0];
				timelineSegments = new TimelineSegment[0];
				return;
			}
			count = rs.readInteger();
			objects = new CObject[count];
			for (int I = 0; I < count; I++) {
				objects[I] = new CObject(rs);
				if (objects[I].masterSpeakerIndex >= 0)
					masterSpeakerList.put(objects[I].masterSpeakerIndex, I);
			}
			if (rs.EndOfBufferReached())
			{
				relations = new Relation[0];
				timelineSegments = new TimelineSegment[0];
				return;
			}
			count = rs.readInteger();
			relations = new Relation[count];
			for (int I = 0; I < count; I++)
				relations[I] = new Relation(rs);
			count = rs.readInteger();
			timelineSegments = new TimelineSegment[count];
			for (int I = 0; I < count; I++)
				timelineSegments[I] = new TimelineSegment(rs);
		} catch (RuntimeException e) {
			e.printStackTrace();
		}
	}

	void addElement(String s, SimpleAttributeSet attrs, int index, int index2, int index3, SourceMapType flags) {
		// try {
		// doc.insertString(doc.getLength(), s, attrs);
		batchDoc.insertString(s, attrs);
		// } catch (BadLocationException e) {
		// TODO Auto-generated catch block
		// e.printStackTrace();
		// }

		positionToSource.put(docPosition, new WhereSource(index, index2, index3, flags));
		if (flags == SourceMapType.headerType || flags == SourceMapType.wordType)
			sourceToPosition.put(index, docPosition);
		docPosition += s.length();
	}

	String getOriginalWord(WordMatch wm) {
		String originalWord = wm.word;
		if (wm.queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (wm.flags & WordMatch.flagFirstLetterCapitalized) != 0)
			originalWord = originalWord.substring(0, 1).toUpperCase() + originalWord.substring(1);
		if ((wm.flags & WordMatch.flagNounOwner) != 0)
			originalWord += "'s";
		if ((wm.flags & WordMatch.flagAllCaps) != 0)
			originalWord = originalWord.toUpperCase();
		return originalWord;
	}

	String phraseString(int begin, int end, boolean shortFormat) {
		String tmp = "";
		for (int K = begin; K < end; K++)
			tmp += getOriginalWord(m[K]) + " ";
		if (begin != end && !shortFormat)
			tmp += "[" + begin + "-" + end + "]";
		return tmp;
	}

	String objectString(int[] objects) {
		String tmp = "";
		for (int s = 0; s < objects.length; s++) {
			tmp += objectString(objects[s], true, true);
			if (s != objects.length - 1)
				tmp += " ";
		}
		return tmp;
	}

	String objectString(cOM om, boolean shortNameFormat, boolean objectOwnerRecursionFlag) {
		String tmp = objectString(om.object, shortNameFormat, objectOwnerRecursionFlag);
		if (shortNameFormat)
			return tmp;
		return tmp + om.salienceFactor;
	}

	String objectString(cOM[] oms, boolean shortNameFormat, boolean objectOwnerRecursionFlag) {
		String tmp = "";
		for (int s = 0; s < oms.length; s++) {
			tmp += objectString(oms[s], shortNameFormat, objectOwnerRecursionFlag);
			if (s != oms.length - 1)
				tmp += " ";
		}
		return tmp;
	}

	String getClass(int objectClass) {
		switch (objectClass) {
		case PRONOUN_OBJECT_CLASS:
			return "pron";
		case REFLEXIVE_PRONOUN_OBJECT_CLASS:
			return "reflex";
		case RECIPROCAL_PRONOUN_OBJECT_CLASS:
			return "recip";
		case NAME_OBJECT_CLASS:
			return "name";
		case GENDERED_GENERAL_OBJECT_CLASS:
			return "gender";
		case BODY_OBJECT_CLASS:
			return "genbod";
		case GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS:
			return "genocc";
		case GENDERED_DEMONYM_OBJECT_CLASS:
			return "gendem";
		case GENDERED_RELATIVE_OBJECT_CLASS:
			return "genre";
		case NON_GENDERED_GENERAL_OBJECT_CLASS:
			return "nongen";
		case NON_GENDERED_BUSINESS_OBJECT_CLASS:
			return "ngbus";
		case NON_GENDERED_NAME_OBJECT_CLASS:
			return "ngname";
		case VERB_OBJECT_CLASS:
			return "vb";
		case PLEONASTIC_OBJECT_CLASS:
			return "pleona";
		case META_GROUP_OBJECT_CLASS:
			return "mg";
		}
		return "ILLEGAL";
	}

	String objectKnownString(int object, boolean shortFormat, boolean objectOwnerRecursionFlag) {
		if (object == 0)
			return "Narrator";
		else if (object == 1)
			return "Audience";
		String tmpstr;
		if (objects[object].objectClass == NAME_OBJECT_CLASS)
			tmpstr = objects[object].name.print(true);
		else
			tmpstr = phraseString(objects[object].begin, objects[object].end, shortFormat);
		int ow = objects[object].ownerWhere;
		if (ow >= 0) {
			boolean selfOwn = object == (m[ow].object);
			for (int I = 0; I < m[ow].objectMatches.length; I++)
				if (object == (m[ow].objectMatches[I].object)) {
					tmpstr = "Self-owning object!";
					selfOwn = true;
				}
			if (!selfOwn && !objectOwnerRecursionFlag) {
				if (m[ow].objectMatches.length > 0)
					objectString(m[ow].objectMatches, true, true);
				else
					tmpstr += objectString(m[ow].object, true, true);
			}
			tmpstr += "{OWNER:" + tmpstr + "}";
		}
		if (objects[object].ownerWhere < -1) {
			tmpstr += "{WO:" + wordOrderWords[-2 - objects[object].ownerWhere] + "}";
		}
		if (!shortFormat) {
			tmpstr += "[" + objects[object].originalLocation + "]";
			tmpstr += "[" + getClass(objects[object].objectClass) + "]";
			if (objects[object].male)
				tmpstr += "[M]";
			if (objects[object].female)
				tmpstr += "[F]";
			if (objects[object].neuter)
				tmpstr += "[N]";
			if (objects[object].plural)
				tmpstr += "[PL]";
			if (objects[object].ownerWhere != -1)
				tmpstr += "[OGEN]";
			// tmpstr+=roleString(objects[object].originalLocation);
			if (objects[object].objectClass == NAME_OBJECT_CLASS) {
				tmpstr += "[" + objects[object].name.print(false) + "]";
			}
			if (objects[object].suspect)
				tmpstr += "[suspect]";
			if (objects[object].verySuspect)
				tmpstr += "[verySuspect]";
			if (objects[object].ambiguous)
				tmpstr += "[ambiguous]";
			if (objects[object].eliminated)
				tmpstr += "[ELIMINATED]";
			if (objects[object].subType >= 0)
				tmpstr += "[" + OCSubTypeStrings[objects[object].subType] + "]";
		}
		if (objects[object].relativeClausePM >= 0 && objects[object].objectClass != NAME_OBJECT_CLASS)
			tmpstr += "[" + phraseString(objects[object].whereRelativeClause,
					objects[object].whereRelativeClause
							+ m[objects[object].whereRelativeClause].pma[objects[object].relativeClausePM].len,
					shortFormat) + "]";
		if (!shortFormat) {
			if (objects[object].associatedAdjectives.length > 0 && objects[object].associatedAdjectives.length < 4) {
				tmpstr += "ADJ:";
				for (int I = 0; I < objects[object].associatedAdjectives.length; I++)
					tmpstr += objects[object].associatedAdjectives[I] + " ";
			}
			if (objects[object].associatedNouns.length > 0 && objects[object].associatedNouns.length < 4) {
				tmpstr += "NOUN:";
				for (int I = 0; I < objects[object].associatedNouns.length; I++)
					tmpstr += objects[object].associatedNouns[I] + " ";
			}
		}
		return tmpstr;
	}

	String objectString(int object, boolean shortNameFormat, boolean objectOwnerRecursionFlag) {
		switch (object) {
		case UNKNOWN_OBJECT:
			return "";
		case OBJECT_UNKNOWN_MALE:
			return "UNKNOWN_MALE";
		case OBJECT_UNKNOWN_FEMALE:
			return "UNKNOWN_FEMALE";
		case OBJECT_UNKNOWN_MALE_OR_FEMALE:
			return "UNKNOWN_MALE_OR_FEMALE";
		case OBJECT_UNKNOWN_NEUTER:
			return "UNKNOWN_NEUTER";
		case OBJECT_UNKNOWN_PLURAL:
			return "UNKNOWN_PLURAL";
		case OBJECT_UNKNOWN_ALL:
			return "ALL";
		default:
			return objectKnownString(object, shortNameFormat, objectOwnerRecursionFlag);
		}
	}

	public static final int stEXIT = 1;
	public static final int stENTER = 2;
	public static final int stSTAY = 3;
	public static final int stESTABLISH = 4;
	public static final int stMOVE = 5;
	public static final int stMOVE_OBJECT = 6;
	public static final int stMOVE_IN_PLACE = 7;
	public static final int stMETAWQ = 8;
	public static final int stCONTACT = 9;
	public static final int stNEAR = 10;
	public static final int stTRANSFER = 11;
	public static final int stLOCATION = 12;
	public static final int stPREPTIME = 13;
	public static final int stPREPDATE = 14;
	public static final int stSUBJDAYOFMONTHTIME = 15;
	public static final int stABSTIME = 16;
	public static final int stABSDATE = 17;
	public static final int stADVERBTIME = 18;
	public static final int stTHINK = 19;
	public static final int stCOMMUNICATE = 20;
	public static final int stSTART = 21;
	public static final int stCHANGE_STATE = 22;
	public static final int stBE = 23;
	public static final int stHAVE = 24;
	public static final int stMETAINFO = 25;
	public static final int stMETAPROFESSION = 26;
	public static final int stMETABELIEF = 27;
	public static final int stTHINKOBJECT = 29;
	public static final int stCHANGESTATE = 30;

	public static final int VT_TENSE_MASK = 7;
	public static final int VT_PRESENT = 1;
	public static final int VT_PRESENT_PERFECT = 2;
	public static final int VT_PAST = 3;
	public static final int VT_PAST_PERFECT = 4;
	public static final int VT_FUTURE = 5;
	public static final int VT_FUTURE_PERFECT = 6;
	public static final int VT_POSSIBLE = 8;
	public static final int VT_PASSIVE = 16;
	public static final int VT_EXTENDED = 32;
	public static final int VT_VERB_CLAUSE = 64;
	public static final int VT_NEGATION = 128;
	public static final int VT_IMPERATIVE = 256;

	String getVerbSenseString(int verbSense) {
		String verbSenseStr = "";
		if (verbSense != 0) {
			if ((verbSense & VT_TENSE_MASK) == VT_PRESENT)
				verbSenseStr += "PRESENT ";
			else if ((verbSense & VT_TENSE_MASK) == VT_PRESENT_PERFECT)
				verbSenseStr += "PRESENT_PERFECT ";
			else if ((verbSense & VT_TENSE_MASK) == VT_PAST)
				verbSenseStr += "PAST ";
			else if ((verbSense & VT_TENSE_MASK) == VT_PAST_PERFECT)
				verbSenseStr += "PAST_PERFECT ";
			else if ((verbSense & VT_TENSE_MASK) == VT_FUTURE)
				verbSenseStr += "FUTURE ";
			else if ((verbSense & VT_TENSE_MASK) == VT_FUTURE_PERFECT)
				verbSenseStr += "FUTURE_PREFECT ";
			if ((verbSense & VT_EXTENDED) == VT_EXTENDED)
				verbSenseStr += "EXTENDED ";
			if ((verbSense & VT_PASSIVE) == VT_PASSIVE)
				verbSenseStr += "PASSIVE ";
			if ((verbSense & VT_NEGATION) == VT_NEGATION)
				verbSenseStr += "NEGATION ";
			if ((verbSense & VT_IMPERATIVE) == VT_IMPERATIVE)
				verbSenseStr += "POSSIBLE ";
			if ((verbSense & VT_POSSIBLE) == VT_POSSIBLE)
				verbSenseStr += "IMPERATIVE ";
		}
		return verbSenseStr;
	}

	String relationString(int r) {
		String[] relStrings = { "", "EXIT", "ENTER", "STAY", "ESTAB", "MOVE", "MOVE_OBJECT", "MOVE_IN_PLACE", "METAWQ",
				"CONTACT", "NEAR", "TRANSFER", "LOCATION", "PREP TIME", "PREP DATE", "SUBJDAY TIME", "ABS TIME",
				"ABS DATE", "ADVERB TIME", "THINK", "COMMUNICATE", "START", "COS", "BE", "HAS-A", "METAINFO",
				"METAPROFESSION", "METABELIEF", "METACONTACT", "THINK_OBJECT", "stCHANGESTATE", "stCONTIGUOUS",
				"stCONTROL", "stAGENTCHANGEOBJECTINTERNALSTATE", "stSENSE", "stCREATE", "stCONSUME", "stMETAFUTUREHAVE",
				"stMETAFUTURECONTACT", "stMETAIFTHEN", "stMETACONTAINS", "stMETADESIRE", "stMETAROLE",
				"stSPATIALORIENTATION", "stIGNORE", "stNORELATION", "stOTHER", "47" };
		try {
			if (r > 0)
				return relStrings[r];
			if (r == -stLOCATION)
				return relStrings[-r];
		} catch (ArrayIndexOutOfBoundsException e) {
			System.out.println("relation not found for #" + r);
		}
		return "unknown";
	}

	Color[] masterSpeakerColors = {
			// new Color(0x00, 0xFF, 0xFF), // "Aqua" too light
			new Color(0x60, 0xCC, 0x93), // "Aquamarine"
			// new Color(0x6F, 0xFF, 0xC3), // "Aquamarine(Light)" too light
			// new Color(0xF6, 0xF6, 0xCC), // "Beige" too light
			// new Color(0xFF, 0xF3, 0xC3), // "Bisque" too light
			// new Color(0x00, 0x00, 0x00), // "Black"
			new Color(0x00, 0x00, 0xFF), // "Blue"
			new Color(0x6F, 0x9F, 0x9F), // "Blue(Cadet)"
			new Color(0x33, 0x33, 0x6F), // "Blue(CornFlower)"
			new Color(0x00, 0x00, 0x9C), // "Blue(Dark)"
			new Color(0x3F, 0x33, 0xF3), // "Blue(Midnight)"
			new Color(0x33, 0x33, 0x9F), // "Blue(Navy)"
			new Color(0x3C, 0x3C, 0xFF), // "Blue(Neon)"
			new Color(0x33, 0x99, 0xCC), // "Blue(Sky)"
			new Color(0x33, 0x6C, 0x9F), // "Blue(Steel)"
			new Color(0x99, 0x3C, 0xF3), // "Blue(Violet)"
			new Color(0xC6, 0x96, 0x33), // "Brass"
			new Color(0x9C, 0x69, 0x63), // "Bronze"
			new Color(0x96, 0x39, 0x39), // "Brown"
			new Color(0x66, 0x30, 0x33), // "Brown(Dark)"
			// new Color(0xF6, 0xCC, 0xC0), // "Brown(Faded)"
			new Color(0x33, 0xCC, 0x99), // "Aquamarine(Med)" // moved to prevent
											// speakers from being to close in color
			new Color(0x6F, 0x9F, 0x90), // "Blue(Cadet3)"
			new Color(0x63, 0x96, 0xFC), // "Blue(CornFlower-Light)"
			new Color(0x6C, 0x33, 0x9F), // "Blue(DarkSlate)"
			new Color(0x99, 0xCC, 0xFF), // "Blue(Light)"
			new Color(0x33, 0x33, 0xCC), // "Blue(Med)"
			new Color(0x6F, 0xFF, 0x00), // "Chartruse"
			new Color(0xC3, 0x69, 0x0F), // "Chocolate"
			new Color(0x6C, 0x33, 0x06), // "Chocolate(Baker's)"
			new Color(0x6C, 0x33, 0x36), // "Chocolate(SemiSweet)"
			new Color(0xC9, 0x63, 0x33), // "Copper"
			new Color(0x39, 0x66, 0x6F), // "Copper(DarkGreen)"
			new Color(0x63, 0x6F, 0x66), // "Copper(Green)"
			new Color(0xFF, 0x6F, 0x60), // "Coral"
			new Color(0xFF, 0x6F, 0x00), // "Coral(Bright)"
			new Color(0xFF, 0xF9, 0xCC), // "Cornsilk"
			new Color(0xCC, 0x03, 0x3C), // "Crimson"
			new Color(0x00, 0xFF, 0xFF), // "Cyan"
			new Color(0x00, 0x9C, 0x9C), // "Cyan(Dark)"
			new Color(0x9F, 0x33, 0x33), // "Firebrick"
			new Color(0xFF, 0x00, 0xFF), // "Fushia"
			new Color(0xFF, 0xCC, 0x00), // "Gold"
			new Color(0xC9, 0xC9, 0x09), // "Gold(Bright)"
			new Color(0xC6, 0xC6, 0x3C), // "Gold(Old)"
			new Color(0xCC, 0xCC, 0x60), // "Goldenrod"
			new Color(0xC9, 0x96, 0x0C), // "Goldenrod(Dark)"
			// new Color(0xF9, 0xF9, 0x9F), // "Goldenrod(Med)" not visible
			new Color(0x90, 0x90, 0x90), // "Gray"
			new Color(0x3F, 0x3F, 0x3F), // "Gray(DarkSlate)"
			new Color(0x66, 0x66, 0x66), // "Gray(Dark)"
			new Color(0x99, 0x99, 0x99), // "Gray(Light)"
			new Color(0xC0, 0xC0, 0xC0), // "Gray(Lighter)"
			new Color(0x00, 0x90, 0x00), // "Green"
			new Color(0x00, 0x63, 0x00), // "Green(Dark)"
			new Color(0x3F, 0x3F, 0x3F), // "Green(Forest)"
			new Color(0x9F, 0xCC, 0x9F), // "Green(Pale)"
			new Color(0x99, 0xCC, 0x33), // "Green(Yellow)"
			new Color(0x9F, 0x9F, 0x6F), // "Khaki"
			new Color(0xCC, 0xC6, 0x6C), // "Khaki(Light)"
			new Color(0x00, 0xFF, 0x00), // "Lime"
			new Color(0xFF, 0x00, 0xFF), // "Magenta"
			new Color(0xCC, 0x00, 0x9C), // "Magenta(Dark)"
			new Color(0x90, 0x00, 0x00), // "Maroon"
			new Color(0x9F, 0x33, 0x6C), // "Maroon3"
			new Color(0x00, 0x00, 0x90), // "Navy"
			new Color(0x90, 0x90, 0x00), // "Olive"
			new Color(0x66, 0x6C, 0x3F), // "Olive(Dark)"
			new Color(0x3F, 0x3F, 0x3F), // "Olive(Darker)"
			new Color(0xFF, 0x6F, 0x00), // "Orange"
			new Color(0xFF, 0x9C, 0x00), // "Orange(Light)"
			new Color(0xCC, 0x60, 0xCC), // "Orchid"
			new Color(0x99, 0x33, 0xCC), // "Orchid(Dark)"
			new Color(0xF9, 0x9C, 0xF9), // "Pink"
			new Color(0xCC, 0x9F, 0x9F), // "Pink(Dusty)"
			new Color(0x90, 0x00, 0x90), // "Purple"
			new Color(0x96, 0x06, 0x69), // "Purple(Dark)"
			new Color(0xC9, 0xC9, 0xF3), // "Quartz"
			new Color(0xFF, 0x00, 0x00), // "Red"
			new Color(0x9C, 0x00, 0x00), // "Red(Dark)"
			new Color(0xFF, 0x33, 0x00), // "Red(Orange)"
			new Color(0x96, 0x63, 0x63), // "Rose(Dusty)"
			new Color(0x6F, 0x33, 0x33), // "Salmon"
			new Color(0x9C, 0x06, 0x06), // "Scarlet"
			new Color(0x9F, 0x6C, 0x33), // "Sienna"
			new Color(0xF6, 0xF9, 0xF9), // "Silver"
			new Color(0x39, 0xC0, 0xCF), // "Sky(Summer)"
			new Color(0xCC, 0x93, 0x60), // "Tan"
			new Color(0x96, 0x69, 0x3F), // "Tan(Dark)"
			new Color(0x00, 0x90, 0x90), // "Teal"
			new Color(0xC9, 0xCF, 0xC9), // "Thistle"
			new Color(0x9C, 0xF9, 0xF9), // "Turquoise"
			new Color(0x00, 0xCF, 0xC0), // "Turquoise(Dark)"
			new Color(0x99, 0x00, 0xFF), // "Violet"
			new Color(0x66, 0x00, 0xCC), // "Violet(Blue)"
			new Color(0xCC, 0x33, 0x99), // "Violet(Red)"
			new Color(0xFF, 0xFF, 0xFF), // "White"
			new Color(0xF9, 0xFC, 0xC6), // "White(Antique)"
			new Color(0xCF, 0xC9, 0x96), // "Wood(Curly)"
			new Color(0x96, 0x6F, 0x33), // "Wood(Dark)"
			new Color(0xF9, 0xC3, 0x96), // "Wood(Light)"
			new Color(0x96, 0x90, 0x63), // "Wood(Med)"
			new Color(0xFF, 0xFF, 0x00) // "Yellow"
	};

	SimpleAttributeSet setAttributes(Relation[] spaceRelations, int spr) {
		SimpleAttributeSet keyWord = new SimpleAttributeSet();
		if (spaceRelations[spr].agentLocationRelationSet) {
			// is the subject moving within a local space, like a room, or is the
			// subject moving from a space to another, like from the building to the
			// street?
			// G + (relations[originalSPRI].beyondLocalSpace) ? "":"S" + SRType + "SR"
			if (spaceRelations[spr].beyondLocalSpace) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setFontSize(keyWord, 12);
			} else
				StyleConstants.setFontSize(keyWord, 8);
			if (spaceRelations[spr].pastHappening)
				StyleConstants.setBackground(keyWord, new Color(170, 0, 241));
			else if (spaceRelations[spr].futureHappening)
				StyleConstants.setBackground(keyWord, new Color(200, 200, 50));
			else if (spaceRelations[spr].beforePastHappening)
				StyleConstants.setBackground(keyWord, new Color(104, 0, 225));
			else if (spaceRelations[spr].futureInPastHappening)
				StyleConstants.setBackground(keyWord, new Color(200, 100, 50));
			else
				StyleConstants.setBackground(keyWord, new Color(100, 224, 225));
		} else {
			if (spaceRelations[spr].genderedLocationRelation) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setBackground(keyWord, new Color(100, 224, 225));
			} else
				StyleConstants.setBackground(keyWord, new Color(200, 200, 200));
		}
		return keyWord;
	}

	boolean shouldPrintRelation(int where, int spr, Boolean[] preference) {
		boolean all = true;
		for (Boolean p : preference)
			if (!p)
				all = false;
		if (all)
			return true;
		boolean timeRelation = relations[spr].timeInfo.length > 0 || (relations[spr].relationType == stABSTIME
				|| relations[spr].relationType == stPREPTIME || relations[spr].relationType == stPREPDATE
				|| relations[spr].relationType == stSUBJDAYOFMONTHTIME || relations[spr].relationType == stABSTIME
				|| relations[spr].relationType == stABSDATE || relations[spr].relationType == stADVERBTIME);
		if (!preference[INCLUDE_STORY] && relations[spr].story)
			return false;
		if (!preference[INCLUDE_QUOTES] && (m[relations[spr].where].objectRole & CObject.IN_PRIMARY_QUOTE_ROLE) != 0)
			return false;
		if (!preference[INCLUDE_TIME_TRANSITION] && !relations[spr].timeTransition)
			return false;
		if (preference[INCLUDE_ABSTRACT] && !(relations[spr].physicalRelation || timeRelation))
			return true;
		if (preference[INCLUDE_TIME])
			return timeRelation;
		if (preference[INCLUDE_SPACE] && !relations[spr].physicalRelation)
			return false;
		return true;
	}

	void printRelation(int where, Boolean[] preferences) {
		if (spr < relations.length && relations[spr].where == where) {
			if (shouldPrintRelation(where, spr, preferences))
				addElement(relationString(relations[spr].relationType), setAttributes(relations, spr), where, spr, -1,
						SourceMapType.relType);
		}
		while (spr < relations.length && relations[spr].where < where)
			spr = relations[spr].nextSPR;
	}

	String printFullRelationString(int originalSPRI, int endSpeakerGroup) {
		String presType = "";
		Relation r = relations[originalSPRI];
		if (r.futureHappening && r.wherePrepObject >= 0 && m[r.wherePrepObject].object >= 0
				&& objects[m[r.wherePrepObject].object].subType == BY_ACTIVITY) {
			for (int li = originalSPRI + 1; Math.abs(relations[li].relationType) == stLOCATION && li < relations.length
					&& relations[li].where < endSpeakerGroup; li++)
				if (relations[li].whereSubject >= 0)
					presType += m[relations[li].whereSubject].word + " ";
		}
		String Q = ((m[r.where].objectRole & CObject.IN_PRIMARY_QUOTE_ROLE) != 0) ? "\"" : "";
		String w = r.where + ":" + Q + ((r.story) ? "story " : "") + r.description + r.presType + presType + Q;
		return w;

	}

	int evaluateSpaceRelation(int where, int endSpeakerGroup, int spr, Boolean[] preferences) {
		if (relations[spr].agentLocationRelationSet) {
			int originalSPRI = spr;
			spr = relations[spr].nextSPR;
			if (shouldPrintRelation(where, originalSPRI, preferences)) {
				String w = printFullRelationString(originalSPRI, endSpeakerGroup);
				addElement(w, setAttributes(relations, originalSPRI), where, spr, -1, SourceMapType.relType2);
				addElement("\n", setAttributes(relations, originalSPRI), where, spr, -1, SourceMapType.relType2);
			}
			if (relations[originalSPRI].whereControllingEntity != -1 && relations[spr].whereControllingEntity == -1
					&& relations[originalSPRI].whereSubject == relations[spr].whereSubject
					&& relations[originalSPRI].whereVerb == relations[spr].whereVerb)
				spr++;
		} else
			spr++;
		return spr;
	}

	BatchDocument fillAgents() {
		docPosition = 0;
		BatchDocument batchDoc = new BatchDocument(masterSpeakerList.size() * 2);
		for (int ms = 0; ms < masterSpeakerList.size(); ms++) {
			int msindex = ms % masterSpeakerColors.length;
			SimpleAttributeSet keyWord = new SimpleAttributeSet();
			StyleConstants.setBold(keyWord, true);
			StyleConstants.setForeground(keyWord, masterSpeakerColors[msindex]);
			String s = objectString(masterSpeakerList.get(ms), false, false);
			s += " ds[" + objects[masterSpeakerList.get(ms)].numDefinitelyIdentifiedAsSpeaker + "] s["
					+ objects[masterSpeakerList.get(ms)].numIdentifiedAsSpeaker + "]";
			batchDoc.insertString(s, keyWord);
			batchDoc.insertString("\n", keyWord);
			positionToAgent.put(docPosition,
					new WhereSource(masterSpeakerList.get(ms), -1, -1, SourceMapType.matchingSpeakerType));
			agentToPosition.put(masterSpeakerList.get(ms), docPosition);
			docPosition += s.length();
		}
		batchDoc.processBatchUpdates(0);
		return batchDoc;
	}

	void printSpeakerGroup(int I, Boolean[] preferences) throws BadLocationException {
		while (currentSpeakerGroup < speakerGroups.length && I == speakerGroups[currentSpeakerGroup].begin) {
			if (speakerGroups[currentSpeakerGroup].tlTransition) {
				SimpleAttributeSet keyWord = new SimpleAttributeSet();
				StyleConstants.setBold(keyWord, true);
				StyleConstants.setBackground(keyWord, new Color(0xFF, 0x00, 0x00));
				addElement("------------------TRANSITION------------------", keyWord, currentSpeakerGroup, I, -1,
						SourceMapType.transitionSpeakerGroupType);
			}
			addElement("\n", null, currentSpeakerGroup, -1, -1, SourceMapType.speakerGroupType);
			paragraphSpacingOn.add(batchDoc.getLength());
			for (int mo : speakerGroups[currentSpeakerGroup].speakers) {
				boolean isPOV = Arrays.binarySearch(speakerGroups[currentSpeakerGroup].povSpeakers, mo) >= 0;
				boolean isObserver = Arrays.binarySearch(speakerGroups[currentSpeakerGroup].observers, mo) >= 0;
				boolean isGrouped = Arrays.binarySearch(speakerGroups[currentSpeakerGroup].groupedSpeakers, mo) >= 0;
				int msindex = objects[mo].masterSpeakerIndex % masterSpeakerColors.length;
				SimpleAttributeSet keyWord = new SimpleAttributeSet();
				StyleConstants.setBold(keyWord, true);
				if (msindex < masterSpeakerColors.length && msindex >= 0)
					StyleConstants.setForeground(keyWord, masterSpeakerColors[msindex]);
				if (isObserver)
					StyleConstants.setBackground(keyWord, new Color(0, 0, 0));
				else if (isPOV)
					StyleConstants.setBackground(keyWord, new Color(140, 140, 140));
				String oStr = "";
				if (isObserver)
					oStr += "[obs]";
				else if (isPOV)
					oStr += "[pov]";
				if (isGrouped)
					oStr += "[grouped]";
				oStr += objectString(mo, false, false);
				addElement(oStr, keyWord, currentSpeakerGroup, -1, mo, SourceMapType.speakerGroupType);
				addElement("\n", keyWord, currentSpeakerGroup, -1, mo, SourceMapType.speakerGroupType);
			}
			addElement("\n", null, currentSpeakerGroup, -1, -1, SourceMapType.speakerGroupType);
			currentSpeakerGroup++;
			if (currentSpeakerGroup >= speakerGroups.length)
				break;
			if (speakerGroups[currentSpeakerGroup - 1].embeddedSpeakerGroups.length > 0)
				currentEmbeddedSpeakerGroup = 0;
			else
				currentEmbeddedSpeakerGroup = -1;
			int endSpr = -1;
			for (endSpr = spr; endSpr < relations.length
					&& relations[endSpr].where < speakerGroups[currentSpeakerGroup].begin; endSpr++)
				;
			if (endSpr > spr) {
				for (int spri = spr; spri < relations.length
						&& relations[spri].where < speakerGroups[currentSpeakerGroup].begin;)
					spri = evaluateSpaceRelation(I, speakerGroups[currentSpeakerGroup].begin, spri, preferences);
			}
		}
		while (currentEmbeddedSpeakerGroup >= 0 && currentSpeakerGroup > 0
				&& currentEmbeddedSpeakerGroup < speakerGroups[currentSpeakerGroup - 1].embeddedSpeakerGroups.length
				&& I == speakerGroups[currentSpeakerGroup
						- 1].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].begin) {
			for (int mo : speakerGroups[currentSpeakerGroup
					- 1].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers) {
				boolean isPOV = Arrays.binarySearch(speakerGroups[currentSpeakerGroup
						- 1].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].povSpeakers, mo) >= 0;
				boolean isObserver = Arrays.binarySearch(speakerGroups[currentSpeakerGroup
						- 1].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].observers, mo) >= 0;
				boolean isGrouped = Arrays
						.binarySearch(
								speakerGroups[currentSpeakerGroup
										- 1].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].groupedSpeakers,
								mo) >= 0;
				int msindex = objects[mo].masterSpeakerIndex % masterSpeakerColors.length;
				if (msindex < 0)
					msindex = 0;
				SimpleAttributeSet keyWord = new SimpleAttributeSet();
				StyleConstants.setForeground(keyWord, masterSpeakerColors[msindex]);
				StyleConstants.setBold(keyWord, true);
				StyleConstants.setFontSize(keyWord, 12);
				if (isObserver)
					StyleConstants.setBackground(keyWord, new Color(0, 0, 0));
				else if (isPOV)
					StyleConstants.setBackground(keyWord, new Color(192, 192, 192));
				String oStr = "";
				if (isObserver)
					oStr += "[obs]";
				else if (isPOV)
					oStr += "[pov]";
				if (isGrouped)
					oStr += "[grouped]";
				oStr += objectString(mo, false, false);
				addElement(oStr, keyWord, currentSpeakerGroup - 1, currentEmbeddedSpeakerGroup, mo,
						SourceMapType.speakerGroupType);
				addElement("\n", keyWord, currentSpeakerGroup - 1, currentEmbeddedSpeakerGroup, mo,
						SourceMapType.speakerGroupType);
			}
			currentEmbeddedSpeakerGroup++;
		}
		if (currentEmbeddedSpeakerGroup >= 0 && currentSpeakerGroup > 0
				&& currentEmbeddedSpeakerGroup < speakerGroups[currentSpeakerGroup - 1].embeddedSpeakerGroups.length
				&& I == speakerGroups[currentSpeakerGroup - 1].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].end) {
			addElement("END", null, currentSpeakerGroup, currentEmbeddedSpeakerGroup, -1,
					SourceMapType.speakerGroupType);
			currentEmbeddedSpeakerGroup = -1;
		}
	}

	void setTimeColorAttributes(int timeColor, SimpleAttributeSet keyWord) {
		if (timeColor > 0) {
			StyleConstants.setBackground(keyWord, new Color(255, 255, 0));
			if ((timeColor & T_UNIT) > 0)
				StyleConstants.setForeground(keyWord, new Color(0, 0, 0));
			else if ((timeColor & T_RECURRING) == T_RECURRING) {
				StyleConstants.setForeground(keyWord, new Color(204, 120, 204));
				StyleConstants.setUnderline(keyWord, true);
			} else if ((timeColor & T_BEFORE) == T_BEFORE) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(60, 255, 60));
			} else if ((timeColor & T_AFTER) == T_AFTER) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(0, 205, 0));
			} else if ((timeColor & T_VAGUE) == T_VAGUE) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(0, 50, 0));
			} else if ((timeColor & T_AT) == T_AT) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(0, 102, 204));
			} else if ((timeColor & T_ON) == T_ON) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(102, 102, 204));
			} else if ((timeColor & T_PRESENT) == T_PRESENT) {
				// StyleConstants.setForeground(keyWord, new Color(102,102,204));
			} else if ((timeColor & T_THROUGHOUT) == T_THROUGHOUT) {
				// StyleConstants.setForeground(keyWord, new Color(102,102,204));
			} else if ((timeColor & T_MIDWAY) == T_MIDWAY) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(204, 120, 204));
			} else if ((timeColor & T_IN) == T_IN) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(60, 255, 60));
			} else if ((timeColor & T_INTERVAL) == T_INTERVAL) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(0, 205, 0));
			} else if ((timeColor & T_START) == T_START) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(0, 50, 0));
			} else if ((timeColor & T_STOP) == T_STOP) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(0, 102, 204));
			} else if ((timeColor & T_RESUME) == T_RESUME) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(102, 102, 204));
			} else if ((timeColor & T_RANGE) == T_RANGE) {
				StyleConstants.setUnderline(keyWord, true);
				StyleConstants.setForeground(keyWord, new Color(152, 152, 204));
			}
		}

	}

	void setVerbSenseAttributes(int verbSense, SimpleAttributeSet keyWord) {
		if (verbSense != 0) {
			if ((verbSense & VT_TENSE_MASK) == VT_PRESENT)
				StyleConstants.setBackground(keyWord, new Color(91, 193, 255));
			else if ((verbSense & VT_TENSE_MASK) == VT_PRESENT_PERFECT)
				StyleConstants.setBackground(keyWord, new Color(91, 255, 193));
			else if ((verbSense & VT_TENSE_MASK) == VT_PAST)
				StyleConstants.setBackground(keyWord, new Color(255, 193, 91));
			else if ((verbSense & VT_TENSE_MASK) == VT_PAST_PERFECT)
				StyleConstants.setBackground(keyWord, new Color(255, 91, 193));
			else if ((verbSense & VT_TENSE_MASK) == VT_FUTURE)
				StyleConstants.setBackground(keyWord, new Color(193, 91, 255));
			else if ((verbSense & VT_TENSE_MASK) == VT_FUTURE_PERFECT)
				StyleConstants.setBackground(keyWord, new Color(193, 193, 51));
			if ((verbSense & VT_EXTENDED) == VT_EXTENDED)
				StyleConstants.setUnderline(keyWord, true);
			if ((verbSense & VT_PASSIVE) == VT_PASSIVE)
				StyleConstants.setForeground(keyWord, new Color(90, 90, 90));
			if ((verbSense & VT_POSSIBLE) == VT_POSSIBLE)
				StyleConstants.setForeground(keyWord, new Color(160, 160, 160));
			if ((verbSense & (VT_PASSIVE | VT_POSSIBLE)) == (VT_PASSIVE | VT_POSSIBLE)) {
				// u=";border-style: solid; border-width: thin";
				if ((verbSense & VT_EXTENDED) == VT_EXTENDED)
					StyleConstants.setForeground(keyWord, new Color(90, 90, 90));
			}
		}
	}

	String getPhrasalVerb(int whereVerb) {
		String baseVerb = m[whereVerb].baseVerb;
		// get_out is very different from get by itself
		if (whereVerb + 1 < m.length && (m[whereVerb + 1].queryWinnerForm(Form.adverbForm) >= 0
				|| m[whereVerb + 1].queryWinnerForm(Form.prepositionForm) >= 0
				|| m[whereVerb + 1].queryWinnerForm(Form.nounForm) >= 0)) 
			return baseVerb + "_" + m[whereVerb + 1].word;
		return "";
	}
	
	boolean isVerbClass(int where,String verbClass)
	{
		String baseVerb = m[where].baseVerb;
		String phrasalVerb = getPhrasalVerb(where);
		return Show.vn.isVerbClass(baseVerb, phrasalVerb, verbClass);
	}
	
	BatchDocument print(WordClass Words, SteppedComboBox pickChapter, Boolean[] preferences) {
		positionToSource = new TreeMap<Integer, WhereSource>();
		sourceToPosition = new TreeMap<Integer, Integer>();
		positionToAgent = new TreeMap<Integer, WhereSource>();
		agentToPosition = new TreeMap<Integer, Integer>();
		paragraphSpacingOff = new ArrayList<Integer>();
		paragraphSpacingOn = new ArrayList<Integer>();
		pickChapter.removeAllItems();
		pickChapter.setPreferredSize(new Dimension(500, 30));
		int section = 0;
		docPosition = 0;
		spr = 0;
		// doc = new DefaultStyledDocument(); // creating new detached document is 30%
		// faster than using built-in
		batchDoc = new BatchDocument(m.length * 2);
		int total = m.length, I = -1, last = -1, current, inObject = -1, endObject = 0, principalWhere = -1,
				inObjectSubType = -1;
		int numCurrentColumn = 0, numCurrentRow = 0, numTotalColumns = 0;
		currentSpeakerGroup = 0;
		boolean inSection = false;
		chapters = new ArrayList<Integer>();
		for (WordMatch wm : m) {
			current = (100 * I++ / total);
			if (current != last) {
				// System.out.println(current + " percent.");
				last = current;
			}
			inSection = false;
			try {
				SimpleAttributeSet keyWord = new SimpleAttributeSet();
				StyleConstants.setBackground(keyWord, new Color(255, 255, 255));
				if (section < sections.length && I >= sections[section].begin && I <= sections[section].endHeader) {
					inSection = true;
					StyleConstants.setBackground(keyWord, new Color(122, 224, 255));
					StyleConstants.setFontSize(keyWord, 22);
					StyleConstants.setBold(keyWord, true);
					if (I == sections[section].begin) {
						String sectionHeader = "";
						int is = sections[section].begin;
						if (m[is].word.equals("chapter"))
							is++;
						for (; is < sections[section].endHeader && sectionHeader.length() < 50; is++)
							if (!m[is].word.equals("|||"))
								sectionHeader += m[is].word + " ";
						pickChapter.addItem(sectionHeader);
						chapters.add(is);
					}
				}
				if (wm.object >= 0 && objects.length>wm.object) {
					inObject = wm.object;
					if (objects[inObject].masterSpeakerIndex < 0 && objects[inObject].aliases.length > 0) {
						for (int ai : objects[inObject].aliases)
							if (objects[ai].masterSpeakerIndex >= 0) {
								inObject = ai;
								break;
							}
					}
					inObjectSubType = objects[inObject].subType;
					int nextEndObject = wm.endObjectPosition;
					if (endObject < nextEndObject) {
						endObject = nextEndObject;
						principalWhere = I;
					}
				}
				if (I >= endObject)
					inObject = inObjectSubType = -1;
				if (inObject >= 0)
					StyleConstants.setForeground(keyWord, new Color(31, 82, 0));
				if (I == principalWhere || (inObject >= 0 && objects[inObject].objectClass == NAME_OBJECT_CLASS)) {
					StyleConstants.setForeground(keyWord, new Color(31, 134, 0));
					if ((wm.objectRole & CObject.POV_OBJECT_ROLE) != 0)
						StyleConstants.setBackground(keyWord, new Color(0, 0, 0));
				}
				if (inObjectSubType >= 0) {
					StyleConstants.setBackground(keyWord, new Color(204, 0, 255));
					StyleConstants.setForeground(keyWord, new Color(255, 255, 255));
				}
				// if (!htmlClass && !timeColor &&
				// !(wm.flags&WordMatch::flagRelationsAlreadyEvaluated) &&
				// wm.word->second.query(quoteForm)<0)
				// htmlClass="unp";
				setTimeColorAttributes(wm.timeColor, keyWord);
				setVerbSenseAttributes(wm.verbSense, keyWord);
				if (wm.verbSense == 0 && inObject >= 0 && objects[inObject].masterSpeakerIndex >= 0) {
					int msindex = objects[inObject].masterSpeakerIndex % masterSpeakerColors.length;
					StyleConstants.setForeground(keyWord, masterSpeakerColors[msindex]);
				}
				paragraphSpacingOff.add(batchDoc.getLength());
				if (wm.word.equals("|||"))
					addElement("\n", null, I, -1, -1, SourceMapType.wordType);
				printRelation(I, preferences);
				printSpeakerGroup(I, preferences);
				if (!wm.word.equals("|||")) {
					SourceMapType smt = SourceMapType.wordType;
					if (inSection)
						smt = SourceMapType.headerType;
					boolean tablesmart = true;
					if (tablesmart) {
						if (wm.word.equals("lpendcolumnheaders")) {
							numTotalColumns = numCurrentColumn;
							numCurrentColumn = 0;
							continue;
						}
						if ((I > 3 && m[I - 2].word.equals("lptable")) || wm.word.equals("lpendcolumn")) {
							addElement("\n", null, I, section, -1, SourceMapType.headerType);
							if (wm.word.equals("lpendcolumn")
									&& (I + 2 < m.length && !m[I + 2].word.equals("lpendcolumnheaders")
											&& !m[I + 2].word.equals("lptable"))) {
								// System.out.println("I-2:"+m[I-2].word);
								// System.out.println("I-1:"+m[I-1].word);
								// System.out.println("I:"+m[I].word);
								// System.out.println("I+1:"+m[I+1].word);
								// System.out.println("I+2:"+m[I+2].word);
								numCurrentColumn++;
								if (numCurrentColumn > numTotalColumns && numTotalColumns > 0) {
									numCurrentColumn = 0;
									numCurrentRow++;
								}
								StyleConstants.setForeground(keyWord, Color.orange);
								if (numCurrentRow == 0 && numTotalColumns == 0)
									addElement("Column Title." + String.valueOf(numCurrentColumn) + ":", keyWord, I,
											section, -1, SourceMapType.headerType);
								else
									addElement(String.valueOf(numCurrentRow) + "." + String.valueOf(numCurrentColumn)
											+ ":", keyWord, I, section, -1, SourceMapType.headerType);
							}
							continue;
						}
						if (I > 1 && (m[I - 1].word.equals("lpendcolumn")
								|| (I + 1 < m.length && m[I + 1].word.equals("lpendcolumn"))))
							continue;
						if (I > 1 && m[I - 1].word.equals("lptable")) {
							StyleConstants.setForeground(keyWord, Color.RED);
							StyleConstants.setFontSize(keyWord, 25);
							StyleConstants.setBold(keyWord, true);
							StyleConstants.setUnderline(keyWord, true);
							addElement(String.valueOf(Integer.parseInt(wm.word) + 1), keyWord, I, section, -1, smt);
							continue;
						}
						if (wm.word.equals("lptable")) {
							addElement("\n", null, I, section, -1, SourceMapType.headerType);
							addElement("\n", null, I, section, -1, SourceMapType.headerType);
							smt = SourceMapType.tableType;
							SimpleAttributeSet keyWordTable = new SimpleAttributeSet();
							StyleConstants.setForeground(keyWordTable, Color.RED);
							StyleConstants.setBold(keyWordTable, true);
							StyleConstants.setFontSize(keyWordTable, 25);
							StyleConstants.setUnderline(keyWordTable, true);
							keyWord = keyWordTable;
							numCurrentColumn = 0;
							addElement("TABLE ", keyWord, I, section, -1, smt);
							// numTable++;
							continue;
						}
					}
					addElement(getOriginalWord(wm), keyWord, I, section, -1, smt);
				}
				if ((wm.word.equals("“") || wm.word.equals("‘"))) {
					// if (wm.word.equals("“"))
					// lastOpeningPrimaryQuote = I;
					if ((wm.flags & WordMatch.flagQuotedString) != 0) {
						SimpleAttributeSet keyWordQS = new SimpleAttributeSet();
						StyleConstants.setForeground(keyWordQS, Color.GREEN);
						addElement("QS", keyWordQS, I, -1, -1, SourceMapType.QSWordType); // green
					} else if (wm.objectMatches.length != 1 || wm.audienceObjectMatches.length == 0
							|| (currentSpeakerGroup < speakerGroups.length
									&& wm.audienceObjectMatches.length >= speakerGroups[currentSpeakerGroup].speakers.length
									&& speakerGroups[currentSpeakerGroup].speakers.length > 1)) {
						SimpleAttributeSet keyWordRED = new SimpleAttributeSet();
						StyleConstants.setForeground(keyWordRED, Color.RED);
						addElement("Unresolved", keyWordRED, I, -1, -1, SourceMapType.URWordType); // green
					}
					// only mark stories that have a speaker relating a tale which does
					// not involve the person being spoken to
					// so 'we' does not mean the speaker + the audience, if there are any
					// other agents mentioned in the story before
					if ((wm.flags & WordMatch.flagEmbeddedStoryResolveSpeakers) != 0) {
						SimpleAttributeSet keyWordRED = new SimpleAttributeSet();
						StyleConstants.setForeground(keyWordRED, Color.RED);
						String ESL = "ES" + (((wm.flags & WordMatch.flagFirstEmbeddedStory) != 0) ? '1' : ' ')
								+ (((wm.flags & WordMatch.flagSecondEmbeddedStory) != 0) ? '2' : ' ');
						addElement(ESL, keyWordRED, I, -1, -1, SourceMapType.ESType);
					}
				}
				if (!inSection)
					StyleConstants.setBackground(keyWord, Color.WHITE);
				for (int J = 0; J < wm.objectMatches.length; J++) {
					int mObject = wm.objectMatches[J].object;
					int objectWhere = objects[mObject].originalLocation;
					// skip honorific
					if ((m[objectWhere].queryWinnerForm(Form.honorificForm) >= 0
							|| m[objectWhere].queryWinnerForm(Form.honorificAbbreviationForm) >= 0)
							&& objectWhere + 1 < m[objectWhere].endObjectPosition) {
						objectWhere++;
						if (m[objectWhere].word.equals("."))
							objectWhere++;
					}
					String w;
					if (mObject == 0)
						w = "Narrator";
					else if (mObject == 1)
						w = "Audience";
					else
						w = m[objectWhere].word;
					String S = ((J == 0) ? "[" : "") + w + ((J < wm.objectMatches.length - 1) ? "," : "");
					if (objects[mObject].masterSpeakerIndex >= 0) {
						int msindex = objects[mObject].masterSpeakerIndex % masterSpeakerColors.length;
						StyleConstants.setForeground(keyWord, masterSpeakerColors[msindex]);
						addElement(S, keyWord, I, mObject, J, SourceMapType.matchingSpeakerType);
					} else {
						addElement(S, null, I, mObject, J, SourceMapType.matchingObjectType);
						if (J > 2) {
							addElement("...", null, I, mObject, J, SourceMapType.matchingObjectType);
							break;
						}
					}
				}
				if (wm.objectMatches.length > 0 && wm.audienceObjectMatches.length > 0)
					addElement(":", null, I, -1, -1, SourceMapType.matchingType);
				if (wm.objectMatches.length > 0 && wm.audienceObjectMatches.length == 0)
					addElement("]", null, I, -1, -1, SourceMapType.matchingType);
				if (wm.objectMatches.length == 0 && wm.audienceObjectMatches.length > 0)
					addElement("[", null, I, -1, -1, SourceMapType.matchingType);
				for (int J = 0; J < wm.audienceObjectMatches.length; J++) {
					StyleConstants.setForeground(keyWord, new Color(102, 204, 255));
					String w;
					int mObject = wm.audienceObjectMatches[J].object;
					if (mObject == 0)
						w = "Narrator";
					else if (mObject == 1)
						w = "Audience";
					else
						w = m[objects[mObject].originalLocation].word;
					addElement(w + ((J < wm.audienceObjectMatches.length - 1) ? "," : "]"), null, I,
							wm.audienceObjectMatches[J].object, J, SourceMapType.audienceMatchingType);
					if (J > 2) {
						addElement("...]", null, I, wm.audienceObjectMatches[J].object, J,
								SourceMapType.audienceMatchingType);
						break;
					}
				}
				boolean newHeader = section < sections.length && I == sections[section].endHeader;
				// if (section < sections.length && I < sections[section].endHeader) {
				// for (int s = 0; s < sections[section].subHeadings.length; s++)
				// newHeader |= (I == sections[section].subHeadings[s]);
				// }
				if (newHeader)
					addElement("\n", null, I, section, -1, SourceMapType.headerType);
				else if (!wm.word.equals("|||"))
					addElement(" ", keyWord, I, -1, -1, SourceMapType.wordType);
				if (wm.word.equals("|||"))
					continue;
				if (section < sections.length - 1 && I >= sections[section].endHeader)
					section++;
			} catch (BadLocationException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		batchDoc.processBatchUpdates(0);
		Dimension cd = pickChapter.getMinimumSize();
		pickChapter.setPreferredSize(new Dimension(100, 30));
		pickChapter.setPopupWidth(cd.width);
		System.out.println("chapter minimum width=" + cd.width);
		return batchDoc;
		// optionally apply paragraph spacing
		// System.exit(0);
	}

}