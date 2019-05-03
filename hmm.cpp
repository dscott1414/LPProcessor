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
// Additive smoothing parameter
double alpha = 0.001;

// from Melanie Tosik
// NLP, Viterbi part - of - speech(POS) tagger
//http://www.melanietosik.com/posts/Viterbi-POS-tagger

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

// L"thank",L"no",L"never",L"then",,L"so",L"number",L"as",L"only", L"van",L"von",L"also",L"not",L"more",L"eg",L"e.g.",L"p.o.",L"like",L" - only used in very specialized patterns
// 	L"p",L"m",L"le",L"de",L"f",L"c",L"k",L"o",L"b",L"!",L"?",
unordered_map <wstring, vector<wstring>> const promoteTag {
	{ L"to",{ L"preposition" }},
	{ L"of",{ L"preposition" }},
	{ L"and",{ L"coordinator" }},
	{ L"or",{ L"coordinator" }},
	{ L"if",{ L"conjunction" }},
	{ L"the",{ L"determiner" }},
	{ L"you",{ L"personal_pronoun" }},
	{ L"his",{ L"possessive_determiner",L"reflexive_pronoun" }},  
	{ L"her",{ L"possessive_determiner",L"reflexive_pronoun" }}, 
	{ L"there",{ L"pronoun",L"adverb" }}, 
	{ L"once",{ L"predeterminer" }},
	{ L"twice",{ L"predeterminer" }},
	{ L"both",{ L"predeterminer" }},
	{ L"but",{ L"conjunction",L"preposition" }},  
	{ L"than",{ L"conjunction",L"preposition" }}, 
	{ L"box",{ L"noun",L"verb" }}, 
	{ L"ex",{ L"adjective",L"adverb" }}, 
	{ L"which",{ L"interrogative_determiner",L"interrogative_pronoun",L"relativizer"}}, 
	{ L"what",{ L"interrogative_determiner",L"interrogative_pronoun",L"relativizer"}}, 
	{ L"whose",{ L"interrogative_determiner",L"relativizer"}}, 
	{ L"how",{ L"relativizer",L"conjunction",L"adverb"}}, 
	{ L"less",{ L"pronoun" }}
};
wstring startTag = L"--s--";
void trainModelFromSource(Source &source, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap)
{
	// Train part - of - speech(POS) tagger model

	// Start state
	vector<wstring> previousTags = { startTag };
	tagCountsMap[startTag] = 1;
	for (vector <WordMatch>::iterator im = source.m.begin(), imEnd = source.m.end(); im != imEnd; im++)
	{
		vector <int> winnerForms;
		im->getWinnerForms(winnerForms);
		vector <wstring> tags;
		for (int wf : winnerForms)
		{
			wstring tag = Forms[wf]->name;
			std::replace(tag.begin(), tag.end(), ' ', '*');
			for (wstring ptag:previousTags)
				tagTransitionCountsMap[ptag + L" " + tag] += 1;
			wordTagCountsMap[tag + L" " + im->word->first] += 1;
			tagCountsMap[tag] += 1;
			tags.push_back(tag);
		}
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
}

vector <wstring> writeModelFile(wstring modelPath, unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap)
{
	vector <wstring> model;

	FILE *out_fp = _wfopen(modelPath.c_str(), L"w");

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
	FILE *model_fp = _wfopen(modelPath.c_str(), L"r");
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
		if (mi->at(0) == L'T')
		{
			std::replace(x.begin(), x.end(), '*', ' ');
			tagTransitionCountsMap[tag + L" " + x] = int(count);
		}
		else
			wordTagCountsMap[tag + L" " + x] = int(count);
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
				wordTagCount = ei->second;

			wordTagProbabilityMatrix[tagIndex][vocabIndex] = (wordTagCount + alpha) / (tagCountsMap[tag] + alpha * vocabSize);
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
// vector <vector<double>> pathMatrix
// unordered_map <string, int> vocabLookupVector
void initViterbiFromSource(Source &source,vector<wstring> &vocab, vector <wstring> &tags,
	vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, unordered_map <wstring, int> &vocabLookupVector)
{
	// Word index vocabLookup vector 
	int count = 0;
	for (vector<wstring>::iterator vi = vocab.begin(), viEnd = vocab.end(); vi != viEnd; vi++, count++)
		vocabLookupVector[*vi] = count;

	int numWordsInSource = source.m.size();
	int numTags = tags.size();
	probabilityMatrix = vector(numTags, vector(numWordsInSource, (double)-std::numeric_limits<double>::infinity()));
	pathMatrix = vector(numTags, vector(numWordsInSource, (int)-1));

	// Initialize start probabilities
	int startTagIndex = (int)std::distance(tags.begin(), find(tags.begin(), tags.end(), startTag));
	int firstWordIndex = vocabLookupVector[source.m[0].word->first];
	for (int tagIndex = 0; tagIndex < numTags; tagIndex++)
	{
		if (tagTransitionProbabilityMatrix[startTagIndex][tagIndex] == 0)
		{
			probabilityMatrix[tagIndex][0] = (double)-std::numeric_limits<double>::infinity(); //double("-inf");
			pathMatrix[tagIndex][0] = 0;
		}
		else
		{
			//probabilityMatrix[tagIndex][0] = log(tagTransitionProbabilityMatrix[startTagIndex][tagIndex]) + log(wordTagProbabilityMatrix[tagIndex][firstWordIndex]); // CHANGE from log add to multiplication
			probabilityMatrix[tagIndex][0] = tagTransitionProbabilityMatrix[startTagIndex][tagIndex] * wordTagProbabilityMatrix[tagIndex][firstWordIndex];
			pathMatrix[tagIndex][0] = 0;
		}
	}
}

// Forward step
// numTags = number of tags
// numWordsInSource = number of words in text 
// order numWordsInSource*numTags*numTags
void forwardFromSource(Source &source,vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, 
												vector <wstring> &tags, unordered_map <wstring, int> &wordSourceIndexLookup, unordered_map <wstring, int> &tagLookup)
{
	int numWordsInSource = source.m.size();
	double probMult = numWordsInSource; // CHANGE from log add to multiplication
	for (int wordSourceIndex = 1; wordSourceIndex < numWordsInSource; wordSourceIndex++)
	{
		if (wordSourceIndex % 5000 == 0)
			printf("Words processed: %d\n", wordSourceIndex);
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
				double prob = probMult * probabilityMatrix[prevTagIndex][wordSourceIndex - 1] * tagTransitionProbabilityMatrix[prevTagIndex][tagIndex] * wordTagProbabilityMatrix[tagIndex][wordVocabIndex];
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
			probabilityMatrix[tagIndex][wordSourceIndex] = best_prob;
			pathMatrix[tagIndex][wordSourceIndex] = previousTagOfHighestProbability;
			if (!source.m[wordSourceIndex-1].testPreferredViterbiForm(tags[previousTagOfHighestProbability]))
				lplog(LOG_ERROR, L"%d:*InterimForward Error setting word %s to tag %s (%d) [probability=%f=(prevProb=%f+log(tagTransitionProbability=%f [prevTag=%s][toTag=%s])+log(wordTagProbability=%f [tag=%s,word=%s])]",
					wordSourceIndex-1, source.m[wordSourceIndex-1].word->first.c_str(), tags[previousTagOfHighestProbability].c_str(), previousTagOfHighestProbability, best_prob,
					probabilityMatrix[previousTagOfHighestProbability][wordSourceIndex], // prevProb
					(tagTransitionProbabilityMatrix[previousTagOfHighestProbability][tagIndex]), tags[previousTagOfHighestProbability].c_str(), tags[tagIndex].c_str(),// tagTransitionProbability, prevTagIndex, toTag
					(wordTagProbabilityMatrix[tagIndex][wordVocabIndex]), tags[tagIndex].c_str(), source.m[wordSourceIndex].word->first.c_str()); // wordTagProbability, toTag, word
		}
		probMult = ((double)numWordsInSource) / maximumProbabilityPerWordIndex; // CHANGE from log add to multiplication
		source.m[wordSourceIndex].preferredViterbiMaximumProbability = maximumProbabilityPerWordIndex;
		source.m[wordSourceIndex].preferredViterbiPreviousTagOfHighestProbability = previousTagOfHighestProbabilityPerWordIndex;
		source.m[wordSourceIndex].preferredViterbiCurrentTagOfHighestProbability = currentTagOfHighestProbabilityPerWordIndex;
	}
}

void backwardFromSource(Source &source,vector <vector<double>> &probabilityMatrix, vector <vector<int>> &pathMatrix, vector <wstring> &tags, unordered_map <wstring, int> &tagLookup)
{
	int numWordsInSource = source.m.size();
	vector <int> z = vector(numWordsInSource, (int)-1);
	double maximumProbability = probabilityMatrix[0][numWordsInSource - 1];

	for (unsigned int tagFormOffset = 0; tagFormOffset < source.m[numWordsInSource - 1].formsSize(); tagFormOffset++)
	{
		int tagIndex = tagLookup[source.m[numWordsInSource - 1].word->second.Form(tagFormOffset)->name];
		if (probabilityMatrix[tagIndex][numWordsInSource - 1] > maximumProbability)
		{
			maximumProbability = probabilityMatrix[tagIndex][numWordsInSource - 1];
			z[numWordsInSource - 1] = tagIndex;
		}
	}
	if (!source.m[numWordsInSource - 1].setPreferredViterbiForm(tags[z[numWordsInSource - 1]], probabilityMatrix[z[numWordsInSource - 1]][numWordsInSource - 1]))
		lplog(LOG_ERROR, L"%d:Error setting word %s to tag %s (%d)", numWordsInSource - 1, source.m[numWordsInSource - 1].word->first.c_str(), tags[z[numWordsInSource - 1]].c_str(), z[numWordsInSource - 1]);
	for (int i = numWordsInSource - 1; i > 0; i--)
	{
		z[i - 1] = pathMatrix[z[i]][i];
		// subtract the previous path probability - just assess the probability of the tag at that word alone.
		if (!source.m[i - 1].setPreferredViterbiForm(tags[z[i - 1]], probabilityMatrix[z[i - 1]][i - 1]- probabilityMatrix[pathMatrix[z[i - 1]][i - 1]][i - 2]))
			lplog(LOG_ERROR, L"%d:Error setting word %s to tag %s (%d)", numWordsInSource - 1, source.m[i - 1].word->first.c_str(), tags[z[i - 1]].c_str(), z[i - 1]);
	}
}



unordered_map<wstring, vector <wstring> > viterbiAssociationMap = {

	// amplification
	//{L"sectionheader", L"noun"},

	// similarity 
	{L"coordinator",{ L"conjunction"} },

	// include possible subclasses
	{L"verb", { L"verbverb",L"think"} }, // feel, see, watch, hear, tell etc // fancy, say (thinksay verbs)
	{L"noun",{ L"dayUnit",L"timeUnit",L"quantifier",L"Proper Noun" } } // all, some etc
};

// include subclasses of forms with their parents.
// include the word itself if viterbi doesn't include it.
// if viterbi specifies the word as the form, specify every form (as in that case the viterbi pick is ambiguous)
void appendAssociatedFormsToViterbiTags(Source &source)
{
	for (auto const&[form, vectorforms] : viterbiAssociationMap)
		for (wstring f : vectorforms)
			viterbiAssociationMap[f].push_back(form);

	int wordIndex=0;
	for (WordMatch &im : source.m)
	{
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
		wordIndex++;
	}
}

void getInternalViterbiInfo(Source &source, int viterbiOriginalTagIndex, int wordSourceIndex, wstring &prevTag, wstring &tag, int &tagTransitionCount, int &wordTagCount,
	vector <vector<int>> &pathMatrix,
	vector <wstring> &tags, //vector <wstring> &vocab,
	unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap)
{
	prevTag = tags[pathMatrix[viterbiOriginalTagIndex][wordSourceIndex]];
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
	vector<vector<double>> &tagTransitionProbabilityMatrix, vector<vector<double>> &wordTagProbabilityMatrix, 
	vector <vector<double>> &probabilityMatrix, 
	vector <vector<int>> &pathMatrix,
	unordered_map <wstring, int> &wordSourceIndexLookup, 
	unordered_map <wstring, int> &tagLookup,
	vector <wstring> &tags, //vector <wstring> &vocab,
	unordered_map <wstring, int> &wordTagCountsMap, unordered_map <wstring, int> &tagTransitionCountsMap, unordered_map <wstring, int> &tagCountsMap)
{
	wstring winnerForms;
	int viterbiMismatchesSetToSeparator = 0, viterbiMismatchesNotSet = 0, viterbiMismatchesIllegal = 0, viterbiMismatchesNotWinner = 0, wordSourceIndex = 0;
	int totalNumWinnerForms = 0, totalNumForms = 0, totalViterbiSpecifiedForms=0;
	double averageViterbiProbability = 0;
	for (vector <WordMatch>::iterator im = source.m.begin(), imEnd = source.m.end(); im != imEnd; im++, wordSourceIndex++)
	{
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
		int winnerFound = false;
		for (int preferredViterbiForm : im->preferredViterbiForms)
		{
			if (preferredViterbiForm > (signed)im->formsSize())
			{
				lplog(LOG_ERROR, L"%d:preferredViterbiForm on word %s is set to an illegal form offset (%d out of %d possible)", wordSourceIndex, im->word->first.c_str(), preferredViterbiForm, im->formsSize());
				viterbiMismatchesIllegal++;
				continue;
			}
			if (im->isWinner(preferredViterbiForm))
				winnerFound = true;
		}
		if (!winnerFound && im->queryWinnerForm(im->preferredViterbiCurrentTagOfHighestProbability) >= 0)
			winnerFound = true;
		if (!winnerFound)
		{
			wstring viterbiForms;
			for (int preferredViterbiForm : im->preferredViterbiForms)
				viterbiForms += im->word->second.Form(preferredViterbiForm)->name + L" ";
			int viterbiOriginalTagIndex = tagLookup[Forms[im->getFormNum(im->originalPreferredViterbiForm)]->name];
			vector <double> best_probs,bestProbabilitiesAtWordOnly;
			vector <wstring> currentTagOfHighestProbabilities;
			// get top 3 tags
			for (auto const&[tag, tagIndex] : tagLookup)
			{
				// pm=probability of path up to this point
				// ttpm=what is the probability that each tag transitions into this one?
				// wtpm=what is the probability that the word has this tag?
				// pathMatrixColumn += L" " + std::to_wstring(pathMatrix[prevTagIndex][viterbiOriginalTagIndex]);
				//double ttpm = tagTransitionProbabilityMatrix[prevTagIndex][tagIndex];
				//double wtpm = wordTagProbabilityMatrix[tagIndex][wordVocabIndex];
				double prob = probabilityMatrix[tagIndex][wordSourceIndex];
				if (wordSourceIndex == 480)
					lplog(LOG_ERROR, L"*** %s %f", tag.c_str(),probabilityMatrix[tagIndex][wordSourceIndex]);
				if (prob > (double)-std::numeric_limits<double>::infinity())
				{
					int I;
					for (I = 0; I < best_probs.size(); I++)
						if (prob > best_probs[I])
							break;
					best_probs.insert(best_probs.begin()+I,prob);
					bestProbabilitiesAtWordOnly.insert(bestProbabilitiesAtWordOnly.begin()+I,prob / probabilityMatrix[pathMatrix[tagIndex][wordSourceIndex]][wordSourceIndex - 1]); // CHANGE from log add to multiplication
					currentTagOfHighestProbabilities.insert(currentTagOfHighestProbabilities.begin()+I,tag);
				}
			}
			if (best_probs[0] != probabilityMatrix[viterbiOriginalTagIndex][wordSourceIndex])
			{
				lplog(LOG_ERROR, L"%d:forward/backward recalculated=%f(%s) != %f(%s) (winner=%s)", wordSourceIndex, best_probs[0], currentTagOfHighestProbabilities[0].c_str(),
					probabilityMatrix[viterbiOriginalTagIndex][wordSourceIndex], im->word->second.Form(im->originalPreferredViterbiForm)->name.c_str(),
					im->winnerFormString(winnerForms).c_str());
				lplog(LOG_ERROR, L"%d:forward/backward %f previous=%s current=%s", wordSourceIndex, im->preferredViterbiMaximumProbability, Forms[im->preferredViterbiPreviousTagOfHighestProbability]->name.c_str(), Forms[im->preferredViterbiCurrentTagOfHighestProbability]->name.c_str());
			}
			//else
			if (!winnerFound)
			{
				wstring prevTag, tag;
				int tagTransitionCount,wordTagCount;
				getInternalViterbiInfo(source, viterbiOriginalTagIndex, wordSourceIndex, prevTag, tag, tagTransitionCount, wordTagCount,pathMatrix,
					tags, wordTagCountsMap, tagTransitionCountsMap);
				lplog(LOG_ERROR, L"%d:preferredViterbiForms %s is/are not among the winner forms %s for word %s [%f tagTransition=%f (#previousTag[%s]=%d #transition=%d) wordTagProbability=%f (#tag[%s]=%d #wordTag=%d)",
					wordSourceIndex, viterbiForms.c_str(), im->winnerFormString(winnerForms).c_str(), im->word->first.c_str(), im->preferredViterbiProbability, 
					tagTransitionProbabilityMatrix[pathMatrix[viterbiOriginalTagIndex][wordSourceIndex]][viterbiOriginalTagIndex], 
					//(tagTransitionCount + alpha) / (tagCountsMap[prevTag] + alpha * tags.size()),  // check
					prevTag.c_str(), tagCountsMap[prevTag], tagTransitionCount,
					wordTagProbabilityMatrix[viterbiOriginalTagIndex][wordSourceIndexLookup[source.m[wordSourceIndex].word->first]],
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
					getInternalViterbiInfo(source, currentTagIndex, wordSourceIndex, prevTag, tag, tagTransitionCount, wordTagCount, pathMatrix,
						tags, wordTagCountsMap, tagTransitionCountsMap);
					lplog(LOG_ERROR, L"%d:preferredViterbiTags %d: %s [%f tagTransition=%f (#previousTag[%s]=%d #transition=%d) wordTagProbability=%f (#tag[%s]=%d #wordTag=%d)]", 
						wordSourceIndex, p, currentTagOfHighestProbabilities[p].c_str(), bestProbabilitiesAtWordOnly[p],
						tagTransitionProbabilityMatrix[pathMatrix[currentTagIndex][wordSourceIndex]][currentTagIndex],
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
		lplog(LOG_ERROR, L"preferredViterbiProbability average (%2.6f)", averageViterbiProbability / source.m.size());
		lplog(LOG_ERROR, L"preferredViterbi form %% of total winners: %f%% of total forms %f%%", 100.0 * totalViterbiSpecifiedForms/totalNumWinnerForms, 100.0 * totalViterbiSpecifiedForms / totalNumForms);
	}
}

// Decode sequences
void tagFromSource(Source &source, vector <wstring> &model,bool compare)
{
	unordered_map <wstring, int> wordTagCountsMap, tagTransitionCountsMap, tagCountsMap;
	loadModel(model, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
	vector <wstring> tags;
	for (auto const& ic : tagCountsMap)
		tags.push_back(ic.first);
	// Transition matrix: the probability of state x+1 given state x (bigram case).  state=tag
	vector<vector<double>> tagTransitionProbabilityMatrix = constructTagTransitionProbabilityMatrix(tagTransitionCountsMap, tagCountsMap, tags);
	// Emission matrix: the probability that a word is tagged as a certain tag
	vector <wstring> vocab = generateVocabFromSource(source, 2);
	vector<vector<double>> wordTagProbabilityMatrix = constructWordTagProbabilityMatrix(wordTagCountsMap, tagCountsMap, tags, vocab);
	// Decode
	vector <vector<double>> probabilityMatrix;
	vector <vector<int>> pathMatrix;
	unordered_map <wstring, int> wordSourceIndexLookup;
	// Initialize start probabilities
	initViterbiFromSource(source, vocab, tags, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix, pathMatrix, wordSourceIndexLookup);
	unordered_map <wstring, int> tagLookup;
	int tagNum = 0;
	for (wstring tag : tags)
		tagLookup[tag] = tagNum++;
	forwardFromSource(source, tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, probabilityMatrix, pathMatrix, tags, wordSourceIndexLookup, tagLookup);
	backwardFromSource(source, probabilityMatrix, pathMatrix, tags, tagLookup);
	appendAssociatedFormsToViterbiTags(source);
	if (compare)
		compareViterbiAgainstStructuredTagging(source,
			tagTransitionProbabilityMatrix, wordTagProbabilityMatrix, 
			probabilityMatrix, pathMatrix,
			wordSourceIndexLookup, 
			tagLookup,
			tags,//vocab,
			wordTagCountsMap,tagTransitionCountsMap,tagCountsMap);
}

void accumulateModelFromSource(Source &source, vector <wstring> &model)
{
	wstring modelPath = source.sourcePath+L".model.txt";
	if (_waccess(modelPath.c_str(), 0) != 0)
	{
		unordered_map <wstring, int> wordTagCountsMap, tagTransitionCountsMap, tagCountsMap;
		trainModelFromSource(source, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
		model = writeModelFile(modelPath, wordTagCountsMap, tagTransitionCountsMap, tagCountsMap);
	}
	else
	{
		model = readModelFile(modelPath);
	}
}

void testViterbiFromSource(Source &source)
{
	vector <wstring> model;
	accumulateModelFromSource(source, model);
	tagFromSource(source, model,true);
}

