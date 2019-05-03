void defineNames(void);
void createMetaNameEquivalencePatterns(void);
void createLetterIntroPatterns(void);

class cNickName
{
public:
	vector <wstring> equivalences;
	cNickName() 
	{ 
	}
	bool operator == (wstring name)  
	{
		unsigned int I;
		for (I=0; I<equivalences.size() && name!=equivalences[I]; I++);
		return I<equivalences.size(); 
	}
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
	cName(void) { hon=hon2=hon3=first=middle=middle2=last=suffix=any=wNULL; nickName=-1; };
	void operator = (const cName& n);
	bool operator == (const cName& n);
	bool justHonorific(void);
	bool getNickName(tIWMM firstName);
	void hn(wchar_t *namePartName,tIWMM namePart,wstring &accumulate,bool printShort);
	void hn(wchar_t *namePartName,tIWMM namePart,wchar_t *accumulate,bool printShort);
	bool hn(tIWMM namePart,wchar_t separationCharacter,wstring &accumulate);
	wstring print(wstring &message,bool printShort);
	bool notNull();
	wstring print(wchar_t *message,bool printShort);
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

