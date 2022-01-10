#define maxCategoryLength 1024
#define dbPedia_Ontology_Type 1
#define YAGO_Ontology_Type 2
#define UMBEL_Ontology_Type 3 // <http://umbel.org/umbel/rc/, <http://umbel.org/umbel#
#define OpenGIS_Ontology_Type 4

class cOntologyEntry
{
public:
	wstring compactLabel;
	wstring infoPage;
	wstring abstractDescription;
	wstring commentDescription;
	wstring birthDate;
	wstring birthPlace;
	wstring occupation;
	int numLine;
	int ontologyHierarchicalRank;
	int ontologyType;
	int resourceType;
	int descriptionFilled;
	unordered_set <wstring> superClasses;
	vector <int> superClassResourceTypes;
	cOntologyEntry()
	{
		numLine = ontologyType = resourceType = -1;
		ontologyHierarchicalRank = 100; // ignore
		descriptionFilled = -1; // number of rows found in ontology
	}
	wstring toString(wstring& tmpstr, wstring origin)
	{
		wstring tmpstr2, tmpstr3, tmpstr4, tmpstr5;
		tmpstr = origin + L":" + ontologyTypeString(ontologyType, resourceType, tmpstr5) + L":" + compactLabel;
		if (infoPage.length() > 0)
			tmpstr += L"\ninfoPage:" + infoPage;
		if (birthDate.length() > 0)
			tmpstr += L"\nbirthDate:" + birthDate;
		if (birthPlace.length() > 0)
			tmpstr += L"\nbirthPlace:" + birthPlace;
		if (occupation.length() > 0)
			tmpstr += L"\noccupation:" + occupation;
		if (abstractDescription.length() > 0)
			tmpstr += L"\nabstract:" + abstractDescription;
		if (commentDescription.length() > 0)
			tmpstr += L"\ncomment:" + commentDescription;
		setString(superClasses, tmpstr2, L" ");
		if (tmpstr2.length() > 0)
			tmpstr += L"[" + tmpstr2 + L"]";
		if (ontologyHierarchicalRank > 0)
			tmpstr += L":ontologyHierarchicalRank " + itos(ontologyHierarchicalRank, tmpstr3);
		return tmpstr;
	}
	bool operator == (const cOntologyEntry& o)
	{
		if (compactLabel != o.compactLabel) return false;
		if (infoPage != o.infoPage) return false;
		if (birthDate != o.birthDate) return false;
		if (birthPlace != o.birthPlace) return false;
		if (occupation != o.occupation) return false;
		if (abstractDescription != o.abstractDescription) return false;
		if (commentDescription != o.commentDescription) return false;
		if (numLine != o.numLine) return false;
		if (ontologyHierarchicalRank != o.ontologyHierarchicalRank) return false;
		if (ontologyType != o.ontologyType) return false;
		if (descriptionFilled != o.descriptionFilled) return false;
		if (superClasses != o.superClasses) return false;
		return true;
	}
	bool operator != (const cOntologyEntry& o)
	{
		return !(*this == o);
	}
	void lplog(int whichLog, wstring origin)
	{
		wstring tmpstr;
		::lplog(whichLog, L"%s", toString(tmpstr, origin).c_str());
	}
};

bool copy(unordered_map <wstring, cOntologyEntry>::iterator& hint, void* buf, int& where, int limit, unordered_map <wstring, cOntologyEntry>& hm);

class cTreeCat
{
public:
	unordered_map <wstring, cOntologyEntry>::iterator cli;
	wstring typeObject;
	int confidence;
	wstring abstract;
	wstring birthDate;
	wstring birthPlace;
	wstring occupation;
	wstring qtype;
	wstring key;
	wstring top; // temporary used for back mapping of top class types
	wstring parentObject;
	wstring derivation; // which classes lead to most superclasses? (attempt to go up the heirarchy of types in the ontology may result in a multiplicity of types which is unhelpful.  This helps track which types lend most to the multiplicity)
	vector <wstring> wikipediaLinks;
	vector <wstring> professionLinks;
	bool preferred;
	bool exactMatch;
	bool preferredUnknownClass;

	cTreeCat(unordered_map <wstring, cOntologyEntry>::iterator cli, wstring typeObject, wstring& parentObject, wstring qtype, int confidence, string& k, string& description, vector <wstring>& wikipediaLinks, vector <wstring>& professionLinks, bool exactMatch)
	{
		this->cli = cli;
		this->typeObject = typeObject;
		this->parentObject = parentObject;
		this->qtype = qtype;
		this->confidence = confidence;
		mTW(k, this->key);
		this->wikipediaLinks = wikipediaLinks;
		this->professionLinks = professionLinks;
		mTW(description, this->abstract);
		preferred = false;
		this->exactMatch = exactMatch;
		preferredUnknownClass = false;
	}
	cTreeCat(unordered_map <wstring, cOntologyEntry>::iterator cli, wstring typeObject, wstring& parentObject, wstring qtype, int confidence, wstring derivation)
	{
		this->cli = cli;
		this->typeObject = typeObject;
		this->parentObject = parentObject;
		this->qtype = qtype;
		this->confidence = confidence;
		this->derivation = derivation;
		preferred = false;
		exactMatch = false;
		preferredUnknownClass = false;
	}
	cTreeCat(unordered_map <wstring, cOntologyEntry>::iterator cli)
	{
		this->cli = cli;
		preferred = false;
		exactMatch = false;
		preferredUnknownClass = false;
		confidence = 0;
	}
	cTreeCat()
	{
		//this->cli=(unordered_map <wstring, cOntologyEntry>::iterator)((void *) 0);
		preferred = false;
		exactMatch = false;
		preferredUnknownClass = false;
		confidence = 0;
	}
	bool equals(const cTreeCat* o)
	{
		if (cli->first != o->cli->first) return false;
		if (cli->second.compactLabel != o->cli->second.compactLabel) return false;
		return true;
	}
	bool operator == (const cTreeCat& o)
	{
		if (cli->first != o.cli->first) return false;
		if (cli->second != o.cli->second) return false;
		if (typeObject != o.typeObject) return false;
		if (parentObject != o.parentObject) return false;
		if (qtype != o.qtype) return false;
		if (key != o.key) return false;
		if (wikipediaLinks != o.wikipediaLinks) return false;
		if (professionLinks != o.professionLinks) return false;
		if (confidence != o.confidence) return false;
		if (infoPage != o.infoPage) return false;
		if (birthDate != o.birthDate) return false;
		if (birthPlace != o.birthPlace) return false;
		if (occupation != o.occupation) return false;
		if (abstract != o.abstract) return false;
		if (comment != o.comment) return false;
		if (preferred != o.preferred) return false;
		if (exactMatch != o.exactMatch) return false;
		if (preferredUnknownClass != o.preferredUnknownClass) return false;
		return true;
	}
	bool operator != (const cTreeCat& o)
	{
		return !(*this == o);
	}
	wstring toString(wstring& tmpstr);
	void lplogTC(int whichLog, wstring object);
	void assignDetails(wstring& a, wstring& c, wstring& ip, wstring& bd, wstring& bp, wstring& occ)
	{
		this->abstract = a;
		this->comment = c;
		this->infoPage = ip;
		this->birthDate = bd;
		this->birthPlace = bp;
		this->occupation = occ;
	}

	bool copy(void* buf, cOntologyEntry& dbsn, int& where, int limit)
	{
		if (!::copy(buf, dbsn.compactLabel, where, limit)) return false;
		if (!::copy(buf, dbsn.infoPage, where, limit)) return false;
		if (!::copy(buf, dbsn.birthDate, where, limit)) return false;
		if (!::copy(buf, dbsn.birthPlace, where, limit)) return false;
		if (!::copy(buf, dbsn.occupation, where, limit)) return false;
		if (!::copy(buf, dbsn.abstractDescription, where, limit)) return false;
		if (!::copy(buf, dbsn.commentDescription, where, limit)) return false;
		if (!::copy(buf, dbsn.numLine, where, limit)) return false;
		if (!::copy(buf, dbsn.ontologyType, where, limit)) return false;
		if (!::copy(buf, dbsn.ontologyHierarchicalRank, where, limit)) return false;
		if (!::copy(buf, dbsn.superClasses, where, limit)) return false;
		if (!::copy(buf, dbsn.descriptionFilled, where, limit)) return false;
		return true;
	}

	bool copy(void* buf, unordered_map <wstring, cOntologyEntry>::iterator dbsi, int& where, int limit)
	{
		if (!::copy(buf, dbsi->first, where, limit)) return false;
		if (!copy(buf, dbsi->second, where, limit)) return false;
		return true;
	}

	bool copy(unordered_map <wstring, cOntologyEntry>& hm, void* buf, int& where, int limit)
	{
		if (!::copy(cli, buf, where, limit, hm)) return false;
		if (!::copy(typeObject, buf, where, limit)) return false;
		if (!::copy(parentObject, buf, where, limit)) return false;
		if (!::copy(qtype, buf, where, limit)) return false;
		if (!::copy(confidence, buf, where, limit)) return false;
		if (!::copy(infoPage, buf, where, limit)) return false;
		if (!::copy(birthDate, buf, where, limit)) return false;
		if (!::copy(birthPlace, buf, where, limit)) return false;
		if (!::copy(occupation, buf, where, limit)) return false;
		if (!::copy(abstract, buf, where, limit)) return false;
		if (!::copy(comment, buf, where, limit)) return false;
		if (!::copy(wikipediaLinks, buf, where, limit)) return false;
		if (!::copy(professionLinks, buf, where, limit)) return false;
		int flags;
		if (!::copy(flags, buf, where, limit)) return false;
		preferred = (flags & 1) != 0;
		exactMatch = (flags & 2) != 0;
		preferredUnknownClass = (flags & 4) != 0;
		return true;
	}

	bool copy(void* buf, int& where, int limit)
	{
		if (!copy(buf, cli, where, limit)) return false;
		if (!::copy(buf, typeObject, where, limit)) return false;
		if (!::copy(buf, parentObject, where, limit)) return false;
		if (!::copy(buf, qtype, where, limit)) return false;
		if (!::copy(buf, confidence, where, limit)) return false;
		if (!::copy(buf, infoPage, where, limit)) return false;
		if (!::copy(buf, birthDate, where, limit)) return false;
		if (!::copy(buf, birthPlace, where, limit)) return false;
		if (!::copy(buf, occupation, where, limit)) return false;
		if (!::copy(buf, abstract, where, limit)) return false;
		if (!::copy(buf, comment, where, limit)) return false;
		if (!::copy(buf, wikipediaLinks, where, limit)) return false;
		if (!::copy(buf, professionLinks, where, limit)) return false;
		int flags = ((preferred) ? 1 : 0) | (((exactMatch) ? 1 : 0) << 1) | ((preferredUnknownClass ? 1 : 0) << 2);
		if (!::copy(buf, flags, where, limit)) return false;
		return true;
	}

	void logIdentity(int logType, wstring object, bool printOnlyPreferred, wstring& rdfInfoPrinted)
	{
		if (printOnlyPreferred && !preferred && !exactMatch) return;
		wstring tmpstr, tmpstr2, tmpstr3;
		::lplog(logType, L"%s%s%s%s[%d]ISTYPE[LI] %s:%s(%s):%s:rank %d (%s,%s)",
			object.c_str(), (preferred) ? L":PREFERRED " : L"", (exactMatch) ? L"EM " : L"", (preferredUnknownClass) ? L"PU " : L"", confidence,
			cli->first.c_str(), ontologyTypeString(cli->second.ontologyType, cli->second.resourceType, tmpstr3), qtype.c_str(), cli->second.compactLabel.c_str(), cli->second.ontologyHierarchicalRank,
			setString(cli->second.superClasses, tmpstr, L" ").c_str(), parentObject.c_str());
		if (rdfInfoPrinted != abstract)
		{
			::lplog(logType, L"    %s:%s:%s:%s",
				parentObject.c_str(), typeObject.c_str(), infoPage.c_str(), abstract.c_str());
			rdfInfoPrinted = abstract;
		}
	}

private:
	wstring infoPage;
	wstring comment;
};

class cOntology
{
public:
	static bool cacheRdfTypes;  // determines whether rdfTypes are cached in memory.  fileCaching is whether they are cached on disk.
	static unordered_map <wstring, cOntologyEntry> dbPediaOntologyCategoryList;
	static bool maxFieldLengths();
	static bool writeOntologyList();
	static bool readOntologyList();
	static bool setPreferred(unordered_map <wstring ,int > &topHierarchyClassIndexes,vector <cTreeCat *> &rdfTypes);
	static void rdfIdentify(wstring object, vector <cTreeCat *> &rdfTypes, wstring fromWhere, bool fileCaching=true);
	static void includeSuperClasses(unordered_map <wstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes);
	static void compressPath(wchar_t *path);
	static void compareRDFTypes();
	static bool inNoERDFTypesDBTable(wstring newPath);
	static bool insertNoERDFTypesDBTable(wstring newPath);
	static int printRDFTypes(const wchar_t * kind, vector <cTreeCat *> &rdfTypes);
	static int printExtendedRDFTypes(wchar_t *kind, vector <cTreeCat *> &rdfTypes, unordered_map <wstring, int > &topHierarchyClassIndexes);
	static void readOpenLibraryInternetArchiveWorksDump();
	static int fillOntologyList(bool reInitialize);

private:
	static unordered_map<wstring, vector <cTreeCat *> > rdfTypeMap; 
	static unordered_map<wstring, int > rdfTypeNumMap; 
	static bool superClassesAllPopulated;

	static MYSQL mysql;
	static bool alreadyConnected;
	static set<string> rejectCategories; // only written during initialize
	static bool forceWebReread;
	// available for future use
	static int lookupInFreebaseSuggest(wstring object,vector <cTreeCat *> &rdfTypes);
	static int getAcronymRDFTypes(wstring &object,vector <cTreeCat *> &rdfTypes);
	// Freebase
	static wstring getFBDescription(wstring id,wstring name);
	static int lookupInFreebase(wstring object,vector <cTreeCat *> &rdfTypes);

	static int getAcronyms(wstring &object,vector <wstring> &acronyms);
	static unordered_map <wstring, cOntologyEntry>::iterator findAnyYAGOSuperClass(wstring cl);
	static unordered_map <wstring, cOntologyEntry>::iterator findCategory(wstring &icat);
	static int findCategoryRank(wstring &qtype,wstring &parentObject,wstring &object,vector <cTreeCat *> &rdfTypes,wstring &uri);
	static bool extractResults(wstring begin,wstring uobject,wstring end,wstring qtype, vector <cTreeCat *> &rdfTypes,vector <wstring> &resources,wstring parentObject);
	static bool inRDFTypeNotFoundTable(wchar_t *object);
	static bool insertRDFTypeNotFoundTable(wchar_t *object);
	static int getRDFTypesMaster(wstring object, vector <cTreeCat *> &rdfTypes, wstring fromWhere, bool fileCaching=true);
	static bool topClassesAvailableToBeAdded(unordered_map <wstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes, int rdfBaseTypeOffset);
	static void includeAllSuperClasses(unordered_map <wstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes, int recursionLevel, int rdfBaseTypeOffset);
	static int fillRanks(int ontologyType);
	static void getRDFTypesFromDbPedia(wstring object,vector <cTreeCat *> &rdfTypes,wstring fromWhere);
	static int enterCategory(string &id,string &k,string &propertyValue,string &description,string &slobject,wstring &object,string &objectType,string &name,vector <wstring> &wikipediaLinks,vector <wstring> &professionLinks,vector <cTreeCat *> &rdfTypes);
	static int lookupInFreebaseQuery(wstring &object,string &slobject,wstring &q,vector <cTreeCat *> &rdfTypes,bool accumulateAliases);
	static int lookupLinks(vector <wstring> &links);
	static wstring stripUmbel(wstring umbelClass, wstring &compactLabel, wstring &labelWithSpace, int &UMBELType);
	static void importUMBELN3Files(const wchar_t * basepath, const wchar_t * extension, unordered_map < wstring, unordered_map <wstring, set< wstring > > > &triplets);
	static bool readUMBELSuperClasses();
	static int readYAGOOntology(const wchar_t * filepath, int &numYAGOEntries, int &numSuperClasses);
	static int readYAGOOntology();
	static int readRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes);
	static wstring extractLinkedFreebaseDescription(string &properties,wstring &description);
	static void cutFinalDigits(wstring &cat);
	static wstring decodeURL(wstring input,wstring &decodedURL);
	static bool copy(void *buf,cOntologyEntry &dbsn,int &where,int limit);
	static bool copy(void *buf,unordered_map <wstring, cOntologyEntry>::iterator dbsi,int &where,int limit);
	static int getDBPediaPath(int where,wstring webAddress,wstring &buffer,wstring epath);
	static int followDbpediaLink(wstring link, wstring property, wstring& value);
	static int getDescription(wstring label, wstring objectName, wstring& abstract, wstring& comment, wstring& infoPage, wstring& birthDate, wstring& birthPlace, wstring& occupation);
	//static int getDescription(unordered_map <wstring, cOntologyEntry>::iterator cli);
	//static int getDescription(vector <wstring> labels,wstring objectName,wstring &abstract,wstring &comment,wstring &infoPage, wstring& occupation);
	static int writeRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes);
	//test
	static void testGetRDFTypesFromDbPedia(wstring object,vector <cTreeCat *> &rdfTypes,wstring parentObject,wstring fromWhere);
	static int testDBPediaPath(int where,wstring webAddress,wstring &buffer,wstring epath);
	static void compareFreebaseWebToFreebaseRDL();
	static void compareFreebaseWebToFreebaseDescriptionRDL();
	static void testWikipedia();
	static void printIdentities(wchar_t *objects[]);
	static void printIdentity(wstring object);
	static bool copy(cOntologyEntry &dbsn,void *buf,int &where,int limit);
	static int readDbPediaOntology();
};

