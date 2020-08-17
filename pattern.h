#include "bitObject.h"
extern vector <wstring> patternTagStrings;

int createBasicPatterns(void);
int createVerbPatterns(void);
void createInfinitivePhrases(void);
void createPrepositionalPhrases(void);
void createSecondaryPatterns1(void);
int createSecondaryPatterns2(void);
void createQuestionPatterns(void);

#define SOURCE_VERSION 7 // increment on any format or pattern change
/*
EXAMPLE
cPattern::create(L"XX{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", L"Y",
										1, L"__C1__S1", 0, 1, 1, ** patternElement 0, patternElementIndex=0
										3, L"_NOUN_OBJ{OBJECT}", L"__NOUN[*]{OBJECT}", L"_ADJECTIVE", 0, 0, 1, ** patternElement 1, patternElementIndex=0, patternElement 1, patternElementIndex=1, patternElement 1, patternElementIndex=2
										0);
*/
class cSource;
class cMatchElement
{
public:
    enum { NEW_FLAG=1 };
		static const unsigned int patternFlag = (1 << 31);
    unsigned int beginPosition;
    unsigned int endPosition;
    // if patternFlag set: (bit 31)
    //    elementMatchedIndex bits 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 - relative position where pattern ends
    //                        bits 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30 - pattern #
    // if patternFlag not set:
    //    elementMatchedIndex is set to the offset into the forms array of the source position (offset into the m array)
    unsigned int elementMatchedIndex;
    short cost;
    unsigned char flags; // this is only used for the "new" flag
    char patternElementIndex;
    int previousMatch;
    int PMAIndex;

    unsigned int cPatternElement; // could be a short but int is better for alignment
    cMatchElement(int inPMAIndex,unsigned int inBeginPosition,unsigned int inEndPosition,unsigned int inElementMatchedIndex,short inCost,bool isPattern,
        int inPreviousMatch,unsigned int inPatternElement,unsigned int inPatternElementIndex,bool inNew)
    {
        // INSERTION of beginPosition - why is it necessary?
        // rather than assume that this pattern match starts from the last pattern match's endPosition, this is necessary because
        // of ignored words, which are ignored in some cases and not ignored in others.  If it is ignored, the present matched
        // pattern must be moved forward by one, but if beginPosition is assumed to be the lastMatched endPosition, and the lastMatched
        // is shared between matches that ignore and matches that do not ignore, THEN you have a possibility for a PMAIndex that
        // references the endPosition of the lastMatch (because there is no beginPosition) which has been moved forward by another match
        // that ignores the position, thus causing PMAIndex to no longer reference the correct sourcePosition.
        beginPosition=inBeginPosition;
        endPosition=inEndPosition;
        elementMatchedIndex=inElementMatchedIndex | ((isPattern) ? patternFlag : 0);
        cost=inCost;
        previousMatch=inPreviousMatch;
        cPatternElement=inPatternElement;
        patternElementIndex=inPatternElementIndex;
        flags=((inNew) ? NEW_FLAG : 0);
        PMAIndex=inPMAIndex;
    };
    unsigned int getChildPattern(void);
    unsigned int getChildLen(void);
    #ifdef LOG_OLD_MATCH
    bool isNew(unsigned int parentPattern)
    {
        if (!(elementMatchedIndex&patternFlag)) return false; // a form cannot be new, as defined as something created on the second or subsequent parsing pass
        if (getChildPattern()>parentPattern) return true; // on the second pass, a child pattern numerically greater than the parent pattern,
        // thus matched after the parent pattern, may create a new possibility for the parent.
        return (flags&NEW_FLAG)==NEW_FLAG; // any pattern that was matched on the second or subsequent pass and created a new PMA entry will have this set.
    }
    #else
    bool isNew(void)
    {
        return (flags&NEW_FLAG)==NEW_FLAG; // any pattern that was matched on the second or subsequent pass and created a new PMA entry will have this set.
    }
    #endif
};

class cPattern;
extern vector <cPattern *> patterns;
extern cBitObject<32, 5, unsigned int, 32> patternsWithNoParents,patternsWithNoChildren;

class cPatternElement {
public:
    int inflectionFlags; // see enum InflectionTypes
    int minimum;
    int maximum;
    wstring patternName;
    int patternNum;
    int elementPosition;
    vector <wstring> formStr;
    vector <wstring> specificWords;
    cIntArray formIndexes;
    cIntArray formCosts;

    vector < set <unsigned int> > formTags;
    vector <bool> formStopDescendingSearch;
    vector <wstring> patternStr;
    cIntArray patternIndexes;
    cIntArray patternCosts;
    vector < set <unsigned int> > patternTags;
    vector <bool> patternStopDescendingSearch;
		vector <int> usageFormFinalMatch;
		vector <int> usageFormEverMatched;
		vector <int> usagePatternFinalMatch;
		vector <int> usagePatternEverMatched;
		set <int> endPositionsSet;
    bool consolidateEndPositions; // no longer used - was for "super" patterns where remembering precise ends was no longer necessary
    cPatternElement(void)
    {
        inflectionFlags=0; // see enum InflectionTypes
        minimum=0;
        maximum=0;
        patternNum=0;
        elementPosition=0;
      consolidateEndPositions=false;
        endPosition = 0;
    };
		void initializeUsage()
		{
			usageFormFinalMatch.reserve(formIndexes.size());
			usageFormEverMatched.reserve(formIndexes.size());
			usagePatternFinalMatch.reserve(patternIndexes.size());
			usagePatternEverMatched.reserve(patternIndexes.size());
		}
    //patternElement(string patternName,string differentiator,int elementNum,set <unsigned int> &descendantTags,char *&buf);
    int matchOne(cSource &source,unsigned int sourcePosition,unsigned int lastElement,vector <cMatchElement> &whatMatched, sTrace &t);
    bool matchRange(cSource &source,int begin,int end,vector <cMatchElement> &whatMatched, sTrace &t);
    bool matchFirst(cSource &source,int sourcePosition,vector <cMatchElement> &whatMatched, sTrace &t);
    bool inflectionMatch(int inflectionFlags,__int64 flags,wstring formStr, sTrace &t);
    wstring formsStr(void);
    bool contains(int pn)
    {
      for (unsigned int e=0; e<patternIndexes.size(); e++)
        if (patternIndexes[e]==pn) return true;
      return false;
    }
		void reportUsage(wchar_t *temp, int J, bool isPattern);
		bool copyUsage(void *buf, int &where, int limit);
		void zeroUsage();
		wchar_t *toText(wchar_t *temp,int J,bool isPattern,int maxBuf);
    bool hasTag(unsigned int tag);
		bool hasTag(unsigned int elementIndex,unsigned int tag,bool isPattern);
    int hasTagInSet(unsigned int patternIndex,unsigned int desiredTagSetNum,unsigned int &tagNumBySet,bool isPattern);
    void writeABNF(wchar_t *buf,int &len);
    int writeABNFElementTag(wchar_t *buf,wstring sForm,int num,int cost,vector <unsigned int> &tags,bool printNum);
    void readABNFElementTag(wstring patternName,wstring differentiator,int elementNum,set <unsigned int> &descendantTags,wchar_t *buf);
    const wchar_t *inflectionFlagsToStr(wstring &sFlags);
		wstring variable;
		int endPosition;
};

void initializePatterns(void);
void printPatternsInABNF(string filePath);

class cTagLocation {
public:
    unsigned short tag;
    unsigned short parentPattern;
    unsigned short parentElement;
    unsigned short pattern; // pattern owning the pattern or element that has tag
    unsigned long sourcePosition;
    unsigned short len;
    short isPattern;
    int PEMAOffset; // offset into pema.  if negative, indicates this tag is for any pattern matching the root of 'pattern'
    cTagLocation(unsigned short inTag,unsigned short inPattern,unsigned short inParentPattern,unsigned short inParentElement,
        unsigned long insourcePosition,unsigned short inLen,unsigned int inPEMAOffset,bool inIsPattern)
    {
        tag=inTag;
        pattern=inPattern;
        parentPattern=inParentPattern;
        parentElement=inParentElement;
        sourcePosition=insourcePosition;
        len=inLen;
        PEMAOffset=inPEMAOffset;
        isPattern=inIsPattern;
    };
    bool operator == (const cTagLocation& o)
    {
        return tag==o.tag &&
            pattern==o.pattern &&
            parentPattern==o.parentPattern &&
            parentElement==o.parentElement &&
            sourcePosition==o.sourcePosition &&
            len==o.len &&
            PEMAOffset==o.PEMAOffset &&
            isPattern==o.isPattern;
    }
    bool operator < (const cTagLocation& o)
    {
        if (tag!=o.tag) return tag<o.tag;
        if (pattern!=o.pattern) return pattern<o.pattern;
        if (parentPattern!=o.parentPattern) return parentPattern<o.parentPattern;
        if (parentElement!=o.parentElement) return parentElement<o.parentElement;
        if (sourcePosition!=o.sourcePosition) return sourcePosition<o.sourcePosition;
        if (len!=o.len) return len<o.len;
        if (PEMAOffset!=o.PEMAOffset) return PEMAOffset<o.PEMAOffset;
        if (isPattern!=o.isPattern) return isPattern<o.isPattern;
        return false;
    }
    bool operator != (const cTagLocation& o)
    {
        return tag!=o.tag ||
            pattern!=o.pattern ||
            parentPattern!=o.parentPattern ||
            parentElement!=o.parentElement ||
            sourcePosition!=o.sourcePosition ||
            len!=o.len ||
            PEMAOffset!=o.PEMAOffset ||
            isPattern!=o.isPattern;
    }
    cTagLocation& operator=(const cTagLocation &o)
    {
        tag=o.tag;
        pattern=o.pattern;
        parentPattern=o.parentPattern;
        parentElement=o.parentElement;
        sourcePosition=o.sourcePosition;
        len=o.len;
        PEMAOffset=o.PEMAOffset;
        isPattern=o.isPattern;
        return *this;
    }
		static bool compareTagLocation(cTagLocation tl1, cTagLocation tl2)
		{
			return tl1.len < tl2.len;
		}
} ;

class cTagSet
{
public:
    wstring name;
    unsigned int NAME_TAG;
    int required;
    vector <unsigned int> tags;
    cTagSet(unsigned int &tagSetNum,wchar_t *tag,int,...);
    void addTagSet(int tagSet);
};

extern vector < cTagSet > desiredTagSets;

int findTag(vector <cTagLocation> &tagSet,wchar_t *tagName,int &nextTag);
int findTag(wchar_t *tagName);
int findOneTag(vector <cTagLocation> &tagSet,wchar_t *tagName,int start=-1);
int findTagConstrained(vector <cTagLocation> &tagSet,wchar_t *tagName,int &nextTag,cTagLocation &parentTag);
int findTagConstrained(vector <cTagLocation> &tagSet,wchar_t *tagName,int &nextTag,unsigned int searchBegin,unsigned int searchEnd);
void findTagSetConstrained(vector <cTagLocation> &tagSet,unsigned int desiredTagSetNum,char *tagFilledArray,cTagLocation &parentTag);
void findTagSet(vector <cTagLocation> &tagSet,unsigned int desiredTagSetNum,char *tagFilledArray);
void printTagSet(int logType,wchar_t *tagSetType,int ts,vector <cTagLocation> &tagSet,vector <wstring> &words);
void printTagSet(int logType,wchar_t *tagSetType,int ts,vector <cTagLocation> &tagSet);
bool tagSetSame(vector <cTagLocation> &tagSet,vector <cTagLocation> &tagSetNew);
void minimizeTagSet(vector  <cTagLocation> &tagSet);

unsigned int findPattern(wstring form);
unsigned int findPattern(wstring form,unsigned int &startingPattern);
unsigned int findPattern(wstring name,wstring diff);

class cPattern {
public:
    cPattern(void)
    {
        emi=0;
        numPushes=0;
        numComparisons=0;
        numHits=0;
        isFutureReference=containsFutureReference=indirectFutureReference=false;
        numMatches=0;
        level=0;
        evaluationTime=0;
        numWinners=0;
        numChildrenWinners=0;
        fillFlag=false;
        fillIfAloneFlag=false;
        onlyBeginMatch=false;
				afterQuote=false;
				strictNoMiddleMatch=false;
				onlyEndMatch=false;
				questionFlag=false;
				notAfterPronoun=false;
				explicitSubjectVerbAgreement = false;
				explicitNounDeterminerAgreement = false;
				metaPattern=false;
        onlyAloneExceptInSubPatternsFlag=false;
        blockDescendants=false;
        includesOneOfTagSet=0;
        includesOnlyDescendantsAllOfTagSet=0;
        includesDescendantsAndSelfAllOfTagSet=0;
        lastTagSetEvaluated=0;
        nextRoot=-1;
        descendantsPopulated=false;
        referencing=false;
        doubleReferencing=false;
        memset(tagSetMemberInclusion,0,64*sizeof(*tagSetMemberInclusion));
        ancestorsSet=false;
        noRepeat=false;
        ignoreFlag=false;
				checkIgnorableForms = false;
        objectTag=-1;
				destinationPatternType=false;
    };
    static bool create(wstring patternName,wstring differentiator,int numForms,...);
		static cPattern *create(cSource *source,wstring patternName,int num,int whereBegin,int whereEnd,unordered_map <wstring, wstring> &parseVariables,bool destinationPatternType);
    bool eliminateTag(wstring tag);
    bool resolveDescendants(bool circular);
    cPattern(cPattern *p);
    ~cPattern(void)
    {
        for (unsigned int e=0; e<elements.size(); e++)
            delete elements[e];
    }
    bool matchPatternPosition(cSource &source,const unsigned int position,bool fill,sTrace &t);
    bool fillPattern(cSource &source,int sourcePosition,vector <cMatchElement> &whatMatched,int elementMatched,unsigned int &insertionPoint,int &reducedCost,bool &pushed,sTrace &t);
    cPattern(string patternName,cPatternElement *element);
    void replace(int elementNum,int patternNum);
    void replace(int elementNum,string patternName);
		void reportUsage(void);
		bool copyUsage(void *buf, int &where, int limit);
		void zeroUsage();
    bool add(int elementNum,wstring patternName,bool logFutureReferences,int cost,set <unsigned int> tags,bool blockDescendants,bool allowRecursiveMatch);
    void lplog(void);
    void readABNF(FILE *fh);
    void writeABNF(FILE *fh,unsigned int lastTag);
    wstring name;
    wstring differentiator;
    unsigned int num;
    unsigned int rootPattern; // first pattern having the same name
    int cost;
    int level;
    int numMatches;
    int evaluationTime;
    int emi;
    int nextRoot;
    int objectTag;
		bool objectContainer;
		bool destinationPatternType;

    set <unsigned int> tags;
    set <unsigned int> descendantTags;
    vector <unsigned int> descendantPatterns;
    unordered_map < wstring, int > variableToLocationMap;
		unordered_map < wstring, int > variableToLengthMap;
		unordered_map < int , wstring > locationToVariableMap;

    // descendantTagSets only used during test procedure -
    bool descendantsPopulated,referencing,doubleReferencing;
    vector < vector<cTagLocation> > descendantTagSets;
    vector < vector<wstring> > descendantWordSets;

    unsigned __int64 includesOneOfTagSet;
    unsigned __int64 tagSetMemberInclusion[64]; // more than 64 tagSets?  more than 64 tags in one tagSet?
    unsigned __int64 includesOnlyDescendantsAllOfTagSet;
    unsigned __int64 includesDescendantsAndSelfAllOfTagSet;
    unsigned int lastTagSetEvaluated;
    bool stopDescendingTagSearch(int element,int index,bool isPattern)
    { 					
			if (isPattern)
			{
				if ((int)elements[element]->patternStopDescendingSearch.size()<=index)
				{
					::lplog(L"_WRONG (1) %s[%s] element #%d index #%d [IS pattern] %d<=%d!",name.c_str(),differentiator.c_str(),element,index,(int)elements[element]->patternStopDescendingSearch.size(),index);
					return false;
				}
			}
			else if ((int)elements[element]->formStopDescendingSearch.size()<=index)
			{
				::lplog(L"_WRONG (2) %s[%s] element #%d index #%d [NOT pattern] %d<=%d!",name.c_str(),differentiator.c_str(),element,index,(int)elements[element]->formStopDescendingSearch.size(),index);
				return false;
			}
			return (isPattern) ? elements[element]->patternStopDescendingSearch[index] : elements[element]->formStopDescendingSearch[index]; 
		}

    // if onlyAloneExceptInSubPatternsFlag or fillIfAloneFlag is set, this makes it possible for patterns to match and fill BUT
    //   be eliminated possibly leaving words with NO matching patterns, but a fill flag.
    bool fillFlag; // pattern regarded as a "top" pattern - the positions are counted as "matched" (not contributing to the notmatched count)
    bool fillIfAloneFlag; // pattern only is regarded as a "top" pattern if previous word has isSeparator=true
    bool onlyBeginMatch; // pattern only matches if previous word has isSeparator=true
    bool afterQuote; // pattern only matches if previous is a quote or a , then a quote
		bool onlyEndMatch; // pattern only matches if end of pattern is the end of sentence
		bool strictNoMiddleMatch; // pattern only matches entire sentence (from punctuation to punctuation)
    bool onlyAloneExceptInSubPatternsFlag; // pattern only wins if it is a subphrase or if previous word has isSeparator=true
    // used if a pattern should only be at the start of a sentence by itself but is also used as a subphrase
    bool blockDescendants;
		// pattern is allowed to submatch itself
		bool allowRecursiveMatch;
    bool isFutureReference;
    bool containsFutureReference;
    bool indirectFutureReference;
    bool noRepeat; // cannot be repeated
    bool ignoreFlag; // ignore pattern - do not consider pattern in costing 
		bool checkIgnorableForms; // during the matching of this pattern, check all forms even if they are marked ignore (such as dash), so if a dash is in the middle of this pattern, it won't match.
		bool questionFlag; // higher cost if not ending in a question mark
		bool notAfterPronoun; // higher cost if after pronoun (she/her)
		bool explicitSubjectVerbAgreement; // contains a subject/verb within itself (blocked otherwise by _BLOCK)
		bool explicitNounDeterminerAgreement; // contains a NOUN with determiner within itself (blocked otherwise by _BLOCK or otherwise not implicitly processed)
		bool metaPattern;
    int numPushes,numComparisons,numHits,numWinners,numChildrenWinners;
    cBitObject<> allElementTags;
		cBitObject<32, 5, unsigned int, 32> childPatterns,mandatoryChildPatterns;
		cBitObject<32, 5, unsigned int, 32> parentPatterns,mandatoryParentPatterns;
    bool ancestorsSet,mandatoryAncestorsSet;
    cBitObject<32, 5, unsigned int, 32> ancestorPatterns,mandatoryAncestorPatterns;
    bool contains(int patternNum,int &elementStart)
    {
        for (elementStart++; elementStart<(signed)elements.size(); elementStart++)
            if (elements[elementStart]->contains(patternNum))
                return true;
        return false;
    }
    size_t numElements(void) { return elements.size(); }
    //static void clearMatched(void)
    //{
    //    whatMatched.clear();
    //}
    void incrementUse(unsigned int elementNum,unsigned int indexNum,bool isPattern)
    {
			if (isPattern)
				elements[elementNum]->usagePatternFinalMatch[indexNum]++;
			else
				elements[elementNum]->usageFormFinalMatch[indexNum]++;
		}
    int getCost(unsigned int elementNum,int offset,bool isPattern)
    {
        return (isPattern) ? elements[elementNum]->patternCosts[offset] : elements[elementNum]->formCosts[offset];
    }
    bool isTopLevelMatch(cSource &source,unsigned int beginPosition,unsigned int endPosition);
    bool nextPossibleElementRange(int currentElement)
    {
        unsigned int low,high;
        for (high=currentElement+1; high<elements.size() && elements[high]->minimum==0; high++);
        if (high>=elements.size())
            high=elements.size()-1;
        if (currentElement>-1 && elements[currentElement]->maximum>1)
            low=currentElement;
        else
            low=currentElement+1;
        //if (t.tracePatternElimination)
        //  ::lplog(LOG_INFO,L"next element of pattern %s[%s] after element #%d will be element #%d-#%d.",name.c_str(),differentiator.c_str(),currentElement,low,high);
        return low<elements.size();
    }
    bool containsOneOfTagSet(cTagSet &desiredTagSet,unsigned int limit,bool includeSelf);
    bool setCheckDescendantsForTagSet(cTagSet &desiredTagSet,bool includeSelf);
    void evaluateTagSets(unsigned int start,unsigned int end);
		void initializeUsage();
    bool hasDescendantTag(unsigned int tag);
    bool onlyDescendant(unsigned int parent,vector <unsigned int> &ancestors);
    bool hasTag(unsigned int tag);
    int elementHasTagInSet(unsigned char cPatternElement,unsigned char patternIndex,unsigned int desiredTagSetNum,unsigned int &tagNumBySet,bool isPattern);
    bool elementHasTag(unsigned char cPatternElement,unsigned char patternIndex,int tag,bool isPattern);
    int hasTagInSet(int desiredTagSetNum,unsigned int &tag);
    void setAncestorPatterns(int childPattern);
    void establishMandatoryChildPatterns(void);
    void setMandatoryAncestorPatterns(int childPattern);
    bool similarSets(set <unsigned int> &tags,set <unsigned int> &tags2);
    bool equivalentTagSet(vector <cTagLocation> &tagSet,vector <cTagLocation> &tagSet2);
		cPatternElement *getElement(int I)
		{
			return elements[I];
		}
    //const char *inflectionFlagsToStr(int flags,string &sFlags);
		static void printPatternStatistics(void);

private:
    vector <cPatternElement *> elements;
//    static vector <cMatchElement> whatMatched;
    void static processForm(wstring &form,wstring &specificWord,int &cost,set <unsigned int> &tags,bool &explicitFutureReference,bool &blockDescendants, bool &allowRecursiveMatch);
    void firstForm(void);
    void lastForm(void);
    void firstNonMandatoryForm(void);
    void lastNonMandatoryForm(void);
};

class cPatternReference
{
public:
    static int firstPatternReference,lastPatternReference;
    cPatternReference(wstring inForm,int inPatternNum,int inDiffNum,int inElementNum,int inCost,set <unsigned int> inTags, bool inBlockDescendants, bool inAllowRecursiveMatch, bool inLogFutureReferences)
    {
        form=inForm;
        patternNum=inPatternNum;
        diffNum=inDiffNum;
        elementNum=inElementNum;
        cost=inCost;
        tags=inTags;
        logFutureReferences=inLogFutureReferences;
				blockDescendants = inBlockDescendants;
				allowRecursiveMatch = inAllowRecursiveMatch;
		};
    void resolve(bool &patternError);
    void resolve(vector <cPattern *> &patterns,bool &patternError);
    wstring form;
    int patternNum;
    int diffNum;
    int elementNum;
    int cost;
    set <unsigned int> tags;
    bool blockDescendants;
    bool logFutureReferences;
		bool allowRecursiveMatch;
private:
};
extern vector <cPatternReference *> patternReferences;

