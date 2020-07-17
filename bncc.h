class bncc
{
public:
  int process(cSource &source,int sourceId,wstring id);
  int processSentence(cSource &source,int sourceId,wchar_t *s,int &lastSentenceEnd,int &printLocation,int sentenceNum);
  int processWord(cSource &source,int sourceId,wchar_t *buffer,int tag,int secondTag,int &lastSentenceEnd,int &printLocation,int sentenceNum);
  int findPreferredForm(vector <WordMatch>::iterator im,int tag,bool optional,const wchar_t *location,int sentenceNum,bool depositPreference,bool reportNotFound);
  bool findMultiplePreferredForm(vector <WordMatch>::iterator im,int tag,const wchar_t *location,int sentence,int &f,bool reportNotFound);
  int unknownCount;
  bncc(void);
};