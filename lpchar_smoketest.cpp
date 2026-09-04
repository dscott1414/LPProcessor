// lpchar_smoketest.cpp - standalone correctness proof for batch B1
// (lpchar.h/.cpp + utfConvert.h/.cpp). NOT part of lpcore/lp/CorpusAnalysis --
// built as its own EXCLUDE_FROM_ALL CMake target (lpchar_smoketest), compiling
// only lpchar.cpp/utfConvert.cpp directly, independent of the lpcore/lp/
// CorpusAnalysis targets (which are not expected to build until later batches).
// Mirrors the B0 cmake_smoketest.cpp convention: no test framework exists in this
// codebase and none is introduced here, just a plain main() with a small manual
// PASS/FAIL harness.
//
// Every expected value below was cross-checked against this machine's real libc
// (bash printf / a standalone clang-compiled C snippet / python3's utf-8 and
// cp1252 codecs) before being written in, specifically so a failure here means
// "the engine disagrees with known-correct ground truth", not "my hand arithmetic
// might be off".
#include "lpchar.h"
#include "utfConvert.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <string>

static int gChecks = 0;
static int gFailures = 0;

static void checkImpl(bool cond, const char* what, const char* file, int line)
{
	++gChecks;
	if (!cond)
	{
		++gFailures;
		std::printf("FAIL [%s:%d] %s\n", file, line, what);
	}
}
#define CHECK(cond) checkImpl((cond), #cond, __FILE__, __LINE__)

// ===================================================================================
// 1) printf engine
// ===================================================================================
static void testPrintfEngine()
{
	lpchar_t buf[512];

	// --- literal text passthrough, %s, %% ---
	lp_snprintf(buf, 512, u"plain text, no specifiers");
	CHECK(lpwstring(buf) == u"plain text, no specifiers");

	lp_snprintf(buf, 512, u"%s", u"hello");
	CHECK(lpwstring(buf) == u"hello");

	lp_snprintf(buf, 512, u"100%% done");
	CHECK(lpwstring(buf) == u"100% done");

	// --- %d / %u / %x / %X / %c ---
	lp_snprintf(buf, 512, u"%d", (int)42);
	CHECK(lpwstring(buf) == u"42");

	lp_snprintf(buf, 512, u"%d", (int)-17);
	CHECK(lpwstring(buf) == u"-17");

	lp_snprintf(buf, 512, u"%u", (unsigned int)4000000000u);
	CHECK(lpwstring(buf) == u"4000000000");

	lp_snprintf(buf, 512, u"%x", (unsigned int)255);
	CHECK(lpwstring(buf) == u"ff");

	lp_snprintf(buf, 512, u"%X", (unsigned int)255);
	CHECK(lpwstring(buf) == u"FF");

	lp_snprintf(buf, 512, u"%c", (int)u'Q');
	CHECK(lpwstring(buf) == u"Q");

	// --- %f / %g, default and explicit precision (real patterns: utilities.cpp's
	//     dtos() uses "%4.2g"; specials_main.cpp/pattern.cpp use "%5.2f"-family) ---
	lp_snprintf(buf, 512, u"%f", 1.5);
	CHECK(lpwstring(buf) == u"1.500000"); // C default %f precision is 6 decimal digits, confirmed against real libc

	lp_snprintf(buf, 512, u"%5.2f", 3.14159);
	CHECK(lpwstring(buf) == lpwstring(1, u' ') + u"3.14"); // confirmed against real libc: "[ 3.14]"

	lp_snprintf(buf, 512, u"%4.2g", 3.14159);
	CHECK(lpwstring(buf) == lpwstring(1, u' ') + u"3.1"); // confirmed against real libc: "[ 3.1]"

	// --- width/precision on %s (real patterns: agreement.cpp's "%-35.35s",
	//     specials_main.cpp's "%50.50s"/"%15.15s", getMusicBrainz-family "%40s") ---
	lp_snprintf(buf, 512, u"[%10s]", u"hi");
	CHECK(lpwstring(buf) == u"[" + lpwstring(8, u' ') + u"hi]"); // confirmed against real libc: "[        hi]"

	lp_snprintf(buf, 512, u"%-8.3s|", u"hello");
	CHECK(lpwstring(buf) == u"hel" + lpwstring(5, u' ') + u"|"); // confirmed against real libc: "hel     |"

	// --- I64 (the MSVC-only 3-char length-modifier run this engine must special-
	//     case): plain, zero-padded-width variants, and unsigned, matching real
	//     call sites (main.cpp/specials_main.cpp "%08I64d"/"%06I64d", createOntology.cpp
	//     "%03I64d", getWikipedia.cpp "%I64u") ---
	lp_snprintf(buf, 512, u"%I64d", (long long)9223372036854775807LL); // INT64_MAX
	CHECK(lpwstring(buf) == u"9223372036854775807");

	lp_snprintf(buf, 512, u"%03I64d", (long long)7);
	CHECK(lpwstring(buf) == u"007");

	lp_snprintf(buf, 512, u"%08I64d", (long long)1234);
	CHECK(lpwstring(buf) == u"00001234");

	lp_snprintf(buf, 512, u"%I64u", (unsigned long long)18446744073709551615ULL); // UINT64_MAX
	CHECK(lpwstring(buf) == u"18446744073709551615");

	// --- a realistic COMPOSITE format string, modeled closely on
	//     DBWordRelations.cpp:185's real progress line (same specifiers, same
	//     literal punctuation), not a synthetic one ---
	lp_snprintf(buf, 512, u"PROGRESS: %03I64d%% word relations read with %04d seconds elapsed \r",
	            (long long)5, (int)42);
	CHECK(lpwstring(buf) == u"PROGRESS: 005% word relations read with 0042 seconds elapsed \r");

	// --- %S: MSVC "opposite width", a narrow char* argument inside a wide format
	//     string -- heavily real (170+ call sites, e.g. DBUtility.cpp's
	//     "mysql_real_query failed - %S (len=%d): ", DBCreateSQLSchema.cpp's
	//     "Failed to connect to MySQL - %S" both format mysql_error()'s char*) ---
	lp_snprintf(buf, 512, u"Error: %S (len=%d)", "connection refused", (int)19);
	CHECK(lpwstring(buf) == u"Error: connection refused (len=19)");

	// --- %ld (real pattern: memoryStat.cpp's "%ld%% memory in use...") ---
	lp_snprintf(buf, 512, u"%ld%%", (long)87);
	CHECK(lpwstring(buf) == u"87%");

	// --- %z (size_t/ptrdiff_t; real patterns: tokenize.cpp's "%06zu",
	//     createOntology.cpp's "%06zd") ---
	lp_snprintf(buf, 512, u"%06zu", (size_t)42);
	CHECK(lpwstring(buf) == u"000042");

	lp_snprintf(buf, 512, u"%06zd", (ptrdiff_t)-7);
	CHECK(lpwstring(buf) == u"-00007"); // confirmed against a compiled clang test binary

	// --- lp_vsnprintf truncation contract: NUL-terminates even when truncated,
	//     and returns the UNTRUNCATED required length (matches C99 vsnprintf,
	//     documented as a deliberate improvement over legacy MSVC lp_vsnprintf) ---
	lpchar_t small[4];
	int needed = lp_snprintf(small, 4, u"%s", u"hello world");
	CHECK(needed == 11); // "hello world" is 11 chars -- reported length ignores truncation
	CHECK(small[3] == 0); // still NUL-terminated within the 4-lpchar_t buffer
	CHECK(lpwstring(small) == u"hel"); // first 3 chars + the forced NUL

	// --- lp_vsnprintf with bufferCount==0 / buffer==nullptr: pure "measure" call,
	//     must not write anywhere, matches the vsnprintf(nullptr,0,...) idiom ---
	int measured = lp_snprintf(nullptr, 0, u"%d-%s", (int)7, u"x");
	CHECK(measured == 3); // "7-x"

	std::printf("testPrintfEngine: done (%d checks so far)\n", gChecks);
}

// ===================================================================================
// 2) UTF-8 <-> UTF-16 codec
// ===================================================================================
static void testUtfCodec()
{
	// --- full round trip: ASCII + a BMP accented Latin char (e) + a BMP CJK char
	//     (verified U+4E2D "middle") + an astral emoji requiring a UTF-16 surrogate
	//     pair (verified U+1F600 GRINNING FACE). Every byte/code-unit value below
	//     was cross-checked with python3's own utf-8/utf-16-le codec, not hand
	//     arithmetic alone: "cafe(with e-acute) <CJK> <emoji>" ->
	//       utf8:  63 61 66 C3 A9 20 E4 B8 AD 20 F0 9F 98 80
	//       utf16: 0063 0061 0066 00E9 0020 4E2D 0020 D83D DE00
	lpwstring wide;
	wide.push_back(u'c'); wide.push_back(u'a'); wide.push_back(u'f'); wide.push_back((lpchar_t)0x00E9);
	wide.push_back(u' ');
	wide.push_back((lpchar_t)0x4E2D);
	wide.push_back(u' ');
	wide.push_back((lpchar_t)0xD83D); wide.push_back((lpchar_t)0xDE00); // U+1F600 surrogate pair

	std::string utf8 = lp_utf16_to_utf8(wide); // copies out of the thread_local buffer immediately
	const unsigned char expectedUtf8Bytes[] = {
		0x63, 0x61, 0x66, 0xC3, 0xA9, 0x20, 0xE4, 0xB8, 0xAD, 0x20, 0xF0, 0x9F, 0x98, 0x80
	};
	CHECK(utf8.size() == sizeof(expectedUtf8Bytes));
	CHECK(utf8.size() == sizeof(expectedUtf8Bytes) &&
	      std::memcmp(utf8.data(), expectedUtf8Bytes, sizeof(expectedUtf8Bytes)) == 0);

	LpDetectedEncoding enc;
	lpwstring roundTripped = lp_narrow_to_wide(utf8, enc);
	CHECK(enc == LpDetectedEncoding::UTF8);
	CHECK(roundTripped == wide); // exact round trip, including the surrogate pair

	// --- rung 1: strict, VALID UTF-8 decodes via the UTF-8 rung directly ---
	{
		std::string validUtf8Bytes;
		validUtf8Bytes.push_back('c'); validUtf8Bytes.push_back('a'); validUtf8Bytes.push_back('f');
		validUtf8Bytes.push_back((char)0xC3); validUtf8Bytes.push_back((char)0xA9); // e-acute, valid 2-byte UTF-8
		LpDetectedEncoding e2;
		lpwstring w2 = lp_narrow_to_wide(validUtf8Bytes, e2);
		CHECK(e2 == LpDetectedEncoding::UTF8);
		CHECK(w2.size() == 4 && w2[0] == u'c' && w2[1] == u'a' && w2[2] == u'f' && w2[3] == (lpchar_t)0x00E9);

		bool ok = lp_try_decode_strict_utf8(validUtf8Bytes, w2);
		CHECK(ok);
	}

	// --- rung 2a: INVALID UTF-8 (a lone 0xE9 lead byte with no continuation --
	//     0xE9 requires two 0x80-0xBF continuation bytes to be valid 3-byte UTF-8)
	//     falls back to ISO-8859-1, where byte 0xE9 IS a valid, total mapping to
	//     U+00E9 -- confirmed against python3's latin-1 codec ---
	{
		std::string invalidUtf8_validLatin1;
		invalidUtf8_validLatin1.push_back((char)0xE9);
		bool strictOk = lp_try_decode_strict_utf8(invalidUtf8_validLatin1, wide /* scratch, overwritten */);
		CHECK(!strictOk); // must fail rung 1 entirely, no partial/lossy decode

		LpDetectedEncoding e3;
		lpwstring w3 = lp_narrow_to_wide(invalidUtf8_validLatin1, e3);
		CHECK(e3 == LpDetectedEncoding::ISO_8859_1);
		CHECK(w3.size() == 1 && w3[0] == (lpchar_t)0x00E9);
	}

	// --- rung 2b: bytes 0x93/0x94 (also invalid as UTF-8 -- they fall in the
	//     continuation-only range 0x80-0xBF, so they are not even a valid UTF-8
	//     LEAD byte) must decode via the CP1252 table to the smart-quote code
	//     points U+201C/U+201D, NOT the raw Latin-1 C1 control codes U+0093/
	//     U+0094 -- confirmed against python3's cp1252 codec ---
	{
		std::string smartQuoted; // byte-for-byte: 0x93 'H' 'e' 'l' 'l' 'o' 0x94
		smartQuoted.push_back((char)0x93);
		smartQuoted += "Hello";
		smartQuoted.push_back((char)0x94);

		bool strictOk = lp_try_decode_strict_utf8(smartQuoted, wide /* scratch, overwritten */);
		CHECK(!strictOk);

		LpDetectedEncoding e4;
		lpwstring w4 = lp_narrow_to_wide(smartQuoted, e4);
		CHECK(e4 == LpDetectedEncoding::CP1252);
		lpwstring expected4;
		expected4.push_back((lpchar_t)0x201C);
		expected4 += u"Hello";
		expected4.push_back((lpchar_t)0x201D);
		CHECK(w4 == expected4);

		bool usedTable = false;
		lpwstring w4b;
		lp_decode_latin1_cp1252(smartQuoted, w4b, usedTable);
		CHECK(usedTable);
		CHECK(w4b == expected4);
	}

	// --- a plain ASCII-only string should decode identically regardless of rung
	//     (sanity check that the common case is unaffected by any of this) ---
	{
		std::string ascii = "just ascii text 123";
		LpDetectedEncoding e5;
		lpwstring w5 = lp_narrow_to_wide(ascii, e5);
		CHECK(e5 == LpDetectedEncoding::UTF8); // ASCII is always trivially valid UTF-8
		CHECK(w5 == u"just ascii text 123");
	}

	std::printf("testUtfCodec: done (%d checks so far)\n", gChecks);
}

// ===================================================================================
// 3) lp_wtoi / lp_wtoll / lp_wcscasecmp / lp_towlower_str / lp_strlen / lp_strcpy /
//    lp_strnlen
// ===================================================================================
static void testStringPrimitives()
{
	// --- lp_wtoi ---
	CHECK(lp_wtoi(u"123") == 123);
	CHECK(lp_wtoi(u"-456") == -456);
	CHECK(lp_wtoi(u"  789") == 789);   // leading whitespace
	CHECK(lp_wtoi(u"+42") == 42);      // explicit '+'
	CHECK(lp_wtoi(u"abc") == 0);       // no digits at all
	CHECK(lp_wtoi(u"") == 0);          // empty string edge case
	CHECK(lp_wtoi(u"42abc") == 42);    // stops at first non-digit
	CHECK(lp_wtoi(u"2147483647") == INT32_MAX);   // exact boundary, must NOT saturate
	CHECK(lp_wtoi(u"-2147483648") == INT32_MIN);  // exact boundary, must NOT saturate
	CHECK(lp_wtoi(u"9999999999") == INT32_MAX);   // overflow -> saturate, not UB
	CHECK(lp_wtoi(u"-9999999999") == INT32_MIN);  // negative overflow -> saturate

	// --- lp_wtoll (needs the full 64-bit range per the task's edge-case ask) ---
	CHECK(lp_wtoll(u"123") == 123);
	CHECK(lp_wtoll(u"") == 0); // empty string edge case
	CHECK(lp_wtoll(u"9223372036854775807") == INT64_MAX);   // exact boundary, must NOT saturate
	CHECK(lp_wtoll(u"-9223372036854775808") == INT64_MIN);  // exact boundary, must NOT saturate
	CHECK(lp_wtoll(u"99999999999999999999") == INT64_MAX);  // overflow -> saturate
	CHECK(lp_wtoll(u"-99999999999999999999") == INT64_MIN); // negative overflow -> saturate

	// --- lp_wcscasecmp (ASCII-range, command-line-flag-parsing use case) ---
	CHECK(lp_wcscasecmp(u"Hello", u"HELLO") == 0);
	CHECK(lp_wcscasecmp(u"", u"") == 0); // empty string edge case
	CHECK(lp_wcscasecmp(u"abc", u"abd") < 0);
	CHECK(lp_wcscasecmp(u"abd", u"abc") > 0);
	CHECK(lp_wcscasecmp(u"-server", u"-SERVER") == 0); // realistic main.cpp-style flag comparison

	// --- lp_towlower_str ---
	{
		lpwstring s = u"HELLO World 123";
		lp_towlower_str(s);
		CHECK(s == u"hello world 123");
	}
	{
		lpwstring s; // empty string edge case
		lp_towlower_str(s);
		CHECK(s == u"");
	}

	// --- lp_strlen / lp_strnlen ---
	CHECK(lp_strlen(u"") == 0);      // empty string edge case
	CHECK(lp_strlen(u"hello") == 5);
	CHECK(lp_strnlen(u"hello", 3) == 3);  // truncated by maxlen
	CHECK(lp_strnlen(u"hi", 10) == 2);    // shorter than maxlen
	CHECK(lp_strnlen(u"", 5) == 0);       // empty string edge case

	// --- lp_strcpy ---
	{
		lpchar_t dest[32];
		lpchar_t* ret = lp_strcpy(dest, u"hello");
		CHECK(ret == dest);              // returns dst, matches lp_strcpy's contract
		CHECK(lpwstring(dest) == u"hello");
	}

	std::printf("testStringPrimitives: done (%d checks so far)\n", gChecks);
}


// Batch B5: lp_itow / lp_i64tow (the _itow/_i64tow replacement).
static void testItow()
{
	lpchar_t buffer[16];
	CHECK(lpwstring(lp_itow(0, buffer)) == u"0");
	CHECK(lpwstring(lp_itow(42, buffer)) == u"42");
	CHECK(lpwstring(lp_itow(-7, buffer)) == u"-7");
	CHECK(lpwstring(lp_itow(2147483647, buffer)) == u"2147483647");
	CHECK(lpwstring(lp_itow(-2147483647 - 1, buffer)) == u"-2147483648");
	lpchar_t wide[24]; // 16 digits plus NUL does not fit in buffer[16] -- see below
	CHECK(lpwstring(lp_i64tow((int64_t)9007199254740993LL, wide)) == u"9007199254740993");
	// A buffer too small must truncate and stay NUL-terminated, never overflow.
	// Several real call sites pass lpchar_t[10], which cannot hold INT_MIN.
	lpchar_t small[4];
	CHECK(lpwstring(lp_itow(123456, small)) == u"123");
	CHECK(lp_strlen(small) == 3);
	std::printf("testItow: done (%d checks so far)\n", gChecks);
}

// Batch B5/B6a: lp_strncasecmp, lp_swscanf.
static void testCompareAndScan()
{
	CHECK(lp_strncasecmp(u"Hello", u"hello", 5) == 0);
	CHECK(lp_strncasecmp(u"HELLO", u"help", 3) == 0);   // bounded compare stops at 3
	CHECK(lp_strncasecmp(u"HELLO", u"help", 4) != 0);
	CHECK(lp_strncasecmp(u"abc", u"abc", 100) == 0);    // stops at the NUL, no overrun
	CHECK(lp_strncasecmp(u"a", u"b", 1) < 0);
	CHECK(lp_strncasecmp(u"b", u"a", 1) > 0);

	int month = 0, day = 0, year = 0;
	CHECK(lp_swscanf(u"12-25-1991", u"%d-%d-%d", &month, &day, &year) == 3);
	CHECK(month == 12 && day == 25 && year == 1991);
	CHECK(lp_swscanf(u"3/4", u"%d/%d", &month, &year) == 2);
	CHECK(month == 3 && year == 4);
	CHECK(lp_swscanf(u"notanumber", u"%d", &month) == 0);
	// %s is refused rather than writing narrow bytes into a wide buffer.
	lpchar_t target[8];
	CHECK(lp_swscanf(u"word", u"%s", target) == -1);
	std::printf("testCompareAndScan: done (%d checks so far)\n", gChecks);
}

int main()
{
	testPrintfEngine();
	testUtfCodec();
	testStringPrimitives();
	testItow();
	testCompareAndScan();

	if (gFailures == 0)
		std::printf("\n=== ALL %d CHECKS PASSED ===\n", gChecks);
	else
		std::printf("\n=== %d OF %d CHECKS FAILED ===\n", gFailures, gChecks);

	return gFailures == 0 ? 0 : 1;
}
