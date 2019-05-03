package show;
	public class Form {
    static Form[] forms;
	  boolean hasInflections;
	  boolean isTopLevel;
	  boolean isIgnore;
	  String name;
	  String shortName;
	  String inflectionsClass;
	  int index; // to DB
	  // classes that have this set: month, all place forms
	  // this means that even though it is capitalized, and not seen uncapitalized, words having this form will not be considered
	  // as ONLY a proper noun.  It will have a Proper Noun as well as the form.
	  boolean properNounSubClass;
	  boolean verbForm;
	  // only honorific.  So if this word is capitalized, it will not be recognized as a Proper Noun at all.
	  boolean blockProperNounRecognition;
	  boolean formCheck; // used only when checking dictionary entries

		public Form(LittleEndianDataInputStream rs) {
	    name=rs.readString();
	    shortName=rs.readString();
	    inflectionsClass=rs.readString();
	    hasInflections = rs.readShort()!=0;
	    properNounSubClass = rs.readShort()!=0;
	    isTopLevel = rs.readShort()!=0;
	    isIgnore = rs.readShort()!=0;
	    verbForm = rs.readShort()!=0;
	    blockProperNounRecognition = rs.readShort()!=0;
	    formCheck = rs.readShort()!=0;
		}
		public static int findForm(String name) {
			for (int f=0; f<forms.length; f++)
				if (forms[f].name.equals(name))
					return f;
			return -1;
		}
		public static void initializeForms()
		{
			adverbForm=findForm("adverb");
			nounForm=findForm("noun");
			adjectiveForm=findForm("adjective");
			prepositionForm=findForm("preposition");
			honorificForm = Form.findForm("honorific");
			honorificAbbreviationForm = Form.findForm("honorific_abbreviation");
		}
		public static int adverbForm=-1;//=findForm("adverb");
		public static int nounForm=-1;//=findForm("noun");
		public static int adjectiveForm=-1;//=findForm("adjective");
		public static int prepositionForm=-1;//=findForm("preposition");
		public static int honorificForm = -1;//Form.findForm("honorific");
		public static int honorificAbbreviationForm = -1;//Form.findForm("honorific_abbreviation");
		
	}

