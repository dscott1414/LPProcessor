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
#include "logging.h"
#include <mysql.h>
#include "general.h"
// ethereal parameters to track packets
// host 64.15.203.18
// TCP PORT 80

const wchar_t *getLastErrorMessage(wstring &out);
#define MAX_BUF 120000 // try to read file in one gulp
#define MAX_LINK 2048
#define SLEEP_PER_REQUEST 0
#define KB_THROTTLE 30

int lostDoc=0,retry=0,noTitle=0;  // not part of source parallel processing
__int64 originalBytes=0; // not part of source parallel processing
bool readTimeoutError=false; // not part of source parallel processing

typedef struct 
{
	HINTERNET RequestHandle;
	char *buffer;
	int bufsize;
	DWORD *dwRead;
} tIRFW;

DWORD WINAPI InternetReadFile_Child(void *vThreadParm)
{
	tIRFW *p=(tIRFW *) vThreadParm;
	if (!InternetReadFile( p->RequestHandle, p->buffer, p->bufsize, p->dwRead))
	{
		wstring lem;
		lplog(LOG_ERROR,L"InternetReadFile reports %s.",getLastErrorMessage(lem));
		return 1;
	}
	return 0;
}
 
bool InternetReadFile_Wait(	HINTERNET RequestHandle,char *buffer,int bufsize,DWORD *dwRead)
{
	tIRFW p;
	readTimeoutError=false;
	p.buffer=buffer;
	p.bufsize=bufsize;
	p.RequestHandle=RequestHandle;
	p.dwRead=dwRead;
	*dwRead=0;
	// Create a worker thread
	DWORD    dwThreadID;
	HANDLE hThread = CreateThread(
										 NULL,            // Pointer to thread security attributes
										 0,               // Initial thread stack size, in bytes
										 InternetReadFile_Child,  // Pointer to thread function
										 &p,     // The argument for the new thread
										 0,               // Creation flags
										 &dwThreadID      // Pointer to returned thread identifier
								 );

	// Wait for the call to InternetConnect in worker function to complete
	DWORD dwTimeout = 5*60*1000; // in milliseconds
	if ( WaitForSingleObject ( hThread, dwTimeout ) == WAIT_TIMEOUT )
	{
		 InternetCloseHandle ( RequestHandle );
		 CloseHandle (hThread);
		 wprintf(L"\nRetry on document (InternetReadFile failure).\n");
		 readTimeoutError=true;
		 // Wait until the worker thread exits
		 // WaitForSingleObject ( hThread, INFINITE );
		 return false;
	}
	// The state of the specified object (thread) is signaled
	DWORD   dwExitCode = 0;
	if ( !GetExitCodeThread( hThread, &dwExitCode ) ) 
	{
		CloseHandle (hThread);
		return false;
	}
	CloseHandle (hThread);
	return dwExitCode==0;
}

int substNextDocNum(wstring URL,wstring &nextURL)
{
	wstring pDocNum=L"p_docnum=";
	int whereDocNum=URL.find(pDocNum);
	if (whereDocNum<0) return -1;
	whereDocNum+=pDocNum.length();
	int endDocNum=URL.find(L"&",whereDocNum);
	wstring sDocNum=URL.substr(whereDocNum,endDocNum-whereDocNum);
	int docnum=_wtoi(sDocNum.c_str());
	docnum++;
	wchar_t cDocNum[10];
	_itow(docnum,cDocNum,10);
	nextURL=URL;
	nextURL.replace(whereDocNum,endDocNum-whereDocNum,cDocNum);
	return 0;
}

int getArticle(	HINTERNET hFile,wstring refer,wstring URL,bool &last,wstring &nextURL,FILE *nb,int docNum,__int64 &totalBytes,int &numWords)
{
	HINTERNET RequestHandle;
	//wchar_t wlink[MAX_LINK];
	//memset(wlink,0,MAX_LINK*2);
	//MultiByteToWideChar(CP_ACP,0,URL.c_str(),URL.length(),wlink,MAX_LINK);
	LPCTSTR lpszVerb=L"GET";
	LPCTSTR lpszObjectName=URL.c_str();
	LPCTSTR lpszVersion=NULL;
	//wstring referString=L"http://infoweb.newsbank.com/iw-search/we/InfoWeb?p_action=list&d_sources=location&p_product=NewsBank&p_theme=aggregated4&p_nbid=";
	//wchar_t wlink2[MAX_LINK];
	//memset(wlink2,0,MAX_LINK*2);
	//MultiByteToWideChar(CP_ACP,0,refer.c_str(),refer.length(),wlink2,MAX_LINK);
	LPCTSTR lpszReferer=refer.c_str();
	LPCTSTR szAcceptTypes[20];
	szAcceptTypes[0]=L"image/gif";
	szAcceptTypes[1]=L"image/x-xbitmap";
	szAcceptTypes[2]=L"image/jpeg";
	szAcceptTypes[3]=L"image/pjpeg";
	szAcceptTypes[4]=L"application/x-shockwave-flash";
	szAcceptTypes[5]=L"application/vnd.ms-excel";
	szAcceptTypes[6]=L"application/vnd.ms-powerpoint";
	szAcceptTypes[7]=L"application/msword";
	szAcceptTypes[8]=L"*/*";
	szAcceptTypes[9]=NULL;
	DWORD dwFlags=INTERNET_FLAG_KEEP_CONNECTION;
	DWORD_PTR dwContext=NULL;
	Sleep(SLEEP_PER_REQUEST);
	RequestHandle=HttpOpenRequest(hFile,lpszVerb,lpszObjectName,lpszVersion,lpszReferer,szAcceptTypes,dwFlags,dwContext);
	wstring lem;
	if (RequestHandle==NULL) 
	{
		lplog(L"ERROR:%d:Cannot open request (1) - %s.",docNum,getLastErrorMessage(lem));
		return -3; // retry
	}
	LPCTSTR lpszHeaders=L"Accept-Language: en-us\r\nAccept-Encoding: gzip, deflate\r\n";
	DWORD dwHeadersLength=wcslen(lpszHeaders);
	LPVOID lpOptional=NULL;
	DWORD dwOptionalLength=0;
	BOOL success;
	success=HttpSendRequest(RequestHandle,lpszHeaders,dwHeadersLength,lpOptional,dwOptionalLength);
	if (!success)
	{
		if (!retry)
		{
			lplog(L"ERROR:%d:Cannot send request (1) - %s.\n",docNum,getLastErrorMessage(lem));
			wprintf(L"\nRetry on document %d.\r",docNum);
		}
		else
		{
			Sleep(1000*retry);
			wprintf(L"Retry on document %d (%d).\r",docNum,retry);
			if (retry>5) 
			{
				wprintf(L"\nAutodial hangup on document %d (%d).\n",docNum,retry);
				InternetAutodialHangup(0);
			}
		}
		retry++;
		InternetCloseHandle(RequestHandle);
		return -3; // retry
	}
	if (retry)
	{
		wprintf(L"\n");
		retry=0;
	}
	char buffer[MAX_BUF+4];
	wstring searchResponse;
	DWORD dwRead,fileBytesRead=0;
	while ( InternetReadFile_Wait( RequestHandle, buffer, MAX_BUF, &dwRead ) )
	{
		if ( dwRead == 0 )
			break;
		buffer[dwRead] = 0;
		totalBytes+=dwRead;
		fileBytesRead+=dwRead;
		wstring t;
		searchResponse+=mTW(buffer,t);
	}
	InternetCloseHandle(RequestHandle);
	if (readTimeoutError) return -3;
	//searchResponse.find(L"com.convera.hl.RWNotFoundException"); // int temp=
	bool checkDocNotFound=
		(searchResponse.find(L"com.convera.hl.RWNotFoundException")!=-1) ||
		(searchResponse.find(L"com.newsbank.util.NException")!=-1);
	if (checkDocNotFound)
	{
		lplog(L"ERROR:%d:Doc not found\n",docNum);
		lostDoc++;
		if (lostDoc>5) 
		{
			last=true;
			return -2;
		}
		last=false;
		substNextDocNum(URL,nextURL);
		return 0;
	}
	int whereLastEnd=searchResponse.find(L"\">Next</a>");
	if (!(last=whereLastEnd<0))
	{
		lostDoc=0;
		wstring whereLastStr=L"<a href=\"";
		int whereLastBegin=searchResponse.rfind(whereLastStr.c_str(),whereLastEnd);
		if (whereLastBegin<0)
		{
			lplog(L"ERROR:%d:Cannot find begin of next URL in:\n%s\n from offset %d",docNum,searchResponse.c_str(),whereLastEnd);
			return -1;
		}
		whereLastBegin+=whereLastStr.length();
		nextURL=searchResponse.substr(whereLastBegin,whereLastEnd-whereLastBegin);
	}
	/*
	<div class="title">MAN OF YEAR' CHOICE DRAWS PROTESTS AT TIME</div>
<div class="info">Boston Globe<br>January 1, 1980<br>Author: Associated Press</div>
<div style="font-weight:bold" class="info">Estimated printed pages: 1</div>
<div style="width:350px" class="artSeparator">
<img src="http://images.newsbank.com/infoweb/agg/shim.gif" height="1"></div>
<div class="text">
<p>More than 200 persons called Time magazine yesterday to complain because the weekly publication designated Iranian leader Ayatollah Ruhollah Khomeini as its 1979 "Man of the Year."<P></P>
<P></P>    Brian Brown, a magazine spokesman, characterized the telephone calls as "negative" and said some persons canceled their subscriptions. He said the number of cancelations had not been tallied.</p>
<P></P>
<P></P>    There were no calls of approval as of yesterday afternoon, he said.<P></P>
<P></P>   The magazine stressed when it announced the designation Sunday that each year since the practice began in 1927, the editors sought to identify the individual who "has done the most to change the news, for better or for worse."<P></P>
<P></P>   On Long Island, Nassau County Executive Francis Purcell canceled all county offices' subscriptions to the weekly news magazine and urged residents to follow suit.<P></P>
<P></P>   "Whatever Khomeini's news value, Time's editors displayed a "callous insensitivity to the feelings of the vast majority of Americans" in choosing him as "Man of the Year," he said.<P></P>
<P></P>   Purcell said Time should have chosen the 50 American hostages in Iran for its cover story in the first issue of the new year.</div>
*/
	wstring searchTitle=L"<div class=\"title\">";
	int beginTitle=searchResponse.find(searchTitle);
	if (beginTitle<0) 
	{
		noTitle++;
		if (noTitle>5) 
		{
			last=true;
			return -2;
		}
		lplog(L"ERROR:%d:Cannot find beginTitle in:\n%s\n",docNum,searchResponse.c_str());
		last=false;
		substNextDocNum(URL,nextURL);
		return 0;
	}
	noTitle=0;
	int endTitle=searchResponse.find(L"</div>",beginTitle);
	if (endTitle<0) 
	{
		lplog(L"ERROR:%d:Cannot find endTitle in:\n%s\n from offset %d",docNum,searchResponse.c_str(),beginTitle);
		last=false;
		substNextDocNum(URL,nextURL);
		return 0;
	}
	beginTitle+=searchTitle.length();
	wstring title=searchResponse.substr(beginTitle,endTitle-beginTitle);

	wstring searchInfo=L"<div class=\"info\">";
	int beginInfo=searchResponse.find(searchInfo);
	if (beginInfo<0)
	{
		lplog(L"ERROR:%d:Cannot find beginInfo in:\n%s\n",docNum,searchResponse.c_str());
		last=false;
		substNextDocNum(URL,nextURL);
		return 0;
	}
	int endInfo=searchResponse.find(L"</div>",beginInfo);
	if (endInfo<0)
	{
		lplog(L"ERROR:%d:Cannot find endInfo in:\n%s\n from offset %d",docNum,searchResponse.c_str(),beginInfo);
		last=false;
		substNextDocNum(URL,nextURL);
		return 0;
	}
	beginInfo+=searchInfo.length();
	wstring info=searchResponse.substr(beginInfo,endInfo-beginInfo);

	wstring searchText=L"<div class=\"text\">";
	int beginText=searchResponse.find(searchText);
	if (beginText<0)
	{
		lplog(L"ERROR:%d:Cannot find beginText in:\n%s",docNum,searchResponse.c_str());
		last=false;
		substNextDocNum(URL,nextURL);
		return 0;
	}
	int endText=searchResponse.find(L"</div>",beginText);
	if (endText<0)
	{
		lplog(L"ERROR:%d:Cannot find endText in:\n%s\n from offset %d",docNum,searchResponse.c_str(),beginText);
		last=false;
		substNextDocNum(URL,nextURL);
		return 0;
	}
	beginText+=searchText.length();
	wstring text=searchResponse.substr(beginText,endText-beginText);
	if (info.find(L"Chicago Tribune")==wstring::npos)
	{
		// eliminate <p>, replace </p> with \n
		int para=0;
		while ((para=text.find(L"<p>",para))>=0)
			text.erase(para,3);
		para=0;
		while ((para=text.find(L"</p>",para))>=0)
			text.replace(para,4,L"\n");
		para=0;
		while ((para=text.find(L"<P>",para))>=0)
			text.erase(para,3);
		para=0;
		while ((para=text.find(L"</P>",para))>=0)
			text.replace(para,4,L"\n");
	}
	else
	{
		int para=0;
		while ((para=text.find(L"<p>",para))>=0)
			text.erase(para,3);
		para=0;
		while ((para=text.find(L"</p>",para))>=0)
			text.replace(para,4,L"\n");
		para=0;
		//while ((para=text.find("<P></P>\n<P></P>",para))>=0)
		//	text.replace(para,15," ");
		//para=0;
		while ((para=text.find(L"<P></P>",para))>=0)
			text.erase(para,7);
		para=0;
		while ((para=text.find(L"``",para))>=0)
			text.replace(para,2,L"\"");
		para=0;
		while ((para=text.find(L".     ",para))>=0)
			text.replace(para,6,L".  \n");
		para=0;
		while ((para=text.find(L"\"     ",para))>=0)
			text.replace(para,6,L"\"  \n");
		para=0;
	}
	for (unsigned int I=0; I<text.length(); I++)
		if (text[I]==L' ' && text[I+1]!=L' ') 
			numWords++;
	fwprintf(nb,L"%s\n\n%s\n\n%s\n~|~\n",title.c_str(),info.c_str(),text.c_str());
	return 0;
}

int acquireNewsBankByDay(struct tm *day,FILE *nb,time_t startTime,__int64 &totalBytes,int &numWords)
{
	HINTERNET hFile;
	HINTERNET hINet;
	hINet = InternetOpen(L"InetURL/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	wstring lem;
	if ( !hINet )
	{
		wprintf(L"\nERROR:InternetOpen Failed - %s",getLastErrorMessage(lem));
		return -1;
	}
	wchar_t *str=L"http://infoweb.newsbank.com/cgi-bin/welcome/libcard.pl/MonmouthCounty";
	hFile=InternetConnect(hINet,L"infoweb.newsbank.com",INTERNET_DEFAULT_HTTP_PORT,L"",L"",INTERNET_SERVICE_HTTP,0,0);
	if (!hFile)
	{
		wprintf(L"\nERROR:Cannot open URL %s - %s.",str,getLastErrorMessage(lem));
		return -1;
	}
	HINTERNET RequestHandle;
	LPCTSTR lpszVerb=L"POST";
	LPCTSTR lpszObjectName=L"/cgi-bin/welcome/libcard.pl/MonmouthCounty";
	LPCTSTR lpszVersion=NULL;
	LPCTSTR lpszReferer=L"http://infoweb.newsbank.com/cgi-bin/welcome/libcard.pl/MonmouthCounty";
	LPCTSTR szAcceptTypes[20];
	szAcceptTypes[0]=L"image/gif";
	szAcceptTypes[1]=L"image/x-xbitmap";
	szAcceptTypes[2]=L"image/jpeg";
	szAcceptTypes[3]=L"image/pjpeg";
	szAcceptTypes[4]=L"application/x-shockwave-flash";
	szAcceptTypes[5]=L"*/*";
	szAcceptTypes[6]=NULL;
	DWORD dwFlags=INTERNET_FLAG_KEEP_CONNECTION;
	DWORD_PTR dwContext=NULL;
	Sleep(SLEEP_PER_REQUEST);
	RequestHandle=HttpOpenRequest(hFile,lpszVerb,lpszObjectName,lpszVersion,lpszReferer,szAcceptTypes,dwFlags,dwContext);
	if (RequestHandle==NULL) 
	{
		wprintf(L"\nERROR:Cannot open request (0) - %s.",getLastErrorMessage(lem));
		return -1;
	}
	BOOL success;
	LPCTSTR lpszHeaders=L"Accept-Language: en-us\r\nContent-Type: application/x-www-form-urlencoded\r\nAccept-Encoding: gzip, deflate\r\n";
	size_t dwHeadersLength=wcslen(lpszHeaders);
	LPVOID lpOptional=L"action=login&user=MonmouthCounty&libcard=80035000052216";
	size_t dwOptionalLength=wcslen((wchar_t *)lpOptional);
	success=HttpSendRequest(RequestHandle,lpszHeaders,dwHeadersLength,lpOptional,dwOptionalLength);
	if (!success)
	{
		wprintf(L"\nERROR:Cannot send request (2) - %s.\n",getLastErrorMessage(lem));
		return -1;
	}
	char buffer[MAX_BUF];
	DWORD dwRead,fileBytesRead=0;
	wstring welcomeBuffer;
	while ( InternetReadFile_Wait( RequestHandle, buffer, MAX_BUF, &dwRead ) )
	{
		if ( dwRead == 0 )
			break;
		buffer[dwRead] = 0;
		totalBytes+=dwRead;
		fileBytesRead+=dwRead;
		wstring t;
		welcomeBuffer+=mTW(buffer,t);
	}
	InternetCloseHandle(RequestHandle);
	if (readTimeoutError) return -3;

	wchar_t *searchForID=L"\">America's Newspapers</a>";
	int endpos=welcomeBuffer.find(searchForID);
	if (endpos<0)
	{
		wprintf(L"\nERROR:Cannot find news link in %s.",welcomeBuffer.c_str());
		return -1;
	}
	wchar_t *searchBeginLink=L"href=\"";
	int savepos=welcomeBuffer.rfind(searchBeginLink,endpos);
	savepos+=wcslen(searchBeginLink);
	wstring link=welcomeBuffer.substr(savepos,endpos-savepos);
	//wchar_t wlink[MAX_LINK];
	//MultiByteToWideChar(CP_ACP,0,link.c_str(),link.length(),wlink,MAX_LINK);
	//wprintf(L"%02d/%02d/%d:%s\n",day->tm_mon+1,day->tm_mday,day->tm_year+1900,wlink);
	/*********************************************************************************/
	lpszVerb=L"POST";
	wstring saveReferString=wstring(L"http://infoweb.newsbank.com")+link;
	wchar_t *wch=(wchar_t *)wcschr(link.c_str(),L'?');
	if (wch) *wch=0;
	lpszObjectName=link.c_str();
	lpszVersion=NULL;
	int tmp=saveReferString.find(L"NEWSARCHIVES");
	if (tmp>=0) saveReferString.resize(tmp+wcslen(L"NEWSARCHIVES"));
	lpszReferer=saveReferString.c_str();
	szAcceptTypes[0]=L"image/gif";
	szAcceptTypes[1]=L"image/x-xbitmap";
	szAcceptTypes[2]=L"image/jpeg";
	szAcceptTypes[3]=L"image/pjpeg";
	szAcceptTypes[4]=L"application/x-shockwave-flash";
	szAcceptTypes[5]=L"application/vnd.ms-excel";
	szAcceptTypes[6]=L"application/vnd.ms-powerpoint";
	szAcceptTypes[7]=L"application/msword";
	szAcceptTypes[8]=L"*/*";
	szAcceptTypes[9]=NULL;
	dwFlags=INTERNET_FLAG_KEEP_CONNECTION;
	dwContext=NULL;
	Sleep(SLEEP_PER_REQUEST);
	RequestHandle=HttpOpenRequest(hFile,lpszVerb,lpszObjectName,lpszVersion,lpszReferer,szAcceptTypes,dwFlags,dwContext);
	if (RequestHandle==NULL) 
	{
		wprintf(L"\nERROR:Cannot open request - %s.",getLastErrorMessage(lem));
		return -1;
	}
	lpszHeaders=L"Accept-Language: en-us\r\nContent-Type: application/x-www-form-urlencoded\r\nAccept-Encoding: gzip, deflate\r\n";
	dwHeadersLength=wcslen(lpszHeaders);
	int whereid=saveReferString.find(L"p_nbid");
	if (whereid<0)
	{
		wprintf(L"\nERROR:p_nbid not found!");
		return -1;
	}
	int untilWhere=saveReferString.find(L"&",whereid);
	string searchURL;
	searchURL="p_product=NewsBank&p_theme=aggregated4&f_event=100&"
		"p_queryname=3&p_action=search&d_search_type=keyword&p_maxdocs=2000&p_perpage=10&d_sources=location&d_place=United+States&"
		"p_field_psudo-sort-0=psudo-sort&d_issuesearch=&p_multi=WPIW%7CCTRB%7CCREB%7CBGBK%7CNYPB%7CASBB%7CBRCB%7CSTLB%7CAMGB%7CDMNB%7CDFWB&"
		"xcal_xpandlevel=1&xcal_xpandlimit=1&p_text_base-0=&p_sort=YMD_date%2Bsource%3AD&p_field_YMD_date-0=YMD_date&p_params_YMD_date-0=date%3AB%2CE&"
		"p_text_YMD_date-0=&p_field_datesel-0=custom&p_bool_YMD_date-0=and&p_field_YMD_date-1=YMD_date&"
		"p_params_YMD_date-1=date%3AB%2CE&p_text_YMD_date-1=";
	char *month="Jan";
	switch (day->tm_mon)
	{
	case 0:month="Jan"; break;
	case 1:month="Feb"; break;
	case 2:month="March"; break;
	case 3:month="April"; break;
	case 4:month="May"; break;
	case 5:month="June"; break;
	case 6:month="July"; break;
	case 7:month="August"; break;
	case 8:month="September"; break;
	case 9:month="October"; break;
	case 10:month="November"; break;
	case 11:month="December"; break;
	}
	char sdate[100];
	sprintf(sdate,"%s+%d%%2C%d+-+%s+%d%%2C%d",month,day->tm_mday,day->tm_year+1900,month,day->tm_mday,day->tm_year+1900);
	char totalSearchURL[10000];
	//sprintf(totalSearchURL,"d_para=no&%S%s%s&Search.x=48&Search.y=13",saveReferString.substr(whereid,untilWhere-whereid+1).c_str(),searchURL.c_str(),sdate);
	sprintf(totalSearchURL,"d_para=no&%S%s%s&Search.x=0&Search.y=0",saveReferString.substr(whereid,untilWhere-whereid+1).c_str(),searchURL.c_str(),sdate);
	lpOptional=totalSearchURL;
	dwOptionalLength=strlen(totalSearchURL);
	success=HttpSendRequest(RequestHandle,lpszHeaders,dwHeadersLength,lpOptional,dwOptionalLength);
	if (!success)
	{
		wprintf(L"\nERROR:Cannot send request (3) - %s.\n",getLastErrorMessage(lem));
		return -1;
	}
	wstring searchURLResponse;
	while ( InternetReadFile_Wait( RequestHandle, buffer, MAX_BUF, &dwRead ) )
	{
		if ( dwRead == 0 )
			break;
		buffer[dwRead] = 0;
		totalBytes+=dwRead;
		fileBytesRead+=dwRead;
		wstring t;
		searchURLResponse+=mTW(buffer,t);
	}
	InternetCloseHandle(RequestHandle);
	if (readTimeoutError) return -3;

	// search this string for every link...
	int searchPos=0;
	bool last;
	if ((searchPos=searchURLResponse.find(L"\" class=\"resultsLink\"",searchPos+1))<0)
	{
		if (searchURLResponse.find(L"There are no articles that contain all the keywords you entered")!=wstring::npos) 
		{
			wprintf(L"\nNo articles for %02d/%02d/%d.\n",day->tm_mon+1,day->tm_mday,day->tm_year+1900);
			lplog(L"No NewsBank articles for %02d/%02d/%d.\n",day->tm_mon+1,day->tm_mday,day->tm_year+1900);
			return 0;
		}
		wprintf(L"\nERROR:No articles found.\n");
		return -1;
	}
	wstring searchBegin=L"<a href=\"";
	int back=searchURLResponse.rfind(searchBegin.c_str(),searchPos);
	if (back<0) 
	{
		wprintf(L"\nERROR:No articles found (2).\n");
		return -1;
	}
	back+=searchBegin.length();
	wstring URL=searchURLResponse.substr(back,searchPos-back);
	wstring lastURL,nextURL;
	lastURL=wstring(L"http://infoweb.newsbank.com")+link;
	int I=1;
	int error;
	while ((error=getArticle(hFile,lastURL,URL,last,nextURL,nb,I,totalBytes,numWords))==0 || error==-3)
	{
		if (error==-3) continue;
		__time64_t ltime;
		_time64( &ltime );
		char thetime[25];
		strncpy(thetime,_ctime64( &ltime )+4,15);
		thetime[16]=0;
		wprintf(L"%S:%04d:%02d/%02d/%d:%08I64d KB - %2.2f KB/s %d words\r",thetime,
			I++,day->tm_mon+1,day->tm_mday,day->tm_year+1900,totalBytes/1024,((float)totalBytes-originalBytes)/1024/(time(NULL)-startTime),numWords);
		int numAddSecs=(int) (((float)totalBytes-originalBytes)/1024/KB_THROTTLE-(time(NULL)-startTime));
		if (numAddSecs>=0) 
			Sleep(numAddSecs*1000);
		lastURL=URL;
		URL=nextURL;
		if (last) break;
	}
	InternetCloseHandle( hFile );
	return error;
}

int acquireNewsBank(char *nbdir,int startTimer)
{
	//The time is represented as seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC). 
	// we want everything after 1977.
	time_t timer=(10*365+2)*24*3600;
	timer=startTimer*24*3600;
	int numWords=0;
	__int64 totalBytes=0;
	FILE *yearFile=fopen("lastYear","r");
	if (yearFile)
	{
		fscanf(yearFile,"%I64d %d %I64d",&timer,&numWords,&totalBytes);
		fclose(yearFile);
	}
	originalBytes=totalBytes;
	time_t startTime;
	srand( (unsigned int)(startTime=time( NULL )));
	_mkdir(nbdir);
	while (true)
	{
		//time_t dayStartTime=(unsigned)time( NULL ) ;
		struct tm *day=gmtime(&timer);
		char nbDayDir[1024];
		sprintf(nbDayDir,"%s\\%d",nbdir,day->tm_year+1900);
		_mkdir(nbDayDir);
		char nbDayFile[1024];
		sprintf(nbDayFile,"%s\\%d\\%I64d.txt",nbdir,day->tm_year+1900,timer/(24*3600));
		FILE *nb=fopen(nbDayFile,"w");
		if (!nb) 
		{
			lplog(L"ERROR:%s:Cannot open path %s.",nbDayDir,nbDayFile);
			return 0;
		}
		lostDoc=noTitle=0;
		int error;
		if ((error=acquireNewsBankByDay(day,nb,startTime,totalBytes,numWords))<0 && (error==-1 || error==-3)) 
		{
			Sleep(2*60*1000); // Internet problems...
			continue;
		}
		fclose(nb);
		if (day->tm_year>2004) break;
		//wprintf(L" - Day %d (%d seconds)\n",timer/(24*3600),time(NULL)-dayStartTime);
		wprintf(L"\n");
		timer+=24*3600;
		yearFile=fopen("lastYear","w");
		if (yearFile)
		{
			fprintf(yearFile,"%I64d %d %I64d",timer,numWords,totalBytes);
			fclose(yearFile);
		}
	}
	return 0;
}

