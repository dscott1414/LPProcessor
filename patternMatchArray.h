#define IOHANDLE int
int lplog(const wchar_t *format,...);
extern short logCache;

class cPatternMatchArray
{
public:
	unsigned int count;
	unsigned int allocated;
	typedef struct _tPatternMatch
	{
		short len;
		short cost;
		void resetFlags(void) { flags=0; }
		bool isWinner(void) { return (flags&(1<<WINNER_FLAG))!=0; }
		bool isEval(void) { return (flags&(1<<COST_EVAL_FLAG))!=0; }
		void removeWinnerFlag(void) { flags&=~(1<<WINNER_FLAG); }
		void removeEvalFlag(void) { flags&=~(1<<COST_EVAL_FLAG); }
#ifdef LOG_OLD_MATCH
		bool isNew(void) { return (flags&(1<<NEW_FLAG))!=0; }
		void removeNewFlag(void) { flags&=~(1<<NEW_FLAG); }
		void setNew(void) { flags|=(1<<NEW_FLAG); }
#else
		// set at the time of insertion
		void setPass(int pass) { flags|=(pass<<PASS_FLAGS); }
		bool isNew(int pass) { return pass-2<(flags>>PASS_FLAGS); } // pass==2 flags==0 false
#endif
		void setWinner(void) { flags|=(1<<WINNER_FLAG); }
		void setEval(void) { flags|=(1<<COST_EVAL_FLAG); }
		unsigned int getPattern() { return pattern; }
		int getCost() { return cost; }
		int getAverageCost() { return 1000*cost/len; } // COSTCALC
		int getAverageCost(int addedCost) { return 1000*(cost+addedCost)/len; } // COSTCALC
		void setPattern(short inPattern) { pattern=inPattern; } // setting pattern assumes resetting winnerflag and cost
		void addCostTillMax(int deltaCost)
		{
			if (cost+deltaCost>MAX_SIGNED_SHORT)
				cost=MAX_SIGNED_SHORT;
			else if (cost+deltaCost<MIN_SIGNED_SHORT)
				cost=MIN_SIGNED_SHORT;
			else
				cost+=deltaCost;
		}
		int pemaByPatternEnd; // first PEMA entry in this source position the PMA array belongs to that has the same pattern and end
		int pemaByChildPatternEnd; // first PEMA entry in this source position that has a child pattern = pattern and childEnd = end
		// the below field is set when a descendant pattern (like _S1) of a pattern (like _MS1) must
		// store the relations that occurred in it.  Normally this pattern would directly set its cost.
		// but in the case of a pattern like _MS1, the relations in _S1 are counted against the cost of _MS1,
		// and so the cost of _S1 is artificially lower because there is no relationCost.  If there is
		// another pattern _S1 that is encountered before _MS1, that _S1 does not set descendantRelationships,
		// and its cost is fully accounted for in the pm->cost.
		short descendantRelationships;
	private:
		enum flags { WINNER_FLAG=0,NEW_FLAG=1,COST_EVAL_FLAG=2,PASS_FLAGS=3,ELIMINATE_FLAG=4 };
		unsigned short pattern;
		unsigned short flags;
	} tPatternMatch;
	cPatternMatchArray();
	~cPatternMatchArray();
	void clear(void);
	cPatternMatchArray(const cPatternMatchArray &rhs);

	tPatternMatch *content;
	void minimize(void);
	bool write(IOHANDLE file);
	bool read(char *buffer,int &where,unsigned int limit);
	bool write(void *buffer,int &where,unsigned int limit);
	bool operator==(const cPatternMatchArray other) const;
	cPatternMatchArray& operator=(const cPatternMatchArray &rhs);
	bool operator!=(const cPatternMatchArray other) const;
	typedef allocator<tPatternMatch>::reference reference;
	typedef allocator<tPatternMatch>::const_reference const_reference;
	reference operator[](unsigned int _P0);
	const_reference operator[](unsigned int _P0) const;
	//bool push_back(tPatternMatch &pm);
	int push_back_unique(int pass,short cost,unsigned short p,short end,bool &reduced,bool &pushed);
	int getNextPosition(int w);
	int erase(unsigned int at);
	int erase(void);
	tPatternMatch *find(unsigned int p,short end);
	tPatternMatch *lower_bound(unsigned int p,short end);
	bool consolidateWinners(int lastPEMAConsolidationIndex,cPatternElementMatchArray &pema,int *wa,int position,int &maxMatch,sTrace &t);
	bool findMaxLen(wstring pattern,int &element);
	int findMaxLen(void);
	int queryPatternDiff(wstring pattern,wstring differentiator,int &maxLen);
	int queryPatternDiffLessThenLength(wstring pattern, wstring differentiator, int &maxLen);
	int queryPatternDiff(wstring pattern,wstring differentiator);
	int findAgent(int &element,int maximumMaxEnd,bool allowPronouns);
	int queryPattern(wstring pattern);
	int queryPattern(wstring pattern,int &len);
	int queryAllPattern(wstring pattern,int startAt);
	int queryPattern(int pattern,int &len);
	int queryPattern(int pattern);
	int queryPatternWithLen(wstring pattern,int len);
	int queryPatternWithLen(int pattern,int len);
	int queryMaximumLowestCostPattern(wstring pattern,int &len);
	int queryQuestionFlagPattern();
	int querySingleNoun(int &end);
	int queryTagSet(unsigned int &element,int desiredTagSetNum,int &maxLen);
	int queryTag(int tag)
	{
		int gElement=-1,maxLen=-1;
		for (unsigned int PMAElement=0; PMAElement<count; PMAElement++)
			if (patterns[content[PMAElement].getPattern()]->hasTag(tag) && content[PMAElement].len>maxLen)
			{
				gElement=PMAElement;
				maxLen=content[PMAElement].len;
				break;
			}
		return gElement;
	}
	int findObjectElement(int &lastTag);

private:
	int push_back(unsigned int insertionPoint,int pass,short cost,unsigned short p,short end);
};

