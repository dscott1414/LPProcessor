/*
	getWikipedia.cpp - Wikipedia/HTML scrape, RDF-type cache, and ISA-relation lookup

	Overview:
		Two jobs in one TU: (1) download printable Wikipedia pages, strip chrome, and
		flatten tables/lists into the token stream; (2) resolve source objects to
		DBpedia/YAGO RDF types (disk + in-memory cache) and mine "X is a Y" sentences
		from those pages to assign place subtypes. Also hosts shared string-join
		helpers (vectorString/setString) used across QA.

	Pipeline position:
		On-demand during question answering and object typing (after objects exist).
		Not part of novel-parse initialization. processPath can parse a cached page
		as a child cSource.

	Key entry points:
		- reduceWikipediaPage / interpretHTMLTable / scanForTables - HTML reduction
		- getWikipediaPath / getObjectString - cache path + search URL
		- getExtendedRDFTypes / getExtendedRDFTypesMaster / getAssociationMapMaster
		- evaluateISARelation / identifyISARelation / processPath
		- lastErrorMsg / convertIllegalChars / eliminateHTML

	Dependencies:
		CACHEDIR\\wikipediaCache and dbPediaCache; cInternet::readPage; cOntology;
		MySQL no-ERDF-types table; Words.TABLE / END_COLUMN sentinels.

	Notes / gotchas:
		Wikipedia URLs use https:// and the search/object term is percent-encoded via encodeURL
		(defined in createOntology.cpp) before being interpolated into the query string; the
		nextWikiAddress link taken verbatim from a MediaWiki href is already percent-encoded and
		is not re-encoded. writeExtendedRDFTypes, getWikipediaPath's cBuffer, and readAttribs/
		writeAttribs now heap-allocate their scratch buffers (tmalloc/tfree) instead of putting
		multi-MiB arrays on the stack. processPath deletes and NULLs its 'source' out-param on
		every failure path after allocating it, and its callers check the return value before
		dereferencing source. firstMatchTableDeleteNested updates beginPos on both the primary
		<li> match and the <li value= fallback.
		readPageWinHTTP is compiled only under TEST_CODE and leaks WinHTTP handles on error.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "lpProcess.h"
#include "word.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "time.h"
#include <errno.h>
#include <functional>
//#include <locale>
#include "profile.h"
#include "internet.h"
#include "QuestionAnswering.h"

extern int logQuestionDetail; // not protected - too intensive to protect and doesn't matter
extern int logProximityMap; // not protected - too intensive to protect and doesn't matter
bool unlockTables(MYSQL& mysql);
void encodeURL(lpwstring winput, lpwstring& wencodedURL); // defined in createOntology.cpp
#define MAX_PATH_LEN 2048
#define MAX_BUF 2000000
//extern lpwstring basehttpquery; // initialized
//extern lpwstring prefix_colon; // initialized
//extern lpwstring prefix_owl_ontology; // initialized
//extern lpwstring prefix_dbpedia; // initialized
//extern lpwstring selectWhere; // initialized

	/* test wikipedia reduction */
	//lpchar_t webAddress[1024];
	//int ret; 
	//lpwstring buffer;
	//lp_snprintf(webAddress,MAX_LEN,u"http://en.wikipedia.org/wiki/Special:Search?search=%s&printable=yes&redirect=no",u"Paul_Krugman"); 
	//lp_snprintf(webAddress,MAX_LEN,u"http://en.wikipedia.org/wiki/Special:Search?search=%s&printable=yes&redirect=no",u"James_Michener"); 
	//lp_snprintf(webAddress,MAX_LEN,u"http://en.wikipedia.org/wiki/Curveball_(informant)"); 
	//if (ret=readPage(webAddress,buffer)) return ret;
	//int reduceWikipediaPage(lpwstring &buffer);
	//Words.TABLE=Words.predefineWord(u"lpTABLE"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	//Words.END_COLUMN=Words.predefineWord(u"lpENDCOLUMN"); // used to end each column string which is extracted from <table> and table-like constructions in HTML
	//Words.END_COLUMN_HEADERS=Words.predefineWord(u"lpENDCOLUMNHEADERS"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	//Words.MISSING_COLUMN=Words.predefineWord(u"lpMISSINGCOLUMN"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	//reduceWikipediaPage(buffer);

extern lpchar_t* lpOntologySuperClasses[]; // initialized
bool copy(unordered_map <lpwstring, cOntologyEntry>::iterator& hint, void* buf, int& where, int limit, unordered_map <lpwstring, cOntologyEntry>& hm);


// The message for the last failed system call, as a wide string.
// Batch B5: errno + strerror replaces GetLastError + FormatMessage. Note the
// narrowing of scope this implies and that callers should be aware of: errno is
// only meaningful immediately after a failed call, whereas GetLastError was set
// by Win32 API calls generally. Every call site here logs it directly after the
// failure it describes, so the behaviour is the same in practice.
lpwstring lastErrorMsg()
{
	LFS
		return lpwstring(lp_narrow_to_wide(std::string(strerror(errno))));
}

// In-place: replace WCHAR_ILLEGAL_PATH_CHARS runs with '!', then map any remaining
// non [alnum . - emdash] to '_'. Fatal if the rebuilt path exceeds MAX_PATH_LEN.
void convertIllegalChars(lpchar_t* path)
{
	LFS
		int len = lp_strlen(path);
	lpchar_t* pch = lp_wcstok(path, WCHAR_ILLEGAL_PATH_CHARS);
	if (!pch || len >= MAX_PATH_LEN) return;
	lpchar_t newPath[MAX_PATH_LEN + 4];
	newPath[0] = 0;
	len = 0;
	while (pch != NULL && len < MAX_PATH_LEN)
	{
		if (len + lp_strlen(pch) >= MAX_PATH_LEN)
			break;
		lp_strcpy(newPath + len, pch);
		len += lp_strlen(pch);
		newPath[len++] = u'!';
		pch = lp_wcstok(NULL, WCHAR_ILLEGAL_PATH_CHARS);
	}
	if (len > MAX_PATH_LEN)
		lplog(LOG_FATAL_ERROR, u"conversion overwrite");
	if (len) newPath[--len] = 0;
	lp_strcpy(path, newPath);
	for (int I = 0; path[I]; I++)
		if (!iswalnum(path[I]) && path[I] != u'.' && path[I] != u'-' && path[I] != u'—') // not all unicode dashes may be allowed in a Windows path, so not using isDash
			path[I] = u'_';
}

// Narrow-char cousin of convertIllegalChars: strips ILLEGAL_PATH_CHARS then maps leftovers to '_'.
void deleteIllegalChars(char* path)
{
	LFS
		int len = strlen(path);
	char* pch = strtok(path, ILLEGAL_PATH_CHARS);
	if (!pch || len >= MAX_PATH_LEN) return;
	char newPath[MAX_PATH_LEN];
	newPath[0] = 0;
	len = 0;
	while (pch != NULL && len < MAX_PATH_LEN)
	{
		if (len + strlen(pch) >= MAX_PATH_LEN)
			break;
		strcpy(newPath + len, pch);
		len += strlen(pch);
		pch = strtok(NULL, ILLEGAL_PATH_CHARS);
	}
	strcpy(path, newPath);
	for (int I = 0; path[I]; I++)
		if (!isalnum((unsigned char)path[I]) && path[I] != '.' && path[I] != '-' && path[I] != u'—') // not all unicode dashes may be allowed in a Windows path, so not using isDash
			path[I] = '_';
}

// Replaces every '_' in buffer with a space (Wikipedia title → display phrase).
void convertUnderlines(lpwstring& buffer)
{
	LFS
		for (unsigned int I = 0; I < buffer.length(); I++)
			if (buffer[I] == u'_') buffer[I] = u' ';
}

void eliminateHTML(lpwstring& buffer);
// Walks backward from whereHeadingEnd over </div> and </hN>, extracts the heading into
// header (preferring mw-headline text), sets tableOfContentsFlag if mw-toc-heading is
// present, and erases the heading from buffer (whereHeadingEnd is left at its start).
// process the immediately preceding header: <h2>Contents</h2>
// also process this: <div id="toctitle"><h2>Contents</h2></div>
void processHeader(lpwstring& buffer, size_t& whereHeadingEnd, lpwstring& header, bool& tableOfContentsFlag)
{
	LFS
		size_t beginPos = 0;
	lpwstring tableMetaHeader, match; // a header table that comes on the right and can be ignored for now.
	firstMatch(buffer, u"<table class=\"metadata plainlinks mbox-small", u"</table>", beginPos, match, true);
	while (true)
	{
		while (whereHeadingEnd && iswspace(buffer[whereHeadingEnd])) whereHeadingEnd--;
		if (whereHeadingEnd > 10 && buffer[whereHeadingEnd - 5] == u'<' && buffer[whereHeadingEnd - 4] == u'/' && buffer[whereHeadingEnd - 3] == u'd' && buffer[whereHeadingEnd - 2] == u'i' && buffer[whereHeadingEnd - 1] == u'v' && buffer[whereHeadingEnd] == u'>')
		{
			whereHeadingEnd -= 6;
			while (whereHeadingEnd && iswspace(buffer[whereHeadingEnd])) whereHeadingEnd--;
		}
		else break;
	}
	if (whereHeadingEnd > 10 && buffer[whereHeadingEnd] == u'>' && buffer[whereHeadingEnd - 2] == u'h' && buffer[whereHeadingEnd - 3] == u'/' && buffer[whereHeadingEnd - 4] == u'<')
	{
		// scan for the start of the header
		lpwstring headerStart = u"<h";
		headerStart += buffer[whereHeadingEnd - 1];
		size_t whereHeadingBegin = buffer.rfind(headerStart, whereHeadingEnd);
		if (whereHeadingBegin != lpwstring::npos)
		{
			header = buffer.substr(whereHeadingBegin, whereHeadingEnd - whereHeadingBegin + 1);
			tableOfContentsFlag = header.find(u"mw-toc-heading") != lpwstring::npos;
			buffer.erase(whereHeadingBegin, whereHeadingEnd - whereHeadingBegin + 1);
			whereHeadingEnd = whereHeadingBegin;
			lpwstring title;
			takeLastMatch(header, u"<span class=\"mw-headline\"", u"</span>", title, false);
			if (title.length() > 0)
			{
				int wh = title.find(u">");
				if (wh >= 0) title.erase(0, wh + 1);
				header = title;
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
void scanForTables(lpwstring& buffer, vector < vector <lpwstring> >& tables, bool unordered);
// Parses one <table> already extracted into match: heading, <th> headers, then <td> cells
// (padding MISSING_COLUMN). Nested tables/lists inside cells are scanned first. Pushes
// the flat row onto tables if it has more than the heading.
void interpretHTMLTable(lpwstring& buffer, size_t& whereHeadingEnd, lpwstring& match, vector < vector <lpwstring> >& tables)
{
	LFS
		vector <lpwstring> table;
	lpwstring tableHeader;
	bool tableOfContentsFlag = false;
	processHeader(buffer, whereHeadingEnd, tableHeader, tableOfContentsFlag);
	table.push_back(tableHeader);
	lpwstring columnHeaders, columnHeader;
	size_t rowPosition = 0, beginColumnHeader = 0;
	if (firstMatch(match, u"<tr ", u"</tr>", rowPosition, columnHeaders, false) == lpwstring::npos) return;
	while (firstMatch(columnHeaders, u"<th>", u"</th>", beginColumnHeader, columnHeader, false) != lpwstring::npos)
	{
		scanForTables(columnHeader, tables, true);
		scanForTables(columnHeader, tables, false);
		eliminateHTML(columnHeader);
		table.push_back(columnHeader);
	}
	int numColumns = table.size() - 1;
	size_t beginColumn = 0;
	if (!numColumns)
		return;
	table.push_back(Words.END_COLUMN_HEADERS->first);
	lpwstring columns, columnIndex;
	while (firstMatch(match, u"<tr ", u"</tr>", rowPosition, columns, false) != lpwstring::npos)
	{
		int numColumn;
		for (numColumn = 0, beginColumn = 0; numColumn < numColumns && firstMatch(columns, u"<td", u"</td>", beginColumn, columnIndex, false) != lpwstring::npos; numColumn++)
		{
			// eliminate until the '>'
			size_t gt = columnIndex.find(u'>');
			if (gt != lpwstring::npos)
				columnIndex.erase(0, gt + 1);
			scanForTables(columnIndex, tables, true);
			scanForTables(columnIndex, tables, false);
			eliminateHTML(columnIndex);
			table.push_back(columnIndex);
		}
		for (; numColumn < numColumns; numColumn++)
			table.push_back(Words.MISSING_COLUMN->first);
	}
	if (table.size() > 1)
		tables.push_back(table);
}

void eliminateHTMLCharacterEntities(lpwstring& buffer);
// Strips tags and [N] footnotes after expanding character entities. Inserts a space when
// two tagged spans abut so "ISBN""123" does not glue. The footnote scan's [I+1..I+3] lookahead
// is bounds-safe: each index is only read after the previous one was confirmed to be a real
// (non-terminator) character, and lpwstring guarantees buffer[buffer.size()] reads as u'\0'.
void eliminateHTML(lpwstring& buffer)
{
	LFS

		eliminateHTMLCharacterEntities(buffer);
	lpwstring noHTML;
	bool inHTML = false;
	for (unsigned int I = 0; I < buffer.length(); I++)
		if (buffer[I] == u'<')
			inHTML = true;
		else if (buffer[I] == u'>')
		{
			// make sure there is space between entities that have both been linked like "ISBN" and the ISBN number.
			// (this must check the output accumulated so far, not the tail of the raw input buffer)
			if (!noHTML.empty() && !iswspace(noHTML[noHTML.size() - 1]))
				noHTML += u" ";
			inHTML = false;
		}
		else if (!inHTML)
		{
			// eliminate footnotes
			if (buffer[I] == u'[' && iswdigit(buffer[I + 1]) && (buffer[I + 2] == u']' || (iswdigit(buffer[I + 2]) && buffer[I + 3] == u']')))
			{
				I += 2;
				if (buffer[I + 1] == u']') I++;
				continue;
			}
			noHTML += buffer[I];
		}
	buffer = noHTML;
}

// Extracts the next <li>…</li> (or <li value=) from buffer into match, deleting one level
// of nested <li> from the parent. Returns the start index, or npos.
// this is to skip nested tables, instead of turning nested tables into another entry in the parent table (what firstMatch would do)
// this only works for one level of nesting!  The nested table can have any number of entries
// beginString can be <li value= OR <li>
int firstMatchTableDeleteNested(lpwstring& buffer, size_t& beginPos, lpwstring& match)
{
	lpwstring beginString = u"<li>", endString = u"</li>";
	LFS
		int tempBeginPos = buffer.find(beginString, (beginPos == lpwstring::npos) ? 0 : beginPos);
	if (tempBeginPos == lpwstring::npos)
		beginPos = buffer.find(u"<li value=", (beginPos == lpwstring::npos) ? 0 : beginPos);
	else
		beginPos = tempBeginPos;
	int findEndOfBegin = buffer.find(u">", beginPos);
	if (findEndOfBegin == lpwstring::npos)
		return -1;
	int endPos;
	if (beginPos != lpwstring::npos && (endPos = buffer.find(endString, findEndOfBegin + 1)) != lpwstring::npos)
	{
		int len = endPos - beginPos + endString.length();  // amount to cut, which includes begin and end
		match = buffer.substr(findEndOfBegin + 1, endPos - (findEndOfBegin + 1)); // do not include begin or end
		do {
			int nestedBeginPos = buffer.find(beginString, findEndOfBegin + 1);
			if (nestedBeginPos == lpwstring::npos)
				nestedBeginPos = buffer.find(u"<li value=", findEndOfBegin + 1);
			if (nestedBeginPos != lpwstring::npos && nestedBeginPos < endPos)
			{
				int newEndPos = buffer.find(endString, endPos + endString.length());
				if (newEndPos != lpwstring::npos)
				{
					int nestedEndPos = endPos;
					endPos = newEndPos;
					int nestedLen = nestedEndPos - nestedBeginPos + endString.length(); // amount to cut, which includes begin and end
					if (nestedLen >= 1)
						buffer.erase(nestedBeginPos, nestedLen);
					endPos -= nestedLen;
					len = endPos - beginPos + endString.length(); // amount to cut, which includes begin and end
					//findEndOfBegin = buffer.find(u">", beginPos);
					//if (findEndOfBegin == lpwstring::npos)
					//	return -1;
					match = buffer.substr(findEndOfBegin + 1, endPos - (findEndOfBegin + 1)); // do not include begin or end
				}
			}
			else
				break;
		} while (true);
		if (len >= 1)
			buffer.erase(beginPos, len);
		return beginPos;
	}
	return beginPos = lpwstring::npos;
}

// JK Rowling / Paul Krugman
// <h3> <span class="mw-headline" id="Articles">Articles</span></h3>
//<ul> OR <ol>
//<li>1997: <a href="/wiki/Nestl%C3%A9_Smarties_Book_Prize" title="Nestlé Smarties Book Prize">Nestlé Smarties Book Prize</a>, Gold Award for <i>Harry Potter and the Philosopher's Stone</i></li>
// <h3> is not consistent.  <li> must be consecutive.
// Finds each <ul> or <ol> (unordered flag), attaches the preceding heading, and pushes
// one single-column "table" of <li> rows (TOC headings get Words.TOC_HEADER prefixed).
void scanForTables(lpwstring& buffer, vector < vector <lpwstring> >& tables, bool unordered)
{
	LFS
		size_t tablePosition = 0;
	lpwstring tableHtml;
	const lpchar_t* beginTable, * endTable;
	if (unordered)
	{
		beginTable = u"<ul>";
		endTable = u"</ul>";
	}
	else
	{
		beginTable = u"<ol>";
		endTable = u"</ol>";
	}
	while (firstMatchNonEmbedded(buffer, beginTable, endTable, tablePosition, tableHtml, false) >= 0)
	{
		vector <lpwstring> table;
		lpwstring tableHeader, columnIndex;
		bool tableOfContentsFlag = false;
		processHeader(buffer, tablePosition, tableHeader, tableOfContentsFlag);
		if (tableOfContentsFlag)
			tableHeader = Words.TOC_HEADER->first + u" " + tableHeader;
		table.push_back(tableHeader);
		table.push_back(Words.END_COLUMN_HEADERS->first);
		size_t rowPosition = 0;
		lpwstring row;
		while (firstMatchTableDeleteNested(tableHtml, rowPosition, row) >= 0)
		{
			eliminateHTML(row);
			table.push_back(row);
		}
		if (table.size() > 0)
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
// Reduces a printable Wikipedia HTML page in place: keep body content, drop chrome,
// extract tables/lists into Words.TABLE sentinels, or (search-results pages) follow up
// to 10 hits with Relevance>=70 into wikipediaCache and concatenate their text.
// Returns 0, or a cInternet::readPage error / GETPAGE_CANNOT_CREATE.
int reduceWikipediaPage(lpwstring& buffer)
{
	LFS
		vector < vector <lpwstring> > tables; // first entry is the heading, if no heading the first entry is empty
	lpwstring match;
	takeLastMatch(buffer, u"<!-- start content -->", u"<!-- end content -->", match, false);
	if (match.length())
		buffer = match;
	takeLastMatch(buffer, u"<!-- bodyContent -->", u"<!-- /bodyContent -->", match, false);
	if (match.length())
		buffer = match;
	takeLastMatch(buffer, u"<!-- printfooter -->", u"<!-- /printfooter -->", match, false);
	takeLastMatch(buffer, u"<!-- catlinks -->", u"<!-- /catlinks -->", match, false);
	while (takeLastMatch(buffer, u"<style", u"</style>", match, false) >= 0);
	takeLastMatch(buffer, u"<div class=\"floatnone\">", u"</div>", match, false);
	takeLastMatch(buffer, u"<div style=\"float: left;\">", u"</div>", match, false);
	takeLastMatch(buffer, u"<div style=\"margin-left: 60px;\">", u"</div>", match, false);
	takeLastMatch(buffer, u"<div class=\"infobox sisterproject\" style=\"float:right;\">", u"</div>", match, false);
	takeLastMatch(buffer, u"<div id=\"mwe_player", u"</div>", match, false);
	while (takeLastMatch(buffer, u"<div class=\"thumb", u"</div>", match, false) >= 0);
	takeLastMatch(buffer, u"<form id=\"powersearch\" method=\"get\" action=\"/wiki/Special:Search\">", u"</form>", match, false);
	takeLastMatch(buffer, u"<span class=\"toctogglespan\">", u"</span>", match, false);
	//takeLastMatch(buffer,u"<ol ",u"</ol>",match,false);
	size_t pos = lpwstring::npos;
	while (firstMatch(buffer, u"<!--", u"-->", pos, match, false) != lpwstring::npos);
	while (firstMatch(buffer, u"<table", u"</table>", pos, match, false) != lpwstring::npos)
		interpretHTMLTable(buffer, pos, match, tables);
	while (firstMatch(buffer, u"<sup", u"</sup>", pos, match, false) != lpwstring::npos);
	if (buffer.find(u"Results 1-", 0) == lpwstring::npos)
	{
		// keep apostrophes
		// <span style="padding-left:0.1em;">'</span>s
		scanForTables(buffer, tables, true);
		scanForTables(buffer, tables, false);
		while ((pos = takeLastMatch(buffer, u"<span", u"</span>", match, false)) != lpwstring::npos) // span elements can be embedded in one another
		{
			int onlyText = match.find('>'); // don't scan what is in the HTML, only what should be text
			if (onlyText != lpwstring::npos) match.erase(0, onlyText + 1);
			eliminateHTML(match);
			if (match.size() > 5) // make sure it is not a raised symbol like 'r' or something which is not actual text
				buffer.insert(pos, match); // eliminates Relevance %
		}
		lpwstring justPara;
		while (!nextMatch(buffer, u"<p>", u"</p>", pos, match, false)) justPara += match + u"\n  ";
		buffer = justPara;
		eliminateHTML(buffer);
		while ((pos = buffer.find(u": )")) != lpwstring::npos)
			buffer.replace(pos, 3, u")");
		lpwstring tmpstr;
		for (unsigned int I = 0; I < tables.size(); I++)
		{
			buffer += u"\n\n\n" + Words.TABLE->first + u" " + itos(I, tmpstr) + u".\n\n";
			for (unsigned int J = 0; J < tables[I].size(); J++)
				buffer += tables[I][J] + u". " + Words.END_COLUMN->first + u".\n";
		}
	}
	else
	{
		// <li style="padding-bottom: 1em;"><a href="/wiki/1949_Armistice_Agreements" title="1949 Armistice Agreements">1949 Armistice Agreements</a><br />
		// <span style="color: green; font-size: small;">Relevance: 77.2% -  - </span></li>
		lpwstring final;
		pos = lpwstring::npos;
		for (int numPagesScanned = 0; numPagesScanned < 10 && !nextMatch(buffer, u"<li", u"</li>", pos, match, false); numPagesScanned++)
		{
			lpwstring nextWikiAddress;
			takeLastMatch(match, u"<a href=\"/wiki/", u"\" title", nextWikiAddress, false);
			lpwstring sRelevance;
			takeLastMatch(match, u"Relevance: ", u"%", sRelevance, false);
			if (sRelevance.length() && lp_wtoi(sRelevance.c_str()) < 70)
				break;
			lpchar_t path[MAX_LEN];
			int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\wikipediaCache", CACHEDIR) + 1;
			if (lp_wmkdir(path) < 0 && errno == ENOENT)
				lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
			lp_snprintf(path, MAX_LEN, u"%s\\wikipediaCache\\_%s.txt", CACHEDIR, nextWikiAddress.c_str());
			convertIllegalChars(path + pathlen);
			distributeToSubDirectories(path, pathlen, true);
			if (lp_waccess(path, 0) < 0)
			{
				lpchar_t webAddress[MAX_LEN];
				// https://en.wikipedia.org/w/index.php?title=Localized_versions_of_the_Monopoly_game&printable=yes
				// https://en.wikipedia.org/w/index.php?title=List_of_French_phrases_used_by_English_speakers&printable=yes
				// nextWikiAddress comes verbatim from a MediaWiki-generated href, which is already
				// percent-encoded, so it is not re-encoded here (that would double-encode it).
				lp_snprintf(webAddress, MAX_LEN, u"https://en.wikipedia.org/w/index.php?title=%s&printable=yes", nextWikiAddress.c_str());
				lplog(LOG_WIKIPEDIA, u"%s", webAddress);
				int ret;
				lpwstring secondaryBuffer;
				if (ret = cInternet::readPage(webAddress, secondaryBuffer)) return ret;
				reduceWikipediaPage(secondaryBuffer);
				int fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
				if (fd < 0)
				{
					lplog(LOG_ERROR, u"ERROR:Cannot create path %s - %S (5).", path, sys_errlist[errno]);
					return cInternet::GETPAGE_CANNOT_CREATE;
				}
				::write(fd, secondaryBuffer.c_str(), secondaryBuffer.length() * sizeof(secondaryBuffer[0]));
				::close(fd);
				convertUnderlines(nextWikiAddress);
				final += nextWikiAddress + u"\n\n";
				final += secondaryBuffer + u"\n\n\n";
			}
			else
			{
				// heap-allocated (tmalloc/tfree) rather than a ~3.81 MiB stack array
				lpchar_t* cBuffer = (lpchar_t*)tmalloc(MAX_BUF * sizeof(lpchar_t));
				if (cBuffer)
				{
					int actualLenInBytes;
					if (!getPath(path, cBuffer, MAX_BUF, actualLenInBytes))
					{
						cBuffer[actualLenInBytes / sizeof(buffer[0])] = 0;
						convertUnderlines(nextWikiAddress);
						final += nextWikiAddress + u"\n\n";
						final += cBuffer + lpwstring(u"\n\n\n");
					}
					tfree(MAX_BUF * sizeof(lpchar_t), cBuffer);
				}
			}
		}
		buffer = final;
	}
	if (buffer.find(u"For search options, see Help:Searching.") != lpwstring::npos && buffer.length() < 45)
		buffer.clear();
	return 0;
}

// Joins vstr into tmpstr with a leading separator on every element (including the first).
lpwstring vectorString(vector <lpwstring>& vstr, lpwstring& tmpstr, lpwstring separator)
{
	LFS
		tmpstr.clear();
	for (vector <lpwstring>::iterator sci = vstr.begin(), sciEnd = vstr.end(); sci != sciEnd; sci++)
		tmpstr += separator + *sci;
	return tmpstr;
}

// Narrow-string overload of vectorString (leading separator on every element).
string vectorString(vector <string>& vstr, string& tmpstr, string separator)
{
	LFS
		tmpstr.clear();
	for (vector <string>::iterator sci = vstr.begin(), sciEnd = vstr.end(); sci != sciEnd; sci++)
		tmpstr += separator + *sci;
	return tmpstr;
}

// Joins a vector-of-vectors as " [a,b] [c,d]" using the lpwstring vectorString helper.
lpwstring vectorString(vector < vector <lpwstring> >& vstr, lpwstring& tmpstr, lpwstring separator)
{
	LFS
		lpwstring tmpstr2;
	tmpstr.clear();
	for (vector < vector <lpwstring> >::iterator sci = vstr.begin(), sciEnd = vstr.end(); sci != sciEnd; sci++)
		tmpstr += u" [" + vectorString(*sci, tmpstr2, separator) + u"]";
	return tmpstr;
}

// Joins a set into tmpstr with separator, then drops the leading separator if non-empty.
lpwstring setString(set <lpwstring>& sstr, lpwstring& tmpstr, const lpchar_t* separator)
{
	LFS
		tmpstr.clear();
	for (set <lpwstring>::iterator sci = sstr.begin(), sciEnd = sstr.end(); sci != sciEnd; sci++)
		tmpstr += separator + *sci;
	if (tmpstr.length())
		tmpstr = tmpstr.substr(1);
	return tmpstr;
}

// unordered_set overload of setString (order is hash order).
lpwstring setString(unordered_set <lpwstring>& sstr, lpwstring& tmpstr, const lpchar_t* separator)
{
	LFS
		tmpstr.clear();
	for (auto ws : sstr)
		tmpstr += separator + ws;
	if (tmpstr.length())
		tmpstr = tmpstr.substr(1);
	return tmpstr;
}

// Narrow set join; unlike the wide overload, the leading separator is kept.
string setString(set <string>& sstr, string& tmpstr, const char* separator)
{
	LFS
		tmpstr.clear();
	for (set <string>::iterator sci = sstr.begin(), sciEnd = sstr.end(); sci != sciEnd; sci++)
		tmpstr += separator + *sci;
	return tmpstr;
}

// Writes DBPEDIA / YAGO / UMBEL<resourceType> / Unknown into tmpstr and returns tmpstr.c_str().
const lpchar_t* ontologyTypeString(int ontologyType, int resourceType, lpwstring& tmpstr)
{
	LFS
		switch (ontologyType)
		{
		case dbPedia_Ontology_Type:tmpstr = u"DBPEDIA"; break;
		case YAGO_Ontology_Type:tmpstr = u"YAGO"; break;
		case UMBEL_Ontology_Type:tmpstr = u"UMBEL"; tmpstr += ('0' + resourceType); break;
		default:tmpstr = u"Unknown"; break;
		}
	return tmpstr.c_str();
}

// Formats this RDF category (parent, type, preferred flags, supers, confidence) into tmpstr.
lpwstring cTreeCat::toString(lpwstring& tmpstr)
{
	LFS
		lpwstring tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6, tmpstr7, tmpstr8;
	return tmpstr = parentObject + u":" + typeObject + ((preferred) ? u"PREFERRED" : u"") + ((exactMatch) ? u"EMPREFERRED" : u"") + u" ISTYPE " + cli->first + u":" +
		ontologyTypeString(cli->second.ontologyType, cli->second.resourceType, tmpstr8) + u":" + cli->second.compactLabel +
		u"Wikipedia:" + vectorString(wikipediaLinks, tmpstr5, u",") +
		u"Professions:" + vectorString(professionLinks, tmpstr6, u",") +
		u"[SUPER " + setString(cli->second.superClasses, tmpstr, u" ") + u"]:confidence,ontologyHierarchicalRank " + itos(cli->second.ontologyHierarchicalRank, tmpstr3) + u"," + itos(confidence, tmpstr7) + u"(" + qtype + u")";
}

// Logs toString() at whichLog, optionally prefixed with ofWhichObject.
void cTreeCat::lplogTC(int whichLog, lpwstring ofWhichObject)
{
	LFS
		lpwstring tmpstr;
	if (ofWhichObject.length())
		::lplog(whichLog, u"%s:%s", ofWhichObject.c_str(), toString(tmpstr).c_str());
	else
		::lplog(whichLog, u"%s", toString(tmpstr).c_str());
}

// Tries getRDFTypes with decreasing PP extensions and keeps the lowest-confidence / preferred
// set, then includeSuperClasses + setPreferred. Returns rdfTypes.size().
int cSource::getExtendedRDFTypes(int where, vector <cTreeCat*>& rdfTypes, unordered_map <lpwstring, int >& topHierarchyClassIndexes, lpwstring fromWhere, bool ignoreMatches, bool fileCaching)
{
	LFS
		int lastLowestConfidence = -1;
	bool lastPreferenceFound = false;
	int maxPrepositionalPhraseNonMixMatch = -1;
	ppExtensionAvailable(where, maxPrepositionalPhraseNonMixMatch, true);
	bool contextualBase = (m[where].endObjectPosition - m[where].beginObjectPosition) >= 3;
	int minPrepositionalPhraseNonMixMatch = (maxPrepositionalPhraseNonMixMatch == 0 || contextualBase) ? 0 : 1;
	for (int extendNumPP = maxPrepositionalPhraseNonMixMatch; extendNumPP >= minPrepositionalPhraseNonMixMatch; extendNumPP--)
	{
		vector <cTreeCat*> tempRdfTypes;
		getRDFTypes(where, tempRdfTypes, fromWhere, extendNumPP, ignoreMatches, fileCaching);
		bool preferenceFound = true;
		int lowestConfidence = CONFIDENCE_NOMATCH;
		for (unsigned int r = 0; r < tempRdfTypes.size(); r++)
			if (tempRdfTypes[r]->cli->first != SEPARATOR)
			{
				preferenceFound |= tempRdfTypes[r]->preferred || tempRdfTypes[r]->exactMatch;
				lowestConfidence = min(lowestConfidence, tempRdfTypes[r]->confidence);
			}
		if (lastLowestConfidence < 0 || (lastLowestConfidence > 1 && lowestConfidence == 1 && (preferenceFound || !lastPreferenceFound)) || (lastLowestConfidence == lowestConfidence))
		{
			rdfTypes = tempRdfTypes;
			lastLowestConfidence = lowestConfidence;
			lastPreferenceFound = preferenceFound;
		}
		else
		{
			lastLowestConfidence = min(lastLowestConfidence, lowestConfidence);
			rdfTypes.insert(rdfTypes.end(), tempRdfTypes.begin(), tempRdfTypes.end());
		}
		// if the base is contextualized (large enough to form a single object on its own), then exit as soon as we have anything
		if (contextualBase && rdfTypes.size() > 0)
			break;
	}
	int originalRDFTypesSize = rdfTypes.size();
	{
		lpwstring oStr;
		getRDFWhereString(where, oStr, u"_", 0, ignoreMatches);
		lplog(LOG_RESOLUTION, u"%d:SUPERCLASSES of %s BEGIN %d", where, oStr.c_str());
	}
	//extern int logOntologyDetail;
	//logSynonymDetail = logQuestionDetail = logOntologyDetail = logRDFDetail = 1;
	cOntology::includeSuperClasses(topHierarchyClassIndexes, rdfTypes);
	{
		lpwstring oStr;
		getRDFWhereString(where, oStr, u"_", 0, ignoreMatches);
		lplog(LOG_RESOLUTION, u"%d:SUPERCLASSES of %s END %d->%d", where, oStr.c_str(), originalRDFTypesSize, rdfTypes.size());
		if (originalRDFTypesSize && (rdfTypes.size() / originalRDFTypesSize > 2 || rdfTypes.size() - originalRDFTypesSize > 10))
			lplog(LOG_RESOLUTION, u"%d:SUPERCLASSES GROWTH! of %s %d->%d", where, oStr.c_str(), originalRDFTypesSize, rdfTypes.size());
	}
	//logSynonymDetail = logQuestionDetail = logOntologyDetail = 0;
	// reset lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms
	for (int I = 0; I < rdfTypes.size(); I++)
		rdfTypes[I]->preferred = false;
	cOntology::setPreferred(topHierarchyClassIndexes, rdfTypes);
	return rdfTypes.size();
}

// rdfIdentify on objectString(object) plus superclasses/preferred. No Wikipedia fetch.
int cSource::getObjectRDFTypes(int object, vector <cTreeCat*>& rdfTypes, unordered_map <lpwstring, int >& topHierarchyClassIndexes, lpwstring fromWhere)
{
	LFS
		cOntology::fillOntologyList(false);
	lpwstring tmpstr;
	objectString(object, tmpstr, true);
	cOntology::rdfIdentify(tmpstr, rdfTypes, fromWhere);
	cOntology::includeSuperClasses(topHierarchyClassIndexes, rdfTypes);
	// reset lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms
	for (int I = 0; I < rdfTypes.size(); I++)
		rdfTypes[I]->preferred = false;
	cOntology::setPreferred(topHierarchyClassIndexes, rdfTypes);
	return rdfTypes.size();
}

// Loads a binary .eRdfTypes cache. Returns -1 if missing, -2 if version mismatches (file
// deleted), 0 on success. Allocates cTreeCat with new (caller / cache map owns them).
int cSource::readExtendedRDFTypes(lpchar_t path[4096], vector <cTreeCat*>& rdfTypes, unordered_map <lpwstring, int >& topHierarchyClassIndexes)
{
	LFS
		int fd = lp_wopen(path, O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE), errorCode = (fd < 0 ? errno : 0);
	if (errorCode != 0)
	{
		//lplog(LOG_ERROR,u"Cannot open extended rdfTypes (%s) - %S (%d).",path,strerror(errno),errno);
		return -1;
	}
	void* buffer;
	int bufferlen = lp_filelength(fd), where = 0;
	buffer = (void*)tmalloc(bufferlen + 10);
	::read(fd, buffer, bufferlen);
	::close(fd);
	if (*((lpchar_t*)buffer) != EXTENDED_RDFTYPE_VERSION) // version
	{
		tfree(bufferlen + 10, buffer);
		lp_wremove(path);
		return -2;
	}
	where += 2;
	int rdfTypeCount;
	if (!::copy(rdfTypeCount, buffer, where, bufferlen))
		lplog(LOG_FATAL_ERROR, u"Cannot read count of extended rdfTypes (%s) - %S (%d).", path, strerror(errno), errno);
	for (int c = 0; c < rdfTypeCount; c++)
	{
		cTreeCat* tc = new cTreeCat();
		if (!tc->copy(cOntology::dbPediaOntologyCategoryList, buffer, where, bufferlen))
			lplog(LOG_FATAL_ERROR, u"Cannot read tree cat of extended rdfTypes (%s) - %S (%d).", path, strerror(errno), errno);
		rdfTypes.push_back(tc);
	}
	int topHierarchyClassIndexesCount;
	if (!::copy(topHierarchyClassIndexesCount, buffer, where, bufferlen))
		lplog(LOG_FATAL_ERROR, u"Cannot read top index count of extended rdfTypes (%s) - %S (%d).", path, strerror(errno), errno);
	for (int c = 0; c < topHierarchyClassIndexesCount; c++)
	{
		lpwstring hClass;
		int index = 0;
		if (!::copy(hClass, buffer, where, bufferlen) ||
			!::copy(index, buffer, where, bufferlen))
			lplog(LOG_FATAL_ERROR, u"Cannot read top index of extended rdfTypes (%s) - %S (%d).", path, strerror(errno), errno);
		topHierarchyClassIndexes[hClass] = index;
	}
	tfree(bufferlen + 10, buffer);
	return 0;
}

#define EMAX_BUF MAX_BUF*10
// Writes .eRdfTypes via a 20MB heap buffer (tmalloc/tfree), flushing every ~8KB. Returns -1 if
// create (or the allocation) fails.
int cSource::writeExtendedRDFTypes(lpchar_t path[4096], vector <cTreeCat*>& rdfTypes, unordered_map <lpwstring, int >& topHierarchyClassIndexes)
{
	LFS
		int fd = lp_wopen(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE), errorCode = (fd < 0 ? errno : 0);
	if (errorCode != 0)
	{
		lplog(LOG_ERROR, u"Cannot write extended rdfTypes (%s) - %S (%d).", path, strerror(errno), errno);
		return -1;
	}
	char* buffer = (char*)tmalloc(EMAX_BUF);
	if (!buffer)
	{
		lplog(LOG_ERROR, u"Cannot allocate %d bytes to write extended rdfTypes (%s).", EMAX_BUF, path);
		::close(fd);
		return -1;
	}
	int where = 0;
	*((lpchar_t*)buffer) = EXTENDED_RDFTYPE_VERSION;
	where += 2;
	if (!::copy(buffer, (int)rdfTypes.size(), where, EMAX_BUF))
		lplog(LOG_FATAL_ERROR, u"Cannot write extended rdfTypes count (%s) - %S (%d).", path, strerror(errno), errno);
	for (vector <cTreeCat*>::iterator ri = rdfTypes.begin(), riEnd = rdfTypes.end(); ri != riEnd; ri++)
	{
		if (!(*ri)->copy(buffer, where, EMAX_BUF))
			lplog(LOG_FATAL_ERROR, u"Cannot write extended rdfTypes (%s) - %S (%d).", path, strerror(errno), errno);
		if (where > (EMAX_BUF)-8192)
		{
			::write(fd, buffer, where);
			where = 0;
		}
	}
	if (!::copy(buffer, (int)topHierarchyClassIndexes.size(), where, EMAX_BUF))
		lplog(LOG_FATAL_ERROR, u"Cannot write extended rdfTypes top class count (%s) - %S (%d).", path, strerror(errno), errno);
	for (unordered_map <lpwstring, int >::iterator ti = topHierarchyClassIndexes.begin(), tiEnd = topHierarchyClassIndexes.end(); ti != tiEnd; ti++)
	{
		if (!::copy(buffer, ti->first, where, EMAX_BUF) || !::copy(buffer, ti->second, where, EMAX_BUF))
			lplog(LOG_FATAL_ERROR, u"Cannot write extended rdfTypes (%s) - %S (%d).", path, strerror(errno), errno);
		if (where > (EMAX_BUF)-8192)
		{
			::write(fd, buffer, where);
			where = 0;
		}
	}
	if (where > 0)
		::write(fd, buffer, where);
	tfree(EMAX_BUF, buffer);
	::close(fd);
	return 0;
}

// Lowercases RDFType, '_'→' ', and decodes $XXXX hex escapes in place. Returns RDFType.
lpwstring transformRDFTypeName(lpwstring& RDFType)
{
	transform(RDFType.begin(), RDFType.end(), RDFType.begin(), (int(*)(int)) tolower);
	replace(RDFType.begin(), RDFType.end(), u'_', u' ');
	size_t where = 0;
	while (where < RDFType.length() - 4 && (where = RDFType.find(u'$', where)) != lpwstring::npos)
	{
		if (where + 4 < RDFType.length() && iswxdigit(RDFType[where + 1]) && iswxdigit(RDFType[where + 2]) && iswxdigit(RDFType[where + 3]) && iswxdigit(RDFType[where + 4]))
		{
			lpchar_t replaceCh = 0;
			for (int I = where + 1; I < where + 5; I++)
				replaceCh = (replaceCh << 4) + ((iswdigit(RDFType[I])) ? (RDFType[I] - u'0') : (RDFType[I] - 'a' + 10));
			RDFType.erase(where + 1, 4);
			RDFType[where] = replaceCh;
		}
		where++;
	}
	return RDFType;
}

// If childWord is a multi-word category whose last token is uncapitalized (and has no ) or $),
// copies that last token into lastWord and returns true. Used to map "American novelist"→"novelist".
bool cSource::categoryMultiWord(lpwstring& childWord, lpwstring& lastWord)
{
	int lastSpace = -1;
	// contains capitalized word?
	bool isLastCapitalized = true;
	for (int I = 0; I < childWord.length(); I++)
	{
		if (childWord[I] == u')' || childWord[I] == u'$')
			return false;
		// last space?
		if (childWord[I] == u' ' || childWord[I] == u'_')
		{
			lastSpace = I;
			isLastCapitalized = iswupper(childWord[I + 1]) != 0;
		}
	}
	if (isLastCapitalized)
		return false;
	lastWord = childWord;
	lastWord.erase(0, lastSpace + 1);
	return true;
}

// attempt to transform rdf types, wikipedia links and profession links into single words which can be matched to other words
void cSource::getRDFTypeSimplificationToWordAssociationWithObjectMap(lpwstring object, vector <cTreeCat*>& rdfTypes, unordered_map<lpwstring, int>& RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap)
{
	for (vector <cTreeCat*>::iterator rdfi = rdfTypes.begin(), rdfiEnd = rdfTypes.end(); rdfi != rdfiEnd; rdfi++)
	{
		if ((*rdfi)->cli->first == SEPARATOR)
			continue;
		lpwstring childWord = (*rdfi)->cli->first, transformedChildWord, lastWord;
		if (categoryMultiWord(childWord, lastWord))
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE, u"RDFSimplificationToWordMapping MultiWord %-32s->%s confidence %d", childWord.c_str(), lastWord.c_str(), (*rdfi)->confidence << 1);
			RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[lastWord] = (*rdfi)->confidence << 1;
		}
		RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[transformedChildWord = transformRDFTypeName(childWord)] = (*rdfi)->confidence;
		if (logQuestionDetail)
			lplog(LOG_WHERE, u"RDFSimplificationToWordMapping %-32s->%s confidence %d", childWord.c_str(), transformedChildWord.c_str(), (*rdfi)->confidence);
		//(*rdfi)->lplog(LOG_WHERE,object+u" LLDEBUGQQ ["+transformedChildWord+u"]");

		for (vector <lpwstring>::iterator wli = (*rdfi)->wikipediaLinks.begin(), wliEnd = (*rdfi)->wikipediaLinks.end(); wli != wliEnd; wli++)
		{
			childWord = *wli;
			if (categoryMultiWord(childWord, lastWord))
			{
				if (logQuestionDetail)
					lplog(LOG_WHERE, u"RDFSimplificationToWordMapping MultiWordWikipediaLink %-32s->%s confidence %d", childWord.c_str(), lastWord.c_str(), (*rdfi)->confidence << 1);
				RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[lastWord] = (*rdfi)->confidence << 1;
			}
			RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[transformedChildWord = transformRDFTypeName(childWord)] = (*rdfi)->confidence;
			if (logQuestionDetail)
				lplog(LOG_WHERE, u"RDFSimplificationToWordMapping WikipediaLink %-32s->%s confidence %d", childWord.c_str(), transformedChildWord.c_str(), (*rdfi)->confidence);
		}
		for (vector <lpwstring>::iterator pli = (*rdfi)->professionLinks.begin(), pliEnd = (*rdfi)->professionLinks.end(); pli != pliEnd; pli++)
		{
			childWord = *pli;
			if (categoryMultiWord(childWord, lastWord))
			{
				if (logQuestionDetail)
					lplog(LOG_WHERE, u"RDFSimplificationToWordMapping MultiWordProfessionLink %-32s->%s confidence %d", childWord.c_str(), lastWord.c_str(), (*rdfi)->confidence << 1);
				RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[lastWord] = (*rdfi)->confidence << 1;
			}
			RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap[transformedChildWord = transformRDFTypeName(childWord)] = (*rdfi)->confidence;
			if (logQuestionDetail)
				lplog(LOG_WHERE, u"RDFSimplificationToWordMapping WikipediaLink %-32s->%s confidence %d", childWord.c_str(), transformedChildWord.c_str(), (*rdfi)->confidence);
		}
	}
}

// accumulate RDF types for each object string defined by the source from the word positions (where - where+numWords).
// getExtendedRDFTypesMaster accumulates RDF types in a map per source called extendedRdfTypeMap.  
// the results are extracted from the map and returned in RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.
// in addition, the number of times each object RDFType is asked for is accumulated in extendedRdfTypeNumMap.
int cSource::getAssociationMapMaster(int where, int numWords, unordered_map <lpwstring, int >& RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap, lpwstring fromWhere, bool fileCaching)
{
	LFS
		lpwstring newObjectName;
	bool isObject = m[where].getObject() >= 0;
	if (numWords < 0 && getRDFWhereString(where, newObjectName, u"_", -1) < 0)
		return -1;
	if (numWords >= 1 && !isObject)
		phraseString(where, where + numWords, newObjectName, true);
	if (numWords >= 1 && isObject)
		phraseString(m[where].beginObjectPosition, m[where].beginObjectPosition + numWords, newObjectName, true);
	unordered_map<lpwstring, int >::iterator rdfni;
	if ((rdfni = extendedRdfTypeNumMap.find(newObjectName)) == extendedRdfTypeNumMap.end())
	{
		vector <cTreeCat*> rdfTypes;
		unordered_map <lpwstring, int > topHierarchyClassIndexes;
		// this routine accumulates RDF types in a map per source called extendedRdfTypeMap
		getExtendedRDFTypesMaster(where, numWords, rdfTypes, topHierarchyClassIndexes, fromWhere, -1, fileCaching, false);
		// if there are no rdfTypes from the matched object, perhaps from the original object?
		if (rdfTypes.empty() && m[where].objectMatches.size() > 0)
			// this routine accumulates RDF types in a map per source called extendedRdfTypeMap
			getExtendedRDFTypesMaster(where, -1, rdfTypes, topHierarchyClassIndexes, fromWhere, -1, fileCaching, true);
		//lplog(LOG_WHERE, u"%s results in %d rdfTypes.", newObjectName.c_str(),rdfTypes.size());
	}
	else
		(*rdfni).second++;
	RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap = extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
	//lplog(LOG_WHERE, u"%s results in %d words.", newObjectName.c_str(), extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.size());
	return 0;
}

// Builds the legacy cache key: sourcePath+where+extendNumPP, then strips a dbPediaCache/
// webSearchCache prefix and a leading "m.". Written into object.
void getOldRDFName(lpwstring& sourcePath, int where, int extendNumPP, lpwstring& object)
{
	lpwstring tmp1, tmp2;
	object = sourcePath + itos(where, tmp1) + itos(extendNumPP, tmp2);
	int of = object.find(u"dbPediaCache");
	if (of != lpwstring::npos)
		object.erase(0, of + lp_strlen(u"dbPediaCache") + 6);
	of = object.find(u"webSearchCache");
	if (of != lpwstring::npos)
		object.erase(0, of + lp_strlen(u"webSearchCache") + 6);
	if (object[0] == u'm' && object[1] == u'.' && object.length() > 2)
		object.erase(0, 2);
}

// Builds CACHEDIR\\dbPediaCache\\_<sanitized object>.eRdfTypes into path (4096 assumed).
// Truncates at MAX_PATH-12 and at pathlen+246 to keep the filename legal.
void makePath(lpwstring& object, lpchar_t* path)
{
	int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\dbPediaCache", CACHEDIR);// , retCode = -1;
	if (lp_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	lp_snprintf(path + pathlen, MAX_LEN - pathlen, u"\\_%s", object.c_str());
	convertIllegalChars(path + pathlen + 1);
	distributeToSubDirectories(path, pathlen + 1, true);
	if (lp_strlen(path) + 12 > MAX_PATH)
		cOntology::compressPath(path);
	path[MAX_PATH - 12] = 0;
	path[pathlen + 2 + 244] = 0;// to guard against a filename that is too long
	lp_strcpy((path) + lp_strlen(path), u".eRdfTypes");
}

// Placeholder skip: always false. Intended to reject objects that should not be typed.
// may reject 'his contributions' in the future
bool cSource::noRDFTypes()
{
	//int begin = m[where].beginObjectPosition,end=m[where].endObjectPosition;
	return false;
}

// get all RDF types associated with the object which is defined by the words consecutively from position (where) to position (where+numWords) in source.
// this may be extended by some prepositional phrases as determined by extendNumPP
// return results in rdfTypes and topHierarchyClassIndexes
int cSource::getExtendedRDFTypesMaster(int where, int numWords, vector <cTreeCat*>& rdfTypes, unordered_map <lpwstring, int >& topHierarchyClassIndexes, lpwstring fromWhere, int extendNumPP, bool fileCaching, bool ignoreMatches)
{
	LFS
		if (noRDFTypes())
			return 0;
	cOntology::fillOntologyList(false);
	lpwstring tmp1, tmp2, object, newObjectName;
	if (numWords < 0 && getRDFWhereString(where, newObjectName, u"_", extendNumPP, ignoreMatches) < 0)
		return -1;
	if (numWords >= 1)
		phraseString(m[where].beginObjectPosition, m[where].beginObjectPosition + numWords, newObjectName, true);
	// protects from buffer overrun and also very unlikely that legal names are > 200 in length
	if (newObjectName.length() > 200)
		return -1;
	// cache rdf types for an object.
	unordered_map<lpwstring, int >::iterator rdfni;
	if ((rdfni = extendedRdfTypeNumMap.find(newObjectName)) == extendedRdfTypeNumMap.end() || !cOntology::cacheRdfTypes)
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
	lpchar_t path[4096], newPath[4096];
	makePath(object, path);
	makePath(newObjectName, newPath);
	int retCode = -1, newRetCode = -1;
	if (!fileCaching ||
		(lp_waccess(path, 0) < 0 && lp_waccess(newPath, 0) < 0) ||
		((retCode = readExtendedRDFTypes(path, rdfTypes, topHierarchyClassIndexes)) < 0 && (newRetCode = readExtendedRDFTypes(newPath, rdfTypes, topHierarchyClassIndexes)) < 0))
	{
		// not in memory cache, not cached on disk.  Get all rdf types.
		rdfTypes.clear();
		topHierarchyClassIndexes.clear();
		unordered_map <lpwstring, cOntologyEntry>::iterator dbSeparator = cOntology::dbPediaOntologyCategoryList.find(SEPARATOR);
		getExtendedRDFTypes(where, rdfTypes, topHierarchyClassIndexes, fromWhere, ignoreMatches, fileCaching);
		if (rdfTypes.empty() && numWords > 0)
			cOntology::rdfIdentify(newObjectName.c_str(), rdfTypes, u"O");
		//lplog(LOG_WHERE, u"%s results in %d rdfTypes.", newObjectName.c_str(), rdfTypes.size()); 
		for (unordered_map <lpwstring, int >::iterator tI = topHierarchyClassIndexes.begin(), tIEnd = topHierarchyClassIndexes.end(); tI != tIEnd; tI++)
		{
			if (tI->second < 0 || tI->second >= rdfTypes.size())
				lplog(LOG_WHERE, u"topHierarchyClassIndexes (size %d - rdfType size %d) %d,%s out of bounds!", topHierarchyClassIndexes.size(), rdfTypes.size(), tI->second, tI->first.c_str());
			else
				rdfTypes[tI->second]->top = tI->first;
		}
		// compress RDF types
		vector <cTreeCat*> urs;
		for (int r = 0; r < rdfTypes.size(); r++)
		{
			if (rdfTypes[r]->cli == dbSeparator)
			{
				if (urs.size() > 0 && urs[urs.size() - 1]->cli != dbSeparator)
					urs.push_back(rdfTypes[r]);
				continue;
			}
			bool found = false;
			for (int u = 0; u < urs.size() && !found; u++)
				if (found = (urs[u]->equals(rdfTypes[r])))
				{
					if (rdfTypes[r]->confidence < urs[u]->confidence)
						urs[u]->confidence = rdfTypes[r]->confidence;
					if (rdfTypes[r]->wikipediaLinks.size() > urs[u]->wikipediaLinks.size())
						urs[u]->wikipediaLinks = rdfTypes[r]->wikipediaLinks;
					if (rdfTypes[r]->professionLinks.size() > urs[u]->professionLinks.size())
						urs[u]->professionLinks = rdfTypes[r]->professionLinks;
					if (rdfTypes[r]->preferred)
						urs[u]->preferred = rdfTypes[r]->preferred;
					if (!cOntology::cacheRdfTypes)
						delete rdfTypes[r];
				}
			if (!found)
				urs.push_back(rdfTypes[r]);
		}
		topHierarchyClassIndexes.clear();
		for (int r = 0; r < urs.size(); r++)
			if (urs[r]->top.length())
				topHierarchyClassIndexes[urs[r]->top] = r;
		//	static int totalRdfs=0,uniqueRdfs=0;
		//	totalRdfs+=rdfTypes.size();
		//	uniqueRdfs+=(int)urs.size();
		//	lplog(LOG_INFO,u"getExtendedRDFTypesMaster RDFTypes compression %d %d %d %d",rdfTypes.size(),urs.size(),totalRdfs,uniqueRdfs);
		if (urs.empty())
			cOntology::insertNoERDFTypesDBTable(newObjectName);
		else if (writeExtendedRDFTypes(newPath, urs, topHierarchyClassIndexes) < 0)
			return -1;
		newRetCode = 0;
		rdfTypes = urs;
	}
	extendedRdfTypeMap[newObjectName].rdfTypes = rdfTypes;
	extendedRdfTypeMap[newObjectName].topHierarchyClassIndexes = topHierarchyClassIndexes;
	getRDFTypeSimplificationToWordAssociationWithObjectMap(newObjectName, rdfTypes, extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap);
	//lplog(LOG_WHERE, u"getExtendedRDFTypesMaster %s derived words %d from rdfTypes %d.", newObjectName.c_str(), extendedRdfTypeMap[newObjectName].RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.size(), rdfTypes.size());
	if (retCode >= 0)
		lplog(LOG_WHERE, u"getExtendedRDFTypesMaster used path %s for object %s.", path, object.c_str());
	//	if (newRetCode>=0)
	//		lplog(LOG_WHERE, u"getExtendedRDFTypesMaster used [new] path %s for object %s.", newPath, newObjectName.c_str());
	if (!lp_waccess(path, 0))
	{
		if (lp_wremove(newPath) < 0)
			lplog(LOG_ERROR, u"REMOVE %s - %S", newPath, sys_errlist[errno]);
		else if (lp_wrename(path, newPath))
			lplog(LOG_ERROR, u"RENAME %s to %s - %S", path, newPath, sys_errlist[errno]);
		else
			lplog(LOG_WHERE, u"getExtendedRDFTypesMaster moved path %s to [new] path %s.", path, newPath);
	}
	return retCode;
}

// Builds an underscored Wikipedia search string for the object at 'where' (first/last name
// if a 1-token named object). Optionally drops leading uncapitalized tokens and appends
// attached PPs. Proper-noun tokens are also pushed onto lookForSubject for ISA matching.
void cSource::getObjectString(int where, lpwstring& object, vector <lpwstring>& lookForSubject, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool removePrecedingUncapitalizedWordsFromProperNouns)
{
	LFS
		int o, begin, end;
	if (m[where].objectMatches.size() > 0)
	{
		o = m[where].objectMatches[0].object;
		begin = m[objects[o].originalLocation].beginObjectPosition;
		end = m[objects[o].originalLocation].endObjectPosition;
	}
	else
	{
		o = m[where].getObject();
		begin = m[where].beginObjectPosition;
		end = m[where].endObjectPosition;
	}
	if (end - begin == 1 && (objects[o].objectClass == NAME_OBJECT_CLASS || objects[o].objectClass == NON_GENDERED_NAME_OBJECT_CLASS) && objects[o].name.first != wNULL && objects[o].name.last != wNULL)
	{
		object = objects[o].name.first->first + u"_";
		object[0] = towupper(object[0]);
		int lastchpos = object.length();
		object += objects[o].name.last->first;
		object[lastchpos] = towupper(object[lastchpos]);
		return;
	}
	if (removePrecedingUncapitalizedWordsFromProperNouns)
	{
		// get the last uncapitalized word.
		int lastUncapitalizedWord = -2;
		for (int I = begin; I < end; I++)
			if (m[I].flags & (cWordMatch::flagAddProperNoun | cWordMatch::flagOnlyConsiderProperNounForms | cWordMatch::flagFirstLetterCapitalized | cWordMatch::flagNounOwner))
			{
				lastUncapitalizedWord = I - 1;
				break;
			}
		// if all uncapitalized words, or all capitalized words, return
		if (lastUncapitalizedWord == -2 || lastUncapitalizedWord == end - 2)
			return;
		begin = lastUncapitalizedWord + 1;
	}
	for (int I = begin; I < end; I++)
	{
		int len = object.length();
		object += m[I].word->first;
		if (m[I].flags & (cWordMatch::flagAddProperNoun | cWordMatch::flagOnlyConsiderProperNounForms | cWordMatch::flagFirstLetterCapitalized))
			object[len] = towupper(object[len]);
		if (m[I].flags & (cWordMatch::flagNounOwner))
		{
			object += '\'';
			if (object[object.length() - 2] != 's') object += 's';
		}
		if (m[I].flags & cWordMatch::flagAllCaps)
			for (unsigned int J = len; object[J]; J++) object[J] = towupper(object[J]);
		if (I < end - 1)
			object += u"_";
		if (m[I].forms.isSet(PROPER_NOUN_FORM_NUM))
		{
			if (m[I].word->second.mainEntry != wNULL)
				lookForSubject.push_back(m[I].word->second.mainEntry->first);
			else
				lookForSubject.push_back(m[I].word->first);
		}
	}
	if (includeNonMixedCaseDirectlyAttachedPrepositionalPhrases > 0)
	{
		vector <lpwstring> prepPhraseStrings;
		lpwstring logres = object;
		int numWords;
		appendPrepositionalPhrases(where, object, prepPhraseStrings, numWords, true, u"_", includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
		if (prepPhraseStrings.size() > 0)
			object = prepPhraseStrings[0];
	}
}

// Decodes $XXXX hex escapes in a DBpedia/Wikipedia link (e.g. $0028 → '('). The I+1..I+4
// lookahead is bounds-safe: each hex-digit check short-circuits the && chain before a later
// index is read unless the earlier one was confirmed to be a real character, and
// lpwstring guarantees a read (or a write of u'\0') at wikilink[wikilink.size()] is defined.
// Curveball_$0028informant$0029$002FArchive1
// Curveball_$0028informant$0029
void convertFromWikilinkEscape(lpwstring& wikilink)
{
	LFS
		lpwstring wl;
	for (unsigned int I = 0; I < wikilink.length(); I++)
		if (wikilink[I] == u'$' &&
			(iswdigit(wikilink[I + 1]) || (wikilink[I + 1] >= 'A' && wikilink[I + 1] <= 'F')) &&
			(iswdigit(wikilink[I + 2]) || (wikilink[I + 2] >= 'A' && wikilink[I + 2] <= 'F')) &&
			(iswdigit(wikilink[I + 3]) || (wikilink[I + 3] >= 'A' && wikilink[I + 3] <= 'F')) &&
			(iswdigit(wikilink[I + 4]) || (wikilink[I + 4] >= 'A' && wikilink[I + 4] <= 'F')))
		{
			lpchar_t save = wikilink[I + 5];
			wikilink[I + 5] = 0;
			lpchar_t ch = (lpchar_t)lp_wcstol(wikilink.c_str() + I + 1, 0, 16);
			wikilink[I + 5] = save;
			wl += ch;
			I += 4;
		}
		else
			wl += wikilink[I];
	wikilink = wl;
}

// this is called with the understanding that removePrecedingUncapitalizedWordsFromProperNouns is set to false, then to true in a subsequent call, so that if there are no preceding uncapitalized words, nothing is done.
int cSource::getWikipediaPath(int principalWhere, vector <lpwstring>& wikipediaLinks, lpchar_t* path, vector <lpwstring>& lookForSubject, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool removePrecedingUncapitalizedWordsFromProperNouns)
{
	LFS
		lpwstring object;
	if (principalWhere >= 0)
		getObjectString(principalWhere, object, lookForSubject, includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, removePrecedingUncapitalizedWordsFromProperNouns);
	else if (wikipediaLinks.size() > 0)
	{
		lpwstring wl = wikipediaLinks[0];
		convertFromWikilinkEscape(wl);
		if (wl.size() > 0)
		{
			lplog(LOG_WHERE, u"%d:Substituted wikilink %s for object %s.", principalWhere, wl.c_str(), object.c_str());
			object = wl;
		}
	}
	if (object.empty())
		return -1;
	// remove ownership if the last word (Curveball's) should also check for adjectival object
	if (principalWhere >= 0 && (m[principalWhere].flags & cWordMatch::flagAdjectivalObject) && object.length() > 2 && object[object.length() - 1] == u's' && object[object.length() - 2] == u'\'')
		object.erase(object.length() - 2);
	int pathlen = lp_snprintf(path, MAX_LEN, u"%s\\wikipediaCache", CACHEDIR) + 1;
	if (lp_wmkdir(path) < 0 && errno == ENOENT)
		lplog(LOG_FATAL_ERROR, u"Cannot create directory %s.", path);
	lp_snprintf(path, MAX_LEN, u"%s\\wikipediaCache\\_%s.txt", CACHEDIR, object.c_str());
	convertIllegalChars(path + pathlen);
	distributeToSubDirectories(path, pathlen, true);
	path[MAX_PATH - 16] = 0; // give room for extensions
	if (logTraceOpen)
		lplog(LOG_WHERE, u"TRACEOPEN %s %s", path, LP_TEXT(__func__).c_str());
	if (lp_waccess(path, 0) < 0)
	{
		lpchar_t webAddress[MAX_LEN];
		lpwstring uobject;
		encodeURL(object, uobject); // object is built from parsed words and may contain '&', apostrophes, etc; escape before interpolating into the query string
		lp_snprintf(webAddress, MAX_LEN, u"https://en.wikipedia.org/wiki/Special:Search?search=%s&printable=yes&redirect=no", uobject.c_str());
		lplog(LOG_WIKIPEDIA, u"PRIMARY:  %s", webAddress);
		int ret;
		lpwstring buffer;
		if (ret = cInternet::readPage(webAddress, buffer)) return ret;
		reduceWikipediaPage(buffer);
		int fd = lp_wopen(path, O_CREAT | O_RDWR | O_BINARY, _S_IREAD | _S_IWRITE);
		if (fd < 0)
		{
			lplog(LOG_ERROR, u"ERROR:Cannot create path %s - %S (6).", path, sys_errlist[errno]);
			return cInternet::GETPAGE_CANNOT_CREATE;
		}
		::write(fd, buffer.c_str(), buffer.length() * sizeof(buffer[0]));
		::close(fd);
	}
	return 0;
}





// True if the span has both a non-capitalized alpha token and an ALL-CAPS token that contains
// a vowel (e.g. PLAYLISTBYAOL_On_Valentine's) — used to reject ISA lookup.
bool cSource::mixedCaseObject(int begin, int len)
{
	LFS
		bool lowerCaseFound = false, allUpperCaseWithVowelsFound = false;
	for (int I = begin; I < begin + len; I++)
	{
		if (iswalpha(m[I].word->first[0]))
		{
			lowerCaseFound |= !(m[I].flags & (cWordMatch::flagAllCaps | cWordMatch::flagFirstLetterCapitalized));
			if (m[I].flags & (cWordMatch::flagAllCaps))
			{
				for (unsigned int w = 0; w < m[I].word->first.length() && !allUpperCaseWithVowelsFound; w++)
				{
					lpchar_t wc = towlower(m[I].word->first[w]);
					if (wc == u'a' || wc == u'e' || wc == u'i' || wc == u'o' || wc == u'u')
						allUpperCaseWithVowelsFound = true;
				}
			}
		}
	}
	if (lowerCaseFound && allUpperCaseWithVowelsFound)
	{
		lpwstring tmpstr;
		for (int I = begin; I < begin + len; I++)
			phraseString(begin, begin + len, tmpstr, true, u" ");
	}
	return lowerCaseFound && allUpperCaseWithVowelsFound;
}

// True if the span should skip web ISA: 1-word uncapitalized, DET+uncap, or < half capitalized.
// return true if object SHOULD NOT be identified through extensive web methods
bool cSource::capitalizationCheck(int begin, int len)
{
	// count # of capitalized words
	int wordsCapitalized = 0;
	for (int I = begin; I < begin + len; I++)
	{
		if (m[I].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m[I].flags & cWordMatch::flagFirstLetterCapitalized) || (m[I].flags & cWordMatch::flagAllCaps))
			wordsCapitalized++;
	}
	bool firstWordCapitalized = (m[begin].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m[begin].flags & cWordMatch::flagFirstLetterCapitalized) || (m[begin].flags & cWordMatch::flagAllCaps));
	if ((len == 1 && !firstWordCapitalized) ||
		// only word capitalized is the determiner
		(len == 2 && wordsCapitalized == 1 && firstWordCapitalized && m[begin].queryForm(determinerForm) >= 0) ||
		// more than half the words are not capitalized
		(len > 2 && wordsCapitalized < len / 2))
	{
		lpwstring tmpstr;
		if (logQuestionDetail)
			lplog(LOG_ERROR, u"capitalizationCheck rejected %s", phraseString(begin, begin + len, tmpstr, true).c_str());
		return true;
	}
	return false;
}

// True if the object at principalWhere should not be Wikipedia-typed (not a name, already
// accessed, speaker, time unit, mixed case, possessive, trailing colon, or a letter).
bool cSource::rejectISARelation(int principalWhere)
{
	LFS
		int o = m[principalWhere].getObject(), begin = m[principalWhere].beginObjectPosition, len = m[principalWhere].endObjectPosition - begin;
	bool tmp1 = false, tmp2 = false, tmp3 = false, tmp4 = false, tmp5 = false, tmp6 = false, tmp7 = false, tmp8 = false, tmp9 = false, tmp10 = false, tmp11 = false; // for use with debugging
	return ((tmp1 = o < 0) || //  || (objects[o].getSubType()>=0 && objects[o].getSubType()!=UNKNOWN_PLACE_SUBTYPE)
			// only names
		(tmp2 = (objects[o].objectClass != NAME_OBJECT_CLASS && objects[o].objectClass != NON_GENDERED_NAME_OBJECT_CLASS)) ||
		// no capitalization other than the first word (and more than 1 word in length) OR mostly not capitalized
		(tmp11 = capitalizationCheck(begin, len)) ||
		// no matches, look up ISA once per object, and no honorific (sir, Lieutenant, etc, or Miss Tuppence)
		(tmp3 = m[principalWhere].objectMatches.size() || objects[o].wikipediaAccessed || objects[o].name.hon != wNULL) ||
		// no speakers, no relativizers (because they get the object they represent set on them)
		(tmp4 = objects[o].numIdentifiedAsSpeaker > 0 || objects[o].PISHail > 0 || objects[o].PISDefinite > 0 || m[principalWhere].queryWinnerForm(relativizerForm) >= 0) ||
		// Tuesday, March
		(tmp5 = (len == 1 && (m[principalWhere].word->second.timeFlags & T_UNIT))) ||
		// STORIES_Tina_Fey_Announces / PLAYLISTBYAOL_On_Valentine's day
		(tmp8 = mixedCaseObject(begin, len)) ||
		// his_new_memoir / her_hit_Torn / your_Horoscope -- may alternatively cut the possessives off
		(tmp9 = m[begin].queryForm(possessiveDeterminerForm) >= 0 && !(m[begin].flags & (cWordMatch::flagAllCaps | cWordMatch::flagFirstLetterCapitalized))) ||
		// Subway_Hero_Describes_Saving_Man_From_Tracks_:
		(tmp10 = m[begin + len - 1].word->first == u":") ||
		// last Friday
		// a Friday
		(tmp6 = (len == 2 && (m[principalWhere].word->second.timeFlags & T_UNIT) &&
			(m[begin].queryWinnerForm(determinerForm) >= 0 || m[begin].queryWinnerForm(numeralOrdinalForm) >= 0 || m[begin].queryWinnerForm(adjectiveForm) >= 0))) ||
		// A, C
		(tmp7 = (len == 1 && (m[principalWhere].queryForm(letterForm) >= 0)))); // for fictional books only!
	/*
	lpwstring tmpstr;
	// Tuesday, March
	if ((objectWordLength==1 && (m[principalWhere].word->second.timeFlags&T_UNIT)) ||
			// last Friday
			// a Friday
			(objectWordLength==2 && (m[principalWhere].word->second.timeFlags&T_UNIT) &&
			 (m[begin].queryWinnerForm(determinerForm)>=0 || m[begin].queryWinnerForm(numeralOrdinalForm)>=0 || m[begin].queryWinnerForm(adjectiveForm)>=0)))
		lplog(LOG_RESOLUTION,u"%06d:TR new time %s",principalWhere,objectString(o,tmpstr,false).c_str());
	*/
}

// True if path is <10 bytes or looks like .pdf/.php (skip as an ISA child source).
bool cQuestionAnswering::rejectPath(const lpchar_t* path)
{
	struct stat buf;
	lp_wstat(path, &buf);
	return (buf.st_size < 10 || lp_strstr(path, u".pdf") != NULL || lp_strstr(path, u".php") != NULL); // Nobel Prize abstract is 2 bytes / also don't bother with pdf or php files for now
}

// Parses (or reuses from sourcesMap) the Wikipedia/web page at path as a child cSource.
// Returns 0 if source is usable, -1 if rejectPath or tokenize produced an empty stream
// (writes an empty .SourceCache marker). On any -1 return after 'source' was allocated
// here, it is deleted and set to NULL before returning, so callers must check the return
// value before dereferencing source (rejectPath's -1 leaves source untouched).
int limitProcessingForProfiling = 0;
int cQuestionAnswering::processPath(cSource* parentSource, const lpchar_t* path, cSource*& source, cSource::sourceTypeEnum st, int pathSourceConfidence, bool parseOnly)
{
	LFS
		if (logTraceOpen)
			lplog(LOG_WHERE, u"TRACEOPEN %s %s", path, LP_TEXT(__func__).c_str());
	unordered_map <lpwstring, cSource*>::iterator smi = sourcesMap.find(path);
	lpchar_t sourcesParsedTitle[1024];
	(sourcesParsedTitle)[0] = 0;
	lpchar_t* ch = lp_strstr(sourcesParsedTitle, u"...");
	extern int questionProgress;
	if (smi != sourcesMap.end())
	{
		source = smi->second;
		source->numSearchedInMemory++;
		if (ch)
		{
			lp_snprintf(ch + 3, 1024 - (size_t)(ch + 3 - sourcesParsedTitle), u"[%d%%] %I64u sources processed:%s[%d]", questionProgress, sourcesMap.size(), path, source->numSearchedInMemory); // batch B10: bounded by what is left of sourcesParsedTitle
			lpReportProgress(sourcesParsedTitle);
		}
		return 0;
	}
	if (rejectPath(path))
		return -1;
	if (ch)
	{
		lp_snprintf(ch + 3, 1024 - (size_t)(ch + 3 - sourcesParsedTitle), u"[%d%%]  %I64u sources processed:%s", questionProgress, sourcesMap.size(), path);
		lpReportProgress(sourcesParsedTitle);
	}
	source = new cSource(&parentSource->mysql, st, pathSourceConfidence);
	source->numSearchedInMemory = 1;
	source->isFormsProcessed = false;
	source->processOrder = ++parentSource->processOrder;
	source->multiWordStrings = parentSource->multiWordStrings;
	source->multiWordObjects = parentSource->multiWordObjects;
	lpwstring wpath = path, start = u"~~BEGIN";
	int repeatStart = 1;
	bool justParsed = false;
	Words.readWords(wpath, -1, false, u"");
	if (!source->readSource(wpath, false, justParsed, false, u"") || (justParsed && !parseOnly))
	{
		lplog(LOG_WIKIPEDIA | LOG_RESOLUTION | LOG_RESCHECK | LOG_WHERE, u"Begin Processing %s...", path);
		if (!justParsed)
		{
			lp_wprintf(u"\nParsing %s...\n", path);
			unsigned int unknownCount = 0, quotationExceptions = 0, totalQuotations = 0;
			int globalOverMatchedPositionsTotal = 0;
			string cPath;
			wTM(path, cPath);
			source->tokenize(u"", u"", wpath, u"", start, repeatStart, unknownCount);
			source->doQuotesOwnershipAndContractions(totalQuotations);
			unlockTables(parentSource->mysql);
			if (source->m.empty())
			{
				lpwstring failurePath = path;
				failurePath += u".SourceCache";
				int fd;
				fd = lp_wopen(failurePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE); // , errorCode =
				if (fd >= 0)
					close(fd);
				delete source;
				source = NULL;
				return -1;
			}
			bool s1 = logMatchedSentences, s2 = logUnmatchedSentences;
			logMatchedSentences = logUnmatchedSentences = true;
			source->printSentences(false, unknownCount, quotationExceptions, totalQuotations, globalOverMatchedPositionsTotal);
			if (source->m.size())
			{
				source->identifyObjects();
				source->analyzeWordSenses();
				source->narrativeIsQuoted = source->sourceType != cSource::GUTENBERG_SOURCE_TYPE;
				source->syntacticRelations();
			}
			logMatchedSentences = s1;
			logUnmatchedSentences = s2;
			lplog();
			source->write(path, false, false, u"");
			source->writeWords(path, u"");
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
			lpwstring failurePath = path;
			failurePath += u".SourceCache";
			int fd = lp_wopen(failurePath.c_str(), O_CREAT);
			if (fd >= 0)
				close(fd);
			delete source;
			source = NULL;
			return -1;
		}
		if (!parseOnly)
		{
			source->parentSource = parentSource;
			vector <int> secondaryQuotesResolutions;
			//Words.writeWords(path);  not necessary and wastes huge amount of space
			source->identifySpeakerGroups();
			source->resolveSpeakers(secondaryQuotesResolutions);
			source->resolveFirstSecondPersonPronouns(secondaryQuotesResolutions);
		}
		source->write(path, !parseOnly, false, u"");
		source->writeWords(path, u"");
		lplog(LOG_WIKIPEDIA | LOG_RESOLUTION | LOG_RESCHECK | LOG_WHERE, u"End Processing %s...", path);
	}
	sourcesMap[path] = source;
	return 0;
}


// Builds the DBpedia lookup string for the object at 'where' (First_Last or original words
// joined by separator). Optionally requires attached PPs (returns -1 if none). Strips a
// trailing 's. Returns -1 if there is no object/span.
int cSource::getRDFWhereString(int where, lpwstring& oStr, const lpchar_t* separator, int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases, bool ignoreMatches)
{
	LFS
		int o, begin, len;
	if (m[where].objectMatches.size() > 0 && !ignoreMatches)
	{
		o = m[where].objectMatches[0].object;
		begin = m[objects[o].originalLocation].beginObjectPosition;
		len = m[objects[o].originalLocation].endObjectPosition - begin;
	}
	else
	{
		o = m[where].getObject();
		begin = m[where].beginObjectPosition;
		len = m[where].endObjectPosition - begin;
	}
	if (o < 0 || begin < 0 || len < 0)
		return -1;
	if (len == 1 && (objects[o].objectClass == NAME_OBJECT_CLASS || objects[o].objectClass == NON_GENDERED_NAME_OBJECT_CLASS) && objects[o].name.first != wNULL && objects[o].name.last != wNULL)
	{
		oStr = objects[o].name.first->first;
		oStr[0] = towupper(oStr[0]);
		oStr += separator;
		int lastchpos = oStr.length();
		oStr += objects[o].name.last->first;
		oStr[lastchpos] = towupper(oStr[lastchpos]);
	}
	else
	{
		getOriginalWord(begin, oStr, false);
		if (oStr == u"the")
			oStr = u"The"; // The New York Times
		for (int I = begin + 1; I < begin + len; I++)
		{
			oStr += separator;
			getOriginalWord(I, oStr, true);
		}
	}
	//lplog(LOG_WHERE, u"getRDFWhereString yields %s before includeNonMixedCaseDirectlyAttachedPrepositionalPhrases=%d", oStr.c_str(), includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
	if (includeNonMixedCaseDirectlyAttachedPrepositionalPhrases > 0)
	{
		vector <lpwstring> prepPhraseStrings;
		lpwstring logres = oStr;
		int numWords;
		appendPrepositionalPhrases(where, logres, prepPhraseStrings, numWords, true, separator, includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
		if (prepPhraseStrings.empty())
			return -1;
		oStr = prepPhraseStrings[0];
	}
	//lplog(LOG_WHERE, u"getRDFWhereString yields %s after includeNonMixedCaseDirectlyAttachedPrepositionalPhrases=%d", oStr.c_str(), includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
	// remove ownership (Curveball's)
	if (oStr.length() > 2 && oStr[oStr.length() - 1] == u's' && oStr[oStr.length() - 2] == u'\'')
		oStr.erase(oStr.length() - 2);
	return 0;
}

// Measures a title-like capitalized span starting at where (stop at EOS / TABLE / bracket).
// Writes numWords and numPrepositions; trims a trailing comma-clause. Returns false only if
// the span collapsed to empty (numWords forced to 1).
bool cSource::analyzeRDFTitle(unsigned int where, int& numWords, int& numPrepositions, lpwstring tableName)
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
		(m[I].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m[I].flags & cWordMatch::flagFirstLetterCapitalized) || (m[I].flags & cWordMatch::flagAllCaps) ||
			m[I].word->first[0] == u',' || m[I].word->first[0] == u':' || m[I].word->first[0] == u';' || m[I].queryForm(determinerForm) >= 0 || m[I].queryForm(numeralOrdinalForm) >= 0 || m[I].queryForm(prepositionForm) >= 0 || m[I].queryForm(coordinatorForm) >= 0);
		I++, numWords++)
	{
		if (m[I].queryForm(prepositionForm) >= 0)
			numPrepositions++;
		if (m[I].getObject() > 0)
		{
			numWords += (m[I].endObjectPosition - 1 - I);
			I = m[I].endObjectPosition - 1; // skip objects and periods associated with abbreviations and names
		}
		if (m[I].word->first[0] == u',')
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
			lpwstring tmpstr, tmpstr2;
			lplog(LOG_WHERE, u"Processing table %s: %d:title %s rejected words after comma resulting in %s", tableName.c_str(), where, phraseString(where, where + numWords, tmpstr, true, u" ").c_str(), phraseString(where, firstComma, tmpstr2, true, u" ").c_str());
		}
		numWords = firstComma - where;
	}
	while (numWords > 1 && (m[where + numWords - 1].word->first[0] == u',' || m[where + numWords - 1].queryForm(determinerForm) >= 0 || m[where + numWords - 1].queryForm(prepositionForm) >= 0 || m[where + numWords - 1].queryForm(coordinatorForm) >= 0))
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

// rdfIdentify on several fallbacks of the object string at 'where' (title expansion, strip
// a_/the_, profession+name, colon, "3 M", drop adjectival owner). Returns -1 on a missing
// begin or a >512-char string; 0 otherwise (rdfTypes may still be empty).
// rdfTypes calls rdfIdentify, which then caches the rdfTypes of each individual string into the dbpedia cache.
int cSource::getRDFTypes(int where, vector <cTreeCat*>& rdfTypes, lpwstring fromWhere, int extendNumPP, bool ignoreMatches, bool fileCaching)
{
	LFS
		int o, begin, objectWordLength, pw;
	if (m[where].objectMatches.size() > 0 && !ignoreMatches)
	{
		o = m[where].objectMatches[0].object;
		begin = m[pw = objects[o].originalLocation].beginObjectPosition;
		objectWordLength = m[pw].endObjectPosition - begin;
	}
	else
	{
		o = m[pw = where].getObject();
		begin = m[where].beginObjectPosition;
		objectWordLength = m[where].endObjectPosition - begin;
	}
	int oLen = objects[o].len();
	// if where=Krugman matches o=Paul Krugman, take the longer match
	if (oLen > objectWordLength)
	{
		pw = objects[o].originalLocation;
		objectWordLength = oLen;
		begin = objects[o].begin;
	}
	if (begin < 0)
	{
		lplog(LOG_WHERE | LOG_ERROR, u"%06d:%S getRDFTypes handed an object with no begin!", where, fromWhere.c_str());
		return -1;
	}
	lpwstring oStr;
	int numWords, numPrepositions = 0;
	getRDFWhereString(where, oStr, u"_", extendNumPP, ignoreMatches);
	if (oStr.length() > 512)
	{
		lplog(LOG_WHERE | LOG_ERROR, u"%06d:%S getRDFTypes handed a very long object (%s)!", where, fromWhere.c_str(), oStr.c_str());
		return -1;
	}
	lpwstring saveStr = oStr;
	// specifically if ignoreMatches is true, we must obey the number of prepositions allowed, which will not be true with analyzeTitle.
	// even though this should never be necessary because if ignoreMatches is true, then objectMatches will not be empty.
	if (m[where].objectMatches.empty() && !ignoreMatches)
	{
		analyzeRDFTitle(begin, numWords, numPrepositions, u"");
		if (m[where].endObjectPosition - m[where].beginObjectPosition > numWords)
			numWords = m[where].endObjectPosition - m[where].beginObjectPosition;
		lpwstring tmpstr;
		phraseString(begin, begin + numWords, tmpstr, true, u"_");
		// remove ownership (Curveball's)
		if (tmpstr.length() > 2 && tmpstr[tmpstr.length() - 1] == u's' && tmpstr[tmpstr.length() - 2] == u'\'')
			tmpstr.erase(tmpstr.length() - 2);
		if (tmpstr.size() > oStr.length())
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE, u"%d:replaced %s with %s", where, oStr.c_str(), tmpstr.c_str());
			oStr = tmpstr;
		}
	}
	cOntology::rdfIdentify(oStr, rdfTypes, u"1", fileCaching);
	if (numPrepositions > extendNumPP && rdfTypes.empty())
	{
		cOntology::rdfIdentify(saveStr.c_str(), rdfTypes, u"a", fileCaching);
		if (!rdfTypes.empty())
			oStr = saveStr;
	}
	// a Nobel prize
	if (rdfTypes.empty() && (!lp_strncmp(oStr.c_str(), u"a_", 2) || !lp_strncmp(oStr.c_str(), u"the_", 4)))
	{
		oStr = u"The"; // The Nobel Prize
		for (int I = begin + 1; I < begin + objectWordLength; I++)
		{
			oStr += u"_";
			int len = oStr.length();
			getOriginalWord(I, oStr, true);
			oStr[len] = towupper(oStr[len]);
		}
		cOntology::rdfIdentify(oStr, rdfTypes, u"2", fileCaching);
	}
	// The Humanist Antonio+de+Nebrija
	if (rdfTypes.empty() && !lp_strncmp(oStr.c_str(), u"The_", 4))
	{
		if (m[begin + 1].queryForm(commonProfessionForm) >= 0 && objectWordLength > 3)
		{
			getOriginalWord(begin + 2, oStr, false);
			for (int I = begin + 3; I < begin + objectWordLength; I++)
			{
				oStr += u"_";
				int len = oStr.length();
				getOriginalWord(I, oStr, true);
				oStr[len] = towupper(oStr[len]);
			}
			cOntology::rdfIdentify(oStr, rdfTypes, u"3", fileCaching);
		}
	}
	int colon;
	if (rdfTypes.empty() && (colon = oStr.find(u':')) != lpwstring::npos)
	{
		if (colon && oStr[colon - 1] == u'_')
			oStr.erase(colon - 1);
		cOntology::rdfIdentify(oStr, rdfTypes, u"4", fileCaching);
	}
	int lowestConfidence = CONFIDENCE_NOMATCH;
	for (unsigned int I = 0; I < rdfTypes.size(); I++)
		if (rdfTypes[I]->cli->first != SEPARATOR)
			lowestConfidence = min(lowestConfidence, rdfTypes[I]->confidence);
	if (lowestConfidence > 1 && (!lp_strncmp(oStr.c_str(), u"The_", 4) || !lp_strncmp(oStr.c_str(), u"the_", 4)))
	{
		oStr.erase(0, 4);
		cOntology::rdfIdentify(oStr, rdfTypes, u"5", fileCaching);
	}
	// 3 M
	if (rdfTypes.empty() && objectWordLength == 2 && m[begin].queryWinnerForm(NUMBER_FORM_NUM) >= 0 && m[begin + 1].word->first.length() == 1)
	{
		getOriginalWord(begin, oStr, false);
		getOriginalWord(begin + 1, oStr, true);
		cOntology::rdfIdentify(oStr, rdfTypes, u"6", fileCaching);
	}
	// Rihanna [6][name][M][F][A:Rihanna ] record compactLabel [PO (Rihanna's record compactLabel Def Jam[6-11]{OWNER:Rihanna }[9][ngname][N][OGEN]) def].
	// strip out begin adjectival object
	// noun that is left must have at least two words
	for (int I = begin; I < begin + objectWordLength - 2 && rdfTypes.empty(); I++)
		if ((m[I].flags & cWordMatch::flagAdjectivalObject) && m[I].endObjectPosition <= begin + objectWordLength - 2)
		{
			lpwstring tmpstr;
			if (m[I].endObjectPosition >= 0)
				phraseString(m[I].endObjectPosition, begin + objectWordLength, tmpstr, true, u"_");
			else
				phraseString(I + 1, begin + objectWordLength, tmpstr, true, u"_");
			cOntology::rdfIdentify(tmpstr, rdfTypes, u"7", fileCaching);
		}
		else if (!(m[I].flags & cWordMatch::flagFirstLetterCapitalized) && (m[I + 1].flags & cWordMatch::flagFirstLetterCapitalized) && I < begin + objectWordLength + 2)
		{
			lpwstring tmpstr;
			phraseString(I + 1, begin + objectWordLength, tmpstr, true, u"_");
			cOntology::rdfIdentify(tmpstr, rdfTypes, u"8", fileCaching);
		}
	return 0;
}

// Types the named object at principalWhere from DBpedia RDF (getExtendedRDFTypesMaster).
// Speakers/honorifics are immediately marked isWikiPerson / not-a-place. Returns -1 if
// rejectISARelation; 0 after setting isWikiPerson/Place/Business/Work from preferred tops.
int cSource::identifyISARelation(int principalWhere, bool initialTenseOnly, bool fileCaching)
{
	LFS
		int o = m[principalWhere].getObject(), begin = m[principalWhere].beginObjectPosition, len = m[principalWhere].endObjectPosition - begin;
	// save time calling getExtendedRDFTypesMaster - we are pretty sure this is a person
	if (o >= 0 && ((objects[o].name.hon != wNULL && !objects[o].name.justHonorific()) || objects[o].numIdentifiedAsSpeaker > 0 || objects[o].PISHail > 0 || objects[o].PISDefinite > 0))
	{
		objects[o].isNotAPlace = true;
		objects[o].resetSubType();
		objects[o].isWikiPerson = true;
		return 0;
	}
	// must be a name
	if (rejectISARelation(principalWhere) || o < 0)
	{
		if (o >= 0 && logQuestionDetail && debugTrace.traceRelations)
		{
			lpwstring tmpstr;
			lplog(LOG_INFO, u"ISARelation declined:%s", objectString(o, tmpstr, false).c_str());
		}
		return -1;
	}
	if (objects[o].dbPediaAccessed) return 0;
	objects[o].dbPediaAccessed = true;
	vector <cTreeCat*> rdfTypes;
	unordered_map <lpwstring, int > topHierarchyClassIndexes;
	getExtendedRDFTypesMaster(principalWhere, -1, rdfTypes, topHierarchyClassIndexes, LP_TEXT(__func__), -1, fileCaching);
	if (len == 1 && !(m[principalWhere].flags & cWordMatch::flagFirstLetterCapitalized)) // lower case 'prize'
	{
		for (unsigned int r = 0; r < rdfTypes.size(); r++)
			rdfTypes[r]->top.clear();
		//remove person or business or organization because these are always upper case.  This avoids 'prize' being associated with a person.
		for (unordered_map <lpwstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
			rdfTypes[idi->second]->top = u"top";
		topHierarchyClassIndexes.clear();
		for (unsigned int r = 0; r < rdfTypes.size();)
			if (rdfTypes[r]->cli->first == u"person" || rdfTypes[r]->cli->first == u"business" || rdfTypes[r]->cli->first == u"organization")
				rdfTypes.erase(rdfTypes.begin() + r);
			else
			{
				if (rdfTypes[r]->top.length())
					topHierarchyClassIndexes[rdfTypes[r]->cli->first] = r;
				r++;
			}
	}
	if (logQuestionDetail)
	{
		lpwstring rdfInfoPrinted;
		for (unsigned int r = 0; r < rdfTypes.size(); r++)
			if ((rdfTypes[r]->preferred || rdfTypes[r]->preferredUnknownClass || rdfTypes[r]->exactMatch))
				rdfTypes[r]->logIdentity(LOG_WHERE, u"ISARelation", false, rdfInfoPrinted);
	}
	int p = 100;
	for (unordered_map <lpwstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
		p = min(p, rdfTypes[idi->second]->confidence);
	int numPreferred = 0;
	bool isPerson = false, isPlace = false, isBusiness = false, isWork = false;
	int placeTypeFound = -1;
	lpwstring rdfInfoPrinted;
	for (unordered_map <lpwstring, int >::iterator idi = topHierarchyClassIndexes.begin(), idiEnd = topHierarchyClassIndexes.end(); idi != idiEnd; idi++)
	{
		if (logQuestionDetail)
			rdfTypes[idi->second]->logIdentity(LOG_WHERE, u"ISARelationIdOffset", false, rdfInfoPrinted);
		lpwstring tmpstr;
		if (logQuestionDetail)
			lplog(LOG_WHERE, u"ISADEBUG %s:%s:%d:%s", objectString(o, tmpstr, false).c_str(), rdfTypes[idi->second]->cli->first.c_str(), rdfTypes[idi->second]->confidence, idi->first.c_str());
		if (rdfTypes[idi->second]->confidence == p)
		{
			isPerson |= (idi->first == u"person");
			isPlace |= (idi->first == u"place") || (idi->first == u"location");
			isBusiness |= (idi->first == u"business") || (idi->first == u"organization");
			isWork |= (idi->first == u"work") || (idi->first == u"writtenwork") || (idi->first == u"creativeWork") || (idi->first == u"album") || (idi->first == u"periodical");
			if (idi->first == u"provincesandterritoriesofcanada")
				placeTypeFound = CANADIAN_PROVINCE_CITY;
			else if (idi->first == u"country")
				placeTypeFound = COUNTRY;
			else if (idi->first == u"island")
				placeTypeFound = ISLAND;
			else if (idi->first == u"mountain")
				placeTypeFound = MOUNTAIN_RANGE_PEAK_LANDFORM;
			else if (idi->first == u"geoclasspark")
				placeTypeFound = PARK_MONUMENT;
			else if (idi->first == u"river")
				placeTypeFound = RIVER_LAKE_WATERWAY;
			else if (idi->first == u"city")
				placeTypeFound = WORLD_CITY_TOWN_VILLAGE;
			else if (idi->first == u"statesoftheunitedstates")
				placeTypeFound = US_STATE_TERRITORY_REGION;
			else if (idi->first == u"state")
				placeTypeFound = US_STATE_TERRITORY_REGION;
			numPreferred++;
		}
	}
	if (!cOntology::cacheRdfTypes)
		for (vector <cTreeCat*>::iterator rdfi = rdfTypes.begin(), rdfiEnd = rdfTypes.end(); rdfi != rdfiEnd; rdfi++)
			delete (*rdfi); // now caching them
	isPlace |= (placeTypeFound >= 0);
	if (isPlace && placeTypeFound < 0)
		placeTypeFound = UNKNOWN_PLACE_SUBTYPE;
	lpwstring tmpstr;
	if (!isPlace && (isPerson || isBusiness || isWork))
	{
		objects[o].isNotAPlace = true;
		objects[o].resetSubType();
		lplog(LOG_WIKIPEDIA, u"%06d:%s reassigned (dbPedia) to not a place.", principalWhere, objectString(o, tmpstr, false).c_str());
	}
	if ((initialTenseOnly || !(objects[o].male ^ objects[o].female)) && (placeTypeFound >= 0 || (isWork && !isPerson)))
	{
		bool reassigned = false;
		if (reassigned = (placeTypeFound >= 0 && (objects[o].getSubType() < 0 || objects[o].getSubType() == UNKNOWN_PLACE_SUBTYPE)))
			objects[o].setSubType(placeTypeFound);
		if (objects[o].objectClass == NAME_OBJECT_CLASS)
			objects[o].objectClass = NON_GENDERED_NAME_OBJECT_CLASS;
		if (objects[o].objectClass == GENDERED_GENERAL_OBJECT_CLASS) objects[o].objectClass = NON_GENDERED_GENERAL_OBJECT_CLASS;
		objects[o].male = objects[o].female = false;
		objects[o].neuter = true;
		if (reassigned)
			lplog(LOG_WIKIPEDIA, u"%06d:%s reassigned (dbPedia) to the object subtype %s.", principalWhere,
				objectString(o, tmpstr, false).c_str(), OCSubTypeStrings[placeTypeFound]);
		else
			lplog(LOG_WIKIPEDIA, u"%06d:%s ungendered (dbPedia).", principalWhere, objectString(o, tmpstr, false).c_str());
	}
	if ((initialTenseOnly || !(objects[o].male ^ objects[o].female)) && isBusiness && !isPerson && !isPlace &&
		(objects[o].objectClass == NAME_OBJECT_CLASS ||
			objects[o].objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
			objects[o].objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS ||
			objects[o].objectClass == NON_GENDERED_NAME_OBJECT_CLASS))
	{
		objects[o].objectClass = NON_GENDERED_BUSINESS_OBJECT_CLASS;
		lplog(LOG_WIKIPEDIA, u"%06d:%s reassigned (dbPedia) to a business.", principalWhere, objectString(o, tmpstr, false).c_str());
		objects[o].male = objects[o].female = false;
		objects[o].neuter = true;
	}
	if ((initialTenseOnly || (objects[o].male ^ objects[o].female)) && !isBusiness && isPerson && !isPlace && !isWork &&
		objects[o].objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
	{
		objects[o].objectClass = NAME_OBJECT_CLASS;
		lplog(LOG_WIKIPEDIA, u"%06d:%s reassigned (dbPedia) to a name.", principalWhere, objectString(o, tmpstr, false).c_str());
		objects[o].male = objects[o].female = true;
		objects[o].neuter = false;
		if (objects[o].name.first == wNULL && objects[o].name.any == wNULL)
			objects[o].name.any = m[principalWhere].word;
	}
	objects[o].isWikiPlace = isPlace;
	objects[o].isWikiPerson = isPerson;
	objects[o].isWikiBusiness = isBusiness;
	objects[o].isWikiWork = isWork;
	return 0;
}

// True if the question source object is a single ALL-CAPS token that also has a non-noun
// form (e.g. US as acronym) — matching against mixed-case child sources is unreliable.
// is questionInformationSourceObject entirely uppercase, single word and having another class other than noun? (US - as in the acronym for the United States)
// is childSource primarily capitalized?  If so, throw out this source because matching is not reliable in this case.
bool cSource::checkForUppercaseSources(int questionInformationSourceObject)
{
	if (questionInformationSourceObject < 0 || objects[questionInformationSourceObject].end - objects[questionInformationSourceObject].begin != 1 ||
		!(m[objects[questionInformationSourceObject].begin].flags & cWordMatch::flagAllCaps))
		return false;
	tIWMM w = m[objects[questionInformationSourceObject].begin].word;
	bool otherThanNoun = false;
	for (unsigned int f = 0; f < w->second.formsSize() && !otherThanNoun; f++)
		otherThanNoun = (w->second.Form(f)->index != nounForm && w->second.Form(f)->index != PROPER_NOUN_FORM_NUM);
	return otherThanNoun;
}

// If >90% of tokens from I+1 to EOS are ALL-CAPS, advances I to that EOS and returns true
// (caller should skip the sentence as an unreliable child source).
bool cSource::skipSentenceForUpperCase(unsigned int& I)
{
	// is childSource primarily capitalized around I?
	unsigned int numWordsCapitalized = 0, numWordsChecked = 0, s;
	for (s = I + 1; s < m.size() && !isEOS(s); s++, numWordsChecked++)
		if (m[s].flags & cWordMatch::flagAllCaps)
			numWordsCapitalized++;
	if (numWordsChecked && 100 * numWordsCapitalized / numWordsChecked > 90)
	{
		if (logQuestionDetail)
			lplog(LOG_WHERE, u"tossing out all cap source %d:%s.", I, sourcePath.c_str());
		I = s; // skip to the next sentence
		return true;
	}
	return false;
}

#ifdef TEST_CODE

int runJavaJerichoHTML(lpwstring webAddress, lpwstring outputPath, string& outbuf);

// TEST_CODE-only WinHTTP GET (hardcoded umbel.org). Overwrites str. Returns 0/-1.
// Error paths leak any handles already opened (no CloseHandle before return).
// a routine derived from readPage which uses Windows HTTP routines – not used
int cWord::readPageWinHTTP(lpchar_t* str, lpwstring& buffer)
{
	LFS
		lpwstring lem;
	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	LPSTR pszOutBuffer;
	BOOL  bResults = FALSE;
	HINTERNET  hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;
	// http://umbel.org/umbel/rc/MusicalPerformer
	if (!cInternet::readPage(u"http://umbel.org/umbel/rc/MusicalPerformer.rdf", buffer)) return 0;

	lpchar_t server[1024];
	lp_strcpy(server, u"umbel.org");
	lp_strcpy(str, u"/umbel/rc/MusicalPerformer.rdf");
	// Use WinHttpOpen to obtain a session handle.
	if (!(hSession = WinHttpOpen(u"HTTP/1.1",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0)))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpOpen - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// Use WinHttpSetTimeouts to set a new time-out values.
	if (!WinHttpSetTimeouts(hSession, 0, 0, 0, 0))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpSetTimeouts - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}

	// Specify an HTTP server.
	if (!(hConnect = WinHttpConnect(hSession, server,
		INTERNET_DEFAULT_HTTPS_PORT, 0)))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpConnect %s - %s.", server, getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// Create an HTTP request handle.
	if (!(hRequest = WinHttpOpenRequest(hConnect, u"GET", str,
		NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		0))) //WINHTTP_FLAG_SECURE )))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpOpenRequest %s - %s.", str, getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// Use WinHttpSetTimeouts to set a new time-out values.
	if (!WinHttpSetTimeouts(hRequest, 0, 0, 0, 0))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpSetTimeouts on request - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// WINHTTP_OPTION_SEND_TIMEOUT
	// Sets or retrieves an unsigned long integer value that contains the time-out value, in milliseconds, to send a request or write some data. If sending the request takes longer than the timeout, the send operation is canceled. The default timeout is 30 seconds.
	DWORD timeoutValue;
	DWORD dwBufferLength = sizeof(timeoutValue);
	if (!WinHttpQueryOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutValue, &dwBufferLength))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpQueryOption on request - %s.", getLastErrorMessage(lem));
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	lplog(LOG_ERROR, u"ERROR:WinHttpQueryOption yields a timeout value of %d(%d).", timeoutValue, dwBufferLength);
	timeoutValue = 0;
	dwBufferLength = sizeof(timeoutValue);
	if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutValue, dwBufferLength))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpSetOption on request - %s.", getLastErrorMessage(lem));
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
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpSendRequest %s%s - %s (%d MS).", server, str, getLastErrorMessage(lem), clock() - start);
		lplog(LOG_ERROR, NULL);
		return -1;
	}
	// End the request.
	if (!(bResults = WinHttpReceiveResponse(hRequest, NULL)))
	{
		lplog(LOG_ERROR, u"ERROR:Cannot WinHttpReceiveResponse %s%s - %s.", server, str, getLastErrorMessage(lem));
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
					(int)errno);

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
					printf("Error %u in WinHttpReadData.\n", (int)errno);
				else
					printf("%s", pszOutBuffer);

				// Free the memory allocated to the buffer.
				delete[] pszOutBuffer;
			}
		} while (dwSize > 0);
	}


	// Report any errors.
	if (!bResults)
		printf("Error %d has occurred.\n", (int)errno);

	// Close any open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);
	return 0;
}
#endif