#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <winsock.h>
#include "Winhttp.h"
#include "io.h"
#include "time.h"
#include <fcntl.h>
#include "sys/stat.h"
#include <share.h>
#pragma warning (disable: 4503)
#pragma warning (disable: 4996)
#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
using namespace std;
#include "mysql.h"
#include "wn.h"
#include "..\..\word.h"
#include "..\..\ontology.h"
#include "..\..\source.h"
void scrapeNewThesaurus(wstring word, int synonymType, vector <sDefinition> &d);

__declspec(thread) static void *mTWbuffer = NULL;
__declspec(thread) static unsigned int mTWbufSize = 0;
// multibyte to wide string
const wchar_t *mTW(string inString, wstring &outString)
{
		int queryLength = MultiByteToWideChar(CP_UTF8, 0, inString.c_str(), -1, (wchar_t *)mTWbuffer, mTWbufSize / 2); // bufSize/2 is # of wide chars allocated
	if (mTWbufSize == 0)
	{
		mTWbufSize = max(queryLength * 4, 10000);
		mTWbuffer = malloc(mTWbufSize);
		if (!MultiByteToWideChar(CP_UTF8, 0, inString.c_str(), -1, (wchar_t *)mTWbuffer, mTWbufSize / 2))
			wprintf(L"Error in translating sql request: %S", inString.c_str());
	}
	if (!queryLength)
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			wprintf(L"Error in translating sql request: %S", inString.c_str());
		queryLength = MultiByteToWideChar(CP_UTF8, 0, inString.c_str(), -1, NULL, 0);
		unsigned int previousBufSize = mTWbufSize;
		mTWbufSize = queryLength * 4; // queryLength is # of wide chars returned, bufSize is the # of bytes
		if (!(mTWbuffer = realloc(mTWbuffer, mTWbufSize)))
			wprintf(L"Out of memory requesting %d bytes from sql query %S!", mTWbufSize, inString.c_str());
		if (!MultiByteToWideChar(CP_UTF8, 0, inString.c_str(), -1, (wchar_t *)mTWbuffer, mTWbufSize / 2))
			wprintf(L"Error in translating sql request: %S", inString.c_str());
	}
	outString = (wchar_t *)mTWbuffer;
	return outString.c_str();
}


void *WideCharToMultiByte(wchar_t *q, int &queryLength, void *&buffer, unsigned int &bufSize)
{
		queryLength = WideCharToMultiByte(CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL);
	if (bufSize == 0)
	{
		bufSize = max(queryLength * 2, 10000);
		buffer = malloc(bufSize);
		if (!WideCharToMultiByte(CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL))
		{
			wprintf(L"Error in translating sql request: %s", q);
			return NULL;
		}
	}
	if (!queryLength)
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			wprintf(L"Error in translating sql request: %s", q);
			return NULL;
		}
		queryLength = WideCharToMultiByte(CP_UTF8, 0, q, -1, NULL, 0, NULL, NULL);
		unsigned int previousBufSize = bufSize;
		bufSize = queryLength * 2;
		if (!(buffer = realloc(buffer, bufSize)))
		{
			wprintf(L"Out of memory requesting %d bytes from sql query %s!", bufSize, q);
			return NULL;
		}
		if (!WideCharToMultiByte(CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL))
		{
			wprintf(L"Error in translating sql request: %s", q);
			return NULL;
		}
	}
	return buffer;
}


void *sqlQueryBuffer = NULL; 
unsigned int sqlQueryBufSize = 0; 
bool myquery(MYSQL *mysql, wchar_t *q, MYSQL_RES * &result)
{
		int queryLength;
	void *buffer = WideCharToMultiByte(q, queryLength, sqlQueryBuffer, sqlQueryBufSize);
	if (mysql_real_query(mysql, (char *)buffer, queryLength) != 0)
	{
		wprintf(L"mysql_real_query failed - %S (len=%d): ", mysql_error(mysql), queryLength);
		wstring q2 = q;
		q2 += L"\n";
		return false;
	}
	if (!(result = mysql_store_result(mysql)))
	{
		wprintf(L"%S: Failed to retrieve any results for %s", mysql_error(mysql), q);
		return false;
	}
	return true;
}

void getSynonymsFromDB(MYSQL mysql, wstring word, vector <set <wstring> > &synonyms, int synonymType)
{
	wstring query = L"select accumulatedSynonyms from thesaurus where mainEntry = '";
	query += word + L"'";
	// thesaurus mappings
	// "adj"=1, "adv"=2, "prep"=4, "pron"=8, "conj"=16, "det"=32, "interj"=64, "n"=128, "v"=256, NULL };
	synonymType--; // increased by one in previous code that put the file 
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
			set <wstring> ss;
			string properties = (sqlrow[0] == NULL) ? "" : sqlrow[0];
			int lastBegin = 0;
			for (unsigned int s = 0; s < properties.size(); s++)
				if (properties[s] == ';')
				{
					wstring wtmp;
					mTW(properties.substr(lastBegin, s - lastBegin), wtmp);
					if (wtmp.length()>0 && wtmp[wtmp.length() - 1] == '*')
						wtmp.erase(wtmp.length() - 1);
					ss.insert(wtmp);
					lastBegin = s + 1;
				}
			synonyms.push_back(ss);
		}
		mysql_free_result(result);
	}
}

void scrapeThesaurus(wstring word, wstring buffer, set <wstring> &synonyms)
{
	wstring match;
	int lastNewLine = 1000;
	size_t beginPos = buffer.find(L"Main Entry:", 0);
	if (beginPos == wstring::npos)
		return;
	beginPos += wcslen(L"Main Entry:");
	size_t endPos = 1000000, tmpPos;
	const wchar_t *endStr[] = { L"Main Entry:", L"Roget's 21st Century Thesaurus", L"Adjective Finder", L"Synonym Collection", L"Search another word", L"Antonyms:", L"* = informal/non-formal usage", NULL };
	for (int I = 0; endStr[I] != NULL; I++)
		if ((tmpPos = buffer.find(endStr[I], beginPos)) != wstring::npos && tmpPos<endPos)
			endPos = tmpPos;
	if (endPos == wstring::npos)
		return;
	match = buffer.substr(beginPos, endPos - beginPos);
	bool noMainEntryMatch = (match.find(word) == wstring::npos), addressEncountered = false;
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
				lastNewLine = (int)s.length();
			else if (addressEncountered = match[pos] == L'.' && pos>3 && match[pos - 1] == L'w' && match[pos - 2] == L'w' && match[pos - 3] == L'w')
				break;
			transform(s.begin(), s.end(), s.begin(), (int(*)(int)) tolower);
			if (s.length()>0)
			{
				if ((s.length() >= 64 || addressEncountered) && lastNewLine<(signed)s.length())
					s = s.substr(0, lastNewLine);
				if (s.length() >= 64)
					wprintf(L"Synonym of %s (%s) is too long \n%s.\n", word.c_str(), s.c_str(), match.c_str());
				else
					synonyms.insert(s);
			}
	}
	extern int logSynonymDetail;
	//if (noMainEntryMatch && synonyms.find(word) == synonyms.end())
	//	wprintf(L"%s itself not found in synonyms.\n", word.c_str());
}

wstring reduce(wstring me1)
{
	size_t ws = 0;
	ws = me1.find_first_of(L"/");
	if (ws != wstring::npos)
		me1.erase(me1.begin() + ws, me1.end());
	while ((ws = me1.find_first_of(L"&.\r\n \'-)(")) != wstring::npos)
		me1.erase(me1.begin() + ws);
	_wcslwr((wchar_t *)me1.c_str());
	return me1;
}

bool compareWordSets(set <wstring> &s1, set <wstring> &s2)
{
	set <wstring> s11, s21;
	for (set <wstring>::iterator ss = s1.begin(), ssEnd = s1.end(); ss != ssEnd; ss++)
	{
		s11.insert(reduce(*ss));
	}
	for (set <wstring>::iterator ss = s2.begin(), ssEnd = s2.end(); ss != ssEnd; ss++)
	{
		s21.insert(reduce(*ss));
	}
	for (set <wstring>::iterator ss = s11.begin(), ssEnd = s11.end(); ss != ssEnd; ss++)
	{
		if (s21.find(*ss) == s21.end())
			return false;
	}
	return true;
}

int _tmain(int argc, TCHAR *argv[])
{
	WIN32_FIND_DATA ffd;
	HANDLE hFind;
	MYSQL mysql;
	if (mysql_init(&mysql) == NULL)
		wprintf( L"Failed to initialize MySQL");
	bool keep_connect = true;
	mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
	string sqlStr;
	if ((mysql_real_connect(&mysql, "localhost", "root", "byron0", "lp", 0, NULL, 0) != NULL))
	{
		mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
		if (mysql_set_character_set(&mysql, "utf8"))
			wprintf( L"Error setting default client character set to utf8.  Default now: %S\n", mysql_character_set_name(&mysql));
	}
	mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
	_wchdir(L"E:\\old thesaurus entries");
	hFind = FindFirstFile(L"*.thesaurus.txt.*", &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("FindFirstFile failed (%d)\n", GetLastError());
		return -1;
	}
	else
	{
		int removed = 0, total = 0,nonMatched=0,dbEmpty=0;
		do
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				_tprintf(TEXT("  %s   <DIR>\n"), ffd.cFileName);
			}
			else
			{
				int fd, error = _wsopen_s(&fd, ffd.cFileName, _O_RDWR | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
				total++;
				size_t fl = _filelength(fd);
				wchar_t *buffer = (wchar_t *)malloc(fl + 2);
				if (fd >= 0)
				{
					_read(fd, buffer, (unsigned int)fl);
					_close(fd);
				}
				set <wstring> scrapedSynonyms;
				wstring word = ffd.cFileName;
				int where = (int)word.find('.');
				word.erase(where);
				word.erase(word.begin() + 0, word.begin() + 1);
				int space;
				while ((space = (int)word.find('+')) != wstring::npos)
					word[space] = ' ';
				scrapeThesaurus(word, buffer, scrapedSynonyms);
				//scrapeNewThesaurus();
				vector < set <wstring> > dbSynonyms;
				int synonymType = -1;
				if (isdigit(ffd.cFileName[wcslen(ffd.cFileName) - 1]))
					synonymType = ffd.cFileName[wcslen(ffd.cFileName) - 1] - '0';
				getSynonymsFromDB(mysql, word, dbSynonyms, synonymType);
				bool atLeastOneIdentical = false;
				for (int I = 0; I < dbSynonyms.size(); I++)
				{
					atLeastOneIdentical |= compareWordSets(scrapedSynonyms,dbSynonyms[I]);
				}
				if (dbSynonyms.empty())
					dbEmpty++;
				else if (!atLeastOneIdentical)
				{
					nonMatched++;
					printf("WORD [%d:%d]%S:\n", nonMatched,total,word.c_str());
					printf("scrapedSynonyms:");
					for (set <wstring>::iterator ss = scrapedSynonyms.begin(), ssEnd = scrapedSynonyms.end(); ss != ssEnd; ss++)
						printf("%S;", ss->c_str());
					printf("\ndbSynonyms:");
					for (int I = 0; I < dbSynonyms.size(); I++)
					{
						if (dbSynonyms.size()>1)
							printf("\n[%d]:", I);
						for (set <wstring>::iterator ss = dbSynonyms[I].begin(), ssEnd = dbSynonyms[I].end(); ss != ssEnd; ss++)
							printf("%S;", ss->c_str());
					}
					printf("\n");
				}
				free(buffer);
			}
		} while (FindNextFile(hFind, &ffd) != 0);
		printf("dbempty=%d nonMatched=%d total=%d\n", dbEmpty, nonMatched, total);
		FindClose(hFind);
	}
}
