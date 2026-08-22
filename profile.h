/*
	profile.h - RAII function profiler (cProfile) and the LFS/LFSL/DLFS instrumentation macros

	Overview:
		When PROFILE is defined, every `LFS` at the top of a function constructs a
		cProfile on the stack whose destructor records elapsed QPC time, private-byte
		delta, and call count into a process-wide map keyed by a colon-separated
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
		- counterBegin/counterEnd/printCounters - ad-hoc QPC buckets (not the path tree).

	Key data structures / globals:
		- timeMapTotal / timeSort / memorySort / countSort / functionPath - process-wide
			and explicitly not thread-safe (author comments on each).
		- mySQLTotalTime - summed in DBUtility.cpp under mySQLTotalTimeSRWLock.
		- networkTimeSRWLock - guards the accumulateNetworkTime maps.
		- logQuestionProfileTime - master enable; constructor returns immediately if 0.

	Notes / gotchas:
		- accumulateNetworkTime() takes const wchar_t* but writes a NUL over the second
			'/' of the URL and restores it - that is UB on a string literal and will
			fault if the page is in read-only memory.
		- lfprint(char*,int) passes timeMapTotal[f] (a CP struct) to a %I64d format;
			varargs + class type is undefined.  It happens to print CP::t on MSVC.
		- Nested LFS in a recursive function (paice stem()) will grow functionPath
			without bound for that thread.
*/
#include "time.h"
#include "Psapi.h"
extern int logQuestionProfileTime;
int clocksec();

class cProfile
{
	class CP
	{
	public:
		__int64 t; // time
		__int64 c; // count
		__int64 m; // memory
		__int64 mwc; // memory without children
	};
	struct timeSetCompare
	{
		bool operator()(const unordered_map <string,CP>::iterator &lhs, const unordered_map <string,CP>::iterator &rhs) const
		{
			return lhs->second.t<rhs->second.t;
		}
	};
	struct memorySetCompare
	{
		bool operator()(const unordered_map <string,CP>::iterator &lhs, const unordered_map <string,CP>::iterator &rhs) const
		{
			return lhs->second.mwc<rhs->second.mwc;
		}
	};
	struct countSetCompare
	{
		bool operator()(const unordered_map <string,CP>::iterator &lhs, const unordered_map <string,CP>::iterator &rhs) const
		{
			return lhs->second.c>rhs->second.c;
		}
	};
	static unordered_map <string,CP> timeMapTotal; // profiler is not threadsafe
	static set <unordered_map <string,CP>::iterator ,timeSetCompare> timeSort; // sort unordered_map by value // profiler is not threadsafe
	static set <unordered_map <string,CP>::iterator ,memorySetCompare> memorySort; // sort unordered_map by value // profiler is not threadsafe
	static set <unordered_map <string,CP>::iterator ,countSetCompare> countSort; // sort unordered_map by value // profiler is not threadsafe
	static string functionPath; // profiler is not threadsafe
	static __int64 accumulatedOverheadTime; // profiler is not threadsafe
	static __int64 totalCount; // profiler is not threadsafe
	static unordered_map < wstring, __int64 > netAndSleepTimes,onlyNetTimes,numTimesPerURL;   // protect by networkTimeSRWLock
	static bool lockInitialized;
public:
	static __int64 accumulationNetworkProfileTimer,accumulateOnlyNetTimer,lastNetworkTimePrinted,accumulateNetworkTimeCount;  // protect by networkTimeSRWLock
	static int totalInternetTimeWaitBandwidthControl;  // protect by totalInternetTimeWaitBandwidthControlSRWLock
	static int lastNetClock;   // protect by networkTimeSRWLock
	static SRWLOCK networkTimeSRWLock;
	static __int64 mySQLTotalTime; // protect by mySQLTotalTimeSRWLock
	string saveFunctionPath;
	__int64 startTime,startPrivateBytes;
	// Push 'function' (or a decimal 'num') onto functionPath and snapshot QPC +
	// PrivateUsage.  Empty function resets the network accumulators.  No-op when
	// logQuestionProfileTime is 0.  Not re-entrant across threads (functionPath is static).
	cProfile(const char * function,int num=0)
	{
		startPrivateBytes = 0;
		startTime = 0;
		if (function[0]==0)
		{
			accumulationNetworkProfileTimer=0;
			accumulateOnlyNetTimer=0;
			lastNetworkTimePrinted=0;
			accumulateNetworkTimeCount=0;  
		}
		if (!logQuestionProfileTime) return;
		saveFunctionPath=functionPath;
		if (functionPath.length()>0)
			functionPath+=string(":");
		if (num>0)
		{
			char f[50];
			itoa(num,f,10);
			functionPath+=f;
		}
		else
		{
			const char *wc=strchr(function,L':');
			if (wc)
				functionPath+=wc+2;
			else
				functionPath+=function;
		}
		PROCESS_MEMORY_COUNTERS_EX memCounter;
		BOOL ret=GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memCounter,sizeof(memCounter));
		startPrivateBytes=(ret) ? memCounter.PrivateUsage : -1;
		QueryPerformanceCounter((LARGE_INTEGER *)&startTime);
	}
	// Add elapsed QPC ticks and private-byte delta to timeMapTotal[functionPath],
	// restore the caller's path, and charge the destructor itself to accumulatedOverheadTime.
	~cProfile()
	{
		if (!logQuestionProfileTime) return;
		__int64 endTime,overheadTime;
		QueryPerformanceCounter((LARGE_INTEGER *)&endTime);
		unordered_map <string,CP>::iterator tmi;
		if ((tmi=timeMapTotal.find(functionPath.c_str()))==timeMapTotal.end())
		{
			timeMapTotal[functionPath.c_str()].t=0;
			tmi=timeMapTotal.find(functionPath.c_str());
			tmi->second.c=0;
			tmi->second.m=0;
			tmi->second.mwc=0;
		}
		tmi->second.t+=(endTime-startTime);
		tmi->second.c++;
		PROCESS_MEMORY_COUNTERS_EX memCounter;
		BOOL ret=GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memCounter,sizeof(memCounter));
		if (ret && startPrivateBytes>0)
			tmi->second.m+=memCounter.PrivateUsage - startPrivateBytes;
		//lplog(LOG_INFO,L"lfe %S %I64d %I64d",functionPath.c_str(),endTime,timeMapTotal[functionPath.c_str()]);
		QueryPerformanceCounter((LARGE_INTEGER *)&overheadTime);
		accumulatedOverheadTime+=overheadTime-endTime;
		totalCount++;
		functionPath=saveFunctionPath;
	}
	static __int64 cb;
	static unordered_map <string ,__int64 > counterMap;
	static unordered_map <string ,int > counterNumMap;
	// Start (or restart) the ad-hoc QPC interval stored in cb.
	static void counterBegin(void)
	{
		QueryPerformanceCounter((LARGE_INTEGER *)&cb);
	}

	// Add (now-cb) to counterMap[countType], bump counterNumMap, then restart cb
	// so consecutive counterEnd calls measure a chain of intervals.
	static void counterEnd(const char *countType)
	{
		__int64 endcb;
		QueryPerformanceCounter((LARGE_INTEGER *)&endcb);
		endcb-=cb;
		unordered_map <string ,__int64 >::iterator cmi=counterMap.find(countType);
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

	// printf each counterMap bucket as ticks and percent of the sum.  Divides by
	// total without a zero check - do not call if no counterEnd has run.
	static void printCounters()
	{
		__int64 total=0;
		for (unordered_map <string ,__int64 >::iterator cmi=counterMap.begin(),cmiEnd=counterMap.end(); cmi!=cmiEnd; cmi++)
			total+=cmi->second;
		for (unordered_map <string ,__int64 >::iterator cmi=counterMap.begin(),cmiEnd=counterMap.end(); cmi!=cmiEnd; cmi++)
		{
			unordered_map <string ,int >::iterator cmci=counterNumMap.find(cmi->first);
			printf("%s:%07I64d %02I64d%% [COUNT:%d]\n",cmi->first.c_str(),cmi->second,cmi->second*100/total,cmci->second);
		}
	}

	// Log one timeMapTotal entry.  Empty 'function' also dumps SQL wait and
	// flushes accumulateNetworkTime.  timeMapTotal[f] is a CP, not an __int64;
	// the %I64d happens to print CP::t on MSVC.
	static void lfprint(char *function,int num)
	{
		if (num>0)
		{
			char f[1024];
			strcpy(f,function);
			sprintf(f+strlen(f)," line:%d",num);
			lplog(LOG_INFO,L"TIME %S:%I64d",f,timeMapTotal[f]);
		}
		else
			lplog(LOG_INFO,L"TIME %S:%I64d",function,timeMapTotal[function]);
		if (strlen(function)==0)
		{
			lplog(LOG_INFO,L"SQL: %08I64d ms waitTime=%0I64d",mySQLTotalTime,totalInternetTimeWaitBandwidthControl);
			accumulateNetworkTime(L"",0,0);
		}
	}

	// Close out 'profile', then dump TIME / MEMORY / COUNT reports for every
	// functionPath, with direct-child attribution (paths that add exactly one
	// more ':' segment).  No-op when logQuestionProfileTime is 0.
	static void lfprint(cProfile &profile)
	{
		if (!logQuestionProfileTime) return;
		__int64 endTime,total,memoryTotal=0;
		QueryPerformanceCounter((LARGE_INTEGER *)&endTime);
		unordered_map <string,CP>::iterator tmi;
		if (functionPath.length()==0)
			functionPath=profile.functionPath;
		if ((tmi=timeMapTotal.find(functionPath.c_str()))==timeMapTotal.end())
			total=timeMapTotal[functionPath.c_str()].t=(endTime-profile.startTime);
		else
			total=tmi->second.t+=(endTime-profile.startTime);
		PROCESS_MEMORY_COUNTERS_EX memCounter;
		BOOL ret=GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memCounter,sizeof(memCounter));
		if (ret>0)
			memoryTotal=memCounter.PrivateUsage;
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		lplog(LOG_INFO,L"TIME TOTALS:%I64d ms %I64d s (%I64d minutes)",total*1000/frequency.QuadPart,total/frequency.QuadPart,total/frequency.QuadPart/60);
		lplog(LOG_INFO,L"TIME PROFILE OVERHEAD:%I64d ms %I64d%%",accumulatedOverheadTime*1000/frequency.QuadPart,accumulatedOverheadTime*100/total);
		for (unordered_map<string,CP>::iterator ti=timeMapTotal.begin(),tiEnd=timeMapTotal.end(); ti!=tiEnd; ti++)
			timeSort.insert(ti);
		for (set<unordered_map<string,CP>::iterator>::iterator si=timeSort.begin(),siEnd=timeSort.end(); si!=siEnd; si++)
		{
			__int64 directChildrenMemory=0;
			__int64 directChildrenTime=0;
			for (set<unordered_map<string,CP>::iterator>::iterator ssi=timeSort.begin(),ssiEnd=timeSort.end(); ssi!=ssiEnd; ssi++)
				if ((*si)->first.length()<(*ssi)->first.length() && (*ssi)->first.substr(0,(*si)->first.length())==(*si)->first &&
						(*ssi)->first.find(':',(*si)->first.length()+1)==string::npos)
				{
					directChildrenMemory+=(*ssi)->second.m;
					directChildrenTime+=(*ssi)->second.t;
				}
			(*si)->second.mwc=(*si)->second.m-directChildrenMemory;
			__int64 percentage=(*si)->second.t*100/total;
			if (percentage>0)
			{
				__int64 ms=(*si)->second.t*1000/frequency.QuadPart;
				__int64 withoutChildrenMS=((*si)->second.t-directChildrenTime)*1000/frequency.QuadPart;
				__int64 withoutChildrenPercentage=((*si)->second.t-directChildrenTime)*100/total;
				if (withoutChildrenPercentage>0 || percentage>20 || (*si)->second.c>1000000)
				{
					lplog(LOG_INFO,L"TIME %S:%I64d ms %I64d%% [without children %I64d ms %I64d%%] count=%I64d",(*si)->first.c_str(),ms,percentage,withoutChildrenMS,withoutChildrenPercentage,(*si)->second.c);
					for (set<unordered_map<string,CP>::iterator>::iterator ssi=timeSort.begin(),ssiEnd=timeSort.end(); ssi!=ssiEnd; ssi++)
					{
						__int64 msts=(*ssi)->second.t*1000/frequency.QuadPart;
						__int64 percentage2=(*ssi)->second.t*100/(*si)->second.t;
						__int64 percentageOfTotal=(*ssi)->second.t*100/total;
						if (percentage2>0 && (*si)->first.length()<(*ssi)->first.length() && (*ssi)->first.substr(0,(*si)->first.length())==(*si)->first)
						{
							string theRest=(*ssi)->first.c_str()+(*si)->first.length()+(((*si)->first.length()) ? 1 : 0);
							if (theRest.find(':')==string::npos)
								lplog(LOG_INFO,L"  %S:%I64d ms [%I64d%% parent] [%I64d%% total]",theRest.c_str(), msts,percentage2,percentageOfTotal);
						}
					}
				}
			}
		}
		for (unordered_map<string,CP>::iterator ti=timeMapTotal.begin(),tiEnd=timeMapTotal.end(); ti!=tiEnd; ti++)
		{
			memorySort.insert(ti);
		  countSort.insert(ti);
		}
		for (set<unordered_map<string,CP>::iterator>::iterator si=memorySort.begin(),siEnd=memorySort.end(); si!=siEnd; si++)
		{
			__int64 percentage=(*si)->second.m*100/memoryTotal;
			if (percentage>0)
			{
				__int64 KB=(*si)->second.m/1000;
				__int64 withoutChildrenKB=((*si)->second.mwc)/1000;
				__int64 withoutChildrenPercentage=((*si)->second.mwc)*100/memoryTotal;
				if (withoutChildrenPercentage>0 || percentage>20 || (*si)->second.c>1000000)
				{
					lplog(LOG_INFO,L"MEMORY %S:%I64d KB %I64d%% [without children %I64d KB %I64d%%] count=%I64d",
						(*si)->first.c_str(),KB,percentage,withoutChildrenKB,withoutChildrenPercentage,(*si)->second.c);
					for (set<unordered_map<string,CP>::iterator>::iterator ssi=memorySort.begin(),ssiEnd=memorySort.end(); ssi!=ssiEnd; ssi++)
					{
						__int64 msKB=(*ssi)->second.m/1000;
						__int64 msPercentage=(*ssi)->second.m*100/(*si)->second.m;
						__int64 msPercentageOfTotal=(*ssi)->second.m*100/memoryTotal;
						if (msPercentage>0 && (*si)->first.length()<(*ssi)->first.length() && (*ssi)->first.substr(0,(*si)->first.length())==(*si)->first)
						{
							string theRest=(*ssi)->first.c_str()+(*si)->first.length()+1;
							if (theRest.find(':')==string::npos)
								lplog(LOG_INFO,L"  %S:%I64d KB [%I64d%% parent] [%I64d%% total]",theRest.c_str(),msKB,msPercentage,msPercentageOfTotal);
						}
					}
				}
			}
		}
		int countListMax=0;
		for (set<unordered_map<string,CP>::iterator>::iterator si=countSort.begin(),siEnd=countSort.end(); si!=siEnd && countListMax<40; si++,countListMax++)
		{
			string func=(*si)->first;
			size_t lastColon=func.rfind(':');
			if (lastColon!=wstring::npos) 
				func.erase(0,lastColon+1);
			lplog(LOG_INFO,L"COUNT %30S:%I64d",func.c_str(),(*si)->second.c);
			lplog(LOG_INFO,L"      %30S",(*si)->first.c_str());
		}
	}
	// Add one HTTP wait: (now-timer) to the sleep+net total and (now-lNC) to
	// net-only, keyed by the host carved out of 'str' (scheme://host/...).
	// timer==0 still takes the lock (used as a flush from FATAL and lfprint).
	// Casts away const and writes a temporary NUL into str - UB on a literal.
	static void accumulateNetworkTime(const wchar_t *str,int timer,int lNC)
	{ 
		if (!lockInitialized)
			InitializeSRWLock(&networkTimeSRWLock);
 		AcquireSRWLockExclusive(&networkTimeSRWLock);
		int c=clock();
		accumulateNetworkTimeCount++;
		if (timer)
		{
			int t=c-timer,ont=c- lNC;
			accumulationNetworkProfileTimer+=t;
			accumulateOnlyNetTimer+=ont;
			wchar_t *pos=(wchar_t *)wcschr(str,L'/');
			if (pos!=0 && (pos=wcschr(pos+2,L'/'))!=0)
			{
				*pos=0;
				unordered_map <wstring,__int64>::iterator nt=onlyNetTimes.find(str);
				if (nt==onlyNetTimes.end())
				{
					netAndSleepTimes[str]=t;
					onlyNetTimes[str]=ont;
					numTimesPerURL[str]=1;
				}
				else
				{
					netAndSleepTimes[str]+=t;
					nt->second+=ont;
					numTimesPerURL[str]++;
				}
				*pos=L'/';
			}
		}
		//if (accumulationNetworkProfileTimer>0 && (c-lastNetworkTimePrinted>120000 || timer==0))
		//{
		//	lplog(LOG_WHERE,L"%07I64d:accumulated time=%09I64d MS (%I64d hours) accumulated net time=%09I64d MS (%I64d hours,%I64d MS/call)",accumulateNetworkTimeCount,
		//		accumulationNetworkProfileTimer,accumulationNetworkProfileTimer/3600000,accumulateOnlyNetTimer,accumulateOnlyNetTimer/3600000,accumulateOnlyNetTimer/accumulateNetworkTimeCount);
		//	for (unordered_map <wstring,__int64>::iterator nti=onlyNetTimes.begin(),ntEnd=onlyNetTimes.end(); nti!=ntEnd; nti++)
		//		lplog(LOG_WHERE,L"%40s:%07I64d:%09I64d(%02I64d%%) %09I64d(%02I64d%% %I64d MS/call)",nti->first.c_str(),numTimesPerURL[nti->first.c_str()],
		//					netAndSleepTimes[nti->first.c_str()],netAndSleepTimes[nti->first.c_str()]*100/accumulationNetworkProfileTimer,
		//					nti->second,nti->second*100/accumulationNetworkProfileTimer,
		//					(numTimesPerURL[nti->first.c_str()]) ? nti->second/numTimesPerURL[nti->first.c_str()] : 0L);
		//	lastNetworkTimePrinted=c;
		//}
 		ReleaseSRWLockExclusive(&networkTimeSRWLock);
	}
};

#ifdef PROFILEDETAIL
	#define DLFS cProfile profile(__FUNCTION__);
	#define LFSL cProfile profile(__FUNCTION__,__LINE__);
#endif

//#define PROFILE
#ifdef PROFILE
	#define DLFS 
	#define LFS cProfile profile(__FUNCTION__);
	#define LFSL 
#else
	#define DLFS 
	#define LFS 
	#define LFSL 
#endif

