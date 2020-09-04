#pragma once
class cQuestionAnswering
{
public:
	class cSearchSource
	{
	public:
		bool isSnippet; // snippet
		bool skipFullPath; // skip full path if snippet has good answer
		bool hasCorrespondingSnippet; // is a full article with a corresponding snippet in dbPedia
		int fullPathIndex; // location of corresponding fullPath
		wstring fullWebPath;
		wstring pathInCache;
	};

	enum qtf {
		unknownQTFlag = 1, whichQTFlag = 2, whereQTFlag = 3, whatQTFlag = 4, whoseQTFlag = 5, howQTFlag = 6, whenQTFlag = 7, whomQTFlag = 8, whyQTFlag = 9, wikiBusinessQTFlag = 10, wikiWorkQTFlag = 11, typeQTMask = (1 << 4) - 1,
		referencingObjectQTFlag = 1 << 4, subjectQTFlag = 2 << 4, objectQTFlag = 3 << 4, secondaryObjectQTFlag = 4 << 4, prepObjectQTFlag = 5 << 4,
		// answer must be an adjective - whose book?
		QTAFlag = 1 << 8
	};

	int processQuestionSource(cSource *questionSource, bool parseOnly, bool useParallelQuery);
	int processPath(cSource *parentSource, const wchar_t *path, cSource *&source, cSource::sourceTypeEnum st, int sourceConfidence, bool parseOnly);
	bool matchAllSourcePositions(cSource* parentSource, int parentWhere, cSource* childSource, int childWhere, bool& namedNoMatch, bool& synonym, bool parentInQuestionObject, int& semanticMismatch, int& adjectivalMatch, sTrace& debugTrace);
	bool matchSourcePositions(cSource* parentSource, int parentWhere, cSource* childSource, int childWhere, bool& namedNoMatch, bool& synonym, bool parentInQuestionObject, int& semanticMismatch, int& adjectivalMatch, sTrace& debugTrace);
	int checkParticularPartQuestionTypeCheck(cSource* questionSource, __int64 questionType, int childWhere, int childObject, int& semanticMismatch);

	private:
		class cAnswerConfidence
		{
		public:
			int anySemanticMismatch;
			bool subQueryNoMatch;
			int confidence;
		};
		class cAS
		{
		public:
			wstring sourceType;
			wstring rejectAnswer;
			int confidence;
			int matchSum;
			int matchSumWithConfidenceAndNumIdenticalAnswersScored;
			wstring matchInfo;
			cSource *source;
			cSpaceRelation* sri;
			int equivalenceClass;
			int ws, wo, wp; // used to differentiate compound nouns
			int whereChildCandidateAnswer;
			bool finalAnswer;
			bool subQueryMatch;
			bool subQueryExisted;
			int numIdenticalAnswers;
			bool fromTable;
			bool fromWikipediaInfoBox;
			wstring tableNum;
			wstring tableName;
			int columnIndex;
			int rowIndex;
			int entryIndex;
			cColumn::cEntry entry;
			cAS(wstring _sourceType, cSource *_source, int _confidence, int _matchSum, wstring _matchInfo, cSpaceRelation* _sri, int _equivalenceClass, int _ws, int _wo, int _wp, bool _fromWikipediaInfoBox, bool _fromTable, wstring _tableNum, wstring _tableName, int _columnIndex, int _rowIndex, int _entryIndex, cColumn::cEntry *_entry)
			{
				sourceType = _sourceType;
				source = _source;
				confidence = _confidence;
				matchSum = _matchSum;
				matchInfo = _matchInfo;
				sri = _sri;
				equivalenceClass = _equivalenceClass;
				ws = _ws;
				wo = _wo;
				wp = _wp;
				fromWikipediaInfoBox = _fromWikipediaInfoBox;
				fromTable = _fromTable;
				tableNum = _tableNum;
				tableName = _tableName;
				columnIndex = _columnIndex;
				rowIndex = _rowIndex;
				entryIndex = _entryIndex;
				if (_entry)
					entry = *_entry;
				finalAnswer = false;
				subQueryMatch = false;
				subQueryExisted = false;
				numIdenticalAnswers = 0;
				whereChildCandidateAnswer = 0;
			}
		};
		class cSemanticMatchInfo
		{
		public:
			bool synonym;
			int semanticMismatch;
			int confidence;
			cSemanticMatchInfo(bool synonym, int semanticMismatch, int confidence)
			{
				this->synonym = synonym;
				this->semanticMismatch = semanticMismatch;
				this->confidence = confidence;
			};
			cSemanticMatchInfo()
			{
				this->synonym = false;
				this->semanticMismatch = 0;
				this->confidence = CONFIDENCE_NOMATCH;
			};
		};
		void eraseSourcesMap();
	bool isQuestionPassive(cSource *questionSource, vector <cSpaceRelation>::iterator sri, cSpaceRelation * &ssri);
	bool detectTransitoryAnswer(cSource *questionSource, cSpaceRelation* sri, cSpaceRelation * &ssri, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	void initializeTransformations(cSource *questionSource, unordered_map <wstring, wstring> &parseVariables);
	bool processPathToPattern(cSource *questionSource, const wchar_t *path, cSource *&source);
	map <vector <cSpaceRelation>::iterator, vector <cPattern *> > transformationPatternMap;
	cSource *transformSource;
	unordered_map <wstring, cSource *> sourcesMap;
	unordered_map <wstring, cAnswerConfidence> childCandidateAnswerMap;
	void copySource(cSource *toSource, cSpaceRelation *constantQuestionSRI, cPattern *originalQuestionPattern, cPattern *constantQuestionPattern, unordered_map <int, int> &sourceMap, unordered_map <wstring, wstring> &parseVariables);
	int getWhereQuestionTypeObject(cSource *questionSource, cSpaceRelation* sri);
	int analyzeQuestionFromSource(cSource *questionSource, wchar_t *derivation, wstring childSourceType, cSource *childSource, cSpaceRelation * parentSRI, vector < cAS > &answerSRIs, int &maxAnswer, bool eraseIfNoAnswers, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int questionTypeCheck(cSource *questionSource, wstring derivation, cSpaceRelation* parentSRI, cAS &childCAS, int &semanticMismatch, bool &unableToDoquestionTypeCheck);
	int verbTenseMatch(cSource *questionSource, cSpaceRelation* parentSRI, cAS &childCAS);
	int semanticMatch(cSource *questionSource, wstring derivation, cSpaceRelation* parentSRI, cAS &childCAS, int &semanticMismatch);
	int semanticMatchSingle(cSource *questionSource, wstring derivation, cSpaceRelation* parentSRI, cSource *childSource, int whereChild, int childObject, int &semanticMismatch, bool &subQueryNoMatch,
		vector <cSpaceRelation> &subQueries, int numConsideredParentAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	bool checkIdentical(cSource *questionSource, cSpaceRelation* sri, cAS &cas1, cAS &cas2);
	void setWhereChildCandidateAnswer(cSource *questionSource, cAS &childCAS, cSpaceRelation* parentSRI);
	bool enterAnswerAccumulatingIdenticalAnswers(cSource *questionSource, cSpaceRelation *sri, cAS candidateAnswer, int &maxAnswer, vector < cAS > &answerSRIs);
	int  determineBestAnswers(cSource *questionSource, cSpaceRelation*  sri,vector < cAS > &answerSRIs,int maxAnswer,vector <cSpaceRelation> &subQueries,cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion,bool useParallelQuery);
	bool isModifiedGeneric(cAS &sri);
	int printAnswers(cSpaceRelation*  sri, vector < cAS > &answerSRIs);
	int searchWebSearchQueries(cSource *questionSource, wchar_t derivation[1024], cSpaceRelation* ssri,vector <cSpaceRelation> &subQueries,
		vector < cAS > &answerSRIs, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion,vector <wstring> &webSearchQueryStrings,
		bool parseOnly, int &finalAnswer, int &maxAnswer, bool useParallelQuery, int &trySearchIndex, bool useGoogleSearch,bool &lastResultPage);
	int matchAnswersOfPreviousQuestion(cSource *questionSource, cSpaceRelation *ssri, vector <int> &wherePossibleAnswers);
	int findConstrainedAnswers(cSource *questionSource, vector < cAS > &answerSRIs, vector <int> &wherePossibleAnswers);
	int metaPatternMatch(cSource *questionSource, cSource *childSource, vector <cSpaceRelation>::iterator childSRI, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int	searchTableForAnswer(cSource *questionSource, wchar_t derivation[1024], cSpaceRelation* sri, unordered_map <int, cWikipediaTableCandidateAnswers * > &wikiTableMap, vector <cSpaceRelation> &subQueries, vector < cAS > &answerSRIs, int &maxAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	void addTables(cSource *questionSource, int whereQuestionType, cSource *wikipediaSource, vector < cSourceTable > &wikiTables);
	void analyzeQuestionThroughAbstractAndWikipediaFromRDFType(cSource *questionSource, wchar_t *derivation, int whereQuestionContextSuggestion, cSpaceRelation * parentSRI, cTreeCat *rdfType, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, unordered_map <int, cWikipediaTableCandidateAnswers *> &wikiTableMap, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, set <wstring> &wikipediaLinksAlreadyScanned);
	void enhanceWebSearchQueries(vector <wstring> &webSearchQueryStrings, wstring semanticSuggestion);
	void getWebSearchQueries(cSource *questionSource, cSpaceRelation* parentSRI, vector <wstring> &objects);
	void accumulateProximityMaps(cSource *questionSource,cSpaceRelation* parentSRI, cSource *childSource, bool confidence);
	bool processChildObjectIntoString(cSource *childSource, int childObject, unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings, wstring &childObjectString);
	void recordDistanceIntoProximityMap(cSource *childSource, unsigned int childSourceIndex, set <cObject::cLocation> &questionObjectMatchInChildSourceLocations,
		set <cObject::cLocation>::iterator &questionObjectMatchIndex, bool confidence, unordered_map <wstring, cProximityMap::cProximityEntry>::iterator closestObjectIterator);
	void accumulateProximityEntry(cSource *questionSource, unsigned int where, set <cObject::cLocation> &principalObjectLocations, set <cObject::cLocation>::iterator &polIndex, bool confidence, cSpaceRelation* parentSRI, cProximityMap *semanticMap, unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings);
	int	parseSubQueriesParallel(cSource *questionSource,cSource *childSource, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer, set <wstring> &wikipediaLinksAlreadyScanned);
	bool analyzeRDFTypeBirthDate(cSource* questionSource, cSpaceRelation* ssri, wstring derivation, vector < cAS >& answerSRIs, int& maxAnswer, wstring birthDate);
	bool analyzeRDFTypes(cSource *questionSource, vector <cSpaceRelation>::iterator sqi, cSpaceRelation *ssri, wstring derivation,vector < cAS > &answerSRIs, int &maxAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, unordered_map <int, cWikipediaTableCandidateAnswers * > &wikiTableMap,bool rejectEmptyObjects);
	int	matchSubQueries(cSource *questionSource, wstring derivation, cSource *childSource, int &semanticMismatch, bool &subQueryNoMatch, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer, int whereChildCandidateAnswerEnd, int numConsideredParentAnswer, int semMatchValue, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	bool checkObjectIdentical(cSource *source1, cSource *source2, int object1, int object2);
	bool checkParticularPartIdentical(cSource *source1, cSource *source2, int where1, int where2);
	unordered_map<wstring, cSemanticMatchInfo> questionGroupMap;
	int checkParentGroup(cSource *parentSource, int parentWhere, cSource *childSource, int childWhere, int childObject, bool &synonym, int &semanticMismatch);
	int spinParses(MYSQL &mysql, vector <cSearchSource> &accumulatedParseRequests);
	int accumulateParseRequests(cSpaceRelation* parentSRI, int webSitesAskedFor, int index, bool googleSearch, vector <wstring> &webSearchQueryStrings, int &offset, vector <cSearchSource> &accumulatedParseRequests);
	int analyzeAccumulatedRequests(cSource *questionSource,wchar_t *derivation, cSpaceRelation *parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, vector <cSearchSource> &accumulatedParseRequests,
		cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	int webSearchForQueryParallel(cSource *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
		vector <wstring> &webSearchQueryStrings,int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int webSearchForQuerySerial(cSource *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
		vector <wstring> &webSearchQueryStrings,int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	bool dbSearchMusicBrainzSearchType(cSource *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs,
		int firstWhere, wstring firstMatchListType, int secondWhere, wstring secondMatchListType, set <wstring> &matchVerbsList);
	bool dbSearchMusicBrainz(cSource *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs);
	bool dbSearchForQuery(cSource *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs);
	bool matchOwnershipDbMusicBrainzObject(cSource *questionSource, wchar_t *derivation, int whereObject, vector <mbInfoReleaseType> &mbs);
	bool matchOwnershipDbQuery(cSource *questionSource,wchar_t *derivation, cSpaceRelation* parentSRI);
	bool matchOwnershipDbMusicBrainz(cSource *questionSource,wchar_t *derivation, cSpaceRelation* parentSRI);
	int processSnippet(cSource *questionSource, wstring snippet, wstring object, cSource *&source, bool parseOnly);
	int processAbstract(cSource *questionSource, cTreeCat *rdfType, cSource *&source, bool parseOnly);
	int processWikipedia(cSource *questionSource, int principalWhere, cSource *&source, vector <wstring> &wikipediaLinks, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool parseOnly, set <wstring> &wikipediaLinksAlreadyScanned, bool removePrecedingUncapitalizedWordsFromProperNouns);
	bool matchObjectsExactByName(vector <cObject>::iterator parentObject, vector <cObject>::iterator childObject, bool &namedNoMatch);
	static bool matchObjectsByName(cSource *parentSource, vector <cObject>::iterator parentObject, cSource *childSource, vector <cObject>::iterator childObject, bool &namedNoMatch, sTrace debugTrace);
	static bool matchChildSourcePositionByName(cSource *parentSource, vector <cObject>::iterator parentObject, cSource *childSource, int childWhere, bool &namedNoMatch, sTrace &debugTrace);
	static bool matchTimeObjects(cSource *parentSource, int parentWhere, cSource *childSource, int childWhere);
	int sriPrepMatch(cSource *parentSource, cSource *childSource, int parentWhere, int childWhere, int cost);
	int sriVerbMatch(cSource *parentSource, cSource *childSource, int parentWhere, int childWhere, wstring &matchInfoDetailVerb, wstring verbTypeMatch,int cost);
	int sriMatch(cSource *questionSource, cSource *childSource, int parentWhere, int childWhere, int whereQuestionType, __int64 questionType, bool &totalMatch, wstring &matchInfoDetail, int cost, bool subQuery);
	int equivalenceClassCheck(cSource *questionSource, cSource *childSource, vector <cSpaceRelation>::iterator childSRI, cSpaceRelation* parentSRI, int whereChildSpecificObject, int &equivalenceClass, int matchSum);
	int equivalenceClassCheck2(cSource *questionSource, cSource *childSource, vector <cSpaceRelation>::iterator childSRI, cSpaceRelation* parentSRI, int whereChildSpecificObject, int &equivalenceClass, int matchSum);
	bool rejectPath(const wchar_t *path);
	bool matchParticularAnswer(cSource *questionSource, cSpaceRelation *ssri, int whereMatch, int wherePossibleAnswer, set <int> &addWhereQuestionInformationSourceObjects);
	void detectSubQueries(cSource *questionSource, vector <cSpaceRelation>::iterator sri, vector <cSpaceRelation> &subQueries);
	bool matchAnswerSourceMatch(cSource *questionSource,cSpaceRelation *ssri, int whereMatch, int wherePossibleAnswer, set <int> &addWhereQuestionInformationSourceObjects);
	static bool fileCaching;
	void clear()
	{
		for (unordered_map <wstring, cSource *>::iterator smi = sourcesMap.begin(); smi != sourcesMap.end();)
		{
			cSource *source = smi->second;
			source->clearSource();
			delete source;
			sourcesMap.erase(smi);
			smi = sourcesMap.begin();
		}
	}
};