/*
	tableColumn.h - Wikipedia-table model used by question answering to pick a coherent answer column

	Overview:
		A scraped Wikipedia <table> is tokenized into cSource with synthetic TABLE /
		END_COLUMN / END_COLUMN_HEADERS words.  cSourceTable walks those tokens into
		cColumn / cRow / cEntry, then scores each column's RDF-type coherence against
		the table title and the question's type-object.  A "coherent" column is one
		whose cells share a simplified RDF type (or a title synonym) at >= 90%.

	Pipeline position:
		Stage 8.  cQuestionAnswering::addTables() constructs one cSourceTable per
		Words.TABLE in a Wikipedia source; the resulting wikiQuestionTypeObjectAnswers
		feed property-value answers.

	Key data structures / globals:
		- cEntry - one cell (or title/header): [begin, begin+numWords) in the wiki
			source, plus synonym/RDF match flags.
		- cRow - the cells of one row plus per-row title-synonym counters.
		- cColumn - all rows of one column, the accumulatedRDFTypesMap, and
			coherencyPercentage.
		- cSourceTable - columns + columnHeaders + tableTitleEntry for one table.
		- cWikipediaTableCandidateAnswers - wikipediaSource plus the tables kept as
			answers for one question-type object.

	Notes / gotchas:
		- adaptiveWhere is the "object declare" position when the cell is an object,
			otherwise equal to begin.  RDF lookup uses adaptiveWhere+numWords.
		- lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms is overloaded as the
			"preferred cell in this row" flag even when the title did not match
			(setRowPreference writes it).
		- determineColumnRDFTypeCoherency currently returns true on both the
			<90% and >=90% paths (the reject is commented as TEMP DEBUG).
*/
#pragma once
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
	// Sort by frequency descending, then word descending.  Used as the comparator
	// of mostCommonAssociationTypeSet so begin() is the most frequent type.
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
		// Cell covering source positions [b, b+n) whose "object declare" position
		// is o (o==b when the cell is not an object).
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
			coherentTable = false;
		};
		// Empty cell: begin/adaptiveWhere/numWords = -1.
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
			coherentTable = false;
		}
		int begin;
		int adaptiveWhere; // if an object, this is where the object is declared (not necessarily its beginning), if not an object then adaptiveWhere=begin
		int numWords;
		int RDFTypeSimplifiedToWordFoundInTitleSynonyms;
		bool lastWordFoundInTitleSynonyms;
		bool lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms;
		bool tableOfContentsFlag;  // applies only to title entry
		bool coherentTable;  // applies only to title entry
		int queryAssociationsMatched;
		int titleAssociationsMatched;
		vector <int> matchedQuestionObject;
		vector <int> synonymMatchedQuestionObject;
		wstring simplifiedRDFTypes;
		wstring sprint(cSource *source, wstring &buffer);
		void logEntry(int logType, const wchar_t *tableName, int row, int entryIndex, cSource *source);
		void accumulateEntryRDFTypes(cSource *wikipediaSource, wstring tableName, int row, int entry, unordered_set <wstring> &titleSynonyms, unordered_map < wstring, cAssociationType > &accumulatedRDFTypesMap, bool fileCaching);
	};
	class cRow
	{
	public:
		int numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow = 0;
		int numSimplifiedRDFTypesFoundForRow = 0;
		int maxTitleFound = 0;
		int numLastWordsFoundInTitleSynonymsInRow = 0;
		vector <cEntry> entries;
		// One table row.  Copies e and zeroes the per-row synonym counters.
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
	void removeDomainFromAccumulatedRDFTypesMap(const wchar_t *domainAssociations[]);
	bool determineColumnRDFTypeCoherency(cSource *wikipediaSource, cColumn::cEntry titleEntry, unordered_set <wstring> &titleSynonyms, wstring tableName,bool keepMusicDomain, bool keepFilmDomain, bool fileCaching);
	void zeroColumnAccumulatedRDFTypes();
	void accumulateColumnRDFTypes(cSource *wikipediaSource, wstring tableName, unordered_set <wstring> &titleSynonyms, bool keepMusicDomain, bool keepFilmDomain, bool onlyPreferred, bool fileCaching);
	void getMostCommonRDFTypes(const wchar_t * when, wstring tableName);
	int getSumOfAllFullyConfidentRDFTypeFrequencies(cSource *wikipediaSource, int row, int entry, int &maxFrequency, wstring &maxAssociation, bool fileCaching);
	bool testTitlePreference(cSource *wikipediaSource, wstring tableName, unordered_set <wstring> &titleSynonyms, bool fileCaching);
	void setRowPreference(cSource *wikipediaSource, wstring tableName, bool fileCaching);
	// each lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms entry has two values:
	//   the average of the associationValue
	//   the number of values/the size of the 
	int calculateColumnRDFTypeCoherence(cSource *wikipediaSource, cColumn::cEntry titleEntry, wstring tableName, bool fileCaching);
	void logColumn(int logType, const wchar_t * when, wstring tableName);
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

	// Empty table: no source, no columns, columnHeaderMatchTitle = -1 (none).
	cSourceTable()
	{
		columnHeaderMatchTitle = -1;
	}
	cSourceTable(int &where, int whereQuestionTypeObject, cSource *wikipediaSource, cSource *questionSource,bool fileCaching);
	bool getTableFromSource(int I, int whereQuestionTypeObject, cSource *wikipediaSource, cSource *questionSource);
	bool isEntryInvalid(int beginColumn, vector <int> &wikiColumns, cSource *wikipediaSource);
	bool analyzeTitle(unsigned int where, int &numWords, int &numPrepositions, wstring tableName, cSource *wikipediaSource);
};

class cWikipediaTableCandidateAnswers
{
public:
	vector < cSourceTable > wikiQuestionTypeObjectAnswers;
	cSource *wikipediaSource;
	// Shallow pointer to wikipediaSource (not owned) plus a copy of the tables.
	cWikipediaTableCandidateAnswers(cSource *wikipediaSource, vector < cSourceTable > wikiQuestionTypeObjectAnswers)
	{
		this->wikipediaSource = wikipediaSource;
		this->wikiQuestionTypeObjectAnswers = wikiQuestionTypeObjectAnswers;
	}
};
