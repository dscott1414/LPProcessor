#include <windows.h>
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "source.h"
#include "time.h"
#include "malloc.h"
#include "profile.h"

bool Source::tagInFocus(int begin,int end)
{ LFS
	for (unsigned I=0; I<collectTagsFocusPositions.size(); I++)
		if (begin<=collectTagsFocusPositions[I] && end>collectTagsFocusPositions[I])
			return true;
	return false;
}

#if defined NDEBUG
#define COLLECT_TAGS_TIME_LIMIT 100
#else
#define COLLECT_TAGS_TIME_LIMIT 400
#endif


int Source::replicate(int recursionLevel,int PEMAPosition,int position,vector <tTagLocation> &tagSet,vector < vector <tTagLocation> > &childTagSets,vector < vector <tTagLocation> > &tagSets, unordered_map <int, vector < vector <tTagLocation> > > &TagSetMap)
{ LFS
	size_t originalTagSetSize=tagSet.size();
	for (unsigned I=0; I<childTagSets.size(); I++)
	{
		tagSet.insert(tagSet.end(),childTagSets[I].begin(),childTagSets[I].end());
		collectTags(recursionLevel,PEMAPosition,position,tagSet,tagSets,TagSetMap);
		tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
	}
	if (!childTagSets.size())
	{
		collectTags(recursionLevel,PEMAPosition,position,tagSet,tagSets,TagSetMap);
		tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
	}
	return childTagSets.size();
}

bool tagSetSame(vector <tTagLocation> &tagSet,vector <tTagLocation> &tagSetNew)
{ LFS
	if (tagSet.size()!=tagSetNew.size()) return false;
	for (unsigned int I=0; I<tagSet.size(); I++)
		if (tagSet[I]!=tagSetNew[I])
			return false;
	return true;
}

int Source::collectTags(int recursionLevel,int PEMAPosition,int position,vector <tTagLocation> &tagSet,vector < vector <tTagLocation> > &tagSets, unordered_map <int, vector < vector <tTagLocation> > > &TagSetMap)
{ LFS
	if (PEMAPosition<0)
	{
		if (timerForExit++==31 && !debugTrace.tracePatternElimination && (clock()-beginTime)>COLLECT_TAGS_TIME_LIMIT)
			exitTags=true;
		bool duplicate=false;
		if (recursionLevel)
			for (size_t ts=(tagSets.size()>10) ? tagSets.size()-10 : 0; ts<tagSets.size() && !duplicate; ts++)
				duplicate=tagSetSame(tagSet,tagSets[ts]);
		// !duplicate && tagSet.size() if removing empty tagsets from top level EMPTAG
		if (recursionLevel==0 && secondaryPEMAPositions.size() && secondaryPEMAPositions[secondaryPEMAPositions.size()-1].tagSet!=tagSets.size())
			secondaryPEMAPositions.push_back(costPatternElementByTagSet(position,-PEMAPosition,-1,tagSets.size(),pema[-PEMAPosition].getElement()));
		if (!duplicate) // Eliminate empty tagsets? (recursionLevel || tagSet.size()) && (EMPTAG)
		{
			tagSets.push_back(tagSet);
			exitTags=(tagSets.size()>MAX_TAGSETS);
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
	int pattern=pema[PEMAPosition].getPattern(),patternElement=pema[PEMAPosition].getElement();
	int begin=pema[PEMAPosition].begin+position,end=pema[PEMAPosition].end+position;
	size_t relativeEnd=end-position,originalTagSetSize=tagSet.size();
	int relativeBegin=begin-position;
	patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+PEMAPosition;
	if (debugTrace.traceTags)
		lplog(L"%*s%d:%06d %s[%s](%d,%d) element #%d #tags collected=%d",recursionLevel*2," ",position,PEMAPosition,
		patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,patternElement,tagSet.size());
	tagSet.reserve(6);
	for (;  PEMAPosition>=0 && pem->getPattern()==pattern && pem->end==relativeEnd && !exitTags; PEMAPosition=pem->nextByPatternEnd,pem=pema.begin()+PEMAPosition)
		if (pem->begin==relativeBegin)
		{
			if (recursionLevel==0) // pushing down PEMAPositions to gain more accuracy
				secondaryPEMAPositions.push_back(costPatternElementByTagSet(position,PEMAPosition,-1,tagSets.size(),patternElement));
			int nextPEMAPosition,nextPosition=position,tag;
			if ((nextPEMAPosition=pem->nextPatternElement)>=0)
				nextPosition=begin-pema[nextPEMAPosition].begin; // update next position
			else
				nextPEMAPosition=-PEMAPosition;
			if (pem->isChildPattern())
			{
				// must match to root pattern because of compression done in patternElementMatchArray::push_back_unique
				unsigned int childEnd=pem->getChildLen(),beginTag=0;
				while ((tag=patterns[pattern]->elementHasTagInSet(pem->getElement(),pem->getElementIndex(),desiredTagSetNum,beginTag,true))>=0)
				{
					if (debugTrace.traceTags)
						lplog(L"%*s%d:TAG %s FOUND %s[%s](%d,%d) %s[*]",recursionLevel*2," ",position,patternTagStrings[tag].c_str(),
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,patterns[pem->getChildPattern()]->name.c_str());
					tagSet.push_back(tTagLocation(tag,pem->getChildPattern(),pattern,pem->getElement(),position,childEnd,-PEMAPosition,true));
				}
				// element blocks all descendants.  Continue with next element.
				if (blocking && patterns[pattern]->stopDescendingTagSearch(pem->getElement(),pem->getElementIndex(),pem->isChildPattern()))
				{
					if (debugTrace.traceTags)
						lplog(L"%*s%d:%s[%s](%d,%d) BLOCKED element #%d index #%d",(recursionLevel+1)*2," ",
						position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,
						pem->getElement(),pem->getElementIndex());
					//if (recursionLevel==0)
					//    secondaryPEMAPositions.push_back(costPatternElementByTagSet(position,PEMAPosition,-1,tagSets.size(),patternElement));
					if (!exitTags)
						collectTags(recursionLevel,nextPEMAPosition,nextPosition,tagSet,tagSets,TagSetMap);
					tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
					continue;
				}
				size_t originalTagSetSizePlusParentsTags=tagSet.size();
				for (int p=patterns[pem->getChildPattern()]->rootPattern; p>=0 && !exitTags; p=patterns[p]->nextRoot)
				{
					if (p==pattern || !m[position].patterns.isSet(p)) continue;
					patternMatchArray::tPatternMatch *pma=m[position].pma.find(p,childEnd);
					if (pma==NULL) continue;
					int childPEMAPosition=pma->pemaByPatternEnd;
					patternElementMatchArray::tPatternElementMatch *childPem=pema.begin()+childPEMAPosition;
					for (; childPEMAPosition>=0 && childPem->getPattern()==p && childPem->end==childEnd; childPEMAPosition=childPem->nextByPatternEnd,childPem=pema.begin()+childPEMAPosition)
						if (!childPem->begin) break;
					if (childPEMAPosition<0 || childPem->getPattern()!=p || childPem->end!=childEnd || childPem->begin) continue;
					//if (recursionLevel==0) // pushing down PEMAPositions to gain more accuracy
					//  secondaryPEMAPositions.push_back(costPatternElementByTagSet(position,PEMAPosition,childPEMAPosition,tagSets.size(),patternElement));
					beginTag=0;
					bool found=false;
					while ((tag=patterns[p]->hasTagInSet(desiredTagSetNum,beginTag))>=0)
					{
						if (debugTrace.traceTags)
							lplog(L"%*s%d:TAG %s FOUND (pattern) %s[%s](%d,%d)",recursionLevel*2," ",position,patternTagStrings[tag].c_str(),
							patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position,position+childEnd);
						tagSet.push_back(tTagLocation(tag,p,pattern,pem->getElement(),position,childEnd,childPEMAPosition,true));
					}
					if (found)
					{
						tagSet.erase(tagSet.begin()+originalTagSetSizePlusParentsTags,tagSet.end());
						continue;
					}
					// child pattern blocks all descendants.  This is only executed in the first element of the pattern to
					// grab all the tags which are not blocked.
					if (focused && !tagInFocus(position,position+childEnd))
						collectTags(recursionLevel,nextPEMAPosition,nextPosition,tagSet,tagSets,TagSetMap);
					else if (blocking && patterns[p]->blockDescendants)
					{
						if (debugTrace.traceTags)
							lplog(L"%*s%d:%s[%s](%d,%d) BLOCKED",(recursionLevel+1)*2," ",
							position,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position,position+childEnd);
						if (!exitTags)
							collectTags(recursionLevel,nextPEMAPosition,nextPosition,tagSet,tagSets,TagSetMap);
					}
					else if (!(patterns[p]->includesOneOfTagSet&((__int64)1<<desiredTagSetNum)))
					{
						if (debugTrace.traceTags)
							lplog(L"%*s%d:%06d %s[%s](%d,%d) (not in tag set %s)",(recursionLevel+1)*2," ",
							position,childPEMAPosition,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position,position+childEnd,
							desiredTagSets[desiredTagSetNum].name.c_str());
						if (!exitTags)
							collectTags(recursionLevel,nextPEMAPosition,nextPosition,tagSet,tagSets,TagSetMap);
					}
					else
					{
						if (TagSetMap.find(childPEMAPosition)==TagSetMap.end())
						{
							vector <tTagLocation> childTagSet;
							vector < vector <tTagLocation> > childTagSets;
							if (!exitTags)
								collectTags(recursionLevel+2,childPEMAPosition,position,childTagSet,childTagSets,TagSetMap);
							TagSetMap[childPEMAPosition]=childTagSets;
							if (debugTrace.traceTags)
							{
								int nonNullTagSets=0;
								for (unsigned int K=0; K<childTagSets.size(); K++)
									if (childTagSets[K].size())
										nonNullTagSets++;
								lplog(L"%*s%d:childPEMAPosition %d resulted in %d tagSets (%d nonNull)",recursionLevel*2," ",position,childPEMAPosition,
									childTagSets.size(),nonNullTagSets);
							}
						}
						replicate(recursionLevel,nextPEMAPosition,nextPosition,tagSet,TagSetMap[childPEMAPosition],tagSets,TagSetMap);
					}
					tagSet.erase(tagSet.begin()+originalTagSetSizePlusParentsTags,tagSet.end());
				}
			}
			else
			{
				unsigned int beginTag=0;
				while ((tag=patterns[pattern]->elementHasTagInSet(pem->getElement(),pem->getElementIndex(),desiredTagSetNum,beginTag,false))>=0)
				{
					if (debugTrace.traceTags)
						lplog(L"%*s%d:TAG %s FOUND form %s",recursionLevel*2," ",position,patternTagStrings[tag].c_str(),
						Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str());
					tagSet.push_back(tTagLocation(tag,m[position].getFormNum(pem->getChildForm()),pattern,pem->getElement(),position,1,PEMAPosition,false));
				}
				if (debugTrace.traceTags)
					lplog(L"%*s%d:%s[%s](%d,%d) form %s element #%d",(recursionLevel+1)*2," ",
					position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,
					Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str(),
					pem->getElement());
				//if (recursionLevel==0) // pushing down PEMAPositions to gain more accuracy
				//    secondaryPEMAPositions.push_back(costPatternElementByTagSet(position,PEMAPosition,-1,tagSets.size(),patternElement));
				if (!exitTags)
					collectTags(recursionLevel,nextPEMAPosition,nextPosition,tagSet,tagSets,TagSetMap);
			}
			tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
		}
		return tagSets.size();
}

bool Source::getVerb(vector <tTagLocation> &tagSet,int &tag)
{ LFS
	int nextVerbTag=-1,nextVObjectTag=-1,nextVAgreeTag=-1;
	int whereVerbTag=findTag(tagSet,L"VERB",nextVerbTag);
	if (whereVerbTag<0) return false;
	int whereVObjectTag=findTagConstrained(tagSet,L"V_OBJECT",nextVObjectTag,tagSet[whereVerbTag]);
	// if there is no vobject, take last vagree, otherwise, take last vobject.
	if (nextVObjectTag>=0) tag=nextVObjectTag;
	else if (whereVObjectTag>=0) tag=whereVObjectTag;
	else
	{
		int whereVAgreeTag=findTagConstrained(tagSet,L"V_AGREE",nextVAgreeTag,tagSet[whereVerbTag]);
		if (nextVAgreeTag>=0) tag=nextVAgreeTag;
		else if (whereVAgreeTag>=0) tag=whereVAgreeTag;
		else return false;
	}
	return true;
}

bool Source::getIVerb(vector <tTagLocation> &tagSet,int &tag)
{ LFS
	int nextVObjectTag=-1;
	int whereVObjectTag=findTag(tagSet,L"V_OBJECT",nextVObjectTag);
	// if there is no vobject, take last vagree, otherwise, take last vobject.
	if (nextVObjectTag>=0) tag=nextVObjectTag;
	else if (whereVObjectTag>=0) tag=whereVObjectTag;
	else return false;
	return true;
}

void tFI::transferUsagePatternsToCosts(int highestCost,unsigned int upStart,unsigned int upLength)
{ LFS
	int highest=-1,lowest=255,K=0;
	for (unsigned int up=upStart; up<upStart+upLength; up++)
	{
		highest=max(highest,usagePatterns[up]);
		lowest=min(lowest,usagePatterns[up]);
	}
	if (lowest && (K=highest/lowest)<2) return;
	if (highest<5) return;
	if (!lowest || K>highestCost)
	{
		// 8 4 0 -> K=2  0 2 4
		// 16 2 2 -> K=8 0 4 4
		// example: word trust
		//wordId | formId | count  cost
		//  7139 |    100 |   886  0    noun (singular)                      (4*(886-886))/886==0
		//  7139 |    101 |    16  3    adjective                            (4*(886- 16))/886==3
		//  7139 |    102 |   418  2    verb (present tense first person)    (4*(886-418))/886==2
		for (unsigned int up=upStart; up<upStart+upLength; up++)
			usageCosts[up]=(highestCost*(highest-((unsigned int)usagePatterns[up])))/highest;
	}
	else
	{
		// 8 4 4 -> K=2  0 1 1
		for (unsigned int up=upStart; up<upStart+upLength; up++)
			usageCosts[up]=K-(((unsigned int)usagePatterns[up])/lowest);
	}
}

bool Source::tagIsCertain(int position)
{ LFS
	return !preTaggedSource || (m[position].flags&WordMatch::flagBNCFormNotCertain)==0;
}

bool Source::resolveObjectTagBeforeObjectResolution(vector <tTagLocation> &tagSet,int tag,tIWMM &word)
{ LFS
	if (tag<0) return false;
	if (tagSet[tag].len==1)
		word=resolveToClass(tagSet[tag].sourcePosition);
	else
	{
		// if object is not type of NOUN, forget it. (searching for N_AGREE in an S1 would be pointless and very time-consuming).
		if (patterns[tagSet[tag].pattern]->name.find(L"NOUN")==wstring::npos || patterns[tagSet[tag].pattern]->hasTag(GNOUN_TAG) || patterns[tagSet[tag].pattern]->hasTag(MNOUN_TAG))
			return false;
		vector < vector <tTagLocation> > tagSets;
		// He gave a book.
		if (startCollectTagsFromTag(false,nAgreeTagSet,tagSet[tag],tagSets,GNOUN_TAG,false)>0 || 
			  startCollectTagsFromTag(false,nAgreeTagSet,tagSet[tag],tagSets,MNOUN_TAG,false)>0 )
			for (unsigned int J=0; J<tagSets.size(); J++)
				if (tagSets[J].size()==1)
				{
					word=resolveToClass(tagSets[J][0].sourcePosition);
					break;
				}
				if (word==wNULL) return false;
	}
	return true;
}

// Proper Noun agreement check
// If the noun has any proper nouns but is not all proper nouns:
// IF there is a determiner, followed by a lowest cost adjective (optional), and a Proper Noun, OK.
// otherwise, add 10.
// 16953:the practical Tommy[16953-16956][16955][name  ][NEUTER]**[A:tommy [193]]**
// 37780:this young Tommy[37780-37783][37782][name  ][MALE  ]**[A:tommy [193]]**
// REJECT:
// 60073:a flash Tommy[60073-60076][60075][name  ][NEUTER]**[A:tommy [193]]**
// 21396:the stairs Tommy[21396-21399][21398][name  ][NEUTER]**[A:tommy [193]]**
// 60328:Tommy stopped Conrad[60328-60331][60330][name  ][MALE  ][OGEN]**[A:conrad [51]]**[ambiguous]
// 62307:Tommy , Julius's eyes[62307-62311][62310][nongen][OGEN]
// 65396:added Tommy[65396-65398][65397][name  ][MALE  ]**[A:tommy [193]]**[ambiguous]
// ALSO:
// 87682:Tommy boy[87682-87684][87683][gender][MALE  ][OGEN]
int Source::properNounCheck(int &traceSource,int begin,int end,int whereDet)
{ LFS
	int howManyProperNouns=0,lastProperNoun=-1;
	for (int I=begin; I<end; I++)
		if (m[I].forms.isSet(PROPER_NOUN_FORM_NUM))
		{
			howManyProperNouns++;
			lastProperNoun=I;
		}
		else
			lastProperNoun=-1; // proper noun is used as an adjective?  Government job
	if (howManyProperNouns==end-begin || !howManyProperNouns || lastProperNoun==-1)
		return 0;
	// proper noun is > 4 in length, does not begin with a determiner or does not end with a proper noun.
	// because this is called from evaluateNounDeterminer, this routine cannot be given
	// only a NAME, but a NAME that is part of another larger structure.  At this point, the
	// structure has the proper noun as the last point in the structure.
	// the Savoy to the War Office! is > 4 in length, yet is legal.
	if (whereDet!=begin)
	{
		if (debugTrace.traceDeterminer)
		{
			wstring tmpName;
			for (int I=begin; I<end; I++) tmpName+=m[I].word->first+wstring(L" ");
			lplog(L"%d:PNC name %s is an incorrectly configured proper noun %d %d %d [SOURCE=%06d].",begin,tmpName.c_str(),end-begin,whereDet,lastProperNoun,traceSource=gTraceSource);
		}
		return tFI::COST_OF_INCORRECT_PROPER_NOUN;
	}
	return 0;
	// SKIP
	/*
	// the Savoy to the War Office! has gaps, yet is legal.
	// no non-capitalized gaps allowed in proper noun
	for (int I=begin+1; I<lastProperNoun; I++)
	{
	if (m[I].forms.isSet(PROPER_NOUN_FORM_NUM))
	{
	if (t.traceDeterminer)
	lplog(L"%d:PNC name %s has a gap in proper noun at %d %d [SOURCE=%06d].",begin,tmpName.c_str(),I,lastProperNoun,traceSource=gTraceSource);
	return tFI::COST_OF_INCORRECT_PROPER_NOUN;
	}
	}
	return 0;
	*/
}

bool tlcompare(const vector <tTagLocation> &lhs,const vector <tTagLocation> &rhs)
{ LFS
	if (lhs.size()<rhs.size()) return true;
	if (lhs.size()>rhs.size()) return false;
	for (unsigned int I=0; I<lhs.size(); I++)
	{
		if (((tTagLocation)lhs[I])!=((tTagLocation)rhs[I]))
			return (((tTagLocation)lhs[I])<((tTagLocation)rhs[I]));
	}
	return false;
}

void showDiffTagSets(vector < vector <tTagLocation> > &tagSets,vector < vector <tTagLocation> > &tagSetsNew)
{ LFS
	for (unsigned int I=0; I<tagSets.size(); I++)
		if (!I || !tagSetSame(tagSets[I-1],tagSets[I]))
			printTagSet(LOG_INFO,L"ORIGINAL",I,tagSets[I]);
	for (unsigned int I=0; I<tagSetsNew.size(); I++)
		if (!I || !tagSetSame(tagSetsNew[I-1],tagSetsNew[I]))
			printTagSet(LOG_INFO,L"NEW",I,tagSetsNew[I]);
	lplog(LOG_FATAL_ERROR,L"ERROR!");
}

void compareTagSets(vector < vector <tTagLocation> > tagSets,vector < vector <tTagLocation> > tagSetsNew)
{ LFS
	sort(tagSets.begin(),tagSets.end(),tlcompare);
	sort(tagSetsNew.begin(),tagSetsNew.end(),tlcompare);
	unsigned int I=0,J=0;
	while (J<tagSetsNew.size() && !tagSetsNew[J].size()) J++;
	while (I<tagSets.size() && !tagSets[I].size()) I++;
	if ((J==tagSetsNew.size() && I!=tagSets.size()) || (J!=tagSetsNew.size() && I==tagSets.size()))
		lplog(LOG_FATAL_ERROR,L"New and old have differing data.");
	for (; I<tagSets.size() && J<tagSetsNew.size(); I++,J++)
	{
		while (I<tagSets.size()-1 && tagSetSame(tagSets[I],tagSets[I+1])) I++;
		while (J<tagSetsNew.size()-1 && tagSetSame(tagSetsNew[J],tagSetsNew[J+1])) J++;
		if (I==tagSets.size() && J==tagSetsNew.size()) break;
		if (I==tagSets.size() || J==tagSetsNew.size() || !tagSetSame(tagSets[I],tagSetsNew[J]))
		{
			lplog(LOG_INFO,L"Difference at %d (new) and %d (old).",J,I);
			showDiffTagSets(tagSets,tagSetsNew);
		}
	}
}

size_t Source::startCollectTagsFromTag(bool inTrace,int tagSet,tTagLocation &tl,vector < vector <tTagLocation> > &tagSets,int rejectTag,bool collectSelfTags)
{ LFS
	int pattern=tl.pattern,position=tl.sourcePosition,end=tl.len,PEMAOffset=tl.PEMAOffset;
	if (collectSelfTags && !(patterns[pattern]->includesDescendantsAndSelfAllOfTagSet&((__int64)1<<tagSet)))
		return 0;
	if (!collectSelfTags && !(patterns[pattern]->includesOnlyDescendantsAllOfTagSet&((__int64)1<<tagSet)))
		return 0;
	if (PEMAOffset<0)
	{
		for (int p=patterns[pattern]->rootPattern; p>=0; p=patterns[p]->nextRoot)
		{
			if (!m[position].patterns.isSet(p)) continue;
			if (rejectTag>=0 && patterns[p]->hasTag(rejectTag))
				continue;
			patternMatchArray::tPatternMatch *pma=m[position].pma.find(p,end);
			if (pma==NULL) continue;
			PEMAOffset=pma->pemaByPatternEnd;
			patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+PEMAOffset;
			for (; PEMAOffset>=0 && pem->getPattern()==p && pem->end==end; PEMAOffset=pem->nextByPatternEnd,pem=pema.begin()+PEMAOffset)
				if (!pem->begin) break;
			if (PEMAOffset<0 || pem->getPattern()!=p || pem->end!=end || pem->begin) continue;
			startCollectTags(inTrace,tagSet,position,PEMAOffset,tagSets,true,collectSelfTags);
		}
		return tagSets.size();
	}
	return startCollectTags(inTrace,tagSet,position,PEMAOffset,tagSets,true,collectSelfTags);
}

size_t Source::startCollectTags(bool inTrace,int tagSet,int position,int PEMAPosition,vector < vector <tTagLocation> > &tagSets,bool obeyBlock,bool collectSelfTags)
{  LFS
	secondaryPEMAPositions.clear();
	if (PEMAPosition<0)
	{
		//lplog(LOG_ERROR,L"PEMA offset is negative at position %d - all PEMA positions have been eliminated.",position);
		return 0;
	}
	debugTrace.traceTags=inTrace && debugTrace.traceTagSetCollection;
	int pattern=pema[PEMAPosition].getPattern();
	if (collectSelfTags && !(patterns[pattern]->includesDescendantsAndSelfAllOfTagSet&((__int64)1<<tagSet)))
	{
		if (debugTrace.traceTags)
			lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (not enough tags [descendants and self])",position,PEMAPosition,desiredTagSets[tagSet].name.c_str(),
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
		return 0;
	}
	if (!collectSelfTags && !(patterns[pattern]->includesOnlyDescendantsAllOfTagSet&((__int64)1<<tagSet)))
	{
		if (debugTrace.traceTags)
			lplog(L"%d:======== EVALUATION %06d %s %s[%s](%d,%d) SKIPPED (not enough tags [only descendants])",position,PEMAPosition,desiredTagSets[tagSet].name.c_str(),
						patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
		return 0;
	}
	if (debugTrace.traceTags)
		lplog(L"%d:======== EVALUATING %06d %s %s[%s](%d,%d)",position,PEMAPosition,desiredTagSets[tagSet].name.c_str(),
					patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
	exitTags=false;

	vector <tTagLocation> tTagSet;
	beginTime=clock();
	timerForExit=0;
	blocking=obeyBlock; // only true for BNCPatternViolation
	desiredTagSetNum=tagSet;
	focused=collectTagsFocusPositions.size()>0;
	//TagSetMap.clear();
	if (collectSelfTags)
	{
		int tag;
		unsigned int beginTag=0,p=pema[PEMAPosition].getPattern(),recursionLevel=0;
		//bool found=false;
		while ((tag=patterns[p]->hasTagInSet(desiredTagSetNum,beginTag))>=0)
		{
			if (debugTrace.traceTags)
				lplog(L"%*s%d:TAG %s FOUND (self pattern) %s[%s](%d,%d)",recursionLevel*2," ",position,patternTagStrings[tag].c_str(),
							patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
			tTagSet.push_back(tTagLocation(tag,p,p,pema[PEMAPosition].getElement(),position,pema[PEMAPosition].end,PEMAPosition,true));
		}
	}
	collectTags(0,PEMAPosition,position,tTagSet,tagSets,pemaMapToTagSetsByPemaByTagSet[tagSet]);
	int numTagSets=(signed)tagSets.size();
	for (int J=secondaryPEMAPositions.size()-1; J>=0; J--)
		if (secondaryPEMAPositions[J].tagSet>=numTagSets)
			secondaryPEMAPositions.erase(secondaryPEMAPositions.begin()+J);
	if (!exitTags)
		return tagSets.size();
	if (debugTrace.traceMatchedSentences || debugTrace.traceUnmatchedSentences)
	{
		if (tagSets.size()<MAX_TAGSETS)
			lplog(LOG_ERROR,L"%d:Maximum time limit hit (%d seconds) when collecting tags for %s %s[%s](%d,%d)",position,COLLECT_TAGS_TIME_LIMIT,desiredTagSets[tagSet].name.c_str(),
			patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
		else if (debugTrace.traceTagSetCollection)
			lplog(LOG_ERROR,L"%d:Maximum # of tagsets hit when collecting tags for %s %s[%s](%d,%d)",position,desiredTagSets[tagSet].name.c_str(),
			patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position,position+pema[PEMAPosition].end);
	}
	return tagSets.size();
}

bool WordMatch::isPPN(void)
{ LFS
	// this covers personal_pronoun_nominative,personal_pronoun_accusative,personal_pronoun,all proper names
	return !(word->second.inflectionFlags&NEUTER_GENDER) &&
		((word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON|THIRD_PERSON)) ||
		queryWinnerForm(indefinitePronounForm)>=0 || queryWinnerForm(reciprocalPronounForm)>=0 ||
		queryWinnerForm(accForm)>=0 || queryWinnerForm(nomForm)>=0) ||
		((((word->second.inflectionFlags&MALE_GENDER)==MALE_GENDER) ^ ((word->second.inflectionFlags&FEMALE_GENDER)==FEMALE_GENDER)) &&
		((flags&flagOnlyConsiderProperNounForms) || (flags && queryWinnerForm(PROPER_NOUN_FORM_NUM)>=0)));
}

tIWMM WordMatch::resolveToClass()
{ LFS
	tIWMM w=word;
	if (isPPN())
		w=Words.PPN;
	else if (queryWinnerForm(NUMBER_FORM_NUM)>=0)
		w=Words.NUM;
	else if (queryWinnerForm(dateForm)>=0)
		w=Words.DATE;
	else if (queryWinnerForm(timeForm)>=0)
		w=Words.TIME;
	else if (queryWinnerForm(telenumForm) >= 0)
		w = Words.TELENUM;
	else if (word->second.mainEntry != wNULL)
		w=word->second.mainEntry;
	return w;
}

// this is used before any objects are resolved, or marked for time or location
tIWMM Source::resolveToClass(int where)
{ LFS  
  tIWMM o=wNULL;
	if (m[where].getObject()>=0)
	{
		int beginObjectPosition=m[where].beginObjectPosition;
		if (m[beginObjectPosition].pma.queryPattern(L"_DATE")!=-1)
			return Words.DATE;
		else if (m[beginObjectPosition].pma.queryPattern(L"_TIME")!=-1)
			return Words.TIME;
		else if (m[beginObjectPosition].pma.queryPattern(L"_TELENUM")!=-1)
			return Words.TELENUM;
	}
	if (m[where].isPPN())
		return Words.PPN;
	else if (m[where].queryWinnerForm(NUMBER_FORM_NUM)>=0)
		return Words.NUM;
	else if (m[where].queryWinnerForm(dateForm)>=0)
		return Words.DATE;
	else if (m[where].queryWinnerForm(timeForm)>=0)
		return Words.TIME;
	else if (m[where].queryWinnerForm(telenumForm)>=0)
		return Words.TELENUM;
	else if (m[where].pma.queryPattern(L"_DATE")!=-1)
		return Words.DATE;
	else if (m[where].pma.queryPattern(L"_TIME")!=-1)
		return Words.TIME;
	else if (m[where].pma.queryPattern(L"_TELENUM")!=-1)
		return Words.TELENUM;
	return m[where].resolveToClass();
}

tIWMM Source::resolveObjectToClass(int where,int o)
{ LFS
		int objectClass=objects[o].objectClass;
		if (objects[o].isLocationObject)
			return Words.LOCATION;
		if (objects[o].isTimeObject)
			return Words.TIME;
		if (objects[o].numDefinitelyIdentifiedAsSpeaker>0 || objects[o].numIdentifiedAsSpeaker>0)
			return Words.PPN;
		if (objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || 
			  objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
				objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
				objectClass==GENDERED_RELATIVE_OBJECT_CLASS)
			return Words.PPN;
		// gendered but not a PPN
		if (objectClass==BODY_OBJECT_CLASS || 
			// non-gendered - not PPN
			  objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS ||
				objectClass==NON_GENDERED_BUSINESS_OBJECT_CLASS ||
				objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
			return m[where].word;
		return wNULL;
}

// this is used before any objects are resolved, or marked for time or location
tIWMM Source::fullyResolveToClass(int where)
{ LFS
	tIWMM w=resolveToClass(where);
	wstring word=m[where].word->first;
	if (m[where].getObject()>=0)
	{
		int o=m[where].getObject();
		tIWMM ow=resolveObjectToClass(where,o);
		if (ow!=wNULL) return ow;
		if (w==Words.PPN && objects[o].neuter && !objects[o].female && !objects[o].male)
			return m[where].word;
		if (m[where].objectMatches.size()!=1)
			return w;
		tIWMM specials[]={Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION};
		for (unsigned int I=0; I<sizeof(specials)/sizeof(tIWMM); I++)
			if (w==specials[I])
				return w;
		ow=resolveObjectToClass(where,m[where].objectMatches[0].object);
		if (ow!=wNULL) return ow;
	}
	return w;
}

bool Source::forcePrepObject(vector <tTagLocation> &tagSet,int tag,int &object,int &whereObject,tIWMM &word)
{ LFS
	if (identifyObject(findTag(L"NOUN"),tagSet[tag].sourcePosition,-1,false,-1,-1)>=0 &&
			resolveTag(tagSet,tag,object,whereObject,word))
	{
		wstring tmpstr;
		if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%d:%s object forced by preposition phrase match",whereObject,whereString(whereObject,tmpstr,false).c_str());
		return true;
	}
	return false;
}

/*
All 4-digit numbers replaced with 'date'.
Gendered proper nouns or personal pronouns are replaced with PPN.
*/
bool Source::resolveTag(vector <tTagLocation> &tagSet,int tag,int &object,int &whereObject,tIWMM &word)
{ LFS
	if (tag<0) return false;
	word=wNULL;
	if ((object=findObject(tagSet[tag],whereObject))>=0)
	{
		if (objects[object].objectClass==PRONOUN_OBJECT_CLASS ||
			objects[object].objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS ||
			objects[object].objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS ||
			(objects[object].objectClass==NAME_OBJECT_CLASS &&
			(objects[object].male ^ objects[object].female)) ||
			objects[object].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
			objects[object].objectClass==BODY_OBJECT_CLASS ||
			objects[object].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			objects[object].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
			objects[object].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
			objects[object].objectClass==META_GROUP_OBJECT_CLASS)
			word=Words.PPN;
		else if (tagIsCertain(objects[object].originalLocation))
			word=resolveToClass(objects[object].originalLocation);
	}
	if (tagSet[tag].len==1 && tagIsCertain(tagSet[tag].sourcePosition) && (object<0 || m[tagSet[tag].sourcePosition].getObject()>=0))
		word=resolveToClass(whereObject=tagSet[tag].sourcePosition);
	if (word==wNULL)
		return false;
	return true;
}

