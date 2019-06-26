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

const unsigned int isPattern=(1<<31);
const unsigned int minimumPatternLengthForAnalysis=3,maximumPatternLengthForAnalysis=6;

class patternCount
{
public:
	unsigned int count;
	unsigned int element;
	int parent;

	vector <int> f;
	vector <int> p;
	patternCount(int inElement,int inParent,int inCount)
	{
		element=inElement;
		parent=inParent;
		count=inCount;
	};
	patternCount::~patternCount() 
	{
	}
	patternCount::patternCount(const patternCount &rhs)
	{
			count=rhs.count;
			element=rhs.element;
			f=rhs.f;
			p=rhs.p;
			parent=rhs.parent;
	} 	
};

vector <patternCount> patternCountTree[maximumPatternLengthForAnalysis];

// accumulate new patterns

// pc belongs to tree level [patternLength].
// the elements pc points to belong to the level [patternLength+1]
void Source::accumulateNewPattern(unsigned int w,int pc,int patternLength)
{
	if (w>=m.size()) return;
	WordMatch *wm=&m[w];
	if (patternLength+1>=maximumPatternLengthForAnalysis || wm->word==Words.sectionWord) 
		return;
	int patternsChosen=false;
	int size=wm->PEMACount;
  for (int p=wm->beginPEMAPosition; p!=wm->endPEMAPosition; pema[p].nextByPosition)
	{
		//wprintf(L"%d==%d? - %d ",wm->pema[p].begin,w,(int)wm->pema[p].getPattern());
		// if pattern doesn't begin on this element, or is not a winner pattern, ignore.  
		if (pema[p].begin || pema[p].end!=wm->maxLACMatch) continue;
		patternsChosen=true;
		vector <int>::iterator ip=patternCountTree[patternLength][pc].p.begin(),ipend=patternCountTree[patternLength][pc].p.end();
		for (; ip!=ipend; ip++)
			if (patternCountTree[patternLength+1][(*ip)].element==(pema[p].getPattern()|isPattern)) break;
		// not found - create new element
		if (ip==ipend)
		{
			patternCount nextpc(pema[p].getPattern()|isPattern,pc,1);
			size_t size=patternCountTree[patternLength+1].size();
			patternCountTree[patternLength+1].push_back(nextpc);
			patternCountTree[patternLength][pc].p.push_back(size);
			accumulateNewPattern(pema[p].end+w,size,patternLength+1);
		}
		else
		{
			// found - increment count
			patternCountTree[patternLength+1][(*ip)].count++;
			accumulateNewPattern(pema[p].end+w,*ip,patternLength+1);
		}
	}
	// if this position is set to be ignored, skip this position.
	if (m[w].word->second.isIgnore())
	{
		accumulateNewPattern(w+1,pc,patternLength);
		return;
	}
	size=m[w].formsSize();
	for (int f=0; f<size; f++)
	{
		// if not topLevelSeparator, ignore.
		int form=m[w].getFormNum(f);
        if (!Forms[form]->isTopLevel || Forms[form]->name==L"!" || Forms[form]->name==L"?" || Forms[form]->name==L".") continue;
		vector <int>::iterator ip=patternCountTree[patternLength][pc].f.begin(),ipend=patternCountTree[patternLength][pc].f.end();
		for (; ip!=ipend; ip++)
			if (patternCountTree[patternLength+1][(*ip)].element==form) 
				break;
		if (ip==ipend)
		{
			patternCount nextpc(form,pc,1);
			size_t size=patternCountTree[patternLength+1].size();
			patternCountTree[patternLength+1].push_back(nextpc);
			patternCountTree[patternLength][pc].f.push_back(size);
			accumulateNewPattern(w+1,size,patternLength+1);
		}
		else
		{
			patternCountTree[patternLength+1][(*ip)].count++;
			accumulateNewPattern(w+1,*ip,patternLength+1);
		}
	}
}

bool Source::primitiveMatch(vector<int> elements,int w,wstring &patternMatch)
{
	vector <unsigned int> endPoints;
	vector <wstring> wordMatches;
	unsigned int begin=0;
	endPoints.push_back(w);
	wordMatches.push_back(L"");
	for (unsigned int e=0; e<elements.size(); e++)
	{
		size_t end=endPoints.size();
		for (unsigned int ep=begin; ep<end; ep++)
		{
			if (endPoints[ep]>=m.size())
			{
				lplog(L"primitiveMatch accessed illegal endPoint %d at offset %d (w=%d).",endPoints[ep],ep,w);
				continue;
			}
			unsigned int endPoint=endPoints[ep];
			vector <WordMatch>::iterator im=m.begin()+endPoint,imend=m.end();
			wstring originalWordMatch=wordMatches[ep];
			while (im->word->second.isIgnore() && im!=imend) 
			{
				originalWordMatch+=L" "+im->word->first+L" ";
				endPoint++;
				im++;
			}
			if (elements[e]&isPattern)
			{
				for (unsigned int pattern=im->beginPEMAPosition; pattern!=im->endPEMAPosition; pattern=pema[pattern].nextByPosition)
					if (pema[pattern].getPattern()==(elements[e]&~isPattern) &&
							!pema[pattern].begin)
					{
						endPoints.push_back(pema[pattern].end+endPoint);
						wstring wordMatch=originalWordMatch;
						int p=elements[e]&~isPattern;
						wordMatch+=L" "+patterns[p]->name+L"["+patterns[p]->differentiator+L"] (";
						for (unsigned int I=endPoint; I<pema[pattern].end+endPoint; I++)
							wordMatch+=m[I].word->first+L" ";
						wordMatch[wordMatch.length()-1]=L')';
						wordMatches.push_back(wordMatch);
					}
			}
			else if (im->queryForm(elements[e])!=-1)
			{
				if (endPoint+1<m.size() || e==elements.size()-1)  // make sure not to run off the end
					endPoints.push_back(endPoint+1);
				wordMatches.push_back(originalWordMatch+L" "+Forms[elements[e]]->name+L" ("+im->word->first+L")");
			}
		}
		if (end==endPoints.size()) return false;
		patternMatch=wordMatches[wordMatches.size()-1];
		begin=end;
	}
	return true;
}

bool patternCountCompare(const patternCount &lhs, const patternCount &rhs)
{
	return lhs.count>rhs.count;
}

bool Source::printPattern(int patternLength,int pc)
{
		wstring pattern;
		vector <int> elements;
		int pl=patternLength,c=pc;
		while (patternCountTree[pl][c].parent!=-1)
		{
			int element;
			elements.insert(elements.begin(),element=patternCountTree[pl][c].element);
			if (element&isPattern)
			{
				wchar_t tmp[10];
				_itow(element&=~(isPattern),tmp,10);
				pattern=patterns[element]->name + L"[" + wstring(tmp) + L"] " + pattern;
			}
			else
				pattern=Forms[element]->name + L" " + pattern;
			c=patternCountTree[pl][c].parent;
			pl--;
		}
		lplog(L"______________________________________________\n%d:%s\n______________________________________________",
			patternCountTree[patternLength][pc].count,pattern.c_str());
		unsigned int match=0;
		for (unsigned int w=0; match<10 && w<m.size(); )
		{
			wstring patternMatch;
			if (primitiveMatch(elements,w,patternMatch)) 
			{
				lplog(L"%d:%s",w,patternMatch.c_str());
				match++;
			}
			w=m[w].pma.getNextPosition(w);
		}
		return match>0;
}

bool Source::printPatterns(int patternLength,unsigned int topLimit)
{
	if (patternCountTree[patternLength].size()<topLimit) topLimit=patternCountTree[patternLength].size();
	if (!topLimit) return false;
	partial_sort(patternCountTree[patternLength].begin(),patternCountTree[patternLength].begin()+topLimit,
							 patternCountTree[patternLength].end(),patternCountCompare);
	lplog(L"\n\n\nPatterns of length %d:\n",patternLength);
	for (unsigned int I=0; I<topLimit; I++)
		printPattern(patternLength,I);
	return true;
}

void Source::printAccumulatedPatterns(void)
{
	wprintf(L"PROGRESS: 0%% patterns printed with %d seconds elapsed (%I64d bytes) \r",clock()/CLOCKS_PER_SEC,memoryAllocated);
	for (int p=maximumPatternLengthForAnalysis-1; p>=minimumPatternLengthForAnalysis; p--)
	{
		printPatterns(p,10);
		wprintf(L"PROGRESS: %d%% patterns printed with %04d seconds elapsed (%I64d bytes) \r",
    		(maximumPatternLengthForAnalysis-p)*100/(maximumPatternLengthForAnalysis-minimumPatternLengthForAnalysis),clock()/CLOCKS_PER_SEC,memoryAllocated);
	}
}

void Source::accumulateNewPatterns(void)
{
	for (int I=2; I<maximumPatternLengthForAnalysis; I++) patternCountTree[I].reserve(50000);
	if (!patternCountTree[0].size()) patternCountTree[0].push_back(patternCount(0,-1,0));
	int lastProgressPercent=-1,where;
	for (unsigned int w=0; w<m.size(); w++)
	{
		if ((where=w*100/m.size())>lastProgressPercent)
		{
			wprintf(L"PROGRESS: %d%% patterns analyzed with %04d seconds elapsed (%I64d bytes) \r",where,clock()/CLOCKS_PER_SEC,memoryAllocated);
			lastProgressPercent=where;
		}
		accumulateNewPattern(w,0,0);
		//w=m[w].pma.getNextPosition(w);
	}
	wprintf(L"PROGRESS: 100%% patterns analyzed with %04d seconds elapsed (%I64d bytes) \n",clock()/CLOCKS_PER_SEC,memoryAllocated);
	lplog(L"pattern length:# tree nodes");
	for (int I=0; I<maximumPatternLengthForAnalysis; I++) 
		lplog(L"%d:%d",I,patternCountTree[I].size());
	for (int I=0; I<maximumPatternLengthForAnalysis; I++) patternCountTree[I].clear();
  printAccumulatedPatterns();
}

