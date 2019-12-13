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
#include <iterator>
#include "mysqldb.h"
#include "DIYDiskArray.h"
#include<jni.h>
#include "hmm.h"

using namespace std;
// Additive smoothing parameter
double alpha = 0.001;

//#define USE_ALPHA_FOR_WORDTAG

// from Melanie Tosik
// NLP, Viterbi part - of - speech(POS) tagger
//http://www.melanietosik.com/posts/Viterbi-POS-tagger

int createJavaVM(JavaVM *&vm, JNIEnv *&env)
{
	JavaVMOption options[5];
	memset(&options, 0, sizeof(options));
	options[0].optionString = "-Djava.class.path=.;F:\\lp\\Stanford\\workspace\\StanfordParser\\target\\StanfordParser-0.0.1-SNAPSHOT.jar";
	options[1].optionString = "-Xms10m"; // 1MB
	options[2].optionString = "-Xmx3g"; // 1GB
	options[3].optionString = "-mx2400m"; // 2.4GB
	options[4].optionString = "-server"; // Selects server application runtime optimizations. The directory server will take longer to start and “warm up” but will be more aggressively optimized to produce higher throughput.

	JavaVMInitArgs vm_args;
	vm_args.version = JNI_VERSION_1_8;
	vm_args.nOptions = 5;
	vm_args.options = options;
	vm_args.ignoreUnrecognized = 1;
	jint res = JNI_CreateJavaVM(&vm, (void **)&env, &vm_args);
	return res;
}

// https://www.clips.uantwerpen.be/pages/mbsp-tags
unordered_map<wstring, vector<wstring>> pennMapToLP = {
{ L"CC",{ L"conjunction" }},
{ L"CD",{L"numeral_cardinal" }},
{ L"DT",{L"determiner" }},
{ L"EX",{L"there" }},
{ L"FW",{L"" }},
{ L"IN",{L"preposition",L"conjunction" }},
{ L"JJ",{L"adjective" }},
{ L"JJR",{L"adjective" }},
{ L"JJS",{L"adjective" }},
{ L"LS",{L"|||" }},
{ L"MD",{L"modal_auxiliary" }},
{ L"NN",{L"noun" }},
{ L"NNS",{L"noun" }},
{ L"NNP",{L"Proper Noun" }},
{ L"NNPS",{L"Proper Noun" }},
{ L"PDT",{L"predeterminer" }},
{ L"POS",{L"" }},
{ L"PRP",{L"personal_pronoun_accusative",L"personal_pronoun_nominative",L"personal_pronoun",L"reflexive_pronoun" }},
{ L"PRP$",{L"possessive_determiner" }},
{ L"RB",{L"adverb" }},
{ L"RBR",{L"adverb" }},
{ L"RBS",{L"adverb" }},
{ L"RP",{L"particle" }},
{ L"SYM",{L"symbol" }},
{ L"TO",{L"to" }},
{ L"UH",{L"interjection" }},
{ L"VB",{L"verb" }},
{ L"VBD",{L"verb" }},
{ L"VBG",{L"verb" }},
{ L"VBN",{L"verb" }},
{ L"VBP",{L"verb" }},
{ L"VBZ",{L"verb" }},
{ L"WDT",{L"relativizer",L"interrogative_determiner",L"demonstrative_determiner",L"which",L"what",L"whose" }}, // wh-determiner: which, whatever, whichever 
{ L"WP",{L"interrogative_pronoun",L"what" }}, // wh-pronoun, personal:	what, who, whom 
{ L"WP$",{L"interrogative_determiner",L"whose",L"relativizer" }}, // wh-pronoun, possessive:	whose, whosever 
{ L"WRB",{L"adverb",L"relativizer",L"how"} }, // wh-adverb:	where, when - LP does not distinguish WRB used to introduce a relative phrase (which is true) and its adverbial use (also true)
{ L".",{ }}, // punctuation mark, sentence closer	.; ? *
{ L",",{ }}, //	punctuation mark, comma	,
{ L":",{ }}, // punctuation mark, colon :
{ L"(",{ }}, // contextual separator, left paren (
{ L")",{ }}, //	contextual separator, right paren )
{ L"''",{ }}, //	not listed in standard PennBank tag list but emitted by Stanford
{ L"``",{ }}, //	not listed in standard PennBank tag list but emitted by Stanford
{ L"$",{ }} //	not listed in standard PennBank tag list but emitted by Stanford
};

bool foundParsedSentence(Source &source, wstring sentence, wstring &parse)
{
	//if (!myquery(&source.mysql, L"LOCK TABLES stanfordPCFGParsedSentences READ")) return false; // moved out to higher level for performance
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	std::replace(sentence.begin(), sentence.end(), L'\'', L'"');
	size_t sentencehash=std::hash<std::wstring>{}(sentence);
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select parse from stanfordPCFGParsedSentences where sentencehash = %I64d", (__int64)sentencehash); // must be %I64d because of BIGINT signed considerations
	MYSQL_RES * result = NULL;
	MYSQL_ROW sqlrow = NULL;
	parse.erase();
	if (myquery(&source.mysql, qt, result) && (sqlrow = mysql_fetch_row(result)))
	{
		mTW(sqlrow[0], parse);
		if (parse.empty())
			lplog(LOG_FATAL_ERROR, L"Parse is empty to %s", sentence.c_str());
		parse += L" ";
		std::replace(parse.begin(), parse.end(), L'"', L'\'');
	}
	mysql_free_result(result);
	//source.unlockTables(); // moved out to higher level for performance
	return parse.length() > 0;
}


int setParsedSentence(Source &source, wstring sentence, wstring parse)
{
	//if (!myquery(&source.mysql, L"LOCK TABLES stanfordPCFGParsedSentences WRITE")) // moved out to higher level for performance
	//	return -1;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	std::replace(sentence.begin(), sentence.end(), L'\'', L'"');
	std::replace(parse.begin(), parse.end(), L'\'', L'"');
	size_t sentencehash = std::hash<std::wstring>{}(sentence);
	_snwprintf(qt, QUERY_BUFFER_LEN, L"insert stanfordPCFGParsedSentences (parse,sentence,sentencehash) VALUES('%s','%s',%I64d)", parse.c_str(),sentence.c_str(),(__int64)sentencehash);
	if (!myquery(&source.mysql, qt))
		return -1;
	//source.unlockTables();
	return 0;
}

int parseSentence(Source &source,JNIEnv *env, wstring sentence, wstring &parse, bool pcfg)
{
	static jclass parserDemoClass;
	static jmethodID parseSentenceMethod;
	static bool initialized = false;
	if (pcfg && foundParsedSentence(source, sentence, parse))
		return 0;
	if (!initialized)
	{
		// First get the class that contains the method you need to call
		parserDemoClass = env->FindClass("edu/stanford/nlp/parser/lexparser/demo/ParserDemo");
		if (env->ExceptionCheck()) {
			env->ExceptionDescribe();
			return -1;
		}
		//jmethodID ParserDemoConstructorMethod = env->GetMethodID(parserDemoClass, "<init>", "()V");
		//if ((env)->ExceptionCheck()) {
		//	env->ExceptionDescribe();
		//	return -2;
		//}
		//jobject ParserDemoObject = env->CallObjectMethod(parserDemoClass, ParserDemoConstructorMethod);
		// Get the method parseSentence, taking one string and returning one string
		parseSentenceMethod = env->GetStaticMethodID(parserDemoClass, "parseSentence", "(Ljava/lang/String;Z)Ljava/lang/String;");
		if ((env)->ExceptionCheck()) {
			env->ExceptionDescribe();
			return -2;
		}
		initialized = true;
	}
	lplog(LOG_ERROR, L"Did not find sentence! %s", sentence.c_str());
	// Construct the sentence argument - Java takes UTF8
	string out;
	jstring parseSentenceArgumentString = env->NewStringUTF(wTM(sentence, out));
	if ((env)->ExceptionCheck()) {
		env->ExceptionDescribe();
		return -3;
	}
	// Call the parseSentence method with a sentence argument
	jobject result = env->CallStaticObjectMethod(parserDemoClass, parseSentenceMethod, parseSentenceArgumentString, (pcfg) ? JNI_TRUE : JNI_FALSE);
	if ((env)->ExceptionCheck()) {
		env->ExceptionDescribe();
		return -4;
	}
	// Get a C-style string
	jboolean isCopy;
	const char *parseSentenceReturnString = env->GetStringUTFChars((jstring)result, &isCopy);
	if ((env)->ExceptionCheck()) {
		env->ExceptionDescribe();
		return -5;
	}
	mTW(parseSentenceReturnString, parse);
	// Clean up
	env->ReleaseStringUTFChars((jstring)result, parseSentenceReturnString);
	if ((env)->ExceptionCheck()) {
		env->ExceptionDescribe();
		return -6;
	}
	if (pcfg && setParsedSentence(source, sentence, parse) < 0)
		return -7;
	return 0;
}

int findLPPOSEquivalents(wstring sentence, wstring &parse,wstring originalWord,vector<wstring> &posList,int duplicateSkip,bool pcfg)
{
	parse = L" " + parse; // take care of the edge case where the match is at the beginning
	// pcfg output:
	// parse=(ROOT (PRN (: ;) (S (NP (NP (NP (QP (CC and) (CD Bunny))) (, ,) (CC and) (NP (NNP Bobtail)) (, ,)) (CC and) (NP (NNP Billy))) (VP (VBD were) (ADVP (RB always)) (VP (VBG doing) (NP (JJ *) (NN something)))))))
	if (pcfg)
	{
		if (originalWord[originalWord.length() - 2] == L'\'' && originalWord[originalWord.length() - 1] == L's')
			originalWord.erase(originalWord.length() - 2);
		originalWord = L" " + originalWord + L")";
		size_t wow = parse.find(originalWord);
		for (int dup = 0; dup < duplicateSkip; dup++)
		{
			if (wow != wstring::npos)
				wow = parse.find(originalWord,wow+1);
		}
		if (wow != wstring::npos)
		{
			auto firstparen = parse.rfind(L'(', wow);
			if (firstparen != wstring::npos)
			{
				wstring partofspeech = parse.substr(firstparen + 1, wow - firstparen - 1);
				auto lpPOS = pennMapToLP.find(partofspeech);
				if (lpPOS != pennMapToLP.end())
				{
					posList = lpPOS->second;
				}
				else
					lplog(LOG_ERROR, L"Part of Speech %s not found.", partofspeech.c_str());
			}
		}
	}
	else
	{
		// tagger output:
		// ;_: and_CC bunny_NN ,_, and_CC bobtail_NN ,_, and_CC billy_NNP were_VBD always_RB doing_VBG something_NN 
		originalWord = L" " + originalWord + L"_";
		size_t wow = parse.find(originalWord);
		if (wow == wstring::npos)
		{
			transform(originalWord.begin(), originalWord.end(), originalWord.begin(), (int(*)(int)) tolower);
			wow = parse.find(originalWord);
		}
		for (int dup = 0; dup < duplicateSkip; dup++)
		{
			if (wow != wstring::npos)
				wow = parse.find(originalWord, wow + 1);
		}
		if (wow != wstring::npos)
		{
			wow += originalWord.length();
			auto nextspace = parse.find(L' ', wow);
			if (nextspace != wstring::npos)
			{
				wstring partofspeech = parse.substr(wow, nextspace- wow);
				auto lpPOS = pennMapToLP.find(partofspeech);
				if (lpPOS != pennMapToLP.end())
				{
					posList = lpPOS->second;
				}
				else
					lplog(LOG_ERROR, L"Part of Speech %s not found.", partofspeech.c_str());
			}
		}
	}
	return 0;
}

// Shutdown the VM.
void destroyJavaVM(JavaVM *vm)
{
	vm->DestroyJavaVM();
}

vector <wstring> generateVocabFromSource(Source &source, int min_cnt = 2)
{
	//Generate vocabulary
	unordered_map <wstring, int> vocabAll;

	for (WordMatch &im: source.m)
		vocabAll[im.word->first] += 1;
	vector <wstring> vocabvector;
	// Remove words appearing only once
	for (auto const&[word, count] : vocabAll)
		if (count >= min_cnt)
			vocabvector.push_back(word);

	// Sort 
	sort(vocabvector.begin(), vocabvector.end());

	return vocabvector;
}

wstring startTag = L"--s--";
void trainModelFromSource(Source &source, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap)
{
	// Train part-of-speech (POS) tagger model
	set<wstring> taggedWords, allWords;
	// Start state
	vector<wstring> previousTags = { startTag };
	tagCountsMap[startTag] = 1;
	for (WordMatch im:source.m)
	{
		vector <int> winnerForms;
		im.getWinnerForms(winnerForms);
		vector <wstring> tags;
		wstring word = im.word->first;
		for (int wf : winnerForms)
		{
			wstring tag = Forms[wf]->name;
			std::replace(tag.begin(), tag.end(), ' ', '*');
			for (wstring ptag:previousTags)
				tagTransitionCountsMap[ptag + L" " + tag] += 1;
			std::replace(word.begin(), word.end(), ' ', '*');
			wordTagCountsMap[tag + L" " + word] += 1;
			tagCountsMap[tag] += 1;
			tags.push_back(tag);
		}
		allWords.insert(word);
		if (winnerForms.size()>0)
			taggedWords.insert(word);
		previousTags = tags;
		// not compatible with winner - startTag is never winner
		//if (source.isEOS(im - source.m.begin()))
		//{
		//	wstring word = L"--n--";
		//	wstring tag = startTag;
		//	for (wstring ptag : previousTags)
		//	{
		//		std::replace(ptag.begin(), ptag.end(), ' ', '*');
		//		tagTransitionCountsMap[ptag + L" " + tag] += 1;
		//	}
		//	wordTagCountsMap[tag + L" " + word] += 1;
		//	tagCountsMap[tag] += 1;
		//	previousTags = { tag };
		//}
	}
	set<wstring> untaggedWords;
	set_difference(allWords.begin(), allWords.end(), taggedWords.begin(), taggedWords.end(),std::inserter(untaggedWords, untaggedWords.begin()));
	lplog(LOG_ERROR, L"allWords=%d taggedWords=%d untaggedWords=%d",allWords.size(),taggedWords.size(),untaggedWords.size());
	if (untaggedWords.size())
	{
		wstring wordsToAdd;
		for (wstring utw : untaggedWords)
			wordsToAdd += L"\"" + utw + L"\",";
		MYSQL_RES * result;
		MYSQL_ROW sqlrow;
		wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
		_snwprintf(qt, QUERY_BUFFER_LEN, L"select w.word, f.name as formname, MAX(count) from words w, wordforms wf, forms f where wf.formId = f.id and w.id = wf.wordId and w.word in(%s) group by word", wordsToAdd.substr(0, wordsToAdd.length() - 1).c_str());
		if (!myquery(&source.mysql, qt, result))
			lplog(LOG_FATAL_ERROR, L"Error in model training.");
		//insert words that never got matched with any tag.
		// this is to prevent null probabilities in hmm matrix.
		for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
		{
			wstring word, tag;
			mTW(sqlrow[0], word);
			mTW(sqlrow[1], tag);
			wordTagCountsMap[tag + L" " + word] = 1;
		}
	}
}

vector <wstring> writeModelFile(wstring modelPath, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap)
{
	vector <wstring> model;

	FILE *out_fp = _wfopen(modelPath.c_str(), L"w, ccs=UNICODE");

	// Write transition counts
	for (auto const&[tags, count] : tagTransitionCountsMap)
	{
		wstring tline = L"T " + tags + L" " + std::to_wstring(count);
		model.push_back(tline);
		fwprintf(out_fp, L"%s\n", tline.c_str());
	}
	// Write emission counts
	for (auto const&[tagword, count] : wordTagCountsMap)
	{
		wstring eline = L"E " + tagword + L" " + std::to_wstring(count);
		model.push_back(eline);
		fwprintf(out_fp, L"%s\n", eline.c_str());
	}
	// Write tagCountsMap unordered_map
	for (auto const&[tag, count] : tagCountsMap)
	{
		wstring cline = L"C " + tag + L" " + std::to_wstring(count);
		model.push_back(cline);
		fwprintf(out_fp, L"%s\n", cline.c_str());
	}
	fclose(out_fp);
	return model;
}

vector <wstring> readModelFile(wstring modelPath)
{
	vector <wstring> model;
	FILE *model_fp = _wfopen(modelPath.c_str(), L"r, ccs=UNICODE");
	// Start state
	wchar_t line[100 + 1];
	while (fgetws(line, sizeof(line), model_fp) != NULL)
	{
		line[wcslen(line) - 1] = 0;
		model.push_back(line);
	}
	fclose(model_fp);
	return model;
}

// Load model
void loadModel(vector <wstring> &model, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap)
{
	for (vector <wstring>::iterator mi = model.begin(), miEnd = model.end(); mi != miEnd; mi++)
	{
		if ((mi-model.begin()) % 5000 == 0)
			printf("Loading model processed: %03d%%:%09I64d\r", (int) (100 * (mi - model.begin()) / model.size()), (__int64)(mi - model.begin()));
		wstring type, tag, x;
		int count;
		std::wstringstream convertor(*mi);
		if (mi->at(0) == L'C')
		{
			convertor >> type >> tag >> count;
			std::replace(tag.begin(), tag.end(), '*', ' ');
			if (convertor.fail() == true)
				lplog(LOG_ERROR,L"failed to read in tagCountsMap data %s", mi->c_str());
			else
				tagCountsMap[tag] = int(count);
			continue;
		}
		convertor >> type >> tag >> x >> count;
		std::replace(tag.begin(), tag.end(), '*', ' ');
		std::replace(x.begin(), x.end(), '*', ' ');
		if (mi->at(0) == L'T')
		{
			tagTransitionCountsMap[tag + L" " + x] = int(count);
		}
		else
		{
			wordTagCountsMap[tag + L" " + x] = int(count);
		}
	}
}

vector<vector<double>> constructTagTransitionProbabilityMatrix(unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap, vector <wstring> &tags)
{
	int tagsSize = tags.size();
	vector <vector<double>> tagTransitionProbabilityMatrix(tagsSize, vector(tagsSize, (double)0));

	for (int previousTagIndex = 0; previousTagIndex < tagsSize; previousTagIndex++)
	{
		for (int currentTagIndex = 0; currentTagIndex < tagsSize; currentTagIndex++)
		{
			wstring prevTag = tags[previousTagIndex];
			wstring tag = tags[currentTagIndex];
			// Compute smoothed transition probability
			int tagTransitionCount = 0;
			unordered_map <wstring, int>::iterator ti = tagTransitionCountsMap.find(prevTag + L" " + tag);
			if (ti != tagTransitionCountsMap.end())
				tagTransitionCount = ti->second;

			tagTransitionProbabilityMatrix[previousTagIndex][currentTagIndex] = (tagTransitionCount + alpha) / (tagCountsMap[prevTag] + alpha * tagsSize);
		}
	}
	return tagTransitionProbabilityMatrix;
}

// Generate emission matrix wordTagProbabilityMatrix of size numTags x vocabSize
// [wordTagProbabilityMatrix[i][j] stores the probability of observing o_j from state s_i]
vector<vector<double>> constructWordTagProbabilityMatrix(unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagCountsMap, vector <wstring> &tags, vector<wstring> &vocab)
{
	int tagsSize = tags.size();
	int vocabSize = vocab.size();
	vector <vector<double>> wordTagProbabilityMatrix(tagsSize, vector(vocabSize, (double)0));

	for (int tagIndex = 0; tagIndex < tagsSize; tagIndex++)
	{
		for (int vocabIndex = 0; vocabIndex < vocabSize; vocabIndex++)
		{
			wstring tag = tags[tagIndex];
			wstring word = vocab[vocabIndex];
			// Compute smoothed emission probability
			int wordTagCount = 0;
			unordered_map <wstring, int>::iterator ei = wordTagCountsMap.find(tag + L" " + word);
			if (ei != wordTagCountsMap.end())
			{
				wordTagCount = ei->second;
				#ifndef USE_ALPHA_FOR_WORDTAG
					wordTagProbabilityMatrix[tagIndex][vocabIndex] = ((double)wordTagCount) / (tagCountsMap[tag]);
				#endif
			}
			#ifdef USE_ALPHA_FOR_WORDTAG
				wordTagProbabilityMatrix[tagIndex][vocabIndex] = (wordTagCount + alpha) / (tagCountsMap[tag] + alpha * vocabSize);
			#endif
		}
	}
	return wordTagProbabilityMatrix;
}

// O  Observation space - set<string> &vocab
// S  State space - vector <string> &tags
// Y  Sequence of observations - vector<string> &prep
// tagTransitionProbabilityMatrix  Transition matrix - vector<vector<int>> tagTransitionProbabilityMatrix
// wordTagProbabilityMatrix  Emission matrix - vector<vector<int>> wordTagProbabilityMatrix
// X  Output - vector<int> X
// intermediates
// vector <vector<double>> probabilityMatrix
// vector <vector<int>> pathMatrix
// unordered_map <string, int> vocabLookupVector
void initViterbiStartProbabilities(int numWords,wstring firstWord,vector<wstring> &vocab, vector <wstring> &tags,
	vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, 
	DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix,
	unordered_map <wstring, int> &vocabReverseLookup)
{
	printf("initializing viterbi                                                \r");
	// Word index vocabReverseLookup map
	int count = 0;
	for (vector<wstring>::iterator vi = vocab.begin(), viEnd = vocab.end(); vi != viEnd; vi++, count++)
		vocabReverseLookup[*vi] = count;

	int numTags = tags.size();
	probabilityMatrix.initialize(numTags, numWords, (double)-std::numeric_limits<double>::infinity());
	pathMatrix.initialize(numTags, numWords, (int)-1);

	// Initialize start probabilities
	int startTagIndex = (int)std::distance(tags.begin(), find(tags.begin(), tags.end(), startTag));
	int firstWordIndex = vocabReverseLookup[firstWord];
	for (int tagIndex = 0; tagIndex < numTags; tagIndex++)
	{
		if (tagTransitionProbabilityMatrix[startTagIndex][tagIndex] == 0)
		{
			probabilityMatrix.put(tagIndex,0,(double)-std::numeric_limits<double>::infinity()); //double("-inf");
			pathMatrix.put(tagIndex,0,0);
		}
		else
		{
			//probabilityMatrix[tagIndex][0] = log(tagTransitionProbabilityMatrix[startTagIndex][tagIndex]) + log(wordTagProbabilityMatrix[tagIndex][firstWordIndex]); // CHANGE from log add to multiplication
			probabilityMatrix.put(tagIndex, 0, tagTransitionProbabilityMatrix[startTagIndex][tagIndex] * wordTagProbabilityMatrix[tagIndex][firstWordIndex]);
			pathMatrix.put(tagIndex, 0, 0);
		}
	}
}

wstring getContext(Source &source, int wordSourceIndex,bool star,int &duplicateSkip)
{
	wstring context;
	int begin=max(0,wordSourceIndex-20), end=min(source.m.size(),wordSourceIndex+20);
	for (int I = wordSourceIndex-1; I >= 0 && I > wordSourceIndex - 20; I--)
		if (source.isEOS(I))
		{
			begin = I;
			break;
		}
	for (int I = wordSourceIndex+1; I<source.m.size() && I<wordSourceIndex+20; I++)
		if (source.isEOS(I))
		{
			end = I;
			break;
		}
	wstring originalWord;
	source.getOriginalWord(wordSourceIndex, originalWord, false, false);
	for (int I = begin; I < end; I++)
	{
		wstring originalIWord;
		source.getOriginalWord(I, originalIWord, false, false);
		if (I<wordSourceIndex && originalIWord == originalWord)
			duplicateSkip++;
		if (I == wordSourceIndex && star)
			context += L"*";
		context += originalIWord + L" ";
	}
	return context;
}

// Forward step
// numTags = number of tags
// numWordsInSource = number of words in text 
// order numWordsInSource*numTags*numTags
// --- output in probabilityMatrix and pathMatrix
void forwardFromSource(Source &source,vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix,
												vector <wstring> &tags, unordered_map <wstring, int> &wordSourceIndexLookup, unordered_map <wstring, int> &tagLookup)
{
	int numWordsInSource = source.m.size();
	double probMult = 1.0*numWordsInSource* numWordsInSource; // CHANGE from log add to multiplication
	for (int wordSourceIndex = 1; wordSourceIndex < numWordsInSource; wordSourceIndex++)
	{
		if (wordSourceIndex % 5000 == 0)
			printf("Words forward processed: %03d%%:%09d\r", 100*wordSourceIndex/numWordsInSource,wordSourceIndex);
		int wordVocabIndex = wordSourceIndexLookup[source.m[wordSourceIndex].word->first];
		double maximumProbabilityPerWordIndex = (double)-std::numeric_limits<double>::infinity();
		int previousTagOfHighestProbabilityPerWordIndex = -1, currentTagOfHighestProbabilityPerWordIndex=-1;
		for (int tag: source.m[wordSourceIndex].getForms())
		{
			if (tag == UNDEFINED_FORM_NUM)
				continue;
			unordered_map <wstring, int>::iterator tli = tagLookup.find(Forms[tag]->name);
			if (tli == tagLookup.end())
				continue; // this tag occurs for the word but is nowhere in the training model, because the form was never winner
			int tagIndex = tli->second;
			double best_prob = (double)-std::numeric_limits<double>::infinity();
			int previousTagOfHighestProbability = -1;
			for (int prevTag: source.m[wordSourceIndex - 1].getForms())
			{
				if (prevTag == UNDEFINED_FORM_NUM)
					continue;
				tli = tagLookup.find(Forms[prevTag]->name);
				if (tli == tagLookup.end())
					continue; // this tag occurs for the word but is nowhere in the training model, because the form was never winner
				int prevTagIndex = tli->second;
				//double prob = probabilityMatrix[prevTagIndex][wordSourceIndex - 1] +	log(tagTransitionProbabilityMatrix[prevTagIndex][tagIndex]) + log(wordTagProbabilityMatrix[tagIndex][wordVocabIndex]); // CHANGE from log add to multiplication
				double prob = probMult * probabilityMatrix.get(prevTagIndex,wordSourceIndex - 1) * tagTransitionProbabilityMatrix[prevTagIndex][tagIndex] * wordTagProbabilityMatrix[tagIndex][wordVocabIndex];
				if (prob<0)
					lplog(LOG_ERROR, L"probability:%d:%s%s:tag %s:previous %.14f*tag transition %.14f*word tag %.14f=%.14f", wordSourceIndex, source.m[wordSourceIndex].word->first.c_str(), 
						(source.m[wordSourceIndex].flags&WordMatch::flagOnlyConsiderProperNounForms) ? L"[onlyProperNounSet]":L"",
						Forms[tag]->name.c_str(),probabilityMatrix.get(prevTagIndex,wordSourceIndex - 1),tagTransitionProbabilityMatrix[prevTagIndex][tagIndex],wordTagProbabilityMatrix[tagIndex][wordVocabIndex],prob);
				if (prob > best_prob)
				{
					best_prob = prob;
					previousTagOfHighestProbability = prevTagIndex;
				}
			}
			if (best_prob > maximumProbabilityPerWordIndex)
			{
				maximumProbabilityPerWordIndex = best_prob;
				previousTagOfHighestProbabilityPerWordIndex = previousTagOfHighestProbability;
				currentTagOfHighestProbabilityPerWordIndex = tag;
			}
			probabilityMatrix.put(tagIndex,wordSourceIndex,best_prob);
			#ifdef USE_ALPHA_FOR_WORDTAG
				if (best_prob < 0.000000001)
					lplog(LOG_ERROR, L"Low probability detected:word:%s,tagIndex=%d:%s,wordSourceIndex=%d:%.14f", source.m[wordSourceIndex].word->first.c_str(), tagIndex, Forms[tag]->name.c_str(),wordSourceIndex, best_prob);
			#else
			  if (wordSourceIndex==75)
					lplog(LOG_ERROR, L"Low probability detected:word:%s,tagIndex=%d:%s,wordSourceIndex=%d:%.14f", source.m[wordSourceIndex].word->first.c_str(), tagIndex, Forms[tag]->name.c_str(), wordSourceIndex, best_prob);
			#endif	
			pathMatrix.put(tagIndex,wordSourceIndex,previousTagOfHighestProbability);
			if (previousTagOfHighestProbability>=0 && !source.m[wordSourceIndex-1].testPreferredViterbiForm(tags[previousTagOfHighestProbability]))
				lplog(LOG_ERROR, L"%d:*InterimForward Error setting word %s to tag %s (%d) [probability=%f=(prevProb=%f+log(tagTransitionProbability=%f [prevTag=%s][toTag=%s])+log(wordTagProbability=%f [tag=%s,word=%s])]",
					wordSourceIndex-1, source.m[wordSourceIndex-1].word->first.c_str(), tags[previousTagOfHighestProbability].c_str(), previousTagOfHighestProbability, best_prob,
					probabilityMatrix.get(previousTagOfHighestProbability,wordSourceIndex), // prevProb
					(tagTransitionProbabilityMatrix[previousTagOfHighestProbability][tagIndex]), tags[previousTagOfHighestProbability].c_str(), tags[tagIndex].c_str(),// tagTransitionProbability, prevTagIndex, toTag
					(wordTagProbabilityMatrix[tagIndex][wordVocabIndex]), tags[tagIndex].c_str(), source.m[wordSourceIndex].word->first.c_str()); // wordTagProbability, toTag, word
		}
		if (maximumProbabilityPerWordIndex < 0.000000001)
		{
			if (source.m[wordSourceIndex].flags&WordMatch::flagOnlyConsiderProperNounForms)
			{
				int duplicateSkip=0;
				lplog(LOG_ERROR, L"%d:MAXREDO forceProperNoun incorrect:[%s]", wordSourceIndex, getContext(source, wordSourceIndex,true, duplicateSkip).c_str());
				source.m[wordSourceIndex].flags &= ~WordMatch::flagOnlyConsiderProperNounForms;
				wordSourceIndex--;
				continue;
			}
			else
				lplog(LOG_ERROR, L"Low MAXIMUM probability detected:wordSourceIndex=%d:%.14f", wordSourceIndex, maximumProbabilityPerWordIndex);
		}
		if (maximumProbabilityPerWordIndex != (double)-std::numeric_limits<double>::infinity())
			probMult = ((double)numWordsInSource* numWordsInSource) / maximumProbabilityPerWordIndex; // CHANGE from log add to multiplication
		if (probMult == nan(NULL))
			lplog(LOG_FATAL_ERROR, L"Viterbi: forward probability multiplier is not a number: %f", maximumProbabilityPerWordIndex);
		if (probMult < 0.000000001)
			lplog(LOG_ERROR, L"Low probMult probability detected:%d:%.14f %.14f/%.14f", wordSourceIndex, probMult, ((double)numWordsInSource* numWordsInSource),maximumProbabilityPerWordIndex);
		source.m[wordSourceIndex].preferredViterbiMaximumProbability = maximumProbabilityPerWordIndex;
		source.m[wordSourceIndex].preferredViterbiPreviousTagOfHighestProbability = previousTagOfHighestProbabilityPerWordIndex;
		source.m[wordSourceIndex].preferredViterbiCurrentTagOfHighestProbability = currentTagOfHighestProbabilityPerWordIndex;
	}
}

int backwardFromSource(Source &source,vector <wstring> &tags, unordered_map <wstring, int> &tagLookup, DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix)
{
	int numWordsInSource = source.m.size(),criticalErrors=0;
	vector <int> z = vector(numWordsInSource, (int)-1);
	double maximumProbability = probabilityMatrix.get(0, numWordsInSource - 1);
	for (unsigned int tagFormOffset = 0; tagFormOffset < source.m[numWordsInSource - 1].formsSize(); tagFormOffset++)
	{
		int tagIndex = tagLookup[source.m[numWordsInSource - 1].word->second.Form(tagFormOffset)->name];
		if (probabilityMatrix.get(tagIndex,numWordsInSource - 1) > maximumProbability)
		{
			maximumProbability = probabilityMatrix.get(tagIndex,numWordsInSource - 1);
			z[numWordsInSource - 1] = tagIndex;
		}
	}
	lplog(LOG_INFO, L"Viterbi maximum probability=%.14f", maximumProbability);
	if (z[numWordsInSource - 1] < 0 || !source.m[numWordsInSource - 1].setPreferredViterbiForm(tags[z[numWordsInSource - 1]], probabilityMatrix.get(z[numWordsInSource - 1],numWordsInSource - 1)))
	{
		lplog(LOG_ERROR, L"%d:(1)Error setting word %s to tag %s (%d)", numWordsInSource - 1, source.m[numWordsInSource - 1].word->first.c_str(), (z[numWordsInSource - 1] >= 0) ? tags[z[numWordsInSource - 1]].c_str() : L"ILLEGAL TAG", z[numWordsInSource - 1]);
		criticalErrors++;
	}
	for (int i = numWordsInSource - 1; i > 0; i--)
	{
		if (i % 5000 == 0)
			printf("Words backward processed: %03d%%:%09d\r", 100 * (numWordsInSource-i) / numWordsInSource, i);
		if (z[i] < 0)
		{
			lplog(LOG_ERROR, L"%d:Error setting next path (%d)", i, z[i]);
			criticalErrors++;
			break;
		}
		z[i - 1] = pathMatrix.get(z[i],i);
		// remove the previous path probability - just assess the probability of the tag at that word alone.
		if (z[i - 1] < 0 || !source.m[i - 1].setPreferredViterbiForm(tags[z[i - 1]], probabilityMatrix.get(z[i - 1],i - 1)))
		{
			lplog(LOG_ERROR, L"%d:(2)Error setting word %s to tag %s (%d)", i - 1, source.m[i - 1].word->first.c_str(), (z[i - 1] >= 0) ? tags[z[i - 1]].c_str() : L"ILLEGAL TAG", z[i - 1]);
			criticalErrors++;
		}
	}
	return criticalErrors;
}



unordered_map<wstring, vector <wstring> > viterbiAssociationMap = {

	// amplification
	//{L"sectionheader", L"noun"},

	// similarity 
	{L"coordinator",{ L"conjunction"} },

	// include possible subclasses
	{L"verb", { L"verbverb",L"SYNTAX:Accepts S as Object"} }, // feel, see, watch, hear, tell etc // fancy, say (thinksay verbs)
	{L"noun",{ L"dayUnit",L"timeUnit",L"simultaneousUnit",L"quantifier",L"Proper Noun" } }, // all, some etc
	{L"adjective",{ L"quantifier" } } // many
};

// include subclasses of forms with their parents.
// include the word itself if viterbi doesn't include it.
// if viterbi specifies the word as the form, specify every form (as in that case the viterbi pick is ambiguous)
void appendAssociatedFormsToViterbiTags(Source &source)
{
	unordered_map<wstring, vector <wstring> > originalViterbiAssociationMap = viterbiAssociationMap;
	for (auto const&[form, vectorforms] : originalViterbiAssociationMap)
		for (wstring f : vectorforms)
			viterbiAssociationMap[f].push_back(form);

	int wordIndex=0;
	for (WordMatch &im : source.m)
	{
		if (wordIndex % 5000 == 0)
			printf("Appending associated forms: %03I64d%%:%09d\r", (__int64)(((__int64)100) * wordIndex / source.m.size()), wordIndex);
		if (im.preferredViterbiForms.empty())
			break;
		im.originalPreferredViterbiForm = im.preferredViterbiForms[0];
		bool viterbiFormMatchedWord = false;
		if (im.formsSize()>1)
			for (int vf : im.preferredViterbiForms)
			{
				wstring formName = Forms[im.getFormNum(vf)]->name;
				// if the form is the word itself, then actually match all forms.
				if (formName == im.word->first)
				{
					//lplog(LOG_ERROR, L"%d:viterbi extended %s to all forms (word match)", wordIndex, formName.c_str());
					viterbiFormMatchedWord = true;
					im.preferredViterbiForms.clear();
					for (unsigned int f = 0; f < im.formsSize(); f++)
						im.preferredViterbiForms.push_back(f);
					break;
				}
			}
		if (!viterbiFormMatchedWord)
		{
			for (int vf : im.preferredViterbiForms)
			{
				wstring formName = Forms[im.getFormNum(vf)]->name;
				unordered_map<wstring, vector <wstring>>::iterator mi = viterbiAssociationMap.find(formName);
				if (mi != viterbiAssociationMap.end())
				{
					for (wstring associatedForm : mi->second)
					{
						int formOffset = im.queryForm(FormsClass::findForm(associatedForm));
						if (formOffset >= 0)
						{
							im.preferredViterbiForms.push_back(formOffset);
							//lplog(LOG_ERROR, L"%d:viterbi subClass/superClass extended %s->%s", wordIndex, formName.c_str(), associatedForm.c_str());
						}
					}
					break;
				}
			}
			// never exclude the word itself from possibile match
			int selfForm = FormsClass::findForm(im.word->first);
			if (selfForm >= 0)
			{
				int formOffset = im.queryForm(selfForm);
				if (formOffset >= 0)
				{
					im.preferredViterbiForms.push_back(formOffset);
					//lplog(LOG_ERROR, L"%d:viterbi extended to word match %s", wordIndex, im.word->first.c_str());
				}
			}
		}
		// if viterbi form is an adjective, or noun, and the word is a verb gerund (missing, driving) then also include the verb form
		if ((std::find(im.preferredViterbiForms.begin(), im.preferredViterbiForms.end(), adjectiveForm) != im.preferredViterbiForms.end() ||
			   std::find(im.preferredViterbiForms.begin(), im.preferredViterbiForms.end(), nounForm) != im.preferredViterbiForms.end()) &&
			im.queryForm(verbForm)>=0 && (im.word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE))
		{
			im.preferredViterbiForms.push_back(verbForm);
		}
		wordIndex++;
	}
}

void getInternalViterbiInfo(Source &source, int viterbiOriginalTagIndex, int wordSourceIndex, wstring &prevTag, wstring &tag, int &tagTransitionCount, int &wordTagCount,
	vector <wstring> &tags, //vector <wstring> &vocab,
	DIYDiskArray<int> &pathMatrix,
	unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap)
{
	prevTag = tags[pathMatrix.get(viterbiOriginalTagIndex,wordSourceIndex)];
	tag = tags[viterbiOriginalTagIndex];
	// Compute smoothed transition probability
	tagTransitionCount = 0;
	unordered_map <wstring, int>::iterator ti = tagTransitionCountsMap.find(prevTag + L" " + tag);
	if (ti != tagTransitionCountsMap.end())
		tagTransitionCount = ti->second;
	// Compute smoothed emission probability
	wordTagCount = 0;
	unordered_map <wstring, int>::iterator ei = wordTagCountsMap.find(tag + L" " + source.m[wordSourceIndex].word->first);
	if (ei != wordTagCountsMap.end())
		wordTagCount = ei->second;
}

void compareViterbiAgainstStructuredTagging(Source &source, 
	vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, DIYDiskArray<double> &probabilityMatrix, DIYDiskArray<int> &pathMatrix,
	unordered_map <wstring, int> &wordSourceIndexLookup, 
	unordered_map <wstring, int> &tagLookup,
	vector <wstring> &tags, //vector <wstring> &vocab,
	unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap,
	JNIEnv *env,bool pcfg)
{
	wstring winnerFormsString;
	int viterbiMismatchesSetToSeparator = 0, viterbiMismatchesNotSet = 0, viterbiMismatchesIllegal = 0, viterbiMismatchesNotWinner = 0, wordSourceIndex = 0;
	int totalNumWinnerForms = 0, totalNumForms = 0, totalViterbiSpecifiedForms=0, totalViterbiPathViolatedForms=0;
	double averageViterbiProbability = 0;
	unordered_map<wstring, int> winnerViolationFormCountMap,winnerViolationWordCountMap;
	int pathViolations = 0;
	int stanfordNotIdentifiedNum=0, stanfordIsLPWinnerNum=0, stanfordIsViterbiWinnerNum=0;
	if (!myquery(&source.mysql, L"LOCK TABLES stanfordPCFGParsedSentences WRITE")) // moved out parseSentence (actually in foundParseSentence and setParsedSentence) for performance
		return;
	for (vector <WordMatch>::iterator im = source.m.begin(), imEnd = source.m.end(); im != imEnd; im++, wordSourceIndex++)
	{
		if (wordSourceIndex % 5000 == 0)
			printf("Comparing viterbi against structured tagging: %03I64d%%:%09d\r", (__int64)(((__int64)100) * wordSourceIndex / source.m.size()), wordSourceIndex);
		averageViterbiProbability += im->preferredViterbiProbability;
		totalNumWinnerForms += im->getNumWinners();
		totalNumForms += im->formsSize();
		totalViterbiSpecifiedForms += im->preferredViterbiForms.size();
		if (find(im->preferredViterbiForms.begin(), im->preferredViterbiForms.end(), -2) != im->preferredViterbiForms.end())
		{
			if (im->forms.isSet(quoteForm) || im->isTopLevel())
				continue;
			lplog(LOG_ERROR, L"%d:preferredViterbiForm on word %s is set to separator", wordSourceIndex, im->word->first.c_str());
			viterbiMismatchesSetToSeparator++;
			continue;
		}
		if (im->preferredViterbiForms.empty())
		{
			lplog(LOG_ERROR, L"%d:preferredViterbiForm on word %s is not set", wordSourceIndex, im->word->first.c_str());
			viterbiMismatchesNotSet++;
			continue;
		}
		bool winnerFound = false, locallyPreferredPathInPreferred = false;
		int preferredViterbiCurrentTagOfHighestProbabilityOffset=im->queryForm(im->preferredViterbiCurrentTagOfHighestProbability);
		for (int preferredViterbiForm : im->preferredViterbiForms)
		{
			if (preferredViterbiForm > (signed)im->formsSize())
			{
				lplog(LOG_ERROR, L"%d:preferredViterbiForm on word %s is set to an illegal form offset (%d out of %d possible)", wordSourceIndex, im->word->first.c_str(), preferredViterbiForm, im->formsSize());
				viterbiMismatchesIllegal++;
				continue;
			}
			if (preferredViterbiForm == preferredViterbiCurrentTagOfHighestProbabilityOffset)
				locallyPreferredPathInPreferred = true;
			if (im->isWinner(preferredViterbiForm))
				winnerFound = true;
		}
		if (!locallyPreferredPathInPreferred)
			totalViterbiPathViolatedForms++;
		if (!winnerFound && im->queryWinnerForm(im->preferredViterbiCurrentTagOfHighestProbability) >= 0)
			winnerFound = true;
		int viterbiOriginalTagIndex = tagLookup[Forms[im->getFormNum(im->originalPreferredViterbiForm)]->name];
		if (im->preferredViterbiMaximumProbability != probabilityMatrix.get(viterbiOriginalTagIndex, wordSourceIndex))
			pathViolations++;
		if (!winnerFound)
		{
			vector <double> best_probs,bestProbabilitiesAtWordOnly;
			vector <wstring> currentTagOfHighestProbabilities;
			// get top 3 tags - this rescans all tags and gives them in best order.
			// the path in pathMatrix sometimes bypasses the highest probability tag for a particular spot, because the transition to the NEXT tag is not optimal (pathMatrix is the entire optimized path)
			for (auto const&[tag, tagIndex] : tagLookup)
			{
				// pm=probability of path up to this point
				// ttpm=what is the probability that each tag transitions into this one?
				// wtpm=what is the probability that the word has this tag?
				// pathMatrixColumn += L" " + std::to_wstring(pathMatrix[prevTagIndex][viterbiOriginalTagIndex]);
				//double ttpm = tagTransitionProbabilityMatrix[prevTagIndex][tagIndex];
				//double wtpm = wordTagProbabilityMatrix[tagIndex][wordVocabIndex];
				double prob = probabilityMatrix.get(tagIndex,wordSourceIndex);
				if (prob > (double)-std::numeric_limits<double>::infinity())
				{
					int I;
					for (I = 0; I < best_probs.size(); I++)
						if (prob > best_probs[I])
							break;
					best_probs.insert(best_probs.begin()+I,prob);
					bestProbabilitiesAtWordOnly.insert(bestProbabilitiesAtWordOnly.begin()+I,prob / ((wordSourceIndex>0) ? probabilityMatrix.get(pathMatrix.get(tagIndex,wordSourceIndex),wordSourceIndex - 1):1)); // CHANGE from log add to multiplication
					currentTagOfHighestProbabilities.insert(currentTagOfHighestProbabilities.begin()+I,tag);
				}
			}
			if (best_probs[0] != probabilityMatrix.get(viterbiOriginalTagIndex,wordSourceIndex))
			{
				lplog(LOG_ERROR, L"%d:path free is best=%f(%s) != %f(%s) (winner=%s)", wordSourceIndex, best_probs[0], currentTagOfHighestProbabilities[0].c_str(),
					probabilityMatrix.get(viterbiOriginalTagIndex,wordSourceIndex), im->word->second.Form(im->originalPreferredViterbiForm)->name.c_str(),
					im->winnerFormString(winnerFormsString).c_str());
				lplog(LOG_ERROR, L"%d:forward/backward %f previous=%s current=%s", wordSourceIndex, im->preferredViterbiMaximumProbability, Forms[im->preferredViterbiPreviousTagOfHighestProbability]->name.c_str(), Forms[im->preferredViterbiCurrentTagOfHighestProbability]->name.c_str());
			}
			//else
			if (!winnerFound)
			{
				int duplicateSkip = 0;
				wstring contextSentence= getContext(source, wordSourceIndex, false,duplicateSkip),parse;
				if (parseSentence(source, env, contextSentence, parse, pcfg) < 0)
					lplog(LOG_FATAL_ERROR, L"Parse failed.");
				vector <wstring> posList;
				wstring out, originalWord = source.getOriginalWord(wordSourceIndex, out, false, false);
				findLPPOSEquivalents(contextSentence, parse, originalWord, posList,duplicateSkip,pcfg);
				contextSentence = getContext(source, wordSourceIndex, true, duplicateSkip);
				bool stanfordNotIdentified = posList.empty(), stanfordIsLPWinner=false,stanfordIsViterbiWinner=false;
				wstring viterbiForms;
				for (int preferredViterbiForm : im->preferredViterbiForms)
				{
					if (std::find(posList.begin(), posList.end(), im->word->second.Form(preferredViterbiForm)->name) != posList.end())
						stanfordIsViterbiWinner = true;
					viterbiForms += im->word->second.Form(preferredViterbiForm)->name + L" ";
				}
				lplog(LOG_ERROR, L"%d:context %s [%s]", wordSourceIndex, contextSentence.c_str(),parse.c_str());
				winnerViolationFormCountMap[im->winnerFormString(winnerFormsString,false)]++;
				winnerViolationWordCountMap[im->word->first]++;
				wstring prevTag, tag;
				int tagTransitionCount,wordTagCount;
				wstring winnerWordTagProbability;
				vector <int> winnerForms;
				im->getWinnerForms(winnerForms);
				if (winnerForms.size() > 0)
					winnerWordTagProbability = L"(";
				for (int wf:winnerForms)
				{
					if (std::find(posList.begin(), posList.end(), Forms[wf]->name) != posList.end())
						stanfordIsLPWinner = true;
					auto tli = tagLookup.find(Forms[wf]->name);
					if (tli != tagLookup.end())
					{
						int path = pathMatrix.get(tli->second, wordSourceIndex);
						double winnerProb = probabilityMatrix.get(tli->second, wordSourceIndex) / ((wordSourceIndex > 0 && path>=0) ? probabilityMatrix.get(path, wordSourceIndex - 1):1);
						wstring wtp;
						wchar_t ctmp[32];
						swprintf(ctmp, 32, L"%f", winnerProb);
						wtp = ctmp;
						if (winnerForms.size() > 1)
							winnerWordTagProbability += L"["+ Forms[wf]->name + L"="+ wtp + L"]"; 
						else
							winnerWordTagProbability += wtp;
					}
				}
				if (winnerForms.size() > 0)
					winnerWordTagProbability += L")";
				getInternalViterbiInfo(source, viterbiOriginalTagIndex, wordSourceIndex, prevTag, tag, tagTransitionCount, wordTagCount,tags, pathMatrix,wordTagCountsMap, tagTransitionCountsMap);
				lplog(LOG_ERROR, L"stanfordNotIdentified = %s stanfordIsLPWinner=%s stanfordIsViterbiWinner=%s", (stanfordNotIdentified) ? L"true" : L"false", (stanfordIsLPWinner) ? L"true" : L"false", (stanfordIsViterbiWinner) ? L"true" : L"false");
				if (stanfordNotIdentified) stanfordNotIdentifiedNum++;
				if (stanfordIsLPWinner) stanfordIsLPWinnerNum++;
				if (stanfordIsViterbiWinner) stanfordIsViterbiWinnerNum++;
				lplog(LOG_ERROR, L"%d:preferredViterbiForms %s is/are not among the winner forms %s%s for word %s [%f tagTransition=%f (#previousTag[%s]=%d #transition=%d) wordTagProbability=%f (#tag[%s]=%d #wordTag=%d)]",
					wordSourceIndex, viterbiForms.c_str(), // %d:preferredViterbiForms %s 
					im->winnerFormString(winnerFormsString).c_str(), winnerWordTagProbability.c_str(), im->word->first.c_str(), // is/are not among the winner forms %s%s for word %s 
					im->preferredViterbiProbability, 
					tagTransitionProbabilityMatrix[pathMatrix.get(viterbiOriginalTagIndex,wordSourceIndex)][viterbiOriginalTagIndex], 
					//(tagTransitionCount + alpha) / (tagCountsMap[prevTag] + alpha * tags.size()),  // check
					prevTag.c_str(), tagCountsMap[prevTag], tagTransitionCount,
					wordTagProbabilityMatrix[viterbiOriginalTagIndex][wordSourceIndexLookup[source.m[wordSourceIndex].word->first]], // wordTagProbability
					//(wordTagCount + alpha) / (tagCountsMap[tag] + alpha * vocab.size()), // check
					tag.c_str(), tagCountsMap[tag], wordTagCount);
				viterbiMismatchesNotWinner++;
			}
			// 172 letter  is / are not among the winner forms personal_pronoun_nominative[0]  for word i???
			// measurement_abbreviation  is/are not among the winner forms preposition[0]  for word in
			if (!winnerFound)
			{
				for (int p = 0; p < 5 && p < ((int)best_probs.size()); p++)
				{
					wstring prevTag, tag;
					int tagTransitionCount, wordTagCount;
					int currentTagIndex = tagLookup[currentTagOfHighestProbabilities[p]];
					getInternalViterbiInfo(source, currentTagIndex, wordSourceIndex, prevTag, tag, tagTransitionCount, wordTagCount,
						tags, pathMatrix, wordTagCountsMap, tagTransitionCountsMap);
					lplog(LOG_ERROR, L"%d:preferredViterbiTags %d: %s [%f tagTransition=%f (#previousTag[%s]=%d #transition=%d) wordTagProbability=%f (#tag[%s]=%d #wordTag=%d)]", 
						wordSourceIndex, p, currentTagOfHighestProbabilities[p].c_str(), bestProbabilitiesAtWordOnly[p],
						tagTransitionProbabilityMatrix[pathMatrix.get(currentTagIndex,wordSourceIndex)][currentTagIndex],
						//(tagTransitionCount + alpha) / (tagCountsMap[prevTag] + alpha * tags.size()),  // check
						prevTag.c_str(), tagCountsMap[prevTag], tagTransitionCount,
						wordTagProbabilityMatrix[currentTagIndex][wordSourceIndexLookup[source.m[wordSourceIndex].word->first]],
						//(wordTagCount + alpha) / (tagCountsMap[tag] + alpha * vocab.size()), // check
						tag.c_str(), tagCountsMap[tag], wordTagCount);
				}
			}
		}
		//else
		//	lplog(LOG_ERROR, L"%d:preferredViterbiForms %s is/are among the winner forms %s for word %s [%f]",
		//		wordSourceIndex, viterbiForms.c_str(), im->winnerFormString(winnerForms).c_str(), im->word->first.c_str(), im->preferredViterbiProbability);
	}
	source.unlockTables();
	map<int, wstring, std::greater<int>> orderedFormCountMap;
	for (auto const&[winnerForm, count] : winnerViolationFormCountMap)
		orderedFormCountMap[count] = winnerForm;
	for (auto const&[count, winnerForm] : orderedFormCountMap)
		lplog(LOG_ERROR, L"wrong viterbi matched winnerForms %s %d (%d%%)", winnerForm.c_str(), count, count * 100 / viterbiMismatchesNotWinner);

	map<int, wstring, std::greater<int>> orderedWordCountMap;
	for (auto const&[winnerWord, count] : winnerViolationWordCountMap)
		orderedWordCountMap[count] = winnerWord;
	for (auto const&[count, winnerWord] : orderedWordCountMap)
		lplog(LOG_ERROR, L"wrong viterbi matched winnerWord %s %d (%d%%)", winnerWord.c_str(), count, count * 100 / viterbiMismatchesNotWinner);

	int viterbiMismatches = viterbiMismatchesSetToSeparator + viterbiMismatchesNotSet + viterbiMismatchesIllegal + viterbiMismatchesNotWinner;
	if (viterbiMismatches > 0)
	{
		if (viterbiMismatchesSetToSeparator > 0)
			lplog(LOG_ERROR, L"preferredViterbiForm is set to separator %d times (%d%%)", viterbiMismatchesSetToSeparator, viterbiMismatchesSetToSeparator * 100 / source.m.size());
		if (viterbiMismatchesNotSet > 0)
			lplog(LOG_ERROR, L"preferredViterbiForm is not set %d times (%d%%)", viterbiMismatchesNotSet, viterbiMismatchesNotSet * 100 / source.m.size());
		if (viterbiMismatchesIllegal > 0)
			lplog(LOG_ERROR, L"preferredViterbiForm is set to an illegal form offset %d times (%d%%)", viterbiMismatchesIllegal, viterbiMismatchesIllegal * 100 / source.m.size());
		if (viterbiMismatchesNotWinner > 0)
			lplog(LOG_ERROR, L"preferredViterbiForm is not among the winner forms %d times (%2.3f%%)", viterbiMismatchesNotWinner, ((double)viterbiMismatchesNotWinner * 100) / source.m.size());
		if (viterbiMismatchesSetToSeparator > 0 || viterbiMismatchesNotSet > 0 || viterbiMismatchesIllegal > 0)
			lplog(LOG_ERROR, L"preferredViterbiForm error %d times (%2.3f%%)", viterbiMismatches, ((double)viterbiMismatches * 100) / source.m.size());
		if (pathViolations >0)
			lplog(LOG_ERROR, L"preferredViterbi pathViolations=%d addedForms=%d (%d%%)", pathViolations, totalViterbiPathViolatedForms, 100 * pathViolations / source.m.size());
		lplog(LOG_ERROR, L"preferredViterbi form %% of total winners: %f%% of total forms %f%%", 100.0 * totalViterbiSpecifiedForms / totalNumWinnerForms, 100.0 * (totalViterbiSpecifiedForms + totalViterbiPathViolatedForms) / totalNumForms);
		lplog(LOG_ERROR, L"preferredViterbi forms (%d) per word=%f -> viterbiForms (%d) per word=%f", totalNumForms, totalNumForms*1.0 / source.m.size(), (totalViterbiSpecifiedForms + totalViterbiPathViolatedForms), (totalViterbiSpecifiedForms + totalViterbiPathViolatedForms)*1.0 / source.m.size());
		lplog(LOG_ERROR, L"stanfordNotIdentifiedNum=%d(%d%%) stanfordIsLPWinnerNum=%d(%d%%) stanfordIsViterbiWinnerNum=%d(%d%%)", 
			stanfordNotIdentifiedNum, (stanfordNotIdentifiedNum*100)/viterbiMismatchesNotWinner,
			stanfordIsLPWinnerNum, (stanfordIsLPWinnerNum * 100) / viterbiMismatchesNotWinner,
			stanfordIsViterbiWinnerNum, (stanfordIsViterbiWinnerNum * 100) / viterbiMismatchesNotWinner);
	}
}

// Decode sequences
// wordCountLimit - use words that occur across the corpus no less than this number
void tagFromSource(Source &source, vector <wstring> &model,int wordCountLimit, JNIEnv *env,bool compare)
{
	unordered_map <wstring, int> wordTagCountsMap, tagTransitionCountsMap, tagCountsMap;
	loadModel(model, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
	printf("constructing transition and emission matrices                                              \r");
	vector <wstring> tags;
	for (auto const& ic : tagCountsMap)
		tags.push_back(ic.first);
	// Transition matrix: the probability of state x+1 given state x (bigram case).  state=tag
	vector<vector<double>> tagTransitionProbabilityMatrix = constructTagTransitionProbabilityMatrix(tagTransitionCountsMap, tagCountsMap, tags);
	// Emission matrix: the probability that a word is tagged as a certain tag
	vector <wstring> vocab = generateVocabFromSource(source, wordCountLimit);
	vector<vector<double>> wordTagProbabilityMatrix = constructWordTagProbabilityMatrix(wordTagCountsMap, tagCountsMap, tags, vocab);
	DIYDiskArray<double> probabilityMatrix((source.m.size() > 12000000) ? L"M:\\caches\\ViterbiProbabilityMatrixArray.tmp" : NULL);
	DIYDiskArray<int> pathMatrix((source.m.size() > 25000000) ? L"M:\\caches\\ViterbiPathMatrixArray.tmp":NULL);
	// Decode
	unordered_map <wstring, int> vocabReverseLookup;
	// Initialize start probabilities
	initViterbiStartProbabilities(source.m.size(), source.m[0].word->first,vocab, tags, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix, pathMatrix, vocabReverseLookup);
	unordered_map <wstring, int> tagLookup;
	int tagNum = 0;
	for (wstring tag : tags)
		tagLookup[tag] = tagNum++;
	forwardFromSource(source, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix,pathMatrix,tags, vocabReverseLookup, tagLookup);
	if (!backwardFromSource(source, tags, tagLookup, probabilityMatrix, pathMatrix))
	{
		appendAssociatedFormsToViterbiTags(source);
		if (compare)
			compareViterbiAgainstStructuredTagging(source,
				tagTransitionProbabilityMatrix, wordTagProbabilityMatrix,
				probabilityMatrix, pathMatrix,
				vocabReverseLookup,
				tagLookup,
				tags,//vocab,
				wordTagCountsMap, tagTransitionCountsMap, tagCountsMap,
				env,true);
	}
}

void createModelFromSource(Source &source, vector <wstring> &model)
{
	wstring modelPath = source.sourcePath+L".model.txt";
	if (_waccess(modelPath.c_str(), 0) != 0)
	{
		printf("creating model                                                \r");
		unordered_map <wstring, int> wordTagCountsMap, tagTransitionCountsMap, tagCountsMap;
		trainModelFromSource(source, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
		model = writeModelFile(modelPath, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
	}
	else
	{
		printf("reading model                                                \r");
		model = readModelFile(modelPath);
	}
}

void testViterbiFromSource(Source &source)
{
	vector <wstring> model;
	createModelFromSource(source, model);
	JavaVM *vm;
	JNIEnv *env;
	createJavaVM(vm, env);
	tagFromSource(source, model, 1, env, true);
	destroyJavaVM(vm);
}

