#pragma once
class cInternet
{

	typedef struct
	{
		HINTERNET RequestHandle;
		char *buffer;
		int bufsize;
		DWORD *dwRead;
	} tIRFW;

public:
	static int readPage(const wchar_t *str, wstring &buffer);
	static bool InetOption(bool global, int option, const wchar_t * description, unsigned long value);
	static void InternetStatusCallback(HINTERNET hInternet,DWORD_PTR dwContext,DWORD dwInternetStatus,LPVOID lpvStatusInformation,DWORD dwStatusInformationLength);
	static bool LPInternetOpen(int timer);
	static int readPage(const wchar_t *str, wstring &buffer, wstring &headers);
	static int readBinaryPage(wchar_t *str, int destfile, int &total);
	static bool closeConnection(void);
	static int cacheWebPath(wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, bool forceWebReread, bool &networkAccessed, wstring &diskPath);
	static DWORD WINAPI InternetReadFile_Child(void *vThreadParm);
	static bool InternetReadFile_Wait(HINTERNET RequestHandle, char *buffer, int bufsize, DWORD *dwRead);
	static HINTERNET hINet;
	static int bandwidthControl;
	static wstring redirectUrl;
	static bool readTimeoutError;
	static SRWLOCK totalInternetTimeWaitBandwidthControlSRWLock;
	static int getWebPath(int where, wstring webAddress, wstring &buffer, wstring epath, wstring cacheTypePath, wstring &filePathOut, wstring &headers, int index, bool clean, bool readInfoBuffer, bool forceWebReread=false);
	static void ReadAndHandleOutput(HANDLE hPipeRead, string &outbuf);
	static HANDLE PrepAndLaunchRedirectedChild(wstring commandLine,HANDLE hChildStdOut, HANDLE hChildStdIn, HANDLE hChildStdErr);
	static int runJavaJerichoHTML(wstring webAddress, wstring outputPath, string &outbuf);

	enum {
		GETWEBPATH_CANNOT_OPEN_PATH = -7, INTERNET_OPEN_FAILED = -13, INTERNET_OPEN_URL_FAILED = -14,
		GETPAGE_CANNOT_CREATE = -15
	};
	#define MAX_LEN 2048
	static int internetWebSearchRetryAttempts;
};

