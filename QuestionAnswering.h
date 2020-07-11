#pragma once
class cQuestionAnswering
{
public:
	class searchSource
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
		QTAFlag = 1 << 8
	};

	int processQuestionSource(Source *questionSource, bool parseOnly, bool useParallelQuery);
	int processPath(Source *parentSource, const wchar_t *path, Source *&source, Source::sourceTypeEnum st, int sourceConfidence, bool parseOnly);
	bool matchSourcePositions(Source *parentSource, int parentWhere, Source *childSource, int childWhere, bool &namedNoMatch, bool &synonym, bool parentInQuestionObject, int &semanticMismatch, int &adjectivalMatch, sTrace &debugTrace);

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
			wstring matchInfo;
			Source *source;
			cSpaceRelation* sri;
			int equivalenceClass;
			int ws, wo, wp; // used to differentiate compound nouns
			int identityWith;
			bool finalAnswer;
			bool subQueryMatch;
			bool subQueryExisted;
			bool identicalWithAnswerFromAnotherSource;
			cAS(wstring _sourceType, Source *_source, int _confidence, int _matchSum, wstring _matchInfo, cSpaceRelation* _sri, int _equivalenceClass, int _ws, int _wo, int _wp)
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
				identityWith = -1;
				finalAnswer = false;
				subQueryMatch = false;
				subQueryExisted = false;
			}
		};
		class SemanticMatchInfo
		{
		public:
			bool synonym;
			int semanticMismatch;
			int confidence;
			SemanticMatchInfo(bool synonym, int semanticMismatch, int confidence)
			{
				this->synonym = synonym;
				this->semanticMismatch = semanticMismatch;
				this->confidence = confidence;
			};
			SemanticMatchInfo()
			{
				this->synonym = false;
				this->semanticMismatch = 0;
				this->confidence = CONFIDENCE_NOMATCH;
			};
		};
		void detectByClausePassive(Source *questionSource, vector <cSpaceRelation>::iterator sri, cSpaceRelation * &ssri);
	bool detectTransitoryAnswer(Source *questionSource, cSpaceRelation* sri, cSpaceRelation * &ssri, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	void initializeTransformations(Source *questionSource, unordered_map <wstring, wstring> &parseVariables);
	bool processPathToPattern(Source *questionSource, const wchar_t *path, Source *&source);
	map <vector <cSpaceRelation>::iterator, vector <cPattern *> > transformationPatternMap;
	Source *transformSource;
	unordered_map <wstring, Source *> sourcesMap;
	unordered_map <wstring, cAnswerConfidence> childCandidateAnswerMap;
	void copySource(Source *toSource, cSpaceRelation *constantQuestionSRI, cPattern *originalQuestionPattern, cPattern *constantQuestionPattern, unordered_map <int, int> &sourceMap, unordered_map <wstring, wstring> &parseVariables);
	int getWhereQuestionTypeObject(Source *questionSource, cSpaceRelation* sri);
	int analyzeQuestionFromSource(Source *questionSource, wchar_t *derivation, wstring childSourceType, Source *childSource, cSpaceRelation * parentSRI, vector < cAS > &answerSRIs, int &maxAnswer, int rejectDuplicatesFrom, bool eraseIfNoAnswers, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int questionTypeCheck(Source *questionSource, wstring derivation, cSpaceRelation* parentSRI, cAS &childCAS, int &semanticMismatch, bool &unableToDoquestionTypeCheck, bool &subQueryNoMatch, vector <cSpaceRelation> &subQueries, int numConsideredParentAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	void verbTenseMatch(Source *questionSource, cSpaceRelation* parentSRI, cAS &childCAS, bool &tenseMismatch);
	int semanticMatch(Source *questionSource, wstring derivation, cSpaceRelation* parentSRI, cAS &childCAS, int &semanticMismatch, bool &subQueryNoMatch, vector <cSpaceRelation> &subQueries, int numConsideredParentAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	int semanticMatchSingle(Source *questionSource, wstring derivation, cSpaceRelation* parentSRI, Source *childSource, int whereChild, int childObject, int &semanticMismatch, bool &subQueryNoMatch,
		vector <cSpaceRelation> &subQueries, int numConsideredParentAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	bool checkIdentical(Source *questionSource, cSpaceRelation* sri, cAS &cas1, cAS &cas2);
	void setWhereChildCandidateAnswer(Source *questionSource, cAS &childCAS, cSpaceRelation* parentSRI);
	void matchAnswersToQuestionType(Source *questionSource, cSpaceRelation*  sri, vector < cAS > &answerSRIs, int maxAnswer, vector <cSpaceRelation> &subQueries,
		vector <int> &uniqueAnswers, vector <int> &uniqueAnswersPopularity, vector <int> &uniqueAnswersConfidence,
		int &highestPopularity, int &lowestConfidence, int &lowestSourceConfidence, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	bool isModifiedGeneric(cAS &sri);
	int printAnswers(cSpaceRelation*  sri, vector < cAS > &answerSRIs, vector <int> &uniqueAnswers, vector <int> &uniqueAnswersPopularity, vector <int> &uniqueAnswersConfidence,
		int highestPopularity, int lowestConfidence, int lowestSourceConfidence);
	int searchWebSearchQueries(Source *questionSource, wchar_t derivation[1024], cSpaceRelation* ssri, unordered_map <int, WikipediaTableCandidateAnswers * > &wikiTableMap,
		vector <cSpaceRelation> &subQueries, vector < cAS > &answerSRIs, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion,
		vector <wstring> &webSearchQueryStrings, bool parseOnly, bool answerPluralSpecification, int &finalAnswer, int &maxAnswer, bool useParallelQuery);
	int matchAnswersOfPreviousQuestion(Source *questionSource, cSpaceRelation *ssri, vector <int> &wherePossibleAnswers);
	int findConstrainedAnswers(Source *questionSource, vector < cAS > &answerSRIs, vector <int> &wherePossibleAnswers);
	void analyzeQuestionFromSpecificSource(Source *questionSource, wstring childSourceType, Source *childSource, cSpaceRelation * parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int rejectDuplicatesFrom, int ws, int wo);
	void enterAnswer(Source *questionSource, wchar_t *derivation, wstring childSourceType, Source *childSource, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs, int &maxAnswer, int rejectDuplicatesFrom, int matchSum,
		wstring matchInfo, vector <cSpaceRelation>::iterator childSRI, int equivalenceClass, int ws, int wo, int wp, int &answersContainedInSource);
	int metaPatternMatch(Source *questionSource, Source *childSource, vector <cSpaceRelation>::iterator childSRI, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int	searchTableForAnswer(Source *questionSource, wchar_t derivation[1024], cSpaceRelation* sri, unordered_map <int, WikipediaTableCandidateAnswers * > &wikiTableMap, vector <cSpaceRelation> &subQueries, vector < cAS > &answerSRIs, int &maxAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	void addTables(Source *questionSource, int whereQuestionType, Source *wikipediaSource, vector < SourceTable > &wikiTables);
	void getPrepositionalPhraseNonMixMatch(Source *questionSource, int whereQuestionContextSuggestion, int &minPrepositionalPhraseNonMixMatch, int &maxPrepositionalPhraseNonMixMatch);
	void analyzeQuestionFromRDFType(Source *questionSource, wchar_t *derivation, int whereQuestionContextSuggestion, cSpaceRelation * parentSRI, cTreeCat *rdfType, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, unordered_map <int, WikipediaTableCandidateAnswers *> &wikiTableMap, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, set <wstring> &wikipediaLinksAlreadyScanned);
	void enhanceWebSearchQueries(vector <wstring> &webSearchQueryStrings, wstring semanticSuggestion);
	void getWebSearchQueries(Source *questionSource, cSpaceRelation* parentSRI, vector <wstring> &objects);
	void accumulateSemanticMaps(Source *questionSource,cSpaceRelation* parentSRI, Source *childSource, bool confidence);
	void accumulateSemanticEntry(Source *questionSource, unsigned int where, set <cObject::cLocation> &principalObjectLocations, set <cObject::cLocation>::iterator &polIndex, bool confidence, cSpaceRelation* parentSRI, cSemanticMap *semanticMap, unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings);
	int	parseSubQueriesParallel(Source *questionSource,Source *childSource, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer, set <wstring> &wikipediaLinksAlreadyScanned);
	bool analyzeRDFTypes(Source *questionSource, vector <cSpaceRelation>::iterator sqi, cSpaceRelation *ssri, wstring derivation,vector < cAS > &answerSRIs, int &maxAnswer, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, unordered_map <int, WikipediaTableCandidateAnswers * > &wikiTableMap,bool rejectEmptyObjects);
	int	matchSubQueries(Source *questionSource, wstring derivation, Source *childSource, int &semanticMismatch, bool &subQueryNoMatch, vector <cSpaceRelation> &subQueries, int whereChildCandidateAnswer, int whereChildCandidateAnswerEnd, int numConsideredParentAnswer, int semMatchValue, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion, bool useParallelQuery);
	bool checkObjectIdentical(Source *source1, Source *source2, int object1, int object2);
	bool checkParticularPartIdentical(Source *source1, Source *source2, int where1, int where2);
	unordered_map<wstring, SemanticMatchInfo> questionGroupMap;
	int checkParentGroup(Source *parentSource, int parentWhere, Source *childSource, int childWhere, int childObject, bool &synonym, int &semanticMismatch);
	void printCAS(wstring logPrefix, cAS &childCAS);
	int spinParses(MYSQL &mysql, vector <searchSource> &accumulatedParseRequests);
	int accumulateParseRequests(cSpaceRelation* parentSRI, int webSitesAskedFor, int index, bool googleSearch, vector <wstring> &webSearchQueryStrings, int &offset, vector <searchSource> &accumulatedParseRequests);
	int analyzeAccumulatedRequests(Source *questionSource,wchar_t *derivation, cSpaceRelation *parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, vector <searchSource> &accumulatedParseRequests,
		cPattern *&mapPatternAnswer,cPattern *&mapPatternQuestion);
	int webSearchForQueryParallel(Source *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
		vector <wstring> &webSearchQueryStrings,int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	int webSearchForQuerySerial(Source *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, bool parseOnly, vector < cAS > &answerSRIs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
		vector <wstring> &webSearchQueryStrings,int &offset, cPattern *&mapPatternAnswer, cPattern *&mapPatternQuestion);
	bool dbSearchMusicBrainzSearchType(Source *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs,
		int firstWhere, wstring firstMatchListType, int secondWhere, wstring secondMatchListType, const wchar_t *matchVerbsList[]);
	bool dbSearchMusicBrainz(Source *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs);
	bool dbSearchForQuery(Source *questionSource, wchar_t *derivation, cSpaceRelation* parentSRI, vector < cAS > &answerSRIs);
	bool matchOwnershipDbMusicBrainzObject(Source *questionSource, wchar_t *derivation, int whereObject);
	bool matchOwnershipDbQuery(Source *questionSource,wchar_t *derivation, cSpaceRelation* parentSRI);
	bool matchOwnershipDbMusicBrainz(Source *questionSource,wchar_t *derivation, cSpaceRelation* parentSRI);
	int processSnippet(Source *questionSource, wstring snippet, wstring object, Source *&source, bool parseOnly);
	int processAbstract(Source *questionSource, cTreeCat *rdfType, Source *&source, bool parseOnly);
	int processWikipedia(Source *questionSource, int principalWhere, Source *&source, vector <wstring> &wikipediaLinks, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool parseOnly, set <wstring> &wikipediaLinksAlreadyScanned);
	bool matchObjectsExact(vector <cObject>::iterator parentObject, vector <cObject>::iterator childObject, bool &namedNoMatch);
	static bool matchObjects(Source *parentSource, vector <cObject>::iterator parentObject, Source *childSource, vector <cObject>::iterator childObject, bool &namedNoMatch, sTrace debugTrace);
	static bool matchChildSourcePosition(Source *parentSource, vector <cObject>::iterator parentObject, Source *childSource, int childWhere, bool &namedNoMatch, sTrace &debugTrace);
	static bool matchTimeObjects(Source *parentSource, int parentWhere, Source *childSource, int childWhere);
	int sriPrepMatch(Source *parentSource, Source *childSource, int parentWhere, int childWhere, int cost);
	int sriVerbMatch(Source *parentSource, Source *childSource, int parentWhere, int childWhere, int cost);
	int sriMatch(Source *questionSource, Source *childSource, int parentWhere, int childWhere, int whereQuestionType, __int64 questionType, bool &totalMatch, int cost);
	int equivalenceClassCheck(Source *questionSource, Source *childSource, vector <cSpaceRelation>::iterator childSRI, cSpaceRelation* parentSRI, int whereChildSpecificObject, int &equivalenceClass, int matchSum);
	int equivalenceClassCheck2(Source *questionSource, Source *childSource, vector <cSpaceRelation>::iterator childSRI, cSpaceRelation* parentSRI, int whereChildSpecificObject, int &equivalenceClass, int matchSum);
	bool rejectPath(const wchar_t *path);
	bool matchParticularAnswer(Source *questionSource, cSpaceRelation *ssri, int whereMatch, int wherePossibleAnswer, set <int> &addWhereQuestionInformationSourceObjects);
	void detectSubQueries(Source *questionSource, vector <cSpaceRelation>::iterator sri, vector <cSpaceRelation> &subQueries);
	bool matchAnswerSourceMatch(Source *questionSource,cSpaceRelation *ssri, int whereMatch, int wherePossibleAnswer, set <int> &addWhereQuestionInformationSourceObjects);
	void clear()
	{
		for (unordered_map <wstring, Source *>::iterator smi = sourcesMap.begin(); smi != sourcesMap.end();)
		{
			Source *source = smi->second;
			source->clearSource();
			delete source;
			sourcesMap.erase(smi);
			smi = sourcesMap.begin();
		}
	}
};