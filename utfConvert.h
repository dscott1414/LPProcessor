/*
	utfConvert.h - UTF-16(lpwstring) <-> UTF-8(std::string) codec

	Overview:
		This is the portable replacement engine for utilities.cpp's wTM()/mTW()/
		mTWCodePage() functions (Win32 WideCharToMultiByte/MultiByteToWideChar
		wrappers -- zero macOS equivalent). Those three functions are NOT rewritten
		in this batch (that is a later batch, B7); this file provides the codec
		PRIMITIVES that batch will build thin wTM/mTW/mTWCodePage wrappers on top of,
		so the API shape here is deliberately chosen to make that later rewrite
		mechanical.

		Two directions, two different problems:

		1) Wide -> narrow (lp_encode_utf8 / lp_utf16_to_utf8): ALWAYS UTF-8. A
		   hand-rolled, surrogate-pair-aware UTF-16 -> UTF-8 encoder. The source
		   lpwstring is always well-formed UTF-16 by construction (it either came
		   from this same codec's narrow-to-wide direction, or from a literal), so
		   there is no detection ambiguity and this direction cannot fail.

		2) Narrow -> wide (lp_narrow_to_wide), WITH detection: replicates the
		   original mTW()'s exact encoding-detection ladder, because real cached
		   corpora (Project Gutenberg, British National Corpus, scraped web pages)
		   were originally parsed under this exact ladder -- changing its behaviour
		   would silently reinterpret already-cached data differently. The ladder,
		   preserved exactly (see utilities.cpp's own header comment for the
		   original, and lp_narrow_to_wide's doc comment below for why the ladder's
		   nominal 4th rung is unreachable in both the original and here):
		     (1) strict, validating UTF-8 decode (reject overlong encodings,
		         invalid continuation bytes, and UTF-8-encoded surrogate code
		         points) -- mirrors Win32's MB_ERR_INVALID_CHARS: any invalid byte
		         sequence fails this rung ENTIRELY, no lossy/replacement-character
		         decode.
		     (2) ISO-8859-1, except any byte in 0x80-0x9F is instead looked up in a
		         small Windows-1252 table (real legacy text using that byte range
		         almost always means CP1252 smart-quotes/dashes, not actual C1
		         control codes). This rung is trivially total -- every byte 0x00-0xFF
		         has a defined mapping under this scheme -- so it can never itself
		         fail.

	Key entry points:
		- lp_utf16_to_utf8() / lp_encode_utf8() - wide -> UTF-8, always succeeds.
		- lp_narrow_to_wide() - narrow -> wide with the 3-rung detection ladder.
		- lp_try_decode_strict_utf8() / lp_decode_latin1_cp1252() - the two rungs
		  exposed individually, for a later batch's mTWCodePage (forced, no
		  detection) replacement to call directly.

	Notes / gotchas:
		- lp_utf16_to_utf8()/lp_narrow_to_wide() (the no-out-param convenience
		  overloads) return a reference into a thread_local, grow-only, never-freed
		  scratch buffer -- mirrors the original wTM()/mTW()'s __declspec(thread)
		  buffers, which exist so repeated conversions are allocation-free once the
		  buffer reaches steady-state size (wTM is on the hot path of every MySQL
		  query; not yet exercised in this batch, but the contract is preserved from
		  the start rather than retrofitted later). The returned reference is valid
		  only until the next call to the SAME function on the SAME thread -- same
		  "don't hold a returned pointer across another conversion" rule the
		  original already documented. Copy the content out (e.g.
		  `std::string copy = lp_utf16_to_utf8(x);`) if it needs to outlive that.
		- The lower-level lp_try_decode_strict_utf8()/lp_decode_latin1_cp1252()/
		  lp_encode_utf8() building blocks are plain value/reference-parameter
		  functions with no thread_local involved -- easy to call, test, and reuse
		  directly (e.g. from a future forced-codepage mTWCodePage replacement).
		- LpDetectedEncoding is the "which rung won" out-parameter, mirroring the
		  original mTW()'s `codepage` out-parameter (UTF8 <-> CP_UTF8, ISO_8859_1
		  <-> 28591, CP1252 <-> 1252). It also fully subsumes the original's
		  `iso8859ControlCharactersFound` out-flag: in the original, that flag is
		  only ever set true in the same situation that also produces codepage==1252
		  (the direct "codepage=1252 because 8859 itself failed" branch is
		  unreachable dead code -- see lp_narrow_to_wide()'s doc comment), so a later
		  batch's mTW wrapper can derive that flag as simply
		  `detectedEncoding == LpDetectedEncoding::CP1252`.
*/
#pragma once
#include "lpchar.h"
#include <string>

enum class LpDetectedEncoding { UTF8, ISO_8859_1, CP1252 };

// Batch B3: the two Win32 code-page identifiers this codebase names symbolically
// (the others -- 1252, 28591 -- it already writes as bare numeric literals). They
// came from windows.h, which is gone, but the numeric values are kept exactly so
// that the codepage int threaded through wTM/mTW/mTWCodePage and stored alongside
// cached corpora keeps meaning the same thing it always did. Batch B7 rewires
// those three functions onto the codec above; until then these keep their existing
// call sites (utilities.cpp, tokenize.cpp, DBUtility.cpp, Internet.cpp) compiling
// and comparing correctly.
//
// CP_ACP ("the system ANSI code page") has no macOS equivalent -- macOS has no
// non-Unicode system code page at all. Its two real call sites (Internet.cpp's
// wTM(path, spath, CP_ACP) when building a local cache filename) are B9's to
// resolve; the value is defined here only so those lines still compile and still
// mean "not UTF-8" in the meantime.
constexpr int CP_ACP = 0;
constexpr int CP_UTF8 = 65001;

// ---------------------------------------------------------------------------------
// Low-level building blocks (no thread_local, no hidden state)
// ---------------------------------------------------------------------------------

// Attempts a STRICT UTF-8 decode of `in` into `out` (out is cleared, then filled on
// success). Returns true and leaves a fully-decoded `out` on success. Returns false
// on ANY invalid byte sequence (overlong encoding, truncated/invalid continuation
// byte, invalid lead byte, or a UTF-8-encoded surrogate code point U+D800-U+DFFF) --
// mirrors Win32 MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...) exactly: no
// partial/lossy/replacement-character decode is ever produced on failure, and
// `out`'s content must not be used when this returns false.
bool lp_try_decode_strict_utf8(const std::string& in, lpwstring& out);

// Decodes `in` as ISO-8859-1, except any byte in 0x80-0x9F is instead mapped
// through a small Windows-1252 lookup table (see file header). Every byte 0x00-0xFF
// has a defined mapping under this scheme, so this cannot fail -- there is no error
// return. Sets usedCp1252Table=true if ANY byte in 0x80-0x9F was encountered
// (mirrors the original mTW()'s iso8859ControlCharactersFound out-flag).
void lp_decode_latin1_cp1252(const std::string& in, lpwstring& out, bool& usedCp1252Table);

// Hand-rolled, surrogate-pair-aware UTF-16 -> UTF-8 encoder. APPENDS to `out`
// (does not clear it first). Always succeeds -- lpwstring is well-formed UTF-16 by
// construction, so there is no invalid-input case on this direction.
void lp_encode_utf8(const lpwstring& in, std::string& out);

// ---------------------------------------------------------------------------------
// High-level, thread_local-scratch-backed convenience (see Notes/gotchas above)
// ---------------------------------------------------------------------------------

const std::string& lp_utf16_to_utf8(const lpwstring& in);

// Decodes `in` using the 3-rung detection ladder described above; detectedEncoding
// reports which rung produced the result. This function CANNOT fail (rung 2 is
// total), so it has no error return -- unlike the original mTW(), which is
// documented fatal-on-total-failure for a case that is provably unreachable.
const lpwstring& lp_narrow_to_wide(const std::string& in, LpDetectedEncoding& detectedEncoding);

// Convenience overload for callers that don't need to know which rung won.
const lpwstring& lp_narrow_to_wide(const std::string& in);

// Batch B5: replacement for MSVC's TEXT() macro, whose only use in this codebase is
// TEXT(__FUNCTION__) -- turning the compiler's narrow function-name string into a
// wide one for a `fromWhere` diagnostic argument. TEXT() cannot work here (it
// prefixes an L to a string literal, and __FUNCTION__ is not one), so this converts
// at runtime instead. Every call site passes the result to an lpwstring-by-value
// parameter, which copies it, so the thread_local scratch reference is safe.
#define LP_TEXT(narrowText) (lp_narrow_to_wide(std::string(narrowText)))
