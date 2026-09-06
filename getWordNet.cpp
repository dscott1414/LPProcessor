/*
	getWordNet.cpp - WordNet synonym/hypernym/hyponym/VerbNet class helpers

	Overview:
		Wraps the Princeton WordNet C API (findtheinfo_ds, morphstr, index_lookup,
		traceptrs_ds) to extract synonyms, antonyms, hypernyms, hyponyms, coordinate
		terms, and familiarity counts. Also maps verbs onto Levin/VerbNet classes and
		caches ordered hypernym chains.

	Pipeline position:
		Initialization (initWordNet, initializeNounVerbMapping) and later whenever
		objects/speakers need synonym or "kind of" tests.

	Key entry points:
		- initWordNet / addToWordNet / checkexist
		- getSynonyms / getWordNetSynonymsOnly / getAntonyms / getFamiliarity
		- getHyperNyms / hasHyperNym / getAllOrderedHyperNyms
		- analyzeNounClass / analyzeVerbNetClass / deriveMainEntry

	Key data structures / globals:
		- synonymMap / synonymDeletionMap / mostCommonSynonymMap - hand overrides
		- orderedHyperNymsMap - cached hypernym chains, guarded by
			orderedHyperNymsMapSRWLock (a std::shared_mutex since batch B3)
		- nounVerbMap - agentive nominalization mapping
		- internalSynonymMap[4] - per-POS memo of getSynonyms results

	Dependencies:
		WordNet dict files (wninit); source\\lists VerbNet already loaded for
		analyzeVerbNetClass.

	Notes / gotchas:
		getSynonyms is WordNet-only. It used to fall back to a MySQL 'thesaurus'
		table and then to scraping thesaurus.com; both were removed at the author's
		request along with getThesaurus.cpp, so a word absent from WordNet now yields
		no synonyms rather than reaching the network. synonymMap/synonymDeletionMap
		still apply.
		wordCheck always ends in LOG_FATAL_ERROR by design (it is a diagnostic
		report-then-exit utility). WordNet SynsetPtrs from findtheinfo_ds are not
		freed.
*/#pragma warning(disable : 4786 ) // disable warning C4786
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include <stdlib.h>
#include "wn.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "vcXML.h"
#include "profile.h"
#include "internet.h"

bool myquery(MYSQL* mysql, const lpchar_t* q, MYSQL_RES*& result, bool allowFailure = false);
bool myquery(MYSQL* mysql, const lpchar_t* q, bool allowFailure = false);
bool wordNetInitialized = false; // initialized
unordered_map <lpwstring, lpwstring> mostCommonSynonymMap; // initialized
unordered_map <lpwstring, lpwstring> synonymMap; // synonyms that are left out of WordNet // initialized
unordered_map <lpwstring, lpwstring> synonymDeletionMap; // initialized

set <int> offsets; // initialized
extern unordered_map <lpwstring, int> levinVerbToClassSectionMap; // dictionary initialized 
extern unordered_map <int, lpwstring> levinClassSectionNames; // dictionary initialized 
extern unordered_map <lpwstring, set <int> > vbNetVerbToClassMap;
int numVbNetClassFound = 0, numOneSenseVbNetClassFound = 0, numMultiSenseVbNetClassFound = 0, numVbNetClassMultiSenseNotFound = 0, verbsMappedToVerbNet = 0;

bool cacheOrderedHyperNyms = true; // initialized
unordered_map<lpwstring, vector < vector <string> > > orderedHyperNymsMap; // protected with orderedHyperNymsMapSRWLock
unordered_map<lpwstring, int > orderedHyperNymsNumMap; // protected with orderedHyperNymsMapSRWLock
// used for agentiveNominalizations 
unordered_map <lpwstring, set < lpwstring > > nounVerbMap; // initialized

// Walks synset_ptr (ptrlist then nextss), collecting lowercased space-normalized synonyms
// per sense into words. Skips the query 'word' itself. ignoreTopLevel drops the first sense
// at recur==0. Returns the number of synonyms inserted. Does not free synset_ptr.
int extractWordsFromSynset(char* word, SynsetPtr synset_ptr, int recur, vector <unordered_set <lpwstring> >& words, bool ignoreTopLevel, sTrace& t)
{
	LFS
		if (!synset_ptr) return 0;
	/*
		for (int I=0; I<synset_ptr -> ptrcount; I++)
		{
			lpchar_t *synTypes[]={ "","ANTPTR","HYPERPTR","HYPOPTR","ENTAILPTR","SIMPTR","ISMEMBERPTR","ISSTUFFPTR","ISPARTPTR","HASMEMBERPTR","HASSTUFFPTR","HASPARTPTR",
					 "MERONYM","HOLONYM","CAUSETO","PPLPTR","SEEALSOPTR","PERTPTR","ATTRIBUTE","VERBGROUP","DERIVATION","CLASSIFICATION","CLASS",
					 "SYNS","FREQ","FRAMES","COORDS","RELATIVES","HMERONYM","HHOLONYM","WNGREP","OVERVIEW" };
			lp_wprintf(u"%d:%s\n",I,synTypes[synset_ptr->ptrtyp[I]]);
		}
		lp_wprintf(u"%s\n",FmtSynset(synset_ptr, 1));
	*/
	int numWords = 0;
	if (recur || !ignoreTopLevel)
	{
		unordered_set <lpwstring> sense;
		for (int I = 0; I < synset_ptr->wcount; I++)
		{
			if (!strcmp(word, synset_ptr->words[I])) continue;
			numWords++;
			char synword[WORDBUF];
			strcpy(synword, synset_ptr->words[I]);
			strsubst(synword, '_', ' ');
			lp_towlower_str(synword);
			lpwstring w;
			sense.insert(mTW(synword, w));
			//if (t.traceSpeakerResolution)
			}
		words.push_back(sense);
		if (!recur && synset_ptr->wcount && t.traceSpeakerResolution) lplog(LOG_WORDNET, u"");
	}
	int tmpNumWords;
	if ((tmpNumWords = extractWordsFromSynset(word, synset_ptr->ptrlist, recur + 1, words, ignoreTopLevel, t)) && !recur && t.traceSpeakerResolution)
		lplog(LOG_WORDNET, u"");
	numWords += extractWordsFromSynset(word, synset_ptr->nextss, recur, words, ignoreTopLevel, t) + tmpNumWords;
	return numWords;
}

// Flat overload: unions every sense from the vector version into words. Returns words.size().
int extractWordsFromSynset(char* word, SynsetPtr synset_ptr, int recur, unordered_set <lpwstring>& words, bool ignoreTopLevel, sTrace& t)
{
	vector <unordered_set <lpwstring> > wordsBySense;
	extractWordsFromSynset(word, synset_ptr, recur, wordsBySense, ignoreTopLevel, t);
	for (int I = 0; I < wordsBySense.size(); I++)
		words.insert(wordsBySense[I].begin(), wordsBySense[I].end());
	return words.size();
}

// Seeds synonymMap / synonymDeletionMap with hand overrides (professor→teacher, drop book=album).
void addToWordNet()
{
	LFS
		synonymMap[u"professor"] = u"teacher";
	synonymDeletionMap[u"book"] = u"album"; // allowing book and album to be synonyms would be mixing the two largest categories of named items
	synonymDeletionMap[u"detail"] = u"specialty"; // this is not correct - leads to 'full detail' = 'Krugman's specialty'
	synonymDeletionMap[u"person"] = u"party"; // this is not correct - leads to 'full detail' = 'Krugman's specialty'
}

// One-shot wninit(); LOG_FATAL_ERROR if the WordNet dict files cannot be opened.
void initWordNet()
{
	LFS
		if (!wordNetInitialized)
		{
			if (wninit() < 0)
				lplog(LOG_FATAL_ERROR, u"WordNet failed initialization!");
			wordNetInitialized = true;
			addToWordNet();
		}
}

// True if WordNet has word (or any morphstr of it) in wordClass (NOUN/VERB/ADJ/ADV).
bool checkexist(char* word, int wordClass)
{
	LFS
		initWordNet();
	if (getindex(word, wordClass)) return true;
	for (char* morphword = morphstr(word, wordClass); morphword; morphword = morphstr(NULL, wordClass))
		if (getindex(morphword, wordClass)) return true;
	return false;
}

// True if checkexist succeeds for any of NOUN/VERB/ADJ/ADV.
bool checkexist(char* word)
{
	LFS
		return checkexist(word, NOUN) || checkexist(word, VERB) || checkexist(word, ADJ) || checkexist(word, ADV);
}



unordered_map <lpwstring, vector < unordered_set <lpwstring> > > internalSynonymMap[4];
// Flattens the per-sense getSynonyms overload into one set.
void cSource::getSynonyms(lpwstring word, unordered_set <lpwstring>& synonyms, int synonymType)
{
	vector <unordered_set <lpwstring> > synonymsSenses;
	getSynonyms(word, synonymsSenses, synonymType);
	for (int s = 0; s < synonymsSenses.size(); s++)
		synonyms.insert(synonymsSenses[s].begin(), synonymsSenses[s].end());
}

// WordNet SIMPTR + synonymMap, minus synonymDeletionMap. Memoized in
// internalSynonymMap[synonymType]. Ignores non-alpha/_ words and anything containing "http".
void cSource::getSynonyms(lpwstring word, vector <unordered_set <lpwstring> >& synonyms, int synonymType)
{
	LFS
		// check if word is legal
		for (int I = 0; I < word.length(); I++)
			if (!iswalpha(word[I]) && word[I] != u'_') // two words has a _ in it
				return;
	if (word.find(u"http") != lpwstring::npos)
		return;
	auto smi = internalSynonymMap[synonymType].find(word);
	if (smi != internalSynonymMap[synonymType].end())
	{
		synonyms = smi->second;
		return;
	}
	initWordNet();
	string sWord;
	SynsetPtr sp = findtheinfo_ds(wTM(word, sWord), synonymType, SIMPTR, ALLSENSES);

	extractWordsFromSynset(wTM(word, sWord), sp, 0, synonyms, false, debugTrace);
	unordered_map <lpwstring, lpwstring>::iterator si = synonymMap.find(word);
	if (si != synonymMap.end())
	{
		for (int I = 0; I < synonyms.size(); I++)
			synonyms[I].insert(si->second);
	}
	si = synonymDeletionMap.find(word);
	if (si != synonymDeletionMap.end())
		for (int I = 0; I < synonyms.size(); I++)
			synonyms[I].erase(si->second);
	internalSynonymMap[synonymType][word] = synonyms;
}



// WordNet ANTPTR on ADJ for word; ignoreTopLevel so the queried adjective itself is omitted.
void getAntonyms(lpwstring word, unordered_set <lpwstring>& antonyms, sTrace& t)
{
	LFS
		initWordNet();
	string sWord;
	SynsetPtr sp = findtheinfo_ds(wTM(word, sWord), ADJ, ANTPTR, ALLSENSES);
	extractWordsFromSynset(wTM(word, sWord), sp, 0, antonyms, true, t);
}

// WordNet sense_cnt for word as ADJ or NOUN. Returns 0 if the index lookup misses.
int getFamiliarity(lpwstring word, bool isAdjective)
{
	LFS
		initWordNet();
	string sWord;
	IndexPtr index = index_lookup(wTM(word, sWord), (isAdjective) ? ADJ : NOUN);
	if (index == NULL) return 0;
	return index->sense_cnt;
}

// Max sense_cnt across ADJ/NOUN/VERB/ADV. Returns -1 if word is empty or wTM produced "".
int getHighestFamiliarity(lpwstring word)
{
	LFS
		if (word.empty())
			return -1;
	initWordNet();
	int maxFamiliarity = -1;
	string sWord;
	char* w = wTM(word, sWord);
	if (!*w)
		return -1;
	IndexPtr index = index_lookup(w, ADJ);
	if (index != NULL)
		maxFamiliarity = index->sense_cnt;
	if ((index = index_lookup(w, NOUN)) != NULL)
		maxFamiliarity = max(maxFamiliarity, index->sense_cnt);
	if ((index = index_lookup(w, VERB)) != NULL)
		maxFamiliarity = max(maxFamiliarity, index->sense_cnt);
	if ((index = index_lookup(w, ADV)) != NULL)
		maxFamiliarity = max(maxFamiliarity, index->sense_cnt);
	return maxFamiliarity;
}

// Walks each sense's HYPERPTR chain into a set of noun lemmas (skipping capitalized if
// avoidCapitalizedNouns). Mutates o->ptrlist as it traces. Does not free read_synset results.
void getHyperNyms(SynsetPtr sp, vector < set <string> >& objects, bool avoidCapitalizedNouns, bool print)
{
	LFS
		for (; sp; sp = sp->nextss)
		{
			set <string> oneSenseObjects;
			SynsetPtr o = sp;
			while (o->ptrlist)
			{
				int index = o->ptrlist->hereiam;
				o = read_synset(NOUN, index, 0);
				for (int w = 0; w < o->wcount; w++)
					if (!avoidCapitalizedNouns || islower(o->words[w][0]))
					{
						oneSenseObjects.insert(o->words[w]);
						if (print)
							printf("%s|", o->words[w]);
					}
				o->ptrlist = traceptrs_ds(o, o->searchtype = HYPERPTR, NOUN, 0);
			}
			if (print)
				printf("\n");
			if (oneSenseObjects.size())
				objects.push_back(oneSenseObjects);
		}
}

// Like getHyperNyms but keeps chain order (a "" sentinel starts each hop) per sense.
void getOrderedHyperNyms(SynsetPtr sp, vector < vector <string> >& objects, bool avoidCapitalizedNouns, bool print)
{
	LFS
		for (; sp; sp = sp->nextss)
		{
			vector <string> oneSenseObjects;
			SynsetPtr o = sp;
			while (o->ptrlist)
			{
				if (print)
					printf("\n  ");
				oneSenseObjects.push_back("");
				int index = o->ptrlist->hereiam;
				o = read_synset(NOUN, index, 0);
				for (int w = 0; w < o->wcount; w++)
					if (!avoidCapitalizedNouns || islower(o->words[w][0]))
					{
						oneSenseObjects.push_back(o->words[w]);
						if (print)
							printf("%s|", o->words[w]);
					}
				o->ptrlist = traceptrs_ds(o, o->searchtype = HYPERPTR, NOUN, 0);
			}
			if (print)
				printf("\n");
			if (oneSenseObjects.size())
				objects.push_back(oneSenseObjects);
		}
}

// findtheinfo_ds(word, NOUN, HYPERPTR), trying morphstr if needed. Writes sp; true if non-NULL.
bool initHyperNym(lpwstring word, SynsetPtr& sp)
{
	LFS
		initWordNet();
	string sWord;
	sp = findtheinfo_ds(wTM(word, sWord), NOUN, HYPERPTR, ALLSENSES);
	if (!sp)
	{
		char* mword;
		for (mword = morphstr(wTM(word, sWord), NOUN); mword; mword = morphstr(NULL, NOUN))
			if (sp = findtheinfo_ds(mword, NOUN, HYPERPTR, ALLSENSES)) break;
	}
	return sp != NULL;
}

// Walks HYPERPTR chains of sp. Sets found if MBCSHyperNum occurs in any sense. Returns true
// only if every sense contains that hypernym (numSenses == numHypernymFound).
bool hasHyperNym(SynsetPtr sp, string MBCSHyperNum, bool& found, bool trace)
{
	LFS
		int numSenses = 0, numHypernymFound = 0;
	for (; sp; sp = sp->nextss, numSenses++)
	{
		SynsetPtr o = sp;
		while (o->ptrlist)
		{
			int index = o->ptrlist->hereiam;
			o = read_synset(NOUN, index, 0);
			for (int w = 0; w < o->wcount; w++)
			{
				if (MBCSHyperNum == o->words[w])
				{
					numHypernymFound++;
					found = true;
					if (!trace) break;
				}
				if (trace)
					lplog(LOG_WHERE, u"  SENSE %d:HYPERNYM %S [%d found]", numSenses, o->words[w], numHypernymFound);
			}
			o->ptrlist = traceptrs_ds(o, o->searchtype = HYPERPTR, NOUN, 0);
		}
	}
	return numSenses > 0 && numSenses == numHypernymFound;
}

// initHyperNym(word) then hasHyperNym(sp, hyperNym). found is not cleared first.
bool hasHyperNym(lpwstring word, lpwstring hyperNym, bool& found, bool trace)
{
	LFS
		SynsetPtr sp;
	initHyperNym(word, sp);
	string MBCSHyperNym;
	wTM(hyperNym, MBCSHyperNym);
	if (trace && sp)
		lplog(LOG_WHERE, u"HYPERNYMS of %s:", word.c_str());
	return hasHyperNym(sp, MBCSHyperNym, found, trace);
}

// MBCS→wide then splitMultiWord(lpwstring).
void splitMultiWord(string MBCSMultiWord, vector <lpwstring>& words)
{
	LFS
		lpwstring w;
	splitMultiWord(mTW(MBCSMultiWord, w), words);
}

// Splits multiWord on space or '_' into words (clears words first). Consecutive separators skipped.
void splitMultiWord(lpwstring multiWord, vector <lpwstring>& words)
{
	LFS
		words.clear();
	lpwstring word;
	for (unsigned I = 0; I < multiWord.length(); I++)
		if (multiWord[I] == ' ' || multiWord[I] == '_')
		{
			if (!word.length()) continue;
			words.push_back(word);
			word.erase();
		}
		else
			word += multiWord[I];
	if (word.length())
		words.push_back(word);
}

/*
static struct {
		char *option;		// user's search request
		int search;			// search to pass findtheinfo()
		int pos;			// part-of-speech to pass findtheinfo()
		int helpmsgidx;		// index into help message table
		char *compactLabel;		// text for search header message
} *optptr, optlist[] = {
		{ "-synsa", SIMPTR,	ADJ, 0, "Similarity" },
		{ "-antsa", ANTPTR,	ADJ, 1, "Antonyms" },
		{ "-perta", PERTPTR, ADJ, 0, "Pertainyms" },
		{ "-attra", ATTRIBUTE, ADJ, 2, "Attributes" },
		{ "-domna", CLASSIFICATION, ADJ, 3, "Domain" },
		{ "-domta", CLASS, ADJ, 4, "Domain Terms" },
		{ "-famla", FREQ, ADJ, 5, "Familiarity" },
		{ "-grepa", WNGREP, ADJ, 6, "Grep" },

		{ "-synsn", HYPERPTR, NOUN, 0, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
		{ "-antsn", ANTPTR,	NOUN, 2, "Antonyms" },
		{ "-coorn", COORDS, NOUN, 3, "Coordinate Terms (sisters)" },
		{ "-hypen", -HYPERPTR, NOUN, 4, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
		{ "-hypon", HYPOPTR, NOUN, 5, "Hyponyms" },
		{ "-treen", -HYPOPTR, NOUN, 6, "Hyponyms" },
		{ "-holon", HOLONYM, NOUN, 7, "Holonyms" },
		{ "-sprtn", ISPARTPTR, NOUN, 7, "Part Holonyms" },
		{ "-smemn", ISMEMBERPTR, NOUN, 7, "Member Holonyms" },
		{ "-ssubn", ISSTUFFPTR, NOUN, 7, "Substance Holonyms" },
		{ "-hholn",	-HHOLONYM, NOUN, 8, "Holonyms" },
		{ "-meron", MERONYM, NOUN, 9, "Meronyms" },
		{ "-subsn", HASSTUFFPTR, NOUN, 9, "Substance Meronyms" },
		{ "-partn", HASPARTPTR, NOUN, 9, "Part Meronyms" },
		{ "-membn", HASMEMBERPTR, NOUN, 9, "Member Meronyms" },
		{ "-hmern", -HMERONYM, NOUN, 10, "Meronyms" },
		{ "-nomnn", DERIVATION, NOUN, 11, "Derived Forms" },
		{ "-derin", DERIVATION, NOUN, 11, "Derived Forms" },
		{ "-domnn", CLASSIFICATION, NOUN, 13, "Domain" },
		{ "-domtn", CLASS, NOUN, 14, "Domain Terms" },
		{ "-attrn", ATTRIBUTE, NOUN, 12, "Attributes" },
		{ "-famln", FREQ, NOUN, 15, "Familiarity" },
		{ "-grepn", WNGREP, NOUN, 16, "Grep" },

		{ "-synsv", HYPERPTR, VERB, 0, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
		{ "-simsv", RELATIVES, VERB, 1, "Synonyms (Grouped by Similarity of Meaning)" },
		{ "-antsv", ANTPTR, VERB, 2, "Antonyms" },
		{ "-coorv", COORDS, VERB, 3, "Coordinate Terms (sisters)" },
		{ "-hypev", -HYPERPTR, VERB, 4, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
		{ "-hypov", HYPOPTR, VERB, 5, "Troponyms (hyponyms)" },
		{ "-treev", -HYPOPTR, VERB, 5, "Troponyms (hyponyms)" },
		{ "-tropv", -HYPOPTR, VERB, 5, "Troponyms (hyponyms)" },
		{ "-entav", ENTAILPTR, VERB, 6, "Entailment" },
		{ "-causv", CAUSETO, VERB, 7, "\'Cause To\'" },
		{ "-nomnv", DERIVATION, VERB, 8, "Derived Forms" },
		{ "-deriv", DERIVATION, VERB, 8, "Derived Forms" },
		{ "-domnv", CLASSIFICATION, VERB, 10, "Domain" },
		{ "-domtv", CLASS, VERB, 11, "Domain Terms" },
		{ "-framv", FRAMES, VERB, 9, "Sample Sentences" },
		{ "-famlv", FREQ, VERB, 12, "Familiarity" },
		{ "-grepv", WNGREP, VERB, 13, "Grep" },

		{ "-synsr", SYNS, ADV, 0, "Synonyms" },
		{ "-antsr", ANTPTR, ADV, 1, "Antonyms" },
		{ "-pertr", PERTPTR, ADV, 0, "Pertainyms" },
		{ "-domnr", CLASSIFICATION, ADV, 2, "Domain" },
		{ "-domtr", CLASS, ADV, 3, "Domain Terms" },
		{ "-famlr", FREQ, ADV, 4, "Familiarity" },
		{ "-grepr", WNGREP, ADV, 5, "Grep" },

		{ "-over", OVERVIEW, ALL_POS, -1, "Overview" },
		{ NULL, 0, 0, 0, NULL }
};
*/
// Appends WordNet coordinate (sister) terms of word in wnClass. preferredSense / ignoreSenses
// filter which synsets are used. Writes the count after sense 0 into numFirstSense.
// Returns true if idx was found (objects may be unchanged).
bool addCoords(lpchar_t* word, vector <tmWS >& objects, int wnClass, lpchar_t* preferredSense, int& numFirstSense, set <string>& ignoreSenses, bool print)
{
	LFS
		initWordNet();
	string MBCSWord;
	wTM(word, MBCSWord);
	string MBCSPreferredSense;
	if (preferredSense)
		wTM(preferredSense, MBCSPreferredSense);
	int originalSize = objects.size();
	numFirstSense = 0;
	vector <lpwstring> words;
	IndexPtr idx = index_lookup((char*)MBCSWord.c_str(), wnClass);
	if (!idx) return false;
	for (int sense = 0; sense < idx->off_cnt; sense++)
	{
		if (sense == 1) numFirstSense = objects.size();
		bool foundSense = false, ignoreSense = false;
		SynsetPtr synptr = read_synset(wnClass, idx->offset[sense], idx->wd);
		for (int w = 0; w < synptr->wcount; w++)
		{
			if (!strcmp(MBCSWord.c_str(), synptr->words[w]))
				continue;
			if (preferredSense && !strcmp(MBCSPreferredSense.c_str(), synptr->words[w]))
				foundSense = true;
			if (ignoreSense = ignoreSenses.size() && ignoreSenses.find(synptr->words[w]) != ignoreSenses.end())
			{
				if (print)
					printf("%s ** IGNORED SENSE found **\n", synptr->words[w]);
				break;
			}
			else if (print)
				printf("%s\n", synptr->words[w]);
			splitMultiWord(synptr->words[w], words);
			objects.push_back(words);
		}
		if (ignoreSense) continue;
		for (int i = 0; i < synptr->ptrcount; i++)
		{
			if ((synptr->ptrtyp[i] == HYPERPTR || synptr->ptrtyp[i] == INSTANCE) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword)))
			{
				SynsetPtr cursyn = read_synset(wnClass, synptr->ptroff[i], " ");
				for (int w = 0; w < cursyn->wcount; w++)
				{
					if (preferredSense && !strcmp(MBCSPreferredSense.c_str(), cursyn->words[w]))
						foundSense = true;
					if (print)
						printf("%s\n", cursyn->words[w]);
					splitMultiWord(cursyn->words[w], words);
					objects.push_back(words);
				}
				for (int j = 0; j < cursyn->ptrcount; j++)
				{
					if (cursyn->ptrtyp[j] == HYPOPTR || cursyn->ptrtyp[j] == INSTANCES)
					{
						SynsetPtr cs2 = read_synset(wnClass, cursyn->ptroff[j], "");
						for (int w = 0; w < cs2->wcount; w++)
						{
							if (print)
								printf("%s\n", cs2->words[w]);
							splitMultiWord(cs2->words[w], words);
							objects.push_back(words);
						}
					}
				}
			}
		}
		if (preferredSense)
		{
			if (!foundSense)
				objects.erase(objects.begin() + originalSize, objects.end());
			else
				return objects.size() > 0;
		}
	}
	return objects.size() > 0;
}

// Like addCoords but only the first sense, into a set. Writes the sense count into numSense.
bool addOneSenseCoords(lpchar_t* word, set < lpwstring >& objects, int wnClass, int& numSense)
{
	LFS
		initWordNet();
	string MBCSWord;
	wTM(word, MBCSWord);
	numSense = 0;
	IndexPtr idx = index_lookup((char*)MBCSWord.c_str(), wnClass);
	if (!idx) return false;
	numSense = idx->off_cnt;
	for (int sense = 0; sense < idx->off_cnt; sense++)
	{
		SynsetPtr synptr = read_synset(wnClass, idx->offset[sense], idx->wd);
		lpwstring tw;
		for (int w = 0; w < synptr->wcount; w++)
			if (strcmp(MBCSWord.c_str(), synptr->words[w]))
				objects.insert(mTW(synptr->words[w], tw));
		for (int i = 0; i < synptr->ptrcount; i++)
		{
			if ((synptr->ptrtyp[i] == HYPERPTR || synptr->ptrtyp[i] == INSTANCE) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword)))
			{
				SynsetPtr cursyn = read_synset(wnClass, synptr->ptroff[i], "");
				for (int w = 0; w < cursyn->wcount; w++)
					objects.insert(mTW(cursyn->words[w], tw));
				for (int j = 0; j < cursyn->ptrcount; j++)
				{
					if (cursyn->ptrtyp[j] == HYPOPTR || cursyn->ptrtyp[j] == INSTANCES)
					{
						SynsetPtr cs2 = read_synset(wnClass, cursyn->ptroff[j], "");
						for (int w = 0; w < cs2->wcount; w++)
							objects.insert(mTW(cs2->words[w], tw));
					}
				}
			}
		}
	}
	return objects.size() > 0;
}

// DFS from synset offset 'index', appending hyponym lemmas (depth-tagged) if the sense
// matches preferredSense / is not in ignoreSenses. Sets foundSense when preferredSense hits.
void recurseHyponym(int index, int depth, vector <tmWS >& objects, char* preferredSense, bool& foundSense, set <string>& ignoreSenses, bool print)
{
	LFS
		vector <lpwstring> words;
	SynsetPtr cursyn = read_synset(NOUN, index, "");
	if (print && cursyn->wcount) printf("%*s==>", depth * 2, " ");
	bool ignoreSense = false;
	for (int w = 0; w < cursyn->wcount; w++)
	{
		if (preferredSense && !strcmp(preferredSense, cursyn->words[w]))
			foundSense = true;
		if (ignoreSense = ignoreSenses.size() && ignoreSenses.find(cursyn->words[w]) != ignoreSenses.end())
		{
			if (print)
				printf("%s (IGNORED RECURSIVE SENSE found **)", cursyn->words[w]);
			break;
		}
		else if (print)
			printf("%s,", cursyn->words[w]);
		splitMultiWord(cursyn->words[w], words);
		objects.push_back(words);
	}
	if (print && cursyn->wcount) lp_wprintf(u"\n");
	if (ignoreSense) return;
	for (int k = 0; k < cursyn->ptrcount; k++)
		if (cursyn->ptrtyp[k] == HYPOPTR || cursyn->ptrtyp[k] == INSTANCES)
			recurseHyponym(cursyn->ptroff[k], depth + 1, objects, preferredSense, foundSense, ignoreSenses, print);
}

// Collects hyponyms (and INSTANCES) of word as NOUN, optionally restricted to preferredSense.
// Morphs if the surface form has no index. Returns true if any object was appended.
bool addHyponyms(lpchar_t* word, vector <tmWS >& objects, lpchar_t* preferredSense, set <string>& ignoreSenses, bool print)
{
	LFS
		initWordNet();
	string MBCSPreferredSense, MBCSWord;
	wTM(word, MBCSWord);
	if (preferredSense) wTM(preferredSense, MBCSPreferredSense);
	int originalSize = objects.size();
	vector <lpwstring> words;
	IndexPtr idx = index_lookup((char*)MBCSWord.c_str(), NOUN);
	if (!idx)
	{
		char* mword;
		for (mword = morphstr((char*)MBCSWord.c_str(), NOUN); mword; mword = morphstr(NULL, NOUN))
			if (idx = index_lookup(mword, NOUN)) break;
		if (!idx) return false;
		MBCSWord = mword;
	}
	for (int sense = 0; sense < idx->off_cnt; sense++)
	{
		bool foundSense = false, ignoreSense = false;
		SynsetPtr synptr = read_synset(NOUN, idx->offset[sense], idx->wd);
		if (print && synptr->wcount) printf("PRINCIPAL:");
		for (int w = 0; w < synptr->wcount; w++)
		{
			if (!strcmp(MBCSWord.c_str(), synptr->words[w]))
				continue;
			if (preferredSense && !strcmp((char*)MBCSPreferredSense.c_str(), synptr->words[w]))
				foundSense = true;
			if (ignoreSense = ignoreSenses.size() && ignoreSenses.find(synptr->words[w]) != ignoreSenses.end())
			{
				if (print)
					printf("%s (IGNORED SENSE found **)", synptr->words[w]);
				break;
			}
			else if (print)
				printf("%s,", synptr->words[w]);
			splitMultiWord(synptr->words[w], words);
			objects.push_back(words);
		}
		if (print && synptr->wcount) printf("\n");
		if (ignoreSense) continue;
		for (int i = 0; i < synptr->ptrcount; i++)
		{
			if ((synptr->ptrtyp[i] == HYPOPTR || synptr->ptrtyp[i] == INSTANCES) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword)))
			{
				recurseHyponym(synptr->ptroff[i], 1, objects, (char*)MBCSPreferredSense.c_str(), foundSense, ignoreSenses, print);
			}
		}
		if (preferredSense)
		{
			if (!foundSense)
				objects.erase(objects.begin() + originalSize, objects.end());
			else
				return objects.size() > 0;
		}
	}
	return objects.size() > 0;
}

// Unfiltered hyponym DFS: inserts every single-word hyponym lemma into objects.
void recurseHyponym(int index, int depth, set <lpwstring>& objects, bool print)
{
	LFS
		vector <lpwstring> words;
	lpwstring tw;
	SynsetPtr cursyn = read_synset(NOUN, index, "");
	if (print && cursyn->wcount) printf("%*s==>", depth * 2, " ");
	for (int w = 0; w < cursyn->wcount; w++)
		if (!strchr(cursyn->words[w], ' ') && !strchr(cursyn->words[w], '_'))
			objects.insert(mTW(cursyn->words[w], tw));
	if (print && cursyn->wcount) printf("\n");
	for (int k = 0; k < cursyn->ptrcount; k++)
		if (cursyn->ptrtyp[k] == HYPOPTR || cursyn->ptrtyp[k] == INSTANCES)
			recurseHyponym(cursyn->ptroff[k], depth + 1, objects, print);
}

// Unfiltered NOUN hyponym collection (morph fallback). Returns objects.size() > 0.
bool addHyponyms(lpchar_t* word, set <lpwstring>& objects, bool print)
{
	LFS
		initWordNet();
	string MBCSWord;
	wTM(word, MBCSWord);
	//int originalSize=objects.size();
	vector <lpwstring> words;
	IndexPtr idx = index_lookup((char*)MBCSWord.c_str(), NOUN);
	if (!idx)
	{
		char* mword;
		for (mword = morphstr((char*)MBCSWord.c_str(), NOUN); mword; mword = morphstr(NULL, NOUN))
			if (idx = index_lookup(mword, NOUN)) break;
		if (!idx) return false;
		MBCSWord = mword;
	}
	for (int sense = 0; sense < idx->off_cnt; sense++)
	{
		//bool foundSense=false;
		SynsetPtr synptr = read_synset(NOUN, idx->offset[sense], idx->wd);
		lpwstring stw;
		for (int w = 0; w < synptr->wcount; w++)
		{
			if (!strcmp(MBCSWord.c_str(), synptr->words[w]))
				continue;
			if (!strchr(synptr->words[w], ' ') && !strchr(synptr->words[w], '_'))
				objects.insert(mTW(synptr->words[w], stw));
		}
		if (print && synptr->wcount) printf("\n");
		for (int i = 0; i < synptr->ptrcount; i++)
		{
			if ((synptr->ptrtyp[i] == HYPOPTR || synptr->ptrtyp[i] == INSTANCES) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword)))
			{
				recurseHyponym(synptr->ptroff[i], 1, objects, print);
			}
		}
	}
	return objects.size() > 0;
}

// Picks the synonym of 'in' with the highest WordNet sense_cnt (memoized in mostCommonSynonymMap).
// Fills out, synonyms, familiarity counts, and the live sp/index. Returns out.
lpwstring getMostCommonSynonym(lpwstring in, lpwstring& out, bool isNoun, bool isVerb, bool isAdjective, bool isAdverb,
	SynsetPtr& sp, IndexPtr& index, unordered_set <lpwstring>& synonyms, int& initialFamiliarity, int& highestFamiliarity, sTrace& t)
{
	LFS
		unordered_map <lpwstring, lpwstring>::iterator smi;
	if ((smi = mostCommonSynonymMap.find(in)) == mostCommonSynonymMap.end())
	{
		initWordNet();
		out = in;
		int wnClass = -1;
		if (isNoun) wnClass = NOUN;
		else if (isVerb) wnClass = VERB;
		else if (isAdjective) wnClass = ADJ;
		else if (isAdverb) wnClass = ADV;
		if (wnClass >= 0)
		{
			string inStr;
			sp = findtheinfo_ds(wTM(in, inStr), wnClass, SIMPTR, ALLSENSES);
			index = NULL;
			if (sp)
			{
				extractWordsFromSynset(wTM(in, inStr), sp, 0, synonyms, false, t);
				index = index_lookup(wTM(in, inStr), wnClass);
				if (index)
				{
					initialFamiliarity = highestFamiliarity = index->sense_cnt;
					for (auto si = synonyms.begin(), siEnd = synonyms.end(); si != siEnd; si++)
					{
						IndexPtr synonymIndex = index_lookup(wTM(*si, inStr), wnClass);
						if (synonymIndex && synonymIndex->sense_cnt > highestFamiliarity)
						{
							highestFamiliarity = synonymIndex->sense_cnt;
							out = *si;
						}
					}
				}
			}
		}
		mostCommonSynonymMap[in] = out;
	}
	else
		out = (smi)->second;
	return out;
}

// Convenience overload: stack temporaries for sp/index/synonyms/familiarity.
lpwstring getMostCommonSynonym(lpwstring in, lpwstring& out, bool isNoun, bool isVerb, bool isAdjective, bool isAdverb, sTrace& t)
{
	LFS
		SynsetPtr sp;
	IndexPtr index;
	unordered_set <lpwstring> synonyms;
	int initialFamiliarity;
	int highestFamiliarity;
	return getMostCommonSynonym(in, out, isNoun, isVerb, isAdjective, isAdverb, sp, index, synonyms, initialFamiliarity, highestFamiliarity, t);
}


// If 'in' ends with 'ending', replace that suffix with 'replace' and OR inflectionFlags
// with the matching VERB_* bit. Returns true if a strip happened.
bool stripEndingIfFound(lpwstring& in, const lpchar_t* ending, const lpchar_t* replace, int& inflectionFlags)
{
	LFS
		if (in.length() > lp_strlen(ending) && !lp_strcmp(in.c_str() + in.length() - lp_strlen(ending), ending))
		{
			lpwstring save = in;
			save.erase(save.length() - lp_strlen(ending), lp_strlen(ending));
			save += replace;
			tIWMM w = Words.query(save);
			if (w != Words.end())
			{
				in = save;
				inflectionFlags = w->second.inflectionFlags;
				return true;
			}
		}
	return false;
}

// Morphs 'in' toward a WordNet lemma (strip -ing/-ed/-s etc.) and updates inflectionFlags.
// lastNounNotFound / lastVerbNotFound suppress repeat logs for the same miss.
void deriveMainEntry(int where, int fromWhere, lpwstring& in, int& inflectionFlags, bool isVerb, bool isNoun, lpwstring& lastNounNotFound, lpwstring& lastVerbNotFound)
{
	LFS
		if (isVerb && !(inflectionFlags & VERB_PRESENT_FIRST_SINGULAR))
		{
			if (in == lastVerbNotFound) return;
			if (in == u"ishas" || in == u"wouldhad" || in == u"ishasdoes" || in == u"should") return;
			if (inflectionFlags & VERB_PRESENT_PARTICIPLE)
			{
				if (!stripEndingIfFound(in, u"ing", u"", inflectionFlags) && !stripEndingIfFound(in, u"ing", u"e", inflectionFlags) &&
					!stripEndingIfFound(in, u"nning", u"n", inflectionFlags))
					stripEndingIfFound(in, u"rring", u"r", inflectionFlags);
			}
			else if (inflectionFlags & VERB_PAST)
			{
				if (!stripEndingIfFound(in, u"ied", u"y", inflectionFlags) && !stripEndingIfFound(in, u"ed", u"", inflectionFlags))
					stripEndingIfFound(in, u"ed", u"e", inflectionFlags);
			}
			tIWMM me;
			if ((inflectionFlags & VERB_INFLECTIONS_MASK) && (me = Words.gquery(in)->second.mainEntry) != wNULL &&
				me->first != in && (me->second.inflectionFlags & VERB_PRESENT_FIRST_SINGULAR))
			{
				in = me->first;
				inflectionFlags = me->second.inflectionFlags;
			}
			if ((inflectionFlags & VERB_INFLECTIONS_MASK) && !(inflectionFlags & VERB_PRESENT_FIRST_SINGULAR))
			{
				lpwstring sFlags;
				tIWMM meError = Words.gquery(in);
				if (!(meError->second.flags & cSourceWordInfo::mainEntryErrorNoted))
					lplog(LOG_DICTIONARY, u"%06d:%s irregular verb [mainEntry %s] (%s,%d).", where, in.c_str(),
						(meError->second.mainEntry != wNULL) ? meError->second.mainEntry->first.c_str() : u"",
						inflectionFlagsToStr(inflectionFlags & VERB_INFLECTIONS_MASK, sFlags), fromWhere);
				meError->second.flags |= cSourceWordInfo::mainEntryErrorNoted;
				lastVerbNotFound = in;
			}
		}
	if (isNoun && !(inflectionFlags & SINGULAR))
	{
		if (in == lastNounNotFound) return;
		if (inflectionFlags & PLURAL)
			stripEndingIfFound(in, u"s", u"", inflectionFlags);
		tIWMM me;
		if (!(inflectionFlags & SINGULAR) && (inflectionFlags & NOUN_INFLECTIONS_MASK) && (me = Words.gquery(in)->second.mainEntry) != wNULL &&
			me->first != in && (me->second.inflectionFlags & SINGULAR))
		{
			in = me->first;
			inflectionFlags = me->second.inflectionFlags;
		}
		if (!(inflectionFlags & SINGULAR) && (inflectionFlags & NOUN_INFLECTIONS_MASK))
		{
			lpwstring sFlags;
			if (in == u"many" || in == u"various") return;
			lplog(LOG_DICTIONARY, u"%06d:%s irregular noun not found [mainEntry %s] (%s,%d).", where, in.c_str(),
				(Words.gquery(in)->second.mainEntry != wNULL) ? Words.gquery(in)->second.mainEntry->first.c_str() : u"",
				inflectionFlagsToStr(inflectionFlags & NOUN_INFLECTIONS_MASK, sFlags), fromWhere);
			lastNounNotFound = in;
		}
	}
}

// Among coordinate terms in objects, finds the most familiar one that is in vbNetVerbToClassMap
// and copies that class set onto original. Sets proposedSubstitution / oneSenseVbNetClassFound.
void scanCoordObjects(lpwstring& original, lpwstring& cdstr, set <lpwstring>& objects, int wnClass, int& highestCoordFamiliarity, lpwstring& coordFamiliarity, bool& proposedSubstitution, bool& oneSenseVbNetClassFound)
{
	LFS
		lpwstring tmp;
	unordered_map <lpwstring, set<int> >::iterator inlvtoCi = vbNetVerbToClassMap.end();
	for (set <lpwstring>::iterator oi = objects.begin(), oiEnd = objects.end(); oi != oiEnd; oi++)
	{
		cdstr += *oi;
		string oiStr;
		IndexPtr index = index_lookup(wTM(*oi, oiStr), wnClass);
		if (index)
		{
			cdstr += u"[" + itos(index->sense_cnt, tmp) + u"]";
			if (index->sense_cnt > highestCoordFamiliarity)
			{
				highestCoordFamiliarity = index->sense_cnt;
				coordFamiliarity = *oi;
				proposedSubstitution = true;
			}
			if (wnClass == VERB)
			{
				oneSenseVbNetClassFound |= (inlvtoCi = vbNetVerbToClassMap.find(*oi)) != vbNetVerbToClassMap.end();
				if (inlvtoCi != vbNetVerbToClassMap.end())
				{
					vbNetVerbToClassMap[original].insert(inlvtoCi->second.begin(), inlvtoCi->second.end());
					// lplog(LOG_TIME,u"mapped %s",original.c_str());
				}
			}
		}
		cdstr += u"|";
	}
	if (cdstr.length())
		cdstr.erase(cdstr.length() - 1);
}

// True if 'word' occurs in every sense-set of objects (a hypernym shared by all senses).
bool inEveryGroup(string word, vector < set <string> >& objects)
{
	LFS
		for (vector < set <string> >::iterator oi = objects.begin(), oiEnd = objects.end(); oi != oiEnd; oi++)
			if (oi->find(word) == oi->end())
				return false;
	return true;
}

// Loads CACHEDIR\\wordNetCache\\<in> (renames "con" → "_con_"). Returns false if missing.
bool readHyperNymCache(lpwstring& in, vector < set <string> >& objects)
{
	LFS
		if (in == u"con") in = u"_con_"; // prevent Windows redirection
	lpwstring path = lpwstring(CACHEDIR) + u"\\wordNetCache\\" + in;
	IOHANDLE fd = lp_wopen(path.c_str(), O_RDWR | O_BINARY);
	if (fd < 0) return false;
	void* buffer;
	int bufferlen = lp_filelength(fd);
	buffer = (void*)tmalloc(bufferlen + 10);
	::read(fd, buffer, bufferlen);
	close(fd);
	int where = 0, n;
	if (!copy(n, buffer, where, bufferlen)) return false;
	for (int I = 0; I < n; I++)
	{
		set <string> tobjects;
		if (!copy(tobjects, buffer, where, bufferlen))
			::lplog(LOG_FATAL_ERROR, u"Buffer overrun encountered in read buffer at location %d (limit=%d).", where, bufferlen);
		objects.push_back(tobjects);
	}
	tfree(bufferlen, buffer);
	return true;
}

// Loads CACHEDIR\\wordNetCache\\orderedHyperNyms_<in> into objects.
bool readHyperNymCache(lpwstring& in, vector < vector <string> >& objects)
{
	LFS
		if (in == u"con") in = u"_con_"; // prevent Windows redirection
	lpwstring path = lpwstring(CACHEDIR) + u"\\wordNetCache\\orderedHyperNyms_" + in;
	IOHANDLE fd = lp_wopen(path.c_str(), O_RDWR | O_BINARY);
	if (fd < 0) return false;
	void* buffer;
	int bufferlen = lp_filelength(fd);
	buffer = (void*)tmalloc(bufferlen + 10);
	::read(fd, buffer, bufferlen);
	close(fd);
	int where = 0, n;
	if (!copy(n, buffer, where, bufferlen)) return false;
	objects.reserve(n);
	for (int I = 0; I < n; I++)
	{
		vector <string> tobjects;
		if (!copy(tobjects, buffer, where, bufferlen))
			::lplog(LOG_FATAL_ERROR, u"Buffer overrun encountered in read buffer at location %d (limit=%d).", where, bufferlen);
		objects.push_back(tobjects);
	}
	tfree(bufferlen, buffer);
	return true;
}

// Writes objects to wordNetCache\\<in>. Returns false if open/copy fails (fd leaked on copy fail).
#define MAX_BUF 102400
bool writeHyperNymCache(lpwstring& in, vector < set <string> >& objects)
{
	LFS
		if (in == u"con") in = u"_con_"; // prevent Windows redirection
	lpwstring path = lpwstring(CACHEDIR) + u"\\wordNetCache\\" + in;
	int fd = lp_wopen(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE), where = 0;
	if (fd < 0) return false;
	char buffer[MAX_BUF];
	if (!copy(buffer, (int)objects.size(), where, MAX_BUF)) return false;
	for (unsigned int I = 0; I < objects.size(); I++)
		if (!copy(buffer, objects[I], where, MAX_BUF)) return false;
	if (write(fd, buffer, where) < 0)
	{
		lplog(LOG_FATAL_ERROR, u"Cannot write rdfTypes dbPediaCache - %S.", strerror(errno));
		return false;
	}
	close(fd);
	return true;
}

// Writes ordered hypernyms to wordNetCache\\orderedHyperNyms_<in>. Same leak-on-copy-fail as the set version.
bool writeHyperNymCache(lpwstring& in, vector < vector <string> >& objects)
{
	LFS
		if (in == u"con") in = u"_con_"; // prevent Windows redirection
	lpwstring path = lpwstring(CACHEDIR) + u"\\wordNetCache\\orderedHyperNyms_" + in;
	int fd = lp_wopen(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE), where = 0;
	if (fd < 0) return false;
	char buffer[MAX_BUF];
	if (!copy(buffer, (int)objects.size(), where, MAX_BUF)) return false;
	for (unsigned int I = 0; I < objects.size(); I++)
		if (!copy(buffer, objects[I], where, MAX_BUF)) return false;
	if (write(fd, buffer, where) < 0)
	{
		lplog(LOG_FATAL_ERROR, u"Cannot write rdfTypes dbPediaCache - %S.", strerror(errno));
		return false;
	}
	close(fd);
	return true;
}

// u"communication" includes such things as screaming, etc.
// u"measure" includes milkshake and other things which are measured
// u"attribute" includes all diseases
// u"relation" includes all familial relations (husband)
// u"group" includes all groups like bikers, rockers, skinheads
// u"set" includes tormentor and radio
// Walks hypernyms of 'in' to set measurableObject / notMeasurableObject / grouping
// (physical object vs. abstraction vs. collection). Updates last*NotFound for log suppression.
void analyzeNounClass(int where, int fromWhere, lpwstring in, int inflectionFlags, bool& measurableObject, bool& notMeasurableObject, bool& grouping, sTrace& t, lpwstring& lastNounNotFound, lpwstring& lastVerbNotFound)
{
	LFS
		initWordNet();
	deriveMainEntry(where, fromWhere, in, inflectionFlags, false, true, lastNounNotFound, lastVerbNotFound);
	vector < set <string> > objects;
	if (!readHyperNymCache(in, objects))
	{
		SynsetPtr sp;
		if (!initHyperNym(in, sp)) return;
		getHyperNyms(sp, objects, true, false);
		writeHyperNymCache(in, objects);
	}
	if (inEveryGroup("physical_object", objects) || inEveryGroup("physical_entity", objects))
		measurableObject = true;
	if ((inEveryGroup("psychological_feature", objects) && !inEveryGroup("cognitive_state", objects)) || inEveryGroup("abstraction", objects))
	{
		notMeasurableObject = true;
		measurableObject = false;
	}
	else
	{
		bool found = true;
		for (vector < set <string> >::iterator oi = objects.begin(), oiEnd = objects.end(); oi != oiEnd; oi++)
			if (oi->find("process") == oi->end() && oi->find("abstraction") == oi->end())
				found = false;
		if (found)
		{
			measurableObject = false;
			notMeasurableObject = true;
			if (t.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"mcs %s: physical=%s notPhysical=%s",
					in.c_str(), (measurableObject) ? u"true" : u"false", (notMeasurableObject) ? u"true" : u"false");
		}
	}
	grouping = inEveryGroup("grouping", objects);
	/*
		int initialFamiliarity=-1,highestFamiliarity=-1,numSense=0,wnClass=VERB;
		lpwstring synonym,cdstr,coordFamiliarity,tmp;
		set <lpwstring> synonyms,objects;
		SynsetPtr sp=NULL;
		if (index)
		{
			initialFamiliarity=highestFamiliarity=index->sense_cnt;
		}
		int highestCoordFamiliarity=initialFamiliarity;
		getMostCommonSynonym(in,synonym,false,true,false,false,sp,index,synonyms,initialFamiliarity,highestFamiliarity,t);
		if (synonyms.size())
		{
			int kindFamiliarity;
			lpwstring kindOf=getIsKindOf(in,kindFamiliarity);
			if (kindFamiliarity>highestFamiliarity)
			{
				synonym=kindOf;
				highestFamiliarity=kindFamiliarity;
				//lplog(LOG_RESOLUTION,u"%s is a kind of %s.",in.c_str(),synonym.c_str());
			}
		}
	*/
	if (!measurableObject && !notMeasurableObject && t.traceSpeakerResolution)
		lplog(LOG_WORDNET, u"%d:mcs %s: physical=%s notPhysical=%s", where,
			in.c_str(), (measurableObject) ? u"true" : u"false", (notMeasurableObject) ? u"true" : u"false");
}

// Fills kindOfObjects from cache or initHyperNym+getHyperNyms (then writes the cache).
void getAllHyperNyms(lpwstring in, vector < set <string> >& kindOfObjects)
{
	LFS
		initWordNet();
	if (!readHyperNymCache(in, kindOfObjects))
	{
		SynsetPtr sp;
		if (initHyperNym(in, sp))
			getHyperNyms(sp, kindOfObjects, true, false);
		writeHyperNymCache(in, kindOfObjects);
	}
}

// Ordered-chain cousin of getAllHyperNyms (memory map + disk cache keyed orderedHyperNyms_*).
void getAllOrderedHyperNyms(lpwstring in, vector < vector <string> >& kindOfObjects)
{
	LFS
		if (cacheOrderedHyperNyms)
		{
			// Batch B3: EXCLUSIVE, where the Win32 original took this lock SHARED --
			// a pre-existing bug, not a porting choice. Both branches below write:
			// the miss path inserts into orderedHyperNymsNumMap (which can rehash
			// the whole table) and the hit path increments a counter in it. A shared
			// lock let those writes run concurrently with each other, so porting it
			// as a shared_lock would have carried a real data race forward. The
			// structurally identical cache in createOntology.cpp's
			// getRDFTypesMaster() already took its lock exclusively and even
			// documents why; this is now consistent with it.
			std::unique_lock<std::shared_mutex> orderedHyperNymsLock(orderedHyperNymsMapSRWLock);
			unordered_map<lpwstring, int >::iterator ohnmi;
			if ((ohnmi = orderedHyperNymsNumMap.find(in)) == orderedHyperNymsNumMap.end())
				orderedHyperNymsNumMap[in] = 1;
			else
			{
				(*ohnmi).second++;
				kindOfObjects = orderedHyperNymsMap[in];
				return;
			}
		}
	initWordNet();
	if (!readHyperNymCache(in, kindOfObjects))
	{
		SynsetPtr sp;
		if (initHyperNym(in, sp))
			getOrderedHyperNyms(sp, kindOfObjects, true, false);
		writeHyperNymCache(in, kindOfObjects);
	}
	if (cacheOrderedHyperNyms)
	{
		std::unique_lock<std::shared_mutex> orderedHyperNymsLock(orderedHyperNymsMapSRWLock);
		orderedHyperNymsMap[in] = kindOfObjects;
	}
}

// True if any hypernym set of 'in' contains 'group' (after deriveMainEntry).
bool inWordNetClass(int where, lpwstring in, int inflectionFlags, string group, lpwstring& lastNounNotFound, lpwstring& lastVerbNotFound)
{
	LFS
		initWordNet();
	deriveMainEntry(where, 35, in, inflectionFlags, false, true, lastNounNotFound, lastVerbNotFound);
	vector < set <string> > objects;
	getAllHyperNyms(in, objects);
	for (vector < set <string> >::iterator oi = objects.begin(), oiEnd = objects.end(); oi != oiEnd; oi++)
		if (oi->find(group) != oi->end())
			return true;
	return false;
}

// Maps verb 'in' onto vbNetVerbToClassMap via lemma / most-common synonym / coordinate
// terms. Increments numIrregular when an inflected form cannot be classed. Writes a
// proposedSubstitute if a more familiar synonym was used.
void analyzeVerbNetClass(int where, lpwstring in, lpwstring& proposedSubstitute, int& numIrregular, int inflectionFlags, sTrace& t, lpwstring& lastNounNotFound, lpwstring& lastVerbNotFound)
{
	LFS
		initWordNet();
	deriveMainEntry(where, 37, in, inflectionFlags, true, false, lastNounNotFound, lastVerbNotFound);
	SynsetPtr sp = NULL;
	unordered_set <lpwstring> synonyms;
	set <lpwstring> objects;
	int initialFamiliarity = -1, highestFamiliarity = -1, numSense = 0, wnClass = VERB;
	lpwstring synonym, cdstr, coordFamiliarity, tmp;
	string inStr;
	IndexPtr index = index_lookup(wTM(in, inStr), wnClass);
	if (index)
	{
		initialFamiliarity = highestFamiliarity = index->sense_cnt;
		numSense = index->off_cnt;
	}
	unordered_map <lpwstring, set<int> >::iterator inlvtoCi = vbNetVerbToClassMap.find(in);
	if (inlvtoCi != vbNetVerbToClassMap.end())
	{
		numVbNetClassFound++;
	}
	else
	{
		int highestCoordFamiliarity = initialFamiliarity;
		bool proposedSubstitution = false, oneSenseVbNetClassFound = false, multiSenseVbNetClassFound = false;
		getMostCommonSynonym(in, synonym, false, true, false, false, sp, index, synonyms, initialFamiliarity, highestFamiliarity, t);
		if (numSense == 1)
		{
			if (synonym != in)
			{
				oneSenseVbNetClassFound |= (inlvtoCi = vbNetVerbToClassMap.find(synonym)) != vbNetVerbToClassMap.end();
				if (inlvtoCi != vbNetVerbToClassMap.end())
				{
					vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(), inlvtoCi->second.end());
					//lplog(LOG_TIME,u"mapped %s (2)",in.c_str());
				}
			}
			if (!oneSenseVbNetClassFound)
				for (auto si = synonyms.begin(), siEnd = synonyms.end(); si != siEnd; si++)
					if ((inlvtoCi = vbNetVerbToClassMap.find(*si)) != vbNetVerbToClassMap.end())
					{
						oneSenseVbNetClassFound = true;
						synonym = *si;
						vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(), inlvtoCi->second.end());
						//lplog(LOG_TIME,u"mapped %s (5)",in.c_str());
					}
			if (!oneSenseVbNetClassFound)
			{
				addOneSenseCoords((lpchar_t*)in.c_str(), objects, wnClass, numSense);
				scanCoordObjects(in, cdstr, objects, wnClass, highestCoordFamiliarity, coordFamiliarity, proposedSubstitution, oneSenseVbNetClassFound);
			}
		}
		vector <lpwstring> multiSenseMatchingSynonyms;
		if (numSense > 1)
		{
			if (synonym != in)
			{
				multiSenseVbNetClassFound |= (inlvtoCi = vbNetVerbToClassMap.find(synonym)) != vbNetVerbToClassMap.end();
				if (inlvtoCi != vbNetVerbToClassMap.end())
				{
					vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(), inlvtoCi->second.end());
					//lplog(LOG_TIME,u"mapped %s (3)",in.c_str());
				}
			}
			if (!multiSenseVbNetClassFound)
			{
				if (synonyms.empty())
				{
					addOneSenseCoords((lpchar_t*)in.c_str(), objects, wnClass, numSense);
					scanCoordObjects(in, cdstr, objects, wnClass, highestCoordFamiliarity, coordFamiliarity, proposedSubstitution, multiSenseVbNetClassFound);
				}
				else
					for (auto si = synonyms.begin(), siEnd = synonyms.end(); si != siEnd; si++)
						if ((inlvtoCi = vbNetVerbToClassMap.find(*si)) != vbNetVerbToClassMap.end())
						{
							multiSenseMatchingSynonyms.push_back(*si);
							vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(), inlvtoCi->second.end());
							//lplog(LOG_TIME,u"mapped %s (4)",in.c_str());
							multiSenseVbNetClassFound = true;
						}
			}
			else
				multiSenseMatchingSynonyms.push_back(synonym);
		}
		proposedSubstitution = synonym != in;
		lpwstring inName, outName;
		if (oneSenseVbNetClassFound)
			numOneSenseVbNetClassFound++;
		if (multiSenseVbNetClassFound)
			numMultiSenseVbNetClassFound++;
		if (!oneSenseVbNetClassFound && !multiSenseVbNetClassFound)
		{
			lpwstring sFlags, multiSenseSynonym;
			inflectionFlagsToStr(inflectionFlags & VERB_INFLECTIONS_MASK, sFlags);
			for (auto si = synonyms.begin(), siEnd = synonyms.end(); si != siEnd; si++)
				multiSenseSynonym += u" " + *si;
			if (t.traceSpeakerResolution)
			{
				if (inflectionFlags & VERB_PRESENT_FIRST_SINGULAR)
					lplog(LOG_WORDNET, u"%d:%s vbNet class not found [mainEntry %s] %s - %s.", 1 + numVbNetClassMultiSenseNotFound++, in.c_str(),
						(Words.gquery(in)->second.mainEntry != wNULL) ? Words.gquery(in)->second.mainEntry->first.c_str() : u"",
						((inflectionFlags & VERB_INFLECTIONS_MASK) == VERB_PRESENT_FIRST_SINGULAR) ? u"" : sFlags.c_str(), multiSenseSynonym.c_str());
				else
					lplog(LOG_WORDNET, u"%d:%s irregular class not found", 1 + numIrregular++, in.c_str());
			}
		}
	}
	if (in != synonym)
		proposedSubstitute = synonym;
	else if (in != coordFamiliarity)
		proposedSubstitute = coordFamiliarity;
}

// Loads source\\lists\\nounVerbMapping (binary cache of agentive nominalizations) into
// nounVerbMap, or rebuilds it from WordNet DERIVATION links and writes the cache.
// Returns -1 if the on-disk copy is corrupt; the tmalloc buffer is tfree'd on every path.
int cSource::initializeNounVerbMapping(void)
{
	LFS
		const lpchar_t* path = u"source\\lists\\nounVerbMapping";
	initWordNet();
	int nvfd = lp_wopen(path, O_RDWR | O_BINARY);
	if (nvfd >= 0)
	{
		int bufferlen = lp_filelength(nvfd), where = 0;
		void* buffer = (void*)tmalloc(bufferlen + 10);
		::read(nvfd, buffer, bufferlen);
		close(nvfd);
		int numMappings;
		if (!copy(numMappings, buffer, where, bufferlen)) { tfree(bufferlen + 10, buffer); return -1; }
		for (int I = 0; I < numMappings; I++)
		{
			lpwstring noun;
			set <lpwstring> verbs;
			if (!copy(noun, buffer, where, bufferlen)) { tfree(bufferlen + 10, buffer); return -1; }
			if (!copy(verbs, buffer, where, bufferlen)) { tfree(bufferlen + 10, buffer); return -1; }
			nounVerbMap[noun] = verbs;
		}
		tfree(bufferlen + 10, buffer);
		return 0;
	}
	int readWikiNominalizations(MYSQL & mysql, unordered_map <lpwstring, set < lpwstring > > &agentiveNominalizations);
	readWikiNominalizations(mysql, nounVerbMap);

	char noun[1024];
	// Only reached when source/lists/nounVerbMapping is missing (the branch above
	// returns when it opens). This is an external WordNet 2.1 installation, not
	// part of this tree or of caches/, and it is not present on macOS -- the path
	// was the author's Windows install, "F:\\Program Files (x86)\\WordNet\\2.1\\
	// dict\\index.noun". Overridable with LP_WORDNET_DICT for a machine that does
	// have one. The NULL check is new: fopen's result was passed straight to
	// fgets(), so a missing file segfaulted here rather than reporting anything.
	const char* wordNetIndexNoun = getenv("LP_WORDNET_DICT");
	if (!wordNetIndexNoun || !*wordNetIndexNoun)
		wordNetIndexNoun = "/usr/local/WordNet-2.1/dict/index.noun";
	FILE* nounfp = fopen(wordNetIndexNoun, "r");
	if (!nounfp)
	{
		lplog(LOG_ERROR, u"initializeNounVerbMapping: cannot open WordNet index %S - %S. "
			u"Rebuild source/lists/nounVerbMapping, or set LP_WORDNET_DICT to an index.noun.",
			wordNetIndexNoun, strerror(errno));
		return -1;
	}
	while (fgets(noun, 1024, nounfp))
	{
		char* ch = strchr(noun, ' ');
		if (ch) *ch = 0;
		IndexPtr idx = NULL;
		int offsetcnt = 0;
		unsigned long senseOffsets[MAXSENSE];

		for (int i = 0; i < MAXSENSE; i++)
			senseOffsets[i] = 0;

		char* cnoun = noun;
		set <lpwstring> verbs;
		while ((idx = getindex(cnoun, NOUN)) != NULL)
		{
			cnoun = NULL;
			/* Go through all of the searchword's senses in the database and perform the search requested. */
			for (int sense = 0; sense < idx->off_cnt; sense++)
			{
				int skipit = 0;
				/* Determine if this synset has already been done with a different spelling. If so, skip it. */
				for (int i = 0; i < offsetcnt && !skipit; i++)
				{
					if (senseOffsets[i] == idx->offset[sense])
						skipit = 1;
				}
				if (skipit != 1)
				{
					senseOffsets[offsetcnt++] = idx->offset[sense];
					SynsetPtr cursyn = read_synset(NOUN, idx->offset[sense], idx->wd);
					for (int i = 0; i < cursyn->ptrcount; i++)
					{
						if ((cursyn->ptrtyp[i] == DERIVATION) && (cursyn->pfrm[i] == cursyn->whichword) && cursyn->ppos[i] == VERB)
						{
							SynsetPtr cursyn2 = read_synset(cursyn->ppos[i], cursyn->ptroff[i], "");
							lpwstring tsynw;
							verbs.insert(mTW(cursyn2->words[cursyn->pto[i] - 1], tsynw));
							free_synset(cursyn2);
						}
					}
					free_synset(cursyn);
				} /* end if (skipit) */
			} /* end for (sense) */
			free_index(idx);
		} /* end while (idx) */
		if (!verbs.empty())
		{
			lpwstring wnoun;
			mTW(noun, wnoun);
			nounVerbMap[wnoun] = verbs;
		}
	}
	fclose(nounfp);
	int where = 0, nounVerbMapSize = nounVerbMap.size();
	nvfd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
	if (nvfd < 0)
	{
		lplog(LOG_ERROR, u"ERROR:Unable to open %s - %S. (1)", path, strerror(errno));
		return false;
	}
	char buffer[MAX_BUF];
	if (!copy(buffer, nounVerbMapSize, where, MAX_BUF)) return false;
	for (unordered_map <lpwstring, set < lpwstring > >::iterator nvi = nounVerbMap.begin(), nviEnd = nounVerbMap.end(); nvi != nviEnd; nvi++)
	{
		if (!copy(buffer, nvi->first, where, MAX_BUF)) return -1;
		if (!copy(buffer, nvi->second, where, MAX_BUF)) return -1;
		if (where > MAX_BUF - 1024)
		{
			if (::write(nvfd, buffer, where) < 0) return -1;
			where = 0;
		}
	}
	if (where && ::write(nvfd, buffer, where) < 0)
	{
		lplog(LOG_FATAL_ERROR, u"Cannot write rdfTypes dbPediaCache - %S.", strerror(errno));
		return -1;
	}
	close(nvfd);
	return 0;
}

