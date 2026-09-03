/*
	getWordNetMaps.cpp - Binary cache I/O for per-source WordNet synonym/antonym/gender maps

	Overview:
		Serializes and restores cSource's WordNet-derived maps (noun/adjective synonyms
		and antonyms, gender flags, and physicalObjectByWN bits) to a sibling
		"<path>.WNCache" file so later parses of the same source skip WordNet walks.

	Pipeline position:
		Called after WordNet maps have been built for a source, and again on reload
		before objects/pronouns use those maps for matching.

	Key entry points:
		- writeWNMap / readWNMap - word -> vector<word> maps
		- writeGWNMap / readGWNMap - word -> int flag maps
		- writeWNMaps / readWNMaps - full cache file (four WN maps + two gender maps +
		  physical-object flags, terminated by an empty word)
		- clearWNMaps - empties the six in-memory maps

	Dependencies:
		Words lexicon (query by string); tmalloc/tfree; POSIX-style _wopen/write.

	Notes / gotchas:
		writeWNMaps heap-allocates its 10MB scratch buffer (tmalloc/tfree) rather than
		putting it on the stack, and closes fd / frees the buffer on every return path.
		readWNMaps tfrees its tmalloc'd file buffer on every failure path too. Words not
		already in the lexicon are dropped on read, so the cache is only useful after the
		word table is loaded.
*/
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
// Appends sourceWordMap (count, then each key string + vector of related words) into buffer,
// flushing to fd when within 4096 of limit. Returns false if copy/write fails.
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

// Reads one word->vector<word> map from buffer at 'where'. Words missing from Words are
// skipped. Returns -1 on copy failure or if where walked past bufferlen; 0 on success.
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

// Appends a word->int map (count, then each key + flags) and flush()es after every entry.
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

// Reads one word->int map. Unknown words are skipped; last write wins if a word repeats.
// Returns -1 on copy failure or overrun; 0 on success.
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

// Writes all six WN maps plus physicalObjectByWN flags to path+".WNCache".
// Returns false if the file cannot be created or any write fails; fd is closed and the
// scratch buffer freed on every path.
bool cSource::writeWNMaps(wstring path)
{
	LFS
		path += L".WNCache";
	int fd = _wopen(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0) return false;

	char* buffer = (char*)tmalloc(MAX_BUF);
	if (!buffer) { close(fd); return false; }
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
			if (!copy(buffer, w->first, where, MAX_BUF)) { tfree(MAX_BUF, buffer); close(fd); return false; }
			if (!copy(buffer, w->second.flags, where, MAX_BUF)) { tfree(MAX_BUF, buffer); close(fd); return false; }
			if (!flush(fd, buffer, where)) { tfree(MAX_BUF, buffer); close(fd); return false; }
		}
	}
	wstring empty;
	if (!copy(buffer, empty, where, MAX_BUF))
	{
		tfree(MAX_BUF, buffer);
		close(fd);
		return false;
	}
	if (::write(fd, buffer, where) < 0)
	{
		tfree(MAX_BUF, buffer);
		close(fd);
		return false;
	}
	tfree(MAX_BUF, buffer);
	close(fd);
	return true;
}

// Drops synonym/antonym/gender maps for this source (physical-object flags on Words stay).
void cSource::clearWNMaps()
{
	wnSynonymsNounMap.clear();
	wnSynonymsAdjectiveMap.clear();
	wnAntonymsNounMap.clear();
	wnAntonymsAdjectiveMap.clear();
	wnGenderAdjectiveMap.clear();
	wnGenderNounMap.clear();
}

// Loads path+".WNCache" into the six maps and ORs physical-object flags onto Words.
// Returns false if the file is missing or any section is corrupt; the tmalloc'd file
// buffer is tfree'd on every path.
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
	if (readWNMap(wnSynonymsNounMap, buffer, where, bufferlen) < 0) { tfree(bufferlen + 10, buffer); return false; }
	if (readWNMap(wnSynonymsAdjectiveMap, buffer, where, bufferlen) < 0) { tfree(bufferlen + 10, buffer); return false; }
	if (readWNMap(wnAntonymsNounMap, buffer, where, bufferlen) < 0) { tfree(bufferlen + 10, buffer); return false; }
	if (readWNMap(wnAntonymsAdjectiveMap, buffer, where, bufferlen) < 0) { tfree(bufferlen + 10, buffer); return false; }
	if (readGWNMap(wnGenderAdjectiveMap, buffer, where, bufferlen) < 0) { tfree(bufferlen + 10, buffer); return false; }
	if (readGWNMap(wnGenderNounMap, buffer, where, bufferlen) < 0) { tfree(bufferlen + 10, buffer); return false; }
	wstring word;
	int flags;
	while (where < bufferlen)
	{
		if (!copy(word, buffer, where, bufferlen)) { tfree(bufferlen + 10, buffer); return false; }
		if (word.empty()) break;
		if (!copy(flags, buffer, where, bufferlen)) { tfree(bufferlen + 10, buffer); return false; }
		tIWMM w = Words.query(word);
		if (w != Words.end())
			w->second.flags |= (flags & (cSourceWordInfo::physicalObjectByWN | cSourceWordInfo::notPhysicalObjectByWN));
	}
	bool ok = (where <= bufferlen);
	tfree(bufferlen + 10, buffer);
	return ok;
}

