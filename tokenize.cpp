#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "source.h"
#include "profile.h"

bool Source::adjustWord(unsigned int q)
{ LFS
	bool insertedOrDeletedWord=false;
	// let's have some dinner!
	if ((m[q].flags&WordMatch::flagNounOwner) && m[q].word->first==L"let")
	{
		m[q].flags&=~WordMatch::flagNounOwner;
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"us"),0,debugTrace));
		m[q+1].forms.set(accForm);
		insertedOrDeletedWord=true;
	}
	// d'ye -> do you
	else if (m[q].word->first == L"d'ye" || m[q].word->first == L"d'you")
	{
		m[q].word = Words.gquery(L"do");
		m[q].forms.clear();
		m[q].forms.set(FormsClass::gFindForm(L"does"));
		m[q].flags = 0;
		m.insert(m.begin() + q, WordMatch(Words.gquery(L"you"), 0, debugTrace));
		m[q + 1].forms.set(FormsClass::gFindForm(L"personal_pronoun"));
		insertedOrDeletedWord = true;
	}
	// t'other -> the other
	else if (m[q].word->first == L"t'other")
	{
		m[q].word = Words.gquery(L"the");
		m[q].forms.clear();
		m[q].forms.set(FormsClass::gFindForm(L"determiner"));
		m[q].flags = 0;
		m.insert(m.begin() + q, WordMatch(Words.gquery(L"other"), 0, debugTrace));
		m[q + 1].forms.set(FormsClass::gFindForm(L"pronoun"));
		insertedOrDeletedWord = true;
	}
	// more'n -> more than
	else if (m[q].word->first == L"more'n")
	{
		m[q].word = Words.gquery(L"more");
		m[q].forms.clear();
		m[q].forms.set(FormsClass::gFindForm(L"adverb"));
		m[q].flags = 0;
		m.insert(m.begin() + q, WordMatch(Words.gquery(L"than"), 0, debugTrace));
		m[q + 1].forms.set(FormsClass::gFindForm(L"conjunction"));
		m[q + 1].forms.set(FormsClass::gFindForm(L"preposition"));
		insertedOrDeletedWord = true;
	}
	// What's he want? --> What does he want?
	// What's for dinner? --> What is for dinner?
	// What's that got to do with it? --> What has that got to do with it?
	else if ((m[q].flags&WordMatch::flagNounOwner) && (m[q].word->first==L"what"))
	{
		m[q].flags&=~WordMatch::flagNounOwner;
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"ishasdoes"),0,debugTrace));
		m[q+1].setPreferredForm();
		insertedOrDeletedWord=true;
	}
	else if ((m[q].flags&WordMatch::flagNounOwner) && (m[q].word->first==L"that"))
	{
		m[q].flags&=~WordMatch::flagNounOwner;
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"is"),0,debugTrace));
		m[q+1].setPreferredForm();
		insertedOrDeletedWord=true;
	}
	// twas -> it was
	else if (WordClass::isSingleQuote(m[q].word->first[0]) && q + 1 < m.size() && m[q + 1].word->first == L"twas")
	{
		m[q].word = Words.gquery(L"it");
		m[q].forms.clear();
		m[q].forms.set(personalPronounForm);
		m[q].flags = 0;
		m[q + 1].word = Words.gquery(L"was");
		m[q + 1].forms.clear();
		m[q + 1].forms.set(FormsClass::gFindForm(L"is"));
		m[q + 1].flags = 0;
	}
	// 'Tis the season --> It is the season
	else if (WordClass::isSingleQuote(m[q].word->first[0]) && q+1<m.size() && m[q+1].word->first==L"tis")
	{
		m[q].word=Words.gquery(L"it");
		m[q].forms.clear();
		m[q].forms.set(personalPronounForm);
		m[q].flags=0;
		m[q+1].word=Words.gquery(L"is");
		m[q+1].forms.clear();
		m[q+1].forms.set(FormsClass::gFindForm(L"is"));
		m[q+1].flags=0;
	}
	else if (m[q].word->first == L"gotta")
	{
		// Gotta penny?                   have a
		// You gotta lot of nerve...      have a
		// I gotta go now.                have to
		m[q].word = Words.gquery(L"have");
		m[q].flags = 0;
		m[q].forms.clear();
		m[q].setPreferredForm();
		if (m[q + 1].queryForm(verbForm) < 0)
		{
			m.insert(m.begin() + q + 1, WordMatch(Words.gquery(L"a"), 0, debugTrace));
			m[q + 1].forms.set(determinerForm);
		}
		else
		{
			m.insert(m.begin() + q + 1, WordMatch(Words.gquery(L"to"), 0, debugTrace));
			m[q + 1].forms.set(toForm);
		}
		insertedOrDeletedWord = true;
	}
	else if (m[q].word->first == L"dinna")
	{
		m[q].word = Words.gquery(L"didn't");
		m[q].flags = 0;
		m[q].forms.clear();
		m[q].forms.set(doForm);
		m.insert(m.begin() + q + 1, WordMatch(Words.gquery(L"you"), 0, debugTrace));
		m[q + 1].forms.set(FormsClass::gFindForm(L"personal_pronoun"));
		insertedOrDeletedWord = true;
	}
	// I lived at 23 Beek St.  That was a nice block.
	else if (q>0 && m[q].queryForm(sa_abbForm)>=0 &&
		(m[q-1].flags&WordMatch::flagFirstLetterCapitalized) &&
		(m[q+1].flags&WordMatch::flagFirstLetterCapitalized))
	{
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"."),0,debugTrace));
		insertedOrDeletedWord=true;
	}
	// 2:30 A.M.
	else if (q>0 && (m[q].word->first==L"a.m." || m[q].word->first==L"p.m.") && (m[q-1].queryForm(timeForm)>=0 || m[q-1].queryForm(NUMBER_FORM_NUM)>=0) &&
		(m[q+1].flags&WordMatch::flagFirstLetterCapitalized) && m[q+1].queryForm(L"daysOfWeek")<0)
	{
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"."),0,debugTrace));
		insertedOrDeletedWord=true;
	}
	// 2390 B.C.
	else if (q>0 && (m[q].word->first==L"a.d." || m[q].word->first==L"b.c.") && m[q-1].queryForm(numberForm)>=0 &&
		(m[q+1].flags&WordMatch::flagFirstLetterCapitalized))
	{
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"."),0,debugTrace));
		insertedOrDeletedWord=true;
	}
	else if (q>0 && m[q].word->first==L"no." &&
		m[q+1].queryForm(numberForm)<0 && m[q+1].queryForm(numeralCardinalForm)<0 && m[q+1].queryForm(romanNumeralForm)<0 &&
		(m[q+1].flags&WordMatch::flagFirstLetterCapitalized))
	{
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"."),0,debugTrace));
		insertedOrDeletedWord=true;
	}
	else if (m[q].word->first==L"had" && m[q+1].word->first==L"better" && m[q+2].queryForm(verbForm)>=0)
	{
		// I had better leave now.
		m[q].word=Words.gquery(L"have");
		m[q].flags=0;
		m[q].forms.clear();
		m[q].setPreferredForm();
		m[q+1].word=Words.gquery(L"to");
		m[q+1].flags=0;
		m[q+1].forms.clear();
		m[q+1].forms.set(toForm);
	}
	else if (m[q].word->first==L"wanna")
	{
		// I wanna leave now.
		m[q].word=Words.gquery(L"want");
		m[q].flags=0;
		m[q].forms.clear();
		m[q].setPreferredForm();
		m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"to"),0,debugTrace));
		insertedOrDeletedWord=true;
		m[q+1].forms.set(toForm);
	}
	return insertedOrDeletedWord;
}

bool Source::quoteTest(int q,unsigned int &quoteCount,int &lastPSQuote,tIWMM quoteType,tIWMM quoteOpenType,tIWMM quoteCloseType)
{ LFS
  if (m[q].word!=quoteType) return false;
  quoteCount++;
	lastPSQuote =q;
  if (quoteCount&1)
    m[q].word=quoteOpenType;
  else
    m[q].word=quoteCloseType;
#ifdef LOG_QUOTATIONS
  lplog(L"Detected %s at %d increased to %d.",kind,q,quoteCount);
  displayQuoteContext(q-3,q+3);
#endif
  return true;
}

void Source::secondaryQuoteTest(int q,unsigned int &secondaryQuotations,int &lastSecondaryQuote,
																tIWMM secondaryQuoteWord,tIWMM secondaryQuoteOpenWord,tIWMM secondaryQuoteCloseWord)
{ LFS
	// if space before quote, it should be an open quote.  if space after quote, it should be a close quote.
	// if two open quotes follow each other, delete the first open quote.
	bool preferCloseQuoteBySpace=(m[q].flags&WordMatch::flagAlphaBeforeHint) && !(m[q].flags&WordMatch::flagAlphaAfterHint);
	bool preferOpenQuoteBySpace=!(m[q].flags&WordMatch::flagAlphaBeforeHint) && (m[q].flags&WordMatch::flagAlphaAfterHint);
	bool preferOpenQuoteByCount=!(secondaryQuotations&1);
	bool preferCloseQuoteByCount=(secondaryQuotations&1);
	if (preferOpenQuoteBySpace && q+1<(signed)m.size() && 
			// is the following word unknown? (suggests an abbreviated word)
		  ((m[q+1].queryForm(UNDEFINED_FORM_NUM)>=0) ||
			// is the previous open quote followed by a more common word?
			 (preferCloseQuoteByCount && 
			  getHighestFamiliarity(m[q+1].word->first)<getHighestFamiliarity(m[lastSecondaryQuote+1].word->first))))
	{
		#ifdef LOG_QUOTATIONS
			lplog(L"%d-%d:Quoted abbreviation detected at %d.  Skipping.",q-5,q+5,q);
			displayQuoteContext(q-5,q+5);
		#endif
	}
	else if (preferOpenQuoteBySpace && q+1<(signed)m.size() && 
				// is the previous open quote followed by a LESS common word?
				preferCloseQuoteByCount && 
			  getHighestFamiliarity(m[q+1].word->first)>getHighestFamiliarity(m[lastSecondaryQuote+1].word->first))
	{
		// change lastSecondaryQuote to a normal quote.
		m[lastSecondaryQuote].word=secondaryQuoteWord;
		#ifdef LOG_QUOTATIONS
			lplog(L"%d-%d:Quoted abbreviation detected at %d (2).  Skipping.",lastSecondaryQuote-5,lastSecondaryQuote+5,lastSecondaryQuote);
			displayQuoteContext(lastSecondaryQuote-5,lastSecondaryQuote+5);
		#endif
		lastSecondaryQuote=q;
		m[q].word=secondaryQuoteOpenWord;
	}
	else
	{
		if ((preferOpenQuoteBySpace && preferCloseQuoteByCount) || (preferCloseQuoteBySpace && preferOpenQuoteByCount))
		{
			#ifdef LOG_QUOTATIONS
				lplog(L"%d-%d:Secondary quotations open/close quote clash detected at %d.  Ignoring preferences.",q-5,q+5,q);
				displayQuoteContext(q-5,q+5);
			#endif
			preferOpenQuoteBySpace=preferCloseQuoteBySpace=false;
		}
		if (preferCloseQuoteBySpace ^ preferOpenQuoteBySpace)
			m[q].word=(preferCloseQuoteBySpace) ? secondaryQuoteCloseWord : secondaryQuoteOpenWord;
		else
			m[q].word=(preferCloseQuoteByCount) ? secondaryQuoteCloseWord : secondaryQuoteOpenWord;
		secondaryQuotations++;
		lastSecondaryQuote=q;
		#ifdef LOG_QUOTATIONS
			lplog(L"Detected %s at %d increased to %d.",L"secondaryQuotations",q,secondaryQuotations);
			displayQuoteContext(q-3,q+3);
		#endif
	}
}

void Source::eraseLastQuote(int &lastPSQuote,tIWMM quoteCloseWord,unsigned int &q)
{ LFS
	while (lastPSQuote>0 && m[lastPSQuote].word!=quoteCloseWord) lastPSQuote--;
	m.erase(m.begin()+lastPSQuote);
	q--; // erased word before this
	m[q-1].word=quoteCloseWord; // erase previous close quote and make this location the close quote
	for (unsigned int s2=0; s2<sentenceStarts.size(); s2++)
	{
		if (sentenceStarts[s2]>lastPSQuote)
			sentenceStarts[s2]--;
		if (sentenceStarts[s2]==lastPSQuote && m[sentenceStarts[s2]].word==Words.sectionWord)
			sentenceStarts[s2]++;// corrects the setting because of a dangling quotation that has now been erased
	}
	unordered_map <unsigned int, wstring> newMetaCommandsEmbeddedInSource;
	for (auto const&[where, comment] : metaCommandsEmbeddedInSource)
		newMetaCommandsEmbeddedInSource[(where < q) ? where : where-1] = comment;
	metaCommandsEmbeddedInSource = newMetaCommandsEmbeddedInSource;
	lastPSQuote=q-1;
}

bool Source::getFormFlags(int where, bool &maybeVerb, bool &maybeNoun, bool &maybeAdjective, bool &preferNoun)
{
	int verbFormOffset= m[where].queryForm(verbForm),nounFormOffset,adjectiveFormOffset;
	// does this position have any verb form other than verbForm itself (then check if it does have verbForm, that it is a probable verbForm)
	maybeVerb = (m[where].word->second.hasVerbForm() && verbFormOffset<0) || (verbFormOffset >= 0 && m[where].word->second.getUsageCost(verbFormOffset) < 4);
	if ((m[where].flags&WordMatch::flagFirstLetterCapitalized) && !(m[where].flags&WordMatch::flagAllCaps))
		maybeVerb = false;
	maybeNoun = (nounFormOffset = m[where].queryForm(nounForm)) >= 0 && m[where].word->second.getUsageCost(nounFormOffset) < 4;
	maybeAdjective = (adjectiveFormOffset = m[where].queryForm(adjectiveForm)) >= 0 && m[where].word->second.getUsageCost(adjectiveFormOffset) < 4;
	preferNoun = maybeNoun && (adjectiveFormOffset<0 || ((unsigned)m[where].word->second.getUsagePattern(nounFormOffset))>((unsigned)m[where].word->second.getUsagePattern(adjectiveFormOffset)));
	if (debugTrace.traceParseInfo)
		lplog(LOG_INFO, L"%s %s %s %s %s nounOffset=%d adjectiveOffset=%d noun count=%u adjective count=%u", m[where].word->first.c_str(), (maybeVerb) ? L"verb" : L"", (maybeNoun) ? L"noun" : L"", (maybeAdjective) ? L"adjective" : L"",
			(preferNoun) ? L"preferNoun":L"",nounFormOffset, adjectiveFormOffset, (unsigned char)m[where].word->second.getUsagePattern(nounFormOffset), (unsigned char)m[where].word->second.getUsagePattern(adjectiveFormOffset));
	return false;
}

// executed after tokenization but before pattern matching.
// also handle Let's = Let us and What's he want = What does he want?
// also handle 'Tis as in 'Tis the time for all good men... (It is)
unsigned int Source::doQuotesOwnershipAndContractions(unsigned int &primaryQuotations,bool newsBank)
{ LFS
	unsigned int quotationExceptions=0;
	unsigned int secondaryQuotations=0;
	int lastPrimaryQuote=-1,lastSecondaryQuote=-1;
	tIWMM primaryQuoteWord=Words.gquery(primaryQuoteType);
	tIWMM secondaryQuoteWord=Words.gquery(secondaryQuoteType);
	tIWMM primaryQuoteOpenWord=Words.gquery(L"“"),primaryQuoteCloseWord=Words.gquery(L"”");
	tIWMM secondaryQuoteOpenWord=Words.gquery(L"‘"),secondaryQuoteCloseWord=Words.gquery(L"’");
	unsigned int lastWord=m.size();
	primaryQuotations=0;
	wstring originalWord;
	// scan for only single quotations - convert if necessary
	// rearrange quotes, also figure out plural ownership and more on would/had is/has
	for (unsigned int s=0; s+1<sentenceStarts.size(); s++)
	{
		unsigned int begin=sentenceStarts[s];
		unsigned int end=sentenceStarts[s+1];
		bool endOfParagraph=false;
		while (end && m[end-1].word==Words.sectionWord)
		{
			end--; // cut off end of paragraphs
			endOfParagraph=true;
		}
		if (begin>=end)
		{
			sentenceStarts.erase(s--);
			continue;
		}
		unsigned int sectionEnd;
		if (!newsBank && isSectionHeader(begin,end,sectionEnd))
			if (end!=sectionEnd) sentenceStarts.insert(s+1,sectionEnd);
		for (unsigned int q=begin; q<end; q++)
		{
			bool afterPossibleAbbreviation=(q>1 && m[q-1].word->first==L"." && (m[q-2].queryForm(abbreviationForm)>=0 || m[q-2].queryForm(honorificAbbreviationForm)>=0 || m[q-2].queryForm(letterForm)>=0));
			// logic copied from Source::parse
			tIWMM w=(q) ? m[q-1].word : wNULL;
			bool firstWordInSentence=q==begin || w->first==L"." || // include "." because the last word may be a contraction
				w==Words.sectionWord ||	w->first==L"--" /* Ulysses */ || w->first==L"?" || w->first==L"!" || 
				w->first==L":" /* Secret Adversary - Second month:  Promoted to drying aforesaid plates.*/||
				w->first==L";" /* I am a Soldier A jolly British Soldier ; You can see that I am a Soldier by my feet . . . */||
				w->second.query(quoteForm)>=0 || w->second.query(dashForm)>=0 || w->second.query(bracketForm)>=0 ||
				(q-3==begin && m[q-2].word->second.query(quoteForm)>=0 && w->first==L"(") || // Pay must be good.' (We might as well make that clear from the start.)
				(q-2==begin && w->first==L"(");   // The bag dropped.  (If you didn't know).
			// if the first word, and there are no other usages of it not being the first word but capitalized,
			// and there are usages of it not being capitalized, or it is NOT (unknown or a proper noun)
			// OR
			// if first word, and all other usages of the word are that it is accompanied by another following word if it is capitalized (numProperNounUsageAsAdjective),
			// and it is not accompanied by another following capitalized word, and it is not (unknown or a proper noun). ('New' York)
			// ADDITIONALLY, 
			// [mightBeName] if (the previous word is a .) AND (the word before that is an honorific and capitalized) St. Pancras
			bool mightBeName=q>2 && m[q-1].word->first==L"." && 
				   (m[q-2].word->second.query(honorificForm)>=0 || m[q-2].word->second.query(honorificAbbreviationForm)>=0 || m[q-2].word->second.query(letterForm)>=0) && 
					 (m[q-2].flags&(WordMatch::flagFirstLetterCapitalized));
			// added q>0 because in abstracts, most of the other criteria is invalid (see Curveball - Rafid Ahmed Alwan al-Janabi, known by the Central Intelligence Agency cryptonym "Curveball")
			if (((firstWordInSentence && q>0) || m[q].flags&WordMatch::flagAllCaps) && !mightBeName &&
				  (m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN)==0 ||
				   (m[q].word->second.numProperNounUsageAsAdjective==m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN) &&
					  (q+1>=m.size() || (m[q+1].flags&(WordMatch::flagFirstLetterCapitalized|WordMatch::flagAllCaps))==0))) &&
					//m[q].word->second.relatedSubTypeObjects.size()==0 && // not a known common place 
					(q<1 || (m[q-1].word->first!=L"the" && !(m[q-1].flags&WordMatch::flagAllCaps))) && 
					!((m[q].flags&WordMatch::flagAllCaps) && m[q].word->second.formsSize()==0) && // The US / that IRS tax form
				  // must NOT be queryForm because of test
				  (m[q].word->second.getUsagePattern(tFI::LOWER_CASE_USAGE_PATTERN)>0 || m[q].word->second.query(PROPER_NOUN_FORM_NUM)<0))
			{
				// if flagAddProperNoun and first word, check if usage pattern is 0.  If it is, unflag word.
				if (m[q].flags&WordMatch::flagAddProperNoun)
				{
					m[q].flags&=~WordMatch::flagAddProperNoun;
					if (debugTrace.traceParseInfo)
						lplog(LOG_INFO, L"%d:%s:removed flagAddProperNoun asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.",
							q, getOriginalWord(q, originalWord, false), m[q].word->second.numProperNounUsageAsAdjective,
							m[q].word->second.getUsagePattern(tFI::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN),
							m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);
				}
				// refuse to make it proper noun, even if it is listed as one.  see WordMatch::queryForm(int form)
				else
					if ((m[q].flags&WordMatch::flagFirstLetterCapitalized) && !afterPossibleAbbreviation && !(m[q].flags&WordMatch::flagAllCaps) && m[q].word->second.localWordIsLowercase > 0)
					{
						m[q].flags|=WordMatch::flagRefuseProperNoun;
						if (debugTrace.traceParseInfo)
							lplog(LOG_INFO, L"%d:%s:added flagRefuseProperNoun asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.", 
								q, getOriginalWord(q, originalWord,false),m[q].word->second.numProperNounUsageAsAdjective,
								m[q].word->second.getUsagePattern(tFI::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN),
								m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);
					}
			}
			if (firstWordInSentence && (m[q].flags&WordMatch::flagFirstLetterCapitalized) && !(m[q].flags&WordMatch::flagAddProperNoun) && !(m[q].flags & WordMatch::flagRefuseProperNoun) &&
				m[q].word->second.query(PROPER_NOUN_FORM_NUM) < 0 && m[q].word->second.localWordIsLowercase == 0 && m[q].word->second.localWordIsCapitalized > 2)
			{
				m[q].flags |= WordMatch::flagAddProperNoun;
				if (debugTrace.traceParseInfo)
					lplog(LOG_INFO, L"%d:%s:added flagAddProperNoun (from local) asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.",
						q, getOriginalWord(q, originalWord, false), m[q].word->second.numProperNounUsageAsAdjective,
						m[q].word->second.getUsagePattern(tFI::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN),
						m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);

			}
			if (m[q].word->first == L"lord" && (m[q].flags&WordMatch::flagFirstLetterCapitalized) && !(m[q].flags&WordMatch::flagAddProperNoun) && q + 1 < m.size() && !(m[q + 1].flags&WordMatch::flagFirstLetterCapitalized))
			{
				m[q].flags |= WordMatch::flagAddProperNoun;
				if (debugTrace.traceParseInfo)
					lplog(LOG_INFO, L"%d:%s:added flagAddProperNoun (SPECIAL CASE lord) asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.",
						q, getOriginalWord(q, originalWord, false), m[q].word->second.numProperNounUsageAsAdjective,
						m[q].word->second.getUsagePattern(tFI::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN),
						m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);
			}
			// if capitalized, not all caps, at least 2 letters long, not a cardinal or ordinal number
			// does not already have flagRefuseProperNoun or flagAddProperNoun or flagOnlyConsiderProperNounForms or flagOnlyConsiderOtherNounForms set
			// is never used in the lower case, and has been used as a proper noun unambiguously
			if ((m[q].flags&WordMatch::flagFirstLetterCapitalized) && !(m[q].flags&WordMatch::flagAllCaps) && m[q].word->first[1] &&
				m[q].word->second.query(numeralOrdinalForm)==-1 && m[q].word->second.query(numeralCardinalForm)==-1 &&
				!(m[q].flags&(WordMatch::flagRefuseProperNoun|WordMatch::flagOnlyConsiderProperNounForms|WordMatch::flagOnlyConsiderOtherNounForms)) &&
				(m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN)>0 || m[q].word->second.localWordIsCapitalized>0) &&  // PROPER_NOUN_USAGE_PATTERN is only updated if proper noun form is added 
				m[q].word->second.getUsagePattern(tFI::LOWER_CASE_USAGE_PATTERN)==0 &&
				// word was found capitalized alone (the number of times being a capitalized adjective is < the number of times being capitalized)
				(m[q].word->second.numProperNounUsageAsAdjective<m[q].word->second.getUsagePattern(tFI::PROPER_NOUN_USAGE_PATTERN) ||
				// OR this particular position is followed by a capitalized word (Peel Edgerton) - peel is also a verb!
				 (q+1<m.size() && (m[q+1].flags&(WordMatch::flagFirstLetterCapitalized|WordMatch::flagAllCaps))!=0)))
			{
				// if word serves only as an adjective in a proper noun ('New' York), don't mark as only proper noun
				//lplog(L"%d:DEBUG PNU %d %d",q,m[q].word->second.numProperNounUsageAsAdjective,(int)m[q].word->second.usagePatterns[tFI::PROPER_NOUN_USAGE_PATTERN]);
				bool atLeastOneProperNounForm=(m[q].flags&WordMatch::flagAddProperNoun)!=0;
				for (unsigned int I=0; I<m[q].word->second.formsSize() && !atLeastOneProperNounForm; I++)
					if (m[q].word->second.Form(I)->properNounSubClass || m[q].word->second.forms()[I]==PROPER_NOUN_FORM_NUM)
						atLeastOneProperNounForm=true;
				if (atLeastOneProperNounForm)
				{
					m[q].flags|=WordMatch::flagOnlyConsiderProperNounForms;
					if (debugTrace.traceParseInfo)
						lplog(LOG_INFO,L"%d:%s:added flagOnlyConsiderProperNounForms (2).",q, getOriginalWord(q, originalWord, false));
				}
			}
			if ((m[q].flags&WordMatch::flagOnlyConsiderProperNounForms) && end-begin>4 && !m[q].word->second.isUnknown())
			{
				// check if from begin to end there is only capitalized words except for determiners or prepositions
				// What Do We Need to Know About the International Monetary System? (Paul Krugman)
				bool allCapitalized=true;
				int longestContinuousTerm=0,termLength=0,numCommonClassCapitalizedWords=0,numCapitalized=0;
				for (unsigned int si=begin; si<end; si++)
				{
					if (!iswalpha(m[si].word->first[0])) 
					{
						termLength=0;
						numCapitalized++;
						continue;
					}
					termLength++;
					longestContinuousTerm=max(longestContinuousTerm,termLength);
					bool isCapitalized=(m[si].queryForm(PROPER_NOUN_FORM_NUM)>=0 || (m[si].flags&WordMatch::flagFirstLetterCapitalized) || (m[si].flags&WordMatch::flagAllCaps));
					if (m[si].word->second.isCommonWord() && isCapitalized)
						numCommonClassCapitalizedWords++;
					if (!isCapitalized && m[si].queryForm(determinerForm)<0 && m[si].queryForm(prepositionForm)<0)
						allCapitalized=false;
					else
						numCapitalized++;
				}
				if ((allCapitalized && longestContinuousTerm>4 && numCommonClassCapitalizedWords>0) ||
						((end-begin)>(unsigned)numCapitalized && longestContinuousTerm>4 && (unsigned)numCommonClassCapitalizedWords>=((end-begin)-numCapitalized))) // take care of small errors / How Jay-Z Went from Street Corner to Corner Office by Zack O'Malley Greenburg (2011: Portfolio (Penguin), 240 pages) ISBN 978-1-59184-381-8
				{
					m[q].flags&=~WordMatch::flagOnlyConsiderProperNounForms;
					if (debugTrace.traceParseInfo)
						lplog(LOG_INFO,L"%d:%s:removed flagOnlyConsiderProperNounForms [allCapitalized longestContinuousTerm=%d numCommonClassCapitalizedWords=%d numCapitalized=%d totalLength=%d].",
									q, getOriginalWord(q, originalWord, false), longestContinuousTerm,numCommonClassCapitalizedWords,numCapitalized,end-begin);
				}
				else if (debugTrace.traceParseInfo)
						lplog(LOG_INFO,L"%d:%s:did not remove flagOnlyConsiderProperNounForms [allCapitalized=%s longestContinuousTerm=%d numCommonClassCapitalizedWords=%d numCapitalized=%d totalLength=%d].",
									q, getOriginalWord(q, originalWord, false), (allCapitalized) ? L"true":L"false",longestContinuousTerm,numCommonClassCapitalizedWords,numCapitalized,end-begin);
			}
			if (m[q].word==Words.sectionWord)
			{
				if (q && m[q-1].word==primaryQuoteOpenWord && (primaryQuotations&1))  // section found with an open quotation
				{
					primaryQuotations--;
#ifdef LOG_QUOTATIONS
					lplog(L"%d-%d:Primary quotations odd at end-of-section at %d (1).  Removing quote at %d.  Total primaryQuotations decreased to %d.",
						begin,end,q,lastPrimaryQuote,primaryQuotations);
					displayQuoteContext(begin-5,end+5);
#endif
					eraseLastQuote(lastPrimaryQuote,primaryQuoteCloseWord,q);
					end--;
					quotationExceptions++;
				}
				else if (q && m[q-1].word==secondaryQuoteOpenWord && (secondaryQuotations&1))
				{
					secondaryQuotations--;
#ifdef LOG_QUOTATIONS
					lplog(L"%d-%d:Secondary quotations odd at end-of-section at %d (3).  Removing quote at %d.  Total secondaryQuotations decreased to %d.",
						begin,end,q,lastSecondaryQuote,secondaryQuotations);
					displayQuoteContext(begin-5,end+5);
#endif
					eraseLastQuote(lastSecondaryQuote,secondaryQuoteCloseWord,q);
					end--;
					quotationExceptions++;
				}
				continue;
			}
			if (!quoteTest(q,primaryQuotations,lastPrimaryQuote,primaryQuoteWord,primaryQuoteOpenWord,primaryQuoteCloseWord))
			{
				if (m[q].word==secondaryQuoteWord) 
					secondaryQuoteTest(q,secondaryQuotations,lastSecondaryQuote,secondaryQuoteWord,secondaryQuoteOpenWord,secondaryQuoteCloseWord);
			}
			// do plural ownership
			if (m[q].flags&WordMatch::flagPossiblePluralNounOwner)
			{
				m[q].flags&=~WordMatch::flagPossiblePluralNounOwner;
				// only if the word is a plural noun type and their is no unresolved single quote
				if (((m[q].word->second.inflectionFlags&PLURAL) || m[q].word->first[m[q].word->first.length()-1]==L's') && !(secondaryQuotations&1))
				{
					m[q].flags|=WordMatch::flagNounOwner;
					if (m[q+1].word->first==L"\'")
					{
#ifdef LOG_QUOTATIONS
						lplog(L"%d-%d:flagPossiblePluralNounOwner deletes single quote at %d.",begin,end,q+1);
						displayQuoteContext(begin-5,end+5);
#endif
						m.erase(m.begin()+q+1);
						for (unsigned int s2=s+1; s2<sentenceStarts.size(); s2++)
							sentenceStarts[s2]--;
						unordered_map <unsigned int, wstring> newMetaCommandsEmbeddedInSource;
						for (auto const&[where, comment] : metaCommandsEmbeddedInSource)
							newMetaCommandsEmbeddedInSource[(where < q+1) ? where : where - 1] = comment;
						metaCommandsEmbeddedInSource = newMetaCommandsEmbeddedInSource;
						end--;
					}
				}
			}
			// do is/has
			// the other's more expensive.
			// current word ends in 's, is a noun type or that or one and the next word is not a noun type and is not punctuation and has another form besides a verb.
			// Dave's sure been right. Dave's now about 80?
			// Dave's beans are great.
			// a barn of Dave's.
			// no one's been there.
           // must be owner (have 's after it) 
			// Jay-Z's planning to marry Beyonce?
			else if ((m[q].flags&WordMatch::flagNounOwner) &&
				// must be single or have a determiner AFTER the word
				(!(m[q].word->second.inflectionFlags&PLURAL) || (q+1<lastWord && m[q+1].queryWinnerForm(determinerForm)>=0)) &&
				// must be a nounType, that, a pronoun (nominal (one, I, we) acc (him, them...), indefinite or quantifier 
				(m[q].isNounType() || m[q].word->first==L"that" || m[q].queryForm(nomForm)>=0 || m[q].queryForm(accForm)>=0 || m[q].queryForm(indefinitePronounForm)>=0 || m[q].queryForm(quantifierForm)>=0) &&
				// and NOT "let" (because of "let's")
				m[q].word->first!=L"let" &&
				(q+1<lastWord && m[q+1].word!=Words.sectionWord &&
				// the next word must be "been" or (NOT is and NOT punctuation).
				(m[q+1].word->first==L"been" || (m[q+1].queryForm(L"is")<0 && m[q+1].queryForm(L"is_negation")<0 && iswalpha(m[q+1].word->first[0])))) &&
				// the previous word must not be "not" / (not Whittington's one) and not "is"
				(!q || (m[q-1].word->first!=L"not" && m[q-1].queryForm(isForm)<0)))
			{
				// scan for immediately preceding preposition
				// from q to begin, scan for any word that preposition is the lowest cost.
				// if word is not determiner or Proper Noun or preposition break.
				int I=q-1;
				for (; I>=(int)begin && m[I].queryForm(PROPER_NOUN_FORM_NUM)>=0; I--);
				for (; I>=(int)begin && m[I].queryForm(determinerForm)>=0; I--);
				if (I>=0)
				{
					int numForm=m[I].queryForm(prepositionForm);
					if (numForm<0 || m[I].word->second.getUsageCost(numForm)>1)
					{
						// is the next word a noun?  If there is no noun, 's cannot be ownership.
						// if it IS a noun, it might mean ownership, or maybe not (further work?)
						I=q+1;
						for (; I<(int)lastWord && m[I].isModifierType() && !m[I].isNounType(); I++);
						// other's a cadillac
						// other's reform.
						if ((I<(int)lastWord && !m[I].isNounType()) || ((numForm=m[q].queryForm(relativizerForm))>=0 && m[q].word->second.getUsageCost(numForm)<3))
						{
							m[q].flags&=~WordMatch::flagNounOwner;
							m.insert(m.begin()+q+1,WordMatch(Words.gquery(L"ishas"),0,debugTrace));
							for (unsigned int s2=s+1; s2<sentenceStarts.size(); s2++)
								sentenceStarts[s2]++;
							end++;
						}
					}
				}
			}
			// The captain's right.
			// if there is no verb before the owning noun, and no verb after the owned word, and the word is more likely an adjective, then convert.
			// Also a title (which will be in all caps) is more likely to be a noun phrase, so don't expand an ownership.  Also 'worth' is a strange adjective, which tends to be owned (a thousand pound's worth)
			if (m[q].word->first != L"let" && (q+1>=m.size() || m[q+1].word->first != L"worth") && (m[q].flags&WordMatch::flagNounOwner) && !(m[q].flags&WordMatch::flagAllCaps))
			{
				int capitalizedWords = 0;
				for (unsigned int I = begin; I < end; I++)
					if (m[I].flags&WordMatch::flagFirstLetterCapitalized)
						capitalizedWords++;
				// detect titles - titles tend to be noun phrases
				// David Lloyd's Last Will . 
				if (capitalizedWords * 100 / (end - begin) < 75)
				{
					if (debugTrace.traceParseInfo)
						lplog(LOG_INFO, L"NOUNOWNER %s test", m[q].word->first.c_str());
					bool maybeVerb = false, maybeNoun, maybeAdjective, preferNoun, detectNoun = true;
					// go backwards skip past any adjectives, nouns or determiners.  Stop at verb, or at non-noun/adjective or beginning of sentence (begin).
					// go forwards skip past adjectives or nouns. Stop at verb or at non-noun/adjective or at end of sentence (end)
					for (unsigned int I = begin; I < q && !maybeVerb; I++)
						getFormFlags(I, maybeVerb, maybeNoun, maybeAdjective, preferNoun);
					maybeNoun = preferNoun = false;
					int whereLastNoun = -1, whereLastAdjective = -1;
					// don't go beyond the double quote, as it will probably have a verb after it (said) which should not be counted in this analysis.
					// sentenceStarts may not include an end of sentence!
					for (unsigned int I = q + 1; I < end && !isEOS(I) && !maybeVerb && !WordClass::isDoubleQuote(m[I].word->first[0]); I++)
					{
						getFormFlags(I, maybeVerb, maybeNoun, maybeAdjective, preferNoun);
						if (!maybeNoun && !maybeAdjective && !WordClass::isDash(m[I].word->first[0]))
							detectNoun = false;
						if (detectNoun)
						{
							if (maybeNoun && preferNoun)
								whereLastNoun = I;
							if (maybeAdjective && (!maybeNoun || !preferNoun))
								whereLastAdjective = I;
						}
					}
					// if there is no verb anywhere in the sentence (must be very conservative at this stage of tokenization)
					// and if there is no probable noun after the ownership, and there is at least one adjective, then convert.
					if (!maybeVerb && whereLastNoun < 0 && whereLastAdjective >= 0)
					{
						m[q].flags &= ~WordMatch::flagNounOwner;
						m.insert(m.begin() + q + 1, WordMatch(Words.gquery(L"ishas"), 0, debugTrace));
						for (unsigned int s2 = s + 1; s2 < sentenceStarts.size(); s2++)
							sentenceStarts[s2]++;
						unordered_map <unsigned int, wstring> newMetaCommandsEmbeddedInSource;
						for (auto const&[where, comment] : metaCommandsEmbeddedInSource)
							newMetaCommandsEmbeddedInSource[(where > q) ? where +1 : where ] = comment;
						metaCommandsEmbeddedInSource = newMetaCommandsEmbeddedInSource;
						end++;
						if (debugTrace.traceParseInfo)
						{
							wstring sentence;
							for (unsigned int swhere = begin; swhere < end; swhere++)
							{
								wstring originalIWord;
								getOriginalWord(swhere, originalIWord, false, false);
								if (swhere == q)
									originalIWord = L"*" + originalIWord + L"*";
								sentence += originalIWord + L" ";
							}
							lplog(LOG_INFO, L"OWNERCONVERSION [%d]:%s", whereLastAdjective, sentence.c_str());
						}
					}
				}
				else
					if (debugTrace.traceParseInfo)
						lplog(LOG_INFO, L"NOUNOWNER %s capitalized words test %d*100/%d<75", m[q].word->first.c_str(), capitalizedWords, end - begin);
			}
			if (adjustWord(q))
			{
				for (unsigned int s2=s+1; s2<sentenceStarts.size(); s2++)
					sentenceStarts[s2]++;
				unordered_map <unsigned int, wstring> newMetaCommandsEmbeddedInSource;
				for (auto const&[where, comment] : metaCommandsEmbeddedInSource)
					newMetaCommandsEmbeddedInSource[(where > q) ? where + 1 : where] = comment;
				metaCommandsEmbeddedInSource = newMetaCommandsEmbeddedInSource;
				end++;
			}
			// if the word 'no one' is immediately before a non capitalized word which is almost certainly a noun, or cannot be a verb, convert it to 'no' 'one'
			// Of course , no one man would attempt such a thing.
			int nounFormOffset, verbFormOffset;
			if (q < m.size() - 1 && m[q].word->first == L"no one" &&
				(nounFormOffset = m[q + 1].queryForm(L"noun")) >= 0 && // next word could be a noun
				(!m[q + 1].word->second.hasVerbForm() || ((verbFormOffset = m[q + 1].queryForm(L"verb")) >= 0 && m[q + 1].word->second.getUsageCost(verbFormOffset) > m[q + 1].word->second.getUsageCost(nounFormOffset)))) // next word is not a verb, or the cost of the verb is > cost of noun
			{
				m[q].word = Words.gquery(L"no");
				m.insert(m.begin() + q + 1, WordMatch(Words.gquery(L"one"), 0, debugTrace));
				for (unsigned int s2 = s + 1; s2 < sentenceStarts.size(); s2++)
					sentenceStarts[s2]++;
				unordered_map <unsigned int, wstring> newMetaCommandsEmbeddedInSource;
				for (auto const&[where, comment] : metaCommandsEmbeddedInSource)
					newMetaCommandsEmbeddedInSource[(where > q) ? where + 1 : where] = comment;
				metaCommandsEmbeddedInSource = newMetaCommandsEmbeddedInSource;
				end++;
			}
		}
		// if a ' is right after the end of a sentence, and there was a preceding ', extend the end of the sentence.
		// secondaryQuotations should be BEFORE the check for primary (") quotations because single quotations (') should always
		// be inside the primary quotations (") when closing the quote.
		if (end<m.size() && (m[end].word==secondaryQuoteWord && (secondaryQuotations&1)==1))
		{
			secondaryQuotations++;
			unsigned int toEnd=end;
			if (m[end+1].word==Words.sectionWord) endOfParagraph=true;
			while (toEnd+1<m.size() && m[toEnd+1].word==Words.sectionWord) toEnd++;
			sentenceStarts[s+1]=toEnd+1;
			m[end].word=secondaryQuoteCloseWord;
			end++;
#ifdef LOG_QUOTATIONS
			lplog(L"Detected secondaryQuotations at %d increased to %d. (E)",end,secondaryQuotations);
			displayQuoteContext(end-3,end+3);
#endif
		}
		if (end<m.size() && (m[end].word==primaryQuoteWord && (primaryQuotations&1)==1))
		{
			primaryQuotations++;
			unsigned int toEnd=end;
			if (m[end+1].word==Words.sectionWord) endOfParagraph=true;
			while (toEnd+1<m.size() && m[toEnd+1].word==Words.sectionWord) toEnd++;
			sentenceStarts[s+1]=toEnd+1;
			m[end].word=primaryQuoteCloseWord;
			end++;
#ifdef LOG_QUOTATIONS
			lplog(L"Detected primaryQuotations at %d increased to %d. (E)",end,primaryQuotations);
			displayQuoteContext(end-3,end+3);
#endif
		}
		// end of sentence and end of paragraph and still no quote!
		if (((primaryQuotations&1)==1 || (secondaryQuotations&1)==1) && endOfParagraph)
		{
			if (primaryQuotations&1)
			{
				primaryQuotations++;
#ifdef LOG_QUOTATIONS
				lplog(L"%d-%d:Quotations odd upon reaching end-of-section (2).  Inserted quotation at %d. Increasing total primaryQuotations to %d.",begin,end,end,primaryQuotations);
#endif
				m.insert(m.begin()+end,WordMatch(primaryQuoteCloseWord,0,debugTrace));
				m[end].flags|=WordMatch::flagInsertedQuote;
				for (unsigned int s2=s+1; s2<sentenceStarts.size(); s2++)
					sentenceStarts[s2]++;
				unordered_map <unsigned int, wstring> newMetaCommandsEmbeddedInSource;
				for (auto const&[where, comment] : metaCommandsEmbeddedInSource)
					newMetaCommandsEmbeddedInSource[(where > end) ? where + 1 : where] = comment;
				metaCommandsEmbeddedInSource = newMetaCommandsEmbeddedInSource;
			}
			if (secondaryQuotations&1)
			{
				//if (wcsstr(L"aeiou",m[lastSecondaryQuote+1].word->first[0])!=NULL ||
				secondaryQuotations--;
				m[lastSecondaryQuote].word=secondaryQuoteWord;
				//if (m[end-1].word==primaryQuoteCloseWord) end--; // insert secondary quotes inside of primary quotes
#ifdef LOG_QUOTATIONS
				lplog(L"%d-%d:Secondary quotations odd upon reaching end-of-section (4).  Reverted quotation at %d. Decreasing total secondaryQuotations to %d.",begin,end,lastSecondaryQuote,secondaryQuotations);
#endif
				lastSecondaryQuote=-1;
				//m.insert(m.begin()+end,WordMatch(secondaryQuoteCloseWord,0));
				//m[end].flags|=WordMatch::flagInsertedQuote;
				//for (unsigned int s2=s+1; s2<sentenceStarts.size(); s2++)
				//	sentenceStarts[s2]++;
			}
#ifdef LOG_QUOTATIONS
			displayQuoteContext(begin-5,end+5);
#endif
			quotationExceptions++;
		}
	}
	puts("");
	vector <WordMatch>::iterator im=m.begin(),imEnd=m.end();
	int outerPrimaryQuotes=0,outerSecondaryQuotes=0;
	int innerPrimaryQuotes=0,innerSecondaryQuotes=0;
	bool inSecondaryQuote=false;
	bool inPrimaryQuote=false;
	for (int I=0; im!=imEnd; im++,I++)
	{
		if (im->word==primaryQuoteOpenWord) 
		{
			if (inSecondaryQuote || inPrimaryQuote)
			{
				innerPrimaryQuotes++;
				im->word=secondaryQuoteOpenWord;
			}
			else
				outerPrimaryQuotes++;
			inPrimaryQuote=true;
		}
		else if (im->word==secondaryQuoteOpenWord) 
		{
			inSecondaryQuote=true;
			if (inPrimaryQuote)
				innerSecondaryQuotes++;
			else
			{
				im->word=primaryQuoteOpenWord;
				outerSecondaryQuotes++;
			}
		}
		else if (im->word==primaryQuoteCloseWord) 
		{
			inPrimaryQuote=false;
			if (inSecondaryQuote)
				im->word=secondaryQuoteCloseWord;
		}
		else if (im->word==secondaryQuoteCloseWord) 
		{
			inSecondaryQuote=false;
			if (!inPrimaryQuote)
				im->word=primaryQuoteCloseWord;
		}
	}
	setForms();
	return quotationExceptions;
}

void Source::adjustWords(void)
{ LFS
	// rearrange quotes, also figure out plural ownership and more on would/had is/has
	for (unsigned int s=0; s+1<sentenceStarts.size(); s++)
	{
		unsigned int begin=sentenceStarts[s];
		unsigned int end=sentenceStarts[s+1];
    while (end && m[end-1].word==Words.sectionWord)
      end--; // cut off end of paragraphs
		if (begin>=end)
		{
			sentenceStarts.erase(s--);
			continue;
		}
		unsigned int sectionEnd;
		if (isSectionHeader(begin,end,sectionEnd))
		{
			sentenceStarts.insert(s+1,sectionEnd);
			continue;
		}
		bool endOfParagraph=false;
		while (end && m[end-1].word==Words.sectionWord)
		{
			end--; // cut off end of paragraphs
			endOfParagraph=true;
		}
		if (!end)
			continue;
		for (unsigned int q=begin; q<end; q++)
		{
			if (adjustWord(q))
			{
				for (unsigned int s2=s+1; s2<sentenceStarts.size(); s2++)
					sentenceStarts[s2]++;
				unordered_map <unsigned int, wstring> newMetaCommandsEmbeddedInSource;
				for (auto const&[where, comment] : metaCommandsEmbeddedInSource)
					newMetaCommandsEmbeddedInSource[(where > q) ? where + 1 : where] = comment;
				metaCommandsEmbeddedInSource = newMetaCommandsEmbeddedInSource;
				end++;
			}
		}
	}
}

