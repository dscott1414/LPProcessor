#ifdef LOG_AGREE_PATTERN_EVALUATION
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
#include "time.h"

bool cPattern::similarSets(set <unsigned int> &tags,set <unsigned int> &tags2)
{
  return tags==tags2;
  /*
  if (tags.size()!=tags2.size()) return false;
  for (unsigned int tag=0; tag<tags.size(); tag++)
    if (find(tags2.begin(),tags2.end(),tags[tag])==tags2.end()) return false;
  return true;
  */
}

bool agreeTagSetOK(vector <tTagLocation> &tagSet)
{
  // exactly one SUBJECT required.
  // exactly one GNOUN or N_AGREE under SUBJECT required.
  // at least one VERB required.
  // if conditional, V_AGREE not required.
  // one or two V_AGREEs under VERB. (not enforced if > 2)
  int nextNoun=-1,nextGNoun=-1,nextVerb=-1,nextSubject=-1,nextConditionalTag=-1,nextFutureTag=-1;
  int whereSubject=findTag(tagSet,L"SUBJECT",nextSubject);
  if (whereSubject<0 || nextSubject>=0) return false;
  int whereNoun=findTagConstrained(tagSet,L"N_AGREE",nextNoun,tagSet[whereSubject]);
  int whereGNoun=findTagConstrained(tagSet,L"GNOUN",nextGNoun,tagSet[whereSubject]);
  if ((whereNoun<0 && whereGNoun<0) || nextGNoun>=0 || nextNoun>=0)
    return false;
  bool atLeastOneVerb=false;
  int start=-1;
  do
  {
    int whereMainVerb=findOneTag(tagSet,L"VERB",start);
    if (whereMainVerb<0)
      return atLeastOneVerb;
    atLeastOneVerb=true;
    int whereVerb=findTagConstrained(tagSet,L"V_AGREE",nextVerb,tagSet[whereMainVerb]);
    bool conditional=findTagConstrained(tagSet,L"conditional",nextConditionalTag,tagSet[whereMainVerb])>=0;
    conditional|=findTagConstrained(tagSet,L"future",nextFutureTag,tagSet[whereMainVerb])>=0;
    if ((!conditional && whereVerb<0) || nextConditionalTag>=0)
      return false;
    start=whereMainVerb;
  }
  while (true);
}

void minimizeTagSet(vector  <tTagLocation> &tagSet)
{
  // eliminate all GNOUNs, and SINGULAR, PLURALs and N_AGREEs not in SUBJECT
  // eliminate all conditionals and V_AGREEs not in VERB, and all but the first V_AGREE in verb
  // the next two rules assume there are never any valid VERBs in SUBJECTs, nor SUBJECTs in VERBs
  // if there is a SUBJECT but no VERB, eliminate all conditionals and V_AGREEs
  // if there is a VERB but no SUBJECT, eliminate all GNOUNs, and SINGULAR, PLURALs and N_AGREEs
  int nextSubject=-1,whereSubject=findTag(tagSet,L"SUBJECT",nextSubject);
  int nextMainVerb=-1,whereMainVerb=findTag(tagSet,L"VERB",nextMainVerb);
  if (nextSubject>=0 || nextMainVerb>=0) return;
  int beginSubject,endSubject,beginMainVerb,endMainVerb;
  if (whereSubject>=0)
  {
    beginSubject=tagSet[whereSubject].sourcePosition;
    endSubject=tagSet[whereSubject].len+beginSubject;
  }
  if (whereMainVerb>=0)
  {
    beginMainVerb=tagSet[whereMainVerb].sourcePosition;
    endMainVerb=tagSet[whereMainVerb].len+beginMainVerb;
  }
  if (whereSubject<0 && whereMainVerb<0)
  {
    bool sawVAgree=false;
    for (unsigned int I=0; I<tagSet.size(); I++)
      if (patternTagStrings[tagSet[I].tag]==L"V_AGREE")
      {
        if (sawVAgree)
          tagSet.erase(tagSet.begin()+I--);
        else
          sawVAgree=true;
      }
    return;
  }
  for (unsigned int I=0; I<tagSet.size(); )
    if (((patternTagStrings[tagSet[I].tag]==L"GNOUN" || patternTagStrings[tagSet[I].tag]==L"N_AGREE" ||
          patternTagStrings[tagSet[I].tag]==L"SINGULAR" || patternTagStrings[tagSet[I].tag]==L"PLURAL") &&
         (whereSubject<0 || tagSet[I].len+tagSet[I].sourcePosition<=(unsigned)beginSubject || tagSet[I].sourcePosition>=(unsigned)endSubject)) ||
        ((patternTagStrings[tagSet[I].tag]==L"V_AGREE" || patternTagStrings[tagSet[I].tag]==L"conditional" || patternTagStrings[tagSet[I].tag]==L"future") &&
         (whereMainVerb<0 || tagSet[I].len+tagSet[I].sourcePosition<=(unsigned)beginMainVerb || tagSet[I].sourcePosition>=(unsigned)endMainVerb)))
      tagSet.erase(tagSet.begin()+I);
    else
      I++;
  bool sawVAgree=false;
  for (unsigned int I=0; I<tagSet.size(); I++)
    if (patternTagStrings[tagSet[I].tag]==L"V_AGREE")
    {
      if (sawVAgree)
        tagSet.erase(tagSet.begin()+I--);
      else
        sawVAgree=true;
    }
}

void cPattern::addGeneratedPatternsRecursively(int p,int originalTagSetSize,bool  blocking,bool repeat,int desiredTagSetNum,vector <unsigned int> futureParentElements,
                           vector <tTagLocation> tagSet,vector <wstring> words,vector < vector <tTagLocation> > &tagSets,vector < vector <wstring> > &wordSets,int  position,int loopCounter)
{
  size_t numDescendantSets=patterns[p]->descendantTagSets.size();
  size_t tagSetSize=tagSet.size(),wordsSize=words.size();
  loopCounter*=numDescendantSets;
  for (unsigned int dts=0; dts<numDescendantSets; dts++,loopCounter--)
  {
    // print out original tagSet
//    ::lplog(L"ORIGINAL");
//    printTagSet(-1,tagSet,words);
    // print out additional tagSet
//    ::lplog(L"ADDITIONAL");
//    printTagSet(dts,patterns[p]->descendantTagSets[dts],patterns[p]->descendantWordSets[dts]);

    if ((loopCounter&255)==0)
      wprintf(L"%08d\r",loopCounter);
    // add all descendants of the pattern to the origin
    tagSet.insert(tagSet.end(),patterns[p]->descendantTagSets[dts].begin(),patterns[p]->descendantTagSets[dts].end());
    words.insert(words.end(),patterns[p]->descendantWordSets[dts].begin(),patterns[p]->descendantWordSets[dts].end());
    // adjust positions and ends
    size_t childSize=patterns[p]->descendantWordSets[dts].size();
    for (unsigned int t=originalTagSetSize; t<tagSetSize; t++)
      tagSet[t].len=childSize;
    for (unsigned int t=tagSetSize; t<tagSet.size(); t++)
      tagSet[t].sourcePosition+=position;
//    ::lplog(L"_FINAL");
//    printTagSet(dts,tagSet,words);
    generateTags(blocking,repeat || (dts>0),desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,childSize+position,loopCounter);
    tagSet.erase(tagSet.begin()+tagSetSize,tagSet.end());
    words.erase(words.begin()+wordsSize,words.end());
  }
}

int cPattern::generateTags(bool blocking,bool repeat,int desiredTagSetNum,vector <unsigned int> futureParentElements,
                           vector <tTagLocation> tagSet,vector <wstring> words,vector < vector <tTagLocation> > &tagSets,vector < vector <wstring> > &wordSets,int  position,int loopCounter)
{
  //extern int overallTime;
  //if (((clock()-overallTime)/CLOCKS_PER_SEC)>600) exit(0);
  unsigned int pattern,patternElement,s;
  int tag;
  while (true)
  {
    if (!futureParentElements.size())
    {
      size_t tsSize;
      if (tsSize=tagSet.size())
      {
        //if (includesAllOfTagSet&((__int64)1<<desiredTagSetNum))
        //{
          minimizeTagSet(tagSet);
          tsSize=tagSet.size();
        //}
        if (tagSets.size()>1000 && agreeTagSetOK(tagSet))
          return 1000;
        vector <tTagLocation>::iterator t=tagSet.begin(),tEnd=tagSet.end(),it;
        unsigned int ts;
        for (ts=0; ts<tagSets.size(); ts++)
        {
          if (tsSize!=tagSets[ts].size()) continue;
          vector <tTagLocation>::iterator t2=tagSets[ts].begin();
          for (it=t; it!=tEnd && it->tag==t2->tag; it++,t2++);
          if (it==tEnd) break;
        }
        if (ts==tagSets.size())
        {
          tagSets.push_back(tagSet);
          wordSets.push_back(words);
        }
      }
      return tagSets.size();
    }
    s=futureParentElements.size()-2;
    pattern=futureParentElements[s];
    patternElement=futureParentElements[s+1];
    if (patternElement<patterns[pattern]->numElements())
      break;
    futureParentElements.erase(futureParentElements.begin()+s,futureParentElements.end());
  }
  futureParentElements[s+1]++; // next pattern element
  unsigned int originalTagSetSize=tagSet.size();
  unsigned int originalWordSetSize=words.size();
  for (unsigned int I=0; I<patterns[pattern]->elements[patternElement]->patternIndexes.size(); I++)
  {
    unsigned int p=patterns[pattern]->elements[patternElement]->patternIndexes[I];
      #ifdef LOG_GENERATE_PATTERNS
      if (!repeat)
      {
        char temp[1024];
        temp[0]=0;
        for (unsigned int K=0; K<futureParentElements.size(); K+=2)
          sprintf(temp+strlen(temp),"%d:%s[%s] ",futureParentElements[K+1]-1,patterns[futureParentElements[K]]->name.c_str(),patterns[futureParentElements[K]]->differentiator.c_str());
        ::lplog(L"Evaluating %s index %d (%s[%s]).",temp,I,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str());
      }
      #endif
      futureParentElements[s+1]=patternElement+1; // next pattern element
      unsigned int beginTag=0;
      while ((tag=patterns[pattern]->elementHasTagInSet(patternElement,I,desiredTagSetNum,beginTag,true))>=0)
      {
        #ifdef LOG_GENERATE_PATTERNS
          if (!repeat) ::lplog(L"  TAG %s FOUND in parent element",patternTagStrings[tag].c_str());
        #endif
        tagSet.push_back(tTagLocation(tag,p,pattern,patternElement,position,position+1/*filled in later*/,-1,true));
      }
      // element blocks all descendants.  Continue with next element.
      if (blocking && patterns[pattern]->stopDescendingTagSearch(patternElement,I,true))
      {
        #ifdef LOG_GENERATE_PATTERNS
          if (!repeat) ::lplog(L"  BLOCKED in parent element");
        #endif
        wchar_t temp[100];
        wsprintf(temp,L"%s[%s]BE#%d",patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),patternElement);
        words.push_back(temp);
        generateTags(blocking,repeat,desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,position+1,loopCounter);
        tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
        words.erase(words.begin()+originalWordSetSize,words.end());
        continue;
      }
      beginTag=0;
      while ((tag=patterns[p]->hasTagInSet(desiredTagSetNum,beginTag))>=0)
      {
        #ifdef LOG_GENERATE_PATTERNS
          if (!repeat) ::lplog(L"  TAG %s FOUND",patternTagStrings[tag].c_str());
        #endif
        tagSet.push_back(tTagLocation(tag,p,pattern,patternElement,position,position+1/*filled in later*/,-1,true));
      }
      // child pattern blocks all descendants.  This is only executed in the first element of the pattern to
      // grab all the tags which are not blocked.
      if (blocking && patterns[p]->blockDescendants)
      {
        #ifdef LOG_GENERATE_PATTERNS
          if (!repeat) ::lplog(L"  BLOCKED in child pattern");
        #endif
        wchar_t temp[100];
        wsprintf(temp,L"%s[%s]B",patterns[p]->name.c_str(),patterns[p]->differentiator.c_str());
        words.push_back(temp);
        generateTags(blocking,repeat,desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,position+1,loopCounter);
      }
      else if (!(patterns[p]->includesOneOfTagSet&((__int64)1<<desiredTagSetNum)))
      {
        #ifdef LOG_GENERATE_PATTERNS
          if (!repeat) ::lplog(L"  child pattern not in tag set %s",desiredTagSets[desiredTagSetNum].name.c_str());
        #endif
        wchar_t temp[100];
        wsprintf(temp,L"%s[%s]NI",patterns[p]->name.c_str(),patterns[p]->differentiator.c_str());
        words.push_back(temp);
        generateTags(blocking,repeat,desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,position+1,loopCounter);
      }
      else
      {
        if (!patterns[p]->descendantsPopulated && !patterns[p]->doubleReferencing)
        {
          #ifdef LOG_GENERATE_PATTERNS
            ::lplog(L"Reference: %s[%s]",patterns[p]->name.c_str(),patterns[p]->differentiator.c_str());
          #endif
          vector <unsigned int> tfPE;
          vector <wstring> tWords;
          tfPE.push_back(p); // pattern
          tfPE.push_back(0);           // next element
          if (!patterns[p]->referencing)
            patterns[p]->referencing=true;
          else
            patterns[p]->doubleReferencing=true;
          vector <tTagLocation> tTagSet;
          generateTags(blocking,repeat,desiredTagSetNum,tfPE,tTagSet,tWords,patterns[p]->descendantTagSets,patterns[p]->descendantWordSets,0,loopCounter);
          if (patterns[p]->doubleReferencing)
            patterns[p]->doubleReferencing=false;
          else
          {
            patterns[p]->referencing=false;
            patterns[p]->descendantsPopulated=true;
          }
          ::lplog(L"Reference: %s[%s] DONE - %d tagSets",patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),patterns[p]->descendantTagSets.size());
          #ifdef LOG_GENERATE_PATTERNS
            for (unsigned int ts=0; ts<patterns[p]->descendantTagSets.size() && ts<40;  ts++)
              printTagSet("G",ts,patterns[p]->descendantTagSets[ts],patterns[p]->descendantWordSets[ts]);
          #endif
        }
        if (patterns[p]->descendantsPopulated)
        {
          addGeneratedPatternsRecursively(p,originalTagSetSize,blocking,repeat,desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,position,loopCounter);
          if (patterns[pattern]->elements[patternElement]->minimum==0)
          {
            tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
            words.erase(words.begin()+originalWordSetSize,words.end());
            generateTags(blocking,false,desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,position,loopCounter);
          }
        }
      }
    tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
    words.erase(words.begin()+originalWordSetSize,words.end());
    }
  for (unsigned int I=0; I<patterns[pattern]->elements[patternElement]->formIndexes.size(); I++)
    {
    unsigned int f=patterns[pattern]->elements[patternElement]->formIndexes[I];
      #ifdef LOG_GENERATE_PATTERNS
        if (!repeat)
        {
          char temp[1024];
          temp[0]=0;
          for (unsigned int K=0; K<futureParentElements.size(); K+=2)
            sprintf(temp+strlen(temp),"%d:%s[%s] ",futureParentElements[K+1]-1,patterns[futureParentElements[K]]->name.c_str(),patterns[futureParentElements[K]]->differentiator.c_str());
          ::lplog(L"Evaluating %s index %d (%s).",temp,I,Forms[f]->shortName.c_str());
        }
      #endif
      if (I && similarSets(patterns[pattern]->elements[patternElement]->formTags[I],
                           patterns[pattern]->elements[patternElement]->formTags[I-1]))
         continue;
      unsigned int beginTag=0;
      while ((tag=patterns[pattern]->elementHasTagInSet(patternElement,I,desiredTagSetNum,beginTag,false))>=0)
      {
        #ifdef LOG_GENERATE_PATTERNS
          if (!repeat)  ::lplog(L"  TAG %s FOUND",patternTagStrings[tag].c_str());
        #endif
        tagSet.push_back(tTagLocation(tag,f,pattern,patternElement,position,1,0,false));
      }
      futureParentElements[s+1]=patternElement+1; // update next element
      words.push_back(Forms[f]->shortName.c_str());
      generateTags(blocking,repeat,desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,position+1,loopCounter);
    tagSet.erase(tagSet.begin()+originalTagSetSize,tagSet.end());
    words.erase(words.begin()+originalWordSetSize,words.end());
  }
  return tagSets.size();
}

void cPattern::evaluateAllTagPatternsForAgreement(unsigned int desiredTagSetNum)
{
  vector <unsigned int> futureParentElements;
  vector <tTagLocation> tagSet;
  vector < vector <tTagLocation> > tagSets;
  vector <wstring> words;
  vector < vector <wstring> > wordSets;
  futureParentElements.push_back(num);
  futureParentElements.push_back(0);
  generateTags(true,false,desiredTagSetNum,futureParentElements,tagSet,words,tagSets,wordSets,0,1);
  ::lplog(L"#TAGSETS=%d",tagSets.size());
  for (unsigned int ts=0; ts<tagSets.size(); ts++)
  {
    if (agreeTagSetOK(tagSets[ts])) continue;
    minimizeTagSet(tagSets[ts]);
    printTagSet(LOG_INFO,L"EA",ts,tagSets[ts],wordSets[ts]);
  }
  ::lplog();
}
#endif