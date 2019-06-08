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

bool checkLinkExists(wstring link)
{
	int fpos=link.find_last_of(L'/',link.length()-1);
	link[fpos]=L'_';
	fpos=link.find_last_of(L'/',fpos-1);
	// wstring filepath=L"interviews\\GHARIB_ONE_ON_ONE_"+link.substr(fpos+1); Susie Gharib
	wstring filepath=L"interviews\\MOYERS_"+link.substr(fpos+1); 
	return _waccess(filepath.c_str(),0)==0;
}

#define MAX_DOC_NUM 100000
int getInterviewTranscriptGharibMoyers(	)
{
	// Susie Gharib interviews
	// wstring baseURL=L"http://www.pbs.org/nbr/info/search.html?q=site%3Awww.pbs.org%2Fnbr%2Fsite%2Fonair+%22Susie+Gharib%22"; // &start=100&num=10
	// int intervalSize=10;
	wstring baseURL=L"http://www.pbs.org/moyers/journal/archives/archives.php?start="; // 80
	int intervalSize=20;
	Internet::bandwidthControl=0;
	wstring tmp;
	for (int docNum=0; docNum<MAX_DOC_NUM; docNum+=intervalSize) 
	{
		wstring header=L"-------------------------------"+itos(docNum,tmp)+L"\n";
		wprintf(header.c_str());
		// wstring URL=baseURL+L"&start="+itos(docNum,tmp)+L"&num=10";  Susie Gahrib
		wstring URL=baseURL+itos(docNum,tmp);
		int ret; 
		wstring buffer;
		if (ret=Internet::readPage(URL.c_str(),buffer)) return ret;
		size_t pos = wstring::npos;
		__int64 rpos = wstring::npos;
		wstring match,link;
		 
		// while ((pos=firstMatch(buffer,L"<b class=\"result-title\">",L"</b>",pos,match,false))>=0) Susie Gharib
			//if (((rpos=firstMatch(match,L"<a href=\"",L"\">",rpos,link,false)))>=0) // Susie Gharib
			while (((int)(pos=takeLastMatch(buffer,L"<a href=\"",L"\">Transcript</a>",link,false)))>=0) 
			{
				wstring linkbuffer,sections,away,linklink;
				if (checkLinkExists(link)) continue;
				if (ret= Internet::readPage(link.c_str(),linkbuffer)) continue;
				//int ipos=-1;
				//while (firstMatch(linkbuffer,L"<!--begin image-->",L"<!--end image-->",ipos,away,false)>=0);
				// int spos=-1;
				//if (((rpos=firstMatch(linkbuffer,L"<!--begin page-->",L"<!--end page-->",spos,sections,false)))>=0)
				bool transcriptFound=false;
				linklink.erase();
				while (((int)(pos=takeLastMatch(linkbuffer,L"<div id=\"",L"</div>",sections,false)))>=0) 
				{
					if (sections.find(L"Read Transcript")!=wstring::npos)
					{
						wstring rlink;
						while (((int)(pos=takeLastMatch(sections,L"<a href=\"",L"alt=\"Read Transcript\"",rlink,false)))>=0)
						{
							int fpos=rlink.find(L'"');
							if (fpos!=wstring::npos)
								linklink=rlink.substr(0,fpos-1);
						}
					}
					if (sections.find(L"watchtranscript")!=wstring::npos)
					{
						transcriptFound=true;
						while (((int)(pos=firstMatch(sections,L"<img",L">",(size_t)pos,away,false)))>=0);
						firstMatch(sections,L"watchtranscript",L">", (size_t)pos,away,false);
						wprintf(L"%s\n",link.c_str()); 
						int fpos=link.find_last_of(L'/',link.length()-1);
						link[fpos]=L'_';
						fpos=link.find_last_of(L'/',fpos-1);
						// wstring filepath=L"interviews\\GHARIB_ONE_ON_ONE_"+link.substr(fpos+1); Susie Gharib
						wstring filepath=L"interviews\\MOYERS_"+link.substr(fpos+1); 
						int fd=_wopen(filepath.c_str(),_O_CREAT|_O_RDWR);
						if (fd>=0)
						{
							_write(fd,sections.c_str(),sections.length());
							close(fd);
						}
						break;
					}
				}
				if (!transcriptFound)
				{
					wprintf(L"transcript not found: %s\n",link.c_str()); 
					if (linklink.length())
					{
						if (linklink.find(L"/")==wstring::npos)
						{
							int fpos=link.find_last_of(L'/',link.length()-1);
							linklink=link.substr(0,fpos+1)+linklink;
						}
						wprintf(L"redirect to: %s\n",linklink.c_str()); 
						if (!checkLinkExists(linklink) && (ret= Internet::readPage(linklink.c_str(),linkbuffer))==0)
						{
							while (((int)(pos=takeLastMatch(linkbuffer,L"<div id=\"",L"</div>",sections,false)))>=0) 
							{
								if (sections.find(L"watchtranscript")!=wstring::npos)
								{
									transcriptFound=true;
									while (((int)(pos=firstMatch(sections,L"<img",L">",(size_t)pos,away,false)))>=0);
									firstMatch(sections,L"watchtranscript",L">",(size_t)pos,away,false);
									wprintf(L"%s\n",link.c_str()); 
									int fpos=link.find_last_of(L'/',link.length()-1);
									link[fpos]=L'_';
									fpos=link.find_last_of(L'/',fpos-1);
									// wstring filepath=L"interviews\\GHARIB_ONE_ON_ONE_"+link.substr(fpos+1); Susie Gharib
									wstring filepath=L"interviews\\MOYERS_"+link.substr(fpos+1); 
									int fd=_wopen(filepath.c_str(),_O_CREAT|_O_RDWR);
									if (fd>=0)
									{
										_write(fd,sections.c_str(),sections.length());
										close(fd);
									}
								}
							}
						}
					}
				}
				rpos=-1;
			}
	}
	return 0;
}


int getInterviewTranscript(	)
{
	wstring baseURL=L"http://www.npr.org/templates/rundowns/rundown.php?prgId=2&prgDate="; //11-1-2005"
	Internet::bandwidthControl=0;
	wstring tmp;
	//The time is represented as seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC). 
	// we want everything after 1977.
	time_t timer=(35*365+11*31)*24*3600; // November 1st 2005 is the first date with transcripts for ATC
	while (true)
	{
		struct tm *day=gmtime(&timer);
		timer+=24*3600;
		wchar_t URL[1024];
		wsprintf(URL,L"%s%d-%d-%d",baseURL.c_str(),day->tm_mon,day->tm_mday,1900+day->tm_year);
		wprintf(L"Day %I64d [%s]\n",timer/(24*3600),URL);
		int ret; 
		wstring buffer;
		if (ret= Internet::readPage(URL,buffer)) return ret;
		int pos=-1,rpos=-1;
		wstring match,link;
		// <a class="transcript" href="http://www.npr.org/templates/story/story.php?storyId=4981602">Transcript</a>
			while ((pos=takeLastMatch(buffer,L"<a class=\"transcript\" href=\"",L"\">Transcript</a>",link,false))>=0) 
			{
				wstring linkbuffer,sections,away;
				if (checkLinkExists(link)) continue;
				if (ret= Internet::readPage(link.c_str(),linkbuffer)) continue;
				bool transcriptFound=false;
				while ((pos=takeLastMatch(linkbuffer,L"<div class=\"transcript\">",L"</div>",sections,false))>=0) 
				{
						transcriptFound=true;
						wprintf(L"%s\n",link.c_str()); 
						int fpos=link.find_last_of(L'=',link.length()-1);
						wstring filepath=L"interviews\\ATC_"+link.substr(fpos+1); 
						int fd=_wopen(filepath.c_str(),_O_CREAT|_O_RDWR);
						if (fd>=0)
						{
							_write(fd,sections.c_str(),sections.length());
							close(fd);
						}
						break;
				}
				if (!transcriptFound)
				{
					wprintf(L"transcript not found: %s\n",link.c_str()); 
				}
				rpos=-1;
			}
	}
	return 0;
}


