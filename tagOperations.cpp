/*
	tagOperations.cpp - Collect, compare, and resolve pattern-match tag sets

	Overview:
		Walks the PEMA (pattern-element match array) tree for a desired tag set
		(nAgree, prep, verb, …), collecting every combination of tagged child
		patterns/forms into vector<vector<cTagLocation>>. Handles blocking
		patterns, optional focus positions, a time/count cap (COLLECT_TAGS_TIME_LIMIT
		/ MAX_TAGSETS), and a memo map keyed by child PEMA position. Also contains
		helpers that pick VERB/V_OBJECT tags, resolve a tag to a word class (PPN,
		NUM, DATE, …), and apply the proper-noun determiner cost.

	Pipeline position:
		After parse / pattern matching. Used by agreement, object identification,
		speaker resolution, and BNC cost evaluation whenever a pattern's tagged
		pieces must be enumerated.

	Key entry points:
		- collectTags / startCollectTags / startCollectTagsFromTag - the collector
		- replicate - cartesian-product a child's tagSets onto the parent
		- getVerb / getIVerb / resolveTag / resolveObjectTagBeforeObjectResolution
		- resolveToClass / fullyResolveToClass - map a position to a class token

	Key data structures / globals:
		- desiredTagSetNum, blocking, focused, exitTags, beginTime, timerForExit -
		  collector state on cSource (not thread-safe)
		- secondaryPEMAPositions - PEMA indexes pushed at recursionLevel==0 so
		  later cost code can attribute a tagSet to an element

	Notes / gotchas:
		compareTagSets copies both vectors by value then sort()s them.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include "profile.h"

// True if any collectTagsFocusPositions index lies in [begin, end).
bool cSource::tagInFocus(int begin, int end)
{
	LFS
		for (unsigned I = 0; I < collectTagsFocusPositions.size(); I++)
			if (begin <= collectTagsFocusPositions[I] && end > collectTagsFocusPositions[I])
				return true;
	return false;
}

#if defined NDEBUG
#define COLLECT_TAGS_TIME_LIMIT 70
#else
#define COLLECT_TAGS_TIME_LIMIT 1000
#endif


// For each child tagSet, append it to tagSet, recurse collectTags, then erase the append.
// If childTagSets is empty, still recurses once (parent-only continuation). Returns child count.
int cSource::replicate(int recursionLevel, int PEMAPosition, int position, vector <cTagLocation>& tagSet, vector < vector <cTagLocation> >& childTagSets, vector < vector <cTagLocation> >& tagSets, unordered_map <int, vector < vector <cTagLocation> > >& TagSetMap)
{
	LFS
		size_t originalTagSetSize = tagSet.size();
	for (unsigned I = 0; I < childTagSets.size(); I++)
	{
		tagSet.insert(tagSet.end(), childTagSets[I].begin(), childTagSets[I].end());
		collectTags(recursionLevel, PEMAPosition, position, tagSet, tagSets, TagSetMap);
		tagSet.erase(tagSet.begin() + originalTagSetSize, tagSet.end());
	}
	if (!childTagSets.size())
	{
		collectTags(recursionLevel, PEMAPosition, position, tagSet, tagSets, TagSetMap);
		tagSet.erase(tagSet.begin() + originalTagSetSize, tagSet.end());
	}
	return childTagSets.size();
}

// Element-wise equality of two tag location vectors (same size and each cTagLocation ==).
bool tagSetSame(vector <cTagLocation>& tagSet, vector <cTagLocation>& tagSetNew)
{
	LFS
		if (tagSet.size() != tagSetNew.size()) return false;
	for (unsigned int I = 0; I < tagSet.size(); I++)
		if (tagSet[I] != tagSetNew[I])
			return false;
	return true;
}

// Recursively collect tag combinations from PEMAPosition. PEMAPosition < 0 is the leaf:
// push tagSet onto tagSets (unless a recent duplicate) and maybe record secondaryPEMAPositions.
// Otherwise walk sibling PEMA rows for this pattern/end, emit tags, and either block,
// skip unfocused children, or recurse into the child pattern (memoized in TagSetMap).
// Returns tagSets.size(). Sets exitTags on timeout or MAX_TAGSETS.
int cSource::collectTags(int recursionLevel, int PEMAPosition, int position, vector <cTagLocation>& tagSet, vector < vector <cTagLocation> >& tagSets, unordered_map <int, vector < vector <cTagLocation> > >& TagSetMap)
{
	LFS
		if (PEMAPosition < 0)
		{
			if (timerForExit++ == 31 && !debugTrace.tracePatternElimination && (clock() - beginTime) > COLLECT_TAGS_TIME_LIMIT)
				exitTags = true;
			bool duplicate = false;
			size_t duplicateTS = tagSets.size();
			if (recursionLevel)
				for (size_t ts = (tagSets.size() > 10) ? tagSets.size() - 10 : 0; ts < tagSets.size() && !duplicate; ts++)
					if (duplicate = tagSetSame(tagSet, tagSets[ts]))
						duplicateTS = ts;
			// !duplicate && tagSet.size() if removing empty tagsets from top level EMPTAG
			if (recursionLevel == 0 && secondaryPEMAPositions.size() && secondaryPEMAPositions[secondaryPEMAPositions.size() - 1].getTagSet() != tagSets.size())
				secondaryPEMAPositions.push_back(cCostPatternElementByTagSet(position, -PEMAPosition, -1, duplicateTS, pema[-PEMAPosition].getElement()));
			if (!duplicate) // Eliminate empty tagsets? (recursionLevel || tagSet.size()) && (EMPTAG)
			{
				tagSets.push_back(tagSet);
				exitTags = (tagSets.size() > MAX_TAGSETS);
			}
			// this alternate section was tried and this didn't work properly
			// if (!recursionLevel && (!tagSet.size() || duplicate)) (EMPTAG)
			//{
			// erase all secondaryPEMAPositions referring to an empty tagSet
			//int I;
			//for (I=secondaryPEMAPositions.size()-1; I>=0 && secondaryPEMAPositions[I].tagSet==tagSets.size(); I--);
			//I++;
			//if (I>=0 && I<(signed)secondaryPEMAPositions.size())
			//  secondaryPEMAPositions.erase(secondaryPEMAPositions.begin()+I,secondaryPEMAPositions.end());
			//}
			// end eliminate empty tagsets section
			return tagSets.size();
		}
	int pattern = pema[PEMAPosition].getParentPattern(), cPatternElement = pema[PEMAPosition].getElement();
	int begin = pema[PEMAPosition].begin + position, end = pema[PEMAPosition].end + position;
	size_t relativeEnd = end - position, originalTagSetSize = tagSet.size();
	int relativeBegin = begin - position;
	cPatternElementMatchArray::tPatternElementMatch* pem = pema.begin() + PEMAPosition;
	if (debugTrace.traceTags)
		lplog(u"%*s%d:%06d %s[%s](%d,%d) element #%d #tags collected=%d", recursionLevel * 2, " ", position, PEMAPosition,
			patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end, cPatternElement, tagSet.size());
	tagSet.reserve(6);
	for (; PEMAPosition >= 0 && pem->getParentPattern() == pattern && pem->end == relativeEnd && !exitTags; PEMAPosition = pem->nextByPatternEnd, pem = pema.begin() + PEMAPosition)
		if (pem->begin == relativeBegin)
		{
			if (recursionLevel == 0) // pushing down PEMAPositions to gain more accuracy
				secondaryPEMAPositions.push_back(cCostPatternElementByTagSet(position, PEMAPosition, -1, tagSets.size(), cPatternElement));
			int nextPEMAPosition, nextPosition = position, tag;
			if ((nextPEMAPosition = pem->nextPatternElement) >= 0)
				nextPosition = begin - pema[nextPEMAPosition].begin; // update next position
			else
				nextPEMAPosition = -PEMAPosition;
			if (pem->isChildPattern())
			{
				// must match to root pattern because of compression done in cPatternElementMatchArray::push_back_unique
				unsigned int childEnd = pem->getChildLen(), beginTag = 0;
				while ((tag = patterns[pattern]->elementHasTagInSet(pem->getElement(), pem->getElementIndex(), desiredTagSetNum, beginTag, true)) >= 0)
				{
					if (debugTrace.traceTags)
						lplog(u"%*s%d:TAG %s FOUND %s[%s](%d,%d) %s[*]", recursionLevel * 2, " ", position, patternTagStrings[tag].c_str(),
							patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end, patterns[pem->getChildPattern()]->name.c_str());
					tagSet.push_back(cTagLocation(tag, pem->getChildPattern(), pattern, pem->getElement(), position, childEnd, -PEMAPosition, true));
				}
				// element blocks all descendants.  Continue with next element.
				if (blocking && patterns[pattern]->stopDescendingTagSearch(pem->getElement(), pem->getElementIndex(), pem->isChildPattern()))
				{
					if (debugTrace.traceTags)
						lplog(u"%*s%d:%s[%s](%d,%d) BLOCKED element #%d index #%d", (recursionLevel + 1) * 2, " ",
							position, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end,
							pem->getElement(), pem->getElementIndex());
					//if (recursionLevel==0)
					//    secondaryPEMAPositions.push_back(cCostPatternElementByTagSet(position,PEMAPosition,-1,tagSets.size(),patternElement));
					if (!exitTags)
						collectTags(recursionLevel, nextPEMAPosition, nextPosition, tagSet, tagSets, TagSetMap);
					tagSet.erase(tagSet.begin() + originalTagSetSize, tagSet.end());
					continue;
				}
				size_t originalTagSetSizePlusParentsTags = tagSet.size();
				for (int p = patterns[pem->getChildPattern()]->rootPattern; p >= 0 && !exitTags; p = patterns[p]->nextRoot)
				{
					if (p == pattern || !m[position].patterns.isSet(p)) continue;
					cPatternMatchArray::tPatternMatch* pma = m[position].pma.find(p, childEnd);
					if (pma == NULL) continue;
					int childPEMAPosition = pma->pemaByPatternEnd;
					cPatternElementMatchArray::tPatternElementMatch* childPem = pema.begin() + childPEMAPosition;
					for (; childPEMAPosition >= 0 && childPem->getParentPattern() == p && childPem->end == childEnd; childPEMAPosition = childPem->nextByPatternEnd, childPem = pema.begin() + childPEMAPosition)
						if (!childPem->begin) break;
					if (childPEMAPosition < 0 || childPem->getParentPattern() != p || childPem->end != childEnd || childPem->begin) continue;
					//if (recursionLevel==0) // pushing down PEMAPositions to gain more accuracy
					//  secondaryPEMAPositions.push_back(cCostPatternElementByTagSet(position,PEMAPosition,childPEMAPosition,tagSets.size(),patternElement));
					beginTag = 0;
					bool found = false;
					while ((tag = patterns[p]->hasTagInSet(desiredTagSetNum, beginTag)) >= 0)
					{
						found = true;
						if (debugTrace.traceTags)
							lplog(u"%*s%d:TAG %s FOUND (pattern) %s[%s](%d,%d)", recursionLevel * 2, " ", position, patternTagStrings[tag].c_str(),
								patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position, position + childEnd);
						tagSet.push_back(cTagLocation(tag, p, pattern, pem->getElement(), position, childEnd, childPEMAPosition, true));
					}
					if (found)
					{
						tagSet.erase(tagSet.begin() + originalTagSetSizePlusParentsTags, tagSet.end());
						continue;
					}
					// child pattern blocks all descendants.  This is only executed in the first element of the pattern to
					// grab all the tags which are not blocked.
					if (focused && !tagInFocus(position, position + childEnd))
						collectTags(recursionLevel, nextPEMAPosition, nextPosition, tagSet, tagSets, TagSetMap);
					else if (blocking && patterns[p]->blockDescendants)
					{
						if (debugTrace.traceTags)
							lplog(u"%*s%d:%s[%s](%d,%d) BLOCKED", (recursionLevel + 1) * 2, " ",
								position, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position, position + childEnd);
						if (!exitTags)
							collectTags(recursionLevel, nextPEMAPosition, nextPosition, tagSet, tagSets, TagSetMap);
					}
					else if (!(patterns[p]->includesOneOfTagSet & ((int64_t)1 << desiredTagSetNum)))
					{
						if (debugTrace.traceTags)
							lplog(u"%*s%d:%06d %s[%s](%d,%d) (not in tag set %s)", (recursionLevel + 1) * 2, " ",
								position, childPEMAPosition, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position, position + childEnd,
								desiredTagSets[desiredTagSetNum].name.c_str());
						if (!exitTags)
							collectTags(recursionLevel, nextPEMAPosition, nextPosition, tagSet, tagSets, TagSetMap);
					}
					else
					{
						if (TagSetMap.find(childPEMAPosition) == TagSetMap.end())
						{
							vector <cTagLocation> childTagSet;
							vector < vector <cTagLocation> > childTagSets;
							if (!exitTags)
								collectTags(recursionLevel + 2, childPEMAPosition, position, childTagSet, childTagSets, TagSetMap);
							TagSetMap[childPEMAPosition] = childTagSets;
							if (debugTrace.traceTags)
							{
								int nonNullTagSets = 0;
								for (unsigned int K = 0; K < childTagSets.size(); K++)
									if (childTagSets[K].size())
										nonNullTagSets++;
								lplog(u"%*s%d:childPEMAPosition %d resulted in %d tagSets (%d nonNull)", recursionLevel * 2, " ", position, childPEMAPosition,
									childTagSets.size(), nonNullTagSets);
							}
						}
						replicate(recursionLevel, nextPEMAPosition, nextPosition, tagSet, TagSetMap[childPEMAPosition], tagSets, TagSetMap);
					}
					tagSet.erase(tagSet.begin() + originalTagSetSizePlusParentsTags, tagSet.end());
				}
			}
			else
			{
				unsigned int beginTag = 0;
				while ((tag = patterns[pattern]->elementHasTagInSet(pem->getElement(), pem->getElementIndex(), desiredTagSetNum, beginTag, false)) >= 0)
				{
					if (debugTrace.traceTags)
						lplog(u"%*s%d:TAG %s FOUND form %s", recursionLevel * 2, " ", position, patternTagStrings[tag].c_str(),
							Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str());
					tagSet.push_back(cTagLocation(tag, m[position].getFormNum(pem->getChildForm()), pattern, pem->getElement(), position, 1, PEMAPosition, false));
				}
				if (debugTrace.traceTags)
					lplog(u"%*s%d:%s[%s](%d,%d) form %s element #%d", (recursionLevel + 1) * 2, " ",
						position, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end,
						Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str(),
						pem->getElement());
				//if (recursionLevel==0) // pushing down PEMAPositions to gain more accuracy
				//    secondaryPEMAPositions.push_back(cCostPatternElementByTagSet(position,PEMAPosition,-1,tagSets.size(),patternElement));
				if (!exitTags)
					collectTags(recursionLevel, nextPEMAPosition, nextPosition, tagSet, tagSets, TagSetMap);
			}
			tagSet.erase(tagSet.begin() + originalTagSetSize, tagSet.end());
		}
	return tagSets.size();
}

// Picks the verb-side tag: last V_OBJECT constrained to VERB, else last V_AGREE. Writes its
// index into tag. Returns false if there is no VERB or neither V_OBJECT nor V_AGREE.
bool cSource::getVerb(vector <cTagLocation>& tagSet, int& tag)
{
	LFS
		int nextVerbTag = -1, nextVObjectTag = -1, nextVAgreeTag = -1;
	int whereVerbTag = findTag(tagSet, u"VERB", nextVerbTag);
	if (whereVerbTag < 0) return false;
	int whereVObjectTag = findTagConstrained(tagSet, u"V_OBJECT", nextVObjectTag, tagSet[whereVerbTag]);
	// if there is no vobject, take last vagree, otherwise, take last vobject.
	if (nextVObjectTag >= 0) tag = nextVObjectTag;
	else if (whereVObjectTag >= 0) tag = whereVObjectTag;
	else
	{
		int whereVAgreeTag = findTagConstrained(tagSet, u"V_AGREE", nextVAgreeTag, tagSet[whereVerbTag]);
		if (nextVAgreeTag >= 0) tag = nextVAgreeTag;
		else if (whereVAgreeTag >= 0) tag = whereVAgreeTag;
		else return false;
	}
	return true;
}

// Like getVerb but only looks for V_OBJECT (infinitive / object-verb). Returns false if none.
// Restored: the 2026-09-05 uncalled-code sweep removed this and its only call site
// (the `(infinitive) ? !getIVerb(...) : !getVerb(...)` ternary in
// cSource::evaluateVerbObjects), which left verbTagIndex read uninitialized there.
bool cSource::getIVerb(vector <cTagLocation>& tagSet, int& tag)
{
	LFS
		int nextVObjectTag = -1;
	int whereVObjectTag = findTag(tagSet, u"V_OBJECT", nextVObjectTag);
	// if there is no vobject, take last vagree, otherwise, take last vobject.
	if (nextVObjectTag >= 0) tag = nextVObjectTag;
	else if (whereVObjectTag >= 0) tag = whereVObjectTag;
	else return false;
	return true;
}


// True unless this is a BNC-pre-tagged source and the form at position is flagged uncertain.
bool cSource::tagIsCertain(int position)
{
	LFS
		return !preTaggedSource || (m[position].flags & cWordMatch::flagBNCFormNotCertain) == 0;
}

// Resolves tag to a class word (PPN/NUM/…) before objects exist. Single-token tags use
// resolveToClass; multi-token NOUN tags collect GNOUN/MNOUN and take the first singleton.
// Returns false if tag < 0, the pattern is not a NOUN, or no class word is found.
bool cSource::resolveObjectTagBeforeObjectResolution(vector <cTagLocation>& tagSet, int tag, tIWMM& word, lpwstring purpose)
{
	LFS
		if (tag < 0) return false;
	if (tagSet[tag].len == 1)
		word = resolveToClass(tagSet[tag].sourcePosition);
	else
	{
		// if object is not type of NOUN, forget it. (searching for N_AGREE in an S1 would be pointless and very time-consuming).
		if (patterns[tagSet[tag].pattern]->name.find(u"NOUN") == lpwstring::npos || patterns[tagSet[tag].pattern]->hasTag(GNOUN_TAG) || patterns[tagSet[tag].pattern]->hasTag(MNOUN_TAG))
			return false;
		vector < vector <cTagLocation> > tagSets;
		// He gave a book.
		if (startCollectTagsFromTag(false, nAgreeTagSet, tagSet[tag], tagSets, GNOUN_TAG, true, false, purpose + u"| resolve object - GNOUN") > 0 ||
			startCollectTagsFromTag(false, nAgreeTagSet, tagSet[tag], tagSets, MNOUN_TAG, true, false, purpose + u"| resolve object - MNOUN") > 0)
			for (unsigned int J = 0; J < tagSets.size(); J++)
				if (tagSets[J].size() == 1)
				{
					word = resolveToClass(tagSets[J][0].sourcePosition);
					break;
				}
		if (word == wNULL) return false;
	}
	return true;
}





// Collects tagSet starting from an existing cTagLocation (pattern/position/len/PEMAOffset).
// If PEMAOffset < 0, searches root-pattern PMA rows of that span (skipping rejectTag).
// Returns tagSets.size() if any non-empty set was produced, else 0 (also 0 if the pattern
// bitmap says it cannot contain tagSet).
size_t cSource::startCollectTagsFromTag(bool inTrace, int tagSet, cTagLocation& tl, vector < vector <cTagLocation> >& tagSets, int rejectTag, bool obeyBlock, bool collectSelfTags, lpwstring purpose)
{
	LFS
		int pattern = tl.pattern, position = tl.sourcePosition, end = tl.len, PEMAOffset = tl.PEMAOffset;
	if (obeyBlock) // in this case, if we check for descendants even with obeyBlock on, _NOUN[9] will not appear to contain prepTagSet because _PP is blocked in that pattern.  It will only contain PREP.
	{
		if (collectSelfTags && !(patterns[pattern]->includesDescendantsAndSelfAllOfTagSet & ((int64_t)1 << tagSet)))
			return 0;
		if (!collectSelfTags && !(patterns[pattern]->includesOnlyDescendantsAllOfTagSet & ((int64_t)1 << tagSet)))
			return 0;
	}
	if (PEMAOffset < 0)
	{
		for (int p = patterns[pattern]->rootPattern; p >= 0; p = patterns[p]->nextRoot)
		{
			if (!m[position].patterns.isSet(p)) continue;
			if (rejectTag >= 0 && patterns[p]->hasTag(rejectTag))
				continue;
			cPatternMatchArray::tPatternMatch* pma = m[position].pma.find(p, end);
			if (pma == NULL) continue;
			PEMAOffset = pma->pemaByPatternEnd;
			cPatternElementMatchArray::tPatternElementMatch* pem = pema.begin() + PEMAOffset;
			for (; PEMAOffset >= 0 && pem->getParentPattern() == p && pem->end == end; PEMAOffset = pem->nextByPatternEnd, pem = pema.begin() + PEMAOffset)
				if (!pem->begin) break;
			if (PEMAOffset < 0 || pem->getParentPattern() != p || pem->end != end || pem->begin) continue;
			startCollectTags(inTrace, tagSet, position, PEMAOffset, tagSets, obeyBlock, collectSelfTags, purpose + u"| from tag ");
		}
	}
	else
		startCollectTags(inTrace, tagSet, position, PEMAOffset, tagSets, obeyBlock, collectSelfTags, purpose + u"| from tag ");
	for (auto ttagSet : tagSets)
		if (ttagSet.size() > 0)
			return tagSets.size();
	return 0;
}

// Entry for collectTags: clears secondaryPEMAPositions, optionally seeds tTagSet with the
// parent pattern's own tags, then collects. Drops empty tagSets and repairs secondary
// indexes. Returns 0 if PEMAPosition < 0 or the includes* bitmap cannot contain tagSet;
// otherwise tagSets.size() (even if exitTags tripped the time/count cap).
size_t cSource::startCollectTags(bool inTrace, int tagSet, int position, int PEMAPosition, vector < vector <cTagLocation> >& tagSets, bool obeyBlock, bool collectSelfTags, lpwstring purpose)
{
	LFS
		secondaryPEMAPositions.clear();
	if (PEMAPosition < 0)
	{
		//lplog(LOG_ERROR,u"PEMA offset is negative at position %d - all PEMA positions have been eliminated.",position);
		return 0;
	}
	debugTrace.traceTags = inTrace && debugTrace.traceTagSetCollection;
	int pattern = pema[PEMAPosition].getParentPattern();
	if (collectSelfTags && !(patterns[pattern]->includesDescendantsAndSelfAllOfTagSet & ((int64_t)1 << tagSet)))
	{
		if (debugTrace.traceTags)
			lplog(u"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (not enough tags [descendants and self]) - from %s", position, PEMAPosition, desiredTagSets[tagSet].name.c_str(),
				patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position, position + pema[PEMAPosition].end, purpose.c_str());
		return 0;
	}
	if (!collectSelfTags && !(patterns[pattern]->includesOnlyDescendantsAllOfTagSet & ((int64_t)1 << tagSet)))
	{
		if (debugTrace.traceTags)
			lplog(u"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (not enough tags [only descendants]) - from %s", position, PEMAPosition, desiredTagSets[tagSet].name.c_str(),
				patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position, position + pema[PEMAPosition].end, purpose.c_str());
		return 0;
	}
	if (debugTrace.traceTags)
		lplog(u"%d:======== EVALUATING %06d %s %s[%s](%d,%d) - from %s", position, PEMAPosition, desiredTagSets[tagSet].name.c_str(),
			patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position, position + pema[PEMAPosition].end, purpose.c_str());
	exitTags = false;

	vector <cTagLocation> tTagSet;
	beginTime = clock();
	timerForExit = 0;
	blocking = obeyBlock; // only true for BNCPatternViolation
	desiredTagSetNum = tagSet;
	focused = collectTagsFocusPositions.size() > 0;
	//TagSetMap.clear();
	if (collectSelfTags)
	{
		int tag;
		unsigned int beginTag = 0, p = pema[PEMAPosition].getParentPattern(), recursionLevel = 0;
		//bool found=false;
		while ((tag = patterns[p]->hasTagInSet(desiredTagSetNum, beginTag)) >= 0)
		{
			if (debugTrace.traceTags)
				lplog(u"%*s%d:TAG %s FOUND (self pattern) %s[%s](%d,%d)", recursionLevel * 2, " ", position, patternTagStrings[tag].c_str(),
					patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), position, position + pema[PEMAPosition].end);
			tTagSet.push_back(cTagLocation(tag, p, p, pema[PEMAPosition].getElement(), position, pema[PEMAPosition].end, PEMAPosition, true));
		}
	}
	collectTags(0, PEMAPosition, position, tTagSet, tagSets, pemaMapToTagSetsByPemaByTagSet[tagSet]);
	int numTagSets = (signed)tagSets.size();
	for (int J = secondaryPEMAPositions.size() - 1; J >= 0; J--)
		if (secondaryPEMAPositions[J].getTagSet() >= tagSets.size())
		{
			if (debugTrace.traceTags)
			{
				lpwstring sentence, originalIWord;
				for (int w = max(0, position - 8); w < (int)min(m.size(), (size_t)(position + 8)); w++) // batch B12: explicit common type
				{
					getOriginalWord(w, originalIWord, false, false);
					sentence += originalIWord + u" ";
				}
				lplog(LOG_INFO, u"%s:%d:%d:index %d out of %d has tagSet %d! [%s]", sourcePath.c_str(), position, numTagSets, J, secondaryPEMAPositions.size(), secondaryPEMAPositions[J].getTagSet(), sentence.c_str());
			}
			secondaryPEMAPositions.erase(secondaryPEMAPositions.begin() + J);
		}
	for (int nt = numTagSets - 1; nt >= 0; nt--)
	{
		if (tagSets[nt].empty())
		{
			tagSets.erase(tagSets.begin() + nt);
			for (int J = secondaryPEMAPositions.size() - 1; J >= 0; J--)
			{
				// `else if`, not a second `if`: the erase above removes element J, so
				// re-reading secondaryPEMAPositions[J] on the next line read one past
				// the end whenever J was the last element -- into capacity, where the
				// leftover value differs from run to run. AddressSanitizer reports it
				// as a container-overflow. The two tests are mutually exclusive
				// (== nt vs > nt), so chaining them is also what was meant.
				if (secondaryPEMAPositions[J].getTagSet() == nt)
					secondaryPEMAPositions.erase(secondaryPEMAPositions.begin() + J);
				else if (secondaryPEMAPositions[J].getTagSet() > nt)
					secondaryPEMAPositions[J].setTagSet(secondaryPEMAPositions[J].getTagSet() - 1);
			}
			numTagSets--;
		}
	}
	if (!exitTags)
		return tagSets.size();
	if (debugTrace.traceMatchedSentences || debugTrace.traceUnmatchedSentences)
	{
		if (tagSets.size() < MAX_TAGSETS)
			lplog(LOG_ERROR, u"%d:Maximum time limit hit (%d microseconds) when collecting tags for %s %s[%s](%d,%d)", position, COLLECT_TAGS_TIME_LIMIT, desiredTagSets[tagSet].name.c_str(),
				patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position, position + pema[PEMAPosition].end);
		else if (debugTrace.traceTagSetCollection)
			lplog(LOG_ERROR, u"%d:Maximum # of tagsets hit when collecting tags for %s %s[%s](%d,%d)", position, desiredTagSets[tagSet].name.c_str(),
				patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position, position + pema[PEMAPosition].end);
	}
	return tagSets.size();
}

// True if this token is a gendered personal/indefinite/reciprocal pronoun or a
// single-gender proper name (flagOnlyConsiderProperNounForms, or flagAddProperNoun + winner PN).
bool cWordMatch::isPPN(void)
{
	LFS
		// this covers personal_pronoun_nominative,personal_pronoun_accusative,personal_pronoun,all proper names
		return !(word->second.inflectionFlags & NEUTER_GENDER) &&
		((word->second.inflectionFlags & (FIRST_PERSON | SECOND_PERSON | THIRD_PERSON)) ||
			queryWinnerForm(indefinitePronounForm) >= 0 || queryWinnerForm(reciprocalPronounForm) >= 0 ||
			queryWinnerForm(personalPronounAccusativeForm) >= 0 || queryWinnerForm(nomForm) >= 0) ||
		((((word->second.inflectionFlags & MALE_GENDER) == MALE_GENDER) ^ ((word->second.inflectionFlags & FEMALE_GENDER) == FEMALE_GENDER)) &&
			((flags & flagOnlyConsiderProperNounForms) || ((flags & flagAddProperNoun) && queryWinnerForm(PROPER_NOUN_FORM_NUM) >= 0)));
}

// Maps this token to a class word: PPN, NUM, DATE, TIME, TELENUM, else mainEntry, else self.
tIWMM cWordMatch::resolveToClass()
{
	LFS
		tIWMM w = word;
	if (isPPN())
		w = Words.PPN;
	else if (queryWinnerForm(NUMBER_FORM_NUM) >= 0)
		w = Words.NUM;
	else if (queryWinnerForm(dateForm) >= 0)
		w = Words.DATE;
	else if (queryWinnerForm(timeForm) >= 0)
		w = Words.TIME;
	else if (queryWinnerForm(telenumForm) >= 0)
		w = Words.TELENUM;
	else if (word->second.mainEntry != wNULL)
		w = word->second.mainEntry;
	return w;
}

// Like cWordMatch::resolveToClass but also honors an already-attached _DATE/_TIME/_TELENUM
// object pattern at beginObjectPosition. Used before objects are fully resolved.
// this is used before any objects are resolved, or marked for time or location
tIWMM cSource::resolveToClass(int where)
{
	LFS
		tIWMM o = wNULL;
	if (m[where].getObject() >= 0)
	{
		int beginObjectPosition = m[where].beginObjectPosition;
		if (m[beginObjectPosition].pma.queryPattern(u"_DATE") != -1)
			return Words.DATE;
		else if (m[beginObjectPosition].pma.queryPattern(u"_TIME") != -1)
			return Words.TIME;
		else if (m[beginObjectPosition].pma.queryPattern(u"_TELENUM") != -1)
			return Words.TELENUM;
	}
	if (m[where].isPPN())
		return Words.PPN;
	else if (m[where].queryWinnerForm(NUMBER_FORM_NUM) >= 0)
		return Words.NUM;
	else if (m[where].queryWinnerForm(dateForm) >= 0)
		return Words.DATE;
	else if (m[where].queryWinnerForm(timeForm) >= 0)
		return Words.TIME;
	else if (m[where].queryWinnerForm(telenumForm) >= 0)
		return Words.TELENUM;
	else if (m[where].pma.queryPattern(u"_DATE") != -1)
		return Words.DATE;
	else if (m[where].pma.queryPattern(u"_TIME") != -1)
		return Words.TIME;
	else if (m[where].pma.queryPattern(u"_TELENUM") != -1)
		return Words.TELENUM;
	return m[where].resolveToClass();
}

// Maps a resolved object o at 'where' to LOCATION, TIME, PPN (speakers / gendered classes),
// the surface word (body / non-gendered), or wNULL if the class is unrecognized.
tIWMM cSource::resolveObjectToClass(int where, int o)
{
	LFS
		int objectClass = objects[o].objectClass;
	if (objects[o].isLocationObject)
		return Words.LOCATION;
	if (objects[o].getIsTimeObject())
		return Words.TIME;
	if (objects[o].numDefinitelyIdentifiedAsSpeaker > 0 || objects[o].numIdentifiedAsSpeaker > 0)
		return Words.PPN;
	if (objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
		objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
		objectClass == GENDERED_DEMONYM_OBJECT_CLASS ||
		objectClass == GENDERED_RELATIVE_OBJECT_CLASS)
		return Words.PPN;
	// gendered but not a PPN
	if (objectClass == BODY_OBJECT_CLASS ||
		// non-gendered - not PPN
		objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS ||
		objectClass == NON_GENDERED_BUSINESS_OBJECT_CLASS ||
		objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
		return m[where].word;
	return wNULL;
}

// resolveToClass plus object-class override. If the object is neuter-only PPN, falls back
// to the surface word. If there is exactly one objectMatch, prefers that match's class
// unless the original class was already a special (PPN/NUM/DATE/TIME/TELENUM/LOCATION).
// this is used before any objects are resolved, or marked for time or location
tIWMM cSource::fullyResolveToClass(int where)
{
	LFS
		tIWMM w = resolveToClass(where);
	lpwstring word = m[where].word->first;
	if (m[where].getObject() >= 0)
	{
		int o = m[where].getObject();
		tIWMM ow = resolveObjectToClass(where, o);
		if (ow != wNULL) return ow;
		if (w == Words.PPN && objects[o].neuter && !objects[o].female && !objects[o].male)
			return m[where].word;
		if (m[where].objectMatches.size() != 1)
			return w;
		tIWMM specials[] = { Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION };
		for (unsigned int I = 0; I < sizeof(specials) / sizeof(tIWMM); I++)
			if (w == specials[I])
				return w;
		ow = resolveObjectToClass(where, m[where].objectMatches[0].object);
		if (ow != wNULL) return ow;
	}
	return w;
}

// Tries identifyObject on a NOUN at the tag's sourcePosition, then resolveTag. Used when a
// prepositional object must exist for a match. Returns true if both succeed.
bool cSource::forcePrepObject(vector <cTagLocation>& tagSet, int tag, int& object, int& whereObject, tIWMM& word)
{
	LFS
		if (identifyObject(findTag(u"NOUN"), tagSet[tag].sourcePosition, -1, false, -1, -1) >= 0 &&
			resolveTag(tagSet, tag, object, whereObject, word))
		{
			lpwstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%d:%s object forced by preposition phrase match", whereObject, whereString(whereObject, tmpstr, false).c_str());
			return true;
		}
	return false;
}

// Resolves tagSet[tag] to (object, whereObject, word). Gendered/pronoun/meta-group objects
// become Words.PPN; other certain objects use resolveToClass. Single-token certain tags
// override to the token's class. Returns false if tag < 0 or word stays wNULL.
/*
All 4-digit numbers replaced with 'date'.
Gendered proper nouns or personal pronouns are replaced with PPN.
*/
bool cSource::resolveTag(vector <cTagLocation>& tagSet, int tag, int& object, int& whereObject, tIWMM& word)
{
	LFS
		if (tag < 0) return false;
	word = wNULL;
	if ((object = findObject(tagSet[tag], whereObject)) >= 0)
	{
		if (objects[object].objectClass == PRONOUN_OBJECT_CLASS ||
			objects[object].objectClass == REFLEXIVE_PRONOUN_OBJECT_CLASS ||
			objects[object].objectClass == RECIPROCAL_PRONOUN_OBJECT_CLASS ||
			(objects[object].objectClass == NAME_OBJECT_CLASS &&
				(objects[object].male ^ objects[object].female)) ||
			objects[object].objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
			objects[object].objectClass == BODY_OBJECT_CLASS ||
			objects[object].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			objects[object].objectClass == GENDERED_DEMONYM_OBJECT_CLASS ||
			objects[object].objectClass == GENDERED_RELATIVE_OBJECT_CLASS ||
			objects[object].objectClass == META_GROUP_OBJECT_CLASS)
			word = Words.PPN;
		else if (tagIsCertain(objects[object].originalLocation))
			word = resolveToClass(objects[object].originalLocation);
	}
	if (tagSet[tag].len == 1 && tagIsCertain(tagSet[tag].sourcePosition) && (object < 0 || m[tagSet[tag].sourcePosition].getObject() >= 0))
		word = resolveToClass(whereObject = tagSet[tag].sourcePosition);
	if (word == wNULL)
		return false;
	return true;
}

