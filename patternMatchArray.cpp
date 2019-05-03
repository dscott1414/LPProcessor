#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "word.h"
#include "profile.h"

patternMatchArray::patternMatchArray()
{ LFS
  count=0;
  allocated=0;
  content=NULL;
};

patternMatchArray::~patternMatchArray()
{ LFS
  if (allocated) tfree(allocated*sizeof(*content),content);
  count=0;
  allocated=0;
  content=NULL;
}

void patternMatchArray::clear(void)
{ LFS
  if (allocated) tfree(allocated*sizeof(*content),content);
  count=0;
  allocated=0;
}

patternMatchArray::patternMatchArray(const patternMatchArray &rhs)
{ LFS
  count=rhs.count;
  allocated=rhs.allocated;
  content=NULL;
  if (allocated)
  {
    content = (tPatternMatch *)tmalloc(allocated*sizeof(*content));
    if (!content)
      lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (1)");
    memcpy(content,rhs.content,count*sizeof(*content));
  }
}

void patternMatchArray::minimize(void)
{ LFS
  int oldAllocated=allocated;
  allocated=count;
  content=(tPatternMatch *)trealloc(2,content,oldAllocated*sizeof(*content),allocated*sizeof(*content));
}

bool patternMatchArray::write(IOHANDLE file)
{ LFS
  _write(file,&count,sizeof(count));
  _write(file,content,count*sizeof(*content));
  return true;
}

bool patternMatchArray::read(char *buffer,int &where,unsigned int limit)
{ LFS
	if (where+sizeof(count)+count*sizeof(*content)>limit) return false;
  if (!copy(count,buffer,where,limit)) return false;
  allocated=count;
  content=(tPatternMatch *)tmalloc(count*sizeof(*content));
  if (!content)
  {
    ::lplog();
    return false;
  }
	if (where+count*sizeof(*content)>limit)
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! PMA %d (20)",limit,where+count*sizeof(*content));
  memcpy(content,buffer+where,count*sizeof(*content));
	where+=count*sizeof(*content);
  return true;
}

bool patternMatchArray::write(void *buffer,int &where,unsigned int limit)
{ LFS
  if (!copy(buffer,count,where,limit)) return false;
	if (where+count*sizeof(*content)>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (16)",limit);
  memcpy(((char *)buffer)+where,content,count*sizeof(*content));
  where+=count*sizeof(*content);
	return true;
}

bool patternMatchArray::operator==(const patternMatchArray other) const
{ LFS
  if (count!=other.count) return false;
  return memcmp(content, other.content,count*sizeof(*content))==0;
}

patternMatchArray& patternMatchArray::operator=(const patternMatchArray &rhs)
{ LFS
  if (allocated) tfree(allocated*sizeof(*content),content);
  count=rhs.count;
  allocated=rhs.allocated;
  content=NULL;
  if (allocated)
  {
    content = (tPatternMatch *)tmalloc(allocated*sizeof(*content));
    if (!content)
      lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (3)");
    memcpy(content,rhs.content,count*sizeof(*content));
  }
  return *this;
}

bool patternMatchArray::operator!=(const patternMatchArray other) const
{ LFS
  if (count!=other.count) return true;
  return memcmp(content, other.content,count*sizeof(*content))!=0;
}

patternMatchArray::reference patternMatchArray::operator[](unsigned int _P0)
{ LFS
  #ifdef INDEX_CHECK
    if (_P0>=count || _P0<0)
    {
      logCache=0;
      lplog(L"Illegal reference (7) to element %d in an array with only %d elements!",_P0,count);
      if (count==0) push_back(0,0,0,0);
      return content[0];
    }
  #endif
  return (content[_P0]);
}

patternMatchArray::const_reference patternMatchArray::operator[](unsigned int _P0) const
{ LFS
  #ifdef INDEX_CHECK
    if (_P0>=count || _P0<0)
    {
      logCache=0;
      lplog(L"Illegal reference (8) to element %d in an array with only %d elements!",_P0,count);
      if (count==0) throw;
      return content[0];
    }
  #endif
  return (content[_P0]);
}

int patternMatchArray::push_back(unsigned int insertionPoint,int pass,short cost,unsigned short p,short end)
{ LFS
  count++;
  if (allocated<=count)
  {
    int oldAllocated=allocated;
    if (!allocated) allocated=5;
    else allocated*=2;
    //allocated=count+1;
    content=(tPatternMatch *)trealloc(4,content,oldAllocated*sizeof(*content),allocated*sizeof(*content));
  }
  if (insertionPoint!=count-1)
    memmove(content+insertionPoint+1,content+insertionPoint,((count-1)-insertionPoint)*sizeof(*content));
  tPatternMatch *c=content+insertionPoint;
  c->setPattern(p);
  c->cost=cost;
  c->resetFlags();
  c->len=end;
  c->pemaByPatternEnd=-1; // first PEMA entry in this source position the PMA array belongs to that has the same pattern and end
  c->pemaByChildPatternEnd=-1; // first PEMA entry in this source position that has a child pattern = pattern and childEnd = end
  c->descendantRelationships=-1;
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
int patternMatchArray::push_back_unique(int pass,short cost,unsigned short p,short end,bool &reduced,bool &pushed)
{ LFS
  pushed=reduced=false;
  if (pushed=!count) return push_back(0,pass,cost,p,end);
  tPatternMatch *c=lower_bound(p,end);
  if ((unsigned)(c-content)<count && c->getPattern()==p && c->len==end)
  {
    if (c->getCost()>cost)
    {
      reduced=true;
      // make sure lowest cost possibility of match is recorded
      #ifdef LOG_PATTERN_COST_CHECK
        lplog(L"%d:%s[%s](%d,%d) PMA cost (will be) reduced from %d to %d",position,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position,position+end,
            c->getCost(),cost);
      #endif
    }
    return (int)(c-content);
  }
#ifdef LOG_OLD_MATCH
  if (pass>=1 || patterns[p]->firstPassForwardReference)
#else
  if (pass>=1)
#endif
  {
    // this pass is predicated on that patterns are always bunched together.
    // Therefore, a pattern using a child pattern that was not already there must be on the second pass (or be using a forward reference).
    // example:
    //   __NOUN[D] is a forward reference.
    //   A parent pattern that matches a NOUN may be before __NOUN[D].  So when __NOUN[D] is matched, it may create a lower cost
    //     parent pattern.  Therefore, set "reduced" to update parents to that possibly lower cost.
    reduced=true; // if second pass, a new match MAY also match PEMA pattern
    // make sure lowest cost possibility of match is recorded
    #ifdef LOG_PATTERN_COST_CHECK
      lplog(L"%d:%s[%s](%d,%d) PMA cost (will be) reduced from %d to %d (2)",position,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),position,position+end,
          MAX_SIGNED_SHORT,cost);
    #endif
    cost=MAX_SIGNED_SHORT; // ensure update with reduceParents
  }
  pushed=true;
  return push_back((int)(c-content),pass,cost,p,end);
}

int patternMatchArray::erase(unsigned int at)
{ LFS
  #ifdef INDEX_CHECK
    if (at>=count || at<0)
    {
      logCache=0;
      lplog(L"Illegal reference (9) to element %d in an array with only %d elements!",at,count);
      if (count==0) throw;
      return count;
    }
  #endif
  memmove(content+at,content+at+1,(count-at-1)*sizeof(content[at]));
  count--;
  return count;
}

int patternMatchArray::erase(void)
{ LFS
  count=0;
  return 0;
}

bool patternMatchArray::findMaxLen(wstring pattern,int &element)
{ LFS
  element=-1;
  int maxLen=-1;
  for (unsigned int I=0; I<count; I++)
    if (patterns[content[I].getPattern()]->name==pattern && content[I].len>maxLen)
    {
      maxLen=content[I].len;
      element=I;
    }
  return element>=0;
}

int patternMatchArray::findMaxLen(void)
{ LFS
  int element=-1,maxLen=-1;
  for (unsigned int I=0; I<count; I++)
    if (content[I].len>maxLen)
    {
      maxLen=content[I].len;
      element=I;
    }
  return element;
}

int patternMatchArray::queryPattern(wstring pattern)
{ LFS
  int maxLen;
  return queryPattern(pattern,maxLen);
}

int patternMatchArray::queryPattern(wstring pattern,int &len)
{ LFS
  int maxLen=-1,element=-1;
  for (unsigned int I=0; I<count; I++)
    if (patterns[content[I].getPattern()]->name==pattern && content[I].len>maxLen)
    {
      maxLen=content[I].len;
      element=I|patternFlag;
    }
  len=maxLen;
  return element;
}

int patternMatchArray::queryAllPattern(wstring pattern,int startAt)
{ LFS
  for (unsigned int I=startAt; I<count; I++)
    if (patterns[content[I].getPattern()]->name==pattern)
      return I;
  return -1;
}

int patternMatchArray::queryMaximumLowestCostPattern(wstring pattern,int &len)
{ LFS
  int maxLen=-1,element=-1,minCost=10000;
  for (unsigned int I=0; I<count; I++)
    if (patterns[content[I].getPattern()]->name==pattern && 
			  (content[I].cost<minCost || (content[I].cost==minCost && content[I].len>maxLen)))
    {
      maxLen=content[I].len;
			minCost=content[I].cost;
      element=I|patternFlag;
    }
  len=maxLen;
  return element;
}

int patternMatchArray::queryPattern(int pattern,int &len)
{ LFS
  int element=-1;
  for (unsigned int I=0; I<count; I++)
    if (content[I].getPattern()==pattern && content[I].len>len)
    {
      len=content[I].len;
      element=I|patternFlag;
    }
  return element;
}

int patternMatchArray::queryTagSet(unsigned int &element,int desiredTagSetNum,int &maxLen)
{ LFS
  unsigned int tagInSet;
  maxLen=-1;
  int tag=-1;
  for (unsigned int I=0; I<count; I++)
    if (content[I].len>=maxLen && patterns[content[I].getPattern()]->tagSetMemberInclusion[desiredTagSetNum])
    {
      if (content[I].len==maxLen && patternTagStrings[tag]==L"NAME") continue; // NAME tags have precedence over NOUN tags
      tagInSet=0;
      tag=patterns[content[I].getPattern()]->hasTagInSet(desiredTagSetNum,tagInSet);
      maxLen=content[I].len;
      element=I|patternFlag;
    }
  return tag;
}


int patternMatchArray::queryPatternWithLen(int pattern,int len)
{ LFS
  tPatternMatch *e;
  if ((e=find(pattern,len))==NULL) return -1;
  return (int)(e-content)|patternFlag;
}

int patternMatchArray::queryPatternWithLen(wstring pattern,int len)
{ LFS
  int element=-1;
  for (unsigned int I=0; I<count; I++)
    if (patterns[content[I].getPattern()]->name==pattern && content[I].len==len)
      element=I|patternFlag;
  return element;
}

int patternMatchArray::queryPattern(int pattern)
{ LFS
  int maxLen=-1,element=-1;
  for (unsigned int I=0; I<count; I++)
    if (content[I].getPattern()==pattern && content[I].len>maxLen)
    {
      maxLen=content[I].len;
      element=I|patternFlag;
    }
  return element;
}

int patternMatchArray::findAgent(int &element,int maximumMaxLen,bool includePronouns)
{ LFS
  element=-1;
  int maxLen=-1;
  for (unsigned int I=0; I<count; I++)
  {
    int p=content[I].getPattern();
    wchar_t diff=patterns[p]->differentiator[0];
    if (patterns[p]->name==L"__NOUN" &&
        (diff==L'2' || (includePronouns && diff==L'C')) &&
        content[I].len>maxLen && content[I].len<=maximumMaxLen)
    {
      maxLen=content[I].len;
      element=I|patternFlag;
    }
  }
  return element;
}

int patternMatchArray::queryPattern(wstring pattern,wstring differentiator)
{ LFS
	int maxLen=-1;
	return queryPattern(pattern,differentiator,maxLen);
}

int patternMatchArray::queryPattern(wstring pattern,wstring differentiator,int &maxLen)
{ LFS
  maxLen=-1;
	int element=-1;
  for (unsigned int I=0; I<count; I++)
    if (patterns[content[I].getPattern()]->name==pattern && patterns[content[I].getPattern()]->differentiator==differentiator &&
        content[I].len>maxLen)
    {
      maxLen=content[I].len;
      element=I|patternFlag;
    }
  return element;
}

int patternMatchArray::queryQuestionFlagPattern()
{ LFS
  int maxLen=-1;
	int element=-1;
  for (unsigned int I=0; I<count; I++)
    if (patterns[content[I].getPattern()]->questionFlag && content[I].len>maxLen)
    {
      maxLen=content[I].len;
      element=I|patternFlag;
    }
  return element;
}

int patternMatchArray::getNextPosition(int w)
{ LFS
  int minPatternMatch=1<<31;
  for (unsigned int I=0; I<count; I++)
    if (content[I].len<minPatternMatch)
      minPatternMatch=content[I].len;
  if (minPatternMatch!=(1<<31) && minPatternMatch>w)
    return minPatternMatch;
  return w+1;
}

int compare( patternMatchArray::tPatternMatch *pm1, patternMatchArray::tPatternMatch *pm2 )
{ DLFS
  //if (t.tracePatternElimination)
  //  lplog(L"    PMA comparing p=%d end=%d to p=%d end=%d",pm1->pattern,pm1->end,pm2->getPattern(),pm2->end);
  if (pm1->getPattern()<pm2->getPattern()) return -1;
  if (pm1->getPattern()>pm2->getPattern()) return 1;
  if (pm1->len<pm2->len) return -1;
  if (pm1->len>pm2->len) return 1;
  return 0;
}

patternMatchArray::tPatternMatch *patternMatchArray::find(unsigned int p,short len)
{ LFS
  tPatternMatch key;
  key.setPattern(p);
  key.len=len;
  //if (t.tracePatternElimination)
  //  lplog(L"    PMA searching for p=%d end=%d",p,end);
  return (tPatternMatch *)bsearch(&key,content,count,sizeof(*content),(int (*)(const void*, const void*))compare);
  //if (!result) return NULL;
  //while (result->pattern==p && result->end==end) result--;
  //return result;
}

patternMatchArray::tPatternMatch *patternMatchArray::lower_bound(unsigned int p,short end)
{ LFS
  int len = count,half;
  tPatternMatch *middle,*first=content;

  while (len > 0)
  {
    half = len >> 1;
    middle = first+half;
    if (p>middle->getPattern() || (p==middle->getPattern() && end>middle->len))
    {
      first = middle+1;
      len = len - half - 1;
    }
    else
      len = half;
  }
  return first;
}

bool patternMatchArray::consolidateWinners(int lastPEMAConsolidationIndex,patternElementMatchArray &pema,int *wa,int position,int &maxMatch,sTrace &t)
{ LFS
  int target=0,numWinners=0;
	maxMatch=0;
  for (unsigned int J=0; J<count; J++)
  {
    if (content[J].isWinner())
    {
      if (target!=J)
        memcpy(content+target,content+J,sizeof(*content));
			maxMatch=max(maxMatch,content[target].len);
      content[target].removeWinnerFlag();
      //if (content[target].maxWinner(lowestAverageCost,maxLACMatch))
      numWinners++;
      pema.getNextValidPosition(lastPEMAConsolidationIndex,wa,&content[target].pemaByPatternEnd,patternElementMatchArray::BY_PATTERN_END);
      pema.translate(lastPEMAConsolidationIndex,wa,&content[target].pemaByPatternEnd,patternElementMatchArray::BY_PATTERN_END);
      pema.getNextValidPosition(lastPEMAConsolidationIndex,wa,&content[target].pemaByChildPatternEnd,patternElementMatchArray::BY_CHILD_PATTERN_END);
      pema.translate(lastPEMAConsolidationIndex,wa,&content[target].pemaByChildPatternEnd,patternElementMatchArray::BY_CHILD_PATTERN_END);
      target++;
    }
  }
  if (t.tracePatternElimination)
    lplog(L"%d:PMA count reduced from %d to %d",position,count,target);
  count=target;
  return numWinners>1;
}