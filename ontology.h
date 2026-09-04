/*
	ontology.h - YAGO / DBpedia / UMBEL / OpenGIS blended-ontology types used to type named objects.

	Overview:
		Declares the in-memory ontology node (cOntologyEntry), one candidate type for an
		object (cTreeCat), and the static cOntology facade that fillOntologyList() /
		rdfIdentify() implement in createOntology.cpp.  Categories from YAGO, DBpedia,
		UMBEL and OpenGIS are blended into one map keyed by a lower-cased, space-separated
		label so a surface name can be looked up regardless of which source supplied it.

	Pipeline position:
		Initialization (README "Ontology"): fillOntologyList() is called from rdfIdentify()
		the first time an object is typed.  identifyISARelation() in source.cpp then
		walks the resulting cTreeCat list.  Not part of tokenize/parse itself.

	Key data structures / globals:
		- cOntologyEntry - one class/category: compactLabel, superClasses, hierarchical
		  rank (100 = unranked / ignore), ontologyType 1..4, optional DBpedia description
		  fields.  Lives in cOntology::dbPediaOntologyCategoryList for the process lifetime.
		- cTreeCat - one typed hit for an object: iterator into the category map plus
		  confidence, preferred / exactMatch flags, and a private infoPage/comment copy.
		  Heap-allocated; when cacheRdfTypes is true the pointers are shared in rdfTypeMap
		  and must not be deleted.
		- cOntology - all static; see createOntology.cpp for the maps and MySQL handle.

	Notes / gotchas:
		- Default cTreeCat() explicitly value-initializes cli (a singular iterator);
		  both call sites resolve it via copy() before any dereference.
		- copy() serializers omit resourceType / superClassResourceTypes / key / derivation.
		- Ontology type constants match README Structure members item 7.
*/
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#pragma once
// Batch B12: include guard added -- source.h now includes this header for
// cTreeCat rather than depending on every .cpp to include ontology.h first,
// and several .cpp files include it directly as well.
#define maxCategoryLength 1024
#define dbPedia_Ontology_Type 1
#define YAGO_Ontology_Type 2
#define UMBEL_Ontology_Type 3 // <http://umbel.org/umbel/rc/, <http://umbel.org/umbel#
#define OpenGIS_Ontology_Type 4

class cOntologyEntry
{
public:
	lpwstring compactLabel;
	lpwstring infoPage;
	lpwstring abstractDescription;
	lpwstring commentDescription;
	lpwstring birthDate;
	lpwstring birthPlace;
	lpwstring occupation;
	int numLine;
	int ontologyHierarchicalRank;
	int ontologyType;
	int resourceType;
	int descriptionFilled;
	unordered_set <lpwstring> superClasses;
	vector <int> superClassResourceTypes;
	// Rank 100 means "not yet placed in the hierarchy" (fillRanks / findCategoryRank).
	// descriptionFilled is the SPARQL row count from getDescription, or -1 if never fetched.
	cOntologyEntry()
	{
		numLine = ontologyType = resourceType = -1;
		ontologyHierarchicalRank = 100; // ignore
		descriptionFilled = -1; // number of rows found in ontology
	}
	// Format this entry into tmpstr (reused scratch).  origin is the map key / caller label.
	lpwstring toString(lpwstring& tmpstr, lpwstring origin)
	{
		lpwstring tmpstr2, tmpstr3, tmpstr4, tmpstr5;
		tmpstr = origin + u":" + ontologyTypeString(ontologyType, resourceType, tmpstr5) + u":" + compactLabel;
		if (infoPage.length() > 0)
			tmpstr += u"\ninfoPage:" + infoPage;
		if (birthDate.length() > 0)
			tmpstr += u"\nbirthDate:" + birthDate;
		if (birthPlace.length() > 0)
			tmpstr += u"\nbirthPlace:" + birthPlace;
		if (occupation.length() > 0)
			tmpstr += u"\noccupation:" + occupation;
		if (abstractDescription.length() > 0)
			tmpstr += u"\nabstract:" + abstractDescription;
		if (commentDescription.length() > 0)
			tmpstr += u"\ncomment:" + commentDescription;
		setString(superClasses, tmpstr2, u" ");
		if (tmpstr2.length() > 0)
			tmpstr += u"[" + tmpstr2 + u"]";
		if (ontologyHierarchicalRank > 0)
			tmpstr += u":ontologyHierarchicalRank " + itos(ontologyHierarchicalRank, tmpstr3);
		return tmpstr;
	}
	// Field-wise equality, including resourceType and superClassResourceTypes.
	// Not currently called anywhere in the tree (verified repo-wide); the fields
	// were added here so a future caller gets full identity rather than a
	// silent partial compare.
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
		if (resourceType != o.resourceType) return false;
		if (descriptionFilled != o.descriptionFilled) return false;
		if (superClasses != o.superClasses) return false;
		if (superClassResourceTypes != o.superClassResourceTypes) return false;
		return true;
	}
	bool operator != (const cOntologyEntry& o)
	{
		return !(*this == o);
	}
	// Log toString(origin) to whichLog via the process-wide lplog.
	void lplog(int whichLog, lpwstring origin)
	{
		lpwstring tmpstr;
		::lplog(whichLog, u"%s", toString(tmpstr, origin).c_str());
	}
};

// Binary-cache insert: deserialize key+entry from buf into hm and set hint to the inserted iterator.
bool copy(unordered_map <lpwstring, cOntologyEntry>::iterator& hint, void* buf, int& where, int limit, unordered_map <lpwstring, cOntologyEntry>& hm);

class cTreeCat
{
public:
	unordered_map <lpwstring, cOntologyEntry>::iterator cli;
	lpwstring typeObject;
	int confidence;
	lpwstring abstract;
	lpwstring birthDate;
	lpwstring birthPlace;
	lpwstring occupation;
	lpwstring qtype;
	lpwstring key;
	lpwstring top; // temporary used for back mapping of top class types
	lpwstring parentObject;
	lpwstring derivation; // which classes lead to most superclasses? (attempt to go up the heirarchy of types in the ontology may result in a multiplicity of types which is unhelpful.  This helps track which types lend most to the multiplicity)
	vector <lpwstring> wikipediaLinks;
	vector <lpwstring> professionLinks;
	bool preferred;
	bool exactMatch;
	bool preferredUnknownClass;

	// Freebase hit: k/description are narrow strings converted with mTW.  preferred stays false.
	cTreeCat(unordered_map <lpwstring, cOntologyEntry>::iterator cli, lpwstring typeObject, lpwstring& parentObject, lpwstring qtype, int confidence, string& k, string& description, vector <lpwstring>& wikipediaLinks, vector <lpwstring>& professionLinks, bool exactMatch)
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
	// Hierarchy-walk hit (includeAllSuperClasses).  derivation records the path of class keys.
	cTreeCat(unordered_map <lpwstring, cOntologyEntry>::iterator cli, lpwstring typeObject, lpwstring& parentObject, lpwstring qtype, int confidence, lpwstring derivation)
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
	// Separator / placeholder: only cli is meaningful (often the SEPARATOR category).
	cTreeCat(unordered_map <lpwstring, cOntologyEntry>::iterator cli)
	{
		this->cli = cli;
		preferred = false;
		exactMatch = false;
		preferredUnknownClass = false;
		confidence = 0;
	}
	// Used by readRDFTypes before copy() fills fields.  cli is explicitly
	// value-initialized to a singular (non-dereferenceable) iterator rather than
	// left to whatever the implicit member-init happened to produce; both current
	// call sites (createOntology.cpp, getWikipedia.cpp) invoke copy() immediately
	// afterward, which resolves cli against dbPediaOntologyCategoryList before it
	// is ever dereferenced.
	cTreeCat() : cli()
	{
		preferred = false;
		exactMatch = false;
		preferredUnknownClass = false;
		confidence = 0;
	}
	// Identity by category key and compactLabel only.  Dereferences both cli iterators.
	bool equals(const cTreeCat* o)
	{
		if (cli->first != o->cli->first) return false;
		if (cli->second.compactLabel != o->cli->second.compactLabel) return false;
		return true;
	}
	// Full field compare including the private infoPage/comment copies.  Dereferences cli.
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
	lpwstring toString(lpwstring& tmpstr);
	void lplogTC(int whichLog, lpwstring object);
	// Copy DBpedia description fields from getDescription() onto this hit.
	void assignDetails(lpwstring& a, lpwstring& c, lpwstring& ip, lpwstring& bd, lpwstring& bp, lpwstring& occ)
	{
		this->abstract = a;
		this->comment = c;
		this->infoPage = ip;
		this->birthDate = bd;
		this->birthPlace = bp;
		this->occupation = occ;
	}

	// Deserialize one cOntologyEntry from the binary cache buffer.  where advances.
	// Omits resourceType / superClassResourceTypes (not in the on-disk format).
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

	// Serialize the category key plus the entry that dbsi points at.
	bool copy(void* buf, unordered_map <lpwstring, cOntologyEntry>::iterator dbsi, int& where, int limit)
	{
		if (!::copy(buf, dbsi->first, where, limit)) return false;
		if (!copy(buf, dbsi->second, where, limit)) return false;
		return true;
	}

	// Deserialize this cTreeCat from an .rdfTypes cache file, resolving cli against hm.
	// Flags packed as bit0 preferred, bit1 exactMatch, bit2 preferredUnknownClass.
	bool copy(unordered_map <lpwstring, cOntologyEntry>& hm, void* buf, int& where, int limit)
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

	// Serialize this cTreeCat (including cli key/entry) into an .rdfTypes cache buffer.
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

	// Log one ISTYPE[LI] line.  If printOnlyPreferred, skip unless preferred or exactMatch.
	// rdfInfoPrinted suppresses repeat abstract dumps for the same description text.
	void logIdentity(int logType, lpwstring object, bool printOnlyPreferred, lpwstring& rdfInfoPrinted)
	{
		if (printOnlyPreferred && !preferred && !exactMatch) return;
		lpwstring tmpstr, tmpstr2, tmpstr3;
		::lplog(logType, u"%s%s%s%s[%d]ISTYPE[LI] %s:%s(%s):%s:rank %d (%s,%s)",
			object.c_str(), (preferred) ? u":PREFERRED " : u"", (exactMatch) ? u"EM " : u"", (preferredUnknownClass) ? u"PU " : u"", confidence,
			cli->first.c_str(), ontologyTypeString(cli->second.ontologyType, cli->second.resourceType, tmpstr3), qtype.c_str(), cli->second.compactLabel.c_str(), cli->second.ontologyHierarchicalRank,
			setString(cli->second.superClasses, tmpstr, u" ").c_str(), parentObject.c_str());
		if (rdfInfoPrinted != abstract)
		{
			::lplog(logType, u"    %s:%s:%s:%s",
				parentObject.c_str(), typeObject.c_str(), infoPage.c_str(), abstract.c_str());
			rdfInfoPrinted = abstract;
		}
	}

private:
	lpwstring infoPage;
	lpwstring comment;
};

// Static facade for the blended ontology.  All members are process-lifetime.
// Implementation and the maps themselves live in createOntology.cpp.
class cOntology
{
public:
	static bool cacheRdfTypes;  // determines whether rdfTypes are cached in memory.  fileCaching is whether they are cached on disk.
	static unordered_map <lpwstring, cOntologyEntry> dbPediaOntologyCategoryList;
	static bool maxFieldLengths();
	static bool writeOntologyList();
	static bool readOntologyList();
	static bool setPreferred(unordered_map <lpwstring ,int > &topHierarchyClassIndexes,vector <cTreeCat *> &rdfTypes);
	static void rdfIdentify(lpwstring object, vector <cTreeCat *> &rdfTypes, lpwstring fromWhere, bool fileCaching=true);
	static void includeSuperClasses(unordered_map <lpwstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes);
	static void compressPath(lpchar_t *path);
	static void compareRDFTypes();
	static bool inNoERDFTypesDBTable(lpwstring newPath);
	static bool insertNoERDFTypesDBTable(lpwstring newPath);
	static int printRDFTypes(const lpchar_t * kind, vector <cTreeCat *> &rdfTypes);
	static int printExtendedRDFTypes(lpchar_t *kind, vector <cTreeCat *> &rdfTypes, unordered_map <lpwstring, int > &topHierarchyClassIndexes);
	static void readOpenLibraryInternetArchiveWorksDump();
	static int fillOntologyList(bool reInitialize);

private:
	static unordered_map<lpwstring, vector <cTreeCat *> > rdfTypeMap; 
	static unordered_map<lpwstring, int > rdfTypeNumMap; 
	static bool superClassesAllPopulated;

	static MYSQL mysql;
	static bool alreadyConnected;
	static set<string> rejectCategories; // only written during initialize
	static bool forceWebReread;
	// available for future use
	static int lookupInFreebaseSuggest(lpwstring object,vector <cTreeCat *> &rdfTypes);
	static int getAcronymRDFTypes(lpwstring &object,vector <cTreeCat *> &rdfTypes);
	// Freebase
	static lpwstring getFBDescription(lpwstring id,lpwstring name);
	static int lookupInFreebase(lpwstring object,vector <cTreeCat *> &rdfTypes);

	static int getAcronyms(lpwstring &object,vector <lpwstring> &acronyms);
	static unordered_map <lpwstring, cOntologyEntry>::iterator findAnyYAGOSuperClass(lpwstring cl);
	static unordered_map <lpwstring, cOntologyEntry>::iterator findCategory(lpwstring &icat);
	static int findCategoryRank(lpwstring &qtype,lpwstring &parentObject,lpwstring &object,vector <cTreeCat *> &rdfTypes,lpwstring &uri);
	static bool extractResults(lpwstring begin,lpwstring uobject,lpwstring end,lpwstring qtype, vector <cTreeCat *> &rdfTypes,vector <lpwstring> &resources,lpwstring parentObject);
	static bool inRDFTypeNotFoundTable(lpchar_t *object);
	static bool insertRDFTypeNotFoundTable(lpchar_t *object);
	static int getRDFTypesMaster(lpwstring object, vector <cTreeCat *> &rdfTypes, lpwstring fromWhere, bool fileCaching=true);
	static bool topClassesAvailableToBeAdded(unordered_map <lpwstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes, int rdfBaseTypeOffset);
	static void includeAllSuperClasses(unordered_map <lpwstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes, int recursionLevel, int rdfBaseTypeOffset);
	static int fillRanks(int ontologyType);
	static void getRDFTypesFromDbPedia(lpwstring object,vector <cTreeCat *> &rdfTypes,lpwstring fromWhere);
	static int enterCategory(string &id,string &k,string &propertyValue,string &description,string &slobject,lpwstring &object,string &objectType,string &name,vector <lpwstring> &wikipediaLinks,vector <lpwstring> &professionLinks,vector <cTreeCat *> &rdfTypes);
	static int lookupInFreebaseQuery(lpwstring &object,string &slobject,lpwstring &q,vector <cTreeCat *> &rdfTypes,bool accumulateAliases);
	static int lookupLinks(vector <lpwstring> &links);
	static lpwstring stripUmbel(lpwstring umbelClass, lpwstring &compactLabel, lpwstring &labelWithSpace, int &UMBELType);
	static void importUMBELN3Files(const lpchar_t * basepath, const lpchar_t * extension, unordered_map < lpwstring, unordered_map <lpwstring, set< lpwstring > > > &triplets);
	static bool readUMBELSuperClasses();
	static int readYAGOOntology(const lpchar_t * filepath, int &numYAGOEntries, int &numSuperClasses);
	static int readYAGOOntology();
	static int readRDFTypes(lpchar_t path[4096],vector <cTreeCat *> &rdfTypes);
	static lpwstring extractLinkedFreebaseDescription(string &properties,lpwstring &description);
	static void cutFinalDigits(lpwstring &cat);
	static lpwstring decodeURL(lpwstring input,lpwstring &decodedURL);
	static bool copy(void *buf,cOntologyEntry &dbsn,int &where,int limit);
	static bool copy(void *buf,unordered_map <lpwstring, cOntologyEntry>::iterator dbsi,int &where,int limit);
	static int getDBPediaPath(int where,lpwstring webAddress,lpwstring &buffer,lpwstring epath);
	static int followDbpediaLink(lpwstring link, lpwstring property, lpwstring& value);
	static int getDescription(lpwstring label, lpwstring objectName, lpwstring& abstract, lpwstring& comment, lpwstring& infoPage, lpwstring& birthDate, lpwstring& birthPlace, lpwstring& occupation);
	//static int getDescription(unordered_map <lpwstring, cOntologyEntry>::iterator cli);
	//static int getDescription(vector <lpwstring> labels,lpwstring objectName,lpwstring &abstract,lpwstring &comment,lpwstring &infoPage, lpwstring& occupation);
	static int writeRDFTypes(lpchar_t path[4096],vector <cTreeCat *> &rdfTypes);
	//test
	static int testDBPediaPath(int where,lpwstring webAddress,lpwstring &buffer,lpwstring epath);
	static void testWikipedia();
	static void printIdentities(lpchar_t *objects[]);
	static void printIdentity(lpwstring object);
	static bool copy(cOntologyEntry &dbsn,void *buf,int &where,int limit);
	static int readDbPediaOntology();
};

