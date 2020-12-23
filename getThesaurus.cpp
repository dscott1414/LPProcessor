#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <winsock.h>
#include "Winhttp.h"
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "time.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "profile.h"
#include <share.h>
#include <tchar.h>
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
bool myquery(MYSQL *mysql, wchar_t *q, MYSQL_RES * &result, bool allowFailure = false);
void scrapeNewThesaurus(wstring word, int synonymType, vector <sDefinition> &d);

void getSynonymsFromDB(MYSQL mysql, wstring word, vector <unordered_set <wstring> > &synonyms, vector <wstring > &alternatives, int synonymType)
{
	wstring query = L"select primarySynonyms, accumulatedSynonyms from thesaurus where mainEntry = '";
	query += word + L"'";
	// thesaurus mappings
	// "adj"=1, "adv"=2, "prep"=4, "pron"=8, "conj"=16, "det"=32, "interj"=64, "n"=128, "v"=256, NULL };
	if (synonymType == 1) // NOUN
		query += L" and (wordType&128)=128";
	else if (synonymType == 2) // VERB
		query += L" and (wordType&256)=256";
	else if (synonymType == 3) // ADJ
		query += L" and (wordType&1)=1";
	else if (synonymType == 4) // ADV
		query += L" and (wordType&2)=2";
	MYSQL_RES *result = NULL;
	MYSQL_ROW sqlrow;
	if (myquery(&mysql, (wchar_t *)query.c_str(), result))
	{
		while ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			unordered_set <wstring> ss;
			string primarySynonyms = (sqlrow[0] == NULL) ? "" : sqlrow[0];
			string properties = (sqlrow[1] == NULL) ? "" : sqlrow[1];
			wstring firstWordSynonym;
			int lastBegin = 0;
			for (unsigned int s = 0; s < properties.size(); s++)
				if (properties[s] == ';')
				{
				wstring wtmp;
				mTW(properties.substr(lastBegin, s - lastBegin), wtmp);
				if (wtmp.length()>0 && wtmp[wtmp.length() - 1] == '*')
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
			wstring pslast;
			mTW(primarySynonyms, pslast);
			wstring alternative = pslast + L" " + firstWordSynonym;
			alternatives.push_back(alternative);
		}
		mysql_free_result(result);
	}
}

void split(string str, vector <string> &words, char *splitch)
{
	int ch = -1;
	vector <string> syns;
	do
	{
		int nextch = str.find(splitch, ch + 1);
		words.push_back(str.substr(ch+1, nextch - ch - 1));
		ch = nextch;
	}
	while (ch != string::npos);
}

string vectorString(vector <string> &vstr, string &tmpstr, string separator);

void splitPrimarySynonyms(MYSQL mysql)
{
	wstring query = L"select primarySynonyms from thesaurus where primarySynonyms like '% or %'";
	MYSQL_RES *result = NULL;
	MYSQL_ROW sqlrow;
	if (myquery(&mysql, (wchar_t *)query.c_str(), result))
	{
		int totalRows = 0;
		int rowsNotProcessed = 0;
		while ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			totalRows++;
			string primarySynonyms = (sqlrow[0] == NULL) ? "" : sqlrow[0];
			// split by semicolons
			bool caseMet = false;
			vector <string> syns,syns2;
			split(primarySynonyms, syns2, ";");
			for (int s = 0; s < syns2.size(); s++)
			{
				if (syns2[s].find(" or ") != string::npos)
				{
					vector <string> synwords;
					split(syns2[s], synwords, " ");
					string sw;
					if (synwords[1] == "or")
					{
						for (int I = 3; I < synwords.size(); I++)
							sw += " " + synwords[I];
						string s1 = synwords[0] + sw;
						string s2 = synwords[2] + sw;
						syns.push_back(s1);
						syns.push_back(s2);
						caseMet = true;
					}
				}
				else
					syns.push_back(syns2[s]);
			}
			if (caseMet)
			{
				string tmpstr;
				lplog(LOG_WHERE, L"%S:%S", primarySynonyms.c_str(), vectorString(syns, tmpstr, " | ").c_str());
			}
			else
				rowsNotProcessed++;
		}
		lplog(LOG_WHERE, L"%d out of %d processed (%d%%).", totalRows - rowsNotProcessed, totalRows, (totalRows - rowsNotProcessed)*100/totalRows);
	}
	logCache = 0;
	lplog(LOG_WHERE, L"STOP");
	exit(0);
}

void scrapeOldThesaurus(wstring word, wstring buffer, unordered_set <wstring> &synonyms)
{
	wstring match;
	int lastNewLine = 1000;
	size_t beginPos = buffer.find(L"Main Entry:", 0);
	if (beginPos == wstring::npos)
		return;
	beginPos += wcslen(L"Main Entry:");
	size_t endPos = 1000000, tmpPos;
	wchar_t *endStr[] = { L"Main Entry:", L"Roget's 21st Century Thesaurus", L"Adjective Finder", L"Synonym Collection", L"Search another word", L"Antonyms:", L"* = informal/non-formal usage", NULL };
	for (int I = 0; endStr[I] != NULL; I++)
		if ((tmpPos = buffer.find(endStr[I], beginPos)) != wstring::npos && tmpPos<endPos)
			endPos = tmpPos;
	if (endPos == wstring::npos)
		return;
	match = buffer.substr(beginPos, endPos - beginPos);
	bool addressEncountered = false; // noMainEntryMatch = (match.find(word) == wstring::npos), 
	size_t pos = match.find(L"Synonyms:");
	if (pos != wstring::npos)
	{
		wstring s;
		for (pos += wcslen(L"Synonyms:"); pos<(signed)match.length(); pos++)
			if (iswalpha(match[pos]) || match[pos] == L'\'')
				s += match[pos];
			else if (match[pos] == L' ')
			{
				if (!s.empty()) s += match[pos];
			}
			else if (match[pos] == L',')
			{
				while (s.length()>0 && iswspace(s[s.length() - 1]))
					s.erase(s.length() - 1);
				transform(s.begin(), s.end(), s.begin(), (int(*)(int)) tolower);
				if (s.length()>0)
				{
					synonyms.insert(s);
				}
				s.clear();
			}
			else if (match[pos - 1] != L',' && match[pos] == 13 && match[pos + 1] == 10 &&
				(iswupper(match[pos + 2]) || iswdigit(match[pos + 2]) || !iswalpha(match[pos + 2]))) // Ads start with unpredictable strings, but always capitalized, after a newline.
			{
				break;
			}
			else if (match[pos] == 13 && match[pos + 1] == 10)
				lastNewLine = s.length();
			else if (addressEncountered = match[pos] == L'.' && pos>3 && match[pos - 1] == L'w' && match[pos - 2] == L'w' && match[pos - 3] == L'w')
				break;
			transform(s.begin(), s.end(), s.begin(), (int(*)(int)) tolower);
			if (s.length()>0)
			{
				if ((s.length() >= 64 || addressEncountered) && lastNewLine<(signed)s.length())
					s = s.substr(0, lastNewLine);
				if (s.length() >= 64)
					wprintf(L"\nSynonym of %s (%s) is too long.\n", word.c_str(), s.c_str());
				else
					synonyms.insert(s);
			}
	}
	extern int logSynonymDetail;
	//if (noMainEntryMatch && synonyms.find(word) == synonyms.end())
	//	wprintf(L"%s itself not found in synonyms.\n", word.c_str());
}

void convert(wchar_t &c)
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
		static set <wchar_t> unknowns;
		if (unknowns.find(c) == unknowns.end())
		{
			unknowns.insert(c);
			printf("Unknown character - %d %C\n", c, c);
		}
	}
}

wstring reduce(wstring me1)
{
	size_t ws = 0;
	ws = me1.find_first_of(L"/");
	if (ws != wstring::npos)
		me1.erase(me1.begin() + ws, me1.end());
	ws = me1.find_first_of(L"*");
	if (ws != wstring::npos)
		me1.erase(me1.begin() + ws, me1.end());
	ws = me1.find_first_of(L"|");
	if (ws != wstring::npos)
		me1.erase(me1.begin() + ws, me1.end());
	while ((ws = me1.find_first_of(L"&.\r\n \'-)(")) != wstring::npos)
		me1.erase(me1.begin() + ws);
	_wcslwr((wchar_t *)me1.c_str());
	for (int r = 0; r < me1.size(); r++)
		convert(me1[r]);
	return me1;
}

bool compareWordSets(unordered_set <wstring> &s1, unordered_set <string> &s2)
{
	set <wstring> s11, s21;
	for (auto ss = s1.begin(), ssEnd = s1.end(); ss != ssEnd; ss++)
	{
		s11.insert(reduce(*ss));
	}
	for (auto ss = s2.begin(), ssEnd = s2.end(); ss != ssEnd; ss++)
	{
		wstring t;
		mTW(*ss, t);
		s21.insert(reduce(t));
	}
	for (auto ss = s11.begin(), ssEnd = s11.end(); ss != ssEnd; ss++)
	{
		if (s21.find(*ss) == s21.end())
			return false;
	}
	return true;
}

// lastWordPrimaryFromDB == "fatigue"
// firstWordSynonymFromDB == "catch flies"
// alternative == fatiguecatch flies
bool compareWordSets(vector <string> &s1, unordered_set <wstring> &synonymsFromDB, wstring alternative)
{
	set <wstring> s11, synonymsFromDBR;
	for (vector <string>::iterator ss = s1.begin(), ssEnd = s1.end(); ss != ssEnd; ss++)
	{
		wstring t;
		s11.insert(reduce(mTW(*ss,t)));
	}
	for (auto ss = synonymsFromDB.begin(), ssEnd = synonymsFromDB.end(); ss != ssEnd; ss++)
	{
		synonymsFromDBR.insert(reduce(*ss));
	}
	wstring alternativeR = reduce(alternative);
	for (auto ss = s11.begin(), ssEnd = s11.end(); ss != ssEnd; ss++)
	{
		if (synonymsFromDBR.find(*ss) == synonymsFromDBR.end() && *ss != alternativeR)
			return false;
	}
	return true;
}

void testThesaurus()
{
	WIN32_FIND_DATA ffd;
	HANDLE hFind;
	MYSQL mysql;
	if (mysql_init(&mysql) == NULL)
		wprintf(L"Failed to initialize MySQL");
	bool keep_connect = true;
	mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
	string sqlStr;
	if ((mysql_real_connect(&mysql, "localhost", "root", "byron0", "lp", 0, NULL, 0) != NULL))
	{
		mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
		if (mysql_set_character_set(&mysql, "utf8"))
			wprintf(L"Error setting default client character set to utf8.  Default now: %S\n", mysql_character_set_name(&mysql));
	}
	mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
	wstring thesaurusDir = wstring(LMAINDIR) + L"\\old thesaurus entries";
	_wchdir(thesaurusDir.c_str());
	hFind = FindFirstFile(L"*.thesaurus.txt.*", &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("FindFirstFile failed (%d)\n", (int)GetLastError());
		return;
	}
	else
	{
		int total = 0, nonMatched = 0, dbEmpty = 0; // removed = 0, 
		do
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				_tprintf(TEXT("  %s   <DIR>\n"), ffd.cFileName);
			}
			else
			{
				int synonymType = -1;
				if (isdigit(ffd.cFileName[wcslen(ffd.cFileName) - 1]))
					synonymType = ffd.cFileName[wcslen(ffd.cFileName) - 1] - '1';
				if (synonymType < 0)
					continue;
				int fd;
				_wsopen_s(&fd, ffd.cFileName, _O_RDWR | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE); // error = 
				total++;
				size_t fl = _filelength(fd);
				wchar_t *buffer = (wchar_t *)malloc(fl + 2);
				if (fd >= 0)
				{
					_read(fd, buffer, (unsigned int)fl);
					_close(fd);
				}
				unordered_set <wstring> scrapedSynonyms;
				wstring word = ffd.cFileName;
				int where = word.find('.');
				word.erase(where);
				word.erase(word.begin() + 0, word.begin() + 1);
				int space;
				while ((space = word.find('+')) != wstring::npos)
					word[space] = ' ';
				scrapeOldThesaurus(word, buffer, scrapedSynonyms);
				vector < wstring > dbAlternatives;
				vector < unordered_set <wstring> > dbSynonyms;
				if (synonymType >= 0)
				{
					getSynonymsFromDB(mysql, word, dbSynonyms, dbAlternatives, synonymType);
					vector <sDefinition> d;
					scrapeNewThesaurus(word, synonymType, d);
					for (int I = 0; I < dbSynonyms.size(); I++)
					{
						bool atLeastOneScrapedIdenticalWithNewScraped = false;
						bool atLeastOneNewScrapedIdenticalWithDB = false;
						for (int J = 0; J < d.size(); J++)
						{
							atLeastOneScrapedIdenticalWithNewScraped |= compareWordSets(d[J].accumulatedSynonyms, scrapedSynonyms,L"");
							atLeastOneNewScrapedIdenticalWithDB |= compareWordSets(d[J].accumulatedSynonyms, dbSynonyms[I], dbAlternatives[I]);
						}
						if (!atLeastOneNewScrapedIdenticalWithDB && !atLeastOneScrapedIdenticalWithNewScraped)
						{
							nonMatched++;
							printf("\nWORD [%d:%d]%S[%d]:\n", nonMatched, total, word.c_str(), synonymType);
							printf("scrapedSynonyms:");
							for (auto ss = scrapedSynonyms.begin(), ssEnd = scrapedSynonyms.end(); ss != ssEnd; ss++)
								if (dbSynonyms[I].find(*ss)==dbSynonyms[I].end())
									printf("%S;", ss->c_str());
							printf("\ndbSynonyms:");
							if (dbSynonyms.size()>1)
								printf("\n[%d]:", I);
							for (auto ss = dbSynonyms[I].begin(), ssEnd = dbSynonyms[I].end(); ss != ssEnd; ss++)
								printf("%S;", ss->c_str());
							printf("\nnewScrapedSynonyms:");
							for (int J = 0; J < d.size(); J++)
							{
								if (d.size()>1)
									printf("\n[%d]:", J);
								for (vector <string>::iterator ss = d[J].accumulatedSynonyms.begin(), ssEnd = d[J].accumulatedSynonyms.end(); ss != ssEnd; ss++)
								{
									wstring s;
									mTW(*ss, s);
									int whereBar = s.find(L'|');
									if (whereBar != wstring::npos)
										s.erase(whereBar);
									if (dbSynonyms[I].find(s) == dbSynonyms[I].end())
										printf("%S;", s.c_str());
								}
							}
							printf("\n");
						}
					}
					if (dbSynonyms.empty())
						dbEmpty++;
				}
				free(buffer);
			}
		} while (FindNextFile(hFind, &ffd) != 0);
		printf("dbempty=%d nonMatched=%d total=%d (%d%%)\n", dbEmpty, nonMatched, total,100*nonMatched/(total-dbEmpty));
		FindClose(hFind);
	}
}


#define MAX_COLUMNS 4
vector <string> tokens;
bool isBeginToken(int I, char *token)
{
	return !strncmp(tokens[I].c_str() + 1, token, strlen(token)) &&
		tokens[I][0] == '<' && 
		tokens[I].length() > 1 &&
		tokens[I][tokens[I].length()-1] == '>';
}

bool isWord(int I)
{
	return tokens[I].find("<") == string::npos;
}

bool isEndToken(int I, char *token)
{
	return !strncmp(tokens[I].c_str() + 2, token, strlen(token)) &&
		tokens[I][0] == '<' && 
		tokens[I][1] == '/' &&
		tokens[I].length() > 1 &&
		tokens[I][tokens[I].length() - 1] == '>';
}

bool isWord(int I, char *word)
{
	return tokens[I] == word;
}

bool isCapitalWord(string &s)
{
	if (s.empty())
		return false;
	for (unsigned int J = 0; J < s.length(); J++)
		if (!isupper((unsigned)s[J]) && !isspace((unsigned)s[J]))
			return false;
	return true;
}

bool isCapitalWord(int I)
{
	return isCapitalWord(tokens[I]);
}

bool isToken(string &s)
{
	return s[0] == '<' &&
		s.length() > 1 &&
		s[s.length() - 1] == '>';
}

bool isNumber(string &s,int &n)
{
	for (int I = 0; s[I]; I++)
		if (!isdigit(s[I]) && !isspace(s[I]) && (s[I] != '-' || I==0))
			return false;
	n = atoi(s.c_str());
	return true;
}

bool isToken(int I)
{
	return isToken(tokens[I]);
}

void error(int I)
{
	printf("STOP %d\n", I);
	getchar();
}

string trim(string &s, string &tmp)
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

wstring trim(wstring &s, wstring &tmp)
{
	tmp = s;
	size_t J;
	for (J = tmp.length() - 1; J; J--)
		if (!iswspace((wchar_t)tmp[J]))
			break;
	if (J < tmp.length() - 1)
		tmp.erase(J + 1);
	for (J = 0; tmp[J]; J++)
		if (!iswspace((wchar_t)tmp[J]))
			break;
	if (J > 0)
		tmp.erase(0, J);
	return tmp;
}

__int64 breakByCommaSemiColon(vector <string> &words, int t, bool primarySemiColon)
{
	string tmp;
	string token = trim(tokens[t],tmp);
	string word;
	__int64 semicolonIndex = -1;
	bool 	printWordsTemp = false;
	for (unsigned int w = 0; w < token.size(); w++)
		if (token[w] == ',')
		{
			if (!primarySemiColon)
			{
				words.push_back(word);
				word.clear();
			}
			else
			{
				int whereSemiColon = token.find(';', w);
				__int64 whereOr = token.find("or",w);
				if (whereOr>0 && (isalpha(token[whereOr - 1]) || isalpha(token[whereOr + 2])))
				{
					whereOr = string::npos;
				}
				if (whereOr!=string::npos && (whereSemiColon==string::npos || whereOr<whereSemiColon))
					printWordsTemp = true;
				else
				{
					words.push_back(word);
					word.clear();
				}
			}
		}
		else if (token[w] == ';')
		{
			words.push_back(word);
			words.push_back(";");
			word.clear();
			if (semicolonIndex >= 0 && semicolonIndex + 1 < (signed)words.size() && words[semicolonIndex + 1] != "concept" && words[semicolonIndex + 1] != "concepts")
			{
				words.erase(words.begin() + semicolonIndex);
				semicolonIndex = -1;
			}
			if (semicolonIndex >= 0)
				error(t);
			semicolonIndex = words.size() - 1;
		}
		else if (token[w] != ' ' || word.size()>0)
			word += token[w];
	if (word.length()>0)
		words.push_back(word);
	if (semicolonIndex >= 0 && semicolonIndex + 1 < (signed)words.size() && words[semicolonIndex + 1] != "concept" && words[semicolonIndex + 1] != "concepts")
	{
		words.erase(words.begin() + semicolonIndex);
		semicolonIndex = -1;
	}
	return semicolonIndex;
}

string trim(int I, string &tmp)
{
	return trim(tokens[I], tmp);
}

void processIntoTokens()
{
	int fd;
	string tablesPath = string(MAINDIR) + "\\Linguistics information\\thesaurus\\Koptimized_tags_noTables.html";
	_sopen_s(&fd, tablesPath.c_str(), _O_RDWR | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	int fl = _filelength(fd);
	char *buffer = (char *)malloc(fl + 2);
	string buf;
	//int numTable = 0;
	if (fd >= 0)
	{
		_read(fd, buffer, fl);
		_close(fd);
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
		while (I<fl && (buffer[I] == '\n' || buffer[I] == '\r'))
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
}

void distributeRest(sDefinition &d)
{
	bool firstSynonymWordHit = d.accumulatedSynonyms.size()>0;
	bool conceptsHit = false;
	bool lookingForAnt = false;
	bool foundAnt = false;
	string tmp;
	for (unsigned int I = 0; I < d.rest.size(); I++)
	{
		if (isToken(d.rest[I]))
			continue;
		else if (firstSynonymWordHit == false)
		{
			if (d.primarySynonyms.size() > 0 && d.primarySynonyms[d.primarySynonyms.size() - 1][d.primarySynonyms[d.primarySynonyms.size() - 1].size() - 1] == '-')
			{
				d.primarySynonyms[d.primarySynonyms.size() - 1].erase(d.primarySynonyms[d.primarySynonyms.size() - 1].size() - 1);
				d.primarySynonyms[d.primarySynonyms.size() - 1] += d.rest[I];
			}
			else
			{
				d.accumulatedSynonyms.push_back(d.rest[I]);
				firstSynonymWordHit = true;
			}
		}
		else if (conceptsHit == false)
		{
			if (d.rest[I] == ";")
			{
				if (d.rest[I + 1] == "</span>")
					I++;
				if (I + 2 < d.rest.size() &&
					(trim(d.rest[I + 1], tmp) == "concept" || trim(d.rest[I + 1], tmp) == "concepts"))
				{
					conceptsHit = true;
					int cn;
					if (d.rest[I + 2] != "<i>" && d.rest[I + 2] != "</span>" && !isNumber(d.rest[I+2], cn))
						error(I);
					else
					{
						I += 2;
						if (!isNumber(d.rest[I], cn))
							I++;
						for (; I < d.rest.size(); I++)
							if (isNumber(d.rest[I], cn))
								d.concepts.push_back(cn);
							else if (d.rest[I] == "</i>")
							{
								while (I+1<d.rest.size() && isNumber(d.rest[I + 1], cn))
								{
									d.concepts.push_back(cn);
									I++;
								}
								lookingForAnt = true;
								break;
							}
							else if (d.rest[I] == "Ant.")
							{
								lookingForAnt = true;
								I--;
								break;
							}
							else
								error(I);
					}
				}
				else
					error(I);
			}
			else
				d.accumulatedSynonyms.push_back(d.rest[I]);
		}
		else if (lookingForAnt == true && foundAnt == false)
		{
			if (trim(d.rest[I], tmp) == "—" || trim(d.rest[I], tmp) == "-")
			{
			}
			else if (trim(d.rest[I], tmp) == "Ant." || trim(d.rest[I], tmp) == "-Ant.")
			{
				foundAnt = true;
			}
			else
				error(I);
		}
		else if (lookingForAnt == true && foundAnt == true)
		{
				if (isCapitalWord(d.rest[I]))
					error(I);
				else
					d.accumulatedAntonyms.push_back(d.rest[I]);
		}
	}
}

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

void error(vector <sDefinition>::iterator e1,char *where,string whichEntry)
{
	static int errors = 0;
	printf("\n%d:%s %s                     \n", errors++, where, whichEntry.c_str());
	printEntry(*e1);
	//error(-1);
}

bool compareOK(string me1, string me2)
{
	size_t ws = 0;
	ws = me1.find_first_of("/");
	if (ws != string::npos)
		me1.erase(me1.begin() + ws, me1.end());
	ws = me2.find_first_of("/");
	if (ws != string::npos)
		me2.erase(me2.begin() + ws, me2.end());
	while ((ws = me1.find_first_of("&.\r\n \'-)(")) != string::npos)
		me1.erase(me1.begin() + ws);
	while ((ws = me2.find_first_of("&.\r\n \'-)(")) != string::npos)
		me2.erase(me2.begin() + ws);
	_strlwr((char *)me1.c_str());
	_strlwr((char *)me2.c_str());
	size_t len = min(me1.length(), me2.length());
	return strncmp(me2.c_str(),me1.c_str(),len)<=0;
}

set <string> wordTypeMap;
void checkLastEntry(vector <sDefinition> & thesaurus)
{
	vector <sDefinition>::iterator e1 = thesaurus.begin() + thesaurus.size() - 1;
	if (thesaurus.size() >= 2)
	{
		vector <sDefinition>::iterator e2 = thesaurus.begin() + thesaurus.size() - 2;
		if (!compareOK(e1->mainEntry, e2->mainEntry))
			error(e1, "mainEntry", e2->mainEntry);
	}
	//for (int I = 0; I < (signed)e1->primarySynonyms.size() - 1; I++)
	//	if (e1->primarySynonyms[I] > e1->primarySynonyms[I + 1])
	//		error(e1, "primarySynonyms", e1->primarySynonyms[I]);
	for (int I = 0; I < (signed)e1->accumulatedAntonyms.size() - 1; I++)
		if (!compareOK(e1->accumulatedAntonyms[I+1],e1->accumulatedAntonyms[I]))
			error(e1, "accumulatedAntonyms", e1->accumulatedAntonyms[I]);
	for (int I = 0; I < (signed)e1->accumulatedSynonyms.size() - 1; I++)
		if (!compareOK(e1->accumulatedSynonyms[I+1],e1->accumulatedSynonyms[I ]))
			error(e1, "accumulatedSynonyms", e1->accumulatedSynonyms[I]);
	for (int I = 0; I < (signed)e1->concepts.size() - 1; I++)
		if (e1->concepts[I] > e1->concepts[I + 1])
			error(e1, "concepts", "");
	wordTypeMap.insert(e1->wordType);
}

void breakBySpace(string inputWord,vector <string> &words)
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
		else if (token[w] != ' ' || word.size()>0)
			word += token[w];
	if (word.length()>0)
		words.push_back(word);
}

int numDashedWords = 0, numconvertedWords = 0;
void removeDash(MYSQL mysql, string &possibleDashedWord)
{
	__int64 whereDash = possibleDashedWord.find('-');
	if (whereDash == string::npos)
		return;
	bool extraStar = false;
	if (extraStar = possibleDashedWord[possibleDashedWord.size() - 1] == '*')
		possibleDashedWord.erase(possibleDashedWord.size() - 1);
	numDashedWords++;
	wstring rd;
	mTW(possibleDashedWord, rd);
	__int64 whereSpace = rd.find(' ');
	while (whereSpace != string::npos)
	{
		if (whereSpace > 0 && !(rd[whereSpace - 1] == '-' || rd[whereSpace + 1] == '-'))
		{
			whereSpace = rd.find(' ', whereSpace+1);
			continue;
		}
		rd.erase(rd.begin() + whereSpace);
		whereSpace = rd.find(' ', whereSpace);
	}
	whereSpace = rd.find(' ');
	if (whereSpace != wstring::npos)
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
		wstring wtmp;
		mTW(tmp, wtmp);
		if (wtmp != rd)
		{
			rd=wtmp;
			numconvertedWords++;
			printf("%d:%d:%s -> %S                                            \n", numDashedWords, numconvertedWords, possibleDashedWord.c_str(), rd.c_str());
			if (extraStar)
				rd += L'*';
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
	if ((result = Words.parseWord(&mysql, rd, iWord,false))>=0)
	{
		numconvertedWords++;
		printf("%d:%d:%s -> %S                                            \n", numDashedWords, numconvertedWords, possibleDashedWord.c_str(), rd.c_str());
		if (extraStar)
			rd += L'*';
		wTM(rd, possibleDashedWord);
	}
	else
		printf("%d:%S (from %s) not found.                                  \n", numDashedWords, rd.c_str(), possibleDashedWord.c_str());
}

void removeDashes(MYSQL mysql, sDefinition &d)
{
	removeDash(mysql, d.mainEntry);
	for (int I = 0; I < (signed)d.accumulatedAntonyms.size(); I++)
		removeDash(mysql, d.accumulatedAntonyms[I]);
	for (int I = 0; I < (signed)d.accumulatedSynonyms.size(); I++)
		removeDash(mysql, d.accumulatedSynonyms[I]);
}

void combineDashes(vector <string> &a)
{
	for (unsigned int I = 0; I < a.size(); I++)
		if (a[I][a[I].size() - 1] == '-' && I<a.size()-1)
		{
			a[I].erase(a[I].size() - 1);
			a[I] += a[I + 1];
			a.erase(a.begin() + I + 1);
			I--;
		}
}

/*
"p" - if after Ant., ignore.  if the next HTML tag after </i>, move the </i> until before the "p" tag
"s19" - sometimes at start of entry, but also at other locations
"s21" - always around Ant.
"s24" - always introductory
"s25" - like "p"
"s27" - like "21"
*/
vector <sDefinition> thesaurus;
int getThesaurus(MYSQL mysql)
{
	processIntoTokens();
	printf("\n");
	string tmp2,tmp3;
	for (unsigned int I = 0; I < tokens.size(); )
	{
		printf("phase two %zu%% %d out of %zu\r", I * 100 / tokens.size(), I, tokens.size());
		sDefinition d;
		//printf("\n\n%d:%s\n", I,tokens[I].c_str());
		//for (unsigned int J = I+1; J < tokens.size() && !(isBeginToken(J, "span class=\"s19") || isBeginToken(J, "span class=\"s24")); J++)
			//printf("%d:%s\n", J,tokens[J].c_str());
		if ((isBeginToken(I, "span class=\"s19") || isBeginToken(I, "span class=\"s24"))
			&& isWord(I + 1) && isEndToken(I + 2, "span") &&
			isWord(I + 3, "[") && isBeginToken(I + 4, "i") && isWord(I + 5) && isEndToken(I + 6, "i") &&
			trim(I + 7, tmp3) == "]")
		{
			d.mainEntry = trim(tokens[I + 1],tmp2);
			d.wordType = tokens[I + 5];
			int currentLocation = I + 8;
			bool allowEndScan = false;
			if (isBeginToken(I + 8, "i") && isWord(I + 9) && isEndToken(I + 10, "i"))
			{
				__int64 whereSemiColon = breakByCommaSemiColon(d.primarySynonyms, I + 9,true);
				currentLocation = I + 11;
				if (whereSemiColon > 0)
				{
					d.rest = d.primarySynonyms;
					d.rest.erase(d.rest.begin(), d.rest.begin() + whereSemiColon);
					d.primarySynonyms.erase(d.primarySynonyms.begin() + whereSemiColon, d.primarySynonyms.end());
				}
				if (whereSemiColon<0 && isWord(I + 11)) 
				{
					whereSemiColon = breakByCommaSemiColon(d.accumulatedSynonyms, I + 11,false);
					currentLocation = I + 12;
					if (whereSemiColon > 0)
					{
						d.rest = d.accumulatedSynonyms;
						d.rest.erase(d.rest.begin(), d.rest.begin() + whereSemiColon);
						d.accumulatedSynonyms.erase(d.accumulatedSynonyms.begin() + whereSemiColon, d.accumulatedSynonyms.end());
					}
					if (isBeginToken(I + 12, "span class=\"p\"") || isBeginToken(I + 12, "span class=\"s25\""))
					{
						//bool concatOR = false;
						if (d.primarySynonyms.size() > 0 && d.accumulatedSynonyms.size()>0)
						{
							string lPS = d.primarySynonyms[d.primarySynonyms.size() - 1];
							if (lPS.size() > 3 && lPS[lPS.size() - 3] == ' ' && lPS[lPS.size() - 2] == 'o' && lPS[lPS.size() - 1] == 'r')
							{
								lPS += " ";
								lPS += d.accumulatedSynonyms[0];
								d.accumulatedSynonyms.erase(d.accumulatedSynonyms.begin());
								d.primarySynonyms[d.primarySynonyms.size() - 1] = lPS;
								printf("OR:%s\n", lPS.c_str());
							}
						}
						d.primarySynonyms.insert(d.primarySynonyms.end(), d.accumulatedSynonyms.begin(), d.accumulatedSynonyms.end());
						d.accumulatedSynonyms.clear();
						currentLocation = I + 13;
						allowEndScan = true;
					}
				}
			}
			for (I = currentLocation; I<tokens.size() && !(isBeginToken(I, "span class=\"s19") || isBeginToken(I, "span class=\"s24")); I++)
			{
				//printf("phase two %d%% %d out of %d\r", I * 100 / tokens.size(), I, tokens.size());
				if (isBeginToken(I, "i") || isEndToken(I, "i") || (allowEndScan && isEndToken(I,"span")))
					d.rest.push_back(tokens[I]);
				else if (isToken(I))
					continue;
				else 
				{
					if (isCapitalWord(I))
					{
						if (trim(tokens[I],tmp2).size() == 1)
							printf("*%s*", tokens[I].c_str());
						else
							error(I);
					}
					else
						breakByCommaSemiColon(d.rest, I,false);
				}
			}
			if (I + 6<tokens.size() && isEndToken(I + 1, "span") &&
				isBeginToken(I + 2, "i") && trim(tokens[I + 3], tmp2) == "Ant." && isEndToken(I + 4, "i") &&
				isWord(I + 5))
			{
				//for (unsigned int J = I; J < I + 7; J++)
				//	printf("%d:%s\n", J, tokens[J].c_str());
				breakByCommaSemiColon(d.accumulatedAntonyms, I + 5,false); // __int64 whereSemiColon = 
				I += 6;
			}
			else if (I + 6<tokens.size() && isEndToken(I + 1, "span") &&
				trim(tokens[I + 2], tmp2) == "Ant." && isBeginToken(I + 3, "span") &&
				isWord(I + 4) && isEndToken(I + 5, "span"))
			{
				//for (unsigned int J = I; J < I + 7; J++)
				//	printf("%d:%s\n", J, tokens[J].c_str());
				__int64 whereSemiColon = breakByCommaSemiColon(d.accumulatedAntonyms, I + 4,false);
				I += 6;
				if (whereSemiColon < 0 && isWord(I) && (isBeginToken(I + 1, "span class=\"s19") || isBeginToken(I + 1, "span class=\"s24")))
				{
					whereSemiColon = breakByCommaSemiColon(d.accumulatedAntonyms, I,false );
					I++;
				}
			}
			distributeRest(d);
			int cn;
			for (unsigned int s = 0; s < d.accumulatedSynonyms.size(); s++)
			{
				string synonymConcept = d.accumulatedSynonyms[s];
				if (isdigit(synonymConcept[synonymConcept.size() - 1]) &&	!strncmp((char *)(synonymConcept.c_str()), "concept", 7))
				{
					unsigned int J = 0;
					while (J < synonymConcept.size() && !isdigit(synonymConcept[J]))
						J++;
					synonymConcept.erase(synonymConcept.begin(), synonymConcept.begin() + J);
					if (d.concepts.size()>0)
						error(I);
					if (!isNumber(synonymConcept, cn))
						error(I);
					d.concepts.push_back(cn);
					int c = s;
					for (s++; s < d.accumulatedSynonyms.size(); s++)
						if (!isNumber(d.accumulatedSynonyms[s], cn))
							error(I);
						else
							d.concepts.push_back(cn);
					d.accumulatedSynonyms.erase(d.accumulatedSynonyms.begin() + c, d.accumulatedSynonyms.end());
				}
			}
			combineDashes(d.accumulatedSynonyms);
			combineDashes(d.accumulatedAntonyms);
			removeDashes(mysql,d);
			thesaurus.push_back(d);
			checkLastEntry(thesaurus);
				//printEntry(d);
		}
		else
			error(I);
	}
	int maxSynonymAccumulatedSize = 0, maxAntonymAccumulatedSize = 0, maxConceptSize = 0,maxPrimarySynonymAccumulatedSize=0;
	for (int I = 0; I < thesaurus.size(); I++)
	{
		int accumulatedSize = 0;
		for (int J = 0; J < thesaurus[I].accumulatedAntonyms.size(); J++)
			accumulatedSize += thesaurus[I].accumulatedAntonyms[J].length()+1;
		maxAntonymAccumulatedSize = max(maxAntonymAccumulatedSize, accumulatedSize);
		accumulatedSize = 0;
		for (int J = 0; J < thesaurus[I].accumulatedSynonyms.size(); J++)
			accumulatedSize += thesaurus[I].accumulatedSynonyms[J].length() + 1;
		maxSynonymAccumulatedSize = max(maxSynonymAccumulatedSize, accumulatedSize);
		accumulatedSize = 0;
		for (int J = 0; J < thesaurus[I].primarySynonyms.size(); J++)
			accumulatedSize += thesaurus[I].primarySynonyms[J].length() + 1;
		maxPrimarySynonymAccumulatedSize = max(maxPrimarySynonymAccumulatedSize, accumulatedSize);
		maxConceptSize = max(maxConceptSize, (int)(thesaurus[I].concepts.size() * 5));
	}
	printf("\n");
	printf("maxSynonymAccumulatedSize=%d\n", maxSynonymAccumulatedSize);
	printf("maxAntonymAccumulatedSize=%d\n", maxAntonymAccumulatedSize);
	printf("maxPrimarySynonymAccumulatedSize=%d\n", maxPrimarySynonymAccumulatedSize);
	printf("maxConceptSize=%d\n", maxConceptSize);
	for (set <string>::iterator wti = wordTypeMap.begin(), wtiEnd = wordTypeMap.end(); wti != wtiEnd; wti++)
		printf("%s\n", wti->c_str());
	//	writeThesaurusEntry(thesaurus[I]);
	
	//fd = open(MAINDIR+"\\Linguistics information\\k3_no_tables2.html", _O_RDWR | _O_CREAT, _S_IREAD | _S_IWRITE);
	//fl = buf.length();
	//int error=_write(fd, buf.c_str(), buf.length());
	//printf("%d\n", errno);
	//close(fd);
	return 0;
}

// original: Kipfer B.A. Roget's 21st century thesaurus (3ed., Delta, 2005)(ISBN 0385338953)(O)(977s)_LEn_ - Copy.pdf
// Koptimized.pdf produced using Adobe Acrobat Pro
// Koptimized.html produced from Koptimized.pdf using Adobe Acrobat Pro (save as other HTML Web Page)
// remove tags except for <i> and <span class s5, tr,th,td and <table
int stripTags()
{
	string tablesPath = string(MAINDIR) + "\\Linguistics information\\thesaurus\\Koptimized.html";
	int fd, error = _sopen_s(&fd, tablesPath.c_str(), _O_RDWR, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	size_t fl = _filelength(fd);
	char *buffer = (char *)malloc(fl + 2);
	string buf;
	//int numTable = 0;
	if (fd >= 0)
	{
		_read(fd, buffer, (unsigned int)fl);
		_close(fd);
	}
	bool saveNextSpan = false;
	for (int I = 0; I < fl; I++)
	{
		if (I%4096==4095)
			printf("phase two %zu%% %d out of %zu\r", I * 100 / fl, I, fl);
		if (buffer[I] == '<')
		{
			string token;
			for (; I < fl && buffer[I] != '>'; I++)
			{
				if (buffer[I] == '\n')
					buffer[I] = ' ';
				if (buffer[I]!=' ' || buffer[I-1]!=' ')
					token += buffer[I];
			}
			token += buffer[I];
			if (buffer[I + 1] == '\n')
				I++;
			//printf("%s\n",token.c_str());
			bool save = false;
			if (token == "</span>" && saveNextSpan)
			{
				save = true;
				saveNextSpan = false;
			}
			if (!strncmp(token.c_str()+1, "span", strlen("span")))
			{
				save = true;
				saveNextSpan = true;
			}
			if (!strncmp(token.c_str() + 1, "tr", strlen("tr")) || token == "</tr>")
				save = true;
			if (!strncmp(token.c_str() + 1, "td", strlen("td")) || token == "</td>")
				save = true;
			if (!strncmp(token.c_str() + 1, "th", strlen("th")) || token == "</th>")
				save = true;
			if (!strncmp(token.c_str() + 1, "i", strlen("i")) || token == "</i>")
				save = true;
			if (!strncmp(token.c_str() + 1, "table", strlen("table")) || token == "</table>")
				save = true;
			if (token == "</br>")
				buf += " ";
			// strip out page numbers and titles
			if (!strncmp(token.c_str() + 1, "p class=\"s16\"", strlen("p class=\"s16\"")))
			{
				for (; I < fl; I++)
					if (!strncmp(buffer + I, "</p>", strlen("</p>")))
						break;
				I += 3;
			}
			// strip out thumb print black circle latters class s22
			if (!strncmp(token.c_str() + 1, "span class=\"s22\"", strlen("span class=\"s22\"")))
			{
				for (; I < fl; I++)
					if (!strncmp(buffer + I, "</span>", strlen("</span>")))
						break;
				I += 6;
				save = false;
			}
			if (!strncmp(token.c_str() + 1, "p class=\"s22\"", strlen("p class=\"s22\"")))
			{
				for (; I < fl; I++)
					if (!strncmp(buffer + I, "</p>", strlen("</p>")))
						break;
				I += 3;
			}
			if (!strncmp(token.c_str() + 1, "p class=\"s23\"", strlen("p class=\"s23\"")))
			{
				for (; I < fl; I++)
					if (!strncmp(buffer + I, "</p>", strlen("</p>")))
						break;
				I += 3;
			}

			if (save)
				buf += token + "\n";
			else if (token[1] == 'p')
				buf += " ";
		}
		else
		{
			if (buffer[I]>0)
				buf += buffer[I];
			else
			{
				if (buffer[I] == -30 && buffer[I + 1] == -128 && buffer[I + 2] == -108)
				{
					buf += "-";
					I += 2;
				}
				else if (buffer[I] == -30 && buffer[I + 1] == -128 && (buffer[I + 2]<0 && buffer[I + 3] >= 0 && !isalpha(buffer[I + 3])))
						I += 2;
					else
					{
					if (buffer[I] == -30 && buffer[I + 1] == -128 && buffer[I + 2] == -103)
					{
						buf += '\'';
						I += 2;
					}
					if (buffer[I] == -30 && buffer[I + 1] == -128 && buffer[I + 2] == -104)
					{
						buf += '\'';
						I += 2;
					}
					else if (buffer[I] == -30 && buffer[I + 1] == -128 && buffer[I + 2] == -100)
					{
						buf += '\'';
						I += 2;
					}
					else if (buffer[I] == -61 && (buffer[I + 1] == -86 || buffer[I + 1] == -87 || buffer[I + 1] == -88))
					{
						buf += 'e';
						I += 1;
					}
					else if (buffer[I] == -61 && buffer[I + 1] == -76)
					{
						buf += 'o';
						I += 1;
					}
					else if (buffer[I] == -61 && buffer[I + 1] == -82)
					{
						buf += 'i';
						I += 1;
					}
					else if (buffer[I] == -61 && (buffer[I + 1] == -94 || buffer[I + 1] == -95 || buffer[I + 1] == -96))
					{
						buf += 'a';
						I += 1;
					}
					else if (buffer[I] == -61 && (buffer[I + 1] == -89))
					{
						buf += 'c';
						I += 1;
					}
					else if (buffer[I - 1] >= 0)
					{
						printf("%d: ", I);
						for (int J = I - 20; J < I + 20; J++)
						{
							if (J == I)
								printf("*");
							printf("%c", buffer[J]);
							if (buffer[J] < 0)
								printf("[%d]", (int)buffer[J]);
						}
						printf("\n");
					}
				}
			}
		}
	}
	error = _sopen_s(&fd, tablesPath.c_str(), _O_RDWR | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	fl = buf.length();
	error=_write(fd, buf.c_str(), (unsigned int)buf.length());
	printf("%d\n", errno);
	_close(fd);
	return 0;
}

/* there are no th tags
<tr ????>
  <td ????>Jill</td>
  <td ???/>
  <td>50</td>
</tr>
*/
void processTable(string &tableToken)
{
	string columns[3];
	for (unsigned int I = 0; I < tableToken.size(); I++)
	{
		// strip out thumb print black circle latters class s22
		for (; I < tableToken.size() && isspace(tableToken[I]); I++);
		if (I == tableToken.size())
			break;
		if (!strncmp(tableToken.c_str() + I, "<tr", strlen("<tr")))
		{
			int columnIndex = 0;
			while (I < tableToken.size())
			{
				for (; I < tableToken.size() && tableToken[I] != '>'; I++);
				for (I++; I < tableToken.size() && isspace(tableToken[I]); I++);
				if (!strncmp(tableToken.c_str() + I, "<td", strlen("<td")))
				{
					if (columnIndex>2)
						error(I);
					for (; I < tableToken.size() && tableToken[I] != '>'; I++);
					if (tableToken[I - 1] != '/')
					{
						for (I++; I < tableToken.size() && isspace(tableToken[I]); I++);
						string columnToken;
						for (; I < tableToken.size() && strncmp(tableToken.c_str() + I, "</td", strlen("</td")); I++)
							columns[columnIndex] += tableToken[I];
						I += 4;
					}
					columnIndex++;
				}
				else
				{
					I--;
					break;
				}
			}
		}
		else if (!strncmp(tableToken.c_str() + I, "</tr>", strlen("</tr>")))
		{
			I += 4;
		}
		else
			error(I);
	}
	tableToken.clear();
	for (int I = 0; I < 3; I++)
		tableToken += columns[I];
	if (columns[1].length() > 0)
		error(-1);
}

// remove top and bottom text not belonging to definitions from Koptimized_tags.html
//resolve tables
int resolveTables()
{
	string tablesPath = string(MAINDIR) + "\\Linguistics information\\thesaurus\\Koptimized_tags.html";
	int fd, error = _sopen_s(&fd, tablesPath.c_str(), _O_RDWR|_O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	size_t fl = _filelength(fd);
	char *buffer = (char *)malloc(fl + 2);
	string buf;
	//int numTable = 0;
	if (fd >= 0)
	{
		_read(fd, buffer, (unsigned int)fl);
		_close(fd);
	}
	//bool saveNextSpan = false;
	int tableNum = 0;
	for (int I = 0; I < fl; I++)
	{
		if (I % 4096 == 4095)
			printf("phase two %zu%% %d out of %zu\r", I * 100 / fl, I, fl);
		if (!strncmp(buffer+I, "<table", strlen("<table")))
		{
			string token;
			int begin = I;
			for (; I < fl; I++)
				if (buffer[I] == '>')
					break;
			for (I++; I < fl; I++)
				if (!strncmp(buffer + I, "</table>", strlen("</table>")))
					break;
				else
					token += buffer[I];
			I += 7;
			processTable(token);
			printf("table %d:%d to %d  \n", tableNum++, begin, I);
			buf += token;
		}
		else
			buf += buffer[I];
		if (buf.size() == 7055662 + 161)
			printf("HALT!");
	}
	tablesPath = string(MAINDIR) + "\\Linguistics information\\thesaurus\\Koptimized_tags_noTables2.html";
	error = _sopen_s(&fd, tablesPath.c_str(), _O_RDWR | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	fl = buf.length();
	error = _write(fd, buf.c_str(), (unsigned int)buf.length());
	printf("%d\n", errno);
	_close(fd);
	return 0;
}

