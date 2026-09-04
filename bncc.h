/*
	bncc.h - British National Corpus preference-tag applicator (bncc)

	Overview:
		Declares the helper that walks a tokenized cSource and, for each BNC-tagged
		word, calls findPreferredForm / findMultiplePreferredForm to deposit a usage
		preference on the matching cWordMatch.  Implementation lives in a separate
		BNC translation unit (not in this assignment).

	Pipeline position:
		Optional corpus-statistics pass used to seed form-usage costs (the BNC
		preferences consulted later by eliminateLoserPatterns).

	Key entry points:
		- process() - drive one sourceId.
		- processSentence() / processWord() - per-sentence / per-token.
		- findPreferredForm() / findMultiplePreferredForm() - map a BNC tag onto a form.

	Key data structures / globals:
		- unknownCount - words whose BNC tag could not be matched to a form.
*/
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
class bncc
{
public:
  int process(cSource &source,int sourceId,lpwstring id);
  int processSentence(cSource &source,int sourceId,lpchar_t *s,int &lastSentenceEnd,int &printLocation,int sentenceNum);
  int processWord(cSource &source,int sourceId, lpchar_t * buffer,int tag,int secondTag,int &lastSentenceEnd,int &printLocation,int sentenceNum);
  int findPreferredForm(vector <cWordMatch>::iterator im,int tag,bool optional,const lpchar_t *location,int sentenceNum,bool depositPreference,bool reportNotFound);
  bool findMultiplePreferredForm(vector <cWordMatch>::iterator im,int tag,const lpchar_t *location,int sentence,int &f,bool reportNotFound);
  int unknownCount;
  bncc(void);
};