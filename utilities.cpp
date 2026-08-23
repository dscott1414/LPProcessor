/*
	utilities.cpp - leaf-level helpers shared by every stage of the parser: number/string
	formatting, wide<->multibyte conversion, and the binary (de)serialization primitives
	used by the word/object/pattern caches.

	Overview:
		Four unrelated groups of helpers live here:
		1) itos/dtos - format an int/double into a caller-supplied wstring using a
		   1024-wchar stack scratch buffer (no bounds checks - see gotchas).
		2) wTM / mTW / mTWCodePage - wchar_t <-> char conversion around the Win32
		   WideCharToMultiByte / MultiByteToWideChar APIs.  Each direction owns one
		   __declspec(thread) scratch buffer that grows monotonically and is never
		   freed, so conversions are allocation-free in steady state.  mTW also
		   performs encoding *detection* (UTF-8 -> ISO-8859-1 -> CP1252 -> US-ASCII)
		   which is what source.cpp/tokenize.cpp rely on to read Gutenberg texts.
		3) copy(...) - a large overload family implementing a flat, untagged binary
		   format used to write and re-read the caches (wordCache / object caches /
		   ontology).  Every overload threads a byte offset `where` and a buffer size
		   `limit`.  Argument order picks the direction:
		       copy(buf, value, where, limit)  == SERIALIZE (write value into buf)
		       copy(value, buf, where, limit)  == DESERIALIZE (read value out of buf)
		   The two directions must stay exactly symmetric: a writer that emits a
		   different number of bytes than its reader consumes silently corrupts
		   everything that follows it in the stream.
		4) small wstring janitors (escapeSingleQuote/trim/removeExcessSpaces/
		   splitString) plus the debug-only tag-annotation printer used to dump a
		   sentence with its matched SUBJECT/VERB/OBJECT tag brackets.

	Pipeline position:
		Not a stage; called from all of them.  wTM is on the hot path of every MySQL
		query (DB.cpp/DBUtility.cpp), mTW is used when reading the source document
		(stage 2), and the copy() family is used whenever a cache is written or read.

	Key entry points:
		- itos()/dtos() - int/double to wstring
		- wTM() - wstring -> string (default CP_UTF8)
		- mTW() - string -> wstring with encoding auto-detection
		- mTWCodePage() - string -> wstring with a forced code page, returns 0 on error
		- copy() - serialize/deserialize scalars, strings, sets, vectors, cName, cIntArray
		- escapeSingleQuote()/removeSingleQuote()/removeExcessSpaces()/trim()
		- getSentenceWithTags() - debug rendering of a sentence with tag brackets

	Key data structures / globals:
		- wTMbuffer/wTMbufSize - per-thread wide->multibyte scratch buffer, grow-only,
		  never freed.  Only valid until the next wTM() call on the same thread.
		- mTWbuffer/mTWbufSize - per-thread multibyte->wide scratch buffer, shared by
		  mTW() and mTWCodePage(), grow-only, never freed, minimum 1MB once grown.

	Dependencies:
		Win32 (WideCharToMultiByte/MultiByteToWideChar/FormatMessage/_heapchk),
		tmalloc/trealloc from memoryStat.cpp, lplog from logging.cpp, Words (word.h)
		for copyWString, cSource/pattern tag machinery for getSentenceWithTags.

	Notes / gotchas:
		- LFS/DLFS at the top of most functions are the profiler macros from profile.h;
		  they expand to nothing unless PROFILE/PROFILEDETAIL is defined, which is why
		  the first statement of each body looks over-indented.
		- Everything returned by wTM/mTW/mTWCodePage points either into the caller's
		  own out-string or into the per-thread scratch buffer.  Never hold such a
		  pointer across another conversion call on the same thread.
		- The serializing copy() overloads for scalars write FIRST and check `limit`
		  AFTER, so they detect an overrun only once it has already happened; the
		  deserializing string overloads likewise read the string before validating
		  `limit`.  Treat `limit` as a diagnostic, not as protection against a
		  truncated or corrupt cache file.
		- Most copy() overloads never actually return false: they call
		  lplog(LOG_FATAL_ERROR,...), and logstring() calls exit(0) for that level.
		  So a "return false" path in a caller is mostly unreachable.
		- itos()/dtos() use fixed 1024-wchar stack buffers with unbounded wcscpy/wcscat/
		  wsprintf; callers must keep prefixes, suffixes and formats short.
*/
#include <windows.h>
#define _WINSOCKAPI_ /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include <fcntl.h>
#include "mysql.h"
#include <direct.h>
#include <sys/stat.h>
#include <crtdbg.h>
#include "profile.h"
#include <sstream>

// Append "before" + decimal(i) + "after" to concat.
// concat is appended to, not overwritten - used to build up log/SQL text piecewise.
// No bounds checking: before+digits+after must stay under 1024 wchar_t.
void itos(const wchar_t* before, int i, wstring& concat, wchar_t* after)
{
	LFS
		wchar_t temp[1024];
	wcscpy(temp, before);
	_itow(i, temp + wcslen(temp), 10);
	wcscat(temp, after);
	concat += temp;
}

// As above, but the suffix is a wstring so only "before"+digits go through the
// fixed 1024-wchar scratch buffer; "after" is concatenated safely.
void itos(const wchar_t* before, int i, wstring& concat, wstring after)
{
	LFS
		wchar_t temp[1024];
	wcscpy(temp, before);
	_itow(i, temp + wcslen(temp), 10);
	concat += temp + after;
}

// Decimal-render i into the caller-owned scratch string tmp and return it.
// tmp exists purely so the result can be used inline (e.g. in a wstring concat or
// as a %s argument) without a dangling temporary.
wstring itos(int i, wstring& tmp)
{
	LFS
		wchar_t temp[1024];
	_itow(i, temp, 10);
	return tmp = temp;
}

// Render i using a caller-supplied printf format (e.g. L"%03d") into tmp.
// Uses wsprintf, which has no size limit argument, so a format that expands past
// 1024 wchar_t overruns the stack buffer.
wstring itos(int i, const wchar_t* format, wstring& tmp)
{
	LFS
		wchar_t temp[1024];
	wsprintf(temp, format, i);
	return tmp = temp;
}

// Render a double into tmp with the fixed format L"%4.2g" (2 significant digits).
// Used for costs/confidences in logs, so precision is deliberately low.
wstring dtos(double fl, wstring& tmp)
{
	LFS
		wchar_t ctmp[1024];
	swprintf(ctmp, 1024, L"%4.2g", fl);
	return tmp = ctmp;
}

// Per-thread grow-only scratch buffer for wide->multibyte conversion.  Never freed;
// its contents are only meaningful until the next wTM() call on the same thread.
__declspec(thread) static void* wTMbuffer = NULL;
__declspec(thread) static unsigned int wTMbufSize = 0;
// Wide -> multibyte conversion (default CP_UTF8).  This is the funnel every wide SQL
// statement passes through before being handed to libmysql, hence the "sql request"
// wording in the error messages.
// The result is copied into outString and the returned pointer aliases outString's
// own storage, so it stays valid as long as outString does.
// Sizing protocol: the first WideCharToMultiByte call passes the current buffer size;
// when the buffer does not exist yet (wTMbufSize==0) that call is a pure size query
// (cbMultiByte==0) and returns the required byte count, which is then doubled and
// allocated.  On a later call with an existing but too-small buffer the API instead
// returns 0/ERROR_INSUFFICIENT_BUFFER, which is the `if (!queryLength)` path below.
// Any conversion failure is fatal (lplog(LOG_FATAL_ERROR) exits the process).
char* wTM(wstring inString, string& outString, int codePage)
{
	LFS
		int queryLength = WideCharToMultiByte(codePage, 0, inString.c_str(), -1, (LPSTR)wTMbuffer, wTMbufSize, NULL, NULL);
	if (wTMbufSize == 0)
	{
		wTMbufSize = max(queryLength * 2, 10000);
		wTMbuffer = tmalloc(wTMbufSize);
		if (!WideCharToMultiByte(codePage, 0, inString.c_str(), -1, (LPSTR)wTMbuffer, wTMbufSize, NULL, NULL))
			lplog(LOG_FATAL_ERROR, L"Error in translating sql request: %s", inString.c_str());
	}
	if (!queryLength)
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			lplog(LOG_FATAL_ERROR, L"Error in translating sql request: %s", inString.c_str());
		queryLength = WideCharToMultiByte(codePage, 0, inString.c_str(), -1, NULL, 0, NULL, NULL);
		unsigned int previousBufSize = wTMbufSize;
		wTMbufSize = queryLength * 2;
		if (!(wTMbuffer = trealloc(23, wTMbuffer, previousBufSize, wTMbufSize)))
			lplog(LOG_FATAL_ERROR, L"Out of memory requesting %d bytes from sql query %s!", wTMbufSize, inString.c_str());
		if (!WideCharToMultiByte(codePage, 0, inString.c_str(), -1, (LPSTR)wTMbuffer, wTMbufSize, NULL, NULL))
			lplog(LOG_FATAL_ERROR, L"Error in translating sql request: %s", inString.c_str());
	}
	if (wTMbuffer)
		outString = (char*)wTMbuffer;
	return (char*)outString.c_str();
}

// Per-thread grow-only scratch buffer for multibyte->wide conversion, shared by
// mTW() and mTWCodePage().  Never freed; once grown it is at least 1MB.
__declspec(thread) static void* mTWbuffer = NULL;
__declspec(thread) static unsigned int mTWbufSize = 0;
// Multibyte -> wide conversion WITH encoding auto-detection.  This is how a raw
// Gutenberg byte stream (or any 8-bit DB/web payload) becomes wchar_t text.
// codepage is an out-parameter: the code page that actually decoded the input, tried
// in order UTF-8 -> 28591 (ISO-8859-1) -> 1252 (Windows Western) -> 20127 (US-ASCII).
// MB_ERR_INVALID_CHARS is what makes each attempt fail (ERROR_NO_UNICODE_TRANSLATION)
// on byte sequences that are illegal in that encoding, which is the whole detection
// mechanism.
// iso8859ControlCharactersFound is set true only when the input decoded as 8859-1 but
// contained bytes 128-159; those are legal-but-control in 8859-1 and printable in
// 1252, so 1252 is preferred.  NOTE it is never set to false here, so it behaves as an
// in/out flag - the caller must initialise it (tokenize.cpp does).
// Returns outString.c_str(); a failure to decode under all four code pages is fatal.
const wchar_t* mTW(string inString, wstring& outString, int& codepage, bool& iso8859ControlCharactersFound)
{
	LFS
		codepage = CP_UTF8;
	int queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, 0);
	if (!queryLength && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
	{
		codepage = 28591; // iso-8859-1	ISO 8859-1 Latin 1; Western European (ISO)
		queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, 0);
		if (!queryLength && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
		{
			codepage = 1252; // ANSI Latin 1; Western European (Windows)
			queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, 0);
			if (!queryLength && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
			{
				codepage = 20127; // ASCII: ISO-646-US (US-ASCII), ASCII, US-ASCII  // US-ASCII (7-bit)
				queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, 0);
			}
		}
		else
		{
			// scan text for control characters 128-159.  If they exist in the text, force to 1252, because in 8859 they are legal but they are control characters.
			int tempIndex = 0;
			for (char ch : inString)
			{
				if (iso8859ControlCharactersFound = (((unsigned char)ch) >= 128 && ((unsigned char)ch) <= 159))
					break;
				tempIndex++;
			}
			if (iso8859ControlCharactersFound)
			{
				int tempQueryLength = MultiByteToWideChar(1252, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, 0);
				if (tempQueryLength > 0)
				{
					codepage = 1252;
					queryLength = tempQueryLength;
				}
			}
		}
	}
	if (!queryLength)
		lplog(LOG_FATAL_ERROR, L"Error (2) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
	// queryLength is a wchar_t count (already including the terminator, because
	// cbMultiByte was -1); <<1 converts it to bytes, +1 is slack.
	unsigned int desiredBufferSizeInBytes = (queryLength + 1) << 1;
	if (mTWbufSize < desiredBufferSizeInBytes)
	{
		desiredBufferSizeInBytes = max(desiredBufferSizeInBytes, 1000000); // make minimum buffer 1MB to avoid repeatedly reallocating for trivially small sizes
		unsigned int previousBufSize = mTWbufSize;
		mTWbufSize = max(desiredBufferSizeInBytes, mTWbufSize);
		mTWbuffer = (previousBufSize == 0) ? tmalloc(mTWbufSize) : trealloc(24, mTWbuffer, previousBufSize, mTWbufSize);
		if (!mTWbuffer)
			lplog(LOG_FATAL_ERROR, L"Out of memory requesting %d bytes from translating buffer %S!", mTWbufSize, inString.c_str());
	}
	// second pass actually decodes; mTWbufSize is bytes so /2 gives the wchar_t capacity
	if (!(queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, mTWbufSize / 2)))
		lplog(LOG_FATAL_ERROR, L"Error (3) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
	outString = (wchar_t*)mTWbuffer;
	return outString.c_str();
}

// Convenience overload for callers that want the detected code page but do not care
// about the 8859-vs-1252 control character disambiguation.
const wchar_t* mTW(string inString, wstring& outString, int& codepage)
{
	bool iso8859ControlCharactersFound;
	return mTW(inString, outString, codepage, iso8859ControlCharactersFound);
}

// Multibyte -> wide with a FORCED code page and no detection: used when the document
// itself declares its encoding and tokenize.cpp decides to re-decode (reDecodeNecessary).
// Unlike mTW() this is non-fatal.  Returns 0 and sets error to
//   -1 the input is not valid in `codepage` (size query failed)
//   -2 the scratch buffer could not be grown
//   -3 the decode pass failed even though the size query succeeded
// On success returns outString.c_str() and leaves `error` untouched, so the caller must
// initialise it.  Shares mTWbuffer with mTW(), so it invalidates any pointer previously
// returned by mTW() on this thread.
const wchar_t* mTWCodePage(string inString, wstring& outString, int codepage, int& error)
{
	LFS
		int queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, 0);
	if (!queryLength)
	{
		lplog(LOG_ERROR, L"Error (mTWCodePage) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
		error = -1;
		return 0;
	}
	unsigned int desiredBufferSizeInBytes = (queryLength + 1) << 1;
	if (mTWbufSize < desiredBufferSizeInBytes)
	{
		desiredBufferSizeInBytes = max(desiredBufferSizeInBytes, 1000000); // make minimum buffer 1MB to avoid repeatedly reallocating for trivially small sizes
		unsigned int previousBufSize = mTWbufSize;
		mTWbufSize = max(desiredBufferSizeInBytes, mTWbufSize);
		mTWbuffer = (previousBufSize == 0) ? tmalloc(mTWbufSize) : trealloc(24, mTWbuffer, previousBufSize, mTWbufSize);
		if (!mTWbuffer)
		{
			lplog(LOG_ERROR, L"Out of memory requesting %d bytes from translating buffer %S!", mTWbufSize, inString.c_str());
			error = -2;
			return 0;
		}
	}
	if (!(queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t*)mTWbuffer, mTWbufSize / 2)))
	{
		lplog(LOG_ERROR, L"Error (mTWCodePage 2) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
		error = -3;
		return 0;
	}
	outString = (wchar_t*)mTWbuffer;
	return outString.c_str();
}

// Simplest overload: decode with auto-detection and discard which code page won.
const wchar_t* mTW(string inString, wstring& outString)
{
	int codepage;
	return mTW(inString, outString, codepage);
}

// ---------------------------------------------------------------------------------
// SERIALIZERS: copy(buf, value, where, limit) writes `value` into buf at byte offset
// `where` and advances `where` past it.  `limit` is the total size of buf in bytes.
// Every one of these has an exact counterpart in the DESERIALIZER block further down;
// the byte counts must match or the rest of the stream is misinterpreted.
// ---------------------------------------------------------------------------------

// Write a wstring as raw UTF-16 code units terminated by a wide NUL.
// This is the only serializer that reports an overrun by returning false instead of
// killing the process, which is why the container serializers below can meaningfully
// propagate a false result.
bool copy(void* buf, wstring str, int& where, int limit)
{
	DLFS
		if (where + (str.length() + 1) * sizeof(str[0]) > (unsigned)limit)
		{
			lplog(LOG_ERROR, L"Maximum copy limit of %d bytes reached (5)!", limit);
			return false;
		}
	wcscpy((wchar_t*)(((char*)buf) + where), str.c_str());
	((char*)buf)[where + wcslen(str.c_str()) * sizeof(str[0])] = 0;
	where += (wcslen(str.c_str()) + 1) * sizeof(str[0]);
	return true;
}

// Write a narrow string plus its NUL.  Not declared in general.h, so it is only
// reachable from this translation unit (the set/vector<string> serializers below).
// An overrun here is fatal rather than a false return.
bool copy(void* buf, string str, int& where, int limit)
{
	DLFS
		if (where + (str.length() + 1) * sizeof(str[0]) > (unsigned)limit)
			lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (6)", limit);
	strcpy((((char*)buf) + where), str.c_str());
	((char*)buf)[where + str.length() * sizeof(str[0])] = 0;
	where += (str.length() + 1) * sizeof(str[0]);
	return true;
}

// Write a 4-byte int in native byte order and alignment-agnostic fashion (the cast
// through char* means `where` need not be a multiple of 4).
// Beware: the limit test happens AFTER the store, so an overrun is reported only once
// the 4 bytes have already been written past the end of buf.
bool copy(void* buf, int num, int& where, int limit)
{
	DLFS
		* ((int*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (7)", limit);
	return true;
}

// Write a 2-byte short.  Same store-then-check ordering as the int overload.
bool copy(void* buf, short num, int& where, int limit)
{
	DLFS
		* ((short*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (8)", limit);
	return true;
}

// Write a 2-byte unsigned short.  Same store-then-check ordering as the int overload.
bool copy(void* buf, unsigned short num, int& where, int limit)
{
	DLFS
		* ((unsigned short*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (9)", limit);
	return true;
}

// Write a 4-byte unsigned int.  Same store-then-check ordering as the int overload.
bool copy(void* buf, unsigned int num, int& where, int limit)
{
	DLFS
		* ((unsigned int*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (10)", limit);
	return true;
}

// Write an 8-byte signed integer.  Same store-then-check ordering as the int overload.
bool copy(void* buf, __int64 num, int& where, int limit)
{
	DLFS
		* ((__int64*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (11)", limit);
	return true;
}

// Write an 8-byte unsigned integer.  Same store-then-check ordering as above.
bool copy(void* buf, unsigned __int64 num, int& where, int limit)
{
	DLFS
		* ((unsigned __int64*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (12)", limit);
	return true;
}

// Write a single byte.  `where` is post-incremented before the limit test, so when
// where==limit on entry this stores one byte past the end of buf and only then reports.
bool copy(void* buf, char ch, int& where, int limit)
{
	DLFS
	((char*)buf)[where++] = ch;
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (13)", limit);
	return true;
}

// Write a single byte (unsigned flavour, identical layout to the char overload; the
// duplicated "(13)" tag in the error message makes the two indistinguishable in logs).
bool copy(void* buf, unsigned char ch, int& where, int limit)
{
	DLFS
	((char*)buf)[where++] = ch;
	if (where > limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (13)", limit);
	return true;
}

// Write a set<int> as [int count][count x int], in the set's sorted order.
// Returns false as soon as any element write fails, leaving `where` partway through -
// callers must treat a false return as "the whole buffer is now unusable".
bool copy(void* buf, set <int>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (set<int>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit)) return false;
	return true;
}

// Write a vector<int> as [int count][count x int], preserving order.
bool copy(void* buf, vector <int>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (vector<int>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit)) return false;
	return true;
}

// Write a vector<wstring> as [int count][count x NUL-terminated UTF-16], in order.
bool copy(void* buf, vector <wstring>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (vector<wstring>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit)) return false;
	return true;
}

// Write a vector<string> as [int count][count x NUL-terminated bytes], in order.
bool copy(void* buf, vector <string>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (vector<string>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit)) return false;
	return true;
}

// Write a set<string> as [int count][count x NUL-terminated bytes], sorted order.
bool copy(void* buf, set <string>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (set<string>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit)) return false;
	return true;
}

bool copy(void* buf, unordered_set <string>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (auto is : s)
		if (!copy(buf, is, where, limit)) return false;
	return true;
}

bool copy(void* buf, set <wstring>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (set<wstring>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit)) return false;
	return true;
}

bool copy(void* buf, unordered_set <wstring>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (auto is : s)
		if (!copy(buf, is, where, limit)) return false;
	return true;
}

bool copy(void* buf, cIntArray& a, int& where, int limit)
{
	DLFS
		if (where + sizeof(a.size()) + a.size() * sizeof(int) > (unsigned)limit)
			lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (16)", limit);
	*((int*)(((char*)buf) + where)) = a.size();
	where += sizeof(a.size());
	memcpy(((char*)buf) + where, a.begin(), a.size() * sizeof(int));
	where += a.size() * sizeof(int);
	return true;
}

bool copy(void* buf, cLastVerbTenses& a, int& where, int limit)
{
	DLFS
		return copy(buf, a.lastTense, where, limit) && copy(buf, a.lastVerb, where, limit);
}

// copy only the first element
bool copyWString(void* buf, tIWMM w, int& where, int limit)
{
	DLFS
		return copy(buf, (w == wNULL) ? L"" : w->first, where, limit);
}

bool copy(void* buf, cName& a, int& where, int limit)
{
	DLFS
		//int begin=where;
		if (!copy(buf, a.nickName, where, limit)) return false;
	if (!copyWString(buf, a.hon, where, limit)) return false;
	if (!copyWString(buf, a.hon2, where, limit)) return false;
	if (!copyWString(buf, a.hon3, where, limit)) return false;
	if (!copyWString(buf, a.first, where, limit)) return false;
	if (!copyWString(buf, a.middle, where, limit)) return false;
	if (!copyWString(buf, a.middle2, where, limit)) return false;
	if (!copyWString(buf, a.last, where, limit)) return false;
	if (!copyWString(buf, a.suffix, where, limit)) return false;
	if (!copyWString(buf, a.any, where, limit)) return false;
	return true;
}

bool copy(string& str, void* buf, int& where, int limit)
{
	DLFS
		str = (((char*)buf) + where);
	if (where + (str.length() + 1) * sizeof(str[0]) > (unsigned)limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (14)", limit);
	where += (str.length() + 1) * sizeof(str[0]);
	return true;
}

bool copy(wstring& str, void* buf, int& where, int limit)
{
	DLFS
		str = (wchar_t*)((char*)buf + where);
	if (where + (str.length() + 1) * sizeof(str[0]) > (unsigned)limit)
		lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached! (15)", limit);
	where += (str.length() + 1) * sizeof(str[0]);
	return true;
}

bool copy(const wchar_t* wcstr, void* buf, int& where, int limit)
{
	DLFS
		wstring wstr = wcstr;
	return copy(wstr, buf, where, limit);
}

bool copy(int& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((int*)(((char*)buf) + where));
	where += sizeof(num);
	return true;
}

bool copy(unsigned int& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((unsigned int*)(((char*)buf) + where));
	where += sizeof(num);
	return true;
}

bool copy(__int64& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((__int64*)(((char*)buf) + where));
	where += sizeof(num);
	return true;
}

bool copy(unsigned __int64& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((unsigned __int64*)(((char*)buf) + where));
	where += sizeof(num);
	return true;
}

bool copy(short& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((short*)(((char*)buf) + where));
	where += sizeof(num);
	return true;
}

bool copy(unsigned short& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((unsigned short*)(((char*)buf) + where));
	where += sizeof(num);
	return true;
}

bool copy(char& ch, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(ch) > limit) return false;
	ch = ((char*)buf)[where++];
	return true;
}

bool copy(unsigned char& ch, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(ch) > limit) return false;
	ch = ((char*)buf)[where++];
	return true;
}

bool copy(set <int>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		int i;
		if (!copy(i, buf, where, limit)) return false;
		s.insert(i);
	}
	return true;
}

bool copy(vector <int>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	s.reserve(count);
	for (int I = 0; I < count; I++)
	{
		int i;
		if (!copy(i, buf, where, limit)) return false;
		s.push_back(i);
	}
	return true;
}

bool copy(vector <wstring>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count<0 || count>(limit - where) / 2)
	{
		lplog(LOG_FATAL_ERROR, L"illegal count on read - %d!", count);
		return false;
	}
	s.reserve(count);
	for (int I = 0; I < count; I++)
	{
		wstring si;
		if (!copy(si, buf, where, limit)) return false;
		s.push_back(si);
	}
	return true;
}

bool copy(vector <string>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	s.reserve(count);
	for (int I = 0; I < count; I++)
	{
		string si;
		if (!copy(si, buf, where, limit)) return false;
		s.push_back(si);
	}
	return true;
}

bool copy(set <string>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		string str;
		if (!copy(str, buf, where, limit)) return false;
		s.insert(str);
	}
	return true;
}

bool copy(set <wstring>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		wstring str;
		if (!copy(str, buf, where, limit)) return false;
		s.insert(str);
	}
	return true;
}

bool copy(unordered_set <wstring>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		wstring str;
		if (!copy(str, buf, where, limit)) return false;
		s.insert(str);
	}
	return true;
}

bool copy(cIntArray& a, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(int) > limit) return false;
	int num = *((int*)(((char*)buf) + where));
	where += sizeof(num);
	if ((where + num * (int)sizeof(int)) > limit) return false;
	if (num < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	for (int I = 0; I < num; I++, where += sizeof(int))
		a.push_back(*((int*)(((char*)buf) + where)));
	return true;
}

bool copy(cLastVerbTenses& a, void* buf, int& where, int limit)
{
	DLFS
		if (!copy(a.lastTense, buf, where, limit)) return false;
	if (!copy(a.lastVerb, buf, where, limit)) return false;
	return true;
}

bool copyWString(tIWMM& w, void* buf, int& where, int limit)
{
	DLFS
		wstring str;
	if (!copy(str, buf, where, limit)) return false;
	if (str.empty())
		w = wNULL;
	else
	{
		w = Words.query(str);
		if (w == Words.end())
		{
			while (str.length() > 0 && str[str.length() - 1] == L' ')
				str.erase(str.begin() + str.length() - 1);
			w = Words.query(str);
			if (w == Words.end())
			{
				::lplog(LOG_ERROR, L"word %s not found in creating read in object name.", str.c_str());
				w = wNULL;
			}
		}
	}
	return true;
}

bool copy(cName& a, void* buf, int& where, int limit)
{
	DLFS
		//int begin=where;
		if (!copy(a.nickName, buf, where, limit)) return false;
	if (!copyWString(a.hon, buf, where, limit)) return false;
	if (!copyWString(a.hon2, buf, where, limit)) return false;
	if (!copyWString(a.hon3, buf, where, limit)) return false;
	if (!copyWString(a.first, buf, where, limit)) return false;
	if (!copyWString(a.middle, buf, where, limit)) return false;
	if (!copyWString(a.middle2, buf, where, limit)) return false;
	if (!copyWString(a.last, buf, where, limit)) return false;
	if (!copyWString(a.suffix, buf, where, limit)) return false;
	if (!copyWString(a.any, buf, where, limit)) return false;
	return true;
}

// Function to lookup an error message from an error code.
const char* LastErrorStr(void)
{
	LFS
		char* szReturn = NULL;
	int hr = GetLastError();
	if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, hr, GetUserDefaultLangID(), (CHAR*)&szReturn, 0, NULL) == 0)
		return "Unknown";

	return szReturn;
}

void checkHeap(wchar_t* desc)
{
	LFS
		// Check heap status
		int heapstatus = _heapchk();
	switch (heapstatus)
	{
	case _HEAPOK:
		break;
	case _HEAPEMPTY:
		printf(" OK - %ls heap is empty\n", desc);
		break;
	case _HEAPBADBEGIN:
		printf("ERROR - %ls bad start of heap\n", desc);
		break;
	case _HEAPBADNODE:
		printf("ERROR - %ls bad node in heap\n", desc);
		break;
	}
	if (!_CrtCheckMemory())
		printf("ERROR - %ls memory is bad", desc);
}

void escapeSingleQuote(wstring& lobject)
{
	LFS
		wstring slo2;
	for (unsigned int I = 0; I < lobject.size(); I++)
		if (lobject[I] == L'\'')
			slo2 += L"\\'";
		else
			slo2 += lobject[I];
	lobject = slo2;
}

void removeSingleQuote(wstring& lobject)
{
	LFS
		wstring slo2;
	for (unsigned int I = 0; I < lobject.size() - 1; I++)
		if (lobject[I] != L'\\' || lobject[I + 1] != L'\'')
			slo2 += lobject[I];
		else
			I++;
	if (lobject[lobject.size() - 1] != L'\'')
		slo2 += lobject[lobject.size() - 1];
	lobject = slo2;
}

void removeExcessSpaces(wstring& lobject)
{
	LFS
		wstring slo2;
	for (unsigned int I = 0; I < lobject.size(); I++)
		if (lobject[I] != L' ' || I == 0 || lobject[I - 1] != ' ')
			slo2 += lobject[I];
	lobject = slo2;
}

void trim(wstring& str)
{
	LFS
		int whereSpace = 0;
	while (str[whereSpace] == L' ')
		whereSpace++;
	if (whereSpace > 0)
		str.erase(0, whereSpace);
	whereSpace = str.length() - 1;
	while (whereSpace >= 0 && str[whereSpace] == L' ')
		whereSpace--;
	if (whereSpace < str.length() - 1)
		str.erase(whereSpace + 1);
}

vector<wstring> splitString(wstring str, wchar_t wc)
{
	vector<wstring> strings;
	std::wistringstream f(str);
	wstring s;
	while (std::getline(f, s, wc))
		strings.push_back(s);
	return strings;
}


int addTagMark(wstring tag, vector<cTagLocation> tagSet, map <int, set<wstring>>& tagBeginPositionMap, map <int, set<wstring>>& tagEndPositionMap)
{
	int nextTagIndex = -1, tagIndex = findTag(tagSet, (wchar_t*)tag.c_str(), nextTagIndex);
	if (tagIndex >= 0)
	{
		tagBeginPositionMap[tagSet[tagIndex].sourcePosition].insert(tag);
		tagEndPositionMap[tagSet[tagIndex].sourcePosition + tagSet[tagIndex].len - 1].insert(tag);
	}
	return tagIndex;
}

void addTagConstrainedMark(wstring tag, int constrainByTag, vector<cTagLocation> tagSet, map <int, set<wstring>>& tagBeginPositionMap, map <int, set<wstring>>& tagEndPositionMap)
{
	int nextConstrainedTagIndex = -1, constrainedTagIndex = (constrainByTag >= 0) ? findTagConstrained(tagSet, (wchar_t*)tag.c_str(), nextConstrainedTagIndex, tagSet[constrainByTag]) : -1;
	if (constrainedTagIndex >= 0)
	{
		tagBeginPositionMap[tagSet[constrainedTagIndex].sourcePosition].insert(tag);
		tagEndPositionMap[tagSet[constrainedTagIndex].sourcePosition + tagSet[constrainedTagIndex].len - 1].insert(tag);
	}
}

void getTagPositionsFromTagSet(vector<cTagLocation> tagSet, map <int, set<wstring>>& tagBeginPositionMap, map <int, set<wstring>>& tagEndPositionMap)
{
	if (addTagMark(L"SUBJECT", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	if (addTagMark(L"VERB", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	if (addTagMark(L"OBJECT", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	int nextMainVerbTag = -1, mainVerbTag = findTag(tagSet, L"VERB", nextMainVerbTag);
	addTagConstrainedMark(L"V_AGREE", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(L"conditional", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(L"past", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(L"future", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
}

void getTagPositions(cSource& source, int position, int pemaByPatternEnd, map <int, set<wstring>>& tagBeginPositionMap, map <int, set<wstring>>& tagEndPositionMap)
{
	vector < vector <cTagLocation> > tagSets;
	if (source.startCollectTags(false, subjectVerbRelationTagSet, position, pemaByPatternEnd, tagSets, true, false, L"tags for debugging") > 0)
	{
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (tagSets[J].size())
				getTagPositionsFromTagSet(tagSets[J], tagBeginPositionMap, tagEndPositionMap);
		}
	}
}

void getSentenceWithTags(cSource& source, int patternBegin, int patternEnd, int sentenceBegin, int sentenceEnd, int PEMAPosition, wstring& sentence)
{
	map <int, set<wstring>> tagBeginPositionMap, tagEndPositionMap;
	getTagPositions(source, patternBegin, PEMAPosition, tagBeginPositionMap, tagEndPositionMap);
	wstring originalIWord;
	bool inPattern = false;
	for (int I = sentenceBegin; I < sentenceEnd; I++)
	{
		source.getOriginalWord(I, originalIWord, false, false);
		if (I == patternBegin)
		{
			sentence += L"**";
			inPattern = true;
		}
		if (tagBeginPositionMap.find(I) != tagBeginPositionMap.end())
		{
			for (wstring tag : tagBeginPositionMap[I])
				sentence += tag + L"{";
		}
		sentence += originalIWord;
		if (tagEndPositionMap.find(I) != tagEndPositionMap.end())
		{
			bool notFoundInBeginPositions = tagBeginPositionMap.find(I) == tagBeginPositionMap.end();
			for (wstring tag : tagEndPositionMap[I])
			{
				if (notFoundInBeginPositions || tagBeginPositionMap[I].find(tag) == tagBeginPositionMap[I].end())
					sentence += L" " + tag;
				sentence += L"}";
			}
		}
		if (I == patternEnd - 1)
		{
			sentence += L"**";
			inPattern = false;
		}
		sentence += L" ";
	}
}

