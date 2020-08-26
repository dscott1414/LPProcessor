#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "profile.h"
#include "paice.h"

bool cSource::adjustWord(unsigned int q)
{ LFS
	bool insertedOrDeletedWord=false;
	// let's have some dinner!
	if ((m[q].flags&cWordMatch::flagNounOwner) && m[q].word->first==L"let")
	{
		m[q].flags&=~cWordMatch::flagNounOwner;
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"us"),0,debugTrace));
		m[q+1].forms.set(personalPronounAccusativeForm);
		insertedOrDeletedWord=true;
	}
	// d'ye -> do you
	else if (m[q].word->first == L"d'ye" || m[q].word->first == L"d'you")
	{
		m[q].word = Words.gquery(L"do");
		m[q].forms.clear();
		m[q].forms.set(cForms::gFindForm(L"does"));
		m[q].flags = 0;
		m.insert(m.begin() + q, cWordMatch(Words.gquery(L"you"), 0, debugTrace));
		m[q + 1].forms.set(cForms::gFindForm(L"personal_pronoun"));
		insertedOrDeletedWord = true;
	}
	// t'other -> the other
	else if (m[q].word->first == L"t'other")
	{
		m[q].word = Words.gquery(L"the");
		m[q].forms.clear();
		m[q].forms.set(cForms::gFindForm(L"determiner"));
		m[q].flags = 0;
		m.insert(m.begin() + q, cWordMatch(Words.gquery(L"other"), 0, debugTrace));
		m[q + 1].forms.set(cForms::gFindForm(L"pronoun"));
		insertedOrDeletedWord = true;
	}
	// more'n -> more than
	else if (m[q].word->first == L"more'n")
	{
		m[q].word = Words.gquery(L"more");
		m[q].forms.clear();
		m[q].forms.set(cForms::gFindForm(L"adverb"));
		m[q].flags = 0;
		m.insert(m.begin() + q, cWordMatch(Words.gquery(L"than"), 0, debugTrace));
		m[q + 1].forms.set(cForms::gFindForm(L"conjunction"));
		m[q + 1].forms.set(cForms::gFindForm(L"preposition"));
		insertedOrDeletedWord = true;
	}
	// What's he want? --> What does he want?
	// What's for dinner? --> What is for dinner?
	// What's that got to do with it? --> What has that got to do with it?
	else if ((m[q].flags&cWordMatch::flagNounOwner) && (m[q].word->first==L"what"))
	{
		m[q].flags&=~cWordMatch::flagNounOwner;
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"ishasdoes"),0,debugTrace));
		m[q+1].setPreferredForm();
		insertedOrDeletedWord=true;
	}
	else if ((m[q].flags&cWordMatch::flagNounOwner) && (m[q].word->first==L"that"))
	{
		m[q].flags&=~cWordMatch::flagNounOwner;
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"is"),0,debugTrace));
		m[q+1].setPreferredForm();
		insertedOrDeletedWord=true;
	}
	// twas -> it was
	else if (cWord::isSingleQuote(m[q].word->first[0]) && q + 1 < m.size() && m[q + 1].word->first == L"twas")
	{
		m[q].word = Words.gquery(L"it");
		m[q].forms.clear();
		m[q].forms.set(personalPronounForm);
		m[q].flags = 0;
		m[q + 1].word = Words.gquery(L"was");
		m[q + 1].forms.clear();
		m[q + 1].forms.set(cForms::gFindForm(L"is"));
		m[q + 1].flags = 0;
	}
	// 'Tis the season --> It is the season
	else if (cWord::isSingleQuote(m[q].word->first[0]) && q+1<m.size() && m[q+1].word->first==L"tis")
	{
		m[q].word=Words.gquery(L"it");
		m[q].forms.clear();
		m[q].forms.set(personalPronounForm);
		m[q].flags=0;
		m[q+1].word=Words.gquery(L"is");
		m[q+1].forms.clear();
		m[q+1].forms.set(cForms::gFindForm(L"is"));
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
			m.insert(m.begin() + q + 1, cWordMatch(Words.gquery(L"a"), 0, debugTrace));
			m[q + 1].forms.set(determinerForm);
		}
		else
		{
			m.insert(m.begin() + q + 1, cWordMatch(Words.gquery(L"to"), 0, debugTrace));
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
		m.insert(m.begin() + q + 1, cWordMatch(Words.gquery(L"you"), 0, debugTrace));
		m[q + 1].forms.set(cForms::gFindForm(L"personal_pronoun"));
		insertedOrDeletedWord = true;
	}
	// I lived at 23 Beek St.  That was a nice block.
	else if (q>0 && m[q].queryForm(sa_abbForm)>=0 &&
		(m[q-1].flags&cWordMatch::flagFirstLetterCapitalized) &&
		(m[q+1].flags&cWordMatch::flagFirstLetterCapitalized))
	{
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"."),0,debugTrace));
		insertedOrDeletedWord=true;
	}
	// 2:30 A.M.
	else if (q>0 && (m[q].word->first==L"a.m." || m[q].word->first==L"p.m.") && (m[q-1].queryForm(timeForm)>=0 || m[q-1].queryForm(NUMBER_FORM_NUM)>=0) &&
		(m[q+1].flags&cWordMatch::flagFirstLetterCapitalized) && m[q+1].queryForm(L"daysOfWeek")<0)
	{
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"."),0,debugTrace));
		insertedOrDeletedWord=true;
	}
	// 2390 B.C.
	else if (q>0 && (m[q].word->first==L"a.d." || m[q].word->first==L"b.c.") && m[q-1].queryForm(numberForm)>=0 &&
		(m[q+1].flags&cWordMatch::flagFirstLetterCapitalized))
	{
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"."),0,debugTrace));
		insertedOrDeletedWord=true;
	}
	else if (q>0 && m[q].word->first==L"no." &&
		m[q+1].queryForm(numberForm)<0 && m[q+1].queryForm(numeralCardinalForm)<0 && m[q+1].queryForm(romanNumeralForm)<0 &&
		(m[q+1].flags&cWordMatch::flagFirstLetterCapitalized))
	{
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"."),0,debugTrace));
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
		m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"to"),0,debugTrace));
		insertedOrDeletedWord=true;
		m[q+1].forms.set(toForm);
	}
	return insertedOrDeletedWord;
}

bool cSource::quoteTest(int q,unsigned int &quoteCount,int &lastPSQuote,tIWMM quoteType,tIWMM quoteOpenType,tIWMM quoteCloseType)
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

void cSource::secondaryQuoteTest(int q,unsigned int &secondaryQuotations,int &lastSecondaryQuote,
																tIWMM secondaryQuoteWord,tIWMM secondaryQuoteOpenWord,tIWMM secondaryQuoteCloseWord)
{ LFS
	// if space before quote, it should be an open quote.  if space after quote, it should be a close quote.
	// if two open quotes follow each other, delete the first open quote.
	bool preferCloseQuoteBySpace=(m[q].flags&cWordMatch::flagAlphaBeforeHint) && !(m[q].flags&cWordMatch::flagAlphaAfterHint);
	bool preferOpenQuoteBySpace=!(m[q].flags&cWordMatch::flagAlphaBeforeHint) && (m[q].flags&cWordMatch::flagAlphaAfterHint);
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

void cSource::eraseLastQuote(int &lastPSQuote,tIWMM quoteCloseWord,unsigned int &q)
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

bool cSource::getFormFlags(int where, bool &maybeVerb, bool &maybeNoun, bool &maybeAdjective, bool &preferNoun)
{
	int verbFormOffset= m[where].queryForm(verbForm),nounFormOffset,adjectiveFormOffset;
	// does this position have any verb form other than verbForm itself (then check if it does have verbForm, that it is a probable verbForm)
	maybeVerb = (m[where].word->second.hasVerbForm() && verbFormOffset<0) || (verbFormOffset >= 0 && m[where].word->second.getUsageCost(verbFormOffset) < 4);
	if ((m[where].flags&cWordMatch::flagFirstLetterCapitalized) && !(m[where].flags&cWordMatch::flagAllCaps))
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
unsigned int cSource::doQuotesOwnershipAndContractions(unsigned int &primaryQuotations)
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
		if (sourceType!=NEWS_BANK_SOURCE_TYPE && isSectionHeader(begin,end,sectionEnd))
			if (end!=sectionEnd) sentenceStarts.insert(s+1,sectionEnd);
		for (unsigned int q=begin; q<end; q++)
		{
			bool afterPossibleAbbreviation=(q>1 && m[q-1].word->first==L"." && (m[q-2].queryForm(abbreviationForm)>=0 || m[q-2].queryForm(honorificAbbreviationForm)>=0 || m[q-2].queryForm(letterForm)>=0));
			// logic copied from cSource::parse
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
					 (m[q-2].flags&(cWordMatch::flagFirstLetterCapitalized));
			// added q>0 because in abstracts, most of the other criteria is invalid (see Curveball - Rafid Ahmed Alwan al-Janabi, known by the Central Intelligence Agency cryptonym "Curveball")
			if (((firstWordInSentence && q>0) || m[q].flags&cWordMatch::flagAllCaps) && !mightBeName &&
				  (m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN)==0 ||
				   (m[q].word->second.numProperNounUsageAsAdjective==m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN) &&
					  (q+1>=m.size() || (m[q+1].flags&(cWordMatch::flagFirstLetterCapitalized|cWordMatch::flagAllCaps))==0))) &&
					//m[q].word->second.relatedSubTypeObjects.size()==0 && // not a known common place 
					(q<1 || (m[q-1].word->first!=L"the" && !(m[q-1].flags&cWordMatch::flagAllCaps))) && 
					!((m[q].flags&cWordMatch::flagAllCaps) && m[q].word->second.formsSize()==0) && // The US / that IRS tax form
				  // must NOT be queryForm because of test
				  (m[q].word->second.getUsagePattern(cSourceWordInfo::LOWER_CASE_USAGE_PATTERN)>0 || m[q].word->second.query(PROPER_NOUN_FORM_NUM)<0))
			{
				// if flagAddProperNoun and first word, check if usage pattern is 0.  If it is, unflag word.
				if (m[q].flags&cWordMatch::flagAddProperNoun)
				{
					m[q].flags&=~cWordMatch::flagAddProperNoun;
					if (debugTrace.traceParseInfo)
						lplog(LOG_INFO, L"%d:%s:removed flagAddProperNoun asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.",
							q, getOriginalWord(q, originalWord, false), m[q].word->second.numProperNounUsageAsAdjective,
							m[q].word->second.getUsagePattern(cSourceWordInfo::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN),
							m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);
				}
				// refuse to make it proper noun, even if it is listed as one.  see cWordMatch::queryForm(int form)
				else
					if ((m[q].flags&cWordMatch::flagFirstLetterCapitalized) && !afterPossibleAbbreviation && !(m[q].flags&cWordMatch::flagAllCaps) && m[q].word->second.localWordIsLowercase > 0)
					{
						m[q].flags|=cWordMatch::flagRefuseProperNoun;
						if (debugTrace.traceParseInfo)
							lplog(LOG_INFO, L"%d:%s:added flagRefuseProperNoun asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.", 
								q, getOriginalWord(q, originalWord,false),m[q].word->second.numProperNounUsageAsAdjective,
								m[q].word->second.getUsagePattern(cSourceWordInfo::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN),
								m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);
					}
			}
			if (firstWordInSentence && (m[q].flags&cWordMatch::flagFirstLetterCapitalized) && !(m[q].flags&cWordMatch::flagAddProperNoun) && !(m[q].flags & cWordMatch::flagRefuseProperNoun) &&
				m[q].word->second.query(PROPER_NOUN_FORM_NUM) < 0 && 
				((m[q].word->second.localWordIsLowercase == 0 && m[q].word->second.localWordIsCapitalized > 2) || m[q].word->second.localWordIsCapitalized > 20))
			{
				m[q].flags |= cWordMatch::flagAddProperNoun;
				if (debugTrace.traceParseInfo)
					lplog(LOG_INFO, L"%d:%s:added flagAddProperNoun (from local) asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.",
						q, getOriginalWord(q, originalWord, false), m[q].word->second.numProperNounUsageAsAdjective,
						m[q].word->second.getUsagePattern(cSourceWordInfo::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN),
						m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);

			}
			if (m[q].word->first == L"lord" && (m[q].flags&cWordMatch::flagFirstLetterCapitalized) && !(m[q].flags&cWordMatch::flagAddProperNoun) && q + 1 < m.size() && !(m[q + 1].flags&cWordMatch::flagFirstLetterCapitalized))
			{
				m[q].flags |= cWordMatch::flagAddProperNoun;
				if (debugTrace.traceParseInfo)
					lplog(LOG_INFO, L"%d:%s:added flagAddProperNoun (SPECIAL CASE lord) asAdjective=%d global lower case=%d global upper case=%d local lower case=%d local upper case=%d.",
						q, getOriginalWord(q, originalWord, false), m[q].word->second.numProperNounUsageAsAdjective,
						m[q].word->second.getUsagePattern(cSourceWordInfo::LOWER_CASE_USAGE_PATTERN), (int)m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN),
						m[q].word->second.localWordIsLowercase, m[q].word->second.localWordIsCapitalized);
			}
			// if capitalized, not all caps, at least 2 letters long, not a cardinal or ordinal number
			// does not already have flagRefuseProperNoun or flagAddProperNoun or flagOnlyConsiderProperNounForms or flagOnlyConsiderOtherNounForms set
			// is never used in the lower case, and has been used as a proper noun unambiguously
			if ((m[q].flags&cWordMatch::flagFirstLetterCapitalized) && !(m[q].flags&cWordMatch::flagAllCaps) && m[q].word->first[1] &&
				m[q].word->second.query(numeralOrdinalForm)==-1 && m[q].word->second.query(numeralCardinalForm)==-1 &&
				!(m[q].flags&(cWordMatch::flagRefuseProperNoun|cWordMatch::flagOnlyConsiderProperNounForms|cWordMatch::flagOnlyConsiderOtherNounForms)) &&
				(m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN)>0 || m[q].word->second.localWordIsCapitalized>0) &&  // PROPER_NOUN_USAGE_PATTERN is only updated if proper noun form is added 
				m[q].word->second.getUsagePattern(cSourceWordInfo::LOWER_CASE_USAGE_PATTERN)==0 &&
				// word was found capitalized alone (the number of times being a capitalized adjective is < the number of times being capitalized)
				(m[q].word->second.numProperNounUsageAsAdjective<m[q].word->second.getUsagePattern(cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN) ||
				// OR this particular position is followed by a capitalized word (Peel Edgerton) - peel is also a verb!
				 (q+1<m.size() && (m[q+1].flags&(cWordMatch::flagFirstLetterCapitalized|cWordMatch::flagAllCaps))!=0)))
			{
				// if word serves only as an adjective in a proper noun ('New' York), don't mark as only proper noun
				//lplog(L"%d:DEBUG PNU %d %d",q,m[q].word->second.numProperNounUsageAsAdjective,(int)m[q].word->second.usagePatterns[cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN]);
				bool atLeastOneProperNounForm=(m[q].flags&cWordMatch::flagAddProperNoun)!=0;
				for (unsigned int I=0; I<m[q].word->second.formsSize() && !atLeastOneProperNounForm; I++)
					if (m[q].word->second.Form(I)->properNounSubClass || m[q].word->second.forms()[I]==PROPER_NOUN_FORM_NUM)
						atLeastOneProperNounForm=true;
				if (atLeastOneProperNounForm)
				{
					m[q].flags|=cWordMatch::flagOnlyConsiderProperNounForms;
					if (debugTrace.traceParseInfo)
						lplog(LOG_INFO,L"%d:%s:added flagOnlyConsiderProperNounForms (2).",q, getOriginalWord(q, originalWord, false));
				}
			}
			if ((m[q].flags&cWordMatch::flagOnlyConsiderProperNounForms) && end-begin>4 && !m[q].word->second.isUnknown())
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
					bool isCapitalized=(m[si].queryForm(PROPER_NOUN_FORM_NUM)>=0 || (m[si].flags&cWordMatch::flagFirstLetterCapitalized) || (m[si].flags&cWordMatch::flagAllCaps));
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
					m[q].flags&=~cWordMatch::flagOnlyConsiderProperNounForms;
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
			if (m[q].flags&cWordMatch::flagPossiblePluralNounOwner)
			{
				m[q].flags&=~cWordMatch::flagPossiblePluralNounOwner;
				// only if the word is a plural noun type and their is no unresolved single quote
				if (((m[q].word->second.inflectionFlags&PLURAL) || m[q].word->first[m[q].word->first.length()-1]==L's') && !(secondaryQuotations&1))
				{
					m[q].flags|=cWordMatch::flagNounOwner;
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
			else if ((m[q].flags&cWordMatch::flagNounOwner) &&
				// must be single or have a determiner AFTER the word
				(!(m[q].word->second.inflectionFlags&PLURAL) || (q+1<lastWord && m[q+1].queryWinnerForm(determinerForm)>=0)) &&
				// must be a nounType, that, a pronoun (nominal (one, I, we) acc (him, them...), indefinite or quantifier 
				(m[q].isNounType() || m[q].word->first==L"that" || m[q].queryForm(nomForm)>=0 || m[q].queryForm(personalPronounAccusativeForm)>=0 || m[q].queryForm(indefinitePronounForm)>=0 || m[q].queryForm(quantifierForm)>=0) &&
				// and NOT "let" (because of "let's")
				m[q].word->first!=L"let" &&
				(q+1<lastWord && m[q+1].word!=Words.sectionWord &&
				// the next word must be "been" or (NOT is and NOT punctuation and NOT 'will').
				(m[q+1].word->first==L"been" || (m[q+1].queryForm(L"is")<0 && m[q+1].queryForm(L"is_negation")<0 && iswalpha(m[q+1].word->first[0]) && m[q + 1].word->first != L"will" && m[q + 1].word->first != L"can"))) &&
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
							m[q].flags&=~cWordMatch::flagNounOwner;
							m.insert(m.begin()+q+1,cWordMatch(Words.gquery(L"ishas"),0,debugTrace));
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
			if (m[q].word->first != L"let" && (q+1>=m.size() || m[q+1].word->first != L"worth") && (m[q].flags&cWordMatch::flagNounOwner) && !(m[q].flags&cWordMatch::flagAllCaps))
			{
				int capitalizedWords = 0;
				for (unsigned int I = begin; I < end; I++)
					if (m[I].flags&cWordMatch::flagFirstLetterCapitalized)
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
					for (unsigned int I = q + 1; I < end && !isEOS(I) && !maybeVerb && !cWord::isDoubleQuote(m[I].word->first[0]); I++)
					{
						getFormFlags(I, maybeVerb, maybeNoun, maybeAdjective, preferNoun);
						if (!maybeNoun && !maybeAdjective && !cWord::isDash(m[I].word->first[0]))
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
						m[q].flags &= ~cWordMatch::flagNounOwner;
						m.insert(m.begin() + q + 1, cWordMatch(Words.gquery(L"ishas"), 0, debugTrace));
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
				m.insert(m.begin() + q + 1, cWordMatch(Words.gquery(L"one"), 0, debugTrace));
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
				m.insert(m.begin()+end,cWordMatch(primaryQuoteCloseWord,0,debugTrace));
				m[end].flags|=cWordMatch::flagInsertedQuote;
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
				//m.insert(m.begin()+end,cWordMatch(secondaryQuoteCloseWord,0));
				//m[end].flags|=cWordMatch::flagInsertedQuote;
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
	vector <cWordMatch>::iterator im=m.begin(),imEnd=m.end();
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

void cSource::adjustWords(void)
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

#define ENCODING_STRING L"Character set encoding:"
// 0 -not set
#define HAS_BOM 1
#define FIND_START 2
#define FIND_START_BUFFER_CONVERT 4
#define QUOTE_IN_START 8
#define FIND_START_INITIAL_FAIL 16
#define FIND_START_INITIAL_SUCCESS 32
#define FIND_START_SUCCESS_AFTER_CONVERT 64
// UTF-8 // Unicode (UTF-8)
#define SOURCE_UTF8 128 // 65001
// Latin: Latin-1, Latin1, ISO Latin - 1, Latin 1, ISO - Latin - 1 // // ANSI Latin 1; Western European (Windows)
#define SOURCE_1252 256  // ANSI Latin 1; Western European (Windows) // 1252
// 8859: ISO - 8859 - 1, ISO 8859 - 1, ASCII, with some ISO - 8859 - 1 characters // ISO 8859-1 Latin 1; Western European (ISO)
#define SOURCE_8859 512 // ISO 8859-1 Latin 1; Western European (ISO) // 28591
#define SOURCE_ASCII 2048 // ASCII: ISO-646-US (US-ASCII), ASCII, US-ASCII  // US-ASCII (7-bit) // 20127
#define SOURCE_UNICODE 1024
#define ENCODING_MATCH_FAILED 4096
#define ENCODING_EXPLICIT_NOTE_DISAGREEMENT 8192
bool reDecodeNecessary(wstring encodingRecordedInDocument, int &codePage, bool iso8859ControlCharactersFound, bool &explicitNoteDisagreement)
{
	int encodingRID = codePage;
	if (encodingRecordedInDocument.find(L"UTF") != wstring::npos)
		encodingRID = 65001;
	else if (encodingRecordedInDocument.find(L"Latin") != wstring::npos)
		encodingRID = 1252;
	else if (encodingRecordedInDocument.find(L"8859") != wstring::npos)
		encodingRID = 28591;
	else if (encodingRecordedInDocument.find(L"ASCII") != wstring::npos)
		encodingRID = 20127;
	if (encodingRID != codePage)
	{
		if (explicitNoteDisagreement = codePage == 1252 && iso8859ControlCharactersFound && encodingRID == 28591)
			lplog(LOG_ERROR, L"Encoding error: %s (%d) embedded in source disagrees with decoding guess %d, but control characters found so explicit encoding note in source is discarded.", encodingRecordedInDocument.c_str(), encodingRID, codePage);
		else
		{
			lplog(LOG_ERROR, L"Encoding error: %s (%d) embedded in source disagrees with decoding guess %d", encodingRecordedInDocument.c_str(), encodingRID, codePage);
			codePage = encodingRID;
			return true;
		}
	}
	return false;
}


int cSource::readSourceBuffer(wstring title, wstring etext, wstring path, wstring encodingFromDB, wstring &start, int &repeatStart)
{
	LFS
		beginClock = clock();
	int readBufferFlags = 0;
	//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path.c_str(), __FUNCTIONW__);
	HANDLE fd = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
	if (fd == INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR, L"ERROR:Unable to open %s - %S. (3)", path.c_str(), _sys_errlist[errno]);
		return -1;
	}
	if (!GetFileSizeEx(fd, (PLARGE_INTEGER)&bufferLen))
	{
		lplog(LOG_ERROR, L"ERROR:Unable to get the file length of %s - %S. (3)", path.c_str(), _sys_errlist[errno]);
		return -1;
	}
	bookBuffer = (wchar_t *)tmalloc((size_t)bufferLen + 10);
	DWORD numBytesRead;
	if (!ReadFile(fd, bookBuffer, (unsigned int)bufferLen, &numBytesRead, 0) || bufferLen != numBytesRead)
	{
		CloseHandle(fd);
		lplog(LOG_ERROR, L"ERROR:Unable to read %s - %S. (3)", path.c_str(), _sys_errlist[errno]);
		return -1;
	}
	CloseHandle(fd);
	if (bufferLen < 0 || bufferLen == 0)
		return PARSE_EOF;
	bool hasBOM = bookBuffer[0] == 0xFEFF;
	bufferScanLocation = (hasBOM) ? 1 : 0; // detect BOM
	sourcePath = path;
	bufferLen /= sizeof(bookBuffer[0]);
	if (sourceType == cSource::WEB_SEARCH_SOURCE_TYPE || sourceType == WIKIPEDIA_SOURCE_TYPE || sourceType == INTERACTIVE_SOURCE_TYPE || sourceType == PATTERN_TRANSFORM_TYPE || sourceType == REQUEST_TYPE)
		return 0;
	bookBuffer[bufferLen] = 0;
	bookBuffer[bufferLen + 1] = 0;
	bookBuffer[bufferLen + 2] = 0;
	bookBuffer[bufferLen + 3] = 0;
	bookBuffer[bufferLen + 4] = 0;
	if (hasBOM)
		readBufferFlags += HAS_BOM;
	int bl = (int)bufferLen;
	bookBuffer = (wchar_t *)trealloc(20, bookBuffer, (bl + 10) << 1, (bl + 10) << 2);
	bufferLen <<= 1;
	wstring wb;
	int codepage;
	bool iso8859ControlCharactersFound = false;
	mTW((char *)bookBuffer, wb, codepage, iso8859ControlCharactersFound);
	wstring sourceEncoding = L"NOT FOUND";
	if (wb.length() < 10)
	{
		readBufferFlags |= SOURCE_UNICODE;
		sourceEncoding = L"UNICODE";
		wb = bookBuffer;
	}
	else
	{
		int ew, weol;
		if ((ew = wb.find(ENCODING_STRING)) != wstring::npos && ((weol = wb.find(13, ew + wcslen(ENCODING_STRING))) != wstring::npos || (weol = wb.find(10, ew + wcslen(ENCODING_STRING))) != wstring::npos))
		{
			ew += wcslen(ENCODING_STRING);
			sourceEncoding = wb.substr(ew, weol - ew);
			trim(sourceEncoding);
		}
		int error = 0, desiredCodePage = codepage;
		bool explicitNoteDisagreement = false;
		if (sourceEncoding != L"NOT FOUND" && (reDecodeNecessary(sourceEncoding, desiredCodePage, iso8859ControlCharactersFound, explicitNoteDisagreement)) && !mTWCodePage((char *)bookBuffer, wb, desiredCodePage, error))
		{
			desiredCodePage = 1252; // try ASCII
			if (mTWCodePage((char *)bookBuffer, wb, desiredCodePage, error))
				codepage = desiredCodePage;
			readBufferFlags |= ENCODING_MATCH_FAILED;
		}
		else
			codepage = desiredCodePage;
		error = wcscpy_s(bookBuffer, bufferLen + 10, wb.c_str());
		if (error)
			lplog(LOG_FATAL_ERROR, L"ERROR:Unable to copy string length %d into buffer of length %I64d wchar - (%d) %d.", (int)wb.length(), bufferLen, error, GetLastError());
		if (codepage == CP_UTF8)
			readBufferFlags |= SOURCE_UTF8;
		else if (codepage == 1252) // ANSI Latin 1; Western European (Windows)
			readBufferFlags |= SOURCE_1252;
		else if (codepage == 28591) // ISO 8859-1 Latin 1; Western European (ISO)
			readBufferFlags |= SOURCE_8859;
		else if (codepage == 20127) // ASCII: ISO-646-US (US-ASCII), ASCII, US-ASCII  // US-ASCII (7-bit)
			readBufferFlags |= SOURCE_ASCII;
		if (explicitNoteDisagreement)
			readBufferFlags |= ENCODING_EXPLICIT_NOTE_DISAGREEMENT;
	}
	bool startSet = true;
	if (start == L"**FIND**" || encodingFromDB != sourceEncoding || (readBufferFlags & ENCODING_MATCH_FAILED) != 0)
	{
		readBufferFlags += FIND_START;
		startSet = findStart(wb, start, repeatStart, title);
		// write path back to DB
		updateSourceStart(start, repeatStart, etext, bufferLen);
		updateSourceEncoding(readBufferFlags, sourceEncoding, etext);
	}
	if (!startSet)
		return -1;
	size_t quoteEscapeFromDB = wstring::npos;
	while ((quoteEscapeFromDB = start.find(L"\\'")) != wstring::npos)
	{
		readBufferFlags |= QUOTE_IN_START;
		start.erase(start.begin() + quoteEscapeFromDB);
	}
	if (scanUntil(start.c_str(), repeatStart, false) < 0)
	{
		lplog(LOG_ERROR, L"ERROR:Unable to find start in %s - start=%s, repeatStart=%d.", path.c_str(), start.c_str(), repeatStart);
		start = L"**START NOT FOUND**";
		updateSourceStart(start, -1, etext, 0);
		return -3;
	}
	if (sourceType != PATTERN_TRANSFORM_TYPE) // patterns are included in variables which have _ in them
		for (unsigned int I = 0; I < bufferLen; I++)
			if (bookBuffer[I] == L'_') bookBuffer[I] = L' ';
	while (bufferLen > 0 && !bookBuffer[bufferLen - 1])
		bufferLen--;
	return 0;
}

int cSource::parseBuffer(wstring &path, unsigned int &unknownCount)
{
	LFS
		int lastProgressPercent = 0, result = 0, runOnSentences = 0;
	bool alreadyAtEnd = false, previousIsProperNoun = false;
	bool webScrapeParse = sourceType == WEB_SEARCH_SOURCE_TYPE || sourceType == REQUEST_TYPE, multipleEnds = false;
	size_t lastSentenceEnd = m.size(), numParagraphsInSection = 0;
	unordered_map <wstring, wstring> parseVariables;
	if (bufferScanLocation == 0 && bookBuffer[0] == 65279)
		bufferScanLocation = 1;
	while (result == 0 && !exitNow && runOnSentences<20) // too many run on sentences indicate malformed source
	{
		wstring sWord, comment;
		int nounOwner = 0;
		bool flagAlphaBeforeHint = (bufferScanLocation && iswalpha(bookBuffer[bufferScanLocation - 1]));
		bool flagNewLineBeforeHint = (bufferScanLocation && bookBuffer[bufferScanLocation - 1] == 13);
		result = Words.readWord(bookBuffer, bufferLen, bufferScanLocation, sWord, comment, nounOwner, false, webScrapeParse, debugTrace, &mysql, sourceId);
		if (comment.size() > 0)
			metaCommandsEmbeddedInSource[m.size()] = comment;
		bool flagAlphaAfterHint = (bufferScanLocation < bufferLen && iswalpha(bookBuffer[bufferScanLocation]));
		if (result == PARSE_EOF)
			break;
		if (result == PARSE_PATTERN)
		{
			temporaryPatternBuffer[(int)m.size()] = sWord;
			result = 0;
			// [variable name]=[substitute word for parsing]:[pattern list]
			size_t equalsPos = sWord.find(L'='), colonPos = sWord.find(L':');
			if (equalsPos != wstring::npos && colonPos != wstring::npos)
			{
				wstring variable = sWord.substr(0, equalsPos);
				parseVariables[variable] = sWord = sWord.substr(equalsPos + 1, colonPos - equalsPos - 1);
				::lplog(LOG_WHERE, L"%d:parse created mapped variable %s=(%s)", m.size(), variable.c_str(), parseVariables[variable].c_str());
			}
			else
			{
				::lplog(LOG_WHERE, L"%d:parse used mapped variable %s=(%s)", m.size(), sWord.c_str(), parseVariables[sWord].c_str());
				sWord = parseVariables[sWord];
			}
		}
		if (result == PARSE_END_SECTION || result == PARSE_END_PARAGRAPH || result == PARSE_END_BOOK || result == PARSE_DUMP_LOCAL_OBJECTS)
		{
			if (webScrapeParse && m.size() > 1 && m[m.size() - 1].word == Words.sectionWord)
			{
				result = 0;
				continue;
			}
			if (analyzeEnd(path, lastSentenceEnd, m.size(), multipleEnds))
			{
				m.erase(m.begin() + lastSentenceEnd, m.end());
				alreadyAtEnd = true;
				break;
			}
			if (result == PARSE_END_PARAGRAPH && (sourceType != NEWS_BANK_SOURCE_TYPE || numParagraphsInSection++ < 2))
			{
				sentenceStarts.push_back(lastSentenceEnd);
				lastSentenceEnd = m.size();
			}
			if (sourceType == NEWS_BANK_SOURCE_TYPE && result == PARSE_END_BOOK)
				numParagraphsInSection = 0;
			if (m.size() == lastSentenceEnd) lastSentenceEnd++;
			m.push_back(cWordMatch(Words.sectionWord, (result == PARSE_DUMP_LOCAL_OBJECTS) ? 1 : 0, debugTrace));
			result = 0;
			continue;
		}
		size_t dash = wstring::npos, firstDash = wstring::npos;
		int numDash = 0, offset = 0;
		for (wchar_t dq : sWord)
		{
			if (cWord::isDash(dq))
			{
				numDash++;
				// if the word contains two dashes that follow right after each other or are of different types, split word immediately.
				if (numDash > 1 && (offset == dash + 1 || dq != sWord[dash]))
				{
					if (dq != sWord[dash] && offset > dash + 1)
					{
						bufferScanLocation -= sWord.length() - offset;
						sWord.erase(offset, sWord.length() - offset);
						numDash = 1;
						break;
					}
					else if (dash > 0)
					{
						bufferScanLocation -= sWord.length() - dash;
						sWord.erase(dash, sWord.length() - dash);
						dash = wstring::npos;
						numDash = 0;
						break;
					}
				}
				dash = offset;
				if (firstDash == wstring::npos)
					firstDash = offset;
			}
			offset++;
		}
		bool firstLetterCapitalized = iswupper(sWord[0]) != 0;
		tIWMM w = (m.size()) ? m[m.size() - 1].word : wNULL;
		// this logic is copied in doQuotesOwnershipAndContractions
		bool firstWordInSentence = m.size() == lastSentenceEnd || w->first == L"." || // include "." because the last word may be a contraction
			w == Words.sectionWord || w->first == L"--" /* Ulysses */ || w->first == L"?" || w->first == L"!" ||
			w->first == L":" /* Secret Adversary - Second month:  Promoted to drying aforesaid plates.*/ ||
			w->first == L";" /* I am a Soldier A jolly British Soldier ; You can see that I am a Soldier by my feet . . . */ ||
			w->second.query(quoteForm) >= 0 || w->second.query(dashForm) >= 0 || // BNC 4.00 PM - We
			w->second.query(bracketForm) >= 0 || // BNC (c) Complete the ...
			(m.size() - 3 == lastSentenceEnd && m[m.size() - 2].word->second.query(quoteForm) >= 0 && w->first == L"(") || // Pay must be good.' (We might as well make that clear from the start.)
			(m.size() - 2 == lastSentenceEnd && w->first == L"(");   // The bag dropped.  (If you didn't know).
		// keep names like al-Jazeera or dashed words incorporating first words that are unknown like fierro-regni or Jay-Z
		// preserve dashes by setting insertDashes to true.
		// Handle cases like 10-15 or 10-60,000 which are returned as PARSE_NUM
		if (dash != wstring::npos && result == 0 && // if (not date or number)
			sWord[1] != 0 && // not a single dash
			!cWord::isDash(sWord[dash + 1]) && // not '--'
			!(sWord[0] == L'a' && cWord::isDash(sWord[1]) && numDash == 1) // a-working
			 // state-of-the-art
			)
		{
			wstring lowerWord = sWord;
			transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), (int(*)(int)) tolower);
			unsigned int unknownWords = 0, capitalizedWords = 0, openWords = 0, letters = 0;
			vector <wstring> dashedWords = splitString(sWord, sWord[dash]);
			wstring removeDashesWord;
			for (wstring subWord : dashedWords)
			{
				wstring lowerSubWord = subWord;
				transform(lowerSubWord.begin(), lowerSubWord.end(), lowerSubWord.begin(), (int(*)(int)) tolower);
				tIWMM iWord = cWord::fullQuery(&mysql, lowerSubWord, sourceId);
				if (iWord == cWord::end() || iWord->second.query(UNDEFINED_FORM_NUM) >= 0)
					unknownWords++;
				if (iWord != cWord::end() && cStemmer::wordIsNotUnknownAndOpen(iWord, debugTrace.traceParseInfo))
					openWords++;
				if (subWord.length() > 1 && iswupper(subWord[0]) && !iswupper(subWord[1]))  // al-Jazeera, not BALL-PLAYING
					capitalizedWords++;
				if (subWord.length() == 1)  // F-R-E-E-D-O-M  
					letters++;
				removeDashesWord += lowerSubWord;
			}
			// if this does not qualify as a dashed word, back up to just AFTER the dash
			tIWMM iWord = cWord::fullQuery(&mysql, lowerWord, sourceId);
			tIWMM iWordNoDashes = cWord::fullQuery(&mysql, removeDashesWord, sourceId);
			//bool notCapitalized = (capitalizedWords == 0 || (capitalizedWords == 1 && firstWordInSentence));
			// break apart if it is not capitalized, there are no unknown words, and the dashed word is unknown.
			// however, there may be cases like I-THE, which webster/dictionary.com incorrectly say are defined.
			if ((unknownWords <= capitalizedWords || unknownWords <= dashedWords.size() / 2) &&
				(iWord == Words.end() || iWord->second.query(UNDEFINED_FORM_NUM) >= 0) &&
				(capitalizedWords!=1 || letters!=1 || dashedWords.size()!=2 || dashedWords[1].length()!=1) && // keep Jay-Z
				(iWordNoDashes == Words.end() || iWordNoDashes->second.query(UNDEFINED_FORM_NUM) >= 0 || letters != dashedWords.size() || letters < 3))
			{
				bufferScanLocation -= sWord.length() - firstDash;
				sWord.erase(firstDash, sWord.length() - firstDash);
			}
			else if (sWord != L"--" && debugTrace.traceParseInfo)
				lplog(LOG_INFO, L"%s NOT split (#unknownWords=%d #capitalizedWords=%d #letters=%d %s).", sWord.c_str(), unknownWords, capitalizedWords, letters, ((iWord == Words.end() || iWord->second.query(UNDEFINED_FORM_NUM) >= 0)) ? L"UNKNOWN" : L"NOT unknown");
			firstLetterCapitalized = (capitalizedWords > 0);
		}
		bool allCaps = Words.isAllUpper(sWord);
		wcslwr((wchar_t *)sWord.c_str());
		bool added;
		bool endSentence = result == PARSE_END_SENTENCE;
		if (endSentence && m.size() && sWord == L".")
		{
			wchar_t *abbreviationForms[] = { L"letter",L"abbreviation",L"measurement_abbreviation",L"street_address_abbreviation",L"business_abbreviation",
				L"time_abbreviation",L"date_abbreviation",L"honorific_abbreviation",L"trademark",L"pagenum",NULL };
			for (unsigned int af = 0; abbreviationForms[af] && endSentence; af++)
				if (m[m.size() - 1].queryForm(abbreviationForms[af]) >= 0)
					endSentence = false;
		}
		tIWMM iWord = Words.end();
		if (result == PARSE_NUM)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, NUMBER_FORM_NUM, 0, 0, L"", sourceId, added);
		else if (result == PARSE_PLURAL_NUM)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, NUMBER_FORM_NUM, PLURAL, 0, L"", sourceId, added);
		else if (result == PARSE_ORD_NUM)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, numeralOrdinalForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_ADVERB_NUM)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, adverbForm, 0, 0, sWord, sourceId, added);
		else if (result == PARSE_DATE)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, dateForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_TIME)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, timeForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_TELEPHONE_NUMBER)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, telephoneNumberForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_MONEY_NUM)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, moneyForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_WEB_ADDRESS)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, webAddressForm, 0, 0, L"", sourceId, added);
		else if (result < 0 && result != PARSE_END_SENTENCE)
			break;
		else
			if ((result = Words.parseWord(&mysql, sWord, iWord, firstLetterCapitalized, nounOwner, sourceId, debugTrace.traceParseInfo)) < 0)
				break;
		result = 0;
		if (iWord->second.isUnknown())
		{
			lplog(LOG_INFO, L"UNKNOWN: %s", sWord.c_str());
			unknownCount++;
		}
		unsigned __int64 flags;
		iWord->second.adjustFormsInflections(sWord, flags, firstWordInSentence, nounOwner, allCaps, firstLetterCapitalized, false);
		if (allCaps && m.size() && ((m[m.size() - 1].word->first == L"the" && !(m[m.size() - 1].flags&cWordMatch::flagAllCaps)) || iWord->second.formsSize() == 0))
		{
			flags |= cWordMatch::flagAddProperNoun;
			iWord->second.zeroNewProperNounCostIfUsedAllCaps();
#ifdef LOG_PATTERN_COST_CHECK
			::lplog(L"Added ProperNoun [from the] to word %s (form #%d) at cost %d.", originalWord.c_str(), formsSize(), usageCosts[formsSize()]);
#endif
		}
		if ((flags&cWordMatch::flagAddProperNoun) && debugTrace.traceParseInfo)
			lplog(LOG_INFO, L"%d:%s:added flagAddProperNoun.", m.size(), sWord.c_str());
		if ((flags&cWordMatch::flagOnlyConsiderProperNounForms) && debugTrace.traceParseInfo)
			lplog(LOG_INFO, L"%d:%s:added flagOnlyConsiderProperNounForms.", m.size(), sWord.c_str());
		if ((flags&cWordMatch::flagOnlyConsiderOtherNounForms) && debugTrace.traceParseInfo)
			lplog(LOG_INFO, L"%d:%s:added flagOnlyConsiderOtherNounForms.", m.size(), sWord.c_str());
		// does not necessarily have to be a proper noun after a word with a . at the end (P.N.C.)
		// The description of a green toque , a coat with a handkerchief in the pocket marked P.L.C. He looked an agonized question at Mr . Carter .
		if ((flags&cWordMatch::flagOnlyConsiderProperNounForms) && m.size() && m[m.size() - 1].word->first.length() > 1 && m[m.size() - 1].word->first[m[m.size() - 1].word->first.length() - 1] == L'.')
		{
			flags &= ~cWordMatch::flagOnlyConsiderProperNounForms;
			if (debugTrace.traceParseInfo)
				lplog(LOG_INFO, L"%d:removed flagOnlyConsiderProperNounForms.", m.size());
		}
		// if a word is capitalized, but is always followed by another word that is also capitalized, then 
		// don't treat it as an automatic proper noun ('New' York)
		bool isProperNoun = (flags&cWordMatch::flagAddProperNoun) && !allCaps && !firstWordInSentence;
		if (isProperNoun && previousIsProperNoun)
		{
			m[m.size() - 1].word->second.numProperNounUsageAsAdjective++;
			if (debugTrace.traceParseInfo)
				lplog(LOG_INFO, L"%d:%s:increased usage of proper noun as adjective to %d.", m.size() - 1, m[m.size() - 1].word->first.c_str(), m[m.size() - 1].word->second.numProperNounUsageAsAdjective);
		}
		previousIsProperNoun = isProperNoun;
		// used in disambiguating abbreviated quotes from nested quotes
		if (sWord == secondaryQuoteType)
		{
			if (flagAlphaBeforeHint) flags |= cWordMatch::flagAlphaBeforeHint;
			if (flagAlphaAfterHint) flags |= cWordMatch::flagAlphaAfterHint;
		}
		if (sourceType == NEWS_BANK_SOURCE_TYPE && numParagraphsInSection < 3)
			flags |= cWordMatch::flagMetaData;
		// The description of a green toque , a coat with a handkerchief in the pocket marked P.L.C. He looked an agonized question at Mr . Carter .
		if (firstLetterCapitalized && (sWord == L"he" || sWord == L"she" || sWord == L"it" || sWord == L"they" || sWord == L"we" || sWord == L"you") && m.size() &&
			m[m.size() - 1].word->first.length() > 1 && m[m.size() - 1].word->first[m[m.size() - 1].word->first.length() - 1] == L'.')
		{
			m[m.size() - 1].word->second.flags |= cSourceWordInfo::topLevelSeparator;
			sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd = m.size();
		}
		if (flagNewLineBeforeHint) flags |= cWordMatch::flagNewLineBeforeHint;
		m.emplace_back(iWord, flags, debugTrace);
		// check for By artist
		if (webScrapeParse && m.size() > 2 && m[m.size() - 3].word->first == L"by" && (m[m.size() - 3].flags&cWordMatch::flagNewLineBeforeHint) &&
			(m[m.size() - 1].flags&cWordMatch::flagFirstLetterCapitalized) && (m[m.size() - 2].flags&cWordMatch::flagFirstLetterCapitalized))
		{
			sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd = m.size() + 1;
			m.push_back(cWordMatch(Words.sectionWord, 0, debugTrace));
		}
		// Last modified: 2012-01-29T21:38:56Z
		// Published: Saturday, Jan. 28, 2012 - 12:00 am | Page 11A
		// Last Modified: Sunday, Jan. 29, 2012 - 1:38 pm
		if (webScrapeParse && m.size() > 2 && (m[m.size() - 3].flags&cWordMatch::flagNewLineBeforeHint) &&
			(m[m.size() - 1].flags&cWordMatch::flagFirstLetterCapitalized) && (m[m.size() - 2].flags&cWordMatch::flagFirstLetterCapitalized) && bookBuffer[bufferScanLocation] == L':')
		{
			sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd = m.size() + 1;
			m.push_back(cWordMatch(Words.sectionWord, 0, debugTrace));
		}
		int numWords = m.size() - lastSentenceEnd;
		if (numWords > 150 || (numWords > 100 && (iWord->second.query(conjunctionForm) >= 0 || iWord->first == L";")))
		{
			lplog(LOG_ERROR, L"ERROR:Terminating run-on sentence at word %d in source %s (word offset %d).", numWords, path.c_str(), m.size());
			runOnSentences++;
			endSentence = true;
		}
		if (endSentence)
		{
			if (analyzeEnd(path, lastSentenceEnd, m.size(), multipleEnds))
			{
				m.erase(m.begin() + lastSentenceEnd, m.end());
				alreadyAtEnd = true;
				break;
			}
			if (sentenceStarts.size() && lastSentenceEnd == sentenceStarts[sentenceStarts.size() - 1] + 1)
				sentenceStarts[sentenceStarts.size() - 1]++;
			else
				sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd = m.size();
			if ((int)(bufferScanLocation * 100 / bufferLen) > lastProgressPercent)
			{
				lastProgressPercent = (int)(bufferScanLocation * 100 / bufferLen);
				wprintf(L"PROGRESS: %03d%% (%06zu words) %I64d out of %I64d bytes read with %d seconds elapsed (%I64d bytes) \r", lastProgressPercent, m.size(), bufferScanLocation, bufferLen, clocksec(), memoryAllocated);
			}
			continue;
		}
	}
	if (!alreadyAtEnd && analyzeEnd(path, lastSentenceEnd, m.size(), multipleEnds))
	{
		m.erase(m.begin() + lastSentenceEnd, m.end());
	}
	else
		sentenceStarts.push_back(lastSentenceEnd);
	if (bufferLen > 0)
	{
		lastProgressPercent = (int)(bufferScanLocation * 100 / bufferLen);
		wprintf(L"PROGRESS: %03d%% (%06zu words) %I64d out of %I64d bytes read with %d seconds elapsed (%I64d bytes) \r", lastProgressPercent, m.size(), bufferScanLocation, bufferLen, clocksec(), memoryAllocated);
		if (runOnSentences > 0)
			lplog(LOG_ERROR, L"ERROR:%s:%d sentence early terminations (%d%%)...", path.c_str(), runOnSentences, 100 * runOnSentences / sentenceStarts.size());
	}
	return 0;
}

int cSource::tokenize(wstring title, wstring etext, wstring path, wstring encoding, wstring &start, int &repeatStart, unsigned int &unknownCount)
{
	LFS
		int ret = 0;
	if ((ret = readSourceBuffer(title, etext, path, encoding, start, repeatStart)) >= 0)
	{
		if (wcsncmp(bookBuffer, L"%PDF-", wcslen(L"%PDF-")))
			ret = parseBuffer(path, unknownCount);
		else
			lplog(LOG_ERROR, L"%s: Skipped parsing PDF file", path.c_str());
		tfree((int)bufferLen, bookBuffer);
		bufferScanLocation = bufferLen = 0;
		bookBuffer = NULL;
	}
	return ret;
}

