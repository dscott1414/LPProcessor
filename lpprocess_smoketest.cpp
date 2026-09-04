/*
	lpprocess_smoketest.cpp - compiled-and-run checks for batch B4a's lpProcess.cpp

	Deliberately independent of lpcore (which does not compile until batch B14), in
	the same spirit as lpchar_smoketest.cpp: it links lpProcess.cpp, utfConvert.cpp
	and lpchar.cpp only. Everything here is a real assertion against real behaviour --
	processes are actually spawned, actually signalled, and actually reaped.

	Build and run with:
	  cmake --build <builddir> --target lpprocess_smoketest
	  <builddir>/bin/lpprocess_smoketest
*/
#include "lpProcess.h"
#include "utfConvert.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <time.h>

static int checks = 0;
static int failures = 0;

static void check(bool condition, const char* what)
{
	checks++;
	if (!condition)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static int64_t nowMilliseconds()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ---------------------------------------------------------------------------
static void testExecutableDiscovery()
{
	const std::string& directory = lpExecutableDirectory();
	check(!directory.empty(), "lpExecutableDirectory() is not empty");
	check(directory[0] == '/', "lpExecutableDirectory() is absolute");
	// This test binary lives in the same directory the port expects lp and
	// CorpusAnalysis to be in, so its own name must resolve as a sibling.
	std::string self = lpSiblingExecutablePath("lpprocess_smoketest");
	check(self == directory + "/lpprocess_smoketest", "sibling path is directory + name");
	check(access(self.c_str(), X_OK) == 0, "the sibling path we computed for ourselves is executable");
	printf("testExecutableDiscovery: %s\n", directory.c_str());
}

// ---------------------------------------------------------------------------
static void testSpawnAndExitCode()
{
	pid_t child = 0;
	// /bin/sh -c 'exit 7' is a portable "exit with a known code".
	std::vector<std::string> arguments = { "/bin/sh", "-c", "exit 7" };
	check(lpSpawnProcess(child, arguments) == 0, "lpSpawnProcess succeeds for /bin/sh");
	check(child > 0, "lpSpawnProcess yields a positive pid");

	int exitStatus = 0;
	int index = lpWaitForAnyChildProcess(&child, 1, 10000, exitStatus);
	check(index == 0, "the one child is reported at index 0");
	check(WIFEXITED(exitStatus), "child exited normally");
	check(WEXITSTATUS(exitStatus) == 7, "child's exit code came through as 7");
}

static void testSpawnFailureIsReported()
{
	pid_t child = -1;
	std::vector<std::string> arguments = { "/nonexistent/definitely/not/here", "arg" };
	printf("  (one expected posix_spawn failure message follows)\n  ");
	check(lpSpawnProcess(child, arguments) == -1, "spawning a missing executable fails");
	check(child == -1, "childPid is left untouched on failure");

	pid_t none = 0;
	int status = 0;
	check(lpSpawnProcess(none, std::vector<std::string>()) == -1, "spawning with no arguments fails");
	check(lpWaitForAnyChildProcess(nullptr, 0, 0, status) == LP_WAIT_FAILED, "waiting on nothing reports LP_WAIT_FAILED");
}

// An argument containing spaces must arrive at the child as ONE argument. This is
// the concrete regression the argv rewrite exists to prevent: the old
// command-line-string form would have split it.
static void testArgumentWithSpacesStaysOneArgument()
{
	pid_t child = 0;
	// $1 is the single argument; exit 0 only if it is intact and unsplit.
	std::vector<std::string> arguments = {
		"/bin/sh", "-c",
		"test \"$1\" = \"a b c\" && test $# -eq 1", "sh", "a b c"
	};
	check(lpSpawnProcess(child, arguments) == 0, "spawn with a spaced argument succeeds");
	int exitStatus = 0;
	lpWaitForAnyChildProcess(&child, 1, 10000, exitStatus);
	check(WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus) == 0,
		"an argument containing spaces arrives as exactly one argument");
}

// ---------------------------------------------------------------------------
static void testWaitPicksTheRightChildAndTimesOut()
{
	pid_t children[3] = { 0, 0, 0 };
	std::vector<std::string> slow = { "/bin/sh", "-c", "sleep 30" };
	std::vector<std::string> quick = { "/bin/sh", "-c", "sleep 1; exit 3" };
	check(lpSpawnProcess(children[0], slow) == 0, "spawned slow child 0");
	check(lpSpawnProcess(children[1], quick) == 0, "spawned quick child 1");
	check(lpSpawnProcess(children[2], slow) == 0, "spawned slow child 2");

	int exitStatus = 0;
	int index = lpWaitForAnyChildProcess(children, 3, 15000, exitStatus);
	check(index == 1, "the child that actually exited is the one reported");
	check(WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus) == 3, "its exit code is correct");

	// The other two are still running, so a short deadline must time out rather
	// than report a bogus index.
	children[1] = 0; // reaped
	int64_t began = nowMilliseconds();
	index = lpWaitForAnyChildProcess(children, 3, 500, exitStatus);
	int64_t elapsed = nowMilliseconds() - began;
	check(index == LP_WAIT_TIMEOUT, "a deadline with no exit reports LP_WAIT_TIMEOUT");
	check(elapsed >= 400 && elapsed < 4000, "the timeout is honoured approximately");

	// lpSignalInterrupt must actually reach the children. `sleep` dies on SIGINT.
	check(lpSignalInterrupt(children[0]), "lpSignalInterrupt delivers to child 0");
	check(lpSignalInterrupt(children[2]), "lpSignalInterrupt delivers to child 2");
	for (int remaining = 0; remaining < 2; remaining++)
	{
		index = lpWaitForAnyChildProcess(children, 3, 10000, exitStatus);
		check(index == 0 || index == 2, "a signalled child is reaped");
		if (index >= 0)
		{
			check(WIFSIGNALED(exitStatus) && WTERMSIG(exitStatus) == SIGINT,
				"the signalled child reports death by SIGINT");
			children[index] = 0;
		}
	}
	check(lpWaitForAnyChildProcess(children, 3, 0, exitStatus) == LP_WAIT_FAILED,
		"once every slot is empty, waiting reports LP_WAIT_FAILED");
}

// A slot holding a pid that is not ours must not keep the wait spinning to the
// full deadline -- it can never complete, so it should not count as live.
static void testStalePidDoesNotBlockTheDeadline()
{
	pid_t children[1] = { 999999 }; // not a child of ours
	int exitStatus = 0;
	int64_t began = nowMilliseconds();
	int index = lpWaitForAnyChildProcess(children, 1, 60000, exitStatus);
	int64_t elapsed = nowMilliseconds() - began;
	check(index == LP_WAIT_FAILED, "a non-child pid reports LP_WAIT_FAILED");
	check(elapsed < 2000, "and does so immediately, not after the full 60s deadline");
}

// ---------------------------------------------------------------------------
static void testProgressSanitizing()
{
	check(lpSanitizeProgressText("plain title") == "plain title", "plain text is unchanged");
	check(lpSanitizeProgressText("a\nb") == "ab", "a newline is dropped");
	check(lpSanitizeProgressText("a\007b") == "ab", "a BEL (which would end the escape) is dropped");
	check(lpSanitizeProgressText("a\033]0;evil\007b") == "a]0;evilb", "an embedded escape loses its ESC");
	check(lpSanitizeProgressText("a\x7f" "b") == "ab", "DEL is dropped");
	// Non-ASCII UTF-8 must survive: every continuation byte is >= 0x80.
	check(lpSanitizeProgressText("caf\xc3\xa9") == "caf\xc3\xa9", "UTF-8 multibyte text survives");
	// And the whole path from lpchar_t through to the sanitizer.
	check(lpSanitizeProgressText(lp_utf16_to_utf8(lpwstring(u"Moby\nDick"))) == "MobyDick",
		"lpchar_t title with a newline is sanitized end to end");
	lpReportProgress(u"lpprocess_smoketest"); // no-op unless run on a tty; must not crash
	lpReportProgress(nullptr);               // must tolerate NULL
	checks += 2;
}

int main()
{
	testExecutableDiscovery();
	testSpawnAndExitCode();
	testSpawnFailureIsReported();
	testArgumentWithSpacesStaysOneArgument();
	testWaitPicksTheRightChildAndTimesOut();
	testStalePidDoesNotBlockTheDeadline();
	testProgressSanitizing();

	if (failures)
		printf("\n=== %d of %d CHECKS FAILED ===\n", failures, checks);
	else
		printf("\n=== ALL %d CHECKS PASSED ===\n", checks);
	return failures != 0;
}
