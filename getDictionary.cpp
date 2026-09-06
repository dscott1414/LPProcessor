/*
	getDictionary.cpp - HTML/JSON dictionary scrape, form discovery, and path/cache helpers

	Overview:
		Runtime lexicon fill-in from WordNet and Wiktionary extracts. NOTE:
		cWord::getForms currently has no part-of-speech source wired up at all --
		see the comment on it below. Also owns the shared HTML-slice helpers
		(takeLastMatch, firstMatch, nextMatch, eliminateHTMLCharacterEntities) used
		by the Wikipedia scraper, plus distributeToSubDirectories / getPath for
		cache files.

	Pipeline position:
		Called from cWord::getForms when a token is not already in the word table
		(initialization and during tokenize of new sources).

	Key entry points:
		- cWord::getForms / checkAdd / splitWord / illegalWord
		- getWNForms
		- getInflection / discoverInflections / identifyFormClass
		- takeLastMatch / firstMatch / nextMatch / firstMatchNonEmbedded
		- distributeToSubDirectories / getPath / eliminateHTMLCharacterEntities

	Dependencies:
		WordNet; yajl; webSearchCache cache dir.
		(The dictionary.com integration was removed at the author's request, which
		is why illegalWord() no longer consults MySQL and its `mysql` parameter is
		unused.)

	Notes / gotchas:
		firstMatch(lpchar_t* / char*) NUL-terminates the endString in
		the caller's buffer. getPath return polarity is inverted at several call sites
		(0 = success).
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <thread>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "errno.h"
#include "word.h"
#include "time.h"
#include "profile.h"
#include "paice.h"
#include "mysqldb.h"
#include "mysqld_error.h"
#include <sstream>
#include <iostream>
#include <vector>
extern "C" {
#include <yajl_tree.h>
}
#define MAX_LEN 2048
#include "internet.h"

void encodeURL(lpwstring winput, lpwstring& wencodedURL); // defined in createOntology.cpp

int bandwidthControl = 1; // minimum seconds between requests   // initialized before threads


// Inserts X\\Y\\ after pathlen using the first two filename chars ('.' ? '!'), optionally
// mkdir. Returns early (path already rewritten) if a mkdir fails for a reason other than EEXIST.
void distributeToSubDirectories(lpchar_t* fullPath, int pathlen, bool createDirs)
{
	LFS
		lpchar_t* path = fullPath + pathlen;
	if (path[1] && path[2] && path[1] != ' ' && path[2] != ' ')
	{
		memmove(path + 4, path, (lp_strlen(path) + 1) * sizeof(path[0]));
		path[0] = path[5];
		if (path[0] == u'.')
			path[0] = u'!';
		path[1] = '\\';
		if (createDirs)
		{
			path[2] = 0;
			if (lp_wmkdir(fullPath) < 0 && errno != EEXIST)
				return;
		}
		path[2] = path[6];
		if (path[2] == u'.')
			path[2] = u'!';
		path[3] = '\\';
		if (createDirs)
		{
			lpchar_t savech = path[4];
			path[4] = 0;
			if (lp_wmkdir(fullPath) < 0 && errno != EEXIST)
				return;
			path[4] = savech;
		}
	}
}

// Finds the last beginString?endString pair, copies it into match, and erases it from buffer.
// Returns the start index, or TAKE_LAST_MATCH_BEGIN/END_NOT_FOUND.
int takeLastMatch(lpwstring& buffer, lpwstring beginString, lpwstring endString, lpwstring& match, bool include_begin_and_end)
{
	LFS
		size_t beginPos = lpwstring::npos, pos = 0, endPos;
	while (pos != lpwstring::npos)
	{
		pos = buffer.find(beginString, (beginPos == lpwstring::npos) ? 0 : beginPos + beginString.length());
		if (pos != lpwstring::npos && (endPos = buffer.find(endString, pos + beginString.length())) == lpwstring::npos) break;
		if (pos != lpwstring::npos) beginPos = pos;
	}
	if (beginPos == lpwstring::npos)
	{
		//  lp_wprintf(u"begin expression %s not found.",beginString.c_str());
		return TAKE_LAST_MATCH_BEGIN_NOT_FOUND;
	}
	endPos = buffer.find(endString, beginPos + beginString.length());
	if (endPos == lpwstring::npos)
	{
		// lp_wprintf(u"end expression %s not found.",beginString.c_str());
		return TAKE_LAST_MATCH_END_NOT_FOUND;
	}
	int len = endPos - beginPos + endString.length();
	match = buffer.substr(beginPos + ((include_begin_and_end) ? 0 : beginString.length()), len - ((include_begin_and_end) ? 0 : (endString.length() + beginString.length())));
	if (len >= 1)
		buffer.erase(beginPos, len);
	while (buffer.length() && buffer[buffer.length() - 1] == ' ')
		buffer.erase(buffer.length() - 1);
	return beginPos;
}

// First beginString?endString at/after beginPos; copies into match and erases. Returns the
// start, or npos (also written to beginPos).
size_t firstMatch(lpwstring& buffer, lpwstring beginString, lpwstring endString, size_t& beginPos, lpwstring& match, bool include_begin_and_end)
{
	LFS
		beginPos = buffer.find(beginString, (beginPos == lpwstring::npos) ? 0 : beginPos);
	int endPos;
	if (beginPos != lpwstring::npos && (endPos = buffer.find(endString, beginPos + beginString.length())) != lpwstring::npos)
	{
		int len = endPos - beginPos + endString.length();
		match = buffer.substr(beginPos + ((include_begin_and_end) ? 0 : beginString.length()), len - ((include_begin_and_end) ? 0 : (endString.length() + beginString.length())));
		if (len >= 1)
			buffer.erase(beginPos, len);
		return beginPos;
	}
	return beginPos = lpwstring::npos;
}

// Narrow-string firstMatch.
size_t firstMatch(string& buffer, string beginString, string endString, size_t& beginPos, string& match, bool include_begin_and_end)
{
	LFS
		beginPos = buffer.find(beginString, (beginPos == string::npos) ? 0 : beginPos);
	int endPos;
	if (beginPos != string::npos && (endPos = buffer.find(endString, beginPos + beginString.length())) != string::npos)
	{
		int len = endPos - beginPos + endString.length();
		match = buffer.substr(beginPos + ((include_begin_and_end) ? 0 : beginString.length()), len - ((include_begin_and_end) ? 0 : (endString.length() + beginString.length())));
		if (len >= 1)
			buffer.erase(beginPos, len);
		return beginPos;
	}
	return beginPos = string::npos;
}

// In-place: NUL-terminates at endString and returns a pointer to the interior. Mutates buffer.
lpchar_t* firstMatch(lpchar_t* buffer, const lpchar_t* beginString, const lpchar_t* endString)
{
	LFS
		lpchar_t* beginPos = lp_strstr(buffer, beginString);
	lpchar_t* endPos;
	if (beginPos != NULL)
	{
		beginPos += lp_strlen(beginString);
		if ((endPos = lp_strstr(beginPos, endString)) != NULL)
		{
			*endPos = 0;
			return beginPos;
		}
	}
	return NULL;
}

// Narrow in-place firstMatch (NUL-terminates the caller buffer at endString).
char* firstMatch(char* buffer, const char* beginString, const char* endString)
{
	LFS
		char* beginPos = strstr(buffer, beginString);
	char* endPos;
	if (beginPos != NULL)
	{
		beginPos += strlen(beginString);
		if ((endPos = strstr(beginPos, endString)) != NULL)
		{
			*endPos = 0;
			return beginPos;
		}
	}
	return NULL;
}

// Like firstMatch but skips an endString that has another beginString before it (one nest level).
// Returns the start, or -1 (and beginPos = -1).
// if there is an embedded beginString/endString within the beginString/endString, only take the larger one.
int firstMatchNonEmbedded(lpwstring& buffer, lpwstring beginString, lpwstring endString, size_t& beginPos, lpwstring& match, bool include_begin_and_end)
{
	LFS
		beginPos = buffer.find(beginString, (beginPos == lpwstring::npos) ? 0 : beginPos);
	int endPos, lastEmbeddedEnd = beginPos + beginString.length();
	while (beginPos != lpwstring::npos && (endPos = buffer.find(endString, lastEmbeddedEnd)) != lpwstring::npos)
	{
		size_t embeddedPos = buffer.find(beginString, lastEmbeddedEnd);
		if (embeddedPos != lpwstring::npos && embeddedPos < endPos)
		{
			lastEmbeddedEnd = endPos + endString.length();
			continue;
		}
		int len = endPos - beginPos + endString.length();
		match = buffer.substr(beginPos + ((include_begin_and_end) ? 0 : beginString.length()), len - ((include_begin_and_end) ? 0 : (endString.length() + beginString.length())));
		if (len >= 1)
			buffer.erase(beginPos, len);
		return beginPos;
	}
	return beginPos = -1;
}

// Non-destructive firstMatch: copies the span into match and advances beginPos past endString.
// Returns 0, NEXT_MATCH_BEGIN_NOT_FOUND, or NEXT_MATCH_END_NOT_FOUND.
int nextMatch(lpwstring& buffer, lpwstring beginString, lpwstring endString, size_t& beginPos, lpwstring& match, bool include_begin_and_end)
{
	LFS
		size_t pos = 0;
	pos = buffer.find(beginString, (beginPos == lpwstring::npos) ? 0 : beginPos);
	if (pos == lpwstring::npos)
	{
		//  lp_wprintf(u"begin expression %s not found.",beginString.c_str());
		return NEXT_MATCH_BEGIN_NOT_FOUND;
	}
	size_t endPos = buffer.find(endString, pos + beginString.length());
	if (endPos == lpwstring::npos)
	{
		// lp_wprintf(u"end expression %s not found.",beginString.c_str());
		return NEXT_MATCH_END_NOT_FOUND;
	}
	int len = endPos - pos + endString.length();
	match = buffer.substr(pos + ((include_begin_and_end) ? 0 : beginString.length()), len - ((include_begin_and_end) ? 0 : (endString.length() + beginString.length())));
	beginPos = endPos + endString.length();
	return 0;
}

/*
Name            Syntax      Description
Aacute          &Aacute;    Capital A, acute accent
Agrave          &Agrave;    Capital A, grave accent
Acirc           &Acirc;     Capital A, circumflex accent
Atilde          &Atilde;    Capital A, tilde
Aring           &Aring;     Capital A, ring
Auml            &Auml;      Capital A, dieresis or umlaut mark
AElig           &AElig;     Capital AE dipthong (ligature)
Ccedil          &Ccedil;    Capital C, cedilla
Eacute          &Eacute;    Capital E, acute accent
Egrave          &Egrave;    Capital E, grave accent
Ecirc           &Ecirc;     Capital E, circumflex accent
Euml            &Euml;      Capital E, dieresis or umlaut mark
Iacute          &Iacute;    Capital I, acute accent
Igrave          &Igrave;    Capital I, grave accent
Icirc           &Icirc;     Capital I, circumflex accent
Iuml            &Iuml;      Capital I, dieresis or umlaut mark
ETH             &ETH;       Capital Eth, Icelandic
Ntilde          &Ntilde;    Capital N, tilde
Oacute          &Oacute;    Capital O, acute accent
Ograve          &Ograve;    Capital O, grave accent
Ocirc           &Ocirc;     Capital O, circumflex accent
Otilde          &Otilde;    Capital O, tilde
Ouml            &Ouml;      Capital O, dieresis or umlaut mark
Oslash          &Oslash;    Capital O, slash
Uacute          &Uacute;    Capital U, acute accent
Ugrave          &Ugrave;    Capital U, grave accent
Ucirc           &Ucirc;     Capital U, circumflex accent
Uuml            &Uuml;      Capital U, dieresis or umlaut mark
Yacute          &Yacute;    Capital Y, acute accent

THORN           &THORN;     Capital THORN, Icelandic
szlig           &szlig;     Small sharp s, German (sz ligature)

aacute          &aacute;    Small a, acute accent
agrave          &agrave;    Small a, grave accent
acirc           &acirc;     Small a, circumflex accent
atilde          &atilde;    Small a, tilde
atilde          &atilde;    Small a, tilde
auml            &auml;      Small a, dieresis or umlaut mark
aelig           &aelig;     Small ae dipthong (ligature)
ccedil          &ccedil;    Small c, cedilla
eacute          &eacute;    Small e, acute accent
egrave          &egrave;    Small e, grave accent
ecirc           &ecirc;     Small e, circumflex accent
euml            &euml;      Small e, dieresis or umlaut mark
iacute          &iacute;    Small i, acute accent
igrave          &igrave;    Small i, grave accent
icirc           &icirc;     Small i, circumflex accent
iuml            &iuml;      Small i, dieresis or umlaut mark
eth             &eth;       Small eth, Icelandic
ntilde          &ntilde;    Small n, tilde
oacute          &oacute;    Small o, acute accent
ograve          &ograve;    Small o, grave accent
ocirc           &ocirc;     Small o, circumflex accent
otilde          &otilde;    Small o, tilde
ouml            &ouml;      Small o, dieresis or umlaut mark
oslash          &oslash;    Small o, slash
uacute          &uacute;    Small u, acute accent
ugrave          &ugrave;    Small u, grave accent
ucirc           &ucirc;     Small u, circumflex accent
uuml            &uuml;      Small u, dieresis or umlaut mark
yacute          &yacute;    Small y, acute accent
thorn           &thorn;     Small thorn, Icelandic
yuml            &yuml;      Small y, dieresis or umlaut mark

SYMBOLS
&nbsp;      non-breaking space
�               &iexcl;     inverted exclamation mark
�               &curren;    currency
�               &cent;      cent
�               &pound;     pound
�               &yen;       yen
�               &brvbar;    broken vertical bar
�               &sect;      section
�               &uml;       spacing diaeresis
�               &copy;      copyright
�               &ordf;      feminine ordinal indicator
�               &laquo;     angle quotation mark (left)
�               &not;       negation
�               &shy;       soft hyphen
�               &reg;       registered trademark
�               &trade;     trademark
�               &macr;      spacing macron
�               &deg;       degree
�               &plusmn;    plus-or-minus
�               &sup2;      superscript 2
�               &sup3;      superscript 3
�               &acute;     spacing acute
�               &micro;     micro
�               &para;      paragraph
�               &middot;    middle dot
�               &cedil;     spacing cedilla
�               &sup1;      superscript 1
�               &ordm;      masculine ordinal indicator
�               &raquo;     angle quotation mark (right)
�               &frac14;    fraction 1/4
�               &frac12;    fraction 1/2
�               &frac34;    fraction 3/4
�               &iquest;    inverted question mark
�               &times;     multiplication
�               &divide;    division

*/
// Replaces &Name; with the first letter of Name, drops &#NNN; and &sym; entirely.
// buffer[pos+1] (and the other lookaheads below) are read without an explicit length check,
// but this is bounds-safe: lpwstring guarantees buffer[buffer.size()] reads as u'\0', and
// every lookahead beyond pos+1 is only reached after the previous character was confirmed
// to be real (not the terminator), so the index never exceeds buffer.size().
void eliminateHTMLCharacterEntities(lpwstring& buffer)
{
	LFS
		// all begin with & and end with ;
		const lpchar_t* ce[] = {
			u"Aacute",u"Agrave",u"Acirc",u"Atilde",u"Aring",u"Auml",u"AElig",u"Ccedil",u"Eacute",u"Egrave",u"Ecirc",u"Euml",u"Iacute",u"Igrave",u"Icirc",u"Iuml",u"ETH",u"Ntilde",
			u"Oacute",u"Ograve",u"Ocirc",u"Otilde",u"Ouml",u"Oslash",u"Uacute",u"Ugrave",u"Ucirc",u"Uuml",u"Yacute",u"THORN",u"szlig",u"aacute",u"agrave",u"acirc",u"atilde",
			u"atilde",u"auml",u"aelig",u"ccedil",u"eacute",u"egrave",u"ecirc",u"euml",u"iacute",u"igrave",u"icirc",u"iuml",u"eth",u"ntilde",u"oacute",u"ograve",u"ocirc",
			u"otilde",u"ouml",u"oslash",u"uacute",u"ugrave",u"ucirc",u"uuml",u"yacute",u"thorn",u"yuml", // &amp; and other XML special characters are taken care of while parsing
			NULL
	};

	const lpchar_t* sym[] = {
		u"nbsp",u"iexcl",u"curren",u"cent",u"pound",u"yen",u"brvbar",u"sect",u"uml",u"copy",u"ordf",u"laquo",u"not",u"shy",u"reg",u"trade",u"macr",u"deg",u"plusmn",u"sup2",u"sup3",
		u"acute",u"micro",u"para",u"middot",u"cedil",u"sup1",u"ordm",u"raquo",u"frac14",u"frac12",u"frac34",u"iquest",u"times",u"divide",
		NULL
	};
	int pos;
	if ((pos = buffer.find('&')) == lpwstring::npos || (!iswalpha(buffer[pos + 1]) && buffer[pos + 1] != u'#')) return;
	while (true)
	{
		if (buffer[pos + 1] == u'#')
		{
			int end = pos + 2;
			while (iswdigit(buffer[end]))
				end++;
			buffer.erase(pos, end - pos + 1);
			pos = -1;
		}
		if (pos != -1)
			for (unsigned int I = 0; ce[I]; I++)
				if (!lp_strncmp(buffer.c_str() + pos + 1, ce[I], lp_strlen(ce[I])) && buffer[pos + 1 + lp_strlen(ce[I])] == ';')
				{
					buffer.erase(pos, lp_strlen(ce[I]) + 1);
					buffer[pos] = ce[I][0];
					pos = -1;
					break;
				}
		if (pos != -1)
			for (unsigned int I = 0; sym[I]; I++)
				if (!lp_strncmp(buffer.c_str() + pos + 1, sym[I], lp_strlen(sym[I])) && buffer[pos + 1 + lp_strlen(sym[I])] == ';')
				{
					buffer.erase(pos, lp_strlen(sym[I]) + 2);
					pos = -1;
					break;
				}
		if ((pos = buffer.find('&', pos + 1)) == lpwstring::npos || (!iswalpha(buffer[pos + 1]) && buffer[pos + 1] != u'#')) return;
	}
}

// Remove the "middot" syllable separators that dictionary sources put inside a
// headword ("dic\u00b7tion\u00b7ar\u00b7y"), then run eliminateHTMLCharacterEntities.
//
// Three characters are stripped, all of which real dictionary markup uses for this:
//   U+00B7 MIDDLE DOT        -- the common one
//   U+2027 HYPHENATION POINT -- the character actually designated for syllabification
//   U+2022 BULLET            -- what some sources substitute for the middot
//
// The ASCII full stop '.' is deliberately NOT removed. It is not a syllable
// separator, and stripping it would corrupt abbreviations ("U.S.", "etc.") and
// every sentence boundary in the surrounding text.
void removeDots(lpwstring& str)
{
	LFS
		static const lpchar_t middots[] = { u'\u00b7', u'\u2027', u'\u2022', 0 };
	for (const lpchar_t* d = middots; *d; d++)
	{
		size_t dot;
		while ((dot = str.find(*d)) != lpwstring::npos)
			str.erase(dot, 1);
	}
	eliminateHTMLCharacterEntities(str);
}

// Fills allInflections with MW-style inflection strings for sWord given form/mainEntry/iform
// (handles -ed/-ing/-s and irregular tables). Returns the count generated.
/* examples
-ed/-ing/-s
*/

/*
Regular plural Nouns
A noun that has only a regular English plural formed by adding the suffix -s or the suffix -es or by
changing a final -y to -i- and adding the suffix -es is indicated by an -s or -es on the Inflected Form line:

Main Entry: 1bird . . .
Function: noun
Inflected Form: -s . . .

Irregular plurals
If a plural is irregular in any way, the full form is given in boldface:

Main Entry: 1man . . .
Function: noun
Inflected Form: plural men . . .

Variant plurals
If a noun has two or more plurals, all are written out in full and joined by or or also to indicate
whether the forms are equal variants or secondary variants:

Main Entry: 1fish . . .
Function: noun
Inflected Form: plural fish or fishes . . .

Main Entry: 1court-martial . . .
Function: noun
Inflected Form: plural courts-martial also court-martials . . .

Nouns that are plural in form and that are regularly used in plural construction are labeled noun
plural (without a comma):

Main Entry: en�vi�rons
Function: noun plural . . .

If the plural form is not always construed as a plural, the compactLabel continues with an applicable qualification:

Main Entry: ge�net�ics . . .
Function: noun plural but singular in construction . . .

Main Entry: pol�i�tics . . .
Function: noun plural but singular or plural in construction

Main Entry: math�e�mat�ics . . .
Function: noun plural but usually singular in construction

The phrase singular in construction indicates that the entry word takes a singular verb.

Compound plurals
Plurals are usually omitted for compound nouns containing a terminal element that corresponds to a whole
English word whose plural is regular and is shown at its own place. For example, the plurals for blackbird,
arrow grass, cake-eater, and bioecology are omitted because they can be found at bird, grass, eater, and ecology.

At words (as bioecology) that may be unfamiliar, an etymology consisting of the elements of the compound
words shows the element at which an omitted plural can be looked up. Plurals are often not indicated at
nonstandard terms. At compounds with doubtful irregular plurals, the plural forms are written out in full.

Plurals entered at their own place
A plural form that is spelled quite differently from the singular form or which would fall more than five
inches from the singular form in the printed dictionary will have its own entry, which will cross-reference
to the singular form:

Main Entry: mice . . .
plural of MOUSE

Main Entry: geni�i . . .
plural of GENIUS

Such an entry does not specify whether it is the only plural; it simply tells where to look for relevant
information. At genius the variant plurals geniuses and genii are shown. The plural geniuses is not a main entry
because it is alphabetically close to the main entry of genius.

Regular verbs
Verbs are considered regular when their past is simply formed by adding a terminal -ed with no other
change except dropping a final -e or changing a final -y to -i-.

The principal parts for these verbs are indicated by adding -es/-ing/-s or -ed/-ing/-es to represent
the past and past participle endings (-ed), the present participle ending (-ing), and the present 3rd
singular ending (-s or -es):

Main Entry: 1bark . . .
Function: verb
Inflected Form: -ed/-ing/-s

Main Entry: 1wish . . .
Function: verb
Inflected Form: -ed/-ing/-es

Irregular verbs
The four principal parts of verbs appear in the order past, the past participle, the present participle,
and the present 3rd singular and are shown in full in boldface whenever any one of them has an irregular
or unexpected combination of letters:

Main Entry: 1make . . .
Function: verb
Inflected Form: made . . . ; made ; making ; makes

Main Entry: 2dye . . .
Function: verb
Inflected Form: dyed ; dyed ; dyeing ; dyes

Whenever any of those four parts has a variant, all parts are written out in full:

Main Entry: 3ring . . .
Function: verb
Inflected Form: rang . . . ; also rung . . . ; rung ; ringing ; rings

Main Entry: 1show . . .
Function: verb
Inflected Form: showed ; shown . . . or showed ; showing ; shows

If the four spaces usually occupied by inflectional forms cannot (for lack of evidence) all be filled, the
surviving forms that can be given are identified by an italic compactLabel:

Main Entry: aby
Variant: or abye . . .
Function: verb
Inflected Form: past or past participle abought

Principal parts of compound verbs
Principal parts of verbs are usually omitted at compounds containing a terminal element or related homograph
whose principal parts are regular and are shown at the entry for that terminal element or related homograph.
For example, the principal parts of freewheel, overdrive, and unwrap are not given at those entries because
they can be found at wheel, drive, and wrap.

An etymology consisting of the elements of a compound verb shows the elements at which omitted principal parts
can be looked up. Principal parts are often not given at nonstandard terms or at verbs of relatively low frequency.

Principal parts of verbs entered at their own place
A principal verb part that is spelled quite differently from the infinitive or which would fall more than five
inches from the main entry in the printed dictionary will have its own entry, which will cross-reference the
infinitive:

Main Entry: burned
past of BURN

Main Entry: denies
present third singular of DENY

Comparatives and superlatives of adjectives and adverbs
All adjectives and adverbs that have comparatives and superlatives with the suffixes -er and -est have these
forms explicitly or implicitly shown in this dictionary. In some cases, they are written out in full in the
entry; in others, cutback forms are used. The following paragraphs explain how the principal parts of adjectives
and adverbs are depicted in the dictionary.

Showing -er and -est forms does not imply anything more about the use of more and most with a simple adjective
or adverb than that the comparative and superlative degrees can often be expressed in either way (luckier or
more lucky, smoothest or most smooth).

Regular comparatives and superlatives
When comparatives or superlatives are formed by simply adding -er and -est with no change except dropping of
final -e or changing of final -y to -i-, those forms are indicated by -er/-est following the Inflected Form(s)
compactLabel:

Main Entry: 1green . . .
Function: adjective
Inflected Form(s): -er/-est


Irregular comparatives and superlatives
Comparatives and superlatives are written out in full in boldface when they are irregular or when they double a
final consonant:

Main Entry: 1red . . .
Function: adjective
Inflected Form(s): redder; reddest

Comparatives and superlatives of compounds
Comparatives and superlatives are usually omitted at compounds containing a constituent element whose
inflection is shown at its own entry. Comparatives and superlatives of words such as kinderhearted and
unluckiest are omitted because-er and -est are shown at kind and at lucky. Similarly the comparatives and
superlatives of adverbs are often omitted when an adjective homograph shows them, as at flat and hot.

Comparatives and superlatives entered at their own place

Comparatives and superlatives that are spelled quite differently from their base form or which would fall
more than five inches from the main entry in the printed dictionary will have their own entries, which will
cross-reference the base entry:

Main Entry: hotter
comparative of HOT

Main Entry: hottest
superlative of HOT

*/
/* examples
<b>-ed/-ing/-s</b>
<b>ran</b> \
\; <i>or nonstandard</i> <b>run</b>; <b>run</b>; <b>running</b>; <b>runs</b>
*/
/*
Nouns:
Inflected Form: -s . . .
Inflected Form: plural men . . .
Inflected Form: plural fish or fishes . . .
Verbs:
Inflected Form: -ed/-ing/-s
Inflected Form: dyed ; dyed ; dyeing ; dyes
Adjectives&Adverbs
Inflected Form(s): -er/-est
Inflected Form(s): redder; reddest
*/
// "<i>also dialect</i>","<i>also chiefly","<i>also dialect",
// "<i>or nonstandard</i>","<i>or dialect","<i>or archaic</i>","<i>or chiefly"
const lpchar_t* alternates[] = {
	u"<i>also",u"<i>or",u"<i>chiefly in",
};

// True if sWord and sWord2 match ignoring case, spaces, and dashes.
bool equivalentIfIgnoreDashSpaceCase(lpwstring sWord, lpwstring sWord2)
{
	LFS
		// convert acute
		int pos;
	if ((pos = sWord.find(u"&eacute;")) != lpwstring::npos)
		sWord.replace(pos, 8, u"e");
	int iW = 0, iW2 = 0;
	while (true)
	{
		if (iW > 0)
		{
			while (sWord[iW] == u'-' || sWord[iW] == u' ') iW++;
			while (sWord2[iW2] == u'-' || sWord2[iW2] == u' ') iW2++;
		}
		if (towlower(sWord[iW]) != towlower(sWord2[iW2])) return false;
		if (!sWord[iW]) return true;
		iW++; iW2++;
	}
	return false;
}

// Adds sWord as form/inflection if missing. Returns 0 if added or already present, -1 if
// sForm is unknown. Trailing spaces on sWord/definitionEntry are stripped (MW artifact).
int cWord::checkAdd(const lpchar_t* fromWhere, tIWMM& iWord, lpwstring sWord, int flags, lpwstring sForm, int inflection, int derivationRules, lpwstring definitionEntry, int sourceId, bool log)
{
	LFS
		int iForm;
	vector <cForm*>::iterator ifc, ifcend = Forms.end();
	for (iForm = 0, ifc = Forms.begin(); ifc != ifcend && (*ifc)->name != sForm; ifc++, iForm++);
	if (ifc == ifcend)
	{
		unsigned int chi;
		if (sForm.find(u"adjective") != lpwstring::npos)
			sForm = u"adjective";
		else if (sForm.find(u"adverb") != lpwstring::npos)
			sForm = u"adverb";
		else if ((chi = sForm.find(u"verb")) != lpwstring::npos && (!sForm[chi + 4] || iswspace(sForm[chi + 4])))
			sForm = u"verb";
		else if (sForm.find(u"past part") != lpwstring::npos)
			sForm = u"verb";
		else if (sForm.find(u"plural in construction") != lpwstring::npos)
			sForm = u"noun";
		else if (sForm.find(u"exclamation") != lpwstring::npos)  // from Cambridge
			sForm = u"interjection";
		else if (sForm.find(u"definite article") != lpwstring::npos)
			sForm = u"determiner";
		else if (sForm.find(u"indefinite article") != lpwstring::npos)
			sForm = u"quantifier";
		else if (sForm.find(u"verbal auxiliary") != lpwstring::npos)
		{
			if (log)
				lplog(LOG_DICTIONARY, u"Form %s rejected!", sForm.c_str());
			return 0;
		}
	}
	while (sWord[sWord.length() - 1] == ' ') // strangely, all multi-word words in MW seem to have a space at the end
		sWord.erase(sWord.length() - 1);
	while (definitionEntry[definitionEntry.length() - 1] == ' ') // crow
		definitionEntry.erase(definitionEntry.length() - 1);
	lpwstring inflectionName;
	if (hasFormInflection(iWord, sForm, inflection) == WMM.end())
	{
		bool added;
		tIWMM saveIWord = iWord;
		iForm = addWordToForm(sWord, iWord, flags, sForm, sForm, inflection, derivationRules, definitionEntry, sourceId, added);
		if (saveIWord != WMM.end()) iWord = saveIWord;
		if (added)
		{
			if (log)
				lplog(LOG_DICTIONARY, u"(%s) word %s: Added Inflection%s (Form %s, definitionEntry %s)", fromWhere, sWord.c_str(), getInflectionName(inflection, iForm, inflectionName), sForm.c_str(), definitionEntry.c_str());
			return 0;
		}
	}
	iForm = cForms::findForm(sForm);
	if (iForm < 0)
	{
		if (log)
			lplog(LOG_DICTIONARY, u"form %s is not found!", sForm.c_str());
		return -1;
	}
	if (log)
		lplog(LOG_DICTIONARY, u"(%s) word %s: (Already) Added Inflection%s (Form %s, definitionEntry %s)", fromWhere, sWord.c_str(), getInflectionName(inflection, iForm, inflectionName), sForm.c_str(), definitionEntry.c_str());
	return 0;
}

// Reads pathname via CreateFile/ReadFile into buffer (maxlen bytes). Returns 0 on success
// (actualLen set), non-zero on open/read failure. Callers often treat 0 as "cache hit".
// this was changed from standard read
// because Microsoft version of ::read had a bug in it
int getPath(const lpchar_t* pathname, void* buffer, int maxlen, int& actualLen)
{
	LFS
		// Batch B8: POSIX open/fstat/read replace CreateFile/GetFileSize/ReadFile.
		int hFile = lp_wopen(pathname, O_RDONLY);

	if (hFile < 0)
	{
		lpwstring bcct;
		if (errno != ERROR_PATH_NOT_FOUND && errno != ERROR_FILE_NOT_FOUND)
			lplog(LOG_ERROR, u"GetPath cannot open path %s - %s", pathname, getLastErrorMessage(bcct));
		return GETPATH_CANNOT_OPEN_PATH;
	}
	actualLen = (int)lp_filelength(hFile);
	if (actualLen < 0)
	{
		lplog(LOG_ERROR, u"ERROR:filelength of file %s yields an invalid filelength (%d).", pathname, actualLen);
		::close(hFile);
		return GETPATH_INVALID_FILELENGTH1;
	}
	if (actualLen == 0)
	{
		::close(hFile);
		return 0;
	}
	if (actualLen + 1 >= maxlen)
	{
		lplog(LOG_ERROR, u"ERROR:filelength of file %s (%d) is greater than the maximum allowed (%d).", pathname, actualLen + 1, maxlen);
		::close(hFile);
		return GETPATH_INVALID_FILELENGTH2;
	}
	if (::read(hFile, buffer, (size_t)actualLen) != (ssize_t)actualLen)
	{
		lplog(LOG_ERROR, u"ERROR:read error of file %s.", pathname);
		::close(hFile);
		return GETPATH_INVALID_FILELENGTH2;
	}
	((char*)buffer)[actualLen] = 0; // batch B8: ::read's count, was ReadFile's lenRead out-param
	((char*)buffer)[actualLen + 1] = 0;
	::close(hFile);
	return 0;
}

// If sWord is a dashed/spaced compound, tries to add each piece. Returns 0 if any piece
// was added as a known form; non-zero otherwise.
int cWord::splitWord(MYSQL* mysql, tIWMM& iWord, lpwstring sWord, int sourceId, bool log)
{
	LFS
		if (sWord.length() < 5)
			return -1;
	static unordered_set <lpwstring> rejectSplitEndings = { u"o",u"ing",u"ton",u"rin",u"tin",u"pin",u"ism",u"ons",u"aire",u"ana",u"la",u"ard",u"ell",u"sey",u"ness",u"la",u"ies",
	u"ley",u"ers",u"ish",u"ner",u"ington",u"leys",u"que",u"tes",u"ion",u"in",u"els",u"era",u"ists",u"sie",u"and",u"ingly",u"ium",u"ics",u"ilate",u"issima",u"ells" };
	vector <lpwstring> components = splitString(sWord, '-');
	tIWMM iWordComponent = WMM.end();
	for (lpwstring w : components)
		if ((iWordComponent = fullQuery(mysql, w, sourceId)) == WMM.end())
			break;
	// don't split a word with a dash in it
	if (components.size() == 1 && (iWordComponent == WMM.end() || !cStemmer::wordIsNotUnknownAndOpen(iWordComponent, log) || rejectSplitEndings.find(components[components.size() - 1]) != rejectSplitEndings.end())) // not found or unknown
	{
		for (unsigned int I = 2; I < sWord.length() - 2; I++)
		{
			components.clear();
			lpwstring firstWord = sWord.substr(0, I);
			components.push_back(sWord.substr(I, sWord.length() - I));
			tIWMM firstQIWord;
			// with splitting word this way, the previous word must also be known and of an open word type. 
			if (((firstQIWord = fullQuery(mysql, firstWord, sourceId)) != WMM.end() && cStemmer::wordIsNotUnknownAndOpen(firstQIWord, log)) &&
				((iWordComponent = fullQuery(mysql, components[components.size() - 1], sourceId)) != WMM.end() && cStemmer::wordIsNotUnknownAndOpen(iWordComponent, log)) &&
				(rejectSplitEndings.find(components[components.size() - 1]) == rejectSplitEndings.end()))
				break;
			iWordComponent = WMM.end();
		}
	}
	if (iWordComponent != WMM.end() && cStemmer::wordIsNotUnknownAndOpen(iWordComponent, log) && rejectSplitEndings.find(components[components.size() - 1]) == rejectSplitEndings.end())
	{
		// (SW) word bone-cracking( main: verb present part)
		// (SW) word white-maned(main: verb past)
		// (SW) word micro-electric( main: adjective)
		// (SW) word half-wittingly(main: adverb)
		// (SW) word arms-sales(main: noun)
		iWord = end();
		if (iWordComponent->second.query(verbForm) >= 0)
			checkAdd(u"SW", iWord, sWord, 0, u"verb", iWordComponent->second.inflectionFlags, 0, components[components.size() - 1], sourceId, log);
		if (iWordComponent->second.query(nounForm) >= 0)
			checkAdd(u"SW", iWord, sWord, 0, u"noun", iWordComponent->second.inflectionFlags, 0, components[components.size() - 1], sourceId, log);
		if (iWordComponent->second.query(adjectiveForm) >= 0)
			checkAdd(u"SW", iWord, sWord, 0, u"adjective", iWordComponent->second.inflectionFlags, 0, components[components.size() - 1], sourceId, log);
		if (iWordComponent->second.query(adverbForm) >= 0)
			checkAdd(u"SW", iWord, sWord, 0, u"adverb", iWordComponent->second.inflectionFlags, 0, components[components.size() - 1], sourceId, log);
		if (iWord == end()) return -1;
		checkAdd(u"SW", iWord, sWord, 0, COMBINATION_FORM, 0, 0, components[components.size() - 1], sourceId, log);
		lplog(LOG_DICTIONARY, u"WordPosMAP splitWord %s-->%s", sWord.c_str(), components[components.size() - 1].c_str());
		lplog(LOG_DICTIONARY, u"%s TEMPWordPosMAP", components[components.size() - 1].c_str());
		return 0;
	}
	return -1;
}

// Case-insensitive lp_strcmp wrapper used to sort MW POS strings.
bool loosesort(const lpchar_t* s1, const lpchar_t* s2)
{
	LFS
		if (*s1 && *s2) return lp_strcmp(s1, s2) < 0;
	if (*s1)
		return lp_strncmp(s1, s2 + 1, lp_strlen(s1)) < 0;
	return lp_strncmp(s1 + 1, s2, lp_strlen(s2)) < 0;
}


/*
// map to class/classes
Italian adverb
adjective or adverb
adverb or adjective
adjective
adjective combining form
German adjective
verb
phrasal verb
impersonal verb
Latin verb
pronoun, singular or plural in construction
noun
noun phrase
noun or adjective
adjective or noun
noun, plural in form but singular in construction
noun, plural in form but singular or plural in construction
French noun phrase
Latin noun phrase
plural noun
French noun
noun combining form
noun or intransitive verb
adverb
interjection
French interjection
abbreviation
Latin abbreviation
symbol
preposition
conjunction
trademark
pronoun
pronoun or adjective
honorific title

// ignore
prefix
noun suffix
adverb suffix
adjective suffix
verb suffix
plural noun suffix
Greek phrase
French phrase
Latin phrase
German phrase
Spanish phrase
Italian phrase
Irish phrase
Hawaiian phrase
proverbial saying
conventional saying
French quotation from ...
Latin quotation from ...
German quotation from ...
pronunciation spelling
script annotation
combining form

// custom
idiom
biographical name
geographical name
certification mark
communications code word
communications signal
*/
vector<lpwstring> ignoreBefore = { u"pronunciation spelling" };
vector<lpwstring> classes = { u"adjective",u"adverb",u"verb",u"noun",u"interjection",u"abbreviation",u"symbol",u"preposition",u"conjunction",u"trademark",u"pronoun",u"honorific" };
vector<lpwstring> ignoreAfter = { u"prefix",u"suffix",u"phrase",u"saying",u"quotation",u"pronunciation spelling",u"script annotation",u"combining form",u"contraction",u"indefinite article",u"definite article" }; // must be processed after classes



// True if word contains a codepoint outside the Latin/common punctuation range LP can inflect.
// True if word cannot be represented in CP1252 without substitution.
// Batch B8: see createOntology.cpp's detectNonEuropean -- identical question,
// identical replacement. The Win32 WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS)
// probe is replaced by a direct check against the CP1252 repertoire.
bool detectNonEuropean(lpwstring word); // defined in createOntology.cpp
bool detectNonEuropeanWord(lpwstring word)
{
	return detectNonEuropean(word);
}


// True if sWord should not be added (too long, disqualified punctuation, or DB-blocked).
bool cWord::illegalWord(MYSQL* mysql, lpwstring sWord)
{
	// `mysql` is now unused: this used to end with a dictionary.com existence
	// check (existsInDictionaryDotCom), which rejected any word the site had no
	// page for. That integration has been removed at the author's request, so the
	// only remaining tests are the two below and MORE WORDS NOW COUNT AS LEGAL
	// than did before. The parameter is kept because illegalWord() is declared in
	// word.h and called from identifyObjects.cpp on the live parse path; changing
	// its signature is a separate decision from removing the integration.
	(void)mysql;
	// non English word?
	if (detectNonEuropeanWord(sWord) || sWord.find_first_of(u"ãâäáàæçêéèêëîíïñôóòöõôûüùú") != lpwstring::npos)
		return true;
	// embedded quote?
	size_t whereQuote = sWord.find('\'');
	if (whereQuote != lpwstring::npos && whereQuote > 0 && whereQuote < sWord.length() - 1)
		return true;
	return false;
}

// this routine should look up words from wiktionary or some other dictionary
// this returns >0 if word is found or WORD_NOT_FOUND if word lookup fails.
// Discovers forms for an unknown sWord. Adds them via checkAdd. Returns 0 if any
// form was added, negative if the word is illegal/empty.
// !!! NO DICTIONARY SOURCE IS WIRED UP.  This function always reports the word as
// not found, so unknown words acquire no forms.
//
// It used to get its part-of-speech set from the Merriam-Webster Collegiate API,
// which has been removed.  Nothing replaced it: discoverInflections/addNewOrModify
// below still work and are left intact, but they have no posSet to work from, so
// the function short-circuits rather than pretending to succeed.
//
// To restore unknown-word discovery, plug a source in here that fills `posSet`
// with form ids (the shape identifyFormClass() produces) and set `plural`, then
// delete the early return.  getWNForms() further down this file already returns
// WordNet parts of speech and has no callers -- it is the obvious candidate, but
// wiring it in changes which words the parser will learn, so it is a deliberate
// decision rather than a drop-in and is left to whoever makes it.
int cWord::getForms(MYSQL* mysql, tIWMM& iWord, lpwstring sWord, int sourceId, bool logEverything)
{
	LFS
		(void)iWord; (void)sourceId; (void)logEverything;
	if (illegalWord(mysql, sWord))
		return WORD_NOT_FOUND;
	return WORD_NOT_FOUND;
}

// Batch B9: errno + strerror, replacing GetLastError + FormatMessage against
// wininet.dll. The old version also appended InternetGetLastResponseInfo's extended
// text for ERROR_INTERNET_EXTENDED_ERROR; libcurl reports its own failures through
// the error buffer cInternet keeps, so there is no equivalent second source to
// consult here.
const lpchar_t* getLastErrorMessage(lpwstring& out)
{
	LFS
		out = lp_narrow_to_wide(std::string(strerror(errno)));
	return out.c_str();
}




#ifdef CHECK_WORD_CACHE

// Debug compare of a word after getForms (Words2 snapshot). Returns ret unchanged.
int cWord::checkWord(cWord& Words2, tIWMM originalIWord, tIWMM newWord, int ret)
{
	LFS
		int wait = 0;
	if (ret == WORD_NOT_FOUND || newWord == WMM.end()) return WORD_NOT_FOUND;
	if (ret)
		while (wait) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	// check for mainEntry
	if (originalIWord->first == newWord->first && originalIWord->second == newWord->second)
	{
		unsigned int* forms = newWord->second.forms();
		int count = newWord->second.formsSize(), I = 0;
		for (; I < count && !Forms[forms[I]]->inflectionsClass.length(); I++);
		if (I == count || (I < count && newWord->second.mainEntry != (tIWMM)NULL)) return 0;
		lplog(u"Word %s has a NULL main entry.", newWord->first.c_str());
	}
	lplog(u"name %s differs.", originalIWord->first.c_str());
	while (wait) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	return 0;
}

//#define TEST_SPECIFIC
lpchar_t* unknowns[] = { u"countermarches",NULL,
 "ageist",
NULL };

// True if str ends with endMatch.
bool endStringMatch(const lpchar_t* str, lpchar_t* endMatch)
{
	LFS
		return !lp_strcmp(str + lp_strlen(str) - lp_strlen(endMatch), endMatch);
}

#include "wn.h"

// Appends WordNet POS form indexes present for w. Returns true if any were found.
bool getWNForms(lpwstring w, vector <int>& WNForms)
{
	LFS
		if (checkexist((lpchar_t*)w.c_str(), NOUN)) WNForms.push_back(nounForm);
	if (checkexist((lpchar_t*)w.c_str(), VERB))  WNForms.push_back(verbForm);
	if (checkexist((lpchar_t*)w.c_str(), ADJ))  WNForms.push_back(adjectiveForm);
	if (checkexist((lpchar_t*)w.c_str(), ADV)) WNForms.push_back(adverbForm);
	return WNForms.size() > 0;
}

#endif


