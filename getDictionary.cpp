#include <errno.h>
#include <windows.h>
#include "WinInet.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "errno.h"
#include "word.h"
#include "time.h"
#include <direct.h>
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

int bandwidthControl=1; // minimum seconds between requests   // initialized before threads

void removeRedundantSpace(wstring &buffer)
{ LFS
	wstring tmpstr;
	for (unsigned int I=0; I<buffer.size(); I++)
		if (!iswspace(buffer[I]) || I==0 || !iswspace(buffer[I-1]))
			tmpstr+=buffer[I];
	buffer=tmpstr;
}

void distributeToSubDirectories(wchar_t *fullPath,int pathlen,bool createDirs)
{ LFS
	wchar_t *path=fullPath+pathlen;
	if (path[1] && path[2] && path[1]!=' ' && path[2]!=' ')
	{
		memmove(path+4,path,(wcslen(path)+1)*sizeof(path[0]));
		path[0]=path[5];
		if (path[0] == L'.')
			path[0] = L'!';
		path[1]='\\';
		if (createDirs)
		{
			path[2]=0;
			if (_wmkdir(fullPath)<0 && errno != EEXIST)
				return;
		}
		path[2]=path[6];
		if (path[2] == L'.')
			path[2] = L'!';
		path[3]='\\';
		if (createDirs)
		{
			wchar_t savech=path[4];
			path[4]=0;
			if (_wmkdir(fullPath) < 0 && errno != EEXIST)
				return;
			path[4]=savech;
		}
	}
}

int takeLastMatch(wstring &buffer,wstring beginString,wstring endString,wstring &match,bool include_begin_and_end)
{ LFS
	size_t beginPos=wstring::npos,pos=0,endPos;
	while (pos!=wstring::npos)
	{
		pos=buffer.find(beginString,(beginPos==wstring::npos) ? 0 : beginPos+beginString.length());
		if (pos!=wstring::npos && (endPos=buffer.find(endString,pos+beginString.length()))==wstring::npos) break;
		if (pos!=wstring::npos) beginPos=pos;
	}
	if (beginPos==wstring::npos)
	{
		//  wprintf(L"begin expression %s not found.",beginString.c_str());
		return TAKE_LAST_MATCH_BEGIN_NOT_FOUND;
	}
	endPos=buffer.find(endString,beginPos+beginString.length());
	if (endPos==wstring::npos)
	{
		// wprintf(L"end expression %s not found.",beginString.c_str());
		return TAKE_LAST_MATCH_END_NOT_FOUND;
	}
	int len=endPos-beginPos+endString.length();
	match=buffer.substr(beginPos+((include_begin_and_end) ? 0 : beginString.length()),len-((include_begin_and_end) ? 0 : (endString.length()+beginString.length())));
	if (len>=1)
		buffer.erase(beginPos,len);
	while (buffer.length() && buffer[buffer.length()-1]==' ')
		buffer.erase(buffer.length()-1);
	return beginPos;
}

size_t firstMatch(wstring &buffer, wstring beginString, wstring endString, size_t &beginPos, wstring &match, bool include_begin_and_end)
{
	LFS
		beginPos = buffer.find(beginString, (beginPos == wstring::npos) ? 0 : beginPos);
	int endPos;
	if (beginPos != wstring::npos && (endPos = buffer.find(endString, beginPos + beginString.length())) != wstring::npos)
	{
		int len = endPos - beginPos + endString.length();
		match = buffer.substr(beginPos + ((include_begin_and_end) ? 0 : beginString.length()), len - ((include_begin_and_end) ? 0 : (endString.length() + beginString.length())));
		if (len >= 1)
			buffer.erase(beginPos, len);
		return beginPos;
	}
	return beginPos = wstring::npos;
}

size_t firstMatch(string &buffer, string beginString, string endString, size_t &beginPos, string &match, bool include_begin_and_end)
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

wchar_t *firstMatch(wchar_t *buffer, const wchar_t *beginString, const wchar_t *endString)
{
	LFS
	wchar_t* beginPos = wcsstr(buffer, beginString);
	wchar_t *endPos;
	if (beginPos != NULL)
	{
		beginPos += wcslen(beginString);
		if ((endPos = wcsstr(beginPos, endString)) != NULL)
		{
			*endPos = 0;
			return beginPos;
		}
	}
	return NULL;
}

char *firstMatch(char *buffer, const char *beginString, const char *endString)
{
	LFS
	char* beginPos = strstr(buffer, beginString);
	char *endPos;
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

// if there is an embedded beginString/endString within the beginString/endString, only take the larger one.
int firstMatchNonEmbedded(wstring &buffer,wstring beginString,wstring endString,size_t &beginPos,wstring &match,bool include_begin_and_end)
{ LFS
	beginPos=buffer.find(beginString,(beginPos==wstring::npos) ? 0 : beginPos);
	int endPos,lastEmbeddedEnd=beginPos+beginString.length();
	while (beginPos!=wstring::npos && (endPos=buffer.find(endString,lastEmbeddedEnd))!=wstring::npos)
	{
		size_t embeddedPos=buffer.find(beginString,lastEmbeddedEnd);
		if (embeddedPos!=wstring::npos && embeddedPos<endPos)
		{
			lastEmbeddedEnd=endPos+endString.length();
			continue;
		}
		int len=endPos-beginPos+endString.length();
		match=buffer.substr(beginPos+((include_begin_and_end) ? 0 : beginString.length()),len-((include_begin_and_end) ? 0 : (endString.length()+beginString.length())));
		if (len>=1)
			buffer.erase(beginPos,len);
		return beginPos;
	}
	return beginPos=-1;
}

int nextMatch(wstring &buffer,wstring beginString,wstring endString,size_t &beginPos,wstring &match,bool include_begin_and_end)
{ LFS
	size_t pos=0;
	pos=buffer.find(beginString,(beginPos==wstring::npos) ? 0 : beginPos);
	if (pos==wstring::npos)
	{
		//  wprintf(L"begin expression %s not found.",beginString.c_str());
		return NEXT_MATCH_BEGIN_NOT_FOUND;
	}
	size_t endPos=buffer.find(endString,pos+beginString.length());
	if (endPos==wstring::npos)
	{
		// wprintf(L"end expression %s not found.",beginString.c_str());
		return NEXT_MATCH_END_NOT_FOUND;
	}
	int len=endPos-pos+endString.length();
	match=buffer.substr(pos+((include_begin_and_end) ? 0 : beginString.length()),len-((include_begin_and_end) ? 0 : (endString.length()+beginString.length())));
	beginPos=endPos+endString.length();
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
¡               &iexcl;     inverted exclamation mark
¤               &curren;    currency
¢               &cent;      cent
£               &pound;     pound
¥               &yen;       yen
¦               &brvbar;    broken vertical bar
§               &sect;      section
¨               &uml;       spacing diaeresis
©               &copy;      copyright
ª               &ordf;      feminine ordinal indicator
«               &laquo;     angle quotation mark (left)
¬               &not;       negation
­               &shy;       soft hyphen
®               &reg;       registered trademark
™               &trade;     trademark
¯               &macr;      spacing macron
°               &deg;       degree
±               &plusmn;    plus-or-minus
²               &sup2;      superscript 2
³               &sup3;      superscript 3
´               &acute;     spacing acute
µ               &micro;     micro
¶               &para;      paragraph
·               &middot;    middle dot
¸               &cedil;     spacing cedilla
¹               &sup1;      superscript 1
º               &ordm;      masculine ordinal indicator
»               &raquo;     angle quotation mark (right)
¼               &frac14;    fraction 1/4
½               &frac12;    fraction 1/2
¾               &frac34;    fraction 3/4
¿               &iquest;    inverted question mark
×               &times;     multiplication
÷               &divide;    division

*/
void eliminateHTMLCharacterEntities(wstring &buffer)
{ LFS
	// all begin with & and end with ;
	const wchar_t *ce[]={
		L"Aacute",L"Agrave",L"Acirc",L"Atilde",L"Aring",L"Auml",L"AElig",L"Ccedil",L"Eacute",L"Egrave",L"Ecirc",L"Euml",L"Iacute",L"Igrave",L"Icirc",L"Iuml",L"ETH",L"Ntilde",
		L"Oacute",L"Ograve",L"Ocirc",L"Otilde",L"Ouml",L"Oslash",L"Uacute",L"Ugrave",L"Ucirc",L"Uuml",L"Yacute",L"THORN",L"szlig",L"aacute",L"agrave",L"acirc",L"atilde",
		L"atilde",L"auml",L"aelig",L"ccedil",L"eacute",L"egrave",L"ecirc",L"euml",L"iacute",L"igrave",L"icirc",L"iuml",L"eth",L"ntilde",L"oacute",L"ograve",L"ocirc",
		L"otilde",L"ouml",L"oslash",L"uacute",L"ugrave",L"ucirc",L"uuml",L"yacute",L"thorn",L"yuml", // &amp; and other XML special characters are taken care of while parsing
		NULL
	};

	const wchar_t *sym[]={
		L"nbsp",L"iexcl",L"curren",L"cent",L"pound",L"yen",L"brvbar",L"sect",L"uml",L"copy",L"ordf",L"laquo",L"not",L"shy",L"reg",L"trade",L"macr",L"deg",L"plusmn",L"sup2",L"sup3",
		L"acute",L"micro",L"para",L"middot",L"cedil",L"sup1",L"ordm",L"raquo",L"frac14",L"frac12",L"frac34",L"iquest",L"times",L"divide",
		NULL
	};
	int pos;
	if ((pos=buffer.find('&'))==wstring::npos || (!iswalpha(buffer[pos+1]) && buffer[pos+1]!=L'#')) return;
	while (true)
	{
		if (buffer[pos+1]==L'#')
		{
			int end=pos+2;
			while (iswdigit(buffer[end]))
				end++;
			buffer.erase(pos,end-pos+1);
			pos=-1;
		}
		if (pos!=-1)
			for (unsigned int I=0; ce[I]; I++)
				if (!wcsncmp(buffer.c_str()+pos+1,ce[I],wcslen(ce[I])) && buffer[pos+1+wcslen(ce[I])]==';')
				{
					buffer.erase(pos,wcslen(ce[I])+1);
					buffer[pos]=ce[I][0];
					pos=-1;
					break;
				}
		if (pos!=-1)
			for (unsigned int I=0; sym[I]; I++)
				if (!wcsncmp(buffer.c_str()+pos+1,sym[I],wcslen(sym[I])) && buffer[pos+1+wcslen(sym[I])]==';')
				{
					buffer.erase(pos,wcslen(sym[I])+2);
					pos=-1;
					break;
				}
		if ((pos=buffer.find('&',pos+1))==wstring::npos || (!iswalpha(buffer[pos+1]) && buffer[pos+1]!=L'#')) return;
	}
}

void removeDots(wstring &str)
{ LFS
	int dot;
	while ((dot=str.find('·'))>=0)
		str.erase(dot,1);
	eliminateHTMLCharacterEntities(str);
}

/* examples
-ed/-ing/-s
*/
int getInflection(wstring sWord,wstring form,wstring mainEntry,wstring iform,vector <wstring> &allInflections)
{ LFS
	wchar_t *ch=(wchar_t *)iform.c_str();
	int inflection=1,chosenInflection=0;
	while (true)
	{
		wchar_t *next=wcschr(ch+1,'/'),extension[100];
		if (next) *next=0;
		wcscpy(extension,ch+((*ch=='-') ? 1 : 0));
		if (!wcscmp(extension,sWord.c_str()+sWord.length()-wcslen(extension))) chosenInflection=inflection;
		inflection++;
		wstring Inflection=mainEntry;
		if (chosenInflection)
		{
			if ((Inflection+extension)!=sWord)
			{
				Inflection=Inflection+Inflection[Inflection.length()-1];
				if ((Inflection+extension)!=sWord) Inflection=mainEntry;
				else
					lplog(LOG_DICTIONARY,L"Doubled letter in use of matching extension to word=%s MainEntry %s extension %s.",
					sWord.c_str(),mainEntry.c_str(),extension);
			}
		}
		if (extension[0]=='e' && mainEntry[mainEntry.length()-1]=='e')
			Inflection+=extension+1;
		// this doesn't always work (example: enjoy, enjoyed) - but sometimes does (bloomy,bloomier)
		else if (extension[0]=='e' && mainEntry[mainEntry.length()-1]=='y')
		{
			Inflection[Inflection.length()-1]='i';
			Inflection+=extension;
			if (sWord!=Inflection)
				Inflection=mainEntry+extension;
		}
		// also Lady -> Ladies
		else if (!wcscmp(extension,L"s") && mainEntry[mainEntry.length()-1]==L'y')
		{
			Inflection[Inflection.length()-1]=L'i';
			Inflection+=L"es";
			if (sWord!=Inflection)
				Inflection=mainEntry+extension;
		}
		else if (extension[0]==L'i' && mainEntry[mainEntry.length()-1]==L'e')
		{
			if (sWord!=Inflection+extension)
				Inflection.erase(Inflection.length()-1,Inflection.length());
			Inflection=Inflection+extension;
		}
		else if (!wcscmp(extension,L"s") && mainEntry[mainEntry.length()-1]==L's')
		{
			Inflection=Inflection+L"es";
		}
		else if (!wcscmp(extension,L"s") && (mainEntry[mainEntry.length()-1]==L'o' || mainEntry[mainEntry.length()-1]==L'h'))
		{
			Inflection+=L"es";
			if (sWord!=Inflection)
				Inflection=mainEntry+extension;
		}
		else
			Inflection+=extension;
		allInflections.push_back(Inflection);
		if (form==L"verb" && allInflections.size()==1) allInflections.push_back(Inflection); // past==past_participle
		if (!next) return chosenInflection;  // normal form?
		ch=next+1;
	}
	return chosenInflection;
}

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

Main Entry: en·vi·rons
Function: noun plural . . .

If the plural form is not always construed as a plural, the compactLabel continues with an applicable qualification:

Main Entry: ge·net·ics . . .
Function: noun plural but singular in construction . . .

Main Entry: pol·i·tics . . .
Function: noun plural but singular or plural in construction

Main Entry: math·e·mat·ics . . .
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

Main Entry: geni·i . . .
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
const wchar_t *alternates[]={
	L"<i>also",L"<i>or",L"<i>chiefly in",
};

bool equivalentIfIgnoreDashSpaceCase(wstring sWord,wstring sWord2)
{ LFS
	// convert acute
	int pos;
	if ((pos=sWord.find(L"&eacute;"))!=wstring::npos)
		sWord.replace(pos,8,L"e");
	int iW=0,iW2=0;
	while (true)
	{
		if (iW>0)
		{
			while (sWord[iW]==L'-' || sWord[iW]==L' ') iW++;
			while (sWord2[iW2]==L'-' || sWord2[iW2]==L' ') iW2++;
		}
		if (towlower(sWord[iW])!=towlower(sWord2[iW2])) return false;
		if (!sWord[iW]) return true;
		iW++; iW2++;
	}
	return false;
}

int cWord::checkAdd(const wchar_t * fromWhere,tIWMM &iWord,wstring sWord,int flags,wstring sForm,int inflection,int derivationRules,wstring definitionEntry,int sourceId,bool log)
{ LFS
	int iForm;
	vector <cForm *>::iterator ifc,ifcend=Forms.end();
	for (iForm=0,ifc=Forms.begin(); ifc!=ifcend && (*ifc)->name!=sForm; ifc++,iForm++);
	if (ifc==ifcend)
	{
		unsigned int chi;
		if (sForm.find(L"adjective")!=wstring::npos)
			sForm=L"adjective";
		else if (sForm.find(L"adverb")!=wstring::npos)
			sForm=L"adverb";
		else if ((chi=sForm.find(L"verb"))!=wstring::npos && (!sForm[chi+4] || iswspace(sForm[chi+4])))
			sForm=L"verb";
		else if (sForm.find(L"past part")!=wstring::npos)
			sForm=L"verb";
		else if (sForm.find(L"plural in construction")!=wstring::npos)
			sForm=L"noun";
		else if (sForm.find(L"exclamation")!=wstring::npos)  // from Cambridge
			sForm=L"interjection";
		else if (sForm.find(L"definite article")!=wstring::npos)
			sForm=L"determiner";
		else if (sForm.find(L"indefinite article")!=wstring::npos)
			sForm=L"quantifier";
		else if (sForm.find(L"verbal auxiliary")!=wstring::npos)
		{
			if (log)
			lplog(LOG_DICTIONARY,L"Form %s rejected!",sForm.c_str());
			return 0;
		}
	}
	while (sWord[sWord.length()-1]==' ') // strangely, all multi-word words in MW seem to have a space at the end
		sWord.erase(sWord.length()-1);
	while (definitionEntry[definitionEntry.length()-1]==' ') // crow
		definitionEntry.erase(definitionEntry.length()-1);
	wstring inflectionName;
	if (hasFormInflection(iWord,sForm,inflection)==WMM.end())
	{
		bool added;
		tIWMM saveIWord=iWord;
		iForm=addWordToForm(sWord,iWord,flags,sForm,sForm,inflection,derivationRules,definitionEntry,sourceId,added);
		if (saveIWord!=WMM.end()) iWord=saveIWord;
		if (added)
		{
			if (log)
			lplog(LOG_DICTIONARY,L"(%s) word %s: Added Inflection%s (Form %s, definitionEntry %s)",fromWhere,sWord.c_str(),getInflectionName(inflection,iForm,inflectionName),sForm.c_str(),definitionEntry.c_str());
			return 0;
		}
	}
	iForm=cForms::findForm(sForm);
	if (iForm<0)
	{
		if (log)
		lplog(LOG_DICTIONARY,L"form %s is not found!",sForm.c_str());
		return -1;
	}
	if (log)
	lplog(LOG_DICTIONARY,L"(%s) word %s: (Already) Added Inflection%s (Form %s, definitionEntry %s)",fromWhere,sWord.c_str(),getInflectionName(inflection,iForm,inflectionName),sForm.c_str(),definitionEntry.c_str());
	return 0;
}

// this was changed from standard read
// because Microsoft version of _read had a bug in it
int getPath(const wchar_t *pathname,void *buffer,int maxlen,int &actualLen)
{ LFS
	HANDLE hFile = CreateFile(pathname,    // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);                 // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{
		wstring bcct;
		if (GetLastError()!=ERROR_PATH_NOT_FOUND && GetLastError()!=ERROR_FILE_NOT_FOUND)
			lplog(LOG_ERROR,L"GetPath cannot open path %s - %s",pathname,getLastErrorMessage(bcct));
		return GETPATH_CANNOT_OPEN_PATH;
	}
	actualLen=GetFileSize(hFile,NULL);
	if (actualLen<0)
	{
		lplog(LOG_ERROR,L"ERROR:filelength of file %s yields an invalid filelength (%d).",pathname,actualLen);
		CloseHandle(hFile);
		return GETPATH_INVALID_FILELENGTH1;
	}
	if (actualLen==0)
	{
		CloseHandle(hFile);
		return 0;
	}
	if (actualLen+1>=maxlen)
	{
		lplog(LOG_ERROR,L"ERROR:filelength of file %s (%d) is greater than the maximum allowed (%d).",pathname,actualLen+1,maxlen);
		CloseHandle(hFile);
		return GETPATH_INVALID_FILELENGTH2;
	}
	DWORD lenRead=0;
	if (!ReadFile(hFile,buffer,actualLen,&lenRead,NULL) || actualLen!=lenRead)
	{
		lplog(LOG_ERROR,L"ERROR:read error of file %s.",pathname);
		CloseHandle(hFile);
		return GETPATH_INVALID_FILELENGTH2;
	}
	((char *)buffer)[lenRead]=0;
	((char *)buffer)[lenRead+1]=0;
	CloseHandle(hFile);
	return 0;
}

int cWord::splitWord(MYSQL *mysql,tIWMM &iWord,wstring sWord,int sourceId,bool log)
{ LFS
	if (sWord.length()<5) 
		return -1;
	static unordered_set <wstring> rejectSplitEndings = { L"o",L"ing",L"ton",L"rin",L"tin",L"pin",L"ism",L"ons",L"aire",L"ana",L"la",L"ard",L"ell",L"sey",L"ness",L"la",L"ies",
	L"ley",L"ers",L"ish",L"ner",L"ington",L"leys",L"que",L"tes",L"ion",L"in",L"els",L"era",L"ists",L"sie",L"and",L"ingly",L"ium",L"ics",L"ilate",L"issima",L"ells" };
	vector <wstring> components = splitString(sWord, '-');
	tIWMM iWordComponent=WMM.end();
	for (wstring w : components)
		if ((iWordComponent = fullQuery(mysql, w, sourceId)) == WMM.end())
			break;
	// don't split a word with a dash in it
	if (components.size()==1 && (iWordComponent == WMM.end() || !cStemmer::wordIsNotUnknownAndOpen(iWordComponent,log) || rejectSplitEndings.find(components[components.size() - 1]) != rejectSplitEndings.end())) // not found or unknown
	{
		for (unsigned int I = 2; I < sWord.length() - 2; I++)
		{
			components.clear();
			wstring firstWord = sWord.substr(0, I);
			components.push_back(sWord.substr(I, sWord.length() - I));
			tIWMM firstQIWord;
			// with splitting word this way, the previous word must also be known and of an open word type. 
			if (((firstQIWord = fullQuery(mysql, firstWord, sourceId)) != WMM.end() && cStemmer::wordIsNotUnknownAndOpen(firstQIWord,log)) &&
				((iWordComponent = fullQuery(mysql, components[components.size() - 1], sourceId)) != WMM.end() && cStemmer::wordIsNotUnknownAndOpen(iWordComponent,log)) &&
				(rejectSplitEndings.find(components[components.size() - 1]) == rejectSplitEndings.end()))
					break;
			iWordComponent = WMM.end();
		}
	}
	if (iWordComponent != WMM.end() && cStemmer::wordIsNotUnknownAndOpen(iWordComponent,log) && rejectSplitEndings.find(components[components.size() - 1]) == rejectSplitEndings.end())
	{
		// (SW) word bone-cracking( main: verb present part)
		// (SW) word white-maned(main: verb past)
		// (SW) word micro-electric( main: adjective)
		// (SW) word half-wittingly(main: adverb)
		// (SW) word arms-sales(main: noun)
		iWord=end();
		if (iWordComponent->second.query(verbForm) >= 0)
			checkAdd(L"SW", iWord, sWord, 0, L"verb", iWordComponent->second.inflectionFlags, 0, components[components.size()-1], sourceId,log);
		if (iWordComponent->second.query(nounForm)>=0)
			checkAdd(L"SW",iWord,sWord,0,L"noun", iWordComponent->second.inflectionFlags,0, components[components.size() - 1],sourceId, log);
		if (iWordComponent->second.query(adjectiveForm)>=0)
			checkAdd(L"SW",iWord,sWord,0,L"adjective", iWordComponent->second.inflectionFlags,0, components[components.size() - 1],sourceId, log);
		if (iWordComponent->second.query(adverbForm)>=0)
			checkAdd(L"SW",iWord,sWord,0,L"adverb", iWordComponent->second.inflectionFlags,0, components[components.size() - 1],sourceId, log);
		if (iWord==end()) return -1;
		checkAdd(L"SW",iWord,sWord,0,COMBINATION_FORM,0,0, components[components.size() - 1],sourceId, log);
		lplog(LOG_DICTIONARY, L"WordPosMAP splitWord %s-->%s", sWord.c_str(), components[components.size() - 1].c_str());
		lplog(LOG_DICTIONARY, L"%s TEMPWordPosMAP", components[components.size() - 1].c_str());
		return 0;
	}
	return -1;
}

bool loosesort( const wchar_t *s1, const wchar_t *s2 )
{ LFS
	if (*s1 && *s2) return wcscmp(s1,s2)<0;
	if (*s1) 
		return wcsncmp(s1,s2+1,wcslen(s1))<0;
	return wcsncmp(s1+1,s2,wcslen(s2))<0;
}

string lookForPOS(string originalWord,yajl_val node,bool logEverything,int &inflection,string &referWord)
{
	inflection = 0;
	// check identity to make sure word matches
	const char * metaPath[] = { "meta", "id", (const char *)0 };
	yajl_val metaIdDocs = yajl_tree_get(node, metaPath, yajl_t_any);
	string id = (metaIdDocs == NULL) ? "" : YAJL_GET_STRING(metaIdDocs);
	if (metaIdDocs == NULL && logEverything)
		lplog(LOG_INFO, L"WAPI %S:meta id not found", originalWord.c_str());
	transform(id.begin(), id.end(), id.begin(), ::tolower);
	int whereColon = id.find(':');
	if (whereColon != string::npos)
		id = id.substr(0, whereColon);
	if (metaIdDocs == NULL || originalWord == id)
	{
		const char * flPathMain[] = { "fl", (const char *)0 };
		yajl_val flDocs = yajl_tree_get(node, flPathMain, yajl_t_any);
		if (logEverything)
			lplog(LOG_INFO, L"WAPI %S:fl %S", originalWord.c_str(),(flDocs == NULL) ? "not found": YAJL_GET_STRING(flDocs));
		if (flDocs != NULL) return YAJL_GET_STRING(flDocs);
		const char * cxsPath[] = { "cxs", "cxtis","cxt", (const char *)0 };
		yajl_val cxsIdDocs = yajl_tree_get(node, cxsPath, yajl_t_any);
		string cxs = (cxsIdDocs == NULL) ? "" : YAJL_GET_STRING(cxsIdDocs);
		if (cxsIdDocs != NULL && logEverything)
			lplog(LOG_INFO, L"WAPI %S:cxs refer %s", cxs.c_str());
		referWord = cxs;
		return "";
	}
	// main meta entry does not match the originalWord.
	// try ins (inflections)
	const char * inflectionsPath[] = { "ins", (const char *)0 };
	yajl_val insDocs = yajl_tree_get(node, inflectionsPath, yajl_t_array);
	if (insDocs)
	{
		for (unsigned int docNum = 0; docNum < insDocs->u.array.len; docNum++)
		{
			yajl_val doc = insDocs->u.array.values[docNum];
			const char * ifPath[] = { "if", (const char *)0 };
			yajl_val ifDocs = yajl_tree_get(doc, ifPath, yajl_t_any);
			if (ifDocs == NULL)
				continue;
			string ifElement =  YAJL_GET_STRING(ifDocs);
			ifElement.erase(std::remove(ifElement.begin(), ifElement.end(), L'*'), ifElement.end());
			transform(ifElement.begin(), ifElement.end(), ifElement.begin(), ::tolower);
			if (ifElement == originalWord)
			{
				const char * ilPath[] = { "il", (const char *)0 };
				yajl_val ilDocs = yajl_tree_get(doc, ilPath, yajl_t_any);
				if (ilDocs == NULL)
					continue;
				const char * flPathMain[] = { "fl", (const char *)0 };
				yajl_val flinsDocs = yajl_tree_get(node, flPathMain, yajl_t_any);
				string flinsForm = (flinsDocs == NULL) ? "" : YAJL_GET_STRING(flinsDocs);
				string ilElement = YAJL_GET_STRING(ilDocs);
				if (ilElement == "plural" || ((ilElement == "or" || ilElement == "also") && flinsForm == "noun"))
					inflection = 1;
				else if (logEverything)
					lplog(LOG_INFO, L"WAPI %S:Unknown inflection type %S", originalWord.c_str(), ilElement.c_str());
				if (logEverything)
					lplog(LOG_INFO, L"WAPI %S:INS fl %S%s", originalWord.c_str(), flinsForm.c_str(),(inflection==1) ? L" plural":L"");
				return flinsForm;
			}
		}
	}
	// now try run-ons
	const char * runonPath[] = { "uros", (const char *)0 };
	yajl_val urosDocs = yajl_tree_get(node, runonPath, yajl_t_array);
	if (urosDocs)
		for (unsigned int docNum = 0; docNum < urosDocs->u.array.len; docNum++)
		{
			yajl_val doc = urosDocs->u.array.values[docNum];
			const char * urePath[] = { "ure", (const char *)0 };
			yajl_val ureDocs = yajl_tree_get(doc, urePath, yajl_t_any);
			string ure = (ureDocs == NULL) ? "" : YAJL_GET_STRING(ureDocs);
			ure.erase(std::remove(ure.begin(), ure.end(), L'*'), ure.end());
			transform(ure.begin(), ure.end(), ure.begin(), ::tolower);
			if (ure == originalWord)
			{
				const char * flPathUre[] = { "fl", (const char *)0 };
				yajl_val flDocs = yajl_tree_get(doc, flPathUre, yajl_t_any);
				if (logEverything)
					lplog(LOG_INFO, L"WAPI %S:UROS fl %S", originalWord.c_str(), (flDocs == NULL) ? "not found" : YAJL_GET_STRING(flDocs));
				return (flDocs == NULL) ? "" : YAJL_GET_STRING(flDocs);
			}
			else
				if (logEverything)
					lplog(LOG_INFO, L"WAPI %S:UROS ure %S [not match]", originalWord.c_str(), ure.c_str());
		}
	if (logEverything)
	{
		wstring wid;
		lplog(LOG_INFO, L"WAPI %S:meta id %s originalWord %s uros %s ins %s", originalWord.c_str(),
			(metaIdDocs == NULL) ? L"not found" : mTW(id, wid),
			(originalWord == id) ? L"matched" : L"not matched",
			(urosDocs) ? L"found" : L"not found",
			(insDocs) ? L"found" : L"not found"
		);
	}
	return "";
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
vector<wstring> ignoreBefore = { L"pronunciation spelling" };
vector<wstring> classes = { L"adjective",L"adverb",L"verb",L"noun",L"interjection",L"abbreviation",L"symbol",L"preposition",L"conjunction",L"trademark",L"pronoun",L"honorific" };
vector<wstring> ignoreAfter = { L"prefix",L"suffix",L"phrase",L"saying",L"quotation",L"pronunciation spelling",L"script annotation",L"combining form",L"contraction",L"indefinite article",L"definite article" }; // must be processed after classes

void identifyFormClass(set<int> &posSet, wstring pos, bool &plural)
{
		//investigate sidgwick should never have reached splitWord!

	plural = pos.find(L"noun") != wstring::npos && pos.find(L"plural") != wstring::npos;
	if (pos == L"idiom")
	{
		posSet.insert(cForms::gFindForm(L"noun"));
		posSet.insert(cForms::gFindForm(L"adjective"));
	}
	else if (pos.find(L"name") != wstring::npos)
	{
		posSet.insert(PROPER_NOUN_FORM_NUM);
	}
	else if (pos == L"certification mark" || pos == L"service mark")
	{
		posSet.insert(cForms::gFindForm(L"symbol"));
	}
	else if (pos.find(L"communications") != wstring::npos)
	{
		posSet.insert(cForms::gFindForm(L"noun"));
	}
	for (auto c : ignoreBefore)
		if (pos.find(c.c_str()) != wstring::npos)
			return;
	for (auto c : classes)
	{
		int where;
		if ((where = pos.find(c.c_str())) != wstring::npos)
		{
			// this is enough to differentiate between noun, pronoun, verb and adverb
			if (where == 0 || iswspace(pos[where - 1]))
				posSet.insert(cForms::gFindForm(c.c_str()));
		}
	}
	for (auto c : ignoreAfter)
		if (pos.find(c.c_str()) != wstring::npos)
			return;
	if (posSet.empty())
		printf("form not recognized - %S\n", pos.c_str());
}

// returns false if not found by the site (or error)
bool existsInDictionaryDotCom(MYSQL *mysql,wstring word, bool &networkAccessed)
{
	if (word.length() <= 2 || word.length()>31)
		return false;
	//initializeDatabaseHandle(mysql, L"localhost", alreadyConnected);
	MYSQL_RES * result;
	_int64 numResults = 0;
	wchar_t qt[1024];
	wchar_t path[1024];
	path[0] = '_';
	wcscpy(path+1, word.c_str());
	convertIllegalChars(path + 1);
	_snwprintf(qt, 1024, L"select 1 from notwords where word = '%s'", path);
	if (!myquery(mysql, L"LOCK TABLES notwords READ"))
		return false;
	if (myquery(mysql, qt, result))
	{
		numResults = mysql_num_rows(result);
		mysql_free_result(result);
	}
	if (!myquery(mysql, L"UNLOCK TABLES"))
		return false;
	//lplog(LOG_INFO, L"*** existsInDictionaryDotCom: statement %s resulted in numRows=%d.", qt, numResults);
	if (numResults > 0)
		return false;

	wstring buffer,diskPath;
	if (cInternet::cacheWebPath(L"https://www.dictionary.com/browse/" + word, buffer, word, L"DictionaryDotCom", false, networkAccessed,diskPath))
		return false;
	if ((networkAccessed && cInternet::redirectUrl.find(L"noresults")!=wstring::npos) || 
		  buffer.find(L"No results found") != wstring::npos || 
		  buffer.find(L"dcom-no-result") != wstring::npos ||
			buffer.find(L"dcom-misspell") != wstring::npos)
	{
		if (!myquery(mysql, L"LOCK TABLES notwords WRITE"))
			return false;
		wsprintf(qt, L"INSERT INTO notwords VALUES ('%s')", path);
		myquery(mysql, qt, true);
		_wremove(diskPath.c_str());
		if (!myquery(mysql, L"UNLOCK TABLES"))
			return false;
		return false;
	}
	return true;
}

bool detectNonEuropeanWord(wstring word)
{
	char temptransbuf[1024];
	BOOL usedDefaultChar;
	int ret = WideCharToMultiByte(
		1252,									//UINT CodePage,
		WC_NO_BEST_FIT_CHARS,//DWORD dwFlags,
		word.c_str(),				 //LPCWCH lpWideCharStr,
		word.length(),			 //int cchWideChar,
		temptransbuf,              //LPSTR lpMultiByteStr,
		1024,           // int cbMultiByte,
		NULL,                //LPCCH lpDefaultChar,
		&usedDefaultChar										 //LPBOOL lpUsedDefaultChar
	);
	return (ret == 0) || (usedDefaultChar);
	//if (!ret)
	//	wprintf(L"%s %d\n", word.c_str(), GetLastError());
	//else if (usedDefaultChar)
	//	wprintf(L"%s UDC\n", word.c_str());
	//else
	//	return false;
	//return true;
}

bool getMerriamWebsterDictionaryAPIForms(wstring sWord, set <int> &posSet, bool &plural, bool &networkAccessed,bool logEverything)
{
	wstring pageURL = L"https://www.dictionaryapi.com/api/v3/references/collegiate/json/";
	pageURL += sWord + L"?key=ba4ac476-dac1-4b38-ad6b-fe36e8416e07";
	wstring jsonWideBuffer, diskPath;
	if (!cInternet::cacheWebPath(pageURL, jsonWideBuffer, sWord, L"Webster", false, networkAccessed,diskPath))
	{
		if (jsonWideBuffer.find(L'{') == wstring::npos)
		{
			//if (logEverything)
			//	lplog(LOG_INFO, L"WAPI %s:API returned list [not found]",sWord.c_str());
			return false;
		}
		char errbuf[1024];
		errbuf[0] = 0;
		string jsonBuffer;
		wTM(jsonWideBuffer, jsonBuffer);
		string originalWord;
		wTM(sWord, originalWord);
		yajl_val node = yajl_tree_parse((const char *)jsonBuffer.c_str(), errbuf, sizeof(errbuf));
		/* parse error handling */
		if (node == NULL) {
			lplog(LOG_ERROR, L"Parse error:%s\n %S", jsonBuffer.c_str(), errbuf);
			return false;
		}
		wstring posStr, pos;
		if (node->type == yajl_t_array)
			for (unsigned int docNum = 0; docNum < node->u.array.len; docNum++)
			{
				yajl_val doc = node->u.array.values[docNum];
				int inflection;
				string referWord;
				mTW(lookForPOS(originalWord, doc,logEverything,inflection,referWord), pos);
				if (pos.length() > 0)
				{
					identifyFormClass(posSet, pos, plural);
					plural |= (inflection == 1);
				}
				else if (referWord.length() > 0)
				{
					wstring wReferWord;
					getMerriamWebsterDictionaryAPIForms(mTW(referWord,wReferWord), posSet, plural, networkAccessed, logEverything);
				}
			}
	}
	return posSet.size() > 0;
}

// pass back these inflections:
int discoverInflections(set <int> posSet, bool plural, wstring word)
{
	int inflections=0;
	for (auto c : posSet)
	{
		if (c == nounForm)
		{
			if (plural)
				inflections += PLURAL;
			else
				inflections += SINGULAR;
		}
		else if (c == verbForm)
		{
			if (word.length() > 2)
			{
				if (word[word.length() - 3] == L'i' && word[word.length() - 2] == L'n' && word[word.length() - 1] == L'g')
					inflections += VERB_PRESENT_PARTICIPLE;
				else if (word[word.length() - 2] == L'e' && word[word.length() - 1] == L'd')
					inflections += VERB_PAST_PARTICIPLE;
				else if (word[word.length() - 1] == L's')
					inflections += VERB_PRESENT_THIRD_SINGULAR; //?? guess
				else
					inflections += VERB_PRESENT_FIRST_SINGULAR;
			}
		}
		else if (c == adjectiveForm)
		{
			// ADJECTIVE_NORMATIVE = 8192, ADJECTIVE_COMPARATIVE = 16384,ADJECTIVE_SUPERLATIVE = 32768,
			if (word.length() < 3)
				inflections += ADJECTIVE_NORMATIVE;
			else
			{
				if (word[word.length() - 2] == L'e' && word[word.length() - 1] == L'r')
					inflections += ADJECTIVE_COMPARATIVE;
				else if (word[word.length() - 3] == L'e' && word[word.length() - 2] == L's' && word[word.length() - 1] == L't')
					inflections += ADJECTIVE_SUPERLATIVE;
				else
					inflections += ADJECTIVE_NORMATIVE;
			}
		}
		else if (c == adverbForm)
		{
			// ADVERB_NORMATIVE = 65536, ADVERB_COMPARATIVE = 131072,ADVERB_SUPERLATIVE = 262144
			if (word.length() < 3)
				inflections += ADVERB_NORMATIVE;
			else
			{
				if (word[word.length() - 2] == L'e' && word[word.length() - 1] == L'r')
					inflections += ADVERB_COMPARATIVE;
				else if (word[word.length() - 3] == L'e' && word[word.length() - 2] == L's' && word[word.length() - 1] == L't')
					inflections += ADVERB_SUPERLATIVE;
				else
					inflections += ADVERB_NORMATIVE;
			}
		}
	}
	return 0;
}

bool cWord::illegalWord(MYSQL *mysql, wstring sWord)
{
	// non English word?
	if (detectNonEuropeanWord(sWord) || sWord.find_first_of(L"ãâäáàæçêéèêëîíïñôóòöõôûüùú") != wstring::npos)
		return true;
	// embedded quote?
	size_t whereQuote = sWord.find('\'');
	if (whereQuote != wstring::npos && whereQuote > 0 && whereQuote < sWord.length() - 1)
		return true;
	// check dictionary.com for a sanity check
	bool networkAccessed;
	if (!existsInDictionaryDotCom(mysql, sWord, networkAccessed))
		return true;
	return false;
}

// this routine should look up words from wiktionary or some other dictionary
// this returns >0 if word is found or WORD_NOT_FOUND if word lookup fails.
int cWord::getForms(MYSQL *mysql, tIWMM &iWord, wstring sWord, int sourceId,bool logEverything)
{
	LFS
	if (illegalWord(mysql, sWord))
		return WORD_NOT_FOUND;
	changedWords=true;
	bool plural, networkAccessed;
	// check webster for a list of the forms (because of their API)
	set <int> posSet;
	if (!getMerriamWebsterDictionaryAPIForms(sWord, posSet, plural, networkAccessed,logEverything))
		return WORD_NOT_FOUND;
	// discover common inflections for each open word class
	int inflections = discoverInflections(posSet, plural, sWord);
	int flags = 0, derivationRules = 0;
	bool added;
	wstring sME = sWord;
	for (int form : posSet)
		iWord = addNewOrModify(mysql, sWord, flags, form, inflections, derivationRules, sME, sourceId, added);
	return (iWord == Words.end()) ? WORD_NOT_FOUND : posSet.size();
}

const wchar_t *getLastErrorMessage(wstring &out)
{ LFS
	wchar_t msg[10000]; 
	DWORD dw = GetLastError();
	FormatMessage( 
		FORMAT_MESSAGE_FROM_SYSTEM 
		| FORMAT_MESSAGE_IGNORE_INSERTS // don't process inserts
    | FORMAT_MESSAGE_FROM_HMODULE,  // retrieve message from specified DLL
		GetModuleHandle(L"wininet.dll"),dw,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPTSTR) &msg,10000, NULL );
	int len;
	if (msg[len=wcslen(msg)-1]=='\n') msg[len]=0;
	if (msg[len=wcslen(msg)-1]=='\r') msg[len]=0;
	if (dw==ERROR_INTERNET_EXTENDED_ERROR)
	{
		len=wcslen(msg);
		DWORD remainingBufferLen=10000-len;
		InternetGetLastResponseInfo(&dw,msg+len,&remainingBufferLen);
	}
	out=msg;
	return out.c_str();
}



#ifdef CHECK_WORD_CACHE

int cWord::checkWord(cWord &Words2,tIWMM originalIWord,tIWMM newWord,int ret)
{ LFS
	int wait=0;
	if (ret==WORD_NOT_FOUND || newWord==WMM.end()) return WORD_NOT_FOUND;
	if (ret)
		while (wait) Sleep(1000);
	// check for mainEntry
	if (originalIWord->first==newWord->first && originalIWord->second==newWord->second)
	{
		unsigned int *forms=newWord->second.forms();
		int count=newWord->second.formsSize(),I=0;
		for (; I<count && !Forms[forms[I]]->inflectionsClass.length(); I++);
		if (I==count || (I<count && newWord->second.mainEntry!=(tIWMM) NULL)) return 0;
		lplog(L"Word %s has a NULL main entry.",newWord->first.c_str());
	}
	lplog(L"name %s differs.",originalIWord->first.c_str());
	while (wait) Sleep(1000);
	return 0;
}

//#define TEST_SPECIFIC
wchar_t *unknowns[]={L"countermarches",NULL,
 "ageist", 
NULL };

bool endStringMatch(const wchar_t *str,wchar_t *endMatch)
{ LFS
	return !wcscmp(str+wcslen(str)-wcslen(endMatch),endMatch);
}

#include "wn.h"

bool getWNForms(wstring w,vector <int> &WNForms)
{ LFS
		if (checkexist((wchar_t *)w.c_str(),NOUN)) WNForms.push_back(nounForm);
		if (checkexist((wchar_t *)w.c_str(),VERB))  WNForms.push_back(verbForm);
		if (checkexist((wchar_t *)w.c_str(),ADJ))  WNForms.push_back(adjectiveForm);
		if (checkexist((wchar_t *)w.c_str(),ADV)) WNForms.push_back(adverbForm);
		return WNForms.size()>0;
}

#endif

// filter out everything but nouns
// filter out all {{ links }}
// filter out any entry having nothing but a {{ link }} after the # sign
// delete left and right brackets
// English	zymosterol	Noun	# {{biochemistry}} A [[cholesterol]] [[intermediate]].
// create table wiktionaryNouns  ( noun char(56) COLLATE utf8mb4_bin NOT NULL,definition TEXT(1024) COLLATE utf8mb4_bin NOT NULL ) ENGINE=MyISAM AUTO_INCREMENT=798922 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
// LOAD DATA INFILE MAINDIR+'\\Linguistics information\\TEMP-E20120211.nounsOnly.tsv' INTO TABLE wiktionaryNouns FIELDS TERMINATED BY ',' ENCLOSED BY '"' LINES TERMINATED BY '\n';
void extractFromWiktionary(wchar_t *filename)
{ LFS
	FILE *listfile=_wfopen(filename,L"rb"); // binary mode reads unicode
	wchar_t outputName[2048];
	wcscpy(outputName,filename);
	wcscat(outputName,L".tsv");
	unsigned int maxNoun=0,maxDefinition=0;
	FILE *outputFile=_wfopen(outputName,L"wb"); // binary mode reads unicode
	if (listfile)
	{
		char url[2048];
		while (fgets(url,2047,listfile))
		{
			if (url[strlen(url)-1]==L'\n') url[strlen(url)-1]=0;
			if (url[strlen(url)-1]==L'\r') url[strlen(url)-1]=0;
			char *ch=strchr(url,'#'),*e=strstr(url,"English"),*n=strstr(url,"Noun");
			if (!ch || !e || !n || e>n || n>ch || e>ch) continue;
			for (e+=strlen("English"); isspace((unsigned char)*e); e++);
			for (n--; n>e && isspace((unsigned char)*n); n--);
			n[1]=0;
			if (strlen(e)<2) continue;
			string word=e;
			int I=0,J=(int)(ch+1-url);
			while (isspace((unsigned char)url[J])) J++;
			for (; url[J]; J++)
			{
				if (url[J]=='{')
				{
					while (url[J]!='}' && url[J])
						J++;
					if (url[J]=='}') J++;
					if (url[J]=='}') J++;
				}
				if (url[J]=='[' || url[J]==']' || (isspace((unsigned char)url[J]) && isspace((unsigned char)url[J+1]))) continue;
				if (url[J]=='"') url[J]='\'';
				if (J>I) url[I++]=url[J];
			}
			url[I]=0;
			if (!url[0] || (isspace((unsigned char)url[0]) && !url[1])) continue;
			fprintf(outputFile,"\"%s\",\"%s\"\n",word.c_str(),isspace((unsigned char)url[0]) ? url+1:url);
			maxNoun=max(maxNoun,word.length()+2);
			maxDefinition=max(maxDefinition,strlen(url+2));
		}
		fclose(listfile);
		fclose(outputFile);
		exit(0);
	}
}

