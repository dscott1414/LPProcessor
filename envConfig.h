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
		- getDBUser() / getDBPassword() - MySQL credentials.
		- getMainDir() / getCacheDir() / getWebSearchCacheDir() / getTextDir() -
			filesystem roots; fall back to the general.h macros with a logged
			error if the environment variable is not set.
		- getGoogleCSEKey() / getGoogleCSEContext() / getBingSubscriptionKey() /
			third-party API credentials.

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
			Google Cloud Console, Azure/Bing) is not something
			this module can do - only removing the hardcoded literals from source.
*/
#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#include <string>

const std::string& getDBUser();
const std::string& getDBPassword();

const lpwstring& getMainDir();
const lpwstring& getCacheDir();
const lpwstring& getWebSearchCacheDir();
const lpwstring& getTextDir();

const lpwstring& getGoogleCSEKey();
const lpwstring& getGoogleCSEContext();
const lpwstring& getBingSubscriptionKey();

// Batch B11: the Java classpath used to create the JNI VM for the Stanford parser
// (hmm.cpp). Was a hardcoded Windows path,
// ".;F:\\lp\\Stanford\\workspace\\StanfordParser\\target\\StanfordParser-0.0.1-SNAPSHOT.jar".
// Override with LP_STANFORD_CLASSPATH; the default is the same jar relative to
// getMainDir(), with POSIX separators (':' between entries, '/' within a path).
const std::string& getStanfordClasspath();
