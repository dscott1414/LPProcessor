// envConfig.cpp - see envConfig.h for the overview.
#pragma warning (disable: 4996) // getenv/_wgetenv "unsafe" - matches the rest of the tree's CRT usage.
#include <string>
#include "logging.h"
#include "general.h"
#include "envConfig.h"

static const std::string& cachedNarrowEnv(const char* varName, const char* fallback)
{
	static std::string value = [&]() -> std::string
	{
		const char* env = getenv(varName);
		return (env && *env) ? std::string(env) : std::string(fallback);
	}();
	return value;
}

static const std::string& requiredNarrowEnv(const char* varName, const wchar_t* humanDescription)
{
	static std::string value = [&]() -> std::string
	{
		const char* env = getenv(varName);
		if (!env || !*env)
		{
			lplog(LOG_FATAL_ERROR, L"Required environment variable %S is not set (%s). "
				L"The hardcoded default that used to live in source has been removed - "
				L"set %S before running.", varName, humanDescription, varName);
			return std::string(); // unreachable: LOG_FATAL_ERROR aborts.
		}
		return std::string(env);
	}();
	return value;
}

const std::string& getDBUser()
{
	return cachedNarrowEnv("LP_DB_USER", "root");
}

const std::string& getDBPassword()
{
	return requiredNarrowEnv("LP_DB_PASSWORD", L"MySQL password for LP_DB_USER");
}

const std::string& getDBHost()
{
	return cachedNarrowEnv("LP_DB_HOST", "localhost");
}

static const std::wstring& widePathEnv(const wchar_t* varName, const wchar_t* fallback)
{
	static std::wstring value = [&]() -> std::wstring
	{
		const wchar_t* env = _wgetenv(varName);
		if (env && *env)
			return std::wstring(env);
		lplog(LOG_ERROR, L"Environment variable %s is not set; using the compile-time default %s", varName, fallback);
		return std::wstring(fallback);
	}();
	return value;
}

const std::wstring& getMainDir()
{
	return widePathEnv(L"LP_MAIN_DIR", LMAINDIR);
}

const std::wstring& getCacheDir()
{
	return widePathEnv(L"LP_CACHE_DIR", CACHEDIR);
}

const std::wstring& getWebSearchCacheDir()
{
	return widePathEnv(L"LP_WEBSEARCH_CACHE_DIR", WEBSEARCH_CACHEDIR);
}

const std::wstring& getTextDir()
{
	return widePathEnv(L"LP_TEXT_DIR", TEXTDIR);
}

static const std::wstring& requiredWideEnv(const wchar_t* varName, const wchar_t* humanDescription)
{
	static std::wstring value = [&]() -> std::wstring
	{
		const wchar_t* env = _wgetenv(varName);
		if (!env || !*env)
		{
			lplog(LOG_FATAL_ERROR, L"Required environment variable %s is not set (%s). "
				L"The hardcoded default that used to live in source has been removed - "
				L"set %s before running.", varName, humanDescription, varName);
			return std::wstring(); // unreachable: LOG_FATAL_ERROR aborts.
		}
		return std::wstring(env);
	}();
	return value;
}

const std::wstring& getGoogleCSEKey()
{
	return requiredWideEnv(L"LP_GOOGLE_CSE_KEY", L"Google Custom Search API key");
}

const std::wstring& getGoogleCSEContext()
{
	return requiredWideEnv(L"LP_GOOGLE_CSE_CX", L"Google Custom Search engine id (cx)");
}

const std::wstring& getBingSubscriptionKey()
{
	return requiredWideEnv(L"LP_BING_KEY", L"Bing v7 Ocp-Apim-Subscription-Key");
}

const std::wstring& getMerriamWebsterKey()
{
	return requiredWideEnv(L"LP_MERRIAM_WEBSTER_KEY", L"Merriam-Webster Collegiate API key");
}
