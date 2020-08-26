#include <windows.h>
#define _WINSOCKAPI_ /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include <fcntl.h>
#include "mysql.h"
#include <direct.h>
#include <sys/stat.h>
#include <crtdbg.h>
#include "profile.h"
#include <sstream>

void itos(wchar_t *before,int i,wstring &concat,wchar_t *after)
{ LFS
	wchar_t temp[1024];
	wcscpy(temp,before);
	_itow(i,temp+wcslen(temp),10);
	wcscat(temp,after);
	concat+=temp;
}

void itos(wchar_t *before,int i,wstring &concat,wstring after)
{ LFS
	wchar_t temp[1024];
	wcscpy(temp,before);
	_itow(i,temp+wcslen(temp),10);
	concat+=temp+after;
}

wstring itos(int i, wstring &tmp)
{
	LFS
		wchar_t temp[1024];
	_itow(i, temp, 10);
	return tmp = temp;
}

wstring itos(int i, wchar_t *format, wstring &tmp)
{
	LFS
	wchar_t temp[1024];
	wsprintf(temp,format,i);
	return tmp = temp;
}

wstring dtos(double fl,wstring &tmp)
{ LFS
	wchar_t ctmp[1024];
	swprintf(ctmp,1024,L"%4.2g",fl);
	return tmp=ctmp;
}

__declspec(thread) static void *wTMbuffer=NULL; 
__declspec(thread) static unsigned int wTMbufSize=0; 
char *wTM(wstring inString,string &outString,int codePage)
{ LFS
	int queryLength=WideCharToMultiByte( codePage, 0, inString.c_str(), -1, (LPSTR)wTMbuffer, wTMbufSize, NULL, NULL );
	if (wTMbufSize==0)
	{
		wTMbufSize=max(queryLength*2,10000);
		wTMbuffer=tmalloc(wTMbufSize);
		if (!WideCharToMultiByte( codePage, 0, inString.c_str(), -1, (LPSTR)wTMbuffer, wTMbufSize, NULL, NULL ))
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",inString.c_str());
	}
	if (!queryLength)
	{
		if (GetLastError()!=ERROR_INSUFFICIENT_BUFFER) 
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",inString.c_str());
		queryLength=WideCharToMultiByte( codePage, 0, inString.c_str(), -1, NULL, 0, NULL, NULL );
		unsigned int previousBufSize=wTMbufSize;
		wTMbufSize=queryLength*2;
		if (!(wTMbuffer=trealloc(23,wTMbuffer,previousBufSize,wTMbufSize)))
			lplog(LOG_FATAL_ERROR,L"Out of memory requesting %d bytes from sql query %s!",wTMbufSize,inString.c_str());
		if (!WideCharToMultiByte( codePage, 0, inString.c_str(), -1, (LPSTR)wTMbuffer, wTMbufSize, NULL, NULL ))
			lplog(LOG_FATAL_ERROR,L"Error in translating sql request: %s",inString.c_str());
	}
	outString=(char *)wTMbuffer;
	return (char *)outString.c_str();
}

__declspec(thread) static void *mTWbuffer = NULL;
__declspec(thread) static unsigned int mTWbufSize = 0;
const wchar_t *mTW(string inString, wstring &outString, int &codepage,bool &iso8859ControlCharactersFound)
{
	LFS
	codepage = CP_UTF8;
	int queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, 0);
	if (!queryLength && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
	{
		codepage = 28591; // iso-8859-1	ISO 8859-1 Latin 1; Western European (ISO)
		queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, 0);
		if (!queryLength && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
		{
			codepage = 1252; // ANSI Latin 1; Western European (Windows)
			queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, 0);
			if (!queryLength && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
			{
				codepage = 20127; // ASCII: ISO-646-US (US-ASCII), ASCII, US-ASCII  // US-ASCII (7-bit)
				queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, 0);
			}
		}
		else
		{
			// scan text for control characters 128-159.  If they exist in the text, force to 1252, because in 8859 they are legal but they are control characters.
			int tempIndex = 0;
			for (char ch : inString)
			{
				if (iso8859ControlCharactersFound = (((unsigned char)ch) >= 128 && ((unsigned char)ch) <= 159))
					break;
				tempIndex++;
			}
			if (iso8859ControlCharactersFound)
			{
				int tempQueryLength = MultiByteToWideChar(1252, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, 0);
				if (tempQueryLength > 0)
				{
					codepage = 1252;
					queryLength = tempQueryLength;
				}
			}
		}
	}
	if (!queryLength)
		lplog(LOG_FATAL_ERROR, L"Error (2) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
	unsigned int desiredBufferSizeInBytes = (queryLength + 1) << 1;
	if (mTWbufSize < desiredBufferSizeInBytes)
	{
		desiredBufferSizeInBytes = max(desiredBufferSizeInBytes, 1000000); // make minimum buffer 1MB to avoid repeatedly reallocating for trivially small sizes
		unsigned int previousBufSize = mTWbufSize;
		mTWbufSize = max(desiredBufferSizeInBytes, mTWbufSize);
		mTWbuffer = (previousBufSize == 0) ? tmalloc(mTWbufSize) : trealloc(24, mTWbuffer, previousBufSize, mTWbufSize);
		if (!mTWbuffer)
			lplog(LOG_FATAL_ERROR, L"Out of memory requesting %d bytes from translating buffer %S!", mTWbufSize, inString.c_str());
	}
	if (!(queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, mTWbufSize / 2)))
		lplog(LOG_FATAL_ERROR, L"Error (3) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
	outString = (wchar_t *)mTWbuffer;
	return outString.c_str();
}

const wchar_t *mTW(string inString, wstring &outString, int &codepage)
{
	bool iso8859ControlCharactersFound;
	return mTW(inString, outString, codepage, iso8859ControlCharactersFound);
}

const wchar_t *mTWCodePage(string inString, wstring &outString, int codepage,int &error)
{
	LFS
	int queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, 0);
	if (!queryLength)
	{
		lplog(LOG_ERROR, L"Error (mTWCodePage) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
		error = -1;
		return 0;
	}
	unsigned int desiredBufferSizeInBytes = (queryLength + 1) << 1;
	if (mTWbufSize < desiredBufferSizeInBytes)
	{
		desiredBufferSizeInBytes = max(desiredBufferSizeInBytes, 1000000); // make minimum buffer 1MB to avoid repeatedly reallocating for trivially small sizes
		unsigned int previousBufSize = mTWbufSize;
		mTWbufSize = max(desiredBufferSizeInBytes, mTWbufSize);
		mTWbuffer = (previousBufSize == 0) ? tmalloc(mTWbufSize) : trealloc(24, mTWbuffer, previousBufSize, mTWbufSize);
		if (!mTWbuffer)
		{
			lplog(LOG_ERROR, L"Out of memory requesting %d bytes from translating buffer %S!", mTWbufSize, inString.c_str());
			error = -2;
			return 0;
		}
	}
	if (!(queryLength = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, inString.c_str(), -1, (wchar_t *)mTWbuffer, mTWbufSize / 2)))
	{
		lplog(LOG_ERROR, L"Error (mTWCodePage 2) (%d) in translating buffer: %S", GetLastError(), inString.c_str());
		error = -3;
		return 0;
	}
	outString = (wchar_t *)mTWbuffer;
	return outString.c_str();
}

const wchar_t *mTW(string inString, wstring &outString)
{
	int codepage;
	return mTW(inString, outString, codepage);
}

bool copy(void *buf,wstring str,int &where,int limit)
{ DLFS
	if (where + (str.length() + 1) * sizeof(str[0]) > (unsigned)limit)
	{
		lplog(LOG_ERROR, L"Maximum copy limit of %d bytes reached (5)!", limit);
		return false;
	}
	wcscpy((wchar_t *)(((char *)buf)+where),str.c_str());
	((char *)buf)[where+wcslen(str.c_str())*sizeof(str[0])]=0;
	where+=(wcslen(str.c_str())+1)*sizeof(str[0]);
	return true;
}

bool copy(void *buf,string str,int &where,int limit)
{ DLFS
	if (where+(str.length()+1)*sizeof(str[0])>(unsigned)limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (6)",limit);
	strcpy((((char *)buf)+where),str.c_str());
	((char *)buf)[where+str.length()*sizeof(str[0])]=0;
	where+=(str.length()+1)*sizeof(str[0]);
	return true;
}

bool copy(void *buf,int num,int &where,int limit)
{ DLFS
	*((int *)(((char *)buf)+where))=num;
	where+=sizeof(num);
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (7)",limit);
	return true;
}

bool copy(void *buf,short num,int &where,int limit)
{ DLFS
	*((short *)(((char *)buf)+where))=num;
	where+=sizeof(num);
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (8)",limit);
	return true;
}

bool copy(void *buf,unsigned short num,int &where,int limit)
{ DLFS
	*((unsigned short *)(((char *)buf)+where))=num;
	where+=sizeof(num);
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (9)",limit);
	return true;
}

bool copy(void *buf,unsigned int num,int &where,int limit)
{ DLFS
	*((unsigned int *)(((char *)buf)+where))=num;
	where+=sizeof(num);
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (10)",limit);
	return true;
}

bool copy(void *buf,__int64 num,int &where,int limit)
{ DLFS
	*((__int64 *)(((char *)buf)+where))=num;
	where+=sizeof(num);
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (11)",limit);
	return true;
}

bool copy(void *buf,unsigned __int64 num,int &where,int limit)
{ DLFS
	*((unsigned __int64 *)(((char *)buf)+where))=num;
	where+=sizeof(num);
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (12)",limit);
	return true;
}

bool copy(void *buf,char ch,int &where,int limit)
{ DLFS
	((char *)buf)[where++]=ch;
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (13)",limit);
	return true;
}

bool copy(void *buf,unsigned char ch,int &where,int limit)
{ DLFS
	((char *)buf)[where++]=ch;
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (13)",limit);
	return true;
}

bool copy(void *buf,set <int> &s,int &where,int limit)
{ DLFS
	int count=s.size();
	if (!copy(buf,count,where,limit)) return false;
	for (set<int>::iterator is=s.begin(),isEnd=s.end(); is!=isEnd; is++)
		if (!copy(buf,*is,where,limit)) return false;
	return true;
}

bool copy(void *buf,vector <int> &s,int &where,int limit)
{ DLFS
	int count=s.size();
	if (!copy(buf,count,where,limit)) return false;
	for (vector<int>::iterator is=s.begin(),isEnd=s.end(); is!=isEnd; is++)
		if (!copy(buf,*is,where,limit)) return false;
	return true;
}

bool copy(void *buf,vector <wstring> &s,int &where,int limit)
{ DLFS
	int count=s.size();
	if (!copy(buf,count,where,limit)) return false;
	for (vector<wstring>::iterator is=s.begin(),isEnd=s.end(); is!=isEnd; is++)
		if (!copy(buf,*is,where,limit)) return false;
	return true;
}

bool copy(void *buf,vector <string> &s,int &where,int limit)
{ DLFS
	int count=s.size();
	if (!copy(buf,count,where,limit)) return false;
	for (vector<string>::iterator is=s.begin(),isEnd=s.end(); is!=isEnd; is++)
		if (!copy(buf,*is,where,limit)) return false;
	return true;
}

bool copy(void *buf,set <string> &s,int &where,int limit)
{ DLFS
	int count=s.size();
	if (!copy(buf,count,where,limit)) return false;
	for (set<string>::iterator is=s.begin(),isEnd=s.end(); is!=isEnd; is++)
		if (!copy(buf,*is,where,limit)) return false;
	return true;
}

bool copy(void *buf, unordered_set <string> &s, int &where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (auto is:s)
		if (!copy(buf, is, where, limit)) return false;
	return true;
}

bool copy(void *buf,set <wstring> &s,int &where,int limit)
{ DLFS
	int count=s.size();
	if (!copy(buf,count,where,limit)) return false;
	for (set<wstring>::iterator is=s.begin(),isEnd=s.end(); is!=isEnd; is++)
		if (!copy(buf,*is,where,limit)) return false;
	return true;
}

bool copy(void *buf, unordered_set <wstring> &s, int &where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (auto is :s)
		if (!copy(buf, is, where, limit)) return false;
	return true;
}

bool copy(void *buf,cIntArray &a,int &where,int limit)
{ DLFS
	if (where+sizeof(a.size())+a.size()*sizeof(int)>(unsigned)limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (16)",limit);
	*((int *)(((char *)buf)+where))=a.size();
	where+=sizeof(a.size());
	memcpy(((char *)buf)+where,a.begin(),a.size()*sizeof(int));
	where+=a.size()*sizeof(int);
	return true;
}

bool copy(void *buf, cLastVerbTenses &a, int &where, int limit)
{
	DLFS
	return copy(buf, a.lastTense, where, limit) && copy(buf, a.lastVerb, where, limit);
}

// copy only the first element
bool copyWString(void *buf,tIWMM w,int &where,int limit)
{ DLFS
	return copy(buf,(w==wNULL) ? L"":w->first,where,limit);
}

bool copy(void *buf,cName &a,int &where,int limit)
{ DLFS
	//int begin=where;
	if (!copy(buf,a.nickName,where,limit)) return false;
	if (!copyWString(buf,a.hon,where,limit)) return false;
	if (!copyWString(buf,a.hon2,where,limit)) return false;
	if (!copyWString(buf,a.hon3,where,limit)) return false;
	if (!copyWString(buf,a.first,where,limit)) return false;
	if (!copyWString(buf,a.middle,where,limit)) return false;
	if (!copyWString(buf,a.middle2,where,limit)) return false;
	if (!copyWString(buf,a.last,where,limit)) return false;
	if (!copyWString(buf,a.suffix,where,limit)) return false;
	if (!copyWString(buf,a.any,where,limit)) return false;
	return true;
}

bool copy(string &str,void *buf,int &where,int limit)
{ DLFS
	str=(((char *)buf)+where);
	if (where+(str.length()+1)*sizeof(str[0])>(unsigned)limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (14)",limit);
	where+=(str.length()+1)*sizeof(str[0]);
	return true;
}

bool copy(wstring &str,void *buf,int &where,int limit)
{ DLFS
	str=(wchar_t *)((char *)buf+where);
	if (where+(str.length()+1)*sizeof(str[0])>(unsigned)limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached! (15)",limit);
	where+=(str.length()+1)*sizeof(str[0]);
	return true;
}

bool copy(const wchar_t *wcstr,void *buf,int &where,int limit)
{ DLFS
	wstring wstr=wcstr;
	return copy(wstr,buf,where,limit);
}

bool copy(int &num,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(num)>limit) return false;
	num=*((int *)(((char *)buf)+where));
	where+=sizeof(num);
	return true;
}

bool copy(unsigned int &num,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(num)>limit) return false;
	num=*((unsigned int *)(((char *)buf)+where));
	where+=sizeof(num);
	return true;
}

bool copy(__int64 &num,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(num)>limit) return false;
	num=*((__int64 *)(((char *)buf)+where));
	where+=sizeof(num);
	return true;
}

bool copy(unsigned __int64 &num,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(num)>limit) return false;
	num=*((unsigned __int64 *)(((char *)buf)+where));
	where+=sizeof(num);
	return true;
}

bool copy(short &num,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(num)>limit) return false;
	num=*((short *)(((char *)buf)+where));
	where+=sizeof(num);
	return true;
}

bool copy(unsigned short &num,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(num)>limit) return false;
	num=*((unsigned short *)(((char *)buf)+where));
	where+=sizeof(num);
	return true;
}

bool copy(char &ch,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(ch)>limit) return false;
	ch=((char *)buf)[where++];
	return true;
}

bool copy(unsigned char &ch,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(ch)>limit) return false;
	ch=((char *)buf)[where++];
	return true;
}

bool copy(set <int> &s,void *buf,int &where,int limit)
{ DLFS
	int count;
	if (!copy(count,buf,where,limit)) return false;
	if (count<0)
	{
		lplog(LOG_ERROR,L"negative count on read!");
		return false;
	}
	for (int I=0; I<count; I++)
	{
		int i;
		if (!copy(i,buf,where,limit)) return false;
		s.insert(i);
	}
	return true;
}

bool copy(vector <int> &s,void *buf,int &where,int limit)
{ DLFS
	int count;
	if (!copy(count,buf,where,limit)) return false;
	if (count<0)
	{
		lplog(LOG_ERROR,L"negative count on read!");
		return false;
	}
	s.reserve(count);
	for (int I=0; I<count; I++)
	{
		int i;
		if (!copy(i,buf,where,limit)) return false;
		s.push_back(i);
	}
	return true;
}

bool copy(vector <wstring> &s,void *buf,int &where,int limit)
{ DLFS
	int count;
	if (!copy(count,buf,where,limit)) return false;
	if (count<0 || count>(limit-where)/2)
	{
		lplog(LOG_FATAL_ERROR,L"illegal count on read - %d!",count);
		return false;
	}
	s.reserve(count);
	for (int I=0; I<count; I++)
	{
		wstring si;
		if (!copy(si,buf,where,limit)) return false;
		s.push_back(si);
	}
	return true;
}

bool copy(vector <string> &s,void *buf,int &where,int limit)
{ DLFS
	int count;
	if (!copy(count,buf,where,limit)) return false;
	if (count<0)
	{
		lplog(LOG_ERROR,L"negative count on read!");
		return false;
	}
	s.reserve(count);
	for (int I=0; I<count; I++)
	{
		string si;
		if (!copy(si,buf,where,limit)) return false;
		s.push_back(si);
	}
	return true;
}

bool copy(set <string> &s,void *buf,int &where,int limit)
{ DLFS
	int count;
	if (!copy(count,buf,where,limit)) return false;
	if (count<0)
	{
		lplog(LOG_ERROR,L"negative count on read!");
		return false;
	}
	for (int I=0; I<count; I++)
	{
		string str;
		if (!copy(str,buf,where,limit)) return false;
		s.insert(str);
	}
	return true;
}

bool copy(set <wstring> &s, void *buf, int &where, int limit)
{
	DLFS
		int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		wstring str;
		if (!copy(str, buf, where, limit)) return false;
		s.insert(str);
	}
	return true;
}

bool copy(unordered_set <wstring> &s, void *buf, int &where, int limit)
{
	DLFS
	int count;
	if (!copy(count, buf, where, limit)) return false;
	if (count < 0)
	{
		lplog(LOG_ERROR, L"negative count on read!");
		return false;
	}
	for (int I = 0; I < count; I++)
	{
		wstring str;
		if (!copy(str, buf, where, limit)) return false;
		s.insert(str);
	}
	return true;
}

bool copy(cIntArray &a,void *buf,int &where,int limit)
{ DLFS
	if (where+(int)sizeof(int)>limit) return false;
	int num=*((int *)(((char *)buf)+where));
	where+=sizeof(num);
	if ((where+num*(int)sizeof(int))>limit) return false;
	if (num<0)
	{
		lplog(LOG_ERROR,L"negative count on read!");
		return false;
	}
	for (int I=0; I<num; I++,where+=sizeof(int))
		a.push_back(*((int *)(((char *)buf)+where)));
	return true;
}

bool copy(cLastVerbTenses &a,void *buf,int &where,int limit)
{ DLFS
	if (!copy(a.lastTense,buf,where,limit)) return false;
	if (!copy(a.lastVerb,buf,where,limit)) return false;
	return true;
}

bool copyWString(tIWMM &w,void *buf,int &where,int limit)
{ DLFS
	wstring str;
	if (!copy(str,buf,where,limit)) return false;
	if (str.empty())
		w=wNULL;
	else
	{
		w=Words.query(str);
		if (w==Words.end())
		{
			while (str.length()>0 && str[str.length()-1]==L' ')
				str.erase(str.begin()+str.length()-1);
			w=Words.query(str);
			if (w==Words.end())
			{
				::lplog(LOG_ERROR,L"word %s not found in creating read in object name.",str.c_str());
				w=wNULL;
			}
		}
	}
	return true;
}

bool copy(cName &a,void *buf,int &where,int limit)
{ DLFS
	//int begin=where;
	if (!copy(a.nickName,buf,where,limit)) return false;
	if (!copyWString(a.hon,buf,where,limit)) return false;
	if (!copyWString(a.hon2,buf,where,limit)) return false;
	if (!copyWString(a.hon3,buf,where,limit)) return false;
	if (!copyWString(a.first,buf,where,limit)) return false;
	if (!copyWString(a.middle,buf,where,limit)) return false;
	if (!copyWString(a.middle2,buf,where,limit)) return false;
	if (!copyWString(a.last,buf,where,limit)) return false;
	if (!copyWString(a.suffix,buf,where,limit)) return false;
	if (!copyWString(a.any,buf,where,limit)) return false;
	return true;
}

// Function to lookup an error message from an error code.
char * LastErrorStr(void)
{ LFS
	char *      szReturn = NULL;
	int hr=GetLastError();
	if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,hr,GetUserDefaultLangID(),(CHAR *)&szReturn,0,NULL) == 0)
		return "Unknown";

	return szReturn;
}

void checkHeap(wchar_t *desc)
{ LFS
	// Check heap status
	int heapstatus = _heapchk();
	switch( heapstatus )
	{
	case _HEAPOK:
		break;
	case _HEAPEMPTY:
		printf(" OK - %ls heap is empty\n",desc );
		break;
	case _HEAPBADBEGIN:
		printf( "ERROR - %ls bad start of heap\n",desc );
		break;
	case _HEAPBADNODE:
		printf( "ERROR - %ls bad node in heap\n",desc );
		break;
	}
	if (!_CrtCheckMemory())
		printf("ERROR - %ls memory is bad", desc);
}

void escapeSingleQuote(wstring &lobject)
{
	LFS
		wstring slo2;
	for (unsigned int I = 0; I < lobject.size(); I++)
		if (lobject[I] == L'\'')
			slo2 += L"\\'";
		else
			slo2 += lobject[I];
	lobject = slo2;
}

void removeSingleQuote(wstring &lobject)
{
	LFS
		wstring slo2;
	for (unsigned int I = 0; I < lobject.size() - 1; I++)
		if (lobject[I] != L'\\' || lobject[I + 1] != L'\'')
			slo2 += lobject[I];
		else
			I++;
	if (lobject[lobject.size() - 1] != L'\'')
		slo2 += lobject[lobject.size() - 1];
	lobject = slo2;
}

void removeExcessSpaces(wstring &lobject)
{
	LFS
		wstring slo2;
	for (unsigned int I = 0; I < lobject.size(); I++)
		if (lobject[I] != L' ' || I == 0 || lobject[I - 1] != ' ')
			slo2 += lobject[I];
	lobject = slo2;
}

void trim(wstring &str)
{
	LFS
		int whereSpace = 0;
	while (str[whereSpace] == L' ')
		whereSpace++;
	if (whereSpace > 0)
		str.erase(0, whereSpace);
	whereSpace = str.length() - 1;
	while (whereSpace >= 0 && str[whereSpace] == L' ')
		whereSpace--;
	if (whereSpace < str.length() - 1)
		str.erase(whereSpace + 1);
}

vector<wstring> splitString(wstring str, wchar_t wc)
{
	vector<wstring> strings;
	std::wistringstream f(str);
	wstring s;
	while (std::getline(f, s, wc))
		strings.push_back(s);
	return strings;
}


int addTagMark(wstring tag, vector<cTagLocation> tagSet, map <int, set<wstring>> &tagBeginPositionMap, map <int, set<wstring>> &tagEndPositionMap)
{
	int nextTagIndex = -1, tagIndex = findTag(tagSet, (wchar_t *)tag.c_str(), nextTagIndex);
	if (tagIndex >= 0)
	{
		tagBeginPositionMap[tagSet[tagIndex].sourcePosition].insert(tag);
		tagEndPositionMap[tagSet[tagIndex].sourcePosition + tagSet[tagIndex].len - 1].insert(tag);
	}
	return tagIndex;
}

void addTagConstrainedMark(wstring tag, int constrainByTag, vector<cTagLocation> tagSet, map <int, set<wstring>> &tagBeginPositionMap, map <int, set<wstring>> &tagEndPositionMap)
{
	int nextConstrainedTagIndex = -1, constrainedTagIndex = (constrainByTag >= 0) ? findTagConstrained(tagSet, (wchar_t *)tag.c_str(), nextConstrainedTagIndex, tagSet[constrainByTag]) : -1;
	if (constrainedTagIndex >= 0)
	{
		tagBeginPositionMap[tagSet[constrainedTagIndex].sourcePosition].insert(tag);
		tagEndPositionMap[tagSet[constrainedTagIndex].sourcePosition + tagSet[constrainedTagIndex].len - 1].insert(tag);
	}
}

void getTagPositionsFromTagSet(vector<cTagLocation> tagSet, map <int, set<wstring>> &tagBeginPositionMap, map <int, set<wstring>> &tagEndPositionMap)
{
	if (addTagMark(L"SUBJECT", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	if (addTagMark(L"VERB", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	if (addTagMark(L"OBJECT", tagSet, tagBeginPositionMap, tagEndPositionMap) < 0)
		return;
	int nextMainVerbTag = -1, mainVerbTag = findTag(tagSet, L"VERB", nextMainVerbTag);
	addTagConstrainedMark(L"V_AGREE", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(L"conditional", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(L"past", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
	addTagConstrainedMark(L"future", mainVerbTag, tagSet, tagBeginPositionMap, tagEndPositionMap);
}

void getTagPositions(cSource &source, int position, int pemaByPatternEnd, map <int, set<wstring>> &tagBeginPositionMap, map <int, set<wstring>> &tagEndPositionMap)
{
	vector < vector <cTagLocation> > tagSets;
	if (source.startCollectTags(false, subjectVerbRelationTagSet, position, pemaByPatternEnd, tagSets, true, false, L"tags for debugging") > 0)
	{
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (tagSets[J].size())
				getTagPositionsFromTagSet(tagSets[J], tagBeginPositionMap, tagEndPositionMap);
		}
	}
}

void getSentenceWithTags(cSource &source, int patternBegin, int patternEnd, int sentenceBegin, int sentenceEnd, int PEMAPosition, wstring &sentence)
{
	map <int, set<wstring>> tagBeginPositionMap, tagEndPositionMap;
	getTagPositions(source, patternBegin, PEMAPosition, tagBeginPositionMap, tagEndPositionMap);
	wstring originalIWord;
	bool inPattern = false;
	for (int I = sentenceBegin; I < sentenceEnd; I++)
	{
		source.getOriginalWord(I, originalIWord, false, false);
		if (I == patternBegin)
		{
			sentence += L"**";
			inPattern = true;
		}
		if (tagBeginPositionMap.find(I) != tagBeginPositionMap.end())
		{
			for (wstring tag : tagBeginPositionMap[I])
				sentence += tag + L"{";
		}
		sentence += originalIWord;
		if (tagEndPositionMap.find(I) != tagEndPositionMap.end())
		{
			bool notFoundInBeginPositions = tagBeginPositionMap.find(I) == tagBeginPositionMap.end();
			for (wstring tag : tagEndPositionMap[I])
			{
				if (notFoundInBeginPositions || tagBeginPositionMap[I].find(tag) == tagBeginPositionMap[I].end())
					sentence += L" " + tag;
				sentence += L"}";
			}
		}
		if (I == patternEnd - 1)
		{
			sentence += L"**";
			inPattern = false;
		}
		sentence += L" ";
	}
}

