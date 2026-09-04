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
			exits the process with EXIT_FAILURE via logging.cpp's fatalExit());
			allowFailure==true only logs an error and returns false.

	Notes / gotchas:
		- Buffer convention: declare lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW] but format
			into it with QUERY_BUFFER_LEN as the limit, so the extra 1024 lpchar_t are
			slack for the "one more row than expected" case; checkFull() (DBUtility.cpp)
			flushes an accumulating INSERT/IN list once it passes
			QUERY_BUFFER_LEN_UNDERFLOW, i.e. 1024 lpchar_t before the nominal limit.
		- QUERY_BUFFER_LEN is in lpchar_t units, not bytes: a buffer is ~508KB.
*/
#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
bool myquery(MYSQL *mysql, const lpchar_t *q, bool allowFailure = false);
bool myquery(MYSQL *mysql, const lpchar_t *q, MYSQL_RES * &result, bool allowFailure = false);
#define QUERY_BUFFER_LEN (1024*256-1024*2)
#define QUERY_BUFFER_LEN_OVERFLOW (QUERY_BUFFER_LEN+1024)
#define QUERY_BUFFER_LEN_UNDERFLOW (QUERY_BUFFER_LEN-1024)
