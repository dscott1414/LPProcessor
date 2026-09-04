/*
	profile.h - RAII function profiler (cProfile) and the LFS/LFSL/DLFS instrumentation macros

	Overview:
		When PROFILE is defined, every `LFS` at the top of a function constructs a
		cProfile on the stack whose destructor records elapsed monotonic-clock time,
		process-memory delta, and call count into a process-wide map keyed by a colon-separated
		function-path (caller:callee:...).  lfprint() dumps the tree sorted by time,
		memory-without-children, and call count.  When PROFILE is off (the default),
		LFS/LFSL/DLFS expand to nothing, which is why locals in this codebase appear
		to hang off a bare `LFS` line.

	Pipeline position:
		Optional; used while timing question-answering.  accumulateNetworkTime() is
		also called from Internet.cpp and from logstring() on FATAL so network wait
		is attributed even when the profiler is off.

	Key entry points:
		- cProfile(function,num) / ~cProfile() - push/pop the function path and accumulate.
		- lfprint() - dump TIME/MEMORY/COUNT totals to the log.
		- accumulateNetworkTime() - add one HTTP wait to the per-host maps.
		- counterBegin/counterEnd/printCounters - ad-hoc timing buckets (not the path tree).

	Key data structures / globals:
		- timeMapTotal / timeSort / memorySort / countSort / functionPath - process-wide
			and explicitly not thread-safe (author comments on each).
		- mySQLTotalTime - summed in DBUtility.cpp under mySQLTotalTimeSRWLock.
		- networkTimeSRWLock - guards the accumulateNetworkTime maps.
		- logQuestionProfileTime - master enable; constructor returns immediately if 0.

	Notes / gotchas:
		- Nested LFS in a recursive function (paice stem()) will grow functionPath
			without bound for that thread.
		- logQuestionProfileTime is latched into cProfile::active at construction, so
			toggling it mid-run cannot leave the functionPath stack unbalanced.
*/
#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <set>
#include <unordered_map>
#include "time.h"
#include <chrono>
#include <shared_mutex>
#include <mach/mach.h>
#include "logging.h" // batch B3: lplog/LOG_INFO, used throughout this header
extern int logQuestionProfileTime;
int clocksec();

// ---------------------------------------------------------------------------
// Batch B3: portable replacements for the three Win32 primitives this profiler
// was built on (QueryPerformanceCounter, QueryPerformanceFrequency, and Psapi's
// GetProcessMemoryInfo). Kept in this header because profile.h is their only
// user in the entire tree.
// ---------------------------------------------------------------------------

// QueryPerformanceCounter replacement. steady_clock is the correct analogue: both
// are monotonic (never jump backwards on a clock adjustment) and both are only
// meaningful as a difference between two samples, which is all this profiler ever
// does with them.
inline int64_t lpPerformanceCounter()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

// QueryPerformanceFrequency replacement: ticks per second of the counter above.
// Unlike QPF this is a compile-time constant and cannot fail, so the callers'
// old "QPF failed, or reported a non-positive frequency, so give up" branches
// are gone rather than ported.
constexpr int64_t LP_PERFORMANCE_FREQUENCY = 1000000000LL;

// GetProcessMemoryInfo(...).PrivateUsage replacement; returns bytes, or -1 if the
// kernel call fails (same "-1 means unavailable" convention the callers already
// had for a failed GetProcessMemoryInfo).
//
// Not an exact equivalent, and cannot be: PrivateUsage is Windows' private commit
// charge, a concept macOS does not have. resident_size (this process's share of
// physical memory) is the closest thing the Mach task API offers, and it preserves
// what the profiler actually uses the number for -- a before/after DELTA per
// function, to attribute allocation growth. The absolute figure printed in the
// MEMORY report is now "resident bytes", which is a different (smaller, since it
// excludes swapped-out and never-touched committed pages) number than the Windows
// runs produced; deltas remain directly comparable, totals do not.
inline int64_t lpProcessMemoryBytes()
{
	mach_task_basic_info_data_t info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
		return -1;
	return (int64_t)info.resident_size;
}

class cProfile
{
	class CP
	{
	public:
		int64_t t = 0; // time
		int64_t c = 0; // count
		int64_t m = 0; // memory
		int64_t mwc = 0; // memory without children
	};
	// Each comparator breaks ties on the key: without it a set of iterators drops
	// every function whose value equals one already inserted.
	struct timeSetCompare
	{
		bool operator()(const unordered_map <string,CP>::iterator &lhs, const unordered_map <string,CP>::iterator &rhs) const
		{
			if (lhs->second.t!=rhs->second.t) return lhs->second.t<rhs->second.t;
			return lhs->first<rhs->first;
		}
	};
	struct memorySetCompare
	{
		bool operator()(const unordered_map <string,CP>::iterator &lhs, const unordered_map <string,CP>::iterator &rhs) const
		{
			if (lhs->second.mwc!=rhs->second.mwc) return lhs->second.mwc<rhs->second.mwc;
			return lhs->first<rhs->first;
		}
	};
	struct countSetCompare
	{
		bool operator()(const unordered_map <string,CP>::iterator &lhs, const unordered_map <string,CP>::iterator &rhs) const
		{
			if (lhs->second.c!=rhs->second.c) return lhs->second.c>rhs->second.c;
			return lhs->first<rhs->first;
		}
	};
	static unordered_map <string,CP> timeMapTotal; // profiler is not threadsafe
	static set <unordered_map <string,CP>::iterator ,timeSetCompare> timeSort; // sort unordered_map by value // profiler is not threadsafe
	static set <unordered_map <string,CP>::iterator ,memorySetCompare> memorySort; // sort unordered_map by value // profiler is not threadsafe
	static set <unordered_map <string,CP>::iterator ,countSetCompare> countSort; // sort unordered_map by value // profiler is not threadsafe
	static string functionPath; // profiler is not threadsafe
	static int64_t accumulatedOverheadTime; // profiler is not threadsafe
	static int64_t totalCount; // profiler is not threadsafe
	static unordered_map < lpwstring, int64_t > netAndSleepTimes,onlyNetTimes,numTimesPerURL;   // protect by networkTimeSRWLock
public:
	static int64_t accumulationNetworkProfileTimer,accumulateOnlyNetTimer,lastNetworkTimePrinted,accumulateNetworkTimeCount;  // protect by networkTimeSRWLock
	static int totalInternetTimeWaitBandwidthControl;  // protect by totalInternetTimeWaitBandwidthControlSRWLock
	static int lastNetClock;   // protect by networkTimeSRWLock
	// Batch B3: was a Win32 SRWLOCK; std::shared_mutex is the direct equivalent.
	// The name is kept -- see general.h's note on the other four locks.
	static std::shared_mutex networkTimeSRWLock;
	static int64_t mySQLTotalTime; // protect by mySQLTotalTimeSRWLock
	string saveFunctionPath;
	int64_t startTime,startPrivateBytes;
	bool active; // latched from logQuestionProfileTime so ctor and dtor always agree
	// Push 'function' (or a decimal 'num') onto functionPath and snapshot QPC +
	// PrivateUsage.  Empty function resets the network accumulators.  No-op when
	// logQuestionProfileTime is 0.  Not re-entrant across threads (functionPath is static).
	cProfile(const char * function,int num=0)
	{
		startPrivateBytes = 0;
		startTime = 0;
		active = false;
		if (function==NULL || function[0]==0)
		{
			accumulationNetworkProfileTimer=0;
			accumulateOnlyNetTimer=0;
			lastNetworkTimePrinted=0;
			accumulateNetworkTimeCount=0;  
		}
		if (!logQuestionProfileTime) return;
		active = true;
		saveFunctionPath=functionPath;
		if (functionPath.length()>0)
			functionPath+=string(":");
		if (num>0)
		{
			char f[50];
			snprintf(f,sizeof(f),"%d",num);
			functionPath+=f;
		}
		else if (function!=NULL)
		{
			const char *wc=strchr(function,':');
			// __FUNCTION__ is class::method; skip past "::" only if both colons are there
			if (wc && wc[1]==':' && wc[2])
				functionPath+=wc+2;
			else
				functionPath+=function;
		}
		startPrivateBytes=lpProcessMemoryBytes();
		startTime=lpPerformanceCounter();
	}
	// Add elapsed QPC ticks and private-byte delta to timeMapTotal[functionPath],
	// restore the caller's path, and charge the destructor itself to accumulatedOverheadTime.
	~cProfile()
	{
		if (!active) return;
		int64_t endTime,overheadTime;
		endTime=lpPerformanceCounter();
		CP &cp=timeMapTotal[functionPath];
		cp.t+=(endTime-startTime);
		cp.c++;
		int64_t endPrivateBytes=lpProcessMemoryBytes();
		if (endPrivateBytes>=0 && startPrivateBytes>0)
			cp.m+=endPrivateBytes - startPrivateBytes;
		overheadTime=lpPerformanceCounter();
		accumulatedOverheadTime+=overheadTime-endTime;
		totalCount++;
		functionPath=saveFunctionPath;
	}
	cProfile(const cProfile &) = delete;
	cProfile &operator=(const cProfile &) = delete;
	static int64_t cb;
	static unordered_map <string ,int64_t > counterMap;
	static unordered_map <string ,int > counterNumMap;
	// Start (or restart) the ad-hoc QPC interval stored in cb.
	static void counterBegin(void)
	{
		cb=lpPerformanceCounter();
	}

	// Add (now-cb) to counterMap[countType], bump counterNumMap, then restart cb
	// so consecutive counterEnd calls measure a chain of intervals.
	static void counterEnd(const char *countType)
	{
		int64_t endcb=lpPerformanceCounter();
		endcb-=cb;
		unordered_map <string ,int64_t >::iterator cmi=counterMap.find(countType);
		if (cmi==counterMap.end())
		{
			counterMap[countType]=endcb;
			counterNumMap[countType]=1;
		}
		else
		{
			cmi->second+=endcb;
			counterNumMap[countType]++;
		}
		counterBegin();
	}

	// printf each counterMap bucket as ticks and percent of the sum.
	static void printCounters()
	{
		int64_t total=0;
		for (unordered_map <string ,int64_t >::iterator cmi=counterMap.begin(),cmiEnd=counterMap.end(); cmi!=cmiEnd; cmi++)
			total+=cmi->second;
		if (total<=0) return;
		for (unordered_map <string ,int64_t >::iterator cmi=counterMap.begin(),cmiEnd=counterMap.end(); cmi!=cmiEnd; cmi++)
		{
			unordered_map <string ,int >::iterator cmci=counterNumMap.find(cmi->first);
			// %I64d is MSVC-only; %lld is the portable spelling for int64_t here
			// (this is a narrow printf, not lplog's own lpchar_t engine, which
			// does still accept %I64d -- see lpchar.h).
			printf("%s:%07lld %02lld%% [COUNT:%d]\n",cmi->first.c_str(),(long long)cmi->second,(long long)(cmi->second*100/total),(cmci==counterNumMap.end()) ? 0 : cmci->second);
		}
	}

	// Log one timeMapTotal entry.  Empty 'function' also dumps SQL wait and
	// flushes accumulateNetworkTime.
	static void lfprint(const char *function,int num)
	{
		if (function==NULL) return;
		string key(function);
		if (num>0)
		{
			char f[64];
			snprintf(f,sizeof(f)," line:%d",num);
			key+=f;
		}
		lplog(LOG_INFO,u"TIME %S:%I64d",key.c_str(),timeMapTotal[key].t);
		if (function[0]==0)
		{
			lplog(LOG_INFO,u"SQL: %08I64d ms waitTime=%d",mySQLTotalTime,totalInternetTimeWaitBandwidthControl);
			accumulateNetworkTime(u"",0,0);
		}
	}

	// True if 'child' is 'parent' plus exactly one more ':' segment; 'rest' gets that segment.
	static bool directChild(const string &parent,const string &child,string &rest)
	{
		size_t offset=parent.length();
		if (offset>=child.length()) return false;
		if (child.compare(0,offset,parent)!=0) return false;
		if (offset>0)
		{
			if (child[offset]!=':') return false;
			offset++;
			if (offset>=child.length()) return false;
		}
		rest=child.substr(offset);
		return rest.find(':')==string::npos;
	}

	// Close out 'profile', then dump TIME / MEMORY / COUNT reports for every
	// functionPath, with direct-child attribution (paths that add exactly one
	// more ':' segment).  No-op when logQuestionProfileTime is 0.
	static void lfprint(cProfile &profile)
	{
		// profile.active, not logQuestionProfileTime: if the flag was turned on after
		// 'profile' was constructed its startTime is 0 and the elapsed time is garbage.
		if (!profile.active) return;
		int64_t endTime,total,memoryTotal=0;
		endTime=lpPerformanceCounter();
		total=(timeMapTotal[functionPath].t+=(endTime-profile.startTime));
		int64_t processMemory=lpProcessMemoryBytes();
		if (processMemory>=0)
			memoryTotal=processMemory;
		// Batch B3: the old "QueryPerformanceFrequency failed or returned a
		// non-positive frequency" early return is gone -- LP_PERFORMANCE_FREQUENCY
		// is a compile-time constant that cannot fail.
		constexpr int64_t frequency=LP_PERFORMANCE_FREQUENCY;
		if (total<=0)
		{
			lplog(LOG_INFO,u"TIME TOTALS: no elapsed time recorded");
			return;
		}
		// static sorts persist across calls; a second lfprint would otherwise hold stale iterators
		timeSort.clear();
		memorySort.clear();
		countSort.clear();
		lplog(LOG_INFO,u"TIME TOTALS:%I64d ms %I64d s (%I64d minutes)",total*1000/frequency,total/frequency,total/frequency/60);
		lplog(LOG_INFO,u"TIME PROFILE OVERHEAD:%I64d ms %I64d%%",accumulatedOverheadTime*1000/frequency,accumulatedOverheadTime*100/total);
		for (unordered_map<string,CP>::iterator ti=timeMapTotal.begin(),tiEnd=timeMapTotal.end(); ti!=tiEnd; ti++)
			timeSort.insert(ti);
		for (set<unordered_map<string,CP>::iterator,timeSetCompare>::iterator si=timeSort.begin(),siEnd=timeSort.end(); si!=siEnd; si++)
		{
			int64_t directChildrenMemory=0;
			int64_t directChildrenTime=0;
			string theRest;
			for (set<unordered_map<string,CP>::iterator,timeSetCompare>::iterator ssi=timeSort.begin(),ssiEnd=timeSort.end(); ssi!=ssiEnd; ssi++)
				if (directChild((*si)->first,(*ssi)->first,theRest))
				{
					directChildrenMemory+=(*ssi)->second.m;
					directChildrenTime+=(*ssi)->second.t;
				}
			(*si)->second.mwc=(*si)->second.m-directChildrenMemory;
			int64_t percentage=(*si)->second.t*100/total;
			if (percentage>0)
			{
				int64_t ms=(*si)->second.t*1000/frequency;
				int64_t withoutChildrenMS=((*si)->second.t-directChildrenTime)*1000/frequency;
				int64_t withoutChildrenPercentage=((*si)->second.t-directChildrenTime)*100/total;
				if (withoutChildrenPercentage>0 || percentage>20 || (*si)->second.c>1000000)
				{
					lplog(LOG_INFO,u"TIME %S:%I64d ms %I64d%% [without children %I64d ms %I64d%%] count=%I64d",(*si)->first.c_str(),ms,percentage,withoutChildrenMS,withoutChildrenPercentage,(*si)->second.c);
					if ((*si)->second.t<=0) continue;
					for (set<unordered_map<string,CP>::iterator,timeSetCompare>::iterator ssi=timeSort.begin(),ssiEnd=timeSort.end(); ssi!=ssiEnd; ssi++)
					{
						if (!directChild((*si)->first,(*ssi)->first,theRest)) continue;
						int64_t msts=(*ssi)->second.t*1000/frequency;
						int64_t percentage2=(*ssi)->second.t*100/(*si)->second.t;
						int64_t percentageOfTotal=(*ssi)->second.t*100/total;
						if (percentage2>0)
							lplog(LOG_INFO,u"  %S:%I64d ms [%I64d%% parent] [%I64d%% total]",theRest.c_str(), msts,percentage2,percentageOfTotal);
					}
				}
			}
		}
		for (unordered_map<string,CP>::iterator ti=timeMapTotal.begin(),tiEnd=timeMapTotal.end(); ti!=tiEnd; ti++)
		{
			memorySort.insert(ti);
			countSort.insert(ti);
		}
		for (set<unordered_map<string,CP>::iterator,memorySetCompare>::iterator si=memorySort.begin(),siEnd=memorySort.end(); memoryTotal>0 && si!=siEnd; si++)
		{
			int64_t percentage=(*si)->second.m*100/memoryTotal;
			if (percentage>0)
			{
				int64_t KB=(*si)->second.m/1000;
				int64_t withoutChildrenKB=((*si)->second.mwc)/1000;
				int64_t withoutChildrenPercentage=((*si)->second.mwc)*100/memoryTotal;
				if (withoutChildrenPercentage>0 || percentage>20 || (*si)->second.c>1000000)
				{
					lplog(LOG_INFO,u"MEMORY %S:%I64d KB %I64d%% [without children %I64d KB %I64d%%] count=%I64d",
						(*si)->first.c_str(),KB,percentage,withoutChildrenKB,withoutChildrenPercentage,(*si)->second.c);
					if ((*si)->second.m<=0) continue;
					string theRest;
					for (set<unordered_map<string,CP>::iterator,memorySetCompare>::iterator ssi=memorySort.begin(),ssiEnd=memorySort.end(); ssi!=ssiEnd; ssi++)
					{
						if (!directChild((*si)->first,(*ssi)->first,theRest)) continue;
						int64_t msKB=(*ssi)->second.m/1000;
						int64_t msPercentage=(*ssi)->second.m*100/(*si)->second.m;
						int64_t msPercentageOfTotal=(*ssi)->second.m*100/memoryTotal;
						if (msPercentage>0)
							lplog(LOG_INFO,u"  %S:%I64d KB [%I64d%% parent] [%I64d%% total]",theRest.c_str(),msKB,msPercentage,msPercentageOfTotal);
					}
				}
			}
		}
		int countListMax=0;
		for (set<unordered_map<string,CP>::iterator,countSetCompare>::iterator si=countSort.begin(),siEnd=countSort.end(); si!=siEnd && countListMax<40; si++,countListMax++)
		{
			string func=(*si)->first;
			size_t lastColon=func.rfind(':');
			if (lastColon!=string::npos) 
				func.erase(0,lastColon+1);
			lplog(LOG_INFO,u"COUNT %30S:%I64d",func.c_str(),(*si)->second.c);
			lplog(LOG_INFO,u"      %30S",(*si)->first.c_str());
		}
	}
	// Add one HTTP wait: (now-timer) to the sleep+net total and (now-lNC) to
	// net-only, keyed by the host carved out of 'str' (scheme://host/...).
	// timer==0 still takes the lock (used as a flush from FATAL and lfprint).
	// Batch B3: std::shared_mutex default-constructs into a usable state, so the
	// SRWLOCK_INIT/InitializeSRWLock question this comment used to discuss cannot
	// arise at all any more. The scoped unique_lock also makes the exclusive hold
	// exception-safe, which the old Acquire/Release pair was not.
	static void accumulateNetworkTime(const lpchar_t *str,int timer,int lNC)
	{
		std::unique_lock<std::shared_mutex> networkTimeLock(networkTimeSRWLock);
		int c=clock();
		accumulateNetworkTimeCount++;
		if (timer)
		{
			int t=c-timer,ont=c- lNC;
			accumulationNetworkProfileTimer+=t;
			accumulateOnlyNetTimer+=ont;
			lpwstring url((str) ? str : u"");
			size_t scheme=url.find(u'/');
			size_t endHost=(scheme==lpwstring::npos || scheme+2>url.length()) ? lpwstring::npos : url.find(u'/',scheme+2);
			if (endHost!=lpwstring::npos)
			{
				url.erase(endHost);
				unordered_map <lpwstring,int64_t>::iterator nt=onlyNetTimes.find(url);
				if (nt==onlyNetTimes.end())
				{
					netAndSleepTimes[url]=t;
					onlyNetTimes[url]=ont;
					numTimesPerURL[url]=1;
				}
				else
				{
					netAndSleepTimes[url]+=t;
					nt->second+=ont;
					numTimesPerURL[url]++;
				}
			}
		}
		//if (accumulationNetworkProfileTimer>0 && (c-lastNetworkTimePrinted>120000 || timer==0))
		//{
		//	lplog(LOG_WHERE,u"%07I64d:accumulated time=%09I64d MS (%I64d hours) accumulated net time=%09I64d MS (%I64d hours,%I64d MS/call)",accumulateNetworkTimeCount,
		//		accumulationNetworkProfileTimer,accumulationNetworkProfileTimer/3600000,accumulateOnlyNetTimer,accumulateOnlyNetTimer/3600000,accumulateOnlyNetTimer/accumulateNetworkTimeCount);
		//	for (unordered_map <lpwstring,int64_t>::iterator nti=onlyNetTimes.begin(),ntEnd=onlyNetTimes.end(); nti!=ntEnd; nti++)
		//		lplog(LOG_WHERE,u"%40s:%07I64d:%09I64d(%02I64d%%) %09I64d(%02I64d%% %I64d MS/call)",nti->first.c_str(),numTimesPerURL[nti->first.c_str()],
		//					netAndSleepTimes[nti->first.c_str()],netAndSleepTimes[nti->first.c_str()]*100/accumulationNetworkProfileTimer,
		//					nti->second,nti->second*100/accumulationNetworkProfileTimer,
		//					(numTimesPerURL[nti->first.c_str()]) ? nti->second/numTimesPerURL[nti->first.c_str()] : 0L);
		//	lastNetworkTimePrinted=c;
		//}
	}
};

//#define PROFILE
// PROFILEDETAIL implies PROFILE.  Previously both blocks defined DLFS/LFSL, so
// defining PROFILEDETAIL produced a macro redefinition and silently lost the detail forms.
#if defined(PROFILEDETAIL)
	#define LFS cProfile profile(__FUNCTION__);
	#define DLFS cProfile profile(__FUNCTION__);
	#define LFSL cProfile profile(__FUNCTION__,__LINE__);
#elif defined(PROFILE)
	#define LFS cProfile profile(__FUNCTION__);
	#define DLFS
	#define LFSL
#else
	#define LFS
	#define DLFS
	#define LFSL
#endif

