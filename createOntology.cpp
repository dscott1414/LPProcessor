#include <windows.h>
#include <io.h>
#include "word.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "direct.h"
#include "time.h"
#include <errno.h>
#include <Winhttp.h>
#include <functional>
#include "profile.h"
#include "ontology.h"
#include "mysqldb.h"
#include "mysqld_error.h"
#include "internet.h"

wstring basehttpquery = L"http://localhost:8890/sparql?default-graph-uri=http%3A%2F%2Fdbpedia.org&query=";
wstring decodedbasehttpquery = L"http://localhost:8890/sparql?default-graph-uri=http://dbpedia.org&query=";
//wstring basehttpquery=L"http://dbpedia.org/sparql?default-graph-uri=http%3A%2F%2Fdbpedia.org&query=";
wstring prefix_foaf=L"PREFIX+foaf%3A+%3Chttp%3A%2F%2Fxmlns.com%2Ffoaf%2F0.1%2F%3E%0D%0A";
wstring prefix_colon=L"PREFIX+%3A+%3Chttp%3A%2F%2Fdbpedia.org%2Fresource%2F%3E%0D%0A";
wstring prefix_owl_ontology=L"PREFIX+dbpedia-owl%3A+%3Chttp%3A%2F%2Fdbpedia.org%2Fontology%2F%3E%0D%0A";
wstring prefix_dbpedia=L"PREFIX+dbpedia%3A+%3Chttp%3A%2F%2Fdbpedia.org%2F%3E%0D%0A";
wstring selectWhere=L"SELECT+%3Fv+%0D%0AWHERE+%7B%0D%0A";
#define MAX_BUF 2000000
#define MAX_LEN 2048


MYSQL Ontology::mysql;
set<string> Ontology::rejectCategories;
bool Ontology::cacheRdfTypes=true;
bool Ontology::alreadyConnected=false;
bool Ontology::forceWebReread=true;
extern int logDetail;
unordered_map <wstring, dbs> Ontology::dbPediaOntologyCategoryList;
unordered_map<wstring, vector <cTreeCat *> > Ontology::rdfTypeMap; // protected with rdfTypeMapSRWLock
unordered_map<wstring, int > Ontology::rdfTypeNumMap; // protected with rdfTypeMapSRWLock
bool Ontology::superClassesAllPopulated;

/***
  Read N3 files (begin) - used for reading UMBEL ontology
***/
wchar_t * readTillEndOfN3String(wchar_t *s)
{
	s = s + 1;
	while (true)
	{
		wchar_t *s4 = wcschr(s + 1, '"');
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
		s = wcschr(s + 1, ' ');
	return s;
}

wchar_t *readTillEndOfTripleString(wchar_t *s1, FILE *fp, wchar_t *buffer, int &line, wstring &mapTo)
{
	wchar_t *s2 = wcsstr(s1 + 4, L"\"\"\"");
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
	for (line++; fgetws(buffer, MAX_BUF, fp); line++)
	{
		if (buffer[wcslen(buffer) - 1] == '\n')
			buffer[wcslen(buffer) - 1] = 0;
		s2 = wcsstr(buffer, L"\"\"\"");
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

bool readN3OnePropertyLine(wstring relation,wstring mapFrom, unordered_map < wstring, unordered_map <wstring, set< wstring > > > &triplets, FILE *fp, wchar_t *buffer, int &line, int &nonConformingLines)
{
	for (line++; fgetws(buffer, MAX_BUF, fp); line++)
	{
		for (int I = 0; buffer[I]; I++)
			if (buffer[I] == L'\t')
				buffer[I] = L' ';
		// must be one space, after initial white space
		int index2 = wcsspn(buffer, L" ");
		if (!index2)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		wchar_t *sm1 = buffer + index2 - 1;
		wchar_t *sm2;
		if (*(sm1 + 1) == '"')
		{
			sm2 = readTillEndOfN3String(sm1);
		}
		else
			sm2 = wcschr(sm1 + 1, ' ');
		if (!sm2)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		*sm2 = 0;
		if (*(sm2 + 1) == ',')
		{
			triplets[relation][mapFrom].insert(sm1 + 1);
			continue;
		}
		else if (*(sm2 + 1) != ';' && *(sm2 + 1) != '.')
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s [%d %c]", line, nonConformingLines, __LINE__, buffer, index2, sm1[1]);
			nonConformingLines++;
			continue;
		}
		triplets[relation][mapFrom].insert(sm1 + 1);
		lplog(LOG_WIKIPEDIA, L"%d:line #%d: %s %s %s", __LINE__,line, relation.c_str(),mapFrom.c_str(),sm1 + 1);
		return (*(sm2 + 1) == '.');
	}
	return false;
}

int readN3TwoPropertyLine(wchar_t *path,wstring mapFrom, unordered_map < wstring, unordered_map <wstring, set< wstring > > > &triplets, FILE *fp, wchar_t *buffer, int &line, int &nonConformingLines)
{
	for (line++; fgetws(buffer, MAX_BUF, fp); line++)
	{
		for (int I = 0; buffer[I]; I++)
			if (buffer[I] == L'\t')
				buffer[I] = L' ';
		if (buffer[wcslen(buffer) - 1] == '\n')
			buffer[wcslen(buffer) - 1] = 0;
		if (buffer[0] == 0)
			continue;
		if (buffer[0] == '#')
			continue;
		// must be two spaces, after initial white space
		int index = wcsspn(buffer, L" ");
		if (buffer[index]==0)
			continue;
		if (!index)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line,nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		wchar_t *s1 = wcschr(buffer+index, ' ');
		if (!s1)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line,nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		*s1 = 0;
		wstring relation = buffer + index,mapTo;
		wchar_t *s2;
		if (*(s1 + 1) == '"')
		{
			if (s1[2] == '"' && s1[3] == '"')
			{
				s2=readTillEndOfTripleString(s1,fp,buffer,line,mapTo);
				//lplog(LOG_WIKIPEDIA, L"line #%d: triple string detected:(code line %d) \n**mapTo:\n%s\n**s2:\n%s\n**buffer:\n%s\n**END", line, __LINE__, mapTo.c_str(),s2,buffer);
			}
			else
				s2 = readTillEndOfN3String(s1);
		}
		else
			s2 = wcschr(s1 + 1, ' ');
		if (!s2)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line,nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		*s2 = 0;
		if (mapTo.empty())
			mapTo = s1 + 1;
		triplets[relation][mapFrom].insert(mapTo);
		lplog(LOG_WIKIPEDIA, L"%d:%s:line #%d: relation=%s mapFrom=%s mapTo=%s", __LINE__, path,line, relation.c_str(), mapFrom.c_str(), mapTo.c_str());
		bool appendingDef = false,finished=false;
		if (*(s2 + 1) == ',' && s2[wcslen(s2 + 1)] != ';' && s2[wcslen(s2 + 1)] != '.') // skips inline ,
		{
			if (*(s2 + 2) != 0)
			{
				lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line, nonConformingLines, __LINE__, buffer);
				nonConformingLines++;
				continue;
			}
			appendingDef = true;
		}
		else if (*(s2 + 1) == '.' || s2[wcslen(s2 + 1)] == '.') // skips inline ,
		{
			finished = true;
		}
		else if (*(s2 + 1) != ';' && s2[wcslen(s2+1)] != ';') // skips inline ,
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line,nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		// skos:altLabel "AB 205"@en ,
		//               "AB-205"@en ;
		if (appendingDef)
		{
			finished=readN3OnePropertyLine(relation,mapFrom, triplets, fp, buffer, line, nonConformingLines);
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
int readN3FileTypeIntoRelationsArray(wchar_t *path, unordered_map < wstring, unordered_map <wstring, set< wstring > > > &triplets)
{
	FILE *fp = _wfopen(path, L"r");
	if (!fp)
	{
		lplog(LOG_FATAL_ERROR, L"%s file not found.",path);
		return -1;
	}
	wchar_t buffer[MAX_BUF];
	int line;
	int nonConformingLines = 0;
	for (line = 1; fgetws(buffer, MAX_BUF, fp); line++)
	{
		if (buffer[0] == '@')
			continue;
		if (buffer[0] == '#')
			continue;
		if (buffer[wcslen(buffer)-1]=='\n')
			buffer[wcslen(buffer) - 1] = 0;
		if (buffer[0] == 0)
			continue;
		for (int I = 0; buffer[I]; I++)
			if (buffer[I] == L'\t')
				buffer[I] = L' ';
		// must be three spaces
		wchar_t *s1 = wcschr(buffer, ' ');
		if (!s1)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line,nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		wchar_t *s2 = wcschr(s1 + 1, ' ');
		if (!s2)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line,nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		wchar_t *s3;
		wstring mapTo;
		if (*(s2 + 1) == '"')
		{
			if (s2[2] == '"' && s2[3] == '"')
			{
				s3 = readTillEndOfTripleString(s2, fp, buffer, line, mapTo);
				//lplog(LOG_WIKIPEDIA, L"line #%d: triple string detected:(code line %d) \n**mapTo:\n%s\n**s2:\n%s\n**buffer:\n%s\n**END", line, __LINE__, mapTo.c_str(),s2,buffer);
			}
			else
				s3 = readTillEndOfN3String(s2);
		}
		else
			s3 = wcschr(s2 + 1, ' ');
		if (!s3)
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) %s", line,nonConformingLines, __LINE__, buffer);
			nonConformingLines++;
			continue;
		}
		bool inTwoFieldPerLineProperty = false, inOneFieldPerLineProperty = false;
		if (*(s3 + 1) == ';' || s3[wcslen(s3) - 1] == ';') // skips inline ,
			inTwoFieldPerLineProperty = true;
		else if (*(s3 + 1) == ',')
			inOneFieldPerLineProperty = true;
		else if (*(s3 + 1) != '.')
		{
			lplog(LOG_WIKIPEDIA, L"line #%d: Nonconforming #%d:(code line %d) [%c %d] [%c %d] %s", line,nonConformingLines, __LINE__, *(s2+1), s2-buffer, *(s3 + 1), s3-buffer, buffer);
			nonConformingLines++;
			continue;
		}
		*s1 = *s2 = *s3 = 0;
		wstring mapFrom = buffer;
		wstring relation = s1 + 1;
		if (mapTo.empty())
			mapTo = s2 + 1;
		triplets[relation][mapFrom].insert(mapTo);
		lplog(LOG_WIKIPEDIA, L"%d:line #%d: %s %s %s", __LINE__, line, relation.c_str(), mapFrom.c_str(), mapTo.c_str());
		if (inTwoFieldPerLineProperty)
			readN3TwoPropertyLine(path,mapFrom, triplets, fp, buffer, line, nonConformingLines);
		if (inOneFieldPerLineProperty)
			readN3OnePropertyLine(relation, mapFrom, triplets, fp, buffer, line, nonConformingLines);
	}
	lplog(LOG_WIKIPEDIA, L"relations in %s LIST: Nonconforming lines %d: total lines %d %d%%", path, nonConformingLines, line, nonConformingLines * 100 / line);
	for (unordered_map < wstring, unordered_map <wstring, set<wstring>> >::iterator tbegin = triplets.begin(); tbegin != triplets.end(); tbegin++)
		lplog(LOG_WIKIPEDIA, L"%d:FINAL UMBEL %s:%d", __LINE__,tbegin->first.c_str(), tbegin->second.size());
	return 0;
}

int scanDirectory(wchar_t *basepath,wchar_t *extension, unordered_map < wstring, unordered_map <wstring, set< wstring > > > &triplets)
{
	WIN32_FIND_DATA FindFileData;
	wchar_t path[1024];
	wsprintf(path, L"%s\\*.*", basepath, extension);
	HANDLE hFind = FindFirstFile(path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		wprintf(L"FindFirstFile failed on directory %s (%d)\r", path, (int)GetLastError());
		return -1;
	}
	do
	{
		if (FindFileData.cFileName[0] == '.') continue;
		wchar_t completePath[1024];
		wsprintf(completePath,L"%s\\%s",basepath, FindFileData.cFileName);
		if ((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			scanDirectory(completePath,extension,triplets);
		else
		{
			wchar_t *ext = wcsrchr(completePath, '.');
			if (!wcscmp(ext,extension))
				readN3FileTypeIntoRelationsArray(completePath,triplets);
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);
	FindClose(hFind);
	return 0;
}

/***
	Read N3 files (END)
***/

// cut off YAGO and UMBEL category numbers
void Ontology::cutFinalDigits(wstring &cat)
{ LFS
		int I;
		if (cat.length()>0 && iswdigit(cat[cat.length()-1]))
		{
			for (I=cat.length()-1; I>=0 && iswdigit(cat[I]); I--);
			if (I>0 && iswdigit(cat[I+1]))
				cat=cat.substr(0,I+1);
		}
}

/*
DBPEDIA START
*/
// decode dbpedia URL for calling HTTP API into virtuoso
wstring Ontology::decodeURL(wstring input,wstring &decodedURL)
{ LFS
	decodedURL.clear();
	for (int I=0; input[I]; I++)
		if (input[I]==L'%')
		{
			int ch=0;
			ch+=input[I+1]-((iswalpha(input[I+1])) ? 'A'-10:'0');
			ch<<=4;
			ch+=input[I+2]-((iswalpha(input[I+2])) ? 'A'-10:'0');
			decodedURL+=ch;
			I+=2;
		}
		else if (input[I]==L'+')
			decodedURL+=L' ';
		else
			decodedURL+=input[I];
	return decodedURL;
}

int Ontology::getDBPediaPath(int where,wstring webAddress,wstring &buffer,wstring epath)
{ LFS
	//int timer=clock(); 	
	int bw=-1;
	while ((bw=epath.find_first_of(L"/*?\"<>|,&-"))!=wstring::npos)
		epath[bw]=L'_';
	wstring filePathOut,headers;
	int retValue=Internet::getWebPath(where,webAddress,buffer,epath,L"dbPediaCache",filePathOut,headers,0,false,true,forceWebReread);
	if (buffer.find(L"SPARQL compiler")!=wstring::npos)
	{
		_wremove(filePathOut.c_str());
		lplog(LOG_ERROR,L"PATH %s:\n%s",epath.c_str(),buffer.c_str());
		return -1;
	}
	return retValue;
}

void Ontology::initialize()
{
	char *commonMeaninglessCategories[]={ "topic","track","document","edition","word","image","episode","title","subject","term","category","content","focus","type","base",NULL };
	for (int cmc=0; commonMeaninglessCategories[cmc]; cmc++) rejectCategories.insert(commonMeaninglessCategories[cmc]);
	superClassesAllPopulated=false;
	fillOntologyList(false);
}

unordered_map <wstring, dbs>::iterator Ontology::findCategory(wstring &icat)
{ LFS
		// put into lower case
	wstring cat=icat;
	transform (cat.begin (), cat.end (), cat.begin (), (int(*)(int)) tolower);
	// and remove trailing numbers (important for YAGO)
	// digits must be continuous and from the end
	cutFinalDigits(cat);
	return dbPediaOntologyCategoryList.find(cat);
}

// escape quote in object
void 	adjustQuote(wstring &begin,wstring &object,wstring &webAddress)
{ LFS
	if (object.find(L"'")==wstring::npos && object.find(L"%27")==wstring::npos) return;
	size_t firstSpace=webAddress.find_last_of(L' ',begin.length());
	if (firstSpace==wstring::npos) 
	{
		firstSpace=webAddress.find_last_of(L'+',begin.length());
		int colon=webAddress.rfind(L"%3A",begin.length());
		if (colon>firstSpace)
			firstSpace=colon;
	}
	size_t secondSpace=webAddress.find_first_of(L' ',begin.length()+object.length());
	if (secondSpace==wstring::npos) 
		secondSpace=webAddress.find_first_of(L'+',begin.length()+object.length());
	// find previous space
	if (firstSpace==wstring::npos || secondSpace==wstring::npos) return;
	webAddress.insert(firstSpace,L"\"");
	webAddress.insert(secondSpace+1,L"\"");
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
void encodeURL(wstring winput,wstring &wencodedURL)
{ LFS
	string input;
	wTM(winput,input);
	string encodedURL;
	for (unsigned int I=0; I<input.length(); I++)
	{
		if (!isalnum((unsigned char)input[I]))
		{
			encodedURL+='%';
			int lower=((unsigned char)input[I])&15;
			int higher=((unsigned char)input[I])>>4;
			encodedURL+=(higher<10) ? '0'+higher : 'A'+(higher-10);
			encodedURL+=(lower<10) ? '0'+lower : 'A'+(lower-10);
		}
		else
			encodedURL+=input[I];
	}
	mTW(encodedURL,wencodedURL);
}

wstring getDescriptionString(wstring label)
{
	return basehttpquery + prefix_foaf + prefix_colon +
		L"SELECT+DISTINCT+%3Fv1+%3Fv2+%3Fv3+%3Fv4+%0D%0AWHERE+%7B%0D%0A+"
		L"+%7B+" + label + L"+%3Chttp%3A%2F%2Fwww.w3.org%2F2002%2F07%2Fowl%23sameAs%3E+%3Fs+.+%7D%0D%0A" // OPTIONAL removed
		L"OPTIONAL+%7B+%3Fs+%3Chttp%3A%2F%2Fwikidata.dbpedia.org%2Fontology%2Fabstract%3E+%3Fv1+.+%7D%0D%0A"
		L"OPTIONAL+%7B+%3Fs+%3Chttp%3A%2F%2Fwww.w3.org%2F2000%2F01%2Frdf-schema%23comment%3E+%3Fv2+.+%7D%0D%0A"
		L"OPTIONAL+%7B+" + label + L"+foaf%3Ahomepage+%3Fv3+.+%7D%0D%0A"
		L"OPTIONAL+%7B+" + label + L"+foaf%3Apage+%3Fv4+.+%7D%0D%0A%7D";
}

// these queries are not combined to look up properties of more than one object because it would go over the time limit imposed by the VIRTUOSO server.
// when running SPARQL queries in Virtuoso Conductor ISQL, you must prepend the query with SPARQL, so it must go before even the PREFIX (and not anywhere else)
int Ontology::getDescription(wstring label, wstring objectName, wstring &abstract, wstring &comment, wstring &infoPage)
{
	LFS
	// cl=http://dbpedia.org/class/yago/HealthProfessional110165109
	replace(label.begin(), label.end(), L' ', L'_');
	// replace % with %25:
	wstring rlabel;
	for (unsigned int I = 0; I < label.length(); I++)
		if (I + 2 < label.length() && label[I] == L'%' && label[I + 1] == L'2' && (label[I + 2] == L'8' || label[I + 2] == L'9'))
			rlabel += L"%25";
		else
			rlabel += label[I];
	label = rlabel;
	if (label.find_first_of(L"'#()") != wstring::npos || label.find(L"%") != wstring::npos)
	{
		label = L"<http://dbpedia.org/resource/" + label + L">";
	}
	else 
	{
		label = L"%3A" + label;
	}
	wstring dbPediaQueryString = getDescriptionString(label),temp,buffer;
	if (rdfDetail)
		lplog(LOG_WIKIPEDIA, L"%s\nENCODED WEBADDRESS:%s\nDECODED WEBADDRESS:%s",
			objectName.c_str(), dbPediaQueryString.c_str(), decodeURL(dbPediaQueryString, temp).c_str() + decodedbasehttpquery.length());
	int numRows = 0;
	objectName += L"_getDescription.xml";
	if (!Internet::readPage(dbPediaQueryString.c_str(), buffer))
	//if (!getDBPediaPath(0,dbPediaQueryString,buffer,objectName))
	{
		// get number of rows
		for (size_t w = 0; w < buffer.size(); numRows++, w++)
			if ((w = buffer.find(L"<binding name=\"v1\">", w)) == wstring::npos)
				break;
		if (numRows > 0)
		{
			size_t pos = 0, pos2 = 0;
			wstring tmpstr;
			if (firstMatch(buffer, L"<binding name=\"v1\">", L"</binding>", pos, tmpstr, false) >= 0)
				firstMatch(tmpstr, L"<literal xml:lang=\"en\">", L"</literal>", pos2, abstract, false);
			if (firstMatch(buffer, L"<binding name=\"v2\">", L"</binding>", pos, tmpstr, false) >= 0)
			{
				pos2 = 0;
				firstMatch(tmpstr, L"<literal xml:lang=\"en\">", L"</literal>", pos2, comment, false);
			}
			if (firstMatch(buffer, L"<binding name=\"v3\">", L"</binding>", pos, tmpstr, false) >= 0)
			{
				pos2 = 0;
				firstMatch(tmpstr, L"<uri>", L"</uri>", pos2, infoPage, false);
			}
		}
	}
	return numRows;
}

// get abstract, comment and wikipedia page, all as optional (none are required to appear)
int Ontology::getDescription(unordered_map <wstring, dbs>::iterator cli)
{ LFS
	if (cli->second.descriptionFilled>=0) return cli->second.descriptionFilled;
	int numRows=getDescription(cli->second.compactLabel,cli->first,cli->second.abstractDescription,cli->second.commentDescription,cli->second.infoPage);
	return cli->second.descriptionFilled=numRows;
}

unordered_map <wstring, dbs>::iterator Ontology::findAnyYAGOSuperClass(wstring cl)
{ LFS
	// cl=http://dbpedia.org/class/yago/HealthProfessional110165109
	wstring begin=basehttpquery+L"PREFIX+rdfs%3A+%3Chttp%3A%2F%2Fwww.w3.org%2F2000%2F01%2Frdf-schema%23%3E%0D%0ASELECT+%3Fv%0D%0AWHERE+%7B%0D%0A++%7B+%3Chttp%3A%2F%2Fdbpedia.org%2Fclass%2Fyago%2F";
	wstring end=L"%3E+rdfs%3AsubClassOf+%3Fv+%7D%0D%0A%7D";
	wstring buffer;
	if (!getDBPediaPath(0,begin+cl+end,buffer,cl+L"_findSuperClass.xml"))
	{
		wstring cat=cl,uri;
		transform (cat.begin (), cat.end (), cat.begin (), (int(*)(int)) tolower);
		cutFinalDigits(cat);
		dbPediaOntologyCategoryList[cat].compactLabel=cl;
		unordered_map <wstring, dbs>::iterator cli=dbPediaOntologyCategoryList.find(cat);
		cli->second.ontologyType=YAGO_Ontology_Type;
		cli->second.ontologyHierarchicalRank=100;
		// <uri>http://dbpedia.org/class/yago/Sailor110546633</uri>
		for (size_t pos=0; firstMatch(buffer,L"<uri>",L"</uri>",pos,uri,false)!=wstring::npos; )
		{
			wstring scat=uri.c_str()+wcslen(L"http://dbpedia.org/class/yago/");
			cli->second.superClasses.push_back(scat);
			lplog(LOG_WIKIPEDIA,L"%s:%s:%s:%s",L"YSC",uri.c_str(),cl.c_str(),scat.c_str());
			if (findCategory(scat)==dbPediaOntologyCategoryList.end())
				findAnyYAGOSuperClass(scat);
		}
		return cli;
	}
	return dbPediaOntologyCategoryList.end();
}

// c. derive a combined ranking of a hierarchy from dbPedia, UMBEL, YAGO, OpenGIS
int Ontology::fillRanks(int ontologyType)
{ LFS
	// this assumes that a class has one or zero super classes.  These superclasses may or may not exist as labels.
	int numLoops=0,noSuperClasses,notFoundSuperClasses,ranked,numEntries,newPercent,oldPercent;
	wstring tmpstr;
	wprintf(L"\n");
	for (int entriesFilled=1; entriesFilled>0; numLoops++)
	{
		entriesFilled=noSuperClasses=notFoundSuperClasses=ranked=numEntries=0,newPercent=0,oldPercent=-1;
		for (unordered_map <wstring, dbs>::iterator cli=dbPediaOntologyCategoryList.begin(),clEnd=dbPediaOntologyCategoryList.end(); cli!=clEnd; cli++,numEntries++)
		{
			newPercent=numEntries*100/dbPediaOntologyCategoryList.size();
			if (oldPercent!=newPercent)
			{
				wprintf(L"%03d (%06d/%06zd) ontology items ranked\r",newPercent,numEntries,dbPediaOntologyCategoryList.size());
				oldPercent=newPercent;
			}
			if (cli->second.ontologyHierarchicalRank==100 && cli->second.ontologyType==ontologyType)
			{
				if (cli->second.superClasses.empty())
				{
					cli->second.ontologyHierarchicalRank=0;
					entriesFilled++;
					noSuperClasses++;
				}
				else 
				{
					unordered_map <wstring, dbs>::iterator scli;
					for (vector <wstring>::iterator sci = cli->second.superClasses.begin(), sciEnd = cli->second.superClasses.end(); sci != sciEnd; sci++)
					{
						scli = dbPediaOntologyCategoryList.find(*sci);
						if (scli == dbPediaOntologyCategoryList.end())
							scli = findCategory(*sci);
						if (scli == dbPediaOntologyCategoryList.end() && (*sci!=L"orphans" || cli->second.ontologyType!=UMBEL_Ontology_Type))
							lplog(L"looking for superclass %s of object %s failed.", sci->c_str(),cli->second.toString(tmpstr,cli->first).c_str());
						else
							break;
						//if (scli != dbPediaOntologyCategoryList.end() && (scli = findAnyYAGOSuperClass(scli->second.compactLabel)) != dbPediaOntologyCategoryList.end())
						//	break;
					}
					if (scli==dbPediaOntologyCategoryList.end())
					{
						cli->second.ontologyHierarchicalRank=0;
						entriesFilled++;
						notFoundSuperClasses++;
					}
					else if (scli->second.ontologyHierarchicalRank!=100)
					{
						cli->second.ontologyHierarchicalRank=scli->second.ontologyHierarchicalRank+1;
						entriesFilled++;
						ranked++;
					}
				}
			}
		}
		wprintf(L"%03d: total %07d:%07d noSuperClasses %06d notFoundSuperClasses %06d ranked\n",numLoops,entriesFilled,noSuperClasses,notFoundSuperClasses,ranked);
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

void stripEndOfLine(wchar_t *s,wchar_t &ch)
{ LFS
	while (*s && (s[wcslen(s)-1]=='\n' || s[wcslen(s)-1]=='\r' || iswspace(s[wcslen(s)-1])))
		s[wcslen(s)-1]=0;
	if (s[wcslen(s)-1]==L'.' || s[wcslen(s)-1]==';' || s[wcslen(s)-1]==L',')
	{
		ch=s[wcslen(s)-1];
		s[wcslen(s)-1]=0;
		while (*s && iswspace(s[wcslen(s)-1]))
			s[wcslen(s)-1]=0;
	}
	else
		ch=0;
}

/*
labelWithSpace        <http://umbel.org/umbel/rc/WeatherAttributes_Weather_Topic> rdf:type owl:Class ;
superClasses		    																												rdfs:subClassOf :TopicsCategories ,
																																						<http://umbel.org/umbel/rc/Weather_Topic> ;
				            																										skos:definition "A CycVocabularyTopic and a KBDependentCollection."@en ;
compactLabel						    																								skos:compactLabel "weather attributes weather topic"@en .
*/

wstring stripUmbel(wstring umbelClass,wstring &compactLabel,wstring &labelWithSpace,int &UMBELType)
{
	wchar_t *prefixes[] = { L"<http://umbel.org/umbel/rc/", L"<http://umbel.org/umbel#", L"<http://schema.org/", L"<http://www.geonames.org/ontology#", L"<http://dbpedia.org/ontology/",
		L"<http://purl.org/dc/dcmitype/",		L"<http://purl.org/dc/terms/",		L"<http://purl.org/goodrelations/v1#",		L"<http://purl.org/openorg/",
		L"<http://usefulinc.com/ns/doap#",		L"<http://vocab.org/transit/terms/",		L"<http://www.w3.org/2003/01/geo/wgs84_pos#",		L"<http://www.w3.org/2004/02/skos/core#",
		L"<http://www.w3.org/2006/time#",		L"<http://www.w3.org/2006/timezone#",		L"<http://www.w3.org/ns/org#",		L"<http://xmlns.com/foaf/0.1/", 
		 0 };

	for (int pf = 0; prefixes[pf] != 0; pf++)
	{
		int w = umbelClass.find(prefixes[pf]);
		if (w >= 0)
		{
			w += wcslen(prefixes[pf]);
			int w2 = umbelClass.find(L">", w + 1);
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
		if (umbelClass[0] != L':')
			lplog(LOG_ERROR, L"Pattern not found: %s", umbelClass.c_str());
		else
			compactLabel = umbelClass;
	}
	if (compactLabel[0]==':')
		compactLabel = compactLabel.substr(1);
	wstring temp2;
	temp2 = compactLabel;
	// change _ to space
	std::replace(temp2.begin(), temp2.end(), L'_', L' ');
	labelWithSpace.clear();
	wchar_t pc=0;
	// insert space before capitals other than the first one, if there isn't already one
	for (wstring::iterator wi = temp2.begin(), wiEnd = temp2.end(); wi != wiEnd; wi++)
	{
		wstring::iterator next = wi + 1;
		// insert a space before all but the first capital, as long as there isn't already a space and the next character is not a capital (ACE Inhibitor)
		if (iswupper(*wi) && pc != 0 && iswalnum(pc) && pc!=L' ' && (next== wiEnd || !iswupper(*next)))
			labelWithSpace += L' ';
		pc = tolower(*wi);
		labelWithSpace += pc;
	}
	return labelWithSpace;
}

int Ontology::readUMBELNS()
{ LFS
	wchar_t path[1024];
	wsprintf(path,L"source\\lists\\umbel_superClassCache");
	int fd = _wopen(path, O_RDWR | O_BINARY);
	if (fd >= 0)
	{
		wchar_t version;
		::read(fd, &version, sizeof(version));
		_close(fd);
		if (version == UMBEL_CACHE_VERSION) // version
			return 0;
	}
	fd = _wopen(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0)
	{
		lplog(L"Cannot write umbel_superClassCache - %S.", _sys_errlist[errno]);
		return -1;
	}
	int where = 0, numUMBELClasses=0;
	wstring temp;
	// look for ###
	// skip one line, look for the same
	// skip one line, look for rdfs:subClass and take the rest of the line (end in ;?)
	// take more lines until the line ends in a ; or the line doesn't end in a ,
	unordered_map < wstring, unordered_map <wstring, set<wstring>> > triplets;
	scanDirectory(L"F:\\lp\\umbel downloads", L".n3",triplets);
	unordered_map <wstring, set<wstring>> subClasses = triplets[L"rdfs:subClassOf"];
	unordered_map <wstring, set<wstring>> superClasses = triplets[L"umbel:superClassOf"];
	wchar_t buffer[MAX_BUF];
	*((wchar_t *)buffer) = UMBEL_CACHE_VERSION;
	where += sizeof(wchar_t);
	for (unordered_map <wstring, set<wstring>>::iterator ti = superClasses.begin(); ti != superClasses.end(); ti++)
	{
		for (set<wstring>::iterator iSubClass = ti->second.begin(); iSubClass != ti->second.end(); iSubClass++)
		{
			lplog(LOG_WIKIPEDIA, L"UMBEL SUPERCLASS %s:%s", ti->first.c_str(), iSubClass->c_str());
			subClasses[*iSubClass].insert(ti->first);
		}
	}
	for (unordered_map <wstring, set<wstring>>::iterator ti = subClasses.begin(); ti != subClasses.end(); ti++)
	{
		wstring labelWithSpace,compactLabel;
		int UMBELType;
		stripUmbel(ti->first, compactLabel, labelWithSpace, UMBELType);
		if (!::copy(buffer, labelWithSpace, where, MAX_BUF) || // labelWithSpace
		    !::copy(buffer, compactLabel, where, MAX_BUF) || // compactLabel
				!::copy(buffer, UMBELType, where, MAX_BUF) || // UMBEL resource type
				!::copy(buffer, (int)ti->second.size(), where, MAX_BUF))
			lplog(LOG_FATAL_ERROR,L"Cannot create UMBEL cache - subclasses");
		lplog(LOG_WIKIPEDIA, L"WRITE UMBEL [%s]:%s->%s:%d", ti->first.c_str(),compactLabel.c_str(), labelWithSpace.c_str(), ti->second.size());
		for (set<wstring>::iterator iSuperClass = ti->second.begin(); iSuperClass != ti->second.end(); iSuperClass++)
		{
			stripUmbel(*iSuperClass, compactLabel, labelWithSpace, UMBELType);
			lplog(LOG_WIKIPEDIA, L"  WRITE UMBEL SC [%s]:%s->%s", iSuperClass->c_str(), compactLabel.c_str(), labelWithSpace.c_str());
			if (!::copy(buffer, labelWithSpace, where, MAX_BUF) ||
					!::copy(buffer, UMBELType, where, MAX_BUF))
				lplog(LOG_FATAL_ERROR,L"Cannot create UMBEL cache - superclasses"); 
		}
		if (where>MAX_BUF-40960)
		{
			::write(fd,buffer,where);
			where=0;
		}
	}
	if (where)
		::write(fd,buffer,where);
	close(fd);
	return numUMBELClasses;
}

bool Ontology::readUMBELSuperClasses()
{
	LFS
	int beginEntries = dbPediaOntologyCategoryList.size();
	readUMBELNS();
	IOHANDLE fd=_wopen(L"source\\lists\\umbel_superClassCache",O_RDWR|O_BINARY);
  if (fd<0) 
			return false;
  void *buffer;
  int bufferlen=filelength(fd);
  buffer=(void *)tmalloc(bufferlen+10);
  ::read(fd,buffer,bufferlen);
  close(fd);
  int where=0,line;
	wchar_t version;
	::read(fd, &version, sizeof(version));
	if (version == UMBEL_CACHE_VERSION) // version
		return 0;
	where += sizeof(wchar_t);
	for (line=0; where<bufferlen; line++)
	{
		wstring labelWithSpace,compactLabel;
		int UMBELType,numSuperClasses;
		vector <wstring> superClasses;
		vector <int> superClassUMBELTypes;
		if (!::copy(labelWithSpace, buffer, where, bufferlen)) return false;
		if (!::copy(compactLabel, buffer, where, bufferlen)) return false;
		if (!::copy(UMBELType, buffer, where, bufferlen)) return false;
		if (!::copy(numSuperClasses, buffer, where, bufferlen)) return false;
		for (int I=0; I<numSuperClasses; I++)
		{
			wstring superClass;
			int scType;
			if (!::copy(superClass, buffer, where, bufferlen)) return false;
			if (!::copy(scType, buffer, where, bufferlen)) return false;
			superClasses.push_back(superClass);
			superClassUMBELTypes.push_back(scType);
		}
		dbPediaOntologyCategoryList[labelWithSpace].superClasses = superClasses;
		dbPediaOntologyCategoryList[labelWithSpace].superClassResourceTypes = superClassUMBELTypes;
		unordered_map <wstring, dbs>::iterator cli=dbPediaOntologyCategoryList.find(labelWithSpace);
		cli->second.compactLabel = compactLabel;
		//getDescription(cli);  getDescription is only for the dbpedia ontology
		cli->second.numLine=line;
		cli->second.ontologyType=UMBEL_Ontology_Type;
		cli->second.resourceType = UMBELType;
	}
  tfree(bufferlen,buffer);
	fillRanks(UMBEL_Ontology_Type);
	lplog(LOG_WIKIPEDIA, L"UMBEL ontology inserted %d entries.", dbPediaOntologyCategoryList.size() - beginEntries);
	return where<=bufferlen;
}

wchar_t *bufferedGetws(wchar_t *s,int maxLen,int fd,wchar_t *fileBuffer,__int64 &bufferLength,__int64 &bufferOffset,__int64 &fileOffset,__int64 totalFileLength)
{ LFS
	__int64 I,totalOffset=fileOffset+bufferOffset;
	for (I=totalOffset; I<totalFileLength; I++)
	{
		if (I-fileOffset>=bufferLength-maxLen)
		{
			cProfile::counterEnd("bufferedGetws");
			fileOffset+=bufferLength;
			bufferLength=_read(fd,fileBuffer,MAX_BUF);
			cProfile::counterEnd("bufferedGetwsRead");
			if (bufferLength<=0)
				return NULL;
			bufferOffset=0;
			totalOffset=fileOffset+bufferOffset;
		}
		if (I-totalOffset>=maxLen)
			break;
		s[I-totalOffset]=fileBuffer[I-fileOffset];
		if (s[I-totalOffset]==L'\n')
		{
			I++; // skip
			break;
		}
	}
	s[I-(fileOffset+bufferOffset)]=0;
	bufferOffset=I-fileOffset;
	return s;
}

void transform(wchar_t *ic, wstring &nameIC)
{
	int lastContinuousDigit = 0;
	wchar_t *original = ic;
	for (; *ic; ic++)
	{
		if (ic != original && (iswupper(*ic) && !iswupper(ic[-1])) || (iswdigit(*ic) && !iswdigit(ic[-1])))
			nameIC += L" ";
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

void transform(char *ic, string &nameIC)
{
	int lastContinuousDigit = 0;
	char *original = ic;
	for (; *ic; ic++)
	{
		if (ic != original && (iswupper(*ic) && !iswupper(ic[-1])) || (iswdigit(*ic) && !iswdigit(ic[-1])))
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

int Ontology::readYAGOOntology()
{
	LFS
	int numYAGOEntries = 0, numSuperClasses = 0;
	if (readYAGOOntology(L"source\\lists\\yago_taxonomy.ttl", numYAGOEntries , numSuperClasses ) < 0 || readYAGOOntology(L"source\\lists\\yago_type_links.ttl", numYAGOEntries, numSuperClasses) < 0)
		return -1;
	int emptyYAGOSuperClasses = 0;
	for (unordered_map <wstring, dbs>::iterator cli = dbPediaOntologyCategoryList.begin(), clEnd = dbPediaOntologyCategoryList.end(); cli != clEnd; cli++)
		if (cli->second.ontologyType == YAGO_Ontology_Type && cli->second.compactLabel.empty())
			emptyYAGOSuperClasses++;
	lplog(LOG_WIKIPEDIA, L"YAGO ontology inserted %d entries (%d emptyYAGOSuperClasses).", numYAGOEntries, emptyYAGOSuperClasses);
	fillRanks(YAGO_Ontology_Type);
	return 0;
}

#define MAXYAGOBUF 5000000 // in char
int Ontology::readYAGOOntology(wchar_t *filepath, int &numYAGOEntries, int &numSuperClasses)
{
	LFS
	HANDLE fd = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ| FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	wstring tmpstr, tmpstr2, tmpstr3;
	if (fd == INVALID_HANDLE_VALUE)
	{
		lplog(LOG_FATAL_ERROR, L"YAGOOntology file %s not found.",filepath);
		return -1;
	}
	int line, bufferOffset = 0, bufferLength = 0; // fileOffset=0,
	__int64 totalFileLength, totalFileOffset = 0;
	if (!GetFileSizeEx(fd, (PLARGE_INTEGER)&totalFileLength))
		return -1;
	char fileBuffer[MAXYAGOBUF + 1];
	/* Read a line at a time until eof */
	// <http://dbpedia.org/class/yago/Aa114931472> <http://www.w3.org/2000/01/rdf-schema#compactLabel> "Aa"@en .  obsolete! yago_links.nt deprecated in dbpedia 3.7
	// <http://dbpedia.org/class/yago/Canal102947212> <http://www.w3.org/2002/07/owl#equivalentClass> <http://yago-knowledge.org/resource/wordnet_canal_102947212> . in yago_type_links.ttl
	// <http://dbpedia.org/class/yago/Aa114931472> <http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://dbpedia.org/class/yago/Lava114930989>. in yago_taxonomy.ttl
	int oldPercent = -1, newPercent = 0;
	for (line = 1; ; line++)
	{
		if (bufferOffset > bufferLength - maxCategoryLength)
		{
			if (bufferLength - bufferOffset)
				memcpy(fileBuffer, fileBuffer + bufferOffset, (int)(bufferLength - bufferOffset) * sizeof(char));
			bufferLength = bufferLength - bufferOffset;
			DWORD newBufferLength = 0;
			BOOL success = ReadFile(fd, fileBuffer + bufferLength, (int)(MAXYAGOBUF - bufferLength) * sizeof(char), &newBufferLength, NULL);
			if (!success)
				lplog(LOG_ERROR,L"%S:%d:%s",__FUNCTION__, __LINE__,lastErrorMsg().c_str());
			totalFileOffset += newBufferLength;
			if (newBufferLength <= 0 || !success)
				break;
			newBufferLength /= sizeof(char);
			fileBuffer[bufferLength = newBufferLength + bufferLength] = 0;
			bufferOffset = 0;
			newPercent = (int)(totalFileOffset * 100 / totalFileLength);
			if (newPercent != oldPercent)
				wprintf(L"%03d:YAGO Ontology %07d entries, %06d superclasses out of %08d lines\r", newPercent, numYAGOEntries, numSuperClasses, line);
			oldPercent = newPercent;
		}
		cProfile::counterBegin();
		char *s, *newLine = strchr(s = fileBuffer + bufferOffset, L'\n');
		if (newLine)
		{
			*newLine = 0;
			bufferOffset = (int)(newLine - fileBuffer + 1);
		}
		else
			bufferOffset = bufferLength;
		if (strncmp(s, "<http://dbpedia.org/class/yago/", strlen("<http://dbpedia.org/class/yago/")))
			continue;
		char *name = NULL;
		if ((name = firstMatch(s, "<http://dbpedia.org/class/yago/", ">")) == NULL)
		{
			//lplog(LOG_ERROR,L"Error parsing (1) dbPediaOntology category [no labelWithSpace] on line %d: %s",line,s);
			continue;
		}
		char *ic = name + strlen(name) + 2, *label, *superClass;
		string nameIC;
		transform(name, nameIC);
		int resourceType = 0;
		// wikicat people from tambov oblast
		if (nameIC.find("wikicat ") == 0)
		{
			nameIC = nameIC.substr(strlen("wikicat "));
			resourceType = 1;
		}
		// example: <http://yago-knowledge.org/resource/wordnet_carousel_102966193>
		if ((label = firstMatch(ic, "<http://www.w3.org/2002/07/owl#equivalentClass> <http://yago-knowledge.org/resource/", ">")) != NULL) // yago_type_links.ttl
		{
			// cut until the first _
			char *firstUnderscore = strchr(label, '_'),*lastUnderscore = strrchr(label, '_');
			if (firstUnderscore)
				label = firstUnderscore + 1;
			if (lastUnderscore)
				*lastUnderscore = 0;
			mTW(nameIC, tmpstr);
			dbPediaOntologyCategoryList[tmpstr].compactLabel = mTW(label, tmpstr2);
			unordered_map <wstring, dbs>::iterator cli = dbPediaOntologyCategoryList.find(tmpstr);
			cli->second.numLine = line;
			cli->second.ontologyType = YAGO_Ontology_Type;
			cli->second.resourceType = resourceType;
			numYAGOEntries++;
			continue;
		}
		if ((superClass = firstMatch(ic, "<http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://dbpedia.org/class/yago/", ">")) != NULL)
		{
			string superClassIC;
			transform(superClass, superClassIC);
			mTW(nameIC, tmpstr);
			dbPediaOntologyCategoryList[tmpstr].superClasses.push_back(mTW(superClassIC, tmpstr3));
			unordered_map <wstring, dbs>::iterator cli = dbPediaOntologyCategoryList.find(tmpstr);
			cli->second.numLine = line;
			cli->second.ontologyType = YAGO_Ontology_Type;
			numSuperClasses++;
			continue;
		}
		//lplog(LOG_ERROR,L"Error parsing (2) dbPediaOntology category [compactLabel and subclass not found] on line %d: %s",line,s);
	}
	CloseHandle(fd);
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
#define DBP_SCO L"<http://www.w3.org/2000/01/rdf-schema#subClassOf>"
#define DBP_LABEL L"<http://www.w3.org/2000/01/rdf-schema#label>"
#define DBP_COMMENT L"<http://www.w3.org/2000/01/rdf-schema#comment>"
#define DBP_PREFIX L"<http://dbpedia.org/ontology/"
#define DBP_PREFIX2 L"<http://www.w3.org/2002/07/owl#"
#define DBP_PREFIX3 L"<http://schema.org/"
#define DBP_PREFIX4 L"<http://www.ontologydesignpatterns.org/ont/dul/DUL.owl#"
int Ontology::readDbPediaOntology()
{ LFS
	unordered_map <int,wstring> lastRank;
	unordered_map <wstring, wstring> labelMap;
  FILE *fp=_wfopen(L"dbpedia_downloads\\dbpedia_2016-10.nt",L"rt");
  if (!fp) 
	{
		lplog(LOG_FATAL_ERROR,L"dbPedia Ontology file not found.");
		return -1;
	}
  wchar_t s[maxCategoryLength];
	int line, beginDbPediaEntries = dbPediaOntologyCategoryList.size();
	dbs dbp;
	dbp.ontologyType = dbPedia_Ontology_Type;
	dbp.ontologyHierarchicalRank = 1;
	dbp.numLine = 0;
	wstring currentEntry;
	// these super classes have been inexplicably dropped from the dbpedia ontology file
	wchar_t *droppedSuperClassesEntry[] = { L"Thing",L"Festival",L"MusicGroup",L"SocialPerson",L"Organization",L"Product",0 };
	wchar_t *droppedSuperClasses[] = { L"thing",L"festival",L"music group",L"social person",L"organization",L"product",0 };
	for (int I = 0; droppedSuperClasses[I]; I++)
	{
		dbp.compactLabel = droppedSuperClasses[I];
		labelMap[droppedSuperClassesEntry[I]] = droppedSuperClasses[I];
		dbPediaOntologyCategoryList[droppedSuperClasses[I]] = dbp;
	}
	/* Read a line at a time until eof */
	for (line = 1; fgetws(s, maxCategoryLength, fp); line++)
	{
		int eol = wcslen(s);
		if (s[eol - 1]=='\n') s[eol - 1] = 0;
		if (s[eol - 2] == '\r') s[eol - 2] = 0;
		if (s[0] == 0xFEFF)
		{// detect BOM
			memcpy(s, s + 1, wcslen(s + 1) * sizeof(*s));
			s[wcslen(s) - 1] = 0;
		}
		// get entry
		wchar_t *space = wcsstr(s, L"> ");
		if (!space || wcsncmp(s, DBP_PREFIX, wcslen(DBP_PREFIX)))
			continue;
		*space = 0;
		wstring entry = s + wcslen(DBP_PREFIX);
		if (entry != currentEntry)
		{
			if (currentEntry.length() > 0)
			{
				if (dbp.compactLabel.length() > 0)
				{
					wstring label = dbp.compactLabel;
					dbp.compactLabel = currentEntry;
					labelMap[currentEntry] = label;
					dbp.numLine = line;
					dbPediaOntologyCategoryList[label] = dbp;
				}
				else
					lplog(LOG_WIKIPEDIA, L"entry %s dropped.", currentEntry.c_str());
				dbp.superClasses.clear();
				dbp.compactLabel = L"";
			}
			currentEntry = entry;
		}
		// <http://dbpedia.org/ontology/BasketballLeague> <http://www.w3.org/2000/01/rdf-schema#subClassOf> <http://dbpedia.org/ontology/SportsLeague> .
		wchar_t *pf, *lt, *eq, *nextField=space+2;
		if (wcsstr(nextField, DBP_SCO))
		{
			if ((pf = wcsstr(nextField, DBP_PREFIX)) && (lt = wcschr(pf + wcslen(DBP_PREFIX), L'>')))
			{
				*lt = 0;
				dbp.superClasses.push_back(nextField + wcslen(DBP_SCO) + 1 + wcslen(DBP_PREFIX));
			}
			else if ((pf = wcsstr(nextField, DBP_PREFIX2)) && (lt = wcschr(pf + wcslen(DBP_PREFIX2), L'>')))
			{
				*lt = 0;
				dbp.superClasses.push_back(nextField + wcslen(DBP_SCO) + 1 + wcslen(DBP_PREFIX2));
			}
			else if ((pf = wcsstr(nextField, DBP_PREFIX3)) && (lt = wcschr(pf + wcslen(DBP_PREFIX3), L'>')))
			{
				*lt = 0;
				dbp.superClasses.push_back(nextField + wcslen(DBP_SCO) + 1 + wcslen(DBP_PREFIX3));
			}
			else if ((pf = wcsstr(nextField, DBP_PREFIX4)) && (lt = wcschr(pf + wcslen(DBP_PREFIX4), L'>')))
			{
				*lt = 0;
				dbp.superClasses.push_back(nextField + wcslen(DBP_SCO) + 1 + wcslen(DBP_PREFIX4));
			}
			else
				lplog(LOG_WIKIPEDIA, L"%s prefix not recognized", nextField);
		}
		// <http://dbpedia.org/ontology/BasketballLeague> <http://www.w3.org/2000/01/rdf-schema#compactLabel> "basketball league"@en .
		if (!wcsncmp(nextField, DBP_LABEL, wcslen(DBP_LABEL)) && (eq = wcsstr(nextField + wcslen(DBP_LABEL), L"\"@en .")))
		{
			*eq = 0;
			dbp.compactLabel = nextField + wcslen(DBP_LABEL) + 2;
		}
		if (!wcsncmp(nextField, DBP_COMMENT, wcslen(DBP_COMMENT)) && (eq = wcsstr(nextField + wcslen(DBP_COMMENT), L"\"@en .")))
		{
			*eq = 0;
			dbp.commentDescription = nextField + wcslen(DBP_COMMENT) + 2;
		}
	}
	fclose(fp);
	// transform super classes - each superclass like BasketballLeague is moved to its compactLabel 'basketball league' so that it can be found
	// by looking up by the words in the text and also for the derivation by ontological rank to work
	for (unordered_map <wstring, dbs>::iterator dboci = dbPediaOntologyCategoryList.begin(), dbociEnd = dbPediaOntologyCategoryList.end(); dboci != dbociEnd; dboci++)
	{
		if (dboci->second.ontologyType == dbPedia_Ontology_Type)
		{
			vector <wstring> superClasses;
			for (vector<wstring>::iterator sci = dboci->second.superClasses.begin(), sciEnd = dboci->second.superClasses.end(); sci != sciEnd; sci++)
			{
				unordered_map<wstring, wstring>::iterator lm = labelMap.find(*sci);
				if (lm != labelMap.end())
				{
					superClasses.push_back(lm->second);
				}
				else
					lplog(LOG_WIKIPEDIA, L"superclass %s of %s not found.", sci->c_str(), dboci->second.compactLabel.c_str());
			}
			dboci->second.superClasses = superClasses;
		}
	}
	fillRanks(dbPedia_Ontology_Type);
	lplog(LOG_WIKIPEDIA, L"dbpedia ontology inserted %d entries.", dbPediaOntologyCategoryList.size()- beginDbPediaEntries);
	return 0;
}

/*
unordered_map <wstring, dbs>::iterator copy(unordered_map <wstring, dbs> &hm,void *buf,int &where,int limit,unordered_map <wstring, dbs>::iterator &hint)
{ LFS
	wstring key;
	dbs predicate;
	//unordered_map <wstring, dbs>::iterator hmi;
	if (!copy(key,buf,where,limit)) return NULL;
	if (!copy(predicate,buf,where,limit)) return NULL;
	return hint=hm.insert(hint,std::pair<wstring,dbs>(key,predicate));
	//if ((hmi=hm.find(key))==dbPediaOntologyCategoryList.end())
	//{
	//	hm[key]=predicate;
	//	hmi=hm.find(key);
	//}
	//return hmi;
}
*/

bool Ontology::copy(void *buf, dbs &dbsn, int &where, int limit)
{
	DLFS
		if (!::copy(buf, dbsn.compactLabel, where, limit)) return false;
	if (!::copy(buf, dbsn.infoPage, where, limit)) return false;
	if (!::copy(buf, dbsn.abstractDescription, where, limit)) return false;
	if (!::copy(buf, dbsn.commentDescription, where, limit)) return false;
	if (!::copy(buf, dbsn.numLine, where, limit)) return false;
	if (!::copy(buf, dbsn.ontologyType, where, limit)) return false;
	if (!::copy(buf, dbsn.ontologyHierarchicalRank, where, limit)) return false;
	if (!::copy(buf, dbsn.superClasses, where, limit)) return false;
	if (!::copy(buf, dbsn.descriptionFilled, where, limit)) return false;
	return true;
}

bool copy(dbs &dbsn, void *buf, int &where, int limit)
{
	DLFS
		if (!copy(dbsn.compactLabel, buf, where, limit)) return false;
	if (!copy(dbsn.infoPage, buf, where, limit)) return false;
	if (!copy(dbsn.abstractDescription, buf, where, limit)) return false;
	if (!copy(dbsn.commentDescription, buf, where, limit)) return false;
	if (!copy(dbsn.numLine, buf, where, limit)) return false;
	if (!copy(dbsn.ontologyType, buf, where, limit)) return false;
	if (!copy(dbsn.ontologyHierarchicalRank, buf, where, limit)) return false;
	if (!copy(dbsn.superClasses, buf, where, limit)) return false;
	if (!copy(dbsn.descriptionFilled, buf, where, limit)) return false;
	return true;
}

bool copyOLD(dbs &dbsn, void *buf, int &where, int limit)
{
	DLFS
	if (!copy(dbsn.compactLabel, buf, where, limit)) return false;
	if (!copy(dbsn.infoPage, buf, where, limit)) return false;
	if (!copy(dbsn.abstractDescription, buf, where, limit)) return false;
	if (!copy(dbsn.commentDescription, buf, where, limit)) return false;
	if (!copy(dbsn.numLine, buf, where, limit)) return false;
	if (!copy(dbsn.ontologyType, buf, where, limit)) return false;
	if (!copy(dbsn.ontologyHierarchicalRank, buf, where, limit)) return false;
	if (!copy(dbsn.superClasses, buf, where, limit)) return false;
	return true;
}

bool Ontology::copy(void *buf, unordered_map <wstring, dbs>::iterator dbsi, int &where, int limit)
{
	DLFS
		if (!::copy(buf, dbsi->first, where, limit)) return false;
	if (!copy(buf, dbsi->second, where, limit)) return false;
	return true;
}

bool copy(unordered_map <wstring, dbs>::iterator &hint, void *buf, int &where, int limit, unordered_map <wstring, dbs> &hm)
{
	DLFS
		wstring key;
	dbs predicate;
	if (copy(key, buf, where, limit) && copy(predicate, buf, where, limit))
	{
		std::pair<unordered_map <wstring, dbs>::iterator, bool> p = hm.insert(std::pair<wstring, dbs>(key, predicate));
		hint = p.first;
		return true;
	}
	return false;
}

bool copyOLD(unordered_map <wstring, dbs>::iterator &hint, void *buf, int &where, int limit, unordered_map <wstring, dbs> &hm)
{
	DLFS
		wstring key;
	dbs predicate;
	if (copy(key, buf, where, limit) && copyOLD(predicate, buf, where, limit))
	{
		std::pair<unordered_map <wstring, dbs>::iterator, bool> p = hm.insert(std::pair<wstring, dbs>(key, predicate));
		hint = p.first;
		return true;
	}
	return false;
}

bool rdfCompare(const unordered_map <wstring, dbs>::iterator &lhs,const unordered_map <wstring, dbs>::iterator &rhs)
{ LFS
	return (lhs)->second.ontologyHierarchicalRank < (rhs)->second.ontologyHierarchicalRank;
}

wchar_t *lpOntologySuperClasses[]={L"provincesandterritoriesofcanada",L"country",L"island",L"mountain",L"geoclasspark",L"river",L"stream",L"city",L"statesoftheunitedstates",NULL};

int clocksec()
{
	return clock() / CLOCKS_PER_SEC;
}

int Ontology::fillOntologyList(bool reInitialize)
{ LFS
	if (dbPediaOntologyCategoryList.empty())
	{
		wchar_t path[4096];
		int pathlen=_snwprintf(path,MAX_LEN,L"%s\\dbPediaCache",CACHEDIR);
		_wmkdir(path);
		wcsncpy(path+pathlen,L"\\_rdfTypes",MAX_LEN-pathlen);
		convertIllegalChars(path+pathlen+1);
		path[MAX_PATH-1]=0;
		if (_waccess(path,0)<0 || reInitialize)
		{
			char buffer[MAX_BUF];
			readYAGOOntology();
			readDbPediaOntology();
			readUMBELSuperClasses();
			int fd=_wopen(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD | _S_IWRITE );
			if (fd<0)
			{
				lplog(LOG_ERROR,L"Cannot write rdfTypes dbPediaCache - %S.",_sys_errlist[errno]);
				return -1;
			}
			int where=0;
			*((wchar_t *)buffer)=RDFLIBRARYTYPE_VERSION;
			where+=2;
			int numCategories = dbPediaOntologyCategoryList.size() , categoriesProcessed = 0;
			for (unordered_map <wstring, dbs>::iterator ri=dbPediaOntologyCategoryList.begin(),riEnd=dbPediaOntologyCategoryList.end(); ri!=riEnd; ri++,categoriesProcessed++)
			{
				wstring tmpstr;
				lplog(LOG_WIKIPEDIA,L"%d%%:category %s",categoriesProcessed*100/numCategories,ri->second.toString(tmpstr,ri->first).c_str());
				if (!copy(buffer,ri,where,MAX_BUF))
				{
					lplog(LOG_FATAL_ERROR, L"Cannot write rdfTypes dbPediaCache - %S.", _sys_errlist[errno]);
					return -1;
				}
				if (where>MAX_BUF-40960)
				{
					::write(fd,buffer,where);
					where=0;
				}
			}
			_write(fd,buffer,where);
			_close(fd);
		}
		else
		{
			int fd=_wopen(path,O_RDWR|O_BINARY );
			if (fd<0)
			{
				lplog(LOG_ERROR,L"Cannot open rdfTypes (%s) - %S.",path,_sys_errlist[errno]);
				return -1;
			}
			void *vBuffer;
			int bufferlen=filelength(fd),where=0;
			vBuffer =(void *)tmalloc(bufferlen+10);
			::read(fd, vBuffer,bufferlen);
			_close(fd);
			if (*((wchar_t *)vBuffer)!=RDFLIBRARYTYPE_VERSION) // version
			{
				tfree(bufferlen+10, vBuffer);
				return fillOntologyList(true);
			}
			where+=2;
			wstring name;
		  int lastProgressPercent=-1;
			unordered_map <wstring, dbs>::iterator hint=dbPediaOntologyCategoryList.end();
			int numAbstractDescriptions=0,numCommentDescriptions=0,numInfoPages=0,numOntologyHierarchicalRank=0,numSuperClasses=0;
			while (where<bufferlen)
			{
				if (((__int64)where*100/(__int64)bufferlen)>(__int64)lastProgressPercent)
				{
					lastProgressPercent=((__int64)where*100/(__int64)bufferlen);
					wprintf(L"PROGRESS: %03d%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \r",lastProgressPercent,where,bufferlen,clocksec(),memoryAllocated);
				}
				if (!::copy(hint, vBuffer,where,bufferlen,dbPediaOntologyCategoryList))
				{
					lplog(LOG_FATAL_ERROR, L"Cannot read ontology relations - %S.", _sys_errlist[errno]);
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
			lplog(LOG_WHERE, L"ontology: numAbstractDescriptions=%d,numCommentDescriptions=%d,numInfoPages=%d,numOntologyHierarchicalRank=%d,numSuperClasses=%d",
				numAbstractDescriptions, numCommentDescriptions, numInfoPages, numOntologyHierarchicalRank, numSuperClasses);
			wprintf(L"PROGRESS: 100%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \n",where,bufferlen,clocksec(),memoryAllocated);
			tfree(bufferlen+10, vBuffer);
		}
	}
	for (int I=0; lpOntologySuperClasses[I]; I++)
	{
		if (dbPediaOntologyCategoryList.find(lpOntologySuperClasses[I])!=dbPediaOntologyCategoryList.end())
			//lplog(LOG_ERROR,L"superClass %s not found in ontology list (1).",lpOntologySuperClasses[I]);
		//else
			dbPediaOntologyCategoryList[lpOntologySuperClasses[I]].ontologyHierarchicalRank=-1;
	}
	return 0;
}

int Ontology::findCategoryRank(wstring &qtype,wstring &parentObject,wstring &object,vector <cTreeCat *> &rdfTypes,wstring &uri)
{ LFS
	bool foundDBPediaCategory=false,foundYAGOCategory=false,foundUMBELCategory=false,foundOpenGISCategory=false;
	wstring cat;
	if (foundDBPediaCategory=uri.find(L"http://dbpedia.org/ontology/")!=wstring::npos) cat=uri.c_str()+wcslen(L"http://dbpedia.org/ontology/");
	else if (foundYAGOCategory=uri.find(L"http://dbpedia.org/class/yago/")!=wstring::npos) cat=uri.c_str()+wcslen(L"http://dbpedia.org/class/yago/");
	else if (foundUMBELCategory=uri.find(L"http://umbel.org/umbel/rc/")!=wstring::npos) cat=uri.c_str()+wcslen(L"http://umbel.org/umbel/rc/");
	else if (foundOpenGISCategory=uri.find(L"http://www.opengis.net/")!=wstring::npos) cat=uri.c_str()+wcslen(L"http://www.opengis.net/");
	else return -1;
	unordered_map <wstring, dbs>::iterator cli=findCategory(cat);
	int ret;// , numResults = 0;
	wstring buffer, temp, superClass;
	size_t pos = 0, pos2 = 0;
	if ((cli == dbPediaOntologyCategoryList.end() || cli->second.ontologyHierarchicalRank == 100) && foundYAGOCategory)
	{
		if (ret = getDBPediaPath(-1, uri, buffer, object + L"_cR" + qtype + L".html")) return -1;
		// <a class="uri" rel="rdfs:subClassOf" xmlns:rdfs="http://www.w3.org/2000/01/rdf-schema#" href="http://dbpedia.org/class/yago/Alumnus109786338">
		if (firstMatch(buffer, L"<a class=\"uri\" rel=\"rdfs:subClassOf\"", L">", pos, temp, false) >= 0 &&
			firstMatch(temp, L"href=\"http://dbpedia.org/class/yago/", L"\"", pos2, superClass, false) >= 0)
		{
			dbPediaOntologyCategoryList[cat].superClasses.push_back(superClass);
			cli = dbPediaOntologyCategoryList.find(cat);
			cli->second.ontologyType = YAGO_Ontology_Type;
		}
	}
	// UMBEL getPath doesn't work because of HTTP redirection - this code path is disabled
	// <rdf:Description rdf:about="http://umbel.org/umbel/rc/EukaryoticCell"><rdfs:subClassOf rdf:resource="http://umbel.org/umbel#TopicsCategories"/></rdf:Description>
	// <rdf:Description rdf:about="http://umbel.org/umbel/rc/EukaryoticCell"><rdfs:subClassOf rdf:resource="http://umbel.org/umbel/rc/Cell"/></rdf:Description>
	if ((cli == dbPediaOntologyCategoryList.end() || cli->second.ontologyHierarchicalRank == 100) && foundUMBELCategory)
	{
		if (ret = getDBPediaPath(-1, uri, buffer, object + L"_cR" + qtype + L".html")) return -1;
		if (firstMatch(buffer, L"<rdfs:subClassOf rdf:resource=\"http://umbel.org/umbel/rc/", L"\"", pos, superClass, false) >= 0)
		{
			dbPediaOntologyCategoryList[cat].superClasses.push_back(superClass);
			cli = dbPediaOntologyCategoryList.find(cat);
			cli->second.ontologyType = UMBEL_Ontology_Type;
		}
	}
	if (cli!=dbPediaOntologyCategoryList.end())
	{
		//if (cli->second.compactLabel.size()>0)
		//	getDescription(cli);  only works with dbpedia ontology
		unordered_map <wstring, dbs>::iterator scli=findCategory(superClass);
		if (scli!=dbPediaOntologyCategoryList.end())
			cli->second.ontologyHierarchicalRank=scli->second.ontologyHierarchicalRank+1;
	}
	if (cli!=dbPediaOntologyCategoryList.end())
	{
		rdfTypes.push_back(new cTreeCat(cli,object,parentObject,qtype,qtype[0]-L'0'));
		return cli->second.ontologyHierarchicalRank;
	}
	return -1;
}

// access dbPedia on the local virtuoso server.  Derive type, description and ontological position and rank
bool Ontology::extractResults(wstring begin,wstring uobject,wstring end,wstring qtype, vector <cTreeCat *> &rdfTypes,vector <wstring> &resources,wstring parentObject)
{ LFS
	wstring object;
	if (uobject.find_first_of(L",|:[]#.()$!%")!=wstring::npos && uobject.find(L"http://dbpedia.org")==wstring::npos)
	{
		lplog(LOG_ERROR,L"dbpedia extractResults %s rejected (illegal character)",uobject.c_str());
		return false;
	}
	if (uobject.length()>100)
	{
		lplog(LOG_ERROR,L"dbpedia extractResults %s rejected (too long)",uobject.c_str());
		return false;
	}
	size_t bw;
	// + is illegal in dbPedia
	while ((bw=uobject.find_first_of(L"+/ "))!=wstring::npos)
		uobject[bw]=L'_';
	encodeURL(uobject,object);
	if (WordClass::isDash(object[0]))
		return false;
	wstring webAddress=basehttpquery+begin+object+end,buffer,temp,uri,fpobject=uobject, beginhttpquery = basehttpquery + begin;
	vector <wstring> labels;
	adjustQuote(beginhttpquery,object,webAddress);
	int ret,numResults=0,originalRDFTypesSize=rdfTypes.size();
	if (object.find(L"http%3A%2F%2Fdbpedia%2Eorg%2Fresource%2F")!=wstring::npos)
	{
		labels.push_back(object.c_str()+wcslen(L"http%3A%2F%2Fdbpedia%2Eorg%2Fresource%2F"));
		fpobject=uobject.c_str()+wcslen(L"http://dbpedia.org/resource/");
	}
	else
		labels.push_back(object);
	if (ret = Internet::readPage(webAddress.c_str(), buffer)) return false;
	//if (ret=getDBPediaPath(-1,webAddress,buffer,fpobject+L"_"+qtype+L".xml")) return false;
	takeLastMatch(buffer,L"<results distinct=\"false\" ordered=\"true\">",L"</results>",temp,false);
	for (size_t pos=0; firstMatch(temp,L"<uri>",L"</uri>",pos,uri,false)!=wstring::npos; numResults++)
	{
		lplog(LOG_WIKIPEDIA,L"%s:%s:%s:%s",qtype.c_str(),parentObject.c_str(),fpobject.c_str(),uri.c_str());
		if (uri.find(L"http://dbpedia.org/resource/")!=wstring::npos)
		{
			resources.push_back(uri);
			/*
			// extractResults already encodes the object, but Virtuoso requires () to be encoded still
			wstring decodedURI;
			decodeURL(uri,decodedURI);
			replace(decodedURI.begin(),decodedURI.end(),L' ',L'_');
			// but replace () with %28,%29:
			wstring rlabel;
			for (unsigned int I=0; I<decodedURI.length(); I++)
				if (decodedURI[I]==L'(')
					rlabel+=L"%28";
				else if (decodedURI[I]==L')')
					rlabel+=L"%29";
				else
					rlabel+=decodedURI[I];
			decodedURI=rlabel;
			*/
		}
		else
			findCategoryRank(qtype,parentObject,fpobject,rdfTypes,uri);
	}
	wstring abstract,comment,infoPage;
	getDescription(labels[0],fpobject,abstract,comment,infoPage);
	for (unsigned int I=originalRDFTypesSize; I<rdfTypes.size(); I++)
	{
		rdfTypes[I]->assignDetails(abstract,comment,infoPage);
	}
	//if (!numResults)
	decodeURL(webAddress,temp);
	int whereQuery=temp.find(L"query=");
	if (whereQuery!=wstring::npos)
		temp.erase(0,whereQuery+6);
	if (rdfDetail)
		lplog(LOG_WIKIPEDIA,L"%s:%s:%s\nENCODED WEBADDRESS:%s\nDECODED WEBADDRESS%s",
		  qtype.c_str(),parentObject.c_str(),fpobject.c_str(),webAddress.c_str(),temp.c_str());
	unordered_map <wstring, dbs>::iterator dbSeparator=dbPediaOntologyCategoryList.find(SEPARATOR);
	if (dbSeparator==dbPediaOntologyCategoryList.end())
	{
		dbs ndbs;
		dbPediaOntologyCategoryList[SEPARATOR]=ndbs;
		dbSeparator=dbPediaOntologyCategoryList.find(SEPARATOR);
	}
	if (rdfTypes.size() && rdfTypes[rdfTypes.size()-1]->cli!=dbSeparator)
		rdfTypes.push_back(new cTreeCat(dbSeparator));
	return numResults>0;
}

inline int (isUnderline)(int c) { return c==L'_'; }
int Ontology::enterCategory(string &id,string &k,string &propertyValue,string &description,string &slobject,wstring &object,string &objectType,string &name,vector <wstring> &wikipediaLinks,vector <wstring> &professionLinks,vector <cTreeCat *> &rdfTypes)
{ LFS
	if (rejectCategories.find(propertyValue)==rejectCategories.end())
	{
		unordered_map <wstring, dbs>::iterator cli;
		wstring wPropertyValue,wId;
		mTW(propertyValue,wPropertyValue);
		// turn written_work into writtenwork or written work	
		if ((cli=dbPediaOntologyCategoryList.find(wPropertyValue))==dbPediaOntologyCategoryList.end() && wPropertyValue.find(L'_')!=wstring::npos)
		{
			wstring intoSpaces=wPropertyValue;
			replace(intoSpaces.begin(),intoSpaces.end(),L'_',L' ');
			if ((cli=dbPediaOntologyCategoryList.find(intoSpaces))!=dbPediaOntologyCategoryList.end())
				wPropertyValue=intoSpaces;
			else
			{
				wstring noUnderlines=wPropertyValue;
				noUnderlines.erase(remove_if(noUnderlines.begin(), noUnderlines.end(), isUnderline), noUnderlines.end());
				if ((cli=dbPediaOntologyCategoryList.find(noUnderlines))!=dbPediaOntologyCategoryList.end())
					wPropertyValue=noUnderlines;
			}
		}
		if (cli!=dbPediaOntologyCategoryList.end())
		{
			if (logDetail)
				lplog(LOG_WHERE|LOG_WIKIPEDIA,L"object=%s objectType=%S id=%S name=%S",object.c_str(),objectType.c_str(),id.c_str(),name.c_str());
			cTreeCat *tc;
			if (slobject==name || slobject==k)
				rdfTypes.push_back(tc=new cTreeCat(cli,mTW(id,wId),object,L"1 FB",1,k,description,wikipediaLinks,professionLinks,k==slobject));
			else
				rdfTypes.push_back(tc=new cTreeCat(cli,mTW(id,wId),object,L"6 FB",6,k,description,wikipediaLinks,professionLinks,k==slobject));
			return 1;
		}
		else
		{
			if (logDetail)
				lplog(LOG_WHERE|LOG_WIKIPEDIA,L"NOT FOUND: object=%s objectType=%S id=%S",object.c_str(),objectType.c_str(),id.c_str());
			return -2;
		}
	}
	else if (logDetail)
		lplog(LOG_WHERE|LOG_WIKIPEDIA,L"REJECTED: object=%s objectType=%S id=%S propertyValue=%S",object.c_str(),objectType.c_str(),id.c_str(),propertyValue.c_str());
	return -3;
}

// get acronym
int Ontology::getAcronyms(wstring &object,vector <wstring> &acronyms)
{ LFS
	// must be one word and must be all caps
	for (unsigned int I=0; I<object.size(); I++)
		if (iswlower(object[I]) || iswspace(object[I]) || iswdigit(object[I]))
			return -1;
	// issue request
	// http://acronyms.thefreedictionary.com/BND
	wstring webAddress=L"http://acronyms.thefreedictionary.com/"+object;
	wstring buffer,filePathOut,headers;
	if (Internet::getWebPath(-1,webAddress,buffer,object+L"ACRO",L"acronymCache",filePathOut,headers,-1,false,true, forceWebReread)<0)
		return -1;
	// reduce acronym page
	size_t beginPos=wstring::npos;
	wstring acronymsBuffer,individualAcronymBuffer;
	int firstMatch(wstring &buffer,wstring beginString,wstring endString,size_t &beginPos,wstring &match,bool include_begin_and_end);
	firstMatch(buffer,L"<table id=AcrFinder class=AcrFinder",L"</table>",beginPos,acronymsBuffer,false);
	beginPos=wstring::npos;
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
	while (firstMatch(acronymsBuffer,L"<tr cat=",L"</tr>",beginPos,individualAcronymBuffer,false)!=wstring::npos)
	{
		/*
		<tr cat="64">
        <td class="acr">BND</td>
        <td>Brunei Dollar 
        <span class="illustration">(ISO currency code)</span></td>
      </tr>
			*/
		wstring trashSpan;
		beginPos=-1;
		while (firstMatch(individualAcronymBuffer,L"<span",L"</span>",beginPos,trashSpan,false)!=wstring::npos);
	  wstring base,acronym;
		firstMatch(individualAcronymBuffer,L"acr>",L"</td>",beginPos,base,false);
		firstMatch(individualAcronymBuffer,L"<td>",L"</td>",beginPos,acronym,false);
		trim(base);
		trim(acronym);
		if (base==object)
		  acronyms.push_back(acronym);
	}
	return 0;
}

int Ontology::getAcronymRDFTypes(wstring &object,vector <cTreeCat *> &rdfTypes)
{ LFS
	vector <wstring> acronyms;
	if (Ontology::getAcronyms(object,acronyms)<0)
		return -1;
	for (unsigned int I=0; I<acronyms.size(); I++)
		Ontology::rdfIdentify(acronyms[I],rdfTypes,L"b");
	return 0;
}

/*********************************************************
freebase begin
***********************************************************/
wstring Ontology::extractLinkedFreebaseDescription(string &properties, wstring &wDescription)
{
	int linkDescription = -1;
	while ((linkDescription = properties.find("{L}", linkDescription + 1)) != string::npos)
	{
		int nextLink = properties.find("{", linkDescription + 1);
		wstring wproperties;
		mTW(properties, wproperties);
		wstring q = L"select properties from freebaseProperties where id='";
		if (nextLink < 0)
			q += wproperties.substr(linkDescription + 3) + L"'";
		else
			q += wproperties.substr(linkDescription + 3, nextLink - linkDescription - 3) + L"'";
		MYSQL_RES *result = NULL;
		MYSQL_ROW sqlrow;
		if (!myquery(&mysql, (wchar_t *)q.c_str(), result)) return L"";
		if ((sqlrow = mysql_fetch_row(result)) == NULL) return L"";
		string description = (sqlrow[0] == NULL) ? "" : sqlrow[0];
		mysql_free_result(result);
		size_t whereDescription = description.find("{D}");
		if (whereDescription != string::npos)
			return mTW(description.substr(whereDescription + 3), wDescription);
	}
	int whereDescription = properties.find("{D}");
	if (whereDescription != string::npos)
		return mTW(properties.substr(whereDescription + 3), wDescription);
	return L"";
}

void replaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

// prefer an entry where key=id or labelWithSpace, if it exists
wstring Ontology::getFBDescription(wstring id, wstring name)
{
	LFS
		initializeDatabaseHandle(mysql, L"localhost", alreadyConnected);
	wstring q = L"select properties from freebaseProperties where ", q2 = q, q3 = q;
	if (name.empty())
	{
		q += L"id='" + id + L"'";
		q2 += L"k='" + id + L"'";
		q3 = q + L"and k='" + id + L"'";
	}
	else
	{
		// escape single quotes
		replaceAll(name, L"'", L"\\'");
		q += L"name='" + name + L"'";
		q2 += L"k='" + name.substr(0, 64) + L"'";
		q3 = q + L"and k='" + name.substr(0, 64) + L"'";
	}
	bool q2tried = false, q3tried = false;
	MYSQL_RES *result = NULL;
	MYSQL_ROW sqlrow;
	if (!myquery(&mysql, (wchar_t *)q.c_str(), result)) return L"";
	if ((sqlrow = mysql_fetch_row(result)) == NULL)
	{
		mysql_free_result(result);
		q2tried = true;
		if (!myquery(&mysql, (wchar_t *)q2.c_str(), result)) return L"";
		if ((sqlrow = mysql_fetch_row(result)) == NULL)
		{
			mysql_free_result(result);
			return L"";
		}
	}
	else if (mysql_num_rows(result) > 1)
	{
		q3tried = true;
		MYSQL_RES *resultWithKey = NULL;
		MYSQL_ROW sqlrowWithKey;
		if (myquery(&mysql, (wchar_t *)q3.c_str(), resultWithKey))
		{
			if ((sqlrowWithKey = mysql_fetch_row(resultWithKey)) != NULL)
			{
				mysql_free_result(result);
				sqlrow = sqlrowWithKey;
				result = resultWithKey;
			}
			else
				mysql_free_result(resultWithKey);
		}
	}
	string properties = (sqlrow[0] == NULL) ? "" : sqlrow[0];
	mysql_free_result(result);
	wstring description;
	extractLinkedFreebaseDescription(properties, description);
	if (description.length())
		return description;
	if (!q2tried && myquery(&mysql, (wchar_t *)q2.c_str(), result))
	{
		if ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			string tempProperties = (sqlrow[0] == NULL) ? "" : sqlrow[0];
			mysql_free_result(result);
			return extractLinkedFreebaseDescription(tempProperties, description);
		}
		mysql_free_result(result);
	}
	if (!q3tried && myquery(&mysql, (wchar_t *)q3.c_str(), result))
	{
		if ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			string tempProperties = (sqlrow[0] == NULL) ? "" : sqlrow[0];
			mysql_free_result(result);
			return extractLinkedFreebaseDescription(tempProperties, description);
		}
		mysql_free_result(result);
	}
	return L"";
}

// properties labelWithSpace type
int Ontology::lookupInFreebase(wstring object,vector <cTreeCat *> &rdfTypes)
{ LFS
  initializeDatabaseHandle(mysql,L"localhost",alreadyConnected);
	replace(object.begin(),object.end(),L'_',L' ');
	replace(object.begin(),object.end(),L'+',L' ');
	replace(object.begin(),object.end(),L'|',L' ');
	removeExcessSpaces(object);
	if (object.find(L'\'')!=wstring::npos)
		escapeSingleQuote(object);
	transform (object.begin (), object.end (), object.begin (), (int(*)(int)) tolower);
	wstring lobject=object,k=object;
	string slobject;
	wTM(lobject,slobject);
	if (k.find(L'!')!=wstring::npos)
	{
		replace(k.begin(),k.end(),L'!',L' ');
		removeExcessSpaces(k);
	}
	replace(k.begin(),k.end(),L' ',L'_');
  wstring q=L"select k,id,properties from freebaseProperties where name='"+lobject+L"' OR k='"+k+L"'";
	if (k.find('\'')!=string::npos)
	{
		wstring k2=k;
		removeSingleQuote(k2);
		if (k2!=k)
			q+=L" OR k='"+k2+L"'";
	}
	if (logDetail)
		::lplog(LOG_WHERE,L"%s",q.c_str());
	return lookupInFreebaseQuery(object,slobject,q,rdfTypes,true);
}

// select * from freebaseProperties where id in ('m.012t_z','m.0fj9f','m.0hltv');
int Ontology::lookupLinks(vector <wstring> &links)
{ LFS
  MYSQL_RES *result=NULL;
  MYSQL_ROW sqlrow;
	wstring tmpstr,q=L"select name from freebaseProperties where id in ('"+vectorString(links,tmpstr,L"','")+L"')";
  if (!myquery(&mysql,(wchar_t *)q.c_str(),result,true)) return -1;
	vector<wstring> names;
	wstring name;
  while ((sqlrow=mysql_fetch_row(result))!=NULL) 
		if (sqlrow[0]!=NULL) 
			names.push_back(mTW(sqlrow[0],name));
	links=names;
	return 0;
}

int Ontology::lookupInFreebaseQuery(wstring &object,string &slobject,wstring &q,vector <cTreeCat *> &rdfTypes,bool accumulateAliases)
{ LFS
  MYSQL_RES *result=NULL;
  MYSQL_ROW sqlrow;
	if (!myquery(&mysql, (wchar_t *)q.c_str(), result, true))
	{
		lplog(LOG_FATAL_ERROR, L"freebaseQuery SQL error: %s\n%s\n%S", q.c_str(), object.c_str(), slobject.c_str());
		return -1;
	}
	vector<string> aliases;
  while ((sqlrow=mysql_fetch_row(result))!=NULL) 
	{
		string k=(sqlrow[0]==NULL) ? "" : sqlrow[0],id=(sqlrow[1]==NULL) ? "" : sqlrow[1],properties=(sqlrow[2]==NULL) ? "" : sqlrow[2];
		if (properties.empty())
		{
			if (accumulateAliases)
				aliases.push_back(id);
			continue;
		}
		replace(id.begin(),id.end(),'_',' ');
		dbs ndbs;
		dbPediaOntologyCategoryList[SEPARATOR]=ndbs;
		//unordered_map <wstring, dbs>::iterator dbSeparator=dbPediaOntologyCategoryList.find(SEPARATOR);
		size_t whereDescription=properties.find("{D}"),whereNextType,whereNextWikipediaLink,whereNextProfessionLink;
		size_t whereName=properties.find("{N}");
		string description,objectType;
		string name;
		if (whereDescription!=string::npos)
		{
			description=properties.substr(whereDescription+3);
			properties.erase(whereDescription);
		}
		if (whereName!=string::npos)
		{
			size_t nextBracket=properties.find(whereName+1,'{');
			if (nextBracket!=string::npos)
				name=properties.substr(whereName+3,nextBracket);
			else
				name=properties.substr(whereName+3);
			transform (name.begin (), name.end (), name.begin (), (int(*)(int)) tolower);
		}
		vector <wstring> wikipediaLinks;
		for (int whereWikipediaLink=properties.find("{W}"); whereWikipediaLink!=string::npos; whereWikipediaLink=whereNextWikipediaLink)
		{
			string wikipediaLink;
			wstring wWikipediaLink;
			if ((whereNextWikipediaLink=properties.find("{W}",whereWikipediaLink+1))==string::npos)
				wikipediaLink=properties.substr(whereWikipediaLink+3);
			else
				wikipediaLink=properties.substr(whereWikipediaLink+3,whereNextWikipediaLink-whereWikipediaLink-3);
			size_t nextBracket=wikipediaLink.find("{");
			if (nextBracket!=string::npos)
				wikipediaLink.erase(wikipediaLink.begin()+nextBracket,wikipediaLink.end());
			wikipediaLinks.push_back(mTW(wikipediaLink,wWikipediaLink));
		}
		vector <wstring> professionLinks;
		for (int whereProfessionLink=properties.find("{P}"); whereProfessionLink!=string::npos; whereProfessionLink=whereNextProfessionLink)
		{
			string professionLink;
			wstring wProfessionLink;
			if ((whereNextProfessionLink=properties.find("{P}",whereProfessionLink+1))==string::npos)
				professionLink=properties.substr(whereProfessionLink+3);
			else
				professionLink=properties.substr(whereProfessionLink+3,whereNextProfessionLink-whereProfessionLink-3);
			size_t nextBracket=professionLink.find("{");
			if (nextBracket!=string::npos)
				professionLink.erase(professionLink.begin()+nextBracket,professionLink.end());
			professionLinks.push_back(mTW(professionLink,wProfessionLink));
		}
		lookupLinks(professionLinks);
		for (size_t whereType=properties.find("{T}"); whereType!=string::npos; whereType=whereNextType)
		{
			if ((whereNextType=properties.find("{T}",whereType+1))==string::npos)
				objectType=properties.substr(whereType+3);
			else
				objectType=properties.substr(whereType+3,whereNextType-whereType-3);
			size_t nextBracket=objectType.find("{");
			if (nextBracket!=string::npos)
				objectType.erase(objectType.begin()+nextBracket,objectType.end());
			int whereLastPeriod=objectType.find_last_of('.');
			string lastPropertyValue=objectType.substr(whereLastPeriod+1);
			string familyPropertyValue=objectType.substr(0,whereLastPeriod);
			if (enterCategory(id,k,lastPropertyValue,description,slobject,object,objectType,name,wikipediaLinks,professionLinks,rdfTypes)!=-3)
				enterCategory(id,k,familyPropertyValue,description,slobject,object,objectType,name,wikipediaLinks,professionLinks,rdfTypes);
			unordered_map <wstring, dbs>::iterator dbSeparator=dbPediaOntologyCategoryList.find(SEPARATOR);
			if (rdfTypes.size() && rdfTypes[rdfTypes.size()-1]->cli!=dbSeparator)
				rdfTypes.push_back(new cTreeCat(dbSeparator));
		}
	}
	mysql_free_result(result);
	for (unsigned int I=0; I<aliases.size(); I++)
	{
	  wstring alias,fbq=wstring(L"select k,id,properties from freebaseProperties where id='")+mTW(aliases[I],alias)+L"'";
		lookupInFreebaseQuery(object,slobject,fbq,rdfTypes,false);
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
void Ontology::getRDFTypesFromDbPedia(wstring object,vector <cTreeCat *> &rdfTypes,wstring parentObject,wstring fromWhere)
{ LFS
	wstring buffer,start;
	vector <wstring> resources;
	lplog(LOG_WIKIPEDIA,L"%s:%s",parentObject.c_str(),object.c_str());
	// Irving Berlin
	// SELECT+%3Fv+%0D%0AWHERE+%7B%3AIrving_Berlin+a+%3Fv%7D
	extractResults(prefix_colon+L"SELECT+%3Fv+%0D%0AWHERE+%7B%3A",object,L"+a+%3Fv%7D",L"1 TYPES",rdfTypes,resources,parentObject);
	// get any redirects from object
	extractResults(prefix_colon+prefix_owl_ontology+selectWhere+L"+%7B%3A",object,L"+dbpedia-owl%3AwikiPageRedirects+%3Fv+%7D%0D%0A%7D",L"2 REDIRECT",rdfTypes,resources,parentObject);
	// for each redirect, start over.
	for (unsigned int I=0; I<resources.size(); I++)
	{
		// REDIRECT:http://dbpedia.org/resource/Rafic_Hariri
		start=prefix_colon+prefix_owl_ontology+selectWhere+L"+%7B+%3C"; 
		vector <wstring> recursiveResources;
		extractResults(start,resources[I],L"%3E+a+%3Fv+%7D%0D%0A%7D&format=text%2Fxml&timeout=0&debug=on",L"3 REDIRECT(RESOURCES)",rdfTypes,recursiveResources,object);
		// get any disambiguating references from resource
		extractResults(start,resources[I],L"%3E+dbpedia-owl%3AwikiPageDisambiguates+%3Fv+%7D%0D%0A%7D",L"4 DISAMBIGUATE from REDIRECT",rdfTypes,recursiveResources,parentObject);
		for (unsigned int J=0; J<recursiveResources.size(); J++)
		{
			// DISAMBIGUATE:http://dbpedia.org/resource/Blondie_%28film%29
			start=prefix_dbpedia+selectWhere+L"+%3C"; 
			vector <wstring> recursiveResources2;
			extractResults(start,recursiveResources[J],L"%3E+a+%3Fv+%0D%0A%7D&format=text%2Fxml&timeout=0&debug=on",L"5 DISAMBIGUATE from REDIRECT(RESOURCES)",rdfTypes,recursiveResources2,object);
		}
	}
	resources.clear();
	start=prefix_colon+prefix_owl_ontology+selectWhere+L"+%7B%3A";
	extractResults(start,object,L"+dbpedia-owl%3AwikiPageDisambiguates+%3Fv+%7D%0D%0A%7D",L"3 DISAMBIGUATE",rdfTypes,resources,parentObject);
	// for each disambiguation, start over.
	for (unsigned int I=0; I<resources.size(); I++)
	{
		start=prefix_dbpedia+selectWhere+L"+%3C"; 
		vector <wstring> recursiveResources2;
		extractResults(start,resources[I],L"%3E+a+%3Fv+%0D%0A%7D&format=text%2Fxml&timeout=0&debug=on",L"4 DISAMBIGUATE(RESOURCES)",rdfTypes,recursiveResources2,object);
	}
	lookupInFreebase(object,rdfTypes);
	if (!rdfTypes.size() && rdfDetail) 
		lplog(LOG_WIKIPEDIA|LOG_INFO,L"No rdf types in dbpedia:%s:%s",parentObject.c_str(),object.c_str());
}

int Ontology::readRDFTypes(wchar_t path[4096], vector <cTreeCat *> &rdfTypes)
{
	LFS
		//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path, __FUNCTIONW__);
	int fd=_wopen(path,O_RDWR|O_BINARY ); 
	if (fd<0)
	{
		lplog(LOG_ERROR,L"Cannot open rdfTypes (%s) - %S.",path,_sys_errlist[errno]);
		return -1;
	}
	void *buffer;
	int bufferlen=filelength(fd),where=0;
	buffer=(void *)tmalloc(bufferlen+10);
	::read(fd,buffer,bufferlen);
	_close(fd);
	if (*((wchar_t *)buffer)!=RDFTYPE_VERSION) // version
	{
		tfree(bufferlen+10,buffer);
		_wremove(path);
		return -2;
	}
	where+=2;
	unordered_map <wstring, dbs>::iterator hint=dbPediaOntologyCategoryList.end();
	while (where<bufferlen)
	{
		cTreeCat *tc=new cTreeCat();
		if (!tc->copy(dbPediaOntologyCategoryList,buffer,where,bufferlen)) break;
		rdfTypes.push_back(tc);
	}
	tfree(bufferlen+10,buffer);
	return 0;
}

int Ontology::writeRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes)
{ LFS
		int fd=_wopen(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD | _S_IWRITE );
		if (fd<0)
		{
			lplog(LOG_ERROR,L"Cannot write %s - %S.",path,_sys_errlist[errno]);
			return -1;
		}
		char buffer[MAX_BUF*10];
		int where=0;
		*((wchar_t *)buffer)=RDFTYPE_VERSION;
		where+=2;
		for (vector <cTreeCat *>::iterator ri=rdfTypes.begin(),riEnd=rdfTypes.end(); ri!=riEnd; ri++)
		{
			if (!(*ri)->copy(buffer,where,MAX_BUF*10))
			{
				lplog(LOG_FATAL_ERROR, L"Cannot write %s - %S.", path, _sys_errlist[errno]);
				return -1;
			}
			if (where>(MAX_BUF*10)-8192)
			{
				_write(fd,buffer,where);
				where=0;
			}
		}
		if (where>0)
			_write(fd,buffer,where);
		_close(fd);
		return 0;
}

void Ontology::compressPath(wchar_t *path)
{
	wchar_t *ch=wcsrchr(path,L'\\');
	if (ch)
	{
		wchar_t *destch=ch;
		for (; *ch; ch++)
			if (*ch!=L'_')
			{
				*destch=*ch;
				destch++;
			}
		*destch=0;
	}
}

bool Ontology::inRDFTypeNotFoundTable(wchar_t *object)
{
	initializeDatabaseHandle(mysql, L"localhost", alreadyConnected);
	if (!myquery(&mysql, L"LOCK TABLES noRDFTypes READ"))
		return false;
	MYSQL_RES * result;
	_int64 numResults = 0;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select 1 from noRDFTypes where word = '%s'", object);
	if (myquery(&mysql, qt, result))
	{
		numResults = mysql_num_rows(result);
		mysql_free_result(result);
	}
	if (!myquery(&mysql, L"UNLOCK TABLES"))
		return false;
	lplog(LOG_INFO, L"*** inRDFTypeNotFoundTable Temp: statement %s resulted in numRows=%d.", qt, numResults);
	return numResults > 0;
}

bool Ontology::insertRDFTypeNotFoundTable(wchar_t *object)
{
	initializeDatabaseHandle(mysql, L"localhost", alreadyConnected);
	if (!myquery(&mysql, L"LOCK TABLES noRDFTypes WRITE"))
		return false;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	wsprintf(qt, L"INSERT INTO noRDFTypes VALUES ('%s')", object);
	bool success = (myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY);
	if (!myquery(&mysql, L"UNLOCK TABLES"))
		return false;
	return success;
}

bool Ontology::inNoERDFTypesDBTable(wstring newObjectName)
{
	initializeDatabaseHandle(mysql, L"localhost", alreadyConnected);
	wchar_t newPath[1024];
	wcscpy(newPath, newObjectName.c_str());
	convertIllegalChars(newPath);
	if (!myquery(&mysql, L"LOCK TABLES noERDFTypes READ"))
		return false;
	MYSQL_RES * result;
	_int64 numResults = 0;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select 1 from noERDFTypes where word = '_%s'", newPath);
	if (myquery(&mysql, qt, result))
	{
		numResults = mysql_num_rows(result);
		mysql_free_result(result);
	}
	if (!myquery(&mysql, L"UNLOCK TABLES"))
		return false;
	lplog(LOG_INFO, L"*** inNoERDFTypesDBTable Temp: statement %s resulted in numRows=%d.", qt, numResults);
	return numResults > 0;
}

bool Ontology::insertNoERDFTypesDBTable(wstring newObjectName)
{
	initializeDatabaseHandle(mysql, L"localhost", alreadyConnected);
	wchar_t newPath[1024];
	wcscpy(newPath, newObjectName.c_str());
	convertIllegalChars(newPath);
	if (!myquery(&mysql, L"LOCK TABLES noERDFTypes WRITE"))
		return false;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	wsprintf(qt, L"INSERT INTO noERDFTypes VALUES ('_%s')", newPath);
	bool success = (myquery(&mysql, qt, true) || mysql_errno(&mysql) == ER_DUP_ENTRY);
	if (!myquery(&mysql, L"UNLOCK TABLES"))
		return false;
	return success;
}

int Ontology::printRDFTypes(wchar_t *kind, vector <cTreeCat *> &rdfTypes)
{
	lplog(LOG_WIKIPEDIA, L"BEGIN %s:%d", kind, rdfTypes.size());
	for (int I = 0; I < rdfTypes.size(); I++)
		rdfTypes[I]->lplogTC(LOG_WIKIPEDIA, L"");
	lplog(LOG_WIKIPEDIA, L"END %s:%d %d", kind, rdfTypes.size());
	return 0;
}

int Ontology::printExtendedRDFTypes(wchar_t *kind, vector <cTreeCat *> &rdfTypes, unordered_map <wstring, int > &topHierarchyClassIndexes)
{
	lplog(LOG_WIKIPEDIA, L"BEGIN %s:%d %d", kind, rdfTypes.size(), topHierarchyClassIndexes.size());
	for (int I = 0; I < rdfTypes.size(); I++)
		rdfTypes[I]->lplogTC(LOG_WIKIPEDIA, L"");
	for (unordered_map <wstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
		lplog(LOG_WIKIPEDIA, L"topHierarchyClassIndexes:%s %d", idi->first.c_str(), idi->second);
	lplog(LOG_WIKIPEDIA, L"END %s:%d %d", kind, rdfTypes.size(), topHierarchyClassIndexes.size());
	return 0;
}

int Ontology::getRDFTypesMaster(wstring object, vector <cTreeCat *> &rdfTypes, wstring fromWhere, bool fileCaching)
{ LFS
	if (cacheRdfTypes && fileCaching)
	{
		AcquireSRWLockShared(&rdfTypeMapSRWLock);
		unordered_map<wstring, int >::iterator rdfni;
		if ((rdfni=rdfTypeNumMap.find(object))==rdfTypeNumMap.end())
			rdfTypeNumMap[object]=1;
		else
		{
			(*rdfni).second++;
			rdfTypes=rdfTypeMap[object];
	//		lplog(LOG_WHERE,L"rdfCache %s %d",object.c_str(),(*rdfni).second);
			ReleaseSRWLockShared(&rdfTypeMapSRWLock);
			return 0;
		}
		ReleaseSRWLockShared(&rdfTypeMapSRWLock);
	}
	if (object.length() > 512)
		return -1;
	wchar_t path[4096];
	int pathlen=_snwprintf(path,MAX_LEN,L"%s\\dbPediaCache",CACHEDIR),retCode=-1;
	_wmkdir(path);
	_snwprintf(path+pathlen,MAX_LEN-pathlen,L"\\_%s",object.c_str());
	convertIllegalChars(path+pathlen+1);
	if (wcslen(path + pathlen + 1) > 127 || wcslen(path + pathlen + 1) < 2 || inRDFTypeNotFoundTable(path + pathlen + 1))
		return -1;
	distributeToSubDirectories(path,pathlen+1,true);
	if (wcslen(path)+12>MAX_PATH)
		compressPath(path);
	path[MAX_PATH-12]=0;
	path[pathlen+2+244]=0; // to guard against a filename that is too long
	wcscat(path,L".rdfTypes");
	if (!fileCaching || _waccess(path,0)<0 || (retCode=readRDFTypes(path,rdfTypes))<0)
	{
		getRDFTypesFromDbPedia(object,rdfTypes,L"",fromWhere);
		vector <wstring> acronyms;
		Ontology::getAcronyms(object,acronyms);
		if (acronyms.size())
		{
			wstring tmpstr;
			lplog(LOG_WHERE,L"ACRO:%s yields acronyms:%s",object.c_str(),vectorString(acronyms,tmpstr,L",").c_str());
			for (unsigned int I=0; I<acronyms.size(); I++)
			{
				vector <cTreeCat *> rdfTypesAcronym;
				replace(acronyms[I].begin(),acronyms[I].end(),L' ',L'_');
				Ontology::rdfIdentify(acronyms[I],rdfTypesAcronym,L"b",fileCaching);
				rdfTypes.insert(rdfTypes.end(),rdfTypesAcronym.begin(),rdfTypesAcronym.end());
			}
			//for (unsigned int I=rdfLen; I<rdfTypes.size(); I++)
			//	rdfTypes[I]->lplog(LOG_WHERE);
		}
		if (rdfTypes.empty())
		{
			path[wcslen(path) - 9] = 0;
			insertRDFTypeNotFoundTable(path + pathlen + 5); // added 4 for the distribution to subdirectories
		}
		else
		{
			printRDFTypes(L"RDFM", rdfTypes);
			retCode = writeRDFTypes(path, rdfTypes);
		}
	}
	if (!superClassesAllPopulated)
	{
		bool oneNotFound=false;
		for (int I=0; lpOntologySuperClasses[I]; I++)
		{
			if (dbPediaOntologyCategoryList.find(lpOntologySuperClasses[I])!=dbPediaOntologyCategoryList.end())
				dbPediaOntologyCategoryList[lpOntologySuperClasses[I]].ontologyHierarchicalRank=-1;
			else
				oneNotFound=true;
			//	lplog(LOG_ERROR,L"superClass %s not found in ontology list (2).",lpOntologySuperClasses[I]);
		}
		superClassesAllPopulated=!oneNotFound;
	}
	if (cacheRdfTypes && fileCaching)
	{
		AcquireSRWLockExclusive(&rdfTypeMapSRWLock);
		rdfTypeMap[object]=rdfTypes;
		ReleaseSRWLockExclusive(&rdfTypeMapSRWLock);
	}
	return retCode;
}

bool checkInsert(wchar_t *fromCategory,wchar_t *finalCategory,unordered_map <wstring ,int > &topHierarchyClassIndexes,vector <cTreeCat *> &rdfTypes,unsigned int &I,int &minIdentifiedQType)
{ LFS
	//rdfTypes[I]->lplog(LOG_WHERE);
	if (rdfTypes[I]->cli->first!=fromCategory)
		return false;
	unordered_map <wstring ,int >::iterator idOi=topHierarchyClassIndexes.find(finalCategory);
	bool found=false;
	if (found=idOi==topHierarchyClassIndexes.end())
		topHierarchyClassIndexes[finalCategory]=I;
	else
	{
		if (found=rdfTypes[I]->confidence<rdfTypes[idOi->second]->confidence)
			idOi->second=I;
	}
	if (found)
	{
		minIdentifiedQType=min(minIdentifiedQType,rdfTypes[I]->confidence);
		// advance to end of separator, if any - take the first type (Vijay Singh) first type is person, which is correct, not sports_team,which is listed afterwards
		for (; I<rdfTypes.size(); I++)
			if (rdfTypes[I]->cli->first==SEPARATOR)
				break;
	}
	return true;
}

wchar_t *knownClasses[]={ L"person", L"place", L"gml/_feature", L"location",L"business",L"organisation",L"work",L"plant",L"animal",
	L"disease",L"provincesandterritoriesofcanada",L"country",L"island",L"mountain",L"geoclasspark",L"river",L"stream",L"city",L"statesoftheunitedstates",NULL };
wchar_t *knownMapToClasses[]={ L"person", L"place", L"place", L"place",L"business",L"business",L"creativeWork",L"plant",L"animal",
	L"disease",L"provincesandterritoriesofcanada",L"country",L"island",L"mountain",L"geoclasspark",L"river",L"river",L"city",L"statesoftheunitedstates",NULL };
bool knownClass(wstring c)
{ LFS
		for (int k=0; knownClasses[k]; k++)
			if (c==knownClasses[k])
				return true;
		return false;
}
set <wstring> knownClassesSet;

// only return true if an entry was found belonging to the lowest qtype
bool Ontology::isolateKnownClasses(unordered_map <wstring ,int > &topHierarchyClassIndexes,vector <cTreeCat *> &rdfTypes,int rdfBaseTypeOffset)
{ LFS
  if (knownClassesSet.empty())
	{
		for (int k=0; knownClasses[k]; k++)
			knownClassesSet.insert(knownClasses[k]);
	}
	// get lowest qtype
	int minQType=100;
	for (unsigned int I=rdfBaseTypeOffset; I<rdfTypes.size(); I++)
		if (rdfTypes[I]->confidence>0)
			minQType=min(minQType,rdfTypes[I]->confidence);
	int minIdentifiedQType=100;
	for (unsigned int I=rdfBaseTypeOffset; I<rdfTypes.size(); I++)
	{
		if (rdfTypes[I]->cli->first==SEPARATOR || knownClassesSet.find(rdfTypes[I]->cli->first)==knownClassesSet.end()) continue;
		for (int k=0; knownClasses[k]; k++)
			if (checkInsert(knownClasses[k],knownMapToClasses[k],topHierarchyClassIndexes,rdfTypes,I,minIdentifiedQType)) 
				break;
		//wprintf(L"%s not found!\n",rdfTypes[I]->cli->first);
	}
	return topHierarchyClassIndexes.size()>0; //  && minIdentifiedQType==minQType
}

void Ontology::includeAllSuperClasses(unordered_map <wstring ,int > &topHierarchyClassIndexes,vector <cTreeCat *> &rdfTypes,int recursionLevel,int rdfBaseTypeOffset)
{ LFS
	if (rdfTypes.empty())
		return;
	wstring tmpstr,tmpstr2;
	unsigned int rdfOriginalSize=rdfTypes.size();
	for (unsigned int I=rdfBaseTypeOffset; I<rdfOriginalSize; I++)
	{
		wstring object=rdfTypes[I]->typeObject;
		if (recursionLevel==1 && logDetail)
			rdfTypes[I]->logIdentity(LOG_WIKIPEDIA,object,false);
		//if (rdfTypes[I]->cli->second.superClasses.empty())
		//	wprintf(L"%d:%s:%s [no superclasses]\n",I,rdfTypes[I]->object.c_str(),rdfTypes[I]->cli->first.c_str());
		for (vector <wstring>::iterator sci=rdfTypes[I]->cli->second.superClasses.begin(),sciEnd=rdfTypes[I]->cli->second.superClasses.end(); sci!=sciEnd; sci++)
		{
			unordered_map <wstring, dbs>::iterator cli=findCategory(*sci);
			if (cli!=dbPediaOntologyCategoryList.end() && cli->second.ontologyHierarchicalRank<rdfTypes[I]->cli->second.ontologyHierarchicalRank)
			{
				bool alreadyThere=false;
				for (vector <cTreeCat *>::iterator ri=rdfTypes.begin(),riEnd=rdfTypes.end(); ri!=riEnd && !(alreadyThere=(*ri)->cli==cli); ri++);
				if (!alreadyThere)
				{
					if (logDetail)
					{
						//wprintf(L"  %d %s:%s:%s %s found %s\n",I,rdfTypes[I]->qtype.c_str(),rdfTypes[I]->object.c_str(),rdfTypes[I]->cli->first.c_str(),sci->c_str(),cli->first.c_str());
						lplog(LOG_WIKIPEDIA,L"%*s%s:(%s)ISTYPE %s:%s:%s:[%s]:rank %d(%s IASC)",
							recursionLevel<<1,L" ",object.c_str(),sci->c_str(),cli->first.c_str(),ontologyTypeString(cli->second.ontologyType, cli->second.resourceType,tmpstr2),cli->second.compactLabel.c_str(),vectorString(cli->second.superClasses,tmpstr,L" ").c_str(),cli->second.ontologyHierarchicalRank,rdfTypes[I]->qtype.c_str());
					}
					rdfTypes.push_back(new cTreeCat(cli,object,*sci,rdfTypes[I]->qtype+L" IASC",rdfTypes[I]->qtype[0]-L'0'));
				}
			}
			//else
			//	wprintf(L"  %d:%s:%s %s NOT found or of >= rank\n",I,rdfTypes[I]->object.c_str(),rdfTypes[I]->cli->first.c_str(),sci->c_str());
		}
	}
	if (rdfTypes.size()>rdfOriginalSize && !isolateKnownClasses(topHierarchyClassIndexes,rdfTypes,rdfOriginalSize))
		includeAllSuperClasses(topHierarchyClassIndexes,rdfTypes,++recursionLevel,rdfOriginalSize);
}

void Ontology::includeSuperClasses(unordered_map <wstring, int > &topHierarchyClassIndexes, vector <cTreeCat *> &rdfTypes)
{
	int recursionLevel = 1, rdfBaseTypeOffset = 0;
	if (!isolateKnownClasses(topHierarchyClassIndexes, rdfTypes, rdfBaseTypeOffset))
		includeAllSuperClasses(topHierarchyClassIndexes, rdfTypes, recursionLevel, rdfBaseTypeOffset);
}

bool detectNonEuropean(wstring word)
{
	char buffer[256];
	BOOL usedDefaultChar;
	int ret = WideCharToMultiByte(
		1252,									//UINT CodePage,
		WC_NO_BEST_FIT_CHARS,//DWORD dwFlags,
		word.c_str(),				 //LPCWCH lpWideCharStr,
		word.length(),			 //int cchWideChar,
		buffer,              //LPSTR lpMultiByteStr,
		256,								 // int cbMultiByte,
		NULL,                //LPCCH lpDefaultChar,
		&usedDefaultChar										 //LPBOOL lpUsedDefaultChar
	);
	return (ret == 0) || (usedDefaultChar);
}

// -\'a-zãâäáàæçêéèêëîíïñôóòöõûüù
bool detectNonEnglish(wstring word)
{
	for (wchar_t c : word)
		if (c != '-' && c != L'—' && c != '\'' && !iswalpha(c)) // deliberately not using isDash because we want to constrain to common well known dash types
			return true;
	return false;
}

void Ontology::rdfIdentify(wstring object,vector <cTreeCat *> &rdfTypes,wstring fromWhere, bool fileCaching)
{ LFS
	if (object.size()>40 && logDetail)
		lplog(LOG_ERROR,L"rdfIdentify:object too long - %s",object.c_str());
	else if (object.size()==1 && logDetail)
		lplog(LOG_ERROR,L"rdfIdentify:object too short - %s",object.c_str());
	else if (detectNonEuropean(object) || object.find_first_of(L"ãâäáàæçêéèêëîíïñôóòöõôûüùú")!=wstring::npos) 
		lplog(LOG_ERROR, L"rdfIdentify:object having non european character set (or non English characters) - %s", object.c_str());
	else
	{
		getRDFTypesMaster(object,rdfTypes,fromWhere,fileCaching);
	}
}

bool Ontology::setPreferred(unordered_map <wstring ,int > &topHierarchyClassIndexes,vector <cTreeCat *> &rdfTypes)
{ LFS
	bool chosen=false;
	int p=100,r=1000;
	for (unordered_map <wstring ,int >::iterator idi=topHierarchyClassIndexes.begin(),idiEnd=topHierarchyClassIndexes.end(); idi!=idiEnd; idi++)
		p=min(p,rdfTypes[idi->second]->confidence);
	for (unordered_map <wstring ,int >::iterator idi=topHierarchyClassIndexes.begin(),idiEnd=topHierarchyClassIndexes.end(); idi!=idiEnd; idi++)
		if (p==rdfTypes[idi->second]->confidence)
			r=min(r,rdfTypes[idi->second]->cli->second.ontologyHierarchicalRank);
	for (unordered_map <wstring ,int >::iterator idi=topHierarchyClassIndexes.begin(),idiEnd=topHierarchyClassIndexes.end(); idi!=idiEnd; idi++)
	{
		chosen|=(rdfTypes[idi->second]->preferred=p==rdfTypes[idi->second]->confidence && r==rdfTypes[idi->second]->cli->second.ontologyHierarchicalRank);
	}

	p=100;
	r=1000;
	for (vector <cTreeCat *>::iterator rdfi=rdfTypes.begin(),rdfiEnd=rdfTypes.end(); rdfi!=rdfiEnd; rdfi++)
		if ((*rdfi)->cli->first!=SEPARATOR)
			p=min(p,(*rdfi)->confidence);
	for (vector <cTreeCat *>::iterator rdfi=rdfTypes.begin(),rdfiEnd=rdfTypes.end(); rdfi!=rdfiEnd; rdfi++)
		if (p==(*rdfi)->confidence)
			r=min(r,(*rdfi)->cli->second.ontologyHierarchicalRank);
	for (vector <cTreeCat *>::iterator rdfi=rdfTypes.begin(),rdfiEnd=rdfTypes.end(); rdfi!=rdfiEnd; rdfi++)
		chosen|=((*rdfi)->preferredUnknownClass=(*rdfi)->cli->first!=SEPARATOR && p==(*rdfi)->confidence && r==(*rdfi)->cli->second.ontologyHierarchicalRank);
	return chosen;
}


void Ontology::printIdentity(wstring object)
{ LFS
	replace(object.begin(),object.end(),L' ',L'_');
	vector <cTreeCat *> rdfTypes;
	Ontology::rdfIdentify(object,rdfTypes,L"0");
	unordered_map <wstring ,int > topHierarchyClassIndexes;
	if (!isolateKnownClasses(topHierarchyClassIndexes,rdfTypes,0))
		includeAllSuperClasses(topHierarchyClassIndexes,rdfTypes,1,0);
	setPreferred(topHierarchyClassIndexes,rdfTypes);
	if (rdfTypes.empty())
		lplog(LOG_INFO,L"%s:rdfType not found",object.c_str());
	else
		for (unsigned int I=0; I<rdfTypes.size(); I++)
		{
			wstring tmpstr;
			if (rdfTypes[I]->cli->first==SEPARATOR)
				lplog(LOG_WIKIPEDIA,L"----------------------------------------------------");
			else
			{
				rdfTypes[I]->lplogTC(LOG_WIKIPEDIA,object);
				//unordered_map <wstring, dbs>::iterator clisc;
				//if (rdfTypes[I]->cli->second.superClasses.size()>0)
				//{
				//		for (vector <wstring>::iterator sci=rdfTypes[I]->cli->second.superClasses.begin(),sciEnd=rdfTypes[I]->cli->second.superClasses.end(); sci!=sciEnd; sci++)
				//			if ((clisc=findCategory(*sci))!=dbPediaOntologyCategoryList.end()) 
				//				lplog(LOG_WIKIPEDIA,L"  %s:ISTYPE %s:%s:%s:[%s]:rank %d",sci->c_str(),clisc->first.c_str(),ontologyTypeString(clisc->second.ontologyType),clisc->second.compactLabel.c_str(),vectorString(clisc->second.superClasses,tmpstr).c_str(),clisc->second.rank);
				//}
			}
		}
	for (unordered_map <wstring ,int >::iterator idi=topHierarchyClassIndexes.begin(),idiEnd=topHierarchyClassIndexes.end(); idi!=idiEnd; idi++)
		rdfTypes[idi->second]->logIdentity(LOG_INFO,object,true);
	if (topHierarchyClassIndexes.empty() && rdfTypes.size())
	{
		unsigned int ontologyHierarchicalRank=rdfTypes[0]->cli->second.ontologyHierarchicalRank;
		// get lowest rank
		for (unsigned int I=0; I<rdfTypes.size(); I++)
			ontologyHierarchicalRank=min(ontologyHierarchicalRank,(unsigned)rdfTypes[I]->cli->second.ontologyHierarchicalRank);
		for (unsigned int I=0; I<rdfTypes.size(); I++)
			if (ontologyHierarchicalRank>=(unsigned)rdfTypes[I]->cli->second.ontologyHierarchicalRank)
				rdfTypes[I]->logIdentity(LOG_INFO,object,false);
	}
	if (!Ontology::cacheRdfTypes)
	  for (unsigned int I=0; I<rdfTypes.size(); I++)
	  	delete rdfTypes[I]; // now caching them
}

void Ontology::printIdentities(wchar_t *objects[])
{ LFS
	for (int I=0; objects[I]; I++)
		printIdentity(objects[I]);
}

#ifdef TEST_CODE
void Ontology::compareRDFTypes()
{
  // read in new 
	wchar_t path[4096];
	_snwprintf(path, MAX_LEN, L"%s\\dbPediaCache\\_rdfTypes", CACHEDIR);
	int fd = _wopen(path, O_RDWR | O_BINARY);
	void *vBuffer;
	int bufferlen = filelength(fd), where = 0;
	vBuffer = (void *)tmalloc(bufferlen + 10);
	::read(fd, vBuffer, bufferlen);
	_close(fd);
	int numTypes[5],numOriginalTypes[5];
	memset(numTypes, 0, 5 * (sizeof(*numTypes)));
	memset(numOriginalTypes, 0, 5 * (sizeof(*numOriginalTypes)));
	where += 2;
	wstring name;
	int lastProgressPercent = -1;
	unordered_map <wstring, dbs>::iterator hint = dbPediaOntologyCategoryList.end();
	int numAbstractDescriptions = 0, numCommentDescriptions = 0, numInfoPages = 0, numOntologyHierarchicalRank = 0, numSuperClasses = 0;
	while (where < bufferlen)
	{
		if (((__int64)where * 100 / (__int64)bufferlen) > (__int64)lastProgressPercent)
		{
			lastProgressPercent = ((__int64)where * 100 / (__int64)bufferlen);
			wprintf(L"PROGRESS: %03d%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \r", lastProgressPercent, where, bufferlen, clocksec(), memoryAllocated);
		}
		if (!::copy(hint, vBuffer, where, bufferlen, dbPediaOntologyCategoryList))
			lplog(LOG_FATAL_ERROR, L"Cannot read ontology relations.");
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
	lplog(LOG_WHERE, L"ontology: numAbstractDescriptions=%d,numCommentDescriptions=%d,numInfoPages=%d,numOntologyHierarchicalRank=%d,numSuperClasses=%d",
		numAbstractDescriptions, numCommentDescriptions, numInfoPages, numOntologyHierarchicalRank, numSuperClasses);
	for (int I = 1; I < 5; I++)
		lplog(LOG_WHERE, L"ontology: numType=%d", numTypes[I]);
	wprintf(L"PROGRESS: 100%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \n", where, bufferlen, clocksec(), memoryAllocated);
	//////////////////////////////////// original
	_snwprintf(path, MAX_LEN, L"%s\\dbPediaCache\\_rdfTypes - original", CACHEDIR);
	fd = _wopen(path, O_RDWR | O_BINARY);
	bufferlen = filelength(fd), where = 0;
	vBuffer = (void *)tmalloc(bufferlen + 10);
	::read(fd, vBuffer, bufferlen);
	_close(fd);
	where += 2;
	lastProgressPercent = -1;
	unordered_map <wstring, dbs> originalDbPediaOntologyCategoryList;
	hint = originalDbPediaOntologyCategoryList.end();
	int numOriginalAbstractDescriptions = 0, numOriginalCommentDescriptions = 0, numOriginalInfoPages = 0, numOriginalOntologyHierarchicalRank = 0, numOriginalSuperClasses = 0;
	while (where < bufferlen)
	{
		if (((__int64)where * 100 / (__int64)bufferlen) > (__int64)lastProgressPercent)
		{
			lastProgressPercent = ((__int64)where * 100 / (__int64)bufferlen);
			wprintf(L"PROGRESS: %03d%% %d out of %d original ontology relation bytes read with %I64d seconds elapsed (%d bytes) \r", lastProgressPercent, where, bufferlen, clocksec(), memoryAllocated);
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
	lplog(LOG_WHERE, L"ontology: numOriginalAbstractDescriptions=%d,numOriginalCommentDescriptions=%d,numOriginalInfoPages=%d,numOriginalOntologyHierarchicalRank=%d,numOriginalSuperClasses=%d",
		numOriginalAbstractDescriptions, numOriginalCommentDescriptions, numOriginalInfoPages, numOriginalOntologyHierarchicalRank, numOriginalSuperClasses);
	for (int I = 1; I < 5; I++)
		lplog(LOG_WHERE, L"ontology: numOriginalType=%d", numOriginalTypes[I]);
	wprintf(L"PROGRESS: 100%% %d out of %d ontology relation bytes read with %d seconds elapsed (%I64d bytes) \n", where, bufferlen, clocksec(), memoryAllocated);
	tfree(bufferlen + 10, vBuffer);
}

void Ontology::testWikipedia()
{ LFS
	//t.traceWikipedia=true;
	initialize();
	wchar_t *canadianTerritories[]={ L"Nunavut",	L"Quebec", L"Northwest Territories",L"Ontario", L"British Columbia", L"Alberta",
	 L"Saskatchewan", L"Manitoba", L"Yukon",L"Newfoundland and Labrador", L"New Brunswick",L"Nova Scotia", L"Prince Edward Island",NULL }; // CANADIAN_PROVINCE_CITY
	printIdentities(canadianTerritories);
	wchar_t *countries[]={ L"Russia",	L"Canada", L"France",L"Germany",NULL }; // COUNTRY
	printIdentities(countries);
	wchar_t *islands[]={ L"Greenland",L"New Guinea",L"Borneo",L"Madagascar",L"Baffin Island",L"Sumatra",NULL }; // ISLAND
	printIdentities(islands);
	wchar_t *mountains[]={ L"Mount Everest",L"K2",L"Kangchenjunga",L"Lhotse",L"Makalu",L"Cho Oyu",L"Dhaulagiri",L"Manaslu",L"Nanga Parbat",L"Annapurna",NULL }; // MOUNTAIN_RANGE_PEAK_LANDFORM
	printIdentities(mountains);
	// dbPedia is rather bad at identifying the Oceans, which is actually not a super big deal
	// wchar_t *oceans[]={ L"Pacific Ocean",L"Atlantic Ocean",L"Indian Ocean",L"Southern Ocean",L"Arctic Ocean",NULL }; // OCEAN_SEA
	// printIdentities(oceans);
  wchar_t *parks[]={ L"Acadia",L"American Samoa",L"Arches",L"Badlands",L"Big Bend",L"Biscayne",L"Black Canyon of the Gunnison",L"Bryce Canyon",L"Canyonlands",
										 L"Capitol Reef",L"Carlsbad Caverns",L"Channel Islands",L"Congaree",L"Crater Lake",L"Cuyahoga Valley",L"Death Valley",
										 L"Denali",L"Dry Tortugas",L"Everglades",L"Gates of the Arctic",L"Glacier",L"Glacier Bay",L"Grand Canyon",L"Grand Teton",
										 L"Great Basin",L"Great Sand Dunes",L"Great Smoky Mountains",L"Guadalupe Mountains",L"Haleakalā",L"Hawaii Volcanoes",L"Hot Springs",L"Isle Royale",
										 L"Joshua Tree",L"Katmai",L"Kenai Fjords",L"Kings Canyon",L"Kobuk Valley",L"Lake Clark",L"Lassen Volcanic",L"Mammoth Cave",
										 L"Mesa Verde",L"Mount Rainier",L"North Cascades",L"Olympic",L"Petrified Forest",L"Redwood",L"Rocky Mountain",L"Saguaro",
										 L"Sequoia",L"Shenandoah",L"Theodore Roosevelt",L"Virgin Islands",L"Voyageurs",L"Wind Cave",L"Yellowstone",L"Yosemite",L"Zion",NULL }; // PARK_MONUMENT
	printIdentities(parks);
	wchar_t *rivers[]={ L"Alsek",L"Apalachicola",L"Chattahoochee",L"Flint",L"Colorado",L"Columbia",L"Okanagan",L"Kettle",
										L"Pend Oreille",L"Kootenay",L"Canoe",L"Kicking Horse",L"Dean",L"Embudo River",L"Fraser",L"Pitt",
										L"Thompson",L"Chilcotin",L"Quesnel",L"Nechako",L"Liard",L"Mackenzie",L"Liard River",L"Slave",
										L"Peace",L"Athabasca",L"Mississippi",L"Missouri",L"Yellowstone River",L"Platte River",L"Ohio",L"Nass",
										L"Rio Grande",L"Sacramento",L"Pit",L"Feather",L"Saskatchewan",L"Skagit",L"Skeena",L"Babine",
										L"Bulkley",L"Morice",L"Kitwanga",L"Zymoetz",L"Squamish",L"St. Johns",L"St. Lawrence",L"Ottawa River",L"Yukon",NULL }; // RIVER_LAKE_WATERWAY
	printIdentities(rivers);
	wchar_t *cities[]={ L"New York",L"Los Angeles",L"Chicago",L"Houston",L"Philadelphia",L"Phoenix",L"San Antonio",L"San Diego",L"Dallas",L"San Jose",NULL }; // US_CITY_TOWN_VILLAGE
	printIdentities(cities);
	wchar_t *states[]={ L"Arizona",L"California",L"Florida",L"Illinois",L"Indiana",L"New York",L"North Carolina",L"Ohio",L"Pennsylvania",L"Texas",NULL }; //	US_STATE_TERRITORY_REGION
	printIdentities(states);
	wchar_t *worldCities[]={ L"Paris",L"Marseille",L"Lyon",L"Toulouse",L"Nice",L"Nantes",L"Strasbourg",L"Montpellier",L"Bordeaux",L"Lille",NULL }; // WORLD_CITY_TOWN_VILLAGE
	printIdentities(worldCities);

		printIdentity(L"Paul_Krugman");
	printIdentity(L"Irving_Berlin");
	printIdentity(L"Sting");
		printIdentity(L"IMG");
		printIdentity(L"AMT");
		printIdentity(L"Blondie");
printIdentity(L"Darrell_Hammond");
printIdentity(L"Curveball");
		printIdentity(L"Jay-Z");
		printIdentity(L"International_Management_Group");
		printIdentity(L"IMG");
		printIdentity(L"U.S._Mint");
		printIdentity(L"3M");
		printIdentity(L"Merrill_Lynch");
		printIdentity(L"WWE");
		printIdentity(L"Sago_Mine");
printIdentity(L"Robert_Blake");
printIdentity(L"March_Madness");
		printIdentity(L"Harriet_Miers");
printIdentity(L"USS_Abraham_Lincoln");
		printIdentity(L"Abraham_Lincoln");
		printIdentity(L"Dulles_Airport");
		printIdentity(L"Boston_Pops");
		printIdentity(L"Cunard_Cruise_Lines");
		printIdentity(L"2004_Baseball_World_Series");
		printIdentity(L"Baseball_World_Series");
		printIdentity(L"World_Series");
		printIdentity(L"Jeopardy");
		printIdentity(L"Harry_Potter_and_the_Goblet_of_Fire");
		printIdentity(L"Jasper_Fforde");
		printIdentity(L"Guinness_Brewery");
		printIdentity(L"Michael_Brown");
		printIdentity(L"Ella_Fitzgerald");
		printIdentity(L"CSPI");
		printIdentity(L"Fulbright_Program");
		printIdentity(L"Mohammed");
		printIdentity(L"Lyme_disease");
		printIdentity(L"American_Girl");
		printIdentity(L"Kurt_Weill");
		printIdentity(L"House_of_Chanel");
		printIdentity(L"British_American_Tobacco");
		printIdentity(L"BAT");
		printIdentity(L"Buffalo_Soldiers");
		printIdentity(L"2005_DARPA_Grand_Challenge");
		printIdentity(L"DARPA_Grand_Challenge");
		printIdentity(L"Egypt");
		printIdentity(L"2005_World_Snooker_Championships");
		printIdentity(L"World_Snooker_Championships");
		printIdentity(L"Teenage_Mutant_Ninja_Turtles");
		printIdentity(L"TMNT");
		printIdentity(L"Marsupial");
		printIdentity(L"Kumquat");
		printIdentity(L"Ayn_Rand");
		printIdentity(L"Alan_Greenspan");
		printIdentity(L"Mahmud_Ahmadinejad");
		printIdentity(L"Rafik_Hariri");
		exit(0);
}


int Ontology::testDBPediaPath(int where, wstring webAddress, wstring &buffer, wstring epath)
{
	LFS
		//int timer=clock(); 	
		int bw = -1;
	while ((bw = epath.find_first_of(L"/*?\"<>|,&-")) != wstring::npos)
		epath[bw] = L'_';
	wstring filePathOut, headers;
	int retValue = testWebPath(where, webAddress, epath, L"dbPediaCache", filePathOut, headers);
	if (buffer.find(L"SPARQL compiler") != wstring::npos)
	{
		_wremove(filePathOut.c_str());
		lplog(LOG_ERROR, L"PATH %s:\n%s", epath.c_str(), buffer.c_str());
		return -1;
	}
	return retValue;
}
#endif