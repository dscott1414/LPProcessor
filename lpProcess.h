/*
	lpProcess.h - portable process, signal and progress-reporting primitives

	Overview:
		Batch B4a. These are the replacements for the Win32 process/console layer
		main.cpp and specials_main.cpp were built on: CreateProcess,
		WaitForMultipleObjectsEx, GenerateConsoleCtrlEvent, SetConsoleTitle, and
		the hardcoded relative paths to sibling build outputs.

		They live in their own translation unit rather than inside main.cpp for two
		reasons. First, main.cpp and specials_main.cpp already duplicate a great deal
		of each other (see specials_main.cpp's own "Drift vs main.cpp" header note,
		which lists places where the two copies have silently diverged); the
		controller-mode plumbing is exactly the kind of thing that should not be
		copied a third time. Second, nothing here depends on word.h/source.h, so this
		file compiles and is testable on its own -- see lpprocess_smoketest.cpp --
		well before the rest of the engine does.

	Key entry points:
		- lpExecutableDirectory() / lpSiblingExecutablePath() - locate the running
			binary and its siblings.
		- lpSpawnProcess() - posix_spawn wrapper taking a real argument vector.
		- lpWaitForAnyChildProcess() - wait, with a deadline, for any of N children.
		- lpSignalInterrupt() - ask one child to stop at the end of its document.
		- lpReportProgress() - the single progress-status helper.

	Notes / gotchas:
		- lpWaitForAnyChildProcess polls; see its own comment for why that is the
			right call here and not a compromise.
		- lpReportProgress writes an OSC 0 terminal-title escape and is a no-op when
			stdout is not a tty.
*/
#pragma once
#include "lpchar.h"
#include <string>
#include <vector>
#include <unistd.h>

// Directory holding the running executable, resolved through _NSGetExecutablePath
// (the only reliable way to find it on macOS -- argv[0] may be a bare name found on
// PATH, or a symlink) and then realpath()'d so a symlinked launch still yields the
// real build directory. Empty string if either step fails. Cached: the answer
// cannot change while the process runs.
const std::string& lpExecutableDirectory();

// Absolute path to a binary sitting next to this one, replacing the hardcoded
// relative paths controller mode used to spawn ("QuestionAnsweringx64\lp.exe",
// "ParseAllSourcesx64\lp.exe", "x64\StanfordAllSources\CorpusAnalysis.exe").
//
// Those three were separate Windows build outputs of only two distinct programs --
// two differently-configured builds of lp.exe plus CorpusAnalysis.exe -- and which
// one ran was decided entirely by the command-line arguments, not by the build. The
// CMake build produces one lp and one CorpusAnalysis in a single directory, so the
// correct target is simply the sibling of whatever is running, which also means
// controller mode no longer depends on being launched from the root of a full build
// tree. Falls back to the bare name (i.e. a PATH lookup by the caller) if the
// executable directory could not be determined.
std::string lpSiblingExecutablePath(const char* executableName);

// posix_spawn wrapper. arguments[0] must be the executable path itself, per
// convention. Out: childPid. Returns 0 on success, -1 on failure (childPid
// untouched, and the failure is printed along with the working directory, since a
// wrong working directory is the usual cause).
//
// Takes a real argument vector rather than one command-line string, unlike the
// CreateProcess call it replaces: nothing here can truncate, and an argument
// containing a space (a cache directory path, say) cannot be silently split in two.
int lpSpawnProcess(pid_t& childPid, const std::vector<std::string>& arguments);

// Wait up to timeoutMilliseconds for any of the numPids children to exit. On
// success returns its index in pids[] and sets exitStatus to waitpid's raw status.
// Entries <= 0 in pids[] are skipped, so a slot whose spawn failed is simply not
// waited on.
const int LP_WAIT_TIMEOUT = -1; // nothing exited before the deadline
const int LP_WAIT_FAILED = -2;  // nothing left to wait for
int lpWaitForAnyChildProcess(const pid_t* pids, int numPids, int timeoutMilliseconds, int& exitStatus);

// Ask one child to stop at the end of its current document rather than killing it;
// the child's own interrupt handler provides the two-stage semantics. Returns true
// if the signal was delivered.
bool lpSignalInterrupt(pid_t processId);

// The single progress-reporting helper, replacing every SetConsoleTitle() call site.
//
// SetConsoleTitle put a continuously-updated status line somewhere that did not
// scroll away. The exact equivalent on a terminal is the OSC 0 escape sequence,
// which Terminal.app, iTerm2 and every other common macOS terminal honour by
// setting the window/tab title. Written only when stdout is a tty: with output
// redirected to a file the escape would just be garbage in the middle of the log,
// which matches the old behaviour of ignoring console-API failures under
// redirection.
void lpReportProgress(const lpchar_t* progressText);

// The sanitizing lpReportProgress applies to a title before wrapping it in the
// escape sequence: control characters are dropped rather than passed through, since
// an embedded newline or BEL would terminate the escape early and leave the rest of
// the string interpreted as terminal commands. Titles are built from document titles
// taken out of the corpus, so this is real input sanitizing, not paranoia. Exposed
// separately so it can be tested directly.
std::string lpSanitizeProgressText(const std::string& utf8Text);
