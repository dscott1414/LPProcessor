/*
	DB.cpp - MySQL source-table I/O, lexicon hydrate, object flush, and ontology write

	Overview:
		The operational half of the DB layer (schema create is DBCreateSQLSchema.cpp;
		statement execution is DBUtility.cpp).  This file: claims/releases rows in
		`sources` so multiple LPProcessor processes can divide the Gutenberg corpus;
		hydrates Words from `words`/`wordForms`/`forms` (with an optional
		wordFormCache file); writes discovered objects back to `objects` /
		`objectWordMap`; and has a few ontology/wiktionary helpers.

	Pipeline position:
		Stage 1 (initializeDatabaseHandle, readWordsFromDB) and the bookkeeping
		around each source (getNextUnprocessedSource ... signalFinishedProcessingSource).
		flushObjects runs after identifyObjects when WRITE_OBJECTS_TO_DB is on.

	Key entry points:
		- initializeDatabaseHandle() - mysql_real_connect to DBNAME as root.
		- getNextUnprocessedSource() / resetSource* / signal*ProcessingSource.
		- cWord::readWordsFromDB / readWordFormsFromDB / initializeWordsFromDB.
		- cSource::flushObjects / readMultiSourceObjects / updateSource*.
		- writeDbOntologyEntry / readWikiNominalizations / isBookTitle.

	Dependencies:
		MySQL schema `lp` (see DBCreateSQLSchema.cpp).  wordFormCache next to cwd
		when USE_TEST_CACHE is on.  Hardcoded DSN user/password (root/byron0).

	Notes / gotchas:
		- initializeDatabaseHandle embeds "root"/"byron0".  Same credentials are
			repeated in createDatabase().
		- updateSourceStatistics / 2 / 3 LOCK TABLES sources WRITE and never
			UNLOCK - every stats write leaves the table locked.
		- getNumSources LOCKs then returns -1 on query failure without UNLOCK.
		- isBookTitle interpolates proposedTitle in double quotes, unescaped.
		- readWikiNominalizations never mysql_free_result.
		- isWordFormCacheValid overwrites `result` with a second SELECT without
			freeing the first MYSQL_RES.
		- Form ids in the DB are 1-based; wordForms.formId-1 is the in-memory
			offset.  patternFormNumOffset (32750) stores usage-pattern counts in
			the same table.
		- lplog(LOG_FATAL_ERROR) exits (see logging.cpp); comments in source.h
			that claim otherwise are wrong.
*/
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
#include "mysqldb.h"

bool checkFull(MYSQL* mysql, wchar_t* qt, size_t& len, bool flush, wchar_t* qualifier);

/*

to compare tables:
SELECT id,word,inflectionFlags,flags&(8192*256-1),mainEntryWordId,derivationRules INTO OUTFILE 'F:\wordsbad.txt' FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"' LINES
TERMINATED BY '\n' FROM words;
SELECT wordId, formId, count INTO OUTFILE 'F:\wordformsbad.txt' FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"' LINES TERMINATED BY '\n' FROM wordforms where formId<32000 order by
wordId,formId;
SELECT id,word,inflectionFlags,flags&(8192*256-1),mainEntryWordId,derivationRules INTO OUTFILE 'F:\wordsgood.txt' FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"' LINES
TERMINATED BY '\n' FROM wordsOriginal;
SELECT wordId, formId, count INTO OUTFILE 'F:\wordformsgood.txt' FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"' LINES TERMINATED BY '\n' FROM wordformsOriginal where formId<32000 order by
wordId,formId;

update sources set processed=null, processing=null where title like '%Secret Adversary%';
alter database lp character set utf8;
// to see word relations from a source:
select wrh.w, wr.typeId, fw.word, tw.word from wordRelationsHistory wrh, words fw, words tw, wordRelations wr where wrh.wordRelationId=wr.id and fw.id=wr.fromWordId and tw.id=wr.toWordId and wrh.sourceId=2123 order by wrh.w limit 20;
see the most popular word relations:
+------+-----------+
| id   | word      |
+------+-----------+
|    8 | am        |
|  182 | what      |
|  262 | __ppn__   |
|  764 | be    |
| 2735 | ishas |
| 2747 | it        |
| 5118 | something |
| 5345 | that      |
| 5355 | them  |
| 5362 | there     |
| 5368 | they      |
| 5381 | this  |
| 5757 | which |
| 5769 | who       |
+------+-----------+
create table tempwr (wrttype int, fword CHAR(32), tword CHAR(32), totalCount int)
select wrt.type, fw.word fword, tw.word tword, wr.totalCount from wordrelationtype wrt,words fw, words tw, wordRelations wr where fw.id=wr.fromWordId and tw.id=wr.toWordId and wrt.id=wr.typeId and wrt.id<10 and (wrt.id&1)=0 and fromWordId not in (8,182,262,764,2735,2747,5118,5345,5355,5362,5368,5381,5757,5769) and toWordId not in (8,182,262,764,2735,2747,5118,5345,5355,5362,5368,5381,5757,5769) order by wr.totalCount desc limit 6000;

// ** to see particular words associated only with other words (autogroups)
temp4 is like allnouns except one of the fields must match the wordid that is of interest (6049=doctor)

create table allnouns select subjectLocal from multiwordrelations union all select objectLocal from multiwordrelations union all select prepObjectLocal from multiwordrelations union all select nextObjectLocal from multiwordrelations union all select secondaryObjectLocal from multiwordrelations;
create table an2 select count(subjectLocal) ct, subjectLocal o from allnouns group by subjectLocal order by count(subjectLocal) desc;
// attempts to filter out words that have only been used as proper nouns
select temp4.ct,temp4.o,wo.word,an2.ct,an2.ct/temp4.ct,wf.count from temp4,an2,words wo,wordforms wf where wf.wordId=temp4.o and wf.formid=32756 and wo.id=temp4.o and an2.o=temp4.o and temp4.ct>4 && an2.ct/temp4.ct<800 group by wf.wordid order by an2.ct/temp4.ct asc limit 100;

******************************
selecting 'SPECIAL' relations
what verbs is a 'doctor' specially connected to?
select * from words where word='doctor'; id=6049!
create table subjectVerbCount select COUNT(verb) subjectVerbCount,verb from multiWordRelations where subjectLocal=6049 group by verb;
create table verbCounts select count(mwr.verb) totalVerbCount,mwr.verb from multiWordRelations mwr,subjectVerbCount where mwr.verb=subjectVerbCount.verb group by mwr.verb;
select subjectVerbCount.subjectVerbCount/verbCounts.totalVerbCount,words.word,subjectVerbCount.subjectVerbCount, verbCounts.totalVerbCount from verbCounts,subjectVerbCount,words where verbCounts.totalVerbCount>1 and subjectVerbCount.subjectVerbCount>1 and words.id=verbCounts.verb and verbCounts.verb=subjectVerbCount.verb order by (subjectVerbCount.subjectVerbCount)/(verbCounts.totalVerbCount) desc;
COMBINED for subjectLocal=7058 (professor):
select subjectVerbCount.subjectVerbCount/verbCounts.totalVerbCount,words.word,subjectVerbCount.subjectVerbCount, verbCounts.totalVerbCount
from (select count(verb) totalVerbCount,verb from multiWordRelations group by verb) verbCounts,
	 (select COUNT(verb) subjectVerbCount,verb from multiWordRelations where subjectLocal=7058 group by verb) subjectVerbCount,
		 words
where verbCounts.totalVerbCount>2 and subjectVerbCount.subjectVerbCount>2 and words.id=verbCounts.verb and verbCounts.verb=subjectVerbCount.verb order by (subjectVerbCount.subjectVerbCount)/(verbCounts.totalVerbCount) desc;

COMBINED for verb=169 (teach):
select subjectVerbCount.subjectVerbCount/subjectCounts.totalSubjectCount,words.word,subjectVerbCount.subjectVerbCount, subjectCounts.totalSubjectCount
from (select count(subjectLocal) totalSubjectCount,subjectLocal from multiWordRelations group by subjectLocal) subjectCounts,
	 (select COUNT(subjectLocal) subjectVerbCount,subjectLocal from multiWordRelations where verb=169 group by subjectLocal) subjectVerbCount,
		 words
where subjectCounts.totalSubjectCount>2 and subjectVerbCount.subjectVerbCount>2 and words.id=subjectCounts.subjectLocal and subjectCounts.subjectLocal=subjectVerbCount.subjectLocal order by subjectVerbCount.subjectVerbCount/subjectCounts.totalSubjectCount desc;

// features:
	avoids words that have low counts (<=2) and words that only exist as proper nouns by seeing whether lowerCaseCount is null or lowerCaseCount/upperCaseCount<10
select subjectVerbCount.subjectVerbCount/subjectCounts.totalSubjectCount,words.word,subjectVerbCount.subjectVerbCount, subjectCounts.totalSubjectCount, lowercase.count lowerCaseCount, uppercase.count upperCaseCount
from (select COUNT(words.mainEntryWordId) totalSubjectCount,words.mainEntryWordId subjectLocal from multiWordRelations,words where words.id=multiWordRelations.subjectLocal group by words.mainEntryWordId) subjectCounts,
	 (select COUNT(words.mainEntryWordId) subjectVerbCount,words.mainEntryWordId subjectLocal from multiWordRelations,words where words.id=multiWordRelations.subjectLocal and verb=169 group by words.mainEntryWordId) subjectVerbCount,
		 words
		 left outer join (select wordId,count from wordforms where formId=32756) lowercase on lowercase.wordId=words.id
		 left outer join (select wordId,count from wordforms where formId=32757) uppercase on uppercase.wordId=words.id
where subjectCounts.totalSubjectCount>2 and subjectVerbCount.subjectVerbCount>2 and words.id=subjectCounts.subjectLocal and subjectCounts.subjectLocal=subjectVerbCount.subjectLocal
HAVING lowerCaseCount IS NOT NULL and (upperCaseCount IS NULL OR upperCaseCount/lowerCaseCount<0.1)
order by subjectVerbCount.subjectVerbCount/subjectCounts.totalSubjectCount desc;
*/
// UNLOCK TABLES.  Returns myquery's result (false is FATAL unless the server
// is already gone).
bool unlockTables(MYSQL& mysql)
{
	LFS
		return myquery(&mysql, L"UNLOCK TABLES");
}

// Clear processed/processing on every sources row so the corpus can be
// re-run.  Returns false if the LOCK fails.
bool cSource::resetAllSource()
{
	LFS
		if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return false;
	myquery(&mysql, L"update sources set processed=NULL,processing=NULL");
	unlockTables(mysql);
	return true;
}

// Clear processed/processing for sources.id in [beginSource, endSource).
bool cSource::resetSource(int beginSource, int endSource)
{
	LFS
		if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return false;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set processed=NULL,processing=NULL where id>=%d and id<%d", beginSource, endSource);
	myquery(&mysql, qt);
	unlockTables(mysql);
	return true;
}

// Clear the processing bit on every source (leaves processed alone).  Used
// after a crash so a half-done source can be claimed again.
void cSource::resetProcessingFlags(void)
{
	LFS
		if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return;
	myquery(&mysql, L"update sources set processing=NULL");
	unlockTables(mysql);
}

// Set processing=true on sources.id=thisSourceId.  Returns false if LOCK fails.
bool cSource::signalBeginProcessingSource(int thisSourceId)
{
	LFS
		if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return false;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set processing=true where id=%d", thisSourceId);
	myquery(&mysql, qt);
	unlockTables(mysql);
	return true;
}

// Mark thisSourceId processed, clear processing, stamp lastProcessedTime=NOW().
bool cSource::signalFinishedProcessingSource(int thisSourceId)
{
	LFS
		if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return false;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set processing=NULL, processed=true, lastProcessedTime=NOW() where id=%d", thisSourceId);
	myquery(&mysql, qt);
	unlockTables(mysql);
	return true;
}


// Claim the largest unprocessed source of sourceType with id in [begin, end)
// (end<0 means no upper bound), skipping **SKIP** / **START NOT FOUND**.
// setUsed==true writes processing=true under the same WRITE lock.
// Out-params are filled only when a row is found.  Returns true if one was
// claimed.  mysql_free_result is called even when the SELECT fails (result
// may be NULL - mysql_free_result(NULL) is safe).
bool getNextUnprocessedSource(MYSQL& mysql, int begin, int end, int sourceType, bool setUsed, int& id, wstring& path, wstring& encoding, wstring& start, int& repeatStart, wstring& etext, wstring& author, wstring& title)
{
	LFS
		MYSQL_RES* result = NULL;
	if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return false;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, path, encoding, start, repeatStart, etext, author, title from sources where id>=%d and sourceType=%d and processed IS NULL and processing IS NULL and start!='**SKIP**' and start!='**START NOT FOUND**'", begin, sourceType);
	if (end >= 0)
		_snwprintf(qt + wcslen(qt), QUERY_BUFFER_LEN - wcslen(qt), L" and id<%d", end);
	wcscat(qt + wcslen(qt), L" order by numWords desc limit 1");
	MYSQL_ROW sqlrow = NULL;
	if (myquery(&mysql, qt, result))
	{
		if ((sqlrow = mysql_fetch_row(result)) && setUsed)
		{
			_snwprintf(qt, QUERY_BUFFER_LEN, L"update sources set processing=true where id=%S", sqlrow[0]);
			myquery(&mysql, qt);
		}
		if (sqlrow != NULL)
		{
			id = atoi(sqlrow[0]);
			mTW(sqlrow[1], path);
			if (sqlrow[2] != NULL)
				mTW(sqlrow[2], encoding);
			mTW(sqlrow[3], start);
			repeatStart = atoi(sqlrow[4]);
			if (sqlrow[5] != NULL)
				mTW(sqlrow[5], etext);
			if (sqlrow[6] != NULL)
				mTW(sqlrow[6], author);
			if (sqlrow[7] != NULL)
				mTW(sqlrow[7], title);
		}
	}
	mysql_free_result(result);
	unlockTables(mysql);
	return sqlrow != NULL;
}

// True if at least one processed, not-currently-processing source of
// sourceType still has proc2==step (a later "unknown words" pass).
bool anymoreUnprocessedForUnknown(MYSQL& mysql, int sourceType, int step)
{
	LFS
		if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return false;
	MYSQL_RES* result;
	_int64 numResults = 0;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select id, etext, path, title from sources where sourceType=%d and processed is not NULL and processing is NULL and start!='**SKIP**' and start!='**START NOT FOUND**' and proc2=%d order by id limit 1", sourceType, step);
	if (myquery(&mysql, qt, result))
	{
		numResults = mysql_num_rows(result);
		mysql_free_result(result);
	}
	unlockTables(mysql);
	return numResults > 0;
}

// Write path/start/repeatStart/sizeInBytes for the sources row whose etext
// matches.  path and start are escapeStr'd in place (mutated); etext is not
// escaped (etext values are Gutenberg ids, but this is still a hole).
bool cSource::updateSource(wstring& path, wstring& start, int repeatStart, wstring& etext, int actualLenInBytes)
{
	LFS
		wstring tmp, tmp2, sqlStatement;
	escapeStr(path);
	escapeStr(start);
	sqlStatement = L"update sources set path='" + path + L"', start='" + start + L"', repeatStart=" + itos(repeatStart, tmp) + L", sizeInBytes=" + itos(actualLenInBytes, tmp2) + L" where etext='" + etext + L"'";
	return myquery(&mysql, (wchar_t*)sqlStatement.c_str());
}

// Like updateSource but only start/repeatStart/sizeInBytes.  start is
// truncated to the last 255 chars (the column is VARCHAR(256)).
bool cSource::updateSourceStart(wstring& start, int repeatStart, wstring& etext, __int64 actualLenInBytes)
{
	LFS
		wstring tmp, tmp2, sqlStatement;
	escapeStr(start);
	if (start.length() > 255)
		start = start.substr(start.length() - 255, 255); // start column has a limit of 256 characters
	sqlStatement = L"update sources set start='" + start + L"', repeatStart=" + itos(repeatStart, tmp) + L", sizeInBytes=" + itos((int)actualLenInBytes, tmp2) + L" where etext='" + etext + L"'";
	return myquery(&mysql, (wchar_t*)sqlStatement.c_str());
}

// Write encoding and readBufferFlags for etext.  sourceEncoding is truncated
// to 54 chars but is not escaped (comment about "start column" is stale).
bool cSource::updateSourceEncoding(int readBufferType, wstring sourceEncoding, wstring etext)
{
	LFS
		wstring tmp, sqlStatement;
	if (sourceEncoding.length() > 55)
		sourceEncoding = sourceEncoding.substr(0, 54); // start column has a limit of 256 characters
	sqlStatement = L"update sources set encoding='" + sourceEncoding + L"', readBufferFlags=" + itos(readBufferType, tmp) + L" where etext='" + etext + L"'";
	return myquery(&mysql, (wchar_t*)sqlStatement.c_str());
}


// COUNT(*) of sources of sourceType, excluding SKIP / START NOT FOUND.
// left==true: only still-unprocessed rows.  Returns -1 on LOCK/query failure;
// the query-failure path does not UNLOCK.
int getNumSources(MYSQL& mysql, int sourceType, bool left)
{
	LFS
		MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&mysql, L"LOCK TABLES sources READ")) return -1;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select COUNT(*) FROM sources where sourceType=%d and start!='**START NOT FOUND**' and start!='**SKIP**'%s", sourceType, (left) ? L" and processed is null" : L"");
	if (!myquery(&mysql, qt, result)) return -1;
	unlockTables(mysql);
	int numSources = 0;
	if ((sqlrow = mysql_fetch_row(result)))
		numSources = atoi(sqlrow[0]);
	mysql_free_result(result);
	//if (endSourceId-beginSourceId >= mysql_num_rows(result))
	//	endSourceId=
	//	
	//if (beginSource != 0) mysql_data_seek(result, beginSource);
	//sqlrow = mysql_fetch_row(result);
	//beginSource = atoi(sqlrow[0]);
	//mysql_data_seek(result, endSource);
	//sqlrow = mysql_fetch_row(result);
	//if (sqlrow)
	//	endSource = atoi(sqlrow[0]) + 1;
	//else
	//{
	//	endSource = numSources - 1;
	//	mysql_data_seek(result, endSource);
	//	sqlrow = mysql_fetch_row(result);
	//	endSource = atoi(sqlrow[0]) + 1;
	//}
	//numSources = endSource - beginSource;
	return numSources;
}

// mysql_init + mysql_real_connect to host 'where', schema DBNAME, user
// root / password byron0.  Sets utf8mb4 / utf8mb4_bin and MYSQL_OPT_RECONNECT.
// alreadyConnected is in/out: true on entry skips the connect; set true on
// success.  Returns 0 or -1.  Credentials are hardcoded.
int initializeDatabaseHandle(MYSQL& mysql, const wchar_t* where, bool& alreadyConnected)
{
	LFS
		if (alreadyConnected) return 0;
	if (mysql_init(&mysql) == NULL)
		lplog(LOG_FATAL_ERROR, L"Failed to initialize MySQL");
	bool keep_connect = true;
	mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
	string sqlStr;
	if (alreadyConnected = (mysql_real_connect(&mysql, wTM(where, sqlStr), "root", "byron0", DBNAME, 0, NULL, 0) != NULL))
	{
		myquery(&mysql, L"SET NAMES 'utf8mb4' COLLATE 'utf8mb4_bin'");
		mysql_options(&mysql, MYSQL_OPT_RECONNECT, &keep_connect);
		if (mysql_set_character_set(&mysql, "utf8mb4"))
			lplog(LOG_ERROR, L"Error setting default client character set to utf8mb4.  Default now: %S\n", mysql_character_set_name(&mysql));
		return 0;
	}
	return -1;
}

// Write parse-quality counters onto this source's row.  LOCKs sources WRITE
// and never UNLOCKs - every call leaves the table locked for this connection.
void cSource::updateSourceStatistics(int numSentences, int matchedSentences, int numWords, int numUnknown,
	int numUnmatched, int numOvermatched, int numQuotations, int quotationExceptions, int numTicks, int numPatternMatches)
{
	LFS
		wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"UPDATE sources SET numSentences=%d, matchedSentences=%d, numWords=%d, numUnknown=%d, numUnmatched=%d, "
		L"numOvermatched=%d, numQuotations=%d, quotationExceptions=%d, numTicks=%d, numPatternMatches=%d where id=%d",
		numSentences, matchedSentences, numWords, numUnknown, numUnmatched, numOvermatched, numQuotations, quotationExceptions,
		numTicks, numPatternMatches, sourceId);
	myquery(&mysql, qt);
}

// Write sizeInBytes / numWordRelations.  Same missing-UNLOCK as above.
void cSource::updateSourceStatistics2(int sizeInBytes, int numWordRelations)
{
	LFS
		wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"UPDATE sources SET sizeInBytes=%d, numWordRelations=%d where id=%d", sizeInBytes, numWordRelations, sourceId);
	myquery(&mysql, qt);
}

// Write numMultiWordRelations.  Same missing-UNLOCK as above.
void cSource::updateSourceStatistics3(int numMultiWordRelations)
{
	LFS
		wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"UPDATE sources SET numMultiWordRelations=%d where id=%d", numMultiWordRelations, sourceId);
	myquery(&mysql, qt);
}

// Load objects.common=1 (multi-source / location-like) and their
// objectWordMap rows, appending cObject / cWordMatch entries and filling
// relatedObjectsMap.  wordMap[wordId] must be valid for every mapped word.
// BIT columns are tested as sqlrow[n][0]==1 (the binary 1 MySQL returns for
// BIT, not '1').  objects[currentId] treats the DB objectId as a vector
// index - non-dense ids are out-of-range.  Returns 0 or -1.
int cSource::readMultiSourceObjects(tIWMM* wordMap, int numWords)
{
	LFS
		int startTime = clock();
	MYSQL_RES* result = NULL;
	if (!myquery(&mysql, L"select id, objectClass, nickName, ownerObject, firstSpeakerGroup, male, female, neuter, plural FROM objects where common=1", result)) return -1;
	MYSQL_ROW sqlrow;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		cName nm;
		nm.nickName = atoi(sqlrow[2]);
		cObject o((OC)atoi(sqlrow[1]), nm, -1, -1, -1, -1, atoi(sqlrow[3]), sqlrow[4][0] == 1, sqlrow[5][0] == 1, sqlrow[6][0] == 1, sqlrow[7][0] == 1, false);
		o.dbIndex = atoi(sqlrow[0]);
		o.multiSource = true;
		objects.push_back(o);
	}
	mysql_free_result(result);
	if (!myquery(&mysql, L"select objectId, wordId, orderOrType FROM objectWordMap ORDER BY objectId, orderOrType", result)) return -1;
	int previousId = -1;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		int currentId = atoi(sqlrow[0]);
		if (previousId == (int)currentId)
		{
			objects[previousId].end++;
			objects[previousId].originalLocation++;
		}
		else
			objects[currentId].begin = objects[currentId].end = objects[currentId].originalLocation = objects[currentId].firstLocation = m.size();
		if (atoi(sqlrow[1]) >= numWords)
			lplog(LOG_FATAL_ERROR, L"non-existent word reference.");
		m.emplace_back(wordMap[atoi(sqlrow[1])], 0, debugTrace);
		relatedObjectsMap[wordMap[atoi(sqlrow[1])]].insert(currentId);
		if (objects[currentId].objectClass == NAME_OBJECT_CLASS)
		{
			switch (atoi(sqlrow[2]))
			{
			case cName::HON: objects[currentId].name.hon = wordMap[atoi(sqlrow[1])]; break;
			case cName::HON2: objects[currentId].name.hon2 = wordMap[atoi(sqlrow[1])]; break;
			case cName::HON3: objects[currentId].name.hon3 = wordMap[atoi(sqlrow[1])]; break;
			case cName::FIRST: objects[currentId].name.first = wordMap[atoi(sqlrow[1])]; break;
			case cName::MIDDLE: objects[currentId].name.middle = wordMap[atoi(sqlrow[1])]; break;
			case cName::MIDDLE2: objects[currentId].name.middle2 = wordMap[atoi(sqlrow[1])]; break;
			case cName::LAST: objects[currentId].name.last = wordMap[atoi(sqlrow[1])]; break;
			case cName::SUFFIX: objects[currentId].name.suffix = wordMap[atoi(sqlrow[1])]; break;
			case cName::ANY: objects[currentId].name.any = wordMap[atoi(sqlrow[1])]; break;
			}
		}
	}
	mysql_free_result(result);
	if (logDatabaseDetails)
		lplog(L"readMultiSourceObjects took %d seconds.", (clock() - startTime) / CLOCKS_PER_SEC);
	return 0;
}

// REPLACE INTO objects and INSERT INTO objectWordMap for every index in
// objectsToFlush.  Uses checkFull() to batch VALUES lists.  Returns 0, or
// -1 if LOCK/checkFull fails (the checkFull-failure path does not UNLOCK).
// objectLocations is not written (numObjectLocationsInserted stays 0).
// right now it is assumed that all objects from books are kept separate,
// so there is no check for merging information between info sources
// but there is a 'common' list consisting of all multiple-word objects, usually locations.
int cSource::flushObjects(set <int>& objectsToFlush)
{
	LFS
		wprintf(L"Writing objects to database...\r");
	if (!myquery(&mysql, L"LOCK TABLES objects WRITE")) return -1;

	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	wchar_t wmqt[QUERY_BUFFER_LEN_OVERFLOW];
	int startTime = clock(), numObjectsInserted = 0, numObjectLocationsInserted = 0, where, lastProgressPercent = -1;
	wcscpy(qt, L"REPLACE INTO objects VALUES ");
	size_t len = wcslen(qt);
	set <int>::iterator oi = objectsToFlush.begin(), oiEnd = objectsToFlush.end();
	for (; oi != oiEnd && !exitNow; oi++, numObjectsInserted++)
	{
		//int no=*oi;
		cObject* o = &(objects[*oi]);
		len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L"(%d,%d,%d,%d,%d,%d,%d,%d,%d,NULL,%d,%d,%d,%d,%d,%d,%d),",
			sourceId, *oi,
			o->objectClass, o->numEncounters, o->numIdentifiedAsSpeaker, o->name.nickName, o->getOwnerWhere(), o->getFirstSpeakerGroup(),
			getProfession(*oi),
			o->identified == true ? 1 : 0, o->plural == true ? 1 : 0, o->male == true ? 1 : 0, o->female == true ? 1 : 0, o->neuter == true ? 1 : 0,
			/*multiSource*/0,
			(o->objectClass == NAME_OBJECT_CLASS) ? 1 : 0);
		if ((where = numObjectsInserted * 100 / objects.size()) > lastProgressPercent)
		{
			wprintf(L"PROGRESS: %03d%% objects written to database with %04d seconds elapsed \r", where, clocksec());
			lastProgressPercent = where;
		}
		if (!checkFull(&mysql, qt, len, false, NULL)) return -1;
	}
	if (!checkFull(&mysql, qt, len, true, NULL)) return -1;
	myquery(&mysql, L"UNLOCK TABLES");
	lastProgressPercent = -1;
	int objectDetails = 0;
	if (!myquery(&mysql, L"LOCK TABLES objectWordMap WRITE", true)) return -1;
	wcscpy(wmqt, L"INSERT INTO objectWordMap VALUES ");
	size_t wmlen = wcslen(wmqt);
	for (oi = objectsToFlush.begin(); oi != oiEnd && !exitNow; oi++, objectDetails++)
	{
		cObject* o = &(objects[*oi]);
		if ((where = objectDetails * 100 / objects.size()) > lastProgressPercent)
		{
			wprintf(L"PROGRESS: %03d%% object details written to database with %04d seconds elapsed \r", where, clocksec());
			lastProgressPercent = where;
		}
		if (o->objectClass == NAME_OBJECT_CLASS)
			wmlen += o->name.insertSQL(wmqt + wmlen, sourceId, *oi, QUERY_BUFFER_LEN - wmlen);
		else
		{
			for (int I = o->begin; I < o->end; I++)
				if (m[I].word->second.index >= 0)
					wmlen += _snwprintf(wmqt + wmlen, QUERY_BUFFER_LEN - wmlen, L"(%d,%d,%d,%d),", sourceId, *oi, m[I].word->second.index, I - o->begin);
		}
		if (!checkFull(&mysql, wmqt, wmlen, false, NULL)) return -1;
	}
	if (!checkFull(&mysql, wmqt, wmlen, true, NULL)) return -1;
	if (numObjectsInserted && logDatabaseDetails)
		lplog(L"Inserting %d objects from %d locations took %d seconds.", numObjectsInserted, numObjectLocationsInserted, (clock() - startTime) / CLOCKS_PER_SEC);
	myquery(&mysql, L"UNLOCK TABLES");
	wprintf(L"PROGRESS: 100%% objects written to database with %04d seconds elapsed         \n", clocksec());
	return 0;
}

// Empty word: no forms, index = -uniqueNewIndex++ (a unique negative id
// until the DB assigns a real one), all usage/relation slots zeroed.
cSourceWordInfo::cSourceWordInfo(void)
{
	LFS
		count = 0;
	inflectionFlags = 0;
	flags = 0;
	derivationRules = 0;
	mainEntry = wNULL;
	index = -uniqueNewIndex++;
	memset(usagePatterns, 0, MAX_USAGE_PATTERNS);
	memset(usageCosts, 0, MAX_USAGE_PATTERNS);
	memset(deltaUsagePatterns, 0, MAX_USAGE_PATTERNS);
	memset(relationMaps, 0, numRelationWOTypes * sizeof(*relationMaps));
	changedSinceLastWordRelationFlush = false;
	tmpMainEntryWordId = -1;
	formsOffset = fACount;
	sourceId = -1;
	numProperNounUsageAsAdjective = 0;
	localWordIsCapitalized = 0;
	localWordIsLowercase = 0;
}

// Copy DBUsagePatterns[upStart, upStart+upLength) into usagePatterns,
// scaling so the max value is at most highestPatternCount (128 or 64).
// Does not itself write usageCosts.
// called by transferFormsAndUsage (in this file)
// this procedure does not make a high pattern count into a low cost.
// it makes a high pattern count received from the DB into a count which will fit into the usagePatterns array (max 128)
void cSourceWordInfo::transferDBUsagePatternsToUsagePattern(int highestPatternCount, int* DBUsagePatterns, unsigned int upStart, unsigned int upLength)
{
	LFS
		int highest = -1;
	for (unsigned int up = upStart; up < upStart + upLength; up++)
		highest = max(highest, DBUsagePatterns[up]);
	if (highest < highestPatternCount)
		for (unsigned int up = upStart; up < upStart + upLength; up++)
			usagePatterns[up] = (char)DBUsagePatterns[up];
	else
		// 600 8 0 -> highest=600   128  1 0
		for (unsigned int up = upStart; up < upStart + upLength; up++)
			usagePatterns[up] = (highestPatternCount * ((unsigned int)DBUsagePatterns[up]) / highest);
}

// Hydrate from a DB wordForms slice (forms[i]=formId, forms[i+1]=count,
// iCount pairs).  Appends the form ids onto the process-wide formsArray
// (grows it via trealloc 17).  mainEntry is resolved later from
// tmpMainEntryWordId.  formNum is the "word is its own form" id, or -1.
// formNum is the form that is the word itself, if that form exists.
// read the cSourceWordInfo from the DB.  So, don't mark this with insertNewForms flag
cSourceWordInfo::cSourceWordInfo(unsigned int* forms, unsigned int iCount, int iInflectionFlags, int iFlags, int iTimeFlags, int mainEntryWordId, int iDerivationRules, int iSourceId, int formNum, wstring& word)
{
	LFS
		count = iCount;
	int oldAllocated = allocated;
	if (fACount + count >= allocated)
		formsArray = (unsigned int*)trealloc(17, formsArray, oldAllocated * sizeof(*formsArray), (allocated = (allocated + count) * 2) * sizeof(*formsArray));
	formsOffset = fACount;
	fACount += count;
	inflectionFlags = iInflectionFlags;
	flags = iFlags;
	timeFlags = iTimeFlags;
	derivationRules = iDerivationRules;
	mainEntry = wNULL;
	index = -uniqueNewIndex++;
	//preferVerbPresentParticiple();
	tmpMainEntryWordId = mainEntryWordId;
	sourceId = iSourceId;
	transferFormsAndUsage(forms, iCount, formNum, word);
	fACount += (count - iCount);
	count = iCount;
	memset(relationMaps, 0, numRelationWOTypes * sizeof(*relationMaps));
	numProperNounUsageAsAdjective = 0;
	localWordIsCapitalized = 0;
	localWordIsLowercase = 0;
}

// Split the (formId,count) pairs into real forms vs usage-pattern fake
// form ids (>= patternFormNumOffset-1).  Dedupes forms, fills usagePatterns
// / usageCosts, and sets wordFrequency from TRANSFER_COUNT.  iCount is
// overwritten with the number of real forms kept.
void cSourceWordInfo::transferFormsAndUsage(unsigned int* forms, unsigned int& iCount, int formNum, wstring& word)
{
	LFS
		memset(usagePatterns, 0, MAX_USAGE_PATTERNS);
	memset(usageCosts, 0, MAX_USAGE_PATTERNS);
	memset(deltaUsagePatterns, 0, MAX_USAGE_PATTERNS);
	changedSinceLastWordRelationFlush = false;
	int UPDB[MAX_USAGE_PATTERNS];
	memset(UPDB, 0, sizeof(*UPDB) * MAX_USAGE_PATTERNS);
	int sameNameForm = -1, properNounForm = -1, formCount = 0;
	for (unsigned int I = 0, *J = forms; I < (unsigned)iCount; I++, J += 2)
	{
		// has to be patternFormOffset-1 because each form has 1 subtracted from it on read.
		if (*J >= (patternFormNumOffset - 1) && *J - (patternFormNumOffset - 1) > MAX_USAGE_PATTERNS - MAX_FORM_USAGE_PATTERNS)
			::lplog(LOG_INFO, L"%s:Incorrect form # - %d.", word.c_str(), *J);
		else if (*J >= (patternFormNumOffset - 1))
			UPDB[*J - (patternFormNumOffset - 1) + MAX_FORM_USAGE_PATTERNS] = *(J + 1);
		else
		{
			// filter out duplicate forms (which for some reason have crept in)
			unsigned int* f, * fend;
			for (f = formsArray + formsOffset, fend = formsArray + formsOffset + formCount; f != fend; f++)
				if (*f == *J)
					break;
			if (f != fend)
			{
				//::lplog(LOG_INFO,L"Duplicate form %s seen in word %s.",Forms[*J]->name.c_str(),word.c_str());
				continue;
			}
			if (formCount >= MAX_FORM_USAGE_PATTERNS)
			{
				wstring formStrings;
				for (int K = 0; K < formCount; K++)
					formStrings += Form(K)->name + L" ";
				::lplog(LOG_DICTIONARY, L"numForms of %s over %d: %s.", word.c_str(), MAX_FORM_USAGE_PATTERNS, formStrings.c_str());
			}
			else
			{
				formsArray[formsOffset + formCount] = *J;
				UPDB[formCount] = *(J + 1);
				if (*J == formNum)
					sameNameForm = formCount;
				else if (*J == PROPER_NOUN_FORM_NUM)
					properNounForm = formCount;
				formCount++;
			}
		}
	}
	iCount = formCount;
	transferDBUsagePatternsToUsagePattern(128, UPDB, 0, iCount);
	transferFormUsagePatternsToCosts(sameNameForm, properNounForm, iCount);
	//  transfer noun SINGULAR_NOUN_HAS_DETERMINER,SINGULAR_NOUN_HAS_NO_DETERMINER,
	transferDBUsagePatternsToUsagePattern(64, UPDB, SINGULAR_NOUN_HAS_DETERMINER, 2);
	transferUsagePatternsToCosts(HIGHEST_COST_OF_INCORRECT_NOUN_DET_USAGE, SINGULAR_NOUN_HAS_DETERMINER, 2);
	// transfer verb  VERB_HAS_0_OBJECTS,VERB_HAS_1_OBJECTS,VERB_HAS_2_OBJECTS,
	transferDBUsagePatternsToUsagePattern(64, UPDB, VERB_HAS_0_OBJECTS, 3);
	transferUsagePatternsToCosts(HIGHEST_COST_OF_INCORRECT_VERB_USAGE, VERB_HAS_0_OBJECTS, 3);
	// transfer LOWER_CASE_USAGE_PATTERN,PROPER_NOUN_USAGE_PATTERN,
	usagePatterns[LOWER_CASE_USAGE_PATTERN] = min(127, UPDB[LOWER_CASE_USAGE_PATTERN]);
	usagePatterns[PROPER_NOUN_USAGE_PATTERN] = min(127, UPDB[PROPER_NOUN_USAGE_PATTERN]);
	wordFrequency = UPDB[TRANSFER_COUNT];
}

// Overlay this in-memory word with a DB row.  Rejects the update (returns
// false, sets updateMainInfo) unless the in-memory word has
// queryOnLowerCase/queryOnAnyAppearance AND the DB row does not - i.e. a
// locally-forced re-query wins.  On accept, replaces forms/flags and
// returns true.
// update cSourceWordInfo from the DB.  So, don't mark this with insertNewForms flag
bool cSourceWordInfo::updateFromDB(int wordId, unsigned int* forms, unsigned int iCount, int iInflectionFlags, int iFlags, int iTimeFlags, int iMainEntryWordId, int iDerivationRules, int iSourceId, int formNum, wstring& word)
{
	LFS
		index = wordId;
	if ((!(flags & queryOnLowerCase) && !(flags & queryOnAnyAppearance)) || ((iFlags & queryOnLowerCase) || (iFlags & queryOnAnyAppearance))) // update rejected.
	{
		flags |= updateMainInfo; // update database
		return false;
	}
	count = iCount;
	int oldAllocated = allocated;
	if (fACount + count >= allocated)
		formsArray = (unsigned int*)trealloc(17, formsArray, oldAllocated * sizeof(*formsArray), (allocated = (allocated + count) * 2) * sizeof(*formsArray));
	formsOffset = fACount;
	fACount += count;
	inflectionFlags = iInflectionFlags;
	flags = iFlags;
	timeFlags = iTimeFlags;
	derivationRules = iDerivationRules;
	tmpMainEntryWordId = iMainEntryWordId;
	//index=-1;
	sourceId = iSourceId;
	flags &= ~updateMainInfo; // update from database accepted.  Don't update database
	transferFormsAndUsage(forms, iCount, formNum, word);
	count = iCount;
	return true;
}

#define NUM_WORD_ALLOCATION 102400
// idToMap[wordId] = iWord and iWord->second.index = wordId.  Grows the
// sparse array by NUM_WORD_ALLOCATION slots (filled with wNULL) as needed.
void cWord::mapWordIdToWordStructure(int wordId, tIWMM iWord)
{
	LFS
		while (idsAllocated <= wordId)
		{
			int previousIdsAllocated = idsAllocated;
			idsAllocated += NUM_WORD_ALLOCATION;
			idToMap = (tIWMM*)trealloc(22, idToMap, previousIdsAllocated * sizeof(*idToMap), idsAllocated * sizeof(*idToMap));
			if (!idToMap)
				lplog(LOG_FATAL_ERROR, L"Out of memory allocating %d bytes to idToMap array.", idsAllocated * sizeof(*idToMap));
			for (int I = idsAllocated - NUM_WORD_ALLOCATION; I < idsAllocated; I++)
				idToMap[I] = wNULL;
			//memset(idToMap+idsAllocated-NUM_WORD_ALLOCATION,wNULL,NUM_WORD_ALLOCATION*sizeof(*idToMap));
		}
#ifdef LOG_WORD_FLOW
	lplog(L"Word %s assigned to index %d.", iWord->first.c_str(), wordId);
#endif
	idToMap[wordId] = iWord;
	iWord->second.index = wordId;
}

// idToMap[wordId], or wNULL if wordId is out of range or never mapped.
tIWMM cWord::wordStructureGivenWordIdExists(int wordId)
{
	if (wordId >= idsAllocated || wordId < 0)
		return wNULL;
	return idToMap[wordId];
}

// If an open-class word has no mainEntry, LOG_INFO and set
// queryOnAnyAppearance so the next DB refresh will try again.  'where' is
// only for the log (a call-site cookie).
void cSourceWordInfo::mainEntryCheck(const wstring first, int where)
{
	LFS
		if (mainEntry == wNULL && (query(nounForm) >= 0 || query(verbForm) >= 0 || query(adjectiveForm) >= 0 || query(adverbForm) >= 0))
		{
			if (!iswalpha(first[0])) return;
			::lplog(LOG_INFO, L"Word %s has no mainEntry CHECK(%d)!", first.c_str(), where);
			flags |= cSourceWordInfo::queryOnAnyAppearance;
		}
}

// SELECT forms (optionally only rows newer than lastReadfromDBTime) and
// append any name not already in Forms.  FATAL if both sides grew since the
// last refresh (incompatible form-id spaces).  qt is a scratch buffer.
void cWord::readForms(MYSQL& mysql, wchar_t* qt)
{
	LFS
		MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"SELECT id,name,shortName,inflectionsClass,hasInflections,isTopLevel,properNounSubClass,blockProperNounRecognition from forms");
	if (lastReadfromDBTime > 0)
		_snwprintf(qt + wcslen(qt), QUERY_BUFFER_LEN - wcslen(qt), L" where UNIX_TIMESTAMP(ts)>%d", lastReadfromDBTime);
	if (!myquery(&mysql, qt, result)) return;
	unsigned int numDbForms = 0, newDbForms = 0;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		wstring name, shortName, inflectionsClass;
		mTW(sqlrow[1], name);
		mTW(sqlrow[2], shortName);
		mTW(sqlrow[3], inflectionsClass);
		unsigned int index = atoi(sqlrow[0]);
		numDbForms = max(numDbForms, index);
		if (cForms::findForm(name) < 0)
		{
			int hasInflections = sqlrow[4][0], isTopLevel = sqlrow[5][0], properNounSubClass = sqlrow[6][0], blockProperNounRecognition = sqlrow[7][0];
#ifdef LOG_WORD_FLOW
			lplog(L"form=%s shortForm=%s inflectionsClass=%s hasInflections=%d properNounSubClass=%d isTopLevel=%d blockProperNounRecognition=%d.",
				name.c_str(), shortName.c_str(), inflectionsClass.c_str(), hasInflections, properNounSubClass, isTopLevel, blockProperNounRecognition);
#endif
			newDbForms++;
			Forms.push_back(new cForm(index, name, shortName, inflectionsClass, hasInflections != 0, properNounSubClass != 0, isTopLevel != 0, false, false, blockProperNounRecognition != 0)); // ,Forms.size() removed
			cForms::formMap[name] = Forms.size() - 1;
		}
	}
	mysql_free_result(result);
	// if there are new forms in the database AND new forms in memory, this is incompatible!
	if (lastReadfromDBTime > 0 && newDbForms > 0 && Forms.size() > numDbForms)
		lplog(LOG_FATAL_ERROR, L"New forms discovered in db AND new forms already in memory!  Exiting!");
}

// True if the cwd wordFormCache file is newer than MAX(ts) of words and
// wordForms.  The first SELECT's MYSQL_RES is overwritten by the second
// without a free (leak).  Returns false if the file is missing or older.
bool cWord::isWordFormCacheValid(MYSQL& mysql)
{
	HANDLE hFile = CreateFile(L"wordFormCache", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR, L"Failed to open wordFormCache - %d", GetLastError());
		return false;
	}
	FILETIME lastWriteTime;
	int retCode = GetFileTime(hFile, NULL, NULL, &lastWriteTime);
	CloseHandle(hFile);
	if (!retCode)
		return false;
	LARGE_INTEGER date, adjust;
	date.HighPart = lastWriteTime.dwHighDateTime;
	date.LowPart = lastWriteTime.dwLowDateTime;
	// 100-nanoseconds = milliseconds * 10000
	adjust.QuadPart = 11644473600000 * 10000;
	// removes the diff between 1970 and 1601
	date.QuadPart -= adjust.QuadPart;
	// converts back from 100-nanoseconds to seconds
	int wordFormCacheLastWriteTime = (int)(date.QuadPart / 10000000);
	MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	// this is insufficient as ISAM table update times are not updated consistently
	//if (!myquery(&mysql, L"SELECT UNIX_TIMESTAMP(UPDATE_TIME) FROM information_schema.tables WHERE TABLE_SCHEMA = 'lp'	AND (TABLE_NAME = 'wordforms' or table_name = 'words')", result)) 
	//	return false;
	bool wordCacheValid = true;
	if (!myquery(&mysql, L"SELECT UNIX_TIMESTAMP(MAX(TS)) FROM words", result))
		return false;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		int tableTimestamp = atoi(sqlrow[0]);
		if (tableTimestamp > wordFormCacheLastWriteTime)
			wordCacheValid = false;
	}
	if (!myquery(&mysql, L"SELECT UNIX_TIMESTAMP(MAX(TS)) FROM wordforms", result))
		return false;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		int tableTimestamp = atoi(sqlrow[0]);
		if (tableTimestamp > wordFormCacheLastWriteTime)
			wordCacheValid = false;
	}
	if (!wordCacheValid)
		printf("Refreshing wordcache...                                \n");
	mysql_free_result(result);
	return wordCacheValid;
}

// Fill words[wordId]=offset into wordForms, counts[wordId]=#forms, and
// wordForms as a flat (formId-1, count) pairs array.  Tries wordFormCache
// first when USE_TEST_CACHE and lastReadfromDBTime==-1.  Returns 0 if the
// caller should stop (skipWordInitialization cache hit), 2 to continue, -1
// on query failure.  specialExtension is unused.
// called by readWordsFromDB (in this file)
// Reads word ids into words with numWordForms filled with the number of wordforms, and 
//   wordForms filled with the forms of the words, with counts the number of wordForms for each word.
// if return code is 0, parent calling this procedure should return with a 0.
// if return code is 2, parent calling this procedure should continue.
// qt is a scratch query buffer.
int cWord::readWordFormsFromDB(MYSQL& mysql, int maxWordId, wchar_t* qt, int* words, int* counts, int& numWordForms, unsigned int*& wordForms, bool printProgress, bool skipWordInitialization, wstring specialExtension)
{
	LFS
		memset(words, -1, maxWordId * sizeof(int));
	memset(counts, 0, maxWordId * sizeof(int));
	bool testCacheSuccess = false;
#ifdef USE_TEST_CACHE
	;
	if (lastReadfromDBTime == -1 && isWordFormCacheValid(mysql))
	{
		if (skipWordInitialization)
			return 0;
		int testfd = open("wordFormCache", O_RDWR | O_BINARY);
		if (testfd >= 0)
		{
			int checkMaxWordId;
			::read(testfd, &checkMaxWordId, sizeof(checkMaxWordId));
			if (checkMaxWordId == maxWordId)
			{
				::read(testfd, words, maxWordId * sizeof(int));
				::read(testfd, counts, maxWordId * sizeof(int));
				::read(testfd, &numWordForms, sizeof(numWordForms));
				wordForms = (unsigned int*)tmalloc(numWordForms * sizeof(int) * 2);
				::read(testfd, wordForms, numWordForms * sizeof(int) * 2);
				testCacheSuccess = numWordForms > 0;
			}
			close(testfd);
		}
	}
#endif
	if (!testCacheSuccess)
	{
		int wf = 0;
		numWordForms = 0;
		MYSQL_RES* result = NULL;
		MYSQL_ROW sqlrow;
		int len = _snwprintf(qt, QUERY_BUFFER_LEN, L"select wf.wordId, wf.formId, wf.count from wordForms wf, words w where w.id=wf.wordId AND w.sourceId IS NULL");
		if (lastReadfromDBTime > 0)
			len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L" AND UNIX_TIMESTAMP(w.ts)>%d", lastReadfromDBTime);
		_snwprintf(qt + len, QUERY_BUFFER_LEN - len, L" ORDER BY wordId, formId");
		if (!myquery(&mysql, qt, result)) return -1;
		numWordForms = (int)mysql_num_rows(result);
		wordForms = (unsigned int*)tmalloc(numWordForms * sizeof(int) * 2);
		if (printProgress)
			wprintf(L"Reading word forms from database...                        \r");
		while ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			int wordId = atoi(sqlrow[0]);
			if (wordId<0 || wordId>maxWordId)
				lplog(LOG_FATAL_ERROR, L"wordId < 0 or > %d.", maxWordId);
			if (words[wordId] == -1) words[wordId] = wf;
			counts[wordId]++;
			wordForms[wf++] = atoi(sqlrow[1]) - 1; // formId -1 yields form offset in memory
			wordForms[wf++] = atoi(sqlrow[2]); // count
		}
		mysql_free_result(result);
#ifdef USE_TEST_CACHE
		if (lastReadfromDBTime == -1)
		{
			int testfd = open("wordFormCache", O_WRONLY | O_CREAT | O_BINARY, _S_IREAD | _S_IWRITE);
			if (testfd >= 0)
			{
				if (::write(testfd, &maxWordId, sizeof(maxWordId)) < 0) return -1;
				if (::write(testfd, words, maxWordId * sizeof(int)) < 0) return -1;
				if (::write(testfd, counts, maxWordId * sizeof(int)) < 0) return -1;
				if (::write(testfd, &numWordForms, sizeof(numWordForms)) < 0) return -1;
				if (::write(testfd, wordForms, numWordForms * sizeof(int) * 2) < 0) return -1;
				close(testfd);
			}
			if (!myquery(&mysql, L"flush tables")) return -1;  // try to prevent corruption on possible power loss
		}
#endif
	}
	return 2;
}

// MAX(id)+1 from words (the allocation size for the words[]/counts[]
// scratch arrays).  Returns -1 on query/empty failure; the empty-row path
// does not mysql_free_result.
int getMaxWordId(MYSQL& mysql)
{
	LFS
		MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	if (!myquery(&mysql, L"select MAX(id) from words", result)) return -1;
	if ((sqlrow = mysql_fetch_row(result)) == NULL) return -1;
	int maxWordId = (sqlrow[0] == NULL) ? 0 : atoi(sqlrow[0]) + 1;
	mysql_free_result(result);
	return maxWordId;
}

// SELECT words where sourceId IS NULL and insert/update WMM, resolving
// mainEntry via tmpMainEntryWordId after the scan.  Ensures the ||| section
// sentinel exists.  Sets lastReadfromDBTime to NOW()+1.  Returns 0 or -1
// (the NOW() NULL-row path returns -1 without freeing result).
int cWord::initializeWordsFromDB(MYSQL& mysql, int* words, int* counts, unsigned int*& wordForms, int& numWordsInserted, int& numWordsModified, bool printProgress)
{
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	if (printProgress)
		wprintf(L"Reading from database: waiting for response from source NULL word query...                        \r");
	// read words
	tIWMM iWord = wNULL;
	int len = _snwprintf(qt, QUERY_BUFFER_LEN, L"select id, word, inflectionFlags, flags, timeFlags, mainEntryWordId, derivationRules from words where sourceId IS NULL");
	if (lastReadfromDBTime > 0)
		len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L" AND UNIX_TIMESTAMP(ts)>%d", lastReadfromDBTime);
	//_snwprintf(qt+len,QUERY_BUFFER_LEN-len,L" ORDER BY word");
	if (!myquery(&mysql, qt, result)) return -1;
	__int64 numRows = mysql_num_rows(result);
	WMM.reserve(numRows);
	int where, lastProgressPercent = -1, numWords = 0;
	if (printProgress)
		wprintf(L"Reading from database: reading results from source NULL word query...                        \r");
	vector <tIWMM> wordsToResolve;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		int wordId = atoi(sqlrow[0]), flags = atoi(sqlrow[3]), timeFlags = atoi(sqlrow[4]), mainEntryWordId = (sqlrow[5] == NULL) ? -1 : atoi(sqlrow[5]), derivationRules = (sqlrow[6] == NULL) ? 0 : atoi(sqlrow[6]);
		unsigned int inflectionFlags = atoi(sqlrow[2]);
		wstring sWord;
		mTW(sqlrow[1], sWord);
		if (words[wordId] == -1)
		{
			if (!iswdigit(sWord[0]) && sWord[0])
				lplog(LOG_ERROR, L"No forms for %s (wordId=%d) found!", sWord.c_str(), wordId);
			continue;
		}
		int selfFormNum = cForms::findForm(sWord);
		tIWMM tryWord;
		if (lastReadfromDBTime >= 0 && (tryWord = WMM.find(sWord)) != WMM.end())
		{
			iWord = tryWord;
#ifdef LOG_WORD_FLOW
			lplog(L"Updated (from sourceId %d) read word %s.", sourceId, sWord.c_str());
#endif
			if (iWord->second.index < 0)
				mapWordIdToWordStructure(wordId, iWord);
			if (iWord->second.updateFromDB(wordId, wordForms + words[wordId], counts[wordId], inflectionFlags, flags, timeFlags, mainEntryWordId, derivationRules, -1, selfFormNum, sWord))
				numWordsModified++;
		}
		else
		{
			iWord = WMM.insert(tWFIMap(sWord, cSourceWordInfo(wordForms + words[wordId], counts[wordId], inflectionFlags, flags, timeFlags, mainEntryWordId, derivationRules, -1, selfFormNum, sWord))).first;
			numWordsInserted++;
#ifdef LOG_WORD_FLOW
			lplog(L"Inserted (from sourceId %d) read word %s.", sourceId, sWord.c_str());
#endif
			mapWordIdToWordStructure(wordId, iWord);
		}
		if (mainEntryWordId >= 0)
			wordsToResolve.push_back(iWord);
		if ((where = numWords++ * 100 / (int)numRows) > lastProgressPercent)
		{
			if (printProgress)
				wprintf(L"PROGRESS: %03d%% words read with %04d seconds elapsed\r", where, clocksec());
			lastProgressPercent = where;
		}
		if (iWord->second.query(timeForm) < 0 && iWord->second.query(numberForm) < 0 && iWord->second.query(telenumForm) < 0 &&
			handleExtendedParseWords((wchar_t*)sWord.c_str())) // if handleExtendedParseWords is true, removed word
			iWord = wNULL;
	}
	mysql_free_result(result);
	int numMainEntriesNotFound = 0;
	mainEntryMap.reserve(mainEntryMap.size() + wordsToResolve.size());
	for (vector<tIWMM>::iterator wi = wordsToResolve.begin(), wiEnd = wordsToResolve.end(); wi != wiEnd; wi++)
	{
		if (wordStructureGivenWordIdExists((*wi)->second.tmpMainEntryWordId) == wNULL)
		{
			numMainEntriesNotFound++;
			lplog(LOG_FATAL_ERROR, L"mainEntryWordId %d not found for word %s (lastReadfromDBTime=%d)!", (*wi)->second.tmpMainEntryWordId, (*wi)->first.c_str(), lastReadfromDBTime); // mainEntries and any references must always have the same source
			(*wi)->second.mainEntry = *wi;
		}
		else
		{
			(*wi)->second.mainEntry = wordStructureGivenWordIdExists((*wi)->second.tmpMainEntryWordId);
			(*wi)->second.tmpMainEntryWordId = -1;
			if ((*wi)->second.mainEntry != wNULL)
				mainEntryMap[(*wi)->second.mainEntry->first].push_back(*wi);
		}
	}
	if ((sectionWord = WMM.find(L"|||")) == WMM.end())
	{
		bool added;
		addNewOrModify(NULL, wstring(L"|||"), cSourceWordInfo::topLevelSeparator, SECTION_FORM_NUM, 0, 0, L"section", -1, added);
		sectionWord = WMM.find(L"|||");
	}
	if (!myquery(&mysql, L"select UNIX_TIMESTAMP(NOW())", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) == NULL)
		return -1;
	if (sqlrow[0] == NULL)
	{
		if (printProgress)
			wprintf(L"Finished reading from database...                                               \r");
		mysql_free_result(result);
		return 0; // no words to read
	}
	lastReadfromDBTime = atoi(sqlrow[0]) + 1;
	mysql_free_result(result);
	return 0;
}

// Stage-1 lexicon load: readForms, getMaxWordId, readWordFormsFromDB,
// initializeWordsFromDB, optional generateFormStatistics.  Returns 0, or
// maxWordId if that helper failed (<=0).
// update read: read NULL source since lastReadfromDBTime and update or insert the entries into memory (lastReadfromDBTime>0)
int cWord::readWordsFromDB(MYSQL& mysql, bool generateFormStatisticsFlag, bool printProgress, bool skipWordInitialization)
{
	LFS
		int startTime = clock();
	int numWordsInserted = 0, numWordsModified = 0;
	if (printProgress)
		wprintf(L"Started reading from database...                        \r");
	//lplog(L"TIME DB READ START for source %d:%s",sourceId,getTimeStamp());
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	readForms(mysql, qt);
	if (nounForm == -1) nounForm = cForms::gFindForm(L"noun");
	if (verbForm == -1) verbForm = cForms::gFindForm(L"verb");
	if (adjectiveForm < 0) adjectiveForm = cForms::gFindForm(L"adjective");
	if (adverbForm < 0) adverbForm = cForms::gFindForm(L"adverb");
	if (timeForm < 0) timeForm = cForms::gFindForm(L"time");
	numberForm = NUMBER_FORM_NUM;
	cForms::changedForms = false;
	int maxWordId = getMaxWordId(mysql);
	if (maxWordId <= 0) return maxWordId;
	// read wordForms of the words
	int* words = (int*)tmalloc(maxWordId * sizeof(int));
	int* counts = (int*)tmalloc(maxWordId * sizeof(int));
	int numWordForms;
	unsigned int* wordForms = NULL;
	if (readWordFormsFromDB(mysql, maxWordId, qt, words, counts, numWordForms, wordForms, printProgress, skipWordInitialization, L"") == 0) return 0;
	if (initializeWordsFromDB(mysql, words, counts, wordForms, numWordsInserted, numWordsModified, printProgress) < 0)
		lplog(LOG_FATAL_ERROR, L"Cannot read words from DB.");
	tfree(maxWordId * sizeof(int), counts);
	tfree(maxWordId * sizeof(int), words);
	tfree(numWordForms * sizeof(int) * 2, wordForms);
	if (generateFormStatisticsFlag)
		generateFormStatistics();
	if (printProgress)
		wprintf(L"Finished reading from database...                                                  \r");
	if (numWordsInserted || numWordsModified)
		lplog(L"%d words inserted into and %d words modified in memory in %d seconds.", numWordsInserted, numWordsModified, (clock() - startTime) / CLOCKS_PER_SEC);
	return 0;
}

// True if openlibraryinternetarchivebooksdump has a row whose title equals
// proposedTitle.  Title is interpolated in double quotes with no escape.
bool isBookTitle(MYSQL& mysql, wstring proposedTitle)
{
	if (!myquery(&mysql, L"LOCK TABLES openlibraryinternetarchivebooksdump READ"))
		return false;
	MYSQL_RES* result;
	_int64 numResults = 0;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select 1 from openlibraryinternetarchivebooksdump where title = \"%s\"", proposedTitle.c_str());
	if (myquery(&mysql, qt, result))
	{
		numResults = mysql_num_rows(result);
		mysql_free_result(result);
	}
	if (!myquery(&mysql, L"UNLOCK TABLES"))
		return false;
	lplog(LOG_INFO, L"*** isBookTitle: statement %s resulted in numRows=%d.", qt, numResults);
	return numResults > 0;
}

// Scan wiktionarynouns definitions containing "gent noun of" or "one who "
// and fill agentiveNominalizations[noun] with the extracted verb.
// Does not mysql_free_result (leak).  Returns 0 or -1.
int readWikiNominalizations(MYSQL& mysql, unordered_map <wstring, set < wstring > >& agentiveNominalizations)
{
	LFS
		MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	if (!myquery(&mysql, L"select noun,lower(definition) from wiktionarynouns where lower(definition) like '%gent noun of%' or lower(definition) like '%one who %'", result)) return -1;
	//int previousId=-1;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		wstring noun, verb, definition;
		mTW(sqlrow[0], noun);
		mTW(sqlrow[1], definition);
		size_t pos = definition.find(L"gent noun of");
		if (pos != wstring::npos)
		{
			pos += wcslen(L"gent noun of ");
			for (; iswalpha(definition[pos]) || definition[pos] == '-'; pos++)
				verb += definition[pos];
		}
		else
		{
			size_t pos2 = definition.find(L"one who ");
			if (pos2 != wstring::npos)
			{
				pos2 += wcslen(L"one who ");
				for (; iswalpha(definition[pos2]) || definition[pos2] == '-'; pos2++)
					verb += definition[pos2];
				if (verb.empty() || verb == L"is" || verb[verb.length() - 1] != L's' || verb[0] != noun[0])
					verb.clear();
			}
		}
		if (!verb.empty())
		{
			agentiveNominalizations[noun].insert(verb);
			lplog(LOG_WHERE, L"WK %s %s", noun.c_str(), verb.c_str());
		}
	}
	return 0;
}

// Raise the running max-* lengths for one ontology entry and count
// superClasses strings over 150/170/190 chars.  Used only to size the table.
void maxFieldLengths(const wstring key, cOntologyEntry& dbPredicate, int& maxKey, int& maxCompactLabel, int& maxInfoPage, int& maxAbstractDescription, int& maxCommentDescription, int& maxSuperClasses, int& numGT150, int& numGT170, int& numGT190)
{
	wstring superClasses;
	for (auto sc : dbPredicate.superClasses)
		superClasses += sc + L"|";
	maxKey = max(maxKey, (signed)key.length());
	maxCompactLabel = max(maxCompactLabel, (signed)dbPredicate.compactLabel.length());
	maxInfoPage = max(maxInfoPage, (signed)dbPredicate.infoPage.length());
	maxAbstractDescription = max(maxAbstractDescription, (signed)dbPredicate.abstractDescription.length());
	maxCommentDescription = max(maxCommentDescription, (signed)dbPredicate.commentDescription.length());
	maxSuperClasses = max(maxSuperClasses, (signed)superClasses.length());
	if (superClasses.length() > 150)
		numGT150++;
	if (superClasses.length() > 170)
		numGT170++;
	if (superClasses.length() > 190)
		numGT190++;
}

// In-place: prefix ' and " with \\, and double existing backslashes.  Used
// by writeDbOntologyEntry; not the same as escapeStr() (single-quote only).
void escapeAllQuote(wstring& str)
{
	LFS
		wstring ess;
	for (unsigned int I = 0; I < str.length(); I++)
	{
		if (str[I] == '\'' || str[I] == '\"') ess += '\\';
		ess += str[I];
		if (str[I] == '\\') ess += '\\';
	}
	str = ess;
}

// INSERT one ontology row.  Mutates dbPredicate (clamps negative ranks) and
// escapeAllQuote's onkey/compactLabel/abstractDescription in place; infoPage
// and commentDescription / superClasses are NOT escaped.  allowFailure=true
// so ER_DUP_ENTRY is treated as success.  Superclass strings >10000 are
// logged and skipped.
bool writeDbOntologyEntry(MYSQL& mysql, const wstring key, cOntologyEntry& dbPredicate)
{
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	wstring superClasses;
	for (auto sc : dbPredicate.superClasses)
		superClasses += sc + L"|";
	wstring onkey = key;
	escapeAllQuote(onkey);
	escapeAllQuote(dbPredicate.compactLabel);
	escapeAllQuote(dbPredicate.abstractDescription);
	if (dbPredicate.numLine < 0)
		dbPredicate.numLine = 0;
	if (dbPredicate.ontologyHierarchicalRank < 0)
		dbPredicate.ontologyHierarchicalRank = 100;
	if (dbPredicate.ontologyType < 0)
		dbPredicate.ontologyType = 100;
	_snwprintf(qt, QUERY_BUFFER_LEN, L"insert into ontology (onkey,compactLabel,infoPage,abstractDescription,commentDescription,numLine,ontologyHierarchicalRank,ontologyType,superClasses) VALUES (\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%d,%d,%d,\"%s\")",
		onkey.c_str(), dbPredicate.compactLabel.c_str(), dbPredicate.infoPage.c_str(), dbPredicate.abstractDescription.c_str(), dbPredicate.commentDescription.c_str(), dbPredicate.numLine, dbPredicate.ontologyHierarchicalRank,
		dbPredicate.ontologyType, superClasses.c_str());
	if (superClasses.length() > 10000)
		lplog(LOG_INFO, L"SUPER CLASSES TOO LONG:\n%s", qt);
	else
	{
		bool success = (myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY);
		return success;
	}
	return false;
}

