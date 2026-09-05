/*
	utilities.cpp - leaf-level helpers shared by every stage of the parser: number/string
	formatting, wide<->multibyte conversion, and the binary (de)serialization primitives
	used by the word/object/pattern caches.

	Overview:
		Four unrelated groups of helpers live here:
		1) itos/dtos - format an int/double into a caller-supplied lpwstring using a
		   1024-wchar stack scratch buffer (no bounds checks - see gotchas).
		2) wTM / mTW / mTWCodePage - lpchar_t <-> char conversion around the Win32
		   WideCharToMultiByte / MultiByteToWideChar APIs.  Each direction owns one
		   thread_local scratch buffer that grows monotonically and is never
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
		4) small lpwstring janitors (escapeSingleQuote/trim/removeExcessSpaces/
		   splitString) plus the debug-only tag-annotation printer used to dump a
		   sentence with its matched SUBJECT/VERB/OBJECT tag brackets.

	Pipeline position:
		Not a stage; called from all of them.  wTM is on the hot path of every MySQL
		query (DB.cpp/DBUtility.cpp), mTW is used when reading the source document
		(stage 2), and the copy() family is used whenever a cache is written or read.

	Key entry points:
		- itos()/dtos() - int/double to lpwstring
		- wTM() - lpwstring -> string (default CP_UTF8)
		- mTW() - string -> lpwstring with encoding auto-detection
		- mTWCodePage() - string -> lpwstring with a forced code page, returns 0 on error
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
		- The serializing copy() overloads for scalars check `limit` BEFORE writing, and
		  the deserializing string/lpwstring overloads bound their NUL-terminator scan to
		  `limit - where` before touching `str`, so a truncated or corrupt cache file is
		  caught before any out-of-bounds read or write happens rather than after.
		- Most copy() overloads never actually return false on the FATAL path: they call
		  lplog(LOG_FATAL_ERROR,...), which exits the process (EXIT_FAILURE) and does not
		  return.  So a "return false" written after such a call is mostly unreachable;
		  it is still correct defensive style for the few overloads (the lpwstring
		  serializer, and the bounded string/lpwstring deserializers) that log at
		  LOG_ERROR/LOG_FATAL_ERROR and return false instead.
		- itos()/dtos() build into small fixed-size scratch buffers (a handful of lpchar_t
		  for decimal digits, or an explicit-length lp_snprintf), not into unbounded
		  1024-wchar lp_strcpy/wcscat/lp_wsprintf targets, so a long prefix/suffix/format
		  cannot overrun the stack.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include <fcntl.h>
#include "mysql.h"
#include <sys/stat.h>
#include "profile.h"
#include <sstream>

// Append "before" + decimal(i) + "after" to concat.
// concat is appended to, not overwritten - used to build up log/SQL text piecewise.
// Builds directly into concat (via a small fixed buffer only for the digits, which
// can never exceed 11 lpchar_t for a 32-bit int) instead of the previous lp_strcpy/wcscat
// into a fixed 1024-wchar stack buffer, which overran the stack for any before/after
// combination longer than that.
void itos(const lpchar_t* before, int i, lpwstring& concat, lpchar_t* after)
{
	LFS
		lpchar_t digits[16];
	lp_itow(i, digits);
	concat += before;
	concat += digits;
	concat += after;
}

// As above, but the suffix is already a lpwstring.
void itos(const lpchar_t* before, int i, lpwstring& concat, lpwstring after)
{
	LFS
		lpchar_t digits[16];
	lp_itow(i, digits);
	concat += before;
	concat += digits;
	concat += after;
}

// Decimal-render i into the caller-owned scratch string tmp and return it.
// tmp exists purely so the result can be used inline (e.g. in a lpwstring concat or
// as a %s argument) without a dangling temporary.
lpwstring itos(int i, lpwstring& tmp)
{
	LFS
		lpchar_t temp[1024];
	lp_itow(i, temp);
	return tmp = temp;
}

// Render i using a caller-supplied printf format (e.g. u"%03d") into tmp.
// Uses lp_snprintf with an explicit length instead of the previous unbounded lp_wsprintf,
// so a pathological format/width cannot overrun the stack buffer.
lpwstring itos(int i, const lpchar_t* format, lpwstring& tmp)
{
	LFS
		lpchar_t temp[1024];
	lp_snprintf(temp, 1024, format, i);
	return tmp = temp;
}


// ---------------------------------------------------------------------------
// Batch B7: wTM / mTW / mTWCodePage are now thin wrappers over utfConvert.h's
// codec, replacing Win32 WideCharToMultiByte / MultiByteToWideChar (which have no
// macOS equivalent at all).
//
// The behaviour that matters is preserved exactly, because real cached corpora
// were parsed under it: mTW still tries a STRICT UTF-8 decode first and falls back
// to ISO-8859-1-with-a-Windows-1252-table for bytes 0x80-0x9F, and still reports
// which encoding won through the `codepage` out-parameter using the same numeric
// values (CP_UTF8 / 28591 / 1252) that get stored alongside cached documents.
// See utfConvert.h's header for the full rationale and for why the original
// ladder's nominal 4th rung (20127 / US-ASCII) is unreachable in both versions.
//
// The grow-only thread_local scratch buffers are gone: the codec keeps its own
// (see utfConvert.h's "Notes / gotchas"), so these functions no longer manage
// memory at all. That also removes the four tmalloc/trealloc failure paths and
// the "queryLength is a lpchar_t count, <<1 for bytes" sizing arithmetic, which
// existed only to satisfy the Win32 two-call sizing protocol.
// ---------------------------------------------------------------------------

// Wide -> multibyte. Always UTF-8 now; the codePage argument is accepted for
// source compatibility with the ~90 existing call sites but the only value any of
// them passes is CP_UTF8 or CP_ACP, and CP_ACP ("the system ANSI code page") has no
// macOS meaning -- there is no non-Unicode system encoding to convert to. Both are
// therefore UTF-8, which for CP_ACP's two call sites (Internet.cpp building a local
// cache filename) is what the filesystem wants anyway.
// The result is copied into outString and the returned pointer aliases outString's
// own storage, so it stays valid as long as outString does.
char* wTM(lpwstring inString, string& outString, int codePage)
{
	LFS
		(void)codePage;
	outString = lp_utf16_to_utf8(inString);
	return (char*)outString.c_str();
}

// Multibyte -> wide WITH encoding auto-detection.  This is how a raw Gutenberg byte
// stream (or any 8-bit DB/web payload) becomes lpchar_t text.
// codepage is an out-parameter: the code page that actually decoded the input.
// iso8859ControlCharactersFound is set true only when the input decoded as 8859-1
// but contained bytes 128-159; those are legal-but-control in 8859-1 and printable
// in 1252, so 1252 is preferred.  NOTE it is never set to false here, so it behaves
// as an in/out flag - the caller must initialise it (tokenize.cpp does).
// Returns outString.c_str().  Unlike the Win32 version this cannot fail: the
// fallback rung is total (every byte 0x00-0xFF has a mapping), so the old
// "failure to decode under all four code pages is fatal" path was unreachable and
// is not reproduced.
const lpchar_t* mTW(string inString, lpwstring& outString, int& codepage, bool& iso8859ControlCharactersFound)
{
	LFS
		LpDetectedEncoding detected;
	outString = lp_narrow_to_wide(inString, detected);
	switch (detected)
	{
	case LpDetectedEncoding::UTF8:       codepage = CP_UTF8; break;
	case LpDetectedEncoding::ISO_8859_1: codepage = 28591; break;   // ISO 8859-1 Latin 1; Western European
	case LpDetectedEncoding::CP1252:     codepage = 1252; break;    // ANSI Latin 1; Western European (Windows)
	}
	// The codec's CP1252 rung IS "8859-1 except 0x80-0x9F came from the 1252
	// table", which is precisely the condition this flag reported.
	if (detected == LpDetectedEncoding::CP1252)
		iso8859ControlCharactersFound = true;
	return outString.c_str();
}

// Convenience overload for callers that want the detected code page but do not care
// about the 8859-vs-1252 control character disambiguation.
const lpchar_t* mTW(string inString, lpwstring& outString, int& codepage)
{
	bool iso8859ControlCharactersFound = false;
	return mTW(inString, outString, codepage, iso8859ControlCharactersFound);
}

// Multibyte -> wide with a FORCED code page and no detection: used when the document
// itself declares its encoding and tokenize.cpp decides to re-decode
// (reDecodeNecessary).  Non-fatal.  Returns 0 and sets error to -1 when the input is
// not valid in `codepage`; on success returns outString.c_str() and leaves `error`
// untouched, so the caller must initialise it.
// Batch B7: the -2 (buffer could not be grown) and -3 (decode pass failed after a
// successful size query) returns are no longer reachable -- there is no separate
// buffer to grow and no two-pass protocol -- but they remain declared in the
// contract above so callers that switch on them still compile and behave.
const lpchar_t* mTWCodePage(string inString, lpwstring& outString, int codepage, int& error)
{
	LFS
		if (codepage == CP_UTF8)
		{
			if (!lp_try_decode_strict_utf8(inString, outString))
			{
				lplog(LOG_ERROR, u"Error (mTWCodePage) in translating buffer as UTF-8: %S", inString.c_str());
				error = -1;
				return 0;
			}
			return outString.c_str();
		}
	// Everything else this codebase forces (28591, 1252, 20127) lands on the
	// latin1/cp1252 rung, which is total and cannot fail.
	bool usedCp1252Table = false;
	lp_decode_latin1_cp1252(inString, outString, usedCp1252Table);
	return outString.c_str();
}

// Simplest overload: decode with auto-detection and discard which code page won.
const lpchar_t* mTW(string inString, lpwstring& outString)
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

// Write a lpwstring as raw UTF-16 code units terminated by a wide NUL.
// This is the only serializer that reports an overrun by returning false instead of
// killing the process, which is why the container serializers below can meaningfully
// propagate a false result.
bool copy(void* buf, lpwstring str, int& where, int limit)
{
	DLFS
		if (where + (str.length() + 1) * sizeof(str[0]) > (unsigned)limit)
		{
			lplog(LOG_ERROR, u"Maximum copy limit of %d bytes reached (5)!", limit);
			return false;
		}
	lp_strcpy((lpchar_t*)(((char*)buf) + where), str.c_str());
	((char*)buf)[where + lp_strlen(str.c_str()) * sizeof(str[0])] = 0;
	where += (lp_strlen(str.c_str()) + 1) * sizeof(str[0]);
	return true;
}

// Write a narrow string plus its NUL.  Not declared in general.h, so it is only
// reachable from this translation unit (the set/vector<string> serializers below).
// An overrun here is fatal rather than a false return.
bool copy(void* buf, string str, int& where, int limit)
{
	DLFS
		if (where + (str.length() + 1) * sizeof(str[0]) > (unsigned)limit)
			lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (6)", limit);
	strcpy((((char*)buf) + where), str.c_str());
	((char*)buf)[where + str.length() * sizeof(str[0])] = 0;
	where += (str.length() + 1) * sizeof(str[0]);
	return true;
}

// Write a 4-byte int in native byte order and alignment-agnostic fashion (the cast
// through char* means `where` need not be a multiple of 4).
// The limit test happens BEFORE the store and calls LOG_FATAL_ERROR (which aborts the
// process via fatalExit()), so an overrun is caught before any bytes are written past buf.
bool copy(void* buf, int num, int& where, int limit)
{
	DLFS
	if (where +sizeof(num) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (7)", limit);
	* ((int*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	return true;
}

// Write a 2-byte short.  Same check-before-store ordering as the int overload.
bool copy(void* buf, short num, int& where, int limit)
{
	DLFS
	if (where +sizeof(num) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (7)", limit);
	* ((short*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	return true;
}

// Write a 2-byte unsigned short.  Same check-before-store ordering as the int overload.
bool copy(void* buf, unsigned short num, int& where, int limit)
{
	DLFS
	if (where +sizeof(num) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (9)", limit);
	* ((unsigned short*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	return true;
}

// Write a 4-byte unsigned int.  Same check-before-store ordering as the int overload.
bool copy(void* buf, unsigned int num, int& where, int limit)
{
	DLFS
	if (where +sizeof(num) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (10)", limit);
	* ((unsigned int*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	return true;
}

// Write an 8-byte signed integer.  Same check-before-store ordering as the int overload.
bool copy(void* buf, int64_t num, int& where, int limit)
{
	DLFS
	if (where +sizeof(num) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (11)", limit);
	* ((int64_t*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	return true;
}

// Write an 8-byte unsigned integer.  Same check-before-store ordering as above.
bool copy(void* buf, uint64_t num, int& where, int limit)
{
	DLFS
	if (where +sizeof(num) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (12)", limit);
	* ((uint64_t*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	return true;
}

// Write a single byte.  The limit test runs before the post-incrementing store, and
// LOG_FATAL_ERROR aborts the process, so where==limit on entry is caught, not overrun.
bool copy(void* buf, char ch, int& where, int limit)
{
	DLFS
	if (where +sizeof(ch) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (13)", limit);
	((char*)buf)[where++] = ch;
	return true;
}

// Write a single byte (unsigned flavour, identical layout to the char overload; the
// duplicated "(13)" tag in the error message makes the two indistinguishable in logs).
bool copy(void* buf, unsigned char ch, int& where, int limit)
{
	DLFS
	if (where +sizeof(ch) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (14)", limit);
	((char*)buf)[where++] = ch;
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

// Write a vector<lpwstring> as [int count][count x NUL-terminated UTF-16], in order.
bool copy(void* buf, vector <lpwstring>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (vector<lpwstring>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
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

bool copy(void* buf, set <lpwstring>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (set<lpwstring>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit)) return false;
	return true;
}

bool copy(void* buf, unordered_set <lpwstring>& s, int& where, int limit)
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
			lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (16)", limit);
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
		return copy(buf, (w == wNULL) ? u"" : w->first, where, limit);
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

// Bounded read: unlike the old "str = buf+where; then check limit" shape, this never
// scans past `limit` looking for the terminating NUL, so a truncated/corrupt cache
// file cannot walk this off the end of buf before the overrun is detected.
bool copy(string& str, void* buf, int& where, int limit)
{
	DLFS
		if (where >= limit)
		{
			lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (14)", limit);
			return false;
		}
	const char* start = ((char*)buf) + where;
	size_t maxLen = (size_t)(limit - where);
	size_t len = strnlen(start, maxLen);
	if (len >= maxLen) // no NUL terminator within the remaining buffer
	{
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (14)", limit);
		return false;
	}
	str.assign(start, len);
	where += (int)(len + 1) * sizeof(str[0]);
	return true;
}

bool copy(lpwstring& str, void* buf, int& where, int limit)
{
	DLFS
		if (where >= limit)
		{
			lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (15)", limit);
			return false;
		}
	const lpchar_t* start = (lpchar_t*)((char*)buf + where);
	size_t maxChars = (size_t)(limit - where) / sizeof(lpchar_t);
	size_t len = lp_strnlen(start, maxChars);
	if (len >= maxChars) // no NUL terminator within the remaining buffer
	{
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached! (15)", limit);
		return false;
	}
	str.assign(start, len);
	where += (int)(len + 1) * sizeof(lpchar_t);
	return true;
}

bool copy(const lpchar_t* wcstr, void* buf, int& where, int limit)
{
	DLFS
		lpwstring wstr = wcstr;
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

bool copy(int64_t& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((int64_t*)(((char*)buf) + where));
	where += sizeof(num);
	return true;
}

bool copy(uint64_t& num, void* buf, int& where, int limit)
{
	DLFS
		if (where + (int)sizeof(num) > limit) return false;
	num = *((uint64_t*)(((char*)buf) + where));
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
		lplog(LOG_ERROR, u"negative count on read!");
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
		lplog(LOG_ERROR, u"negative count on read!");
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

bool copy(vector <lpwstring>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count<0 || count>(limit - where) / 2)
	{
		lplog(LOG_FATAL_ERROR, u"illegal count on read - %d!", count);
		return false;
	}
	s.reserve(count);
	for (int I = 0; I < count; I++)
	{
		lpwstring si;
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
		lplog(LOG_ERROR, u"negative count on read!");
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
		lplog(LOG_ERROR, u"negative count on read!");
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

bool copy(set <lpwstring>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, u"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		lpwstring str;
		if (!copy(str, buf, where, limit)) return false;
		s.insert(str);
	}
	return true;
}

bool copy(unordered_set <lpwstring>& s, void* buf, int& where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, u"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		lpwstring str;
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
		lplog(LOG_ERROR, u"negative count on read!");
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
		lpwstring str;
	if (!copy(str, buf, where, limit)) return false;
	if (str.empty())
		w = wNULL;
	else
	{
		w = Words.query(str);
		if (w == Words.end())
		{
			while (str.length() > 0 && str[str.length() - 1] == u' ')
				str.erase(str.begin() + str.length() - 1);
			w = Words.query(str);
			if (w == Words.end())
			{
				::lplog(LOG_ERROR, u"word %s not found in creating read in object name.", str.c_str());
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



void escapeSingleQuote(lpwstring& lobject)
{
	LFS
		lpwstring slo2;
	for (unsigned int I = 0; I < lobject.size(); I++)
		if (lobject[I] == u'\'')
			slo2 += u"\\'";
		else
			slo2 += lobject[I];
	lobject = slo2;
}

void removeSingleQuote(lpwstring& lobject)
{
	LFS
		lpwstring slo2;
	for (unsigned int I = 0; I < lobject.size() - 1; I++)
		if (lobject[I] != u'\\' || lobject[I + 1] != u'\'')
			slo2 += lobject[I];
		else
			I++;
	if (lobject[lobject.size() - 1] != u'\'')
		slo2 += lobject[lobject.size() - 1];
	lobject = slo2;
}

void removeExcessSpaces(lpwstring& lobject)
{
	LFS
		lpwstring slo2;
	for (unsigned int I = 0; I < lobject.size(); I++)
		if (lobject[I] != u' ' || I == 0 || lobject[I - 1] != ' ')
			slo2 += lobject[I];
	lobject = slo2;
}

void trim(lpwstring& str)
{
	LFS
		int whereSpace = 0;
	while (str[whereSpace] == u' ')
		whereSpace++;
	if (whereSpace > 0)
		str.erase(0, whereSpace);
	whereSpace = str.length() - 1;
	while (whereSpace >= 0 && str[whereSpace] == u' ')
		whereSpace--;
	if (whereSpace < str.length() - 1)
		str.erase(whereSpace + 1);
}

vector<lpwstring> splitString(lpwstring str, lpchar_t wc)
{
	// Batch B2: std::wistringstream/std::getline has no reliable char16_t support
	// (no standard ctype<char16_t> locale facet); split manually. Matches
	// std::getline(stream, s, delim)-in-a-loop semantics exactly, including the
	// "no trailing empty entry after a trailing delimiter" and "empty input
	// produces zero entries" edge cases (getline fails, extracting nothing, the
	// instant the read position is already at the end of the string).
	vector<lpwstring> strings;
	size_t start = 0;
	while (start < str.size())
	{
		size_t delimPos = str.find(wc, start);
		if (delimPos == lpwstring::npos)
		{
			strings.push_back(str.substr(start));
			break;
		}
		strings.push_back(str.substr(start, delimPos - start));
		start = delimPos + 1;
	}
	return strings;
}


int addTagMark(lpwstring tag, vector<cTagLocation> tagSet, map <int, set<lpwstring>>& tagBeginPositionMap, map <int, set<lpwstring>>& tagEndPositionMap)
{
	int nextTagIndex = -1, tagIndex = findTag(tagSet, (lpchar_t*)tag.c_str(), nextTagIndex);
	if (tagIndex >= 0)
	{
		tagBeginPositionMap[tagSet[tagIndex].sourcePosition].insert(tag);
		tagEndPositionMap[tagSet[tagIndex].sourcePosition + tagSet[tagIndex].len - 1].insert(tag);
	}
	return tagIndex;
}

void addTagConstrainedMark(lpwstring tag, int constrainByTag, vector<cTagLocation> tagSet, map <int, set<lpwstring>>& tagBeginPositionMap, map <int, set<lpwstring>>& tagEndPositionMap)
{
	int nextConstrainedTagIndex = -1, constrainedTagIndex = (constrainByTag >= 0) ? findTagConstrained(tagSet, (lpchar_t*)tag.c_str(), nextConstrainedTagIndex, tagSet[constrainByTag]) : -1;
	if (constrainedTagIndex >= 0)
	{
		tagBeginPositionMap[tagSet[constrainedTagIndex].sourcePosition].insert(tag);
		tagEndPositionMap[tagSet[constrainedTagIndex].sourcePosition + tagSet[constrainedTagIndex].len - 1].insert(tag);
	}
}

void getTagPositionsFromTagSet(vector<cTagLocation> tagSet, map <int, set<lpwstring>>& tagBeginPositionMap, map <int, set<lpwstring>>& tagEndPositionMap)
{
	if (addTagMark(u"SUBJECT", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	if (addTagMark(u"VERB", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	if (addTagMark(u"OBJECT", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	int nextMainVerbTag = -1, mainVerbTag = findTag(tagSet, u"VERB", nextMainVerbTag);
	addTagConstrainedMark(u"V_AGREE", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(u"conditional", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(u"past", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(u"future", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
}

void getTagPositions(cSource& source, int position, int pemaByPatternEnd, map <int, set<lpwstring>>& tagBeginPositionMap, map <int, set<lpwstring>>& tagEndPositionMap)
{
	vector < vector <cTagLocation> > tagSets;
	if (source.startCollectTags(false, subjectVerbRelationTagSet, position, pemaByPatternEnd, tagSets, true, false, u"tags for debugging") > 0)
	{
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (tagSets[J].size())
				getTagPositionsFromTagSet(tagSets[J], tagBeginPositionMap, tagEndPositionMap);
		}
	}
}

void getSentenceWithTags(cSource& source, int patternBegin, int patternEnd, int sentenceBegin, int sentenceEnd, int PEMAPosition, lpwstring& sentence)
{
	map <int, set<lpwstring>> tagBeginPositionMap, tagEndPositionMap;
	getTagPositions(source, patternBegin, PEMAPosition, tagBeginPositionMap, tagEndPositionMap);
	lpwstring originalIWord;
	bool inPattern = false;
	for (int I = sentenceBegin; I < sentenceEnd; I++)
	{
		source.getOriginalWord(I, originalIWord, false, false);
		if (I == patternBegin)
		{
			sentence += u"**";
			inPattern = true;
		}
		if (tagBeginPositionMap.find(I) != tagBeginPositionMap.end())
		{
			for (lpwstring tag : tagBeginPositionMap[I])
				sentence += tag + u"{";
		}
		sentence += originalIWord;
		if (tagEndPositionMap.find(I) != tagEndPositionMap.end())
		{
			bool notFoundInBeginPositions = tagBeginPositionMap.find(I) == tagBeginPositionMap.end();
			for (lpwstring tag : tagEndPositionMap[I])
			{
				if (notFoundInBeginPositions || tagBeginPositionMap[I].find(tag) == tagBeginPositionMap[I].end())
					sentence += u" " + tag;
				sentence += u"}";
			}
		}
		if (I == patternEnd - 1)
		{
			sentence += u"**";
			inPattern = false;
		}
		sentence += u" ";
	}
}

