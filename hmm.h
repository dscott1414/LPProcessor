/*
	hmm.h - first-party HMM / Viterbi POS-tagger declarations (not the KenLM vendor tree).

	Overview:
		Public surface for hmm.cpp: train a tag-transition / word-tag model from a
		parsed cSource, run Viterbi, and optionally compare against Stanford PCFG
		via JNI.

	Pipeline position:
		Optional post-parse helper.  specials_main.cpp step 8 concatenates many
		sources then calls testViterbiFromSource().  Not on the main.cpp parse path.

	Notes / gotchas:
		- tagFromSource / initViterbiStartProbabilities / forwardFromSource /
		  findLPPOSEquivalents used to be declared here with a different arity than
		  their hmm.cpp definitions (stale declarations that no caller outside
		  hmm.cpp ever instantiated); the declarations below now match hmm.cpp.
		- createJavaVM hard-codes F:\lp\Stanford\... in hmm.cpp.
*/
#include "DIYDiskArray.h"
void tagFromSource(cSource &source, vector <wstring> &model, int wordCountLimit, JNIEnv *env, bool compare);
void createModelFromSource(cSource &source, vector <wstring> &model);
vector <wstring> writeModelFile(wstring modelPath, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap);
vector <wstring> readModelFile(wstring modelPath);
void trainModelFromSource(cSource &source, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap);
void initViterbiStartProbabilities(int numWords, wstring firstWord, vector<wstring> &vocab, vector <wstring> &tags,
	vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix,
	DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix, unordered_map <wstring, int> &vocabReverseLookup);
void loadModel(vector <wstring> &model, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap);
vector<vector<double>> constructTagTransitionProbabilityMatrix(unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap, vector <wstring> &tags);
vector<vector<double>> constructWordTagProbabilityMatrix(unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagCountsMap, vector <wstring> &tags, vector<wstring> &vocab);
void forwardFromSource(cSource &source, vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix,
	DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix,
	vector <wstring> &tags, unordered_map <wstring, int> &wordSourceIndexLookup, unordered_map <wstring, int> &tagLookup);
void testViterbiFromSource(cSource &source);
int createJavaVM(JavaVM *&vm, JNIEnv *&env);
int parseSentence(cSource &source,JNIEnv *env, wstring sentence, wstring &parse, bool pcfg,bool lockTable);
int findLPPOSEquivalents(wstring sentence, wstring &parse, wstring originalWord, vector<wstring> &posList, int duplicateSkip, bool pcfg);
void destroyJavaVM(JavaVM *vm);
