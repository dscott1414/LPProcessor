#include <windows.h>
#include <io.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "malloc.h"
#include "profile.h"

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
unsigned int Source::getAllLocations(unsigned int position,int parentPattern,int rootPattern,int childLen,int parentLen,vector <unsigned int> &allLocations)
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
				assessCost(NULL,pm,-1,position,tagSets);
			}
			minCost=min(minCost,pm->getCost());
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
int Source::getMinCost(patternElementMatchArray::tPatternElementMatch *pem)
{ LFS
	int minCost=MAX_COST,pattern=pem->getPattern(),relativeEnd=pem->end,relativeBegin=pem->begin;
	for (; pem && pem->getPattern()==pattern && pem->end==relativeEnd; pem=(pem->nextByPatternEnd<0) ? NULL : pema.begin()+pem->nextByPatternEnd)
		if (pem->begin==relativeBegin)
			minCost=min(minCost,pem->getOCost());
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
int Source::markChildren(patternElementMatchArray::tPatternElementMatch *pem,int position,int recursionLevel,int allRootsLowestCost)
{ LFS
	// find first position with pattern and relativeEnd
	int pattern=pem->getPattern(),begin=pem->begin+position,end=pem->end+position,relativeEnd=pem->end,relativeBegin=pem->begin;
	int   numChildren=0,minCost=(allRootsLowestCost==MIN_INT) ? getMinCost(pem) : allRootsLowestCost;
	if (debugTrace.tracePatternElimination)
		lplog(L"%*s%d:MC pattern %s[%s](%d,%d) element #%d minCost=%d-----------------",recursionLevel*2," ",
		position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,pem->getElement(),minCost);
	for (; pem && pem->getPattern()==pattern && pem->end==relativeEnd && !exitNow; pem=(pem->nextByPatternEnd<0) ? NULL : pema.begin()+pem->nextByPatternEnd)
		if (pem->begin==relativeBegin)
		{
			if (pem->getOCost()!=minCost)
			{
				if (debugTrace.tracePatternElimination)
				{
					if (pem->isChildPattern())
						lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %s[%s](%d,%d) PEMA rejected (cost %d > minCost %d).",recursionLevel*2," ",position,
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,pem->getOCost(),
						patterns[pem->getChildPattern()]->name.c_str(),patterns[pem->getChildPattern()]->differentiator.c_str(),position,position+pem->getChildLen(),
						pem->getOCost(),minCost);
					else
						lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %d form PEMA rejected (cost %d > minCost %d).",recursionLevel*2," ",position,
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,pem->getOCost(),
						pem->getChildForm(),pem->getOCost(),minCost);
				}
				continue;
			}
			if (debugTrace.tracePatternElimination)
			{
				if (pem->isChildPattern())
					lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %s[%s](%d,%d) PEMA set winner.",recursionLevel*2," ",position,
					patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,minCost,
					patterns[pem->getChildPattern()]->name.c_str(),patterns[pem->getChildPattern()]->differentiator.c_str(),position,position+pem->getChildLen());
				else
					lplog(L"%*sMC position %d:pattern %s[%s](%d,%d)*%d child %d PEMA set winner.",recursionLevel*2," ",position,
					patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,minCost,
					pem->getChildForm());
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
				int lowestCost=getAllLocations(position,pattern,rootp,childLen,end-begin,allLocations);
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
						continue;
					if (debugTrace.tracePatternElimination)
						lplog(L"%*sMC position %d:pattern %s[%s](%d,%d) child %s[%s](%d,%d) PMA set winner (cost %d<=lowest cost %d).",
						recursionLevel*2," ",position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,
						patterns[childp]->name.c_str(),patterns[childp]->differentiator.c_str(),position,position+childLen,pm->getCost(),lowestCost);
					numChildren+=markChildren(pema.begin()+pm->pemaByPatternEnd,position,recursionLevel+1,lowestCost);
					patterns[pm->getPattern()]->numChildrenWinners++;
					pm->setWinner();
				}
			}
			else
			{
				if (debugTrace.tracePatternElimination)
					lplog(L"%*sMC position %d:pattern %s[%s](%d,%d) child %d FORM set winner.",
					recursionLevel*2," ",position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,pem->getChildForm());
				m[position].setWinner(pem->getChildForm());
			}
			int nextPatternElement=pem->nextPatternElement;
			if (nextPatternElement>=0)
				numChildren+=markChildren(pema.begin()+nextPatternElement,begin-pema[nextPatternElement].begin,recursionLevel,allRootsLowestCost);
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
			if (debugTrace.traceAgreement)
				lplog(L"%d:lowest cost tag %s costs %d (OCost=%d + parentCost %d) < old cost of %d",lowestCostTag.sourcePosition,tagName,tmpCost,pema[abs(tagSet[where].PEMAOffset)].getOCost(),parentCost,cost);
			cost=tmpCost;
			return true;
		}
		else if (debugTrace.traceAgreement)
			lplog(L"%d:agreement tag %s not lowest cost (OCost %d + parentCost %d>=%d)",lowestCostTag.sourcePosition,tagName,tmpCost-parentCost,parentCost,cost);
	}
	else if (debugTrace.traceAgreement)
		lplog(L"%d:agreement tag %s not found",lowestCostTag.sourcePosition,tagName);
	return false;
}

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
int Source::evaluateAgreement(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,unsigned parentPosition,unsigned int position,vector<tTagLocation> &tagSet,int &traceSource)
{ LFS
	vector < vector<tTagLocation> > subjectTagSets;
	int nextVerb=-1,nextSubject=-1,nextMainVerb=-1,nextConditionalTag=-1,nextFutureTag=-1;
	int whereSubject=findTag(tagSet,L"SUBJECT",nextSubject);
	if (whereSubject<0) return 0;
	// evaluate if SUBJECT is an _ADJECTIVE and ALSO a _NAME.  This is not allowed as a subject
	// A _NAME is allowed as an adjective (Dover Street Pub), and an _ADJECTIVE is allowed as a subject (The poor)
	// but a _NAME as a subject should be an OBJECT, not classified as an ADJECTIVE.
	// This allows _ADJECTIVE to be general, and still disallows _S1[4] from using a _NAME as subject.
	// Many[annie] ishas the time Annie ishas said to me[albert] :
	//wchar_t temp2[1024];
	//if (tagSet[whereSubject].PEMAOffset>=0 && pema[tagSet[whereSubject].PEMAOffset].getPattern()>=0)
	//	lplog(L"%d:SUBJECT %s",tagSet[whereSubject].sourcePosition,pema[tagSet[whereSubject].PEMAOffset].toText(tagSet[whereSubject].sourcePosition,temp2,m));
	int nounPosition=-1,nameLastPosition=-1;
	bool singularSet=false,pluralSet=false,restateSet=false;
	if (tagSet[whereSubject].len>1)
	{
		int nounCost=1000000,GNounCost=1000000,nameCost=1000000,sTagSet=-1;
		tTagLocation ntl(0,0,0,0,0,0,0,false),gtl(0,0,0,0,0,0,0,false),natl(0,0,0,0,0,0,0,false);
		subjectTagSets.clear();
		tTagLocation *tl=&tagSet[whereSubject];
		int pattern=tl->pattern,tagSetSubjectPosition=tl->sourcePosition,end=tl->len,PEMAOffset=tl->PEMAOffset;
		if (patterns[pattern]->includesDescendantsAndSelfAllOfTagSet&((__int64)1<<subjectTagSet))
		{
			if (PEMAOffset<0)
			{
				for (int p=patterns[pattern]->rootPattern; p>=0; p=patterns[p]->nextRoot)
				{
					if (!m[tagSetSubjectPosition].patterns.isSet(p)) continue;
					patternMatchArray::tPatternMatch *pma=m[tagSetSubjectPosition].pma.find(p,end);
					if (pma==NULL) continue;
					PEMAOffset=pma->pemaByPatternEnd;
					patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+PEMAOffset;
					for (; PEMAOffset>=0 && pem->getPattern()==p && pem->end==end; PEMAOffset=pem->nextByPatternEnd,pem=pema.begin()+PEMAOffset)
						if (!pem->begin) break;
					if (PEMAOffset<0 || pem->getPattern()!=p || pem->end!=end || pem->begin) continue;
					int startSize=subjectTagSets.size();
					int parentCost=pema[abs(PEMAOffset)].getOCost();
					startCollectTags(debugTrace.traceAgreement,subjectTagSet,tagSetSubjectPosition,PEMAOffset,subjectTagSets,true,true);
					// find lowest cost noun or gnoun reference
					for (unsigned int K=startSize; K<subjectTagSets.size(); K++)
					{
						if (debugTrace.traceAgreement)
							printTagSet(LOG_INFO,L"AGREE-SUBJECT (1)",K,subjectTagSets[K],tagSetSubjectPosition,tagSet[whereSubject].PEMAOffset);
						// both of the below statements must execute!  (no || )
						int nagreeNextTag,nextTag;
						bool foundMNoun=false;
						if (findLowCostTag(subjectTagSets[K],GNounCost,L"GNOUN",gtl,parentCost,nextTag) && GNounCost<nounCost && GNounCost<nameCost) sTagSet=K;
						if ((foundMNoun=findLowCostTag(subjectTagSets[K],GNounCost,L"MNOUN",gtl,parentCost,nextTag)) && GNounCost<nounCost && GNounCost<nameCost) sTagSet=K;
						// if there is an mnoun and 2 nagrees (or more), then don't count the nagree because the mnoun will produce 2 or more nagrees inside it and lowCost tag only counts the cost
						//     of one of the nagrees, so it would win, when clearly the mnoun should win.
						if (findLowCostTag(subjectTagSets[K],nounCost,L"N_AGREE",ntl,parentCost,nagreeNextTag) && nounCost<GNounCost && nounCost<nameCost && (nagreeNextTag==-1 || !foundMNoun)) sTagSet=K;
						if (findLowCostTag(subjectTagSets[K],nameCost,L"NAME",natl,parentCost,nextTag) && nameCost<nounCost && nameCost<GNounCost) sTagSet=K;
					}
				}
			}
			else
			{
				startCollectTags(debugTrace.traceAgreement,subjectTagSet,tagSetSubjectPosition,PEMAOffset,subjectTagSets,true,true);
				// find lowest cost noun or gnoun reference
				for (unsigned int K=0; K<subjectTagSets.size(); K++)
				{
					if (debugTrace.traceAgreement)
						printTagSet(LOG_INFO,L"AGREE-SUBJECT (2)",K,subjectTagSets[K],tagSetSubjectPosition,tagSet[whereSubject].PEMAOffset);
					// both of the below statements must execute!  (no || )
					int nagreeNextTag,nextTag;
					bool foundMNoun=false;
					if (findLowCostTag(subjectTagSets[K],GNounCost,L"GNOUN",gtl,0,nextTag) && GNounCost<nounCost && GNounCost<nameCost) sTagSet=K;
					if ((foundMNoun=findLowCostTag(subjectTagSets[K],GNounCost,L"MNOUN",gtl,0,nextTag)) && GNounCost<nounCost && GNounCost<nameCost) sTagSet=K;
						// if there is an mnoun and 2 nagrees (or more), then don't count the nagree because the mnoun will produce 2 or more nagrees inside it and lowCost tag only counts the cost
						//     of one of the nagrees, so it would win, when clearly the mnoun should win.
					if (findLowCostTag(subjectTagSets[K],nounCost,L"N_AGREE",ntl,0,nagreeNextTag) && nounCost<GNounCost && nounCost<nameCost && (nagreeNextTag==-1 || !foundMNoun)) sTagSet=K;
					if (findLowCostTag(subjectTagSets[K],nameCost,L"NAME",natl,0,nextTag) && nameCost<nounCost && nameCost<GNounCost) sTagSet=K;
				}
			}
		}
		if (!subjectTagSets.size())
		{
			if (debugTrace.traceAgreement)
				lplog(L"%d:No tags found under subject SUBJECT=(%d,%d).",tagSetSubjectPosition,whereSubject,nextSubject);
			return 0;
		}
		if (debugTrace.traceAgreement)
			lplog(L"%d:Final costs for subject tagset: nounCost=%d GNounCost=%d nameCost=%d tagSet#=%d",tagSetSubjectPosition,nounCost,GNounCost,nameCost,sTagSet);
		if (sTagSet==-1 || (nounCost==GNounCost && nameCost>nounCost && ntl.len!=gtl.len))
		{
			if (debugTrace.traceAgreement)
				lplog(L"%d:Tags are inconsistent for subject tagsets ntl.end=%d gtl.end=%d.",tagSetSubjectPosition,ntl.len,gtl.len);
			return 0;
		}
		if (nameCost<=nounCost)
		{
			if (debugTrace.traceAgreement)
				lplog(L"%d:Name cost %d is <= nounCost %d.  Setting noun position to -2.",tagSetSubjectPosition,nameCost,nounCost);
			nounPosition=-2;
			nameLastPosition=ntl.sourcePosition+ntl.len-1;
		}
		else if (nounCost<=GNounCost)
			nounPosition=ntl.sourcePosition;
		// if NAME tag is totally before N_AGREE tag, then NAME is used as an adjective.  N_AGREE should be used for agreement.
		// How many American Girl dolls have been sold?
		if (nameCost==nounCost && natl.sourcePosition+natl.len<=ntl.sourcePosition) 
			nounPosition=ntl.sourcePosition;
		int nextSingularTag=-1,nextPluralTag=-1,nextRE=-1,nextMNoun=-1;
		singularSet=findTag(subjectTagSets[sTagSet],L"SINGULAR",nextSingularTag)>=0;
		pluralSet=findTag(subjectTagSets[sTagSet],L"PLURAL",nextPluralTag)>=0;
		restateSet=findTag(subjectTagSets[sTagSet],L"RE_OBJECT",nextRE)>=0;
		// if singular is set (because mnoun has an 'OR', like 'a mouse or a horse'), but nounPosition>=0 and the noun is plural, then forget about the singular setting and mark as plural.
		if (singularSet && nounPosition>=0 && findTag(subjectTagSets[sTagSet],L"MNOUN",nextMNoun)>=0)
		{
			pluralSet=(m[nounPosition].word->second.inflectionFlags&PLURAL)==PLURAL;
			singularSet=(m[nounPosition].word->second.inflectionFlags&SINGULAR)==SINGULAR;
			if (debugTrace.traceAgreement && pluralSet)
				lplog(L"%d:Singular tag overridden in case of 'OR' mnoun where each object in compound noun is a plural noun.",tagSetSubjectPosition);
		}
	}
	else
		nounPosition=tagSet[whereSubject].sourcePosition;
	int whereMainVerb=findTag(tagSet,L"VERB",nextMainVerb);
	int whereVerbTag=(whereMainVerb>=0) ? findTagConstrained(tagSet,L"V_AGREE",nextVerb,tagSet[whereMainVerb]) : -1;
	int whereConditional=(whereMainVerb>=0) ? findTagConstrained(tagSet,L"conditional",nextConditionalTag,tagSet[whereMainVerb]) : -1;
	int wherePast=(whereMainVerb>=0) ? findTagConstrained(tagSet,L"past",nextConditionalTag,tagSet[whereMainVerb]) : -1;
	int whereFuture=(whereMainVerb>=0) ? findTagConstrained(tagSet,L"future",nextFutureTag,tagSet[whereMainVerb]) : -1;
	// St. Pancras? - Pancras is an unknown, so verb is allowed.  Yet Pancras is after a ., so capitalization is allowed.
	// disallow this case.
	if (whereSubject>=0 && whereVerbTag>=0 && tagSet[whereVerbTag].sourcePosition>tagSet[whereSubject].sourcePosition && 
			(m[tagSet[whereVerbTag].sourcePosition].flags&WordMatch::flagFirstLetterCapitalized) &&
			m[tagSet[whereSubject].sourcePosition+tagSet[whereSubject].len-1].word->first==L".")
	{
		if (debugTrace.traceAgreement)
			lplog(L"%d: verb capitalized [SOURCE=%06d] cost=%d",position,traceSource=gTraceSource++,20);
		return 20;
	}
	if ((whereConditional<0 && whereFuture<0 && whereVerbTag<0) || whereSubject<0 || whereMainVerb<0 || nextSubject>=0 || nextMainVerb>=0)
	{
		// check for question
		if (whereSubject>=0 && whereMainVerb<0 && (whereVerbTag=findTag(tagSet,L"V_AGREE",nextVerb))>=0 && nextVerb>=0 &&
			tagSet[whereVerbTag].sourcePosition<tagSet[whereSubject].sourcePosition &&
			tagSet[whereSubject].sourcePosition<tagSet[nextVerb].sourcePosition)
		{
			whereConditional=findTagConstrained(tagSet,L"conditional",nextConditionalTag,tagSet[whereVerbTag]);
			if (debugTrace.traceAgreement)
				lplog(L"%d:Question detected. V_AGREE=(%d,%d) SUBJECT=%d nounPosition=%d whereConditional=%d",position,whereVerbTag,nextVerb,whereSubject,nounPosition,whereConditional);
		}
		else
		{
			if (debugTrace.traceAgreement)
				lplog(L"%d:Search for noun and verb tag returned V_AGREE=(%d,%d) SUBJECT=(%d,%d) VERB=(%d,%d)",
				position,whereVerbTag,nextVerb,whereSubject,nextSubject,whereMainVerb,nextMainVerb);
			return 0;
		}
	}
	int relationCost=0;
	tFI::cRMap *rm;
	int verbPosition=(whereVerbTag>=0) ? tagSet[whereVerbTag].sourcePosition : -1;
	if ((nounPosition>=0 || nounPosition==-2) && verbPosition>=0)
	{
		tIWMM nounWord,verbWord;
		int nextObjectVerbTag=-1,whereObjectVerbTag=(whereMainVerb>=0) ? findTagConstrained(tagSet,L"V_OBJECT",nextObjectVerbTag,tagSet[whereMainVerb]) : -1;
		if (nextObjectVerbTag>=0)
			verbWord=m[tagSet[nextObjectVerbTag].sourcePosition].word;
		else if (whereObjectVerbTag>=0)
			verbWord=m[tagSet[whereObjectVerbTag].sourcePosition].word;
		else if (nextVerb>=0)
			verbWord=m[tagSet[nextVerb].sourcePosition].word; // get the verb last V_AGREE if there is no V_OBJECT
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
			relationCost=-COST_PER_RELATION;
		else // Whom did Obama defeat for the Senate seat? - Obama is a single word, so it never gets a name (nounPosition=-2) designation, and so never tries __ppn__->verb relation
			if (nounWord!=Words.PPN && tagSet[whereSubject].len==1 && (m[nounPosition].flags&WordMatch::flagFirstLetterCapitalized) &&
			    (rm=Words.PPN->second.relationMaps[SubjectWordWithVerb]) && (tr=rm->r.find(verbWord))!=rm->r.end())
			relationCost=-COST_PER_RELATION;
		if (debugTrace.traceAgreement)
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
	if (restateSet && relationCost)
	{
		if (debugTrace.traceAgreement)
			lplog(L"Subject restated.  cost %d = %d/subject length=%d.",relationCost/tagSet[whereSubject].len,relationCost,tagSet[whereSubject].len);
		relationCost/=tagSet[whereSubject].len;
	}
	if (whereConditional>=0 || whereFuture>=0) // he will have, they will have etc.
	{
		if (debugTrace.traceAgreement)
			lplog(L"%d: conditional agree [SOURCE=%06d] cost=%d",position,traceSource=gTraceSource++,relationCost);
		return relationCost;
	}
	//int nextPersonTag=-1;
	//  bool thirdPersonSet=(whereSubject>=0) ? findTagConstrained(tagSet,"THIRD_PERSON",nextPersonTag,tagSet[whereSubject])>=0 : false;
	int person=THIRD_PERSON;
	// if third_person not already set,
	if (nounPosition>=0)
	{
		person=m[nounPosition].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON|THIRD_PERSON);
		// many is the time she has said to me...
		if (m[nounPosition].word->first==L"many")
			singularSet=pluralSet=true;
	}
	if (!person)
		person=THIRD_PERSON;
	// if singular | plural not already set
	// words like "there" may be plural or singular depending on usage
	if (!singularSet && !pluralSet && nounPosition>=0)
	{
		pluralSet=(m[nounPosition].word->second.inflectionFlags&PLURAL)==PLURAL;
		singularSet=(m[nounPosition].word->second.inflectionFlags&SINGULAR)==SINGULAR;
		// a proper noun is very rarely plural - Tuppence should only be considered singular
		// special rules for multiple proper nouns needed in future (two ICBMs)
		if (pluralSet && singularSet && (m[nounPosition].flags&WordMatch::flagOnlyConsiderProperNounForms))
			pluralSet=false;
		if (m[nounPosition].word->first==L"who" || // those who were OR this person who was
			  m[nounPosition].word->first==L"that") // the settlement that was OR the settlements that were
			singularSet=pluralSet=true;
	}
	// The Prince of Asturias Awards (Spanish) are a series awarded in Spain.
	int inflectionFlags=m[verbPosition].word->second.inflectionFlags&VERB_INFLECTIONS_MASK;
	int np=nounPosition;
	if (np==-2 && nameLastPosition>=0)
		np=nameLastPosition;
	if (np>=0 && np+4<(signed)m.size() && !pluralSet && 
		  (inflectionFlags==VERB_PRESENT_PLURAL || inflectionFlags==VERB_PAST_PLURAL) && 
		  (m[np].flags&WordMatch::flagFirstLetterCapitalized) && m[np+1].word->first==L"of" && 
		  (m[np+2].flags&WordMatch::flagFirstLetterCapitalized) && (m[np+3].flags&WordMatch::flagFirstLetterCapitalized) &&
			(m[np+3].word->second.inflectionFlags&PLURAL)==PLURAL)
	{
		singularSet=false;
		pluralSet=true;
		if (debugTrace.traceAgreement)
			lplog(L"%d:Singular name with direct capitalized prepositional phrase with plural last object [name is adjective] leads to switched agreement.",position);
	}
	// The Men of Yore Briefcase is essential for every trip.
	if (np>=0 && np+4<(signed)m.size() && pluralSet && 
		  (inflectionFlags==VERB_PRESENT_FIRST_SINGULAR || inflectionFlags==VERB_PRESENT_THIRD_SINGULAR) && 
		  (m[np].flags&WordMatch::flagFirstLetterCapitalized) && m[np+1].word->first==L"of" && 
		  (m[np+2].flags&WordMatch::flagFirstLetterCapitalized) && (m[np+3].flags&WordMatch::flagFirstLetterCapitalized) &&
			(m[np+3].word->second.inflectionFlags&SINGULAR)==SINGULAR)
	{
		singularSet=true;
		pluralSet=false;
		if (debugTrace.traceAgreement)
			lplog(L"%d:Plural name with direct capitalized prepositional phrase with singular last object [name is adjective] leads to switched agreement.",position);
	}
	if (!singularSet && !pluralSet)
		singularSet=true;
	wstring temp;
	if (debugTrace.traceAgreement)
		lplog(L"%d:Noun phrase at %d was %s [plural=%s singular=%s]",position,nounPosition,getInflectionName(person,nounInflectionMap,temp),(pluralSet) ? L"true" : L"false",(singularSet) ? L"true" : L"false");
	bool ambiguousTense;
	// the verb 'beat' is both past and present tense
	if (ambiguousTense=(inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST))==(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST))
	{
		if (wherePast>=0) inflectionFlags&=VERB_PAST_PARTICIPLE|VERB_PAST|VERB_PAST_PLURAL;
		else inflectionFlags&=VERB_PRESENT_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL;
	}
	if (debugTrace.traceAgreement)
		lplog(L"%d:Verb phrase at (%d:%d-%d) had mask of %s.",position,verbPosition,
					tagSet[whereMainVerb].sourcePosition,tagSet[whereMainVerb].len+tagSet[whereMainVerb].sourcePosition,
					getInflectionName(inflectionFlags,verbInflectionMap,temp));
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
	}
	int cost=((agree) ? 0 : NON_AGREEMENT_COST)+relationCost;
	if (!agree && ambiguousTense && whereVerbTag>=0 && whereConditional<0 && 
			// avoid stomping on part of another pattern 'my millionaire would probably run for his life!'
			(m[verbPosition].word->second.query(modalAuxiliaryForm)<0 && m[verbPosition].word->second.query(futureModalAuxiliaryForm)<0 && 
			 m[verbPosition].word->second.query(negationModalAuxiliaryForm)<0 && m[verbPosition].word->second.query(negationFutureModalAuxiliaryForm)<0))
	{
		//pema[abs(tagSet[whereVerbTag].PEMAOffset)].addOCostTillMax(1);
		pema[abs(tagSet[whereMainVerb].PEMAOffset)].addOCostTillMax(1);
		if (debugTrace.traceAgreement)
			lplog(L"%d: Disagreement at ambiguous tense PEMA %d[%d] [SOURCE=%06d] increased cost=1",position,abs(tagSet[whereVerbTag].PEMAOffset),abs(tagSet[whereMainVerb].PEMAOffset),gTraceSource);
	}
	if (debugTrace.traceAgreement)
		lplog(L"%d: %s [SOURCE=%06d] cost=%d",position,(agree) ? L"agree" : L"DISAGREE",traceSource=gTraceSource++,cost);
	return cost;
}

bool Source::evaluateAgreement(int verbPosition,int whereSubject)
{ LFS
	int person=THIRD_PERSON,inflectionFlags=m[verbPosition].word->second.inflectionFlags&VERB_INFLECTIONS_MASK;
	if (whereSubject>=0)
		person=m[whereSubject].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON|THIRD_PERSON);
	if (!person)
		person=THIRD_PERSON;
	bool agree=true;
	switch(inflectionFlags)
	{
	case VERB_PRESENT_PARTICIPLE:
	case VERB_PAST_PARTICIPLE:
	case VERB_PAST:break;
	case VERB_PRESENT_FIRST_SINGULAR: // only FIRST_SINGULAR !
		// you want, I want, they want, we want NOT he want!
		if ((person&THIRD_PERSON)==THIRD_PERSON) agree=false;
		if (((person&SECOND_PERSON) || (person&THIRD_PERSON)) && m[verbPosition].word->first==L"am")
			agree=false;
		break;
	case VERB_PRESENT_THIRD_SINGULAR:
		// he wants ONLY
		if ((person&THIRD_PERSON)!=THIRD_PERSON) agree=false;
		break;
	case VERB_PRESENT_PLURAL:
	case VERB_PAST_PLURAL:
		if ((person&SECOND_PERSON)!=SECOND_PERSON && (person!=(FIRST_PERSON|SECOND_PERSON))) agree=false;
		break;
	case VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL:
		if ((person&FIRST_PERSON)!=FIRST_PERSON && (person&SECOND_PERSON)!=SECOND_PERSON && (person!=(FIRST_PERSON|SECOND_PERSON))) agree=false;
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
		im->pma.findMaxLen(L"_VERBPAST",elementVP)==true && nounAvgCost>=im->pma[element&~patternFlag].getAverageCost())
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
		nounAvgCost<im->pma[element&~patternFlag].getAverageCost()) return false;
	if (debugTrace.tracePatternElimination)
		lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) is not a winner (S1 preference) nounAvgCost=%d >= S1Cost %d.",position,J,
		p->name.c_str(),p->differentiator.c_str(),position,im->pma[J].len+position,nounAvgCost,im->pma[element&~patternFlag].getAverageCost());
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
	if (startCollectTags(debugTrace.traceBNCPreferences,BNCPreferencesTagSet,position,PEMAPosition,tagSets,false,false)>0)
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
bool Source::tagSetAllIn(vector <costTagSet> &PEMAPositions,int I)
{ LFS
	while (I>=0)
	{
		if (!pema[PEMAPositions[I].PEMAPosition].flagSet(patternElementMatchArray::IN_CHAIN))
			return false;
		int tagSetElement=PEMAPositions[I].element-1;
		if (!tagSetElement) return true;
		for (I--; I>=0 && PEMAPositions[I].element!=tagSetElement; I--);
	}
	return true;
}

// ONLY AND ALL pema in chainPEMAPositions have IN_CHAIN flag set.
#define MIN_CHAIN_COST -MAX_COST
void Source::setChain(vector <patternElementMatchArray::tPatternElementMatch *> chainPEMAPositions,vector <costTagSet> &PEMAPositions,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int &traceSource,int &minOverallChainCost)
{ LFS
	unsigned int I;
	// get the last element that has IN_CHAIN set for the first tagset that has all of its elements having IN_CHAIN set
	for (I=0; I<PEMAPositions.size(); )
	{
		for (int ts=PEMAPositions[I].tagSet; I+1<PEMAPositions.size() && ts==PEMAPositions[I+1].tagSet; I++);
		if (tagSetAllIn(PEMAPositions,I))
			break;
		else // skip all elements that depend on this element
			for (int e=PEMAPositions[I++].element; I<PEMAPositions.size() && e<PEMAPositions[I].element; I++);
	}
	int maxDeltaCost=MIN_CHAIN_COST,lastWinningPEMAPosition=-1;
	if (I<PEMAPositions.size())
	{
		lastWinningPEMAPosition=I;
		// found first element match.  How many more elements match?
		// elements which are greater must always ADD cost, or keep it the same.  The cost never decreases. see lowerPreviousElementCosts
		for (maxDeltaCost=PEMAPositions[I++].cost; I<PEMAPositions.size() && PEMAPositions[I].element>PEMAPositions[I-1].element && tagSetAllIn(PEMAPositions,I); I++)
			if (PEMAPositions[I].cost>maxDeltaCost)
				maxDeltaCost=PEMAPositions[lastWinningPEMAPosition=I].cost;
		// back up if PEMA element is the same
		// POS   : PEMA   TS E# COST MINC TRSC REJECT (0 set)
		// 000008: 000180 00 01 000  000  001  true
		// 000009: 000181 00 02 000  000  001  true
		// 000009: 000181 01 03 002  002  002  false
		/* decreased pattern matching accuracy - does not take into account when pattern actually ends
		if (I==PEMAPositions.size() && I) I--;
		for (int pp=PEMAPositions[I].PEMAPosition; I<PEMAPositions.size() && PEMAPositions[I].PEMAPosition==pp; I--)
		if (PEMAPositions[I].cost<maxDeltaCost)
		maxDeltaCost=PEMAPositions[lastWinningPEMAPosition=I].cost;
		*/
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
		for (; I<PEMAPositions.size() && PEMAPositions[I].element==PEMAPositions[I-1].element && PEMAPositions[I].PEMAPosition==PEMAPositions[I-1].PEMAPosition; I++)
			if (PEMAPositions[I].cost<maxDeltaCost)
				maxDeltaCost=PEMAPositions[lastWinningPEMAPosition=I].cost;
	}
	int maxOCost=MIN_CHAIN_COST;
	for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI=chainPEMAPositions.begin(),cPIEnd=chainPEMAPositions.end(); cPI!=cPIEnd; cPI++)
		maxOCost=max(maxOCost,(*cPI)->getOCost()+(*cPI)->cumulativeDeltaCost);
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
				len+=wsprintf(temp+len,L"%d (oCost=%d deltaCost=%d)",*cPI-pema.begin(),(*cPI)->getOCost(),(*cPI)->cumulativeDeltaCost);
			lplog(L"TS#%03d set chain %s with %d=maxOCost %d + maxDeltaCost %d [SOURCE=%06d].",
				PEMAPositions[lastWinningPEMAPosition].tagSet,temp,maxOCost+maxDeltaCost,maxOCost,maxDeltaCost,PEMAPositions[lastWinningPEMAPosition].traceSource);
		}
		maxOCost+=maxDeltaCost;
		// set the cost of the entire chain to maxOCost
		for (vector <patternElementMatchArray::tPatternElementMatch *>::iterator cPI=chainPEMAPositions.begin(),cPIEnd=chainPEMAPositions.end(); cPI!=cPIEnd; cPI++)
			if ((*cPI)->flagSet(patternElementMatchArray::COST_DONE))
			{
				if ((*cPI)->tempCost>maxOCost && (*cPI)->begin==0 && minOverallChainCost>maxOCost)
				{
					traceSource=PEMAPositions[lastWinningPEMAPosition].traceSource;
					minOverallChainCost=maxOCost;
				}
				(*cPI)->tempCost=min((*cPI)->tempCost,maxOCost);
			}
			else
			{
				(*cPI)->tempCost=maxOCost;
				(*cPI)->setFlag(patternElementMatchArray::COST_DONE);
				if ((*cPI)->begin==0 && minOverallChainCost>maxOCost)
				{
					traceSource=PEMAPositions[lastWinningPEMAPosition].traceSource;
					minOverallChainCost=maxOCost;
				}
				PEMAPositionsSet.push_back(*cPI);
			}
}

void Source::findAllChains(vector <costTagSet> &PEMAPositions,int PEMAPosition,int position,vector <patternElementMatchArray::tPatternElementMatch *> &chain,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet,int totalCost,int &traceSource,int &minOverallChainCost)
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
			if (nextPatternElementPEMAPosition>=0)
				findAllChains(PEMAPositions,nextPatternElementPEMAPosition,position+relativeBegin-pema[nextPatternElementPEMAPosition].begin,
				chain,PEMAPositionsSet,totalCost+pem->iCost,traceSource,minOverallChainCost);
			else
				setChain(chain,PEMAPositions,PEMAPositionsSet,traceSource,minOverallChainCost);
			pem->removeFlag(patternElementMatchArray::IN_CHAIN);
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
int Source::cascadeUpToAllParents(bool recalculatePMCost,int basePosition,patternMatchArray::tPatternMatch *childPM,int traceSource,vector <patternElementMatchArray::tPatternElementMatch *> &PEMAPositionsSet)
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
					cascadeUpToAllParents(recalculatePMCost,parentBasePosition,parentPM,traceSource,PEMAPositionsSet);
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
							lplog(L"assessCost PEMA %06dA Added %d cost for a total of %d",*ppsi-pema.begin(),(*ppsi)->cumulativeDeltaCost,(*ppsi)->getOCost()+(*ppsi)->cumulativeDeltaCost);
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
void Source::lowerPreviousElementCosts(vector <int> &costs,vector <int> &traceSources,
																			 vector <costTagSet>::iterator &IP,vector <costTagSet>::iterator IPEnd)
{ LFS
	vector <costTagSet>::iterator lastElements[MAX_ELEMENT];
	for (unsigned int I=0; I<MAX_ELEMENT; I++) lastElements[I]=IPEnd;
	int tagSet;
	for (tagSet=IP->tagSet; IP!=IPEnd && IP->tagSet==tagSet; IP++)
	{
		lastElements[IP->element]=IP;
		IP->cost=costs[IP->tagSet];
		IP->traceSource=traceSources[IP->tagSet];
	}
	// Important to change only the last element!
	// divide array into sets of increasing elements.
	// if in any set the cost of a member A is greater than another member B and element of A is < than element of B (it came first)
	//  then decrease the cost of A to the cost of B.  This is because with any set of PEMA elements SPEMA that form a complete matched pattern
	//  if element of A < element of B and member B belongs to set SPEMA then member A also belongs to SPEMA. Member A also belongs to another
	//  set SPEMA2, with a higher cost, but we prefer the lower cost one because we are seeking patterns with the lowest cost.
	while (IP!=IPEnd && IP->element)
	{
		tagSet=IP->tagSet;
		for (int lowestElement=IP->element-1; lowestElement>=0; lowestElement--)
			if (lastElements[lowestElement]!=IPEnd)
				if (lastElements[lowestElement]->cost>costs[tagSet])
				{
					lastElements[lowestElement]->cost=costs[tagSet];
					lastElements[lowestElement]->traceSource=traceSources[tagSet];
				}
				for (; IP!=IPEnd && IP->tagSet==tagSet; IP++)
				{
					lastElements[IP->element]=IP;
					IP->cost=costs[IP->tagSet];
					IP->traceSource=traceSources[IP->tagSet];
				}
	}
}

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
int Source::setSecondaryCosts(vector <int> &costs,vector <costTagSet> &PEMAPositions,vector <int> &traceSources,patternMatchArray::tPatternMatch *pm,int basePosition)
{ LFS
	// assign cost to each P matching TS#.  Keep lowest E (LE) for each TS#.
	// for each E<LE, the last P having E<LE, assign cost of P to be the minimum of the TS cost and the cost already at P.
	vector <costTagSet>::iterator IPBegin=PEMAPositions.begin(),IPEnd=PEMAPositions.end(),IP;
	set <int> origins;
	for (IP=IPBegin; IP!=IPEnd; IP++)
	{
		if (IP->element>=MAX_ELEMENT)
		{
			lplog(L"# Elements of set exceeds %d!",MAX_ELEMENT);
			return 0;
		}
		origins.insert(pema[IP->PEMAPosition].origin);
	}
	int costSet=0;
	for (IP=IPBegin; IP!=IPEnd; IPBegin=IP)
	{
		lowerPreviousElementCosts(costs,traceSources,IP,IPEnd);
		if (debugTrace.traceSecondaryPEMACosting)
		{
			lplog(L"POS   : PEMA   TS E# COST MINC TRSC REJECT (%d set)",costSet++);
			for (vector <costTagSet>::iterator tIP=IPBegin; tIP!=IP; tIP++)
				lplog(L"%06d: %06d %02d %02d %03d  %03d  %03d  %s",
				tIP->position,tIP->PEMAPosition,tIP->tagSet,tIP->element,costs[tIP->tagSet],tIP->cost,tIP->traceSource,
				(pema[tIP->PEMAPosition].tempCost==tIP->cost) ? L"true ":L"false");
		}
	}
	vector <patternElementMatchArray::tPatternElementMatch *> chain,PEMAPositionsSet;
	chain.reserve(16);
	PEMAPositionsSet.reserve(1024);
	int traceSource=-1,totalCost=0,minOverallChainCost=MAX_COST;
	for (set <int>::iterator o=origins.begin(),oEnd=origins.end(); o!=oEnd; o++)
		findAllChains(PEMAPositions,*o,basePosition,chain,PEMAPositionsSet,totalCost,traceSource,minOverallChainCost);
	bool recalculatePMCost=false;
	recalculateOCosts(recalculatePMCost,PEMAPositionsSet,0,traceSource);
	cascadeUpToAllParents(recalculatePMCost,basePosition,pm,traceSource,PEMAPositionsSet);
	for (vector<patternElementMatchArray::tPatternElementMatch *>::iterator ppsi=PEMAPositionsSet.begin(),ppsiEnd=PEMAPositionsSet.end(); ppsi!=ppsiEnd; ppsi++)
	{
		(*ppsi)->removeFlag(patternElementMatchArray::COST_DONE);
		if ((*ppsi)->cumulativeDeltaCost)
		{
			if (debugTrace.traceSecondaryPEMACosting)
				lplog(L"assessCost PEMA %06dA Added %d cost for a total of %d",*ppsi-pema.begin(),(*ppsi)->cumulativeDeltaCost,(*ppsi)->getOCost()+(*ppsi)->cumulativeDeltaCost);
			(*ppsi)->addOCostTillMax((*ppsi)->cumulativeDeltaCost);
			(*ppsi)->cumulativeDeltaCost=0;
		}
	}
	return 0;
}

void normalize(unsigned char *usages,int start,int len)
{ LFS
	len+=start;
	int I;
	for (I=start; I<len && usages[I]==255; I++);
	if (I==len)
		for (int J=start; J<len; J++)
			usages[J]>>=1;
}

int Source::calculateVerbAfterVerbUsage(int whereVerb,unsigned int nextWord)
{ LFS
	if (nextWord<m.size() && m[nextWord].forms.isSet(verbForm))
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
		//  add "verbal_auxiliary{V_AGREE}",0,1,1, // he dares eat this?
		if ((im->forms.isSet(futureModalAuxiliaryForm) || im->forms.isSet(negationFutureModalAuxiliaryForm) ||
			im->forms.isSet(modalAuxiliaryForm) || im->forms.isSet(negationModalAuxiliaryForm)) &&
			(m[nextWord].word->second.inflectionFlags&(VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL|VERB_PRESENT_SECOND_SINGULAR))!=0)
			return cost;
		if ((im->forms.isSet(isForm) || im->forms.isSet(isNegationForm) || im->word->first==L"be" || im->word->first==L"been") &&
			(m[nextWord].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE)!=0)
			return cost;
		// 'having' is sufficiently rare that I will not include it to be conservative. 'having examined'
		if ((((im->forms.isSet(haveForm) || im->forms.isSet(haveNegationForm))) || // && !(im->word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE)) ||
			im->forms.isSet(isForm) || im->forms.isSet(isNegationForm) ||
			im->word->first==L"be" || im->word->first==L"being") && // "been" included in isForm
			(m[nextWord].word->second.inflectionFlags&VERB_PAST_PARTICIPLE)!=0)
			return cost;
		if ((im->forms.isSet(doesForm) || im->forms.isSet(doesNegationForm)) && im->word->first!=L"doing" &&
				(m[nextWord].word->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR)!=0)
			return cost;
	}
	return 0;
}

//  desiredTagSets.push_back(tTS(NOUN_DETERMINER_TAGSET,1,"N_AGREE",NULL));
int Source::evaluateNounDeterminer(vector <tTagLocation> &tagSet, bool assessCost, int &traceSource, int begin, int end)
{
	LFS
		int nounTag = -1, nextNounTag = -1, nAgreeTag = -1, nextNAgreeTag = -1, nextDet = -1, PNC = 0;// , nextName = -1, subObjectTag = -1;
	if ((nounTag=findTag(tagSet,L"NOUN",nextNounTag))>=0)
		nAgreeTag=findTagConstrained(tagSet,L"N_AGREE",nextNAgreeTag,tagSet[nounTag]);
	if (nAgreeTag<end-1 && calculateVerbAfterVerbUsage(end-1,end)) // if nAgreeTag<end-1, it is more likely a compound noun
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:Noun (%d,%d) is compound, has a verb at end and a verb after the end [SOURCE=%06d].",begin,begin,end,traceSource=gTraceSource);
		PNC+=tFI::COST_OF_INCORRECT_VERBAL_NOUN;
	}
	if (begin>0 && m[begin-1].word->first==L"to" && m[begin].queryForm(verbForm)>=0 && 
			!(m[begin].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER)) && m[begin].word->second.usageCosts[m[begin].queryForm(verbForm)]<4)
	{
		wstring formsString;
		if (debugTrace.traceDeterminer)
			lplog(L"%d:%s: Noun (%d,%d) has verb form after 'to' - verb cost=%d",begin,m[begin].word->first.c_str(),begin,end,
				m[begin].word->second.usageCosts[m[begin].queryForm(verbForm)]);
		PNC+=10;
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
	if (nAgreeTag<0 || nounTag<0) 
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:Noun (%d,%d) has no nAgree or no noun tag (cost=%d) [SOURCE=%06d].",begin,begin,end,PNC,traceSource=gTraceSource++);
		return PNC;
	}
	/*
	if ((subObjectTag=findOneTag(tagSet,L"SUBOBJECT",-1))>=0)
	{
		vector < vector <tTagLocation> > subobjectTagSets;
		if (startCollectTagsFromTag(false,nounDeterminerTagSet,tagSet[subObjectTag],subobjectTagSets,-1,false)>=0)
			for (unsigned int J=0; J<subobjectTagSets.size(); J++)
			{
				if (t.traceSpeakerResolution)
					::printTagSet(LOG_RESOLUTION,L"SUBOBJECT",J,subobjectTagSets[J]);
				if (subobjectTagSets[J].empty()) continue; // shouldn't happen - but does
				PNC+=evaluateNounDeterminer(subobjectTagSets[J],true,traceSource,tagSet[subObjectTag].sourcePosition,tagSet[subObjectTag].sourcePosition+tagSet[subObjectTag].len,tagSet[subObjectTag].PEMAOffset);
			}
	}
	*/
	tIWMM nounWord=m[tagSet[nAgreeTag].sourcePosition].word;
	if (!(nounWord->second.inflectionFlags&SINGULAR))
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:noun %s is not singular [SOURCE=%06d].",tagSet[nAgreeTag].sourcePosition,nounWord->first.c_str(),traceSource=gTraceSource++);
		return PNC;
	}
	if (!tagIsCertain(tagSet[nAgreeTag].sourcePosition))
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:tag for noun %s is not certain [SOURCE=%06d].",tagSet[nAgreeTag].sourcePosition,nounWord->first.c_str(),traceSource=gTraceSource++);
		return PNC;
	}
	int whereDet=(findTagConstrained(tagSet,L"DET",nextDet,tagSet[nounTag])>=0) ? 0 : 1;
	int b=tagSet[nounTag].sourcePosition;
	if (whereDet==1 && (m[b].queryForm(PROPER_NOUN_FORM)>=0 || m[b].queryForm(honorificForm)>=0 || m[b].queryForm(honorificAbbreviationForm)>=0))
	{
		int whereNAgree=tagSet[nAgreeTag].sourcePosition;
		//lplog(L"%d: %d %s %s",begin,b,(m[b].queryForm(PROPER_NOUN_FORM)>=0) ? L"true":L"false",(m[b].flags&WordMatch::flagNounOwner) ? L"true":L"false");
		while ((m[b].queryForm(PROPER_NOUN_FORM)>=0 || m[b].queryForm(honorificForm)>=0 || m[b].queryForm(honorificAbbreviationForm)>=0 || m[b].word->first==L".") && 
					 !(m[b].flags&WordMatch::flagNounOwner) && b<whereNAgree) b++;
		if (m[b].queryForm(PROPER_NOUN_FORM)>=0 && (m[b].flags&WordMatch::flagNounOwner) && b<whereNAgree)
			whereDet=0;
	}
	// fix problem otherwise caused by separating out these relativizers which also act as determiners from the noun pattern itself
	// without this, REL1 and RELQ have costs associated with singular subjects that have separated relativizers
	// What percentage of new car sales was for hybrid cars?
	if (whereDet==1 && begin>0 && (m[begin-1].word->first==L"which" || m[begin-1].word->first==L"what" || m[begin-1].word->first==L"whose"))
	{
		if (debugTrace.traceDeterminer)
			lplog(L"%d:noun %s lacks a determiner but is led by a determiner relative [SOURCE=%06d].",tagSet[nAgreeTag].sourcePosition,nounWord->first.c_str(),traceSource=gTraceSource++);
		whereDet=0;
	}
	/*
		DEBUG 2 - 4330 man's manner
	// attempt to check whether this is a compound noun where a previous noun actually has the determiner
	if (whereDet==1 && nounWord->second.usageCosts[tFI::SINGULAR_NOUN_HAS_DETERMINER+whereDet]!=0 && 
			(wf=nounWord->second.query(nounForm))>=0 && nounWord->second.usageCosts[wf]==0 &&
			begin>3 && m[begin-1].queryForm(coordinatorForm)>=0 && m[begin-2].queryForm(nounForm)>=0 && 
			(m[begin-3].queryForm(determinerForm)>=0 || 
			 (m[begin-3].queryForm(nounForm)>=0 && ((m[begin-3].word->second.inflectionFlags&(PLURAL_OWNER|SINGULAR_OWNER)) || (m[begin-3].flags&WordMatch::flagNounOwner)) && m[begin-4].queryForm(determinerForm)>=0) ||
			 (m[begin-3].queryForm(adjectiveForm)>=0 && m[begin-4].queryForm(determinerForm)>=0)))
	{
		//if (t.traceDeterminer)
		lplog(L"%d:determiner for noun %s found in previous coordinated phrase @%d [SOURCE=%06d].",
					tagSet[nAgreeTag].sourcePosition,nounWord->first.c_str(),begin-2,traceSource=gTraceSource);
		whereDet=0;
	}
	*/
	if (nounWord->second.mainEntry!=wNULL)
		nounWord=nounWord->second.mainEntry;
	if (debugTrace.traceDeterminer)
		lplog(L"%d:singular noun %s has %s determiner (cost=%d) [SOURCE=%06d].",tagSet[nAgreeTag].sourcePosition,nounWord->first.c_str(),(whereDet==0) ? L"a":L"no",nounWord->second.usageCosts[tFI::SINGULAR_NOUN_HAS_DETERMINER+whereDet],traceSource=gTraceSource++);
	if (assessCost)
		return PNC+nounWord->second.usageCosts[tFI::SINGULAR_NOUN_HAS_DETERMINER+whereDet];
	normalize(nounWord->second.usagePatterns,tFI::SINGULAR_NOUN_HAS_DETERMINER,2);
	normalize(nounWord->second.deltaUsagePatterns,tFI::SINGULAR_NOUN_HAS_DETERMINER,2);
	nounWord->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_DETERMINER+whereDet]++;
	nounWord->second.deltaUsagePatterns[tFI::SINGULAR_NOUN_HAS_DETERMINER+whereDet]++;
	int transferTotal=0;
	for (unsigned int I=tFI::SINGULAR_NOUN_HAS_DETERMINER; I<tFI::SINGULAR_NOUN_HAS_DETERMINER+2; I++)
		transferTotal+=nounWord->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_DETERMINER+I];
	if ((transferTotal&15)==15)
		nounWord->second.transferUsagePatternsToCosts(tFI::HIGHEST_COST_OF_INCORRECT_NOUN_DET_USAGE,tFI::SINGULAR_NOUN_HAS_DETERMINER,2);
	return 0;
}

void Source::evaluateNounDeterminers(int PEMAPosition,int position,vector < vector <tTagLocation> > &tagSets)
{ LFS // DLFS
	vector <int> costs;
	if (pema[PEMAPosition].flagSet(patternElementMatchArray::COST_ND))
	{
		if (debugTrace.traceDeterminer)
		{
			int pattern=pema[PEMAPosition].getPattern();
			lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (already evaluated)",position,PEMAPosition,desiredTagSets[roleTagSet].name.c_str(),
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
		}
		return;
	}
	pema[PEMAPosition].setFlag(patternElementMatchArray::COST_ND);
	// search for all subjects, objects, subobjects and prepobjects
	if (startCollectTags(debugTrace.traceDeterminer,roleTagSet,position,PEMAPosition,tagSets,true,true)>0)
	{
		vector < vector <tTagLocation> > nTagSets;
		vector <int> nCosts,traceSources;
		if (tagSets.size()==1 && tagSets[0].empty())
		{
			// When a sentence is merely a command, and potentially matches a __NOUN[9], there is no OBJECT that is collected, because that is only provided by C1__S1.
			//    In this case, without this push_back, the __NOUN[9] would skip costing completely, and would have an advantage over the VERBREL1.
			//    If the verb can also be a noun, and the noun is less costly than the verb, the __NOUN[9] would win, because there is no extra cost associated with
			//    not having the determiner.
			// MOVEClimb aboard the lugger . 
			tagSets[0].push_back(tTagLocation(0,pema[PEMAPosition].getChildPattern(),pema[PEMAPosition].getPattern(),pema[PEMAPosition].getElement(),position,pema[PEMAPosition].end,-PEMAPosition,true));
			pema[PEMAPosition].removeFlag(patternElementMatchArray::COST_ND);
		}
		// for each pattern of subjects, objects and subobjects
		for (unsigned int I=0; I<tagSets.size(); I++)
		{
			if (I && tagSetSame(tagSets[I],tagSets[I-1]))
			{
				if (debugTrace.traceDeterminer)
					lplog(L"OB TAGSET #%d (REPEAT OF PREVIOUS)",I);
				continue;
			}
			else
			{
				if (debugTrace.traceDeterminer)
					printTagSet(LOG_INFO,L"OB",I,tagSets[I],position,PEMAPosition);
				// for each subject, object or subobject
				for (unsigned int J=0; J<tagSets[I].size(); J++)
				{
					tTagLocation *tl=&tagSets[I][J];
					if (patterns[tl->parentPattern]->stopDescendingTagSearch(tl->parentElement,pema[(tl->PEMAOffset<0) ? -tl->PEMAOffset:tl->PEMAOffset].__patternElementIndex,pema[(tl->PEMAOffset<0) ? -tl->PEMAOffset:tl->PEMAOffset].isChildPattern()))
						continue;
					int nPattern=tl->pattern,nPosition=tl->sourcePosition,nLen=tl->len,traceSource=-1;
					if (tl->PEMAOffset<0)
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
									lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (already evaluated)",nPosition,nPEMAPosition,desiredTagSets[nounDeterminerTagSet].name.c_str(),
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
											secondaryPEMAPositions.push_back(costTagSet(nPosition,ePEMAPosition,-1,0,pema[ePEMAPosition].getElement()));
									}
									nCosts.push_back(10);
									traceSources.push_back(traceSource);
									setSecondaryCosts(nCosts,secondaryPEMAPositions,traceSources,pma,nPosition);
								}
								continue;
							}
							startCollectTags(debugTrace.traceDeterminer,nounDeterminerTagSet,nPosition,nPEMAPosition,nTagSets,true,true);
							for (unsigned int K=0; K<nTagSets.size(); K++)
							{
								if (debugTrace.traceDeterminer)
									printTagSet(LOG_INFO,L"ND",K,nTagSets[K],nPosition,nPEMAPosition);
								nCosts.push_back(evaluateNounDeterminer(nTagSets[K],true,traceSource,nPosition,nPosition+nLen));
								traceSources.push_back(traceSource);
							}
							setSecondaryCosts(nCosts,secondaryPEMAPositions,traceSources,pma,nPosition);
						} // for each iteration of each object
					}
					else if (!patterns[nPattern]->hasTag(GNOUN_TAG) && !patterns[nPattern]->hasTag(MNOUN_TAG) && tl->isPattern)
					{
						if (tl->PEMAOffset!=PEMAPosition && pema[tl->PEMAOffset].flagSet(patternElementMatchArray::COST_ND))
							continue;
						pema[tl->PEMAOffset].setFlag(patternElementMatchArray::COST_ND);
						patternMatchArray::tPatternMatch *pma=(m[nPosition].patterns.isSet(nPattern)) ? m[nPosition].pma.find(nPattern,nLen) : NULL;
						if (!pma)
						{
							lplog(L"%d:Noun Determiner %s[%s](%d,%d) not found!",nPosition,patterns[nPattern]->name.c_str(),patterns[nPattern]->differentiator.c_str(),nPosition,nPosition+nLen);
							continue;
						}
						nTagSets.clear();
						nCosts.clear();
						traceSources.clear();
						startCollectTags(debugTrace.traceDeterminer,nounDeterminerTagSet,nPosition,tl->PEMAOffset,nTagSets,true,true);
						for (unsigned int K=0; K<nTagSets.size(); K++)
						{
							if (debugTrace.traceDeterminer)
								printTagSet(LOG_INFO,L"ND",K,nTagSets[K],nPosition,tl->PEMAOffset);
							int tTraceSource=-1;
							nCosts.push_back(evaluateNounDeterminer(nTagSets[K],true, tTraceSource,nPosition,nPosition+nLen));
							traceSources.push_back(tTraceSource);
						}
						setSecondaryCosts(nCosts,secondaryPEMAPositions,traceSources,pma,nPosition);
					}
				}
			} // for each object
		} // for each pattern of subjects, objects and subobjects
	}
}

bool Source::assessEVALCost(tTagLocation &tl,int pattern,patternMatchArray::tPatternMatch *pm,int position)
{ LFS // DLFS
	int EVALPosition=tl.sourcePosition;
	patternMatchArray::tPatternMatch *EVALpm=m[EVALPosition].pma.find(pattern,tl.len);
	if (EVALpm && !EVALpm->isEval())
	{
		vector < vector <tTagLocation> > tTagSets;
		assessCost(pm,EVALpm,position,EVALPosition,tTagSets);
		EVALpm->setEval();
		return true;
	}
	return false;
}

bool Source::checkRelation(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,tIWMM verbWord,tIWMM objectWord,int relationType)
{ LFS
	if (objectWord==wNULL) return false;
	tFI::cRMap *rm=verbWord->second.relationMaps[relationType];
	if (objectWord->second.mainEntry!=wNULL) objectWord=objectWord->second.mainEntry;
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
																vector <tTagLocation> &tagSet,bool infinitive,bool assessCost,int &voRelationsFound,int &traceSource)
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
		if (debugTrace.traceVerbObjects)
			lplog(L"          %d:verb is infinitive but verb not found - skipping.",position);
		return 0;
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
	normalize(verbWord->second.usagePatterns,tFI::VERB_HAS_0_OBJECTS,3);
	normalize(verbWord->second.deltaUsagePatterns,tFI::VERB_HAS_0_OBJECTS,3);
	unsigned int numObjects=0;
	int object1=-1,object2=-1,wo1=-1,wo2=-1;
	tIWMM object1Word=wNULL,object2Word=wNULL;
	//tFI::cRMap *rm=(tFI::cRMap *)NULL;
	tFI::cRMap::tIcRMap tr=tNULL;
	bool success;
	if (whereObjectTag>=0)
	{
		if (assessCost)
			success=resolveObjectTagBeforeObjectResolution(tagSet,whereObjectTag,object1Word);
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
			if (!(success=resolveObjectTagBeforeObjectResolution(tagSet,nextObjectTag,object1Word)))
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
		if (nextWord+1<m.size() && 
				(m[nextWord].word->first==L"not" || m[nextWord].word->first==L"never" || // is it a not or never?
				 (m[nextWord].forms.isSet(adverbForm) && m[nextWord].word->second.usageCosts[m[nextWord].queryForm(adverbForm)]<4 &&  // is it possibly an adverb?
					(!m[nextWord].forms.isSet(verbForm) ||                                 // and definitely not a verb (don't skip it unnecessarily)
					 (m[nextWord+1].forms.isSet(verbForm) && m[nextWord+1].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE))))) // OR is the next word after a verb participle?
					nextWord++; // could possibly be an adverb in between
		// they were at once taken up to his suite. - be conservative by only including the most likely (lowest cost) path
		// Don't include 'that' as an adverb! / Another voice which Tommy rather thought was that[voice] of Boris replied :
		if (nextWord<m.size() && m[whereVerb].forms.isSet(isForm) && m[nextWord].forms.isSet(prepositionForm) && 
				((prepForm=m[nextWord].queryForm(prepositionForm))>=0 && m[nextWord].word->second.usageCosts[prepForm]==0))
		{
			int maxLen=-1,element;
			if ((element=m[nextWord].pma.queryMaximumLowestCostPattern(L"_PP",maxLen))!=-1 && m[nextWord+maxLen].forms.isSet(verbForm))
				nextWord+=maxLen;
		}
		// if the word after this verb is compatible (it should be included as part of the verb), yet is not included, 
		// then punch up the cost.
		verbAfterVerbCost=calculateVerbAfterVerbUsage(whereVerb,nextWord);
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
		// increase parent pattern cost at verb
		verbObjectCost=verbWord->second.usageCosts[tFI::VERB_HAS_0_OBJECTS+numObjects];
		if (numObjects>0)
		{
			wstring w=m[tagSet[whereObjectTag].sourcePosition].word->first;
			if (tagSet[whereObjectTag].len==1 && (w==L"i" || w==L"he" || w==L"she" || w==L"we" || w==L"they"))
			{
				verbObjectCost+=6;
				if (debugTrace.traceVerbObjects)
					lplog(L"          %d:objectWord %s is a nominative pronoun used as an accusative.",
					tagSet[whereObjectTag].sourcePosition,object1Word->first.c_str(),verbObjectCost);
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
		if (numObjects==1 && verbObjectCost && whereVerb+1<m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0 && m[whereVerb+1].queryForm(particleForm)>=0 &&
			// if the particle is followed by a preposition, then don't decrease the cost of having an object because then that encourages the preposition to become an adverb.
			// also include a case where 'to' is not a preposition because of to-day and to-morrow
				(whereVerb+3>=m.size() || m[whereVerb+2].queryForm(prepositionForm)<0 || m[whereVerb+3].word->first==L"-"))
		{
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:decreased verbObjectCost=%d to %d for verb %s because of possible particle usage (%s)",
							tagSet[verbTagIndex].sourcePosition,verbObjectCost,0,verbWord->first.c_str(),m[whereVerb+1].word->first.c_str());
			verbObjectCost=0;
		}
		if (numObjects==2)
		{
			wstring w=m[tagSet[nextObjectTag].sourcePosition].word->first;
			if (tagSet[nextObjectTag].len==1 && (w==L"i" || w==L"he" || w==L"she" || w==L"we" || w==L"they"))
			{
				verbObjectCost+=6;
				wstring tmpstr;
				if (debugTrace.traceVerbObjects && object2>=0)
					lplog(L"          %d:objectWord %s is a nominative pronoun used as an accusative (verbObjectCost=%d).",
								tagSet[nextObjectTag].sourcePosition,objectString(object2,tmpstr,true).c_str(),verbObjectCost);
			}
			if (debugTrace.traceVerbObjects)
				lplog(L"          %d:increased objectDistanceCost=%d to %d (object1@%d-%d, object2@%d-%d)",
							tagSet[verbTagIndex].sourcePosition,objectDistanceCost,objectDistanceCost+(tagSet[nextObjectTag].sourcePosition-(tagSet[whereObjectTag].sourcePosition+tagSet[whereObjectTag].len)),
							tagSet[whereObjectTag].sourcePosition,tagSet[whereObjectTag].sourcePosition+tagSet[whereObjectTag].len,
							tagSet[nextObjectTag].sourcePosition,tagSet[nextObjectTag].sourcePosition+tagSet[nextObjectTag].len);
			objectDistanceCost+=(tagSet[nextObjectTag].sourcePosition-(tagSet[whereObjectTag].sourcePosition+tagSet[whereObjectTag].len));
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
				lplog(L"          %d:verb %s has verbAfterVerbCost=%d. voRelationsFound cancelled.",
				tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),verbAfterVerbCost);
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
	verbWord->second.usagePatterns[tFI::VERB_HAS_0_OBJECTS+numObjects]++;
	verbWord->second.deltaUsagePatterns[tFI::VERB_HAS_0_OBJECTS+numObjects]++;
	int transferTotal=0;
	for (unsigned int I=tFI::VERB_HAS_0_OBJECTS; I<tFI::VERB_HAS_0_OBJECTS+3; I++)
		transferTotal+=verbWord->second.usagePatterns[tFI::VERB_HAS_0_OBJECTS+I];
	if ((transferTotal&15)==15)
		verbWord->second.transferUsagePatternsToCosts(tFI::HIGHEST_COST_OF_INCORRECT_VERB_USAGE,tFI::VERB_HAS_0_OBJECTS,3);
	if (debugTrace.traceVerbObjects)
		lplog(L"          %d:verb %s has %d objects.",tagSet[verbTagIndex].sourcePosition,verbWord->first.c_str(),numObjects);
	return 0;
}

int Source::assessCost(patternMatchArray::tPatternMatch *parentpm,patternMatchArray::tPatternMatch *pm,int parentPosition,int position,vector < vector <tTagLocation> > &tagSets)
{ LFS
	if (debugTrace.traceVerbObjects || debugTrace.traceDeterminer || debugTrace.traceBNCPreferences || debugTrace.traceAgreement)
	{
		if (parentpm==NULL)
			lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) ASSESS COST.",position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position);
		else
			lplog(L"position %d:pma %d:%s[%s](%d,%d) ASSESS COST ( from %s[%s](%d,%d) ).",
			position,pm-m[position].pma.content,patterns[pm->getPattern()]->name.c_str(),patterns[pm->getPattern()]->differentiator.c_str(),position,pm->len+position,
			patterns[parentpm->getPattern()]->name.c_str(),patterns[parentpm->getPattern()]->differentiator.c_str(),parentPosition,parentpm->len+parentPosition);
	}
	tagSets.clear();
	if (!pema[pm->pemaByPatternEnd].flagSet(patternElementMatchArray::COST_EVAL) && /*!stopRecursion && */startCollectTags(debugTrace.traceEVALObjects,EVALTagSet,position,pm->pemaByPatternEnd,tagSets,true,false)>0)
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
							evalAssessed|=assessEVALCost(tagSets[J][K],p,pm,position);
				}
				else
					evalAssessed|=assessEVALCost(tagSets[J][K],tagSets[J][K].pattern,pm,position);
			}
		}
		if (evalAssessed && (debugTrace.traceVerbObjects || debugTrace.traceDeterminer || debugTrace.traceBNCPreferences || debugTrace.traceAgreement))
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
		startCollectTags(debugTrace.traceAgreement,agreementTagSet,position,pm->pemaByPatternEnd,tagSets,true,false)>0)
	{
		pema[pm->pemaByPatternEnd].setFlag(patternElementMatchArray::COST_AGREE);
		vector <costTagSet> saveSecondaryPEMAPositions=secondaryPEMAPositions; // evaluateAgreement now calls startCollectTags as well...
		vector <int> costs,traceSources;
		for (unsigned int J=0; J<tagSets.size(); J++)                          // which erases secondaryPEMAPositions
		{
			if (J && tagSetSame(tagSets[J],tagSets[J-1]))
			{
				costs.push_back(costs[J-1]);
				traceSources.push_back(traceSources[J-1]);
				if (debugTrace.traceAgreement)
					lplog(L"AGREE TAGSET #%d (REPEAT OF PREVIOUS) - cost %d.",J,costs[J-1]);
				continue;
			}
			else
			{
				if (debugTrace.traceAgreement)
					printTagSet(LOG_INFO,L"AGREE",J,tagSets[J],position,pm->pemaByPatternEnd);
				int traceSource=-1;
				costs.push_back(evaluateAgreement(parentpm,pm,parentPosition,position,tagSets[J],traceSource));
				traceSources.push_back(traceSource);
			}
		}
		if (tagSets.size()) setSecondaryCosts(costs,saveSecondaryPEMAPositions,traceSources,pm,position);
	}
	tagSets.clear();
	if (!preTaggedSource)
	{
		bool infinitive=false;
		if (!pema[pm->pemaByPatternEnd].flagSet(patternElementMatchArray::COST_NVO) &&
			(startCollectTags(debugTrace.traceVerbObjects,verbObjectsTagSet,position,pm->pemaByPatternEnd,tagSets,true,false)>0 ||
			(infinitive=(startCollectTags(debugTrace.traceVerbObjects,iverbTagSet,position,pm->pemaByPatternEnd,tagSets,true,false)>0))))
		{
			pema[pm->pemaByPatternEnd].setFlag(patternElementMatchArray::COST_NVO);
			vector <costTagSet> saveSecondaryPEMAPositions=secondaryPEMAPositions; // evaluateVerbObjects now calls startCollectTags as well...
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
					int tmpVOCost=evaluateVerbObjects(parentpm,pm,parentPosition,position,tagSets[J],infinitive,true,tvoRelationsFound,traceSource);
					costs.push_back(tmpVOCost-COST_PER_RELATION*tvoRelationsFound);
					traceSources.push_back(traceSource);
				}
			}
			if (tagSets.size()) setSecondaryCosts(costs,saveSecondaryPEMAPositions,traceSources,pm,position);
		}
		tagSets.clear();
		evaluateNounDeterminers(pm->pemaByPatternEnd,position,tagSets); // this does not include blocked prepositional phrases
		tagSets.clear();
		// finds all first-level prepositional phrases (otherwise these are blocked because PREPOBJECT is blocked inside of a PREP
		if (startCollectTags(debugTrace.traceDeterminer,ndPrepTagSet,position,pm->pemaByPatternEnd,tagSets,true,false)>0)
		{
			for (unsigned int J=0; J<tagSets.size(); J++)                          
				for (unsigned int K=0; K<tagSets[J].size(); K++)
				{
					vector < vector <tTagLocation> > ndTagSets;					
					evaluateNounDeterminers(abs(tagSets[J][K].PEMAOffset),tagSets[J][K].sourcePosition,ndTagSets); // this does not include blocked prepositional phrases
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
	if (debugTrace.traceVerbObjects || debugTrace.traceDeterminer || debugTrace.traceAgreement || debugTrace.traceBNCPreferences)
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

int tFI::getLowestTopLevelCost(void)
{ LFS
	int lowestCost=10000;
	for (unsigned int f=0; f<count; f++)
		if (Form(f)->isTopLevel)
			lowestCost=min(lowestCost,usageCosts[f]);
	return lowestCost;
}

int tFI::getLowestCost(void)
{ LFS
	int lowestCost=10000,offset=-1;
	for (unsigned int f=0; f<count; f++)
		if (lowestCost>usageCosts[f] || (lowestCost==usageCosts[f] && forms()[f]!=PROPER_NOUN_FORM_NUM))
		{
			lowestCost=usageCosts[f];
			offset=f;
		}
	return offset;
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
bool WordMatch::compareCost(int AC1,int LEN1,int lowestSeparatorCost,bool alsoSet)
{ LFS
	if (lowestSeparatorCost>=0 && AC1>=lowestSeparatorCost) return false; // prevent patterns that match only separators optimally
	int AC2=minAvgCostAfterAssessCost,LEN2=maxLACAACMatch;
	bool setInternal=false;
	if (LEN2==0) setInternal=true;
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
	}
	return setInternal;
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
	tagSets.reserve(4096);
	vector <int> minSeparatorCost;
	vector < vector <unsigned int> > winners;
	minSeparatorCost.reserve(end-begin+1);
	for (unsigned int I=0; I<end-begin+1 && I<m.size()-begin; I++)
		minSeparatorCost[I]=m[begin+I].word->second.lowestSeparatorCost();
	int matchedPositions=0;
	for (unsigned int position=begin; position<end && !exitNow; position++)
	{
		if (debugTrace.tracePatternElimination)
			lplog(L"position %d:PMA count=%d ----------------------",position,m[position].pma.count);
		vector <unsigned int> preliminaryWinnersPreAssessCost;
		patternMatchArray::tPatternMatch *pm=m[position].pma.content;
		for (unsigned int I=0; I<m[position].pma.count; I++,pm++)
		{
			cPattern *p=patterns[pm->getPattern()];
			bool isWinner=false;
			if (!p->isTopLevelMatch(*this,position,position+pm->len))
			{
				if (debugTrace.tracePatternElimination)
					lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 1 not marked as winner (%s).",position,I,
					p->name.c_str(),p->differentiator.c_str(),position,pm->len+position,(p->onlyAloneExceptInSubPatternsFlag)? L"onlyAloneExceptInSubPatternsFlag" : L"no fill flag");
				continue;
			}
			unsigned int bp;
			int len=pm->len; // COSTCALC
			int paca=PRE_ASSESS_COST_ALLOWANCE*len/5; // adjust for length - the longer a pattern is the more other patterns
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
				preliminaryWinnersPreAssessCost.push_back(I);
			else if (debugTrace.tracePatternElimination)
				lplog(L"position %d:pma %d:pattern %s[%s](%d,%d) PHASE 1 is not a winner (%d<%d) OR averageCost=%d.",position,I,
				p->name.c_str(),p->differentiator.c_str(),position,len+position,len,m[position].maxLACMatch,averageCost);
		}
		for (unsigned int I=0; I<preliminaryWinnersPreAssessCost.size(); I++)
		{
			pm=m[position].pma.content+preliminaryWinnersPreAssessCost[I];
			assessCost(NULL,pm,-1,position,tagSets);
		}
		winners.push_back(preliminaryWinnersPreAssessCost);
	}
	for (unsigned int position=begin; position<end && !exitNow; position++)
	{
		patternMatchArray::tPatternMatch *pm;
		int maxLen=0;
		vector <unsigned int> preliminaryWinners;
		for (unsigned int I=0; I<winners[position-begin].size(); I++)
		{
			pm=m[position].pma.content+winners[position-begin][I];
			cPattern *p=patterns[pm->getPattern()];
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
					if (m[bp].compareCost(minAvgCostAfterAssessCost,len,minSeparatorCost[bp-begin],true))
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
	}
	if (debugTrace.tracePatternElimination)
		for (unsigned int bp=begin; bp<end; bp++)
			lplog(L"position %d: PHASE 2 cost=%d len=%d",bp,m[bp].minAvgCostAfterAssessCost,m[bp].maxLACAACMatch);
	for (unsigned int position=begin; position<end && !exitNow; position++)
	{
		patternMatchArray::tPatternMatch *pm;
		for (unsigned int winner=0; winner<winners[position-begin].size(); winner++)
		{
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
				if (m[bp].compareCost(minAvgCostAfterAssessCost,len,minSeparatorCost[bp-begin],false))
					globalMinAvgCostAfterAssessCostWinner=true;
				else if (debugTrace.tracePatternElimination)
					lplog(L"%d:%s[%s](%d,%d) PHASE 3 lost GMACAACW (%d>%d) (OR %d>%d) [cost=%d len=%d].",bp,p->name.c_str(),p->differentiator.c_str(),position,len+position,
					minAvgCostAfterAssessCost,m[bp].minAvgCostAfterAssessCost,m[bp].maxLACAACMatch,len,pm->getCost(),len);
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
						pm->getCost(),minAvgCostAfterAssessCost,bp);
					markChildren(pema.begin()+pm->pemaByPatternEnd,position,0,MIN_INT);
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
	return (matchedPositions<0) ? 0 : matchedPositions;
}
