#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include "WinInet.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
//#include <winsock.h>
//#include "Winhttp.h"
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

/*
void WordClass::updateWordRelationIndexesFromDB(MYSQL &mysql,int lastReadfromDBTime,int sourceId)
{ LFS
  int startTime=clock(),wrUpdated=0;
  wchar_t nqt[256];
  MYSQL_ROW sqlrow;
  MYSQL_RES *result=NULL;
	// test # with timestamp 
	int numTimestampRows=-1;
	_snwprintf(nqt,256,L"SELECT COUNT(sourceId) from wordRelations where UNIX_TIMESTAMP(ts)>=%d",lastReadfromDBTime);
	if (!myquery(&mysql,nqt,result)) return;
  if ((sqlrow=mysql_fetch_row(result))!=NULL)
		numTimestampRows=atoi(sqlrow[0]);
	int numSourceRows=-1;
	_snwprintf(nqt,256,L"SELECT COUNT(sourceId) from wordRelations where sourceId=%d",sourceId);
	if (!myquery(&mysql,nqt,result)) return;
  if ((sqlrow=mysql_fetch_row(result))!=NULL)
		numSourceRows=atoi(sqlrow[0]);
	if (numSourceRows!=numTimestampRows)
		lplog(LOG_ERROR,L"numSourceRows!=numTimestampRows! %d %d",numSourceRows,numTimestampRows);
	else
		lplog(LOG_INFO,L"numSourceRows=numTimestampRows %d %d",numSourceRows,numTimestampRows);
  // get ids of everything we just inserted
	_snwprintf(nqt,256,L"SELECT id, sourceId, lastWhere, totalCount, fromWordId, toWordId, typeId from wordRelations where UNIX_TIMESTAMP(ts)>=%d",lastReadfromDBTime);
	if (!myquery(&mysql,nqt,result)) return;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
		int fromId=atoi(sqlrow[4]),toId=atoi(sqlrow[5]);
		if (fromId>=idsAllocated)
      lplog(LOG_ERROR,L"Unknown 'from' word id %d encountered reading word relations",fromId);
		else if (toId>=idsAllocated)
			lplog(LOG_ERROR,L"Unknown 'to' word id %d encountered reading word relations",toId);
		else
		{
			tIWMM iFromWord=idToMap[fromId],iToWord=idToMap[toId];
			if (iFromWord==wNULL)
				lplog(LOG_ERROR,L"Unknown 'from' word id %d encountered reading word relations",fromId);
			else if (iToWord==wNULL)
				lplog(LOG_ERROR,L"Unknown 'to' word id %d encountered reading word relations from word %s",toId,iFromWord->first.c_str());
			else
			{
				wrUpdated++;
				int relationType=atoi(sqlrow[6]);
				iFromWord->second.allocateMap(relationType);
				//iFromWord->second.relationMaps[relationType]->r[iToWord].index=atoi(sqlrow[0]);
				iFromWord->second.relationMaps[relationType]->r[iToWord].sourceId=atoi(sqlrow[1]);
				iFromWord->second.relationMaps[relationType]->r[iToWord].rlastWhere=atoi(sqlrow[2]);
				iFromWord->second.relationMaps[relationType]->r[iToWord].frequency=atoi(sqlrow[3]);
			}
		}
  }
  mysql_free_result(result);
  if (wrUpdated && logDatabaseDetails)
    lplog(L"Updating %d word relations from DB took %d seconds.",wrUpdated,(clock()-startTime)/CLOCKS_PER_SEC);
}
*/

int WordClass::getNumWordRelationsToWrite(void)
{ LFS
  int totalWordRelationsToWrite=0;
  for (tIWMM w=begin(),wEnd=end(); w!=wEnd && !exitNow; w++)
    if (w->second.index>0 && w->second.changedSinceLastWordRelationFlush)
      for (int rType=0; rType<numRelationWOTypes; rType++)
        if (w->second.relationMaps[rType])
          for (tFI::cRMap::tIcRMap r=w->second.relationMaps[rType]->r.begin(),rEnd=w->second.relationMaps[rType]->r.end(); r!=rEnd; r++)
            if (r->second.deltaFrequency)
              totalWordRelationsToWrite++;
  return totalWordRelationsToWrite;
}

bool deltaFrequencyCompare(const tFI::cRMap::tIcRMap &lhs,const tFI::cRMap::tIcRMap &rhs)
{ LFS
  return lhs->second.deltaFrequency<rhs->second.deltaFrequency;
}

// update the database for all relations that have deltaFrequencies (changed since originally being read or
//   having this function run) AND that have relation indexes so that we know they exist in the DB
// this function is called by flushWordRelations
/* removed because this requires a read from the DB to keep indexes in sync.  This was causing problems because the indexes were unexpectedly out of sync during
//  multiple sources writing to the DB simultaneously.  Much simpler to just write to the DB all the time and never read.
int WordClass::updateDBWordRelations(MYSQL &mysql,int totalWordRelationsToWrite)
{ LFS
  int overallStartTime=clock(),overallRelationsUpdated=0;
  vector <tFI::cRMap::tIcRMap> ur;
  ur.reserve(totalWordRelationsToWrite);
  for (tIWMM w=begin(),wEnd=end(); w!=wEnd && !exitNow; w++)
    if (w->second.index>0 && w->second.changedSinceLastWordRelationFlush)
      for (int rType=0; rType<numRelationWOTypes; rType++)
        if (w->second.relationMaps[rType])
          for (tFI::cRMap::tIcRMap r=w->second.relationMaps[rType]->r.begin(),rEnd=w->second.relationMaps[rType]->r.end(); r!=rEnd; r++)
            if (r->second.index>=0 && r->second.deltaFrequency)
              ur.push_back(r);
  sort(ur.begin(),ur.end(),deltaFrequencyCompare);
  int startTime=clock(),relationsUpdated=0,where,lastProgressPercent=-1,currentDelta=-1;
  wchar_t nqt[query_buffer_len_overflow];
  size_t nlen=0;
  for (vector<tFI::cRMap::tIcRMap>::iterator uri=ur.begin(),uriEnd=ur.end(); uri!=uriEnd && !exitNow; uri++)
  {
    if ((*uri)->second.deltaFrequency!=currentDelta)
    {
      if (relationsUpdated && !checkFull(&mysql,nqt,nlen,true,NULL)) return -1;
      if ((clock()-startTime)>CLOCKS_PER_SEC && logDatabaseDetails)
        lplog(L"Updating %d relations (delta %d) took %d seconds.",relationsUpdated,currentDelta,(clock()-startTime)/CLOCKS_PER_SEC);
      startTime=clock();
			_snwprintf(nqt,query_buffer_len,L"UPDATE wordRelations SET totalCount=totalCount+%d WHERE id in (",(*uri)->second.deltaFrequency);
      nlen=wcslen(nqt);
      relationsUpdated=0;
      currentDelta=(*uri)->second.deltaFrequency;
    }
    nlen+=_snwprintf(nqt+nlen,query_buffer_len-nlen,L"%d,",(*uri)->second.index);
    (*uri)->second.deltaFrequency=0;
    relationsUpdated++;
    overallRelationsUpdated++;
    if ((where=overallRelationsUpdated*100/totalWordRelationsToWrite)>lastProgressPercent)
    {
      wprintf(L"PROGRESS: %03d%% word relations flushed with %04d seconds elapsed \r",where,clocksec());
      lastProgressPercent=where;
    }
    if (!checkFull(&mysql,nqt,nlen,false,NULL)) return -1;
  }
  if (relationsUpdated && !checkFull(&mysql,nqt,nlen,true,NULL)) return -1;
  if ((clock()-startTime)>CLOCKS_PER_SEC && logDatabaseDetails)
      lplog(L"Updating %d relations (delta %d) took %d seconds.",relationsUpdated,currentDelta,(clock()-startTime)/CLOCKS_PER_SEC);
	startTime=clock();
	lastProgressPercent=-1;
  wcscpy(nqt,L"INSERT IGNORE into wordRelations (id, sourceId, lastWhere) VALUES ");
  //wchar_t dupqt[128]; // may overwrite sourceId=0, which is needed by gquery in getDictionary (all sources)
  //_snwprintf(dupqt,128,L" ON DUPLICATE KEY UPDATE sourceId=VALUES(sourceId),lastWhere=VALUES(lastWhere)");
	nlen=wcslen(nqt);
  for (vector<tFI::cRMap::tIcRMap>::iterator uri=ur.begin(),uriEnd=ur.end(); uri!=uriEnd && !exitNow; uri++)
  {
		if ((*uri)->second.index>=0)
			nlen+=_snwprintf(nqt+nlen,query_buffer_len-nlen,L"(%d,%d,%d),",(*uri)->second.index,(*uri)->second.sourceId,(*uri)->second.rlastWhere);
    if (!checkFull(&mysql,nqt,nlen,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,nqt,nlen,true,NULL)) return -1;
  if (overallRelationsUpdated	&& logDatabaseDetails)
	{
	  lplog(L"Updating relations (lastWhere) took %d ms.",(clock()-startTime));
		lplog(L"Updating %d relations overall took %d ms.",overallRelationsUpdated,clock()-overallStartTime);
	}
  return overallRelationsUpdated;
}

// this assumes all wordRelations have correct indexes (not unknown)
int	Source::flushWordRelationsHistory(MYSQL &mysql)
{ LFS
    int startTime=clock(),relationHistoriesInserted=0;
    wchar_t nqt[query_buffer_len_overflow];
		wcscpy(nqt,L"INSERT into wordRelationsHistory (wordRelationId, sourceId, w, narrativeNum, genericLocation, associatedSubject, flags) VALUES ");
    size_t nlen=wcslen(nqt);
		for (vector <cRelationHistory>::iterator rhi=relationHistory.begin(),rhiEnd=relationHistory.end(); rhi!=rhiEnd && !exitNow; rhi++)
			if (rhi->r->second.index>=0)
      {
				nlen+=_snwprintf(nqt+nlen,query_buffer_len-nlen,L"(%d,%d,%u,%d,%d,%d,%d),",rhi->r->second.index,sourceId,rhi->toWhere, rhi->narrativeNum,NULL,rhi->subject,rhi->flags);
				relationHistoriesInserted++;
				if (!checkFull(&mysql,nqt,nlen,false,NULL)) 
					return -1;
      }
			else
				lplog(LOG_ERROR,L"Missing index for word relation %d,%d:%s - %d.",rhi->r,rhi->toWhere,rhi->r->first->first.c_str(),rhi->r->second.deltaFrequency);
		if (!checkFull(&mysql,nqt,nlen,true,NULL)) 
			return -1;
		_snwprintf(nqt,query_buffer_len,L"UPDATE sources SET numWordRelations=%d where id=%d", relationHistoriesInserted,sourceId);
		relationHistory.clear();
    if ((clock()-startTime)>CLOCKS_PER_SEC && logDatabaseDetails) // relationsInserted &&
      lplog(L"Inserting %d relation history took %d seconds.",relationHistoriesInserted,(clock()-startTime)/CLOCKS_PER_SEC);
	  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return 0;
		myquery(&mysql,nqt);
	  if (!myquery(&mysql,L"UNLOCK TABLES")) return 0;
		return 0;
}
*/

// write all wordrelations to the DB.  After this function, all deltaFrequencies should be zero and
// all wordrelations in memory should have indexes.
int WordClass::flushWordRelations(MYSQL &mysql)
{ LFS
	tIWMM specials[]={Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION};
  for (unsigned int I=0; I<sizeof(specials)/sizeof(tIWMM); I++)
    specials[I]->second.changedSinceLastWordRelationFlush=true;
	#ifdef WRITE_WORDS_AND_FORMS_TO_DB
		if (!acquireLock(mysql,false)) return 0;
	#endif
  int overallStartTime=clock();
	if (!myquery(&mysql,L"LOCK TABLES wordRelations WRITE")) return -1;
  wprintf(L"Loading word relations index...\r");
  if (logDatabaseDetails)
    lplog(L"Inserting relations overall start...");
  MYSQL_RES *result=NULL;
	if (!myquery(&mysql,L"LOAD INDEX INTO CACHE wordRelations",result)) 
	{
		myquery(&mysql,L"UNLOCK TABLES");
		return -1;
	}
  MYSQL_ROW sqlrow=mysql_fetch_row(result);
  if (sqlrow==NULL || sqlrow[3]==NULL || string(sqlrow[3])=="OK")
    lplog(LOG_FATAL_ERROR,L"Loading wordRelations index failed -- MyISAM Engine not mandatory?");
  mysql_free_result(result);
	if (!myquery(&mysql,L"select UNIX_TIMESTAMP(NOW())",result)) 
	{
		myquery(&mysql,L"UNLOCK TABLES");
		return -1;
	}
  if ((sqlrow=mysql_fetch_row(result))==NULL || sqlrow[0]==NULL) lplog(LOG_FATAL_ERROR,L"wordRelations find time returned NULL!");
  //int lastReadfromDBTime=atoi(sqlrow[0]);
  mysql_free_result(result);
  wprintf(L"Writing word relations to database...\r");
  int totalWordRelationsToWrite=getNumWordRelationsToWrite();
	int overallRelationsInserted=0;
  // update all relations that already exist in the DB using update (faster)
  // int overallRelationsInserted=updateDBWordRelations(mysql,totalWordRelationsToWrite);
  // update all relations that probably don't exist in the DB by inserting them (although still use DUPLICATE KEY UPDATE if another instance
  // has updated wordRelations in the meantime)
  int minNextDelta=1;
  while (minNextDelta<1000000 && !exitNow)
  {
    int startTime=clock(),relationsInserted=0,where,lastProgressPercent=-1;
    wchar_t nqt[query_buffer_len_overflow];
		wcscpy(nqt,L"INSERT into wordRelations (fromWordId, toWordId, sourceId, lastWhere, totalCount, typeId) VALUES ");
    wchar_t dupqt[128];
    _snwprintf(dupqt,128,L" ON DUPLICATE KEY UPDATE totalCount=totalCount+%d, sourceId=VALUES(sourceId), lastWhere=VALUES(lastWhere)",minNextDelta);
    size_t nlen=wcslen(nqt);
    int currentDelta=minNextDelta;
    minNextDelta=1000000;
    for (tIWMM w=begin(),wEnd=end(); w!=wEnd && !exitNow; w++)
      if (w->second.index>0 && w->second.changedSinceLastWordRelationFlush)
      {
        bool stillFlush=false;
        for (int rType=0; rType<numRelationWOTypes; rType++)
          if (w->second.relationMaps[rType])
            for (tFI::cRMap::tIcRMap r=w->second.relationMaps[rType]->r.begin(),rEnd=w->second.relationMaps[rType]->r.end(); r!=rEnd; r++)
            {
              if (r->second.deltaFrequency==currentDelta)
              {
                if (r->first->second.index<0 || r->second.sourceId<0)
										//lplog(L"ERROR! word %s in relations type %s to word %s has an index of -1!",w->first.c_str(),getRelStr(rType),r->first->first.c_str());
										lplog(LOG_ERROR,L"flushWordRelations:word %s has an index of %d and a sourceId of %d!",r->first->first.c_str(),r->first->second.index,r->second.sourceId);
                else
                {
									nlen+=_snwprintf(nqt+nlen,query_buffer_len-nlen,L"(%d,%d,%d,%u,%d,%d),",w->second.index,r->first->second.index,r->second.sourceId, r->second.rlastWhere, r->second.deltaFrequency,rType);
                  r->second.deltaFrequency=0;
                  relationsInserted++;
                  overallRelationsInserted++;
                  if ((where=overallRelationsInserted*100/totalWordRelationsToWrite)>lastProgressPercent)
                  {
                    wprintf(L"PROGRESS: %03d%% word relations flushed with %04d seconds elapsed \r",where,clocksec());
                    lastProgressPercent=where;
                  }
                }
              }
              else
              {
                stillFlush|=(r->second.deltaFrequency>0);
                if (r->second.deltaFrequency>currentDelta)
                  minNextDelta=min(minNextDelta,r->second.deltaFrequency);
              }
							if (!checkFull(&mysql,nqt,nlen,false,dupqt)) 
								return -1;
            }
        if (!stillFlush)
          w->second.changedSinceLastWordRelationFlush=false;
      }
		if (!checkFull(&mysql,nqt,nlen,true,dupqt)) 
			return -1;
    if ((clock()-startTime)>CLOCKS_PER_SEC && logDatabaseDetails) // relationsInserted &&
      lplog(L"Inserting %d relations (delta %d) took %d seconds.",relationsInserted,currentDelta,(clock()-startTime)/CLOCKS_PER_SEC);
  }
  for (tIWMM w=begin(),wEnd=end(); w!=wEnd && !exitNow; w++)
	{
		if (w->second.changedSinceLastWordRelationFlush)
			lplog(LOG_ERROR,L"word id %d still has wordRelations unflushed.",w->second.index);
    w->second.changedSinceLastWordRelationFlush=false;
	}
  if (overallRelationsInserted && logDatabaseDetails)
    lplog(L"Inserting %d relations overall took %d seconds.",overallRelationsInserted,(clock()-overallStartTime)/CLOCKS_PER_SEC);
	//updateWordRelationIndexesFromDB(mysql,lastReadfromDBTime,sourceId);
  myquery(&mysql,L"UNLOCK TABLES");
	#ifdef WRITE_WORDS_AND_FORMS_TO_DB
		releaseLock(mysql);
  #endif
  wprintf(L"PROGRESS: 100%% word relations flushed with %04d seconds elapsed \n",clocksec());
  return 0;
}

// read word DB indexes for mainEntries of words not already having indexes and belonging to the current source.
// return a set of wordIds for the mainEntries of words belonging to the current source and not previously had wordRelations refreshed.
int Source::readWordIndexesForWordRelationsRefresh(vector <int> &wordIds)
{ LFS
	for (tIWMM w=Words.begin(),wEnd=Words.end(); w!=wEnd; w++)
	{
		w->second.flags&=~tFI::wordIndexRead;
		w->second.clearRelationMaps();
	}
  wchar_t qt[query_buffer_len_overflow];
  int len=_snwprintf(qt,query_buffer_len,L"select id,word from words where word in ("),wordId,originalLen=len;
	tIWMM specials[]={Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION};
	for (unsigned int I=0; I<sizeof(specials)/sizeof(tIWMM); I++)
	{
		len+=_snwprintf(qt+len,query_buffer_len-len,L"\"%s\",",specials[I]->first.c_str());
		specials[I]->second.flags|=tFI::wordIndexRead;
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
				int lencode=_snwprintf(qt+len,query_buffer_len_overflow-len,L"\"%s\",",ME->first.c_str());
				if (lencode>=0)
					len+=lencode;
				else
				{
					lplog(LOG_ERROR,L"readWordIndexesForWordRelationsRefresh:error getting word %s.",ME->first.c_str());
          qt[len]=0;
				}
        if (len>query_buffer_len)
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
	wprintf(L"Refreshing word relations for %zu words...                       \r", wordIds.size());
	if (!myquery(&mysql, L"LOCK TABLES wordRelationsMemory READ")) return -1; 
	wchar_t qt[query_buffer_len_overflow];
	/* // this 'optimization' increased time from 19 seconds to 81 seconds
	// set all ct bits in wordRelations to false
	if (!myquery(&mysql,L"UPDATE wordRelations set ct=false")) return -1;
	// set all ct bits for the words we are interested in wordRelations to true
	while (I<totalWRIDs)
	{
	  int len=0;
	  for (; I<totalWRIDs && len<query_buffer_len; I++)
		len+=_snwprintf(qt+len,query_buffer_len-len,L"%d,",wordIds[I]);
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
		for (; I < totalWRIDs && len < query_buffer_len; I++)
			len += _snwprintf(qt + len, query_buffer_len - len, L"%d,", wordIds[I]);
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
	// the following is for performance testing
	//int dbTime = (int)((clock() - startTime) / CLOCKS_PER_SEC);
	//justMySqlTime /= CLOCKS_PER_SEC;
	//vector <testWordRelation> testWordRelationsFromSolr;
	//int overallRefreshSolrTime=0, justSolrTime=0;
	//refreshWordRelationsFromSolr(testWordRelationsFromSolr, overallRefreshSolrTime, justSolrTime);
	//wprintf(L"Compare: DB: %04d (%04d) seconds elapsed SOLR: %04d (%04d) seconds elapsed\n", dbTime, justMySqlTime, overallRefreshSolrTime, justSolrTime);
	//compareTestRelations(testWordRelations, testWordRelationsFromSolr);
	return 0;
}

// this is for performance testing
extern "C"
{
#include "yajl_tree.h"
}


int getSolrField(char *element, yajl_val doc)
{
	const char * path[] = { element, (const char *)0 };
	yajl_val vid = yajl_tree_get(doc, path, yajl_t_number);
	return (vid != NULL && YAJL_IS_NUMBER(vid)) ? atoi(YAJL_GET_NUMBER(vid)) : -1;
}

#define MAX_BUF 200000

int readPageFromSolr(const wchar_t *queryParams, wstring &buffer)
{
	LFS
	if (!Internet::LPInternetOpen(0))
		return Internet::INTERNET_OPEN_FAILED;
	HINTERNET hHttpSession =InternetConnect(Internet::hINet, L"127.0.0.1", 8983, NULL /* user */, NULL /* password */, INTERNET_SERVICE_HTTP, 0, 0);
	if (hHttpSession == NULL)
	{
		lplog(LOG_ERROR, L"Cannot connect to Solr");
		return -1;
	}
	HINTERNET hHttpRequest = HttpOpenRequest(hHttpSession, L"POST", L"/solr/WordRelations/query", NULL /*http version*/, NULL /*referrer*/, NULL /*(LPCWSTR *)rgpszAcceptTypes*/, 0, 0);
	HttpAddRequestHeaders(hHttpRequest, L"Content-Type: application/json\r\n", -1, HTTP_ADDREQ_FLAG_ADD); // x-www-form-urlencoded
	//lplog(LOG_ERROR, L"%s",queryParams);
	//char qp2[100];
	//strcpy(qp2, "{ params: { q: \"*:*\",rows:1 } }");
	string cParams;
	wTM(queryParams, cParams);
	bool success= HttpSendRequest(hHttpRequest, NULL, 0, (void *)cParams.c_str(), cParams.length());
	if (!success)
	{
		lplog(LOG_ERROR,L"Unable to send request to Solr. Error %ld\n", GetLastError());
		return -1;
	}
	DWORD StatusCode = 0,StatusCodeLen = sizeof(StatusCode);
	HttpQueryInfo(hHttpRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &StatusCode, &StatusCodeLen, NULL);
	if (success && StatusCode == 200)
	{
		DWORD dwRead;
		char cBuffer[MAX_BUF + 4];
		// does InternetReadFile in a child process to prevent lock
		while (Internet::InternetReadFile_Wait(hHttpRequest, cBuffer, MAX_BUF, &dwRead))
		{
			if (dwRead == 0)
				break;
			cBuffer[dwRead] = 0;
			wstring wb;
			buffer += mTW(cBuffer, wb);
		}
	}
	//printf("Response %S\n",buffer.c_str());
	InternetCloseHandle(hHttpRequest);
	InternetCloseHandle(hHttpSession);
	return 0;
}

// this is called immediately after reading in words from text (tokenization), before parsing and costing.
// read word relations for only the words in the current source that have not been read before.
int Source::refreshWordRelationsFromSolr(vector <testWordRelation> &testWordRelations,int &overallTime,int &justSolrTime)
{
	if (preTaggedSource) return 0; // we use BNC to construct word relations in the first place
	LFS
	// find index ids for all words not having been refreshed previously and belonging ONLY to the current source
	wprintf(L"Reading word indexes...                       \r");
	vector <int> wordIds;
	if (!myquery(&mysql, L"LOCK TABLES words READ")) return -1;
	readWordIndexesForWordRelationsRefresh(wordIds);
	myquery(&mysql, L"UNLOCK TABLES");
	int startTime = clock();
	size_t I = 0, wrRead = 0, wrAdded = 0, totalWRIDs = wordIds.size();
	__int64 lastProgressPercent = -1, where;
	wprintf(L"Refreshing word relations for %zu words...                       \r", wordIds.size());
	wchar_t qt[query_buffer_len_overflow];
	// read all wordRelations for words that have never had their wordRelations read
	// do not try to update wordRelations for those words that already have read their wordRelations.
	//   the assumption is that the deltaWordRelations from other sources will not be great enough to justify the expense.
	I = 0;
	while (I < totalWRIDs)
	{
		wcscpy(qt, L"{ params: { q: \"(");
		int solrQueryLength = wcslen(qt);
		for (; I < totalWRIDs && solrQueryLength < 6000; I++) // solr 8.0 maximum header length as of 3/29/2019
			solrQueryLength += _snwprintf(qt + solrQueryLength, query_buffer_len - solrQueryLength, L"fromWordId:%d OR ", wordIds[I]);
		//erase last OR and put in typeId restriction
		solrQueryLength -= wcslen(L" OR ");
		wcscpy(qt + solrQueryLength, L") AND (typeId:0 OR typeId:2 OR typeId:4 OR typeId:6 OR typeId:8 OR typeId:10 OR typeId:12 OR typeId:14 OR typeId:16 OR typeId:18 OR typeId:20 OR typeId:22 OR typeId:24 OR typeId:26 OR typeId:28 OR typeId:30 OR typeId:32 OR typeId:34 OR typeId:36)\"");
		solrQueryLength = wcslen(qt);
		int numRows = 1;
		for (int start = 0; start < numRows; )
		{
			// put in fromWordIds
			wsprintf(qt + solrQueryLength,L",rows:%d,start:%d } }", 10000, start);
			wstring jsonBuffer;
			int clockSolr = clock();
			if (readPageFromSolr(qt, jsonBuffer) < 0)
				return -1;
			justSolrTime += (clock() - clockSolr);
			/* null plug buffers */
			char errbuf[1024];
			errbuf[0] = 0;
			string fileData;
			wTM(jsonBuffer, fileData);
			yajl_val node = yajl_tree_parse((const char *)fileData.c_str(), errbuf, sizeof(errbuf));
			/* parse error handling */
			if (node == NULL) {
				lplog(LOG_ERROR, L"Solr results parse error:%s\n %S", jsonBuffer.c_str(), errbuf);
				return -1;
			}
			if (start == 0)
			{
				const char * numRowsPath[] = { "response","numFound", (const char *)0 };
				yajl_val yajlNumRows = yajl_tree_get(node, numRowsPath, yajl_t_number);
				numRows = (yajlNumRows != NULL && YAJL_IS_NUMBER(yajlNumRows)) ? atoi(YAJL_GET_NUMBER(yajlNumRows)) : -1;
			}
			const char * path[] = { "response","docs", (const char *)0 };
			yajl_val docs = yajl_tree_get(node, path, yajl_t_array);
			if (docs == NULL || docs->u.array.len == 0)
				break;
			// get snippet and link
			for (unsigned int docNum = 0; docNum < docs->u.array.len; docNum++,start++)
			{
				yajl_val doc = docs->u.array.values[docNum];
				if (doc->type == yajl_t_object)
				{
					bool isNew;
					int fromId = getSolrField("fromWordId", doc), toId = getSolrField("toWordId", doc);
					int relationSourceId = getSolrField("sourceId", doc), lastWhere = getSolrField("lastWhere", doc), totalCount = getSolrField("totalCount", doc), relationType = getSolrField("typeId", doc); // index = atoi(sqlrow[0]), 
					testWordRelations.push_back(testWordRelation(getSolrField("id", doc), relationSourceId, lastWhere, fromId, toId, totalCount, relationType));
					tIWMM iFromWord, iToWord;
					if (fromId >= Words.idsAllocated || (iFromWord = Words.idToMap[fromId]) == wNULL)
						lplog(LOG_ERROR, L"Unknown 'from' word id %d encountered reading word relations", fromId);
					else if (toId < Words.idsAllocated && (iToWord = Words.idToMap[toId]) != wNULL && (iToWord->second.flags&tFI::inSourceFlag))
					{
						wrRead++;
						iFromWord->second.allocateMap(relationType);
						iFromWord->second.relationMaps[relationType]->addRelation(relationSourceId, lastWhere, iToWord, isNew, totalCount, true);
						relationType = getComplementaryRelationship((relationWOTypes)relationType);
						iToWord->second.allocateMap(relationType);
						iToWord->second.relationMaps[relationType]->addRelation(relationSourceId, lastWhere, iFromWord, isNew, totalCount, true);
						wrAdded += 2;
					}
				}
			}
		}
		if ((where = 100 * I / totalWRIDs) > lastProgressPercent)
		{
			wprintf(L"PROGRESS: %03I64d%% word relations read with %04d seconds elapsed                    \r", where, clocksec());
			lastProgressPercent = where;
		}
	}
	for (vector <WordMatch>::iterator im = m.begin(), imEnd = m.end(); im != imEnd; im++)
		im->getMainEntry()->second.flags &= ~tFI::inSourceFlag;
	overallTime = (clock() - startTime)/CLOCKS_PER_SEC;
	justSolrTime /= CLOCKS_PER_SEC;
	if ((wrRead || wrAdded) && logDatabaseDetails)
		lplog(L"Reading %d (%d added) word relations took %d seconds.", wrRead, wrAdded, (int)((clock() - startTime) / CLOCKS_PER_SEC));
	wprintf(L"PROGRESS: 100%% word relations read with %04d seconds elapsed                    \n", clocksec());
	return 0;
}

