// envConfig.cpp - see envConfig.h for the overview.
#include <string>
#include <stdlib.h>
// Batch B2: general.h's own declarations (e.g. wTM's `string &outString` param)
// are written bare (no std:: qualifier), matching the codebase-wide convention
// of always having "using namespace std;" active before general.h is reached
// (word.h does this -- BEFORE its own #include "general.h" -- for every other
// file). This file reaches general.h directly without going through word.h, and
// was missing this -- a pre-existing gap unrelated to wchar_t/char16_t
// (confirmed via git diff against HEAD), just not previously surfaced because
// nothing had compiled this far yet. Must precede the general.h include below,
// not just appear somewhere in this file, since #include is processed inline.
using namespace std;
#include "logging.h"
#include "general.h"
#include "envConfig.h"
#include "utfConvert.h"

// Batch B3: _wgetenv (MSVC's wide getenv, reading the CRT's own UTF-16 copy of the
// environment) has no POSIX equivalent -- the environment on macOS is bytes, full
// stop. Every read goes through narrow getenv() and, where the value is wanted
// wide, through utfConvert.h's decoder. That decoder's UTF-8-first detection ladder
// is exactly right for macOS environment bytes, which are UTF-8 in any modern
// locale but are not guaranteed to be valid UTF-8 by the OS.
//
// Also batch B3: the caching moved OUT of these two helpers and into each
// individual getter below. It used to live in a `static` local inside the shared
// helper, which -- because a function-local static is one object for the whole
// function, not one per argument value -- meant the FIRST getter to run populated
// the single cache and every later getter sharing that helper returned its value:
// getGoogleCSEContext() /
// getBingSubscriptionKey() would both hand back the Google
// CSE key. The per-getter statics below keep every property envConfig.h documents
// (read once, cached, no static-initialization-order fiasco, thread-safe
// initialization) and are correct per variable.

static string narrowEnv(const char* varName, const char* fallback)
{
	const char* env = getenv(varName);
	return (env && *env) ? string(env) : string(fallback);
}

static string requiredNarrowEnv(const char* varName, const lpchar_t* humanDescription)
{
	const char* env = getenv(varName);
	if (!env || !*env)
	{
		lplog(LOG_FATAL_ERROR, u"Required environment variable %S is not set (%s). "
			u"The hardcoded default that used to live in source has been removed - "
			u"set %S before running.", varName, humanDescription, varName);
		return string(); // unreachable: LOG_FATAL_ERROR aborts.
	}
	return string(env);
}

static lpwstring widePathEnv(const char* varName, const lpchar_t* fallback)
{
	const char* env = getenv(varName);
	if (env && *env)
		return lpwstring(lp_narrow_to_wide(string(env)));
	lplog(LOG_ERROR, u"Environment variable %S is not set; using the compile-time default %s", varName, fallback);
	return lpwstring(fallback);
}

static lpwstring requiredWideEnv(const char* varName, const lpchar_t* humanDescription)
{
	const char* env = getenv(varName);
	if (!env || !*env)
	{
		lplog(LOG_FATAL_ERROR, u"Required environment variable %S is not set (%s). "
			u"The hardcoded default that used to live in source has been removed - "
			u"set %S before running.", varName, humanDescription, varName);
		return lpwstring(); // unreachable: LOG_FATAL_ERROR aborts.
	}
	return lpwstring(lp_narrow_to_wide(string(env)));
}

const std::string& getDBUser()
{
	static const string value = narrowEnv("LP_DB_USER", "root");
	return value;
}

const std::string& getDBPassword()
{
	static const string value = requiredNarrowEnv("LP_DB_PASSWORD", u"MySQL password for LP_DB_USER");
	return value;
}


const lpwstring& getMainDir()
{
	static const lpwstring value = widePathEnv("LP_MAIN_DIR", LMAINDIR);
	return value;
}

const lpwstring& getCacheDir()
{
	static const lpwstring value = widePathEnv("LP_CACHE_DIR", CACHEDIR);
	return value;
}

const lpwstring& getWebSearchCacheDir()
{
	static const lpwstring value = widePathEnv("LP_WEBSEARCH_CACHE_DIR", WEBSEARCH_CACHEDIR);
	return value;
}

const lpwstring& getTextDir()
{
	static const lpwstring value = widePathEnv("LP_TEXT_DIR", TEXTDIR);
	return value;
}

const lpwstring& getGoogleCSEKey()
{
	static const lpwstring value = requiredWideEnv("LP_GOOGLE_CSE_KEY", u"Google Custom Search API key");
	return value;
}

const lpwstring& getGoogleCSEContext()
{
	static const lpwstring value = requiredWideEnv("LP_GOOGLE_CSE_CX", u"Google Custom Search engine id (cx)");
	return value;
}

const lpwstring& getBingSubscriptionKey()
{
	static const lpwstring value = requiredWideEnv("LP_BING_KEY", u"Bing v7 Ocp-Apim-Subscription-Key");
	return value;
}

const std::string& getStanfordClasspath()
{
	static const string value = []() -> string
	{
		const char* env = getenv("LP_STANFORD_CLASSPATH");
		if (env && *env) return string(env);
		// Not fatal, and not a logged error either: the Stanford VM is only created
		// by the HMM tagger comparison paths, so most runs never need this at all.
		// Separators are POSIX: ':' between classpath entries, '/' within a path.
		return "." + string(":") + lp_utf16_to_utf8(getMainDir()) +
			"/Stanford/workspace/StanfordParser/target/StanfordParser-0.0.1-SNAPSHOT.jar";
	}();
	return value;
}
