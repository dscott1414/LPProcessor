/*
	DIYDiskArray.h - 2-D array of T that lives either in RAM or as a seekable disk file

	Overview:
		Template used for matrices too large to keep in RAM (e.g. pairwise scores).
		If constructed with a non-NULL path, initialize() creates/opens the file and
		pre-fills it with 'value'; put/get lseek+read/write one T.  If path is NULL,
		storage is vector<vector<T>>.  The unused 'check' flag mirrors every write
		into checkMatrix and re-reads it.

	Pipeline position:
		Support; not on the main parse path.  Callers pass CACHEDIR-based paths.

	Key entry points:
		- initialize(first,second,value) - allocate (first+1)*(second+1) slots.
		- put / get - store or load one cell; out-of-range is LOG_FATAL_ERROR.
		- errstr() - _wcserror_s of the last errno into errbuffer.

	Notes / gotchas:
		- initialize() writes sizeof(buf) even when fewer bytes remain, so the file
			is longer than (first+1)*(second+1)*sizeof(T) (harmless slack).
		- Disk index is (first*(saveSecond+1) + second), matching the (saveFirst+1)
			by (saveSecond+1) allocation - put/get allow first==saveFirst and
			second==saveSecond (an inclusive range).  Previously the stride was
			saveSecond (missing +1), so the last column of each row aliased the
			first cell of the next row; fixed.  The current sole caller (hmm.cpp's
			Viterbi matrices) never actually indexes the last row/column, so this
			was latent, not observably wrong, but a future caller that does use
			the full inclusive range would have silently corrupted data.
		- RAM mode's `matrix` and `checkMatrix` are now sized (first+1) by
			(second+1) too, for the same reason (they were previously sized
			exactly (first,second), one short of the inclusive max index, so
			put/get(saveFirst,saveSecond) was an out-of-bounds vector access).
		- check/checkMatrix are private with no setter, so `check` is always
			false and that self-verification path is unreachable today.
		- get() on a failed fd returns -1, which is not a valid T for every T.
*/
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <io.h>
#include <sys\stat.h>
#include <fcntl.h> 
#include <sys\types.h>
#include <share.h>

using namespace std;
#pragma once
template <class T>
class DIYDiskArray
{
	vector<vector <T>> matrix;
	vector<vector <T>> checkMatrix;
	wstring path;
	bool useDisk;
	int matrixfp;
	errno_t error;
	bool anyErrorFatal;
	wchar_t errbuffer[1024];
	__int64 saveFirst, saveSecond;
	bool check=false;

public:
	// Allocate a (first+1) by (second+1) matrix filled with 'value', in both
	// disk and RAM mode (matching put/get's inclusive first<=saveFirst,
	// second<=saveSecond range check).  Disk mode creates/opens 'path' and
	// writes the fill in 4096-T chunks (the last write is a full chunk even
	// if fewer bytes remain).
	errno_t initialize(__int64 first, __int64 second, T value)
	{
		saveFirst = first;
		saveSecond = second;
		if (useDisk)
		{
			error = _wsopen_s(&matrixfp, path.c_str(), _O_BINARY | _O_RDWR | _O_RANDOM | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
			if (error && anyErrorFatal)
				lplog(LOG_FATAL_ERROR, L"DIYDiskArray:initialization _wsopen failure = %d[%s] with path %s", error, errstr(), path.c_str());
			if (!error)
			{
				__int64 totalBytes = (first + 1) * (second + 1) * sizeof(T);
				T buf[4096];
				for (int I = 0; I < 4096; I++)
					buf[I] = value;
				for (__int64 bytes = 0; bytes < totalBytes; bytes += sizeof(buf))
					if (_write(matrixfp, buf, sizeof(buf)) < 0)
					{
						error = errno;
						if (error && anyErrorFatal)
							lplog(LOG_FATAL_ERROR, L"DIYDiskArray:initialization _write [%I64d] failure = %d[%s] with path %s", bytes, error, errstr(), path.c_str());
					}
			}
			if (check)
				checkMatrix = vector(first + 1, vector(second + 1, value));
			return error;
		}
		else
			matrix = vector(first + 1, vector(second + 1, value));
		return 0;
	}

	// Format this->error into errbuffer (1024 wchar_t).  Returned pointer is
	// owned by *this and is overwritten by the next errstr() call.
	wchar_t *errstr()
	{
		_wcserror_s(errbuffer, 1024, error);
		return errbuffer;
	}

	// Store value at [first][second].  Out of range is FATAL.  Disk seek uses
	// stride (saveSecond+1), matching the (saveFirst+1)x(saveSecond+1) allocation.
	// Returns 0, or -1 if the fd is already in error.
	int put(__int64 first, __int64 second, T value)
	{
		if (check)
			lplog(LOG_ERROR, L"DIYDiskArray:put [%I64d,%I64d] %f", first, second, value);
		if (first > saveFirst || second > saveSecond || first < 0 || second < 0)
			lplog(LOG_FATAL_ERROR, L"DIYDiskArray:illegal parameters put [%I64d,%I64d]", first, second);
		if (!useDisk)
			matrix[first][second] = value;
		else
		{
			if (matrixfp == -1 || error != 0)
				return -1;
			if (_lseeki64(matrixfp, (first*(saveSecond + 1) + second) * sizeof(value), SEEK_SET) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:put _lseeki64 [%I64d,%I64d] failure = %d[%s] with path %s", first, second, error, errstr(), path.c_str());
				return -1;
			}
			if (_write(matrixfp, &value, sizeof(value)) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:_write failure = %d[%s] with path %s", error, errstr(), path.c_str());
				return -1;
			}
			if (check)
			{
				checkMatrix[first][second] = value;
				if (get(first, second) != value)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray [%I64d,%I64d] put failure disk=%f memory check=%f", first, second, value, checkMatrix[first][second]);
			}
		}
		return 0;
	}

	// Load [first][second].  Out of range is FATAL.  On a dead fd returns -1
	// (not a valid T for every T); on seek/read failure returns 0.
	T get(__int64 first, __int64 second)
	{
		if (check)
			lplog(LOG_ERROR, L"DIYDiskArray:get [%I64d,%I64d]", first, second);
		if (first > saveFirst || second > saveSecond || first < 0 || second < 0)
			lplog(LOG_FATAL_ERROR, L"DIYDiskArray:illegal parameters get [%I64d,%I64d]", first, second);
		if (!useDisk)
			return matrix[first][second];
		else
		{
			if (matrixfp == -1 || error != 0)
				return -1;
			T value;
			if (_lseeki64(matrixfp, (first*(saveSecond + 1) + second) * sizeof(value), SEEK_SET) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:put _lseeki64 [%I64d,%I64d] failure = %d[%s] with path %s", first, second, error, errstr(), path.c_str());
				return 0;
			}
			if (_read(matrixfp, &value, sizeof(value)) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:_read failure = %d[%s] with path %s", error, errstr(), path.c_str());
				return 0;
			}
			if (check)
			{
				if (checkMatrix[first][second] != value)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray [%I64d,%I64d] get failure disk=%f memory check=%f", first, second, value, checkMatrix[first][second]);
			}
			return value;
		}
	}

	// tpath is copied into an owned wstring, so a temporary c_str() is safe.
	// NULL path = RAM mode.  anyErrorFatal defaults true so I/O errors abort.
	DIYDiskArray(const wchar_t *tpath)
	{
		useDisk = tpath != NULL;
		if (useDisk)
			path = tpath;
		matrixfp = -1;
		error = 0;
		anyErrorFatal = true;
	}


	// Flush and close the disk fd if one was opened.
	~DIYDiskArray()
	{
		if (matrixfp != -1)
		{
			_commit(matrixfp);
			_close(matrixfp);
			matrixfp = -1;
		}
	}

	DIYDiskArray(const DIYDiskArray &) = delete;
	DIYDiskArray &operator=(const DIYDiskArray &) = delete;
};


