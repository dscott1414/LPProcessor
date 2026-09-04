/*
	getTwitter.cpp - One-shot Twitter Atom search scraper (legacy search.twitter.com)

	Overview:
		Polls the long-retired Twitter Atom search API for a query string, extracts
		<entry> blocks, and logs unique tweet IDs + titles. Sleeps ten minutes between
		query cycles. Never returns on the success path.

	Pipeline position:
		Standalone acquisition utility; not called from the novel-parse pipeline.

	Key entry points:
		- I64ToS() - formats an int64_t into tmp
		- logCurrentTime() - logs UTC month-day-year
		- getTwitterEntries() - infinite scrape loop for 'filter'

	Dependencies:
		cInternet::readPage, WinInet, Windows console title.

	Notes / gotchas:
		'filter' is percent-encoded (via encodeURL, defined in createOntology.cpp) before
		being appended to the URL. lastId tracks the maximum tweet id seen (used as
		since_id on the next cycle), not just whichever entry was processed last.
		while(true) only exits on HTTP error.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <thread>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "lpProcess.h"
#include "errno.h"
#include <time.h>
#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
using namespace std;
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "internet.h"

void encodeURL(lpwstring winput, lpwstring& wencodedURL); // defined in createOntology.cpp

// ethereal parameters to track packets
// host 64.15.203.18
// TCP PORT 80

#define MAX_BUF 120000 // try to read file in one gulp
// Twitter@DavidScott17

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

// Writes decimal i into tmp (via a 1024-wchar stack buffer) and returns tmp.
lpwstring I64ToS(int64_t i, lpwstring& tmp)
{
	lpchar_t temp[1024];
	lp_i64tow(i, temp);
	return tmp = temp;
}

// �:D�     :-) :) :o) :] :3 :c) :> =] 8) =) :} :^)
// �:-(�, �:(�, �=(�, �;(�
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

// Logs the current UTC date as month-day-year (month is 0-based, same as tm_mon).
void logCurrentTime(void)
{
	time_t seconds = time(NULL);
	struct tm* day = gmtime(&seconds);
	lplog(u"%d-%d-%d", day->tm_mon, day->tm_mday, 1900 + day->tm_year);
}

extern thread_local lpwstring logFileExtension; // batch B5: matches logging.h
// Scrapes search.twitter.com Atom for 'filter' forever. Returns only on readPage error.
// not currently called from anywhere in the tree (see file header).
int getTwitterEntries(lpchar_t* filter)
{
	// happy
	lpwstring baseURL = u"https://search.twitter.com/search.atom?lang=en&rpp=100&q="; // ORq=%3A-)ORq=%3D) - removed - actually decreases results!

	// %3A) happy
	// %3A( sad
	// escape filter before interpolating it into the query string (spaces/'&'/etc would otherwise
	// corrupt or truncate the request); if a caller ever needs to pass pre-built query syntax
	// (raw "q="/"OR" clauses, already-percent-escaped emoticons) that caller will need its own
	// query-construction path instead of a single opaque 'filter' string.
	lpwstring uFilter;
	encodeURL(filter, uFilter);
	baseURL += uFilter;
	cInternet::bandwidthControl = 0;
	int64_t lastId = -1;
	int numTotalPerQueryCollected = 0;
	set <int64_t> tweets;
	logCache = 0;
	lpchar_t logbuf[1024];
	lp_snprintf(logbuf, 1024, u".Twitter.%s", filter); // batch B10: bounded
	logFileExtension = logbuf;
	while (true)
	{
		logCurrentTime();
		lpwstring buffer, URL = baseURL, tmp;
		if (lastId >= 0)
			URL += u"&since_id=" + I64ToS(lastId, buffer);
		lpwstring consoleTitle = u"# tweets total=" + itos(tweets.size(), buffer) + u" perQuery=" + itos(numTotalPerQueryCollected, tmp) + lpwstring(u" filter=") + filter;
		lpReportProgress(consoleTitle.c_str());
		numTotalPerQueryCollected = 0;
		for (int page = 1; page < 15; page++)
		{
			lpwstring pagedURL = URL + u"&page=" + itos(page, buffer);
			int pos = -1, ret, numCollected = tweets.size();
			if (ret = cInternet::readPage(pagedURL.c_str(), buffer)) return ret;
			lpwstring entry;
			int numEntriesPerPage = 0;
			for (; (pos = takeLastMatch(buffer, u"<entry>", u"</entry>", entry, false)) >= 0; numEntriesPerPage++)
			{
				lpwstring sid, title;
				const lpchar_t* wch;
				pos = takeLastMatch(entry, u"<id>", u"</id>", sid, false);
				if ((wch = lp_strchr(sid.c_str(), u':')) != NULL && (wch = lp_strchr(wch + 1, u':')) != NULL)
				{
					int64_t thisId = lp_wtoll(wch + 1);
					if (thisId > lastId)
						lastId = thisId; // track the maximum id seen (used as since_id next cycle), not just whichever entry was processed last
					pos = takeLastMatch(entry, u"<title>", u"</title>", title, false);
					if (tweets.find(thisId) == tweets.end())
					{
						printf("%010I64d:%lS\n", thisId, title.c_str());
						lplog(u"%010I64d:%s", thisId, title.c_str());
						tweets.insert(thisId);
						numTotalPerQueryCollected++;
					}
				}
			}
			if (numEntriesPerPage == 0 || (tweets.size() - numCollected) == 0) break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 60 * 10));
	}
	return 0;
}


