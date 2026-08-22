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
		- path is stored as a borrowed pointer; a temporary wstring.c_str() dangles.
		- Destructor is empty: the fd is never closed (leak, and the file is not
			flushed).
		- initialize() writes sizeof(buf) even when fewer bytes remain, so the file
			is longer than (first+1)*(second+1)*sizeof(T).
		- Disk index is (first*saveSecond + second).  Dimensions are saveFirst+1 by
			saveSecond+1, so the stride should be (saveSecond+1).  The last column of
			each row aliases the first cell of the next row.
		- checkMatrix is sized (first) x (second), one short of the inclusive max
			index, so a put of (saveFirst, saveSecond) is OOB when check==true.
		- get() on a failed fd returns -1, which is not a valid T for every T.
*/
#include <sstream>
#include <iostream>
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
	const wchar_t *path;
	int matrixfp;
	errno_t error;
	bool anyErrorFatal;
	wchar_t errbuffer[1024];
	__int64 saveFirst, saveSecond;
	bool check=false;

public:
	// Allocate a (first+1) by (second+1) matrix filled with 'value'.  Disk mode
	// creates/opens 'path' and writes the fill in 4096-T chunks (the last write
	// is a full chunk even if fewer bytes remain).  RAM mode is vector(first) x
	// vector(second) - one short of the inclusive max index.
	errno_t initialize(__int64 first, __int64 second, T value)
	{
		saveFirst = first;
		saveSecond = second;
		if (path != NULL)
		{
			error = _wsopen_s(&matrixfp, path, _O_BINARY | _O_RDWR | _O_RANDOM | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
			if (error && anyErrorFatal)
				lplog(LOG_FATAL_ERROR, L"DIYDiskArray:initialization _wsopen failure = %d[%s] with path %s", error, errstr(), path);
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
							lplog(LOG_FATAL_ERROR, L"DIYDiskArray:initialization _write [%I64d] failure = %d[%s] with path %s", bytes, error, errstr(), path);
					}
			}
			if (check)
				checkMatrix = vector(first, vector(second, value));
			return error;
		}
		else
			matrix = vector(first, vector(second, value));
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
	// (first*saveSecond + second) - stride is saveSecond, not saveSecond+1.
	// Returns 0, or -1 if the fd is already in error.
	int put(__int64 first, __int64 second, T value)
	{
		if (check)
			lplog(LOG_ERROR, L"DIYDiskArray:put [%I64d,%I64d] %f", first, second, value);
		if (first > saveFirst || second > saveSecond || first < 0 || second < 0)
			lplog(LOG_FATAL_ERROR, L"DIYDiskArray:illegal parameters put [%I64d,%I64d]", first, second);
		if (path == NULL)
			matrix[first][second] = value;
		else
		{
			if (matrixfp == -1 || error != 0)
				return -1;
			if (_lseeki64(matrixfp, (first*saveSecond + second) * sizeof(value), SEEK_SET) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:put _lseeki64 [%I64d,%I64d] failure = %d[%s] with path %s", first, second, error, errstr(), path);
				return -1;
			}
			if (_write(matrixfp, &value, sizeof(value)) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:_write failure = %d[%s] with path %s", error, errstr(), path);
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
		if (path == NULL)
			return matrix[first][second];
		else
		{
			if (matrixfp == -1 || error != 0)
				return -1;
			T value;
			if (_lseeki64(matrixfp, (first*saveSecond + second) * sizeof(value), SEEK_SET) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:put _lseeki64 [%I64d,%I64d] failure = %d[%s] with path %s", first, second, error, errstr(), path);
				return 0;
			}
			if (_read(matrixfp, &value, sizeof(value)) < 0)
			{
				error = errno;
				if (error && anyErrorFatal)
					lplog(LOG_FATAL_ERROR, L"DIYDiskArray:_read failure = %d[%s] with path %s", error, errstr(), path);
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

	// Borrow tpath (not copied - a temporary dangles).  NULL path = RAM mode.
	// anyErrorFatal defaults true so I/O errors abort.
	DIYDiskArray(const wchar_t *tpath)
	{
		path = tpath;
		anyErrorFatal = true;
	}


	// Intentionally empty: the disk fd is never closed or flushed.
	~DIYDiskArray()
	{
	}
};


