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
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "profile.h"
#include "mysqldb.h"

#define READ_WORDS_FOR_ALL_SOURCE -1
#define READ_NEW_MAINENTRY -2
#define READ_NEW_WORDS -3
bool checkFull(MYSQL *mysql,wchar_t *qt,size_t &len,bool flush,wchar_t *qualifier);

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
bool Source::resetAllSource()
{ LFS
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  myquery(&mysql,L"update sources set processed=NULL,processing=NULL");
  unlockTables();
  return true;
}

bool Source::unlockTables(void)
{ LFS
  return myquery(&mysql,L"UNLOCK TABLES");
}

bool Source::resetSource(int beginSource,int endSource)
{ LFS
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
  _snwprintf(qt,query_buffer_len,L"update sources set processed=NULL,processing=NULL where id>=%d and id<%d",beginSource,endSource);
  myquery(&mysql,qt);
  unlockTables();
  return true;
}

void Source::resetProcessingFlags(void)
{ LFS
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return;
  myquery(&mysql,L"update sources set processing=NULL");
  unlockTables();
}

bool Source::signalBeginProcessingSource(int thisSourceId)
{  LFS
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
  _snwprintf(qt,query_buffer_len,L"update sources set processing=true where id=%d", thisSourceId);
  myquery(&mysql,qt);
  unlockTables();
  return true;
}

bool Source::signalFinishedProcessingSource(int thisSourceId)
{ LFS
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
  _snwprintf(qt,query_buffer_len,L"update sources set processing=NULL, processed=true where id=%d", thisSourceId);
  myquery(&mysql,qt);
  unlockTables();
  return true;
}


bool Source::getNextUnprocessedSource(int begin, int end, enum Source::sourceTypeEnum st, bool setUsed, int &id, wstring &path, wstring &start, int &repeatStart, wstring &etext, wstring &author, wstring &title)
{
	LFS
		MYSQL_RES * result;
		if (!myquery(&mysql, L"LOCK TABLES sources WRITE")) return false;
	wchar_t qt[query_buffer_len_overflow];
	_snwprintf(qt, query_buffer_len, L"select id, path, start, repeatStart, etext, author, title from sources where id>=%d and sourceType=%d and processed IS NULL and processing IS NULL and start!='**SKIP**' and start!='**START NOT FOUND**'",begin, st);
	if (end>=0)
		_snwprintf(qt+wcslen(qt), query_buffer_len-wcslen(qt), L" and id<%d",end);
	wcscat(qt + wcslen(qt), L" order by id ASC limit 1");
	MYSQL_ROW sqlrow = NULL;
	if (myquery(&mysql, qt, result))
	{
		if ((sqlrow = mysql_fetch_row(result)) && setUsed)
		{
			_snwprintf(qt, query_buffer_len, L"update sources set processing=true where id=%S", sqlrow[0]);
			myquery(&mysql, qt);
		}
		if (sqlrow != NULL)
		{
			sourceId=id = atoi(sqlrow[0]);
			mTW(sqlrow[1], path);
			mTW(sqlrow[2], start);
			repeatStart = atoi(sqlrow[3]);
			if (sqlrow[4]!=NULL)
				mTW(sqlrow[4], etext);
			if (sqlrow[5]!=NULL)
				mTW(sqlrow[5], author);
			if (sqlrow[6] != NULL)
				mTW(sqlrow[6], title);
		}
	}
	mysql_free_result(result);
	unlockTables();
	return sqlrow!=NULL;
}

bool Source::getNextUnprocessedParseRequest(int &prId, wstring &pathInCache)
{
	LFS
	MYSQL_RES * result=NULL;
	if (!myquery(&mysql, L"LOCK TABLES parseRequests WRITE")) return false;
	MYSQL_ROW sqlrow = NULL;
	if (myquery(&mysql, L"select prId,pathInCache from parseRequests where status=0 order by prId ASC limit 1", result))
	{
		if ((sqlrow = mysql_fetch_row(result)))
		{
			wchar_t qt[query_buffer_len_overflow];
			_snwprintf(qt, query_buffer_len, L"update parseRequests set status=1 where prId=%S", sqlrow[0]);
			myquery(&mysql, qt);
		}
		prId = atoi(sqlrow[0]);
		mTW(sqlrow[1], pathInCache);
	}
	mysql_free_result(result);
	unlockTables();
	return sqlrow!=NULL;
}

bool Source::updateSource(wstring &path, wstring &start, int repeatStart, wstring &etext, int actualLenInBytes)
{
	LFS
		wstring tmp, tmp2, sqlStatement;
	escapeStr(path);
	escapeStr(start);
	sqlStatement = L"update sources set path='" + path + L"', start='" + start + L"', repeatStart=" + itos(repeatStart, tmp) + L", sizeInBytes=" + itos(actualLenInBytes, tmp2) + L" where etext='" + etext + L"'";
	return myquery(&mysql, (wchar_t *)sqlStatement.c_str());
}

bool Source::updateSourceStart(wstring &start, int repeatStart, wstring &etext, __int64 actualLenInBytes)
{
	LFS
	wstring tmp, tmp2, sqlStatement;
	escapeStr(start);
	sqlStatement = L"update sources set start='" + start + L"', repeatStart=" + itos(repeatStart, tmp) + L", sizeInBytes=" + itos((int)actualLenInBytes, tmp2) + L" where etext='" + etext + L"'";
	return myquery(&mysql, (wchar_t *)sqlStatement.c_str());
}

int Source::getNumSources(enum Source::sourceTypeEnum st,bool left)
{ LFS
	MYSQL_RES *result = NULL;
	MYSQL_ROW sqlrow;
	wchar_t qt[query_buffer_len_overflow];
  if (!myquery(&mysql,L"LOCK TABLES sources READ")) return -1;
  _snwprintf(qt,query_buffer_len,L"select COUNT(id) FROM sources where sourceType=%d and start!='**START NOT FOUND**' and start!='**SKIP**'%s",st,(left) ? L" and processed is null":L"");
  if (!myquery(&mysql,qt,result)) return -1;
  unlockTables();
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

int initializeDatabaseHandle(MYSQL &mysql,wchar_t *where,bool &alreadyConnected)
{ LFS
  if (alreadyConnected) return 0;
  if(mysql_init(&mysql)==NULL)
    lplog(LOG_FATAL_ERROR,L"Failed to initialize MySQL");
	bool keep_connect=true;
	mysql_options(&mysql,MYSQL_OPT_RECONNECT,&keep_connect);
	string sqlStr;
  if (alreadyConnected=(mysql_real_connect(&mysql,wTM(where,sqlStr),"root","byron0",DBNAME,0,NULL,0)!=NULL))
	{
		mysql_options(&mysql,MYSQL_OPT_RECONNECT,&keep_connect);
		if (mysql_set_character_set(&mysql, "utf8")) 
			lplog(LOG_ERROR,L"Error setting default client character set to utf8.  Default now: %S\n", mysql_character_set_name(&mysql));
    return 0;
	}
	mysql_options(&mysql,MYSQL_OPT_RECONNECT,&keep_connect);
	return -1;
}

void Source::updateSourceStatistics(int numSentences, int matchedSentences, int numWords, int numUnknown,
    int numUnmatched,int numOvermatched, int numQuotations, int quotationExceptions, int numTicks, int numPatternMatches)
{ LFS
  wchar_t qt[query_buffer_len_overflow];
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return;
  _snwprintf(qt,query_buffer_len,L"UPDATE sources SET numSentences=%d, matchedSentences=%d, numWords=%d, numUnknown=%d, numUnmatched=%d, "
    L"numOvermatched=%d, numQuotations=%d, quotationExceptions=%d, numTicks=%d, numPatternMatches=%d where id=%d",
    numSentences, matchedSentences,numWords,numUnknown,numUnmatched,numOvermatched,numQuotations,quotationExceptions,
    numTicks,numPatternMatches,sourceId);
  myquery(&mysql,qt);
}

void Source::updateSourceStatistics2(int sizeInBytes, int numWordRelations)
{ LFS
  wchar_t qt[query_buffer_len_overflow];
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return;
  _snwprintf(qt,query_buffer_len,L"UPDATE sources SET sizeInBytes=%d, numWordRelations=%d where id=%d",sizeInBytes, numWordRelations,sourceId);
  myquery(&mysql,qt);
}

void Source::updateSourceStatistics3(int numMultiWordRelations)
{ LFS
  wchar_t qt[query_buffer_len_overflow];
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return;
  _snwprintf(qt,query_buffer_len,L"UPDATE sources SET numMultiWordRelations=%d where id=%d",numMultiWordRelations,sourceId);
  myquery(&mysql,qt);
}

int Source::readMultiSourceObjects(tIWMM *wordMap,int numWords)
{ LFS
  int startTime=clock();
  MYSQL_RES *result=NULL;
  if (!myquery(&mysql,L"select id, objectClass, nickName, ownerObject, firstSpeakerGroup, male, female, neuter, plural FROM objects where common=1",result)) return -1;
  MYSQL_ROW sqlrow;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
    cName nm;
    nm.nickName=atoi(sqlrow[2]);
    cObject o((OC)atoi(sqlrow[1]),nm,-1,-1,-1,-1,atoi(sqlrow[3]),sqlrow[4][0]==1,sqlrow[5][0]==1,sqlrow[6][0]==1,sqlrow[7][0]==1,false);
    o.index=atoi(sqlrow[0]);
    o.multiSource=true;
    objects.push_back(o);
  }
  mysql_free_result(result);
  if (!myquery(&mysql,L"select objectId, wordId, orderOrType FROM objectWordMap ORDER BY objectId, orderOrType",result)) return -1;
  int previousId=-1;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
    int currentId=atoi(sqlrow[0]);
    if (previousId==(int)currentId)
    {
      objects[previousId].end++;
      objects[previousId].originalLocation++;
    }
    else
      objects[currentId].begin=objects[currentId].end=objects[currentId].originalLocation=objects[currentId].firstLocation=m.size();
    if (atoi(sqlrow[1])>=numWords)
      lplog(LOG_FATAL_ERROR,L"non-existent word reference.");
    m.emplace_back(wordMap[atoi(sqlrow[1])],0,debugTrace);
    relatedObjectsMap[wordMap[atoi(sqlrow[1])]].insert(currentId);
    if (objects[currentId].objectClass==NAME_OBJECT_CLASS)
    {
      switch(atoi(sqlrow[2]))
      {
        case cName::HON: objects[currentId].name.hon=wordMap[atoi(sqlrow[1])]; break;
        case cName::HON2: objects[currentId].name.hon2=wordMap[atoi(sqlrow[1])]; break;
        case cName::HON3: objects[currentId].name.hon3=wordMap[atoi(sqlrow[1])]; break;
        case cName::FIRST: objects[currentId].name.first=wordMap[atoi(sqlrow[1])]; break;
        case cName::MIDDLE: objects[currentId].name.middle=wordMap[atoi(sqlrow[1])]; break;
        case cName::MIDDLE2: objects[currentId].name.middle2=wordMap[atoi(sqlrow[1])]; break;
        case cName::LAST: objects[currentId].name.last=wordMap[atoi(sqlrow[1])]; break;
        case cName::SUFFIX: objects[currentId].name.suffix=wordMap[atoi(sqlrow[1])]; break;
        case cName::ANY: objects[currentId].name.any=wordMap[atoi(sqlrow[1])]; break;
      }
    }
  }
  mysql_free_result(result);
	if (logDatabaseDetails)
	  lplog(L"readMultiSourceObjects took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

// right now it is assumed that all objects from books are kept separate,
// so there is no check for merging information between info sources
// but there is a 'common' list consisting of all multiple-word objects, usually locations.
int Source::flushObjects(set <int> &objectsToFlush)
{
	LFS
  if (sourceId==READ_WORDS_FOR_ALL_SOURCE) return 0;
  wprintf(L"Writing objects to database...\r");
  if (!myquery(&mysql,L"LOCK TABLES objects WRITE")) return -1;

  wchar_t qt[query_buffer_len_overflow];
  wchar_t wmqt[query_buffer_len_overflow];
  int startTime=clock(),numObjectsInserted=0,numObjectLocationsInserted=0,where,lastProgressPercent=-1;
  wcscpy(qt,L"REPLACE INTO objects VALUES ");
  size_t len=wcslen(qt);
  set <int>::iterator oi=objectsToFlush.begin(),oiEnd=objectsToFlush.end();
  for (; oi!=oiEnd && !exitNow; oi++,numObjectsInserted++)
  {
		//int no=*oi;
		cObject *o=&(objects[*oi]);
    len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d,%d,%d,%d,%d,%d,%d,%d,%d,NULL,%d,%d,%d,%d,%d,%d,%d),",
			sourceId,*oi,
      o->objectClass,o->numEncounters,o->numIdentifiedAsSpeaker,o->name.nickName,o->ownerWhere,o->firstSpeakerGroup,
			getProfession(*oi),
      o->identified==true ? 1 : 0,o->plural==true ? 1 : 0,o->male==true ? 1 : 0,o->female==true ? 1 : 0,o->neuter==true ? 1 : 0,
			/*multiSource*/0,
      (o->objectClass==NAME_OBJECT_CLASS) ? 1 : 0);
    if ((where=numObjectsInserted*100/objects.size())>lastProgressPercent)
    {
      wprintf(L"PROGRESS: %03d%% objects written to database with %04d seconds elapsed \r",where,clocksec());
      lastProgressPercent=where;
    }
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  myquery(&mysql,L"UNLOCK TABLES");
  lastProgressPercent=-1;
  int objectDetails=0;
  if (!myquery(&mysql,L"LOCK TABLES objectWordMap WRITE",true)) return -1;
  wcscpy(wmqt,L"INSERT INTO objectWordMap VALUES ");
  size_t wmlen=wcslen(wmqt);
  for (oi=objectsToFlush.begin(); oi!=oiEnd && !exitNow; oi++,objectDetails++)
  {
		cObject *o= &(objects[*oi]);
    if ((where=objectDetails*100/objects.size())>lastProgressPercent)
    {
      wprintf(L"PROGRESS: %03d%% object details written to database with %04d seconds elapsed \r",where,clocksec());
      lastProgressPercent=where;
    }
    if (o->objectClass==NAME_OBJECT_CLASS)
      wmlen+=o->name.insertSQL(wmqt+wmlen,sourceId,*oi,query_buffer_len-wmlen);
    else
    {
      for (int I=o->begin; I<o->end; I++)
				if (m[I].word->second.index>=0)
					wmlen+=_snwprintf(wmqt+wmlen,query_buffer_len-wmlen,L"(%d,%d,%d,%d),",sourceId,*oi,m[I].word->second.index,I-o->begin);
    }
    if (!checkFull(&mysql,wmqt,wmlen,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,wmqt,wmlen,true,NULL)) return -1;
  if (numObjectsInserted && logDatabaseDetails)
		lplog(L"Inserting %d objects from %d locations took %d seconds.",numObjectsInserted,numObjectLocationsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  myquery(&mysql,L"UNLOCK TABLES");
  wprintf(L"PROGRESS: 100%% objects written to database with %04d seconds elapsed         \n",clocksec());
  return 0;
}

int Source::flushObjectRelations()
{ LFS
  return 0;
}

// words belonging to a certain toGroup:
//   What are the words that map to all the words in a toGroup?
//   select toWordId from groupMapTo where groupId=GROUPA and typeId=TIDA
//   select fromWordId fwid  from wordRelations wr where
//     (select COUNT(toWordId) from wordRelations wr2 where
//        wr2.fromWordId=wr.fromWordId and toWordId in (select toWordId from groupMapTo where groupId=GROUPA and typeId=TIDA))
// adding to existing group (GID GROUPA, TypeId TIDA, original timestamp of group as last read from DB OTS)
//   group in memory may not have all the words of the group in DB
//   if any fromWords to add:
//   {
//     for each fromWord FWID to add
//       select toWordId from groupMapTo where groupId=GROUPA and typeId=TIDA and
//              toWordId NOT IN (select toWordId from wordRelations where fromWordId=FWID and typeId=TIDA)
//       if there are any entries, create a new group
//   }
//   if any toWords to add:
//   {
//     for each toWord TWID to add
//       select fromWordId from groups where groupId=GROUPA and typeId=TIDA and
//              fromWordId NOT IN (select fromWordId from wordRelations where toWordId=TWID and typeId=TIDA)
//       if there are any entries, create a new group
//   }
int WordClass::flushGroups()
{ LFS
  return 0;
}

// not being called
int WordClass::flushForms(MYSQL &mysql)
{ LFS
  MYSQL_RES *result=NULL;
  if (!myquery(&mysql,L"LOCK TABLES forms WRITE")) return -1;
  if (!myquery(&mysql,L"SELECT MAX(id) from forms",result)) return -1;
  MYSQL_ROW sqlrow=mysql_fetch_row(result);
  int numFormsInDB=(sqlrow[0]==NULL) ? 0 : atoi(sqlrow[0]);
  mysql_free_result(result);
  if (numFormsInDB==Forms.size()) return 0;
  if (!myquery(&mysql,L"LOCK TABLES forms WRITE")) return -1;
  //if (!myquery(&mysql,L"ALTER TABLE forms DISABLE KEYS")) return -1;
  //if (!myquery(&mysql,L"LOCK TABLES forms WRITE")) return -1;
  set<wstring>::iterator fi;
  wchar_t qt[query_buffer_len_overflow];
  int startTime=clock();
  wcscpy(qt,L"INSERT INTO forms VALUES ");
  size_t len=wcslen(qt);
  //bool newFormsFound=false;
  // load forms
  for (unsigned int f=numFormsInDB,id=numFormsInDB+1; f<Forms.size(); f++)
  {
    if (Forms[f]->name.find(L"objective case of")!=wstring::npos)
    {
      Forms[f]->index=-1;
      continue;
    }
    //wchar_t name[1024],shortName[1024];
    //mysql_real_escape_string(&mysql, name, Forms[f]->name.c_str(), Forms[f]->name.length());
    //mysql_real_escape_string(&mysql, shortName, Forms[f]->shortName.c_str(), Forms[f]->shortName.length());
		if (Forms[f]->name!=L"\\")
			len+=_snwprintf(qt+len,query_buffer_len-len,L"(NULL,\"%s\",\"%s\",\"%s\",%d,%d,%d,%d, NULL),",
	      Forms[f]->name.c_str(),Forms[f]->shortName.c_str(),Forms[f]->inflectionsClass.c_str(),
				Forms[f]->hasInflections,Forms[f]->isTopLevel,Forms[f]->properNounSubClass,Forms[f]->blockProperNounRecognition);
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
    Forms[f]->index=id++;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
	if (logDatabaseDetails)
	  lplog(L"Inserting forms took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  //if (!myquery(&mysql,L"ALTER TABLE forms ENABLE KEYS")) return -1;
  return 0;
}

// called by writeWordsToDB (in this file)
// write all mainEntries of all words that have no indexes (new words).  Set insertNewForms flag for each.
int WordClass::flushNewMainEntryWords(MYSQL &mysql,vector <tIWMM> &queryWords,bool justQuery)
{ LFS
  if (!myquery(&mysql,L"LOCK TABLES words WRITE")) return -1;
  int startTime=clock(),numWordsInserted=0;
  wchar_t qt[query_buffer_len_overflow];
  wcscpy(qt,L"INSERT IGNORE INTO words VALUES "); // IGNORE is necessary because C++ treats unicode strings as different, but MySQL treats them as the same
  size_t len=wcslen(qt);
  //for (iWord=begin(); iWord!=iWordEnd; iWord++)
  //  if (iWord->first[0]>='0' && iWord->first[0]<='9') // time, date or number not considered unknown.
  //    iWord->second.flags&=~tFI::updateMainInfo;
  // insert only mainEntries
  for (tIWMM iWord=begin(),iWordEnd=end(); iWord!=iWordEnd; iWord++)
  {
    tIWMM mainEntry=iWord->second.mainEntry;
    if (mainEntry==wNULL || mainEntry->second.index>=0 || iWord==mainEntry) continue;
    if (iWord->first.length()>MAX_WORD_LENGTH)
    {
      lplog(LOG_ERROR,L"Word %s is too long.... Rejected (1).",iWord->first.c_str());
      continue;
    }
    if (iWord->first[iWord->first.length()-1]==' ')
    {
			lplog(LOG_ERROR,L"Word %s has a space at the end... Rejected (1).",iWord->first.c_str());
      continue;
    }
		if (!justQuery)
		{
			wstring word;
			wchar_t source[10];
			encodeEscape(mysql, word, mainEntry->first);
			mainEntry->second.flags&=~(tFI::updateMainInfo|tFI::intersectionGroup);
			mainEntry->second.flags|=tFI::insertNewForms;
			len+=_snwprintf(qt+len,query_buffer_len-len,L"(NULL,\"%s\",%d,%d,%d,%d,%d,%s,NULL),",
				word.c_str(),mainEntry->second.inflectionFlags,mainEntry->second.flags&~(tFI::resetFlagsOnRead),mainEntry->second.timeFlags,-1,mainEntry->second.derivationRules,
				(mainEntry->second.sourceId==READ_WORDS_FOR_ALL_SOURCE) ? L"NULL" : _itow(mainEntry->second.sourceId,source,10));
			if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
	    numWordsInserted++;
	    #ifdef LOG_WORD_FLOW
		    lplog(L"Inserted mainEntry word %s.",word);
			#endif
		}
    queryWords.push_back(mainEntry);
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  if (numWordsInserted && logDatabaseDetails && !justQuery)
		lplog(L"Inserting %d mainEntry words took %d seconds.",numWordsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

// called by writeWordsToDB (in this file)
// write all mainEntries of all words that have no indexes (new words).  Set insertNewForms flag for each.
int WordClass::flushNewWords(MYSQL &mysql,vector <tIWMM> &queryWords,bool justQuery)
{ LFS
  int startTime=clock(),numWordsInserted=0;
  wchar_t qt[query_buffer_len_overflow],source[10];
  wcscpy(qt,L"INSERT IGNORE INTO words (word,inflectionFlags,flags,timeFlags,mainEntryWordId,derivationRules,sourceId) VALUES "); // IGNORE is necessary because C++ treats unicode strings as different, but MySQL treats them as the same
  size_t len=wcslen(qt);
  for (tIWMM iWord=begin(),iWordEnd=end(); iWord!=iWordEnd; iWord++)
  {
    if (iWord->second.index>=0) continue;
    wstring word;
		wstring save=iWord->first;
    encodeEscape(mysql, word, iWord->first);
    iWord->second.flags&=~(tFI::updateMainInfo|tFI::intersectionGroup);
    if (iWord->first.length()>MAX_WORD_LENGTH)
    {
      lplog(LOG_ERROR,L"Word %s is too long.... Rejected (2).",iWord->first.c_str());
      continue;
    }
    if (iWord->first[iWord->first.length()-1]==' ')
    {
			lplog(LOG_ERROR,L"Word %s has a space at the end... Rejected (2) [original=%s].",iWord->first.c_str(),save.c_str());
      continue;
    }
    if (iWord->first.find(L'\\')!=wstring::npos)
    {
			if (iWord->first!=L"\\")
				lplog(LOG_ERROR,L"Word %s is a slash... Rejected (3) [original=%s].",iWord->first.c_str(),save.c_str());
      continue;
    }
    queryWords.push_back(iWord);
		if (!justQuery)
		{
			iWord->second.flags|=tFI::insertNewForms;
			if (iWord->second.mainEntry==wNULL)
				len+=_snwprintf(qt+len,query_buffer_len-len,L"(\"%s\",%d,%d,%d,NULL,%d,%s),",
					word.c_str(),iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.derivationRules,
					(iWord->second.sourceId==READ_WORDS_FOR_ALL_SOURCE) ? L"NULL" : _itow(iWord->second.sourceId,source,10));
			else
				len+=_snwprintf(qt+len,query_buffer_len-len,L"(\"%s\",%d,%d,%d,%d,%d,%s),",
					word.c_str(),iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.mainEntry->second.index,iWord->second.derivationRules,
					(iWord->second.sourceId==READ_WORDS_FOR_ALL_SOURCE) ? L"NULL" : _itow(iWord->second.sourceId,source,10));
			if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
			#ifdef LOG_WORD_FLOW
	      lplog(L"Inserted non-mainEntry word %s.",word);
			#endif
		}
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  if (numWordsInserted && logDatabaseDetails && !justQuery)
		lplog(L"Inserting %d non-mainEntry words took %d seconds.",numWordsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int WordClass::updateOnlyMainEntryIndexToDB(MYSQL &mysql,vector <tIWMM> &queryWords)
{ LFS
  wchar_t qt[query_buffer_len_overflow],dupqt[128];
  wcscpy(qt,L"insert into words (id,mainEntryWordId) VALUES ");
  _snwprintf(dupqt,128,L" ON DUPLICATE KEY UPDATE mainEntryWordId=VALUES(mainEntryWordId)");
  size_t len=wcslen(qt);
  for (vector <tIWMM>::iterator qwi=queryWords.begin(),qwiEnd=queryWords.end(); qwi!=qwiEnd; qwi++)
		if ((*qwi)->second.mainEntry!=wNULL && (*qwi)->second.index>=0 && (*qwi)->second.mainEntry->second.index>=0)
		{
			len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, %d),",(*qwi)->second.index,(*qwi)->second.mainEntry->second.index);
			if (!checkFull(&mysql,qt,len,false,dupqt)) return -1;
		}
  if (!checkFull(&mysql,qt,len,true,dupqt)) return -1;
  return 0;
}

// called by writeWordsToDB (in this file)
// update word information without wordForms.  No flags are set.
int WordClass::updateWordMainInfoToDB(MYSQL &mysql,int &numUpdates)
{ LFS
  int startTime=clock();
  tIWMM iWord,iWordEnd=end();
  wchar_t qt[query_buffer_len_overflow];
  //int len=0;
  numUpdates=0;
  for (iWord=begin(); iWord!=iWordEnd; iWord++)
  {
    if (!(iWord->second.flags&tFI::updateMainInfo)) continue;
    /* could have changed:
      int inflectionFlags;
      int flags;
      int derivationRules;
      tIWMM mainEntry;
      forms
    */
    if (iWord->first.length()>MAX_WORD_LENGTH)
    {
			lplog(LOG_ERROR,L"Word %s is too long.... Rejected (3).",iWord->first.c_str());
      continue;
    }
    if (iWord->first[iWord->first.length()-1]==' ')
    {
			lplog(LOG_ERROR,L"Word %s has a space at the end... Rejected (3).",iWord->first.c_str());
      continue;
    }
    numUpdates++;
    wstring word;
    encodeEscape(mysql, word, iWord->first);
    int len;
    if (iWord->second.mainEntry==wNULL)
      len=_snwprintf(qt,query_buffer_len,L"UPDATE words SET word=\"%s\",inflectionFlags=%d,flags=%d,timeFlags=%d,derivationRules=%d,sourceId=NULL WHERE id=%d LIMIT 1",
          word.c_str(),iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.derivationRules,iWord->second.index);
    else
      len=_snwprintf(qt,query_buffer_len,L"UPDATE words SET word=\"%s\",inflectionFlags=%d,flags=%d,timeFlags=%d,mainEntryWordId=%d,derivationRules=%d,sourceId=NULL WHERE id=%d LIMIT 1",
          word.c_str(),iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.mainEntry->second.index,iWord->second.derivationRules,iWord->second.index);
    if (!myquery(&mysql,qt)) continue;
    #ifdef LOG_WORD_FLOW
      lplog(L"Updated word %s.",word);
    #endif
  }
  if (numUpdates && logDatabaseDetails)
    lplog(L"Updating non-form info for %d existing words took %d seconds.",numUpdates,(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

// called by writeWordsToDB (in this file)
// insert/update new form data
// if insertNewForms is set on word, include all forms, regardless of whether deltaCount is 0.
//   if insertNewForms is not set, skip forms where deltaCount is 0.
// load wordForms table for new words and words for which the forms have changed
int WordClass::updateWordFormsToDB(MYSQL &mysql)
{ LFS
  if (!myquery(&mysql,L"LOCK TABLES wordForms WRITE")) return -1;
  int startTime=clock();
  tIWMM iWord,iWordEnd=end();
  wchar_t qt[query_buffer_len_overflow],dupqt[128];
  wcscpy(qt,L"INSERT INTO wordForms (wordId, formId, count) VALUES ");
  _snwprintf(dupqt,128,L" ON DUPLICATE KEY UPDATE count=count+VALUES(count)");
  size_t len=wcslen(qt),maxForms=Forms.size(),wordFormsInserted=0;
  for (iWord=begin(); iWord!=iWordEnd; iWord++)
  {
		// if TRANSFER_COUNT is 0, the word never appeared in text.
    bool writeAllForms=(iWord->second.flags&tFI::insertNewForms)!=0;
		if (!iWord->second.usagePatterns[tFI::TRANSFER_COUNT] && !writeAllForms) continue;
		if (iWord->second.index<0) continue; // should never happen!
    #ifdef LOG_WORD_FLOW
      lplog(L"%s word %s (index=%d) - %d forms.",(iWord->second.flags&tFI::updateMainInfo) ? "Updating" : "Creating",iWord->first.c_str(),iWord->second.index,iWord->second.formsSize());
    #endif
    iWord->second.flags&=~(tFI::updateMainInfo|tFI::insertNewForms);
    for (unsigned int f=0; f<iWord->second.formsSize(); f++,wordFormsInserted++)
    {
			if ((!writeAllForms && iWord->second.deltaUsagePatterns[f]==0) || iWord->second.illegal(f,maxForms,iWord)) continue;
      len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, %d, %d),",iWord->second.index, iWord->second.Form(f)->index,iWord->second.deltaUsagePatterns[f]);
			iWord->second.deltaUsagePatterns[f]=0;
      if (!checkFull(&mysql,qt,len,false,dupqt)) return -1;
    }
    for (unsigned int f=tFI::MAX_FORM_USAGE_PATTERNS; f<tFI::LAST_USAGE_PATTERN; f++)
    {
			if (!writeAllForms && iWord->second.deltaUsagePatterns[f]==0) continue;
      wordFormsInserted++;
      len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, %d, %d),",iWord->second.index, f+tFI::patternFormNumOffset-tFI::MAX_FORM_USAGE_PATTERNS,iWord->second.deltaUsagePatterns[f]);
      iWord->second.deltaUsagePatterns[f]=0;
	    if (!checkFull(&mysql,qt,len,false,dupqt)) return -1;
    }
  }
  if (!checkFull(&mysql,qt,len,true,dupqt)) return -1;
  if (wordFormsInserted && logDatabaseDetails)
    lplog(L"Inserting %d wordforms took %d seconds.",wordFormsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

void tFI::lplog(void)
{ LFS
  ::lplog(L"%d %d %d %s",index,flags,inflectionFlags,(mainEntry==wNULL) ? L"" : mainEntry->first.c_str());
}

int WordClass::insertWordIds(MYSQL &mysql,wchar_t *qt)
{ LFS
  MYSQL_RES *result=NULL;
  MYSQL_ROW sqlrow;
  if (!myquery(&mysql,qt,result)) return -1;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
    int wordId=atoi(sqlrow[0]);
		wstring word;
		mTW(sqlrow[1],word);
		tIWMM w;
		if ((w=query(word))!=end())
		{
			w->second.index=wordId;
			assign(wordId,w);
		}
		else
			lplog(LOG_FATAL_ERROR,L"NOT FOUND word=%s %S",word.c_str(),sqlrow[1]);
	}
  mysql_free_result(result);
  return 0;
}

int WordClass::refreshWordIdsFromDB(MYSQL &mysql,vector <tIWMM> &queryWords)
{ LFS
	if (queryWords.empty()) return 0;
  if (!myquery(&mysql,L"LOCK TABLES words READ")) return -1; 
  int startTime=clock();
  wchar_t qt[query_buffer_len_overflow];
  wcscpy(qt,L"select id,word from words where word in (");
  size_t len=wcslen(qt);
	//int qwc=0;

  for (vector <tIWMM>::iterator qwi=queryWords.begin(),qwiEnd=queryWords.end(); qwi!=qwiEnd; qwi++)
  {
    wstring word;
    encodeEscape(mysql, word, (*qwi)->first);
		 len+=_snwprintf(qt+len,query_buffer_len-len,L"\"%s\",",word.c_str());
	   if (len>query_buffer_len_underflow)
		 {
			 wcscpy(qt+len-1,L")");
			 insertWordIds(mysql,qt);
			 wcscpy(qt,L"select id,word from words where word in (");
			 len=wcslen(qt);
		 }
  }
  if (qt[wcslen(qt)-1]!=L'(')
	{
	  wcscpy(qt+len-1,L")");
 	  insertWordIds(mysql,qt);
	}
  myquery(&mysql,L"UNLOCK TABLES");
  if (logDatabaseDetails)
    lplog(L"Refreshing wordIds took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int WordClass::readWordIndexesFromDB(MYSQL &mysql)
{ LFS
  int startTime=clock();
  wprintf(L"Refreshing word indexes from database...\r");
	vector <tIWMM> queryWords;
	// accumulate all new main entries in queryWords
  if (flushNewMainEntryWords(mysql,queryWords,true)<0) return -3;
	// accumulate all new words (not main entries) in queryWords
  if (flushNewWords(mysql,queryWords,true)<0) return -4;
	// get the wordIds assigned to all the main entries and words that have just been flushed to the DB
	if (refreshWordIdsFromDB(mysql,queryWords)<0) return -5;
	if (logDatabaseDetails)
	  lplog(L"WordClass::readWordIndexesFromDB took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
	return 0;
}

// called by writeSource (in this file)
// steps:
//  get the maximum AUTO_INCREMENT index for a word to use for mainEntry index
//  insert all mainEntries that don't have an auto_increment index assigned to it.
//  insert all new words into words table
//  update inflectionFlags, flags, mainEntry id and derivationRules for all changed words
//  update all wordForms for all changed words
int WordClass::writeWordsToDB(MYSQL &mysql)
{ LFS
  int startTime=clock();
  wprintf(L"Writing words to database...\r");
	if (logDatabaseDetails)
	  lplog(L"WordClass::writeWordsToDB begin...");
  int numUpdates;
	vector <tIWMM> queryWords;
	// flush all new main entries to DB
  if (flushNewMainEntryWords(mysql,queryWords,false)<0) return -3;
	// flush all new words (not main entries) to DB
  if (flushNewWords(mysql,queryWords,false)<0) return -4;
	// get the wordIds assigned to all the main entries and words that have just been flushed to the DB
	if (refreshWordIdsFromDB(mysql,queryWords)<0) return -5;
	// update the mainEntry ids (wordIds) of all the main entries and words that have just been flushed to the DB
	if (updateOnlyMainEntryIndexToDB(mysql,queryWords)<0) return -6;
	// update any word information that has been changed since it has been read from the DB
  if (updateWordMainInfoToDB(mysql,numUpdates)<0) return -7;
	// update word forms that have been newly assigned to words since having been read from the DB
  if (updateWordFormsToDB(mysql)<0) return -8;
  myquery(&mysql,L"UNLOCK TABLES");
  wprintf(L"Finished writing to database.      \r");
	if (logDatabaseDetails)
	  lplog(L"WordClass::writeWordsToDB took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

tFI::tFI(void)
{ LFS
  count=0;
  inflectionFlags=0;
  flags=0;
  derivationRules=0;
  mainEntry=wNULL;
  index=-uniqueNewIndex++;
  memset(usagePatterns,0,MAX_USAGE_PATTERNS);
  memset(usageCosts,0,MAX_USAGE_PATTERNS);
  memset(deltaUsagePatterns,0,MAX_USAGE_PATTERNS);
  memset(relationMaps,0,numRelationWOTypes*sizeof(*relationMaps));
  changedSinceLastWordRelationFlush=false;
  tmpMainEntryWordId=-1;
  formsOffset=fACount;
  sourceId=-1;
}

// called by transferFormsAndUsage (in this file)
// this procedure does not make a high pattern count into a low cost.
// it makes a high pattern count received from the DB into a count which will fit into the usagePatterns array (max 128)
void tFI::transferDBUsagePatternsToUsagePattern(int highestPatternCount,int *DBUsagePatterns,unsigned int upStart,unsigned int upLength)
{ LFS
  int highest=-1;
  for (unsigned int up=upStart; up<upStart+upLength; up++)
    highest=max(highest,DBUsagePatterns[up]);
  if (highest<highestPatternCount)
    for (unsigned int up=upStart; up<upStart+upLength; up++)
      usagePatterns[up]=(char)DBUsagePatterns[up];
  else
    // 600 8 0 -> highest=600   128  1 0
    for (unsigned int up=upStart; up<upStart+upLength; up++)
      usagePatterns[up]=(highestPatternCount*((unsigned int)DBUsagePatterns[up])/highest);
  }

// formNum is the form that is the word itself, if that form exists.
// read the tFI from the DB.  So, don't mark this with insertNewForms flag
tFI::tFI(unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int mainEntryWordId,int iDerivationRules,int iSourceId,int formNum,wstring &word)
{ LFS
  count=iCount;
  int oldAllocated=allocated;
  if (fACount+count>=allocated)
    formsArray=(unsigned int *)trealloc(17,formsArray,oldAllocated*sizeof(*formsArray),(allocated=(allocated+count)*2)*sizeof(*formsArray));
  formsOffset=fACount;
  fACount+=count;
  inflectionFlags=iInflectionFlags;
  flags=iFlags;
  timeFlags=iTimeFlags;
  derivationRules=iDerivationRules;
  mainEntry=wNULL;
  index=-uniqueNewIndex++;
  //preferVerbPresentParticiple();
  tmpMainEntryWordId=mainEntryWordId;
  sourceId=iSourceId;
  transferFormsAndUsage(forms,iCount,formNum,word);
  fACount+=(count-iCount);
  count=iCount;
  memset(relationMaps,0,numRelationWOTypes*sizeof(*relationMaps));
}

void tFI::transferFormsAndUsage(unsigned int *forms,unsigned int &iCount,int formNum,wstring &word)
{ LFS
  memset(usagePatterns,0,MAX_USAGE_PATTERNS);
  memset(usageCosts,0,MAX_USAGE_PATTERNS);
  memset(deltaUsagePatterns,0,MAX_USAGE_PATTERNS);
  changedSinceLastWordRelationFlush=false;
  int UPDB[MAX_USAGE_PATTERNS];
  memset(UPDB,0,sizeof(*UPDB)*MAX_USAGE_PATTERNS);
  int sameNameForm=-1,properNounForm=-1,formCount=0;
  for (unsigned int I=0,*J=forms; I<(unsigned)iCount; I++,J+=2)
  {
    // has to be patternFormOffset-1 because each form has 1 subtracted from it on read.
    if (*J>=(patternFormNumOffset-1) && *J-(patternFormNumOffset-1)>MAX_USAGE_PATTERNS-MAX_FORM_USAGE_PATTERNS)
      ::lplog(LOG_INFO,L"%s:Incorrect form # - %d.",word.c_str(),*J);
    else if (*J>=(patternFormNumOffset-1))
      UPDB[*J-(patternFormNumOffset-1)+MAX_FORM_USAGE_PATTERNS]=*(J+1);
    else
    {
			// filter out duplicate forms (which for some reason have crept in)
			unsigned int *f,*fend;
		  for (f=formsArray+formsOffset,fend=formsArray+formsOffset+formCount; f!=fend; f++)
				if (*f==*J)
					break;
			if (f!=fend)
			{
				//::lplog(LOG_INFO,L"Duplicate form %s seen in word %s.",Forms[*J]->name.c_str(),word.c_str());
				continue;
			}
      if (formCount>=MAX_FORM_USAGE_PATTERNS)
			{
				wstring formStrings;
				for (int K=0; K<formCount; K++)
					formStrings+=Form(K)->name+L" ";
				::lplog(LOG_DICTIONARY,L"numForms of %s over %d: %s.",word.c_str(),MAX_FORM_USAGE_PATTERNS,formStrings.c_str());
			}
      else
      {
        formsArray[formsOffset+formCount]=*J;
        UPDB[formCount]=*(J+1);
        if (*J==formNum)
          sameNameForm=formCount;
        else if (*J==PROPER_NOUN_FORM_NUM)
          properNounForm=formCount;
        formCount++;
      }
    }
  }
  iCount=formCount;
  transferDBUsagePatternsToUsagePattern(128,UPDB,0,iCount);
  transferFormUsagePatternsToCosts(sameNameForm,properNounForm,iCount);
  //  transfer noun SINGULAR_NOUN_HAS_DETERMINER,SINGULAR_NOUN_HAS_NO_DETERMINER,
  transferDBUsagePatternsToUsagePattern(64,UPDB,SINGULAR_NOUN_HAS_DETERMINER,2);
  transferUsagePatternsToCosts(HIGHEST_COST_OF_INCORRECT_NOUN_DET_USAGE,SINGULAR_NOUN_HAS_DETERMINER,2);
  // transfer verb  VERB_HAS_0_OBJECTS,VERB_HAS_1_OBJECTS,VERB_HAS_2_OBJECTS,
  transferDBUsagePatternsToUsagePattern(64,UPDB,VERB_HAS_0_OBJECTS,3);
  transferUsagePatternsToCosts(HIGHEST_COST_OF_INCORRECT_VERB_USAGE,VERB_HAS_0_OBJECTS,3);
  // transfer LOWER_CASE_USAGE_PATTERN,PROPER_NOUN_USAGE_PATTERN,
  usagePatterns[LOWER_CASE_USAGE_PATTERN]=max(127,UPDB[LOWER_CASE_USAGE_PATTERN]);
  usagePatterns[PROPER_NOUN_USAGE_PATTERN]=max(127,UPDB[PROPER_NOUN_USAGE_PATTERN]);
}

// update tFI from the DB.  So, don't mark this with insertNewForms flag
bool tFI::updateFromDB(int wordId,unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int iMainEntryWordId,int iDerivationRules,int iSourceId,int formNum,wstring &word)
{ LFS
  index=wordId;
  if ((!(flags&queryOnLowerCase) && !(flags&queryOnAnyAppearance)) || ((iFlags&queryOnLowerCase) || (iFlags&queryOnAnyAppearance))) // update rejected.
  {
    flags|=updateMainInfo; // update database
    return false;
  }
  count=iCount;
  int oldAllocated=allocated;
  if (fACount+count>=allocated)
    formsArray=(unsigned int *)trealloc(17,formsArray,oldAllocated*sizeof(*formsArray),(allocated=(allocated+count)*2)*sizeof(*formsArray));
  formsOffset=fACount;
  fACount+=count;
  inflectionFlags=iInflectionFlags;
  flags=iFlags;
  timeFlags=iTimeFlags;
  derivationRules=iDerivationRules;
  tmpMainEntryWordId=iMainEntryWordId;
  //index=-1;
  sourceId=iSourceId;
  flags&=~updateMainInfo; // update from database accepted.  Don't update database
  transferFormsAndUsage(forms,iCount,formNum,word);
  count=iCount;
  return true;
}

#define NUM_WORD_ALLOCATION 102400
void WordClass::assign(int wordId,tIWMM iWord)
{ LFS
  while (idsAllocated<=wordId)
  {
		int previousIdsAllocated=idsAllocated;
    idsAllocated+=NUM_WORD_ALLOCATION;
    idToMap=(tIWMM *)trealloc(22,idToMap,previousIdsAllocated,idsAllocated*sizeof(*idToMap));
    if (!idToMap)
      lplog(LOG_FATAL_ERROR,L"Out of memory allocating %d bytes to idToMap array.",idsAllocated*sizeof(*idToMap));
		for (int I = idsAllocated - NUM_WORD_ALLOCATION; I < idsAllocated; I++)
			idToMap[I] = wNULL;
    //memset(idToMap+idsAllocated-NUM_WORD_ALLOCATION,wNULL,NUM_WORD_ALLOCATION*sizeof(*idToMap));
  }
  #ifdef LOG_WORD_FLOW
		lplog(L"Word %s assigned to index %d.",iWord->first.c_str(),wordId);
	#endif
  idToMap[wordId]=iWord;
  iWord->second.index=wordId;
}

void tFI::mainEntryCheck(const wstring first,int where)
{ LFS
  if (mainEntry==wNULL && (query(nounForm)>=0 || query(verbForm)>=0 || query(adjectiveForm)>=0 || query(adverbForm)>=0))
  {
		if (!iswalpha(first[0])) return;
    ::lplog(LOG_INFO,L"Word %s has no mainEntry CHECK(%d)!",first.c_str(),where);
    flags|=tFI::queryOnAnyAppearance;
  }
}

void WordClass::readForms(MYSQL &mysql, wchar_t *qt)
{ LFS
  MYSQL_RES *result=NULL;
  MYSQL_ROW sqlrow;
  _snwprintf(qt,query_buffer_len,L"SELECT id,name,shortName,inflectionsClass,hasInflections,isTopLevel,properNounSubClass,blockProperNounRecognition from forms");
  if (lastReadfromDBTime>0)
    _snwprintf(qt+wcslen(qt),query_buffer_len-wcslen(qt),L" where UNIX_TIMESTAMP(ts)>%d",lastReadfromDBTime);
  if (!myquery(&mysql,qt,result)) return;
  unsigned int numDbForms=0,newDbForms=0;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
    wstring name,shortName,inflectionsClass;
		mTW(sqlrow[1],name);
		mTW(sqlrow[2],shortName);
		mTW(sqlrow[3],inflectionsClass);
    unsigned int index=atoi(sqlrow[0]);
    numDbForms=max(numDbForms,index);
    if (FormsClass::findForm(name)<0)
    {
      int hasInflections=sqlrow[4][0],isTopLevel=sqlrow[5][0],properNounSubClass=sqlrow[6][0],blockProperNounRecognition=sqlrow[7][0];
      #ifdef LOG_WORD_FLOW
        lplog(L"form=%s shortForm=%s inflectionsClass=%s hasInflections=%d properNounSubClass=%d isTopLevel=%d blockProperNounRecognition=%d.",
          name.c_str(),shortName.c_str(),inflectionsClass.c_str(),hasInflections,properNounSubClass,isTopLevel,blockProperNounRecognition);
      #endif
      newDbForms++;
			Forms.push_back(new FormClass(index,name,shortName,inflectionsClass,hasInflections!=0,properNounSubClass!=0,isTopLevel!=0,false,false,blockProperNounRecognition!=0)); // ,Forms.size() removed
			FormsClass::formMap[name]=Forms.size()-1;
    }
  }
  mysql_free_result(result);
  // if there are new forms in the database AND new forms in memory, this is incompatible!
  if (lastReadfromDBTime>0 && newDbForms>0 && Forms.size()>numDbForms)
    lplog(LOG_FATAL_ERROR,L"New forms discovered in db AND new forms already in memory!  Exiting!");
}

// called by readWordsFromDB (in this file)
// Reads word ids into words with numWordForms filled with the number of wordforms, and 
//   wordForms filled with the forms of the words, with counts the number of wordForms for each word.
// if return code is 0, parent calling this procedure should return with a 0.
// if return code is 2, parent calling this procedure should continue.
// qt is a scratch query buffer.
int WordClass::readWordFormsFromDB(MYSQL &mysql,int sourceId,wstring sourcePath,int maxWordId,wchar_t *qt,	int *words,int *counts,int &numWordForms,unsigned int * &wordForms, bool printProgress)
{ LFS
  memset(words,-1,maxWordId*sizeof(int));
  memset(counts,0,maxWordId*sizeof(int));
  bool testCacheSuccess=false;
  #ifdef USE_TEST_CACHE
    if (sourceId==READ_WORDS_FOR_ALL_SOURCE && lastReadfromDBTime==-1)
    {
      int testfd=open("wordFormCache",O_RDWR|O_BINARY);
      if (testfd>=0)
      {
        int checkMaxWordId;
        ::read(testfd,&checkMaxWordId,sizeof(checkMaxWordId));
        if (checkMaxWordId==maxWordId)
        {
          ::read(testfd,words,maxWordId*sizeof(int));
          ::read(testfd,counts,maxWordId*sizeof(int));
          ::read(testfd,&numWordForms,sizeof(numWordForms));
          wordForms=(unsigned int *)tmalloc(numWordForms*sizeof(int)*2);
          ::read(testfd,wordForms,numWordForms*sizeof(int)*2);
          testCacheSuccess=numWordForms>0;
        }
        close(testfd);
      }
    }
  #endif
  if (!testCacheSuccess)
  {
		MYSQL_RES *result=NULL;
		MYSQL_ROW sqlrow;
    int len=_snwprintf(qt,query_buffer_len,L"select wf.wordId, wf.formId, wf.count from wordForms wf, words w where w.id=wf.wordId");
    if (sourceId==READ_WORDS_FOR_ALL_SOURCE)
    {
      len+=_snwprintf(qt+len,query_buffer_len-len,L" AND w.sourceId IS NULL");
      if (lastReadfromDBTime>0)
        len+=_snwprintf(qt+len,query_buffer_len-len,L" AND UNIX_TIMESTAMP(w.ts)>%d",lastReadfromDBTime);
    }
    else if (sourceId>=0) len+=_snwprintf(qt+len,query_buffer_len-len,L" AND w.sourceId=%d",sourceId);
    _snwprintf(qt+len,query_buffer_len-len,L" ORDER BY wordId, formId");
		if (printProgress)
			wprintf(L"Querying word forms from database (sourceId=%d)...                        \r",sourceId);
    if (!myquery(&mysql,qt,result)) return -1;
      numWordForms=(int)mysql_num_rows(result);
    wordForms=(unsigned int *)tmalloc(numWordForms*sizeof(int)*2);
    int wf=0;
		if (printProgress)
			wprintf(L"Reading word forms from database...                        \r");
    while ((sqlrow=mysql_fetch_row(result))!=NULL)
    {
      int wordId=atoi(sqlrow[0]);
      if (wordId<0 || wordId>maxWordId)
        lplog(LOG_FATAL_ERROR,L"wordId < 0 or > %d.",maxWordId);
      if (words[wordId]==-1) words[wordId]=wf;
      counts[wordId]++;
      wordForms[wf++]=atoi(sqlrow[1])-1; // formId -1 yields form offset in memory
      wordForms[wf++]=atoi(sqlrow[2]); // count
    }
    mysql_free_result(result);
    if (wf==0 && sourceId>0)
    {
      if (!sourcePath.empty())
        readWords(sourcePath,sourceId); // no wordRelations possible because none of these exist in DB
			if (printProgress)
				wprintf(L"Finished reading from database...                                               \r");
      tfree(maxWordId*sizeof(int),counts);
      tfree(maxWordId*sizeof(int),words);
      tfree(numWordForms*sizeof(int)*2,wordForms);
      //lplog(L"TIME DB READ END (4) for source %d:%s",sourceId,getTimeStamp());
      return 0;
    }
    #ifdef USE_TEST_CACHE
      if (sourceId==READ_WORDS_FOR_ALL_SOURCE && lastReadfromDBTime==-1)
      {
        int testfd=open("wordFormCache",O_WRONLY|O_CREAT|O_BINARY,_S_IREAD|_S_IWRITE);
        if (testfd>=0)
        {
          ::write(testfd,&maxWordId,sizeof(maxWordId));
          ::write(testfd,words,maxWordId*sizeof(int));
          ::write(testfd,counts,maxWordId*sizeof(int));
          ::write(testfd,&numWordForms,sizeof(numWordForms));
          ::write(testfd,wordForms,numWordForms*sizeof(int)*2);
          close(testfd);
        }
      }
    #endif
  }
	return 2;
}

int getMaxWordId(MYSQL &mysql)
{ LFS
  MYSQL_RES *result=NULL;
  MYSQL_ROW sqlrow;
  if (!myquery(&mysql,L"select MAX(id) from words",result)) return -1;
  if ((sqlrow=mysql_fetch_row(result))==NULL) return -1;
  int maxWordId=(sqlrow[0]==NULL) ? 0 : atoi(sqlrow[0])+1;
  mysql_free_result(result);
	return maxWordId;
}

// called by readWithLock (in this file)
// Ways this function is used:
// sourceId==READ_WORDS_FOR_ALL_SOURCE. update read: read NULL source since lastReadfromDBTime and update or insert the entries into memory (lastReadfromDBTime>0)
// sourceId>=0. source read: read all source words belonging to a certain source
// only pay attention to lastReadfromDBTime IF sourceId==READ_WORDS_FOR_ALL_SOURCE.  If sourceId is >=0, then the source was never read before (sources are never repeated).
int WordClass::readWordsFromDB(MYSQL &mysql,int sourceId,wstring sourcePath,bool generateFormStatisticsFlag,int &numWordsInserted,int &numWordsModified, bool printProgress)
{ LFS
	if (printProgress)
		wprintf(L"Started reading from database...                        \r");
  //lplog(L"TIME DB READ START for source %d:%s",sourceId,getTimeStamp());
  MYSQL_RES *result=NULL;
  MYSQL_ROW sqlrow;
  wchar_t qt[query_buffer_len_overflow];
  if (sourceId==READ_WORDS_FOR_ALL_SOURCE)
    readForms(mysql,qt);
  if (nounForm==-1) nounForm=FormsClass::gFindForm(L"noun");
  if (verbForm==-1) verbForm=FormsClass::gFindForm(L"verb");
  if (adjectiveForm<0) adjectiveForm=FormsClass::gFindForm(L"adjective");
  if (adverbForm<0) adverbForm=FormsClass::gFindForm(L"adverb");
  if (timeForm<0) timeForm=FormsClass::gFindForm(L"time");
  numberForm=NUMBER_FORM_NUM;
  FormsClass::changedForms=false;
	int maxWordId=getMaxWordId(mysql);
	if (maxWordId<=0) return maxWordId;
  if (sourceId==READ_WORDS_FOR_ALL_SOURCE)
  {
		// have to do it twice to catch mainEntryWords that point to other mainEntryWords
	  myquery(&mysql,L"update words w inner join words mw on w.mainEntryWordId=mw.id set mw.sourceId=NULL where w.sourceId is NULL");
	  myquery(&mysql,L"update words w inner join words mw on w.mainEntryWordId=mw.id set mw.sourceId=NULL where w.sourceId is NULL");
  }
  else if (sourceId>=0)  
	{
		wsprintf(qt,L"update words w inner join words mw on w.mainEntryWordId=mw.id set mw.sourceId=%d where w.sourceId=%d and mw.sourceId is not null",sourceId,sourceId);
		myquery(&mysql,qt);
		myquery(&mysql,qt);
	}
  // read wordForms of the words
  int *words=(int *)tmalloc(maxWordId*sizeof(int));
  int *counts=(int *)tmalloc(maxWordId*sizeof(int));
  int numWordForms;
  unsigned int *wordForms;
	if (readWordFormsFromDB(mysql,sourceId,sourcePath,maxWordId,qt,words,counts,numWordForms,wordForms, printProgress)==0) return 0;

  // read words
  tIWMM iWord=wNULL;
  int len=_snwprintf(qt,query_buffer_len,L"select id, word, inflectionFlags, flags, timeFlags, mainEntryWordId, derivationRules from words");
  if (sourceId==READ_WORDS_FOR_ALL_SOURCE)
  {
    len+=_snwprintf(qt+len,query_buffer_len-len,L" where sourceId IS NULL");
    if (lastReadfromDBTime>0)
      len+=_snwprintf(qt+len,query_buffer_len-len,L" AND UNIX_TIMESTAMP(ts)>%d",lastReadfromDBTime);
  }
  else if (sourceId>=0)  len+=_snwprintf(qt+len,query_buffer_len-len,L" where sourceId=%d",sourceId);
  _snwprintf(qt+len,query_buffer_len-len,L" ORDER BY word");
  if (!myquery(&mysql,qt,result)) return -1;
  __int64 numRows=mysql_num_rows(result);
  int where,lastProgressPercent=-1,numWords=0;
	vector <tIWMM> wordsToResolve;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
    int wordId=atoi(sqlrow[0]),inflectionFlags=atoi(sqlrow[2]),flags=atoi(sqlrow[3]),timeFlags=atoi(sqlrow[4]),mainEntryWordId=(sqlrow[5]==NULL) ? -1:atoi(sqlrow[5]),derivationRules=(sqlrow[6]==NULL) ? 0:atoi(sqlrow[6]);
    wstring sWord;
		mTW(sqlrow[1],sWord);
    if (words[wordId]==-1)
		{
			if (!iswdigit(sWord[0]) && sWord[0])
				lplog(LOG_ERROR,L"No forms for %s (wordId=%d) found!",sWord.c_str(),wordId);
		  continue;
		}
    int selfFormNum=FormsClass::findForm(sWord);
		tIWMM tryWord;
    if ((sourceId!=-1 || lastReadfromDBTime>=0) && (tryWord=WMM.find(sWord))!=WMM.end())
    {
			iWord=tryWord;
      #ifdef LOG_WORD_FLOW
        lplog(L"Updated (from sourceId %d) read word %s.",sourceId,sWord.c_str());
      #endif
      if (iWord->second.index<0)
        assign(wordId,iWord);
      if (iWord->second.updateFromDB(wordId,wordForms+words[wordId],counts[wordId],inflectionFlags,flags,timeFlags,mainEntryWordId,derivationRules,sourceId,selfFormNum,sWord))
        numWordsModified++;
    }
    else
    {
      iWord=WMM.insert(tWFIMap(sWord,tFI(wordForms+words[wordId],counts[wordId],inflectionFlags,flags,timeFlags,mainEntryWordId,derivationRules,sourceId,selfFormNum,sWord))).first;
      numWordsInserted++;
      #ifdef LOG_WORD_FLOW
        lplog(L"Inserted (from sourceId %d) read word %s.",sourceId,sWord.c_str());
      #endif
      assign(wordId,iWord);
    }
		if (mainEntryWordId>=0)
			wordsToResolve.push_back(iWord);
    if ((where=numWords++*100/(int)numRows)>lastProgressPercent)
    {
			if (printProgress)
				wprintf(L"PROGRESS: %03d%% words read with %04d seconds elapsed\r",where,clocksec());
      lastProgressPercent=where;
    }
    if (iWord->second.query(timeForm)<0 && iWord->second.query(numberForm)<0 && iWord->second.query(telenumForm)<0 &&
				handleExtendedParseWords((wchar_t *)sWord.c_str())) // if handleExtendedParseWords is true, removed word
			iWord=wNULL;
  }
  mysql_free_result(result);
  tfree(maxWordId*sizeof(int),counts);
  tfree(maxWordId*sizeof(int),words);
  tfree(numWordForms*sizeof(int)*2,wordForms);
  int numMainEntriesNotFound=0;
	for (vector<tIWMM>::iterator wi=wordsToResolve.begin(),wiEnd=wordsToResolve.end(); wi!=wiEnd; wi++)
	{
		if ((*wi)->second.tmpMainEntryWordId>=idsAllocated || (*wi)->second.tmpMainEntryWordId<0 || (idToMap[(*wi)->second.tmpMainEntryWordId]==wNULL && (*wi)->second.tmpMainEntryWordId!=0))
    {
      numMainEntriesNotFound++;
			lplog(L"mainEntryWordId %d not found for word %s (sourceId=%d lastReadfromDBTime=%d)!",(*wi)->second.tmpMainEntryWordId,(*wi)->first.c_str(),sourceId,lastReadfromDBTime); // mainEntries and any references must always have the same source
			(*wi)->second.mainEntry=*wi; 
    }
		else
		{
			(*wi)->second.mainEntry=idToMap[(*wi)->second.tmpMainEntryWordId];
			if ((*wi)->second.mainEntry!=wNULL)
				mainEntryMap[(*wi)->second.mainEntry].push_back(*wi);
		}
  }
  if (generateFormStatisticsFlag) 
		generateFormStatistics();
  if ((sectionWord=WMM.find(L"|||"))==WMM.end())
  {
    bool added;
    addNewOrModify(NULL,wstring(L"|||"),tFI::topLevelSeparator,SECTION_FORM_NUM,0,0,L"section",-1,added);
    sectionWord=WMM.find(L"|||");
  }
  if (sourceId==READ_WORDS_FOR_ALL_SOURCE)
  {
    if (!myquery(&mysql,L"select UNIX_TIMESTAMP(NOW())",result)) return -1;
    if ((sqlrow=mysql_fetch_row(result))==NULL) return -1;
    if (sqlrow[0]==NULL)
    {
			if (printProgress)
				wprintf(L"Finished reading from database...                                               \r");
      mysql_free_result(result);
      return 0; // no words to read
    }
    lastReadfromDBTime=atoi(sqlrow[0])+1;
    mysql_free_result(result);
  }
	if (printProgress)
		wprintf(L"Finished reading from database...                                                  \r");
  return 0;
}

int WordClass::readWithLock(MYSQL &mysql,int sourceId,wstring sourcePath,bool generateFormStatistics, bool printProgress)
{ LFS
  int startTime=clock();
	#ifdef WRITE_WORDS_AND_FORMS_TO_DB
		acquireLock(mysql,true);
	#endif
  int numWordsInserted=0,numWordsModified=0;
  int ret=readWordsFromDB(mysql,sourceId,sourcePath, generateFormStatistics,numWordsInserted,numWordsModified, printProgress);
  myquery(&mysql,L"UNLOCK TABLES");
	#ifdef WRITE_WORDS_AND_FORMS_TO_DB
		releaseLock(mysql);
	#endif
	if (numWordsInserted || numWordsModified)
    lplog(L"%d words inserted into and %d words modified in memory in %d seconds.",numWordsInserted,numWordsModified,(clock()-startTime)/CLOCKS_PER_SEC);
  return ret;
}

int Source::writeSource()
{ LFS
  int ret=0;
	#ifdef WRITE_WORDS_AND_FORMS_TO_DB
		if (!Words.acquireLock(mysql,true)) return 0;
		ret=Words.writeWordsToDB(mysql,sourcePath,sourceId);
		ret=Words.readWordIndexesFromDB(mysql);
  #endif
  #ifdef WRITE_WORDS_AND_FORMS_TO_DB
		Words.releaseLock(mysql);
	#endif
  #ifdef WRITE_WORD_RELATIONS
		Words.flushWordRelations(mysql,sourceId); 
	#endif
  #ifdef WRITE_MULTI_WORD_RELATIONS
		set <int> objectsToFlush;
		flushMultiWordRelations(mysql,objectsToFlush);
    ret=flushObjects(objectsToFlush);
  #endif
  return ret;
}

int readProcessedLocationsInSource(wstring path)
{ LFS
	path+=L".SourceCache";
	IOHANDLE fd=_wopen(path.c_str(),O_RDWR|O_BINARY);
	if (fd<0) return -1;
	int bufferlen=min(1024,filelength(fd)),where=0;
	void *buffer=(void *)tmalloc(bufferlen+10);
	::read(fd,buffer,bufferlen);
	close(fd);
	wstring location=(wchar_t *)((char *)buffer+where);
  where+=(location.length()+1)*sizeof(location[0]);
  unsigned int count=*((unsigned int *)(((char *)buffer)+where));
	tfree(bufferlen,buffer);
	return count;
}

#define editor L"C:\\Program Files (x86)\\Notepad++\\notepad++.exe"
// also can use UltraEdit
void createEditSession(wstring &path)
{ LFS
		STARTUPINFO si;
		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi;
		ZeroMemory( &pi, sizeof(pi) );
		wchar_t processParameters[1024];
		wsprintf(processParameters,L"\"%s\" \"%s\"",editor,path.c_str());
		if( !CreateProcess( editor, // No module name (use command line)
			processParameters, // Command line
			NULL, // Process handle not inheritable
			NULL, // Thread handle not inheritable
			FALSE, // Set handle inheritance to FALSE
			0,
			NULL, // Use parent's environment block
			NULL, // Use parent's starting directory 
			&si, // Pointer to STARTUPINFO structure
			&pi ) // Pointer to PROCESS_INFORMATION structure
			) 
		{
			printf( "CreateProcess failed (%d).\n", (int)GetLastError());
		}
}

// for all the texts in the DB, compare the current way with what is already in the DB.
#define MAX_BUF 1024*1024
#define MAX_PERCENT 100000
int Source::testStartCode()
{ LFS
	if (!myquery(&mysql,L"LOCK TABLES sources WRITE,sources s WRITE,wordRelationsHistory wrh WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
	_snwprintf(qt,query_buffer_len,
		L"select count(wrh.sourceId), s.id, s.path, s.start, s.repeatStart, s.author, s.title from wordRelationsHistory wrh, sources s where s.id=wrh.sourceId and sourceType=2 group by wrh.sourceId order by COUNT(sourceId) ASC");
	MYSQL_RES *result;
  MYSQL_ROW sqlrow=NULL;
  if (!myquery(&mysql,qt,result)) return -1;
	logCache=0;
	int numDiff=0,missedStarts=0,numSources=0;
	int highestPercentWRC=0,lowestPercentWRC=MAX_PERCENT,highestPercentProcessed=0,lowestPercentProcessed=MAX_PERCENT;
	__int64 totalWRC=0,totalBufferLength=0,totalProcessed=0;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
		int numWordRelations=atoi(sqlrow[0]);
		totalWRC+=numWordRelations;
		numSources++;
		sourceId=atoi(sqlrow[1]);
    wstring tp,path=wstring(CACHEDIR)+mTW(sqlrow[2],tp);
		wchar_t *extensions[]={ L".sourceCache",L".sourceCache.2",L".WNCache",L".wordCacheFile", NULL};
		for (int I=0; extensions[I]; I++)
		{
			wstring pathext=path+extensions[I];
			//_wremove(pathext.c_str());
		}
		int numProcessedLocations=readProcessedLocationsInSource(path);
		if (numProcessedLocations<0)
		{
				wchar_t temp[1024];
				wsprintf(temp,L"update sources set processed=null where id=%d",sourceId);
				myquery(&mysql,temp);
				continue;
		}
		totalProcessed+=numProcessedLocations;
    wstring start,DBStart;
		mTW(sqlrow[3],DBStart);
    int repeatStart,DBRepeatStart=atoi(sqlrow[4]);
		wstring author;
		mTW(sqlrow[5],author);
		wstring title;
		mTW(sqlrow[6],title);
		wstring buffer;
		wchar_t *cBuffer=(wchar_t *)tmalloc(MAX_BUF*10);
		int actualLenInBytes;
		if (!getPath(path.c_str(),cBuffer,MAX_BUF*10,actualLenInBytes))
		{
			wchar_t wch=cBuffer[100];
			cBuffer[100]=0;
			bool nlfound=wcschr(cBuffer,'\n')!=NULL;
			cBuffer[100]=wch;
			if ((cBuffer[0]==65279 || iswalnum(cBuffer[0])) && nlfound)
				buffer=cBuffer;
			else
				mTW((char *)cBuffer,buffer);
		}
		tfree(MAX_BUF*10,cBuffer);
		totalBufferLength+=actualLenInBytes;
		findStart(buffer,start,repeatStart,title);
		//if (start!=DBStart || DBRepeatStart>repeatStart)
		//{
			wstring firstLine;
			// take only the first line of start
			if (start.find(L'\n')!=wstring::npos)
			{
				firstLine=start.substr(0,start.find(L'\n')-1);
				//	if (firstLine==DBStart && (repeatStart==DBRepeatStart))
				//		continue;
			}
			bookBuffer=(wchar_t *)buffer.c_str();
			bufferLen=buffer.length();
			bufferScanLocation=0;
			scanUntil(start.c_str(),repeatStart,true);
			__int64 ws=bufferScanLocation;
			bufferScanLocation=0;
			scanUntil(DBStart.c_str(),DBRepeatStart,true);
			__int64 DBws=bufferScanLocation;
			if (ws<DBws) ws+=start.length();
			if (DBws<ws) DBws+=DBStart.length();
			int adjustForGutenbergText=max(1,(actualLenInBytes-(39*1024)));
			int currentPercentProcessed=numProcessedLocations*100/adjustForGutenbergText;
			lowestPercentProcessed=min(lowestPercentProcessed,currentPercentProcessed);
			highestPercentProcessed=max(highestPercentProcessed,currentPercentProcessed);
			int currentPercentWRC=numWordRelations*100*10/numProcessedLocations;
			lowestPercentWRC=min(lowestPercentWRC,currentPercentWRC);
			highestPercentWRC=max(highestPercentWRC,currentPercentWRC);
			wstring warning;
			if (currentPercentWRC<15 || currentPercentProcessed<8) 
			{
					warning=L"**";
					createEditSession(path);
			}
			lplog(LOG_INFO,L"%s%05d:%02d%% Processed (%d):%02d%% WRC (%d):%s",
				warning.c_str(),sourceId,currentPercentProcessed,numProcessedLocations,currentPercentWRC,numWordRelations,title.c_str());
			updateSourceStatistics2(actualLenInBytes,numWordRelations);
			if (((start!=DBStart && (firstLine!=DBStart || firstLine.empty())) && abs(ws-DBws)>20) || (currentPercentWRC<30))
			{
				missedStarts++;
				if (DBws!=ws)
				{
					wprintf(L"%d:%s\\%s \nDB:%s[%d]\nNEW:%s[%d]\n%I64d!=%I64d\n",numDiff,
						author.c_str(),title.c_str(),DBStart.c_str(),DBRepeatStart,start.c_str(),repeatStart,DBws,ws);
					lplog(LOG_INFO,L"%d:%s\\%s \nDB: %s[%d]\nNEW: %s[%d]\n%I64d!=%I64d\n",numDiff++,
						author.c_str(),title.c_str(),DBStart.c_str(),DBRepeatStart,start.c_str(),repeatStart,DBws,ws);
				}
				else
				{
					wprintf(L"%d:%s\\%s \n%s[%d]@%I64d\n",numDiff,
						author.c_str(),title.c_str(),DBStart.c_str(),DBRepeatStart,DBws);
					lplog(LOG_INFO,L"%d:%s\\%s \n%s[%d]@%I64d\n",numDiff++,
						author.c_str(),title.c_str(),DBStart.c_str(),DBRepeatStart,DBws);
				}
				/*
				wstring sqlStatement;
				wchar_t temp[1024];
				escapeStr(start);
				sqlStatement=L"update sources set start='"+start+L"', repeatStart="+itos(repeatStart,tmp)+L" where id='"+wstring(_itow(id,temp,10))+L"'";
				myquery(&mysql,(wchar_t *)sqlStatement.c_str());
				*/
			}
		//}
  }
	wprintf(L"%d %d",numDiff,missedStarts);
	lplog(LOG_INFO,L"numSources: %d totalBufferLength:%I64d totalProcessed=%I64d totalWRC: %I64d ",numSources,totalBufferLength,totalProcessed,totalWRC);
	lplog(LOG_INFO,L"AVERAGE Processed: %02d%%",totalProcessed*100/totalBufferLength);
	lplog(LOG_INFO,L"HIGHEST Processed: %02d%%",highestPercentProcessed);
	lplog(LOG_INFO,L"LOWEST Processed: %02d%%",lowestPercentProcessed);
	lplog(LOG_INFO,L"AVERAGE WRC: %02d%%",totalWRC*100*10/totalProcessed);
	lplog(LOG_INFO,L"HIGHEST WRC: %02d%%",highestPercentWRC);
	lplog(LOG_INFO,L"LOWEST WRC: %02d%%",lowestPercentWRC);
  mysql_free_result(result);
  unlockTables();
	return 0;
}

int readWikiNominalizations(MYSQL &mysql, unordered_map <wstring,set < wstring > > &agentiveNominalizations)
{ LFS
	MYSQL_RES *result=NULL;
	MYSQL_ROW sqlrow;
	if (!myquery(&mysql,L"select noun,lower(definition) from wiktionarynouns where lower(definition) like '%gent noun of%' or lower(definition) like '%one who %'",result)) return -1;
	//int previousId=-1;
	while ((sqlrow=mysql_fetch_row(result))!=NULL)
	{
		wstring noun,verb,definition;
		mTW(sqlrow[0],noun);
    mTW(sqlrow[1],definition);
		size_t pos=definition.find(L"gent noun of");
		if (pos!=wstring::npos)
		{
			pos+=wcslen(L"gent noun of ");
			for (; iswalpha(definition[pos]) || definition[pos]=='-'; pos++)
				verb+=definition[pos];
		}
		else
		{
			size_t pos2=definition.find(L"one who ");
			if (pos2!=wstring::npos)
			{
				pos2+=wcslen(L"one who ");
				for (; iswalpha(definition[pos2]) || definition[pos2]=='-'; pos2++)
					verb+=definition[pos2];
				if (verb.empty() || verb==L"is" || verb[verb.length()-1]!=L's' || verb[0]!=noun[0])
					verb.clear();
			}
		}
		if (!verb.empty())
		{
			agentiveNominalizations[noun].insert(verb);
			lplog(LOG_WHERE,L"WK %s %s",noun.c_str(),verb.c_str());
		}
	}
	return 0;
}

int Source::writeParseRequestToDatabase(vector <searchSource>::iterator pri)
{
	wchar_t qt[16384];
	wstring pathInCache = pri->pathInCache;
	escapeStr(pathInCache);
	wsprintf(qt, L"INSERT INTO parseRequests (typeId, status, fullWebPath, pathInCache) VALUES (%d,%d,\"%s\",\"%s\")",
		(pri->isSnippet) ? 0 : 1,
		0,
		pri->fullWebPath.c_str(),
		pathInCache.c_str());
	return myquery(&mysql, qt);
}

