#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "profile.h"

void defineNames(void)
{
	LFS
		//cPattern *p=NULL;
		cPattern::create(L"__NAMEINTRO{PLURAL}", L"1", 2, L"honorific{HON}", L"_HON_ABB{HON}", NO_OWNER, 1, 1,
			1, L"and", 0, 1, 1,
			2, L"honorific{HON}", L"_HON_ABB{HON}", NO_OWNER, 1, 1,
			2, L"honorific{HON2}", L"_HON_ABB{HON2}", NO_OWNER, 0, 1,
			2, L"honorific{HON3}", L"_HON_ABB{HON3}", NO_OWNER, 0, 1,
			0);
	cPattern::create(L"__NAMEINTRO{SINGULAR}", L"2",
		2, L"honorific*-1{HON}", L"_HON_ABB*-1{HON}", NO_OWNER, 1, 1, // encourages __NAMEINTRO and not _NAME"H"
		2, L"honorific{HON2}", L"_HON_ABB{HON2}", NO_OWNER, 0, 1,
		2, L"honorific{HON3}", L"_HON_ABB{HON3}", NO_OWNER, 0, 1,
		0);
	// Dr. Helen Billows Mirren // Cornelius de Witt / Helen Mirren
	// The Rev. Dr. Bartholomew
	cPattern::create(L"__NAME", L"1",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"Proper Noun{FIRST}", NO_OWNER, 1, 2,
		6, L"determiner|le{MIDDLE}", L"preposition|de{MIDDLE}", L"noun|van{MIDDLE}", L"noun|von{MIDDLE}", L"_ABB{MIDDLE}", L"letter{MIDDLE}", NO_OWNER, 0, 1,
		//1,L".",0,0,1, // included by _ABB
		1, L"Proper Noun{LAST}", NO_OWNER, 1, 2,
		0); // noun removed
// Dr. Cornelius
	cPattern::create(L"__NAME", L"2",
		1, L"__NAMEINTRO", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"letter{LAST}", NO_OWNER, 1, 1,
		0);
	// M. de Louvois / M. Louvois
	cPattern::create(L"__NAME", L"3",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 0, 1,
		4, L"determiner|le{MIDDLE}", L"preposition|de{MIDDLE}", L"noun|van{MIDDLE}", L"noun|von{MIDDLE}", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*2{LAST}", NO_OWNER, 1, 2, 0); // Names ending in a noun should be not common

// D' Artagnan / O'Malley / d'artagnan
	cPattern::create(L"__NAME", L"4",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		1, L"letter{LAST2}", 0, 1, 1,
		1, L"quotes", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*1{LAST}", NO_OWNER, 1, 1, 0); // Names ending in a noun should be not common
// M. A.
	cPattern::create(L"__NAME", L"5",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 1, 1,
		1, L"letter{LAST}", 0, 1, 1,
		1, L".", 0, 1, 1,
		0);

	// M. A. L.
	cPattern::create(L"__NAME", L"6",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 1, 1,
		1, L"letter{MIDDLE}", 0, 1, 1,
		1, L".", 0, 1, 1,
		1, L"letter{LAST}", 0, 1, 1,
		1, L".", 0, 1, 1,
		0);

	// Monsieur le Pen / Monsieur le compte / Eamon de Valera // Covento de San -Francisco
	cPattern::create(L"__NAME", L"9",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		4, L"determiner|le{MIDDLE}", L"preposition|de{MIDDLE}", L"noun|van{MIDDLE}", L"noun|von{MIDDLE}", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*1{LAST}", NO_OWNER, 1, 2, 0); // Names ending in a noun should be not common

// M. D' Artagnan / K. O'Malley / M. d'artagnan
	cPattern::create(L"__NAME", L"A",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 0, 1,
		1, L"Proper Noun{MIDDLE}", NO_OWNER, 0, 1,
		1, L"letter{LAST2}", 0, 1, 1,
		1, L"quotes", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*1{LAST}", NO_OWNER, 1, 1, 0);  // Names ending in a noun should be not common
	cPattern::create(L"__NAME", L"B", 1, L"Proper Noun{SINGULAR:ANY}", NO_OWNER, 1, 1, 0);
	// Alan A.
	cPattern::create(L"__NAME", L"C", 1, L"Proper Noun{SINGULAR:FIRST}", NO_OWNER, 1, 1,
		1, L"letter{LAST}", 0, 1, 1,
		1, L".", 0, 1, 1,
		0);
	// M. A. Wycliffe, M A Wycliff, A Wycliff
	cPattern::create(L"__NAME", L"D",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 0, 1,
		1, L"letter{MIDDLE}", 0, 0, 1,
		1, L".", 0, 0, 1,
		1, L"Proper Noun{LAST}", NO_OWNER, 1, 1,
		0);
	// B.A. Summa cum laude
	cPattern::create(L"__NAME", L"DEGREESCL",
		1, L"letter*-2", 0, 1, 1,
		1, L".", 0, 0, 1,
		1, L"letter*-2", 0, 1, 1,
		1, L".", 0, 0, 1,
		2, L"noun|summa*-1", L"noun|magna*-1", 0, 0, 1,
		1, L"adjective|cum", 0, 1, 1,
		1, L"adjective|laude", 0, 1, 1,
		0);

	// Mr. --
	cPattern::create(L"__NAME", L"E",
		2, L"honorific{SINGULAR:HON}", L"_HON_ABB{SINGULAR:HON}", 0, 1, 1, // if this is made optional, -- will always match __NAME, which is often not correct
		1, L"--", 0, 1, 1,
		0);

	// Bishop Manuel de Mollinedo y Angulo
	cPattern::create(L"__NAME", L"F",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 0, 1,
		1, L"Proper Noun{MIDDLE}", NO_OWNER, 0, 1,
		1, L"letter{LAST2}", 0, 1, 1,
		1, L"quotes", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*1{LAST}", NO_OWNER, 1, 1, 0);  // Names ending in a noun should be not common

	cPattern::create(L"_NAME{NAME}", L"1", 1, L"__NAME", 0, 1, 1, 0);

	// Sir Cornelius de Witt the fifth
	cPattern::create(L"_NAME{NAME}", L"7", // '7' keeps with the numbering of __NAME
		1, L"__NAME", 0, 1, 1,
		1, L"determiner", 0, 1, 1,
		1, L"numeral_ordinal{SUFFIX}", 0, 1, 1, 0);
	// Helen Mirren III.
	cPattern::create(L"_NAME{NAME}", L"8", // '8' keeps with the numbering of __NAME
		1, L"__NAME", 0, 1, 1,
		1, L"roman_numeral{SUFFIX}", 0, 1, 1,
		1, L".", 0, 0, 1, 0);

	cPattern::create(L"_NAME{NAME}", L"G",
		1, L"Proper Noun", NO_OWNER, 1, 3,
		2, L"business_abbreviation*-1{BUS}", L"_BUS_ABB*-1{BUS}", 0, 1, 1,
		0);
	cPattern::create(L"_NAME{NAME}", L"H",
		2, L"honorific{SINGULAR:HON}", L"_HON_ABB{SINGULAR:HON}", NO_OWNER, 1, 1,
		0);
	// The RMS Lusitania was a luxury ocean liner.
	cPattern::create(L"_NAME", L"K",
		1, L"abbreviation*1", ONLY_CAPITALIZED, 1, 1,
		1, L"Proper Noun{ANY}", NO_OWNER, 1, 1,
		0);
	// Number Fourteen, please close the door.
	cPattern::create(L"_NAME{NAME}", L"Q",
		1, L"Proper Noun|number", 0, 1, 1,
		3, L"Number*-3{ANY}", L"roman_numeral*-3{ANY}", L"numeral_cardinal*-3{ANY}", NO_OWNER, 1, 1,
		0);
	// B-17
	cPattern::create(L"_NAME{NAME:SINGULAR}", L"R",
		1, L"letter", 0, 1, 1,
		1, L"dash*-2", 0, 0, 1,
		1, L"Number", NO_OWNER, 1, 2,
		0);

	// the name " Rita "
	cPattern::create(L"_NAME{NAME}", L"M",
		1, L"determiner|the", 0, 1, 1,
		1, L"noun|name", 0, 1, 1,
		1, L"quotes", OPEN_INFLECTION, 1, 1,
		1, L"_NAME*-3", 0, 1, 1, // quoted nouns should be rare in general
		2, L",", L".", 0, 0, 1,
		1, L"quotes", CLOSE_INFLECTION, 1, 1, 0);
	// ISBN 0-393-07101-4
	cPattern::create(L"_ISBN", L"",
		1, L"abbreviation|isbn", 0, 1, 1,
		1, L"number", 0, 1, 1,
		1, L"dash|-", 0, 0, 1,
		1, L"number", 0, 1, 1,
		1, L"dash|-", 0, 0, 1,
		1, L"number", 0, 1, 1,
		1, L"dash|-", 0, 0, 1,
		1, L"number", 0, 1, 1,
		0);

	// Mrs. Pinkerton, 
	cPattern::create(L"_LINE1ADDRESS", L"",
		1, L"_NAME{SUBOBJECT}", 0, 1, 1,
		1, L",", 0, 1, 1,
		0);
	// 23 Beekam St. / 318 St -Paul's -Road
	cPattern::create(L"_LINE2ADDRESS", L"",
		1, L"Number", 0, 1, 1,
		2, L"Proper Noun", L"_NAME*1", 0, 1, 1,
		2, L"_STREET_ABB", L"street_address", 0, 1, 1,
		0);
	// Nilam, Nebraska, 19807
	cPattern::create(L"_LINE3ADDRESS", L"1",
		1, L"_NAME", 0, 1, 1, // "US city town village"
		1, L",", 0, 1, 1,
		1, L"Proper Noun", NO_OWNER, 1, 1, // US state territory region
		1, L",", 0, 0, 1,
		1, L"Number", 0, 1, 1,
		0);
	// London E 9 6 QP
	cPattern::create(L"_LINE3ADDRESS", L"2",
		1, L"_NAME", 0, 1, 1, // canadian province city",L"world city town village
		1, L"letter", 0, 1, 1,
		1, L"Number", 0, 1, 1,
		1, L"Number", 0, 1, 1,
		1, L"Proper Noun", NO_OWNER, 1, 1,
		0);
	// p.o. box 3
	cPattern::create(L"_POADDRESS", L"4",
		1, L"abbreviation|p.o.", 0, 1, 1,
		1, L"noun|box", 0, 1, 1,
		1, L"Number", 0, 1, 1,
		0);
	// P O Box 3
	cPattern::create(L"_POADDRESS", L"5",
		1, L"letter|p", 0, 1, 1,
		1, L"letter|o", 0, 1, 1,
		1, L"noun|box", 0, 1, 1,
		1, L"Number", 0, 1, 1,
		0);
	cPattern::create(L"_MADDRESS", L"",
		1, L"_LINE1ADDRESS", 0, 1, 1,
		2, L"_LINE2ADDRESS", L"_POADDRESS", 0, 1, 1,
		1, L",", 0, 1, 1,
		2, L"_LINE3ADDRESS", L"_NAME", 0, 1, 1,
		0);
	// Dave and Jen's place / S & P
	cPattern::create(L"__NAMEOWNER{PLURAL:NAMEOWNER}", L"8",
		1, L"_ADVERB", 0, 0, 1,
		1, L"Proper Noun{FIRST}", 0, 1, 1,
		2, L"coordinator", L"&", 0, 1, 1,
		2, L"noun{FIRST}", L"Proper Noun{FIRST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	// Dr. Helen Billows Mirren's
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"1",
		1, L"_ADVERB", 0, 0, 1,
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"Proper Noun{FIRST}", NO_OWNER, 1, 2,
		2, L"noun{MIDDLE}", L"_ABB{MIDDLE}", NO_OWNER, 0, 1,
		1, L"Proper Noun{LAST}", SINGULAR_OWNER, 1, 1, 0);

	// Dr. Mirren's
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"2",
		1, L"_ADVERB", 0, 0, 1,
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"Proper Noun{LAST:ANY}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1, 0);
	// M. de Louvois / M. Louvois
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"3",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 0, 1,
		3, L"determiner|le{MIDDLE}", L"preposition|de{MIDDLE}", L"noun|van{MIDDLE}", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*2{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 2, 0); // Names ending in a noun should be not common
// D' Artagnan / O'Malley / d'artagnan
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"4",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		1, L"letter{QLAST}", 0, 1, 1,
		1, L"quotes", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*1{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1, 0); // Names ending in a noun should be not common
// Monsieur le Pen / Monsieur le compte / Eamon de Valera // Covento de San -Francisco
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"9",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		2, L"determiner|le{MIDDLE}", L"preposition|de{MIDDLE}", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*1{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 2, 0); // Names ending in a noun should be not common
// M. D' Artagnan / K. O'Malley / M. d'artagnan
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"A",
		1, L"__NAMEINTRO", 0, 0, 1,
		1, L"letter{FIRST}", 0, 1, 1,
		1, L".", 0, 0, 1,
		1, L"Proper Noun{MIDDLE}", NO_OWNER, 0, 1,
		1, L"letter{QLAST}", 0, 1, 1,
		1, L"quotes", 0, 1, 1,
		2, L"Proper Noun{LAST}", L"noun*1{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1, 0);  // Names ending in a noun should be not common
// Number Fourteen's voice filled the room.
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"Q",
		1, L"Proper Noun|number", 0, 1, 1,
		3, L"Number*-3{ANY}", L"roman_numeral*-3{ANY}", L"numeral_cardinal*-3{ANY}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	cPattern::create(L"__NAMEOWNER{NAMEOWNER}", L"H",
		1, L"honorific{SINGULAR:HON}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);

	cPattern::create(L"__NAMEOWNER{NAMEOWNER:SINGULAR}", L"R",
		1, L"letter", 0, 1, 1,
		1, L"dash*-2", 0, 0, 1,
		1, L"Number", SINGULAR_OWNER, 1, 2,
		0);
	cPattern::create(L"_NAMEOWNER", L"", 1, L"__NAMEOWNER", 0, 1, 1, 0);

}

void cName::operator = (const cName& n)
{
	LFS
		nickName = n.nickName;
	hon = n.hon;
	hon2 = n.hon2;
	hon3 = n.hon3;
	first = n.first;
	middle = n.middle;
	middle2 = n.middle2;
	last = n.last;
	suffix = n.suffix;
	any = n.any;
}

bool cName::operator == (const cName& n)
{
	LFS
		return hon == n.hon && hon2 == n.hon2 && hon3 == n.hon3 &&
		first == n.first && middle == n.middle && middle2 == n.middle2 && last == n.last && suffix == n.suffix &&
		any == n.any;
}

bool cName::getNickName(tIWMM firstName)
{
	LFS
		unordered_map<wstring, int>::iterator iNickname;
	iNickname = nicknameEquivalenceMap.find(firstName->first);
	if (iNickname == nicknameEquivalenceMap.end()) return false;
	nickName = iNickname->second;
	return true;
}

void cName::hn(const wchar_t* namePartName, tIWMM namePart, wstring& accumulate, bool printShort, const wchar_t* separator = L" ")
{
	LFS
		if (namePart == wNULL) return;
	if (!printShort)
	{
		accumulate += namePartName;
		accumulate += L":";
	}
	int len = accumulate.length();
	accumulate += namePart->first + separator;
	accumulate[len] = towupper(accumulate[len]);
}

void cName::hn(const wchar_t* namePartName, tIWMM namePart, wchar_t* accumulate, bool printShort, const wchar_t* separator = L" ")
{
	LFS
		if (namePart == wNULL) return;
	if (!printShort)
	{
		wcscat(accumulate, namePartName);
		wcscat(accumulate, L":");
	}
	wchar_t* ch = accumulate + wcslen(accumulate);
	wcscat(accumulate, namePart->first.c_str());
	wcscat(accumulate, separator);
	*ch = towupper(*ch);
}

wstring cName::print(wstring& message, bool printShort, const wchar_t* separator = L" ")
{
	LFS
		hn(L"H1", hon, message, printShort, separator);
	hn(L"H2", hon2, message, printShort, separator);
	hn(L"H3", hon3, message, printShort, separator);
	hn(L"F", first, message, printShort, separator);
	hn(L"M1", middle, message, printShort, separator);
	hn(L"M2", middle2, message, printShort, separator);
	hn(L"L", last, message, printShort, separator);
	hn(L"A", any, message, printShort, separator);
	hn(L"S", suffix, message, printShort, separator);
	if (nickName >= 0 && !printShort)
	{
		wchar_t temp[10];
		wsprintf(temp, L"[%d]", nickName);
		message += temp;
	}
	if (message.length() > 0 && message[message.length() - 1] == L' ')
		message.erase(message.length() - 1);
	return message;
}

// optimized
wstring cName::print(wchar_t* message, bool printShort, const wchar_t* separator = L" ")
{
	LFS
		message[0] = 0;
	hn(L"H1", hon, message, printShort, separator);
	hn(L"H2", hon2, message, printShort, separator);
	hn(L"H3", hon3, message, printShort, separator);
	hn(L"F", first, message, printShort, separator);
	hn(L"M1", middle, message, printShort, separator);
	hn(L"M2", middle2, message, printShort, separator);
	hn(L"L", last, message, printShort, separator);
	hn(L"S", suffix, message, printShort, separator);
	hn(L"A", any, message, printShort, separator);
	if (nickName >= 0 && !printShort)
		wsprintf(message + wcslen(message), L"[%d]", nickName);
	return message;
}

bool cName::notNull()
{
	LFS
		return hon == wNULL && hon2 == wNULL && hon3 == wNULL &&
		first == wNULL && middle == wNULL && middle2 == wNULL && last == wNULL && suffix == wNULL && any == wNULL;
}

bool cName::hn(tIWMM namePart, wchar_t separationCharacter, wstring& accumulate)
{
	LFS
		if (namePart == wNULL) return false;
	int currentPosition = accumulate.length();
	accumulate += namePart->first + separationCharacter;
	accumulate[currentPosition] = toupper(accumulate[currentPosition]);
	return true;
}

wstring cName::original(wstring& message, wchar_t separationCharacter, bool justFirstAndLast)
{
	LFS
		//bool alreadyPrinted=false;
		if (!justFirstAndLast)
		{
			hn(hon, separationCharacter, message);
			hn(hon2, separationCharacter, message);
			hn(hon3, separationCharacter, message);
		}
	if (!hn(first, separationCharacter, message))
		hn(any, separationCharacter, message);
	if (!justFirstAndLast)
	{
		hn(middle, separationCharacter, message);
		hn(middle2, separationCharacter, message);
	}
	hn(last, separationCharacter, message);
	if (!justFirstAndLast)
		hn(suffix, separationCharacter, message);
	if (message.length() > 1)
		message.erase(message.length() - 1); // erase last separation character
	return message;
}

bool cName::match(tIWMM sub1, tIWMM sub2, bool returnTrueOnNull)
{
	LFS
		if (sub1 == wNULL || sub2 == wNULL) return returnTrueOnNull;
	// compare Number 14 with Number Fourteen
	if (sub1->second.query(numeralCardinalForm) >= 0 && sub2->second.query(NUMBER_FORM_NUM) >= 0)
	{
		int anyNum1 = mapNumeralCardinal(sub1->first);
		int anyNum2 = _wtoi(sub2->first.c_str());
		if (anyNum1 == anyNum2) return true;
	}
	else if (sub2->second.query(numeralCardinalForm) >= 0 && sub1->second.query(NUMBER_FORM_NUM) >= 0)
	{
		int anyNum2 = mapNumeralCardinal(sub2->first);
		int anyNum1 = _wtoi(sub1->first.c_str());
		if (anyNum1 == anyNum2) return true;
	}
	if (sub1->first.length() == 1 || sub2->first.length() == 1)
		return (sub1->first[0] == sub2->first[0]);
	return (sub1->first == sub2->first);
}

bool cName::in(tIWMM inhon, vector <tIWMM>& hons)
{
	LFS
		vector <tIWMM>::iterator hi = hons.begin(), hiEnd = hons.end();
	for (; hi != hiEnd; hi++) if (inhon == *hi) return true;
	return false;
}

void cName::merge(tIWMM& w1, tIWMM w2)
{
	LFS
		if (w2 != wNULL && (w1 == wNULL || (w1->first[1] && w2->first[1]))) w1 = w2;
}

// merge first, middle, middle2 or last if we only have a letter and n has more.
void cName::merge(cName& n, sTrace& t)
{
	LFS
		vector <tIWMM> hons;
	wstring name1, name2, name3;
	if (t.traceObjectResolution || t.traceSpeakerResolution)
	{
		n.print(name1, false);
		print(name2, false);
	}
	/*
	if (n.any!=wNULL)
	{
		if (t.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"merge %s into %s (n.any).",name1.c_str(),name2.c_str());
		return;
	}
	if (any!=wNULL)
	{
		if (t.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"merge %s into %s (any).",name1.c_str(),name2.c_str());
		*this=n;
		return;
	}
	*/
	if (hon != wNULL) hons.push_back(hon);
	if (hon2 != wNULL) hons.push_back(hon2);
	if (hon3 != wNULL) hons.push_back(hon3);
	if (n.hon != wNULL && !in(n.hon, hons)) hons.push_back(n.hon);
	if (n.hon2 != wNULL && !in(n.hon2, hons)) hons.push_back(n.hon2);
	if (n.hon3 != wNULL && !in(n.hon3, hons)) hons.push_back(n.hon3);
	hon = hon2 = hon3 = wNULL;
	if (hons.size() > 0) hon = hons[0];
	if (hons.size() > 1) hon2 = hons[1];
	if (hons.size() > 2) hon3 = hons[2];
	merge(any, n.any);
	// this=H1:sir L:james n=H1:sir F:james M1:peel L:edgerton
	if (first == wNULL && n.first != wNULL && match(n.first, last))
	{
		last = n.last;
		first = n.first;
	}
	// this=F:Peel L:Edgerton matches n=F:James M1:Peel L:Edgerton
	if (middle == wNULL && first != wNULL && n.middle != wNULL && match(n.middle, first) && !match(n.first, n.middle))
	{
		middle = first;
		first = n.first;
	}
	else
	{
		if (first == wNULL || first->first.length() == 1)
			merge(first, n.first);
		if (middle == wNULL || middle->first.length() == 1)
			merge(middle, n.middle);
	}
	if (n.nickName >= 0 && nickName < 0) nickName = n.nickName;
	merge(middle2, n.middle2);
	// Comparing H1:sir F:james M1:peel L:edgerton [106] WITH H1:sir L:james 
	if (match(n.last, last))
		merge(last, n.last);
	merge(suffix, n.suffix);
	if (any != wNULL && (first != wNULL || last != wNULL))
	{
		tIWMM tAny = any;
		any = wNULL;
		if (t.traceObjectResolution || t.traceSpeakerResolution)
		{
			print(name3, false);
			if (first != any && last != any && middle != any)
				lplog(LOG_RESOLUTION, L"merge %s into %s -> %s dropped ('any' designation) %s.", name1.c_str(), name2.c_str(), name3.c_str(), tAny->first.c_str());
			else
				lplog(LOG_RESOLUTION, L"merge %s into %s -> %s.", name1.c_str(), name2.c_str(), name3.c_str());
		}
	}
	else if (t.traceSpeakerResolution)
	{
		print(name3, false);
		lplog(LOG_RESOLUTION, L"merge %s into %s -> %s.", name1.c_str(), name2.c_str(), name3.c_str());
	}
}

/*
2  H1:mrs.                F:firstname M1:middle L:last [FEMALE]
4  H1:mrs.                F:firstname           L:last [FEMALE]
5  H1:mr.                 F:firstname           L:last S:fifth [MALE]
6  H1:mr.                 F:firstname           L:last S:iii [MALE]
7  H1:mr. H2:mrs.                               L:last [FEMALE][MALE][PLURAL]
8  A:any [FEMALE][MALE]

if any is NULL in s and s2:
plural == plural
1 against 1 - if F & M1 & L & S match, return true.
'match' is if F1 or F2 are null, return true.
					if F1 or F2 is a letter then if F2[0]==F1[0] return true.
if all match are true,
	for 1:if F or M1 or L is a letter, 1=2. merge=true
	for 1:if F or M1 or L or S is NULL, 1=2. merge=true
if merge, change all s2 to s.

if any is NOT NULL in s:
	if any = firstname or lastname, return true. merge=true
	if merge, change all s2 to s.
*/
// does not compare plural or sex
bool cName::like(cName& n, sTrace& t)
{
	LFS
		if (t.traceNameResolution)
		{
			wstring tmpstr, tmpstr2;
			lplog(LOG_RESOLUTION, L"Comparing %s WITH %s", n.print(tmpstr, false).c_str(), print(tmpstr2, false).c_str());
		}
	if (isCompletelyNull() || n.isCompletelyNull())
	{
		wstring tmpstr, tmpstr2;
		lplog(LOG_RESOLUTION, L"Null comparison comparing %s WITH %s", n.print(tmpstr, false).c_str(), print(tmpstr2, false).c_str());
		return false;
	}
	if (*this == n) return true;
	if (n.any == wNULL && any == wNULL)
	{
		// if middle or last names don't match, dump.
		if (!match(n.middle, middle)) return false;
		if (!match(n.middle2, middle2)) return false;
		if (!match(n.last, last))
		{
			if (last->first != L"something" && n.last->first != L"something" &&
				// Comparing H1:sir F:james M1:peel L:edgerton [106] WITH H1:sir L:james 
				// but rule out Comparing H1:dr L:adams  WITH H1:mr F:a L:carter (first->first.length()!=1)
				!(first == wNULL && n.first != wNULL && n.first->first.length() != 1 && match(n.first, last)) &&
				!(n.first == wNULL && first != wNULL && first->first.length() != 1 && match(first, n.last)))
				return false;
			// don't match Boris Something with Mr. Carter
			if ((last->first == L"something" && n.first == wNULL) || (n.last->first == L"something" && first == wNULL))
				return false;
		}
		if (!match(n.suffix, suffix)) return false;
		// check first name or nicknames
		if (!match(n.first, first))
		{
			// n=F:Peel L:Edgerton matches this=F:James M1:Peel L:Edgerton
			// this=F:Peel L:Edgerton matches n=F:James M1:Peel L:Edgerton
			if ((middle == wNULL || !match(n.first, middle)) && (n.middle == wNULL || !match(first, n.middle)))
			{
				if (n.nickName == -1 || nickName == -1) return false;
				if (n.nickName != nickName) return false;
			}
		}
		// see if one or the other is only an honorific
		// if so, only match the honorific
		// each honorific in the honorific-only name must match the honorifics in the non-honorific only name.
		// the honorific-only name may not have have all the honorifics and 'like' will still return true.
		bool selfOnlyHonorific = justHonorific(), otherOnlyHonorific = n.justHonorific();
		if (otherOnlyHonorific || selfOnlyHonorific)
		{
			if (selfOnlyHonorific && !otherOnlyHonorific)
			{
				if (hon == wNULL || (!match(n.hon, hon, false) && !match(n.hon2, hon, false) && !match(n.hon3, hon, false))) return false;
				if (hon2 != wNULL && (!match(n.hon, hon2, false) && !match(n.hon2, hon2, false) && !match(n.hon3, hon2, false))) return false;
				if (hon3 != wNULL && (!match(n.hon, hon3, false) && !match(n.hon2, hon3, false) && !match(n.hon3, hon3, false))) return false;
				return true;
			}
			if (!selfOnlyHonorific && otherOnlyHonorific)
			{
				if (n.hon == wNULL || (!match(hon, n.hon, false) && !match(hon2, n.hon, false) && !match(hon3, n.hon, false))) return false;
				if (n.hon2 != wNULL && (!match(hon, n.hon2, false) && !match(hon2, n.hon2, false) && !match(hon3, n.hon2, false))) return false;
				if (n.hon3 != wNULL && (!match(hon, n.hon3, false) && !match(hon2, n.hon3, false) && !match(hon3, n.hon3, false))) return false;
				return true;
			}
			//each must have the same number (and be identical)
			if (hon == wNULL || (!match(n.hon, hon, false) && !match(n.hon2, hon, false) && !match(n.hon3, hon, false))) return false;
			if (hon2 == wNULL ^ n.hon2 == wNULL) return false;
			if (hon2 == wNULL) return true;
			if (!match(n.hon, hon2, false) && !match(n.hon2, hon2, false) && !match(n.hon3, hon2, false)) return false;
			if (hon3 == wNULL ^ n.hon3 == wNULL) return false;
			if (hon3 == wNULL) return true;
			if (!match(n.hon, hon3, false) && !match(n.hon2, hon3, false) && !match(n.hon3, hon3, false)) return false;
			return true;
		}
		if (hon != wNULL && n.hon != wNULL)
		{
			// Miss (Janet) Vandermeyer and Mrs. Vandermeyer must not match!  50055
			if (hon->first == L"mrs" && n.hon->first == L"miss") return false;
			if (n.hon->first == L"mrs" && hon->first == L"miss") return false;
		}
		return true;
	}
	// if any don't match, dump.
	if (nickName != -1 && n.nickName == nickName)
		return true;
	if (hon != wNULL && n.hon != wNULL)
	{
		// Miss (Janet) Vandermeyer and Mrs. Vandermeyer must not match!  50055
		if (hon->first == L"mrs" && n.hon->first == L"miss") return false;
		if (n.hon->first == L"mrs" && hon->first == L"miss") return false;
	}
	//if (!unambiguousGenderFound) return false; // don't allow looser matching
	// must add length()>1 because Albert should not match A. Carter (Albert, a small lift boy)
	// possibly if they are 'near' to each other (within the same speaker group) we could let this slide.
	if (any == wNULL)
		return (match(n.any, first, false) && first->first.length() > 1) || (match(n.any, last, false) && last->first.length() > 1);
	if (n.any == wNULL)
		return (match(any, n.first, false) && n.first->first.length() > 1) || (match(any, n.last, false) && n.last->first.length() > 1);
	return match(n.any, any, false);
}

// is this name just an honorific (miss or lord)
bool cName::justHonorific(void)
{
	LFS
		return hon != wNULL && first == wNULL && last == wNULL && any == wNULL;
}

// if this a person?
// there is no suffix or honorific
// must have at least a first and last
// the first and last words must not have a sex
// the last word cannot be unknown and must be either primarily an adjective or noun
// on all objects relating to last word (and having the same or no first word), there is no suffix or honorific
// The Lusitania - ngname
// St. James Park - ngname
// The Times - ngname
// old mother Greenbank - female name
// the one Mabel Lewis - female name
// two Johnnies - plural name
// someone called Jane Finn - female name
// the Sisters - plural name
// the aforementioned lieutenant Thomas Beresford - male name
// the YOUNG ADVENTURERS not a name or ngname
// the Sinn Feiner
bool cName::neuterName(bool startsWithDeterminer, bool ownedByName, int len)
{
	LFS
		if (suffix != wNULL || hon != wNULL) // there is no suffix or honorific
			return false;
	// St. James' Park (ownedByName)
	if ((first == wNULL || last == wNULL) && !startsWithDeterminer && !ownedByName) // must have at least a first and last
		return false;
	// the first and middle words must not have a sex
	if ((first != wNULL && (first->second.inflectionFlags & (MALE_GENDER | FEMALE_GENDER | MALE_GENDER_ONLY_CAPITALIZED | FEMALE_GENDER_ONLY_CAPITALIZED)) &&
		first->second.query(commonProfessionForm) < 0 && first->second.query(demonymForm) < 0) ||
		(middle != wNULL && (middle->second.inflectionFlags & (MALE_GENDER | FEMALE_GENDER | MALE_GENDER_ONLY_CAPITALIZED | FEMALE_GENDER_ONLY_CAPITALIZED))))
		return startsWithDeterminer && len == 2;
	// the last word cannot be unknown 
	// The Lusitania
	// The late great Bob Carmichel 
	tIWMM anyLast = last;
	if (anyLast == wNULL && (anyLast = any) == wNULL) return false;
	if (anyLast->second.query(UNDEFINED_FORM_NUM) >= 0)
		return startsWithDeterminer || len == 2;
	// neutral if the last word is either primarily an adjective or noun
	// Williams Park
	int flc = anyLast->second.getLowestCost();
	return flc >= 0 && ((anyLast->second.forms()[flc] == nounForm || anyLast->second.forms()[flc] == adjectiveForm));
}

// this routine does not apply unless the sex is a confident match.
// does the first and last name match?
// does the sex and last names match, with the first names matching if existing?
// does the title and any other name match?
	// self and n are already like each other (like==true)
bool cName::confidentMatch(cName& n, bool sexConfidentMatch, sTrace& t)
{
	LFS
		if (t.traceNameResolution)
		{
			wstring tmpstr, tmpstr2;
			lplog(LOG_RESOLUTION, L"Comparing %s WITH %s CONFIDENT MATCH", n.print(tmpstr, false).c_str(), print(tmpstr2, false).c_str());
		}
	if (*this == n) return true;
	if (!sexConfidentMatch) return false;
	if (n.any == wNULL && any == wNULL)
	{
		// if the suffix matches and first and last match, given first and last are not both matching against NULL
		if ((match(n.suffix, suffix, false) || n.suffix == suffix) && match(n.first, first, true) && match(n.last, last, true) &&
			((n.first != wNULL && first != wNULL) || (n.last != wNULL && last != wNULL)))
			return true;
		// check first name and last name
		if (match(n.first, first, false) && match(n.last, last, false))
			return true;
		return false;
	}
	if (nickName != -1 && n.nickName == nickName && match(n.suffix, suffix, true) &&
		match(n.first, first, true) && match(n.last, last, true))
		return true;
	return (match(n.suffix, suffix, false) || sexConfidentMatch) && match(n.first, first, true) && match(n.last, last, true) &&
		(match(n.any, first, false) || match(n.any, last, false) || match(any, n.first, false) || match(any, n.last, false));
}

void cName::insertSubSQL(wchar_t* buffer, int sourceId, int index, int maxbuf, tIWMM hp, int& buflen, enum cName::nameType ht)
{
	LFS
		if (hp != wNULL && hp->second.index >= 0) buflen += _snwprintf(buffer + buflen, maxbuf - buflen, L"(%d,%d,%d,%d),", sourceId, index, hp->second.index, ht);
}

int cName::insertSQL(wchar_t* buffer, int sourceId, int index, int maxbuf)
{
	LFS
		int buflen = 0;
	insertSubSQL(buffer, sourceId, index, maxbuf, hon, buflen, HON);
	insertSubSQL(buffer, sourceId, index, maxbuf, hon2, buflen, HON2);
	insertSubSQL(buffer, sourceId, index, maxbuf, hon3, buflen, HON3);
	insertSubSQL(buffer, sourceId, index, maxbuf, first, buflen, FIRST);
	insertSubSQL(buffer, sourceId, index, maxbuf, middle, buflen, MIDDLE);
	insertSubSQL(buffer, sourceId, index, maxbuf, middle2, buflen, MIDDLE2);
	insertSubSQL(buffer, sourceId, index, maxbuf, last, buflen, LAST);
	insertSubSQL(buffer, sourceId, index, maxbuf, suffix, buflen, SUFFIX);
	insertSubSQL(buffer, sourceId, index, maxbuf, any, buflen, ANY);
	return buflen;
}

bool cSource::evaluateName(vector <cTagLocation>& tagSet, cName& name, bool& isMale, bool& isFemale, bool& isPlural, bool& isBusiness)
{
	LFS
		name.hon = name.hon2 = name.hon3 = name.first = name.middle = name.middle2 = name.last = name.suffix = name.any = wNULL;
	name.nickName = -1;
	if (isBusiness = (findOneTag(tagSet, L"BUS", -1) >= 0)) return true;
	//bool added=false;
	isMale = isFemale = false;
	int nextFirst = -1, nextMiddle = -1, nextLast = -1, nextHon1 = -1;
	int whereHon1 = findTag(tagSet, L"HON", nextHon1), whereHon2 = findOneTag(tagSet, L"HON2", -1), whereHon3 = findOneTag(tagSet, L"HON3", -1);
	int whereFirst = findTag(tagSet, L"FIRST", nextFirst);
	int whereAny = findOneTag(tagSet, L"ANY", -1);
	int whereMiddle = findTag(tagSet, L"MIDDLE", nextMiddle);
	if (whereMiddle < 0 && nextFirst >= 0)
		whereMiddle = nextFirst;
	int whereLast = findTag(tagSet, L"LAST", nextLast);
	//int whereQLast=findTag(tagSet,L"QLAST",nextQLast);
	int whereSuffix = findOneTag(tagSet, L"SUFFIX", -1);
	isPlural |= findOneTag(tagSet, L"PLURAL", -1) >= 0;
	if (whereHon1 == -1)
	{
		if (whereFirst == -1 && whereAny == -1 && whereLast == -1)
			return false;
		if (whereSuffix != -1)
			name.suffix = m[tagSet[whereSuffix].sourcePosition].word;
		if ((whereFirst != -1) ^ (whereLast != -1))
		{
			if ((name.any = setSex(tagSet, max(whereFirst, whereLast), isMale, isFemale, isPlural)) != wNULL)
				name.getNickName(name.any);
			return true;
		}
		if (whereAny != -1)
		{
			if ((name.any = setSex(tagSet, whereAny, isMale, isFemale, isPlural)) != wNULL)
				name.getNickName(name.any);
			return (name.any->first.length() > 1) || m[tagSet[whereAny].sourcePosition + 1].word->first == L"." || // a person cannot be referred to by a single letter (with no period after it)
				(tagSet[whereAny].sourcePosition > 0 && !isPlural && m[tagSet[whereAny].sourcePosition - 1].queryWinnerForm(NUMBER_FORM_NUM) >= 0); // 3 M / 3 D / 4 G
		}
	}
	name.hon = setSex(tagSet, whereHon1, isMale, isFemale, isPlural);
	setSex(tagSet, nextHon1, isMale, isFemale, isPlural);
	name.hon2 = setSex(tagSet, whereHon2, isMale, isFemale, isPlural);
	name.hon3 = setSex(tagSet, whereHon3, isMale, isFemale, isPlural);
	// first name is only indicative if the honorific does not exist or has no gender
	if ((name.first = setSex(tagSet, whereFirst, isMale, isFemale, isPlural)) != wNULL)
		name.getNickName(name.first);
	name.middle = setSex(tagSet, whereMiddle, isMale, isFemale, isPlural);
	if (nextMiddle != -1)
		name.middle2 = m[tagSet[nextMiddle].sourcePosition].word;
	if (whereLast != -1)
	{
		if (name.hon != wNULL && name.first == wNULL && name.hon->first == L"st")
			name.last = setSex(tagSet, whereLast, isMale, isFemale, isPlural);
		else
			name.last = m[tagSet[whereLast].sourcePosition].word;
	}
	if (nextLast != -1)
		name.last = m[tagSet[nextLast].sourcePosition].word;
	if (whereSuffix != -1)
		name.suffix = m[tagSet[whereSuffix].sourcePosition].word;
	return true;
}

bool cSource::identifyNameAdjective(int where, cName& name, bool& isMale, bool& isFemale)
{
	LFS
		int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(L"__NAMEOWNER", nameEnd)) == -1)
	{
		if (m[where].queryWinnerForm(PROPER_NOUN_FORM_NUM) >= 0 && (m[where].flags & cWordMatch::flagNounOwner))
		{
			name.hon = name.hon2 = name.hon3 = name.first = name.middle = name.middle2 = name.last = name.suffix = wNULL;
			name.any = m[where].word;
			isMale = (name.any->second.inflectionFlags & (MALE_GENDER | MALE_GENDER_ONLY_CAPITALIZED)) != 0;
			isFemale = (name.any->second.inflectionFlags & (FEMALE_GENDER | FEMALE_GENDER_ONLY_CAPITALIZED)) != 0;
			name.getNickName(name.any);
			return true;
		}
		return false;
	}
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(false, nameTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, L"identify name adjective") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, L"NR", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateNameAdjective(tagSets[J], name, isMale, isFemale))
				return true;
		}
	return false;
}

// possible combinations:
// A
// H1 or H1 && H2 or H1 && H2 && H3
// F or F&L or F&L&M1 or F&L&M1&M2
bool cSource::identifyName(int where, int& element, cName& name, bool& isMale, bool& isFemale, bool& isPlural, bool& isBusiness)
{
	LFS
		int nameEnd = -1;
	if ((element = m[where].pma.queryPattern(L"_NAME", nameEnd)) == -1) return false;
	nameEnd += where;
	// if last letter is 's', preceded by The or quantifier, then plural
	isPlural = (m[nameEnd - 1].word->first[m[nameEnd - 1].word->first.size() - 1] == L's' &&
		(m[where].word->first == L"the" || (m[where].queryForm(L"quantifier") >= 0 && m[where].word->first != L"one")));
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(false, nameTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, L"identify name") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, L"NR", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateName(tagSets[J], name, isMale, isFemale, isPlural, isBusiness))
				return true;
		}
	return false;
}

bool cSource::identifyName(int begin, int principalWhere, int end, int& nameElement, cName& name, bool& isMale,
	bool& isFemale, bool& isNeuter, bool& isPlural, bool& isBusiness, bool& comparableName,
	bool& comparableNameAdjective, bool& requestWikiAgreement, OC& objectClass)
{
	LFS
		if (!(comparableName = (nameElement != -1 && identifyName(principalWhere, nameElement, name, isMale, isFemale, isPlural, isBusiness))))
			comparableNameAdjective = identifyNameAdjective(principalWhere, name, isMale, isFemale);
	// scan for after adjectives
	for (int aa = begin + 1, len = 0; aa < end; aa++)
		if (m[aa].pma.queryPattern(L"_ADJECTIVE_AFTER", len) && aa + len == end)
		{
			end = aa;
			break;
		}
	// 'the archdeacon' - where archdeacon is both an honorific AND an occupation
	// this is in case the last name is not only recognized as a _NAME but also as a _NOUN and the _NOUN takes precedence
	// but this should be a name.
	// but NOT 'the young lady' but also 'the timid archdeacon'
	if (!comparableName && !comparableNameAdjective && m[principalWhere].queryWinnerForm(honorificForm) >= 0 &&
		(m[principalWhere].word->second.inflectionFlags & PLURAL) != PLURAL && principalWhere > 0 &&
		(m[principalWhere - 1].queryWinnerForm(adjectiveForm) < 0 || m[principalWhere].queryForm(L"pinr") < 0))
	{
		name.hon2 = name.hon3 = name.first = name.middle = name.middle2 = name.last = name.suffix = name.any = wNULL;
		name.hon = m[principalWhere].word;
		isMale = (name.hon->second.inflectionFlags & MALE_GENDER) != 0;
		isFemale = (name.hon->second.inflectionFlags & FEMALE_GENDER) != 0;
		isNeuter = false;
		isPlural = false;
		comparableName = true;
		objectClass = NAME_OBJECT_CLASS;
	}
	// Nurse Edith
	if (name.hon == wNULL && name.first != wNULL && name.last != wNULL && name.first->second.query(commonProfessionForm) >= 0 && (name.last->second.inflectionFlags & (MALE_GENDER | FEMALE_GENDER)))
	{
		name.hon = name.first;
		name.any = name.last;
		name.first = name.last = wNULL;
		wstring tmpstr;
		lplog(LOG_RESOLUTION, L"%06d:Used common profession of name %s", principalWhere, name.print(tmpstr, false).c_str());
	}
	if (comparableName || comparableNameAdjective)
	{
		bool ownedByName = false;
		if (comparableName && begin != principalWhere)
		{
			for (int I = begin; I < principalWhere && !ownedByName; I++)
				if (m[I].queryWinnerForm(PROPER_NOUN_FORM_NUM) >= 0 &&
					(((m[I].word->second.inflectionFlags) & (PLURAL_OWNER | SINGULAR_OWNER)) ||
						((m[I].flags) & cWordMatch::flagNounOwner)))
					ownedByName = true;
		}
		bool isDemonym = m[end - 1].queryForm(demonymForm) >= 0 && (m[begin].queryForm(determinerForm) >= 0 || m[begin].queryWinnerForm(demonstrativeDeterminerForm) >= 0);
		//(end-begin<=2 || m[end-2].queryWinnerForm(PROPER_NOUN_FORM_NUM)<0);
// if only one word is capitalized and that word is a name of a character, return NULL
// a metamorphosed Tuppence
		bool isSingleName = false;
		if (name.first == wNULL && name.last == wNULL && name.any != wNULL)
		{
			for (set<int>::iterator s = relatedObjectsMap[name.any].begin(), send = relatedObjectsMap[name.any].end(); s != send && !isSingleName; s++)
				if (objects[*s].firstLocation < begin && objects[*s].objectClass == NAME_OBJECT_CLASS &&
					!(m[objects[*s].firstLocation].flags & cWordMatch::flagAdjectivalObject))
				{
					for (vector <cObject::cLocation>::iterator I = objects[*s].locations.begin(), IEnd = objects[*s].locations.end(); I != IEnd && !isSingleName; I++)
						isSingleName = (m[I->at].objectRole & SUBJECT_ROLE) != 0;
				}
		}
		bool isLastName = false;
		// if two words are capitalized and the last word matches a last word of a name of a character, return NULL
		// Peel Edgerton
		if (name.first != wNULL && name.last != wNULL && name.any == wNULL)
		{
			for (set<int>::iterator s = relatedObjectsMap[name.last].begin(), send = relatedObjectsMap[name.last].end(); s != send && !isLastName; s++)
				isLastName = (objects[*s].firstLocation < begin&& objects[*s].objectClass == NAME_OBJECT_CLASS && objects[*s].name.last == name.last &&
					!(m[objects[*s].firstLocation].flags & cWordMatch::flagAdjectivalObject)); //  && (m[objects[*s].firstLocation].objectRole&SUBJECT_ROLE)
		}
		wstring nw;
		if (!isLastName && !isSingleName && !name.justHonorific() && !isDemonym &&
			((requestWikiAgreement = name.neuterName(m[begin].queryWinnerForm(determinerForm) >= 0, ownedByName, end - begin)) || isBusiness ||
				// Friday is my usual day, Ma'am.
				// Friday and Saturday passed uneventfully.
				(end - begin == 1 && ((nameElement = m[begin].word->second.timeFlags) & T_UNIT))))
		{
			objectClass = NON_GENDERED_NAME_OBJECT_CLASS;
			isNeuter = true;
			isMale = isFemale = false;
		}
		else
		{
			if (isDemonym)
			{
				objectClass = GENDERED_DEMONYM_OBJECT_CLASS;
				if ((m[principalWhere].word->second.inflectionFlags & PLURAL) == PLURAL)
					isPlural = true;
			}
			// must only be one word, in a position where it doesn't have to be a proper noun,
			// not an indefinitePronoun or there
			else if (end != principalWhere + 1 || (m[principalWhere].flags & (cWordMatch::flagOnlyConsiderProperNounForms)) ||
				(m[principalWhere].queryForm(indefinitePronounForm) < 0 && m[principalWhere].word->first != L"there"))
				objectClass = NAME_OBJECT_CLASS;
			if (!isMale && !isFemale) isMale = isFemale = true;
			isNeuter = false;
		}
		/* Parks
			 two Johnnies
			 */
		if (m[end - 1].queryWinnerForm(nounForm) >= 0 || (m[begin].queryForm(numeralCardinalForm) >= 0 && m[begin].word->first != L"one"))
			isPlural = (m[end - 1].word->second.inflectionFlags & PLURAL) == PLURAL;
		return objectClass == NAME_OBJECT_CLASS || objectClass == NON_GENDERED_NAME_OBJECT_CLASS;
	}
	return false;
}

tIWMM cSource::setSex(vector <cTagLocation>& tagSet, int where, bool& isMale, bool& isFemale, bool isPlural)
{
	LFS
		if (where < 0) return wNULL;
	tIWMM word = m[tagSet[where].sourcePosition].word;
	if (!isPlural && (isMale ^ isFemale)) return word; // male vs female already set
	int inflectionFlags = word->second.inflectionFlags;
	if (isPlural)
	{
		isMale |= (inflectionFlags & MALE_GENDER) != 0;
		isFemale |= (inflectionFlags & FEMALE_GENDER) != 0;
	}
	else
	{
		isMale = (inflectionFlags & MALE_GENDER) != 0;
		isFemale = (inflectionFlags & FEMALE_GENDER) != 0;
	}
	if ((m[tagSet[where].sourcePosition].flags & cWordMatch::flagFirstLetterCapitalized) != 0)
	{
		if (isPlural)
		{
			isMale |= (inflectionFlags & MALE_GENDER_ONLY_CAPITALIZED) != 0;
			isFemale |= (inflectionFlags & FEMALE_GENDER_ONLY_CAPITALIZED) != 0;
		}
		else
		{
			isMale |= (inflectionFlags & MALE_GENDER_ONLY_CAPITALIZED) != 0;
			isFemale |= (inflectionFlags & FEMALE_GENDER_ONLY_CAPITALIZED) != 0;
		}
	}
	return word;
}

// an __NAMEOWNER of diff 3, with the second element being a single Proper Noun / Al's
// an __NAMEOWNER of diff 8, with the first and third elements being a proper noun / Dave and Jen's place
// an __NAMEOWNER of diff A, which is a name adjective / Dr. Helen Billows Mirren's
// an __NAMEOWNER of diff B, which is a name adjective / Dr. Mirren's
// ignore the next two for now
// a __NAME of diff 'A' / Al's Shack
// a __PP where the last element is a PROPER_NOUN with an owner / at old Red's
bool cSource::evaluateNameAdjective(vector <cTagLocation>& tagSet, cName& name, bool& isMale, bool& isFemale)
{
	LFS
		name.hon = name.hon2 = name.hon3 = name.first = name.middle = name.middle2 = name.last = name.suffix = name.any = wNULL;
	isMale = isFemale = false;
	int nextFirst = -1, nextMiddle = -1, nextLast = -1, nextHon1 = -1;
	int whereHon1 = findTag(tagSet, L"HON", nextHon1), whereHon2 = findOneTag(tagSet, L"HON2", -1), whereHon3 = findOneTag(tagSet, L"HON3", -1);
	int whereFirst = findTag(tagSet, L"FIRST", nextFirst);
	int whereAny = findOneTag(tagSet, L"ANY", -1);
	int whereMiddle = findTag(tagSet, L"MIDDLE", nextMiddle);
	if (whereMiddle < 0 && nextFirst >= 0)
		whereMiddle = nextFirst;
	int whereLast = findTag(tagSet, L"LAST", nextLast);
	//int whereQLast=findTag(tagSet,L"QLAST",nextQLast);
	bool isPlural = findOneTag(tagSet, L"PLURAL", -1) != -1;
	if (whereHon1 == -1)
	{
		if (whereFirst == -1 && whereAny == -1 && whereLast == -1)
			return false;
		if ((whereFirst != -1) ^ (whereLast != -1))
		{
			if ((name.any = setSex(tagSet, max(whereFirst, whereLast), isMale, isFemale, isPlural)) != wNULL)
				name.getNickName(name.any);
			return true;
		}
		if (whereAny != -1)
		{
			if ((name.any = setSex(tagSet, whereAny, isMale, isFemale, isPlural)) != wNULL)
				name.getNickName(name.any);
			return (name.any->first.length() > 1) || m[tagSet[whereAny].sourcePosition + 1].word->first == L"."; // a person cannot be referred to by a single letter (with no period after it)
		}
	}
	name.hon = setSex(tagSet, whereHon1, isMale, isFemale, isPlural);
	setSex(tagSet, nextHon1, isMale, isFemale, isPlural);
	name.hon2 = setSex(tagSet, whereHon2, isMale, isFemale, isPlural);
	name.hon3 = setSex(tagSet, whereHon3, isMale, isFemale, isPlural);
	if ((isMale ^ isFemale) && whereFirst != -1)
		name.first = m[tagSet[whereFirst].sourcePosition].word;
	else
		name.first = setSex(tagSet, whereFirst, isMale, isFemale, isPlural);
	if (name.first != wNULL)
		name.getNickName(name.first);
	name.middle = setSex(tagSet, whereMiddle, isMale, isFemale, isPlural);
	if (nextMiddle != -1)
		name.middle2 = m[tagSet[nextMiddle].sourcePosition].word;
	if (whereLast != -1)
		name.last = m[tagSet[whereLast].sourcePosition].word;
	if (nextLast != -1)
		name.last = m[tagSet[nextLast].sourcePosition].word;
	return true;
}

// These patterns match names to objects.
// _META_NAME_EQUIVALENCE is usually inQuote, but doesn't have to be.
// _META_SPEAKER is always inQuote.
// add INFP "wanted to be called "Brown"
void createMetaNameEquivalencePatterns(void)
{
	LFS
		//cPattern *p=NULL;
		cPattern::create(L"_META_PP{_IGNORE}", L"",
			3, L"to", L"preposition|by", L"preposition|at", 0, 1, 1,
			2, L"__NOUN", L"__MNOUN", 0, 1, 1,
			0);
	// NAME, known _PP as NAME
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"1",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, L",", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		3, L"verb|known", L"verb|called", L"verb|named", VERB_PAST_PARTICIPLE, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"_META_PP", 0, 0, 1, // to her friends
		2, L"as", L"preposition|by", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// NAME is also known as NAME / I am also known as NAME
	// he/she/Donny/the young man was called/named
	// the man known as Number One
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"2",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		2, L"_IS", L"is", 0, 0, 1, // (must be optional for) the man known as Number One
		3, L"verb|known", L"verb|called", L"verb|named", VERB_PAST_PARTICIPLE, 1, 1,
		1, L"_PP", 0, 0, 1,
		2, L"as", L"preposition|by", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// Rafid Ahmed Alwan al-Janabi (Arabic: رافد أحمد علوان‎, Rāfid Aḥmad Alwān; born 1968), known by the Defense Intelligence Agency cryptonym "Curveball", is an Iraqi citizen
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"T",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, L",", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		3, L"verb|known", L"verb|called", L"verb|named", VERB_PAST_PARTICIPLE, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"_META_PP", 0, 0, 1, // to her friends
		2, L"preposition|as", L"preposition|by", 0, 0, 1,
		// this is NOUN[G]
		5, L"determiner{DET}", L"demonstrative_determiner{DET}", L"possessive_determiner{DET}", L"interrogative_determiner{DET}", L"quantifier{DET}", 0, 1, 1,
		1, L"_ADJECTIVE", 0, 0, 2,
		1, L"quotes", OPEN_INFLECTION, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 1, 1,
		0);
	// my/his/her/Donny's name is/was "Dumpling".
	// He gave 'his name as Count Stepanov'
	// he gave her his name : sir James Peel Edgerton .
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"3",
		2, L"possessive_determiner{NAME_PRIMARY}", L"_NAMEOWNER{NAME_PRIMARY}", 0, 1, 1, // his, her, Donny's
		1, L"_ADJECTIVE", 0, 0, 1,
		2, L"noun|name", L"noun|birthname", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		3, L"is{V_AGREE:V_OBJECT}", L"preposition|as", L":", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// Miss Prudence Cowley, fifth daughter of Archdeacon Cowley of Little Missendell, Suffolk.
	cPattern::create(L"__RENOUN{_IGNORE:NOUN}", L"",
		7, L"determiner{DET}", L"demonstrative_determiner{DET}", L"possessive_determiner{DET}", L"interrogative_determiner{DET}", L"quantifier{DET}", L"__HIS_HER_DETERMINER*1", L"_NAMEOWNER{DET}", 0, 0, 1,
		1, L"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		2, L"noun{N_AGREE}", L"indefinite_pronoun{N_AGREE}", NO_OWNER | FEMALE_GENDER | MALE_GENDER, 1, 1, // Mister Carbonell, the only brother 
		0);
	// He called him Brown.
	// He addressed the other as Boris.
	// His friends nicknamed him "Mr. Brown"
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"4",
		1, L"__NOUN", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		15, L"verb|called", L"verb|named", L"verb|renamed", L"verb|nicknamed", L"verb|addressed",
		L"verb|call", L"verb|name", L"verb|rename", L"verb|nickname", L"verb|address",
		L"verb|calls", L"verb|names", L"verb|renames", L"verb|nicknames", L"verb|addresses", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		2, L"personal_pronoun_accusative{NAME_PRIMARY}", L"__RENOUN{NAME_PRIMARY}", 0, 1, 1,
		2, L"preposition|as", L"preposition|by", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// He called himself Brown. / I called myself Bob.
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"5",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		9, L"verb|called", L"verb|named", L"verb|nicknamed", L"verb|call", L"verb|name", L"verb|nickname", L"verb|calls", L"verb|names", L"verb|nicknames", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"reflexive_pronoun{NAME_PRIMARY}", 0, 1, 1,
		1, L"as", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// His friends gave him the nickname of "Mr. Brown"
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"6",
		2, L"__NOUN", L"__MNOUN", 0, 1, 1,
		1, L"__ALLVERB", 0, 1, 1,
		1, L"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // 3,L"him{NAME_PRIMARY}",L"her{NAME_PRIMARY}",L"them{NAME_PRIMARY}",0,0,1,
		1, L"determiner|the", 0, 1, 1,
		2, L"noun|nickname", L"noun|name", 0, 1, 1,
		1, L"preposition|of", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// He took the nickname of "Red".
	// But NOT  I[tuppence] really did invent the name of Jane Finn ! 
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"7",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		3, L"verb|got", L"verb|took", L"verb|received", 0, 1, 1,
		1, L"determiner|the", 0, 1, 1,
		2, L"noun|nickname", L"noun|name", 0, 1, 1,
		1, L"preposition|of", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"8",
		1, L"_NAME{GNOUN:NAME:NAME_PRIMARY}", 0, 1, 1,
		1, L"__C1_IP", 0, 1, 1,
		0);
	// the elderly woman, looking more like a housekeeper than a servant
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"9",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L",", 0, 1, 1,
		1, L"_VERBONGOING", 0, 1, 1, // looking
		1, L"adverb|more", 0, 1, 1,
		1, L"preposition|like", 0, 1, 1,
		1, L"__RENOUN{_BLOCK:RE_OBJECT}", 0, 1, 1,
		1, L"preposition|than", 0, 1, 1,
		1, L"__RENOUN{_BLOCK:RE_OBJECT:NAME_SECONDARY}", 0, 1, 1,
		0);
	// it ought to be enough for an innocent young girl like Jane .
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"V",
		1, L"preposition", 0, 1, 1,
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L"preposition|like", 0, 1, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		0);
	// female crook, answering to the name[name] of Rita ? ”
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"Q",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L",", 0, 1, 1,
		2, L"verb|answering", L"verb|answers", 0, 1, 1,
		1, L"preposition|to", 0, 1, 1,
		1, L"determiner|the", 0, 1, 1,
		2, L"noun|nickname", L"noun|name", 0, 1, 1,
		1, L"preposition|of", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// , a young fellow called Brown. // must be after a comma to prevent confusion with other equivalences
	// about someone called Jane Finn?
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"A",
		2, L",", L"preposition", 0, 1, 1,
		1, L"__RENOUN{NAME_PRIMARY:RE_OBJECT}", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		5, L"verb|called", L"verb|named", L"verb|renamed", L"verb|nicknamed", L"verb|addressed", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"preposition|as", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// he is always spoken of by the unassuming title of ‘QS mr . Brown . ’
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"B",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he
		1, L"_IS", 0, 1, 1,
		1, L"verb|spoken{V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		1, L"preposition|of", 0, 1, 1,
		1, L"preposition|by", 0, 1, 1,
		1, L"determiner{DET}", 0, 1, 1,
		1, L"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		1, L"noun|title{N_AGREE}", NO_OWNER, 1, 1,
		1, L"preposition|of", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// he goes by the name "Mr. Brown"
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"C",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he
		1, L"possessive_determiner*4", 0, 0, 1, // removed _ADVERB and added it to later patterns // the hidden object use should be very rare!
		1, L"verb|goes{vS:V_AGREE:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		1, L"preposition|by", 0, 1, 1,
		1, L"determiner{DET}", 0, 1, 1,
		1, L"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		1, L"noun|name{N_AGREE}", NO_OWNER, 1, 1,
		1, L"preposition|of", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// Tommy put him down as being a Russian or a Pole.
	// him ADV as being NOUN
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"D",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he/him/Bob
		1, L"_ADVERB", 0, 0, 1,
		1, L"preposition|as", 0, 0, 1,
		1, L"verb|being", 0, 1, 1,
		2, L"__NOUN{_BLOCK:NAME_SECONDARY}", L"__MNOUN{_BLOCK:NAME_SECONDARY}", 0, 1, 1,
		0);
	// the girl[miss] put him[julius] down as thirty - five . 
	// we had her down as Rita Vandermeyer.
	// to put him[man] down as an actor or a lawyer
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"E",
		3, L"verb|put", L"verb|have", L"have|had", 0, 1, 1,
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he/him/Bob
		1, L"adverb|down", 0, 0, 1,
		1, L"preposition|as", 0, 1, 1,
		1, L"verb|being", 0, 0, 1,
		2, L"__NOUN{_BLOCK:NAME_SECONDARY}", L"__MNOUN{_BLOCK:NAME_SECONDARY}", 0, 1, 1,
		0);
	// he[julius] was of middle height , and squarely built to match his[julius] jaw[julius] . 
	//	his[julius] face[julius] was pugnacious but pleasant . 
	// 	no one could have mistaken him[julius] for anything but an American 
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"F",
		1, L"indefinite_pronoun|no one", 0, 1, 1,
		1, L"modal_auxiliary|could", 0, 1, 1,
		1, L"have", 0, 1, 1,
		1, L"verb|mistaken", 0, 1, 1,
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he/him/Bob
		1, L"preposition|for", 0, 1, 1,
		1, L"indefinite_pronoun|anything", 0, 1, 1,
		1, L"preposition|but", 0, 1, 1,
		2, L"__NOUN{_BLOCK:NAME_SECONDARY}", L"__MNOUN{_BLOCK:NAME_SECONDARY}", 0, 1, 1,
		0);
	// Tommy recognized in him[Irish] an Irish Sinn feiner . 
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"G",
		1, L"__C1__S1", 0, 1, 1,
		1, L"__ALLVERB", 0, 1, 1,
		1, L"preposition|in", 0, 1, 1,
		1, L"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // him
		2, L"__NOUN{NAME_SECONDARY}", L"__MNOUN{NAME_SECONDARY}", 0, 1, 1, // a natural actor
		0);
	// I understood her[Janet] to be a niece of Mrs. Vandermeyer's . 
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"R",
		1, L"__C1__S1", 0, 1, 1,
		1, L"__ALLVERB", 0, 1, 1,
		1, L"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // her
		2, L"preposition|to", L"to", 0, 1, 1,
		1, L"be", 0, 1, 1,
		2, L"__NOUN{NAME_SECONDARY}", L"__MNOUN{NAME_SECONDARY}", 0, 1, 1, // a niece
		0);
	// another voice{OWNER: WO -3 another}[which Tommy rather thought was that of Boris replied]
	// another voice{OWNER: WO -3 another}[which Tommy fancied was that of the tall , commanding - looking man whose face had seemed familiar to him]
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"H",
		2, L"__NOUN{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1,
		1, L",", 0, 0, 1,
		1, L"relativizer", 0, 1, 1, // which / who
		1, L"__C1__S1", 0, 1, 1, // Tommy / another person
		1, L"_ADVERB", 0, 0, 1, // rather
		1, L"_THINKPAST", 0, 1, 1, // thought / fancied / was certain
		2, L"_IS", L"is", VERB_PAST, 1, 1, // was
		2, L"relativizer|that", L"demonstrative_determiner|that", 0, 1, 1, // that
		1, L"preposition|of", 0, 1, 1, // of
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Boris
		0);
	// he gave her his name : sir James Peel Edgerton . (see "3")
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"J",
		1, L"personal_pronoun{NAME_PRIMARY}", SINGULAR, 1, 1,
		1, L"verb|gave", 0, 1, 1,
		1, L"possessive_determiner", 0, 0, 1, // his, her
		1, L"possessive_determiner{NAME_PRIMARY}", SINGULAR, 1, 1, // his, her
		2, L"noun|name", L"noun|birthname", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		2, L"preposition|as", L":", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"quotes", OPEN_INFLECTION, 0, 1,
		1, L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"quotes", CLOSE_INFLECTION, 0, 1, 0);
	//  he nevertheless conveyed the impression of a big man
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"K",
		2, L"__NOUN{NAME_PRIMARY}", L"__MNOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L"adverb", 0, 0, 1,
		1, L"verb|conveyed", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"determiner|the", 0, 1, 1,
		2, L"noun|impression", L"noun|look", 0, 1, 1,
		1, L"preposition|of", 0, 0, 1,
		1, L"__RENOUN{_BLOCK:RE_OBJECT:NAME_SECONDARY}", 0, 1, 1,
		0);
	//  a woman dressed as a hospital nurse
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"M",
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, L"adverb", 0, 0, 1,
		1, L"verb|dressed", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"preposition|as", 0, 1, 1,
		2, L"__NOUN{NAME_SECONDARY}", L"__MNOUN{NAME_SECONDARY}", 0, 1, 1,
		0);
	// medical man written all over him
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"N",
		1, L"__RENOUN{NAME_SECONDARY}", 0, 1, 1, // cannot be __NOUN because of incorrectly disambiguated noun grouping - 
		1, L"adverb", 0, 0, 1,                   // I[julius] was lying in bed[bed] with a hospital nurse ( not Whittington's one[nurse] )
		1, L"verb|written", 0, 1, 1,						 //  on one side of me[julius] , and a little black - bearded man with gold glasses , 
		1, L"adverb|all", 0, 1, 1,               // and medical man written all[all] over him[man] , on the other[side] .
		1, L"preposition|over", 0, 1, 1,
		1, L"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		0);
	// He[tommy] recognized it[voice] at once for that of the bearded and efficient German[man] ,
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"P",
		1, L"__C1__S1", 0, 1, 1,
		1, L"verb|recognized", 0, 1, 1,
		5, L"personal_pronoun_nominative{N_AGREE:NAME_SECONDARY}", L"personal_pronoun_accusative{N_AGREE:NAME_SECONDARY}", L"personal_pronoun{N_AGREE:NAME_SECONDARY}", L"noun{NAME_SECONDARY}", L"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, L"_META_PP", 0, 0, 1, // at once
		2, L"preposition|for", L"preposition|as", 0, 1, 1,
		2, L"__NOUN{NAME_PRIMARY}", L"__MNOUN{NAME_PRIMARY}", 0, 1, 1, // a natural actor
		0);
	// they knew him now for a spy
	cPattern::create(L"_META_NAME_EQUIVALENCE{_IGNORE}", L"S",
		1, L"__C1__S1", 0, 1, 1,
		1, L"verb|knew", VERB_PAST, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		4, L"personal_pronoun_accusative{N_AGREE:NAME_PRIMARY}", L"personal_pronoun{N_AGREE:NAME_PRIMARY}", L"noun{NAME_PRIMARY}", L"_NAME{NAME_PRIMARY}", 0, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		1, L"preposition|for", 0, 1, 1,
		1, L"__NOUN{NAME_SECONDARY}", 0, 1, 1, // a natural actor
		0);

	// patterns in quotes that indicate an entity has become physically present
	cPattern::create(L"_META_ANNOUNCE{_IGNORE:_ONLY_BEGIN_MATCH}", L"1",
		1, L"noun|here", 0, 1, 1,
		2, L"verb|is", L"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		3, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", L"__MNOUN{NAME_PRIMARY}", 0, 1, 1, // Bob / another knock
		0);
	cPattern::create(L"_META_ANNOUNCE{_IGNORE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", L"2",
		1, L"noun|here", 0, 1, 1,
		3, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", L"__MNOUN{NAME_PRIMARY}", 0, 1, 1, // He
		2, L"verb|is", L"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, L"_ADVERB", 0, 0, 1,
		0);
	// patterns in quotes that indicate the person speaking [ over the phone ]
	// Bob speaking.
	cPattern::create(L"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", L"1",
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"verb|speaking", VERB_PRESENT_PARTICIPLE, 1, 1,
		0);
	// Bob here.
	cPattern::create(L"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", L"2",
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"noun|here", 0, 1, 1,
		0);
	// The following pattern is only indicative of the speaker if
	// we are sure that this is a conversation over the phone - but how can we be sure?
	// This is Bob.
	cPattern::create(L"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", L"3",
		1, L"demonstrative_determiner|this", 0, 1, 1,
		2, L"verb|is", L"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		0);
	// The Sinn Feiner was speaking
	cPattern::create(L"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", L"4",
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"verb|was", VERB_PAST, 1, 1,
		1, L"verb|speaking", 0, 1, 1,
		0);
	// I am Dr. Hall / you are Conrad
	cPattern::create(L"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", L"5",
		2, L"personal_pronoun_nominative|i{NAME_SECONDARY}", L"personal_pronoun|you{NAME_SECONDARY}", 0, 1, 1,
		2, L"is|am", L"is|are", 0, 1, 1,
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		0);
	// What is your name?
	cPattern::create(L"_META_SPEAKER_QUERY{_IGNORE:_ONLY_END_MATCH:_QUESTION}", L"1",
		1, L"relativizer|what", 0, 1, 1,
		3, L"verb|is", L"is|ishas", L"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, L"possessive_determiner{NAME_ABOUT}", 0, 1, 1,
		1, L"noun|name", 0, 1, 1,
		0);
	// Who are you? / Who is Annie?
	cPattern::create(L"_META_SPEAKER_QUERY{_IGNORE:_ONLY_END_MATCH:_QUESTION}", L"2",
		1, L"relativizer|who", 0, 1, 1,
		3, L"is|are", L"is|is", L"is|ishas", VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		2, L"personal_pronoun{NAME_ABOUT}", L"_NAME{NAME_ABOUT}", 0, 1, 1,
		0);
	// Under the name of -- --
	cPattern::create(L"_META_SPEAKER_QUERY{_IGNORE:_ONLY_END_MATCH:_QUESTION}", L"3",
		1, L"preposition|under", 0, 1, 1,
		1, L"determiner|the", 0, 1, 1,
		1, L"noun|name", 0, 1, 1,
		1, L"preposition|of", 0, 1, 1,
		1, L"dash{NAME_ABOUT}", 0, 1, 3,
		0);
	// What girl?
	cPattern::create(L"_META_SPEAKER_QUERY{_IGNORE:_STRICT_NO_MIDDLE_MATCH:_QUESTION}", L"4",
		2, L"relativizer|what", L"relativizer|which", 0, 1, 1,
		1, L"noun{NAME_ABOUT}", 0, 1, 1,
		0);
	// , sir.
	cPattern::create(L"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE{_IGNORE:_ONLY_END_MATCH}", L"",
		1, L",", 0, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"_PP", 0, 1, 1, // sir / of course
		0);
	// Bob. / 'Ouse parlourmaid
	cPattern::create(L"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", L"1",
		2, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);
	// My name is Bob.
	cPattern::create(L"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", L"2",
		1, L"possessive_determiner{NAME_ABOUT}", 0, 1, 1,
		1, L"noun|name", 0, 1, 1,
		2, L"verb|is", L"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);
	// You can call me Bob.
	cPattern::create(L"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", L"3",
		1, L"personal_pronoun|you", 0, 1, 1,
		1, L"modal_auxiliary|can", 0, 1, 1,
		1, L"verb|call", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		1, L"personal_pronoun_accusative{NAME_ABOUT}", 0, 1, 1,
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);
	// Call me Bob.
	cPattern::create(L"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", L"4",
		1, L"verb|call", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		1, L"personal_pronoun_accusative{NAME_ABOUT}", 0, 1, 1,
		1, L"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);

	// Bob came with him
	cPattern::create(L"_META_GROUP{_IGNORE}", L"1",
		2, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"__ALLVERB", 0, 1, 1,
		1, L"preposition|with", 0, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// With him came Mr. Bill
	cPattern::create(L"_META_GROUP{_IGNORE:_ONLY_BEGIN_MATCH}", L"2",
		2, L"preposition|with", L"preposition|by", 0, 1, 1,
		3, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", L"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"__ALLVERB", 0, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// He ran next to Mr. Bill.
	cPattern::create(L"_META_GROUP{_IGNORE}", L"3",
		2, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"__ALLVERB", 0, 1, 1,
		1, L"preposition|next", 0, 1, 1,
		1, L"to", 0, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// Next to him ran Mr. Bill.
	cPattern::create(L"_META_GROUP{_IGNORE:_ONLY_BEGIN_MATCH}", L"4",
		1, L"preposition|next", 0, 1, 1,
		1, L"preposition|to", 0, 1, 1,
		3, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", L"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"__ALLVERB", 0, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// He was accompanied by Mr. Bill
	cPattern::create(L"_META_GROUP{_IGNORE}", L"5",
		2, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		2, L"_IS", L"is", 0, 0, 1, // (must be optional for) the man known as Number One
		4, L"verb|accompanied", L"verb|followed", L"verb|led", L"verb|joined", 0, 1, 1,
		1, L"preposition|by", 0, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// He accompanied Tuppence.
	cPattern::create(L"_META_GROUP{_IGNORE}", L"6",
		2, L"_NAME{NAME_PRIMARY}", L"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, L"_HAVE", 0, 0, 1, // He had joined them
		4, L"verb|accompanied", L"verb|followed", L"verb|led", L"verb|joined", 0, 1, 1,
		2, L"_NAME{NAME_SECONDARY}", L"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
}

void cSource::equivocateObjects(int where, int eTo, int eFrom)
{
	LFS
		if (eTo < 0) return;
	bool ambiguousGender = objects[eTo].male && objects[eTo].female;
	switch (eFrom)
	{
	case cObject::eOBJECTS::UNKNOWN_OBJECT: return;
	case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE:
		objects[eTo].male = true;
		objects[eTo].female = objects[eTo].neuter = objects[eTo].plural = false;
		if (ambiguousGender)
			addDefaultGenderedAssociatedNouns(eTo);
		return;
	case cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE:
		objects[eTo].female = true;
		objects[eTo].male = objects[eTo].neuter = objects[eTo].plural = false;
		if (ambiguousGender)
			addDefaultGenderedAssociatedNouns(eTo);
		return;
	case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE_OR_FEMALE:
		objects[eTo].neuter = objects[eTo].plural = false;
		return;
	case cObject::eOBJECTS::OBJECT_UNKNOWN_NEUTER:
		objects[eTo].neuter = true;
		objects[eTo].female = objects[eTo].male = objects[eTo].plural = false;
		return;
	case cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL:
		objects[eTo].plural = true;
		return;
	default:;
	}
	if ((objects[eFrom].male ^ objects[eFrom].female) && (!(objects[eTo].male ^ objects[eTo].female) || objects[eFrom].objectClass == PRONOUN_OBJECT_CLASS))
	{
		objects[eTo].male = objects[eFrom].male;
		objects[eTo].female = objects[eFrom].female;
	}
	//objects[eTo].neuter=objects[eFrom].neuter;
	//objects[eTo].plural=objects[eFrom].plural;
	if (objects[eTo].objectClass != PRONOUN_OBJECT_CLASS && objects[eTo].objectClass != REFLEXIVE_PRONOUN_OBJECT_CLASS && objects[eTo].objectClass != RECIPROCAL_PRONOUN_OBJECT_CLASS &&
		objects[eFrom].objectClass != PRONOUN_OBJECT_CLASS && objects[eFrom].objectClass != REFLEXIVE_PRONOUN_OBJECT_CLASS && objects[eFrom].objectClass != RECIPROCAL_PRONOUN_OBJECT_CLASS &&
		find(objects[eTo].aliases.begin(), objects[eTo].aliases.end(), eFrom) == objects[eTo].aliases.end())
	{
		// a young man is not an alias.  my cousin is an alias. 
		// a young man is already incorporated by adjective/noun synonym matching
		// a young man is also not specific enough, since aliases are considered a very strong (+10000) association
		if (objects[eFrom].objectClass != GENDERED_GENERAL_OBJECT_CLASS || m[objects[eFrom].originalLocation].queryForm(L"pinr") < 0)
		{
			wstring tmpstr, tmpstr2;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Object %s gained alias %s (1).", where, objectString(eTo, tmpstr, true).c_str(), objectString(eFrom, tmpstr2, true).c_str());
			objects[eFrom].aliases.push_back(eTo);
			//if (objects[eTo].objectClass==NAME_OBJECT_CLASS)
			objects[eTo].aliases.push_back(eFrom);
		}
	}
}

struct {
	const wchar_t* nc;
	int num;
} numeralCardinalMap[] = {
	{ L"zero", 0 },
	{ L"naught", 0 },
	{ L"one", 1 }, { L"two", 2 }, { L"three", 3 }, { L"four", 4 }, { L"five", 5 }, { L"six", 6 }, { L"seven", 7 }, { L"eight", 8 }, { L"nine", 9 }, { L"ten", 10 },
	{ L"eleven", 11 }, { L"twelve", 12 }, { L"dozen", 12 }, { L"thirteen", 13 }, { L"fourteen", 14 }, { L"fifteen", 15 }, { L"sixteen", 16 }, { L"seventeen", 17 }, { L"eighteen", 18 }, { L"nineteen", 19 }, { L"twenty", 20 },
	{ L"umpteen", 15 }, { L"gross", 144 },
	{ L"thirty", 30 }, { L"forty", 40 }, { L"fifty", 50 }, { L"sixty", 60 }, { L"seventy", 70 }, { L"eighty", 80 }, { L"ninety", 90 },
	{ L"hundred", 100 }, { L"thousand", 1000 }, { L"million", 1000000 }, { L"billion", 1000000000 },
	{ NULL, -1 } };
int mapNumeralCardinal(const wstring& word)
{
	LFS
		int agei = 0;
	for (; numeralCardinalMap[agei].nc && numeralCardinalMap[agei].nc != word; agei++);
	return numeralCardinalMap[agei].num;
}

struct {
	const wchar_t* nc;
	int num;
} numeralOrdinalMap[] = {
	{ L"zeroth", 0 },
	{ L"first", 1 }, { L"second", 2 }, { L"third", 3 }, { L"fourth", 4 }, { L"fifth", 5 }, { L"sixth", 6 }, { L"seventh", 7 }, { L"eighth", 8 }, { L"ninth", 9 }, { L"tenth", 10 },
	{ L"eleventh", 11 }, { L"twelfth", 12 }, { L"thirteenth", 13 }, { L"fourteenth", 14 }, { L"fifteenth", 15 }, { L"sixteenth", 16 }, { L"seventeenth", 17 }, { L"eighteenth", 18 }, { L"nineteenth", 19 }, { L"twentieth", 0 },
	{ L"umpteenth", 15 },
	{ L"thirtieth", 30 }, { L"fortieth", 40 }, { L"fiftieth", 50 }, { L"sixtieth", 60 }, { L"seventieth", 70 }, { L"eightieth", 80 }, { L"ninetieth", 90 },
	{ L"hundredth", 100 }, { L"thousandth", 1000 }, { L"millionth", 1000000 }, { L"billionth", 1000000000 },
	{ NULL, -1 } };
int mapNumeralOrdinal(const wstring& word)
{
	LFS
		if (word.length() > 2 && iswdigit(word[0]) && word[word.length() - 2] == 't' && word[word.length() - 1] == 'h')
		{
			return _wtoi(word.c_str());
		}
	int agei = 0;
	for (; numeralOrdinalMap[agei].nc && numeralOrdinalMap[agei].nc != word; agei++);
	return numeralOrdinalMap[agei].num;
}

// Julius was about thirty-five.
bool cSource::ageDetection(int where, int primary, int secondary)
{
	LFS
		// This was one of the ...
		if (m[objects[secondary].originalLocation].relPrep >= 0 && m[m[objects[secondary].originalLocation].relPrep].word->first == L"of")
			return false;
	// of whom the German was one
	if (m[objects[secondary].originalLocation].objectRole & (EXTENDED_ENCLOSING_ROLE | NONPAST_ENCLOSING_ROLE | NONPRESENT_ENCLOSING_ROLE | SENTENCE_IN_REL_ROLE | SENTENCE_IN_ALT_REL_ROLE))
		return false;
	bool yearsFound = false;
	int age = 0, agei;
	for (int I = objects[secondary].begin; I < objects[secondary].end; I++)
		if (m[I].queryWinnerForm(numeralCardinalForm) >= 0)
		{
			if ((agei = mapNumeralCardinal(m[I].word->first)) < 0) return false;
			age += agei;
		}
		else if (m[I].queryWinnerForm(dashForm) < 0 && m[I].queryWinnerForm(adjectiveForm) < 0)
		{
			if (!(m[I].word->second.timeFlags & T_LENGTH))
				return false;
			// add additional code for other values later
			if (m[I].word->first != L"years" && m[I].word->first != L"year")
				return false;
			yearsFound = true;
		}
	if (!age || (age == 1 && !yearsFound)) return false;
	if (debugTrace.traceSpeakerResolution)
	{
		wstring tmpstr;
		lplog(LOG_RESOLUTION, L"%06d:Object %s has age %d.", where, objectString(primary, tmpstr, true).c_str(), age);
	}
	if (age < 40 && find(objects[primary].associatedAdjectives.begin(), objects[primary].associatedAdjectives.end(), Words.gquery(L"young")) == objects[primary].associatedAdjectives.end())
		objects[primary].associatedAdjectives.push_back(Words.gquery(L"young"));
	if (age >= 50 && find(objects[primary].associatedAdjectives.begin(), objects[primary].associatedAdjectives.end(), Words.gquery(L"old")) == objects[primary].associatedAdjectives.end())
		objects[primary].associatedAdjectives.push_back(Words.gquery(L"old"));
	return true;
}

// in secondary quotes, inPrimaryQuote=false
bool cSource::evaluateMetaNameEquivalence(int where, vector <cTagLocation>& tagSet, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
		int primaryTag = findOneTag(tagSet, L"NAME_PRIMARY", -1), secondaryTag = findOneTag(tagSet, L"NAME_SECONDARY", -1);
	if (primaryTag < 0 || secondaryTag < 0) return false;
	int wherePrimary = tagSet[primaryTag].sourcePosition, whereSecondary = tagSet[secondaryTag].sourcePosition;
	if (tagSet[primaryTag].len > 1 && m[wherePrimary].principalWherePosition >= 0) // could be an adjective
		wherePrimary = m[wherePrimary].principalWherePosition;
	wstring tmpstr;
	int primaryNameObject = m[wherePrimary].getObject();
	int tmpLastRelativePhrase = lastRelativePhrase, tmpLastBeginS1 = lastBeginS1, tmpLastQ2 = lastQ2, tmpLastVerb = lastVerb;
	for (int J = where + 1; J < wherePrimary; J++)
	{
		if (m[J].pma.queryPattern(L"_REL1") != -1)
			tmpLastRelativePhrase = J;
		if (m[J].pma.queryPattern(L"__S1") != -1)
			tmpLastBeginS1 = J;
		if (m[J].pma.queryPattern(L"_Q2") != -1)
			tmpLastQ2 = J;
		if (m[J].hasVerbRelations)
			tmpLastVerb = J;
	}
	// MNOUN?
	bool scanForMultiple = false;
	if (tagSet[secondaryTag].len > 1) // could be an adjective
	{
		cPatternMatchArray::tPatternMatch* pma = m[whereSecondary].pma.content;
		for (unsigned int PMAElement = 0; PMAElement < m[whereSecondary].pma.count && !scanForMultiple; PMAElement++, pma++)
			if (pma->len == tagSet[secondaryTag].len && patterns[pma->getPattern()]->hasTag(MNOUN_TAG))
				scanForMultiple = true;
		// could be an adjective
		if (m[whereSecondary].principalWherePosition >= 0)
			whereSecondary = m[whereSecondary].principalWherePosition;
	}
	// primary could also be a MNOUN
	if (tagSet[primaryTag].len > 1)
	{
		cPatternMatchArray::tPatternMatch* pma = m[wherePrimary].pma.content;
		for (unsigned int PMAElement = 0; PMAElement < m[wherePrimary].pma.count; PMAElement++, pma++)
			if (pma->len == tagSet[primaryTag].len && patterns[pma->getPattern()]->hasTag(MNOUN_TAG))
			{
				if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
				{
					lplog(LOG_RESOLUTION, L"%06d:Metaname equivalence rejected (primary is multiple) for primary %d:[%s]", where, wherePrimary, objectString(primaryNameObject, tmpstr, false).c_str());
				}
				return false;
			}
	}
	vector <int> objectsResolved;
	resolveObject(wherePrimary, false, inPrimaryQuote, inSecondaryQuote, tmpLastBeginS1, tmpLastRelativePhrase, tmpLastQ2, tmpLastVerb, false, false, false);
	objectsResolved.push_back(wherePrimary);
	if (m[wherePrimary].objectMatches.size() == 1)
		primaryNameObject = m[wherePrimary].objectMatches[0].object;
	if (primaryNameObject == cObject::eOBJECTS::UNKNOWN_OBJECT)
	{
		m[wherePrimary].flags &= ~cWordMatch::flagObjectResolved;
		return false;
	}
	vector <int> secondaryNameObjects, eraseREObjects;
	tmpLastRelativePhrase = lastRelativePhrase;
	tmpLastBeginS1 = lastBeginS1;
	tmpLastQ2 = lastQ2;
	for (int J = where + 1; J < whereSecondary; J++)
	{
		if (m[J].pma.queryPattern(L"_REL1") != -1)
			tmpLastRelativePhrase = J;
		if (m[J].pma.queryPattern(L"__S1") != -1)
			tmpLastBeginS1 = J;
		if (m[J].pma.queryPattern(L"_Q2") != -1)
			tmpLastQ2 = J;
	}
	if (whereSecondary < 0 || scanForMultiple)
	{
		bool acceptPrepObjects = true;
		bool stopOnPreposition = (m[tagSet[secondaryTag].sourcePosition].objectRole & MPLURAL_ROLE) == 0;
		for (unsigned int I = tagSet[secondaryTag].sourcePosition; I < tagSet[secondaryTag].sourcePosition + tagSet[secondaryTag].len; I++)
			if (m[I].queryWinnerForm(prepositionForm) >= 0)
			{
				acceptPrepObjects = false;
				if (stopOnPreposition) break;
			}
			else if (m[I].getObject() >= 0 && (acceptPrepObjects || !(m[I].objectRole & PREP_OBJECT_ROLE)))
			{
				// whether to put him[man] down as an actor or a lawyer 
				eraseREObjects.push_back((!(m[I].objectRole & RE_OBJECT_ROLE)) ? I : -1);
				m[I].objectRole |= RE_OBJECT_ROLE; // prevent secondary objects from becoming 'live'
				resolveObject(I, false, inPrimaryQuote, inSecondaryQuote, tmpLastBeginS1, tmpLastRelativePhrase, tmpLastQ2, tmpLastVerb, false, false, false);
				objectsResolved.push_back(I);
				if (m[I].objectMatches.size() == 1)
					secondaryNameObjects.push_back(m[I].objectMatches[0].object);
				else
					secondaryNameObjects.push_back(m[I].getObject());
			}
		if (secondaryNameObjects.empty()) return false;
	}
	else
	{
		eraseREObjects.push_back((!(m[whereSecondary].objectRole & RE_OBJECT_ROLE)) ? whereSecondary : -1);
		objectsResolved.push_back(whereSecondary);
		m[whereSecondary].objectRole |= RE_OBJECT_ROLE; // prevent secondary objects from becoming 'live'
		resolveObject(whereSecondary, false, inPrimaryQuote, inSecondaryQuote, tmpLastBeginS1, tmpLastRelativePhrase, tmpLastQ2, tmpLastVerb, false, false, false);
		if (m[whereSecondary].objectMatches.size() == 1 || (m[whereSecondary].objectMatches.size() == 2 && m[whereSecondary].objectMatches[1].object == primaryNameObject))
			secondaryNameObjects.push_back(m[whereSecondary].objectMatches[0].object);
		else if (m[whereSecondary].objectMatches.size() == 2 && m[whereSecondary].objectMatches[0].object == primaryNameObject)
			secondaryNameObjects.push_back(m[whereSecondary].objectMatches[1].object);
		else if (m[whereSecondary].getObject() >= 0)
			secondaryNameObjects.push_back(m[whereSecondary].getObject());
	}
	// He[boris] gave his[boris] name as Count Stepanov . 
	if (m[wherePrimary].objectMatches.size() == 2 && secondaryNameObjects.size() == 1 && in(secondaryNameObjects[0], m[wherePrimary].objectMatches) != m[wherePrimary].objectMatches.end())
		primaryNameObject = (m[wherePrimary].objectMatches[0].object == secondaryNameObjects[0]) ? m[wherePrimary].objectMatches[1].object : m[wherePrimary].objectMatches[0].object;
	if (inPrimaryQuote && (m[wherePrimary].word->second.inflectionFlags & (FIRST_PERSON | SECOND_PERSON)) != 0 && secondaryNameObjects.size() == 1 &&
		objects[secondaryNameObjects[0]].objectClass == NAME_OBJECT_CLASS && tempSpeakerGroup.speakers.find(secondaryNameObjects[0]) == tempSpeakerGroup.speakers.end())
	{
		m[whereSecondary].objectRole |= ((m[wherePrimary].word->second.inflectionFlags & FIRST_PERSON) ? IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE : IN_QUOTE_REFERRING_AUDIENCE_ROLE);
		if (!speakerGroupsEstablished)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Insert into speaker group (%d,%d) from metaname equivalence:%s", where, wherePrimary, whereSecondary, objectString(secondaryNameObjects[0], tmpstr, true).c_str());
			tempSpeakerGroup.speakers.insert(secondaryNameObjects[0]);
		}
		else if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:Established %d:%s as %s from metaname equivalence", where, whereSecondary, objectString(secondaryNameObjects[0], tmpstr, true).c_str(),
				((m[wherePrimary].word->second.inflectionFlags & FIRST_PERSON) ? L"speaker" : L"audience"));
	}
	if (inPrimaryQuote && (m[wherePrimary].word->second.inflectionFlags & THIRD_PERSON) && secondaryNameObjects.size() == 1 &&
		objects[secondaryNameObjects[0]].objectClass == NAME_OBJECT_CLASS && tempSpeakerGroup.speakers.find(secondaryNameObjects[0]) == tempSpeakerGroup.speakers.end() && !speakerGroupsEstablished)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:Reject from speaker group (%d,%d) from third-person metaname equivalence:%s", where, wherePrimary, whereSecondary, objectString(secondaryNameObjects[0], tmpstr, true).c_str());
		metaNameOthersInSpeakerGroups.push_back(whereSecondary);
	}
	/*
	bool oneIn,allIn;
	if (intersect(wherePrimary,secondaryNameObjects,oneIn,allIn))
	{
		if (t.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:primary and secondary are equal",where);
		return true;
	}
	*/
	bool atLeastOneSecondarySucceeded = false;
	for (unsigned int sno = 0; sno < secondaryNameObjects.size(); sno++)
	{
		int secondaryNameObject = secondaryNameObjects[sno];
		if (primaryNameObject == secondaryNameObject || primaryNameObject < 0 || secondaryNameObject < 0 ||
			find(objects[primaryNameObject].aliases.begin(), objects[primaryNameObject].aliases.end(), secondaryNameObject) != objects[primaryNameObject].aliases.end())
			continue;
		int primaryClass = objects[primaryNameObject].objectClass, secondaryClass = objects[secondaryNameObject].objectClass;
		bool primaryAcceptable = primaryClass == NAME_OBJECT_CLASS || primaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || primaryClass == GENDERED_DEMONYM_OBJECT_CLASS || primaryClass == GENDERED_RELATIVE_OBJECT_CLASS || primaryClass == BODY_OBJECT_CLASS;
		bool secondaryAcceptable = secondaryClass == NAME_OBJECT_CLASS || secondaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || secondaryClass == GENDERED_DEMONYM_OBJECT_CLASS || secondaryClass == GENDERED_RELATIVE_OBJECT_CLASS || secondaryClass == BODY_OBJECT_CLASS || secondaryClass == GENDERED_GENERAL_OBJECT_CLASS;
		if ((!primaryAcceptable && !secondaryAcceptable) ||
			(primaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && secondaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS) ||
			(primaryClass == GENDERED_DEMONYM_OBJECT_CLASS && secondaryClass == GENDERED_DEMONYM_OBJECT_CLASS) ||
			(primaryClass == GENDERED_RELATIVE_OBJECT_CLASS && secondaryClass == GENDERED_RELATIVE_OBJECT_CLASS) ||
			(primaryClass == BODY_OBJECT_CLASS && secondaryClass == BODY_OBJECT_CLASS) ||
			primaryClass == PRONOUN_OBJECT_CLASS || primaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS || primaryClass == NON_GENDERED_BUSINESS_OBJECT_CLASS ||
			secondaryClass == PRONOUN_OBJECT_CLASS || secondaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS || secondaryClass == NON_GENDERED_BUSINESS_OBJECT_CLASS ||
			(objects[secondaryNameObject].plural != objects[secondaryNameObject].plural))
		{
			if (secondaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS && ageDetection(where, primaryNameObject, secondaryNameObject))
				continue;
			int inflectionFlags = m[wherePrimary].word->second.inflectionFlags;
			if ((((inflectionFlags & MALE_GENDER) == MALE_GENDER) ^ ((inflectionFlags & FEMALE_GENDER) == FEMALE_GENDER)) && m[wherePrimary].objectMatches.size() > 1)
			{
				wstring tmpstr2;
				lplog(LOG_RESOLUTION, L"%06d:Metaname equivalence rejected (uncertain) for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
				continue;
			}
			else
			{
				// the Defense Intelligence Agency cryptonym "Curveball"
				// the overall object is nongen, but the specific referring object is a name
				int maxLen = 1, element = -1;
				if ((objects[secondaryNameObject].begin != whereSecondary && secondaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
					((element = m[whereSecondary].pma.queryPattern(L"_NAME", maxLen)) != -1)) || (tagSet[secondaryTag].len == 1 && (m[whereSecondary].flags & cWordMatch::flagFirstLetterCapitalized)))
				{
					if (m[wherePrimary].objectMatches.empty() || objects[m[wherePrimary].objectMatches[0].object].begin != whereSecondary)
					{
						if (identifyObject(-1, whereSecondary, element, false, -1, -1) >= 0)
						{
							secondaryNameObject = objects.size() - 1;
							if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
							{
								wstring tmpstr2;
								lplog(LOG_RESOLUTION, L"%06d:Metaname equivalence accepted (class change) for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
							}
						}
					}
				}
				else
				{
					if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
					{
						wstring tmpstr2;
						lplog(LOG_RESOLUTION, L"%06d:Metaname equivalence rejected (wrong class) for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
					}
					if (eraseREObjects[sno] != -1)
						m[eraseREObjects[sno]].objectRole &= ~RE_OBJECT_ROLE;
					for (unsigned int J = 0; J < objectsResolved.size(); J++)
						m[objectsResolved[J]].flags &= ~cWordMatch::flagObjectResolved;
					continue;
				}
			}
		}
		if (objects[primaryNameObject].plural != objects[secondaryNameObject].plural)
		{
			if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
			{
				wstring tmpstr2;
				lplog(LOG_RESOLUTION, L"%06d:Metaname equivalence rejected (different plurality) for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
			}
			continue;
		}
		// somebody named Jane Finn should be equivocated to Jane Finn
		//if (overlaps(objects.begin()+primaryNameObject,objects.begin()+secondaryNameObject))
		//{
		//	if (t.traceNameResolution || t.traceObjectResolution)
		//	{
		//		wstring tmpstr,tmpstr2;
		//		lplog(LOG_RESOLUTION,L"%06d:Metaname equivalence rejected (overlaps) for secondary %d:[%s] to primary %d:[%s]",where,whereSecondary,objectString(secondaryNameObject,tmpstr,false).c_str(),wherePrimary,objectString(primaryNameObject,tmpstr2,false).c_str());
		//	}
		//	continue;
		//}
		bool switched;
		// prefer to match to a name or a metagroup
		if (switched =
			((secondaryClass == GENDERED_GENERAL_OBJECT_CLASS ||
				secondaryClass == BODY_OBJECT_CLASS ||
				secondaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
				secondaryClass == GENDERED_DEMONYM_OBJECT_CLASS ||
				secondaryClass == GENDERED_RELATIVE_OBJECT_CLASS) &&
				(primaryClass == NAME_OBJECT_CLASS || primaryClass == META_GROUP_OBJECT_CLASS)) ||
			(secondaryClass == BODY_OBJECT_CLASS &&
				(primaryClass == GENDERED_GENERAL_OBJECT_CLASS ||
					primaryClass == BODY_OBJECT_CLASS ||
					primaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
					primaryClass == GENDERED_DEMONYM_OBJECT_CLASS ||
					primaryClass == GENDERED_RELATIVE_OBJECT_CLASS)))
		{
			int tmp = secondaryNameObject, tmp2 = secondaryClass, tmp3 = whereSecondary;
			secondaryNameObject = primaryNameObject;
			secondaryClass = primaryClass;
			whereSecondary = wherePrimary;
			primaryNameObject = tmp;
			primaryClass = tmp2;
			wherePrimary = tmp3;
		}
		atLeastOneSecondarySucceeded = true;
		for (vector <cWordMatch>::iterator im = m.begin() + m[wherePrimary].beginObjectPosition, imEnd = m.begin() + m[wherePrimary].endObjectPosition; im != imEnd; im++)
		{
			im->objectRole |= META_NAME_EQUIVALENCE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE, L"%06d:Removed HAIL role (evaluateMetaNameEquivalence).", where);
			// prevents HAIL re-evaluation on mistaken HAIL 
			im->objectRole &= ~HAIL_ROLE; // [tommy:tuppence] Are you[tuppence] proposing a third advertisement : Wanted , female crook[tuppence] , answering to the name[name] of Rita ? ”
		}
		//  he[man] nevertheless conveyed the impression of a big man
		if (primaryClass == GENDERED_GENERAL_OBJECT_CLASS && secondaryClass == GENDERED_GENERAL_OBJECT_CLASS)
		{
			if ((objects[secondaryNameObject].associatedNouns.size() || objects[secondaryNameObject].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
			{
				wstring nouns, adjectives;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:Object %s original associated nouns (%s) and adjectives (%s) taking from %d:%s (4)",
						where, objectString(primaryNameObject, tmpstr, false).c_str(), wordString(objects[primaryNameObject].associatedNouns, nouns).c_str(), wordString(objects[primaryNameObject].associatedAdjectives, adjectives).c_str(),
						whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str());
			}
			narrowGender(whereSecondary, primaryNameObject);
			for (vector <tIWMM>::iterator ai = objects[secondaryNameObject].associatedAdjectives.begin(), aiEnd = objects[secondaryNameObject].associatedAdjectives.end(); ai != aiEnd; ai++)
				if (find(objects[primaryNameObject].associatedAdjectives.begin(), objects[primaryNameObject].associatedAdjectives.end(), *ai) == objects[primaryNameObject].associatedAdjectives.end())
					objects[primaryNameObject].associatedAdjectives.push_back(*ai);
			for (vector <tIWMM>::iterator ai = objects[secondaryNameObject].associatedNouns.begin(), aiEnd = objects[secondaryNameObject].associatedNouns.end(); ai != aiEnd; ai++)
				if (find(objects[primaryNameObject].associatedNouns.begin(), objects[primaryNameObject].associatedNouns.end(), *ai) == objects[primaryNameObject].associatedNouns.end())
					objects[primaryNameObject].associatedNouns.push_back(*ai);
			if (!(m[wherePrimary].word->second.flags & cSourceWordInfo::genericGenderIgnoreMatch))
				objects[primaryNameObject].updateGenericGender(where, m[whereSecondary].word, objects[secondaryNameObject].objectGenericAge, L"metaNameEquivalence", debugTrace);
			if ((objects[secondaryNameObject].associatedNouns.size() || objects[secondaryNameObject].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
			{
				wstring nouns, adjectives;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:Object %s associated nouns (%s) and adjectives (%s) (3)",
						where, objectString(primaryNameObject, tmpstr, false).c_str(), wordString(objects[secondaryNameObject].associatedNouns, nouns).c_str(), wordString(objects[secondaryNameObject].associatedAdjectives, adjectives).c_str());
			}
			if (objects[primaryNameObject].relativeClausePM < 0 && objects[secondaryNameObject].relativeClausePM >= 0)
			{
				objects[primaryNameObject].relativeClausePM = objects[secondaryNameObject].relativeClausePM;
				objects[primaryNameObject].whereRelativeClause = objects[secondaryNameObject].whereRelativeClause;
				objects[secondaryNameObject].relativeClausePM = objects[secondaryNameObject].whereRelativeClause = -1; // to prevent this object from becoming more visible to focus
			}
			continue;
		}
		// do not arbitrarily assign a name to another if they are not positively absolutely identified and both previously existed
		// The man known as Number One
		if (currentSpeakerGroup >= 2)
		{
			int lastSpeakerGroupEnd = (speakerGroupsEstablished) ? speakerGroups[currentSpeakerGroup - 1].sgEnd : speakerGroups[currentSpeakerGroup - 2].sgEnd;
			if ((primaryClass == NAME_OBJECT_CLASS || primaryClass == GENDERED_GENERAL_OBJECT_CLASS) && secondaryClass == NAME_OBJECT_CLASS &&
				(m[wherePrimary].objectMatches.size() || m[whereSecondary].objectMatches.size()) && currentSpeakerGroup &&
				objects[primaryNameObject].firstLocation < lastSpeakerGroupEnd &&
				objects[secondaryNameObject].firstLocation < lastSpeakerGroupEnd)
			{
				wstring tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:meta resolution refused between %d:%s and %d:%s (%d)!", where,
						objects[primaryNameObject].originalLocation, objectString(objects.begin() + primaryNameObject, tmpstr, false).c_str(),
						objects[secondaryNameObject].originalLocation, objectString(objects.begin() + secondaryNameObject, tmpstr2, false).c_str(),
						speakerGroups[currentSpeakerGroup - 1].sgEnd);
				if (m[wherePrimary].getObject() != primaryNameObject && m[whereSecondary].getObject() == secondaryNameObject)
				{
					m[wherePrimary].objectMatches.clear();
					m[wherePrimary].objectMatches.push_back(cOM(secondaryNameObject, SALIENCE_THRESHOLD));
					objects[secondaryNameObject].locations.push_back(wherePrimary);
					objects[secondaryNameObject].updateFirstLocation(wherePrimary);
				}
				continue;
			}
		}
		if (sourceType == cSource::INTERACTIVE_SOURCE_TYPE)
		{
			// check if already there
			if (in(secondaryNameObject, m[wherePrimary].objectMatches) == m[wherePrimary].objectMatches.end())
			{
				objects.push_back(objects[secondaryNameObject]);
				secondaryNameObject = objects.size() - 1;
				m[wherePrimary].objectMatches.push_back(cOM(secondaryNameObject, SALIENCE_THRESHOLD));
				objects[secondaryNameObject].begin = m[whereSecondary].beginObjectPosition = whereSecondary;
				objects[secondaryNameObject].end = m[whereSecondary].endObjectPosition = whereSecondary + tagSet[secondaryTag].len;
				objects[secondaryNameObject].objectClass = objects[primaryNameObject].objectClass;
				objects[secondaryNameObject].locations.push_back(wherePrimary);
				objects[secondaryNameObject].aliases.push_back(primaryNameObject);
				objects[secondaryNameObject].updateFirstLocation(wherePrimary);
				wstring tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:%s gains match of %s.", wherePrimary,
						objectString(objects.begin() + primaryNameObject, tmpstr, false).c_str(),
						objectString(objects.begin() + secondaryNameObject, tmpstr2, false).c_str());
			}
		}
		if (((primaryClass == GENDERED_GENERAL_OBJECT_CLASS ||
			primaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			primaryClass == GENDERED_DEMONYM_OBJECT_CLASS ||
			primaryClass == GENDERED_RELATIVE_OBJECT_CLASS ||
			primaryClass == META_GROUP_OBJECT_CLASS ||
			primaryClass == BODY_OBJECT_CLASS) &&
			secondaryClass == NAME_OBJECT_CLASS) ||
			(primaryClass == BODY_OBJECT_CLASS &&
				(secondaryClass == GENDERED_GENERAL_OBJECT_CLASS ||
					secondaryClass == BODY_OBJECT_CLASS ||
					secondaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
					secondaryClass == GENDERED_DEMONYM_OBJECT_CLASS ||
					secondaryClass == GENDERED_RELATIVE_OBJECT_CLASS)))
		{
			if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
			{
				wstring tmpstr2;
				lplog(LOG_RESOLUTION, L"%06d:Metaname replacement detected for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
			}
			objects[secondaryNameObject].isNotAPlace = objects[primaryNameObject].isNotAPlace = true;
			if (objects[secondaryNameObject].getSubType() >= 0)
			{
				objects[secondaryNameObject].resetSubType();
				objects[secondaryNameObject].isNotAPlace = true;
				if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
					lplog(LOG_RESOLUTION, L"%06d:Removing place designation (2) from object %s.", where, objectString(secondaryNameObject, tmpstr, false).c_str());
			}
			if (objects[primaryNameObject].getSubType() >= 0)
			{
				if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[secondaryNameObject].getSubType() >= 0)
					lplog(LOG_RESOLUTION, L"%06d:Removing place designation (3) from object %s.", where, objectString(primaryNameObject, tmpstr, false).c_str());
				objects[primaryNameObject].resetSubType();
				objects[primaryNameObject].isNotAPlace = true;
			}
			if (objects[primaryNameObject].male ^ objects[primaryNameObject].female)
			{
				bool ambiguousGender = objects[secondaryNameObject].male && objects[secondaryNameObject].female;
				objects[secondaryNameObject].male = objects[primaryNameObject].male;
				objects[secondaryNameObject].female = objects[primaryNameObject].female;
				if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
				{
					wstring tmpstr2;
					lplog(LOG_RESOLUTION, L"%06d:Match %s becomes %s from object %s (1).", where,
						objectString(objects.begin() + secondaryNameObject, tmpstr, false).c_str(), (objects[secondaryNameObject].male) ? L"male" : L"female",
						objectString(objects.begin() + primaryNameObject, tmpstr2, false).c_str());
				}
				if (ambiguousGender)
					addDefaultGenderedAssociatedNouns(secondaryNameObject);
				if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[secondaryNameObject].getSubType() >= 0)
				{
					lplog(LOG_RESOLUTION, L"%06d:Removing place designation (4) from object %s.", where, objectString(secondaryNameObject, tmpstr, false).c_str());
				}
				objects[secondaryNameObject].resetSubType(); // not a place
				objects[secondaryNameObject].isNotAPlace = true;
			}
			if (primaryClass != BODY_OBJECT_CLASS)
				replaceObjectInSection(where, secondaryNameObject, primaryNameObject, L"metaname");
			else
			{
				vector <cLocalFocus>::iterator lsi;
				if (pushObjectIntoLocalFocus(whereSecondary, secondaryNameObject, false, false, inPrimaryQuote, inSecondaryQuote, L"metaname", lsi))
					pushLocalObjectOntoMatches(wherePrimary, lsi, L"metaname");
			}
			if (switched)
			{
				int tmp = secondaryNameObject, tmp2 = secondaryClass, tmp3 = whereSecondary;
				secondaryNameObject = primaryNameObject;
				secondaryClass = primaryClass;
				whereSecondary = wherePrimary;
				primaryNameObject = tmp;
				primaryClass = tmp2;
				wherePrimary = tmp3;
			}
			continue;
		}
		for (vector <cWordMatch>::iterator im = m.begin() + m[wherePrimary].beginObjectPosition, imEnd = m.begin() + m[wherePrimary].endObjectPosition; im != imEnd; im++)
		{
			im->objectRole |= META_NAME_EQUIVALENCE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE, L"%06d:Removed HAIL role (evaluateMetaNameEquivalence).", where);
			// prevents HAIL re-evaluation on mistaken HAIL 
			im->objectRole &= ~HAIL_ROLE; // [tommy:tuppence] Are you[tuppence] proposing a third advertisement : Wanted , female crook[tuppence] , answering to the name[name] of Rita ? ”
		}
		//if (primaryNameObject>=0 && primaryClass==NAME_OBJECT_CLASS && 
		//		(secondaryNameObject<0 || secondaryClass!=BODY_OBJECT_CLASS))
		// scan all future speakerGroups and see whether they have both the 'from' and the 'to'
		for (unsigned int sg = currentSpeakerGroup; sg < speakerGroups.size(); sg++)
			if (speakerGroups[sg].speakers.find(primaryNameObject) != speakerGroups[sg].speakers.end() && speakerGroups[sg].speakers.find(secondaryNameObject) != speakerGroups[sg].speakers.end())
			{
				speakerGroups[sg].speakers.erase(secondaryNameObject);
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr2;
					lplog(LOG_RESOLUTION, L"%06d:Alias %s erased from %s.", where, objectString(secondaryNameObject, tmpstr, true).c_str(), toText(speakerGroups[sg], tmpstr2));
				}
			}
		equivocateObjects(where, primaryNameObject, secondaryNameObject);
		//if (secondaryNameObject>=0 && secondaryClass==NAME_OBJECT_CLASS && 
		//		(primaryNameObject<0 || primaryClass!=BODY_OBJECT_CLASS))
		equivocateObjects(where, secondaryNameObject, primaryNameObject);
		// don't move adjectives to a gendered object with only two words - 'a woman'
		// otherwise 'a woman' gets to be a more significant object than the other one, which would be more descriptive 'the nurse'
		// 22284:Metaname equivalence detected for 
		//   secondary 22290:[an Irish Sinn feiner[22288-22291][22290][gendem][M][OBJECT][RE][NONPRESENT][FOCUS_EVALUATED]] to 
		//   primary 22287:[The man[21946-21948][21947][gender][M][SUBJECT][IS][NONPRESENT][FOCUS_EVALUATED][who came up the staircase with a furtive , soft - footed tread[21948-21961]]]
		if (objects[primaryNameObject].objectClass != GENDERED_GENERAL_OBJECT_CLASS || objects[primaryNameObject].end - objects[primaryNameObject].begin > 2 ||
			objects[primaryNameObject].relativeClausePM >= 0)
			moveNyms(where, primaryNameObject, secondaryNameObject, L"evaluateMetaNameEquivalence primary->secondary");
		if (objects[secondaryNameObject].objectClass != GENDERED_GENERAL_OBJECT_CLASS || objects[secondaryNameObject].end - objects[secondaryNameObject].begin > 2 ||
			objects[secondaryNameObject].relativeClausePM >= 0)
			moveNyms(where, secondaryNameObject, primaryNameObject, L"evaluateMetaNameEquivalence secondary->primary");
		if (objects[primaryNameObject].relativeClausePM < 0 && objects[secondaryNameObject].relativeClausePM >= 0)
		{
			objects[primaryNameObject].relativeClausePM = objects[secondaryNameObject].relativeClausePM;
			objects[primaryNameObject].whereRelativeClause = objects[secondaryNameObject].whereRelativeClause;
			objects[secondaryNameObject].relativeClausePM = objects[secondaryNameObject].whereRelativeClause = -1; // to prevent this object from becoming more visible to focus
		}
		if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
		{
			wstring tmpstr2;
			lplog(LOG_RESOLUTION, L"%06d:Metaname equivalence detected for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
		}
		if (switched)
		{
			int tmp = secondaryNameObject;
			secondaryNameObject = primaryNameObject;
			primaryNameObject = tmp;
		}
	}
	return atLeastOneSecondarySucceeded;
}

// in secondary quotes, inPrimaryQuote=false
bool cSource::identifyMetaNameEquivalence(int where, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
		int element, startAt = 0; // nameEnd=-1,
	while ((element = m[where].pma.queryAllPattern(L"_META_NAME_EQUIVALENCE", startAt)) != -1)
	{
		vector < vector <cTagLocation> > tagSets;
		// obeyBlock must be false because of _META_NAME_EQUIVALENCE[8]
		if (startCollectTags(true, metaNameEquivalenceTagSet, where, m[where].pma[element].pemaByPatternEnd, tagSets, false, true, L"name equivalence") > 0)
			for (unsigned int J = 0; J < tagSets.size(); J++)
			{
				if (debugTrace.traceNameResolution)
					printTagSet(LOG_RESOLUTION, L"MNE", J, tagSets[J], where, m[where].pma[element].pemaByPatternEnd);
				if (evaluateMetaNameEquivalence(where, tagSets[J], inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb))
					return true;
			}
		startAt = element + 1;
	}
	return false;
}

bool cSource::evaluateMetaSpeaker(int where, vector <cTagLocation>& tagSet)
{
	LFS
		int primaryTag, secondaryTag = findOneTag(tagSet, L"NAME_SECONDARY", -1);
	unsigned int wherePrimary = tagSet[primaryTag = findOneTag(tagSet, L"NAME_PRIMARY", -1)].sourcePosition;
	int whereSecondary = (secondaryTag >= 0) ? tagSet[secondaryTag].sourcePosition : -1;
	bool isAudience = (whereSecondary >= 0 && (m[whereSecondary].word->second.inflectionFlags & SECOND_PERSON) == SECOND_PERSON);
	if (tagSet[primaryTag].len > 1 && m[wherePrimary].principalWherePosition >= 0) // make sure to bypass any adjectives
		wherePrimary = m[wherePrimary].principalWherePosition;
	m[wherePrimary].objectRole |= (isAudience) ? IN_QUOTE_REFERRING_AUDIENCE_ROLE : IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE;
	if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
	{
		wstring tmpstr2;
		lplog(LOG_RESOLUTION, L"%06d:Meta %s detected for %d:[%s]", where, (isAudience) ? L"audience" : L"speaker", wherePrimary, objectString(m[wherePrimary].getObject(), tmpstr2, false).c_str());
	}
	return true;
}

bool cSource::identifyMetaSpeaker(int where, bool inQuote)
{
	LFS
		if (!inQuote) return false; // these patterns only apply in a quote
	int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(L"_META_SPEAKER", nameEnd)) == -1) return false;
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(true, metaNameEquivalenceTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, L"meta speaker identification") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, L"MS", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateMetaSpeaker(where, tagSets[J]))
				return true;
		}
	return false;
}

bool cSource::evaluateAnnounce(int where, vector <cTagLocation>& tagSet)
{
	LFS
		int primaryTag;
	unsigned int wherePrimary = tagSet[primaryTag = findOneTag(tagSet, L"NAME_PRIMARY", -1)].sourcePosition;
	if (tagSet[primaryTag].len > 1 && m[wherePrimary].principalWherePosition >= 0) // make sure to bypass any adjectives
		wherePrimary = m[wherePrimary].principalWherePosition;
	int oc = (m[wherePrimary].getObject() >= 0) ? objects[m[wherePrimary].getObject()].objectClass : -1;
	// must be a name or gendered object
	if (oc == NAME_OBJECT_CLASS || oc == GENDERED_GENERAL_OBJECT_CLASS || oc == BODY_OBJECT_CLASS || oc == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
		oc == GENDERED_DEMONYM_OBJECT_CLASS || oc == META_GROUP_OBJECT_CLASS || oc == GENDERED_RELATIVE_OBJECT_CLASS)
	{
		m[wherePrimary].objectRole |= PP_OBJECT_ROLE;
		if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
		{
			wstring tmpstr2;
			lplog(LOG_RESOLUTION, L"%06d:Meta announce detected for %d:[%s]", where, wherePrimary, objectString(m[wherePrimary].getObject(), tmpstr2, false).c_str());
		}
	}
	return true;
}

// Here is Bob!  / Here is another knock.
bool cSource::identifyAnnounce(int where, bool inQuote)
{
	LFS
		if (!inQuote) return false; // these patterns only apply in a quote
	int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(L"_META_ANNOUNCE", nameEnd)) == -1) return false;
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(true, metaSpeakerTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, L"meta announce") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, L"MA", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateAnnounce(where, tagSets[J]))
				return true;
		}
	return false;
}

bool cSource::evaluateMetaGroup(int where, vector <cTagLocation>& tagSet, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
		int primaryTag, secondaryTag;
	int wherePrimary = tagSet[primaryTag = findOneTag(tagSet, L"NAME_PRIMARY", -1)].sourcePosition;
	int whereSecondary = tagSet[secondaryTag = findOneTag(tagSet, L"NAME_SECONDARY", -1)].sourcePosition;
	if (tagSet[primaryTag].len > 1 && m[wherePrimary].principalWherePosition >= 0) // make sure to bypass any adjectives
		wherePrimary = m[wherePrimary].principalWherePosition;
	if (tagSet[secondaryTag].len > 1 && m[whereSecondary].principalWherePosition >= 0) // make sure to bypass any adjectives
		whereSecondary = m[whereSecondary].principalWherePosition;
	int op = m[wherePrimary].getObject(), os = m[whereSecondary].getObject();
	if (op >= 0 && os >= 0 && (objects[op].male || objects[op].female) && (objects[os].male || objects[os].female) &&
		objects[op].objectClass != BODY_OBJECT_CLASS && objects[os].objectClass != BODY_OBJECT_CLASS)
		//!objects[op].plural && !objects[os].plural)
	{
		resolveObject(wherePrimary, false, false, false, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, false, false, false);
		if (m[wherePrimary].objectMatches.size() >= 1) op = m[wherePrimary].objectMatches[0].object;
		resolveObject(whereSecondary, false, false, false, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, false, false, false);
		if (m[whereSecondary].objectMatches.size() >= 1) os = m[whereSecondary].objectMatches[0].object;
		vector <cLocalFocus>::iterator plsi = in(op), slsi = in(os);
		if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
		{
			wstring tmpstr2, tmpstr3;
			lplog(LOG_RESOLUTION, L"%06d:Meta group detected for %d:%s PP[%s] and %d:%s PP[%s]", where,
				wherePrimary, whereString(wherePrimary, tmpstr2, false).c_str(), (plsi != localObjects.end() && plsi->physicallyPresent) ? L"true" : L"false",
				whereSecondary, whereString(whereSecondary, tmpstr3, false).c_str(), (slsi != localObjects.end() && slsi->physicallyPresent) ? L"true" : L"false");
		}
		if (plsi != localObjects.end() && slsi != localObjects.end())
		{
			wstring tmpstr2, tmpstr3;
			if (plsi->physicallyPresent && !slsi->physicallyPresent)
			{
				lplog(LOG_RESOLUTION, L"%06d: %d:[%s] made %d:%s physically present", where, wherePrimary, whereString(wherePrimary, tmpstr2, false).c_str(), whereSecondary, whereString(whereSecondary, tmpstr3, false).c_str());
				for (int I = 0; I < (signed)m[whereSecondary].objectMatches.size(); I++)
				{
					vector <cLocalFocus>::iterator lsi = in(m[whereSecondary].objectMatches[I].object);
					if (lsi != localObjects.end())
						lsi->physicallyPresent = true;
				}
				slsi->physicallyPresent = true;
			}
			else if (slsi->physicallyPresent && !plsi->physicallyPresent)
			{
				lplog(LOG_RESOLUTION, L"%06d: %d:[%s] made %d:%s physically present", where, whereSecondary, whereString(whereSecondary, tmpstr3, false).c_str(), wherePrimary, whereString(wherePrimary, tmpstr2, false).c_str());
				for (int I = 0; I < (signed)m[wherePrimary].objectMatches.size(); I++)
				{
					vector <cLocalFocus>::iterator lsi = in(m[wherePrimary].objectMatches[I].object);
					if (lsi != localObjects.end())
						lsi->physicallyPresent = true;
				}
				plsi->physicallyPresent = true;
			}
		}
		return true;
	}
	return false;
}

// Here is Bob!  / Here is another knock.
bool cSource::identifyMetaGroup(int where, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
		if (inPrimaryQuote || inSecondaryQuote) return false; // these patterns only apply to speakers
	int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(L"_META_GROUP", nameEnd)) == -1) return false;
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(true, metaNameEquivalenceTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, L"meta group identification") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, L"MG", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateMetaGroup(where, tagSets[J], lastBeginS1, lastRelativePhrase, lastQ2, lastVerb))
				return true;
		}
	return false;
}

// this is so that Doctor Hall matches Dr. Hall, and matches doc.  
// If doc is introduced first (or doctor, or reverend) and then Reverend Holland is introduced, they should be related
// also Hasbro Co. should match Hasbro Company
struct {
	const wchar_t* abbreviation;
	const wchar_t* full;
} abbreviationWordMapList[] =
// honorifics
{
	{ L"dr",L"doc" },
	{ L"doc",L"doctor" },
	{ L"doctor",L"dr" },
	{ L"st",L"saint" },
	{ L"mr",L"mister" },
	{ L"m",L"mister" },
	{ L"mrs",L"missus" },
	{ L"rev",L"reverend" },
	{ L"ms",L"miz" },
	{ L"ms",L"miss" },
	{ L"prof",L"professor" },
	//measurement_abbreviation
		{L"lbs",L"pounds"},
		{L"mo",L"month"},
		{L"mos",L"months"},
		{L"cm",L"centimeter"},
		{L"kg",L"kilogram"},
		{L"km",L"kilometer"},
		{L"kw",L"kilowatt"},
		{L"lb",L"pound"},
		{L"ft",L"foot"},
		{L"oz",L"ounce"},
		{L"in",L"inch"},
		{L"mg",L"milligram"},
		{L"ml",L"milliliter"},
		{L"mm",L"millimeter"},
		{L"tbsp",L"tablespoon"},
		{L"tsp",L"teaspoon"},
		// street_address_abbreviation
			{L"st",L"street"},
			{L"av",L"avenue"},
			{L"ave",L"avenue"},
			{L"dr",L"drive"},
			{L"rd",L"road"},
			{L"pk",L"pike"}, // streets (NewsBank)
		// business_abbreviation
			{L"inc",L"incorporated"},
			{L"ltd",L"limited"},
			{L"corp",L"corporation"},
			{L"co",L"company"},
			{NULL,0} };
struct wordMapCompare
{
	bool operator()(const tIWMM& lhs, const tIWMM& rhs) const
	{
		return lhs->first < rhs->first;
	}
};

map <tIWMM, vector <tIWMM>, cSource::wordMapCompare> abbreviationMap;
void cSource::buildMap(void)
{
	LFS
		if (abbreviationMap.empty())
		{
			for (unsigned int I = 0; abbreviationWordMapList[I].abbreviation; I++)
			{
				tIWMM abb = Words.query(abbreviationWordMapList[I].abbreviation), full = Words.query(abbreviationWordMapList[I].full);
				if (abb != Words.end() && full != Words.end())
				{
					abbreviationMap[abb].push_back(full);
					abbreviationMap[full].push_back(abb);
				}
			}
		}
}

void cSource::addWordAbbreviationMap(tIWMM w, vector <tIWMM>& nouns)
{
	LFS
		if (w == wNULL) return;
	map <tIWMM, vector <tIWMM>, cSource::wordMapCompare>::iterator iMap = abbreviationMap.find(w);
	if (iMap == abbreviationMap.end()) return;
	if (find(nouns.begin(), nouns.end(), w) == nouns.end()) nouns.push_back(w);
	for (vector <tIWMM>::iterator ami = iMap->second.begin(), amiEnd = iMap->second.end(); ami != amiEnd; ami++)
		if (find(nouns.begin(), nouns.end(), *ami) == nouns.end()) nouns.push_back(*ami);
}

// detect any mapping not already there due to title-abbreviations
void cSource::addAssociatedNounsFromTitle(int o)
{
	LFS
		if (objects[o].objectClass == NAME_OBJECT_CLASS)
		{
			buildMap();
			addWordAbbreviationMap(objects[o].name.hon, objects[o].associatedNouns);
			addWordAbbreviationMap(objects[o].name.hon2, objects[o].associatedNouns);
			addWordAbbreviationMap(objects[o].name.hon3, objects[o].associatedNouns);
		}
		else if (objects[o].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
		{
			buildMap();
			// do only for the principalWhere, whether for the little 'doctor' or for Mrs. Vandermeyer's cook (where nouns should NOT be associated from Mrs. Vandermeyer)
			addWordAbbreviationMap(m[objects[o].originalLocation].word, objects[o].associatedNouns);
		}
}

bool cSource::abbreviationEquivalent(tIWMM w, tIWMM w2)
{
	LFS
		// even if generic, we want to make sure a matching head will get rewarded (the big man)
		if (w == w2) return true;
	if (w == wNULL) return false;
	map <tIWMM, vector <tIWMM>, wordMapCompare>::iterator iMap = abbreviationMap.find(w);
	if (iMap == abbreviationMap.end()) return false;
	return find(iMap->second.begin(), iMap->second.end(), w2) != iMap->second.end();
}

bool cSource::accumulateRelatedObjects(int object, set <int>& relatedObjects)
{
	LFS
		int begin = objects[object].begin, end = objects[object].end;
	buildMap();
	for (int I = begin; I < end; I++)
	{
		tIWMM w = m[I].word;
		wstring tmpstr, tmpstr2;
		relatedObjects.insert(relatedObjectsMap[w].begin(), relatedObjectsMap[w].end());
		map <tIWMM, vector<tIWMM>, cSource::wordMapCompare>::iterator ami = abbreviationMap.find(w);
		if (ami != abbreviationMap.end())
			for (vector <tIWMM>::iterator wi = ami->second.begin(), wiEnd = ami->second.end(); wi != wiEnd; wi++)
				relatedObjects.insert(relatedObjectsMap[*wi].begin(), relatedObjectsMap[*wi].end());
	}
	return relatedObjects.size() > 0;
}

// detect a Janet Vandermeyer and a Rita Vandermeyer
void cSource::accumulateNameLikeStats(vector <cObject>::iterator& object, int o, bool& firstNameAmbiguous, bool& lastNameAmbiguous, tIWMM& ambiguousFirst, int& ambiguousNickName, tIWMM& ambiguousLast)
{
	LFS
		bool ga = firstNameAmbiguous || lastNameAmbiguous;
	// if the object contains both a first and last name
	if (object->name.first != wNULL && object->name.last != wNULL && objects[o].name.first != wNULL && objects[o].name.last != wNULL)
	{
		if (object->name.last == objects[o].name.last)
			firstNameAmbiguous |= (!object->name.match(object->name.first, objects[o].name.first, false) && object->name.nickName != objects[o].name.nickName);
		lastNameAmbiguous |= (object->name.match(object->name.first, objects[o].name.first, false) && object->name.last != objects[o].name.last);
	}
	if (object->name.any != wNULL && object->name.first == wNULL && object->name.last == wNULL && objects[o].name.first != wNULL && objects[o].name.last != wNULL)
	{
		if (object->name.any == objects[o].name.first)
		{
			if (ambiguousLast == wNULL) ambiguousLast = objects[o].name.last;
			else lastNameAmbiguous |= (ambiguousLast != objects[o].name.last);
		}
		if (object->name.any == objects[o].name.last)
		{
			if (ambiguousFirst == wNULL)
			{
				ambiguousFirst = objects[o].name.first;
				ambiguousNickName = objects[o].name.nickName;
			}
			else firstNameAmbiguous |= (object->name.match(ambiguousFirst, objects[o].name.first, false) && (ambiguousNickName != objects[o].name.nickName));
		}
	}
	if (!ga && (firstNameAmbiguous || lastNameAmbiguous))
	{
		wstring tmpstr, tmpstr2;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%s ambiguous with %s [%s,%d,%s,%s,%s].", objectString(object, tmpstr, false).c_str(), objectString(o, tmpstr2, false).c_str(),
				(ambiguousFirst == wNULL) ? L"" : ambiguousFirst->first.c_str(), ambiguousNickName, (ambiguousLast == wNULL) ? L"" : ambiguousLast->first.c_str(),
				(firstNameAmbiguous) ? L"firstNameAmbiguous" : L"", (lastNameAmbiguous) ? L"lastNameAmbiguous" : L"");

	}
}

// merge current object onto matched object.  change all references (m and ls)
//                       of the current object to the new object.
//    if a name is not mentioned definitively as a speaker, try to match it to a speaker.
bool cSource::resolveNameObject(int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches, int forwardCallingObject)
{
	LFS
		// don't match 'sir' to anything using this 'relatedObjects' kind of resolution
		if (object->name.justHonorific() && object->name.hon->second.query(L"pinr") >= 0) return false;
	// Supposing Mr . Brown -- Julius -- was there waiting
	// detect a name with an embedded --
	int embeddedDash = -1;
	for (int I = where; I < m[where].endObjectPosition - 1; I++)
		if (m[I].word->first == L"--")
		{
			embeddedDash = I;
			break;
		}
	// if embeddedDash is beyond the last name, then it is not embedded, but symptomatic of a parsing error
	if (embeddedDash != -1 && object->name.last != wNULL)
	{
		for (int I = where; I < m[where].endObjectPosition - 1; I++)
			if (m[I].word == object->name.hon && embeddedDash < I)
				embeddedDash = -1;
		for (int I = where; I < m[where].endObjectPosition - 1; I++)
			if (m[I].word->first == object->name.last->first && embeddedDash > I)
				embeddedDash = -1;
	}
	if (embeddedDash != -1)
	{
		wstring tmpstr;
		if (debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:Could not resolve %d-%d:%s - embedded dash@%d, possible combination of two different names [last=%s] - marking as eliminated", where,
				m[where].beginObjectPosition, m[where].endObjectPosition, objectString(object, tmpstr, false).c_str(), embeddedDash, (object->name.last != wNULL) ? object->name.last->first.c_str() : L"NULL");
		object->eliminated = true;
		for (unsigned int I = 0; I < speakerGroups.size(); I++)
			speakerGroups[I].speakers.erase(m[where].getObject());
		return false;
	}
	set <int> matchingObjects;
	bool lastNameAmbiguous = false, firstNameAmbiguous = false;
	tIWMM ambiguousFirst = wNULL, ambiguousLast = wNULL;
	int ambiguousNickName = -1;
	int objectToBeReplaced = m[where].getObject();
	vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsiEnd = localObjects.end();
	wstring tmpstr, tmpstr2;
	bool unambiguousGenderFound;
	for (; lsi != lsiEnd; lsi++)
	{
		if (lsi->om.object <= 1) continue;
		lsi->res.clear();
		lsi->numMatchedAdjectives = 0;
		if (lsi->om.object != objectToBeReplaced)
		{
			accumulateNameLikeStats(object, lsi->om.object, firstNameAmbiguous, lastNameAmbiguous, ambiguousFirst, ambiguousNickName, ambiguousLast);
			if (object->nameMatch(objects[lsi->om.object], debugTrace))
				matchingObjects.insert(lsi->om.object);
			else
				if (objects[lsi->om.object].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
					abbreviationEquivalent(m[objects[lsi->om.object].originalLocation].word, object->name.hon) &&
					objects[lsi->om.object].matchGender(*object, unambiguousGenderFound) &&
					objects[lsi->om.object].name.like(object->name, debugTrace) && objects[lsi->om.object].plural == object->plural)
					matchingObjects.insert(lsi->om.object);
				else
					lsi->om.salienceFactor = -1;
		}
		else
			lsi->om.salienceFactor = -1;
	}
	if (matchingObjects.empty() && usePIS && (m[where].objectRole & HAIL_ROLE) && currentSpeakerGroup < speakerGroups.size())
	{
		set <int>::iterator pISO = speakerGroups[currentSpeakerGroup].speakers.begin(), pISOEnd = speakerGroups[currentSpeakerGroup].speakers.end();
		for (; pISO != pISOEnd; pISO++)
			if (*pISO > 1 && *pISO != objectToBeReplaced && object->nameMatch(objects[*pISO], debugTrace))
			{
				accumulateNameLikeStats(object, *pISO, firstNameAmbiguous, lastNameAmbiguous, ambiguousFirst, ambiguousNickName, ambiguousLast);
				matchingObjects.insert(*pISO);
			}
	}
	bool globalSearch;
	if (globalSearch = matchingObjects.empty())
	{
		// do not allow resolution against objects that have not been encountered yet!
		set <int> relatedObjects;
		accumulateRelatedObjects(objectToBeReplaced, relatedObjects);
		set<int>::iterator begin = relatedObjects.begin(), end = relatedObjects.end();
		for (set<int>::iterator s = begin; s != end && !object->eliminated; s++)
		{
			int ro = *s;
			if (ro <= 1) continue;
			if (objects[ro].eliminated)
				followObjectChain(ro);
			accumulateNameLikeStats(object, *s, firstNameAmbiguous, lastNameAmbiguous, ambiguousFirst, ambiguousNickName, ambiguousLast);
			vector <cObject>::iterator rObject = objects.begin() + ro;
			if (ro != objectToBeReplaced && rObject->firstLocation < where && !rObject->eliminated &&
				((((rObject->objectClass == NAME_OBJECT_CLASS || rObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS) && !rObject->name.justHonorific())) ||
					(rObject->objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && in(ro) != localObjects.end())))
			{
				// the only way a previous position has not been resolved is if
				// this is in the identifySpeakerGroups phase and the previous position
				// is inQuotes.  We should be very cautious about replacing objects in the identifySpeakerGroups phase because it is not reversed.
				if (rObject->objectClass == NAME_OBJECT_CLASS || rObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
				{
					if (!(m[rObject->originalLocation].flags & cWordMatch::flagObjectResolved) && forwardCallingObject < 0)
					{
						int lastSpeakerGroupEnd = (speakerGroups.size() > 0) ? speakerGroups[speakerGroups.size() - 1].sgEnd : 0;
						bool moreQualified = rObject->name.first != wNULL && rObject->name.last != wNULL;
						if (rObject->originalLocation >= lastSpeakerGroupEnd || moreQualified || !(firstNameAmbiguous || lastNameAmbiguous))
						{
							vector <cOM> relatedObjectMatches;
							resolveNameObject(rObject->originalLocation, rObject, relatedObjectMatches, where);
						}
						else
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION, L"%06d:Object %s (%d,%d) related to object %s?", where, objectString(rObject, tmpstr, false).c_str(), rObject->originalLocation, lastSpeakerGroupEnd, objectString(object, tmpstr2, false).c_str());
							continue;
						}
						// resolveObject(object->originalLocation,false,true,-1,false,false,false); must not call resolveObject because it will bring this object into local focus
					}
					if (rObject->eliminated) continue;
					// does 'War' match 'War Office'
					bool match = false;
					if (rObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS && object->objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
						match = false; // shouldn't get here
					else if (rObject->objectClass == NAME_OBJECT_CLASS && object->objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
						match = rObject->nameNonGenderedMatch(m, *object);
					else if (rObject->objectClass == NON_GENDERED_NAME_OBJECT_CLASS && object->objectClass == NAME_OBJECT_CLASS)
						match = object->nameNonGenderedMatch(m, *rObject);
					else if (rObject->objectClass == NAME_OBJECT_CLASS && object->objectClass == NAME_OBJECT_CLASS)
						match = object->nameMatch(*rObject, debugTrace);
					if (match)
						matchingObjects.insert(ro);
				}
				else if (rObject->objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
				{
					// any honorific of the name object must be related to the principal where position of rObject
					// so rObject==doc or the doctor, so w=doc or w=doctor
					tIWMM w = m[rObject->originalLocation].word;
					map <tIWMM, vector<tIWMM>, cSource::wordMapCompare>::iterator ami = abbreviationMap.find(w);
					// if Dr. Hall (hon=Dr.) matches doc
					if (object->name.hon == w || object->name.hon2 == w || object->name.hon3 == w ||
						(ami != abbreviationMap.end() &&
							// OR if Dr. Hall (hon=Dr. [doc]) matches doc (TRUE)
							(find(ami->second.begin(), ami->second.end(), object->name.hon) != ami->second.end() ||
								find(ami->second.begin(), ami->second.end(), object->name.hon2) != ami->second.end() ||
								find(ami->second.begin(), ami->second.end(), object->name.hon3) != ami->second.end())))
						matchingObjects.insert(ro);
				}
			}
		}
		if (object->eliminated)
		{
			if (debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Object %s eliminated during prior object resolution", where, objectString(object, tmpstr, false).c_str());
			return true;
		}
		if (matchingObjects.empty())
		{
			if (debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Could not resolve %s", where, objectString(object, tmpstr, false).c_str());
			return false;
		}
	}
	// sort by nearness of last mention?
	bool qualified = object->name.first != wNULL && object->name.last != wNULL;
	bool allMatchesMatch = true;
	int maxOccurrence = -1, minOccurrence = 10000000;
	if (globalSearch)
	{
		for (set<int>::iterator mo = matchingObjects.begin(), moEnd = matchingObjects.end(); mo != moEnd && allMatchesMatch; )
		{
			maxOccurrence = max(maxOccurrence, objects[*mo].numEncounters + objects[*mo].numIdentifiedAsSpeaker);
			minOccurrence = min(minOccurrence, objects[*mo].numEncounters + objects[*mo].numIdentifiedAsSpeaker);
			int ro = *mo;
			if (objects[ro].eliminated)
			{
				followObjectChain(ro);
				matchingObjects.erase(mo++);
				if (!objects[ro].eliminated)
				{
					matchingObjects.insert(ro);
					mo = matchingObjects.begin();
				}
			}
			else
				mo++;
		}
		/*
		if (minOccurrence==0) minOccurrence=1;
		for (set<int>::iterator mo=matchingObjects.begin(),moEnd=matchingObjects.end(); mo!=moEnd && allMatchesMatch; mo++)
			for (set<int>::iterator mo2=mo; mo2!=moEnd && allMatchesMatch; mo2++)
				allMatchesMatch=(objects[*mo].nameMatch(objects[*mo2]));
		// if not everything matches, get rid of names that are really quite rare, unless
		if (!allMatchesMatch && matchingObjects.size()>1 && minOccurrence*5<maxOccurrence)
		{
			for (set<int>::iterator mo=matchingObjects.begin(),moEnd=matchingObjects.end(); mo!=moEnd; )
			{
				lplog(LOG_RESOLUTION,L"%06d:NGM matchingObject %s has occurrence %d",where,objectString(*mo,tmpstr,false).c_str(),
						objects[*mo].numEncounters+objects[*mo].numIdentifiedAsSpeaker);
				if (objects[*mo].numEncounters+objects[*mo].numIdentifiedAsSpeaker<=minOccurrence)
					matchingObjects.erase(mo++);
				else
					mo++;
			}
		}
		*/
	}
	for (set<int>::iterator mo = matchingObjects.begin(), moEnd = matchingObjects.end(); mo != moEnd; mo++)
	{
		if (objects[*mo].eliminated) continue;
		// 'doc' matches 'Dr. Hall'
		if (objects[*mo].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
		{
			vector <cObject>::iterator mObject = objects.begin() + *mo;
			int o = (int)(object - objects.begin());
			// remove object if object being replaced (*mo) is an unresolvable object coming after where.
			if (unResolvablePosition(mObject->begin) && (mObject->originalLocation > object->originalLocation ||
				(currentSpeakerGroup < speakerGroups.size() && (mObject->begin < speakerGroups[currentSpeakerGroup].sgBegin ||
					(currentEmbeddedSpeakerGroup >= 0 && mObject->begin < speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin)))))
			{
				if (debugTrace.traceNameResolution)
					lplog(LOG_RESOLUTION, L"%06d:unresolvable occupation %s matches but original location occurs after %s (%d>%d) or before current speaker group",
						where, objectString(*mo, tmpstr, true).c_str(), objectString(object, tmpstr2, true).c_str(),
						mObject->originalLocation, object->originalLocation);
				continue;
			}
			if (object->objectClass == NAME_OBJECT_CLASS && !object->name.justHonorific())
			{
				if ((lsi = in(*mo)) == localObjects.end())
				{
					if (debugTrace.traceNameResolution)
						lplog(LOG_RESOLUTION, L"%06d:matching object %s is not in local salience (resolveNameWithOccupationObject) - rejected.",
							where, objectString(*mo, tmpstr, true).c_str());
				}
				else
					replaceObjectWithObject(atBefore(*mo, where), mObject, o, L"resolveNameWithOccupationObject");
			}
			else
			{
				if (debugTrace.traceNameResolution)
					lplog(LOG_RESOLUTION, L"%06d:owned occupation %s matches but does not replace occupation %s", where, objectString(*mo, tmpstr, true).c_str(), objectString(object, tmpstr2, true).c_str());
				objectMatches.push_back(cOM(*mo, SALIENCE_THRESHOLD));
			}
			continue;
		}
		// even though every object in matchingObjects must match the original object in name and sex and plurality
		// why is matchSex called?
		// after replacing one object with another the objects in matchingObjects are also matched with each other.
		// in this case because sex may be matched although original object is MALE & FEMALE and the other is only MALE or only FEMALE
		// if the second object replaces the first, the next object (if any) may no longer match because it may be the opposite sex to the second object
		// and so it matched the original object because it was ambiguous but now because of replacement (and the original object
		// now becomes the second object and thus only one sex) is no longer ambiguous.
		// also if sister matches both sister greenbank and sister matilda.  Both shouldn't match to each other.
		if (object->nameMatch(objects[*mo], debugTrace) &&
			// don't replace 'sir' or 'mister' with anything throughout a section.
			(!objects[*mo].name.justHonorific() || objects[*mo].name.hon->second.query(L"pinr") < 0))
		{
			bool matchingQualified = objects[*mo].name.first != wNULL && objects[*mo].name.last != wNULL;
			bool globallyAmbiguous = false;
			if (globalSearch && (firstNameAmbiguous || lastNameAmbiguous) && !(qualified && matchingQualified))
			{
				// now determine whether ambiguousness applies to this particular name
				// if object is full, and *mo is firstName, and lastNameAmbiguous then ambiguous, etc
				bool firstNameOnly = false, lastNameOnly = false;
				firstNameOnly = (qualified && !matchingQualified) && objects[*mo].name.first != wNULL && lastNameAmbiguous;
				firstNameOnly |= (!qualified && matchingQualified) && object->name.first != wNULL && lastNameAmbiguous;
				lastNameOnly = (qualified && !matchingQualified) && objects[*mo].name.last != wNULL && firstNameAmbiguous;
				lastNameOnly |= (!qualified && matchingQualified) && object->name.last != wNULL && firstNameAmbiguous;
				if (globallyAmbiguous = firstNameOnly || lastNameOnly)
					lplog(LOG_RESOLUTION, L"%06d:matching %s: globally ambiguous name %s [%s,%s]",
						where, objectString(object, tmpstr, true).c_str(), objectString(*mo, tmpstr2, true).c_str(),
						(firstNameOnly) ? L"firstNameOnly" : L"", (lastNameOnly) ? L"lastNameOnly" : L"");
			}
			// object or matching object is composed of only one component which is ambiguous
			// prefer unowned objects
			// 'Porsche' should be matched with 'her Porsche' - but not replaced.
			// if not in local and number of parts don't match 
			const wchar_t* reason = NULL;
			if ((objects[*mo].getOwnerWhere() != -1 && object->getOwnerWhere() == -1)) reason = L"preferUnOwnedObjects";
			if (object->name.justHonorific()) reason = L"justHonorific";
			if (!object->matchGenderIncludingNeuter(objects[*mo], unambiguousGenderFound)) reason = L"genderConflict";
			if (globallyAmbiguous) reason = L"globallyAmbiguous";
			if (reason != NULL)
			{
				if (debugTrace.traceNameResolution)
					lplog(LOG_RESOLUTION, L"%06d:owned name %s matches but does not replace name %s [%s]", where, objectString(*mo, tmpstr, true).c_str(), objectString(object, tmpstr2, true).c_str(), reason);
				objectMatches.push_back(cOM(*mo, SALIENCE_THRESHOLD));
			}
			else
			{
				replaceObjectWithObject(where, object, *mo, L"resolveNameObject");
				object = objects.begin() + *mo;
			}
		}
	}
	return true;
}

