// lpProcess.cpp - see lpProcess.h for the overview. Batch B4a.
#include "lpProcess.h"
#include "utfConvert.h"
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <mach-o/dyld.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" char** environ;

const std::string& lpExecutableDirectory()
{
	static const std::string directory = []() -> std::string
	{
		uint32_t size = 0;
		_NSGetExecutablePath(nullptr, &size); // fails, but fills in the needed length
		if (size == 0) return std::string();
		std::string path(size, '\0');
		if (_NSGetExecutablePath(&path[0], &size) != 0)
			return std::string();
		char resolved[PATH_MAX];
		if (!realpath(path.c_str(), resolved))
			return std::string();
		std::string full(resolved);
		size_t lastSlash = full.rfind('/');
		return (lastSlash == std::string::npos) ? std::string() : full.substr(0, lastSlash);
	}();
	return directory;
}

std::string lpSiblingExecutablePath(const char* executableName)
{
	const std::string& directory = lpExecutableDirectory();
	if (directory.empty()) return std::string(executableName);
	return directory + "/" + executableName;
}

int lpSpawnProcess(pid_t& childPid, const std::vector<std::string>& arguments)
{
	if (arguments.empty()) return -1;
	std::vector<char*> argv;
	argv.reserve(arguments.size() + 1);
	for (const std::string& argument : arguments)
		argv.push_back(const_cast<char*>(argument.c_str()));
	argv.push_back(nullptr);

	pid_t spawnedPid = 0;
	int spawnError = posix_spawn(&spawnedPid, arguments[0].c_str(), nullptr, nullptr, argv.data(), environ);
	if (spawnError != 0)
	{
		char cwd[1024];
		if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "<unknown>");
		printf("posix_spawn of %s failed (%d) %s in %s.\n", arguments[0].c_str(), spawnError, strerror(spawnError), cwd);
		return -1;
	}
	childPid = spawnedPid;
	return 0;
}

// Implemented by polling waitpid(WNOHANG) rather than blocking, because a blocking
// waitpid cannot be given a deadline. That is fine here, and deliberately chosen
// over the alternatives (a kqueue EVFILT_PROC/NOTE_EXIT loop, or SIGALRM to
// interrupt a blocking wait): the only reason these waits have a timeout at all is
// to refresh the progress title every few minutes, so a quarter-second poll is
// three orders of magnitude finer than anything depending on it, and it costs one
// cheap syscall per child per interval.
//
// waitpid is asked about each pid specifically rather than passing -1, so this
// cannot swallow the exit of some other child the caller is not tracking.
int lpWaitForAnyChildProcess(const pid_t* pids, int numPids, int timeoutMilliseconds, int& exitStatus)
{
	if (!pids || numPids <= 0) return LP_WAIT_FAILED;
	const int pollIntervalMilliseconds = 250;
	for (int waited = 0; ; waited += pollIntervalMilliseconds)
	{
		bool anyStillLive = false;
		for (int index = 0; index < numPids; index++)
		{
			if (pids[index] <= 0) continue; // empty slot, or a spawn that failed
			int status = 0;
			pid_t result = waitpid(pids[index], &status, WNOHANG);
			if (result == pids[index])
			{
				exitStatus = status;
				return index;
			}
			if (result == 0 || (result < 0 && errno == EINTR))
				anyStillLive = true;
			// result < 0 with any other errno means this pid is not ours to wait on
			// (already reaped, or never a child): it can never complete, so it does
			// not count as live -- otherwise a stale entry would keep this loop
			// spinning until the timeout on every call.
		}
		if (!anyStillLive) return LP_WAIT_FAILED;
		if (waited >= timeoutMilliseconds) return LP_WAIT_TIMEOUT;
		usleep(pollIntervalMilliseconds * 1000);
	}
}

// One line, where Windows needed forty: the controller there had to leave its own
// console, attach to the child's, permanently disable its own Ctrl-C handling
// (never restored - restoring it would kill the controller too), raise the event for
// the whole attached console group, then reattach or allocate a fresh console. On
// POSIX, signalling another process is the native operation.
bool lpSignalInterrupt(pid_t processId)
{
	return kill(processId, SIGINT) == 0;
}

std::string lpSanitizeProgressText(const std::string& utf8Text)
{
	std::string sanitized;
	sanitized.reserve(utf8Text.size());
	for (char ch : utf8Text)
		if ((unsigned char)ch >= 0x20 && ch != 0x7f)
			sanitized += ch;
	return sanitized;
}

void lpReportProgress(const lpchar_t* progressText)
{
	if (!progressText || !isatty(STDOUT_FILENO)) return;
	std::string sanitized = lpSanitizeProgressText(lp_utf16_to_utf8(lpwstring(progressText)));
	printf("\033]0;%s\007", sanitized.c_str());
	fflush(stdout);
}
