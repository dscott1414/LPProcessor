#define IOHANDLE int
int lplog(const wchar_t *format,...);
extern short logCache;

class cPatternElementMatchArray
{
public:
  unsigned int count;
  unsigned int allocated;
  enum chainType { BY_PATTERN_END=0, BY_POSITION=1, BY_CHILD_PATTERN_END=2 };
	enum eFlags { WINNER_FLAG = 1, CHILDPATBITS = 15, COST_EVAL = 2, COST_ND = 4, COST_AGREE = 8, COST_NVO = 16, COST_DONE = 32, IN_CHAIN = 64, ELIMINATED = 128, COST_TERTIARY = 256, COST_ROLE = 512, COST_PREP = 1024 };
  typedef struct _tPatternElementMatch 
  {
    short begin;
    short end;
    unsigned char __patternElement;
    unsigned char __patternElementIndex;
    unsigned char getElement(void) { return __patternElement; }
    unsigned char getElementIndex(void) { return __patternElementIndex; }
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
		// this incremental cost is only used in reduceParent!
		short getIncrementalCost() {
			return iCost;
		}
		void setIncrementalCost(int ic) {
			iCost=ic;
		}
		void setOCost(int c)
    {
			#ifdef LOG_PATTERN_COST_CHECK
				if (c>MAX_SIGNED_SHORT)
					lplog(L"cost overflow of %d reduced to %d.",c,MAX_SIGNED_SHORT);
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
		const wchar_t *flagsStr(wstring &temp)
		{
			temp.clear();
			if (flagSet(WINNER_FLAG)) temp+=L" WINNER";
			if (flagSet(CHILDPATBITS)) temp += L" CHILDPATBITS";
			if (flagSet(COST_EVAL)) temp += L" COST_EVAL";
			if (flagSet(COST_ND)) temp += L" COST_ND";
			if (flagSet(COST_AGREE)) temp += L" COST_AGREE";
			if (flagSet(COST_NVO)) temp += L" COST_NVO";
			if (flagSet(COST_DONE)) temp += L" COST_DONE";
			if (flagSet(IN_CHAIN)) temp += L" IN_CHAIN";
			if (flagSet(ELIMINATED)) temp += L" ELIMINATED";
			if (flagSet(COST_ROLE)) temp += L" COST_ROLE";
			return temp.c_str();
		}
    __int64 getRole(__int64 &tagRole); 
    bool isChildPattern(void) { return (PEMAElementMatchedSubIndex&cMatchElement::patternFlag)== cMatchElement::patternFlag; }
		unsigned int getChildPattern(void) { return (PEMAElementMatchedSubIndex&~cMatchElement::patternFlag)>>CHILDPATBITS; }
    unsigned int getChildLen(void) { return PEMAElementMatchedSubIndex&((1<<CHILDPATBITS)-1); }
    unsigned int getChildForm(void) { return PEMAElementMatchedSubIndex; }
    void setSubIndex(unsigned int subIndexPattern,unsigned int endPosition) { PEMAElementMatchedSubIndex=(subIndexPattern <<CHILDPATBITS)+endPosition; }
    wchar_t *toText(unsigned int position,wchar_t *temp,vector <cWordMatch> &m); 
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
  void minimize(void);
  void clear(void);
  void check(void);
  int greatestLength(unsigned int p,int where);
  bool write(IOHANDLE file);
	bool WriteFile(HANDLE file);
  bool read(IOHANDLE file);
  bool write(void *buffer,int &where,unsigned int limit);
  bool read(char *buffer,int &where,unsigned int limit);
  bool operator==(const cPatternElementMatchArray other) const;
  cPatternElementMatchArray& operator=(const cPatternElementMatchArray &rhs);
  bool operator!=(const cPatternElementMatchArray other) const;
  typedef allocator<tPatternElementMatch>::reference reference;
  typedef allocator<tPatternElementMatch>::const_reference const_reference;
  reference operator[](unsigned int _P0);
  const_reference operator[](unsigned int _P0) const;
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
