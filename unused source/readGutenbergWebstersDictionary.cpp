class WebstersEntry
{
public:
	wstring word, info;
	vector<wstring> wordClasses;
	vector <wstring> definitions;
	wstring getLine(wstring &buffer, int &where, int bufferLen)
	{
		wstring line;
		for (; buffer[where] != '\r' && where < bufferLen; where++)
			line += buffer[where];
		where++;
		if (buffer[where] == '\n')
			where++;
		return line;
	}

	wstring getUntilBlankLine(wstring &buffer, int &where, int bufferLen, bool &firstLineEntirelyUpperCase)
	{
		wstring paragraph;
		while (true)
		{
			wstring line = getLine(buffer, where, bufferLen);

			if (paragraph.empty())
			{
				bool oneLower = false;
				for (int where = 0; where < line.length() && line[where] != '\r'; where++)
					if (oneLower = iswlower(line[where]))
						break;
				firstLineEntirelyUpperCase = !oneLower;
			}

			if (line.empty())
				return paragraph;
			paragraph += line + L" ";
		}
	}

	bool isAllUpper(wstring line)
	{
		for (wchar_t ch : line)
			if (iswlower(ch))
				return false;
		return true;
	}

	int getWebsterEntryWord(wstring &buffer, int &where, wstring &word, int bufferLen)
	{
		wstring line;
		while ((line = getLine(buffer, where, bufferLen)).empty() && where < bufferLen);
		if (!line.empty() && isAllUpper(line))
		{
			word = line;
			return where;
		}
		return -1;
	}

	int getWebsterEntryWordClass(wstring &buffer, int &where, vector<wstring> &wordClasses, int bufferLen)
	{
		bool firstLineEntirelyUpperCase;
		wstring paragraph = getUntilBlankLine(buffer, where, bufferLen, firstLineEntirelyUpperCase);
		int whereComma = paragraph.find(',');
		int whereEtymology = paragraph.find(L"Etym:");
		if (whereEtymology != wstring::npos)
			paragraph.erase(whereEtymology);
		if (whereComma >= 0)
		{
			map<wstring, wstring> websterMapping = {
				{ L" vb.n.",L"verbal noun"},
				{ L" vb. n.",L"verbal noun"},
				{ L" n.",L"noun"},
				{ L" v. t.",L"transitive verb"},
				{ L" v.t.",L"transitive verb"},
				{ L" v.i.",L"intransitive verb"},
				{ L" a.",L"adjective"},
				{ L" adj.",L"adjective"},
				{ L" adv.",L"adverb"},
				{ L" p.p.",L"past participle"},
				{ L" p.a.",L"participial adjective"},
				{ L" p.pr.",L"present participle"},
				{ L" p.ple.",L"present participle"},
				{ L" p.",L"participle"},
				{ L" prep.",L"preposition"},
				{ L" imp.",L"imperfect"},
				{ L" inf.",L"infinitive"},
				{ L" interj.",L"interjection"},
				{ L" acc.",L"accusative"},
			};
			for (auto const&[webster, form] : websterMapping)
				if (paragraph.find(webster, whereComma) != wstring::npos)
				{
					wordClasses.push_back(form);
					break;
				}
		}
		return wordClasses.size();
	}

	bool getWebsterEntryStartsWith(wstring &buffer, wstring searchStr)
	{
		return buffer.substr(0, searchStr.length()) == searchStr;
	}

	bool getWebsterEntryStartsWithNum(wstring &buffer, int &num)
	{
		int I;
		for (I = 0; I < buffer.length() && iswdigit(buffer[I]); I++);
		if (I > 0 && buffer[I] == '.')
		{
			num = _wtoi(buffer.c_str());
			return true;
		}
		return false;
	}

	bool getWebsterEntryWordInfo(wstring &buffer, int &where, vector <wstring> &definitions, int bufferLen)
	{
		int saveWhere = where;
		bool firstLineEntirelyUpperCase;
		wstring paragraph = getUntilBlankLine(buffer, where, bufferLen, firstLineEntirelyUpperCase);
		int num = 1;
		if (getWebsterEntryStartsWith(paragraph, L"Defn:"))
		{
			wprintf(L"DEFN %d:%s\n", where, paragraph.c_str());
			definitions.push_back(paragraph);
			return true;
		}
		if (getWebsterEntryStartsWith(paragraph, L"Note:"))
		{
			wprintf(L"NOTE %d:%s\n", where, paragraph.c_str());
			return true;
		}
		if (getWebsterEntryStartsWith(paragraph, L"Syn."))
		{
			wprintf(L"SYN %d:%s\n", where, paragraph.c_str());
			//definitions.push_back(paragraph);
			return true;
		}
		if (getWebsterEntryStartsWithNum(paragraph, num))
		{
			wprintf(L"NUM %d:%s\n", where, paragraph.c_str());
			definitions.push_back(paragraph);
			return true;
		}
		if (!firstLineEntirelyUpperCase)
		{
			wprintf(L"ADD %d:%s\n", where, paragraph.c_str());
			return true;
		}
		//if (paragraph.find(L"Etym:") != wstring::npos)
		//{
		//	wprintf(L"ETYM %d:%s\n", where, paragraph.c_str());
		//	return true;
		//}
		where = saveWhere;
		return false;
	}

	WebstersEntry(wstring &buffer, int &where, int bufferLen, bool &reachedEnd)
	{
		// search for a line with all caps
		where = getWebsterEntryWord(buffer, where, word, bufferLen);
		if (reachedEnd = where < 0) return;
		getWebsterEntryWordClass(buffer, where, wordClasses, bufferLen);
		if (reachedEnd = where < 0) return;
		wprintf(L"\n*** %s %s\n", word.c_str(), (wordClasses.size() > 0) ? wordClasses[0].c_str() : L"X");
		while (getWebsterEntryWordInfo(buffer, where, definitions, bufferLen));
		reachedEnd = false;
	}
};

#define WebsterStart L"Produced by Graham Lawrence"
int readUnabridgedGutenbergWebstersDictionary()
{
	nounForm = 1;
	verbForm = 2;
	adjectiveForm = 3;
	adverbForm = 4;
	prepositionForm = 5;

	int bookhandle = _wopen(L"J:\\caches\\texts\\Webster, Noah\\Unabridged Dictionary.txt", O_RDWR | O_BINARY);
	if (bookhandle < 0)
	{
		lplog(LOG_ERROR, L"ERROR:Unable to open dictionary - %S", _sys_errlist[errno]);
		return -1;
	}
	int bufferLen;
	char *dictionaryBuffer = (char *)tmalloc((size_t)(bufferLen = _filelength(bookhandle)) + 10);
	bufferLen = _read(bookhandle, dictionaryBuffer, (unsigned int)bufferLen);
	_close(bookhandle);
	if (bufferLen < 0 || bufferLen == 0)
		return -1;
	dictionaryBuffer[bufferLen + 1] = 0;
	wstring buffer;
	mTW(dictionaryBuffer, buffer);
	int where = buffer.find(WebsterStart);
	if (where == wstring::npos)
		return -1;
	where += wcslen(WebsterStart);
	int numWordEntries = 0;
	bool reachedEnd = false;
	vector <WebstersEntry> entries;
	while (!reachedEnd)
	{
		entries.push_back(WebstersEntry(buffer, where, bufferLen, reachedEnd));
		numWordEntries++;
	}
	map <wstring, int> formCounts;
	for (auto we : entries)
	{
		if (we.wordClasses.size() > 0)
			formCounts[we.wordClasses[0]]++;
		else
			formCounts[L"unknown"]++;
	}
	wprintf(L"Webster Entries found:%d\n", numWordEntries);
	for (auto const&[form, count] : formCounts)
		wprintf(L"Form %s:%d\n", form.c_str(), count);
	return 0;
}
