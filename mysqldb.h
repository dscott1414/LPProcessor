#pragma once
bool myquery(MYSQL *mysql, wchar_t *q, bool allowFailure = false);
bool myquery(MYSQL *mysql, wchar_t *q, MYSQL_RES * &result, bool allowFailure = false);
#define QUERY_BUFFER_LEN (1024*256-1024*2)
#define QUERY_BUFFER_LEN_OVERFLOW QUERY_BUFFER_LEN+1024
#define QUERY_BUFFER_LEN_UNDERFLOW QUERY_BUFFER_LEN-1024
