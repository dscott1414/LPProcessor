/*
	names.h - cName / cNickName and the name-pattern / nickname-map entry points

	Overview:
		Declares the proper-name value type used by every cObject of class
		NAME_OBJECT_CLASS: honorifics HON/HON2/HON3, first / middle /
		middle2 / last / suffix / any, and a nickname class id. Also
		declares the pattern builders (defineNames,
		createMetaNameEquivalencePatterns, createLetterIntroPatterns) and
		the process-wide nicknameEquivalenceMap (US Census + lexicon,
		populated in initializeDictionary.cpp).

	Pipeline position:
		Included from source.h. cName is embedded in cObject; matching
		happens in names.cpp (like / confidentMatch / resolveNameObject)
		during stage 6.

	Key data structures / globals:
		- cNickName::equivalences - one nickname cluster (rarely used
		  directly; the map stores an int class id instead)
		- nicknameEquivalenceMap - wstring first-name -> class id
		- cName parts are tIWMM iterators into Words; wNULL is unset.
		  nickName == -1 means “no nickname class”

	Notes / gotchas:
		- operator== / like() do not compare nickName except on the
		  first-name mismatch path. Sex and plurality live on cObject,
		  not cName.
		- notNull() is implemented in names.cpp as “all parts are null”.
		- isNull / isCompletelyNull / matchHonorifics are defined in
		  resolveObjects.cpp, not here.
		- createLetterIntroPatterns() is defined in resolveSpeakers.cpp.
*/
#pragma once
void defineNames(void);
void createMetaNameEquivalencePatterns(void);
void createLetterIntroPatterns(void);

class cNickName
{
public:
	vector <wstring> equivalences;
	// Empty equivalence list; filled by the census nickname loader.
	cNickName() 
	{ 
	}
	// True if `name` is in equivalences (linear scan). Takes the string by value.
	bool operator == (wstring name)  
	{
		unsigned int I;
		for (I=0; I<equivalences.size() && name!=equivalences[I]; I++);
		return I<equivalences.size(); 
	}
	// Inverse of ==, but also lplog’s every comparison (expensive; not a
	// pure inverse in side effects).
	bool operator != (wstring name)  
	{
		unsigned int I;
		for (I=0; I<equivalences.size() && name!=equivalences[I]; I++)
			lplog(L"comparing %s against %s.",name.c_str(),equivalences[I].c_str());
		return I==equivalences.size(); 
	}
};

extern unordered_map <wstring, int> nicknameEquivalenceMap;

class cName
{
public:
	enum nameType { HON,HON2,HON3,FIRST,MIDDLE,MIDDLE2,LAST,SUFFIX,ANY };
	int nickName;
	tIWMM hon,hon2,hon3;
	tIWMM first;
	tIWMM middle,middle2;
	tIWMM last;
	tIWMM suffix;
	tIWMM any;
	// All parts wNULL, nickName -1 (no census class).
	cName(void) { hon=hon2=hon3=first=middle=middle2=last=suffix=any=wNULL; nickName=-1; }
	void operator = (const cName& n);
	bool operator == (const cName& n);
	bool justHonorific(void);
	bool getNickName(tIWMM firstName);
	void hn(const wchar_t * namePartName,tIWMM namePart,wstring &accumulate,bool printShort, const wchar_t * separator);
	void hn(const wchar_t * namePartName,tIWMM namePart, wchar_t * accumulate,bool printShort, const wchar_t * separator);
	bool hn(tIWMM namePart,wchar_t separationCharacter,wstring &accumulate);
	wstring print(wstring &message,bool printShort, const wchar_t * separator);
	bool notNull();
	wstring print(wchar_t *message,bool printShort, const wchar_t * separator);
	wstring original(wstring &message,wchar_t separationCharacter,bool justFirstAndLast);
	bool match(tIWMM sub1,tIWMM sub2,bool returnTrueOnNull=true);
	void merge(tIWMM &w1,tIWMM w2);
	bool in(tIWMM hon,vector <tIWMM> &hons);
	// merge first, middle, middle2 or last if we only have a letter and n has more.
	void merge(cName &n, sTrace &t);
	bool like(cName &n,sTrace &t); // names are compatible - could be the same
	bool confidentMatch(cName &n,bool sexConfidentMatch,sTrace &t); // names match in multiple ways, almost certainly the same
	void insertSubSQL(wchar_t *buffer,int sourceId,int index,int maxbuf,tIWMM hp,int &buflen,enum cName::nameType ht);
	int insertSQL(wchar_t *buffer,int sourceId,int index,int maxbuf);
	bool neuterName(bool startsWithDeterminer,bool ownedByName,int len);
	bool matchHonorifics(wstring sHon);
	bool isNull();
	bool isCompletelyNull();
};

