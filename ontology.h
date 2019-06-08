class Ontology
{
public:
	static bool cacheRdfTypes;
	static unordered_map <wstring, dbs> dbPediaOntologyCategoryList;
	static void initialize();
	static bool setPreferred(unordered_map <wstring ,int > &topHierarchyClassIndexes,vector <cTreeCat *> &rdfTypes);
	static void rdfIdentify(wstring object, vector <cTreeCat *> &rdfTypes, wstring fromWhere, bool fileCaching=true);
	static void logIdentity(int logType,wstring object,cTreeCat *cat,bool printOnlyPreferred);
	static void includeSuperClasses(unordered_map <wstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes);
	static void compressPath(wchar_t *path);
	static void compareRDFTypes();
	static int readUMBELNS();

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

	static int fillOntologyList(bool reInitialize);
	static int getAcronyms(wstring &object,vector <wstring> &acronyms);
	static unordered_map <wstring, dbs>::iterator findAnyYAGOSuperClass(wstring cl);
	static unordered_map <wstring, dbs>::iterator findCategory(wstring &icat);
	static int findCategoryRank(wstring &qtype,wstring &parentObject,wstring &object,vector <cTreeCat *> &rdfTypes,wstring &uri);
	static bool extractResults(wstring begin,wstring uobject,wstring end,wstring qtype, vector <cTreeCat *> &rdfTypes,vector <wstring> &resources,wstring parentObject);
	static bool inRDFTypeNotFoundTable(wchar_t *object);
	static int getRDFTypesMaster(wstring object, vector <cTreeCat *> &rdfTypes, wstring fromWhere, bool fileCaching=true);
	static bool isolateKnownClasses(unordered_map <wstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes, int rdfBaseTypeOffset);
	static void includeAllSuperClasses(unordered_map <wstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes, int recursionLevel, int rdfBaseTypeOffset);
	static int fillRanks(int ontologyType);
	static void getRDFTypesFromDbPedia(wstring object,vector <cTreeCat *> &rdfTypes,wstring parentObject,wstring fromWhere);
	static int enterCategory(string &id,string &k,string &propertyValue,string &description,string &slobject,wstring &object,string &objectType,string &name,vector <wstring> &wikipediaLinks,vector <wstring> &professionLinks,vector <cTreeCat *> &rdfTypes);
	static int lookupInFreebaseQuery(wstring &object,string &slobject,wstring &q,vector <cTreeCat *> &rdfTypes,bool accumulateAliases);
	static int lookupLinks(vector <wstring> &links);
	static bool readUMBELSuperClasses();
	static int readYAGOOntology(wchar_t *filepath, int &numYAGOEntries, int &numSuperClasses);
	static int readYAGOOntology();
//	static int readDbPediaOntology();
	static int readRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes);
	static wstring extractLinkedFreebaseDescription(string &properties,wstring &description);
	static void cutFinalDigits(wstring &cat);
	static wstring decodeURL(wstring input,wstring &decodedURL);
	static bool copy(void *buf,dbs &dbsn,int &where,int limit);
	static bool copy(void *buf,unordered_map <wstring, dbs>::iterator dbsi,int &where,int limit);
	static int getDBPediaPath(int where,wstring webAddress,wstring &buffer,wstring epath);
	static int getDescription(wstring label,wstring objectName,wstring &abstract,wstring &comment,wstring &infoPage);
	static int getDescription(unordered_map <wstring, dbs>::iterator cli);
	static int getDescription(vector <wstring> labels,wstring objectName,wstring &abstract,wstring &comment,wstring &infoPage);
	static int writeRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes);
	//test
	static void testGetRDFTypesFromDbPedia(wstring object,vector <cTreeCat *> &rdfTypes,wstring parentObject,wstring fromWhere);
	static int testDBPediaPath(int where,wstring webAddress,wstring &buffer,wstring epath);
	static void compareFreebaseWebToFreebaseRDL();
	static void compareFreebaseWebToFreebaseDescriptionRDL();
	static void testWikipedia();
	static void printIdentities(wchar_t *objects[]);
	static void printIdentity(wstring object);
	static bool copy(dbs &dbsn,void *buf,int &where,int limit);
	//static bool copy(unordered_map <wstring, dbs>::iterator &hint,void *buf,int &where,int limit,unordered_map <wstring, dbs> &hm);
	static int readDbPediaOntology();
};

