/*
	DBUtility.cpp - the bottom of the MySQL access layer: statement execution, escaping and the accumulating-INSERT helper

	Overview:
		Everything in the program that talks to MySQL goes through myquery() here.
		A statement is built as a wide string by the caller, translated to UTF-8 into
		one shared scratch buffer (sqlQueryBuffer) and executed with
		mysql_real_query().  The rest of the file is the support cast: the wide->UTF-8
		converter that grows that buffer, checkFull() (which lets callers accumulate
		thousands of rows into one giant INSERT ... VALUES (..),(..),.. and flush it
		when the buffer is nearly full), the two SQL string escapers, the global
		database advisory lock (MySQL GET_LOCK) used to serialise multiple
		LPProcessor processes, and a form-usage statistics dump.

	Pipeline position:
		Stage 1 (initialization) and every later stage that reads or writes words,
		word relations, objects or sources.  Called from DB.cpp, DBWordRelations.cpp,
		DBCreateSQLSchema.cpp, source.cpp, get*.cpp and main.cpp.

	Key entry points:
		- myquery() x2 - execute a statement, optionally storing the result set.
		- WideCharToMultiByte() (the 4-argument overload defined here, which shadows
			the Win32 API name) - wide->UTF-8 into a caller-owned growable buffer.
		- checkFull() - flush an accumulating INSERT/IN list when it nears full.
		- escapeStr() / encodeEscape() - SQL literal escaping.
		- cWord::acquireLock() / releaseLock() - cross-process advisory DB lock.
		- cWord::generateFormStatistics() - log per-form word counts.

	Key data structures / globals:
		- sqlQueryBuffer / sqlQueryBufSize - one process-wide UTF-8 scratch buffer for
			the statement currently being sent.  Grows on demand, never freed.
			Documented as protected by mySQLQueryBufferSRWLock.
		- cProfile::mySQLTotalTime - accumulated query time, guarded by
			mySQLTotalTimeSRWLock.

	Dependencies:
		libmysql (mysql_real_query, mysql_store_result, mysql_real_escape_string),
		Win32 WideCharToMultiByte, the tmalloc/trealloc tracked allocator, lplog.

	Notes / gotchas:
		- lplog(LOG_FATAL_ERROR,...) does not return: logstring() exits EXIT_FAILURE.  So a
			failed statement with allowFailure==false terminates the process, and code
			written after such a call is effectively unreachable.
		- mySQLQueryBufferSRWLock is held across mysql_real_query() itself (not just the
			wide->UTF-8 conversion), because the query pointer is into the shared
			process-wide sqlQueryBuffer: releasing before the query runs would let a
			concurrent caller grow/realloc that buffer out from under it.
		- Escaping here is hand-rolled and single-quote oriented; callers that quote
			values with double quotes are not covered by escapeStr().
		- LFS is the profiling macro from profile.h and expands to nothing unless
			PROFILE is defined, which is why declarations appear to hang off it.
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

static void* sqlQueryBuffer = NULL; // protect by mySQLQueryBufferSRWLock
static unsigned int sqlQueryBufSize = 0; // protect by mySQLQueryBufferSRWLock


// Execute one SQL statement that returns no result set.
// q is the statement as a wide string; it is translated to UTF-8 into the shared
// sqlQueryBuffer and sent with mysql_real_query().
// allowFailure==false: a server-side error is logged at LOG_FATAL_ERROR, which exits
// the process.  allowFailure==true: the error is logged and false is returned so the
// caller can carry on (used for "CREATE TABLE which may already exist" and for
// INSERTs that may hit a duplicate key).
// Returns true if the server accepted the statement.
// Side effects: mutates the shared query buffer; adds the elapsed time to
// cProfile::mySQLTotalTime under mySQLTotalTimeSRWLock.
bool myquery(MYSQL* mysql, const wchar_t* q, bool allowFailure)
{
	LFS
		int seconds = clock(), queryLength;
	AcquireSRWLockExclusive(&mySQLQueryBufferSRWLock);
	// this is the 4-argument overload below, not the Win32 API of the same name; it may
	// grow/replace sqlQueryBuffer.  The lock is held across mysql_real_query() itself (not
	// released right after the conversion) because buffer points into that same
	// process-wide sqlQueryBuffer: releasing early would let a concurrent caller
	// grow/realloc it out from under this pointer while mysql_real_query() is still
	// reading it.
	void* buffer = WideCharToMultiByte(q, queryLength, sqlQueryBuffer, sqlQueryBufSize);
	int queryFailed = mysql_real_query(mysql, (char*)buffer, queryLength) != 0;
	ReleaseSRWLockExclusive(&mySQLQueryBufferSRWLock);
	if (queryFailed)
	{
		//if (wcslen(q)>QUERY_BUFFER_LEN) q[QUERY_BUFFER_LEN]=0;
		lplog(LOG_ERROR, L"mysql_real_query failed - %S (len=%d): ", mysql_error(mysql), queryLength);
		wstring q2 = q;
		q2 += L"\n";
		if (!allowFailure)
			logstring(LOG_FATAL_ERROR, q2.c_str());
		else
			logstring(LOG_ERROR, q2.c_str());
		return false;
		//logstring(-1,NULL);
		//exit(0);
	}
	AcquireSRWLockExclusive(&mySQLTotalTimeSRWLock);
	cProfile::mySQLTotalTime += clock() - seconds;
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

// Execute one SQL statement and retrieve its whole result set into result
// (mysql_store_result, so the rows are buffered client side).
// result is an out parameter and is only meaningful when true is returned; ownership
// passes to the caller, who must mysql_free_result() it on every exit path.
// Returns false if the statement failed OR if it produced no result set at all
// (which for a statement that should return rows means the query was not a SELECT,
// or the server ran out of memory); an empty-but-valid result set still returns true.
bool myquery(MYSQL* mysql, const wchar_t* q, MYSQL_RES*& result, bool allowFailure)
{
	LFS
		if (!myquery(mysql, q, allowFailure))
			return false;
	if (!(result = mysql_store_result(mysql)))
		lplog(LOG_INFO, L"%S: Failed to retrieve any results for %s", mysql_error(mysql), q);
	return result != NULL;
}

// Translate the wide string q to UTF-8 in a caller-owned buffer that is grown as
// needed.  Deliberately overloads the Win32 name WideCharToMultiByte (4 arguments
// instead of 8) and is also used by logging.cpp with thread-local buffers.
// buffer/bufSize are in/out: buffer is allocated (tmalloc) when bufSize==0 and
// reallocated (trealloc) when the existing buffer is too small; both are updated.
// queryLength is out: the byte count returned by the Win32 conversion, which for a
// -1 (NUL-terminated) source INCLUDES the terminating NUL byte.
// Returns buffer, or NULL on a conversion/allocation failure - though the
// LOG_FATAL_ERROR logging on those paths exits the process first.
void* WideCharToMultiByte(const wchar_t* q, int& queryLength, void*& buffer, unsigned int& bufSize)
{
	LFS
		queryLength = WideCharToMultiByte(CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL);
	// first ever call: the probe above passed bufSize 0, so Win32 returned the required
	// byte count without writing anything - allocate that (doubled, minimum 10000) and
	// convert for real.
	if (bufSize == 0)
	{
		bufSize = max(queryLength * 2, 10000);
		buffer = tmalloc(bufSize);
		if (!WideCharToMultiByte(CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL))
		{
			lplog(LOG_FATAL_ERROR, L"Error in translating sql request: %s", q);
			return NULL;
		}
	}
	if (!queryLength)
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			lplog(LOG_FATAL_ERROR, L"Error in translating sql request: %s", q);
			return NULL;
		}
		queryLength = WideCharToMultiByte(CP_UTF8, 0, q, -1, NULL, 0, NULL, NULL);
		unsigned int previousBufSize = bufSize;
		bufSize = queryLength * 2;
		if (!(buffer = trealloc(21, buffer, previousBufSize, bufSize)))
		{
			lplog(LOG_FATAL_ERROR, L"Out of memory requesting %d bytes from sql query %s!", bufSize, q);
			return NULL;
		}
		if (!WideCharToMultiByte(CP_UTF8, 0, q, -1, (LPSTR)buffer, bufSize, NULL, NULL))
		{
			lplog(LOG_FATAL_ERROR, L"Error in translating sql request: %s", q);
			return NULL;
		}
	}
	return buffer;
}

/*
Tries to obtain a lock with a name given by the wstring str, using a timeout of timeout seconds.
Returns 1 if the lock was obtained successfully,
				0 if the attempt timed out (for example, because another client has previously locked the name), or
				NULL if an error occurred (such as running out of memory or the thread was killed with mysqladmin kill).
If you have a lock obtained with GET_LOCK(), it is released when you execute RELEASE_LOCK(), execute a new GET_LOCK(), or your connection terminates (either normally or abnormally).
*/
bool cWord::acquireLock(MYSQL& mysql, bool persistent)
{
	LFS
		int startTime = clock();
	wprintf(L"Acquiring lock on database...\r");
	while (true)
	{
		MYSQL_RES* result = NULL;
		if (!myquery(&mysql, L"SELECT GET_LOCK('lp_lock',20)", result)) return false;
		MYSQL_ROW sqlrow = mysql_fetch_row(result);
		if (sqlrow == NULL)
			lplog(LOG_FATAL_ERROR, L"Error acquiring lock.");
		int lockAcquired = atoi(sqlrow[0]);
		mysql_free_result(result);
		if (lockAcquired == 1)
		{
			if ((clock() - startTime) > CLOCKS_PER_SEC && logDatabaseDetails)
				lplog(L"Acquiring global database lock took %d seconds.", (clock() - startTime) / CLOCKS_PER_SEC);
			return true;
		}
		else if (!persistent)
		{
			//lplog(L"Skipping global database lock (%d seconds).",(clock()-startTime)/CLOCKS_PER_SEC);
			return false;
		}
		wprintf(L"Acquiring lock on database (%05ld seconds)...\r", (clock() - startTime) / CLOCKS_PER_SEC);
	}
	return true;
}

/*
Releases the lock named by the wstring str that was obtained with GET_LOCK().
Returns 1 if the lock was released, 0 if the lock was not established by this thread (in which case the lock is not released), and NULL if the named lock did not exist.
The lock does not exist if it was never obtained by a call to GET_LOCK() or if it has previously been released.
*/
#ifdef WRITE_WORDS_AND_FORMS_TO_DB
void cWord::releaseLock(MYSQL& mysql)
{
	LFS
		MYSQL_RES* result = NULL;
	if (!myquery(&mysql, L"SELECT RELEASE_LOCK('lp_lock')", result)) return;
	MYSQL_ROW sqlrow = mysql_fetch_row(result);
	if (sqlrow == NULL)
		lplog(LOG_FATAL_ERROR, L"Lock does not exist.");
	if (sqlrow[0] == NULL || atoi(sqlrow[0]) == 0)
		lplog(LOG_ERROR, L"Lock was never acquired.");
	mysql_free_result(result);
}
#endif

bool checkFull(MYSQL* mysql, wchar_t* qt, size_t& len, bool flush, wchar_t* qualifier)
{
	LFS
		bool ret = true;
	if (len > QUERY_BUFFER_LEN_UNDERFLOW || (flush && qt[len - 1] == L','))
	{
		if (qt[len - 2] == L')')
			qt[--len] = 0; // must be INSERT strip off extra ,
		else
			qt[len - 1] = L')'; // must be IN - put in ending )
		if (qualifier != NULL)
		{
			wcscpy(qt + len, qualifier);
			len += wcslen(qualifier);
		}
		qt[len] = 0;
		if (!myquery(mysql, qt, false))
		{
			lplog(L"checkFull %S (len=%d): ", mysql_error(mysql), len);
			wcscat(qt, L"\n");
			myquery(mysql, L"UNLOCK TABLES");
			logstring(LOG_INFO | LOG_FATAL_ERROR, qt);
		}
		wchar_t* ch = wcsstr(qt, L"VALUES");
		if (ch)
		{
			ch[7] = 0; // must be INSERT
			len = ch + 7 - qt;
		}
		else if (ch = wcschr(qt, '('))
		{
			ch[1] = 0; // must be IN
			len = ch + 1 - qt;
		}
		else
		{
			myquery(mysql, L"UNLOCK TABLES");
			lplog(LOG_FATAL_ERROR, L"Incorrect command %s given to checkFull", qt);
		}
	}
	return ret;
}

unsigned long encodeEscape(MYSQL& mysql, wstring& to, wstring from)
{
	LFS
		string sFrom;
	wTM(from, sFrom);
	// mysql_real_escape_string can write up to 2*len+1 bytes; the old fixed 1024 buffer overran.
	vector <char> tmp(sFrom.length() * 2 + 1);
	unsigned long len = mysql_real_escape_string(&mysql, tmp.data(), sFrom.c_str(), sFrom.length());
	mTW(tmp.data(), to);
	return len;
}

// Escape everything MySQL treats specially inside a quoted literal, so the result is
// safe in both '...' and "..." contexts.  NUL is escaped as \0 rather than dropped.
void escapeStr(wstring& str)
{
	LFS
		wstring ess;
	ess.reserve(str.length());
	for (unsigned int I = 0; I < str.length(); I++)
	{
		switch (str[I])
		{
		case L'\'': ess += L"\\'"; break;
		case L'"': ess += L"\\\""; break;
		case L'\\': ess += L"\\\\"; break;
		case L'\n': ess += L"\\n"; break;
		case L'\r': ess += L"\\r"; break;
		case L'\032': ess += L"\\Z"; break;
		case L'\0': ess += L"\\0"; break;
		default: ess += str[I]; break;
		}
	}
	str = ess;
}

// escapeStr for a value used inline: returns the escaped copy and leaves the input alone.
wstring escaped(const wstring& str)
{
	wstring copy(str);
	escapeStr(copy);
	return copy;
}

void cWord::generateFormStatistics(void)
{
	LFS
		int* formsCount = (int*)tmalloc(Forms.size() * sizeof(int));
	bool* formAlone = (bool*)tmalloc(Forms.size() * sizeof(bool));
	int unknownAlwaysCapitalized = 0;
	unsigned int f;
	for (f = 0; f < Forms.size(); f++)
	{
		formsCount[f] = 0;
		formAlone[f] = true;
	}
	tIWMM iWord, wordEnd = WMM.end();
	for (iWord = WMM.begin(); iWord != wordEnd; iWord++)
	{
		if (iWord->second.forms()[0] == UNDEFINED_FORM_NUM)
		{
			formsCount[UNDEFINED_FORM_NUM]++;
			lplog(L"%s", iWord->first.c_str());
		}
		else
			for (unsigned int I = 0; I < iWord->second.formsSize(); I++)
			{
				formsCount[iWord->second.forms()[I]]++;
				if (iWord->second.formsSize() > 1) formAlone[iWord->second.forms()[I]] = false;
			}
	}
	int numFormAlone = 0, numFormSingle = 0;
	lplog(L"%30s: %6s %s", "FORM NAME", "COUNT", "ALWAYS ALONE");
	for (f = 0; f < (signed)Forms.size(); f++)
	{
		lplog(L"%30s: %6d %s", Forms[f]->name.c_str(), formsCount[f], (formAlone[f]) ? L"true" : L"false");
		if (formAlone[f] == true) numFormAlone++;
		if (formsCount[f] == 1) numFormSingle++;
	}
	lplog(L"# forms = %d.", Forms.size());
	lplog(L"Forms alone=%d.\nForms having one word=%d.\n# unknown Personal Nouns=%d.\n", numFormAlone, numFormSingle, unknownAlwaysCapitalized);
	lplog(L"# total words=%d.", WMM.size());
	tfree(Forms.size() * sizeof(int), formsCount);
	tfree(Forms.size() * sizeof(bool), formAlone);
}

