/*
	lpchar.cpp - see lpchar.h for the full design writeup.

	This file has exactly one job per function group:
		1) lp_vsnprintf/lp_snprintf - the specifier-by-specifier format-string parser
		   and its narrow-vsnprintf delegate for every non-string/char conversion.
		2) lp_wtoi/lp_wtoll - a single shared, overflow-safe digit-accumulation
		   helper instantiated for both widths.
		3) lp_wcscasecmp/lp_towlower_str - ASCII-range case helpers.
		4) lp_strlen/lp_strcpy/lp_strnlen - trivial NUL-terminated-array walks.

	Dependencies: standard library only (<cstdio> for the narrow vsnprintf delegate,
	<vector> for the rare oversized-numeric-field growth path). Deliberately does
	NOT include windows.h/io.h/winhttp.h/mysql.h or any other heavy/platform header
	-- see lpchar.h's file header for why that matters (virtually every other file
	in the codebase will pull this in before almost anything else).
*/
#include "lpchar.h"
#include "utfConvert.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <vector>

// ===================================================================================
// Printf engine
// ===================================================================================

namespace
{
	inline bool isAsciiDigit(lpchar_t c) { return c >= u'0' && c <= u'9'; }

	// Consumes a run of ASCII decimal digits at *pp (advancing it), returning their
	// value (0 if there are none). Clamped defensively so a pathological/malformed
	// width or precision digit run cannot overflow `int` while parsing -- no real
	// format string anywhere in the codebase has a width/precision anywhere near
	// this large.
	int consumeUnsignedDecimal(const lpchar_t*& p)
	{
		long v = 0;
		while (isAsciiDigit(*p))
		{
			v = v * 10 + (*p - u'0');
			if (v > 1000000) v = 1000000;
			++p;
		}
		return (int)v;
	}

	// Appends `data[0..len)` to out, padded to `width` with spaces (left-justified
	// if requested). This is the entire width/flag handling for %s/%S/%c/%C --
	// those conversions never delegate to the narrow vsnprintf, so this is the only
	// place their padding logic lives. Precision (truncation) is applied by the
	// caller before this is reached.
	void emitWidened(lpwstring& out, const lpchar_t* data, size_t len, int width, bool leftJustify)
	{
		size_t w = (width > 0) ? (size_t)width : 0;
		if (len >= w)
		{
			out.append(data, len);
			return;
		}
		size_t pad = w - len;
		if (leftJustify)
		{
			out.append(data, len);
			out.append(pad, u' ');
		}
		else
		{
			out.append(pad, u' ');
			out.append(data, len);
		}
	}

	// Formats `value` against the (already fully-resolved, ASCII, single-specifier)
	// narrow sub-format `narrowFmt` and appends the (guaranteed-ASCII) result to
	// `out`, byte-for-byte widened. Tries a small stack buffer first; on the rare
	// overflow (e.g. an unusually large width/precision) retries once with a
	// heap buffer sized exactly to the real narrow vsnprintf's own reported
	// required length.
	template <typename T>
	void formatAndAppend(lpwstring& out, const char* narrowFmt, T value)
	{
		char stackBuf[128];
		int n = std::snprintf(stackBuf, sizeof(stackBuf), narrowFmt, value);
		if (n < 0) return; // defensive; shouldn't happen for the controlled formats this engine builds
		if ((size_t)n < sizeof(stackBuf))
		{
			for (int i = 0; i < n; ++i) out.push_back((lpchar_t)(unsigned char)stackBuf[i]);
			return;
		}
		std::vector<char> heapBuf((size_t)n + 1);
		int n2 = std::snprintf(heapBuf.data(), heapBuf.size(), narrowFmt, value);
		if (n2 < 0) return;
		int cnt = (n2 < (int)heapBuf.size()) ? n2 : (int)heapBuf.size() - 1;
		for (int i = 0; i < cnt; ++i) out.push_back((lpchar_t)(unsigned char)heapBuf[i]);
	}
} // namespace

int lp_vsnprintf(lpchar_t* buffer, size_t bufferCount, const lpchar_t* format, va_list args)
{
	lpwstring out;
	const lpchar_t* p = format;
	while (*p)
	{
		if (*p != u'%')
		{
			out.push_back(*p++);
			continue;
		}
		const lpchar_t* specBegin = p; // points at '%', used only for the malformed-tail / unrecognized-conversion fallback text
		++p; // consume '%'

		if (*p == u'%')
		{
			out.push_back(u'%');
			++p;
			continue;
		}

		// --- flags ---
		bool flagMinus = false, flagPlus = false, flagSpace = false, flagHash = false, flagZero = false;
		for (;;)
		{
			if (*p == u'-') { flagMinus = true; ++p; }
			else if (*p == u'+') { flagPlus = true; ++p; }
			else if (*p == u' ') { flagSpace = true; ++p; }
			else if (*p == u'#') { flagHash = true; ++p; }
			else if (*p == u'0') { flagZero = true; ++p; }
			else break;
		}

		// --- width ---
		bool widthIsStar = false;
		int widthLiteral = -1; // -1 == not specified
		if (*p == u'*') { widthIsStar = true; ++p; }
		else if (isAsciiDigit(*p)) { widthLiteral = consumeUnsignedDecimal(p); }

		// --- precision ---
		bool hasPrecisionSyntax = false, precisionIsStar = false;
		int precisionLiteral = 0;
		if (*p == u'.')
		{
			hasPrecisionSyntax = true;
			++p;
			if (*p == u'*') { precisionIsStar = true; ++p; }
			else { precisionLiteral = consumeUnsignedDecimal(p); } // "%.d"-style (no digits after '.') means precision 0
		}

		// --- length modifier --- "I64" is special-cased exactly like standard "ll"
		// (MSVC-only token; see lpchar.h). 0=none,1=hh,2=h,3=l,4=ll/I64,5=L,6=z,7=j,8=t
		int lenModKind = 0;
		if (p[0] == u'I' && p[1] == u'6' && p[2] == u'4') { lenModKind = 4; p += 3; }
		else if (p[0] == u'h' && p[1] == u'h') { lenModKind = 1; p += 2; }
		else if (p[0] == u'l' && p[1] == u'l') { lenModKind = 4; p += 2; }
		else if (p[0] == u'h') { lenModKind = 2; p += 1; }
		else if (p[0] == u'l') { lenModKind = 3; p += 1; }
		else if (p[0] == u'L') { lenModKind = 5; p += 1; }
		else if (p[0] == u'z') { lenModKind = 6; p += 1; }
		else if (p[0] == u'j') { lenModKind = 7; p += 1; }
		else if (p[0] == u't') { lenModKind = 8; p += 1; }

		if (*p == 0)
		{
			// Malformed: '%' (plus whatever flags/width/precision/length modifier)
			// with no conversion character before the string ends. Emit the literal
			// text seen so far and stop -- there is no conversion character to act on.
			out.append(specBegin, (size_t)(p - specBegin));
			break;
		}
		lpchar_t conv = *p;
		++p;

		// '*' width/precision arguments are consumed from the va_list here, in
		// left-to-right source order (width before precision before the value
		// itself) -- matching standard printf argument order.
		int width = widthLiteral;
		if (widthIsStar)
		{
			int w = va_arg(args, int);
			if (w < 0) { flagMinus = true; w = -w; } // negative '*' width means left-justify, standard behaviour
			width = w;
		}
		bool hasPrecision = hasPrecisionSyntax;
		int precision = precisionLiteral;
		if (hasPrecisionSyntax && precisionIsStar)
		{
			int pr = va_arg(args, int);
			if (pr < 0) hasPrecision = false; // negative '*' precision means "as if omitted", standard behaviour
			else precision = pr;
		}

		if (conv == u's')
		{
			const lpchar_t* s = va_arg(args, const lpchar_t*);
			static const lpchar_t nullMarker[] = u"(null)";
			size_t len;
			if (!s) { s = nullMarker; len = 6; }
			else { len = lp_strlen(s); }
			if (hasPrecision && (size_t)precision < len) len = (size_t)precision;
			emitWidened(out, s, len, width, flagMinus);
		}
		else if (conv == u'S')
		{
			// MSVC "opposite width": a narrow (char*) string embedded in a wide
			// format -- real call sites pass e.g. mysql_error() (utilities.cpp's
			// mTW*/DBUtility.cpp/DBCreateSQLSchema.cpp). Byte-widened, not UTF-8
			// decoded -- matches the original MSVC %S behaviour in the "C" locale
			// (no encoding awareness), and is consistent with how every other
			// delegated specifier here is widened.
			const char* s = va_arg(args, const char*);
			static const char nullMarkerN[] = "(null)";
			size_t len;
			if (!s) { s = nullMarkerN; len = 6; }
			else { len = std::strlen(s); }
			if (hasPrecision && (size_t)precision < len) len = (size_t)precision;
			lpwstring widened;
			widened.resize(len);
			for (size_t i = 0; i < len; ++i) widened[i] = (lpchar_t)(unsigned char)s[i];
			emitWidened(out, widened.data(), len, width, flagMinus);
		}
		else if (conv == u'c')
		{
			lpchar_t c = (lpchar_t)va_arg(args, int); // promoted, matches standard varargs char promotion
			emitWidened(out, &c, 1, width, flagMinus);
		}
		else if (conv == u'C')
		{
			char c = (char)va_arg(args, int);
			lpchar_t widened = (lpchar_t)(unsigned char)c;
			emitWidened(out, &widened, 1, width, flagMinus);
		}
		else
		{
			bool isUnsignedConv = (conv == u'u' || conv == u'x' || conv == u'X' || conv == u'o');
			bool isFloatConv = (conv == u'f' || conv == u'F' || conv == u'e' || conv == u'E' ||
			                    conv == u'g' || conv == u'G' || conv == u'a' || conv == u'A');
			bool isSignedIntConv = (conv == u'd' || conv == u'i');
			bool isPtrConv = (conv == u'p');

			if (!isUnsignedConv && !isFloatConv && !isSignedIntConv && !isPtrConv)
			{
				// Unrecognized conversion character -- fail visibly rather than
				// silently mis-rendering (see lpchar.h's Notes/gotchas). Emit the
				// original specifier text verbatim and keep processing the rest of
				// the format string.
				out.append(specBegin, (size_t)(p - specBegin));
				continue;
			}

			// Every conversion reaching here is guaranteed pure-ASCII output in the
			// C locale -- rebuild an equivalent single-specifier narrow sub-format
			// (from the RESOLVED width/precision/flags, not by copying source text,
			// since a '*' in the original text has already been consumed above) and
			// delegate to the real narrow vsnprintf.
			//
			// The length-modifier text embedded in narrowFmt is derived from `kind`
			// itself (not independently from the raw parsed lenModKind) so the type
			// we hand to the delegate's OWN varargs always agrees with the modifier
			// we tell it to expect -- a mismatch there would be undefined behaviour.
			// z/j/t are folded into one "8-byte integer" bucket (emitted as 'z'
			// against a size_t/ptrdiff_t value) since size_t/intmax_t/ptrdiff_t are
			// all 8 bytes on every platform this code targets, past (Windows x64)
			// or present (macOS arm64/x86_64).
			enum class Kind { INT, UINT, LONG, ULONG, LLONG, ULLONG, SIZE, SSIZE, DOUBLE, LDOUBLE, PTR } kind;
			const char* narrowLenMod = "";
			if (isPtrConv) { kind = Kind::PTR; narrowLenMod = ""; }
			else if (isFloatConv)
			{
				if (lenModKind == 5) { kind = Kind::LDOUBLE; narrowLenMod = "L"; }
				else { kind = Kind::DOUBLE; narrowLenMod = ""; }
			}
			else if (lenModKind == 6 || lenModKind == 7 || lenModKind == 8)
			{
				kind = isUnsignedConv ? Kind::SIZE : Kind::SSIZE;
				narrowLenMod = "z";
			}
			else if (lenModKind == 4) { kind = isUnsignedConv ? Kind::ULLONG : Kind::LLONG; narrowLenMod = "ll"; }
			else if (lenModKind == 3) { kind = isUnsignedConv ? Kind::ULONG : Kind::LONG; narrowLenMod = "l"; }
			else { kind = isUnsignedConv ? Kind::UINT : Kind::INT; narrowLenMod = ""; } // none/hh/h all promote to int

			char narrowFmt[64];
			size_t nf = 0;
			narrowFmt[nf++] = '%';
			if (flagHash) narrowFmt[nf++] = '#';
			if (flagZero) narrowFmt[nf++] = '0';
			if (flagPlus) narrowFmt[nf++] = '+';
			if (flagSpace) narrowFmt[nf++] = ' ';
			if (flagMinus) narrowFmt[nf++] = '-';
			if (width >= 0) nf += (size_t)std::snprintf(narrowFmt + nf, sizeof(narrowFmt) - nf, "%d", width);
			if (hasPrecision)
			{
				narrowFmt[nf++] = '.';
				nf += (size_t)std::snprintf(narrowFmt + nf, sizeof(narrowFmt) - nf, "%d", precision);
			}
			for (const char* m = narrowLenMod; *m; ++m) narrowFmt[nf++] = *m;
			narrowFmt[nf++] = (char)conv; // conv is always ASCII (d i o u x X f F e E g G a A p) here
			narrowFmt[nf] = '\0';

			switch (kind)
			{
			case Kind::INT:     formatAndAppend(out, narrowFmt, va_arg(args, int)); break;
			case Kind::UINT:    formatAndAppend(out, narrowFmt, va_arg(args, unsigned int)); break;
			case Kind::LONG:    formatAndAppend(out, narrowFmt, va_arg(args, long)); break;
			case Kind::ULONG:   formatAndAppend(out, narrowFmt, va_arg(args, unsigned long)); break;
			case Kind::LLONG:   formatAndAppend(out, narrowFmt, va_arg(args, long long)); break;
			case Kind::ULLONG:  formatAndAppend(out, narrowFmt, va_arg(args, unsigned long long)); break;
			case Kind::SSIZE:   formatAndAppend(out, narrowFmt, va_arg(args, ptrdiff_t)); break;
			case Kind::SIZE:    formatAndAppend(out, narrowFmt, va_arg(args, size_t)); break;
			case Kind::DOUBLE:  formatAndAppend(out, narrowFmt, va_arg(args, double)); break;
			case Kind::LDOUBLE: formatAndAppend(out, narrowFmt, va_arg(args, long double)); break;
			case Kind::PTR:     formatAndAppend(out, narrowFmt, va_arg(args, void*)); break;
			}
		}
	}

	size_t len = out.size();
	if (bufferCount > 0 && buffer)
	{
		size_t toCopy = (len < bufferCount - 1) ? len : (bufferCount - 1);
		std::memcpy(buffer, out.data(), toCopy * sizeof(lpchar_t));
		buffer[toCopy] = 0;
	}
	return (int)len;
}

int lp_snprintf(lpchar_t* buffer, size_t bufferCount, const lpchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = lp_vsnprintf(buffer, bufferCount, format, args);
	va_end(args);
	return result;
}

// ===================================================================================
// wprintf/fwprintf replacements (see lpchar.h doc comment for why these exist)
// ===================================================================================

int lp_vfwprintf(FILE* stream, const lpchar_t* format, va_list args)
{
	va_list args2;
	va_copy(args2, args);
	int need = lp_vsnprintf(nullptr, 0, format, args2);
	va_end(args2);
	if (need <= 0) return need;
	std::vector<lpchar_t> buf((size_t)need + 1);
	lp_vsnprintf(buf.data(), buf.size(), format, args);
	lpwstring formatted(buf.data(), (size_t)need);
	const std::string& utf8 = lp_utf16_to_utf8(formatted);
	return (int)std::fwrite(utf8.data(), 1, utf8.size(), stream);
}

int lp_fwprintf(FILE* stream, const lpchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = lp_vfwprintf(stream, format, args);
	va_end(args);
	return result;
}


int lp_wprintf(const lpchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = lp_vfwprintf(stdout, format, args);
	va_end(args);
	return result;
}

// ===================================================================================
// Numeric parsing
// ===================================================================================

namespace
{
	// Shared implementation for lp_wtoi/lp_wtoll: skips optional whitespace, an
	// optional sign, then accumulates a run of decimal digits as an unsigned
	// magnitude, saturating at (maxMagPos for a non-negative result, maxMagNeg for
	// a negative one) instead of overflowing -- matches strtol/wcstol-family
	// defined-overflow semantics. maxMagNeg may legitimately be 2^63 (int64's
	// negative range is one larger in magnitude than its positive range), which is
	// why the negation at the end is done via an explicit boundary check rather
	// than a plain `-(long long)mag` (that cast alone would be out-of-range/
	// implementation-defined for exactly that one magnitude).
	long long wtoAccumulate(const lpchar_t* s, uint64_t maxMagPos, uint64_t maxMagNeg)
	{
		if (!s) return 0;
		size_t i = 0;
		while (s[i] == u' ' || s[i] == u'\t' || s[i] == u'\n' || s[i] == u'\v' || s[i] == u'\f' || s[i] == u'\r') ++i;
		bool neg = false;
		if (s[i] == u'+' || s[i] == u'-') { neg = (s[i] == u'-'); ++i; }

		uint64_t limit = neg ? maxMagNeg : maxMagPos;
		uint64_t mag = 0;
		for (; s[i] >= u'0' && s[i] <= u'9'; ++i)
		{
			unsigned d = (unsigned)(s[i] - u'0');
			if (mag > (limit - d) / 10) { mag = limit; break; } // would overflow -- saturate and stop (no endptr to report, so no need to keep scanning)
			mag = mag * 10 + d;
		}

		if (neg)
		{
			if (mag > (uint64_t)INT64_MAX) return INT64_MIN; // exactly maxMagNeg==2^63 can only happen for lp_wtoll's own boundary
			return -(long long)mag;
		}
		return (long long)mag; // mag <= maxMagPos <= INT64_MAX here, always well-defined
	}
} // namespace

int lp_wtoi(const lpchar_t* s)
{
	long long v = wtoAccumulate(s, (uint64_t)INT32_MAX, (uint64_t)INT32_MAX + 1);
	if (v < INT32_MIN) return INT32_MIN;
	if (v > INT32_MAX) return INT32_MAX;
	return (int)v;
}

int64_t lp_wtoll(const lpchar_t* s)
{
	return (int64_t)wtoAccumulate(s, (uint64_t)INT64_MAX, (uint64_t)INT64_MAX + 1);
}

long lp_wcstol(const lpchar_t* s, lpchar_t** endptr, int base)
{
	if (endptr) *endptr = (lpchar_t*)s;
	if (!s) return 0;
	const lpchar_t* p = s;
	while (*p == u' ' || *p == u'\t' || *p == u'\n' || *p == u'\v' || *p == u'\f' || *p == u'\r') ++p;
	bool neg = false;
	if (*p == u'+' || *p == u'-') { neg = (*p == u'-'); ++p; }

	if ((base == 0 || base == 16) && p[0] == u'0' && (p[1] == u'x' || p[1] == u'X')) { p += 2; base = 16; }
	else if (base == 0 && p[0] == u'0') base = 8;
	else if (base == 0) base = 10;

	const lpchar_t* digitsStart = p;
	unsigned long mag = 0;
	bool overflow = false;
	for (;; ++p)
	{
		lpchar_t c = *p;
		int digit;
		if (c >= u'0' && c <= u'9') digit = (int)(c - u'0');
		else if (c >= u'a' && c <= u'z') digit = (int)(c - u'a') + 10;
		else if (c >= u'A' && c <= u'Z') digit = (int)(c - u'A') + 10;
		else break;
		if (digit >= base) break;
		if (mag > (ULONG_MAX - (unsigned long)digit) / (unsigned long)base) overflow = true;
		else mag = mag * (unsigned long)base + (unsigned long)digit;
	}

	if (p == digitsStart) return 0; // no digits consumed -- endptr stays at s, matching the standard contract
	if (endptr) *endptr = (lpchar_t*)p;
	if (overflow) return neg ? LONG_MIN : LONG_MAX;
	if (neg) return (mag > (unsigned long)LONG_MAX + 1) ? LONG_MIN : -(long)mag;
	return (mag > (unsigned long)LONG_MAX) ? LONG_MAX : (long)mag;
}

// ===================================================================================
// Case-insensitive compare / lowering (ASCII-range only -- see lpchar.h)
// ===================================================================================

int lp_wcscasecmp(const lpchar_t* a, const lpchar_t* b)
{
	if (a == b) return 0;
	if (!a) return -1;
	if (!b) return 1;
	for (;;)
	{
		lpchar_t ca = *a++, cb = *b++;
		lpchar_t la = (ca >= u'A' && ca <= u'Z') ? (lpchar_t)(ca + (u'a' - u'A')) : ca;
		lpchar_t lb = (cb >= u'A' && cb <= u'Z') ? (lpchar_t)(cb + (u'a' - u'A')) : cb;
		if (la != lb) return (la < lb) ? -1 : 1;
		if (ca == 0) return 0; // both strings ended together
	}
}

void lp_towlower_str(lpwstring& s)
{
	for (auto& ch : s)
	{
		if (ch >= u'A' && ch <= u'Z') ch = (lpchar_t)(ch + (u'a' - u'A'));
	}
}

void lp_towlower_str(lpchar_t* s)
{
	for (; *s; ++s)
	{
		if (*s >= u'A' && *s <= u'Z') *s = (lpchar_t)(*s + (u'a' - u'A'));
	}
}

void lp_towlower_str(char* s)
{
	for (; *s; ++s)
	{
		if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + ('a' - 'A'));
	}
}

void lp_toupper_str(lpwstring& s)
{
	for (auto& ch : s)
	{
		if (ch >= u'a' && ch <= u'z') ch = (lpchar_t)(ch - (u'a' - u'A'));
	}
}

void lp_toupper_str(lpchar_t* s)
{
	for (; *s; ++s)
	{
		if (*s >= u'a' && *s <= u'z') *s = (lpchar_t)(*s - (u'a' - u'A'));
	}
}

// ===================================================================================
// Trivial NUL-terminated-array helpers
// ===================================================================================

size_t lp_strlen(const lpchar_t* s)
{
	size_t n = 0;
	while (s[n]) ++n;
	return n;
}

lpchar_t* lp_strcpy(lpchar_t* dst, const lpchar_t* src)
{
	lpchar_t* d = dst;
	while ((*d++ = *src++)) {}
	return dst;
}

size_t lp_strnlen(const lpchar_t* s, size_t maxlen)
{
	size_t n = 0;
	while (n < maxlen && s[n]) ++n;
	return n;
}

// ===================================================================================
// Search/compare/misc-legacy helpers (see lpchar.h doc comment for why these exist)
// ===================================================================================

const lpchar_t* lp_strchr(const lpchar_t* s, lpchar_t c)
{
	for (;; ++s)
	{
		if (*s == c) return s;
		if (!*s) return nullptr;
	}
}
lpchar_t* lp_strchr(lpchar_t* s, lpchar_t c)
{
	return const_cast<lpchar_t*>(lp_strchr((const lpchar_t*)s, c));
}

const lpchar_t* lp_strrchr(const lpchar_t* s, lpchar_t c)
{
	const lpchar_t* last = nullptr;
	for (;; ++s)
	{
		if (*s == c) last = s;
		if (!*s) return last;
	}
}
lpchar_t* lp_strrchr(lpchar_t* s, lpchar_t c)
{
	return const_cast<lpchar_t*>(lp_strrchr((const lpchar_t*)s, c));
}

const lpchar_t* lp_strstr(const lpchar_t* haystack, const lpchar_t* needle)
{
	if (!*needle) return haystack;
	size_t needleLen = lp_strlen(needle);
	for (; *haystack; ++haystack)
	{
		size_t i = 0;
		while (i < needleLen && haystack[i] == needle[i]) ++i;
		if (i == needleLen) return haystack;
	}
	return nullptr;
}
lpchar_t* lp_strstr(lpchar_t* haystack, const lpchar_t* needle)
{
	return const_cast<lpchar_t*>(lp_strstr((const lpchar_t*)haystack, needle));
}

int lp_strcmp(const lpchar_t* a, const lpchar_t* b)
{
	while (*a && *a == *b) { ++a; ++b; }
	return (int)*a - (int)*b;
}

int lp_strncmp(const lpchar_t* a, const lpchar_t* b, size_t n)
{
	while (n && *a && *a == *b) { ++a; ++b; --n; }
	if (n == 0) return 0;
	return (int)*a - (int)*b;
}

size_t lp_wcsspn(const lpchar_t* s, const lpchar_t* accept)
{
	size_t n = 0;
	while (s[n] && lp_strchr(accept, s[n])) ++n;
	return n;
}

namespace
{
	thread_local lpchar_t* lp_wcstokSavedPos = nullptr;
}

lpchar_t* lp_wcstok(lpchar_t* str, const lpchar_t* delim)
{
	lpchar_t* s = str ? str : lp_wcstokSavedPos;
	if (!s) return nullptr;
	while (*s && lp_strchr(delim, *s)) ++s;
	if (!*s) { lp_wcstokSavedPos = nullptr; return nullptr; }
	lpchar_t* tokenStart = s;
	while (*s && !lp_strchr(delim, *s)) ++s;
	if (*s) { *s = 0; lp_wcstokSavedPos = s + 1; }
	else lp_wcstokSavedPos = nullptr;
	return tokenStart;
}

lpchar_t* lp_wcsdup(const lpchar_t* s)
{
	if (!s) return nullptr;
	size_t n = lp_strlen(s);
	lpchar_t* d = (lpchar_t*)std::malloc((n + 1) * sizeof(lpchar_t));
	if (!d) return nullptr;
	std::memcpy(d, s, (n + 1) * sizeof(lpchar_t));
	return d;
}

// ---------------------------------------------------------------------------------
// Batch B5: _itow/_i64tow replacement -- see lpchar.h for why the public entry points
// are array-reference templates and this shared implementation takes an explicit
// count. Always radix 10 (every real call site used 10). Always NUL-terminates when
// bufferCount > 0, truncating rather than overflowing if the buffer is too small.
// ---------------------------------------------------------------------------------
lpchar_t* lp_itow_n(int64_t value, lpchar_t* buffer, size_t bufferCount)
{
	if (!buffer || bufferCount == 0) return buffer;
	char narrow[32];
	snprintf(narrow, sizeof(narrow), "%lld", (long long)value);
	size_t i = 0;
	for (; narrow[i] && i + 1 < bufferCount; i++)
		buffer[i] = (lpchar_t)(unsigned char)narrow[i];
	buffer[i] = 0;
	return buffer;
}

// ---------------------------------------------------------------------------------
// Batch B5: fgetws/fputws replacements -- see lpchar.h. The stream is treated as
// UTF-8, decoded/encoded through utfConvert. A line whose bytes are not valid UTF-8
// falls back to the same latin1/cp1252 rung the rest of the codec uses, so a stray
// legacy-encoded line in a data file degrades the way mTW() would rather than
// failing the read.
// ---------------------------------------------------------------------------------
#include "utfConvert.h"

lpchar_t* lp_fgetws(lpchar_t* buffer, int bufferCount, FILE* stream)
{
	if (!buffer || bufferCount <= 0 || !stream) return nullptr;
	// A UTF-8 character is at most 4 bytes, so this is always enough narrow input
	// to produce bufferCount-1 UTF-16 code units. (4 bytes can also produce a
	// surrogate PAIR, i.e. 2 units, so this is generous rather than tight.)
	std::string narrow;
	narrow.resize((size_t)bufferCount * 4 + 1);
	if (!fgets(&narrow[0], (int)narrow.size(), stream))
		return nullptr;
	narrow.resize(strlen(narrow.c_str()));
	const lpwstring& wide = lp_narrow_to_wide(narrow);
	size_t copyCount = wide.size();
	if (copyCount > (size_t)bufferCount - 1) copyCount = (size_t)bufferCount - 1;
	for (size_t i = 0; i < copyCount; i++) buffer[i] = wide[i];
	buffer[copyCount] = 0;
	return buffer;
}

// See lpchar.h. Mirrors fgetws() on an MSVC "rb" stream: the terminating newline
// is kept in the buffer, the result is NUL-terminated, and NULL is returned only
// when nothing at all could be read. A trailing odd byte at EOF ends the line.
lpchar_t* lp_fgetws16(lpchar_t* buffer, int bufferCount, FILE* stream)
{
	if (!buffer || bufferCount <= 0 || !stream) return nullptr;
	int count = 0;
	while (count < bufferCount - 1)
	{
		unsigned char pair[2];
		if (fread(pair, 1, 2, stream) != 2) break;
		lpchar_t unit = (lpchar_t)(pair[0] | (pair[1] << 8));   // little-endian
		buffer[count++] = unit;
		if (unit == u'\n') break;
	}
	if (count == 0) return nullptr;
	buffer[count] = 0;
	return buffer;
}

int lp_fputws(const lpchar_t* s, FILE* stream)
{
	if (!s || !stream) return -1;
	const std::string& narrow = lp_utf16_to_utf8(lpwstring(s));
	return (fputs(narrow.c_str(), stream) < 0) ? -1 : 1;
}

// ---------------------------------------------------------------------------------
// Batch B5/B6a: _wcsnicmp, swscanf and fwscanf replacements. See lpchar.h.
// ---------------------------------------------------------------------------------
int lp_strncasecmp(const lpchar_t* a, const lpchar_t* b, size_t n)
{
	if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
	for (size_t i = 0; i < n; i++)
	{
		lpchar_t ca = a[i], cb = b[i];
		if (ca >= u'A' && ca <= u'Z') ca = (lpchar_t)(ca - u'A' + u'a');
		if (cb >= u'A' && cb <= u'Z') cb = (lpchar_t)(cb - u'A' + u'a');
		if (ca != cb) return (ca < cb) ? -1 : 1;
		if (ca == 0) return 0;
	}
	return 0;
}

// Narrow an ASCII-only lpchar_t string. Any code unit above 0x7F becomes '?', which
// cannot affect a numeric conversion and keeps the byte count identical.
static std::string narrowAscii(const lpchar_t* s)
{
	std::string out;
	for (; s && *s; s++)
		out += (*s <= 0x7F) ? (char)*s : '?';
	return out;
}

int lp_swscanf(const lpchar_t* input, const lpchar_t* format, ...)
{
	if (!input || !format) return -1;
	std::string narrowFormat = narrowAscii(format);
	// See lpchar.h: %s would write narrow characters into what the caller believes
	// is an lpchar_t buffer. Refuse rather than corrupt memory.
	if (narrowFormat.find("%s") != std::string::npos) return -1;
	std::string narrowInput = narrowAscii(input);
	va_list args;
	va_start(args, format);
	int result = vsscanf(narrowInput.c_str(), narrowFormat.c_str(), args);
	va_end(args);
	return result;
}

int lp_fwscanf(FILE* stream, const lpchar_t* format, ...)
{
	if (!stream || !format) return -1;
	std::string narrowFormat = narrowAscii(format);
	if (narrowFormat.find("%s") != std::string::npos) return -1;
	va_list args;
	va_start(args, format);
	int result = vfscanf(stream, narrowFormat.c_str(), args);
	va_end(args);
	return result;
}
