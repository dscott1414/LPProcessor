/*
	getThesaurus.cpp - thesaurus.com markup parsing and DB synonym lookup

	Overview:
		Thesaurus support: scrapes the old thesaurus.com markup into sDefinition rows
		(mainEntry, wordType, primary/accumulated synonyms and antonyms) and reads
		synonyms back out of the DB.

		NOTE: the batch ingest driver and its one-shot diagnostics (getThesaurus,
		testThesaurus, resolveTables, splitPrimarySynonyms, stripTags,
		processIntoTokens, distributeRest, removeDashes) were removed as uncalled
		code; nothing in the tree invoked them. What is left is the part the parser
		actually uses at runtime.

	Pipeline position:
		Runtime synonym lookup. The table this reads is no longer populated from
		here -- whatever builds it now lives outside this file.

	Key entry points:
		- getSynonymsFromDB - read synonyms for a word out of lp.thesaurus
		- scrapeOldThesaurus - parse cached thesaurus.com markup into synonyms

	Dependencies:
		MySQL 'lp.thesaurus'; LMAINDIR\\old thesaurus entries; wn.h POS constants.

	Notes / gotchas:
		getSynonymsFromDB escapes the word (escaped(), source.h) before concatenating
		into SQL.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "time.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "profile.h"
#pragma warning (disable: 4503)
#pragma warning (disable: 4996)
#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
using namespace std;
#include "mysql.h"
#include "wn.h"
bool myquery(MYSQL* mysql, const lpchar_t* q, MYSQL_RES*& result, bool allowFailure = false);
void scrapeNewThesaurus(lpwstring word, int synonymType, vector <sDefinition>& d);

// SELECT primary/accumulated synonyms for mainEntry=word (wordType bitmask). word is escaped
// (escaped(), source.h) before being concatenated into SQL. Also builds alternatives from the
// last token of primarySynonyms plus the first accumulated synonym. Does not LOCK.
void getSynonymsFromDB(MYSQL mysql, lpwstring word, vector <unordered_set <lpwstring> >& synonyms, vector <lpwstring >& alternatives, int synonymType)
{
	lpwstring query = u"select primarySynonyms, accumulatedSynonyms from thesaurus where mainEntry = '";
	query += escaped(word) + u"'";
	// thesaurus mappings
	// "adj"=1, "adv"=2, "prep"=4, "pron"=8, "conj"=16, "det"=32, "interj"=64, "n"=128, "v"=256, NULL };
	if (synonymType == 1) // NOUN
		query += u" and (wordType&128)=128";
	else if (synonymType == 2) // VERB
		query += u" and (wordType&256)=256";
	else if (synonymType == 3) // ADJ
		query += u" and (wordType&1)=1";
	else if (synonymType == 4) // ADV
		query += u" and (wordType&2)=2";
	MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	if (myquery(&mysql, (lpchar_t*)query.c_str(), result))
	{
		while ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			unordered_set <lpwstring> ss;
			string primarySynonyms = (sqlrow[0] == NULL) ? "" : sqlrow[0];
			string properties = (sqlrow[1] == NULL) ? "" : sqlrow[1];
			lpwstring firstWordSynonym;
			int lastBegin = 0;
			for (unsigned int s = 0; s < properties.size(); s++)
				if (properties[s] == ';')
				{
					lpwstring wtmp;
					mTW(properties.substr(lastBegin, s - lastBegin), wtmp);
					if (wtmp.length() > 0 && wtmp[wtmp.length() - 1] == '*')
						wtmp.erase(wtmp.length() - 1);
					ss.insert(wtmp);
					if (lastBegin == 0)
						firstWordSynonym = wtmp;
					lastBegin = s + 1;
				}
			synonyms.push_back(ss);
			lastBegin = 0;
			//			-- if primarySynonyms contain 'or' as in 'prize or reward;' then split in multiple parts base on 'or' and record both parts
			int whereLastSpace = primarySynonyms.find_last_of(' ');
			if (whereLastSpace != string::npos)
			{
				primarySynonyms.erase(0, whereLastSpace + 1);
				primarySynonyms.erase(primarySynonyms.length() - 1);
			}
			else
				primarySynonyms.clear();
			lpwstring pslast;
			mTW(primarySynonyms, pslast);
			lpwstring alternative = pslast + u" " + firstWordSynonym;
			alternatives.push_back(alternative);
		}
		mysql_free_result(result);
	}
}

// Splits str on each occurrence of splitch (including a trailing empty piece after the last).
void split(string str, vector <string>& words, const char* splitch)
{
	int ch = -1;
	vector <string> syns;
	do
	{
		int nextch = str.find(splitch, ch + 1);
		words.push_back(str.substr(ch + 1, nextch - ch - 1));
		ch = nextch;
	} while (ch != string::npos);
}

string vectorString(vector <string>& vstr, string& tmpstr, string separator);



// Maps a small set of Latin-1 accented letters onto ASCII; logs each unseen codepoint once.
void convert(lpchar_t& c)
{
	if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
		return;
	switch (c)
	{
	case 224: c = 'a'; break;
	case 225: c = 'a'; break;
	case 226: c = 'a'; break;
	case 231: c = 'c'; break;
	case 232: c = 'e'; break;
	case 233: c = 'e'; break;
	case 234: c = 'e'; break;
	case 244: c = 'o'; break;
	default:
		static set <lpchar_t> unknowns;
		if (unknowns.find(c) == unknowns.end())
		{
			unknowns.insert(c);
			printf("Unknown character - %d %C\n", (int)c, c);
		}
	}
}

// Strips / * | and punctuation from me1, lowercases, then convert() each remaining char.
lpwstring reduce(lpwstring me1)
{
	size_t ws = 0;
	ws = me1.find_first_of(u"/");
	if (ws != lpwstring::npos)
		me1.erase(me1.begin() + ws, me1.end());
	ws = me1.find_first_of(u"*");
	if (ws != lpwstring::npos)
		me1.erase(me1.begin() + ws, me1.end());
	ws = me1.find_first_of(u"|");
	if (ws != lpwstring::npos)
		me1.erase(me1.begin() + ws, me1.end());
	while ((ws = me1.find_first_of(u"&.\r\n \'-)(")) != lpwstring::npos)
		me1.erase(me1.begin() + ws);
	lp_towlower_str((lpchar_t*)me1.c_str());
	for (int r = 0; r < me1.size(); r++)
		convert(me1[r]);
	return me1;
}





#define MAX_COLUMNS 4
vector <string> tokens;




// True if s is non-empty and starts with an uppercase letter.
bool isCapitalWord(string& s)
{
	if (s.empty())
		return false;
	for (unsigned int J = 0; J < s.length(); J++)
		if (!isupper((unsigned)s[J]) && !isspace((unsigned)s[J]))
			return false;
	return true;
}

// isCapitalWord(tokens[I]).
bool isCapitalWord(int I)
{
	return isCapitalWord(tokens[I]);
}

// True if s looks like an HTML tag (starts with '<' and ends with '>').
bool isToken(string& s)
{
	return s[0] == '<' &&
		s.length() > 1 &&
		s[s.length() - 1] == '>';
}


// isToken(tokens[I]).
bool isToken(int I)
{
	return isToken(tokens[I]);
}

// Prints tokens around I and aborts the ingest (error() implementation).
void error(int I)
{
	printf("STOP %d\n", I);
	if (getchar() == EOF)
		printf("ERROR");
}

// Copies s into tmp with leading/trailing whitespace removed; returns tmp.
string trim(string& s, string& tmp)
{
	tmp = s;
	size_t J;
	for (J = tmp.length() - 1; J; J--)
		if (!isspace((unsigned char)tmp[J]))
			break;
	if (J < tmp.length() - 1)
		tmp.erase(J + 1);
	for (J = 0; tmp[J]; J++)
		if (!isspace((unsigned char)tmp[J]))
			break;
	if (J > 0)
		tmp.erase(0, J);
	return tmp;
}

// Wide-string trim into tmp; returns tmp.
lpwstring trim(lpwstring& s, lpwstring& tmp)
{
	tmp = s;
	size_t J;
	for (J = tmp.length() - 1; J; J--)
		if (!iswspace((lpchar_t)tmp[J]))
			break;
	if (J < tmp.length() - 1)
		tmp.erase(J + 1);
	for (J = 0; tmp[J]; J++)
		if (!iswspace((lpchar_t)tmp[J]))
			break;
	if (J > 0)
		tmp.erase(0, J);
	return tmp;
}


// trim(tokens[I], tmp).
string trim(int I, string& tmp)
{
	return trim(tokens[I], tmp);
}

// Reads MAINDIR\\...\\Koptimized_tags_noTables.html into global tokens (tags vs. text).
void processIntoTokens()
{
	int fd;
	string tablesPath = string(MAINDIR) + "\\Linguistics information\\thesaurus\\Koptimized_tags_noTables.html";
	fd = ::open(tablesPath.c_str(), O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0)
	{
		lplog(LOG_ERROR, u"ERROR:processIntoTokens cannot open %S.", tablesPath.c_str());
		return;
	}
	int fl = lp_filelength(fd);
	char* buffer = (char*)malloc(fl + 4);
	string buf;
	//int numTable = 0;
	{
		if (::read(fd, buffer, fl) < 0)
			lplog(LOG_FATAL_ERROR, u"Error reading thesaurus HTML file.");
		::close(fd);
	}
	string tag, beginTag, endTag, words;
	set <string> tags;
	bool lastTagWasBeginTag = false;
	// , removeTag = false;
	tokens.reserve(700000);
	// starts at a begin tag
	int I = 0;
	for (; I < fl && buffer[I] != '<'; I++); // skip encoding
	for (; I < fl;)
	{
		if ((I % 1000) == 0)
			printf("phase one %d%% %d out of %d [numTokens=%d]\r", I * 100 / fl, I, fl, (int)tokens.size());
		for (; I < fl && isspace((unsigned char)buffer[I]); I++);
		tag.clear();
		bool hasSlash = false;
		for (; I < fl && buffer[I] != '>'; I++)
		{
			if (buffer[I] == '/')
				hasSlash = true;
			tag += buffer[I];
		}
		tag += buffer[I++];
		tags.insert(tag);
		while (I < fl && (buffer[I] == '\n' || buffer[I] == '\r'))
			I++;
		if (!hasSlash || lastTagWasBeginTag)
			tokens.push_back(tag);
		lastTagWasBeginTag = !hasSlash;
		words.clear();
		bool allSpace = true;
		for (; I < fl && buffer[I] != '<'; I++)
		{
			if (!isspace((unsigned char)buffer[I]))
				allSpace = false;
			words += buffer[I];
		}
		if (!allSpace)
			tokens.push_back(words);
	}
	int t = 0;
	for (set <string>::iterator ti = tags.begin(), tiEnd = tags.end(); ti != tiEnd; ti++)
		printf("%d:%s\n", t++, ti->c_str());
	free(buffer);
}


// Debug-prints one sDefinition (mainEntry, wordType, synonym/antonym lists).
void printEntry(sDefinition d)
{
	printf("mainEntry[%s] wordType[%s]:", d.mainEntry.c_str(), d.wordType.c_str());
	for (unsigned int I = 0; I < d.primarySynonyms.size(); I++)
		printf("primarySynonyms[%s]", d.primarySynonyms[I].c_str());
	if (d.accumulatedSynonyms.size() > 0)
	{
		printf("\naccumulatedSynonyms:");
		for (unsigned int I = 0; I < d.accumulatedSynonyms.size(); I++)
			printf("[%s]", d.accumulatedSynonyms[I].c_str());
	}
	if (d.accumulatedAntonyms.size() > 0)
	{
		printf("\naccumulatedAntonyms:");
		for (unsigned int I = 0; I < d.accumulatedAntonyms.size(); I++)
			printf("[%s]", d.accumulatedAntonyms[I].c_str());
	}
	printf("\n");
	for (unsigned int I = 0; I < d.concepts.size(); I++)
		printf("concepts[%d]", d.concepts[I]);
	printf("\n");
}

// Logs a parse disagreement at 'where' for whichEntry and continues (unlike error(int)).
void error(vector <sDefinition>::iterator e1, const char* where, string whichEntry)
{
	static int errors = 0;
	printf("\n%d:%s %s                     \n", errors++, where, whichEntry.c_str());
	printEntry(*e1);
	//error(-1);
}


set <string> wordTypeMap;

// Splits inputWord on spaces into words (no empty pieces).
void breakBySpace(string inputWord, vector <string>& words)
{
	string tmp;
	string token = trim(inputWord, tmp);
	string word;
	//bool 	printWordsTemp = false;
	for (unsigned int w = 0; w < token.size(); w++)
		if (token[w] == ' ')
		{
			words.push_back(word);
			word.clear();
		}
		else if (token[w] != ' ' || word.size() > 0)
			word += token[w];
	if (word.length() > 0)
		words.push_back(word);
}

int numDashedWords = 0, numconvertedWords = 0;
// If possibleDashedWord contains '-', looks up the undashed form in Words/DB and rewrites it.
void removeDash(MYSQL mysql, string& possibleDashedWord)
{
	int64_t whereDash = possibleDashedWord.find('-');
	if (whereDash == string::npos)
		return;
	bool extraStar = false;
	if (extraStar = possibleDashedWord[possibleDashedWord.size() - 1] == '*')
		possibleDashedWord.erase(possibleDashedWord.size() - 1);
	numDashedWords++;
	lpwstring rd;
	mTW(possibleDashedWord, rd);
	int64_t whereSpace = rd.find(' ');
	while (whereSpace != string::npos)
	{
		if (whereSpace > 0 && !(rd[whereSpace - 1] == '-' || rd[whereSpace + 1] == '-'))
		{
			whereSpace = rd.find(' ', whereSpace + 1);
			continue;
		}
		rd.erase(rd.begin() + whereSpace);
		whereSpace = rd.find(' ', whereSpace);
	}
	whereSpace = rd.find(' ');
	if (whereSpace != lpwstring::npos)
	{
		string tmp;
		wTM(rd, tmp);
		vector <string> words;
		breakBySpace(tmp, words);
		tmp.clear();
		for (int I = 0; I < words.size(); I++)
		{
			removeDash(mysql, words[I]);
			tmp += words[I];
			if (I < words.size() - 1)
				tmp += " ";
		}
		lpwstring wtmp;
		mTW(tmp, wtmp);
		if (wtmp != rd)
		{
			rd = wtmp;
			numconvertedWords++;
			printf("%d:%d:%s -> %S                                            \n", numDashedWords, numconvertedWords, possibleDashedWord.c_str(), rd.c_str());
			if (extraStar)
				rd += u'*';
			wTM(rd, possibleDashedWord);
		}
		else
			printf("%d:%S (from %s) not found.                                  \n", numDashedWords, rd.c_str(), possibleDashedWord.c_str());
		return;
	}
	while (whereDash != string::npos)
	{
		rd.erase(rd.begin() + whereDash);
		whereDash = rd.find('-');
	}
	tIWMM iWord = Words.end();
	int result = 0;
	if ((result = Words.parseWord(&mysql, rd, iWord, false)) >= 0)
	{
		numconvertedWords++;
		printf("%d:%d:%s -> %S                                            \n", numDashedWords, numconvertedWords, possibleDashedWord.c_str(), rd.c_str());
		if (extraStar)
			rd += u'*';
		wTM(rd, possibleDashedWord);
	}
	else
		printf("%d:%S (from %s) not found.                                  \n", numDashedWords, rd.c_str(), possibleDashedWord.c_str());
}



/*
"p" - if after Ant., ignore.  if the next HTML tag after </i>, move the </i> until before the "p" tag
"s19" - sometimes at start of entry, but also at other locations
"s21" - always around Ant.
"s24" - always introductory
"s25" - like "p"
"s27" - like "21"
*/
// Tokenizes the stripped Roget HTML and walks span.s19/s24 entries into the global
// thesaurus vector. Returns 0. Does not INSERT into MySQL despite
// taking a mysql handle (write is commented out).
vector <sDefinition> thesaurus;


/* there are no th tags
<tr ????>
	<td ????>Jill</td>
	<td ???/>
	<td>50</td>
</tr>
*/


