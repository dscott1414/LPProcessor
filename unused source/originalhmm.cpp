/*
	originalhmm.cpp - C++ port of a WSJ HMM POS-tagger (hmm.py / settings.py)

	Overview:
		Train a first-order HMM on WSJ_02-21.pos (emission/transition/
		context counts, additive smoothing alpha=0.001), then Viterbi-tag
		WSJ_24 (dev) or WSJ_23 (test). Unknown tokens are binned by
		digit/punct/case/suffix into --unk_*-- classes. Writes
		hmm_model.txt / hmm_vocab.txt and tagged output.

	Pipeline position:
		Standalone POS experiment; not wired into the LP pattern parser.
		The live codebase uses a different HMM (hmm.h / KenLM) if at all.

	Key entry points:
		- generate_vocab() / assign_unk() / train_model() / load_model()
		- constructTagTransitionProbabilityMatrix() / constructWordTagProbabilityMatrix()
		- preprocess() / initViterbi() / initViterbi2() / forward() / backward()
		- decode() / tag() / decode_seq() / runTest()

	Dependencies:
		Relative paths ../HMM2CTest/WSJ/... and data/. fopen without
		NULL checks. TAGS_WSJ[] is unused (tags come from context keys).

	Notes / gotchas:
		fopen results are never null-checked. line[strlen(line)-1]=0
		assumes every fgets line has a newline (empty last line UB).
		cout << s on every training token. Viterbi is O(T*K^2) over the
		whole file as one sequence (newlines become --n-- / --s--).
		boolean (not bool) in tag/decode_seq is an MSVC/Windows typedef.
*/
#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "math.h"
#include "sys/stat.h"
#include <sys/types.h>
#include "profile.h"
#include <sstream>
#include <iostream>
#include <vector>
#include "hmm.h"

using namespace std;
// this corresponds to settings.py BEGIN
char *TRAIN = "../HMM2CTest/WSJ/WSJ_02-21.pos";

char *DEV_WORDS = "../HMM2CTest/WSJ/WSJ_24.words";
char *DEV_POS = "../HMM2CTest/WSJ/WSJ_24.pos";
char *DEV_OUT = "../HMM2CTest/output/wsj_24.pos";

char *TEST_WORDS = "../HMM2CTest/WSJ/WSJ_23.words";
char *TEST_POS = "../HMM2CTest/WSJ/WSJ_23.pos";
char *TEST_OUT = "../HMM2CTest/output/wsj_23.pos";

char *MODEL = "../HMM2CTest/data/hmm_model.txt";
char *VOCAB = "../HMM2CTest/data/hmm_vocab.txt";
char *UNK_TOKS = "../HMM2CTest/data/unk_toks.txt";

char *CONFUSION_MATRIX = "docs/confusion_matrix.csv";

char *TAGS_WSJ[] = {
	"#",
		"$",
		"''",
		"(",
		")",
		",",
		".",
		":",
		"CC",
		"CD",
		"DT",
		"EX",
		"FW",
		"IN",
		"JJ",
		"JJR",
		"JJS",
		"LS",
		"MD",
		"NN",
		"NNP",
		"NNPS",
		"NNS",
		"PDT",
		"POS",
		"PRP",
		"PRP$",
		"RB",
		"RBR",
		"RBS",
		"RP",
		"SYM",
		"TO",
		"UH",
		"VB",
		"VBD",
		"VBG",
		"VBN",
		"VBP",
		"VBZ",
		"WDT",
		"WP",
		"WP$",
		"WRB",
		"``",
};
// this corresponds to settings.py END

// this corresponds to hmm.py BEGIN
// punctuation in python corresponds to these lp word classes (initalizeDictionary): punctuation, brackets, quotes, dash, and top level forms.

// punct = set(string.punctuation)  
char punctuation[] = { ',', '+', '\'', '^', '}', '#', ';', '[', '"', '%', '(', '|', '-', '=', ']', ':', '_', '{', '>', '&', '.', '@', '?', '!', '/', '~', '$', '*', '<', '`', ')', '\\' };

// Morphology rules used to assign unknown word tokens
char *noun_suffix[] = { "action", "age", "ance", "cy", "dom", "ee", "ence", "er", "hood", "ion", "ism", "ist", "ity", "ling", "ment", "ness", "or", "ry", "scape", "ship", "ty",0 };
char *verb_suffix[] = { "ate", "ify", "ise", "ize",0 };
char *adj_suffix[] = { "able", "ese", "ful", "i", "ian", "ible", "ic", "ish", "ive", "less", "ly", "ous",0 };
char *adv_suffix[] = { "ward", "wards", "wise",0 };

// Additive smoothing parameter
double alpha = 0.001;

// called by runTest
// Count tokens in the tab-separated training file, keep those with
// frequency >= min_cnt, append UNK_TOKS, sort, write VOCAB, return the
// vector. fopen is unchecked. Drops the last char of every line as NL.
vector <string> generate_vocab(int min_cnt = 2, char *training_filepath = TRAIN)
{
	//Generate vocabulary
	map <string, int> vocabAll;

	FILE *train_fp = fopen(training_filepath, "r");

	char line[100 + 1];
	while (fgets(line, sizeof(line), train_fp) != NULL)
	{
		line[strlen(line) - 1] = 0;
		vector<string> strings;
		istringstream f(line);
		string s;
		while (getline(f, s, '\t'))
		{
			cout << s << endl;
			strings.push_back(s);
		}
		// Ignore empty lines
		if (strings.empty() || strings.size() != 2)
			continue;
		string tok = strings[0];
		string tag = strings[1];
		vocabAll[tok] += 1;
	}
	fclose(train_fp);

	vector <string> vocabvector;
	// Remove words appearing only once
	for (map <string, int>::iterator vocabIter = vocabAll.begin(), vocabEnd = vocabAll.end(); vocabIter != vocabEnd; vocabIter++)
		if (vocabIter->second >= min_cnt)
			vocabvector.push_back(vocabIter->first);

	// Add newline / unknown word tokens
	FILE *unknown_fp = fopen(UNK_TOKS, "r");
	while (fgets(line, sizeof(line), unknown_fp) != NULL)
	{
		line[strlen(line) - 1] = 0;
		vocabvector.push_back(line);
	}
	fclose(unknown_fp);

	// Sort 
	sort(vocabvector.begin(), vocabvector.end());

	FILE *vocab_fp = fopen(VOCAB, "w");
	for (vector <string>::iterator vocabIter = vocabvector.begin(), vocabEnd = vocabvector.end(); vocabIter != vocabEnd; vocabIter++)
		fprintf(vocab_fp, "%s\n", vocabIter->c_str());
	fclose(vocab_fp);

	return vocabvector;
}

// Map an OOV token to --unk_digit-- / --unk_punct-- / --unk_upper-- /
// --unk_{noun,verb,adj,adv}-- (suffix tables) / --unk--. First match wins.
// Returns a pointer to a string literal.
char *assign_unk(string tok)
{
	// Assign unknown word tokens
	// Digits
	for (string::iterator chi = tok.begin(), chiEnd = tok.end(); chi != chiEnd; chi++)
		if (isdigit(*chi))
			return "--unk_digit--";
	for (string::iterator chi = tok.begin(), chiEnd = tok.end(); chi != chiEnd; chi++)
		if (ispunct(*chi))
			return "--unk_punct--";
	for (string::iterator chi = tok.begin(), chiEnd = tok.end(); chi != chiEnd; chi++)
		if (isupper(*chi))
			return "--unk_upper--";

	for (int I = 0; noun_suffix[I]; I++)
		if (tok.length() >= strlen(noun_suffix[I]) && (0 == tok.compare(tok.length() - strlen(noun_suffix[I]), strlen(noun_suffix[I]), noun_suffix[I])))
			return "--unk_noun--";
	for (int I = 0; verb_suffix[I]; I++)
		if (tok.length() >= strlen(verb_suffix[I]) && (0 == tok.compare(tok.length() - strlen(verb_suffix[I]), strlen(verb_suffix[I]), verb_suffix[I])))
			return "--unk_verb--";
	for (int I = 0; adj_suffix[I]; I++)
		if (tok.length() >= strlen(adj_suffix[I]) && (0 == tok.compare(tok.length() - strlen(adj_suffix[I]), strlen(adj_suffix[I]), adj_suffix[I])))
			return "--unk_adj--";
	for (int I = 0; adv_suffix[I]; I++)
		if (tok.length() >= strlen(adv_suffix[I]) && (0 == tok.compare(tok.length() - strlen(adv_suffix[I]), strlen(adv_suffix[I]), adv_suffix[I])))
			return "--unk_adv--";
	return "--unk--";
}

//called by runTest
// One pass over training_filepath: count transitions (prev tag -> tag),
// emissions (tag -> word or unk class), and context (tag). Blank lines
// are --n-- / --s--. Writes MODEL and returns the same lines as a vector.
vector <string> train_model(vector<string> vocabvector, char *training_filepath)
{
	// Train part - of - speech(POS) tagger model
	set<string>	vocab(std::begin(vocabvector), std::end(vocabvector));
	map <string, int> emiss;
	map <string, int> trans;
	map <string, int> context;

	FILE *train_fp = fopen(training_filepath, "r");
	// Start state
	string prev = "--s--";
	char line[100 + 1];
	while (fgets(line, sizeof(line), train_fp) != NULL)
	{
		line[strlen(line) - 1] = 0;
		vector<string> strings;
		istringstream f(line);
		string s;
		while (getline(f, s, '\t'))
		{
			cout << s << endl;
			strings.push_back(s);
		}
		string word, tag;
		// End of sentence
		if (strings.empty() || strings.size() != 2)
		{
			word = "--n--";
			tag = "--s--";
		}
		else
		{
			word = strings[0];
			tag = strings[1];
			// Handle unknown words
			if (vocab.find(word) == vocab.end())
				word = assign_unk(word);
		}
		trans[prev + " " + tag] += 1;
		emiss[tag + " " + word] += 1;
		context[tag] += 1;
		prev = tag;
	}
	fclose(train_fp);

	vector <string> model;

	FILE *out_fp = fopen(MODEL, "w");

	// Write transition counts
	for (map <string, int>::iterator ti = trans.begin(), tiEnd = trans.end(); ti != tiEnd; ti++)
	{
		string tline = "T " + ti->first + " " + std::to_string(ti->second);
		model.push_back(tline);
		fprintf(out_fp, "%s\n", tline.c_str());
	}
	// Write emission counts
	for (map <string, int>::iterator ei = emiss.begin(), eiEnd = emiss.end(); ei != eiEnd; ei++)
	{
		string eline = "E " + ei->first + " " + std::to_string(ei->second);
		model.push_back(eline);
		fprintf(out_fp, "%s\n", eline.c_str());
	}
	// Write context map
	for (map <string, int>::iterator ci = context.begin(), ciEnd = context.end(); ci != ciEnd; ci++)
	{
		string cline = "C " + ci->first + " " + std::to_string(ci->second);
		model.push_back(cline);
		fprintf(out_fp, "%s\n", cline.c_str());
	}
	fclose(out_fp);
	return model;
}

// Load model - called in decode_seq
// Parse model lines: "C tag count", "T tag next count", "E tag word count"
// into the three maps. Malformed C lines are printed; T/E parse failures
// are not checked.
void load_model(vector <string> &model, map <string, int> &emiss, map <string, int> &trans, map <string, int> &context)
{
	for (vector <string>::iterator mi = model.begin(), miEnd = model.end(); mi != miEnd; mi++)
	{
		string type, tag, x;
		int count;
		std::stringstream convertor(*mi);
		if (mi->at(0) == 'C')
		{
			convertor >> type >> tag >> count;
			if (convertor.fail() == true)
				printf("failed to read in context data %s", mi->c_str());
			else
				context[tag] = int(count);
			continue;
		}
		convertor >> type >> tag >> x >> count;
		if (mi->at(0) == 'T')
			trans[tag + " " + x] = int(count);
		else
			emiss[tag + " " + x] = int(count);
	}
}

//				Generate transition matrix tagTransitionProbabilityMatrix of size numTags x numTags
//				[A_ij stores the probability of transiting from state s_i to state s_j]
// called by decode_seq
// KxK matrix A[i][j] = P(tags[j]|tags[i]) with add-alpha smoothing
// (alpha / (context[prev] + alpha*K)). Missing transitions count as 0.
vector<vector<double>> constructTagTransitionProbabilityMatrix(map <string, int> &trans, map <string, int> &context, vector <string> &tags)
{
	int K = tags.size();
	vector <vector<double>> A(K, vector(K, (double)0));

	for (int I = 0; I < K; I++)
	{
		for (int J = 0; J < K; J++)
		{
			string prevTag = tags[I];
			string tag = tags[J];
			// Compute smoothed transition probability
			int count = 0;
			map <string, int>::iterator ti = trans.find(prevTag + " " + tag);
			if (ti != trans.end())
				count = ti->second;

			A[I][J] = (count + alpha) / (context[prevTag] + alpha * K);
		}
	}
	return A;
}

// Generate emission matrix wordTagProbabilityMatrix of size numTags x N
// [B_ij stores the probability of observing o_j from state s_i]
// called by decode_seq
// KxN matrix B[i][j] = P(vocab[j]|tags[i]) with add-alpha smoothing
// over the vocab size N.
vector<vector<double>> constructWordTagProbabilityMatrix(map <string, int> &emiss, map <string, int> &context, vector <string> &tags, vector<string> &vocab)
{
	int K = tags.size();
	int N = vocab.size();
	vector <vector<double>> B(K, vector(N, (double)0));

	for (int I = 0; I < K; I++)
	{
		for (int J = 0; J < N; J++)
		{
			string tag = tags[I];
			string word = vocab[J];
			// Compute smoothed emission probability
			int count = 0;
			map <string, int>::iterator ei = emiss.find(tag + " " + word);
			if (ei != emiss.end())
				count = ei->second;

			B[I][J] = (count + alpha) / (context[tag] + alpha * N);
		}
	}
	return B;
}

// called by tag
// Read one-word-per-line data_filepath. Push the raw token to orig and
// either the same token, --n-- (blank line), or assign_unk() to prep.
// Vocab membership is a linear find (O(|V|) per token).
void preprocess(vector<string> &vocab, char *data_filepath, vector<string> &orig, vector<string> &prep)
{
	// Read data
	FILE *data_fp = fopen(data_filepath, "r");
	// Start state
	string prev = "--s--";
	char word[100 + 1];
	for (int cnt = 0; fgets(word, sizeof(word), data_fp) != NULL; cnt++)
	{
		word[strlen(word) - 1] = 0;
		// End of sentence
		if (word[0] == 0)
		{
			orig.push_back(word);
			prep.push_back("--n--");
			continue;
		}
		// Handle unknown words
		else if (find(vocab.begin(), vocab.end(), word) == vocab.end())
		{
			orig.push_back(word);
			prep.push_back(assign_unk(word));
			continue;
		}
		else
		{
			orig.push_back(word);
			prep.push_back(word);
		}
	}
}

// O  Observation space - set<string> &vocab
// S  State space - vector <string> &tags
// Y  Sequence of observations - vector<string> &prep
// A  Transition matrix - vector<vector<int>> tagTransitionProbabilityMatrix
// B  Emission matrix - vector<vector<int>> wordTagProbabilityMatrix
// X  Output - vector<int> X
// intermediates
// vector <vector<double>> probabilityMatrix
// vector <vector<double>> pathMatrix
// map <string, int> vocabLookupVector
// called by decode
// Allocate KxT probability (0) and path (-1) matrices, empty X[T], and
// a word->column lookup from vocab. T = prep.size().
void initViterbi(vector<string> &vocab, vector <string> &tags, vector<string> &prep,
	vector<string> &X, vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, map <string, int> &lookup)
{
	int K = tags.size();

	// Word index vocabLookupVector table
	int count = 0;
	for (vector<string>::iterator vi = vocab.begin(), viEnd = vocab.end(); vi != viEnd; vi++, count++)
		lookup[*vi] = count;

	int T = prep.size();
	probabilityMatrix = vector(K, vector(T, (double)0.0));
	pathMatrix = vector(K, vector(T, (int)-1));

	// Predicted tags
	X = vector(T, string());
}

// S  State space - vector <string> &tags
// called by decode
// Column 0: log P(--s-- -> tag_i) + log P(prep[0]|tag_i). Zero A is -inf.
// --s-- missing from tags makes s_idx==K and A[s_idx] out of range.
void initViterbi2(vector <string> &tags, vector<string> &prep, vector<vector<double>> &A, vector<vector<double>> &B,
	vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, map <string, int> &lookup)
{
	// Initialize start probabilities
	int s_idx = std::distance(tags.begin(), find(tags.begin(), tags.end(), "--s--"));
	int K = tags.size();
	for (int i = 0; i < K; i++)
	{
		if (A[s_idx][i] == 0)
		{
			probabilityMatrix[i][0] = (double)-std::numeric_limits<double>::infinity(); //double("-inf");
			pathMatrix[i][0] = 0;
		}
		else
		{
			probabilityMatrix[i][0] = log(A[s_idx][i]) + log(B[i][lookup[prep[0]]]);
			pathMatrix[i][0] = 0;
		}
	}
}

// Forward step
// called by decode
// Viterbi forward: for each time i and state j, take max_k of
// prob[k][i-1] + log A[k][j] + log B[j][prep[i]]. O(T*K^2).
void forward(int K, vector<string> &prep, vector<vector<double>> &A, vector<vector<double>> &B, vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, map <string, int> &lookup)
{
	int T = prep.size();
	for (int i = 1; i < T; i++)
	{
		if (i % 5000 == 0)
			printf("Words processed: %d\n", i);

		for (int j = 0; j < K; j++)
		{
			double best_prob = (double)-std::numeric_limits<double>::infinity();
			int best_path = -1;

			for (int k = 0; k < K; k++)
			{
				double prob = probabilityMatrix[k][i - 1] + log(A[k][j]) + log(B[j][lookup[prep[i]]]);
				if (prob > best_prob)
				{
					best_prob = prob;
					best_path = k;
				}
			}
			probabilityMatrix[j][i] = best_prob;
			pathMatrix[j][i] = best_path;
		}
	}
}

// Backward step
// numTags = number of tags
// numWordsInSource = number of words in text 
// order numWordsInSource+numTags
// called by decode
// Backpointer walk from the best state at T-1. If every last-column
// probability is <= probabilityMatrix[0][T-1] and that cell is not the
// max, z[T-1] stays -1 and tags[-1] is UB. Writes predicted tags into X.
void backward(int T, vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, vector <string> &tags, vector<string> &X)
{
	int K = tags.size();
	vector <int> z = vector(T, (int)-1);
	double maximumProbability = probabilityMatrix[0][T - 1];

	for (int k = 1; k < K; k++)
	{
		if (probabilityMatrix[k][T - 1] > maximumProbability)
		{
			maximumProbability = probabilityMatrix[k][T - 1];
			z[T - 1] = k;
		}
	}
	X[T - 1] = tags[z[T - 1]];
	for (int i = T - 1; i > 0; i--)
	{
		z[i - 1] = pathMatrix[z[i]][i];
		X[i - 1] = tags[z[i - 1]];
	}
}

// Debug dump of tags, prep, A, B, probability/path matrices, lookup, X.
// fopen unchecked. Shared column counter `c` wraps across sections.
void dump(vector<string> &prep, vector<vector<double>> &A, vector<vector<double>> &B, vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, map <string, int> &lookup, vector <string> &tags, vector<string> &X, char *filename)
{
	FILE *out = fopen(filename, "w");
	fprintf(out, "TAGS\n");
	for (string t : tags)
		fprintf(out, "%s\n", t.c_str());
	fprintf(out, "PREP\n");
	for (string p : prep)
		fprintf(out, "%s\n", p.c_str());
	fprintf(out, "tagTransitionProbabilityMatrix\n");
	int c = 0;
	for (vector<double> vA : A)
	{
		for (double f : vA)
		{
			if ((c++ % 8) == 0)
				fprintf(out, "\n");
			fprintf(out, "%2.4f ", f);
		}
		fprintf(out, "\n");
	}
	fprintf(out, "wordTagProbabilityMatrix\n");
	for (vector<double> vB : B)
	{
		for (double f : vB)
		{
			if ((c++ % 8) == 0)
				fprintf(out, "\n");
			fprintf(out, "%2.4f ", f);
		}
		fprintf(out, "\n");
	}
	fprintf(out, "probabilityMatrix\n");
	for (vector<double> vT1 : probabilityMatrix)
	{
		for (double f : vT1)
		{
			if ((c++ % 8) == 0)
				fprintf(out, "\n");
			fprintf(out, "%2.4f ", f);
		}
		fprintf(out, "\n");
	}
	fprintf(out, "pathMatrix\n");
	for (vector<int> vT2 : pathMatrix)
	{
		for (int i : vT2)
		{
			if ((c++ % 8) == 0)
				fprintf(out, "\n");
			fprintf(out, "%d ", i);
		}
		fprintf(out, "\n");
	}
	fprintf(out, "lookup\n");
	for (const auto&[key, value] : lookup) {
		fprintf(out, "%s %d\n", key.c_str(), value);
	}
	fprintf(out, "X\n");
	for (string t : X)
	{
		if ((c++ % 8) == 0)
			fprintf(out, "\n");
		fprintf(out, "%s", t.c_str());
	}
	fclose(out);
}

//	Run the algorithm
// called by tag
// Full Viterbi: init matrices, seed column 0, forward, backward into X.
void decode(vector<string> &vocab, vector <string> &tags, vector<string> &prep, vector<vector<double>> &A, vector<vector<double>> &B,
	vector<string> &X)
{
	int K = tags.size();
	int T = prep.size();
	vector <vector<double>> probabilityMatrix;
	vector <vector<int>> pathMatrix;
	map <string, int> lookup;
	// Initialize start probabilities
	initViterbi(vocab, tags, prep, X, probabilityMatrix, pathMatrix, lookup);
	initViterbi2(tags, prep, A, B, probabilityMatrix, pathMatrix, lookup);
	forward(K, prep, A, B, probabilityMatrix, pathMatrix, lookup);
	//dump(prep, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix, pathMatrix, vocabLookupVector, tags,X,"../HMM2CTest/data/INTERMEDIATE.txt");
	backward(T, probabilityMatrix, pathMatrix, tags, X);
	//dump(prep, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix, pathMatrix, vocabLookupVector, tags, X, "../HMM2CTest/data/FINAL.txt");
}

// Tag development / test data
// called by decode_seq
// Preprocess DEV_WORDS or TEST_WORDS, decode, write word\\ttag lines
// (blank orig => blank line) to DEV_OUT / TEST_OUT.
void tag(boolean isTest, vector <string> &tags, vector<string> &vocab, vector<vector<double>> &A, vector<vector<double>> &B)
{
	// Preprocess data
	char *data_fp = DEV_WORDS;
	if (isTest)
		data_fp = TEST_WORDS;
	vector<string> orig, prep;
	preprocess(vocab, data_fp, orig, prep);

	// Decode
	vector<string> pred;
	decode(vocab, tags, prep, A, B, pred);
	FILE *out_fp = fopen((isTest) ? TEST_OUT : DEV_OUT, "w");
	for (int I = 0; I < orig.size(); I++)
	{
		if (orig[I].empty())
			fprintf(out_fp, "\n");
		else
			fprintf(out_fp, "%s\t%s\n", orig[I].c_str(), pred[I].c_str());
	}
	fclose(out_fp);
}

// load_model, collect tags from context keys, build A/B, tag(isTest).
void decode_seq(boolean isTest, vector <string> &model, vector<string> &vocab)
{
	// Decode sequences
	map <string, int> emiss, trans, context;
	load_model(model, emiss, trans, context);
	vector <string> tags;
	for (auto const& ic : context)
		tags.push_back(ic.first);
	// Transition matrix
	vector<vector<double>> A = constructTagTransitionProbabilityMatrix(trans, context, tags);
	// Emission matrix
	vector<vector<double>> B = constructWordTagProbabilityMatrix(emiss, context, tags, vocab);
	tag(isTest, tags, vocab, A, B);
}

// Load or generate VOCAB and MODEL, then decode_seq on dev (isTest=false)
// or test. access()==0 means the cache file exists.
void runTest(bool isTest)
{
	vector <string> vocab;
	if (access(VOCAB, 0) != 0)
	{
		printf("Generating vocabulary...\n");
		vocab = generate_vocab();
	}
	else
	{
		FILE *vocab_fp = fopen(VOCAB, "r");
		// Start state
		char line[100 + 1];
		while (fgets(line, sizeof(line), vocab_fp) != NULL)
		{
			line[strlen(line) - 1] = 0;
			vocab.push_back(line);
		}
		fclose(vocab_fp);
	}
	vector <string> model;
	if (access(MODEL, 0) != 0)
	{
		printf("Training model...\n");
		model = train_model(vocab, TRAIN);
	}
	else
	{
		FILE *model_fp = fopen(MODEL, "r");
		// Start state
		char line[100 + 1];
		while (fgets(line, sizeof(line), model_fp) != NULL)
		{
			line[strlen(line) - 1] = 0;
			model.push_back(line);
		}
		fclose(model_fp);
	}

	printf("Decoding %s split...\n", (isTest) ? "test" : "dev");
	decode_seq(isTest, model, vocab);
	printf("Done\n");
}

// --------------------------------------------------------------------------------------------------
