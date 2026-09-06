/*
	lpfile_smoketest.cpp - proves lpNarrowPath() joins this codebase's '\'-separated
	path literals onto a POSIX root and reaches the real file.

	Why this exists:
		Paths here are built with '\' -- 123 string literals of the form
		u"%s\\dbPediaCache\\_%s.txt", plus distributeToSubDirectories(), which writes
		'\' into path[1] and path[3] by hand. On Windows that was the separator; on
		macOS '\' is an ordinary filename character. lpNarrowPath() translates at the
		single point where a wide path becomes the bytes the OS wants, which is what
		lets the roots in general.h be POSIX while those 123 literals stay untouched.

		That translation is invisible when it breaks: every open() simply misses, the
		engine treats a miss as "not cached yet", and a run silently re-fetches
		everything instead of reading the 178 GB of caches. Hence a real
		compiled-and-run check rather than a reading pass.

	Build and run:
		cmake --build <builddir> --target lpfile_smoketest
		<builddir>/bin/lpfile_smoketest

	Notes:
		- The cache root comes from LP_CACHE_DIR, falling back to general.h's
			CACHEDIR default, so this tracks whatever the program itself would use.
		- File-opening checks are skipped (not failed) when the caches are not
			present, so this stays useful on a machine that has only the source tree.
		- Run it from the main directory if you want the relative-path check, which
			mirrors where initialize()'s chdir("..") leaves the process.
*/
#include <string>
// Matches the codebase-wide convention (word.h does this before its own
// #include "general.h"): general.h's declarations are written unqualified.
using namespace std;
#include "general.h"   // CACHEDIR -- so this test tracks the real default root
#include "lpFile.h"
#include "lpchar.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static int checks = 0, failures = 0, skipped = 0;

static void expectPath(const lpwstring &wide, const std::string &expected, const char *what)
{
	checks++;
	std::string got = lpNarrowPath(wide);
	if (got != expected)
	{
		printf("  FAIL  %s\n        got      '%s'\n        expected '%s'\n", what, got.c_str(), expected.c_str());
		failures++;
	}
	else
		printf("  ok    %s\n", what);
}

// Opens a file that must exist. Skips (does not fail) when the cache tree is absent.
static void expectOpens(const lpwstring &wide, const char *what, bool cachesPresent)
{
	std::string narrow = lpNarrowPath(wide);
	if (!cachesPresent)
	{
		skipped++;
		printf("  skip  %s (caches not present)\n", what);
		return;
	}
	checks++;
	int fd = lp_wopen(wide.c_str(), O_RDONLY);
	if (fd < 0)
	{
		printf("  FAIL  %s did not open: %s\n", what, narrow.c_str());
		failures++;
		return;
	}
	char head[64];
	ssize_t bytesRead = ::read(fd, head, sizeof(head));
	::close(fd);
	if (bytesRead <= 0)
	{
		printf("  FAIL  %s opened but read nothing: %s\n", what, narrow.c_str());
		failures++;
		return;
	}
	printf("  ok    %s\n", what);
}

static bool isDirectory(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

int main()
{
	// Track whatever root the program itself would use.
	const char *envRoot = getenv("LP_CACHE_DIR");
	std::string root = (envRoot && *envRoot) ? std::string(envRoot) : lpNarrowPath(lpwstring(CACHEDIR));
	lpwstring wideRoot = lpwstring(lp_narrow_to_wide(root));
	printf("cache root: %s\n\n", root.c_str());

	printf("=== separator translation ===\n");
	// getWordNet.cpp:1213 builds exactly this shape.
	expectPath(wideRoot + u"\\wordNetCache\\samarang", root + "/wordNetCache/samarang",
	           "root + backslash-separated literal");
	// The two separators distributeToSubDirectories() inserts by hand.
	expectPath(wideRoot + u"\\dbPediaCache\\D\\!\\_D.eRdfTypes", root + "/dbPediaCache/D/!/_D.eRdfTypes",
	           "fan-out separators from distributeToSubDirectories");
	// An already-POSIX path must pass through untouched.
	expectPath(wideRoot, root, "POSIX path unchanged");
	// Both spellings must be able to mix, which is what makes a POSIX root safe.
	expectPath(wideRoot + u"/texts\\The Art of War.txt", root + "/texts/The Art of War.txt",
	           "mixed separators");
	// A relative path keeps its shape; only separators change.
	expectPath(lpwstring(u"source\\lists\\nounVerbMapping"), "source/lists/nounVerbMapping",
	           "relative path");
	// Backslash is the only thing rewritten -- '/' and ordinary characters survive.
	expectPath(lpwstring(u"a/b\\c d!e"), "a/b/c d!e", "only backslashes are rewritten");

	printf("\n=== real files opened through the lp_w* layer ===\n");
	bool haveCaches = isDirectory(root);
	expectOpens(wideRoot + u"\\wordNetCache\\samarang", "wordNetCache entry",
	            haveCaches && isDirectory(root + "/wordNetCache"));
	expectOpens(wideRoot + u"\\texts\\The Art of War.txt", "texts entry",
	            haveCaches && isDirectory(root + "/texts"));
	expectOpens(wideRoot + u"\\gutenbergCatalog\\pg35171.rdf", "gutenbergCatalog entry",
	            haveCaches && isDirectory(root + "/gutenbergCatalog"));
	// Mirrors where initialize()'s chdir("..") leaves the process.
	expectOpens(lpwstring(u"source\\lists\\nounVerbMapping"), "source/lists (relative to main dir)",
	            isDirectory("source/lists"));

	printf("\n");
	if (skipped)
		printf("%d check(s) skipped because their tree was not present.\n", skipped);
	if (failures)
	{
		printf("=== %d of %d CHECKS FAILED ===\n", failures, checks);
		return 1;
	}
	printf("=== ALL %d CHECKS PASSED ===\n", checks);
	return 0;
}
