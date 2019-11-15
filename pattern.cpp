#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "word.h"
#include "source.h"
#include "time.h"
#include "profile.h"

vector <cPattern *> patterns; // initialized
int patternReference::firstPatternReference=-1,patternReference::lastPatternReference=-1; // initialized
vector <patternReference *> patternReferences; // initialized
bitObject<32, 5, unsigned int, 32> patternsWithNoParents,patternsWithNoChildren; // initialized
bool overMatchMemoryExceeded=false; // used only in error - one way 
vector < tTS > desiredTagSets; // initialized
vector <wstring> patternTagStrings; // initialized

struct {
  int flag;
  wchar_t *sFlag;
} inflectionFlagList[]=
{ 
  {SINGULAR,L"SINGULAR"},
  {PLURAL,L"PLURAL"},
  {SINGULAR_OWNER,L"SINGULAR_OWNER"},
  {PLURAL_OWNER,L"PLURAL_OWNER"},
  {VERB_PAST,L"VERB_PAST"},
  {VERB_PAST_PARTICIPLE,L"VERB_PAST_PARTICIPLE"},
  {VERB_PRESENT_PARTICIPLE,L"VERB_PRESENT_PARTICIPLE"},
  {VERB_PRESENT_THIRD_SINGULAR,L"VERB_PRESENT_THIRD_SINGULAR"},
  {VERB_PRESENT_FIRST_SINGULAR,L"VERB_PRESENT_FIRST_SINGULAR"},
  {VERB_PAST_THIRD_SINGULAR,L"VERB_PAST_THIRD_SINGULAR"},
  {VERB_PAST_PLURAL,L"VERB_PAST_PLURAL"},
  {VERB_PRESENT_PLURAL,L"VERB_PRESENT_PLURAL"},
  {VERB_PRESENT_SECOND_SINGULAR,L"VERB_PRESENT_SECOND_SINGULAR"},
  {ADJECTIVE_NORMATIVE,L"ADJECTIVE_NORMATIVE"},
  {ADJECTIVE_COMPARATIVE,L"ADJECTIVE_COMPARATIVE"},
  {ADJECTIVE_SUPERLATIVE,L"ADJECTIVE_SUPERLATIVE"},
  {ADVERB_NORMATIVE,L"ADVERB_NORMATIVE"},
  {ADVERB_COMPARATIVE,L"ADVERB_COMPARATIVE"},
  {ADVERB_SUPERLATIVE,L"ADVERB_SUPERLATIVE"},
  {MALE_GENDER,L"MALE_GENDER"},
  {FEMALE_GENDER,L"FEMALE_GENDER"},
  {NEUTER_GENDER,L"NEUTER_GENDER"},
  {FIRST_PERSON,L"FIRST_PERSON"},
  {SECOND_PERSON,L"SECOND_PERSON"},
  {THIRD_PERSON,L"THIRD_PERSON"},
  {NO_OWNER,L"NO_OWNER"},
  {OPEN_INFLECTION,L"OPEN_INFLECTION"},
  {CLOSE_INFLECTION,L"CLOSE_INFLECTION"},
	{FEMALE_GENDER_ONLY_CAPITALIZED,L"FEMALE_GENDER_ONLY_CAPITALIZED"},
	{MALE_GENDER_ONLY_CAPITALIZED,L"MALE_GENDER_ONLY_CAPITALIZED"},
{-1,NULL}
};

unsigned int matchElement::getChildPattern()
{ LFS
  return patternElementMatchArray::PATMASK(elementMatchedIndex);
};

unsigned int matchElement::getChildLen()
{ LFS
  return patternElementMatchArray::ENDMASK(elementMatchedIndex);
};

wstring patternElement::formsStr(void)
{ LFS
  wstring allForms;
  int index;
  for (unsigned int form=0; form<formIndexes.size(); form++)
  {
    if ((index=formIndexes[form])<0)
      allForms+=L"***";
    else
      allForms=allForms+wstring(L" ")+ Forms[index]->name;
  }
  for (unsigned int pattern=0; pattern<patternIndexes.size(); pattern++)
  {
    if ((index=patternIndexes[pattern])<0)
      allForms+=L"***";
    else
      allForms=allForms+wstring(L" ")+ patterns[index]->name;
  }
  return allForms;
}

wstring matchesToString(vector <matchElement> &whatMatched,int elementMatched,wstring &s)
{ LFS
  wchar_t temp[100];
  s.clear();
  while (elementMatched>=0)
  {
    if (whatMatched[elementMatched].elementMatchedIndex&patternFlag)
    {
      if (whatMatched[elementMatched].getChildPattern()>=patterns.size())
        lplog(LOG_FATAL_ERROR,L"Illegal pattern #%d at elementMatched %d!",whatMatched[elementMatched].getChildPattern(),elementMatched);
      wsprintf(temp,L"(%d %s[%s] (childRelEnd=%d, cost=%d)) ",whatMatched[elementMatched].endPosition,
        patterns[whatMatched[elementMatched].getChildPattern()]->name.c_str(),
        patterns[whatMatched[elementMatched].getChildPattern()]->differentiator.c_str(),
        whatMatched[elementMatched].getChildLen(),whatMatched[elementMatched].cost);
    }
    else
      wsprintf(temp,L"(%d %d) ",whatMatched[elementMatched].endPosition,whatMatched[elementMatched].elementMatchedIndex);
    elementMatched=whatMatched[elementMatched].previousMatch;
    s=temp+s;
  }
  return s;
}

bool patternElement::matchRange(Source &source,int matchBegin,int matchEnd,vector <matchElement> &whatMatched)
{ LFS // DLFS
  int rep;
  if (minimum==0)
  {
    for (int matchPosition=matchBegin; matchPosition<matchEnd; matchPosition++)
    {
      whatMatched.push_back(matchElement(-1,whatMatched[matchPosition].endPosition,whatMatched[matchPosition].endPosition,0,0,false,matchPosition,elementPosition,-2,false));
#ifdef LOG_PATTERN_MATCHING
      wstring s;
      ::lplog(L"%d:pattern %s:matchPosition %d:whatMatched #%d:insert as though no match (minimum==0) matchString inserted:%s",
        whatMatched[matchPosition].endPosition,patternName.c_str(),elementPosition,whatMatched.size()-1,matchesToString(whatMatched,whatMatched.size()-1,s).c_str());
#endif
    }
  }
  for (rep=0; rep<maximum && matchBegin!=matchEnd; rep++)
  {
    int saveBegin=whatMatched.size();
#ifdef LOG_PATTERN_MATCHING
    if (rep>0)
      ::lplog(L"   pattern %s:matchPosition %d:starting rep %d at whatMatched index %d.",
      patternName.c_str(),elementPosition,rep,saveBegin);
#endif
		for (int matchPosition = matchBegin; matchPosition < matchEnd; matchPosition++)
		{
			matchOne(source, whatMatched[matchPosition].endPosition, matchPosition, whatMatched);
		}
    matchBegin=saveBegin;
    matchEnd=whatMatched.size();
  }
	if (matchBegin!=matchEnd)
	{
		endPosition=whatMatched[whatMatched.size()-1].endPosition;
#ifdef LOG_PATTERN_MATCHING
		if (patternName.empty())
			::lplog(LOG_WHERE,L"   pattern %s:matchPosition %d:ending at endPosition %d.",
							patternName.c_str(),elementPosition,endPosition);
#endif
	}
  return matchBegin!=matchEnd || rep>1 || minimum==0; // if no matches, only return success if the element is optional
}

bool patternElement::matchFirst(Source &source,int sourcePosition,vector <matchElement> &whatMatched)
{ LFS
  int rep;
  if (minimum==0)
  {
    // if minimum==0 and not matched (this is a placeholder) then the cost should be 0.
    whatMatched.push_back(matchElement(-1,sourcePosition,sourcePosition,0,0,false,-1,elementPosition,-3,false));  // sourcePosition matched, elementMatched, patternFlag, previous match matchPosition
#ifdef LOG_PATTERN_MATCHING
    wstring s;
    lplog(L"%d:pattern %s:matchPosition %d:whatMatched #%d:insert as though no match (minimum==0) matchString inserted:%s",
      sourcePosition,patternName.c_str(),elementPosition,whatMatched.size()-1,matchesToString(whatMatched,whatMatched.size()-1,s).c_str());
#endif
  }
  // match the first matchPosition once
  if (!matchOne(source,sourcePosition,-1,whatMatched))
  {
#ifdef LOG_PATTERN_MATCHING
    lplog(L"%d:pattern %s:matchPosition %d:Match failed",sourcePosition,patternName.c_str(),elementPosition);
#endif
		endPosition=-1;
    return false;
  }
  int matchBegin,matchEnd=whatMatched.size();
  // what Matched size = 1
  //   minimum==0 match of element did not succeed.  skip entirely, as further reps will also fail.
  //   minimum!=0 match of element did succeed. start with matchBegin=0 to check for further reps from the initial rep.
  // what matched size = 2
  //   minimum==0 - both 0 and 1 reps matched.  now start with matchBegin=1 to avoid matching the first element twice
  //   minimum!=0 match of two or more different combinations did succeed. start with matchBegin=0 to check for further reps.
  if (matchEnd==1 && minimum==0) 
	{
		endPosition=-1;
		return true; // succeeded only with no match
	}
  matchBegin=(matchEnd==1 || minimum) ? 0:1;
  if (consolidateEndPositions)
    endPositionsSet.clear();
  for (rep=1; rep<maximum && matchBegin!=matchEnd; rep++)
  {
    int saveBegin=whatMatched.size();
#ifdef LOG_PATTERN_MATCHING
    ::lplog(L"   pattern %s:matchPosition %d:starting rep %d at whatMatched index %d.",
      patternName.c_str(),elementPosition,rep,saveBegin);
#endif
    for (int matchPosition=matchBegin; matchPosition<matchEnd; matchPosition++)
    {
      if (!consolidateEndPositions || endPositionsSet.find(whatMatched[matchPosition].endPosition)==endPositionsSet.end())
      {
        matchOne(source,whatMatched[matchPosition].endPosition,matchPosition,whatMatched);
        if (consolidateEndPositions)
          endPositionsSet.insert(whatMatched[matchPosition].endPosition);
      }
    }
    matchBegin=saveBegin;
    matchEnd=whatMatched.size();
  }
	endPosition=whatMatched[whatMatched.size()-1].endPosition;
#ifdef LOG_PATTERN_MATCHING
	if (patternName.empty())
		::lplog(LOG_WHERE,L"   pattern %s:matchPosition %d:ending at endPosition %d.",
			      patternName.c_str(),elementPosition,endPosition);
#endif
  return true;
}

// flags are the flags to match in the patternElement.
// inflectionFlags are the inflections of the word in the source
// formStr is the form of the word of the source
bool patternElement::inflectionMatch(int inflectionFlagsFromWord,__int64 flags,wstring sourceFormStr)
{ LFS
  if (sourceFormStr==L"noun" && (flags&WordMatch::flagNounOwner))
  {
    if (inflectionFlagsFromWord&SINGULAR)
      inflectionFlagsFromWord=SINGULAR_OWNER|(inflectionFlagsFromWord&~SINGULAR);
    else if (inflectionFlagsFromWord&PLURAL)
      inflectionFlagsFromWord=PLURAL_OWNER|(inflectionFlagsFromWord&~PLURAL);
    else inflectionFlagsFromWord|=SINGULAR_OWNER;
    // if word is owner, but element flags doesn't specify that, fail!  Gives ability to specify __N1 and __NAMEs do NOT match ownership nouns.
    if (inflectionFlags&NO_OWNER) return false;
  }
	if ((inflectionFlags&ONLY_CAPITALIZED) && !(flags&(WordMatch::flagAllCaps|WordMatch::flagFirstLetterCapitalized)))
    return false;
  if ((inflectionFlags&inflectionFlagsFromWord)!=0) return true;
  return  (sourceFormStr==L"verb" && !(inflectionFlags&VERB_INFLECTIONS_MASK)) ||
    (sourceFormStr==L"noun" && !(inflectionFlags&NOUN_INFLECTIONS_MASK)) ||
    (sourceFormStr==L"adjective" && !(inflectionFlags&ADJECTIVE_INFLECTIONS_MASK)) ||
    (sourceFormStr==L"adverb" && !(inflectionFlags&ADVERB_INFLECTIONS_MASK)) ||
    (sourceFormStr==L"quotes" && !(inflectionFlags&INFLECTIONS_MASK)) ||
    (sourceFormStr==L"brackets" && !(inflectionFlags&INFLECTIONS_MASK));
}

#define MAX_PATTERN_NUM_MATCH 4000 // Charles Dickens may require a lower limit.
// because endPositionMatches is being pushed back into whatMatched, endPositionMatches must be a COPY
// of the position of whatMatched it started out as.  Do not pass endPositionMatches as a reference.
bool patternElement::matchOne(Source &source,unsigned int sourcePosition,unsigned int lastElement,vector <matchElement> &whatMatched)
{ LFS
  if (sourcePosition>=source.m.size())
  {
#ifdef LOG_PATTERN_MATCHING
    lplog(L"%d:pattern %s:position %d:No more words available",
      sourcePosition,patternName.c_str(),elementPosition);
#endif
    return false;
  }
  bool oneMatch=false;
  vector <WordMatch>::iterator im=source.m.begin()+sourcePosition;
  for (unsigned int pattern=0; pattern<patternIndexes.size(); pattern++)
  {
    unsigned int p=patternIndexes[pattern];
		if (im->patterns.isSet(p))
    {
      patternMatchArray::tPatternMatch *pm=im->pma.content;
      int startPositions=whatMatched.size(),msize=startPositions;
      if (msize>MAX_PATTERN_NUM_MATCH)
      {
        if (!overMatchMemoryExceeded && traceParseInfo)
          lplog(LOG_ERROR,L"ERROR:%d:pattern %s[%s]:position %d:Match size array reached over %d entries",
          sourcePosition,patternName.c_str(),patterns[patternNum]->differentiator.c_str(),elementPosition,msize);
        im->maxLACMatch=-1;
        overMatchMemoryExceeded=true;
        return false;
      }
      unsigned int count=im->pma.count;
      patterns[patternNum]->numComparisons+=count;
      unsigned int I=0;
      for (; I<count && pm->getPattern()< p; I++,pm++);
      for (; I<count && pm->getPattern()==p; I++,pm++)
      {
        patterns[p]->numHits++;
        bool found=false;
        for (int m=msize-1; m>=startPositions && !found; m--)
          found=(whatMatched[m].endPosition==(pm->len+sourcePosition)); // avoid saving duplicate end positions for the same pattern
        if (!found)
        {
          if (patterns[p]->noRepeat && lastElement!=-1 &&
            patterns[whatMatched[lastElement].getChildPattern()]->rootPattern==patterns[p]->rootPattern &&
            patterns[whatMatched[lastElement].getChildPattern()]->noRepeat)
          {
#ifdef LOG_PATTERN_MATCHING
            lplog(L"%d:pattern %s:position %d:No REPEAT match for pattern %s",sourcePosition,patternName.c_str(),elementPosition,
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
            whatMatched.push_back(matchElement(I,sourcePosition,pm->len+sourcePosition,patternElementMatchArray::subIndex(p,pm->len),
              costs[form]+pm->getCost(),true,lastElement,elementPosition,form,pm->isNew()));
#else
            whatMatched.push_back(matchElement(I,sourcePosition,pm->len+sourcePosition,patternElementMatchArray::subIndex(p,pm->len),
              patternCosts[pattern]+pm->getCost(),true,lastElement,elementPosition,pattern,pm->isNew(source.pass)));
#endif
            msize++;
            oneMatch=true;
          }
        }
#ifdef LOG_PATTERN_MATCHING
        wstring s;
        lplog(L"%d:pattern %s:position %d:whatMatched #%d:End position %d, End element matched index %d subpattern %s matchString %sinserted:%s",
          sourcePosition,patternName.c_str(),elementPosition,
          whatMatched.size()-1,pm->len,I,patterns[p]->name.c_str(),(!found) ? L"" : L"NOT ",matchesToString(whatMatched,whatMatched.size()-1,s).c_str());
#endif
      }
    }
#ifdef LOG_PATTERN_MATCHING
    if (!oneMatch)
      lplog(L"%d:pattern %s:position %d:No match for pattern %s[%s]",sourcePosition,patternName.c_str(),elementPosition,
      patterns[p]->name.c_str(),patterns[p]->differentiator.c_str());
#endif
  }
  for (unsigned int form=0; form<formIndexes.size(); form++)
  {
		unsigned int f=formIndexes[form];
    if (!im->forms.isSet(f))
    {
#ifdef LOG_PATTERN_MATCHING
      lplog(L"%d:pattern %s:position %d:No match for form %s",sourcePosition,patternName.c_str(),elementPosition,
        Forms[f]->name.c_str());
#endif
    }
    else
    {
      if (
				  (!inflectionFlags || 
				   (Forms[f]->inflectionsClass.empty() && !((im->flags&WordMatch::flagNounOwner) && (inflectionFlags&NO_OWNER)) && !((im->flags&(WordMatch::flagAllCaps|WordMatch::flagFirstLetterCapitalized)) && (inflectionFlags&ONLY_CAPITALIZED))) ||
           inflectionMatch(im->word->second.inflectionFlags,im->flags,Forms[f]->inflectionsClass)) &&
				  (specificWords[form].empty() || im->word->first==specificWords[form] || (im->word->second.mainEntry!=wNULL && im->word->second.mainEntry->first==specificWords[form])))
      {
        int ME=im->queryForm(f);
        if (ME<0) continue;
				// if this is a pattern match phase which is after the main phase,
				// such as META_NAME_EQUIVALENCE or other, then only match against the forms which won
				// in the main phase (unless the form = the word itself).
				if (patterns[patternNum]->ignoreFlag && !im->isWinner(ME) && im->word->first!=Forms[f]->name)
					continue;
        int cost=formCosts[form];
        if (!preTaggedSource && im->costable())
          cost+=im->word->second.usageCosts[ME];
        //if (f==PROPER_NOUN_FORM_NUM && (im->flags&(WordMatch::flagAllCaps|WordMatch::flagAddProperNoun))==(WordMatch::flagAllCaps|WordMatch::flagAddProperNoun)) // make this proper noun a little expensive
        //  cost++;
        // refuse any other form for this very common word if the next word is not punctuation.
        if (im->word->first==L"i" && f!=nomForm &&
          sourcePosition+1<source.m.size() && (signed)source.m[sourcePosition+1].word->first[0]>0 && iswalpha(source.m[sourcePosition+1].word->first[0]))
          cost+=10;
        if (im->word->first==L"a" && f!=determinerForm &&
          sourcePosition+1<source.m.size() && (signed)source.m[sourcePosition+1].word->first[0]>0 &&
          (iswalpha(source.m[sourcePosition+1].word->first[0]) || source.m[sourcePosition+1].queryForm(quoteForm)>=0) && source.m[sourcePosition+1].word->first!=L"is")
          cost+=10;
        if ((im->word->first==L"if" || im->word->first==L"and" || im->word->first==L"but") && (f==nounForm || f==adjectiveForm || f==PROPER_NOUN_FORM_NUM || f==abbreviationForm) &&
          !(im->word->second.inflectionFlags&PLURAL))
          cost+=10;
        whatMatched.push_back(matchElement(-1,sourcePosition,sourcePosition+1,ME,cost,false,lastElement,elementPosition,form,false));
        oneMatch=true;
#ifdef LOG_PATTERN_MATCHING
        wstring s;
				lplog(L"%d:pattern %s:position %d:whatMatched #%d:form %s::%s found. matchString inserted:%s",
          sourcePosition,patternName.c_str(),elementPosition,whatMatched.size()-1,
          Forms[f]->name.c_str(),specificWords[form].c_str(),matchesToString(whatMatched,whatMatched.size()-1,s).c_str());
#endif
      }
#ifdef LOG_PATTERN_MATCHING
      else
      { wstring inflectionName,inflectionName2;
				lplog(L"%d:pattern %s:position %d:Form %s:%s found inflective%s but did not match inflective%s (or form).",sourcePosition,patternName.c_str(),elementPosition,
							Forms[f]->name.c_str(),specificWords[form].c_str(),getInflectionName(im->word->second.inflectionFlags,f,inflectionName),
							getInflectionName(inflectionFlags,f,inflectionName2));
      }
#endif
    }
  }
	// if matched, immediately return true
	if (oneMatch)
		return true;
	// if not matched, but match is optional (minimum==0) and word should not be ignored (or pattern is set to disable ignore forms), return true (go on to next element)
	// words to ignore can only be forms, not patterns
	// no inflected forms allowed
	// if [first non-optional element in pattern , and - removed] pattern has not matched any elements previously,
	//   reject, because the pattern will have a chance to match with the next position
	// (and we want to avoid double matching)
	if ((!im->word->second.isIgnore() || patterns[patternNum]->checkIgnorableForms))
		return minimum==0;
	// if there is nothing to match against, or current source position is the matched end position, return false (fail match)
  if (!whatMatched.size() || sourcePosition==whatMatched[0].endPosition) return false;
#ifdef LOG_PATTERN_MATCHING
  lplog(L"%d:pattern %s:position %d:%s is ignored%s.",
    sourcePosition,patternName.c_str(),elementPosition,im->word->first.c_str(),(minimum==0) ? L" optional pattern":L"");
#endif
  //whatMatched[lastElement].sentencePosition++; // eliminated because multiple matches could use this and some might ignore and others not
  // see beginPosition in matchElement
  return matchOne(source,sourcePosition+1,lastElement,whatMatched);
}

// this assumes that optional or multiple elements cannot have a next element that could match
bool cPattern::matchPatternPosition(Source &source, const unsigned int sourcePosition, bool fill, sTrace &t)
{
	LFS  // DLFS
  source.whatMatched.clear();
  source.whatMatched.reserve(10000);
  overMatchMemoryExceeded=false;
  vector <patternElement *>::iterator e=elements.begin(),eEnd=elements.end();
  if (!(*e)->matchFirst(source,sourcePosition,source.whatMatched)) return false;
  int begin=0;
  for (e++; e!=eEnd; e++)
  {
		int saveEnd=source.whatMatched.size();
    if (!(*e)->matchRange(source,begin,saveEnd,source.whatMatched))
    {
#ifdef LOG_PATTERN_MATCHING
      ::lplog(L"%d:pattern %s:position %d:Match failed",sourcePosition,name.c_str(),e-elements.begin());
#endif
      return false;
    }
    if (source.whatMatched.size()!=saveEnd) begin=saveEnd;
  }
#ifdef LOG_PATTERN_MATCHING
  ::lplog(L"%d:pattern %s:Match succeeded",sourcePosition,name.c_str());
#endif
	if (!fill) 
	{
		unsigned int whereMatch=sourcePosition;
	  for (e=elements.begin(),eEnd=elements.end(); e!=eEnd; e++)
		{
			if ((*e)->variable.length())
			{
				variableToLocationMap[(*e)->variable]=whereMatch;
				variableToLengthMap[(*e)->variable]=(*e)->endPosition-whereMatch;
				locationToVariableMap[whereMatch]=(*e)->variable;
				#ifdef LOG_PATTERN_MATCHING
					::lplog(LOG_WHERE,L"%d:matchPattern %d mapped variable %s=(begin=%d,end=%d)",sourcePosition,num,(*e)->variable.c_str(),whereMatch,(*e)->endPosition);
				#endif
			}
			whereMatch=(*e)->endPosition;
		}
		return source.whatMatched.size()!=0;
	}
	vector <unsigned int> insertionPoints;
	vector <int> diffCosts;
  bool additionalMatch=false; // set if a match was found that was not found previously
  // now transfer the matches to the source
  for (unsigned int I=begin; I<source.whatMatched.size(); I++)
  {
#ifdef LOG_PATTERN_MATCHING
    ::lplog(L"%d:pattern %s:  ___whatMatched #%d___",sourcePosition,name.c_str(),I);
#endif
    unsigned int insertionPoint=-1;
    int reducedCost=MAX_SIGNED_SHORT;
    bool pushed=false;
    additionalMatch|=fillPattern(source,sourcePosition,source.whatMatched,I,insertionPoint,reducedCost,pushed,t);
    if (pushed && insertionPoint!=source.m[sourcePosition].pma.count-1)
    {
      for (unsigned int J=0; J<insertionPoints.size(); J++)
        if (insertionPoints[J]>=insertionPoint)
          insertionPoints[J]++;
    }
    if (reducedCost!=MAX_SIGNED_SHORT)
    {
      insertionPoints.push_back(insertionPoint);
      diffCosts.push_back(reducedCost);
    }
  }
  numMatches++;
	if (insertionPoints.size())
	{
#ifdef QLOGPATTERN
		if (pass>0) lplog(L"Pattern %s lowered costs against sentence position %d",p->name.c_str(),pos);
#endif
		source.reduceParents(sourcePosition,insertionPoints,diffCosts);
	}
  return additionalMatch;
}

bool cPattern::fillPattern(Source &source,int sourcePosition,vector <matchElement> &whatMatched,int elementMatched,unsigned int &insertionPoint,int &reducedCost,bool &pushed,sTrace &t)
{ LFS // DLFS
  int endPosition=whatMatched[elementMatched].endPosition;
  if (source.lastSourcePositionSet<endPosition) source.lastSourcePositionSet=endPosition;
	if ((strictNoMiddleMatch || onlyEndMatch) && ignoreFlag && endPosition<(signed)source.m.size() && !wcschr(L"!?.",source.m[endPosition].word->first[0]))
		return false;
	if ((strictNoMiddleMatch || onlyEndMatch) && endPosition<(signed)source.m.size())
	{
		if (iswalpha(source.m[endPosition].word->first[0]))	return false;
		if (endPosition && (source.m[endPosition-1].flags&WordMatch::flagFirstLetterCapitalized))
		{
			bool endSentence=true;
			// also if next is a period, but before that is an honorific
			// As though that first scrutiny had been satisfactory , Mrs . Vandemeyer motioned to a chair . (CLOSING_S1 will match Mrs)
			wchar_t *abbreviationForms[]={ L"letter",L"abbreviation",L"measurement_abbreviation",L"street_address_abbreviation",L"business_abbreviation",
				L"time_abbreviation",L"date_abbreviation",L"honorific_abbreviation",L"trademark",L"pagenum",NULL };
			for (unsigned int af=0; abbreviationForms[af] && endSentence; af++)
				if (source.m[endPosition-1].queryForm(abbreviationForms[af])>=0)
					endSentence=false;
			if (endPosition && source.m[endPosition].word->first[0]==L'.' && !endSentence && endPosition+1<(signed)source.m.size() &&
					// prevent Mr... / Bellavue Dr. The 
					iswalpha(source.m[endPosition+1].word->first[0]) && source.m[endPosition+1].queryForm(PROPER_NOUN_FORM_NUM)>=0)
		return false;
		}
	}
  bool additionalMatch=false,isNew=false;
  int elementCost=0,numElementsMatched=0;
  //#ifdef LOG_OLD_MATCH
  /*
  On second pass, if any subpattern matched is above the current pattern in number OR new for this pass, mark NEW
  if none of the elementsMatched are new, return false, without filling anything, or checking anything.
  this "new_for_pass" flag will be in PMA. (LOG_OLD_MATCH only)
  */
  if (source.pass>1)
  {
    for (int tmpElementMatched=elementMatched; tmpElementMatched>=0;  tmpElementMatched=whatMatched[tmpElementMatched].previousMatch)
      isNew|=whatMatched[tmpElementMatched].isNew(); // num LOG_OLD_MATCH
    if (!isNew)
    {
#ifdef LOG_PATTERN_MATCHING
      ::lplog(L"%d:pattern %s[%s](%d,%d) NOT NEW MATCH",sourcePosition,name.c_str(),differentiator.c_str(),sourcePosition,endPosition);
#endif
      return pushed=false;
    }
  }
  else
    isNew=true;
  //  #endif
	if ((strictNoMiddleMatch || onlyEndMatch) && endPosition<(signed)source.m.size() && iswalpha(source.m[endPosition].word->first[0]))
		elementCost+=100;
	if (questionFlag && endPosition<=(signed)source.m.size())
	{
		// question could be embedded in a coordinated phrase:
		// But if so, where was the girl, and what had she done with the papers?
		// search for a sentence ending:
		unsigned int ep=endPosition;
		for (; ep<source.m.size() && !source.isEOS(ep); ep++);
		// -2 is to compensate for verbs taking objects like: are you not?  
		// 'are' should take an object, which would give a -1 cost to a _COMMAND.  So to conteract this, -2 is given here.
		if (ep<source.m.size())
			elementCost+=(source.m[ep].word->first==L"?") ? -2 : 2; 
	}
	int tmpCost;
  if (onlyBeginMatch && sourcePosition && (tmpCost=source.m[sourcePosition-1].word->second.lowestSeparatorCost())>0)
    elementCost+=tmpCost/1000; // lowestSeparatorCost multiplies by 1000
  for (int tmpElementMatched=elementMatched; tmpElementMatched>=0;  tmpElementMatched=whatMatched[tmpElementMatched].previousMatch)
  {
    int subMatchBegin=whatMatched[tmpElementMatched].beginPosition;
    int subMatchEnd=whatMatched[tmpElementMatched].endPosition;
    if (subMatchBegin<subMatchEnd)
    {
      source.reverseMatchElements.assign(numElementsMatched++,tmpElementMatched);
      elementCost+=whatMatched[tmpElementMatched].cost; // add up cost of each element in pattern, which is also all costs of subPatterns
    }
  }
  bool reduced;
  insertionPoint=source.m[sourcePosition].pma.push_back_unique(source.pass,elementCost,num,endPosition-sourcePosition,reduced,pushed);
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
    ::lplog(L"%d:pma position %d %s[%s](%d,%d) costing error - on non-new reduced cost from %d (original) to %d!",sourcePosition,insertionPoint,name.c_str(),differentiator.c_str(),
      sourcePosition,source.m[sourcePosition].pma[insertionPoint].len+sourcePosition,source.m[sourcePosition].pma[insertionPoint].cost,elementCost);
  }
  if (pushed)
  {
    source.m[sourcePosition].patterns.set(num);
    vector <matchElement>::iterator iMatch=whatMatched.begin(),iMatchEnd=whatMatched.end();
    for (; iMatch!=iMatchEnd; iMatch++)
      if (iMatch->beginPosition==sourcePosition && iMatch->PMAIndex>=(int)insertionPoint)
        iMatch->PMAIndex++;
  }
  if (reduced)
    reducedCost=elementCost;
  else
    reducedCost=MAX_SIGNED_SHORT;
#if defined(LOG_PATTERN_MATCHING) || defined(LOG_PATTERN_COST_CHECK)
  wstring s;
  ::lplog(L"%d:pattern %s[%s](%d,%d) total cost=%d at PMAOffset %d (pushed=%s,reduced=%s) for matches %s",
    sourcePosition,name.c_str(),differentiator.c_str(),sourcePosition,endPosition,cost,
    insertionPoint,(pushed) ? L"true" : L"false",(reduced) ? L"true" : L"false",
    matchesToString(whatMatched,elementMatched,s).c_str());
#endif
#ifdef LOG_PATTERN_COST_CHECK
  if (cost)
    ::lplog(L"%d:pma position %d %s[%s](%d,%d) set cost %d.",sourcePosition,insertionPoint,name.c_str(),differentiator.c_str(),
    sourcePosition,source.m[sourcePosition].pma[insertionPoint].len+sourcePosition,cost);
#endif
  int *formerPosition=&source.m[sourcePosition].pma[insertionPoint].pemaByPatternEnd,*firstPosition=formerPosition;
  // if maxMatch updated and the pattern is not fill (Tag("_FINAL")), then when winners are calculated,
  // if a pattern which is not tagged "_FINAL" has matched more elements than another which is tagged "_FINAL", then
  // every match will be removed and the words will end up being unmatched.
  if (isTopLevelMatch(source,sourcePosition,endPosition))
  {
    vector <WordMatch>::iterator im=source.m.begin()+sourcePosition,imEnd=source.m.begin()+endPosition;
    for (; im!=imEnd; im++) im->flags|=WordMatch::flagTopLevelPattern;
    im=source.m.begin()+sourcePosition;
    int len=endPosition-sourcePosition,avgCost=elementCost*1000/len; // COSTCALC
    if (t.tracePatternElimination)
    {
      for (unsigned int relpos=0; im!=imEnd; im++,relpos++)
        if (im->updateMaxMatch(len,avgCost))
          ::lplog(L"%d:Pattern %s[%s](%d,%d) established a new maxLACMatch %d or lowest average cost %d=%d*1000/%d",
          sourcePosition+relpos,name.c_str(),differentiator.c_str(),sourcePosition,endPosition,len,avgCost,elementCost,len); // COSTCALC
    }
    else
      for (; im!=imEnd; im++)
        im->updateMaxMatch(len,avgCost);
  }
  bool POFlag=false;
  for (int I=numElementsMatched-1; I>=0; I--)
  {
    vector <matchElement>::iterator thisMatch=whatMatched.begin()+source.reverseMatchElements[I];
    int subMatchBegin=thisMatch->beginPosition;
    bool newElement;
    unsigned int PEMAOffset=source.pema.push_back_unique(formerPosition,subMatchBegin,elementCost,thisMatch->cost,
      num,sourcePosition-subMatchBegin,endPosition-subMatchBegin,thisMatch->elementMatchedIndex,thisMatch->patternElement,
      thisMatch->patternElementIndex,source.m.size()-sourcePosition,newElement,POFlag);
    if (newElement)
    {
      elements[thisMatch->patternElement]->usageEverMatched[thisMatch->patternElementIndex]++;
      source.pema[PEMAOffset].origin=*firstPosition;
      if (source.pema[*firstPosition].begin!=0)
        break;
      vector <WordMatch>::iterator mb=source.m.begin()+subMatchBegin;
      if (mb->beginPEMAPosition<0) mb->beginPEMAPosition=PEMAOffset;
      if (mb->endPEMAPosition>=0)
        source.pema[mb->endPEMAPosition].nextByPosition=PEMAOffset;
      mb->endPEMAPosition=PEMAOffset;
      mb->PEMACount++;
      if (thisMatch->PMAIndex>=0)
      {
        source.pema[PEMAOffset].nextByChildPatternEnd=mb->pma[thisMatch->PMAIndex].pemaByChildPatternEnd;
        mb->pma[thisMatch->PMAIndex].pemaByChildPatternEnd=PEMAOffset;
        // check inconsistent PMAOffsets
        /*
        patternMatchArray::tPatternMatch *pm=mb->pma.content;
        if (thisMatch->PMAIndex>=(int)mb->pma.count)
        ::lplog(LOG_FATAL_ERROR,L"    %d:RP PMA pattern offset %d is >= %d - ILLEGAL",subMatchBegin,thisMatch->PMAIndex,mb->pma.count);
        unsigned int childPattern=pm[thisMatch->PMAIndex].getPattern();
        int childEnd=pm[thisMatch->PMAIndex].end;
        patternElementMatchArray::tPatternElementMatch *pem=source.pema.begin()+pm[thisMatch->PMAIndex].pemaByChildPatternEnd;
        if (patterns[childPattern]->name!=patterns[pem->getChildPattern()]->name ||
        childEnd!=pem->getChildLen())
        ::lplog(L"INCONSISTENT PMAOffset %d pemaByChildPatternEnd=%d %s[%s](%d,%d) leads to %s[%s](%d,%d) CHILD %s[%s](%d,%d)!",
        thisMatch->PMAIndex,pm[thisMatch->PMAIndex].pemaByChildPatternEnd,
        patterns[childPattern]->name.c_str(),patterns[childPattern]->differentiator.c_str(),subMatchBegin,subMatchBegin+childEnd,
        patterns[pem->getPattern()]->name.c_str(),patterns[pem->getPattern()]->differentiator.c_str(),subMatchBegin,subMatchBegin+pem->end,
        patterns[pem->getChildPattern()]->name.c_str(),patterns[pem->getChildPattern()]->differentiator.c_str(),subMatchBegin,subMatchBegin+pem->getChildLen());
        */
      }
#ifdef LOG_PATTERN_CHAINS
      source.logPatternChain(sourcePosition,insertionPoint,patternElementMatchArray::BY_PATTERN_END);
      source.logPatternChain(subMatchBegin,mb->beginPEMAPosition,patternElementMatchArray::BY_POSITION);
      source.logPatternChain(subMatchBegin,PEMAOffset,patternElementMatchArray::BY_CHILD_PATTERN_END);
#endif
      numPushes++;
      additionalMatch=true;
    }
#ifdef LOG_PATTERN_MATCHING
    if (thisMatch->elementMatchedIndex&(patternFlag))
      ::lplog(L"%d:pattern %s[%s](%d,%d) %s[%s](%d,%d) cost=%d%s",
      subMatchBegin,name.c_str(),differentiator.c_str(),sourcePosition,endPosition,
      patterns[thisMatch->getChildPattern()]->name.c_str(),patterns[thisMatch->getChildPattern()]->differentiator.c_str(),
      subMatchBegin,thisMatch->getChildLen()+subMatchBegin,thisMatch->cost,(newElement) ? L"" : L" -- DUPLICATE");
    else
      ::lplog(L"%d:pattern %s[%s](%d,%d) %s cost=%d%s",subMatchBegin,name.c_str(),differentiator.c_str(),sourcePosition,endPosition,
      source.m[subMatchBegin].word->second.Form(thisMatch->elementMatchedIndex)->name.c_str(),thisMatch->cost,(newElement) ? L"" : L" -- DUPLICATE");
#endif
    POFlag=true;
    formerPosition=&source.pema[PEMAOffset].nextPatternElement;
  }
  return additionalMatch;
}

unsigned int findPattern(wstring form)
{ LFS
  unsigned int startingPattern=0;
  return findPattern(form,startingPattern);
}

void findPatterns(wstring form,bitObject<> &patternsFound)
{ LFS
  unsigned int startingPattern=0;
  int p;
  while ((p=findPattern(form,startingPattern))>=0)
    patternsFound.set(p);
}

unsigned int findPattern(wstring form,unsigned int &startingPattern)
{ LFS
  for (; startingPattern<patterns.size() && (patterns[startingPattern]->name!=form); startingPattern++);
  return startingPattern;
}

unsigned int findPattern(wstring name,wstring diff)
{ LFS
  unsigned int p;
  for (p=0; p<patterns.size() && (patterns[p]->name!=name || patterns[p]->differentiator!=diff); p++);
  return (p==patterns.size()) ? -1 : p;
}

// _VERB|wrapping[*]*2{VERB:pM:V_OBJECT} is a verb with future reference and a cost of 2.
void cPattern::processForm(wstring &form,wstring &specificWord,int &cost,set <unsigned int> &tags,bool &explicitFutureReference,bool &blockDescendants,bool &allowRecursiveMatch)
{ LFS
  const wchar_t *ch;
	for (ch=form.c_str(); *ch && *ch!=L'|' && *ch!=L'[' && *ch!=L'*' && *ch!=L'{'; ch++);
  cost=0;
	blockDescendants= allowRecursiveMatch = explicitFutureReference=false;
  unsigned int eraseFromThisPoint=(unsigned int)(ch-form.c_str());
	if (*ch==L'|')
  {
		const wchar_t *sword=ch+1;
	  for (; *ch && *ch!=L'[' && *ch!=L'*' && *ch!=L'{'; ch++);
		wchar_t saveEnd=*ch;
		*((wchar_t *)ch)=0;
		specificWord=sword;
		*((wchar_t *)ch)=saveEnd;
  }
  if (*ch==L'[')
  {
    explicitFutureReference=true;
		allowRecursiveMatch = (ch[1] == L'R');
    ch+=3;
  }
  if (*ch==L'*')
  {
    ch++;
    wstring keep;
    if (*ch==L'-')
    {
      keep+=*ch;
      ch++;
    }
    while (iswdigit(*ch))
    {
      keep+=*ch;
      ch++;
    }
    cost=_wtoi(keep.c_str());
  }
  if (*ch=='{')
  {
    while (*ch==L'{' || *ch==L':')
    {
      ch++;
      wstring tag;
      while (*ch!=L':' && *ch!=L'}')
      {
        tag+=*ch;
        ch++;
      }
      if (!tag.length()) continue;
      unsigned int I;
      for (I=0; I<patternTagStrings.size() && tag!=patternTagStrings[I]; I++);
      if (I==patternTagStrings.size()) patternTagStrings.push_back(tag);
      if (tag==L"_BLOCK")
        blockDescendants=true;
      else
        tags.insert(I);
    }
    ch++;
  }
  // "_REL1{_BLOCK}*1"
  if (*ch)
    ::lplog(LOG_FATAL_ERROR,L"Incorrect form syntax in form %s.",form.c_str());
  form.erase(eraseFromThisPoint,form.length());
}

bool cPattern::eliminateTag(wstring tag)
{ LFS
  for (set <unsigned int>::iterator t=tags.begin(),tEnd=tags.end(); t!=tEnd; t++)
    if (tag==patternTagStrings[*t])
    {
      tags.erase(t);
      return true;
    }
    return false;
}

#ifdef ABNF
int patternElement::writeABNFElementTag(wchar_t *buf,wstring sForm,int num,int cost,vector <unsigned int> &tags,int maxBuf,bool printNum)
{ LFS
  int len=0;
  if (printNum)
    len+=_snwprintf(buf,maxBuf,L"%s#%s",sForm.c_str(),patterns[num]->differentiator.c_str());
  else
    len+=_snwprintf(buf,maxBuf,L"%s",sForm.c_str());
  if (cost) len+=_snwprintf(buf+len,maxBuf-len,L"$%d",cost);
  if (tags.size()) buf[len++]=L'[';
  for (unsigned int I=0; I<tags.size(); I++)
    len+=_snwprintf(buf+len,maxBuf-len,L"%s:",patternTagStrings[tags[I]].c_str());
  if (buf[len-1]==L':') buf[len-1]=L']';
  buf[len++]=L'/';
  buf[len]=0;
  return len;
}

// does not feed into patternReferences!
// ABNF patterns include all pattern #s (NOUN#2, NOUN#3), whereas the hardcoded patterns include only the main form (NOUN)
// This is because ABNF patterns include costing data for each NOUN# individually (original purpose)
//
void patternElement::readABNFElementTag(wstring patternName,wstring differentiator,int elementNum,set <unsigned int> &descendantTags,wchar_t *buf)
{ LFS
  wchar_t *c_diff=wcschr(buf,L'#'),*c_cost=wcschr(buf,L'$'),*c_tags=wcschr(buf,L'[');
  if (c_diff) *c_diff=0;
  if (c_cost) *c_cost=0;
  if (c_tags) *c_tags=0;
  wstring form=buf;
  int diffNum=(c_diff) ? wtoi(c_diff+1) : -1;
  int cost=(c_cost) ? wtoi(c_cost+1) : 0,f=-1;
  bool blockDescendants=false;
  vector <unsigned int> elementTags;
  if (c_tags)
  {
    wchar_t *tag=c_tags+1;
    while (tag)
    {
      wchar_t *next_tag=wcschr(tag,L':');
      if (!next_tag) next_tag=wcschr(tag,L']');
      if (!next_tag) break;
      *next_tag=0;
      if (!wcscmp(tag,L"_BLOCK"))
        blockDescendants=true;
      else
        elementTags.push_back(findTag(tag));
    }
  }
  if (form[0]==L'_')
    patternReferences.push_back(new patternReference(form,patterns.size(),diffNum,elementNum,cost,elementTags,blockDescendants,false));
  else
  {
    if ((f=FormsClass::findForm(form))<0)
      ::lplog(LOG_FATAL_ERROR|LOG_ERROR,"FATAL_ERROR:Pattern %s[%s] uses an undefined form %s.",patternName.c_str(),differentiator.c_str(),form.c_str());
    formStr.push_back(form);
    costs.push_back(cost);
    indexes.push_back(f);
    usageFinalMatch.push_back(0);
    usageEverMatched.push_back(0);
    tags.push_back(elementTags);
    stopDescendingSearch.push_back(blockDescendants);
    isPattern.push_back(false);
  }
  for (unsigned int I=0; I<elementTags.size(); I++)
    descendantTags.insert(elementTags[I]);
}

void patternElement::writeABNF(wchar_t *buf,int &len,int maxBuf)
{ LFS
  if (minimum!=1 || maximum!=1) len+=_snwprintf(buf+len,maxBuf-len,L"%d*%d",minimum,maximum);
  wstring sFlags;
  allFlags(sFlags);
  if (sFlags.length()) len+=_snwprintf(buf+len,maxBuf-len,L"{%s}",sFlags.c_str());
  buf[len++]=L'(';
  for (unsigned int I=0; I<formStr.size(); I++)
  {
    bool printNum=(formStr.size()>I+1 && formStr[I]==formStr[I+1]) || (I!=0 && formStr[I]==formStr[I-1]);
    len+=writeABNFElementTag(buf+len,formStr[I],indexes[I],costs[I],tags[I],printNum);
  }
  buf[len-1]=L')';
}

// readABNF
patternElement::patternElement(wstring patternName,wstring differentiator,int elementNum,set <unsigned int> &descendantTags,wchar_t *&buf)
{ LFS
  wchar_t *c_tags=wcschr(buf,L'(');
  if (!c_tags) return;
  *c_tags=0;
  wchar_t *c_flags=wcschr(buf,L'{');
  if (c_flags) *c_flags=0;
  wchar_t *ch=wcschr(buf,L'*');
  elementPosition=elementNum;
  if (ch)
  {
    *ch=0;
    minimum=wtoi(buf);
    maximum=wtoi(ch+1);
  }
  else
    minimum=maximum=1;
  inflectionFlags=0;
  if (c_flags)
    for (unsigned int I=0; inflectionFlagList[I].sFlag; I++)
      if (strstr(c_flags+1,inflectionFlagList[I].sFlag))
        inflectionFlags|=inflectionFlagList[I].flag;
  wchar_t *tag=c_tags+1;
  while (true)
  {
    wchar_t *next_tag;
    if (!(next_tag=wcschr(tag,L'/')) && !(next_tag=wcschr(tag,L')'))) break;
    *next_tag=0;
    readABNFElementTag(patternName,differentiator,elementNum,descendantTags,tag);
    tag=next_tag+1;
  }
}

/*
rulename(# number) ($ number)  = (number) (*) (number) name ($ number) ([ TAG:TAG ... ]) (/) ...
__NAME#3  = 0*1(honorific[SINGULAR]/__NAMEINTRO)
letter{FIRST}
*"."
*/
void cPattern::writeABNF(FILE *fh,unsigned int lastTag)
{ LFS
  wchar_t buf[1024];
  int len;
  len=_snwprintf(buf,1024,L"%s",name.c_str());
  if (differentiator.length()) len+=_snwprintf(buf+len,1024-len,L"#%s",differentiator.c_str());
  if (cost) len+=_snwprintf(buf+len,1024-len,L"$%d",cost);
  buf[len++]=L'[';
  for (unsigned int I=0; I<tags.size(); I++)
    if (tags[I]<lastTag)
      len+=_snwprintf(buf+len,1024-len,L"%s:",patternTagStrings[tags[I]].c_str());
  if (blockDescendants) len+=_snwprintf(buf+len,1024-len,L"_BLOCK:");
  if (fillIfAloneFlag) len+=_snwprintf(buf+len,1024-len,L"_FINAL_IF_ALONE:");
  if (fillFlag) len+=_snwprintf(buf+len,1024-len,L"_FINAL:");
  if (onlyAloneExceptInSubPatternsFlag) len+=_snwprintf(buf+len,1024-len,L"_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:");
  if (onlyBeginMatch) len+=_snwprintf(buf+len,1024-len,L"_ONLY_BEGIN_MATCH:");
  if (onlyAfterQuote) len+=_snwprintf(buf+len,1024-len,L"_AFTER_QUOTE:");
	if (strictNoMiddleMatch) len+=_snwprintf(buf+len,1024-len,L"_STRICT_NO_MIDDLE_MATCH:");
	if (onlyEndMatch) len+=_snwprintf(buf+len,1024-len,L"__ONLY_END_MATCH:");
  if (firstPassForwardReference) len+=_snwprintf(buf+len,1024-len,L"_FORWARD_REFERENCE:");
  if (noRepeat) len+=_snwprintf(buf+len,1024-len,L"_NO_REPEAT:");
  if (ignoreFlag) len+=_snwprintf(buf+len,1024-len,L"_IGNORE:");
  if (questionFlag) len+=_snwprintf(buf+len,1024-len,L"_QUESTION:");
	if (notAfterPronoun) len += _snwprintf(buf + len, 1024 - len, L"_NOT_AFTER_PRONOUN:");
	if (explicitSubjectVerbAgreement) len += _snwprintf(buf + len, 1024 - len, L"_EXPLICIT_SUBJECT_VERB_AGREEMENT:");
	if (buf[len-1]==L':') buf[len-1]=L']';
  if (buf[len-1]==L'[') len--;
  buf[len++]=L'=';
  buf[len]=0;
  fwprintf(fh,L"%s\n",buf);
  len=0;
  for (unsigned int I=0; I<elements.size(); I++)
  {
    elements[I]->writeABNF(buf,len);
    if (len>SCREEN_WIDTH)
    {
      fwprintf(fh,L"%s\n",buf);
      len=0;
    }
  }
  if (len) fwprintf(fh,L"%s\n",buf);
  fwprintf(fh,L"\n");
}

void writePatternsInABNF(wstring filePath,unsigned int lastTag)
{ LFS
  FILE *fp=fwopen(filePath.c_str(),L"w+");
  if (fp==NULL)
    return;
  for (unsigned int I=0; I<patterns.size(); I++)
    patterns[I]->writeABNF(fp,lastTag);
  fclose(fp);
}

void readPatternsInABNF(wstring filePath)
{ LFS
  FILE *fp=fwopen(filePath.c_str(),L"r+");
  if (fp==NULL)
    return;
  bool valid=true;
  while (valid)
  {
    cPattern *pattern=new cPattern(fp,valid);
    if (valid) patterns.push_back(pattern);
  }
  fclose(fp);
}

cPattern::cPattern(FILE *fh,bool &valid)
{ LFS
  cPattern();
  wchar_t buf[1024],*ch;
  valid=false;
  while (true)
  {
    if (fgets(buf,1024,fh)==NULL) return;
    if (ch=wcschr(buf,L'=')) break;
  }
  valid=true;
  *ch=0;
  wchar_t *c_differentiator=wcschr(buf,L'#');
  wchar_t *c_cost=wcschr(buf,L'$');
  wchar_t *c_tags=wcschr(buf,L'[');
  if (c_differentiator) *c_differentiator=0;
  if (c_cost) *c_cost=0;
  if (c_tags) *c_tags=0;
  name=buf;
  if (c_differentiator) differentiator=c_differentiator+1;
  if (c_cost) cost=wtoi(c_cost+1);
  if (c_tags)
  {
    wchar_t *tag=c_tags+1;
    while (tag)
    {
      wchar_t *next_tag=wcschr(tag,L':');
      if (!next_tag) next_tag=wcschr(tag,L']');
      if (!next_tag) break;
      *next_tag=0;
      if (!wcscmp(tag,L"_BLOCK")) blockDescendants=true;
      else if (!wcscmp(tag,L"_FINAL_IF_ALONE")) fillIfAloneFlag=true;
      else if (!wcscmp(tag,L"_FINAL")) fillFlag=true;
      else if (!wcscmp(tag,L"_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN")) onlyAloneExceptInSubPatternsFlag=true;
      else if (!wcscmp(tag,L"_ONLY_BEGIN_MATCH")) onlyBeginMatch=true;
      else if (!wcscmp(tag,L"_AFTER_QUOTE")) afterQuote=true;
			else if (!wcscmp(tag,L"_STRICT_NO_MIDDLE_MATCH")) strictNoMiddleMatch=true;
			else if (!wcscmp(tag,L"_ONLY_END_MATCH")) onlyEndMatch=true;
      else if (!wcscmp(tag,L"_FORWARD_REFERENCE")) firstPassForwardReference=true;
      else if (!wcscmp(tag,L"_NO_REPEAT")) noRepeat=true;
      else if (!wcscmp(tag,L"_IGNORE")) ignoreFlag=true;
      else if (!wcscmp(tag,L"_QUESTION")) questionFlag=true;
      else if (!wcscmp(tag,L"_NOT_AFTER_PRONOUN")) notAfterPronoun=true;
			else if (!wcscmp(tag, L"_EXPLICIT_SUBJECT_VERB_AGREEMENT")) explicitSubjectVerbAgreement = true;
      else
        tags.push_back(findTag(tag));
    }
  }
  int elementNum=0;
  while (true)
  {
    fgets(buf,1024,fh);
    if (wcslen(buf)<2) break;
    wchar_t *c_element=buf;
    while (true)
    {
      if (wcschr(c_element,L')'))
        elements.push_back(new patternElement(name,differentiator,elementNum++,descendantTags,c_element));
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
bool cPattern::create(wstring patternName, wstring differentiator, int numForms, ...)
{
	LFS
  bool OK=true,explicitFutureReference; // nonOptionalElementFound=false,
  cPattern *p=new cPattern();
  va_list patternMarker;
	wstring specificPatternWord; //specificPatternWord is not valid for pattern names
  processForm(patternName,specificPatternWord,p->cost,p->tags,explicitFutureReference,p->blockDescendants,p->allowRecursiveMatch);
  p->fillIfAloneFlag=p->eliminateTag(L"_FINAL_IF_ALONE");
  p->fillFlag=p->eliminateTag(L"_FINAL");
  p->onlyAloneExceptInSubPatternsFlag=p->eliminateTag(L"_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN");
  p->onlyBeginMatch=p->eliminateTag(L"_ONLY_BEGIN_MATCH");
  p->afterQuote=p->eliminateTag(L"_AFTER_QUOTE");
	p->strictNoMiddleMatch=p->eliminateTag(L"_STRICT_NO_MIDDLE_MATCH");
	p->onlyEndMatch=p->eliminateTag(L"_ONLY_END_MATCH");
  p->firstPassForwardReference=p->eliminateTag(L"_FORWARD_REFERENCE");
  p->noRepeat=p->eliminateTag(L"_NO_REPEAT");
  p->ignoreFlag=p->eliminateTag(L"_IGNORE");
  p->questionFlag=p->eliminateTag(L"_QUESTION");
	p->notAfterPronoun = p->eliminateTag(L"_NOT_AFTER_PRONOUN");
	p->explicitSubjectVerbAgreement = p->eliminateTag(L"_EXPLICIT_SUBJECT_VERB_AGREEMENT");
	p->checkIgnorableForms= p->eliminateTag(L"_CHECK_IGNORABLE_FORMS");
	// free form is dangerous to use.  New forms are added to the word only if
	// the form has never existed.  If somehow a word is part of a pattern which
	// is meant to be a form (the form = the word), but the form already exists,
	// the form is not added to the word (there is no clue that this should be done,
	// and it should not be done in most cases, only in a case where the word==form name).
	// If the form is not added to the word and the form was never added to the word,
	// (or it was but then was never updated to the database for some reason), 
	// then this pattern will never be used because there are no words associated
	// with the form.
	// Moreover, this feature, thogh convenient, leads to a proliferation of forms.
	bool freeForm=p->eliminateTag(L"_FREE_FORM");
  p->name=patternName;
	p->differentiator=differentiator;
  p->num=patterns.size();
  p->rootPattern=0;
  //size_t pr=patternReferences.size();
  va_start( patternMarker,numForms );     /* Initialize variable arguments. */
  wstring form;
  int elementPosition=0;
  while (numForms)
  {
    patternElement *element=new patternElement;
    element->patternName=patternName;
		vector <int> predefinedForms;
    while (numForms>0)
    {
      form=va_arg( patternMarker, wchar_t *);
      set <unsigned int> elementTags;
      bool blockDescendants, allowRecursiveMatch;
      int elementCost,f=-1;
			wstring specificWord;
      processForm(form,specificWord,elementCost,elementTags,explicitFutureReference,blockDescendants,allowRecursiveMatch);
      for (set <unsigned int>::iterator et=elementTags.begin(),etEnd=elementTags.end(); et!=etEnd; et++)
        p->allElementTags.set(*et);
      bool isPattern;
      if (isPattern=form[0]==L'_') //specificWord is not valid for _PATTERNS
        patternReferences.push_back(new patternReference(form,p->num,-1,p->elements.size(),elementCost,elementTags,blockDescendants, allowRecursiveMatch, !explicitFutureReference));
      else
      {
        if ((f=FormsClass::findForm(form))<0)
				{
					if (!freeForm)
						::lplog(LOG_FATAL_ERROR|LOG_ERROR,L"FATAL_ERROR:Pattern %s[%s] uses an undefined form %s (1).",p->name.c_str(),p->differentiator.c_str(),form.c_str());
				  Words.predefineWord(form.c_str(),tFI::queryOnAnyAppearance);
					f=FormsClass::findForm(form);
					predefinedForms.push_back(f);
				}
        element->formStr.push_back(form);
        element->specificWords.push_back(specificWord);
        element->formIndexes.push_back(f);
        element->formCosts.push_back(elementCost);
        element->usageFinalMatch.push_back(0);
        element->usageEverMatched.push_back(0);
        element->formTags.push_back(elementTags);
        element->formStopDescendingSearch.push_back(blockDescendants);
      }
      p->descendantTags.insert(elementTags.begin(),elementTags.end());
      numForms--;
    }
    if (element->patternIndexes.size()>255)
      ::lplog(LOG_FATAL_ERROR,L"Pattern %s[%s] has >255 indexes (%d) - see patternElementNum member of patternElementMatchArray class",p->name.c_str(),p->differentiator.c_str(),element->patternIndexes.size());
    element->inflectionFlags=va_arg( patternMarker, int);
		if (predefinedForms.size())
		{
			for (unsigned int pdf=0; pdf<predefinedForms.size(); pdf++)
			{
				if (element->inflectionFlags&VERB_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass=L"verb";
				if (element->inflectionFlags&NOUN_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass=L"noun";
				if (element->inflectionFlags&ADJECTIVE_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass=L"adjective";
				if (element->inflectionFlags&ADVERB_INFLECTIONS_MASK)
					Forms[predefinedForms[pdf]]->inflectionsClass=L"adverb";
			}
		}
    element->minimum=va_arg( patternMarker, int);
    element->maximum=va_arg( patternMarker, int);
    element->elementPosition=p->elements.size();
    p->elements.push_back(element);
    elementPosition++;
    numForms=va_arg( patternMarker, int);
  }
  if (p->elements.size()>255)
    ::lplog(LOG_FATAL_ERROR,L"Pattern %s[%s] has too many elements >255 (%d) - see patternElementNum member of patternElementMatchArray class",p->name.c_str(),p->differentiator.c_str(),p->elements.size());
  if (((int)findPattern(patternName,p->rootPattern))<0)
    p->rootPattern=p->num;
  if (p->rootPattern!=p->num)
  {
    p->nextRoot=p->rootPattern;
    int tmpNext;
    while ((tmpNext=patterns[p->nextRoot]->nextRoot)>=0)
      p->nextRoot=tmpNext;
    patterns[p->nextRoot]->nextRoot=p->num;
    p->nextRoot=-1;
  }
  for (unsigned int e=0; e<p->elements.size(); e++)
    p->elements[e]->patternNum=p->num;
  if (patternReference::firstPatternReference==-1)
    patternReference::firstPatternReference=p->num;
  va_end( patternMarker );              /* Reset variable arguments.      */
  if (patterns.size()>65535)
    ::lplog(LOG_FATAL_ERROR|LOG_ERROR,L"FATAL ERROR:# Patterns exceeds allowable 65536 patterns storable in patternElement array on pattern %s[%s].",
				    patternName.c_str(),differentiator.c_str());
  patterns.push_back(p);
  return OK;
}

cPattern *cPattern::create(Source *source,wstring patternName,int num,int whereBegin,int whereEnd, unordered_map <wstring, wstring> &parseVariables,bool destinationPatternType)
{ LFS
  bool explicitFutureReference; // nonOptionalElementFound=false,OK=true,
  cPattern *p=new cPattern();
	p->cost=0;
  p->questionFlag=true;
  p->name=patternName;
  p->num=num;
  p->rootPattern=0;
  int elementPosition=0;
	for (int I=whereBegin; I<whereEnd; I++)
	{
    patternElement *element=new patternElement;
    element->patternName=patternName;
		unordered_map < int,wstring>::iterator substitutePattern=source->temporaryPatternBuffer.find(I);
		wstring forms,tmpstr;
		if (substitutePattern!=source->temporaryPatternBuffer.end())
		{
			// [variable name]=[substitute word for parsing]:[pattern list] || [variable name]
			forms=substitutePattern->second;
			size_t equalsPos=forms.find(L'='),colonPos=forms.find(L':');
			if (equalsPos!=wstring::npos && colonPos!=wstring::npos)
			{
				element->variable=forms.substr(0,equalsPos);
				parseVariables[element->variable]=forms=forms.substr(colonPos+1);
#ifdef LOG_PATTERN_MATCHING
				::lplog(LOG_WHERE,L"%d:pattern create mapped variable %s=(%s)",whereBegin,element->variable.c_str(),parseVariables[element->variable].c_str());
#endif
			}
			else 
			{
				forms=parseVariables[element->variable=forms];
#ifdef LOG_PATTERN_MATCHING
				::lplog(LOG_WHERE,L"%d:pattern used mapped variable %s=(%s)",whereBegin,element->variable.c_str(),parseVariables[element->variable].c_str());
#endif
			}
			if (destinationPatternType)
			{
				p->locationToVariableMap[I]=element->variable;
				p->destinationPatternType=destinationPatternType;
#ifdef LOG_PATTERN_MATCHING
				::lplog(LOG_WHERE,L"pattern %d mapped location %d to variable %s",p->num,I,element->variable.c_str());
#endif
			}
		}
		else
			forms=source->m[I].patternWinnerFormString(tmpstr); // verb|called
		for (int pos=-1,nextpos=0; pos<(signed)forms.size() && nextpos>=0; pos=nextpos)
		{
			nextpos=forms.find(L',',pos+1);
			wstring form;
			if (nextpos!=wstring::npos) 
				form=forms.substr(pos+1,nextpos-pos-1);
			else
				form=forms.substr(pos+1);
			set <unsigned int> elementTags;
			bool blockDescendants,allowRecursiveMatch;
			int elementCost,f=-1;
			wstring specificWord;
			processForm(form,specificWord,elementCost,elementTags,explicitFutureReference,blockDescendants,allowRecursiveMatch);
			for (set <unsigned int>::iterator et=elementTags.begin(),etEnd=elementTags.end(); et!=etEnd; et++)
				p->allElementTags.set(*et);
			bool isPattern;
			if (isPattern=form[0]==L'_')
				//specificWord is not valid for _PATTERNS 
				patternReferences.push_back(new patternReference(form,p->num,-1,p->elements.size(),elementCost,elementTags,blockDescendants,allowRecursiveMatch,!explicitFutureReference));
			else
			{
				if ((f=FormsClass::findForm(form))<0)
					::lplog(LOG_FATAL_ERROR|LOG_ERROR,L"FATAL_ERROR:Pattern %s[%s] uses an undefined form %s (2).",p->name.c_str(),p->differentiator.c_str(),form.c_str());
				element->formStr.push_back(form);
				element->specificWords.push_back(specificWord);
				element->formIndexes.push_back(f);
				element->formCosts.push_back(elementCost);
				element->usageFinalMatch.push_back(0);
				element->usageEverMatched.push_back(0);
				element->formTags.push_back(elementTags);
				element->formStopDescendingSearch.push_back(blockDescendants);
			}
			p->descendantTags.insert(elementTags.begin(),elementTags.end());
		}
    if (element->patternIndexes.size()>255)
      ::lplog(LOG_FATAL_ERROR,L"Pattern %s[%s] has >255 indexes (%d) - see patternElementNum member of patternElementMatchArray class",p->name.c_str(),p->differentiator.c_str(),element->patternIndexes.size());
    element->inflectionFlags=0;
    element->minimum=1;
    element->maximum=1;
    element->elementPosition=p->elements.size();
    p->elements.push_back(element);
    elementPosition++;
  }
  for (unsigned int e=0; e<p->elements.size(); e++)
    p->elements[e]->patternNum=p->num;
  return p;
}

const wchar_t *patternElement::allFlags(wstring &sFlags)
{ LFS
  sFlags.clear();
  for (int I=0; inflectionFlagList[I].sFlag; I++)
    if (inflectionFlagList[I].flag&inflectionFlags)
      sFlags+=inflectionFlagList[I].sFlag+wstring(L" ");
  return sFlags.c_str();
}

const wchar_t *allFlags(int inflectionFlags,wstring &sFlags)
{ LFS
  sFlags.clear();
  for (int I=0; inflectionFlagList[I].sFlag; I++)
    if (inflectionFlagList[I].flag&inflectionFlags)
      sFlags+=inflectionFlagList[I].sFlag+wstring(L" ");
  return sFlags.c_str();
}

struct {
	int flag;
	wchar_t *sFlag;
} wordFlagList[] =
{
	{tFI::topLevelSeparator,L"topLevelSeparator"},
	{tFI::ignoreFlag,L"ignoreFlag"},
	{tFI::queryOnLowerCase,L"queryOnLowerCase"},
	{tFI::queryOnAnyAppearance,L"queryOnAnyAppearance"},
	{tFI::updateMainInfo,L"updateMainInfo"},
	{tFI::updateMainEntry,L"updateMainEntry"},
	{tFI::insertNewForms,L"insertNewForms"},
	{tFI::isMainEntry,L"isMainEntry"},
	{tFI::intersectionGroup,L"intersectionGroup"},
	{tFI::wordIndexRead,L"wordIndexRead"},
	{tFI::wordRelationsRefreshed,L"wordRelationsRefreshed"},
	{tFI::newWordFlag,L"newWordFlag"},
	{tFI::inSourceFlag,L"inSourceFlag"},
	{tFI::alreadyTaken,L"alreadyTaken"},
	{tFI::physicalObjectByWN,L"physicalObjectByWN"},
	{tFI::notPhysicalObjectByWN,L"notPhysicalObjectByWN"},
	{tFI::uncertainPhysicalObjectByWN,L"uncertainPhysicalObjectByWN"},
	{tFI::genericGenderIgnoreMatch,L"genericGenderIgnoreMatch"},
	{tFI::prepMoveType,L"prepMoveType"},
	{tFI::genericAgeGender,L"genericAgeGender"},
	{tFI::stateVerb,L"stateVerb"},
	{tFI::possibleStateVerb,L"possibleStateVerb"},
	{tFI::mainEntryErrorNoted,L"mainEntryErrorNoted"},
	{-1,NULL}
};

const wchar_t *allWordFlags(int wordflags, wstring &sFlags)
{
	LFS
	sFlags.clear();
	for (int I = 0; wordFlagList[I].sFlag; I++)
		if (wordFlagList[I].flag&wordflags)
			sFlags += wordFlagList[I].sFlag + wstring(L" ");
	return sFlags.c_str();
}

void cPattern::lplog(void)
{
	LFS
		wchar_t temp[1024];
	wsprintf(temp, L"\n%d:%s[%s] root=%d", num, name.c_str(), differentiator.c_str(), rootPattern);
	if (isFutureReference) wcscat(temp, L" FUTURE_REFERENCE");
	if (containsFutureReference) wcscat(temp, L" CONTAINS_FUTURE_REFERENCE");
	if (indirectFutureReference) wcscat(temp, L" INDIRECT_FUTURE_REFERENCE");

	if (fillFlag) wcscat(temp, L" {_FINAL}");
	if (fillIfAloneFlag) wcscat(temp, L" {_FINAL_IF_ALONE}");
	if (onlyBeginMatch) wcscat(temp, L" {_ONLY_BEGIN_MATCH}");
	if (afterQuote) wcscat(temp, L" {_AFTER_QUOTE}");
	if (strictNoMiddleMatch) wcscat(temp, L" {_STRICT_NO_MIDDLE_MATCH}");
	if (onlyEndMatch) wcscat(temp, L" {_ONLY_END_MATCH}");
	if (onlyAloneExceptInSubPatternsFlag) wcscat(temp, L" {_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}");
	if (blockDescendants) wcscat(temp, L" {_BLOCK}");
	if (noRepeat) wcscat(temp, L" {_NO_REPEAT}");
	if (ignoreFlag) wcscat(temp, L" {_IGNORE}");
	if (questionFlag) wcscat(temp, L" {_QUESTION}");
	if (notAfterPronoun) wcscat(temp, L" {_NOT_AFTER_PRONOUN}");
	if (explicitSubjectVerbAgreement) wcscat(temp, L" {_EXPLICIT_SUBJECT_VERB_AGREEMENT}");
	for (set<unsigned int>::iterator dt = tags.begin(), dtEnd = tags.end(); dt != dtEnd; dt++)
		_snwprintf(temp + wcslen(temp), 1024 - wcslen(temp), L"{%s}", patternTagStrings[*dt].c_str());
	::lplog(L"%s", temp);
	wstring temp2;
	for (unsigned int p = 0; p < patterns.size(); p++)
		if (ancestorPatterns.isSet(p))
			temp2 = temp2 + wstring(L" ") + patterns[p]->name + wstring(L"[") + patterns[p]->differentiator + wstring(L"]");
	if (temp[0]) ::lplog(L"ancestors:%s", temp2.c_str());
	temp[0] = 0;
	for (set<unsigned int>::iterator dt = descendantTags.begin(), dtEnd = descendantTags.end(); dt != dtEnd; dt++)
		_snwprintf(temp + wcslen(temp), 1024 - wcslen(temp), L"%s ", patternTagStrings[*dt].c_str());
	if (temp[0]) ::lplog(L"DESCENDANT TAGS:%s", temp);
	temp[0] = 0;
	for (unsigned int I = 0; I < sizeof(includesOneOfTagSet) * 8; I++)
		if (includesOneOfTagSet&((__int64)1 << I))
			_snwprintf(temp + wcslen(temp), 1024 - wcslen(temp), L"%s ", desiredTagSets[I].name.c_str());
	if (temp[0]) ::lplog(L"One Of Tag Sets: %s", temp);
	temp[0] = 0;
	for (unsigned int I = 0; I < sizeof(includesOnlyDescendantsAllOfTagSet) * 8; I++)
		if (includesOnlyDescendantsAllOfTagSet&((__int64)1 << I))
			_snwprintf(temp + wcslen(temp), 1024 - wcslen(temp), L"%s ", desiredTagSets[I].name.c_str());
	if (temp[0]) ::lplog(L"All Of Tag Sets (only descendants): %s", temp);
	temp[0] = 0;
	for (unsigned int I = 0; I < sizeof(includesDescendantsAndSelfAllOfTagSet) * 8; I++)
		if (includesDescendantsAndSelfAllOfTagSet&((__int64)1 << I))
			_snwprintf(temp + wcslen(temp), 1024 - wcslen(temp), L"%s ", desiredTagSets[I].name.c_str());
	if (temp[0]) ::lplog(L"All Of Tag Sets (descendants and self): %s", temp);
	for (unordered_map < wstring, int >::iterator vlpi = variableToLocationMap.begin(), vlpiEnd = variableToLocationMap.end(); vlpi != vlpiEnd; vlpi++)
		::lplog(L"variable %s mapped to location %d.", vlpi->first.c_str(), vlpi->second);
	for (unordered_map < wstring, int >::iterator vlpi = variableToLengthMap.begin(), vlpiEnd = variableToLengthMap.end(); vlpi != vlpiEnd; vlpi++)
		::lplog(L"variable %s mapped to length %d.", vlpi->first.c_str(), vlpi->second);
	for (unordered_map < int, wstring >::iterator lvmi = locationToVariableMap.begin(), lvmiEnd = locationToVariableMap.end(); lvmi != lvmiEnd; lvmi++)
		::lplog(L"location %d mapped to variable %s.", lvmi->first, lvmi->second.c_str());
	for (unsigned int I = 0; I < elements.size(); I++)
	{
		wstring formsString;
		for (unsigned int J = 0; J < elements[I]->formCosts.size(); J++)
			formsString = formsString + elements[I]->toText(temp, J, false, 1024) + L" ";
		for (unsigned int J = 0; J < elements[I]->patternCosts.size(); J++)
			formsString = formsString + elements[I]->toText(temp, J, true, 1024) + L" ";
		wstring sFlags;
		::lplog(L"%d:%d-%d %s %s", I, elements[I]->minimum, elements[I]->maximum, elements[I]->allFlags(sFlags), formsString.c_str());
	}
}

void cPattern::reportUsage(void)
{
	LFS
	wchar_t temp[1024];
	wstring fullname = name + L"["+differentiator+L"]";
	wsprintf(temp, L"%40s ", fullname.c_str());
	for (unsigned int I = 0; I < elements.size(); I++)
	{
		for (unsigned int J = 0; J < elements[I]->formCosts.size(); J++)
			elements[I]->reportUsage(temp, J, false);
		for (unsigned int J = 0; J < elements[I]->patternCosts.size(); J++)
			elements[I]->reportUsage(temp, J, true);
	}
}

bool cPattern::add(int elementNum,wstring patternName,bool logFutureReferences,int elementCost,set <unsigned int> elementTags,
                   bool elementBlockDescendants,bool elementAllowRecursiveMatch)
{ LFS
  bool found=false,error=false;
  unsigned int p=0;
  //int mandatoryChild=-1;
  while (findPattern(patternName,p)<patterns.size())
  {
    if (p!=num || elementAllowRecursiveMatch)
    {
      if (patternReference::lastPatternReference<(int)p)
        patternReference::lastPatternReference=p;
      elements[elementNum]->patternStr.push_back(patternName);
      childPatterns.set(p);
      patterns[p]->parentPatterns.set(num);
      elements[elementNum]->patternIndexes.push_back(p);
      elements[elementNum]->usageFinalMatch.push_back(0);
      elements[elementNum]->usageEverMatched.push_back(0);
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
        descendantTags.insert(patterns[p]->tags.begin(),patterns[p]->tags.end());
      elements[elementNum]->patternStopDescendingSearch.push_back(elementBlockDescendants);
      if (patterns[p]->containsFutureReference || patterns[p]->indirectFutureReference)
        indirectFutureReference=true;
      if (p>num)
      {
        if (!found) logFutureReferences=false; // all the patterns referred to are not defined yet, so there is no confusion.
        // this lplog message is only to help with patterns accidentally referring to patterns defined later.
        patterns[p]->isFutureReference=true;
        containsFutureReference=true;
        if (logFutureReferences)
        {
          ::lplog(L"Pattern %s[%s] element %s matches also against future pattern %s[%s].",
            name.c_str(),differentiator.c_str(),patternName.c_str(),patterns[p]->name.c_str(),patterns[p]->differentiator.c_str());
          error=true;
        }
      }
      found=true;
    }
#ifdef LOG_PATTERN_MATCHING
		else
			::lplog(L"Pattern %s[%s] element %s blocked self match against pattern %s[%s].",
				name.c_str(), differentiator.c_str(), patternName.c_str(), patterns[p]->name.c_str(), patterns[p]->differentiator.c_str());
#endif
		p++;
  }
  if (!found)
    ::lplog(L"Resolve failure: pattern %s not found while defining pattern %s[%s].",patternName.c_str(),name.c_str(),differentiator.c_str());
  return (!found)|error;
}

void cPattern::setAncestorPatterns(int childPattern)
{ LFS
  for (unsigned int p=0; p<patterns.size(); p++)
    if (patterns[childPattern]->parentPatterns.isSet(p) && !ancestorPatterns.isSet(p))
    {
      ancestorPatterns.set(p);
      if (patterns[p]->ancestorsSet)
        ancestorPatterns|=patterns[p]->ancestorPatterns;
      else
        setAncestorPatterns(p);
    }
}

void cPattern::setMandatoryAncestorPatterns(int childPattern)
{ LFS
  for (unsigned int p=0; p<patterns.size(); p++)
    if (patterns[childPattern]->mandatoryParentPatterns.isSet(p) && !mandatoryAncestorPatterns.isSet(p))
    {
      mandatoryAncestorPatterns.set(p);
      if (patterns[p]->mandatoryAncestorsSet)
        ancestorPatterns|=patterns[p]->mandatoryAncestorPatterns;
      else
        setMandatoryAncestorPatterns(p);
    }
}

// if the pattern will be eliminated on these grounds DO NOT set maxMatch
// otherwise, patterns not matching up to these # of elements will be eliminated even though they are
//   the winners. - used in eliminateLoserPatterns
bool cPattern::isTopLevelMatch(Source &source,unsigned int beginPosition,unsigned int endPosition)
{ LFS
  return fillFlag ||
    ((fillIfAloneFlag || onlyAloneExceptInSubPatternsFlag) &&
    ((!beginPosition || source.m[beginPosition-1].word->second.isSeparator()) &&
    (source.m.size()<=endPosition || source.m[endPosition].word->second.isSeparator())));
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
match over all patterns incorporating these patterns up to the last patternReference (LAST)
repeat
continue matching patterns after LAST pattern
*/

void patternReference::resolve(bool &patternError)
{ LFS
  patternError|=patterns[patternNum]->add(elementNum,form,logFutureReferences,cost,tags,blockDescendants,allowRecursiveMatch);
}

void patternReference::resolve(vector <cPattern *> &transformPatterns,bool &patternError)
{ LFS
  patternError|=transformPatterns[patternNum]->add(elementNum,form,logFutureReferences,cost,tags,blockDescendants, allowRecursiveMatch);
}

bool cPattern::hasDescendantTag(unsigned int tag)
{ LFS
  return descendantTags.find(tag)!=descendantTags.end();
  /*
  for (set <unsigned int>::iterator dt=descendantTags.begin(),dtEnd=descendantTags.end(); dt!=dtEnd; dt++)
  if (tag==*dt)
  return true;
  return false;
  */
}

bool cPattern::hasTag(unsigned int tag)
{ LFS
  return tags.find(tag)!=tags.end();
}

// a pattern may only contain some elements.  Only if it has no elements of the tag set will it be marked false.
bool cPattern::containsOneOfTagSet(tTS &desiredTagSet,unsigned int limit,bool includeSelf)
{ LFS
  if (includeSelf)
  {
    for (unsigned tag=0; tag<limit; tag++)
      if (hasDescendantTag(desiredTagSet.tags[tag]) || hasTag(desiredTagSet.tags[tag])) return true;
    return false;
  }
  for (unsigned tag=0; tag<limit; tag++)
    if (hasDescendantTag(desiredTagSet.tags[tag]))
      return true;
  return false;
}

bool cPattern::setCheckDescendantsForTagSet(tTS &desiredTagSet,bool includeSelf)
{ LFS
  if (desiredTagSet.required<0)
    return containsOneOfTagSet(desiredTagSet,(unsigned)(-desiredTagSet.required),includeSelf);
  if (includeSelf)
  {
    for (unsigned tag=0; tag<(unsigned)desiredTagSet.required; tag++)
      if (!hasDescendantTag(desiredTagSet.tags[tag]) && !hasTag(desiredTagSet.tags[tag])) return false;
    return true;
  }
  for (unsigned tag=0; tag<(unsigned)desiredTagSet.required; tag++)
    if (!hasDescendantTag(desiredTagSet.tags[tag])) return false;
  return true;
}

unsigned int findTagSet(wchar_t *tagSet)
{ LFS
  for (unsigned int I=0; I<desiredTagSets.size(); I++)
    if (desiredTagSets[I].name==tagSet)
      return I;
  lplog(LOG_FATAL_ERROR,L"Tagset %s not found!",tagSet);
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

unsigned int PREP_TAG,OBJECT_TAG,SUBOBJECT_TAG,REOBJECT_TAG,IOBJECT_TAG,SUBJECT_TAG,PREP_OBJECT_TAG,VERB_TAG,PLURAL_TAG,MPLURAL_TAG,GNOUN_TAG,MNOUN_TAG,PNOUN_TAG,VNOUN_TAG,HAIL_TAG,NAME_TAG,REL_TAG,SENTENCE_IN_REL_TAG,FLOAT_TIME_TAG;

void tTS::addTagSet(int tagSet)
{ LFS
  for (unsigned int I=0; I<desiredTagSets[tagSet].tags.size(); I++)
    tags.push_back(desiredTagSets[tagSet].tags[I]);
}

// the first number argument is the number of arguments ALL must be in the pattern before the pattern field includesAllOfTagSet bit
// for the tagset.  If the first number is negative it indicates the number of AT LEAST ONE of the arguments that must be in the pattern.
void initializeTagSets(int &startSuperTagSets)
{ LFS
	desiredTagSets.push_back(tTS(verbSenseTagSet,L"_VERB_SENSE",-28,
    L"no",L"never",L"not",L"past",L"imp",L"future",L"id",L"conditional",L"vS",L"vAC",L"vC",L"vD",L"vB",L"vAB",L"vBC",L"vABC",L"vCD",L"vBD",L"vABD",L"vACD",L"vBCD",L"vABCD",L"vE",L"vrBD",L"vrB",L"vrBC",L"vrD",L"vAD",NULL));
  desiredTagSets.push_back(tTS(subjectVerbAgreementTagSet,L"_AGREEMENT",3,L"SUBJECT",L"VERB",L"V_AGREE",L"V_OBJECT",L"conditional",L"future",L"past",NULL)); // V_OBJECT used for relations
  // "N_AGREE",L"GNOUN",L"SINGULAR",L"PLURAL" were put into SUBJECT_TAGSET because these tags also belong in OBJECTS which are after the verb, have nothing to
  // do with agreement and yet greatly multiply the number of tagsets.
	desiredTagSets.push_back(tTS(subjectTagSet,L"_SUBJECT",-4,L"N_AGREE",L"GNOUN",L"MNOUN",L"NAME",L"SINGULAR",L"PLURAL",L"RE_OBJECT",NULL));
	desiredTagSets.push_back(tTS(specificAnaphorTagSet,L"_SPECIFIC_ANAPHOR",-3,L"N_AGREE",L"GNOUN",L"MNOUN",L"PLURAL",L"SUBJECT",L"V_AGREE",NULL));
  // VERB_OBJECTS_TAGSET: although a verb does not need to have objects to be assessed for the # of objects it has, the pattern must allow for objects.
  desiredTagSets.push_back(tTS(verbObjectsTagSet,L"_VERB_OBJECTS",3,L"VERB",L"V_OBJECT",L"OBJECT",L"HOBJECT",L"V_AGREE",
		                       L"vD",L"vrD",L"vAD",L"vBD",L"vCD",L"vABD",L"vACD",L"vBCD",L"vABCD",L"IVERB",NULL));
  desiredTagSets.push_back(tTS(iverbTagSet,L"_IVERB",1,L"ITO",L"V_OBJECT",L"OBJECT",L"IVERB",L"PREP",L"REL",L"ADJ",L"ADV",L"HOBJECT",L"V_AGREE",L"V_HOBJECT",L"VERB2",L"ADJOBJECT",L"ADVOBJECT",L"MNOUN",L"MVERB",L"S_IN_REL",NULL)); // ITO is simply a infinitive marker
  desiredTagSets.push_back(tTS(nounDeterminerTagSet,L"_NOUN_DETERMINER",2,L"NOUN",L"N_AGREE",L"DET",L"SUBOBJECT",NULL));
  // VNOUN: noun which is an activity (running, or Bill running down the street)
  // PNOUN: only personal nouns such as he, she, herself, them, etc.
  // GNOUN: address, date, time, telephone number, number,
	// MNOUN: a noun represented syntactically by a single entity but which is made up of multiple nouns (blue, gray and purple)
	// ADJOBJECT: the latter, the former
  desiredTagSets.push_back(tTS(objectTagSet,L"_OBJECTS",-6,L"NOUN",L"VNOUN",L"PNOUN",L"GNOUN",L"NAME",L"ADJOBJECT",L"NAMEOWNER",NULL));
  // RE_OBJECT is an object restated, IOBJECT is an immediate object of an infinitive phrase, PREPOBJECT is an object of a prepositional phrase, and SUBOBJECT is everything else.
  desiredTagSets.push_back(tTS(roleTagSet,L"_ROLE",-6,L"SUBJECT",L"OBJECT",L"RE_OBJECT",L"SUBOBJECT",L"IOBJECT",L"PREPOBJECT",L"HAIL",NULL));
  desiredTagSets.push_back(tTS(subjectVerbRelationTagSet,L"_SUBJECT_VERB_RELATION",3,L"VERB",L"V_OBJECT",L"SUBJECT",L"OBJECT",L"PREP",L"IVERB",L"REL",L"ADJ",L"ADV",L"HOBJECT",L"V_AGREE",L"V_HOBJECT",L"VERB2",L"ADJOBJECT",L"ADVOBJECT",L"MNOUN",L"MVERB",L"S_IN_REL",L"QTYPE",NULL));
	desiredTagSets.push_back(tTS(verbObjectRelationTagSet,L"_VERB_OBJECT_RELATION",3,L"VERB",L"V_OBJECT",L"OBJECT",L"SUBJECT",L"PREP",L"IVERB",L"REL",L"ADJ",L"ADV",L"HOBJECT",L"V_AGREE",L"V_HOBJECT",L"VERB2",L"ADJOBJECT",L"ADVOBJECT",L"MNOUN",L"MVERB",L"S_IN_REL",L"QTYPE",NULL));
  desiredTagSets.push_back(tTS(nameTagSet,L"_NAME",-7,L"FIRST",L"MIDDLE",L"LAST",L"QLAST",L"ANY",L"HON",L"BUS",L"HON2",L"HON3",L"SUFFIX",L"SINGULAR",L"PLURAL",NULL));
  desiredTagSets.push_back(tTS(metaNameEquivalenceTagSet,L"_META_NAME_EQUIVALENCE",-3,L"NAME_PRIMARY",L"NAME_SECONDARY",L"NAME_ABOUT",NULL));
  desiredTagSets.push_back(tTS(metaSpeakerTagSet,L"_META_SPEAKER",1,L"NAME_PRIMARY",NULL));
  desiredTagSets.push_back(tTS(prepTagSet,L"_PREP",1,L"P",L"PREPOBJECT",L"PREP",L"REL",L"S_IN_REL",NULL));
  desiredTagSets.push_back(tTS(BNCPreferencesTagSet,L"_BNC_PREFERENCES_TAGSET",-4,L"NOUN",L"VERB",L"ADJ",L"ADV",NULL));
  desiredTagSets.push_back(tTS(nAgreeTagSet,L"_N_AGREE_TAGSET",1,L"N_AGREE",NULL));
  desiredTagSets.push_back(tTS(EVALTagSet,L"_EVAL_TAGSET",-3,L"EVAL",L"REL",L"IVERB",NULL));
  desiredTagSets.push_back(tTS(idRelationTagSet,L"_ID_RELATION_TAGSET",3,L"id",L"OBJECT",L"SUBJECT",NULL));
  desiredTagSets.push_back(tTS(notbutTagSet,L"_NOTBUT_TAGSET",1,L"not",L"but",NULL));
  desiredTagSets.push_back(tTS(mobjectTagSet,L"_MOBJECT_TAGSET",1,L"MOBJECT",NULL));
  desiredTagSets.push_back(tTS(qtobjectTagSet,L"_QUESTION",1,L"QTYPE",NULL));
	// the following is used because noun determiners need to scan in a sentence and find all prepositional phrases as well
	//    as objects, but _ROLE tagset cannot be used because PREP is not a object type, and PREPOBJECT is blocked 
  desiredTagSets.push_back(tTS(ndPrepTagSet,L"_NDP",1,L"PREP",NULL));
  desiredTagSets.push_back(tTS(timeTagSet,L"_TIME",-10,L"TIMESPEC",L"HOUR",L"TIMEMODIFIER",L"TIMECAPACITY",L"TIMETYPE",L"DAYMONTH",L"MONTH",L"YEAR",L"DATESPEC",L"DAYWEEK",L"SEASON",L"HOLIDAY",L"MINUTE",NULL));

  startSuperTagSets=desiredTagSets.size();
  // these tagsets indicate the pattern has the descendant tagset which is blocked.
  desiredTagSets.push_back(tTS(descendantAgreementTagSet,L"_DESCENDANTS_HAVE_AGREEMENT",1,L"_AGREEMENT",NULL));

#ifdef LOG_UNUSED_TAGS
  bool *tagUsed=(bool *)tcalloc(patternTagStrings.size(),sizeof(bool));
  for (unsigned dts=0; dts<desiredTagSets.size(); dts++)
    for (unsigned I=0; I<desiredTagSets[dts].tags.size(); I++)
      tagUsed[desiredTagSets[dts].tags[I]]=true;
  for (unsigned I=0; I<patternTagStrings.size(); I++)
    if (!tagUsed[I] && patternTagStrings[I][0]!=L'_' && patternTagStrings[I]!="_BLOCK")
      lplog(L"TAG %s not used in tagSet.",patternTagStrings[I].c_str());
#endif
  desiredTagSets[iverbTagSet].addTagSet(verbSenseTagSet);
  desiredTagSets[subjectVerbRelationTagSet].addTagSet(verbSenseTagSet);
  desiredTagSets[verbObjectRelationTagSet].addTagSet(verbSenseTagSet);

  PREP_TAG=findTag(L"PREP");
  SUBOBJECT_TAG=findTag(L"SUBOBJECT");
  OBJECT_TAG=findTag(L"OBJECT");
  REOBJECT_TAG=findTag(L"RE_OBJECT");
  IOBJECT_TAG=findTag(L"IOBJECT");
  SUBJECT_TAG=findTag(L"SUBJECT");
  PREP_OBJECT_TAG=findTag(L"PREPOBJECT");
  VERB_TAG=findTag(L"VERB");
  PLURAL_TAG=findTag(L"PLURAL");
  MPLURAL_TAG=findTag(L"MPLURAL");
	SENTENCE_IN_REL_TAG=findTag(L"S_IN_REL");
  GNOUN_TAG=findTag(L"GNOUN");
  MNOUN_TAG=findTag(L"MNOUN");
  PNOUN_TAG=findTag(L"PNOUN");
	VNOUN_TAG=findTag(L"VNOUN");
  HAIL_TAG=findTag(L"HAIL");
  NAME_TAG=findTag(L"NAME");
	REL_TAG=findTag(L"REL");
	FLOAT_TIME_TAG=findTag(L"FLOATTIME");
}

int cPattern::hasTagInSet(int desiredTagSetNum,unsigned int &tag)
{ LFS
  if (!tagSetMemberInclusion[desiredTagSetNum]) return -1;
  size_t numTags=desiredTagSets[desiredTagSetNum].tags.size();
  for (; tag<numTags; tag++)
    if (tagSetMemberInclusion[desiredTagSetNum]&((__int64)1<<tag))
      return desiredTagSets[desiredTagSetNum].tags[tag++];
  return -1;
}

wchar_t *patternElement::toText(wchar_t *temp, int J, bool isPattern, int maxBuf)
{
	LFS
		if (isPattern)
		{
			wsprintf(temp, L"%s[%s]*%d|%d|%d%s", patterns[patternIndexes[J]]->name.c_str(), patterns[patternIndexes[J]]->differentiator.c_str(), patternCosts[J], usageEverMatched[J], usageFinalMatch[J], (patternStopDescendingSearch[J]) ? L"{_BLOCK}" : L"");
			if (patternTags[J].size())
			{
				wcscat(temp, L"{");
				for (set <unsigned int>::iterator t = patternTags[J].begin(), tEnd = patternTags[J].end(); t != tEnd; t++)
					_snwprintf(temp + wcslen(temp), maxBuf - wcslen(temp), L"%s:", patternTagStrings[*t].c_str());
				temp[wcslen(temp) - 1] = L'}';
			}
		}
		else
		{
			wstring fs = formStr[J];
			if (specificWords[J].length())
				fs += L"|" + specificWords[J];
			wsprintf(temp, L"%s*%d|%d|%d%s", fs.c_str(), formCosts[J], usageEverMatched[J], usageFinalMatch[J], (formStopDescendingSearch[J]) ? L"{_BLOCK}" : L"");
			if (formTags[J].size())
			{
				wcscat(temp, L"{");
				for (set <unsigned int>::iterator t = formTags[J].begin(), tEnd = formTags[J].end(); t != tEnd; t++)
					_snwprintf(temp + wcslen(temp), maxBuf - wcslen(temp), L"%s:", patternTagStrings[*t].c_str());
				temp[wcslen(temp) - 1] = L'}';
			}
		}
	return temp;
}

void patternElement::reportUsage(wchar_t *temp, int J, bool isPattern)
{
	LFS
	int len = wcslen(temp);
	if (isPattern)
	{
		wstring fullname = patterns[patternIndexes[J]]->name + L"["+patterns[patternIndexes[J]]->differentiator+L"]";
		wsprintf(temp+len, L"%40s  %05d  %05d", fullname.c_str(), usageEverMatched[J], usageFinalMatch[J]);
	}
	else
	{
		wstring fs = formStr[J];
		if (specificWords[J].length())
			fs += L"|" + specificWords[J];
		wsprintf(temp+len, L"%40s  %05d  %05d", fs.c_str(), usageEverMatched[J], usageFinalMatch[J]);
	}
	::lplog(L"%s", temp);
	temp[len] = 0;
}

bool patternElement::hasTag(unsigned int tag)
{ LFS
  for (vector < set <unsigned int> >::iterator I=patternTags.begin(),IEnd=patternTags.end(); I!=IEnd; I++)
    if (I->find(tag)!=I->end())
      return true;
  for (vector < set <unsigned int> >::iterator I=formTags.begin(),IEnd=formTags.end(); I!=IEnd; I++)
    if (I->find(tag)!=I->end())
      return true;
  return false;
}

bool patternElement::hasTag(unsigned int elementIndex,unsigned int tag,bool isPattern)
{ LFS
	if (isPattern)
	{
    if (patternTags[elementIndex].find(tag)!=patternTags[elementIndex].end())
      return true;
	}
	else
    if (formTags[elementIndex].find(tag)!=formTags[elementIndex].end())
      return true;
  return false;
}

int patternElement::hasTagInSet(unsigned int elementIndex,unsigned int desiredTagSetNum,unsigned int &tagNumBySet,bool isPattern)
{ LFS
  if (isPattern)
  {
    for (; tagNumBySet<desiredTagSets[desiredTagSetNum].tags.size(); tagNumBySet++)
      if (find(patternTags[elementIndex].begin(),patternTags[elementIndex].end(),desiredTagSets[desiredTagSetNum].tags[tagNumBySet])!=patternTags[elementIndex].end())
        return desiredTagSets[desiredTagSetNum].tags[tagNumBySet++];
  }
  else
  {
    for (; tagNumBySet<desiredTagSets[desiredTagSetNum].tags.size(); tagNumBySet++)
      if (find(formTags[elementIndex].begin(),formTags[elementIndex].end(),desiredTagSets[desiredTagSetNum].tags[tagNumBySet])!=formTags[elementIndex].end())
        return desiredTagSets[desiredTagSetNum].tags[tagNumBySet++];
  }
  return -1;
}

int cPattern::elementHasTagInSet(unsigned char patternElement,unsigned char elementIndex,unsigned int desiredTagSetNum,unsigned int &tagNumBySet,bool isPattern)
{ DLFS
  if (((signed char)elementIndex)==-2) return -1;
  return elements[patternElement]->hasTagInSet(elementIndex,desiredTagSetNum,tagNumBySet,isPattern);
}

bool cPattern::elementHasTag(unsigned char patternElement,unsigned char elementIndex,int tag,bool isPattern)
{ LFS
	return (((signed char)elementIndex)==-2 || !elements[patternElement]->hasTag(elementIndex,tag,isPattern)) ? false : true;
}

// the only descendant who is a leaf in the tree is the pattern itself.
bool cPattern::onlyDescendant(unsigned int parent,vector <unsigned int> &ancestors)
{ LFS
  for (unsigned int d=0; d<descendantPatterns.size(); d++)
  {
    if (find(ancestors.begin(),ancestors.end(),descendantPatterns[d])!=ancestors.end()) continue;
    ancestors.push_back(descendantPatterns[d]);
    if (descendantPatterns[d]!=parent && !patterns[descendantPatterns[d]]->onlyDescendant(parent,ancestors))
      return false;
  }
  return descendantPatterns.size()>0;
}

bool cPattern::resolveDescendants(bool circular)
{ LFS
  for (unsigned int d=0; d<descendantPatterns.size(); )
  {
    if (circular)
      ::lplog(L"%s[%s]: descendant %s[%s].",name.c_str(),differentiator.c_str(),patterns[descendantPatterns[d]]->name.c_str(),patterns[descendantPatterns[d]]->differentiator.c_str());
    vector <unsigned int> ancestors;
    if (!patterns[descendantPatterns[d]]->descendantPatterns.size() || patterns[descendantPatterns[d]]->onlyDescendant(num,ancestors))
    {
      unsigned int p=descendantPatterns[d];
      descendantPatterns.erase(descendantPatterns.begin()+d);
      descendantTags.insert(patterns[p]->descendantTags.begin(),patterns[p]->descendantTags.end());
      descendantTags.insert(patterns[p]->tags.begin(),patterns[p]->tags.end());
    }
    else
      d++;
  }
  return descendantPatterns.size()==0;
}

void cPattern::evaluateTagSets(unsigned int start,unsigned int end)
{ LFS
  //if (lastTagSetEvaluated>start) return;
  for (unsigned int dts=start; dts<end; dts++)
  {
    if (containsOneOfTagSet(desiredTagSets[dts],desiredTagSets[dts].tags.size(),false))
      includesOneOfTagSet|=(__int64)1<<dts;
    for (unsigned tag=0; tag<desiredTagSets[dts].tags.size(); tag++)
      if (hasTag(desiredTagSets[dts].tags[tag]))
        tagSetMemberInclusion[dts]|=((__int64)1<<tag);
    bool containsAll=setCheckDescendantsForTagSet(desiredTagSets[dts],false);
    if (containsAll)
    {
      includesOnlyDescendantsAllOfTagSet|=(__int64)1<<dts;
      includesDescendantsAndSelfAllOfTagSet|=(__int64)1<<dts;
    }
    else if (setCheckDescendantsForTagSet(desiredTagSets[dts],true))
      includesDescendantsAndSelfAllOfTagSet|=(__int64)1<<dts;
    // this percolates all tag sets through all parents, ignoring any block tags
    // 10/25/2006 - this is so that super tag sets can find elements that are blocked
    if (!start && containsAll)
    {
      tags.insert(desiredTagSets[dts].NAME_TAG);
      for (unsigned p=0; p<patterns.size(); p++)
        if (mandatoryAncestorPatterns.isSet(p))
          patterns[p]->descendantTags.insert(desiredTagSets[dts].NAME_TAG);
    }
  }
}

void cPattern::establishMandatoryChildPatterns(void)
{ LFS
  for (vector <patternElement *>::iterator e=elements.begin(),eEnd=elements.end(); e!=eEnd; e++)
    if ((*e)->minimum && (*e)->patternIndexes.size()==1 && (*e)->formIndexes.size()==0)
    {
      int p=(*e)->patternIndexes[0];
      mandatoryChildPatterns.set(p);
      patterns[p]->mandatoryParentPatterns.set(num);
    }
}

void initializePatterns(void)
{ LFS
  createBasicPatterns();
  createVerbPatterns();
  createSecondaryPatterns1();
  createInfinitivePhrases();
  createPrepositionalPhrases();
	createQuestionPatterns();
  createSecondaryPatterns2();
	createLetterIntroPatterns();

  bool patternError=false;
  for (unsigned int r=0; r<patternReferences.size(); r++)
    patternReferences[r]->resolve(patternError);
  if (patternError) exit(0);
  //int lastTag=patternTagStrings.size();
  int startSuperTagSets;
  initializeTagSets(startSuperTagSets);
  for (unsigned int p=0; p<patterns.size(); p++)
  {
    patterns[p]->establishMandatoryChildPatterns();
    if (patterns[p]->mandatoryChildPatterns.isEmpty())
      patternsWithNoChildren.set(p);
    if (patterns[p]->parentPatterns.isEmpty())
      patternsWithNoParents.set(p);
  }
  int startTime=clock();
  wprintf(L"Evaluating tagsets...               \r");
  for (unsigned int p=0; p<patterns.size(); p++)
  {
    patterns[p]->setAncestorPatterns(p);
    patterns[p]->ancestorsSet=true;
    patterns[p]->setMandatoryAncestorPatterns(p);
    patterns[p]->mandatoryAncestorsSet=true;
  }
  //startTime=clock();
  int unresolvedDescendants=-1;
  while (unresolvedDescendants)
  {
    unresolvedDescendants=0;
    for (unsigned int p=0; p<patterns.size(); p++)
      if (patterns[p]->resolveDescendants(false))
        patterns[p]->evaluateTagSets(0,startSuperTagSets);
      else
        unresolvedDescendants++;
  }
  for (unsigned int p=0; p<patterns.size(); p++)
  {
    patterns[p]->evaluateTagSets(startSuperTagSets,desiredTagSets.size());
#ifdef LOG_PATTERNS
    patterns[p]->lplog();
#endif
  }
  lplog(LOG_INFO,L"Processing patterns took %d ms.",(clock()-startTime));
	wprintf(L"Finished tagsets...               \r");
	//printPatternsInABNF("patterns.abnf",lastTag);
#ifdef LOG_AGREE_PATTERN_EVALUATION
  lplog(L"AGREEMENT PATTERNS");
  for (unsigned int p=0; p<patterns.size(); p++)
    if (patterns[p]->includesDescendantsAndSelfAllOfTagSet&1<<subjectVerbAgreementTagSet)
    {
      lplog(L"%d:AP:%s[%s]",p,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str());
      patterns[p]->evaluateAllTagPatternsForAgreement(1);
    }
    lplog(LOG_FATAL_ERROR,L"END AGREEMENT PATTERNS");
#endif
}

tTS::tTS(unsigned int &tagSetNum,wchar_t *tag,int requiredNumOfTags,...)
{ LFS
  va_list tagMarker;
  va_start( tagMarker,requiredNumOfTags );     /* Initialize variable arguments. */
	tagSetNum=desiredTagSets.size();
  name=tag;
  patternTagStrings.push_back(name);
  required=requiredNumOfTags;
  NAME_TAG=findTag(tag);
  for (unsigned int I=1; true; I++)
  {
    wchar_t *nextTag=va_arg( tagMarker, wchar_t *);
    if (!nextTag) break;
    tags.push_back(findTag(nextTag));
  }
  va_end( tagMarker );              /* Reset variable arguments.      */
}

int findTag(wchar_t *tagName)
{ LFS
  for (unsigned int tag=0; tag<patternTagStrings.size(); tag++)
    if (patternTagStrings[tag]==tagName)
      return tag;
  lplog(LOG_FATAL_ERROR,L"Tag %s not found!",tagName);
  return -1;
}

int findTag(vector <tTagLocation> &tagSet,wchar_t *tagName,int &nextTag)
{ LFS
  for (unsigned int I=nextTag+1; I<tagSet.size(); I++)
    if (patternTagStrings[tagSet[I].tag]==tagName)
    {
      int saveTag=I;
      for (I++; I<tagSet.size() && patternTagStrings[tagSet[I].tag]!=tagName; I++);
      //|| (tagSet[I].sourcePosition==tagSet[saveTag].sourcePosition && tagSet[I].end==tagSet[saveTag].end)); I++);
      if (I==tagSet.size())
        nextTag=-1;
      else
        nextTag=I;
      return saveTag;
    }
    return -1;
}

int findOneTag(vector <tTagLocation> &tagSet,wchar_t *tagName,int start)
{ LFS
  for (unsigned int I=start+1; I<tagSet.size(); I++)
    if (patternTagStrings[tagSet[I].tag]==tagName) return I;
  return -1;
}

int findTagConstrained(vector <tTagLocation> &tagSet,wchar_t *tagName,int &nextTag,unsigned int parentBegin,unsigned int parentEnd)
{ LFS
  vector <tTagLocation>::iterator tsi=tagSet.begin()+nextTag+1,tsiEnd=tagSet.end();
  for (unsigned int I=nextTag+1; tsi!=tsiEnd; I++,tsi++)
    if (tsi->sourcePosition>=parentBegin &&
      tsi->sourcePosition+tsi->len<=parentEnd &&
      patternTagStrings[tsi->tag]==tagName)
    {
      int saveTag=I;
      for (I++,tsi++; tsi!=tsiEnd && (patternTagStrings[tsi->tag]!=tagName || tsi->sourcePosition<parentBegin || tsi->len+tsi->sourcePosition>parentEnd); I++,tsi++);
      if (tsi==tsiEnd)
        nextTag=-1;
      else
        nextTag=I;
      return saveTag;
    }
    return -1;
}

int findTagConstrained(vector <tTagLocation> &tagSet,wchar_t *tagName,int &nextTag,tTagLocation &parentTag)
{ LFS
  unsigned int parentBegin=parentTag.sourcePosition,parentEnd=parentBegin+parentTag.len;
	return findTagConstrained(tagSet,tagName,nextTag,parentBegin,parentEnd);
}

int inSet(unsigned int desiredTagSetNum,int desiredTag)
{ LFS
  size_t numTags=desiredTagSets[desiredTagSetNum].tags.size(),I=0;
  for (; I<numTags && desiredTagSets[desiredTagSetNum].tags[I]!=desiredTag; I++);
  return (I<numTags) ? I : -1;
}

void findTagSetConstrained(vector <tTagLocation> &tagSet,unsigned int desiredTagSetNum,char *tagFilledArray,tTagLocation &parentTag)
{ LFS
  unsigned int parentBegin=parentTag.sourcePosition,parentEnd=parentBegin+parentTag.len;
  int setOffset;
  for (unsigned int I=0; I<tagSet.size(); I++)
    if ((setOffset=inSet(desiredTagSetNum,tagSet[I].tag))>=0 &&
      tagSet[I].sourcePosition>=parentBegin &&
      tagSet[I].sourcePosition+tagSet[I].len<=parentEnd)
      tagFilledArray[setOffset]=1;
}

void findTagSet(vector <tTagLocation> &tagSet,unsigned int desiredTagSetNum,char *tagFilledArray)
{ LFS
  int setOffset;
  for (unsigned int I=0; I<tagSet.size(); I++)
    if ((setOffset=inSet(desiredTagSetNum,tagSet[I].tag))>=0)
      tagFilledArray[setOffset]=1;
}

bool cPattern::equivalentTagSet(vector <tTagLocation> &tagSet,vector <tTagLocation> &tagSet2)
{ LFS
  if (tagSet.size()!=tagSet2.size()) return false;
  vector <tTagLocation>::iterator t=tagSet.begin(),tEnd=tagSet.end(),t2=tagSet2.begin();
  for (; t!=tEnd; t++,t2++)
    if (t->tag!=t2->tag)
      return false;
  return true;
}

void printTagSet(int logType,wchar_t *descriptor,int ts,vector <tTagLocation> &tagSet)
{ LFS
  vector <tTagLocation>::iterator its=tagSet.begin();
  vector <tTagLocation>::iterator itsEnd=tagSet.end();
  if (ts>=0 && descriptor) ::lplog(logType,L"%s TAGSET %05d: #TAGS=%d",descriptor,ts,tagSet.size());
  if (ts>=0)
    for (; its!=itsEnd; its++)
    {
      if (its->isPattern)
        ::lplog(logType,L"TAGSET %05d: %03d %s[%s] %06d:%s[%s](%d,%d) TAG %s [%d,%d]",ts,its->sourcePosition,patterns[its->parentPattern]->name.c_str(),patterns[its->parentPattern]->differentiator.c_str(),
				its->PEMAOffset,patterns[its->pattern]->name.c_str(),patterns[its->pattern]->differentiator.c_str(),its->sourcePosition,its->sourcePosition+its->len,patternTagStrings[its->tag].c_str(),
        its->PEMAOffset,its->parentElement);
      else
        ::lplog(logType,L"TAGSET %05d: %03d %s[%s] %06d:%s(%d,%d) TAG %s [%d,%d]",ts,its->sourcePosition,patterns[its->parentPattern]->name.c_str(),patterns[its->parentPattern]->differentiator.c_str(),
					its->PEMAOffset,Forms[its->pattern]->shortName.c_str(),its->sourcePosition,its->sourcePosition+its->len,patternTagStrings[its->tag].c_str(),
        its->PEMAOffset,its->parentElement);
    }
  else
    for (; its!=itsEnd; its++)
    {
      if (its->isPattern)
        ::lplog(logType,L"%03d %s[%s] %s[%s](%d,%d) TAG %s [%d,%d]",its->sourcePosition,patterns[its->parentPattern]->name.c_str(),patterns[its->parentPattern]->differentiator.c_str(),
        patterns[its->pattern]->name.c_str(),patterns[its->pattern]->differentiator.c_str(),its->sourcePosition,its->sourcePosition+its->len,patternTagStrings[its->tag].c_str(),
        its->PEMAOffset,its->parentElement);
      else
        ::lplog(logType,L"%03d %s[%s] %s(%d,%d) TAG %s [%d,%d]",its->sourcePosition,patterns[its->parentPattern]->name.c_str(),patterns[its->parentPattern]->differentiator.c_str(),
        Forms[its->pattern]->shortName.c_str(),its->sourcePosition,its->sourcePosition+its->len,patternTagStrings[its->tag].c_str(),
        its->PEMAOffset,its->parentElement);
    }
}

// exactly like pema::queryPattern
int Source::queryPattern(int position, wstring pattern, int &maxEnd)
{
	LFS
		int maxLen = -1, pemaPosition = -1, nextByPosition = m[position].beginPEMAPosition;
	for (; nextByPosition != -1; nextByPosition = pema[nextByPosition].nextByPosition)
		if (patterns[pema[nextByPosition].getPattern()]->name == pattern && (pema[nextByPosition].end - pema[nextByPosition].begin) > maxLen)
			maxLen = pema[pemaPosition = nextByPosition].end - pema[nextByPosition].begin;
	if (pemaPosition != -1) maxEnd = pema[pemaPosition].end;
	return pemaPosition;
}

// exactly like pema::queryPattern
int Source::queryPattern(int position, wstring pattern, wstring differentiator)
{
	LFS
	for (int nextByPosition = m[position].beginPEMAPosition; nextByPosition != -1; nextByPosition = pema[nextByPosition].nextByPosition)
		if (patterns[pema[nextByPosition].getPattern()]->name == pattern && patterns[pema[nextByPosition].getPattern()]->differentiator == differentiator)
			return nextByPosition;
	return -1;
}

void Source::printTagSet(int logType,wchar_t *descriptor,int ts,vector <tTagLocation> &tagSet,int position,int PEMAPosition)
{ LFS
  wchar_t temp[1024];
  if (descriptor) wcscpy(temp,descriptor);
	if (PEMAPosition >= 0 && position >= 0)
		wsprintf(temp + ((descriptor == NULL) ? 0 : wcslen(descriptor)), L"%s[%s](%d,%d)",
			patterns[pema[PEMAPosition].getPattern()]->name.c_str(), patterns[pema[PEMAPosition].getPattern()]->differentiator.c_str(), position + pema[PEMAPosition].begin, position + pema[PEMAPosition].end);
  ::printTagSet(logType,temp,ts,tagSet);
}

void printTagSet(int logType,wchar_t *descriptor,int ts,vector <tTagLocation> &tagSet,vector <wstring> &words)
{ LFS
  wstring clause;
  for (unsigned I=0; I<words.size(); I++)
    clause+=words[I]+L" ";
  if (clause.length() && ts>=0) ::lplog(logType,L"%s TAGSET %05d: %s",descriptor,ts,clause.c_str());
  printTagSet(logType,NULL,ts,tagSet);
}

void Source::printTagSet(int logType,wchar_t *descriptor,int ts,vector <tTagLocation> &tagSet,int position,int PEMAPosition,vector <wstring> &words)
{ LFS
  wstring clause;
  for (unsigned I=0; I<words.size(); I++)
    clause+=words[I]+L" ";
  if (clause.length() && ts>=0) ::lplog(logType,L"%s TAGSET %05d: %s",descriptor,ts,clause.c_str());
  printTagSet(logType,NULL,ts,tagSet,position,PEMAPosition);
}

void cPattern::firstForm(void)
{ LFS
}

void cPattern::lastForm(void)
{ LFS
}

void cPattern::firstNonMandatoryForm(void)
{ LFS
}

void cPattern::lastNonMandatoryForm(void)
{ LFS
}

void cPattern::printPatternStatistics(void)
{ LFS
	int totalWinnersMatched=0;
	for (unsigned int p=0; p<patterns.size(); p++) 
		totalWinnersMatched+=patterns[p]->numWinners+patterns[p]->numChildrenWinners;
	if (totalWinnersMatched)
		for (unsigned int p=0; p<patterns.size(); p++)
		{
			if ((p&31)==0)
				::lplog(L"\n%24s[  ]: %8s %7s %10s %11s %10s %7s %%",L"pattern",L"matches",L"ET",L"emi",L"pushes",L"compares",L"hits",L"winners",L"name");
			::lplog(L"%c%23s[%2s]: %08u %07u %010u %010u %011u %010u %07u %5.2f%%",
				(patterns[p]->numWinners==0 && patterns[p]->numChildrenWinners==0) ? '*' : (patterns[p]->fillFlag || patterns[p]->fillIfAloneFlag) ? '!' : ' ',
				patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),
				patterns[p]->numMatches,patterns[p]->evaluationTime,patterns[p]->emi,patterns[p]->numPushes,
				patterns[p]->numComparisons,patterns[p]->numHits,patterns[p]->numWinners+patterns[p]->numChildrenWinners,(float)(patterns[p]->numWinners+patterns[p]->numChildrenWinners)*100/totalWinnersMatched);
		}
	::lplog(L"%40s %40s  %5s  %5s", L"PATTERN", L"PATTERN ELEMENT",L"#EVER",L"#FINAL");
	for (unsigned int p = 0; p < patterns.size(); p++)
		patterns[p]->reportUsage();
	for (unsigned int r=0; r<patternReferences.size(); r++)
			delete patternReferences[r];
	for (unsigned int p=0; p<patterns.size(); p++)
		delete patterns[p];
}

