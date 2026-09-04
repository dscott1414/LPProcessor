/*
	patternMatchArray.cpp - storage, insert, query and winner-compaction for per-token PMA

	Overview:
		Implements cPatternMatchArray: a growable, (pattern#,len)-sorted array of
		competing pattern matches that start at one source position.  Insertion is
		push_back_unique (lower_bound + optional memmove insert).  After costing,
		consolidateWinners copies winner slots down and remaps the two PEMA chain
		heads each slot holds.

	Pipeline position:
		Stage 4.  Called from cPattern::fillPattern (writes) and from
		eliminateLoserPatterns / relation and object queries (reads).

	Key entry points:
		- push_back_unique() / push_back() - insert or locate a (pattern,len)
		- find() / lower_bound() - bsearch and hand-rolled lower_bound
		- consolidateWinners() - keep only winners; translate PEMA indexes
		- queryPattern* / queryPatternDiff* / queryTagSet / findAgent / getNextPosition

	Key data structures / globals:
		- content / count / allocated - the buffer; allocated doubles from 5
		- compare() - qsort/bsearch comparator on (pattern, len)

	Notes / gotchas:
		- clear() tfree's content and NULLs it, matching the destructor and
		  PEMA::clear.
		- read() parses count via copy() (which bounds-checks the read itself)
		  before using it for allocation or the payload bounds check.
		- queryPattern(int, int& len) initializes len=-1 on entry, same as the
		  other overloads.
		- getNextPosition seeds minPatternMatch with INT_MIN.
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
#include "profile.h"
#include <climits>

// Empty PMA.  content is NULL until the first push_back.
cPatternMatchArray::cPatternMatchArray()
{
	LFS
		count = 0;
	allocated = 0;
	content = NULL;
};

// Free the buffer (if any) and leave a NULL content so a stray use is a clean crash.
cPatternMatchArray::~cPatternMatchArray()
{
	LFS
		if (allocated) tfree(allocated * sizeof(*content), content);
	count = 0;
	allocated = 0;
	content = NULL;
}

// Drop every entry and free the buffer, NULLing content like the destructor.
void cPatternMatchArray::clear(void)
{
	LFS
		if (allocated) tfree(allocated * sizeof(*content), content);
	count = 0;
	allocated = 0;
	content = NULL;
}

// Deep copy.  On tmalloc failure logs FATAL and returns with content==NULL but
// count/allocated still copied from rhs (any later [] is a null deref).
cPatternMatchArray::cPatternMatchArray(const cPatternMatchArray& rhs)
{
	LFS
		count = rhs.count;
	allocated = rhs.allocated;
	content = NULL;
	if (allocated)
	{
		content = (tPatternMatch*)tmalloc(allocated * sizeof(*content));
		if (!content)
		{
			lplog(LOG_FATAL_ERROR, u"OUT OF MEMORY (1)");
			return;
		}
		memcpy(content, rhs.content, count * sizeof(*content));
	}
}

// Shrink allocated down to count.  trealloc result is assigned over content:
// if it fails the original buffer is leaked and content becomes NULL.
void cPatternMatchArray::minimize(void)
{
	LFS
		int oldAllocated = allocated;
	allocated = count;
	content = (tPatternMatch*)trealloc(2, content, oldAllocated * sizeof(*content), allocated * sizeof(*content));
}

// Write count then the raw content bytes to a POSIX fd.  Return is always true;
// ::write errors are ignored.
bool cPatternMatchArray::write(IOHANDLE file)
{
	LFS
		::write(file, &count, sizeof(count));
	::write(file, content, count * sizeof(*content));
	return true;
}

// Deserialize from a memory image.  copy() parses the real count (bounds-checked
// against limit internally) before it is used for allocation or the payload
// bounds check below.  Returns false on a short buffer or tmalloc failure;
// fatals if the memcpy would exceed limit.
bool cPatternMatchArray::read(char* buffer, int& where, unsigned int limit)
{
	LFS
	if (where + sizeof(count) > limit) return false;
	if (!copy(count, buffer, where, limit)) return false;
	allocated = count;
	content = (tPatternMatch*)tmalloc(count * sizeof(*content));
	if (!content)
	{
		::lplog();
		return false;
	}
	if (where + count * sizeof(*content) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! PMA %d (20)", limit, where + count * sizeof(*content));
	memcpy(content, buffer + where, count * sizeof(*content));
	where += count * sizeof(*content);
	return true;
}

// Serialize into a memory image via copy() + memcpy.  Fatals if the payload
// would exceed limit after the count has already been written.
bool cPatternMatchArray::write(void* buffer, int& where, unsigned int limit)
{
	LFS
		if (!copy(buffer, count, where, limit)) return false;
	if (where + count * sizeof(*content) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (16)", limit);
	memcpy(((char*)buffer) + where, content, count * sizeof(*content));
	where += count * sizeof(*content);
	return true;
}

// Byte-compare content[0..count).
bool cPatternMatchArray::operator==(const cPatternMatchArray &other) const
{
	LFS
		if (count != other.count) return false;
	return memcmp(content, other.content, count * sizeof(*content)) == 0;
}

// Replace this buffer with a deep copy of rhs.
cPatternMatchArray& cPatternMatchArray::operator=(const cPatternMatchArray& rhs)
{
	LFS
	if (this == &rhs) return *this;
	if (allocated) tfree(allocated * sizeof(*content), content);
	count = rhs.count;
	allocated = rhs.allocated;
	content = NULL;
	if (allocated)
	{
		content = (tPatternMatch*)tmalloc(allocated * sizeof(*content));
		if (!content)
		{
			lplog(LOG_FATAL_ERROR, u"OUT OF MEMORY (3)");
			return *this;
		}
		memcpy(content, rhs.content, count * sizeof(*content));
	}
	return *this;
}

// Inverse of operator==.
bool cPatternMatchArray::operator!=(const cPatternMatchArray &other) const
{
	LFS
		if (count != other.count) return true;
	return memcmp(content, other.content, count * sizeof(*content)) != 0;
}

// Slot _P0.  INDEX_CHECK (off by default) logs and returns content[0] on OOB,
// pushing a dummy entry if the array is empty.
cPatternMatchArray::tPatternMatch& cPatternMatchArray::operator[](unsigned int _P0)
{
	LFS
#ifdef INDEX_CHECK
		if (_P0 >= count || _P0 < 0)
		{
			logCache = 0;
			lplog(u"Illegal reference (7) to element %d in an array with only %d elements!", _P0, count);
			if (count == 0) push_back(0, 0, 0, 0);
			return content[0];
		}
#endif
	return (content[_P0]);
}

// Const [] — INDEX_CHECK throws if count==0 rather than inserting a dummy.
const cPatternMatchArray::tPatternMatch& cPatternMatchArray::operator[](unsigned int _P0) const
{
	LFS
#ifdef INDEX_CHECK
		if (_P0 >= count || _P0 < 0)
		{
			logCache = 0;
			lplog(u"Illegal reference (8) to element %d in an array with only %d elements!", _P0, count);
			if (count == 0) throw;
			return content[0];
		}
#endif
	return (content[_P0]);
}

// Insert a new slot at insertionPoint, shifting the tail right.  Grows allocated
// *2 from 5.  Initializes PEMA heads to -1 and descendantRelationships to -1.
// Returns insertionPoint.  No uniqueness check — caller is push_back_unique.
int cPatternMatchArray::push_back(unsigned int insertionPoint, int pass, short cost, unsigned short p, short end)
{
	LFS
		count++;
	if (allocated <= count)
	{
		int oldAllocated = allocated;
		if (!allocated) allocated = 5;
		else allocated *= 2;
		//allocated=count+1;
		content = (tPatternMatch*)trealloc(4, content, oldAllocated * sizeof(*content), allocated * sizeof(*content));
	}
	if (insertionPoint != count - 1)
		memmove(content + insertionPoint + 1, content + insertionPoint, ((count - 1) - insertionPoint) * sizeof(*content));
	tPatternMatch* c = content + insertionPoint;
	c->setPattern(p);
	c->cost = cost;
	c->resetFlags();
	c->len = end;
	c->pemaByPatternEnd = -1; // first PEMA entry in this source position the PMA array belongs to that has the same pattern and end
	c->pemaByChildPatternEnd = -1; // first PEMA entry in this source position that has a child pattern = pattern and childEnd = end
	c->descendantRelationships = -1;
#ifdef LOG_OLD_MATCH
	// on second and subsequent passes, set this flag to indicate that parent patterns
		//   having only non-new matches can be discarded.  Only valid if it can be guaranteed that the previous pass
		// used the same pattern (which is only true of old matching method)
	if (pass)
		content[insertionPoint].setNew();
#else
	content[insertionPoint].setPass(pass);
#endif
	return insertionPoint;
}

// this sorts by ascending pattern # and by ascending end #
// Locate (p,end) via lower_bound.  If present and this cost is cheaper, set
// reduced (the slot's cost is NOT written here — fillPattern/reduceParents
// does that).  If absent, insert; on pass>=1 force cost=MAX_SIGNED_SHORT and
// reduced=true so reduceParents rewrites parents.  pushed is true iff a slot
// was created.  Returns the slot index.
int cPatternMatchArray::push_back_unique(int pass, short cost, unsigned short p, short end, bool& reduced, bool& pushed)
{
	LFS
		pushed = reduced = false;
	if (pushed = !count) return push_back(0, pass, cost, p, end);
	tPatternMatch* c = lower_bound(p, end);
	if ((unsigned)(c - content) < count && c->getPattern() == p && c->len == end)
	{
		if (c->getCost() > cost)
		{
			reduced = true;
			// make sure lowest cost possibility of match is recorded
#ifdef LOG_PATTERN_COST_CHECK
			lplog(u"%d:%s[%s](%d,%d) PMA cost (will be) reduced from %d to %d", position, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position, position + end,
				c->getCost(), cost);
#endif
		}
		return (int)(c - content);
	}
	if (pass >= 1)
	{
		// this pass is predicated on that patterns are always bunched together.
		// Therefore, a pattern using a child pattern that was not already there must be on the second pass (or be using a forward reference).
		// example:
		//   __NOUN[D] is a forward reference.
		//   A parent pattern that matches a NOUN may be before __NOUN[D].  So when __NOUN[D] is matched, it may create a lower cost
		//     parent pattern.  Therefore, set "reduced" to update parents to that possibly lower cost.
		reduced = true; // if second pass, a new match MAY also match PEMA pattern
		// make sure lowest cost possibility of match is recorded
#ifdef LOG_PATTERN_COST_CHECK
		lplog(u"%d:%s[%s](%d,%d) PMA cost (will be) reduced from %d to %d (2)", position, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position, position + end,
			MAX_SIGNED_SHORT, cost);
#endif
		cost = MAX_SIGNED_SHORT; // ensure update with reduceParents
	}
	pushed = true;
	return push_back((int)(c - content), pass, cost, p, end);
}

// Remove slot `at` by memmove.  INDEX_CHECK throws on empty / OOB.
int cPatternMatchArray::erase(unsigned int at)
{
	LFS
#ifdef INDEX_CHECK
		if (at >= count || at < 0)
		{
			logCache = 0;
			lplog(u"Illegal reference (9) to element %d in an array with only %d elements!", at, count);
			if (count == 0) throw;
			return count;
		}
#endif
	memmove(content + at, content + at + 1, (count - at - 1) * sizeof(content[at]));
	count--;
	return count;
}

// Drop every entry without freeing the buffer (count=0; allocated stays).
int cPatternMatchArray::erase(void)
{
	LFS
		count = 0;
	return 0;
}

// Longest match whose pattern *name* equals `pattern`.  On success element is
// the PMA index and the return is true; otherwise element=-1 and false.
bool cPatternMatchArray::findMaxLen(lpwstring pattern, int& element)
{
	LFS
		element = -1;
	int maxLen = -1;
	for (unsigned int I = 0; I < count; I++)
		if (patterns[content[I].getPattern()]->name == pattern && content[I].len > maxLen)
		{
			maxLen = content[I].len;
			element = I;
		}
	return element >= 0;
}

// PMA index of the longest match of any pattern, or -1 if the array is empty.
int cPatternMatchArray::findMaxLen(void)
{
	LFS
		int element = -1, maxLen = -1;
	for (unsigned int I = 0; I < count; I++)
		if (content[I].len > maxLen)
		{
			maxLen = content[I].len;
			element = I;
		}
	return element;
}

// PMA index (OR'd with patternFlag) of the longest match named `pattern`, or
// the discarded maxLen overload's return (-1 if none).
int cPatternMatchArray::queryPattern(lpwstring pattern)
{
	LFS
		int maxLen;
	return queryPattern(pattern, maxLen);
}

// Longest match named `pattern`.  Returns PMA index | patternFlag, or -1; len
// is set to that match's length or -1 if none.
int cPatternMatchArray::queryPattern(lpwstring pattern, int& len)
{
	LFS
		int maxLen = -1, element = -1;
	for (unsigned int I = 0; I < count; I++)
		if (patterns[content[I].getPattern()]->name == pattern && content[I].len > maxLen)
		{
			maxLen = content[I].len;
			element = I | cMatchElement::patternFlag;
		}
	len = maxLen;
	return element;
}

// First PMA index at or after startAt whose pattern name equals `pattern`, or -1.
int cPatternMatchArray::queryAllPattern(lpwstring pattern, int startAt)
{
	LFS
		for (unsigned int I = startAt; I < count; I++)
			if (patterns[content[I].getPattern()]->name == pattern)
				return I;
	return -1;
}

// Among matches named `pattern`, pick the cheapest, breaking ties by longest
// len.  Returns PMA index | patternFlag (or -1).  minCost starts at 10000, so a
// match costing more than that is ignored.
int cPatternMatchArray::queryMaximumLowestCostPattern(lpwstring pattern, int& len)
{
	LFS
		int maxLen = -1, element = -1, minCost = 10000;
	for (unsigned int I = 0; I < count; I++)
		if (patterns[content[I].getPattern()]->name == pattern &&
			(content[I].cost < minCost || (content[I].cost == minCost && content[I].len > maxLen)))
		{
			maxLen = content[I].len;
			minCost = content[I].cost;
			element = I | cMatchElement::patternFlag;
		}
	len = maxLen;
	return element;
}

// Longest match of pattern *number* `pattern`.  len is initialized to -1 on
// entry, same as the lpwstring overload.
int cPatternMatchArray::queryPattern(int pattern, int& len)
{
	LFS
	len = -1;
	int element = -1;
	for (unsigned int I = 0; I < count; I++)
		if (content[I].getPattern() == pattern && content[I].len > len)
		{
			len = content[I].len;
			element = I | cMatchElement::patternFlag;
		}
	return element;
}

// Longest match whose pattern belongs to desiredTagSetNum.  On a length tie,
// a NAME tag already chosen wins over a later NOUN (guarded by tag>=0 so an
// unset `tag` from a prior no-op hasTagInSet is never used to index
// patternTagStrings).  Returns the tag id (or -1); element is the PMA index
// | patternFlag.
int cPatternMatchArray::queryTagSet(unsigned int& element, int desiredTagSetNum, int& maxLen)
{
	LFS
		unsigned int tagInSet;
	maxLen = -1;
	int tag = -1;
	for (unsigned int I = 0; I < count; I++)
		if (content[I].len >= maxLen && patterns[content[I].getPattern()]->tagSetMemberInclusion[desiredTagSetNum])
		{
			if (content[I].len == maxLen && tag >= 0 && patternTagStrings[tag] == u"NAME") continue; // NAME tags have precedence over NOUN tags
			tagInSet = 0;
			tag = patterns[content[I].getPattern()]->hasTagInSet(desiredTagSetNum, tagInSet);
			maxLen = content[I].len;
			element = I | cMatchElement::patternFlag;
		}
	return tag;
}


// Exact (pattern#, len) via bsearch.  Returns PMA index | patternFlag, or -1.
int cPatternMatchArray::queryPatternWithLen(int pattern, int len)
{
	LFS
		tPatternMatch* e;
	if ((e = find(pattern, len)) == NULL) return -1;
	return (int)(e - content) | cMatchElement::patternFlag;
}

// Last (not first) PMA index | patternFlag whose name and len both match, or -1.
int cPatternMatchArray::queryPatternWithLen(lpwstring pattern, int len)
{
	LFS
		int element = -1;
	for (unsigned int I = 0; I < count; I++)
		if (patterns[content[I].getPattern()]->name == pattern && content[I].len == len)
			element = I | cMatchElement::patternFlag;
	return element;
}

// Longest match of pattern *number* `pattern`.  Returns PMA index | patternFlag, or -1.
int cPatternMatchArray::queryPattern(int pattern)
{
	LFS
		int maxLen = -1, element = -1;
	for (unsigned int I = 0; I < count; I++)
		if (content[I].getPattern() == pattern && content[I].len > maxLen)
		{
			maxLen = content[I].len;
			element = I | cMatchElement::patternFlag;
		}
	return element;
}

// Longest __NOUN whose differentiator is '2' (common noun) or, if
// includePronouns, 'C' (pronominal), and whose len <= maximumMaxLen.
// Returns the PMA index | patternFlag (also written to element), or -1.
int cPatternMatchArray::findAgent(int& element, int maximumMaxLen, bool includePronouns)
{
	LFS
		element = -1;
	int maxLen = -1;
	for (unsigned int I = 0; I < count; I++)
	{
		int p = content[I].getPattern();
		lpchar_t diff = patterns[p]->differentiator[0];
		if (patterns[p]->name == u"__NOUN" &&
			(diff == u'2' || (includePronouns && diff == u'C')) &&
			content[I].len > maxLen && content[I].len <= maximumMaxLen)
		{
			maxLen = content[I].len;
			element = I | cMatchElement::patternFlag;
		}
	}
	return element;
}

// Longest match of (pattern, differentiator); discards the maxLen out-param.
int cPatternMatchArray::queryPatternDiff(lpwstring pattern, lpwstring differentiator)
{
	LFS
		int maxLen = -1;
	return queryPatternDiff(pattern, differentiator, maxLen);
}

// Longest match of (name, differentiator).  "*" means any differentiator;
// "X*" means differentiator[0]=='X'.  Returns PMA index | patternFlag, or -1.
int cPatternMatchArray::queryPatternDiff(lpwstring pattern, lpwstring differentiator, int& maxLen)
{
	LFS
		maxLen = -1;
	if (differentiator == u"*")
		return queryPattern(pattern, maxLen);
	int element = -1;
	if (differentiator.length() > 1 && differentiator[1] == u'*')
	{
		for (unsigned int I = 0; I < count; I++)
			if (patterns[content[I].getPattern()]->name == pattern && patterns[content[I].getPattern()]->differentiator[0] == differentiator[0] &&
				content[I].len > maxLen)
			{
				maxLen = content[I].len;
				element = I | cMatchElement::patternFlag;
			}
	}
	else
	{
		for (unsigned int I = 0; I < count; I++)
			if (patterns[content[I].getPattern()]->name == pattern && patterns[content[I].getPattern()]->differentiator == differentiator &&
				content[I].len > maxLen)
			{
				maxLen = content[I].len;
				element = I | cMatchElement::patternFlag;
			}
	}
	return element;
}

// Longest match of (name, differentiator) whose len is strictly < the inbound
// maxLen.  Writes that length back to maxLen.  Same "*" / "X*" wildcards.
int cPatternMatchArray::queryPatternDiffLessThenLength(lpwstring pattern, lpwstring differentiator, int& maxLen)
{
	LFS
		int len = -1;
	int element = -1;
	if (differentiator.length() > 1 && differentiator[1] == u'*')
	{
		for (unsigned int I = 0; I < count; I++)
			if (patterns[content[I].getPattern()]->name == pattern && patterns[content[I].getPattern()]->differentiator[0] == differentiator[0] &&
				content[I].len < maxLen && content[I].len > len)
			{
				len = content[I].len;
				element = I | cMatchElement::patternFlag;
			}
	}
	else
	{
		for (unsigned int I = 0; I < count; I++)
			if (patterns[content[I].getPattern()]->name == pattern && patterns[content[I].getPattern()]->differentiator == differentiator &&
				content[I].len < maxLen && content[I].len > len)
			{
				len = content[I].len;
				element = I | cMatchElement::patternFlag;
			}
	}
	maxLen = len;
	return element;
}

// Longest match whose pattern has _QUESTION set.  Returns PMA index | patternFlag, or -1.
int cPatternMatchArray::queryQuestionFlagPattern()
{
	LFS
		int maxLen = -1;
	int element = -1;
	for (unsigned int I = 0; I < count; I++)
		if (patterns[content[I].getPattern()]->questionFlag && content[I].len > maxLen)
		{
			maxLen = content[I].len;
			element = I | cMatchElement::patternFlag;
		}
	return element;
}

// Smallest match length in this PMA, if it is > w; otherwise w+1.  Used to
// skip forward when no match covering more than `w` tokens starts here.
// Seeds the scan with INT_MIN.
int cPatternMatchArray::getNextPosition(int w)
{
	LFS
		int minPatternMatch = INT_MIN;
	for (unsigned int I = 0; I < count; I++)
		if (content[I].len < minPatternMatch)
			minPatternMatch = content[I].len;
	if (minPatternMatch != (INT_MIN) && minPatternMatch > w)
		return minPatternMatch;
	return w + 1;
}

// bsearch comparator: ascending pattern #, then ascending len.  Not a member;
// the commented trace used a `t` that is not in scope here.
int compare(cPatternMatchArray::tPatternMatch* pm1, cPatternMatchArray::tPatternMatch* pm2)
{
	DLFS
		//if (t.tracePatternElimination)
		//  lplog(u"    PMA comparing p=%d end=%d to p=%d end=%d",pm1->pattern,pm1->end,pm2->getParentPattern(),pm2->end);
		if (pm1->getPattern() < pm2->getPattern()) return -1;
	if (pm1->getPattern() > pm2->getPattern()) return 1;
	if (pm1->len < pm2->len) return -1;
	if (pm1->len > pm2->len) return 1;
	return 0;
}

// Binary search for exact (p, len).  Returns a pointer into content, or NULL.
// Relies on the array staying sorted by push_back_unique.
cPatternMatchArray::tPatternMatch* cPatternMatchArray::find(unsigned int p, short len)
{
	LFS
		tPatternMatch key;
	key.setPattern(p);
	key.len = len;
	//if (t.tracePatternElimination)
	//  lplog(u"    PMA searching for p=%d end=%d",p,end);
	return (tPatternMatch*)bsearch(&key, content, count, sizeof(*content), (int (*)(const void*, const void*))compare);
	//if (!result) return NULL;
	//while (result->pattern==p && result->end==end) result--;
	//return result;
}

// First slot not ordered before (p, end).  May point one-past-last (content+count)
// when every existing slot is < (p,end) — push_back_unique treats that as "insert here".
cPatternMatchArray::tPatternMatch* cPatternMatchArray::lower_bound(unsigned int p, short end)
{
	LFS
		int len = count, half;
	tPatternMatch* middle, * first = content;

	while (len > 0)
	{
		half = len >> 1;
		middle = first + half;
		if (p > middle->getPattern() || (p == middle->getPattern() && end > middle->len))
		{
			first = middle + 1;
			len = len - half - 1;
		}
		else
			len = half;
	}
	return first;
}

// Compact this PMA to winner slots only (memcpy down), clear WINNER_FLAG, and
// remap pemaByPatternEnd / pemaByChildPatternEnd through PEMA's wa[] map.
// maxMatch is the longest surviving len.  Returns true if more than one winner
// remains.  `position` is the source-m index, used only for the elimination log.
bool cPatternMatchArray::consolidateWinners(int lastPEMAConsolidationIndex, cPatternElementMatchArray& pema, int* wa, int position, int& maxMatch, sTrace& t)
{
	LFS
		int target = 0, numWinners = 0;
	maxMatch = 0;
	for (unsigned int J = 0; J < count; J++)
	{
		if (content[J].isWinner())
		{
			if (target != J)
				memcpy(content + target, content + J, sizeof(*content));
			maxMatch = max(maxMatch, (int)content[target].len); // batch B5: maxMatch is int&
			content[target].removeWinnerFlag();
			//if (content[target].maxWinner(lowestAverageCost,maxLACMatch))
			numWinners++;
			pema.getNextValidPosition(lastPEMAConsolidationIndex, wa, &content[target].pemaByPatternEnd, cPatternElementMatchArray::BY_PATTERN_END);
			pema.translate(lastPEMAConsolidationIndex, wa, &content[target].pemaByPatternEnd, cPatternElementMatchArray::BY_PATTERN_END);
			pema.getNextValidPosition(lastPEMAConsolidationIndex, wa, &content[target].pemaByChildPatternEnd, cPatternElementMatchArray::BY_CHILD_PATTERN_END);
			pema.translate(lastPEMAConsolidationIndex, wa, &content[target].pemaByChildPatternEnd, cPatternElementMatchArray::BY_CHILD_PATTERN_END);
			target++;
			if (t.tracePatternElimination)
				lplog(u"position %d:pma %d:Kept %s[%s](%d,%d) [winner]", position, J, patterns[content[J].getPattern()]->name.c_str(), patterns[content[J].getPattern()]->differentiator.c_str(), position, position + content[J].len);
		}
		else if (t.tracePatternElimination)
			lplog(u"position %d:pma %d:Eliminated %s[%s](%d,%d) [not winner]", position, J, patterns[content[J].getPattern()]->name.c_str(), patterns[content[J].getPattern()]->differentiator.c_str(), position, position + content[J].len);
	}
	if (t.tracePatternElimination)
		lplog(u"%d:PMA count reduced from %d to %d", position, count, target);
	count = target;
	return numWinners > 1;
}