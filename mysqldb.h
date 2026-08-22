/*
	mysqldb.h - shared declarations for the MySQL access layer (query entry points + query buffer sizing)

	Overview:
		Every SQL statement in LPProcessor is composed as a wide string (usually in a
		stack buffer) and handed to one of the two myquery() overloads, which translate
		it to UTF-8 and call mysql_real_query().  The three macros below define the
		convention used everywhere for those stack buffers.

	Key entry points:
		- myquery(mysql,q,allowFailure) - execute a statement with no result set.
		- myquery(mysql,q,result,allowFailure) - execute and mysql_store_result() into
			result; caller owns result and must mysql_free_result() it.
			allowFailure==false makes a failing statement fatal (lplog LOG_FATAL_ERROR
			calls exit(0)); allowFailure==true only logs an error and returns false.

	Notes / gotchas:
		- Buffer convention: declare wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW] but format
			into it with QUERY_BUFFER_LEN as the limit, so the extra 1024 wchar_t are
			slack for the "one more row than expected" case; checkFull() (DBUtility.cpp)
			flushes an accumulating INSERT/IN list once it passes
			QUERY_BUFFER_LEN_UNDERFLOW, i.e. 1024 wchar_t before the nominal limit.
		- QUERY_BUFFER_LEN is in wchar_t units, not bytes: a buffer is ~508KB.
		- QUERY_BUFFER_LEN_OVERFLOW/UNDERFLOW are NOT parenthesized, so they must only
			ever be used as a whole expression (e.g. an array bound); writing
			2*QUERY_BUFFER_LEN_OVERFLOW does not mean what it looks like.
*/
#pragma once
bool myquery(MYSQL *mysql, const wchar_t *q, bool allowFailure = false);
bool myquery(MYSQL *mysql, const wchar_t *q, MYSQL_RES * &result, bool allowFailure = false);
#define QUERY_BUFFER_LEN (1024*256-1024*2)
#define QUERY_BUFFER_LEN_OVERFLOW QUERY_BUFFER_LEN+1024
#define QUERY_BUFFER_LEN_UNDERFLOW QUERY_BUFFER_LEN-1024
