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

#define NULLWORD 187
bool checkFull(MYSQL *mysql,wchar_t *qt,size_t &len,bool flush,wchar_t *qualifier);

void tFI::allocateMap(int relationType)
{ LFS
  if (relationMaps[relationType]==NULL)
    relationMaps[relationType]=new cRMap;
}

bool deltaFrequencyCompare(const tFI::cRMap::tIcRMap &lhs,const tFI::cRMap::tIcRMap &rhs)
{ LFS
  return lhs->second.deltaFrequency<rhs->second.deltaFrequency;
}

// read word DB indexes for mainEntries of words not already having indexes and belonging to the current source.
// return a set of wordIds for the mainEntries of words belonging to the current source and not previously had wordRelations refreshed.
int Source::readWordIndexesForWordRelationsRefresh(vector <int> &wordIds)
{ LFS
	for (tIWMM w=Words.begin(),wEnd=Words.end(); w!=wEnd; w++)
	{
		w->second.flags&=~(tFI::wordIndexRead|tFI::inSourceFlag);
		w->second.clearRelationMaps();
	}
  wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
  int len=_snwprintf(qt,QUERY_BUFFER_LEN,L"select id,word from words where word in ("),wordId,originalLen=len;
	tIWMM specials[]={Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION};
	for (unsigned int I=0; I<sizeof(specials)/sizeof(tIWMM); I++)
	{
		len+=_snwprintf(qt+len,QUERY_BUFFER_LEN-len,L"\"%s\",",specials[I]->first.c_str());
		specials[I]->second.flags|=tFI::wordIndexRead| tFI::inSourceFlag;
		specials[I]->second.clearRelationMaps();
	}
  Words.sectionWord->second.flags|=tFI::wordIndexRead;
  tIWMM tWord;
  vector <WordMatch>::iterator im=m.begin(),imEnd=m.end();
  MYSQL_RES *result=NULL;
  MYSQL_ROW sqlrow;
  for (int I=0; im!=imEnd; im++,I++)
  {
    tIWMM ME=resolveToClass(I);
		wstring s=ME->first;
		//int flags= ME->second.flags;
    ME->second.flags|=tFI::inSourceFlag;
		if (ME->first.length() != 1 && !(ME->second.flags&tFI::wordIndexRead) && ME->first.find('\"') == wstring::npos && ME->first.find('?') == wstring::npos)
    {
      ME->second.flags|=tFI::wordIndexRead|tFI::newWordFlag;
      if (ME->second.index>=0)
        wordIds.push_back(ME->second.index);
      else
      {
				int lencode=_snwprintf(qt+len,QUERY_BUFFER_LEN_OVERFLOW-len,L"\"%s\",",ME->first.c_str());
				if (lencode>=0)
					len+=lencode;
				else
				{
					lplog(LOG_ERROR,L"readWordIndexesForWordRelationsRefresh:error getting word %s.",ME->first.c_str());
          qt[len]=0;
				}
        if (len>QUERY_BUFFER_LEN)
        {
          qt[len-1]=')';
          qt[len]=0;
          if (!myquery(&mysql,qt,result)) return -1;
          while ((sqlrow=mysql_fetch_row(result))!=NULL)
          {
            wordIds.push_back(wordId=atoi(sqlrow[0]));
						wstring w;
            if ((tWord=Words.query(mTW(sqlrow[1],w)))!=Words.end())
              Words.assign(wordId,tWord);
          }
          mysql_free_result(result);
          len=originalLen;
        }
      }
    }
  }
  if (qt[len-1]==',')
  {
    qt[len-1]=')';
    if (!myquery(&mysql,qt,result)) return -1;
    while ((sqlrow=mysql_fetch_row(result))!=NULL)
    {
      wordIds.push_back(wordId=atoi(sqlrow[0]));
			wstring w;
      if ((tWord=Words.query(mTW(sqlrow[1],w)))!=Words.end())
        Words.assign(wordId,tWord);
    }
    mysql_free_result(result);
  }
  return 0;
}

// this is for performance testing between MySQL and Solr
void compareTestRelations(vector <Source::testWordRelation> testWordRelations, vector <Source::testWordRelation> testWordRelationsFromSolr)
{
	vector <Source::testWordRelation> wordRelationsNotFoundInSolr;
	for (Source::testWordRelation tr : testWordRelations)
	{
		if (std::find(testWordRelationsFromSolr.begin(), testWordRelationsFromSolr.end(), tr) == testWordRelationsFromSolr.end())
			wordRelationsNotFoundInSolr.push_back(tr);
	}
	if (wordRelationsNotFoundInSolr.size() > 0 || testWordRelations.size()!= testWordRelationsFromSolr.size())
		lplog(LOG_FATAL_ERROR, L"Word relations No match!");
}

// this is called immediately after reading in words from text (tokenization), before parsing and costing.
// read word relations for only the words in the current source that have not been read before.
int Source::refreshWordRelations()
{
	if (preTaggedSource) return 0; // we use BNC to construct word relations in the first place
	LFS
	// find index ids for all words not having been refreshed previously and belonging ONLY to the current source
	wprintf(L"Reading word indexes...                       \r");
	vector <int> wordIds;
	if (!myquery(&mysql, L"LOCK TABLES words READ")) return -1;
	readWordIndexesForWordRelationsRefresh(wordIds);
	myquery(&mysql, L"UNLOCK TABLES");
	//vector <testWordRelation> testWordRelations; performance testing
	int startTime = clock();
	size_t I = 0, wrRead = 0, wrAdded = 0, totalWRIDs = wordIds.size();
	__int64 lastProgressPercent = -1, where;
	wprintf(L"Waiting for lock on word relations for %zu words...                       \r", wordIds.size());
	if (!myquery(&mysql, L"LOCK TABLES wordRelationsMemory READ")) return -1; 
	wprintf(L"Acquired read lock.  Refreshing word relations for %zu words...                       \r", wordIds.size());
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	/* // this 'optimization' increased time from 19 seconds to 81 seconds
	// set all ct bits in wordRelations to false
	if (!myquery(&mysql,L"UPDATE wordRelations set ct=false")) return -1;
	// set all ct bits for the words we are interested in wordRelations to true
	while (I<totalWRIDs)
	{
	  int len=0;
	  for (; I<totalWRIDs && len<QUERY_BUFFER_LEN; I++)
		len+=_snwprintf(qt+len,QUERY_BUFFER_LEN-len,L"%d,",wordIds[I]);
	  if (!len) break;
	  qt[len-1]=0; //erase last ,
	  wstring sqt="update wordRelations set ct=true where toWordId in (" + wstring(qt) + ")";
	  if (!myquery(&mysql,(wchar_t *)sqt.c_str())) return -1;
	}
	*/
	// read all wordRelations for words that have never had their wordRelations read
	// do not try to update wordRelations for those words that already have read their wordRelations.
	//   the assumption is that the deltaWordRelations from other sources will not be great enough to justify the expense.
	//int justMySqlTime = 0; performance testing
		I = 0;
	while (I < totalWRIDs)
	{
		int len = 0;
		__int64 wrSubRead = 0;
		for (; I < totalWRIDs && len < QUERY_BUFFER_LEN; I++)
			len += _snwprintf(qt + len, QUERY_BUFFER_LEN - len, L"%d,", wordIds[I]);
		if (!len) break;
		qt[len - 1] = 0; //erase last ,
		//wstring sqt="select fromWordId, toWordId, totalCount, typeId from wordRelations where fromWordId in (" + wstring(qt) + ") and ct=true";
		wstring sqt = L"select id, sourceId, lastWhere, fromWordId, toWordId, totalCount, typeId from wordRelationsMemory where fromWordId in (" + wstring(qt) + L") AND typeId in (0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36)";
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
			// testWordRelations.push_back(testWordRelation(atoi(sqlrow[0]), atoi(sqlrow[1]), atoi(sqlrow[2]), atoi(sqlrow[3]), atoi(sqlrow[4]), atoi(sqlrow[5]), atoi(sqlrow[6]))); performance testing
			bool isNew;
			int fromId = atoi(sqlrow[3]), toId = atoi(sqlrow[4]);
			tIWMM iFromWord, iToWord;
			if (fromId >= Words.idsAllocated || (iFromWord = Words.idToMap[fromId]) == wNULL)
				lplog(LOG_ERROR, L"Unknown 'from' word id %d encountered reading word relations", fromId);
			else if (toId < Words.idsAllocated && (iToWord = Words.idToMap[toId]) != wNULL && (iToWord->second.flags&tFI::inSourceFlag))
			{
				wrRead++;
				int relationSourceId = atoi(sqlrow[1]), lastWhere = atoi(sqlrow[2]), totalCount = atoi(sqlrow[5]), relation = atoi(sqlrow[6]); // index = atoi(sqlrow[0]), 
				iFromWord->second.allocateMap(relation);
				iFromWord->second.relationMaps[relation]->addRelation(relationSourceId, lastWhere, iToWord, isNew, totalCount, true);
				relation = getComplementaryRelationship((relationWOTypes)relation);
				iToWord->second.allocateMap(relation);
				iToWord->second.relationMaps[relation]->addRelation(relationSourceId, lastWhere, iFromWord, isNew, totalCount, true);
				wrAdded += 2;
			}
			if ((where = wrSubRead++ * 100 * I / (numRows*totalWRIDs)) > lastProgressPercent)
			{
				wprintf(L"PROGRESS: %03I64d%% word relations read with %04d seconds elapsed                    \r", where, clocksec());
				lastProgressPercent = where;
			}
		}
		mysql_free_result(result);
	}
	for (vector <WordMatch>::iterator im = m.begin(), imEnd = m.end(); im != imEnd; im++)
		im->getMainEntry()->second.flags &= ~tFI::inSourceFlag;
	if ((wrRead || wrAdded) && logDatabaseDetails)
		lplog(L"Reading %d (%d added) word relations took %d seconds.", wrRead, wrAdded, (int)((clock() - startTime) / CLOCKS_PER_SEC));
	wprintf(L"PROGRESS: 100%% word relations read with %04d seconds elapsed                    \n", clocksec());
	myquery(&mysql, L"UNLOCK TABLES");  
	return 0;
}

