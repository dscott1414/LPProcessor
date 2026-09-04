/*
	DBWordRelations.cpp - load word-relation maps for the words of the current source

	Overview:
		After the lexicon is in memory, readWordIdsNeedingWordRelations() collects
		every word (and its resolveToClass main-entry) that appears in this
		source, plus the synthetic PPN/NUM/DATE/... sentinels.  initializeWordRelationsFromDB
		then LOCKs wordRelationsMemory and SELECTs every row whose fromWordId
		(and, if inSourceFlagSet is false, toWordId) is in that set, restricted
		to even typeIds 0..36.  Each row is installed on both endpoints via
		allocateMap + addRelation (the complementary type is written on the to-word).

	Pipeline position:
		Stage 1, after readWordsFromDB and before parse.  Also re-run when a
		source's word set changes.

	Key entry points:
		- cSourceWordInfo::allocateMap() - new cRMap for one relationType.
		- cSource::readWordIdsNeedingWordRelations() - fill the id set.
		- cWord::initializeWordRelationsFromDB() - the batched SELECT.

	Dependencies:
		Table wordRelationsMemory (MEMORY engine, see DBCreateSQLSchema.cpp).
		Words must already contain every id (wordStructureGivenWordIdExists).

	Notes / gotchas:
		- NULLWORD 187 is unused in this file.
		- typeId in (0,2,4,...,36) keeps only one direction of each pair; the
			complement is synthesized in memory by getComplementaryRelationship.
		- The IN-list is built from integer ids (no injection) but is not
			escaped as a prepared statement; checkFull is not used - the loop
			caps each batch at QUERY_BUFFER_LEN lpchar_t.
		- LOCK is released on the query-failure path and at the end; the
			function returns -1 if the initial LOCK fails (no unlock needed).
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "time.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "vcXML.h"
#include "profile.h"
#include "mysqldb.h"
#include "internet.h"
#include "questionAnswering.h"

#define NULLWORD 187
bool checkFull(MYSQL* mysql, lpchar_t* qt, size_t& len, bool flush, lpchar_t* qualifier);

// Ensure relationMaps[relationType] is a live cRMap.  No-op if already
// allocated.  relationType is a relationWOTypes enumerator (0..numRelationWOTypes).
void cSourceWordInfo::allocateMap(int relationType)
{
	LFS
		if (relationMaps[relationType] == NULL)
			relationMaps[relationType] = new cRMap;
}

// Insert every token's word id and (if resolveToClass is not a 1-char class
// token) its main-entry id into the set.  Also seeds the PPN/TELENUM/NUM/
// DATE/TIME/LOCATION/sectionWord sentinels.  Newly inserted words have their
// relationMaps cleared so the subsequent DB load starts clean.
// Always returns 0.
// main entries from all words in source must already exist in memory (Words array)
// return a set of wordIds for all words including the mainEntries, except for special words that do not already have wordRelations
int cSource::readWordIdsNeedingWordRelations(set <int>& wordIdsAndMainEntryIdsInSourceNeedingWordRelations)
{
	LFS
		tIWMM specials[] = { Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION,Words.sectionWord };
	for (unsigned int I = 0; I < sizeof(specials) / sizeof(tIWMM); I++)
	{
		wordIdsAndMainEntryIdsInSourceNeedingWordRelations.insert(specials[I]->second.index);
		specials[I]->second.clearRelationMaps();
	}
	tIWMM tWord;
	vector <cWordMatch>::iterator im = m.begin(), imEnd = m.end();
	for (int I = 0; im != imEnd; im++, I++)
	{
		if (wordIdsAndMainEntryIdsInSourceNeedingWordRelations.insert(im->word->second.index).second)
			im->word->second.clearRelationMaps();
		tIWMM ME = resolveToClass(I);
		if (ME->first.length() != 1)
		{
			if (wordIdsAndMainEntryIdsInSourceNeedingWordRelations.insert(ME->second.index).second)
				ME->second.clearRelationMaps();
		}
	}
	return 0;
}

// LOCK wordRelationsMemory READ, then SELECT relations for 'wordIds' in
// QUERY_BUFFER_LEN batches.  inSourceFlagSet==true: only fromWordId IN (...);
// false: fromWordId OR toWordId (needed when the counterpart is outside the
// source).  Each row is attached to both endpoints with complementary types.
// Returns 0, or -1 if LOCK/SELECT fails (UNLOCK is issued on the SELECT miss).
// mysql is taken by value (a copy of the handle).
int cWord::initializeWordRelationsFromDB(MYSQL mysql, set <int> wordIds, bool inSourceFlagSet, bool log)
{
	int startTime = clock();
	size_t wrRead = 0, wrAdded = 0, totalWRIDs = wordIds.size(), numWordsProcessed = 0;
	int64_t lastProgressPercent = -1, where;
	if (totalWRIDs > 10)
		lp_wprintf(u"Waiting for lock on word relations for %zu words...                       \r", totalWRIDs);
	if (!myquery(&mysql, u"LOCK TABLES wordRelationsMemory READ")) return -1;
	if (totalWRIDs > 10)
		lp_wprintf(u"Acquired read lock.  Refreshing word relations for %zu words...                       \r", totalWRIDs);
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	/* // this 'optimization' increased time from 19 seconds to 81 seconds
	// set all ct bits in wordRelations to false
	if (!myquery(&mysql,u"UPDATE wordRelations set ct=false")) return -1;
	*/
	// read all wordRelations for words that have never had their wordRelations read
	//int justMySqlTime = 0; performance testing
	set <int>::iterator wi = wordIds.begin();
	while (wi != wordIds.end())
	{
		int len = 0;
		int64_t wrSubRead = 0;
		for (; wi != wordIds.end() && len < QUERY_BUFFER_LEN; wi++, numWordsProcessed++)
			len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u"%d,", *wi);
		if (!len) break;
		qt[len - 1] = 0; //erase last ,
		if (log)
			lplog(LOG_DICTIONARY, u"Refreshing word relations of wordIds %s", qt);
		//lpwstring sqt="select fromWordId, toWordId, totalCount, typeId from wordRelations where fromWordId in (" + lpwstring(qt) + ") and ct=true";
		lpwstring sqt;
		if (inSourceFlagSet)
			sqt = u"select id, sourceId, lastWhere, fromWordId, toWordId, totalCount, typeId from wordRelationsMemory where fromWordId in (" + lpwstring(qt) + u") AND typeId in (0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36)";
		else
			sqt = u"select id, sourceId, lastWhere, fromWordId, toWordId, totalCount, typeId from wordRelationsMemory where (fromWordId in (" + lpwstring(qt) + u") OR toWordId in (" + lpwstring(qt) + u")) AND typeId in (0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36)";
		//if (I==totalWRIDs) // actually makes query take 50% longer
		//  sqt+=" AND toWordId in (" + lpwstring(qt) + ")";
		//int tempSQLTime = clock(); performance testing
		MYSQL_RES* result = NULL;
		if (!myquery(&mysql, (lpchar_t*)sqt.c_str(), result))
		{
			myquery(&mysql, u"UNLOCK TABLES");
			return -1;
		}
		int64_t numRows = mysql_num_rows(result);
		// justMySqlTime += clock() - tempSQLTime; performance testing
		MYSQL_ROW sqlrow;
		while (true)
		{
			//tempSQLTime = clock();
			if ((sqlrow = mysql_fetch_row(result)) == NULL)
				break;
			// justMySqlTime += clock() - tempSQLTime; performance testing
			// testWordRelations.push_back(cTestWordRelation(atoi(sqlrow[0]), atoi(sqlrow[1]), atoi(sqlrow[2]), atoi(sqlrow[3]), atoi(sqlrow[4]), atoi(sqlrow[5]), atoi(sqlrow[6]))); performance testing
			bool isNew;
			int fromId = atoi(sqlrow[3]), toId = atoi(sqlrow[4]);
			tIWMM iFromWord = wordStructureGivenWordIdExists(fromId), iToWord = wordStructureGivenWordIdExists(toId);
			if (iFromWord != wNULL && iToWord != wNULL)
			{
				// CHECK
				// logCache = 0;
				// lplog(LOG_INFO, u"toWord=%s fromWord=%s", iFromWord->first.c_str(), iToWord->first.c_str());
				wrRead++;
				int relationSourceId = atoi(sqlrow[1]), lastWhere = atoi(sqlrow[2]), totalCount = atoi(sqlrow[5]), wordRelation = atoi(sqlrow[6]); // index = atoi(sqlrow[0]), 
				//if (logDetail)
				//	lplog(LOG_DICTIONARY, u"word %s acquired a relation %s with %s.", iFromWord->first.c_str(), getRelStr(wordRelation), iToWord->first.c_str());
				iFromWord->second.allocateMap(wordRelation);
				iFromWord->second.relationMaps[wordRelation]->addRelation(relationSourceId, lastWhere, iToWord, isNew, totalCount, true);
				wordRelation = getComplementaryRelationship((relationWOTypes)wordRelation);
				iToWord->second.allocateMap(wordRelation);
				iToWord->second.relationMaps[wordRelation]->addRelation(relationSourceId, lastWhere, iFromWord, isNew, totalCount, true);
				wrAdded += 2;
			}
			if ((where = wrSubRead++ * 100 * numWordsProcessed / (numRows * totalWRIDs)) > lastProgressPercent)
			{
				if (totalWRIDs > 10)
					lp_wprintf(u"PROGRESS: %03I64d%% word relations read with %04d seconds elapsed                    \r", where, clocksec());
				lastProgressPercent = where;
			}
		}
		mysql_free_result(result);
	}
	//for (vector <cWordMatch>::iterator im = m.begin(), imEnd = m.end(); im != imEnd; im++)
	//	im->getMainEntry()->second.flags &= ~cSourceWordInfo::inSourceFlag;
	if ((wrRead || wrAdded) && logDatabaseDetails)
		lplog(u"Reading %d (%d added) word relations took %d seconds.", wrRead, wrAdded, (int)((clock() - startTime) / CLOCKS_PER_SEC));
	if (totalWRIDs > 10)
		lp_wprintf(u"PROGRESS: 100%% word relations read with %04d seconds elapsed                    \n", clocksec());
	myquery(&mysql, u"UNLOCK TABLES");
	return 0;
}

