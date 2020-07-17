#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include "WinInet.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "time.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "vcXML.h"
#include "profile.h"
#include "mysqldb.h"
#include "internet.h"
#include "questionAnswering.h"

#define NULLWORD 187
bool checkFull(MYSQL *mysql,wchar_t *qt,size_t &len,bool flush,wchar_t *qualifier);

void cSourceWordInfo::allocateMap(int relationType)
{ LFS
  if (relationMaps[relationType]==NULL)
    relationMaps[relationType]=new cRMap;
}

// main entries from all words in source must already exist in memory (Words array)
// return a set of wordIds for all words including the mainEntries, except for special words that do not already have wordRelations
int cSource::readWordIdsNeedingWordRelations(set <int> &wordIdsAndMainEntryIdsInSourceNeedingWordRelations)
	{
	LFS
	tIWMM specials[] = { Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION,Words.sectionWord };
	for (unsigned int I=0; I<sizeof(specials)/sizeof(tIWMM); I++)
	{
		wordIdsAndMainEntryIdsInSourceNeedingWordRelations.insert(specials[I]->second.index);
		specials[I]->second.clearRelationMaps();
	}
  tIWMM tWord;
  vector <cWordMatch>::iterator im=m.begin(),imEnd=m.end();
  for (int I=0; im!=imEnd; im++,I++)
  {
		if (wordIdsAndMainEntryIdsInSourceNeedingWordRelations.insert(im->word->second.index).second)
			im->word->second.clearRelationMaps();
    tIWMM ME=resolveToClass(I);
		if (ME->first.length() != 1)
		{
			if (wordIdsAndMainEntryIdsInSourceNeedingWordRelations.insert(ME->second.index).second)
				ME->second.clearRelationMaps();
		}
  }
  return 0;
}

int cWord::initializeWordRelationsFromDB(MYSQL mysql, set <int> wordIds, bool inSourceFlagSet,bool log)
{
	int startTime = clock();
	size_t wrRead = 0, wrAdded = 0, totalWRIDs = wordIds.size(),numWordsProcessed=0;
	__int64 lastProgressPercent = -1, where;
	if (totalWRIDs > 10)
		wprintf(L"Waiting for lock on word relations for %zu words...                       \r", totalWRIDs);
	if (!myquery(&mysql, L"LOCK TABLES wordRelationsMemory READ")) return -1; 
	if (totalWRIDs > 10)
		wprintf(L"Acquired read lock.  Refreshing word relations for %zu words...                       \r", totalWRIDs);
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	/* // this 'optimization' increased time from 19 seconds to 81 seconds
	// set all ct bits in wordRelations to false
	if (!myquery(&mysql,L"UPDATE wordRelations set ct=false")) return -1;
	*/
	// read all wordRelations for words that have never had their wordRelations read
	//int justMySqlTime = 0; performance testing
	set <int>::iterator wi = wordIds.begin();
	while (wi != wordIds.end())
	{
		int len = 0;
		__int64 wrSubRead = 0;
		for (; wi != wordIds.end() && len < QUERY_BUFFER_LEN; wi++, numWordsProcessed++)
			len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L"%d,", *wi);
		if (!len) break;
		qt[len - 1] = 0; //erase last ,
		if (log)
			lplog(LOG_DICTIONARY, L"Refreshing word relations of wordIds %s", qt);
		//wstring sqt="select fromWordId, toWordId, totalCount, typeId from wordRelations where fromWordId in (" + wstring(qt) + ") and ct=true";
		wstring sqt;
		if (inSourceFlagSet)
			sqt = L"select id, sourceId, lastWhere, fromWordId, toWordId, totalCount, typeId from wordRelationsMemory where fromWordId in (" + wstring(qt) + L") AND typeId in (0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36)";
		else
			sqt = L"select id, sourceId, lastWhere, fromWordId, toWordId, totalCount, typeId from wordRelationsMemory where (fromWordId in (" + wstring(qt) + L") OR toWordId in (" + wstring(qt) + L")) AND typeId in (0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36)";
		//if (I==totalWRIDs) // actually makes query take 50% longer
		//  sqt+=" AND toWordId in (" + wstring(qt) + ")";
		//int tempSQLTime = clock(); performance testing
		MYSQL_RES *result = NULL;
		if (!myquery(&mysql, (wchar_t *)sqt.c_str(), result))
		{
			myquery(&mysql, L"UNLOCK TABLES");
			return -1;
		}
		__int64 numRows = mysql_num_rows(result);
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
			tIWMM iFromWord= wordStructureGivenWordIdExists(fromId), iToWord= wordStructureGivenWordIdExists(toId);
			if (iFromWord != wNULL && iToWord != wNULL)
			{
				// CHECK
				// logCache = 0;
				// lplog(LOG_INFO, L"toWord=%s fromWord=%s", iFromWord->first.c_str(), iToWord->first.c_str());
				wrRead++;
				int relationSourceId = atoi(sqlrow[1]), lastWhere = atoi(sqlrow[2]), totalCount = atoi(sqlrow[5]), wordRelation = atoi(sqlrow[6]); // index = atoi(sqlrow[0]), 
				//if (logDetail)
				//	lplog(LOG_DICTIONARY, L"word %s acquired a relation %s with %s.", iFromWord->first.c_str(), getRelStr(wordRelation), iToWord->first.c_str());
				iFromWord->second.allocateMap(wordRelation);
				iFromWord->second.relationMaps[wordRelation]->addRelation(relationSourceId, lastWhere, iToWord, isNew, totalCount, true);
				wordRelation = getComplementaryRelationship((relationWOTypes)wordRelation);
				iToWord->second.allocateMap(wordRelation);
				iToWord->second.relationMaps[wordRelation]->addRelation(relationSourceId, lastWhere, iFromWord, isNew, totalCount, true);
				wrAdded += 2;
			}
			if ((where = wrSubRead++ * 100 * numWordsProcessed / (numRows*totalWRIDs)) > lastProgressPercent)
			{
				if (totalWRIDs > 10)
					wprintf(L"PROGRESS: %03I64d%% word relations read with %04d seconds elapsed                    \r", where, clocksec());
				lastProgressPercent = where;
			}
		}
		mysql_free_result(result);
	}
	//for (vector <cWordMatch>::iterator im = m.begin(), imEnd = m.end(); im != imEnd; im++)
	//	im->getMainEntry()->second.flags &= ~cSourceWordInfo::inSourceFlag;
	if ((wrRead || wrAdded) && logDatabaseDetails)
		lplog(L"Reading %d (%d added) word relations took %d seconds.", wrRead, wrAdded, (int)((clock() - startTime) / CLOCKS_PER_SEC));
	if (totalWRIDs > 10)
		wprintf(L"PROGRESS: 100%% word relations read with %04d seconds elapsed                    \n", clocksec());
	myquery(&mysql, L"UNLOCK TABLES");  
	return 0;
}

