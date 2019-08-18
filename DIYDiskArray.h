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
	wchar_t *path;
	int matrixfp;
	errno_t error;
	bool anyErrorFatal;
	wchar_t errbuffer[1024];
	__int64 saveFirst, saveSecond;
	bool check=false;

public:
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

	wchar_t *errstr()
	{
		_wcserror_s(errbuffer, 1024, error);
		return errbuffer;
	}

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

	DIYDiskArray(wchar_t *tpath)
	{
		path = tpath;
		anyErrorFatal = true;
	}


	~DIYDiskArray()
	{
	}
};


