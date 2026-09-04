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
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#include "DIYDiskArray.h"
void tagFromSource(cSource &source, vector <lpwstring> &model, int wordCountLimit, JNIEnv *env, bool compare);
void createModelFromSource(cSource &source, vector <lpwstring> &model);
vector <lpwstring> writeModelFile(lpwstring modelPath, unordered_map <lpwstring, int> &wordTagCountsMap, unordered_map <lpwstring, int> &tagTransitionCountsMap, unordered_map <lpwstring, int> &tagCountsMap);
vector <lpwstring> readModelFile(lpwstring modelPath);
void trainModelFromSource(cSource &source, unordered_map <lpwstring, int> &wordTagCountsMap, unordered_map <lpwstring, int> &tagTransitionCountsMap, unordered_map <lpwstring, int> &tagCountsMap);
void initViterbiStartProbabilities(int numWords, lpwstring firstWord, vector<lpwstring> &vocab, vector <lpwstring> &tags,
	vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix,
	DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix, unordered_map <lpwstring, int> &vocabReverseLookup);
void loadModel(vector <lpwstring> &model, unordered_map <lpwstring, int> &wordTagCountsMap, unordered_map <lpwstring, int> &tagTransitionCountsMap, unordered_map <lpwstring, int> &tagCountsMap);
vector<vector<double>> constructTagTransitionProbabilityMatrix(unordered_map <lpwstring, int> &tagTransitionCountsMap, unordered_map <lpwstring, int> &tagCountsMap, vector <lpwstring> &tags);
vector<vector<double>> constructWordTagProbabilityMatrix(unordered_map <lpwstring, int> &wordTagCountsMap, unordered_map <lpwstring, int> &tagCountsMap, vector <lpwstring> &tags, vector<lpwstring> &vocab);
void forwardFromSource(cSource &source, vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix,
	DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix,
	vector <lpwstring> &tags, unordered_map <lpwstring, int> &wordSourceIndexLookup, unordered_map <lpwstring, int> &tagLookup);
void testViterbiFromSource(cSource &source);
int createJavaVM(JavaVM *&vm, JNIEnv *&env);
int parseSentence(cSource &source,JNIEnv *env, lpwstring sentence, lpwstring &parse, bool pcfg,bool lockTable);
int findLPPOSEquivalents(lpwstring sentence, lpwstring &parse, lpwstring originalWord, vector<lpwstring> &posList, int duplicateSkip, bool pcfg);
void destroyJavaVM(JavaVM *vm);
