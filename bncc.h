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
class bncc
{
public:
  int process(cSource &source,int sourceId,wstring id);
  int processSentence(cSource &source,int sourceId,wchar_t *s,int &lastSentenceEnd,int &printLocation,int sentenceNum);
  int processWord(cSource &source,int sourceId, wchar_t * buffer,int tag,int secondTag,int &lastSentenceEnd,int &printLocation,int sentenceNum);
  int findPreferredForm(vector <cWordMatch>::iterator im,int tag,bool optional,const wchar_t *location,int sentenceNum,bool depositPreference,bool reportNotFound);
  bool findMultiplePreferredForm(vector <cWordMatch>::iterator im,int tag,const wchar_t *location,int sentence,int &f,bool reportNotFound);
  int unknownCount;
  bncc(void);
};