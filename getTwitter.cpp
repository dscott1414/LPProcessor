																																																									 #include <errno.h>
#include <windows.h>
#include "WinInet.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "errno.h"
#include <time.h>
#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
using namespace std;
#include <direct.h>
#include "word.h"
#include "source.h"
#include "internet.h"

// ethereal parameters to track packets
// host 64.15.203.18
// TCP PORT 80

#define MAX_BUF 120000 // try to read file in one gulp
// e-mail: dscott1414@yahoo.com
// Twitter@DavidScott17
// password: builder!12

/*
<entry>
		<id>tag:search.twitter.com,2005:15981891232468992</id>
		<published>2010-12-18T04:09:14Z</published>
		<link type="text/html" rel="alternate" href="http://twitter.com/izzybesneaky/statuses/15981891232468992"/>
		<title>I heard my Momz laugh :) knowing she's happy makes me happy :D #nohomo lol</title>
		<content type="html">I heard my Momz laugh :) knowing she&amp;apos;s happy makes me happy :D &lt;a href=&quot;http://search.twitter.com/search?q=%23nohomo&quot; onclick=&quot;pageTracker._setCustomVar(2, 'result_type', 'recent', 3);pageTracker._trackPageview('/intra/hashtag/#nohomo');&quot;&gt;#nohomo&lt;/a&gt; lol</content>
		<updated>2010-12-18T04:09:14Z</updated>
		<link type="image/png" rel="image" href="http://a2.twimg.com/profile_images/1175759486/IMAG0038_normal.jpg"/>
		<twitter:geo>
		</twitter:geo>
		<twitter:metadata>
			<twitter:result_type>recent</twitter:result_type>
		</twitter:metadata>
		<twitter:source>&lt;a href=&quot;http://twitter.com/&quot;&gt;web&lt;/a&gt;</twitter:source>
		<twitter:lang>en</twitter:lang>
		<author>
			<name>izzybesneaky (IZZY :D)</name>
			<uri>http://twitter.com/izzybesneaky</uri>
		</author>
	</entry>
	*/

wstring I64ToS(__int64 i,wstring &tmp)
{
	wchar_t temp[1024];
	_i64tow(i,temp,10);
	return tmp=temp;
}

// “:D”     :-) :) :o) :] :3 :c) :> =] 8) =) :} :^)
// “:-(”, “:(”, “=(”, “;(”
// Dollar ("$") 24
// Ampersand ("&") 26
// Plus ("+") 2B
// Comma (",") 2C
// Forward slash/Virgule ("/") 2F
// Colon (":") 3A
// Semi-colon (";") 3B
// Equals ("=") 3D
// Question mark ("?") 3F
// 'At' symbol ("@") 40

void logCurrentTime(void)
{
	time_t seconds = time (NULL);
	struct tm *day=gmtime(&seconds);
	lplog(L"%d-%d-%d",day->tm_mon,day->tm_mday,1900+day->tm_year);
}

extern wstring logFileExtension; 
int getTwitterEntries(wchar_t *filter)
{
	// happy
	wstring baseURL=L"http://search.twitter.com/search.atom?lang=en&rpp=100&q="; // ORq=%3A-)ORq=%3D) - removed - actually decreases results!
	
	// %3A) happy
	// %3A( sad
	baseURL+=filter;
	cInternet::bandwidthControl=0;
	__int64 lastId=-1;
	int numTotalPerQueryCollected=0;
	set <__int64> tweets;
	logCache=0;
	wchar_t logbuf[1024];
	_swprintf(logbuf,L".Twitter.%s",filter);
	logFileExtension=logbuf;
	while (true)
	{
		logCurrentTime();
		wstring buffer,URL=baseURL,tmp;
		if (lastId>=0)
			URL+=L"&since_id="+I64ToS(lastId,buffer);
		wstring consoleTitle=L"# tweets total="+itos(tweets.size(),buffer)+L" perQuery="+itos(numTotalPerQueryCollected,tmp)+wstring(L" filter=")+filter;
		SetConsoleTitle(consoleTitle.c_str());
		numTotalPerQueryCollected=0;
		for (int page=1; page<15; page++)
		{
			wstring pagedURL=URL+L"&page="+itos(page,buffer);
			int pos=-1,ret,numCollected=tweets.size();
			if (ret= cInternet::readPage(pagedURL.c_str(),buffer)) return ret;
			wstring entry;
			int numEntriesPerPage=0;
			for (; (pos=takeLastMatch(buffer,L"<entry>",L"</entry>",entry,false))>=0; numEntriesPerPage++)
			{
				wstring sid,title;
				const wchar_t *wch;
				pos=takeLastMatch(entry,L"<id>",L"</id>",sid,false);
				if ((wch=wcschr(sid.c_str(),L':'))!=NULL && (wch=wcschr(wch+1,L':'))!=NULL)
				{
					lastId=_wtoi64(wch+1);
					pos=takeLastMatch(entry,L"<title>",L"</title>",title,false);
					if (tweets.find(lastId)==tweets.end())
					{
						printf("%010I64d:%lS\n",lastId,title.c_str());
						lplog(L"%010I64d:%s",lastId,title.c_str());
						tweets.insert(lastId);
						numTotalPerQueryCollected++;
					}
				}
			}
			if (numEntriesPerPage==0 || (tweets.size()-numCollected)==0) break;
		}
		Sleep(1000*60*10);
	}
	return 0;
}


