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

static void *sqlQueryBuffer=NULL; // protect by mySQLQueryBufferSRWLock
static unsigned int sqlQueryBufSize=0; // protect by mySQLQueryBufferSRWLock


bool myquery(MYSQL *mysql,const wchar_t *q,bool allowFailure)
{ LFS
	int seconds=clock(),queryLength;
	AcquireSRWLockExclusive(&mySQLQueryBufferSRWLock);
  void *buffer=WideCharToMultiByte(q,queryLength,sqlQueryBuffer,sqlQueryBufSize);
	ReleaseSRWLockExclusive(&mySQLQueryBufferSRWLock);
	if(mysql_real_query(mysql, (char *)buffer, queryLength )!=0)
  {
    //if (wcslen(q)>QUERY_BUFFER_LEN) q[QUERY_BUFFER_LEN]=0;
		lplog(LOG_ERROR,L"mysql_real_query failed - %S (len=%d): ", mysql_error(mysql),queryLength);
		wstring q2=q;
		q2+=L"\n";
		if (!allowFailure)
			logstring(LOG_FATAL_ERROR,q2.c_str());
		else
			logstring(LOG_ERROR, q2.c_str());
		return false;
    //logstring(-1,NULL);
		//exit(0);
  }
	AcquireSRWLockExclusive(&mySQLTotalTimeSRWLock);
	cProfile::mySQLTotalTime+=clock()-seconds;
	ReleaseSRWLockExclusive(&mySQLTotalTimeSRWLock);
	if (logDatabaseDetails) 
	{
		//lplog(LOG_INFO,L"SQL: %08d:%S",clock()-seconds,buffer);
		//lplog(LOG_INFO,NULL);
	}
	//if ((seconds=clock()-seconds) && seconds/CLOCKS_PER_SEC && !logDatabaseDetails)
	//	lplog(L"%d seconds: query %s",seconds/CLOCKS_PER_SEC,q);
	//lplog(LOG_INFO,L"SQL: %08d:%S",clock()-seconds,buffer);
  return true;
}

bool myquery(MYSQL *mysql,const wchar_t *q,MYSQL_RES * &result,bool allowFailure)
{ LFS
  if (!myquery(mysql,q,allowFailure))
    return false;
  if (!(result= mysql_store_result(mysql)))
    lplog(LOG_INFO,L"%S: Failed to retrieve any results for %s", mysql_error(mysql), q);
  return result!=NULL;
}

void *WideCharToMultiByte(const wchar_t * q,int &queryLength,void *&buffer,unsigned int &bufSize)
{ LFS
	queryLength=WideCharToMultiByte( CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL );
	if (bufSize==0)
	{
		bufSize=max(queryLength*2,10000);
		buffer=tmalloc(bufSize);
		if (!WideCharToMultiByte( CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL ))
		{
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",q);
			return NULL;
		}
	}
	if (!queryLength)
	{
		if (GetLastError()!=ERROR_INSUFFICIENT_BUFFER) 
		{
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",q);
			return NULL;
		}
		queryLength=WideCharToMultiByte( CP_UTF8, 0, q, -1, NULL, 0, NULL, NULL );
		unsigned int previousBufSize=bufSize;
		bufSize=queryLength*2;
		if (!(buffer=trealloc(21,buffer,previousBufSize,bufSize)))
		{
			lplog(LOG_FATAL_ERROR,L"Out of memory requesting %d bytes from sql query %s!",bufSize,q);
			return NULL;
		}
		if (!WideCharToMultiByte( CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL ))
		{
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",q);
			return NULL;
		}
	}
	return buffer;
}

wchar_t *getTimeStamp(void)
{ LFS
   static wchar_t ts[32]; // not used
   struct tm *newtime; 
   time_t aclock;
   time( &aclock );   // Get time in seconds
   newtime = localtime( &aclock );   // Convert time to struct tm form
   if (newtime)
   {
     wchar_t* lt = _wasctime(newtime);
     if (lt)
     {
       /* Print local time as a wstring */
       wcscpy(ts, lt);
       ts[wcslen(ts) - 1] = 0;
     }
   }
   return ts;
}

/*
Tries to obtain a lock with a name given by the wstring str, using a timeout of timeout seconds.
Returns 1 if the lock was obtained successfully,
        0 if the attempt timed out (for example, because another client has previously locked the name), or
        NULL if an error occurred (such as running out of memory or the thread was killed with mysqladmin kill).
If you have a lock obtained with GET_LOCK(), it is released when you execute RELEASE_LOCK(), execute a new GET_LOCK(), or your connection terminates (either normally or abnormally).
*/
bool cWord::acquireLock(MYSQL &mysql,bool persistent)
{ LFS
  int startTime=clock();
  wprintf(L"Acquiring lock on database...\r");
  while (true)
  {
    MYSQL_RES *result=NULL;
    if (!myquery(&mysql,L"SELECT GET_LOCK('lp_lock',20)",result)) return false;
    MYSQL_ROW sqlrow=mysql_fetch_row(result);
    if (sqlrow==NULL)
      lplog(LOG_FATAL_ERROR,L"Error acquiring lock.");
    int lockAcquired=atoi(sqlrow[0]);
    mysql_free_result(result);
    if (lockAcquired==1)
    {
      if ((clock()-startTime)>CLOCKS_PER_SEC && logDatabaseDetails)
				lplog(L"Acquiring global database lock took %d seconds.",(clock()-startTime)/CLOCKS_PER_SEC);
      return true;
    }
    else if (!persistent)
    {
      //lplog(L"Skipping global database lock (%d seconds).",(clock()-startTime)/CLOCKS_PER_SEC);
      return false;
    }
    wprintf(L"Acquiring lock on database (%05ld seconds)...\r",(clock()-startTime)/CLOCKS_PER_SEC);
  }
  return true;
}

/*
Releases the lock named by the wstring str that was obtained with GET_LOCK().
Returns 1 if the lock was released, 0 if the lock was not established by this thread (in which case the lock is not released), and NULL if the named lock did not exist.
The lock does not exist if it was never obtained by a call to GET_LOCK() or if it has previously been released.
*/
#ifdef WRITE_WORDS_AND_FORMS_TO_DB
void cWord::releaseLock(MYSQL &mysql)
{ LFS
  MYSQL_RES *result=NULL;
  if (!myquery(&mysql,L"SELECT RELEASE_LOCK('lp_lock')",result)) return;
  MYSQL_ROW sqlrow=mysql_fetch_row(result);
  if (sqlrow==NULL)
    lplog(LOG_FATAL_ERROR,L"Lock does not exist.");
  if (sqlrow[0]==NULL || atoi(sqlrow[0])==0)
    lplog(LOG_ERROR,L"Lock was never acquired.");
  mysql_free_result(result);
}
#endif

bool checkFull(MYSQL *mysql,wchar_t *qt,size_t &len,bool flush,wchar_t *qualifier)
{ LFS
  bool ret=true;
  if (len>QUERY_BUFFER_LEN_UNDERFLOW || (flush && qt[len-1]==L','))
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
    if(!myquery(mysql, qt,false))
    {
      lplog(L"checkFull %S (len=%d): ", mysql_error(mysql),len);
				wcscat(qt,L"\n");
			myquery(mysql,L"UNLOCK TABLES");
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
		{
			myquery(mysql,L"UNLOCK TABLES");
      lplog(LOG_FATAL_ERROR,L"Incorrect command %s given to checkFull",qt);
		}
  }
  return ret;
}

unsigned long encodeEscape(MYSQL &mysql, wstring &to, wstring from)
{ LFS
	string sFrom;
	wTM(from,sFrom);
	char tmp[1024];
	mysql_real_escape_string(&mysql, tmp, sFrom.c_str(), sFrom.length());
	mTW(tmp,to);
	return 0;
}

void escapeStr(wstring &str)
{ LFS
	wstring ess;
	for (unsigned int I=0; I<str.length(); I++)
	{
		if (str[I]=='\'') ess+='\\';
		ess+=str[I];
		if (str[I]=='\\') ess+='\\';
	}
	str=ess;
}

void cWord::generateFormStatistics(void)
{ LFS
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

