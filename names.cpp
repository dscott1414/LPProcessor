/*
	names.cpp - proper-name patterns, honorific / nickname matching, and
		meta-name equivalence (“X, known as Y”)

	Overview:
		Registers the __NAME / _NAME / __NAMEOWNER / address / ISBN
		patterns and the _META_NAME_EQUIVALENCE / _META_SPEAKER /
		_META_GROUP family that link a primary mention to an alias,
		occupation, or group. cName stores up to three honorifics, first /
		middle / last / suffix / any plus a nickname-class id from the
		US-Census-derived nicknameEquivalenceMap (filled in
		initializeDictionary.cpp). like() / confidentMatch() / merge()
		are the coreference predicates used by resolveNameObject() and
		speaker resolution. Gender is taken from honorific and first-name
		inflection flags (MALE_GENDER / FEMALE_GENDER, including the
		capitalized-only variants).

	Pipeline position:
		Initialization: defineNames() / createMetaNameEquivalencePatterns()
		with the other pattern builders. Stage 6 (objects & pronouns):
		identifyName() during object creation; resolveNameObject() during
		coreference; evaluateMetaNameEquivalence() while walking the
		source. createLetterIntroPatterns() lives in resolveSpeakers.cpp.

	Key entry points:
		- defineNames() - name / address / owner patterns
		- identifyName() / evaluateName() - tag-set -> cName + sex
		- like() / confidentMatch() / merge() - name compatibility
		- resolveNameObject() - attach this mention to prior NAME objects
		- evaluateMetaNameEquivalence() - “called / known as / named”
		- mapNumeralCardinal() / mapNumeralOrdinal() - also used by
		  timeRelations.cpp for “the twentieth”

	Key data structures / globals:
		- nicknameEquivalenceMap - first-name -> nickname class id
		- abbreviationWordMapList / abbreviationMap - Dr/doc/doctor,
		  Co/Company, St/Street (ambiguous “st” = saint and street)
		- relatedObjectsMap - word -> object indexes sharing that word

	Dependencies:
		Words lexicon (honorific / proper-noun / census gender flags),
		cPattern, MySQL via insertSQL() (name-part rows).

	Notes / gotchas:
		- LFS at every function entry.
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
#include "profile.h"

// Register __NAME / _NAME / __NAMEOWNER / address / ISBN / PO-box patterns
// (Dr. Helen Mirren, D'Artagnan, “the name ‘Rita’”, “23 Beekam St.”).
void defineNames(void)
{
	LFS
		//cPattern *p=NULL;
		cPattern::create(u"__NAMEINTRO{PLURAL}", u"1", 2, u"honorific{HON}", u"_HON_ABB{HON}", NO_OWNER, 1, 1,
			1, u"and", 0, 1, 1,
			2, u"honorific{HON}", u"_HON_ABB{HON}", NO_OWNER, 1, 1,
			2, u"honorific{HON2}", u"_HON_ABB{HON2}", NO_OWNER, 0, 1,
			2, u"honorific{HON3}", u"_HON_ABB{HON3}", NO_OWNER, 0, 1,
			0);
	cPattern::create(u"__NAMEINTRO{SINGULAR}", u"2",
		2, u"honorific*-1{HON}", u"_HON_ABB*-1{HON}", NO_OWNER, 1, 1, // encourages __NAMEINTRO and not _NAME"H"
		2, u"honorific{HON2}", u"_HON_ABB{HON2}", NO_OWNER, 0, 1,
		2, u"honorific{HON3}", u"_HON_ABB{HON3}", NO_OWNER, 0, 1,
		0);
	// Dr. Helen Billows Mirren // Cornelius de Witt / Helen Mirren
	// The Rev. Dr. Bartholomew
	cPattern::create(u"__NAME", u"1",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"Proper Noun{FIRST}", NO_OWNER, 1, 2,
		6, u"determiner|le{MIDDLE}", u"preposition|de{MIDDLE}", u"noun|van{MIDDLE}", u"noun|von{MIDDLE}", u"_ABB{MIDDLE}", u"letter{MIDDLE}", NO_OWNER, 0, 1,
		//1,u".",0,0,1, // included by _ABB
		1, u"Proper Noun{LAST}", NO_OWNER, 1, 2,
		0); // noun removed
// Dr. Cornelius
	cPattern::create(u"__NAME", u"2",
		1, u"__NAMEINTRO", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"letter{LAST}", NO_OWNER, 1, 1,
		0);
	// M. de Louvois / M. Louvois
	cPattern::create(u"__NAME", u"3",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 0, 1,
		4, u"determiner|le{MIDDLE}", u"preposition|de{MIDDLE}", u"noun|van{MIDDLE}", u"noun|von{MIDDLE}", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*2{LAST}", NO_OWNER, 1, 2, 0); // Names ending in a noun should be not common

// D' Artagnan / O'Malley / d'artagnan
	cPattern::create(u"__NAME", u"4",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		1, u"letter{LAST2}", 0, 1, 1,
		1, u"quotes", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*1{LAST}", NO_OWNER, 1, 1, 0); // Names ending in a noun should be not common
// M. A.
	cPattern::create(u"__NAME", u"5",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 1, 1,
		1, u"letter{LAST}", 0, 1, 1,
		1, u".", 0, 1, 1,
		0);

	// M. A. L.
	cPattern::create(u"__NAME", u"6",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 1, 1,
		1, u"letter{MIDDLE}", 0, 1, 1,
		1, u".", 0, 1, 1,
		1, u"letter{LAST}", 0, 1, 1,
		1, u".", 0, 1, 1,
		0);

	// Monsieur le Pen / Monsieur le compte / Eamon de Valera // Covento de San -Francisco
	cPattern::create(u"__NAME", u"9",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		4, u"determiner|le{MIDDLE}", u"preposition|de{MIDDLE}", u"noun|van{MIDDLE}", u"noun|von{MIDDLE}", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*1{LAST}", NO_OWNER, 1, 2, 0); // Names ending in a noun should be not common

// M. D' Artagnan / K. O'Malley / M. d'artagnan
	cPattern::create(u"__NAME", u"A",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 0, 1,
		1, u"Proper Noun{MIDDLE}", NO_OWNER, 0, 1,
		1, u"letter{LAST2}", 0, 1, 1,
		1, u"quotes", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*1{LAST}", NO_OWNER, 1, 1, 0);  // Names ending in a noun should be not common
	cPattern::create(u"__NAME", u"B", 1, u"Proper Noun{SINGULAR:ANY}", NO_OWNER, 1, 1, 0);
	// Alan A.
	cPattern::create(u"__NAME", u"C", 1, u"Proper Noun{SINGULAR:FIRST}", NO_OWNER, 1, 1,
		1, u"letter{LAST}", 0, 1, 1,
		1, u".", 0, 1, 1,
		0);
	// M. A. Wycliffe, M A Wycliff, A Wycliff
	cPattern::create(u"__NAME", u"D",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 0, 1,
		1, u"letter{MIDDLE}", 0, 0, 1,
		1, u".", 0, 0, 1,
		1, u"Proper Noun{LAST}", NO_OWNER, 1, 1,
		0);
	// B.A. Summa cum laude
	cPattern::create(u"__NAME", u"DEGREESCL",
		1, u"letter*-2", 0, 1, 1,
		1, u".", 0, 0, 1,
		1, u"letter*-2", 0, 1, 1,
		1, u".", 0, 0, 1,
		2, u"noun|summa*-1", u"noun|magna*-1", 0, 0, 1,
		1, u"adjective|cum", 0, 1, 1,
		1, u"adjective|laude", 0, 1, 1,
		0);

	// Mr. --
	cPattern::create(u"__NAME", u"E",
		2, u"honorific{SINGULAR:HON}", u"_HON_ABB{SINGULAR:HON}", 0, 1, 1, // if this is made optional, -- will always match __NAME, which is often not correct
		1, u"--", 0, 1, 1,
		0);

	// Bishop Manuel de Mollinedo y Angulo
	cPattern::create(u"__NAME", u"F",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 0, 1,
		1, u"Proper Noun{MIDDLE}", NO_OWNER, 0, 1,
		1, u"letter{LAST2}", 0, 1, 1,
		1, u"quotes", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*1{LAST}", NO_OWNER, 1, 1, 0);  // Names ending in a noun should be not common

	cPattern::create(u"_NAME{NAME}", u"1", 1, u"__NAME", 0, 1, 1, 0);

	// Sir Cornelius de Witt the fifth
	cPattern::create(u"_NAME{NAME}", u"7", // '7' keeps with the numbering of __NAME
		1, u"__NAME", 0, 1, 1,
		1, u"determiner", 0, 1, 1,
		1, u"numeral_ordinal{SUFFIX}", 0, 1, 1, 0);
	// Helen Mirren III.
	cPattern::create(u"_NAME{NAME}", u"8", // '8' keeps with the numbering of __NAME
		1, u"__NAME", 0, 1, 1,
		1, u"roman_numeral{SUFFIX}", 0, 1, 1,
		1, u".", 0, 0, 1, 0);

	cPattern::create(u"_NAME{NAME}", u"G",
		1, u"Proper Noun", NO_OWNER, 1, 3,
		2, u"business_abbreviation*-1{BUS}", u"_BUS_ABB*-1{BUS}", 0, 1, 1,
		0);
	cPattern::create(u"_NAME{NAME}", u"H",
		2, u"honorific{SINGULAR:HON}", u"_HON_ABB{SINGULAR:HON}", NO_OWNER, 1, 1,
		0);
	// The RMS Lusitania was a luxury ocean liner.
	cPattern::create(u"_NAME", u"K",
		1, u"abbreviation*1", ONLY_CAPITALIZED, 1, 1,
		1, u"Proper Noun{ANY}", NO_OWNER, 1, 1,
		0);
	// Number Fourteen, please close the door.
	cPattern::create(u"_NAME{NAME}", u"Q",
		1, u"Proper Noun|number", 0, 1, 1,
		3, u"Number*-3{ANY}", u"roman_numeral*-3{ANY}", u"numeral_cardinal*-3{ANY}", NO_OWNER, 1, 1,
		0);
	// B-17
	cPattern::create(u"_NAME{NAME:SINGULAR}", u"R",
		1, u"letter", 0, 1, 1,
		1, u"dash*-2", 0, 0, 1,
		1, u"Number", NO_OWNER, 1, 2,
		0);

	// the name " Rita "
	cPattern::create(u"_NAME{NAME}", u"M",
		1, u"determiner|the", 0, 1, 1,
		1, u"noun|name", 0, 1, 1,
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		1, u"_NAME*-3", 0, 1, 1, // quoted nouns should be rare in general
		2, u",", u".", 0, 0, 1,
		1, u"quotes", CLOSE_INFLECTION, 1, 1, 0);
	// ISBN 0-393-07101-4
	cPattern::create(u"_ISBN", u"",
		1, u"abbreviation|isbn", 0, 1, 1,
		1, u"number", 0, 1, 1,
		1, u"dash|-", 0, 0, 1,
		1, u"number", 0, 1, 1,
		1, u"dash|-", 0, 0, 1,
		1, u"number", 0, 1, 1,
		1, u"dash|-", 0, 0, 1,
		1, u"number", 0, 1, 1,
		0);

	// Mrs. Pinkerton, 
	cPattern::create(u"_LINE1ADDRESS", u"",
		1, u"_NAME{SUBOBJECT}", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	// 23 Beekam St. / 318 St -Paul's -Road
	cPattern::create(u"_LINE2ADDRESS", u"",
		1, u"Number", 0, 1, 1,
		2, u"Proper Noun", u"_NAME*1", 0, 1, 1,
		2, u"_STREET_ABB", u"street_address", 0, 1, 1,
		0);
	// Nilam, Nebraska, 19807
	cPattern::create(u"_LINE3ADDRESS", u"1",
		1, u"_NAME", 0, 1, 1, // "US city town village"
		1, u",", 0, 1, 1,
		1, u"Proper Noun", NO_OWNER, 1, 1, // US state territory region
		1, u",", 0, 0, 1,
		1, u"Number", 0, 1, 1,
		0);
	// London E 9 6 QP
	cPattern::create(u"_LINE3ADDRESS", u"2",
		1, u"_NAME", 0, 1, 1, // canadian province city",u"world city town village
		1, u"letter", 0, 1, 1,
		1, u"Number", 0, 1, 1,
		1, u"Number", 0, 1, 1,
		1, u"Proper Noun", NO_OWNER, 1, 1,
		0);
	// p.o. box 3
	cPattern::create(u"_POADDRESS", u"4",
		1, u"abbreviation|p.o.", 0, 1, 1,
		1, u"noun|box", 0, 1, 1,
		1, u"Number", 0, 1, 1,
		0);
	// P O Box 3
	cPattern::create(u"_POADDRESS", u"5",
		1, u"letter|p", 0, 1, 1,
		1, u"letter|o", 0, 1, 1,
		1, u"noun|box", 0, 1, 1,
		1, u"Number", 0, 1, 1,
		0);
	cPattern::create(u"_MADDRESS", u"",
		1, u"_LINE1ADDRESS", 0, 1, 1,
		2, u"_LINE2ADDRESS", u"_POADDRESS", 0, 1, 1,
		1, u",", 0, 1, 1,
		2, u"_LINE3ADDRESS", u"_NAME", 0, 1, 1,
		0);
	// Dave and Jen's place / S & P
	cPattern::create(u"__NAMEOWNER{PLURAL:NAMEOWNER}", u"8",
		1, u"_ADVERB", 0, 0, 1,
		1, u"Proper Noun{FIRST}", 0, 1, 1,
		2, u"coordinator", u"&", 0, 1, 1,
		2, u"noun{FIRST}", u"Proper Noun{FIRST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	// Dr. Helen Billows Mirren's
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"1",
		1, u"_ADVERB", 0, 0, 1,
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"Proper Noun{FIRST}", NO_OWNER, 1, 2,
		2, u"noun{MIDDLE}", u"_ABB{MIDDLE}", NO_OWNER, 0, 1,
		1, u"Proper Noun{LAST}", SINGULAR_OWNER, 1, 1, 0);

	// Dr. Mirren's
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"2",
		1, u"_ADVERB", 0, 0, 1,
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"Proper Noun{LAST:ANY}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1, 0);
	// M. de Louvois / M. Louvois
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"3",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 0, 1,
		3, u"determiner|le{MIDDLE}", u"preposition|de{MIDDLE}", u"noun|van{MIDDLE}", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*2{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 2, 0); // Names ending in a noun should be not common
// D' Artagnan / O'Malley / d'artagnan
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"4",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		1, u"letter{QLAST}", 0, 1, 1,
		1, u"quotes", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*1{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1, 0); // Names ending in a noun should be not common
// Monsieur le Pen / Monsieur le compte / Eamon de Valera // Covento de San -Francisco
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"9",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"Proper Noun{FIRST}", NO_OWNER, 0, 1,
		2, u"determiner|le{MIDDLE}", u"preposition|de{MIDDLE}", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*1{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 2, 0); // Names ending in a noun should be not common
// M. D' Artagnan / K. O'Malley / M. d'artagnan
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"A",
		1, u"__NAMEINTRO", 0, 0, 1,
		1, u"letter{FIRST}", 0, 1, 1,
		1, u".", 0, 0, 1,
		1, u"Proper Noun{MIDDLE}", NO_OWNER, 0, 1,
		1, u"letter{QLAST}", 0, 1, 1,
		1, u"quotes", 0, 1, 1,
		2, u"Proper Noun{LAST}", u"noun*1{LAST}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1, 0);  // Names ending in a noun should be not common
// Number Fourteen's voice filled the room.
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"Q",
		1, u"Proper Noun|number", 0, 1, 1,
		3, u"Number*-3{ANY}", u"roman_numeral*-3{ANY}", u"numeral_cardinal*-3{ANY}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	cPattern::create(u"__NAMEOWNER{NAMEOWNER}", u"H",
		1, u"honorific{SINGULAR:HON}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);

	cPattern::create(u"__NAMEOWNER{NAMEOWNER:SINGULAR}", u"R",
		1, u"letter", 0, 1, 1,
		1, u"dash*-2", 0, 0, 1,
		1, u"Number", SINGULAR_OWNER, 1, 2,
		0);
	cPattern::create(u"_NAMEOWNER", u"", 1, u"__NAMEOWNER", 0, 1, 1, 0);

}

/*
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

// Exact iterator equality of all parts. Does not compare nickName.
bool cName::operator == (const cName& n)
{
	LFS
		return hon == n.hon && hon2 == n.hon2 && hon3 == n.hon3 &&
		first == n.first && middle == n.middle && middle2 == n.middle2 && last == n.last && suffix == n.suffix &&
		any == n.any;
}
*/
bool cName::getNickName(tIWMM firstName)
{
	LFS
		unordered_map<lpwstring, int>::iterator iNickname;
	iNickname = nicknameEquivalenceMap.find(firstName->first);
	if (iNickname == nicknameEquivalenceMap.end()) return false;
	nickName = iNickname->second;
	return true;
}

// Append one name part to accumulate, capitalizing the first letter.
// printShort omits the “H1:” / “F:” label.
void cName::hn(const lpchar_t* namePartName, tIWMM namePart, lpwstring& accumulate, bool printShort, const lpchar_t* separator = u" ")
{
	LFS
		if (namePart == wNULL) return;
	if (!printShort)
	{
		accumulate += namePartName;
		accumulate += u":";
	}
	int len = accumulate.length();
	accumulate += namePart->first + separator;
	accumulate[len] = towupper(accumulate[len]);
}

// Format all parts into message (in/out). Appends “[nickId]” when not short.
lpwstring cName::print(lpwstring& message, bool printShort, const lpchar_t* separator = u" ")
{
	LFS
		hn(u"H1", hon, message, printShort, separator);
	hn(u"H2", hon2, message, printShort, separator);
	hn(u"H3", hon3, message, printShort, separator);
	hn(u"F", first, message, printShort, separator);
	hn(u"M1", middle, message, printShort, separator);
	hn(u"M2", middle2, message, printShort, separator);
	hn(u"u", last, message, printShort, separator);
	hn(u"A", any, message, printShort, separator);
	hn(u"S", suffix, message, printShort, separator);
	if (nickName >= 0 && !printShort)
	{
		lpchar_t temp[10];
		lp_wsprintf(temp, u"[%d]", nickName);
		message += temp;
	}
	if (message.length() > 0 && message[message.length() - 1] == u' ')
		message.erase(message.length() - 1);
	return message;
}

// Append namePart + separationCharacter, capitalizing the first letter.
// False if namePart is wNULL.
bool cName::hn(tIWMM namePart, lpchar_t separationCharacter, lpwstring& accumulate)
{
	LFS
		if (namePart == wNULL) return false;
	int currentPosition = accumulate.length();
	accumulate += namePart->first + separationCharacter;
	accumulate[currentPosition] = toupper(accumulate[currentPosition]);
	return true;
}

// Space-separated original-order name (honors optional). Strips the trailing
// separator. justFirstAndLast omits honors / middles / suffix.
lpwstring cName::original(lpwstring& message, lpchar_t separationCharacter, bool justFirstAndLast)
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

// Name-part equality: identical string, or numeral_cardinal vs Number
// (“fourteen” / “14”), or first-letter match when either is a single
// letter. Null parts return returnTrueOnNull (default true).
bool cName::match(tIWMM sub1, tIWMM sub2, bool returnTrueOnNull)
{
	LFS
		if (sub1 == wNULL || sub2 == wNULL) return returnTrueOnNull;
	// compare Number 14 with Number Fourteen
	if (sub1->second.query(numeralCardinalForm) >= 0 && sub2->second.query(NUMBER_FORM_NUM) >= 0)
	{
		int anyNum1 = mapNumeralCardinal(sub1->first);
		int anyNum2 = lp_wtoi(sub2->first.c_str());
		if (anyNum1 == anyNum2) return true;
	}
	else if (sub2->second.query(numeralCardinalForm) >= 0 && sub1->second.query(NUMBER_FORM_NUM) >= 0)
	{
		int anyNum2 = mapNumeralCardinal(sub2->first);
		int anyNum1 = lp_wtoi(sub1->first.c_str());
		if (anyNum1 == anyNum2) return true;
	}
	if (sub1->first.length() == 1 || sub2->first.length() == 1)
		return (sub1->first[0] == sub2->first[0]);
	return (sub1->first == sub2->first);
}

// True if inhon is already in hons (honorific-merge helper).
bool cName::in(tIWMM inhon, vector <tIWMM>& hons)
{
	LFS
		vector <tIWMM>::iterator hi = hons.begin(), hiEnd = hons.end();
	for (; hi != hiEnd; hi++) if (inhon == *hi) return true;
	return false;
}

// Copy w2 onto w1 if w1 is empty/a bare initial (length <= 1) and w2 is longer.
// A w1 that already holds a full part is left untouched.
void cName::merge(tIWMM& w1, tIWMM w2)
{
	LFS
	if (w2 != wNULL && (w1 == wNULL || (w1->first.size() <= 1 && w2->first.size() > 1)))
		w1 = w2;
}

// Union honorifics (up to 3) and fill empty / letter parts from n. Drops
// `any` once first or last is set. Logs when t.traceObject/SpeakerResolution.
// merge first, middle, middle2 or last if we only have a letter and n has more.
void cName::merge(cName& n, sTrace& t)
{
	LFS
		vector <tIWMM> hons;
	lpwstring name1, name2, name3;
	if (t.traceObjectResolution || t.traceSpeakerResolution)
	{
		n.print(name1, false);
		print(name2, false);
	}
	/*
	if (n.any!=wNULL)
	{
		if (t.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,u"merge %s into %s (n.any).",name1.c_str(),name2.c_str());
		return;
	}
	if (any!=wNULL)
	{
		if (t.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,u"merge %s into %s (any).",name1.c_str(),name2.c_str());
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
				lplog(LOG_RESOLUTION, u"merge %s into %s -> %s dropped ('any' designation) %s.", name1.c_str(), name2.c_str(), name3.c_str(), tAny->first.c_str());
			else
				lplog(LOG_RESOLUTION, u"merge %s into %s -> %s.", name1.c_str(), name2.c_str(), name3.c_str());
		}
	}
	else if (t.traceSpeakerResolution)
	{
		print(name3, false);
		lplog(LOG_RESOLUTION, u"merge %s into %s -> %s.", name1.c_str(), name2.c_str(), name3.c_str());
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
// Compatible names (same person possible): matching middle/last/suffix,
// first or shared nickName, honorific-only subset, Mrs ≠ Miss. `any`
// matches first or last of length > 1. Does not compare plural or sex.
// does not compare plural or sex
bool cName::like(cName& n, sTrace& t)
{
	LFS
		if (t.traceNameResolution)
		{
			lpwstring tmpstr, tmpstr2;
			lplog(LOG_RESOLUTION, u"Comparing %s WITH %s", n.print(tmpstr, false).c_str(), print(tmpstr2, false).c_str());
		}
	if (isCompletelyNull() || n.isCompletelyNull())
	{
		lpwstring tmpstr, tmpstr2;
		lplog(LOG_RESOLUTION, u"Null comparison comparing %s WITH %s", n.print(tmpstr, false).c_str(), print(tmpstr2, false).c_str());
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
			if (last->first != u"something" && n.last->first != u"something" &&
				// Comparing H1:sir F:james M1:peel L:edgerton [106] WITH H1:sir L:james 
				// but rule out Comparing H1:dr L:adams  WITH H1:mr F:a L:carter (first->first.length()!=1)
				!(first == wNULL && n.first != wNULL && n.first->first.length() != 1 && match(n.first, last)) &&
				!(n.first == wNULL && first != wNULL && first->first.length() != 1 && match(first, n.last)))
				return false;
			// don't match Boris Something with Mr. Carter
			if ((last->first == u"something" && n.first == wNULL) || (n.last->first == u"something" && first == wNULL))
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
			if (hon->first == u"mrs" && n.hon->first == u"miss") return false;
			if (n.hon->first == u"mrs" && hon->first == u"miss") return false;
		}
		return true;
	}
	// if any don't match, dump.
	if (nickName != -1 && n.nickName == nickName)
		return true;
	if (hon != wNULL && n.hon != wNULL)
	{
		// Miss (Janet) Vandermeyer and Mrs. Vandermeyer must not match!  50055
		if (hon->first == u"mrs" && n.hon->first == u"miss") return false;
		if (n.hon->first == u"mrs" && hon->first == u"miss") return false;
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

// True if only hon is set (no first/last/any) — “Miss” / “Lord”.
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
// True if this looks like a ship/newspaper/place (“The Lusitania”,
// “St. James Park”): no hon/suffix, last is primarily noun/adjective, first
// has no census sex. startsWithDeterminer / ownedByName / len relax the
// first+last requirement.
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
	// Stronger than like(): first+last (or suffix+one of them) with
	// sexConfidentMatch. False immediately if sex is not confident.
	// self and n are already like each other (like==true)
bool cName::confidentMatch(cName& n, bool sexConfidentMatch, sTrace& t)
{
	LFS
		if (t.traceNameResolution)
		{
			lpwstring tmpstr, tmpstr2;
			lplog(LOG_RESOLUTION, u"Comparing %s WITH %s CONFIDENT MATCH", n.print(tmpstr, false).c_str(), print(tmpstr2, false).c_str());
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



// Fill name + sex/plural/business from a _NAME tag-set (HON/FIRST/MIDDLE/
// LAST/ANY/SUFFIX/BUS). Single-letter ANY is rejected unless followed by “.”
// or preceded by a Number (“3 M”).
bool cSource::evaluateName(vector <cTagLocation>& tagSet, cName& name, bool& isMale, bool& isFemale, bool& isPlural, bool& isBusiness)
{
	LFS
		name.hon = name.hon2 = name.hon3 = name.first = name.middle = name.middle2 = name.last = name.suffix = name.any = wNULL;
	name.nickName = -1;
	if (isBusiness = (findOneTag(tagSet, u"BUS", -1) >= 0)) return true;
	//bool added=false;
	isMale = isFemale = false;
	int nextFirst = -1, nextMiddle = -1, nextLast = -1, nextHon1 = -1;
	int whereHon1 = findTag(tagSet, u"HON", nextHon1), whereHon2 = findOneTag(tagSet, u"HON2", -1), whereHon3 = findOneTag(tagSet, u"HON3", -1);
	int whereFirst = findTag(tagSet, u"FIRST", nextFirst);
	int whereAny = findOneTag(tagSet, u"ANY", -1);
	int whereMiddle = findTag(tagSet, u"MIDDLE", nextMiddle);
	if (whereMiddle < 0 && nextFirst >= 0)
		whereMiddle = nextFirst;
	int whereLast = findTag(tagSet, u"LAST", nextLast);
	//int whereQLast=findTag(tagSet,u"QLAST",nextQLast);
	int whereSuffix = findOneTag(tagSet, u"SUFFIX", -1);
	isPlural |= findOneTag(tagSet, u"PLURAL", -1) >= 0;
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
			return (name.any->first.length() > 1) || (tagSet[whereAny].sourcePosition + 1 < (int)m.size() && m[tagSet[whereAny].sourcePosition + 1].word->first == u".") || // a person cannot be referred to by a single letter (with no period after it)
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
		if (name.hon != wNULL && name.first == wNULL && name.hon->first == u"st")
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

// Possessive name at `where` (__NAMEOWNER or “ProperNoun’s”). Fills name +
// sex. True if a tag-set (or the single-token owner fallback) matched.
bool cSource::identifyNameAdjective(int where, cName& name, bool& isMale, bool& isFemale)
{
	LFS
		int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(u"__NAMEOWNER", nameEnd)) == -1)
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
	if (startCollectTags(false, nameTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, u"identify name adjective") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, u"NR", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateNameAdjective(tagSets[J], name, isMale, isFemale))
				return true;
		}
	return false;
}

// _NAME at `where` -> evaluateName. Plural if the last token ends in ‘s’
// and the phrase starts with “the” / a quantifier other than “one”.
// possible combinations:
// A
// H1 or H1 && H2 or H1 && H2 && H3
// F or F&L or F&L&M1 or F&L&M1&M2
bool cSource::identifyName(int where, int& element, cName& name, bool& isMale, bool& isFemale, bool& isPlural, bool& isBusiness)
{
	LFS
		int nameEnd = -1;
	if ((element = m[where].pma.queryPattern(u"_NAME", nameEnd)) == -1) return false;
	nameEnd += where;
	// if last letter is 's', preceded by The or quantifier, then plural
	isPlural = (m[nameEnd - 1].word->first[m[nameEnd - 1].word->first.size() - 1] == u's' &&
		(m[where].word->first == u"the" || (m[where].queryForm(u"quantifier") >= 0 && m[where].word->first != u"one")));
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(false, nameTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, u"identify name") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, u"NR", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateName(tagSets[J], name, isMale, isFemale, isPlural, isBusiness))
				return true;
		}
	return false;
}

// Full object-creation name test: _NAME or __NAMEOWNER, plus honorific-only
// “the archdeacon”, Nurse+given, neuterName / business / demonym class
// assignment. Sets comparableName / requestWikiAgreement / objectClass.
bool cSource::identifyName(int begin, int principalWhere, int end, int& nameElement, cName& name, bool& isMale,
	bool& isFemale, bool& isNeuter, bool& isPlural, bool& isBusiness, bool& comparableName,
	bool& comparableNameAdjective, bool& requestWikiAgreement, OC& objectClass)
{
	LFS
		if (!(comparableName = (nameElement != -1 && identifyName(principalWhere, nameElement, name, isMale, isFemale, isPlural, isBusiness))))
			comparableNameAdjective = identifyNameAdjective(principalWhere, name, isMale, isFemale);
	// scan for after adjectives
	for (int aa = begin + 1, len = 0; aa < end; aa++)
		if (m[aa].pma.queryPattern(u"_ADJECTIVE_AFTER", len) && aa + len == end)
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
		(m[principalWhere - 1].queryWinnerForm(adjectiveForm) < 0 || m[principalWhere].queryForm(u"pinr") < 0))
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
		lpwstring tmpstr;
		lplog(LOG_RESOLUTION, u"%06d:Used common profession of name %s", principalWhere, name.print(tmpstr, false).c_str());
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
		lpwstring nw;
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
				(m[principalWhere].queryForm(indefinitePronounForm) < 0 && m[principalWhere].word->first != u"there"))
				objectClass = NAME_OBJECT_CLASS;
			if (!isMale && !isFemale) isMale = isFemale = true;
			isNeuter = false;
		}
		/* Parks
			 two Johnnies
			 */
		if (m[end - 1].queryWinnerForm(nounForm) >= 0 || (m[begin].queryForm(numeralCardinalForm) >= 0 && m[begin].word->first != u"one"))
			isPlural = (m[end - 1].word->second.inflectionFlags & PLURAL) == PLURAL;
		return objectClass == NAME_OBJECT_CLASS || objectClass == NON_GENDERED_NAME_OBJECT_CLASS;
	}
	return false;
}

// Read MALE/FEMALE (and capitalized-only) inflection flags at tagSet[where]
// into isMale/isFemale. If sex is already exclusive and not plural, leave
// it. where < 0 -> wNULL.
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
// evaluateName for an __NAMEOWNER tag-set (possessive). Same HON/FIRST/…
// layout; no suffix / business.
bool cSource::evaluateNameAdjective(vector <cTagLocation>& tagSet, cName& name, bool& isMale, bool& isFemale)
{
	LFS
		name.hon = name.hon2 = name.hon3 = name.first = name.middle = name.middle2 = name.last = name.suffix = name.any = wNULL;
	isMale = isFemale = false;
	int nextFirst = -1, nextMiddle = -1, nextLast = -1, nextHon1 = -1;
	int whereHon1 = findTag(tagSet, u"HON", nextHon1), whereHon2 = findOneTag(tagSet, u"HON2", -1), whereHon3 = findOneTag(tagSet, u"HON3", -1);
	int whereFirst = findTag(tagSet, u"FIRST", nextFirst);
	int whereAny = findOneTag(tagSet, u"ANY", -1);
	int whereMiddle = findTag(tagSet, u"MIDDLE", nextMiddle);
	if (whereMiddle < 0 && nextFirst >= 0)
		whereMiddle = nextFirst;
	int whereLast = findTag(tagSet, u"LAST", nextLast);
	//int whereQLast=findTag(tagSet,u"QLAST",nextQLast);
	bool isPlural = findOneTag(tagSet, u"PLURAL", -1) != -1;
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
			return (name.any->first.length() > 1) || (tagSet[whereAny].sourcePosition + 1 < (int)m.size() && m[tagSet[whereAny].sourcePosition + 1].word->first == u"."); // a person cannot be referred to by a single letter (with no period after it)
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

// _META_NAME_EQUIVALENCE / _META_SPEAKER / _META_GROUP / _ANNOUNCE patterns
// (“X, known as Y”, “my name is”, “call me”, “X and Y”).
// These patterns match names to objects.
// _META_NAME_EQUIVALENCE is usually inQuote, but doesn't have to be.
// _META_SPEAKER is always inQuote.
// add INFP "wanted to be called "Brown"
void createMetaNameEquivalencePatterns(void)
{
	LFS
		//cPattern *p=NULL;
		cPattern::create(u"_META_PP{_IGNORE}", u"",
			3, u"to", u"preposition|by", u"preposition|at", 0, 1, 1,
			2, u"__NOUN", u"__MNOUN", 0, 1, 1,
			0);
	// NAME, known _PP as NAME
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"1",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, u",", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		3, u"verb|known", u"verb|called", u"verb|named", VERB_PAST_PARTICIPLE, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_META_PP", 0, 0, 1, // to her friends
		2, u"as", u"preposition|by", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// NAME is also known as NAME / I am also known as NAME
	// he/she/Donny/the young man was called/named
	// the man known as Number One
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"2",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		2, u"_IS", u"is", 0, 0, 1, // (must be optional for) the man known as Number One
		3, u"verb|known", u"verb|called", u"verb|named", VERB_PAST_PARTICIPLE, 1, 1,
		1, u"_PP", 0, 0, 1,
		2, u"as", u"preposition|by", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// Rafid Ahmed Alwan al-Janabi (Arabic: رافد أحمد علوان‎, Rāfid Aḥmad Alwān; born 1968), known by the Defense Intelligence Agency cryptonym "Curveball", is an Iraqi citizen
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"T",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, u",", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		3, u"verb|known", u"verb|called", u"verb|named", VERB_PAST_PARTICIPLE, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_META_PP", 0, 0, 1, // to her friends
		2, u"preposition|as", u"preposition|by", 0, 0, 1,
		// this is NOUN[G]
		5, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET}", u"interrogative_determiner{DET}", u"quantifier{DET}", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 0, 2,
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 1, 1,
		0);
	// my/his/her/Donny's name is/was "Dumpling".
	// He gave 'his name as Count Stepanov'
	// he gave her his name : sir James Peel Edgerton .
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"3",
		2, u"possessive_determiner{NAME_PRIMARY}", u"_NAMEOWNER{NAME_PRIMARY}", 0, 1, 1, // his, her, Donny's
		1, u"_ADJECTIVE", 0, 0, 1,
		2, u"noun|name", u"noun|birthname", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		3, u"is{V_AGREE:V_OBJECT}", u"preposition|as", u":", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// Miss Prudence Cowley, fifth daughter of Archdeacon Cowley of Little Missendell, Suffolk.
	cPattern::create(u"__RENOUN{_IGNORE:NOUN}", u"",
		7, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET}", u"interrogative_determiner{DET}", u"quantifier{DET}", u"__HIS_HER_DETERMINER*1", u"_NAMEOWNER{DET}", 0, 0, 1,
		1, u"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		2, u"noun{N_AGREE}", u"indefinite_pronoun{N_AGREE}", NO_OWNER | FEMALE_GENDER | MALE_GENDER, 1, 1, // Mister Carbonell, the only brother 
		0);
	// He called him Brown.
	// He addressed the other as Boris.
	// His friends nicknamed him "Mr. Brown"
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"4",
		1, u"__NOUN", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		15, u"verb|called", u"verb|named", u"verb|renamed", u"verb|nicknamed", u"verb|addressed",
		u"verb|call", u"verb|name", u"verb|rename", u"verb|nickname", u"verb|address",
		u"verb|calls", u"verb|names", u"verb|renames", u"verb|nicknames", u"verb|addresses", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"personal_pronoun_accusative{NAME_PRIMARY}", u"__RENOUN{NAME_PRIMARY}", 0, 1, 1,
		2, u"preposition|as", u"preposition|by", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// He called himself Brown. / I called myself Bob.
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"5",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		9, u"verb|called", u"verb|named", u"verb|nicknamed", u"verb|call", u"verb|name", u"verb|nickname", u"verb|calls", u"verb|names", u"verb|nicknames", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"reflexive_pronoun{NAME_PRIMARY}", 0, 1, 1,
		1, u"as", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// His friends gave him the nickname of "Mr. Brown"
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"6",
		2, u"__NOUN", u"__MNOUN", 0, 1, 1,
		1, u"__ALLVERB", 0, 1, 1,
		1, u"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // 3,u"him{NAME_PRIMARY}",u"her{NAME_PRIMARY}",u"them{NAME_PRIMARY}",0,0,1,
		1, u"determiner|the", 0, 1, 1,
		2, u"noun|nickname", u"noun|name", 0, 1, 1,
		1, u"preposition|of", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// He took the nickname of "Red".
	// But NOT  I[tuppence] really did invent the name of Jane Finn ! 
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"7",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		3, u"verb|got", u"verb|took", u"verb|received", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"noun|nickname", u"noun|name", 0, 1, 1,
		1, u"preposition|of", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"8",
		1, u"_NAME{GNOUN:NAME:NAME_PRIMARY}", 0, 1, 1,
		1, u"__C1_IP", 0, 1, 1,
		0);
	// the elderly woman, looking more like a housekeeper than a servant
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"9",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u",", 0, 1, 1,
		1, u"_VERBONGOING", 0, 1, 1, // looking
		1, u"adverb|more", 0, 1, 1,
		1, u"preposition|like", 0, 1, 1,
		1, u"__RENOUN{_BLOCK:RE_OBJECT}", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		1, u"__RENOUN{_BLOCK:RE_OBJECT:NAME_SECONDARY}", 0, 1, 1,
		0);
	// it ought to be enough for an innocent young girl like Jane .
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"V",
		1, u"preposition", 0, 1, 1,
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u"preposition|like", 0, 1, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		0);
	// female crook, answering to the name[name] of Rita ? ”
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"Q",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u",", 0, 1, 1,
		2, u"verb|answering", u"verb|answers", 0, 1, 1,
		1, u"preposition|to", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"noun|nickname", u"noun|name", 0, 1, 1,
		1, u"preposition|of", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// , a young fellow called Brown. // must be after a comma to prevent confusion with other equivalences
	// about someone called Jane Finn?
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"A",
		2, u",", u"preposition", 0, 1, 1,
		1, u"__RENOUN{NAME_PRIMARY:RE_OBJECT}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		5, u"verb|called", u"verb|named", u"verb|renamed", u"verb|nicknamed", u"verb|addressed", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"preposition|as", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// he is always spoken of by the unassuming title of ‘QS mr . Brown . ’
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"B",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he
		1, u"_IS", 0, 1, 1,
		1, u"verb|spoken{V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		1, u"preposition|of", 0, 1, 1,
		1, u"preposition|by", 0, 1, 1,
		1, u"determiner{DET}", 0, 1, 1,
		1, u"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		1, u"noun|title{N_AGREE}", NO_OWNER, 1, 1,
		1, u"preposition|of", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// he goes by the name "Mr. Brown"
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"C",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he
		1, u"possessive_determiner*4", 0, 0, 1, // removed _ADVERB and added it to later patterns // the hidden object use should be very rare!
		1, u"verb|goes{vS:V_AGREE:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		1, u"preposition|by", 0, 1, 1,
		1, u"determiner{DET}", 0, 1, 1,
		1, u"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		1, u"noun|name{N_AGREE}", NO_OWNER, 1, 1,
		1, u"preposition|of", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	// Tommy put him down as being a Russian or a Pole.
	// him ADV as being NOUN
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"D",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he/him/Bob
		1, u"_ADVERB", 0, 0, 1,
		1, u"preposition|as", 0, 0, 1,
		1, u"verb|being", 0, 1, 1,
		2, u"__NOUN{_BLOCK:NAME_SECONDARY}", u"__MNOUN{_BLOCK:NAME_SECONDARY}", 0, 1, 1,
		0);
	// the girl[miss] put him[julius] down as thirty - five . 
	// we had her down as Rita Vandermeyer.
	// to put him[man] down as an actor or a lawyer
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"E",
		3, u"verb|put", u"verb|have", u"have|had", 0, 1, 1,
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he/him/Bob
		1, u"adverb|down", 0, 0, 1,
		1, u"preposition|as", 0, 1, 1,
		1, u"verb|being", 0, 0, 1,
		2, u"__NOUN{_BLOCK:NAME_SECONDARY}", u"__MNOUN{_BLOCK:NAME_SECONDARY}", 0, 1, 1,
		0);
	// he[julius] was of middle height , and squarely built to match his[julius] jaw[julius] . 
	//	his[julius] face[julius] was pugnacious but pleasant . 
	// 	no one could have mistaken him[julius] for anything but an American 
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"F",
		1, u"indefinite_pronoun|no one", 0, 1, 1,
		1, u"modal_auxiliary|could", 0, 1, 1,
		1, u"have", 0, 1, 1,
		1, u"verb|mistaken", 0, 1, 1,
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // he/him/Bob
		1, u"preposition|for", 0, 1, 1,
		1, u"indefinite_pronoun|anything", 0, 1, 1,
		1, u"preposition|but", 0, 1, 1,
		2, u"__NOUN{_BLOCK:NAME_SECONDARY}", u"__MNOUN{_BLOCK:NAME_SECONDARY}", 0, 1, 1,
		0);
	// Tommy recognized in him[Irish] an Irish Sinn feiner . 
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"G",
		1, u"__C1__S1", 0, 1, 1,
		1, u"__ALLVERB", 0, 1, 1,
		1, u"preposition|in", 0, 1, 1,
		1, u"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // him
		2, u"__NOUN{NAME_SECONDARY}", u"__MNOUN{NAME_SECONDARY}", 0, 1, 1, // a natural actor
		0);
	// I understood her[Janet] to be a niece of Mrs. Vandermeyer's . 
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"R",
		1, u"__C1__S1", 0, 1, 1,
		1, u"__ALLVERB", 0, 1, 1,
		1, u"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // her
		2, u"preposition|to", u"to", 0, 1, 1,
		1, u"be", 0, 1, 1,
		2, u"__NOUN{NAME_SECONDARY}", u"__MNOUN{NAME_SECONDARY}", 0, 1, 1, // a niece
		0);
	// another voice{OWNER: WO -3 another}[which Tommy rather thought was that of Boris replied]
	// another voice{OWNER: WO -3 another}[which Tommy fancied was that of the tall , commanding - looking man whose face had seemed familiar to him]
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"H",
		2, u"__NOUN{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"relativizer", 0, 1, 1, // which / who
		1, u"__C1__S1", 0, 1, 1, // Tommy / another person
		1, u"_ADVERB", 0, 0, 1, // rather
		1, u"_THINKPAST", 0, 1, 1, // thought / fancied / was certain
		2, u"_IS", u"is", VERB_PAST, 1, 1, // was
		2, u"relativizer|that", u"demonstrative_determiner|that", 0, 1, 1, // that
		1, u"preposition|of", 0, 1, 1, // of
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Boris
		0);
	// he gave her his name : sir James Peel Edgerton . (see "3")
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"J",
		1, u"personal_pronoun{NAME_PRIMARY}", SINGULAR, 1, 1,
		1, u"verb|gave", 0, 1, 1,
		1, u"possessive_determiner", 0, 0, 1, // his, her
		1, u"possessive_determiner{NAME_PRIMARY}", SINGULAR, 1, 1, // his, her
		2, u"noun|name", u"noun|birthname", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"preposition|as", u":", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"quotes", OPEN_INFLECTION, 0, 1,
		1, u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 0, 1, 0);
	//  he nevertheless conveyed the impression of a big man
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"K",
		2, u"__NOUN{NAME_PRIMARY}", u"__MNOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u"adverb", 0, 0, 1,
		1, u"verb|conveyed", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"noun|impression", u"noun|look", 0, 1, 1,
		1, u"preposition|of", 0, 0, 1,
		1, u"__RENOUN{_BLOCK:RE_OBJECT:NAME_SECONDARY}", 0, 1, 1,
		0);
	//  a woman dressed as a hospital nurse
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"M",
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		1, u"adverb", 0, 0, 1,
		1, u"verb|dressed", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"preposition|as", 0, 1, 1,
		2, u"__NOUN{NAME_SECONDARY}", u"__MNOUN{NAME_SECONDARY}", 0, 1, 1,
		0);
	// medical man written all over him
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"N",
		1, u"__RENOUN{NAME_SECONDARY}", 0, 1, 1, // cannot be __NOUN because of incorrectly disambiguated noun grouping - 
		1, u"adverb", 0, 0, 1,                   // I[julius] was lying in bed[bed] with a hospital nurse ( not Whittington's one[nurse] )
		1, u"verb|written", 0, 1, 1,						 //  on one side of me[julius] , and a little black - bearded man with gold glasses , 
		1, u"adverb|all", 0, 1, 1,               // and medical man written all[all] over him[man] , on the other[side] .
		1, u"preposition|over", 0, 1, 1,
		1, u"__NOUN{NAME_PRIMARY}", 0, 1, 1,
		0);
	// He[tommy] recognized it[voice] at once for that of the bearded and efficient German[man] ,
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"P",
		1, u"__C1__S1", 0, 1, 1,
		1, u"verb|recognized", 0, 1, 1,
		5, u"personal_pronoun_nominative{N_AGREE:NAME_SECONDARY}", u"personal_pronoun_accusative{N_AGREE:NAME_SECONDARY}", u"personal_pronoun{N_AGREE:NAME_SECONDARY}", u"noun{NAME_SECONDARY}", u"_NAME{NAME_SECONDARY}", 0, 1, 1,
		1, u"_META_PP", 0, 0, 1, // at once
		2, u"preposition|for", u"preposition|as", 0, 1, 1,
		2, u"__NOUN{NAME_PRIMARY}", u"__MNOUN{NAME_PRIMARY}", 0, 1, 1, // a natural actor
		0);
	// they knew him now for a spy
	cPattern::create(u"_META_NAME_EQUIVALENCE{_IGNORE}", u"S",
		1, u"__C1__S1", 0, 1, 1,
		1, u"verb|knew", VERB_PAST, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		4, u"personal_pronoun_accusative{N_AGREE:NAME_PRIMARY}", u"personal_pronoun{N_AGREE:NAME_PRIMARY}", u"noun{NAME_PRIMARY}", u"_NAME{NAME_PRIMARY}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"preposition|for", 0, 1, 1,
		1, u"__NOUN{NAME_SECONDARY}", 0, 1, 1, // a natural actor
		0);

	// patterns in quotes that indicate an entity has become physically present
	cPattern::create(u"_META_ANNOUNCE{_IGNORE:_ONLY_BEGIN_MATCH}", u"1",
		1, u"noun|here", 0, 1, 1,
		2, u"verb|is", u"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		3, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", u"__MNOUN{NAME_PRIMARY}", 0, 1, 1, // Bob / another knock
		0);
	cPattern::create(u"_META_ANNOUNCE{_IGNORE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"2",
		1, u"noun|here", 0, 1, 1,
		3, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", u"__MNOUN{NAME_PRIMARY}", 0, 1, 1, // He
		2, u"verb|is", u"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		0);
	// patterns in quotes that indicate the person speaking [ over the phone ]
	// Bob speaking.
	cPattern::create(u"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", u"1",
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"verb|speaking", VERB_PRESENT_PARTICIPLE, 1, 1,
		0);
	// Bob here.
	cPattern::create(u"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", u"2",
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"noun|here", 0, 1, 1,
		0);
	// The following pattern is only indicative of the speaker if
	// we are sure that this is a conversation over the phone - but how can we be sure?
	// This is Bob.
	cPattern::create(u"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", u"3",
		1, u"demonstrative_determiner|this", 0, 1, 1,
		2, u"verb|is", u"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		0);
	// The Sinn Feiner was speaking
	cPattern::create(u"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", u"4",
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"verb|was", VERB_PAST, 1, 1,
		1, u"verb|speaking", 0, 1, 1,
		0);
	// I am Dr. Hall / you are Conrad
	cPattern::create(u"_META_SPEAKER{_IGNORE:_ONLY_BEGIN_MATCH}", u"5",
		2, u"personal_pronoun_nominative|i{NAME_SECONDARY}", u"personal_pronoun|you{NAME_SECONDARY}", 0, 1, 1,
		2, u"is|am", u"is|are", 0, 1, 1,
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		0);
	// What is your name?
	cPattern::create(u"_META_SPEAKER_QUERY{_IGNORE:_ONLY_END_MATCH:_QUESTION}", u"1",
		1, u"relativizer|what", 0, 1, 1,
		3, u"verb|is", u"is|ishas", u"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, u"possessive_determiner{NAME_ABOUT}", 0, 1, 1,
		1, u"noun|name", 0, 1, 1,
		0);
	// Who are you? / Who is Annie?
	cPattern::create(u"_META_SPEAKER_QUERY{_IGNORE:_ONLY_END_MATCH:_QUESTION}", u"2",
		1, u"relativizer|who", 0, 1, 1,
		3, u"is|are", u"is|is", u"is|ishas", VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		2, u"personal_pronoun{NAME_ABOUT}", u"_NAME{NAME_ABOUT}", 0, 1, 1,
		0);
	// Under the name of -- --
	cPattern::create(u"_META_SPEAKER_QUERY{_IGNORE:_ONLY_END_MATCH:_QUESTION}", u"3",
		1, u"preposition|under", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"noun|name", 0, 1, 1,
		1, u"preposition|of", 0, 1, 1,
		1, u"dash{NAME_ABOUT}", 0, 1, 3,
		0);
	// What girl?
	cPattern::create(u"_META_SPEAKER_QUERY{_IGNORE:_STRICT_NO_MIDDLE_MATCH:_QUESTION}", u"4",
		2, u"relativizer|what", u"relativizer|which", 0, 1, 1,
		1, u"noun{NAME_ABOUT}", 0, 1, 1,
		0);
	// , sir.
	cPattern::create(u"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE{_IGNORE:_ONLY_END_MATCH}", u"",
		1, u",", 0, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"_PP", 0, 1, 1, // sir / of course
		0);
	// Bob. / 'Ouse parlourmaid
	cPattern::create(u"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", u"1",
		2, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);
	// My name is Bob.
	cPattern::create(u"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", u"2",
		1, u"possessive_determiner{NAME_ABOUT}", 0, 1, 1,
		1, u"noun|name", 0, 1, 1,
		2, u"verb|is", u"is", VERB_PRESENT_THIRD_SINGULAR, 1, 1,
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);
	// You can call me Bob.
	cPattern::create(u"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", u"3",
		1, u"personal_pronoun|you", 0, 1, 1,
		1, u"modal_auxiliary|can", 0, 1, 1,
		1, u"verb|call", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		1, u"personal_pronoun_accusative{NAME_ABOUT}", 0, 1, 1,
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);
	// Call me Bob.
	cPattern::create(u"_META_SPEAKER_QUERY_RESPONSE{_IGNORE:_STRICT_NO_MIDDLE_MATCH}", u"4",
		1, u"verb|call", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		1, u"personal_pronoun_accusative{NAME_ABOUT}", 0, 1, 1,
		1, u"_NAME{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"_META_SPEAKER_QUERY_RESPONSE_AUDIENCE", 0, 0, 1, // , sir
		0);

	// Bob came with him
	cPattern::create(u"_META_GROUP{_IGNORE}", u"1",
		2, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"__ALLVERB", 0, 1, 1,
		1, u"preposition|with", 0, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// With him came Mr. Bill
	cPattern::create(u"_META_GROUP{_IGNORE:_ONLY_BEGIN_MATCH}", u"2",
		2, u"preposition|with", u"preposition|by", 0, 1, 1,
		3, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", u"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"__ALLVERB", 0, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// He ran next to Mr. Bill.
	cPattern::create(u"_META_GROUP{_IGNORE}", u"3",
		2, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"__ALLVERB", 0, 1, 1,
		1, u"preposition|next", 0, 1, 1,
		1, u"to", 0, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// Next to him ran Mr. Bill.
	cPattern::create(u"_META_GROUP{_IGNORE:_ONLY_BEGIN_MATCH}", u"4",
		1, u"preposition|next", 0, 1, 1,
		1, u"preposition|to", 0, 1, 1,
		3, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", u"personal_pronoun_accusative{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"__ALLVERB", 0, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// He was accompanied by Mr. Bill
	cPattern::create(u"_META_GROUP{_IGNORE}", u"5",
		2, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		2, u"_IS", u"is", 0, 0, 1, // (must be optional for) the man known as Number One
		4, u"verb|accompanied", u"verb|followed", u"verb|led", u"verb|joined", 0, 1, 1,
		1, u"preposition|by", 0, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
	// He accompanied Tuppence.
	cPattern::create(u"_META_GROUP{_IGNORE}", u"6",
		2, u"_NAME{NAME_PRIMARY}", u"__NOUN{NAME_PRIMARY}", 0, 1, 1, // Bob
		1, u"_HAVE", 0, 0, 1, // He had joined them
		4, u"verb|accompanied", u"verb|followed", u"verb|led", u"verb|joined", 0, 1, 1,
		2, u"_NAME{NAME_SECONDARY}", u"__NOUN{NAME_SECONDARY}", 0, 1, 1, // Bob
		0);
}

// Copy gender from eFrom onto eTo and, when both are non-pronouns, add
// each other as aliases. eFrom may be a cObject::eOBJECTS sentinel
// (UNKNOWN_MALE, …) that only sets flags.
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
		if (objects[eFrom].objectClass != GENDERED_GENERAL_OBJECT_CLASS || m[objects[eFrom].originalLocation].queryForm(u"pinr") < 0)
		{
			lpwstring tmpstr, tmpstr2;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:Object %s gained alias %s (1).", where, objectString(eTo, tmpstr, true).c_str(), objectString(eFrom, tmpstr2, true).c_str());
			objects[eFrom].aliases.push_back(eTo);
			//if (objects[eTo].objectClass==NAME_OBJECT_CLASS)
			objects[eTo].aliases.push_back(eFrom);
		}
	}
}

struct {
	const lpchar_t* nc;
	int num;
} numeralCardinalMap[] = {
	{ u"zero", 0 },
	{ u"naught", 0 },
	{ u"one", 1 }, { u"two", 2 }, { u"three", 3 }, { u"four", 4 }, { u"five", 5 }, { u"six", 6 }, { u"seven", 7 }, { u"eight", 8 }, { u"nine", 9 }, { u"ten", 10 },
	{ u"eleven", 11 }, { u"twelve", 12 }, { u"dozen", 12 }, { u"thirteen", 13 }, { u"fourteen", 14 }, { u"fifteen", 15 }, { u"sixteen", 16 }, { u"seventeen", 17 }, { u"eighteen", 18 }, { u"nineteen", 19 }, { u"twenty", 20 },
	{ u"umpteen", 15 }, { u"gross", 144 },
	{ u"thirty", 30 }, { u"forty", 40 }, { u"fifty", 50 }, { u"sixty", 60 }, { u"seventy", 70 }, { u"eighty", 80 }, { u"ninety", 90 },
	{ u"hundred", 100 }, { u"thousand", 1000 }, { u"million", 1000000 }, { u"billion", 1000000000 },
	{ NULL, -1 } };
// “one”..“billion” / “dozen” / “umpteen” -> int. Unknown -> -1 (sentinel
// at the end of numeralCardinalMap).
int mapNumeralCardinal(const lpwstring& word)
{
	LFS
		int agei = 0;
	for (; numeralCardinalMap[agei].nc && numeralCardinalMap[agei].nc != word; agei++);
	return numeralCardinalMap[agei].num;
}

struct {
	const lpchar_t* nc;
	int num;
} numeralOrdinalMap[] = {
	{ u"zeroth", 0 },
	{ u"first", 1 }, { u"second", 2 }, { u"third", 3 }, { u"fourth", 4 }, { u"fifth", 5 }, 
	{ u"sixth", 6 }, { u"seventh", 7 }, { u"eighth", 8 }, { u"ninth", 9 }, { u"tenth", 10 },
	{ u"eleventh", 11 }, { u"twelfth", 12 }, { u"thirteenth", 13 }, { u"fourteenth", 14 }, { u"fifteenth", 15 }, 
	{ u"sixteenth", 16 }, { u"seventeenth", 17 }, { u"eighteenth", 18 }, { u"nineteenth", 19 }, { u"twentieth", 20 }, 
	{ u"umpteenth", 15 },
	{ u"thirtieth", 30 }, { u"fortieth", 40 }, { u"fiftieth", 50 }, { u"sixtieth", 60 }, { u"seventieth", 70 }, { u"eightieth", 80 }, { u"ninetieth", 90 },
	{ u"hundredth", 100 }, { u"thousandth", 1000 }, { u"millionth", 1000000 }, { u"billionth", 1000000000 },
	{ NULL, -1 } };
// “first”..“billionth”, or a digit string ending in “th”. Unknown -> -1.
int mapNumeralOrdinal(const lpwstring& word)
{
	LFS
		if (word.length() > 2 && iswdigit(word[0]) && word[word.length() - 2] == 't' && word[word.length() - 1] == 'h')
		{
			return lp_wtoi(word.c_str());
		}
	int agei = 0;
	for (; numeralOrdinalMap[agei].nc && numeralOrdinalMap[agei].nc != word; agei++);
	return numeralOrdinalMap[agei].num;
}

// If secondary is “thirty-five years”, attach young (<40) or old (>=50)
// to primary. False for “one of the …” / relative-clause “the German was one”.
// Julius was about thirty-five.
bool cSource::ageDetection(int where, int primary, int secondary)
{
	LFS
		// This was one of the ...
		if (m[objects[secondary].originalLocation].relPrep >= 0 && m[m[objects[secondary].originalLocation].relPrep].word->first == u"of")
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
			if (m[I].word->first != u"years" && m[I].word->first != u"year")
				return false;
			yearsFound = true;
		}
	if (!age || (age == 1 && !yearsFound)) return false;
	if (debugTrace.traceSpeakerResolution)
	{
		lpwstring tmpstr;
		lplog(LOG_RESOLUTION, u"%06d:Object %s has age %d.", where, objectString(primary, tmpstr, true).c_str(), age);
	}
	if (age < 40 && find(objects[primary].associatedAdjectives.begin(), objects[primary].associatedAdjectives.end(), Words.gquery(u"young")) == objects[primary].associatedAdjectives.end())
		objects[primary].associatedAdjectives.push_back(Words.gquery(u"young"));
	if (age >= 50 && find(objects[primary].associatedAdjectives.begin(), objects[primary].associatedAdjectives.end(), Words.gquery(u"old")) == objects[primary].associatedAdjectives.end())
		objects[primary].associatedAdjectives.push_back(Words.gquery(u"old"));
	return true;
}

// True if the primary mention is a multi-noun (MNOUN) of tag.len — reject
// meta-name equivalence against a coordinated NP.
bool cSource::primaryIsMNoun(int where, int wherePrimary, cTagLocation &tag)
{
	if (tag.len > 1)
	{
		cPatternMatchArray::tPatternMatch* pma = m[wherePrimary].pma.content;
		for (unsigned int PMAElement = 0; PMAElement < m[wherePrimary].pma.count; PMAElement++, pma++)
			if (pma->len == tag.len && patterns[pma->getPattern()]->hasTag(MNOUN_TAG))
			{
				if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
				{
					lpwstring tmpstr;
					int primaryNameObject = m[wherePrimary].getObject();
					lplog(LOG_RESOLUTION, u"%06d:Metaname equivalence rejected (primary is multiple) for primary %d:[%s]", where, wherePrimary, objectString(primaryNameObject, tmpstr, false).c_str());
				}
				return true;
			}
	}
	return false;
}

// Scan (where, wherePrimary) for the latest _REL1 / __S1 / _Q2 / verb,
// writing the four last* outs (used before resolveObject).
void cSource::findLast(int where, int wherePrimary, int &tmpLastRelativePhrase, int &tmpLastBeginS1, int &tmpLastQ2, int &tmpLastVerb)
{
	for (int J = where + 1; J < wherePrimary; J++)
	{
		if (m[J].pma.queryPattern(u"_REL1") != -1)
			tmpLastRelativePhrase = J;
		if (m[J].pma.queryPattern(u"__S1") != -1)
			tmpLastBeginS1 = J;
		if (m[J].pma.queryPattern(u"_Q2") != -1)
			tmpLastQ2 = J;
		if (m[J].hasVerbRelations)
			tmpLastVerb = J;
	}
}

// resolveObject the primary mention, then reset last* and findLast toward
// the secondary so the next resolve sees the right sentence/quote context.
void cSource::resolvePrimaryMetaNameObject(int where, int wherePrimary, int whereSecondary, int& tmpLastRelativePhrase, int lastRelativePhrase, int& tmpLastBeginS1, int lastBeginS1, int& tmpLastQ2, int lastQ2, int& tmpLastVerb,
	bool inPrimaryQuote, bool inSecondaryQuote)
{
	findLast(where, wherePrimary, tmpLastRelativePhrase, tmpLastBeginS1, tmpLastQ2, tmpLastVerb);
	resolveObject(wherePrimary, false, inPrimaryQuote, inSecondaryQuote, tmpLastBeginS1, tmpLastRelativePhrase, tmpLastQ2, tmpLastVerb, false, false, false);
	tmpLastRelativePhrase = lastRelativePhrase;
	tmpLastBeginS1 = lastBeginS1;
	tmpLastQ2 = lastQ2;
	int tmpLastVerb2;
	findLast(where, whereSecondary, tmpLastRelativePhrase, tmpLastBeginS1, tmpLastQ2, tmpLastVerb2);
}

// Resolve every object in the secondary tag span (stop at a prep unless
// MPLURAL). Marks RE_OBJECT_ROLE so they do not go live. True if any
// secondaryNameObjects were collected.
bool cSource::findAndResolveSecondaryObjects(int& tmpLastRelativePhrase, int& tmpLastBeginS1, int& tmpLastQ2, int& tmpLastVerb, bool inPrimaryQuote, bool inSecondaryQuote, int secondaryTag, vector <cTagLocation>& tagSet, vector <int> &objectsResolved,
 vector <int> &secondaryNameObjects, vector <int> &eraseREObjects)
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
	return secondaryNameObjects.size() > 0;
}

// resolveObject the single secondary mention; push its object (preferring
// the match that is not primaryNameObject) onto secondaryNameObjects.
void cSource::resolveSecondaryMetaNameObject(int whereSecondary,int primaryNameObject,int& tmpLastRelativePhrase, int& tmpLastBeginS1, int& tmpLastQ2, int& tmpLastVerb, bool inPrimaryQuote, bool inSecondaryQuote, vector <int>& objectsResolved,
	vector <int>& secondaryNameObjects, vector <int>& eraseREObjects)
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

// Quoted “I am X” / “you are X”: mark the secondary name as self-referring
// speaker or audience and, before speaker groups exist, insert it into
// tempSpeakerGroup.speakers.
void cSource::insertSecondaryNameObjectInQuoteIntoSpeakers(int where, int wherePrimary, int whereSecondary, vector <int> &secondaryNameObjects, bool inPrimaryQuote)
{
	if (inPrimaryQuote && (m[wherePrimary].word->second.inflectionFlags & (FIRST_PERSON | SECOND_PERSON)) != 0 && secondaryNameObjects.size() == 1 &&
		objects[secondaryNameObjects[0]].objectClass == NAME_OBJECT_CLASS && tempSpeakerGroup.speakers.find(secondaryNameObjects[0]) == tempSpeakerGroup.speakers.end())
	{
		lpwstring tmpstr;
		m[whereSecondary].objectRole |= ((m[wherePrimary].word->second.inflectionFlags & FIRST_PERSON) ? IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE : IN_QUOTE_REFERRING_AUDIENCE_ROLE);
		if (!speakerGroupsEstablished)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:Insert into speaker group (%d,%d) from metaname equivalence:%s", where, wherePrimary, whereSecondary, objectString(secondaryNameObjects[0], tmpstr, true).c_str());
			tempSpeakerGroup.speakers.insert(secondaryNameObjects[0]);
		}
		else if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:Established %d:%s as %s from metaname equivalence", where, whereSecondary, objectString(secondaryNameObjects[0], tmpstr, true).c_str(),
				((m[wherePrimary].word->second.inflectionFlags & FIRST_PERSON) ? u"speaker" : u"audience"));
	}
}

// the Defense Intelligence Agency cryptonym "Curveball"
// Rafid Ahmed Alwan al-Janabi (Arabic: رافد أحمد علوان الجنابي, Rāfid Aḥmad Alwān; born 1968), known by the Defense Intelligence Agency cryptonym "Curveball",[1] is a German citizen who defected from Iraq in 1999, [Wikipedia]
// the overall object is nongen, but the specific referring object is a name
// 1. secondary object is a non gendered general class object.
// 2. secondary object must be more than one word.
// 3. secondary object last word(= whereSecondary) is a capitalized NAME
// 4. primary is not secondary.
// 5. identify secondary.
// The clerk[clerk] --he[mr, clerk] called him[clerk, mr] Brown[brown]
// 12994: he
// 012994 : Metaname equivalence accepted(class change) for secondary 12997 : [Brown[12997 - 12998][12997][gender][M]] to primary 12996 : [him[12996 - 12997][12996][pron][M]]
// But my[tuppence] friends call me[tuppence] Tuppence.”
// 37816 : my
// 037816 : Metaname equivalence accepted(class change) for secondary 37820 : [Tuppence[37820 - 37821][37820][nongen][N][PL]] to primary 37819 : [me[37819 - 37820][37819][pron][M][F]]
/*
int maxLen = 1, element = -1;
if (wherePrimary==whereSecondary-1 &&
	 (objects[secondaryNameObject].begin != whereSecondary && secondaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
	 ((element = m[whereSecondary].pma.queryPattern(u"_NAME", maxLen)) != -1)) || (tagSet[secondaryTag].len == 1 && (m[whereSecondary].flags & cWordMatch::flagFirstLetterCapitalized)) &&
	 (m[wherePrimary].objectMatches.empty() || objects[m[wherePrimary].objectMatches[0].object].begin != whereSecondary) &&
	 (identifyObject(-1, whereSecondary, element, false, -1, -1) >= 0))
{
	secondaryNameObject = objects.size() - 1;
	if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
	{
		lpwstring tmpstr2;
		lplog(LOG_RESOLUTION, u"%06d:Metaname equivalence accepted (class change) for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
		tmpstr.clear();
	}
}
*/

// True if this primary/secondary pair should not be aliased: same object,
// already aliases, ageDetection, wrong class, or different plurality.
bool cSource::rejectSecondaryMetaNameEquivalence(int where, int sno, int wherePrimary, int whereSecondary, int primaryNameObject, vector <int>& objectsResolved,vector <int>& secondaryNameObjects, vector <int>& eraseREObjects)
{
	lpwstring tmpstr;
	int secondaryNameObject = secondaryNameObjects[sno];
	// either primary or secondary don't exist, or primary and secondary are the same, or they are aliases of each other
	int primaryClass = objects[primaryNameObject].objectClass, secondaryClass = objects[secondaryNameObject].objectClass;
	if (primaryNameObject == secondaryNameObject || primaryNameObject < 0 || secondaryNameObject < 0 ||
		find(objects[primaryNameObject].aliases.begin(), objects[primaryNameObject].aliases.end(), secondaryNameObject) != objects[primaryNameObject].aliases.end())
		return true;
	// secondary is an age (The German was 35.)
	if (secondaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS && ageDetection(where, primaryNameObject, secondaryNameObject))
		return true;
	bool primaryAcceptable = primaryClass == NAME_OBJECT_CLASS || primaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || primaryClass == GENDERED_DEMONYM_OBJECT_CLASS || primaryClass == GENDERED_RELATIVE_OBJECT_CLASS || primaryClass == BODY_OBJECT_CLASS;
	bool secondaryAcceptable = secondaryClass == NAME_OBJECT_CLASS || secondaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || secondaryClass == GENDERED_DEMONYM_OBJECT_CLASS || secondaryClass == GENDERED_RELATIVE_OBJECT_CLASS || secondaryClass == BODY_OBJECT_CLASS || secondaryClass == GENDERED_GENERAL_OBJECT_CLASS;
	if ((!primaryAcceptable && !secondaryAcceptable) ||
		(primaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && secondaryClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS) ||
		(primaryClass == GENDERED_DEMONYM_OBJECT_CLASS && secondaryClass == GENDERED_DEMONYM_OBJECT_CLASS) ||
		(primaryClass == GENDERED_RELATIVE_OBJECT_CLASS && secondaryClass == GENDERED_RELATIVE_OBJECT_CLASS) ||
		(primaryClass == BODY_OBJECT_CLASS && secondaryClass == BODY_OBJECT_CLASS) ||
		primaryClass == PRONOUN_OBJECT_CLASS || primaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS || primaryClass == NON_GENDERED_BUSINESS_OBJECT_CLASS ||
		secondaryClass == PRONOUN_OBJECT_CLASS || secondaryClass == NON_GENDERED_GENERAL_OBJECT_CLASS || secondaryClass == NON_GENDERED_BUSINESS_OBJECT_CLASS ||
		(objects[primaryNameObject].plural != objects[secondaryNameObject].plural))
	{
		int inflectionFlags = m[wherePrimary].word->second.inflectionFlags;
		if ((((inflectionFlags & MALE_GENDER) == MALE_GENDER) ^ ((inflectionFlags & FEMALE_GENDER) == FEMALE_GENDER)) && m[wherePrimary].objectMatches.size() > 1)
		{
			lpwstring tmpstr2;
			lplog(LOG_RESOLUTION, u"%06d:Metaname equivalence rejected (uncertain) for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
			return true;
		}
		{
			if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
			{
				lpwstring tmpstr2;
				lplog(LOG_RESOLUTION, u"%06d:Metaname equivalence rejected (wrong class) for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
			}
			if (eraseREObjects[sno] != -1)
				m[eraseREObjects[sno]].objectRole &= ~RE_OBJECT_ROLE;
			for (unsigned int J = 0; J < objectsResolved.size(); J++)
				m[objectsResolved[J]].flags &= ~cWordMatch::flagObjectResolved;
			return true;
		}
	}
	// Note: a primary/secondary plurality mismatch is now caught by the
	// plurality clause in the class-check above (it used to compare
	// objects[secondaryNameObject].plural to itself, a tautology, so this
	// used to be the only place a plurality mismatch was actually rejected).
	// somebody named Jane Finn should be equivocated to Jane Finn
	//if (overlaps(objects.begin()+primaryNameObject,objects.begin()+secondaryNameObject))
	//{
	//	if (t.traceNameResolution || t.traceObjectResolution)
	//	{
	//		lpwstring tmpstr,tmpstr2;
	//		lplog(LOG_RESOLUTION,u"%06d:Metaname equivalence rejected (overlaps) for secondary %d:[%s] to primary %d:[%s]",where,whereSecondary,objectString(secondaryNameObject,tmpstr,false).c_str(),wherePrimary,objectString(primaryNameObject,tmpstr2,false).c_str());
	//	}
	//	return true;
	//}
	return false;
}

// Swap so the NAME / META_GROUP side is primary when the other side is a
// weaker class (occupation, body, demonym, relative, general).
void cSource::switchToPreferPrimaryNameOrMetaGroup(int &wherePrimary, int &primaryNameObject, int &whereSecondary, int &secondaryNameObject)
{
	int primaryClass = objects[primaryNameObject].objectClass, secondaryClass = objects[secondaryNameObject].objectClass;
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
}

// Mark the primary span META_NAME_EQUIVALENCE and clear HAIL_ROLE (the
// “name of Rita” is not a hail).
void cSource::removeHail(int where, int wherePrimary)
{
	for (vector <cWordMatch>::iterator im = m.begin() + m[wherePrimary].beginObjectPosition, imEnd = m.begin() + m[wherePrimary].endObjectPosition; im != imEnd; im++)
	{
		im->objectRole |= META_NAME_EQUIVALENCE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE, u"%06d:Removed HAIL role (evaluateMetaNameEquivalence).", where);
		// prevents HAIL re-evaluation on mistaken HAIL 
		im->objectRole &= ~HAIL_ROLE; // [tommy:tuppence] Are you[tuppence] proposing a third advertisement : Wanted , female crook[tuppence] , answering to the name[name] of Rita ? ”
	}
}

// Copy associatedNouns / adjectives and exclusive gender from secondary
// onto primary; log when tracing.
void cSource::associateSecondaryAdjectivesAndGenderToPrimary(int where, int wherePrimary, int primaryNameObject, int whereSecondary, int secondaryNameObject)
{
	lpwstring tmpstr;
	if ((objects[secondaryNameObject].associatedNouns.size() || objects[secondaryNameObject].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
	{
		lpwstring nouns, adjectives;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:Object %s original associated nouns (%s) and adjectives (%s) taking from %d:%s (4)",
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
		objects[primaryNameObject].updateGenericGender(where, m[whereSecondary].word, objects[secondaryNameObject].objectGenericAge, u"metaNameEquivalence", debugTrace);
	if ((objects[secondaryNameObject].associatedNouns.size() || objects[secondaryNameObject].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
	{
		lpwstring nouns, adjectives;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:Object %s associated nouns (%s) and adjectives (%s) (3)",
				where, objectString(primaryNameObject, tmpstr, false).c_str(), wordString(objects[secondaryNameObject].associatedNouns, nouns).c_str(), wordString(objects[secondaryNameObject].associatedAdjectives, adjectives).c_str());
	}
	if (objects[primaryNameObject].relativeClausePM < 0 && objects[secondaryNameObject].relativeClausePM >= 0)
	{
		objects[primaryNameObject].relativeClausePM = objects[secondaryNameObject].relativeClausePM;
		objects[primaryNameObject].whereRelativeClause = objects[secondaryNameObject].whereRelativeClause;
		objects[secondaryNameObject].relativeClausePM = objects[secondaryNameObject].whereRelativeClause = -1; // to prevent this object from becoming more visible to focus
	}
}

// True if either side is still an unidentified / unknown-gender pronoun
// that we should not lock into an alias yet.
bool cSource::refuseIdentificationIfNotIdentifiedOrNotKnown(int where, int wherePrimary, int primaryNameObject, int whereSecondary, int secondaryNameObject)
{
	// The man known as Number One
	if (currentSpeakerGroup >= 2)
	{
		int lastSpeakerGroupEnd = (speakerGroupsEstablished) ? speakerGroups[currentSpeakerGroup - 1].sgEnd : speakerGroups[currentSpeakerGroup - 2].sgEnd;
		int primaryClass = objects[primaryNameObject].objectClass, secondaryClass = objects[secondaryNameObject].objectClass;
		if ((primaryClass == NAME_OBJECT_CLASS || primaryClass == GENDERED_GENERAL_OBJECT_CLASS) && secondaryClass == NAME_OBJECT_CLASS &&
			(m[wherePrimary].objectMatches.size() || m[whereSecondary].objectMatches.size()) && currentSpeakerGroup &&
			objects[primaryNameObject].firstLocation < lastSpeakerGroupEnd &&
			objects[secondaryNameObject].firstLocation < lastSpeakerGroupEnd)
		{
			lpwstring tmpstr,tmpstr2;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:meta resolution refused between %d:%s and %d:%s (%d)!", where,
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
			return true;
		}
	}
	return false;
}

// Drop a mistaken place subtype, copy gender, and replaceObject the
// primary mention with the secondary name (the usual “X, known as Y” write).
void cSource::removePlaceSetGenderAndReplacePrimaryWithSecondary(int where, bool inPrimaryQuote, bool inSecondaryQuote, int wherePrimary, int primaryNameObject, int whereSecondary, int secondaryNameObject)
{
	lpwstring tmpstr;
	int primaryClass = objects[primaryNameObject].objectClass;
	if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
	{
		lpwstring tmpstr2;
		lplog(LOG_RESOLUTION, u"%06d:Metaname replacement detected for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
	}
	objects[secondaryNameObject].isNotAPlace = objects[primaryNameObject].isNotAPlace = true;
	if (objects[secondaryNameObject].getSubType() >= 0)
	{
		objects[secondaryNameObject].resetSubType();
		objects[secondaryNameObject].isNotAPlace = true;
		if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
			lplog(LOG_RESOLUTION, u"%06d:Removing place designation (2) from object %s.", where, objectString(secondaryNameObject, tmpstr, false).c_str());
	}
	if (objects[primaryNameObject].getSubType() >= 0)
	{
		if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[secondaryNameObject].getSubType() >= 0)
			lplog(LOG_RESOLUTION, u"%06d:Removing place designation (3) from object %s.", where, objectString(primaryNameObject, tmpstr, false).c_str());
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
			lpwstring tmpstr2;
			lplog(LOG_RESOLUTION, u"%06d:Match %s becomes %s from object %s (1).", where,
				objectString(objects.begin() + secondaryNameObject, tmpstr, false).c_str(), (objects[secondaryNameObject].male) ? u"male" : u"female",
				objectString(objects.begin() + primaryNameObject, tmpstr2, false).c_str());
		}
		if (ambiguousGender)
			addDefaultGenderedAssociatedNouns(secondaryNameObject);
		if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[secondaryNameObject].getSubType() >= 0)
		{
			lplog(LOG_RESOLUTION, u"%06d:Removing place designation (4) from object %s.", where, objectString(secondaryNameObject, tmpstr, false).c_str());
		}
	}
	if (primaryClass != BODY_OBJECT_CLASS)
		replaceObjectInSection(where, secondaryNameObject, primaryNameObject, u"metaname");
	else
	{
		vector <cLocalFocus>::iterator lsi;
		if (pushObjectIntoLocalFocus(whereSecondary, secondaryNameObject, false, false, inPrimaryQuote, inSecondaryQuote, u"metaname", lsi))
			pushLocalObjectOntoMatches(wherePrimary, lsi, u"metaname");
	}
}

// After aliasing, share gender, adjectives, and relative-clause attachments
// both ways so later resolution sees one person.
void cSource::equalizeGenderAdjectivesAndRelativeClauses(int where, int primaryNameObject, int secondaryNameObject)
{
	equivocateObjects(where, primaryNameObject, secondaryNameObject);
	equivocateObjects(where, secondaryNameObject, primaryNameObject);
	// don't move adjectives to a gendered object with only two words - 'a woman'
	// otherwise 'a woman' gets to be a more significant object than the other one, which would be more descriptive 'the nurse'
	// 22284:Metaname equivalence detected for 
	//   secondary 22290:[an Irish Sinn feiner[22288-22291][22290][gendem][M][OBJECT][RE][NONPRESENT][FOCUS_EVALUATED]] to 
	//   primary 22287:[The man[21946-21948][21947][gender][M][SUBJECT][IS][NONPRESENT][FOCUS_EVALUATED][who came up the staircase with a furtive , soft - footed tread[21948-21961]]]
	if (objects[primaryNameObject].objectClass != GENDERED_GENERAL_OBJECT_CLASS || objects[primaryNameObject].end - objects[primaryNameObject].begin > 2 ||
		objects[primaryNameObject].relativeClausePM >= 0)
		moveNyms(where, primaryNameObject, secondaryNameObject, u"evaluateMetaNameEquivalence primary->secondary");
	if (objects[secondaryNameObject].objectClass != GENDERED_GENERAL_OBJECT_CLASS || objects[secondaryNameObject].end - objects[secondaryNameObject].begin > 2 ||
		objects[secondaryNameObject].relativeClausePM >= 0)
		moveNyms(where, secondaryNameObject, primaryNameObject, u"evaluateMetaNameEquivalence secondary->primary");
	if (objects[primaryNameObject].relativeClausePM < 0 && objects[secondaryNameObject].relativeClausePM >= 0)
	{
		objects[primaryNameObject].relativeClausePM = objects[secondaryNameObject].relativeClausePM;
		objects[primaryNameObject].whereRelativeClause = objects[secondaryNameObject].whereRelativeClause;
		objects[secondaryNameObject].relativeClausePM = objects[secondaryNameObject].whereRelativeClause = -1; // to prevent this object from becoming more visible to focus
	}
}

// One secondary of a meta-name hit: reject, maybe swap sides, copy
// adjectives/gender, replaceObject, strip hail. False if refused.
bool cSource::evaluateSecondaryMetaNameEquivalence(int where, vector <cTagLocation>& tagSet, bool inPrimaryQuote, bool inSecondaryQuote, int sno, int wherePrimary, int whereSecondary, int primaryNameObject, int secondaryTag,  vector <int>& objectsResolved,
	vector <int>& secondaryNameObjects, vector <int>& eraseREObjects)
{
	if (rejectSecondaryMetaNameEquivalence(where, sno, wherePrimary, whereSecondary, primaryNameObject, objectsResolved, secondaryNameObjects, eraseREObjects))
		return false;
	lpwstring tmpstr;
	int secondaryNameObject = secondaryNameObjects[sno];
	switchToPreferPrimaryNameOrMetaGroup(wherePrimary, primaryNameObject, whereSecondary, secondaryNameObject);
	int primaryClass = objects[primaryNameObject].objectClass, secondaryClass = objects[secondaryNameObject].objectClass;
	removeHail(where, wherePrimary);
	//  he[man] nevertheless conveyed the impression of a big man
	if (primaryClass == GENDERED_GENERAL_OBJECT_CLASS && secondaryClass == GENDERED_GENERAL_OBJECT_CLASS)
	{
		associateSecondaryAdjectivesAndGenderToPrimary(where, wherePrimary, primaryNameObject, whereSecondary, secondaryNameObject);
		return true;
	}
	// (only in resolve) do not arbitrarily assign a name to another if they are not positively absolutely identified and both previously existed
	if (refuseIdentificationIfNotIdentifiedOrNotKnown(where, wherePrimary, primaryNameObject, whereSecondary, secondaryNameObject))
		return true;
	bool atLeastOneSecondarySucceeded = true;
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
			lpwstring tmpstr2;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:%s gains match of %s.", wherePrimary,
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
		removePlaceSetGenderAndReplacePrimaryWithSecondary(where, inPrimaryQuote, inSecondaryQuote, wherePrimary, primaryNameObject, whereSecondary, secondaryNameObject);
		return atLeastOneSecondarySucceeded;
	}
	removeHail(where, wherePrimary);
	// scan all future speakerGroups and see whether they have both the 'from' and the 'to'
	for (unsigned int sg = currentSpeakerGroup; sg < speakerGroups.size(); sg++)
		if (speakerGroups[sg].speakers.find(primaryNameObject) != speakerGroups[sg].speakers.end() && speakerGroups[sg].speakers.find(secondaryNameObject) != speakerGroups[sg].speakers.end())
		{
			speakerGroups[sg].speakers.erase(secondaryNameObject);
			if (debugTrace.traceSpeakerResolution)
			{
				lpwstring tmpstr2;
				lplog(LOG_RESOLUTION, u"%06d:Alias %s erased from %s.", where, objectString(secondaryNameObject, tmpstr, true).c_str(), toText(speakerGroups[sg], tmpstr2));
			}
		}
	equalizeGenderAdjectivesAndRelativeClauses(where, primaryNameObject, secondaryNameObject);
	if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
	{
		lpwstring tmpstr2;
		lplog(LOG_RESOLUTION, u"%06d:Metaname equivalence detected for secondary %d:[%s] to primary %d:[%s]", where, whereSecondary, objectString(secondaryNameObject, tmpstr, false).c_str(), wherePrimary, objectString(primaryNameObject, tmpstr2, false).c_str());
	}
	return atLeastOneSecondarySucceeded;
}

// in secondary quotes, inPrimaryQuote=false
// “X known as / called / named Y”: resolve both sides, then
// evaluateSecondaryMetaNameEquivalence for each secondary. True if any pair
// was accepted.
bool cSource::evaluateMetaNameEquivalence(int where, vector <cTagLocation>& tagSet, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
	int primaryTag = findOneTag(tagSet, u"NAME_PRIMARY", -1), secondaryTag = findOneTag(tagSet, u"NAME_SECONDARY", -1);
	if (primaryTag < 0 || secondaryTag < 0) return false;
	int wherePrimary = tagSet[primaryTag].sourcePosition, whereSecondary = tagSet[secondaryTag].sourcePosition;
	if (tagSet[primaryTag].len > 1 && m[wherePrimary].principalWherePosition >= 0) // could be an adjective
		wherePrimary = m[wherePrimary].principalWherePosition;
	lpwstring tmpstr;
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
	if (primaryIsMNoun(where,wherePrimary,tagSet[primaryTag]))
		return false;
	int tmpLastRelativePhrase = lastRelativePhrase, tmpLastBeginS1 = lastBeginS1, tmpLastQ2 = lastQ2, tmpLastVerb = lastVerb;
	resolvePrimaryMetaNameObject(where, wherePrimary, whereSecondary, tmpLastRelativePhrase, lastRelativePhrase, tmpLastBeginS1, lastBeginS1, tmpLastQ2, lastQ2, tmpLastVerb, inPrimaryQuote, inSecondaryQuote);
	int primaryNameObject = m[wherePrimary].getObject();
	if (m[wherePrimary].objectMatches.size() == 1)
		primaryNameObject = m[wherePrimary].objectMatches[0].object;
	if (primaryNameObject < 0)
	{
		m[wherePrimary].flags &= ~cWordMatch::flagObjectResolved;
		return false;
	}
	vector <int> objectsResolved;
	objectsResolved.push_back(wherePrimary);
	vector <int> secondaryNameObjects, eraseREObjects;
	if (whereSecondary < 0 || scanForMultiple)
	{
		// whether to put him[man] down as an actor or a lawyer 
		if (!findAndResolveSecondaryObjects(tmpLastRelativePhrase, tmpLastBeginS1, tmpLastQ2, tmpLastVerb, inPrimaryQuote, inSecondaryQuote, secondaryTag, tagSet, objectsResolved,
				secondaryNameObjects, eraseREObjects))
			return false;
	}
	else
	{
		resolveSecondaryMetaNameObject(whereSecondary, primaryNameObject, tmpLastRelativePhrase, tmpLastBeginS1, tmpLastQ2, tmpLastVerb, inPrimaryQuote, inSecondaryQuote, objectsResolved,
			secondaryNameObjects, eraseREObjects);
	}
	// He[boris] gave his[boris] name as Count Stepanov . 
	// if there are two primary matches, and one secondary match, and the secondary match matches one of the primary matches, switch the primaryNameObeject to the other match.
	if (m[wherePrimary].objectMatches.size() == 2 && secondaryNameObjects.size() == 1 && in(secondaryNameObjects[0], m[wherePrimary].objectMatches) != m[wherePrimary].objectMatches.end())
		primaryNameObject = (m[wherePrimary].objectMatches[0].object == secondaryNameObjects[0]) ? m[wherePrimary].objectMatches[1].object : m[wherePrimary].objectMatches[0].object;
	insertSecondaryNameObjectInQuoteIntoSpeakers(where, wherePrimary, whereSecondary, secondaryNameObjects, inPrimaryQuote);
	if (inPrimaryQuote && (m[wherePrimary].word->second.inflectionFlags & THIRD_PERSON) && secondaryNameObjects.size() == 1 &&
		objects[secondaryNameObjects[0]].objectClass == NAME_OBJECT_CLASS && tempSpeakerGroup.speakers.find(secondaryNameObjects[0]) == tempSpeakerGroup.speakers.end() && !speakerGroupsEstablished)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:Reject from speaker group (%d,%d) from third-person metaname equivalence:%s", where, wherePrimary, whereSecondary, objectString(secondaryNameObjects[0], tmpstr, true).c_str());
		metaNameOthersInSpeakerGroups.push_back(whereSecondary);
	}
	bool atLeastOneSecondarySucceeded = false;
	for (unsigned int sno = 0; sno < secondaryNameObjects.size(); sno++)
	{
		atLeastOneSecondarySucceeded|=evaluateSecondaryMetaNameEquivalence(where, tagSet, inPrimaryQuote, inSecondaryQuote, sno, wherePrimary, whereSecondary, primaryNameObject, secondaryTag, objectsResolved,
			secondaryNameObjects, eraseREObjects);
	}
	return atLeastOneSecondarySucceeded;
}

// in secondary quotes, inPrimaryQuote=false
// _META_NAME_EQUIVALENCE at `where` -> collect tag-sets and
// evaluateMetaNameEquivalence. True on the first accepted set.
bool cSource::identifyMetaNameEquivalence(int where, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
		int element, startAt = 0; // nameEnd=-1,
	while ((element = m[where].pma.queryAllPattern(u"_META_NAME_EQUIVALENCE", startAt)) != -1)
	{
		vector < vector <cTagLocation> > tagSets;
		// obeyBlock must be false because of _META_NAME_EQUIVALENCE[8]
		if (startCollectTags(true, metaNameEquivalenceTagSet, where, m[where].pma[element].pemaByPatternEnd, tagSets, false, true, u"name equivalence") > 0)
			for (unsigned int J = 0; J < tagSets.size(); J++)
			{
				if (debugTrace.traceNameResolution)
					printTagSet(LOG_RESOLUTION, u"MNE", J, tagSets[J], where, m[where].pma[element].pemaByPatternEnd);
				if (evaluateMetaNameEquivalence(where, tagSets[J], inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb))
					return true;
			}
		startAt = element + 1;
	}
	return false;
}

// _META_SPEAKER tag-set: the quoted “I / you” is this name. Marks
// IN_QUOTE_SELF_REFERRING / AUDIENCE and inserts into tempSpeakerGroup.
bool cSource::evaluateMetaSpeaker(int where, vector <cTagLocation>& tagSet)
{
	LFS
		int primaryTag, secondaryTag = findOneTag(tagSet, u"NAME_SECONDARY", -1);
	unsigned int wherePrimary = tagSet[primaryTag = findOneTag(tagSet, u"NAME_PRIMARY", -1)].sourcePosition;
	int whereSecondary = (secondaryTag >= 0) ? tagSet[secondaryTag].sourcePosition : -1;
	bool isAudience = (whereSecondary >= 0 && (m[whereSecondary].word->second.inflectionFlags & SECOND_PERSON) == SECOND_PERSON);
	if (tagSet[primaryTag].len > 1 && m[wherePrimary].principalWherePosition >= 0) // make sure to bypass any adjectives
		wherePrimary = m[wherePrimary].principalWherePosition;
	m[wherePrimary].objectRole |= (isAudience) ? IN_QUOTE_REFERRING_AUDIENCE_ROLE : IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE;
	if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
	{
		lpwstring tmpstr2;
		lplog(LOG_RESOLUTION, u"%06d:Meta %s detected for %d:[%s]", where, (isAudience) ? u"audience" : u"speaker", wherePrimary, objectString(m[wherePrimary].getObject(), tmpstr2, false).c_str());
	}
	return true;
}

// _META_SPEAKER at `where` (always in-quote). True if evaluateMetaSpeaker
// accepted a tag-set.
bool cSource::identifyMetaSpeaker(int where, bool inQuote)
{
	LFS
		if (!inQuote) return false; // these patterns only apply in a quote
	int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(u"_META_SPEAKER", nameEnd)) == -1) return false;
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(true, metaNameEquivalenceTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, u"meta speaker identification") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, u"MS", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateMetaSpeaker(where, tagSets[J]))
				return true;
		}
	return false;
}

// “announced himself as X” / similar: treat the name as a speaker
// identification for the subject.
bool cSource::evaluateAnnounce(int where, vector <cTagLocation>& tagSet)
{
	LFS
		int primaryTag;
	unsigned int wherePrimary = tagSet[primaryTag = findOneTag(tagSet, u"NAME_PRIMARY", -1)].sourcePosition;
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
			lpwstring tmpstr2;
			lplog(LOG_RESOLUTION, u"%06d:Meta announce detected for %d:[%s]", where, wherePrimary, objectString(m[wherePrimary].getObject(), tmpstr2, false).c_str());
		}
	}
	return true;
}

// Here is Bob!  / Here is another knock.
// _ANNOUNCE pattern at `where`. True if evaluateAnnounce accepted a set.
bool cSource::identifyAnnounce(int where, bool inQuote)
{
	LFS
		if (!inQuote) return false; // these patterns only apply in a quote
	int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(u"_META_ANNOUNCE", nameEnd)) == -1) return false;
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(true, metaSpeakerTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, u"meta announce") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, u"MA", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateAnnounce(where, tagSets[J]))
				return true;
		}
	return false;
}

// “X and Y” / “X accompanied Y”: resolve both names and record a
// META_GROUP membership / alias so later pronouns can hit the pair.
bool cSource::evaluateMetaGroup(int where, vector <cTagLocation>& tagSet, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
		int primaryTag, secondaryTag;
	int wherePrimary = tagSet[primaryTag = findOneTag(tagSet, u"NAME_PRIMARY", -1)].sourcePosition;
	int whereSecondary = tagSet[secondaryTag = findOneTag(tagSet, u"NAME_SECONDARY", -1)].sourcePosition;
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
			lpwstring tmpstr2, tmpstr3;
			lplog(LOG_RESOLUTION, u"%06d:Meta group detected for %d:%s PP[%s] and %d:%s PP[%s]", where,
				wherePrimary, whereString(wherePrimary, tmpstr2, false).c_str(), (plsi != localObjects.end() && plsi->physicallyPresent) ? u"true" : u"false",
				whereSecondary, whereString(whereSecondary, tmpstr3, false).c_str(), (slsi != localObjects.end() && slsi->physicallyPresent) ? u"true" : u"false");
		}
		if (plsi != localObjects.end() && slsi != localObjects.end())
		{
			lpwstring tmpstr2, tmpstr3;
			if (plsi->physicallyPresent && !slsi->physicallyPresent)
			{
				lplog(LOG_RESOLUTION, u"%06d: %d:[%s] made %d:%s physically present", where, wherePrimary, whereString(wherePrimary, tmpstr2, false).c_str(), whereSecondary, whereString(whereSecondary, tmpstr3, false).c_str());
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
				lplog(LOG_RESOLUTION, u"%06d: %d:[%s] made %d:%s physically present", where, whereSecondary, whereString(whereSecondary, tmpstr3, false).c_str(), wherePrimary, whereString(wherePrimary, tmpstr2, false).c_str());
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
// _META_GROUP at `where` -> evaluateMetaGroup on collected tag-sets.
bool cSource::identifyMetaGroup(int where, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb)
{
	LFS
		if (inPrimaryQuote || inSecondaryQuote) return false; // these patterns only apply to speakers
	int element, nameEnd = -1;
	if ((element = m[where].pma.queryPattern(u"_META_GROUP", nameEnd)) == -1) return false;
	vector < vector <cTagLocation> > tagSets;
	if (startCollectTags(true, metaNameEquivalenceTagSet, where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, u"meta group identification") > 0)
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceNameResolution)
				printTagSet(LOG_RESOLUTION, u"MG", J, tagSets[J], where, m[where].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd);
			if (evaluateMetaGroup(where, tagSets[J], lastBeginS1, lastRelativePhrase, lastQ2, lastVerb))
				return true;
		}
	return false;
}

// this is so that Doctor Hall matches Dr. Hall, and matches doc.  
// If doc is introduced first (or doctor, or reverend) and then Reverend Holland is introduced, they should be related
// also Hasbro Co. should match Hasbro Company
struct {
	const lpchar_t* abbreviation;
	const lpchar_t* full;
} abbreviationWordMapList[] =
// honorifics
{
	{ u"dr",u"doc" },
	{ u"doc",u"doctor" },
	{ u"doctor",u"dr" },
	{ u"st",u"saint" },
	{ u"mr",u"mister" },
	{ u"m",u"mister" },
	{ u"mrs",u"missus" },
	{ u"rev",u"reverend" },
	{ u"ms",u"miz" },
	{ u"ms",u"miss" },
	{ u"prof",u"professor" },
	//measurement_abbreviation
		{u"lbs",u"pounds"},
		{u"mo",u"month"},
		{u"mos",u"months"},
		{u"cm",u"centimeter"},
		{u"kg",u"kilogram"},
		{u"km",u"kilometer"},
		{u"kw",u"kilowatt"},
		{u"lb",u"pound"},
		{u"ft",u"foot"},
		{u"oz",u"ounce"},
		{u"in",u"inch"},
		{u"mg",u"milligram"},
		{u"ml",u"milliliter"},
		{u"mm",u"millimeter"},
		{u"tbsp",u"tablespoon"},
		{u"tsp",u"teaspoon"},
		// street_address_abbreviation
			{u"st",u"street"},
			{u"av",u"avenue"},
			{u"ave",u"avenue"},
			{u"dr",u"drive"},
			{u"rd",u"road"},
			{u"pk",u"pike"}, // streets
		// business_abbreviation
			{u"inc",u"incorporated"},
			{u"ltd",u"limited"},
			{u"corp",u"corporation"},
			{u"co",u"company"},
			{NULL,0} };
struct wordMapCompare
{
	bool operator()(const tIWMM& lhs, const tIWMM& rhs) const
	{
		return lhs->first < rhs->first;
	}
};

map <tIWMM, vector <tIWMM>, cSource::wordMapCompare> abbreviationMap;
// Lazy-fill abbreviationMap from abbreviationWordMapList (Dr/doc, Co/
// Company, St/street|saint). No-op if already built.
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

// Push w and every abbreviation equivalent onto nouns (unique).
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

// For a NAME, add hon/hon2/hon3 equivalents to associatedNouns; for an
// occupation, add the principal word (“doc” <-> “doctor”).
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

// True if w == w2 or w2 is in abbreviationMap[w] (Dr ~ doctor).
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

// Objects whose span shares a word (or abbreviation equivalent) with
// `object`. True if relatedObjects is non-empty.
bool cSource::accumulateRelatedObjects(int object, set <int>& relatedObjects)
{
	LFS
		int begin = objects[object].begin, end = objects[object].end;
	buildMap();
	for (int I = begin; I < end; I++)
	{
		tIWMM w = m[I].word;
		lpwstring tmpstr, tmpstr2;
		relatedObjects.insert(relatedObjectsMap[w].begin(), relatedObjectsMap[w].end());
		map <tIWMM, vector<tIWMM>, cSource::wordMapCompare>::iterator ami = abbreviationMap.find(w);
		if (ami != abbreviationMap.end())
			for (vector <tIWMM>::iterator wi = ami->second.begin(), wiEnd = ami->second.end(); wi != wiEnd; wi++)
				relatedObjects.insert(relatedObjectsMap[*wi].begin(), relatedObjectsMap[*wi].end());
	}
	return relatedObjects.size() > 0;
}

// Detect Janet+Rita sharing a last name (firstNameAmbiguous) or the same
// first with different lasts (lastNameAmbiguous). Also tracks an `any`
// that matches multiple firsts/lasts.
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
		lpwstring tmpstr, tmpstr2;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%s ambiguous with %s [%s,%d,%s,%s,%s].", objectString(object, tmpstr, false).c_str(), objectString(o, tmpstr2, false).c_str(),
				(ambiguousFirst == wNULL) ? u"" : ambiguousFirst->first.c_str(), ambiguousNickName, (ambiguousLast == wNULL) ? u"" : ambiguousLast->first.c_str(),
				(firstNameAmbiguous) ? u"firstNameAmbiguous" : u"", (lastNameAmbiguous) ? u"lastNameAmbiguous" : u"");

	}
}

// Global search: every related NAME / occupation that like()/nameMatch
// this object. May recursively resolveNameObject an unresolved earlier
// mention (identifySpeakerGroups phase). Fills matchingObjects.
void cSource::matchRelatedObjects(const int where, vector <cObject>::iterator &object, const int forwardCallingObject, const int objectToBeReplaced,
	bool &firstNameAmbiguous, bool &lastNameAmbiguous, tIWMM &ambiguousFirst, tIWMM &ambiguousLast, int &ambiguousNickName,
	set <int> &matchingObjects)
{
	lpwstring tmpstr, tmpstr2;
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
							lplog(LOG_RESOLUTION, u"%06d:Object %s (%d,%d) related to object %s?", where, objectString(rObject, tmpstr, false).c_str(), rObject->originalLocation, lastSpeakerGroupEnd, objectString(object, tmpstr2, false).c_str());
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
}

// For one matching object: replaceObject (occupation <-> name) or push a
// cOM, unless globallyAmbiguous (Janet vs Rita) or the honorific is pinr.
void cSource::pushIntoObjectMatchesOrReplace(const int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches, const set <int>::iterator mo,
	const bool firstNameAmbiguous, const bool lastNameAmbiguous, const bool qualified, const bool globalSearch)
{
	bool unambiguousGenderFound;
	lpwstring tmpstr, tmpstr2;
	if (objects[*mo].eliminated) 
		return;
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
				lplog(LOG_RESOLUTION, u"%06d:unresolvable occupation %s matches but original location occurs after %s (%d>%d) or before current speaker group",
					where, objectString(*mo, tmpstr, true).c_str(), objectString(object, tmpstr2, true).c_str(),
					mObject->originalLocation, object->originalLocation);
			return;
		}
		if (object->objectClass == NAME_OBJECT_CLASS && !object->name.justHonorific())
		{
			vector <cLocalFocus>::iterator lsi;
			if ((lsi = in(*mo)) == localObjects.end())
			{
				if (debugTrace.traceNameResolution)
					lplog(LOG_RESOLUTION, u"%06d:matching object %s is not in local salience (resolveNameWithOccupationObject) - rejected.",
						where, objectString(*mo, tmpstr, true).c_str());
			}
			else
				replaceObjectWithObject(atBefore(*mo, where), mObject, o, u"resolveNameWithOccupationObject");
		}
		else
		{
			if (debugTrace.traceNameResolution)
				lplog(LOG_RESOLUTION, u"%06d:owned occupation %s matches but does not replace occupation %s", where, objectString(*mo, tmpstr, true).c_str(), objectString(object, tmpstr2, true).c_str());
			objectMatches.push_back(cOM(*mo, SALIENCE_THRESHOLD));
		}
		return;
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
		(!objects[*mo].name.justHonorific() || objects[*mo].name.hon->second.query(u"pinr") < 0))
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
				lplog(LOG_RESOLUTION, u"%06d:matching %s: globally ambiguous name %s [%s,%s]",
					where, objectString(object, tmpstr, true).c_str(), objectString(*mo, tmpstr2, true).c_str(),
					(firstNameOnly) ? u"firstNameOnly" : u"", (lastNameOnly) ? u"lastNameOnly" : u"");
		}
		// object or matching object is composed of only one component which is ambiguous
		// prefer unowned objects
		// 'Porsche' should be matched with 'her Porsche' - but not replaced.
		// if not in local and number of parts don't match 
		const lpchar_t* reason = NULL;
		if ((objects[*mo].getOwnerWhere() != -1 && object->getOwnerWhere() == -1)) reason = u"preferUnOwnedObjects";
		if (object->name.justHonorific()) reason = u"justHonorific";
		if (!object->matchGenderIncludingNeuter(objects[*mo], unambiguousGenderFound)) reason = u"genderConflict";
		if (globallyAmbiguous) reason = u"globallyAmbiguous";
		if (reason != NULL)
		{
			tmpstr.clear();
			tmpstr2.clear();
			if (debugTrace.traceNameResolution)
				lplog(LOG_RESOLUTION, u"%06d:owned name %s matches but does not replace name %s [%s]", where, objectString(*mo, tmpstr, true).c_str(), objectString(object, tmpstr2, true).c_str(), reason);
			objectMatches.push_back(cOM(*mo, SALIENCE_THRESHOLD));
		}
		else
		{
			replaceObjectWithObject(where, object, *mo, u"resolveNameObject");
			object = objects.begin() + *mo;
		}
	}
}

// followObjectChain eliminated members; drop them, re-insert the live
// replacement, and restart the scan if anything was revived.
void cSource::removeEliminatedObjects(set <int> &matchingObjects)
{
	// maxOccurrence = max(maxOccurrence, objects[*mo].numEncounters + objects[*mo].numIdentifiedAsSpeaker);
	// minOccurrence = min(minOccurrence, objects[*mo].numEncounters + objects[*mo].numIdentifiedAsSpeaker);
	for (set<int>::iterator mo = matchingObjects.begin(), moEnd = matchingObjects.end(); mo != moEnd; )
	{
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
}

// If local-focus lsi nameMatch / honorific-equivalent-matches object, add
// it to matchingObjects; else salienceFactor = -1.
void cSource::matchLocalObjectWithNameObject(vector <cLocalFocus>::iterator lsi, vector <cObject>::iterator& object, 
	const int objectToBeReplaced, bool &unambiguousGenderFound,
	bool &firstNameAmbiguous, bool &lastNameAmbiguous, tIWMM& ambiguousFirst, tIWMM& ambiguousLast, int& ambiguousNickName,
	set <int> &matchingObjects)
{
	if (lsi->om.object <= 1) return;
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

// Coreference a NAME mention: skip pinr honorifics, eliminate embedded-dash
// “Brown -- Julius” parses, then match local focus, then (if empty) hail
// speakers, then a global related-objects search. Pushes matches or
// replaceObjects. True if at least one match (or the object was eliminated).
// merge current object onto matched object.  change all references (m and ls)
//                       of the current object to the new object.
//    if a name is not mentioned definitively as a speaker, try to match it to a speaker.
bool cSource::resolveNameObject(int where, vector <cObject>::iterator& object, vector <cOM>& objectMatches, int forwardCallingObject)
{
	LFS
		// don't match 'sir' to anything using this 'relatedObjects' kind of resolution
		if (object->name.justHonorific() && object->name.hon->second.query(u"pinr") >= 0) return false;
	// Supposing Mr . Brown -- Julius -- was there waiting
	// detect a name with an embedded --
	int embeddedDash = -1;
	for (int I = where; I < m[where].endObjectPosition - 1; I++)
		if (m[I].word->first == u"--")
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
		lpwstring tmpstr;
		if (debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:Could not resolve %d-%d:%s - embedded dash@%d, possible combination of two different names [last=%s] - marking as eliminated", where,
				m[where].beginObjectPosition, m[where].endObjectPosition, objectString(object, tmpstr, false).c_str(), embeddedDash, (object->name.last != wNULL) ? object->name.last->first.c_str() : u"NULL");
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
	lpwstring tmpstr, tmpstr2;
	bool unambiguousGenderFound;
	for (; lsi != lsiEnd; lsi++)
		matchLocalObjectWithNameObject(lsi, object, objectToBeReplaced, unambiguousGenderFound, firstNameAmbiguous, lastNameAmbiguous, ambiguousFirst, ambiguousLast, ambiguousNickName, matchingObjects);

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
		matchRelatedObjects(where, object, forwardCallingObject, objectToBeReplaced,
			firstNameAmbiguous, lastNameAmbiguous, ambiguousFirst, ambiguousLast, ambiguousNickName, matchingObjects);
		if (object->eliminated)
		{
			if (debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:Object %s eliminated during prior object resolution", where, objectString(object, tmpstr, false).c_str());
			return true;
		}
		if (matchingObjects.empty())
		{
			if (debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:Could not resolve %s", where, objectString(object, tmpstr, false).c_str());
			return false;
		}
	}
	// sort by nearness of last mention?
	if (globalSearch)
		removeEliminatedObjects(matchingObjects);
	bool qualified = object->name.first != wNULL && object->name.last != wNULL;
	for (set<int>::iterator mo = matchingObjects.begin(), moEnd = matchingObjects.end(); mo != moEnd; mo++)
		pushIntoObjectMatchesOrReplace(where, object, objectMatches, mo, firstNameAmbiguous, lastNameAmbiguous, qualified, globalSearch);
	return true;
}

