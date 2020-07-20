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
//#include <locale>
#include "profile.h"
#include "ontology.h"
#include <share.h>
#include "internet.h"
#include "QuestionAnswering.h"

extern int logQuestionDetail; // not protected - too intensive to protect and doesn't matter
extern int logSemanticMap; // not protected - too intensive to protect and doesn't matter
bool unlockTables(MYSQL &mysql);
#define MAX_PATH_LEN 2048
#define MAX_BUF 2000000
//extern wstring basehttpquery; // initialized
//extern wstring prefix_colon; // initialized
//extern wstring prefix_owl_ontology; // initialized
//extern wstring prefix_dbpedia; // initialized
//extern wstring selectWhere; // initialized

	/* test wikipedia reduction */
	//wchar_t webAddress[1024];
	//int ret; 
	//wstring buffer;
	//_snwprintf(webAddress,MAX_LEN,L"http://en.wikipedia.org/wiki/Special:Search?search=%s&printable=yes&redirect=no",L"Paul_Krugman"); 
	//_snwprintf(webAddress,MAX_LEN,L"http://en.wikipedia.org/wiki/Special:Search?search=%s&printable=yes&redirect=no",L"James_Michener"); 
	//_snwprintf(webAddress,MAX_LEN,L"http://en.wikipedia.org/wiki/Curveball_(informant)"); 
	//if (ret=readPage(webAddress,buffer)) return ret;
	//int reduceWikipediaPage(wstring &buffer);
	//Words.TABLE=Words.predefineWord(L"lpTABLE"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	//Words.END_COLUMN=Words.predefineWord(L"lpENDCOLUMN"); // used to end each column string which is extracted from <table> and table-like constructions in HTML
	//Words.END_COLUMN_HEADERS=Words.predefineWord(L"lpENDCOLUMNHEADERS"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	//Words.MISSING_COLUMN=Words.predefineWord(L"lpMISSINGCOLUMN"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	//reduceWikipediaPage(buffer);

extern wchar_t *lpOntologySuperClasses[]; // initialized
bool copy(unordered_map <wstring, cOntologyEntry>::iterator &hint,void *buf,int &where,int limit,unordered_map <wstring, cOntologyEntry> &hm);


// lastErrorMsg
wstring lastErrorMsg()
{ LFS
  LPVOID lpvMessageBuffer;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
          NULL, GetLastError(),
          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPTSTR)&lpvMessageBuffer, 0, NULL);
  wstring error=(wchar_t *)lpvMessageBuffer;
  LocalFree(lpvMessageBuffer);
	return error;
}

void convertIllegalChars(wchar_t *path)
{ LFS
	int len = wcslen(path);
	wchar_t *pch = wcstok (path,WCHAR_ILLEGAL_PATH_CHARS);
	if (!pch ||  len>= MAX_PATH_LEN) return;
	wchar_t newPath[MAX_PATH_LEN];
	len=0;
  while (pch != NULL && len<MAX_PATH_LEN)
  {
		if (len + wcslen(pch) >= MAX_PATH_LEN)
			break;
    wcscpy(newPath+len,pch);
		len+=wcslen(pch);
		newPath[len++]=L'!';
		pch = wcstok (NULL,WCHAR_ILLEGAL_PATH_CHARS);
  }
	if (len>MAX_PATH_LEN)
		lplog(LOG_FATAL_ERROR, L"conversion overwrite");
	if (len) newPath[--len]=0;
	wcscpy(path,newPath);
	for (int I=0; path[I]; I++)
		if (!iswalnum(path[I]) && path[I]!=L'.' && path[I] != L'-' && path[I] != L'—') // not all unicode dashes may be allowed in a Windows path, so not using isDash
			path[I]=L'_';
}

void deleteIllegalChars(char *path)
{ LFS
	int len = strlen(path);
	char *pch = strtok(path, ILLEGAL_PATH_CHARS);
	if (!pch || len >= MAX_PATH_LEN) return;
	char newPath[MAX_PATH_LEN];
	len=0;
  while (pch != NULL && len<MAX_PATH_LEN)
  {
		if (len + strlen(pch) >= MAX_PATH_LEN)
			break;
		strcpy(newPath + len, pch);
		len+=strlen(pch);
    pch = strtok (NULL, ILLEGAL_PATH_CHARS);
  }
	strcpy(path,newPath);
	for (int I=0; path[I]; I++)
		if (!isalnum((unsigned char)path[I]) && path[I]!='.' && path[I] != '-' && path[I] != L'—') // not all unicode dashes may be allowed in a Windows path, so not using isDash
			path[I]='_';
}

void convertUnderlines(wstring &buffer)
{ LFS
	for (unsigned int I=0; I<buffer.length(); I++)
		if (buffer[I]==L'_') buffer[I]=L' ';
}

void eliminateHTML(wstring &buffer);
// process the immediately preceding header: <h2>Contents</h2>
// also process this: <div id="toctitle"><h2>Contents</h2></div>
void processHeader(wstring &buffer,size_t &whereHeadingEnd,wstring &header,bool &tableOfContentsFlag)
{ LFS
  size_t beginPos=0;
  wstring tableMetaHeader,match; // a header table that comes on the right and can be ignored for now.
	firstMatch(buffer,L"<table class=\"metadata plainlinks mbox-small",L"</table>",beginPos,match,true);
	while (true)
	{
		while (whereHeadingEnd && iswspace(buffer[whereHeadingEnd])) whereHeadingEnd--;
		if (whereHeadingEnd > 10 && buffer[whereHeadingEnd - 5] == L'<' && buffer[whereHeadingEnd - 4] == L'/' && buffer[whereHeadingEnd - 3] == L'd' && buffer[whereHeadingEnd - 2] == L'i' && buffer[whereHeadingEnd - 1] == L'v' && buffer[whereHeadingEnd] == L'>')
		{
			whereHeadingEnd -= 6;
			while (whereHeadingEnd && iswspace(buffer[whereHeadingEnd])) whereHeadingEnd--;
		}
		else break;
	}
	if (whereHeadingEnd>10 && buffer[whereHeadingEnd]==L'>' && buffer[whereHeadingEnd-2]==L'h' && buffer[whereHeadingEnd-3]==L'/' && buffer[whereHeadingEnd-4]==L'<')
	{
		// scan for the start of the header
		wstring headerStart=L"<h";
		headerStart+=buffer[whereHeadingEnd-1];
		size_t whereHeadingBegin=buffer.rfind(headerStart,whereHeadingEnd);
		if (whereHeadingBegin!=wstring::npos)
		{
			header=buffer.substr(whereHeadingBegin,whereHeadingEnd-whereHeadingBegin+1);
			tableOfContentsFlag = header.find(L"mw-toc-heading") != wstring::npos;
			buffer.erase(whereHeadingBegin,whereHeadingEnd-whereHeadingBegin+1);
			whereHeadingEnd=whereHeadingBegin;
			wstring title;
			takeLastMatch(header,L"<span class=\"mw-headline\"",L"</span>",title,false);
			if (title.length()>0)
			{
				int wh=title.find(L">");
				if (wh>=0) title.erase(0,wh+1);
				header=title;
			}
			eliminateHTML(header);
		}
	}
}

// James Michener
// <h3><span class="editsection">[<a href="">edit</a>]</span> <span class="mw-headline" id="Books_.E2.80.94_fiction">Books — fiction</span></h3>
// <table class="wikitable sortable">
//<tr>
//<th>Book Title</th>
//<th>Year Published</th>
//</tr>
//<tr>
//<td><i><a href="/wiki/Tales_of_the_South_Pacific" title="Tales of the South Pacific">Tales of the South Pacific</a></i></td>
//<td>1947</td>
//</tr>
//</table>
// Spain
//<div class="rellink boilerplate seealso">See also: <a href="/wiki/List_of_metropolitan_areas_in_Spain_by_population" title="List of metropolitan areas in Spain by population" class="mw-redirect">List of metropolitan areas in Spain by population</a></div>
//<p>Source: <a href="/wiki/European_Spatial_Planning_Observation_Network" title="European Spatial Planning Observation Network" class="mw-redirect">ESPON</a>, 2007<sup id="cite_ref-129" class="reference"><a href="#cite_note-129"><span>[</span>119<span>]</span></a></sup></p>
//<table class="wikitable">
//<tr style="text-align:right;">
//<th>Pos.</th>
//<th>City</th>
//<th>Region</th>
//<th>Prov.</th>
//<th>population</th>
//</tr>
void scanForTables(wstring &buffer,vector < vector <wstring> > &tables,bool unordered);
void interpretHTMLTable(wstring &buffer,size_t &whereHeadingEnd,wstring &match,vector < vector <wstring> > &tables)
{ LFS
	vector <wstring> table;
	wstring tableHeader;
	bool tableOfContentsFlag=false;
	processHeader(buffer,whereHeadingEnd,tableHeader,tableOfContentsFlag);
	table.push_back(tableHeader);
	wstring columnHeaders,columnHeader;
	size_t rowPosition=0,beginColumnHeader=0;
	if (firstMatch(match,L"<tr ",L"</tr>",rowPosition,columnHeaders,false)<0) return;
	while (firstMatch(columnHeaders,L"<th>",L"</th>",beginColumnHeader,columnHeader,false)>=0)
	{
		scanForTables(columnHeader,tables,true);
		scanForTables(columnHeader,tables,false);
		eliminateHTML(columnHeader);
		table.push_back(columnHeader);
	}
	int numColumns=table.size()-1;
	size_t beginColumn=0;
	if (!numColumns)
		return;
	table.push_back(Words.END_COLUMN_HEADERS->first);
	wstring columns,column;
	while (firstMatch(match,L"<tr ",L"</tr>",rowPosition,columns,false)>=0) 
	{
		int numColumn;
		for (numColumn=0,beginColumn=0; numColumn<numColumns && firstMatch(columns,L"<td",L"</td>",beginColumn,column,false)>=0; numColumn++)
		{
			// eliminate until the '>'
			size_t gt=column.find(L'>');
			if (gt!=wstring::npos)
				column.erase(0,gt+1);
			scanForTables(column,tables,true);
			scanForTables(column,tables,false);
			eliminateHTML(column);
			table.push_back(column);
		}
		for (; numColumn<numColumns; numColumn++)
			table.push_back(Words.MISSING_COLUMN->first);
	}
	if (table.size()>1)
		tables.push_back(table);
}

void eliminateHTMLCharacterEntities(wstring &buffer);
void eliminateHTML(wstring &buffer)
{ LFS

	eliminateHTMLCharacterEntities(buffer);
	wstring noHTML;
	bool inHTML=false;
	for (unsigned int I=0; I<buffer.length(); I++)
		if (buffer[I]==L'<')
			inHTML=true;
		else if (buffer[I]==L'>')
			inHTML=false;
		else if (!inHTML)
		{
			// eliminate footnotes
			if (buffer[I]==L'[' && iswdigit(buffer[I+1]) && (buffer[I+2]==L']' || (iswdigit(buffer[I+2]) && buffer[I+3]==L']')))
			{
				I+=2;
				if (buffer[I+1]==L']') I++;
				continue;
			}
			noHTML+=buffer[I];
		}
	buffer=noHTML;
}

// JK Rowling / Paul Krugman
// <h3> <span class="mw-headline" id="Articles">Articles</span></h3>
//<ul> OR <ol>
//<li>1997: <a href="/wiki/Nestl%C3%A9_Smarties_Book_Prize" title="Nestlé Smarties Book Prize">Nestlé Smarties Book Prize</a>, Gold Award for <i>Harry Potter and the Philosopher's Stone</i></li>
// <h3> is not consistent.  <li> must be consecutive.
void scanForTables(wstring &buffer, vector < vector <wstring> > &tables, bool unordered)
{
	LFS
	size_t tablePosition=0;
	wstring tableHtml;
	wchar_t *beginTable,*endTable;
	if (unordered)
	{
		beginTable=L"<ul>";
		endTable=L"</ul>";
	}
	else
	{
		beginTable=L"<ol>";
		endTable=L"</ol>";
	}
	while (firstMatchNonEmbedded(buffer,beginTable,endTable,tablePosition,tableHtml,false)>=0) 
	{
		vector <wstring> table;
		wstring tableHeader,column;
		bool tableOfContentsFlag = false;
		processHeader(buffer,tablePosition,tableHeader,tableOfContentsFlag);
		if (tableOfContentsFlag)
			tableHeader=Words.TOC_HEADER->first+ L" " + tableHeader;
		table.push_back(tableHeader);
		table.push_back(Words.END_COLUMN_HEADERS->first);
		size_t rowPosition=0;
		wstring row;
		while (firstMatch(tableHtml,L"<li",L"</li>",rowPosition,row,false)>=0) 
		{
			int wh=row.find(L">");
			if (wh>=0) row.erase(0,wh+1);
			eliminateHTML(row);
			table.push_back(row);
		}
		if (table.size()>0)
			tables.push_back(table);
	}
}

#define MAX_LEN 2048
// In the future this may use the alternative interface http://en.wikipedia.org/wiki/Special:Export/Curveball_(informant) which obeys alternative MediaMiki parsing rules: http://www.mediawiki.org/wiki/Markup_spec
//
// search for Results 1-
//   if found, look for each page in:
// <li style="padding-bottom: 1em;"><a href="/wiki/The_Sinking_of_the_Lusitania:_Terror_at_Sea" title="The Sinking of the Lusitania: Terror at Sea">The Sinking of the Lusitania: Terror at Sea</a><br /><span style="color: green; font-size: small;">Relevance: 100.0% -  - </span></li>
// search for Relevance: 100.0%
//        and <a href="/wiki/The_Sinking_of_the_Lusitania:_Terror_at_Sea" title
// all matches until relevance<80% or
// <a href="/wiki/Lusitania_%28disambiguation%29" title FOUND (disambiguation)
//
// OR
// if Results not found:
// search for each <p> - </p> pair
// remove <b> </b> , <a href > </a> <i> </i>
// concatenate
// <p><b>Allies</b> spelled with a capital "A", usually denotes the countries who fought together against the <a href="/wiki/Central_Powers" title="Central Powers">Central Powers</a> in <a href="/wiki/World_War_I" title="World War I">World War I</a> (see <a href="/wiki/Triple_Entente" title="Triple Entente">Triple Entente</a> or <a href="/wiki/Allies_of_World_War_I" title="Allies of World War I">Allies of World War I</a>), or those who fought against the <a href="/wiki/Axis_powers_of_World_War_II" title="Axis powers of World War II">Axis Powers</a> in <a href="/wiki/World_War_II" title="World War II">World War II</a>.</p>
//
// 
int reduceWikipediaPage(wstring &buffer)
{ LFS
	vector < vector <wstring> > tables; // first entry is the heading, if no heading the first entry is empty
	wstring match;
	takeLastMatch(buffer,L"<!-- start content -->",L"<!-- end content -->",match,false);
	if (match.length())
		buffer=match;
	takeLastMatch(buffer,L"<!-- bodyContent -->",L"<!-- /bodyContent -->",match,false);
	if (match.length())
		buffer=match;
	takeLastMatch(buffer,L"<!-- printfooter -->",L"<!-- /printfooter -->",match,false);
	takeLastMatch(buffer,L"<!-- catlinks -->",L"<!-- /catlinks -->",match,false);
	while (takeLastMatch(buffer, L"<style", L"</style>", match, false) >= 0);
	takeLastMatch(buffer,L"<div class=\"floatnone\">",L"</div>",match,false);
	takeLastMatch(buffer,L"<div style=\"float: left;\">",L"</div>",match,false);
	takeLastMatch(buffer,L"<div style=\"margin-left: 60px;\">",L"</div>",match,false);
	takeLastMatch(buffer,L"<div class=\"infobox sisterproject\" style=\"float:right;\">",L"</div>",match,false);
	takeLastMatch(buffer,L"<div id=\"mwe_player", L"</div>", match, false);
	while (takeLastMatch(buffer, L"<div class=\"thumb", L"</div>", match, false) >= 0);
	takeLastMatch(buffer,L"<form id=\"powersearch\" method=\"get\" action=\"/wiki/Special:Search\">",L"</form>",match,false);
	takeLastMatch(buffer, L"<span class=\"toctogglespan\">", L"</span>", match, false);
	//takeLastMatch(buffer,L"<ol ",L"</ol>",match,false);
	size_t pos=wstring::npos;
	while (firstMatch(buffer,L"<!--",L"-->",pos,match,false)>=0);
	while (firstMatch(buffer,L"<table",L"</table>",pos,match,false)>=0)
		interpretHTMLTable(buffer,pos,match,tables);
	while (firstMatch(buffer,L"<sup",L"</sup>",pos,match,false)>=0);
	if (buffer.find(L"Results 1-",0)==wstring::npos)
	{
		// keep apostrophes
		// <span style="padding-left:0.1em;">'</span>s
		scanForTables(buffer,tables,true);
		scanForTables(buffer,tables,false);
		while ((pos=takeLastMatch(buffer,L"<span",L"</span>",match,false))!=wstring::npos) // span elements can be embedded in one another
		{
			int onlyText=match.find('>'); // don't scan what is in the HTML, only what should be text
			if (onlyText!=wstring::npos) match.erase(0,onlyText+1);
			eliminateHTML(match);
			if (match.size()>5) // make sure it is not a raised symbol like 'r' or something which is not actual text
				buffer.insert(pos,match); // eliminates Relevance %
		}
		wstring justPara;
		while (!nextMatch(buffer,L"<p>",L"</p>",pos,match,false)) justPara+=match+L"\n  "; 
		buffer=justPara;
		eliminateHTML(buffer);
		while ((pos=buffer.find(L": )"))!=wstring::npos)
			buffer.replace(pos,3,L")");
		wstring tmpstr;
		for (unsigned int I=0; I<tables.size(); I++)
		{
			buffer+=L"\n\n\n"+Words.TABLE->first+L" "+itos(I,tmpstr)+L".\n\n";
			for (unsigned int J=0; J<tables[I].size(); J++)
				buffer+=tables[I][J]+L". "+Words.END_COLUMN->first+L".\n";
		}
	}
	else
	{
		// <li style="padding-bottom: 1em;"><a href="/wiki/1949_Armistice_Agreements" title="1949 Armistice Agreements">1949 Armistice Agreements</a><br />
		// <span style="color: green; font-size: small;">Relevance: 77.2% -  - </span></li>
		wstring final;
		pos=wstring::npos;
		for (int numPagesScanned=0; numPagesScanned<10 && !nextMatch(buffer,L"<li",L"</li>",pos,match,false); numPagesScanned++)
		{
			wstring nextWikiAddress;
			takeLastMatch(match,L"<a href=\"/wiki/",L"\" title",nextWikiAddress,false);
			wstring sRelevance;
			takeLastMatch(match,L"Relevance: ",L"%",sRelevance,false);
			if (sRelevance.length() && _wtoi(sRelevance.c_str())<70)
				break;
			wchar_t path[1024];
			int pathlen=_snwprintf(path,MAX_LEN,L"%s\\wikipediaCache",CACHEDIR)+1;
			_wmkdir(path);
			_snwprintf(path,MAX_LEN,L"%s\\wikipediaCache\\_%s.txt",CACHEDIR,nextWikiAddress.c_str());
			convertIllegalChars(path+pathlen);
			distributeToSubDirectories(path,pathlen,true);
			if (_waccess(path,0)<0)
			{
				wchar_t webAddress[1024];
				// http://en.wikipedia.org/w/index.php?title=Localized_versions_of_the_Monopoly_game&printable=yes
				// http://en.wikipedia.org/w/index.php?title=List_of_French_phrases_used_by_English_speakers&printable=yes
				_snwprintf(webAddress,MAX_LEN,L"http://en.wikipedia.org/w/index.php?title=%s&printable=yes",nextWikiAddress.c_str()); 
				lplog(LOG_WIKIPEDIA,L"%s",webAddress);
				int ret; 
				wstring secondaryBuffer;
				if (ret= cInternet::readPage(webAddress,secondaryBuffer)) return ret;
				reduceWikipediaPage(secondaryBuffer);
				int fd=_wopen(path,O_CREAT|O_RDWR|O_BINARY,_S_IREAD | _S_IWRITE );
				if (fd<0)
				{
					lplog(LOG_ERROR,L"ERROR:Cannot create path %s - %S (5).",path,sys_errlist[errno]);
					return cInternet::GETPAGE_CANNOT_CREATE;
				}
				_write(fd,secondaryBuffer.c_str(),secondaryBuffer.length()*sizeof(secondaryBuffer[0]));
				_close(fd);
				convertUnderlines(nextWikiAddress);
				final+=nextWikiAddress+L"\n\n";
				final+=secondaryBuffer+L"\n\n\n";
			}
			else
			{
				wchar_t cBuffer[MAX_BUF];
				int actualLenInBytes;
				if (!getPath(path,cBuffer,MAX_BUF,actualLenInBytes))
				{
					cBuffer[actualLenInBytes/sizeof(buffer[0])]=0;
					convertUnderlines(nextWikiAddress);
					final+=nextWikiAddress+L"\n\n";
					final+=cBuffer+wstring(L"\n\n\n");
				}
			}
		}
		buffer=final;
	}
	if (buffer.find(L"For search options, see Help:Searching.")!=wstring::npos && buffer.length()<45)
		buffer.clear();
	return 0;
}

wstring vectorString(vector <wstring> &vstr, wstring &tmpstr, wstring separator)
{
	LFS
		tmpstr.clear();
	for (vector <wstring>::iterator sci = vstr.begin(), sciEnd = vstr.end(); sci != sciEnd; sci++)
		tmpstr += separator + *sci;
	return tmpstr;
}

string vectorString(vector <string> &vstr, string &tmpstr, string separator)
{
	LFS
		tmpstr.clear();
	for (vector <string>::iterator sci = vstr.begin(), sciEnd = vstr.end(); sci != sciEnd; sci++)
		tmpstr += separator + *sci;
	return tmpstr;
}

wstring vectorString(vector < vector <wstring> > &vstr, wstring &tmpstr, wstring separator)
{ LFS
  wstring tmpstr2;
	tmpstr.clear();
	for (vector < vector <wstring> >::iterator sci=vstr.begin(),sciEnd=vstr.end(); sci!=sciEnd; sci++)
		tmpstr+=L" ["+vectorString(*sci,tmpstr2,separator)+L"]";
	return tmpstr;
}

wstring setString(set <wstring> &sstr,wstring &tmpstr,wchar_t *separator)
{ LFS
	tmpstr.clear();
	for (set <wstring>::iterator sci=sstr.begin(),sciEnd=sstr.end(); sci!=sciEnd; sci++)
		tmpstr+=separator+*sci;
	return tmpstr;
}

string setString(set <string> &sstr, string &tmpstr, char *separator)
{ LFS
	tmpstr.clear();
	for (set <string>::iterator sci=sstr.begin(),sciEnd=sstr.end(); sci!=sciEnd; sci++)
		tmpstr += separator + *sci;
	return tmpstr;
}

const wchar_t *ontologyTypeString(int ontologyType,int resourceType,wstring &tmpstr)
{ LFS
	switch (ontologyType)
	{
	case dbPedia_Ontology_Type:tmpstr = L"DBPEDIA"; break;
	case YAGO_Ontology_Type:tmpstr = L"YAGO"; break;
	case UMBEL_Ontology_Type:tmpstr = L"UMBEL"; tmpstr += ('0' + resourceType); break;
	default:tmpstr = L"Unknown"; break;
	}
	return tmpstr.c_str();
}

wstring cTreeCat::toString(wstring &tmpstr)
{ LFS
	wstring tmpstr2,tmpstr3,tmpstr4,tmpstr5,tmpstr6,tmpstr7,tmpstr8;
	return tmpstr=parentObject+L":"+typeObject+((preferred) ? L"PREFERRED":L"")+((exactMatch) ? L"EMPREFERRED":L"")+L" ISTYPE "+cli->first+L":"+
		ontologyTypeString(cli->second.ontologyType, cli->second.resourceType,tmpstr8)+L":"+cli->second.compactLabel+
		L"Wikipedia:"+vectorString(wikipediaLinks,tmpstr5,L",")+
		L"Professions:"+vectorString(professionLinks,tmpstr6,L",")+
		L"[SUPER "+vectorString(cli->second.superClasses,tmpstr,L" ")+L"]:confidence,ontologyHierarchicalRank "+itos(cli->second.ontologyHierarchicalRank,tmpstr3)+L","+itos(confidence,tmpstr7)+L"("+qtype+L")";
}

void cTreeCat::lplogTC(int whichLog,wstring ofWhichObject)
{ LFS
	wstring tmpstr;
	if (ofWhichObject.length())
		::lplog(whichLog,L"%s:%s",ofWhichObject.c_str(),toString(tmpstr).c_str());
	else
		::lplog(whichLog,L"%s",toString(tmpstr).c_str());
}

int cSource::getExtendedRDFTypes(int where, vector <cTreeCat *> &rdfTypes, unordered_map <wstring, int > &topHierarchyClassIndexes, wstring fromWhere, bool ignoreMatches, bool fileCaching)
{
	LFS
		int lastLowestConfidence = -1;
	bool lastPreferenceFound = false;
	int maxPrepositionalPhraseNonMixMatch = -1;
	ppExtensionAvailable(where, maxPrepositionalPhraseNonMixMatch, true);
	bool contextualBase = (m[where].endObjectPosition - m[where].beginObjectPosition) >= 3;
	int minPrepositionalPhraseNonMixMatch = (maxPrepositionalPhraseNonMixMatch == 0 || contextualBase) ? 0 : 1;
	for (int extendNumPP=maxPrepositionalPhraseNonMixMatch; extendNumPP>=minPrepositionalPhraseNonMixMatch; extendNumPP--)
	{
		vector <cTreeCat *> tempRdfTypes;
		getRDFTypes(where,tempRdfTypes,fromWhere,extendNumPP, ignoreMatches, fileCaching);
		bool preferenceFound=true;
		int lowestConfidence=CONFIDENCE_NOMATCH;
		for (unsigned int r=0; r<tempRdfTypes.size(); r++)
			if (tempRdfTypes[r]->cli->first!=SEPARATOR)
			{
				preferenceFound|=tempRdfTypes[r]->preferred || tempRdfTypes[r]->exactMatch;
				lowestConfidence=min(lowestConfidence,tempRdfTypes[r]->confidence);
			}
		if (lastLowestConfidence<0 || (lastLowestConfidence>1 && lowestConfidence==1 && (preferenceFound || !lastPreferenceFound)) || (lastLowestConfidence==lowestConfidence))
		{
		  rdfTypes=tempRdfTypes;
			lastLowestConfidence=lowestConfidence;
			lastPreferenceFound=preferenceFound;
		}
		else
		{
			lastLowestConfidence=min(lastLowestConfidence,lowestConfidence);
			rdfTypes.insert(rdfTypes.end(),tempRdfTypes.begin(),tempRdfTypes.end());
		}
		// if the base is contextualized (large enough to form a single object on its own), then exit as soon as we have anything
		if (contextualBase && rdfTypes.size() > 0)
			break;
	}
	cOntology::includeSuperClasses(topHierarchyClassIndexes, rdfTypes);
	// reset lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms
	for (int I=0; I<rdfTypes.size(); I++)
		rdfTypes[I]->preferred=false;
	cOntology::setPreferred(topHierarchyClassIndexes,rdfTypes);
	return rdfTypes.size();
}

int cSource::getObjectRDFTypes(int object,vector <cTreeCat *> &rdfTypes,unordered_map <wstring ,int > &topHierarchyClassIndexes,wstring fromWhere)
{ LFS
	wstring tmpstr;
  objectString(object,tmpstr,true);
  cOntology::rdfIdentify(tmpstr,rdfTypes,fromWhere);
	cOntology::includeSuperClasses(topHierarchyClassIndexes, rdfTypes);
	// reset lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms
	for (int I=0; I<rdfTypes.size(); I++)
		rdfTypes[I]->preferred=false;
	cOntology::setPreferred(topHierarchyClassIndexes,rdfTypes);
	return rdfTypes.size();
}

int cSource::readExtendedRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes,unordered_map <wstring ,int > &topHierarchyClassIndexes)
{ LFS
	int fd,errorCode=_wsopen_s(&fd,path,O_RDWR|O_BINARY,_SH_DENYNO,_S_IREAD|_S_IWRITE );
	if (errorCode!=0)
	{
		//lplog(LOG_ERROR,L"Cannot open extended rdfTypes (%s) - %S (%d).",path,_sys_errlist[errno],errno);
		return -1;
	}
	void *buffer;
	int bufferlen=filelength(fd),where=0;
	buffer=(void *)tmalloc(bufferlen+10);
	::read(fd,buffer,bufferlen);
	_close(fd);
	if (*((wchar_t *)buffer)!=EXTENDED_RDFTYPE_VERSION) // version
	{
		tfree(bufferlen+10,buffer);
		_wremove(path);
		return -2;
	}
	where+=2;
	int rdfTypeCount;
	if (!::copy(rdfTypeCount,buffer,where,bufferlen))
		lplog(LOG_FATAL_ERROR,L"Cannot read count of extended rdfTypes (%s) - %S (%d).",path,_sys_errlist[errno],errno);
	for (int c=0; c<rdfTypeCount; c++)
	{
		cTreeCat *tc=new cTreeCat();
		if (!tc->copy(cOntology::dbPediaOntologyCategoryList,buffer,where,bufferlen)) 
			lplog(LOG_FATAL_ERROR, L"Cannot read tree cat of extended rdfTypes (%s) - %S (%d).", path, _sys_errlist[errno], errno);
		rdfTypes.push_back(tc);
	}
	int topHierarchyClassIndexesCount;
	if (!::copy(topHierarchyClassIndexesCount,buffer,where,bufferlen))
		lplog(LOG_FATAL_ERROR, L"Cannot read top index count of extended rdfTypes (%s) - %S (%d).", path, _sys_errlist[errno], errno);
	for (int c=0; c<topHierarchyClassIndexesCount; c++)
	{
		wstring hClass;
		int index=0;
		if (!::copy(hClass,buffer,where,bufferlen) ||
			!::copy(index,buffer,where,bufferlen)) 
			lplog(LOG_FATAL_ERROR, L"Cannot read top index of extended rdfTypes (%s) - %S (%d).", path, _sys_errlist[errno], errno);
		topHierarchyClassIndexes[hClass]=index;
	}
	tfree(bufferlen+10,buffer);
	return 0;
}

#define EMAX_BUF MAX_BUF*10
int cSource::writeExtendedRDFTypes(wchar_t path[4096],vector <cTreeCat *> &rdfTypes,unordered_map <wstring ,int > &topHierarchyClassIndexes)
{ LFS
	int fd,errorCode=_wsopen_s(&fd,path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,_SH_DENYNO,_S_IREAD|_S_IWRITE );
	if (errorCode!=0)
	{
		lplog(LOG_ERROR,L"Cannot write extended rdfTypes (%s) - %S (%d).",path,_sys_errlist[errno],errno);
		return -1;
	}
	char buffer[EMAX_BUF];
	int where=0;
	*((wchar_t *)buffer)=EXTENDED_RDFTYPE_VERSION;
	where+=2;
	if (!::copy(buffer,(int)rdfTypes.size(),where,EMAX_BUF))
		lplog(LOG_FATAL_ERROR, L"Cannot write extended rdfTypes count (%s) - %S (%d).", path, _sys_errlist[errno], errno);
	for (vector <cTreeCat *>::iterator ri=rdfTypes.begin(),riEnd=rdfTypes.end(); ri!=riEnd; ri++)
	{
		if (!(*ri)->copy(buffer,where,EMAX_BUF))
			lplog(LOG_FATAL_ERROR, L"Cannot write extended rdfTypes (%s) - %S (%d).", path, _sys_errlist[errno], errno);
		if (where>(EMAX_BUF)-8192)
		{
			_write(fd,buffer,where);
			where=0;
		}
	}
	if (!::copy(buffer,(int)topHierarchyClassIndexes.size(),where,EMAX_BUF))
		lplog(LOG_FATAL_ERROR, L"Cannot write extended rdfTypes top class count (%s) - %S (%d).", path, _sys_errlist[errno], errno);
	for (unordered_map <wstring ,int >::iterator ti=topHierarchyClassIndexes.begin(),tiEnd=topHierarchyClassIndexes.end(); ti!=tiEnd; ti++)
	{
		if (!::copy(buffer,ti->first,where,EMAX_BUF) || !::copy(buffer,ti->second,where,EMAX_BUF))
			lplog(LOG_FATAL_ERROR, L"Cannot write extended rdfTypes (%s) - %S (%d).", path, _sys_errlist[errno], errno);
		if (where>(EMAX_BUF)-8192)
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

wstring transformRDFTypeName(wstring &RDFType)
{
	transform (RDFType.begin (), RDFType.end (), RDFType.begin (), (int(*)(int)) tolower);
	replace(RDFType.begin (), RDFType.end (),L'_',L' ');
	size_t where=0;
	while (where<RDFType.length()-4 && (where=RDFType.find(L'$',where))!=wstring::npos)
	{
		if (where+4<RDFType.length() && iswxdigit(RDFType[where+1]) && iswxdigit(RDFType[where+2]) && iswxdigit(RDFType[where+3]) && iswxdigit(RDFType[where+4]))
		{
			wchar_t replaceCh=0;
			for (int I=where+1; I<where+5; I++)
				replaceCh=(replaceCh<<4)+((iswdigit(RDFType[I])) ? (RDFType[I]-L'0') : (RDFType[I]-'a'+10));
			RDFType.erase(where+1,4);
			RDFType[where]=replaceCh;
		}
		where++;
	}
	return RDFType;
}

bool cSource::categoryMultiWord(wstring &childWord, wstring &lastWord)
{
	int lastSpace = -1;
	// contains capitalized word?
	bool isLastCapitalized=true;
	for (int I = 0; I < childWord.length(); I++)
	{
		if (childWord[I] == L')' || childWord[I] == L'$')
			return false;
		// last space?
		if (childWord[I] == L' ' || childWord[I] == L'_')
		{
			lastSpace = I;
			isLastCapitalized = iswupper(childWord[I + 1])!=0;
		}
	}
	if (isLastCapitalized)
		return false;
	lastWord = childWord;
	lastWord.erase(0, lastSpace+1);
	return true;
}

// attempt to transform rdf types, wikipedia links and profession links into single words which can be matched to other words
void cSource::getRDFTypeSimplificationToWordAssociationWithObjectMap(wstring object,vector <cTreeCat *> &rdfTypes,unordered_map<wstring,int> &RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap)
{
	for (vector <cTreeCat *>::iterator rdfi=rdfTypes.begin(),rdfiEnd=rdfTypes.end(); rdfi!=rdfiEnd; rdfi++)
	{
		if ((*rdfi)->cli->first==SEPARATOR) 
			continue;
		wstring childWord=(*rdfi)->cli->first,transformedChildWord,lastWord;
		if (categoryMultiWord(childWord, lastWord))
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"RDFSimplificationToWordMapping MultiWord %-32s->%s confidence %d", childWord.c_str(), lastWord.c_str(), (*rdfi)->confidence << 1);
			RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[lastWord] = (*rdfi)->confidence << 1;
		}
		RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[transformedChildWord = transformRDFTypeName(childWord)] = (*rdfi)->confidence;
		if (logQuestionDetail)
			lplog(LOG_WHERE, L"RDFSimplificationToWordMapping %-32s->%s confidence %d", childWord.c_str(), transformedChildWord.c_str(), (*rdfi)->confidence );
		//(*rdfi)->lplog(LOG_WHERE,object+L" LLDEBUGQQ ["+transformedChildWord+L"]");

		for (vector <wstring>::iterator wli=(*rdfi)->wikipediaLinks.begin(),wliEnd=(*rdfi)->wikipediaLinks.end(); wli!=wliEnd; wli++)
		{
			childWord=*wli;
			if (categoryMultiWord(childWord, lastWord))
			{
				if (logQuestionDetail)
					lplog(LOG_WHERE, L"RDFSimplificationToWordMapping MultiWordWikipediaLink %-32s->%s confidence %d", childWord.c_str(), lastWord.c_str(), (*rdfi)->confidence << 1);
				RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[lastWord] = (*rdfi)->confidence << 1;
			}
			RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[transformedChildWord = transformRDFTypeName(childWord)] = (*rdfi)->confidence;
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"RDFSimplificationToWordMapping WikipediaLink %-32s->%s confidence %d", childWord.c_str(), transformedChildWord.c_str(), (*rdfi)->confidence);
		}
		for (vector <wstring>::iterator pli=(*rdfi)->professionLinks.begin(),pliEnd=(*rdfi)->professionLinks.end(); pli!=pliEnd; pli++)
		{
			childWord=*pli;
			if (categoryMultiWord(childWord, lastWord))
			{
				if (logQuestionDetail)
					lplog(LOG_WHERE, L"RDFSimplificationToWordMapping MultiWordProfessionLink %-32s->%s confidence %d", childWord.c_str(), lastWord.c_str(), (*rdfi)->confidence << 1);
				RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[lastWord] = (*rdfi)->confidence << 1;
			}
			RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[transformedChildWord = transformRDFTypeName(childWord)] = (*rdfi)->confidence;
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"RDFSimplificationToWordMapping WikipediaLink %-32s->%s confidence %d", childWord.c_str(), transformedChildWord.c_str(), (*rdfi)->confidence);
		}
	}
}

// accumulate RDF types for each object string defined by the source from the word positions (where - where+numWords).
// getExtendedRDFTypesMaster accumulates RDF types in a map per source called extendedRdfTypeMap.  
// the results are extracted from the map and returned in RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.
// in addition, the number of times each object RDFType is asked for is accumulated in extendedRdfTypeNumMap.
int cSource::getAssociationMapMaster(int where,int numWords,unordered_map <wstring ,int > &RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap,wstring fromWhere)
{ LFS
	wstring newObjectName;
	bool isObject = m[where].getObject() >= 0;
	if (numWords<0 && getRDFWhereString(where, newObjectName, L"_", -1) < 0) 
		return -1;
	if (numWords >= 1 && !isObject)
		phraseString(where, where + numWords, newObjectName, true);
	if (numWords >= 1 && isObject)
		phraseString(m[where].beginObjectPosition, m[where].beginObjectPosition + numWords, newObjectName, true);
	unordered_map<wstring, int >::iterator rdfni;
	if ((rdfni = extendedRdfTypeNumMap.find(newObjectName)) == extendedRdfTypeNumMap.end()) 
	{
		vector <cTreeCat *> rdfTypes;
		unordered_map <wstring ,int > topHierarchyClassIndexes;
		// this routine accumulates RDF types in a map per source called extendedRdfTypeMap
		getExtendedRDFTypesMaster(where, numWords, rdfTypes, topHierarchyClassIndexes, fromWhere, -1, true, false);
		// if there are no rdfTypes from the matched object, perhaps from the original object?
		if (rdfTypes.empty() && m[where].objectMatches.size()>0)
			// this routine accumulates RDF types in a map per source called extendedRdfTypeMap
			getExtendedRDFTypesMaster(where, -1, rdfTypes, topHierarchyClassIndexes, fromWhere, -1, true, true);
		//lplog(LOG_WHERE, L"%s results in %d rdfTypes.", newObjectName.c_str(),rdfTypes.size());
	}
	else
		(*rdfni).second++;
	RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap = extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
	//lplog(LOG_WHERE, L"%s results in %d words.", newObjectName.c_str(), extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.size());
	return 0;
}

void getOldRDFName(wstring &sourcePath, int where, int extendNumPP, wstring &object)
{
	wstring tmp1, tmp2;
	object = sourcePath + itos(where, tmp1) + itos(extendNumPP, tmp2);
	int of = object.find(L"dbPediaCache");
	if (of != wstring::npos)
		object.erase(0, of + wcslen(L"dbPediaCache") + 6);
	of = object.find(L"webSearchCache");
	if (of != wstring::npos)
		object.erase(0, of + wcslen(L"webSearchCache") + 6);
	if (object[0] == L'm' && object[1] == L'.' && object.length()>2)
		object.erase(0, 2);
}

void makePath(wstring &object, wchar_t *path)
{
	int pathlen = _snwprintf(path, MAX_LEN, L"%s\\dbPediaCache", CACHEDIR);// , retCode = -1;
	_wmkdir(path);
	_snwprintf(path + pathlen, MAX_LEN - pathlen, L"\\_%s", object.c_str());
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	if (wcslen(path) + 12>MAX_PATH)
		cOntology::compressPath(path);
	path[MAX_PATH - 12] = 0;
	path[pathlen + 2 + 244] = 0;// to guard against a filename that is too long
	wcscat(path, L".eRdfTypes");
}

// may reject 'his contributions' in the future
bool cSource::noRDFTypes()
{
	//int begin = m[where].beginObjectPosition,end=m[where].endObjectPosition;
	return false;
}

// get all RDF types associated with the object which is defined by the words consecutively from position (where) to position (where+numWords) in source.
// this may be extended by some prepositional phrases as determined by extendNumPP
// return results in rdfTypes and topHierarchyClassIndexes
int cSource::getExtendedRDFTypesMaster(int where, int numWords, vector <cTreeCat *> &rdfTypes, unordered_map <wstring, int > &topHierarchyClassIndexes, wstring fromWhere, int extendNumPP, bool fileCaching, bool ignoreMatches)
{
	LFS
	if (noRDFTypes())
		return 0;
	wstring tmp1, tmp2, object,newObjectName;
	if (numWords<0 && getRDFWhereString(where, newObjectName, L"_", extendNumPP, ignoreMatches) < 0)
		return -1;
	if (numWords >= 1)
		phraseString(m[where].beginObjectPosition, m[where].beginObjectPosition + numWords, newObjectName, true);
	// protects from buffer overrun and also very unlikely that legal names are > 200 in length
	if (newObjectName.length() > 200)
		return -1;
	// cache rdf types for an object.
	unordered_map<wstring, int >::iterator rdfni;
	if ((rdfni = extendedRdfTypeNumMap.find(newObjectName)) == extendedRdfTypeNumMap.end() || !fileCaching)
		extendedRdfTypeNumMap[newObjectName] = 1;
	else
	{
		(*rdfni).second++;
		rdfTypes = extendedRdfTypeMap[newObjectName].rdfTypes;
		topHierarchyClassIndexes = extendedRdfTypeMap[newObjectName].topHierarchyClassIndexes;
		return 0;
	}
	// An object may not have any rdf types, this is kept in a table for efficiency
	if (cOntology::inNoERDFTypesDBTable(newObjectName))
	{
		rdfTypes.clear();
		topHierarchyClassIndexes.clear();
		extendedRdfTypeMap[newObjectName].rdfTypes = rdfTypes;
		extendedRdfTypeMap[newObjectName].topHierarchyClassIndexes = topHierarchyClassIndexes;
		return 0;
	}
	// if it is not in the in memory cache, check if it is cached on disk.  Either by an old name or a new name.
	getOldRDFName(sourcePath, where, extendNumPP, object);
	wchar_t path[4096], newPath[4096];
	makePath(object, path);
	makePath(newObjectName, newPath);
	int retCode=-1,newRetCode=-1;
	if (!fileCaching || 
		  (_waccess(path, 0)<0 && _waccess(newPath, 0)<0) || 
			((retCode = readExtendedRDFTypes(path, rdfTypes, topHierarchyClassIndexes))<0 && (newRetCode = readExtendedRDFTypes(newPath, rdfTypes, topHierarchyClassIndexes))<0))
	{
		// not in memory cache, not cached on disk.  Get all rdf types.
		rdfTypes.clear();
		topHierarchyClassIndexes.clear();
		unordered_map <wstring, cOntologyEntry>::iterator dbSeparator=cOntology::dbPediaOntologyCategoryList.find(SEPARATOR);
		getExtendedRDFTypes(where, rdfTypes, topHierarchyClassIndexes, fromWhere, ignoreMatches, fileCaching);
		if (rdfTypes.empty() && numWords>0)
			cOntology::rdfIdentify(newObjectName.c_str(), rdfTypes, L"O");
		//lplog(LOG_WHERE, L"%s results in %d rdfTypes.", newObjectName.c_str(), rdfTypes.size()); 
		for (unordered_map <wstring, int >::iterator tI = topHierarchyClassIndexes.begin(), tIEnd = topHierarchyClassIndexes.end(); tI != tIEnd; tI++)
		{
			if (tI->second<0 || tI->second>=rdfTypes.size())
				lplog(LOG_WHERE,L"topHierarchyClassIndexes (size %d - rdfType size %d) %d,%s out of bounds!",topHierarchyClassIndexes.size(),rdfTypes.size(),tI->second,tI->first.c_str());
			else
				rdfTypes[tI->second]->top=tI->first;
		}
		// compress RDF types
		vector <cTreeCat *> urs;
		for (int r=0; r<rdfTypes.size(); r++)
		{
			if (rdfTypes[r]->cli==dbSeparator)
			{
				if (urs.size()>0 && urs[urs.size()-1]->cli!=dbSeparator)
					urs.push_back(rdfTypes[r]);
				continue;
			}
			bool found=false;
			for (int u=0; u<urs.size() && !found; u++)
				if (found=(urs[u]->equals(rdfTypes[r])))
				{
					if (rdfTypes[r]->confidence<urs[u]->confidence)
						urs[u]->confidence=rdfTypes[r]->confidence;
					if (rdfTypes[r]->wikipediaLinks.size()>urs[u]->wikipediaLinks.size())
						urs[u]->wikipediaLinks=rdfTypes[r]->wikipediaLinks;
					if (rdfTypes[r]->professionLinks.size()>urs[u]->professionLinks.size())
						urs[u]->professionLinks=rdfTypes[r]->professionLinks;
					if (rdfTypes[r]->preferred)
						urs[u]->preferred=rdfTypes[r]->preferred;
					if (!cOntology::cacheRdfTypes)
						delete rdfTypes[r];
				}
			if (!found)
				urs.push_back(rdfTypes[r]);
		}
		topHierarchyClassIndexes.clear();
		for (int r=0; r<urs.size(); r++)
			if (urs[r]->top.length())
				topHierarchyClassIndexes[urs[r]->top]=r;
	//	static int totalRdfs=0,uniqueRdfs=0;
	//	totalRdfs+=rdfTypes.size();
	//	uniqueRdfs+=(int)urs.size();
	//	lplog(LOG_INFO,L"getExtendedRDFTypesMaster RDFTypes compression %d %d %d %d",rdfTypes.size(),urs.size(),totalRdfs,uniqueRdfs);
		if (urs.empty())
			cOntology::insertNoERDFTypesDBTable(newObjectName);
		else if (writeExtendedRDFTypes(newPath,urs,topHierarchyClassIndexes)<0)
			return -1;
		newRetCode = 0;
		rdfTypes=urs;
	}
	extendedRdfTypeMap[newObjectName].rdfTypes = rdfTypes;
	extendedRdfTypeMap[newObjectName].topHierarchyClassIndexes = topHierarchyClassIndexes;
	getRDFTypeSimplificationToWordAssociationWithObjectMap(newObjectName, rdfTypes, extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap);
	//lplog(LOG_WHERE, L"getExtendedRDFTypesMaster %s derived words %d from rdfTypes %d.", newObjectName.c_str(), extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.size(), rdfTypes.size());
	if (retCode >= 0)
		lplog(LOG_WHERE, L"getExtendedRDFTypesMaster used path %s for object %s.", path, object.c_str());
//	if (newRetCode>=0)
//		lplog(LOG_WHERE, L"getExtendedRDFTypesMaster used [new] path %s for object %s.", newPath, newObjectName.c_str());
	if (!_waccess(path, 0))
	{
		if (_wremove(newPath)<0)
			lplog(LOG_ERROR, L"REMOVE %s - %S", newPath, sys_errlist[errno]);
		else if (_wrename(path, newPath))
			lplog(LOG_ERROR, L"RENAME %s to %s - %S", path, newPath, sys_errlist[errno]);
		else
			lplog(LOG_WHERE, L"getExtendedRDFTypesMaster moved path %s to [new] path %s.", path, newPath);
	}
	return retCode;
}

void cSource::getObjectString(int where,wstring &object,vector <wstring> &lookForSubject,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases)
{ LFS
	int o,begin,end;
	if (m[where].objectMatches.size()>0)
	{
		o=m[where].objectMatches[0].object;
		begin=m[objects[o].originalLocation].beginObjectPosition;
		end=m[objects[o].originalLocation].endObjectPosition;
	}
	else
	{
		o=m[where].getObject();
		begin=m[where].beginObjectPosition;
		end=m[where].endObjectPosition;
	}
	if (end-begin==1 && (objects[o].objectClass==NAME_OBJECT_CLASS || objects[o].objectClass==NON_GENDERED_NAME_OBJECT_CLASS) && objects[o].name.first!=wNULL && objects[o].name.last!=wNULL)
	{
		object=objects[o].name.first->first+L"_";
		object[0]=towupper(object[0]);
		int lastchpos=object.length();
		object+=objects[o].name.last->first;
		object[lastchpos]=towupper(object[lastchpos]);
		return;
	}
	for (int I=begin; I<end; I++)
	{
		int len=object.length();
		object+=m[I].word->first;
    if (m[I].flags&(cWordMatch::flagAddProperNoun|cWordMatch::flagOnlyConsiderProperNounForms|cWordMatch::flagFirstLetterCapitalized))
      object[len]=towupper(object[len]);
    if (m[I].flags&(cWordMatch::flagNounOwner))
    {
      object+='\'';
      if (object[object.length()-2]!='s') object+='s';
    }
    if (m[I].flags&cWordMatch::flagAllCaps)
      for (unsigned int J=len; object[J]; J++) object[J]=towupper(object[J]);
		if (I<end-1)
			object+=L"_";
		if (m[I].forms.isSet(PROPER_NOUN_FORM_NUM))
		{
			if (m[I].word->second.mainEntry!=wNULL)
				lookForSubject.push_back(m[I].word->second.mainEntry->first);
			else
				lookForSubject.push_back(m[I].word->first);
		}
	}
	if (includeNonMixedCaseDirectlyAttachedPrepositionalPhrases>0)
	{
		vector <wstring> prepPhraseStrings;
		wstring logres=object;
		int numWords;
		appendPrepositionalPhrases(where,object,prepPhraseStrings,numWords,true,L"_",includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
		if (prepPhraseStrings.size()>0)
			object=prepPhraseStrings[0];	
	}
}

// Curveball_$0028informant$0029$002FArchive1 
// Curveball_$0028informant$0029 
void convertFromWikilinkEscape(wstring &wikilink)
{ LFS
	wstring wl;
	for (unsigned int I=0; I<wikilink.length(); I++)
		if (wikilink[I]==L'$' && 
			  (iswdigit(wikilink[I+1]) || (wikilink[I+1]>='A' && wikilink[I+1]<='F')) && 
			  (iswdigit(wikilink[I+2]) || (wikilink[I+2]>='A' && wikilink[I+2]<='F')) && 
			  (iswdigit(wikilink[I+3]) || (wikilink[I+3]>='A' && wikilink[I+3]<='F')) && 
			  (iswdigit(wikilink[I+4]) || (wikilink[I+4]>='A' && wikilink[I+4]<='F')))
		{
			wchar_t save=wikilink[I+5];
			wikilink[I+5]=0;
			wchar_t ch=(wchar_t)wcstol(wikilink.c_str()+I+1,0,16);
			wikilink[I+5]=save;
			wl+=ch;
			I+=4;
		}
		else
			wl+=wikilink[I];
	wikilink=wl;
}

int cSource::getWikipediaPath(int principalWhere,vector <wstring> &wikipediaLinks,wchar_t *path,vector <wstring> &lookForSubject,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases)
{ LFS
	wstring object; 
	if (principalWhere>=0)
		getObjectString(principalWhere,object,lookForSubject,includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
	else if (wikipediaLinks.size()>0)
	{
		wstring wl=wikipediaLinks[0];
		convertFromWikilinkEscape(wl);
		if (wl.size()>0)
		{
			lplog(LOG_WHERE,L"%d:Substituted wikilink %s for object %s.",principalWhere,wl.c_str(),object.c_str());
			object=wl;
		}
	}
	if (object.empty())
		return -1;
	// remove ownership if the last word (Curveball's) should also check for adjectival object
	if (principalWhere>=0 && (m[principalWhere].flags&cWordMatch::flagAdjectivalObject) && object.length()>2 && object[object.length() - 1] == L's' && object[object.length() - 2] == L'\'')
			object.erase(object.length()-2);
	int pathlen=_snwprintf(path,MAX_LEN,L"%s\\wikipediaCache",CACHEDIR)+1;
	_wmkdir(path);
	_snwprintf(path,MAX_LEN,L"%s\\wikipediaCache\\_%s.txt",CACHEDIR,object.c_str());
	convertIllegalChars(path+pathlen);
	distributeToSubDirectories(path,pathlen,true);
	path[MAX_PATH-16]=0; // give room for extensions
	if (logTraceOpen)
		lplog(LOG_WHERE, L"TRACEOPEN %s %s", path, __FUNCTIONW__);
	if (_waccess(path, 0)<0)
	{
		wchar_t webAddress[1024];
		_snwprintf(webAddress,MAX_LEN,L"http://en.wikipedia.org/wiki/Special:Search?search=%s&printable=yes&redirect=no",object.c_str()); 
		lplog(LOG_WIKIPEDIA,L"PRIMARY:  %s",webAddress);
		int ret; 
		wstring buffer;
		if (ret= cInternet::readPage(webAddress,buffer)) return ret;
		reduceWikipediaPage(buffer);
		int fd=_wopen(path,O_CREAT|O_RDWR|O_BINARY,_S_IREAD | _S_IWRITE ); 
		if (fd<0)
		{
			lplog(LOG_ERROR,L"ERROR:Cannot create path %s - %S (6).",path,sys_errlist[errno]);
			return cInternet::GETPAGE_CANNOT_CREATE;
		}
		_write(fd,buffer.c_str(),buffer.length()*sizeof(buffer[0]));
		_close(fd);
	}
	return 0;
}

// if subject object matches, and verb is IS, evaluate object for an OCType.
int cSource::evaluateISARelation(int parentSourceWhere,int where,vector <cTagLocation> &tagSet,vector <wstring> &lookForSubject)
{ LFS
	int nextTag=-1,verbTagIndex;
	if ((verbTagIndex=findOneTag(tagSet,L"VERB",-1))<0) return -1;
	// I am not your enemy.
	// I will never be your enemy.
	// I am no enemy of yours.
	bool isId=false,mainEntryMatch=true;
	getVerbTense(tagSet,verbTagIndex,isId); // int tsSense=
	int whereVerb=tagSet[verbTagIndex].sourcePosition;
	if (m[whereVerb].getMainEntry()->first==L"denote")
		isId=true;
	if (!isId || 
	    findTagConstrained(tagSet,L"not",nextTag,tagSet[verbTagIndex])>=0 || findTagConstrained(tagSet,L"never",nextTag,tagSet[verbTagIndex])>=0)
		return -1;
	nextTag=-1;
	tIWMM subjectWord=wNULL,objectWord=wNULL;
	int subjectTag=findTag(tagSet,L"SUBJECT",nextTag),subjectObject,whereSubjectObject;
	if (!resolveTag(tagSet,subjectTag,subjectObject,whereSubjectObject,subjectWord) || subjectObject<0) return -1;
	// match original subject against found subject
	int elementsFound = 0;// , ol = objects[subjectObject].originalLocation;

	for (int I=m[whereSubjectObject].beginObjectPosition; I<m[whereSubjectObject].endObjectPosition; I++)
	{
		wstring w=(m[I].word->second.mainEntry!=wNULL) ? m[I].word->second.mainEntry->first : m[I].word->first;
		if (find(lookForSubject.begin(),lookForSubject.end(),w)!=lookForSubject.end() && !(m[I].flags&cWordMatch::flagAdjectivalObject))
			elementsFound++;
		else if (I>=whereSubjectObject)
			mainEntryMatch=false;
	}
	if (elementsFound!=lookForSubject.size()) return -1;
	wstring tmpstr,tmpstr2;
	if (!mainEntryMatch)
	{
		lplog(LOG_WIKIPEDIA,L"%06d:%d:Rejected subject %s because of non matching main word@>=%d.",parentSourceWhere,
			where,objectString(subjectObject,tmpstr,true).c_str(),whereSubjectObject);
		return -1;
	}
	// now identify object
	nextTag=-1;
	int objectTag=findTag(tagSet,L"OBJECT",nextTag),objectObject,whereObjectObject;
	if (objectTag<0 || nextTag>=0) return -1;
	if (!resolveTag(tagSet,objectTag,objectObject,whereObjectObject,objectWord) || objectObject<0) return -1;
	lplog(LOG_WIKIPEDIA,L"%06d:%d:Found subject %s having ISA object %s [subType %s].",parentSourceWhere,
		where,objectString(subjectObject,tmpstr,true).c_str(),objectString(objectObject,tmpstr2,true).c_str(),
		(objects[objectObject].getSubType()>=0) ? OCSubTypeStrings[objects[objectObject].getSubType()] : L"N/A");
	// Llanelly (Welsh) is the name of both a village and its respective parish
	if (m[whereObjectObject].word->first==L"name" && m[whereObjectObject+1].word->first==L"of" && m[whereObjectObject+1].getRelObject()>=0)
	{
		int element=-1;
		if ((element=m[whereObjectObject].pma.queryTag(MNOUN_TAG))>=0)
		{
			int whereCompoundObject=-1;
			for (whereCompoundObject=whereObjectObject; whereCompoundObject<m[whereObjectObject].pma[element].len && m[whereCompoundObject].nextCompoundPartObject<0; whereCompoundObject++);
			while (whereCompoundObject>=0)
			{
				objectObject=m[whereCompoundObject].getObject();
				lplog(LOG_WIKIPEDIA,L"%06d:%d:Found subject %s having ISA multi object %s [subType %s].",parentSourceWhere,
					where,objectString(subjectObject,tmpstr,true).c_str(),objectString(objectObject,tmpstr2,true).c_str(),
					(objects[objectObject].getSubType()>=0) ? OCSubTypeStrings[objects[objectObject].getSubType()] : L"N/A");
				if (objects[objectObject].getSubType()<0 && !objects[objectObject].neuter && (objects[objectObject].male || objects[objectObject].female))
					return NOT_A_PLACE;
				if (objects[objectObject].getSubType()>=0)
					return objects[objectObject].getSubType();
				whereCompoundObject=m[whereCompoundObject].nextCompoundPartObject;
			}
		}
		else
		{
			objectObject=m[m[whereObjectObject+1].getRelObject()].getObject();
			if (objectObject<0) return -1;
			lplog(LOG_WIKIPEDIA,L"%06d:%d:Found subject %s having ISA named object %s [subType %s].",parentSourceWhere,
				where,objectString(subjectObject,tmpstr,true).c_str(),objectString(objectObject,tmpstr2,true).c_str(),
				(objects[objectObject].getSubType()>=0) ? OCSubTypeStrings[objects[objectObject].getSubType()] : L"N/A");
		}
	}
	if (objects[objectObject].getSubType()<0 && !objects[objectObject].neuter && (objects[objectObject].male || objects[objectObject].female))
		return NOT_A_PLACE;
	return (objects[objectObject].getSubType()<0) ? NUM_SUBTYPES : objects[objectObject].getSubType();
}

bool cSource::getISARelations(int parentSourceWhere,int where,vector < vector <cTagLocation> > &tagSets,vector <int> &OCTypes,vector <wstring> &lookForSubject)
{ LFS
  vector <cWordMatch>::iterator im=m.begin()+where;
	cPatternMatchArray::tPatternMatch *pma=im->pma.content;
	for (unsigned PMAElement=0; PMAElement<im->pma.count; PMAElement++,pma++)
	{
		//  desiredTagSets.push_back(cTagSet(subjectVerbRelationTagSet,"_SUBJECT_VERB_RELATION",3,"VERB","V_OBJECT","SUBJECT","OBJECT","PREP","IVERB","REL","ADJ","ADV","HOBJECT","V_AGREE","V_HOBJECT",NULL));
		if ((((patterns[pma->getPattern()]->includesOnlyDescendantsAllOfTagSet)&((__int64)1<<subjectVerbRelationTagSet))!=0))
		{
			tagSets.clear();
			if (startCollectTags(debugTrace.traceRelations,subjectVerbRelationTagSet,where,pma->pemaByPatternEnd,tagSets,true,false,L"get ISA relation")>0)
			{
				for (unsigned int J=0; J<tagSets.size(); J++)
				{
					if (debugTrace.traceRelations)
						printTagSet(LOG_RESOLUTION,L"ART",J,tagSets[J],where,pma->pemaByPatternEnd);
					int OCType=evaluateISARelation(parentSourceWhere,where,tagSets[J],lookForSubject);
					if (OCType>=0 || OCType==NOT_A_PLACE)
						OCTypes.push_back(OCType);
				}
			}
		}
	}
	return OCTypes.size()>0;
}

bool readAttribs(wstring path,vector <int> &OCTypes)
{ LFS
	char buffer[MAX_BUF];
	wstring attribsPath=wstring(path)+L".attribs";
	int where=0,len;
	if (getPath(attribsPath.c_str(),buffer,MAX_BUF,len)!=0) return false;
	if (!copy(OCTypes,buffer,where,len)) return false;
	return true;
}

bool writeAttribs(wstring path,vector <int> &OCTypes)
{ LFS
	char buffer[MAX_BUF];
	int len=0,fd;
	if (!copy(buffer,OCTypes,len,MAX_BUF)) return false;
	wstring attribsPath=wstring(path)+L".attribs";
	if ((fd=_wopen(attribsPath.c_str(),O_CREAT|O_RDWR|O_BINARY,_S_IREAD | _S_IWRITE ))<0)
	{
		lplog(LOG_ERROR,L"ERROR:Cannot create path %s - %S (7).",attribsPath.c_str(),sys_errlist[errno]);
		return false;
	}
	_write(fd,buffer,len);
	_close(fd);
	return true;
}

bool cSource::mixedCaseObject(int begin,int len)
{ LFS
  bool lowerCaseFound=false,allUpperCaseWithVowelsFound=false;
  for (int I=begin; I<begin+len; I++)
	{
		if (iswalpha(m[I].word->first[0]))
		{
			lowerCaseFound|=!(m[I].flags&(cWordMatch::flagAllCaps|cWordMatch::flagFirstLetterCapitalized));
			if (m[I].flags&(cWordMatch::flagAllCaps))
			{
				for (unsigned int w=0; w<m[I].word->first.length() && !allUpperCaseWithVowelsFound; w++)
				{
					wchar_t wc=towlower(m[I].word->first[w]);
					if (wc==L'a' || wc==L'e' || wc==L'i' || wc==L'o' || wc==L'u')
						allUpperCaseWithVowelsFound=true;
				}
			}
		}
	}
	if (lowerCaseFound && allUpperCaseWithVowelsFound)
	{
		wstring tmpstr;
	  for (int I=begin; I<begin+len; I++)
			phraseString(begin,begin+len,tmpstr,true,L" ");
	}
	return lowerCaseFound && allUpperCaseWithVowelsFound;
}

// return true if object SHOULD NOT be identified through extensive web methods
bool cSource::capitalizationCheck(int begin,int len)
{
	// count # of capitalized words
	int wordsCapitalized = 0;
	for (int I = begin; I < begin + len; I++)
	{
		if (m[I].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m[I].flags&cWordMatch::flagFirstLetterCapitalized) || (m[I].flags&cWordMatch::flagAllCaps))
			wordsCapitalized++;
	}
	bool firstWordCapitalized = (m[begin].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m[begin].flags&cWordMatch::flagFirstLetterCapitalized) || (m[begin].flags&cWordMatch::flagAllCaps));
	if ((len == 1 && !firstWordCapitalized) ||
		// only word capitalized is the determiner
		(len == 2 && wordsCapitalized == 1 && firstWordCapitalized && m[begin].queryForm(determinerForm) >= 0) ||
		// more than half the words are not capitalized
		(len > 2 && wordsCapitalized < len / 2))
	{
		wstring tmpstr;
		if (logQuestionDetail)
			lplog(LOG_ERROR, L"capitalizationCheck rejected %s", phraseString(begin, begin + len, tmpstr, true).c_str());
		return true;
	}
	return false;
}

bool cSource::rejectISARelation(int principalWhere)
{
	LFS
	int o=m[principalWhere].getObject(),begin=m[principalWhere].beginObjectPosition,len=m[principalWhere].endObjectPosition-begin;
	bool tmp1=false,tmp2=false,tmp3=false,tmp4=false,tmp5=false,tmp6=false,tmp7=false,tmp8=false,tmp9=false,tmp10=false,tmp11=false; // for use with debugging
	return ((tmp1=o<0) || //  || (objects[o].getSubType()>=0 && objects[o].getSubType()!=UNKNOWN_PLACE_SUBTYPE)
			// only names
		  (tmp2=(objects[o].objectClass!=NAME_OBJECT_CLASS && objects[o].objectClass!=NON_GENDERED_NAME_OBJECT_CLASS)) ||
			// no capitalization other than the first word (and more than 1 word in length) OR mostly not capitalized
			(tmp11 = capitalizationCheck(begin,len)) ||
			// no matches, look up ISA once per object, and no honorific (sir, Lieutenant, etc, or Miss Tuppence)
			(tmp3=m[principalWhere].objectMatches.size() || objects[o].wikipediaAccessed || objects[o].name.hon!=wNULL) ||
			// no speakers, no relativizers (because they get the object they represent set on them)
			(tmp4=objects[o].numIdentifiedAsSpeaker>0 || objects[o].PISHail>0 || objects[o].PISDefinite>0 || m[principalWhere].queryWinnerForm(relativizerForm)>=0) ||
			// Tuesday, March
			(tmp5=(len==1 && (m[principalWhere].word->second.timeFlags&T_UNIT))) ||
			// STORIES_Tina_Fey_Announces / PLAYLISTBYAOL_On_Valentine's day
			(tmp8=mixedCaseObject(begin,len)) ||
			// his_new_memoir / her_hit_Torn / your_Horoscope -- may alternatively cut the possessives off
			(tmp9=m[begin].queryForm(possessiveDeterminerForm)>=0 && !(m[begin].flags&(cWordMatch::flagAllCaps|cWordMatch::flagFirstLetterCapitalized))) ||
			// Subway_Hero_Describes_Saving_Man_From_Tracks_:
			(tmp10=m[begin+len-1].word->first==L":") ||
			// last Friday
			// a Friday
			(tmp6=(len==2 && (m[principalWhere].word->second.timeFlags&T_UNIT) && 
			 (m[begin].queryWinnerForm(determinerForm)>=0 || m[begin].queryWinnerForm(numeralOrdinalForm)>=0 || m[begin].queryWinnerForm(adjectiveForm)>=0))) ||
			// A, C
			(tmp7=(len==1 && (m[principalWhere].queryForm(letterForm)>=0)))); // for fictional books only!
		/*
		wstring tmpstr;
		// Tuesday, March
		if ((objectWordLength==1 && (m[principalWhere].word->second.timeFlags&T_UNIT)) ||
				// last Friday
				// a Friday
				(objectWordLength==2 && (m[principalWhere].word->second.timeFlags&T_UNIT) && 
				 (m[begin].queryWinnerForm(determinerForm)>=0 || m[begin].queryWinnerForm(numeralOrdinalForm)>=0 || m[begin].queryWinnerForm(adjectiveForm)>=0)))
			lplog(LOG_RESOLUTION,L"%06d:TR new time %s",principalWhere,objectString(o,tmpstr,false).c_str());
		*/
}

bool cQuestionAnswering::rejectPath(const wchar_t *path)
{
	struct _stat64 buf;
	_wstati64(path, &buf);
	return (buf.st_size < 10 || wcsstr(path, L".pdf") != NULL || wcsstr(path, L".php") != NULL); // Nobel Prize abstract is 2 bytes / also don't bother with pdf or php files for now
}

int limitProcessingForProfiling=0;
int cQuestionAnswering::processPath(cSource *parentSource,const wchar_t *path, cSource *&source, cSource::sourceTypeEnum st, int pathSourceConfidence, bool parseOnly)
{
	LFS
		if (logTraceOpen)
			lplog(LOG_WHERE, L"TRACEOPEN %s %s", path, __FUNCTIONW__);
	unordered_map <wstring, cSource *>::iterator smi = sourcesMap.find(path);
	wchar_t sourcesParsedTitle[1024];
	GetConsoleTitle(sourcesParsedTitle, 1024);
	wchar_t *ch = wcsstr(sourcesParsedTitle, L"...");
	extern int questionProgress;
	if (smi != sourcesMap.end())
	{
		source = smi->second;
		source->numSearchedInMemory++;
		if (ch)
		{
			wsprintf(ch + 3, L"[%d%%] %d sources processed:%s[%d]", questionProgress, sourcesMap.size(), path, source->numSearchedInMemory);
			SetConsoleTitle(sourcesParsedTitle);
		}
		return 0;
	}
	if (rejectPath(path))
		return -1;
	if (ch)
	{
		wsprintf(ch + 3, L"[%d%%]  %d sources processed:%s", questionProgress, sourcesMap.size(), path);
		SetConsoleTitle(sourcesParsedTitle);
	}
	source = new cSource(&parentSource->mysql, st, pathSourceConfidence);
	source->numSearchedInMemory = 1;
	source->isFormsProcessed = false;
	source->processOrder = ++parentSource->processOrder;
	source->multiWordStrings = parentSource->multiWordStrings;
	source->multiWordObjects = parentSource->multiWordObjects;
	wstring wpath = path, start = L"~~BEGIN";
	int repeatStart = 1;
	bool justParsed = false;
	Words.readWords(wpath, -1, false, L"");
	if (!source->readSource(wpath, false, justParsed, false, L"") || (justParsed && !parseOnly))
	{
		lplog(LOG_WIKIPEDIA | LOG_RESOLUTION | LOG_RESCHECK | LOG_WHERE, L"Begin Processing %s...", path);
		if (!justParsed)
		{
			wprintf(L"\nParsing %s...\n", path);
			unsigned int unknownCount = 0, quotationExceptions = 0, totalQuotations = 0;
			int globalOverMatchedPositionsTotal = 0;
			string cPath;
			wTM(path, cPath);
			source->tokenize(L"", L"", wpath, L"", start, repeatStart, unknownCount);
			source->doQuotesOwnershipAndContractions(totalQuotations);
			unlockTables(parentSource->mysql);
			if (source->m.empty())
			{
				wstring failurePath = path;
				failurePath += L".SourceCache";
				int fd;
				_wsopen_s(&fd, failurePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE); // , errorCode = 
				if (fd >= 0)
					close(fd);
				return -1;
			}
			bool s1 = logMatchedSentences, s2 = logUnmatchedSentences;
			logMatchedSentences = logUnmatchedSentences = true;
			source->printSentences(false, unknownCount, quotationExceptions, totalQuotations, globalOverMatchedPositionsTotal);
			logMatchedSentences = s1;
			logUnmatchedSentences = s2;
			lplog();
			source->write(path, false, false, L"");
			source->writeWords(path, L"");
			limitProcessingForProfiling = 0;
			puts("");
		}
		else
		{
			source->printSentencesCheck(true);
			lplog();
		}
		if (source->m.empty())
		{
			wstring failurePath = path;
			failurePath += L".SourceCache";
			int fd = _wopen(failurePath.c_str(), O_CREAT);
			if (fd >= 0)
				close(fd);
			return -1;
		}
		if (!parseOnly)
		{
			source->parentSource = parentSource;
			source->identifyObjects();
			vector <int> secondaryQuotesResolutions;
			source->analyzeWordSenses();
			source->narrativeIsQuoted = true;
			source->syntacticRelations();
			//Words.writeWords(path);  not necessary and wastes huge amount of space
			source->identifySpeakerGroups();
			source->resolveSpeakers(secondaryQuotesResolutions);
			source->resolveFirstSecondPersonPronouns(secondaryQuotesResolutions);
		}
		source->write(path, !parseOnly, false, L"");
		source->writeWords(path, L"");
		lplog(LOG_WIKIPEDIA | LOG_RESOLUTION | LOG_RESCHECK | LOG_WHERE, L"End Processing %s...", path);
	}
	sourcesMap[path] = source;
	return 0;
}

int cSource::identifyISARelationTextAnalysis(cQuestionAnswering &qa,int principalWhere, bool parseOnly)
{
	LFS
		// must be a name
		int o = m[principalWhere].getObject();// , begin = m[principalWhere].beginObjectPosition;// , objectWordLength = m[principalWhere].endObjectPosition - begin;
	if (rejectISARelation(principalWhere)) return -1;
	objects[o].wikipediaAccessed=true;
  wchar_t path[1024];
	vector <wstring> lookForSubject,wikipediaLinks;
	getWikipediaPath(principalWhere,wikipediaLinks,path,lookForSubject,0);
	vector <int> OCTypes;
	if (!readAttribs(path,OCTypes))
	{
		cSource *source = NULL;
		qa.processPath(this,path,source,cSource::WIKIPEDIA_SOURCE_TYPE,2,parseOnly);
		vector <cWordMatch>::iterator im=source->m.begin(),imEnd=source->m.end();
		vector < vector <cTagLocation> > tagSets;
		for (int I=0; im!=imEnd; im++,I++)
			source->getISARelations(principalWhere,I,tagSets,OCTypes,lookForSubject);
		writeAttribs(path,OCTypes);
	}
	else
		lplog(LOG_WIKIPEDIA,L"%06d:Read attribs for %s...",principalWhere,path);
	wstring tmpstr;
	bool disallowNotAPlace=false,gendered=false;
	for (unsigned int I=0; I<OCTypes.size() && !disallowNotAPlace; I++)
	{
		disallowNotAPlace=(OCTypes[I]!=NUM_SUBTYPES && OCTypes[I]!=NOT_A_PLACE);
		gendered|=(OCTypes[I]==NOT_A_PLACE);
	}
	if (!disallowNotAPlace && gendered)
	{
			objects[o].isNotAPlace=true;
			objects[o].resetSubType();
			lplog(LOG_WIKIPEDIA,L"%06d:%s reassigned (wikipedia) to not a place [gendered].",principalWhere,objectString(o,tmpstr,false).c_str());
			return 0;
	}
	for (unsigned int I=0; I<OCTypes.size(); I++)
		if (OCTypes[I]>=0 && objects[o].getSubType()!=OCTypes[I] && !objects[o].isNotAPlace &&
			  // these are tightly limited 
			  objects[o].getSubType()!=COUNTRY && objects[o].getSubType()!=OCEAN_SEA &&	objects[o].getSubType()!=US_STATE_TERRITORY_REGION)
		{
			if (OCTypes[I]==NUM_SUBTYPES)
			{
				if (disallowNotAPlace) continue;
				objects[o].isNotAPlace=true;
				objects[o].resetSubType();
				lplog(LOG_WIKIPEDIA,L"%06d:%s reassigned (wikipedia) to not a place.",principalWhere,objectString(o,tmpstr,false).c_str());
			}
			else if (!(objects[o].male ^ objects[o].female) || OCTypes[I]!=UNKNOWN_PLACE_SUBTYPE)
			{
				objects[o].setSubType(OCTypes[I]);
				lplog(LOG_WIKIPEDIA,L"%06d:%s reassigned (wikipedia) to the object subtype %s.",principalWhere,
				    objectString(o,tmpstr,false).c_str(),OCSubTypeStrings[OCTypes[I]]);
			}
		}
	return 0;
}

int cSource::getRDFWhereString(int where, wstring &oStr, wchar_t *separator, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool ignoreMatches)
{ LFS
	int o,begin,len;
	if (m[where].objectMatches.size()>0 && !ignoreMatches)
	{
		o=m[where].objectMatches[0].object;
		begin=m[objects[o].originalLocation].beginObjectPosition;
		len=m[objects[o].originalLocation].endObjectPosition-begin;
	}
	else
	{
		o=m[where].getObject();
		begin=m[where].beginObjectPosition;
		len=m[where].endObjectPosition-begin;
	}
	if (o < 0)
		return -1;
	if (len==1 && (objects[o].objectClass==NAME_OBJECT_CLASS || objects[o].objectClass==NON_GENDERED_NAME_OBJECT_CLASS) && objects[o].name.first!=wNULL && objects[o].name.last!=wNULL)
	{
		oStr=objects[o].name.first->first;
		oStr[0]=towupper(oStr[0]);
		oStr+=separator;
		int lastchpos=oStr.length();
		oStr+=objects[o].name.last->first;
		oStr[lastchpos]=towupper(oStr[lastchpos]);
	}
	else
	{
		getOriginalWord(begin, oStr, false);
		if (oStr==L"the")
			oStr=L"The"; // The New York Times
		for (int I=begin+1; I<begin+len; I++)
		{
			oStr+=separator;
			getOriginalWord(I, oStr, true);
		}
	}
	//lplog(LOG_WHERE, L"getRDFWhereString yields %s before includeNonMixedCaseDirectlyAttachedPrepositionalPhrases=%d", oStr.c_str(), includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
	if (includeNonMixedCaseDirectlyAttachedPrepositionalPhrases>0)
	{
		vector <wstring> prepPhraseStrings;
		wstring logres=oStr;
		int numWords;
		appendPrepositionalPhrases(where,logres,prepPhraseStrings,numWords,true,separator,includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
		if (prepPhraseStrings.empty())
			return -1;
		oStr=prepPhraseStrings[0];
	}
	//lplog(LOG_WHERE, L"getRDFWhereString yields %s after includeNonMixedCaseDirectlyAttachedPrepositionalPhrases=%d", oStr.c_str(), includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
	// remove ownership (Curveball's)
	if (oStr.length() > 2 && oStr[oStr.length() - 1] == L's' && oStr[oStr.length() - 2] == L'\'')
		oStr.erase(oStr.length() - 2);
	return 0;
}

bool cSource::analyzeRDFTitle(unsigned int where, int &numWords, int &numPrepositions, wstring tableName)
{
	LFS
		numWords = 0;
	// check if from begin to end there is only capitalized words except for determiners or prepositions - 
	// What Do We Need to Know About the International Monetary System? (Paul Krugman)
	// be sensitive to breaks, like / Fundación De Asturias [7047][name][N][F:Fundación M1:De L:Asturias ][region] ( Spain ) , Prince of Asturias Awards in Social Sciences[7051-7062].
	bool allCapitalized = true;
	numPrepositions = 0;
	int lastComma = -1, lastConjunction = -1, firstComma = -1;
	for (unsigned int I = where; I < m.size() && m[I].word != Words.END_COLUMN && m[I].word != Words.TABLE && allCapitalized && !isEOS(I) && m[I].queryForm(bracketForm) < 0 &&
		(m[I].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m[I].flags&cWordMatch::flagFirstLetterCapitalized) || (m[I].flags&cWordMatch::flagAllCaps) ||
			m[I].word->first[0] == L',' || m[I].word->first[0] == L':' || m[I].word->first[0] == L';' || m[I].queryForm(determinerForm) >= 0 || m[I].queryForm(numeralOrdinalForm) >= 0 || m[I].queryForm(prepositionForm) >= 0 || m[I].queryForm(coordinatorForm) >= 0);
		I++, numWords++)
	{
		if (m[I].queryForm(prepositionForm) >= 0)
			numPrepositions++;
		if (m[I].getObject() > 0)
		{
			numWords += (m[I].endObjectPosition - 1 - I);
			I = m[I].endObjectPosition - 1; // skip objects and periods associated with abbreviations and names
		}
		if (m[I].word->first[0] == L',')
		{
			lastComma = I;
			if (firstComma<0 || lastConjunction>firstComma)
				firstComma = I;
		}
		if (m[I].queryForm(conjunctionForm) >= 0)
			lastConjunction = I;
	}
	if (lastComma >= 0 && lastConjunction != lastComma + 1 && lastConjunction != lastComma + 2)
	{
		if (logQuestionDetail)
		{
			wstring tmpstr, tmpstr2;
			lplog(LOG_WHERE, L"Processing table %s: %d:title %s rejected words after comma resulting in %s", tableName.c_str(), where, phraseString(where, where + numWords, tmpstr, true, L" ").c_str(), phraseString(where, firstComma, tmpstr2, true, L" ").c_str());
		}
		numWords = firstComma - where;
	}
	while (numWords > 1 && (m[where + numWords - 1].word->first[0] == L',' || m[where + numWords - 1].queryForm(determinerForm) >= 0 || m[where + numWords - 1].queryForm(prepositionForm) >= 0 || m[where + numWords - 1].queryForm(coordinatorForm) >= 0))
		numWords--;
	if (m[where].endObjectPosition > (int)where + numWords)
		numWords = m[where].endObjectPosition - where;
	if (numWords == 0)
	{
		numWords = 1;
		return false;
	}
	return true;
}

// rdfTypes calls rdfIdentify, which then caches the rdfTypes of each individual string into the dbpedia cache.
int cSource::getRDFTypes(int where, vector <cTreeCat *> &rdfTypes, wstring fromWhere, int extendNumPP, bool ignoreMatches, bool fileCaching)
{ LFS
	int o,begin,objectWordLength,pw;
	if (m[where].objectMatches.size()>0 && !ignoreMatches)
	{
		o=m[where].objectMatches[0].object;
		begin=m[pw=objects[o].originalLocation].beginObjectPosition;
		objectWordLength=m[pw].endObjectPosition-begin;
	}
	else
	{
		o=m[pw=where].getObject();
		begin=m[where].beginObjectPosition;
		objectWordLength=m[where].endObjectPosition-begin;
	}
	int oLen=objects[o].len();
	// if where=Krugman matches o=Paul Krugman, take the longer match
	if (oLen>objectWordLength)
	{
		pw=objects[o].originalLocation;
		objectWordLength=oLen;
		begin=objects[o].begin;
	}
	if (begin<0)
	{
		lplog(LOG_WHERE|LOG_ERROR,L"%06d:%S getRDFTypes handed an object with no begin!",where,fromWhere.c_str());
		return -1;
	}
	wstring oStr;
	int numWords,numPrepositions=0;
	getRDFWhereString(where,oStr,L"_",extendNumPP,ignoreMatches);
	if (oStr.length()>512)
	{
		lplog(LOG_WHERE | LOG_ERROR, L"%06d:%S getRDFTypes handed a very long object (%s)!", where, fromWhere.c_str(),oStr.c_str());
		return -1;
	}
	wstring saveStr=oStr;
	// specifically if ignoreMatches is true, we must obey the number of prepositions allowed, which will not be true with analyzeTitle.
	// even though this should never be necessary because if ignoreMatches is true, then objectMatches will not be empty.
	if (m[where].objectMatches.empty() && !ignoreMatches)
	{
		analyzeRDFTitle(begin,numWords,numPrepositions,L"");
		if (m[where].endObjectPosition - m[where].beginObjectPosition > numWords)
			numWords = m[where].endObjectPosition - m[where].beginObjectPosition;
		wstring tmpstr;
		phraseString(begin,begin+numWords,tmpstr,true,L"_");
		// remove ownership (Curveball's)
		if (tmpstr.length()>2 && tmpstr[tmpstr.length()-1]==L's' && tmpstr[tmpstr.length()-2]==L'\'')
			tmpstr.erase(tmpstr.length()-2);
		if (tmpstr.size()>oStr.length())
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE,L"%d:replaced %s with %s",where,oStr.c_str(),tmpstr.c_str());
			oStr=tmpstr;
		}
	}
	cOntology::rdfIdentify(oStr, rdfTypes, L"1", fileCaching);
	if (numPrepositions>extendNumPP && rdfTypes.empty())
	{
		cOntology::rdfIdentify(saveStr.c_str(), rdfTypes, L"a", fileCaching);
		 if (!rdfTypes.empty())
			 oStr=saveStr;
	}
	// a Nobel prize
	if (rdfTypes.empty() && (!wcsncmp(oStr.c_str(),L"a_",2) || !wcsncmp(oStr.c_str(),L"the_",4)))
	{
		oStr=L"The"; // The Nobel Prize
		for (int I=begin+1; I<begin+objectWordLength; I++)
		{
			oStr+=L"_";
			int len=oStr.length();
			getOriginalWord(I, oStr, true);
			oStr[len]=towupper(oStr[len]);
		}
		cOntology::rdfIdentify(oStr, rdfTypes, L"2", fileCaching);
	}
	// The Humanist Antonio+de+Nebrija
	if (rdfTypes.empty() && !wcsncmp(oStr.c_str(),L"The_",4))
	{
		if (m[begin+1].queryForm(commonProfessionForm)>=0 && objectWordLength>3)
		{
			getOriginalWord(begin + 2, oStr, false);
			for (int I=begin+3; I<begin+objectWordLength; I++)
			{
				oStr += L"_";
				int len=oStr.length();
				getOriginalWord(I, oStr, true);
				oStr[len]=towupper(oStr[len]);
			}
			cOntology::rdfIdentify(oStr, rdfTypes, L"3", fileCaching);
		}
	}
	int colon;
	if (rdfTypes.empty() && (colon=oStr.find(L':'))!=wstring::npos)
	{
		if (colon && oStr[colon-1]==L'_')
			oStr.erase(colon-1);
		cOntology::rdfIdentify(oStr, rdfTypes, L"4", fileCaching);
	}
	int lowestConfidence=CONFIDENCE_NOMATCH;
	for (unsigned int I=0; I<rdfTypes.size(); I++)
		if (rdfTypes[I]->cli->first!=SEPARATOR)
			lowestConfidence=min(lowestConfidence,rdfTypes[I]->confidence);
	if (lowestConfidence>1 && (!wcsncmp(oStr.c_str(), L"The_", 4) || !wcsncmp(oStr.c_str(), L"the_", 4)))
	{
		oStr.erase(0, 4);
		cOntology::rdfIdentify(oStr, rdfTypes, L"5", fileCaching);
	}
	// 3 M
	if (rdfTypes.empty() && objectWordLength==2 && m[begin].queryWinnerForm(NUMBER_FORM_NUM)>=0 && m[begin+1].word->first.length()==1)
	{
		getOriginalWord(begin,oStr,false);
		getOriginalWord(begin + 1, oStr, true);
		cOntology::rdfIdentify(oStr, rdfTypes, L"6", fileCaching);
	}
	// Rihanna [6][name][M][F][A:Rihanna ] record compactLabel [PO (Rihanna's record compactLabel Def Jam[6-11]{OWNER:Rihanna }[9][ngname][N][OGEN]) def].
	// strip out begin adjectival object
	// noun that is left must have at least two words
	for (int I=begin; I<begin+objectWordLength-2 && rdfTypes.empty(); I++)
		if ((m[I].flags&cWordMatch::flagAdjectivalObject) && m[I].endObjectPosition<=begin+objectWordLength-2)
		{
			wstring tmpstr;
			if (m[I].endObjectPosition>=0)
				phraseString(m[I].endObjectPosition, begin + objectWordLength, tmpstr, true, L"_");
			else
				phraseString(I+1, begin + objectWordLength, tmpstr, true, L"_");
			cOntology::rdfIdentify(tmpstr, rdfTypes, L"7", fileCaching);
		}
		else if (!(m[I].flags&cWordMatch::flagFirstLetterCapitalized) && (m[I+1].flags&cWordMatch::flagFirstLetterCapitalized) && I<begin+objectWordLength+2)
		{
			wstring tmpstr;
			phraseString(I+1,begin+objectWordLength,tmpstr,true,L"_");
			cOntology::rdfIdentify(tmpstr, rdfTypes, L"8", fileCaching);
		}
	return 0;
}

int cSource::identifyISARelation(int principalWhere,bool initialTenseOnly)
{ LFS
	cOntology::initialize();
	int o=m[principalWhere].getObject(),begin=m[principalWhere].beginObjectPosition,len=m[principalWhere].endObjectPosition-begin;
	// save time calling getExtendedRDFTypesMaster - we are pretty sure this is a person
	if (o>=0 && ((objects[o].name.hon!=wNULL && !objects[o].name.justHonorific()) || objects[o].numIdentifiedAsSpeaker>0 || objects[o].PISHail>0 || objects[o].PISDefinite>0))
	{
		objects[o].isNotAPlace=true;
		objects[o].resetSubType();
		objects[o].isWikiPerson=true;
		return 0;
	}
	// must be a name
	if (rejectISARelation(principalWhere) || o < 0)
	{
		if (o >= 0 && logQuestionDetail)
		{
			wstring tmpstr;
			lplog(LOG_INFO, L"ISARelation declined:%s", objectString(o, tmpstr, false).c_str());
		}
		return -1;
	}
	if (objects[o].dbPediaAccessed) return 0;
	objects[o].dbPediaAccessed=true;
	vector <cTreeCat *> rdfTypes;
	unordered_map <wstring ,int > topHierarchyClassIndexes;
	getExtendedRDFTypesMaster(principalWhere,-1,rdfTypes,topHierarchyClassIndexes,TEXT(__FUNCTION__));
	if (len==1 && !(m[principalWhere].flags&cWordMatch::flagFirstLetterCapitalized)) // lower case 'prize'
	{
		for (unsigned int r=0; r<rdfTypes.size(); r++)
			rdfTypes[r]->top.clear();
		//remove person or business or organization because these are always upper case.  This avoids 'prize' being associated with a person.
		for (unordered_map <wstring ,int >::iterator idi=topHierarchyClassIndexes.begin(),idiEnd=topHierarchyClassIndexes.end(); idi!=idiEnd; idi++)
			rdfTypes[idi->second]->top=L"top";
		topHierarchyClassIndexes.clear();
		for (unsigned int r=0; r<rdfTypes.size();)
			if (rdfTypes[r]->cli->first==L"person" || rdfTypes[r]->cli->first==L"business" || rdfTypes[r]->cli->first==L"organization")
				rdfTypes.erase(rdfTypes.begin()+r);
			else
			{
				if (rdfTypes[r]->top.length())
					topHierarchyClassIndexes[rdfTypes[r]->cli->first]=r;
				r++;
			}
	}
	if (logQuestionDetail)
		for (unsigned int r=0; r<rdfTypes.size(); r++)
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch))
				rdfTypes[r]->logIdentity(LOG_WHERE,L"ISARelation",false);
	int p=100;
	for (unordered_map <wstring ,int >::iterator idi=topHierarchyClassIndexes.begin(),idiEnd=topHierarchyClassIndexes.end(); idi!=idiEnd; idi++)
		p=min(p,rdfTypes[idi->second]->confidence);
	int numPreferred=0;
	bool isPerson=false,isPlace=false,isBusiness=false,isWork=false;
	int placeTypeFound=-1;
	for (unordered_map <wstring ,int >::iterator idi=topHierarchyClassIndexes.begin(),idiEnd=topHierarchyClassIndexes.end(); idi!=idiEnd; idi++)
	{
		if (logQuestionDetail)
			rdfTypes[idi->second]->logIdentity(LOG_WHERE,L"ISARelationIdOffset",false);
		wstring tmpstr;
		if (logQuestionDetail)
			lplog(LOG_WHERE,L"ISADEBUG %s:%s:%d:%s",objectString(o,tmpstr,false).c_str(),rdfTypes[idi->second]->cli->first.c_str(),rdfTypes[idi->second]->confidence,idi->first.c_str());
		if (rdfTypes[idi->second]->confidence==p)
		{
			isPerson|=(idi->first==L"person");
			isPlace|=(idi->first==L"place") || (idi->first==L"location");
			isBusiness|=(idi->first==L"business") || (idi->first==L"organization");
			isWork|=(idi->first==L"work") || (idi->first==L"writtenwork") || (idi->first==L"creativeWork") || (idi->first==L"album") || (idi->first==L"periodical");
			if (idi->first==L"provincesandterritoriesofcanada")
				placeTypeFound=CANADIAN_PROVINCE_CITY;
			else if (idi->first==L"country")
				placeTypeFound=COUNTRY;
			else if (idi->first==L"island")
				placeTypeFound=ISLAND;
			else if (idi->first==L"mountain")
				placeTypeFound=MOUNTAIN_RANGE_PEAK_LANDFORM;
			else if (idi->first==L"geoclasspark")
				placeTypeFound=PARK_MONUMENT;
			else if (idi->first==L"river")
				placeTypeFound=RIVER_LAKE_WATERWAY;
			else if (idi->first==L"city")
				placeTypeFound=WORLD_CITY_TOWN_VILLAGE;
			else if (idi->first==L"statesoftheunitedstates")
				placeTypeFound=US_STATE_TERRITORY_REGION;
			else if (idi->first==L"state")
				placeTypeFound=US_STATE_TERRITORY_REGION;
			numPreferred++;
		}
	}
	if (!cOntology::cacheRdfTypes)
	  for (vector <cTreeCat *>::iterator rdfi=rdfTypes.begin(),rdfiEnd=rdfTypes.end(); rdfi!=rdfiEnd; rdfi++)
	  	delete (*rdfi); // now caching them
	isPlace|=(placeTypeFound>=0);
	if (isPlace && placeTypeFound<0)
		placeTypeFound=UNKNOWN_PLACE_SUBTYPE;
	wstring tmpstr;
	if (!isPlace && (isPerson || isBusiness || isWork))
	{
			objects[o].isNotAPlace=true;
			objects[o].resetSubType();
			lplog(LOG_WIKIPEDIA,L"%06d:%s reassigned (dbPedia) to not a place.",principalWhere,objectString(o,tmpstr,false).c_str());
	}
	if ((initialTenseOnly || !(objects[o].male ^ objects[o].female)) && (placeTypeFound>=0 || (isWork && !isPerson)))
	{
		bool reassigned=false;
		if (reassigned=(placeTypeFound>=0 && (objects[o].getSubType()<0 || objects[o].getSubType()==UNKNOWN_PLACE_SUBTYPE)))
			objects[o].setSubType(placeTypeFound);
		if (objects[o].objectClass==NAME_OBJECT_CLASS) 
			objects[o].objectClass=NON_GENDERED_NAME_OBJECT_CLASS;
		if (objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS) objects[o].objectClass=NON_GENDERED_GENERAL_OBJECT_CLASS;
		objects[o].male=objects[o].female=false;
		objects[o].neuter=true;
		if (reassigned)
			lplog(LOG_WIKIPEDIA,L"%06d:%s reassigned (dbPedia) to the object subtype %s.",principalWhere,
				objectString(o,tmpstr,false).c_str(),OCSubTypeStrings[placeTypeFound]);
		else
			lplog(LOG_WIKIPEDIA,L"%06d:%s ungendered (dbPedia).",principalWhere,objectString(o,tmpstr,false).c_str());
	}
	if ((initialTenseOnly || !(objects[o].male ^ objects[o].female)) && isBusiness && !isPerson && !isPlace && 
		(objects[o].objectClass==NAME_OBJECT_CLASS || 
		 objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS || 
		 objects[o].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS || 
		 objects[o].objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
	{
		objects[o].objectClass=NON_GENDERED_BUSINESS_OBJECT_CLASS;
		lplog(LOG_WIKIPEDIA,L"%06d:%s reassigned (dbPedia) to a business.",principalWhere,objectString(o,tmpstr,false).c_str());
		objects[o].male=objects[o].female=false;
		objects[o].neuter=true;
	}
	if ((initialTenseOnly || (objects[o].male ^ objects[o].female)) && !isBusiness && isPerson && !isPlace && !isWork && 
			objects[o].objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
	{
		objects[o].objectClass=NAME_OBJECT_CLASS;
		lplog(LOG_WIKIPEDIA,L"%06d:%s reassigned (dbPedia) to a name.",principalWhere,objectString(o,tmpstr,false).c_str());
		objects[o].male=objects[o].female=true;
		objects[o].neuter=false;
		if (objects[o].name.first==wNULL && objects[o].name.any==wNULL)
			objects[o].name.any=m[principalWhere].word;
	}
	objects[o].isWikiPlace=isPlace;
	objects[o].isWikiPerson=isPerson;
	objects[o].isWikiBusiness=isBusiness;
	objects[o].isWikiWork=isWork;
	return 0;
}

// is questionInformationSourceObject entirely uppercase, single word and having another class other than noun? (US - as in the acronym for the United States)
// is childSource primarily capitalized?  If so, throw out this source because matching is not reliable in this case.
bool cSource::checkForUppercaseSources(int questionInformationSourceObject)
{
	if (questionInformationSourceObject<0 || objects[questionInformationSourceObject].end-objects[questionInformationSourceObject].begin!=1 || 
		!(m[objects[questionInformationSourceObject].begin].flags&cWordMatch::flagAllCaps))
		return false;
	tIWMM w=m[objects[questionInformationSourceObject].begin].word;
	bool otherThanNoun=false;
	for (unsigned int f=0; f<w->second.formsSize() && !otherThanNoun; f++)
		otherThanNoun=(w->second.Form(f)->index!=nounForm && w->second.Form(f)->index!=PROPER_NOUN_FORM_NUM);
	return otherThanNoun;
}

bool cSource::skipSentenceForUpperCase(unsigned int &I)
{
	// is childSource primarily capitalized around I?
	unsigned int numWordsCapitalized=0,numWordsChecked=0,s;
	for (s=I+1; s<m.size() && !isEOS(s); s++,numWordsChecked++)
		if (m[s].flags&cWordMatch::flagAllCaps)
			numWordsCapitalized++;
	if (numWordsChecked && 100*numWordsCapitalized/numWordsChecked>90)
	{
		if (logQuestionDetail)
			lplog(LOG_WHERE,L"tossing out all cap source %d:%s.",I,sourcePath.c_str());
		I=s; // skip to the next sentence
		return true;
	}
  return false;
}

#ifdef TEST_CODE

int runJavaJerichoHTML(wstring webAddress, wstring outputPath, string &outbuf);

// a routine derived from readPage which uses Windows HTTP routines – not used
int cWord::readPageWinHTTP(wchar_t *str, wstring &buffer)
{
	LFS
		wstring lem;
	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	LPSTR pszOutBuffer;
	BOOL  bResults = FALSE;
	HINTERNET  hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;
	// http://umbel.org/umbel/rc/MusicalPerformer
	if (!cInternet::readPage(L"http://umbel.org/umbel/rc/MusicalPerformer.rdf", buffer)) return 0;

	wchar_t server[1024];
	wcscpy(server, L"umbel.org");
	wcscpy(str, L"/umbel/rc/MusicalPerformer.rdf");
	// Use WinHttpOpen to obtain a session handle.
	if (!(hSession = WinHttpOpen(L"HTTP/1.1",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0)))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpOpen - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// Use WinHttpSetTimeouts to set a new time-out values.
	if (!WinHttpSetTimeouts(hSession, 0, 0, 0, 0))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpSetTimeouts - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}

	// Specify an HTTP server.
	if (!(hConnect = WinHttpConnect(hSession, server,
		INTERNET_DEFAULT_HTTPS_PORT, 0)))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpConnect %s - %s.", server, getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// Create an HTTP request handle.
	if (!(hRequest = WinHttpOpenRequest(hConnect, L"GET", str,
		NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		0))) //WINHTTP_FLAG_SECURE )))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpOpenRequest %s - %s.", str, getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// Use WinHttpSetTimeouts to set a new time-out values.
	if (!WinHttpSetTimeouts(hRequest, 0, 0, 0, 0))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpSetTimeouts on request - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// WINHTTP_OPTION_SEND_TIMEOUT
	// Sets or retrieves an unsigned long integer value that contains the time-out value, in milliseconds, to send a request or write some data. If sending the request takes longer than the timeout, the send operation is canceled. The default timeout is 30 seconds.
	DWORD timeoutValue;
	DWORD dwBufferLength = sizeof(timeoutValue);
	if (!WinHttpQueryOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutValue, &dwBufferLength))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpQueryOption on request - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	lplog(LOG_ERROR, L"ERROR:WinHttpQueryOption yields a timeout value of %d(%d).", timeoutValue, dwBufferLength);
	timeoutValue = 0;
	dwBufferLength = sizeof(timeoutValue);
	if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutValue, dwBufferLength))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpSetOption on request - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	int start = clock();
	// Send a request.
	if (!(bResults = WinHttpSendRequest(hRequest,
		WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		WINHTTP_NO_REQUEST_DATA, 0,
		0, 0)))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpSendRequest %s%s - %s (%d MS).", server, str, getLastErrorMessage(lem), clock() - start);
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// End the request.
	if (!(bResults = WinHttpReceiveResponse(hRequest, NULL)))
	{
		lplog(LOG_ERROR, L"ERROR:Cannot WinHttpReceiveResponse %s%s - %s.", server, str, getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}

	// Keep checking for data until there is nothing left.
	if (bResults)
	{
		do
		{
			// Check for available data.
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				printf("Error %u in WinHttpQueryDataAvailable.\n",
				(int)GetLastError());

			// Allocate space for the buffer.
			pszOutBuffer = new char[dwSize + 1];
			if (!pszOutBuffer)
			{
				printf("Out of memory\n");
				dwSize = 0;
			}
			else
			{
				// Read the data.
				ZeroMemory(pszOutBuffer, dwSize + 1);

				if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
					dwSize, &dwDownloaded))
					printf("Error %u in WinHttpReadData.\n", (int)GetLastError());
				else
					printf("%s", pszOutBuffer);

				// Free the memory allocated to the buffer.
				delete[] pszOutBuffer;
			}
		} while (dwSize > 0);
	}


	// Report any errors.
	if (!bResults)
		printf("Error %d has occurred.\n", (int)GetLastError());

	// Close any open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);
	return 0;
}
#endif