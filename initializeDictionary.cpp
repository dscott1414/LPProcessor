/*
	initializeDictionary.cpp - Closed-class lexicon bootstrap and word-cache I/O

	Overview:
		Builds the in-memory word/form tables before any source is read: predefines
		pronouns, determiners, verbs, honorifics, abbreviations, numbers, letters,
		prepositions, interjections; loads Levin verb classes, gendered nouns,
		demonyms, place names, nicknames, and multi-word objects from source\\lists;
		then sets form flags (isCommonForm, isVerbForm, isIgnore) and per-form costs.
		Also serializes/restores the word cache (.wordCacheFile).

	Pipeline position:
		Stage 1 (Initialization). cWord::initialize() is the top-level entry, called
		once before tokenize. readWords() reloads a per-source or global cache.

	Key entry points:
		- initialize / createWordCategories / findPredefinedForms / initializeCosts
		- predefineWord / predefineWords / predefineVerbsFromFile
		- readVerbClasses / readVerbClassNames / addGenderedNouns / addPlaces
		- readWords / writeWord / readFormsCache / writeFormsCache

	Key data structures / globals:
		- levinVerbToClassSectionMap / levinClassSectionNames - Levin class IDs packed
		  as four 8-bit fields in an int (<<24/16/8/0)
		- WMM / Forms - the global lexicon this file fills
		- nicknameEquivalenceMap - loaded from male/female nickname lists

	Dependencies:
		source\\lists\\*.txt (Levin, names, places, nicknames, verb inflection files);
		readVBNet() from vcXML.cpp; WordNet only indirectly via later getForms.

	Notes / gotchas:
		predefineWords(Inflections[]) temporarily swaps an apostrophe to U+02BC and restores it
		(self-reverting, no lasting effect on the caller's array). predefineWords(InflectionsRoot[])
		permanently rewrites spaces to dashes in words[].word with no restore - a real mutation of
		the caller's storage, but every current call site passes a freshly-constructed local array
		consumed by exactly one predefineWords() call and never read again, so it has no observable
		effect today. It would matter if a future caller reused/shared an InflectionsRoot[] array
		across multiple calls or kept reading it afterward; flagged here rather than restructured,
		since there is nothing to fix without a caller that is actually affected.
		readWords tfrees its tmalloc buffer on every early-return path now.
		disqualify() rejects most dotted tokens unless they look like A.B.C. abbreviations.
		gquery fatal-errors if a required sentinel word is missing.
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
#include <sys/stat.h>
#include "word.h"
#include "names.h"
#include "time.h" // clock
#include "wn.h"
#include "profile.h"
unordered_map <int, lpwstring> levinClassSectionToVerbMap; // initialized
unordered_map <lpwstring, int> levinVerbToClassSectionMap; // dictionary initialized 
unordered_map <int, lpwstring> levinClassSectionNames; // dictionary initialized 

// Finds sWord in WMM or LOG_FATAL_ERROR. Used for required sentinel words (__ppn__, …).
tIWMM cWord::gquery(lpwstring sWord)
{
	LFS
		tIWMM iWord = WMM.find(sWord);
	if (iWord == end())
		lplog(LOG_FATAL_ERROR, u"FATAL ERROR: word %s not found.", sWord.c_str());
	return iWord;
}

// Adds 'word' as its own form name (closed-class sentinel). Returns the new/existing iterator.
tIWMM cWord::predefineWord(const lpchar_t* word, int flags)
{
	LFS
		unsigned int iForm = cForms::addNewForm(word, word, false);
	handleExtendedParseWords((lpchar_t*)word);
	bool added;
	return addNewOrModify(NULL, word, flags, iForm, 0, 0, u"", -1, added);
}

// Adds each NULL-terminated words[] entry under sForm. Returns the form index.
int cWord::predefineWords(const lpchar_t* words[], lpwstring sForm, lpwstring shortForm, int flags, bool properNounSubClass)
{
	LFS
		unsigned int iForm = cForms::addNewForm(sForm, shortForm, false, properNounSubClass);
	for (int I = 0; words[I]; I++)
	{
		handleExtendedParseWords(words[I]);
		bool added;
		addNewOrModify(NULL, words[I], flags, iForm, 0, 0, u"", -1, added);
	}
	return iForm;
}

// Adds Inflections[] (word + inflection bits). Mutates any apostrophe in the caller's
// buffers to U+02BC and back so both spellings are stored. Returns 0.
int cWord::predefineWords(Inflections words[], lpwstring sForm, lpwstring shortName, lpwstring inflectionsClass, int flags, bool properNounSubClass)
{
	LFS
		size_t aForm = cForms::createForm(sForm, shortName, true, inflectionsClass, properNounSubClass);
	for (int I = 0; words[I].word[0]; I++)
	{
		handleExtendedParseWords(words[I].word);
		bool added;
		addNewOrModify(NULL, words[I].word, flags, aForm, words[I].inflection, 0, u"", -1, added);
		lpchar_t* quote;
		if (quote = lp_strchr(words[I].word, u'\''))
		{
			*quote = u'ʼ';
			handleExtendedParseWords(words[I].word);
			addNewOrModify(NULL, words[I].word, flags, aForm, words[I].inflection, 0, u"", -1, added);
			*quote = u'\'';
		}
	}
	return 0;
}

// Adds InflectionsRoot[] (word + inflection + mainEntry). Permanently changes spaces in
// the caller's word buffers to dashes (not restored) so both spellings are stored; safe with
// every current caller (a freshly-built, single-use local array) but see the file header note.
int cWord::predefineWords(InflectionsRoot words[], lpwstring sForm, lpwstring shortName, lpwstring inflectionsClass, int flags, bool properNounSubClass)
{
	LFS
		unsigned int aForm = cForms::createForm(sForm, shortName, true, inflectionsClass, properNounSubClass);
	for (int I = 0; words[I].word[0]; I++)
	{
		handleExtendedParseWords(words[I].word);
		bool added;
		addNewOrModify(NULL, words[I].word, flags, aForm, words[I].inflection, 0, words[I].mainEntry, -1, added);
		// check for possible spaces and add dashes if only one space
		bool containsSpaces = false;
		for (unsigned int J = 0; words[I].word[J]; J++)
			if (words[I].word[J] == ' ')
			{
				containsSpaces = true;
				words[I].word[J] = '-';
			}
		if (containsSpaces)
		{
			handleExtendedParseWords(words[I].word);
			addNewOrModify(NULL, words[I].word, flags, aForm, words[I].inflection, 0, words[I].mainEntry, -1, added);
		}
	}
	return 0;
}

// Reads four-column verb rows (1sg past presPart 3sg) from path. BOM-strips 65279.
// LOG_FATAL_ERROR if the file is missing. Returns 0.
int cWord::predefineVerbsFromFile(lpwstring sForm, lpwstring shortName, const lpchar_t* path, int flags)
{
	LFS
		const lpchar_t* inflectionsClass = u"verb";
	bool properNounSubClass = false;
	FILE* tf = lp_wfopen(path, "rb");
	if (!tf)
	{
		lplog(LOG_FATAL_ERROR, u"verb list %s is missing.", path);
		return -1;
	}
	unsigned int aForm = cForms::createForm(sForm, shortName, true, inflectionsClass, properNounSubClass);
	while (true)
	{
		lpchar_t firstSingular[1024], past[1024], presentParticiple[1024], thirdSingular[1024];
		if (lp_fwscanf(tf, u"%s %s %s %s", firstSingular, past, presentParticiple, thirdSingular) != 4)
			break;
		if (firstSingular[0] == 65279) // BOM
			lp_strcpy(firstSingular, firstSingular + 1);
		handleExtendedParseWords(firstSingular);
		handleExtendedParseWords(past);
		handleExtendedParseWords(presentParticiple);
		handleExtendedParseWords(thirdSingular);
		bool added;
		addNewOrModify(NULL, firstSingular, flags, aForm, VERB_PRESENT_FIRST_SINGULAR, 0, firstSingular, -1, added);
		addNewOrModify(NULL, past, flags, aForm, VERB_PAST, 0, firstSingular, -1, added);
		addNewOrModify(NULL, presentParticiple, flags, aForm, VERB_PRESENT_PARTICIPLE, 0, firstSingular, -1, added);
		addNewOrModify(NULL, thirdSingular, flags, aForm, VERB_PRESENT_THIRD_SINGULAR, 0, firstSingular, -1, added);
	}
	fclose(tf);
	return 0;
}

// Parses source\\lists\\levinVerbClasses.txt ("verb : 2.10, 33") into the Levin maps.
// Class numbers are packed as four 8-bit fields. Returns false on a malformed line.
bool cWord::readVerbClasses(void)
{
	LFS
		const lpchar_t* path;
	FILE* tf = lp_wfopen(path = u"source\\lists\\levinVerbClasses.txt", "rb");
	if (!tf)
	{
		lplog(LOG_FATAL_ERROR, u"verb class list %s is missing.", path);
		return false;
	}
	lpchar_t line[1024];
	// acclaim : 2.10, 2.13.1, 2.13.2, 2.13.3, 33 
	while (lp_fgetws(line, 1023, tf))
	{
		if (lp_strchr(line, '/')) continue;
		lpchar_t* ch = lp_strchr(line, u':'), * ch2, * period;
		if (!ch)
		{
			fclose(tf);
			return false;
		}
		ch[-1] = 0;
		lpwstring verb = line;
		ch += 2;
		while (true)
		{
			if (ch2 = lp_strchr(ch, u',')) *ch2 = 0;
			int classSection = 0;
			while (true)
			{
				if (period = lp_strchr(ch, u'.')) *period = 0;
				classSection += lp_wtoi(ch) << 24;
				if (!period) break;
				if (period = lp_strchr(ch = period + 1, u'.')) *period = 0;
				classSection += lp_wtoi(ch) << 16;
				if (!period) break;
				if (period = lp_strchr(ch = period + 1, u'.')) *period = 0;
				classSection += lp_wtoi(ch) << 8;
				if (!period) break;
				if (period = lp_strchr(ch = period + 1, u'.')) *period = 0;
				classSection += lp_wtoi(ch);
				if (!period) break;
			}
			levinClassSectionToVerbMap[classSection] = verb;
			levinVerbToClassSectionMap[verb] = classSection;
			if (!ch2) break;
			ch = ch2 + 2;
		}
	}
	fclose(tf);
	return true;
}

// Parses levinVerbCategories.txt into levinClassSectionNames. LOG_FATAL_ERROR if missing.
bool cWord::readVerbClassNames(void)
{
	LFS
		const lpchar_t* path;
	FILE* tf = lp_wfopen(path = u"source\\lists\\levinVerbCategories.txt", "rb");
	if (!tf)
		lplog(LOG_FATAL_ERROR, u"verb class category list %s is missing.", path);
	lpchar_t line[1024];
	//  01234567890
	//   01.1.2.1  Causative/Inchoative Alternation
	while (lp_fgetws(line, 1023, tf))
	{
		if (lp_strstr(line, u"//")) continue;
		lpchar_t* ch, * period, * cutoff;
		if (ch = lp_strchr(line, 13)) *ch = 0;
		if (cutoff = lp_strchr(line + 11, u'\"')) *cutoff = 0;
		if (cutoff = lp_strstr(line + 11, u" verbs")) *cutoff = 0;
		lpwstring verbClassName = line + 11;
		line[10] = 0;
		ch = line + 1;
		int classSection = 0;
		while (true)
		{
			if (period = lp_strchr(ch, u'.')) *period = 0;
			classSection += lp_wtoi(ch) << 24;
			if (!period) break;
			if (period = lp_strchr(ch = period + 1, u'.')) *period = 0;
			classSection += lp_wtoi(ch) << 16;
			if (!period) break;
			if (period = lp_strchr(ch = period + 1, u'.')) *period = 0;
			classSection += lp_wtoi(ch) << 8;
			if (!period) break;
			if (period = lp_strchr(ch = period + 1, u'.')) *period = 0;
			classSection += lp_wtoi(ch);
			if (!period) break;
		}
		levinClassSectionNames[classSection] = verbClassName;
	}
	fclose(tf);
	return true;
}

// Reads a SSA-style Rank,Male,Number,Female,Number file and adds the names as proper nouns
// with MALE/FEMALE_GENDER. Skips three header lines. Returns -1 if path cannot be opened.
int cWord::addProperNamesFile(lpwstring path)
{
	LFS
		FILE* fp = lp_wfopen(path.c_str(), "rb");
	if (!fp) return -1;
	lpchar_t line[1024];
	lp_fgetws(line, 1024, fp);
	lp_fgetws(line, 1024, fp);
	lp_fgetws(line, 1024, fp);
	//Rank,Male,Number,Female,Number
	// 9,Edward,7428,Mildred,5800
	while (lp_fgetws(line, 1024, fp))
	{
		int rank;
		lpchar_t* ch = line;
		while (*ch != ',' && *ch) ch++;
		if (!*ch) continue;
		*ch = 0;
		rank = lp_wtoi(line);
		lpchar_t* savech = ++ch;
		while (*ch != ',' && *ch) ch++;
		if (!*ch) continue;
		*ch = 0;
		//lpwstring maleName=savech;
		bool added;
		int flags = (query(savech) == end()) ? cSourceWordInfo::queryOnLowerCase : 0;
		addNewOrModify(NULL, savech, flags, PROPER_NOUN_FORM_NUM/*MALE_GENDER+(rank<<8)+year*/, MALE_GENDER | MALE_GENDER_ONLY_CAPITALIZED, 0, savech, -1, added);
		savech = ++ch;
		while (*ch != ',' && *ch) ch++;
		if (!*ch) continue;
		*ch = 0;
		savech = ++ch;
		while (*ch != ',' && *ch) ch++;
		if (!*ch) continue;
		*ch = 0;
		//lpwstring femaleName=savech;
		flags = (query(savech) == end()) ? cSourceWordInfo::queryOnLowerCase : 0;
		addNewOrModify(NULL, savech, flags, PROPER_NOUN_FORM_NUM/*FEMALE_GENDER+(rank<<8)+year*/, FEMALE_GENDER | FEMALE_GENDER_ONLY_CAPITALIZED, 0, savech, -1, added);
	}
	fclose(fp);
	return 0;
}

#define MAX_BUF 32000

// Serializes iWord's string + cSourceWordInfo into buffer at where. Returns -1 if copy fails.
int cWord::writeWord(tIWMM iWord, void* buffer, int& where, int limit)
{
	LFS
		if (iWord->second.mainEntry == wNULL)
			lplog(LOG_ERROR, u"mainentry is NULL for word %s!", iWord->first.c_str());
	if (!copy(buffer, iWord->first, where, limit))
		return -1;
	iWord->second.write(buffer, where, limit);
	return 0;
}

// Reads the form-table prefix of a .wordCacheFile. Creates any form name not already in
// Forms. Returns the new 'where', or -1 on copy failure.
int cWord::readFormsCache(char* buffer, int bufferlen, int& numReadForms)
{
	LFS
		int where = 0;
	unsigned int numForms;
	if (!copy(numForms, buffer, where, bufferlen)) return -1;
	numReadForms = numForms;
	for (unsigned int iForm = 0; iForm < numForms; iForm++)
	{
		lpwstring name, shortName, inflectionsClass;
		if (!copy(name, buffer, where, bufferlen)) return -1;
		if (!copy(shortName, buffer, where, bufferlen)) return -1;
		if (!copy(inflectionsClass, buffer, where, bufferlen)) return -1;
		short hasInflections, properNounSubClass, isTopLevel, isIgnore, isVerbForm, blockProperNounRecognition, formCheck;
		if (!copy(hasInflections, buffer, where, bufferlen)) return -1;
		if (!copy(properNounSubClass, buffer, where, bufferlen)) return -1;
		if (!copy(isTopLevel, buffer, where, bufferlen)) return -1;
		if (!copy(isIgnore, buffer, where, bufferlen)) return -1;
		if (!copy(isVerbForm, buffer, where, bufferlen)) return -1;
		if (!copy(blockProperNounRecognition, buffer, where, bufferlen)) return -1;
		if (!copy(formCheck, buffer, where, bufferlen)) return -1;
		//lplog(u"%d:hasInflections=%d form=%s shortForm=%s inflectionsClass=%s",where,(int)hasInflections,name.c_str(),shortName.c_str(),inflectionsClass.c_str());
		//int f=cForms::createForm(name,shortName,hasInflections!=0,inflectionsClass,properNounSubClass!=0);
		if (cForms::findForm(name) < 0)
		{
			Forms.push_back(new cForm(-1, name, shortName, inflectionsClass, hasInflections != 0, properNounSubClass != 0, isTopLevel != 0, isIgnore != 0, isVerbForm != 0, blockProperNounRecognition != 0, formCheck != 0));
			cForms::formMap[name] = Forms.size() - 1;
		}
	}
	return where;
}

// Writes Forms.size() then each cForm to fd. LOG_FATAL_ERROR on write failure. Returns bytes written.
int cWord::writeFormsCache(int fd)
{
	LFS
		int where, numForms = (int)Forms.size(), len = (int)sizeof(numForms);
	if (::write(fd, &numForms, sizeof(numForms)) < 0)
		lplog(LOG_FATAL_ERROR, u"Out of space writing forms cache.");
	if (logDatabaseDetails)
		lplog(u"%d:writing %d forms.", len, numForms);
	int I = 0;
	for (vector <cForm*>::iterator fc = Forms.begin(), fcend = Forms.end(); fc != fcend; fc++, I++)
	{
		char buffer[MAX_BUF];
		where = 0;
		//lplog(u"%d:%d:wrote form %s.",len,I,(*fc)->name.c_str());
		(*fc)->write(buffer, where, MAX_BUF);
		if (::write(fd, buffer, where) < 0)
			lplog(LOG_FATAL_ERROR, u"Out of space writing forms cache.");
		len += where;
	}
	return len;
}

// True if sWord is too long, trailing-space, >2 dashes, or a dotted token that is not X.X.X.
bool disqualify(lpwstring sWord)
{
	LFS
		if (sWord.length() > MAX_WORD_LENGTH)
			return true;
	if (sWord[sWord.length() - 1] == ' ')
	{
		lplog(LOG_ERROR, u"Word '%s' has a space at the end... Rejected (4).", sWord.c_str());
		return true;
	}
	if (sWord == u"--" || sWord == u"." || sWord == u"...")
		return false;
	int dashes = 0;
	for (unsigned I = 0; I < sWord.length(); I++)
		if (cWord::isDash(sWord[I])) dashes++;
	if (dashes > 2) return true;
	if (sWord.find('.') == lpwstring::npos)
		return false;
	// word may only have periods if it is a real abbreviation X.X.X.X. ...
	// all other abbreviations must have already been introduced through initializeDictionary
	for (const lpchar_t* w = sWord.c_str(); *w; w += 2)
	{
		if (*w < 0 || !iswalpha(*w))
			return true;
		if (!w[1] || w[1] != '.')
			return true;
	}
	return false;
}

// Loads oPath+".wordCacheFile" into WMM/Forms and resolves mainEntry links. Returns -1 if
// the file is missing or a copy fails (tmalloc buffer leaked on those paths).
int cWord::readWords(lpwstring oPath, int sourceId, bool disqualifyWords, lpwstring specialExtension)
{
	LFS
		oPath += u".wordCacheFile";
	int fd = lp_wopen(oPath.c_str(), O_RDONLY | O_BINARY);
	if (fd < 0) return -1;
	char* buffer;
	int bufferlen = lp_filelength(fd);
	buffer = (char*)tmalloc(bufferlen + 10);
	::read(fd, buffer, bufferlen);
	close(fd);
	int where = 0, numReadForms;
	where = readFormsCache(buffer, bufferlen, numReadForms);
	if (where < 0) { tfree(bufferlen + 10, buffer); return -1; }
	// for preferVerbPresentParticiple contained in the cSourceWordInfo constructor
	if (nounForm == -1) nounForm = cForms::gFindForm(u"noun");
	if (adjectiveForm < 0) adjectiveForm = cForms::gFindForm(u"adjective");
	if (adverbForm < 0) adverbForm = cForms::gFindForm(u"adverb");
	if (prepositionForm < 0) prepositionForm = cForms::gFindForm(u"preposition");
	cForms::changedForms = false;
	lpwstring sWord;
	tIWMM iWord = wNULL;
	vector <lpwstring> mainEntries;
	vector <tIWMM> entries;
	bool rejectionTitleIsPrinted = false;
	//int numPreps=0;
	while (where < bufferlen)
	{
		//int saveWhere=where;
		if (!copy(sWord, buffer, where, bufferlen)) { tfree(bufferlen + 10, buffer); return -1; }
		lpwstring sME;
		if (disqualifyWords && disqualify(sWord))
		{
			if (!rejectionTitleIsPrinted)
			{
				lplog(LOG_ERROR, u"These words from word cache file %s are rejected:", oPath.c_str());
				rejectionTitleIsPrinted = true;
			}
			lplog(LOG_ERROR, u"    %s", sWord.c_str());
			cSourceWordInfo(buffer, where, bufferlen, sME, sourceId); // advance 'where' in buffer
			continue;
		}
		if ((iWord = WMM.find(sWord)) != WMM.end())
		{
			if (iWord->second.updateFromDisk(buffer, where, bufferlen, sME))
			{
				entries.push_back(iWord);
				mainEntries.push_back(sME);
#ifdef LOG_WORD_FLOW
				lplog(u"Updated word %s.", sWord.c_str());
#endif
				for (unsigned int K = 0; K < iWord->second.formsSize(); K++)
				{
					if (iWord->second.forms()[K] >= (unsigned)numReadForms)
						lplog(LOG_FATAL_ERROR, u"Word %s has illegal form #%d (out of max %d)!", sWord.c_str(), iWord->second.forms()[K], numReadForms);

				}
			}
#ifdef LOG_WORD_FLOW
			else
				lplog(u"Rejected word %s.", sWord.c_str());
#endif
		}
		else
		{
			iWord = WMM.insert(tWFIMap(sWord, cSourceWordInfo(buffer, where, bufferlen, sME, sourceId))).first;
			entries.push_back(iWord);
			mainEntries.push_back(sME);
			handleExtendedParseWords((lpchar_t*)sWord.c_str());
			for (unsigned int K = 0; K < iWord->second.formsSize(); K++)
			{
				if (iWord->second.forms()[K] >= (unsigned)numReadForms)
					lplog(LOG_FATAL_ERROR, u"Word %s has illegal form #%d (out of max %d)!", sWord.c_str(), iWord->second.forms()[K], numReadForms);

			}
		}
	}
	for (unsigned int I = 0; I < mainEntries.size(); I++)
		if (mainEntries[I].length())
		{
			iWord = WMM.find(mainEntries[I]);
			if (iWord == end())
			{
				entries[I]->second.flags |= cSourceWordInfo::queryOnAnyAppearance;
				lplog(LOG_FATAL_ERROR, u"MainEntry %s not found in memory for word %s!", mainEntries[I].c_str(), entries[I]->first.c_str());
			}
			else
			{
				entries[I]->second.mainEntry = iWord;
				if (iWord != wNULL)
					mainEntryMap[iWord->first].push_back(entries[I]);
			}
		}
	tfree(bufferlen + 10, buffer);
	tIWMM w = begin(), wEnd = end();
	for (w = begin(); w != wEnd; w++)
		if (w->second.mainEntry == wNULL && iswalpha(w->first[0]) &&
			(w->second.query(nounForm) >= 0 ||
				w->second.query(verbForm) >= 0 ||
				w->second.query(adjectiveForm) >= 0 ||
				w->second.query(adverbForm) >= 0))
		{
			if (logDetail)
				lplog(LOG_ERROR, u"Word %s has no mainEntry after reading wordcache!", w->first.c_str());
			w->second.flags |= cSourceWordInfo::queryOnAnyAppearance;
			w->second.mainEntry = w;
		}
	return 0;
}

// Reads genPath (one noun per line; optional +HYPO/+COORDS and (sense)). Adds each as
// wordForm with defaultInflectionFlags; optionally expands WordNet hyponyms/coords.
// Returns -1 if the file is missing.
int cWord::addGenderedNouns(const lpchar_t* genPath, int defaultInflectionFlags, int wordForm)
{
	LFS
		FILE* fgen = lp_wfopen(genPath, "rb"); // binary mode reads unicode
	if (!fgen) return -1;
	lpchar_t noun[101];
	while (lp_fgetws(noun, 100, fgen))
	{
		if (noun[0] == 0xFEFF) // detect BOM
			memcpy(noun, noun + 1, (lp_strlen(noun + 1) + 1) * sizeof(noun[0]));
		if (noun[lp_strlen(noun) - 1] == '\n') noun[lp_strlen(noun) - 1] = 0;
		if (noun[lp_strlen(noun) - 1] == '\r') noun[lp_strlen(noun) - 1] = 0; // in binary mode, cr/lf is not translated
		bool addWordNetSearch = false, hypo = false;
		lpchar_t* preferredSense = NULL, * ch;
		if ((hypo = (ch = lp_strstr(noun, u" +HYPO")) != NULL) || (ch = lp_strstr(noun, u" +COORDS")))
		{
			addWordNetSearch = true;
			*ch = 0;
			lpchar_t* chsense = lp_strchr(ch + 1, '('), * chsense2 = lp_strchr(ch + 1, ')');
			if (chsense && chsense2)
			{
				*chsense2 = 0;
				preferredSense = chsense + 1;
			}
		}
		while (noun[0] && isspace(noun[lp_strlen(noun) - 1]))
			noun[lp_strlen(noun) - 1] = 0;
		handleExtendedParseWords(noun);
		bool added;
		tIWMM word = query(noun);
		if (word == end())
			addNewOrModify(NULL, noun, cSourceWordInfo::queryOnAnyAppearance, wordForm, defaultInflectionFlags, 0, noun, -1, added);
		else
		{
			int inflectionFlags = word->second.inflectionFlags;
			if (!(inflectionFlags & (MALE_GENDER | FEMALE_GENDER))) inflectionFlags |= defaultInflectionFlags;
			addNewOrModify(NULL, noun, 0, wordForm, inflectionFlags, 0, noun, -1, added);
		}
		if (addWordNetSearch)
		{
			vector <tmWS > objects;
			set <string> ignoreCategories;
			int numFirstSense;
			if (hypo)	addHyponyms(noun, objects, preferredSense, ignoreCategories, false);
			else addCoords(noun, objects, NOUN, preferredSense, numFirstSense, ignoreCategories, false);
			for (unsigned int I = 0; I < objects.size(); I++)
			{
				// toss multiword professions for now
				if (objects[I].ws.size() == 1)
				{
					handleExtendedParseWords((lpchar_t*)objects[I].ws[0].c_str());
					word = query(objects[I].ws[0].c_str());
					if (word == end())
						addNewOrModify(NULL, objects[I].ws[0].c_str(), cSourceWordInfo::queryOnAnyAppearance, wordForm, defaultInflectionFlags, 0, objects[I].ws[0].c_str(), -1, added);
					else
					{
						int inflectionFlags = word->second.inflectionFlags;
						if (!(inflectionFlags & (MALE_GENDER | FEMALE_GENDER))) inflectionFlags |= defaultInflectionFlags;
						addNewOrModify(NULL, objects[I].ws[0].c_str(), cSourceWordInfo::queryOnAnyAppearance, wordForm, inflectionFlags, 0, objects[I].ws[0].c_str(), -1, added);
					}
				}
			}
		}
	}
	fclose(fgen);
	return 0;
}

// every line is a nation followed by a person associated with that nation (noun,adjective)separated by a comma
// ; He is from Afghanistan.  He is an Afghan.  He is Afghani.
// Reads demPath (demonym / place pairs) and marks them as gendered demonym nouns.
int cWord::addDemonyms(const lpchar_t* demPath)
{
	LFS
		demonymForm = cForms::createForm(u"demonym", u"de", true, u"noun", true);
	FILE* fdem = lp_wfopen(demPath, "rb"); // binary mode reads unicode
	if (!fdem)
	{
		lplog(LOG_FATAL_ERROR, u"demonym list file %s not found.", demPath);
		return -1;
	}
	lpchar_t demonym[101];
	while (lp_fgetws(demonym, 100, fdem))
	{
		if (demonym[0] == 0xFEFF) // detect BOM
			memcpy(demonym, demonym + 1, (lp_strlen(demonym + 1) + 1) * sizeof(demonym[0]));
		if (demonym[lp_strlen(demonym) - 1] == '\n') demonym[lp_strlen(demonym) - 1] = 0;
		if (demonym[lp_strlen(demonym) - 1] == '\r') demonym[lp_strlen(demonym) - 1] = 0; // in binary mode, cr/lf is not translated
		lp_towlower_str(demonym);
		if (demonym[0] == u';') continue;
		lpchar_t* nounDemonym = lp_strchr(demonym, u',');
		if (!nounDemonym) continue;
		int inflectionFlags = 0;
		lpchar_t* adjectiveDemonym = lp_strchr(++nounDemonym, u',');
		if (!adjectiveDemonym) continue;
		*adjectiveDemonym = 0;
		lpchar_t* kind = lp_strchr(++adjectiveDemonym, u',');
		bool male = false, female = false, plural = false;
		while (kind)
		{
			*kind = 0;
			kind++;
			switch (*kind)
			{
			case u'm':male = true; break;
			case u'f':female = true; break;
			case u'p':plural = true; break;
			}
			kind = lp_strchr(kind + 1, u',');
		}
		if (male) inflectionFlags = MALE_GENDER;
		if (female) inflectionFlags = FEMALE_GENDER;
		if (!female && !male) inflectionFlags |= MALE_GENDER | FEMALE_GENDER;
		inflectionFlags |= (plural) ? PLURAL : SINGULAR;
		int len = lp_strlen(nounDemonym) - 1;
		while (len > 1 && nounDemonym[len] == ' ') len--;
		if (nounDemonym[len + 1] == ' ')
			nounDemonym[len + 1] = 0;
		len = lp_strlen(adjectiveDemonym) - 1;
		while (len > 1 && adjectiveDemonym[len] == ' ') len--;
		if (adjectiveDemonym[len + 1] == ' ')
			adjectiveDemonym[len + 1] = 0;
		handleExtendedParseWords(nounDemonym);
		bool added;
		addNewOrModify(NULL, nounDemonym, cSourceWordInfo::queryOnAnyAppearance, demonymForm, inflectionFlags, 0, nounDemonym, -1, added);
		addNewOrModify(NULL, nounDemonym, cSourceWordInfo::queryOnAnyAppearance, nounForm, inflectionFlags, 0, nounDemonym, -1, added);
		if (lp_strcmp(nounDemonym, adjectiveDemonym))
		{
			addNewOrModify(NULL, adjectiveDemonym, cSourceWordInfo::queryOnAnyAppearance, demonymForm, MALE_GENDER | FEMALE_GENDER, 0, adjectiveDemonym, -1, added);
			addNewOrModify(NULL, adjectiveDemonym, cSourceWordInfo::queryOnAnyAppearance, adjectiveForm, MALE_GENDER | FEMALE_GENDER, 0, adjectiveDemonym, -1, added);
			addNewOrModify(NULL, adjectiveDemonym, cSourceWordInfo::queryOnAnyAppearance, nounForm, inflectionFlags | PLURAL & ~SINGULAR, 0, adjectiveDemonym, -1, added);
		}
		else
			addNewOrModify(NULL, adjectiveDemonym, cSourceWordInfo::queryOnAnyAppearance, adjectiveForm, 0, 0, adjectiveDemonym, -1, added);
	}
	fclose(fdem);
	return 0;
}

//     bool added;
//    words.push_back(addNewOrModify(mwords[I],cSourceWordInfo::queryOnAnyAppearance,PROPER_NOUN_FORM_NUM,0,0,mwords[I],-1,added));
// Reads a place-name list at pPath into objects and the lexicon (location subtype).
// Returns false if the file is missing.
bool cWord::addPlaces(lpwstring pPath, vector <tmWS >& objects)
{
	LFS
		FILE* fp = lp_wfopen(pPath.c_str(), "rb"); // binary mode reads unicode
	if (!fp) return false;
	int numFirstSense;
	lpchar_t place[1024];
	while (lp_fgetws(place, 1020, fp))
	{
		if (place[0] == 0xFEFF) // detect BOM
			memcpy(place, place + 1, (lp_strlen(place + 1) + 1) * sizeof(place[0]));
		int len = lp_strlen(place);
		if (len >= 1 && place[0] == u';') continue;
		if (place[len - 1] == '\n') place[--len] = 0;
		if (place[len - 1] == '\r') place[--len] = 0; // in binary mode, cr/lf is not translated
		while (place[len - 1] == ' ') place[--len] = 0;
		set <string> ignoreCategories;
		bool addWordNetSearch = false;
		lpchar_t* preferredSense = NULL, * ch, * ch2;
		bool hypo = false, print = false;
		if (print = (ch = lp_strstr(place, u" +print")) != NULL)
			*ch = 0;
		while (((ch = lp_strrchr(place, u'-')) != NULL || (ch = lp_strrchr(place, u'—')) != NULL) && ch != place)
		{
			string ic;
			ignoreCategories.insert(wTM(ch + 1, ic));
			if (print)
				printf("IGNORING: [%lS]\n", ch + 1);
			while (ch != place && *(ch - 1) == ' ') ch--;
			*ch = 0;
		}
		if ((hypo = (ch = lp_strstr(place, u" +HYPO")) != NULL) || (ch = lp_strstr(place, u" +COORDS")))
		{
			addWordNetSearch = true;
			*ch = 0;
			lpchar_t* chsense = lp_strchr(ch + 1, '('), * chsense2 = lp_strchr(ch + 1, ')');
			if (chsense && chsense2)
			{
				*chsense2 = 0;
				preferredSense = chsense + 1;
			}
		}
		lp_towlower_str(place);
		// if has () other than for WORDNET sense, remove
		ch = lp_strchr(place, '(');
		ch2 = lp_strchr(place, ')');
		if (ch && ch2)
		{
			while (ch > place && ch[-1] >= 0 && iswspace(ch[-1])) ch--;
			*ch = 0;
		}
		// if has one comma, register name before comma, and content after comma appended
		ch = lp_strrchr(place, ',');
		if (ch)
			*ch = 0;
		vector <lpwstring> words;
		if (lp_strchr(place, ' ') != NULL)
		{
			if (addWordNetSearch)
			{
				if (hypo)	addHyponyms(place, objects, preferredSense, ignoreCategories, print);
				else addCoords(place, objects, NOUN, preferredSense, numFirstSense, ignoreCategories, print);
			}
			splitMultiWord(place, words);
			objects.push_back(tmWS(words));
			continue;
		}
		handleExtendedParseWords(place);
		if (ch)
		{
			ch++;
			while (*ch == ' ') ch++;
			if (*ch)
			{
				if (addWordNetSearch)
				{
					if (hypo)	addHyponyms(place, objects, preferredSense, ignoreCategories, print);
					else addCoords(place, objects, NOUN, preferredSense, numFirstSense, ignoreCategories, print);
				}
				splitMultiWord(ch, words);
				words.push_back(place);
				objects.push_back(words);
			}
		}
		if (isDash(place[0]))
		{
			for (unsigned int I = 0; I < objects.size(); I++)
				if (objects[I].ws[objects[I].ws.size() - 1] == place + 1)
					objects.erase(objects.begin() + I);
			continue;
		}
		words.clear();
		words.push_back(place);
		if (addWordNetSearch)
		{
			if (hypo)	addHyponyms(place, objects, preferredSense, ignoreCategories, print);
			else addCoords(place, objects, NOUN, preferredSense, numFirstSense, ignoreCategories, print);
		}
		objects.push_back(words);
	}
	fclose(fp);
	return true;
}

// Loads filePath (canonical<TAB>nick…) into nicknameEquivalenceMap.
void cWord::addNickNames(const lpchar_t* filePath)
{
	LFS
		FILE* nf = lp_wfopen(filePath, "rb"); // binary mode reads unicode
	if (!nf)
	{
		lplog(LOG_FATAL_ERROR, u"ERROR:Unable to open %s.  (2)", filePath);
		return;
	}
	lpchar_t names[1024];
	int equivalenceClass = 0;
	while (lp_fgetws(names, 1024, nf))
	{
		if (names[0] == 0xFEFF) // detect BOM
			memcpy(names, names + 1, (lp_strlen(names + 1) + 1) * sizeof(names[0]));
		typedef pair <lpwstring, int> tNickPair;
		lpchar_t seps[] = u" ,", * token;
		for (token = lp_wcstok(names, seps); token != NULL; token = lp_wcstok(NULL, seps))
		{
			if (token[lp_strlen(token) - 1] == '\n') token[lp_strlen(token) - 1] = 0;
			if (token[lp_strlen(token) - 1] == '\r') token[lp_strlen(token) - 1] = 0; // in binary mode, cr/lf is not translated
			if (!token[0]) continue;
			lp_towlower_str(token);
			//lplog(u"Inserted %s with class %d.",token,equivalenceClass);
			nicknameEquivalenceMap.insert(tNickPair(token, equivalenceClass));
		}
		equivalenceClass++;
	}
	fclose(nf);
}

// Loads cached multi-word object strings/objects from disk. Returns false if missing.
bool cWord::readWordsOfMultiWordObjects(vector < vector < tmWS > >& multiWordStrings, vector < vector < vector <tIWMM> > >& multiWordObjects)
{
	LFS
		for (int pp = 0; OCSubTypeStrings[pp]; pp++)
		{
			vector < tmWS > placeWords;
			if (!addPlaces(lpwstring(u"source\\lists\\places\\") + OCSubTypeStrings[pp] + u".txt", placeWords))
				return false;
			multiWordStrings.push_back(placeWords);
			vector < vector <tIWMM> > multiWordObjectsCategory;
			multiWordObjects.push_back(multiWordObjectsCategory);
		}
	return true;
}

// multiWordStrings is read once from txt files in Source initialization.
// vector < tmWS > multiWordStrings;
// after each readWithLock (after each book adds more words)
// the multiWordStrings vector is scanned and any more objects which have all the words defined are moved to the multiWordObjects array,
//   and the entry is removed from the multiWordStrings array
// Resolves multiWordStrings into tIWMM sequences in multiWordObjects (and caches them).
void cWord::addMultiWordObjects(vector < vector < tmWS > >& multiWordStrings, vector < vector < vector <tIWMM> > >& multiWordObjects)
{
	LFS
		int multiWordObjectType = 0, numStringsScanned = 0, numStringsEntered = 0;
	for (vector < vector < tmWS > >::iterator mwsi = multiWordStrings.begin(), mwsiEnd = multiWordStrings.end(); mwsi != mwsiEnd; mwsi++, multiWordObjectType++)
		for (vector < tmWS >::iterator mwsii = mwsi->begin(); mwsii != mwsi->end(); numStringsScanned++, mwsii++)
			if (!mwsii->taken)
			{
				vector <tIWMM> multiWordObject;
				tIWMM w = WMM.end();
				for (vector <lpwstring>::iterator mwsiii = mwsii->ws.begin(), mwsiiiEnd = mwsii->ws.end(); mwsiii != mwsiiiEnd; mwsiii++)
					if ((w = query(*mwsiii)) != WMM.end())
						multiWordObject.push_back(w);
					else
						break;
				if (w != WMM.end())
				{
					multiWordObjects[multiWordObjectType].push_back(multiWordObject);
					for (vector <tIWMM>::iterator wi = multiWordObject.begin(), wiEnd = multiWordObject.end(); wi != wiEnd; wi++)
					{
						cSourceWordInfo* twi = &(*wi)->second;
						if (twi->query(adjectiveForm) >= 0 || twi->query(nounForm) >= 0 || twi->query(PROPER_NOUN_FORM_NUM) >= 0)
						{
							twi->relatedSubTypes.push_back(multiWordObjectType);
							twi->relatedSubTypeObjects.push_back(multiWordObjects[multiWordObjectType].size() - 1);
							if (twi->mainEntry != wNULL && twi->mainEntry != Words.end() && twi->mainEntry != (*wi))
							{
								twi->mainEntry->second.relatedSubTypes.push_back(multiWordObjectType);
								twi->mainEntry->second.relatedSubTypeObjects.push_back(multiWordObjects[multiWordObjectType].size() - 1);
							}
						}
					}
					//mwsii=mwsi->erase(mwsii);
					mwsii->taken = true;
					numStringsEntered++;
				}
			}
	//lplog(LOG_RESOLUTION,u"(%d/%d) multiWordObjects entered.",numStringsEntered,numStringsScanned);
}

const lpchar_t* roman_numeral[] = {
	u"i",u"ii",u"iii",u"iv",u"v",u"vi",u"vii",u"viii",u"ix",
	u"x",u"xi",u"xii",u"xiii",u"xiv",u"xv",u"xvi",u"xvii",u"xviii",u"xix",
	u"xx",u"xxi",u"xxii",u"xxiii",u"xxiv",u"xxv",u"xxvi",u"xxvii",u"xxviii",u"xxix",
	u"xxx",u"xxxi",u"xxxii",u"xxxiii",u"xxxiv",u"xxxv",u"xxxvi",u"xxxvii",u"xxxviii",u"xxxix",
	u"xl",u"xli",u"xlii",u"xliii",u"xliv",u"xlv",u"xlvi",u"xlvii",u"xlviii",u"xlix",
	u"l",u"li",u"lii",u"liii",u"liv",u"lv",u"lvi",u"lvii",u"lviii",u"lix",
	u"lx",u"lxi",u"lxii",u"lxiii",u"lxiv",u"lxv",u"lxvi",u"lxvii",u"lxviii",u"lxix",
	u"lxx",u"lxxi",u"lxxii",u"lxxiii",u"lxxiv",u"lxxv",u"lxxvi",u"lxxvii",u"lxxviii",u"lxxix",
	NULL };

// Predfines Mr/Mrs/Dr/Sir/… honorific and honorific-abbreviation forms.
void cWord::createHonorificWordCategories()
{
	Inflections honorific[] = { {u"mister",MALE_GENDER},{u"missus",FEMALE_GENDER},{u"miss",FEMALE_GENDER},
		{u"monsieur",MALE_GENDER},{u"madame",FEMALE_GENDER},{u"mademoiselle",FEMALE_GENDER},{u"sir",MALE_GENDER},{u"ma'am",FEMALE_GENDER},{u"lord",MALE_GENDER},
		{u"lady",FEMALE_GENDER},{u"m'm",FEMALE_GENDER},{NULL,0} };
	predefineWords(honorific, u"honorific", u"hon", u"noun", cSourceWordInfo::queryOnAnyAppearance, false);
	Forms[Forms.size() - 1]->blockProperNounRecognition = true; // any words having this form will never be considered as proper nouns
	// honorifics that can resolve like gendered nouns without a previous introduction of a character
	//   that has this as part of his/her name.  This is an honorific that is not a title or occupation (like deacon)
	//   these honorifics can still be used similarly to occupations ('what does the little lady like today?')
	//   but they are not occupations in that they can be resolved to any name which has a gender match and
	//   they do not require an 'introduction' of a character with this honorific in their name (unlike
	//   the honorific 'archdeacon' as in 'the archdeacon' or 'Archdeacon Cowley'.
	//   Previous Introduction Not Required
	Inflections pinr[] = { {u"mister",MALE_GENDER},{u"mr",MALE_GENDER},{u"missus",FEMALE_GENDER},{u"miss",FEMALE_GENDER},
		{u"monsieur",MALE_GENDER},{u"madame",FEMALE_GENDER},{u"mademoiselle",FEMALE_GENDER},{u"sir",MALE_GENDER},{u"ma'am",FEMALE_GENDER},//{u"lord",MALE_GENDER},
		{u"lady",FEMALE_GENDER},{u"m'm",FEMALE_GENDER},{NULL,0} };
	predefineWords(pinr, u"pinr", u"pinr", u"noun", false, false);
	Inflections honorific_abbreviation[] = { {u"mr",MALE_GENDER},{u"mrs",FEMALE_GENDER},{u"dr",MALE_GENDER | FEMALE_GENDER},
		{u"rev",MALE_GENDER | FEMALE_GENDER},{u"sen",MALE_GENDER | FEMALE_GENDER},{u"ms",FEMALE_GENDER},{u"st",MALE_GENDER | FEMALE_GENDER},{u"m",MALE_GENDER},{NULL,0} };
	predefineWords(honorific_abbreviation, u"honorific_abbreviation", u"hon_abb", u"noun", 0, false);
	Forms[Forms.size() - 1]->blockProperNounRecognition = true; // any words having this form will never be considered as proper nouns
	Inflections honorificFull[] = { {u"mother",FEMALE_GENDER},{u"lady",FEMALE_GENDER},{u"aunt",FEMALE_GENDER},{u"sister",FEMALE_GENDER},
		{u"grandmother",FEMALE_GENDER},{u"duchess",FEMALE_GENDER},{u"queen",FEMALE_GENDER},{u"princess",FEMALE_GENDER},{u"baroness",FEMALE_GENDER},
		{u"mistress",FEMALE_GENDER},{u"marchioness",FEMALE_GENDER},{u"countess",FEMALE_GENDER},{u"viscountess",FEMALE_GENDER},
		{u"father",MALE_GENDER},{u"uncle",MALE_GENDER},{u"grandfather",MALE_GENDER},
		{u"king",MALE_GENDER},{u"duke",MALE_GENDER},{u"prince",MALE_GENDER},{u"baron",MALE_GENDER},{u"marquis",MALE_GENDER},
		{u"earl",MALE_GENDER},{u"viscount",MALE_GENDER},{u"count",MALE_GENDER},{u"cousin",MALE_GENDER | FEMALE_GENDER},
		{u"inspector",MALE_GENDER | FEMALE_GENDER},{NULL,0} };
	predefineWords(honorificFull, u"honorific", u"hon", u"noun", cSourceWordInfo::queryOnAnyAppearance, false);
	Inflections militaryTitle[] = {
		// army/marines/air force officers
		{u"general",MALE_GENDER},{u"lieutenant general",MALE_GENDER},{u"major general",MALE_GENDER},{u"brigadier general",MALE_GENDER},
		{u"colonal",MALE_GENDER},{u"lieutenant colonal",MALE_GENDER},{u"major",MALE_GENDER},{u"captain",MALE_GENDER},
		{u"lieutenant",MALE_GENDER},

		// navy/coast guard officers
		{u"admiral",MALE_GENDER},{u"vice admiral",MALE_GENDER},{u"rear admiral",MALE_GENDER},
		{u"captain",MALE_GENDER},{u"commander",MALE_GENDER},{u"lieutenant commander",MALE_GENDER},{u"ensign",MALE_GENDER},

		// army/marines/navy/coast guard warrant officers
		{u"chief warrant officer",MALE_GENDER},{u"warrant officer",MALE_GENDER},

		// enlisted army
		{u"sargeant major",MALE_GENDER},{u"master sargeant",MALE_GENDER},{u"sargeant",MALE_GENDER},
		{u"staff sargeant",MALE_GENDER},{u"corporal",MALE_GENDER},{u"private",MALE_GENDER},

		// enlisted marines
		{u"gunnery sargeant",MALE_GENDER},{u"lance corporal",MALE_GENDER},

		// enlisted air force
		{u"chief master sargeant",MALE_GENDER},{u"senior master sargeant",MALE_GENDER},{u"master sargeant",MALE_GENDER},{u"technical sargeant",MALE_GENDER},
		{u"senior airman",MALE_GENDER},{u"airman",MALE_GENDER},

		// enlisted navy/coast guard
		{u"master chief petty officer",MALE_GENDER},{u"senior chief petty officer",MALE_GENDER},{u"chief petty officer",MALE_GENDER},
		{u"petty officer",MALE_GENDER},{u"seaman",MALE_GENDER},

		{NULL,0} };
	predefineWords(militaryTitle, u"honorific", u"hon", u"noun", cSourceWordInfo::queryOnAnyAppearance, false);
	Inflections religiousTitle[] = {
				{u"deacon",MALE_GENDER},{u"archdeacon",MALE_GENDER},{u"elder",MALE_GENDER},
				{u"priest",MALE_GENDER},{u"priestess",FEMALE_GENDER},{u"bishop",MALE_GENDER},{u"archbishop",MALE_GENDER},
				{u"minister",MALE_GENDER},{u"reverend",MALE_GENDER},{u"chaplain",MALE_GENDER},
				{u"vicar",MALE_GENDER},{u"canon",MALE_GENDER},{u"warden",MALE_GENDER},
				{u"father",MALE_GENDER},{u"monsignor",MALE_GENDER},{u"monseigneur",MALE_GENDER},
		{u"cardinal",MALE_GENDER},{u"pastor",MALE_GENDER},{u"rector",MALE_GENDER},
				{u"eminence",MALE_GENDER},{u"excellence",MALE_GENDER},{u"pope",MALE_GENDER},{u"emperor",MALE_GENDER},
				{NULL,0} };
	predefineWords(religiousTitle, u"honorific", u"hon", u"noun", cSourceWordInfo::queryOnAnyAppearance, false);
	Inflections governmentTitle[] = {
				{u"president",MALE_GENDER},{u"prime minister",MALE_GENDER},{u"vice president",MALE_GENDER},
				{u"speaker of the house",MALE_GENDER},{u"senator",MALE_GENDER},{u"governor",MALE_GENDER},{u"mayor",MALE_GENDER},
				{u"attorney general",MALE_GENDER},{u"chief of staff",MALE_GENDER},{u"national security advisor",MALE_GENDER},
				{u"chief",MALE_GENDER},{u"chieftess",FEMALE_GENDER},{u"ambassador",MALE_GENDER | FEMALE_GENDER},
				{NULL,0} };
	predefineWords(governmentTitle, u"honorific", u"hon", u"noun", cSourceWordInfo::queryOnAnyAppearance, false);
}

// abbreviations - no abbreviation can end with a period because
//   then the period would be hidden from the parser.  Because no period would be seen as a distinct
//   word by the parser, and if the period would also stand for the end of the sentence, as well as the
//   end of the abbreviation, no abbreviations can end with a period, but instead
//   almost all abbreviations have their own _PATTERNS which add an optional period at the end.
// Predfines SA abbreviations (etc., i.e., …) and related closed-class abbrev forms.
void cWord::createAbbreviationWordCategories()
{
	Inflections measurement_abbreviation[] = {
		{u"lbs",PLURAL},{u"mo",SINGULAR},{u"mos",PLURAL},  {u"cm",SINGULAR},{u"bt",SINGULAR},{u"cc",SINGULAR},
		{u"kg",SINGULAR},{u"km",SINGULAR},{u"kw",SINGULAR},
		{u"lb",SINGULAR},{u"ft",SINGULAR | PLURAL},{u"oz",SINGULAR | PLURAL},{u"in",SINGULAR | PLURAL},
		{u"mg",SINGULAR},{u"ml",SINGULAR},{u"mm",SINGULAR},{u"mpg",SINGULAR},{u"mph",SINGULAR},
		{u"oz",SINGULAR},{u"ppm",SINGULAR},{u"rpm",SINGULAR},{u"tbsp",SINGULAR},{u"tsp",SINGULAR},
		{NULL,0} };
	predefineWords(measurement_abbreviation, u"measurement_abbreviation", u"meas_abb", u"noun");
	// abbreviations - see abbreviations note below
	Inflections street_address_abbreviation[] = {
		{u"st",SINGULAR},{u"av",SINGULAR},{u"ave",SINGULAR},{u"dr",SINGULAR},{u"rd",SINGULAR},{u"pk",SINGULAR}, // streets (NewsBank)
		{NULL,0} };
	predefineWords(street_address_abbreviation, u"street_address_abbreviation", u"sa_abb", u"noun", 0, true);
	Inflections street_address[] = {
		{u"street",SINGULAR},{u"avenue",SINGULAR},{u"drive",SINGULAR},{u"road",SINGULAR},{u"pike",SINGULAR}, // streets (NewsBank)
		{NULL,0} };
	predefineWords(street_address, u"street_address", u"sa", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	Inflections business[] = { {u"incorporated",SINGULAR},{u"limited",SINGULAR},{u"corporation",SINGULAR},{u"company",SINGULAR},{NULL,0} };
	predefineWords(business, u"business", u"b", u"noun", 0, true);
	// abbreviations - see abbreviations note below
	Inflections business_abbreviation[] = { {u"inc",SINGULAR},{u"ltd",SINGULAR},{u"corp",SINGULAR},{u"co",SINGULAR},{NULL,0} };
	predefineWords(business_abbreviation, u"business_abbreviation", u"b_abb", u"noun", 0, true);
	// abbreviations - see abbreviations note below
	Inflections abbreviation[] = {
		{u"appt",SINGULAR},{u"art",SINGULAR},{u"isbn",SINGULAR},{u"ibid",SINGULAR},{u"ln",SINGULAR},
		{u"i.e",SINGULAR},{u"ie",SINGULAR},{u"etc",SINGULAR},{u"e.g",SINGULAR},{u"eg",SINGULAR},{u"circa",SINGULAR},{u"c",SINGULAR},
		{u"pc",SINGULAR},{u"dna",SINGULAR},{u"rna",SINGULAR},{u"pcb",SINGULAR},{u"pc",SINGULAR},{u"ph",SINGULAR},{u"pcb",SINGULAR},
		{u"no",SINGULAR},{u"ok",SINGULAR},{u"o.k",SINGULAR},{u"etc",SINGULAR},{u"p",SINGULAR},{u"a.b.c",SINGULAR},{u"v.a.d",SINGULAR}, // a.b.c. from Secret Adversary
		{NULL,0} };
	predefineWords(abbreviation, u"abbreviation", u"abb", u"noun", 0, true);
	// abbreviations - see abbreviations note below
	Inflections pagenumber[] = {
		{u"pp",SINGULAR},{u"p",SINGULAR},{u"fig",SINGULAR},
		{NULL,0} };
	predefineWords(pagenumber, u"pnum", u"pnum");
	// abbreviations - see abbreviations note below
	Inflections time_abbreviation[] = { {u"p.m",SINGULAR},{u"a.m",SINGULAR},{u"pm",SINGULAR},{u"am",SINGULAR},{NULL,0} };
	predefineWords(time_abbreviation, u"time_abbreviation", u"tabb", u"noun");
	// abbreviations - see abbreviations note below
	Inflections date_abbreviation[] = { {u"a.d",SINGULAR},{u"b.c",SINGULAR},{u"ad",SINGULAR},{u"bc",SINGULAR},{NULL,0} };
	predefineWords(date_abbreviation, u"date_abbreviation", u"dabb", u"noun");
}

// Predfines personal/possessive/reflexive/reciprocal/indefinite/relative pronouns and dets.
void cWord::createPronounCategories()
{
	Inflections pronoun[] = { {u"yonder",SINGULAR | PLURAL | NEUTER_GENDER},{u"there",SINGULAR | PLURAL | NEUTER_GENDER},
		 {u"both",PLURAL | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		 {u"either",SINGULAR | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		 {u"neither",SINGULAR | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},{u"any",SINGULAR | PLURAL | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		 {u"all",SINGULAR | PLURAL | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},{u"another",SINGULAR | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		 {u"other",SINGULAR | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},{u"each",SINGULAR | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		 {u"less",SINGULAR},{u"more",SINGULAR | PLURAL},
		 {u"least",SINGULAR | PLURAL},{u"most",SINGULAR | PLURAL},
		 {u"such",SINGULAR | PLURAL},{NULL,0} };
	predefineWords(pronoun, u"pronoun", u"pn", u"noun", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* determiner[] = { u"the",u"a",u"an",u"th'",u"another",NULL };
	predefineWords(determiner, u"determiner", u"det");
	Forms[Forms.size() - 1]->blockProperNounRecognition = true; // any words having this form will never be considered as proper nouns
	Inflections demonstrative_determiner[] = {
				{u"this",SINGULAR | NEUTER_GENDER},{u"that",SINGULAR | NEUTER_GENDER},{u"these",PLURAL | NEUTER_GENDER},{u"those",PLURAL | NEUTER_GENDER},{NULL,0} };
	predefineWords(demonstrative_determiner, u"demonstrative_determiner", u"dem_det", u"noun");
	Inflections possessive_determiner[] = {
		{u"my",SINGULAR_OWNER | FEMALE_GENDER | MALE_GENDER | FIRST_PERSON},{u"our",PLURAL_OWNER | FEMALE_GENDER | MALE_GENDER | FIRST_PERSON | SECOND_PERSON},
		{u"your",SINGULAR_OWNER | PLURAL_OWNER | FEMALE_GENDER | MALE_GENDER | SECOND_PERSON},
		{u"her",SINGULAR_OWNER | FEMALE_GENDER | THIRD_PERSON},{u"his",SINGULAR_OWNER | MALE_GENDER | THIRD_PERSON},
		{u"its",SINGULAR_OWNER | NEUTER_GENDER | THIRD_PERSON},{u"their",PLURAL_OWNER | FEMALE_GENDER | MALE_GENDER | NEUTER_GENDER | THIRD_PERSON},
		//{u"that's",SINGULAR_OWNER|NEUTER_GENDER}, // interferes with that's processing
		{u"thy",SINGULAR | PLURAL | FEMALE_GENDER | MALE_GENDER | SECOND_PERSON},{NULL,0} };
	predefineWords(possessive_determiner, u"possessive_determiner", u"pos_det", u"noun");
	const lpchar_t* interrogative_determiner[] = { u"what",u"which",u"whose",u"whatever",u"whichever",u"whosoever",u"whoever",u"whomever",u"whereever",u"whenever",NULL };
	predefineWords(interrogative_determiner, u"interrogative_determiner", u"int_det");
	// http://www.wwnorton.com/write/waor.htm
	// Some of the oil has been cleaned up.
	// Some of the problems have been solved.
	Inflections quantifier[] = { {u"all",SINGULAR | PLURAL},{u"some",SINGULAR | PLURAL},{u"much",SINGULAR | PLURAL},{u"many",PLURAL},{u"few",PLURAL},{u"each",SINGULAR},{u"every",PLURAL},{u"several",PLURAL},{u"plenty",PLURAL},{NULL,0} };
	predefineWords(quantifier, u"quantifier", u"quant", u"noun", cSourceWordInfo::queryOnAnyAppearance, false);

	Inflections personal_pronoun_nominative[] = {
		{u"one",SINGULAR | MALE_GENDER | FEMALE_GENDER | THIRD_PERSON},
		{u"i",SINGULAR | FIRST_PERSON | MALE_GENDER_ONLY_CAPITALIZED | FEMALE_GENDER_ONLY_CAPITALIZED},{u"we",PLURAL | FIRST_PERSON | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{NULL,0} };
	predefineWords(personal_pronoun_nominative, u"personal_pronoun_nominative", u"pers_pron_nom", u"noun");
	Inflections personal_pronoun_accusative[] = {
		{u"me",SINGULAR | FIRST_PERSON | MALE_GENDER | FEMALE_GENDER},{u"us",PLURAL | FIRST_PERSON | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{u"him",SINGULAR | MALE_GENDER | THIRD_PERSON},{u"her",SINGULAR | FEMALE_GENDER | THIRD_PERSON},
		{u"them",PLURAL | THIRD_PERSON | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		{u"em",PLURAL | THIRD_PERSON | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		{u"'em",PLURAL | THIRD_PERSON | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},{NULL,0} };
	predefineWords(personal_pronoun_accusative, u"personal_pronoun_accusative", u"pers_pron_acc", u"noun");
	Inflections personal_pronoun[] = {
		{u"you",SINGULAR | PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},{u"ye",SINGULAR | PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{u"ya",SINGULAR | PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{u"yer",SINGULAR | PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},{u"youse",SINGULAR | PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{u"he",SINGULAR | MALE_GENDER | THIRD_PERSON},{u"she",SINGULAR | FEMALE_GENDER | THIRD_PERSON},
		{u"it",SINGULAR | NEUTER_GENDER | THIRD_PERSON},
		{u"they",PLURAL | THIRD_PERSON | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},
		{u"thee",SINGULAR | PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},{u"thees",PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{u"thou",SINGULAR | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},{NULL,0} };
	predefineWords(personal_pronoun, u"personal_pronoun", u"pers_pron", u"noun");
	Inflections reflexive_pronoun[] = {
		{u"myself",SINGULAR | FIRST_PERSON},{u"ourselves",PLURAL | FIRST_PERSON | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{u"yourself",SINGULAR | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},{u"yourselves",PLURAL | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},{u"himself",SINGULAR | MALE_GENDER | THIRD_PERSON},
		{u"herself",SINGULAR | FEMALE_GENDER | THIRD_PERSON},{u"itself",SINGULAR | NEUTER_GENDER | THIRD_PERSON},
		{u"themselves",PLURAL | THIRD_PERSON | MALE_GENDER | FEMALE_GENDER | NEUTER_GENDER},{NULL,0} };
	predefineWords(reflexive_pronoun, u"reflexive_pronoun", u"refl_pron", u"noun");
	Inflections possessive_pronoun[] = {
		{u"mine",SINGULAR | FIRST_PERSON},{u"ours",PLURAL | FIRST_PERSON | SECOND_PERSON | MALE_GENDER | FEMALE_GENDER},
		{u"yours",SINGULAR | PLURAL | SECOND_PERSON},{u"thine",SINGULAR | PLURAL | SECOND_PERSON},
		{u"his",SINGULAR | MALE_GENDER | THIRD_PERSON},{u"hers",SINGULAR | FEMALE_GENDER | THIRD_PERSON}, // {u"it's",SINGULAR|NEUTER_GENDER},
		{u"theirs",PLURAL | THIRD_PERSON | MALE_GENDER | FEMALE_GENDER},{u"everybodies",PLURAL | MALE_GENDER | FEMALE_GENDER},
		{u"somebodies",SINGULAR | MALE_GENDER | FEMALE_GENDER},{u"nobodies",SINGULAR | MALE_GENDER | FEMALE_GENDER},{NULL,0} };
	// removed - interfering with contraction processing no one's could be 'no one is'
	//{u"everyone's",PLURAL},{u"someone's",SINGULAR},{u"no one's",SINGULAR},
	predefineWords(possessive_pronoun, u"possessive_pronoun", u"pos_pron", u"noun", cSourceWordInfo::queryOnAnyAppearance);
	const lpchar_t* reciprocal_pronoun[] = { u"each other",u"one another",NULL };
	predefineWords(reciprocal_pronoun, u"reciprocal_pronoun", u"recip_pron");
	Inflections indefinite_pronoun[] = { // agreement is singular ownership is fuzzy
			{u"everybody",SINGULAR | MALE_GENDER | FEMALE_GENDER},{u"everyone",SINGULAR | MALE_GENDER | FEMALE_GENDER},{u"every one",SINGULAR | MALE_GENDER | FEMALE_GENDER},
			{u"people",PLURAL | MALE_GENDER | FEMALE_GENDER},{u"person",SINGULAR | MALE_GENDER | FEMALE_GENDER},
			{u"others",PLURAL | MALE_GENDER | FEMALE_GENDER},{u"other",SINGULAR | MALE_GENDER | FEMALE_GENDER},
			{u"everything",SINGULAR | NEUTER_GENDER},{u"somebody",SINGULAR | MALE_GENDER | FEMALE_GENDER},
			{u"someone",SINGULAR | MALE_GENDER | FEMALE_GENDER},{u"something",SINGULAR | NEUTER_GENDER},
			{u"anybody",SINGULAR | MALE_GENDER | FEMALE_GENDER},{u"anyone",SINGULAR | MALE_GENDER | FEMALE_GENDER},
			{u"anything",SINGULAR | NEUTER_GENDER},{u"nobody",SINGULAR | MALE_GENDER | FEMALE_GENDER},
			{u"no one",SINGULAR | MALE_GENDER | FEMALE_GENDER},{u"nothing",SINGULAR | NEUTER_GENDER},
			{u"crowd",PLURAL | MALE_GENDER | FEMALE_GENDER},
			{NULL,0} };
	predefineWords(indefinite_pronoun, u"indefinite_pronoun", u"indef_pron", u"noun", 0);
	// 	who, whom, whose, which, that, what, whoever, whomever, whichever, whatever - ,u"wherever",u"whenever"
	const lpchar_t* interrogative_pronoun[] = { u"who",u"whom",u"what",u"which",u"whoever",u"whomever",u"whatever",u"whichever",NULL };
	predefineWords(interrogative_pronoun, u"interrogative_pronoun", u"inter_pron");
}

// Predfines be/have/do/does/modals and their negations, plus think-class verbs.
void cWord::createVerbAssociatedCategories()
{
	Inflections negation_verb_contraction[] = { {u"daresn't",VERB_PRESENT_FIRST_SINGULAR},{u"dasn't",VERB_PRESENT_FIRST_SINGULAR},
			{u"dunno",VERB_PRESENT_FIRST_SINGULAR},{NULL,0} };//{u"oughtn't",VERB_PRESENT_THIRD_SINGULAR},
	//{u"didn't",VERB_PAST},{u"doesn't",VERB_PAST_THIRD_SINGULAR},{u"don't",VERB_PRESENT_FIRST_SINGULAR},{u"don't",VERB_PRESENT_PLURAL},NULL};
	predefineWords(negation_verb_contraction, u"negation_verb_contraction", u"neg_vb_cont", u"verb");
	const lpchar_t* modal_auxiliary[] = { u"can",u"canst",u"could",u"couldst",u"may",u"mayst",u"might",u"must",u"should",u"shouldst",u"would",u"wouldst",u"wouldhad",u"ought",u"aught",NULL };
	predefineWords(modal_auxiliary, u"modal_auxiliary", u"mod_aux", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* future_modal_auxiliary[] = { u"shall",u"will",u"wilt",u"shalt",NULL };
	predefineWords(future_modal_auxiliary, u"future_modal_auxiliary", u"fut_mod_aux", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* negation_modal_auxiliary[] = { u"can't",u"mayn't",u"mustn't",u"shouldn't",u"couldn't",u"wouldn't",u"cannot",u"mightn't",u"oughtn't",NULL };
	//"can not",u"may not",u"must not",u"should not",u"could not",u"would not",u"might not",NULL};
	predefineWords(negation_modal_auxiliary, u"negation_modal_auxiliary", u"neg_mod_aux");
	const lpchar_t* negation_future_modal_auxiliary[] = { u"won't",u"shan't",NULL }; // "will not",u"shall not",
	predefineWords(negation_future_modal_auxiliary, u"negation_future_modal_auxiliary", u"neg_fut_mod_aux");
	InflectionsRoot verb[] = {
							{u"showed",VERB_PAST,u"show"},{u"shown",VERB_PAST_PARTICIPLE,u"show"},{u"showing",VERB_PRESENT_PARTICIPLE,u"show"},
							{u"shows",VERB_PRESENT_THIRD_SINGULAR,u"show"},{u"show",VERB_PRESENT_FIRST_SINGULAR,u"show"},
							{u"cut out",VERB_PAST,u"cut out"},{u"cut out",VERB_PAST_PARTICIPLE,u"cut out"},{u"cutting out",VERB_PRESENT_PARTICIPLE,u"cut out"},
							{u"cuts out",VERB_PRESENT_THIRD_SINGULAR,u"cut out"},{u"cut out",VERB_PRESENT_FIRST_SINGULAR,u"cut out"},
							{u"withdrew",VERB_PAST,u"withdraw"},{u"foresaw",VERB_PAST,u"foresee"},
							{u"withdrawn",VERB_PAST_PARTICIPLE,u"withdraw"},{u"foresaw",VERB_PAST_PARTICIPLE,u"foreseen"},
							{u"misunderstood",VERB_PAST | VERB_PAST_PARTICIPLE,u"misunderstand"},
							{u"dunno",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR,u"dunno"},
							{u"done",VERB_PAST_PARTICIPLE,u"do"},
							{u"lasted",VERB_PAST,u"last"},{u"lasted",VERB_PAST_PARTICIPLE,u"last"},{u"lasting",VERB_PRESENT_PARTICIPLE,u"last"}, // last is also a number - which is marked as nonquery
							{u"lasts",VERB_PRESENT_THIRD_SINGULAR,u"last"},{u"last",VERB_PRESENT_FIRST_SINGULAR,u"last"},
							// the rest are from BNC
							{u"rebuilt",VERB_PAST,u"rebuild"},{u"rebuilt",VERB_PAST_PARTICIPLE,u"rebuild"}, // rebuilt is not displayed as an m-w entry correctly
							{u"capitalise",VERB_PRESENT_FIRST_SINGULAR,u"capitalise"},{u"fax",VERB_PRESENT_FIRST_SINGULAR,u"fax"},{u"fritz",VERB_PRESENT_FIRST_SINGULAR,u"fritz"},
							{u"agonised",VERB_PAST,u"agonise"},{u"individualised",VERB_PAST,u"individualise"},
							{u"overshot",VERB_PAST,u"overshoot"},{u"oversized",VERB_PAST,u"oversize"},{u"oversold",VERB_PAST,u"oversell"},{u"recombined",VERB_PAST,u"recombine"},{u"spasmed",VERB_PAST,u"spasm"},{u"underpaid",VERB_PAST,u"underpay"},{u"unwound",VERB_PAST,u"unwind"},{u"overfed",VERB_PAST,u"overfeed"},
							{u"airbrushes",VERB_PRESENT_THIRD_SINGULAR,u"airbrush"},{u"bringeth",VERB_PRESENT_THIRD_SINGULAR,u"bring"},{u"catalyses",VERB_PRESENT_THIRD_SINGULAR,u"catalyse"},{u"changeth",VERB_PRESENT_THIRD_SINGULAR,u"change"},{u"goeth",VERB_PRESENT_THIRD_SINGULAR,u"go"},
							{u"interwoven",VERB_PAST_PARTICIPLE,u"interweave"},{u"overseen",VERB_PAST_PARTICIPLE,u"oversee"},{u"overgrown",VERB_PAST_PARTICIPLE,u"overgrow"},{u"outdone",VERB_PAST_PARTICIPLE,u"outdo"},
							{u"hypothesising",VERB_PRESENT_PARTICIPLE,u"hypothesise"},{u"labouring",VERB_PRESENT_PARTICIPLE,u"labour"},{u"outsourcing",VERB_PRESENT_PARTICIPLE,u"outsource"},{u"scrutinising",VERB_PRESENT_PARTICIPLE,u"scrutinise"},{u"sleepwalking",VERB_PRESENT_PARTICIPLE,u"sleepwalk"},{u"snorkelling",VERB_PRESENT_PARTICIPLE,u"snorkel"},{u"underlying",VERB_PRESENT_PARTICIPLE,u"underlie"},
							{u"slew",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR,u"slue"},{u"slewing",VERB_PRESENT_PARTICIPLE,u"slue"},{u"slewed",VERB_PAST,u"slue"},
							{u"slough",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR,u"slue"},{u"sloughing",VERB_PRESENT_PARTICIPLE,u"slue"},{u"sloughed",VERB_PAST,u"slue"},
							// these words must be specified as the dictionary lookup gets too many forms (>8)
							{u"air",VERB_PRESENT_FIRST_SINGULAR,u"air"},
							{u"bite",VERB_PRESENT_FIRST_SINGULAR,u"bite"},{u"bit",VERB_PAST,u"bite"},
							{u"ear",VERB_PRESENT_FIRST_SINGULAR,u"ear"},
							{u"fat",VERB_PRESENT_FIRST_SINGULAR,u"fat"},
							{u"wot",VERB_PRESENT_FIRST_SINGULAR,u"wot"},
							{NULL,0} };
	predefineWords(verb, u"verb", u"v", u"verb", cSourceWordInfo::queryOnAnyAppearance);
	InflectionsRoot is[] = {
							{u"am",VERB_PRESENT_FIRST_SINGULAR,u"am"}, // this must be first (mainEntry without queryOnAppearance)
							{u"is",VERB_PRESENT_THIRD_SINGULAR,u"am"},{u"ishas",VERB_PRESENT_THIRD_SINGULAR,u"ishas"},
							{u"ishasdoes",VERB_PRESENT_THIRD_SINGULAR,u"ishasdoes"},
							{u"are",VERB_PRESENT_PLURAL,u"am"},
							{u"art",VERB_PRESENT_PLURAL,u"am"},
							{u"was",VERB_PAST,u"am"},{u"been",VERB_PAST_PARTICIPLE,u"am"},
							{u"were",VERB_PAST_PLURAL,u"am"},
							{u"wert",VERB_PAST_PLURAL,u"am"},
							{NULL,0} };
	predefineWords(is, u"is", u"is", u"verb");
	//{u"is not",VERB_PRESENT_THIRD_SINGULAR},{u"am not",VERB_PRESENT_FIRST_SINGULAR},{u"are not",VERB_PRESENT_PLURAL},
	//{u"was not",VERB_PAST_PARTICIPLE},{u"was not",VERB_PAST},{u"were not",VERB_PAST_PLURAL},
	InflectionsRoot is_negation[] = {
							{u"isn't",VERB_PRESENT_THIRD_SINGULAR,u"am"},{u"ain't",VERB_PRESENT_FIRST_SINGULAR,u"am"},
							{u"amn't",VERB_PRESENT_FIRST_SINGULAR,u"am"},{u"an't",VERB_PRESENT_FIRST_SINGULAR,u"am"},
							{u"is't",VERB_PRESENT_THIRD_SINGULAR,u"am"},
							{u"aren't",VERB_PRESENT_PLURAL,u"am"},{u"wasn't",VERB_PAST,u"am"},{u"wasn't",VERB_PAST_PARTICIPLE,u"am"},
							{u"weren't",VERB_PAST_PLURAL,u"am"},{NULL,0} };
	predefineWords(is_negation, u"is_negation", u"is_neg", u"verb");
	predefineWord(u"be");
	predefineWord(u"being", cSourceWordInfo::queryOnAnyAppearance);
	predefineWord(u"been");
	InflectionsRoot have[] = {
							{u"have",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL,u"have"}, // this must be first (mainEntry without queryOnAppearance)
							{u"had",VERB_PAST,u"have"},{u"wouldhad",VERB_PAST,u"wouldhad"},{u"had",VERB_PAST_PARTICIPLE,u"have"},{u"wouldhad",VERB_PAST_PARTICIPLE,u"wouldhad"},
							{u"having",VERB_PRESENT_PARTICIPLE,u"have"},
							{u"has",VERB_PRESENT_THIRD_SINGULAR,u"have"},{u"hast",VERB_PRESENT_SECOND_SINGULAR,u"have"},
							{u"hath",VERB_PRESENT_THIRD_SINGULAR,u"have"},
							{u"ishas",VERB_PRESENT_THIRD_SINGULAR,u"have"},{u"ishasdoes",VERB_PRESENT_THIRD_SINGULAR,u"have"},{NULL,0} };
	predefineWords(have, u"have", u"have", u"verb");
	InflectionsRoot have_negation[] = {
							{u"haven't",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL,u"have"},// this must be first (mainEntry without queryOnAppearance)
							{u"havent",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL,u"have"},// this must be first (mainEntry without queryOnAppearance)
							{u"hadn't",VERB_PAST,u"haven't"},{u"hain't",VERB_PRESENT_FIRST_SINGULAR,u"have"},
							{u"hain't",VERB_PRESENT_THIRD_SINGULAR,u"have"},
							{u"han't",VERB_PRESENT_FIRST_SINGULAR,u"have"},{u"han't",VERB_PRESENT_THIRD_SINGULAR,u"have"},
							{u"hasn't",VERB_PRESENT_THIRD_SINGULAR,u"have"},
							{u"haven't",VERB_PRESENT_THIRD_SINGULAR,u"have"},
							//{u"have not",VERB_PRESENT_FIRST_SINGULAR},{u"had not",VERB_PRESENT_FIRST_SINGULAR},
							{NULL,0} };
	predefineWords(have_negation, u"have_negation", u"have_neg", u"verb");
	InflectionsRoot does[] = {
							{u"do",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL,u"do"},// this must be first (mainEntry without queryOnAppearance)
							{u"did",VERB_PAST,u"do"},{u"done",VERB_PAST_PARTICIPLE,u"do"},{u"doing",VERB_PRESENT_PARTICIPLE,u"do"},
							{u"does",VERB_PRESENT_THIRD_SINGULAR,u"do"},{u"dost",VERB_PRESENT_SECOND_SINGULAR,u"do"},
							{u"doth",VERB_PRESENT_THIRD_SINGULAR,u"do"},
							{u"ishasdoes",VERB_PRESENT_THIRD_SINGULAR,u"ishasdoes"},{NULL,0} };
	predefineWords(does, u"does", u"does", u"verb");
	InflectionsRoot does_negation[] = {
							{u"don't",VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_PLURAL,u"do"},// this must be first (mainEntry without queryOnAppearance)
							{u"didn't",VERB_PAST,u"do"},{u"doesn't",VERB_PRESENT_THIRD_SINGULAR,u"do"},
							{u"doesn't",VERB_PRESENT_THIRD_SINGULAR,u"do"},
							//,{u"did not",VERB_PAST},{u"does not",VERB_PRESENT_THIRD_SINGULAR},{u"do not",VERB_PRESENT_FIRST_SINGULAR},
							{NULL,0} };
	predefineWords(does_negation, u"does_negation", u"does_neg", u"verb");
	InflectionsRoot verbverb[] = {
		{u"dare",VERB_PRESENT_FIRST_SINGULAR,u"dare"},{u"dared",VERB_PAST,u"dare"},{u"daring",VERB_PRESENT_PARTICIPLE,u"dare"},{u"dares",VERB_PRESENT_THIRD_SINGULAR,u"dare"},
		{u"rather",VERB_PRESENT_FIRST_SINGULAR,u"rather"},{u"rathered",VERB_PAST,u"rather"},{u"ruther",VERB_PAST,u"rather"},{u"rathering",VERB_PRESENT_PARTICIPLE,u"rather"},{u"rathers",VERB_PRESENT_THIRD_SINGULAR,u"rather"},
		{u"make",VERB_PRESENT_FIRST_SINGULAR,u"make"},{u"made",VERB_PAST,u"make"},{u"making",VERB_PRESENT_PARTICIPLE,u"make"},{u"makes",VERB_PRESENT_THIRD_SINGULAR,u"make"},
		{u"let",VERB_PRESENT_FIRST_SINGULAR,u"let"},{u"let",VERB_PAST,u"let"},{u"letting",VERB_PRESENT_PARTICIPLE,u"let"},{u"lets",VERB_PRESENT_THIRD_SINGULAR,u"let"},
		{u"help",VERB_PRESENT_FIRST_SINGULAR,u"help"},{u"helped",VERB_PAST,u"help"},{u"helping",VERB_PRESENT_PARTICIPLE,u"help"},{u"helps",VERB_PRESENT_THIRD_SINGULAR,u"help"},
		{u"need",VERB_PRESENT_FIRST_SINGULAR,u"need"},{u"needed",VERB_PAST,u"need"},{u"needing",VERB_PRESENT_PARTICIPLE,u"need"},{u"needs",VERB_PRESENT_THIRD_SINGULAR,u"need"},
		// feeling, hearing, seeing, watching, telling, having...
		{u"feel",VERB_PRESENT_FIRST_SINGULAR,u"feel"},{u"felt",VERB_PAST,u"feel"},      {u"feeling",VERB_PRESENT_PARTICIPLE,u"feel"},  {u"feels",VERB_PRESENT_THIRD_SINGULAR,u"feel"},
		{u"hear",VERB_PRESENT_FIRST_SINGULAR,u"hear"},{u"heard",VERB_PAST,u"hear"},     {u"hearing",VERB_PRESENT_PARTICIPLE,u"hear"},  {u"hears",VERB_PRESENT_THIRD_SINGULAR,u"hear"},
		{u"see",VERB_PRESENT_FIRST_SINGULAR,u"see"},{u"saw",VERB_PAST,u"see"},          {u"seeing",VERB_PRESENT_PARTICIPLE,u"see"},    {u"sees",VERB_PRESENT_THIRD_SINGULAR,u"see"},
		{u"watch",VERB_PRESENT_FIRST_SINGULAR,u"watch"},{u"watched",VERB_PAST,u"watch"},{u"watching",VERB_PRESENT_PARTICIPLE,u"watch"},{u"watches",VERB_PRESENT_THIRD_SINGULAR,u"watch"},
		{u"tell",VERB_PRESENT_FIRST_SINGULAR,u"tell"},{u"told",VERB_PAST,u"tell"},      {u"telling",VERB_PRESENT_PARTICIPLE,u"tell"},  {u"tells",VERB_PRESENT_THIRD_SINGULAR,u"tell"},
		{NULL,0} };
	predefineWords(verbverb, u"verbverb", u"verbverb", u"verb", cSourceWordInfo::queryOnAnyAppearance);
	//InflectionsRoot verbverb2[] = {
	 // {u"have",VERB_PRESENT_FIRST_SINGULAR,u"have"},{u"had",VERB_PAST,u"have"},{u"having",VERB_PRESENT_PARTICIPLE,u"have"},{u"has",VERB_PRESENT_THIRD_SINGULAR,u"have"},
	 // {NULL,0}};
	//predefineWords(verbverb2,u"verbverb",u"verbverb",u"verb");
	// help run him out of town!
	// help define the status.
	// dare run three miles!
	// rather run two miles.
	// go steal it, if you want.
	// come show me the house.
	const lpchar_t* verbal_auxiliary[] = { u"rather",u"ruther",u"dare",u"dares",u"help",u"helps",u"go",u"come",u"need",u"needs",NULL };
	predefineWords(verbal_auxiliary, u"verbal_auxiliary", u"vb_aux", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* past_verbal_auxiliary[] = { u"dared",u"helped",u"needed",NULL };
	predefineWords(past_verbal_auxiliary, u"past_verbal_auxiliary", u"past_vb_aux", cSourceWordInfo::queryOnAnyAppearance, false);
	extern const lpchar_t* stateVerbs[];
	for (unsigned int I = 0; stateVerbs[I]; I++)
		gquery(stateVerbs[I])->second.flags |= cSourceWordInfo::stateVerb;
	extern const lpchar_t* possibleStateVerbs[];
	for (unsigned int I = 0; possibleStateVerbs[I]; I++)
		gquery(possibleStateVerbs[I])->second.flags |= cSourceWordInfo::possibleStateVerb;
	predefineVerbsFromFile(u"SYNTAX:Accepts S as Object", u"SYN:S as O", u"source\\lists\\thinksayVerbs.txt", 0);
	predefineVerbsFromFile(u"istate", u"istate", u"source\\lists\\internalStateVerbs.txt", 0);
	const lpchar_t* particles[] = { u"apart", u"about", u"across", u"along", u"around", u"aside", u"away", u"back", u"by", u"down",
												 u"forth", u"forward", u"home", u"in", u"off", u"on", u"out", u"over", u"past", u"round", u"through", u"under", u"up",NULL };
	predefineWords(particles, u"particle", u"pa");
}

// Predfines closed-class nouns (relativizers used as nouns, letters-as-nouns, etc.).
void cWord::createNounCategories()
{
	// of late, he is not there. // _PP only takes nouns as objects.
	InflectionsRoot noun[] = { {u"desk",SINGULAR,u"desk"},{u"desks",PLURAL,u"desk"},{u"eardrop",SINGULAR,u"eardrop"},{u"eardrops",PLURAL,u"eardrop"},
											{u"as",SINGULAR,u"as"},{u"art",SINGULAR,u"art"},{u"arts",PLURAL,u"art"},
											{u"to-day",SINGULAR,u"to-day"},{u"to-morrow",SINGULAR,u"to-morrow"},{u"to-night",SINGULAR,u"to-night"},
											{u"hanger-on",SINGULAR,u"hanger-on"},{u"hangers-on",PLURAL,u"hanger-on"},{u"man-of-war",SINGULAR,u"man-of-war"},
											{u"in-law",SINGULAR,u"in-law"},{u"in-laws",PLURAL,u"in-law"},
											{u"jack-of-all-trades",SINGULAR,u"jack-of-all-trades"},
											{u"late",SINGULAR,u"late"},{u"time",SINGULAR,u"time"},{u"times",PLURAL,u"time"},{u"for ever",SINGULAR,u"for ever"},
											{u"veronal",SINGULAR,u"veronal"},{u"lookout",SINGULAR,u"lookout"},{u"o'clock",SINGULAR,u"o'clock"},
		// these words must be specified as the dictionary lookup gets too many forms (>8)
		{u"air",SINGULAR,u"air"},{u"airs",PLURAL,u"air"},
		{u"bit",SINGULAR,u"bit"},{u"bits",PLURAL,u"bit"},
		{u"ear",SINGULAR,u"ear"},{u"ears",PLURAL,u"ear"},
		{u"fat",SINGULAR,u"fat"},{u"fats",PLURAL,u"fat"},
		{u"but",SINGULAR,u"but"},{u"buts",PLURAL,u"but"},
		{u"how",SINGULAR,u"how"},{u"hows",PLURAL,u"how"},
		{u"what",SINGULAR,u"what"},{u"whats",PLURAL,u"what"},
													{u"whereabouts",SINGULAR,u"whereabouts"},
		{NULL,0} };
	predefineWords(noun, u"noun", u"n", u"noun", cSourceWordInfo::queryOnAnyAppearance);
	commonProfessionForm = cForms::createForm(u"commonProfession", u"cp", true, u"noun", false);
	moneyForm = cForms::createForm(u"money", u"m", true, u"noun", false);
	webAddressForm = cForms::createForm(u"webAddress", u"wa", true, u"noun", false);
	addGenderedNouns(u"source\\lists\\commonProfessions.txt", SINGULAR | MALE_GENDER | FEMALE_GENDER, commonProfessionForm);
	addGenderedNouns(u"source\\lists\\commonProfessionsPlural.txt", PLURAL | MALE_GENDER | FEMALE_GENDER, commonProfessionForm); // only exist in plural form [staff]
	addGenderedNouns(u"source\\lists\\commonProfessionsFemale.txt", SINGULAR | FEMALE_GENDER, commonProfessionForm);
	gquery(u"woman")->second.remove(commonProfessionForm);
	gquery(u"hand")->second.remove(commonProfessionForm);
	friendForm = cForms::createForm(u"friend", u"fr", true, u"noun", false);
	addGenderedNouns(u"source\\lists\\friends.txt", SINGULAR | MALE_GENDER | FEMALE_GENDER, friendForm);

	// source: http://geography.about.com/library/weekly/aa030900a.htm
	addDemonyms(u"source\\lists\\demonyms.txt");

	/* add Proper Noun categories */
	createTimeCategories(false);
	const lpchar_t* telephoneNumbers[] = { u"388-1976",NULL };
	predefineWords(telephoneNumbers, u"telephone_number", u"telenum", 0, false);

	// this is placed here to avoid renumbering forms
	inCreateDictionaryPhase = false;
	if (nounForm == -1) nounForm = cForms::gFindForm(u"noun");
	addGenderedNouns(u"source\\lists\\singularFemaleGender.txt", SINGULAR | FEMALE_GENDER, nounForm);
	addGenderedNouns(u"source\\lists\\pluralFemaleGender.txt", PLURAL | FEMALE_GENDER, nounForm);
	addGenderedNouns(u"source\\lists\\singularMaleGender.txt", SINGULAR | MALE_GENDER, nounForm);
	addGenderedNouns(u"source\\lists\\pluralMaleGender.txt", PLURAL | MALE_GENDER, nounForm);
	const lpchar_t* nameListPaths[] = {
		u"source\\lists\\Names\\Names1990.csv",u"source\\lists\\Names\\Names1980.csv",u"source\\lists\\Names\\Names1970.csv",u"source\\lists\\Names\\Names1960.csv",u"source\\lists\\Names\\Names1950.csv",
		u"source\\lists\\Names\\Names1940.csv",u"source\\lists\\Names\\Names1930.csv",u"source\\lists\\Names\\Names1920.csv",u"source\\lists\\Names\\Names1910.csv",u"source\\lists\\Names\\Names1900.csv",NULL
	};
	for (int nlp = 0; nameListPaths[nlp]; nlp++)
		addProperNamesFile(nameListPaths[nlp]);
}

// Predfines closed-class adverbs (not, never, here, …).
void cWord::createAdverbCategories()
{
	InflectionsRoot adverb[] = {
				{u"as",ADVERB_NORMATIVE,u"as"},{u"there",ADVERB_NORMATIVE,u"there"},{u"little",ADVERB_NORMATIVE,u"little"},
				{u"just",ADVERB_NORMATIVE,u"just"},{u"rather",ADVERB_NORMATIVE,u"rather"},{u"ruther",ADVERB_NORMATIVE,u"ruther"},
				{u"yonder",ADVERB_NORMATIVE,u"yonder"},{u"for ever",ADVERB_NORMATIVE,u"for ever"},{u"however",ADVERB_NORMATIVE,u"however"},
				{u"bad",ADVERB_NORMATIVE,u"bad"},{u"worse",ADVERB_COMPARATIVE,u"bad"},{u"worst",ADVERB_SUPERLATIVE,u"bad"},
				{u"good",ADVERB_NORMATIVE,u"good"},{u"better",ADVERB_COMPARATIVE,u"good"},{u"best",ADVERB_SUPERLATIVE,u"good"},
				{u"even",ADVERB_NORMATIVE,u"even"},{u"grimly",ADVERB_NORMATIVE,u"grim"}, // not in m-w!
				{u"backwards",ADVERB_NORMATIVE,u"backward"}, // not in m-w!
				{u"afterwards",ADVERB_NORMATIVE,u"afterward"}, // not in m-w!
				// these words must be specified as the dictionary lookup gets too many forms (>8)
				{u"what",ADVERB_NORMATIVE,u"what"},
				{u"but",ADVERB_NORMATIVE,u"but"},
				{u"how",ADVERB_NORMATIVE,u"how"},
				{u"ere",ADVERB_NORMATIVE,u"ere"},
				{u"worldwide",ADVERB_NORMATIVE,u"worldwide"},
				{NULL,0} };
	predefineWords(adverb, u"adverb", u"adv", u"adverb", cSourceWordInfo::queryOnAnyAppearance);
	InflectionsRoot adverb2[] = { {u"this",ADVERB_NORMATIVE,u"this"},{u"that",ADVERB_NORMATIVE,u"that"},{u"next",ADVERB_NORMATIVE,u"next"},{u"first",ADVERB_NORMATIVE,u"first"},{NULL,0} };
	predefineWords(adverb2, u"adverb", u"adv", u"adverb");  // do not allow discovery of this or that because they are also m-w pronouns and adjectives, which are
																													// already included in the patterns as determiners
}

// Predfines closed-class adjectives / adjectival particles.
void cWord::createAdjectiveCategories()
{
	InflectionsRoot adjective[] = { {u"little",ADJECTIVE_NORMATIVE,u"little"},{u"littler",ADJECTIVE_COMPARATIVE,u"little"},
											{u"littlest",ADJECTIVE_SUPERLATIVE,u"little"},{u"just",ADJECTIVE_NORMATIVE,u"just"},
											{u"yonder",ADJECTIVE_NORMATIVE,u"yonder"},{u"thorough",ADJECTIVE_NORMATIVE,u"thorough"},
											{u"would-be",ADJECTIVE_NORMATIVE,u"would-be"},{u"towards",ADJECTIVE_NORMATIVE,u"towards"},
											{u"art",ADJECTIVE_NORMATIVE,u"art"},{u"slicked-back",ADJECTIVE_NORMATIVE,u"slicked-back"},
											{u"bad",ADJECTIVE_NORMATIVE,u"bad"},{u"worse",ADJECTIVE_COMPARATIVE,u"bad"},{u"worst",ADJECTIVE_SUPERLATIVE,u"bad"},
											{u"good",ADJECTIVE_NORMATIVE,u"good"},{u"better",ADJECTIVE_COMPARATIVE,u"good"},{u"best",ADJECTIVE_SUPERLATIVE,u"good"},
											{u"state-of-the-art",ADJECTIVE_NORMATIVE,u"state-of-the-art"},
											{u"op-ed",ADJECTIVE_NORMATIVE,u"op-ed"},
		// these words must be specified as the dictionary lookup gets too many forms (>8)
		{u"fat",ADJECTIVE_NORMATIVE,u"fat"},
		{u"what",ADJECTIVE_NORMATIVE,u"what"},
		{u"but",ADJECTIVE_NORMATIVE,u"but"},
		{u"next",ADJECTIVE_NORMATIVE,u"next"},
		{NULL,0} };
	predefineWords(adjective, u"adjective", u"adj", u"adjective", cSourceWordInfo::queryOnAnyAppearance);
}

// Predfines numeral/ordinal/quantifier/money/date/time/telenum/webAddress forms.
void cWord::createNumberCategories()
{
	const lpchar_t* numeral_cardinal[] = {
		u"zero",u"naught",u"one",u"two",u"three",u"four",u"five",u"six",u"seven",u"eight",u"nine",u"ten",u"eleven",
		u"ones",u"twos",u"threes",u"fours",u"fives",u"sixes",u"sevens",u"eights",u"nines",u"tens",u"elevens",
		u"twelve",u"dozen",u"dozens",u"thirteen",u"fourteen",u"fifteen",u"sixteen",u"seventeen",u"eighteen",u"nineteen",u"umpteen",u"gross",
		u"twenty",u"twenties",u"thirty",u"thirties",u"forty",u"forties",u"fifty",u"fifties",u"sixty",u"sixties",u"seventy",u"seventies",u"eighty",u"eighties",u"ninety",u"nineties",
		u"hundred",u"hundreds",u"thousand",u"thousands",u"million",u"millions",u"billion",u"billions",u"trillion",u"trillions",NULL };
	predefineWords(numeral_cardinal, u"numeral_cardinal", u"card");
	predefineWords(roman_numeral, u"roman_numeral", u"roman");
	const lpchar_t* numeral_ordinal[] = { u"first",u"second",u"seconds",u"third",u"thirds",u"fourth",u"fourths",u"fifth",u"fifths",
		u"sixth",u"sixths",u"seventh",u"sevenths",u"eighth",u"eighths",u"ninth",u"ninths",u"tenth",u"tenths",
		u"eleventh",u"elevenths",u"twelfth",u"twelveths",u"twelves",u"thirteenth",u"thirteenths",u"fourteenth",u"fourteenths",u"fifteenth",u"fifteenths",
		u"sixteenth",u"sixteenths",u"seventeenth",u"seventeenths",u"eighteenth",u"eighteenths",u"nineteenth",u"nineteenths",
		u"twentieth",u"twentieths",u"thirtieth",u"thirtieths",u"fortieth",u"fortieths",u"fiftieth",u"fiftieths",u"sixtieth",u"sixtieths",u"seventieth",u"seventieths",u"eightieth",u"eightieths",
		u"ninetieth",u"nintieths",u"hundredth",u"hundredths",u"thousandth",u"thousandths",u"millionth",u"millionths",u"billionth",u"billionths",u"trillionth",u"trillionths",
		u"1st",u"2nd",u"3rd",u"4th",u"5th",u"6th",u"7th",u"8th",u"9th",u"10th",
		u"11th",u"12th",u"13th",u"14th",u"15th",u"16th",u"17th",u"18th",u"19th",u"20th",
		u"21st",u"22nd",u"23rd",u"24th",u"25th",u"26th",u"27th",u"28th",u"29th",u"30th",
		u"31st",u"32nd",u"33rd",u"34th",u"35th",u"36th",u"37th",u"38th",u"39th",u"40th",
		u"first",u"last",u"next",u"umpteenth",u"nth",NULL }; // BNC
	predefineWords(numeral_ordinal, u"numeral_ordinal", u"ord");
}

// Predfines the letter form (A, B, …) used for single-letter tokens.
void cWord::createLetterCategory()
{
	const lpchar_t* letter[] = { u"a",u"b",u"c",u"d",u"e",u"f",u"g",u"h",u"i",u"j",u"k",u"l",u"m",u"n",u"o",u"p",u"q",u"r",u"s",u"t",
	u"u",u"v",u"w",u"x",u"y",u"z",NULL };
	predefineWords(letter, u"letter", u"let");
	if (letterForm < 0) letterForm = cForms::gFindForm(u"letter");
	Forms[letterForm]->inflectionsClass = u"noun";
	{
		// The letters also should not be included as names (proper nouns)
		lpchar_t anyLetter[3];
		for (lpchar_t l = 'a'; l <= 'z'; l++)
		{
			anyLetter[0] = l;
			anyLetter[1] = 0;
			tIWMM q = Words.gquery(anyLetter);
			if (q->second.query(PROPER_NOUN_FORM_NUM) >= 0)
				q->second.remove(PROPER_NOUN_FORM_NUM);
			anyLetter[1] = '.';
			anyLetter[2] = 0;
			remove(anyLetter);
			//lplog(u"removed Prop Noun from %c.",letter[0]);
		}
	}
}

// Predfines punctuation/quote/dash/bracket/period/comma forms.
void cWord::createNonLetterWordCategories()
{
	// add special section word
	bool added;
	addNewOrModify(NULL, lpwstring(u"|||"), cSourceWordInfo::topLevelSeparator, SECTION_FORM_NUM, 0, 0, u"", -1, added);
	sectionWord = WMM.begin();
	createHonorificWordCategories();
	Inflections brackets[] = { {u"{",OPEN_INFLECTION},{u"}",CLOSE_INFLECTION},
														{u"(",OPEN_INFLECTION},{u")",CLOSE_INFLECTION},
														{u"<",OPEN_INFLECTION},{u">",CLOSE_INFLECTION},
														{u"[",OPEN_INFLECTION},{u"]",CLOSE_INFLECTION},{NULL,0} };
	predefineWords(brackets, u"brackets", u"brackets", u"brackets");
	// "\"" is mapped to {u"“",OPEN_INFLECTION},{u"”",CLOSE_INFLECTION}
	// "'" is mapped to {u"‘",OPEN_INFLECTION},{u"’",CLOSE_INFLECTION}
	Inflections quotes[] = { {u"\"",0},{u"'",0},{u"`",OPEN_INFLECTION},
													{u"‘",OPEN_INFLECTION},{u"’",CLOSE_INFLECTION},{u"“",OPEN_INFLECTION},{u"”",CLOSE_INFLECTION},{NULL,0} };
	predefineWords(quotes, u"quotes", u"quotes", u"quotes");
	const lpchar_t* dash[] = { u"-",u"—",u"–",u"--",NULL };                          predefineWords(dash, u"dash", u"dash");
	const lpchar_t* punctuation[] = { u".",u"·",u"...",u";",u":",u"@",u"#",u"$",u"%",u"^",u"&",u"*",u"--",u"+",u"=",u"_",u"|",u"‚",u",",u"/",u"~",u"…",u"│",NULL };
	for (const lpchar_t** p = punctuation; *p; p++) predefineWord(*p);
}

// Predfines prepositions, to, and particles.
void cWord::createPrepositionCategories()
{
	const lpchar_t* preposition[] = { u"as",u"into",u"against",u"along",u"anigh",u"at",u"atop",u"between",u"betwixt",u"bout",
		u"by",u"circa",u"concerning",u"considering",u"contra",u"cum",u"during",u"enduring",u"fae",u"for",u"forby",
		u"fro",u"in",u"in-between",u"maugre",u"minus",u"next to",u"notwithstanding",u"of",u"de",u"on",u"outen",u"per",u"plus",u"qua",
		u"sans",u"thorough",u"through",u"till",u"times",u"to",u"touching",u"underneath",u"unlike",u"until",u"upon",
		u"via",u"without",
		u"about",u"above",u"across",u"after",u"again",u"alongside",u"around",u"aside",u"aslant",u"astraddle",u"astride",
		u"athwart",u"before",u"behind",u"below",u"beneath",u"beside",u"beyond",u"but",u"ere",u"except",u"fromward",
		u"in between",u"nearby",u"o'er",u"or",u"since",u"than",u"throughout",u"toward",u"towards",
		u"under",u"upward",u"within",
		// these words must be specified as the dictionary lookup gets too many forms (>8)
		u"ere",
		NULL };
	predefineWords(preposition, u"preposition", u"prep", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* verbalPreposition[] = { u"including",NULL };
	predefineWords(verbalPreposition, u"verbalPreposition", u"vprep", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* preposition2[] = { u"with",NULL }; // this is also defined as an adverb and as a noun which are very rare usages
	predefineWords(preposition2, u"preposition", u"prep", 0, false);
}

// Predfines interjections (oh, ah, …).
void cWord::createInterjectionCategory()
{
	const lpchar_t* interjection[] = { u"aha",u"my",u"um",u"er",u"ah",u"eh",u"gosh",u"ouch",u"huh",u"aah",u"phoo",u"gee",u"heh",u"eek",u"ahem",u"whoa",u"lordy",u"begad",u"hmm",u"ssh",u"avast",
													u"mm",u"mmm",u"hm",u"good-bye",u"h'm",u"o",u"no",u"yes",u"hush",u"aye",u"bravo",u"nay",u"ach",u"yea",u"yeah",
							u"abudah",u"ahh",u"amen",u"ar",u"aw",u"burmah",u"cor",u"daaah",u"damn",u"doggone",u"ee",u"euch",u"farewell",u"femgliah",u"gah",
							u"gees",u"glug",u"goddammit",u"goddamn",u"golly",u"hah",u"hallelujah",u"hey",u"hic",u"hooray",u"howdy",u"hoy",u"hum",u"hurrah",
							u"jeez",u"mmmm",u"na",u"nah",u"nope",u"och",u"oi",u"oo",u"phew",u"pop",u"pow",u"siah",u"slam",u"ta",u"tara",u"uh",u"waaaaah",
							u"wham",u"whee",u"whoop",u"whoosh",u"yep",u"yo",u"yuk",u"yup",u"shush",u"daah",u"dah",u"naaah",u"naw",u"nope",u"oy",
							u"allo",u"ay",u"bah",u"ciao",u"cripes",u"eureka",u"forsooth",u"goodbye",u"hello",u"hiya",u"hullo",u"hurray",u"oh",
							u"olah",u"ooh",u"oooh",u"pah",u"presto",u"shh",u"shucks",u"umm",u"ya",u"yuck",u"zounds",u"alas",u"ha",u"hurrah",u"hurray",
							u"hoo",u"sorry",u"lord", // u"how" - removed because / Empire State of Mind: How Jay-Z Went from Street Corner to Corner Office 'How' matches __INTRO_N and prevents _RELQ
							NULL };
	predefineWords(interjection, u"interjection", u"inter", cSourceWordInfo::queryOnAnyAppearance, false);
}

// Predfines that/which/who relativizers.
void cWord::createRelativizerCategory()
{
	// introduces relative clauses
		// relative pronouns - who, which, whose, whom
		// relative adverbs - when, where, why

		// Subject and object pronouns cannot be distinguished by their forms - who, which are used for subject and object pronouns.
		//   You can, however, distinguish them as follows:
		//   If the relative pronoun is followed by a verb, the relative pronoun is a subject pronoun. Subject pronouns must always be used.
		//     the apple which is lying on the table
		//   If the relative pronoun is not followed by a verb (but by a noun or pronoun), the relative pronoun is an object pronoun. Object pronouns can be dropped in defining relative clauses, which are then called Contact Clauses.
		//     the apple (which) George lay on the table
	// who, whom, whose, which, that, what, whoever, whoever, whomever, whichever, whatever
	const lpchar_t* relativizer[] = { u"who",u"which",u"whom",u"whose", // "that" removed - doesn't necessarily fit with all the other relativizers - doesn't necessarily start a question
																															// whose shoes? which shoes? NOT 'that shoe'
														u"where",u"when",u"why",u"whence",u"whereabouts",u"whereby",u"whereat",u"whensoever",u"wherefore",u"whereon",u"whereto",u"wherewith",
														u"what",u"how",u"ow",u"whither",
												 NULL }; // BNCC
	predefineWords(relativizer, u"relativizer", u"rel");
}

// Registers prefixes that may attach with a dash (self-, un-, …) for tokenize.
void cWord::defineDashedPrefixes()
{
	// a list of words that tend to be prefixes used with dashes.  Dashes are separated out into separate words, so we want these words to stand alone.
		// these are statistically significant words judged from the BNC
	InflectionsRoot dashed_prefixes[] = {
		{u"neo",ADVERB_NORMATIVE,u"neo"},{u"full",ADVERB_NORMATIVE,u"full"},{u"off",ADVERB_NORMATIVE,u"off"},{u"light",ADVERB_NORMATIVE,u"light"},{u"ultra",ADVERB_NORMATIVE,u"ultra"},
		{u"micro",ADVERB_NORMATIVE,u"micro"},{u"sun",ADVERB_NORMATIVE,u"sun"},{u"pseudo",ADVERB_NORMATIVE,u"pseudo"},{u"white",ADVERB_NORMATIVE,u"white"},{u"hard",ADVERB_NORMATIVE,u"hard"},
		{u"air",ADVERB_NORMATIVE,u"air"},{u"back",ADVERB_NORMATIVE,u"back"},{u"un",ADVERB_NORMATIVE,u"un"},{u"black",ADVERB_NORMATIVE,u"black"},{u"much",ADVERB_NORMATIVE,u"much"},
		{u"quasi",ADVERB_NORMATIVE,u"quasi"},{u"counter",ADVERB_NORMATIVE,u"counter"},{u"euro",ADVERB_NORMATIVE,u"euro"},{u"inter",ADVERB_NORMATIVE,u"inter"},
		{u"ever",ADVERB_NORMATIVE,u"ever"},{u"de",ADVERB_NORMATIVE,u"de"},{u"hand",ADVERB_NORMATIVE,u"hand"},{u"single",ADVERB_NORMATIVE,u"single"},{u"co",ADVERB_NORMATIVE,u"co"},
		{u"mini",ADVERB_NORMATIVE,u"mini"},{u"pro",ADVERB_NORMATIVE,u"pro"},{u"in",ADVERB_NORMATIVE,u"in"},{u"multi",ADVERB_NORMATIVE,u"multi"},{u"semi",ADVERB_NORMATIVE,u"semi"},
		{u"sub",ADVERB_NORMATIVE,u"sub"},{u"mid",ADVERB_NORMATIVE,u"mid"},{u"post",ADVERB_NORMATIVE,u"post"},{u"pre",ADVERB_NORMATIVE,u"pre"},{u"ex",ADVERB_NORMATIVE,u"ex"},
		{u"re",ADVERB_NORMATIVE,u"re"},{u"anti",ADVERB_NORMATIVE,u"anti"},{u"non",ADVERB_NORMATIVE,u"non"},{NULL,0} };
	predefineWords(dashed_prefixes, u"adverb", u"adv", u"adverb", cSourceWordInfo::queryOnAnyAppearance);
	predefineWords(dashed_prefixes, u"adjective", u"adj", u"adjective", cSourceWordInfo::queryOnAnyAppearance);
}

// eliminated complementizer 12/14/2006 - what has too many forms and complementizer and startquestion are somewhat redundant
// replaced complementizer with interrogative_determiner
//lpchar_t *complementizer[] = {u"whoever",u"whichever",u"whomever",u"whereever",u"whenever",u"what",NULL};
//predefineWords(complementizer,u"complementizer",u"comp");
// eliminated startquestion - replaced with relativizer 12/14/2006
//lpchar_t *startquestion[] = {u"who",u"which",u"whom",u"whose",u"where",u"when",u"why",u"what",u"how",u"wherein",u"whereof",u"whither",u"whereabouts",NULL};
//predefineWords(startquestion,u"startquestion",u"sq");
// Calls every create* category helper and predefineVerbsFromFile. Returns 0.
int cWord::createWordCategories()
{
	LFS
		changedWords = true;
	inCreateDictionaryPhase = true;
	cForms::createForm(UNDEFINED_FORM, UNDEFINED_SHORT_FORM, false, u"", false);
	cForms::createForm(SECTION_FORM, SECTION_SHORT_FORM, true, u"", false);
	cForms::createForm(COMBINATION_FORM, COMBINATION_SHORT_FORM, true, u"", false);
	cForms::createForm(PROPER_NOUN_FORM, PROPER_NOUN_SHORT_FORM, false, u"noun", false);
	cForms::createForm(NUMBER_FORM, NUMBER_SHORT_FORM, false, u"", false);
	PPN = predefineWord(u"__ppn__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	TELENUM = predefineWord(u"__telenum__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	NUM = predefineWord(u"__num__"); // number used for relations
	DATE = predefineWord(u"__date__"); // date used for relations.
	TIME = predefineWord(u"__time__"); // time used for relations.
	LOCATION = predefineWord(u"__location__"); // location used for relations.
	createNonLetterWordCategories();
	createLetterCategory();
	createAbbreviationWordCategories();
	createPronounCategories();
	createPrepositionCategories();
	createNumberCategories();
	createNounCategories();
	createAdjectiveCategories();
	createVerbAssociatedCategories();
	createAdverbCategories();
	createInterjectionCategory();
	createRelativizerCategory();
	defineDashedPrefixes();
	const lpchar_t* trademark[] = { u"benzadrine",NULL };
	predefineWords(trademark, u"trademark", u"tm");
	const lpchar_t* symbol[] = { u"he",NULL };
	predefineWords(symbol, u"symbol", u"sym");
	const lpchar_t* conjunction[] = { u"and",u"as if",u"but",u"however",u"if",u"lest",u"nor",u"or",u"than",u"unless",u"as",u"before",u"after",
		u"cos",u"even",u"given",u"immediately",u"insofar",u"whereupon", // BNCC
		u"an'", // short for 'and'
		u"o'", // short for 'of'
		u"whencesoever",u"whenever",u"whensoever",u"whereas",u"wheresoever",u"whiles",u"whilst",u"&",u"wherever", // "--" removed There may be a risk--if I've been followed.
		// these words must be specified as the dictionary lookup gets too many forms (>8)
		u"how",u"ere",
		NULL };
	predefineWords(conjunction, u"conjunction", u"conj", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* inserts[] = { u"ouch",u"right-o",u"good-bye",NULL }; // chapter 14 LGSWE
	predefineWords(inserts, u"inserts", u"ins");
	const lpchar_t* politeness_discourse_marker[] = { u"please",u"pray",NULL }; // chapter 14 LGSWE // "thank you" removed (decreased matches)
	predefineWords(politeness_discourse_marker, u"politeness_discourse_marker", u"pi", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* predeterminer[] = { u"all",u"half",u"double",u"both",u"once",u"twice",u"thrice",u"such",NULL };
	predefineWords(predeterminer, u"predeterminer", u"predeterminer", cSourceWordInfo::queryOnAnyAppearance, false);
	const lpchar_t* sNoQuery[] = { u"and",u"you",u"p.o.",u"!",u"?",NULL };
	for (const lpchar_t** s = sNoQuery; *s; s++) predefineWord(*s);
	const lpchar_t* sectionheader[] = { u"book",u"chapter",u"part",u"prologue",u"epilogue",u"volume",NULL };
	predefineWords(sectionheader, u"sectionheader", u"sh", cSourceWordInfo::queryOnAnyAppearance, false);
	// defined by Daniel Webster
	const lpchar_t* servicemark[] = { u"automat",u"bake-off",u"college board",u"laundromat",u"planned parenthood",u"soap box derby",u"comsat",u"grammy",u"rolfing",u"super bowl",NULL };
	predefineWords(servicemark, u"service mark", u"sm");
	const lpchar_t* coordinator[] = { u"and",u"or",u"nor",u"an'",u"plus",NULL };  // He and I / David and Jane, etc. ('but' removed) 2/2/2007
	predefineWords(coordinator, u"coordinator", u"coord");
	predefineHolidays();
	const lpchar_t* specials[] = { u"ex",u"there",u"to",u"only",u"but",u"also",u"thank",u"box",u"which",u"what",u"whose",u"how",u"both",u"van",u"von",NULL };
	for (const lpchar_t** s = specials; *s; s++) predefineWord(*s, cSourceWordInfo::queryOnAnyAppearance);
	const lpchar_t* specials2[] = { u"if",u"then",u"more",u"than",u"so",u"number",u"of",u"as",u"like",NULL };
	for (const lpchar_t** s = specials2; *s; s++) predefineWord(*s, cSourceWordInfo::queryOnAnyAppearance);
	// you need not define the parameters for me. 2/5/2007
	// not is registered as a (noun, adjective, adverb and preposition, but it is very rarely anything but an adverb)
	// never is an adverb
	// no is an adverb, adjective or noun (very rare)
	// p is the British abbreviation for pence
	// b is billion
	const lpchar_t* specials3[] = { u"not",u"no",u"never",u"p",u"m",u"le",u"de",u"f",u"c",u"k",u"o",u"b",u"his",u"her",u"or",u"eg",u"e.g.",u"the",u"less",u"once",u"twice",NULL };  
	for (const lpchar_t** s = specials3; *s; s++) predefineWord(*s, 0);
	// these are words which result in too many forms (>8), have been defined fully and should not be overwritten by caches.
	const lpchar_t* seal[] = { u"air",u"bit",u"but",u"how",u"what",NULL };
	for (unsigned int I = 0; seal[I]; I++)
		gquery(seal[I])->second.flags &= ~cSourceWordInfo::queryOnAnyAppearance;
	return 0;
}

// Case-insensitive lpwstring less-than for sorting unknown-word lists.
bool string_compare(const lpwstring& s1, const lpwstring& s2)
{
	LFS
		return s1 < s2 ? 1 : 0;
}

// Loads fileName into unknownWords (one token per line).
void cWord::readUnknownWords(lpchar_t* fileName, vector <lpwstring>& unknownWords)
{
	LFS
		int fd = lp_wopen(fileName, O_RDWR);
	if (fd < 0)
		lplog(LOG_FATAL_ERROR, u"FATAL:%s unknown word list does not exist.", fileName);
	unsigned int len = lp_filelength(fd);
	lpchar_t* buffer = (lpchar_t*)tmalloc(len);
	::read(fd, buffer, len);
	close(fd);
	len /= sizeof(buffer[0]);
	unknownWords.reserve(500000);
	//bool sorted=true,duplicates=false;
	unsigned int lastWord = 0;
	while (iswspace(buffer[lastWord])) lastWord++;
	//int wordBeforeThat=-1;
	for (unsigned int I = lastWord; I < len; I++)
		if (buffer[I] == 10 || buffer[I] == 13)
		{
			buffer[I] = 0;
			while (buffer[I + 1] >= 0 && iswspace(buffer[I + 1])) I++;
			//lp_towlower_str(buffer+lastWord);
			if (lp_strlen(buffer + lastWord) < 2)
			{
				//wordBeforeThat=lastWord;
				lastWord = I + 1;
				continue;
			}
			//if (unknownWords[unknownWords.size()-1]!=buffer+lastWord)
			unknownWords.push_back(buffer + lastWord);
			//else
			//  duplicates=true;
			//if (sorted && unknownWords.size()>1)
			//  sorted=unknownWords[unknownWords.size()-2]<=unknownWords[unknownWords.size()-1];
			//wordBeforeThat=lastWord;
			lastWord = I + 1;
		}
	tfree(len, buffer);
	//if (!sorted)
	//  sort(unknownWords.begin(),unknownWords.end(),string_compare);
	//if (!sorted || duplicates)
	//  writeUnknownWords(fileName,unknownWords);
}

// Writes unknownWords to fileName, sorted via string_compare.
void cWord::writeUnknownWords(lpchar_t* fileName, vector <lpwstring>& unknownWords)
{
	LFS
		if (!unknownWords.size()) return;
	lpchar_t tmp[1024];
	lp_wsprintf(tmp, u"%s", fileName);
	FILE* words = lp_wfopen(tmp, (appendToUnknownWordsMode) ? "ab" : "wb");
	if (!words)
	{
		lplog(LOG_FATAL_ERROR, u"FATAL:%s Cannot open unknown word list.", tmp);
		return;
	}
	for (unsigned int I = 0; I < unknownWords.size(); I++)
	{
		if (unknownWords[I][0] >= '0' && unknownWords[I][0] <= '9') // time, date or number not considered unknown.
			continue;
		// if next word matches except for subtraction of last letter, remove word.
		if (unknownWords[I] == unknownWords[I + 1].substr(0, unknownWords[I].length()))
			continue;
		lp_fwprintf(words, u"%s\n", unknownWords[I].c_str());
	}
	fclose(words);
	if (appendToUnknownWordsMode) unknownWords.clear();
}

cWord::cWord(void)
{
	LFS
		appendToUnknownWordsMode = false;
	cSourceWordInfo::allocated = 20000;
	cSourceWordInfo::formsArray = (unsigned int*)tmalloc(cSourceWordInfo::allocated * (sizeof(*cSourceWordInfo::formsArray)));
	cSourceWordInfo::fACount = 0;
	cSourceWordInfo::uniqueNewIndex = 1; // use to insure every word has a unique index, even though it hasn't been
	minimumLastWordWrittenClockDiff = 240;
	changedWords = inCreateDictionaryPhase = false;
	lastReadfromDBTime = -1;
	idToMap = NULL;
	idsAllocated = 0;
	disinclinationRecursionCount = 0;
}

// Sets this word's cost for 'form' to 0 (winner). Returns true if the form existed.
bool cSourceWordInfo::toLowestCost(int form)
{
	LFS
		int hf;
	if ((hf = query(form)) >= 0)
	{
		int flc = getLowestCost();
		usagePatterns[hf] = usagePatterns[flc];
		usageCosts[hf] = usageCosts[flc];
		return true;
	}
	return false;
}

// Like toLowestCost(form) but only if preferForm is also present on the word.
bool cSourceWordInfo::toLowestCostPreferForm(int form, int preferForm)
{
	LFS
		int hf, pf;
	if ((hf = query(form)) >= 0 && (pf = query(preferForm)) >= 0)
	{
		usagePatterns[hf] = usagePatterns[pf];
		usageCosts[hf] = usageCosts[pf];
		return true;
	}
	return toLowestCost(form);
}

// Sets the cost of 'form' to 'cost'. Returns true if the form existed.
bool cSourceWordInfo::setCost(int form, int cost)
{
	LFS
		int hf;
	if ((hf = query(form)) >= 0)
	{
		usagePatterns[hf] = 255 * cost / 4;
		usageCosts[hf] = cost;
		return true;
	}
	return false;
}

// Marks a hard-coded list of change-of-state verbs with the CHANGE_STATE usage flag.
void cWord::initializeChangeStateVerbs()
{
	LFS
		InflectionsRoot changeState[] = {
			{u"start",VERB_PRESENT_FIRST_SINGULAR,u"start"},			    {u"started",VERB_PAST,u"start"},					{u"starting",VERB_PRESENT_PARTICIPLE,u"start"},					{u"starts",VERB_PRESENT_THIRD_SINGULAR,u"start"},
			{u"begin",VERB_PRESENT_FIRST_SINGULAR,u"begin"},			    {u"began",VERB_PAST,u"begin"},						{u"beginning",VERB_PRESENT_PARTICIPLE,u"begin"},				{u"begins",VERB_PRESENT_THIRD_SINGULAR,u"begin"},
			{u"commence",VERB_PRESENT_FIRST_SINGULAR,u"commence"},		{u"commenced",VERB_PAST,u"commence"},			{u"commencing",VERB_PRESENT_PARTICIPLE,u"commence"},		{u"commences",VERB_PRESENT_THIRD_SINGULAR,u"commence"},
			{u"initiate",VERB_PRESENT_FIRST_SINGULAR,u"initiate"},		{u"initiated",VERB_PAST,u"initiate"},			{u"initiating",VERB_PRESENT_PARTICIPLE,u"initiate"},		{u"initiates",VERB_PRESENT_THIRD_SINGULAR,u"initiate"},
			{u"stop",VERB_PRESENT_FIRST_SINGULAR,u"stop"},						{u"stopped",VERB_PAST,u"stop"},						{u"stopping",VERB_PRESENT_PARTICIPLE,u"stop"},					{u"stops",VERB_PRESENT_THIRD_SINGULAR,u"stop"},
			{u"halt",VERB_PRESENT_FIRST_SINGULAR,u"halt"},						{u"halted",VERB_PAST,u"halt"},						{u"halting",VERB_PRESENT_PARTICIPLE,u"halt"},						{u"halts",VERB_PRESENT_THIRD_SINGULAR,u"halt"},
			{u"conclude",VERB_PRESENT_FIRST_SINGULAR,u"conclude"},		{u"concluded",VERB_PAST,u"conclude"},			{u"concluding",VERB_PRESENT_PARTICIPLE,u"conclude"},		{u"concludes",VERB_PRESENT_THIRD_SINGULAR,u"conclude"},
			{u"discontinue",VERB_PRESENT_FIRST_SINGULAR,u"discontinue"},{u"discontinued",VERB_PAST,u"discontinue"},{u"discontinuing",VERB_PRESENT_PARTICIPLE,u"discontinue"},  {u"discontinues",VERB_PRESENT_THIRD_SINGULAR,u"discontinue"},
			{u"close",VERB_PRESENT_FIRST_SINGULAR,u"close"},					{u"closed",VERB_PAST,u"close"},						{u"closing",VERB_PRESENT_PARTICIPLE,u"close"},					{u"closes",VERB_PRESENT_THIRD_SINGULAR,u"close"},
			{u"cease",VERB_PRESENT_FIRST_SINGULAR,u"cease"},					{u"ceased",VERB_PAST,u"cease"},						{u"ceasing",VERB_PRESENT_PARTICIPLE,u"cease"},					{u"ceases",VERB_PRESENT_THIRD_SINGULAR,u"cease"},
			{u"quit",VERB_PRESENT_FIRST_SINGULAR,u"quit"},						{u"quit",VERB_PAST,u"quit"},							{u"quitting",VERB_PRESENT_PARTICIPLE,u"quit"},					{u"quits",VERB_PRESENT_THIRD_SINGULAR,u"quit"},
			{u"interrupt",VERB_PRESENT_FIRST_SINGULAR,u"interrupt"},	{u"interrupted",VERB_PAST,u"interrupt"},	{u"interrupting",VERB_PRESENT_PARTICIPLE,u"interrupt"},	{u"interrupts",VERB_PRESENT_THIRD_SINGULAR,u"interrupt"},
			{u"suspend",VERB_PRESENT_FIRST_SINGULAR,u"suspend"},			{u"suspended",VERB_PAST,u"suspend"},			{u"suspending",VERB_PRESENT_PARTICIPLE,u"suspend"},			{u"suspends",VERB_PRESENT_THIRD_SINGULAR,u"suspend"},
			{u"pause",VERB_PRESENT_FIRST_SINGULAR,u"pause"},					{u"paused",VERB_PAST,u"pause"},						{u"pausing",VERB_PRESENT_PARTICIPLE,u"pause"},					{u"pauses",VERB_PRESENT_THIRD_SINGULAR,u"pause"},
			{u"finish",VERB_PRESENT_FIRST_SINGULAR,u"finish"},				{u"finished",VERB_PAST,u"finish"},				{u"finishing",VERB_PRESENT_PARTICIPLE,u"finish"},				{u"finishes",VERB_PRESENT_THIRD_SINGULAR,u"finish"},
			{u"end",VERB_PRESENT_FIRST_SINGULAR,u"end"},							{u"ended",VERB_PAST,u"end"},							{u"ending",VERB_PRESENT_PARTICIPLE,u"end"},							{u"ends",VERB_PRESENT_THIRD_SINGULAR,u"end"},
			{u"terminate",VERB_PRESENT_FIRST_SINGULAR,u"terminate"},	{u"terminated",VERB_PAST,u"terminate"},		{u"terminating",VERB_PRESENT_PARTICIPLE,u"terminate"},	{u"terminates",VERB_PRESENT_THIRD_SINGULAR,u"terminate"},
			{u"complete",VERB_PRESENT_FIRST_SINGULAR,u"complete"},		{u"completed",VERB_PAST,u"complete"},			{u"completing",VERB_PRESENT_PARTICIPLE,u"complete"},		{u"completes",VERB_PRESENT_THIRD_SINGULAR,u"complete"},
			{u"conclude",VERB_PRESENT_FIRST_SINGULAR,u"conclude"},		{u"concluded",VERB_PAST,u"conclude"},			{u"concluding",VERB_PRESENT_PARTICIPLE,u"conclude"},		{u"concludes",VERB_PRESENT_THIRD_SINGULAR,u"conclude"},
			{u"resume",VERB_PRESENT_FIRST_SINGULAR,u"resume"},				{u"resumed",VERB_PAST,u"resume"},					{u"resuming",VERB_PRESENT_PARTICIPLE,u"resume"},				{u"resumes",VERB_PRESENT_THIRD_SINGULAR,u"resume"},
			{u"continue",VERB_PRESENT_FIRST_SINGULAR,u"continue"},		{u"continued",VERB_PAST,u"continue"},			{u"continuing",VERB_PRESENT_PARTICIPLE,u"continue"},		{u"continues",VERB_PRESENT_THIRD_SINGULAR,u"continue"},
			{u"recommence",VERB_PRESENT_FIRST_SINGULAR,u"recommence"},{u"recommenced",VERB_PAST,u"recommence"},	{u"recommencing",VERB_PRESENT_PARTICIPLE,u"recommence"},{u"recommences",VERB_PRESENT_THIRD_SINGULAR,u"recommence"},
			{u"renew",VERB_PRESENT_FIRST_SINGULAR,u"renew"},					{u"renewed",VERB_PAST,u"renew"},					{u"renewing",VERB_PRESENT_PARTICIPLE,u"renew"},					{u"renews",VERB_PRESENT_THIRD_SINGULAR,u"renew"},
			{u"restart",VERB_PRESENT_FIRST_SINGULAR,u"restart"},			{u"restarted",VERB_PAST,u"restart"},			{u"restarting",VERB_PRESENT_PARTICIPLE,u"restart"},			{u"restarts",VERB_PRESENT_THIRD_SINGULAR,u"restart"},
			{NULL,0} };
	predefineWords(changeState, u"changeState", u"changeState", u"verb", 0);
}

// Resolves global verbForm / beForm / haveForm / … indexes after forms have been created.
void cWord::findPredefinedVerb()
{
	if (doesForm < 0) doesForm = cForms::gFindForm(u"does");
	if (doesNegationForm < 0) doesNegationForm = cForms::gFindForm(u"does_negation");
	if (verbForm < 0) verbForm = cForms::gFindForm(u"verb");
	if (thinkForm < 0) thinkForm = cForms::gFindForm(u"SYNTAX:Accepts S as Object");
	if (verbverbForm < 0) verbverbForm = cForms::gFindForm(u"verbverb");
	if (beForm < 0) beForm = cForms::gFindForm(u"be");
	if (haveForm < 0) haveForm = cForms::gFindForm(u"have");
	if (haveNegationForm < 0) haveNegationForm = cForms::gFindForm(u"have_negation");
	if (doForm < 0) doForm = cForms::gFindForm(u"does");
	if (doNegationForm < 0) doNegationForm = cForms::gFindForm(u"does_negation");
	if (futureModalAuxiliaryForm < 0) futureModalAuxiliaryForm = cForms::gFindForm(u"future_modal_auxiliary");
	if (negationModalAuxiliaryForm < 0) negationModalAuxiliaryForm = cForms::gFindForm(u"negation_modal_auxiliary");
	if (negationFutureModalAuxiliaryForm < 0) negationFutureModalAuxiliaryForm = cForms::gFindForm(u"negation_future_modal_auxiliary");
	if (modalAuxiliaryForm < 0)  modalAuxiliaryForm = cForms::gFindForm(u"modal_auxiliary");
	if (isForm < 0) isForm = cForms::gFindForm(u"is");
	if (isNegationForm < 0) isNegationForm = cForms::gFindForm(u"is_negation");
}

// Resolves global pronoun/determiner form indexes (nomForm, possessiveDeterminerForm, …).
void cWord::findPredefinedPronoun()
{
	if (personalPronounAccusativeForm < 0) personalPronounAccusativeForm = cForms::gFindForm(u"personal_pronoun_accusative");
	if (indefinitePronounForm < 0) indefinitePronounForm = cForms::gFindForm(u"indefinite_pronoun");
	if (reciprocalPronounForm < 0) reciprocalPronounForm = cForms::gFindForm(u"reciprocal_pronoun");
	if (pronounForm < 0) pronounForm = cForms::gFindForm(u"pronoun");
	if (nomForm < 0) nomForm = cForms::gFindForm(u"personal_pronoun_nominative");
	if (possessivePronounForm < 0) possessivePronounForm = cForms::gFindForm(u"possessive_pronoun");  // mine, ours etc.
	if (reflexivePronounForm < 0) reflexivePronounForm = cForms::gFindForm(u"reflexive_pronoun"); // myself, himself
	if (personalPronounForm < 0) personalPronounForm = cForms::gFindForm(u"personal_pronoun");
}

// Resolves determinerForm / demonstrativeDeterminerForm / quantifierForm indexes.
void cWord::findPredefinedDeterminer()
{
	if (demonstrativeDeterminerForm < 0) demonstrativeDeterminerForm = cForms::gFindForm(u"demonstrative_determiner");
	if (determinerForm < 0) determinerForm = cForms::gFindForm(u"determiner");
	if (possessiveDeterminerForm < 0) possessiveDeterminerForm = cForms::gFindForm(u"possessive_determiner");
	if (interrogativeDeterminerForm < 0) interrogativeDeterminerForm = cForms::gFindForm(u"interrogative_determiner");
	if (predeterminerForm < 0) predeterminerForm = cForms::gFindForm(u"predeterminer");
}

// Calls findPredefinedVerb/Pronoun/Determiner and caches noun/adjective/adverb/prep forms.
void cWord::findPredefinedForms()
{
	findPredefinedVerb();
	findPredefinedPronoun();
	findPredefinedDeterminer();
	if (adverbForm < 0) adverbForm = cForms::gFindForm(u"adverb");
	if (adjectiveForm < 0) adjectiveForm = cForms::gFindForm(u"adjective");
	if (commaForm < 0) commaForm = cForms::gFindForm(u",");
	if (conjunctionForm < 0) conjunctionForm = cForms::gFindForm(u"conjunction");
	if (honorificForm < 0) honorificForm = cForms::gFindForm(u"honorific");
	if (nounForm == -1) nounForm = cForms::gFindForm(u"noun");
	if (numeralCardinalForm < 0) numeralCardinalForm = cForms::gFindForm(u"numeral_cardinal");
	if (numeralOrdinalForm < 0) numeralOrdinalForm = cForms::gFindForm(u"numeral_ordinal");
	if (romanNumeralForm < 0) romanNumeralForm = cForms::gFindForm(u"roman_numeral");
	if (periodForm < 0) periodForm = cForms::gFindForm(u".");
	if (quantifierForm < 0) quantifierForm = cForms::gFindForm(u"quantifier");
	if (quoteForm < 0) quoteForm = cForms::gFindForm(u"quotes");
	if (dashForm < 0) dashForm = cForms::gFindForm(u"dash");
	if (dateForm < 0) dateForm = cForms::gFindForm(u"date");
	if (timeForm < 0) timeForm = cForms::gFindForm(u"time");
	if (telephoneNumberForm < 0) telephoneNumberForm = cForms::gFindForm(u"telephone_number");
	if (coordinatorForm < 0) coordinatorForm = cForms::gFindForm(u"coordinator");
	if (abbreviationForm < 0) abbreviationForm = cForms::gFindForm(u"abbreviation");
	if (sa_abbForm < 0) sa_abbForm = cForms::gFindForm(u"street_address_abbreviation");
	numberForm = NUMBER_FORM_NUM;
	if (interjectionForm < 0) interjectionForm = cForms::gFindForm(u"interjection");
	if (letterForm < 0) letterForm = cForms::gFindForm(u"letter");
	if (prepositionForm < 0) prepositionForm = cForms::gFindForm(u"preposition");
	if (telenumForm < 0) telenumForm = cForms::gFindForm(u"telephone_number");
	if (bracketForm < 0) bracketForm = cForms::gFindForm(u"brackets");
	if (toForm < 0) toForm = cForms::gFindForm(u"to");
	if (relativizerForm < 0)  relativizerForm = cForms::gFindForm(u"relativizer");
	if (honorificAbbreviationForm < 0) honorificAbbreviationForm = cForms::gFindForm(u"honorific_abbreviation");
	if (businessForm < 0)  businessForm = cForms::gFindForm(u"business");
	if (demonymForm < 0)  demonymForm = cForms::gFindForm(u"demonym");
	if (commonProfessionForm < 0) commonProfessionForm = cForms::gFindForm(u"commonProfession");
	if (friendForm < 0) friendForm = cForms::gFindForm(u"friend");
	if (moneyForm < 0) moneyForm = cForms::gFindForm(u"money");
	if (webAddressForm < 0) webAddressForm = cForms::gFindForm(u"webAddress");
	if (internalStateForm < 0) internalStateForm = cForms::gFindForm(u"internalState");
	if (particleForm < 0) particleForm = cForms::gFindForm(u"particle");
	if (relativeForm < 0) relativeForm = cForms::gFindForm(u"relative");
	if (monthForm < 0) monthForm = cForms::gFindForm(u"month");
	//if (internalStateForm<0) internalStateForm=cForms::gFindForm(u"internalState");
	//if (relativeForm<0)  relativeForm=cForms::gFindForm(u"relative");
}

// Hand-tuned cost/usage tweaks for high-frequency ambiguous words after the cache load.
void cWord::adjustUsages()
{
	// gquery(u"--")->second.flags &= ~cSourceWordInfo::ignoreFlag; // ignore all dashes EXCEPT the double dash!  Stanford check 33023/5697357 0.580% BEFORE.  
	gquery(u"tell")->second.usagePatterns[cSourceWordInfo::VERB_HAS_2_OBJECTS] = 255;
	gquery(u"tell")->second.usageCosts[cSourceWordInfo::VERB_HAS_2_OBJECTS] = 0;
	gquery(u"descend")->second.usagePatterns[cSourceWordInfo::VERB_HAS_1_OBJECTS] = 255;
	gquery(u"descend")->second.usageCosts[cSourceWordInfo::VERB_HAS_1_OBJECTS] = 0;
	gquery(u"wish")->second.usagePatterns[cSourceWordInfo::VERB_HAS_2_OBJECTS] = 255; // I wish you some figgy pudding
	gquery(u"wish")->second.usageCosts[cSourceWordInfo::VERB_HAS_2_OBJECTS] = 0;
	//gquery(u"get")->second.usagePatterns[cSourceWordInfo::VERB_HAS_2_OBJECTS] = 127; // to get her a taxi - already set in DB
	//gquery(u"get")->second.usageCosts[cSourceWordInfo::VERB_HAS_2_OBJECTS] = 2;
	gquery(u"speed")->second.usagePatterns[cSourceWordInfo::VERB_HAS_1_OBJECTS] = 0;
	gquery(u"speed")->second.usageCosts[cSourceWordInfo::VERB_HAS_1_OBJECTS] = 4;
	gquery(u"other")->second.toLowestCost(indefinitePronounForm);
	gquery(u"last")->second.toLowestCost(adjectiveForm);
	gquery(u"last")->second.toLowestCost(adverbForm);
	gquery(u"few")->second.toLowestCost(quantifierForm);
	gquery(u"spring")->second.toLowestCost(verbForm);
	gquery(u"whenever")->second.toLowestCost(relativizerForm);
	gquery(u"such")->second.toLowestCost(cForms::gFindForm(u"predeterminer"));
	gquery(u"dove")->second.setCost(verbForm, 3);
	gquery(u"nurse")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0;
	gquery(u"nurse")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
	gquery(u"turn")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0;
	gquery(u"turn")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
	gquery(u"rap")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0;
	gquery(u"rap")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
	gquery(u"mind")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0;
	gquery(u"mind")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
	gquery(u"walk")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0;
	gquery(u"walk")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
	gquery(u"repair")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0; // He wanted to repair to the Gallery.
	gquery(u"repair")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
	gquery(u"step")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0;
	gquery(u"step")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
	gquery(u"side")->second.usagePatterns[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 0;
	gquery(u"side")->second.usageCosts[cSourceWordInfo::SINGULAR_NOUN_HAS_NO_DETERMINER] = 4;
}

// Sets default per-form costs (proper noun expensive, closed class cheap, etc.).
void cWord::initializeCosts()
{
	// set usageCosts for 'think' verbs to be equal to the cost of verb usage.
// see similar routine for regularizing usage cost of nouns - usageCostToNoun
// **changes 
// flags, inflectionFlags, usagePatterns, usageCosts, numProperNounUsageAsAdjective
	for (tIWMM iWord = Words.WMM.begin(), iWordEnd = Words.WMM.end(); iWord != iWordEnd; iWord++)
	{
		iWord->second.setIgnore();
		iWord->second.setTopLevel();
		iWord->second.flags &= ~(cSourceWordInfo::physicalObjectByWN | cSourceWordInfo::notPhysicalObjectByWN | cSourceWordInfo::uncertainPhysicalObjectByWN);
		iWord->second.costEquivalentSubClass(commonProfessionForm, nounForm);
		iWord->second.costEquivalentSubClass(thinkForm, verbForm);
		iWord->second.costEquivalentSubClass(verbverbForm, verbForm);
		// reset these counts, which are more relevant on a per-source basis
		iWord->second.usagePatterns[cSourceWordInfo::PROPER_NOUN_USAGE_PATTERN] = iWord->second.usagePatterns[cSourceWordInfo::LOWER_CASE_USAGE_PATTERN] = 0;
		// make honorifics not costly (honorifics are not reflected out of BNC properly so they are underweighted)
		iWord->second.toLowestCostPreferForm(honorificForm, nounForm);
		if (!iWord->second.toLowestCost(demonstrativeDeterminerForm) && iWord->second.query(relativizerForm) < 0 &&
			iWord->first != u"there" && iWord->first != u"another" && iWord->first != u"so" && iWord->first != u"each" && iWord->first != u"every" && iWord->first != u"either" && iWord->first != u"neither" && iWord->first != u"other")
			iWord->second.toLowestCost(pronounForm);
	}
}

// Top-level lexicon bootstrap: nicknames, findPredefinedForms, sentinel words (__ppn__,
// TABLE, …), form flags, Levin + VerbNet, initializeCosts, adjustUsages, time categories.
void cWord::initialize()
{
	LFS
	lp_wprintf(u"Initializing dictionary...                  \r");
	// read into nicknameEquivalenceMap
	addNickNames(u"source\\lists\\maleNicknames.txt");
	addNickNames(u"source\\lists\\femaleNicknames.txt");

	// SET internal form variables
	// avoid looking these common forms up...
	findPredefinedForms();

	// set internal word variables
	PPN = gquery(u"__ppn__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	TELENUM = gquery(u"__telenum__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	NUM = gquery(u"__num__"); // number used for relations.
	DATE = gquery(u"__date__");
	TIME = gquery(u"__time__");
	predefineWord(u"__location__"); // location used for relations.
	LOCATION = gquery(u"__location__");
	TABLE = predefineWord(u"lpTABLE"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	END_COLUMN = predefineWord(u"lpENDCOLUMN"); // used to end each column string which is extracted from <table> and table-like constructions in HTML
	END_COLUMN_HEADERS = predefineWord(u"lpENDCOLUMNHEADERS"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	TOC_HEADER = predefineWord(u"lpTOC"); // notes a table which is the table of contents for the page
	MISSING_COLUMN = predefineWord(u"lpMISSINGCOLUMN"); // used to start the table section which is extracted from <table> and table-like constructions in HTML

	//** SET Forms
	vector <int> commonForms =
	{ reflexivePronounForm,nomForm,personalPronounAccusativeForm,conjunctionForm,demonstrativeDeterminerForm,possessiveDeterminerForm,interrogativeDeterminerForm,
		indefinitePronounForm,reciprocalPronounForm,pronounForm,thinkForm,relativeForm,
		determinerForm,doesForm,doesNegationForm,possessivePronounForm,quantifierForm,coordinatorForm,
		beForm,haveForm,haveNegationForm,doForm,doNegationForm,interjectionForm,personalPronounForm,
		isForm,isNegationForm,prepositionForm,toForm,relativizerForm,particleForm,
		doForm,doNegationForm,modalAuxiliaryForm,futureModalAuxiliaryForm,negationModalAuxiliaryForm,negationFutureModalAuxiliaryForm };
	for (int cf : commonForms)
		Forms[cf]->isCommonForm = true;

	// numeralCardinalForm, numeralOrdinalForm, romanNumeralForm, quantifierform, dateForm,timeForm, telephoneNumberForm, moneyForm, and webAddressForm
	// are not closed, but they can be positively identified so they do not have to be written in the cache file
	vector<int> nonCachedForms = {
				beForm,bracketForm,commaForm,conjunctionForm,coordinatorForm,dashForm,dateForm,demonstrativeDeterminerForm,determinerForm,
				doForm,doNegationForm,doesForm,doesNegationForm,futureModalAuxiliaryForm,haveForm,haveNegationForm,honorificAbbreviationForm,
				honorificForm,indefinitePronounForm,interrogativeDeterminerForm,isForm,isNegationForm,letterForm,modalAuxiliaryForm,
				moneyForm,monthForm,negationFutureModalAuxiliaryForm,negationModalAuxiliaryForm,nomForm,numberForm,numeralCardinalForm,
				numeralOrdinalForm,particleForm,periodForm,personalPronounAccusativeForm,personalPronounForm,possessiveDeterminerForm,
				possessivePronounForm,pronounForm,quantifierForm,quoteForm,reciprocalPronounForm,reflexivePronounForm,relativeForm,
				relativizerForm,romanNumeralForm,sa_abbForm,telenumForm,telephoneNumberForm,timeForm,toForm,webAddressForm };
	for (int form : nonCachedForms)
		Forms[form]->isNonCachedForm = true;

	// "quotes" removed 6/24 because it was causing NOUN to match double nouns across ".
	// example:_NOUN [my dear child , " interrupted tuppence]
	vector <lpwstring> ignoreForms =
	{ u"dash",u"/",u"^",u"|",u"│" }; // bracketing with "/" sometimes used as emphasis (took out "--", 8/30/2005) added "|" 4/11/2006 for BNC (took out u"interjection" 9/28/2019)
	for (lpwstring ifs : ignoreForms)
	{
		int f = cForms::gFindForm(ifs);
		Forms[f]->isIgnore = true;
	}

	vector <int> verbForms = { verbForm,verbverbForm,isForm,isNegationForm,haveForm,haveNegationForm,doesForm,doesNegationForm,
		modalAuxiliaryForm,negationModalAuxiliaryForm,futureModalAuxiliaryForm,negationFutureModalAuxiliaryForm,beForm,thinkForm };
	for (int vf : verbForms)
		Forms[vf]->isVerbForm = true;

	vector <int> nounForms = { nounForm,indefinitePronounForm,personalPronounForm,personalPronounAccusativeForm,nomForm,PROPER_NOUN_FORM_NUM,reflexivePronounForm,letterForm };
	for (int nf : nounForms)
		Forms[nf]->isNounForm = true;

	// initialize levin arrays
	readVerbClasses();
	readVerbClassNames();
	// initialize vbNet arrays
	readVBNet();

	initializeCosts();

	adjustUsages();

	// initialize time flags
	createTimeCategories(true);  // normalize cost only
	extendedParseHolidays();
	printf("Finished initializing dictionary...\r");
}