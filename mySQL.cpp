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

/* how to import Gutenberg 
1. get catalog.rdf - use ultraedit
2. delete all entries except for the etext entries
3. remove all rdf pre-terms like dc: and rdf:
4. change all <etext to INSERT INTO MyTempTable VALUES ('<etext and so forth in ultra edit
5. copy to SQL Server window
create Wikipedia database
CREATE TABLE MyTempTable (VXML xml);
INSERT INTO MyTempTable VALUES ('<etext ID="etext34796">
	<title parseType="Literal">William Shakespeare as he lived.An Historical Tale</title>
	<creator parseType="Literal">Curling, Henry</creator>
	<friendlytitle parseType="Literal">William Shakespeare as he lived. by Henry Curling</friendlytitle>
	<language><ISO639-2><value>en</value></ISO639-2></language>
	<created><W3CDTF><value>2010-12-30</value></W3CDTF></created>
</etext>');
CREATE TABLE sources
		(ID varchar(max),
		author varchar(max),
	 contributor varchar(max),
	 title varchar(max),
	 lang varchar(max),
	 subj varchar(max),
	 date varchar(max)
		) ;
INSERT into sources  SELECT 
		[VXML].value('(etext/@ID)[1]','varchar(max)') ID, 
		[VXML].value('(etext/creator)[1]','varchar(max)') author, 
		[VXML].value('(etext/contributor)[1]','varchar(max)') contributor, 
		[VXML].value('(etext/title)[1]','varchar(max)') title,
		[VXML].value('(etext/language/ISO639-2/value)[1]','varchar(max)') lang,
		[VXML].value('(etext/subject/LCC/value)[1]','varchar(max)') subj,
		[VXML].value('(etext/created/W3CDTF/value)[1]','varchar(max)') date
FROM 
MyTempTable 
order by author

select ID, REPLACE(contributor,',',' ') as author,LEFT(REPLACE(REPLACE(title,',',' '),' ',' '),80) as title from sources where (author is NULL AND contributor is NOT NULL) and lang='en' and subj like 'P%'
UNION
select ID, REPLACE(author,',',' '),LEFT(REPLACE(REPLACE(title,',',' '),' ',' '),80) as title from sources where (author is NOT NULL) and lang='en' and subj like 'P%'
6. save result of above as a CSV delimited file.  replace each 0A with 20 (in hex mode), replace , with ',' and etext with ' and $^n with '^n
CREATE TABLE `gsource` (
	`id` int(11) unsigned NOT NULL AUTO_INCREMENT,
	`etext` varchar(10) CHARACTER SET utf8 DEFAULT NULL,
	`author` varchar(64) CHARACTER SET utf8 DEFAULT NULL,
	`title` varchar(128) CHARACTER SET utf8 DEFAULT NULL,
	`date` datetime DEFAULT NULL,
	UNIQUE KEY `id` (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=latin1 
sample line: '10122','Graves  Robert  1895-1985','Fairies and Fusiliers'
in mysql: load data infile 'D:\\lp\\catalog2.txt' into table gsource FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '\''  LINES TERMINATED BY '\r\n' (etext,author,title,date);
update gsource,sources set sources.etext=gsource.etext where sources.title=gsource.title;
update gsource,sources set sources.date=gsource.date where sources.title=gsource.title;
 delete gsource from gsource, sources where sources.title=gsource.title;

insert sources (etext, sourceType, path, start, repeatStart, author, title, date) select etext,2,'**RETREIVE**','',-1,author,title,date from gsource where author like '%Lytton%' and title NOT LIKE '%0%' AND title not like '%Volume%' and title not like '%Book%';
insert sources (etext, sourceType, path, start, repeatStart, author, title, date) select etext,2,'**RETREIVE**','',-1,author,title,date from gsource where author like '%Wymark%' and title NOT LIKE '%Book%' AND title not like '%Part%' and title not like '%Index%';
*/
#define query_buffer_len (1024*256-1024*2)
#define query_buffer_len_overflow query_buffer_len+1024
#define query_buffer_len_underflow query_buffer_len-1024
bool alreadyConnected=false;

void tFI::allocateMap(int relationType)
{
  if (relationMaps[relationType]==NULL)
    relationMaps[relationType]=new cRMap;
}

bool myquery(MYSQL *mysql,wchar_t *q)
{
	static void *buffer=NULL;
	static unsigned int bufSize=0;

	int queryLength=WideCharToMultiByte( CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL );
	if (bufSize==0)
	{
		bufSize=max(queryLength*2,10000);
		buffer=tmalloc(bufSize);
		if (!WideCharToMultiByte( CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL ))
		{
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",q);
			return false;
		}
	}
	if (!queryLength)
	{
		if (GetLastError()!=ERROR_INSUFFICIENT_BUFFER) 
		{
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",q);
			return false;
		}
		queryLength=WideCharToMultiByte( CP_UTF8, 0, q, -1, NULL, 0, NULL, NULL );
		unsigned int previousBufSize=bufSize;
		bufSize=queryLength*2;
		if (!(buffer=trealloc(21,buffer,previousBufSize,bufSize)))
		{
			lplog(LOG_FATAL_ERROR,L"Out of memory requesting %d bytes from sql query %s!",bufSize,q);
			return false;
		}
		if (!WideCharToMultiByte( CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL ))
		{
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",q);
			return false;
		}
	}
	int seconds=clock();
	if (extraDetailedLogging) 
	{
		lplog(LOG_INFO,L"Executing %S",buffer);
		lplog(LOG_INFO,NULL);
	}
  if(mysql_real_query(mysql, (char *)buffer, queryLength )!=0)/*success*/
  {
    //if (wcslen(q)>query_buffer_len) q[query_buffer_len]=0;
		lplog(LOG_ERROR,L"mysql_real_query failed - %S (len=%d): ", mysql_error(mysql),queryLength);
		logstring(LOG_ERROR,q);
    //logstring(-1,NULL);
		//exit(0);
  }
	//if ((seconds=clock()-seconds) && seconds/CLOCKS_PER_SEC && !extraDetailedLogging)
	//	lplog(L"%d seconds: query %s",seconds/CLOCKS_PER_SEC,q);
  return true;
}

bool myquery(MYSQL *mysql,wchar_t *q,MYSQL_RES * &result)
{
  if (!myquery(mysql,q))
    return false;
  if (!(result= mysql_store_result(mysql)))
    lplog(LOG_INFO,L"%S: Failed to retrieve any results for %s", mysql_error(mysql), q);
  return result!=NULL;
}

bool checkFull(MYSQL *mysql,wchar_t *qt,size_t &len,bool flush,wchar_t *qualifier)
{
  bool ret=true;
  if (len>query_buffer_len_underflow || (flush && qt[len-1]==L','))
  {
    if (qt[len-2]==L')')
      qt[--len]=0; // must be INSERT strip off extra ,
    else
      qt[len-1]=L')'; // must be IN - put in ending )
    if (qualifier!=NULL)
    {
      wcscpy(qt+len,qualifier);
      len+=wcslen(qualifier);
    }
		qt[len]=0;
    if(!myquery(mysql, qt ))
    {
      lplog(L"checkFull %S (len=%d): ", mysql_error(mysql),len);
				wcscat(qt,L"\n");
      logstring(LOG_INFO|LOG_FATAL_ERROR,qt);
    }
    wchar_t *ch=wcsstr(qt,L"VALUES");
    if (ch)
    {
      ch[7]=0; // must be INSERT
			len=ch+7-qt;
    }
    else if (ch=wcschr(qt,'('))
    {
      ch[1]=0; // must be IN
      len=ch+1-qt;
    }
    else
      lplog(LOG_FATAL_ERROR,L"Incorrect command %s given to checkFull",qt);
  }
  return ret;
}

unsigned long encodeEscape(MYSQL &mysql, wchar_t *to, wstring from)
{
	char *cFrom=wTM(from);
	char tmp[1024];
	mysql_real_escape_string(&mysql, tmp, cFrom, strlen(cFrom));
	wcscpy(to,mTW(tmp));
	return 0;
}


bool Source::resetAllSource()
{
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  myquery(&mysql,L"update sources set processed=NULL,processing=NULL");
  unlockTables();
  return true;
}

bool Source::unlockTables(void)
{
  return myquery(&mysql,L"UNLOCK TABLES");
}

bool Source::resetSource(int beginSource,int endSource)
{
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
  _snwprintf(qt,query_buffer_len,L"update sources set processed=NULL,processing=NULL where id>=%d and id<%d",beginSource,endSource);
  myquery(&mysql,qt);
  unlockTables();
  return true;
}

void Source::resetProcessingFlags(void)
{
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return;
  myquery(&mysql,L"update sources set processing=NULL");
  unlockTables();
}

bool Source::signalBeginProcessingSource(int sourceId)
{
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
  _snwprintf(qt,query_buffer_len,L"update sources set processing=true where id=%d",sourceId);
  myquery(&mysql,qt);
  unlockTables();
  return true;
}

bool Source::signalFinishedProcessingSource(int sourceId)
{
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
  _snwprintf(qt,query_buffer_len,L"update sources set processing=NULL, processed=true where id=%d",sourceId);
  myquery(&mysql,qt);
  unlockTables();
  return true;
}

MYSQL_ROW Source::getNextUnprocessedSource(int begin,int end,enum sourceType st)
{
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return false;
  wchar_t qt[query_buffer_len_overflow];
	_snwprintf(qt,query_buffer_len,L"select id, path, start, repeatStart, etext, author, title from sources where id>=%d and id<%d and sourceType=%d and processed IS NULL and processing IS NULL order by id ASC limit 1",
    begin,end,st);
  MYSQL_ROW sqlrow=NULL;
  MYSQL_RES *result;
  if (myquery(&mysql,qt,result))
  {
    if (sqlrow=mysql_fetch_row(result))
    {
      _snwprintf(qt,query_buffer_len,L"update sources set processing=true where id=%s",mTW(sqlrow[0]));
      myquery(&mysql,qt);
    }
    //mysql_free_result(result); sqlrow of this result is passed back to callee.  Memory leak.
  }
  unlockTables();
	if (sqlrow) sourceId=atoi(sqlrow[0]);
  return sqlrow;
}

void escapeStr(wstring &str)
{
	wstring ess;
	for (unsigned int I=0; I<str.length(); I++)
	{
		if (str[I]=='\'') ess+='\\';
		ess+=str[I];
		if (str[I]=='\\') ess+='\\';
	}
	str=ess;
}

bool Source::updateSource(wstring &path,wstring &start,int repeatStart,wstring &etext)
{
	wstring tmp,sqlStatement;
	escapeStr(path);
	escapeStr(start);
	sqlStatement=L"update sources set path='"+path+L"', start='"+start+L"', repeatStart="+itos(repeatStart,tmp)+L" where etext='"+etext+L"'";
	return myquery(&mysql,(wchar_t *)sqlStatement.c_str());
}

int Source::connectDatabase(wchar_t *where)
{
  if (alreadyConnected) return 0;
  if(mysql_init(&mysql)==NULL)
    lplog(LOG_FATAL_ERROR,L"Failed to initialize MySQL");
  my_bool keep_connect=true;
	mysql_options(&mysql,MYSQL_OPT_RECONNECT,&keep_connect);
  if (alreadyConnected=(mysql_real_connect(&mysql,wTM(where),"root","byron0",DBNAME,0,NULL,0)!=NULL))
	{
		mysql_options(&mysql,MYSQL_OPT_RECONNECT,&keep_connect);
		if (!mysql_set_character_set(&mysql, "utf8")) 
			lplog(L"Default client character set: %S\n", mysql_character_set_name(&mysql));
    return 0;
	}
	mysql_options(&mysql,MYSQL_OPT_RECONNECT,&keep_connect);
	if (!myquery(&mysql,L"CREATE TABLE wordRelations2 (id int(11) unsigned NOT NULL auto_increment unique, "
		L"lastWhere int NOT NULL, "
		L"fromWordId INT UNSIGNED NOT NULL, INDEX fw_ind (fromWordId), FOREIGN KEY (fromWordId) REFERENCES words(id), "
		L"toWordId INT UNSIGNED NOT NULL, INDEX tw_ind (toWordId), FOREIGN KEY (toWordId) REFERENCES words(id), "
		L"typeId SMALLINT UNSIGNED NOT NULL, "
		//"ct BIT NOT NULL, " // used when retrieving words only in the current source
		L"UNIQUE INDEX uw_ind (fromWordId,toWordId,typeId), " // must be kept for ON DUPLICATE KEY UPDATE logic in flushWordRelations
		L"totalCount INT NOT NULL, "
		L"ts TIMESTAMP, INDEX ts_ind (ts))")) return -1;
  return -1;
}

int Source::createObjectTables(void)
{
  // the name bit will be set so that objectWordMap orderOrType will be defined as a type,
  // hon,hon2,hon3; first; middle,middle2; last; suffix; any;
  if (!myquery(&mysql,L"CREATE TABLE objects (id int(11) unsigned NOT NULL auto_increment unique, objectClass INT NOT NULL, "
    L"numEncounters INT NOT NULL, numIdentifiedAsSpeaker INT NOT NULL, nickName INT NOT NULL, ts TIMESTAMP, "
    L"ownerWhere INT NOT NULL, firstSpeakergroup INT NOT NULL, identified BIT NOT NULL, plural BIT NOT NULL, male BIT NOT NULL, female BIT NOT NULL, neuter BIT NOT NULL, "
    L"common BIT NOT NULL, name BIT NOT NULL)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE objectLocations ("
    L"objectId INT UNSIGNED NOT NULL, INDEX oi_ind (objectId), FOREIGN KEY (objectId) REFERENCES objects(id), "
    L"sourceId INT UNSIGNED NOT NULL, INDEX s_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id), "
    L"at INT)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE objectWordMap ("
    L"objectId INT UNSIGNED NOT NULL, INDEX oi_ind (objectId), FOREIGN KEY (objectId) REFERENCES objects(id), "
    L"wordId INT UNSIGNED NOT NULL, INDEX w_ind (wordId), FOREIGN KEY (wordId) REFERENCES words(id), "
    L"orderOrType TINYINT NOT NULL)")) return -1;
  return 0;
}

int Source::createTimeRelationTables(void)
{
  // timeGroupMembers: timeRelationId foreign key to timeRelations, objectId, wordRelationId
  if (!myquery(&mysql,L"CREATE TABLE timeGroupMembers ("
    L"timeGroupId INT UNSIGNED NOT NULL, INDEX tgi_ind (timeGroupId), FOREIGN KEY (timeGroupId) REFERENCES timeGroups(id), "
    L"objectId INT UNSIGNED NOT NULL, INDEX oi_ind (objectId), FOREIGN KEY (objectId) REFERENCES objects(id), "
    L"wordRelationId INT UNSIGNED NOT NULL, INDEX r_ind (relationId), FOREIGN KEY (relationId) REFERENCES wordRelations(id))")) return -1;
  // timeGroups:
  if (!myquery(&mysql,L"CREATE TABLE timeGroups (id int(11) unsigned NOT NULL auto_increment unique, "
    L"speakerId INT UNSIGNED NOT NULL, INDEX si_ind (speakerId), FOREIGN KEY (speakerId) REFERENCES objects(id), "
    L"sourceId INT UNSIGNED NOT NULL, INDEX s_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id))")) return -1;
  // timeRelationTypes: ’BEFORE’|’AFTER’|’ON_OR_BEFORE’|’ON_OR_AFTER’|’LESS_THAN’|’MORE_THAN’|
  //                    ’EQUAL_OR_LESS’|’EQUAL_OR_MORE’|’START’|’MID’|’END’|’APPROX’
  if (!myquery(&mysql,L"CREATE TABLE timeRelationTypes (id int(11) unsigned NOT NULL auto_increment unique, "
    L"type VARCHAR(256) CHARACTER SET utf8 NOT NULL)")) return -1;
  // timeGroupRelations:
  if (!myquery(&mysql,L"CREATE TABLE timeGroupRelations (id int(11) unsigned NOT NULL auto_increment unique, "
    L"timeRelationTypeId INT UNSIGNED NOT NULL, INDEX tri_ind (timeRelationTypeId), FOREIGN KEY (timeRelationTypeId) REFERENCES timeRelationTypes(id), "
    L"timeGroupId INT UNSIGNED NOT NULL, INDEX tgi_ind (timeGroupId), FOREIGN KEY (timeGroupId) REFERENCES timeGroups(id), "
    L"timeGroup2Id INT UNSIGNED NOT NULL, INDEX tgi2_ind (timeGroup2Id), FOREIGN KEY (timeGroup2Id) REFERENCES timeGroups(id), "
    L")")) return -1;
  return 0;
}


// Larry loves Mary.
// Larry object:
//   RelationsWithObjects VerbWithSubjectWord
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is wordId to loves
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <tRelation> cr:
//         index is objectId to Mary
//         count++;
// Mary object:
//   RelationsWithObjects VerbWithDirectWord
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is wordId to loves
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <tRelation> cr:
//         index is objectId to Larry
//         count++;
// loves word:
//   verb=mainEntry love (of loves)
//   RelationsWithObjects SubjectWordWithVerb
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is objectId to Larry
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <tRelation> cr:
//         index is objectId to Mary
//         count++;
//   RelationsWithObjects DirectWordWithVerb
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is objectId to Mary
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <tRelation> cr:
//         index is objectId to Larry
//         count++;

int Source::createRelationTables(void)
{
  if (!myquery(&mysql,L"CREATE TABLE objectRelations (id int(11) unsigned NOT NULL auto_increment unique, "
    L"fromObjectId INT UNSIGNED NOT NULL, INDEX fo_ind (fromObjectId), FOREIGN KEY (fromObjectId) REFERENCES objects(id), "
    L"toObjectId INT UNSIGNED NOT NULL, INDEX to_ind (toObjectId), FOREIGN KEY (toObjectId) REFERENCES objects(id), "
    L"totalCount INT NOT NULL, "
    L"typeId INT UNSIGNED NOT NULL, INDEX t_ind(typeId), "
    L"ts TIMESTAMP)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE wordRelations (id int(11) unsigned NOT NULL auto_increment unique, "
		L"lastWhere int NOT NULL, "
    L"fromWordId INT UNSIGNED NOT NULL, INDEX fw_ind (fromWordId), FOREIGN KEY (fromWordId) REFERENCES words(id), "
    L"toWordId INT UNSIGNED NOT NULL, INDEX tw_ind (toWordId), FOREIGN KEY (toWordId) REFERENCES words(id), "
    L"typeId SMALLINT UNSIGNED NOT NULL, "
    //"ct BIT NOT NULL, " // used when retrieving words only in the current source
    L"UNIQUE INDEX uw_ind (fromWordId,toWordId,typeId), " // must be kept for ON DUPLICATE KEY UPDATE logic in flushWordRelations
    L"totalCount INT NOT NULL, "
    L"ts TIMESTAMP, INDEX ts_ind (ts))")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE objectRelationCombos (id int(11) unsigned NOT NULL auto_increment unique, "
    L"relationId INT UNSIGNED NOT NULL, INDEX r_ind (relationId), FOREIGN KEY (relationId) REFERENCES objectRelations(id), "
    L"relation2Id INT UNSIGNED NOT NULL, INDEX r2_ind (relation2Id), FOREIGN KEY (relation2Id) REFERENCES objectRelations(id), "
    L"totalCount INT NOT NULL, "
    L"typeId SMALLINT UNSIGNED NOT NULL, "
    L"ts TIMESTAMP)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE wordRelationCombos (id int(11) unsigned NOT NULL auto_increment unique, "
    L"relationId INT UNSIGNED NOT NULL, INDEX r_ind (relationId), FOREIGN KEY (relationId) REFERENCES wordRelations(id), "
    L"relation2Id INT UNSIGNED NOT NULL, INDEX r2_ind (relation2Id), FOREIGN KEY (relation2Id) REFERENCES wordRelations(id), "
    L"totalCount INT NOT NULL, "
    L"typeId SMALLINT UNSIGNED NOT NULL, "
    L"ts TIMESTAMP)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE relationsFlow (id int(11) unsigned NOT NULL auto_increment unique, "
    // subject or object (objectId)
    L"objectId INT UNSIGNED, INDEX o_ind (objectId), FOREIGN KEY (objectId) REFERENCES objects(id), "
    // relationIndex
    L"relationId INT UNSIGNED NOT NULL, INDEX r_ind (relationId), FOREIGN KEY (relationId) REFERENCES wordRelations(id), "
    // { T_SEQUENTIAL, T_ON, T_AFTER, T_PRESENT,T_VAGUE, T_RECURRING etc }
    L"timeRelationType SMALLINT UNSIGNED NOT NULL, "
    // day  (year > season > month > week > day (date) > afternoon > hour > minute (time) > second...
    L"capacity SMALLINT UNSIGNED NOT NULL, "
    // reference time
    L"timeExpressionId INT UNSIGNED NOT NULL, FOREIGN KEY (timeExpressionId) REFERENCES relationsFlow(id), "
    // other generic relations in the same expression
    L"relationsTributaryId INT UNSIGNED NOT NULL, FOREIGN KEY (relationsTributaryId) REFERENCES relationsTributary(id), "
    L"sequenceId INT UNSIGNED," // # - after timeExpression (increases by 10 to allow for insertions, or none if simultaneous
    L"sourceId INT UNSIGNED NOT NULL, FOREIGN KEY (timeExpressionId) REFERENCES relationsFlow(id), "
    L"ts TIMESTAMP)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE tributaryFlow (id int(11) unsigned NOT NULL auto_increment unique, "
    // relationIndex
    L"relationId INT UNSIGNED NOT NULL, FOREIGN KEY (relationId) REFERENCES wordRelations(id), "
    // relationTributary
    L"nextId INT UNSIGNED NOT NULL, FOREIGN KEY (nextId) REFERENCES relationsTributary(id), "
    L"ts TIMESTAMP)")) return -1;
  return 0;
}

int generateBNCSources(MYSQL &mysql,wchar_t *indexFile) // note this is slightly modified from the original
{
  int startTime=clock();
  wchar_t qt[query_buffer_len_overflow];
  wcscpy(qt,L"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
  size_t len=wcslen(qt);
  HANDLE hFile = CreateFile(indexFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    if (GetLastError()!=ERROR_PATH_NOT_FOUND)
      lplog(LOG_FATAL_ERROR,L"GetPath cannot open BNC sources path %s - %s",indexFile,getLastErrorMessage());
    return -17;
  }
  unsigned int actualLen=GetFileSize(hFile,NULL);
  if (actualLen<=0)
  {
    lplog(LOG_ERROR|LOG_INFO,L"ERROR:filelength of file %s yields an invalid filelength (%d).",indexFile,actualLen);
    CloseHandle(hFile);
    return -18;
  }
  wchar_t *buffer=(wchar_t *)tmalloc(actualLen+1);
  DWORD lenRead=0;
  if (!ReadFile(hFile,buffer,actualLen,&lenRead,NULL) || actualLen!=lenRead)
  {
    lplog(LOG_ERROR|LOG_INFO,L"ERROR:read error of file %s.",indexFile);
    CloseHandle(hFile);
    return -19;
  }
  buffer[actualLen]=0;
  CloseHandle(hFile);
  int where=0,returnCode=-20,numWords=0;
  while (true)
  {
    wchar_t *docStart=wcsstr(buffer+where,L"<doc>");
    if (!docStart)
    {
      returnCode=0;
      break;
    }
    returnCode=-20;
    wchar_t *docEnd=wcsstr(docStart,L"</doc>");
    if (!docEnd) break;
    *docEnd=0;
    wchar_t *id=wcsstr(docStart,L"<idno>");
    if (!id) break;
    id+=wcslen(L"<idno>");
    wchar_t *idEnd=wcsstr(id,L"</idno>");
    if (!idEnd) break;
    *idEnd=0;
    wchar_t *genre=wcsstr(idEnd+1,L"<genre>");
    if (!genre) break;
      where=(docEnd-buffer)+wcslen(L"</doc>");
    if (genre[wcslen(L"<genre>")]!='W')
      continue;
    len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, \"%s\", \"\", 0),",BNCType,id);
    if (!checkFull(&mysql,qt,len,false,NULL)) return -20;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -21;
	if (extraDetailedLogging)
		lplog(L"Inserting BNC sources took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  tfree(actualLen+1,buffer);
  return returnCode;
}

int generateNewsBankSources(MYSQL &mysql)
{
  int startTime=clock();
  wchar_t qt[query_buffer_len_overflow];
  wcscpy(qt,L"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
  size_t len=wcslen(qt);
  for (unsigned int I=2557; I<7057; I++)
  {
    time_t timer=I*24*3600;
    struct tm *day=gmtime(&timer);
    len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, \"newsbank\\%d\\%d.txt\", \"\", 0),",NewsBankType,day->tm_year+1900,timer/(24*3600));
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
	if (extraDetailedLogging)
	  lplog(L"Inserting NewsBank sources took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int generateTestSources(MYSQL &mysql)
{
  int startTime=clock();
  wchar_t qt[query_buffer_len_overflow];
  wcscpy(qt,L"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
  size_t len=wcslen(qt);
  wchar_t *testSources[]={L"agreement",L"date-time-number",L"lappinl",L"modification",L"Nameres",L"pattern matching",L"resolution",L"time",L"timeExpressions",L"usage",
                       L"verb object",NULL};
  for (unsigned int I=0; testSources[I]; I++)
  {
    len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, \"tests\\\\%s.txt\", \"~~BEGIN\", 1),",TestType,testSources[I]);
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
	if (extraDetailedLogging)
	  lplog(L"Inserting Test sources took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int Source::insertWordRelationTypes(void)
{
  wchar_t qt[query_buffer_len_overflow];
	wcscpy(qt,L"INSERT INTO wordRelationType VALUES ");
  size_t len=wcslen(qt);
  for (unsigned int I=firstRelationType; I<numRelationWOTypes; I++)
  {
    len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, \"%s\"),",I,getRelStr(I));
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  return 0;
}

void Source::updateSourceStatistics(int sourceId, int numSentences, int matchedSentences, int numWords, int numUnknown,
    int numUnmatched,int numOvermatched, int numQuotations, int quotationExceptions, int numTicks, int numPatternMatches)
{
  wchar_t qt[query_buffer_len_overflow];
  if (!myquery(&mysql,L"LOCK TABLES sources WRITE")) return;
  _snwprintf(qt,query_buffer_len,L"UPDATE sources SET numSentences=%d, matchedSentences=%d, numWords=%d, numUnknown=%d, numUnmatched=%d, "
    L"numOvermatched=%d, numQuotations=%d, quotationExceptions=%d, numTicks=%d, numPatternMatches=%d where id=%d",
    numSentences, matchedSentences,numWords,numUnknown,numUnmatched,numOvermatched,numQuotations,quotationExceptions,
    numTicks,numPatternMatches,sourceId);
  myquery(&mysql,qt);
}

int Source::createDatabase(wchar_t *server,bool forceDrop)
{
  if (!connectDatabase(server))
  {
    mysql_close(&mysql);
    return 0;
  }
  if (mysql_errno(&mysql)!=ER_BAD_DB_ERROR || mysql_real_connect(&mysql,wTM(server),"root","byron0",NULL,0,NULL,0)==NULL)
  {
    lplog(LOG_FATAL_ERROR,L"Failed to connect to MySQL - %S", mysql_error(&mysql));
    int err=mysql_errno(&mysql);
    return -1;
  }
  wchar_t qt[2*query_buffer_len_overflow];
  _snwprintf(qt,query_buffer_len,L"CREATE DATABASE %s character set utf8",DBNAME);
  if (!myquery(&mysql,qt)) return -1;
  _snwprintf(qt,query_buffer_len,L"USE %s",DBNAME);
  if (!myquery(&mysql,qt)) return -1;
  // create forms table
  if (!myquery(&mysql,L"CREATE TABLE forms (id int(11) unsigned NOT NULL auto_increment unique, name CHAR(32) CHARACTER SET utf8 NOT NULL,shortName CHAR(32) CHARACTER SET utf8 NOT NULL, inflectionsClass CHAR(12) CHARACTER SET utf8 NOT NULL, "
    L"hasInflections BIT NOT NULL, isTopLevel BIT NOT NULL, properNounSubClass BIT NOT NULL, blockProperNounRecognition BIT NOT NULL,ts TIMESTAMP)")) return -1;
  // create sources table
  if (!myquery(&mysql,L"CREATE TABLE sources "
		L"(id int(11) unsigned NOT NULL auto_increment unique, sourceType TINYINT NOT NULL, "
		L"etext VARCHAR (10) CHARACTER SET utf8,"
		L"path VARCHAR (1024) CHARACTER SET utf8 NOT NULL,"
		L"start VARCHAR(256) CHARACTER SET utf8 NOT NULL, repeatStart INT NOT NULL, author VARCHAR (32) CHARACTER SET utf8, title VARCHAR (128) CHARACTER SET utf8, date DATETIME,"
    L"numSentences INT, matchedSentences INT, numWords INT, numUnknown INT, numUnmatched INT, numOvermatched INT,"
    L"numQuotations INT, quotationExceptions INT, numTicks INT, numPatternMatches INT, "
    L"processing BIT, processed BIT, ts TIMESTAMP)"))
  {
    lplog(LOG_FATAL_ERROR,L"Failed to create sources - %S", mysql_error(&mysql));
    return -1;
  }
  int actualLen;
  getPath(L"source\\lists\\bookSources.sql",qt,2*query_buffer_len,actualLen);
	qt[actualLen]=0;
  wstring source=qt;
  //mysql_real_escape_string(&mysql, qt, source.c_str(), actualLen);
  if (!myquery(&mysql,qt))
  {
    lplog(LOG_FATAL_ERROR,L"Failed to populate sources with books - %S", mysql_error(&mysql));
    return -1;
  }
  generateBNCSources(mysql,L"\\BNC-world\\doc\\Source\\bncIndex.xml");
  generateNewsBankSources(mysql);
  generateTestSources(mysql);
  // create wordRelations types table (for convenience)
  if (!myquery(&mysql,L"CREATE TABLE wordRelationType (id int(11) unsigned NOT NULL unique, type CHAR(32) CHARACTER SET utf8 UNIQUE NOT NULL)"))
  insertWordRelationTypes();
  // create words table
  if (!myquery(&mysql,L"CREATE TABLE words (id int(11) unsigned NOT NULL auto_increment unique, "
    L"word CHAR(32) CHARACTER SET utf8 UNIQUE NOT NULL, inflectionFlags INT NOT NULL, flags INT NOT NULL, timeFlags INT NOT NULL," // INDEX word_ind (word) dropped (increased index space by more than 1000 times)
    L"mainEntryWordId INT UNSIGNED NULL, INDEX me_ind (mainEntryWordId), derivationRules INT, "
    L"sourceId INT UNSIGNED, INDEX s_ind (sourceId), " //FOREIGN KEY (sourceId) REFERENCES sources(id),
    L"ts TIMESTAMP)"))
  {
    lplog(LOG_FATAL_ERROR,L"Failed to create words table - %S", mysql_error(&mysql));
    return -1;
  }
  // create wordForms table
  if (!myquery(&mysql,L"CREATE TABLE wordForms (wordId INT UNSIGNED NOT NULL, formId INT UNSIGNED NOT NULL, count INT NOT NULL, ts TIMESTAMP, "
    L"INDEX wId_ind (wordId), " // FOREIGN KEY (wordId) REFERENCES words(id), "
    L"INDEX fId_ind (formId))"))
  {
    lplog(LOG_FATAL_ERROR,L"Failed to create wordForms - %S", mysql_error(&mysql));
    return -1;
  }
  if (createObjectTables()<0)
  {
    lplog(LOG_FATAL_ERROR,L"Failed to create object tables - %S", mysql_error(&mysql));
    return -1;
  }
  if (createRelationTables()<0)
  {
    lplog(LOG_FATAL_ERROR,L"Failed to create relation tables - %S", mysql_error(&mysql));
    return -1;
  }
  return 0;
}

int Source::readMultiSourceObjects(tIWMM *wordMap,int numWords)
{
  int startTime=clock();
  MYSQL_RES *result;
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
    m.push_back(WordMatch(wordMap[atoi(sqlrow[1])],0));
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
	if (extraDetailedLogging)
	  lplog(L"readMultiSourceObjects took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

// right now it is assumed that all objects from books are kept separate,
// so there is no check for merging information between info sources
// but there is a 'common' list consisting of all multiple-word objects, usually locations.
int Source::flushObjects(unsigned int sourceId)
{
  if (sourceId==-1) return 0;
  wprintf(L"Writing objects to database...\r");

  wchar_t qt[query_buffer_len_overflow];
  wchar_t wmqt[query_buffer_len_overflow];
  wchar_t olqt[query_buffer_len_overflow];

  int startTime=clock(),numObjectsInserted=0,numObjectLocationsInserted=0,where,lastProgressPercent=-1;
  _snwprintf(qt,query_buffer_len,L"delete from objectLocations where sourceId=%d",sourceId);
  if (!myquery(&mysql,qt)) return -1;
  _snwprintf(qt,query_buffer_len,L"delete from o using objectLocations as ol, objects as o where id=ol.objectId AND ol.sourceId=%d "
             L"AND o.id NOT in (SELECT objectId from objectLocations AS ol where ol.sourceId!=%d)",sourceId,sourceId);
  if (!myquery(&mysql,qt)) return -1;
  MYSQL_RES *result;
  if (!myquery(&mysql,L"SELECT MAX(id) from objects",result)) return -1;
  MYSQL_ROW sqlrow=mysql_fetch_row(result);
  int objectId=(sqlrow[0]==NULL) ? 1 : atoi(sqlrow[0]);
  mysql_free_result(result);

  wcscpy(qt,L"INSERT INTO objects VALUES ");
  size_t len=wcslen(qt);
  vector <cObject>::iterator o=objects.begin(),oEnd=objects.end();
  for (; o!=oEnd && !exitNow; o++)
  {
    if (!o->multiSource)
    {
      len+=_snwprintf(qt+len,query_buffer_len-len,L"(NULL,%d,%d,%d,%d,NULL,%d,%d,%d,%d,%d,%d,%d,%d,%d),",
        o->objectClass,o->numEncounters,o->numIdentifiedAsSpeaker,o->name.nickName,o->ownerWhere,o->firstSpeakerGroup,
        o->identified==true ? 1 : 0,o->plural==true ? 1 : 0,o->male==true ? 1 : 0,o->female==true ? 1 : 0,o->neuter==true ? 1 : 0,
				/*multiSource*/0,
        (o->objectClass==NAME_OBJECT_CLASS) ? 1 : 0);
      o->index=objectId++;
      numObjectsInserted++;
      if ((where=numObjectsInserted*100/objects.size())>lastProgressPercent)
      {
        wprintf(L"PROGRESS: %03d%% objects written to database with %04d seconds elapsed \r",where,clock()/CLOCKS_PER_SEC);
        lastProgressPercent=where;
      }
    }
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  lastProgressPercent=-1;
  int objectDetails=0;
  wcscpy(wmqt,L"INSERT INTO objectWordMap VALUES ");
  size_t wmlen=wcslen(wmqt);
  wcscpy(olqt,L"INSERT INTO objectLocations VALUES ");
  size_t ollen=wcslen(olqt);
  for (o=objects.begin(); o!=oEnd && !exitNow; o++,objectDetails++)
  {
    if ((where=objectDetails*100/objects.size())>lastProgressPercent)
    {
      wprintf(L"PROGRESS: %03d%% object details written to database with %04d seconds elapsed \r",where,clock()/CLOCKS_PER_SEC);
      lastProgressPercent=where;
    }
    if (!o->multiSource)
    {
      if (o->objectClass==NAME_OBJECT_CLASS)
        wmlen+=o->name.insertSQL(wmqt+wmlen,o->index,query_buffer_len-wmlen);
      else
      {
        for (int I=o->begin; I<o->end; I++)
          wmlen+=_snwprintf(wmqt+wmlen,query_buffer_len-wmlen,L"(%d,%d,%d),",o->index,m[I].word->second.index,I-o->begin);
      }
    }
    if (!checkFull(&mysql,wmqt,wmlen,false,NULL)) return -1;
    for (unsigned loc=0; loc<o->locations.size(); loc++)
    {
      ollen+=_snwprintf(olqt+ollen,query_buffer_len-ollen,L"(%d,%d,%d),",o->index,sourceId,o->locations[loc].at);
      if (!checkFull(&mysql,olqt,ollen,false,NULL)) return -1;
      numObjectLocationsInserted++;
    }
  }
  if (!checkFull(&mysql,olqt,ollen,true,NULL)) return -1;
  if (!checkFull(&mysql,wmqt,wmlen,true,NULL)) return -1;
  if (numObjectsInserted && extraDetailedLogging)
		lplog(L"Inserting %d objects from %d locations took %d seconds.",numObjectsInserted,numObjectLocationsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  wprintf(L"PROGRESS: 100%% objects written to database with %04d seconds elapsed         \n",clock()/CLOCKS_PER_SEC);
  return 0;
}

void WordClass::updateWordRelationIndexesFromDB(MYSQL &mysql,int lastReadfromDBTime)
{
  int startTime=clock(),wrUpdated=0;
  wchar_t nqt[256];
  // get ids of everything we just inserted
	_snwprintf(nqt,256,L"SELECT id, lastWhere, totalCount, fromWordId, toWordId, typeId from wordRelations where UNIX_TIMESTAMP(ts)>%d",lastReadfromDBTime);
  MYSQL_RES *result;
	if (!myquery(&mysql,nqt,result)) return;
  MYSQL_ROW sqlrow;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
		tIWMM iFromWord=idToMap[atoi(sqlrow[3])],iToWord=idToMap[atoi(sqlrow[4])];
    wrUpdated++;
    if (iFromWord==wNULL)
      lplog(LOG_FATAL_ERROR,L"Unknown 'from' word id %d encountered reading word relations",atoi(sqlrow[0]));
    if (iToWord==wNULL)
			lplog(LOG_FATAL_ERROR,L"Unknown 'to' word id %d encountered reading word relations from word %s",atoi(sqlrow[3]),iFromWord->first.c_str());
		int relationType=atoi(sqlrow[5]);
		iFromWord->second.allocateMap(relationType);
		iFromWord->second.relationMaps[relationType]->r[iToWord].index=atoi(sqlrow[0]);
		iFromWord->second.relationMaps[relationType]->r[iToWord].rlastWhere=atoi(sqlrow[1]);
		iFromWord->second.relationMaps[relationType]->r[iToWord].frequency=atoi(sqlrow[2]);
  }
  mysql_free_result(result);
  if (wrUpdated && extraDetailedLogging)
    lplog(L"Updating %d relations took %d seconds.",wrUpdated,(clock()-startTime)/CLOCKS_PER_SEC);
}

int WordClass::getNumWordRelationsToWrite(void)
{
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
{
  return lhs->second.deltaFrequency<rhs->second.deltaFrequency;
}

// update the database for all relations that have deltaFrequencies (changed since originally being read or
//   having this function run) AND that have relation indexes so that we know they exist in the DB
// this function is called by flushWordRelations
int WordClass::updateDBWordRelations(MYSQL &mysql,int totalWordRelationsToWrite)
{
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
      if ((clock()-startTime)>CLOCKS_PER_SEC && extraDetailedLogging)
        lplog(L"Updating %d relations (delta %d) took %d seconds.",relationsUpdated,currentDelta,(clock()-startTime)/CLOCKS_PER_SEC);
      startTime=clock();
			_snwprintf(nqt,query_buffer_len,L"UPDATE wordRelations2 SET totalCount=totalCount+%d WHERE id in (",(*uri)->second.deltaFrequency);
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
      wprintf(L"PROGRESS: %03d%% word relations flushed with %04d seconds elapsed \r",where,clock()/CLOCKS_PER_SEC);
      lastProgressPercent=where;
    }
    if (!checkFull(&mysql,nqt,nlen,false,NULL)) return -1;
  }
  if (relationsUpdated && !checkFull(&mysql,nqt,nlen,true,NULL)) return -1;
  if ((clock()-startTime)>CLOCKS_PER_SEC && extraDetailedLogging)
      lplog(L"Updating %d relations (delta %d) took %d seconds.",relationsUpdated,currentDelta,(clock()-startTime)/CLOCKS_PER_SEC);
	startTime=clock();
	relationsUpdated=0;
	lastProgressPercent=-1;
	for (vector<tFI::cRMap::tIcRMap>::iterator uri=ur.begin(),uriEnd=ur.end(); uri!=uriEnd && !exitNow; uri++)
	{
		//if ((*uri)->second.deltaFrequency>1) break; // only after the first time
		//if ((*uri)->second.frequency==1)
		wstring tmpstr,tmpstr2,query=L"UPDATE wordRelations2 SET lastWhere="+itos((*uri)->second.rlastWhere,tmpstr)+L" WHERE id="+itos((*uri)->second.index,tmpstr2); 
		if ((*uri)->second.rlastWhere>=0 && !myquery(&mysql,(wchar_t *)query.c_str())) continue;
		relationsUpdated++;
		if ((where=relationsUpdated*100/totalWordRelationsToWrite)>lastProgressPercent)
		{
			wprintf(L"PROGRESS: %03d%% word relations lastWhere updates with %04d seconds elapsed \r",where,clock()/CLOCKS_PER_SEC);
			lastProgressPercent=where;
		}
	}
  if (overallRelationsUpdated	&& extraDetailedLogging)
		lplog(L"Updating %d relations overall took %d seconds.",overallRelationsUpdated,(clock()-overallStartTime)/CLOCKS_PER_SEC);
  return overallRelationsUpdated;
}

// write all wordrelations to the DB.  After this function, all deltaFrequencies should be zero and
// all wordrelations in memory should have indexes.
int WordClass::flushWordRelations(MYSQL &mysql)
{
	tIWMM specials[]={Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION};
  for (unsigned int I=0; I<sizeof(specials)/sizeof(tIWMM); I++)
    specials[I]->second.changedSinceLastWordRelationFlush=true;
  if (!acquireLock(mysql,false)) return 0;
  int overallStartTime=clock();
	if (!myquery(&mysql,L"LOCK TABLES wordRelations WRITE, wordRelations2 WRITE, wordRelationCombos WRITE")) return -1;
  wprintf(L"Loading word relations index...\r");
  MYSQL_RES *result;
	if (!myquery(&mysql,L"LOAD INDEX INTO CACHE wordRelations",result)) 
	{
		myquery(&mysql,L"UNLOCK TABLES");
		return -1;
	}
  MYSQL_ROW sqlrow=mysql_fetch_row(result);
  if (sqlrow==NULL || sqlrow[3]==NULL || mTW(sqlrow[3])==L"OK")
    lplog(LOG_FATAL_ERROR,L"Loading wordRelations index failed -- MyISAM Engine not mandatory?");
  mysql_free_result(result);
	if (!myquery(&mysql,L"select UNIX_TIMESTAMP(NOW())",result)) 
	{
		myquery(&mysql,L"UNLOCK TABLES");
		return -1;
	}
  if ((sqlrow=mysql_fetch_row(result))==NULL || sqlrow[0]==NULL) lplog(LOG_FATAL_ERROR,L"wordRelations find time returned NULL!");
  int lastReadfromDBTime=atoi(sqlrow[0])+1;
  mysql_free_result(result);
  wprintf(L"Writing word relations to database...\r");
  int totalWordRelationsToWrite=getNumWordRelationsToWrite();
  // update all relations that already exist in the DB using update (faster)
  int overallRelationsInserted=updateDBWordRelations(mysql,totalWordRelationsToWrite);
  // update all relations that probably don't exist in the DB by inserting them (although still use DUPLICATE KEY UPDATE if another instance
  // has updated wordRelations in the meantime)
  int minNextDelta=1;
  while (minNextDelta<1000000 && !exitNow)
  {
    int startTime=clock(),relationsInserted=0,where,lastProgressPercent=-1;
    wchar_t nqt[query_buffer_len_overflow];
		wcscpy(nqt,L"INSERT into wordRelations2 (fromWordId, toWordId, lastWhere, totalCount, typeId) VALUES ");
    wchar_t dupqt[128];
    _snwprintf(dupqt,128,L" ON DUPLICATE KEY UPDATE totalCount=totalCount+%d, lastWhere=VALUES(lastWhere)",minNextDelta);
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
                if (r->first->second.index<0)
										//lplog(L"ERROR! word %s in relations type %s to word %s has an index of -1!",w->first.c_str(),getRelStr(rType),r->first->first.c_str());
										lplog(LOG_ERROR,L"flushWordRelations:word %s has an index of -1!",r->first->first.c_str());
                else
                {
									/*
									wstring tmpstr,tmpstr2,tmpstr3,tmpstr4,query=L"UPDATE wordRelations2 SET lastWhere="+itos(r->second.rlastWhere,tmpstr)+
										L" WHERE fromWordId="+itos(w->second.index,tmpstr2)+
										L" AND toWordId="+itos(r->first->second.index,tmpstr3)+
										L" AND typeId="+itos(rType,tmpstr4); 
									if (!myquery(&mysql,(wchar_t *)query.c_str())) 
									{
										myquery(&mysql,L"UNLOCK TABLES");
										return -1;
									}
									*/
									nlen+=_snwprintf(nqt+nlen,query_buffer_len-nlen,L"(%d,%d,%d,%d,%d),",w->second.index,r->first->second.index,r->second.rlastWhere, r->second.frequency,rType);
                  r->second.deltaFrequency=0;
                  relationsInserted++;
                  overallRelationsInserted++;
                  if ((where=overallRelationsInserted*100/totalWordRelationsToWrite)>lastProgressPercent)
                  {
                    wprintf(L"PROGRESS: %03d%% word relations flushed with %04d seconds elapsed \r",where,clock()/CLOCKS_PER_SEC);
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
							{
								myquery(&mysql,L"UNLOCK TABLES");
								return -1;
							}
            }
        if (!stillFlush)
          w->second.changedSinceLastWordRelationFlush=false;
      }
		if (!checkFull(&mysql,nqt,nlen,true,dupqt)) 
		{
			myquery(&mysql,L"UNLOCK TABLES");
			return -1;
		}
    if ((clock()-startTime)>CLOCKS_PER_SEC && extraDetailedLogging) // relationsInserted &&
      lplog(L"Inserting %d relations (delta %d) took %d seconds.",relationsInserted,currentDelta,(clock()-startTime)/CLOCKS_PER_SEC);
  }
  for (tIWMM w=begin(),wEnd=end(); w!=wEnd && !exitNow; w++)
    w->second.changedSinceLastWordRelationFlush=false;
  if (overallRelationsInserted && extraDetailedLogging)
    lplog(L"Inserting %d relations overall took %d seconds.",overallRelationsInserted,(clock()-overallStartTime)/CLOCKS_PER_SEC);
	updateWordRelationIndexesFromDB(mysql,lastReadfromDBTime);
  flushRelationCombos(mysql);
  myquery(&mysql,L"UNLOCK TABLES");
  releaseLock(mysql);
  wprintf(L"PROGRESS: 100%% word relations flushed with %04d seconds elapsed \n",clock()/CLOCKS_PER_SEC);
  return 0;
}

// read word DB indexes for those words not already having indexes and belonging to the current source.
// return a set of wordIds for those words belonging to the current source and not previously had wordRelations refreshed.
int Source::readWordIndexesForWordRelationsRefresh(vector <int> &wordIds)
{
	for (tIWMM w=Words.begin(),wEnd=Words.end(); w!=wEnd; w++)
	{
		w->second.flags&=~tFI::wordRelationsRefreshed;
		w->second.clearRelationMaps();
	}
  wchar_t qt[query_buffer_len_overflow];
  int len=_snwprintf(qt,query_buffer_len,L"select id,word from words where word in ("),wordId,originalLen=len;
	tIWMM specials[]={Words.PPN,Words.TELENUM,Words.NUM,Words.DATE,Words.TIME,Words.LOCATION};
	for (unsigned int I=0; I<sizeof(specials)/sizeof(tIWMM); I++)
	{
		len+=_snwprintf(qt+len,query_buffer_len-len,L"\"%s\",",specials[I]->first.c_str());
		specials[I]->second.flags|=tFI::wordRelationsRefreshed;
		specials[I]->second.clearRelationMaps();
	}
  Words.sectionWord->second.flags|=tFI::wordRelationsRefreshed;
  tIWMM tWord;
  vector <WordMatch>::iterator im=m.begin(),imEnd=m.end();
  MYSQL_RES *result;
  MYSQL_ROW sqlrow;
  for (int I=0; im!=imEnd; im++,I++)
  {
    tIWMM ME=resolveToClass(I);
		wstring s=ME->first;
		int flags= ME->second.flags;
    ME->second.flags|=tFI::inSourceFlag;
    if (ME->first.length()!=1 && !(ME->second.flags&tFI::wordRelationsRefreshed))
    {
      ME->second.flags|=tFI::wordRelationsRefreshed|tFI::newWordFlag;
      if (ME->second.index>=0)
        wordIds.push_back(ME->second.index);
      else
      {
        len+=_snwprintf(qt+len,query_buffer_len-len,L"\"%s\",",ME->first.c_str());
        if (len>query_buffer_len)
        {
          qt[len-1]=')';
          if (!myquery(&mysql,qt,result)) return -1;
          while ((sqlrow=mysql_fetch_row(result))!=NULL)
          {
            wordIds.push_back(wordId=atoi(sqlrow[0]));
            if ((tWord=Words.query(mTW(sqlrow[1])))!=Words.end())
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
      if ((tWord=Words.query(mTW(sqlrow[1])))!=Words.end())
        Words.assign(wordId,tWord);
    }
    mysql_free_result(result);
  }
  return 0;
}

// this is called immediately after reading in words from text (tokenization), before parsing and costing.
// read word relations for only the words in the current source that have not been read before.
int Source::refreshWordRelations(void)
{
  if (preTaggedSource) return 0; // we use BNC to construct word relations in the first place
  // find index ids for all words not having been refreshed previously and belonging ONLY to the current source
  wprintf(L"Reading word indexes...                       \r");
  vector <int> wordIds;
  if (!myquery(&mysql,L"LOCK TABLES words READ")) return -1;
  readWordIndexesForWordRelationsRefresh(wordIds);
	myquery(&mysql,L"UNLOCK TABLES");
  int startTime=clock();
  size_t I=0,wrRead=0,wrAdded=0,totalWRIDs=wordIds.size();
  __int64 lastProgressPercent=-1,where;
  wprintf(L"Refreshing word relations for %d words...                       \r",wordIds.size());
  if (!myquery(&mysql,L"LOCK TABLES wordRelations READ")) return -1;
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
  I=0;
  while (I<totalWRIDs)
  {
    int len=0;
    __int64 wrSubRead=0;
    for (; I<totalWRIDs && len<query_buffer_len; I++)
      len+=_snwprintf(qt+len,query_buffer_len-len,L"%d,",wordIds[I]);
    if (!len) break;
    qt[len-1]=0; //erase last ,
    //wstring sqt="select fromWordId, toWordId, totalCount, typeId from wordRelations where fromWordId in (" + wstring(qt) + ") and ct=true";
		wstring sqt=L"select id, lastWhere, fromWordId, toWordId, totalCount, typeId from wordRelations where fromWordId in (" + wstring(qt) + L") AND typeId in (0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36)";
    //if (I==totalWRIDs) // actually makes query take 50% longer
    //  sqt+=" AND toWordId in (" + wstring(qt) + ")";
    MYSQL_RES *result;
		if (!myquery(&mysql,(wchar_t *)sqt.c_str(),result)) 
		{
			myquery(&mysql,L"UNLOCK TABLES");
			return -1;
		}
    __int64 numRows=mysql_num_rows(result);
    MYSQL_ROW sqlrow;
    while ((sqlrow=mysql_fetch_row(result))!=NULL)
    {
      bool isNew;
			tIWMM iFromWord=Words.idToMap[atoi(sqlrow[2])],iToWord=Words.idToMap[atoi(sqlrow[3])];
      wrRead++;
      if (iFromWord!=wNULL && iToWord!=wNULL && (iToWord->second.flags&tFI::inSourceFlag))
      {
				int relation=atoi(sqlrow[5]);
				int lastWhere=atoi(sqlrow[1]);
				int index=atoi(sqlrow[0]);
        iFromWord->second.allocateMap(relation);
				iFromWord->second.relationMaps[relation]->addRelation(index,lastWhere,iToWord,isNew,atoi(sqlrow[4]),true);
        relation=getComplementaryRelationship((relationWOTypes)relation);
        iToWord->second.allocateMap(relation);
				iToWord->second.relationMaps[relation]->addRelation(index,lastWhere,iFromWord,isNew,atoi(sqlrow[4]),true);
        wrAdded+=2;
      }
      if ((where=wrSubRead++*100*I/(numRows*totalWRIDs))>lastProgressPercent)
      {
        wprintf(L"PROGRESS: %03I64d%% word relations read with %04d seconds elapsed                    \r",where,clock()/CLOCKS_PER_SEC);
        lastProgressPercent=where;
      }
    }
    mysql_free_result(result);
  }
  for (vector <WordMatch>::iterator im=m.begin(),imEnd=m.end(); im!=imEnd; im++)
		im->getMainEntry()->second.flags&=~tFI::inSourceFlag;
  if ((wrRead || wrAdded) && extraDetailedLogging)
    lplog(L"Reading %d (%d added) word relations took %d seconds.",wrRead,wrAdded,(clock()-startTime)/CLOCKS_PER_SEC);
	wprintf(L"PROGRESS: 100%% word relations read with %04d seconds elapsed                    \n",clock()/CLOCKS_PER_SEC);
	myquery(&mysql,L"UNLOCK TABLES");
  return 0;
}

int WordClass::flushRelationCombos(MYSQL &mysql)
{
  int startTime=clock(),relationCombosNum=0,where,lastProgressPercent=-1;
  wchar_t nqt[query_buffer_len_overflow];
  wcscpy(nqt,L"INSERT into wordRelationCombos (relationId, relation2Id, typeId) VALUES ");
  wchar_t dupqt[128];
  _snwprintf(dupqt,128,L" ON DUPLICATE KEY UPDATE totalCount=totalCount+1");
  size_t nlen=wcslen(nqt);
  for (vector<relc>::iterator rc=relationCombos.begin(),rcEnd=relationCombos.end(); rc!=rcEnd && !exitNow; rc++,relationCombosNum++)
    if (rc->rel1->second.index>=0 && rc->rel2->second.index>=0)
    {
      nlen+=_snwprintf(nqt+nlen,query_buffer_len-nlen,L"(%d,%d,%d),",rc->rel1->second.index>=0,rc->rel2->second.index,rc->rcType);
      if ((where=relationCombosNum*100/relationCombos.size())>lastProgressPercent)
      {
        wprintf(L"PROGRESS: %03d%% combo word relations flushed with %04d seconds elapsed \r",where,clock()/CLOCKS_PER_SEC);
        lastProgressPercent=where;
      }
      if (!checkFull(&mysql,nqt,nlen,false,dupqt)) return -1;
    }
  if (!checkFull(&mysql,nqt,nlen,true,dupqt)) return -1;
  if (relationCombosNum && extraDetailedLogging)
    lplog(L"Inserting %d combo relations took %d seconds.",relationCombosNum,(clock()-startTime)/CLOCKS_PER_SEC);
  relationCombos.clear();
  wprintf(L"PROGRESS: 100%% combo word relations flushed with %04d seconds elapsed \n",clock()/CLOCKS_PER_SEC);
  return 0;
}

int Source::flushObjectRelations(int sourceId)
{
  return 0;
}

int Source::createGroupTables(void)
{
  if (!myquery(&mysql,L"CREATE TABLE groups (groupId int(11) unsigned NOT NULL, "
    L"typeId SMALLINT UNSIGNED NOT NULL, INDEX t_ind(typeId), "
    L"fromWordId INT UNSIGNED NOT NULL, INDEX fw_ind (fromWordId), FOREIGN KEY (fromWordId) REFERENCES words(id), "
    L"ts TIMESTAMP)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE groupMapTo (groupId INT UNSIGNED NOT NULL, INDEX g_ind (groupId), FOREIGN KEY (groupId) REFERENCES groups(groupId), "
    L"toWordId INT UNSIGNED NOT NULL, INDEX tw_ind (toWordId), FOREIGN KEY (toWordId) REFERENCES words(id), "
    L"ts TIMESTAMP)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE subGroups (groupId INT UNSIGNED NOT NULL, INDEX g_ind (groupId), FOREIGN KEY (groupId) REFERENCES groups(groupId), "
    L"subgroupId INT UNSIGNED NOT NULL, INDEX sg_ind (subgroupId), FOREIGN KEY (subgroupId) REFERENCES groups(id), "
    L"UNIQUE INDEX gsg_ind (groupId,subgroupId))")) return -1;
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
int WordClass::flushGroups(int sourceId)
{
  return 0;
}

int WordClass::flushForms(MYSQL &mysql)
{
  MYSQL_RES *result;
  if (!myquery(&mysql,L"LOCK TABLES forms WRITE")) return -1;
  if (!myquery(&mysql,L"SELECT MAX(id) from forms",result)) return -1;
  MYSQL_ROW sqlrow=mysql_fetch_row(result);
  int numFormsInDB=(sqlrow[0]==NULL) ? 0 : atoi(sqlrow[0]);
  mysql_free_result(result);
  if (numFormsInDB==Forms.size()) return 0;
  if (!myquery(&mysql,L"LOCK TABLES forms WRITE")) return -1;
  if (!myquery(&mysql,L"ALTER TABLE forms DISABLE KEYS")) return -1;
  if (!myquery(&mysql,L"LOCK TABLES forms WRITE")) return -1;
  set<wstring>::iterator fi;
  wchar_t qt[query_buffer_len_overflow];
  int startTime=clock();
  wcscpy(qt,L"INSERT INTO forms VALUES ");
  size_t len=wcslen(qt);
  bool newFormsFound=false;
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
	if (extraDetailedLogging)
	  lplog(L"Inserting forms took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  if (!myquery(&mysql,L"ALTER TABLE forms ENABLE KEYS")) return -1;
  return 0;
}

/*
  max_allowed_packet
  The maximum packet length to send to or receive from the server. (Default value is 16MB.)
*/

int WordClass::flushNewMainEntryWords(MYSQL &mysql,int &nextIDInDB)
{
  if (!myquery(&mysql,L"LOCK TABLES words WRITE")) return -1;
  if (!myquery(&mysql,L"ALTER TABLE words DISABLE KEYS")) return -1;
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
    wchar_t tmp[1024];
    encodeEscape(mysql, tmp, iWord->first);
    if (mainEntry==wNULL || mainEntry->second.index>=0 || iWord==mainEntry) continue;
    wchar_t word[1024],source[10];
    encodeEscape(mysql, word, mainEntry->first);
    mainEntry->second.flags&=~(tFI::updateMainInfo|tFI::intersectionGroup);
    if (iWord->first.length()>MAX_WORD_LENGTH)
    {
      lplog(LOG_ERROR|LOG_INFO,L"Word %s is too long.... Rejected (1).",iWord->first.c_str());
      continue;
    }
    if (iWord->first[iWord->first.length()-1]==' ')
    {
			lplog(LOG_ERROR,L"Word %s has a space at the end... Rejected (1).",iWord->first.c_str());
      continue;
    }
    mainEntry->second.flags|=tFI::insertNewForms;
    len+=_snwprintf(qt+len,query_buffer_len-len,L"(NULL,\"%s\",%d,%d,%d,%d,%d,%s,NULL),",
      word,mainEntry->second.inflectionFlags,mainEntry->second.flags&~(tFI::resetFlagsOnRead),mainEntry->second.timeFlags,nextIDInDB,mainEntry->second.derivationRules,
      (mainEntry->second.sourceId==-1) ? L"NULL" : _itow(mainEntry->second.sourceId,source,10));
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
    assign(nextIDInDB++,mainEntry);
    numWordsInserted++;
    #ifdef LOG_WORD_FLOW
      lplog(L"Inserted mainEntry word %s.",word);
    #endif
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  if (numWordsInserted && extraDetailedLogging)
		lplog(L"Inserting %d mainEntry words took %d seconds.",numWordsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int WordClass::flushNewWords(MYSQL &mysql,int &nextIDInDB)
{
  int startTime=clock(),numWordsInserted=0;
  wchar_t qt[query_buffer_len_overflow],source[10];
  wcscpy(qt,L"INSERT IGNORE INTO words VALUES "); // IGNORE is necessary because C++ treats unicode strings as different, but MySQL treats them as the same
  size_t len=wcslen(qt);
  for (tIWMM iWord=begin(),iWordEnd=end(); iWord!=iWordEnd; iWord++)
  {
    if (iWord->second.index>=0) continue;
    wchar_t word[1024];
		wstring save=iWord->first;
    encodeEscape(mysql, word, iWord->first);
    iWord->second.flags&=~(tFI::updateMainInfo|tFI::intersectionGroup);
    if (iWord->first.length()>MAX_WORD_LENGTH)
    {
      lplog(LOG_ERROR|LOG_INFO,L"Word %s is too long.... Rejected (2).",iWord->first.c_str());
      continue;
    }
    if (iWord->first[iWord->first.length()-1]==' ')
    {
			lplog(LOG_ERROR,L"Word %s has a space at the end... Rejected (2) [original=%s].",iWord->first.c_str(),save.c_str());
      continue;
    }
    if (iWord->first.find(L'\\')!=wstring::npos)
    {
			lplog(LOG_ERROR,L"Word %s is a slash... Rejected (3) [original=%s].",iWord->first.c_str(),save.c_str());
      continue;
    }
    iWord->second.flags|=tFI::insertNewForms;
    assign(nextIDInDB++,iWord);
    if (iWord->second.mainEntry==wNULL)
      len+=_snwprintf(qt+len,query_buffer_len-len,L"(NULL,\"%s\",%d,%d,%d,NULL,%d,%s,NULL),",
        word,iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.derivationRules,
        (iWord->second.sourceId==-1) ? L"NULL" : _itow(iWord->second.sourceId,source,10));
    else
      len+=_snwprintf(qt+len,query_buffer_len-len,L"(NULL,\"%s\",%d,%d,%d,%d,%d,%s,NULL),",
        word,iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.mainEntry->second.index,iWord->second.derivationRules,
        (iWord->second.sourceId==-1) ? L"NULL" : _itow(iWord->second.sourceId,source,10));
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
    #ifdef LOG_WORD_FLOW
      lplog(L"Inserted non-mainEntry word %s.",word);
    #endif
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  if (numWordsInserted && extraDetailedLogging)
		lplog(L"Inserting %d non-mainEntry words took %d seconds.",numWordsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int WordClass::updateWords(MYSQL &mysql,int &numUpdates,int sourceId)
{
  int startTime=clock();
  tIWMM iWord,iWordEnd=end();
  wchar_t qt[query_buffer_len_overflow];
  int len=0;
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
    wchar_t word[1024];
    encodeEscape(mysql, word, iWord->first);
    int len;
    if (iWord->second.mainEntry==wNULL)
      len=_snwprintf(qt,query_buffer_len,L"UPDATE words SET word=\"%s\",inflectionFlags=%d,flags=%d,timeFlags=%d,derivationRules=%d,sourceId=NULL WHERE id=%d LIMIT 1",
          word,iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.derivationRules,iWord->second.index);
    else
      len=_snwprintf(qt,query_buffer_len,L"UPDATE words SET word=\"%s\",inflectionFlags=%d,flags=%d,timeFlags=%d,mainEntryWordId=%d,derivationRules=%d,sourceId=NULL WHERE id=%d LIMIT 1",
          word,iWord->second.inflectionFlags,iWord->second.flags&~(tFI::resetFlagsOnRead),iWord->second.timeFlags,iWord->second.mainEntry->second.index,iWord->second.derivationRules,iWord->second.index);
    if (!myquery(&mysql,qt)) continue;
    #ifdef LOG_WORD_FLOW
      lplog(L"Updated word %s.",word);
    #endif
  }
  if (numUpdates && extraDetailedLogging)
    lplog(L"Updating non-form info for %d existing words took %d seconds.",numUpdates,(clock()-startTime)/CLOCKS_PER_SEC);
  if (!myquery(&mysql,L"ALTER TABLE words ENABLE KEYS")) return -1;
  return 0;
}

int WordClass::updateWordForms(MYSQL &mysql,bool allNew)
{
  if (!myquery(&mysql,L"LOCK TABLES wordForms WRITE")) return -1;
  if (!myquery(&mysql,L"ALTER TABLE wordForms DISABLE KEYS")) return -1;
  if (!myquery(&mysql,L"LOCK TABLES wordForms WRITE")) return -1;
  int startTime=clock();
  tIWMM iWord,iWordEnd=end();
  wchar_t qt[query_buffer_len_overflow];
  size_t len=_snwprintf(qt,query_buffer_len,L"DELETE FROM wordForms WHERE wordId in (");
  for (iWord=begin(); iWord!=iWordEnd && len<query_buffer_len; iWord++)
    if (iWord->second.flags&tFI::updateMainInfo)
      len+=_snwprintf(qt+len,query_buffer_len-len,L"%d,",iWord->second.index);
  if (qt[len-1]==',')
  {
    wcscpy(qt+len-1,L")");
    if (!myquery(&mysql,qt)) return -1;
  }
  // insert new form data
  wcscpy(qt,L"INSERT INTO wordForms VALUES ");
  len=wcslen(qt);
  size_t maxForms=Forms.size(),wordFormsInserted=0;
  // load wordForms table for new words and words for which the forms have changed
  for (iWord=begin(); iWord!=iWordEnd; iWord++)
  {
    int flags=iWord->second.flags;
    if (!allNew && !(iWord->second.flags&tFI::updateMainInfo) && !(iWord->second.flags&tFI::insertNewForms)) continue;
    #ifdef LOG_WORD_FLOW
      lplog(L"%s word %s (index=%d) - %d forms.",(iWord->second.flags&tFI::updateMainInfo) ? "Updating" : "Creating",iWord->first.c_str(),iWord->second.index,iWord->second.formsSize());
    #endif
    iWord->second.flags&=~(tFI::updateMainInfo|tFI::insertNewForms);
    for (unsigned int f=0; f<iWord->second.formsSize(); f++,wordFormsInserted++)
    {
      if (iWord->second.index<=0 || iWord->second.Form(f)->index<=0 || iWord->second.Form(f)->index>(signed)maxForms)
        lplog(LOG_ERROR,L"Illegal index for word %s has index %d with form #%d having index %d (form offset %d)!",
           iWord->first.c_str(),iWord->second.index,f,iWord->second.Form(f)->index,iWord->second.forms()[f]);
      else
        len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, %d, %d, NULL),",iWord->second.index, iWord->second.Form(f)->index,iWord->second.usagePatterns[f]);
      if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
    }
    for (unsigned int I=tFI::MAX_FORM_USAGE_PATTERNS; I<tFI::LAST_USAGE_PATTERN; I++)
      if (iWord->second.usagePatterns[I])
      {
        len+=_snwprintf(qt+len,query_buffer_len-len,L"(%d, %d, %d, NULL),",iWord->second.index, I+tFI::patternFormNumOffset-tFI::MAX_FORM_USAGE_PATTERNS,iWord->second.usagePatterns[I]);
        iWord->second.deltaUsagePatterns[I]=0;
        wordFormsInserted++;
      }
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  if (wordFormsInserted && extraDetailedLogging)
    lplog(L"Inserting %d wordforms took %d seconds.",wordFormsInserted,(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

void tFI::lplog(void)
{
  ::lplog(L"%d %d %d %s",index,flags,inflectionFlags,(mainEntry==wNULL) ? L"" : mainEntry->first.c_str());
}

void WordClass::updateUsages(MYSQL &mysql)
{
  int startTime=clock();
  int usages=0;
  tIWMM iWord,iWordEnd=end();
  wchar_t qt[query_buffer_len_overflow];
  for (iWord=begin(); iWord!=iWordEnd && !exitNow; iWord++)
  {
    if (iWord->second.index<=0)
    {
      lplog(LOG_ERROR,L"Index for word %s has index %d - rejected usage",iWord->first.c_str(),iWord->second.index);
      continue;
    }
    for (unsigned int f=0; f<iWord->second.formsSize(); f++)
    {
      if (!iWord->second.deltaUsagePatterns[f]) continue;
      if (iWord->second.Form(f)->index<=0 || iWord->second.Form(f)->index>(int)Forms.size())
        lplog(LOG_FATAL_ERROR,L"Illegal index for word %s has index %d with form #%d having index %d (form offset %d)!",
          iWord->first.c_str(),iWord->second.index,f,iWord->second.Form(f)->index,iWord->second.forms()[f]);
      else
      {
        _snwprintf(qt,query_buffer_len,L"UPDATE wordForms SET count=count+%d WHERE wordId=%d AND formId=%d LIMIT 1",
          iWord->second.deltaUsagePatterns[f],iWord->second.index,iWord->second.Form(f)->index);
        iWord->second.deltaUsagePatterns[f]=0;
        usages++;
        if (!myquery(&mysql,qt)) return;
      }
    }
    for (unsigned int f=tFI::MAX_FORM_USAGE_PATTERNS; f<tFI::LAST_USAGE_PATTERN; f++)
      if (iWord->second.deltaUsagePatterns[f])
      {
        _snwprintf(qt,query_buffer_len,L"UPDATE wordForms SET count=count+%d WHERE wordId=%d AND formId=%d LIMIT 1",
          iWord->second.deltaUsagePatterns[f],iWord->second.index,f+tFI::patternFormNumOffset-tFI::MAX_FORM_USAGE_PATTERNS);
        if (!myquery(&mysql,qt)) return;
        if (mysql_affected_rows(&mysql)!=1)
        {
          _snwprintf(qt,query_buffer_len,L"INSERT into wordForms VALUES (%d, %d, %d, NULL)",iWord->second.index, f+tFI::patternFormNumOffset-tFI::MAX_FORM_USAGE_PATTERNS,iWord->second.usagePatterns[f]);
          if (!myquery(&mysql,qt)) return;
        }
        iWord->second.deltaUsagePatterns[f]=0;
        usages++;
      }
  }
  if (usages && extraDetailedLogging)
    lplog(L"Updating %d usages in wordforms took %d seconds.",usages,(clock()-startTime)/CLOCKS_PER_SEC);
}

wchar_t *getTimeStamp(void)
{
   static wchar_t ts[32];
   struct tm *newtime;
   time_t aclock;
   time( &aclock );   // Get time in seconds
   newtime = localtime( &aclock );   // Convert time to struct tm form
   /* Print local time as a wstring */
   wcscpy(ts,_wasctime( newtime ));
   ts[wcslen(ts)-1]=0;
   return ts;
}

/*
Tries to obtain a lock with a name given by the wstring str, using a timeout of timeout seconds.
Returns 1 if the lock was obtained successfully,
        0 if the attempt timed out (for example, because another client has previously locked the name), or
        NULL if an error occurred (such as running out of memory or the thread was killed with mysqladmin kill).
If you have a lock obtained with GET_LOCK(), it is released when you execute RELEASE_LOCK(), execute a new GET_LOCK(), or your connection terminates (either normally or abnormally).
*/
bool WordClass::acquireLock(MYSQL &mysql,bool persistent)
{
  #ifdef WRITE_WORDS_AND_FORMS_TO_DB
  int startTime=clock();
  wprintf(L"Acquiring lock on database...\r");
  while (true)
  {
    MYSQL_RES *result;
    if (!myquery(&mysql,L"SELECT GET_LOCK('lp_lock',20)",result)) return false;
    MYSQL_ROW sqlrow=mysql_fetch_row(result);
    if (sqlrow==NULL)
      lplog(LOG_FATAL_ERROR,L"Error acquiring lock.");
    int lockAcquired=atoi(sqlrow[0]);
    mysql_free_result(result);
    if (lockAcquired==1)
    {
      if ((clock()-startTime)>CLOCKS_PER_SEC && extraDetailedLogging)
				lplog(L"Acquiring global database lock took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
      return true;
    }
    else if (!persistent)
    {
      lplog(L"Skipping global database lock (%d seconds).",(clock()-startTime)/CLOCKS_PER_SEC);
      return false;
    }
    wprintf(L"Acquiring lock on database (%05d seconds)...\r",(clock()-startTime)/CLOCKS_PER_SEC);
  }
  #endif
  return true;
}

/*
Releases the lock named by the wstring str that was obtained with GET_LOCK().
Returns 1 if the lock was released, 0 if the lock was not established by this thread (in which case the lock is not released), and NULL if the named lock did not exist.
The lock does not exist if it was never obtained by a call to GET_LOCK() or if it has previously been released.
*/
void WordClass::releaseLock(MYSQL &mysql)
{
  MYSQL_RES *result;
  if (!myquery(&mysql,L"SELECT RELEASE_LOCK('lp_lock')",result)) return;
  MYSQL_ROW sqlrow=mysql_fetch_row(result);
  if (sqlrow==NULL)
    lplog(LOG_FATAL_ERROR,L"Lock does not exist.");
  if (sqlrow[0]==NULL || atoi(sqlrow[0])==0)
    lplog(LOG_ERROR,L"Lock was never acquired.");
  mysql_free_result(result);
}

// steps:
//  get the maximum AUTO_INCREMENT index for a word to use for mainEntry index
//  insert all mainEntries that don't have an auto_increment index assigned to it.
//  insert all new words into words table
//  update inflectionFlags, flags, mainEntry id and derivationRules for all changed words
//  delete all wordForms for all changed words
//  insert all wordForms for all changed words
//  update usage counts for all word forms seen in current source
int WordClass::writeWordsToDB(MYSQL &mysql,wchar_t *sourcePath,int sourceId)
{
  int startTime=clock();
  wprintf(L"Updating from database...\r");
  //lplog(L"TIME DB WRITE BEGIN for source %d:%s",sourceId,getTimeStamp());
  if (sourceId>=0)
  {
    #ifdef WRITE_WORD_CACHE
      writeWords(sourcePath);
    #endif
    tIWMM startUpdateScan=Words.begin();
    int numWordsInserted=0,numWordsModified=0;
    read(mysql,-1,NULL,false,wNULL,numWordsInserted,numWordsModified); // update in memory all words having sourceId==-1 (we assume that no changes to the current sourceId have occurred).
    while (startUpdateScan!=wNULL) read(mysql,-2,NULL,false,startUpdateScan,numWordsInserted,numWordsModified); // read sourceId ==-2 just updates to memory the new words discovered in the most recent source.
    startUpdateScan=Words.begin();
    while (startUpdateScan!=wNULL) read(mysql,-3,NULL,false,startUpdateScan,numWordsInserted,numWordsModified); // read sourceId ==-3 just updates to memory the new words discovered in the most recent source.
    if (numWordsInserted || numWordsModified)
    lplog(L"%d words inserted into and %d words modified in memory in %d seconds.",numWordsInserted,numWordsModified,(clock()-startTime)/CLOCKS_PER_SEC);
    // it is assumed that words that are updated in memory do not need to be updated from the database because
    // updated words are only updated once (after discovery) and thus the database would have nothing to add.
    // In this case the words will be updated into the database and should be identical to the values already there.
  }
  #ifndef WRITE_WORDS_AND_FORMS_TO_DB
    myquery(&mysql,L"UNLOCK TABLES");
    wprintf(L"Finished updating from database.      \r");
    lplog(L"WordClass::write took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
    return 0;
  #endif
  wprintf(L"Writing to database...            \r");
  if (flushForms(mysql)<0)
    return -2;
  int nextIDInDB=1;
  if (sourceId>=0)
  {
    if (!myquery(&mysql,L"LOCK TABLES words READ")) return -1;
    MYSQL_RES *result;
    if (!myquery(&mysql,L"SELECT MAX(id) from words",result)) return -1;
    MYSQL_ROW sqlrow=mysql_fetch_row(result);
    nextIDInDB=atoi(sqlrow[0])+1; // +1 for next id to assign
    mysql_free_result(result);
  }
  // ensure that if a word and its mainEntry are in the same sourceId
  for (tIWMM iWord=begin(),iWordEnd=end(); iWord!=iWordEnd; iWord++)
    if ((iWord->second.index<0 && !iWord->second.isRareWord()) || (iWord->second.index>=0 && (iWord->second.flags&tFI::updateMainInfo)))
      iWord->second.sourceId=-1;
  // repeat - there may be two words referring to the same mainEntry that conflict with each other, but one with the sourceId!=-1
  // may agree with the mainEntry and come first, thus being missed by the previous loop.
  for (unsigned int I=0; I<2; I++)
    for (tIWMM iWord=begin(),iWordEnd=end(); iWord!=iWordEnd; iWord++)
    {
      tIWMM mainEntry=iWord->second.mainEntry;
      if (mainEntry!=wNULL && mainEntry!=iWord && iWord->second.sourceId!=mainEntry->second.sourceId)
      {
        //wstring sWord=iWord->first,sWord2=iWord->second.mainEntry->first;
        iWord->second.flags|=tFI::updateMainInfo;
        mainEntry->second.flags|=tFI::updateMainInfo;
        iWord->second.sourceId=mainEntry->second.sourceId=min(iWord->second.sourceId,mainEntry->second.sourceId);
      }
      #ifdef LOG_WORD_FLOW
        if (mainEntry!=wNULL && mainEntry!=iWord)
          lplog(L"MAINENTRY_CHECK %s %s %d %d",iWord->first.c_str(),mainEntry->first.c_str(),iWord->second.sourceId,mainEntry->second.sourceId);
      #endif
    }
  if (flushNewMainEntryWords(mysql,nextIDInDB)<0)
    return -3;
  if (flushNewWords(mysql,nextIDInDB)<0)
    return -4;
  if (sourceId==-1)
  {
    if (!myquery(&mysql,L"LOCK TABLES words WRITE")) return -1;
    if (!myquery(&mysql,L"ALTER TABLE words ADD CONSTRAINT mainEntryWordId_FK FOREIGN KEY (mainEntryWordId) REFERENCES words (id)")) return -1;
    if (!myquery(&mysql,L"LOCK TABLES words WRITE")) return -1;
    if (!myquery(&mysql,L"ALTER TABLE words ADD CONSTRAINT sourceId_FK FOREIGN KEY (sourceId) REFERENCES sources (id)")) return -1;
    if (!myquery(&mysql,L"LOCK TABLES words WRITE")) return -1;
  }
  if (!myquery(&mysql,L"ALTER TABLE words ENABLE KEYS")) return -1;

  int numUpdates;
  if (updateWords(mysql,numUpdates,sourceId)<0)
    return -5;
  if (updateWordForms(mysql,sourceId==-1)<0)
    return -6;
  if (sourceId==-1)
  {
    if (!myquery(&mysql,L"LOCK TABLES wordForms WRITE")) return -1;
    if (!myquery(&mysql,L"ALTER TABLE wordForms ADD CONSTRAINT wordId_FK FOREIGN KEY (wordId) REFERENCES words (id)")) return -1;
    // the following was removed because this index takes up many gigabytes of space and is never used directly.
    // on 1664063 rows, it produced an index file of 50227735552 bytes (45GB) when dropped, this went to 16.5 MB.
    //if (!myquery(&mysql,L"LOCK TABLES wordForms WRITE")) return -1;
    //if (!myquery(&mysql,L"ALTER TABLE wordForms ADD CONSTRAINT formId_FK FOREIGN KEY (formId) REFERENCES forms (id)")) return -1;
    if (!myquery(&mysql,L"LOCK TABLES wordForms WRITE")) return -1;
  }
  if (!myquery(&mysql,L"ALTER TABLE wordForms ENABLE KEYS")) return -1;
  if (sourceId>=0) updateUsages(mysql);
  myquery(&mysql,L"UNLOCK TABLES");
  wprintf(L"Finished writing to database.      \r");
  //lplog(L"TIME DB WRITE END for source %d:%s",sourceId,getTimeStamp());
	if (extraDetailedLogging)
	  lplog(L"WordClass::write took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int Source::write(int sourceId,wchar_t *sourcePath)
{
  static int flushRelationsInterval=0;
  int ret=0;
  #ifdef WRITE_WORDS_AND_FORMS_TO_DB
	  if (!Words.acquireLock(mysql,true)) return 0;
  #endif
	ret=Words.writeWordsToDB(mysql,sourcePath,sourceId);
  #ifdef WRITE_OBJECTS_TO_DB
    if (!ret) ret=flushObjects(sourceId);
    if (!ret) ret=flushObjectRelations(sourceId);
  #endif
  Words.releaseLock(mysql);
  #ifdef WRITE_WORD_RELATIONS
    if ((flushRelationsInterval++&31)==31)
      ret=Words.flushWordRelations(mysql); // words have their indexes updated
  #endif
  return ret;
}

tFI::tFI(void)
{
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
  verbRelationIndex=-1;
  formsOffset=fACount;
  sourceId=-1;
}

// this procedure does not make a high pattern count into a low cost.
// it makes a high pattern count received from the DB into a count which will fit into the usagePatterns array (max 128)
void tFI::transferDBUsagePatternsToUsagePattern(int highestPatternCount,int *DBUsagePatterns,unsigned int upStart,unsigned int upLength)
{
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
tFI::tFI(unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int iDerivationRules,int iSourceId,int formNum,wstring &word)
{
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
  verbRelationIndex=-1;
  sourceId=iSourceId;
  transferFormsAndUsage(forms,iCount,formNum,word);
  fACount+=(count-iCount);
  count=iCount;
  memset(relationMaps,0,numRelationWOTypes*sizeof(*relationMaps));
}

void tFI::transferFormsAndUsage(unsigned int *forms,unsigned int &iCount,int formNum,wstring &word)
{
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
				::lplog(LOG_INFO,L"numForms of %s over %d: %s.",word.c_str(),MAX_FORM_USAGE_PATTERNS,formStrings.c_str());
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

bool tFI::update(int wordId,unsigned int *forms,unsigned int iCount,int iInflectionFlags,int iFlags,int iTimeFlags,int iDerivationRules,int iSourceId,int formNum,wstring &word)
{
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
  //mainEntry=wNULL;
  //index=-1;
  sourceId=iSourceId;
  flags&=~updateMainInfo; // update from database accepted.  Don't update database
  transferFormsAndUsage(forms,iCount,formNum,word);
  count=iCount;
  return true;
}

void WordClass::assign(int wordId,tIWMM iWord)
{
  while (idsAllocated<=wordId)
  {
		int previousIdsAllocated=idsAllocated;
    idsAllocated+=10240;
    idToMap=(tIWMM *)trealloc(22,idToMap,previousIdsAllocated,idsAllocated*sizeof(*idToMap));
    if (!idToMap)
      lplog(LOG_FATAL_ERROR,L"Out of memory allocating %d bytes to idToMap array.",idsAllocated*sizeof(*idToMap));
    memset(idToMap+idsAllocated-10240,0,10240*sizeof(*idToMap));
  }
  #ifdef LOG_WORD_FLOW
    lplog(L"Word %s assigned to index %d.",iWord->first.c_str(),wordId);
  #endif
  idToMap[wordId]=iWord;
  iWord->second.index=wordId;
}

void tFI::mainEntryCheck(const wstring first)
{
  if (mainEntry==wNULL && (query(nounForm)>=0 || query(verbForm)>=0 || query(adjectiveForm)>=0 || query(adverbForm)>=0))
  {
		if (!iswalpha(first[0])) return;
    ::lplog(LOG_INFO,L"Word %s has no mainEntry (2)!",first.c_str());
    flags|=tFI::queryOnAnyAppearance;
  }
}

void WordClass::readForms(MYSQL &mysql, wchar_t *qt)
{
  MYSQL_RES *result;
  MYSQL_ROW sqlrow;
  _snwprintf(qt,query_buffer_len,L"SELECT id,name,shortName,inflectionsClass,hasInflections,isTopLevel,properNounSubClass,blockProperNounRecognition from forms");
  if (lastReadfromDBTime>0)
    _snwprintf(qt+wcslen(qt),query_buffer_len-wcslen(qt),L" where UNIX_TIMESTAMP(ts)>%d",lastReadfromDBTime);
  if (!myquery(&mysql,qt,result)) return;
  unsigned int numDbForms=0,newDbForms=0;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
    wstring name=mTW(sqlrow[1]),shortName=mTW(sqlrow[2]),inflectionsClass=mTW(sqlrow[3]);
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

int WordClass::readWithLock, (MYSQL &mysql,int sourceId,wchar_t *sourcePath,bool statistics,tIWMM &startUpdateScan)
{
  int startTime=clock();
  acquireLock(mysql,true);
  int numWordsInserted=0,numWordsModified=0;
  int ret=read(mysql,sourceId,sourcePath,statistics,startUpdateScan,numWordsInserted,numWordsModified);
  myquery(&mysql,L"UNLOCK TABLES");
  releaseLock(mysql);
  if (numWordsInserted || numWordsModified)
    lplog(L"%d words inserted into and %d words modified in memory in %d seconds.",numWordsInserted,numWordsModified,(clock()-startTime)/CLOCKS_PER_SEC);
  return ret;
}

void WordClass::generateStatistics(void)
{
  int *formsCount=(int *)tmalloc(Forms.size()*sizeof(int));
  bool *formAlone=(bool *)tmalloc(Forms.size()*sizeof(bool));
  int unknownAlwaysCapitalized=0;
  unsigned int f;
  for (f=0; f<Forms.size(); f++)
  {
    formsCount[f]=0;
    formAlone[f]=true;
  }
  tIWMM iWord,wordEnd=WMM.end();
  for (iWord=WMM.begin(); iWord!=wordEnd; iWord++)
  {
    if (iWord->second.forms()[0]==UNDEFINED_FORM_NUM)
    {
        formsCount[UNDEFINED_FORM_NUM]++;
        lplog(L"%s",iWord->first.c_str());
    }
    else
      for (unsigned int I=0; I<iWord->second.formsSize(); I++)
      {
        formsCount[iWord->second.forms()[I]]++;
        if (iWord->second.formsSize()>1) formAlone[iWord->second.forms()[I]]=false;
      }
  }
  int numFormAlone=0,numFormSingle=0;
  lplog(L"%30s: %6s %s","FORM NAME","COUNT","ALWAYS ALONE");
  for (f=0; f<(signed)Forms.size(); f++)
  {
    lplog(L"%30s: %6d %s",Forms[f]->name.c_str(),formsCount[f],(formAlone[f]) ? L"true" : L"false");
    if (formAlone[f]==true) numFormAlone++;
    if (formsCount[f]==1) numFormSingle++;
  }
  lplog(L"# forms = %d.",Forms.size());
  lplog(L"Forms alone=%d.\nForms having one word=%d.\n# unknown Personal Nouns=%d.\n",numFormAlone,numFormSingle,unknownAlwaysCapitalized);
  lplog(L"# total words=%d.",WMM.size());
  tfree(Forms.size()*sizeof(int),formsCount);
  tfree(Forms.size()*sizeof(bool),formAlone);
}

// three ways this function is used:
// sourceId>=0. source read: read all source words belonging to a certain source
// sourceId==-1. startup read: read all NULL source words (lastReadfromDBTime<0)
// sourceId==-1. update read: read all NULL source since lastReadfromDBTime and update or insert the entries into memory (lastReadfromDBTime>0)
// sourceId==-2. update read: read all words that are mainWord entries of the words that are new in memory and see whether they already exist in database
// sourceId==-3. update read: read all words that are new in memory and see whether they already exist in database
// sourceId==-4. test read: read all words 
// only pay attention to lastReadfromDBTime IF sourceId==-1.  If sourceId is >=0, then the source was never read before (sources are never repeated).
int WordClass::read(MYSQL &mysql,int sourceId,wchar_t *sourcePath,bool statistics,tIWMM &startUpdateScan,int &numWordsInserted,int &numWordsModified)
{
  wprintf(L"Started reading from database...                        \r");
  //lplog(L"TIME DB READ START for source %d:%s",sourceId,getTimeStamp());
  if (!myquery(&mysql,L"LOCK TABLES forms READ,words READ,wordForms READ,wordForms as wf READ,words as w READ,words as w1 READ,words as w2 READ,wordRelations wr READ")) return -1;
  MYSQL_RES *result;
  MYSQL_ROW sqlrow;
  wchar_t qt[query_buffer_len_overflow];
  if (sourceId==-1 || sourceId==-4)
    readForms(mysql,qt);
  if (!myquery(&mysql,L"select MAX(id) from words",result)) return -1;
  if ((sqlrow=mysql_fetch_row(result))==NULL) return -1;
  if (sqlrow[0]==NULL)
  {
    wprintf(L"Finished reading from database...                                                 \r");
    mysql_free_result(result);
    //lplog(L"TIME DB READ END (1) for source %d:%s",sourceId,getTimeStamp());
    return 0; // no words to read
  }
  int maxWordId=atoi(sqlrow[0])+1,len;
  mysql_free_result(result);

  wchar_t newWordsQT[query_buffer_len_overflow];
  if (sourceId==-2 || sourceId==-3)
  {
    len=0;
    tIWMM iWord=startUpdateScan;
    for (tIWMM iWordEnd=end(); iWord!=iWordEnd && len<query_buffer_len; iWord++)
      if (iWord->second.index<0 || (iWord->second.flags&tFI::newWordFlag))
      {
        iWord->second.flags&=~tFI::newWordFlag;
				if (iWord->first!=L"\\")
					len+=_snwprintf(newWordsQT+len,query_buffer_len-len,L"\"%s\",",iWord->first.c_str());
      }
    if (newWordsQT[len-1]==',') newWordsQT[len-1]=0;
    startUpdateScan=(len>=query_buffer_len) ? iWord : wNULL;
    #ifdef LOG_WORD_FLOW
      lplog(L"newWordsQT=%s.",newWordsQT);
    #endif
    if (len==0)
    {
      wprintf(L"Finished reading from database...                                                      \r");
      //lplog(L"TIME DB READ END (2) for source %d:%s",sourceId,getTimeStamp());
      return 0;
    }
  }
  wchar_t newMainEntryWordIdsQT[query_buffer_len_overflow];
  if (sourceId==-2)
  {
    len=0;
    // get mainEntry words of new words
    _snwprintf(qt,query_buffer_len,L"select mainEntryWordId from words where word in (%s) and mainEntryWordId!=id",newWordsQT);
    if (!myquery(&mysql,qt,result)) return -1;
    while ((sqlrow=mysql_fetch_row(result))!=NULL && len<query_buffer_len)
      len+=_snwprintf(newMainEntryWordIdsQT+len,query_buffer_len-len,L"%S,",sqlrow[0]);
    mysql_free_result(result);
    int fromId=0;
    while (fromId!=len)
    {
      #ifdef LOG_WORD_FLOW
        lplog(L"finding newMainEntryWordIdsQT %s",newMainEntryWordIdsQT);
      #endif
      // get mainEntry words of those mainEntries
      newMainEntryWordIdsQT[len-1]=0;
      _snwprintf(qt,query_buffer_len,L"select mainEntryWordId from words where id in (%s) and mainEntryWordId!=id",newMainEntryWordIdsQT+fromId);
      fromId=len;
      if (!myquery(&mysql,qt,result)) return -1;
      newMainEntryWordIdsQT[len-1]=',';
      while ((sqlrow=mysql_fetch_row(result))!=NULL && len<query_buffer_len)
        len+=_snwprintf(newMainEntryWordIdsQT+len,query_buffer_len-len,L"%S,",sqlrow[0]);
      mysql_free_result(result);
      newMainEntryWordIdsQT[len-1]=0;
    }
    if (len>=query_buffer_len)
      lplog(LOG_FATAL_ERROR,L"mainEntryIds exceeded in update request %s",qt);
    if (len==0)
    {
      wprintf(L"Finished reading from database...                                                    \r");
      //lplog(L"TIME DB READ END (3) for source %d:%s",sourceId,getTimeStamp());
      return 0;
    }
    #ifdef LOG_WORD_FLOW
      lplog(L"newMainEntryWordIdsQT=%s.",newMainEntryWordIdsQT);
    #endif
  }
  // read wordForms of the words
  int *words=(int *)tmalloc(maxWordId*sizeof(int));
  int *counts=(int *)tmalloc(maxWordId*sizeof(int));
  int numWordForms;
  memset(words,-1,maxWordId*sizeof(int));
  memset(counts,0,maxWordId*sizeof(int));
  unsigned int *wordForms;
  bool testCacheSuccess=false;
  #ifdef USE_TEST_CACHE
    if (sourceId==-1 && lastReadfromDBTime==-1)
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
    len=_snwprintf(qt,query_buffer_len,L"select wf.wordId, wf.formId, wf.count from wordForms wf, words w where w.id=wf.wordId");
    if (sourceId==-1)
    {
      len+=_snwprintf(qt+len,query_buffer_len-len,L" AND w.sourceId IS NULL");
      if (lastReadfromDBTime>0)
        len+=_snwprintf(qt+len,query_buffer_len-len,L" AND UNIX_TIMESTAMP(w.ts)>%d",lastReadfromDBTime);
    }
    else if (sourceId==-2) len+=_snwprintf(qt+len,query_buffer_len-len,L" AND w.id in (%s)",newMainEntryWordIdsQT); // update all mainEntries of new words
    else if (sourceId==-3) len+=_snwprintf(qt+len,query_buffer_len-len,L" AND w.word in (%s)",newWordsQT);  // update all new words
    else if (sourceId>=0) len+=_snwprintf(qt+len,query_buffer_len-len,L" AND w.sourceId=%d",sourceId);
    _snwprintf(qt+len,query_buffer_len-len,L" ORDER BY wordId, formId");
    if (len>sizeof(qt))
      lplog(LOG_FATAL_ERROR,L"Query %s too long.",qt);
    wprintf(L"Querying word forms from database (sourceId=%d)...                        \r",sourceId);
    //lplog(LOG_INFO,L"%s",qt);
    if (!myquery(&mysql,qt,result)) return -1;
      numWordForms=(int)mysql_num_rows(result);
    wordForms=(unsigned int *)tmalloc(numWordForms*sizeof(int)*2);
    int wf=0;
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
      if (sourcePath!=NULL)
        readWords(sourcePath,sourceId); // no wordRelations possible because none of these exist in DB
      wprintf(L"Finished reading from database...                                               \r");
      tfree(maxWordId*sizeof(int),counts);
      tfree(maxWordId*sizeof(int),words);
      tfree(numWordForms*sizeof(int)*2,wordForms);
      //lplog(L"TIME DB READ END (4) for source %d:%s",sourceId,getTimeStamp());
      return 0;
    }
    #ifdef USE_TEST_CACHE
      if (sourceId==-1 && lastReadfromDBTime==-1)
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
  if (nounForm==-1) nounForm=FormsClass::gFindForm(L"noun");
  if (verbForm==-1) verbForm=FormsClass::gFindForm(L"verb");
  if (adjectiveForm<0) adjectiveForm=FormsClass::gFindForm(L"adjective");
  if (adverbForm<0) adverbForm=FormsClass::gFindForm(L"adverb");
  if (timeForm<0) timeForm=FormsClass::gFindForm(L"time");
  numberForm=NUMBER_FORM_NUM;

  FormsClass::changedForms=false;

  // read words
  wstring sWord;
  tIWMM iWord=wNULL;
  len=_snwprintf(qt,query_buffer_len,L"select id, word, inflectionFlags, flags, timeFlags, derivationRules from words");
  if (sourceId==-1)
  {
    len+=_snwprintf(qt+len,query_buffer_len-len,L" where sourceId IS NULL");
    if (lastReadfromDBTime>0)
      len+=_snwprintf(qt+len,query_buffer_len-len,L" AND UNIX_TIMESTAMP(ts)>%d",lastReadfromDBTime);
  }
  else if (sourceId==-2) len+=_snwprintf(qt+len,query_buffer_len-len,L" where id in (%s)",newMainEntryWordIdsQT); // update all mainEntries of new words
  else if (sourceId==-3) len+=_snwprintf(qt+len,query_buffer_len-len,L" where word in (%s)",newWordsQT); // update all new words
  else if (sourceId>=0)  len+=_snwprintf(qt+len,query_buffer_len-len,L" where sourceId=%d",sourceId);
  _snwprintf(qt+len,query_buffer_len-len,L" ORDER BY word");
  if (!myquery(&mysql,qt,result)) return -1;
  DWORD memoryAllocated=0;
  getCounter(L"PrivateBytes",memoryAllocated);
  __int64 numRows=mysql_num_rows(result);
  int where,lastProgressPercent=-1,numWords=0;
  while ((sqlrow=mysql_fetch_row(result))!=NULL)
  {
    int wordId=atoi(sqlrow[0]),inflectionFlags=atoi(sqlrow[2]),flags=atoi(sqlrow[3]),timeFlags=atoi(sqlrow[4]),derivationRules=atoi(sqlrow[5]);
    wstring sWord=mTW(sqlrow[1]);
    if (words[wordId]==-1 && !iswdigit(sWord[0]))
      lplog(LOG_ERROR,L"No forms for %s (wordId=%d) found!",sWord.c_str(),wordId);
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
      if (iWord->second.update(wordId,wordForms+words[wordId],counts[wordId],inflectionFlags,flags,timeFlags,derivationRules,sourceId,selfFormNum,sWord))
        numWordsModified++;
    }
    else
    {
      if (iWord!=wNULL && lastReadfromDBTime<0 && (sourceId==-1 || sourceId==-4)) // previous entry in alphabetical order so that map will insert in linear time
        iWord=WMM.insert(iWord,tWFIMap(sWord,tFI(wordForms+words[wordId],counts[wordId],inflectionFlags,flags,timeFlags,derivationRules,sourceId,selfFormNum,sWord)));
      else
        iWord=WMM.insert(tWFIMap(sWord,tFI(wordForms+words[wordId],counts[wordId],inflectionFlags,flags,timeFlags,derivationRules,sourceId,selfFormNum,sWord))).first;
      numWordsInserted++;
      #ifdef LOG_WORD_FLOW
        lplog(L"Inserted (from sourceId %d) read word %s.",sourceId,sWord.c_str());
      #endif
      assign(wordId,iWord);
    }
    if ((where=numWords++*100/(int)numRows)>lastProgressPercent)
    {
      DWORD tmp=0;
      getCounter(L"PrivateBytes",tmp);
      wchar_t s[256];
      if ((tmp-memoryAllocated) && numWordsInserted)
        _snwprintf(s,256,L"%03d MB used %d bytes/word",(tmp-memoryAllocated)/1024/1024,(tmp-memoryAllocated)/numWordsInserted);
      else
        s[0]=0;
      wprintf(L"PROGRESS: %03d%% words read with %04d seconds elapsed %s\r",where,clock()/CLOCKS_PER_SEC,s);
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
  if (numWordsModified || numWordsInserted)
  {
    // read main entries words
    len=_snwprintf(qt,query_buffer_len,L"select id, mainEntryWordId from words where ");
    if (sourceId==-1)
    {
      len+=_snwprintf(qt+len,query_buffer_len-len,L"sourceId IS NULL AND ");
      if (lastReadfromDBTime>0)
        len+=_snwprintf(qt+len,query_buffer_len-len,L"UNIX_TIMESTAMP(ts)>%d AND ",lastReadfromDBTime);
    }
    else if (sourceId==-2) len+=_snwprintf(qt+len,query_buffer_len-len,L"id in (%s) AND ",newMainEntryWordIdsQT); // update all mainEntries of new words
    else if (sourceId==-3) len+=_snwprintf(qt+len,query_buffer_len-len,L"word in (%s) AND ",newWordsQT); // update all new words
    else if (sourceId>=0)  len+=_snwprintf(qt+len,query_buffer_len-len,L"sourceId=%d AND ",sourceId);
    _snwprintf(qt+len,query_buffer_len-len,L"mainEntryWordId IS NOT NULL ORDER BY id");
    if (!myquery(&mysql,qt,result)) return -1;
    int numMainEntriesNotFound=0;
    while ((sqlrow=mysql_fetch_row(result))!=NULL)
    {
      int wordId=atoi(sqlrow[0]),mainEntry=atoi(sqlrow[1]);
      if (idToMap[wordId]==wNULL || idToMap[mainEntry]==wNULL)
      {
        if (!numMainEntriesNotFound) lplog(LOG_INFO,L"%s",qt);
        numMainEntriesNotFound++;
        if (idToMap[wordId]==wNULL)
          lplog(L" WordId %d not found (sourceId=%d lastReadfromDBTime=%d)!",wordId,sourceId,lastReadfromDBTime); // mainEntries and any references must always have the same source
        if (idToMap[mainEntry]==wNULL)
          lplog(L" mainEntryWordId %d not found for wordId %d (%s) (sourceId=%d lastReadfromDBTime=%d)!",mainEntry,wordId,idToMap[wordId]->first.c_str(),sourceId,lastReadfromDBTime); // mainEntries and any references must always have the same source
      }
      idToMap[wordId]->second.mainEntry=idToMap[mainEntry];
    }
    if (numMainEntriesNotFound) lplog(L"%d main entries not found!",numMainEntriesNotFound);
    mysql_free_result(result);
  }
  if (numWordsModified || numWordsInserted)
    for (tIWMM w=begin(),wEnd=end(); w!=wEnd; w++) 
			w->second.mainEntryCheck(w->first);
  if (statistics) generateStatistics();
  if ((sectionWord=WMM.find(L"|||"))==WMM.end())
  {
    bool added;
    addNewOrModify(wstring(L"|||"),tFI::topLevelSeparator,SECTION_FORM_NUM,0,0,L"section",-1,added);
    sectionWord=WMM.find(L"|||");
  }
  if (sourceId==-1)
  {
    if (!myquery(&mysql,L"select UNIX_TIMESTAMP(NOW())",result)) return -1;
    if ((sqlrow=mysql_fetch_row(result))==NULL) return -1;
    if (sqlrow[0]==NULL)
    {
      wprintf(L"Finished reading from database...                                               \r");
      mysql_free_result(result);
      //lplog(L"TIME DB READ END (4) for source %d:%s",sourceId,getTimeStamp());
      return 0; // no words to read
    }
    lastReadfromDBTime=atoi(sqlrow[0])+1;
    mysql_free_result(result);
  }
  wprintf(L"Finished reading from database...                                                  \r");
  //lplog(L"TIME DB READ END (5) for source %d:%s",sourceId,getTimeStamp());
  return 0;
}

int Source::getAllSource(enum sourceType st,MYSQL_RES * &result)
{
  wchar_t qt[query_buffer_len_overflow];
  if (!myquery(&mysql,L"LOCK TABLES sources READ")) return -1;
  _snwprintf(qt,query_buffer_len,L"select id, path, start, repeatStart FROM sources where sourceType=%d",st);
  if (!myquery(&mysql,qt,result)) return -1;
  unlockTables();
  int numSources=(int)mysql_num_rows(result);
  // don't free result, will be used by callee!
  return numSources;
}