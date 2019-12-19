#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "word.h"
#include "source.h"
#include "profile.h"

patternElementMatchArray::patternElementMatchArray()
{ LFS
    count=0;
    allocated=0;
    content=NULL;
};

patternElementMatchArray::~patternElementMatchArray()
{ LFS
    if (allocated) tfree(allocated*sizeof(*content),content);
    count=0;
    allocated=0;
    content=NULL;
}

void patternElementMatchArray::clear(void)
{ LFS
    if (allocated) tfree(allocated*sizeof(*content),content);
    allocated=0;
    count=0;
  content=NULL;
}

patternElementMatchArray::patternElementMatchArray(const patternElementMatchArray &rhs)
{ LFS
    count=rhs.count;
    allocated=rhs.allocated;
    content=NULL;
    if (allocated)
    {
        content = (tPatternElementMatch *)tmalloc(allocated*sizeof(*content));
        if (!content)
      lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (5)");
        memcpy(content,rhs.content,count*sizeof(*content));
    }
}

void patternElementMatchArray::minimize(void)
{ LFS
    int oldAllocated=allocated;
    allocated=count;
    content=(tPatternElementMatch *)trealloc(6,content,oldAllocated*sizeof(*content),allocated*sizeof(*content));
}

bool patternElementMatchArray::write(IOHANDLE file)
{
	LFS
		_write(file, &count, sizeof(count));
	_write(file, content, count * sizeof(*content));
	return true;
}

bool patternElementMatchArray::WriteFile(HANDLE file)
{
	LFS
	DWORD dwBytesWritten;
	if (!::WriteFile(file, &count, sizeof(count), &dwBytesWritten, NULL) || dwBytesWritten != sizeof(count)) return false;
	if (!::WriteFile(file, content, count * sizeof(*content), &dwBytesWritten, NULL) || dwBytesWritten != count * sizeof(*content)) return false;
	return true;
}

bool patternElementMatchArray::read(IOHANDLE file)
{ LFS
    _read(file,&count,sizeof(count));
    allocated=count;
    if (count>1000000)
    {
        lplog(LOG_ERROR,L"Illegal count of %d (>1000000) encountered!");
        return false; // extremely unlikely to have more than this # of matches
    }
    content=(tPatternElementMatch *)tmalloc(count*sizeof(*content));
    if (!content)
    {
        lplog();
        return false;
    }
    _read(file,content,count*sizeof(*content));
    return true;
}

bool patternElementMatchArray::write(void *buffer,int &where,unsigned int limit)
{ LFS
  if (!copy(buffer,count,where,limit)) return false;
	if (where+count*sizeof(*content)>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (15)",limit);
  memcpy(((char *)buffer)+where,content,count*sizeof(*content));
  where+=count*sizeof(*content);
  return true;
}

bool patternElementMatchArray::read(char *buffer,int &where,unsigned int limit)
{ LFS
  if (!copy(count,buffer,where,limit)) return false;
	if (where+count*sizeof(*content)>limit) 
	{
		lplog(LOG_ERROR,L"Maximum read copy limit of %d bytes reached! (15)",limit);
		count=0;
		return false;
	}
  allocated=count;
  content=(tPatternElementMatch *)tmalloc(count*sizeof(*content));
  if (!content)
  {
    lplog();
    return false;
  }
  memcpy(content,buffer+where,count*sizeof(*content));
  where+=count*sizeof(*content);
  return true;
}

bool patternElementMatchArray::operator==(const patternElementMatchArray other) const
{ LFS
    if (count!=other.count) return false;
  return memcmp(content, other.content,count*sizeof(*content))==0;
}

patternElementMatchArray& patternElementMatchArray::operator=(const patternElementMatchArray &rhs)
{ LFS
    if (allocated) tfree(allocated*sizeof(*content),content);
    count=rhs.count;
    allocated=rhs.allocated;
    content=NULL;
    if (allocated)
    {
        content = (tPatternElementMatch *)tmalloc(allocated*sizeof(*content));
        if (!content)
      lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (7)");
        memcpy(content,rhs.content,count*sizeof(*content));
    }
    return *this;
}

bool patternElementMatchArray::operator!=(const patternElementMatchArray other) const
{ LFS
    if (count!=other.count) return true;
  return memcmp(content, other.content,count*sizeof(*content))!=0;
}

patternElementMatchArray::reference patternElementMatchArray::operator[](unsigned int _P0)
{ LFS
    #ifdef INDEX_CHECK
        static int catchError=0;
        catchError++;
        if (_P0>=count || _P0<0)
            lplog(LOG_FATAL_ERROR,L"Illegal reference (10) to element %d in an array with only %d elements! - %d",_P0,count,catchError);
    #endif
    return (content[_P0]);
}

patternElementMatchArray::const_reference patternElementMatchArray::operator[](unsigned int _P0) const
{ LFS
    #ifdef INDEX_CHECK
        if (_P0>=count || _P0<0)
        {
            logCache=0;
            lplog(L"Illegal reference (11) to element %d in an array with only %d elements!",_P0,count);
            if (count==0) throw;
            return content[0];
        }
    #endif
    return (content[_P0]);
}

int patternElementMatchArray::push_back(int oCost,int iCost,unsigned int p,int begin,int end,int elementMatchedSubIndex,
                                        unsigned int patternElement,unsigned int patternElementIndex,int allocationHint)
{ LFS
  if (!count) count++; // make sure no PEMA offset can be zero, because of a limitation in collectTags that uses the negation of PEMAPosition
  count++;
  if (allocated<=count)
  {
    int oldAllocated=allocated;
    if (!allocated) 
			allocated=allocationHint*10;
    else 
		{
			allocated+=(allocationHint*10); // 3000000
			if (allocationHint<1000) allocated+=(allocationHint*50);
			//lplog(L"PEMA memory reallocation %d-%d:%d:%d->%d",begin,end,allocationHint,oldAllocated,allocated);
		}
    content=(tPatternElementMatch *)trealloc(8,content,oldAllocated*sizeof(*content),allocated*sizeof(*content));
  }
  tPatternElementMatch *c=content+count-1;
  c->setPattern(p);
  c->setOCost(oCost);
  c->setIncrementalCost(iCost);
  c->removeWinnerFlag();
  c->begin=begin;
  c->end=end;
  c->setElementAndIndex(patternElement,patternElementIndex);
  c->setElementMatchedSubIndex(elementMatchedSubIndex);
  c->nextByPosition=-1;
  c->nextByPatternEnd=-1;
  c->nextPatternElement=-1;
  c->nextByChildPatternEnd=-1;
  c->origin=-1;
  c->tempCost=0;
  //c->sourcePosition=0; // BPM
  c->cumulativeDeltaCost=0;
  if (begin<MIN_SIGNED_SHORT || begin>MAX_SIGNED_SHORT || end<MIN_SIGNED_SHORT || end>MAX_SIGNED_SHORT || iCost>MAX_SIGNED_SHORT)
    lplog(LOG_FATAL_ERROR,L"elements exceeded maximum values.");
  return count-1;
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
int patternElementMatchArray::push_back_unique(int *firstPosition,unsigned int position,int oCost,int iCost,unsigned int p,int begin,int end,int elementMatchedSubIndex,
                                               unsigned int patternElement,unsigned int patternElementIndex,int allocationHint,bool &newElement,bool POFlag)
{ LFS
  bool isPattern;
  unsigned int referredPattern=-1,endPoint=-1;
  if (isPattern=(elementMatchedSubIndex&matchElement::patternFlag)== matchElement::patternFlag)
  {
    if (PATMASK(elementMatchedSubIndex)>=patterns.size())
    {
      lplog(LOG_ERROR,L"%d:FATAL ERROR:Illegal pattern # reference %d at p=%d begin=%d end=%d elementMatchedSubIndex=%d",
        position,PATMASK(elementMatchedSubIndex),p,begin,end,elementMatchedSubIndex);
      return newElement=false;
    }
    referredPattern=patterns[PATMASK(elementMatchedSubIndex)]->rootPattern;
    endPoint=ENDMASK(elementMatchedSubIndex);
  }
  // set pemaByPatternEnd and nextByPatternEnd
  int *saveFirstPosition=firstPosition;
  bool savePOFlag=POFlag; // for saveFirstPosition
  while (true)
  {
    int nextByPatternEnd=*firstPosition;
    tPatternElementMatch *c=content+nextByPatternEnd;
    if (nextByPatternEnd<0 || c->begin<begin) break;
    if (c->begin==begin && c->getElement()==patternElement &&
      ((isPattern && c->isChildPattern() && patterns[c->getChildPattern()]->rootPattern==referredPattern &&   c->getChildLen()==endPoint) ||
      (!isPattern && c->getChildForm()==elementMatchedSubIndex)))
    {
      if (c->getOCost()>oCost)
      {
#ifdef LOG_PATTERN_COST_CHECK
        if (isPattern)
          lplog(L"%d:%s[%s](%d,%d) %s[%s](%d,%d) element #%d PEMA cost reduced from %d to %d (child pattern)",
                position,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position+begin,position+end,
                patterns[c->getChildPattern()]->name.c_str(),patterns[c->getChildPattern()]->differentiator.c_str(),position,position+c->getChildLen(),
                c-content,c->getOCost(),oCost);
        else
          lplog(L"%d:%s[%s](%d,%d) PEMA cost reduced from %d to %d (child form)",
              position,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position+begin,position+end,
              c->getOCost(),oCost);
#endif
        c->setOCost(oCost);
      }
      if (c->getIncrementalCost()>iCost)
      {
#ifdef LOG_PATTERN_COST_CHECK
        if (isPattern)
          lplog(L"%d:%s[%s](%d,%d) %s[%s](%d,%d) element #%d PEMA ICost reduced from %d to %d (child pattern)",
          position,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position+begin,position+end,
          patterns[c->getChildPattern()]->name.c_str(),patterns[c->getChildPattern()]->differentiator.c_str(),position,position+c->getChildLen(),
          c-content,c->iCost,iCost);
        else
          lplog(L"%d:%s[%s](%d,%d) PEMA ICost reduced from %d to %d (child form)",
          position,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position+begin,position+end,
          c->iCost,iCost);
#endif
        c->setIncrementalCost(iCost);
      }
      patterns[p]->emi++;
      newElement=false;
      return nextByPatternEnd;
    }
    POFlag=true;
    firstPosition=&c->nextByPatternEnd;
  }
  int keepOffset=0,keepFirstOffset=0;
  if (savePOFlag) keepFirstOffset=(int)(saveFirstPosition-((int *)content));
  if (POFlag)     keepOffset=(int)(firstPosition-((int *)content));
  int PEMAOffset=push_back(oCost,iCost,p,begin,end,elementMatchedSubIndex,patternElement,patternElementIndex,allocationHint);
  //  content[PEMAOffset].sourcePosition=position; // for CHECKPEER BPM
  if (savePOFlag) saveFirstPosition=((int *)content)+keepFirstOffset;
  if (POFlag)     firstPosition=((int *)content)+keepOffset;
#ifdef LOG_PATTERN_COST_CHECK
  if (*firstPosition>=0)
    lplog(L"%d:Extended patternEnd %s[%s](%d,%d)->%s[%s](%d,%d) chain link %d->%d",
    position,
    patterns[content[PEMAOffset].getPattern()]->name.c_str(),patterns[content[PEMAOffset].getPattern()]->differentiator.c_str(),
    position+content[PEMAOffset].begin,position+content[PEMAOffset].end,
    patterns[content[*firstPosition].getPattern()]->name.c_str(),patterns[content[*firstPosition].getPattern()]->differentiator.c_str(),
    position+content[*firstPosition].begin,position+content[*firstPosition].end,
    PEMAOffset,*firstPosition);
  else
    lplog(L"%d:Created patternEnd %s[%s](%d,%d) child %s[%s](%d,%d) chain link %d->%d",
    position,
    patterns[content[PEMAOffset].getPattern()]->name.c_str(),patterns[content[PEMAOffset].getPattern()]->differentiator.c_str(),
    position+content[PEMAOffset].begin,position+content[PEMAOffset].end,
    patterns[content[PEMAOffset].getChildPattern()]->name.c_str(),patterns[content[PEMAOffset].getChildPattern()]->differentiator.c_str(),
    position,position+content[PEMAOffset].getChildLen(),
    PEMAOffset,*firstPosition);
#endif
  content[PEMAOffset].nextByPatternEnd=(*firstPosition==-1) ? -PEMAOffset : *firstPosition;
  newElement=true;
  *firstPosition=PEMAOffset;
  if (firstPosition==saveFirstPosition && content[PEMAOffset].nextByPatternEnd!=-PEMAOffset)
  {
    int p2;
    for (p2=*saveFirstPosition; content[p2].nextByPatternEnd>=0; p2=content[p2].nextByPatternEnd);
#ifdef LOG_PATTERN_COST_CHECK
    lplog(L"Changing last position in chain from %d to %d.",content[p2].nextByPatternEnd,-PEMAOffset);
#endif
    content[p2].nextByPatternEnd=-PEMAOffset; // this is to create a circular list
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
  lplog(L"ERROR IN CHAIN %s!",temp);
  */
  return PEMAOffset;
}

int patternElementMatchArray::getNextValidByPosition(int lastPEMAConsolidationIndex,int *wa,int &nextPosition)
{ LFS
  if (nextPosition<lastPEMAConsolidationIndex && nextPosition>=0)
  {
    int fPosition=nextPosition, lastPosition=fPosition;
    while (fPosition<lastPEMAConsolidationIndex && fPosition>=0)
      fPosition=content[lastPosition=fPosition].nextByPosition;
    if (fPosition>=0)
      return getNextValidByPosition(lastPEMAConsolidationIndex,wa,content[lastPosition].nextByPosition);
  }
  while (nextPosition>=lastPEMAConsolidationIndex && wa[nextPosition-lastPEMAConsolidationIndex]==-1)
    nextPosition=content[nextPosition].nextByPosition;
  return nextPosition;
}

void patternElementMatchArray::check(void)
{ LFS
  for (unsigned int I=0; I<count; I++)
    if (content[I].nextByPosition>=(signed)count || content[I].nextByPatternEnd>=(signed)count || content[I].nextByChildPatternEnd>=(signed)count)
      lplog(LOG_FATAL_ERROR,L"FATAL!");
}

void patternElementMatchArray::skipPastPositions(int lastPEMAConsolidationIndex,int *&nextPosition,enum chainType cType)
{ LFS
  // skip positions established with last sentence
  if (*nextPosition<lastPEMAConsolidationIndex && *nextPosition>=0)
  {
    int fPosition=*nextPosition,lastPosition=fPosition;
    while (fPosition<lastPEMAConsolidationIndex && fPosition>=0)
      switch (cType)
      {
        case BY_POSITION:           fPosition=content[lastPosition=fPosition].nextByPosition; break;
        case BY_PATTERN_END:        fPosition=content[lastPosition=fPosition].nextByPatternEnd; break;
        case BY_CHILD_PATTERN_END:  fPosition=content[lastPosition=fPosition].nextByChildPatternEnd; break;
      }
    if (fPosition>=0)
      switch (cType)
      {
        case BY_POSITION:           nextPosition=&content[lastPosition].nextByPosition; break;
        case BY_PATTERN_END:        nextPosition=&content[lastPosition].nextByPatternEnd; break;
        case BY_CHILD_PATTERN_END:  nextPosition=&content[lastPosition].nextByChildPatternEnd; break;
      }
  }
}

void patternElementMatchArray::getNextValidPosition(int lastPEMAConsolidationIndex,int *wa,int *nextPosition,enum chainType cType)
{ LFS
  skipPastPositions(lastPEMAConsolidationIndex,nextPosition,cType);
  while (*nextPosition>=lastPEMAConsolidationIndex && (unsigned int)*nextPosition<count && wa[*nextPosition-lastPEMAConsolidationIndex]==-1)
  {
    switch (cType)
    {
    case BY_POSITION:           *nextPosition=content[*nextPosition].nextByPosition; break;
    case BY_PATTERN_END:        *nextPosition=content[*nextPosition].nextByPatternEnd; break;
    case BY_CHILD_PATTERN_END:  *nextPosition=content[*nextPosition].nextByChildPatternEnd; break;
    }
  }
}

void patternElementMatchArray::translate(int lastPEMAConsolidationIndex,int *wa,int *position,enum chainType cType)
{ LFS
	bool translateLoopedPositions=false;
	if (translateLoopedPositions=*position<0 && *position!=-1)
		*position=-*position;
	skipPastPositions(lastPEMAConsolidationIndex,position,cType);
	if (*position<0 || *position<lastPEMAConsolidationIndex) return;
	if (wa[*position-lastPEMAConsolidationIndex]==-1)
	{
		if (!translateLoopedPositions)
			lplog(LOG_ERROR,L"PEMAIndex %d referenced but not winner.",*position);
		*position=-1;
		return;
	}
	*position=wa[*position-lastPEMAConsolidationIndex];
	if (translateLoopedPositions)
		*position=-*position;
}

void patternElementMatchArray::generateWinnerConsolidationArray(int lastPEMAConsolidationIndex,int *&wa,int &numWinners)
{ LFS
	if ((signed)count-lastPEMAConsolidationIndex<=0) return;
	wa=(int *)tmalloc((count-lastPEMAConsolidationIndex)*sizeof(*wa));
	numWinners=0;
	for (unsigned int J=lastPEMAConsolidationIndex; J<count; J++)
		wa[J-lastPEMAConsolidationIndex]=(content[J].isWinner()) ? lastPEMAConsolidationIndex+numWinners++ : -1;
	for (unsigned int J=lastPEMAConsolidationIndex; J<count; J++)
	{
		if (wa[J-lastPEMAConsolidationIndex]<0) continue;
		getNextValidByPosition(lastPEMAConsolidationIndex,wa,content[J].nextByPosition);
		getNextValidPosition(lastPEMAConsolidationIndex,wa,&content[J].nextByPatternEnd,BY_PATTERN_END);
		getNextValidPosition(lastPEMAConsolidationIndex,wa,&content[J].nextByChildPatternEnd,BY_CHILD_PATTERN_END);
		getNextValidPosition(lastPEMAConsolidationIndex,wa,&content[J].nextPatternElement,BY_PATTERN_END); // this is correct
		getNextValidPosition(lastPEMAConsolidationIndex,wa,&content[J].origin,BY_PATTERN_END);
	}
}

bool patternElementMatchArray::consolidateWinners(int lastPEMAConsolidationIndex,int *wa,int numWinners,sTrace &t)
{ LFS
	if (lastPEMAConsolidationIndex>=(signed)count) return true;
	// consolidate winners
	for (unsigned int I=lastPEMAConsolidationIndex; I<count; I++)
	{
		if (wa[I-lastPEMAConsolidationIndex]>=0)
		{
			patterns[content[I].getPattern()]->incrementUse(content[I].__patternElement,content[I].__patternElementIndex,content[I].getIsPattern());
			translate(lastPEMAConsolidationIndex,wa,&content[I].nextByPosition,BY_POSITION);
			translate(lastPEMAConsolidationIndex,wa,&content[I].nextByPatternEnd,BY_PATTERN_END);
			translate(lastPEMAConsolidationIndex,wa,&content[I].nextByChildPatternEnd,BY_CHILD_PATTERN_END);
			translate(lastPEMAConsolidationIndex,wa,&content[I].nextPatternElement,BY_PATTERN_END);
			translate(lastPEMAConsolidationIndex,wa,&content[I].origin,BY_PATTERN_END);
			if (wa[I-lastPEMAConsolidationIndex]!=I)
				memcpy(content+wa[I-lastPEMAConsolidationIndex],content+I,sizeof(*content));
			content[wa[I-lastPEMAConsolidationIndex]].removeWinnerFlag();
		}
	}
	if (wa) tfree((count-lastPEMAConsolidationIndex)*sizeof(*wa),wa);
	if (t.tracePatternElimination)
		lplog(L"PEMA reduced from %d to count %d",count,numWinners);
	count=lastPEMAConsolidationIndex+numWinners;
	return numWinners>1;
}

int patternElementMatchArray::generatePEMACount(int nextPosition)
{ LFS
  int I=0;
  for (; nextPosition!=-1; I++,nextPosition=content[nextPosition].nextByPosition)
    if (nextPosition>(signed)count)
      lplog(LOG_FATAL_ERROR,L"Incorrect PEMA Position %d",nextPosition);
  return I;
}

int patternElementMatchArray::queryTag(int nextPosition,int tag)
{ LFS
  for (; nextPosition!=-1; nextPosition=content[nextPosition].nextByPosition)
		if (content[nextPosition].hasTag(tag))
		  return nextPosition;
	return -1;
}

bool patternElementMatchArray::ownedByOtherWinningPattern(int nextPosition,int p,int len)
{ LFS
	int rootPattern=patterns[p]->rootPattern;
  for (; nextPosition!=-1; nextPosition=content[nextPosition].nextByPosition)
	{
		if (content[nextPosition].isWinner() &&
			  content[nextPosition].isChildPattern() && 
			  patterns[content[nextPosition].getChildPattern()]->rootPattern==rootPattern && 
				content[nextPosition].getChildLen()==len)
			return true;
	}
	return false;
}

bool patternElementMatchArray::ownedByOtherPattern(int nextPosition,int p,int len)
{ LFS
	int rootPattern=patterns[p]->rootPattern;
  for (; nextPosition!=-1; nextPosition=content[nextPosition].nextByPosition)
	{
		if (content[nextPosition].isChildPattern() && 
			  patterns[content[nextPosition].getChildPattern()]->rootPattern==rootPattern && 
				content[nextPosition].getChildLen()==len)
			return true;
	}
	return false;
}

__int64 patternElementMatchArray::tPatternElementMatch::getRole(__int64 &tagRole)
{ LFS
	// role tags must ONLY be child tags, not tags assigned to the whole pattern
	int tag;
	__int64 childTagRole=0;
	unsigned int tagNumBySet=0;
	tagRole=(patterns[getPattern()]->tags.find(MPLURAL_TAG)!=patterns[getPattern()]->tags.end()) ? MPLURAL_ROLE : 0;
	// a noun could have multiple nouns in it but still be singular - NOUN "O"
	tagRole+=(patterns[getPattern()]->tags.find(MNOUN_TAG)!=patterns[getPattern()]->tags.end()) ? MNOUN_ROLE : 0;
	tagRole+=(patterns[getPattern()]->tags.find(SENTENCE_IN_REL_TAG)!=patterns[getPattern()]->tags.end()) ? SENTENCE_IN_REL_ROLE : 0;
	while ((tag=patterns[getPattern()]->elementHasTagInSet(__patternElement,__patternElementIndex,roleTagSet,tagNumBySet,isChildPattern()))>=0)
	{
		if (tag==HAIL_TAG) childTagRole|=HAIL_ROLE;
		if (tag==SUBJECT_TAG) childTagRole|=SUBJECT_ROLE;
		if (tag==OBJECT_TAG) childTagRole|=OBJECT_ROLE;
		if (tag==SUBOBJECT_TAG) childTagRole|=SUBOBJECT_ROLE;
		if (tag==REOBJECT_TAG) childTagRole|=RE_OBJECT_ROLE;
		if (tag==IOBJECT_TAG) childTagRole|=IOBJECT_ROLE;
		if (tag==PREP_OBJECT_TAG) childTagRole|=PREP_OBJECT_ROLE;
	}
	return childTagRole;
}

wchar_t *patternElementMatchArray::tPatternElementMatch::toText(unsigned int position,wchar_t *temp,vector <WordMatch> &m)
{ LFS
	int len=wsprintf(temp,L"%s[%s](%d,%d) child",
		patterns[getPattern()]->name.c_str(),patterns[getPattern()]->differentiator.c_str(),position+begin,position+end);
	if (isChildPattern())
		len+=wsprintf(temp+len,L" %s[*](%d,%d)",patterns[getChildPattern()]->name.c_str(),position,position+getChildLen());
	else
		len+=wsprintf(temp+len,L" form %s",Forms[m[position].getFormNum(getChildForm())]->shortName.c_str());
	return temp;
}
