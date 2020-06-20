package show;

public class WordMatch {
	long flags;
	
	static long flagUsedPossessionRelation = ((long)1 << 53);
	static long flagUsedBeRelation = ((long)1 << 52);
	static long flagInLingeringStatement = ((long)1 << 51);
	static long flagRelativeObject = ((long)1 << 50);
	static long flagInInfinitivePhrase = ((long)1 << 49);
	static long flagResolvedByRecent = ((long)1 << 48);
	static long flagLastContinuousQuote = ((long)1 << 47);
	static long flagAlternateResolveForwardFromLastSubjectAudience = ((long)1 << 46);
	static long flagAlternateResolveForwardFromLastSubjectSpeakers = ((long)1 << 45);
	static long flagGenderIsAmbiguousResolveAudience = ((long)1 << 44); // when a
	// definitely specified audience is a gendered pronoun with more than one
	// speaker in the current speaker group having that gender
	static long flagRelativeHead = ((long)1 << 43);
	static long flagQuoteContainsSpeaker = ((long)1 << 42);
	static long flagResolveMetaGroupByGender = ((long)1 << 41);
	static long flagEmbeddedStoryResolveSpeakersGap = ((long)1 << 40);
	static long flagEmbeddedStoryBeginResolveSpeakers = ((long)1 << 39); // only set if
	// aged
	static long flagAge = ((long)1 << 38); // only set if aged
	// set dynamically?
	// static long flagWNNotPhysicalObject=(1<<37);
	static long flagWNPhysicalObject = ((long)1 << 36);
	static long flagInPStatement = ((long)1 << 35);
	static long flagInQuestion = ((long)1 << 34);
	static long flagRelationsAlreadyEvaluated = ((long)1 << 32);
	static long flagNounOwner = ((long)1 << 31);
	// form flags
	static long flagAddProperNoun = (1 << 30);
	static long flagOnlyConsiderProperNounForms = (1 << 29);
	static long flagAllCaps = (1 << 28);
	static long flagTopLevelPattern = (1 << 27);
	static long flagPossiblePluralNounOwner = (1 << 26);
	static long flagNotMatched = (1 << 25);
	static long flagInsertedQuote = (1 << 24);
	static long flagFirstLetterCapitalized = (1 << 23);
	static long flagRefuseProperNoun = (1 << 22);
	static long flagMetaData = (1 << 21);
	static long flagOnlyConsiderOtherNounForms = (1 << 20);
	// BNC patternPreferences (only used for nonQuotes)
	static long flagBNCPreferNounPatternMatch = (1 << 19);
	static long flagBNCPreferVerbPatternMatch = (1 << 18);
	static long flagBNCPreferAdjectivePatternMatch = (1 << 17);
	static long flagBNCPreferAdverbPatternMatch = (1 << 16);
	static long flagBNCPreferIgnore = (1 << 15);
	static long flagBNCFormNotCertain = (1 << 14); // tag was not set on word
	// (because a tag was set for
	// multiple words)

	// speaker resolution (only used for quotes)
	static long flagFromPreviousHailResolveSpeakers = (1 << 23);
	static long flagAlphaBeforeHint = (1 << 22);
	static long flagAlphaAfterHint = (1 << 21);
	static long flagAlternateResolutionFinishedSpeakers = (1 << 20);
	static long flagForwardLinkResolveAudience = (1 << 19);
	static long flagForwardLinkResolveSpeakers = (1 << 18);
	static long flagFromLastDefiniteResolveAudience = (1 << 17);
	static long flagDefiniteResolveSpeakers = (1 << 16);
	static long flagMostLikelyResolveAudience = (1 << 15);
	static long flagMostLikelyResolveSpeakers = (1 << 14);
	static long flagSpecifiedResolveAudience = (1 << 13);
	static long flagAudienceFromSpeakerGroupResolveAudience = (1 << 12);
	static long flagHailedResolveAudience = (1 << 11);
	// if one speaker is known (flagDefiniteResolveSpeakers) then resolve others
	// around it by assuming the speakers alternate
	static long flagAlternateResolveBackwardFromDefiniteAudience = (1 << 10); // resolves
	// unresolved speakers backwards from a known definite speaker
	static long flagAlternateResolveBackwardFromDefiniteSpeakers = (1 << 9); // resolves
	// unresolved speakers backwards from a	known definite speaker
	static long flagForwardLinkAlternateResolveAudience = (1 << 8);
	static long flagForwardLinkAlternateResolveSpeakers = (1 << 7);
	static long flagEmbeddedStoryResolveSpeakers = (1 << 6);
	static long flagAlternateResolveForwardAfterDefinite = (1 << 5); // this flag
	// is set for all alternative settings forward after a definite
	// speaker previously results from imposeMostLikely were only set with
	// flagMostLikelyResolveSpeakers flagAlternateResolveForwardAfterDefinite allows an alternate speaker
	// imposed from below and an alternate speaker
	// imposed from above to stop running over each other
	// 1<<4 (flagObjectResolved) is zeroed for all positions after
	// identifySpeakerGroups
	static long flagGenderIsAmbiguousResolveSpeakers = (1 << 3); // when a
	// definitely specified speaker is a gendered pronoun with more than one
	// speaker in the current speaker group having that gender
	static long flagQuotedString = (1 << 2);
	static long flagSecondEmbeddedStory = (1 << 1);
	static long flagFirstEmbeddedStory = (1 << 0);
	// only for non-quotes
	static long flagObjectResolved = (1 << 4);
	static long flagAdjectivalObject = (1 << 3);
	static long flagUnresolvableObjectResolvedThroughSpeakerGroup = (1 << 2);
	static long flagObjectPleonastic = (1 << 1);
	static long flagIgnoreAsSpeaker = (1 << 0);

	short maxMatch;
	int minAvgCostAfterAssessCost;
	int lowestAverageCost;
	short maxLACMatch;
	short maxLACAACMatch;
	long objectRole;
	byte verbSense;
	byte timeColor;
	int beginPEMAPosition;
	int endPEMAPosition;
	int PEMACount;
	patternMatch[] pma;
	bitObject forms;
	bitObject winnerForms; // used for BNC to remember tagged form
	bitObject patterns;
	int object;
	int principalWherePosition; // principalWherePosition is set at the
	// beginning location of an object - its
	// value is the principalWhere of the object
	int principalWhereAdjectivalPosition; // principalWhereAdjectivalPosition
	// is at the beginning of an
	// object used as an adjective.
	int originalObject; // object that has been replaced by another object
	// but that reflects the object originally found at
	// this position
	// a position may be both subject and object
	int relObject; // if this is a subject, what object does it relate to?
	// (for pronoun disambiguation) - also used if speaker
	// explicitly names another speaker
	// (flagQuoteContainsSpeaker)
	int relNextObject; // if this is a subject, what is the next subject in
	// the same sentence? If object, what is the next
	// object in the same sentence? (for pronoun
	// disambiguation)
	int nextCompoundPartObject; // subject or object, links compound objects
	// together
	int previousCompoundPartObject; // subject or object, links compound
	// objects together : also links an
	// infinitive verb back to the mainVerb
	int relSubject; // if this is an object, what subject does it relate to?
	// (for pronoun disambiguation)
	int relVerb; // for subjects and objects
	int relPrep; // subjects, objects and verbs should have this set to the
	// prepositions of the prep phrases,
	// and prep phrases themselves have them set to the next prep phrase in
	// the sentence
	// I passed two Johnnies in the street talking about ... / passed is
	// 'two Johnnies' relVerb, relInternalVerb is set to 'talking'
	int relInternalVerb; // he wanted you to come to the dance. relVerb of
	// you is 'wanted' relInternalVerb of you is
	// 'come'
	// relInternalObject is also used for another object of the same subject
	// location but the object is of another verb. This verb may be before
	// the subject (_INTRO_S1)
	int relInternalObject; // I guess you have no right to ...
	// relInternalObject of I is 'you' - this is to
	// prevent LL routines
	int beginObjectPosition, endObjectPosition; // where is the object
	// defined on this position
	// begin and end?
	cOM[] objectMatches;
	cOM[] audienceObjectMatches;
	// the next field is also used to store tsSense for verbs
	int quoteForwardLink; // next quote in same paragraph (or tsSense)
	int quoteBackLink; // previous quote in same paragraph
	int nextQuote; // set at the beginning of the quote to the beginning of
	// the next quote in a separate paragraph
	int previousQuote; // set at the beginning of the quote to the beginning
	// of the previous quote in a separate paragraph
	int endQuote; // matching end quote
	int tmpWinnerForms;
	int skipResponse; // not saved
	int embeddedStorySpeakerPosition; // tracks who is speaking when they
	// are relating an event that
	// happened in the past
	int audiencePosition, speakerPosition;
	boolean printBeforeElimination, traceAgreement, traceEVALObjects, traceAnaphors, traceRelations, traceSpeakerResolution, traceNyms,
			traceRole;
	boolean traceObjectResolution, traceVerbObjects, traceDeterminer, traceBNCPreferences, tracePatternElimination, traceNameResolution;
	boolean traceSecondaryPEMACosting, traceMatchedSentences, traceUnmatchedSentences, traceIncludesPEMAIndex;
	boolean traceTagSetCollection, collectPerSentenceStats, spaceRelation, hasVerbRelations, andChainType, notFreePrep;
	short logCache;
	String word, baseVerb;

	public WordMatch(WordClass Words, LittleEndianDataInputStream rs) {
		word = rs.readString();
		baseVerb = rs.readString();
		flags = rs.readLong();
		maxMatch = rs.readShort();
		minAvgCostAfterAssessCost = rs.readInteger();
		lowestAverageCost = rs.readInteger();
		maxLACMatch = rs.readShort();
		maxLACAACMatch = rs.readShort();
		objectRole = rs.readLong();
		verbSense = rs.readByte();
		timeColor = rs.readByte();
		beginPEMAPosition = rs.readInteger();
		endPEMAPosition = rs.readInteger();
		PEMACount = rs.readInteger();
		int count = rs.readInteger();
		pma = new patternMatch[count];
		for (int I = 0; I < count; I++)
			pma[I] = new patternMatch(rs);
		forms = new bitObject(rs,16);
		winnerForms = new bitObject(rs,16);
		patterns = new bitObject(rs,32);
		object = rs.readInteger();
		principalWherePosition = rs.readInteger();
		principalWhereAdjectivalPosition = rs.readInteger();
		originalObject = rs.readInteger();
		count = rs.readInteger();
		objectMatches = new cOM[count];
		for (int I = 0; I < count; I++)
			objectMatches[I] = new cOM(rs);
		count = rs.readInteger();
		audienceObjectMatches = new cOM[count];
		for (int I = 0; I < count; I++)
			audienceObjectMatches[I] = new cOM(rs);
		quoteForwardLink = rs.readInteger();
		endQuote = rs.readInteger();
		nextQuote = rs.readInteger();
		previousQuote = rs.readInteger();
		relObject = rs.readInteger();
		relSubject = rs.readInteger();
		relVerb = rs.readInteger();
		relPrep = rs.readInteger();
		beginObjectPosition = rs.readInteger();
		endObjectPosition = rs.readInteger();
		tmpWinnerForms = rs.readInteger();
		embeddedStorySpeakerPosition = rs.readInteger();
		long traceFlags = rs.readLong();
		collectPerSentenceStats = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceTagSetCollection = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceIncludesPEMAIndex = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceUnmatchedSentences = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceMatchedSentences = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceSecondaryPEMACosting = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		tracePatternElimination = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceBNCPreferences = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceDeterminer = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceVerbObjects = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceObjectResolution = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceNameResolution = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceSpeakerResolution = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceRelations = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceAnaphors = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceEVALObjects = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceAgreement = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		traceRole = (traceFlags & 1) == 1;
		traceFlags >>= 1;
		printBeforeElimination = (traceFlags & 1) == 1;
		logCache = rs.readShort();
		skipResponse = -1;
	  relNextObject = rs.readInteger();
	  nextCompoundPartObject = rs.readInteger();
	  previousCompoundPartObject = rs.readInteger();
	  relInternalVerb = rs.readInteger();
	  relInternalObject = rs.readInteger();
	  quoteForwardLink = rs.readInteger();
	  quoteBackLink = rs.readInteger();
	  speakerPosition = rs.readInteger();
	  audiencePosition = rs.readInteger();
		spaceRelation = false;
		andChainType = false;
		notFreePrep = false;
		hasVerbRelations = false;
	}

	public tFI getWordtFI() {
		tFI tfi=WordClass.Words.get(word);
		//if (tfi==null)
		//	System.out.println("Word "+word+" not found!");
		return tfi;
	}

	public int queryWordForm(int form) {
		tFI tfi=getWordtFI();
		if (tfi==null) return -1;
		for (int f = 0; f < tfi.forms.length; f++)
			if (tfi.forms[f] == form)
				return f;
		return -1;
	}

	public int queryForm(int form) {
		if (Form.forms==null)
			return -1;
		if ((flags & flagAddProperNoun) != 0 && form == Source.PROPER_NOUN_FORM_NUM)
		{
			tFI tfi=getWordtFI();
			if (tfi==null) return -1;
			return tfi.forms.length;
		}
		if ((flags & flagOnlyConsiderProperNounForms) != 0) {
			if (form >= Form.forms.length)
				if (form != Source.PROPER_NOUN_FORM_NUM && !Form.forms[form].properNounSubClass)
					return -1;
		} else if (form == Source.PROPER_NOUN_FORM_NUM && ((flags & flagFirstLetterCapitalized) == 0 || (flags & flagRefuseProperNoun) != 0))
			return -1;
		return queryWordForm(form);
	}

	boolean isWinner(int form) {
		return (tmpWinnerForms > 0) ? ((1 << form) & tmpWinnerForms) != 0 : true;
	}

	boolean hasWinnerVerbForm()
	{
		return getWordtFI().hasWinnerVerbForm(tmpWinnerForms);
	}

	public int queryWinnerForm(int form) {
		int picked;
		if (getWordtFI()==null || getWordtFI().forms==null)
			return -1;
		if ((flags & flagAddProperNoun) != 0 && form == Source.PROPER_NOUN_FORM_NUM)
			picked = getWordtFI().forms.length;
		else if ((flags & flagOnlyConsiderProperNounForms) != 0) {
			try {
				if (form != Source.PROPER_NOUN_FORM_NUM && !Form.forms[form].properNounSubClass)
					return -1;
			} catch (Exception e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			picked = queryWordForm(form);
		} else
			picked = queryWordForm(form);
		if (picked >= 0 && isWinner(picked))
			return picked;
		return -1;
	}

	long roles[] = { CObject.SUBOBJECT_ROLE, CObject.SUBJECT_ROLE, CObject.OBJECT_ROLE, CObject.META_NAME_EQUIVALENCE, CObject.MPLURAL_ROLE,
			CObject.HAIL_ROLE, CObject.IOBJECT_ROLE, CObject.PREP_OBJECT_ROLE, CObject.RE_OBJECT_ROLE, CObject.IS_OBJECT_ROLE,
			CObject.NOT_OBJECT_ROLE, CObject.NONPAST_OBJECT_ROLE, CObject.ID_SENTENCE_TYPE, CObject.NO_ALT_RES_SPEAKER_ROLE,
			CObject.IS_ADJ_OBJECT_ROLE, CObject.NONPRESENT_OBJECT_ROLE, CObject.PLACE_OBJECT_ROLE, CObject.MOVEMENT_PREP_OBJECT_ROLE,
			CObject.NON_MOVEMENT_PREP_OBJECT_ROLE, CObject.SUBJECT_PLEONASTIC_ROLE, CObject.IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE,
			CObject.UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE, CObject.SENTENCE_IN_REL_ROLE, CObject.PASSIVE_SUBJECT_ROLE, CObject.POV_OBJECT_ROLE,
			CObject.MNOUN_ROLE, CObject.PRIMARY_SPEAKER_ROLE, CObject.SECONDARY_SPEAKER_ROLE, CObject.FOCUS_EVALUATED, CObject.ID_SENTENCE_TYPE,
			CObject.DELAYED_RECEIVER_ROLE, CObject.IN_PRIMARY_QUOTE_ROLE, CObject.IN_SECONDARY_QUOTE_ROLE, CObject.IN_EMBEDDED_STORY_OBJECT_ROLE,
			CObject.EXTENDED_OBJECT_ROLE, CObject.NOT_ENCLOSING_ROLE, CObject.EXTENDED_ENCLOSING_ROLE, CObject.NONPAST_ENCLOSING_ROLE,
			CObject.NONPRESENT_ENCLOSING_ROLE, CObject.POSSIBLE_ENCLOSING_ROLE, CObject.THINK_ENCLOSING_ROLE };
	String r_c[] = { "SUBOBJ", "SUBJ", "OBJ", "META_EQUIV", "MP", "H", "IOBJECT", "PREP", "RE", "IS", "NOT", "NONPAST", "ID",
			"NO_ALT_RES_SPEAKER", "IS_ADJ", "NONPRESENT", "PL", "MOVE", "NON_MOVE", "PLEO", "INQ_SELF_REF_SPEAKER", "UNRES_FROM_IMPLICIT",
			"S_IN_REL", "PASS_SUBJ", "POV", "MNOUN", "SP", "SECONDARY_SP", "EVAL", "ID", "DELAY", "PRIM", "SECOND", "EMBED", "EXT", "NOT_ENC",
			"EXT_ENC", "NPAST_ENC", "NPRES_ENC", "POSS_ENC", "THINK_ENC" };

	public String roleString() {
		String role = "";
		int I = 0;
		for (String r : r_c)
			if ((objectRole & roles[I++]) != 0)
				role += r + ",";
		if (role.length()>0)
			role=role.substring(0, role.length()-1);
		return role;
	}

	public String relations() {
		String retMessage = "";
		if (relSubject!=-1) retMessage+="rSubj=" + relSubject;
		if (relVerb!=-1) retMessage+=" rVerb=" + relVerb;
		if (relObject!=-1) retMessage+=" rObj=" + relObject;
		if (relPrep!=-1) retMessage+=" rPrep=" + relPrep;
		if (relInternalVerb!=-1) retMessage+=" rInternalVerb=" + relInternalVerb;
		if (relNextObject!=-1) retMessage+=" rNextObject=" + relNextObject;
		if (nextCompoundPartObject!=-1) retMessage+=" nextCompoundPartObject=" + nextCompoundPartObject;
		if (previousCompoundPartObject!=-1) retMessage+=" previousCompoundPartObject=" + previousCompoundPartObject;
		if (relInternalObject!=-1) retMessage+=" rInternalObject=" + relInternalObject;
		if (nextQuote!=-1) retMessage+=" nextQuote=" + nextQuote;
		if (previousQuote!=-1) retMessage+=" previousQuote=" + previousQuote;
		if (retMessage.length()>0 && retMessage.charAt(0)==' ')
			retMessage=retMessage.substring(1);
		return retMessage;
	}

	public String getWinnerForms()
	{
		String sForms="(";
		for (int I=0; I<Form.forms.length; I++)
			if (forms.isSet(I))
			{
				if (queryWinnerForm(I)>=0) sForms+="*";
				sForms+=Form.forms[I].shortName+" ";
			}
		return sForms.trim()+") ";
	}
	
	public String printFlags()
	{
		String str="";
		if ((flags&flagInQuestion)!=0) str+=" Q";
		if ((flags&flagFirstLetterCapitalized)!=0) str+=" C";
		if ((flags&flagUsedPossessionRelation)!=0) str+=" UsedPossessionRelation"; 
		if ((flags&flagUsedBeRelation)!=0) str+=" UsedBeRelation";
		if ((flags&flagInLingeringStatement)!=0) str+=" InLingeringStatement";
		if ((flags&flagRelativeObject)!=0) str+=" RelativeObject";
		if ((flags&flagInInfinitivePhrase)!=0) str+=" InInfinitivePhrase";
		if ((flags&flagResolvedByRecent)!=0) str+=" ResolvedByRecent";
		if ((flags&flagLastContinuousQuote)!=0) str+=" LastContinuousQuote";
		if ((flags&flagAlternateResolveForwardFromLastSubjectAudience)!=0) str+=" AlternateResolveForwardFromLastSubjectAudience";
		if ((flags&flagAlternateResolveForwardFromLastSubjectSpeakers)!=0) str+=" AlternateResolveForwardFromLastSubjectSpeakers";
		if ((flags&flagGenderIsAmbiguousResolveAudience)!=0) str+=" GenderIsAmbiguousResolveAudience"; // when a
		// definitely specified audience is a gendered pronoun with more than one
		// speaker in the current speaker group having that gender
		if ((flags&flagRelativeHead)!=0) str+=" RelativeHead";
		if ((flags&flagQuoteContainsSpeaker)!=0) str+=" QuoteContainsSpeaker";
		if ((flags&flagResolveMetaGroupByGender)!=0) str+=" ResolveMetaGroupByGender";
		if ((flags&flagEmbeddedStoryResolveSpeakersGap)!=0) str+=" EmbeddedStoryResolveSpeakersGap";
		if ((flags&flagEmbeddedStoryBeginResolveSpeakers)!=0) str+=" EmbeddedStoryBeginResolveSpeakers"; // only set if
		// aged
		if ((flags&flagAge)!=0) str+=""; // only set if aged
		// set dynamically?
		// if ((flags&flagWNNotPhysicalObject=(1<<37";
		if ((flags&flagWNPhysicalObject)!=0) str+=" WNPhysical";
		if ((flags&flagInPStatement)!=0) str+=" InPStatement";
		if ((flags&flagRelationsAlreadyEvaluated)!=0) str+=" RelationsAlreadyEvaluated";
		if ((flags&flagNounOwner)!=0) str+=" NounOwner";
		// form flags
		if ((flags&flagAddProperNoun)!=0) str+=" AddPN";
		if ((flags&flagOnlyConsiderProperNounForms)!=0) str+=" OnlyPN";
		if ((flags&flagAllCaps)!=0) str+=" AllCaps";
		if ((flags&flagTopLevelPattern)!=0) str+="";
		if ((flags&flagPossiblePluralNounOwner)!=0) str+=" PossiblePluralNounOwner";
		if ((flags&flagNotMatched)!=0) str+=" NotMatched";
		if ((flags&flagInsertedQuote)!=0) str+=" InsertedQuote";
		if ((flags&flagRefuseProperNoun)!=0) str+=" RefuseProperNoun";
		if ((flags&flagMetaData)!=0) str+=" MetaData";
		if ((flags&flagOnlyConsiderOtherNounForms)!=0) str+=" OnlyConsiderOtherNounForms";
		// BNC patternPreferences (only used for nonQuotes)
		if ((flags&flagBNCPreferNounPatternMatch)!=0) str+=" BNCPreferNounPatternMatch";
		if ((flags&flagBNCPreferVerbPatternMatch)!=0) str+=" BNCPreferVerbPatternMatch";
		if ((flags&flagBNCPreferAdjectivePatternMatch)!=0) str+=" BNCPreferAdjectivePatternMatch";
		if ((flags&flagBNCPreferAdverbPatternMatch)!=0) str+=" BNCPreferAdverbPatternMatch";
		if ((flags&flagBNCPreferIgnore)!=0) str+=" BNCPreferIgnore";
		if ((flags&flagBNCFormNotCertain)!=0) str+=" BNCFormNotCertain"; // tag was not set on word
		// (because a tag was set for
		// multiple words)

		// speaker resolution (only used for quotes)
		if (word=="\"")
		{
			if ((flags&flagFromPreviousHailResolveSpeakers)!=0) str+=" FromPreviousHailResolveSpeakers";
			if ((flags&flagAlphaBeforeHint)!=0) str+=" AlphaBeforeHint";
			if ((flags&flagAlphaAfterHint)!=0) str+=" AlphaAfterHint";
			if ((flags&flagAlternateResolutionFinishedSpeakers)!=0) str+=" AlternateResolutionFinishedSpeakers";
			if ((flags&flagForwardLinkResolveAudience)!=0) str+=" ForwardLinkResolveAudience";
			if ((flags&flagForwardLinkResolveSpeakers)!=0) str+=" ForwardLinkResolveSpeakers";
			if ((flags&flagFromLastDefiniteResolveAudience)!=0) str+=" FromLastDefiniteResolveAudience";
			if ((flags&flagDefiniteResolveSpeakers)!=0) str+=" DefiniteResolveSpeakers";
			if ((flags&flagMostLikelyResolveAudience)!=0) str+=" MostLikelyResolveAudience";
			if ((flags&flagMostLikelyResolveSpeakers)!=0) str+=" MostLikelyResolveSpeakers";
			if ((flags&flagSpecifiedResolveAudience)!=0) str+=" SpecifiedResolveAudience";
			if ((flags&flagAudienceFromSpeakerGroupResolveAudience)!=0) str+=" AudienceFromSpeakerGroupResolveAudience";
			if ((flags&flagHailedResolveAudience)!=0) str+=" HailedResolveAudience";
			// if one speaker is known (flagDefiniteResolveSpeakers) then resolve others
			// around it by assuming the speakers alternate
			if ((flags&flagAlternateResolveBackwardFromDefiniteAudience)!=0) str+=" AlternateResolveBackwardFromDefiniteAudience"; // resolves
			// unresolved speakers backwards from a known definite speaker
			if ((flags&flagAlternateResolveBackwardFromDefiniteSpeakers)!=0) str+=" AlternateResolveBackwardFromDefiniteSpeakers"; // resolves
			// unresolved speakers backwards from a	known definite speaker
			if ((flags&flagForwardLinkAlternateResolveAudience)!=0) str+=" ForwardLinkAlternateResolveAudience";
			if ((flags&flagForwardLinkAlternateResolveSpeakers)!=0) str+=" ForwardLinkAlternateResolveSpeakers";
			if ((flags&flagEmbeddedStoryResolveSpeakers)!=0) str+=" EmbeddedStoryResolveSpeakers";
			if ((flags&flagAlternateResolveForwardAfterDefinite)!=0) str+=" AlternateResolveForwardAfterDefinite"; // this flag
			// is set for all alternative settings forward after a definite
			// speaker previously results from imposeMostLikely were only set with
			// flagMostLikelyResolveSpeakers flagAlternateResolveForwardAfterDefinite allows an alternate speaker
			// imposed from below and an alternate speaker
			// imposed from above to stop running over each other
			// 1<<4 (flagObjectResolved) is zeroed for all positions after
			// identifySpeakerGroups
			if ((flags&flagGenderIsAmbiguousResolveSpeakers)!=0) str+=" GenderIsAmbiguousResolveSpeakers"; // when a
			// definitely specified speaker is a gendered pronoun with more than one
			// speaker in the current speaker group having that gender
			if ((flags&flagQuotedString)!=0) str+=" QuotedString";
			if ((flags&flagSecondEmbeddedStory)!=0) str+=" SecondEmbeddedStory";
			if ((flags&flagFirstEmbeddedStory)!=0) str+=" FirstEmbeddedStory";
		}
		// only for non-quotes
		if ((flags&flagObjectResolved)!=0) str+="";
		if ((flags&flagAdjectivalObject)!=0) str+=" AdjectivalObject";
		if ((flags&flagUnresolvableObjectResolvedThroughSpeakerGroup)!=0) str+=" UnresolvableObjectResolvedThroughSpeakerGroup";
		if ((flags&flagObjectPleonastic)!=0) str+=" Pleonastic";
		if ((flags&flagIgnoreAsSpeaker)!=0) str+=" IgnoreAsSpeaker";

		return str;
	}
	
}

