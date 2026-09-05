/*
	patternElementMatchArray.cpp - document-wide PEMA storage, chain insert, winner pack

	Overview:
		Implements the single cSource::pema array.  Each slot is one element of one
		pattern match, threaded on four intrusive lists (by position, by
		(pattern,end), by child (pattern,end), and "next element of the same
		parent match").  push_back_unique inserts into the by-pattern-end chain
		sorted by descending begin; consolidateWinners builds a wa[] map of
		winner destinations, rewrites every chain index, and memcpy's survivors
		down so the array is dense again.

	Pipeline position:
		Stage 4 write path (fillPattern) and the post-winnow compact
		(eliminateLoserPatterns -> generateWinnerConsolidationArray /
		consolidateWinners).  Stages 5+ only walk the chains.

	Key entry points:
		- push_back() / push_back_unique() - allocate a slot and link it
		- generateWinnerConsolidationArray() / consolidateWinners() / translate()
		- getNextValidPosition / skipPastPositions / getNextValidByPosition -
		  walk past already-consolidated (previous sentence) indexes
		- ownedByOtherWinningPattern() / queryTag() / generatePEMACount()
		- tPatternElementMatch::getRole() / toText()

	Key data structures / globals:
		- content / count / allocated - slot 0 is reserved unused (collectTags
		  negates PEMA offsets)
		- wa[] - temporary "old index -> new index or -1" map, freed inside
		  consolidateWinners

	Notes / gotchas:
		- First push_back bumps count from 0 to 1 before writing, so the first
		  real entry is at index 1.
		- nextByPatternEnd < 0 is a circular-list back-pointer (-offset), not
		  "end of list" (-1 is the empty-head sentinel).
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
#include "profile.h"

// Empty PEMA.  content is NULL until the first push_back.
cPatternElementMatchArray::cPatternElementMatchArray()
{
	LFS
		count = 0;
	allocated = 0;
	content = NULL;
};

// Free the buffer (if any) and NULL content.
cPatternElementMatchArray::~cPatternElementMatchArray()
{
	LFS
		if (allocated) tfree(allocated * sizeof(*content), content);
	count = 0;
	allocated = 0;
	content = NULL;
}

// Drop every entry, free the buffer, and NULL content (unlike PMA::clear).
void cPatternElementMatchArray::clear(void)
{
	LFS
		if (allocated) tfree(allocated * sizeof(*content), content);
	allocated = 0;
	count = 0;
	content = NULL;
}

// Deep copy.  On tmalloc failure logs FATAL and returns with content==NULL
// but count/allocated still copied from rhs.
cPatternElementMatchArray::cPatternElementMatchArray(const cPatternElementMatchArray& rhs)
{
	LFS
		count = rhs.count;
	allocated = rhs.allocated;
	content = NULL;
	if (allocated)
	{
		content = (tPatternElementMatch*)tmalloc(allocated * sizeof(*content));
		if (!content)
		{
			lplog(LOG_FATAL_ERROR, u"OUT OF MEMORY (5)");
			return;
		}
		memcpy(content, rhs.content, count * sizeof(*content));
	}
}


// Write count then raw content bytes to a POSIX fd.  Always returns true.
bool cPatternElementMatchArray::write(IOHANDLE file)
{
	LFS
		::write(file, &count, sizeof(count));
	::write(file, content, count * sizeof(*content));
	return true;
}

// Write count + content to a POSIX fd.  Returns false if either write is short.
// Batch B5: was WriteFile(HANDLE), which shadowed the Win32 API of the same name and
// then called it via ::WriteFile; renamed to writeToFile so nothing here is named
// after an API that no longer exists.
bool cPatternElementMatchArray::writeToFile(int file)
{
	LFS
		if (::write(file, &count, sizeof(count)) != (ssize_t)sizeof(count)) return false;
	if (::write(file, content, count * sizeof(*content)) != (ssize_t)(count * sizeof(*content))) return false;
	return true;
}

// Read count then content from a POSIX fd.  Rejects count>1e6.  A zero-count
// PEMA still calls tmalloc(0); a NULL return is treated as failure.
bool cPatternElementMatchArray::read(IOHANDLE file)
{
	LFS
		if (::read(file, &count, sizeof(count)) < 0)
		{
			lplog(LOG_ERROR, u"read error!");
			return false;
		}
	allocated = count;
	if (count > 1000000)
	{
		lplog(LOG_ERROR, u"Illegal count of %d (>1000000) encountered!", count);
		return false; // extremely unlikely to have more than this # of matches
	}
	content = (tPatternElementMatch*)tmalloc(count * sizeof(*content));
	if (!content)
	{
		lplog();
		return false;
	}
	if (::read(file, content, count * sizeof(*content)) < 0)
	{
		lplog(LOG_ERROR, u"read error!");
		return false;
	}
	return true;
}

// Serialize into a memory image.  Fatals if the payload would exceed limit.
bool cPatternElementMatchArray::write(void* buffer, int& where, unsigned int limit)
{
	LFS
		if (!copy(buffer, count, where, limit)) return false;
	if (where + count * sizeof(*content) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (15)", limit);
	memcpy(((char*)buffer) + where, content, count * sizeof(*content));
	where += count * sizeof(*content);
	return true;
}

// Deserialize from a memory image.  On overflow sets count=0 and returns false
// (does not free a previous buffer).
bool cPatternElementMatchArray::read(char* buffer, int& where, unsigned int limit)
{
	LFS
		if (!copy(count, buffer, where, limit)) return false;
	if (where + count * sizeof(*content) > limit)
	{
		lplog(LOG_ERROR, u"Maximum read copy limit of %d bytes reached! (15)", limit);
		count = 0;
		return false;
	}
	allocated = count;
	content = (tPatternElementMatch*)tmalloc(count * sizeof(*content));
	if (!content)
	{
		lplog();
		return false;
	}
	memcpy(content, buffer + where, count * sizeof(*content));
	where += count * sizeof(*content);
	return true;
}

// Byte-compare content[0..count).
bool cPatternElementMatchArray::operator==(const cPatternElementMatchArray &other) const
{
	LFS
		if (count != other.count) return false;
	return memcmp(content, other.content, count * sizeof(*content)) == 0;
}

// Replace this buffer with a deep copy of rhs.
cPatternElementMatchArray& cPatternElementMatchArray::operator=(const cPatternElementMatchArray& rhs)
{
	LFS
	if (this == &rhs) return *this;
	if (allocated) tfree(allocated * sizeof(*content), content);
	count = rhs.count;
	allocated = rhs.allocated;
	content = NULL;
	if (allocated)
	{
		content = (tPatternElementMatch*)tmalloc(allocated * sizeof(*content));
		if (!content)
		{
			lplog(LOG_FATAL_ERROR, u"OUT OF MEMORY (7)");
			return *this;
		}
		memcpy(content, rhs.content, count * sizeof(*content));
	}
	return *this;
}

// Inverse of operator==.
bool cPatternElementMatchArray::operator!=(const cPatternElementMatchArray &other) const
{
	LFS
		if (count != other.count) return true;
	return memcmp(content, other.content, count * sizeof(*content)) != 0;
}

// Slot _P0.  INDEX_CHECK fatals on OOB (no dummy insert).
cPatternElementMatchArray::tPatternElementMatch& cPatternElementMatchArray::operator[](unsigned int _P0)
{
	LFS
#ifdef INDEX_CHECK
		static int catchError = 0;
	catchError++;
	if (_P0 >= count || _P0 < 0)
		lplog(LOG_FATAL_ERROR, u"Illegal reference (10) to element %d in an array with only %d elements! - %d", _P0, count, catchError);
#endif
	return (content[_P0]);
}

// Const [].  INDEX_CHECK throws if count==0 rather than fataling.
const cPatternElementMatchArray::tPatternElementMatch& cPatternElementMatchArray::operator[](unsigned int _P0) const
{
	LFS
#ifdef INDEX_CHECK
		if (_P0 >= count || _P0 < 0)
		{
			logCache = 0;
			lplog(u"Illegal reference (11) to element %d in an array with only %d elements!", _P0, count);
			if (count == 0) throw;
			return content[0];
		}
#endif
	return (content[_P0]);
}

// Append one slot.  The first call skips index 0 (count goes 0->1->2) because
// collectTags negates PEMA offsets.  Grows by allocationHint*10 (plus *50 extra
// when the hint is <1000).  Fatals if begin/end/iCost do not fit in a short.
// Returns the new index (count-1).
int cPatternElementMatchArray::push_back(int oCost, int iCost, unsigned int p, int begin, int end, int elementMatchedSubIndex,
	unsigned int cPatternElement, unsigned int patternElementIndex, int allocationHint)
{
	LFS
		if (!count) count++; // make sure no PEMA offset can be zero, because of a limitation in collectTags that uses the negation of PEMAPosition
	count++;
	if (allocated <= count)
	{
		int oldAllocated = allocated;
		if (!allocated)
			allocated = allocationHint * 10;
		else
		{
			allocated += (allocationHint * 10); // 3000000
			if (allocationHint < 1000) allocated += (allocationHint * 50);
			//lplog(u"PEMA memory reallocation %d-%d:%d:%d->%d",begin,end,allocationHint,oldAllocated,allocated);
		}
		content = (tPatternElementMatch*)trealloc(8, content, oldAllocated * sizeof(*content), allocated * sizeof(*content));
	}
	tPatternElementMatch* c = content + count - 1;
	c->setParentPattern(p);
	c->setOCost(oCost);
	c->setIncrementalCost(iCost);
	c->removeWinnerFlag();
	c->begin = begin;
	c->end = end;
	c->setElementAndIndex(cPatternElement, patternElementIndex);
	c->setElementMatchedSubIndex(elementMatchedSubIndex);
	c->nextByPosition = -1;
	c->nextByPatternEnd = -1;
	c->nextPatternElement = -1;
	c->nextByChildPatternEnd = -1;
	c->origin = -1;
	c->tempCost = 0;
	//c->sourcePosition=0; // BPM
	c->cumulativeDeltaCost = 0;
	if (begin<MIN_SIGNED_SHORT || begin>MAX_SIGNED_SHORT || end<MIN_SIGNED_SHORT || end>MAX_SIGNED_SHORT || iCost > MAX_SIGNED_SHORT)
		lplog(LOG_FATAL_ERROR, u"elements exceeded maximum values.");
	return count - 1;
}

// this sorts by descending begin
// cases:
// 1. *firstPosition==-1 (never been a match and *firstPosition points to the PMA pemaByPatternEnd)
// 2. *first Position=valid PEMA index
//   a. begin is > than any in list
//   b. begin is = to one in list
//     i. entry match found
//     ii. entry match not found
//   c. begin is < than any in list
// Walk the by-pattern-end chain from *firstPosition looking for an equivalent
// child (same begin, same element, same child form or same child rootPattern+len).
// On a hit, cheapen oCost/iCost if this call is cheaper and return that index
// (newElement=false).  Otherwise insert, splice into the descending-begin
// chain, and close the circular back-pointer (-PEMAOffset).  firstPosition /
// saveFirstPosition are raw pointers into content; they are converted to
// integer offsets across the realloc inside push_back.  Illegal child pattern
// # logs and returns 0 (which is also the reserved unused slot).
int cPatternElementMatchArray::push_back_unique(int* firstPosition, unsigned int position, int oCost, int iCost, unsigned int p, int begin, int end, int elementMatchedSubIndex,
	unsigned int cPatternElement, unsigned int patternElementIndex, int allocationHint, bool& newElement, bool POFlag)
{
	LFS
		bool isPattern;
	unsigned int referredPattern = -1, endPoint = -1;
	if (isPattern = (elementMatchedSubIndex & cMatchElement::patternFlag) == cMatchElement::patternFlag)
	{
		if (PATMASK(elementMatchedSubIndex) >= patterns.size())
		{
			lplog(LOG_ERROR, u"%d:FATAL ERROR:Illegal pattern # reference %d at p=%d begin=%d end=%d elementMatchedSubIndex=%d",
				position, PATMASK(elementMatchedSubIndex), p, begin, end, elementMatchedSubIndex);
			return newElement = false;
		}
		referredPattern = patterns[PATMASK(elementMatchedSubIndex)]->rootPattern;
		endPoint = ENDMASK(elementMatchedSubIndex);
	}
	// set pemaByPatternEnd and nextByPatternEnd
	int* saveFirstPosition = firstPosition;
	bool savePOFlag = POFlag; // for saveFirstPosition
	while (true)
	{
		int nextByPatternEnd = *firstPosition;
		tPatternElementMatch* c = content + nextByPatternEnd;
		if (nextByPatternEnd < 0 || c->begin < begin) break;
		if (c->begin == begin && c->getElement() == cPatternElement &&
			((isPattern && c->isChildPattern() && patterns[c->getChildPattern()]->rootPattern == referredPattern && c->getChildLen() == endPoint) ||
				(!isPattern && c->getChildForm() == elementMatchedSubIndex)))
		{
			if (c->getOCost() > oCost)
			{
#ifdef LOG_PATTERN_COST_CHECK
				if (isPattern)
					lplog(u"%d:%s[%s](%d,%d) %s[%s](%d,%d) element #%d PEMA cost reduced from %d to %d (child pattern)",
						position, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position + begin, position + end,
						patterns[c->getChildPattern()]->name.c_str(), patterns[c->getChildPattern()]->differentiator.c_str(), position, position + c->getChildLen(),
						c - content, c->getOCost(), oCost);
				else
					lplog(u"%d:%s[%s](%d,%d) PEMA cost reduced from %d to %d (child form)",
						position, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position + begin, position + end,
						c->getOCost(), oCost);
#endif
				c->setOCost(oCost);
			}
			if (c->getIncrementalCost() > iCost)
			{
#ifdef LOG_PATTERN_COST_CHECK
				if (isPattern)
					lplog(u"%d:%s[%s](%d,%d) %s[%s](%d,%d) element #%d PEMA ICost reduced from %d to %d (child pattern)",
						position, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position + begin, position + end,
						patterns[c->getChildPattern()]->name.c_str(), patterns[c->getChildPattern()]->differentiator.c_str(), position, position + c->getChildLen(),
						c - content, c->iCost, iCost);
				else
					lplog(u"%d:%s[%s](%d,%d) PEMA ICost reduced from %d to %d (child form)",
						position, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position + begin, position + end,
						c->iCost, iCost);
#endif
				c->setIncrementalCost(iCost);
			}
			patterns[p]->emi++;
			newElement = false;
			return nextByPatternEnd;
		}
		POFlag = true;
		firstPosition = &c->nextByPatternEnd;
	}
	int keepOffset = 0, keepFirstOffset = 0;
	if (savePOFlag) keepFirstOffset = (int)(saveFirstPosition - ((int*)content));
	if (POFlag)     keepOffset = (int)(firstPosition - ((int*)content));
	int PEMAOffset = push_back(oCost, iCost, p, begin, end, elementMatchedSubIndex, cPatternElement, patternElementIndex, allocationHint);
	//  content[PEMAOffset].sourcePosition=position; // for CHECKPEER BPM
	if (savePOFlag) saveFirstPosition = ((int*)content) + keepFirstOffset;
	if (POFlag)     firstPosition = ((int*)content) + keepOffset;
#ifdef LOG_PATTERN_COST_CHECK
	if (*firstPosition >= 0)
		lplog(u"%d:Extended patternEnd %s[%s](%d,%d)->%s[%s](%d,%d) chain link %d->%d",
			position,
			patterns[content[PEMAOffset].getPattern()]->name.c_str(), patterns[content[PEMAOffset].getPattern()]->differentiator.c_str(),
			position + content[PEMAOffset].begin, position + content[PEMAOffset].end,
			patterns[content[*firstPosition].getPattern()]->name.c_str(), patterns[content[*firstPosition].getPattern()]->differentiator.c_str(),
			position + content[*firstPosition].begin, position + content[*firstPosition].end,
			PEMAOffset, *firstPosition);
	else
		lplog(u"%d:Created patternEnd %s[%s](%d,%d) child %s[%s](%d,%d) chain link %d->%d",
			position,
			patterns[content[PEMAOffset].getPattern()]->name.c_str(), patterns[content[PEMAOffset].getPattern()]->differentiator.c_str(),
			position + content[PEMAOffset].begin, position + content[PEMAOffset].end,
			patterns[content[PEMAOffset].getChildPattern()]->name.c_str(), patterns[content[PEMAOffset].getChildPattern()]->differentiator.c_str(),
			position, position + content[PEMAOffset].getChildLen(),
			PEMAOffset, *firstPosition);
#endif
	content[PEMAOffset].nextByPatternEnd = (*firstPosition == -1) ? -PEMAOffset : *firstPosition;
	newElement = true;
	*firstPosition = PEMAOffset;
	if (firstPosition == saveFirstPosition && content[PEMAOffset].nextByPatternEnd != -PEMAOffset)
	{
		int p2;
		for (p2 = *saveFirstPosition; content[p2].nextByPatternEnd >= 0; p2 = content[p2].nextByPatternEnd);
#ifdef LOG_PATTERN_COST_CHECK
		lplog(u"Changing last position in chain from %d to %d.", content[p2].nextByPatternEnd, -PEMAOffset);
#endif
		content[p2].nextByPatternEnd = -PEMAOffset; // this is to create a circular list
	}
	/* check chain */
	/*
	int p2,len=0;
	char temp[1024];
	for (p2=*saveFirstPosition; p2>=0; p2=content[p2].nextByPatternEnd)
	len+=sprintf(temp+len,"%d ",p2);
	len+=sprintf(temp+len,"%d ",p2);
	temp[len]=0;
	if (-p2!=*saveFirstPosition)
	lplog(u"ERROR IN CHAIN %s!",temp);
	*/
	return PEMAOffset;
}

// Advance nextPosition along nextByPosition until it is either -1 or a winner
// at or after lastPEMAConsolidationIndex (wa[i-last]!=-1).  Recurses once if
// the walk skipped a block of already-consolidated (previous sentence) slots.
int cPatternElementMatchArray::getNextValidByPosition(int lastPEMAConsolidationIndex, int* wa, int& nextPosition)
{
	LFS
		if (nextPosition < lastPEMAConsolidationIndex && nextPosition >= 0)
		{
			int fPosition = nextPosition, lastPosition = fPosition;
			while (fPosition < lastPEMAConsolidationIndex && fPosition >= 0)
				fPosition = content[lastPosition = fPosition].nextByPosition;
			if (fPosition >= 0)
				return getNextValidByPosition(lastPEMAConsolidationIndex, wa, content[lastPosition].nextByPosition);
		}
	while (nextPosition >= lastPEMAConsolidationIndex && wa[nextPosition - lastPEMAConsolidationIndex] == -1)
		nextPosition = content[nextPosition].nextByPosition;
	return nextPosition;
}

// Debug: fatal if any chain index is >= count.
void cPatternElementMatchArray::check(void)
{
	LFS
		for (unsigned int I = 0; I < count; I++)
			if (content[I].nextByPosition >= (signed)count || content[I].nextByPatternEnd >= (signed)count || content[I].nextByChildPatternEnd >= (signed)count)
				lplog(LOG_FATAL_ERROR, u"FATAL!");
}

// If *nextPosition still points into the already-consolidated prefix, walk
// that chain until it lands on an index >= lastPEMAConsolidationIndex (or
// goes negative).  nextPosition is a pointer-to-slot-field so the caller can
// then rewrite that field.
void cPatternElementMatchArray::skipPastPositions(int lastPEMAConsolidationIndex, int*& nextPosition, enum chainType cType)
{
	LFS
		// skip positions established with last sentence
		if (*nextPosition < lastPEMAConsolidationIndex && *nextPosition >= 0)
		{
			int fPosition = *nextPosition, lastPosition = fPosition;
			while (fPosition < lastPEMAConsolidationIndex && fPosition >= 0)
				switch (cType)
				{
				case BY_POSITION:           fPosition = content[lastPosition = fPosition].nextByPosition; break;
				case BY_PATTERN_END:        fPosition = content[lastPosition = fPosition].nextByPatternEnd; break;
				case BY_CHILD_PATTERN_END:  fPosition = content[lastPosition = fPosition].nextByChildPatternEnd; break;
				}
			if (fPosition >= 0)
				switch (cType)
				{
				case BY_POSITION:           nextPosition = &content[lastPosition].nextByPosition; break;
				case BY_PATTERN_END:        nextPosition = &content[lastPosition].nextByPatternEnd; break;
				case BY_CHILD_PATTERN_END:  nextPosition = &content[lastPosition].nextByChildPatternEnd; break;
				}
		}
}

// skipPastPositions, then walk the chosen chain while wa[index-last]==-1
// (eliminated).  Leaves *nextPosition at the next surviving slot or a
// negative sentinel.
void cPatternElementMatchArray::getNextValidPosition(int lastPEMAConsolidationIndex, int* wa, int* nextPosition, enum chainType cType)
{
	LFS
		skipPastPositions(lastPEMAConsolidationIndex, nextPosition, cType);
	while (*nextPosition >= lastPEMAConsolidationIndex && (unsigned int)*nextPosition < count && wa[*nextPosition - lastPEMAConsolidationIndex] == -1)
	{
		switch (cType)
		{
		case BY_POSITION:           *nextPosition = content[*nextPosition].nextByPosition; break;
		case BY_PATTERN_END:        *nextPosition = content[*nextPosition].nextByPatternEnd; break;
		case BY_CHILD_PATTERN_END:  *nextPosition = content[*nextPosition].nextByChildPatternEnd; break;
		}
	}
}

// Remap *position through wa[] after skipPastPositions.  Negative values other
// than -1 are circular back-pointers: un-negate, remap, re-negate.  A
// non-winner that is not a looped pointer is logged as an error and set to -1.
void cPatternElementMatchArray::translate(int lastPEMAConsolidationIndex, int* wa, int* position, enum chainType cType)
{
	LFS
		bool translateLoopedPositions = false;
	if (translateLoopedPositions = *position < 0 && *position != -1)
		*position = -*position;
	skipPastPositions(lastPEMAConsolidationIndex, position, cType);
	if (*position < 0 || *position < lastPEMAConsolidationIndex) return;
	if (wa[*position - lastPEMAConsolidationIndex] == -1)
	{
		if (!translateLoopedPositions)
			lplog(LOG_ERROR, u"PEMAIndex %d referenced but not winner.", *position);
		*position = -1;
		return;
	}
	*position = wa[*position - lastPEMAConsolidationIndex];
	if (translateLoopedPositions)
		*position = -*position;
}

// Allocate wa[count-last] and fill it with the destination index of each
// winner (or -1).  Then rewrite each winner's five chain fields to the next
// still-valid slot (not yet the final compacted index — translate() does that).
// No-op if there is nothing past lastPEMAConsolidationIndex.
void cPatternElementMatchArray::generateWinnerConsolidationArray(int lastPEMAConsolidationIndex, int*& wa, int& numWinners)
{
	LFS
		if ((signed)count - lastPEMAConsolidationIndex <= 0) return;
	wa = (int*)tmalloc((count - lastPEMAConsolidationIndex) * sizeof(*wa));
	numWinners = 0;
	for (unsigned int J = lastPEMAConsolidationIndex; J < count; J++)
		wa[J - lastPEMAConsolidationIndex] = (content[J].isWinner()) ? lastPEMAConsolidationIndex + numWinners++ : -1;
	for (unsigned int J = lastPEMAConsolidationIndex; J < count; J++)
	{
		if (wa[J - lastPEMAConsolidationIndex] < 0) continue;
		getNextValidByPosition(lastPEMAConsolidationIndex, wa, content[J].nextByPosition);
		getNextValidPosition(lastPEMAConsolidationIndex, wa, &content[J].nextByPatternEnd, BY_PATTERN_END);
		getNextValidPosition(lastPEMAConsolidationIndex, wa, &content[J].nextByChildPatternEnd, BY_CHILD_PATTERN_END);
		getNextValidPosition(lastPEMAConsolidationIndex, wa, &content[J].nextPatternElement, BY_PATTERN_END); // this is correct
		getNextValidPosition(lastPEMAConsolidationIndex, wa, &content[J].origin, BY_PATTERN_END);
	}
}

// memcpy each winner onto wa[i], incrementUse() its pattern alternative,
// translate() its chain fields, then tfree wa and shrink count to
// last+numWinners.  Returns true if more than one winner remains.
bool cPatternElementMatchArray::consolidateWinners(int lastPEMAConsolidationIndex, int* wa, int numWinners, sTrace& t)
{
	LFS
		if (lastPEMAConsolidationIndex >= (signed)count) return true;
	// consolidate winners
	for (unsigned int I = lastPEMAConsolidationIndex; I < count; I++)
	{
		if (wa[I - lastPEMAConsolidationIndex] >= 0)
		{
			patterns[content[I].getParentPattern()]->incrementUse(content[I].__patternElement, content[I].__patternElementIndex, content[I].isChildPattern());
			translate(lastPEMAConsolidationIndex, wa, &content[I].nextByPosition, BY_POSITION);
			translate(lastPEMAConsolidationIndex, wa, &content[I].nextByPatternEnd, BY_PATTERN_END);
			translate(lastPEMAConsolidationIndex, wa, &content[I].nextByChildPatternEnd, BY_CHILD_PATTERN_END);
			translate(lastPEMAConsolidationIndex, wa, &content[I].nextPatternElement, BY_PATTERN_END);
			translate(lastPEMAConsolidationIndex, wa, &content[I].origin, BY_PATTERN_END);
			if (wa[I - lastPEMAConsolidationIndex] != I)
				memcpy(content + wa[I - lastPEMAConsolidationIndex], content + I, sizeof(*content));
			content[wa[I - lastPEMAConsolidationIndex]].removeWinnerFlag();
		}
	}
	if (wa) tfree((count - lastPEMAConsolidationIndex) * sizeof(*wa), wa);
	if (t.tracePatternElimination)
		lplog(u"PEMA reduced from %d to count %d", count, numWinners);
	count = lastPEMAConsolidationIndex + numWinners;
	return numWinners > 1;
}

// Length of the nextByPosition chain starting at nextPosition.  
int cPatternElementMatchArray::generatePEMACount(int nextPosition)
{
	LFS
		int I = 0;
	for (; nextPosition != -1; I++, nextPosition = content[nextPosition].nextByPosition)
		if (nextPosition < 0 || nextPosition >= (signed)count)
			lplog(LOG_FATAL_ERROR, u"Incorrect PEMA Position %d", nextPosition);
	return I;
}

// First slot on the nextByPosition chain whose element carries `tag`, or -1.
int cPatternElementMatchArray::queryTag(int nextPosition, int tag)
{
	LFS
		for (; nextPosition != -1; nextPosition = content[nextPosition].nextByPosition)
			if (content[nextPosition].hasTag(tag))
				return nextPosition;
	return -1;
}

// True if some winner on the nextByPosition chain other than parentPEMAPosition
// already claims a child whose rootPattern and childLen equal (p, len).
bool cPatternElementMatchArray::ownedByOtherWinningPattern(int parentPEMAPosition, int nextPosition, int p, int len)
{
	LFS
		int rootPattern = patterns[p]->rootPattern;
	for (; nextPosition != -1; nextPosition = content[nextPosition].nextByPosition)
	{
		if (nextPosition != parentPEMAPosition &&
			content[nextPosition].isWinner() &&
			content[nextPosition].isChildPattern() &&
			patterns[content[nextPosition].getChildPattern()]->rootPattern == rootPattern &&
			content[nextPosition].getChildLen() == len)
			return true;
	}
	return false;
}

// Like ownedByOtherWinningPattern but any owner (winner or not) counts, and
// there is no parent-slot exclusion.
bool cPatternElementMatchArray::ownedByOtherPattern(int nextPosition, int p, int len)
{
	LFS
		int rootPattern = patterns[p]->rootPattern;
	for (; nextPosition != -1; nextPosition = content[nextPosition].nextByPosition)
	{
		if (content[nextPosition].isChildPattern() &&
			patterns[content[nextPosition].getChildPattern()]->rootPattern == rootPattern &&
			content[nextPosition].getChildLen() == len)
			return true;
	}
	return false;
}

// Map this element's role tags onto the relation-role bitfield.  tagRole is
// an out-param for pattern-level MPLURAL / MNOUN / S_IN_REL; the return is
// the child-tag roles (HAIL/SUBJECT/OBJECT/...).  Walks roleTagSet via
// elementHasTagInSet.  Role tags are required to live on the child, not on
// the parent pattern as a whole.
int64_t cPatternElementMatchArray::tPatternElementMatch::getRole(int64_t& tagRole)
{
	LFS
		// role tags must ONLY be child tags, not tags assigned to the whole pattern
		int tag;
	int64_t childTagRole = 0;
	unsigned int tagNumBySet = 0;
	tagRole = (patterns[getParentPattern()]->tags.find(MPLURAL_TAG) != patterns[getParentPattern()]->tags.end()) ? MPLURAL_ROLE : 0;
	// a noun could have multiple nouns in it but still be singular - NOUN "O"
	tagRole += (patterns[getParentPattern()]->tags.find(MNOUN_TAG) != patterns[getParentPattern()]->tags.end()) ? MNOUN_ROLE : 0;
	tagRole += (patterns[getParentPattern()]->tags.find(SENTENCE_IN_REL_TAG) != patterns[getParentPattern()]->tags.end()) ? SENTENCE_IN_REL_ROLE : 0;
	while ((tag = patterns[getParentPattern()]->elementHasTagInSet(__patternElement, __patternElementIndex, roleTagSet, tagNumBySet, isChildPattern())) >= 0)
	{
		if (tag == HAIL_TAG) childTagRole |= HAIL_ROLE;
		if (tag == SUBJECT_TAG) childTagRole |= SUBJECT_ROLE;
		if (tag == OBJECT_TAG) childTagRole |= OBJECT_ROLE;
		if (tag == SUBOBJECT_TAG) childTagRole |= SUBOBJECT_ROLE;
		if (tag == REOBJECT_TAG) childTagRole |= RE_OBJECT_ROLE;
		if (tag == IOBJECT_TAG) childTagRole |= IOBJECT_ROLE;
		if (tag == PREP_OBJECT_TAG) childTagRole |= PREP_OBJECT_ROLE;
	}
	return childTagRole;
}

// Format "PARENT[diff](absBegin,absEnd) child CHILD[*](...)" or "... form NAME"
// into caller-provided temp.  No bound is passed to lp_wsprintf.
// Batch B5: tempCount added -- temp is a pointer parameter, so the bounded
// lp_wsprintf cannot deduce its size; every caller passes a 1024-element buffer.
lpchar_t* cPatternElementMatchArray::tPatternElementMatch::toText(unsigned int position, lpchar_t* temp, size_t tempCount, vector <cWordMatch>& m)
{
	LFS
		int len = lp_snprintf(temp, tempCount, u"%s[%s](%u,%u) child",
			patterns[getParentPattern()]->name.c_str(), patterns[getParentPattern()]->differentiator.c_str(), position + begin, position + end);
	if (isChildPattern())
		len += ((size_t)len < tempCount) ? lp_snprintf(temp + len, tempCount - len, u" %s[*](%u,%u)", patterns[getChildPattern()]->name.c_str(), position, position + getChildLen()) : 0;
	else
		len += ((size_t)len < tempCount) ? lp_snprintf(temp + len, tempCount - len, u" form %s", Forms[m[position].getFormNum(getChildForm())]->shortName.c_str()) : 0;
	return temp;
}
