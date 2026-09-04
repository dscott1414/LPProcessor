/*
	memoryStat.cpp - process/system memory reporting and the tmalloc family

	Overview:
		getCounter() reports this process's memory footprint via the Mach task API.
		reportMemoryUsage() logs system physical totals plus this process's
		footprint.  tmalloc / tcalloc / trealloc / tfree wrap the CRT allocators and
		keep the process-wide memoryAllocated counter; OOM logs FATAL (which exits)
		and then a dead exit(0).

		Batch B5 replaced a WMI/COM implementation of the first two that had never
		worked (see the note above getCounter below).

	Pipeline position:
		tmalloc is the allocator used by the binary cache, wordForms buffers, and
		DIYDiskArray.  reportMemoryUsage is called only from the OOM path.

	Key entry points:
		- getCounter() - this process's memory footprint (Mach task_info).
		- initializeCounter() / freeCounter() - now no-ops, kept for their call sites.
		- reportMemoryUsage() - one-line memory dump to main.lplog.
		- tmalloc / tcalloc / trealloc / tfree - tracked malloc.

	Key data structures / globals:
		- memoryAllocated - process-wide; comment says "protect with mutex" but
			there is no lock.
		(the COM objects that used to live here are gone -- batch B5)

	Notes / gotchas:
		- tcalloc's OOM message uses 'num' (element count), not num*SizeOfElements.
		- trealloc bumps memoryAllocated before realloc; on failure the counter is
			wrong and the original pointer is still valid (but FATAL exits anyway).
		- In _DEBUG, trealloc memset's the new tail - the comment says this is to
			keep checked iterators happy.
*/

// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
using namespace std;
#include "logging.h"
#include "mysql.h"
#include "general.h"
#include "profile.h"
#include <mach/mach.h>
#include <mach/task_info.h>
#include <sys/sysctl.h>
#pragma warning(disable: 4267)

int64_t memoryAllocated = 0; // protect with mutex

// ---------------------------------------------------------------------------
// Batch B5: the WMI apparatus (a Wbem refresher against root\cimv2, plus the COM
// initialization around it) is DELETED, not ported. It was already dead on
// Windows: the pConfig->AddEnum() that would have registered the
// Win32_PerfRawData_PerfProc_Process enumerator is commented out, so pEnum was
// never assigned and getCounter() always returned -1. Rather than reproduce a
// non-functional subsystem, this implements what it was supposed to do, using the
// Mach task API -- which is both a working replacement and far less code than the
// COM dance it replaces.
// ---------------------------------------------------------------------------

// Nothing to set up: the Mach calls below need no initialization or teardown.
// Both are kept (rather than deleted along with the WMI code) because main.cpp and
// specials_main.cpp call them during startup and shutdown; batch B4a/B4b can drop
// the call sites, at which point these can go too.
int initializeCounter(void)
{
	return 0;
}

void freeCounter(void)
{
}

// Report one named process counter. Only "PrivateBytes" is meaningful, and it is
// answered with this task's phys_footprint -- the number macOS itself treats as
// "how much memory this process is responsible for", and the closest analogue to
// Windows' private commit charge. Returns 0 on success, -1 if the counter is
// unknown or the kernel call fails (the same convention the WMI version declared,
// and unlike that one this actually succeeds).
int getCounter(const lpchar_t* counter, unsigned long& dwValue)
{
	dwValue = 0;
	if (!counter || lp_strcmp(counter, u"PrivateBytes") != 0)
		return -1;
	task_vm_info_data_t vmInfo;
	mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
	if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &count) != KERN_SUCCESS)
		return -1;
	dwValue = (unsigned long)vmInfo.phys_footprint;
	return 0;
}

// Log a one-line memory dump. Batch B5: host_statistics64 + sysctl replace
// GlobalMemoryStatusEx.
//
// Two figures the Windows version printed are deliberately absent rather than
// faked: the page-file totals (macOS has dynamically sized swap files, not a
// preallocated page file, so "total page file" has no value to report) and the
// "extended virtual" figure (a 32-bit-Windows AWE concept with no counterpart at
// all). The virtual-size figures are this process's address space rather than a
// system-wide total, which is what the Mach API exposes and is the more useful
// number anyway.
void reportMemoryUsage(void)
{
	LFS
		unsigned long privateBytes = 0;
	getCounter(u"PrivateBytes", privateBytes);

	int64_t totalPhysical = 0;
	size_t totalPhysicalSize = sizeof(totalPhysical);
	if (sysctlbyname("hw.memsize", &totalPhysical, &totalPhysicalSize, nullptr, 0) != 0)
		totalPhysical = 0;

	int64_t freePhysical = 0;
	vm_size_t pageSize = 0;
	mach_port_t host = mach_host_self();
	if (host_page_size(host, &pageSize) == KERN_SUCCESS)
	{
		vm_statistics64_data_t vmStat;
		mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
		if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vmStat, &count) == KERN_SUCCESS)
			// "Free" as the OS means it: untouched pages plus those it can reclaim
			// without writing anything out.
			freePhysical = (int64_t)(vmStat.free_count + vmStat.inactive_count + vmStat.purgeable_count) * (int64_t)pageSize;
	}

	int64_t virtualSize = 0;
	task_basic_info_64_data_t taskInfo;
	mach_msg_type_number_t taskCount = TASK_BASIC_INFO_64_COUNT;
	if (task_info(mach_task_self(), TASK_BASIC_INFO_64, (task_info_t)&taskInfo, &taskCount) == KERN_SUCCESS)
		virtualSize = (int64_t)taskInfo.virtual_size;

	const int64_t MB = 1024 * 1024;
	int memoryLoad = (totalPhysical > 0) ? (int)(100 - (freePhysical * 100 / totalPhysical)) : 0;
	lplog(u"%d%% memory in use: Physical (%I64dMB total,%I64dMB free)  Process (%I64dMB virtual) ProcessBytes=%I64dMB",
		memoryLoad, totalPhysical / MB, freePhysical / MB, virtualSize / MB, (int64_t)privateBytes / MB);
}

/* memtrack */
// malloc(num) and add num to memoryAllocated.  On OOM: dump memory, FATAL
// (exits), then a dead exit(0).  Does not check for wrap of memoryAllocated.
void* tmalloc(size_t num)
{
	LFS
		memoryAllocated += num;
	void* newMemory = malloc(num);
	if (newMemory != NULL) return newMemory;
	reportMemoryUsage();
	lplog(LOG_FATAL_ERROR, u"Out of memory requesting %d bytes!", num);
	::lplog(NULL);
	exit(0);
}

// calloc and add num*SizeOfElements to memoryAllocated.  The OOM message
// prints 'num' (elements), not the byte count.
void* tcalloc(size_t num, size_t SizeOfElements)
{
	LFS
		memoryAllocated += (num * SizeOfElements);
	void* newMemory = calloc(num, SizeOfElements);
	if (newMemory != NULL) return newMemory;
	reportMemoryUsage();
	lplog(LOG_FATAL_ERROR, u"Out of memory requesting %d bytes!", num);
	::lplog(NULL);
	exit(0);
}

// realloc, charging (newbytes-oldbytes) first.  'from' is a caller cookie
// printed on OOM to identify the site (see the integers in intArray.h / DB.cpp).
// _DEBUG memset's the new tail.  On failure FATAL-exits; original stays valid.
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
	lplog(LOG_FATAL_ERROR, u"Out of memory requesting %d bytes! (%d)", newbytes, from);
	::lplog(NULL);
	//char buf[11];
	//_fgets(buf,10,stdin);
	exit(0);
}

// Subtract oldbytes from memoryAllocated and free.  Caller is trusted for
// oldbytes; a mismatch silently drifts the counter.  free(NULL) is OK.
void tfree(size_t oldbytes, void* original)
{
	LFS
		memoryAllocated -= oldbytes;
	free(original);
}

