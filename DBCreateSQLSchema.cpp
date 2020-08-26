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
#include "QuestionAnswering.h"

bool checkFull(MYSQL *mysql,wchar_t *qt,size_t &len,bool flush,wchar_t *qualifier);

//maxSynonymAccumulatedSize = 552
//maxAntonymAccumulatedSize = 106
//maxPrimarySynonymAccumulatedSize = 86
//maxConceptSize = 35

int cSource::createThesaurusTables()
{
	if (!myquery(&mysql, L"CREATE TABLE thesaurus ("
		L"mainEntry CHAR(32) CHARACTER SET utf8mb4 NOT NULL, INDEX me_ind (mainEntry), "
		L"wordType SMALLINT UNSIGNED NOT NULL,"
		L"primarySynonyms CHAR(100) CHARACTER SET utf8mb4 NOT NULL, "
		L"accumulatedSynonyms TEXT(1000) CHARACTER SET utf8mb4 NOT NULL, "
		L"accumulatedAntonyms CHAR(120) CHARACTER SET utf8mb4 NOT NULL, "
		L"concepts CHAR(40) CHARACTER SET utf8mb4 NOT NULL)")) 
		return -1;
	return 0;
}

/*
typedef struct {
string mainEntry;
string wordType;
vector <string> primarySynonyms;
vector <string> accumulatedSynonyms, accumulatedAntonyms;
vector <int> concepts;
vector <string> rest;
} sDefinition;

*/
int cSource::writeThesaurusEntry(sDefinition &d)
{
	wchar_t qt[4096];
	char *wt[] = { "adj", "adv", "prep", "pron", "conj", "det", "interj", "n", "v", NULL };
	int w,wtTotal=0;
	for (int I = 0; wt[I]; I++)
		if ((w = d.wordType.find(wt[I])) != string::npos)
			wtTotal += 1 << I;
	if (wtTotal&((1 << 3) | (1 << 4) | (1 << 6)))
		wtTotal &= ~(1 << 7);
	if (wtTotal&(1 << 1))
		wtTotal &= ~(1 << 8);
	_snwprintf(qt, 1024, L"INSERT INTO thesaurus VALUES (\"%S\",%d,\"",
		d.mainEntry.c_str(), wtTotal);
	for (int I = 0; I < d.primarySynonyms.size(); I++)
		_snwprintf(qt + wcslen(qt), 1024, L"%S;", d.primarySynonyms[I].c_str());
	wcscat(qt, L"\",\"");
	for (int I = 0; I < d.accumulatedSynonyms.size(); I++)
		_snwprintf(qt + wcslen(qt), 1024, L"%S;", d.accumulatedSynonyms[I].c_str());
	wcscat(qt, L"\",\"");
	for (int I = 0; I < d.accumulatedAntonyms.size(); I++)
		_snwprintf(qt + wcslen(qt), 1024, L"%S;", d.accumulatedAntonyms[I].c_str());
	wcscat(qt, L"\",\"");
	for (int I = 0; I < d.concepts.size(); I++)
		_snwprintf(qt + wcslen(qt), 1024, L"%d;", d.concepts[I]);
	wcscat(qt, L"\")");
	return myquery(&mysql, qt);
}

// thse are not used and do not exist within the database
int cSource::createGroupTables(void)
{ LFS
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

// thse are not used and do not exist within the database
int cSource::createLocationTables(void)
{ LFS
	/*
		locations table:
		location id (auto-generated)
		sourceId smallint
		where int 
		generic location id (key to locations table) - park/room/building etc
	*/
	if (!myquery(&mysql,L"CREATE TABLE locations ("
		L"id int(11) unsigned NOT NULL auto_increment unique, sourceId smallint NOT NULL, w int NOT NULL, "
		L"genericLocation INT UNSIGNED, FOREIGN KEY (genericLocation) REFERENCES locations(id))")) return -1;
	/*
		locationObjectAssociation table:
		location id (key to locations table)
		word (representing noun), physically present at said location
		specific location where (sourceId + where)
	*/
	if (!myquery(&mysql,L"CREATE TABLE locationObjectAssociation ("
		L"locationId INT UNSIGNED NOT NULL, FOREIGN KEY (locationId) REFERENCES locations(id), "
    L"wordId INT UNSIGNED NOT NULL, INDEX w_ind (wordId), FOREIGN KEY (wordId) REFERENCES words(id), "
		L"sourceId smallint NOT NULL, "
		L"w int NOT NULL)")) return -1;
	return 0;
}

int cSource::createObjectTables(void)
{ LFS
  // the name bit will be set so that objectWordMap orderOrType will be defined as a type,
  // hon,hon2,hon3; first; middle,middle2; last; suffix; any;
  if (!myquery(&mysql,L"CREATE TABLE objects ("
		L"sourceId INT NOT NULL, "
		L"objectNum INT NOT NULL, "
		L"objectClass INT NOT NULL, "
    L"numEncounters INT NOT NULL, "
		L"numIdentifiedAsSpeaker INT NOT NULL, "
		L"nickName INT NOT NULL, "
    L"ownerWhere INT NOT NULL, "
		L"firstSpeakerGroup INT NOT NULL, "
		L"profession INT NOT NULL, "
		L"ts TIMESTAMP, "
		L"identified BIT NOT NULL, plural BIT NOT NULL, male BIT NOT NULL, female BIT NOT NULL, neuter BIT NOT NULL, "
    L"common BIT NOT NULL, name BIT NOT NULL)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE objectLocations ("
    L"objectId INT UNSIGNED NOT NULL, INDEX oi_ind (objectId), FOREIGN KEY (objectId) REFERENCES objects(id), "
    L"sourceId INT UNSIGNED NOT NULL, INDEX s_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id), "
    L"at INT)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE objectWordMap ("
		L"sourceId INT NOT NULL, "
		L"objectNum INT NOT NULL, "
    L"wordId INT UNSIGNED NOT NULL, INDEX w_ind (wordId), FOREIGN KEY (wordId) REFERENCES words(id), "
    L"orderOrType TINYINT NOT NULL)")) return -1;
  return 0;
}

int cSource::createTimeRelationTables(void)
{ LFS
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
    L"type VARCHAR(256) CHARACTER SET utf8mb4 NOT NULL)")) return -1;
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
//       add to vector <cRelation> cr:
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
//       add to vector <cRelation> cr:
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
//       add to vector <cRelation> cr:
//         index is objectId to Mary
//         count++;
//   RelationsWithObjects DirectWordWithVerb
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is objectId to Mary
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <cRelation> cr:
//         index is objectId to Larry
//         count++;

int cSource::createRelationTables(void)
{ LFS
  if (!myquery(&mysql,L"CREATE TABLE objectRelations (id int(11) unsigned NOT NULL auto_increment unique, "
    L"fromObjectId INT UNSIGNED NOT NULL, INDEX fo_ind (fromObjectId), FOREIGN KEY (fromObjectId) REFERENCES objects(id), "
    L"toObjectId INT UNSIGNED NOT NULL, INDEX to_ind (toObjectId), FOREIGN KEY (toObjectId) REFERENCES objects(id), "
    L"totalCount INT NOT NULL, "
    L"typeId INT UNSIGNED NOT NULL, INDEX t_ind(typeId), "
    L"ts TIMESTAMP)")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE wordRelations ("
		L"id int(11) unsigned NOT NULL auto_increment unique, "
		L"sourceId smallint(5) unsigned NOT NULL, "
		L"lastWhere int NOT NULL, "
    L"fromWordId INT UNSIGNED NOT NULL DEFAULT '0', INDEX fw_ind (fromWordId), FOREIGN KEY (fromWordId) REFERENCES words(id), "
    L"toWordId INT UNSIGNED NOT NULL DEFAULT '0', INDEX tw_ind (toWordId), FOREIGN KEY (toWordId) REFERENCES words(id), "
    L"typeId SMALLINT UNSIGNED NOT NULL DEFAULT '0', "
    L"UNIQUE INDEX uw_ind (fromWordId,toWordId,typeId), " // must be kept for ON DUPLICATE KEY UPDATE logic in flushWordRelations
    L"totalCount INT NOT NULL DEFAULT '0', "
    L"ts TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, INDEX ts_ind (ts)) DELAY_KEY_WRITE=1")) return -1;
	if (!myquery(&mysql, L"CREATE TABLE wordrelationsmemory ("
		L"id int(11) unsigned NOT NULL AUTO_INCREMENT,"
		L"sourceId smallint(5) unsigned NOT NULL,"
		L"lastWhere int(10) unsigned NOT NULL,"
		L"fromWordId int(10) unsigned NOT NULL DEFAULT '0',"
		L"toWordId int(10) unsigned NOT NULL DEFAULT '0',"
		L"typeId smallint(5) unsigned NOT NULL DEFAULT '0',"
		L"totalCount int(11) NOT NULL DEFAULT '0',"
		L"UNIQUE KEY id (id),"
		L"UNIQUE KEY uw_ind (fromWordId,typeId),"
		L"KEY fw_ind (fromWordId),"
		L"KEY tw_ind (toWordId),"
		L") ENGINE = MEMORY AUTO_INCREMENT = 36258188 DEFAULT CHARSET = latin1 DELAY_KEY_WRITE = 1;")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE multiWordRelations ("
		L"id int(11) unsigned NOT NULL auto_increment unique, "
    L"sourceId INT SIGNED NOT NULL DEFAULT -1, INDEX sourceId_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id), "
		L"w INT SIGNED NOT NULL DEFAULT -1, " // where is a reserved word so renamed to 'w'
		L"sentenceNum INT SIGNED NOT NULL DEFAULT -1, "
		L"narrativeNum INT SIGNED NOT NULL DEFAULT -1, "
		L"speaker INT SIGNED NOT NULL DEFAULT -1, " 
		L"audience INT SIGNED NOT NULL DEFAULT -1, " 
    L"referencingEntityLocal INT SIGNED NOT NULL DEFAULT -1, INDEX referencingEntity_ind (referencingEntityLocal), FOREIGN KEY (referencingEntityLocal) REFERENCES words(id), "
    L"referencingEntityMatched INT SIGNED NOT NULL DEFAULT -1, "
		L"referencingVerb INT SIGNED NOT NULL DEFAULT -1,FOREIGN KEY (referencingVerb) REFERENCES words(id), "
    L"subjectAdjectiveMatchedOwner INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (subjectAdjectiveMatchedOwner) REFERENCES objects(id), "
    L"subjectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (subjectAdjective) REFERENCES words(id), "
    L"subjectAdjective2 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (subjectAdjective2) REFERENCES words(id), "
    L"subjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX subject_ind (subjectLocal), FOREIGN KEY (subjectLocal) REFERENCES words(id), "
		L"subjectMatched INT SIGNED NOT NULL DEFAULT -1, "
    L"verb INT SIGNED NOT NULL DEFAULT -1,FOREIGN KEY (verb) REFERENCES words(id), "
    L"adverb INT SIGNED DEFAULT -1,FOREIGN KEY (adverb) REFERENCES words(id), "
    L"objectAdjectiveMatchedOwner INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjectiveMatchedOwner) REFERENCES objects(id), "
    L"objectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjective) REFERENCES words(id), "
    L"objectAdjective2 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjective2) REFERENCES words(id), "
    L"objectAdjective3 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjective3) REFERENCES words(id), "
    L"objectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX object_ind (objectLocal), FOREIGN KEY (objectLocal) REFERENCES words(id), "
		L"objectMatched INT SIGNED NOT NULL DEFAULT -1, "
    L"nextObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX nextObjectLocal_ind (nextObjectLocal), FOREIGN KEY (nextObjectLocal) REFERENCES words(id), "
		L"nextObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
    L"secondaryVerb INT SIGNED NOT NULL DEFAULT -1,FOREIGN KEY (verb) REFERENCES words(id), "
    L"secondaryObjectAdjectiveMatchedOwner INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (secondaryObjectAdjectiveMatchedOwner) REFERENCES objects(id), "
    L"secondaryObjectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (secondaryObjectAdjective) REFERENCES words(id), "
    L"secondaryObjectAdjective2 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (secondaryObjectAdjective2) REFERENCES words(id), "
    L"secondaryObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX secondaryObjectLocal_ind (secondaryObjectLocal), FOREIGN KEY (secondaryObjectLocal) REFERENCES words(id), "
		L"secondaryObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
    L"nextSecondaryObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX secondaryObjectLocal_ind (secondaryObjectLocal), FOREIGN KEY (secondaryObjectLocal) REFERENCES words(id), "
		L"nextSecondaryObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
    L"relationType INT SIGNED NOT NULL DEFAULT -1, "
    L"objectSubType INT SIGNED NOT NULL DEFAULT -1, "
    L"timeProgression INT SIGNED NOT NULL DEFAULT -1, "
    L"flags BIGINT SIGNED NOT NULL DEFAULT -1) DELAY_KEY_WRITE=1")) return -1;
  if (!myquery(&mysql,L"CREATE TABLE prepPhraseMultiWordRelations ("
    L"sourceId INT SIGNED NOT NULL DEFAULT -1, INDEX sourceId_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id), "
		L"w INT SIGNED NOT NULL DEFAULT -1, " // where is a reserved word so renamed to 'w'
    L"nearestVerb INT SIGNED DEFAULT NULL, INDEX nearestVerb_ind (nearestVerb), FOREIGN KEY (nearestVerb) REFERENCES words(id), "
    L"nearestObject INT SIGNED DEFAULT NULL, INDEX nearestObject_ind (nearestObject), FOREIGN KEY (nearestObject) REFERENCES words(id), "
    L"principalWord INT SIGNED DEFAULT NULL, INDEX principalWord_ind (principalWord), FOREIGN KEY (principalWord) REFERENCES words(id), "
    L"immediatePrincipalWord INT SIGNED DEFAULT NULL, INDEX immediatePrincipalWord_ind (immediatePrincipalWord), FOREIGN KEY (immediatePrincipalWord) REFERENCES words(id), "
		L"prep INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (prep) REFERENCES words(id), "
		L"prepObjectAdjectiveMatchedOwner int(11) NOT NULL DEFAULT -1, "
    L"prepObjectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (prepObjectAdjective) REFERENCES words(id), "
    L"prepObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX prepObjectLocal_ind (prepObjectLocal), FOREIGN KEY (prepObjectLocal) REFERENCES words(id), "
		L"prepObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
    L"prepObjectSubType INT SIGNED NOT NULL DEFAULT -1, "
    L"flags INT SIGNED NOT NULL DEFAULT -1) DELAY_KEY_WRITE=1")) return -1;
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
  return 0;
}

int generateBNCSources(MYSQL &mysql,wstring indexFile) // note this is slightly modified from the original
{ LFS
  int startTime=clock();
  wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
  wcscpy(qt,L"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
  size_t len=wcslen(qt);
  HANDLE hFile = CreateFile(indexFile.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
		wstring bcct;
    if (GetLastError()!=ERROR_PATH_NOT_FOUND)
      lplog(LOG_FATAL_ERROR,L"GetPath cannot open BNC sources path %s - %s",indexFile.c_str(),getLastErrorMessage(bcct));
    return -17;
  }
  unsigned int actualLen=GetFileSize(hFile,NULL);
  if (actualLen<=0)
  {
    lplog(LOG_ERROR,L"ERROR:filelength of file %s yields an invalid filelength (%d).",indexFile.c_str(),actualLen);
    CloseHandle(hFile);
    return -18;
  }
  wchar_t *buffer=(wchar_t *)tmalloc(actualLen+1);
  DWORD lenRead=0;
  if (!ReadFile(hFile,buffer,actualLen,&lenRead,NULL) || actualLen!=lenRead)
  {
    lplog(LOG_ERROR,L"ERROR:read error of file %s.",indexFile.c_str());
    CloseHandle(hFile);
    return -19;
  }
  buffer[actualLen]=0;
  CloseHandle(hFile);
	int where = 0, returnCode = -20;// , numWords = 0;
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
		len+=_snwprintf(qt+len,QUERY_BUFFER_LEN-len,L"(%d, \"%s\", \"\", 0),",cSource::BNC_SOURCE_TYPE,id);
    if (!checkFull(&mysql,qt,len,false,NULL)) return -20;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -21;
	if (logDatabaseDetails)
		lplog(L"Inserting BNC sources took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  tfree(actualLen+1,buffer);
  return returnCode;
}

int generateNewsBankSources(MYSQL &mysql)
{ LFS
  int startTime=clock();
  wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
  wcscpy(qt,L"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
  size_t len=wcslen(qt);
  for (unsigned int I=2557; I<7057; I++)
  {
    time_t timer=I*24*3600;
    struct tm *day=gmtime(&timer);
		len+=_snwprintf(qt+len,QUERY_BUFFER_LEN-len,L"(%d, \"newsbank\\%d\\%I64d.txt\", \"\", 0),",cSource::NEWS_BANK_SOURCE_TYPE,day->tm_year+1900,timer/(24*3600));
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
	if (logDatabaseDetails)
	  lplog(L"Inserting NewsBank sources took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int generateTestSources(MYSQL &mysql)
{ LFS
  int startTime=clock();
  wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
  wcscpy(qt,L"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
  size_t len=wcslen(qt);
  wchar_t *testSources[]={L"agreement",L"date-time-number",L"lappinl",L"modification",L"Nameres",L"pattern matching",L"resolution",L"time",L"timeExpressions",L"usage",
                       L"verb object",NULL};     
  for (unsigned int I=0; testSources[I]; I++)
  {
		len+=_snwprintf(qt+len,QUERY_BUFFER_LEN-len,L"(%d, \"tests\\\\%s.txt\", \"~~BEGIN\", 1),",cSource::TEST_SOURCE_TYPE,testSources[I]);
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
	if (logDatabaseDetails)
	  lplog(L"Inserting Test sources took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
  return 0;
}

int generateParseRequestSources(MYSQL &mysql,vector <cQuestionAnswering::cSearchSource>::iterator pri)
{
	wchar_t qt[16384];
	wstring pathInCache = pri->pathInCache;
	pathInCache = pathInCache.substr(wcslen(TEXTDIR)+1,pathInCache.length()- wcslen(TEXTDIR)-1);
	escapeStr(pathInCache);
	wsprintf(qt, L"INSERT INTO sources (sourceType, etext, path, start, repeatStart, author, title, processing, processed) VALUES (%d,\"%s\",\"%s\",\"\",0,\"\",\"%s\",NULL,NULL)",
		cSource::REQUEST_TYPE, (pri->isSnippet) ? L"snippet" : L"full", pathInCache.c_str(), pri->fullWebPath.c_str());
		return myquery(&mysql, qt);
}

int deleteGeneratedParseRequests(MYSQL &mysql)
{
	wchar_t qt[1024];
	wsprintf(qt, L"delete from sources where sourceType=%d",cSource::REQUEST_TYPE);
	return myquery(&mysql,qt);
}

int cSource::insertWordRelationTypes(void)
{ LFS
  wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	wcscpy(qt,L"INSERT INTO wordRelationType VALUES ");
  size_t len=wcslen(qt);
  for (unsigned int I=firstRelationType; I<numRelationWOTypes; I++)
  {
    len+=_snwprintf(qt+len,QUERY_BUFFER_LEN-len,L"(%d, \"%s\"),",I,getRelStr(I));
    if (!checkFull(&mysql,qt,len,false,NULL)) return -1;
  }
  if (!checkFull(&mysql,qt,len,true,NULL)) return -1;
  return 0;
}

int cSource::createDatabase(wchar_t *server)
{ LFS
  if (!initializeDatabaseHandle(mysql,server,alreadyConnected))
  {
    mysql_close(&mysql);
    return 0;
  }
	string sqlStr;
  if (mysql_errno(&mysql)!=ER_BAD_DB_ERROR || mysql_real_connect(&mysql,wTM(server,sqlStr),"root","byron0",NULL,0,NULL,0)==NULL)
  {
    lplog(LOG_FATAL_ERROR,L"Failed to connect to MySQL - %S", mysql_error(&mysql));
    //int err=mysql_errno(&mysql);
    return -1;
  }
  wchar_t qt[2*QUERY_BUFFER_LEN_OVERFLOW];
  _snwprintf(qt,QUERY_BUFFER_LEN,L"CREATE DATABASE %s character set utf8mb4",LDBNAME);
  if (!myquery(&mysql,qt)) return -1;
  _snwprintf(qt,QUERY_BUFFER_LEN,L"USE %s",LDBNAME);
  if (!myquery(&mysql,qt)) return -1;
  // create forms table
  if (!myquery(&mysql,L"CREATE TABLE forms ("
		L"id int(11) unsigned NOT NULL auto_increment unique, "
		L"name CHAR(32) CHARACTER SET utf8mb4 NOT NULL, "
		L"shortName CHAR(32) CHARACTER SET utf8mb4 NOT NULL, "
		L"inflectionsClass CHAR(12) CHARACTER SET utf8mb4 NOT NULL, "
    L"hasInflections BIT NOT NULL, "
		L"isTopLevel BIT NOT NULL, "
		L"properNounSubClass BIT NOT NULL, "
		L"blockProperNounRecognition BIT NOT NULL,"
		L"ts TIMESTAMP)")) return -1;
  // create sources table
  if (!myquery(&mysql,L"CREATE TABLE sources "
		L"(id int(11) unsigned NOT NULL auto_increment unique, "
		L"sourceType TINYINT(4) NOT NULL, "
		L"etext VARCHAR (10) CHARACTER SET utf8mb4,"
		L"path VARCHAR (1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,"
		L"start VARCHAR(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL, "
		L"repeatStart INT NOT NULL, "
		L"author VARCHAR (128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
		L"title VARCHAR (1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
		L"date DATETIME,"
    L"numSentences INT, matchedSentences INT, numWords INT, numUnknown INT, numUnmatched INT, numOvermatched INT,"
    L"numQuotations INT, quotationExceptions INT, numTicks INT, numPatternMatches INT, "
		L"sizeInBytes INT, numWordRelations INT, numMultiWordRelations INT, "
    L"processing BIT, processed BIT, "
		L"proc2 INT, " // other processing steps
		L"duplicateId INT," // if not null, this source should be skipped because it is a duplicate of another source (id)
		L"lastProcessedTime TIMESTAMP DEFAULT 0, " // not set to current timestamp or updated automatically
		L"ts TIMESTAMP," // specifying nothing is the same as DEFAULT CURRENT_TIMESTAMP and ON UPDATE CURRENT_TIMESTAMP
		L"KEY `EtextIndex` (`etext`),"
		L"KEY `nsi` (`sourceType`,`start`,`processed`)"
		L") DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_bin"))
  {
    lplog(LOG_FATAL_ERROR,L"Failed to create sources - %S", mysql_error(&mysql));
    return -1;
  }
  int actualLen;
  getPath(L"source\\lists\\bookSources.sql",qt,2*QUERY_BUFFER_LEN,actualLen);
	qt[actualLen]=0;
  wstring source=qt;
  //mysql_real_escape_string(&mysql, qt, source.c_str(), actualLen);
  if (!myquery(&mysql,qt))
  {
    lplog(LOG_FATAL_ERROR,L"Failed to populate sources with books - %S", mysql_error(&mysql));
    return -1;
  }
  generateBNCSources(mysql, wstring(LMAINDIR) + L"\\BNC-world\\doc\\Source\\bncIndex.xml");
  generateNewsBankSources(mysql);
  generateTestSources(mysql);
  // create wordRelations types table (for convenience)
  if (!myquery(&mysql,L"CREATE TABLE wordRelationType (id int(11) unsigned NOT NULL unique, type CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL)"))
  insertWordRelationTypes();
  // create words table
	if (!myquery(&mysql, L"CREATE TABLE words (id int(11) unsigned NOT NULL auto_increment unique, "
		L"word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT '',"
		L"inflectionFlags INT UNSIGNED NOT NULL default '0',"
		L"flags INT NOT NULL default '0',"
		L"timeFlags INT NOT NULL default '0'," // INDEX word_ind (word) dropped (increased index space by more than 1000 times)
		L"mainEntryWordId INT UNSIGNED NULL, INDEX me_ind (mainEntryWordId), "
		L"derivationRules INT DEFAULT 0, "
		L"sourceId INT UNSIGNED DEFAULT NULL, INDEX s_ind (sourceId), " //FOREIGN KEY (sourceId) REFERENCES sources(id),
		L"ts TIMESTAMP, "
		L"INDEX ts_ind (ts)"
		L") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"))
	{
		lplog(LOG_FATAL_ERROR, L"Failed to create words table - %S", mysql_error(&mysql));
		return -1;
	}
	// create wordForms table
  if (!myquery(&mysql,L"CREATE TABLE wordForms ("
		L"wordId INT UNSIGNED NOT NULL, "
		L"formId INT UNSIGNED NOT NULL, "
		L"count INT NOT NULL, "
		L"ts TIMESTAMP, "
    L"INDEX wId_ind (wordId), " // FOREIGN KEY (wordId) REFERENCES words(id), "
		L"INDEX fId_ind (formId), "
		L"INDEX ts_ind (ts), "
		L"UNIQUE INDEX uw_ind (wordId,formId)"))
  {
    lplog(LOG_FATAL_ERROR,L"Failed to create wordForms - %S", mysql_error(&mysql));
    return -1;
  }
	if (!myquery(&mysql, L"ALTER TABLE wordForms DELAY_KEY_WRITE = 1")) return -1;
	// words unknown processing
	if (!myquery(&mysql, L"CREATE TABLE wordFrequency ("
		L"word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT '',"
		L"totalFrequency       INT NOT NULL default '0', "
		L"unknownFrequency     INT NOT NULL default '0', "
		L"capitalizedFrequency INT NOT NULL default '0', "
		L"allCapsFrequency     INT NOT NULL default '0', "
		L"lastSourceId INT UNSIGNED DEFAULT NULL, INDEX s_ind (lastSourceId), "
		L"nonEuropeanWord BIT "
		L") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin ENGINE = INNODB"))  // must use INNODB because of per row locking required by multiple processes 
	{
		lplog(LOG_FATAL_ERROR, L"Failed to create wordsFrequency table - %S", mysql_error(&mysql));
		return -1;
	}
	if (!myquery(&mysql, L"CREATE TABLE noRDFTypes ("
		L"word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT ''"
		L") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"))  
	{
		lplog(LOG_FATAL_ERROR, L"Failed to create noRDFTypes table - %S", mysql_error(&mysql));
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

