#pragma once
bool myquery(MYSQL *mysql, wchar_t *q, bool allowFailure = false);
bool myquery(MYSQL *mysql, wchar_t *q, MYSQL_RES * &result, bool allowFailure = false);
#define query_buffer_len (1024*256-1024*2)
#define query_buffer_len_overflow query_buffer_len+1024
#define query_buffer_len_underflow query_buffer_len-1024
