#include <windows.h>
#include <io.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "malloc.h"
#include "profile.h"
#include <algorithm>    // std::lower_bound, std::upper_bound, std::sort

// (CMREADME018)
void Source::setRole(int position,patternElementMatchArray::tPatternElementMatch *pem)
{ LFS
	int end=position+pem->end;
	__int64 tagRole=0,childRole=pem->getRole(tagRole);
	if ((tagRole&SENTENCE_IN_REL_ROLE) && 
			m[position].word->second.query(L"interrogative_pronoun")<0 && m[position].word->first!=L"whose" && m[position].word->first!=L"where" && m[position].word->first!=L"that")
	{
		tagRole&=~SENTENCE_IN_REL_ROLE; // not really a relative clause!
		tagRole|=SENTENCE_IN_ALT_REL_ROLE;
	}
	// She[tuppence] was relieved to see that the visitor[second] was the second of the two men[mr] whom Tommy had taken upon himself[tommy] to follow .
	if ((tagRole&SENTENCE_IN_REL_ROLE) && position && isVision(position-1))
		tagRole&=~SENTENCE_IN_REL_ROLE; // subject is in relative clause, but should be regarded as not in one for the purposes of physical presence
	// detect when the secondary object differs in gender from the previous object.  If so, cancel the role.
	// all re_objects come immediately after their primary object.
	if ((childRole&RE_OBJECT_ROLE) && m[position].principalWherePosition>=0 && m[m[position].principalWherePosition].getObject()>=0)
	{
		for (int I=position-1; (I>=position-10 || !isEOS(I)) && I>=0; I--)
			if (m[I].getObject()>=0 && !(m[I].flags&WordMatch::flagAdjectivalObject))
			{
				int originalObject=m[I].getObject(),secondaryObject=m[m[position].principalWherePosition].getObject();
				bool genderUncertainMatch=false;
				if (!objects[originalObject].matchGenderIncludingNeuter(objects[secondaryObject],genderUncertainMatch))
				{
					childRole&=~RE_OBJECT_ROLE;
					if (debugTrace.traceSpeakerResolution)
					{
						wstring tmpstr,tmpstr2;
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, L"%06d:removed re_object role from object %s (original object %d:%s).", position, objectString(secondaryObject, tmpstr, true).c_str(), I, objectString(originalObject, tmpstr2, true).c_str());
					}
				}
				break;
			}
	}
	// detect plural ambiguity - move the mplural forward to start at the last object occurring before the conjunction
	// a tall man with close - cropped hair and a short , pointed , naval - looking beard 
	if (tagRole&MPLURAL_ROLE)
	{
		int whereLastObjectBeforeConjunction=-1;
		for (int I=position; I<end; I++)
		{
			if (m[I].getObject()>=0) whereLastObjectBeforeConjunction=I;
			if (m[I].queryWinnerForm(conjunctionForm)>=0 || m[I].queryWinnerForm(coordinatorForm)>=0)
			{
				if (whereLastObjectBeforeConjunction>=0 && position!=whereLastObjectBeforeConjunction)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:ambiguous multiple object compressed from %d-%d to %d-%d.",
							I,position,end,whereLastObjectBeforeConjunction,end);
					position=whereLastObjectBeforeConjunction;
				}
				break;
			}
		}
	}
	if (tagRole)
	{
		int maxEnd=-1;
		for (int I=position; I<end; I++)
		{
			// skip over any relative phrases _PP and _REL1
			// a tall man with close - cropped hair and a short , pointed , naval - looking beard , who sat at the head of the table[table] with papers in front of him
			if ((tagRole&MPLURAL_ROLE) && (m[I].pma.queryPattern(L"_REL1",maxEnd)!=-1 || m[I].pma.queryPattern(L"_PP",maxEnd)!=-1))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:skip multiple object (%d-%d) (%d-%d).",I,position,end,I,I+maxEnd);
				I+=maxEnd-1;
			}
			else
				m[I].objectRole|=tagRole;
		}
	}
	if (childRole>0)
	{
		if (pem->isChildPattern())
			end=position+pem->getChildLen();
		else
			end=position+1;
		for (int I=position; I<end; I++)
		{
			// because getRole always goes in forward order through the text, if the currentRole is already a PREP_OBJECT_ROLE,
			// then we have a prepositional phrase with an embedded sentence in it, which is misparsed, and the assumption is
			// that the prepositional phrase should be cancelled.
			if ((childRole&SUBJECT_ROLE) && (m[I].objectRole&PREP_OBJECT_ROLE))
				m[I].objectRole&=~PREP_OBJECT_ROLE; // if it is already a prepositional object, and we are making it a subject, then subtract the preposition.
			// if passive subject don't mark with prep object
			if (!(childRole&PREP_OBJECT_ROLE) || !(m[I].objectRole&PASSIVE_SUBJECT_ROLE))
				m[I].objectRole|=childRole;
		}
	}
}

// finds all pattern matches that:
// 1. Are the lowest cost patterns
// 2. have the same rootPattern
// 4. have the same end as childend
unsigned int Source::getAllLocations(unsigned int position,int parentPattern,int rootPattern,int childLen,int parentLen,vector <unsigned int> &allLocations, int recursionLevel, unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions,bool &reassessParentCosts)
{ LFS
	int minCost=MAX_COST;
	patternMatchArray::tPatternMatch *pm;
	if (childLen!=parentLen) parentPattern=-1;
	for (int rp=rootPattern; rp>=0; rp=patterns[rp]->nextRoot)
	{
		if (rp!=parentPattern && m[position].patterns.isSet(rp) && (pm=m[position].pma.find(rp,childLen))!=NULL)
		{
			// check for patterns that are compatible with VOC or AGREE that have been rejected from assess cost because they have no fill flag, yet are winners because their parents are.
			cPattern *p=patterns[pm->getPattern()];
			if (!p->isTopLevelMatch(*this,position,position+pm->len) && p->fillIfAloneFlag)
			{
				vector < vector <tTagLocation> > tagSets;
				if (debugTrace.tracePatternElimination)
				{
					lplog(L"%*sMC getAllLocations BEGIN REASSESS COST position %d:pattern %s[%s](%d,%d)",
						recursionLevel * 2, " ", position, p->name.c_str(), p->differentiator.c_str(), position, position + pm->len);
				}
				assessCost(NULL,pm,-1,position,tagSets, tertiaryPEMAPositions,false,L"get all locations");
				reassessParentCosts = true;
				if (debugTrace.tracePatternElimination)
				{
					lplog(L"%*sMC getAllLocations END REASSESS COST position %d:pattern %s[%s](%d,%d)",
						recursionLevel * 2, " ", position, p->name.c_str(), p->differentiator.c_str(), position, position + pm->len);
				}
			}
			if (minCost > pm->getCost())
			{
				if (debugTrace.tracePatternElimination)
				{
					lplog(L"%*sMC getAllLocations position %d:pattern %s[%s](%d,%d) (cost %d < previous lowest cost %d)",
						recursionLevel * 2, " ", position, p->name.c_str(), p->differentiator.c_str(), position, position+pm->len,
						pm->getCost(), minCost);
				}
				minCost = pm->getCost();
			}
			else if (debugTrace.tracePatternElimination)
				lplog(L"%*sMC getAllLocations position %d:pattern %s[%s](%d,%d) (cost %d >= previous lowest cost %d)",
					recursionLevel * 2, " ", position, p->name.c_str(), p->differentiator.c_str(), position, position + pm->len,
					pm->getCost(), minCost);
			allLocations.push_back((int)(pm-m[position].pma.content));
		}
	}
	return minCost;
}

// determine the minimum cost of the pattern recorded over all the ways it was matched with the given begin and end.
// although this could get the minimum cost throughout the pattern,
//   agreement costs are only attributed to each individual disagreeing element within the pattern,
//   not necessarily percolating up to the top pattern on each position (although it will always percolate up to the
//     FIRST position of each pattern that only contains disagreeing positions)
int Source::getMinCost(patternElementMatchArray::tPatternElementMatch *pem, int &minPEMAOffset)
{ LFS
	int minCost=MAX_COST,pattern=pem->getPattern(),relativeEnd=pem->end,relativeBegin=pem->begin;
	for (; pem && pem->getPattern()==pattern && pem->end==relativeEnd; pem=(pem->nextByPatternEnd<0) ? NULL : pema.begin()+pem->nextByPatternEnd)
		if (pem->begin == relativeBegin && minCost > pem->getOCost())
		{
			minCost = pem->getOCost();
			minPEMAOffset = (int) (pem - pema.begin());
		}
	return minCost;
}

// mark pma children(p,begin,end)
//   for each position from begin to end
//     for each elementMatch in pema matching p, end and begin
//       look up element match (p and end) in pma
//         mark winner
//         mark pma children
//       look up element match in pema - mark winner
// begin and end are non relative.
// childNum added because S1 (in particular) could have multiple children matching in different places so that
// S1 matching different children in different places could be treated evenly, when in fact because of agreement
// problems S1 matching different children in different places should not be treated the same, even though ultimately
// they have the same parent.
// the below example starts at 71.
//   sentence:I was given this watch by my father.
//   analysis of subclause: this watch by my father
// this(71)[8]             watch(72)[8]       by(73)[8]       my(74)[8]                     father(75)[8]                       .(76)[
// dem_det S*0             v PRES_1ST*0       prep*0          inter*0                               n S MG*0                                .*0
//                                 n S*0                      adv ADV*0       pos_det SO 1P*0               v PRES_1ST*0
//                             adj ADJ*0              adj ADJ*0
//                                                            n S*0
//71:Saved cost increase to __NOUN[2](71,74)
//71:Saved cost increase to __NOUN[5](71,74)
//71:Saved cost increase to __NOUN[2](72,74)
//71:Saved cost increase to __NOUN[2](71,73)
//71:Saved cost increase to __NOUN[6](71,73)
//71:Saved cost increase to __NOUN[2](72,73)
//71:Saved cost increase to __NOUN[6](71,72)
//71:Saved cost increase to __C1__S1[](71,72)
//71:Increased cost of __S1[1](71,76) to 10 (childEnd=72)
//71:Saved cost increase to __ALLVERB[](72,73)
//71:Saved cost increase to __ALLVERB[](72,74)
//71:Saved cost increase to __ALLVERB[](72,75)
//71:Saved cost increase to __C2__S1[](72,73)
//71:Saved cost increase to __C2__S1[](72,74)
//71:Saved cost increase to __C2__S1[](72,75)

//  this(71)
//  Before eliminateLoserPatterns         After eliminateLoserPatterns
//  __S1[1](71,76)*4 __C1__S1[*](74)      __C1__S1[](71,73)*0 __NOUN[*](73)       CORRECT
//  __S1[1](71,76)*0 __C1__S1[*](73)      __S1[1](71,76)*0 __C1__S1[*](73)        CORRECT
//  __S1[1](71,76)*0 __C1__S1[*](72)
//  __C1__S1[](71,72)*0 __NOUN[*](72)
//  __C1__S1[](71,73)*0 __NOUN[*](73)
//  __C1__S1[](71,74)*0 __NOUN[*](74)

// watch(72)
//  Before eliminateLoserPatterns         After eliminateLoserPatterns
//  __S1[1](71,76)*0 __C2__S1[*](75)            __C2__S1[](72,73)*20 __ALLVERB[*](73)   INCORRECT! - the parent with this childEnd of 72 has been eliminated.
//  __S1[1](71,76)*2 __C2__S1[*](74)      __C2__S1[](72,75)*20 __ALLVERB[*](75)   INCORRECT!
//  __S1[1](71,76)*0 __C2__S1[*](73)      __C2__S1[](72,76)*0 __ALLVERB[*](73)    INCORRECT!
//  __S1[1](71,76)*0 __C2__S1[*](76)      __S1[1](71,76)*0 __C2__S1[*](75)        INCORRECT!
//  __C2__S1[](72,73)*0 __ALLVERB[*](73)  __S1[1](71,76)*0 __C2__S1[*](73)        INCORRECT!
//  __C2__S1[](72,74)*2 __ALLVERB[*](74)  __S1[1](71,76)*0 __C2__S1[*](76)        INCORRECT!
//  __C2__S1[](72,75)*0 __ALLVERB[*](75)
//  __C2__S1[](72,76)*0 __ALLVERB[*](73)

// by(73)
//  Before eliminateLoserPatterns         After eliminateLoserPatterns
// __ALLOBJECTS[](73,76)*1 __NOUN[*](74)   __ALLOBJECTS[](73,76)*0 _NOUN[*](76)   INCORRECT!
// __ALLOBJECTS[](73,76)*0 __NOUN[*](76)   __C2__S1[](73,76)*0 __ALLVERB[*](76)   CORRECT (although this depends on 'my' being an interjection, to match with _ADVERB)
// __C2__S1[](73,76)*0 __ALLVERB[*](76)   __S1[1](71,76)*0 __ALLOBJECTS[*](76)   INCORRECT! - not possible, skipped __C2__S1 so this should have been eliminated
// __S1[1](71,76)*0 __ALLOBJECTS[*](76)   __S1[1](71,76)*0 __C2__S1[*](76)       CORRECT - agrees with S1 having a childEnd of 73
// __S1[1](71,76)*0 __C2__S1[*](76)
// mark a pattern with a begin and end, begin at source position position.
int Source::markChildren(patternElementMatchArray::tPatternElementMatch *pem, int position, int recursionLevel, int allRootsLowestCost, unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions,bool &reassessParentCosts)
{
	LFS
		// find first position with pattern and relativeEnd
	int pattern = pem->getPattern(), begin = pem->begin + position, end = pem->end + position, relativeEnd = pem->end, relativeBegin = pem->begin,minPEMAOffset = -1;
	int numChildren=0,minCost=(allRootsLowestCost==MIN_INT) ? getMinCost(pem,minPEMAOffset) : allRootsLowestCost;
	if (debugTrace.tracePatternElimination)
	{
		wstring minPEMAOffsetStr;
		if (minPEMAOffset != -1)
		{
			unsigned int minPattern = pema[minPEMAOffset].getPattern();
			wstring tmp1, tmp2;
			minPEMAOffsetStr = L" from " + patterns[minPattern]->name + L"[" + patterns[minPattern]->differentiator + L"](" + itos((unsigned int)pema[minPEMAOffset].begin + position, tmp1) + L"," + itos(pema[minPEMAOffset].end + position, tmp2) + L")";
		}
		lplog(L"%*s%d:MC pattern %s[%s](%d,%d) element #%d minCost=%d%s-----------------", recursionLevel * 2, " ",
			position, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end, pem->getElement(), minCost, minPEMAOffsetStr.c_str());
	}
	for (; pem && pem->getPattern()==pattern && pem->end==relativeEnd && !exitNow; pem=(pem->nextByPatternEnd<0) ? NULL : pema.begin()+pem->nextByPatternEnd)
		if (pem->begin==relativeBegin)
		{ 
			//if (pem->getOCost() < minCost)
			//	lplog(LOG_ERROR|LOG_INFO,L"COSTING INTERNAL ERROR - PEMA %06d:%s[%s](%d,%d)*%d less than minCost %d", pem - pema.begin(), patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end, pem->getOCost(), minCost);
			if (pem->getOCost()>minCost)
			{
				if (debugTrace.tracePatternElimination)
				{
					if (pem->isChildPattern())
						lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %s[%s](%d,%d) PEMA rejected (cost %d > minCost %d).",recursionLevel*2," ",position,
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,pem->getOCost(),
						patterns[pem->getChildPattern()]->name.c_str(),patterns[pem->getChildPattern()]->differentiator.c_str(),position,position+pem->getChildLen(),
						pem->getOCost(),minCost);
					else
						lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %s form PEMA rejected (cost %d > minCost %d).",recursionLevel*2," ",position,
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,pem->getOCost(),
							Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str(),pem->getOCost(),minCost);
				}
				continue;
			}
			if (debugTrace.tracePatternElimination)
			{
				if (pem->isChildPattern())
					lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %s[%s](%d,%d) PEMA[%d] set winner.",recursionLevel*2," ",position,
					patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,minCost,
					patterns[pem->getChildPattern()]->name.c_str(),patterns[pem->getChildPattern()]->differentiator.c_str(),position,position+pem->getChildLen(),
					pem - pema.begin());
				else
				{
					lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %s PEMA[%d] set winner.", recursionLevel * 2, " ", position,
						patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end, minCost,
						Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str(), pem - pema.begin());
				}
			}
			pem->setWinner();
			setRole(position,pem);
			numChildren++;
			if (pem->isChildPattern())
			{
				int childLen=pem->getChildLen();
				int rootp=patterns[pem->getChildPattern()]->rootPattern;
				vector <unsigned int> allLocations;
				// this routine gets all the patterns that are actually matched by the root pattern.
				// this also returns locations in pma so that each pattern is also accompanied by a cost.
				int lowestCost=getAllLocations(position,pattern,rootp,childLen,end-begin,allLocations, recursionLevel, tertiaryPEMAPositions,reassessParentCosts);
				vector <int> setAsWinners;
				// if (lowestCost>minCost) // this sometimes happens because of negative costs in some patterns
				// 	 lplog(L"STOP! lowestCost %ld>minCost %d.",lowestCost,minCost);
				for (unsigned int lc=0; lc<allLocations.size(); lc++)
				{
					patternMatchArray::tPatternMatch *pm=m[position].pma.content+allLocations[lc];
					unsigned int childp=pm->getPattern();
					if (pm->getCost()>lowestCost)
					{
						if (debugTrace.tracePatternElimination)
							lplog(L"%*sMC position %d:pattern %s[%s](%d,%d) child %s[%s](%d,%d) PMA rejected (cost %d>lowest cost %d)",
								recursionLevel*2," ",position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,
								patterns[childp]->name.c_str(),patterns[childp]->differentiator.c_str(),position,position+childLen,pm->getCost(),lowestCost);
						continue;
					}
					if (pm->isWinner())
					{
						if (debugTrace.tracePatternElimination)
							lplog(L"%*sMC position %d:pma %d:pattern %s[%s](%d,%d) child %s[%s](%d,%d) PMA kept as winner (cost %d,lowest cost %d).",
								recursionLevel * 2, " ", position, allLocations[lc], patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end,
								patterns[childp]->name.c_str(), patterns[childp]->differentiator.c_str(), position, position + childLen, pm->getCost(), lowestCost);
						continue;
					}
					setAsWinners.push_back(allLocations[lc]);
					if (debugTrace.tracePatternElimination)
						lplog(L"%*sMC position %d:pma %d:pattern %s[%s](%d,%d) child %s[%s](%d,%d) PMA set winner (cost %d<=lowest cost %d).",
							recursionLevel*2," ",position, allLocations[lc], patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,
							patterns[childp]->name.c_str(),patterns[childp]->differentiator.c_str(),position,position+childLen,pm->getCost(),lowestCost);
					bool localReassessParentCosts = false;
					numChildren+=markChildren(pema.begin()+pm->pemaByPatternEnd,position,recursionLevel+1,lowestCost, tertiaryPEMAPositions, localReassessParentCosts);
					patterns[pm->getPattern()]->numChildrenWinners++;
					pm->setWinner();
					if (localReassessParentCosts)
					{
						reassessParentCosts = true; // let parent markChildren know!
						// recalculate lowestCost that could have changed
						lowestCost = MAX_COST;
						for (unsigned int clc = 0; clc < allLocations.size(); clc++)
							lowestCost = min(lowestCost, m[position].pma.content[allLocations[clc]].getCost());
						// go back to beginning and see whether there are other patterns that could become winners.
						lc = 0;
						// the lowest cost could have changed, or the cost of the winners could have changed, so re-evaluate the winners already set
						vector <int> keptWinners;
						for (int alreadySet : setAsWinners)
						{
							if (m[position].pma.content[alreadySet].getCost() > lowestCost)
							{
								childp = m[position].pma.content[alreadySet].getPattern();
								if (debugTrace.tracePatternElimination)
									lplog(L"%*sMC position %d:pattern %s[%s](%d,%d) child %s[%s](%d,%d) PMA CHANGED COST rejected WINNER REVERSED (cost %d>lowest cost %d)",
										recursionLevel * 2, " ", position, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end,
										patterns[childp]->name.c_str(), patterns[childp]->differentiator.c_str(), position, position + childLen, m[position].pma.content[alreadySet].getCost(), lowestCost);
								vector <__int64> alreadyCovered;
								removeWinnerFlag(position, m[position].pma.content + alreadySet, 2, alreadyCovered);
							}
							else
								keptWinners.push_back(alreadySet);
						}
						setAsWinners = keptWinners;
					}
				}
			}
			else
			{
				if (debugTrace.tracePatternElimination)
					lplog(L"%*sMC position %d:pattern %s[%s](%d,%d) child %s FORM set winner.",
					recursionLevel*2," ",position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end, Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str());
				m[position].setWinner(pem->getChildForm());
			}
			int nextPatternElement=pem->nextPatternElement;
			if (nextPatternElement >= 0)
			{
				minPEMAOffset = -1;
				int nextPEMALowestCost = getMinCost(pema.begin() + nextPatternElement, minPEMAOffset);
				if (debugTrace.tracePatternElimination)
				{
					int nextPEMAPosition = begin - pema[nextPatternElement].begin;
					int nextPEMAPattern = pema[nextPatternElement].getPattern(), nextPEMABegin = pema[nextPatternElement].begin + nextPEMAPosition, nextPEMAEnd = pema[nextPatternElement].end + nextPEMAPosition;
					wstring minPEMAOffsetStr;
					if (minPEMAOffset != -1)
					{
						unsigned int minPattern = pema[minPEMAOffset].getPattern();
						wstring tmp1, tmp2;
						minPEMAOffsetStr = L" from " + patterns[minPattern]->name + L"[" + patterns[minPattern]->differentiator + L"](" + itos(pema[minPEMAOffset].begin + nextPEMAPosition, tmp1) + L"," + itos(pema[minPEMAOffset].end + nextPEMAPosition, tmp2) + L")";
					}
					lplog(L"%*s%d:MC pattern %s[%s](%d,%d) element #%d minCost=%d%s-----------------", recursionLevel * 2, " ",
						nextPEMAPosition, patterns[nextPEMAPattern]->name.c_str(), patterns[nextPEMAPattern]->differentiator.c_str(), nextPEMABegin, nextPEMAEnd, pema[nextPatternElement].getElement(), nextPEMALowestCost, minPEMAOffsetStr.c_str());
					if (nextPEMALowestCost != allRootsLowestCost)
						lplog(L"%*s%d:MC lowest cost difference (nextPEMALowestCost %d != allRootsLowestCost %d)!", recursionLevel * 2, " ", nextPEMAPosition, nextPEMALowestCost , allRootsLowestCost);
				}
				numChildren += markChildren(pema.begin() + nextPatternElement, begin - pema[nextPatternElement].begin, recursionLevel, nextPEMALowestCost, tertiaryPEMAPositions,reassessParentCosts);
			}
		}
		return numChildren;
}

bool Source::findLowCostTag(vector<tTagLocation> &tagSet,int &cost,wchar_t *tagName,tTagLocation &lowestCostTag,int parentCost,int &nextTag)
{ LFS
	nextTag=-1;
	int where=findTag(tagSet,tagName,nextTag);
	if (where>=0)
	{
		int tmpCost=pema[abs(tagSet[where].PEMAOffset)].getOCost()+parentCost;
		if (tmpCost<cost)
		{
			lowestCostTag=tagSet[where];
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:lowest cost tag %s costs %d (OCost=%d + parentCost %d) < old cost of %d",lowestCostTag.sourcePosition,tagName,tmpCost,pema[abs(tagSet[where].PEMAOffset)].getOCost(),parentCost,cost);
			cost=tmpCost;
			return true;
		}
		else if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d:agreement tag %s not lowest cost (OCost %d + parentCost %d>=%d)",lowestCostTag.sourcePosition,tagName,tmpCost-parentCost,parentCost,cost);
	}
	else if (debugTrace.traceSubjectVerbAgreement)
		lplog(L"%d:agreement tag %s not found",lowestCostTag.sourcePosition,tagName);
	return false;
}

int Source::getSubjectInfo(tTagLocation subjectTag,int whereSubject, int &nounPosition,int &nameLastPosition, bool &restateSet, bool &singularSet, bool &pluralSet,bool &adjectivalSet)
{
	nounPosition = nameLastPosition = -1;
	singularSet = pluralSet = restateSet = adjectivalSet = false;
	if (subjectTag.len > 1)
	{
		int nounCost = 1000000, GNounCost = 1000000, nameCost = 1000000, sTagSet = -1;
		tTagLocation ntl(0, 0, 0, 0, 0, 0, 0, false), gtl(0, 0, 0, 0, 0, 0, 0, false), natl(0, 0, 0, 0, 0, 0, 0, false);
		vector < vector<tTagLocation> > subjectTagSets;
		tTagLocation *tl = &subjectTag;
		int pattern = tl->pattern, tagSetSubjectPosition = tl->sourcePosition, end = tl->len, PEMAOffset = tl->PEMAOffset;
		if (patterns[pattern]->includesDescendantsAndSelfAllOfTagSet&((__int64)1 << subjectTagSet))
		{
			if (PEMAOffset < 0)
			{
				for (int p = patterns[pattern]->rootPattern; p >= 0; p = patterns[p]->nextRoot)
				{
					if (!m[tagSetSubjectPosition].patterns.isSet(p)) continue;
					patternMatchArray::tPatternMatch *pma = m[tagSetSubjectPosition].pma.find(p, end);
					if (pma == NULL) continue;
					PEMAOffset = pma->pemaByPatternEnd;
					patternElementMatchArray::tPatternElementMatch *pem = pema.begin() + PEMAOffset;
					for (; PEMAOffset >= 0 && pem->getPattern() == p && pem->end == end; PEMAOffset = pem->nextByPatternEnd, pem = pema.begin() + PEMAOffset)
						if (!pem->begin) break;
					if (PEMAOffset < 0 || pem->getPattern() != p || pem->end != end || pem->begin) continue;
					int startSize = subjectTagSets.size();
					int parentCost = pema[abs(PEMAOffset)].getOCost();
					startCollectTags(debugTrace.traceSubjectVerbAgreement, subjectTagSet, tagSetSubjectPosition, PEMAOffset, subjectTagSets, true, true,L"GetSubjectInfoRootPattern");
					// find lowest cost noun or gnoun reference
					for (unsigned int K = startSize; K < subjectTagSets.size(); K++)
					{
						if (debugTrace.traceSubjectVerbAgreement)
							printTagSet(LOG_INFO, L"AGREE-SUBJECT (1)", K, subjectTagSets[K], tagSetSubjectPosition, subjectTag.PEMAOffset);
						// both of the below statements must execute!  (no || )
						int nagreeNextTag, nextTag;
						bool foundMNoun = false,foundGNoun=false,foundNAgree=false;
						if ((foundGNoun=findLowCostTag(subjectTagSets[K], GNounCost, L"GNOUN", gtl, parentCost, nextTag)) && GNounCost < nounCost && GNounCost < nameCost) sTagSet = K;
						if ((foundMNoun = findLowCostTag(subjectTagSets[K], GNounCost, L"MNOUN", gtl, parentCost, nextTag)) && GNounCost < nounCost && GNounCost < nameCost) sTagSet = K;
						// if there is an mnoun and 2 nagrees (or more), then don't count the nagree because the mnoun will produce 2 or more nagrees inside it and lowCost tag only counts the cost
						//     of one of the nagrees, so it would win, when clearly the mnoun should win.
						if ((foundNAgree=findLowCostTag(subjectTagSets[K], nounCost, L"N_AGREE", ntl, parentCost, nagreeNextTag)) && nounCost < GNounCost && nounCost < nameCost && (nagreeNextTag == -1 || !foundMNoun)) sTagSet = K;
						if (findLowCostTag(subjectTagSets[K], nameCost, L"NAME", natl, parentCost, nextTag) && nameCost < nounCost && nameCost < GNounCost) sTagSet = K;
						// AGREE-SUBJECT (1) TAGSET 00000: #TAGS=2
						// TAGSET 00000: 002 __NOUN[2] 000002 : __N1[](2, 3) TAG N_AGREE[2, 3]
						// TAGSET 00000 : 003 __NOUN[9] 000128 : _REL1[5](3, 7) TAG GNOUN[128, 2]
						// Anyone who sees his friends runs to greet them.
						// NAgree='Anyone' and GNoun='who sees his friends', and subject is really 'Anyone', but that will not have the lowest cost ever in this example because
						// REL1 will have agreement internally and so will be marked lower.
						// In this sentence - Anyone who arrives late are welcome. REL1 is marked as the same cost (GNounCost==nounCost) because who could refer to a plural or a singular entity.
						if (foundGNoun && foundNAgree && !foundMNoun && ntl.sourcePosition + ntl.len == gtl.sourcePosition && GNounCost <= nounCost)
						{
							nounCost = GNounCost - 1;
							if (nounCost < nameCost)
								sTagSet = K;
						}
					}
				}
			}
			else
			{
				startCollectTags(debugTrace.traceSubjectVerbAgreement, subjectTagSet, tagSetSubjectPosition, PEMAOffset, subjectTagSets, true, true, L"GetSubjectInfoSpecificPattern");
				// find lowest cost noun or gnoun reference
				for (unsigned int K = 0; K < subjectTagSets.size(); K++)
				{
					if (debugTrace.traceSubjectVerbAgreement)
						printTagSet(LOG_INFO, L"AGREE-SUBJECT (2)", K, subjectTagSets[K], tagSetSubjectPosition, subjectTag.PEMAOffset);
					// both of the below statements must execute!  (no || )
					int nagreeNextTag, nextTag;
					bool foundMNoun = false;
					if (findLowCostTag(subjectTagSets[K], GNounCost, L"GNOUN", gtl, 0, nextTag) && GNounCost < nounCost && GNounCost < nameCost) sTagSet = K;
					if ((foundMNoun = findLowCostTag(subjectTagSets[K], GNounCost, L"MNOUN", gtl, 0, nextTag)) && GNounCost < nounCost && GNounCost < nameCost) sTagSet = K;
					// if there is an mnoun and 2 nagrees (or more), then don't count the nagree because the mnoun will produce 2 or more nagrees inside it and lowCost tag only counts the cost
					//     of one of the nagrees, so it would win, when clearly the mnoun should win.
					if (findLowCostTag(subjectTagSets[K], nounCost, L"N_AGREE", ntl, 0, nagreeNextTag) && nounCost < GNounCost && nounCost < nameCost && (nagreeNextTag == -1 || !foundMNoun)) sTagSet = K;
					if (findLowCostTag(subjectTagSets[K], nameCost, L"NAME", natl, 0, nextTag) && nameCost < nounCost && nameCost < GNounCost) sTagSet = K;
				}
			}
		}
		if (!subjectTagSets.size())
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:No tags found under subject SUBJECT=%d.", tagSetSubjectPosition, whereSubject);
			return -1;
		}
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d:Final costs for subject tagset: nounCost=%d GNounCost=%d nameCost=%d tagSet#=%d", tagSetSubjectPosition, nounCost, GNounCost, nameCost, sTagSet);
		if (sTagSet == -1)
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:Unable to find consistent tagset for subject. ", tagSetSubjectPosition);
			return -1;
		}
		int nextSingularTag = -1, nextPluralTag = -1, nextRE = -1;
		singularSet = findTag(subjectTagSets[sTagSet], L"SINGULAR", nextSingularTag) >= 0;
		pluralSet = findTag(subjectTagSets[sTagSet], L"PLURAL", nextPluralTag) >= 0;
		restateSet = findTag(subjectTagSets[sTagSet], L"RE_OBJECT", nextRE) >= 0;
		adjectivalSet = !singularSet && GNounCost != 1000000;
		int nextMnounTag=-1, mnounTag = findTag(subjectTagSets[sTagSet], L"MNOUN", nextMnounTag);
		if (nounCost == GNounCost && nameCost > nounCost && ntl.len != gtl.len && mnounTag<0)
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:Tags are inconsistent for subject tagsets ntl.end=%d gtl.end=%d.", tagSetSubjectPosition, ntl.len, gtl.len);
			return -1;
		}
		if (nameCost <= nounCost)
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:Name cost %d is <= nounCost %d.  Setting noun position to -2 and nameLastPosition to %d.", tagSetSubjectPosition, nameCost, nounCost, ntl.sourcePosition + ntl.len - 1);
			nounPosition = -2;
			if (nounCost==nameCost)
				nameLastPosition = ntl.sourcePosition + ntl.len - 1;
			else
				nameLastPosition = natl.sourcePosition + natl.len - 1;
		}
		else if (nounCost <= GNounCost && ntl.len>0)
			nounPosition = ntl.sourcePosition;
		// if NAME tag is totally before N_AGREE tag, then NAME is used as an adjective.  N_AGREE should be used for agreement.
		// How many American Girl dolls have been sold?
		if (nameCost == nounCost && natl.sourcePosition + natl.len <= ntl.sourcePosition && ntl.len > 0)
			nounPosition = ntl.sourcePosition;
		// if singular is set (because mnoun has an 'OR', like 'a mouse or a horse'), but nounPosition>=0 and the noun is plural, then forget about the singular setting and mark as plural.
		if (singularSet && !pluralSet && nounPosition >= 0 && mnounTag >= 0)
		{
			int nextAgreeNounTag=-1;
			findTag(subjectTagSets[sTagSet], L"N_AGREE", nextAgreeNounTag);
			if (nextAgreeNounTag >= 0)
				nounPosition = subjectTagSets[sTagSet][nextAgreeNounTag].sourcePosition;
			pluralSet = (m[nounPosition].word->second.inflectionFlags&PLURAL) == PLURAL;
			singularSet = (m[nounPosition].word->second.inflectionFlags&SINGULAR) == SINGULAR;
			if (debugTrace.traceSubjectVerbAgreement && pluralSet)
				lplog(L"%d:Singular tag overridden in case of 'OR' mnoun where the last object in compound noun is a plural noun.", tagSetSubjectPosition);
		}
	}
	else
		nounPosition = subjectTag.sourcePosition;
	return 0;
}

void getSentenceWithTags(Source source, int patternBegin, int patternEnd, int sentenceBegin, int sentenceEnd, int PEMAPosition, wstring &sentence);

/*
1. Compound subjects made with 'and' are plural when they are used before
the verb and refer to more than one thing:
The lion and the tiger belong to the cat family.
Bach and Beethoven are among the greatest composers of all time.
2. When a compound subject made with 'and' follows the verb, and the
first item in the compound is singular, the verb may agree with that:
There was a desk and three chairs in the room.
Strictly speaking, the verb should agree with both items: There were a desk
and three chairs in the room. But since There were a desk sounds odd, no
matter what follows desk, the verb may agree with desk alone—the first
item. If the first item is plural, the verb always agrees with it:
At the entrance stand two marble pillars and a statue of Napoleon.
3. A compound subject that is made with and and refers to only one
thing is always singular:
The founder and first president of the college was Eleazor Wheelock.
COLLECTIVE NOUNS ( a flock of geese) AND NOUNS OF MEASUREMENT (3/4 of a cup)
Collective nouns and nouns of measurement are singular when they refer
to a unit, and plural when they refer to the individuals or elements of
a unit:
Half of the cake was eaten.
Half of the jewels were stolen.
THE WORD NUMBER AS SUBJECT
The word number is singular when it follows the, plural when it follows a:
The number of applications was huge.
A number of teenagers now hold full-time jobs.
*/
// tagSet is modified during this procedure!  Do not pass by address!
int Source::evaluateSubjectVerbAgreement(patternMatchArray::tPatternMatch *parentpm, patternMatchArray::tPatternMatch *pm, unsigned parentPosition, unsigned int position, vector<tTagLocation> tagSet, int &traceSource)
{
	LFS
	int nextSubjectTag = -1, subjectTag = findTag(tagSet, L"SUBJECT", nextSubjectTag);
	if (subjectTag < 0) return 0;
	int nextMainVerbTag = -1;
	int mainVerbTag = findTag(tagSet, L"VERB", nextMainVerbTag);
	//wstring debugSwitchBack;
	// for the subject reversal, constrain matches to the relativizer itself because otherwise there are 300,000 unique matches in 106 sources and all studied make no sense to reverse.  (see switchcheck debug statement below)
		// if child is a REL, rather than S1, then the pattern is 'A man who fights other soldiers...', and who should not be replaced with soldiers, but rather with 'a man'!  This alternate check is not performed.
	if (subjectTag < mainVerbTag && tagSet[subjectTag].len==1 && patterns[pm->getPattern()]->name == L"__S1")
	{
		// if subject is: there, here, who, what, how, where, whose, when, why, what, and the subject comes before the verb, make the object the subject.
		// There is a book on the table. / There are books on the table.
		// What is the problem ? / What are the problems ?
		// How has the flower grown this quickly ? / How have the flowers grown this quickly ? // the question pattern should assure that the subject is already after the verb!
		set <wstring> reverseSubjects = { L"there", L"here", L"who", L"what", L"how", L"where", L"whose", L"when", L"why", L"what" };
		bool eligibleSubject = reverseSubjects.find(m[tagSet[subjectTag].sourcePosition].word->first) != reverseSubjects.end();
		vector < vector <tTagLocation> > subjectVerbRelationTagSets;
		// use subjectVerbRelationTagSet because it includes subject, verb and object so we can attempt to match with the existing subject verb agreement tagset.
		if (eligibleSubject && startCollectTags(debugTrace.traceSubjectVerbAgreement, subjectVerbRelationTagSet, position, pm->pemaByPatternEnd, subjectVerbRelationTagSets, true, false,L"evaluateSubjectVerbAgreement") > 0)
		{
			for (int svrTagSetIndex = 0; svrTagSetIndex < subjectVerbRelationTagSets.size(); svrTagSetIndex++)
			{
				int nextSVRSubject = -1, whereSVRSubject = findTag(subjectVerbRelationTagSets[svrTagSetIndex], L"SUBJECT", nextSVRSubject);
				int nextSVRMainVerb = -1, whereSVRMainVerb = findTag(subjectVerbRelationTagSets[svrTagSetIndex], L"VERB", nextSVRMainVerb);
				// attempt to match with the existing subject verb agreement tagset.
				if (whereSVRSubject>=0 && whereSVRMainVerb >=0 && mainVerbTag>=0 && subjectVerbRelationTagSets[svrTagSetIndex][whereSVRSubject].sourcePosition == tagSet[subjectTag].sourcePosition && subjectVerbRelationTagSets[svrTagSetIndex][whereSVRMainVerb].sourcePosition == tagSet[mainVerbTag].sourcePosition)
				{
					int nextSVRObject = -1, whereSVRObject = findTag(subjectVerbRelationTagSets[svrTagSetIndex], L"OBJECT", nextSVRObject);
					if (whereSVRObject >= 0 && subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].sourcePosition> tagSet[subjectTag].sourcePosition)
					{
						if (debugTrace.traceSubjectVerbAgreement)
						{
							wstring objectStr, subjectStr;
							lplog(L"%d: object %d:%s substituted for subject %d:%s in here/there/question subject", position,
								subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].sourcePosition,
								phraseString(subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].sourcePosition, subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].sourcePosition + subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].len, objectStr, true).c_str(),
								tagSet[subjectTag].sourcePosition,
								phraseString(tagSet[subjectTag].sourcePosition, tagSet[subjectTag].sourcePosition + tagSet[subjectTag].len, subjectStr, true).c_str());
						}
						// log this to check validity
						//wstring objectStr, subjectStr, verbStr, sentenceStr;
						//debugSwitchBack = L"SWITCHCHECK SUBJECT=" + phraseString(tagSet[subjectTag].sourcePosition, tagSet[subjectTag].sourcePosition + tagSet[subjectTag].len, subjectStr, true) + L" VERB = " +
						//	phraseString(tagSet[mainVerbTag].sourcePosition, tagSet[mainVerbTag].sourcePosition + tagSet[mainVerbTag].len, verbStr, true) + L" OBJECT=" +
						//	phraseString(subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].sourcePosition, subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].sourcePosition + subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].len, objectStr, true) +
						//	L" SENTENCE=" +
						//	phraseString(tagSet[subjectTag].sourcePosition, subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].sourcePosition + subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject].len, sentenceStr, true);
						tagSet[subjectTag] = subjectVerbRelationTagSets[svrTagSetIndex][whereSVRObject];
						break;
					}
				}
			}
		}
	}
	bool restateSet, singularSet, pluralSet,adjectivalSet;
	int nounPosition, nameLastPosition;
	if (getSubjectInfo(tagSet[subjectTag], subjectTag, nounPosition, nameLastPosition, restateSet, singularSet, pluralSet,adjectivalSet) < 0)
		return 0;
	// to block unwanted C1_S1 -> reflexive_pronoun
	//if (tagSet[subjectTag].len == 1)
	//{
		// remove form
		if (m[tagSet[subjectTag].sourcePosition].queryForm(reflexivePronounForm) >= 0)
			return 20;
		// remove pattern
		//	int pattern = tagSet[subjectTag].pattern, subjectPosition = tagSet[subjectTag].sourcePosition, end = tagSet[subjectTag].len, PEMAOffset = tagSet[subjectTag].PEMAOffset;
		//	if (PEMAOffset < 0)
		//	{
		//		for (int p = patterns[pattern]->rootPattern; p >= 0; p = patterns[p]->nextRoot)
		//		{
		//			if (!m[subjectPosition].patterns.isSet(p)) continue;
		//			patternMatchArray::tPatternMatch *pma = m[subjectPosition].pma.find(p, end);
		//			if (pma == NULL) continue;
		//			PEMAOffset = pma->pemaByPatternEnd;
		//			patternElementMatchArray::tPatternElementMatch *pem = pema.begin() + PEMAOffset;
		//			for (; PEMAOffset >= 0 && pem->getPattern() == p && pem->end == end; PEMAOffset = pem->nextByPatternEnd, pem = pema.begin() + PEMAOffset)
		//				if (!pem->begin) break;
		//			if (PEMAOffset < 0 || pem->getPattern() != p || pem->end != end || pem->begin) continue;
		//			if (patterns[pema[PEMAOffset].getPattern()]->name == L"_ADJECTIVE")
		//				return 20; // adjective is allowed as a C1_S1, but lets see whether it is really needed!
		//		}
		//	}
	//}
	// evaluate if SUBJECT is an _ADJECTIVE and ALSO a _NAME.  This is not allowed as a subject
	// A _NAME is allowed as an adjective (Dover Street Pub), and an _ADJECTIVE is allowed as a subject (The poor)
	// but a _NAME as a subject should be an OBJECT, not classified as an ADJECTIVE.
	// This allows _ADJECTIVE to be general, and still disallows _S1[4] from using a _NAME as subject.
	// Many[annie] ishas the time Annie ishas said to me[albert] :
	//wchar_t temp2[1024];
	//if (tagSet[subjectTag].PEMAOffset>=0 && pema[tagSet[subjectTag].PEMAOffset].getPattern()>=0)
	//	lplog(L"%d:SUBJECT %s",tagSet[subjectTag].sourcePosition,pema[tagSet[subjectTag].PEMAOffset].toText(tagSet[subjectTag].sourcePosition,temp2,m));
	int nextConditionalTag = -1, nextFutureTag = -1;
	int nextVerbAgreeTag = -1, verbAgreeTag=(mainVerbTag>=0) ? findTagConstrained(tagSet,L"V_AGREE",nextVerbAgreeTag,tagSet[mainVerbTag]) : -1;
	int conditionalTag=(mainVerbTag>=0) ? findTagConstrained(tagSet,L"conditional",nextConditionalTag,tagSet[mainVerbTag]) : -1;
	int pastTag=(mainVerbTag>=0) ? findTagConstrained(tagSet,L"past",nextConditionalTag,tagSet[mainVerbTag]) : -1;
	int futureTag=(mainVerbTag>=0) ? findTagConstrained(tagSet,L"future",nextFutureTag,tagSet[mainVerbTag]) : -1;
	// St. Pancras? - Pancras is an unknown, so verb is allowed.  Yet Pancras is after a ., so capitalization is allowed.
	// disallow this case.
	if (subjectTag>=0 && verbAgreeTag>=0 && tagSet[verbAgreeTag].sourcePosition>tagSet[subjectTag].sourcePosition && 
			(m[tagSet[verbAgreeTag].sourcePosition].flags&WordMatch::flagFirstLetterCapitalized) &&
			m[tagSet[subjectTag].sourcePosition+tagSet[subjectTag].len-1].word->first==L".")
	{
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d: verb capitalized [SOURCE=%06d] cost=%d",position,traceSource=gTraceSource++,20);
		return 20;
	}
	// his sacrificed ambition / his is modifying a past verb which serves as an adjective
	if (mainVerbTag>=0 && subjectTag>=0 && nextSubjectTag<0 && tagSet[subjectTag].len==1 && m[tagSet[subjectTag].sourcePosition].queryForm(possessiveDeterminerForm)!=-1 &&
		  nextVerbAgreeTag<0 && tagSet[mainVerbTag].len==1 && // &&
		tagSet[subjectTag].sourcePosition== tagSet[mainVerbTag].sourcePosition-1 && 
		(m[tagSet[mainVerbTag].sourcePosition].queryForm(nounForm)!=-1 || 
		((m[tagSet[mainVerbTag].sourcePosition].word->second.inflectionFlags&VERB_PAST) == VERB_PAST && m[tagSet[mainVerbTag].sourcePosition+1].queryForm(nounForm) != -1)))
		//if (nextSubjectTag < 0 && tagSet[subjectTag].len == 1 && m[tagSet[subjectTag].sourcePosition].queryForm(possessiveDeterminerForm) != -1 &&
		//	nextVerbAgreeTag < 0 && tagSet[mainVerbTag].len == 1 && (m[tagSet[mainVerbTag].sourcePosition].word->second.inflectionFlags&VERB_PAST) != -1 &&
		//	tagSet[subjectTag].sourcePosition == tagSet[mainVerbTag].sourcePosition - 1 && m[tagSet[mainVerbTag].sourcePosition + 1].queryForm(nounForm) != -1)
	{
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d: use of possessive determiner as subject [SOURCE=%06d] cost=%d", position, traceSource = gTraceSource++, 6);
		//int patternEnd = position + pm->len;
		//wstring sentence;
		//int ss = sentenceStarts.binary_search_lower_bound(position);
		//getSentenceWithTags(*this, position, patternEnd, sentenceStarts[ss], sentenceStarts[ss+1], pm->pemaByPatternEnd, sentence);
		//lplog(L"ZZCHECK %d:%s",position,sentence.c_str());
		return 6;
	}
	//if (mainVerbTag >= 0 && subjectTag >= 0)
	//	lplog(L"ZZCHECK %d:%d<0 %d==1 %d!=-1 %d<0 %d==1 %d==%d-1 %d!=-1", position, nextSubjectTag, tagSet[subjectTag].len, m[tagSet[subjectTag].sourcePosition].queryForm(possessiveDeterminerForm),
	//		nextVerbAgreeTag, tagSet[mainVerbTag].len, tagSet[subjectTag].sourcePosition, tagSet[mainVerbTag].sourcePosition, m[tagSet[mainVerbTag].sourcePosition].queryForm(nounForm));
	if ((conditionalTag<0 && futureTag<0 && verbAgreeTag<0) || subjectTag<0 || mainVerbTag<0 || nextSubjectTag>=0)
	{
		// check for question
		if (subjectTag>=0 && mainVerbTag<0 && (verbAgreeTag=findTag(tagSet,L"V_AGREE",nextVerbAgreeTag))>=0 && nextVerbAgreeTag>=0 &&
			tagSet[verbAgreeTag].sourcePosition<tagSet[subjectTag].sourcePosition &&
			tagSet[subjectTag].sourcePosition<tagSet[nextVerbAgreeTag].sourcePosition)
		{
			conditionalTag=findTagConstrained(tagSet,L"conditional",nextConditionalTag,tagSet[verbAgreeTag]);
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:Question detected. V_AGREE=(%d,%d) SUBJECT=%d nounPosition=%d conditionalTag=%d",position,verbAgreeTag,nextVerbAgreeTag,subjectTag,nounPosition,conditionalTag);
		}
		else
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:Search for noun and verb tag returned V_AGREE tag=(%d,%d) SUBJECT tag=(%d,%d) VERB tag=(%d,%d) [SOURCE=%06d] cost=%d",
								position,verbAgreeTag,nextVerbAgreeTag,subjectTag,nextSubjectTag,mainVerbTag,nextMainVerbTag, traceSource = gTraceSource++, 0);
			return 0;
		}
	}
	int relationCost=0;
	tFI::cRMap *rm;
	int verbPosition=(verbAgreeTag>=0) ? tagSet[verbAgreeTag].sourcePosition : -1;
	// prevent _INTRO_S1 that matches a _PP like 'from her' and a subject like 'face' when it should be 'from her face' - This particular intro_S1 should not be allowed 
	// But this , she knew , was not so , for her face was perfectly unharmed ; 
	// position must = nounPosition because nounPosition = the main noun, which may have a determiner which is perfectly normal (and in this case position!=nounPosition)
	if (nounPosition >= 1 && position==nounPosition && m[nounPosition].queryForm(nounForm) !=-1 && m[nounPosition - 1].queryForm(possessiveDeterminerForm) !=-1)
	{
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d:Noun subject %d:%s is preceeded by a possessive determiner %d:%s",
				position, nounPosition, m[nounPosition].word->first.c_str(),nounPosition-1,m[nounPosition-1].word->first.c_str());
		relationCost += 10;
	}
	if ((nounPosition>=0 || nounPosition==-2) && verbPosition>=0)
	{
		tIWMM nounWord,verbWord;
		int nextObjectVerbTag=-1,whereObjectVerbTag=(mainVerbTag>=0) ? findTagConstrained(tagSet,L"V_OBJECT",nextObjectVerbTag,tagSet[mainVerbTag]) : -1;
		//lplog(L"%d:%s %s", verbPosition, m[verbPosition].word->first.c_str(), patterns[tagSet[verbAgreeTag].parentPattern]->name.c_str());
		if (patterns[tagSet[verbAgreeTag].parentPattern]->name==L"_VERB_BARE_INF") // if this is a _VERB_BARE_INF - take the first verb
			verbWord = m[verbPosition].word;
		else 
		if (nextObjectVerbTag>=0)
			verbWord=m[tagSet[nextObjectVerbTag].sourcePosition].word;
		else if (whereObjectVerbTag>=0)
			verbWord=m[tagSet[whereObjectVerbTag].sourcePosition].word;
		else if (nextVerbAgreeTag>=0)
			verbWord=m[tagSet[nextVerbAgreeTag].sourcePosition].word; // get the verb last V_AGREE if there is no V_OBJECT
		else
			verbWord=m[verbPosition].word;
		if (nounPosition==-2)
			nounWord=Words.PPN;
		else
			nounWord=resolveToClass(nounPosition);
		if (verbWord->second.mainEntry!=wNULL)
			verbWord=verbWord->second.mainEntry;
		tFI::cRMap::tIcRMap tr=tNULL;
		if ((rm=nounWord->second.relationMaps[SubjectWordWithVerb]) && ((tr=rm->r.find(verbWord))!=rm->r.end()))
			relationCost-=COST_PER_RELATION;
		else // Whom did Obama defeat for the Senate seat? - Obama is a single word, so it never gets a name (nounPosition=-2) designation, and so never tries __ppn__->verb relation
			if (nounWord!=Words.PPN && tagSet[subjectTag].len==1 && (m[nounPosition].flags&WordMatch::flagFirstLetterCapitalized) &&
			    (rm=Words.PPN->second.relationMaps[SubjectWordWithVerb]) && (tr=rm->r.find(verbWord))!=rm->r.end())
			relationCost-=COST_PER_RELATION;
		if (debugTrace.traceSubjectVerbAgreement)
		{
			wchar_t temp[1024];
			int len=(parentpm) ? wsprintf(temp,L"%s[%s](%d,%d) ",patterns[parentpm->getPattern()]->name.c_str(),patterns[parentpm->getPattern()]->differentiator.c_str(),parentPosition,parentpm->len+parentPosition) : 0;
			wsprintf(temp+len,L"%s[%s](%d,%d) ",patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position);
			if (!rm || tr==rm->r.end())
				lplog(L"%s %d:Subject '%s' has NO SubjectWordWithVerb relationship with '%s'.",
							temp,nounPosition,nounWord->first.c_str(),verbWord->first.c_str());
			else
				lplog(L"%s %d:Subject '%s' has %d SubjectWordWithVerb relationship count with '%s'.",
							temp,nounPosition,nounWord->first.c_str(),tr->second.frequency,verbWord->first.c_str());
		}
	}
	if (nounPosition>=0 && m[nounPosition].queryForm(accForm) >= 0)
	{
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d:Noun phrase at %d is accusative pronoun [additional cost of 4]!", position, nounPosition);
		//relationCost += 4; must be carefully tested
	}
	if (restateSet && relationCost)
	{
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"Subject restated.  cost %d = %d/subject length=%d.",relationCost/tagSet[subjectTag].len,relationCost,tagSet[subjectTag].len);
		relationCost/=tagSet[subjectTag].len;
	}
	if (conditionalTag>=0 || futureTag>=0) // he will have, they will have etc.
	{
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d: conditional agree [SOURCE=%06d] cost=%d",position,traceSource=gTraceSource++,relationCost);
		return relationCost;
	}
	//int nextPersonTag=-1;
	//  bool thirdPersonSet=(subjectTag>=0) ? findTagConstrained(tagSet,"THIRD_PERSON",nextPersonTag,tagSet[subjectTag])>=0 : false;
	int person=THIRD_PERSON;
	// if third_person not already set,
	if (nounPosition >= 0)
	{
		// SANAM
		// substitute the object of the preposition
		set <wstring> SANAM = { L"some", L"any", L"none", L"all", L"most" };
		if (SANAM.find(m[nounPosition].word->first) != SANAM.end() && nounPosition+1<m.size() && m[nounPosition+1].word->first==L"of")
		{
			singularSet=pluralSet = false;
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d:SANAM detected: tracing immediately proceeding prepositional phrase.", nounPosition);
			// substitute the object of the preposition.  We cannot use the relPrep or other syntactic relations fields because they are not set yet at this stage.
			// Some of the debt is paid off.
			// Some of the debts are being paid off.
			for (unsigned int pmOffset = 0; pmOffset < m[nounPosition + 1].pma.count; pmOffset++)
				if (patterns[m[nounPosition + 1].pma.content[pmOffset].getPattern()]->name == L"_PP")
				{
					vector < vector <tTagLocation> > prepTagSets;
					if (startCollectTags(debugTrace.traceSubjectVerbAgreement, prepTagSet, nounPosition + 1, m[nounPosition + 1].pma.content[pmOffset].pemaByPatternEnd, prepTagSets, true, false,L"evaluateSubjectVerbAgreement SANAM") > 0)
					{
						for (unsigned int J = 0; J < prepTagSets.size(); J++)
						{
							int nextTag=-1, tag = findTag(prepTagSets[J], L"PREPOBJECT", nextTag);
							vector < vector <tTagLocation> > ndTagSets;
							if (tag >= 0 && startCollectTagsFromTag(debugTrace.traceSubjectVerbAgreement, nounDeterminerTagSet, prepTagSets[J][tag], ndTagSets, -1, true, true, L"subject verb agreement - SANAM") > 0)
							{
								if (debugTrace.traceSubjectVerbAgreement)
									lplog(L"%d:SANAM detection: prepobject at %d-%d.", nounPosition,prepTagSets[J][tag].sourcePosition, prepTagSets[J][tag].sourcePosition + prepTagSets[J][tag].len);
								for (unsigned int K = 0; K < ndTagSets.size(); K++)
								{
									int nounTag = -1, nextNounTag = -1, nAgreeTag = -1, nextNAgreeTag = -1;
									if ((nounTag = findTag(ndTagSets[K], L"NOUN", nextNounTag)) >= 0 && (nAgreeTag = findTagConstrained(ndTagSets[K], L"N_AGREE", nextNAgreeTag, ndTagSets[K][nounTag])) > 0)
									{
										if (debugTrace.traceSubjectVerbAgreement)
											lplog(L"%d:SANAM detection: N_AGREE within prepobject %d-%d located at %d.", nounPosition, prepTagSets[J][tag].sourcePosition, prepTagSets[J][tag].sourcePosition + prepTagSets[J][tag].len, ndTagSets[K][nAgreeTag].sourcePosition);
										tIWMM nounWord = m[nounPosition = ndTagSets[K][nAgreeTag].sourcePosition].word;
										if (nounWord->second.inflectionFlags&SINGULAR)
										{
											if (debugTrace.traceSubjectVerbAgreement)
												lplog(L"%d:noun %s is singular.", nounPosition, nounWord->first.c_str());
											singularSet = true;
										}
										if (nounWord->second.inflectionFlags&PLURAL)
										{
											if (debugTrace.traceSubjectVerbAgreement)
												lplog(L"%d:noun %s is plural.", nounPosition, nounWord->first.c_str());
											pluralSet = true;
										}
									}
								}
							}
						}
					}
				}
			
		}
		person = m[nounPosition].word->second.inflectionFlags&(FIRST_PERSON | SECOND_PERSON | THIRD_PERSON);
	}
	if (!person)
		person=THIRD_PERSON;
	// if singular | plural not already set
	// words like "there" may be plural or singular depending on usage
	int inflectionFlags = m[verbPosition].word->second.inflectionFlags&VERB_INFLECTIONS_MASK;
	//wstring verbInflections;
	//lplog(L"%d:%s %s", verbPosition, m[verbPosition].word->first.c_str(), getInflectionName(inflectionFlags, verbInflectionMap, verbInflections));
	if (!singularSet && !pluralSet && nounPosition>=0)
	{
		pluralSet=(m[nounPosition].word->second.inflectionFlags&PLURAL)==PLURAL;
		singularSet=(m[nounPosition].word->second.inflectionFlags&SINGULAR)==SINGULAR;
		// a proper noun is very rarely plural - Tuppence should only be considered singular
		// special rules for multiple proper nouns needed in future (two ICBMs)
		if (pluralSet && singularSet && (m[nounPosition].flags&WordMatch::flagOnlyConsiderProperNounForms) && 
			  (person!=THIRD_PERSON || ((inflectionFlags&VERB_PRESENT_PLURAL)!= VERB_PRESENT_PLURAL && (inflectionFlags&VERB_PAST_PLURAL)!= VERB_PAST_PLURAL)))
			pluralSet=false;
		if (m[nounPosition].word->first==L"who" || // those who were OR this person who was
			  m[nounPosition].word->first==L"that") // the settlement that was OR the settlements that were
			singularSet=pluralSet=true;
	}
	// The Prince of Asturias Awards (Spanish) are a series awarded in Spain.
	int np=nounPosition;
	if (np==-2 && nameLastPosition>=0)
	{
		// The above Books are among a large collection
		np = nameLastPosition;
		if ((m[np].flags&WordMatch::flagOnlyConsiderProperNounForms) &&
				(m[np].word->second.inflectionFlags&PLURAL) == PLURAL && 
				(person == THIRD_PERSON && (inflectionFlags == VERB_PRESENT_PLURAL || inflectionFlags == VERB_PAST_PLURAL)))
			pluralSet = true;
	}
	if (np>=0 && np+4<(signed)m.size() && !pluralSet && 
		  (inflectionFlags==VERB_PRESENT_PLURAL || inflectionFlags==VERB_PAST_PLURAL) && 
		  (m[np].flags&WordMatch::flagFirstLetterCapitalized) && m[np+1].word->first==L"of" && 
		  (m[np+2].flags&WordMatch::flagFirstLetterCapitalized) && (m[np+3].flags&WordMatch::flagFirstLetterCapitalized) &&
			(m[np+3].word->second.inflectionFlags&PLURAL)==PLURAL)
	{
		singularSet=false;
		pluralSet=true;
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d:Singular name with direct capitalized prepositional phrase with plural last object [name is adjective] leads to switched agreement.",position);
	}
	// The Men of Yore Briefcase is essential for every trip.
	if (np>=0 && np+4<(signed)m.size() && pluralSet && 
		  ((inflectionFlags&VERB_PRESENT_FIRST_SINGULAR) || (inflectionFlags&VERB_PRESENT_THIRD_SINGULAR)) && 
		  (m[np].flags&WordMatch::flagFirstLetterCapitalized) && m[np+1].word->first==L"of" && 
		  (m[np+2].flags&WordMatch::flagFirstLetterCapitalized) && (m[np+3].flags&WordMatch::flagFirstLetterCapitalized) &&
			(m[np+3].word->second.inflectionFlags&SINGULAR)==SINGULAR)
	{
		singularSet=true;
		pluralSet=false;
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d:Plural name with direct capitalized prepositional phrase with singular last object [name is adjective] leads to switched agreement.",position);
	}
	if (!singularSet && !pluralSet)
		singularSet=true;
	if (debugTrace.traceSubjectVerbAgreement)
	{
		wstring temp;
		lplog(L"%d:Noun phrase at %d was %s [plural=%s singular=%s]", position, nounPosition, getInflectionName(person, nounInflectionMap, temp), (pluralSet) ? L"true" : L"false", (singularSet) ? L"true" : L"false");
	}
	bool ambiguousTense;
	// the verb 'beat' is both past and present tense
	if (ambiguousTense=(inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST))==(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST))
	{
		if (pastTag>=0) inflectionFlags&=VERB_PAST_PARTICIPLE|VERB_PAST|VERB_PAST_PLURAL;
		else inflectionFlags&=VERB_PRESENT_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL;
	}
	if (debugTrace.traceSubjectVerbAgreement)
	{
		wstring temp;
		lplog(L"%d:Verb phrase at (%d:%d-%d) had mask of %s.", position, verbPosition,
			tagSet[mainVerbTag].sourcePosition, tagSet[mainVerbTag].len + tagSet[mainVerbTag].sourcePosition,
			getInflectionName(inflectionFlags, verbInflectionMap, temp));
	}
	//	The present subjunctive is identical to the bare infinitive(and imperative) of the verb in all forms.
	// This means that, for almost all verbs, the present subjunctive differs from the present indicative only in the 
	// third person singular form, which lacks the ending - s in the subjunctive.

	//Present indicative
	//	I own, you own, he owns, we own, they own
	//Present subjunctive
	//	(that) I own, (that)you own, (that)he own, (that)we own, (that)they own

	//	With the verb be, however, the two moods are fully distinguished :
	//Present indicative
	//	I am, you are, he is, we are, they are
	//Present subjunctive
	//	(that) I be, (that)you be, (that)he be, (that)we be, (that)they be
	//	Note also the defective verb beware, which lacks indicative forms, but has a present subjunctive : (that)she beware…

	//The two moods are also fully distinguished when negated.
	//Present subjunctive forms are negated by placing the word not before them.

	//Present indicative
	//	I do not own, you do not own, he does not own…; I am not…
	//Present subjunctive
	//	(that) I not own, (that)you not own, (that)he not own…; (that)I not be…

	//	The past subjunctive exists as a distinct form only for the verb be, which has the form were throughout :
	//Past indicative
	//	I was, you were, he was, we were, they were
	//Past subjunctive
	//	(that) I were, (that)you were, (that)he were, (that)we were, (that)they were

	//In the past tense, there is no difference between the two moods as regards manner of negation : I was not; (that)I were not.  
	//Verbs other than be are described as lacking a past subjunctive, 
	//or possibly as having a past subjunctive identical in form to the past indicative : (that)I owned; (that)I did not own.

	//Certain subjunctives(particularly were) can also be distinguished from indicatives by the possibility of inversion with the subject.

	bool subjunctiveMood = findOneTag(tagSet, L"SUBJUNCTIVE")>=0;
	// if/lest exception
	// If I were a rich man, I would make more charitable donations.
	// If he were here right now, he would help us.
	// this exception applies only to unreal conditionals—that is, situations that do not reflect reality. (Hint: unreal conditionals often contain words like “would” or “ought to.”) 
	// When talking about a possibility that did happen or might be true, “was” and “were” are used normally.
	// If I was rude to you, I apologize.
	// wishing he were here right now
	// ‘ I am exactly the same , ’ Catherine repeated , wishing her aunt were a little less sympathetic .
	if (tagSet[subjectTag].sourcePosition > 1 &&
		(m[tagSet[subjectTag].sourcePosition - 1].word->first == L"if" || m[tagSet[subjectTag].sourcePosition - 1].word->first == L"lest" ||
			m[tagSet[subjectTag].sourcePosition - 1].queryForm(thinkForm) != -1))
	{
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d: if/lest/SYNTAX:Accepts S as Object exception triggers subjunctive mood", position);
		subjunctiveMood = true;
	}
	// if the subjunctive tag is found, it means that the subjunctive mood MAY BE used, not that it is definitely being used.
	if (subjunctiveMood)
	{
		// past subjunctive for "be" = 'were' present subjunctive for "be" = 'be'
		if (verbPosition >= 0 && (m[verbPosition].word->first == L"were" || m[verbPosition].word->first == L"be"))
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d: subjunctive tag agreement 1) verb is 'were' or 'be'.", position);
			return relationCost;
		}
		// ambiguous for third person - subjunctive mood disagrees with non subjunctive 
		if (person&THIRD_PERSON)
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d: subjunctive tag agreement 2) subject is third person.", position);
			return relationCost;
		}
		if (findOneTag(tagSet, L"not") >= 0)
		{
			if (debugTrace.traceSubjectVerbAgreement)
				lplog(L"%d: subjunctive tag agreement 3) verb negated.", position);
			return relationCost;
		}
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d: subjunctive mood failed to find a case where it would make a difference in agreement.", position);
	}
	bool agree=true;
	switch(inflectionFlags)
	{
	case VERB_PRESENT_PARTICIPLE:
	case VERB_PAST_PARTICIPLE:
	case VERB_PAST:break;
	case VERB_PRESENT_FIRST_SINGULAR: // only FIRST_SINGULAR !
		// you want, I want, they want, we want NOT he want!
		if ((person&THIRD_PERSON)==THIRD_PERSON && (singularSet && !pluralSet)) agree=false;
		if (((person&SECOND_PERSON) || (person&THIRD_PERSON)) && m[verbPosition].word->first==L"am")
			agree=false;
		break;
	case VERB_PRESENT_THIRD_SINGULAR:
		// he wants ONLY
		if ((person&THIRD_PERSON)!=THIRD_PERSON || (!singularSet && pluralSet)) agree=false;
		break;
	case VERB_PRESENT_PLURAL:
	case VERB_PAST_PLURAL:
		if (((person&THIRD_PERSON)!=THIRD_PERSON || (singularSet && !pluralSet)) &&
			(person&SECOND_PERSON)!=SECOND_PERSON && (person!=(FIRST_PERSON|SECOND_PERSON))) agree=false;
		break;
	case VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL:
		if (((person&THIRD_PERSON)!=THIRD_PERSON || (singularSet && !pluralSet)) && (person&FIRST_PERSON)!=FIRST_PERSON &&
			(person&SECOND_PERSON)!=SECOND_PERSON && (person!=(FIRST_PERSON|SECOND_PERSON))) agree=false;
		break;
	case VERB_PAST|VERB_PAST_PARTICIPLE:
		break;
	case VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR:
		break;
	case VERB_PAST_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR:
		break;
	case VERB_PRESENT_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR:
		break;
	case VERB_PAST|VERB_PRESENT_THIRD_SINGULAR:
		break;
	case VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PAST_PLURAL:
		break;
	case VERB_PAST|VERB_PRESENT_PARTICIPLE|VERB_PRESENT_THIRD_SINGULAR:
		break;
	case VERB_PRESENT_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR:
		break;
	case VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL:
		break;
	case VERB_PAST|VERB_PAST_PARTICIPLE|VERB_PRESENT_PARTICIPLE:
	case VERB_PAST|VERB_PRESENT_PARTICIPLE:
	case VERB_PAST_PARTICIPLE|VERB_PRESENT_PARTICIPLE:
	case VERB_PAST_THIRD_SINGULAR:
	case VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR:
	case VERB_PRESENT_PARTICIPLE|VERB_PRESENT_THIRD_SINGULAR:
	case VERB_PRESENT_SECOND_SINGULAR:
	case VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_PLURAL:
	case 0:
		break;
	default:
		{
			wstring temp;
			lplog(LOG_ERROR, L"inflectionFlags of %s (%d) on %s not supported!", getInflectionName(inflectionFlags, verbForm, temp), inflectionFlags, m[verbPosition].word->first.c_str());
		}
	}
	int cost=((agree) ? 0 : NON_AGREEMENT_COST)+relationCost;
	if (!agree && ambiguousTense && verbAgreeTag>=0 && conditionalTag<0 && 
			// avoid stomping on part of another pattern 'my millionaire would probably run for his life!'
			(m[verbPosition].word->second.query(modalAuxiliaryForm)<0 && m[verbPosition].word->second.query(futureModalAuxiliaryForm)<0 && 
			 m[verbPosition].word->second.query(negationModalAuxiliaryForm)<0 && m[verbPosition].word->second.query(negationFutureModalAuxiliaryForm)<0))
	{
		//pema[abs(tagSet[whereVerbTag].PEMAOffset)].addOCostTillMax(1);
		pema[abs(tagSet[mainVerbTag].PEMAOffset)].addOCostTillMax(1);
		if (debugTrace.traceSubjectVerbAgreement)
			lplog(L"%d: Disagreement at ambiguous tense PEMA %d[%d] [SOURCE=%06d] increased cost=1",position,abs(tagSet[verbAgreeTag].PEMAOffset),abs(tagSet[mainVerbTag].PEMAOffset),gTraceSource);
	}
	if (debugTrace.traceSubjectVerbAgreement)
		lplog(L"%d: %s [SOURCE=%06d] cost=%d",position,(agree) ? L"agree" : L"DISAGREE",traceSource=gTraceSource++,cost);
	//if (!agree && debugSwitchBack.length()>0)
	//	lplog(LOG_INFO, debugSwitchBack.c_str());
	return cost;
}

// FIRST_PERSON - I / we
// SECOND_PERSON - you
// THIRD_PERSON - he/she/it/they
bool Source::evaluateSubjectVerbAgreement(int verbPosition,int whereSubject,bool &agreementTestable)
{ LFS
	if (whereSubject < 0)
		return true;
	int person = m[whereSubject].word->second.inflectionFlags&(FIRST_PERSON | SECOND_PERSON | THIRD_PERSON);
	if (!person)
		person = THIRD_PERSON;
	int subjectNounInflectionFlags = m[whereSubject].word->second.inflectionFlags&(PLURAL | SINGULAR);
	bool agree=true;
	agreementTestable = true;
	switch(m[verbPosition].word->second.inflectionFlags&VERB_INFLECTIONS_MASK)
	{
	case VERB_PRESENT_PARTICIPLE:  // showing
	case VERB_PAST_PARTICIPLE: // shown
	case VERB_PAST: // showed
		agreementTestable = false;
		break;
	case VERB_PRESENT_FIRST_SINGULAR: // only FIRST_SINGULAR !
		// show 
		// you want, I want, they want, we want NOT he/she/it want!
		if (((person&THIRD_PERSON)==THIRD_PERSON) && (subjectNounInflectionFlags&SINGULAR) == SINGULAR)
			agree=false;
		if (m[whereSubject].word->first!=L"i" && m[verbPosition].word->first==L"am")
			agree=false;
		break;
	case VERB_PRESENT_THIRD_SINGULAR:
		// shows
		// he/she/it wants ONLY
		if (((person&THIRD_PERSON)!=THIRD_PERSON) || (subjectNounInflectionFlags&PLURAL)==PLURAL)
			agree=false;
		break;
	case VERB_PRESENT_PLURAL: // are
	case VERB_PAST_PLURAL: // were
		// NOT I/he/she/it
		if ((subjectNounInflectionFlags&PLURAL) != PLURAL)
			agree=false;
		break;
	case VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL: // they have/I have/we have/you have
		if (((person&THIRD_PERSON) == THIRD_PERSON) && (subjectNounInflectionFlags&SINGULAR) == SINGULAR)
			agree = false;
		break;
	}
	return agree;
}

// eliminate patterns based on a little semantic help
// this was created to help speaker resolution, after a closed quote or a comma
// this is very helpful when a word is both a verb and an adjective, so that the pattern is very ambiguous
// see also: evaluateVerbObjects
bool Source::preferVerbRel(int position,unsigned int J,cPattern *p)
{ LFS
	vector <WordMatch>::iterator im=m.begin()+position-1;
	if (!position || (im->word->first!=L"," &&
		(!im->forms.isSet(quoteForm) || (im->word->second.inflectionFlags&CLOSE_INFLECTION)!=CLOSE_INFLECTION)))
		return true;
	im=m.begin()+position;
	// reject a _NOUN match when a VERBREL1 matches in the same place, over the same positions
	// and the first word has a _VERBPAST, and the previous word is a quotes.
	if (patterns[im->pma[J].getPattern()]->name!=L"__NOUN" && patterns[im->pma[J].getPattern()]->name!=L"__MNOUN") return true;
	int nounAvgCost=im->pma[J].getAverageCost(),element,elementVP;
	if ((element=im->pma.queryPatternWithLen(L"_VERBREL1",im->pma[J].len))!=-1 &&
		im->pma.findMaxLen(L"_VERBPAST",elementVP)==true && nounAvgCost>=im->pma[element&~matchElement::patternFlag].getAverageCost())
	{
		if (debugTrace.tracePatternElimination)
			lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) is not a winner (VERBREL preference).",position,J,
			p->name.c_str(),p->differentiator.c_str(),position,im->pma[J].len+position);
		return false;
	}
	return true;
}

// eliminate patterns based on a little semantic help
// this was created to help speaker resolution
// this is very helpful when a word is both a verb and an adjective, so that the pattern is very ambiguous
bool Source::preferS1(int position,unsigned int J)
{ LFS
	vector <WordMatch>::iterator im=m.begin()+position;
	cPattern *p=patterns[im->pma[J].getPattern()];
	// also reject if an _S1 occupies the same elements and is the same or less in cost.
	if (patterns[im->pma[J].getPattern()]->name!=L"__NOUN" && patterns[im->pma[J].getPattern()]->name!=L"__MNOUN") return false;
	int nounAvgCost=im->pma[J].getAverageCost(),element;
	if ((element=im->pma.queryPatternWithLen(L"__S1",im->pma[J].len))==-1 ||
		nounAvgCost<im->pma[element&~matchElement::patternFlag].getAverageCost()) return false;
	if (debugTrace.tracePatternElimination)
		lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) is not a winner (S1 preference) nounAvgCost=%d >= S1Cost %d.",position,J,
		p->name.c_str(),p->differentiator.c_str(),position,im->pma[J].len+position,nounAvgCost,im->pma[element&~matchElement::patternFlag].getAverageCost());
	return true;
}

wchar_t *flagStr(int flag)
{ LFS
	switch (flag)
	{
	case WordMatch::flagBNCPreferAdverbPatternMatch:return L"Adverb";
	case WordMatch::flagBNCPreferAdjectivePatternMatch:return L"Adjective";
	case WordMatch::flagBNCPreferNounPatternMatch:return L"Noun";
	case WordMatch::flagBNCPreferVerbPatternMatch:return L"Verb";
	}
	return NULL;
}

int Source::evaluateBNCPreferenceForPosition(int position,int patternPreference,int flag,bool remove)
{ LFS
	if (position<0 || position>=(int)m.size())
		lplog(LOG_FATAL_ERROR,L"evaluateBNCPreferences - bad data!");
	int cost=0;
	if ((m[position].flags&flag) && patternPreference!=flag)
	{
		if (!(m[position].flags&WordMatch::flagBNCPreferIgnore))
		{
#ifdef LOG_BNC_PATTERNS_CHECK
			lplog(L"%d:Pattern matched is %s, but BNC preference is %s.",position,flagStr(patternPreference),flagStr(flag));
#endif
			cost+=4;
		}
#ifdef LOG_BNC_PATTERNS_CHECK
		else
			lplog(L"%d:Pattern matched is %s, but BNC preference is %s (IGNORED).",position,flagStr(patternPreference),flagStr(flag));
#endif
	}
	if (remove && (m[position].flags&patternPreference))
	{
		m[position].flags|=WordMatch::flagBNCPreferIgnore;
#ifdef LOG_BNC_PATTERNS_CHECK
		lplog(L"Setting position %d to ignore.",position);
#endif
	}
	return cost;
}

int Source::evaluateBNCPreference(vector <tTagLocation> &tagSet,wchar_t *tag,int patternPreference,bool remove)
{ LFS
	int cost=0;
	for (unsigned int I=0; I<tagSet.size(); I++)
		if (patternTagStrings[tagSet[I].tag]==tag)
		{
#ifdef LOG_BNC_PATTERNS_CHECK
			lplog(L"%s:%d-%d",tag,tagSet[I].sourcePosition,tagSet[I].sourcePosition+tagSet[I].end);
#endif
			for (unsigned int position=tagSet[I].sourcePosition; position<tagSet[I].sourcePosition+tagSet[I].len; position++)
			{
				cost+=
					evaluateBNCPreferenceForPosition(position,patternPreference,WordMatch::flagBNCPreferAdverbPatternMatch,remove) +
					evaluateBNCPreferenceForPosition(position,patternPreference,WordMatch::flagBNCPreferAdjectivePatternMatch,remove) +
					evaluateBNCPreferenceForPosition(position,patternPreference,WordMatch::flagBNCPreferNounPatternMatch,remove) +
					evaluateBNCPreferenceForPosition(position,patternPreference,WordMatch::flagBNCPreferVerbPatternMatch,remove);
			}
		}
		return cost;
}

// check that the areas of the ADJ tags are not flagged by noun, verb or adverb preferences, and if an adjective preference
// is there, also remove that.
// check that the areas of the ADV tags are not flagged by noun, verb or adjective preferences, and if an adverb preference
// is there, also remove that.
// check that the areas of the NOUN tags are not flagged by adjective, adverb, or verb preferences
// check that the areas of the VERB tags are not flagged by adjective, adverb, or noun preferences
int Source::evaluateBNCPreferences(int position,int PEMAPosition,vector <tTagLocation> &tagSet)
{ LFS
	int cost=evaluateBNCPreference(tagSet,L"ADV",WordMatch::flagBNCPreferAdverbPatternMatch,true);
	cost+=evaluateBNCPreference(tagSet,L"ADJ",WordMatch::flagBNCPreferAdjectivePatternMatch,true);
	cost+=evaluateBNCPreference(tagSet,L"NOUN",WordMatch::flagBNCPreferNounPatternMatch,false);
	cost+=evaluateBNCPreference(tagSet,L"VERB",WordMatch::flagBNCPreferVerbPatternMatch,false);
#ifdef LOG_BNC_PATTERNS_CHECK
	lplog(L"Erasing positions %d-%d to ignore.",pema[PEMAPosition].begin+position,pema[PEMAPosition].end+position);
#endif
	if (PEMAPosition<0 || PEMAPosition>=(int)pema.count ||
		pema[PEMAPosition].begin+position<0 || pema[PEMAPosition].begin+position>=(int)pema.count ||
		pema[PEMAPosition].end+position<0 || pema[PEMAPosition].end+position>=(int)pema.count)
		lplog(LOG_FATAL_ERROR,L"evaluateBNCPreferences - bad data!");
	for (int I=pema[PEMAPosition].begin+position; I<pema[PEMAPosition].end+position; I++)
		m[I].flags&=~WordMatch::flagBNCPreferIgnore;
	return cost;
}

int Source::BNCPatternViolation(int position,int PEMAPosition,vector < vector <tTagLocation> > &tagSets)
{ LFS
	int lowestCost=30;
	if (PEMAPosition<0 || PEMAPosition>=(int)pema.count ||
		pema[PEMAPosition].begin+position<0 || pema[PEMAPosition].begin+position>=(int)pema.count ||
		pema[PEMAPosition].end+position<0 || pema[PEMAPosition].end+position>=(int)pema.count)
		lplog(LOG_FATAL_ERROR,L"evaluateBNCPreferences - bad data!");
	for (int I=pema[PEMAPosition].begin+position; I<pema[PEMAPosition].end+position; I++)
		if (m[I].flags&(WordMatch::flagBNCPreferAdjectivePatternMatch|
			WordMatch::flagBNCPreferNounPatternMatch|
			WordMatch::flagBNCPreferVerbPatternMatch|
			WordMatch::flagBNCPreferAdverbPatternMatch))
			collectTagsFocusPositions.push_back(I);
	if (!collectTagsFocusPositions.size()) return 0;
	if (startCollectTags(debugTrace.traceBNCPreferences,BNCPreferencesTagSet,position,PEMAPosition,tagSets,false,false,L"BNCPatternViolation")>0)
		for (unsigned int J=0; J<tagSets.size(); J++)
		{
			//printTagSet("BNC",J,tagSets[J]);
			int tmpCost=evaluateBNCPreferences(position,PEMAPosition,tagSets[J]);
			lowestCost=min(lowestCost,tmpCost);
		}
		collectTagsFocusPositions.clear();
		return lowestCost;
}

// include every member of PEMAPositions which has the same or lower element # and has a lower index than I
bool Source::tagSetAllIn(vector <costPatternElementByTagSet> &PEMAPositions,int I)
{ LFS
	while (I>=0)
	{
		if (!pema[PEMAPositions[I].getPEMAPosition()].flagSet(patternElementMatchArray::IN_CHAIN))
			return false;
		int tagSetElement=PEMAPositions[I].getElement()-1;
		if (!tagSetElement) return true;
		for (I--; I>=0 && PEMAPositions[I].getElement()>tagSetElement; I--);
	}
	return true;
}

// ONLY AND ALL pema in chainPEMAPositions have IN_CHAIN flag set.
// input:
//   PEMAPositions - used not set
//   chainPEMAPositions - tempCost set, to transfer to PEMAPositionsSet.
// output:
//   PEMAPositionsSet - set eith elements from chainPEMAPositions, with tempCost set to the minimum of the maximum cost (maxOCost)
//     maxOCost is the maximum of the original cost of the element (the cost of just the words and the cost associated with the element of the pattern explicitly set in the pattern definition) PLUS the cumulativeDeltaCost
#define MIN_CHAIN_COST -MAX_COST
void Source::setChain(vector <patternElementMatchArray::tPatternElementMatch *> chainPEMAPositions,vector <costPatternElementByTagSet> &PEMAPositions,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int &traceSource,int &minOverallChainCost)
{ LFS
	if (debugTrace.traceSecondaryPEMACosting && PEMAPositions.size() > 0 && chainPEMAPositions.size() > 0)
		lplog(L"*** BEGIN set chain TS#%03d [SOURCE=%06d] %06d", PEMAPositions[0].getTagSet(), PEMAPositions[0].getTraceSource(), chainPEMAPositions[0] - pema.begin());
	unsigned int I;
	wstring flags;
	// get the last element that has IN_CHAIN set for the first tagset that has all of its elements having IN_CHAIN set
	for (I=0; I<PEMAPositions.size(); )
	{
		for (int ts=PEMAPositions[I].getTagSet(); I+1<PEMAPositions.size() && ts==PEMAPositions[I+1].getTagSet(); I++);
		if (tagSetAllIn(PEMAPositions,I))
			break;
		else // skip all elements that depend on this element
			for (int e=PEMAPositions[I++].getElement(); I<PEMAPositions.size() && e<PEMAPositions[I].getElement(); I++);
	}
	if (debugTrace.traceSecondaryPEMACosting)
		lplog(L"%06d:the last index in PEMAPositions for the first tagset that has all of its elements having IN_CHAIN set",I);
	int maxDeltaCost=MIN_CHAIN_COST,lastWinningPEMAPosition=0;
	if (I<PEMAPositions.size())
	{
		lastWinningPEMAPosition=I;
		if (debugTrace.traceSecondaryPEMACosting)
		{
			lplog(L"     PEMAPosition %d: TS#%03d [SOURCE=%06d] element=%d cost=%d maxDeltaCost=%d (lastWinningPEMAPosition=%d).", I,
				PEMAPositions[I].getTagSet(), PEMAPositions[I].getTraceSource(), PEMAPositions[I].getElement(), PEMAPositions[I].getCost(), maxDeltaCost, lastWinningPEMAPosition);
		}
		// found first element match.  How many more elements match?
		// elements which are greater must always ADD cost, or keep it the same.  The cost never decreases. see lowerPreviousElementCosts
		for (maxDeltaCost = PEMAPositions[I++].getCost(); I<PEMAPositions.size() && PEMAPositions[I].getElement()>PEMAPositions[I - 1].getElement() && tagSetAllIn(PEMAPositions, I); I++)
		{
			if (PEMAPositions[I].getCost() > maxDeltaCost)
				maxDeltaCost = PEMAPositions[lastWinningPEMAPosition = I].getCost();
			if (debugTrace.traceSecondaryPEMACosting)
			{
				lplog(L"     PEMAPosition %d: TS#%03d [SOURCE=%06d] element=%d cost=%d maxDeltaCost=%d (lastWinningPEMAPosition=%d).",I,
					PEMAPositions[I].getTagSet(), PEMAPositions[I].getTraceSource(), PEMAPositions[I].getElement(), PEMAPositions[I].getCost(), maxDeltaCost, lastWinningPEMAPosition);
			}
		}
		// find lowest cost for this element.  Otherwise this would stop at element 106, when really 108 is better.
		//  POS   : PEMA   TS E# COST MINC TRSC REJECT (3 set)
		//  000002: 002580 27 01 000  -09  108  false
		//  000004: 002581 27 02 000  -09  108  false
		//  000004: 002581 28 03 000  000  106  false
		//  000004: 002581 29 03 000  000  107  false
		//  000004: 002581 30 03 -09  -09  108  false
		//  000004: 002581 31 03 -09  -09  109  false
		//  000004: 002581 32 03 001  001  110  false
		//  000004: 002581 33 03 001  001  111  false
		//  000004: 002581 34 03 000  000  112  false
		//  000004: 002581 35 03 000  000  112  false
		for (; I < PEMAPositions.size() && PEMAPositions[I].getElement() == PEMAPositions[I - 1].getElement() && PEMAPositions[I].getPEMAPosition() == PEMAPositions[I - 1].getPEMAPosition(); I++)
		{
			if (PEMAPositions[I].getCost() < maxDeltaCost)
				maxDeltaCost = PEMAPositions[lastWinningPEMAPosition = I].getCost();
			if (debugTrace.traceSecondaryPEMACosting)
			{
				lplog(L"     PEMAPosition %d: TS#%03d [SOURCE=%06d] element=%d cost=%d maxDeltaCost=%d (lastWinningPEMAPosition=%d).", I,
					PEMAPositions[I].getTagSet(), PEMAPositions[I].getTraceSource(), PEMAPositions[I].getElement(), PEMAPositions[I].getCost(), maxDeltaCost, lastWinningPEMAPosition);
			}
		}
	}
	int maxOCost=MIN_CHAIN_COST;
	for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI = chainPEMAPositions.begin(), cPIEnd = chainPEMAPositions.end(); cPI != cPIEnd; cPI++)
		maxOCost = max(maxOCost, (*cPI)->getOCost() + (*cPI)->cumulativeDeltaCost);
	if (maxDeltaCost==MIN_CHAIN_COST)
		maxDeltaCost=0;
	// if deltacost is negative, take the highest OCost in the entire chain,
	// subtract deltacost and make that the highest OCost.
	else
		if (debugTrace.traceSecondaryPEMACosting)
		{
			wchar_t temp[1024];
			int len=0;
			for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI=chainPEMAPositions.begin(),cPIEnd=chainPEMAPositions.end(); cPI!=cPIEnd; cPI++)
				len+=wsprintf(temp+len,L"%06d (oCost=%d cumulativeDeltaCost=%d) %s | ",*cPI-pema.begin(),(*cPI)->getOCost(), (*cPI)->cumulativeDeltaCost, (*cPI)->flagsStr(flags));
			lplog(L"TS#%03d set chain %s with %d=maxOCost %d + maxDeltaCost %d [SOURCE=%06d].",
				PEMAPositions[lastWinningPEMAPosition].getTagSet(),temp,maxOCost+maxDeltaCost,maxOCost,maxDeltaCost,PEMAPositions[lastWinningPEMAPosition].getTraceSource());
		}
	maxOCost+=maxDeltaCost;
	// set the cost of the entire chain to maxOCost
	for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI=chainPEMAPositions.begin(),cPIEnd=chainPEMAPositions.end(); cPI!=cPIEnd; cPI++)
		if ((*cPI)->flagSet(patternElementMatchArray::COST_DONE))
		{
			if ((*cPI)->tempCost>maxOCost && (*cPI)->begin==0 && minOverallChainCost>maxOCost)
			{
				traceSource=PEMAPositions[lastWinningPEMAPosition].getTraceSource();
				minOverallChainCost=maxOCost;
			}
			if (debugTrace.traceSecondaryPEMACosting)
			{
				lplog(L"%06d COST_DONE already (oCost=%d cumulativeDeltaCost=%d) %s set tempCost=min(previous=%d,maxOCost=%d)=%d", 
					*cPI - pema.begin(), (*cPI)->getOCost(), (*cPI)->cumulativeDeltaCost, (*cPI)->flagsStr(flags), (*cPI)->tempCost, maxOCost, min((*cPI)->tempCost, maxOCost));
			}
			(*cPI)->tempCost=min((*cPI)->tempCost,maxOCost);
		}
		else
		{
			if (debugTrace.traceSecondaryPEMACosting)
			{
				lplog(L"%06d (oCost=%d cumulativeDeltaCost=%d) %s set tempCost (previous=%d) to maxOCost=%d set COST_DONE!",
					*cPI - pema.begin(), (*cPI)->getOCost(), (*cPI)->cumulativeDeltaCost, (*cPI)->flagsStr(flags), (*cPI)->tempCost, maxOCost);
			}
			(*cPI)->tempCost=maxOCost;
			(*cPI)->setFlag(patternElementMatchArray::COST_DONE);
			if ((*cPI)->begin==0 && minOverallChainCost>maxOCost)
			{
				traceSource=PEMAPositions[lastWinningPEMAPosition].getTraceSource();
				minOverallChainCost=maxOCost;
			}
			PEMAPositionsSet.push_back(*cPI);
		}
	if (debugTrace.traceSecondaryPEMACosting && PEMAPositions.size() > 0 && chainPEMAPositions.size() > 0)
		lplog(L"*** END set chain TS#%03d [SOURCE=%06d] %06d", PEMAPositions[0].getTagSet(), PEMAPositions[0].getTraceSource(), chainPEMAPositions[0] - pema.begin());
}

void Source::findAllChains(vector <costPatternElementByTagSet> &PEMAPositions,int PEMAPosition,vector <patternElementMatchArray::tPatternElementMatch *> &chain,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int &traceSource,int &minOverallChainCost)
{ LFS // DLFS
	// find first position with pattern and relativeEnd
	patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+PEMAPosition;
	int pattern=pem->getPattern(),relativeEnd=pem->end,relativeBegin=pem->begin;
	for (; pem && pem->getPattern()==pattern && pem->end==relativeEnd && !exitNow; pem=(pem->nextByPatternEnd<0) ? NULL : pema.begin()+pem->nextByPatternEnd)
		if (pem->begin==relativeBegin)
		{
			int nextPatternElementPEMAPosition=pem->nextPatternElement;
			chain.push_back(pem);
			pem->setFlag(patternElementMatchArray::IN_CHAIN);
			if (debugTrace.traceSecondaryPEMACosting)
				lplog(L"Set flag IN_CHAIN for PEMA position %06d.",pem-pema.begin());
			if (nextPatternElementPEMAPosition>=0)
				findAllChains(PEMAPositions,nextPatternElementPEMAPosition,chain,PEMAPositionsSet,traceSource,minOverallChainCost);
			else
				setChain(chain,PEMAPositions,PEMAPositionsSet,traceSource,minOverallChainCost);
			pem->removeFlag(patternElementMatchArray::IN_CHAIN);
			if (debugTrace.traceSecondaryPEMACosting)
				lplog(L"Removed flag IN_CHAIN for PEMA position %06d.", pem - pema.begin());
			chain.pop_back();
		}
}

// for each chain, calculate highest OCost and did chain include changed PEMAPosition?
// If chain included changed PEMAPosition, add deltaCost to highest OCost.
// for all positions:
//   set tempCost to highest OCost (if already set, set if lower than already set tempCost)
//   add to PEMAPositionsSet.
void Source::setChain2(vector <patternElementMatchArray::tPatternElementMatch *> &chainPEMAPositions,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int deltaCost)
{ LFS
	int maxOCost=MIN_CHAIN_COST;
	for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI=chainPEMAPositions.begin(),cPIEnd=chainPEMAPositions.end(); cPI!=cPIEnd; cPI++)
	{
		maxOCost=max(maxOCost,(*cPI)->getOCost()+(*cPI)->cumulativeDeltaCost);
		#ifdef TRACE_CHAIN2
			lplog(L"%d:SetChain2 maxOCost=%d getOCost=%d cumulativeDeltaCost=%d  =%d",(*cPI)-pema.begin(),maxOCost,
						(*cPI)->getOCost(),(*cPI)->cumulativeDeltaCost,(*cPI)->getOCost()+(*cPI)->cumulativeDeltaCost);
		#endif
	}
	#ifdef TRACE_CHAIN2
		if (t.traceSecondaryPEMACosting)
			lplog(L"SetChain2 maxOCost=%d deltaCost=%d = %d------------------",maxOCost,deltaCost,maxOCost+deltaCost);
	#endif
	maxOCost+=deltaCost;
	for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI=chainPEMAPositions.begin(),cPIEnd=chainPEMAPositions.end(); cPI!=cPIEnd; cPI++)
	{
			(*cPI)->flagSet(patternElementMatchArray::COST_DONE);
			#ifdef TRACE_CHAIN2
				lplog(L"%d:SetChain2 SAVED=%s tempCost %d > maxOCost %d",(*cPI)-pema.begin(),(!savePosition) ? L"true":L"false",(*cPI)->tempCost,maxOCost);
			#endif
	}
	// set the cost of the entire chain to maxOCost
	for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI=chainPEMAPositions.begin(),cPIEnd=chainPEMAPositions.end(); cPI!=cPIEnd; cPI++)
		if ((*cPI)->processTempCost(maxOCost))
		{
			PEMAPositionsSet.push_back(*cPI);
			#ifdef TRACE_CHAIN2
				lplog(L"%d:SetChain2 SAVED",(*cPI)-pema.begin());
			#endif
		}
}


// from origin, accumulate all chains till end.
void Source::findAllChains2(int PEMAPosition,int position,vector <patternElementMatchArray::tPatternElementMatch *> &chain,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int changedPosition,int rootPattern,int len,bool includesPatternMatch,int deltaCost)
{ LFS
	// find first position with pattern and relativeEnd
	patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+PEMAPosition;
	int pattern=pem->getPattern(),relativeEnd=pem->end,relativeBegin=pem->begin;
	//if (pem->isChildPattern())
	//  lplog(L"FAC2 %d==%d %s[%s]==%s[%s] && %d=%d",position,changedPosition,
	//    patterns[patterns[pem->getChildPattern()]->rootPattern]->name.c_str(),patterns[patterns[pem->getChildPattern()]->rootPattern]->differentiator.c_str(),
	//      patterns[rootPattern]->name.c_str(),patterns[rootPattern]->differentiator.c_str(),pem->getChildLen(),len);
	for (; pem && pem->getPattern()==pattern && pem->end==relativeEnd && !exitNow; pem=(pem->nextByPatternEnd<0) ? NULL : pema.begin()+pem->nextByPatternEnd)
		if (pem->begin==relativeBegin)
		{
			chain.push_back(pem);
			bool nextIncludesPatternMatch=includesPatternMatch|(position==changedPosition && pem->isChildPattern() &&
				patterns[pem->getChildPattern()]->rootPattern==rootPattern && pem->getChildLen()==len);
			#ifdef TRACE_CHAIN2
				lplog(L"%d:findAllChains2 position=%d changedPosition=%d childPattern=%s includesPatternMatch=%s %s[%s]len=%d %s[%s]len=%d",
					pem-pema.begin(),position,changedPosition,(pem->isChildPattern()) ? "true":"false",(nextIncludesPatternMatch) ? L"true" : L"false",
					patterns[patterns[pem->getChildPattern()]->rootPattern]->name.c_str(),
					patterns[patterns[pem->getChildPattern()]->rootPattern]->differentiator.c_str(),pem->getChildLen(),
					patterns[rootPattern]->name.c_str(),patterns[rootPattern]->differentiator.c_str(),len);
			#endif
			if (pem->nextPatternElement>=0)
				findAllChains2(pem->nextPatternElement,position+relativeBegin-pema[pem->nextPatternElement].begin,
											 chain,PEMAPositionsSet,changedPosition,rootPattern,len,nextIncludesPatternMatch,deltaCost);
			else
				setChain2(chain,PEMAPositionsSet,(nextIncludesPatternMatch) ? deltaCost : 0);
			chain.pop_back();
		}
}

// if PM cost MAY has changed, recalculate the lowest PM cost.  If it didn't change, return 0.
// if PM is a top level match, recalculate maxMatch.
// get all patterns from the root pattern and calculate the minimum cost including the changed cost and then not.
int Source::cascadeUpToAllParents(bool recalculatePMCost,int basePosition,patternMatchArray::tPatternMatch *childPM,int traceSource,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,wchar_t *fromWhere)
{ LFS // DLFS
	wchar_t temp[1024];
	if (!recalculatePMCost)
	{
		if (debugTrace.traceSecondaryPEMACosting)
			lplog(L"%d PM NO Recalculation %s[%s](%d,%d) - cost %d",basePosition,
			patterns[childPM->getPattern()]->name.c_str(),patterns[childPM->getPattern()]->differentiator.c_str(),
			basePosition,basePosition+childPM->len,childPM->getCost());
		return 0; // comment out to include below test
	}
	int deltaCost=0,finalMinimumCost=10000;
	for (int PEMAOffset=childPM->pemaByPatternEnd; PEMAOffset>=0; PEMAOffset=pema[PEMAOffset].nextByPatternEnd)
	{
		finalMinimumCost=min(finalMinimumCost,pema[PEMAOffset].getOCost()+pema[PEMAOffset].cumulativeDeltaCost);
		if (debugTrace.traceSecondaryPEMACosting)
			lplog(L"%d:PM Recalculation pema %d:%s - cost %d=%d + %d",basePosition,PEMAOffset,
			pema[PEMAOffset].toText(basePosition,temp,m),pema[PEMAOffset].getOCost()+pema[PEMAOffset].cumulativeDeltaCost,pema[PEMAOffset].getOCost(),pema[PEMAOffset].cumulativeDeltaCost);
	}
	if (finalMinimumCost==10000) finalMinimumCost=0;
	//if (!recalculatePMCost && finalMinimumCost!=childPM->getCost())  // calculation testing 
	//  lplog(L"%d:COST ERROR - finalMinimumCost %d!=childPM->getCost() %d",basePosition,finalMinimumCost,childPM->getCost());
	if (!(deltaCost=finalMinimumCost-childPM->getCost()))
	{
		if (debugTrace.traceSecondaryPEMACosting)
			lplog(L"%d:PM Recalculation shows no changed cost %s[%s](%d,%d) - cost %d",basePosition,
			patterns[childPM->getPattern()]->name.c_str(),patterns[childPM->getPattern()]->differentiator.c_str(),
			basePosition,basePosition+childPM->len,childPM->getCost());
		return 0;
	}
	// if pattern is a top level match, update maxMatch
	if (patterns[childPM->getPattern()]->isTopLevelMatch(*this,basePosition,basePosition+childPM->len))
	{
		if (debugTrace.tracePatternElimination)
			::lplog(L"%d:Pattern %s[%s](%d,%d) cost changed %d -> %d [SOURCE=%06d]",
			basePosition,
			patterns[childPM->getPattern()]->name.c_str(),patterns[childPM->getPattern()]->differentiator.c_str(),
			basePosition,basePosition+childPM->len,
			childPM->cost,finalMinimumCost,traceSource);
		if (childPM->cost<finalMinimumCost)
		{
			vector <WordMatch>::iterator im=m.begin()+basePosition,imEnd=m.begin()+basePosition+childPM->len;
			int len=childPM->len,avgCost=finalMinimumCost*1000/len,lowerAverageCost=childPM->getAverageCost(); // COSTCALC
			for (unsigned int relpos=0; im!=imEnd; im++,relpos++)
				if (im->updateMaxMatch(len,avgCost,lowerAverageCost) && debugTrace.tracePatternElimination)
					::lplog(L"%d:Pattern %s[%s](%d,%d) established a new HIGHER maxLACMatch %d or lowest average cost %d=%d*1000/%d",
					basePosition+relpos,
					patterns[childPM->getPattern()]->name.c_str(),patterns[childPM->getPattern()]->differentiator.c_str(),
					basePosition,basePosition+len,len,avgCost,finalMinimumCost,len); // COSTCALC
		}
	}
	// options:
	//   lower cost:
	//   childPM is lowest pm and gets lower.
	//   childPM is not the lowest pm, gets lower and is still not the lowest pm
	//   childPM is not the lowest pm, gets lower and becomes the lowest pm.
	//   higher cost:
	//   childPM is lowest pm, gets higher and is still the lowest pm
	//   childPM is lowest pm, gets higher and another pm is the lowest pm but a slightly higher pm
	//   childPM is lowest pm, gets higher and another pm is the lowest pm with same value
	//   childPM is not the lowest pm, gets higher and is still not the lowest
	vector <patternMatchArray::tPatternMatch *> allLocations;
	int lowestCost=MAX_COST,lowestCostAfter=MAX_COST;
	// get ALL children (from child root pattern)
	for (int p=patterns[childPM->getPattern()]->rootPattern,childend=childPM->len; p>=0; p=patterns[p]->nextRoot)
	{
		if (!m[basePosition].patterns.isSet(p)) continue;
		patternMatchArray::tPatternMatch *pm=(p==childPM->getPattern()) ? childPM : m[basePosition].pma.find(p,childend);
		if (pm==NULL) continue;
		lowestCost=min(lowestCost,pm->getCost());
		lowestCostAfter=min(lowestCostAfter,(pm==childPM) ? finalMinimumCost : pm->getCost());
		allLocations.push_back(pm);
	}
	childPM->cost=finalMinimumCost;
	if (lowestCostAfter==lowestCost)
	{
		if (debugTrace.traceSecondaryPEMACosting)
			lplog(L"%d:PM %d Recalculating %s[%s](%d,%d): original cost %d -> final cost %d NO RECURSION UPWARD (pm cost after %d == lowest pm cost before %d)",basePosition,
			childPM-m[basePosition].pma.content,patterns[childPM->getPattern()]->name.c_str(),patterns[childPM->getPattern()]->differentiator.c_str(),
			basePosition,basePosition+childPM->len,finalMinimumCost-deltaCost,finalMinimumCost,lowestCostAfter,lowestCost);
		return 0;
	}
	if (debugTrace.traceSecondaryPEMACosting)
		lplog(L"%d:PM %d Recalculating %s[%s](%d,%d): original cost %d -> final cost %d RECURSION delta %d",basePosition,
		childPM-m[basePosition].pma.content,patterns[childPM->getPattern()]->name.c_str(),patterns[childPM->getPattern()]->differentiator.c_str(),
		basePosition,basePosition+childPM->len,finalMinimumCost-deltaCost,finalMinimumCost,lowestCostAfter-lowestCost);
	deltaCost=lowestCostAfter-lowestCost;
	// get all origins
	set <int> origins;
	int firstPEMAPositionsSet=PEMAPositionsSet.size();
	for (unsigned int I=0; I<allLocations.size(); I++)
	{
		patternMatchArray::tPatternMatch *pm=allLocations[I];
		int childPattern=pm->getPattern();
		for (int PEMAOffset=pm->pemaByChildPatternEnd; PEMAOffset>=0; PEMAOffset=pema[PEMAOffset].nextByChildPatternEnd)
			// a child cannot be its own parent - a pattern never matches itself
			if (pema[PEMAOffset].getPattern()!=childPattern)
			{
				pema[PEMAOffset].addICostTillMax(deltaCost);
				int parentBasePosition=basePosition+pema[PEMAOffset].begin;
				int origin=pema[PEMAOffset].origin;
				if (origins.find(origin)!=origins.end()) continue;
				origins.insert(origin);
				vector <patternElementMatchArray::tPatternElementMatch *> chain;
				chain.reserve(16); // most of the time there will be less than 16 elements matched in a pattern
				if (debugTrace.traceSecondaryPEMACosting)
					lplog(L"%d:FINDCHAINS %s[%s](%d,%d) ORIGIN=%06d %s position=%d basePosition=%d rootPattern=%d %s[%s] len=%d deltaCost=%d.",
					basePosition,patterns[childPM->getPattern()]->name.c_str(),patterns[childPM->getPattern()]->differentiator.c_str(),basePosition,basePosition+childPM->len,
					origin,pema[origin].toText(parentBasePosition,temp,m),parentBasePosition,basePosition,patterns[childPM->getPattern()]->rootPattern,
					patterns[patterns[childPM->getPattern()]->rootPattern]->name.c_str(),patterns[patterns[childPM->getPattern()]->rootPattern]->differentiator.c_str(),
					childPM->len,deltaCost);
				findAllChains2(origin,parentBasePosition,chain,PEMAPositionsSet,basePosition,patterns[childPM->getPattern()]->rootPattern,childPM->len,false,deltaCost); // sets COST_DONE
				recalculateOCosts(recalculatePMCost,PEMAPositionsSet,firstPEMAPositionsSet,traceSource);
				patternMatchArray::tPatternMatch *parentPM=(m[parentBasePosition].patterns.isSet(pema[PEMAOffset].getPattern())) ? m[parentBasePosition].pma.find(pema[PEMAOffset].getPattern(),pema[PEMAOffset].end-pema[PEMAOffset].begin) : NULL;
				if (parentPM)
					cascadeUpToAllParents(recalculatePMCost,parentBasePosition,parentPM,traceSource,PEMAPositionsSet,fromWhere);
				else if (debugTrace.traceSecondaryPEMACosting)
					lplog(L"%d:%s[%s](%d,%d) NOT FOUND",parentBasePosition,
					patterns[pema[PEMAOffset].getPattern()]->name.c_str(),patterns[pema[PEMAOffset].getPattern()]->differentiator.c_str(),
					parentBasePosition,parentBasePosition+pema[PEMAOffset].end-pema[PEMAOffset].begin);
				for (vector<patternElementMatchArray::tPatternElementMatch *>::iterator ppsi=PEMAPositionsSet.begin()+firstPEMAPositionsSet,ppsiEnd=PEMAPositionsSet.end(); ppsi!=ppsiEnd; ppsi++)
				{
					(*ppsi)->removeFlag(patternElementMatchArray::COST_DONE);
					if ((*ppsi)->cumulativeDeltaCost)
					{
						if (debugTrace.traceSecondaryPEMACosting)
							lplog(L"assessCost PEMA %06dA Added %d cost for a total of %d (%s) [SOURCE=%06d] cascadeUpToAllParents",*ppsi-pema.begin(),(*ppsi)->cumulativeDeltaCost,(*ppsi)->getOCost()+(*ppsi)->cumulativeDeltaCost,fromWhere,traceSource);
						(*ppsi)->addOCostTillMax((*ppsi)->cumulativeDeltaCost);
						(*ppsi)->cumulativeDeltaCost=0;
					}
				}
				PEMAPositionsSet.erase(PEMAPositionsSet.begin()+firstPEMAPositionsSet,PEMAPositionsSet.end());
			}
	}
	return deltaCost;
}

#define MAX_ELEMENT 100
// an element # is the position in the matching pattern, not the position in the text.
// for example, with the test parse: With a mouth so full of food that Buster heard him . 
// 1:======== EVALUATING 000658 _NOUN_DETERMINER __NOUN[9](1,11)
// after 18 tag sets have been collected, we have 4 sets of different tag set origins.  
// origin set 0: PEMA positions 658, 659, 660 and 1791 
//               singular noun mouth has a determiner (cost=0) TAGSET 0+1 [SOURCE=17,18]  
//               000658 000663 000638 000659 __NOUN[9](1,11)*5 __NOUN[*](3) 
// origin set 1: PEMA positions 663, 664, 665, 1792
//               singular noun so has a determiner (cost=3) TAGSET 2,3,8,9 [SOURCE=19,20,25,26] 
//               singular noun mouth has a determiner (cost=0) TAGSET 4,5,6,7 [SOURCE=21,22,23,24] 
//               000663 000668 000646 000664 __NOUN[9](1,11)*6 __NOUN[*](4) 
// origin set 2: PEMA positions 668, 669, 670, 1793
//               singular noun full has a determiner (cost=1) TAGSET 10,11 [SOURCE=27,28]
//               singular noun so has a determiner (cost=3) TAGSET 12,13 [SOURCE=29,30]
//               singular noun mouth has a determiner (cost = 0) TAGSET 14,15,16,17 [SOURCE=31,32,33,34]
//               000668 002040 000654 000669 __NOUN[9](1,11)*2 __NOUN[*](5)
// origin set 3: PEMA positions 2040,2041
//               singular noun mouth has a determiner (cost=0) TAGSET 18 [SOURCE 35]
//               002040 -00658 002038 002041 __NOUN[9](1,11)*15 __NOUN[*](7)
// lowerPreviousElementCosts operates with only one origin set within the tagsets that have been derived from a pattern.
// taking origin set #2:
//  POS   : PEMA   TS E# COST MINC TRSC REJECT(2 set) Secondary costing reason : nounDeterminer
// 	000001: 000668 10 00 001  000  031  true  - this is first occurrence of element #0 of first tag set! cost is set to 1 in IP structure at this position.
// 	000005: 000669 10 02 001  001  027  true  - this is first occurrence of element #2 of first tag set! cost is set to 1 in IP structure at this position.
// 	000007: 000670 10 03 001  001  027  true  - this is first occurrence of element #3 of first tag set! cost is set to 1 in IP structure at this position.
//  the rest of the origin set is handled by the while loop.
// 	000005: 001793 11 02 001  001  028  false - this is not less than the cost of the occurrence of the previous element #
// 	000005: 000669 12 02 003  003  029  false - this is not less than the cost of the occurrence of the previous element #
// 	000007: 000670 12 03 003  003  029  false - this is not less than the cost of the occurrence of the previous element #
// 	000005: 001793 13 02 003  003  030  false - this is not less than the cost of the occurrence of the previous element #
// 	000005: 000669 14 02 000  000  031  false - this is less than the cost of the occurrence of the previous element #, so element #0 and #1 are set to 0.
// 	000007: 000670 14 03 000  000  031  false - this is less than the cost of the occurrence of the previous element #, so element #2 is also set to 0.
// 	000005: 001793 15 02 000  000  032  true  - this is not less than the cost of the occurrence of the previous element #
// 	000005: 000669 16 02 000  000  033  false - this is not less than the cost of the occurrence of the previous element #
// 	000007: 000670 16 03 000  000  033  false - this is not less than the cost of the occurrence of the previous element #
// 	000005: 001793 17 02 000  000  034  true  - this is not less than the cost of the occurrence of the previous element #
// this is obsolete!  replaced by lowerPreviousElementCosts
/*
void Source::lowerPreviousElementCostsOld(vector <costPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, wchar_t *fromWhere)
{
	LFS
		vector <costPatternElementByTagSet>::iterator IPBegin = PEMAPositions.begin(), IPEnd = PEMAPositions.end(), IP;
	int costSet = 0;
	for (IP = IPBegin; IP != IPEnd; IPBegin = IP)
	{
		map <int, vector <costPatternElementByTagSet>::iterator> lastOccurrenceOfElement; // index is pattern element #, assigned value is PEMAPosition iterator.
		int tagSet;
		for (tagSet = IP->getTagSet(); IP != IPEnd && IP->getTagSet() == tagSet; IP++)
		{
			lastOccurrenceOfElement[IP->getElement()] = IP;
			IP->setCost(costs[IP->getTagSet()]);
			IP->setTraceSource(traceSources[IP->getTagSet()]);
		}
		// Important to change only the last occurrence of element!
		// divide array into sets of increasing elements.
		// if in any set the cost of a position A is greater than another position B and position of A is < than position of B (it came first)
		//  then decrease the cost of A to the cost of B.  This is because with any set of PEMA elements SPEMA that form a complete matched pattern
		//  if element of A < element of B and member B belongs to set SPEMA then member A also belongs to SPEMA. Member A also belongs to another
		//  set SPEMA2, with a higher cost, but we prefer the lower cost one because we are seeking patterns with the lowest cost.
		while (IP != IPEnd && IP->getElement())
		{
			tagSet = IP->getTagSet();
			for (int lowestElement = IP->getElement() - 1; lowestElement >= 0; lowestElement--)
				if (lastOccurrenceOfElement.find(lowestElement) != lastOccurrenceOfElement.end())
					if (lastOccurrenceOfElement[lowestElement]->getCost() > costs[tagSet])
					{
						lastOccurrenceOfElement[lowestElement]->setCost(costs[tagSet]);
						lastOccurrenceOfElement[lowestElement]->setTraceSource(traceSources[tagSet]);
					}
			for (; IP != IPEnd && IP->getTagSet() == tagSet; IP++)
			{
				lastOccurrenceOfElement[IP->getElement()] = IP;
				IP->setCost(costs[IP->getTagSet()]);
				IP->setTraceSource(traceSources[IP->getTagSet()]);
			}
		}
		if (debugTrace.traceSecondaryPEMACosting)
		{
			lplog(L"POS   : PEMA   TS E# COST MINC TRSC REJECT (ORIGIN SET %04d) Secondary costing reason: %s", costSet++, fromWhere);
			for (vector <costPatternElementByTagSet>::iterator tIP = IPBegin; tIP != IP; tIP++)
				lplog(L"%06d: %06d %02d %02d %03d  %03d  %03d  %s",
					tIP->getSourcePosition(), tIP->getPEMAPosition(), tIP->getTagSet(), tIP->getElement(), costs[tIP->getTagSet()], tIP->getCost(), tIP->getTraceSource(),
					(pema[tIP->getPEMAPosition()].tempCost == tIP->getCost()) ? L"true " : L"false");
		}
	}
}
*/

// Divide PEMAPositions into sets (logged as ORIGIN SET) depending on the first immediate child (the first branch or first element of the pattern has more than one match)
//    So, __NOUN[9] has a __NOUN as the first element, but that element has multiple matches of differing lengths (see below).
//    within each set, there are pattern elements.
// an element # is the position in the matching pattern, not the position in the text.
// for example, with the test parse: With a mouth so full of food that Buster heard him . 
// 1:======== EVALUATING 000658 _NOUN_DETERMINER __NOUN[9](1,11)
// after 18 tag sets have been collected, we have 4 sets of different tag set origins.  
// origin set 0: PEMA positions 658, 659, 660 and 1791 
//               singular noun mouth has a determiner (cost=0) TAGSET 0+1 [SOURCE=17,18]  
//               000658 000663 000638 000659 __NOUN[9](1,11)*5 __NOUN[*](3) 
// origin set 1: PEMA positions 663, 664, 665, 1792
//               singular noun so has a determiner (cost=3) TAGSET 2,3,8,9 [SOURCE=19,20,25,26] 
//               singular noun mouth has a determiner (cost=0) TAGSET 4,5,6,7 [SOURCE=21,22,23,24] 
//               000663 000668 000646 000664 __NOUN[9](1,11)*6 __NOUN[*](4) 
// origin set 2: PEMA positions 668, 669, 670, 1793
//               singular noun full has a determiner (cost=1) TAGSET 10,11 [SOURCE=27,28]
//               singular noun so has a determiner (cost=3) TAGSET 12,13 [SOURCE=29,30]
//               singular noun mouth has a determiner (cost = 0) TAGSET 14,15,16,17 [SOURCE=31,32,33,34]
//               000668 002040 000654 000669 __NOUN[9](1,11)*2 __NOUN[*](5)
// origin set 3: PEMA positions 2040,2041
//               singular noun mouth has a determiner (cost=0) TAGSET 18 [SOURCE 35]
//               002040 -00658 002038 002041 __NOUN[9](1,11)*15 __NOUN[*](7)
// lowerPreviousElementCosts operates with only one origin set within the tagsets that have been derived from a pattern.
// taking origin set #2:
//  POS   : PEMA   TS E# COST (2 set) Secondary costing reason : nounDeterminer
// 	000001: 000668 10 00 001  - this is first occurrence of element #0 ! cost is set to 1 for this element.
// 	000005: 000669 10 02 001  - this is first occurrence of element #2 ! cost is set to 1 for this element.
// 	000007: 000670 10 03 001  - this is first occurrence of element #3 ! cost is set to 1 for this element.
// 	000005: 001793 11 02 001  - this is not less than the cost of the occurrence of the previous element #
// 	000005: 000669 12 02 003  - this is not less than the cost of the occurrence of the previous element #
// 	000007: 000670 12 03 003  - this is not less than the cost of the occurrence of the previous element #
// 	000005: 001793 13 02 003  - this is not less than the cost of the occurrence of the previous element #
// 	000005: 000669 14 02 000  - this is less than the cost of the occurrence of the previous element #, so element #0 and #1 and #2 are set to 0.
// 	000007: 000670 14 03 000  - this is less than the cost of the occurrence of the previous element #, so element #3 is also set to 0.
// 	000005: 001793 15 02 000  - this is not less than the cost of the occurrence of the previous element #
// 	000005: 000669 16 02 000  - this is not less than the cost of the occurrence of the previous element #
// 	000007: 000670 16 03 000  - this is not less than the cost of the occurrence of the previous element #
// 	000005: 001793 17 02 000  - this is not less than the cost of the occurrence of the previous element #

// for example, with the test parse: it was a pity he did it . 
// 	POS    : PEMA   TS E# COST (ORIGIN SET 0000) Secondary costing reason: verbObjects
// 	000000 : 000152 00 00 006 - this is first occurrence of element #0 ! cost is set to 6 for this element.
// 	000001 : 000153 00 01 006 - this is first occurrence of element #1 ! cost is set to 6 for this element.
// 	000002 : 000154 00 03 006 - this is first occurrence of element #3 ! cost is set to 6 for this element.
// 	000002 : 000154 01 03 006 - this is not less than the cost of the occurrence of the previous element #
// 	000002 : 000154 02 03 006 - this is not less than the cost of the occurrence of the previous element #
// 	000002 : 000181 03 03 -01 - this is less than the cost of the occurrence of the previous element #, so element #0, #1 and #2 are set to -1.
//   setPreviousElementsCostsAtIndex(ip,element):
//     lme=0 - last mandatory element (the last mandatory element before the current element)
//     for each previous element # (e) = element-1 to 0:
//       if pattern[element] has minimum count of > 0, then lme=e, break.
//     for each previous PEMAPosition (pp) = ip-1 to 0:
//       if (pp->position+pema[pp->PEMAPosition].end==sourcePosition and pp->cost>cost and pp->element>=lme and pp->element<element):
//         pp->cost=cost
//         pp->traceSource=traceSource
//         call setPreviousElementsCostsAtIndex(pp)

// get relative source position of the end of the element matched.  So we have to go to the next element and see where that is.
int Source::getEndRelativeSourcePosition(int PEMAPosition)
{
	int nextPEMAPosition = pema[PEMAPosition].nextPatternElement;
	if (nextPEMAPosition < 0)
		return pema[PEMAPosition].end;
	return pema[PEMAPosition].begin - pema[nextPEMAPosition].begin;
}

// get the nearest preceding element which is not optional (minimum > 0)
// for each preceding matched element in the pattern, 
//   check to see whether it is immediately preceding it in the source (its end position matches the passed in patternElementEndPosition), 
//   and that its cost is > than the cost passed in.
//   and also that the element matched for that position is >= the nearest preceding non-optional element 
//   and it is less than the element of the calling procedure (which could have been recursive)
void Source::setPreviousElementsCostsAtIndex(vector <costPatternElementByTagSet> &PEMAPositions, int pp, int cost, int traceSource, int patternElementEndPosition,int pattern,int patternElement)
{
	int lme = 0;
	for (int pe = patternElement - 1; pe >= 0; pe--)
		if (patterns[pattern]->getElement(pe)->minimum > 0)
		{
			lme = pe;
			break;
		}
	for (int ppi = pp; ppi >= 0; ppi--)
	{
		if ((PEMAPositions[ppi].getSourcePosition()+ getEndRelativeSourcePosition(PEMAPositions[ppi].getPEMAPosition())) == patternElementEndPosition && // it is immediately preceding it in the source (its end position matches the passed in patternElementEndPosition)?
			   PEMAPositions[ppi].getCost() > cost && // its cost is > than the cost passed in?
				 PEMAPositions[ppi].getElement() >= lme && // the element matched for that position is >= the nearest preceding non-optional element?
				 PEMAPositions[ppi].getElement() < patternElement)  // less than the element of the calling procedure (which could have been recursive)?
		{
			if (debugTrace.traceSecondaryPEMACosting)
				lplog(L"set index %d (PEMA=%06d) to cost %d from originating index position %d. [OLD SOURCE=%06d] [NEW SOURCE=%06d] %s", ppi, PEMAPositions[ppi].getPEMAPosition(), cost, pp+1, PEMAPositions[ppi].getTraceSource(), traceSource,L"PREVIOUS");
			PEMAPositions[ppi].setCost(cost);
			PEMAPositions[ppi].setTraceSource(traceSource);
			if (ppi>0)
				setPreviousElementsCostsAtIndex(PEMAPositions, ppi - 1, cost, traceSource, PEMAPositions[ppi].getSourcePosition(), pattern, PEMAPositions[ppi].getElement());
		}
		else	if (debugTrace.traceSecondaryPEMACosting)
		{
			bool b1 = (PEMAPositions[ppi].getSourcePosition() + getEndRelativeSourcePosition(PEMAPositions[ppi].getPEMAPosition())) == patternElementEndPosition;// it is immediately preceding it in the source (its end position matches the passed in patternElementEndPosition)?
			bool b2 = PEMAPositions[ppi].getCost() > cost; // its cost is > than the cost passed in?
			bool b3 = PEMAPositions[ppi].getElement() >= lme; // the element matched for that position is >= the nearest preceding non-optional element?
			bool b4 = PEMAPositions[ppi].getElement() < patternElement;  // less than the element of the calling procedure (which could have been recursive)?
			lplog(L"DID NOT set index %d (PEMA=%06d) to cost %d from originating index position %d [REASONS=%s %s %s %s]. ", ppi, PEMAPositions[ppi].getPEMAPosition(), cost, pp + 1,
						(b1) ? L"true" : L"false", (b2) ? L"true" : L"false", (b3) ? L"true" : L"false", (b4) ? L"true" : L"false");
		}
	}
}

void Source::lowerPreviousElementCosts(vector <costPatternElementByTagSet> &PEMAPositions, vector <int> &costsPerTagSet, vector <int> &traceSources, wchar_t *fromWhere)
{
	LFS
	if (debugTrace.traceSecondaryPEMACosting)
	{
		map<int, wstring> pemaPositionsByTagSet;
		for (costPatternElementByTagSet pp:PEMAPositions)
		{
			wstring PP;
			pemaPositionsByTagSet[pp.getTagSet()] += itos(pp.getPEMAPosition(), L"%06d", PP) + L" ";
		}
		for (auto const& [tagSet,PEMAPositionsStr]: pemaPositionsByTagSet)
			lplog(L"tagSet %02d:%s", tagSet, PEMAPositionsStr.c_str());
	}
	for (int ip = 0; ip < PEMAPositions.size(); ip++)
	{
		PEMAPositions[ip].setCost(costsPerTagSet[PEMAPositions[ip].getTagSet()]);
		PEMAPositions[ip].setTraceSource(traceSources[PEMAPositions[ip].getTagSet()]);
		if (debugTrace.traceSecondaryPEMACosting)
			lplog(L"start from position index %d cost=%d tagSet=%02d PEMA=%06d [SOURCE=%06d]", ip, costsPerTagSet[PEMAPositions[ip].getTagSet()], PEMAPositions[ip].getTagSet(),PEMAPositions[ip].getPEMAPosition(), traceSources[PEMAPositions[ip].getTagSet()]);
		if (ip>0)
			setPreviousElementsCostsAtIndex(PEMAPositions,ip - 1, PEMAPositions[ip].getCost(), PEMAPositions[ip].getTraceSource(), PEMAPositions[ip].getSourcePosition(), pema[PEMAPositions[ip].getPEMAPosition()].getPattern(), PEMAPositions[ip].getElement());
		for (int psmi = 0; psmi < ip; psmi++)
			if (PEMAPositions[psmi].getPEMAPosition() == PEMAPositions[ip].getPEMAPosition() && // same PEMAPosition? (but from a different tagset!)
				PEMAPositions[psmi].getCost() > costsPerTagSet[PEMAPositions[ip].getTagSet()]) // its cost is > than the cost passed in?
			{
				if (debugTrace.traceSecondaryPEMACosting)
					lplog(L"set index %d (PEMA=%06d) to cost %d from originating index position %d. [OLD SOURCE=%06d] [NEW SOURCE=%06d] %s", psmi, PEMAPositions[psmi].getPEMAPosition(), costsPerTagSet[PEMAPositions[ip].getTagSet()], ip, PEMAPositions[psmi].getTraceSource(), traceSources[PEMAPositions[ip].getTagSet()], L"SAME");
				PEMAPositions[psmi].setCost(costsPerTagSet[PEMAPositions[ip].getTagSet()]);
				PEMAPositions[psmi].setTraceSource(traceSources[PEMAPositions[ip].getTagSet()]);
			}
	}
	if (debugTrace.traceSecondaryPEMACosting)
	{
		lplog(L"I  :POS B -  E   : PEMA   TS E# TSCOST ADJCOST TRSC Secondary costing reason: %s", fromWhere); 
		for (int ip = 0; ip < PEMAPositions.size(); ip++)
		{
			wchar_t PP[1024];
			memset(PP, L' ', sizeof(PP));
			if (PEMAPositions[ip].getElement() > 100 || PEMAPositions[ip].getElement() < 0)
				wsprintf(PP, L"Illegal Element %d!", PEMAPositions[ip].getElement());
			else
			{
				wsprintf(PP, L"%s[%s](%d,%d)", patterns[pema[PEMAPositions[ip].getPEMAPosition()].getPattern()]->name.c_str(), patterns[pema[PEMAPositions[ip].getPEMAPosition()].getPattern()]->differentiator.c_str(), PEMAPositions[ip].getSourcePosition(), PEMAPositions[ip].getSourcePosition() + pema[PEMAPositions[ip].getPEMAPosition()].getChildLen()); //  + PEMAPositions[ip].getElement() * 16
			}
			lplog(L"%03d:%06d-%06d: %06d %02d %02d  %03d   %03d     %03d:%s", ip,
				PEMAPositions[ip].getSourcePosition(), PEMAPositions[ip].getSourcePosition() + getEndRelativeSourcePosition(PEMAPositions[ip].getPEMAPosition()),
				PEMAPositions[ip].getPEMAPosition(), PEMAPositions[ip].getTagSet(), PEMAPositions[ip].getElement(), costsPerTagSet[PEMAPositions[ip].getTagSet()], PEMAPositions[ip].getCost(), PEMAPositions[ip].getTraceSource(),
				PP);
		}
	}
}

// this is obsolete!  replaced by lowerPreviousElementCosts
/*
void Source::lowerPreviousElementCostsLowerRegardlessOfPosition(vector <costPatternElementByTagSet> &PEMAPositions, vector <int> &costs, vector <int> &traceSources, wchar_t *fromWhere)
{
	LFS
		vector <costPatternElementByTagSet>::iterator IPBegin, IPEnd = PEMAPositions.end(), IP, originSetEnd = PEMAPositions.begin();
	int costSet = 0;
	while (originSetEnd != IPEnd)
	{
		for (IPBegin = originSetEnd, originSetEnd = IPBegin + 1; originSetEnd != IPEnd && originSetEnd->getElement(); originSetEnd++);
		map <int, int> lowestCostOfElement; // index is pattern element #, assigned value is PEMAPosition iterator.
		map <int, int> lowestCostTraceSource; // index is pattern element #, assigned value is traceSource.
		for (IP = IPBegin; IP != originSetEnd; IP++)
		{
			for (int element = IP->getElement(); element >= 0; element--)
			{
				if (lowestCostOfElement.find(element) == lowestCostOfElement.end() || costs[IP->getTagSet()] < lowestCostOfElement[element])
				{
					lowestCostOfElement[element] = costs[IP->getTagSet()];
					lowestCostTraceSource[element] = traceSources[IP->getTagSet()];
				}
				else
					break;
			}
		}
		for (IP = IPBegin; IP != originSetEnd; IP++)
		{
			IP->setCost(lowestCostOfElement[IP->getElement()]);
			IP->setTraceSource(lowestCostTraceSource[IP->getElement()]);
		}
		if (debugTrace.traceSecondaryPEMACosting)
		{
			lplog(L"POS   : PEMA   TS E# COST MINC TRSC REJECT (ORIGIN SET %04d) Secondary costing reason: %s", costSet++, fromWhere);
			for (IP = IPBegin; IP != originSetEnd; IP++)
				lplog(L"%06d: %06d %02d %02d %03d  %03d  %03d  %s",
					IP->getSourcePosition(), IP->getPEMAPosition(), IP->getTagSet(), IP->getElement(), costs[IP->getTagSet()], IP->getCost(), IP->getTraceSource(),
					(pema[IP->getPEMAPosition()].tempCost == IP->getCost()) ? L"true " : L"false");
		}
	}
}
*/

void Source::recalculateOCosts(bool &recalculatePMCost,vector<patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int start,int traceSource)
{ LFS
	recalculatePMCost=false;
	for (vector<patternElementMatchArray::tPatternElementMatch *>::iterator ppsi=PEMAPositionsSet.begin()+start,ppsiEnd=PEMAPositionsSet.end(); ppsi!=ppsiEnd; ppsi++)
		if ((*ppsi)->tempCost!=((*ppsi)->getOCost()+(*ppsi)->cumulativeDeltaCost))
		{
			int deltaCost=(*ppsi)->tempCost-((*ppsi)->getOCost()+(*ppsi)->cumulativeDeltaCost);
			if (debugTrace.traceSecondaryPEMACosting)
				lplog(L"Set PEMA position %06d to cumulative delta cost %d=%d+ (original) %d (delta cost %d = %d-%d) [SOURCE=%06d]",
							*ppsi-pema.begin(),(*ppsi)->cumulativeDeltaCost+deltaCost,deltaCost,(int)(*ppsi)->cumulativeDeltaCost,
							deltaCost,(int)(*ppsi)->tempCost,(*ppsi)->getOCost(),traceSource);
			(*ppsi)->cumulativeDeltaCost+=deltaCost;
			if ((*ppsi)->begin==0)
				recalculatePMCost=true;
		}
		else
			if (debugTrace.traceSecondaryPEMACosting)
				lplog(L"IGNORE PEMA position %06d to cumulative delta cost %d=%d+ (original) %d (delta cost %d = %d-%d) [SOURCE=%06d]",
							*ppsi-pema.begin(),(int)(*ppsi)->cumulativeDeltaCost,0,(int)(*ppsi)->cumulativeDeltaCost,
							0,(int)(*ppsi)->tempCost,(*ppsi)->getOCost(),traceSource);

}

/* purpose of setSecondaryCosts is to increase or decrease costs calculated from the tagSet
AS THOUGH the patterns (SET A) matched by the tag set were decreased or increased by the cost of the tag set
at the time they were matched (and so therefore the cost is reflected in any parent pattern that uses
the patterns (SET A)

/*
P: PEMA TS# E
0:  147 0   0
1:  148 0   1
2:  149 0   2
3:  154 0   2
4:  155 0   3
5:  152 1   1
6:  153 1   2
7:  156 2   2
8:  157 2   3
1     2    3  4
3     4
2          3  4
3     4
TS 1=COST 1, TS 2=COST 2, TS COST 3=3, TS COST 4=4
P:  TS#  E
0:   0   1
1:   0   2
2:   0   3
3:   0   4
4:   1   3 (BIP,IP=4 lowestElement=3)
5:   1   4
6:   2   2 (BIP,IP=6 lowestElement=2)
7:   2   3
8:   2   4
9:   3   3
10:   3   4

*/
// set the costs of the next to top tier of the pattern (secondary)
int Source::setSecondaryCosts(vector <costPatternElementByTagSet> &PEMAPositions,patternMatchArray::tPatternMatch *pm,int basePosition, wchar_t *fromWhere)
{ LFS
	// assign cost to each P matching TS#.  Keep lowest E (LE) for each TS#.
	// for each E<LE, the last P having E<LE, assign cost of P to be the minimum of the TS cost and the cost already at P.
	vector <patternElementMatchArray::tPatternElementMatch *> chain,PEMAPositionsSet;
	chain.reserve(16);
	PEMAPositionsSet.reserve(1024);
	int traceSource=-1,minOverallChainCost=MAX_COST;
	set <int> origins;
	for (auto costTS:PEMAPositions)
	{
		if (origins.find(pema[costTS.getPEMAPosition()].origin) == origins.end())
		{
			origins.insert(pema[costTS.getPEMAPosition()].origin);
			findAllChains(PEMAPositions, pema[costTS.getPEMAPosition()].origin, chain, PEMAPositionsSet, traceSource, minOverallChainCost);
		}
	}
	bool recalculatePMCost=false;
	recalculateOCosts(recalculatePMCost,PEMAPositionsSet,0,traceSource);
	cascadeUpToAllParents(recalculatePMCost,basePosition,pm,traceSource,PEMAPositionsSet,fromWhere);
	for (vector<patternElementMatchArray::tPatternElementMatch *>::iterator ppsi=PEMAPositionsSet.begin(),ppsiEnd=PEMAPositionsSet.end(); ppsi!=ppsiEnd; ppsi++)
	{
		(*ppsi)->removeFlag(patternElementMatchArray::COST_DONE);
		if ((*ppsi)->cumulativeDeltaCost)
		{
			if (debugTrace.traceSecondaryPEMACosting)
				lplog(L"assessCost PEMA %06dA Added %d cost for a total of %d (%s) [SOURCE=%06d] setSecondaryCosts",*ppsi-pema.begin(),(*ppsi)->cumulativeDeltaCost,(*ppsi)->getOCost()+(*ppsi)->cumulativeDeltaCost,fromWhere,traceSource);
			(*ppsi)->addOCostTillMax((*ppsi)->cumulativeDeltaCost);
			(*ppsi)->cumulativeDeltaCost=0;
		}
	}
	return 0;
}

int Source::calculateVerbAfterVerbUsage(int whereVerb,unsigned int nextWord,bool adverbialObject)
{ LFS
	if (nextWord<m.size() && (m[nextWord].forms.isSet(verbForm) || adverbialObject))
	{
		// tried to adjust by the probability of the next word being a verb
		// but this made parsing worse, even if cost was high (costs are not entirely accurate either)
		//if (m[nextWord].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR)
		//	cost=(int)((FORM_USAGE_PATTERN_HIGHEST_COST-m[nextWord].word->second.usageCosts[m[nextWord].queryForm(verbForm)])*1.5);
		int cost=tFI::HIGHEST_COST_OF_INCORRECT_VERB_AFTER_VERB_USAGE;
		// determine whether verb directly after mainVerb is compatible
		// previous verb: is (be, been),             'object' verb: present participle
		// previous verb: has, is (be, been, being), 'object' verb: past participle
		// increase by HIGHEST_COST_OF_INCORRECT_VERB_AFTER_VERB_USAGE.
		vector <WordMatch>::iterator im=m.begin()+whereVerb;
		int nextVerbIndex = m[nextWord].queryForm(verbForm);
		if (nextVerbIndex < 0)
			return 0;
		int nextVerbCost = m[nextWord].word->second.getUsageCost(nextVerbIndex);
		//  add "verbal_auxiliary{V_AGREE}",0,1,1, // he dares eat this?
		if ((im->forms.isSet(futureModalAuxiliaryForm) || im->forms.isSet(negationFutureModalAuxiliaryForm) ||
			im->forms.isSet(modalAuxiliaryForm) || im->forms.isSet(negationModalAuxiliaryForm)) &&
			((m[nextWord].word->second.inflectionFlags&(VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL | VERB_PRESENT_SECOND_SINGULAR)) != 0 || adverbialObject))
		{
			if ((m[nextWord].word->second.inflectionFlags&(VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL | VERB_PRESENT_SECOND_SINGULAR)) != VERB_PRESENT_FIRST_SINGULAR ||
				nextVerbCost < 4)
			{
				if (debugTrace.traceVerbObjects)
					lplog(L"VerbAfterVerb: testing verb=%d:%s, nextWord=%d:%s adverbialObject=%s CASE 1 is true", whereVerb, m[whereVerb].word->first.c_str(), nextWord, m[nextWord].word->first.c_str(), (adverbialObject) ? L"true" : L"false");
				return cost;
			}
		}
		if ((im->forms.isSet(isForm) || im->forms.isSet(isNegationForm) || im->word->first == L"be" || im->word->first == L"been") &&
			((m[nextWord].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) != 0 || adverbialObject))
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"VerbAfterVerb: testing verb=%d:%s, nextWord=%d:%s adverbialObject=%s CASE 2 is true", whereVerb, m[whereVerb].word->first.c_str(), nextWord, m[nextWord].word->first.c_str(), (adverbialObject) ? L"true" : L"false");
			return cost;
		}
		// 'having' is sufficiently rare that I will not include it to be conservative. 'having examined'
		if ((((im->forms.isSet(haveForm) || im->forms.isSet(haveNegationForm))) || // && !(im->word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE)) ||
			im->forms.isSet(isForm) || im->forms.isSet(isNegationForm) ||
			im->word->first==L"be" || im->word->first==L"being") && // "been" included in isForm
			(m[nextWord].word->second.inflectionFlags&VERB_PAST_PARTICIPLE)!=0)
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"VerbAfterVerb: testing verb=%d:%s, nextWord=%d:%s adverbialObject=%s CASE 3 is true", whereVerb, m[whereVerb].word->first.c_str(), nextWord, m[nextWord].word->first.c_str(), (adverbialObject) ? L"true" : L"false");
			return cost;
		}
		if ((im->forms.isSet(doesForm) || im->forms.isSet(doesNegationForm)) && im->word->first!=L"doing" &&
				(m[nextWord].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR)!=0)
		{
			int verbAfterVerbCostRatio[] = { 16, 12, 8, 6, 4 };
			cost = (cost * verbAfterVerbCostRatio[nextVerbCost]) / 10;
			if (debugTrace.traceVerbObjects)
				lplog(L"VerbAfterVerb: testing verb=%d:%s, nextWord=%d:%s adverbialObject=%s CASE 4 is true [cost=%d]", whereVerb, m[whereVerb].word->first.c_str(), nextWord, m[nextWord].word->first.c_str(), (adverbialObject) ? L"true" : L"false",cost);
			return cost;
		}
	}
	return 0;
}

//  desiredTagSets.push_back(tTS(NOUN_DETERMINER_TAGSET,1,"N_AGREE",NULL));
int Source::evaluateNounDeterminer(vector <tTagLocation> &tagSet, bool assessCost, int &traceSource, int begin, int end, int fromPEMAPosition)
{
	LFS
	int nounTag = -1, nextNounTag = -1, nAgreeTag = -1, nextNAgreeTag = -1, nextDet = -1, PNC = 0;// , nextName = -1, subObjectTag = -1;
	if ((nounTag=findTag(tagSet,L"NOUN",nextNounTag))>=0) // PNOUN not included because personal pronouns do not have determiners and the patterns cannot match for them, so this case will not occur.
		nAgreeTag=findTagConstrained(tagSet,L"N_AGREE",nextNAgreeTag,tagSet[nounTag]);
	else
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:Noun (%d,%d) has no noun tag (cost=%d) [SOURCE=%06d].", begin, begin, end, PNC, traceSource = gTraceSource++);
		return PNC;
	}
	if (fromPEMAPosition!= abs(tagSet[nounTag].PEMAOffset) && pema[abs(tagSet[nounTag].PEMAOffset)].flagSet(patternElementMatchArray::COST_ND))
	{
		if (debugTrace.traceDeterminer)
		{
			int pattern = pema[abs(tagSet[nounTag].PEMAOffset)].getPattern();
			lplog(L"%d:======== EVALUATION %06d %s[%s](%d,%d) SKIPPED (ND already evaluated)", begin, fromPEMAPosition, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end);
		}
		return 0;
	}
	if (nAgreeTag >= 0 && nAgreeTag < end - 1 && calculateVerbAfterVerbUsage(end - 1, end,false)) // if nAgreeTag<end-1, it is more likely a compound noun
	{
		if (debugTrace.traceDeterminer)
		{
			//printTagSet(LOG_INFO, L"_ND2", -1, tagSet, begin, fromPEMAPosition);
			wstring phrase;
			lplog(L"%d:%s[%s]:%s(%s):Noun (%d,%d) is compound, has a verb at end and a verb after the end (cost=%d). [SOURCE=%06d].", begin, (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->name.c_str(), (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->differentiator.c_str(), phraseString(begin, end, phrase, true).c_str(), m[end].word->first.c_str(), begin, end, tFI::COST_OF_INCORRECT_VERBAL_NOUN, traceSource = gTraceSource);
		}
		PNC += tFI::COST_OF_INCORRECT_VERBAL_NOUN;
	}
	// for nouns where the immediately preceding adjective in the noun is a verb, and that verb has a relation to the head noun.
	// He had *one stinging cut*.  So - does the *cut* *sting*? - head noun is subject, adjective is verb
	if (end - begin > 1 && end-begin<4 && m[end - 2].queryForm(verbForm) >= 0 && (m[end - 2].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) && 
		 m[end - 1].queryForm(nounForm)>=0 && m[end - 1].word->first != L"that" && m[end - 1].queryForm(adverbForm) < 0)
	{
		vector<wstring> determinerTypes = { L"determiner",L"demonstrative_determiner",L"possessive_determiner",L"interrogative_determiner", L"quantifier", L"numeral_cardinal" };
		bool beginIsDeterminer = false;
		for (wstring dt : determinerTypes)
			if (beginIsDeterminer = m[begin].queryWinnerForm(dt) >= 0)
				break;
		if (beginIsDeterminer)
		{
			tIWMM verbWord = m[end - 2].word, subjectWord = m[end - 1].word;
			if (verbWord->second.mainEntry != wNULL) verbWord = verbWord->second.mainEntry;
			if (subjectWord->second.mainEntry != wNULL) subjectWord = subjectWord->second.mainEntry;
			tFI::cRMap *rm = subjectWord->second.relationMaps[SubjectWordWithVerb];
			tFI::cRMap::tIcRMap tr = (rm) ? rm->r.find(verbWord) : tNULL;
			wstring patternName = (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->name;
			if (patternName != L"__S1" && patternName != L"__INTRO_N" && rm != (tFI::cRMap *)NULL && tr != rm->r.end() &&
				subjectWord->second.getUsageCost(m[end - 1].queryForm(nounForm))>0)
			{
				PNC -= subjectWord->second.getUsageCost(m[end-1].queryForm(nounForm));
				if (debugTrace.traceDeterminer)
				{
					wstring logres;
					phraseString(begin, end, logres, true);
					lplog(L"%s[%s]:%s:(head noun)subject '%s' has %d relationship count with (adjective)verb '%s'.  Cost decreased %d.",
						(fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->name.c_str(), (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->differentiator.c_str(),
						logres.c_str(),
						subjectWord->first.c_str(), tr->second.frequency, verbWord->first.c_str(),
						subjectWord->second.getUsageCost(m[end - 1].queryForm(nounForm)));
				}
			}
		}
	}
	// to interest Bob - interest would need a determiner, but not as an adjective, and after a 'to' is probably a verb
	if (end - begin == 2 && m[begin].queryForm(nounForm) >= 0 && begin > 0 && m[begin - 1].word->first == L"to" && m[begin].word->second.getUsageCost(tFI::SINGULAR_NOUN_HAS_NO_DETERMINER) == 4)
	{
		int verbCost = m[begin].word->second.getUsageCost(m[begin].queryForm(verbForm));
		if (verbCost > 0)
		{
			PNC += verbCost;
			if (debugTrace.traceDeterminer)
			{
				wstring logres;
				phraseString(begin - 1, end, logres, true);
				lplog(L"%s[%s]:%s:noun %s used as adjective without determiner after to.  Cost increased %d.",
					(fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->name.c_str(), (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->differentiator.c_str(),
					logres.c_str(), m[begin].word->first.c_str(), verbCost);
			}
		}
	}
	// I went to jail with him.
	if (begin>0 && m[begin-1].word->first==L"to" && m[begin].queryForm(verbForm)>=0 &&
			!(m[begin].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER)) && m[begin].word->second.getUsageCost(m[begin].queryForm(verbForm))<5)  
	{
		// from face to face / rock to rock / stone to stone
		if (begin >= 3 && m[begin - 3].word->first == L"from" && m[begin].word->second.getUsageCost(m[begin].queryForm(verbForm)) > 0)
		{
			if (debugTrace.traceDeterminer)
			{
				wstring phrase;
				lplog(L"%d:%s[%s]:(%s)%s: Noun (%d,%d) has 'from' 'to' construction cost-=4", begin, (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->name.c_str(), (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->differentiator.c_str(), m[begin - 1].word->first.c_str(), phraseString(begin, end, phrase, true).c_str(), begin, end);
			}
			PNC -= 4;
		}
		else
		{
			int toCost[] = { 10,8,7,5,1 };
			int toFormVerbCost = toCost[m[begin].word->second.getUsageCost(m[begin].queryForm(verbForm))];
			int missingDeterminerCost = m[begin].word->second.getUsageCost(tFI::SINGULAR_NOUN_HAS_NO_DETERMINER);
			if (missingDeterminerCost == 4)
				toFormVerbCost += missingDeterminerCost;
			if (debugTrace.traceDeterminer)
			{
				wstring phrase;
				lplog(L"%d:%s[%s]:(%s)%s: Noun (%d,%d) has verb form after 'to' - verb cost+=%d", begin, (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->name.c_str(), (fromPEMAPosition < 0) ? L"" : patterns[pema[fromPEMAPosition].getPattern()]->differentiator.c_str(), m[begin - 1].word->first.c_str(), phraseString(begin, end, phrase, true).c_str(), begin, end,
					toFormVerbCost);
			}
			PNC += toFormVerbCost; // 10
		}
	}
	//__int64 or=m[begin].objectRole;
	// Quick as a flash Tommy / in a moment Edith
	if (end-begin==3 && 
			m[end-1].forms.isSet(PROPER_NOUN_FORM_NUM) && (m[end-2].word->second.timeFlags&T_UNIT) && (m[begin].word->first==L"a" || m[begin].word->first==L"the") &&
			begin && m[begin-1].queryForm(prepositionForm)>=0)
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:Noun (%d,%d) has a time value after a preposition",begin,begin,end);
		PNC+=6;
	}
	// disallow adjectives after nouns!
	// her cold
	// P.N.C. He
	// it is time I strolled around to the Ritz
	if (end-begin>1)
	{
		int wb=begin+1;
		for (; wb<end; wb++)
		{
			if ((m[wb].word->second.query(personalPronounForm)>=0 || m[wb].word->second.query(nomForm)>=0) && 
					(m[wb-1].word->first[m[wb-1].word->first.length()-1]==L'.' || 
					((m[wb-1].word->second.timeFlags&T_UNIT) && m[wb-1].queryForm(prepositionForm)==-1))) // prepositions also have T_UNIT set
			{
				if (debugTrace.traceDeterminer)
					lplog(L"%d:Noun (%d,%d) is a pronoun@%d preceded by a noun ending in a period or time@%d [SOURCE=%06d].",begin,begin,end,wb,wb-1,traceSource=gTraceSource);
				PNC+=10;
			}
		}
	}
	// set the actual noun pattern that is being used, so that if __NOUN[9] comes first, for example, that the NOUNs in NOUN[9] don't get counted twice (once here, and once on their own).
	// problem - what if _NOUN[2] needs to be evaluated separately because _NOUN[9] is not under a certain PP.
	if (debugTrace.traceDeterminer && nounTag>=0 && !pema[abs(tagSet[nounTag].PEMAOffset)].flagSet(patternElementMatchArray::COST_ND))
	{
		if (tagSet[nounTag].isPattern)
			::lplog(LOG_INFO, L"ND NOT SET! %06d:%s[%s] %06d:%s[%s](%d,%d) TAG %s [Element=%d]", tagSet[nounTag].sourcePosition, patterns[tagSet[nounTag].parentPattern]->name.c_str(), patterns[tagSet[nounTag].parentPattern]->differentiator.c_str(),
				tagSet[nounTag].PEMAOffset, patterns[tagSet[nounTag].pattern]->name.c_str(), patterns[tagSet[nounTag].pattern]->differentiator.c_str(), tagSet[nounTag].sourcePosition, tagSet[nounTag].sourcePosition + tagSet[nounTag].len, patternTagStrings[tagSet[nounTag].tag].c_str(),
				tagSet[nounTag].parentElement);
		else
			::lplog(LOG_INFO, L"ND NOT SET! %06d %s[%s] %06d:%s(%d,%d) TAG %s [Element=%d]", tagSet[nounTag].sourcePosition, patterns[tagSet[nounTag].parentPattern]->name.c_str(), patterns[tagSet[nounTag].parentPattern]->differentiator.c_str(),
				tagSet[nounTag].PEMAOffset, Forms[tagSet[nounTag].pattern]->shortName.c_str(), tagSet[nounTag].sourcePosition, tagSet[nounTag].sourcePosition + tagSet[nounTag].len, patternTagStrings[tagSet[nounTag].tag].c_str(),
				tagSet[nounTag].parentElement);
	}
	//pema[tagSet[nounTag].PEMAOffset].setFlag(patternElementMatchArray::COST_ND);
	if (nAgreeTag < 0)
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:Noun (%d,%d) has no nAgree tag (cost=%d) [SOURCE=%06d].", begin, begin, end, PNC, traceSource = gTraceSource++);
		return PNC;
	}

	/*
	if ((subObjectTag=findOneTag(tagSet,L"SUBOBJECT",-1))>=0)
	{
		vector < vector <tTagLocation> > subobjectTagSets;
		if (startCollectTagsFromTag(false,nounDeterminerTagSet,tagSet[subObjectTag],subobjectTagSets,-1,false,L"noun determiner - SUBOBJECT")>=0)
			for (unsigned int J=0; J<subobjectTagSets.size(); J++)
			{
				if (t.traceSpeakerResolution)
					::printTagSet(LOG_RESOLUTION,L"SUBOBJECT",J,subobjectTagSets[J]);
				if (subobjectTagSets[J].empty()) continue; // shouldn't happen - but does
				PNC+=evaluateNounDeterminer(subobjectTagSets[J],true,traceSource,tagSet[subObjectTag].sourcePosition,tagSet[subObjectTag].sourcePosition+tagSet[subObjectTag].len,tagSet[subObjectTag].PEMAOffset);
			}
	}
	*/
	int whereNAgree = tagSet[nAgreeTag].sourcePosition;
	tIWMM nounWord=m[whereNAgree].word;
	if (!(nounWord->second.inflectionFlags&SINGULAR))
	{
		// disallow the adjective 'one' immediately before a noun that is plural
		// if *any one wishes* to know.
		if (begin < whereNAgree && m[whereNAgree-1].word->first == L"one")
		{
			if (debugTrace.traceDeterminer)
				lplog(L"%d:plural noun %s has an immediately preceding contradictory adjective 'one' [SOURCE=%06d].", whereNAgree, nounWord->first.c_str(), traceSource = gTraceSource++);
			PNC += 10;
		}
		else if (debugTrace.traceDeterminer)
			lplog(L"%d:noun %s is not singular [SOURCE=%06d].", whereNAgree, nounWord->first.c_str(), traceSource = gTraceSource++);
		return PNC;
	}
	if (!tagIsCertain(whereNAgree))
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:tag for noun %s is not certain [SOURCE=%06d].", whereNAgree, nounWord->first.c_str(), traceSource = gTraceSource++);
		return PNC;
	}
	// mine is both a possessive pronoun AND a noun in completely different word senses, so they have differing determiner usage.
	if (m[whereNAgree].word->first==L"mine")
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:tag for noun/pronoun %s has potentially conflicting determiner usage (PEMA=%d). [SOURCE=%06d].", whereNAgree, nounWord->first.c_str(), tagSet[nAgreeTag].PEMAOffset, traceSource = gTraceSource++);
		return PNC;
	}
	int whereDeterminerTag;
	bool hasDeterminer=(whereDeterminerTag=findTagConstrained(tagSet,L"DET",nextDet,tagSet[nounTag]))>=0;
	if (whereDeterminerTag >= 0)
	{
		wstring det = m[tagSet[whereDeterminerTag].sourcePosition].word->first;
		if (det == L"some" || det == L"much" || det == L"any") // special determiners that can combine with uncoutable nouns (Longman 4.4.4)
		{
			if (debugTrace.traceDeterminer)
				lplog(L"%d:determiner %s for noun/pronoun %s is also for use in uncountable nouns and so is invalid for determiner checking (PEMA=%d). [SOURCE=%06d].", tagSet[whereDeterminerTag].sourcePosition, det.c_str(),nounWord->first.c_str(), tagSet[nAgreeTag].PEMAOffset, traceSource = gTraceSource++);
			return PNC;
		}
	}
	int b=tagSet[nounTag].sourcePosition;
	if (!hasDeterminer && (m[b].queryForm(PROPER_NOUN_FORM)>=0 || m[b].queryForm(honorificForm)>=0 || m[b].queryForm(honorificAbbreviationForm)>=0))
	{
		//lplog(L"%d: %d %s %s",begin,b,(m[b].queryForm(PROPER_NOUN_FORM)>=0) ? L"true":L"false",(m[b].flags&WordMatch::flagNounOwner) ? L"true":L"false");
		while ((m[b].queryForm(PROPER_NOUN_FORM)>=0 || m[b].queryForm(honorificForm)>=0 || m[b].queryForm(honorificAbbreviationForm)>=0 || m[b].word->first==L".") && 
					 !(m[b].flags&WordMatch::flagNounOwner) && b<whereNAgree) b++;
		if (m[b].queryForm(PROPER_NOUN_FORM)>=0 && (m[b].flags&WordMatch::flagNounOwner) && b<whereNAgree)
			hasDeterminer=true;
	}
	// fix problem otherwise caused by separating out these relativizers which also act as determiners from the noun pattern itself
	// without this, REL1 and RELQ have costs associated with singular subjects that have separated relativizers
	// What percentage of new car sales was for hybrid cars?
	if (!hasDeterminer && begin>0 && (m[begin-1].word->first==L"which" || m[begin-1].word->first==L"what" || m[begin-1].word->first==L"whose"))
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:noun %s lacks a determiner but is led by a determiner relative [SOURCE=%06d].",whereNAgree,nounWord->first.c_str(),traceSource=gTraceSource++);
		hasDeterminer=true;
	}
	/*
		DEBUG 2 - 4330 man's manner
	// attempt to check whether this is a compound noun where a previous noun actually has the determiner
	if (!hasDeterminer && nounWord->second.usageCosts[(hasDeterminer) ? tFI::SINGULAR_NOUN_HAS_DETERMINER: tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]!=0 && 
			(wf=nounWord->second.query(nounForm))>=0 && nounWord->second.usageCosts[wf]==0 &&
			begin>3 && m[begin-1].queryForm(coordinatorForm)>=0 && m[begin-2].queryForm(nounForm)>=0 && 
			(m[begin-3].queryForm(determinerForm)>=0 || 
			 (m[begin-3].queryForm(nounForm)>=0 && ((m[begin-3].word->second.inflectionFlags&(PLURAL_OWNER|SINGULAR_OWNER)) || (m[begin-3].flags&WordMatch::flagNounOwner)) && m[begin-4].queryForm(determinerForm)>=0) ||
			 (m[begin-3].queryForm(adjectiveForm)>=0 && m[begin-4].queryForm(determinerForm)>=0)))
	{
		//if (t.traceDeterminer)
		lplog(L"%d:determiner for noun %s found in previous coordinated phrase @%d [SOURCE=%06d].",
					whereNAgree,nounWord->first.c_str(),begin-2,traceSource=gTraceSource);
		hasDeterminer=true;
	}
	*/
	if (nounWord->second.query(nounForm)>=0 && nounWord->second.query(possessivePronounForm) >= 0)
	if (nounWord->second.mainEntry!=wNULL)
		nounWord=nounWord->second.mainEntry;
	if (debugTrace.traceDeterminer)
		lplog(L"%d:singular noun %s has %s determiner (cost=%d) [SOURCE=%06d].",whereNAgree,nounWord->first.c_str(),(hasDeterminer) ? L"a":L"no",nounWord->second.getUsageCost((hasDeterminer) ? tFI::SINGULAR_NOUN_HAS_DETERMINER : tFI::SINGULAR_NOUN_HAS_NO_DETERMINER),traceSource=gTraceSource++);
	if (assessCost)
		return PNC+nounWord->second.getUsageCost((hasDeterminer) ? tFI::SINGULAR_NOUN_HAS_DETERMINER : tFI::SINGULAR_NOUN_HAS_NO_DETERMINER);
	if (updateWordUsageCostsDynamically)
		nounWord->second.updateNounDeterminerUsageCost(hasDeterminer);
	return 0;
}

void Source::sortTagLocations(vector < vector <tTagLocation> > &tagSets, vector <tTagLocation> &tagSetLocations)
{
	for (auto ts:tagSets)
		for (auto tl:ts)
			tagSetLocations.push_back(tl);
	sort(tagSetLocations.begin(), tagSetLocations.end(), tTagLocation::compareTagLocation);
}

// alternateShortTry is to allow a search for a noun determiner to occur even though there is no OBJECT, SUBJECT.  This should only be allowed on very short sentences.
// if this is allowed on all sentences, significant differences with ST will result (most of them ST is correct)
void Source::evaluateNounDeterminers(int PEMAPosition,int position,vector < vector <tTagLocation> > &tagSets,bool alternateShortTry,wstring purpose)
{ LFS // DLFS
	vector <int> costs;
	if (pema[PEMAPosition].flagSet(patternElementMatchArray::COST_ROLE))
	{
		if (debugTrace.traceDeterminer)
		{
			int pattern=pema[PEMAPosition].getPattern();
			lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (ROLE already evaluated)",position,PEMAPosition,desiredTagSets[roleTagSet].name.c_str(),
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
		}
		return;
	}
	pema[PEMAPosition].setFlag(patternElementMatchArray::COST_ROLE);
	// search for all subjects, objects, subobjects and prepobjects - DO NOT obey BLOCK! (obeyBlock=false)
	// if this is set to not obey block, testing with source 21780, there are about 30 sentences in the entire book that might have improvement.  However, the time required multiplied by 4.
	// Two sentences illustrating the difference:
	// All through morning school Reggie debated the matter with so much absorption that he had no attention to give to mischief, and in consequence earned
	// the good - conduct mark, to his great amazement.
	// If he had been able to whisper the information it would not have seemed so bad, but to be obliged to shout out what in
	// honour he ought to have kept silent about made him so angry that he hated the old man he had come so far to serve.
	// You may lay me down to sleep, my mother dear, But rock me in the cradle all the day.

	startCollectTags(debugTrace.traceDeterminer, roleTagSet, position, PEMAPosition, tagSets, true, true, L"evaluateNounDeterminers - ROLE");
	{
		if (tagSets.empty())
			tagSets.push_back(vector <tTagLocation>());
		if (tagSets.size()==1 && tagSets[0].empty())
		{
			// When a sentence is merely a command, and potentially matches a __NOUN[9], there is no OBJECT that is collected, because that is only provided by C1__S1.
			//    In this case, without this push_back, the __NOUN[9] would skip costing completely, and would have an advantage over the VERBREL1.
			//    If the verb can also be a noun, and the noun is less costly than the verb, the __NOUN[9] would win, because there is no extra cost associated with
			//    not having the determiner.
			// Climb aboard the lugger . 
			tagSets[0].push_back(tTagLocation(0,(alternateShortTry) ? pema[PEMAPosition].getPattern() : pema[PEMAPosition].getChildPattern(),pema[PEMAPosition].getPattern(),pema[PEMAPosition].getElement(),position,pema[PEMAPosition].end,-PEMAPosition,true));
			pema[PEMAPosition].removeFlag(patternElementMatchArray::COST_ROLE);
		}
		vector < vector <tTagLocation> > nTagSets;
		vector <int> nCosts, traceSources;
		// for each pattern of subjects, objects and subobjects
		vector <tTagLocation> tagLocations;
		sortTagLocations(tagSets, tagLocations);
		if (debugTrace.traceDeterminer)
		{
			for (auto tl : tagLocations)
				if (tl.isPattern)
					::lplog(LOG_INFO, L"TL %06d:%s[%s] %06d:%s[%s](%d,%d) TAG %s [Element=%d]", tl.sourcePosition, patterns[tl.parentPattern]->name.c_str(), patterns[tl.parentPattern]->differentiator.c_str(),
						tl.PEMAOffset, patterns[tl.pattern]->name.c_str(), patterns[tl.pattern]->differentiator.c_str(), tl.sourcePosition, tl.sourcePosition + tl.len, patternTagStrings[tl.tag].c_str(),
						tl.parentElement);
				else
					::lplog(LOG_INFO, L"TL %06d %s[%s] %06d:%s(%d,%d) TAG %s [Element=%d]", tl.sourcePosition, patterns[tl.parentPattern]->name.c_str(), patterns[tl.parentPattern]->differentiator.c_str(),
						tl.PEMAOffset, Forms[tl.pattern]->shortName.c_str(), tl.sourcePosition, tl.sourcePosition + tl.len, patternTagStrings[tl.tag].c_str(),
						tl.parentElement);
		}
		for (int tl=0; tl<tagLocations.size(); tl++)
		{
			// don't obey blocks when searching for objects to evaluate
			//if (patterns[tl.parentPattern]->stopDescendingTagSearch(tl.parentElement,pema[(tl.PEMAOffset<0) ? -tl.PEMAOffset:tl.PEMAOffset].__patternElementIndex,pema[(tl.PEMAOffset<0) ? -tl.PEMAOffset:tl.PEMAOffset].isChildPattern()))
			//	continue;
			if (tl > 0 && tagLocations[tl - 1]==tagLocations[tl])
				continue;
			int nPattern=tagLocations[tl].pattern,nPosition=tagLocations[tl].sourcePosition,nLen=tagLocations[tl].len,traceSource=-1;
			if (tagLocations[tl].PEMAOffset<0)
			{
				// for each iteration of that subject, object or subobject
				for (int p=patterns[nPattern]->rootPattern; p>=0; p=patterns[p]->nextRoot)
				{
					if (!m[nPosition].patterns.isSet(p)) continue;
					if (patterns[p]->hasTag(MNOUN_TAG))
						continue;
					patternMatchArray::tPatternMatch *pma=m[nPosition].pma.find(p,nLen);
					if (pma==NULL) continue;
					int nPEMAPosition=pma->pemaByPatternEnd;
					patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+nPEMAPosition;
					for (; nPEMAPosition>=0 && pem->getPattern()==p && pem->end==nLen; nPEMAPosition=pem->nextByPatternEnd,pem=pema.begin()+nPEMAPosition)
						if (!pem->begin) break;
					if (nPEMAPosition<0 || pem->getPattern()!=p || pem->end!=nLen || pem->begin) continue;
					if (pema[nPEMAPosition].flagSet(patternElementMatchArray::COST_ND))
					{
						if (debugTrace.traceDeterminer)
						{
							int pattern=pema[nPEMAPosition].getPattern();
							lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (ND already evaluated)",nPosition,nPEMAPosition,desiredTagSets[nounDeterminerTagSet].name.c_str(),
										patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),nPosition,nPosition+pema[nPEMAPosition].end);
						}
						continue;
					}
					pema[nPEMAPosition].setFlag(patternElementMatchArray::COST_ND);
					nTagSets.clear();
					nCosts.clear();
					traceSources.clear();
					// We're getting a bit unpopular here--blocking the gangway as it were. / a bit is not a subject of 'blocking'
					if (patterns[pma->getPattern()]->hasTag(GNOUN_TAG))
					{
						bool hasEmbeddedDash=false;
						for (int K=nPosition; K<nPosition+nLen-1 && !hasEmbeddedDash; K++) 
							hasEmbeddedDash=(m[K].word->first==L"--"); // must only be --, not a single dash, as single dashes are legal in adjectives and dates like to-day
						if (hasEmbeddedDash)
						{
							if (debugTrace.traceDeterminer)
								lplog(L"%d:noun has dash %s %s[%s](%d,%d) (GNOUN DASH) [SOURCE=%06d]",nPosition,desiredTagSets[nounDeterminerTagSet].name.c_str(),
										patterns[pma->getPattern()]->name.c_str(),patterns[pma->getPattern()]->differentiator.c_str(),nPosition,nPosition+nLen,traceSource=gTraceSource++);
							secondaryPEMAPositions.clear(); // normally cleared by startCollectTags which is not called here
							patternElementMatchArray::tPatternElementMatch *npemi=pema.begin()+nPEMAPosition;
							for (;  nPEMAPosition>=0 && npemi->getPattern()==p && npemi->end==nLen; nPEMAPosition=npemi->nextByPatternEnd,npemi=pema.begin()+nPEMAPosition)
							{
								patternElementMatchArray::tPatternElementMatch *epem=pema.begin()+nPEMAPosition;
								for (int ePEMAPosition=nPEMAPosition;  ePEMAPosition>=0 && epem->getPattern()==p && (epem->end-epem->begin)==nLen; ePEMAPosition=epem->nextPatternElement,epem=pema.begin()+ePEMAPosition)
									secondaryPEMAPositions.push_back(costPatternElementByTagSet(nPosition,ePEMAPosition,-1,0,pema[ePEMAPosition].getElement()));
							}
							nCosts.push_back(10);
							traceSources.push_back(traceSource);
							lowerPreviousElementCosts(secondaryPEMAPositions, nCosts, traceSources, L"hasEmbeddedDash");
							setSecondaryCosts(secondaryPEMAPositions,pma,nPosition,L"hasEmbeddedDash");
						}
						if (patterns[pma->getPattern()]->name == L"__NOUN" && patterns[pma->getPattern()]->differentiator == L"F")
						{
							int highestPNC = 0;
							// find SUBJECT
							vector < vector <tTagLocation> > subjectTagSets;
							if (startCollectTags(false, subjectVerbRelationTagSet, nPosition, nPEMAPosition, subjectTagSets, true, false, L"tags for SUBJECT") > 0)
							{
								nCosts.clear();
								traceSources.clear();
								for (unsigned int J = 0; J < subjectTagSets.size(); J++)
								{
									int nextTagIndex = -1, tagIndex = findTag(subjectTagSets[J], L"SUBJECT", nextTagIndex),PNC=0;
									if (tagIndex >= 0)
										for (unsigned int I = subjectTagSets[J][tagIndex].sourcePosition + 1; I < subjectTagSets[J][tagIndex].sourcePosition + subjectTagSets[J][tagIndex].len; I++) // must be sourcePosition+1 - do not want to penalize a determiner in the very beginning of the subject
											if (m[I].queryForm(determinerForm) >= 0 || m[I].queryForm(coordinatorForm) >= 0)
											{
												//lplog(L"%d:Noun[F] (%d,%d) subject (%d,%d) coordinator and/or determiner detected", I, nPosition, nPosition + pma->len, subjectTagSets[J][tagIndex].sourcePosition,subjectTagSets[J][tagIndex].sourcePosition + subjectTagSets[J][tagIndex].len);
												PNC++;
											}
									nCosts.push_back(PNC);
									traceSources.push_back(traceSource = gTraceSource++);
									highestPNC = max(highestPNC, PNC);
								}
							}
							// correction to __NOUN[F] after studying matches to increase cost with long subjects
							if (highestPNC > 0)
							{
								if (debugTrace.traceDeterminer)
									lplog(L"%d:__NOUN[F](%d,%d) BEGIN subject with coordinators and/or determiners - %d [SOURCE=%06d]", nPosition, nPosition, nPosition + pma->len, highestPNC, traceSource);
								lowerPreviousElementCosts(secondaryPEMAPositions, nCosts, traceSources, L"subjectHasCoordinatorsOrDeterminers");
								setSecondaryCosts(secondaryPEMAPositions, pma, nPosition, L"subjectHasCoordinatorsOrDeterminers");
								if (debugTrace.traceDeterminer)
									lplog(L"%d:__NOUN[F](%d,%d) END subject with coordinators and/or determiners - %d", nPosition, nPosition, nPosition + pma->len, highestPNC);
							}
						}
						continue;
					}
					startCollectTags(debugTrace.traceDeterminer,nounDeterminerTagSet,nPosition,nPEMAPosition,nTagSets,true,true,purpose+L" evaluateNounDeterminers - for each role - root pattern");
					int firstChildSecondaryPEMAPositionIndex = 0;
					for (unsigned int K=0; K<nTagSets.size(); K++)
					{
						if (debugTrace.traceDeterminer)
						{
							// update nPosition from secondaryPEMAPositions to get true child
							// find first position with tagset.
							for (; firstChildSecondaryPEMAPositionIndex < secondaryPEMAPositions.size() && secondaryPEMAPositions[firstChildSecondaryPEMAPositionIndex].getTagSet() != K; firstChildSecondaryPEMAPositionIndex++);
							// if this position is at the first element, then update nPEMAPosition.  This will allow printTagSet to print the first child correctly
							if (firstChildSecondaryPEMAPositionIndex < secondaryPEMAPositions.size() && secondaryPEMAPositions[firstChildSecondaryPEMAPositionIndex].getElement() == 0)
								nPEMAPosition = secondaryPEMAPositions[firstChildSecondaryPEMAPositionIndex].getPEMAPosition();
							printTagSet(LOG_INFO, L"ND1", K, nTagSets[K], nPosition, nPEMAPosition);
						}
						nCosts.push_back(evaluateNounDeterminer(nTagSets[K],true,traceSource,nPosition,nPosition+nLen, nPEMAPosition));
						traceSources.push_back(traceSource);
					}
					lowerPreviousElementCosts(secondaryPEMAPositions, nCosts, traceSources, L"nounDeterminer");
					setSecondaryCosts(secondaryPEMAPositions,pma,nPosition, L"nounDeterminer");
				} // for each iteration of each object
			}
			else if (!patterns[nPattern]->hasTag(GNOUN_TAG) && !patterns[nPattern]->hasTag(MNOUN_TAG) && tagLocations[tl].isPattern)
			{
				if (tagLocations[tl].PEMAOffset!=PEMAPosition && pema[tagLocations[tl].PEMAOffset].flagSet(patternElementMatchArray::COST_ND))
					continue;
				pema[tagLocations[tl].PEMAOffset].setFlag(patternElementMatchArray::COST_ND);
				patternMatchArray::tPatternMatch *pma=(m[nPosition].patterns.isSet(nPattern)) ? m[nPosition].pma.find(nPattern,nLen) : NULL;
				if (!pma)
				{
					lplog(L"%d:Noun Determiner %s[%s](%d,%d) not found!",nPosition,patterns[nPattern]->name.c_str(),patterns[nPattern]->differentiator.c_str(),nPosition,nPosition+nLen);
					continue;
				}
				nTagSets.clear();
				nCosts.clear();
				traceSources.clear();
				startCollectTags(debugTrace.traceDeterminer,nounDeterminerTagSet,nPosition,tagLocations[tl].PEMAOffset,nTagSets,true,true,purpose+L" evaluateNounDeterminers - for each role - specific pattern");
				for (unsigned int K=0; K<nTagSets.size(); K++)
				{
					if (debugTrace.traceDeterminer)
						printTagSet(LOG_INFO,L"ND2",K,nTagSets[K],nPosition,tagLocations[tl].PEMAOffset);
					int tTraceSource=-1;
					nCosts.push_back(evaluateNounDeterminer(nTagSets[K],true, tTraceSource,nPosition,nPosition+nLen, tagLocations[tl].PEMAOffset));
					traceSources.push_back(tTraceSource);
				}
				lowerPreviousElementCosts(secondaryPEMAPositions, nCosts, traceSources, L"nounDeterminer2");
				setSecondaryCosts(secondaryPEMAPositions,pma,nPosition, L"nounDeterminer2");
			}
		}
	}
}

void Source::evaluatePrepObjects(int PEMAPosition, int position, vector < vector <tTagLocation> > &tagSets, wstring purpose)
{
	LFS // DLFS
	if (pema[PEMAPosition].flagSet(patternElementMatchArray::COST_PREP))
	{
		if (debugTrace.tracePreposition)
		{
			int pattern = pema[PEMAPosition].getPattern();
			lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (PREP already evaluated)", position, PEMAPosition, desiredTagSets[prepTagSet].name.c_str(),
				patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position, position + pema[PEMAPosition].end);
		}
		return;
	}
	pema[PEMAPosition].setFlag(patternElementMatchArray::COST_PREP);
	if (startCollectTags(debugTrace.tracePreposition, prepTagSet, position, PEMAPosition, tagSets, true, true, L"evaluatePrepositions - PREP") > 0)
	{
		int nPattern = pema[PEMAPosition].getPattern(), nLen = pema[PEMAPosition].end- pema[PEMAPosition].begin;
		patternMatchArray::tPatternMatch *pm = m[position].pma.find(nPattern, nLen);
		vector <int> costs, traceSources;
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			int wherePrepObjectTag = findOneTag(tagSets[J], L"PREPOBJECT");
			// incorrectly having a subject pronoun in an objective position
			int cost = 0, traceSource = gTraceSource++;
			if (wherePrepObjectTag >= 0 && tagSets[J][wherePrepObjectTag].len == 1)
			{
				int nfindex,prepObjectPosition= tagSets[J][wherePrepObjectTag].sourcePosition;
				wstring word = m[prepObjectPosition].word->first;
				if (word == L"he" || word == L"she" || word == L"they")
					cost = 10;
			// leaving a noun hanging but including its possessive
				else if ((word == L"his" || word == L"her") &&
					(nfindex=m[prepObjectPosition +1].word->second.query(nounForm))>=0 &&
					m[prepObjectPosition +1].word->second.getUsageCost(nfindex) == 0)
					cost = 4;
				if (debugTrace.tracePreposition)
				{
					int pattern = pema[PEMAPosition].getPattern();
					lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) cost %d %s [SOURCE=%06d]", position, PEMAPosition, desiredTagSets[prepTagSet].name.c_str(),
						patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position, position + pema[PEMAPosition].end, cost, purpose.c_str(), traceSource);
				}
			}
			costs.push_back(cost);
			traceSources.push_back(traceSource);
		}
		if (costs.size())
		{
			lowerPreviousElementCosts(secondaryPEMAPositions, costs, traceSources, L"prepObjects");
			setSecondaryCosts(secondaryPEMAPositions, pm, position, L"prepObjects");
		}
	}
}

bool Source::assessEVALCost(tTagLocation &tl,int pattern,patternMatchArray::tPatternMatch *pm,int position, unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions,wstring purpose)
{ LFS // DLFS
	int EVALPosition=tl.sourcePosition;
	patternMatchArray::tPatternMatch *EVALpm=m[EVALPosition].pma.find(pattern,tl.len);
	if (EVALpm && !EVALpm->isEval())
	{
		vector < vector <tTagLocation> > tTagSets;
		assessCost(pm,EVALpm,position,EVALPosition,tTagSets, tertiaryPEMAPositions,false,purpose);
		EVALpm->setEval();
		return true;
	}
	return false;
}

bool Source::checkRelation(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,tIWMM verbWord,tIWMM objectWord,int relationType)
{ LFS
	if (objectWord==wNULL) return false;
	if (objectWord->second.mainEntry!=wNULL) objectWord=objectWord->second.mainEntry;
	tFI::cRMap *rm=verbWord->second.relationMaps[relationType];
	tFI::cRMap::tIcRMap tr=(rm) ? rm->r.find(objectWord) : tNULL;
	if (debugTrace.traceVerbObjects)
	{
		wchar_t temp[1024];
		int len=(parentpm) ? swprintf(temp,L"%s[%s](%d,%d) ",patterns[parentpm->getPattern()]->name.c_str(),patterns[parentpm->getPattern()]->differentiator.c_str(),parentPosition,parentpm->len+parentPosition) : 0;
		swprintf(temp+len,L"%s[%s](%d,%d) ",patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position);
		if (rm==(tFI::cRMap *)NULL || tr==rm->r.end())
			lplog(L"%s %d:verb '%s' has NO %s relationship with '%s'.",
						temp,position,verbWord->first.c_str(),getRelStr(relationType),objectWord->first.c_str());
		else
			lplog(L"%s %d:verb '%s' has %d %s relationship count with '%s'.",
						temp,position,verbWord->first.c_str(), tr->second.frequency,getRelStr(relationType),objectWord->first.c_str());
	}
	return (rm!=(tFI::cRMap *)NULL) && (tr!=rm->r.end());
}

//  desiredTagSets.push_back(tTS(VERB_OBJECTS_TAGSET,3,"VERB","V_OBJECT","OBJECT","V_AGREE","vD","vrD","IVERB",NULL));
int Source::evaluateVerbObjects(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,
																vector <tTagLocation> &tagSet,bool infinitive,bool assessCost,int &voRelationsFound,int &traceSource,wstring purpose)
{ DLFS
	voRelationsFound=0;
	// not sure whether 'to' is used as an infinitive or a preposition without pretagging
	// so only assess verb/object cost of verb, refuse to accumulate statistics
	if (!preTaggedSource && infinitive && !assessCost) 
	{
		if (debugTrace.traceVerbObjects)
			lplog(L"          %d:verb is infinitive and no assessment of cost - skipping.",position);
		return 0;
	}
	int verbTagIndex,nextObjectTag=-1,nextPassiveTag=-1;
	if ((infinitive) ? !getIVerb(tagSet,verbTagIndex) : !getVerb(tagSet,verbTagIndex)) 
	{
		// Why should she *despair* ? - despair IS a verb, but _MQ1[4](0,4) also has __ALLOBJECTS_1 with just _COND, because ALLVERB cannot be used because of the question structure 
		if (infinitive || !patterns[pm->getPattern()]->questionFlag || (verbTagIndex = findOneTag(tagSet, L"V_AGREE")) < 0)
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb not found - skipping.", position);
			return 0;
		}
	}
	wchar_t *passiveTags[]={ L"vD",L"vrD",L"vAD",L"vBD",L"vCD",L"vABD",L"vACD",L"vBCD",L"vABCD", NULL };
	bool passive=false;
	for (int pt=0; passiveTags[pt]; pt++)
		passive|=findTag(tagSet,passiveTags[pt],nextPassiveTag)>=0;
	if (passive && !patterns[pm->getPattern()]->questionFlag) 
	{
		if (debugTrace.traceVerbObjects)
			lplog(L"          %d:verb is passive in pattern %s[%s] - skipping.",position,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str());
		return 0;
	}
	int whereObjectTag=findTag(tagSet,L"OBJECT",nextObjectTag);
	// hObjects are for use with _VERB_BARE_INF - I make/made you approach him, where there is only a relationship between the subject (I) and (you)
	//int wherehObjectTag=findOneTag(tagSet,L"HOBJECT",-1); this is incorrect - the main verb doesn't have the hobject as an object, leading to incorrect cost assessment.
	//if (wherehObjectTag>=0)
	//{
	//	whereObjectTag=wherehObjectTag;
	//	nextObjectTag=-1;
	//}
	if (!tagIsCertain(tagSet[verbTagIndex].sourcePosition)) return 0;
	tIWMM verbWord=m[tagSet[verbTagIndex].sourcePosition].word;
	if (verbWord->second.mainEntry!=wNULL)
		verbWord=verbWord->second.mainEntry;
	unsigned int numObjects=0;
	int object1=-1,object2=-1,wo1=-1,wo2=-1;
	tIWMM object1Word=wNULL,object2Word=wNULL;
	//tFI::cRMap *rm=(tFI::cRMap *)NULL;
	tFI::cRMap::tIcRMap tr=tNULL;
	bool success;
	if (whereObjectTag >= 0 && patterns[pm->getPattern()]->questionFlag && (m[tagSet[whereObjectTag].sourcePosition].word->first == L"how" || m[tagSet[whereObjectTag].sourcePosition].word->first == L"when" || m[tagSet[whereObjectTag].sourcePosition].word->first == L"why"))
	{
		if (nextObjectTag >= 0)
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb %s is in a question, has two objects (%d,%d) and the first object is the how/when/why relativizer - numObjects is decremented.", tagSet[verbTagIndex].sourcePosition, verbWord->first.c_str(), tagSet[whereObjectTag].sourcePosition, tagSet[nextObjectTag].sourcePosition);
			whereObjectTag = nextObjectTag;
			nextObjectTag = -1;
		}
		else
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb %s is in a question, has one object (%d) which is the how/when/why relativizer - numObjects is decremented.", tagSet[verbTagIndex].sourcePosition, verbWord->first.c_str(), tagSet[whereObjectTag].sourcePosition);
			whereObjectTag = -1;
		}
	}
	if (whereObjectTag>=0)
	{
		if (assessCost)
			success=resolveObjectTagBeforeObjectResolution(tagSet,whereObjectTag,object1Word,purpose+ L"| verb objects object 1");
		else
			// He gave a book.
			success=resolveTag(tagSet,whereObjectTag,object1,wo1,object1Word);
		numObjects++;
	}
	if (nextObjectTag>=0)
	{
		// He gave Sally a book.
		object2=object1;
		wo2=wo1;
		object2Word=object1Word;
		object1Word=wNULL;
		if (assessCost)
		{
			if (!(success=resolveObjectTagBeforeObjectResolution(tagSet,nextObjectTag,object1Word, purpose + L"| verb objects object 2")))
				object1Word=m[tagSet[nextObjectTag].sourcePosition+tagSet[nextObjectTag].len-1].word;
		}
		else
			success=resolveTag(tagSet,nextObjectTag,object1,wo1,object1Word);
		numObjects++;
	}
	// How many times[whereObjectTag] did Susan Butcher win the Iditarod Race[nextObjectTag] ? / times acts as an adverb, but is caught as an object, which will make the correct pattern cost much more
	if (patterns[pm->getPattern()]->questionFlag && numObjects==2 && object2Word!=wNULL && (object2Word->second.timeFlags&T_UNIT)!=0 && 
		  (int)tagSet[verbTagIndex].sourcePosition<tagSet[nextObjectTag].sourcePosition && (int)tagSet[verbTagIndex].sourcePosition>tagSet[whereObjectTag].sourcePosition)
	{
		numObjects--;
		if (debugTrace.traceVerbObjects)
			lplog(L"          %d:verb %s is in a question, has two objects (%d,%d) and the second object is a time - numObjects is decremented.",tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),tagSet[whereObjectTag].sourcePosition,tagSet[nextObjectTag].sourcePosition);
	}
	// How many American Girl dolls have been sold?
	if (passive && numObjects==0)
	{
		if (debugTrace.traceVerbObjects)
			lplog(L"          %d:verb is passive and no objects detected in pattern %s[%s] - skipping.",position,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str());
		return 0;
	}
	// report cost
	if (assessCost)
	{
		// These if statements must not be combined with previous if statements!
		if (numObjects>=1 && object1Word!=wNULL && checkRelation(parentpm,pm,parentPosition,position,verbWord,object1Word,VerbWithDirectWord))
			voRelationsFound++;
		if (numObjects==2 && object2Word!=wNULL && checkRelation(parentpm,pm,parentPosition,position,verbWord,object2Word,VerbWithIndirectWord))
			voRelationsFound++;
		int verbObjectCost=0,verbAfterVerbCost=0,objectDistanceCost=0,prepForm=-1;
		unsigned int whereVerb=tagSet[verbTagIndex].sourcePosition+tagSet[verbTagIndex].len-1,nextWord=whereVerb+1;
		if (nextWord + 1 < m.size() && m[nextWord].word->first != L"no" && // There was no thought to which rocket to launch. / thought is a past verb, and no is an adverb of cost < 4.  But still should not be considered a verbafterverb.
			(m[nextWord].word->first == L"not" || m[nextWord].word->first == L"never" || // is it a not or never?
			(m[nextWord].forms.isSet(adverbForm) && m[nextWord].word->second.getUsageCost(m[nextWord].queryForm(adverbForm)) < 4 &&  // is it possibly an adverb?
				(!m[nextWord].forms.isSet(verbForm) ||                                 // and definitely not a verb (don't skip it unnecessarily)
				(m[nextWord + 1].forms.isSet(verbForm) && m[nextWord + 1].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE))))) // OR is the next word after a verb participle?
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:bumped nextWord from %d:%s to %d:%s. (case 1 - adverb)", position, nextWord, m[nextWord].word->first.c_str(), nextWord+1, m[nextWord+1].word->first.c_str());
			nextWord++; // could possibly be an adverb in between
		}
		// they were at once taken up to his suite. - be conservative by only including the most likely (lowest cost) path
		// Don't include 'that' as an adverb! / Another voice which Tommy rather thought was that[voice] of Boris replied :
		if (nextWord<m.size() && m[whereVerb].forms.isSet(isForm) && m[nextWord].forms.isSet(prepositionForm) && 
				((prepForm=m[nextWord].queryForm(prepositionForm))>=0 && m[nextWord].word->second.getUsageCost(prepForm)==0))
		{
			int maxLen=-1,element;
			if ((element = m[nextWord].pma.queryMaximumLowestCostPattern(L"_PP", maxLen)) != -1 && m[nextWord + maxLen].forms.isSet(verbForm))
			{
				if (debugTrace.traceVerbObjects)
					lplog(L"          %d:bumped nextWord from %d:%s to %d:%s. (case 2 - _PP)", position, nextWord, m[nextWord].word->first.c_str(), nextWord + maxLen, m[nextWord + maxLen].word->first.c_str());
				nextWord += maxLen;
			}
		}
		int nextAdvObjectTag = -1, advObjectTag = findTag(tagSet, L"ADVOBJECT", nextAdvObjectTag);
		int nextAdjObjectTag = -1, adjObjectTag = findTag(tagSet, L"ADJOBJECT", nextAdjObjectTag);
		// if the word after this verb is compatible (it should be included as part of the verb), yet is not included, 
		// then punch up the cost.
		if ((verbAfterVerbCost = calculateVerbAfterVerbUsage(whereVerb, nextWord, numObjects == 0 && advObjectTag >= 0)) && patterns[pm->getPattern()]->name == L"__S1" && patterns[pm->getPattern()]->differentiator == L"7")
			verbAfterVerbCost++;  // takes care of ADJECTIVE being lowered in cost in __S1[7]!
		int vsp=tagSet[verbTagIndex].sourcePosition,pp=tagSet[verbTagIndex].parentPattern;
		// make the following pattern costly:
		// we have come across his tracks. / where 'have' is verbverb and 'come' is a present tense (_VERB_BARE_INF)
		if (vsp>0 && m[vsp-1].word->first==L"have" && patterns[pp]->name==L"_VERB_BARE_INF" &&
				(verbWord->second.inflectionFlags&(VERB_PAST_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR))==(VERB_PAST_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR))
		{
			verbAfterVerbCost+=4;
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb %s is both a first-singular and past participle, and is preceded by 'have' - don't match _VERB_BARE_INF.",vsp,verbWord->first.c_str());
		}
		// if the object is a present participle, this is not an object, but rather an adverb:
		// Soon after Henrietta Hen shrieked for the rooster he came **hurrying around a corner of the barn** .   // how he came - adverb
		// at the same time, sometimes it really is an object:
		// Arthur Scott Bailey\The Tale of Freddie Firefly[4325-4330]:
		// When he saw his brothers and cousins go dancing off in **the dark he couldn't help** wanting to dance too . // what he couldn't help - object
		if (numObjects > 0 && (m[tagSet[whereObjectTag].sourcePosition].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) != 0 && verbWord->second.getUsageCost(tFI::VERB_HAS_0_OBJECTS + numObjects) > verbWord->second.getUsageCost(tFI::VERB_HAS_0_OBJECTS+numObjects-1))
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:decreased numObjects=%d to %d for verb %s because the object %s is a present participle (may be adverbial usage)",
					tagSet[verbTagIndex].sourcePosition, numObjects, numObjects-1, verbWord->first.c_str(), m[tagSet[whereObjectTag].sourcePosition].word->first.c_str());
			numObjects--;
		}
		// increase parent pattern cost at verb
		verbObjectCost=verbWord->second.getUsageCost(tFI::VERB_HAS_0_OBJECTS+numObjects);
		if (numObjects == 0 && verbWord->second.query(isForm) >= 0 && adjObjectTag >= 0)
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:decreased objectCost to 0 because of adjectiveObject and isForm.",tagSet[verbTagIndex].sourcePosition);
			verbObjectCost = 0;
		}
		if (numObjects>0)
		{
			wstring w=m[tagSet[whereObjectTag].sourcePosition].word->first;
			if (tagSet[whereObjectTag].len==1 && (w==L"i" || w==L"he" || w==L"she" || w==L"we" || w==L"they") && !patterns[pm->getPattern()]->questionFlag && 
				  (w != L"i" || (verbWord->first != L"is" && verbWord->first != L"do"))) // It is I!  So do I!
			{
				verbObjectCost+=6;
				if (debugTrace.traceVerbObjects)
					lplog(L"          %d:objectWord %s is a nominative pronoun used as an accusative.",
					tagSet[whereObjectTag].sourcePosition,(object1Word != wNULL) ? object1Word->first.c_str():L"",verbObjectCost);
			}
			int distance=tagSet[whereObjectTag].sourcePosition-(tagSet[verbTagIndex].sourcePosition+tagSet[verbTagIndex].len);
			if (distance>2) objectDistanceCost=distance-2;
			if (debugTrace.traceVerbObjects && objectDistanceCost)
				lplog(L"          %d:verb %s has 1st object start at %d - verb end at %d - 2 = %d objectDistanceCost.",
				tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),tagSet[whereObjectTag].sourcePosition,tagSet[verbTagIndex].sourcePosition+tagSet[verbTagIndex].len,objectDistanceCost);
			if (objectDistanceCost>4)
			{
				if (debugTrace.traceVerbObjects)
					lplog(L"          %d:verb %s has objectDistanceCost=%d > 4. voRelationsFound cancelled.",
					tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),objectDistanceCost);
				voRelationsFound=0;
			}
		}
		// they were all alike
		// if there is one object, and the object is 'all' and there is a match for __S1[7] (but this is not __S1[7]), and the word after the object could be an adjective, then increase cost greatly.
		// this encourages 'all' to be an adverb and the next word to be an adjective.
		if (numObjects == 1 && object1Word != wNULL && object1Word->first == L"all" && m.size() > tagSet[whereObjectTag].sourcePosition + tagSet[whereObjectTag].len &&
			m[tagSet[whereObjectTag].sourcePosition + tagSet[whereObjectTag].len].word->second.query(L"adjective") >= 0)
		{
			verbObjectCost += 6;
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb %s is followed by all with an added adverb, which is unlikely (added cost 6).", vsp, verbWord->first.c_str());
		}
		// if one object, and object follows directly after verb, and object consists of adverb, adverb, acc, then add cost.
		if (numObjects == 1 && tagSet[whereObjectTag].sourcePosition + tagSet[whereObjectTag].len < whereVerb + 5)
		{
			bool isAdverb = m[whereVerb + 1].forms.isSet(adverbForm) && m[whereVerb + 1].word->second.getUsageCost(m[whereVerb + 1].queryForm(adverbForm)) < 4; // is it possibly an adverb?
			if (isAdverb && tagSet[whereObjectTag].sourcePosition + tagSet[whereObjectTag].len == whereVerb + 3)
			{
				bool isPreposition = m[whereVerb + 1].forms.isSet(prepositionForm);
				bool objectDoesntTakeAdjectives = (m[whereVerb + 2].queryForm(accForm) != -1 || m[whereVerb + 2].queryForm(personalPronounForm) != -1) &&
					m[whereVerb + 2].word->first != L"he" && m[whereVerb + 2].word->first != L"she";
				if (isPreposition && objectDoesntTakeAdjectives)
				{
					verbObjectCost += 6;
					if (debugTrace.traceVerbObjects)
						lplog(L"          %d:verb %s is followed by an object %s with one previous adverb and the object doesn't take an adjective - more likely a prep phrase.", vsp, verbWord->first.c_str(), object1Word->first.c_str());
				}
			}
			else if (isAdverb  && tagSet[whereObjectTag].sourcePosition + tagSet[whereObjectTag].len == whereVerb + 4)
			{
				isAdverb = m[whereVerb + 2].forms.isSet(adverbForm) && m[whereVerb + 2].word->second.getUsageCost(m[whereVerb + 2].queryForm(adverbForm)) < 4; // is it possibly an adverb?
				bool isPreposition = m[whereVerb + 2].forms.isSet(prepositionForm);
				bool objectDoesntTakeAdjectives = (m[whereVerb + 3].queryForm(accForm) != -1 || m[whereVerb + 3].queryForm(personalPronounForm) != -1) &&
					m[whereVerb + 3].word->first != L"he" && m[whereVerb + 3].word->first != L"she";
				if (isAdverb && isPreposition && objectDoesntTakeAdjectives)
				{
					verbObjectCost += 6;
					if (debugTrace.traceVerbObjects)
						lplog(L"          %d:verb %s is followed by an object %s with two previous adverbs and the object doesn't take an adjective - more likely a prep phrase.", vsp, verbWord->first.c_str(), (object1Word != wNULL) ? object1Word->first.c_str():L"");
				}
			}
		}
		if (numObjects == 1 && verbObjectCost && whereVerb + 1 < m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && m[whereVerb + 1].queryForm(particleForm) >= 0 &&
			// if the particle is followed by a preposition, then don't decrease the cost of having an object because then that encourages the preposition to become an adverb.
			// also include a case where 'to' is not a preposition because of to-day and to-morrow
			(whereVerb + 3 >= m.size() || m[whereVerb + 2].queryForm(prepositionForm) < 0 || m[whereVerb + 3].word->first == L"-") &&
			verbObjectCost < 6)
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:decreased verbObjectCost=%d to %d for verb %s because of possible particle usage (%s)",
					tagSet[verbTagIndex].sourcePosition, verbObjectCost, 0, verbWord->first.c_str(), m[whereVerb + 1].word->first.c_str());
			verbObjectCost = 0;
		}
		// here or there may be considered a prepositional phrase
		// I am swimming here. (I am swimming in the pool) / I am going there now. (I am going to the store now)
		if (numObjects == 1 && verbObjectCost > verbWord->second.getUsageCost(tFI::VERB_HAS_0_OBJECTS) && tagSet[whereObjectTag].len == 1 &&
			(m[tagSet[whereObjectTag].sourcePosition].word->first == L"there" || m[tagSet[whereObjectTag].sourcePosition].word->first == L"here" || m[tagSet[whereObjectTag].sourcePosition].word->first == L"home"))
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:decreased verbObjectCost=%d to %d for verb %s because object is 'here' or 'there' or 'home' (standing in for a PP which may not be considered an object)",
					tagSet[verbTagIndex].sourcePosition, verbObjectCost, verbWord->second.getUsageCost(tFI::VERB_HAS_0_OBJECTS), verbWord->first.c_str(), m[whereVerb + 1].word->first.c_str());
			verbObjectCost = verbWord->second.getUsageCost(tFI::VERB_HAS_0_OBJECTS);
		}
		// modal auxiliaries really should not have objects!
		if (numObjects > 0 && object1Word != wNULL && object1Word->second.query(verbForm)>=0 &&
			(m[whereVerb].word->second.query(modalAuxiliaryForm) >= 0 || m[whereVerb].word->second.query(negationModalAuxiliaryForm) >= 0 || m[whereVerb].word->second.query(futureModalAuxiliaryForm) >= 0 || m[whereVerb].word->second.query(negationFutureModalAuxiliaryForm) >= 0 ||
				m[whereVerb].word->second.query(doesForm) >= 0 || m[whereVerb].word->second.query(doesNegationForm) >= 0))
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:increased verbObjectCost=%d to %d for verb %s because verb is modal auxiliary or does",
					tagSet[verbTagIndex].sourcePosition, verbObjectCost, verbObjectCost+verbWord->second.getUsageCost(tFI::VERB_HAS_1_OBJECTS), verbWord->first.c_str());
			verbObjectCost += verbWord->second.getUsageCost(tFI::VERB_HAS_1_OBJECTS);
		}
		if (numObjects==2)
		{
			int wo = tagSet[whereObjectTag].sourcePosition;
			wstring w = m[wo].word->first;
			wstring w2 = m[tagSet[nextObjectTag].sourcePosition].word->first;
			if (wo+1== tagSet[nextObjectTag].sourcePosition && m[wo].queryForm(determinerForm) >= 0 && m[tagSet[nextObjectTag].sourcePosition].queryForm(nounForm) >= 0)
			{
				verbObjectCost += 6;
				wstring tmpstr;
				if (debugTrace.traceVerbObjects && object2 >= 0)
					lplog(L"          %d:objectWord %s, immediately before %s, is a determiner (verbObjectCost=%d).",
						wo, w.c_str(), w2.c_str(), verbObjectCost);
			}
			bool prepOrSubjectDetected = false;
			if (tagSet[whereObjectTag].isPattern && tagSet[whereObjectTag].len>1)
			{
				if (debugTrace.traceVerbObjects)
						lplog(L"%d:VOC__Prep first object test from %s[%s](%d,%d) %s[%s](%d,%d) BEGIN", tagSet[verbTagIndex].sourcePosition,
							patterns[tagSet[whereObjectTag].parentPattern]->name.c_str(), patterns[tagSet[whereObjectTag].parentPattern]->differentiator.c_str(),
							pema[abs(tagSet[whereObjectTag].PEMAOffset)].begin + tagSet[whereObjectTag].sourcePosition, pema[abs(tagSet[whereObjectTag].PEMAOffset)].end + tagSet[whereObjectTag].sourcePosition,
							patterns[tagSet[whereObjectTag].pattern]->name.c_str(), patterns[tagSet[whereObjectTag].pattern]->differentiator.c_str(),
							tagSet[whereObjectTag].sourcePosition, tagSet[whereObjectTag].sourcePosition + tagSet[whereObjectTag].len);
				vector < vector <tTagLocation> > twoObjectTestTagSets;
				if (prepOrSubjectDetected=(startCollectTagsFromTag(debugTrace.traceSubjectVerbAgreement, twoObjectTestTagSet, tagSet[whereObjectTag], twoObjectTestTagSets, -1, false, true, L"verb objects 2 - prep phrase") > 0))
				{
					objectDistanceCost += 6;
					if (debugTrace.traceVerbObjects)
					{
						lplog(L"          %d:increased objectDistanceCost to %d because first object has an embedded preposition clause [tagsets follow]", wo, objectDistanceCost);
							for (auto totTagSet : twoObjectTestTagSets)
								printTagSet(LOG_INFO, L"PrepInFirstObject", -1, totTagSet, tagSet[whereObjectTag].sourcePosition, tagSet[whereObjectTag].PEMAOffset);
					}
				}
				//for (auto ppTagSet : ndPrepTagSets)
				//{
				//	int nextPrepPhraseTag=-1,prepPhraseTag=findTag(ppTagSet, L"PREP", nextPrepPhraseTag);

				//}

				if (debugTrace.traceVerbObjects)
					lplog(L"%d:VOC__Prep first object test from %s[%s](%d,%d) END", tagSet[verbTagIndex].sourcePosition, patterns[tagSet[whereObjectTag].parentPattern]->name.c_str(), patterns[tagSet[whereObjectTag].parentPattern]->differentiator.c_str(),
						pema[abs(tagSet[whereObjectTag].PEMAOffset)].begin + tagSet[whereObjectTag].sourcePosition, pema[abs(tagSet[whereObjectTag].PEMAOffset)].end + tagSet[whereObjectTag].sourcePosition);
				//wstring object1Temp, object2Temp;
				//lplog(L"PREPTEST {%s} {%s} {%s} %s", m[whereVerb].word->first.c_str(), phraseString(tagSet[whereObjectTag].sourcePosition, tagSet[whereObjectTag].sourcePosition + tagSet[whereObjectTag].len, object1Temp, true).c_str(),
				//	phraseString(tagSet[nextObjectTag].sourcePosition, tagSet[nextObjectTag].sourcePosition + tagSet[nextObjectTag].len, object2Temp, true).c_str(), (prepOrSubjectDetected) ? L"prepDetected" : L"NOPrepDetected");
			}
			if (tagSet[nextObjectTag].len==1 && (w2==L"i" || w2==L"he" || w2==L"she" || w2==L"we" || w2==L"they") && !patterns[pm->getPattern()]->questionFlag)
			{
				verbObjectCost+=6;
				wstring tmpstr;
				if (debugTrace.traceVerbObjects && object2>=0)
					lplog(L"          %d:objectWord %s is a nominative pronoun used as an accusative (verbObjectCost=%d).",
								tagSet[nextObjectTag].sourcePosition,objectString(object2,tmpstr,true).c_str(),verbObjectCost);
			}
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:increased objectDistanceCost=%d to %d (object1@%d-%d, object2@%d-%d)",
							tagSet[verbTagIndex].sourcePosition,objectDistanceCost,objectDistanceCost+(tagSet[nextObjectTag].sourcePosition-(wo+tagSet[whereObjectTag].len)),
							wo,wo+tagSet[whereObjectTag].len,
							tagSet[nextObjectTag].sourcePosition,tagSet[nextObjectTag].sourcePosition+tagSet[nextObjectTag].len);
			objectDistanceCost+=(tagSet[nextObjectTag].sourcePosition-(wo+tagSet[whereObjectTag].len));
			if (voRelationsFound<2 || objectDistanceCost>0)
				verbObjectCost<<=1;
		}
		//if (verbObjectCost==0)
		//  objectDistanceCost>>=1;
		// voRelationsFound=voRelationsFound*(4-verbObjectCost); GRADUATED RELATIONS
		if (verbObjectCost>=4) // top cost
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb %s has verbObjectCost=%d. objectDistanceCost increased from %d to %d. voRelationsFound cancelled.",
				tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),verbObjectCost,objectDistanceCost,objectDistanceCost<<1);
			objectDistanceCost<<=1;
			voRelationsFound=0; // GRADUATED RELATIONS
		}
		if (verbAfterVerbCost) // top cost
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb %s has %s=%d (nextWord=%d:%s). voRelationsFound cancelled.",
				tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),(numObjects == 0 && advObjectTag >= 0) ? L"adverbAfterIsVerbCost":L"verbAfterVerbCost",verbAfterVerbCost,nextWord,m[nextWord].word->first.c_str());
			voRelationsFound=0; // GRADUATED RELATIONS
		}
		// determine whether this is a special "after quotes" case preferVerbRel
		int afterQuoteAttributionBenefit=0;
		if (whereVerb && m[whereVerb-1].forms.isSet(quoteForm) && (m[whereVerb-1].word->second.inflectionFlags&CLOSE_INFLECTION)==CLOSE_INFLECTION &&
			m[whereVerb].forms.isSet(thinkForm) && numObjects==1 && verbAfterVerbCost==0 && patterns[pm->getPattern()]->name==L"_VERBREL1")
		{
			verbObjectCost=0;
			afterQuoteAttributionBenefit=2; // not so much that it causes VERBREL1 to absorb elements that are high cost
		}
		int deltaCost=verbObjectCost+verbAfterVerbCost+objectDistanceCost-afterQuoteAttributionBenefit;
		if (debugTrace.traceVerbObjects)
			lplog(L"          %d:verb %s has %d objects (verbObjectCost=%d, verbAfterVerbCost=%d, objectDistanceCost=%d voRelationsFound=%d afterQuoteAttributionBenefit=%d totalCost=%d) [SOURCE=%06d].",
			tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),numObjects,verbObjectCost,verbAfterVerbCost,objectDistanceCost,voRelationsFound,afterQuoteAttributionBenefit,deltaCost-COST_PER_RELATION*voRelationsFound,traceSource=gTraceSource++);
		return deltaCost;
	}
	// usage patterns being accumulated, cost is not assessed
	if (numObjects==2)
	{
		// only permit when at least one of the objects is not a NON_GENDERED_GENERAL_OBJECT_CLASS or a VERB_OBJECT_CLASS
		// I saw the book bag.
		// I gave her the book.
		if ((object1<0 || objects[object1].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS || 
											objects[object1].objectClass==NON_GENDERED_BUSINESS_OBJECT_CLASS || 
											objects[object1].objectClass==VERB_OBJECT_CLASS || !tagIsCertain(objects[object1].originalLocation)) &&
				(object2<0 || objects[object2].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS || 
											objects[object2].objectClass==NON_GENDERED_BUSINESS_OBJECT_CLASS || 
											objects[object2].objectClass==VERB_OBJECT_CLASS || !tagIsCertain(objects[object2].originalLocation)))
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:verb %s uncertainly has %d objects.",tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),numObjects);
			return 0;
		}
	}
	// otherwise, accumulate usage patterns and update cost
	if (updateWordUsageCostsDynamically)
	{
		verbWord->second.updateVerbObjectsUsageCost(numObjects);
	}
	if (debugTrace.traceVerbObjects)
		lplog(L"          %d:verb %s has %d objects.",tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),numObjects);
	return 0;
}

// accumulate pema positions with their lowest additional cost
void Source::accumulateTertiaryPEMAPositions(int tagSetOffset, int traceSource, vector <tTagLocation>  &tagSet, unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions, int tmpVOCost)
{
	for (tTagLocation its: tagSet)
	{
		if (its.isPattern && !pema[abs(its.PEMAOffset)].flagSet(patternElementMatchArray::COST_EVAL))
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"%06d cost %d [SOURCE=%06d] tertiary entered.", its.PEMAOffset, tmpVOCost, traceSource);
			auto tppi = tertiaryPEMAPositions.find(abs(its.PEMAOffset));
			if (tppi == tertiaryPEMAPositions.end())
			{
				tertiaryPEMAPositions[abs(its.PEMAOffset)].initialize(its.sourcePosition, tagSetOffset, traceSource, tmpVOCost);
			}
			else
				tppi->second.setCost(min(tppi->second.getCost(),tmpVOCost));
		}
		else if (debugTrace.traceVerbObjects)
		{
			lplog(L"%06d cost %d [SOURCE=%06d] tertiary not entered.",its.PEMAOffset,tmpVOCost,traceSource);
		}
	}
}

void Source::applyTertiaryPEMAPositions(unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions)
{
	for (auto its : tertiaryPEMAPositions)
	{
		if (!pema[its.first].flagSet(patternElementMatchArray::COST_EVAL) && !pema[its.first].flagSet(patternElementMatchArray::COST_NVO))
		{
			if (debugTrace.traceVerbObjects)
			{
				if (pema[its.first].isChildPattern())
					lplog(L"tertiaryPEMAPosition %06d:%s[%s](%d,%d) %s[%s](%d): cost increased by %d [SOURCE=%06d] (tagSet=%d).", its.first, patterns[pema[its.first].getPattern()]->name.c_str(), patterns[pema[its.first].getPattern()]->differentiator.c_str(), pema[its.first].begin + its.second.getSourcePosition(), pema[its.first].end + its.second.getSourcePosition(),
						patterns[pema[its.first].getChildPattern()]->name.c_str(), patterns[pema[its.first].getChildPattern()]->differentiator.c_str(), pema[its.first].getChildLen() + its.second.getSourcePosition(), its.second.getCost(), its.second.getTraceSource(), its.second.getTagSet());
				else
					lplog(L"tertiaryPEMAPosition %06d:%s[%s](%d,%d) %s(%d): cost increased by %d [SOURCE=%06d] (tagSet=%d).", its.first, patterns[pema[its.first].getPattern()]->name.c_str(), patterns[pema[its.first].getPattern()]->differentiator.c_str(), pema[its.first].begin + its.second.getSourcePosition(), pema[its.first].end + its.second.getSourcePosition(),
						Forms[m[its.second.getSourcePosition()].getFormNum(pema[its.first].getChildForm())]->shortName.c_str(), pema[its.first].getChildLen() + its.second.getSourcePosition(), its.second.getCost(), its.second.getTraceSource(), its.second.getTagSet());
			}
			pema[its.first].addOCostTillMax(its.second.getCost());
		}
	}
}

int Source::assessCost(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,vector < vector <tTagLocation> > &tagSets, unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions,bool alternateNounDeterminerShortTry,wstring purpose)
{ LFS
	if (debugTrace.traceVerbObjects || debugTrace.traceDeterminer || debugTrace.traceBNCPreferences || debugTrace.traceSubjectVerbAgreement)
	{
		wchar_t originPurpose[1024];
		if (parentpm==NULL)
			wsprintf(originPurpose,L"position %d:pma %d:pattern %s[%s](%d,%d) ASSESS COST.",position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position);
		else
			wsprintf(originPurpose,L"position %d:pma %d:%s[%s](%d,%d) ASSESS COST ( from %s[%s](%d,%d) ).",
							position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,
							patterns[parentpm->getPattern()]->name.c_str(),patterns[parentpm->getPattern()]->differentiator.c_str(),parentPosition,parentpm->len+parentPosition);
		purpose += L"| ";
		purpose += originPurpose;
		lplog(L"%s",originPurpose);
	}
	tagSets.clear();
	if (!pema[pm->pemaByPatternEnd].flagSet(patternElementMatchArray::COST_EVAL) && /*!stopRecursion && */startCollectTags(debugTrace.traceEVALObjects,EVALTagSet,position,pm->pemaByPatternEnd,tagSets,true,false,purpose + L"| base EVAL costing")>0)
	{
		pema[pm->pemaByPatternEnd].setFlag(patternElementMatchArray::COST_EVAL);
		bool evalAssessed=false;
		for (unsigned int J=0; J<tagSets.size(); J++)
		{
			if (debugTrace.traceEVALObjects && tagSets[J].size())
				printTagSet(LOG_INFO,L"EVAL",J,tagSets[J],position,pm->pemaByPatternEnd);
			for (unsigned int K=0; K<tagSets[J].size(); K++)
			{
				if (tagSets[J][K].PEMAOffset<0)
				{
					for (int p=patterns[tagSets[J][K].pattern]->rootPattern; p>=0; p=patterns[p]->nextRoot)
						if (m[tagSets[J][K].sourcePosition].patterns.isSet(p))
							evalAssessed|=assessEVALCost(tagSets[J][K],p,pm,position, tertiaryPEMAPositions,purpose + L"| base eval cost - root pattern");
				}
				else
					evalAssessed|=assessEVALCost(tagSets[J][K],tagSets[J][K].pattern,pm,position, tertiaryPEMAPositions,purpose + L"| base eval cost");
			}
		}
		if (evalAssessed && (debugTrace.traceVerbObjects || debugTrace.traceDeterminer || debugTrace.traceBNCPreferences || debugTrace.traceSubjectVerbAgreement))
		{
			if (parentpm==NULL)
				lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) ASSESS COST END EVAL.",position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position);
			else
				lplog(L"position %d:pma %d:%s[%s](%d,%d) ASSESS COST END EVAL ( from %s[%s](%d,%d) ).",
				position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,
				patterns[parentpm->getPattern()]->name.c_str(),patterns[parentpm->getPattern()]->differentiator.c_str(),parentPosition,parentpm->len+parentPosition);
		}
	}
	tagSets.clear();
	if (!pema[pm->pemaByPatternEnd].flagSet(patternElementMatchArray::COST_AGREE) &&
		startCollectTags(debugTrace.traceSubjectVerbAgreement,subjectVerbAgreementTagSet,position,pm->pemaByPatternEnd,tagSets,true,false, purpose + L"| base subject verb agreement")>0)
	{
		pema[pm->pemaByPatternEnd].setFlag(patternElementMatchArray::COST_AGREE);
		vector <costPatternElementByTagSet> saveSecondaryPEMAPositions=secondaryPEMAPositions; // evaluateSubjectVerbAgreement now calls startCollectTags as well...
		vector <int> costsPerTagSet,traceSources;
		for (unsigned int J=0; J<tagSets.size(); J++)                          // which erases secondaryPEMAPositions
		{
			if (J && tagSetSame(tagSets[J], tagSets[J - 1]))
			{
				costsPerTagSet.push_back(costsPerTagSet[J - 1]);
				traceSources.push_back(traceSources[J - 1]);
				if (debugTrace.traceSubjectVerbAgreement)
					lplog(L"AGREE TAGSET #%d (REPEAT OF PREVIOUS) - cost %d.", J, costsPerTagSet[J - 1]);
				continue;
			}
			else if (J>1 && tagSetSame(tagSets[J], tagSets[J - 2]))
			{
				costsPerTagSet.push_back(costsPerTagSet[J - 2]);
				traceSources.push_back(traceSources[J - 2]);
				if (debugTrace.traceSubjectVerbAgreement)
					lplog(L"AGREE TAGSET #%d (REPEAT OF PREVIOUS) - cost %d.", J, costsPerTagSet[J - 2]);
				continue;
			}
			else
			{
				if (debugTrace.traceSubjectVerbAgreement)
					printTagSet(LOG_INFO, L"AGREE", J, tagSets[J], position, pm->pemaByPatternEnd);
				int traceSource=-1;
				costsPerTagSet.push_back(evaluateSubjectVerbAgreement(parentpm,pm,parentPosition,position,tagSets[J],traceSource));
				traceSources.push_back(traceSource);
			}
		}
		if (tagSets.size())
		{
			lowerPreviousElementCosts(saveSecondaryPEMAPositions, costsPerTagSet, traceSources, L"agreement");
			if (debugTrace.traceTestSubjectVerbAgreement &&	saveSecondaryPEMAPositions.size() > 0 && saveSecondaryPEMAPositions[0].getTagSet() >= 0)
			{
				wstring sentence, originalIWord;
				for (int I = position; I < position + pm->len; I++)
				{
					getOriginalWord(I, originalIWord, false, false);
					sentence += originalIWord + L" ";
				}
				// get lowest cost E#=0
				int lowestCost = 10000,lowestElement=10000;
				for (auto p : saveSecondaryPEMAPositions)
					if ((p.getElement() == lowestElement && p.getCost() < lowestCost) || p.getElement() < lowestElement)
					{
						lowestElement = p.getElement();
						lowestCost = p.getCost();
					}
				for (auto p : saveSecondaryPEMAPositions)
					if ((p.getElement() == lowestElement && p.getCost() == lowestCost))
					{
						lplog(LOG_INFO, L"\n%s [cost=%d] %s", (lowestCost <= 0) ? L"agree" : L"DISAGREE", lowestCost, sentence.c_str());
						for (tTagLocation its : tagSets[p.getTagSet()])
						{
							wstring phrase, word;
							for (unsigned int sp = its.sourcePosition; sp < its.sourcePosition + its.len; sp++)
								phrase = phrase + getOriginalWord(sp, word, false) + L" ";
							if (its.isPattern)
								::lplog(LOG_INFO, L"%03d:%s %s[%s] %s[%s](%d,%d) TAG %s", its.sourcePosition, phrase.c_str(), patterns[its.parentPattern]->name.c_str(), patterns[its.parentPattern]->differentiator.c_str(),
									patterns[its.pattern]->name.c_str(), patterns[its.pattern]->differentiator.c_str(), its.sourcePosition, its.sourcePosition + its.len, patternTagStrings[its.tag].c_str());
							else
								::lplog(LOG_INFO, L"%03d:%s %s[%s] %s(%d,%d) TAG %s", its.sourcePosition, phrase.c_str(), patterns[its.parentPattern]->name.c_str(), patterns[its.parentPattern]->differentiator.c_str(),
									Forms[its.pattern]->shortName.c_str(), its.sourcePosition, its.sourcePosition + its.len, patternTagStrings[its.tag].c_str());
						}
					}
			}
			setSecondaryCosts(saveSecondaryPEMAPositions, pm, position, L"agreement");
		}
		if (debugTrace.traceTags)
			lplog(L"%d:======== END EVALUATING %06d %s %s[%s](%d,%d)", position, pm->pemaByPatternEnd, desiredTagSets[subjectVerbAgreementTagSet].name.c_str(),
				patterns[pema[pm->pemaByPatternEnd].getPattern()]->name.c_str(), patterns[pema[pm->pemaByPatternEnd].getPattern()]->differentiator.c_str(), position, position + pema[pm->pemaByPatternEnd].end);
	}
	tagSets.clear();
	if (!preTaggedSource)
	{
		bool infinitive=false;
		if (!pema[pm->pemaByPatternEnd].flagSet(patternElementMatchArray::COST_NVO) &&
			(startCollectTags(debugTrace.traceVerbObjects,verbObjectsTagSet,position,pm->pemaByPatternEnd,tagSets,true,false, purpose + L"| base verb objects")>0 ||
			(infinitive=(startCollectTags(debugTrace.traceVerbObjects,iverbTagSet,position,pm->pemaByPatternEnd,tagSets,true,false, purpose + L"| infinitive clause verb objects")>0))))
		{
			pema[pm->pemaByPatternEnd].setFlag(patternElementMatchArray::COST_NVO);
			vector <costPatternElementByTagSet> saveSecondaryPEMAPositions=secondaryPEMAPositions; // evaluateVerbObjects now calls startCollectTags as well...
			vector <int> costs,traceSources;
			for (unsigned int J=0; J<tagSets.size(); J++)                          // which erases secondaryPEMAPositions
			{
				if (J && tagSetSame(tagSets[J],tagSets[J-1]))
				{
					costs.push_back(costs[J-1]);
					traceSources.push_back(traceSources[J-1]);
					if (debugTrace.traceVerbObjects)
						lplog(L"VOC TAGSET #%d (REPEAT OF PREVIOUS) - cost %d.",J,costs[J-1]);
					continue;
				}
				else
				{
					int tvoRelationsFound=0,traceSource=-1;
					if (debugTrace.traceVerbObjects)
						printTagSet(LOG_INFO,L"VOC",J,tagSets[J],position,pm->pemaByPatternEnd);
					int tmpVOCost=evaluateVerbObjects(parentpm,pm,parentPosition,position,tagSets[J],infinitive,true,tvoRelationsFound,traceSource,purpose+ ((infinitive) ? L"| infinitive verb objects":L"| verb objects"));
					tmpVOCost -= COST_PER_RELATION * tvoRelationsFound;
					//accumulateTertiaryPEMAPositions(J, traceSource,tagSets[J], tertiaryPEMAPositions, tmpVOCost); / study this later - this is a way of costing the positions underneath the current pattern, but it may add more overhead than this is worth.
					costs.push_back(tmpVOCost);
					traceSources.push_back(traceSource);
				}
			}
			if (tagSets.size())
			{
				lowerPreviousElementCosts(saveSecondaryPEMAPositions, costs, traceSources, L"verbObjects");
				setSecondaryCosts(saveSecondaryPEMAPositions, pm, position, L"verbObjects");
			}
		}
		tagSets.clear();
		evaluateNounDeterminers(pm->pemaByPatternEnd,position,tagSets, alternateNounDeterminerShortTry, purpose + L"| noun determiners");
		tagSets.clear();
		// finds all first-level prepositional phrases (otherwise these are blocked because PREPOBJECT is blocked inside of a PREP
		if (startCollectTags(debugTrace.traceDeterminer,ndPrepTagSet,position,pm->pemaByPatternEnd,tagSets,true,false,purpose + L"| base noun determiner prep phrase")>0)
		{
			for (unsigned int J=0; J<tagSets.size(); J++)                          
				for (unsigned int K=0; K<tagSets[J].size(); K++)
				{
					vector < vector <tTagLocation> > ndTagSets;					
					evaluateNounDeterminers(abs(tagSets[J][K].PEMAOffset), tagSets[J][K].sourcePosition, ndTagSets, alternateNounDeterminerShortTry, purpose + L"| from prep phrases"); // this does not include blocked prepositional phrases
					ndTagSets.clear();
					evaluatePrepObjects(abs(tagSets[J][K].PEMAOffset), tagSets[J][K].sourcePosition, ndTagSets, purpose + L"| from prep phrases"); 
				}
		}
	}
	else
	{
		int bncCost=BNCPatternViolation(position,pm->pemaByPatternEnd,tagSets); // could updateMaxMatch but this could unduly restrict the patterns surviving to stage two of eliminateLoserPatterns
		if (bncCost>0 && patterns[pm->getPattern()]->isTopLevelMatch(*this,position,position+pm->len))
		{
			vector <WordMatch>::iterator im=m.begin()+position,imEnd=m.begin()+position+pm->len;
			int len=pm->len,avgCost=pm->cost+bncCost*1000/len,lowerAverageCost=pm->getAverageCost(); // COSTCALC
			for (unsigned int relpos=0; im!=imEnd; im++,relpos++)
				if (im->updateMaxMatch(len,avgCost,lowerAverageCost) && debugTrace.tracePatternElimination)
					::lplog(L"%d:Pattern %s[%s](%d,%d) established a new HIGHER maxLACMatch %d or lowest average cost %d=%d*1000/%d",
									position+relpos,
									patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),
									position,position+len,len,avgCost,pm->cost+bncCost,len); // COSTCALC
		}
		pm->addCostTillMax(bncCost);
	}
	if (debugTrace.traceVerbObjects || debugTrace.traceDeterminer || debugTrace.traceSubjectVerbAgreement || debugTrace.traceBNCPreferences)
	{
		int minAvgCostAfterAssessCost=pm->getAverageCost();
		if (parentpm==NULL)
			lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) ASSESS COST cost=%d minAvgCostAfterAssessCost=%d",position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,pm->getCost(),minAvgCostAfterAssessCost);
		else
			lplog(L"position %d:pma %d:%s[%s](%d,%d) ASSESS COST ( from %s[%s](%d,%d) ) cost=%d minAvgCostAfterAssessCost=%d",
						position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,
						patterns[parentpm->getPattern()]->name.c_str(),patterns[parentpm->getPattern()]->differentiator.c_str(),parentPosition,parentpm->len+parentPosition,pm->getCost(),minAvgCostAfterAssessCost);
	}
	return pm->getCost();
}

// AC1=incoming avgCost
// LEN1=incoming len
// AC2=this->minAvgCostAfterAssessCost
// LEN2=this->maxLACAACMatch
// if LEN2==0, match has not occurred.  set AC2=AC1 and LEN2=LEN1, return true.
// if AC2>=0 and AC1<0, set AC2=AC1 and LEN2=LEN1, return true.
// if AC2<0 and AC1>=0, return false.
// if AC2>=0 and AC1>=0,
//   if AC2>AC1 or (AC2==AC1 and LEN2<=LEN1)
//     set AC2=AC1 and LEN2=LEN1, return true.
//   else return false
// (AC2<0 and AC1<0)
//   if AC2*LEN2*LEN2> AC1*LEN1*LEN1
//     set AC2=AC1 and LEN2=LEN1, return true.
//   else return false
// this could be done in one line, but it could be more confusing.
bool WordMatch::compareCost(int AC1,int LEN1,int lowestSeparatorCost,int pmaOffset, bool alsoSet)
{ LFS
	if (lowestSeparatorCost>=0 && AC1>=lowestSeparatorCost) return false; // prevent patterns that match only separators optimally
	int AC2=minAvgCostAfterAssessCost,LEN2=maxLACAACMatch;
	bool setInternal=false;
	if (lastWinnerLACAACMatchPMAOffset == pmaOffset) setInternal = true;
	else if (LEN2==0) setInternal=true;
	else if (AC2>=0 && AC1<0) setInternal=true;
	else if (AC2<0 && AC1>=0) setInternal=false;
	else if (AC2>=0 && AC1>=0)
		setInternal= AC2>AC1 || (AC2==AC1 && LEN2<=LEN1);
	else
		setInternal = AC2*LEN2*LEN2 >= AC1*LEN1*LEN1;
	if (setInternal && alsoSet)
	{
		minAvgCostAfterAssessCost=AC1;
		maxLACAACMatch=LEN1;
		lastWinnerLACAACMatchPMAOffset = pmaOffset;
	}
	return setInternal;
}

void Source::evaluateExplicitSubjectVerbAgreement(int position, patternMatchArray::tPatternMatch *pm, vector < vector <tTagLocation> > &tagSets, unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions)
{
	// this is for NOUN[R]
	int nextWord = position + pm->len;
	int verbPosition = position + pm->len - 1;
	if (debugTrace.traceDeterminer)
		lplog(L"%d:Noun (%d,%d) is compound, testing verb=%d, nextWord=%d.", position, position, position + pm->len, verbPosition, nextWord);
	int verbAfterVerbCost = calculateVerbAfterVerbUsage(verbPosition, nextWord,false);
	if (!verbAfterVerbCost)
	{
		// if next word is an adverb, skip.
		int maxLen = -1;
		if (m[nextWord].pma.queryPattern(L"_ADVERB", maxLen) != -1)
		{
			nextWord += maxLen;
			if (debugTrace.traceDeterminer)
				lplog(L"%d:Noun (%d,%d) is compound, testing verb=%d, nextWord=%d.", position, position, position + pm->len, verbPosition, nextWord);
			verbAfterVerbCost = calculateVerbAfterVerbUsage(verbPosition, nextWord,false);
		}
	}
	if (!verbAfterVerbCost)
	{
		// if verb is an adverb, go backward.
		if (m[verbPosition].pma.queryPattern(L"_ADVERB") != -1)
		{
			verbPosition--;
			if (debugTrace.traceDeterminer)
				lplog(L"%d:Noun (%d,%d) is compound, testing verb=%d, nextWord=%d.", position, position, position + pm->len, verbPosition, nextWord);
			verbAfterVerbCost = calculateVerbAfterVerbUsage(verbPosition, nextWord,false);
		}
	}
	if (verbAfterVerbCost)
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:Noun (%d,%d) is compound, has a verb at end and a verb after the end.", position, position, position + pm->len);
		int PEMAPosition = pm->pemaByPatternEnd;
		pema[PEMAPosition].addOCostTillMax(tFI::COST_OF_INCORRECT_VERBAL_NOUN);
		if (debugTrace.traceDeterminer)
			lplog(L"%d:Added %d cost to %s[%s](%d,%d).", position, tFI::COST_OF_INCORRECT_VERBAL_NOUN,
				patterns[pema[PEMAPosition].getPattern()]->name.c_str(), patterns[pema[PEMAPosition].getPattern()]->differentiator.c_str(), position + pema[PEMAPosition].begin, position + pema[PEMAPosition].end);
	}
	assessCost(NULL, pm, -1, position, tagSets, tertiaryPEMAPositions, false, L"eliminate loser patterns - explicit subject verb agreement");
}

void Source::evaluateExplicitNounDeterminerAgreement(int position, patternMatchArray::tPatternMatch *pm, vector < vector <tTagLocation> > &tagSets, unordered_map <int, costPatternElementByTagSet> &tertiaryPEMAPositions)
{
	assessCost(NULL, pm, -1, position, tagSets, tertiaryPEMAPositions, false, L"eliminate loser patterns - explicit noun determiner agreement");
}

// at each position in m:
//   for each pma entry not already marked as winner
//     for each position from begin to end
//       if maxMatch==len then mark pma entry winner
//     if pma is marked winner mark children
//
// copy all winners in place and minimize pema && pma
#define PRE_ASSESS_COST_ALLOWANCE 4
int Source::eliminateLoserPatterns(unsigned int begin,unsigned int end)
{ LFS
	vector < vector <tTagLocation> > tagSets;
	unordered_map <int, costPatternElementByTagSet> tertiaryPEMAPositions;
	tagSets.reserve(4096);
	vector <int> minSeparatorCost;
	vector < vector <unsigned int> > winners; // each winner is a PMAOffset
	minSeparatorCost.reserve(end-begin+1);
	for (unsigned int I=0; I<end-begin+1 && I<m.size()-begin; I++)
		minSeparatorCost[I]=m[begin+I].word->second.lowestSeparatorCost();
	int effectiveSentenceLength = end - begin;
	if (WordClass::isDoubleQuote(m[end - 1].word->first[0]) || WordClass::isSingleQuote(m[end - 1].word->first[0]))
		effectiveSentenceLength--;
	if (WordClass::isDoubleQuote(m[begin].word->first[0]) || WordClass::isSingleQuote(m[begin].word->first[0]))
		effectiveSentenceLength--;
	int matchedPositions=0;
	for (unsigned int position=begin; position<end && !exitNow; position++)
	{
		if (debugTrace.tracePatternElimination)
			lplog(L"position %d:PMA count=%d ----------------------",position,m[position].pma.count);
		if (debugTrace.traceTestSubjectVerbAgreement)
		{
			auto mc = metaCommandsEmbeddedInSource.find(position);
			if (mc != metaCommandsEmbeddedInSource.end())
				lplog(LOG_INFO, L"\n*****  %s  *****", mc->second.c_str());
		}
		vector <unsigned int> preliminaryWinnersPreAssessCost;
		patternMatchArray::tPatternMatch *pm=m[position].pma.content;
		for (unsigned int pmIndex=0; pmIndex<m[position].pma.count; pmIndex++,pm++)
		{
			cPattern *p=patterns[pm->getPattern()];
			bool isWinner=false;
			if (!p->isTopLevelMatch(*this,position,position+pm->len))
			{
				if (debugTrace.tracePatternElimination)
					lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 1 not marked as winner (%s).",position,pmIndex,
					p->name.c_str(),p->differentiator.c_str(),position,pm->len+position,(p->onlyAloneExceptInSubPatternsFlag)? L"onlyAloneExceptInSubPatternsFlag" : L"no fill flag");
				if (p->explicitSubjectVerbAgreement)
					evaluateExplicitSubjectVerbAgreement(position, pm, tagSets, tertiaryPEMAPositions);
				if (p->explicitNounDeterminerAgreement)
					evaluateExplicitNounDeterminerAgreement(position, pm, tagSets, tertiaryPEMAPositions);
				continue;
			}
			unsigned int bp;
			int len=pm->len; // COSTCALC
			int paca=PRE_ASSESS_COST_ALLOWANCE*len/5; // adjust for length - the longer a pattern is the more other patterns
			if (effectiveSentenceLength <= 2) // if this is changed to len == 1, differences with ST will increase significantly
				paca = PRE_ASSESS_COST_ALLOWANCE; // this is to allow expressions like Help! to be verbs even though the verb is more expensive than the noun, but also to allow VO and ND testing
			// which may be better now may be hurt in the second phase, so allow for a lower bar to pass into the second phase.
			int averageCost=(pm->getCost()-paca)*1000/len; // - to allow for patterns that aren't perfect to start with to slide in
			// "--" is the only separator that is also a word class. if a pattern has not matched this, it should be regarded as equal to a pattern
			// that has. -- took out 11/4/2005 because it is not defined which other patterns include the dash, and if a pattern that does not include
			// the dash is compared with another pattern that also does not include the dash, and is one less, then that lesser matching pattern is considered
			// equal (by this dashFirst that is being removed) and that messes up the costing in the if statement below, leaving completely unmatched words
			// potentially *--* example: father's a dear -- I am awfully fond of him -- but you have no idea how I worry him !
			//int dashFirst=(m[position-1].word->first=="--") ? 1:0;
			// is there even one position which the pattern matches the maximum length?
			// ALSO, prevent patterns that match top forms like quotes, commas and so-forth but are otherwise not winners from winning.
			for (bp=position;
				bp<len+position && !(isWinner=(m[bp].maxWinner(len,averageCost,minSeparatorCost[bp-begin])));
				bp++)
			{
				if (debugTrace.tracePatternElimination)
					lplog(L"%d:%s[%s](%d,%d) NOT winner PHASE 1 (len=%d averageCost=%d (%d*1000/%d) isSeparator=%s lowestAverageCost=%d maxLACMatch=%d)",bp,p->name.c_str(),p->differentiator.c_str(),position,len+position,
					len,averageCost,pm->getCost(),len,(m[bp].word->second.isSeparator()) ? L"true" : L"false",m[bp].lowestAverageCost,m[bp].maxLACMatch); // COSTCALC
			}
			if (isWinner && debugTrace.tracePatternElimination)
				lplog(L"%d:%s[%s](%d,%d) WINNER PHASE 1 (len=%d averageCost=%d isSeparator=%s lowestAverageCost=%d maxLACMatch=%d)",bp,p->name.c_str(),p->differentiator.c_str(),position,len+position,
				len,averageCost,(m[bp].word->second.isSeparator()) ? L"true" : L"false",m[bp].lowestAverageCost,m[bp].maxLACMatch);
			if (isWinner)
				preliminaryWinnersPreAssessCost.push_back(pmIndex);
			else if (debugTrace.tracePatternElimination)
				lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 1 is not a winner (%d<%d) OR averageCost=%d.",position,pmIndex,
				p->name.c_str(),p->differentiator.c_str(),position,len+position,len,m[position].maxLACMatch,averageCost);
		}
		for (unsigned int I=0; I<preliminaryWinnersPreAssessCost.size(); I++)
		{
			pm=m[position].pma.content+preliminaryWinnersPreAssessCost[I];
			bool includeExtraProcessing = effectiveSentenceLength <= 2;
			if (patterns[pm->getPattern()]->name == L"__NOUN")
			{
				includeExtraProcessing |= (pm->len + 1 >= effectiveSentenceLength) && effectiveSentenceLength<4;
				//includeExtraProcessing |= (position > begin && m[position - 1].queryForm(coordinatorForm) >= 0 && (int)(position - begin + pm->len + 1) >= effectiveSentenceLength); // resulted in many errors
			}
			assessCost(NULL,pm,-1,position,tagSets,tertiaryPEMAPositions, includeExtraProcessing,L"eliminate loser patterns - preliminary winners");
		}
		winners.push_back(preliminaryWinnersPreAssessCost);
		//for (int t1 = 0; t1 < preliminaryWinnersPreAssessCost.size(); t1++)
		//	lplog(L"%d:TDLELP PHASE 1 preliminaryWinnersPreAssessCost %d", winners.size() - 1, preliminaryWinnersPreAssessCost[t1]); 
	}
	applyTertiaryPEMAPositions(tertiaryPEMAPositions);
	for (unsigned int position=begin; position<end && !exitNow; position++)
	{
		patternMatchArray::tPatternMatch *pm;
		int maxLen=0;
		vector <unsigned int> preliminaryWinners;
		for (unsigned int I=0; I<winners[position-begin].size(); I++)
		{
			if (position - begin >= winners.size() || I >= winners[position - begin].size() || m[position].pma.count <= ((int)winners[position - begin][I]))
				lplog(LOG_FATAL_ERROR, L"%d:%d:illegal winner index found during PHASE 2 of eliminateLoserPatterns.", position, I);
			pm=m[position].pma.content+winners[position-begin][I];
			cPattern *p=patterns[pm->getPattern()];
			if (pm->isWinner())
				lplog(LOG_FATAL_ERROR, L"Shouldn't happen!");
			if (!pm->isWinner() && preferVerbRel(position,winners[position-begin][I],p))
			{
				int len=pm->len;
				maxLen=max(maxLen,len);
				bool globalMinAvgCostAfterAssessCostWinner=false;
				int patternFlagCost=0;
				if (p->onlyAloneExceptInSubPatternsFlag && position && (patternFlagCost=m[position-1].word->second.lowestSeparatorCost())<0)
					patternFlagCost=0;
				int minAvgCostAfterAssessCost=pm->getAverageCost(patternFlagCost);
				for (unsigned int bp=position; bp<position+len; bp++)
					if (m[bp].compareCost(minAvgCostAfterAssessCost,len,minSeparatorCost[bp-begin], winners[position - begin][I],true))
					{
						globalMinAvgCostAfterAssessCostWinner=true;
						if (debugTrace.tracePatternElimination)
							lplog(L"%d:%s[%s](%d,%d) PHASE 2 updated GMACAACW (%d<%d) [cost=%d len=%d].",bp,p->name.c_str(),p->differentiator.c_str(),position,len+position,
							minAvgCostAfterAssessCost,m[bp].minAvgCostAfterAssessCost,pm->getCost(),len);
					}
					else if (debugTrace.tracePatternElimination)
						lplog(L"%d:%s[%s](%d,%d) PHASE 2 lost GMACAACW (%d>%d) (OR %d>%d) [cost=%d len=%d].",bp,p->name.c_str(),p->differentiator.c_str(),position,len+position,
						minAvgCostAfterAssessCost,m[bp].minAvgCostAfterAssessCost,m[bp].maxLACAACMatch,len,pm->getCost(),len);
				if (globalMinAvgCostAfterAssessCostWinner)
					preliminaryWinners.push_back(winners[position-begin][I]);
				if (debugTrace.traceVerbObjects || debugTrace.traceDeterminer || debugTrace.traceBNCPreferences || debugTrace.tracePatternElimination)
					lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 2 is %smarked a preliminary winner (cost=%d minAvgCostAfterAssessCost=%d.",position,winners[position-begin][I],
					p->name.c_str(),p->differentiator.c_str(),position,len+position,(globalMinAvgCostAfterAssessCostWinner) ? L"":L"not ",pm->getCost(),minAvgCostAfterAssessCost);
			}
		}
		winners[position-begin]=preliminaryWinners;
		//for (int t1 = 0; t1 < preliminaryWinners.size(); t1++)
		//	lplog(L"%d:TDLELP PHASE 2 preliminaryWinners %d", position - begin, preliminaryWinners[t1]);  
	}
	if (debugTrace.tracePatternElimination)
		for (unsigned int bp=begin; bp<end; bp++)
			lplog(L"position %d: PHASE 2 cost=%d len=%d",bp,m[bp].minAvgCostAfterAssessCost,m[bp].maxLACAACMatch);
	for (unsigned int position=begin; position<end && !exitNow; position++)
	{
		patternMatchArray::tPatternMatch *pm;
		for (unsigned int winner=0; winner<winners[position-begin].size(); winner++)
		{
			if (position - begin >= winners.size() || winner >= winners[position - begin].size() || m[position].pma.count <= ((int)winners[position - begin][winner]))
				lplog(LOG_FATAL_ERROR, L"%d:%d:illegal winner index found during PHASE 3 of eliminateLoserPatterns.", position, winner);
			pm=m[position].pma.content+winners[position-begin][winner];
			cPattern *p=patterns[pm->getPattern()];
			int len=pm->len;
			//if (!preferS1(position,winners[position-begin][winner])) continue;
			bool globalMinAvgCostAfterAssessCostWinner=false;
			int patternFlagCost=0;
			if (p->onlyAloneExceptInSubPatternsFlag && position && (patternFlagCost=m[position-1].word->second.lowestSeparatorCost())<0)
				patternFlagCost=0;
			int minAvgCostAfterAssessCost=pm->getAverageCost(patternFlagCost);
			unsigned int bp;
			for (bp=position; bp<position+len && !globalMinAvgCostAfterAssessCostWinner; bp++)
				if (m[bp].compareCost(minAvgCostAfterAssessCost, len, minSeparatorCost[bp - begin], winners[position - begin][winner], false))
				{
					globalMinAvgCostAfterAssessCostWinner = true;
					if (debugTrace.tracePatternElimination)
						lplog(L"%d:%s[%s](%d,%d) PHASE 3 winner (no update) GMACAACW (%d<%d) [cost=%d len=%d].", bp, p->name.c_str(), p->differentiator.c_str(), position, len + position,
							minAvgCostAfterAssessCost, m[bp].minAvgCostAfterAssessCost, pm->getCost(), len);
				}
				else if (debugTrace.tracePatternElimination)
				{
					if (m[bp].lastWinnerLACAACMatchPMAOffset >= 0)
					{
						patternMatchArray::tPatternMatch *winnerPM = m[position].pma.content + m[bp].lastWinnerLACAACMatchPMAOffset;
						cPattern *wp = patterns[pm->getPattern()];
						lplog(L"%d:PMOffset %d:%s[%s](%d,%d) PHASE 3 lost GMACAACW (%d>%d) (OR %d>%d) [cost=%d len=%d] against PMOffset %d:%s[%s](%d,%d).", bp, winners[position - begin][winner], p->name.c_str(), p->differentiator.c_str(), position, len + position,
							minAvgCostAfterAssessCost, m[bp].minAvgCostAfterAssessCost, m[bp].maxLACAACMatch, len, pm->getCost(), len,
							m[bp].lastWinnerLACAACMatchPMAOffset,wp->name.c_str(), wp->differentiator.c_str(), position, winnerPM->len + position);
					}
					else
						lplog(L"%d:%s[%s](%d,%d) PHASE 3 lost GMACAACW (%d>%d) (OR %d>%d) [cost=%d len=%d] (no winner set)", bp, p->name.c_str(), p->differentiator.c_str(), position, len + position,
							minAvgCostAfterAssessCost, m[bp].minAvgCostAfterAssessCost, m[bp].maxLACAACMatch, len, pm->getCost(), len);

				}
			if (globalMinAvgCostAfterAssessCostWinner)
			{
				// if a pattern matching just the separator (for example - 'and') goes up against a separator of lower cost, reject.
				if (pm->len==1 && m[position].word->second.isSeparator() && m[position].word->second.getLowestTopLevelCost()<pm->getCost())
				{
					if (debugTrace.tracePatternElimination)
						lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 3 NOT winner separator has lower cost (%d < %d)",position,winners[position-begin][winner],
						patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,
						m[position].word->second.getLowestTopLevelCost(),pm->getCost());
				}
				else
				{
					if (debugTrace.tracePatternElimination)
						lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 3 winner (cost=%d minAvgCostAfterAssessCost %d first winning position=%d)",position,winners[position-begin][winner],
						patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,
						pm->getCost(),minAvgCostAfterAssessCost,bp-1);
					bool reassessParentCosts = false;
					markChildren(pema.begin()+pm->pemaByPatternEnd,position,0,MIN_INT, tertiaryPEMAPositions,reassessParentCosts);
					patterns[pm->getPattern()]->numWinners++;
					pm->setWinner();
					matchedPositions+=pm->len;
				}
			}
			else if (debugTrace.tracePatternElimination)
				lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 3 not winner (minAvgCostAfterAssessCost %d, len=%d)",position,winners[position-begin][winner],
				patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,
				minAvgCostAfterAssessCost,len);
		}
	}
	matchedPositions-=end-begin+1;// +1 for the period, which isn't usually matched
	for (unsigned int position = begin; position < end && !exitNow; position++)
	{
		patternMatchArray::tPatternMatch *pm = m[position].pma.content;
		for (int PMAElement = 0, count = m[position].pma.count; PMAElement < count; PMAElement++, pm++)
			if (pm->isWinner() && eraseWinnerFromRecalculatingAloneness(position, pm))
			{
				for (unsigned bp = position; bp < position + pm->len; bp++)
					if (m[bp].lastWinnerLACAACMatchPMAOffset == PMAElement)
					{
						m[bp].lastWinnerLACAACMatchPMAOffset = -1;
						if (debugTrace.tracePatternElimination)
							lplog(L"%d:%s[%s](%d,%d) PHASE 4 Marked lastWinnerLACAACMatchPMAOffset to -1.", bp, patterns[pm->getPattern()]->name.c_str(), patterns[pm->getPattern()]->differentiator.c_str(), position, pm->len + position);
					}
			}
		m[position].maxMatch = 0;
	}
	return (matchedPositions<0) ? 0 : matchedPositions;
}
