#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include "fcntl.h"
#include "sys/stat.h"
#include <wn.h>
#include "profile.h"
#define MAX_BUF 10240000

// if just the synonyms and antonyms of the objects are accumulated, then if a 'a man' matches 'large man'
//   then all the derived synonyms will match also.  The correct way is to keep track of all the nouns and
//   adjectives and match only to those.  Then no matter how times 'man' matches, the it will only be
//   counted once.  On the other hand, if 'a big man' matches 'a large man' then this will yield a 
//   common number of 2, which will give it an advantage over any other object merely matching to 'man'.
//   If the primary nouns and adjectives are kept track of, then any match associated with them can be counted
//   accurately.
bool cSource::writeWNMap(map <tIWMM, vector <tIWMM>, cSourceWordInfo::cRMap::wordMapCompare >& sourceWordMap, void* buffer, int& where, int fd, int limit)
{
	LFS
		if (!copy(buffer, (int)sourceWordMap.size(), where, limit)) return false;
	for (map <tIWMM, vector <tIWMM>, cSourceWordInfo::cRMap::wordMapCompare >::iterator mi = sourceWordMap.begin(), miEnd = sourceWordMap.end(); mi != miEnd; mi++)
	{
		if (!copy(buffer, mi->first->first, where, limit)) return false;
		if (!copy(buffer, (int)mi->second.size(), where, limit)) return false;
		for (vector <tIWMM>::iterator wi = mi->second.begin(), wiEnd = mi->second.end(); wi != wiEnd; wi++)
		{
			if (!copy(buffer, (*wi)->first, where, limit)) return false;
			if (where > limit - 4096)
			{
				if (::write(fd, buffer, where) < 0)
					return false;
				where = 0;
			}
		}
	}
	return true;
}

int cSource::readWNMap(map <tIWMM, vector <tIWMM>, cSourceWordInfo::cRMap::wordMapCompare >& sourceWordMap, void* buffer, int& where, int bufferlen)
{
	LFS
		unsigned int count;
	if (!copy(count, buffer, where, bufferlen)) return -1;
	for (unsigned int I = 0; I < count && where < bufferlen; I++)
	{
		wstring word;
		if (!copy(word, buffer, where, bufferlen)) return -1;
		unsigned int wcount;
		if (!copy(wcount, buffer, where, bufferlen)) return -1;
		vector<tIWMM> wnv;
		tIWMM wi;
		for (unsigned int w = 0; w < wcount && where < bufferlen; w++)
		{
			wstring wword;
			if (!copy(wword, buffer, where, bufferlen)) return -1;
			if ((wi = Words.query(wword)) != Words.end())
				wnv.push_back(wi);
		}
		if ((wi = Words.query(word)) != Words.end())
		{
			map <tIWMM, vector <tIWMM>, cSourceWordInfo::cRMap::wordMapCompare >::iterator mi;
			if ((mi = sourceWordMap.find(wi)) == sourceWordMap.end())
				sourceWordMap[wi] = wnv;
			else
				mi->second.insert(mi->second.end(), wnv.begin(), wnv.end());
		}
	}
	return (where > bufferlen) ? -1 : 0;
}

bool cSource::writeGWNMap(map <tIWMM, int, cSourceWordInfo::cRMap::wordMapCompare >& wnMap, void* buffer, int& where, int fd, int limit)
{
	LFS
		if (!copy(buffer, (int)wnMap.size(), where, limit)) return false;
	for (map <tIWMM, int, cSourceWordInfo::cRMap::wordMapCompare >::iterator mi = wnMap.begin(), miEnd = wnMap.end(); mi != miEnd; mi++)
	{
		if (!copy(buffer, mi->first->first, where, limit)) return false;
		if (!copy(buffer, mi->second, where, limit)) return false;
		if (!flush(fd, buffer, where)) return false;
	}
	return true;
}

int cSource::readGWNMap(map <tIWMM, int, cSourceWordInfo::cRMap::wordMapCompare >& wnMap, void* buffer, int& where, int bufferlen)
{
	LFS
		unsigned int count;
	if (!copy(count, buffer, where, bufferlen)) return -1;
	for (unsigned int I = 0; I < count && where < bufferlen; I++)
	{
		wstring word;
		if (!copy(word, buffer, where, bufferlen)) return -1;
		tIWMM w = Words.query(word);
		unsigned int flags;
		if (!copy(flags, buffer, where, bufferlen)) return -1;
		if (w == Words.end()) continue;
		wnMap[w] = flags;
	}
	return (where > bufferlen) ? -1 : 0;
}

bool cSource::writeWNMaps(wstring path)
{
	LFS
		path += L".WNCache";
	int fd = _wopen(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0) return false;

	char buffer[MAX_BUF];
	int where = 0;
	writeWNMap(wnSynonymsNounMap, buffer, where, fd, MAX_BUF);
	writeWNMap(wnSynonymsAdjectiveMap, buffer, where, fd, MAX_BUF);
	writeWNMap(wnAntonymsNounMap, buffer, where, fd, MAX_BUF);
	writeWNMap(wnAntonymsAdjectiveMap, buffer, where, fd, MAX_BUF);
	writeGWNMap(wnGenderAdjectiveMap, buffer, where, fd, MAX_BUF);
	writeGWNMap(wnGenderNounMap, buffer, where, fd, MAX_BUF);
	for (tIWMM w = Words.begin(), wEnd = Words.end(); w != wEnd; w++)
	{
		if (w->second.flags & (cSourceWordInfo::physicalObjectByWN | cSourceWordInfo::notPhysicalObjectByWN))
		{
			if (!copy(buffer, w->first, where, MAX_BUF)) return false;
			if (!copy(buffer, w->second.flags, where, MAX_BUF)) return false;
			if (!flush(fd, buffer, where)) return false;
		}
	}
	wstring empty;
	if (!copy(buffer, empty, where, MAX_BUF))
		return false;
	if (::write(fd, buffer, where) < 0)
		return false;
	close(fd);
	return true;
}

void cSource::clearWNMaps()
{
	wnSynonymsNounMap.clear();
	wnSynonymsAdjectiveMap.clear();
	wnAntonymsNounMap.clear();
	wnAntonymsAdjectiveMap.clear();
	wnGenderAdjectiveMap.clear();
	wnGenderNounMap.clear();
}

bool cSource::readWNMaps(wstring path)
{
	LFS
		path += L".WNCache";
	IOHANDLE fd = _wopen(path.c_str(), O_RDWR | O_BINARY);
	if (fd < 0) return false;
	void* buffer;
	int bufferlen = filelength(fd);
	buffer = (void*)tmalloc(bufferlen + 10);
	::read(fd, buffer, bufferlen);
	close(fd);
	int where = 0;
	if (readWNMap(wnSynonymsNounMap, buffer, where, bufferlen) < 0) return false;
	if (readWNMap(wnSynonymsAdjectiveMap, buffer, where, bufferlen) < 0) return false;
	if (readWNMap(wnAntonymsNounMap, buffer, where, bufferlen) < 0) return false;
	if (readWNMap(wnAntonymsAdjectiveMap, buffer, where, bufferlen) < 0) return false;
	if (readGWNMap(wnGenderAdjectiveMap, buffer, where, bufferlen) < 0) return false;
	if (readGWNMap(wnGenderNounMap, buffer, where, bufferlen) < 0) return false;
	wstring word;
	int flags;
	while (where < bufferlen)
	{
		if (!copy(word, buffer, where, bufferlen)) return false;
		if (word.empty()) break;
		if (!copy(flags, buffer, where, bufferlen)) return false;
		tIWMM w = Words.query(word);
		if (w != Words.end())
			w->second.flags |= (flags & (cSourceWordInfo::physicalObjectByWN | cSourceWordInfo::notPhysicalObjectByWN));
	}
	tfree(bufferlen, buffer);
	return where <= bufferlen;
}

