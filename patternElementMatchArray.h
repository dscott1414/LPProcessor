/*
	patternElementMatchArray.h - document-wide array of one-element pattern hits (PEMA)

	Overview:
		Unlike PMA (one array per source position), there is a single PEMA on
		cSource.  Each tPatternElementMatch is one element of one pattern match:
		parent pattern, which element/alternative matched, the child form or child
		pattern+len, the [begin,end) relative to the parent start, and several
		intrusive linked-list indexes that let later stages walk "same position",
		"same (pattern,end)", or "same child (pattern,end)" without scanning.

	Pipeline position:
		Stage 4.  Filled by cPattern::fillPattern() -> push_back_unique(); compacted
		by consolidateWinners() after eliminateLoserPatterns.  Stages 5+ walk the
		chains (nextByPosition / nextByPatternEnd / nextPatternElement) to collect
		tags and build syntactic relations.

	Key entry points:
		- push_back_unique() - insert into the by-pattern-end chain (sorted by
		  descending begin) or cheapen an existing equivalent element
		- consolidateWinners() / generateWinnerConsolidationArray() - pack winners
		  and rewrite every chain index through the wa[] map
		- getRole() - map element tags onto the role bitfield used by relations
		- ownedByOtherWinningPattern() - true if another winner already claims this
		  (childPattern, childLen)

	Key data structures / globals:
		- content[0..count) - slot 0 is deliberately left unused (collectTags
		  negates PEMA offsets, so 0 cannot be a valid index)
		- PEMAElementMatchedSubIndex - same packing as cMatchElement:
		  bit 31 = is-pattern; bits 15..30 = child pattern #; bits 0..14 = child len
		- flags - WINNER plus the COST_* bits that record which costing passes have
		  already charged this element (COST_EVAL / ND / AGREE / NVO / ROLE / PREP)

	Notes / gotchas:
		- CHILDPATBITS is 15, the shift width used to pack/unpack the child
		  pattern #/len halves of PEMAElementMatchedSubIndex.  It is a plain class
		  constant, not an eFlags enumerator (it used to alias eFlags by value,
		  which made flagsStr's `flagSet(CHILDPATBITS)` test bits 0-3 of `flags`
		  instead of anything meaningful; that spurious flag check was removed).
		- nextByPatternEnd is negative when it points back to the start of a
		  circular chain (-PEMAOffset).  translate() un-negates, remaps, re-negates.
		- begin/end are shorts relative to the parent match start; they must fit in
		  a signed short or push_back fatals.
		- iCost (incremental) is only meaningful inside reduceParent; oCost is the
		  full pattern cost copied onto every element of the match.
*/
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#define IOHANDLE int
int lplog(const lpchar_t *format,...);
extern short logCache;

class cPatternElementMatchArray
{
public:
  unsigned int count;
  unsigned int allocated;
  enum chainType { BY_PATTERN_END=0, BY_POSITION=1, BY_CHILD_PATTERN_END=2 };
	enum eFlags { WINNER_FLAG = 1, COST_EVAL = 2, COST_ND = 4, COST_AGREE = 8, COST_NVO = 16, COST_DONE = 32, IN_CHAIN = 64, ELIMINATED = 128, COST_TERTIARY = 256, COST_ROLE = 512, COST_PREP = 1024 };
	// Shift width for the child pattern#/len packing in PEMAElementMatchedSubIndex
	// (see class notes above).  Unrelated to eFlags; kept as a separate constant
	// so it can never again collide with a flag value.
	static const unsigned int CHILDPATBITS = 15;
  typedef struct _tPatternElementMatch 
  {
    short begin;
    short end;
    unsigned char __patternElement;
    unsigned char __patternElementIndex;
    unsigned char getElement(void) { return __patternElement; }
    unsigned char getElementIndex(void) { return __patternElementIndex; }
	// Which step of the parent pattern, and which OR-alternative of that step, matched.
    void setElementAndIndex(unsigned char cPatternElement,unsigned char patternElementIndex)
    {
      __patternElement=cPatternElement;
      __patternElementIndex=patternElementIndex;
    }
    void setElementMatchedSubIndex(int EMSI)
    {
      PEMAElementMatchedSubIndex=EMSI;
    }
    int nextByPosition;        // the next PEMA entry in the same position - allows routines to traverse downwards
    int nextByPatternEnd; // the next PEMA entry in the same position with the same pattern and end
    // nextByPatternEnd is set negative if it points back to the beginning of the loop
    int nextByChildPatternEnd; // the next PEMA entry in the same position with the same childPattern and childEnd
    int nextPatternElement; // the next PEMA entry in the next position with the same pattern and end - allows routines to skip forward by pattern
    int origin; // beginning of the chain for the first element of the pattern
    //int sourcePosition; // so position not needed on print

		// costing variables - in addition to cost, itself.
    short cumulativeDeltaCost; // accumulates costs or benefits of assessCost (from multiple setSecondaryCosts)
    short tempCost; // used for setSecondaryCosts
		
    void setWinner(void) { flags|=WINNER_FLAG; }
    bool isWinner(void) { return (flags&WINNER_FLAG)!=0; }
    void removeWinnerFlag(void) { flags&=~WINNER_FLAG; }
    void setFlag(int flag) { flags|=flag; }
    bool flagSet(int flag) { return (flags&flag)!=0; }
	// Return true if flag was already set; otherwise set it and return false.
	// Used as a one-shot latch (COST_DONE in processTempCost).
    bool testAndSet(int flag) 
    { 
      if (flags&flag) return true; 
      flags|=flag;
      return false;
    }
    void removeFlag(int flag) { flags&=~flag; }
		bool hasTag(int tag) { return patterns[getParentPattern()]->elementHasTag(__patternElement,__patternElementIndex,tag,isChildPattern()); }
    unsigned int getParentPattern() { return pattern; }
    void setParentPattern(int p) { pattern=p; flags=0; }
    int getOCost() { return cost; }
		// Lowest PMA-element cost seen for this parent; only consumed by reduceParent.
		short getIncrementalCost() {
			return iCost;
		}
		void setIncrementalCost(int ic) {
			iCost=ic;
		}
	// Saturate c into signed-short cost (the full pattern cost copied onto this element).
		void setOCost(int c)
    {
			#ifdef LOG_PATTERN_COST_CHECK
				if (c>MAX_SIGNED_SHORT)
					lplog(u"cost overflow of %d reduced to %d.",c,MAX_SIGNED_SHORT);
			#endif
      if (c>MAX_SIGNED_SHORT) cost=MAX_SIGNED_SHORT;
      else if (c<MIN_SIGNED_SHORT) cost=MIN_SIGNED_SHORT;
      else cost=c;
    }
    void addOCostTillMax(int addedCost)
    {
      setOCost(((int)cost)+addedCost);
    }
    void addICostTillMax(int addedCost)
    {
      if (iCost+addedCost>MAX_SIGNED_SHORT) iCost=MAX_SIGNED_SHORT;
      else if (iCost+addedCost<MIN_SIGNED_SHORT) iCost=MIN_SIGNED_SHORT;
      else iCost+=addedCost;
    }
	// Append the set COST_*/WINNER flag names onto temp (debug).
		const lpchar_t *flagsStr(lpwstring &temp)
		{
			temp.clear();
			if (flagSet(WINNER_FLAG)) temp+=u" WINNER";
			if (flagSet(COST_EVAL)) temp += u" COST_EVAL";
			if (flagSet(COST_ND)) temp += u" COST_ND";
			if (flagSet(COST_AGREE)) temp += u" COST_AGREE";
			if (flagSet(COST_NVO)) temp += u" COST_NVO";
			if (flagSet(COST_DONE)) temp += u" COST_DONE";
			if (flagSet(IN_CHAIN)) temp += u" IN_CHAIN";
			if (flagSet(ELIMINATED)) temp += u" ELIMINATED";
			if (flagSet(COST_ROLE)) temp += u" COST_ROLE";
			return temp.c_str();
		}
    int64_t getRole(int64_t &tagRole); 
	// Child packing: bit31=pattern; bits15-30=child pattern #; bits0-14=child len.
	// getChildForm() is the raw word when bit31 is clear (a form offset, not a pattern).
    bool isChildPattern(void) { return (PEMAElementMatchedSubIndex&cMatchElement::patternFlag)== cMatchElement::patternFlag; }
		unsigned int getChildPattern(void) { return (PEMAElementMatchedSubIndex&~cMatchElement::patternFlag)>>CHILDPATBITS; }
    unsigned int getChildLen(void) { return PEMAElementMatchedSubIndex&((1<<CHILDPATBITS)-1); }
    unsigned int getChildForm(void) { return PEMAElementMatchedSubIndex; }
    void setSubIndex(unsigned int subIndexPattern,unsigned int endPosition) { PEMAElementMatchedSubIndex=(subIndexPattern <<CHILDPATBITS)+endPosition; }
    lpchar_t *toText(unsigned int position,lpchar_t *temp,size_t tempCount,vector <cWordMatch> &m); // batch B5: tempCount
	// Latch COST_DONE and keep tempCost = max(tempCost, maxOCost) once latched.
	// Returns true the first time (caller should remember this position).
    bool processTempCost(int maxOCost)
    {
      bool savePosition;
	    if ((savePosition=!testAndSet(cPatternElementMatchArray::COST_DONE)) || tempCost>maxOCost)
		    tempCost=maxOCost;
      return savePosition;
    }
    // char *toText(char *temp,vector <cWordMatch> &m);  BPM
  private:
    unsigned int PEMAElementMatchedSubIndex; // points to a pattern #/end OR a form #
    unsigned short pattern;
    unsigned short flags;
    short cost;
		// this incremental cost is only used in reduceParent!
		short iCost; // lowest cost of PMA element
	} tPatternElementMatch;
  cPatternElementMatchArray();
  ~cPatternElementMatchArray();
  cPatternElementMatchArray(const cPatternElementMatchArray &rhs);

  // Unpack / pack the child-pattern half of elementMatchedSubIndex (see class notes).
  static unsigned int PATMASK(int elementMatchedSubIndex)
  {
    return (unsigned int)((elementMatchedSubIndex&~cMatchElement::patternFlag)>>CHILDPATBITS);
  }
  static unsigned int ENDMASK(int elementMatchedSubIndex)
  {
    return (unsigned int)(elementMatchedSubIndex&((1<<CHILDPATBITS)-1));
  }
  static int subIndex(int p,int end)
  {
    return (p<<CHILDPATBITS)+ENDMASK(end);
  }
  void clear(void);
  void check(void);
  int greatestLength(unsigned int p,int where);
  bool write(IOHANDLE file);
	bool writeToFile(int file); // batch B5: POSIX fd; renamed off the Win32 API name it shadowed
  bool read(IOHANDLE file);
  bool write(void *buffer,int &where,unsigned int limit);
  bool read(char *buffer,int &where,unsigned int limit);
  bool operator==(const cPatternElementMatchArray &other) const;
  cPatternElementMatchArray& operator=(const cPatternElementMatchArray &rhs);
  bool operator!=(const cPatternElementMatchArray &other) const;
  tPatternElementMatch& operator[](unsigned int _P0);
  const tPatternElementMatch& operator[](unsigned int _P0) const;
  int push_back_unique(int *firstPosition,unsigned int position,int oCost,int iCost,unsigned int p,int begin,int end,int elementMatchedSubIndex,
    unsigned int cPatternElement,unsigned int patternElementIndex,int allocationHint,bool &newElement,bool POFlag);
  void getNextValidPosition(int lastPEMAConsolidationIndex,int *wa,int *nextPosition,enum chainType cType);
  int getNextValidByPosition(int lastPEMAConsolidationIndex,int *wa,int &nextPosition);
  void generateWinnerConsolidationArray(int lastPEMAConsolidationIndex,int *&wa,int &numWinners);
  bool consolidateWinners(int lastPEMAConsolidationIndex,int *wa,int numWinners,sTrace &t);
  void translate(int lastPEMAConsolidationIndex,int *wa,int *position,enum chainType cType);
  void skipPastPositions(int lastPEMAConsolidationIndex,int *&nextPosition,enum chainType cType);
	bool ownedByOtherWinningPattern(int parentPEMAPosition,int nextPosition,int p,int len);
	bool ownedByOtherPattern(int nextPosition,int p,int len);
	int queryTag(int nextPosition,int tag);
  int generatePEMACount(int nextPosition);
  tPatternElementMatch *begin(void) { return content; }
  tPatternElementMatch *end(void) { return content+count; }
private:
  int push_back(int oCost,int iCost,unsigned int p,int begin,int end,int subIndex,unsigned int cPatternElement,unsigned int patternElementIndex,int allocationHint);
  tPatternElementMatch *content;
};
