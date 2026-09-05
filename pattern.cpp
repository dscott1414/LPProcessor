/*
	pattern.cpp - pattern match engine, create/resolve, tag-set eval, tag lookup

	Overview:
		The runtime half of stage 4.  initializePatterns() (called once at
		startup) runs every create* in definePatterns.cpp, resolves
		cPatternReference forward refs, then walks the pattern graph to fill
		ancestor bitsets and descendant tag sets.  Per sentence,
		matchPatternPosition() tries one pattern at one source position:
		matchFirst/matchRange/matchOne build a linked whatMatched chain, then
		fillPattern copies a successful chain into that token's PMA and the
		document PEMA.  A second pass re-matches only "new" children so
		forward-referenced subpatterns can cheapen their parents
		(reduceParents).  After all patterns have been tried,
		eliminateLoserPatterns (in source.cpp) costs and winnows; this file
		also owns the tag-set tables and the findTag* helpers later stages use
		to read the surviving match tree.

	Pipeline position:
		Stage 1 init (initializePatterns) and stage 4 parse (match/fill).
		cSource::matchPatternsAgainstSentence is the usual caller of
		matchPatternPosition.  findTag / findTagConstrained / queryPattern
		are used from relations, objects, speakers and QA.

	Key entry points:
		- initializePatterns() / initializeTagSets()
		- cPattern::create() (va_list and the dynamic QA transform overload)
		- cPattern::matchPatternPosition() / fillPattern()
		- cPatternElement::matchOne / matchFirst / matchRange / inflectionMatch
		- findPattern / findTag / findTagConstrained / findOneTag
		- cPattern::isTopLevelMatch / evaluateTagSets / add / resolveDescendants

	Key data structures / globals:
		- patterns / patternReferences / patternTagStrings / desiredTagSets
		- overMatchMemoryExceeded - latched when whatMatched exceeds
		  MAX_PATTERN_NUM_MATCH (4000); matchOne then returns -2
		- verbObjectsTagSet, subjectTagSet, roleTagSet, ... - indexes into
		  desiredTagSets, filled by initializeTagSets
		- PREP_TAG / OBJECT_TAG / ... - interned tag ids for hot paths

	Notes / gotchas:
		- findPattern(name, starting) is the sole surviving overload; it returns
		  patterns.size() on miss, never -1 (the unused lpwstring-only and
		  (name,diff)/-1-sentinel overloads had zero callers and were removed).
		- The #ifdef ABNF read/write path does not compile against current
		  members and is not on the live init path.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include "profile.h"

vector <cPattern*> patterns; // initialized
int cPatternReference::firstPatternReference = -1, cPatternReference::lastPatternReference = -1; // initialized
vector <cPatternReference*> patternReferences; // initialized
cBitObject<32, 5, unsigned int, 32> patternsWithNoParents, patternsWithNoChildren; // initialized
bool overMatchMemoryExceeded = false; // used only in error - one way 
vector < cTagSet > desiredTagSets; // initialized
vector <lpwstring> patternTagStrings; // initialized

struct {
	int flag;
	const lpchar_t* sFlag;
} inflectionFlagList[] =
{
	{SINGULAR,u"SINGULAR"},
	{PLURAL,u"PLURAL"},
	{SINGULAR_OWNER,u"SINGULAR_OWNER"},
	{PLURAL_OWNER,u"PLURAL_OWNER"},
	{VERB_PAST,u"VERB_PAST"},
	{VERB_PAST_PARTICIPLE,u"VERB_PAST_PARTICIPLE"},
	{VERB_PRESENT_PARTICIPLE,u"VERB_PRESENT_PARTICIPLE"},
	{VERB_PRESENT_THIRD_SINGULAR,u"VERB_PRESENT_THIRD_SINGULAR"},
	{VERB_PRESENT_FIRST_SINGULAR,u"VERB_PRESENT_FIRST_SINGULAR"},
	{VERB_PAST_THIRD_SINGULAR,u"VERB_PAST_THIRD_SINGULAR"},
	{VERB_PAST_PLURAL,u"VERB_PAST_PLURAL"},
	{VERB_PRESENT_PLURAL,u"VERB_PRESENT_PLURAL"},
	{VERB_PRESENT_SECOND_SINGULAR,u"VERB_PRESENT_SECOND_SINGULAR"},
	{ADJECTIVE_NORMATIVE,u"ADJECTIVE_NORMATIVE"},
	{ADJECTIVE_COMPARATIVE,u"ADJECTIVE_COMPARATIVE"},
	{ADJECTIVE_SUPERLATIVE,u"ADJECTIVE_SUPERLATIVE"},
	{ADVERB_NORMATIVE,u"ADVERB_NORMATIVE"},
	{ADVERB_COMPARATIVE,u"ADVERB_COMPARATIVE"},
	{ADVERB_SUPERLATIVE,u"ADVERB_SUPERLATIVE"},
	{MALE_GENDER,u"MALE_GENDER"},
	{FEMALE_GENDER,u"FEMALE_GENDER"},
	{NEUTER_GENDER,u"NEUTER_GENDER"},
	{FIRST_PERSON,u"FIRST_PERSON"},
	{SECOND_PERSON,u"SECOND_PERSON"},
	{THIRD_PERSON,u"THIRD_PERSON"},
	{NO_OWNER,u"NO_OWNER"},
	{OPEN_INFLECTION,u"OPEN_INFLECTION"},
	{CLOSE_INFLECTION,u"CLOSE_INFLECTION"},
	{FEMALE_GENDER_ONLY_CAPITALIZED,u"FEMALE_GENDER_ONLY_CAPITALIZED"},
	{MALE_GENDER_ONLY_CAPITALIZED,u"MALE_GENDER_ONLY_CAPITALIZED"},
{-1,NULL}
};

// Child pattern # packed in elementMatchedIndex (bits 15-30).  Only valid when
// patternFlag (bit 31) is set.
unsigned int cMatchElement::getChildPattern()
{
	LFS
		return cPatternElementMatchArray::PATMASK(elementMatchedIndex);
};

// Child match length packed in elementMatchedIndex (bits 0-14).
unsigned int cMatchElement::getChildLen()
{
	LFS
		return cPatternElementMatchArray::ENDMASK(elementMatchedIndex);
};


// Walk the previousMatch chain from elementMatched back to the start and
// format each step into s (used only under LOG_PATTERN_MATCHING).
lpwstring matchesToString(vector <cMatchElement>& whatMatched, int elementMatched, lpwstring& s)
{
	LFS
		lpchar_t temp[100];
	s.clear();
	while (elementMatched >= 0)
	{
		if (whatMatched[elementMatched].elementMatchedIndex & cMatchElement::patternFlag)
		{
			if (whatMatched[elementMatched].getChildPattern() >= patterns.size())
				lplog(LOG_FATAL_ERROR, u"Illegal pattern #%d at elementMatched %d!", whatMatched[elementMatched].getChildPattern(), elementMatched);
			lp_snprintf(temp, 100, u"(%u %s[%s] (childRelEnd=%u, cost=%d)) ", whatMatched[elementMatched].endPosition,
				patterns[whatMatched[elementMatched].getChildPattern()]->name.c_str(),
				patterns[whatMatched[elementMatched].getChildPattern()]->differentiator.c_str(),
				whatMatched[elementMatched].getChildLen(), whatMatched[elementMatched].cost);
		}
		else
			lp_snprintf(temp, 100, u"(%u %u) ", whatMatched[elementMatched].endPosition, whatMatched[elementMatched].elementMatchedIndex);
		elementMatched = whatMatched[elementMatched].previousMatch;
		s = temp + s;
	}
	return s;
}

// Match this element against every already-successful prefix in
// whatMatched[matchBegin, matchEnd).  If minimum==0, also push a zero-width
// "skipped" placeholder for each prefix.  Then repeat up to `maximum` times,
// each round matching from the previous round's new entries.  Returns true if
// at least one new match was produced, or if the element is optional.
bool cPatternElement::matchRange(cSource& source, int matchBegin, int matchEnd, vector <cMatchElement>& whatMatched, sTrace& t)
{
	LFS // DLFS
		int rep;
	if (minimum == 0)
	{
		for (int matchPosition = matchBegin; matchPosition < matchEnd; matchPosition++)
		{
			whatMatched.push_back(cMatchElement(-1, whatMatched[matchPosition].endPosition, whatMatched[matchPosition].endPosition, 0, 0, false, matchPosition, elementPosition, -2, false));
#ifdef LOG_PATTERN_MATCHING
			if (t.tracePatternMatching)
			{
				lpwstring s;
				::lplog(u"%d:pattern %s:matchPosition %d:whatMatched #%d:insert as though no match (minimum==0) matchString inserted:%s",
					whatMatched[matchPosition].endPosition, patternName.c_str(), elementPosition, whatMatched.size() - 1, matchesToString(whatMatched, whatMatched.size() - 1, s).c_str());
			}
#endif
		}
	}
	for (rep = 0; rep < maximum && matchBegin != matchEnd; rep++)
	{
		int saveBegin = whatMatched.size();
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
		{
			if (rep > 0)
				::lplog(u"   pattern %s:matchPosition %d:starting rep %d at whatMatched index %d.",
					patternName.c_str(), elementPosition, rep, saveBegin);
		}
#endif
		for (int matchPosition = matchBegin; matchPosition < matchEnd; matchPosition++)
		{
			matchOne(source, whatMatched[matchPosition].endPosition, matchPosition, whatMatched, t);
		}
		matchBegin = saveBegin;
		matchEnd = whatMatched.size();
	}
	if (matchBegin != matchEnd)
	{
		endPosition = whatMatched[whatMatched.size() - 1].endPosition;
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching && patternName.empty())
			::lplog(LOG_WHERE, u"   pattern %s:matchPosition %d:ending at endPosition %d.",
				patternName.c_str(), elementPosition, endPosition);
#endif
	}
	return matchBegin != matchEnd || rep > 1 || minimum == 0; // if no matches, only return success if the element is optional
}

// First-element variant of matchRange: seeds whatMatched from sourcePosition
// rather than from prior prefixes.  Returns false only when the element is
// mandatory and matchOne failed.  A sole optional-skip (size==1, minimum==0)
// is success with endPosition=-1 so later elements start at the same token.
bool cPatternElement::matchFirst(cSource& source, int sourcePosition, vector <cMatchElement>& whatMatched, sTrace& t)
{
	LFS
		int rep;
	if (minimum == 0)
	{
		// if minimum==0 and not matched (this is a placeholder) then the cost should be 0.
		whatMatched.push_back(cMatchElement(-1, sourcePosition, sourcePosition, 0, 0, false, -1, elementPosition, -3, false));  // sourcePosition matched, elementMatched, patternFlag, previous match matchPosition
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
		{
			lpwstring s;
			lplog(u"%d:pattern %s:matchPosition %d:whatMatched #%d:insert as though no match (minimum==0) matchString inserted:%s",
				sourcePosition, patternName.c_str(), elementPosition, whatMatched.size() - 1, matchesToString(whatMatched, whatMatched.size() - 1, s).c_str());
		}
#endif
	}
	int retCode = 0;
	// match the first matchPosition once
	if ((retCode = matchOne(source, sourcePosition, -1, whatMatched, t)) < 0)
	{
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
			lplog(u"%d:pattern %s:matchPosition %d:[%d]:Match failed", sourcePosition, patternName.c_str(), elementPosition, retCode);
#endif
		endPosition = -1;
		return false;
	}
	int matchBegin, matchEnd = whatMatched.size();
	// what Matched size = 1
	//   minimum==0 match of element did not succeed.  skip entirely, as further reps will also fail.
	//   minimum!=0 match of element did succeed. start with matchBegin=0 to check for further reps from the initial rep.
	// what matched size = 2
	//   minimum==0 - both 0 and 1 reps matched.  now start with matchBegin=1 to avoid matching the first element twice
	//   minimum!=0 match of two or more different combinations did succeed. start with matchBegin=0 to check for further reps.
	if (matchEnd == 1 && minimum == 0)
	{
		endPosition = -1;
		return true; // succeeded only with no match
	}
	matchBegin = (matchEnd == 1 || minimum) ? 0 : 1;
	if (consolidateEndPositions)
		endPositionsSet.clear();
	for (rep = 1; rep < maximum && matchBegin != matchEnd; rep++)
	{
		int saveBegin = whatMatched.size();
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
			::lplog(u"   pattern %s:matchPosition %d:starting rep %d at whatMatched index %d.",
				patternName.c_str(), elementPosition, rep, saveBegin);
#endif
		for (int matchPosition = matchBegin; matchPosition < matchEnd; matchPosition++)
		{
			if (!consolidateEndPositions || endPositionsSet.find(whatMatched[matchPosition].endPosition) == endPositionsSet.end())
			{
				matchOne(source, whatMatched[matchPosition].endPosition, matchPosition, whatMatched, t);
				if (consolidateEndPositions)
					endPositionsSet.insert(whatMatched[matchPosition].endPosition);
			}
		}
		matchBegin = saveBegin;
		matchEnd = whatMatched.size();
	}
	endPosition = whatMatched[whatMatched.size() - 1].endPosition;
#ifdef LOG_PATTERN_MATCHING
	if (t.tracePatternMatching && patternName.empty())
		::lplog(u"   pattern %s:matchPosition %d:ending at endPosition %d.",
			patternName.c_str(), elementPosition, endPosition);
#endif
	return true;
}

// inflectionFlagsFromWord are the inflections of the word in the source
// flags are the flags to match in the patternElement.
// sourceFormStr is the form of the word of the source
// True if the word's inflection is compatible with this element's required
// inflectionFlags for the given form class.  Nouns marked flagNounOwner are
// rewritten to SINGULAR_OWNER/PLURAL_OWNER; NO_OWNER then rejects them.
// ONLY_CAPITALIZED rejects a non-capitalized token.  If the element imposes
// no flags for this class, the class is accepted unconditionally.
bool cPatternElement::inflectionMatch(int inflectionFlagsFromWord, int64_t flags, lpwstring sourceFormStr, sTrace& t)
{
	LFS
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
		{
			lpwstring sInflectionFlagsFromWord, sInflectionFlags;
			::lplog(u"   pattern %s:inflectionMatch:inflectionFlagsFromWord=%s inflectionFlags=%s flags owner=%s capitalized=%s sourceFormStr=%s",
				patternName.c_str(), ::inflectionFlagsToStr(inflectionFlagsFromWord, sInflectionFlagsFromWord), ::inflectionFlagsToStr(inflectionFlags, sInflectionFlags),
				(flags & cWordMatch::flagNounOwner) ? u"TRUE" : u"FALSE",
				(flags & (cWordMatch::flagAllCaps | cWordMatch::flagFirstLetterCapitalized)) ? u"TRUE" : u"FALSE",
				sourceFormStr.c_str());
		}
#endif
	if (sourceFormStr == u"noun" && (flags & cWordMatch::flagNounOwner))
	{
		if (inflectionFlagsFromWord & SINGULAR)
			inflectionFlagsFromWord = SINGULAR_OWNER | (inflectionFlagsFromWord & ~SINGULAR);
		else if (inflectionFlagsFromWord & PLURAL)
			inflectionFlagsFromWord = PLURAL_OWNER | (inflectionFlagsFromWord & ~PLURAL);
		else inflectionFlagsFromWord |= SINGULAR_OWNER;
		// if word is owner, but element flags doesn't specify that, fail!  Gives ability to specify __N1 and __NAMEs do NOT match ownership nouns.
		if (inflectionFlags & NO_OWNER) return false;
	}
	if ((inflectionFlags & ONLY_CAPITALIZED) && !(flags & (cWordMatch::flagAllCaps | cWordMatch::flagFirstLetterCapitalized)))
		return false;
	int matchingInflectionFlags = (inflectionFlags & inflectionFlagsFromWord);
	if ((sourceFormStr == u"verb" && (matchingInflectionFlags & VERB_INFLECTIONS_MASK) != 0) ||
		(sourceFormStr == u"noun" && (matchingInflectionFlags & NOUN_INFLECTIONS_MASK) != 0) ||
		(sourceFormStr == u"adjective" && (matchingInflectionFlags & ADJECTIVE_INFLECTIONS_MASK) != 0) ||
		(sourceFormStr == u"adverb" && (matchingInflectionFlags & ADVERB_INFLECTIONS_MASK) != 0) ||
		(sourceFormStr == u"quotes" && (matchingInflectionFlags & INFLECTIONS_MASK) != 0) ||
		(sourceFormStr == u"brackets" && (matchingInflectionFlags & INFLECTIONS_MASK) != 0))
		return true;
	// if there are NO flags in the pattern element restricting the inflection of the for given by sourceFormStr, return true.  
	//  otherwise if the inflectionFlags don't corespond at all with the inflectionFlags from the word, then return false. (the statement directly above).
	return  (sourceFormStr == u"verb" && !(inflectionFlags & VERB_INFLECTIONS_MASK)) ||
		(sourceFormStr == u"noun" && !(inflectionFlags & NOUN_INFLECTIONS_MASK)) ||
		(sourceFormStr == u"adjective" && !(inflectionFlags & ADJECTIVE_INFLECTIONS_MASK)) ||
		(sourceFormStr == u"adverb" && !(inflectionFlags & ADVERB_INFLECTIONS_MASK)) ||
		(sourceFormStr == u"quotes" && !(inflectionFlags & INFLECTIONS_MASK)) ||
		(sourceFormStr == u"brackets" && !(inflectionFlags & INFLECTIONS_MASK));
}

#define MAX_PATTERN_NUM_MATCH 4000 // Charles Dickens may require a lower limit.
// because endPositionMatches is being pushed back into whatMatched, endPositionMatches must be a COPY
// of the position of whatMatched it started out as.  Do not pass endPositionMatches as a reference.
// Try every form alternative, then every child-pattern alternative, at
// sourcePosition.  Each hit is pushed onto whatMatched linked via lastElement.
// Return 1 if anything matched; 2 if optional (minimum==0) and the word is not
// ignorable; -1 if past end of source; -2 if whatMatched exceeded
// MAX_PATTERN_NUM_MATCH; -3 if mandatory and no form/pattern hit; -4 if the
// only remaining move would ignore a word at the start of the match.  An
// ignorable token (dash etc.) recurses at sourcePosition+1 unless
// checkIgnorableForms is set.  A specific-word alternative with cost>0
// suppresses further pattern alternatives (skipPatternMatchingBecauseSpecificWordMatched).
int cPatternElement::matchOne(cSource& source, unsigned int sourcePosition, unsigned int lastElement, vector <cMatchElement>& whatMatched, sTrace& t)
{
	LFS
		if (sourcePosition >= source.m.size())
		{
#ifdef LOG_PATTERN_MATCHING
			if (t.tracePatternMatching)
				lplog(u"%d:pattern %s:position %d:No more words available",
					sourcePosition, patternName.c_str(), elementPosition);
#endif
			return -1;
		}
	bool oneMatch = false, skipPatternMatchingBecauseSpecificWordMatched = false;
	vector <cWordMatch>::iterator im = source.m.begin() + sourcePosition;
	for (unsigned int form = 0; form < formIndexes.size(); form++)
	{
		unsigned int f = formIndexes[form];
		if (!im->forms.isSet(f))
		{
#ifdef LOG_PATTERN_MATCHING
			if (t.tracePatternMatching)
				lplog(u"%d:pattern %s:position %d:No match for form %s", sourcePosition, patternName.c_str(), elementPosition,
					Forms[f]->name.c_str());
#endif
		}
		else
		{
			if (
				(!inflectionFlags ||
					(Forms[f]->inflectionsClass.empty() && !((im->flags & cWordMatch::flagNounOwner) && (inflectionFlags & NO_OWNER)) && !((im->flags & (cWordMatch::flagAllCaps | cWordMatch::flagFirstLetterCapitalized)) && (inflectionFlags & ONLY_CAPITALIZED))) ||
					inflectionMatch(im->word->second.inflectionFlags, im->flags, Forms[f]->inflectionsClass, t)) &&
				(specificWords[form].empty() || im->word->first == specificWords[form] || (im->word->second.mainEntry != wNULL && im->word->second.mainEntry->first == specificWords[form])))
			{
				int ME = im->queryForm(f);
				if (ME < 0) continue;
				// if this is a pattern match phase which is after the main phase,
				// such as META_NAME_EQUIVALENCE or other, then only match against the forms which won
				// in the main phase (unless the form = the word itself).
				if (patterns[patternNum]->ignoreFlag && !im->isWinner(ME) && im->word->first != Forms[f]->name)
					continue;
				int cost = formCosts[form];
				if (!preTaggedSource && im->costable())
					cost += im->word->second.getUsageCost(ME);
				//if (f==PROPER_NOUN_FORM_NUM && (im->flags&(cWordMatch::flagAllCaps|cWordMatch::flagAddProperNoun))==(cWordMatch::flagAllCaps|cWordMatch::flagAddProperNoun)) // make this proper noun a little expensive
				//  cost++;
				// Bare "Dr" / "Mr" without a following '.' or a capital is usually
				// not an abbreviation; the +5/+10 push a cheaper non-abbrev form.
				if (f == abbreviationForm && sourcePosition + 1 < source.m.size() && source.m[sourcePosition + 1].word->first[0] != u'.')
					cost += 5;
				if (f == abbreviationForm && !(im->flags & (cWordMatch::flagAllCaps | cWordMatch::flagFirstLetterCapitalized)))
					cost += 10;
				// refuse any other form for this very common word if the next word is not punctuation.
				if (im->word->first == u"i" && f != nomForm &&
					sourcePosition + 1 < source.m.size() && (signed)source.m[sourcePosition + 1].word->first[0] > 0 && iswalpha(source.m[sourcePosition + 1].word->first[0]) &&
					(sourcePosition == 0 || (source.m[sourcePosition - 1].word->first != u"chapter" && source.m[sourcePosition - 1].word->first != u"book")))
					continue;
				if (im->word->first == u"a" && f != determinerForm &&
					sourcePosition + 1 < source.m.size() && (signed)source.m[sourcePosition + 1].word->first[0] > 0 &&
					(iswalpha(source.m[sourcePosition + 1].word->first[0]) || source.m[sourcePosition + 1].queryForm(quoteForm) >= 0) && source.m[sourcePosition + 1].word->first != u"is" && (sourcePosition == 0 || source.m[sourcePosition - 1].word->first != u"letter"))
					continue;
				if ((im->word->first == u"if" || im->word->first == u"and" || im->word->first == u"but") && (f == nounForm || f == adjectiveForm || f == PROPER_NOUN_FORM_NUM || f == abbreviationForm) &&
					!(im->word->second.inflectionFlags & PLURAL))
					cost += 10;
				whatMatched.push_back(cMatchElement(-1, sourcePosition, sourcePosition + 1, ME, cost, false, lastElement, elementPosition, form, false));
				oneMatch = true;
#ifdef LOG_PATTERN_MATCHING
				if (t.tracePatternMatching)
				{
					lpwstring s;
					lplog(u"%d:pattern %s:position %d:whatMatched #%d:form %s::%s found. matchString inserted:%s",
						sourcePosition, patternName.c_str(), elementPosition, whatMatched.size() - 1,
						Forms[f]->name.c_str(), specificWords[form].c_str(), matchesToString(whatMatched, whatMatched.size() - 1, s).c_str());
				}
#endif
				// if a word is specifically marked as a match, no more matches will be searched.  This allows a specific word to over-rule a more general search form (own, in ALLOBJECTS_2[2])
				if (skipPatternMatchingBecauseSpecificWordMatched = specificWords[form].length() > 0)
				{
					// IF the cost of the specific word named is 0 or negative, then assume the intent is to make the lowest cost option this particular word (thus matching more patterns won't hurt).
					// IF the cost is positive, then assume that the intent is to make this word more expensive and thus do not match more patterns when they will match this word without more cost.
					if (formCosts[form] <= 0)
						skipPatternMatchingBecauseSpecificWordMatched = false;
					break;
				}
			}
#ifdef LOG_PATTERN_MATCHING
			else if (t.tracePatternMatching)
			{
				lpwstring inflectionName, inflectionName2;
				lplog(u"%d:pattern %s:position %d:Form %s:%s found inflective%s but did not match inflective%s (or form).", sourcePosition, patternName.c_str(), elementPosition,
					Forms[f]->name.c_str(), specificWords[form].c_str(), getInflectionName(im->word->second.inflectionFlags, f, inflectionName),
					getInflectionName(inflectionFlags, f, inflectionName2));
			}
#endif
		}
	}
	if (!skipPatternMatchingBecauseSpecificWordMatched)
		for (unsigned int pattern = 0; pattern < patternIndexes.size(); pattern++)
		{
			unsigned int p = patternIndexes[pattern];
			if (im->patterns.isSet(p))
			{
				cPatternMatchArray::tPatternMatch* pm = im->pma.content;
				int startPositions = whatMatched.size(), msize = startPositions;
				if (msize > MAX_PATTERN_NUM_MATCH)
				{
					if (!overMatchMemoryExceeded && t.traceParseInfo)
						lplog(LOG_ERROR, u"ERROR:%d:pattern %s[%s]:position %d:Match size array reached over %d entries",
							sourcePosition, patternName.c_str(), patterns[patternNum]->differentiator.c_str(), elementPosition, msize);
					im->maxLACMatch = -1;
					overMatchMemoryExceeded = true;
					return -2;
				}
				unsigned int count = im->pma.count;
				patterns[patternNum]->numComparisons += count;
				unsigned int I = 0;
				for (; I < count && pm->getPattern() < p; I++, pm++);
				for (; I < count && pm->getPattern() == p; I++, pm++)
				{
					patterns[p]->numHits++;
					bool found = false;
					for (int m = msize - 1; m >= startPositions && !found; m--)
						found = (whatMatched[m].endPosition == (pm->len + sourcePosition)); // avoid saving duplicate end positions for the same pattern
					if (!found)
					{
						if (patterns[p]->noRepeat && lastElement != -1 &&
							patterns[whatMatched[lastElement].getChildPattern()]->rootPattern == patterns[p]->rootPattern &&
							patterns[whatMatched[lastElement].getChildPattern()]->noRepeat)
						{
#ifdef LOG_PATTERN_MATCHING
							if (t.tracePatternMatching)
								lplog(u"%d:pattern %s:position %d:No REPEAT match for pattern %s", sourcePosition, patternName.c_str(), elementPosition,
									patterns[p]->name.c_str());
#endif
						}
						else
						{
							// we are saving p<<CHILDPATBITS +pm->end because:
							// saving the index I (which was done before) would not work because the indexes change because
							//    of the implementation of multiple parse passes means insertion of patterns out of order.
							// saving p (the pattern #) because this must fit inside of a short (eventually) in the pema array and
							//    p and the pm->end won't necessarily fit - must make sure!
							// so this is the current compromise.
							// if this changes - must change also the treatment of elementMatchedSubIndex in push_back_unique function in pema!
							//                   getMaxDisplaySize and printSentence in word.cpp
#ifdef LOG_OLD_MATCH
							whatMatched.push_back(cMatchElement(I, sourcePosition, pm->len + sourcePosition, cPatternElementMatchArray::subIndex(p, pm->len),
								costs[form] + pm->getCost(), true, lastElement, elementPosition, form, pm->isNew()));
#else
							whatMatched.push_back(cMatchElement(I, sourcePosition, pm->len + sourcePosition, cPatternElementMatchArray::subIndex(p, pm->len),
								patternCosts[pattern] + pm->getCost(), true, lastElement, elementPosition, pattern, pm->isNew(source.pass)));
#endif
							msize++;
							oneMatch = true;
						}
					}
#ifdef LOG_PATTERN_MATCHING
					if (t.tracePatternMatching)
					{
						lpwstring s;
						lplog(u"%d:pattern %s:position %d:whatMatched #%d:End position %d, End element matched index %d subpattern %s matchString %sinserted:%s",
							sourcePosition, patternName.c_str(), elementPosition,
							whatMatched.size() - 1, pm->len, I, patterns[p]->name.c_str(), (!found) ? u"" : u"NOT ", matchesToString(whatMatched, whatMatched.size() - 1, s).c_str());
					}
#endif
				}
			}
#ifdef LOG_PATTERN_MATCHING
			if (t.tracePatternMatching && !oneMatch)
				lplog(u"%d:pattern %s:position %d:No match for pattern %s[%s]", sourcePosition, patternName.c_str(), elementPosition,
					patterns[p]->name.c_str(), patterns[p]->differentiator.c_str());
#endif
		}
	// if matched, immediately return true
	if (oneMatch)
		return 1;
	// if not matched, but match is optional (minimum==0) and word should not be ignored (or pattern is set to disable ignore forms), return true (go on to next element)
	// words to ignore can only be forms, not patterns
	// no inflected forms allowed
	// if [first non-optional element in pattern , and - removed] pattern has not matched any elements previously,
	//   reject, because the pattern will have a chance to match with the next position
	// (and we want to avoid double matching)
	if ((!im->word->second.isIgnore() || patterns[patternNum]->checkIgnorableForms))
	{
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
			lplog(u"%d:pattern %s:position %d:Not matched, but word %s is not set for ignore (%s), or pattern is set to check ignored forms (%s), and returning whether minimum==0 (%d).",
				sourcePosition, patternName.c_str(), elementPosition, im->word->first.c_str(), (im->word->second.isIgnore()) ? u"ignore" : u"not ignore", (patterns[patternNum]->checkIgnorableForms) ? u"check" : u"not check", minimum);
#endif
		return (minimum == 0) ? 2 : -3;
	}
	// if there is nothing to match against, or current source position is the matched end position, return false (fail match)
	if (!whatMatched.size() || sourcePosition == whatMatched[0].endPosition)
		return -4;
#ifdef LOG_PATTERN_MATCHING
	if (t.tracePatternMatching)
		lplog(u"%d:pattern %s:position %d:%s is ignored%s.",
			sourcePosition, patternName.c_str(), elementPosition, im->word->first.c_str(), (minimum == 0) ? u" optional pattern" : u"");
#endif
	//whatMatched[lastElement].sentencePosition++; // eliminated because multiple matches could use this and some might ignore and others not
	// see beginPosition in matchElement
	return matchOne(source, sourcePosition + 1, lastElement, whatMatched, t);
}

// this assumes that optional or multiple elements cannot have a next element that could match
// Try this pattern at sourcePosition.  Clears source.whatMatched, matches the
// first element, then matchRange for each subsequent element.  If fill is
// false (transform/QA patterns), only records variable->location maps and
// returns whether anything matched.  If fill is true, fillPattern is called
// for every surviving full-pattern chain; reduced costs are batched into
// source.reduceParents.  Returns true if at least one *new* PMA/PEMA entry
// was written (additionalMatch).
bool cPattern::matchPatternPosition(cSource& source, const unsigned int sourcePosition, bool fill, sTrace& t)
{
	LFS  // DLFS
		source.whatMatched.clear();
	source.whatMatched.reserve(10000);
	overMatchMemoryExceeded = false;
	vector <cPatternElement*>::iterator e = elements.begin(), eEnd = elements.end();
	if (!(*e)->matchFirst(source, sourcePosition, source.whatMatched, t)) return false;
	int begin = 0;
	for (e++; e != eEnd; e++)
	{
		int saveEnd = source.whatMatched.size();
		if (!(*e)->matchRange(source, begin, saveEnd, source.whatMatched, t))
		{
#ifdef LOG_PATTERN_MATCHING
			if (t.tracePatternMatching)
				::lplog(u"%d:pattern %s:position %d:Match failed", sourcePosition, name.c_str(), e - elements.begin());
#endif
			return false;
		}
		if (source.whatMatched.size() != saveEnd) begin = saveEnd;
	}
#ifdef LOG_PATTERN_MATCHING
	if (t.tracePatternMatching)
		::lplog(u"%d:pattern %s:Match succeeded", sourcePosition, name.c_str());
#endif
	if (!fill)
	{
		unsigned int whereMatch = sourcePosition;
		for (e = elements.begin(), eEnd = elements.end(); e != eEnd; e++)
		{
			if ((*e)->variable.length())
			{
				variableToLocationMap[(*e)->variable] = whereMatch;
				variableToLengthMap[(*e)->variable] = (*e)->endPosition - whereMatch;
				locationToVariableMap[whereMatch] = (*e)->variable;
#ifdef LOG_PATTERN_MATCHING
				if (t.tracePatternMatching)
					::lplog(LOG_WHERE, u"%d:matchPattern %d mapped variable %s=(begin=%d,end=%d)", sourcePosition, num, (*e)->variable.c_str(), whereMatch, (*e)->endPosition);
#endif
			}
			whereMatch = (*e)->endPosition;
		}
		return source.whatMatched.size() != 0;
	}
	vector <unsigned int> insertionPoints;
	vector <int> diffCosts;
	bool additionalMatch = false; // set if a match was found that was not found previously
	// now transfer the matches to the source
	for (unsigned int I = begin; I < source.whatMatched.size(); I++)
	{
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
			::lplog(u"%d:pattern %s:  ___whatMatched #%d___", sourcePosition, name.c_str(), I);
#endif
		unsigned int insertionPoint = -1;
		int reducedCost = MAX_SIGNED_SHORT;
		bool pushed = false;
		additionalMatch |= fillPattern(source, sourcePosition, source.whatMatched, I, insertionPoint, reducedCost, pushed, t);
		if (pushed && insertionPoint != source.m[sourcePosition].pma.count - 1)
		{
			for (unsigned int J = 0; J < insertionPoints.size(); J++)
				if (insertionPoints[J] >= insertionPoint)
					insertionPoints[J]++;
		}
		if (reducedCost != MAX_SIGNED_SHORT)
		{
			insertionPoints.push_back(insertionPoint);
			diffCosts.push_back(reducedCost);
		}
	}
	numMatches++;
	if (insertionPoints.size())
	{
#ifdef QLOGPATTERN
		if (pass > 0) lplog(u"Pattern %s lowered costs against sentence position %d", p->name.c_str(), pos);
#endif
		source.reduceParents(sourcePosition, insertionPoints, diffCosts);
	}
	return additionalMatch;
}

// Materialize one whatMatched chain (ending at elementMatched) into PMA/PEMA.
// Applies _ONLY_END_MATCH / _STRICT_NO_MIDDLE_MATCH / _QUESTION / _ONLY_BEGIN_MATCH
// cost adjustments, sums element costs, push_back_unique's the PMA slot, then
// walks the chain backwards writing PEMA elements and linking
// nextByPatternEnd / nextByPosition / nextByChildPatternEnd.  On pass>1 a
// chain with no isNew() child is discarded.  Returns true if a new PEMA
// element was created.  insertionPoint/reducedCost/pushed are PMA outcomes
// for reduceParents.
bool cPattern::fillPattern(cSource& source, int sourcePosition, vector <cMatchElement>& whatMatched, int elementMatched, unsigned int& insertionPoint, int& reducedCost, bool& pushed, sTrace& t)
{
	LFS // DLFS
		int endPosition = whatMatched[elementMatched].endPosition;
	if (source.lastSourcePositionSet < endPosition) source.lastSourcePositionSet = endPosition;
	if ((strictNoMiddleMatch || onlyEndMatch) && ignoreFlag && endPosition < (signed)source.m.size() && !lp_strchr(u"!?.", source.m[endPosition].word->first[0]))
		return false;
	if ((strictNoMiddleMatch || onlyEndMatch) && endPosition < (signed)source.m.size())
	{
		if (iswalpha(source.m[endPosition].word->first[0]))	return false;
		if (endPosition && (source.m[endPosition - 1].flags & cWordMatch::flagFirstLetterCapitalized))
		{
			bool endSentence = true;
			// also if next is a period, but before that is an honorific
			// As though that first scrutiny had been satisfactory , Mrs . Vandemeyer motioned to a chair . (CLOSING_S1 will match Mrs)
			const lpchar_t* abbreviationForms[] = { u"letter",u"abbreviation",u"measurement_abbreviation",u"street_address_abbreviation",u"business_abbreviation",
				u"time_abbreviation",u"date_abbreviation",u"honorific_abbreviation",u"trademark",u"pagenum",NULL };
			for (unsigned int af = 0; abbreviationForms[af] && endSentence; af++)
				if (source.m[endPosition - 1].queryForm(abbreviationForms[af]) >= 0)
					endSentence = false;
			if (endPosition && source.m[endPosition].word->first[0] == u'.' && !endSentence && endPosition + 1 < (signed)source.m.size() &&
				// prevent Mr... / Bellavue Dr. The 
				iswalpha(source.m[endPosition + 1].word->first[0]) && source.m[endPosition + 1].queryForm(PROPER_NOUN_FORM_NUM) >= 0)
				return false;
		}
	}
	bool additionalMatch = false, isNew = false;
	int elementCost = 0, numElementsMatched = 0;
	//#ifdef LOG_OLD_MATCH
	/*
	On second pass, if any subpattern matched is above the current pattern in number OR new for this pass, mark NEW
	if none of the elementsMatched are new, return false, without filling anything, or checking anything.
	this "new_for_pass" flag will be in PMA. (LOG_OLD_MATCH only)
	*/
	if (source.pass > 1)
	{
		for (int tmpElementMatched = elementMatched; tmpElementMatched >= 0; tmpElementMatched = whatMatched[tmpElementMatched].previousMatch)
			isNew |= whatMatched[tmpElementMatched].isNew(); // num LOG_OLD_MATCH
		if (!isNew)
		{
#ifdef LOG_PATTERN_MATCHING
			if (t.tracePatternMatching)
				::lplog(u"%d:pattern %s[%s](%d,%d) NOT NEW MATCH", sourcePosition, name.c_str(), differentiator.c_str(), sourcePosition, endPosition);
#endif
			pushed = false;
			return false;
		}
	}
	else
		isNew = true;
	//  #endif
	if ((strictNoMiddleMatch || onlyEndMatch) && endPosition < (signed)source.m.size() && iswalpha(source.m[endPosition].word->first[0]))
		elementCost += 100;
	if (questionFlag && endPosition <= (signed)source.m.size())
	{
		// question could be embedded in a coordinated phrase:
		// But if so, where was the girl, and what had she done with the papers?
		// search for a sentence ending:
		unsigned int ep = endPosition;
		for (; ep < source.m.size() && !source.isEOS(ep); ep++);
		// -2 is to compensate for verbs taking objects like: are you not?  
		// 'are' should take an object, which would give a -1 cost to a _COMMAND.  So to conteract this, -2 is given here.
		// Non-questions pay +2 so a _QUESTION pattern loses to a declarative
		// when the sentence does not actually end in '?'.
		if (ep < source.m.size())
			elementCost += (source.m[ep].word->first == u"?") ? -2 : 2;
	}
	int tmpCost;
	if (onlyBeginMatch && sourcePosition && (tmpCost = source.m[sourcePosition - 1].word->second.lowestSeparatorCost()) > 0)
		elementCost += tmpCost / 1000; // lowestSeparatorCost multiplies by 1000
	for (int tmpElementMatched = elementMatched; tmpElementMatched >= 0; tmpElementMatched = whatMatched[tmpElementMatched].previousMatch)
	{
		int subMatchBegin = whatMatched[tmpElementMatched].beginPosition;
		int subMatchEnd = whatMatched[tmpElementMatched].endPosition;
		if (subMatchBegin < subMatchEnd)
		{
			source.reverseMatchElements.assign(numElementsMatched++, tmpElementMatched);
			elementCost += whatMatched[tmpElementMatched].cost; // add up cost of each element in pattern, which is also all costs of subPatterns
		}
	}
	bool reduced;
	insertionPoint = source.m[sourcePosition].pma.push_back_unique(source.pass, elementCost, num, endPosition - sourcePosition, reduced, pushed);
	/*
	// items in PMA
	int pemaByPatternEnd; // first PEMA entry in this source position the PMA array belongs to that has the same pattern and end, begin=0
	int pemaByChildPatternEnd; // first PEMA entry in this source position that has a child pattern = pattern and childEnd = end
	// items in PEMA
	int nextByPosition; // next PEMA entry in the same position - allows routines to traverse downwards
	int nextByPatternEnd; // the next PEMA entry in the same position with the same pattern and end , ordered by begin
	int nextByChildPatternEnd; // the next PEMA entry that has the same childPattern and childEnd
	int nextPatternElement; // the next PEMA entry in the next position with the same pattern and end - allows routines to skip forward by pattern
	// items in m
	int beginPEMAPosition;
	int endPEMAPosition;
	unsigned int PEMACount;
	*/
	if (!isNew && (pushed || reduced))
	{
		::lplog(u"%d:pma position %d %s[%s](%d,%d) costing error - on non-new reduced cost from %d (original) to %d!", sourcePosition, insertionPoint, name.c_str(), differentiator.c_str(),
			sourcePosition, source.m[sourcePosition].pma[insertionPoint].len + sourcePosition, source.m[sourcePosition].pma[insertionPoint].cost, elementCost);
	}
	if (pushed)
	{
		source.m[sourcePosition].patterns.set(num);
		vector <cMatchElement>::iterator iMatch = whatMatched.begin(), iMatchEnd = whatMatched.end();
		for (; iMatch != iMatchEnd; iMatch++)
			if (iMatch->beginPosition == sourcePosition && iMatch->PMAIndex >= (int)insertionPoint)
				iMatch->PMAIndex++;
	}
	if (reduced)
		reducedCost = elementCost;
	else
		reducedCost = MAX_SIGNED_SHORT;
#if defined(LOG_PATTERN_MATCHING) || defined(LOG_PATTERN_COST_CHECK)
	if (t.tracePatternMatching)
	{
		lpwstring s;
		::lplog(u"%d:pattern %s[%s](%d,%d) total cost=%d at PMAOffset %d (pushed=%s,reduced=%s) for matches %s",
			sourcePosition, name.c_str(), differentiator.c_str(), sourcePosition, endPosition, cost,
			insertionPoint, (pushed) ? u"true" : u"false", (reduced) ? u"true" : u"false",
			matchesToString(whatMatched, elementMatched, s).c_str());
	}
#endif
#ifdef LOG_PATTERN_COST_CHECK
	if (cost)
		::lplog(u"%d:pma position %d %s[%s](%d,%d) set cost %d.", sourcePosition, insertionPoint, name.c_str(), differentiator.c_str(),
			sourcePosition, source.m[sourcePosition].pma[insertionPoint].len + sourcePosition, cost);
#endif
	int* formerPosition = &source.m[sourcePosition].pma[insertionPoint].pemaByPatternEnd, * firstPosition = formerPosition;
	// if maxMatch updated and the pattern is not fill (Tag("_FINAL")), then when winners are calculated,
	// if a pattern which is not tagged "_FINAL" has matched more elements than another which is tagged "_FINAL", then
	// every match will be removed and the words will end up being unmatched.
	if (isTopLevelMatch(source, sourcePosition, endPosition))
	{
		vector <cWordMatch>::iterator im = source.m.begin() + sourcePosition, imEnd = source.m.begin() + endPosition;
		for (; im != imEnd; im++) im->flags |= cWordMatch::flagTopLevelPattern;
		im = source.m.begin() + sourcePosition;
		int len = endPosition - sourcePosition, avgCost = elementCost * 1000 / len; // COSTCALC
		for (unsigned int relpos = 0; im != imEnd; im++, relpos++)
			if (im->updateMaxMatch(len, avgCost) && t.tracePatternElimination)
				::lplog(u"TOP %d:Pattern %s[%s](%d,%d) established a new maxLACMatch %d or lowest average cost %d=%d*1000/%d",
					sourcePosition + relpos, name.c_str(), differentiator.c_str(), sourcePosition, endPosition, len, avgCost, elementCost, len); // COSTCALC
	}
	bool POFlag = false;
	for (int I = numElementsMatched - 1; I >= 0; I--)
	{
		vector <cMatchElement>::iterator thisMatch = whatMatched.begin() + source.reverseMatchElements[I];
		int subMatchBegin = thisMatch->beginPosition;
		bool newElement;
		unsigned int PEMAOffset = source.pema.push_back_unique(formerPosition, subMatchBegin, elementCost, thisMatch->cost,
			num, sourcePosition - subMatchBegin, endPosition - subMatchBegin, thisMatch->elementMatchedIndex, thisMatch->cPatternElement,
			thisMatch->patternElementIndex, source.m.size() - sourcePosition, newElement, POFlag);
		if (newElement)
		{
			if (thisMatch->elementMatchedIndex & cMatchElement::patternFlag)
				elements[thisMatch->cPatternElement]->usagePatternEverMatched[thisMatch->patternElementIndex]++;
			else
				elements[thisMatch->cPatternElement]->usageFormEverMatched[thisMatch->patternElementIndex]++;
			source.pema[PEMAOffset].origin = *firstPosition;
			if (source.pema[*firstPosition].begin != 0)
				break;
			vector <cWordMatch>::iterator mb = source.m.begin() + subMatchBegin;
			if (mb->beginPEMAPosition < 0) mb->beginPEMAPosition = PEMAOffset;
			if (mb->endPEMAPosition >= 0)
				source.pema[mb->endPEMAPosition].nextByPosition = PEMAOffset;
			mb->endPEMAPosition = PEMAOffset;
			mb->PEMACount++;
			if (thisMatch->PMAIndex >= 0)
			{
				source.pema[PEMAOffset].nextByChildPatternEnd = mb->pma[thisMatch->PMAIndex].pemaByChildPatternEnd;
				mb->pma[thisMatch->PMAIndex].pemaByChildPatternEnd = PEMAOffset;
				// check inconsistent PMAOffsets
				/*
				cPatternMatchArray::tPatternMatch *pm=mb->pma.content;
				if (thisMatch->PMAIndex>=(int)mb->pma.count)
				::lplog(LOG_FATAL_ERROR,u"    %d:RP PMA pattern offset %d is >= %d - ILLEGAL",subMatchBegin,thisMatch->PMAIndex,mb->pma.count);
				unsigned int childPattern=pm[thisMatch->PMAIndex].getParentPattern();
				int childEnd=pm[thisMatch->PMAIndex].end;
				cPatternElementMatchArray::tPatternElementMatch *pem=source.pema.begin()+pm[thisMatch->PMAIndex].pemaByChildPatternEnd;
				if (patterns[childPattern]->name!=patterns[pem->getChildPattern()]->name ||
				childEnd!=pem->getChildLen())
				::lplog(u"INCONSISTENT PMAOffset %d pemaByChildPatternEnd=%d %s[%s](%d,%d) leads to %s[%s](%d,%d) CHILD %s[%s](%d,%d)!",
				thisMatch->PMAIndex,pm[thisMatch->PMAIndex].pemaByChildPatternEnd,
				patterns[childPattern]->name.c_str(),patterns[childPattern]->differentiator.c_str(),subMatchBegin,subMatchBegin+childEnd,
				patterns[pem->getParentPattern()]->name.c_str(),patterns[pem->getParentPattern()]->differentiator.c_str(),subMatchBegin,subMatchBegin+pem->end,
				patterns[pem->getChildPattern()]->name.c_str(),patterns[pem->getChildPattern()]->differentiator.c_str(),subMatchBegin,subMatchBegin+pem->getChildLen());
				*/
			}
#ifdef LOG_PATTERN_CHAINS
			source.logPatternChain(sourcePosition, insertionPoint, cPatternElementMatchArray::BY_PATTERN_END);
			source.logPatternChain(subMatchBegin, mb->beginPEMAPosition, cPatternElementMatchArray::BY_POSITION);
			source.logPatternChain(subMatchBegin, PEMAOffset, cPatternElementMatchArray::BY_CHILD_PATTERN_END);
#endif
			numPushes++;
			additionalMatch = true;
		}
#ifdef LOG_PATTERN_MATCHING
		if (t.tracePatternMatching)
		{
			if (thisMatch->elementMatchedIndex & (cMatchElement::patternFlag))
				::lplog(u"%d:pattern %s[%s](%d,%d) %s[%s](%d,%d) cost=%d%s",
					subMatchBegin, name.c_str(), differentiator.c_str(), sourcePosition, endPosition,
					patterns[thisMatch->getChildPattern()]->name.c_str(), patterns[thisMatch->getChildPattern()]->differentiator.c_str(),
					subMatchBegin, thisMatch->getChildLen() + subMatchBegin, thisMatch->cost, (newElement) ? u"" : u" -- DUPLICATE");
			else
				::lplog(u"%d:pattern %s[%s](%d,%d) %s cost=%d%s", subMatchBegin, name.c_str(), differentiator.c_str(), sourcePosition, endPosition,
					source.m[subMatchBegin].word->second.Form(thisMatch->elementMatchedIndex)->name.c_str(), thisMatch->cost, (newElement) ? u"" : u" -- DUPLICATE");
		}
#endif
		POFlag = true;
		formerPosition = &source.pema[PEMAOffset].nextPatternElement;
	}
	return additionalMatch;
}

// First pattern named `form` at or after startingPattern.  Advances
// startingPattern to the hit (or to patterns.size() on miss).  Never returns -1;
// patterns.size() is the sole not-found sentinel for findPattern (the
// unused lpwstring-only and (name,diff) overloads, which used a -1 convention
// and had zero callers anywhere in the tree, were removed).
unsigned int findPattern(lpwstring form, unsigned int& startingPattern)
{
	LFS
		for (; startingPattern < patterns.size() && (patterns[startingPattern]->name != form); startingPattern++);
	return startingPattern;
}

// _VERB|wrapping[*]*2{VERB:pM:V_OBJECT} is a verb with future reference and a cost of 2.
// Split a create() token into the bare form/pattern name plus optional
// |specificWord, [R] future-ref / recursive-match, *cost, and {TAG:TAG}.
// Mutates `form` in place (erases the suffix); specificWord is copied out via
// assign(sword, len) rather than by writing through form.c_str().
void cPattern::processForm(lpwstring& form, lpwstring& specificWord, int& cost, set <unsigned int>& tags, bool& explicitFutureReference, bool& blockDescendants, bool& allowRecursiveMatch)
{
	LFS
		const lpchar_t* ch;
	for (ch = form.c_str(); *ch && *ch != u'|' && *ch != u'[' && *ch != u'*' && *ch != u'{'; ch++);
	cost = 0;
	blockDescendants = allowRecursiveMatch = explicitFutureReference = false;
	unsigned int eraseFromThisPoint = (unsigned int)(ch - form.c_str());
	if (*ch == u'|')
	{
		const lpchar_t* sword = ch + 1;
		for (; *ch && *ch != u'[' && *ch != u'*' && *ch != u'{'; ch++);
		specificWord.assign(sword, ch - sword);
	}
	if (*ch == u'[')
	{
		explicitFutureReference = true;
		allowRecursiveMatch = (ch[1] == u'R');
		ch += 3;
	}
	if (*ch == u'*')
	{
		ch++;
		lpwstring keep;
		if (*ch == u'-')
		{
			keep += *ch;
			ch++;
		}
		while (iswdigit(*ch))
		{
			keep += *ch;
			ch++;
		}
		cost = lp_wtoi(keep.c_str());
	}
	if (*ch == '{')
	{
		while (*ch == u'{' || *ch == u':')
		{
			ch++;
			lpwstring tag;
			while (*ch != u':' && *ch != u'}')
			{
				tag += *ch;
				ch++;
			}
			if (!tag.length()) continue;
			unsigned int I;
			for (I = 0; I < patternTagStrings.size() && tag != patternTagStrings[I]; I++);
			if (I == patternTagStrings.size()) patternTagStrings.push_back(tag);
			if (tag == u"_BLOCK")
				blockDescendants = true;
			else
				tags.insert(I);
		}
		ch++;
	}
	// "_REL1{_BLOCK}*1"
	if (*ch)
		::lplog(LOG_FATAL_ERROR, u"Incorrect form syntax in form %s.", form.c_str());
	form.erase(eraseFromThisPoint, form.length());
}

// If `tag` is in this pattern's own tags set, erase it and return true.
// Used at create() time to turn {_FINAL}, {_QUESTION}, ... into bool flags
// so they do not also live as ordinary collectable tags.
bool cPattern::eliminateTag(lpwstring tag)
{
	LFS
		for (set <unsigned int>::iterator t = tags.begin(), tEnd = tags.end(); t != tEnd; t++)
			if (tag == patternTagStrings[*t])
			{
				tags.erase(t);
				return true;
			}
	return false;
}

#ifdef ABNF
// ABNF dump of one form/pattern alternative: name[#diff][$cost][TAG:TAG]/.
int cPatternElement::writeABNFElementTag(lpchar_t* buf, lpwstring sForm, int num, int cost, vector <unsigned int>& tags, int maxBuf, bool printNum)
{
	LFS
		int len = 0;
	if (printNum)
		len += lp_snprintf(buf, maxBuf, u"%s#%s", sForm.c_str(), patterns[num]->differentiator.c_str());
	else
		len += lp_snprintf(buf, maxBuf, u"%s", sForm.c_str());
	if (cost) len += lp_snprintf(buf + len, maxBuf - len, u"$%d", cost);
	if (tags.size()) buf[len++] = u'[';
	for (unsigned int I = 0; I < tags.size(); I++)
		len += lp_snprintf(buf + len, maxBuf - len, u"%s:", patternTagStrings[tags[I]].c_str());
	if (buf[len - 1] == u':') buf[len - 1] = u']';
	buf[len++] = u'/';
	buf[len] = 0;
	return len;
}

// does not feed into patternReferences!
// ABNF patterns include all pattern #s (NOUN#2, NOUN#3), whereas the hardcoded patterns include only the main form (NOUN)
// This is because ABNF patterns include costing data for each NOUN# individually (original purpose)
//
// Parse one ABNF alternative out of buf (mutates buf with NULs).  Pattern
// names are queued as cPatternReference; forms must already exist.
void cPatternElement::readABNFElementTag(lpwstring patternName, lpwstring differentiator, int elementNum, set <unsigned int>& descendantTags, lpchar_t* buf)
{
	LFS
		lpchar_t* c_diff = lp_strchr(buf, u'#'), * c_cost = lp_strchr(buf, u'$'), * c_tags = lp_strchr(buf, u'[');
	if (c_diff) *c_diff = 0;
	if (c_cost) *c_cost = 0;
	if (c_tags) *c_tags = 0;
	lpwstring form = buf;
	int diffNum = (c_diff) ? wtoi(c_diff + 1) : -1;
	int cost = (c_cost) ? wtoi(c_cost + 1) : 0, f = -1;
	bool blockDescendants = false;
	vector <unsigned int> elementTags;
	if (c_tags)
	{
		lpchar_t* tag = c_tags + 1;
		while (tag)
		{
			lpchar_t* next_tag = lp_strchr(tag, u':');
			if (!next_tag) next_tag = lp_strchr(tag, u']');
			if (!next_tag) break;
			*next_tag = 0;
			if (!lp_strcmp(tag, u"_BLOCK"))
				blockDescendants = true;
			else
				elementTags.push_back(findTag(tag));
		}
	}
	if (form[0] == u'_')
		patternReferences.push_back(new cPatternReference(form, patterns.size(), diffNum, elementNum, cost, elementTags, blockDescendants, false));
	else
	{
		if ((f = cForms::findForm(form)) < 0)
			::lplog(LOG_FATAL_ERROR | LOG_ERROR, "FATAL_ERROR:Pattern %s[%s] uses an undefined form %s.", patternName.c_str(), differentiator.c_str(), form.c_str());
		formStr.push_back(form);
		costs.push_back(cost);
		indexes.push_back(f);
		tags.push_back(elementTags);
		stopDescendingSearch.push_back(blockDescendants);
		isPattern.push_back(false);
	}
	for (unsigned int I = 0; I < elementTags.size(); I++)
		descendantTags.insert(elementTags[I]);
}

// ABNF dump of this whole element: [min*max]{INFLECTIONS}(alt/alt/...).
void cPatternElement::writeABNF(lpchar_t* buf, int& len, int maxBuf)
{
	LFS
		if (minimum != 1 || maximum != 1) len += lp_snprintf(buf + len, maxBuf - len, u"%d*%d", minimum, maximum);
	lpwstring sFlags;
	inflectionFlagsToStr(sFlags);
	if (sFlags.length()) len += lp_snprintf(buf + len, maxBuf - len, u"{%s}", sFlags.c_str());
	buf[len++] = u'(';
	for (unsigned int I = 0; I < formStr.size(); I++)
	{
		bool printNum = (formStr.size() > I + 1 && formStr[I] == formStr[I + 1]) || (I != 0 && formStr[I] == formStr[I - 1]);
		len += writeABNFElementTag(buf + len, formStr[I], indexes[I], costs[I], tags[I], printNum);
	}
	buf[len - 1] = u')';
}

// readABNF
// Construct one element by parsing an ABNF "(alt/alt)" clause out of buf.
cPatternElement::cPatternElement(lpwstring patternName, lpwstring differentiator, int elementNum, set <unsigned int>& descendantTags, lpchar_t*& buf)
{
	LFS
		lpchar_t* c_tags = lp_strchr(buf, u'(');
	if (!c_tags) return;
	*c_tags = 0;
	lpchar_t* c_flags = lp_strchr(buf, u'{');
	if (c_flags) *c_flags = 0;
	lpchar_t* ch = lp_strchr(buf, u'*');
	elementPosition = elementNum;
	if (ch)
	{
		*ch = 0;
		minimum = wtoi(buf);
		maximum = wtoi(ch + 1);
	}
	else
		minimum = maximum = 1;
	inflectionFlags = 0;
	if (c_flags)
		for (unsigned int I = 0; inflectionFlagList[I].sFlag; I++)
			if (strstr(c_flags + 1, inflectionFlagList[I].sFlag))
				inflectionFlags |= inflectionFlagList[I].flag;
	lpchar_t* tag = c_tags + 1;
	while (true)
	{
		lpchar_t* next_tag;
		if (!(next_tag = lp_strchr(tag, u'/')) && !(next_tag = lp_strchr(tag, u')'))) break;
		*next_tag = 0;
		readABNFElementTag(patternName, differentiator, elementNum, descendantTags, tag);
		tag = next_tag + 1;
	}
}

/*
rulename(# number) ($ number)  = (number) (*) (number) name ($ number) ([ TAG:TAG ... ]) (/) ...
__NAME#3  = 0*1(honorific[SINGULAR]/__NAMEINTRO)
letter{FIRST}
*"."
*/
// Write this pattern as one ABNF rule.  This path does not compile as-is
// (wtoi / narrow fgets elsewhere in this #ifdef ABNF block); the afterQuote
// reference below has been kept in sync with the live member name so it does
// not add a further mismatch if the block is ever revived.
void cPattern::writeABNF(FILE* fh, unsigned int lastTag)
{
	LFS
		lpchar_t buf[1024];
	int len;
	len = lp_snprintf(buf, 1024, u"%s", name.c_str());
	if (differentiator.length()) len += lp_snprintf(buf + len, 1024 - len, u"#%s", differentiator.c_str());
	if (cost) len += lp_snprintf(buf + len, 1024 - len, u"$%d", cost);
	buf[len++] = u'[';
	for (unsigned int I = 0; I < tags.size(); I++)
		if (tags[I] < lastTag)
			len += lp_snprintf(buf + len, 1024 - len, u"%s:", patternTagStrings[tags[I]].c_str());
	if (blockDescendants) len += lp_snprintf(buf + len, 1024 - len, u"_BLOCK:");
	if (fillIfAloneFlag) len += lp_snprintf(buf + len, 1024 - len, u"_FINAL_IF_ALONE:");
	if (fillFlag) len += lp_snprintf(buf + len, 1024 - len, u"_FINAL:");
	if (onlyAloneExceptInSubPatternsFlag) len += lp_snprintf(buf + len, 1024 - len, u"_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:");
	if (onlyBeginMatch) len += lp_snprintf(buf + len, 1024 - len, u"_ONLY_BEGIN_MATCH:");
	if (afterQuote) len += lp_snprintf(buf + len, 1024 - len, u"_AFTER_QUOTE:");
	if (strictNoMiddleMatch) len += lp_snprintf(buf + len, 1024 - len, u"_STRICT_NO_MIDDLE_MATCH:");
	if (onlyEndMatch) len += lp_snprintf(buf + len, 1024 - len, u"_ONLY_END_MATCH:");
	if (noRepeat) len += lp_snprintf(buf + len, 1024 - len, u"_NO_REPEAT:");
	if (ignoreFlag) len += lp_snprintf(buf + len, 1024 - len, u"_IGNORE:");
	if (questionFlag) len += lp_snprintf(buf + len, 1024 - len, u"_QUESTION:");
	if (notAfterPronoun) len += lp_snprintf(buf + len, 1024 - len, u"_NOT_AFTER_PRONOUN:");
	if (explicitSubjectVerbAgreement) len += lp_snprintf(buf + len, 1024 - len, u"_EXPLICIT_SUBJECT_VERB_AGREEMENT:");
	if (explicitNounDeterminerAgreement) len += lp_snprintf(buf + len, 1024 - len, u"_EXPLICIT_NOUN_DETERMINER_AGREEMENT:");
	if (buf[len - 1] == u':') buf[len - 1] = u']';
	if (buf[len - 1] == u'[') len--;
	buf[len++] = u'=';
	buf[len] = 0;
	lp_fwprintf(fh, u"%s\n", buf);
	len = 0;
	for (unsigned int I = 0; I < elements.size(); I++)
	{
		elements[I]->writeABNF(buf, len);
		if (len > SCREEN_WIDTH)
		{
			lp_fwprintf(fh, u"%s\n", buf);
			len = 0;
		}
	}
	if (len) lp_fwprintf(fh, u"%s\n", buf);
	lp_fwprintf(fh, u"\n");
}

// Dump every pattern to filePath.  fwopen / writeABNF; silent no-op if open fails.
void writePatternsInABNF(lpwstring filePath, unsigned int lastTag)
{
	LFS
		FILE* fp = fwopen(filePath.c_str(), u"w+");
	if (fp == NULL)
		return;
	for (unsigned int I = 0; I < patterns.size(); I++)
		patterns[I]->writeABNF(fp, lastTag);
	fclose(fp);
}

// Read an ABNF file into `patterns`.  Not on the live initializePatterns path.
void readPatternsInABNF(lpwstring filePath)
{
	LFS
		FILE* fp = fwopen(filePath.c_str(), u"r+");
	if (fp == NULL)
		return;
	bool valid = true;
	while (valid)
	{
		cPattern* pattern = new cPattern(fp, valid);
		if (valid) patterns.push_back(pattern);
	}
	fclose(fp);
}

// Parse one ABNF rule from fh.  valid is set false on EOF / no '='.
cPattern::cPattern(FILE* fh, bool& valid)
{
	LFS
		cPattern();
	lpchar_t buf[1024], * ch;
	valid = false;
	while (true)
	{
		if (fgets(buf, 1024, fh) == NULL) return;
		if (ch = lp_strchr(buf, u'=')) break;
	}
	valid = true;
	*ch = 0;
	lpchar_t* c_differentiator = lp_strchr(buf, u'#');
	lpchar_t* c_cost = lp_strchr(buf, u'$');
	lpchar_t* c_tags = lp_strchr(buf, u'[');
	if (c_differentiator) *c_differentiator = 0;
	if (c_cost) *c_cost = 0;
	if (c_tags) *c_tags = 0;
	name = buf;
	if (c_differentiator) differentiator = c_differentiator + 1;
	if (c_cost) cost = wtoi(c_cost + 1);
	if (c_tags)
	{
		lpchar_t* tag = c_tags + 1;
		while (tag)
		{
			lpchar_t* next_tag = lp_strchr(tag, u':');
			if (!next_tag) next_tag = lp_strchr(tag, u']');
			if (!next_tag) break;
			*next_tag = 0;
			if (!lp_strcmp(tag, u"_BLOCK")) blockDescendants = true;
			else if (!lp_strcmp(tag, u"_FINAL_IF_ALONE")) fillIfAloneFlag = true;
			else if (!lp_strcmp(tag, u"_FINAL")) fillFlag = true;
			else if (!lp_strcmp(tag, u"_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN")) onlyAloneExceptInSubPatternsFlag = true;
			else if (!lp_strcmp(tag, u"_ONLY_BEGIN_MATCH")) onlyBeginMatch = true;
			else if (!lp_strcmp(tag, u"_AFTER_QUOTE")) afterQuote = true;
			else if (!lp_strcmp(tag, u"_STRICT_NO_MIDDLE_MATCH")) strictNoMiddleMatch = true;
			else if (!lp_strcmp(tag, u"_ONLY_END_MATCH")) onlyEndMatch = true;
			else if (!lp_strcmp(tag, u"_NO_REPEAT")) noRepeat = true;
			else if (!lp_strcmp(tag, u"_IGNORE")) ignoreFlag = true;
			else if (!lp_strcmp(tag, u"_QUESTION")) questionFlag = true;
			else if (!lp_strcmp(tag, u"_NOT_AFTER_PRONOUN")) notAfterPronoun = true;
			else if (!lp_strcmp(tag, u"_EXPLICIT_SUBJECT_VERB_AGREEMENT")) explicitSubjectVerbAgreement = true;
			else if (!lp_strcmp(tag, u"_EXPLICIT_NOUN_DETERMINER_AGREEMENT")) explicitNounDeterminerAgreement = true;
			else
				tags.push_back(findTag(tag));
		}
	}
	int elementNum = 0;
	while (true)
	{
		fgets(buf, 1024, fh);
		if (lp_strlen(buf) < 2) break;
		lpchar_t* c_element = buf;
		while (true)
		{
			if (lp_strchr(c_element, u')'))
				elements.push_back(new cPatternElement(name, differentiator, elementNum++, descendantTags, c_element));
			else
				break;
			while (iswspace(*c_element)) c_element++;
		}
	}
}
#endif


// no pattern may limit references to only some instances of patterns
// that is, a pattern may not refer to a only _NOUN[1] and _NOUN[2], and not to _NOUN[3] which is after it.
// patternName,each element:(numForms,(form,...),flags,minimum,maximum)  - until numForms=0
// Build one pattern from the va_list DSL used by definePatterns.cpp.  Each
// step is: numForms, numForms form-strings, inflectionFlags, min, max; a
// following 0 ends the pattern.  Form names starting with '_' become
// cPatternReference entries (resolved after all creates).  {_FINAL} etc. are
// stripped into bool flags via eliminateTag.  Always returns true (`OK`);
// fatal-logs on undefined forms (unless {_FREE_FORM}), >255 elements, or
// >65535 patterns.  Chains nextRoot for later same-name variants.
bool cPattern::create(lpwstring patternName, lpwstring differentiator, int numForms, ...)
{
	LFS
		bool OK = true, explicitFutureReference; // nonOptionalElementFound=false,
	cPattern* p = new cPattern();
	va_list patternMarker;
	lpwstring specificPatternWord; //specificPatternWord and deleteSelectedMatchingPatterns are not valid for pattern names
	processForm(patternName, specificPatternWord, p->cost, p->tags, explicitFutureReference, p->blockDescendants, p->allowRecursiveMatch);
	p->fillIfAloneFlag = p->eliminateTag(u"_FINAL_IF_ALONE");
	p->fillFlag = p->eliminateTag(u"_FINAL");
	p->onlyAloneExceptInSubPatternsFlag = p->eliminateTag(u"_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN");
	p->onlyBeginMatch = p->eliminateTag(u"_ONLY_BEGIN_MATCH");
	p->afterQuote = p->eliminateTag(u"_AFTER_QUOTE");
	p->strictNoMiddleMatch = p->eliminateTag(u"_STRICT_NO_MIDDLE_MATCH");
	p->onlyEndMatch = p->eliminateTag(u"_ONLY_END_MATCH");
	p->noRepeat = p->eliminateTag(u"_NO_REPEAT");
	p->ignoreFlag = p->eliminateTag(u"_IGNORE");
	p->questionFlag = p->eliminateTag(u"_QUESTION");
	p->notAfterPronoun = p->eliminateTag(u"_NOT_AFTER_PRONOUN");
	p->explicitSubjectVerbAgreement = p->eliminateTag(u"_EXPLICIT_SUBJECT_VERB_AGREEMENT");
	p->explicitNounDeterminerAgreement = p->eliminateTag(u"_EXPLICIT_NOUN_DETERMINER_AGREEMENT");
	p->checkIgnorableForms = p->eliminateTag(u"_CHECK_IGNORABLE_FORMS");
	// free form is dangerous to use.  New forms are added to the word only if
	// the form has never existed.  If somehow a word is part of a pattern which
	// is meant to be a form (the form = the word), but the form already exists,
	// the form is not added to the word (there is no clue that this should be done,
	// and it should not be done in most cases, only in a case where the word==form name).
	// If the form is not added to the word and the form was never added to the word,
	// (or it was but then was never updated to the database for some reason), 
	// then this pattern will never be used because there are no words associated
	// with the form.
	// Moreover, this feature, though convenient, leads to a proliferation of forms.
	bool freeForm = p->eliminateTag(u"_FREE_FORM");
	p->name = patternName;
	p->differentiator = differentiator;
	p->num = patterns.size();
	p->rootPattern = 0;
	//size_t pr=patternReferences.size();
	va_start(patternMarker, numForms);     /* Initialize variable arguments. */
	lpwstring form;
	int elementPosition = 0;
	while (numForms)
	{
		cPatternElement* element = new cPatternElement;
		element->patternName = patternName;
		vector <int> predefinedForms;
		while (numForms > 0)
		{
			form = va_arg(patternMarker, lpchar_t*);
			set <unsigned int> elementTags;
			bool blockDescendants, allowRecursiveMatch;
			int elementCost, f = -1;
			lpwstring specificWord;
			processForm(form, specificWord, elementCost, elementTags, explicitFutureReference, blockDescendants, allowRecursiveMatch);
			for (set <unsigned int>::iterator et = elementTags.begin(), etEnd = elementTags.end(); et != etEnd; et++)
				p->allElementTags.set(*et);
			bool isPattern;
			if (isPattern = (form[0] == u'_')) //specificWord is not valid for _PATTERNS
				patternReferences.push_back(new cPatternReference(form, p->num, -1, p->elements.size(), elementCost, elementTags, blockDescendants, allowRecursiveMatch, !explicitFutureReference));
			else
			{
				if ((f = cForms::findForm(form)) < 0)
				{
					if (!freeForm)
						::lplog(LOG_FATAL_ERROR | LOG_ERROR, u"FATAL_ERROR:Pattern %s[%s] uses an undefined form %s (1).", p->name.c_str(), p->differentiator.c_str(), form.c_str());
					Words.predefineWord(form.c_str(), cSourceWordInfo::queryOnAnyAppearance);
					f = cForms::findForm(form);
					predefinedForms.push_back(f);
				}
				element->formStr.push_back(form);
				element->specificWords.push_back(specificWord);
				element->formIndexes.push_back(f);
				element->formCosts.push_back(elementCost);
				element->formTags.push_back(elementTags);
				element->formStopDescendingSearch.push_back(blockDescendants);
			}
			p->descendantTags.insert(elementTags.begin(), elementTags.end());
			numForms--;
		}
		if (element->patternIndexes.size() > 255)
			::lplog(LOG_FATAL_ERROR, u"Pattern %s[%s] has >255 indexes (%d) - see patternElementNum member of cPatternElementMatchArray class", p->name.c_str(), p->differentiator.c_str(), element->patternIndexes.size());
		element->inflectionFlags = va_arg(patternMarker, int);
		if (predefinedForms.size())
		{
			for (unsigned int pdf = 0; pdf < predefinedForms.size(); pdf++)
			{
				if (element->inflectionFlags & VERB_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass = u"verb";
				if (element->inflectionFlags & NOUN_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass = u"noun";
				if (element->inflectionFlags & ADJECTIVE_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass = u"adjective";
				if (element->inflectionFlags & ADVERB_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass = u"adverb";
			}
		}
		element->minimum = va_arg(patternMarker, int);
		element->maximum = va_arg(patternMarker, int);
		element->elementPosition = p->elements.size();
		p->elements.push_back(element);
		elementPosition++;
		numForms = va_arg(patternMarker, int);
	}
	if (p->elements.size() > 255)
		::lplog(LOG_FATAL_ERROR, u"Pattern %s[%s] has too many elements >255 (%d) - see patternElementNum member of cPatternElementMatchArray class", p->name.c_str(), p->differentiator.c_str(), p->elements.size());
	// findPattern leaves p->rootPattern at the first existing same-named pattern,
	// or at patterns.size() (== p->num, since p is not pushed onto `patterns`
	// until below) if none exists yet - so p->rootPattern is already correct
	// either way once this call returns.
	findPattern(patternName, p->rootPattern);
	if (p->rootPattern != p->num)
	{
		p->nextRoot = p->rootPattern;
		int tmpNext;
		while ((tmpNext = patterns[p->nextRoot]->nextRoot) >= 0)
			p->nextRoot = tmpNext;
		patterns[p->nextRoot]->nextRoot = p->num;
		p->nextRoot = -1;
	}
	for (unsigned int e = 0; e < p->elements.size(); e++)
		p->elements[e]->patternNum = p->num;
	if (cPatternReference::firstPatternReference == -1)
		cPatternReference::firstPatternReference = p->num;
	va_end(patternMarker);              /* Reset variable arguments.      */
	if (patterns.size() > 65535)
		::lplog(LOG_FATAL_ERROR | LOG_ERROR, u"FATAL ERROR:# Patterns exceeds allowable 65536 patterns storable in patternElement array on pattern %s[%s].",
			patternName.c_str(), differentiator.c_str());
	patterns.push_back(p);
	return OK;
}

// Build a one-off pattern from source.m[whereBegin, whereEnd) for QA
// transform matching.  Positions listed in
// positionToTransformationPatternVariableMap become named variables
// (parseVariables in/out); other positions take the winner form string.
// Not pushed onto the global `patterns` vector — the caller owns the pointer.
cPattern* cPattern::create(cSource* source, lpwstring patternName, int num, int whereBegin, int whereEnd, unordered_map <lpwstring, lpwstring>& parseVariables)
{
	LFS
		bool explicitFutureReference; // nonOptionalElementFound=false,OK=true,
	cPattern* p = new cPattern();
	p->cost = 0;
	p->questionFlag = true;
	p->name = patternName;
	p->num = num;
	p->rootPattern = 0;
	int elementPosition = 0;
	for (int I = whereBegin; I < whereEnd; I++)
	{
		cPatternElement* element = new cPatternElement;
		element->patternName = patternName;
		unordered_map < int, lpwstring>::iterator substitutePattern = source->positionToTransformationPatternVariableMap.find(I);
		lpwstring forms, tmpstr;
		if (substitutePattern != source->positionToTransformationPatternVariableMap.end())
		{
			// [variable name]=[substitute word for parsing]:[pattern list] || [variable name]
			forms = substitutePattern->second;
			size_t equalsPos = forms.find(u'='), colonPos = forms.find(u':');
			if (equalsPos != lpwstring::npos && colonPos != lpwstring::npos)
			{
				element->variable = forms.substr(0, equalsPos);
				parseVariables[element->variable] = forms = forms.substr(colonPos + 1);
#ifdef LOG_PATTERN_MAPPING
				::lplog(LOG_WHERE, u"%d:pattern create mapped variable %s=(%s)", whereBegin, element->variable.c_str(), parseVariables[element->variable].c_str());
#endif
			}
			else
			{
				forms = parseVariables[element->variable = forms];
#ifdef LOG_PATTERN_MAPPING
				::lplog(LOG_WHERE, u"%d:pattern used mapped variable %s=(%s)", whereBegin, element->variable.c_str(), parseVariables[element->variable].c_str());
#endif
			}
		}
		else
			forms = source->m[I].patternWinnerFormString(tmpstr); // verb|called
		for (int pos = -1, nextpos = 0; pos < (signed)forms.size() && nextpos >= 0; pos = nextpos)
		{
			nextpos = forms.find(u',', pos + 1);
			lpwstring form;
			if (nextpos != lpwstring::npos)
				form = forms.substr(pos + 1, nextpos - pos - 1);
			else
				form = forms.substr(pos + 1);
			set <unsigned int> elementTags;
			bool blockDescendants, allowRecursiveMatch;
			int elementCost, f = -1;
			lpwstring specificWord;
			processForm(form, specificWord, elementCost, elementTags, explicitFutureReference, blockDescendants, allowRecursiveMatch);
			for (set <unsigned int>::iterator et = elementTags.begin(), etEnd = elementTags.end(); et != etEnd; et++)
				p->allElementTags.set(*et);
			bool isPattern;
			if (isPattern = form[0] == u'_')
				//specificWord is not valid for _PATTERNS 
				patternReferences.push_back(new cPatternReference(form, p->num, -1, p->elements.size(), elementCost, elementTags, blockDescendants, allowRecursiveMatch, !explicitFutureReference));
			else
			{
				if ((f = cForms::findForm(form)) < 0)
					::lplog(LOG_FATAL_ERROR | LOG_ERROR, u"FATAL_ERROR:Pattern %s[%s] uses an undefined form %s (2).", p->name.c_str(), p->differentiator.c_str(), form.c_str());
				element->formStr.push_back(form);
				element->specificWords.push_back(specificWord);
				element->formIndexes.push_back(f);
				element->formCosts.push_back(elementCost);
				element->formTags.push_back(elementTags);
				element->formStopDescendingSearch.push_back(blockDescendants);
			}
			p->descendantTags.insert(elementTags.begin(), elementTags.end());
		}
		if (element->patternIndexes.size() > 255)
			::lplog(LOG_FATAL_ERROR, u"Pattern %s[%s] has >255 indexes (%d) - see patternElementNum member of cPatternElementMatchArray class", p->name.c_str(), p->differentiator.c_str(), element->patternIndexes.size());
		element->inflectionFlags = 0;
		element->minimum = 1;
		element->maximum = 1;
		element->elementPosition = p->elements.size();
		p->elements.push_back(element);
		elementPosition++;
	}
	for (unsigned int e = 0; e < p->elements.size(); e++)
		p->elements[e]->patternNum = p->num;
	return p;
}

// Render this element's inflectionFlags into sFlags (space-separated names).
const lpchar_t* cPatternElement::inflectionFlagsToStr(lpwstring& sFlags)
{
	LFS
		sFlags.clear();
	for (int I = 0; inflectionFlagList[I].sFlag; I++)
		if (inflectionFlagList[I].flag & inflectionFlags)
			sFlags += inflectionFlagList[I].sFlag + lpwstring(u" ");
	return sFlags.c_str();
}

// Render an inflection bitfield into sFlags.  Shared with word.cpp callers.
const lpchar_t* inflectionFlagsToStr(int inflectionFlags, lpwstring& sFlags)
{
	LFS
		sFlags.clear();
	for (int I = 0; inflectionFlagList[I].sFlag; I++)
		if (inflectionFlagList[I].flag & inflectionFlags)
			sFlags += inflectionFlagList[I].sFlag + lpwstring(u" ");
	return sFlags.c_str();
}

struct {
	int flag;
	const lpchar_t* sFlag;
} wordFlagList[] =
{
	{cSourceWordInfo::topLevelSeparator,u"topLevelSeparator"},
	{cSourceWordInfo::ignoreFlag,u"ignoreFlag"},
	{cSourceWordInfo::queryOnLowerCase,u"queryOnLowerCase"},
	{cSourceWordInfo::queryOnAnyAppearance,u"queryOnAnyAppearance"},
	{cSourceWordInfo::updateMainInfo,u"updateMainInfo"},
	{cSourceWordInfo::updateMainEntry,u"updateMainEntry"},
	{cSourceWordInfo::insertNewForms,u"insertNewForms"},
	{cSourceWordInfo::isMainEntry,u"isMainEntry"},
	{cSourceWordInfo::intersectionGroup,u"intersectionGroup"},
	{cSourceWordInfo::newWordFlag,u"newWordFlag"},
	{cSourceWordInfo::inSourceFlag,u"inSourceFlag"},
	{cSourceWordInfo::alreadyTaken,u"alreadyTaken"},
	{cSourceWordInfo::physicalObjectByWN,u"physicalObjectByWN"},
	{cSourceWordInfo::notPhysicalObjectByWN,u"notPhysicalObjectByWN"},
	{cSourceWordInfo::uncertainPhysicalObjectByWN,u"uncertainPhysicalObjectByWN"},
	{cSourceWordInfo::genericGenderIgnoreMatch,u"genericGenderIgnoreMatch"},
	{cSourceWordInfo::prepMoveType,u"prepMoveType"},
	{cSourceWordInfo::genericAgeGender,u"genericAgeGender"},
	{cSourceWordInfo::stateVerb,u"stateVerb"},
	{cSourceWordInfo::possibleStateVerb,u"possibleStateVerb"},
	{cSourceWordInfo::mainEntryErrorNoted,u"mainEntryErrorNoted"},
	{-1,NULL}
};


// Dump this pattern's flags, ancestor names, descendant tags, tag-set
// membership and every element's alternatives to the log.  Uses a 1024-wchar
// stack buffer with wcscat (no remaining-space check on the flag suffixes).
void cPattern::lplog(int logTypes)
{
	LFS
		lpchar_t temp[1024];
	lp_wsprintf(temp, u"%u:%s[%s] root=%u", num, name.c_str(), differentiator.c_str(), rootPattern);
	if (isFutureReference) lp_strcpy((temp) + lp_strlen(temp), u" FUTURE_REFERENCE");
	if (containsFutureReference) lp_strcpy((temp) + lp_strlen(temp), u" CONTAINS_FUTURE_REFERENCE");
	if (indirectFutureReference) lp_strcpy((temp) + lp_strlen(temp), u" INDIRECT_FUTURE_REFERENCE");

	if (fillFlag) lp_strcpy((temp) + lp_strlen(temp), u" {_FINAL}");
	if (fillIfAloneFlag) lp_strcpy((temp) + lp_strlen(temp), u" {_FINAL_IF_ALONE}");
	if (onlyBeginMatch) lp_strcpy((temp) + lp_strlen(temp), u" {_ONLY_BEGIN_MATCH}");
	if (afterQuote) lp_strcpy((temp) + lp_strlen(temp), u" {_AFTER_QUOTE}");
	if (strictNoMiddleMatch) lp_strcpy((temp) + lp_strlen(temp), u" {_STRICT_NO_MIDDLE_MATCH}");
	if (onlyEndMatch) lp_strcpy((temp) + lp_strlen(temp), u" {_ONLY_END_MATCH}");
	if (onlyAloneExceptInSubPatternsFlag) lp_strcpy((temp) + lp_strlen(temp), u" {_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}");
	if (blockDescendants) lp_strcpy((temp) + lp_strlen(temp), u" {_BLOCK}");
	if (noRepeat) lp_strcpy((temp) + lp_strlen(temp), u" {_NO_REPEAT}");
	if (ignoreFlag) lp_strcpy((temp) + lp_strlen(temp), u" {_IGNORE}");
	if (questionFlag) lp_strcpy((temp) + lp_strlen(temp), u" {_QUESTION}");
	if (notAfterPronoun) lp_strcpy((temp) + lp_strlen(temp), u" {_NOT_AFTER_PRONOUN}");
	if (explicitSubjectVerbAgreement) lp_strcpy((temp) + lp_strlen(temp), u" {_EXPLICIT_SUBJECT_VERB_AGREEMENT}");
	if (explicitNounDeterminerAgreement) lp_strcpy((temp) + lp_strlen(temp), u" {_EXPLICIT_NOUN_DETERMINER_AGREEMENT}");
	for (set<unsigned int>::iterator dt = tags.begin(), dtEnd = tags.end(); dt != dtEnd; dt++)
		lp_snprintf(temp + lp_strlen(temp), 1024 - lp_strlen(temp), u"{%s}", patternTagStrings[*dt].c_str());
	::lplog(logTypes, u"%s", temp);
	lpwstring temp2;
	for (unsigned int p = 0; p < patterns.size(); p++)
		if (ancestorPatterns.isSet(p))
			temp2 += lpwstring(u" ") + patterns[p]->name + lpwstring(u"[") + patterns[p]->differentiator + lpwstring(u"]");
	if (temp2.size() > 0) ::lplog(logTypes, u"ancestors:%s", temp2.c_str());
	temp[0] = 0;
	for (set<unsigned int>::iterator dt = descendantTags.begin(), dtEnd = descendantTags.end(); dt != dtEnd; dt++)
		lp_snprintf(temp + lp_strlen(temp), 1024 - lp_strlen(temp), u"%s ", patternTagStrings[*dt].c_str());
	if (temp[0]) ::lplog(logTypes, u"DESCENDANT TAGS:%s", temp);
	temp[0] = 0;
	for (unsigned int I = 0; I < sizeof(includesOneOfTagSet) * 8; I++)
		if (includesOneOfTagSet & ((int64_t)1 << I))
			lp_snprintf(temp + lp_strlen(temp), 1024 - lp_strlen(temp), u"%s ", desiredTagSets[I].name.c_str());
	if (temp[0]) ::lplog(logTypes, u"One Of Tag Sets: %s", temp);
	temp[0] = 0;
	for (unsigned int I = 0; I < sizeof(includesOnlyDescendantsAllOfTagSet) * 8; I++)
		if (includesOnlyDescendantsAllOfTagSet & ((int64_t)1 << I))
			lp_snprintf(temp + lp_strlen(temp), 1024 - lp_strlen(temp), u"%s ", desiredTagSets[I].name.c_str());
	if (temp[0]) ::lplog(logTypes, u"All Of Tag Sets (only descendants): %s", temp);
	temp[0] = 0;
	for (unsigned int I = 0; I < sizeof(includesDescendantsAndSelfAllOfTagSet) * 8; I++)
		if (includesDescendantsAndSelfAllOfTagSet & ((int64_t)1 << I))
			lp_snprintf(temp + lp_strlen(temp), 1024 - lp_strlen(temp), u"%s ", desiredTagSets[I].name.c_str());
	if (temp[0]) ::lplog(logTypes, u"All Of Tag Sets (descendants and self): %s", temp);
	for (unordered_map < lpwstring, int >::iterator vlpi = variableToLocationMap.begin(), vlpiEnd = variableToLocationMap.end(); vlpi != vlpiEnd; vlpi++)
		::lplog(logTypes, u"variable %s mapped to location %d.", vlpi->first.c_str(), vlpi->second);
	for (unordered_map < lpwstring, int >::iterator vlpi = variableToLengthMap.begin(), vlpiEnd = variableToLengthMap.end(); vlpi != vlpiEnd; vlpi++)
		::lplog(logTypes, u"variable %s mapped to length %d.", vlpi->first.c_str(), vlpi->second);
	for (unordered_map < int, lpwstring >::iterator lvmi = locationToVariableMap.begin(), lvmiEnd = locationToVariableMap.end(); lvmi != lvmiEnd; lvmi++)
		::lplog(logTypes, u"location %d mapped to variable %s.", lvmi->first, lvmi->second.c_str());
	for (unsigned int I = 0; I < elements.size(); I++)
	{
		lpwstring formsString;
		for (unsigned int J = 0; J < elements[I]->formCosts.size(); J++)
			formsString = formsString + elements[I]->toText(temp, J, false, 1024) + u" ";
		for (unsigned int J = 0; J < elements[I]->patternCosts.size(); J++)
			formsString = formsString + elements[I]->toText(temp, J, true, 1024) + u" ";
		lpwstring sFlags;
		::lplog(logTypes, u"%d:%d-%d %s %s", I, elements[I]->minimum, elements[I]->maximum, elements[I]->inflectionFlagsToStr(sFlags), formsString.c_str());
	}
}

// One-line dump: num:name[diff] variables and [inflection forms] per element.
void cPattern::lplogShort(lpwstring patternType, int logTypes)
{
	LFS
		lpchar_t temp[1024], logstr[1024];
	int len = 0;
	len += lp_wsprintf(logstr, u"%u:%s[%s] ", num, name.c_str(), differentiator.c_str());
	for (unordered_map < lpwstring, int >::iterator vlpi = variableToLocationMap.begin(), vlpiEnd = variableToLocationMap.end(); vlpi != vlpiEnd; vlpi++)
		len += lp_wsprintf_at(logstr, len, u"%s->%d ", vlpi->first.c_str(), vlpi->second);
	for (unordered_map < lpwstring, int >::iterator vlpi = variableToLengthMap.begin(), vlpiEnd = variableToLengthMap.end(); vlpi != vlpiEnd; vlpi++)
		len += lp_wsprintf_at(logstr, len, u"%s->%d ", vlpi->first.c_str(), vlpi->second);
	for (unordered_map < int, lpwstring >::iterator lvmi = locationToVariableMap.begin(), lvmiEnd = locationToVariableMap.end(); lvmi != lvmiEnd; lvmi++)
		len += lp_wsprintf_at(logstr, len, u"%d->%s ", lvmi->first, lvmi->second.c_str());
	for (unsigned int I = 0; I < elements.size(); I++)
	{
		lpwstring formsString;
		for (unsigned int J = 0; J < elements[I]->formCosts.size(); J++)
			formsString = formsString + elements[I]->toText(temp, J, false, 1024) + u" ";
		for (unsigned int J = 0; J < elements[I]->patternCosts.size(); J++)
			formsString = formsString + elements[I]->toText(temp, J, true, 1024) + u" ";
		lpwstring sFlags;
		elements[I]->inflectionFlagsToStr(sFlags);
		if (sFlags.size())
			sFlags += u" ";
		len += lp_wsprintf_at(logstr, len, u"[%s%s] ", sFlags.c_str(), formsString.c_str());
	}
	// Batch B5: clamp before terminating. `len` accumulates lp_snprintf's
	// would-have-written return, so a truncated line leaves it past the end of the
	// buffer; the old unbounded wsprintf would simply have overflowed by then.
	if (len > 1023) len = 1023;
	logstr[len] = 0;
	::lplog(logTypes, u"%s:%s", patternType.c_str(), logstr);
}

// Log ever-matched / final-match counts for every form and child-pattern alternative.
void cPattern::reportUsage(void)
{
	LFS
		lpchar_t temp[1024];
	lpwstring fullname = name + u"[" + differentiator + u"]";
	lp_wsprintf(temp, u"%40s ", fullname.c_str());
	for (unsigned int I = 0; I < elements.size(); I++)
	{
		for (unsigned int J = 0; J < elements[I]->formCosts.size(); J++)
			elements[I]->reportUsage(temp, 1024, J, false);
		for (unsigned int J = 0; J < elements[I]->patternCosts.size(); J++)
			elements[I]->reportUsage(temp, 1024, J, true);
	}
}

// Serialize pattern number, element count, and each element's four usage
// vectors into buf.  Returns false if the image is full.
bool cPattern::copyUsage(void* buf, int& where, int limit)
{
	DLFS
		if (!::copy(buf, num, where, limit)) return false;
	if (!::copy(buf, (unsigned int)elements.size(), where, limit)) return false; // batch B6a: size_t matches several copy() overloads equally well
	for (unsigned int I = 0; I < elements.size(); I++)
		if (!elements[I]->copyUsage(buf, where, limit))
			return false;
	return true;
}

// Zero every element's usage counters (std::fill; no-op if they were never sized).
void cPattern::zeroUsage()
{
	for (unsigned int I = 0; I < elements.size(); I++)
		elements[I]->zeroUsage();
}

// Bind every existing pattern named `patternName` as an alternative of
// elementNum (the resolve() of a cPatternReference).  Skips a self-match
// unless elementAllowRecursiveMatch.  A reference to a not-yet-created
// (higher-numbered) pattern sets containsFutureReference and may log.
// Returns true on resolve failure OR a logged future-ref error (the `|`
// is bitwise).
bool cPattern::add(int elementNum, lpwstring patternName, bool logFutureReferences, int elementCost, set <unsigned int> elementTags,
	bool elementBlockDescendants, bool elementAllowRecursiveMatch)
{
	LFS
		bool found = false, error = false;
	unsigned int p = 0;
	//int mandatoryChild=-1;
	while (findPattern(patternName, p) < patterns.size())
	{
		if (p != num || elementAllowRecursiveMatch)
		{
			if (cPatternReference::lastPatternReference < (int)p)
				cPatternReference::lastPatternReference = p;
			elements[elementNum]->patternStr.push_back(patternName);
			childPatterns.set(p);
			patterns[p]->parentPatterns.set(num);
			elements[elementNum]->patternIndexes.push_back(p);
			elements[elementNum]->patternCosts.push_back(elementCost);
			if (!patterns[p]->blockDescendants && !elementBlockDescendants) // if the pattern or the element has blocked descendants
			{
				//if (p>num || patterns[p]->delayedDescendants.size())
				descendantPatterns.push_back(p);
				//for (set<unsigned int>::iterator dt=patterns[p]->descendantTags.begin(),dtEnd=patterns[p]->descendantTags.end(); dt!=dtEnd; dt++)
				//  descendantTags.insert(*dt);
			}
			elements[elementNum]->patternTags.push_back(elementTags);
			if (!elementBlockDescendants) // the pattern tags override a pattern block (which is a block of its descendants), but not an element block of its parent.
				descendantTags.insert(patterns[p]->tags.begin(), patterns[p]->tags.end());
			elements[elementNum]->patternStopDescendingSearch.push_back(elementBlockDescendants);
			if (patterns[p]->containsFutureReference || patterns[p]->indirectFutureReference)
				indirectFutureReference = true;
			if (p > num)
			{
				if (!found) logFutureReferences = false; // all the patterns referred to are not defined yet, so there is no confusion.
				// this lplog message is only to help with patterns accidentally referring to patterns defined later.
				patterns[p]->isFutureReference = true;
				containsFutureReference = true;
				if (logFutureReferences)
				{
					::lplog(u"Pattern %s[%s] element %s matches also against future pattern %s[%s].",
						name.c_str(), differentiator.c_str(), patternName.c_str(), patterns[p]->name.c_str(), patterns[p]->differentiator.c_str());
					error = true;
				}
			}
			found = true;
		}
#ifdef LOG_PATTERN_BUILDING
		::lplog(u"Pattern %s[%s] element %s blocked self match against pattern %s[%s].",
			name.c_str(), differentiator.c_str(), patternName.c_str(), patterns[p]->name.c_str(), patterns[p]->differentiator.c_str());
#endif
		p++;
	}
	if (!found)
		::lplog(u"Resolve failure: pattern %s not found while defining pattern %s[%s].", patternName.c_str(), name.c_str(), differentiator.c_str());
	return (!found) | error;
}

// Recursively set ancestorPatterns to every parent of childPattern (and
// their already-computed ancestors).  Called as setAncestorPatterns(p) with
// p==this->num during initializePatterns.
void cPattern::setAncestorPatterns(int childPattern)
{
	LFS
		for (unsigned int p = 0; p < patterns.size(); p++)
			if (patterns[childPattern]->parentPatterns.isSet(p) && !ancestorPatterns.isSet(p))
			{
				ancestorPatterns.set(p);
				if (patterns[p]->ancestorsSet)
					ancestorPatterns |= patterns[p]->ancestorPatterns;
				else
					setAncestorPatterns(p);
			}
}

// Same walk as setAncestorPatterns but for the mandatory-parent bitset.
void cPattern::setMandatoryAncestorPatterns(int childPattern)
{
	LFS
		for (unsigned int p = 0; p < patterns.size(); p++)
			if (patterns[childPattern]->mandatoryParentPatterns.isSet(p) && !mandatoryAncestorPatterns.isSet(p))
			{
				mandatoryAncestorPatterns.set(p);
				if (patterns[p]->mandatoryAncestorsSet)
					mandatoryAncestorPatterns |= patterns[p]->mandatoryAncestorPatterns;
				else
					setMandatoryAncestorPatterns(p);
			}
}

// if the pattern will be eliminated on these grounds DO NOT set maxMatch
// otherwise, patterns not matching up to these # of elements will be eliminated even though they are
//   the winners. - used in eliminateLoserPatterns
// True if this match is allowed to update maxLACMatch: _FINAL, or
// _FINAL_IF_ALONE / _FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN and the
// span is bounded by separators (or the document edges).
bool cPattern::isTopLevelMatch(cSource& source, unsigned int beginPosition, unsigned int endPosition)
{
	LFS
		return fillFlag ||
		((fillIfAloneFlag || onlyAloneExceptInSubPatternsFlag) &&
			((!beginPosition || source.m[beginPosition - 1].word->second.isSeparator()) &&
				(source.m.size() <= endPosition || source.m[endPosition].word->second.isSeparator())));
}

/*
if an element contains a pattern that is not found,
fill element with pattern # -1.
add entry to patternReferences with pattern # and pointer to element.
for each entry in patternReferences:
erase undefined entries.
insert all definitions of pattern not found at time of definition
accumulate LAST (largest) future reference over all entries in patternReferences.

when pattern matching:
match until the LAST pattern
while (true)
match over all patterns having future references.
if no matches, exit.
match over all patterns incorporating these patterns up to the last cPatternReference (LAST)
repeat
continue matching patterns after LAST pattern
*/

// Bind this forward ref into the global `patterns` table.  ORs any failure
// into patternError (initializePatterns then exit(0)s).
void cPatternReference::resolve(bool& patternError)
{
	LFS
		patternError |= patterns[patternNum]->add(elementNum, form, logFutureReferences, cost, tags, blockDescendants, allowRecursiveMatch);
}

// Same as resolve() but against a caller-supplied pattern vector (QA transforms).
void cPatternReference::resolve(vector <cPattern*>& transformPatterns, bool& patternError)
{
	LFS
		patternError |= transformPatterns[patternNum]->add(elementNum, form, logFutureReferences, cost, tags, blockDescendants, allowRecursiveMatch);
}

// True if `tag` appears on any descendant (or was percolated into descendantTags).
bool cPattern::hasDescendantTag(unsigned int tag)
{
	LFS
		return descendantTags.find(tag) != descendantTags.end();
	/*
	for (set <unsigned int>::iterator dt=descendantTags.begin(),dtEnd=descendantTags.end(); dt!=dtEnd; dt++)
	if (tag==*dt)
	return true;
	return false;
	*/
}

// True if this pattern itself is tagged `tag` (not merely a descendant).
bool cPattern::hasTag(unsigned int tag)
{
	LFS
		return tags.find(tag) != tags.end();
}

// a pattern may only contain some elements.  Only if it has no elements of the tag set will it be marked false.
// True if any of desiredTagSet.tags[0..limit) is on this pattern (includeSelf)
// or its descendants.
bool cPattern::containsOneOfTagSet(cTagSet& desiredTagSet, unsigned int limit, bool includeSelf)
{
	LFS
		if (includeSelf)
		{
			for (unsigned tag = 0; tag < limit; tag++)
				if (hasDescendantTag(desiredTagSet.tags[tag]) || hasTag(desiredTagSet.tags[tag])) return true;
			return false;
		}
	for (unsigned tag = 0; tag < limit; tag++)
		if (hasDescendantTag(desiredTagSet.tags[tag]))
			return true;
	return false;
}

// If required<0: true when at least |required| of the first |required| tags
// are present (containsOneOf).  If required>0: true only when ALL of the
// first `required` tags are present (on descendants, and on self if includeSelf).
bool cPattern::setCheckDescendantsForTagSet(cTagSet& desiredTagSet, bool includeSelf)
{
	LFS
		if (desiredTagSet.required < 0)
			return containsOneOfTagSet(desiredTagSet, (unsigned)(-desiredTagSet.required), includeSelf);
	if (includeSelf)
	{
		for (unsigned tag = 0; tag < (unsigned)desiredTagSet.required; tag++)
			if (!hasDescendantTag(desiredTagSet.tags[tag]) && !hasTag(desiredTagSet.tags[tag])) return false;
		return true;
	}
	for (unsigned tag = 0; tag < (unsigned)desiredTagSet.required; tag++)
		if (!hasDescendantTag(desiredTagSet.tags[tag])) return false;
	return true;
}

// Index of desiredTagSets named tagSet.  Fatals if missing (the return -1 is dead).
unsigned int findTagSet(lpchar_t* tagSet)
{
	LFS
		for (unsigned int I = 0; I < desiredTagSets.size(); I++)
			if (desiredTagSets[I].name == tagSet)
				return I;
	lplog(LOG_FATAL_ERROR, u"Tagset %s not found!", tagSet);
	return -1;
}

unsigned int verbObjectsTagSet;
unsigned int iverbTagSet;
unsigned int nounDeterminerTagSet;
unsigned int subjectVerbAgreementTagSet;
unsigned int subjectTagSet;
unsigned int specificAnaphorTagSet;
unsigned int descendantAgreementTagSet;
unsigned int objectTagSet;
unsigned int subjectVerbRelationTagSet;
unsigned int verbObjectRelationTagSet;
unsigned int verbSenseTagSet;
unsigned int nameTagSet;
unsigned int metaNameEquivalenceTagSet;
unsigned int roleTagSet;
unsigned int prepTagSet;
unsigned int BNCPreferencesTagSet;
unsigned int nAgreeTagSet;
unsigned int EVALTagSet;
unsigned int idRelationTagSet;
unsigned int metaSpeakerTagSet;
unsigned int notbutTagSet;
unsigned int mobjectTagSet;
unsigned int ndPrepTagSet;
unsigned int timeTagSet;
unsigned int qtobjectTagSet;
unsigned int twoObjectTestTagSet;

unsigned int PREP_TAG, OBJECT_TAG, SUBOBJECT_TAG, REOBJECT_TAG, IOBJECT_TAG, SUBJECT_TAG, PREP_OBJECT_TAG, VERB_TAG, IVERB_TAG, PLURAL_TAG, MPLURAL_TAG, GNOUN_TAG, MNOUN_TAG, PNOUN_TAG, VNOUN_TAG, HAIL_TAG, NAME_TAG, REL_TAG, SENTENCE_IN_REL_TAG, FLOAT_TIME_TAG, NOUN_TAG;

// Append every tag of desiredTagSets[tagSet] onto this set (used to fold
// verbSense tags into the relation tag sets).
void cTagSet::addTagSet(int tagSet)
{
	LFS
		for (unsigned int I = 0; I < desiredTagSets[tagSet].tags.size(); I++)
			tags.push_back(desiredTagSets[tagSet].tags[I]);
}

// the first number argument is the number of arguments ALL must be in the pattern before the pattern field includesAllOfTagSet bit
// for the tagset.  If the first number is negative it indicates the number of AT LEAST ONE of the arguments that must be in the pattern.
// Build desiredTagSets and intern the hot tag ids (PREP_TAG, OBJECT_TAG, ...).
// startSuperTagSets is set to the first "super" set (currently
// _DESCENDANTS_HAVE_AGREEMENT) so evaluateTagSets can percolate those last.
void initializeTagSets(int& startSuperTagSets)
{
	LFS
		desiredTagSets.push_back(cTagSet(verbSenseTagSet, u"_VERB_SENSE", -28,
			u"no", u"never", u"not", u"past", u"imp", u"future", u"id", u"conditional", u"vS", u"vAC", u"vC", u"vD", u"vB", u"vAB", u"vBC", u"vABC", u"vCD", u"vBD", u"vABD", u"vACD", u"vBCD", u"vABCD", u"vE", u"vrBD", u"vrB", u"vrBC", u"vrD", u"vAD", NULL));
	desiredTagSets.push_back(cTagSet(subjectVerbAgreementTagSet, u"_AGREEMENT", 3, u"SUBJECT", u"VERB", u"V_AGREE", u"V_OBJECT", u"conditional", u"future", u"past", u"SUBJUNCTIVE", NULL)); // V_OBJECT used for relations
	// "N_AGREE",u"GNOUN",u"SINGULAR",u"PLURAL" were put into SUBJECT_TAGSET because these tags also belong in OBJECTS which are after the verb, have nothing to
	// do with agreement and yet greatly multiply the number of tagsets.
	desiredTagSets.push_back(cTagSet(subjectTagSet, u"_SUBJECT", -4, u"N_AGREE", u"GNOUN", u"MNOUN", u"NAME", u"SINGULAR", u"PLURAL", u"RE_OBJECT", u"MOBJECT", NULL));
	desiredTagSets.push_back(cTagSet(specificAnaphorTagSet, u"_SPECIFIC_ANAPHOR", -3, u"N_AGREE", u"GNOUN", u"MNOUN", u"PLURAL", u"SUBJECT", u"V_AGREE", NULL));
	// VERB_OBJECTS_TAGSET: although a verb does not need to have objects to be assessed for the # of objects it has, the pattern must allow for objects.
	desiredTagSets.push_back(cTagSet(verbObjectsTagSet, u"_VERB_OBJECTS", 3, u"VERB", u"V_OBJECT", u"OBJECT", u"HOBJECT", u"ADVOBJECT", u"ADJOBJECT", u"V_AGREE",
		u"vD", u"vrD", u"vAD", u"vBD", u"vCD", u"vABD", u"vACD", u"vBCD", u"vABCD", u"IVERB", u"PT", NULL));
	desiredTagSets.push_back(cTagSet(iverbTagSet, u"_IVERB", 1, u"ITO", u"V_OBJECT", u"OBJECT", u"IVERB", u"PREP", u"REL", u"ADJ", u"ADV", u"HOBJECT", u"V_AGREE", u"V_HOBJECT", u"VERB2", u"ADJOBJECT", u"ADVOBJECT", u"MNOUN", u"MVERB", u"S_IN_REL", NULL)); // ITO is simply a infinitive marker
	desiredTagSets.push_back(cTagSet(nounDeterminerTagSet, u"_NOUN_DETERMINER", 2, u"NOUN", u"N_AGREE", u"DET", u"SUBOBJECT", NULL));
	// VNOUN: noun which is an activity (running, or Bill running down the street)
	// PNOUN: only personal nouns such as he, she, herself, them, etc.
	// GNOUN: address, date, time, telephone number, number,
	// MNOUN: a noun represented syntactically by a single entity but which is made up of multiple nouns (blue, gray and purple)
	// ADJOBJECT: the latter, the former
	desiredTagSets.push_back(cTagSet(objectTagSet, u"_OBJECTS", -6, u"NOUN", u"VNOUN", u"PNOUN", u"GNOUN", u"NAME", u"ADJOBJECT", u"NAMEOWNER", NULL));
	// RE_OBJECT is an object restated, IOBJECT is an immediate object of an infinitive phrase, PREPOBJECT is an object of a prepositional phrase, and SUBOBJECT is everything else.
	desiredTagSets.push_back(cTagSet(roleTagSet, u"_ROLE", -6, u"SUBJECT", u"OBJECT", u"RE_OBJECT", u"SUBOBJECT", u"IOBJECT", u"PREPOBJECT", u"HAIL", NULL));
	desiredTagSets.push_back(cTagSet(subjectVerbRelationTagSet, u"_SUBJECT_VERB_RELATION", 3, u"VERB", u"V_OBJECT", u"SUBJECT", u"OBJECT", u"PREP", u"IVERB", u"REL", u"ADJ", u"ADV", u"HOBJECT", u"V_AGREE", u"V_HOBJECT", u"VERB2", u"ADJOBJECT", u"ADVOBJECT", u"MNOUN", u"MVERB", u"S_IN_REL", u"QTYPE", u"not", NULL));
	desiredTagSets.push_back(cTagSet(verbObjectRelationTagSet, u"_VERB_OBJECT_RELATION", 3, u"VERB", u"V_OBJECT", u"OBJECT", u"SUBJECT", u"PREP", u"IVERB", u"REL", u"ADJ", u"ADV", u"HOBJECT", u"V_AGREE", u"V_HOBJECT", u"VERB2", u"ADJOBJECT", u"ADVOBJECT", u"MNOUN", u"MVERB", u"S_IN_REL", u"QTYPE", NULL));
	desiredTagSets.push_back(cTagSet(nameTagSet, u"_NAME", -7, u"FIRST", u"MIDDLE", u"LAST", u"QLAST", u"ANY", u"HON", u"BUS", u"HON2", u"HON3", u"SUFFIX", u"SINGULAR", u"PLURAL", NULL));
	desiredTagSets.push_back(cTagSet(metaNameEquivalenceTagSet, u"_META_NAME_EQUIVALENCE", -3, u"NAME_PRIMARY", u"NAME_SECONDARY", u"NAME_ABOUT", NULL));
	desiredTagSets.push_back(cTagSet(metaSpeakerTagSet, u"_META_SPEAKER", 1, u"NAME_PRIMARY", NULL));
	desiredTagSets.push_back(cTagSet(prepTagSet, u"_PREP", 1, u"P", u"PREPOBJECT", u"PREP", u"REL", u"S_IN_REL", NULL));
	desiredTagSets.push_back(cTagSet(BNCPreferencesTagSet, u"_BNC_PREFERENCES_TAGSET", -4, u"NOUN", u"VERB", u"ADJ", u"ADV", NULL));
	desiredTagSets.push_back(cTagSet(nAgreeTagSet, u"_N_AGREE_TAGSET", 1, u"N_AGREE", NULL));
	desiredTagSets.push_back(cTagSet(EVALTagSet, u"_EVAL_TAGSET", -3, u"EVAL", u"REL", u"IVERB", NULL));
	desiredTagSets.push_back(cTagSet(idRelationTagSet, u"_ID_RELATION_TAGSET", 3, u"id", u"OBJECT", u"SUBJECT", NULL));
	desiredTagSets.push_back(cTagSet(notbutTagSet, u"_NOTBUT_TAGSET", 1, u"not", u"but", NULL));
	desiredTagSets.push_back(cTagSet(mobjectTagSet, u"_MOBJECT_TAGSET", 1, u"MOBJECT", NULL));
	desiredTagSets.push_back(cTagSet(qtobjectTagSet, u"_QUESTION", 1, u"QTYPE", NULL));
	// the following is used because noun determiners need to scan in a sentence and find all prepositional phrases as well
	//    as objects, but _ROLE tagset cannot be used because PREP is not a object type, and PREPOBJECT is blocked 
	desiredTagSets.push_back(cTagSet(ndPrepTagSet, u"_NDP", 1, u"PREP", NULL));
	desiredTagSets.push_back(cTagSet(timeTagSet, u"_TIME", -10, u"TIMESPEC", u"HOUR", u"TIMEMODIFIER", u"TIMECAPACITY", u"TIMETYPE", u"DAYMONTH", u"MONTH", u"YEAR", u"DATESPEC", u"DAYWEEK", u"SEASON", u"HOLIDAY", u"MINUTE", NULL));
	desiredTagSets.push_back(cTagSet(twoObjectTestTagSet, u"_TOT", -3, u"PREP", u"SUBJECT", u"REL", u"IVERB", NULL));

	startSuperTagSets = desiredTagSets.size();
	// these tagsets indicate the pattern has the descendant tagset which is blocked.
	desiredTagSets.push_back(cTagSet(descendantAgreementTagSet, u"_DESCENDANTS_HAVE_AGREEMENT", 1, u"_AGREEMENT", NULL));

#ifdef LOG_UNUSED_TAGS
	bool* tagUsed = (bool*)tcalloc(patternTagStrings.size(), sizeof(bool));
	for (unsigned dts = 0; dts < desiredTagSets.size(); dts++)
		for (unsigned I = 0; I < desiredTagSets[dts].tags.size(); I++)
			tagUsed[desiredTagSets[dts].tags[I]] = true;
	for (unsigned I = 0; I < patternTagStrings.size(); I++)
		if (!tagUsed[I] && patternTagStrings[I][0] != u'_' && patternTagStrings[I] != u"_BLOCK")
			lplog(u"TAG %s not used in tagSet.", patternTagStrings[I].c_str());
#endif
	desiredTagSets[iverbTagSet].addTagSet(verbSenseTagSet);
	desiredTagSets[subjectVerbRelationTagSet].addTagSet(verbSenseTagSet);
	desiredTagSets[verbObjectRelationTagSet].addTagSet(verbSenseTagSet);

	PREP_TAG = findTag(u"PREP");
	SUBOBJECT_TAG = findTag(u"SUBOBJECT");
	OBJECT_TAG = findTag(u"OBJECT");
	REOBJECT_TAG = findTag(u"RE_OBJECT");
	IOBJECT_TAG = findTag(u"IOBJECT");
	SUBJECT_TAG = findTag(u"SUBJECT");
	PREP_OBJECT_TAG = findTag(u"PREPOBJECT");
	VERB_TAG = findTag(u"VERB");
	IVERB_TAG = findTag(u"IVERB");
	PLURAL_TAG = findTag(u"PLURAL");
	MPLURAL_TAG = findTag(u"MPLURAL");
	SENTENCE_IN_REL_TAG = findTag(u"S_IN_REL");
	NOUN_TAG = findTag(u"NOUN");
	GNOUN_TAG = findTag(u"GNOUN");
	MNOUN_TAG = findTag(u"MNOUN");
	PNOUN_TAG = findTag(u"PNOUN");
	VNOUN_TAG = findTag(u"VNOUN");
	HAIL_TAG = findTag(u"HAIL");
	NAME_TAG = findTag(u"NAME");
	REL_TAG = findTag(u"REL");
	FLOAT_TIME_TAG = findTag(u"FLOATTIME");
}

// Next tag of this pattern that is in desiredTagSets[desiredTagSetNum],
// starting from `tag` (in/out, incremented past the hit).  Returns the tag
// id, or -1 if the inclusion bitset is empty / exhausted.
int cPattern::hasTagInSet(int desiredTagSetNum, unsigned int& tag)
{
	LFS
		if (!tagSetMemberInclusion[desiredTagSetNum]) return -1;
	size_t numTags = desiredTagSets[desiredTagSetNum].tags.size();
	for (; tag < numTags; tag++)
		if (tagSetMemberInclusion[desiredTagSetNum] & ((int64_t)1 << tag))
			return desiredTagSets[desiredTagSetNum].tags[tag++];
	return -1;
}

// Format alternative J into temp: "name[diff]*cost{_BLOCK}{TAG:TAG}" or
// "form|word*cost...".  wcscat / lp_snprintf with a maxBuf cap on the tags only.
lpchar_t* cPatternElement::toText(lpchar_t* temp, int J, bool isPattern, int maxBuf)
{
	LFS
		temp[0] = 0;
	if (isPattern)
	{
		lp_snprintf(temp, maxBuf, u"%s[%s]*%d%s", patterns[patternIndexes[J]]->name.c_str(), patterns[patternIndexes[J]]->differentiator.c_str(), patternCosts[J], (patternStopDescendingSearch[J]) ? u"{_BLOCK}" : u"");
		if (patternTags[J].size())
		{
			lp_strcpy((temp) + lp_strlen(temp), u"{");
			for (set <unsigned int>::iterator t = patternTags[J].begin(), tEnd = patternTags[J].end(); t != tEnd; t++)
			{
				lp_snprintf(temp + lp_strlen(temp), maxBuf - lp_strlen(temp) - 1, u"%s:", patternTagStrings[*t].c_str());
				temp[maxBuf - 1] = 0;
			}
			temp[lp_strlen(temp) - 1] = u'}';
		}
	}
	else
	{
		lpwstring fs = formStr[J];
		if (specificWords[J].length())
			fs += u"|" + specificWords[J];
		lp_snprintf(temp, maxBuf, u"%s*%d%s", fs.c_str(), formCosts[J], (formStopDescendingSearch[J]) ? u"{_BLOCK}" : u"");
		if (formTags[J].size())
		{
			lp_strcpy((temp) + lp_strlen(temp), u"{");
			for (set <unsigned int>::iterator t = formTags[J].begin(), tEnd = formTags[J].end(); t != tEnd; t++)
			{
				lp_snprintf(temp + lp_strlen(temp), maxBuf - lp_strlen(temp) - 1, u"%s:", patternTagStrings[*t].c_str());
				temp[maxBuf - 1] = 0;
			}
			temp[lp_strlen(temp) - 1] = u'}';
		}
	}
	return temp;
}

// Serialize the four usage vectors.  Returns false if the image is full.
bool cPatternElement::copyUsage(void* buf, int& where, int limit)
{
	if (!::copy(buf, usageFormEverMatched, where, limit)) return false;
	if (!::copy(buf, usageFormFinalMatch, where, limit)) return false;
	if (!::copy(buf, usagePatternEverMatched, where, limit)) return false;
	if (!::copy(buf, usagePatternFinalMatch, where, limit)) return false;
	return true;
}

// Zero the four usage vectors in place (size unchanged).
void cPatternElement::zeroUsage()
{
	std::fill(usageFormEverMatched.begin(), usageFormEverMatched.end(), 0);
	std::fill(usageFormFinalMatch.begin(), usageFormFinalMatch.end(), 0);
	std::fill(usagePatternEverMatched.begin(), usagePatternEverMatched.end(), 0);
	std::fill(usagePatternFinalMatch.begin(), usagePatternFinalMatch.end(), 0);
}

// Append one alternative's ever/final counts onto temp and log the line,
// then restore temp to its incoming length so the caller can reuse the prefix.
// Batch B5: tempCount added. `temp` is a pointer parameter, so the bounded
// lp_wsprintf used everywhere else cannot deduce its size; the caller's buffer
// length is passed explicitly instead. Previously this appended with the unbounded
// Win32 wsprintf into a buffer whose size it had no way to know.
void cPatternElement::reportUsage(lpchar_t* temp, size_t tempCount, int J, bool isPattern)
{
	LFS
		size_t len = lp_strlen(temp);
	if (len >= tempCount) return;
	if (isPattern)
	{
		lpwstring fullname = patterns[patternIndexes[J]]->name + u"[" + patterns[patternIndexes[J]]->differentiator + u"]";
		lp_snprintf(temp + len, tempCount - len, u"%40s  %05d  %05d", fullname.c_str(), usagePatternEverMatched[J], usagePatternFinalMatch[J]);
	}
	else
	{
		lpwstring fs = formStr[J];
		if (specificWords[J].length())
			fs += u"|" + specificWords[J];
		lp_snprintf(temp + len, tempCount - len, u"%40s  %05d  %05d", fs.c_str(), usageFormEverMatched[J], usageFormFinalMatch[J]);
	}
	::lplog(u"%s", temp);
	temp[len] = 0;
}

// True if any form or child-pattern alternative of this element carries `tag`.
bool cPatternElement::hasTag(unsigned int tag)
{
	LFS
		for (vector < set <unsigned int> >::iterator I = patternTags.begin(), IEnd = patternTags.end(); I != IEnd; I++)
			if (I->find(tag) != I->end())
				return true;
	for (vector < set <unsigned int> >::iterator I = formTags.begin(), IEnd = formTags.end(); I != IEnd; I++)
		if (I->find(tag) != I->end())
			return true;
	return false;
}

// True if alternative elementIndex (form or pattern) carries `tag`.
bool cPatternElement::hasTag(unsigned int elementIndex, unsigned int tag, bool isPattern)
{
	LFS
		if (isPattern)
		{
			if (patternTags[elementIndex].find(tag) != patternTags[elementIndex].end())
				return true;
		}
		else
			if (formTags[elementIndex].find(tag) != formTags[elementIndex].end())
				return true;
	return false;
}

// Next tag of alternative elementIndex that is in desiredTagSetNum, starting
// from tagNumBySet (in/out).  Returns the tag id or -1.
int cPatternElement::hasTagInSet(unsigned int elementIndex, unsigned int desiredTagSetNum, unsigned int& tagNumBySet, bool isPattern)
{
	LFS
		if (isPattern)
		{
			for (; tagNumBySet < desiredTagSets[desiredTagSetNum].tags.size(); tagNumBySet++)
				if (find(patternTags[elementIndex].begin(), patternTags[elementIndex].end(), desiredTagSets[desiredTagSetNum].tags[tagNumBySet]) != patternTags[elementIndex].end())
					return desiredTagSets[desiredTagSetNum].tags[tagNumBySet++];
		}
		else
		{
			for (; tagNumBySet < desiredTagSets[desiredTagSetNum].tags.size(); tagNumBySet++)
				if (find(formTags[elementIndex].begin(), formTags[elementIndex].end(), desiredTagSets[desiredTagSetNum].tags[tagNumBySet]) != formTags[elementIndex].end())
					return desiredTagSets[desiredTagSetNum].tags[tagNumBySet++];
		}
	return -1;
}

// Wrapper: -2 in the (signed) elementIndex means "optional skip placeholder"
// and yields -1.  Otherwise forwards to the element's hasTagInSet.
int cPattern::elementHasTagInSet(unsigned char cPatternElement, unsigned char elementIndex, unsigned int desiredTagSetNum, unsigned int& tagNumBySet, bool isPattern)
{
	DLFS
		if (((signed char)elementIndex) == -2) return -1;
	return elements[cPatternElement]->hasTagInSet(elementIndex, desiredTagSetNum, tagNumBySet, isPattern);
}

// True if alternative elementIndex of element cPatternElement carries `tag`.
// elementIndex==-2 (optional skip) is never a hit.
bool cPattern::elementHasTag(unsigned char cPatternElement, unsigned char elementIndex, int tag, bool isPattern)
{
	LFS
		return (((signed char)elementIndex) == -2 || !elements[cPatternElement]->hasTag(elementIndex, tag, isPattern)) ? false : true;
}

// the only descendant who is a leaf in the tree is the pattern itself.
// Walk descendantPatterns (skipping those already in ancestors to cut cycles).
// True if every descendant eventually bottoms out at `parent` or has no
// further descendants — i.e. this pattern's descendant graph is a cycle
// back to parent, so resolveDescendants can flatten it.
bool cPattern::onlyDescendant(unsigned int parent, vector <unsigned int>& ancestors)
{
	LFS
		for (unsigned int d = 0; d < descendantPatterns.size(); d++)
		{
			if (find(ancestors.begin(), ancestors.end(), descendantPatterns[d]) != ancestors.end()) continue;
			ancestors.push_back(descendantPatterns[d]);
			if (descendantPatterns[d] != parent && !patterns[descendantPatterns[d]]->onlyDescendant(parent, ancestors))
				return false;
		}
	return descendantPatterns.size() > 0;
}

// Flatten descendantPatterns whose own descendant list is empty or is a
// cycle back to this pattern: erase them and absorb their tags.  Returns
// true when descendantPatterns is empty (fully resolved).  initializePatterns
// loops this until every pattern returns true.  `circular` only enables a
// log line per descendant.
bool cPattern::resolveDescendants(bool circular)
{
	LFS
		for (unsigned int d = 0; d < descendantPatterns.size(); )
		{
			if (circular)
				::lplog(u"%s[%s]: descendant %s[%s].", name.c_str(), differentiator.c_str(), patterns[descendantPatterns[d]]->name.c_str(), patterns[descendantPatterns[d]]->differentiator.c_str());
			vector <unsigned int> ancestors;
			if (!patterns[descendantPatterns[d]]->descendantPatterns.size() || patterns[descendantPatterns[d]]->onlyDescendant(num, ancestors))
			{
				unsigned int p = descendantPatterns[d];
				descendantPatterns.erase(descendantPatterns.begin() + d);
				descendantTags.insert(patterns[p]->descendantTags.begin(), patterns[p]->descendantTags.end());
				descendantTags.insert(patterns[p]->tags.begin(), patterns[p]->tags.end());
			}
			else
				d++;
		}
	return descendantPatterns.size() == 0;
}

// For desiredTagSets[start, end): set includesOneOf / includesAll-of bitsets
// and tagSetMemberInclusion.  When start==0 and the pattern contains all
// required descendant tags, also insert the tag-set's NAME_TAG into this
// pattern's tags and every mandatory ancestor's descendantTags (so a blocked
// _AGREEMENT still percolates as _DESCENDANTS_HAVE_AGREEMENT).
void cPattern::evaluateTagSets(unsigned int start, unsigned int end)
{
	LFS
		//if (lastTagSetEvaluated>start) return;
		for (unsigned int dts = start; dts < end; dts++)
		{
			if (containsOneOfTagSet(desiredTagSets[dts], desiredTagSets[dts].tags.size(), false))
				includesOneOfTagSet |= (int64_t)1 << dts;
			for (unsigned tag = 0; tag < desiredTagSets[dts].tags.size(); tag++)
				if (hasTag(desiredTagSets[dts].tags[tag]))
					tagSetMemberInclusion[dts] |= ((int64_t)1 << tag);
			bool containsAll = setCheckDescendantsForTagSet(desiredTagSets[dts], false);
			if (containsAll)
			{
				includesOnlyDescendantsAllOfTagSet |= (int64_t)1 << dts;
				includesDescendantsAndSelfAllOfTagSet |= (int64_t)1 << dts;
			}
			else if (setCheckDescendantsForTagSet(desiredTagSets[dts], true))
				includesDescendantsAndSelfAllOfTagSet |= (int64_t)1 << dts;
			// this percolates all tag sets through all parents, ignoring any block tags
			// 10/25/2006 - this is so that super tag sets can find elements that are blocked
			if (!start && containsAll)
			{
				tags.insert(desiredTagSets[dts].NAME_TAG);
				for (unsigned p = 0; p < patterns.size(); p++)
					if (mandatoryAncestorPatterns.isSet(p))
						patterns[p]->descendantTags.insert(desiredTagSets[dts].NAME_TAG);
			}
		}
}

// Size and zero each element's usage counters now that patternIndexes is final.
void cPattern::initializeUsage()
{
	for (auto element : elements)
		element->initializeUsage();
}

// A child is mandatory if the element has minimum>0, exactly one pattern
// alternative, and no form alternatives.  Fills mandatoryChildPatterns and
// the child's mandatoryParentPatterns.
void cPattern::establishMandatoryChildPatterns(void)
{
	LFS
		for (vector <cPatternElement*>::iterator e = elements.begin(), eEnd = elements.end(); e != eEnd; e++)
			if ((*e)->minimum && (*e)->patternIndexes.size() == 1 && (*e)->formIndexes.size() == 0)
			{
				int p = (*e)->patternIndexes[0];
				mandatoryChildPatterns.set(p);
				patterns[p]->mandatoryParentPatterns.set(num);
			}
}

// Stage-1 entry: run every create* (basic/verb/secondary/infinitive/prep/
// question/letter-intro), resolve all cPatternReference entries (exit(0) on
// any future-ref log), initializeTagSets, reject duplicate name+differentiator,
// then compute ancestor bitsets, flatten descendant tags, evaluate tag sets
// and reserve usage counters.  SOURCE_VERSION is not bumped here — the
// caller must increment it when the resulting grammar changes.
void initializePatterns(void)
{
	LFS
		createBasicPatterns();
	createVerbPatterns();
	createSecondaryPatterns1();
	createInfinitivePhrases();
	createPrepositionalPhrases();
	createQuestionPatterns();
	createSecondaryPatterns2();
	createLetterIntroPatterns();

	bool patternError = false;
	for (unsigned int r = 0; r < patternReferences.size(); r++)
		patternReferences[r]->resolve(patternError);
	if (patternError)
		exit(0);
	//int lastTag=patternTagStrings.size();
	int startSuperTagSets;
	initializeTagSets(startSuperTagSets);
	set <lpwstring> namedifferentiators;
	bool patternDuplicateNameError = false;
	for (unsigned int p = 0; p < patterns.size(); p++)
	{
		if (namedifferentiators.find(patterns[p]->name + patterns[p]->differentiator) != namedifferentiators.end())
		{
			patternDuplicateNameError = true;
			lplog(LOG_ERROR | LOG_STDOUT, u"Pattern %s[%s] is a duplicate name+differentiator", patterns[p]->name.c_str(), patterns[p]->differentiator.c_str());
		}
		namedifferentiators.insert(patterns[p]->name + patterns[p]->differentiator);
		patterns[p]->establishMandatoryChildPatterns();
		if (patterns[p]->mandatoryChildPatterns.isEmpty())
			patternsWithNoChildren.set(p);
		if (patterns[p]->parentPatterns.isEmpty())
			patternsWithNoParents.set(p);
	}
	if (patternDuplicateNameError)
		exit(0);
	int startTime = clock();
	lp_wprintf(u"Evaluating tagsets...               \r");
	for (unsigned int p = 0; p < patterns.size(); p++)
	{
		patterns[p]->setAncestorPatterns(p);
		patterns[p]->ancestorsSet = true;
		patterns[p]->setMandatoryAncestorPatterns(p);
		patterns[p]->mandatoryAncestorsSet = true;
	}
	//startTime=clock();
	int unresolvedDescendants = -1;
	while (unresolvedDescendants)
	{
		unresolvedDescendants = 0;
		for (unsigned int p = 0; p < patterns.size(); p++)
			if (patterns[p]->resolveDescendants(false))
				patterns[p]->evaluateTagSets(0, startSuperTagSets);
			else
				unresolvedDescendants++;
	}
	for (unsigned int p = 0; p < patterns.size(); p++)
	{
		patterns[p]->evaluateTagSets(startSuperTagSets, desiredTagSets.size());
		patterns[p]->initializeUsage();
#ifdef LOG_PATTERNS
		patterns[p]->lplog();
#endif
	}
	lplog(LOG_INFO, u"Processing patterns took %d ms.", (clock() - startTime));
	lp_wprintf(u"Finished tagsets...               \r");
	//printPatternsInABNF("patterns.abnf",lastTag);
#ifdef LOG_AGREE_PATTERN_EVALUATION
	lplog(u"AGREEMENT PATTERNS");
	for (unsigned int p = 0; p < patterns.size(); p++)
		if (patterns[p]->includesDescendantsAndSelfAllOfTagSet & 1 << subjectVerbAgreementTagSet)
		{
			lplog(u"%d:AP:%s[%s]", p, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str());
			patterns[p]->evaluateAllTagPatternsForAgreement(1);
		}
	lplog(LOG_FATAL_ERROR, u"END AGREEMENT PATTERNS");
#endif
}

// Construct one named tag set from a NULL-terminated va_list of tag names.
// tagSetNum is set to the new desiredTagSets index.  requiredNumOfTags is
// the "all of first N" / "at least |N| of first |N|" count; extra tags after
// that are still stored and used by containsOneOf / hasTagInSet.
cTagSet::cTagSet(unsigned int& tagSetNum, const lpchar_t* tag, int requiredNumOfTags, ...)
{
	LFS
		va_list tagMarker;
	va_start(tagMarker, requiredNumOfTags);     /* Initialize variable arguments. */
	tagSetNum = desiredTagSets.size();
	name = tag;
	patternTagStrings.push_back(name);
	required = requiredNumOfTags;
	NAME_TAG = findTag(tag);
	for (unsigned int I = 1; true; I++)
	{
		lpchar_t* nextTag = va_arg(tagMarker, lpchar_t*);
		if (!nextTag) break;
		tags.push_back(findTag(nextTag));
	}
	va_end(tagMarker);              /* Reset variable arguments.      */
}

// Intern-table lookup of tagName in patternTagStrings.  Fatals if missing
// (the return -1 is dead).  Tags are interned as they are first seen in
// create() / cTagSet(), so this is only safe after initializePatterns
// (or after the tag has already been mentioned).
int findTag(const lpchar_t* tagName)
{
	LFS
		for (unsigned int tag = 0; tag < patternTagStrings.size(); tag++)
			if (patternTagStrings[tag] == tagName)
				return tag;
	lplog(LOG_FATAL_ERROR, u"Tag %s not found!", tagName);
	return -1;
}

// Next cTagLocation in tagSet named tagName, starting at nextTag+1.  On a
// hit, nextTag is set to the following same-name slot or -1 if this was the
// last.  Returns the hit index or -1.  Not span-constrained.
int findTag(vector <cTagLocation>& tagSet, const lpchar_t* tagName, int& nextTag)
{
	LFS
		for (unsigned int I = nextTag + 1; I < tagSet.size(); I++)
			if (patternTagStrings[tagSet[I].tag] == tagName)
			{
				int saveTag = I;
				for (I++; I < tagSet.size() && patternTagStrings[tagSet[I].tag] != tagName; I++);
				//|| (tagSet[I].sourcePosition==tagSet[saveTag].sourcePosition && tagSet[I].end==tagSet[saveTag].end)); I++);
				if (I == tagSet.size())
					nextTag = -1;
				else
					nextTag = I;
				return saveTag;
			}
	return -1;
}

// First cTagLocation in tagSet named tagName at or after start+1, or -1.
int findOneTag(vector <cTagLocation>& tagSet, const lpchar_t* tagName, int start)
{
	LFS
		for (unsigned int I = start + 1; I < tagSet.size(); I++)
			if (patternTagStrings[tagSet[I].tag] == tagName) return I;
	return -1;
}

// Like findTag, but the hit (and the "next" preview) must lie inside
// [parentBegin, parentEnd).  Used to collect child tags of one parent span.
int findTagConstrained(vector <cTagLocation>& tagSet, const lpchar_t* tagName, int& nextTag, unsigned int parentBegin, unsigned int parentEnd)
{
	LFS
		vector <cTagLocation>::iterator tsi = tagSet.begin() + nextTag + 1, tsiEnd = tagSet.end();
	for (unsigned int I = nextTag + 1; tsi != tsiEnd; I++, tsi++)
		if (tsi->sourcePosition >= parentBegin &&
			tsi->sourcePosition + tsi->len <= parentEnd &&
			patternTagStrings[tsi->tag] == tagName)
		{
			int saveTag = I;
			for (I++, tsi++; tsi != tsiEnd && (patternTagStrings[tsi->tag] != tagName || tsi->sourcePosition<parentBegin || tsi->len + tsi->sourcePosition>parentEnd); I++, tsi++);
			if (tsi == tsiEnd)
				nextTag = -1;
			else
				nextTag = I;
			return saveTag;
		}
	return -1;
}

// Overload: constrain to parentTag's [sourcePosition, sourcePosition+len).
int findTagConstrained(vector <cTagLocation>& tagSet, const lpchar_t* tagName, int& nextTag, cTagLocation& parentTag)
{
	LFS
		unsigned int parentBegin = parentTag.sourcePosition, parentEnd = parentBegin + parentTag.len;
	return findTagConstrained(tagSet, tagName, nextTag, parentBegin, parentEnd);
}

// Offset of desiredTag inside desiredTagSets[desiredTagSetNum], or -1.
int inSet(unsigned int desiredTagSetNum, int desiredTag)
{
	LFS
		size_t numTags = desiredTagSets[desiredTagSetNum].tags.size(), I = 0;
	for (; I < numTags && desiredTagSets[desiredTagSetNum].tags[I] != desiredTag; I++);
	return (I < numTags) ? I : -1;
}

// For each tag of desiredTagSetNum that appears in tagSet inside parentTag's
// span, set tagFilledArray[setOffset]=1.  Caller owns and zeroes the array.
void findTagSetConstrained(vector <cTagLocation>& tagSet, unsigned int desiredTagSetNum, char* tagFilledArray, cTagLocation& parentTag)
{
	LFS
		unsigned int parentBegin = parentTag.sourcePosition, parentEnd = parentBegin + parentTag.len;
	int setOffset;
	for (unsigned int I = 0; I < tagSet.size(); I++)
		if ((setOffset = inSet(desiredTagSetNum, tagSet[I].tag)) >= 0 &&
			tagSet[I].sourcePosition >= parentBegin &&
			tagSet[I].sourcePosition + tagSet[I].len <= parentEnd)
			tagFilledArray[setOffset] = 1;
}

// Unconstrained findTagSetConstrained — any occurrence in tagSet counts.
void findTagSet(vector <cTagLocation>& tagSet, unsigned int desiredTagSetNum, char* tagFilledArray)
{
	LFS
		int setOffset;
	for (unsigned int I = 0; I < tagSet.size(); I++)
		if ((setOffset = inSet(desiredTagSetNum, tagSet[I].tag)) >= 0)
			tagFilledArray[setOffset] = 1;
}


// Log every cTagLocation in tagSet.  ts>=0 prints a "TAGSET N:" header and
// includes PEMAOffset; ts<0 is the compact form used when embedding in a
// larger dump.
void printTagSet(int logType, const lpchar_t* descriptor, int ts, vector <cTagLocation>& tagSet)
{
	LFS
		vector <cTagLocation>::iterator its = tagSet.begin();
	vector <cTagLocation>::iterator itsEnd = tagSet.end();
	if (ts >= 0 && descriptor) ::lplog(logType, u"%s TAGSET %05d: #TAGS=%d", descriptor, ts, tagSet.size());
	if (ts >= 0)
		for (; its != itsEnd; its++)
		{
			if (its->isPattern)
				::lplog(logType, u"TAGSET %05d: %03d %s[%s] %06d:%s[%s](%d,%d) TAG %s [Element=%d]", ts, its->sourcePosition, patterns[its->parentPattern]->name.c_str(), patterns[its->parentPattern]->differentiator.c_str(),
					its->PEMAOffset, patterns[its->pattern]->name.c_str(), patterns[its->pattern]->differentiator.c_str(), its->sourcePosition, its->sourcePosition + its->len, patternTagStrings[its->tag].c_str(),
					its->parentElement);
			else
				::lplog(logType, u"TAGSET %05d: %03d %s[%s] %06d:%s(%d,%d) TAG %s [Element=%d]", ts, its->sourcePosition, patterns[its->parentPattern]->name.c_str(), patterns[its->parentPattern]->differentiator.c_str(),
					its->PEMAOffset, Forms[its->pattern]->shortName.c_str(), its->sourcePosition, its->sourcePosition + its->len, patternTagStrings[its->tag].c_str(),
					its->parentElement);
		}
	else
		for (; its != itsEnd; its++)
		{
			if (its->isPattern)
				::lplog(logType, u"%03d %s[%s] %s[%s](%d,%d) TAG %s [%d,%d]", its->sourcePosition, patterns[its->parentPattern]->name.c_str(), patterns[its->parentPattern]->differentiator.c_str(),
					patterns[its->pattern]->name.c_str(), patterns[its->pattern]->differentiator.c_str(), its->sourcePosition, its->sourcePosition + its->len, patternTagStrings[its->tag].c_str(),
					its->PEMAOffset, its->parentElement);
			else
				::lplog(logType, u"%03d %s[%s] %s(%d,%d) TAG %s [%d,%d]", its->sourcePosition, patterns[its->parentPattern]->name.c_str(), patterns[its->parentPattern]->differentiator.c_str(),
					Forms[its->pattern]->shortName.c_str(), its->sourcePosition, its->sourcePosition + its->len, patternTagStrings[its->tag].c_str(),
					its->PEMAOffset, its->parentElement);
		}
}

// exactly like pema::queryPattern
// Walk the nextByPosition chain at source position `position` and return the
// PEMA index of the longest parent named `pattern` (or -1).  maxEnd is that
// match's relative end.
int cSource::queryPattern(int position, lpwstring pattern, int& maxEnd)
{
	LFS
		int maxLen = -1, pemaPosition = -1, nextByPosition = m[position].beginPEMAPosition;
	for (; nextByPosition != -1; nextByPosition = pema[nextByPosition].nextByPosition)
		if (patterns[pema[nextByPosition].getParentPattern()]->name == pattern && (pema[nextByPosition].end - pema[nextByPosition].begin) > maxLen)
			maxLen = pema[pemaPosition = nextByPosition].end - pema[nextByPosition].begin;
	if (pemaPosition != -1) maxEnd = pema[pemaPosition].end;
	return pemaPosition;
}

// exactly like pema::queryPattern
// First PEMA slot at `position` whose parent name and differentiator both match, or -1.
int cSource::queryPatternDiff(int position, lpwstring pattern, lpwstring differentiator)
{
	LFS
		for (int nextByPosition = m[position].beginPEMAPosition; nextByPosition != -1; nextByPosition = pema[nextByPosition].nextByPosition)
			if (patterns[pema[nextByPosition].getParentPattern()]->name == pattern && patterns[pema[nextByPosition].getParentPattern()]->differentiator == differentiator)
				return nextByPosition;
	return -1;
}

// exactly like pema::queryPattern
// First PEMA slot at `position` whose parent is named `pattern`, or -1.
int cSource::queryPattern(int position, lpwstring pattern)
{
	LFS
		for (int nextByPosition = m[position].beginPEMAPosition; nextByPosition != -1; nextByPosition = pema[nextByPosition].nextByPosition)
			if (patterns[pema[nextByPosition].getParentPattern()]->name == pattern)
				return nextByPosition;
	return -1;
}

// Prefix descriptor with PARENT[diff](absBegin,absEnd) from PEMAPosition, then
// printTagSet.  temp is 1024 wchars with lp_strcpy/lp_wsprintf and no bound check.
void cSource::printTagSet(int logType, const lpchar_t* descriptor, int ts, vector <cTagLocation>& tagSet, int position, int PEMAPosition)
{
	LFS
		lpchar_t temp[1024];
	if (descriptor) lp_strcpy(temp, descriptor);
	if (PEMAPosition >= 0 && position >= 0)
		lp_wsprintf_at(temp, ((descriptor == NULL) ? 0 : lp_strlen(descriptor)), u"%s[%s](%d,%d)",
			patterns[pema[PEMAPosition].getParentPattern()]->name.c_str(), patterns[pema[PEMAPosition].getParentPattern()]->differentiator.c_str(), position + pema[PEMAPosition].begin, position + pema[PEMAPosition].end);
	::printTagSet(logType, temp, ts, tagSet);
}

// Log the concatenated `words` as the TAGSET header, then the locations.
void printTagSet(int logType, const lpchar_t* descriptor, int ts, vector <cTagLocation>& tagSet, vector <lpwstring>& words)
{
	LFS
		lpwstring clause;
	for (unsigned I = 0; I < words.size(); I++)
		clause += words[I] + u" ";
	if (clause.length() && ts >= 0) ::lplog(logType, u"%s TAGSET %05d: %s", descriptor, ts, clause.c_str());
	printTagSet(logType, NULL, ts, tagSet);
}

// Log `words` as the header, then the PEMA-qualified printTagSet.
void cSource::printTagSet(int logType, const lpchar_t* descriptor, int ts, vector <cTagLocation>& tagSet, int position, int PEMAPosition, vector <lpwstring>& words)
{
	LFS
		lpwstring clause;
	for (unsigned I = 0; I < words.size(); I++)
		clause += words[I] + u" ";
	if (clause.length() && ts >= 0) ::lplog(logType, u"%s TAGSET %05d: %s", descriptor, ts, clause.c_str());
	printTagSet(logType, NULL, ts, tagSet, position, PEMAPosition);
}





// Log per-pattern match/push/compare/winner counters and every element's
// usage line.  Read-only: `patterns` / `patternReferences` are process-wide
// globals other code assumes stay alive, so this must not delete them
// (it used to; that left both vectors full of dangling pointers for the rest
// of the process, since main.cpp calls this under #ifdef LOG_PATTERNS well
// before final shutdown, not as the last thing the process does).
void cPattern::printPatternStatistics(void)
{
	LFS
		int totalWinnersMatched = 0;
	for (unsigned int p = 0; p < patterns.size(); p++)
		totalWinnersMatched += patterns[p]->numWinners + patterns[p]->numChildrenWinners;
	if (totalWinnersMatched)
		for (unsigned int p = 0; p < patterns.size(); p++)
		{
			if ((p & 31) == 0)
				::lplog(u"\n%24s[  ]: %8s %7s %10s %11s %10s %7s %7s %%", u"pattern", u"matches", u"ET", u"emi", u"pushes", u"compares", u"hits", u"winners");
			::lplog(u"%c%23s[%2s]: %08u %07u %010u %010u %011u %010u %07u %5.2f%%",
				(patterns[p]->numWinners == 0 && patterns[p]->numChildrenWinners == 0) ? '*' : (patterns[p]->fillFlag || patterns[p]->fillIfAloneFlag) ? '!' : ' ',
				patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(),
				patterns[p]->numMatches, patterns[p]->evaluationTime, patterns[p]->emi, patterns[p]->numPushes,
				patterns[p]->numComparisons, patterns[p]->numHits, patterns[p]->numWinners + patterns[p]->numChildrenWinners, (float)(patterns[p]->numWinners + patterns[p]->numChildrenWinners) * 100 / totalWinnersMatched);
		}
	::lplog(u"%40s %40s  %5s  %5s", u"PATTERN", u"PATTERN ELEMENT", u"#EVER", u"#FINAL");
	for (unsigned int p = 0; p < patterns.size(); p++)
		patterns[p]->reportUsage();
}

