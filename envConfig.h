/*
	envConfig.h - environment-variable-backed credentials and filesystem roots

	Overview:
		Replaces the hardcoded MySQL password and third-party API keys that used
		to live directly in DB.cpp / DBCreateSQLSchema.cpp / getThesaurus.cpp /
		questionAnsweringWebSearch.cpp / getDictionary.cpp (see CODE_REVIEW.md,
		"Secrets in source"), and gives MAINDIR/CACHEDIR/WEBSEARCH_CACHEDIR/
		TEXTDIR an env-var override on top of their general.h compile-time
		defaults.

	Key entry points:
		- getDBUser() / getDBPassword() / getDBHost() - MySQL credentials.
		- getMainDir() / getCacheDir() / getWebSearchCacheDir() / getTextDir() -
			filesystem roots; fall back to the general.h macros with a logged
			error if the environment variable is not set.
		- getGoogleCSEKey() / getGoogleCSEContext() / getBingSubscriptionKey() /
			getMerriamWebsterKey() - third-party API credentials.

	Notes / gotchas:
		- Each getter reads its environment variable once and caches the result
			in a function-local static, so it is safe to call from another
			translation unit's global/dynamic initializers (no static-init-order
			fiasco) and cheap to call repeatedly.
		- Credential/key getters are fatal (lplog(LOG_FATAL_ERROR, ...)) the
			first time they are actually called with the variable unset - there is
			no safe default for a password or API key. Path getters fall back to
			the historical compile-time default and log a (non-fatal) error, since
			paths already have a working default for the author's own machine.
		- Rotating the actual leaked credentials on the real services (MySQL,
			Google Cloud Console, Azure/Bing, dictionaryapi.com) is not something
			this module can do - only removing the hardcoded literals from source.
*/
#pragma once
#include <string>

const std::string& getDBUser();
const std::string& getDBPassword();
const std::string& getDBHost();

const std::wstring& getMainDir();
const std::wstring& getCacheDir();
const std::wstring& getWebSearchCacheDir();
const std::wstring& getTextDir();

const std::wstring& getGoogleCSEKey();
const std::wstring& getGoogleCSEContext();
const std::wstring& getBingSubscriptionKey();
const std::wstring& getMerriamWebsterKey();
