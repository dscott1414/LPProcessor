/*
	hmm.cpp - first-party HMM Viterbi POS tagger trained on LP winner forms, plus Stanford JNI compare.

	Overview:
		Builds tag-transition and word-tag count maps from a parsed cSource (or a
		sidecar .model.txt), constructs add-1-smoothed probability matrices, then
		runs a Viterbi forward/backward pass that writes preferredViterbiForms on
		each cWordMatch.  When compare is true, mismatches are re-parsed with the
		Stanford PCFG via JNI and mapped through pennMapToLP.

	Pipeline position:
		Optional after parse.  testViterbiFromSource() is the usual entry
		(specials_main step 8).  Not invoked from main.cpp's processSource().

	Key entry points:
		- testViterbiFromSource() - train or load model, spin up JVM, tag + compare
		- createModelFromSource() / trainModelFromSource() - count maps from winners
		- tagFromSource() - matrices + Viterbi + optional Stanford compare
		- parseSentence() / foundParsedSentence() / setParsedSentence() - Stanford
		  PCFG cache in stanfordPCFGParsedSentences (hash of the sentence only)

	Key data structures / globals:
		- alpha - additive smoothing for transitions (and emissions if
		  USE_ALPHA_FOR_WORDTAG is defined; it is not)
		- pennMapToLP - Penn Bank tag -> LP form-name list
		- startTag / viterbiAssociationMap - Viterbi start state and form expansion
		- wordTagCountsMap / tagTransitionCountsMap / tagCountsMap - trained counts

	Dependencies:
		JNI + Stanford ParserDemo jar at F:\lp\Stanford\..., MySQL
		stanfordPCFGParsedSentences / words / wordforms / forms, DIYDiskArray
		spilling to M:\caches when the source is huge.

	Notes / gotchas:
		- Emission matrix stays 0 for unseen (tag,word) unless USE_ALPHA_FOR_WORDTAG.
		- Forward uses product + a renormalizing probMult instead of log-sum, so it
		  relies on the isnan() guard (std::isnan) to catch underflow to NaN.
		- Lookup of cached parses is by sentencehash, confirmed against the actual
		  (escaped) sentence text so a hash collision cannot return the wrong parse.
		- setParsedSentence quote-normalizes parse/sentence (so the hash matches
		  foundParsedSentence's) and then escapes both (escaped()/escapeStr) before
		  interpolating into SQL.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "word.h"
#include "ontology.h"
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
bool unlockTables(MYSQL& mysql);

// Create a JNI VM for the Stanford ParserDemo.
// Batch B11: the classpath comes from envConfig's getStanfordClasspath()
// (LP_STANFORD_CLASSPATH, defaulting under getMainDir()) rather than being a
// hardcoded F:\lp Windows path with ';' separators.
// Returns JNI_CreateJavaVM's jint (0 = JNI_OK).
int createJavaVM(JavaVM*& vm, JNIEnv*& env)
{
	JavaVMOption options[5];
	memset(&options, 0, sizeof(options));
	static const std::string stanfordClassPathOption = "-Djava.class.path=" + getStanfordClasspath();
	options[0].optionString = (char *)stanfordClassPathOption.c_str();
	options[1].optionString = (char*)"-Xms10m"; // 10MB initial heap
	options[2].optionString = (char*)"-Xmx3g"; // 3GB max heap
	options[3].optionString = (char*)"-mx2400m"; // 2.4GB
	options[4].optionString = (char*)"-server"; // Selects server application runtime optimizations. The directory server will take longer to start and �warm up� but will be more aggressively optimized to produce higher throughput.

	JavaVMInitArgs vm_args;
	vm_args.version = JNI_VERSION_1_8;
	vm_args.nOptions = 5;
	vm_args.options = options;
	vm_args.ignoreUnrecognized = 1;
	jint res = JNI_CreateJavaVM(&vm, (void**)&env, &vm_args);
	return res;
}

// https://www.clips.uantwerpen.be/pages/mbsp-tags
unordered_map<lpwstring, vector<lpwstring>> pennMapToLP = {
{ u"CC",{ u"conjunction" }},
{ u"CD",{u"numeral_cardinal" }},
{ u"DT",{u"determiner" }},
{ u"EX",{u"there" }},
{ u"FW",{u"" }},
{ u"IN",{u"preposition",u"conjunction" }},
{ u"JJ",{u"adjective" }},
{ u"JJR",{u"adjective" }},
{ u"JJS",{u"adjective" }},
{ u"LS",{u"|||" }},
{ u"MD",{u"modal_auxiliary" }},
{ u"NN",{u"noun" }},
{ u"NNS",{u"noun" }},
{ u"NNP",{u"Proper Noun" }},
{ u"NNPS",{u"Proper Noun" }},
{ u"PDT",{u"predeterminer" }},
{ u"POS",{u"" }},
{ u"PRP",{u"personal_pronoun_accusative",u"personal_pronoun_nominative",u"personal_pronoun",u"reflexive_pronoun" }},
{ u"PRP$",{u"possessive_determiner" }},
{ u"RB",{u"adverb" }},
{ u"RBR",{u"adverb" }},
{ u"RBS",{u"adverb" }},
{ u"RP",{u"particle" }},
{ u"SYM",{u"symbol" }},
{ u"TO",{u"to" }},
{ u"UH",{u"interjection" }},
{ u"VB",{u"verb" }},
{ u"VBD",{u"verb" }},
{ u"VBG",{u"verb" }},
{ u"VBN",{u"verb" }},
{ u"VBP",{u"verb" }},
{ u"VBZ",{u"verb" }},
{ u"WDT",{u"relativizer",u"interrogative_determiner",u"demonstrative_determiner",u"which",u"what",u"whose" }}, // wh-determiner: which, whatever, whichever 
{ u"WP",{u"interrogative_pronoun",u"what" }}, // wh-pronoun, personal:	what, who, whom 
{ u"WP$",{u"interrogative_determiner",u"whose",u"relativizer" }}, // wh-pronoun, possessive:	whose, whosever 
{ u"WRB",{u"adverb",u"relativizer",u"how"} }, // wh-adverb:	where, when - LP does not distinguish WRB used to introduce a relative phrase (which is true) and its adverbial use (also true)
{ u".",{ }}, // punctuation mark, sentence closer	.; ? *
{ u",",{ }}, //	punctuation mark, comma	,
{ u":",{ }}, // punctuation mark, colon :
{ u"(",{ }}, // contextual separator, left paren (
{ u")",{ }}, //	contextual separator, right paren )
{ u"''",{ }}, //	not listed in standard PennBank tag list but emitted by Stanford
{ u"``",{ }}, //	not listed in standard PennBank tag list but emitted by Stanford
{ u"$",{ }} //	not listed in standard PennBank tag list but emitted by Stanford
};

// Map every single- and double-quote character to ASCII '"' so the string can
// sit inside a single-quoted SQL literal.  Does not escape remaining apostrophes
// that cWord::isSingleQuote does not classify.
lpwstring replaceQuotes(lpwstring ws)
{
	lpwstring replacement;
	replacement.reserve(ws.length());
	for (lpchar_t wsc : ws)
		if (cWord::isSingleQuote(wsc) || cWord::isDoubleQuote(wsc))
			replacement += u'"';
		else
			replacement += wsc;
	return replacement;
}

// Look up a Stanford PCFG parse by hash of the (truncated, quote-normalized) sentence,
// confirmed against the actual sentence text so a hash collision cannot return the
// wrong parse (sentencehash narrows the index scan; sentence is the correctness check).
// Truncates to 2999 chars.  Out: parse, with " rewritten to ' and a trailing space.
// Returns true if non-empty.  lockTable: take/release a READ lock around the select
// (callers may lock higher up).
bool foundParsedSentence(cSource& source, lpwstring sentence, lpwstring& parse, bool lockTable)
{
	if (lockTable)
	{
		printf("Waiting for read lock.                        \r");
		if (!myquery(&source.mysql, u"LOCK TABLES stanfordPCFGParsedSentences READ")) return false; // moved out to higher level for performance
		printf("Acquired lock. Selecting sentence.\r");
	}
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (sentence.length() >= 3000)
		sentence = sentence.substr(0, 2999);
	sentence = replaceQuotes(sentence);
	size_t sentencehash = std::hash<lpwstring>{}(sentence);
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select parse from stanfordPCFGParsedSentences where sentencehash = %I64d and sentence = '%s'", (int64_t)sentencehash, escaped(sentence).c_str()); // must be %I64d because of BIGINT signed considerations
	MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow = NULL;
	parse.erase();
	if (myquery(&source.mysql, qt, result) && (sqlrow = mysql_fetch_row(result)))
	{
		mTW(sqlrow[0], parse);
		if (parse.empty())
			lplog(LOG_ERROR, u"Parse is empty to %s", sentence.c_str());
		parse += u" ";
		std::replace(parse.begin(), parse.end(), u'"', u'\'');
	}
	mysql_free_result(result);
	if (lockTable)
	{
		unlockTables(source.mysql); // moved out to higher level for performance
		printf("Released lock.                           \r");
	}
	return parse.length() > 0;
}

// INSERT the PCFG parse.  Sentence/parse are quote-normalized (so the hash and any
// later lookup agree with foundParsedSentence) and then escaped (escapeStr, via
// escaped()) before being interpolated into VALUES('%s','%s').  Returns 0, or -1
// on lock/query fail.  On query fail the WRITE lock is released even if this call
// did not take it.
int setParsedSentence(cSource& source, lpwstring sentence, lpwstring parse, bool lockTable)
{
	if (lockTable)
	{
		printf("Waiting for write lock.                           \r");
		if (!myquery(&source.mysql, u"LOCK TABLES stanfordPCFGParsedSentences WRITE")) // moved out to higher level for performance
			return -1;
		printf("Acquired lock. Inserting sentence in table.\r");
	}
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (sentence.length() >= 3000)
		sentence = sentence.substr(0, 2999);
	sentence = replaceQuotes(sentence);
	parse = replaceQuotes(parse);
	size_t sentencehash = std::hash<lpwstring>{}(sentence);
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"insert stanfordPCFGParsedSentences (parse,sentence,sentencehash) VALUES('%s','%s',%I64d)", escaped(parse).c_str(), escaped(sentence).c_str(), (int64_t)sentencehash);
	if (!myquery(&source.mysql, qt, true))
	{
		unlockTables(source.mysql);
		printf("Released lock.                           \r");
		return -1;
	}
	if (lockTable)
	{
		unlockTables(source.mysql);
		printf("Released lock.                           \r");
	}
	return 0;
}

// Parse via Stanford ParserDemo.parseSentence (static).  If pcfg, try/write the
// MySQL cache first.  Returns 0 or a negative JNI/cache code (-1..-7).
// Static jclass/jmethodID are cached without NewGlobalRef (invalid after detach).
// Local JNI refs are not deleted.  Logs "Did not find sentence!" on every cache miss.
int parseSentence(cSource& source, JNIEnv* env, lpwstring sentence, lpwstring& parse, bool pcfg, bool lockTable)
{
	static jclass parserDemoClass;
	static jmethodID parseSentenceMethod;
	static bool initialized = false;
	if (pcfg && foundParsedSentence(source, sentence, parse, lockTable))
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
	lplog(LOG_ERROR, u"Did not find sentence! %s", sentence.c_str());
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
	const char* parseSentenceReturnString = env->GetStringUTFChars((jstring)result, &isCopy);
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
	if (pcfg && setParsedSentence(source, sentence, parse, lockTable) < 0)
		return -7;
	return 0;
}

// Map the Stanford tag of originalWord onto LP form names via pennMapToLP.
// pcfg: walk "(TAG word)" trees; else " word_TAG " tagger output.
// duplicateSkip skips earlier occurrences of the same surface word in the sentence.
// 's is stripped only in the pcfg branch.  originalWord[length-2] is unguarded
// if the word is shorter than 2.  Header declares an extra JNIEnv* that is unused.
int findLPPOSEquivalents(lpwstring sentence, lpwstring& parse, lpwstring originalWord, vector<lpwstring>& posList, int duplicateSkip, bool pcfg)
{
	parse = u" " + parse; // take care of the edge case where the match is at the beginning
	// pcfg output:
	// parse=(ROOT (PRN (: ;) (S (NP (NP (NP (QP (CC and) (CD Bunny))) (, ,) (CC and) (NP (NNP Bobtail)) (, ,)) (CC and) (NP (NNP Billy))) (VP (VBD were) (ADVP (RB always)) (VP (VBG doing) (NP (JJ *) (NN something)))))))
	if (pcfg)
	{
		if (originalWord.length() >= 2 && originalWord[originalWord.length() - 2] == u'\'' && originalWord[originalWord.length() - 1] == u's')
			originalWord.erase(originalWord.length() - 2);
		originalWord = u" " + originalWord + u")";
		size_t wow = parse.find(originalWord);
		for (int dup = 0; dup < duplicateSkip; dup++)
		{
			if (wow != lpwstring::npos)
				wow = parse.find(originalWord, wow + 1);
		}
		if (wow != lpwstring::npos)
		{
			auto firstparen = parse.rfind(u'(', wow);
			if (firstparen != lpwstring::npos)
			{
				lpwstring partofspeech = parse.substr(firstparen + 1, wow - firstparen - 1);
				auto lpPOS = pennMapToLP.find(partofspeech);
				if (lpPOS != pennMapToLP.end())
				{
					posList = lpPOS->second;
				}
				else
					lplog(LOG_ERROR, u"Part of Speech %s not found.", partofspeech.c_str());
			}
		}
	}
	else
	{
		// tagger output:
		// ;_: and_CC bunny_NN ,_, and_CC bobtail_NN ,_, and_CC billy_NNP were_VBD always_RB doing_VBG something_NN 
		originalWord = u" " + originalWord + u"_";
		size_t wow = parse.find(originalWord);
		if (wow == lpwstring::npos)
		{
			transform(originalWord.begin(), originalWord.end(), originalWord.begin(), (int(*)(int)) tolower);
			wow = parse.find(originalWord);
		}
		for (int dup = 0; dup < duplicateSkip; dup++)
		{
			if (wow != lpwstring::npos)
				wow = parse.find(originalWord, wow + 1);
		}
		if (wow != lpwstring::npos)
		{
			wow += originalWord.length();
			auto nextspace = parse.find(u' ', wow);
			if (nextspace != lpwstring::npos)
			{
				lpwstring partofspeech = parse.substr(wow, nextspace - wow);
				auto lpPOS = pennMapToLP.find(partofspeech);
				if (lpPOS != pennMapToLP.end())
				{
					posList = lpPOS->second;
				}
				else
					lplog(LOG_ERROR, u"Part of Speech %s not found.", partofspeech.c_str());
			}
		}
	}
	return 0;
}

// Shutdown the VM.
// DestroyJavaVM.  Does not null the caller's pointer.
void destroyJavaVM(JavaVM* vm)
{
	vm->DestroyJavaVM();
}

// Vocabulary = distinct source.m word strings that occur at least min_cnt times, sorted.
vector <lpwstring> generateVocabFromSource(cSource& source, int min_cnt = 2)
{
	//Generate vocabulary
	unordered_map <lpwstring, int> vocabAll;

	for (cWordMatch& im : source.m)
		vocabAll[im.word->first] += 1;
	vector <lpwstring> vocabvector;
	// Remove words appearing only once
	for (auto const& [word, count] : vocabAll)
		if (count >= min_cnt)
			vocabvector.push_back(word);

	// Sort 
	sort(vocabvector.begin(), vocabvector.end());

	return vocabvector;
}

lpwstring startTag = u"--s--";
// Count winner-form transitions and emissions.  Spaces in tag/word names become '*'.
// Words that never had a winner form are back-filled from words/wordforms/forms
// (one count each) so the emission matrix is not all zeros.  Each word is escaped
// (escaped()) before being quoted into the IN(...) list, and the SELECT result is
// mysql_free_result'd once the row loop finishes.
void trainModelFromSource(cSource& source, unordered_map <lpwstring, int>& wordTagCountsMap, unordered_map <lpwstring, int>& tagTransitionCountsMap, unordered_map <lpwstring, int>& tagCountsMap)
{
	// Train part-of-speech (POS) tagger model
	set<lpwstring> taggedWords, allWords;
	// Start state
	vector<lpwstring> previousTags = { startTag };
	tagCountsMap[startTag] = 1;
	for (cWordMatch im : source.m)
	{
		vector <int> winnerForms;
		im.getWinnerForms(winnerForms);
		vector <lpwstring> tags;
		lpwstring word = im.word->first;
		for (int wf : winnerForms)
		{
			lpwstring tag = Forms[wf]->name;
			std::replace(tag.begin(), tag.end(), ' ', '*');
			for (lpwstring ptag : previousTags)
				tagTransitionCountsMap[ptag + u" " + tag] += 1;
			std::replace(word.begin(), word.end(), ' ', '*');
			wordTagCountsMap[tag + u" " + word] += 1;
			tagCountsMap[tag] += 1;
			tags.push_back(tag);
		}
		allWords.insert(word);
		if (winnerForms.size() > 0)
			taggedWords.insert(word);
		previousTags = tags;
		// not compatible with winner - startTag is never winner
		//if (source.isEOS(im - source.m.begin()))
		//{
		//	lpwstring word = u"--n--";
		//	lpwstring tag = startTag;
		//	for (lpwstring ptag : previousTags)
		//	{
		//		std::replace(ptag.begin(), ptag.end(), ' ', '*');
		//		tagTransitionCountsMap[ptag + u" " + tag] += 1;
		//	}
		//	wordTagCountsMap[tag + u" " + word] += 1;
		//	tagCountsMap[tag] += 1;
		//	previousTags = { tag };
		//}
	}
	set<lpwstring> untaggedWords;
	set_difference(allWords.begin(), allWords.end(), taggedWords.begin(), taggedWords.end(), std::inserter(untaggedWords, untaggedWords.begin()));
	lplog(LOG_ERROR, u"allWords=%d taggedWords=%d untaggedWords=%d", allWords.size(), taggedWords.size(), untaggedWords.size());
	if (untaggedWords.size())
	{
		lpwstring wordsToAdd;
		for (lpwstring utw : untaggedWords)
			wordsToAdd += u"\"" + escaped(utw) + u"\",";
		MYSQL_RES* result;
		MYSQL_ROW sqlrow;
		lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
		lp_snprintf(qt, QUERY_BUFFER_LEN, u"select w.word, f.name as formname, MAX(count) from words w, wordforms wf, forms f where wf.formId = f.id and w.id = wf.wordId and w.word in(%s) group by word", wordsToAdd.substr(0, wordsToAdd.length() - 1).c_str());
		if (!myquery(&source.mysql, qt, result))
			lplog(LOG_FATAL_ERROR, u"Error in model training.");
		//insert words that never got matched with any tag.
		// this is to prevent null probabilities in hmm matrix.
		for (int row = 0; sqlrow = mysql_fetch_row(result); row++)
		{
			lpwstring word, tag;
			mTW(sqlrow[0], word);
			mTW(sqlrow[1], tag);
			wordTagCountsMap[tag + u" " + word] = 1;
		}
		mysql_free_result(result);
	}
}

// Write T/E/C lines (transition / emission / tag-count) as UNICODE.  Returns an
// empty vector (and writes nothing) if lp_wfopen fails.  Returns the same lines in
// a vector for in-memory load.
vector <lpwstring> writeModelFile(lpwstring modelPath, unordered_map <lpwstring, int>& wordTagCountsMap, unordered_map <lpwstring, int>& tagTransitionCountsMap, unordered_map <lpwstring, int>& tagCountsMap)
{
	vector <lpwstring> model;

	FILE* out_fp = lp_wfopen(modelPath.c_str(), "w, ccs=UNICODE");
	if (!out_fp) return model;

	// Write transition counts
	for (auto const& [tags, count] : tagTransitionCountsMap)
	{
		lpwstring tline = u"T " + tags + u" " + lp_narrow_to_wide(std::to_string(count));
		model.push_back(tline);
		lp_fwprintf(out_fp, u"%s\n", tline.c_str());
	}
	// Write emission counts
	for (auto const& [tagword, count] : wordTagCountsMap)
	{
		lpwstring eline = u"E " + tagword + u" " + lp_narrow_to_wide(std::to_string(count));
		model.push_back(eline);
		lp_fwprintf(out_fp, u"%s\n", eline.c_str());
	}
	// Write tagCountsMap unordered_map
	for (auto const& [tag, count] : tagCountsMap)
	{
		lpwstring cline = u"C " + tag + u" " + lp_narrow_to_wide(std::to_string(count));
		model.push_back(cline);
		lp_fwprintf(out_fp, u"%s\n", cline.c_str());
	}
	fclose(out_fp);
	return model;
}

// Read the T/E/C model file.  Returns an empty vector if lp_wfopen fails.  An empty
// line is left as-is rather than underflowing line[-1].  Lines are capped at 100 chars.
vector <lpwstring> readModelFile(lpwstring modelPath)
{
	vector <lpwstring> model;
	FILE* model_fp = lp_wfopen(modelPath.c_str(), "r, ccs=UNICODE");
	if (!model_fp) return model; 
	// Start state
	lpchar_t line[100 + 1];
	while (lp_fgetws(line, 100, model_fp) != NULL)
	{
		size_t n = lp_strlen(line);
		if (n) line[n - 1] = 0;
		model.push_back(line);
	}
	fclose(model_fp);
	return model;
}

// Batch B2: the model file lines are whitespace-separated tokens (2-3 lpwstring
// fields plus a trailing integer count); loadModel() below used to read them via
// std::wstringstream's operator>>, but that relies on locale facets (ctype<T> for
// whitespace-skipping, num_get<T> for the integer) that the standard library only
// guarantees for char/wchar_t, not char16_t -- manual whitespace split instead.
static vector<lpwstring> tokenizeWhitespaceLine(const lpwstring& line)
{
	vector<lpwstring> tokens;
	size_t p = 0, n = line.size();
	while (p < n)
	{
		while (p < n && (line[p] == u' ' || line[p] == u'\t')) ++p;
		if (p >= n) break;
		size_t start = p;
		while (p < n && line[p] != u' ' && line[p] != u'\t') ++p;
		tokens.push_back(line.substr(start, p - start));
	}
	return tokens;
}

// Load model
// Parse T/E/C lines back into the three count maps.  '*' in tags/words becomes space.
void loadModel(vector <lpwstring>& model, unordered_map <lpwstring, int>& wordTagCountsMap, unordered_map <lpwstring, int>& tagTransitionCountsMap, unordered_map <lpwstring, int>& tagCountsMap)
{
	for (vector <lpwstring>::iterator mi = model.begin(), miEnd = model.end(); mi != miEnd; mi++)
	{
		if ((mi - model.begin()) % 5000 == 0)
			printf("Loading model processed: %03d%%:%09I64d\r", (int)(100 * (mi - model.begin()) / model.size()), (int64_t)(mi - model.begin()));
		lpwstring type, tag, x;
		int count = 0;
		vector<lpwstring> tok = tokenizeWhitespaceLine(*mi);
		if (mi->at(0) == u'C')
		{
			bool extracted = tok.size() >= 3;
			if (extracted) { type = tok[0]; tag = tok[1]; count = lp_wtoi(tok[2].c_str()); }
			std::replace(tag.begin(), tag.end(), '*', ' ');
			if (!extracted)
				lplog(LOG_ERROR, u"failed to read in tagCountsMap data %s", mi->c_str());
			else
				tagCountsMap[tag] = int(count);
			continue;
		}
		if (tok.size() >= 4) { type = tok[0]; tag = tok[1]; x = tok[2]; count = lp_wtoi(tok[3].c_str()); }
		std::replace(tag.begin(), tag.end(), '*', ' ');
		std::replace(x.begin(), x.end(), '*', ' ');
		if (mi->at(0) == u'T')
		{
			tagTransitionCountsMap[tag + u" " + x] = int(count);
		}
		else
		{
			wordTagCountsMap[tag + u" " + x] = int(count);
		}
	}
}

// P(tag_j | tag_i) = (count(i->j) + alpha) / (count(i) + alpha * |tags|).
vector<vector<double>> constructTagTransitionProbabilityMatrix(unordered_map <lpwstring, int>& tagTransitionCountsMap, unordered_map <lpwstring, int>& tagCountsMap, vector <lpwstring>& tags)
{
	int tagsSize = tags.size();
	vector <vector<double>> tagTransitionProbabilityMatrix(tagsSize, vector(tagsSize, (double)0));

	for (int previousTagIndex = 0; previousTagIndex < tagsSize; previousTagIndex++)
	{
		for (int currentTagIndex = 0; currentTagIndex < tagsSize; currentTagIndex++)
		{
			lpwstring prevTag = tags[previousTagIndex];
			lpwstring tag = tags[currentTagIndex];
			// Compute smoothed transition probability
			int tagTransitionCount = 0;
			unordered_map <lpwstring, int>::iterator ti = tagTransitionCountsMap.find(prevTag + u" " + tag);
			if (ti != tagTransitionCountsMap.end())
				tagTransitionCount = ti->second;

			tagTransitionProbabilityMatrix[previousTagIndex][currentTagIndex] = (tagTransitionCount + alpha) / (tagCountsMap[prevTag] + alpha * tagsSize);
		}
	}
	return tagTransitionProbabilityMatrix;
}

// Generate emission matrix wordTagProbabilityMatrix of size numTags x vocabSize
// [wordTagProbabilityMatrix[i][j] stores the probability of observing o_j from state s_i]
// P(word_j | tag_i).  Default (USE_ALPHA_FOR_WORDTAG off): raw count/tagCount, and
// unseen pairs stay 0.  With the define: same additive smoothing as transitions.
vector<vector<double>> constructWordTagProbabilityMatrix(unordered_map <lpwstring, int>& wordTagCountsMap, unordered_map <lpwstring, int>& tagCountsMap, vector <lpwstring>& tags, vector<lpwstring>& vocab)
{
	int tagsSize = tags.size();
	int vocabSize = vocab.size();
	vector <vector<double>> wordTagProbabilityMatrix(tagsSize, vector(vocabSize, (double)0));

	for (int tagIndex = 0; tagIndex < tagsSize; tagIndex++)
	{
		for (int vocabIndex = 0; vocabIndex < vocabSize; vocabIndex++)
		{
			lpwstring tag = tags[tagIndex];
			lpwstring word = vocab[vocabIndex];
			// Compute smoothed emission probability
			int wordTagCount = 0;
			unordered_map <lpwstring, int>::iterator ei = wordTagCountsMap.find(tag + u" " + word);
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
// Allocate probability/path matrices (disk-backed if the DIYDiskArray path is set)
// and seed column 0 from startTag -> each tag * P(firstWord | tag).
// vocabReverseLookup[firstWord] inserts 0 if firstWord is not in vocab.
void initViterbiStartProbabilities(int numWords, lpwstring firstWord, vector<lpwstring>& vocab, vector <lpwstring>& tags,
	vector<vector<double>>& tagTransitionProbabilityMatrix, vector<vector<double>>& wordTagProbabilityMatrix,
	DIYDiskArray<double>& probabilityMatrix, DIYDiskArray<int>& pathMatrix,
	unordered_map <lpwstring, int>& vocabReverseLookup)
{
	printf("initializing viterbi                                                \r");
	// Word index vocabReverseLookup map
	int count = 0;
	for (vector<lpwstring>::iterator vi = vocab.begin(), viEnd = vocab.end(); vi != viEnd; vi++, count++)
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
			probabilityMatrix.put(tagIndex, 0, (double)-std::numeric_limits<double>::infinity()); //double("-inf");
			pathMatrix.put(tagIndex, 0, 0);
		}
		else
		{
			//probabilityMatrix[tagIndex][0] = log(tagTransitionProbabilityMatrix[startTagIndex][tagIndex]) + log(wordTagProbabilityMatrix[tagIndex][firstWordIndex]); // CHANGE from log add to multiplication
			probabilityMatrix.put(tagIndex, 0, tagTransitionProbabilityMatrix[startTagIndex][tagIndex] * wordTagProbabilityMatrix[tagIndex][firstWordIndex]);
			pathMatrix.put(tagIndex, 0, 0);
		}
	}
}

// Sentence window around wordSourceIndex (EOS-bounded, else �20).  If star, prefix
// the target word with '*'.  duplicateSkip counts earlier same-surface tokens (in/out).
lpwstring getContext(cSource& source, int wordSourceIndex, bool star, int& duplicateSkip)
{
	lpwstring context;
	int begin = max(0, wordSourceIndex - 20), end = (int)min(source.m.size(), (size_t)(wordSourceIndex + 20)); // batch B11: explicit common type
	for (int I = wordSourceIndex - 1; I >= 0 && I > wordSourceIndex - 20; I--)
		if (source.isEOS(I))
		{
			begin = I;
			break;
		}
	for (int I = wordSourceIndex + 1; I < source.m.size() && I < wordSourceIndex + 20; I++)
		if (source.isEOS(I))
		{
			end = I;
			break;
		}
	lpwstring originalWord;
	source.getOriginalWord(wordSourceIndex, originalWord, false, false);
	for (int I = begin; I < end; I++)
	{
		lpwstring originalIWord;
		source.getOriginalWord(I, originalIWord, false, false);
		if (I < wordSourceIndex && originalIWord == originalWord)
			duplicateSkip++;
		if (I == wordSourceIndex && star)
			context += u"*";
		context += originalIWord + u" ";
	}
	return context;
}

// Forward step
// numTags = number of tags
// numWordsInSource = number of words in text 
// order numWordsInSource*numTags*numTags
// --- output in probabilityMatrix and pathMatrix
// Viterbi forward: for each word, only consider forms present on that token.
// Uses product * probMult (not log-add).  OOV words index vocab via operator[]
// (default 0 = first vocab word).  Low-prob + flagOnlyConsiderProperNounForms
// clears the flag and retries the same index.
void forwardFromSource(cSource& source, vector<vector<double>>& tagTransitionProbabilityMatrix, vector<vector<double>>& wordTagProbabilityMatrix, DIYDiskArray<double>& probabilityMatrix, DIYDiskArray<int>& pathMatrix,
	vector <lpwstring>& tags, unordered_map <lpwstring, int>& wordSourceIndexLookup, unordered_map <lpwstring, int>& tagLookup)
{
	int numWordsInSource = source.m.size();
	double probMult = 1.0 * numWordsInSource * numWordsInSource; // CHANGE from log add to multiplication
	for (int wordSourceIndex = 1; wordSourceIndex < numWordsInSource; wordSourceIndex++)
	{
		if (wordSourceIndex % 5000 == 0)
			printf("Words forward processed: %03d%%:%09d\r", 100 * wordSourceIndex / numWordsInSource, wordSourceIndex);
		int wordVocabIndex = wordSourceIndexLookup[source.m[wordSourceIndex].word->first];
		double maximumProbabilityPerWordIndex = (double)-std::numeric_limits<double>::infinity();
		int previousTagOfHighestProbabilityPerWordIndex = -1, currentTagOfHighestProbabilityPerWordIndex = -1;
		for (int tag : source.m[wordSourceIndex].getForms())
		{
			if (tag == UNDEFINED_FORM_NUM)
				continue;
			unordered_map <lpwstring, int>::iterator tli = tagLookup.find(Forms[tag]->name);
			if (tli == tagLookup.end())
				continue; // this tag occurs for the word but is nowhere in the training model, because the form was never winner
			int tagIndex = tli->second;
			double best_prob = (double)-std::numeric_limits<double>::infinity();
			int previousTagOfHighestProbability = -1;
			for (int prevTag : source.m[wordSourceIndex - 1].getForms())
			{
				if (prevTag == UNDEFINED_FORM_NUM)
					continue;
				tli = tagLookup.find(Forms[prevTag]->name);
				if (tli == tagLookup.end())
					continue; // this tag occurs for the word but is nowhere in the training model, because the form was never winner
				int prevTagIndex = tli->second;
				//double prob = probabilityMatrix[prevTagIndex][wordSourceIndex - 1] +	log(tagTransitionProbabilityMatrix[prevTagIndex][tagIndex]) + log(wordTagProbabilityMatrix[tagIndex][wordVocabIndex]); // CHANGE from log add to multiplication
				double prob = probMult * probabilityMatrix.get(prevTagIndex, wordSourceIndex - 1) * tagTransitionProbabilityMatrix[prevTagIndex][tagIndex] * wordTagProbabilityMatrix[tagIndex][wordVocabIndex];
				if (prob < 0)
					lplog(LOG_ERROR, u"probability:%d:%s%s:tag %s:previous %.14f*tag transition %.14f*word tag %.14f=%.14f", wordSourceIndex, source.m[wordSourceIndex].word->first.c_str(),
						(source.m[wordSourceIndex].flags & cWordMatch::flagOnlyConsiderProperNounForms) ? u"[onlyProperNounSet]" : u"",
						Forms[tag]->name.c_str(), probabilityMatrix.get(prevTagIndex, wordSourceIndex - 1), tagTransitionProbabilityMatrix[prevTagIndex][tagIndex], wordTagProbabilityMatrix[tagIndex][wordVocabIndex], prob);
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
			probabilityMatrix.put(tagIndex, wordSourceIndex, best_prob);
#ifdef USE_ALPHA_FOR_WORDTAG
			if (best_prob < 0.000000001)
				lplog(LOG_ERROR, u"Low probability detected:word:%s,tagIndex=%d:%s,wordSourceIndex=%d:%.14f", source.m[wordSourceIndex].word->first.c_str(), tagIndex, Forms[tag]->name.c_str(), wordSourceIndex, best_prob);
#else
			if (wordSourceIndex == 75)
				lplog(LOG_ERROR, u"Low probability detected:word:%s,tagIndex=%d:%s,wordSourceIndex=%d:%.14f", source.m[wordSourceIndex].word->first.c_str(), tagIndex, Forms[tag]->name.c_str(), wordSourceIndex, best_prob);
#endif	
			pathMatrix.put(tagIndex, wordSourceIndex, previousTagOfHighestProbability);
			if (previousTagOfHighestProbability >= 0 && !source.m[wordSourceIndex - 1].testPreferredViterbiForm(tags[previousTagOfHighestProbability]))
				lplog(LOG_ERROR, u"%d:*InterimForward Error setting word %s to tag %s (%d) [probability=%f=(prevProb=%f+log(tagTransitionProbability=%f [prevTag=%s][toTag=%s])+log(wordTagProbability=%f [tag=%s,word=%s])]",
					wordSourceIndex - 1, source.m[wordSourceIndex - 1].word->first.c_str(), tags[previousTagOfHighestProbability].c_str(), previousTagOfHighestProbability, best_prob,
					probabilityMatrix.get(previousTagOfHighestProbability, wordSourceIndex), // prevProb
					(tagTransitionProbabilityMatrix[previousTagOfHighestProbability][tagIndex]), tags[previousTagOfHighestProbability].c_str(), tags[tagIndex].c_str(),// tagTransitionProbability, prevTagIndex, toTag
					(wordTagProbabilityMatrix[tagIndex][wordVocabIndex]), tags[tagIndex].c_str(), source.m[wordSourceIndex].word->first.c_str()); // wordTagProbability, toTag, word
		}
		if (maximumProbabilityPerWordIndex < 0.000000001)
		{
			if (source.m[wordSourceIndex].flags & cWordMatch::flagOnlyConsiderProperNounForms)
			{
				int duplicateSkip = 0;
				lplog(LOG_ERROR, u"%d:MAXREDO forceProperNoun incorrect:[%s]", wordSourceIndex, getContext(source, wordSourceIndex, true, duplicateSkip).c_str());
				source.m[wordSourceIndex].flags &= ~cWordMatch::flagOnlyConsiderProperNounForms;
				wordSourceIndex--;
				continue;
			}
			else
				lplog(LOG_ERROR, u"Low MAXIMUM probability detected:wordSourceIndex=%d:%.14f", wordSourceIndex, maximumProbabilityPerWordIndex);
		}
		if (maximumProbabilityPerWordIndex != (double)-std::numeric_limits<double>::infinity())
			probMult = ((double)numWordsInSource * numWordsInSource) / maximumProbabilityPerWordIndex; // CHANGE from log add to multiplication
	if (isnan(probMult))
	{
			lplog(LOG_FATAL_ERROR, u"Viterbi: forward probability multiplier is not a number: %f", maximumProbabilityPerWordIndex);
			return;
		}
		if (probMult < 0.000000001)
			lplog(LOG_ERROR, u"Low probMult probability detected:%d:%.14f %.14f/%.14f", wordSourceIndex, probMult, ((double)numWordsInSource * numWordsInSource), maximumProbabilityPerWordIndex);
		source.m[wordSourceIndex].preferredViterbiMaximumProbability = maximumProbabilityPerWordIndex;
		source.m[wordSourceIndex].preferredViterbiPreviousTagOfHighestProbability = previousTagOfHighestProbabilityPerWordIndex;
		source.m[wordSourceIndex].preferredViterbiCurrentTagOfHighestProbability = currentTagOfHighestProbabilityPerWordIndex;
	}
}

// Walk pathMatrix from the last word's best tag back to 0, calling
// setPreferredViterbiForm.  tagLookup[name] inserts 0 if the form is unknown.
// Returns the number of setPreferred / illegal-path failures.
int backwardFromSource(cSource& source, vector <lpwstring>& tags, unordered_map <lpwstring, int>& tagLookup, DIYDiskArray<double>& probabilityMatrix, DIYDiskArray<int>& pathMatrix)
{
	int numWordsInSource = source.m.size(), criticalErrors = 0;
	vector <int> z = vector(numWordsInSource, (int)-1);
	double maximumProbability = probabilityMatrix.get(0, numWordsInSource - 1);
	for (unsigned int tagFormOffset = 0; tagFormOffset < source.m[numWordsInSource - 1].formsSize(); tagFormOffset++)
	{
		int tagIndex = tagLookup[source.m[numWordsInSource - 1].word->second.Form(tagFormOffset)->name];
		if (probabilityMatrix.get(tagIndex, numWordsInSource - 1) > maximumProbability)
		{
			maximumProbability = probabilityMatrix.get(tagIndex, numWordsInSource - 1);
			z[numWordsInSource - 1] = tagIndex;
		}
	}
	lplog(LOG_INFO, u"Viterbi maximum probability=%.14f", maximumProbability);
	if (z[numWordsInSource - 1] < 0 || !source.m[numWordsInSource - 1].setPreferredViterbiForm(tags[z[numWordsInSource - 1]], probabilityMatrix.get(z[numWordsInSource - 1], numWordsInSource - 1)))
	{
		lplog(LOG_ERROR, u"%d:(1)Error setting word %s to tag %s (%d)", numWordsInSource - 1, source.m[numWordsInSource - 1].word->first.c_str(), (z[numWordsInSource - 1] >= 0) ? tags[z[numWordsInSource - 1]].c_str() : u"ILLEGAL TAG", z[numWordsInSource - 1]);
		criticalErrors++;
	}
	for (int i = numWordsInSource - 1; i > 0; i--)
	{
		if (i % 5000 == 0)
			printf("Words backward processed: %03d%%:%09d\r", 100 * (numWordsInSource - i) / numWordsInSource, i);
		if (z[i] < 0)
		{
			lplog(LOG_ERROR, u"%d:Error setting next path (%d)", i, z[i]);
			criticalErrors++;
			break;
		}
		z[i - 1] = pathMatrix.get(z[i], i);
		// remove the previous path probability - just assess the probability of the tag at that word alone.
		if (z[i - 1] < 0 || !source.m[i - 1].setPreferredViterbiForm(tags[z[i - 1]], probabilityMatrix.get(z[i - 1], i - 1)))
		{
			lplog(LOG_ERROR, u"%d:(2)Error setting word %s to tag %s (%d)", i - 1, source.m[i - 1].word->first.c_str(), (z[i - 1] >= 0) ? tags[z[i - 1]].c_str() : u"ILLEGAL TAG", z[i - 1]);
			criticalErrors++;
		}
	}
	return criticalErrors;
}



unordered_map<lpwstring, vector <lpwstring> > viterbiAssociationMap = {

	// amplification
	//{u"sectionheader", u"noun"},

	// similarity 
	{u"coordinator",{ u"conjunction"} },

	// include possible subclasses
	{u"verb", { u"verbverb",u"SYNTAX:Accepts S as Object"} }, // feel, see, watch, hear, tell etc // fancy, say (thinksay verbs)
	{u"noun",{ u"dayUnit",u"timeUnit",u"simultaneousUnit",u"quantifier",u"Proper Noun" } }, // all, some etc
	{u"adjective",{ u"quantifier" } } // many
};

// include subclasses of forms with their parents.
// include the word itself if viterbi doesn't include it.
// if viterbi specifies the word as the form, specify every form (as in that case the viterbi pick is ambiguous)
// Expand preferredViterbiForms: invert viterbiAssociationMap so subclasses map
// back to parents, keep the word-as-form if present, and add verbForm for
// gerunds tagged adjective/noun.  Stops at the first token with an empty list.
void appendAssociatedFormsToViterbiTags(cSource& source)
{
	unordered_map<lpwstring, vector <lpwstring> > originalViterbiAssociationMap = viterbiAssociationMap;
	for (auto const& [form, vectorforms] : originalViterbiAssociationMap)
		for (lpwstring f : vectorforms)
			viterbiAssociationMap[f].push_back(form);

	int wordIndex = 0;
	for (cWordMatch& im : source.m)
	{
		if (wordIndex % 5000 == 0)
			printf("Appending associated forms: %03I64d%%:%09d\r", (int64_t)(((int64_t)100) * wordIndex / source.m.size()), wordIndex);
		if (im.preferredViterbiForms.empty())
			break;
		im.originalPreferredViterbiForm = im.preferredViterbiForms[0];
		bool viterbiFormMatchedWord = false;
		if (im.formsSize() > 1)
			for (int vf : im.preferredViterbiForms)
			{
				lpwstring formName = Forms[im.getFormNum(vf)]->name;
				// if the form is the word itself, then actually match all forms.
				if (formName == im.word->first)
				{
					//lplog(LOG_ERROR, u"%d:viterbi extended %s to all forms (word match)", wordIndex, formName.c_str());
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
				lpwstring formName = Forms[im.getFormNum(vf)]->name;
				unordered_map<lpwstring, vector <lpwstring>>::iterator mi = viterbiAssociationMap.find(formName);
				if (mi != viterbiAssociationMap.end())
				{
					for (lpwstring associatedForm : mi->second)
					{
						int formOffset = im.queryForm(cForms::findForm(associatedForm));
						if (formOffset >= 0)
						{
							im.preferredViterbiForms.push_back(formOffset);
							//lplog(LOG_ERROR, u"%d:viterbi subClass/superClass extended %s->%s", wordIndex, formName.c_str(), associatedForm.c_str());
						}
					}
					break;
				}
			}
			// never exclude the word itself from possibile match
			int selfForm = cForms::findForm(im.word->first);
			if (selfForm >= 0)
			{
				int formOffset = im.queryForm(selfForm);
				if (formOffset >= 0)
				{
					im.preferredViterbiForms.push_back(formOffset);
					//lplog(LOG_ERROR, u"%d:viterbi extended to word match %s", wordIndex, im.word->first.c_str());
				}
			}
		}
		// if viterbi form is an adjective, or noun, and the word is a verb gerund (missing, driving) then also include the verb form
		if ((std::find(im.preferredViterbiForms.begin(), im.preferredViterbiForms.end(), adjectiveForm) != im.preferredViterbiForms.end() ||
			std::find(im.preferredViterbiForms.begin(), im.preferredViterbiForms.end(), nounForm) != im.preferredViterbiForms.end()) &&
			im.queryForm(verbForm) >= 0 && (im.word->second.inflectionFlags & VERB_PRESENT_PARTICIPLE))
		{
			im.preferredViterbiForms.push_back(verbForm);
		}
		wordIndex++;
	}
}

// Fill prevTag/tag and the raw transition/emission counts for logging a mismatch.
void getInternalViterbiInfo(cSource& source, int viterbiOriginalTagIndex, int wordSourceIndex, lpwstring& prevTag, lpwstring& tag, int& tagTransitionCount, int& wordTagCount,
	vector <lpwstring>& tags, //vector <lpwstring> &vocab,
	DIYDiskArray<int>& pathMatrix,
	unordered_map <lpwstring, int>& wordTagCountsMap, unordered_map <lpwstring, int>& tagTransitionCountsMap)
{
	prevTag = tags[pathMatrix.get(viterbiOriginalTagIndex, wordSourceIndex)];
	tag = tags[viterbiOriginalTagIndex];
	// Compute smoothed transition probability
	tagTransitionCount = 0;
	unordered_map <lpwstring, int>::iterator ti = tagTransitionCountsMap.find(prevTag + u" " + tag);
	if (ti != tagTransitionCountsMap.end())
		tagTransitionCount = ti->second;
	// Compute smoothed emission probability
	wordTagCount = 0;
	unordered_map <lpwstring, int>::iterator ei = wordTagCountsMap.find(tag + u" " + source.m[wordSourceIndex].word->first);
	if (ei != wordTagCountsMap.end())
		wordTagCount = ei->second;
}

// Walk source.m and log Viterbi vs LP-winner disagreements.  On mismatch, parse
// the local sentence with Stanford (WRITE lock held for the whole scan) and
// record whether Stanford agrees with LP or Viterbi.  Final % lines divide by
// viterbiMismatchesNotWinner with no zero guard.
void compareViterbiAgainstStructuredTagging(cSource& source,
	vector<vector<double>>& tagTransitionProbabilityMatrix, vector<vector<double>>& wordTagProbabilityMatrix, DIYDiskArray<double>& probabilityMatrix, DIYDiskArray<int>& pathMatrix,
	unordered_map <lpwstring, int>& wordSourceIndexLookup,
	unordered_map <lpwstring, int>& tagLookup,
	vector <lpwstring>& tags, //vector <lpwstring> &vocab,
	unordered_map <lpwstring, int>& wordTagCountsMap, unordered_map <lpwstring, int>& tagTransitionCountsMap, unordered_map <lpwstring, int>& tagCountsMap,
	JNIEnv* env, bool pcfg)
{
	lpwstring winnerFormsString;
	int viterbiMismatchesSetToSeparator = 0, viterbiMismatchesNotSet = 0, viterbiMismatchesIllegal = 0, viterbiMismatchesNotWinner = 0, wordSourceIndex = 0;
	int totalNumWinnerForms = 0, totalNumForms = 0, totalViterbiSpecifiedForms = 0, totalViterbiPathViolatedForms = 0;
	double averageViterbiProbability = 0;
	unordered_map<lpwstring, int> winnerViolationFormCountMap, winnerViolationWordCountMap;
	int pathViolations = 0;
	int stanfordNotIdentifiedNum = 0, stanfordIsLPWinnerNum = 0, stanfordIsViterbiWinnerNum = 0;
	if (!myquery(&source.mysql, u"LOCK TABLES stanfordPCFGParsedSentences WRITE")) // moved out parseSentence (actually in foundParseSentence and setParsedSentence) for performance
		return;
	for (vector <cWordMatch>::iterator im = source.m.begin(), imEnd = source.m.end(); im != imEnd; im++, wordSourceIndex++)
	{
		if (wordSourceIndex % 5000 == 0)
			printf("Comparing viterbi against structured tagging: %03I64d%%:%09d\r", (int64_t)(((int64_t)100) * wordSourceIndex / source.m.size()), wordSourceIndex);
		averageViterbiProbability += im->preferredViterbiProbability;
		totalNumWinnerForms += im->getNumWinners();
		totalNumForms += im->formsSize();
		totalViterbiSpecifiedForms += im->preferredViterbiForms.size();
		if (find(im->preferredViterbiForms.begin(), im->preferredViterbiForms.end(), -2) != im->preferredViterbiForms.end())
		{
			if (im->forms.isSet(quoteForm) || im->isTopLevel())
				continue;
			lplog(LOG_ERROR, u"%d:preferredViterbiForm on word %s is set to separator", wordSourceIndex, im->word->first.c_str());
			viterbiMismatchesSetToSeparator++;
			continue;
		}
		if (im->preferredViterbiForms.empty())
		{
			lplog(LOG_ERROR, u"%d:preferredViterbiForm on word %s is not set", wordSourceIndex, im->word->first.c_str());
			viterbiMismatchesNotSet++;
			continue;
		}
		bool winnerFound = false, locallyPreferredPathInPreferred = false;
		int preferredViterbiCurrentTagOfHighestProbabilityOffset = im->queryForm(im->preferredViterbiCurrentTagOfHighestProbability);
		for (int preferredViterbiForm : im->preferredViterbiForms)
		{
			if (preferredViterbiForm > (signed)im->formsSize())
			{
				lplog(LOG_ERROR, u"%d:preferredViterbiForm on word %s is set to an illegal form offset (%d out of %d possible)", wordSourceIndex, im->word->first.c_str(), preferredViterbiForm, im->formsSize());
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
			vector <double> best_probs, bestProbabilitiesAtWordOnly;
			vector <lpwstring> currentTagOfHighestProbabilities;
			// get top 3 tags - this rescans all tags and gives them in best order.
			// the path in pathMatrix sometimes bypasses the highest probability tag for a particular spot, because the transition to the NEXT tag is not optimal (pathMatrix is the entire optimized path)
			for (auto const& [tag, tagIndex] : tagLookup)
			{
				// pm=probability of path up to this point
				// ttpm=what is the probability that each tag transitions into this one?
				// wtpm=what is the probability that the word has this tag?
				// pathMatrixColumn += u" " + std::to_wstring(pathMatrix[prevTagIndex][viterbiOriginalTagIndex]);
				//double ttpm = tagTransitionProbabilityMatrix[prevTagIndex][tagIndex];
				//double wtpm = wordTagProbabilityMatrix[tagIndex][wordVocabIndex];
				double prob = probabilityMatrix.get(tagIndex, wordSourceIndex);
				if (prob > (double)-std::numeric_limits<double>::infinity())
				{
					int I;
					for (I = 0; I < best_probs.size(); I++)
						if (prob > best_probs[I])
							break;
					best_probs.insert(best_probs.begin() + I, prob);
					bestProbabilitiesAtWordOnly.insert(bestProbabilitiesAtWordOnly.begin() + I, prob / ((wordSourceIndex > 0) ? probabilityMatrix.get(pathMatrix.get(tagIndex, wordSourceIndex), wordSourceIndex - 1) : 1)); // CHANGE from log add to multiplication
					currentTagOfHighestProbabilities.insert(currentTagOfHighestProbabilities.begin() + I, tag);
				}
			}
			if (best_probs[0] != probabilityMatrix.get(viterbiOriginalTagIndex, wordSourceIndex))
			{
				lplog(LOG_ERROR, u"%d:path free is best=%f(%s) != %f(%s) (winner=%s)", wordSourceIndex, best_probs[0], currentTagOfHighestProbabilities[0].c_str(),
					probabilityMatrix.get(viterbiOriginalTagIndex, wordSourceIndex), im->word->second.Form(im->originalPreferredViterbiForm)->name.c_str(),
					im->winnerFormString(winnerFormsString).c_str());
				lplog(LOG_ERROR, u"%d:forward/backward %f previous=%s current=%s", wordSourceIndex, im->preferredViterbiMaximumProbability, Forms[im->preferredViterbiPreviousTagOfHighestProbability]->name.c_str(), Forms[im->preferredViterbiCurrentTagOfHighestProbability]->name.c_str());
			}
			//else
			if (!winnerFound)
			{
				int duplicateSkip = 0;
				lpwstring contextSentence = getContext(source, wordSourceIndex, false, duplicateSkip), parse;
				if (parseSentence(source, env, contextSentence, parse, pcfg, false) < 0)
					lplog(LOG_FATAL_ERROR, u"Parse failed.");
				vector <lpwstring> posList;
				lpwstring out, originalWord = source.getOriginalWord(wordSourceIndex, out, false, false);
				findLPPOSEquivalents(contextSentence, parse, originalWord, posList, duplicateSkip, pcfg);
				contextSentence = getContext(source, wordSourceIndex, true, duplicateSkip);
				bool stanfordNotIdentified = posList.empty(), stanfordIsLPWinner = false, stanfordIsViterbiWinner = false;
				lpwstring viterbiForms;
				for (int preferredViterbiForm : im->preferredViterbiForms)
				{
					if (std::find(posList.begin(), posList.end(), im->word->second.Form(preferredViterbiForm)->name) != posList.end())
						stanfordIsViterbiWinner = true;
					viterbiForms += im->word->second.Form(preferredViterbiForm)->name + u" ";
				}
				lplog(LOG_ERROR, u"%d:context %s [%s]", wordSourceIndex, contextSentence.c_str(), parse.c_str());
				winnerViolationFormCountMap[im->winnerFormString(winnerFormsString, false)]++;
				winnerViolationWordCountMap[im->word->first]++;
				lpwstring prevTag, tag;
				int tagTransitionCount, wordTagCount;
				lpwstring winnerWordTagProbability;
				vector <int> winnerForms;
				im->getWinnerForms(winnerForms);
				if (winnerForms.size() > 0)
					winnerWordTagProbability = u"(";
				for (int wf : winnerForms)
				{
					if (std::find(posList.begin(), posList.end(), Forms[wf]->name) != posList.end())
						stanfordIsLPWinner = true;
					auto tli = tagLookup.find(Forms[wf]->name);
					if (tli != tagLookup.end())
					{
						int path = pathMatrix.get(tli->second, wordSourceIndex);
						double winnerProb = probabilityMatrix.get(tli->second, wordSourceIndex) / ((wordSourceIndex > 0 && path >= 0) ? probabilityMatrix.get(path, wordSourceIndex - 1) : 1);
						lpwstring wtp;
						lpchar_t ctmp[32];
						lp_snprintf(ctmp, 32, u"%f", winnerProb);
						wtp = ctmp;
						if (winnerForms.size() > 1)
							winnerWordTagProbability += u"[" + Forms[wf]->name + u"=" + wtp + u"]";
						else
							winnerWordTagProbability += wtp;
					}
				}
				if (winnerForms.size() > 0)
					winnerWordTagProbability += u")";
				getInternalViterbiInfo(source, viterbiOriginalTagIndex, wordSourceIndex, prevTag, tag, tagTransitionCount, wordTagCount, tags, pathMatrix, wordTagCountsMap, tagTransitionCountsMap);
				lplog(LOG_ERROR, u"stanfordNotIdentified = %s stanfordIsLPWinner=%s stanfordIsViterbiWinner=%s", (stanfordNotIdentified) ? u"true" : u"false", (stanfordIsLPWinner) ? u"true" : u"false", (stanfordIsViterbiWinner) ? u"true" : u"false");
				if (stanfordNotIdentified) stanfordNotIdentifiedNum++;
				if (stanfordIsLPWinner) stanfordIsLPWinnerNum++;
				if (stanfordIsViterbiWinner) stanfordIsViterbiWinnerNum++;
				lplog(LOG_ERROR, u"%d:preferredViterbiForms %s is/are not among the winner forms %s%s for word %s [%f tagTransition=%f (#previousTag[%s]=%d #transition=%d) wordTagProbability=%f (#tag[%s]=%d #wordTag=%d)]",
					wordSourceIndex, viterbiForms.c_str(), // %d:preferredViterbiForms %s 
					im->winnerFormString(winnerFormsString).c_str(), winnerWordTagProbability.c_str(), im->word->first.c_str(), // is/are not among the winner forms %s%s for word %s 
					im->preferredViterbiProbability,
					tagTransitionProbabilityMatrix[pathMatrix.get(viterbiOriginalTagIndex, wordSourceIndex)][viterbiOriginalTagIndex],
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
					lpwstring prevTag, tag;
					int tagTransitionCount, wordTagCount;
					int currentTagIndex = tagLookup[currentTagOfHighestProbabilities[p]];
					getInternalViterbiInfo(source, currentTagIndex, wordSourceIndex, prevTag, tag, tagTransitionCount, wordTagCount,
						tags, pathMatrix, wordTagCountsMap, tagTransitionCountsMap);
					lplog(LOG_ERROR, u"%d:preferredViterbiTags %d: %s [%f tagTransition=%f (#previousTag[%s]=%d #transition=%d) wordTagProbability=%f (#tag[%s]=%d #wordTag=%d)]",
						wordSourceIndex, p, currentTagOfHighestProbabilities[p].c_str(), bestProbabilitiesAtWordOnly[p],
						tagTransitionProbabilityMatrix[pathMatrix.get(currentTagIndex, wordSourceIndex)][currentTagIndex],
						//(tagTransitionCount + alpha) / (tagCountsMap[prevTag] + alpha * tags.size()),  // check
						prevTag.c_str(), tagCountsMap[prevTag], tagTransitionCount,
						wordTagProbabilityMatrix[currentTagIndex][wordSourceIndexLookup[source.m[wordSourceIndex].word->first]],
						//(wordTagCount + alpha) / (tagCountsMap[tag] + alpha * vocab.size()), // check
						tag.c_str(), tagCountsMap[tag], wordTagCount);
				}
			}
		}
		//else
		//	lplog(LOG_ERROR, u"%d:preferredViterbiForms %s is/are among the winner forms %s for word %s [%f]",
		//		wordSourceIndex, viterbiForms.c_str(), im->winnerFormString(winnerForms).c_str(), im->word->first.c_str(), im->preferredViterbiProbability);
	}
	unlockTables(source.mysql);
	map<int, lpwstring, std::greater<int>> orderedFormCountMap;
	for (auto const& [winnerForm, count] : winnerViolationFormCountMap)
		orderedFormCountMap[count] = winnerForm;
	for (auto const& [count, winnerForm] : orderedFormCountMap)
		lplog(LOG_ERROR, u"wrong viterbi matched winnerForms %s %d (%d%%)", winnerForm.c_str(), count, count * 100 / viterbiMismatchesNotWinner);

	map<int, lpwstring, std::greater<int>> orderedWordCountMap;
	for (auto const& [winnerWord, count] : winnerViolationWordCountMap)
		orderedWordCountMap[count] = winnerWord;
	for (auto const& [count, winnerWord] : orderedWordCountMap)
		lplog(LOG_ERROR, u"wrong viterbi matched winnerWord %s %d (%d%%)", winnerWord.c_str(), count, count * 100 / viterbiMismatchesNotWinner);

	int viterbiMismatches = viterbiMismatchesSetToSeparator + viterbiMismatchesNotSet + viterbiMismatchesIllegal + viterbiMismatchesNotWinner;
	if (viterbiMismatches > 0)
	{
		if (viterbiMismatchesSetToSeparator > 0)
			lplog(LOG_ERROR, u"preferredViterbiForm is set to separator %d times (%d%%)", viterbiMismatchesSetToSeparator, viterbiMismatchesSetToSeparator * 100 / source.m.size());
		if (viterbiMismatchesNotSet > 0)
			lplog(LOG_ERROR, u"preferredViterbiForm is not set %d times (%d%%)", viterbiMismatchesNotSet, viterbiMismatchesNotSet * 100 / source.m.size());
		if (viterbiMismatchesIllegal > 0)
			lplog(LOG_ERROR, u"preferredViterbiForm is set to an illegal form offset %d times (%d%%)", viterbiMismatchesIllegal, viterbiMismatchesIllegal * 100 / source.m.size());
		if (viterbiMismatchesNotWinner > 0)
			lplog(LOG_ERROR, u"preferredViterbiForm is not among the winner forms %d times (%2.3f%%)", viterbiMismatchesNotWinner, ((double)viterbiMismatchesNotWinner * 100) / source.m.size());
		if (viterbiMismatchesSetToSeparator > 0 || viterbiMismatchesNotSet > 0 || viterbiMismatchesIllegal > 0)
			lplog(LOG_ERROR, u"preferredViterbiForm error %d times (%2.3f%%)", viterbiMismatches, ((double)viterbiMismatches * 100) / source.m.size());
		if (pathViolations > 0)
			lplog(LOG_ERROR, u"preferredViterbi pathViolations=%d addedForms=%d (%d%%)", pathViolations, totalViterbiPathViolatedForms, 100 * pathViolations / source.m.size());
		lplog(LOG_ERROR, u"preferredViterbi form %% of total winners: %f%% of total forms %f%%", 100.0 * totalViterbiSpecifiedForms / totalNumWinnerForms, 100.0 * (totalViterbiSpecifiedForms + totalViterbiPathViolatedForms) / totalNumForms);
		lplog(LOG_ERROR, u"preferredViterbi forms (%d) per word=%f -> viterbiForms (%d) per word=%f", totalNumForms, totalNumForms * 1.0 / source.m.size(), (totalViterbiSpecifiedForms + totalViterbiPathViolatedForms), (totalViterbiSpecifiedForms + totalViterbiPathViolatedForms) * 1.0 / source.m.size());
		lplog(LOG_ERROR, u"stanfordNotIdentifiedNum=%d(%d%%) stanfordIsLPWinnerNum=%d(%d%%) stanfordIsViterbiWinnerNum=%d(%d%%)",
			stanfordNotIdentifiedNum, (stanfordNotIdentifiedNum * 100) / viterbiMismatchesNotWinner,
			stanfordIsLPWinnerNum, (stanfordIsLPWinnerNum * 100) / viterbiMismatchesNotWinner,
			stanfordIsViterbiWinnerNum, (stanfordIsViterbiWinnerNum * 100) / viterbiMismatchesNotWinner);
	}
}

// Decode sequences
// wordCountLimit - use words that occur across the corpus no less than this number
// Load model, build matrices, run Viterbi.  wordCountLimit is the vocab min count.
// Huge sources spill probability/path matrices to M:\caches.
void tagFromSource(cSource& source, vector <lpwstring>& model, int wordCountLimit, JNIEnv* env, bool compare)
{
	if (source.m.empty())
	{
		lplog(LOG_ERROR, u"tagFromSource: empty source, nothing to tag.");
		return;
	}
	unordered_map <lpwstring, int> wordTagCountsMap, tagTransitionCountsMap, tagCountsMap;
	loadModel(model, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
	printf("constructing transition and emission matrices                                              \r");
	vector <lpwstring> tags;
	for (auto const& ic : tagCountsMap)
		tags.push_back(ic.first);
	// Transition matrix: the probability of state x+1 given state x (bigram case).  state=tag
	vector<vector<double>> tagTransitionProbabilityMatrix = constructTagTransitionProbabilityMatrix(tagTransitionCountsMap, tagCountsMap, tags);
	// Emission matrix: the probability that a word is tagged as a certain tag
	vector <lpwstring> vocab = generateVocabFromSource(source, wordCountLimit);
	vector<vector<double>> wordTagProbabilityMatrix = constructWordTagProbabilityMatrix(wordTagCountsMap, tagCountsMap, tags, vocab);
	DIYDiskArray<double> probabilityMatrix((source.m.size() > 12000000) ? CACHEDIR u"/ViterbiProbabilityMatrixArray.tmp" : NULL);
	DIYDiskArray<int> pathMatrix((source.m.size() > 25000000) ? CACHEDIR u"/ViterbiPathMatrixArray.tmp" : NULL);
	// Decode
	unordered_map <lpwstring, int> vocabReverseLookup;
	// Initialize start probabilities
	initViterbiStartProbabilities(source.m.size(), source.m[0].word->first, vocab, tags, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix, pathMatrix, vocabReverseLookup);
	unordered_map <lpwstring, int> tagLookup;
	int tagNum = 0;
	for (lpwstring tag : tags)
		tagLookup[tag] = tagNum++;
	forwardFromSource(source, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix, pathMatrix, tags, vocabReverseLookup, tagLookup);
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
				env, true);
	}
}

// Load sourcePath+".model.txt" if present, else train and write it.
void createModelFromSource(cSource& source, vector <lpwstring>& model)
{
	lpwstring modelPath = source.sourcePath + u".model.txt";
	if (lp_waccess(modelPath.c_str(), 0) != 0)
	{
		printf("creating model                                                \r");
		unordered_map <lpwstring, int> wordTagCountsMap, tagTransitionCountsMap, tagCountsMap;
		trainModelFromSource(source, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
		model = writeModelFile(modelPath, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
	}
	else
	{
		printf("reading model                                                \r");
		model = readModelFile(modelPath);
	}
}

// End-to-end: create/load model, JNI VM, tagFromSource(..., compare=true), destroy VM.
void testViterbiFromSource(cSource& source)
{
	vector <lpwstring> model;
	createModelFromSource(source, model);
	JavaVM* vm;
	JNIEnv* env;
	createJavaVM(vm, env);
	tagFromSource(source, model, 1, env, true);
	destroyJavaVM(vm);
}

