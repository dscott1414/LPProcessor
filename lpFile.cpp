// lpFile.cpp - see lpFile.h for the overview. Batch B5.
#include "lpFile.h"
#include "utfConvert.h"
#include <dirent.h>
#include <fnmatch.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

// One place where a wide path becomes the bytes the OS wants.
static inline std::string narrowPath(const lpwstring& path)
{
	return std::string(lp_utf16_to_utf8(path));
}

FILE* lp_wfopen(const lpchar_t* path, const char* mode)
{
	if (!path) return nullptr;
	return fopen(narrowPath(lpwstring(path)).c_str(), mode);
}
FILE* lp_wfopen(const lpwstring& path, const char* mode)
{
	return fopen(narrowPath(path).c_str(), mode);
}

int lp_wremove(const lpchar_t* path)
{
	if (!path) return -1;
	return remove(narrowPath(lpwstring(path)).c_str());
}
int lp_wremove(const lpwstring& path)
{
	return remove(narrowPath(path).c_str());
}

int lp_waccess(const lpchar_t* path, int mode)
{
	if (!path) return -1;
	return access(narrowPath(lpwstring(path)).c_str(), mode);
}
int lp_waccess(const lpwstring& path, int mode)
{
	return access(narrowPath(path).c_str(), mode);
}

// MSVC's _wmkdir takes no mode; 0777 (masked by the process umask, as always) is
// the conventional POSIX equivalent for "create it with default permissions".
int lp_wmkdir(const lpchar_t* path)
{
	if (!path) return -1;
	return mkdir(narrowPath(lpwstring(path)).c_str(), 0777);
}
int lp_wmkdir(const lpwstring& path)
{
	return mkdir(narrowPath(path).c_str(), 0777);
}

int lp_wrename(const lpchar_t* from, const lpchar_t* to)
{
	if (!from || !to) return -1;
	return rename(narrowPath(lpwstring(from)).c_str(), narrowPath(lpwstring(to)).c_str());
}

int lp_wstat(const lpchar_t* path, struct stat* out)
{
	if (!path || !out) return -1;
	return stat(narrowPath(lpwstring(path)).c_str(), out);
}
int lp_wstat(const lpwstring& path, struct stat* out)
{
	if (!out) return -1;
	return stat(narrowPath(path).c_str(), out);
}

int lp_wopen(const lpchar_t* path, int flags, int mode)
{
	if (!path) return -1;
	return open(narrowPath(lpwstring(path)).c_str(), flags, mode);
}
int lp_wopen(const lpwstring& path, int flags, int mode)
{
	return open(narrowPath(path).c_str(), flags, mode);
}

bool lp_wIsDirectory(const lpwstring& path)
{
	struct stat fileStatus;
	if (stat(narrowPath(path).c_str(), &fileStatus) != 0) return false;
	return S_ISDIR(fileStatus.st_mode);
}

static std::vector<lpwstring> directoryEntries(const lpwstring& directory, const lpwstring& pattern,
	bool filterOnType, bool wantDirectories)
{
	std::vector<lpwstring> entries;
	std::string narrowDirectory = narrowPath(directory);
	if (narrowDirectory.empty()) narrowDirectory = ".";
	DIR* dir = opendir(narrowDirectory.c_str());
	if (!dir) return entries; // missing/unreadable: same as a failed FindFirstFile

	// Win32's "*.*" means "everything", including names with no dot at all --
	// fnmatch would read it literally as "must contain a dot", so it is translated
	// rather than passed through.
	std::string narrowPattern = narrowPath(pattern);
	if (narrowPattern.empty() || narrowPattern == "*.*") narrowPattern = "*";

	for (struct dirent* entry = readdir(dir); entry; entry = readdir(dir))
	{
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
		if (fnmatch(narrowPattern.c_str(), entry->d_name, 0) != 0) continue;
		if (filterOnType)
		{
			bool isDirectory;
			if (entry->d_type == DT_DIR) isDirectory = true;
			else if (entry->d_type == DT_UNKNOWN)
			{
				// Some filesystems do not fill in d_type; fall back to stat.
				struct stat fileStatus;
				std::string full = narrowDirectory + "/" + entry->d_name;
				if (stat(full.c_str(), &fileStatus) != 0) continue;
				isDirectory = S_ISDIR(fileStatus.st_mode);
			}
			else isDirectory = false;
			if (isDirectory != wantDirectories) continue;
		}
		entries.push_back(lpwstring(lp_narrow_to_wide(std::string(entry->d_name))));
	}
	closedir(dir);
	return entries;
}

std::vector<lpwstring> lpDirectoryEntries(const lpwstring& directory, const lpwstring& pattern)
{
	return directoryEntries(directory, pattern, false, false);
}

std::vector<lpwstring> lpDirectoryEntries(const lpwstring& directory, const lpwstring& pattern, bool wantDirectories)
{
	return directoryEntries(directory, pattern, true, wantDirectories);
}

long lp_filelength(int fd)
{
	struct stat fileStatus;
	if (fstat(fd, &fileStatus) != 0) return -1;
	return (long)fileStatus.st_size;
}
