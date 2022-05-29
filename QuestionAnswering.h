#pragma once
class cQuestionAnswering
{
public:
	class cTrackDescendantAnswers
	{
	public:
		cTrackDescendantAnswers(int ao,wstring aa,int ln,bool da):inputAnswerObject(ao), ancestorAnswersTrackingString(aa), link(ln), destinationAnswer(da)
		{
		}
		int inputAnswerObject; // this is an object relative to the questionSource
		wstring ancestorAnswersTrackingString;
		int link;
		bool destinationAnswer;
		set <int> wherePossibleAnswers; // only filled if this made it to the destination pattern.  This is filled with source positions, relative to the questionSource
	};

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

	class cAS
	{
	public:
		wstring sourceType;
		wstring rejectAnswer;
		int confidence;
		int matchSum;
		int matchSumWithConfidenceAndNumIdenticalAnswersScored;
		wstring matchInfo;
		cSource* source;
		cSyntacticRelationGroup* srg;
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
		int object; // used in cases where multiple objects resulted from the same query.  An answer must only be one answer, not multiple.  
		cColumn::cEntry entry;
		cAS(wstring _sourceType, cSource* _source, int _confidence, int _matchSum, wstring _matchInfo, cSyntacticRelationGroup* _sri, int _equivalenceClass, int _ws, int _wo, int _wp, bool _fromWikipediaInfoBox, bool _fromTable, wstring _tableNum, wstring _tableName, int _columnIndex, int _rowIndex, int _entryIndex, cColumn::cEntry* _entry)
		{
			sourceType = _sourceType;
			source = _source;
			confidence = _confidence;
			matchSum = _matchSum;
			matchInfo = _matchInfo;
			srg = _sri;
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
			object = -1;
		}
	};
	enum qtf {
		unknownQTFlag = 1, whichQTFlag = 2, whereQTFlag = 3, whatQTFlag = 4, whoseQTFlag = 5, howQTFlag = 6, whenQTFlag = 7, whomQTFlag = 8, whyQTFlag = 9, wikiBusinessQTFlag = 10, wikiWorkQTFlag = 11, typeQTMask = (1 << 4) - 1,
		referencingObjectQTFlag = 1 << 4, subjectQTFlag = 2 << 4, objectQTFlag = 3 << 4, secondaryObjectQTFlag = 4 << 4, prepObjectQTFlag = 5 << 4,
		// answer must be an adjective - whose book?
		QTAFlag = 1 << 8
	};
	int getProximateObjectsMatchingOwnedItemType(cSource* questionSource, int si, cSyntacticRelationGroup*& ssrg, set <int>& proximityOwnedObjects);
	void answerQuestionInSourceWebWikiSearch(cSource* questionSource, 
		const bool parseOnly, const bool useParallelQuery, const bool answerPluralSpecification, bool &webSearchOrWikipediaTableSuccess, const bool disableWebSearch, bool& lastGoogleResultPage,
		cSyntacticRelationGroup*& ssrg,	vector < cAS >& answerSRGs, wchar_t* sqderivation, vector <cSyntacticRelationGroup> &subQueries, unordered_map <int, cWikipediaTableCandidateAnswers* > &wikiTableMap,
		int& numFinalAnswers, int& maxAnswer, vector <wstring> &webSearchQueryStrings);
	void answerQuestionInSourceOwnershipRetryQuery(cSource* questionSource,
		const bool parseOnly, const bool useParallelQuery, bool& webSearchOrWikipediaTableSuccess, const bool disableWebSearch,
		cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& ssrg, vector < cTrackDescendantAnswers>& descendantAnswers,
		vector < cAS >& answerSRGs);
	void answerQuestionInSourceProximityMapWebSearch(cSource* questionSource,
		const bool parseOnly, const bool useParallelQuery, const bool answerPluralSpecification, bool& webSearchOrWikipediaTableSuccess, bool& lastGoogleResultPage,
		cSyntacticRelationGroup*& ssrg, vector < cAS >& answerSRGs, wchar_t* sqderivation, vector <cSyntacticRelationGroup>& subQueries, 
		int& numFinalAnswers, int& maxAnswer, vector <wstring>& webSearchQueryStrings);
	int answerQuestionInSourceInitialize(cSource* questionSource, const bool parseOnly, const bool useParallelQuery, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& ssrg, vector < cTrackDescendantAnswers>& descendantAnswers, const bool disableWebSearch, wstring& ps, wstring& parentNum);
	int answerQuestionInSource(cSource* questionSource, bool parseOnly, bool useParallelQuery, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& ssri, vector < cTrackDescendantAnswers>& descendantAnswers, bool disableWebSearch);
	int answerAllQuestionsInSource(cSource *questionSource, bool parseOnly, bool useParallelQuery);
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
	bool isQuestionPassive(cSource *questionSource, cSyntacticRelationGroup *srg, cSyntacticRelationGroup * &ssri);
	bool processTransformQuestionPattern(cSource* questionSource, wstring patternType, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& lssri, cPattern* sourcePattern, cPattern* linkPattern, cSyntacticRelationGroup* linkSyntacticRelationGroup, unordered_map <wstring, wstring>& parseVariables);
	set <wstring> createAnswerListAsStrings(cSource* questionSource, set <int>& wherePossibleAnswers);
	bool followQuestionLink(int startLinkOffset, vector <cPattern*>& linkPatterns, cSource* questionSource, cSyntacticRelationGroup* srg, cPattern* sourcePattern, vector <cSyntacticRelationGroup*>& linkSyntacticRelationGroups, unordered_map <wstring, wstring>& parseVariables, bool parseOnly, bool useParallelQuery, bool disableWebSearch, vector < cTrackDescendantAnswers>& ancestorAnswers);
	int findMetanamePatterns(cSource* questionSource, cSyntacticRelationGroup* srg);
	int transformQuestion(cSource* questionSource, cSyntacticRelationGroup* srg, cSyntacticRelationGroup*& ssri, vector < cTrackDescendantAnswers>& descendantAnswers, bool parseOnly, bool useParallelQuery, bool disableWebSearch);
	void initializeTransformations(cSource *questionSource, unordered_map <wstring, wstring> &parseVariables);
	bool processPathToPattern(cSource *questionSource, const wchar_t *path, cSource *&source);
	class cTransformPatterns
	{
	public:
		vector <cPattern*> sourcePatterns;
		cPattern *destinationPattern;
		vector <cPattern*> linkPatterns;
		vector <cSyntacticRelationGroup*> linkSyntacticRelationGroups;
		cTransformPatterns(vector <cPattern*> sps, cPattern* dp, vector <cPattern*> &lps, vector <cSyntacticRelationGroup*> &lsr)
		{
			sourcePatterns = sps;
			destinationPattern = dp;
			linkPatterns = lps;
			linkSyntacticRelationGroups = lsr;
		}
		cTransformPatterns()
		{
			destinationPattern = NULL;
		}
	};
	map <vector <cSyntacticRelationGroup>::iterator, cTransformPatterns > transformationPatternMap;
	cSource *transformSource;
	unordered_map <wstring, cSource *> sourcesMap;
	unordered_map <wstring, cAnswerConfidence> childCandidateAnswerMap;
	void copySource(cSource *toSource, cSyntacticRelationGroup *constantQuestionSRI, cPattern *originalQuestionPattern, cPattern *constantQuestionPattern, unordered_map <int, int> &sourceMap, unordered_map <wstring, wstring> &parseVariables);
	int getWhereQuestionTypeObject(cSource *questionSource, cSyntacticRelationGroup* srg);
	void analyzeQuestionFromSourceSyntacticRelationSubjectVerbObjectPrep(cSource* questionSource, const wstring childSourceType, cSource* childSource,
		cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs, int& maxAnswer,
		vector <cSyntacticRelationGroup>::iterator childSRG, const int ws, const wstring matchInfoDetailSubject, const int matchSumSubject, const int wo, int &po,
		const bool questionTypeSubject, const bool questionTypePrepObject,
		const bool subjectMatch, const int verbMatch, const wstring matchInfoDetailVerb,
		wstring &matchInfoDetail, const int objectMatch, const int relativizerAsPrepMatch, const int secondaryObjectMatch, const int secondaryVerbMatch, set<int>& whereAnswerMatchSubquery);
	void analyzeQuestionFromSourceSyntacticRelationSubjectVerbObject(cSource* questionSource, const wstring childSourceType, cSource* childSource,
		cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs, int& maxAnswer,
		vector <cSyntacticRelationGroup>::iterator childSRG, const int ws, const int vi, const wstring matchInfoDetailSubject, const int matchSumSubject, const int wo,
		const bool questionTypeSubject, const bool questionTypeObject, const bool questionTypePrepObject,
		const bool subjectMatch, int &verbMatch, const wstring matchInfoDetailVerb);
	int analyzeQuestionFromSourceSyntacticRelationSubjectVerb(cSource* questionSource, const wstring childSourceType, cSource* childSource,
		cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs, int& maxAnswer,
		vector <cSyntacticRelationGroup>::iterator childSRG, const int ws, const int vi, const wstring matchInfoDetailSubject, const int matchSumSubject,
		const bool questionTypeSubject, const bool questionTypeObject, const bool questionTypePrepObject);
	int analyzeQuestionFromSourceSyntacticRelationSubject(cSource* questionSource, const wstring childSourceType, cSource* childSource,
		cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs, int& maxAnswer,
		vector <cSyntacticRelationGroup>::iterator childSRG, const int ws, const bool questionTypeSubject, const bool questionTypeObject, const bool questionTypePrepObject);
	void analyzeQuestionFromSourceSyntacticRelation(cSource* questionSource, wstring childSourceType, cSource* childSource,
		cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs, int& maxAnswer, 
		vector <cSyntacticRelationGroup>::iterator childSRG, bool questionTypeSubject, bool questionTypeObject, bool questionTypePrepObject);
	int analyzeQuestionFromSource(cSource *questionSource, wchar_t *derivation, wstring childSourceType, cSource *childSource, cSyntacticRelationGroup * parentSRG, vector < cAS > &answerSRGs, int &maxAnswer, bool eraseIfNoAnswers);
	int questionTypeCheck(cSource *questionSource, wstring derivation, cSyntacticRelationGroup* parentSRG, cAS &childCAS, int &semanticMismatch, bool &unableToDoquestionTypeCheck);
	int verbTenseMatch(cSource *questionSource, cSyntacticRelationGroup* parentSRG, cAS &childCAS);
	int semanticMatch(cSource *questionSource, wstring derivation, cSyntacticRelationGroup* parentSRG, cAS &childCAS, int &semanticMismatch);
	int semanticMatchSingle(cSource *questionSource, wstring derivation, cSyntacticRelationGroup* parentSRG, cSource *childSource, int whereChild, int childObject, int &semanticMismatch, bool &subQueryNoMatch,
		vector <cSyntacticRelationGroup> &subQueries, int numConsideredParentAnswer, bool useParallelQuery);
	bool checkIdentical(cSource *questionSource, cSyntacticRelationGroup* srg, cAS &cas1, cAS &cas2);
	void setWhereChildCandidateAnswer(cSource *questionSource, cAS &childCAS, cSyntacticRelationGroup* parentSRG);
	bool enterAnswerAccumulatingIdenticalAnswers(cSource *questionSource, cSyntacticRelationGroup *srg, cAS candidateAnswer, int &maxAnswer, vector < cAS > &answerSRGs);
	int  determineBestAnswers(cSource *questionSource, cSyntacticRelationGroup*  srg,vector < cAS > &answerSRGs,int maxAnswer,vector <cSyntacticRelationGroup> &subQueries,bool useParallelQuery);
	bool isModifiedGeneric(cAS &srg);
	int printAnswers(cSyntacticRelationGroup*  srg, vector < cAS > &answerSRGs);
	int searchWebSearchQueries(cSource *questionSource, wchar_t derivation[1024], cSyntacticRelationGroup* ssri,vector <cSyntacticRelationGroup> &subQueries,
		vector < cAS > &answerSRGs, vector <wstring> &webSearchQueryStrings,
		bool parseOnly, int &finalAnswer, int &maxAnswer, bool useParallelQuery, int &trySearchIndex, bool useGoogleSearch,bool &lastResultPage);
	int matchAnswersOfPreviousQuestion(cSource *questionSource, cSyntacticRelationGroup *ssri, set <int> &wherePossibleAnswers);
	int findConstrainedAnswers(cSource *questionSource, vector < cAS > &answerSRGs, vector < cTrackDescendantAnswers> &descendantAnswers);
	int processMetanameTagset(vector <cTagLocation>& tagSet, int whereMNE, int element, cSource* questionSource, cSource* childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cPattern*& mapPatternAnswer, cPattern*& mapPatternQuestion);
	int metaPatternMatch(cSource *questionSource, cSource *childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cPattern*& mapPatternAnswer, cPattern*& mapPatternQuestion);
	int	searchTableForAnswer(cSource *questionSource, wchar_t derivation[1024], cSyntacticRelationGroup* srg, unordered_map <int, cWikipediaTableCandidateAnswers * > &wikiTableMap, vector <cSyntacticRelationGroup> &subQueries, vector < cAS > &answerSRGs, int &maxAnswer, bool useParallelQuery);
	void addTables(cSource *questionSource, int whereQuestionType, cSource *wikipediaSource, vector < cSourceTable > &wikiTables);
	void analyzeQuestionThroughAbstractAndWikipediaFromRDFType(cSource *questionSource, wchar_t *derivation, int whereQuestionContextSuggestion, cSyntacticRelationGroup * parentSRG, cTreeCat *rdfType, bool parseOnly, vector < cAS > &answerSRGs, int &maxAnswer, unordered_map <int, cWikipediaTableCandidateAnswers *> &wikiTableMap, set <wstring> &wikipediaLinksAlreadyScanned);
	void enhanceWebSearchQueries(vector <wstring> &webSearchQueryStrings, wstring semanticSuggestion);
	void getWebSearchQueries(cSource *questionSource, cSyntacticRelationGroup* parentSRG, vector <wstring> &objects);
	void accumulateProximityMaps(cSource *questionSource,cSyntacticRelationGroup* parentSRG, cSource *childSource, bool confidence);
	bool processChildObjectIntoString(cSource *childSource, int childObject, unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings, wstring &childObjectString);
	void recordDistanceIntoProximityMap(cSource *childSource, unsigned int childSourceIndex, set <cObject::cLocation> &questionObjectMatchInChildSourceLocations,
		set <cObject::cLocation>::iterator &questionObjectMatchIndex, bool confidence, unordered_map <wstring, cProximityMap::cProximityEntry>::iterator closestObjectIterator);
	void accumulateProximityEntry(cSource *questionSource, unsigned int where, set <cObject::cLocation> &principalObjectLocations, set <cObject::cLocation>::iterator &polIndex, bool confidence, cSyntacticRelationGroup* parentSRG, cProximityMap *semanticMap, unordered_set <wstring> & whereQuestionInformationSourceObjectsStrings);
	int	parseSubQueriesParallel(cSource *questionSource,cSource *childSource, vector <cSyntacticRelationGroup> &subQueries, int whereChildCandidateAnswer, set <wstring> &wikipediaLinksAlreadyScanned);
	bool analyzeRDFTypeBirthDate(cSource* questionSource, cSyntacticRelationGroup* ssri, wstring derivation, vector < cAS >& answerSRGs, int& maxAnswer, wstring birthDate);
	bool analyzeRDFTypeOccupation(cSource* questionSource, cSyntacticRelationGroup* ssri, wstring derivation, vector < cAS >& answerSRGs, int& maxAnswer, wstring occupation);
	bool analyzeRDFTypes(cSource* questionSource, cSyntacticRelationGroup *sqi, cSyntacticRelationGroup* ssri, wstring derivation, vector < cAS >& answerSRGs, int& maxAnswer, unordered_map <int, cWikipediaTableCandidateAnswers* >& wikiTableMap, bool rejectEmptyObjects);
	int	matchSubQueries(cSource *questionSource, wstring derivation, cSource *childSource, int &semanticMismatch, bool &subQueryNoMatch, vector <cSyntacticRelationGroup> &subQueries, int whereChildCandidateAnswer, int whereChildCandidateAnswerEnd, int numConsideredParentAnswer, int semMatchValue, bool useParallelQuery);
	bool checkObjectIdentical(cSource *source1, cSource *source2, int object1, int object2);
	bool checkParticularPartIdentical(cSource *source1, cSource *source2, int where1, int where2);
	unordered_map<wstring, cSemanticMatchInfo> questionGroupMap;
	int checkParentGroup(cSource *parentSource, int parentWhere, cSource *childSource, int childWhere, int childObject, bool &synonym, int &semanticMismatch);
	int spinParses(MYSQL &mysql, vector <cSearchSource> &accumulatedParseRequests);
	int accumulateParseRequests(cSyntacticRelationGroup* parentSRG, int webSitesAskedFor, int index, bool googleSearch, vector <wstring> &webSearchQueryStrings, int &offset, vector <cSearchSource> &accumulatedParseRequests);
	int analyzeAccumulatedRequests(cSource *questionSource,wchar_t *derivation, cSyntacticRelationGroup *parentSRG, bool parseOnly, vector < cAS > &answerSRGs, int &maxAnswer, vector <cSearchSource> &accumulatedParseRequests);
	int webSearchForQueryParallel(cSource *questionSource, wchar_t *derivation, cSyntacticRelationGroup* parentSRG, bool parseOnly, vector < cAS > &answerSRGs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
		vector <wstring> &webSearchQueryStrings,int &offset);
	int webSearchForQuerySerial(cSource *questionSource, wchar_t *derivation, cSyntacticRelationGroup* parentSRG, bool parseOnly, vector < cAS > &answerSRGs, int &maxAnswer, int webSitesAskedFor, int index, bool googleSearch,
		vector <wstring> &webSearchQueryStrings,int &offset);
	bool dbSearchMusicBrainzSearchType(cSource *questionSource, wchar_t *derivation, cSyntacticRelationGroup* parentSRG, vector < cAS > &answerSRGs,
		int firstWhere, wstring firstMatchListType, int secondWhere, wstring secondMatchListType, set <wstring> &matchVerbsList);
	bool dbSearchMusicBrainz(cSource *questionSource, wchar_t *derivation, cSyntacticRelationGroup* parentSRG, vector < cAS > &answerSRGs);
	bool dbSearchForQuery(cSource *questionSource, wchar_t *derivation, cSyntacticRelationGroup* parentSRG, vector < cAS > &answerSRGs);
	bool matchOwnershipDbMusicBrainzObject(cSource *questionSource, wchar_t *derivation, int whereObject, vector <mbInfoReleaseType> &mbs);
	bool matchOwnershipDbQuery(cSource *questionSource,wchar_t *derivation, cSyntacticRelationGroup* parentSRG);
	bool matchOwnershipDbMusicBrainz(cSource *questionSource,wchar_t *derivation, cSyntacticRelationGroup* parentSRG);
	int processSnippet(cSource *questionSource, wstring snippet, wstring object, cSource *&source, bool parseOnly);
	int processAbstract(cSource *questionSource, cTreeCat *rdfType, cSource *&source, bool parseOnly);
	int processWikipedia(cSource *questionSource, int principalWhere, cSource *&source, vector <wstring> &wikipediaLinks, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool parseOnly, set <wstring> &wikipediaLinksAlreadyScanned, bool removePrecedingUncapitalizedWordsFromProperNouns);
	bool matchObjectsExactByName(vector <cObject>::iterator parentObject, vector <cObject>::iterator childObject, bool &namedNoMatch);
	static bool matchObjectsByName(cSource *parentSource, vector <cObject>::iterator parentObject, cSource *childSource, vector <cObject>::iterator childObject, bool &namedNoMatch, sTrace debugTrace);
	static bool matchChildSourcePositionByName(cSource *parentSource, vector <cObject>::iterator parentObject, cSource *childSource, int childWhere, bool &namedNoMatch, sTrace &debugTrace);
	static bool matchTimeObjects(cSource *parentSource, int parentWhere, cSource *childSource, int childWhere);
	int sriPrepMatch(cSource *parentSource, cSource *childSource, int parentWhere, int childWhere, int cost);
	int sriVerbMatch(cSource *parentSource, cSource *childSource, int parentWhere, int childWhere, wstring &matchInfoDetailVerb, wstring verbTypeMatch,int cost);
	int srgMatch(cSource *questionSource, cSource *childSource, int parentWhere, int childWhere, int whereQuestionType, __int64 questionType, bool &totalMatch, wstring &matchInfoDetail, int cost, bool subQuery);
	int equivalenceClassCheck(cSource *questionSource, cSource *childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cSyntacticRelationGroup* parentSRG, int whereChildSpecificObject, int &equivalenceClass, int matchSum);
	int equivalenceClassCheck2(cSource *questionSource, cSource *childSource, vector <cSyntacticRelationGroup>::iterator childSRG, cSyntacticRelationGroup* parentSRG, int whereChildSpecificObject, int &equivalenceClass, int matchSum);
	bool rejectPath(const wchar_t *path);
	bool matchParticularAnswer(cSource *questionSource, cSyntacticRelationGroup *ssri, int whereMatch, int wherePossibleAnswer, set <int> &addWhereQuestionInformationSourceObjects);
	void detectSubQueries(cSource *questionSource, cSyntacticRelationGroup *srg, vector <cSyntacticRelationGroup> &subQueries);
	bool matchAnswerSourceMatch(cSource *questionSource,cSyntacticRelationGroup *ssri, int whereMatch, int wherePossibleAnswer, set <int> &addWhereQuestionInformationSourceObjects);
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