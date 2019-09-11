#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <set>
#include <mysql.h>
#include <unordered_map>
using namespace std;

#include "logging.h"
#include "io.h"
#include "general.h"
#include "intarray.h"
#include "pattern.h"
#include "profile.h"

void defineNames(void);
void defineTimePatterns(void);
void createMetaNameEquivalencePatterns(void);

int createNouns(void)
{ LFS
	// this has the same follows as _NOUN[9] (except _PP and _REL1 are optional)
	// _NOUN_OBJ is referenced by getNounPhraseInfo as having a differentiator of L"1"
	cPattern::create(L"_NOUN_OBJ{PNOUN}",L"1",
		5,L"reflexive_pronoun{N_AGREE}",    L"reciprocal_pronoun{N_AGREE}",
			L"relativizer{N_AGREE:SINGULAR}", L"personal_pronoun_nominative{N_AGREE}",  L"personal_pronoun{N_AGREE}",0,1,1,
										1,L"_ADJECTIVE_AFTER*1",0,0,1, // adjectives after the noun are rare
										//2,L"_PP",L"_REL1",0,0,4,  merged with _NOUN[9]
		0);
	// verbs should tend to be used as verbs
	// possessive pronouns are rarely nouns (removed - 'mine' is a perfectly fine noun)
	// L"demonstrative_determiner", removed - because __N1 may take adjectives and this class does not
	cPattern::create(L"__N1{N_AGREE}",L"",18,
		L"possessive_pronoun",L"interrogative_pronoun",L"pronoun",L"indefinite_pronoun",
		L"noun",L"abbreviation",L"measurement_abbreviation*1",L"trademark", // L"country", // removed -- I've got to trust some one--and it must be a woman. (moved to _NOUN)
		L"numeral_cardinal",L"numeral_ordinal",L"quantifier",L"Number*1",L"verb*1{VNOUN}",L"does*1{VNOUN}",L"letter",
		L"time",L"date",L"telephone_number",VERB_PRESENT_PARTICIPLE|NO_OWNER,1,1,0);

	cPattern::create(L"__HIS_HER_DETERMINER",L"",
						1,L"his{DET}",0,1,1,
						3,L",",L"/",L"or",0,1,1,
						1,L"her",0,1,1,0);
	// THE exhaustively broken eclectically skinny bean | skinny beans | the old Joe Miner | a great someone
	// ADJ added to capture which words were owned by a personal agent
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:NOUN}",L"2",
		7,L"determiner{DET}",L"demonstrative_determiner{DET}",L"possessive_determiner{DET:ADJ}",L"interrogative_determiner{DET}",L"quantifier{DET}",L"__HIS_HER_DETERMINER*1",L"_NAMEOWNER{DET}",0,0,1,
				1,L"_ADJECTIVE{_BLOCK}",0,0,3, 
				2,L"noun*1",L"Proper Noun*1",SINGULAR,0,2, // noun and Proper Noun must cost 1 otherwise they will match / diamond necklace
				2,L"__N1",L"_NAME{GNOUN:NAME}",0,1,1,                 // the noun and PN in ADJECTIVE.  The only difference is here they have
																																	 // no OWNER flags
			//2,L"_ADJECTIVE",L"preposition",0,0,2,0);// confused with repeated preposition phrases 'on the afternoon of May'.
				2,L"_ADJECTIVE_AFTER*2",L"letter*1",0,0,1,0); // an adjective after the noun is less common Bill A, Bill B
	cPattern::create(L"_META_GROUP{_FINAL_IF_ALONE:NOUN}",L"",
			1,L"possessive_determiner{DET:ADJ}",0,0,1,
			1,L"_ADJECTIVE{_BLOCK}",0,0,1,
			1,L"friend{N_AGREE}",0,1,1,
			0);
 // this also encourages a word identified as both adverb and adjective to be an adverb if identified before an adjective.
	// the more adjectives repeated, the more uncommon - taken out - discourages nouns taking on adjectives
	// all these old things / all my old things / half these old things / both old things
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:NOUN}",L"3",1,L"predeterminer",0,1,1,
										5,L"determiner{DET}",L"demonstrative_determiner{DET}",L"possessive_determiner{DET}",L"__HIS_HER_DETERMINER*1",L"_NAMEOWNER{DET}",0,0,1,
										3,L"_ADJECTIVE{_BLOCK}",L"noun*1",L"Proper Noun*1",SINGULAR,0,3,
										2,L"__N1",L"_NAME{GNOUN:NAME}",0,1,1,
										1,L"_ADJECTIVE",0,0,2,0);
										//1,L"_POSTMOD",0,0,1,0);
	// The good / the foremost -- quantifier isn't valid when combined with a predeterminer!
	// All the best / all their
	// a baker's
	// a greater good
	// the latter//the former
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:ADJOBJECT}",L"4",1,L"predeterminer",0,0,1, // took out GNOUN - N_AGREE should not be defined under GNOUN because it causes agreement problems - 'Tags are inconsistent for subject tagsets' 12/03/2007
										//5,L"determiner",L"demonstrative_determiner{N_AGREE}",L"possessive_determiner",L"quantifier",L"__HIS_HER_DETERMINER*1",0,1,1,
										//1,L"_ADJECTIVE*3{PLURAL}",0,1,1, // this entire pattern is rare
										3,L"determiner{DET}",L"possessive_determiner{DET}",L"quantifier{DET}",0,1,1, // L"demonstrative_determiner{DET}", removed - covered better by 'this' being a noun and the adjective being an ADJECTIVE_AFTER
										1,L"adverb",0,0,1,
										 // this entire pattern is rare and should not be encouraged (can be confused with an adjective to a noun)
										3,L"adjective*2{N_AGREE}",L"numeral_ordinal*2{N_AGREE}",L"noun*2{N_AGREE}",SINGULAR_OWNER|PLURAL_OWNER,1,1,
										0);
	// a female called Jane Finn
	// a Sikh called Bob
	// this was created to aid name recognition (name recognition help)
	cPattern::create(L"__NOUN",L"7",
										1,L"__NOUN[*]{SUBOBJECT}",0,1,1, 
										2,L"verb|called",L"verb|named",VERB_PAST_PARTICIPLE,1,1,
										1,L"_NAME*-1{SUBOBJECT}",0,1,1,
										0);
	// 'The chance offered her' was real.
	// 14420 (14441), 24612*, 30978, 55112, 92559*, 95781*
	// and red line drawn sideways
	// gloves fitted with the fingers...
	// a chance offered her
	// me tied by the leg here
	// the mysterious man found dead
	// a small brown diary taken...
	// consider using VERB_NO_PAST to increase match
	// __NOUN RU is Restricted Use
	cPattern::create(L"__NOUNRU",L"1",
										1,L"__NOUN[*]",0,1,1, 
										1,L"_VERBPASTPART*2{_BLOCK}",0,1,1,
										2,L"_PP",L"__ALLOBJECTS_1{SUBOBJECT:_BLOCK}",0,1,1,
										0);
	// I dare say [the little we know] won't be any good to you, sir.
	/* - creates too many bad matches
	cPattern::create(L"__NOUN{_BLOCK:GNOUN:VNOUN}",L"R",
										3,L"determiner{DET}",L"possessive_determiner{DET}",L"quantifier{DET}",0,1,1, // L"demonstrative_determiner{DET}", removed - covered better by 'this' being a noun and the adjective being an ADJECTIVE_AFTER
										1,L"adjective{N_AGREE}",0,1,1,
										3,L"Proper Noun{ANY:NAME}",L"personal_pronoun_nominative*1{N_AGREE}",L"personal_pronoun*1{N_AGREE}",NO_OWNER,1,1, // this is tightly controlled -
										2,L"__ALLVERB",L"_COND{VERB}",0,1,1,
										0);
	*/
	cPattern::create(L"__APPNOUN{NOUN}",L"1",
										5,L"determiner*2{DET}",L"demonstrative_determiner*2{DET}",L"possessive_determiner*2{DET}",L"__HIS_HER_DETERMINER*3",L"_NAMEOWNER{DET}",0,0,1,
										1,L"_ADJECTIVE_AFTER",0,0,3, // this cannot contain VERBPAST, as it is not
																								// possible yet to ascertain the difference between this and _S1.
																								// otherwise, _NOUN would win over _S1 because it has fewer subcomponents, which was incorrect.
										4,L"noun*2{N_AGREE}",L"_NAME*3{GNOUN:NAME}",L"personal_pronoun_nominative*2{N_AGREE}",L"personal_pronoun*2{N_AGREE}",NO_OWNER,1,1, // this is tightly controlled -
																								// no pronouns that could be adjectives -
										0);
	// you dears!  you scoundrel!  you old villain!
	// my son the doctor
	// the sooner the better (deleted from this pattern)
	// note that the _NOUN referred to in element 1 must not include _NOUN[6], as personal_pronouns are rarely used in this way.
	cPattern::create(L"__PNOUN{NOUN}",L"1",
										7,L"determiner{DET}",L"demonstrative_determiner{DET}",L"possessive_determiner{DET}",L"interrogative_determiner{DET}",L"quantifier{DET}",L"__HIS_HER_DETERMINER*1",L"_NAMEOWNER{DET}",0,0,1,
										1,L"_ADJECTIVE{_BLOCK}",0,0,3,
										// prefer _NAME over noun
										5,L"you{N_AGREE}",L"noun*1{N_AGREE}",L"personal_pronoun_nominative{N_AGREE}",L"_NAME{GNOUN:NAME}",L"personal_pronoun{N_AGREE}",NO_OWNER,1,1, // the one they want
										0);
	// all the boys, half the boys, double the sauce
	cPattern::create(L"__PNOUN{NOUN}",L"2",
										1,L"predeterminer{DET}",0,1,1,
										1,L"the",0,1,1,
										1,L"__N1",0,1,1,
										0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE}",L"5",
										1,L"__PNOUN",0,1,1,
										1,L",*3",0,0,1,
										1,L"__APPNOUN[*]{_BLOCK:RE_OBJECT}",0,1,1,
										0);
	// this is the same as "5" above but it is marked as a single object so "Iraqi weapons labs" is marked as a single object
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:NOUN}",L"N",
										5,L"determiner{DET}",L"demonstrative_determiner{DET}",L"possessive_determiner{DET}",L"interrogative_determiner{DET}",L"quantifier{DET}",0,0,1,
										1,L"_ADJECTIVE{_BLOCK}",0,0,1,
										// prefer _NAME over noun
										2,L"noun*1",L"_NAME{GNOUN:NAME}",NO_OWNER,1,1, 
										1,L"noun*2{N_AGREE}",NO_OWNER,1,1, // this is tightly controlled -
										0);
		// anyone so simple as _NOUN
		// anyone as simple as
		// anyone as simple and honest as
	cPattern::create(L"__NOUN",L"U",
										1,L"indefinite_pronoun",0,1,1,
										2,L"so",L"as",0,1,1,
										1,L"_ADVERB",0,0,1,
										2,L"adjective{ADJ}",L"verb*1{ADJ}", VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,1,1,                            // we can make cost of *1 for __NOUN[2]
										1,L"as",0,1,1,
										1,L"__APPNOUN[*]{_BLOCK:RE_OBJECT}",0,1,1,
										//1,L"is*1",0,0,1,
										0);
	cPattern::create(L"__NOUN",L"V",
										1,L"indefinite_pronoun",0,1,1,
										2,L"so",L"as",0,1,1,
										1,L"_ADVERB",0,0,1,
										2,L"adjective{ADJ}",L"verb*1{ADJ}", VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,1,1,                            // we can make cost of *1 for __NOUN[2]
										1,L"coordinator",0,1,1,
										2,L"adjective{ADJ}",L"verb*1{ADJ}", VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,1,1,                            // we can make cost of *1 for __NOUN[2]
										1,L"as",0,1,1,
										1,L"__APPNOUN[*]{_BLOCK:RE_OBJECT}",0,1,1,
										//1,L"is*1",0,0,1,
										0);
	cPattern::create(L"__NOUN",L"W",
										1,L"indefinite_pronoun",0,1,1,
										1,L"like",0,1,1,
										1,L"__APPNOUN[*]{_BLOCK:RE_OBJECT}",0,1,1,
										0);

	cPattern::create(L"__NOUN{_FINAL_IF_ALONE}",L"K",
										1,L"__NOUN[*]",0,1,1,
										2,L"eg",L"e.g.",0,1,1,
										1,L"__NOUN[*]{_BLOCK:RE_OBJECT}",0,1,1,
										0);
	// MERGED into __NOUN "C"
	//cPattern::create(L"__NOUN{_FINAL_IF_ALONE:PNOUN}",L"6",1,L"demonstrative_determiner",0,1,1,
	//                      1,L"_ADJECTIVE_AFTER",0,0,1, // they will declare 'these sane'
	//                      0);
	// created subobject so that address is not seen as a single object (so that the name in the address can be visible)
	// changed to _BLOCK so that __NOUN can be captured as a single object
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}",L"8",
		9,L"_LINE2ADDRESS{_BLOCK}",L"_LINE3ADDRESS{_BLOCK}",L"_POADDRESS",L"_MADDRESS",
			L"_DATE",L"_TIME", // He took office a year ago with a pledge
			L"_TELENUM",L"_NUMBER",
		L"demonstrative_determiner",0,1,1,
									 0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:PNOUN}",L"C",3,
						L"personal_pronoun_accusative{N_AGREE}", // used to be SUBJ
						L"personal_pronoun_nominative{N_AGREE}", // used to be OBJ
						L"personal_pronoun{N_AGREE}",
													//L"demonstrative_determiner{N_AGREE}", __NOUN[2]
//                          L"--", // used to be in _N1,took out 11/4/2005-caused too many misparses see other changes *--*
													 0,1,1,
																										 // they will declare 'me sane' - L"_ADJECTIVE_AFTER", NOT applicable because this verb actually has two objects
																										 // also 'me sane' or I sane is not really one noun.
												1,L"reflexive_pronoun",0,0,1,
												0);
	// this pattern must go after all nouns EXCEPT it must be before any noun patterns that use _NOUN as a subpattern
	//cPattern::create(L"_NOUN",L"",1,L"__NOUN[*]",0,1,1,0);

	// better than two years, more than two years
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE}",L"H",
										1,L"_ADJECTIVE",0,1,1,
										1,L"than",0,1,1,
										3,L"_NOUN_OBJ{SUBOBJECT}",L"__NOUN[*]{SUBOBJECT}",L"__NOUNREL{SUBOBJECT}",0,1,1,    // the news I gave Bill __ALLOBJECT
										0);
	// black beans and head lettuce
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}",L"J",
										1,L"both",0,0,1,
										2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ",0,1,1,
										2,L"coordinator|and",L"&",0,1,1,
										3,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",L"__NOUNREL{MOBJECT}",0,1,1,0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:SINGULAR:MNOUN:_BLOCK}",L"O",
										1,L"adverb|either*-2",0,0,1,
										2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",0,1,1,
										1,L"coordinator|or",0,1,1,
										3,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",L"__NOUNREL{MOBJECT}",0,1,1,0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:SINGULAR:not:MNOUN:_BLOCK}",L"7",
										1,L"adverb|neither*-2",0,0,1,
										2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",0,1,1,
										1,L"coordinator|nor",0,1,1,
										3,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",L"__NOUNREL{MOBJECT}",0,1,1,0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:SINGULAR:MNOUN:_BLOCK}",L"A",
										1,L"adverb|neither*-2",0,0,1,
										2,L"__NOUN[*]{not:MOBJECT}",L"_NOUN_OBJ{not:MOBJECT}",0,1,1,
										1,L"coordinator|nor",0,1,1,
										3,L"__NOUN[*]{not:MOBJECT}",L"_NOUN_OBJ{not:MOBJECT}",L"__NOUNREL{not:MOBJECT}",0,1,1,
										1,L",",0,0,1,
										1,L"conjunction|but*-2",0,1,1,
										3,L"__NOUN[*]{but:MOBJECT}",L"_NOUN_OBJ{but:MOBJECT}",L"__NOUNREL{but:MOBJECT}",0,1,1,
										0);
	// beans, |  he,
	cPattern::create(L"__NOUN_S1",L"",
														1,L"coordinator",0,0,1,
														2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",0,1,1,
														1,L"__INTERPPB[*]{_BLOCK}",0,0,1, 
														1,L",",0,1,1,0);
	// black beans, head lettuce, horribly rotten bananas and cheese
	// black beans, and head lettuce, horribly rotten bananas and cheese
	// N1, N2, N3, and N4 and N5. // N4 and N5 is a NOUN[J]
	cPattern::create(L"__MNOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}",L"Y",
												 2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",0,1,1,
												 1,L"__INTERPPB[*]{_BLOCK}",0,0,1, 
												 1,L",",0,1,1,
												 1,L"__NOUN_S1",0,0,4,
												 2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",0,1,1,
												 1,L"__INTERPPB[*]{_BLOCK}",0,0,1, 
												 1,L",",0,0,1,
												 1,L"coordinator",0,1,1,
												 3,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",L"__NOUNREL{MOBJECT}",0,1,1,0);
	// black beans, and head lettuce, horribly rotten bananas, and cheese // note extra comma which should be rare
		// this extra cost makes it less likely that this pattern will match the object of a verb and a
		// relativizer which most likely is the start of a relative phrase.
	// Marguerite with her box of jewels, the church scene, Siebel and his flowers, and Faust and Mephistopheles.
	// EMNOUN is the only case where extra cost is not required because in this case the , is used to offset
	//   a subchained compound noun at the end (Faust and Meph).
	cPattern::create(L"__EMNOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}",L"",
										2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ",0,1,1,
										2,L"coordinator|and",L"&",0,1,1,
										3,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",L"__NOUNREL{MOBJECT}",0,1,1,0);
	cPattern::create(L"__MNOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}",L"Z",
												 2,L"__NOUN[*]{MOBJECT}",L"_NOUN_OBJ{MOBJECT}",0,1,1,
												 1,L"__INTERPPB[*]{_BLOCK}",0,0,1, 
												 1,L",",0,1,1, // The tea came at last, and Tuppence, rousing herself ...
												 1,L"__NOUN_S1",0,0,4,
												 1,L"coordinator",0,1,1,
												 4,L"__NOUN[*]*3{MOBJECT}",L"_NOUN_OBJ*3{MOBJECT}",L"__NOUNREL*3{MOBJECT}",L"__EMNOUN{MOBJECT}",0,1,1,0);
	// my son of the east Village (PP)
	// anxiety which underlay his tone (REL1)
	// the right to lay down arms (INFP)
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE}",L"9",
										2,L"__NOUN[*]",L"_NOUN_OBJ",0,1,1,
										3,L"__INTERPPB[*]{_BLOCK}",L"_DATE*1{FLOATTIME}",L"_TIME*1{FLOATTIME}",0,0,1, 
										3,L"_PP",L"_REL1",L"_INFP",0,1,1,
										3,L"_PP*1",L"_REL1*1",L"_INFP*1",0,0,2,
										0);
	// and medical man written all over him
	/* this pattern was removed because RULE 4 of L&L disallows anything in __IMPLIEDWITH from referencing the enclosing __NOUN
	// which would incorrectly exclude the noun preceding medical man from matching 'him'
	cPattern::create(L"__IMPLIEDWITH",L"",
										2,L"coordinator|and",L"&",0,1,1,
										1,L"__PNOUN[*]",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_VERBPASTPART{vB}",0,1,1,
										2,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",0,0,1,
										0);
	*/
	// so that a relative phrase can still be attributed with a noun that precedes it
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE}",L"X",
										2,L"__NOUN[*]",L"_NOUN_OBJ",0,1,1,
										1,L",",0,1,1,
										1,L"_REL1",0,1,1, // ,L"__IMPLIEDWITH"
										1,L",",0,1,1, // if , is left off, _PP in NOUN[9] and _PP at the end or _REL will multiplicatively combine
										0);
	cPattern::create(L"__NOUNREL{_FINAL_IF_ALONE:_ONLY_END_MATCH}",L"",
										2,L"__NOUN[*]",L"_NOUN_OBJ",0,1,1,
										1,L",",0,1,1,
										1,L"_REL1",0,1,1,
										0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE}",L"B",
										1,L"_ADJECTIVE",0,0,3,
										1,L"quotes",OPEN_INFLECTION,1,1,
										3,L"__NOUN[*]*2",L"__MNOUN[*]*2",L"_NAME",0,1,1, // quoted nouns should be rare in general
										1,L"adverb",0,0,1, // her " afternoon out . "
										2,L",",L".",0,0,1,
										1,L"quotes",CLOSE_INFLECTION,1,1,0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:GNOUN}",L"G",
										7,L"determiner{DET}",L"demonstrative_determiner{DET}",L"possessive_determiner{DET}",L"interrogative_determiner{DET}",L"quantifier{DET}",L"__HIS_HER_DETERMINER*1",L"_NAMEOWNER{DET}",0,1,1,
										1,L"_ADJECTIVE",0,0,3,
										1,L"quotes",OPEN_INFLECTION,1,1,
										3,L"__NOUN[*]*-1",L"__MNOUN[*]",L"_NAME",0,1,1, // compensate for possible missing determiner cost from internal __NOUN
										1,L"adverb",0,0,1, // her " afternoon out . "
										2,L",",L".",0,0,1,
										1,L"quotes",CLOSE_INFLECTION,1,1,0);
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}",L"L",
										1,L"letter",0,1,1,
										1,L".",0,1,1,
										0);
	// no. 7
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}",L"Q",
										7,L"no",L"determiner|no",L"num",L"number",L"Proper Noun|no",L"Proper Noun|num",L"Proper Noun|n",0,1,1,
										1,L".",0,0,1,
										3,L"Number*-3",L"roman_numeral*-3",L"numeral_cardinal*-3",NO_OWNER,1,1,
										0);
	// company
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}",L"M",
										1,L"__NOUN[*]",0,1,1,
										1,L",",0,1,1,
										1,L"_BUS_ABB",0,1,1,0);
	return 0;
}

// rules for patterns - any reference to any pattern must reference any occurrence of that pattern except for itself.
int createBasicPatterns(void)
{ LFS
	//cPattern *p=NULL;
	cPattern::create(L"_ABB",L"1",1,L"abbreviation",0,1,1,
											1,L".",0,0,1,0);
	cPattern::create(L"_MABB",L"",1,L"letter",0,1,1,
											1,L".",0,1,1,0);
	cPattern::create(L"_ABB",L"3",1,L"_MABB",0,0,9,
											1,L"letter",0,1,1,
											1,L".",0,0,1,0);
	cPattern::create(L"_MEAS_ABB",L"",1,L"measurement_abbreviation",0,1,1,
											1,L".",0,0,1,0);
	cPattern::create(L"_STREET_ABB",L"",1,L"street_address_abbreviation",0,1,1,
											1,L".",0,0,1,0);
	cPattern::create(L"_BUS_ABB",L"",1,L"business_abbreviation",0,1,1,
											1,L".",0,0,1,0);
	cPattern::create(L"_TIME_ABB",L"",1,L"time_abbreviation",0,1,1,
											1,L".",0,0,1,0);
	cPattern::create(L"_DATE_ABB",L"",1,L"date_abbreviation",0,1,1,
											1,L".",0,0,1,0);
	cPattern::create(L"_HON_ABB",L"",1,L"honorific_abbreviation",0,1,1,
											1,L".",0,0,1,0);
	// to and fro -- usage: he was tossed to and fro
	// by and by Mrs. Vandermeyer brought me some supper
	cPattern::create(L"__ADVERB",L"1",2,L"to",L"preposition|by",0,1,1,
											2,L"and",L"&",0,1,1,
											2,L"preposition|fro",L"preposition|by",0,1,1,0);
	// this morning / this very morning
	cPattern::create(L"_ADVERB{FLOATTIME}",L"T",
											2,L"demonstrative_determiner{TIMEMODIFIER}",L"adjective{TIMEMODIFIER}",0,1,1,
											1, L"adverb{ADV}", 0, 0, 1, // very
											5,L"month{MONTH}",L"daysOfWeek{DAYWEEK}",L"season{SEASON}",L"timeUnit{TIMECAPACITY}",L"dayUnit{TIMECAPACITY}",0,1,1,
											0);
	cPattern::create(L"_ADVERB{FLOATTIME}",L"9",1,L"to",0,1,1,
												1,L"dash|-*-2",0,1,1,
												2,L"noun|day{DAY}",L"noun|morrow{DAY}",0,1,1,0);
	// lightly,
	cPattern::create(L"__ADV_S1",L"",3,L"adverb{ADV}",L"not{:not}",L"never{:never}",0,1,4,
											 1,L",",0,1,1,0);
	// lightly, slowly
	cPattern::create(L"__ADVERB{FLOATTIME}",L"2",1,L"__ADV_S1",0,0,4,
											3,L"adverb{ADV}",L"not{:not}",L"never{:never}",0,1,4,0);

	// lightly, brightly and painfully slowly
	cPattern::create(L"__ADVERB",L"3",1,L"__ADVERB",0,1,1,
											1,L",",0,0,1,
											1,L"coordinator",0,1,1,
											3,L"adverb{ADV}",L"not{:not}",L"never{:never}",0,1,4,0);
	cPattern::create(L"_ADVERB",L"1",1,L",",0,1,1,
											1,L"__ADVERB",0,1,1,
											1,L",",0,1,1,
											0);
	/* Causes incorrect parse - "Then"--Tuppence's voice shook a little--"there's a boy."
	cPattern::create(L"_ADVERB",L"2",1,L"quotes",OPEN_INFLECTION,1,1,
											1,L"__ADVERB",0,1,1,
											1,L"quotes",CLOSE_INFLECTION,1,1,
											0);
	*/
	cPattern::create(L"_ADVERB",L"3",1,L"brackets",OPEN_INFLECTION,1,1,
											1,L"__ADVERB",0,1,1,
											1,L"brackets",CLOSE_INFLECTION,1,1,
											0);
	//cPattern::create(L"_ADVERB{_BLOCK:EVAL}",L"4",1,L"as",0,1,1,
	//                    1,L"__NOUN[*]",0,1,1,
	//                    1,L"_VERB[*]",0,1,1,
	//                    0);
		cPattern::create(L"_ADVERB{_FINAL:_NO_REPEAT}",L"5",1,L"__ADVERB",0,1,1,0);
	// better than two years, more than two years
	cPattern::create(L"_ADVERB{_FINAL}",L"5",
										1,L"__ADVERB{TIMEMODIFIER}",0,1,1,
										1,L"than",0,1,1,
										3,L"numeral_ordinal{TIMEMODIFIER}",L"numeral_cardinal{TIMEMODIFIER}",L"determiner{TIMEMODIFIER}",0,1,1,
										2,L"timeUnit{TIMECAPACITY}",L"season{SEASON}",0,1,1,
										0);
	cPattern::create(L"_ADVERB{_FINAL}",L"6",
										1,L"more",0,1,1,
										1,L"than",0,1,1,
										2,L"once",L"twice",0,1,1,
										0);
	// He held as much as a 200 pound fish
	cPattern::create(L"_ADVERB{_FINAL}",L"7",1,L"as",0,1,1,
															1,L"adverb|much",0,1,1,
															1,L"as",0,1,1,
															0);
	cPattern::create(L"_ADVERB{_FINAL}",L"8",
											4,L"adjective|little",L"noun|inch",L"noun|step",L"noun|bit",0,1,1,
											1,L"preposition|by",0,1,1,
											4,L"adjective|little",L"noun|inch",L"noun|step",L"noun|bit",0,1,1,0);

	defineNames();
	createMetaNameEquivalencePatterns();
	cPattern::create(L"_NUMBER{_NO_REPEAT}",L"1",
				1,L"numeral_cardinal",0,1,1,
				2,L"numeral_cardinal",L",",0,0,5, // L"and", covered by NOUN[J]
				1,L"numeral_cardinal",0,1,1,0);
	cPattern::create(L"__NUMBER",L"1",1,L",",0,1,1,
											 1,L"Number",0,1,1,0);
	cPattern::create(L"_NUMBER{_NO_REPEAT:FLOATTIME}",L"2",2,L"$*-1",L"#*-1",0,0,1,
											2,L"Number",L"money",0,1,1,
											1,L"__NUMBER*-1",0,0,3,
											0);
	cPattern::create(L"_NUMBER{_NO_REPEAT}",L"3",1,L"number*-1",0,0,1, // the form of the word L"number"
													1,L"numeral_cardinal",0,1,1,0);
	cPattern::create(L"_NUMBER{_NO_REPEAT}",L"4",1,L"Number",0,1,1,
													5,L"%",L"p",L"f",L"c",L"k",0,1,1,0); // both % and p from BNC, f,c,k are temperatures
	// from BNC
	cPattern::create(L"_NUMBER{_NO_REPEAT}",L"5",2,L"$",L"#",0,1,1,
													1,L"Number",0,1,1,
													2,L"m",L"b",0,1,1,0);

	cPattern::create(L"_STEP",L"1", 1,L"brackets",OPEN_INFLECTION,0,1,
												 2,L"letter",L"number",0,1,1,
												 2,L"brackets",L".",CLOSE_INFLECTION,1,1,0);
	// lightly frozen | lightly matted | lightly rotting
	// VERB_PAST removed 12/16/2005
	// adverb use after adjective should be rare!
	// verb use as adjective should be not as common
	// if the proper noun is not preceded by an adverb and is a single word then it will be covered by a modifier to NOUN directly.
	// if the proper noun is not preceded by an adverb and is a multi word then it is a _NAME, which is directly used by _NOUN.
	// if the proper noun is in an owner form then it will be a NAMEOWNER
	cPattern::create(L"__ADJECTIVE",L"2",1,L"_ADVERB",0,0,1,
		7,L"adjective{ADJ}",L"verb*1{ADJ}",L"numeral_ordinal{ADJ}",L"_NUMBER{ADJ}",L"ex{ADJ}",L"noun{ADJ}",L"no{ADJ:no}", // removed *1 as cost so that
			VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,1,2,                            // we can make cost of *1 for __NOUN[2]
												 1,L"adverb*1",0,0,1,0);                                                                // 2/3/2007
		// OWNER attributes deleted - why would ownership occur before a dash?
	cPattern::create(L"__ADJECTIVE",L"3",1,L"_ADVERB",0,0,1,
		6,L"adjective{ADJ}",L"numeral_ordinal{ADJ}",L"_NUMBER{ADJ}",L"ex{ADJ}",L"noun*1{ADJ}",L"no{ADJ:no}",SINGULAR,1,2,
		1,L"dash",0,1,1,
		7,L"adjective{ADJ}",L"verb*1{ADJ}",L"numeral_ordinal{ADJ}",L"_NUMBER{ADJ}",L"ex{ADJ}",L"noun{ADJ}",L"no{ADJ:no}",
			VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,1,2,
		1,L"adverb*1",0,0,1,0);
		// first-class passengers
	cPattern::create(L"__ADJECTIVE",L"A",1,L"_ADVERB",0,0,1,
		6,L"adjective{ADJ}",L"numeral_ordinal{ADJ}",L"_NUMBER{ADJ}",L"ex{ADJ}",L"noun*1{ADJ}",L"no{ADJ:no}",SINGULAR,1,2,
		1,L"dash",0,1,1,
		3,L"noun*1{ADJ}",L"adjective|best*-4",L"noun|class*-4",SINGULAR,1,1,
		8,L"adverb*1",L"adjective{ADJ}",L"verb*1{ADJ}",L"numeral_ordinal{ADJ}",L"_NUMBER{ADJ}",L"ex{ADJ}",L"noun{ADJ}",L"no{ADJ:no}",0,0,2,
		0);

	// _NAME will ONLY be used in a case where a proper noun is not in an owner form AND it is preceded by an adverb.
	cPattern::create(L"__NADJECTIVE",L"",
		2,L"_ADVERB",L"commonProfession*1",0,0,1, // cp=commonProfession What was author Jasper Fforde's first book?
		2,L"_NAMEOWNER",L"_NAME*1",0,1,1,
		1,L"adverb*1",0,0,1,0);
	// an adjective consisting of a past verb is not allowed AFTER a noun (nor ex)
	// VERB_PAST removed 12/16/2005
	// adverb use after adjective should be rare!
	// must restrict a verb after a noun tightly! (lowered from 5 to 3 10/17/2007)
	// a noun as an adjective after another noun is probably not correct!
	cPattern::create(L"_ADJECTIVE_AFTER{_NOT_AFTER_PRONOUN}",L"",1,L"_ADVERB",0,0,1,
												 // every moment *RUNNING* | every moment GAINED (verb entry below) 11/4/2005
		5,L"adjective{ADJ}",L"numeral_ordinal",L"verb*2",L"_NUMBER",L"noun*2",VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,1,1,
												 1,L"adverb*1",0,0,1,0); // he said , nodding -gravely .
	// lightly frozen,
	cPattern::create(L"__ADJ_S2",L"",1,L"__ADJECTIVE[*]",0,1,1,
											 1,L",",0,1,1,0);
	// Ally's or Ally
	cPattern::create(L"__NADJ_S2",L"",1,L"__NADJECTIVE[*]",0,1,1, // __NAMEOWNER also provides PLURAL
											 1,L",",0,1,1,0);
	// lightly frozen and at the same time melting
	// lightly frozen, magnificently rotting and melting
	cPattern::create(L"__ANDADJ",L"",
												 1,L",",0,0,1,
												 1,L"coordinator",0,1,1,
												 2,L"_PP",L"_REL1[*]",0,0,1,
												 1,L"__ADJECTIVE[*]",0,1,1,0); // may combine with __ADJECTIVE L"3L" but should be very rare// __NAMEOWNER also provides PLURAL
	// Ned and at the same time Ally's
	cPattern::create(L"__NANDADJ",L"",
												 1,L",",0,0,1,
												 1,L"coordinator",0,1,1,
												 2,L"_PP",L"_REL1[*]",0,0,1,
												 1,L"__NADJECTIVE[*]",0,1,1,0); // may combine with __ADJECTIVE L"3L" but should be very rare// __NAMEOWNER also provides PLURAL
	// a 121 lb hammer
	cPattern::create(L"__ADJECTIVE",L"4",1,L"Number",0,1,1,
															1,L"_MEAS_ABB{ADJ}",SINGULAR,1,1,0);
	// a 121 to 200 pound fish
	cPattern::create(L"__ADJECTIVE",L"5",1,L"Number",0,1,1,
															1,L"to",0,1,1,
															1,L"Number",0,1,1,
															0);
	// she was younger than he could have imagined
	cPattern::create(L"__ADJECTIVE",L"A",1,L"adjective",ADJECTIVE_COMPARATIVE,1,1,
															1,L"than",0,1,1,
															1,L"__S1{EVAL:_BLOCK}",0,1,1,
															0);
	// lightly frozen, magnificently melting skeleton
	cPattern::create(L"__ADJECTIVE{APLURAL}",L"6",1,L"__ADJ_S2",0,1,3,
															1,L"__ADJECTIVE[*]",0,1,1,
															1,L"__ANDADJ",0,0,1,
															0);
	cPattern::create(L"__NADJECTIVE{APLURAL}",L"7",1,L"__NADJ_S2",0,1,3,
															1,L"__NADJECTIVE[*]*2",0,1,1, // __NAMEOWNER also provides PLURAL
															1,L"__NANDADJ*-2",0,0,1, // strongly encourage AND
															0);
	// magnificently melting and dripping skeleton
	cPattern::create(L"__ADJECTIVE{APLURAL}",L"8",
															1,L"__ADJECTIVE[*]",0,1,1,
															1,L"__ANDADJ",0,1,1,
															0);
	// Ned and Ally's
	cPattern::create(L"__NADJECTIVE{APLURAL}",L"9",
															1,L"__NADJECTIVE[*]",0,1,1, // __NAMEOWNER also provides PLURAL
															1,L"__NANDADJ",0,1,1,
															0);

	cPattern::create(L"_ADJECTIVE{_FINAL_IF_ALONE}",L"1",2,L"quotes",L"brackets",OPEN_INFLECTION,1,1,
												 3,L"__ADJECTIVE",L"_NAMEOWNER",L"noun*1{ADJ}",NO_OWNER,1,1, // block PLURAL
												 2,L"quotes",L"brackets",CLOSE_INFLECTION,1,1,
												 0);
	cPattern::create(L"_ADJECTIVE{_FINAL_IF_ALONE}",L"2",2,L"__ADJECTIVE",L"__NADJECTIVE",0,1,1,0);

	defineTimePatterns();
	// (732) 598-6773
	cPattern::create(L"_TELENUM",L"1",1,L"brackets",OPEN_INFLECTION,1,1,
												1,L"Number",0,1,1,
												1,L"brackets",CLOSE_INFLECTION,1,1,
												1,L"telephone_number",0,1,1,0);

	createNouns();
	cPattern::create(L"_REF{_FINAL}",L"1",1,L"brackets",OPEN_INFLECTION,0,1,
											 1,L"pnum",0,1,1,
											 1,L".",0,1,1,
											 1,L"Number",0,1,1,
											 1,L"brackets",CLOSE_INFLECTION,0,1,
											 0);

	// prepositional phrases interjected as asides
	// , from the very first,
	cPattern::create(L"__INTERPP",L"1",
									 1,L",*2",0,1,1, // a cost so that this doesn't interfere with commas used for compound adverbs, adjectives, and nouns
									 4,L"__NOUN[*]*1",L"_PP",L"interjection",L"conjunction",0,1,1,
									 1,L",",0,1,1,
									 0);
	cPattern::create(L"__INTERPP",L"2",
									 1,L",",0,1,1,
									 4,L"__NOUN[*]",L"_PP",L"interjection",L"conjunction",0,1,1,
									 1,L",",0,1,1,
									 4,L"__NOUN[*]",L"_PP",L"interjection",L"conjunction",0,1,1,
									 1,L",",0,1,1,
									 0);
	// Rafid Ahmed Alwan al-Janabi (Arabic: رافد أحمد علوان‎, Rāfid Aḥmad Alwān; born 1968), known 
	cPattern::create(L"__INTERPPB",L"3",
									 1,L"brackets",OPEN_INFLECTION,1,1,
									 10,L"__NOUN[*]",L"_PP",L"__S1{_BLOCK}",L"_ADJECTIVE[*]",L"interjection",L"conjunction",L"coordinator",L":",L",",L";",0,1,5,
									 1,L"brackets",CLOSE_INFLECTION,1,1,
									 0);
	cPattern::create(L"__INTERPP",L"4",
									 1,L"_REF",0,1,1,0);
	return 0;
};

// Page 694, Section 9.4.2.1 Grammatical patterns
// Pattern 4 - verb + bare infinitive clause (dare, help, let)
// Pattern 5 - verb + NP + bare infinitive clause (have, feel, help)
int createBareInfinitives(void)
{ LFS
	//cPattern *p=NULL;
	// I make/made you go
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"1",
										1,L"verbverb{vS:V_HOBJECT:V_AGREE}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"A",
										1,L"verb|have{vS:V_HOBJECT:V_AGREE}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,1,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"2",
										1,L"verbverb{past:V_HOBJECT:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"B",
										1,L"verb|had{past:V_HOBJECT:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,1,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"C",
										1,L"verbverb{past:V_HOBJECT:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
										1,L"adverb|rather",0,1,1,
										1,L"preposition|than",0,1,1,
										2,L"verbverb",L"verb|had",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	// I would make you go  | I won't make you go
	// you will/would make them protect you ?
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"3",
										1,L"_COND",0,1,1,
										2,L"verbverb{vS:V_HOBJECT}",L"verb|have{vS:V_HOBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	// I don't let him always allow them to depart
	// I do make you go  | I don't make you go - matches VERB 3
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"4",
										1,L"_DO{imp}",0,1,1,
										2,L"verbverb{vS:V_HOBJECT}",L"verb|have{vS:V_HOBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	// I have/had made you do that  / had made L"his indecision of character" be
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"5",
										1,L"_HAVE",0,1,1,
										2,L"verbverb{vB:V_HOBJECT}",L"verb|had{vS:V_HOBJECT}",VERB_PAST_PARTICIPLE,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	// I am/was [making you go] and do that
	// you are [making them protect] you ?
		// Tuppence hated [letting her dangle] below the ceiling.
		// Tuppence loved [feeling the grass grow] under her feet.
		// Terrance hated [letting the grass grow] under her feet.
		// Harry felt her [hearing the glass slide] underneath the fitting.
		// Hillary thought [watching them float] under the water was quite wonderful.
		// David liked [making him do] that.
		// I preferred helping her finish the table.
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"6",
										1,L"_IS",0,1,1,
										2,L"verbverb{vC:V_HOBJECT}",L"verb|having{vS:V_HOBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
		// feeling, hearing, seeing, watching, telling, daring, letting, making, helping, having...
		// Tuppence hated thinking the grass grows.
		cPattern::create(L"__NOUN{_BLOCK:GNOUN:VNOUN}",L"6",
										2,L"verbverb{vE:V_HOBJECT}",L"verb|having{vE:V_HOBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
										0);
	// I have/had made you do that  / had made L"his indecision of characterL" be
	// I will have made him go and do that | I will definitely have let him go and do that
	// I would have made him do that
		// I would have helped do that
	cPattern::create(L"_VERB_BARE_INF{VERB}",L"7",
										1,L"_COND",0,1,1,
										1,L"_HAVE{_BLOCK}",0,1,1,
										2,L"verbverb{vAB:V_HOBJECT}",L"verb|had{vAB:V_HOBJECT}",VERB_PAST_PARTICIPLE,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										0);
	return 0;
}

/* Infinitive phrases are objects or subjects, with their tags blocked from being seen.
	 within the IVERB tag, there are v* verb sense tags, V_OBJECT objects and the OBJECT tags produced by __ALLOBJECTS

To finish her shift without spilling another pizza into a customer's lap is Michelle's only goal tonight.
	 [To finish her shift without spilling another pizza into a customer's lab functions as a noun because
	 it is the subject of the sentence.]

Lakesha hopes to win the approval of her mother by switching her major from fine arts to pre-med.
	[To win the approval of her mother functions as a noun because it is the direct object for the verb hopes.]

To get through Dr. Peterson's boring history lectures, Ryan drinks a triple espresso before class and
	 stabs himself in the thigh with a sharp pencil whenever he catches himself drifting off.
	 [To get through Dr. Peterson's boring history lectures functions as an adjective because it modifies Ryan.]

Kelvin, an aspiring comic book artist, is taking Anatomy and Physiology this semester
	 to understand the interplay of muscle and bone in the human body.
	 [To understand the interplay of muscle and bone in the human body functions as an adverb because it modifies the verb is taking.]

*/
void createInfinitivePhrases(void)
{ LFS
	//cPattern *p=NULL;
	// goes with next INFP  [PRESENT SUB]
	cPattern::create(L"__INFPSUB{IVERB:_BLOCK}",L"",1,L"coordinator*-1{ITO}",0,1,1,
												1,L"_ADVERB",0,0,1,
												4,L"be{vS:id:V_OBJECT}",L"verb{vS:V_OBJECT}",L"does{vS:V_OBJECT}",L"have{vS:V_OBJECT}",
													VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
												3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // to act upon the ephemeral enthusiasms of an odd individual 8/28/2006
												0);
	// to L"infinitive" phrase as a noun object [PRESENT]
	// to send him / to send him and Bob
	// simple infinitive nonfinite verb phrase Quirk CGEL (3.56) vIS
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"1",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"verbal_auxiliary{V_AGREE}",0,0,1, // to help eat this
										4,L"be{vS:id:V_OBJECT}",L"verb{vS:V_OBJECT}",L"does{vS:V_OBJECT}",L"have{vS:V_OBJECT}",
											VERB_PRESENT_FIRST_SINGULAR,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // added _PP 8/28/2006
										3,L"__INFP5SUB",L"__INFPSUB",L"__INFPT2SUB",0,0,1,
										0);
		// verb bare infinitive form of infinitive phrase [PRESENT]
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"B",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"verbverb{vS:V_HOBJECT:V_AGREE}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										2,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",0,0,1,
										2,L"_ADVERB",L"_PP",0,0,2,
										5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",L"does_negation{vS:not:V_AGREE:V_OBJECT}",
											L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // added _PP 8/28/2006
										2,L"__INFP5SUB",L"__INFPSUB",0,0,1,
										0);
	// to L"infinitive" phrase as a noun object
	// to "send him" / to "send him and Bob"
	// simple infinitive nonfinite verb phrase Quirk CGEL (3.56) vIS
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"2",1,L"to{ITO}",0,1,1,
										1,L"quotes",OPEN_INFLECTION,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"verbal_auxiliary{V_AGREE}",0,0,1, // to help eat this?
										4,L"be{vS:id:V_OBJECT}",L"verb{vS:V_OBJECT}",L"does{vS:V_OBJECT}",L"have{vS:V_OBJECT}",
											VERB_PRESENT_FIRST_SINGULAR,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // added _PP 8/28/2006
										1,L"quotes",CLOSE_INFLECTION,1,1,
										0);
	// THINKSAY [PRESENT SUB]
	cPattern::create(L"__INFPT2SUB{IVERB:_BLOCK}",L"",1,L"coordinator{ITO}",0,1,1,
												1,L"_ADVERB",0,0,1,
												1,L"think{vS:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
												1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
												0);
	// to L"infinitive" phrase as a noun object [PRESENT]
	// to think he should go / to think he should go and she should also go.
	// simple infinitive nonfinite verb phrase Quirk CGEL (3.56) vIS
	cPattern::create(L"__INFPT{IVERB:_BLOCK}",L"1",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"verbal_auxiliary{V_AGREE}",0,0,1, // to help think this through
										1,L"think{vS:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR,1,1,
										2,L"_ADVERB",L"preposition*2",0,0,1, // preposition use should be rare!
										1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
										3,L"__INFP5SUB",L"__INFPSUB",L"__INFPT2SUB",0,0,1,
										0);
	// he had reason 'to be proud' -- removed as a duplicate of L"__INFP{VERB}",L"1" 5/10/2006
	//cPattern::create(L"__INFP{VERB}",L"2",1,L"to",0,1,1,
	//                  1,L"_ADVERB",0,0,2,
	//                  1,L"_BE{id}",0,1,1,
	//                  1,L"_ADJECTIVE",0,1,2,
	//                  0);
	// goes with next INFP [PAST SUB]
	cPattern::create(L"__INFP3SUB{IVERB:_BLOCK}",L"",1,L"coordinator{ITO}",0,1,1,
												1,L"_ADVERB",0,0,1,
												1,L"_VERBPASTPART{vB}",0,1,1,
												3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
												0);
	// to have sent him / to have sent him and Bob [PAST]
	// L"B" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"3",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_HAVE{_BLOCK}",0,1,1,
										1,L"_VERBPASTPART{vB}",0,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										1,L"__INFP3SUB*1",0,0,1, // prefer __S1 to __INFP3SUB
										0);
	/*THINKSAY*/
	// goes with next INFP [PAST SUB]
	cPattern::create(L"__INFP3TSUB{IVERB:_BLOCK}",L"",1,L"coordinator{ITO}",0,1,1,
												1,L"_ADVERB",0,0,1,
												1,L"_THINKPASTPART{vB}",0,1,1,
												1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
												0);
	// to have thought he wanted to go / to have thought he wanted this and she didn't stop him. [PAST]
	cPattern::create(L"__INFPT{IVERB:_BLOCK}",L"3",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_HAVE{_BLOCK}",0,1,1,
										1,L"_THINKPASTPART{vB}",0,1,1,
										1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
										1,L"__INFP3TSUB",0,0,1,
										0);
	// goes with next INFP 
	cPattern::create(L"__INFP4SUB{IVERB:_BLOCK}",L"",1,L"coordinator{ITO}",0,1,1,
												1,L"verb{vC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
												3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
												0);
	// to be sending him / to be sending him and Bob [PRESENT]
	// L"C" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"4",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_BE{_BLOCK}",0,1,1,
										1,L"verb{vC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										1,L"__INFP4SUB",0,0,1,
										0);
	/*THINKSAY*/
	// goes with next INFP [PRESENT SUB]
	cPattern::create(L"__INFP4TSUB{IVERB:_BLOCK}",L"",1,L"coordinator{ITO}",0,1,1,
												1,L"think{vC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
												1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
												0);
	// to be thinking he should go / to be thinking he should go and she should too. [PRESENT]
	// L"C" infinitive nonfinite verb phrase Quirk CGEL (3.56) 
	cPattern::create(L"__INFPT{IVERB:_BLOCK}",L"4",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_BE{_BLOCK}",0,1,1,
										1,L"think{vC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
										1,L"__INFP4TSUB",0,0,1,
										0);
	// goes with next INFP [PRESENT SUB]
	cPattern::create(L"__INFP5SUB{IVERB:_BLOCK}",L"",1,L"coordinator{ITO}",0,1,1,
												1,L"_ADVERB",0,0,1,
												// encourage __INFP5SUB to stick closely together / I was just in time to see him ring the bell and get admitted to the house
												2,L"_BE*-1{_BLOCK}",L"verb|get*-1{V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
												1,L"verb{vD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
												3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
												0);
	// PASSIVE
	// to be blended / to get admitted [PRESENT]
	// "D" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"5",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										2,L"_BE{_BLOCK}",L"verb|get{V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
										2,L"verb{vD:V_OBJECT}",L"does{vD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										3,L"__INFP5SUB",L"__INFPSUB",L"__INFPT2SUB",0,0,1,
										0);

	// to have been sending him 
	// L"BC" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"6",1,L"to{ITO}",0,1,1,
										 1,L"_ADVERB",0,0,1,
										 1,L"_HAVE{_BLOCK}",0,1,1,
										 1,L"_BEEN",0,1,1,
										 1,L"verb{vBC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										 0);
	// to have been sent 
	// L"BD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"7",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_HAVE{_BLOCK}",0,1,1,
										1,L"_BEEN",0,1,1,
										1,L"verb{vBD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										1,L"__INFP5SUB",0,0,1,
										0);
	// to be being examined
	// L"CD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"8",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_BE{_BLOCK}",0,1,1,
										1,L"_BEING{_BLOCK}",0,1,1,
										1,L"verb{vCD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										1,L"__INFP5SUB",0,0,1,
										0);
	// to have been being examined
	// L"BCD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"9",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										1,L"_HAVE{_BLOCK}",0,1,1,
										1,L"_BEEN",0,1,1,
										1,L"_BEING{_BLOCK}",0,1,1,
										1,L"verb{vBCD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										1,L"__INFP5SUB",0,0,1,
										0);
	// to being examined
	// L"vrD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFP{IVERB:_BLOCK}",L"A",1,L"to{ITO}",0,1,1,
										1,L"_ADVERB",0,0,1,
										5,L"_NOUN_OBJ{IOBJECT}",L"__NOUN[*]{IOBJECT}", // _NOUN* includes NOUN[D] and NOUN[E]
											L"there",L"demonstrative_determiner{IOBJECT}",L"possessive_determiner{IOBJECT}",0,0,1, // there was never much chance to their being given me
										1,L"_BEING{_BLOCK}",0,1,1,
										1,L"verb{vrD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
										3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,
										1,L"__INFP5SUB",0,0,1,
										0);
	/*THINKSAY*/
	// to have been thinking he should go to the store
	// L"BC" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(L"__INFPT{IVERB:_BLOCK}",L"7",1,L"to{ITO}",0,1,1,
										 1,L"_ADVERB",0,0,1,
										1,L"_HAVE{_BLOCK}",0,1,1,
										 1,L"_BEEN",0,1,1,
										 1,L"think{vBC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										 1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
										0);

	// to be him,
	cPattern::create(L"__INFP_S1",L"",2,L"__INFP",L"__INFPT",0,1,1,
												1,L",",0,1,1,0);
	// to be blended, to be hurried, and to have been washed
	cPattern::create(L"_INFP{_FINAL_IF_ALONE}",L"1",1,L"__INFP_S1",0,0,3,
										 2,L"__INFP",L"__INFPT",0,1,1,
										 1,L",",0,0,1,
										 1,L"coordinator",0,1,1,
										 2,L"_INFP",L"__INFPT",0,1,1,0);
	cPattern::create(L"_INFP{_FINAL_IF_ALONE}",L"2",1,L"_ADVERB",0,0,1,
																						 2,L"__INFP",L"__INFPT",0,1,1,0);
}

int createVerbPatterns(void)
{ LFS
	//cPattern *p=NULL;
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(L"_IS",L"1",
						1,L"_ADVERB",0,0,1,
						2,L"is{V_AGREE:V_OBJECT}",L"is_negation{not:V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	cPattern::create(L"_IS",L"2",
						1,L"_ADVERB",0,0,1,
						2,L"is{past:V_AGREE:V_OBJECT}",L"is_negation{past:not:V_AGREE:V_OBJECT}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	cPattern::create(L"_DO",L"1",
						2,L"does{imp:V_AGREE}",L"does_negation{imp:not:V_AGREE}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	cPattern::create(L"_DO",L"2",
						2,L"does{imp:past:V_AGREE}",L"does_negation{imp:past:not:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	cPattern::create(L"_HAVE",L"1",
						2,L"have{V_AGREE}",L"have_negation{not:V_AGREE}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	cPattern::create(L"_HAVE",L"2",
						2,L"have{past:V_AGREE}",L"have_negation{past:not:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	cPattern::create(L"_GET",L"1",
						1,L"_ADVERB",0,0,1,
						1,L"verb|get{V_AGREE:V_OBJECT}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	cPattern::create(L"_GET",L"2",
						1,L"_ADVERB",0,0,1,
						1,L"verb|get{past:V_AGREE:V_OBJECT}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
						3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
						0);
	// L"AL" structure of verb phrases from Quirk CGEL
	cPattern::create(L"_COND",L"",
									1,L"_ADVERB",0,0,1,
									4,L"future_modal_auxiliary{future:V_AGREE}",L"negation_future_modal_auxiliary{not:future:V_AGREE}",
										L"modal_auxiliary{conditional:V_AGREE}",L"negation_modal_auxiliary{not:conditional:V_AGREE}",0,1,1,
										3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,// preposition use should be rare!
									0);
	// MODAL of L"AL" structure of verb phrases from Quirk CGEL
		/* _COND2 is just a combination of _COND and a verbal auxiliary, which is really a verb taking a bare infinitive phrase (Pattern 4, 9.4.2.1 LGSWE) */
	cPattern::create(L"_COND2",L"",
									1,L"_ADVERB",0,0,1,
									6,L"future_modal_auxiliary{future:V_AGREE}",L"negation_future_modal_auxiliary{not:future:V_AGREE}",
										L"modal_auxiliary{conditional:V_AGREE}",L"negation_modal_auxiliary{not:conditional:V_AGREE}",
																				L"verbal_auxiliary{vS:V_AGREE}",L"past_verbal_auxiliary{past:V_AGREE}",0,1,1, // he dares eat this?
									1,L"verbal_auxiliary{V_AGREE}",0,0,1, // he would dare eat this?
									2,L"_ADVERB",L"_PP*1{_BLOCK}",0,0,2,
									0);
	// everywhere where there is _BEEN, there is an _ADVERB,not,_INTERPP before it.
	// _BEEN is never used alone without a semantic tag with it.  Therefore, no semantic tags are necessary in the pattern itself.
	cPattern::create(L"_BEEN",L"",1,L"been{V_AGREE}",0,1,1,
										1,L"_ADVERB",0,0,1,0);
	// everywhere where there is _BE, there is an _ADVERB,not,_INTERPP before it.
	cPattern::create(L"_BE",L"",
										1,L"be{id:V_AGREE}",0,1,1,
										1,L"_ADVERB",0,0,1,0);
	cPattern::create(L"_BEING",L"",1,L"being{id}",0,1,1,
										 1,L"_ADVERB",0,0,1,0);
	// I go / I go up
	// possessive_determiner is actually an adjective modifying an object missing from the sentence
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(L"_VERBPRESENT",L"1",1,L"possessive_determiner*4",0,0,1, // removed _ADVERB and added it to later patterns // the hidden object use should be very rare!
															5,L"verb{vS:V_AGREE:V_OBJECT}",L"does{vS:V_AGREE:V_OBJECT}",
															L"does_negation{vS:not:V_AGREE:V_OBJECT}",
															L"have{vS:V_AGREE:V_OBJECT}",L"have_negation{vS:not:V_AGREE:V_OBJECT}",
															VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
													 0); // preposition use should be rare!
	cPattern::create(L"_VERBPRESENTC{VERB}",L"",
											  1,L"_VERBPRESENT",0,1,1,
												2,L"coordinator|and*1",L"coordinator|or*1",0,1,1,
											  1,L"_VERBPRESENT",0,1,1,
												0);
		// eliminated as preposition is already included in _IS 2/20/2007
	//cPattern::create(L"_VERB_ID{VERB}",L"1",
	//                        1,L"_IS{vS:id}",0,1,1,
	//                        1,L"preposition*2",0,0,1,0); // preposition use should be rare!
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(L"_THINKPRESENT",L"1",
													 1,L"think{V_AGREE:vS}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
													 2,L"_ADVERB",L"_PP*1",0,0,2,0);
	cPattern::create(L"_THINKPRESENTFIRST",L"",
													 1,L"think{V_AGREE:vS}",VERB_PRESENT_FIRST_SINGULAR,1,1,
													 2,L"_ADVERB",L"_PP*1",0,0,2,0);
	// going / being / seeing
	cPattern::create(L"_VERBONGOING",L"",2,L"_ADVERB",L"possessive_determiner*2",0,0,2, // possessive determiner should be rare
													 3,L"verb{V_AGREE:V_OBJECT}",L"does{V_AGREE:V_OBJECT}",L"have{V_AGREE:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
													 0);// preposition use should be rare!
	cPattern::create(L"_THINKONGOING",L"",
													 1,L"_ADVERB",0,0,2,
													 1,L"think{V_AGREE:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
													 0);// preposition use should be rare!
	// I quickly went lurchingly
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(L"_VERBPAST{VERB}",L"",//1,L"_ADVERB",0,0,1, added ADVERB to subsequent patterns
												7,L"verb{past:V_AGREE}",
													L"does{past:V_AGREE}",L"does_negation{not:past:V_AGREE}",
													L"have{past:V_AGREE}",L"have_negation{not:past:V_AGREE}",
													L"is{past:id:V_AGREE}",L"is_negation{not:past:id:V_AGREE}",
													VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
												0);
	cPattern::create(L"_VERBPASTC{VERB}",L"",
											  1,L"_VERBPAST",0,1,1,
												2,L"coordinator|and*1",L"coordinator|or*1",0,1,1,
											  1,L"_VERBPAST",0,1,1,
												0);
	// She[tuppence] ran rather than walked down the street .
	cPattern::create(L"_VERBPASTCORR",L"", // this cannot have the VERB flag because _VERBPAST already has VERB
												1,L"_VERBPAST",0,1,1,
												1,L"adverb|rather",0,1,1,
												1,L"preposition|than",0,1,1,
												1,L"_VERBPAST{_BLOCK}",0,1,1,
												0);
	// I quickly went lurchingly
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(L"_VERBPASTPART",L"",//1,L"_ADVERB",0,0,1, added ADVERB to subsequent patterns
									 7,L"verb{V_OBJECT}",L"does{V_OBJECT}",L"does_negation{V_OBJECT:not}", // for general verbs, prefer past over past_participle, if there is a choice.
										 L"have{V_OBJECT}",L"have_negation{V_OBJECT:not}",                    // for the others, past_participle is unique, so there is no choice.
										 L"is{V_OBJECT}",L"is_negation{V_OBJECT:not}",VERB_PAST_PARTICIPLE,1,1,
									 0);
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(L"_THINKPAST",L"1",
												1,L"think{past:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
												3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,0);// preposition use should be rare!
	// She[tuppence] felt rather than saw Julius wasn't even trying to hit the ball .
	cPattern::create(L"_THINKPAST",L"2",
												1,L"think{past:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
												1,L"adverb|rather",0,1,1,
												1,L"preposition|than",0,1,1,
												1,L"think",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
												0);
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(L"_THINKPASTPART",L"",
												1,L"think*1{V_OBJECT}",VERB_PAST_PARTICIPLE,1,1, // for think, prefer past over past_participle (past is the same form as past_participle).
												3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,0);// preposition use should be rare!
	// I would go | would not go | I will go | I will not go
	// I dare say | I make do
	// I wouldn't go | I won't go
	// I would be / [I would be going (to see him) removed - covered in VERBPASSIVE[3])
	// L"A" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"1",
										1,L"_COND",0,1,1,
										2,L"_VERBPRESENT",L"_BE{vS:V_OBJECT:id}",0,1,1,0);
	// L"A" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_THINK",L"1",
										1,L"_COND2",0,1,1,
										1,L"_THINKPRESENTFIRST",0,1,1,0);
	// I do go | do not go
	// L"AL" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"3",
										1,L"_DO{imp}",0,1,1,
										1,L"_VERBPRESENT",0,1,1,0);
	// I do think
	cPattern::create(L"_THINK",L"3",
										1,L"_DO{imp}",0,1,1,
										1,L"_THINKPRESENTFIRST",0,1,1,0);
	// I am/was going
	// I am/was being
	// L"CL" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"4",
										1,L"_IS",0,1,1,
										2,L"verb{vC:V_OBJECT}",L"being{vC:id:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										0); // preposition use should be rare!
	// I am/was thinking
	cPattern::create(L"_THINK",L"4",
										1,L"_IS",0,1,1,
										1,L"think{vC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										1,L"_ADVERB",0,0,1,0);
	// I have/had gone  / (had "his indecision of character" been - REMOVED - inappropriate for this pattern) 1/11/2006
	// Bill had been
	// "B" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"5",
										1,L"_HAVE",0,1,1,
										2,L"_VERBPASTPART{vB}",L"_BEEN{vB:id}",0,1,1,0);
	// I have thought
	cPattern::create(L"_THINK",L"5",
										1,L"_HAVE",0,1,1,
										1,L"_THINKPASTPART{vB}",0,1,1,0);
	// I would/will have gone | I would/will definitely have gone
	// L"AB" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"6",
									1,L"_COND",0,1,1,
										1,L"_HAVE",0,1,1,
										2,L"_VERBPASTPART{vAB}",L"_BEEN{vAB:id}",0,1,1,0);
	// I would/will have thought
	cPattern::create(L"_THINK",L"6",
									1,L"_COND",0,1,1,
										1,L"_HAVE",0,1,1,
										1,L"_THINKPASTPART{vAB}",0,1,1,0);
	// I have been sending | I had been sending
	// L"BC" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"8",
													 1,L"_ADVERB",0,0,1,
													 1,L"_HAVE",0,1,1,
													 1,L"_BEEN",0,1,1,
													 3,L"verb{vBC:V_OBJECT}",L"does{vBC:V_OBJECT}",L"have{vBC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
													 0); // preposition use should be rare!
	// I had been telling
	cPattern::create(L"_THINK",L"8",1,L"_ADVERB",0,0,1,
														1,L"_HAVE",0,1,1,
													 1,L"_BEEN",0,1,1,
													 1,L"think{vBC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
													 2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
	// I will be sending | I would be sending | I will not be sending | I would not be sending
	// L"AC" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"9",
									1,L"_COND",0,1,1,
									1,L"_BE{_BLOCK}",0,1,1,
									3,L"verb{vAC:V_OBJECT}",L"does{vAC:V_OBJECT}",L"have{vAC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
									0); // preposition use should be rare!
	// I will be telling | I would be telling | I will not be telling | I would not be telling
	cPattern::create(L"_THINK",L"9",
									1,L"_COND",0,1,1,
									1,L"_BE{_BLOCK}",0,1,1,
									1,L"think{vAC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
									2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
	/*
	// is/was going to be | is/was reputed to be | is reputed to be running
	// this is not a verb phrase.   It is a present verb followed by an infinitive clause.
	cPattern::create(L"_VERB_ID",L"8",
										1,L"_IS",0,1,1,
										1,L"_ADVERB",0,0,2,
										1,L"verb",VERB_PRESENT_PARTICIPLE|VERB_PAST,1,1,
										1,L"to",0,1,1,
										1,L"_ADVERB",0,0,2,
										1,L"_BE",0,1,1,
										1,L"_VERBONGOING",0,0,1,
										0);
	*/
	// I would/will have been telling
	// L"ABCL" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERB{VERB}",L"A",
										1,L"_COND",0,1,1,
										1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation
										1,L"_BEEN",0,1,1,
										3,L"verb{vABC:V_OBJECT}",L"does{vABC:V_OBJECT}",L"have{vABC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										0);// preposition use should be rare!
	// I would have been telling / I will have been telling
	cPattern::create(L"_THINK",L"A",
										1,L"_COND",0,1,1,
										1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation
										1,L"_BEEN",0,1,1,
										1,L"think{vABC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										2,L"_ADVERB",L"preposition*2",0,0,1,0);// preposition use should be rare!
	// PASSIVE CONSTRUCTION
	// I am sent | I was sent
	// I got sent | I got stuck
	// I was told it was M. Fouquet .
	// L"D" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"1",
													2,L"_IS",L"_GET",0,1,1,
													1,L"_VERBPASTPART{vD}",0,1,1,0);
	cPattern::create(L"_THINKPASSIVE{VERB}",L"1",
													1,L"_IS",0,1,1,
													1,L"_THINKPASTPART{vD}",0,1,1,0);
	// I am being sent | I was being sent
	// L"CD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"2",
													 1,L"_IS",0,1,1,
													 1,L"_BEING{_BLOCK}",0,1,1,
													 1,L"_VERBPASTPART{vCD}",0,1,1,0);
	// I am being told it was M. Fouquet .
	cPattern::create(L"_THINKPASSIVE{VERB}",L"2",
													 1,L"_IS",0,1,1,
													 1,L"_BEING{_BLOCK}",0,1,1,
													 1,L"_THINKPASTPART{vCD}",0,1,1,0);
	// I will be sent | I would be sent | I will not be sent | I would not be sent
	// L"AD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"3",
													 1,L"_COND2",0,1,1,
													 1,L"_BE{_BLOCK}",0,1,1,
													 3,L"verb{vAD:V_OBJECT}",L"does{vAD:V_OBJECT}",L"have{vAD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 0); // preposition use should be rare!
	// I will be told | I would be told | I will not be told | I would not be told
	cPattern::create(L"_THINKPASSIVE{VERB}",L"3",
														1,L"_COND",0,1,1,
													 1,L"_BE{_BLOCK}",0,1,1,
													 1,L"think{vAD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
	// I have been sent | I had been sent
	// also (incorrect) I been gone
	// L"BD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"4",1,L"_ADVERB",0,0,1,
													 1,L"_HAVE",0,1,1,
													 1,L"_BEEN",0,1,1,
													 3,L"verb{vBD:V_OBJECT}",L"does{vBD:V_OBJECT}",L"have{vBD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 0); // preposition use should be rare!
	// I have been told | I had been told
	cPattern::create(L"_THINKPASSIVE{VERB}",L"4",1,L"_ADVERB",0,0,1,
													 1,L"_HAVE",0,1,1,
													 1,L"_BEEN",0,1,1,
													 1,L"think{vBD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
	// I would/will have been crushed
	// L"ABD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"5",
														1,L"_COND",0,1,1,
														1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation
													 1,L"_BEEN",0,1,1,
													 3,L"verb{vABD:V_OBJECT}",L"does{vABD:V_OBJECT}",L"have{vABD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 0); // preposition use should be rare!
	// I would/will have been told
	cPattern::create(L"_THINKPASSIVE{VERB}",L"5",
														1,L"_COND",0,1,1,
														1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation
													 1,L"_BEEN",0,1,1,
													 1,L"think{vABD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
	// I may be being examined
	// L"ACD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"6",
													 1,L"_COND",0,1,1,
																										 1,L"_BE{_BLOCK}",0,1,1,
													 1,L"_BEING{_BLOCK}",0,1,1,
													 1,L"_VERBPASTPART{vACD}",0,1,1,0);
	// I have been being examined
	// L"BCD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"7",
														1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation, past
													 1,L"_BEEN",0,1,1,
													 1,L"_BEING{_BLOCK}",0,1,1,
													 3,L"verb{vBCD:V_OBJECT}",L"does{vBCD:V_OBJECT}",L"have{vBCD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 0); // preposition use should be rare!
	// I may have been being examined
	// L"ABCD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_VERBPASSIVE{VERB}",L"8",
														1,L"_COND",0,1,1,
														1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation
													 1,L"_BEEN",0,1,1,
													 1,L"_BEING{_BLOCK}",0,1,1,
													 3,L"verb{vABCD:V_OBJECT}",L"does{vABCD:V_OBJECT}",L"have{vABCD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 0); // preposition use should be rare!
// _BE should be rarely used as an active verb by itself (we be giants)
	cPattern::create(L"__ALLVERB",L"",1,L"_ADVERB",0,0,1,
		10,L"_VERB",L"_VERBPAST{V_OBJECT}",L"_VERBPASTC{V_OBJECT}",L"_VERBPASTCORR{V_OBJECT}",L"_VERBPRESENT{VERB}",L"_VERBPRESENTC{VERB}",L"_BE*4{vS:V_OBJECT:id:VERB}",L"_VERB_BARE_INF",
				L"is{vS:id:V_AGREE:V_OBJECT:VERB}",L"is_negation{vS:id:not:V_AGREE:V_OBJECT:VERB}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
				0,1,1,0);
	// __ALLOBJECTS - should L"_ADJECTIVE" really be an object?  If so, it overlaps with _NOUN, so if it is added back this overlap
	// must be avoided.
	// _ALLOBJECTS should represent all cases of an object of a verb
	// speak/say - add, admit, announce, answer, argue, assert, ask, beg, boast, claim, comment, conclude, confess, cry,
	//             declare, exclaim, explain, hint, insist, maintain, not, object, observe, order, promise, protest, recall,
	//             remark, repeat, reply, report, say, shout, state, tell, think, urge, warn, whisper, wonder, write
	//             falter, mumble, murmur, mutter, snap, sneer, sob

	return 0;
}

void createSecondaryPatterns1(void)
{ LFS
	//cPattern *p=NULL;
	// introduced because otherwise __INTERPP can hide a noun inside itself and be missed
	// by considering the entire APPNOUN as an object in identifyObjects.  But if APPNOUN is not considered as an object,
	// and without the following pattern, __INTERPP will be identified but not the beginning noun.
	cPattern::create(L"__APPTCNOUN{NOUN:EVAL}",L"",
		4,L"determiner*2{DET}",L"demonstrative_determiner*2{DET}",L"possessive_determiner*2{DET}",L"__HIS_HER_DETERMINER*3",0,0,1,
		1,L"_ADJECTIVE_AFTER",0,0,2, // this cannot contain VERBPAST, as it is not possible yet to ascertain the difference between this and _S1.
																// otherwise, _NOUN would win over _S1 because it has fewer subcomponents, which was incorrect.
		4,L"noun*4{SUBJECT:N_AGREE}",L"_NAME*4{SUBJECT:GNOUN:NAME}",L"personal_pronoun_nominative*3{SUBJECT:N_AGREE}",L"personal_pronoun*3{SUBJECT:N_AGREE}",NO_OWNER,1,1, // this is tightly controlled -
		0);
	// this pattern contains a relative clause!
	cPattern::create(L"__APPNOUN{EVAL}",L"2",
		1,L"__APPTCNOUN{SUBJECT}",0,1,1, 
		2,L"__INTERPP[*]*1{_BLOCK}",L"__INTERPPB[*]*1{_BLOCK}",0,0,1,
			 // no pronouns that could be adjectives -
						 // previously incorrect - 'round and lowered his' with 'round and lowered' being an adjective and 'his' being a noun!
						 // then the next noun would be considered a secondary noun, which is incorrect. 'looked round and lowered his voice'
		2,L"__ALLVERB",L"_VERBPASSIVE",0,1,1,     // the news I brought
		//2,L"preposition*4",L"_ADVERB*2",0,0,1, // hanging preposition BNC A01 L"the power I dreamed of" already in ALLOBJECTS
		2,L"__ALLOBJECTS_0*2",L"__ALLOBJECTS_1*2",0,0,1, // the books I bought Bob
		0);
	//cPattern::create(L"__APPNOUN2{NOUN}",L"",
	//  4,L"determiner*2{DET}",L"demonstrative_determiner*2{DET}",L"possessive_determiner*2{DET}",L"__HIS_HER_DETERMINER*3",0,0,1,
	//  1,L"_ADJECTIVE_AFTER*2",0,0,2, // this cannot contain VERBPAST, as it is not possible yet to ascertain the difference between this and _S1.
	//                                // otherwise, _NOUN would win over _S1 because it has fewer subcomponents, which was incorrect.
	//  4,L"noun*4{SUBJECT:N_AGREE}",L"_NAME*6{SUBJECT:GNOUN}",L"personal_pronoun_nominative*3{SUBJECT:N_AGREE}",L"personal_pronoun*3{SUBJECT:N_AGREE}",NO_OWNER,1,1, // this is tightly controlled -
	//  1,L"__INTERPP[*]{_BLOCK}",0,0,1,
			 // no pronouns that could be adjectives -
			 // previously incorrect - 'round and lowered his' with 'round and lowered' being an adjective and 'his' being a noun!
			 // then the next noun would be considered a secondary noun, which is incorrect. 'looked round and lowered his voice'
	//  2,L"__ALLVERB",L"_VERBPASSIVE",0,1,1,     // the news I brought
	//    1,L"__ALLOBJECTS",0,0,1,
	//  0);
	// REPLACED BY __NOUN L"5L"
	//cPattern::create(L"__NOUN{_FINAL_IF_ALONE}",L"X",
	//                  1,L"__PNOUN",0,1,1,
	//                  1,L"__APPNOUN2{_BLOCK:RE_OBJECT}",0,1,1,
	//                  0);

	// encourage prepositional phrases to not have objects for the main noun after the prep phrase
	// by making the PP in ALLOBJECTS[1] very expensive.
	// I will give from the store Mark the book.
	cPattern::create(L"__ALLOBJECTS_0",L"1",
						// included adjective because we still want to have the possibility of multiple
						// adjectives and other possibilities included.  However, because a verb is included in
						// _ADJECTIVE, we must make this choice expensive - but not more than 1 (6/2/2007)
						// _PP already has a preposition and an adverb as its first elements (usually)
						4,L"_ADVERB{ADVOBJECT}",L"_PP",L"preposition*2",L"_ADJECTIVE*1{ADJOBJECT}",0,1,2, // hanging preposition! must be 2 because otherwise combinations are blocked!
						0);
	// a copy of __ADJECTIVE, without coordinators or commas and without a verb (because this would be directly after
	// another verb which is really not allowed
	cPattern::create(L"__ADJECTIVE_WITHOUT_VERB",L"",
								1,L"_ADVERB",0,0,1,
								6,L"adjective",L"numeral_ordinal",L"_NUMBER",L"ex",L"noun*1",L"_NAMEOWNER*2",VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,1,1,
								1,L"adverb*1",0,0,1,
						0);
	cPattern::create(L"__ALLOBJECTS_0",L"2",1,L"__ADJECTIVE_WITHOUT_VERB{ADJ:ADJOBJECT}",0,1,1,0);
	// this is if a sentence has a prepositional phrase followed by an adverb and another prepositional phrase.  
	//   this pattern is necessary because a sentence can end like this and if the prep phrase is short enough, and the prep is also a separator, the 
	//   prep phrase is cut off from the rest of the sentence but because the word is not a separator after parsing is concluded it causes the __S1 pattern
	//   to be removed because it no longer follows the _FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN requirement
	cPattern::create(L"__ALLOBJECTS_0",L"3",
						// included adjective because we still want to have the possibility of multiple
						// adjectives and other possibilities included.  However, because a verb is included in
						// _ADJECTIVE, we must make this choice expensive - but not more than 1 (6/2/2007)
						// _PP already has a preposition and an adverb as its first elements (usually)
						1,L"_PP",0,1,1, // hanging preposition! must be 2 because otherwise combinations are blocked!
						1,L"_ADVERB{ADVOBJECT}",0,1,1,
						1,L"_PP",0,1,1,
						0);
	// NEGATION
	// CGEL 10.54-10.70
	// encourage verbs to group their nouns (without using NOUN[5], which is set considerably higher in cost)
	// (_NOUN[9] includes _INFP as a post-modifier)
	// I want you to take Mrs. Smith back.
	// I want Bill to remember to thank Mrs. Smith for taking us back today.
	// I use language to suit the occasion.
	cPattern::create(L"__ALLOBJECTS_1",L"1",
						2,L"_ADVERB",L"_PP*4",0,0,1, // removed L"preposition*2", because there is no difference at this level of processing between
																			 // L"hang up" the phone and L"hang" up the phone, so resolve to a prepositional phrase and
																			 // then reprocess later to analyze which prepositions actually belong with their verbs.
																			 // __PP already has an adverb as the first element.
						5,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",L"__MNOUN[*]{OBJECT}",L"__NOUNREL*3{OBJECT}",L"__NOUNRU{OBJECT}",0,1,1,
						0);
	// I want to remember to thank Mrs. Smith for taking us back today.
	// the kind of man who would be afraid to meet death!
	cPattern::create(L"__ALLOBJECTS_1",L"2",
						4,L"_ADVERB{ADVOBJECT}",L"_PP*1",L"preposition*2",L"_ADJECTIVE*1{ADJOBJECT}",0,0,2, // hanging preposition!
						//2,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",0,0,1, // Bill
						2,L"_INFP*1",L"_REL1[*]",0,1,1, // to remember (with its object which is [to thank Mrs. Smith for taking us back today])
						0);
	// by creating a second pattern for two objects, we push one object or two objects up another notch to make it more costable
	// by secondaryCosts
	cPattern::create(L"__ALLOBJECTS_2",L"",
						2,L"_ADVERB",L"_PP*4",0,0,1, // removed L"preposition*2", because there is no difference at this level of processing between
																			 // L"hang upL" the phone and L"hangL" up the phone, so resolve to a prepositional phrase and
																			 // then reprocess later to analyze which prepositions actually belong with their verbs.
																			 // __PP already has an adverb as the first element.
						3,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",L"__MNOUN[*]{OBJECT}",0,1,1,
						2,L"_ADVERB*1",L"preposition*2",0,0,1, // hanging preposition!
						4,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",L"__MNOUN[*]{OBJECT}",L"__NOUNREL{OBJECT}",0,1,1,
						0);
	// Thank you , ma'am
	cPattern::create(L"_NAME_INJECT",L"1",
									 1,L",",0,1,1,
									 2,L"_NAME{HAIL}",L"_META_GROUP{HAIL}",0,1,1,0);
	cPattern::create(L"_PRE_VR1{_ONLY_BEGIN_MATCH}",L"",
									 4,L"polite_inserts",L"_ADVERB",L"_PP",L"_NAME",0,0,1, // pray / please / Yes (ADVERB)
									 1,L",",0,1,1,  // _ADVERB is a redundant prefix to __ALLVERB - comma must be non-optional
									 0);
	cPattern::create(L"_VERBPRESENT_CO",L"",
												1,L"_VERBPRESENT",0,1,1,
												1,L"coordinator",0,1,1,
												1,L"_VERBPRESENT{VERB}",0,1,1,
												0);
	// trained to lock and unlock
	// were pushing on to the ... / be easy!
	// shall press you ***
	// had not completed my letter that night.
	// do you? / have you? / are you? / did you / didn't you?
	cPattern::create(L"_VERBREL1{_FINAL:_ONLY_BEGIN_MATCH:_BLOCK:EVAL}",L"1",
									 2,L"_PRE_VR1",L"polite_inserts",0,0,1, // _PP removed - In a moment Tommy was inside. - Is moment an adjective OR noun?
									 2,L"__ALLVERB",L"_VERBPRESENT_CO",0,1,1,  //            _PP               VERBREL
									 // removed L"not{not}",L"never{never}", 7/14/2006 - already included in ADVERB at the end of each subcomponent of __ALLVERB
									 // removed REL1 and _PP - included with __ALLOBJECTS
									 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // L"there{OBJECT}L" removed
																		 // ADVERB made more expensive because ALLOBJECTS already has ADVERB 2/17/2007
									 1,L"_NAME_INJECT",0,0,1, // Thank you , ma'am                 // removed _PP from above because __ALLOBJECTS already has _PP
									 0);
	cPattern::create(L"_VERBREL1{_FINAL:_ONLY_BEGIN_MATCH:_BLOCK:EVAL}",L"2",
									 2,L"_PRE_VR1",L"polite_inserts",0,0,1,
									 1,L"__ALLTHINK*1{VERB}",0,1,1, // L"think" for all active tenses! - possibly add INTRO_S1.
									 2,L"__NOUN[*]{OBJECT}",L"_NOUN_OBJ{OBJECT}",0,0,1,
									 1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									 1,L"_NAME_INJECT",0,0,1,
									 0);
	// only after quotes
	cPattern::create(L"_VERBREL1{_FINAL:_ONLY_BEGIN_MATCH:_BLOCK:EVAL:_AFTER_QUOTE}",L"3",
									 1,L"_VERBPAST",0,1,1,  
									 2,L"__NOUN[*]{SUBJECT}",L"__NOUNREL{SUBJECT}",0,1,1, 
									 0);
	// another voice which 'Tommy rather thought' was that of Boris replied:
	cPattern::create(L"_SUBREL{_BLOCK}",L"",
									 1,L"__C1__S1",0,1,1,
									 1,L"__ALLTHINK",0,1,1,
									 0);
	// _REL1 "2", "3" and "4" use __S1 and are defined later...
	// 'what had been written' was plain to me
	// which underlay his tone / that had been written to him
	// this is a copy of __S1[4] with relativizer as SUBJECT
	cPattern::create(L"_REL1{_BLOCK:REL}",L"1",
										1,L"_ADJECTIVE",0,0,1,
										2,L"relativizer{SUBJECT}",L"demonstrative_determiner|that{SUBJECT}",0,1,1,
									 3,L"_PP",L"_REL1[*]",L"_SUBREL",0,0,1,
									 1,L"_ADJECTIVE",0,0,1,
									 2,L"__SUB_S2",L"_VERBPASSIVE_P",0,1,1,
									 0);
	// this is a copy of __S1[1] with relativizer as SUBJECT
	cPattern::create(L"_REL1{_BLOCK:REL}",L"5",
										1,L"_ADJECTIVE",0,0,1,
										2,L"relativizer{SUBJECT}",L"demonstrative_determiner|that{SUBJECT}",0,1,1,
									 3,L"_PP",L"_REL1[*]",L"_SUBREL",0,0,1,
									 2,L"__ALLVERB",L"_COND{VERB}",0,1,1,
									 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
									 0);
	createBareInfinitives();


	// having sent
	// having given Bill the book
	cPattern::create(L"__VERBREL2{VERB}",L"1",
									 1,L"_ADVERB",0,0,1,
									 1,L"have",VERB_PRESENT_PARTICIPLE,1,1,
									 1,L"_PP",0,0,1, // having in his turn failed to persuade Tuppence...
									 1,L"_ADVERB",0,0,2,
									 2,L"_VERBPASTPART{vrB}",L"_BEEN{vrB:V_OBJECT}",0,1,1,
									 0);
	// , having been giving Bob the book
	cPattern::create(L"__VERBREL2{VERB}",L"2",
									 1,L"_ADVERB",0,0,1,
									 1,L"have",VERB_PRESENT_PARTICIPLE,1,1,
									 1,L"_PP",0,0,1, // having in his turn failed to persuade Tuppence...
									 1,L"_ADVERB",0,0,1,
									 1,L"_BEEN",0,1,1,
									 1,L"_ADVERB",0,0,2,
									 3,L"verb{vrBC:V_OBJECT}",L"does{vrBC:V_OBJECT}",L"have{vrBC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
									 0);
	// , spreading over its Gothic houses, (VERBONGOING)
	// , spread over its
	// , being over the highest peak
	// , having the highest salary
	// giving bill the book
	cPattern::create(L"_VERBREL2{_FINAL_IF_ALONE}",L"1",
						2,L"_VERBONGOING{vE:VERB}",L"__VERBREL2",0,1,1,
						3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1,0);
	// PASSIVE
	// , having been spread
	// , having been given
	// , having not been spread
	cPattern::create(L"__PVERBREL2{VERB}",L"1",
									 1,L"_ADVERB",0,0,1,
									 1,L"have",VERB_PRESENT_PARTICIPLE,1,1,
									 1,L"_ADVERB",0,0,1,
									 1,L"_BEEN",0,1,1,
									 1,L"_ADVERB",0,0,2,
									 1,L"_VERBPASTPART{vrBD}",0,1,1,0);
	// PASSIVE
	// , being informed of the risks
	// , being given the book by Bill
	cPattern::create(L"__PVERBREL2{VERB}",L"2",
									 1,L"_ADVERB",0,0,1,
									 1,L"being",VERB_PRESENT_PARTICIPLE,1,1,
									 1,L"verb{vrD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,0);

	cPattern::create(L"_VERBREL2{_FINAL_IF_ALONE}",L"2",
									 1,L"__PVERBREL2",0,1,1,
									 2,L"_PP",L"_REL1[*]",0,0,1,
									 // verbs in passive clauses only have one object maximum
									 5,L"__NOUN[*]{OBJECT}",L"__MNOUN[*]{OBJECT}",L"_NOUN_OBJ{OBJECT}",L"__NOUNREL{OBJECT}",L"_ADJECTIVE",0,0,1,
									 1,L"_ADVERB",0,0,1,
									 0);
}

void createPrepositionalPhrases(void)
{ LFS
	//cPattern *p=NULL;
	/*
	REMOVED - for is not used as a preposition in this pattern!
	// VERBREL2: particularly for (him, your, Bill's, Bill, the Catcher) being from NYC
	// VERBREL3: particularly for (him, your, Bill's, Bill, the Catcher) having been sent / between having been sent the book
	// VERBREL4: particularly for (him, your, Bill's, Bill, the Catcher) having sent / between having been the catcher
	// VERBREL5: particularly for (him, your, Bill's, Bill, the Catcher) being informed of the risks
	cPattern::create(L"__PP",L"2",1,L"_ADVERB",0,0,1,
									 1,L"preposition",0,1,1,
									 1,L"_ADVERB",0,0,1,
									 4,L"possessive_determiner",//L"demonstrative_determinerL"(included as a _NOUN),L"quantifier", (included as _N1)
										 L"interrogative_determiner",L"_NOUN_OBJ",L"__NOUN",0,0,1,
									 2,L"_VERBREL2",L"_VERBONGOING",0,1,1,
									 0);
	*/
	// of Mrs. Vandermeyer's.
	cPattern::create(L"__PP",L"2",1,L"of{P}",0,1,1,
											 1,L"__NAMEOWNER{PREPOBJECT}",0,1,1,0);
	// He loved himself (himself should be a direct object, not a PP)
	cPattern::create(L"__PP",L"3",1,L"reflexive_pronoun*1",0,1,1,0); // prefer reflexive pronoun be a direct object than a preposition.

	// in which ... / for which...
	// by which you intend to leave the town
	cPattern::create(L"__PP",L"4",2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
									1,L"interrogative_determiner",0,1,1,
									1,L"__S1{PREPOBJECT:OBJECT:EVAL:_BLOCK}",0,1,1,0);
	// for he will be strong. INVALID for is not a preposition in this sentence
	//cPattern::create(L"__PP",L"5",1,L"preposition",0,1,1,
	//                1,L"__S1",0,1,1,0);
	// at old Red's
	cPattern::create(L"__PP",L"6",1,L"_ADVERB*1",0,0,1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
									 1,L"preposition*1{P}",0,0,1, // from within
									 1,L"determiner",0,0,1, // the
									 1,L"Proper Noun{PREPOBJECT}",SINGULAR_OWNER|PLURAL_OWNER,1,1,
									 0);
		// copy of __PP 9
	//cPattern::create(L"__PP",L"7",1,L"_ADVERB",0,0,1,
	//                 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
	//                 1,L"preposition*1{P}",0,0,1, // from within
	//                 1,L"_ADVERB*3",0,0,1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
	//                 1,L"_VERBREL2{_BLOCK:EVAL}",0,1,1,  // we can keep the fact -of having done so quite secret .
	//                 0);
	// there was never much chance of -their ‘ letting me go ’
	cPattern::create(L"__PP",L"8",1,L"_ADVERB*1",0,0,1,  // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
									 1,L"preposition*1{P}",0,0,1, // from within
									 1,L"_ADVERB*3",0,0,1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
									 6,L"_NOUN_OBJ{PREPOBJECT}",L"__NOUN[*]{PREPOBJECT}",L"__MNOUN[*]{PREPOBJECT}", // _NOUN* includes NOUN[D] and NOUN[E]
										 L"there",L"demonstrative_determiner",L"possessive_determiner",0,0,1, // there was never much chance of -their ‘ letting me go ’
									 1,L"quotes",OPEN_INFLECTION,1,1,
									 1,L"_VERBREL2{_BLOCK:EVAL}",0,1,1,  //  the fact -of -them 'having done so'
									 1,L"quotes",CLOSE_INFLECTION,1,1,
									 0);
	cPattern::create(L"__PP",L"9",1,L"_ADVERB*1",0,0,1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
									 1,L"preposition*1{P}",0,0,1, // from within
									 1,L"_ADVERB*3",0,0,1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
									 6,L"_NOUN_OBJ{PREPOBJECT}",L"__NOUN[*]{PREPOBJECT}",L"__MNOUN[*]{PREPOBJECT}", // _NOUN* includes NOUN[D] and NOUN[E]
										 L"there",L"demonstrative_determiner{PREPOBJECT}",L"possessive_determiner{PREPOBJECT}",0,0,1, // there was never much chance of -their ‘ letting me go ’
									 1,L"_VERBREL2*2{_BLOCK:EVAL}",0,1,1,  // we can keep the fact -of having done so quite secret . / the fact -of -them having done so
									 0);
		cPattern::create(L"_PP{_FINAL:_NO_REPEAT}",L"1",1,L"__PP[*]{PREP:_BLOCK}",0,1,5,0);
	// NOTE reflexive_pronouns, possessive_pronouns and reciprocal_pronouns can also be used as _NOUN_OBJ
	// This pattern should not be repeated because _NOUN and _NOUN_OBJ already have _PP as a follow.
	// in the sentence below, the _PP containing moment or two Tommy's indignation should not be included
	// After a moment or two Tommy's indignation got the better of him.
	cPattern::create(L"_PP{_FINAL_IF_ALONE:_BLOCK:PREP:_NO_REPEAT}",L"2",
									 1,L"_ADVERB*1",0,0,1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,  // this should be a particle
									 1,L"preposition{P}",0,0,1, // from within
									 1,L"_ADVERB*3",0,0,1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
									 // Under the original act, how many judges were to be on the court? - __NOUNREL should have at least a cost of 2 because it matches too much (with its comma)
									 7,L"_NOUN_OBJ{PREPOBJECT}",L"__NOUN[*]{PREPOBJECT}",L"__MNOUN[*]{PREPOBJECT}",L"__NOUNREL*4{PREPOBJECT}",L"there",L"_ADJECTIVE[*]*4",L"__NOUNRU{PREPOBJECT}",0,1,1,  // _NOUN* includes NOUN[D] and NOUN[E]
									 0);
	// We are nearer to finding Tuppence.
	//  This is not an infinitive phrase.  'to' is a participle, and finding is an verb with a subject Tuppence
	// it is not a prepositional phrase either, but it is a convenient place to put this.
	cPattern::create(L"_PP{_FINAL:_BLOCK:_NO_REPEAT}",L"3",
									 1,L"to",0,1,1,
									 1,L"_VERBONGOING{VERB:vE}",0,1,1,
									 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,1,1, // there must only be one adjective and it must be last (not mixed in) see *
									 0);
	// in case Albert should miss it. (special case?)
	cPattern::create(L"_PP{_FINAL:_BLOCK:_NO_REPEAT}",L"4",
									 1,L"preposition|in",0,1,1,
									 1,L"noun|case",0,1,1,
									 1,L"__S1{PREPOBJECT:OBJECT:EVAL:_BLOCK}",0,1,1,0);

}

int createSecondaryPatterns2(void)
{ LFS

		// C1_IP keeps the subject/verb together by chopping out the comma and verbal phrase
	// knowing full well the consequences
	// known to her intimate friends for some mysterious reason as L"Tuppence.L"
	// given her book by some of the greatest artists known.
	// My son, known throughout the lands as a great conquerer
	cPattern::create(L"__C1_IP",L"1",
								 1,L",",0,1,1,
								 1,L"_VERBREL2{_BLOCK:EVAL}",0,1,1,
								 1,L",",0,0,1,
									0);
	cPattern::create(L"__C1_IP{_BLOCK:EVAL}",L"2",
								 1,L",",0,1,1,
								 1,L"_ADVERB",0,0,1, // _VERBPASTPART does not have an adverb
								 2,L"_VERBPASTPART",L"being{vC:id:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
								 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,1,1, // added _PP 8/28/2006
								 1,L",",0,0,1,
									0);
	// restatement / this woman, whoever she was, was saved
	// this woman, master of disguise,
	cPattern::create(L"__C1_IP{_BLOCK:EVAL}",L"3",
									 1,L",",0,1,1,
									 3,L"__NOUN[*]{_BLOCK:RE_OBJECT:NAME_SECONDARY}",L"__MNOUN[*]{_BLOCK:RE_OBJECT:NAME_SECONDARY}",L"_REL1[*]",0,1,1,
									 1,L",",0,1,1, // period deleted because of the '.' at end - swallows up the period
									 0);
	// This woman, as an American, would always prevail.
	cPattern::create(L"__C1_IP{_BLOCK:EVAL}",L"4",
									 1,L",",0,1,1,
									 1,L"as",0,1,1,
									 1,L"__NOUN[*]{_BLOCK:RE_OBJECT}",0,1,1,
									 1,L",",0,1,1, 
									 0);
	// A slatternly woman, with vicious eyes, opened the door.
	cPattern::create(L"__C1_IP{_BLOCK:EVAL}",L"5",
									 1,L",",0,1,1,
									 1,L"_PP{_BLOCK}",0,1,1,
									 1,L",",0,1,1, 
									 0);
	// Albert, still round-eyed, demanded breathlessly:
	cPattern::create(L"__C1_IP{_BLOCK:EVAL}",L"6",
									 1,L",",0,1,1,
									 1,L"_ADJECTIVE",0,1,1,
									 1,L",",0,1,1, 
									 0);
	// INFP should be rare// REL1 should be rare// VERBREL2 should be rare
	// _ADJECTIVE should also cover NAMEOWNER
	// there added to prevent "but of Jane Finn there was no mention" - 'there' should be a SUBJECT
	// overwhelmingly 'there' may be parsed as an adverb (which it is), but then not matched to C1__S1 and then
	// parsed as a VERB clause, which then is more expensive.
	cPattern::create(L"__C1__S1",L"1",13,
				L"__NOUN[*]{SUBJECT}",L"__MNOUN[*]{SUBJECT}",L"_INFP*2{GNOUN:SINGULAR:SUBJECT}",L"interrogative_pronoun{N_AGREE:SINGULAR:SUBJECT}",
				L"interrogative_determiner{N_AGREE:SINGULAR:SUBJECT}", 
				L"adverb|there*-1{N_AGREE:SINGULAR:PLURAL:SUBJECT}",L"adverb|here*-1{N_AGREE:SINGULAR:PLURAL:SUBJECT}",
				L"_REL1[*]*2{SUBJECT:GNOUN:SINGULAR}",L"_ADJECTIVE*1{SUBJECT:GNOUN}",
				L"_VERBREL2*2{SUBJECT:GNOUN:SINGULAR:_BLOCK:EVAL}",L"__QSUBJECT{SUBJECT:GNOUN:SINGULAR}",L"__NOUNRU{SUBJECT}",
				L"noun{SUBJECT:N_AGREE}",SINGULAR_OWNER|PLURAL_OWNER,1,1, // Poirot's were pleasantly vague .
			 1,L"__INTERPPB",0,0,1,
			 1,L"__C1_IP",0,0,1, // her father, Neptune, lives in a beautiful castle
			0);
	// with him was the evil-looking Number 14.
	// Beside him stood Annette.
	// Through Tommy's mind[mr] flashed the assurance:
	// on one side was ...
	cPattern::create(L"__C1__S1",L"2",
				4,L"preposition|with*1",L"preposition|beside*1",L"preposition|through",L"preposition|on*2",0,1,1, // rare pattern 
				8,L"adjective{ADJ}",L"verb*1{ADJ}",L"numeral_ordinal{ADJ}",L"_NUMBER{ADJ}",L"ex{ADJ}",L"noun{ADJ}",L"no{ADJ:no}",L"__NAMEOWNER", // removed *1 as cost so that
					VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE|SINGULAR_OWNER|PLURAL_OWNER,0,1,                            // we can make cost of *1 for __NOUN[2]
					4,L"__N1{SUBJECT:NOUN}",L"_NAME{GNOUN:NAME:SUBJECT}",L"_NOUN_OBJ{SUBJECT:NOUN}",L"personal_pronoun_accusative{SUBJECT:NOUN}",0,1,1,                 
			 1,L"__C1_IP",0,0,1,
			0);
	// my dear / my lord
	cPattern::create(L"_END_HAIL_OBJECT{_ONLY_END_MATCH}",L"",
									 1,L"possessive_determiner|my",0,1,1, // my dear
									 3,L"honorific{HON:HAIL}",L"noun{HAIL}",L"adjective|dear",MALE_GENDER|FEMALE_GENDER,1,1,
									0);
	cPattern::create(L"__CLOSING__S1{_ONLY_END_MATCH}",L"1",
									 1,L",",0,1,1, // , ma'am // if this is made optional, _NOUN of C4 and _ALLOBJECT of C3 are identical
									 6,L"_NAME{HAIL}",L"honorific{HON:HAIL}",L"_HON_ABB{HON:HAIL}",L"_META_GROUP{HAIL}",L"noun{HAIL}",L"_END_HAIL_OBJECT{HAIL|OBJECT}",MALE_GENDER|FEMALE_GENDER,1,1, // , sir / , freak! noun includes _NAME, L"honorific",
									0);
	cPattern::create(L"__CLOSING__S1{_ONLY_END_MATCH}",L"2",
									 1,L",",0,0,1,
									 2,L"__INFPSUB*1",L"__INFP3SUB*1",0,1,1, // prefer __S1 to __INFP?SUB - verbs in INFP?SUB must agree with _INFP as object of verb!
									 0);
	cPattern::create(L"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH:FLOATTIME}",L"3",
									 6,L"_ADVERB",L"__ADJECTIVE_WITHOUT_VERB*1",L"_ADJECTIVE*4",L"preposition*2",L"_TIME",L"_DATE",0,1,1, // dangling adverb or adjective // dangling adverb or adjective should be taken only if there is no other match
									 0);
	// already taken by _TIME
	//cPattern::create(L"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}",L"4",
	//                 2,L"demonstrative_determiner{DET}",L"quantifier{DET}",0,0,1,
	//                 1,L"timeUnit",0,1,1,
	//                 0);
		// said Tuppence, her spirits rising.
	cPattern::create(L"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}",L"5",
									 1,L",",0,1,1,
									 1,L"__NOUN[*]{SUBJECT}",0,1,1,
									 1,L"_VERBONGOING{vE:VERB}",0,1,1,
									 0);
		// Boris remarked, glancing at the clock.
		// I ran to the bookstore, yelling all the way.
		// He wandered to close to the woods, wandering into the pit.
	cPattern::create(L"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}",L"6",
									 1,L",",0,1,1,
									 1,L"_VERBREL2{_BLOCK:EVAL}",0,1,1,
									 0);
	// I would like to meet him -- Mr. Brown
	cPattern::create(L"__CLOSING__S1{_ONLY_END_MATCH}",L"7",
									 1,L"dash",0,1,1, // , ma'am // if this is made optional, _NOUN of C4 and _ALLOBJECT of C3 are identical
									 3,L"_NAME",L"honorific{HON}",L"_HON_ABB{HON}",MALE_GENDER|FEMALE_GENDER,1,1, // , sir / , freak! noun includes _NAME, L"honorific",
									 0);

	// DECLARATIVES p.803 CGEL
	// The simple sentence (p. 754)
	// SVC, SVA, SV, SVO, SVOC, SVOA, SVOO
	// SVC: Subject, Verb, subject Complement
	// SVA: Subject, Verb, Adverbial
	// not only ..., but
	// he/The sheriff throws/threw the ball/him/himself off the porch
	// what threw him off the porch?
	cPattern::create(L"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}",L"1",
									 1,L"__C1__S1",0,1,1,
									 2,L"__ALLVERB",L"_COND{VERB}",0,1,1,
									 1,L":",0,0,1, // and they were: "dasher", "prancer", and "comet".  / ALSO another voice[boris] which Tommy rather thought was that[boris] of boris replied :
									 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
									 1,L"__CLOSING__S1",0,0,3,
									 0);
	// __NOUN[D], __NOUN[E], and __NOUN[F] will function as subjects or objects by themselves.
	// they are split this way so that only "E" and "F" are checked for _AGREEMENT, and only "D" and "F" are checked for verb usage.
	// Otherwise, excessive time is wasted.
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:_FORWARD_REFERENCE:_BLOCK:GNOUN:VNOUN}",L"D",
									 1,L"_VERBONGOING*1{VERB:vE}",0,1,1,  // from C2__S1 - also matches _N1// this pattern should not be common
									 //1,L"_INFP{OBJECT}",0,0,1,         // from C2__S1   RINFP 6/7/06
									 //1,L"_PP",0,0,1,         // from C2__S1 - __PP after noun is absorbed into the noun itself through NOUN 9
									 // if the following is made optional this pattern can match _NOUN[9] with an embedded _NOUN[2]
									 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,1,1, // there must only be one adjective and it must be last (not mixed in) see *
									 0);
	// 'the man spending money' was their target.
	// pattern is not grouped with its other root patterns
	/* RINFP - INFP removed after VERB and placed in ALLOBJECTS as an object of the verb.
		 if a verb has an infinitive phrase after it, objects occurring after to-verb belong to the infinitive phrase NOT the main verb.
		 only one object is permitted before the infinitive phrase, and that is optional
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:_FORWARD_REFERENCE:_BLOCK:GNOUN:VNOUN}",L"E",
									 // if the following is made optional this pattern can match _NOUN[9] with an embedded _NOUN[2]
									 1,L"__C1__S1",0,1,1, // changed to cost *2 5/17 to avoid being the same cost as _NOUN[2]
									 1,L"_VERBONGOING*2{VERB:vE}",0,1,1,  // from C2__S1 - also matches _N1// this pattern should not be common
									 1,L"_INFP{OBJECT}",0,0,1,         // from C2__S1
									 2,L"_PP",L"_REL1[*]{_BLOCK}",0,0,1,         // from C2__S1 - __PP after noun is absorbed into the noun itself through NOUN 9
									 0);
 */
	cPattern::create(L"__NOUN{_FINAL_IF_ALONE:_FORWARD_REFERENCE:_BLOCK:GNOUN:VNOUN}",L"F",
									 1,L"__C1__S1",0,1,1, // changed to cost *2 5/17 to avoid being the same cost as _NOUN[2]
									 1,L"_VERBONGOING*1{VERB:vE}",0,1,1,  // from C2__S1 - also matches _N1// this pattern should not be common
									 //1,L"_INFP{OBJECT}",0,0,1,         // from C2__S1   RINFP 6/7/06
									 //1,L"_PP",0,0,1,         // from C2__S1 - __PP after noun is absorbed into the noun itself through NOUN 9
									 // if the following is made optional this pattern can match _NOUN[9] with an embedded _NOUN[2] / this can still match because of __ALLOBJECTS_0
									 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1*1",L"__ALLOBJECTS_2*1",0,1,1, // there must only be one adjective and it must be last (not mixed in) see *
									 0);

	// * When _ALLOBJECT was a single object:
	//     adjective must not be in __ALLOBJECT because __ALLOBJECT is repeated.  When this happens the adjective belonging to
	//     a noun is both an __ALLOBJECT and part of a _NOUN, which is itself an __ALLOBJECT, leading to a double match.
	//     absorbed was the king in all his splendor.
	/*
		should be processed by __S1[1] because C1__S1 now recognizes all adjectives.
	cPattern::create(L"__S1{_FINAL}",L"2",
									 1,L"_ADJECTIVE{SUBJECT:GNOUN}",0,1,1,
									 3,L"does{VERB:vS:V_AGREE}",L"have{VERB:vS:V_AGREE}",L"is{VERB:vS:V_AGREE}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
									 2,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",0,1,1, // converted from __ALLOBJECTS because the previous list of verbs have one object maximum.
									 1,L"_ADJECTIVE",0,0,1,
									 0);

	cPattern::create(L"__S1{_FINAL}",L"3",
									 1,L"_ADJECTIVE{SUBJECT:GNOUN}*2",0,1,1,   // this pattern should be very rare
									 3,L"does{VERB:past:V_AGREE}",L"have{VERB:past:V_AGREE}",L"is{VERB:past:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
									 2,L"_ADVERB",L"preposition*2",0,0,2, // hanging preposition!
									 5,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",L"_INFP{OBJECT}",L"_PP",L"_ADJECTIVE",0,1,1, // converted from __ALLOBJECTS because the previous list of verbs have one object maximum.
									 1,L"_ADJECTIVE",0,0,1,
									 0);
	*/
	// treated similarly to VERBREL
	cPattern::create(L"_VERBPASSIVE_P{_FINAL_IF_ALONE}",L"1",
									 // VERBPASTPART added 8/29/2006 to handle coordinators
									 // they must be given the facts about AIDS and given -the -opportunity to discuss the issues
									 1,L"_VERBPASSIVE",0,1,1, // passive verbs take only one direct object (MAX) // john could be called -a -brilliant -conversationalist .
												 2,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",0,0,1,0);
	cPattern::create(L"_VERBPASSIVE_P{_FINAL_IF_ALONE}",L"2",
									 1,L"_THINKPASSIVE",0,1,1, // passive verbs take only one direct object (MAX) // I was told it was M . Fouquet .
									 2,L"_ADVERB",L"preposition*2",0,0,2, // hanging preposition!
									 1,L"__S1[*]{_BLOCK:OBJECT:EVAL}",0,1,1,
									 0);
	// VERBPASTPART added 8/29/2006 to handle coordinators
	// but 'forgotten to write it on the chart.' Secret Adversary
	cPattern::create(L"_VERBPASSIVE_ACP{_FINAL_IF_ALONE}",L"1",
									 // they must be given the facts about AIDS and given -the -opportunity to discuss the issues
									 1,L"_VERBPASTPART{vD:VERB}",0,1,1, // passive verbs take only one direct object (MAX) // john could be called -a -brilliant -conversationalist .
									 2,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",0,1,1,  // make an object compulsory - a single past participle can be an adjective as in / English girls were just a mite moss-grown.
																																 // otherwise _VERBPASSIVE_ACP can be a FINAL pattern by itself and the - grown is never incorporated into the rest of the sentence.
																																 // also corrects this - / I've screwed and saved and pinched!
									 0);
	// VERBPASTPART added 8/29/2006 to handle coordinators
	cPattern::create(L"_VERBPASSIVE_ACP{_FINAL_IF_ALONE}",L"2",
									 // they must be given the facts about AIDS and given -the -opportunity to discuss the issues
									 1,L"_THINKPASTPART{vD:VERB}",0,1,1, // passive verbs take only one direct object (MAX) // john could be called -a -brilliant -conversationalist .
									 2,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",0,1,1,
									 0);
	cPattern::create(L"__SUB_S1",L"1",
									 1,L"_VERBPASSIVE_P",0,1,1,
									 1,L",",0,1,1,0);
	// block - too complex to evaluate subject-verb agreement (and passive forms don't agree)
	cPattern::create(L"__SUB_S2",L"1",
									 1,L"__SUB_S1{_BLOCK:MVERB}",0,0,3,
									 1,L"_VERBPASSIVE_P",0,1,1,
									 1,L"coordinator",0,1,1,
									 3,L"_VERBPASSIVE_ACP{_BLOCK:MVERB}",L"_VERBPASSIVE_P{_BLOCK:MVERB}",L"_VERBREL1{_BLOCK:MVERB}",0,1,1,
									 0);
	// passive sentence (only one direct object)
	cPattern::create(L"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}",L"4",
									 1,L"__C1__S1",0,1,1,
									 //1,L"_INFP{_BLOCK}",0,0,1, RINFP 6/7/06
									 2,L"_PP",L"_REL1[*]",0,0,1,
									 1,L"_ADJECTIVE",0,0,1,
									 2,L"__SUB_S2",L"_VERBPASSIVE_P",0,1,1,
									 1,L"__CLOSING__S1",0,0,3,
									 0);
	// I will think you will be one forever.
	// I thought you were right.
	// I think you are right.
	// Lawrence told me you were with monsieur Poirot.
	cPattern::create(L"__ALLTHINK",L"",                   
						1,L"_ADVERB",0,0,1,
						3,L"_THINK",L"_THINKPAST",L"_THINKPRESENT",0,1,1,0); // for the programming convenience of getNounPhraseInfo
	// She thinks that if you are going to carry her banner in the procession you ought to let her take your light .
	cPattern::create(L"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}",L"5",
									 1,L"__C1__S1",0,1,1,
									 1,L"__ALLTHINK{VERB}",0,1,1, // L"think" for all active tenses! - possibly add INTRO_S1.
									 3,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",L"_ADJECTIVE",0,0,1, // Lawrence told me you were with monsieur Poirot. /  She felt confident he was there.
									 1,L"demonstrative_determiner|that{S_IN_REL}",0,0,1,
									 2,L"__S1[*]{_BLOCK:OBJECT:EVAL}",L"_MS1[*]{_BLOCK:OBJECT:EVAL}",0,1,1,
									 0);
	// missing 'that' clause
	// Her hope was she would be allowed to skip soccer.
	// I am afraid we shan't require his services .
	// I am happy I am going to the house.
	// Bill would have been ecstatic his neice is graduating tomorrow.
	// I was afraid they would hear the beating of my heart .
	// it is me they are after.
	// It was extremely likely there would be no second taxi.
	// It was Tilda they would go after next.
	cPattern::create(L"_MS1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_STRICT_NO_MIDDLE_MATCH}",L"2",
									 1,L"_STEP",0,0,1,
									 1,L"_INTRO_S1",0,0,1,
									 1,L"__C1__S1",0,1,1,
									 1,L"_IS{VERB:vS:id}",0,1,1,
									 3,L"_ADJECTIVE",L"__NOUN[*]*1{OBJECT}",L"_NOUN_OBJ*1{OBJECT}",0,0,1, // make NOUN more expensive because this is redundant with _NOUN[5]
									 1,L"__S1[*]*2{_BLOCK:OBJECT:EVAL}",0,1,1, // very rare             // and in general _NOUN[5] is correct when this is a NOUN
									 1,L"_MSTAIL",0,0,1,
									 0);
	//cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"B",
	//                 1,L"__C1__S1",0,1,1,
	//                 1,L"_IS{VERB:vS:id}",0,1,1,
	//                 3,L"_ADJECTIVE",L"__NOUN[*]{OBJECT}",L"_NOUN_OBJ{OBJECT}",0,1,1,
	//                 1,L"__S1{_BLOCK:OBJECT:EVAL}",0,1,1,
	//                 0);
	// He is a little red.
	cPattern::create(L"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}",L"7",
									 1,L"__C1__S1",0,1,1,
									 1,L"_IS{vS:id}",0,1,1,
									 1,L"determiner|a",0,1,1,
									 3,L"adjective|little",L"noun|mite",L"adjective|wee",0,1,1,
									 1,L"_ADJECTIVE*-1",0,1,1,
									 2,L"_PP",L"_REL1[*]",0,0,1,
									 0);
	// Later, in her room, she sat and read.
	// restrict this to things relating to "time"? (see [9])
	// __INTRO_N was introduced to prevent meta patterns incorporating Q1 and VERBREL from 
	// acquiring subjects (through __INTRO_N) and then eliminating _S1 by being lower in cost.  __INTRO_S1 should be used
	// in these situations because it requires a separator for most cases.
	cPattern::create(L"__INTRO_N{_ONLY_BEGIN_MATCH:FLOATTIME}",L"1",
									 1,L"__NOUN[*]*1{OBJECT}",0,0,1, 
									 2,L"_ADVERB*1",L"interjection",0,1,1, // could be a time
									 1,L",",0,0,1,
									 0);
	cPattern::create(L"__INTRO_N{_ONLY_BEGIN_MATCH}",L"2",
									 1,L"_ADJECTIVE*1",0,1,1, // afraid I don't.
									 0);
	cPattern::create(L"__INTRO_N{_ONLY_BEGIN_MATCH}",L"3",
									 2,L"_ADVERB",L"_ADJECTIVE*1",0,1,1, // glibly, she refused. / New, isn't she?
									 1,L",",0,1,1,
									 0);
	cPattern::create(L"__INTRO_S1{_ONLY_BEGIN_MATCH}",L"2",
									 6,L"_NAME{HAIL}",L"honorific{HON:HAIL}",L"_HON_ABB{HON:HAIL}",L"_META_GROUP{HAIL}",L"_VERBREL2{_BLOCK}",L"_VERBREL1{_BLOCK}",0,1,1,
									 1,L",",0,1,1,
									 0);
	cPattern::create(L"__INTRO2_S1{_ONLY_BEGIN_MATCH:FLOATTIME}",L"3",
									 3,L"_TIME",L"_DATE",L"letter",0,1,1,
									 3,L"dash",L":",L",",0,0,1,
									 0);
	cPattern::create(L"__INTRO_S1{_ONLY_BEGIN_MATCH:FLOATTIME}",L"4", // could contain _NOUN[J], which could contain a TIME
									 1,L"__NOUN[*]{OBJECT:HAIL}",0,1,1,
									 3,L"dash",L":",L",",0,1,1,
									 0);
	cPattern::create(L"__INTRO2_S1{_ONLY_BEGIN_MATCH:FLOATTIME}",L"5", // PP could match a time
									7,L"_PP",L"_VERBREL2*1{_BLOCK:EVAL}",L"_REL1[*]*1",L"conjunction",L"coordinator",L"then",L"so",0,1,1, // took out *1 from _PP - discouraged legitimate leading prepositional phrases
									 4,L"dash",L":",L",",L"__ADVERB",0,0,1,
									 0);
	cPattern::create(L"__INTRO2_S1{_ONLY_BEGIN_MATCH}",L"6",
									1,L"if",0,1,1,
									1,L"so",0,1,1,
									1,L",",0,0,1,
									 0);
	// What I mean is, ...
	cPattern::create(L"__INTRO2_S1{_ONLY_BEGIN_MATCH}",L"7",
									1,L"what",0,1,1,
									1,L"__S1{_BLOCK:EVAL}",0,1,1,
									1,L"_IS",0,1,1,
									1,L",",0,0,1,
									 0);
		// simple command
		// come on
		// saddle up, we need to get going.
	cPattern::create(L"__INTRO2_S1{_ONLY_BEGIN_MATCH}",L"8",
									1,L"_VERBPRESENT*2{VERB}",0,1,1,
									2,L"_ADVERB",L"preposition*1",0,0,1,
									1,L",*-2",0,0,1,
									0);
		// Every moment I fall in love with you.
		// A moment later she fell outside the window.
		// Wednesdays he practiced piano.
		// The minute she got through the door he was kissing her furiously.
	cPattern::create(L"__INTRO_N{_ONLY_BEGIN_MATCH:NOUN:OBJECT:FLOATTIME}",L"9",
									 2,L"determiner{TIMEMODIFIER:DET}",L"quantifier{TIMEMODIFIER:DET}",0,0,1,
									 1,L"_ADJECTIVE{TIMEMODIFIER:_BLOCK}",0,0,1,
									 5,L"month{MONTH:N_AGREE}",L"daysOfWeek{DAYWEEK:N_AGREE}",L"season{SEASON:N_AGREE}",L"timeUnit{TIMECAPACITY:N_AGREE}",L"dayUnit{TIMECAPACITY:N_AGREE}",0,1,1, // A moment
									 1,L"_ADVERB",0,0,1, // later
									 1,L",",0,0,1,
									 0);
		// hardly able to contain his excitement,
	cPattern::create(L"__INTRO_S1{_ONLY_BEGIN_MATCH}",L"A",
									 1,L"_ADJECTIVE",0,1,1, // hardly able
									 1,L"_INFP",0,1,1, // _INFP made optional would replicate _INTRO_S1[1]
									 1,L",",0,1,1,
									 0);
	// The girl assenting,
	cPattern::create(L"__INTRO_S1{_ONLY_BEGIN_MATCH}",L"B",
									 2,L"__NOUN[*]{SUBJECT}",L"__MNOUN[*]{SUBJECT}",0,1,1, // The girl
									 1,L"_VERBONGOING",0,1,1, // assenting
									 1,L",",0,1,1,
									 0);
	cPattern::create(L"_INTRO_S1{_NO_REPEAT}",L"",
										3,L"__INTRO_S1{_BLOCK:EVAL}",L"__INTRO2_S1{_BLOCK:EVAL}",L"__INTRO_N{_BLOCK:EVAL}",0,1,1,
										3,L"__INTRO_S1*1{_BLOCK:EVAL}",L"__INTRO2_S1*1{_BLOCK:EVAL}",L"__INTRO_N*1{_BLOCK:EVAL}",0,0,2,
										0);
	// Please, Tommy?  Later, Tommy. Yes Mom. Sorry, Tuppence.
	cPattern::create(L"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}",L"5",
									 3,L"polite_inserts",L"_ADVERB",L"interjection",0,1,1, // pray / please / Yes (ADVERB)
									 1,L",",0,0,1,
									 1,L"_NAME_INJECT",0,1,1,0);
	// __S2 -- 8 and 9
	// if no pattern matches a plain interjection, that interjection will never be matched as one
	// if it can be matched as anything else, even though those other forms carry a higher cost.
	cPattern::create(L"__S2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}",L"8",
									 1,L"interjection",0,1,1, // pray / please / Yes (ADVERB)
									 0);
	cPattern::create(L"__S2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}",L"9",
									 1,L"quotes",OPEN_INFLECTION,1,1,
									 1,L"interjection",0,1,1, // pray / please / Yes (ADVERB)
									 2,L",",L".",0,0,1,
									 1,L"quotes",CLOSE_INFLECTION,1,1,
									 0);
	//cPattern::create(L"__S1{_FINAL}",L"7",
	//                 1,L"__C1__S1",0,1,1,
	//                 1,L"_VERB_ID{id}",0,1,1,
	//                 1,L"_ADJECTIVE",0,1,1,
	//                 1,L"_PP",0,1,1,
	//                 0);
	// IMPERATIVES p.803 CGEL
	// don't _BE _NOUN _NOUN_OBJ
	// don't -be -hasty .
	// don't -be a fool . L"
	// stop it.
	// L" don't -be too -disconsolate , Miss Tuppence , L" he said in a low voice .
	// please be quiet.
	cPattern::create(L"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}",L"1",
						3,L"polite_inserts",L"_ADVERB",L"_NAME",0,0,1, // pray / please / Yes (ADVERB) / Tommy
						1,L",",0,0,1,
						1,L"_DO{imp}",0,0,1,
						1,L"_BE{VERB:vS:id}",0,1,1,
						5,L"__NOUN[*]{OBJECT}",L"_NOUN_OBJ{OBJECT}",L"__NOUNREL{OBJECT}",L"_VERBPASTPART",L"_ADJECTIVE",0,0,1,0);
	// make Ben help me.
	cPattern::create(L"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}",L"2",
						3,L"polite_inserts",L"_ADVERB",L"_NAME",0,0,1, // pray / please / Yes (ADVERB) / Tommy
						1,L",",0,0,1,
						1,L"verbverb{vS:V_HOBJECT}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_PLURAL,1,1,
						3,L"_NOUN_OBJ{HOBJECT}",L"__NOUN[*]{HOBJECT}",L"__MNOUN[*]{HOBJECT}",0,1,1,
						2,L"_ADVERB",L"_PP",0,0,2,
						1,L"_VERBPRESENT{VERB}",0,1,1,
						3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
						1,L"__CLOSING__S1",0,0,3,
						0);
	// to the point, my friend.
	cPattern::create(L"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}",L"3",
						1,L"_PP",0,1,1,
						1,L"__CLOSING__S1",0,1,3,
						0);
	// bake some buns with me, please.
	cPattern::create(L"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}",L"3",
						1,L"_ADVERB",0,0,1,
						4,L"_VERBPRESENT{VERB}",L"_VERBPRESENT_CO",L"is{vS:id:V_AGREE:V_OBJECT:VERB}",L"is_negation{vS:id:not:V_AGREE:V_OBJECT:VERB}",
							VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
						3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // L"there{OBJECT}L" removed
						1,L"__CLOSING__S1",0,1,3,
						0);

	cPattern::create(L"_THINKPRESENT_CO",L"",
												1,L"_THINKPRESENTFIRST",0,1,1,
												1,L"coordinator",0,1,1,
												1,L"_THINKPRESENTFIRST{VERB}",0,1,1,
												0);
	// [don't you] say you will marry me.
	cPattern::create(L"_SUBCOM",L"",
						1,L"_DO{imp}",0,1,1,
						3,L"__NOUN[*]{OBJECT}",L"_NOUN_OBJ{OBJECT}",L"__NOUNREL{OBJECT}",0,1,1,
						0);
	cPattern::create(L"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}",L"4",
						3,L"polite_inserts",L"_ADVERB",L"_NAME",0,0,1, // pray / please / Yes (ADVERB) / Tommy
						1,L",",0,0,1,
						1,L"_SUBCOM",0,0,1,
						2,L"_THINKPRESENTFIRST{VERB}",L"_THINKPRESENT_CO",0,1,1,
						3,L"_NOUN_OBJ{OBJECT}",L"__NOUN[*]{OBJECT}",L"__MNOUN[*]{OBJECT}",0,0,1, // Tell me you were with monsieur Poirot.
						1,L"__S1[*]{_BLOCK:OBJECT:EVAL}",0,1,1,
						0);
	// willing to do anything, go anywhere. (Secret Adversary)
	cPattern::create(L"_PLEA{_FINAL:_ONLY_BEGIN_MATCH}",L"1",
												1,L"_VERBONGOING{VERB:vE}",0,1,1,
						3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
						1,L"__CLOSING__S1",0,0,3,
						0);


	// that one might believe every day to be Sunday
	// that such was not the case.
	// char *relativizer[] = {L"who",L"which",L"whom",L"whose",L"where",L"when",L"why",NULL};
	// pattern is not grouped with its other root patterns
	// this cost should be kept higher than the cost for a preposition at a postential end for __S1, like for _VERBPRESENT.
	// whoever he was
	// what he did do
	// she went to London, where she entered a children's hospital.
	// Do NOT add REL tag here - the REL tag is only for relative phrases that be attached to objects
	cPattern::create(L"_REL1{_FINAL_IF_ALONE:_FORWARD_REFERENCE:S_IN_REL}",L"2",//3,L"demonstrative_determiner",L"personal_pronoun_nominative",L"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
										2,L"_ADJECTIVE",L"_ADVERB",0,0,1,
										4,L"relativizer",L"interrogative_determiner",L"as",L"demonstrative_determiner|that",0,1,1,
										1,L"_PP", 0,0,1,  // He knew that *after sunset* almost all the birds were asleep.
										1,L"_ADVERB",0,0,1, // where simply every one is bound to turn up sooner or later
										1,L"__S1{_BLOCK:EVAL}",0,1,1,
										1,L"preposition*4",0,0,1, // that you are afraid 'of'// preposition use should be rare!
										0);
	// how fast he can give Tom books
	// how giving the author says he is
	// Do NOT add REL tag here - the REL tag is only for relative phrases that be attached to objects
	cPattern::create(L"_REL1{_FINAL_IF_ALONE:_FORWARD_REFERENCE:S_IN_REL}",L"3",//3,L"demonstrative_determiner",L"personal_pronoun_nominative",L"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
										1,L"how",0,1,1,
										1,L"_ADJECTIVE",0,1,1,
										1,L"__S1{_BLOCK:EVAL}",0,1,1,
										0);
	// which people he can influence the most
	// what things he boasts about
	// whose people they can boss about every day
	// Do NOT add REL tag here - the REL tag is only for relative phrases that be attached to objects
	cPattern::create(L"_REL1{_FINAL_IF_ALONE:_FORWARD_REFERENCE:S_IN_REL}",L"4",//3,L"demonstrative_determiner",L"personal_pronoun_nominative",L"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
										3,L"which",L"what",L"whose",0,1,1,
										2,L"__NOUN[*]{_BLOCK}",L"__MNOUN[*]{_BLOCK}",0,1,1,
										1,L"__S1{_BLOCK:EVAL}",0,1,1,
										0);
	cPattern::create(L"__PP",L"A",1,L"_ADVERB*1",0,0,1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
									 1,L"preposition*1{P}",0,0,1, // from within
									 1,L"_ADVERB*3",0,0,1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
									 1,L"_REL1[*]{PREPOBJECT}",0,1,1,   // my purpose IN (prep) what follows (_REL) (is to try to place him) A6U .
									 0);
		// this was cut because it is too ambiguous and allowed too many non-coherent parses
		// moved to __C1_IP
	// restatement / this woman, whoever she was, was saved
	// this woman, master of disguise,
	//cPattern::create(L"__NOUN{_FINAL_IF_ALONE:_FORWARD_REFERENCE}",L"A",
	//                 1,L"__NOUN[*]",0,1,1,
	//                 1,L",",0,1,1,
	//                 2,L"__NOUN[*]{_BLOCK:RE_OBJECT}",L"_REL1[*]{_BLOCK}",0,1,1,
	//                 1,L",",0,1,1, // period deleted because of the '.' at end - swallows up the period
	//                  0);

	//cPattern::create(L"__S1",L"7",1,L"relativizer",0,1,1, // put into Q1 L"CL"
	//                 1,L"not",0,1,1,0);
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"1",
											 1,L"_STEP",0,0,1,
											 1,L"_INTRO_S1",0,0,1,
											 1,L"_ADVERB",0,0,1, // even if 
											 2,L"if",L"as",0,1,1,
											 1,L"__S1{_BLOCK:EVAL}",0,1,1,
											 1,L",",0,0,1,
											 1,L"then",0,0,1,
											 1,L"__S1{_BLOCK:EVAL}",0,1,1,
											 0);
	cPattern::create(L"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}",L"1",
											 1,L",",0,0,1,
											 4,L"then",L"so",L"conjunction",L"coordinator",0,1,1,
											 1,L"then",0,0,1,
											 2,L"_PP*2",L"_VERBREL2*2",0,0,1,  // Tommy did not hear Boris's reply , but [in response to it] Whittington said something that sounded like - *1 competition with __MSTAIL[9]
											 2,L"__S1{_BLOCK:EVAL}",L"_REL1[*]",0,1,1, // L"__NOUN[*]*1",0,1,1,  took __NOUN out - absorbed the subject of the second sentence following another sentence (and a conjunction)
											 0);
	cPattern::create(L"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}",L"2",
											 1,L",",0,0,1,
											 3,L"but",L"then",L"and",0,1,1,
											 1,L"then",0,0,1,
											 4,L"__ALLVERB{VERB2}",L"_COND{VERB2}",L"_VERBPASTPART*1{vB:VERB2}",L"_BEEN{vB:id:VERB2}",0,1,1,
											 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
											 1,L"__CLOSING__S1",0,0,3,
											 0);
	// Like all the darned lot of them, he wasn't going to commit himself till he was sure he could deliver the goods.
	cPattern::create(L"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}",L"3",
											 1,L",",0,0,1,
											 4,L"then",L"so",L"conjunction",L"coordinator",0,1,1,
											 1,L"then",0,0,1,
											 1,L"__C1__S1",0,1,1,
											 1,L"_IS{VERB:vS:id}",0,1,1,
											 3,L"_ADJECTIVE",L"__NOUN[*]*1{OBJECT}",L"_NOUN_OBJ*1{OBJECT}",0,0,1, // make NOUN more expensive because this is redundant with _NOUN[5]
											 1,L"__S1[*]*2{_BLOCK:OBJECT:EVAL}",0,1,1, // very rare             // and in general _NOUN[5] is correct when this is a NOUN
											 0);
	// prevents multiplicative nesting in _MS1
	cPattern::create(L"_MSTAIL{_NO_REPEAT}",L"",1,L"__MSTAIL[*]",0,1,4,0);
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"3",
												1,L"_INTRO_S1",0,0,1,
												1,L"__S1{_BLOCK:EVAL}",0,1,1,
												1,L"more",0,1,1,
												1,L"than",0,1,1,
												3,L"__S1{_BLOCK:EVAL}",L"__NOUN",L"__MNOUN",0,1,1,
												0);
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}",L"4",
											 1,L"_INTRO_S1",0,0,1,
											 4,L"__S1{_BLOCK:EVAL}",L"_COMMAND1{_BLOCK:EVAL}",L"_RELQ{_BLOCK:EVAL}",L"_REL1{_BLOCK:EVAL}",0,1,1,
											 1,L"_MSTAIL",0,1,1,
											 0);
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"5",
											 1,L"_INTRO_S1",0,0,1,
											 1,L"if",0,1,1,
											 1,L"__S1{_BLOCK:EVAL}",0,1,1,
											 1,L",",0,0,1,
											 1,L"then",0,0,1,
											 1,L"__S1{_BLOCK:EVAL}",0,1,1,
											 1,L"_MSTAIL",0,1,1,
											 0);
	cPattern::create(L"_MTS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"5",
											 1,L"_INTRO_S1",0,0,1,
											 1,L"__S1{_BLOCK:EVAL}",0,1,1,
											 1,L",",0,0,1,
											 3,L"but",L"then",L"and",0,1,1,
											 1,L"then",0,0,1,
											 1,L"__ALLTHINK",0,1,1, // L"think" for all active tenses! - possibly add INTRO_S1.
											 2,L"_PP",L"_REL1[*]",0,0,1,
											 2,L"_INFP{OBJECT}",L"__S1{_BLOCK:OBJECT:EVAL}",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
											 0);
	// not only... but also...
	// for not only was he born in that city, but his family had been resident there for centuries.
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"6",
									 1,L"not",0,1,1,
									 1,L"only",0,1,1,
									 1,L"_Q1PASSIVE{_BLOCK}",0,0,1,
									 1,L"_PP",0,1,1,
									 1,L",",0,0,1,
									 1,L"but",0,1,1,
									 1,L"also",0,0,1,
									 2,L"_PP",L"__S1{_BLOCK:EVAL}",0,1,1,0);
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}",L"9",
									 1,L"_STEP",0,0,1,
									 1,L"_INTRO_S1",0,1,1,
									 1,L"if",0,0,1,
									 1,L"__S1[*]{_BLOCK:EVAL}",0,1,1,
									 1,L"__CLOSING__S1",0,0,3,
									 1,L"_REF",0,0,1,
									 0); 
	// this was introduced to prevent Q1, VERBREL and so forth from having a NOUN subject
	// hidden by INTRO_S1 that was actually a subject.  As well as being incorrect, this
	// will remove the cost of VERBAFTERVERB (if the NOUN includes a verb), which will make 
	// it potentially even more inaccurate.
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"D",
									 1,L"_STEP",0,0,1,
									 1,L"_INTRO_S1",0,1,1,
									 1,L"if",0,0,1,
									 3,L"_Q1*1{_BLOCK:EVAL}",L"_VERBREL1",L"_COMMAND1{_BLOCK:EVAL}",0,1,1,
									 1,L"__CLOSING__S1",0,0,3,
									 1,L"_REF",0,0,1,
									 //1,L"?*-1",0,0,1,
									 0); // In any case, what do you lose? (_Q1)
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"A",
									 1,L"_STEP",0,1,1,
									 1,L"if",0,0,1,
									 3,L"_Q1*1{_BLOCK:EVAL}",L"_VERBREL1",L"_COMMAND1{_BLOCK:EVAL}",0,1,1,
									 1,L"__CLOSING__S1",0,0,3,
									 1,L"_REF",0,0,1,
									 1,L"?*-1",0,0,1,
									 0); // (a) - slide the block over.
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"E",
									 1,L"_STEP",0,1,1,
									 1,L"if",0,0,1,
									 1,L"__S1[*]{_BLOCK:EVAL}",0,1,1,
									 1,L"_REF",0,0,1,
									 1,L"?*-1",0,0,1,
									 0); // (a) - slide the block over.
		// what he seeks to attain we do not know.
		// When he thinks he has given an extraordinarily clever impersonation he shakes with laughter.
	cPattern::create(L"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",L"C",//3,L"demonstrative_determiner",L"personal_pronoun_nominative",L"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
										10,L"which",L"what",L"whose",L"relativizer|when", L"conjunction|before", L"conjunction|after", L"conjunction|since", L"conjunction|until", L"conjunction|while", L"preposition|during", 0,1,1,
										1,L"__S1{_BLOCK:EVAL}",0,1,1,
										1,L"__S1{_BLOCK:EVAL}",0,1,1,
										0);
	cPattern::create(L"_HAIL{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}",L"",
		4,L"_NAME{HAIL}",L"honorific{HAIL:HON}",L"_HON_ABB{HAIL:HON}",L"_META_GROUP{HAIL}",0,1,1, // , sir / , freak! noun includes _NAME, L"honorific",
										 1,L",",0,1,1,
										 1,L"__NOUN[*]",0,1,1,
										 0);

	//cPattern::create(L"_MS1{_FINAL:_ONLY_BEGIN_MATCH}",L"C",
	//                 1,L"_ADJECTIVE",0,1,1, // afraid I don't.
	//                 1,L"__S1[*]",0,1,1,
	//                 0); // In any case, what do you lose? (_Q1)
	// prevent NOUN[B] from matching a quoted sentence (because NOUN[B] will be the only final pattern matching everything including the quotes)
	//cPattern::create(L"_MS1{_FINAL}",L"B",
	//                1,L"quotes",OPEN_INFLECTION,1,1,
	//                1,L"__S1[*]",0,1,1,
	//                2,L",",L".",0,0,1,
	//                1,L"quotes",CLOSE_INFLECTION,1,1,0);
	// , might, / , could, / He might.
	// Beresford speaking (Agatha Christie)
	cPattern::create(L"__MODAUX{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}",L"1",
													1,L"__NOUN",0,0,1,
													1,L"_COND",0,1,1,0);
	cPattern::create(L"__MODAUX{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}",L"2",
													1,L"__NOUN",0,1,1,
													1,L"_VERBONGOING{vE:VERB}",0,1,1,0);
	// especially him.  especially this case.
	// you too? // me especially? / those?
	cPattern::create(L"__IMPLIEDIS{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}",L"1",
													1,L"_ADVERB",0,1,1,
													2,L"_NOUN_OBJ",L"__NOUN",0,1,1,
													1,L"_ADVERB",0,0,1,
													0);
	// been empty for years. / been emptying for years
	cPattern::create(L"__IMPLIEDIS{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}",L"3",
													1,L"_BEEN",0,1,1,
													2,L"_ADJECTIVE",L"verb",VERB_PRESENT_PARTICIPLE,1,1,
													1,L"_PP",0,1,1,
													0);
	// yes, little lady, out with it.
	// this pattern creates too many problems and doesn't even match the original sentence it was created for.
	cPattern::create(L"__IMPLIEDVERBCOMMAND{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}",L"",
												 1,L"interjection",0,1,1,
												 1,L",",0,1,1,
												 1,L"__NOUN{HAIL}",0,1,1,
												 1,L",",0,1,1,
												 1,L"preposition|out",0,1,1,
												 1,L"_PP",0,1,1,
												 0);
	cPattern::create(L"_SECTIONHEADER{_FINAL:_ONLY_BEGIN_MATCH}",L"",
													1,L"sectionheader*-10",0,1,1,
														 3,L"numeral_cardinal",L"roman_numeral",L"Number",0,1,1,
														 0);
	return 0;
}

