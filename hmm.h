void tagFromSource(Source &source, vector <wstring> &model, bool compare);
void createModelFromSource(Source &source, vector <wstring> &model);
vector <wstring> writeModelFile(wstring modelPath, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap);
vector <wstring> readModelFile(wstring modelPath);
void trainModelFromSource(Source &source, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap);
void initViterbiStartProbabilities(int numWords, wstring firstWord, vector<wstring> &vocab, vector <wstring> &tags,
	vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, unordered_map <wstring, int> &vocabLookupVector);
void loadModel(vector <wstring> &model, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap);
vector<vector<double>> constructTagTransitionProbabilityMatrix(unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap, vector <wstring> &tags);
vector<vector<double>> constructWordTagProbabilityMatrix(unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagCountsMap, vector <wstring> &tags, vector<wstring> &vocab);
void forwardFromSource(Source &source, vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, 
	vector <wstring> &tags, unordered_map <wstring, int> &wordSourceIndexLookup, unordered_map <wstring, int> &tagLookup);
void testViterbiFromSource(Source &source);
int createJavaVM(JavaVM *&vm, JNIEnv *&env);
int parseSentence(Source &source,JNIEnv *env, wstring sentence, wstring &parse, bool pcfg);
int findLPPOSEquivalents(JNIEnv *env, wstring sentence, wstring &parse, wstring originalWord, vector<wstring> &posList, int duplicateSkip, bool pcfg);
void destroyJavaVM(JavaVM *vm);