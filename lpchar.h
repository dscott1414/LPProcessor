/*
	lpchar.h - portable wide-character type + printf engine + string primitives

	Overview:
		The macOS port cannot use wchar_t/wstring directly: wchar_t is 2 bytes on
		Windows but 4 bytes on macOS, and the binary source-cache format (source.cpp's
		copy() family) assumes 2 bytes/char. lpchar_t/lpwstring below are the portable
		fixed-width replacement used everywhere instead (see the port plan's "Key
		design decisions"). This is NOT a Windows-compatibility shim: char16_t has no
		<cwchar>-equivalent string functions in the standard library on ANY platform,
		and no platform ships a char16_t printf, so a from-scratch char16_t codebase
		would need exactly this file regardless of what OS it targets.

		Four groups of things live here:
		1) The type aliases themselves (lpchar_t/lpwstring).
		2) A portable printf engine (lp_vsnprintf/lp_snprintf) operating on lpchar_t
		   strings/format strings, replacing swprintf/_snwprintf/wsprintf-family call
		   sites once they are mechanically renamed (batch B2).
		3) Numeric/case string helpers (lp_wtoi/lp_wtoll/lp_wcscasecmp/
		   lp_towlower_str) replacing _wtoi/_wtoi64/_wcsicmp/_wcslwr.
		4) Trivial NUL-terminated-array helpers (lp_strlen/lp_strcpy/lp_strnlen)
		   replacing wcslen/wcscpy/wcsnlen.

	Printf engine design (see lpchar.cpp for the full parser):
		Literal text and the %s/%c specifiers (and their MSVC "opposite-width"
		counterparts %S/%C, used ~170x in the real codebase for narrow char* strings
		like mysql_error() embedded in a wide format, e.g. utilities.cpp's mTW*
		functions and DBUtility.cpp) copy/widen lpchar_t or narrow-char data straight
		through with no numeric interpretation. Every OTHER conversion (d i o u x X f
		F e E g G a A p and their flag/width/precision/length modifiers) is guaranteed
		to produce pure-ASCII output in the C locale, so the engine rebuilds an
		equivalent single-specifier narrow sub-format, delegates to the platform's own
		vsnprintf, and widens the (ASCII, hence lossless) result byte-for-byte. The
		3-character run "I64" immediately after '%' (or after already-consumed
		flags/width/precision) is special-cased exactly like the standard "ll" length
		modifier, so %I64d/%08I64d/%I64u-style format strings (confirmed real call
		sites: main.cpp, DBWordRelations.cpp, createOntology.cpp, hmm.cpp,
		specials_main.cpp, profile.h, and elsewhere) work with zero per-call-site
		edits. Standard length modifiers h/hh/l/ll/L/z/j/t are also recognized and
		passed through to the delegate (z/j/t are all treated as a single 8-byte-wide
		integer bucket internally -- see lpchar.cpp -- since size_t/intmax_t/
		ptrdiff_t are all 8 bytes on every platform this code targets, past or
		present).

		Explicitly OUT of scope for this file (by design, not oversight):
		  - wsprintf's missing buffer-size argument is a SEPARATE problem from the
		    char16_t width issue (94 real call sites, see the port plan) and is left
		    for the batch that mechanically rewrites those call sites; no unbounded
		    "lp_sprintf" convenience is provided here on purpose.
		  - No lpwstring-returning "just format it for me" convenience is provided;
		    later batches replacing swprintf/_snwprintf-style call sites already have
		    a caller-owned fixed buffer at each site, so lp_snprintf's signature
		    (buffer, bufferCount, format, ...) is a direct mechanical replacement.

	Notes / gotchas:
		- lp_vsnprintf/lp_snprintf always NUL-terminate when bufferCount>0 (even on
		  truncation) and return the number of lpchar_t that WOULD have been written
		  (excluding the NUL) if bufferCount were unlimited -- standard C99 vsnprintf
		  truncation semantics, which is friendlier/safer than legacy MSVC
		  _vsnwprintf (which does NOT null-terminate on truncation). A deliberate,
		  documented improvement, not an oversight.
		- A %s/%S argument that is a null pointer prints the literal text "(null)"
		  instead of crashing/reading garbage (a GNU libc extension this engine
		  chooses to match) -- defensive, since a silent misformat here is exactly
		  the "corrupts everything downstream silently" failure mode this batch was
		  written to avoid.
		- An unrecognized conversion character (anything other than
		  s/S/c/C/%/d/i/o/u/x/X/f/F/e/E/g/G/a/A/p) is copied into the output as
		  literal text (e.g. a stray "%q" stays "%q") rather than crashing OR being
		  silently swallowed, so a malformed/unsupported format string is obviously
		  wrong (greppable) in whatever log/query it ends up in, without taking the
		  whole process down. This has not been observed in the real codebase (grep-
		  checked against every first-party .cpp/.h during this batch) but the
		  engine must not corrupt output quietly on formats it hasn't seen yet.
		- lp_wtoi/lp_wtoll SATURATE on out-of-range input (return INT32_MIN/MAX or
		  INT64_MIN/MAX) instead of invoking undefined behaviour the way plain
		  _wtoi/atoi-family functions do on overflow -- matches strtol/wcstol-family
		  defined-overflow semantics, a deliberate improvement requested for this
		  port rather than a faithful bug-for-bug port of _wtoi's UB.
		- lp_wcscasecmp/lp_towlower_str are ASCII-range-only (no Unicode case
		  folding). Confirmed acceptable: lp_wcscasecmp's only real call sites are
		  command-line flag parsing (main.cpp/specials_main.cpp), and
		  lp_towlower_str's equivalent (_wcslwr) is used the same way.
		- lp_strlen/lp_strcpy/lp_strnlen intentionally have wcscpy's classic UNSAFE,
		  unbounded-write contract (no destination size check) -- they are 1:1
		  replacements for wcscpy et al., not a hardening pass; callers are
		  responsible for destination sizing exactly as they already are today.
*/
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <string>

// Portable stand-ins for wchar_t/wstring -- see file header. Always exactly 2 bytes
// wide on every platform (unlike wchar_t), matching the on-disk binary cache format.
typedef char16_t lpchar_t;
typedef std::u16string lpwstring;

// ---------------------------------------------------------------------------------
// Printf engine
// ---------------------------------------------------------------------------------

// Formats into buffer (capacity bufferCount, in lpchar_t units). Always NUL-
// terminates when bufferCount>0, even if the formatted result is truncated to fit.
// Returns the number of lpchar_t that would have been written had bufferCount been
// unlimited (excluding the terminating NUL) -- matches C99 vsnprintf's return-value
// contract, so `int need = lp_vsnprintf(nullptr, 0, fmt, args);` is a valid "measure
// first" idiom. See the file header for the full specifier-support/design notes.
int lp_vsnprintf(lpchar_t* buffer, size_t bufferCount, const lpchar_t* format, va_list args);

// varargs wrapper over lp_vsnprintf. Direct mechanical replacement for
// swprintf(buffer, bufferCount, format, ...)/_snwprintf(buffer, bufferCount, format, ...).
int lp_snprintf(lpchar_t* buffer, size_t bufferCount, const lpchar_t* format, ...);

// Batch B2 addition (see lpchar.cpp): the codebase has ~120 real call sites using
// plain wprintf(format,...)/fwprintf(stream,format,...) to print progress/status
// messages straight to stdout/a FILE* (e.g. DBUtility.cpp/DBWordRelations.cpp
// "PROGRESS: ...\r" lines, pattern.cpp/hmm.cpp file dumps). Real wprintf/fwprintf
// take a genuine wchar_t* format (a DIFFERENT, platform builtin type from
// lpchar_t/char16_t), so these calls stopped compiling the moment their format
// string literals became u"..."-typed. These wrap lp_vsnprintf's own
// measure-then-fill idiom and hand the guaranteed-well-formed-UTF-16 result to
// utfConvert.h's lp_utf16_to_utf8 for the actual byte-level write -- consistent
// with how every OTHER narrow output path in this codebase (lplog included) is
// UTF-8, not a raw byte-for-byte widen/narrow. NOTE: these are a narrow "make it
// compile, keep exact call shape" replacement, not a redesign of console-progress
// output -- the port plan separately calls for a unified reportProgress() helper
// (later batch, alongside the SetConsoleTitle call sites) that may eventually
// replace these call sites too; lp_wprintf/lp_fwprintf existing now does not
// preclude that.
int lp_vfwprintf(FILE* stream, const lpchar_t* format, va_list args);
int lp_fwprintf(FILE* stream, const lpchar_t* format, ...);
int lp_wprintf(const lpchar_t* format, ...);

// ---------------------------------------------------------------------------------
// Numeric parsing / case-insensitive helpers
// ---------------------------------------------------------------------------------

// Replacement for _wtoi. Skips optional leading ASCII whitespace, an optional
// leading '+'/'-', then a run of ASCII decimal digits (stopping at the first
// non-digit); no locale dependency. SATURATES to INT32_MIN/INT32_MAX on overflow
// (see file header) instead of _wtoi's undefined behaviour. Returns 0 if s is null
// or contains no digits after the optional sign.
int lp_wtoi(const lpchar_t* s);

// Replacement for _wtoi64. Same grammar/semantics as lp_wtoi, saturating to
// INT64_MIN/INT64_MAX.
int64_t lp_wtoll(const lpchar_t* s);

// Batch B2 addition: replacement for wcstol (one real call site,
// getWikipedia.cpp's $XXXX-escape decoder, base 16, endptr unused). Unlike
// lp_wtoi/lp_wtoll (decimal only, by design -- see above), this matches
// wcstol's actual contract: optional leading whitespace, optional sign, an
// arbitrary base 2-36 (or 0 to auto-detect a "0x"/"0" prefix exactly like the
// real function), and reports the first unconsumed character through
// *endptr when endptr is non-null. Saturates to LONG_MIN/LONG_MAX on overflow
// (matching strtol's ERANGE return value) without setting errno -- no call
// site in this codebase checks errno after wcstol.
long lp_wcstol(const lpchar_t* s, lpchar_t** endptr, int base);

// Replacement for _wcsicmp. ASCII-range case-insensitive comparison (see file
// header for why full Unicode case folding is not needed here); returns
// <0/0/>0 exactly like strcmp/wcscmp. A null argument compares as less than any
// non-null string (defensive; the original _wcsicmp has no defined behaviour for
// null arguments).
int lp_wcscasecmp(const lpchar_t* a, const lpchar_t* b);

// Replacement for _wcslwr/wcslwr. Lowercases s in place, ASCII-range only.
void lp_towlower_str(lpwstring& s);
// Batch B2 addition: raw-buffer overload, for the several real call sites that
// pass a fixed local wchar_t buffer (initializeDictionary.cpp) or a
// const_cast-via-c_str() idiom (word.cpp/tokenize.cpp) rather than an lpwstring&
// -- same NUL-terminated, in-place, ASCII-only contract as the lpwstring&
// overload above.
void lp_towlower_str(lpchar_t* s);
// Batch B2 addition: narrow-char overload for _strlwr, an MSVC-CRT-only name
// with zero macOS equivalent (no width issue of its own, but the same class of
// problem as the rest of this file). Its two call sites were in getThesaurus.cpp,
// which has since been removed; kept as the narrow counterpart of the wide
// overload above. ASCII-only, mirrors the wide version exactly.
void lp_towlower_str(char* s);

// Batch B2 addition: uppercasing counterpart to lp_towlower_str, replacing
// wcsupr (3 real call sites, source.cpp, all via the same const_cast-via-
// c_str() idiom as the wcslwr sites above). Same ASCII-only, in-place,
// NUL-terminated-buffer contract, mirrored for symmetry/consistency even
// though only the raw-buffer overload currently has a real caller.
void lp_toupper_str(lpwstring& s);
void lp_toupper_str(lpchar_t* s);

// ---------------------------------------------------------------------------------
// Trivial NUL-terminated-array helpers (direct wcslen/wcscpy/wcsnlen replacements)
// ---------------------------------------------------------------------------------

size_t lp_strlen(const lpchar_t* s);
lpchar_t* lp_strcpy(lpchar_t* dst, const lpchar_t* src);
size_t lp_strnlen(const lpchar_t* s, size_t maxlen);

// ---------------------------------------------------------------------------------
// Batch B2 additions: search/compare/misc-legacy helpers. <cwchar> has no
// char16_t overloads for any of wcschr/wcsstr/wcscmp/wcsncmp/wcsrchr/wcstok/
// wcsdup/wcsspn (confirmed real call sites for every one of these, 7 to 89
// occurrences each depending on the function -- see the batch report), so each
// gets a small, direct, same-signature-shape replacement here rather than
// scattering one-off fixes through the ~15 application files that call them.
// ---------------------------------------------------------------------------------

// Replacement for wcschr/wcsrchr. Same contract as the standard functions,
// including the const/non-const overload pair (a const_cast internally, not a
// second implementation) so callers keep assigning the result to a non-const
// lpchar_t* without an explicit cast, exactly as they do today.
const lpchar_t* lp_strchr(const lpchar_t* s, lpchar_t c);
lpchar_t* lp_strchr(lpchar_t* s, lpchar_t c);
const lpchar_t* lp_strrchr(const lpchar_t* s, lpchar_t c);
lpchar_t* lp_strrchr(lpchar_t* s, lpchar_t c);

// Replacement for wcsstr. Same contract as the standard function (needle==""
// matches immediately at haystack), same const/non-const overload pair.
const lpchar_t* lp_strstr(const lpchar_t* haystack, const lpchar_t* needle);
lpchar_t* lp_strstr(lpchar_t* haystack, const lpchar_t* needle);

// Replacement for wcscmp/wcsncmp. Returns <0/0/>0 exactly like strcmp/wcscmp
// (lpchar_t compared as unsigned 16-bit code units).
int lp_strcmp(const lpchar_t* a, const lpchar_t* b);
int lp_strncmp(const lpchar_t* a, const lpchar_t* b, size_t n);

// Replacement for wcsspn. Returns the length of the initial segment of s
// consisting entirely of characters found in accept.
size_t lp_wcsspn(const lpchar_t* s, const lpchar_t* accept);

// Replacement for the legacy 2-argument wcstok(str, delim) (str==nullptr to
// continue a previous tokenization) -- the ONLY form used by this codebase's 2
// real call sites, not the reentrant 3-argument C11 form. Internal state is
// thread_local (the original MSVC implementation's hidden state is
// effectively per-thread too via TLS; this is not a behaviour change for any
// real call site, all of which are single-threaded parse loops).
lpchar_t* lp_wcstok(lpchar_t* str, const lpchar_t* delim);

// Replacement for wcsdup. Allocates via malloc (NOT tmalloc/new[]) and must be
// released via plain free() -- matching the real wcsdup's contract exactly,
// which this codebase's 3 real call sites (word.cpp) already do at their
// matching free() sites (not touched by this batch).
lpchar_t* lp_wcsdup(const lpchar_t* s);

// ---------------------------------------------------------------------------------
// Batch B5 addition: replacement for MSVC's _itow/_i64tow. Every one of the 14 real
// call sites uses radix 10 and passes a genuine array, so the radix argument is
// dropped and the buffer is taken BY REFERENCE TO ARRAY -- the size is then deduced
// rather than trusted, which is not a stylistic preference here: several call sites
// pass an lpchar_t[10], and the most negative int formats as "-2147483648", eleven
// characters plus a NUL. Those sites have always been two bytes short for that one
// input; with the size deduced, the value truncates instead of overflowing.
// Returns the buffer, matching _itow's contract so that the
// `lpwstring(_itow(...))` call sites need no other change.
// ---------------------------------------------------------------------------------
// Batch B5: replacement for the Win32 unbounded wsprintfW, which takes no buffer
// size at all -- the single largest remaining Win32 dependency by call count (84
// sites). The buffer is taken BY REFERENCE TO ARRAY so its size is deduced rather
// than supplied, which is what makes this a mechanical rename at every call site
// AND bounded at every call site: there is no size argument for anyone to get
// wrong, and a site that passes a bare pointer instead of an array will not
// compile, which is the correct outcome since such a site cannot be bounded
// automatically and needs a human decision.
template <size_t N, typename... Args>
inline int lp_wsprintf(lpchar_t(&buffer)[N], const lpchar_t* format, Args... args)
{
	return lp_snprintf(buffer, N, format, args...);
}

// Batch B5: the "append at an offset" form of the above. Several call sites format
// successive pieces into one buffer with `wsprintf(buf + len, ...)`, which defeats
// array-size deduction because the argument is a pointer. Passing the array and the
// offset separately keeps the size deducible, so these stay bounded too, and the
// truncation is computed against what is actually left rather than the whole buffer.
// Returns what lp_snprintf returns (the length that WOULD have been written), so the
// existing `len += lp_wsprintf(...)` accumulation keeps working; note that on
// truncation that makes len exceed the buffer, which the call sites below guard by
// clamping before the final NUL.
template <size_t N, typename... Args>
inline int lp_wsprintf_at(lpchar_t(&buffer)[N], size_t offset, const lpchar_t* format, Args... args)
{
	if (offset >= N) return 0;
	return lp_snprintf(buffer + offset, N - offset, format, args...);
}

// Batch B5: replacements for fgetws/fputws. The real ones take wchar_t (4 bytes
// here), so they cannot operate on lpchar_t at all. These read/write UTF-8 on the
// stream -- which is what these files already are, and what logging.cpp writes --
// and convert on the way through.
//
// lp_fgetws matches fgetws: reads at most bufferCount-1 characters, stops after a
// newline (which is KEPT in the buffer, as fgetws does), NUL-terminates, and
// returns the buffer or NULL at end-of-file with nothing read.
lpchar_t* lp_fgetws(lpchar_t* buffer, int bufferCount, FILE* stream);

// Reads one line of UTF-16LE code units straight off a binary stream. This is
// what MSVC's fgetws() does on a stream opened "rb" -- it copies wchar_t units
// verbatim rather than converting -- and it is what the ~14 `lp_wfopen(..., "rb")
// // binary mode reads unicode` call sites in initializeDictionary.cpp and
// paice.cpp have always relied on: 28 of the files under source/lists are
// UTF-16LE with a BOM.
//
// lp_fgetws above is the narrow/UTF-8 reader (fgets + decode) and is correct for
// the handful of genuinely narrow files (createOntology.cpp's .nt/.ttl RDF
// dumps). Point each call site at the one matching how the file is encoded; the
// two are not interchangeable. Callers strip the U+FEFF BOM themselves, as they
// already did on Windows.
lpchar_t* lp_fgetws16(lpchar_t* buffer, int bufferCount, FILE* stream);

// Removes one trailing `ch` from s, if present, and returns the new length.
// Safe on an empty string -- unlike the `s[lp_strlen(s) - 1]` idiom it replaces,
// which reads (and then writes) s[-1]. AddressSanitizer caught that as a
// stack-buffer-underflow in cWord::addPlaces, where a run of spaces before the
// buffer let the loop walk `len` negative and zero its way backwards down the
// stack; how far it got depended on what happened to be there, which is one way
// a parse stopped being reproducible. Blank lines in the UTF-16 list files are
// what make the length zero.
int lp_stripTrailing(lpchar_t* s, lpchar_t ch);

// Removes every trailing space from s and returns the new length. Same hazard,
// same guarantee: stops at the start of the string rather than running past it.
int lp_stripTrailingSpaces(lpchar_t* s);

// lp_fputws matches fputws: writes the string with no added newline. Returns a
// non-negative value on success, EOF on failure.
int lp_fputws(const lpchar_t* s, FILE* stream);

// Batch B5/B6a: replacement for MSVC's _wcsnicmp -- a case-insensitive compare
// bounded to n characters. ASCII-range folding only, same as lp_wcscasecmp (the
// real call sites are all keyword matching against ASCII literals).
int lp_strncasecmp(const lpchar_t* a, const lpchar_t* b, size_t n);

// Batch B6a: replacements for swscanf/fwscanf. Neither exists for char16_t on any
// platform. Both convert their input to narrow text and delegate to the ordinary
// sscanf/fscanf, which is sound because every real format string in this codebase
// is pure ASCII and reads numbers (%d/%u/%lf) or ASCII-delimited tokens.
//
// IMPORTANT: a %s conversion therefore writes NARROW characters. No call site here
// uses %s -- they are all numeric -- and passing a %s would be a bug, so the
// implementation deliberately refuses a format containing one rather than
// scribbling char data into an lpchar_t buffer.
int lp_swscanf(const lpchar_t* input, const lpchar_t* format, ...);
int lp_fwscanf(FILE* stream, const lpchar_t* format, ...);

lpchar_t* lp_itow_n(int64_t value, lpchar_t* buffer, size_t bufferCount);
template <size_t N> inline lpchar_t* lp_itow(int value, lpchar_t(&buffer)[N])
{
	return lp_itow_n(value, buffer, N);
}
template <size_t N> inline lpchar_t* lp_i64tow(int64_t value, lpchar_t(&buffer)[N])
{
	return lp_itow_n(value, buffer, N);
}
