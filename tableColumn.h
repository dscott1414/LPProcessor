class cSource;

class cColumn
{
public:
	class cWordFrequencyMatch
	{
	public:
		wstring word;
		int frequency;
		bool queryObjectMatch;
		bool titleObjectMatch;
		cWordFrequencyMatch(wstring w, int f, bool qom = false, bool tom = false)
		{
			word = w;
			frequency = f;
			queryObjectMatch = qom;
			titleObjectMatch = tom;
		}
	};
	struct associationTypeMapCompare
	{
		bool operator()(cWordFrequencyMatch lhs, cWordFrequencyMatch rhs) const
		{
			if (lhs.frequency == rhs.frequency)
				return lhs.word > rhs.word;
			return lhs.frequency > rhs.frequency;
		}
	};
	class cAssociationType
	{
	public:
		int frequency;
		bool queryObjectMatch;
		bool titleObjectMatch;
	};
	class cEntry
	{
	public:
		cEntry(int b, int o, int n)
		{
			begin = b;
			adaptiveWhere = o;
			numWords = n;
			RDFTypeSimplifiedToWordFoundInTitleSynonyms = 0;
			lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = false;
			lastWordFoundInTitleSynonyms = false;
			queryAssociationsMatched = 0;
			titleAssociationsMatched = 0;
			tableOfContentsFlag = false;
		};
		cEntry()
		{
			begin = -1;
			adaptiveWhere = -1;
			numWords = -1;
			RDFTypeSimplifiedToWordFoundInTitleSynonyms = 0;
			lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = false;
			lastWordFoundInTitleSynonyms = false;
			queryAssociationsMatched = 0;
			titleAssociationsMatched = 0;
			tableOfContentsFlag = false;
		}
		int begin;
		int adaptiveWhere; // if an object, this is where the object is declared (not necessarily its beginning), if not an object then adaptiveWhere=begin
		int numWords;
		int RDFTypeSimplifiedToWordFoundInTitleSynonyms;
		bool lastWordFoundInTitleSynonyms;
		bool lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms;
		bool tableOfContentsFlag; 
		int queryAssociationsMatched;
		int titleAssociationsMatched;
		vector <int> matchedQuestionObject;
		vector <int> synonymMatchedQuestionObject;
		wstring simplifiedRDFTypes;
		void logEntry(int logType, const wchar_t *tableName, int row, int entryIndex, cSource *source);
		void accumulateEntryRDFTypes(cSource *wikipediaSource, wstring tableName, int row, int entry, set <wstring> &titleSynonyms, unordered_map < wstring, cAssociationType > &accumulatedRDFTypesMap);
	};
	class cRow
	{
	public:
		int numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow = 0;
		int numSimplifiedRDFTypesFoundForRow = 0;
		int maxTitleFound = 0;
		int numLastWordsFoundInTitleSynonymsInRow = 0;
		vector <cEntry> entries;
		cRow(vector <cEntry> &e)
		{
			entries = e;
			numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow = 0;
			numSimplifiedRDFTypesFoundForRow = 0;
			maxTitleFound = 0;
			numLastWordsFoundInTitleSynonymsInRow = 0;
		}
	};
	vector <cRow > rows; // there are multiple entries for each row
	int invalidEntries;
	int emptyEntries;
	unordered_map < wstring, cAssociationType > accumulatedRDFTypesMap;
	set < cWordFrequencyMatch, associationTypeMapCompare > mostCommonAssociationTypeSet;
	int numDefinite;
	int numNumerical;
	int numPunctuation;
	int numMaxEntries;
	int gMaxFrequency;
	int mostCommonRatio;
	bool matchedHeader;
	int queryAssociationsMatched;
	int titleAssociationsMatched;
	int numRowsWhereLastWordOrSimplifiedRDFTypesFoundInTitleSynonyms;
	int numRowsWhereSimplifiedRDFTypesFound;
	int numCommonObjectAssociations;
	int coherencyPercentage;
	bool invalidColumn;
	cColumn();
	void removeDomainFromAccumulatedRDFTypesMap(wchar_t * domainAssociations[]);
	bool determineColumnRDFTypeCoherency(cSource *wikipediaSource, cColumn::cEntry titleEntry, set <wstring> &titleSynonyms, wstring tableName,bool keepMusicDomain, bool keepFilmDomain);
	void zeroColumnAccumulatedRDFTypes();
	void accumulateColumnRDFTypes(cSource *wikipediaSource, wstring tableName, set <wstring> &titleSynonyms, bool keepMusicDomain, bool keepFilmDomain, bool onlyPreferred);
	void getMostCommonRDFTypes(wchar_t *when, wstring tableName);
	int getSumOfAllFullyConfidentRDFTypeFrequencies(cSource *wikipediaSource, int row, int entry, int &maxFrequency, wstring &maxAssociation);
	bool testTitlePreference(cSource *wikipediaSource, wstring tableName, set <wstring> &titleSynonyms);
	void setRowPreference(cSource *wikipediaSource, wstring tableName);
	// each lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms entry has two values:
	//   the average of the associationValue
	//   the number of values/the size of the 
	int calculateColumnRDFTypeCoherence(cSource *wikipediaSource, cColumn::cEntry titleEntry, wstring tableName);
	void logColumn(int logType, wchar_t *when, wstring tableName);
	static cColumn::cEntry scanColumnEntry(int whereQuestionType, cSource *wikipediaSource, cSource *questionSource, int &I, bool &matchFound, wstring tableName);
};

class cSourceTable
{
public:
	vector <cColumn> columns;
	vector <cColumn::cEntry> columnHeaders;
	cColumn::cEntry tableTitleEntry;
	wstring num;
	cSource *source;
	int columnHeaderMatchTitle;

	cSourceTable()
	{
		columnHeaderMatchTitle = -1;
	}
	cSourceTable(int &where, int whereQuestionTypeObject, cSource *wikipediaSource, cSource *questionSource);
	bool getTableFromSource(int I, int whereQuestionTypeObject, cSource *wikipediaSource, cSource *questionSource);
	bool isEntryInvalid(int beginColumn, vector <int> &wikiColumns, cSource *wikipediaSource);
	bool analyzeTitle(unsigned int where, int &numWords, int &numPrepositions, wstring tableName, cSource *wikipediaSource);
};

class cWikipediaTableCandidateAnswers
{
public:
	vector < cSourceTable > wikiQuestionTypeObjectAnswers;
	cSource *wikipediaSource;
	cWikipediaTableCandidateAnswers(cSource *wikipediaSource, vector < cSourceTable > wikiQuestionTypeObjectAnswers)
	{
		this->wikipediaSource = wikipediaSource;
		this->wikiQuestionTypeObjectAnswers = wikiQuestionTypeObjectAnswers;
	}
};
