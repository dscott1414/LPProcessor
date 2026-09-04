/*
	utfConvert.cpp - see utfConvert.h for the full design writeup.

	Dependencies: standard library only (<cstdint> for the fixed-width types the
	decoder's bit math uses). Deliberately does NOT include windows.h or any other
	heavy/platform header, matching lpchar.h/.cpp's own constraint.
*/
#include "utfConvert.h"
#include <cstdint>

// ===================================================================================
// Rung 1: strict UTF-8
// ===================================================================================

bool lp_try_decode_strict_utf8(const std::string& in, lpwstring& out)
{
	out.clear();
	out.reserve(in.size()); // upper bound for the common mostly-ASCII case

	const unsigned char* s = reinterpret_cast<const unsigned char*>(in.data());
	size_t n = in.size();
	size_t i = 0;
	while (i < n)
	{
		unsigned char b0 = s[i];
		uint32_t cp;
		size_t seqLen;

		if (b0 <= 0x7F) { cp = b0; seqLen = 1; }
		else if (b0 >= 0xC2 && b0 <= 0xDF) { cp = b0 & 0x1Fu; seqLen = 2; }
		else if (b0 >= 0xE0 && b0 <= 0xEF) { cp = b0 & 0x0Fu; seqLen = 3; }
		else if (b0 >= 0xF0 && b0 <= 0xF4) { cp = b0 & 0x07u; seqLen = 4; }
		else return false; // stray continuation byte (0x80-0xBF), always-overlong lead (0xC0-0xC1), or lead byte that can only exceed U+10FFFF (0xF5-0xFF)

		if (i + seqLen > n) return false; // truncated sequence at end of input

		for (size_t k = 1; k < seqLen; ++k)
		{
			unsigned char bk = s[i + k];
			if (bk < 0x80 || bk > 0xBF) return false; // invalid (non-continuation) byte where a continuation byte was required
			cp = (cp << 6) | (uint32_t)(bk & 0x3Fu);
		}

		// Reject overlong encodings (a code point encoded in more bytes than its
		// minimal form requires) and UTF-8-encoded surrogate code points -- mirrors
		// Win32's MB_ERR_INVALID_CHARS. cp>0x10FFFF is reachable specifically via
		// lead byte 0xF4 with a high continuation (e.g. F4 90 80 80 = U+110000);
		// the others are defensive given the lead-byte gating above already
		// excludes most of the way there.
		if (seqLen == 2 && cp < 0x80) return false;
		if (seqLen == 3 && cp < 0x800) return false;
		if (seqLen == 4 && cp < 0x10000) return false;
		if (cp >= 0xD800 && cp <= 0xDFFF) return false;
		if (cp > 0x10FFFF) return false;

		if (cp <= 0xFFFF)
		{
			out.push_back((lpchar_t)cp);
		}
		else
		{
			uint32_t v = cp - 0x10000;
			out.push_back((lpchar_t)(0xD800 + (v >> 10)));
			out.push_back((lpchar_t)(0xDC00 + (v & 0x3FFu)));
		}
		i += seqLen;
	}
	return true;
}

// ===================================================================================
// Rung 2: ISO-8859-1, augmented with a CP1252 table for 0x80-0x9F
// ===================================================================================

namespace
{
	// Standard Microsoft "best-fit" Windows-1252 mapping for byte values 0x80-0x9F
	// (index 0 == byte 0x80, ..., index 31 == byte 0x9F) -- the same table Win32's
	// MultiByteToWideChar(1252, ...) itself uses, which is what the original mTW()
	// ladder this replicates actually called. The five byte values Windows leaves
	// unassigned in this range (0x81, 0x8D, 0x8F, 0x90, 0x9D) map to themselves
	// (identity with their ISO-8859-1/C1-control code point) per Microsoft's own
	// table -- NOT the WHATWG web-encoding-standard table, which instead treats
	// those five as hard decode errors. Bit-exact fidelity with the original Win32
	// behaviour is the goal here (already-cached corpora were parsed under it), not
	// conformance to the browser standard.
	constexpr char16_t kCp1252Table_80_9F[32] = {
		/* 0x80 */ 0x20AC, /* 0x81 */ 0x0081, /* 0x82 */ 0x201A, /* 0x83 */ 0x0192,
		/* 0x84 */ 0x201E, /* 0x85 */ 0x2026, /* 0x86 */ 0x2020, /* 0x87 */ 0x2021,
		/* 0x88 */ 0x02C6, /* 0x89 */ 0x2030, /* 0x8A */ 0x0160, /* 0x8B */ 0x2039,
		/* 0x8C */ 0x0152, /* 0x8D */ 0x008D, /* 0x8E */ 0x017D, /* 0x8F */ 0x008F,
		/* 0x90 */ 0x0090, /* 0x91 */ 0x2018, /* 0x92 */ 0x2019, /* 0x93 */ 0x201C,
		/* 0x94 */ 0x201D, /* 0x95 */ 0x2022, /* 0x96 */ 0x2013, /* 0x97 */ 0x2014,
		/* 0x98 */ 0x02DC, /* 0x99 */ 0x2122, /* 0x9A */ 0x0161, /* 0x9B */ 0x203A,
		/* 0x9C */ 0x0153, /* 0x9D */ 0x009D, /* 0x9E */ 0x017E, /* 0x9F */ 0x0178,
	};
}

void lp_decode_latin1_cp1252(const std::string& in, lpwstring& out, bool& usedCp1252Table)
{
	out.clear();
	out.reserve(in.size());
	usedCp1252Table = false;
	for (unsigned char b : in)
	{
		if (b >= 0x80 && b <= 0x9F)
		{
			usedCp1252Table = true;
			out.push_back(kCp1252Table_80_9F[b - 0x80]);
		}
		else
		{
			out.push_back((lpchar_t)b); // ISO-8859-1: byte value == code point for every byte outside 0x80-0x9F
		}
	}
}

// ===================================================================================
// Wide -> UTF-8 encoder
// ===================================================================================

void lp_encode_utf8(const lpwstring& in, std::string& out)
{
	size_t i = 0, n = in.size();
	while (i < n)
	{
		uint32_t cp = in[i++];
		if (cp >= 0xD800 && cp <= 0xDBFF && i < n)
		{
			char16_t low = in[i];
			if (low >= 0xDC00 && low <= 0xDFFF)
			{
				cp = 0x10000 + (((cp - 0xD800) << 10) | (uint32_t)(low - 0xDC00));
				++i;
			}
			// else: unpaired high surrogate. lpwstring is well-formed UTF-16 by
			// construction (see file header) so this should not occur in practice;
			// fall through and encode the lone surrogate's value as-is rather than
			// throwing/asserting on unexpected input.
		}

		if (cp <= 0x7F)
		{
			out.push_back((char)cp);
		}
		else if (cp <= 0x7FF)
		{
			out.push_back((char)(0xC0 | (cp >> 6)));
			out.push_back((char)(0x80 | (cp & 0x3Fu)));
		}
		else if (cp <= 0xFFFF)
		{
			out.push_back((char)(0xE0 | (cp >> 12)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3Fu)));
			out.push_back((char)(0x80 | (cp & 0x3Fu)));
		}
		else
		{
			out.push_back((char)(0xF0 | (cp >> 18)));
			out.push_back((char)(0x80 | ((cp >> 12) & 0x3Fu)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3Fu)));
			out.push_back((char)(0x80 | (cp & 0x3Fu)));
		}
	}
}

// ===================================================================================
// thread_local-backed convenience wrappers
// ===================================================================================

const std::string& lp_utf16_to_utf8(const lpwstring& in)
{
	thread_local std::string buf;
	buf.clear(); // clear() keeps capacity -- this is the grow-only, never-freed discipline
	lp_encode_utf8(in, buf);
	return buf;
}

const lpwstring& lp_narrow_to_wide(const std::string& in, LpDetectedEncoding& detectedEncoding)
{
	thread_local lpwstring buf;
	if (lp_try_decode_strict_utf8(in, buf)) // lp_try_decode_strict_utf8 clears buf itself
	{
		detectedEncoding = LpDetectedEncoding::UTF8;
		return buf;
	}
	bool usedCp1252 = false;
	lp_decode_latin1_cp1252(in, buf, usedCp1252); // clears/refills buf itself; rung 2 is total, cannot fail
	detectedEncoding = usedCp1252 ? LpDetectedEncoding::CP1252 : LpDetectedEncoding::ISO_8859_1;
	return buf;
}

const lpwstring& lp_narrow_to_wide(const std::string& in)
{
	LpDetectedEncoding discard;
	return lp_narrow_to_wide(in, discard);
}
