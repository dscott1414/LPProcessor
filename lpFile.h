/*
	lpFile.h - portable wide-path filesystem calls

	Overview:
		Batch B5. MSVC has a parallel "wide" CRT (_wfopen, _wremove, _waccess,
		_wmkdir, _wrename, _wstat) taking wchar_t paths; POSIX has exactly one
		filesystem API and it takes bytes. Every one of those call sites needs the
		same two-line dance -- encode the lpwstring path to UTF-8, then call the
		ordinary function -- so it lives here once instead of 76 times.

		The Win32 directory-enumeration family (FindFirstFileW / FindNextFileW /
		FindClose, driving a WIN32_FIND_DATA loop) is also replaced here, by a single
		lpDirectoryEntries() that returns the matching names up front. That shape
		change is deliberate: every real call site in this codebase was already
		collecting names into a container, and returning a vector removes the
		handle-leak-on-early-return hazard the Find* loops all carried.

	Notes / gotchas:
		- Paths are encoded as UTF-8, which is what macOS expects. The filesystem
			normalizes to NFD on HFS+/APFS; that is invisible to open() but means a
			path read back from a directory listing may not compare equal, byte for
			byte, to the one that was written. Only affects code comparing paths as
			strings, not code opening them.
		- lpDirectoryEntries returns bare entry names (not full paths), matching
			WIN32_FIND_DATA::cFileName, so existing call sites that build
			directory + "/" + name keep working unchanged.
		- "." and ".." are never returned, matching what every call site here
			immediately filtered out by hand.
*/
#pragma once
#include "lpchar.h"
#include <string>
#include <vector>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

// MSVC's MAX_PATH was 260. macOS's limit is PATH_MAX (1024), and several buffers
// in this codebase were sized against the Windows value; where a buffer is declared
// with this constant it is now the larger, correct one for this platform.
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

// macOS has no text/binary distinction for file streams, so MSVC's O_BINARY (and
// its _S_IREAD/_S_IWRITE permission spellings) have no meaning here. O_BINARY is
// defined as 0 so the existing `| O_BINARY` in open() flag lists stays valid and
// becomes a no-op, rather than every one of those call sites needing an edit.
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef _S_IREAD
#define _S_IREAD S_IRUSR
#endif
#ifndef _S_IWRITE
#define _S_IWRITE S_IWUSR
#endif

// Batch B5: the handful of Win32 error codes this codebase compares errno against.
// GetLastError() became errno in the same sweep, so these are mapped to the errno
// values that mean the same thing. ERROR_PATH_NOT_FOUND and ERROR_FILE_NOT_FOUND
// are distinct on Windows but are both ENOENT here, which is correct: POSIX does
// not distinguish "a directory in the path is missing" from "the final component
// is missing".
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND ENOENT
#endif
#ifndef ERROR_PATH_NOT_FOUND
#define ERROR_PATH_NOT_FOUND ENOENT
#endif
#ifndef ERROR_INSUFFICIENT_BUFFER
#define ERROR_INSUFFICIENT_BUFFER ERANGE
#endif
#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS EEXIST
#endif

// Direct replacements for the MSVC wide CRT. Each encodes the path to UTF-8 and
// calls the ordinary POSIX/C function; return values and errno behaviour are those
// of the underlying call.
FILE* lp_wfopen(const lpchar_t* path, const char* mode);
FILE* lp_wfopen(const lpwstring& path, const char* mode);
int lp_wremove(const lpchar_t* path);
int lp_wremove(const lpwstring& path);
int lp_waccess(const lpchar_t* path, int mode);
int lp_waccess(const lpwstring& path, int mode);
int lp_wmkdir(const lpchar_t* path);
int lp_wmkdir(const lpwstring& path);
int lp_wrename(const lpchar_t* from, const lpchar_t* to);
int lp_wstat(const lpchar_t* path, struct stat* out);
int lp_wstat(const lpwstring& path, struct stat* out);

// open(2) on a wide path. Mode is only consulted when O_CREAT is in flags.
int lp_wopen(const lpchar_t* path, int flags, int mode = 0666);
int lp_wopen(const lpwstring& path, int flags, int mode = 0666);

// Replacement for MSVC's filelength(fd): the size in bytes of an open descriptor,
// or -1 on failure. (POSIX has no filelength; this is fstat.)
long lp_filelength(int fd);

// True if the path exists and is a directory.
bool lp_wIsDirectory(const lpwstring& path);

// Replacement for the FindFirstFileW/FindNextFileW/FindClose loop. Returns the
// names of the entries in `directory` matching the glob `pattern` (fnmatch syntax,
// so the Win32 "*.*" and "*" patterns both behave as expected), excluding "." and
// "..". An unreadable or missing directory yields an empty vector, which is how
// every call site here already treated a failed FindFirstFile.
std::vector<lpwstring> lpDirectoryEntries(const lpwstring& directory, const lpwstring& pattern);

// Convenience: the same, but returning only entries that are (or are not)
// directories. Several call sites recursed into subdirectories and needed the
// distinction that WIN32_FIND_DATA::dwFileAttributes used to provide.
std::vector<lpwstring> lpDirectoryEntries(const lpwstring& directory, const lpwstring& pattern, bool wantDirectories);
