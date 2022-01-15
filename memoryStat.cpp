// from MSDN
#define _WIN32_DCOM

#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <Wbemidl.h>
using namespace std;
#include "logging.h"
#include "mysql.h"
#include "general.h"
#include "profile.h"
# pragma comment(lib, "wbemuuid.lib")
#pragma warning(disable: 4267)

__int64 memoryAllocated = 0; // protect with mutex

IWbemRefresher* pRefresher = NULL;
IWbemHiPerfEnum* pEnum = NULL;
IWbemServices* pNameSpace = NULL;

int initializeCounter(void)
{
	LFS
		HRESULT hr = S_OK;
	// To add error checking,
	// check returned HRESULT below where collected.
	if (FAILED(hr = CoInitializeEx(NULL, COINIT_MULTITHREADED))) return -1;
	if (FAILED(hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_NONE, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, 0))) return -1;
	IWbemLocator* pWbemLocator = NULL;
	if (FAILED(hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pWbemLocator))) return -1;
	// Connect to the desired namespace.
	BSTR bstrNameSpace;
	if ((bstrNameSpace = SysAllocString(L"\\\\.\\root\\cimv2")) == NULL) return -1;
	if (FAILED(hr = pWbemLocator->ConnectServer(
		bstrNameSpace,
		NULL, // User name
		NULL, // Password
		NULL, // Locale
		0L,   // Security flags
		NULL, // Authority
		NULL, // Wbem context
		&pNameSpace)))
		return -1;
	pWbemLocator->Release();
	SysFreeString(bstrNameSpace);
	bstrNameSpace = NULL;
	if (FAILED(hr = CoCreateInstance(CLSID_WbemRefresher, NULL, CLSCTX_INPROC_SERVER, IID_IWbemRefresher, (void**)&pRefresher))) return -1;
	IWbemConfigureRefresher* pConfig = NULL;
	if (FAILED(hr = pRefresher->QueryInterface(IID_IWbemConfigureRefresher, (void**)&pConfig))) return -1;
	// Add an enumerator to the refresher.
	//long lID = 0;
	//hr = pConfig->AddEnum(pNameSpace, L"Win32_PerfRawData_PerfProc_Process", 0, NULL, &pEnum, &lID);
	pConfig->Release();
	return hr;
}

int getCounter(const wchar_t* counter, DWORD& dwValue)
{
	LFS
		HRESULT hr = S_OK;
	DWORD dwNumObjects = 0, dwNumReturned = 0, dwIDProcess = 0;

	if (pRefresher == NULL || FAILED(hr = pRefresher->Refresh(0L)) || !pEnum) return -1;
	IWbemObjectAccess** apEnumAccess = NULL;
	hr = pEnum->GetObjects(0L, dwNumObjects, apEnumAccess, &dwNumReturned);
	// If the buffer was not big enough,
	// allocate a bigger buffer and retry.
	if (hr == WBEM_E_BUFFER_TOO_SMALL && dwNumReturned > dwNumObjects)
	{
		if ((apEnumAccess = new IWbemObjectAccess * [dwNumReturned]) == NULL) return -1;
		SecureZeroMemory(apEnumAccess, dwNumReturned * sizeof(IWbemObjectAccess*));
		dwNumObjects = dwNumReturned;
		if (FAILED(hr = pEnum->GetObjects(0L, dwNumObjects, apEnumAccess, &dwNumReturned))) return -1;
	}
	else
		if (hr == WBEM_S_NO_ERROR) return -1;
	// First time through, get the handles.
	long lValueHandle = 0, lIDProcessHandle = 0;
	CIMTYPE ValueType, ProcessHandleType;
	if (FAILED(hr = apEnumAccess[0]->GetPropertyHandle(counter, &ValueType, &lValueHandle))) return -1;
	if (FAILED(hr = apEnumAccess[0]->GetPropertyHandle(L"IDProcess", &ProcessHandleType, &lIDProcessHandle))) return -1;
	int processId = GetCurrentProcessId();
	unsigned int I;
	for (I = 0; I < dwNumReturned; I++)
	{
		//DWORD dwProcessId = 0;
		if (FAILED(hr = apEnumAccess[I]->ReadDWORD(lIDProcessHandle, &dwIDProcess))) return -1;
		if (processId == dwIDProcess) break;
	}
	if (I != dwNumReturned) hr = apEnumAccess[I]->ReadDWORD(lValueHandle, &dwValue);
	for (I = 0; I < dwNumReturned; I++) apEnumAccess[I]->Release();
	delete[] apEnumAccess;
	return hr;
}

void freeCounter(void)
{
	LFS
		if (pNameSpace)   pNameSpace->Release();
	if (pEnum)        pEnum->Release();
	if (pRefresher)   pRefresher->Release();
	CoUninitialize();
}

void reportMemoryUsage(void)
{
	LFS
		DWORD privateBytes = 0;
	getCounter(L"PrivateBytes", privateBytes);
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);
	int MB = 1024 * 1024;
	lplog(L"%ld%% memory in use: Physical (%I64dMB total,%I64dMB free)  Virtual (%I64dMB total,%I64dMB free)  Extended (%I64dMB free)  Page file: (%I64dMB total,%I64dMB free) ProcessBytes=%dMB",
		statex.dwMemoryLoad, statex.ullTotalPhys / MB, statex.ullAvailPhys / MB, statex.ullTotalVirtual / MB, statex.ullAvailVirtual / MB, statex.ullAvailExtendedVirtual / MB,
		statex.ullTotalPageFile / MB, statex.ullAvailPageFile / MB, privateBytes / MB);
}

/* memtrack */
void* tmalloc(size_t num)
{
	LFS
		memoryAllocated += num;
	void* newMemory = malloc(num);
	if (newMemory != NULL) return newMemory;
	reportMemoryUsage();
	lplog(LOG_FATAL_ERROR, L"Out of memory requesting %d bytes!", num);
	::lplog(NULL);
	exit(0);
}

void* tcalloc(size_t num, size_t SizeOfElements)
{
	LFS
		memoryAllocated += (num * SizeOfElements);
	void* newMemory = calloc(num, SizeOfElements);
	if (newMemory != NULL) return newMemory;
	reportMemoryUsage();
	lplog(LOG_FATAL_ERROR, L"Out of memory requesting %d bytes!", num);
	::lplog(NULL);
	exit(0);
}

void* trealloc(int from, void* original, unsigned int oldbytes, unsigned int newbytes)
{
	LFS
		memoryAllocated += (newbytes - oldbytes);
	void* newMemory = realloc(original, newbytes);
#ifdef _DEBUG
	memset(((::byte*)newMemory) + oldbytes, 0, (newbytes - oldbytes)); // necessary to make wNULL checked iterator execute because of orphan_me check
#endif
	if (newMemory != NULL) return newMemory;
	reportMemoryUsage();
	lplog(LOG_FATAL_ERROR, L"Out of memory requesting %d bytes! (%d)", newbytes, from);
	::lplog(NULL);
	//char buf[11];
	//_fgets(buf,10,stdin);
	exit(0);
}

void tfree(size_t oldbytes, void* original)
{
	LFS
		memoryAllocated -= oldbytes;
	free(original);
}

