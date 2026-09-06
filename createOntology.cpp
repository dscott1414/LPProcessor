/*
	createOntology.cpp - blend YAGO / DBpedia / UMBEL / OpenGIS into one in-memory ontology
	and resolve named objects to typed cTreeCat hits.

	Overview:
		fillOntologyList() reads yago_taxonomy.ttl, yago_type_links.ttl,
		dbpedia_2016-10.nt and umbel downloads\*.n3 (or the binary
		CACHEDIR\dbPediaCache\_rdfTypes cache) into
		cOntology::dbPediaOntologyCategoryList.  rdfIdentify() then looks an
		object up via local Virtuoso SPARQL (types, redirects, disambiguations),
		Freebase (freebaseProperties), and thefreedictionary.com acronyms, caches
		the hit list as .rdfTypes, and walks superclasses until a known top class
		(person/place/...) can be preferred.

	Pipeline position:
		Initialization / object typing.  rdfIdentify() is the public entry
		(identifyISARelation in source.cpp).  fillOntologyList(false) runs on
		first use.  Not on the tokenize/parse path.

	Key entry points:
		- fillOntologyList() - build or load the blended category map
		- rdfIdentify() - type one object into rdfTypes
		- getRDFTypesMaster() / getRDFTypesFromDbPedia() - cache + SPARQL walk
		- includeSuperClasses() / setPreferred() - hierarchy + preferred flags
		- readYAGOOntology() / readDbPediaOntology() / readUMBELSuperClasses()

	Key data structures / globals:
		- dbPediaOntologyCategoryList - process-lifetime category map
		- rdfTypeMap / rdfTypeNumMap - per-object hit cache (rdfTypeMapSRWLock)
		- rejectCategories - Freebase types dropped as too generic
		- basehttpquery - Virtuoso at localhost:8890 (dbpedia.org SPARQL commented out)
		- cacheRdfTypes / forceWebReread / alreadyConnected / mysql

	Dependencies:
		Virtuoso SPARQL, MySQL (ontology, noRDFTypes, noERDFTypes,
		freebaseProperties, openLibraryInternetArchiveBooksDump), files under
		source\lists, dbpedia_downloads\2016-10, umbel downloads, CACHEDIR\dbPediaCache,
		WinHTTP / cInternet, hardcoded M:\ol_dump_works_... for Open Library.

	Notes / gotchas:
		- writeRDFTypes / readYAGOOntology(filepath,...) / readN3FileIntoTripletMap used to
		  put 4-20MB parse buffers on the stack (MAX_BUF*10, MAXYAGOBUF, MAX_BUF); they now
		  tmalloc/tfree them on the heap instead, since these only ever fit because of the
		  ~21MB StackReserveSize in the .vcxproj (see the smaller MAX_BUF-sized buffers
		  still on the stack elsewhere in this file, e.g. fillOntologyList's local
		  buffer[MAX_BUF] - those are ~2MB and left alone).
		- getRDFTypesMaster mutates rdfTypeNumMap under an exclusive SRWLOCK (both the
		  cache-hit counter bump and the cache-miss insert are writes).
		- SQL for noRDFTypes / noERDFTypes / Freebase escapes single quotes
		  (escapeSingleQuote) before concatenation; still not a bound parameter, so this
		  is defense against accidental quotes in scraped text, not a hardened query.
		- decodeURL guards I+1/I+2 after '%' before reading them.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "time.h"
#include <errno.h>
#include <functional>
#include "profile.h"
#include "mysqldb.h"
#include "mysqld_error.h"
#include "internet.h"
#include <sstream>

lpwstring basehttpquery = u"http://localhost:8890/sparql?default-graph-uri=http%3A%2F%2Fdbpedia.org&query=";
lpwstring decodedbasehttpquery = u"http://localhost:8890/sparql?default-graph-uri=http://dbpedia.org&query=";
//lpwstring basehttpquery=u"http://dbpedia.org/sparql?default-graph-uri=http%3A%2F%2Fdbpedia.org&query=";
lpwstring prefix_foaf = u"PREFIX+foaf%3A+%3Chttp%3A%2F%2Fxmlns.com%2Ffoaf%2F0.1%2F%3E%0D%0A";
lpwstring prefix_colon = u"PREFIX+%3A+%3Chttp%3A%2F%2Fdbpedia.org%2Fresource%2F%3E%0D%0A";
lpwstring prefix_owl_ontology = u"PREFIX+dbpedia-owl%3A+%3Chttp%3A%2F%2Fdbpedia.org%2Fontology%2F%3E%0D%0A";
lpwstring prefix_dbpedia = u"PREFIX+dbpedia%3A+%3Chttp%3A%2F%2Fdbpedia.org%2F%3E%0D%0A";
lpwstring selectWhere = u"SELECT+%3Fv+%0D%0AWHERE+%7B%0D%0A";
#define MAX_BUF 2000000
#define MAX_LEN 2048


MYSQL cOntology::mysql;
set<string> cOntology::rejectCategories = { "topic","track","document","edition","word","image","episode","title","subject","term","category","content","focus","type","base" };
bool cOntology::cacheRdfTypes = true;
bool cOntology::alreadyConnected = false;
bool cOntology::forceWebReread = false;
extern int logOntologyDetail;
unordered_map <lpwstring, cOntologyEntry> cOntology::dbPediaOntologyCategoryList;
unordered_map<lpwstring, vector <cTreeCat*> > cOntology::rdfTypeMap; // protected with rdfTypeMapSRWLock
unordered_map<lpwstring, int > cOntology::rdfTypeNumMap; // protected with rdfTypeMapSRWLock
bool cOntology::superClassesAllPopulated = false;

/***
	Read N3 files (begin) - used for reading UMBEL ontology
***/
// Advance s (which points at the opening '"') past an N3 quoted string, skipping
// '\"'.  Returns the space after the closing quote, or 0 if the quote never closes.
lpchar_t* readTillEndOfN3String(lpchar_t* s)
{
	s = s + 1;
	while (true)
	{
		lpchar_t* s4 = lp_strchr(s + 1, '"');
		if (!s4)
		{
			s4 = s;
			if (*(s + 1) != '@')
				s = 0;
			break;
		}
		s = s4;
		if (*(s4 - 1) != '\\')
		{
			break;
		}
	}
	if (s)
		s = lp_strchr(s + 1, ' ');
	return s;
}

// Consume an N3 """...""" literal that may span lines.  mapTo is the inner text
// (language tag after @ is stripped).  Returns the scan position after the closer.
lpchar_t* readTillEndOfTripleString(lpchar_t* s1, FILE* fp, lpchar_t* buffer, int& line, lpwstring& mapTo)
{
	lpchar_t* s2 = lp_strstr(s1 + 4, u"\"\"\"");
	if (s2)
	{
		s2 += 3;
		if (*s2 == '@')
		{
			*s2 = 0;
			s2 += 3;
		}
		else
			*s2 = 0;
		mapTo = s1 + 1;
		return s2;
	}
	mapTo = s1 + 1;
	for (line++; lp_fgetws(buffer, MAX_BUF, fp); line++)
	{
		lp_stripTrailing(buffer, u'\n');
		s2 = lp_strstr(buffer, u"\"\"\"");
		if (s2)
		{
			s2 += 3;
			if (*s2 == '@')
			{
				*s2 = 0;
				s2 += 3;
			}
			else
				*s2 = 0;
			mapTo += buffer;
			return s2;
		}
		mapTo += buffer;
	}
	return buffer; // take care of incorrect warning
}

// Read continuation objects of one predicate (comma-separated values).
// Returns true when the statement ends with '.' (finished the subject).
bool readN3OnePropertyLine(lpwstring ontologyRelation, lpwstring mapFrom, unordered_map < lpwstring, unordered_map <lpwstring, set< lpwstring > > >& triplets, FILE* fp, lpchar_t* buffer, int& line, int& nonConformingLines)
{
	for (line++; lp_fgetws(buffer, MAX_BUF, fp); line++)
	{
		for (int I = 0; buffer[I]; I++)
			if (buffer[I] == u'\t')
				buffer[I] = u' ';
		// must be one space, after initial white space
		int index2 = lp_wcsspn(buffer, u" ");
		if (!index2)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		lpchar_t* sm1 = buffer + index2 - 1;
		lpchar_t* sm2;
		if (*(sm1 + 1) == '"')
		{
			sm2 = readTillEndOfN3String(sm1);
		}
		else
			sm2 = lp_strchr(sm1 + 1, ' ');
		if (!sm2)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		*sm2 = 0;
		if (*(sm2 + 1) == ',')
		{
			triplets[ontologyRelation][mapFrom].insert(sm1 + 1);
			continue;
		}
		else if (*(sm2 + 1) != ';' && *(sm2 + 1) != '.')
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s [%d %c]", line, nonConformingLines, __LINE__, buffer, index2, sm1[1]);
			nonConformingLines++;
			continue;
		}
		triplets[ontologyRelation][mapFrom].insert(sm1 + 1);
		lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: %s %s %s", line, ontologyRelation.c_str(), mapFrom.c_str(), sm1 + 1);
		return (*(sm2 + 1) == '.');
	}
	return false;
}

// Read indented "predicate object ;/,/." lines after a subject.  Returns 0.
// Batch B10: path is const -- the directory walk now yields const paths.
int readN3TwoPropertyLine(const lpchar_t* path, lpwstring mapFrom, unordered_map < lpwstring, unordered_map <lpwstring, set< lpwstring > > >& triplets, FILE* fp, lpchar_t* buffer, int& line, int& nonConformingLines)
{
	for (line++; lp_fgetws(buffer, MAX_BUF, fp); line++)
	{
		for (int I = 0; buffer[I]; I++)
			if (buffer[I] == u'\t')
				buffer[I] = u' ';
		lp_stripTrailing(buffer, u'\n');
		if (buffer[0] == 0)
			continue;
		if (buffer[0] == '#')
			continue;
		// must be two spaces, after initial white space
		int index = lp_wcsspn(buffer, u" ");
		if (buffer[index] == 0)
			continue;
		if (!index)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		lpchar_t* s1 = lp_strchr(buffer + index, ' ');
		if (!s1)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		*s1 = 0;
		lpwstring ontologyRelation = buffer + index, mapTo;
		lpchar_t* s2;
		if (*(s1 + 1) == '"')
		{
			if (s1[2] == '"' && s1[3] == '"')
			{
				s2 = readTillEndOfTripleString(s1, fp, buffer, line, mapTo);
				//lplog(LOG_WIKIPEDIA, u"line #%d: triple string detected:(code line %d) \n**mapTo:\n%s\n**s2:\n%s\n**buffer:\n%s\n**END", line, __LINE__, mapTo.c_str(),s2,buffer);
			}
			else
				s2 = readTillEndOfN3String(s1);
		}
		else
			s2 = lp_strchr(s1 + 1, ' ');
		if (!s2)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		*s2 = 0;
		if (mapTo.empty())
		{
			lpchar_t* dropAt = lp_strchr(s1 + 1, u'@');
			if (dropAt) *dropAt = 0;
			if (s1[1] == u':') s1++;
			if (s1[1] == u'\"') s1++;
			dropAt = lp_strchr(s1 + 1, u'\"');
			if (dropAt) *dropAt = 0;
			mapTo = s1 + 1;
		}
		triplets[ontologyRelation][mapFrom].insert(mapTo);
		lplog(LOG_WIKIPEDIA, u"UMBEL %s:line #%d: relation=%s mapFrom=%s mapTo=%s", path, line, ontologyRelation.c_str(), mapFrom.c_str(), mapTo.c_str());
		bool appendingDef = false, finished = false;
		if (*(s2 + 1) == ',' && s2[lp_strlen(s2 + 1)] != ';' && s2[lp_strlen(s2 + 1)] != '.') // skips inline ,
		{
			if (*(s2 + 2) != 0)
			{
				lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
				nonConformingLines++;
				continue;
			}
			appendingDef = true;
		}
		else if (*(s2 + 1) == '.' || s2[lp_strlen(s2 + 1)] == '.') // skips inline ,
		{
			finished = true;
		}
		else if (*(s2 + 1) != ';' && s2[lp_strlen(s2 + 1)] != ';') // skips inline ,
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		// skos:altLabel "AB 205"@en ,
		//               "AB-205"@en ;
		if (appendingDef)
		{
			finished = readN3OnePropertyLine(ontologyRelation, mapFrom, triplets, fp, buffer, line, nonConformingLines);
		}
		if (finished)
			break;
	}
	return 0;
}

// <http://umbel.org/umbel/rc/Article_PCW> owl:equivalentClass <http://purl.org/ontology/bibo/Article> .
/*
:AB-205-Helicopter rdf:type owl:Class ;
									 rdfs:subClassOf :Helicopter ;
									 skos:altLabel "AB 205"@en ,
																 "AB-205"@en ;
									 skos:compactLabel "AB-205 helicopter"@en ;
									 skos:definition "License-built version of the U.S. Bell 205 helicopter."@en ;
									 rdfs:isDefinedBy : .
*/
// Parse one UMBEL .n3 file into triplets[predicate][subject] = {objects}.
// Uses a MAX_BUF (2e6 lpchar_t) heap buffer (tmalloc/tfree).  LOG_FATAL if the file is missing.
int readN3FileIntoTripletMap(const lpchar_t* path, unordered_map < lpwstring, unordered_map <lpwstring, set< lpwstring > > >& triplets) // batch B10: const, the directory walk now yields const paths
{
	FILE* fp = lp_wfopen(path, "r");
	if (!fp)
	{
		lplog(LOG_FATAL_ERROR, u"%s file not found.", path);
		return -1;
	}
	lpchar_t* buffer = (lpchar_t*)tmalloc(MAX_BUF * sizeof(lpchar_t));
	if (!buffer)
	{
		lplog(LOG_FATAL_ERROR, u"readN3FileIntoTripletMap: out of memory allocating %d bytes for %s.", MAX_BUF * sizeof(lpchar_t), path);
		fclose(fp);
		return -1;
	}
	int line;
	int nonConformingLines = 0;
	for (line = 1; lp_fgetws(buffer, MAX_BUF, fp); line++)
	{
		if (buffer[0] == '@')
			continue;
		if (buffer[0] == '#')
			continue;
		lp_stripTrailing(buffer, u'\n');
		if (buffer[0] == 0)
			continue;
		for (int I = 0; buffer[I]; I++)
			if (buffer[I] == u'\t')
				buffer[I] = u' ';
		// must be three spaces
		lpchar_t* s1 = lp_strchr(buffer, ' ');
		if (!s1)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		lpchar_t* s2 = lp_strchr(s1 + 1, ' ');
		if (!s2)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		lpchar_t* s3;
		lpwstring mapTo;
		if (*(s2 + 1) == '"')
		{
			if (s2[2] == '"' && s2[3] == '"')
			{
				s3 = readTillEndOfTripleString(s2, fp, buffer, line, mapTo);
				//lplog(LOG_WIKIPEDIA, u"line #%d: triple string detected:(code line %d) \n**mapTo:\n%s\n**s2:\n%s\n**buffer:\n%s\n**END", line, __LINE__, mapTo.c_str(),s2,buffer);
			}
			else
				s3 = readTillEndOfN3String(s2);
		}
		else
			s3 = lp_strchr(s2 + 1, ' ');
		if (!s3)
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		bool inTwoFieldPerLineProperty = false, inOneFieldPerLineProperty = false;
		if (*(s3 + 1) == ';' || s3[lp_strlen(s3) - 1] == ';') // skips inline ,
			inTwoFieldPerLineProperty = true;
		else if (*(s3 + 1) == ',')
			inOneFieldPerLineProperty = true;
		else if (*(s3 + 1) != '.')
		{
			lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: Nonconforming #%d:(code line %d) [%c %d] [%c %d] %s", line, nonConformingLines, __LINE__, *(s2 + 1), s2 - buffer, *(s3 + 1), s3 - buffer, buffer);
			nonConformingLines++;
			continue;
		}
		*s1 = *s2 = *s3 = 0;
		lpwstring mapFrom = (buffer[0] == u':') ? buffer + 1 : buffer;
		lpwstring ontologyRelation = s1 + 1;
		if (mapTo.empty())
			mapTo = s2 + 1;
		triplets[ontologyRelation][mapFrom].insert(mapTo);
		lplog(LOG_WIKIPEDIA, u"UMBEL line #%d: %s %s %s", line, ontologyRelation.c_str(), mapFrom.c_str(), mapTo.c_str());
		if (inTwoFieldPerLineProperty)
			readN3TwoPropertyLine(path, mapFrom, triplets, fp, buffer, line, nonConformingLines);
		if (inOneFieldPerLineProperty)
			readN3OnePropertyLine(ontologyRelation, mapFrom, triplets, fp, buffer, line, nonConformingLines);
	}
	lplog(LOG_WIKIPEDIA, u"relations in %s LIST: Nonconforming lines %d: total lines %d %d%%", path, nonConformingLines, line, nonConformingLines * 100 / line);
	for (unordered_map < lpwstring, unordered_map <lpwstring, set<lpwstring>> >::iterator tbegin = triplets.begin(); tbegin != triplets.end(); tbegin++)
		lplog(LOG_WIKIPEDIA, u"%d:FINAL UMBEL %s:%d", __LINE__, tbegin->first.c_str(), tbegin->second.size());
	tfree(MAX_BUF * sizeof(lpchar_t), buffer);
	fclose(fp);
	return 0;
}

// Recursively import every file under basepath whose suffix equals extension.
// lp_strrchr can be NULL (no '.') and is then passed to lp_strcmp.
void cOntology::importUMBELN3Files(const lpchar_t* basepath, const lpchar_t* extension, unordered_map < lpwstring, unordered_map <lpwstring, set< lpwstring > > >& triplets)
{
	// Batch B10: lpDirectoryEntries replaces the
	// FindFirstFile/FindNextFile/FindClose loop. Entries beginning with '.' are
	// still skipped (the helper already drops "." and ".."; this keeps other
	// dotfiles excluded exactly as before), and directories are distinguished by
	// asking the helper rather than by a dwFileAttributes bit.
	lpwstring base(basepath);
	for (const lpwstring& entry : lpDirectoryEntries(base, u"*"))
	{
		if (entry.empty() || entry[0] == u'.') continue;
		lpwstring completePath = base + u"/" + entry;
		if (lp_wIsDirectory(completePath))
			importUMBELN3Files(completePath.c_str(), extension, triplets);
		else
		{
			const lpchar_t* ext = lp_strrchr(completePath.c_str(), '.');
			if (ext && !lp_strcmp(ext, extension))
				readN3FileIntoTripletMap(completePath.c_str(), triplets);
		}
	}
}

// Import "umbel downloads" *.n3, invert umbel:superClassOf into rdfs:subClassOf,
// and insert UMBEL entries into dbPediaOntologyCategoryList.  fillRanks is
// invoked once per triplets[u""] entry.  Returns true on completion; the sole
// caller (fillOntologyList) does not currently check the result either way.
bool cOntology::readUMBELSuperClasses()
{
	unordered_map < lpwstring, unordered_map <lpwstring, set<lpwstring>> > triplets;
	importUMBELN3Files(u"umbel downloads", u".n3", triplets);
	unordered_map <lpwstring, set<lpwstring>> subClasses = triplets[u"rdfs:subClassOf"];
	for (auto ti : triplets[u"umbel:superClassOf"])
	{
		for (auto iSubClass : ti.second)
			subClasses[iSubClass].insert(ti.first);
	}
	for (auto ti : subClasses)
	{
		lpwstring labelWithSpace, compactLabel;
		int UMBELType;
		stripUmbel(ti.first, compactLabel, labelWithSpace, UMBELType);
		unordered_set <lpwstring> superClasses;
		for (auto iSuperClass : ti.second)
		{
			lpwstring labelWithSpaceSC, compactLabelSC;
			int UMBELTypeSC;
			stripUmbel(iSuperClass, compactLabelSC, labelWithSpaceSC, UMBELTypeSC);
			superClasses.insert(labelWithSpaceSC);
		}
		unordered_map <lpwstring, cOntologyEntry>::iterator cli = dbPediaOntologyCategoryList.find(labelWithSpace);
		if (cli == dbPediaOntologyCategoryList.end())
		{
			dbPediaOntologyCategoryList[labelWithSpace] = cOntologyEntry();
			cli = dbPediaOntologyCategoryList.find(labelWithSpace);
		}
		lpwstring description;
		cli->second.abstractDescription = setString(triplets[u"skos:definition"][compactLabel], description, u" ");
		cli->second.superClasses.insert(superClasses.begin(), superClasses.end());
		cli->second.compactLabel = compactLabel;
		cli->second.ontologyType = UMBEL_Ontology_Type;
		cli->second.resourceType = UMBELType;
	}
	for (auto ti : triplets[u""])
		fillRanks(UMBEL_Ontology_Type);
	return true;
}

/***
	Read N3 files (END)
***/

// cut off YAGO and UMBEL category numbers
// Strip a trailing run of digits (YAGO WordNet-style ids: HealthProfessional110165109).
void cOntology::cutFinalDigits(lpwstring& cat)
{
	LFS
		int I;
	if (cat.length() > 0 && iswdigit(cat[cat.length() - 1]))
	{
		for (I = cat.length() - 1; I >= 0 && iswdigit(cat[I]); I--);
		if (I > 0 && iswdigit(cat[I + 1]))
			cat = cat.substr(0, I + 1);
	}
}

/*
DBPEDIA START
*/
// decode dbpedia URL for calling HTTP API into virtuoso
// Percent-decode into decodedURL (+ -> space).  
lpwstring cOntology::decodeURL(lpwstring input, lpwstring& decodedURL)
{
	LFS
	decodedURL.clear();
	for (int I = 0; input[I]; I++)
		if (input[I] == u'%' && input[I + 1] && input[I + 2])
		{
			int ch = 0;
			lpchar_t h1 = towupper(input[I + 1]), h2 = towupper(input[I + 2]);
			ch += h1 - ((iswalpha(h1)) ? u'A' - 10 : u'0');
			ch <<= 4;
			ch += h2 - ((iswalpha(h2)) ? u'A' - 10 : u'0');
			decodedURL += ch;
			I += 2;
		}
		else if (input[I] == u'+')
			decodedURL += u' ';
		else
			decodedURL += input[I];
	return decodedURL;
}

// Fetch/cache a SPARQL/HTTP result under dbPediaCache.  Sanitizes epath.
// Returns cInternet::getWebPath's code, or -1 if the body looks like a SPARQL error
// (cached file is then deleted).  where is the caller source-index for logging.
int cOntology::getDBPediaPath(int where, lpwstring webAddress, lpwstring& buffer, lpwstring epath)
{
	LFS
		//int timer=clock(); 	
		int bw = -1;
	while ((bw = epath.find_first_of(u"/*?\"<>|,&-")) != lpwstring::npos)
		epath[bw] = u'_';
	lpwstring filePathOut, headers;
	int retValue = cInternet::getWebPath(where, webAddress, buffer, epath, u"dbPediaCache", filePathOut, headers, 0, false, true, forceWebReread);
	if (buffer.find(u"SPARQL compiler") != lpwstring::npos)
	{
		lp_wremove(filePathOut.c_str());
		lplog(LOG_ERROR, u"PATH %s:\n%s", epath.c_str(), buffer.c_str());
		return -1;
	}
	return retValue;
}

// Lower-case icat, strip trailing digits, look up in dbPediaOntologyCategoryList.
unordered_map <lpwstring, cOntologyEntry>::iterator cOntology::findCategory(lpwstring& icat)
{
	LFS
		// put into lower case
		lpwstring cat = icat;
	transform(cat.begin(), cat.end(), cat.begin(), (int(*)(int)) tolower);
	// and remove trailing numbers (important for YAGO)
	// digits must be continuous and from the end
	cutFinalDigits(cat);
	return dbPediaOntologyCategoryList.find(cat);
}

// escape quote in object
// If object contains ' or %27, wrap the corresponding SPARQL token in quotes
// so Virtuoso will accept it.  begin is the query prefix used to locate the token.
void 	adjustQuote(lpwstring& begin, lpwstring& object, lpwstring& webAddress)
{
	LFS
		if (object.find(u"'") == lpwstring::npos && object.find(u"%27") == lpwstring::npos) return;
	size_t firstSpace = webAddress.find_last_of(u' ', begin.length());
	if (firstSpace == lpwstring::npos)
	{
		firstSpace = webAddress.find_last_of(u'+', begin.length());
		int colon = webAddress.rfind(u"%3A", begin.length());
		if (colon > firstSpace)
			firstSpace = colon;
	}
	size_t secondSpace = webAddress.find_first_of(u' ', begin.length() + object.length());
	if (secondSpace == lpwstring::npos)
		secondSpace = webAddress.find_first_of(u'+', begin.length() + object.length());
	// find previous space
	if (firstSpace == lpwstring::npos || secondSpace == lpwstring::npos) return;
	webAddress.insert(firstSpace, u"\"");
	webAddress.insert(secondSpace + 1, u"\"");
}

/*
0-31	ASCII Control Characters	These characters are not printable	Unsafe
32-47	Reserved Characters	' '!?#$%&'()*+,-./	                      Unsafe
48-57	ASCII Characters and Numbers	0-9															Safe
58-64	Reserved Characters	:;<=>?@	                                  Unsafe
65-90	ASCII Characters	A-Z																					Safe
91-96	Reserved Characters	[\]^_`	                                  Unsafe
97-122	ASCII Characters	a-z																				Safe
123-126	Reserved Characters	{|}~	                                  Unsafe
127	Control Characters	 ' '	                                      Unsafe
128-255	Non-ASCII Characters	 ' '	                                Unsafe
*/
// Percent-encode every non-alnum byte of the UTF-8 conversion of winput.
void encodeURL(lpwstring winput, lpwstring& wencodedURL)
{
	LFS
		string input;
	wTM(winput, input);
	string encodedURL;
	for (unsigned int I = 0; I < input.length(); I++)
	{
		if (!isalnum((unsigned char)input[I]))
		{
			encodedURL += '%';
			int lower = ((unsigned char)input[I]) & 15;
			int higher = ((unsigned char)input[I]) >> 4;
			encodedURL += (higher < 10) ? '0' + higher : 'A' + (higher - 10);
			encodedURL += (lower < 10) ? '0' + lower : 'A' + (lower - 10);
		}
		else
			encodedURL += input[I];
	}
	mTW(encodedURL, wencodedURL);
}

// SPARQL for abstract/comment/homepage/birthDate/birthPlace/occupation of label.
lpwstring getDescriptionString(lpwstring label)
{
	return basehttpquery + prefix_foaf + prefix_colon +
		u"SELECT+DISTINCT+%3Fv1+%3Fv2+%3Fv3+%3Fv4+%3Fbd+%3Fbp+%3Focc+%0D%0AWHERE+%7B%0D%0A+"
		u"+%7B+" + label + u"+%3Chttp%3A%2F%2Fwww.w3.org%2F2002%2F07%2Fowl%23sameAs%3E+%3Fs+.+%7D%0D%0A" // OPTIONAL removed
		u"OPTIONAL+%7B+%3Fs+%3Chttp%3A%2F%2Fwikidata.dbpedia.org%2Fontology%2Fabstract%3E+%3Fv1+.+%7D%0D%0A"
		u"OPTIONAL+%7B+%3Fs+%3Chttp%3A%2F%2Fwww.w3.org%2F2000%2F01%2Frdf-schema%23comment%3E+%3Fv2+.+%7D%0D%0A"
		u"OPTIONAL+%7B+" + label + u"+foaf%3Ahomepage+%3Fv3+.+%7D%0D%0A"
		//u"OPTIONAL+%7B+" + label + u"+foaf%3Apage+%3Fv4+.+%7D%0D%0A"
		u"OPTIONAL+%7B+" + label + u"+%3Chttp%3A%2F%2Fdbpedia.org%2Fontology%2FbirthDate%3E+%3Fbd+.+%7D%0D%0A"
		u"OPTIONAL+%7B+" + label + u"+%3Chttp%3A%2F%2Fdbpedia.org%2Fontology%2FbirthPlace%3E+%3Fbp+.+%7D%0D%0A"
		u"OPTIONAL+%7B+" + label + u"+%3Chttp%3A%2F%2Fdbpedia.org%2Fontology%2Foccupation%3E+%3Focc+.+%7D%0D%0A%7D";
}

// If link is a dbpedia.org URI, fetch the page and scrape property's <span> text
// into value.  Non-dbpedia links are copied through.  Returns 0 or -1.
int cOntology::followDbpediaLink(lpwstring link, lpwstring property, lpwstring& value)
{
	if (lp_strncmp(link.c_str(), u"http://dbpedia.org", lp_strlen(u"http://dbpedia.org")))
	{
		value = link;
		return 0;
	}
	lpwstring buffer;
	if (!cInternet::readPage(link.c_str(), buffer))
	{
		size_t pos2 = 0;
		if (firstMatch(buffer, property, u"</span>", pos2, value, false) == lpwstring::npos)
		{
			lplog(LOG_WHERE | LOG_ERROR, u"Unable to find property %s in dbPediaLink buffer resulting from link %s.", property.c_str(), link.c_str());
			return -1;
		}
		if (logRDFDetail)
			lplog(LOG_WHERE, u"Found value %s from property %s in dbPediaLink buffer resulting from link %s.", value.c_str(), property.c_str(), link.c_str());
		return 0;
	}
	value = u"Unable to follow link:" + link;
	return -1;
}

// these queries are not combined to look up properties of more than one object because it would go over the time limit imposed by the VIRTUOSO server.
// when running SPARQL queries in Virtuoso Conductor ISQL, you must prepend the query with SPARQL, so it must go before even the PREFIX (and not anywhere else)
// SPARQL getDescriptionString(label) and scrape v1/v2/v3/bd/bp/occ bindings.
// birthPlace/occupation URIs are followed via followDbpediaLink.  Returns the
// number of <binding name="v1"> rows (0 if the HTTP fetch failed).
int cOntology::getDescription(lpwstring label, lpwstring objectName, lpwstring& abstract, lpwstring& comment, lpwstring& infoPage, lpwstring& birthDate, lpwstring& birthPlace, lpwstring& occupation)
{
	LFS
		// cl=http://dbpedia.org/class/yago/HealthProfessional110165109
		replace(label.begin(), label.end(), u' ', u'_');
	// replace % with %25:
	lpwstring rlabel;
	for (unsigned int I = 0; I < label.length(); I++)
		if (I + 2 < label.length() && label[I] == u'%' && label[I + 1] == u'2' && (label[I + 2] == u'8' || label[I + 2] == u'9'))
			rlabel += u"%25";
		else
			rlabel += label[I];
	label = rlabel;
	if (label.find_first_of(u"'#()") != lpwstring::npos || label.find(u"%") != lpwstring::npos)
	{
		label = u"<http://dbpedia.org/resource/" + label + u">";
	}
	else
	{
		label = u"%3A" + label;
	}
	lpwstring dbPediaQueryString = getDescriptionString(label), temp, buffer;
	if (logRDFDetail)
		lplog(LOG_WIKIPEDIA | LOG_RESOLUTION, u"%s\nENCODED WEBADDRESS:%s\nDECODED WEBADDRESS:%s",
			objectName.c_str(), dbPediaQueryString.c_str(), decodeURL(dbPediaQueryString, temp).c_str() + decodedbasehttpquery.length());
	int numRows = 0;
	if (!cInternet::readPage(dbPediaQueryString.c_str(), buffer))
	{
		// get number of rows
		for (size_t w = 0; w < buffer.size(); numRows++, w++)
			if ((w = buffer.find(u"<binding name=\"v1\">", w)) == lpwstring::npos)
				break;
		if (numRows > 0)
		{
			size_t pos = 0, pos2 = 0;
			lpwstring tmpstr;
			if (firstMatch(buffer, u"<binding name=\"v1\">", u"</binding>", pos, tmpstr, false) != lpwstring::npos)
				firstMatch(tmpstr, u"<literal xml:lang=\"en\">", u"</literal>", pos2, abstract, false);
			if (firstMatch(buffer, u"<binding name=\"v2\">", u"</binding>", pos, tmpstr, false) != lpwstring::npos)
			{
				pos2 = 0;
				firstMatch(tmpstr, u"<literal xml:lang=\"en\">", u"</literal>", pos2, comment, false);
			}
			if (firstMatch(buffer, u"<binding name=\"v3\">", u"</binding>", pos, tmpstr, false) != lpwstring::npos)
			{
				pos2 = 0;
				firstMatch(tmpstr, u"<uri>", u"</uri>", pos2, infoPage, false);
			}
			if (firstMatch(buffer, u"<binding name=\"bd\">", u"</binding>", pos, tmpstr, false) != lpwstring::npos)
			{
				pos2 = 0;
				firstMatch(tmpstr, u">", u"</literal>", pos2, birthDate, false);
			}
			if (firstMatch(buffer, u"<binding name=\"bp\">", u"</binding>", pos, tmpstr, false) != lpwstring::npos)
			{
				pos2 = 0;
				lpwstring birthPlaceLink;
				firstMatch(tmpstr, u"<uri>", u"</uri>", pos2, birthPlaceLink, false);
				followDbpediaLink(birthPlaceLink, u"<span property=\"rdfs:label\" xmlns:rdfs=\"http://www.w3.org/2000/01/rdf-schema#\" xml:lang=\"en\">", birthPlace); // if birthPlace is a reference - http://dbpedia.org/resource/Albany,_New_York
			}
			if (firstMatch(buffer, u"<binding name=\"occ\">", u"</binding>", pos, tmpstr, false) != lpwstring::npos)
			{
				pos2 = 0;
				lpwstring occupationLink;
				firstMatch(tmpstr, u"<uri>", u"</uri>", pos2, occupationLink, false);
				followDbpediaLink(occupationLink, u"<span property=\"dbo:title\" xmlns:dbo=\"http://dbpedia.org/ontology/\" xml:lang=\"en\">", occupation); // if occupation is a reference - http://dbpedia.org/resource/Darrell_Hammond__1
			}
		}
	}
	return numRows;
}

// get abstract, comment and wikipedia page, all as optional (none are required to appear)
//int cOntology::getDescription(unordered_map <lpwstring, cOntologyEntry>::iterator cli)
//{ LFS
//	if (cli->second.descriptionFilled>=0) return cli->second.descriptionFilled;
//	int numRows=getDescription(cli->second.compactLabel,cli->first,cli->second.abstractDescription,cli->second.commentDescription,cli->second.infoPage, cli->second.birthDate, cli->second.birthPlace, cli->second.occupation);
//	return cli->second.descriptionFilled=numRows;
//}

// SPARQL rdfs:subClassOf for a YAGO class not in the in-memory map; insert it
// (rank 100) and recurse for unknown supers.  Returns the new iterator, or end()
// if the fetch fails.  URI prefix strip assumes the http://dbpedia.org/class/yago/ prefix.
unordered_map <lpwstring, cOntologyEntry>::iterator cOntology::findAnyYAGOSuperClass(lpwstring cl)
{
	LFS
		// cl=http://dbpedia.org/class/yago/HealthProfessional110165109
		lpwstring begin = basehttpquery + u"PREFIX+rdfs%3A+%3Chttp%3A%2F%2Fwww.w3.org%2F2000%2F01%2Frdf-schema%23%3E%0D%0ASELECT+%3Fv%0D%0AWHERE+%7B%0D%0A++%7B+%3Chttp%3A%2F%2Fdbpedia.org%2Fclass%2Fyago%2F";
	lpwstring end = u"%3E+rdfs%3AsubClassOf+%3Fv+%7D%0D%0A%7D";
	lpwstring buffer;
	if (!getDBPediaPath(0, begin + cl + end, buffer, cl + u"_findSuperClass.xml"))
	{
		lpwstring cat = cl, uri;
		transform(cat.begin(), cat.end(), cat.begin(), (int(*)(int)) tolower);
		cutFinalDigits(cat);
		dbPediaOntologyCategoryList[cat].compactLabel = cl;
		unordered_map <lpwstring, cOntologyEntry>::iterator cli = dbPediaOntologyCategoryList.find(cat);
		cli->second.ontologyType = YAGO_Ontology_Type;
		cli->second.ontologyHierarchicalRank = 100;
		// <uri>http://dbpedia.org/class/yago/Sailor110546633</uri>
		for (size_t pos = 0; firstMatch(buffer, u"<uri>", u"</uri>", pos, uri, false) != lpwstring::npos; )
		{
			lpwstring scat = uri.c_str() + lp_strlen(u"http://dbpedia.org/class/yago/");
			cli->second.superClasses.insert(scat);
			lplog(LOG_WIKIPEDIA, u"%s:%s:%s:%s", u"YSC", uri.c_str(), cl.c_str(), scat.c_str());
			if (findCategory(scat) == dbPediaOntologyCategoryList.end())
				findAnyYAGOSuperClass(scat);
		}
		return cli;
	}
	return dbPediaOntologyCategoryList.end();
}

// c. derive a combined ranking of a hierarchy from dbPedia, UMBEL, YAGO, OpenGIS
// Repeatedly assign ontologyHierarchicalRank = super.rank+1 (or 0 if no/unknown
// super) for entries of ontologyType still at 100.  Loops until a pass fills none.
int cOntology::fillRanks(int ontologyType)
{
	LFS
		// this assumes that a class has one or zero super classes.  These superclasses may or may not exist as labels.
		int numLoops = 0, noSuperClasses, notFoundSuperClasses, ranked, numEntries, newPercent, oldPercent;
	lpwstring tmpstr;
	lp_wprintf(u"\n");
	for (int entriesFilled = 1; entriesFilled > 0; numLoops++)
	{
		entriesFilled = noSuperClasses = notFoundSuperClasses = ranked = numEntries = 0, newPercent = 0, oldPercent = -1;
		for (unordered_map <lpwstring, cOntologyEntry>::iterator cli = dbPediaOntologyCategoryList.begin(), clEnd = dbPediaOntologyCategoryList.end(); cli != clEnd; cli++, numEntries++)
		{
			newPercent = numEntries * 100 / dbPediaOntologyCategoryList.size();
			if (oldPercent != newPercent)
			{
				lp_wprintf(u"%03d (%06d/%06zd) ontology items ranked\r", newPercent, numEntries, dbPediaOntologyCategoryList.size());
				oldPercent = newPercent;
			}
			if (cli->second.ontologyHierarchicalRank == 100 && cli->second.ontologyType == ontologyType)
			{
				if (cli->second.superClasses.empty())
				{
					cli->second.ontologyHierarchicalRank = 0;
					entriesFilled++;
					noSuperClasses++;
				}
				else
				{
					unordered_map <lpwstring, cOntologyEntry>::iterator scli;
					for (auto sci : cli->second.superClasses)
					{
						scli = dbPediaOntologyCategoryList.find(sci);
						if (scli == dbPediaOntologyCategoryList.end())
							scli = findCategory(sci);
						if (scli == dbPediaOntologyCategoryList.end() && (sci != u"orphans" || cli->second.ontologyType != UMBEL_Ontology_Type))
							lplog(LOG_WIKIPEDIA, u"fillRanks: looking for superclass %s of object %s failed.", sci.c_str(), cli->second.toString(tmpstr, cli->first).c_str());
						else
							break;
						//if (scli != dbPediaOntologyCategoryList.end() && (scli = findAnyYAGOSuperClass(scli->second.compactLabel)) != dbPediaOntologyCategoryList.end())
						//	break;
					}
					if (scli == dbPediaOntologyCategoryList.end())
					{
						cli->second.ontologyHierarchicalRank = 0;
						entriesFilled++;
						notFoundSuperClasses++;
					}
					else if (scli->second.ontologyHierarchicalRank != 100)
					{
						cli->second.ontologyHierarchicalRank = scli->second.ontologyHierarchicalRank + 1;
						entriesFilled++;
						ranked++;
					}
				}
			}
		}
		lp_wprintf(u"%03d: total %07d:%07d noSuperClasses %06d notFoundSuperClasses %06d ranked\n", numLoops, entriesFilled, noSuperClasses, notFoundSuperClasses, ranked);
	}
	return 0;
}

// UMBEL NS file MUST BE IN UTF16 - resave using a conversion utility
// Read the UMBEL NS file downloaded from the website and write another file which just lists the superClasses for each class.
//  This is because downloading the UMBEL resource doesn't work for both the WinHTTP and INET methods because of a timeout/redirect problem.
// Sample:
//###  http://umbel.org/umbel/rc/Artist_Performer
//
//<http://umbel.org/umbel/rc/Artist_Performer> rdf:type owl:Class ;
//                                             
//                                             rdfs:subClassOf :PersonTypes ,
//                                                             <http://umbel.org/umbel/rc/Artist> ,
//                                                             <http://umbel.org/umbel/rc/Entertainer> ;


/*
labelWithSpace   <http://umbel.org/umbel/rc/WeatherAttributes_Weather_Topic> rdf:type owl:Class ;
superClasses		 rdfs:subClassOf :TopicsCategories ,
									<http://umbel.org/umbel/rc/Weather_Topic> ;
									skos:definition "A CycVocabularyTopic and a KBDependentCollection."@en ;
compactLabel			skos:compactLabel "weather attributes weather topic"@en .
*/

// Peel a known URI prefix (UMBELType = prefix index+1), camelCase -> spaced
// lower-case labelWithSpace.  compactLabel is the local name.  Returns labelWithSpace.
lpwstring cOntology::stripUmbel(lpwstring umbelClass, lpwstring& compactLabel, lpwstring& labelWithSpace, int& UMBELType)
{
	const lpchar_t* prefixes[] = { u"<http://umbel.org/umbel/rc/", u"<http://umbel.org/umbel#", u"<http://schema.org/", u"<http://www.geonames.org/ontology#", u"<http://dbpedia.org/ontology/",
		u"<http://purl.org/dc/dcmitype/",		u"<http://purl.org/dc/terms/",		u"<http://purl.org/goodrelations/v1#",		u"<http://purl.org/openorg/",
		u"<http://usefulinc.com/ns/doap#",		u"<http://vocab.org/transit/terms/",		u"<http://www.w3.org/2003/01/geo/wgs84_pos#",		u"<http://www.w3.org/2004/02/skos/core#",
		u"<http://www.w3.org/2006/time#",		u"<http://www.w3.org/2006/timezone#",		u"<http://www.w3.org/ns/org#",		u"<http://xmlns.com/foaf/0.1/",
		 0 };

	for (int pf = 0; prefixes[pf] != 0; pf++)
	{
		int w = umbelClass.find(prefixes[pf]);
		if (w >= 0)
		{
			w += lp_strlen(prefixes[pf]);
			int w2 = umbelClass.find(u">", w + 1);
			if (w2 >= 0)
			{
				compactLabel = umbelClass.substr(w, w2 - w);
				UMBELType = pf + 1;
				break;
			}
		}
	}
	if (compactLabel.empty())
	{
		compactLabel = umbelClass;
	}
	if (compactLabel[0] == ':')
		compactLabel = compactLabel.substr(1);
	lpwstring temp2;
	temp2 = compactLabel;
	// change _ to space
	std::replace(temp2.begin(), temp2.end(), u'_', u' ');
	labelWithSpace.clear();
	lpchar_t pc = 0;
	// insert space before capitals other than the first one, if there isn't already one
	for (lpwstring::iterator wi = temp2.begin(), wiEnd = temp2.end(); wi != wiEnd; wi++)
	{
		lpwstring::iterator next = wi + 1;
		// insert a space before all but the first capital, as long as there isn't already a space and the next character is not a capital (ACE Inhibitor)
		if (iswupper(*wi) && pc != 0 && iswalnum(pc) && pc != u' ' && (next == wiEnd || !iswupper(*next)))
			labelWithSpace += u' ';
		pc = tolower(*wi);
		labelWithSpace += pc;
	}
	return labelWithSpace;
}


// In-place lower-case ic, insert a space before a new capital/digit run, and
// drop a trailing digit run.  Mutates the caller's buffer.
void transform(lpchar_t* ic, lpwstring& nameIC)
{
	int lastContinuousDigit = 0;
	lpchar_t* original = ic;
	for (; *ic; ic++)
	{
		if (ic != original && (iswupper(*ic) && !iswupper(ic[-1])) || (iswdigit(*ic) && !iswdigit(ic[-1])))
			nameIC += u" ";
		*ic = towlower(*ic);
		if (iswdigit(*ic))
		{
			if (!lastContinuousDigit)
				lastContinuousDigit = nameIC.length() - 1;
		}
		else
			lastContinuousDigit = 0;
		nameIC += *ic;
	}
	if (lastContinuousDigit > 0)
		nameIC.erase(lastContinuousDigit);
}

// Narrow-char twin of transform(lpchar_t*).
void transform(char* ic, string& nameIC)
{
	int lastContinuousDigit = 0;
	char* original = ic;
	for (; *ic; ic++)
	{
		if (ic != original && (isupper(*ic) && !isupper(ic[-1])) || (isdigit(*ic) && !isdigit(ic[-1])))
			nameIC += " ";
		*ic = tolower(*ic);
		if (isdigit(*ic))
		{
			if (!lastContinuousDigit)
				lastContinuousDigit = nameIC.length() - 1;
		}
		else
			lastContinuousDigit = 0;
		nameIC += *ic;
	}
	if (lastContinuousDigit > 0)
		nameIC.erase(lastContinuousDigit);
}

// Load yago_taxonomy.ttl then yago_type_links.ttl and fillRanks(YAGO).  Returns -1
// if either file is missing (the per-file reader already LOG_FATALs).
int cOntology::readYAGOOntology()
{
	LFS
		int numYAGOEntries = 0, numSuperClasses = 0;
	if (readYAGOOntology(u"source\\lists\\yago_taxonomy.ttl", numYAGOEntries, numSuperClasses) < 0 || readYAGOOntology(u"source\\lists\\yago_type_links.ttl", numYAGOEntries, numSuperClasses) < 0)
		return -1;
	int emptyYAGOSuperClasses = 0;
	for (unordered_map <lpwstring, cOntologyEntry>::iterator cli = dbPediaOntologyCategoryList.begin(), clEnd = dbPediaOntologyCategoryList.end(); cli != clEnd; cli++)
		if (cli->second.ontologyType == YAGO_Ontology_Type && cli->second.compactLabel.empty())
			emptyYAGOSuperClasses++;
	lplog(LOG_WIKIPEDIA, u"YAGO ontology inserted %d entries (%d emptyYAGOSuperClasses).", numYAGOEntries, emptyYAGOSuperClasses);
	fillRanks(YAGO_Ontology_Type);
	return 0;
}

#define MAXYAGOBUF 5000000 // in char
// Stream one YAGO TTL: equivalentClass lines set compactLabel; subClassOf lines
// insert a super.  5MB char fileBuffer is heap-allocated (tmalloc/tfree).
int cOntology::readYAGOOntology(const lpchar_t* filepath, int& numYAGOEntries, int& numSuperClasses)
{
	LFS
		int fd = lp_wopen(filepath, O_RDONLY); // batch B10: POSIX open replaces CreateFile
	lpwstring tmpstr, tmpstr2, tmpstr3;
	if (fd < 0)
	{
		lplog(LOG_FATAL_ERROR, u"YAGOOntology file %s not found.", filepath);
		return -1;
	}
	int line, bufferOffset = 0, bufferLength = 0; // fileOffset=0,
	int64_t totalFileLength, totalFileOffset = 0;
	struct stat yagoStatus;
	if (fstat(fd, &yagoStatus) != 0)
	{
		::close(fd);
		return -1;
	}
	totalFileLength = yagoStatus.st_size;
	char* fileBuffer = (char*)tmalloc(MAXYAGOBUF + 1);
	if (!fileBuffer)
	{
		lplog(LOG_FATAL_ERROR, u"readYAGOOntology: out of memory allocating %d bytes for %s.", MAXYAGOBUF + 1, filepath);
		::close(fd);
		return -1;
	}
	/* Read a line at a time until eof */
	// <http://dbpedia.org/class/yago/Aa114931472> <http://www.w3.org/2000/01/rdf-schema#compactLabel> "Aa"@en .  obsolete! yago_links.nt deprecated in dbpedia 3.7
	// <http://dbpedia.org/class/yago/Canal102947212> <http://www.w3.org/2002/07/owl#equivalentClass> <http://yago-knowledge.org/resource/wordnet_canal_102947212> . in yago_type_links.ttl
	// <http://dbpedia.org/class/yago/Aa114931472> <http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://dbpedia.org/class/yago/Lava114930989>. in yago_taxonomy.ttl
	int oldPercent = -1, newPercent = 0;
	for (line = 1; bufferOffset != bufferLength || bufferLength == 0; line++)
	{
		if (bufferOffset > bufferLength - maxCategoryLength)
		{
			if (bufferLength - bufferOffset)
				memcpy(fileBuffer, fileBuffer + bufferOffset, (int)(bufferLength - bufferOffset) * sizeof(char));
			bufferLength = bufferLength - bufferOffset;
			ssize_t readResult = ::read(fd, fileBuffer + bufferLength, (size_t)(MAXYAGOBUF - bufferLength) * sizeof(char));
			if (readResult < 0)
			{
				lplog(LOG_ERROR, u"%S:%d:%s", __func__, __LINE__, lastErrorMsg().c_str());
				readResult = 0;
			}
			unsigned int newBufferLength = (unsigned int)readResult;
			totalFileOffset += newBufferLength;
			newBufferLength /= sizeof(char);
			fileBuffer[bufferLength = newBufferLength + bufferLength] = 0;
			bufferOffset = 0;
			newPercent = (int)(totalFileOffset * 100 / totalFileLength);
			if (newPercent != oldPercent)
				lp_wprintf(u"%03d:YAGO Ontology %07d entries, %06d superclasses out of %08d lines\r", newPercent, numYAGOEntries, numSuperClasses, line);
			oldPercent = newPercent;
		}
		cProfile::counterBegin();
		char* s, * newLine = strchr(s = fileBuffer + bufferOffset, u'\n');
		if (newLine)
		{
			*newLine = 0;
			bufferOffset = (int)(newLine - fileBuffer + 1);
		}
		else
			bufferOffset = bufferLength;
		if (strncmp(s, "<http://dbpedia.org/class/yago/", strlen("<http://dbpedia.org/class/yago/")))
			continue;
		char* name = NULL;
		if ((name = firstMatch(s, "<http://dbpedia.org/class/yago/", ">")) == NULL)
		{
			//lplog(LOG_ERROR,u"Error parsing (1) dbPediaOntology category [no labelWithSpace] on line %d: %s",line,s);
			continue;
		}
		char* ic = name + strlen(name) + 2, * label, * superClass;
		int resourceType = 0;
		// wikicat people from tambov oblast
		if (strncmp(name, "wikicat ", strlen("wikicat ")) == 0)
		{
			name += strlen("wikicat ");
			resourceType = 1;
		}
		// example: <http://yago-knowledge.org/resource/wordnet_carousel_102966193>
		if ((label = firstMatch(ic, "<http://www.w3.org/2002/07/owl#equivalentClass> <http://yago-knowledge.org/resource/", ">")) != NULL) // yago_type_links.ttl
		{
			// cut until the first _
			char* firstUnderscore = strchr(label, '_');
			if (firstUnderscore)
				label = firstUnderscore + 1;
			mTW(name, tmpstr);
			dbPediaOntologyCategoryList[tmpstr].compactLabel = mTW(label, tmpstr2);
			unordered_map <lpwstring, cOntologyEntry>::iterator cli = dbPediaOntologyCategoryList.find(tmpstr);
			cli->second.numLine = line;
			cli->second.ontologyType = YAGO_Ontology_Type;
			cli->second.resourceType = resourceType;
			numYAGOEntries++;
			continue;
		}
		if ((superClass = firstMatch(ic, "<http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://dbpedia.org/class/yago/", ">")) != NULL)
		{
			mTW(name, tmpstr);
			dbPediaOntologyCategoryList[tmpstr].superClasses.insert(mTW(superClass, tmpstr3));
			unordered_map <lpwstring, cOntologyEntry>::iterator cli = dbPediaOntologyCategoryList.find(tmpstr);
			cli->second.numLine = line;
			cli->second.ontologyType = YAGO_Ontology_Type;
			numSuperClasses++;
			continue;
		}
		//lplog(LOG_ERROR,u"Error parsing (2) dbPediaOntology category [compactLabel and subclass not found] on line %d: %s",line,s);
	}
	tfree(MAXYAGOBUF + 1, fileBuffer);
	::close(fd);
	return 0;
}

// downloaded from https://wiki.dbpedia.org/develop/datasets/downloads-2016-10#dbpedia-ontology
// <http://dbpedia.org/ontology/BasketballLeague> <http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://dbpedia.org/ontology/SportsLeague> .
// <http://dbpedia.org/ontology/TimePeriod>       <http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://www.w3.org/2002/07/owl#Thing> .
// <http://dbpedia.org/ontology/BasketballLeague> <http://www.w3.org/2000/01/rdf-schema#compactLabel> "basketball league"@en .
// <http://dbpedia.org/ontology/TimePeriod>       <http://www.w3.org/2000/01/rdf-schema#compactLabel> "time period"@en .
// <http://dbpedia.org/ontology/BasketballLeague> <http://www.w3.org/2000/01/rdf-schema#comment> "a group of sports teams that compete against each other in Basketball"@en .

//SPARQL PREFIX : < http ://dbpedia.org/ontology/>
//	SELECT DISTINCT ? entry ? l ? sc ? comment
//	WHERE{
//		{ ? entry < http ://www.w3.org/2000/01/rdf-schema#compactLabel> ?l . }
//		{ ? entry < http ://www.w3.org/2000/01/rdf-schema#subClassOf> ?sc . }
//		{ ? entry < http ://www.w3.org/2000/01/rdf-schema#comment> ?comment . }
//	FILTER(lang(? comment) = 'en')
//	FILTER(lang(? l) = 'en')
//	}
#define DBP_SCO u"<http://www.w3.org/2000/01/rdf-schema#subClassOf>"
#define DBP_LABEL u"<http://www.w3.org/2000/01/rdf-schema#label>"
#define DBP_COMMENT u"<http://www.w3.org/2000/01/rdf-schema#comment>"
#define DBP_PREFIX u"<http://dbpedia.org/ontology/"
#define DBP_PREFIX2 u"<http://www.w3.org/2002/07/owl#"
#define DBP_PREFIX3 u"<http://schema.org/"
#define DBP_PREFIX4 u"<http://www.ontologydesignpatterns.org/ont/dul/DUL.owl#"
// Parse dbpedia_downloads\2016-10\dbpedia_2016-10.nt for label/comment/subClassOf,
// keying the map by the English label (not the CamelCase local name) so
// fillRanks and text lookup share a key.  Seeds a few dropped supers (Thing, ...).
int cOntology::readDbPediaOntology()
{
	LFS
		unordered_map <int, lpwstring> lastRank;
	unordered_map <lpwstring, lpwstring> labelMap;
	FILE* fp = lp_wfopen(u"dbpedia_downloads\\2016-10\\dbpedia_2016-10.nt", "rt");
	if (!fp)
	{
		lplog(LOG_FATAL_ERROR, u"dbPedia Ontology file not found.");
		return -1;
	}
	lpchar_t s[maxCategoryLength];
	int line, beginDbPediaEntries = dbPediaOntologyCategoryList.size();
	cOntologyEntry dbp;
	dbp.ontologyType = dbPedia_Ontology_Type;
	dbp.ontologyHierarchicalRank = 1;
	dbp.numLine = 0;
	lpwstring currentEntry;
	// these super classes have been inexplicably dropped from the dbpedia ontology file
	const lpchar_t* droppedSuperClassesEntry[] = { u"Thing",u"Festival",u"MusicGroup",u"SocialPerson",u"Organization",u"Product",0 };
	const lpchar_t* droppedSuperClasses[] = { u"thing",u"festival",u"music group",u"social person",u"organization",u"product",0 };
	for (int I = 0; droppedSuperClasses[I]; I++)
	{
		dbp.compactLabel = droppedSuperClasses[I];
		labelMap[droppedSuperClassesEntry[I]] = droppedSuperClasses[I];
		dbPediaOntologyCategoryList[droppedSuperClasses[I]] = dbp;
	}
	/* Read a line at a time until eof */
	for (line = 1; lp_fgetws(s, maxCategoryLength, fp); line++)
	{
		int eol = lp_strlen(s);
		if (s[eol - 1] == '\n') s[eol - 1] = 0;
		if (s[eol - 2] == '\r') s[eol - 2] = 0;
		if (s[0] == 0xFEFF)
		{// detect BOM
			// Copy the NUL too, so the shift re-terminates by itself. The old form
			// copied strlen(s+1) units and then did s[lp_strlen(s) - 1] = 0 to put
			// the terminator back, which indexed s[-1] on an empty line. This is
			// what the other four BOM strips in the tree already do.
			memmove(s, s + 1, (lp_strlen(s + 1) + 1) * sizeof(*s));
		}
		// get entry
		lpchar_t* space = lp_strstr(s, u"> ");
		if (!space || lp_strncmp(s, DBP_PREFIX, lp_strlen(DBP_PREFIX)))
			continue;
		*space = 0;
		lpwstring entry = s + lp_strlen(DBP_PREFIX);
		if (entry != currentEntry)
		{
			if (currentEntry.length() > 0)
			{
				if (dbp.compactLabel.length() > 0)
				{
					lpwstring label = dbp.compactLabel;
					dbp.compactLabel = currentEntry;
					labelMap[currentEntry] = label;
					dbp.numLine = line;
					dbPediaOntologyCategoryList[label] = dbp;
				}
				else
					lplog(LOG_WIKIPEDIA, u"entry %s dropped.", currentEntry.c_str());
				dbp.superClasses.clear();
				dbp.compactLabel = u"";
			}
			currentEntry = entry;
		}
		// <http://dbpedia.org/ontology/BasketballLeague> <http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://dbpedia.org/ontology/SportsLeague> .
		lpchar_t* pf, * lt, * eq, * nextField = space + 2;
		if (lp_strstr(nextField, DBP_SCO))
		{
			if ((pf = lp_strstr(nextField, DBP_PREFIX)) && (lt = lp_strchr(pf + lp_strlen(DBP_PREFIX), u'>')))
			{
				*lt = 0;
				dbp.superClasses.insert(nextField + lp_strlen(DBP_SCO) + 1 + lp_strlen(DBP_PREFIX));
			}
			else if ((pf = lp_strstr(nextField, DBP_PREFIX2)) && (lt = lp_strchr(pf + lp_strlen(DBP_PREFIX2), u'>')))
			{
				*lt = 0;
				dbp.superClasses.insert(nextField + lp_strlen(DBP_SCO) + 1 + lp_strlen(DBP_PREFIX2));
			}
			else if ((pf = lp_strstr(nextField, DBP_PREFIX3)) && (lt = lp_strchr(pf + lp_strlen(DBP_PREFIX3), u'>')))
			{
				*lt = 0;
				dbp.superClasses.insert(nextField + lp_strlen(DBP_SCO) + 1 + lp_strlen(DBP_PREFIX3));
			}
			else if ((pf = lp_strstr(nextField, DBP_PREFIX4)) && (lt = lp_strchr(pf + lp_strlen(DBP_PREFIX4), u'>')))
			{
				*lt = 0;
				dbp.superClasses.insert(nextField + lp_strlen(DBP_SCO) + 1 + lp_strlen(DBP_PREFIX4));
			}
			else
				lplog(LOG_WIKIPEDIA, u"%s prefix not recognized", nextField);
		}
		// <http://dbpedia.org/ontology/BasketballLeague> <http://www.w3.org/2000/01/rdf-schema#compactLabel> "basketball league"@en .
		if (!lp_strncmp(nextField, DBP_LABEL, lp_strlen(DBP_LABEL)) && (eq = lp_strstr(nextField + lp_strlen(DBP_LABEL), u"\"@en .")))
		{
			*eq = 0;
			dbp.compactLabel = nextField + lp_strlen(DBP_LABEL) + 2;
		}
		if (!lp_strncmp(nextField, DBP_COMMENT, lp_strlen(DBP_COMMENT)) && (eq = lp_strstr(nextField + lp_strlen(DBP_COMMENT), u"\"@en .")))
		{
			*eq = 0;
			dbp.commentDescription = nextField + lp_strlen(DBP_COMMENT) + 2;
		}
	}
	fclose(fp);
	// transform super classes - each superclass like BasketballLeague is moved to its compactLabel 'basketball league' so that it can be found
	// by looking up by the words in the text and also for the derivation by ontological rank to work
	for (unordered_map <lpwstring, cOntologyEntry>::iterator dboci = dbPediaOntologyCategoryList.begin(), dbociEnd = dbPediaOntologyCategoryList.end(); dboci != dbociEnd; dboci++)
	{
		if (dboci->second.ontologyType == dbPedia_Ontology_Type)
		{
			unordered_set <lpwstring> superClasses;
			for (auto sci : dboci->second.superClasses)
			{
				unordered_map<lpwstring, lpwstring>::iterator lm = labelMap.find(sci);
				if (lm != labelMap.end())
				{
					superClasses.insert(lm->second);
				}
				else
					lplog(LOG_WIKIPEDIA, u"superclass %s of %s not found.", sci.c_str(), dboci->second.compactLabel.c_str());
			}
			dboci->second.superClasses = superClasses;
		}
	}
	fillRanks(dbPedia_Ontology_Type);
	lplog(LOG_WIKIPEDIA, u"dbpedia ontology inserted %d entries.", dbPediaOntologyCategoryList.size() - beginDbPediaEntries);
	return 0;
}

/*
unordered_map <lpwstring, cOntologyEntry>::iterator copy(unordered_map <lpwstring, cOntologyEntry> &hm,void *buf,int &where,int limit,unordered_map <lpwstring, cOntologyEntry>::iterator &hint)
{ LFS
	lpwstring key;
	cOntologyEntry predicate;
	//unordered_map <lpwstring, cOntologyEntry>::iterator hmi;
	if (!copy(key,buf,where,limit)) return NULL;
	if (!copy(predicate,buf,where,limit)) return NULL;
	return hint=hm.insert(hint,std::pair<lpwstring,cOntologyEntry>(key,predicate));
	//if ((hmi=hm.find(key))==dbPediaOntologyCategoryList.end())
	//{
	//	hm[key]=predicate;
	//	hmi=hm.find(key);
	//}
	//return hmi;
}
*/

// Serialize dbsn into buf (same field set as the cTreeCat::copy entry writer).
bool cOntology::copy(void* buf, cOntologyEntry& dbsn, int& where, int limit)
{
	DLFS
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

// Deserialize one cOntologyEntry from the _rdfTypes binary cache.
bool copy(cOntologyEntry& dbsn, void* buf, int& where, int limit)
{
	DLFS
		if (!copy(dbsn.compactLabel, buf, where, limit)) return false;
	if (!copy(dbsn.infoPage, buf, where, limit)) return false;
	if (!copy(dbsn.birthDate, buf, where, limit)) return false;
	if (!copy(dbsn.birthPlace, buf, where, limit)) return false;
	if (!copy(dbsn.occupation, buf, where, limit)) return false;
	if (!copy(dbsn.abstractDescription, buf, where, limit)) return false;
	if (!copy(dbsn.commentDescription, buf, where, limit)) return false;
	if (!copy(dbsn.numLine, buf, where, limit)) return false;
	if (!copy(dbsn.ontologyType, buf, where, limit)) return false;
	if (!copy(dbsn.ontologyHierarchicalRank, buf, where, limit)) return false;
	if (!copy(dbsn.superClasses, buf, where, limit)) return false;
	if (!copy(dbsn.descriptionFilled, buf, where, limit)) return false;
	return true;
}

// Deserialize key+entry and insert into hm; hint becomes the inserted iterator.
bool copy(unordered_map <lpwstring, cOntologyEntry>::iterator& hint, void* buf, int& where, int limit, unordered_map <lpwstring, cOntologyEntry>& hm)
{
	DLFS
		lpwstring key;
	cOntologyEntry dbPredicate;
	if (copy(key, buf, where, limit) && copy(dbPredicate, buf, where, limit))
	{
		std::pair<unordered_map <lpwstring, cOntologyEntry>::iterator, bool> p = hm.insert(std::pair<lpwstring, cOntologyEntry>(key, dbPredicate));
		hint = p.first;
		return true;
	}
	return false;
}


const lpchar_t* lpOntologySuperClasses[] = { u"provincesandterritoriesofcanada",u"country",u"island",u"mountain",u"geoclasspark",u"river",u"stream",u"city",u"statesoftheunitedstates",NULL };

// wall-clock seconds since process start (clock()/CLOCKS_PER_SEC).
int clocksec()
{
	return clock() / CLOCKS_PER_SEC;
}

// Load dbPediaOntologyCategoryList from CACHEDIR\dbPediaCache\_rdfTypes, or
// rebuild from YAGO+DBpedia+UMBEL if missing / reInitialize / version mismatch.
// Then force lpOntologySuperClasses ranks to -1 (preferred geography tops).
int cOntology::fillOntologyList(bool reInitialize)
{
	LFS
		initializeDatabaseHandle(cOntology::mysql, u"localhost", cOntology::alreadyConnected);
	if (dbPediaOntologyCategoryList.empty())
	{
		lpchar_t path[4096];
		int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\dbPediaCache", CACHEDIR);
		if (lp_wmkdir(path) < 0 && errno == ENOENT)
			lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
		// Batch B2: wcsncpy has no char16_t equivalent; lp_strcpy is a safe drop-in
		// here specifically because path[] is 4096 lpchar_t and the bound this
		// wcsncpy passed (MAX_LEN(2048)-pathlen) was already generous slack for a
		// fixed 10-character literal, never a tight/load-bearing truncation.
		lp_strcpy(path + pathlen, u"\\_rdfTypes");
		convertIllegalChars(path + pathlen + 1);
		path[MAX_PATH - 1] = 0;
		if (lp_waccess(path, 0) < 0 || reInitialize)
		{
			// Heap, not stack: MAX_BUF is 2,000,000 bytes here (1.91 MiB), the last
			// of the oversized stack arrays the code review inventoried.
			// cTrackedBuffer frees on all 7 of this function's return paths.
			cTrackedBuffer buffer(MAX_BUF);
			readYAGOOntology();
			readDbPediaOntology();
			// look for ###
			// skip one line, look for the same
			// skip one line, look for rdfs:subClass and take the rest of the line (end in ;?)
			// take more lines until the line ends in a ; or the line doesn't end in a ,
			readUMBELSuperClasses();
			int fd = lp_wopen(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE);
			if (fd < 0)
			{
				lplog(LOG_ERROR, u"Cannot write rdfTypes dbPediaCache - %S.", strerror(errno));
				return -1;
			}
			int where = 0;
			*((lpchar_t*)buffer.get()) = RDFLIBRARYTYPE_VERSION;
			where += 2;
			for (auto ri : dbPediaOntologyCategoryList)
			{
				lpwstring tmpstr;
				lplog(LOG_WIKIPEDIA, u"category %s", ri.second.toString(tmpstr, ri.first).c_str());
				if (!::copy(buffer, ri.first, where, MAX_BUF) || !copy(buffer, ri.second, where, MAX_BUF))
				{
					lplog(LOG_FATAL_ERROR, u"Cannot write rdfTypes dbPediaCache - %S.", strerror(errno));
					return -1;
				}
				if (where > MAX_BUF - 40960)
				{
					if (::write(fd, buffer, where) < 0)
					{
						lplog(LOG_FATAL_ERROR, u"Cannot write rdfTypes dbPediaCache - %S.", strerror(errno));
						return -1;
					}
					where = 0;
				}
			}
			::write(fd, buffer, where);
			::close(fd);
		}
		else
		{
			int fd = lp_wopen(path, O_RDWR | O_BINARY);
			if (fd < 0)
			{
				lplog(LOG_ERROR, u"Cannot open rdfTypes (%s) - %S.", path, strerror(errno));
				return -1;
			}
			void* vBuffer;
			int bufferlen = lp_filelength(fd), where = 0;
			vBuffer = (void*)tmalloc(bufferlen + 10);
			::read(fd, vBuffer, bufferlen);
			::close(fd);
			if (*((lpchar_t*)vBuffer) != RDFLIBRARYTYPE_VERSION) // version
			{
				tfree(bufferlen + 10, vBuffer);
				return fillOntologyList(true);
			}
			where += 2;
			lpwstring name;
			int lastProgressPercent = -1;
			unordered_map <lpwstring, cOntologyEntry>::iterator hint = dbPediaOntologyCategoryList.end();
			int numAbstractDescriptions = 0, numCommentDescriptions = 0, numInfoPages = 0, numOntologyHierarchicalRank = 0, numSuperClasses = 0;
			while (where < bufferlen)
			{
				if (((int64_t)where * 100 / (int64_t)bufferlen) > (int64_t)lastProgressPercent)
				{
					lastProgressPercent = ((int64_t)where * 100 / (int64_t)bufferlen);
					lp_wprintf(u"PROGRESS: %03d%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \r", lastProgressPercent, where, bufferlen, clocksec(), memoryAllocated);
				}
				if (!::copy(hint, vBuffer, where, bufferlen, dbPediaOntologyCategoryList))
				{
					lplog(LOG_FATAL_ERROR, u"Cannot read ontology relations - %S.", strerror(errno));
					return -1;
				}
				//hint->second.lplog(LOG_WHERE, hint->first);
				if (hint->second.abstractDescription.length() > 0)
					numAbstractDescriptions++;
				if (hint->second.commentDescription.length() > 0)
					numCommentDescriptions++;
				if (hint->second.infoPage.length() > 0)
					numInfoPages++;
				if (hint->second.ontologyHierarchicalRank > 0)
					numOntologyHierarchicalRank++;
				if (hint->second.superClasses.size() > 0)
					numSuperClasses++;
			}
			lplog(LOG_WHERE, u"ontology: numAbstractDescriptions=%d,numCommentDescriptions=%d,numInfoPages=%d,numOntologyHierarchicalRank=%d,numSuperClasses=%d",
				numAbstractDescriptions, numCommentDescriptions, numInfoPages, numOntologyHierarchicalRank, numSuperClasses);
			lp_wprintf(u"PROGRESS: 100%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \n", where, bufferlen, clocksec(), memoryAllocated);
			tfree(bufferlen + 10, vBuffer);
		}
	}
	for (int I = 0; lpOntologySuperClasses[I]; I++)
	{
		if (dbPediaOntologyCategoryList.find(lpOntologySuperClasses[I]) != dbPediaOntologyCategoryList.end())
			//lplog(LOG_ERROR,u"superClass %s not found in ontology list (1).",lpOntologySuperClasses[I]);
		//else
			dbPediaOntologyCategoryList[lpOntologySuperClasses[I]].ontologyHierarchicalRank = -1;
	}
	return 0;
}

bool writeDbOntologyEntry(MYSQL& mysql, const lpwstring key, cOntologyEntry& dbPredicate);
bool readDbOntologyEntry(MYSQL& mysql, lpwstring key, cOntologyEntry& oncologyEntry);

void maxFieldLengths(const lpwstring key, cOntologyEntry& dbPredicate, int& maxKey, int& maxCompactLabel, int& maxInfoPage, int& maxAbstractDescription, int& maxCommentDescription, int& maxSuperClasses, int& numGTA, int& numGTB, int& numGTC);

// Scan the in-memory map and log max string lengths (schema-sizing helper).
bool cOntology::maxFieldLengths()
{
	int maxKey = -1, maxCompactLabel = -1, maxInfoPage = -1, maxAbstractDescription = -1, maxCommentDescription = -1, maxSuperClasses = -1;
	int numGT150 = 0, numGT170 = 0, numGT190 = 0;
	for (auto dbp : dbPediaOntologyCategoryList)
		::maxFieldLengths(dbp.first, dbp.second, maxKey, maxCompactLabel, maxInfoPage, maxAbstractDescription, maxCommentDescription, maxSuperClasses, numGT150, numGT170, numGT190);
	lplog(LOG_INFO, u"maxKey=%d maxCompactLabel =%d maxInfoPage =%d maxAbstractDescription =%d maxCommentDescription =%d maxSuperClasses =%d", maxKey, maxCompactLabel, maxInfoPage, maxAbstractDescription, maxCommentDescription, maxSuperClasses);
	lplog(LOG_INFO, u"numGT150=%d numGT170 =%d numGT190 =%d ", numGT150, numGT170, numGT190);
	return true;
}



// Classify uri by ontology prefix, look up / fetch a missing YAGO or UMBEL super,
// push a cTreeCat, return the hierarchical rank or -1.  Confidence is qtype[0]-'0'
// (qtype is like u"1 TYPES").  UMBEL HTTP path is noted as broken by redirects.
int cOntology::findCategoryRank(lpwstring& qtype, lpwstring& parentObject, lpwstring& object, vector <cTreeCat*>& rdfTypes, lpwstring& uri)
{
	LFS
		bool foundDBPediaCategory = false, foundYAGOCategory = false, foundUMBELCategory = false, foundOpenGISCategory = false;
	lpwstring cat;
	if (foundDBPediaCategory = uri.find(u"http://dbpedia.org/ontology/") != lpwstring::npos) cat = uri.c_str() + lp_strlen(u"http://dbpedia.org/ontology/");
	else if (foundYAGOCategory = uri.find(u"http://dbpedia.org/class/yago/") != lpwstring::npos) cat = uri.c_str() + lp_strlen(u"http://dbpedia.org/class/yago/");
	else if (foundUMBELCategory = uri.find(u"http://umbel.org/umbel/rc/") != lpwstring::npos) cat = uri.c_str() + lp_strlen(u"http://umbel.org/umbel/rc/");
	else if (foundOpenGISCategory = uri.find(u"http://www.opengis.net/") != lpwstring::npos) cat = uri.c_str() + lp_strlen(u"http://www.opengis.net/");
	else return -1;
	unordered_map <lpwstring, cOntologyEntry>::iterator cli = findCategory(cat);
	int ret;// , numResults = 0;
	lpwstring buffer, temp, superClass;
	size_t pos = 0, pos2 = 0;
	if ((cli == dbPediaOntologyCategoryList.end() || cli->second.ontologyHierarchicalRank == 100) && foundYAGOCategory)
	{
		if (ret = getDBPediaPath(-1, uri, buffer, object + u"_cR" + qtype + cat + u".html")) return -1;
		// <a class="uri" rel="rdfs:subClassOf" xmlns:rdfs="http://www.w3.org/2000/01/rdf-schema#" href="http://dbpedia.org/class/yago/Alumnus109786338">
		if (firstMatch(buffer, u"<a class=\"uri\" rel=\"rdfs:subClassOf\"", u">", pos, temp, false) != lpwstring::npos &&
			firstMatch(temp, u"href=\"http://dbpedia.org/class/yago/", u"\"", pos2, superClass, false) != lpwstring::npos)
		{
			if (find(dbPediaOntologyCategoryList[cat].superClasses.begin(), dbPediaOntologyCategoryList[cat].superClasses.end(), superClass) == dbPediaOntologyCategoryList[cat].superClasses.end())
				dbPediaOntologyCategoryList[cat].superClasses.insert(superClass);
			cli = dbPediaOntologyCategoryList.find(cat);
			cli->second.ontologyType = YAGO_Ontology_Type;
		}
	}
	// UMBEL getPath doesn't work because of HTTP redirection - this code path is disabled
	// <rdf:Description rdf:about="http://umbel.org/umbel/rc/EukaryoticCell"><rdfs:subClassOf rdf:resource="http://umbel.org/umbel#TopicsCategories"/></rdf:Description>
	// <rdf:Description rdf:about="http://umbel.org/umbel/rc/EukaryoticCell"><rdfs:subClassOf rdf:resource="http://umbel.org/umbel/rc/Cell"/></rdf:Description>
	if ((cli == dbPediaOntologyCategoryList.end() || cli->second.ontologyHierarchicalRank == 100) && foundUMBELCategory)
	{
		if (ret = getDBPediaPath(-1, uri, buffer, object + u"_cR" + qtype + cat + u".html")) return -1;
		if (firstMatch(buffer, u"<rdfs:subClassOf rdf:resource=\"http://umbel.org/umbel/rc/", u"\"", pos, superClass, false) != lpwstring::npos)
		{
			if (find(dbPediaOntologyCategoryList[cat].superClasses.begin(), dbPediaOntologyCategoryList[cat].superClasses.end(), superClass) == dbPediaOntologyCategoryList[cat].superClasses.end())
				dbPediaOntologyCategoryList[cat].superClasses.insert(superClass);
			cli = dbPediaOntologyCategoryList.find(cat);
			cli->second.ontologyType = UMBEL_Ontology_Type;
		}
	}
	if (cli != dbPediaOntologyCategoryList.end())
	{
		//if (cli->second.compactLabel.size()>0)
		//	getDescription(cli);  only works with dbpedia ontology
		unordered_map <lpwstring, cOntologyEntry>::iterator scli = findCategory(superClass);
		if (scli != dbPediaOntologyCategoryList.end())
			cli->second.ontologyHierarchicalRank = scli->second.ontologyHierarchicalRank + 1;
	}
	if (cli != dbPediaOntologyCategoryList.end())
	{
		rdfTypes.push_back(new cTreeCat(cli, object, parentObject, qtype, qtype[0] - u'0', parentObject));
		return cli->second.ontologyHierarchicalRank;
	}
	return -1;
}

// access dbPedia on the local virtuoso server.  Derive type, description and ontological position and rank
// Run one Virtuoso SPARQL (begin+encoded object+end), collect resource URIs or
// typed hits via findCategoryRank, attach getDescription() to new hits, and
// append a SEPARATOR cTreeCat.  Rejects long / illegal-char objects and a
// leading dash.  Returns true if at least one <uri> was seen.
bool cOntology::extractResults(lpwstring begin, lpwstring uobject, lpwstring end, lpwstring qtype, vector <cTreeCat*>& rdfTypes, vector <lpwstring>& resources, lpwstring parentObject)
{
	LFS
		lpwstring object;
	if (uobject.find_first_of(u",|:[]#.()$!%") != lpwstring::npos && uobject.find(u"http://dbpedia.org") == lpwstring::npos)
	{
		lplog(LOG_ERROR, u"dbpedia extractResults %s rejected (illegal character)", uobject.c_str());
		return false;
	}
	if (uobject.length() > 100)
	{
		lplog(LOG_ERROR, u"dbpedia extractResults %s rejected (too long)", uobject.c_str());
		return false;
	}
	size_t bw;
	// + is illegal in dbPedia
	while ((bw = uobject.find_first_of(u"+/ ")) != lpwstring::npos)
		uobject[bw] = u'_';
	encodeURL(uobject, object);
	if (cWord::isDash(object[0]))
		return false;
	lpwstring webAddress = basehttpquery + begin + object + end, buffer, temp, uri, fpobject = uobject, beginhttpquery = basehttpquery + begin;
	vector <lpwstring> labels;
	adjustQuote(beginhttpquery, object, webAddress);
	int ret, numResults = 0, originalRDFTypesSize = rdfTypes.size();
	if (object.find(u"http%3A%2F%2Fdbpedia%2Eorg%2Fresource%2F") != lpwstring::npos)
	{
		labels.push_back(object.c_str() + lp_strlen(u"http%3A%2F%2Fdbpedia%2Eorg%2Fresource%2F"));
		fpobject = uobject.c_str() + lp_strlen(u"http://dbpedia.org/resource/");
	}
	else
		labels.push_back(object);
	if (ret = cInternet::readPage(webAddress.c_str(), buffer)) return false;
	//if (ret=getDBPediaPath(-1,webAddress,buffer,fpobject+u"_"+qtype+u".xml")) return false;
	takeLastMatch(buffer, u"<results distinct=\"false\" ordered=\"true\">", u"</results>", temp, false);
	for (size_t pos = 0; firstMatch(temp, u"<uri>", u"</uri>", pos, uri, false) != lpwstring::npos; numResults++)
	{
		lplog(LOG_WIKIPEDIA, u"%s:%s:%s:%s", qtype.c_str(), parentObject.c_str(), fpobject.c_str(), uri.c_str());
		if (uri.find(u"http://dbpedia.org/resource/") != lpwstring::npos)
			resources.push_back(uri);
		else
			findCategoryRank(qtype, parentObject, fpobject, rdfTypes, uri);
	}
	lpwstring abstract, comment, infoPage, birthDate, birthPlace, occupation;
	getDescription(labels[0], fpobject, abstract, comment, infoPage, birthDate, birthPlace, occupation);
	for (unsigned int I = originalRDFTypesSize; I < rdfTypes.size(); I++)
	{
		rdfTypes[I]->assignDetails(abstract, comment, infoPage, birthDate, birthPlace, occupation);
	}
	//if (!numResults)
	decodeURL(webAddress, temp);
	int whereQuery = temp.find(u"query=");
	if (whereQuery != lpwstring::npos)
		temp.erase(0, whereQuery + 6);
	if (logRDFDetail)
		lplog(LOG_WIKIPEDIA | LOG_RESOLUTION, u"%s:%s:%s\nENCODED WEBADDRESS:%s\nDECODED WEBADDRESS%s",
			qtype.c_str(), parentObject.c_str(), fpobject.c_str(), webAddress.c_str(), temp.c_str());
	unordered_map <lpwstring, cOntologyEntry>::iterator dbSeparator = dbPediaOntologyCategoryList.find(SEPARATOR);
	if (dbSeparator == dbPediaOntologyCategoryList.end())
	{
		cOntologyEntry ndbs;
		dbPediaOntologyCategoryList[SEPARATOR] = ndbs;
		dbSeparator = dbPediaOntologyCategoryList.find(SEPARATOR);
	}
	if (rdfTypes.size() && rdfTypes[rdfTypes.size() - 1]->cli != dbSeparator)
		rdfTypes.push_back(new cTreeCat(dbSeparator));
	return numResults > 0;
}

inline int (isUnderline)(int c) { return c == u'_'; }
// Map a Freebase type string onto dbPediaOntologyCategoryList (try '_', spaces,
// then no-underline).  Confidence 1 if slobject matches name/k, else 6.
// Returns 1 on insert, -2 unknown type, -3 rejectCategories hit.
int cOntology::enterCategory(string& id, string& k, string& propertyValue, string& description, string& slobject, lpwstring& object, string& objectType, string& name, vector <lpwstring>& wikipediaLinks, vector <lpwstring>& professionLinks, vector <cTreeCat*>& rdfTypes)
{
	LFS
		if (rejectCategories.find(propertyValue) == rejectCategories.end())
		{
			unordered_map <lpwstring, cOntologyEntry>::iterator cli;
			lpwstring wPropertyValue, wId;
			mTW(propertyValue, wPropertyValue);
			// turn written_work into writtenwork or written work	
			if ((cli = dbPediaOntologyCategoryList.find(wPropertyValue)) == dbPediaOntologyCategoryList.end() && wPropertyValue.find(u'_') != lpwstring::npos)
			{
				lpwstring intoSpaces = wPropertyValue;
				replace(intoSpaces.begin(), intoSpaces.end(), u'_', u' ');
				if ((cli = dbPediaOntologyCategoryList.find(intoSpaces)) != dbPediaOntologyCategoryList.end())
					wPropertyValue = intoSpaces;
				else
				{
					lpwstring noUnderlines = wPropertyValue;
					noUnderlines.erase(remove_if(noUnderlines.begin(), noUnderlines.end(), isUnderline), noUnderlines.end());
					if ((cli = dbPediaOntologyCategoryList.find(noUnderlines)) != dbPediaOntologyCategoryList.end())
						wPropertyValue = noUnderlines;
				}
			}
			if (cli != dbPediaOntologyCategoryList.end())
			{
				if (logOntologyDetail)
					lplog(LOG_WHERE | LOG_WIKIPEDIA, u"object=%s objectType=%S id=%S name=%S", object.c_str(), objectType.c_str(), id.c_str(), name.c_str());
				cTreeCat* tc;
				if (slobject == name || slobject == k)
					rdfTypes.push_back(tc = new cTreeCat(cli, mTW(id, wId), object, u"1 FB", 1, k, description, wikipediaLinks, professionLinks, k == slobject));
				else
					rdfTypes.push_back(tc = new cTreeCat(cli, mTW(id, wId), object, u"6 FB", 6, k, description, wikipediaLinks, professionLinks, k == slobject));
				return 1;
			}
			else
			{
				if (logOntologyDetail)
					lplog(LOG_WHERE | LOG_WIKIPEDIA, u"NOT FOUND: object=%s objectType=%S id=%S", object.c_str(), objectType.c_str(), id.c_str());
				return -2;
			}
		}
		else if (logOntologyDetail)
			lplog(LOG_WHERE | LOG_WIKIPEDIA, u"REJECTED: object=%s objectType=%S id=%S propertyValue=%S", object.c_str(), objectType.c_str(), id.c_str(), propertyValue.c_str());
	return -3;
}

// get acronym
// Scrape acronyms.thefreedictionary.com for an all-caps single token.
// Returns -1 if the object is not all-caps or the fetch fails.
int cOntology::getAcronyms(lpwstring& object, vector <lpwstring>& acronyms)
{
	LFS
		// must be one word and must be all caps
		for (unsigned int I = 0; I < object.size(); I++)
			if (iswlower(object[I]) || iswspace(object[I]) || iswdigit(object[I]))
				return -1;
	// issue request
	// http://acronyms.thefreedictionary.com/BND
	lpwstring webAddress = u"http://acronyms.thefreedictionary.com/" + object;
	lpwstring buffer, filePathOut, headers;
	if (cInternet::getWebPath(-1, webAddress, buffer, object + u"ACRO", u"acronymCache", filePathOut, headers, -1, false, true, forceWebReread) < 0)
		return -1;
	// reduce acronym page
	size_t beginPos = lpwstring::npos;
	lpwstring acronymsBuffer, individualAcronymBuffer;
	size_t firstMatch(lpwstring & buffer, lpwstring beginString, lpwstring endString, size_t & beginPos, lpwstring & match, bool include_begin_and_end);
	firstMatch(buffer, u"<table id=AcrFinder class=AcrFinder", u"</table>", beginPos, acronymsBuffer, false);
	beginPos = lpwstring::npos;
	/*
<html>
	<head>
		<meta labelWithSpace="generator"
		content="HTML Tidy for HTML5 (experimental) for Windows https://github.com/w3c/tidy-html5/tree/c63cc39" />
		<title></title>
	</head>
	<body>
		<table id="AcrFinder" class="AcrFinder" cellpadding="0" cellspacing="0">
			<tr>
				<th>Acronym</th>
				<th>Definition</th>
			</tr>
			<tr cat="64">
				<td class="acr">BND</td>
				<td>Brunei Dollar
				<span class="illustration">(ISO currency code)</span></td>
			</tr>
			<tr cat="8">
				<td class="acr">BND</td>
				<td>Band</td>
			</tr>
			<tr cat="4">
				<td class="acr">BND</td>
				<td>Bundesnachrichtendienst
				<span class="illustration">(German Intelligence Agency)</span></td>
			</tr>
		</table>
	</body>
</html>
	*/
	while (firstMatch(acronymsBuffer, u"<tr cat=", u"</tr>", beginPos, individualAcronymBuffer, false) != lpwstring::npos)
	{
		/*
		<tr cat="64">
				<td class="acr">BND</td>
				<td>Brunei Dollar
				<span class="illustration">(ISO currency code)</span></td>
			</tr>
			*/
		lpwstring trashSpan;
		beginPos = -1;
		while (firstMatch(individualAcronymBuffer, u"<span", u"</span>", beginPos, trashSpan, false) != lpwstring::npos);
		lpwstring base, acronym;
		firstMatch(individualAcronymBuffer, u"acr>", u"</td>", beginPos, base, false);
		firstMatch(individualAcronymBuffer, u"<td>", u"</td>", beginPos, acronym, false);
		trim(base);
		trim(acronym);
		if (base == object)
			acronyms.push_back(acronym);
	}
	return 0;
}


/*********************************************************
freebase begin
***********************************************************/



// properties labelWithSpace type
// Normalize object (spaces, lower, escape ') and SELECT freebaseProperties
// where name or k matches.  Delegates to lookupInFreebaseQuery(..., accumulateAliases).
int cOntology::lookupInFreebase(lpwstring object, vector <cTreeCat*>& rdfTypes)
{
	LFS
		initializeDatabaseHandle(mysql, u"localhost", alreadyConnected);
	replace(object.begin(), object.end(), u'_', u' ');
	replace(object.begin(), object.end(), u'+', u' ');
	replace(object.begin(), object.end(), u'|', u' ');
	removeExcessSpaces(object);
	if (object.find(u'\'') != lpwstring::npos)
		escapeSingleQuote(object);
	transform(object.begin(), object.end(), object.begin(), (int(*)(int)) tolower);
	lpwstring lobject = object, k = object;
	string slobject;
	wTM(lobject, slobject);
	if (k.find(u'!') != lpwstring::npos)
	{
		replace(k.begin(), k.end(), u'!', u' ');
		removeExcessSpaces(k);
	}
	replace(k.begin(), k.end(), u' ', u'_');
	lpwstring q = u"select k,id,properties from freebaseProperties where name='" + lobject + u"' OR k='" + k + u"'";
	if (k.find('\'') != string::npos)
	{
		lpwstring k2 = k;
		removeSingleQuote(k2);
		if (k2 != k)
			q += u" OR k='" + k2 + u"'";
	}
	if (logOntologyDetail)
		::lplog(LOG_RESOLUTION, u"freebase lookup: %s", q.c_str());
	return lookupInFreebaseQuery(object, slobject, q, rdfTypes, true);
}

// select * from freebaseProperties where id in ('m.012t_z','m.0fj9f','m.0hltv');
// Replace Freebase mids in links with the corresponding name column.  Returns -1 on SQL fail.
int cOntology::lookupLinks(vector <lpwstring>& links)
{
	LFS
		MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	lpwstring tmpstr, q = u"select name from freebaseProperties where id in ('" + vectorString(links, tmpstr, u"','") + u"')";
	if (!myquery(&mysql, (lpchar_t*)q.c_str(), result, true)) return -1;
	vector<lpwstring> names;
	lpwstring name;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
		if (sqlrow[0] != NULL)
			names.push_back(mTW(sqlrow[0], name));
	links = names;
	return 0;
}

// Parse freebaseProperties rows: {T} types (simplified to song/band/musician),
// {W}/{P} links, {D}/{N} text.  Empty-properties rows become alias ids that are
// re-queried once.
int cOntology::lookupInFreebaseQuery(lpwstring& object, string& slobject, lpwstring& q, vector <cTreeCat*>& rdfTypes, bool accumulateAliases)
{
	LFS
		MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	if (!myquery(&mysql, (lpchar_t*)q.c_str(), result, true))
	{
		lplog(LOG_FATAL_ERROR, u"freebaseQuery SQL error: %s\n%s\n%S", q.c_str(), object.c_str(), slobject.c_str());
		return -1;
	}
	set <string> simpleCommonTypes; // this is to keep 1000 songs about Paris from clogging up the file
	vector<string> aliases;
	while ((sqlrow = mysql_fetch_row(result)) != NULL)
	{
		string k = (sqlrow[0] == NULL) ? "" : sqlrow[0], id = (sqlrow[1] == NULL) ? "" : sqlrow[1], properties = (sqlrow[2] == NULL) ? "" : sqlrow[2];
		if (logOntologyDetail)
			::lplog(LOG_RESOLUTION, u"freebase lookup: k=%S id=%S properties=%S", k.c_str(), id.c_str(), properties.c_str());
		if (properties.empty())
		{
			if (accumulateAliases)
				aliases.push_back(id);
			continue;
		}
		replace(id.begin(), id.end(), '_', ' ');
		cOntologyEntry ndbs;
		dbPediaOntologyCategoryList[SEPARATOR] = ndbs;
		//unordered_map <lpwstring, cOntologyEntry>::iterator dbSeparator=dbPediaOntologyCategoryList.find(SEPARATOR);
		size_t whereDescription = properties.find("{D}"), whereNextType, whereNextWikipediaLink, whereNextProfessionLink;
		size_t whereName = properties.find("{N}");
		string description, objectType;
		string name;
		if (whereDescription != string::npos)
		{
			description = properties.substr(whereDescription + 3);
			properties.erase(whereDescription);
		}
		if (whereName != string::npos)
		{
			size_t nextBracket = properties.find('{', whereName + 1);
			if (nextBracket != string::npos)
				// substr's 2nd argument is a length, not an end position: subtract the start.
				name = properties.substr(whereName + 3, nextBracket - (whereName + 3));
			else
				name = properties.substr(whereName + 3);
			transform(name.begin(), name.end(), name.begin(), (int(*)(int)) tolower);
		}
		vector <lpwstring> wikipediaLinks;
		for (size_t whereWikipediaLink = properties.find("{W}"); whereWikipediaLink != string::npos; whereWikipediaLink = whereNextWikipediaLink)
		{
			string wikipediaLink;
			lpwstring wWikipediaLink;
			if ((whereNextWikipediaLink = properties.find("{W}", whereWikipediaLink + 1)) == string::npos)
				wikipediaLink = properties.substr(whereWikipediaLink + 3);
			else
				wikipediaLink = properties.substr(whereWikipediaLink + 3, whereNextWikipediaLink - whereWikipediaLink - 3);
			size_t nextBracket = wikipediaLink.find("{");
			if (nextBracket != string::npos)
				wikipediaLink.erase(wikipediaLink.begin() + nextBracket, wikipediaLink.end());
			wikipediaLinks.push_back(mTW(wikipediaLink, wWikipediaLink));
		}
		vector <lpwstring> professionLinks;
		for (size_t whereProfessionLink = properties.find("{P}"); whereProfessionLink != string::npos; whereProfessionLink = whereNextProfessionLink)
		{
			string professionLink;
			lpwstring wProfessionLink;
			if ((whereNextProfessionLink = properties.find("{P}", whereProfessionLink + 1)) == string::npos)
				professionLink = properties.substr(whereProfessionLink + 3);
			else
				professionLink = properties.substr(whereProfessionLink + 3, whereNextProfessionLink - whereProfessionLink - 3);
			size_t nextBracket = professionLink.find("{");
			if (nextBracket != string::npos)
				professionLink.erase(professionLink.begin() + nextBracket, professionLink.end());
			professionLinks.push_back(mTW(professionLink, wProfessionLink));
		}
		lookupLinks(professionLinks);
		for (size_t whereType = properties.find("{T}"); whereType != string::npos; whereType = whereNextType)
		{
			if ((whereNextType = properties.find("{T}", whereType + 1)) == string::npos)
				objectType = properties.substr(whereType + 3);
			else
				objectType = properties.substr(whereType + 3, whereNextType - whereType - 3);
			size_t nextBracket = objectType.find("{");
			if (nextBracket != string::npos)
				objectType.erase(objectType.begin() + nextBracket, objectType.end());
			int whereLastPeriod = objectType.find_last_of('.');
			string lastPropertyValue = objectType.substr(whereLastPeriod + 1);
			string familyPropertyValue = objectType.substr(0, whereLastPeriod);
			string simple;
			// simplifying extremely common combinations
			if ((lastPropertyValue == "recording" || lastPropertyValue == "single" || lastPropertyValue == "album" || lastPropertyValue == "composition" || lastPropertyValue == "release") && familyPropertyValue == "music")
				simple = "song";
			if (lastPropertyValue == "musical_group" && familyPropertyValue == "music")
				simple = "band";
			if (lastPropertyValue == "artist" && familyPropertyValue == "music")
				simple = "musician";
			if ((lastPropertyValue == "written_work" || lastPropertyValue == "book" || lastPropertyValue == "published_work" || lastPropertyValue == "composition") && familyPropertyValue == "book")
				simple = "song";
			if (simpleCommonTypes.find(simple) != simpleCommonTypes.end())
				continue;
			simpleCommonTypes.insert(simple);
			// this is adding information but it is not what the item IS
			if ((lastPropertyValue == "award_winning_work" || lastPropertyValue == "award_nominated_work") && familyPropertyValue == "award")
				continue;
			if (!simple.empty())
				enterCategory(id, k, simple, description, slobject, object, objectType, name, wikipediaLinks, professionLinks, rdfTypes);
			else if (enterCategory(id, k, lastPropertyValue, description, slobject, object, objectType, name, wikipediaLinks, professionLinks, rdfTypes) != -3)
				enterCategory(id, k, familyPropertyValue, description, slobject, object, objectType, name, wikipediaLinks, professionLinks, rdfTypes);
			unordered_map <lpwstring, cOntologyEntry>::iterator dbSeparator = dbPediaOntologyCategoryList.find(SEPARATOR);
			if (rdfTypes.size() && rdfTypes[rdfTypes.size() - 1]->cli != dbSeparator)
				rdfTypes.push_back(new cTreeCat(dbSeparator));
		}
	}
	mysql_free_result(result);
	for (unsigned int I = 0; I < aliases.size(); I++)
	{
		lpwstring alias, fbq = lpwstring(u"select k,id,properties from freebaseProperties where id='") + mTW(aliases[I], alias) + u"'";
		lookupInFreebaseQuery(object, slobject, fbq, rdfTypes, false);
	}
	//lplog(LOG_WHERE|LOG_WIKIPEDIA,NULL);
	return 0;
}

/*********************************************************
freebase end
***********************************************************/

// object must start with a capital
// getRDFTypes – attempt to follow one object through dbPedia, considering:
//   a. its base entry
//   b. its resources listed in its base entry
//   c. its disambiguations listed with each of these derived resources
//   d. its disambiguations derived from its base entry
//   e. the resources derived from each of the disambiguations derived from its base entry
//   f. look up in freebase
// SPARQL walk: types, redirects (+ their types/disambiguations), then the
// object's own disambiguations, then Freebase.  fromWhere is unused here.
void cOntology::getRDFTypesFromDbPedia(lpwstring object, vector <cTreeCat*>& rdfTypes, lpwstring fromWhere)
{
	LFS
		lpwstring buffer, start;
	vector <lpwstring> resources;
	lplog(LOG_WIKIPEDIA, u"%s", object.c_str());
	// Irving Berlin
	// SELECT+%3Fv+%0D%0AWHERE+%7B%3AIrving_Berlin+a+%3Fv%7D
	extractResults(prefix_colon + u"SELECT+%3Fv+%0D%0AWHERE+%7B%3A", object, u"+a+%3Fv%7D", u"1 TYPES", rdfTypes, resources, u"");
	// get any redirects from object
	extractResults(prefix_colon + prefix_owl_ontology + selectWhere + u"+%7B%3A", object, u"+dbpedia-owl%3AwikiPageRedirects+%3Fv+%7D%0D%0A%7D", u"2 REDIRECT", rdfTypes, resources, u"");
	// for each redirect, start over.
	for (unsigned int I = 0; I < resources.size(); I++)
	{
		// REDIRECT:http://dbpedia.org/resource/Rafic_Hariri
		start = prefix_colon + prefix_owl_ontology + selectWhere + u"+%7B+%3C";
		vector <lpwstring> recursiveResources;
		extractResults(start, resources[I], u"%3E+a+%3Fv+%7D%0D%0A%7D&format=text%2Fxml&timeout=0&debug=on", u"3 REDIRECT(RESOURCES)", rdfTypes, recursiveResources, object);
		// get any disambiguating references from resource
		extractResults(start, resources[I], u"%3E+dbpedia-owl%3AwikiPageDisambiguates+%3Fv+%7D%0D%0A%7D", u"4 DISAMBIGUATE from REDIRECT", rdfTypes, recursiveResources, u"");
		for (unsigned int J = 0; J < recursiveResources.size(); J++)
		{
			// DISAMBIGUATE:http://dbpedia.org/resource/Blondie_%28film%29
			start = prefix_dbpedia + selectWhere + u"+%3C";
			vector <lpwstring> recursiveResources2;
			extractResults(start, recursiveResources[J], u"%3E+a+%3Fv+%0D%0A%7D&format=text%2Fxml&timeout=0&debug=on", u"5 DISAMBIGUATE from REDIRECT(RESOURCES)", rdfTypes, recursiveResources2, object);
		}
	}
	resources.clear();
	start = prefix_colon + prefix_owl_ontology + selectWhere + u"+%7B%3A";
	extractResults(start, object, u"+dbpedia-owl%3AwikiPageDisambiguates+%3Fv+%7D%0D%0A%7D", u"3 DISAMBIGUATE", rdfTypes, resources, u"");
	// for each disambiguation, start over.
	for (unsigned int I = 0; I < resources.size(); I++)
	{
		start = prefix_dbpedia + selectWhere + u"+%3C";
		vector <lpwstring> recursiveResources2;
		extractResults(start, resources[I], u"%3E+a+%3Fv+%0D%0A%7D&format=text%2Fxml&timeout=0&debug=on", u"4 DISAMBIGUATE(RESOURCES)", rdfTypes, recursiveResources2, object);
	}
	lookupInFreebase(object, rdfTypes);
	if (logRDFDetail)
		lplog(LOG_WIKIPEDIA | LOG_INFO, u"%s:%d rdf types in dbpedia", object.c_str(), rdfTypes.size());
}

// Load an .rdfTypes cache.  Returns 0, -1 on open fail, -2 on version mismatch
// (file is then deleted).  New cTreeCat objects are appended to rdfTypes.
int cOntology::readRDFTypes(lpchar_t path[4096], vector <cTreeCat*>& rdfTypes)
{
	LFS
		//lplog(LOG_WHERE, u"TRACEOPEN %s %s", path, LP_TEXT(__func__).c_str());
		int fd = lp_wopen(path, O_RDWR | O_BINARY);
	if (fd < 0)
	{
		lplog(LOG_ERROR, u"Cannot open rdfTypes (%s) - %S.", path, strerror(errno));
		return -1;
	}
	void* buffer;
	int bufferlen = lp_filelength(fd), where = 0;
	buffer = (void*)tmalloc(bufferlen + 10);
	::read(fd, buffer, bufferlen);
	::close(fd);
	if (*((lpchar_t*)buffer) != RDFTYPE_VERSION) // version
	{
		tfree(bufferlen + 10, buffer);
		lp_wremove(path);
		return -2;
	}
	where += 2;
	unordered_map <lpwstring, cOntologyEntry>::iterator hint = dbPediaOntologyCategoryList.end();
	while (where < bufferlen)
	{
		cTreeCat* tc = new cTreeCat();
		if (!tc->copy(dbPediaOntologyCategoryList, buffer, where, bufferlen)) break;
		rdfTypes.push_back(tc);
	}
	tfree(bufferlen + 10, buffer);
	return 0;
}

// Write .rdfTypes.  Uses a ~20MB heap buffer (tmalloc/tfree, tracked in
// memoryAllocated) rather than putting it on the stack.
int cOntology::writeRDFTypes(lpchar_t path[4096], vector <cTreeCat*>& rdfTypes)
{
	LFS
		int fd = lp_wopen(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0)
	{
		lplog(LOG_ERROR, u"Cannot write %s - %S.", path, strerror(errno));
		return -1;
	}
	char* buffer = (char*)tmalloc(MAX_BUF * 10);
	if (!buffer)
	{
		lplog(LOG_FATAL_ERROR, u"writeRDFTypes: out of memory allocating %d bytes for %s.", MAX_BUF * 10, path);
		::close(fd);
		return -1;
	}
	int where = 0;
	*((lpchar_t*)buffer) = RDFTYPE_VERSION;
	where += 2;
	for (vector <cTreeCat*>::iterator ri = rdfTypes.begin(), riEnd = rdfTypes.end(); ri != riEnd; ri++)
	{
		if (!(*ri)->copy(buffer, where, MAX_BUF * 10))
		{
			lplog(LOG_FATAL_ERROR, u"Cannot write %s - %S.", path, strerror(errno));
			tfree(MAX_BUF * 10, buffer);
			::close(fd);
			return -1;
		}
		if (where > (MAX_BUF * 10) - 8192)
		{
			::write(fd, buffer, where);
			where = 0;
		}
	}
	if (where > 0)
		::write(fd, buffer, where);
	tfree(MAX_BUF * 10, buffer);
	::close(fd);
	return 0;
}

// Strip '_' from the last path component so a too-long cache filename fits MAX_PATH.
void cOntology::compressPath(lpchar_t* path)
{
	lpchar_t* ch = lp_strrchr(path, u'\\');
	if (ch)
	{
		lpchar_t* destch = ch;
		for (; *ch; ch++)
			if (*ch != u'_')
			{
				*destch = *ch;
				destch++;
			}
		*destch = 0;
	}
}

// True if `noRDFTypes` has this word.  object is escaped (escapeSingleQuote) before
// being interpolated into SQL, consistent with lookupInFreebase elsewhere in this file.
bool cOntology::inRDFTypeNotFoundTable(lpchar_t* object)
{
	initializeDatabaseHandle(mysql, u"localhost", alreadyConnected);
	if (!myquery(&mysql, u"LOCK TABLES noRDFTypes READ"))
		return false;
	MYSQL_RES* result;
	int64_t numResults = 0;
	lpwstring eobject = object;
	escapeSingleQuote(eobject);
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select 1 from noRDFTypes where word = '%s'", eobject.c_str());
	if (myquery(&mysql, qt, result))
	{
		numResults = mysql_num_rows(result);
		mysql_free_result(result);
	}
	if (!myquery(&mysql, u"UNLOCK TABLES"))
		return false;
	lplog(LOG_INFO, u"*** inRDFTypeNotFoundTable Temp: statement %s resulted in numRows=%d.", qt, numResults);
	return numResults > 0;
}

// INSERT into noRDFTypes.  object is escaped (escapeSingleQuote) before being interpolated.
bool cOntology::insertRDFTypeNotFoundTable(lpchar_t* object)
{
	initializeDatabaseHandle(mysql, u"localhost", alreadyConnected);
	if (!myquery(&mysql, u"LOCK TABLES noRDFTypes WRITE"))
		return false;
	lpwstring eobject = object;
	escapeSingleQuote(eobject);
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_wsprintf(qt, u"INSERT INTO noRDFTypes VALUES ('%s')", eobject.c_str());
	bool success = (myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY);
	if (!myquery(&mysql, u"UNLOCK TABLES"))
		return false;
	return success;
}

// True if noERDFTypes has '_' + convertIllegalChars(name).  convertIllegalChars already
// maps a stray ' to '_' as a side effect, but newPath is escaped too for defense in depth.
bool cOntology::inNoERDFTypesDBTable(lpwstring newObjectName)
{
	initializeDatabaseHandle(mysql, u"localhost", alreadyConnected);
	lpchar_t newPath[1024];
	lp_strcpy(newPath, newObjectName.c_str());
	convertIllegalChars(newPath);
	if (!myquery(&mysql, u"LOCK TABLES noERDFTypes READ"))
		return false;
	MYSQL_RES* result;
	int64_t numResults = 0;
	lpwstring enewPath = newPath;
	escapeSingleQuote(enewPath);
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select 1 from noERDFTypes where word = '_%s'", enewPath.c_str());
	if (myquery(&mysql, qt, result))
	{
		numResults = mysql_num_rows(result);
		mysql_free_result(result);
	}
	if (!myquery(&mysql, u"UNLOCK TABLES"))
		return false;
	//lplog(LOG_INFO, u"*** inNoERDFTypesDBTable Temp: statement %s resulted in numRows=%d.", qt, numResults);
	return numResults > 0;
}

// INSERT into noERDFTypes.  Same sanitizing (convertIllegalChars) plus an
// escapeSingleQuote belt-and-suspenders pass, matching the reader above.
bool cOntology::insertNoERDFTypesDBTable(lpwstring newObjectName)
{
	initializeDatabaseHandle(mysql, u"localhost", alreadyConnected);
	lpchar_t newPath[1024];
	lp_strcpy(newPath, newObjectName.c_str());
	convertIllegalChars(newPath);
	if (!myquery(&mysql, u"LOCK TABLES noERDFTypes WRITE"))
		return false;
	lpwstring enewPath = newPath;
	escapeSingleQuote(enewPath);
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_wsprintf(qt, u"INSERT INTO noERDFTypes VALUES ('_%s')", enewPath.c_str());
	bool success = (myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY);
	if (!myquery(&mysql, u"UNLOCK TABLES"))
		return false;
	return success;
}

// Dump rdfTypes to LOG_WIKIPEDIA.
int cOntology::printRDFTypes(const lpchar_t* kind, vector <cTreeCat*>& rdfTypes)
{
	lplog(LOG_WIKIPEDIA, u"BEGIN %s:%d", kind, rdfTypes.size());
	for (int I = 0; I < rdfTypes.size(); I++)
		rdfTypes[I]->lplogTC(LOG_WIKIPEDIA, u"");
	lplog(LOG_WIKIPEDIA, u"END %s:%d", kind, rdfTypes.size());
	return 0;
}


// Resolve object: in-memory rdfTypeMap, else .rdfTypes file, else SPARQL+acronyms.
// Mutates rdfTypeNumMap under an exclusive SRWLOCK (both the miss-path insert and
// the hit-path counter increment are writes).  Empty results go into noRDFTypes.
// Path offset path+pathlen+5 assumes a 4-char subdirectory.
int cOntology::getRDFTypesMaster(lpwstring object, vector <cTreeCat*>& rdfTypes, lpwstring fromWhere, bool fileCaching)
{
	LFS
		if (cacheRdfTypes)
		{
			// Exclusive: both branches below write rdfTypeNumMap (insert or increment).
			// Batch B3: one scoped lock covers both exits (the early return and the
			// fallthrough) where the Win32 version needed a Release on each path.
			std::unique_lock<std::shared_mutex> rdfTypeMapLock(rdfTypeMapSRWLock);
			unordered_map<lpwstring, int >::iterator rdfni;
			if ((rdfni = rdfTypeNumMap.find(object)) == rdfTypeNumMap.end())
				rdfTypeNumMap[object] = 1;
			else
			{
				(*rdfni).second++;
				rdfTypes = rdfTypeMap[object];
				//		lplog(LOG_WHERE,u"rdfCache %s %d",object.c_str(),(*rdfni).second);
				return 0;
			}
		}
	if (object.length() > 512)
		return -1;
	lpchar_t path[4096];
	int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\dbPediaCache", CACHEDIR), retCode = -1;
	if (lp_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	lp_snprintf(path + pathlen, MAX_LEN - pathlen, u"\\_%s", object.c_str());
	convertIllegalChars(path + pathlen + 1);
	if (lp_strlen(path + pathlen + 1) > 127 || lp_strlen(path + pathlen + 1) < 2 || inRDFTypeNotFoundTable(path + pathlen + 1))
		return -1;
	distributeToSubDirectories(path, pathlen + 1, true);
	if (lp_strlen(path) + 12 > MAX_PATH)
		compressPath(path);
	path[MAX_PATH - 12] = 0;
	path[pathlen + 2 + 244] = 0; // to guard against a filename that is too long
	lp_strcpy((path) + lp_strlen(path), u".rdfTypes");
	if (!fileCaching || lp_waccess(path, 0) < 0 || (retCode = readRDFTypes(path, rdfTypes)) < 0)
	{
		getRDFTypesFromDbPedia(object, rdfTypes, fromWhere);
		vector <lpwstring> acronyms;
		cOntology::getAcronyms(object, acronyms);
		if (acronyms.size())
		{
			lpwstring tmpstr;
			lplog(LOG_WHERE, u"ACRO:%s yields acronyms:%s", object.c_str(), vectorString(acronyms, tmpstr, u",").c_str());
			for (unsigned int I = 0; I < acronyms.size(); I++)
			{
				vector <cTreeCat*> rdfTypesAcronym;
				replace(acronyms[I].begin(), acronyms[I].end(), u' ', u'_');
				cOntology::rdfIdentify(acronyms[I], rdfTypesAcronym, u"b", fileCaching);
				rdfTypes.insert(rdfTypes.end(), rdfTypesAcronym.begin(), rdfTypesAcronym.end());
			}
			//for (unsigned int I=rdfLen; I<rdfTypes.size(); I++)
			//	rdfTypes[I]->lplog(LOG_WHERE);
		}
		if (rdfTypes.empty())
		{
			path[lp_strlen(path) - 9] = 0;
			insertRDFTypeNotFoundTable(path + pathlen + 5); // added 4 for the distribution to subdirectories
		}
		else
		{
			printRDFTypes(u"RDFM", rdfTypes);
			retCode = writeRDFTypes(path, rdfTypes);
		}
	}
	if (!superClassesAllPopulated)
	{
		bool oneNotFound = false;
		for (int I = 0; lpOntologySuperClasses[I]; I++)
		{
			if (dbPediaOntologyCategoryList.find(lpOntologySuperClasses[I]) != dbPediaOntologyCategoryList.end())
				dbPediaOntologyCategoryList[lpOntologySuperClasses[I]].ontologyHierarchicalRank = -1;
			else
				oneNotFound = true;
			//	lplog(LOG_ERROR,u"superClass %s not found in ontology list (2).",lpOntologySuperClasses[I]);
		}
		superClassesAllPopulated = !oneNotFound;
	}
	if (cacheRdfTypes)
	{
		std::unique_lock<std::shared_mutex> rdfTypeMapLock(rdfTypeMapSRWLock);
		rdfTypeMap[object] = rdfTypes;
	}
	return retCode;
}

// insert the rdfType (cli->first) into topHierarchyClassIndexes, if it matches a known top class.
//   if the top class is already there, make sure the topHierarchyClassIndexes are pointing to the rdfType with the lowest confidence (the MOST confident entry)
// If rdfTypes[I] is fromCategory, record it as finalCategory in the top-class map
// (keep the lowest-confidence / most-confident hit).  On insert, I is advanced
// to the next SEPARATOR so later types of the same resource are skipped.
bool checkInsert(const lpchar_t* fromCategory, const lpchar_t* finalCategory, unordered_map <lpwstring, int >& topHierarchyClassIndexes, vector <cTreeCat*>& rdfTypes, unsigned int& I)
{
	LFS
		//rdfTypes[I]->lplog(LOG_WHERE);
		if (rdfTypes[I]->cli->first != fromCategory)
			return false;
	unordered_map <lpwstring, int >::iterator idOi = topHierarchyClassIndexes.find(finalCategory);
	bool found = false;
	if (found = idOi == topHierarchyClassIndexes.end())
		topHierarchyClassIndexes[finalCategory] = I;
	else
	{
		if (found = rdfTypes[I]->confidence < rdfTypes[idOi->second]->confidence)
			idOi->second = I;
	}
	if (found)
	{
		// advance to end of separator, if any - take the first type (Vijay Singh) first type is person, which is correct, not sports_team,which is listed afterwards
		for (; I < rdfTypes.size(); I++)
			if (rdfTypes[I]->cli->first == SEPARATOR)
				break;
	}
	return true;
}

const lpchar_t* knownClasses[] = { u"person", u"place", u"gml/_feature", u"location",u"business",u"organisation",u"work",u"plant",u"animal",
	u"disease",u"provincesandterritoriesofcanada",u"country",u"island",u"mountain",u"geoclasspark",u"river",u"stream",u"city",u"statesoftheunitedstates",NULL };
const lpchar_t* knownMapToClasses[] = { u"person", u"place", u"place", u"place",u"business",u"business",u"creativeWork",u"plant",u"animal",
	u"disease",u"provincesandterritoriesofcanada",u"country",u"island",u"mountain",u"geoclasspark",u"river",u"river",u"city",u"statesoftheunitedstates",NULL };
set <lpwstring> knownClassesSet;

// only return true if all entries have either no super classes or are a known class (above)
// Walk rdfTypes from rdfBaseTypeOffset: map known classes into
// topHierarchyClassIndexes (via knownMapToClasses).  Returns true if some
// unknown class still has a super that is in neither the map nor rdfTypes
// (caller should keep climbing).
bool cOntology::topClassesAvailableToBeAdded(unordered_map <lpwstring, int >& topHierarchyClassIndexes, vector <cTreeCat*>& rdfTypes, int rdfBaseTypeOffset)
{
	LFS
		if (knownClassesSet.empty())
		{
			for (int k = 0; knownClasses[k]; k++)
				knownClassesSet.insert(knownClasses[k]);
		}
	bool superClassMissingFromList = false;
	for (unsigned int I = rdfBaseTypeOffset; I < rdfTypes.size(); I++)
	{
		if (rdfTypes[I]->cli->first == SEPARATOR) continue;
		if (knownClassesSet.find(rdfTypes[I]->cli->first) == knownClassesSet.end())
		{
			for (lpwstring wc : rdfTypes[I]->cli->second.superClasses)
			{
				// does the entry have super classes that are not already in topHierarchyClassIndexes or the rdfTypes?
				bool superClassFound = topHierarchyClassIndexes.find(wc) != topHierarchyClassIndexes.end();
				if (!superClassFound)
				{
					for (cTreeCat* tc : rdfTypes)
						if (superClassFound = tc->cli->first == wc)
							break;
				}
				if (!superClassFound)
					superClassMissingFromList = true;
			}
			continue;
		}
		for (int k = 0; knownClasses[k]; k++)
			if (checkInsert(knownClasses[k], knownMapToClasses[k], topHierarchyClassIndexes, rdfTypes, I))
				break;
	}
	return superClassMissingFromList; //  && minIdentifiedQType==minQType
}

set<lpwstring>  doNotFollow = { u"artifact",u"computer",u"device" ,u"instrumentality" ,u"unit" ,u"object" ,u"product" ,u"website",u"whole",u"work",u"abstraction",u"symbol",u"symbols",u"music",u"song",u"partially intangible individual",u"intangible",
u"artifact-generic",u"agent-non geographical",u"orphans",SEPARATOR,u"agent-generic",u"spatial thing-localized",u"organizations",u"organisation",u"organization",u"business",u"property",u"employer",u"creative work",u"individual",u"periodical",u"credential",
u"food",u"concept",u"model",u"fashion model",u"pipeline",u"resource",u"architecture",u"influence",u"war",u"musician",u"SocialGroup107950920",u"field of study",u"constitution",u"short story",u"adaptation",u"human language",u"place",u"natural language",
u"attribute values",u"settlement",u"publishing company" };
// Append supers of rdfTypes[rdfBaseTypeOffset..) unless they are in doNotFollow.
// Recurses while topClassesAvailableToBeAdded says a known top is still missing.
void cOntology::includeAllSuperClasses(unordered_map <lpwstring, int >& topHierarchyClassIndexes, vector <cTreeCat*>& rdfTypes, int recursionLevel, int rdfBaseTypeOffset)
{
	LFS
		if (rdfTypes.empty())
			return;
	lpwstring tmpstr, tmpstr2;
	unsigned int rdfOriginalSize = rdfTypes.size();
	lpwstring rdfInfoPrinted;
	for (unsigned int I = rdfBaseTypeOffset; I < rdfOriginalSize; I++)
	{
		if (doNotFollow.find(rdfTypes[I]->cli->first) != doNotFollow.end())
			continue;
		lpwstring object = rdfTypes[I]->typeObject;
		if (recursionLevel == 1 && logOntologyDetail)
			rdfTypes[I]->logIdentity(LOG_RESOLUTION | LOG_WIKIPEDIA, object, false, rdfInfoPrinted);
		//if (rdfTypes[I]->cli->second.superClasses.empty())
		//	lp_wprintf(u"%d:%s:%s [no superclasses]\n",I,rdfTypes[I]->object.c_str(),rdfTypes[I]->cli->first.c_str());
		for (auto sci : rdfTypes[I]->cli->second.superClasses)
		{
			if (doNotFollow.find(sci) != doNotFollow.end())
				continue;
			unordered_map <lpwstring, cOntologyEntry>::iterator cli;
			if (recursionLevel)
				cli = dbPediaOntologyCategoryList.find(sci);
			else
				cli = findCategory(sci);
			if (cli != dbPediaOntologyCategoryList.end() && cli->second.ontologyHierarchicalRank <= rdfTypes[I]->cli->second.ontologyHierarchicalRank)
			{
				bool alreadyThere = false;
				for (vector <cTreeCat*>::iterator ri = rdfTypes.begin(), riEnd = rdfTypes.end(); ri != riEnd && !(alreadyThere = (*ri)->cli == cli); ri++);
				if (!alreadyThere)
				{
					if (logOntologyDetail)
					{
						//lp_wprintf(u"  %d %s:%s:%s %s found %s\n",I,rdfTypes[I]->qtype.c_str(),rdfTypes[I]->object.c_str(),rdfTypes[I]->cli->first.c_str(),sci->c_str(),cli->first.c_str());
						lplog(LOG_RESOLUTION | LOG_WIKIPEDIA, u"%*s%s:(%s)ISTYPE %s:%s:%s:[%s]:rank %d(%s IASC [%s])",
							recursionLevel << 1, u" ", object.c_str(), sci.c_str(), cli->first.c_str(), ontologyTypeString(cli->second.ontologyType, cli->second.resourceType, tmpstr2), cli->second.compactLabel.c_str(), setString(cli->second.superClasses, tmpstr, u" ").c_str(), cli->second.ontologyHierarchicalRank, rdfTypes[I]->qtype.c_str(), rdfTypes[I]->derivation.c_str());
					}
					rdfTypes.push_back(new cTreeCat(cli, object, sci, rdfTypes[I]->qtype, rdfTypes[I]->qtype[0] - u'0', rdfTypes[I]->derivation + u"*" + rdfTypes[I]->cli->first));
				}
			}
			else if (cli == dbPediaOntologyCategoryList.end())
				lplog(LOG_RESOLUTION, u"%*s%s:superclass NOT found: %s", recursionLevel << 1, u" ", object.c_str(), sci.c_str());
		}
	}
	if (rdfTypes.size() > rdfOriginalSize && topClassesAvailableToBeAdded(topHierarchyClassIndexes, rdfTypes, rdfOriginalSize))
		includeAllSuperClasses(topHierarchyClassIndexes, rdfTypes, ++recursionLevel, rdfOriginalSize);
}

// Public wrapper: climb from offset 0 if a known top class is not yet present.
void cOntology::includeSuperClasses(unordered_map <lpwstring, int >& topHierarchyClassIndexes, vector <cTreeCat*>& rdfTypes)
{
	int recursionLevel = 1, rdfBaseTypeOffset = 0;
	if (topClassesAvailableToBeAdded(topHierarchyClassIndexes, rdfTypes, rdfBaseTypeOffset))
		includeAllSuperClasses(topHierarchyClassIndexes, rdfTypes, recursionLevel, rdfBaseTypeOffset);
}

// True if word cannot be represented in CP1252 without substitution.
// Batch B10: the Win32 WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, ...) call
// this used answered exactly one question -- "would any character have to be
// replaced by a default substitute in code page 1252?" -- and reported it through
// lpUsedDefaultChar. The same question is answered directly here by checking each
// code unit against the CP1252 repertoire, which needs no conversion at all.
//
// CP1252 covers U+0000-U+00FF except the C1 range U+0080-U+009F, plus 27 specific
// characters mapped into 0x80-0x9F (smart quotes, dashes, the euro sign, and so
// on). Anything else has no CP1252 representation.
bool detectNonEuropean(lpwstring word)
{
	static const lpchar_t cp1252HighRange[] = {
		0x20AC, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6,
		0x2030, 0x0160, 0x2039, 0x0152, 0x017D, 0x2018, 0x2019, 0x201C,
		0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
		0x0153, 0x017E, 0x0178
	};
	for (lpchar_t c : word)
	{
		if (c <= 0x7F) continue;                       // ASCII
		if (c >= 0x00A0 && c <= 0x00FF) continue;      // Latin-1 supplement
		bool found = false;
		for (lpchar_t mapped : cp1252HighRange)
			if (c == mapped) { found = true; break; }
		if (!found) return true;
	}
	return false;
}


// Public typer: fillOntologyList, then skip (only log) overly long/short or
// non-European names when logOntologyDetail is set; otherwise getRDFTypesMaster.
// Long objects are still looked up when logOntologyDetail is false.
void cOntology::rdfIdentify(lpwstring object, vector <cTreeCat*>& rdfTypes, lpwstring fromWhere, bool fileCaching)
{
	LFS
		fillOntologyList(false);
	if (object.size() > 40 && logOntologyDetail)
		lplog(LOG_ERROR, u"rdfIdentify:object too long - %s", object.c_str());
	else if (object.size() == 1 && logOntologyDetail)
		lplog(LOG_ERROR, u"rdfIdentify:object too short - %s", object.c_str());
	else if (detectNonEuropean(object) || object.find_first_of(u"ãâäáàæçêéèêëîíïñôóòöõôûüùú") != lpwstring::npos)
		lplog(LOG_ERROR, u"rdfIdentify:object having non european character set (or non English characters) - %s", object.c_str());
	else
	{
		getRDFTypesMaster(object, rdfTypes, fromWhere, fileCaching);
	}
}

// Mark preferred on the top-class hits with best (lowest) confidence then lowest
// rank; mark preferredUnknownClass the same way over all non-SEPARATOR hits.
// Returns true if any flag was set.
bool cOntology::setPreferred(unordered_map <lpwstring, int >& topHierarchyClassIndexes, vector <cTreeCat*>& rdfTypes)
{
	LFS
		bool chosen = false;
	int p = 100, r = 1000;
	for (unordered_map <lpwstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
		p = min(p, rdfTypes[idi->second]->confidence);
	for (unordered_map <lpwstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
		if (p == rdfTypes[idi->second]->confidence)
			r = min(r, rdfTypes[idi->second]->cli->second.ontologyHierarchicalRank);
	for (unordered_map <lpwstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
	{
		chosen |= (rdfTypes[idi->second]->preferred = p == rdfTypes[idi->second]->confidence && r == rdfTypes[idi->second]->cli->second.ontologyHierarchicalRank);
	}

	p = 100;
	r = 1000;
	for (vector <cTreeCat*>::iterator rdfi = rdfTypes.begin(), rdfiEnd = rdfTypes.end(); rdfi != rdfiEnd; rdfi++)
		if ((*rdfi)->cli->first != SEPARATOR)
			p = min(p, (*rdfi)->confidence);
	for (vector <cTreeCat*>::iterator rdfi = rdfTypes.begin(), rdfiEnd = rdfTypes.end(); rdfi != rdfiEnd; rdfi++)
		if (p == (*rdfi)->confidence)
			r = min(r, (*rdfi)->cli->second.ontologyHierarchicalRank);
	for (vector <cTreeCat*>::iterator rdfi = rdfTypes.begin(), rdfiEnd = rdfTypes.end(); rdfi != rdfiEnd; rdfi++)
		chosen |= ((*rdfi)->preferredUnknownClass = (*rdfi)->cli->first != SEPARATOR && p == (*rdfi)->confidence && r == (*rdfi)->cli->second.ontologyHierarchicalRank);
	return chosen;
}


// Debug: rdfIdentify + climb + setPreferred and log the resulting identity.
// Deletes the cTreeCat list only when cacheRdfTypes is false.
void cOntology::printIdentity(lpwstring object)
{
	LFS
		replace(object.begin(), object.end(), u' ', u'_');
	vector <cTreeCat*> rdfTypes;
	cOntology::rdfIdentify(object, rdfTypes, u"0");
	unordered_map <lpwstring, int > topHierarchyClassIndexes;
	if (topClassesAvailableToBeAdded(topHierarchyClassIndexes, rdfTypes, 0))
		includeAllSuperClasses(topHierarchyClassIndexes, rdfTypes, 1, 0);
	setPreferred(topHierarchyClassIndexes, rdfTypes);
	if (rdfTypes.empty())
		lplog(LOG_INFO, u"%s:rdfType not found", object.c_str());
	else
		for (unsigned int I = 0; I < rdfTypes.size(); I++)
		{
			lpwstring tmpstr;
			if (rdfTypes[I]->cli->first == SEPARATOR)
				lplog(LOG_WIKIPEDIA, u"----------------------------------------------------");
			else
			{
				rdfTypes[I]->lplogTC(LOG_WIKIPEDIA, object);
				//unordered_map <lpwstring, cOntologyEntry>::iterator clisc;
				//if (rdfTypes[I]->cli->second.superClasses.size()>0)
				//{
				//		for (vector <lpwstring>::iterator sci=rdfTypes[I]->cli->second.superClasses.begin(),sciEnd=rdfTypes[I]->cli->second.superClasses.end(); sci!=sciEnd; sci++)
				//			if ((clisc=findCategory(*sci))!=dbPediaOntologyCategoryList.end()) 
				//				lplog(LOG_WIKIPEDIA,u"  %s:ISTYPE %s:%s:%s:[%s]:rank %d",sci->c_str(),clisc->first.c_str(),ontologyTypeString(clisc->second.ontologyType),clisc->second.compactLabel.c_str(),vectorString(clisc->second.superClasses,tmpstr).c_str(),clisc->second.rank);
				//}
			}
		}
	lpwstring rdfInfoPrinted;
	for (unordered_map <lpwstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
		rdfTypes[idi->second]->logIdentity(LOG_INFO, object, true, rdfInfoPrinted);
	if (topHierarchyClassIndexes.empty() && rdfTypes.size())
	{
		unsigned int ontologyHierarchicalRank = rdfTypes[0]->cli->second.ontologyHierarchicalRank;
		// get lowest rank
		for (unsigned int I = 0; I < rdfTypes.size(); I++)
			ontologyHierarchicalRank = min(ontologyHierarchicalRank, (unsigned)rdfTypes[I]->cli->second.ontologyHierarchicalRank);
		for (unsigned int I = 0; I < rdfTypes.size(); I++)
			if (ontologyHierarchicalRank >= (unsigned)rdfTypes[I]->cli->second.ontologyHierarchicalRank)
				rdfTypes[I]->logIdentity(LOG_INFO, object, false, rdfInfoPrinted);
	}
	if (!cOntology::cacheRdfTypes)
		for (unsigned int I = 0; I < rdfTypes.size(); I++)
			delete rdfTypes[I]; // now caching them
}

// printIdentity each NULL-terminated objects[i].
void cOntology::printIdentities(lpchar_t* objects[])
{
	LFS
		for (int I = 0; objects[I]; I++)
			printIdentity(objects[I]);
}




#ifdef TEST_CODE
// TEST_CODE: load _rdfTypes and "_rdfTypes - original" and log per-type counts.
// fd is not checked; first vBuffer is leaked; copyOLD is assumed to exist.
void cOntology::compareRDFTypes()
{
	// read in new 
	lpchar_t path[4096];
	lp_snprintf(path, MAX_LEN, u"%s\\dbPediaCache\\_rdfTypes", CACHEDIR);
	int fd = lp_wopen(path, O_RDWR | O_BINARY);
	void* vBuffer;
	int bufferlen = lp_filelength(fd), where = 0;
	vBuffer = (void*)tmalloc(bufferlen + 10);
	::read(fd, vBuffer, bufferlen);
	::close(fd);
	int numTypes[5], numOriginalTypes[5];
	memset(numTypes, 0, 5 * (sizeof(*numTypes)));
	memset(numOriginalTypes, 0, 5 * (sizeof(*numOriginalTypes)));
	where += 2;
	lpwstring name;
	int lastProgressPercent = -1;
	unordered_map <lpwstring, cOntologyEntry>::iterator hint = dbPediaOntologyCategoryList.end();
	int numAbstractDescriptions = 0, numCommentDescriptions = 0, numInfoPages = 0, numOntologyHierarchicalRank = 0, numSuperClasses = 0;
	while (where < bufferlen)
	{
		if (((int64_t)where * 100 / (int64_t)bufferlen) > (int64_t)lastProgressPercent)
		{
			lastProgressPercent = ((int64_t)where * 100 / (int64_t)bufferlen);
			lp_wprintf(u"PROGRESS: %03d%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \r", lastProgressPercent, where, bufferlen, clocksec(), memoryAllocated);
		}
		if (!::copy(hint, vBuffer, where, bufferlen, dbPediaOntologyCategoryList))
			lplog(LOG_FATAL_ERROR, u"Cannot read ontology relations.");
		hint->second.lplog(LOG_WHERE, hint->first);
		numTypes[hint->second.ontologyType]++;
		if (hint->second.abstractDescription.length() > 0)
			numAbstractDescriptions++;
		if (hint->second.commentDescription.length() > 0)
			numCommentDescriptions++;
		if (hint->second.infoPage.length() > 0)
			numInfoPages++;
		if (hint->second.ontologyHierarchicalRank > 0)
			numOntologyHierarchicalRank++;
		if (hint->second.superClasses.size() > 0)
			numSuperClasses++;
	}
	lplog(LOG_WHERE, u"ontology: numAbstractDescriptions=%d,numCommentDescriptions=%d,numInfoPages=%d,numOntologyHierarchicalRank=%d,numSuperClasses=%d",
		numAbstractDescriptions, numCommentDescriptions, numInfoPages, numOntologyHierarchicalRank, numSuperClasses);
	for (int I = 1; I < 5; I++)
		lplog(LOG_WHERE, u"ontology: numType=%d", numTypes[I]);
	lp_wprintf(u"PROGRESS: 100%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \n", where, bufferlen, clocksec(), memoryAllocated);
	//////////////////////////////////// original
	lp_snprintf(path, MAX_LEN, u"%s\\dbPediaCache\\_rdfTypes - original", CACHEDIR);
	fd = lp_wopen(path, O_RDWR | O_BINARY);
	bufferlen = lp_filelength(fd), where = 0;
	vBuffer = (void*)tmalloc(bufferlen + 10);
	::read(fd, vBuffer, bufferlen);
	::close(fd);
	where += 2;
	lastProgressPercent = -1;
	unordered_map <lpwstring, cOntologyEntry> originalDbPediaOntologyCategoryList;
	hint = originalDbPediaOntologyCategoryList.end();
	int numOriginalAbstractDescriptions = 0, numOriginalCommentDescriptions = 0, numOriginalInfoPages = 0, numOriginalOntologyHierarchicalRank = 0, numOriginalSuperClasses = 0;
	while (where < bufferlen)
	{
		if (((int64_t)where * 100 / (int64_t)bufferlen) > (int64_t)lastProgressPercent)
		{
			lastProgressPercent = ((int64_t)where * 100 / (int64_t)bufferlen);
			lp_wprintf(u"PROGRESS: %03d%% %d out of %d original ontology relation bytes read with %I64d seconds elapsed (%d bytes) \r", lastProgressPercent, where, bufferlen, clocksec(), memoryAllocated);
		}
		::copyOLD(hint, vBuffer, where, bufferlen, originalDbPediaOntologyCategoryList);
		hint->second.lplog(LOG_WHERE, hint->first);
		numOriginalTypes[hint->second.ontologyType]++;
		if (hint->second.abstractDescription.length() > 0)
			numOriginalAbstractDescriptions++;
		if (hint->second.commentDescription.length() > 0)
			numOriginalCommentDescriptions++;
		if (hint->second.infoPage.length() > 0)
			numOriginalInfoPages++;
		if (hint->second.ontologyHierarchicalRank > 0)
			numOriginalOntologyHierarchicalRank++;
		if (hint->second.superClasses.size() > 0)
			numOriginalSuperClasses++;
	}
	lplog(LOG_WHERE, u"ontology: numOriginalAbstractDescriptions=%d,numOriginalCommentDescriptions=%d,numOriginalInfoPages=%d,numOriginalOntologyHierarchicalRank=%d,numOriginalSuperClasses=%d",
		numOriginalAbstractDescriptions, numOriginalCommentDescriptions, numOriginalInfoPages, numOriginalOntologyHierarchicalRank, numOriginalSuperClasses);
	for (int I = 1; I < 5; I++)
		lplog(LOG_WHERE, u"ontology: numOriginalType=%d", numOriginalTypes[I]);
	lp_wprintf(u"PROGRESS: 100%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \n", where, bufferlen, clocksec(), memoryAllocated);
	tfree(bufferlen + 10, vBuffer);
}

// TEST_CODE: printIdentities of hard-coded geo / person / org lists, then exit(0).
void cOntology::testWikipedia()
{
	LFS
		//t.traceWikipedia=true;
		initialize();
	lpchar_t* canadianTerritories[] = { u"Nunavut",	u"Quebec", u"Northwest Territories",u"Ontario", u"British Columbia", u"Alberta",
	 u"Saskatchewan", u"Manitoba", u"Yukon",u"Newfoundland and Labrador", u"New Brunswick",u"Nova Scotia", u"Prince Edward Island",NULL }; // CANADIAN_PROVINCE_CITY
	printIdentities(canadianTerritories);
	lpchar_t* countries[] = { u"Russia",	u"Canada", u"France",u"Germany",NULL }; // COUNTRY
	printIdentities(countries);
	lpchar_t* islands[] = { u"Greenland",u"New Guinea",u"Borneo",u"Madagascar",u"Baffin Island",u"Sumatra",NULL }; // ISLAND
	printIdentities(islands);
	lpchar_t* mountains[] = { u"Mount Everest",u"K2",u"Kangchenjunga",u"Lhotse",u"Makalu",u"Cho Oyu",u"Dhaulagiri",u"Manaslu",u"Nanga Parbat",u"Annapurna",NULL }; // MOUNTAIN_RANGE_PEAK_LANDFORM
	printIdentities(mountains);
	// dbPedia is rather bad at identifying the Oceans, which is actually not a super big deal
	// lpchar_t *oceans[]={ u"Pacific Ocean",u"Atlantic Ocean",u"Indian Ocean",u"Southern Ocean",u"Arctic Ocean",NULL }; // OCEAN_SEA
	// printIdentities(oceans);
	lpchar_t* parks[] = { u"Acadia",u"American Samoa",u"Arches",u"Badlands",u"Big Bend",u"Biscayne",u"Black Canyon of the Gunnison",u"Bryce Canyon",u"Canyonlands",
										 u"Capitol Reef",u"Carlsbad Caverns",u"Channel Islands",u"Congaree",u"Crater Lake",u"Cuyahoga Valley",u"Death Valley",
										 u"Denali",u"Dry Tortugas",u"Everglades",u"Gates of the Arctic",u"Glacier",u"Glacier Bay",u"Grand Canyon",u"Grand Teton",
										 u"Great Basin",u"Great Sand Dunes",u"Great Smoky Mountains",u"Guadalupe Mountains",u"Haleakalā",u"Hawaii Volcanoes",u"Hot Springs",u"Isle Royale",
										 u"Joshua Tree",u"Katmai",u"Kenai Fjords",u"Kings Canyon",u"Kobuk Valley",u"Lake Clark",u"Lassen Volcanic",u"Mammoth Cave",
										 u"Mesa Verde",u"Mount Rainier",u"North Cascades",u"Olympic",u"Petrified Forest",u"Redwood",u"Rocky Mountain",u"Saguaro",
										 u"Sequoia",u"Shenandoah",u"Theodore Roosevelt",u"Virgin Islands",u"Voyageurs",u"Wind Cave",u"Yellowstone",u"Yosemite",u"Zion",NULL }; // PARK_MONUMENT
	printIdentities(parks);
	lpchar_t* rivers[] = { u"Alsek",u"Apalachicola",u"Chattahoochee",u"Flint",u"Colorado",u"Columbia",u"Okanagan",u"Kettle",
										u"Pend Oreille",u"Kootenay",u"Canoe",u"Kicking Horse",u"Dean",u"Embudo River",u"Fraser",u"Pitt",
										u"Thompson",u"Chilcotin",u"Quesnel",u"Nechako",u"Liard",u"Mackenzie",u"Liard River",u"Slave",
										u"Peace",u"Athabasca",u"Mississippi",u"Missouri",u"Yellowstone River",u"Platte River",u"Ohio",u"Nass",
										u"Rio Grande",u"Sacramento",u"Pit",u"Feather",u"Saskatchewan",u"Skagit",u"Skeena",u"Babine",
										u"Bulkley",u"Morice",u"Kitwanga",u"Zymoetz",u"Squamish",u"St. Johns",u"St. Lawrence",u"Ottawa River",u"Yukon",NULL }; // RIVER_LAKE_WATERWAY
	printIdentities(rivers);
	lpchar_t* cities[] = { u"New York",u"Los Angeles",u"Chicago",u"Houston",u"Philadelphia",u"Phoenix",u"San Antonio",u"San Diego",u"Dallas",u"San Jose",NULL }; // US_CITY_TOWN_VILLAGE
	printIdentities(cities);
	lpchar_t* states[] = { u"Arizona",u"California",u"Florida",u"Illinois",u"Indiana",u"New York",u"North Carolina",u"Ohio",u"Pennsylvania",u"Texas",NULL }; //	US_STATE_TERRITORY_REGION
	printIdentities(states);
	lpchar_t* worldCities[] = { u"Paris",u"Marseille",u"Lyon",u"Toulouse",u"Nice",u"Nantes",u"Strasbourg",u"Montpellier",u"Bordeaux",u"Lille",NULL }; // WORLD_CITY_TOWN_VILLAGE
	printIdentities(worldCities);

	printIdentity(u"Paul_Krugman");
	printIdentity(u"Irving_Berlin");
	printIdentity(u"Sting");
	printIdentity(u"IMG");
	printIdentity(u"AMT");
	printIdentity(u"Blondie");
	printIdentity(u"Darrell_Hammond");
	printIdentity(u"Curveball");
	printIdentity(u"Jay-Z");
	printIdentity(u"International_Management_Group");
	printIdentity(u"IMG");
	printIdentity(u"U.S._Mint");
	printIdentity(u"3M");
	printIdentity(u"Merrill_Lynch");
	printIdentity(u"WWE");
	printIdentity(u"Sago_Mine");
	printIdentity(u"Robert_Blake");
	printIdentity(u"March_Madness");
	printIdentity(u"Harriet_Miers");
	printIdentity(u"USS_Abraham_Lincoln");
	printIdentity(u"Abraham_Lincoln");
	printIdentity(u"Dulles_Airport");
	printIdentity(u"Boston_Pops");
	printIdentity(u"Cunard_Cruise_Lines");
	printIdentity(u"2004_Baseball_World_Series");
	printIdentity(u"Baseball_World_Series");
	printIdentity(u"World_Series");
	printIdentity(u"Jeopardy");
	printIdentity(u"Harry_Potter_and_the_Goblet_of_Fire");
	printIdentity(u"Jasper_Fforde");
	printIdentity(u"Guinness_Brewery");
	printIdentity(u"Michael_Brown");
	printIdentity(u"Ella_Fitzgerald");
	printIdentity(u"CSPI");
	printIdentity(u"Fulbright_Program");
	printIdentity(u"Mohammed");
	printIdentity(u"Lyme_disease");
	printIdentity(u"American_Girl");
	printIdentity(u"Kurt_Weill");
	printIdentity(u"House_of_Chanel");
	printIdentity(u"British_American_Tobacco");
	printIdentity(u"BAT");
	printIdentity(u"Buffalo_Soldiers");
	printIdentity(u"2005_DARPA_Grand_Challenge");
	printIdentity(u"DARPA_Grand_Challenge");
	printIdentity(u"Egypt");
	printIdentity(u"2005_World_Snooker_Championships");
	printIdentity(u"World_Snooker_Championships");
	printIdentity(u"Teenage_Mutant_Ninja_Turtles");
	printIdentity(u"TMNT");
	printIdentity(u"Marsupial");
	printIdentity(u"Kumquat");
	printIdentity(u"Ayn_Rand");
	printIdentity(u"Alan_Greenspan");
	printIdentity(u"Mahmud_Ahmadinejad");
	printIdentity(u"Rafik_Hariri");
	exit(0);
}


// TEST_CODE twin of getDBPediaPath using testWebPath.  Checks buffer for
// "SPARQL compiler" even though testWebPath may not have filled buffer.
int cOntology::testDBPediaPath(int where, lpwstring webAddress, lpwstring& buffer, lpwstring epath)
{
	LFS
		//int timer=clock(); 	
		int bw = -1;
	while ((bw = epath.find_first_of(u"/*?\"<>|,&-")) != lpwstring::npos)
		epath[bw] = u'_';
	lpwstring filePathOut, headers;
	int retValue = testWebPath(where, webAddress, epath, u"dbPediaCache", filePathOut, headers);
	if (buffer.find(u"SPARQL compiler") != lpwstring::npos)
	{
		lp_wremove(filePathOut.c_str());
		lplog(LOG_ERROR, u"PATH %s:\n%s", epath.c_str(), buffer.c_str());
		return -1;
	}
	return retValue;
}
#endif