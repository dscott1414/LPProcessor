class Source;

class Column
{
public:
	class CA
	{
	public:
		wstring word;
		int frequency;
		bool queryObjectMatch;
		bool titleObjectMatch;
		CA(wstring w, int f, bool qom = false, bool tom = false)
		{
			word = w;
			frequency = f;
			queryObjectMatch = qom;
			titleObjectMatch = tom;
		}
	};
	struct associationTypeMapCompare
	{
		bool operator()(CA lhs, CA rhs) const
		{
			if (lhs.frequency == rhs.frequency)
				return lhs.word > rhs.word;
			return lhs.frequency > rhs.frequency;
		}
	};
	class AssociationType
	{
	public:
		int frequency;
		bool queryObjectMatch;
		bool titleObjectMatch;
	};
	class Entry
	{
	public:
		Entry(int b, int o, int n)
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
		Entry()
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
		void logEntry(int logType, const wchar_t *tableName, int row, int entryIndex, Source *source);
		void accumulateEntryRDFTypes(Source *wikipediaSource, set <wstring> &titleSynonyms, unordered_map < wstring, AssociationType > &accumulatedRDFTypesMap);
	};
	class cRow
	{
	public:
		int numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow = 0;
		int numSimplifiedRDFTypesFoundForRow = 0;
		int maxTitleFound = 0;
		int numLastWordsFoundInTitleSynonymsInRow = 0;
		vector <Entry> entries;
		cRow(vector <Entry> &e)
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
	unordered_map < wstring, AssociationType > accumulatedRDFTypesMap;
	set < CA, associationTypeMapCompare > mostCommonAssociationTypeSet;
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
	Column();
	void removeDomainFromAccumulatedRDFTypesMap(wchar_t * domainAssociations[]);
	bool determineColumnRDFTypeCoherency(Source *wikipediaSource, Column::Entry titleEntry, set <wstring> &titleSynonyms, wstring tableName,bool keepMusicDomain, bool keepFilmDomain);
	void zeroColumnAccumulatedRDFTypes();
	void accumulateColumnRDFTypes(Source *wikipediaSource, set <wstring> &titleSynonyms, bool keepMusicDomain, bool keepFilmDomain, bool onlyPreferred);
	void getMostCommonRDFTypes(wchar_t *when, wstring tableName);
	int getSumOfAllFullyConfidentRDFTypeFrequencies(Source *wikipediaSource, int row, int entry, int &maxFrequency, wstring &maxAssociation);
	bool testTitlePreference(Source *wikipediaSource, wstring tableName, set <wstring> &titleSynonyms);
	void setRowPreference(Source *wikipediaSource, wstring tableName);
	// each lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms entry has two values:
	//   the average of the associationValue
	//   the number of values/the size of the 
	int calculateColumnRDFTypeCoherence(Source *wikipediaSource, Column::Entry titleEntry, wstring tableName);
	void logColumn(int logType, wchar_t *when, wstring tableName);
	static Column::Entry scanColumnEntry(int whereQuestionType, Source *wikipediaSource, Source *questionSource, int &I, bool &matchFound, wstring tableName);
};

class SourceTable
{
public:
	vector <Column> columns;
	vector <Column::Entry> columnHeaders;
	Column::Entry tableTitleEntry;
	wstring num;
	Source *source;
	int columnHeaderMatchTitle;

	SourceTable()
	{
		columnHeaderMatchTitle = -1;
	}
	SourceTable(int &where, int whereQuestionTypeObject, Source *wikipediaSource, Source *questionSource);
	bool getTableFromSource(int I, int whereQuestionTypeObject, Source *wikipediaSource, Source *questionSource);
	bool isEntryInvalid(int beginColumn, vector <int> &wikiColumns, Source *wikipediaSource);
	bool analyzeTitle(unsigned int where, int &numWords, int &numPrepositions, wstring tableName, Source *wikipediaSource);
};

class WikipediaTableCandidateAnswers
{
public:
	vector < SourceTable > wikiQuestionTypeObjectAnswers;
	Source *wikipediaSource;
	WikipediaTableCandidateAnswers(Source *wikipediaSource, vector < SourceTable > wikiQuestionTypeObjectAnswers)
	{
		this->wikipediaSource = wikipediaSource;
		this->wikiQuestionTypeObjectAnswers = wikiQuestionTypeObjectAnswers;
	}
};
