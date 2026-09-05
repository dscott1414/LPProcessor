/*
	definePatterns.cpp - the 500+ hand-written sentence patterns (stage 4 grammar)

	Overview:
		This translation unit is almost entirely data: sequences of
		cPattern::create() calls that register noun phrases, verb groups,
		infinitives, prepositional phrases, relative clauses, questions,
		commands, and the multi-clause _MS1/_MS2 wrappers.  Nothing here
		matches text; initializePatterns() in pattern.cpp runs the create*
		functions once at startup, then resolves "_FOO" forward references
		and evaluates tag sets.  Pattern syntax and tag tables are documented
		in README.md "Parsing Processing Stages" / the _PATTERN TAG and TAG
		NAME tables.

	Pipeline position:
		Stage 1 initialization, before any document is read.  Called from
		initializePatterns() in this order: createBasicPatterns,
		createVerbPatterns, createSecondaryPatterns1, createInfinitivePhrases,
		createPrepositionalPhrases, createQuestionPatterns (questionProcessing.cpp),
		createSecondaryPatterns2, createLetterIntroPatterns (resolveSpeakers.cpp).
		Name and time patterns (defineNames / defineTimePatterns) live in
		other TUs and are invoked from createBasicPatterns.

	Key entry points:
		- createBasicPatterns() - abbreviations, adverbs, adjectives, nouns,
		  names (via defineNames), time (via defineTimePatterns), interjections
		- createVerbPatterns() - _IS/_DO/_HAVE/_COND/_VERB* and __ALLOBJECTS
		- createInfinitivePhrases() / createBareInfinitives() / createThinkBareInfinitives()
		- createPrepositionalPhrases() - __PP / _PP
		- createSecondaryPatterns1() - appositives, relative clauses, VERBREL
		- createSecondaryPatterns2() - dispatcher for __S1 / _MS1 / _COMMAND / _REL / ...

	Key data structures / globals:
		- None of its own.  Every create() appends to the global `patterns`
		  vector and may push a cPatternReference for a not-yet-defined child.

	Notes / gotchas:
		- A later same-name create() is a new variant (differentiator "1","2",
		  "R", ...), chained via nextRoot.  Referring to __NOUN[*] binds all
		  of them; referring to __NOUN without [*] binds only those already
		  created — the [*] is mandatory when a later variant must be included.
		- Costs (*N / *-N) and {_BLOCK} / {_FINAL*} tags are the only levers
		  the winnow has; a cheaper non-final pattern that out-spans a _FINAL
		  one can leave the sentence unmatched (see README).
		- createQuestionPatterns and createLetterIntroPatterns are not defined
		  here.  defineNames / defineTimePatterns / createMetaNameEquivalencePatterns
		  are declared at the top and called from createBasicPatterns.
		- The Longman Grammar notes above createNouns() are the author's
		  design record for closed-class words (as/so/such/any/that/...).
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>
#include <set>
#include <mysql.h>
#include <unordered_map>
#include <unordered_set>
using namespace std;

#include "logging.h"
#include "general.h" 
#include "intarray.h"
#include "pattern.h"
#include "profile.h"

void defineNames(void);
void defineTimePatterns(void);
void createMetaNameEquivalencePatterns(void);

// study of as: can be used as a coordinator, a conjunction, or an adverb
// Longman 74-76, 85-86, 130,196,336,340,415,423,527-8,550,609,838,841-7,908,919,1000,1011,1017ff,1023-4
// preposition: as a cultivator / The police are treating it as a murder case.
// subordinator (manner, reason, time): as they watched / An armed robber was mugged of his loot as he made his getaway.
// 527:comparative clause: Maybe Henry would realize she was not as nice as she pretended to be.[subordinator] / The last tinkle of the last shard died away and silence closed in as deep as ever before.[preposition]
// adverbs with degree complements: The normal scan must be resumed as quickly as possible. / I didn't do as well as I wish I had.  / I said as heartily as I could.
// as is only mentioned as a non-standard relativizer (609)
// fronting the verb in dependent clauses: Try as she might to make it otherwise... / rich as they are in the prejudices of our enlightened age... / 
// Inversion in dependent clauses (subject coming after the verb): They chatted about Hollywood, and Claudia was fascinated, as were the other guests. / The contraceptive cap can also spark the syndrome, as can a wound infection.
// Subordinator to a verbal clause: A curve may be plotted as shown in Fig 3.16. [p 1023]
// complex subordinators: As far as I know / As long as you don't get caught / As soon as you find your thing

// study of more: can be used as a 'large' quantifier, an amplifier degree adverb (7.14.2.4)
// Longman 278, 522, 524, 525, 527ff, 544-5, 554, 561, 884
// statistics quantifier 600/1000000, adverb 200/1000000

// study of so:
// Longman 68,72,80,86,526-8, 550, 554, 562, 564-6, 751-3, 844, 877-8, 880,883-7, 889, 891, 903, 916-19, 970, 1002, 1046, 1050, 1074, 1078ff
// 72:so is a 'pro-form'
// 80: so is like a coordinator and like a linking adverbial
// 527:structural types of degree complement
//     so + adjective/adverb + that-clause The murder investigation was so contrived that it created false testimony
//     so + adjective/adverb + as to-clause And if anybody was so foolhardy as to pass by the shrine after dusk he was sure to see the old woman hopping about.
// 554: amplifier degree adverb
// 562: linking adverb over 1000/1000000 in AmE
// 844: adverbial subordinator
// 877: so is a linking adverbial sometimes indicating a result/inference
// 891: so as a linking adverbial must occur in the beginning of the clause
// 903,916: so used in a special type of predicative fronting, with subject-operator inversion: so preoccupied was she at the moment/so ruthless was the IRA in its onslaught/so different was the theories of schools
// 917: inversion after linking form: Gail's in, and so is Lisa. / She despised him; so did they all. / As the infections spread in women, so did infections in their babies.
// 

// study of such: 
// longman 258, 280-3, 355,900,902,909,916,1018
// such is named a semi-determiner (4.4.6D)
// 4 uses delineated:
//  1. semi-determiner - We believe, however, that such a theory is possible. frequency out of 1M: 400FICT, 1200ACAD
//  2. intensifier - He's such a flipping bastard. frequency out of 1M: 200FICT, 100ACAD
//  3. complex preposition (such as) - There are crystals, of substances like tourmaline, which are sensitive to the polarization of light. frequency out of 1M: <50FICT, 700ACAD
//  4. complex subordinator (such that) - The a generalization covers a multitude of items, such that it is impossible to nab each one, in no way makes the generalization unclear.
// Use in noun phrase as a fronted object:
//   Such a blunder I had now committed.
//   Such things you must tell me.
// predicative fronting with subject-verb inversion:
//   Under stress, Sammler believed, the whole faltered, and parts (follicles, for instance) became conspicuous. Such at least was his observation.
//   Such a rich chapter it has been!
//   Such a gift he had for gesture.
//   Such is the confusion aboard this vessel I can find no one whoe has the authority to countermand this singularly foolish order.

// study of another:
// Longman 282-283
// another is an indefinite article

// study of any:
// Longman 161, 169, 176, 184, 276, 278, 352, 1113, 1122
//   176: discussing negation, non-assertive form (3.8.6) 
//     1. There aren't any crips
//     2. I don't think we had any cheese
//     3. I don't suppose it's there any more.
//     4. Do you need any more?
//     5. Wonder if Tamsin had any luck selling her house.
//     6. If they haven't got any scampi, get an extra fishcake.
//     7. If there are any problems in performance related pay, we can iron these out.
//     8. Before he could get any words out, Mathew said " I don't want to sell my shares either."
//     9. McIntyre had denied being a member of any party.
//    10. The company can make cars as reliable as any of its competition.
//    11. I'm too old for any of that.
// 276: quantifier - D (arbitrary/negative number or amount)
//     There aren't any women
//     Got any money?
//     

// study of though:
// Longman 85,87,562,842-3,845,850,883-8,891,908,1050
// 85: 'though' belongs to subordinators (subordinating conjunctions) - introducing adverbial clauses
// 87:  'though' used as a subordinator: She had never heard of him, though she did not say so.
//      'though used as an adverb: That's nice though, isn't it?
// 562: lists 'though' as a linking adverb
// 842: lists 'though' as a concessive adverbial subordinator
// 845:  'though' used as a subordinator: Though his eyes took note ofmany elements in the crowd through which he passed they did so morosely.
//                                        Although I felt a little lonely, my exaltation of the mornning had not worn off.
//       When used as a subordinator, though and although are synonymous.
// 850:                                   The elections were peaceful, though hampered by delays.
// 851:  Used as a linking adverbial:
//       Oh, maybe I won't go.  I should though.
//       It was one year she should've done without the invitation though isn't it?
//       I may be exaggerating about the machine gun though: I don't think they had one.
//       When, though, you marry in the Royal Family...
//       Right now, though, he faces the most critical time in his managerial career.

// study of 'mine':
// Longman 328, 340-2,448
// mine listed as a 1st singular pronoun.
// mine listed as a possessive pronoun on section 4.11 (340)
// 341:'a friend of mine' construction noted as a way of speicification with both a determiner (a) and a possessive marker (mine)

// study of 'please':
// Longman 140, 207, 220, 560, 1047, 1098-9
// noted as a discourse marker (politeness) - Please, Dad, I know your busy and that...
// 207: Could I have two pounds please?
// 220: (as used in commands) Pass me his drink please.
// 560: very similar in usage : kindly

// study of 'less'
// Longman 278, 555,915ff
// less is a moderate/small quantifier (278), or a diminisher degree adverb (555)

// study of 'all:
// Longman 71,184,258-9, 275,278,355,1013,1120
// quantifier as predeterminer: He kept whistling at all the girls
// quantifier as pronoun: Is that all I've got Dad?
// quantifier as adverb: Don't get all mucky.
// 184:uses plural concord
// 258: all is a predeterminer
// 275: it is an inclusive quantifier  I'm am just fascinated by all those things
//      Spend all the money.
// 277: frequency 5 occurrences/1000 words

// study of 'but':
// Longman 79-81, 84, 433, 851, 898, 901, 1047, 1070, 1079
// but is a coordinator (coordinating conjunction)
// Quirk: 9.4n; II.9 (conjunction: 4.53n; 6.5; 8.144, 146; 9.58n; 10.66n; 11.44; 12.68; 13 passim; 14.2; 15.43, 44; 19.58, 59n, 65; III.7.12)
//                   (subjunct: 8.111, 116n; 10.69; 11.41)
//                   (preposition: 6.5, 27; 9.7,58; 15.44)


// study of 'that':
// Longman 85-7, 97, 135, 193-4, 196, 233, 270, 272, 274, 309, 336, 349-350, 584-5, 
//         608-9, 611-17, 621, 629, 644-5, 647-8, 650-1, 653-4, 658, 660ff, 671-2, 
//         674, 676-9, 684, 687, 693, 695, 705-6, 709, 714, 724-5 730ff, 739, 751-7,
//         839, 860, 865, 970, 972, 984, 992, 994, 999, 1001-3, 1005-6, 1008,1010, 
//         1016-21, 1043, 1076
// noted as conjunctionm determiner, intesifier, pronoun, relative by Quirk
// 85: subordinating conjunction introducing degree clauses OR complement/nomincal clause OR adverbial caluse as part of a complex subordinator
// 87: that is also a degree adverb (that good), a relativizer, and a demonstrative determiner or pronoun
// 97: that as pronoun: Anyone can see *that*.
// 135: 

// study of 'this':
// Longman 270, 272, 274, 309, 349, 584, 616, 730, 947, 1023-4, 1043
// 272: demostrative determiner
// 616: demonstrative pronoun

// study of 'some':
// Longman 112,176,184,276,278,1007,1039,1113,1115
// 276:some is a moderate/small quantifier, which is a type of determiner
// 176: some is also a pronoun
// Register the noun-phrase family: _NOUN_OBJ, __N1, __NOUN[2/3/4/WILL/MTHAN/R*],
// __PNOUN, __APPNOUN, coordinated / possessive / company-suffix variants.
// Differentiator "2" is the common-noun shape; "C" is
// pronominal; "R"/"R2"/... are relative-clause-containing nouns that set
// _EXPLICIT_SUBJECT_VERB_AGREEMENT.  Returns 0.
int createNouns(void)
{
	LFS
		// this has the same follows as _NOUN[9] (except _PP and _REL1 are optional)
		// _NOUN_OBJ is referenced by getNounPhraseInfo as having a differentiator of "1"
		cPattern::create(u"_NOUN_OBJ{PNOUN}", u"1",
			6, u"reflexive_pronoun{N_AGREE}", u"reciprocal_pronoun{N_AGREE}",
			u"relativizer{N_AGREE:SINGULAR}", u"personal_pronoun_nominative{N_AGREE}", u"personal_pronoun{N_AGREE}", u"adjective|other{N_AGREE}", 0, 1, 1,
			1, u"_ADJECTIVE_AFTER*1", 0, 0, 1, // adjectives after the noun are rare
											//2,u"_PP",u"_REL1",0,0,4,  merged with _NOUN[9]
			0);
	// verbs should tend to be used as verbs
	// possessive pronouns are rarely nouns (removed - 'mine' is a perfectly fine noun)
	// u"demonstrative_determiner", removed - because __N1 may take adjectives and this class does not
	cPattern::create(u"__N1{N_AGREE}", u"", 19,
		u"possessive_pronoun", u"interrogative_pronoun", u"pronoun", u"indefinite_pronoun", u"adjective|other{N_AGREE}",
		u"noun", u"abbreviation", u"measurement_abbreviation*1", u"trademark", // u"country", // removed -- I've got to trust some one--and it must be a woman. (moved to _NOUN)
		u"numeral_cardinal", u"numeral_ordinal", u"quantifier", u"Number*1", u"verb*1{VNOUN}", u"does*1{VNOUN}", u"letter",
		u"time", u"date", u"telephone_number", VERB_PRESENT_PARTICIPLE | NO_OWNER, 1, 1,
		0);

	cPattern::create(u"__HIS_HER_DETERMINER", u"",
		1, u"possessive_determiner|his{DET}", 0, 1, 1,
		3, u",", u"/", u"or", 0, 1, 1,
		1, u"possessive_determiner|her", 0, 1, 1, 0);
	cPattern::create(u"__IPDET", u"",
		1, u"indefinite_pronoun{DET}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	// THE exhaustively broken eclectically skinny bean | skinny beans | the old Joe Miner | a great someone
	// ADJ added to capture which words were owned by a personal agent
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"2",
		9, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET:ADJ}", u"interrogative_determiner{DET}", u"quantifier{DET}", u"numeral_cardinal{DET}", u"__HIS_HER_DETERMINER*1", u"_NAMEOWNER{DET}", u"__IPDET", 0, 0, 1,
		1, u"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		2, u"noun*1", u"Proper Noun*1", SINGULAR, 0, 2, // noun and Proper Noun must cost 1 otherwise they will match / diamond necklace
		1, u"adjective", 0, 0, 1,  // cherry coloured ribbons
		2, u"__N1", u"_NAME{GNOUN:NAME}", 0, 1, 1,                 // the noun and PN in ADJECTIVE.  The only difference is here they have
																															 // no OWNER flags
	//2,u"_ADJECTIVE",u"preposition",0,0,2,0);// confused with repeated preposition phrases 'on the afternoon of May'.
		2, u"_ADJECTIVE_AFTER*2", u"letter*1", 0, 0, 1, 0); // an adjective after the noun is less common Bill A, Bill B
	cPattern::create(u"_META_GROUP{_FINAL_IF_ALONE:NOUN}", u"",
		1, u"possessive_determiner{DET:ADJ}", 0, 0, 1,
		1, u"_ADJECTIVE{_BLOCK}", 0, 0, 1,
		1, u"noun|friend{N_AGREE}", 0, 1, 1,
		0);
	// her will / the last will / the new will/ this will
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"WILL",
		7, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET:ADJ}", u"interrogative_determiner{DET}", u"__HIS_HER_DETERMINER*1", u"_NAMEOWNER{DET}", u"adjective|ill*-2", 0, 1, 1,
		2, u"adjective|last", u"adjective|new", 0, 0, 1,
		1, u"noun|will*-3{N_AGREE}", SINGULAR, 1, 1,
		0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE}", u"MTHAN",
		1, u"_ADJECTIVE", 0, 1, 1,
		1, u"preposition|than*-1", 0, 1, 1,
		3, u"_NOUN_OBJ{OBJECT}", u"__NOUN[*]{OBJECT}", u"__VERBALPHRASENOUN[*]{OBJECT}", 0, 1, 1,
		0);
	// this also encourages a word identified as both adverb and adjective to be an adverb if identified before an adjective.
	// the more adjectives repeated, the more uncommon - taken out - discourages nouns taking on adjectives
	// all these old things / all my old things / half these old things / both old things
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"3",
		2, u"predeterminer|half*-2", u"predeterminer*-1", 0, 1, 1,  // made -1 to encourage binding this limited class as a predeterminer and not as another class such as adverb which is more common and there fore lower cost (such)
		5, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET}", u"__HIS_HER_DETERMINER*1", u"_NAMEOWNER{DET}", 0, 0, 1,
		3, u"_ADJECTIVE{_BLOCK}", u"noun*1", u"Proper Noun*1", SINGULAR, 0, 3,
		2, u"__N1", u"_NAME{GNOUN:NAME}", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 0, 2, 0);
	//1,u"_POSTMOD",0,0,1,0);
// The good / the foremost -- quantifier isn't valid when combined with a predeterminer!
// All the best / all their
// a baker's
// a greater good
// the latter//the former
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:ADJOBJECT}", u"4",
		1, u"predeterminer*-1", 0, 0, 1, // took out GNOUN - N_AGREE should not be defined under GNOUN because it causes agreement problems - 'Tags are inconsistent for subject tagsets' 12/03/2007
		//5,u"determiner",u"demonstrative_determiner{N_AGREE}",u"possessive_determiner",u"quantifier",u"__HIS_HER_DETERMINER*1",0,1,1,
		//1,u"_ADJECTIVE*3{PLURAL}",0,1,1, // this entire pattern is rare
		3, u"determiner{DET}", u"possessive_determiner{DET}", u"quantifier{DET}", 0, 1, 1, // u"demonstrative_determiner{DET}", removed - covered better by 'this' being a noun and the adjective being an ADJECTIVE_AFTER
		1, u"adverb", 0, 0, 1,
		// this entire pattern is rare and should not be encouraged (can be confused with an adjective to a noun)
		3, u"adjective*3{N_AGREE}", u"numeral_ordinal*2{N_AGREE}", u"noun*4{N_AGREE}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	// a female called Jane Finn
	// a Sikh called Bob
	// this was created to aid name recognition (name recognition help)
	cPattern::create(u"__NOUN", u"NAMED",
		1, u"__NOUN[*]{SUBOBJECT}", 0, 1, 1,
		2, u"verb|called", u"verb|named", VERB_PAST_PARTICIPLE, 1, 1,
		1, u"_NAME*-1{SUBOBJECT}", 0, 1, 1,
		0);
	// Johann the keeper's jailer
	cPattern::create(u"__NOUN", u"NAMED2",
		1, u"Proper Noun", 0, 1, 1,
		1, u"determiner{DET}", 0, 1, 1,
		1, u"noun", SINGULAR_OWNER | PLURAL_OWNER, 0, 1,
		1, u"noun", 0, 1, 1,
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
	cPattern::create(u"__NOUNRU{_BLOCK:_EXPLICIT_NOUN_DETERMINER_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS}", u"1",
		1, u"__NOUN[*]", 0, 1, 1,
		1, u"_VERBPASTPART*2{_BLOCK}", 0, 1, 1,
		2, u"_PP", u"__ALLOBJECTS_1{SUBOBJECT:_BLOCK}", 0, 1, 1,
		0);
	// I dare say [the little we know] won't be any good to you, sir.
	// any boy we know
	// I resemble *some one you have met*.
	// But I conclude she would have carried out *whatever plan she might have formed *.
	// First thing *I* see clearly is the road through the woods , not far from the station . // added 'first'
	/* - watch for bad matches! */
	cPattern::create(u"__NOUN{_BLOCK:_EXPLICIT_SUBJECT_VERB_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS}", u"R",
		// u"demonstrative_determiner{DET}", removed - covered better by 'this' being a noun and the adjective being an ADJECTIVE_AFTER
		14, u"interrogative_determiner|whichever", u"interrogative_determiner|whatever", u"determiner|the{DET}", u"determiner|a{DET}", u"determiner|an{DET}",
		u"quantifier|every{DET}", u"quantifier|each{DET}", u"adjective|many{DET}", u"demonstrative_determiner|that{DET}", u"quantifier|any{DET}", u"quantifier|some{DET}", u"adverb|too",
		u"numeral_ordinal|first", u"numeral_cardinal", 0, 1, 1, // My blooper had come back between *two errands she had*
		1, u"_ADJECTIVE*2", 0, 0, 1,  // The *only other* person I saw
		6, u"adjective*1", u"noun{N_AGREE}", u"Proper Noun", u"indefinite_pronoun{N_AGREE}", u"numeral_cardinal{N_AGREE}", u"verb*2", VERB_PRESENT_PARTICIPLE, 1, 1,  // adjective may be loosed from ProperNoun improperly - prevent match in 'The little Pilgrim was startled by this tone.'
		1, u"determiner|the{DET}", 0, 0, 1, // the facilities *the institute* affords
		5, u"Proper Noun*3{ANY:NAME:SUBJECT:PREFER_S1}", u"personal_pronoun_nominative*3{SUBJECT:PREFER_S1}", u"personal_pronoun*3{SUBJECT:PREFER_S1}", u"noun*2{N_AGREE}", u"adverb|there", NO_OWNER, 1, 1, // highly restrict and discourage to prevent unnecessary matches
		3, u"__ALLVERB", u"_COND{VERB}", u"_VERBPASSIVE", 0, 1, 1,
		4, u"__ALLOBJECTS_0*2", u"__ALLOBJECTS_1*2", u"_INFP*2", u"_REL1[*]*2", 0, 0, 1,
		0);
	// I shall make it my business to follow her *everywhere she goes*.
	cPattern::create(u"__NOUN{_BLOCK:_EXPLICIT_SUBJECT_VERB_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS}", u"R2",
		1, u"noun|everywhere{OBJECT}", 0, 1, 1,
		1, u"determiner|the{DET}", 0, 0, 1, // the facilities *the institute* affords
		4, u"Proper Noun*3{ANY:NAME:SUBJECT:PREFER_S1}", u"personal_pronoun_nominative*3{SUBJECT:PREFER_S1}", u"personal_pronoun*3{SUBJECT:PREFER_S1}", u"noun*2{N_AGREE}", NO_OWNER, 1, 1, // highly restrict and discourage to prevent unnecessary matches
		3, u"__ALLVERB", u"_COND{VERB}", u"_VERBPASSIVE", 0, 1, 1,
		4, u"__ALLOBJECTS_0*2", u"__ALLOBJECTS_1*2", u"_INFP*2", u"_REL1[*]*2", 0, 0, 1,
		0);
	//I would have done so if I had known how much *interest* you took in my plans . 
	//Festing had seen no grass like this in Canada and wondered how much *labor* it cost .
	//I know just how tired he was and how much *nerve* he required to keep himself going .
	//He bounded along, careless of how much *noise* he made .
	cPattern::create(u"__NOUN{_BLOCK:_EXPLICIT_SUBJECT_VERB_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS}", u"R3",
		1, u"relativizer|how{DET:OBJECT}", 0, 1, 1,
		1, u"adverb|much", 0, 1, 1,
		1, u"noun{N_AGREE}", 0, 1, 1,  // noise
		4, u"Proper Noun*1{ANY:NAME:SUBJECT:PREFER_S1}", u"personal_pronoun_nominative*1{SUBJECT:PREFER_S1}", u"personal_pronoun*1{SUBJECT:PREFER_S1}", u"noun*2{SUBJECT}", NO_OWNER, 1, 1, // highly restrict and discourage to prevent unnecessary matches
		3, u"__ALLVERB", u"_COND{VERB}", u"_VERBPASSIVE", 0, 1, 1,
		4, u"__ALLOBJECTS_0*2", u"__ALLOBJECTS_1*2", u"_INFP*2", u"_REL1[*]*2", 0, 0, 1,
		0);
	// everything she writes / those I had known / things you like
	cPattern::create(u"__NOUN{_BLOCK:_EXPLICIT_SUBJECT_VERB_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS}", u"R4",
		15, u"indefinite_pronoun|everybody", u"indefinite_pronoun|everyone", u"indefinite_pronoun|every one", u"indefinite_pronoun|everything", u"indefinite_pronoun|somebody", u"indefinite_pronoun|someone",
		u"indefinite_pronoun|something", u"indefinite_pronoun|anybody", u"indefinite_pronoun|anyone", u"indefinite_pronoun|anything", u"indefinite_pronoun|nobody", u"indefinite_pronoun|no one", u"indefinite_pronoun|nothing", u"demonstrative_determiner|those", u"noun|things", 0, 1, 1,
		4, u"Proper Noun*2{ANY:NAME:SUBJECT:PREFER_S1}", u"indefinite_pronoun*2{SUBJECT:PREFER_S1}", u"personal_pronoun_nominative*2{SUBJECT:PREFER_S1}", u"personal_pronoun*2{SUBJECT:PREFER_S1}", NO_OWNER, 1, 1, // highly restrict and discourage to prevent unnecessary matches
		3, u"__ALLVERB", u"_COND{VERB}", u"_VERBPASSIVE", 0, 1, 1,
		2, u"_ADVERB", u"_ADJECTIVE", 0, 0, 1,
		0);
	// she told him *the shape and size she wished it* .
	cPattern::create(u"__NOUN{_BLOCK:_EXPLICIT_SUBJECT_VERB_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS}", u"R5",
		6, u"interrogative_determiner|whichever", u"interrogative_determiner|whatever", u"determiner|the{DET}", u"quantifier|every{DET}", u"quantifier|each{DET}", u"quantifier|any{DET}", 0, 1, 1,
		1, u"noun{N_AGREE}", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		1, u"noun{N_AGREE}", 0, 1, 1,
		1, u"determiner|the{DET}", 0, 0, 1, // the facilities *the institute* affords
		5, u"Proper Noun*3{ANY:NAME:SUBJECT:PREFER_S1}", u"personal_pronoun_nominative*3{SUBJECT:PREFER_S1}", u"personal_pronoun*3{SUBJECT:PREFER_S1}", u"noun*2{N_AGREE}", u"adverb|there", NO_OWNER, 1, 1, // highly restrict and discourage to prevent unnecessary matches
		3, u"__ALLVERB", u"_COND{VERB}", u"_VERBPASSIVE", 0, 1, 1,
		4, u"__ALLOBJECTS_0*2", u"__ALLOBJECTS_1*2", u"_INFP*2", u"_REL1[*]*2", 0, 0, 1,
		0);

	cPattern::create(u"__APPNOUN{NOUN}", u"1",
		5, u"determiner*2{DET}", u"demonstrative_determiner*2{DET}", u"possessive_determiner*2{DET}", u"__HIS_HER_DETERMINER*3", u"_NAMEOWNER{DET}", 0, 0, 1,
		1, u"_ADJECTIVE_AFTER", 0, 0, 3, // this cannot contain VERBPAST, as it is not
																// possible yet to ascertain the difference between this and _S1.
																// otherwise, _NOUN would win over _S1 because it has fewer subcomponents, which was incorrect.
		4, u"noun*2{N_AGREE}", u"_NAME*3{GNOUN:NAME}", u"personal_pronoun_nominative*2{N_AGREE}", u"personal_pronoun*2{N_AGREE}", NO_OWNER, 1, 1, // this is tightly controlled -
																// no pronouns that could be adjectives -
		0);
	// you dears!  you scoundrel!  you old villain!
	// my son the doctor
	// the sooner the better (deleted from this pattern)
	// note that the _NOUN referred to in element 1 must not include _NOUN[6], as personal_pronouns are rarely used in this way.
	cPattern::create(u"__PNOUN{NOUN}", u"1",
		7, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET}", u"interrogative_determiner{DET}", u"quantifier{DET}", u"__HIS_HER_DETERMINER*1", u"_NAMEOWNER{DET}", 0, 0, 1,
		1, u"_ADJECTIVE{_BLOCK}", 0, 0, 3,
		// prefer _NAME over noun
		4, u"noun*1{N_AGREE}", u"personal_pronoun_nominative{N_AGREE}", u"_NAME{GNOUN:NAME}", u"personal_pronoun{N_AGREE}", NO_OWNER, 1, 1, // the one they want
		0);
	// all the boys, half the boys, double the sauce
	cPattern::create(u"__PNOUN{NOUN}", u"2",
		1, u"predeterminer*-1{DET}", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"__N1", 0, 1, 1,
		0);
	//cPattern::create(u"__NOUN{_FINAL_IF_ALONE}",u"5",
	//									1,u"__PNOUN{SUBOBJECT}",0,1,1,
	//									1,u",*3",0,0,1,
	//									1,u"__APPNOUN[*]{_BLOCK:RE_OBJECT}",0,1,1,
	//									0);
	//You impudent young blackguard !
	// you bad cat
	// you wretched old man !
	// you lazy mucker !
	// you young ruffian .
	// you ridiculous crab ?
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"YOU",
		1, u"personal_pronoun|you", 0, 1, 1,
		1, u"adjective*2", 0, 0, 2,
		4, u"numeral_cardinal|two", u"adjective|here*-3", u"noun*1", u"indefinite_pronoun|people", FEMALE_GENDER | MALE_GENDER, 1, 1,
		0);
	// you blooper - breaker !
	// You young word - twister !
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"YOU2",
		1, u"personal_pronoun|you", 0, 1, 1,
		1, u"adjective*2", 0, 0, 2,
		1, u"noun", 0, 1, 1,
		1, u"dash", 0, 1, 1,
		2, u"verb", u"noun*2", SINGULAR | VERB_PAST, 1, 1,
		0);
	// us lads
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"THEM",
		2, u"personal_pronoun_accusative|them", u"personal_pronoun_accusative|us", 0, 1, 1,
		4, u"numeral_cardinal|two", u"adjective|here*-3", u"noun*1", u"indefinite_pronoun|people", FEMALE_GENDER | MALE_GENDER | PLURAL, 1, 1,
		0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"WE",
		1, u"personal_pronoun_nominative|we", 0, 1, 1,
		1, u"numeral_cardinal", 0, 0, 1,
		2, u"numeral_cardinal", u"noun*1", FEMALE_GENDER | MALE_GENDER | PLURAL, 1, 1,
		0);
	// this is the same as "5" above but it is marked as a single object so "Iraqi weapons labs" is marked as a single object
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:NOUN}", u"N",
		5, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET}", u"interrogative_determiner{DET}", u"quantifier{DET}", 0, 0, 1,
		1, u"_ADJECTIVE{_BLOCK}", 0, 0, 1,
		// prefer _NAME over noun
		2, u"noun*1", u"_NAME{GNOUN:NAME}", NO_OWNER, 1, 1,
		1, u"noun*2{N_AGREE}", NO_OWNER, 1, 1, // this is tightly controlled -
		0);
	// anyone so simple as _NOUN
	// anyone as simple as
	// anyone as simple and honest as
	cPattern::create(u"__NOUN", u"U",
		1, u"indefinite_pronoun", 0, 1, 1,
		2, u"adverb|so", u"adverb|as", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"adjective{ADJ}", u"verb*1{ADJ}", VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 1, 1,                            // we can make cost of *1 for __NOUN[2]
		1, u"preposition|as", 0, 1, 1,
		1, u"__APPNOUN[*]{_BLOCK:RE_OBJECT}", 0, 1, 1,
		//1,u"is*1",0,0,1,
		0);
	cPattern::create(u"__NOUN", u"V",
		1, u"indefinite_pronoun", 0, 1, 1,
		2, u"adverb|so", u"adverb|as", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"adjective{ADJ}", u"verb*1{ADJ}", VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 1, 1,                            // we can make cost of *1 for __NOUN[2]
		1, u"coordinator", 0, 1, 1,
		2, u"adjective{ADJ}", u"verb*1{ADJ}", VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 1, 1,                            // we can make cost of *1 for __NOUN[2]
		1, u"preposition|as", 0, 1, 1,
		1, u"__APPNOUN[*]{_BLOCK:RE_OBJECT}", 0, 1, 1,
		//1,u"is*1",0,0,1,
		0);
	cPattern::create(u"__NOUN", u"W",
		1, u"indefinite_pronoun", 0, 1, 1,
		1, u"preposition|like", 0, 1, 1,
		1, u"__APPNOUN[*]{_BLOCK:RE_OBJECT}", 0, 1, 1,
		0);
	// makes no difference
	//cPattern::create(u"__NOUN", u"NAE",
	//									3, u"quantifier|neither", u"quantifier|any", u"quantifier|either", 0, 1, 1,
	//									1, u"preposition|of", 0, 1, 1,
	//									1, u"__NOUN[*]", 0, 1, 1,
	//									0);

	cPattern::create(u"__NOUN{_FINAL_IF_ALONE}", u"K",
		1, u"__NOUN[*]", 0, 1, 1,
		2, u"abbreviation|eg", u"abbreviation|e.g.", 0, 1, 1,
		1, u"__NOUN[*]{_BLOCK:RE_OBJECT}", 0, 1, 1,
		0);
	// MERGED into __NOUN "C"
	//cPattern::create(u"__NOUN{_FINAL_IF_ALONE:PNOUN}",u"",1,u"demonstrative_determiner",0,1,1,
	//                      1,u"_ADJECTIVE_AFTER",0,0,1, // they will declare 'these sane'
	//                      0);
	// created subobject so that address is not seen as a single object (so that the name in the address can be visible)
	// changed to _BLOCK so that __NOUN can be captured as a single object
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}", u"8",
		10, u"_LINE2ADDRESS{_BLOCK}", u"_LINE3ADDRESS{_BLOCK}", u"_POADDRESS", u"_MADDRESS",
		u"_DATE", u"_TIME", u"_ISBN", // He took office a year ago with a pledge
		u"_TELENUM", u"_NUMBER",
		u"demonstrative_determiner", 0, 1, 1,
		0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:PNOUN}", u"C", 3,
		u"personal_pronoun_accusative{N_AGREE}", // used to be OBJ
		u"personal_pronoun_nominative{N_AGREE}", // used to be SUBJ
		u"personal_pronoun{N_AGREE}", 0, 1, 1,
		//u"demonstrative_determiner{N_AGREE}", __NOUN[2]
		//u"--", // used to be in _N1,took out 11/4/2005-caused too many misparses see other changes *--*
// they will declare 'me sane' - u"_ADJECTIVE_AFTER", NOT applicable because this verb actually has two objects
// also 'me sane' or I sane is not really one noun.
1, u"reflexive_pronoun", 0, 0, 1,
0);
	// Yes , she , herself , in spite of *her* boasted strength had come at last to feel the need of being loved for the very weakness she had once despised . 
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:PNOUN}", u"CC", 3,
		u"personal_pronoun_accusative{N_AGREE}", // used to be OBJ
		u"personal_pronoun_nominative{N_AGREE}", // used to be SUBJ
		u"personal_pronoun{N_AGREE}",
		//u"demonstrative_determiner{N_AGREE}", __NOUN[2]
		//u"--", // used to be in _N1,took out 11/4/2005-caused too many misparses see other changes *--*
		0, 1, 1,
		// they will declare 'me sane' - u"_ADJECTIVE_AFTER", NOT applicable because this verb actually has two objects
		// also 'me sane' or I sane is not really one noun.
		1, u",", 0, 1, 1,
		1, u"reflexive_pronoun", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:PNOUN}", u"Y", 5,
		u"personal_pronoun_accusative|us{N_AGREE}", u"personal_pronoun_accusative|them{N_AGREE}",
		u"personal_pronoun_nominative|we{N_AGREE}", u"personal_pronoun|they{N_AGREE}",
		u"personal_pronoun|you{N_AGREE}", PLURAL, 1, 1,
		2, u"predeterminer|both", u"quantifier|each*1", 0, 1, 1, // You must bring us *each* a jewel.
		0);
	// this pattern must go after all nouns EXCEPT it must be before any noun patterns that use _NOUN as a subpattern
	//cPattern::create(u"_NOUN",u"",1,u"__NOUN[*]",0,1,1,0);
	// 6. correction to __NOUN[F] after studying matches to increase cost with long subjects - comment out __NOUN[H]
	// better than two years, more than two years
	//cPattern::create(u"__NOUN{_FINAL_IF_ALONE}",u"H",
	//									1,u"_ADJECTIVE",0,1,1,
	//									1,u"preposition|than",0,1,1,
	//									3,u"_NOUN_OBJ{SUBOBJECT}",u"__NOUN[*]{SUBOBJECT}",u"__NOUNREL{SUBOBJECT}",0,1,1,    
	//									0);
	// black beans and head lettuce
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}", u"J",
		1, u"predeterminer|both*-1", 0, 0, 1,
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ", u"__VERBALPHRASENOUN[*]{OBJECT}", 0, 1, 1,
		2, u"coordinator|and", u"&", 0, 1, 1,
		1, u"not", 0, 0, 1, // I want to be the party that pulls the wires and not the figures *that* dance on the front of the stage .
		4, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__NOUNREL{MOBJECT}", u"__VERBALPHRASENOUN[*]{OBJECT}", 0, 1, 1,
		1, u"predeterminer|both*-1", 0, 0, 1,
		0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:SINGULAR:MNOUN:_BLOCK}", u"O",
		1, u"quantifier|either*-3", 0, 0, 1,
		4, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"_PP", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1, // he had to make the journey between the drydock and his shop *either* by automobile or aeroplane .
		1, u"coordinator|or", 0, 1, 1,
		4, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__NOUNREL{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1, 0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:PLURAL:MNOUN:_BLOCK}", u"P",
		1, u"quantifier|neither*-4", 0, 0, 1,
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u"coordinator|nor", 0, 1, 1,
		4, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__NOUNREL{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1, 0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:SINGULAR:not:MNOUN:_BLOCK}", u"7",
		1, u"adverb|neither*-2", 0, 0, 1,
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u"coordinator|nor", 0, 1, 1,
		4, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__NOUNREL{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1, 0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:SINGULAR:MNOUN:_BLOCK}", u"A",
		1, u"adverb|neither*-2", 0, 0, 1,
		3, u"__NOUN[*]{not:MOBJECT}", u"_NOUN_OBJ{not:MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u"coordinator|nor", 0, 1, 1,
		4, u"__NOUN[*]{not:MOBJECT}", u"_NOUN_OBJ{not:MOBJECT}", u"__NOUNREL{not:MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"conjunction|but*-2", 0, 1, 1,
		4, u"__NOUN[*]{but:MOBJECT}", u"_NOUN_OBJ{but:MOBJECT}", u"__NOUNREL{but:MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		0);
	// beans, |  he,
	cPattern::create(u"__NOUN_S1", u"",
		1, u"coordinator", 0, 0, 1,
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, u",", 0, 1, 1, 0);
	// black beans, head lettuce, horribly rotten bananas and cheese
	// black beans, and head lettuce, horribly rotten bananas and cheese
	// N1, N2, N3, and N4 and N5. // N4 and N5 is a NOUN[J]
	cPattern::create(u"__MNOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}", u"Y",
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, u",", 0, 1, 1,
		1, u"__NOUN_S1", 0, 0, 4,
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, u",", 0, 0, 1,
		1, u"coordinator", 0, 1, 1,
		4, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__NOUNREL{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1, 0);
	// black beans, and head lettuce, horribly rotten bananas, and cheese // note extra comma which should be rare
		// this extra cost makes it less likely that this pattern will match the object of a verb and a
		// relativizer which most likely is the start of a relative phrase.
	// Marguerite with her box of jewels, the church scene, Siebel and his flowers, and Faust and Mephistopheles.
	// EMNOUN is the only case where extra cost is not required because in this case the , is used to offset
	//   a subchained compound noun at the end (Faust and Meph).
	cPattern::create(u"__EMNOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}", u"",
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		2, u"coordinator|and", u"&", 0, 1, 1,
		4, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__NOUNREL{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1, 0);
	cPattern::create(u"__MNOUN{_FINAL_IF_ALONE:PLURAL:MPLURAL:MNOUN:_BLOCK}", u"Z",
		3, u"__NOUN[*]{MOBJECT}", u"_NOUN_OBJ{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1,
		1, u"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		1, u",", 0, 1, 1, // The tea came at last, and Tuppence, rousing herself ...
		1, u"__NOUN_S1", 0, 0, 4,
		1, u"coordinator", 0, 1, 1,
		5, u"__NOUN[*]*3{MOBJECT}", u"_NOUN_OBJ*3{MOBJECT}", u"__NOUNREL*3{MOBJECT}", u"__EMNOUN{MOBJECT}", u"__VERBALPHRASENOUN[*]{MOBJECT}", 0, 1, 1, 0);
	// my son of the east Village (PP)
	// anxiety which underlay his tone (REL1)
	// the right to lay down arms (INFP)
	// CANNOT _BLOCK __NOUN 0 the following sentence will not parse correctly because countenance +PP will not find SUBJECT.
	// “ Don't go , John , ” said Mrs . Wilkinson , still forcing a smile to *her* countenance . 
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE}", u"9",
		//2,u"__NOUN[*]{_BLOCK}",u"_NOUN_OBJ{_BLOCK}",0,1,1,
		2, u"__NOUN[*]{SUBOBJECT}", u"_NOUN_OBJ{SUBOBJECT}", 0, 1, 1,
		3, u"__INTERPPB[*]{_BLOCK}", u"_DATE*1{FLOATTIME}", u"_TIME*1{FLOATTIME}", 0, 0, 1,
		2, u"_ADVERB", u"_ADJECTIVE", 0, 0, 1, // He gave a number *presently* which was his own in Panton Square . / I consider him *primarily responsible* for all the trouble that has occurred.
		3, u"_PP", u"_REL1", u"_INFP", 0, 1, 1,
		3, u"_PP*1", u"_REL1*1", u"_INFP*1", 0, 0, 3,
		0);
	// encourage any/all/either to be quantifiers when followed by 'of'
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE}", u"ANY",
		3, u"quantifier|any*-2{OBJECT}", u"quantifier|all*-1{OBJECT}", u"quantifier|either*-2{OBJECT}", 0, 1, 1,
		1, u"preposition|of{PREP}", 0, 1, 1,
		2, u"__NOUN[*]{_BLOCK}", u"_NOUN_OBJ{_BLOCK}", 0, 1, 1,
		1, u"adjective|left*-3", 0, 0, 1,
		0);
	// and medical man written all over him
	/* this pattern was removed because RULE 4 of L&L disallows anything in __IMPLIEDWITH from referencing the enclosing __NOUN
	// which would incorrectly exclude the noun preceding medical man from matching 'him'
	cPattern::create(u"__IMPLIEDWITH",u"",
										2,u"coordinator|and",u"&",0,1,1,
										1,u"__PNOUN[*]",0,1,1,
										1,u"_ADVERB",0,0,1,
										1,u"_VERBPASTPART{vB}",0,1,1,
										2,u"__ALLOBJECTS_0",u"__ALLOBJECTS_1",0,0,1,
										0);
	*/
	// so that a relative phrase can still be attributed with a noun that precedes it
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE}", u"X",
		2, u"__NOUN[*]", u"_NOUN_OBJ", 0, 1, 1,
		1, u",", 0, 1, 1,
		1, u"_REL1", 0, 1, 1, // ,u"__IMPLIEDWITH"
		1, u",", 0, 1, 1, // if , is left off, _PP in NOUN[9] and _PP at the end or _REL will multiplicatively combine
		0);
	cPattern::create(u"__NOUNREL{_FINAL_IF_ALONE:_ONLY_END_MATCH}", u"",
		2, u"__NOUN[*]", u"_NOUN_OBJ", 0, 1, 1,
		1, u",", 0, 1, 1,
		1, u"_REL1", 0, 1, 1,
		0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE}", u"B",
		1, u"_ADJECTIVE", 0, 0, 3,
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		4, u"__NOUN[*]*2", u"__MNOUN[*]*2", u"_NAME", u"__VERBALPHRASENOUN[*]", 0, 1, 1, // quoted nouns should be rare in general
		1, u"adverb", 0, 0, 1, // her " afternoon out . "
		2, u",*2", u".", 0, 0, 1, // *s discourages match of “ *Send* for the lookout , ” ordered Tom . 
		1, u"quotes", CLOSE_INFLECTION, 1, 1, 0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE}", u"G",
		7, u"determiner{DET}", u"demonstrative_determiner{DET}", u"possessive_determiner{DET}", u"interrogative_determiner{DET}", u"quantifier{DET}", u"__HIS_HER_DETERMINER*1", u"_NAMEOWNER{DET}", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 0, 3,
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		4, u"__NOUN[*]*-1", u"__MNOUN[*]", u"_NAME{GNOUN:NAME}", u"__VERBALPHRASENOUN[*]", 0, 1, 1, // compensate for possible missing determiner cost from internal __NOUN
		1, u"adverb", 0, 0, 1, // her " afternoon out . "
		2, u",", u".", 0, 0, 1,
		1, u"quotes", CLOSE_INFLECTION, 1, 1, 0);
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}", u"u",
		1, u"letter", 0, 1, 1,
		1, u".", 0, 1, 1,
		0);
	// no. 7
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}", u"Q",
		7, u"no", u"determiner|no", u"abbreviation|num", u"number", u"Proper Noun|no", u"Proper Noun|num", u"Proper Noun|n", 0, 1, 1,
		1, u".", 0, 0, 1,
		3, u"Number*-3", u"roman_numeral*-3", u"numeral_cardinal*-3", NO_OWNER, 1, 1,
		0);
	// company
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:GNOUN:SINGULAR}", u"M",
		1, u"__NOUN[*]", 0, 1, 1,
		1, u",", 0, 1, 1,
		1, u"_BUS_ABB", 0, 1, 1, 0);
	return 0;
}

// rules for patterns - any reference to any pattern must reference any occurrence of that pattern except for itself.
// First (and largest) create batch: abbreviation wrappers, __ADVERB / _ADJECTIVE
// families, then createNouns(), defineNames(), defineTimePatterns(),
// createMetaNameEquivalencePatterns(), plus interjection / __INTERPP / __INTERS1.
// Must run before any pattern that mentions __NOUN or _NAME.  Returns 0.
int createBasicPatterns(void)
{
	LFS
		//cPattern *p=NULL;
		cPattern::create(u"_ABB", u"1", 1, u"abbreviation", 0, 1, 1,
			1, u".", 0, 0, 1, 0);
	cPattern::create(u"_MABB", u"", 1, u"letter", 0, 1, 1,
		1, u".", 0, 1, 1, 0);
	cPattern::create(u"_ABB", u"3", 1, u"_MABB", 0, 0, 9,
		1, u"letter", 0, 1, 1,
		1, u".", 0, 0, 1, 0);
	cPattern::create(u"_MEAS_ABB", u"", 1, u"measurement_abbreviation", 0, 1, 1,
		1, u".", 0, 0, 1, 0);
	cPattern::create(u"_STREET_ABB", u"", 1, u"street_address_abbreviation", 0, 1, 1,
		1, u".", 0, 0, 1, 0);
	cPattern::create(u"_BUS_ABB", u"", 1, u"business_abbreviation", 0, 1, 1,
		1, u".", 0, 0, 1, 0);
	cPattern::create(u"_TIME_ABB", u"", 1, u"time_abbreviation", 0, 1, 1,
		1, u".", 0, 0, 1, 0);
	cPattern::create(u"_DATE_ABB", u"", 1, u"date_abbreviation", 0, 1, 1,
		1, u".", 0, 0, 1, 0);
	cPattern::create(u"_HON_ABB", u"", 1, u"honorific_abbreviation", 0, 1, 1,
		1, u".", 0, 0, 1, 0);
	// to and fro -- usage: he was tossed to and fro
	// by and by Mrs. Vandermeyer brought me some supper
	cPattern::create(u"__ADVERB", u"1",
		2, u"preposition|to", u"preposition|by", 0, 1, 1,
		2, u"coordinator|and", u"&", 0, 1, 1,
		2, u"preposition|fro", u"preposition|by", 0, 1, 1, 0);

	cPattern::create(u"__ADVERB", u"H",
		2, u"noun|hand", u"noun|arm", 0, 1, 1,
		2, u"preposition|in", u"preposition|over", 0, 1, 1,
		2, u"noun|hand", u"noun|arm", 0, 1, 1,
		0);
	cPattern::create(u"__ADVERB", u"SIDE",
		1, u"noun|side", 0, 1, 1,
		1, u"preposition|by", 0, 1, 1,
		1, u"noun|side", 0, 1, 1,
		0);
	cPattern::create(u"__ADVERB", u"FACE",
		1, u"noun|face", 0, 1, 1,
		1, u"preposition|to", 0, 1, 1,
		1, u"noun|face", 0, 1, 1,
		0);
	cPattern::create(u"__ADVERB", u"SORTOF",
		1, u"noun|sort", 0, 1, 1,
		1, u"preposition|of*1", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"8",
		4, u"adverb|little", u"noun|inch", u"noun|step", u"noun|bit", 0, 1, 1,
		1, u"preposition|by", 0, 1, 1,
		4, u"adverb|little", u"noun|inch", u"noun|step", u"noun|bit", 0, 1, 1,
		0);
	// Sometimes they will dig out a ditch or canal *all the way* from the edge of the pond up close to where the aspen grows .
	cPattern::create(u"__ADVERB{_FINAL_IF_ALONE}", u"ALLWAY",
		1, u"predeterminer|all", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"noun|way", 0, 1, 1,
		0);
	// this is used as an adverb, a preposition and a subordinator (included as a REL1 syntax)
	cPattern::create(u"__AS_AS", u"",
		1, u"adverb|as*-1", 0, 1, 1,
		2, u"adverb", u"adjective", 0, 1, 2,
		1, u"__NOUN*1", 0, 0, 1, // as brave hearts as / as silent a type as / as stern a voice as 
		2, u"preposition|as{P}", u"conjunction|as if", 0, 1, 1, 0); // when included in a _PP, mark this as a preposition
	cPattern::create(u"__AS_AS", u"2",
		1, u"adverb|as*-1", 0, 1, 1,
		2, u"adverb", u"adjective", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		2, u"adverb", u"adjective", 0, 1, 1,
		1, u"preposition|as{P}", 0, 1, 1, 0); // when included in a _PP, mark this as a preposition
/// TIME related adverbs
// this morning / this very morning / some morning
	cPattern::create(u"_ADVERB{FLOATTIME}", u"T",
		1, u"_ADVERB[*]", 0, 0, 1, // bright and early
		6, u"demonstrative_determiner{TIMEMODIFIER}", u"adjective{TIMEMODIFIER}", u"quantifier|some", u"quantifier|each", u"quantifier|every", u"quantifier|any", 0, 1, 1,
		1, u"adverb{ADV}", 0, 0, 1, // very
		6, u"month{MONTH}", u"daysOfWeek{DAYWEEK}", u"season{SEASON}", u"timeUnit{TIMECAPACITY}", u"dayUnit{TIMECAPACITY}", u"noun|time", 0, 1, 1,
		0);
	// some of the time / some of these days / some of the month
	cPattern::create(u"_ADVERB{FLOATTIME}", u"ST",
		2, u"quantifier|some", u"quantifier|most", 0, 1, 1,
		1, u"preposition|of", 0, 1, 1,
		2, u"determiner|the{TIMEMODIFIER}", u"demonstrative_determiner{TIMEMODIFIER}", 0, 1, 1,
		2, u"timeUnit{TIMECAPACITY}", u"noun|time", 0, 1, 1,
		0);
	// several times
	cPattern::create(u"_ADVERB{FLOATTIME}", u"ST2",
		1, u"quantifier|several", 0, 1, 1,
		1, u"noun|times", 0, 1, 1,
		0);
	// The summer before
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT1",
		2, u"predeterminer|half*-2", u"_ADVERB[*]", 0, 0, 1, // bright an early
		1, u"determiner|the{TIMEMODIFIER}", 0, 1, 1,
		6, u"month{MONTH}", u"daysOfWeek{DAYWEEK}", u"season{SEASON}", u"timeUnit*2{TIMECAPACITY}", u"dayUnit*1{TIMECAPACITY}", u"noun|time", 0, 1, 1,
		2, u"conjunction|before", u"conjunction|after", 0, 1, 1,
		0);
	// long before
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT1b",
		1, u"adverb|long", 0, 1, 1,
		2, u"conjunction|before", u"conjunction|after", 0, 1, 1,
		1, u"dayUnit", 0, 0, 1,
		0);
	// He was *no longer* studying the ground for Lightfoot's tracks.
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT1c",
		1, u"no", 0, 1, 1,
		1, u"adverb|longer", 0, 1, 1,
		0);
	// yesterday morning/tomorrow morning
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT1d",
		3, u"dayUnit|tomorrow{TIMEMODIFIER}", u"dayUnit|to-morrow{TIMEMODIFIER}", u"dayUnit|yesterday{TIMEMODIFIER}", 0, 1, 1,
		4, u"dayUnit|afternoon{TIMECAPACITY}", u"dayUnit|morning{TIMECAPACITY}", u"dayUnit|evening{TIMECAPACITY}", u"dayUnit|night{TIMECAPACITY}", 0, 1, 1,
		0);
	// The next summer 
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT2",
		2, u"predeterminer|half*-2", u"_ADVERB[*]", 0, 0, 1, // bright an early
		1, u"determiner|the{TIMEMODIFIER}", 0, 1, 1,
		7, u"adjective|following*-1", u"adjective|next*-1", u"adjective|succeeding*-1", u"adjective|previous*-1", u"verb|proceeding*-1", u"numeral_ordinal|first", u"numeral_ordinal|last", 0, 0, 1, // following
		6, u"month*1{MONTH}", u"daysOfWeek*4{DAYWEEK}", u"season*1{SEASON}", u"timeUnit*4{TIMECAPACITY}", u"dayUnit*4{TIMECAPACITY}", u"noun|time*2", 0, 1, 1, // daysOfWeek sun should be a noun, minute/second is also an issue
		0);
	// any time between dawn and sunset
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT3",
		2, u"quantifier|any{TIMEMODIFIER}", u"quantifier|some{TIMEMODIFIER}", 0, 1, 1,
		1, u"noun|time", 0, 1, 1,
		1, u"preposition|between", 0, 1, 1,
		5, u"month{MONTH}", u"daysOfWeek{DAYWEEK}", u"season{SEASON}", u"timeUnit{TIMECAPACITY}", u"dayUnit{TIMECAPACITY}", SINGULAR, 1, 1,
		1, u"coordinator|and", 0, 1, 1,
		5, u"month{MONTH}", u"daysOfWeek{DAYWEEK}", u"season{SEASON}", u"timeUnit{TIMECAPACITY}", u"dayUnit{TIMECAPACITY}", SINGULAR, 1, 1,
		0);
	// all the rest of **the summer
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT4",
		1, u"predeterminer|all", 0, 0, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"noun|rest", 0, 1, 1,
		1, u"preposition|of", 0, 1, 1,
		2, u"determiner|the{TIMEMODIFIER}", u"demonstrative_determiner|that{TIMEMODIFIER}", 0, 0, 1, // The rest of Tuesday / July
		6, u"month{MONTH}", u"daysOfWeek{DAYWEEK}", u"season{SEASON}", u"timeUnit{TIMECAPACITY}", u"dayUnit{TIMECAPACITY}", u"noun|time", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT5",
		1, u"predeterminer|all", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"noun|time", 0, 1, 1,
		0);
	cPattern::create(u"__ADVERB", u"AT5day1",
		1, u"dayUnit|day", 0, 1, 1,
		2, u"coordinator|and", u"&", 0, 1, 1,
		1, u"dayUnit|night", 0, 1, 1, 0);
	cPattern::create(u"__ADVERB", u"AT5day2",
		1, u"dayUnit|night", 0, 1, 1,
		2, u"coordinator|and", u"&", 0, 1, 1,
		1, u"dayUnit|day", 0, 1, 1, 0);
	// And she had *many a time* heard him declare that he was not a business man .
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT5b",
		1, u"quantifier|many", 0, 1, 1,
		1, u"determiner|a", 0, 1, 1,
		1, u"noun|time", 0, 1, 1,
		0);
	// Anything gone this *time* ?
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT5c",
		2, u"demonstrative_determiner|this", u"demonstrative_determiner|that", 0, 1, 1,
		1, u"noun|time", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT6",
		1, u"preposition|to", 0, 1, 1,
		1, u"dash|-*-2", 0, 1, 1,
		2, u"noun|day{DAY}", u"noun|morrow{DAY}", 0, 1, 1, 0);
	// Day after day/ week after week
	cPattern::create(u"_ADVERB{FLOATTIME}", u"AT7",
		1, u"timeUnit{TIMECAPACITY}", 0, 1, 1,
		1, u"preposition|after{TIMEMODIFIER}", 0, 1, 1,
		1, u"timeUnit{TIMECAPACITY}", 0, 1, 1,
		0);
	// Should this be an MS1?
	cPattern::create(u"_ADVERB{_FINAL}", u"AT8",
		9, u"relativizer|when", u"conjunction|before", u"conjunction|after", u"conjunction|as", u"conjunction|since", u"conjunction|until", u"conjunction|while", u"__AS_AS", u"quantifier|all*-1", 0, 1, 1,
		1, u"__S1*1{EVAL:_BLOCK}", 0, 1, 1, 0); // this has been probabilistically checked against Stanford and 1 is optimal
	cPattern::create(u"_ADVERB{_FINAL}", u"THEN",
		8, u"preposition|from", u"preposition|before", u"preposition|after", u"coordinator|and", u"preposition|since", u"preposition|until", u"preposition|till", u"preposition|by", 0, 1, 1,
		1, u"adverb|then", 0, 1, 1,
		0);

	cPattern::create(u"_ADVERB{_FINAL}", u"AT9",
		1, u"determiner|the", 0, 0, 1, // only used with 'next'
		4, u"quantifier|every", u"quantifier|each", u"adjective|next", u"quantifier|any", 0, 1, 1,
		1, u"noun|time", 0, 1, 1,
		1, u"__S1*1{EVAL:_BLOCK}", 0, 1, 1, 0);
	// Should this be an MS1?
	cPattern::create(u"_ADVERB{_FINAL}", u"AT10",
		1, u"determiner|the", 0, 1, 1,
		3, u"noun|moment", u"noun|instant", u"noun|second", 0, 1, 1,
		1, u"__S1*1{EVAL:_BLOCK}", 0, 1, 1, 0);
	// a day or two before
	cPattern::create(u"_ADVERB{_FINAL}", u"AT11",
		1, u"predeterminer|half*-1", 0, 0, 1,
		2, u"determiner|a", u"determiner|an", 0, 1, 1,
		3, u"timeUnit*1{TIMECAPACITY}", u"dayUnit*1{TIMECAPACITY}", u"simultaneousUnit*1{TIMECAPACITY}", 0, 1, 1,
		1, u"or", 0, 0, 1,
		1, u"numeral_cardinal|two", 0, 0, 1,
		5, u"adverb|earlier", u"adverb|later", u"adverb|ago", u"conjunction|before", u"conjunction|after", 0, 1, 1,
		0);
	//Jake was silent *a* moment .
	//He was silent *a* moment .
	//Mrs.Penniman, at this, looked thoughtful *a* moment .
	//Her father was silent *a* moment .
	//She was silent *a* moment .
	//Catherine was silent *a* moment .
	//The few shreds of conversation held them *back* a moment .
	//He fell silent *a* moment .
	//Will you *step* into the library a moment ? 
	cPattern::create(u"_ADVERB{_FINAL}", u"AT11m",
		2, u"determiner|a", u"determiner|an", 0, 1, 1,
		2, u"noun|moment", u"noun|instant", 0, 1, 1, // second is more likely an adjective
		2, u"adverb|earlier", u"adverb|later", 0, 0, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"AT11p",
		1, u"determiner|a", 0, 0, 1,
		1, u"adjective|few", 0, 0, 1,
		5, u"noun|moments*1", u"noun|seconds*1", u"noun|minutes*1", u"noun|days*1", u"noun|weeks*1", 0, 1, 1,
		3, u"adverb|earlier", u"adverb|later", u"adverb|ago", 0, 0, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"AT12",
		1, u"preposition|at", 0, 1, 1,
		1, u"noun|once*-4", 0, 1, 1,  // noun cost is 4
		0);
	// after July, after Tuesday, after summer, after dawn, before dusk
	cPattern::create(u"_ADVERB{_FINAL}", u"AT13",
		2, u"preposition|before*-1", u"preposition|after*-1", 0, 1, 1,
		5, u"month{MONTH}", u"daysOfWeek{DAYWEEK}", u"season{SEASON}", u"dayUnit{TIMECAPACITY}", u"adverb|long", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"AT14",
		1, u"not", 0, 0, 1,
		1, u"preposition|for", 0, 1, 1,
		1, u"adverb|long", 0, 1, 1,
		0);
	/// TIME related adverbs [end]
	// DISTANCE related adverbs [begin]
	// *10 feet below her* the boulder trembled.
	cPattern::create(u"_ADVERB{_FINAL}", u"AD1",
		2, u"Number", u"numeral_cardinal", 0, 1, 1,
		6, u"noun|miles", u"noun|yards", u"noun|feet", u"noun|inches", u"noun|meters", u"noun|metres", 0, 1, 1, // this is not meant to be exhaustive, just the most common units for this saying
		1, u"_PP", 0, 1, 1,
		0);
	// A short distance ahead of them
	cPattern::create(u"_ADVERB{_FINAL}", u"AD2",
		1, u"adverb", 0, 0, 1,
		1, u"determiner|a", 0, 1, 1,
		1, u"adjective", 0, 0, 1,
		1, u"noun|distance", 0, 1, 1,
		1, u"_PP", 0, 1, 1,
		0);
	// nearly a foot long
	cPattern::create(u"_ADVERB{_FINAL}", u"AD3",
		1, u"adverb", 0, 0, 1,
		3, u"determiner|a", u"Number", u"numeral_cardinal", 0, 1, 1,
		12, u"noun|mile", u"noun|yard", u"noun|foot", u"noun|inch", u"noun|meter", u"noun|metre", u"noun|miles", u"noun|yards", u"noun|feet", u"noun|inches", u"noun|meters", u"noun|metres", 0, 1, 1, // this is not meant to be exhaustive, just the most common units for this saying
		1, u"adjective*1", 0, 1, 1,
		0);

	// DISTANCE related adverbs [end]
	// lightly,
	cPattern::create(u"__ADV_S1", u"",
		3, u"adverb{ADV}", u"not{:not}", u"never{:never}", 0, 1, 4,
		1, u",", 0, 1, 1, 0);
	// lightly, slowly
	cPattern::create(u"__ADVERB{FLOATTIME}", u"2",
		1, u"__ADV_S1", 0, 0, 4,
		4, u"adverb{ADV}", u"not{:not}", u"never{:never}", u"politeness_discourse_marker", 0, 1, 4, 0);

	// lightly, brightly and painfully slowly
	cPattern::create(u"__ADVERB", u"3",
		1, u"__ADVERB", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"coordinator", 0, 1, 1,
		3, u"adverb{ADV}", u"not{:not}", u"never{:never}", 0, 1, 4, 0);
	cPattern::create(u"_ADVERB", u"1",
		1, u",", 0, 1, 1,
		1, u"__ADVERB", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	// No doubt
	cPattern::create(u"_ADVERB{_ONLY_BEGIN_MATCH}", u"ND",
		1, u"determiner|no", 0, 1, 1,
		1, u"noun|doubt", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"ALLRIGHT",
		1, u"adverb|all", 0, 1, 1,
		1, u"adverb|right*-1", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"HALFWAY",
		1, u"adverb|half", 0, 1, 1,
		1, u"dash*-1", 0, 0, 1,
		1, u"adverb|way*-1", 0, 1, 1,
		0);
	/* Causes incorrect parse - "Then"--Tuppence's voice shook a little--"there's a boy."
	cPattern::create(u"_ADVERB",u"2",1,u"quotes",OPEN_INFLECTION,1,1,
											1,u"__ADVERB",0,1,1,
											1,u"quotes",CLOSE_INFLECTION,1,1,
											0);
	*/
	cPattern::create(u"_ADVERB", u"3",
		1, u"brackets", OPEN_INFLECTION, 1, 1,
		1, u"__ADVERB", 0, 1, 1,
		1, u"brackets", CLOSE_INFLECTION, 1, 1,
		0);
	//cPattern::create(u"_ADVERB{_BLOCK:EVAL}",u"4",1,u"as",0,1,1,
	//                    1,u"__NOUN[*]",0,1,1,
	//                    1,u"_VERB[*]",0,1,1,
	//                    0);
	cPattern::create(u"_ADVERB{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_NO_REPEAT}", u"5", // used to be _FINAL
		2, u"__ADVERB", u"__AS_AS", 0, 1, 2, // as much as humanly possible.
		0);
	// better than two years, more than two years
	cPattern::create(u"_ADVERB{_FINAL}", u"A",
		1, u"__ADVERB{TIMEMODIFIER}", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		3, u"numeral_ordinal{TIMEMODIFIER}", u"numeral_cardinal{TIMEMODIFIER}", u"determiner{TIMEMODIFIER}", 0, 1, 1,
		2, u"timeUnit{TIMECAPACITY}", u"season{SEASON}", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"6",
		1, u"adverb|more", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		2, u"adverb|once*-1", u"adverb|twice", 0, 1, 1,
		0);
	// Mr . Bennet bade her blooper gravely and *more affectionately than* was his wont .
	cPattern::create(u"_ADVERB{_FINAL}", u"MT",
		1, u"adverb|more", 0, 1, 1,
		1, u"adverb", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		0);
	// a great amount worse/ a whole lot worse / a great deal worse
	// a little worse
	cPattern::create(u"_ADVERB{_FINAL}", u"B",
		1, u"determiner|a", 0, 1, 1,
		1, u"adjective", 0, 1, 1,
		3, u"noun|amount", u"noun|lot", u"noun|deal", 0, 0, 1,
		2, u"adjective|worse", u"adverb|better", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"M",
		1, u"quantifier|some", 0, 1, 1,
		2, u"adverb|how", u"noun|way", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"u",
		1, u"determiner|a", 0, 1, 1,
		4, u"adverb|little", u"adverb|bit*1", u"noun|mite", u"adjective|wee", 0, 1, 1, // make it preferable for another match 
		2, u"adverb", u"uncertainDurationUnit|while", 0, 0, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"TL",
		1, u"preposition|but", 0, 1, 1,
		1, u"adverb|little*-1", 0, 1, 1,
		0);
	cPattern::create(u"_ADVERB{_FINAL}", u"Y",
		1, u"predeterminer|all", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"pronoun|same", u"adverb|sooner", 0, 1, 1,
		0);
	// He held as much as a 200 pound fish - this has been taken by the pattern __AS_AS
	//cPattern::create(u"_ADVERB{_FINAL}",u"7",1,u"as",0,1,1,
	//														1,u"adverb|much",0,1,1,
	//														1,u"as",0,1,1,
	//														0);
	cPattern::create(u"_ADVERB{_FINAL}", u"AMONG",
		1, u"preposition|among", 0, 1, 1,
		1, u"personal_pronoun_accusative|them", 0, 1, 1,
		0);

	cPattern::create(u"_ADVERB", u"KIND",
		1, u"noun|kind", 0, 1, 1,
		1, u"preposition|of", 0, 1, 1,
		0);

	cPattern::create(u"_ADVERB{_FINAL}", u"AND",
		1, u"_ADVERB", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		1, u"_ADVERB", 0, 1, 1,
		0);

	defineNames();
	createMetaNameEquivalencePatterns();
	cPattern::create(u"_NUMBER{_NO_REPEAT}", u"1",
		1, u"numeral_cardinal", 0, 1, 1,
		2, u"numeral_cardinal", u",", 0, 0, 5, // u"and", covered by NOUN[J]
		1, u"numeral_cardinal", 0, 1, 1, 0);
	cPattern::create(u"__NUMBER", u"1", 1, u",", 0, 1, 1,
		1, u"Number", 0, 1, 1, 0);
	cPattern::create(u"_NUMBER{_NO_REPEAT:FLOATTIME}", u"2", 2, u"$*-1", u"#*-1", 0, 0, 1,
		2, u"Number", u"money", 0, 1, 1,
		1, u"__NUMBER*-1", 0, 0, 3,
		0);
	cPattern::create(u"_NUMBER{_NO_REPEAT}", u"3", 1, u"number*-1", 0, 0, 1, // the form of the word u"number"
		1, u"numeral_cardinal", 0, 1, 1, 0);
	cPattern::create(u"_NUMBER{_NO_REPEAT}", u"4", 1, u"Number", 0, 1, 1,
		5, u"%", u"letter|p", u"letter|f", u"letter|c", u"letter|k", 0, 1, 1, 0); // both % and p from BNC, f,c,k are temperatures
// from BNC
	cPattern::create(u"_NUMBER{_NO_REPEAT}", u"5", 2, u"$", u"#", 0, 1, 1,
		1, u"Number", 0, 1, 1,
		2, u"letter|m", u"letter|b", 0, 1, 1, 0);

	cPattern::create(u"_STEP", u"1", 1, u"brackets", OPEN_INFLECTION, 0, 1,
		2, u"letter", u"number", 0, 1, 1,
		2, u"brackets", u".", CLOSE_INFLECTION, 1, 1, 0);
	// lightly frozen | lightly matted | lightly rotting
	// VERB_PAST removed 12/16/2005
	// adverb use after adjective should be rare!
	// verb use as adjective should be not as common
	// if the proper noun is not preceded by an adverb and is a single word then it will be covered by a modifier to NOUN directly.
	// if the proper noun is not preceded by an adverb and is a multi word then it is a _NAME, which is directly used by _NOUN.
	// if the proper noun is in an owner form then it will be a NAMEOWNER
	cPattern::create(u"__ADJECTIVE", u"2",
		1, u"_ADVERB", 0, 0, 1,
		//7, u"adjective{ADJ}", u"verb*1{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"preposition|ex{ADJ}", u"noun{ADJ}", u"no{ADJ:no}", // removed *1 as cost so that
		10, u"adjective{ADJ}", u"verb*1{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"preposition|ex{ADJ}", u"noun{ADJ}", u"no{ADJ:no}", u"quantifier|more{ADJ}", u"indefinite_pronoun{ADJ}", u"reciprocal_pronoun{ADJ}",// removed *1 as cost so that
		VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 1, 2,                            // we can make cost of *1 for __NOUN[2]
		//										 1,u"adverb*1",0,0,1,  // why should adverb come AFTER adjective?
		0);                                                                // 2/3/2007
		// OWNER attributes deleted - why would ownership occur before a dash?
	cPattern::create(u"__ADJECTIVE", u"3",
		1, u"_ADVERB", 0, 0, 1,
		6, u"adjective{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"preposition|ex{ADJ}", u"noun*1{ADJ}", u"no{ADJ:no}", SINGULAR, 1, 2,
		1, u"dash|-", 0, 1, 1,
		7, u"adjective{ADJ}", u"verb{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"preposition|ex{ADJ}", u"noun{ADJ}", u"no{ADJ:no}",
		VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		5, u"adjective{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"noun{ADJ}", u"no{ADJ:no}",
		SINGULAR_OWNER | PLURAL_OWNER, 0, 1,
		1, u"adverb*1", 0, 0, 1, 0);
	// first-class passengers
	cPattern::create(u"__ADJECTIVE", u"A",
		1, u"_ADVERB", 0, 0, 1,
		7, u"adjective{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"preposition|ex{ADJ}", u"noun*1{ADJ}", u"no{ADJ:no}", u"quantifier|all", SINGULAR, 1, 2, // new all-metal construction
		1, u"dash", 0, 1, 1,
		3, u"noun*1{ADJ}", u"adjective|best*-4", u"noun|class*-4", SINGULAR, 1, 1,
		8, u"adverb*1", u"adjective{ADJ}", u"verb*1{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"preposition|ex{ADJ}", u"noun{ADJ}", u"no{ADJ:no}", 0, 0, 2,
		0);
	// Nobel Prize-winning
	cPattern::create(u"__ADJECTIVE", u"WIN",
		1, u"Proper Noun", 0, 0, 1,
		2, u"Proper Noun|prize", u"Proper Noun|award", 0, 1, 1, // new all-metal construction
		1, u"dash", 0, 0, 1,
		1, u"verb|winning*-3{ADJ}", 0, 1, 1,
		0);
	cPattern::create(u"__ADJECTIVE", u"UP",
		1, u"_ADVERB", 0, 0, 1,
		1, u"verb{ADJ}", VERB_PAST_PARTICIPLE, 1, 1, // hunched-up
		1, u"dash", 0, 0, 1,
		1, u"adverb|up", 0, 1, 1,
		1, u"adjective{ADJ}", 0, 0, 2,
		0);
	// 6. correction to __NOUN[F] after studying matches to increase cost with long subjects - comment out __NOUN[H]
	// better than two years, more than two years
	cPattern::create(u"__ADJECTIVE{_FINAL_IF_ALONE}", u"MTHAN",
		1, u"_ADJECTIVE", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		5, u"_NOUN_OBJ{SUBOBJECT}", u"__NOUN[*]{SUBOBJECT}", u"__NOUNREL{SUBOBJECT}", u"__S1*2{BLOCK:EVAL}", u"__VERBALPHRASENOUN[*]{SUBOBJECT}", 0, 1, 1,    // I shall treat you **worse than I have yet **. / he was only a year **older than I** am at present .
		0);
	cPattern::create(u"__ADJECTIVE{_FINAL_IF_ALONE}", u"QN",
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		1, u"adjective", 0, 0, 1,
		1, u"_NAME*1", 0, 1, 1,
		1, u"quotes", CLOSE_INFLECTION, 1, 1, 0);
	// somebody else's cat.
	cPattern::create(u"__ADJECTIVE", u"ELSE",
		1, u"indefinite_pronoun{ADJ}", NO_OWNER, 1, 1,
		1, u"adverb|else", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	// _NAME will ONLY be used in a case where a proper noun is not in an owner form AND it is preceded by an adverb.
	cPattern::create(u"__NADJECTIVE", u"",
		2, u"_ADVERB", u"commonProfession*1", 0, 0, 1, // cp=commonProfession What was author Jasper Fforde's first book?
		2, u"_NAMEOWNER", u"_NAME*1", 0, 1, 1,
		1, u"adverb*1", 0, 0, 1, 0);
	// an adjective consisting of a past verb is not allowed AFTER a noun (nor ex)
	// VERB_PAST removed 12/16/2005
	// adverb use after adjective should be rare!
	// must restrict a verb after a noun tightly! (lowered from 5 to 3 10/17/2007)
	// a noun as an adjective after another noun is probably not correct!
	cPattern::create(u"_ADJECTIVE_AFTER{_NOT_AFTER_PRONOUN}", u"",
		1, u"_ADVERB", 0, 0, 1,
		// every moment *RUNNING* | every moment GAINED (verb entry below) 11/4/2005
		5, u"adjective{ADJ}", u"numeral_ordinal", u"verb*2", u"_NUMBER", u"noun*2", VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		1, u"adverb*1", 0, 0, 1, 0); // he said , nodding -gravely .
// I thought that anything so bright and *lovely* should become mine .
	cPattern::create(u"_ADJECTIVE_AFTER", u"2",
		1, u"adverb|so", 0, 1, 1,
		1, u"adjective{ADJ}", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		1, u"adjective{ADJ}", 0, 1, 1,
		0);
	// lightly frozen,
	cPattern::create(u"__ADJ_S2", u"", 1, u"__ADJECTIVE[*]", 0, 1, 1,
		1, u",", 0, 1, 1, 0);
	// Ally's or Ally
	cPattern::create(u"__NADJ_S2", u"", 1, u"__NADJECTIVE[*]", 0, 1, 1, // __NAMEOWNER also provides PLURAL
		1, u",", 0, 1, 1, 0);
	// lightly frozen and at the same time melting
	// lightly frozen, magnificently rotting and melting
	cPattern::create(u"__ANDADJ", u"",
		1, u",", 0, 0, 1,
		3, u"coordinator", u"conjunction|but", u"conjunction|yet*-2", 0, 1, 1, // tired but restless / tired yet restless
		2, u"_PP*3", u"_REL1[*]*3", 0, 0, 1,  // much less probable to include these patterns
		1, u"__ADJECTIVE[*]", 0, 1, 1, 0); // may combine with __ADJECTIVE u"3" but should be very rare// __NAMEOWNER also provides PLURAL
// Ned and at the same time Ally's
	cPattern::create(u"__NANDADJ", u"",
		1, u",", 0, 0, 1,
		3, u"coordinator", u"conjunction|but", u"conjunction|yet*-2", 0, 1, 1, // tired but restless / tired yet restless
		2, u"_PP*3", u"_REL1[*]*3", 0, 0, 1,
		1, u"__NADJECTIVE[*]", 0, 1, 1, 0); // may combine with __ADJECTIVE u"3" but should be very rare// __NAMEOWNER also provides PLURAL
// a 121 lb hammer
	cPattern::create(u"__ADJECTIVE", u"4", 1, u"Number", 0, 1, 1,
		1, u"_MEAS_ABB{ADJ}", SINGULAR, 1, 1, 0);
	// a 121 to 200 pound fish
	cPattern::create(u"__ADJECTIVE", u"5",
		1, u"Number", 0, 1, 1,
		1, u"preposition|to", 0, 1, 1,
		1, u"Number", 0, 1, 1,
		0);
	// she was younger than he could have imagined
	cPattern::create(u"__ADJECTIVE", u"ATHAN",
		1, u"adjective", ADJECTIVE_COMPARATIVE, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		1, u"__S1{EVAL:_BLOCK}", 0, 1, 1,
		0);
	// the nearer we advanced to our goal
	cPattern::create(u"__ADJECTIVE", u"ACOMP",
		1, u"determiner|the", 0, 1, 1,
		1, u"adjective", ADJECTIVE_COMPARATIVE, 1, 1,
		1, u"__S1{EVAL:_BLOCK}", 0, 1, 1,
		0);
	// 7. helps with more .. than ... expressions
	//cPattern::create(u"__ADJECTIVE", u"ATHAN2",
	//														1, u"adverb|more", 0,1,1,
	//														1, u"__ADJECTIVE[*]", 0, 1, 1,
	//														1, u"preposition|than", 0, 1, 1,
	//														1, u"__S1{EVAL:_BLOCK}", 0, 1, 1,
	//														0);
	// lightly frozen, magnificently melting skeleton
	cPattern::create(u"__ADJECTIVE{APLURAL}", u"6",
		1, u"__ADJ_S2", 0, 1, 3,
		1, u"__ADJECTIVE[*]", 0, 1, 1,
		1, u"__ANDADJ", 0, 0, 1,
		0);
	cPattern::create(u"__NADJECTIVE{APLURAL}", u"7",
		1, u"__NADJ_S2", 0, 1, 3,
		1, u"__NADJECTIVE[*]*2", 0, 1, 1, // __NAMEOWNER also provides PLURAL
		1, u"__NANDADJ*-2", 0, 0, 1, // strongly encourage AND
		0);
	// magnificently melting and dripping skeleton
	cPattern::create(u"__ADJECTIVE{APLURAL}", u"8",
		1, u"__ADJECTIVE[*]", 0, 1, 1,
		1, u"__ANDADJ", 0, 1, 1,
		0);
	cPattern::create(u"__ADJECTIVE{SINGULAR}", u"C",
		1, u"quantifier|either*-3", 0, 0, 1, // determiner agrees with ST
		1, u"__ADJECTIVE[*]", 0, 1, 1,
		1, u"coordinator|or", 0, 1, 1,
		1, u"__ADJECTIVE[*]", 0, 1, 1,
		0);
	cPattern::create(u"__ADJECTIVE{SINGULAR}", u"D",
		1, u"quantifier|neither*-4", 0, 0, 1, // determiner agrees with ST - cost now matched with __NOUN[P]
		1, u"__ADJECTIVE[*]", 0, 1, 1,
		1, u"coordinator|nor", 0, 1, 1,
		1, u"__ADJECTIVE[*]", 0, 1, 1,
		0);
	// Ned and Ally's
	cPattern::create(u"__NADJECTIVE{APLURAL}", u"9",
		1, u"__NADJECTIVE[*]", 0, 1, 1, // __NAMEOWNER also provides PLURAL
		1, u"__NANDADJ", 0, 1, 1,
		0);

	cPattern::create(u"_ADJECTIVE{_FINAL_IF_ALONE}", u"1",
		2, u"quotes", u"brackets", OPEN_INFLECTION, 1, 1,
		3, u"__ADJECTIVE", u"_NAMEOWNER", u"noun*1{ADJ}", NO_OWNER, 1, 1, // block PLURAL
		2, u"quotes", u"brackets", CLOSE_INFLECTION, 1, 1,
		0);
	cPattern::create(u"_ADJECTIVE{_FINAL_IF_ALONE}", u"2", 2, u"__ADJECTIVE", u"__NADJECTIVE", 0, 1, 1, 0);

	defineTimePatterns();
	// (732) 598-6773
	cPattern::create(u"_TELENUM", u"1", 1, u"brackets", OPEN_INFLECTION, 1, 1,
		1, u"Number", 0, 1, 1,
		1, u"brackets", CLOSE_INFLECTION, 1, 1,
		1, u"telephone_number", 0, 1, 1, 0);

	createNouns();
	cPattern::create(u"_REF{_FINAL}", u"1", 1, u"brackets", OPEN_INFLECTION, 0, 1,
		1, u"pnum", 0, 1, 1,
		1, u".", 0, 1, 1,
		1, u"Number", 0, 1, 1,
		1, u"brackets", CLOSE_INFLECTION, 0, 1,
		0);

	// prepositional phrases interjected as asides
	// , from the very first,
	cPattern::create(u"__INTERPP", u"1",
		1, u",*2", 0, 1, 1, // a cost so that this doesn't interfere with commas used for compound adverbs, adjectives, and nouns
		7, u"_PP", u"__NOUN[*]*1", u"interjection", u"conjunction", u"_VERBREL1", u"_ADVERB", u"__VERBALPHRASENOUN", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	cPattern::create(u"__INTERPP", u"2",
		1, u",", 0, 1, 1,
		5, u"__NOUN[*]", u"_PP", u"interjection", u"conjunction", u"__VERBALPHRASENOUN", 0, 1, 1,
		1, u",", 0, 1, 1,
		5, u"__NOUN[*]", u"_PP", u"interjection", u"conjunction", u"__VERBALPHRASENOUN", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	// Rafid Ahmed Alwan al-Janabi (Arabic: رافد أحمد علوان‎, Rāfid Aḥmad Alwān; born 1968), known 
	cPattern::create(u"__INTERPPB", u"3",
		1, u"brackets", OPEN_INFLECTION, 1, 1,
		11, u"__NOUN[*]", u"_PP", u"__S1{_BLOCK}", u"_ADJECTIVE[*]", u"interjection", u"conjunction", u"coordinator", u":", u",", u";", u"__VERBALPHRASENOUN", 0, 1, 5,
		1, u"brackets", CLOSE_INFLECTION, 1, 1,
		0);
	cPattern::create(u"__INTERPP", u"4",
		1, u"_REF", 0, 1, 1, 0);
	cPattern::create(u"__INTERS1", u"1",
		1, u",", 0, 1, 1,
		4, u"_PP", u"interjection", u"conjunction", u"_ADVERB", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);

	return 0;
};

// Page 694, Section 9.4.2.1 Grammatical patterns
// Pattern 4 - verb + bare infinitive clause (dare, help, let, bade)
// Pattern 5 - verb + NP + bare infinitive clause (have, feel, help)
// _VERB_BARE_INF variants: "make/have/let/help NP V".  HOBJECT marks the
// NP; V_HOBJECT marks the matrix verb for object-agreement costing.
// Returns 0.
int createBareInfinitives(void)
{
	LFS
		//cPattern *p=NULL;
		// I make/made you go
		cPattern::create(u"_VERB_BARE_INF{VERB}", u"1",
			1, u"verbverb{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
			2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
			2, u"_ADVERB", u"_PP", 0, 0, 2,
			6, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
			u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", u"be{id:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
			0);
	// And **have him shoot** up the ship !
	// I shall ring and **have Blake screw** in another .
	// Why , what would you **have her do** ?	
	// Let me **have them mix** a cocktail for you?
	// We **have all come** away
	// I wasn't going to sit there calmly and **have him take** away all our money .
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"A",
		1, u"have|have{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		6, u"personal_pronoun_accusative{HOBJECT}", u"_NAME{HOBJECT}", u"predeterminer|all{HOBJECT}", u"indefinite_pronoun", u"personal_pronoun|you", u"personal_pronoun|it", 0, 1, 1, // must be very tightly controlled or results in many misparses
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		5, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// And **have the man drive** slowly
	// or **have his head cut** off from his shoulders .
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"B",
		1, u"have|have{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		2, u"determiner", u"possessive_determiner", 0, 1, 1,
		1, u"noun", 0, 1, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		5, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"2",
		1, u"verbverb{past:V_HOBJECT:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		6, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", u"be{id:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"C",
		1, u"have|had{past:V_HOBJECT:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 1, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		5, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"D",
		1, u"verbverb{past:V_HOBJECT:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		1, u"adverb|rather", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		2, u"verbverb", u"have|had", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		5, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I would make you go  | I won't make you go
	// you will/would make them protect you ?
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"3",
		1, u"_COND", 0, 1, 1,
		2, u"verbverb{vS:V_HOBJECT}", u"have|have{vS:V_HOBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		6, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", u"be{id:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I don't let him always allow them to depart
	// I do make you go  | I don't make you go - matches VERB 3
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"4",
		1, u"_DO{imp}", 0, 1, 1,
		2, u"verbverb{vS:V_HOBJECT}", u"have|have{vS:V_HOBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		6, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", u"be{id:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I have/had made you do that  / had made "his indecision of character" be
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"5",
		1, u"_HAVE", 0, 1, 1,
		2, u"verbverb{vB:V_HOBJECT}", u"have|had{vS:V_HOBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		6, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", u"be{id:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I am/was [making you go] and do that
	// you are [making them protect] you ?
		// Tuppence hated [letting her dangle] below the ceiling.
		// Tuppence loved [feeling the grass grow] under her feet.
		// Terrance hated [letting the grass grow] under her feet.
		// Harry felt her [hearing the glass slide] underneath the fitting.
		// Hillary thought [watching them float] under the water was quite wonderful.
		// David liked [making him do] that.
		// I lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms helping her finish the table.
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"6",
		1, u"_IS", 0, 1, 1,
		2, u"verbverb{vC:V_HOBJECT}", u"have|having{vS:V_HOBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		6, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", u"be{id:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// feeling, hearing, seeing, watching, telling, daring, letting, making, helping, having...
	// Tuppence hated thinking the grass grows.
	cPattern::create(u"__NOUN{_BLOCK:GNOUN:VNOUN}", u"6",
		1, u"_ADVERB", 0, 0, 1,
		2, u"verbverb{vE:V_AGREE:V_HOBJECT}", u"have|having{vE:V_AGREE:V_HOBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		5, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_AGREE:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
		0);
	// I will have made him go and do that | I will definitely have let him go and do that
	// I would have made him do that
		// I would have helped do that
	cPattern::create(u"_VERB_BARE_INF{VERB}", u"7",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		2, u"verbverb{vAB:V_HOBJECT}", u"have|had{vAB:V_HOBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		6, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}", u"be{id:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	return 0;
}

// Parallel to createBareInfinitives but the complement is a "think"/"say"
// verb that takes a clause (form "SYNTAX:Accepts S as Object").  Same
// HOBJECT / V_HOBJECT tagging.  Returns 0.
int createThinkBareInfinitives(void)
{
	LFS
		//cPattern *p=NULL;
		// I make/made you think
		cPattern::create(u"_THINK_BARE_INF{VERB}", u"1",
			1, u"verbverb{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
			2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
			2, u"_ADVERB", u"_PP", 0, 0, 2,
			1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
			0);
	// And **have him shoot** up the ship !
	// I shall ring and **have Blake screw** in another .
	// Why , what would you **have her do** ?	
	// Let me **have them mix** a cocktail for you?
	// We **have all come** away
	// I wasn't going to sit there calmly and **have him take** away all our money .
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"A",
		1, u"have|have{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		6, u"personal_pronoun_accusative{HOBJECT}", u"_NAME{HOBJECT}", u"predeterminer|all{HOBJECT}", u"indefinite_pronoun", u"personal_pronoun|you", u"personal_pronoun|it", 0, 1, 1, // must be very tightly controlled or results in many misparses
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// And **have the man drive** slowly
	// or **have his head cut** off from his shoulders .
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"B",
		1, u"have|have{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		2, u"determiner", u"possessive_determiner", 0, 1, 1,
		1, u"noun", 0, 1, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"2",
		1, u"verbverb{past:V_HOBJECT:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"C",
		1, u"have|had{past:V_HOBJECT:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 1, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"D",
		1, u"verbverb{past:V_HOBJECT:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		1, u"adverb|rather", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		2, u"verbverb", u"have|had", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I would make you go  | I won't make you go
	// you will/would make them protect you ?
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"3",
		1, u"_COND", 0, 1, 1,
		2, u"verbverb{vS:V_HOBJECT}", u"have|have{vS:V_HOBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I don't let him always allow them to depart
	// I do make you go  | I don't make you go - matches VERB 3
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"4",
		1, u"_DO{imp}", 0, 1, 1,
		2, u"verbverb{vS:V_HOBJECT}", u"have|have{vS:V_HOBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I have/had made you do that  / had made "his indecision of character" be
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"5",
		1, u"_HAVE", 0, 1, 1,
		2, u"verbverb{vB:V_HOBJECT}", u"have|had{vS:V_HOBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I am/was [making you go] and do that
	// you are [making them protect] you ?
		// Tuppence hated [letting her dangle] below the ceiling.
		// Tuppence loved [feeling the grass grow] under her feet.
		// Terrance hated [letting the grass grow] under her feet.
		// Harry felt her [hearing the glass slide] underneath the fitting.
		// Hillary thought [watching them float] under the water was quite wonderful.
		// David liked [making him do] that.
		// I lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms helping her finish the table.
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"6",
		1, u"_IS", 0, 1, 1,
		2, u"verbverb{vC:V_HOBJECT}", u"have|having{vS:V_HOBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// I will have made him go and do that | I will definitely have let him go and do that
	// I would have made him do that
		// I would have helped do that
	cPattern::create(u"_THINK_BARE_INF{VERB}", u"7",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		2, u"verbverb{vAB:V_HOBJECT}", u"have|had{vAB:V_HOBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
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
// to-infinitive family: __INFPSUB / __INFP / __INFPT / _INFP.  ITO marks
// "to"; IVERB marks the whole phrase; objects go through __ALLOBJECTS_*.
// Coordinated "not only to V but also to V" is _INFP[1].
void createInfinitivePhrases(void)
{
	LFS
		//cPattern *p=NULL;
		// goes with next INFP  [PRESENT SUB]
		cPattern::create(u"__INFPSUB{IVERB:_BLOCK}", u"",
			1, u"coordinator*-1{ITO}", 0, 1, 1,
			1, u"_ADVERB", 0, 0, 1,
			4, u"be{vS:id:V_OBJECT}", u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"have{vS:V_OBJECT}",
			VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
			3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // to act upon the ephemeral enthusiasms of an odd individual 8/28/2006
			0);
	// to u"infinitive" phrase as a noun object [PRESENT]
	// to send him / to send him and Bob
	// simple infinitive nonfinite verb phrase Quirk CGEL (3.56) vIS
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"1",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"verbal_auxiliary{V_AGREE}", 0, 0, 1, // to help eat this
		6, u"verb|like*-3", u"be{vS:id:V_OBJECT}", u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"have{vS:V_OBJECT}", u"SYNTAX:Accepts S as Object|make sure",
		VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		3, u"__INFP5SUB", u"__INFPSUB", u"__INFPT2SUB", 0, 0, 1,
		0);
	// verb bare infinitive form of infinitive phrase [PRESENT]
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"B",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"verbverb{vS:V_HOBJECT:V_AGREE}", u"have{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		5, u"verb{vS:V_AGREE:V_OBJECT}", u"does{vS:V_AGREE:V_OBJECT}", u"does_negation{vS:not:V_AGREE:V_OBJECT}",
		u"have{vS:V_AGREE:V_OBJECT}", u"have_negation{vS:not:V_AGREE:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		2, u"__INFP5SUB", u"__INFPSUB", 0, 0, 1,
		0);
	// verb bare infinitive form of infinitive phrase [PRESENT] with think pattern
	cPattern::create(u"__INFPT{IVERB:_BLOCK}", u"B",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"verbverb{vS:V_HOBJECT:V_AGREE}", u"have{vS:V_HOBJECT:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		2, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", 0, 0, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		1, u"__INFP3TSUB", 0, 0, 1,
		0);
	// to "infinitive" phrase as a noun object
	// to "send him" / to "send him and Bob"
	// simple infinitive nonfinite verb phrase Quirk CGEL (3.56) vIS
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"2",
		1, u"to{ITO}", 0, 1, 1,
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"verbal_auxiliary{V_AGREE}", 0, 0, 1, // to help eat this?
		4, u"be{vS:id:V_OBJECT}", u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"have{vS:V_OBJECT}",
		VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // added _PP 8/28/2006
		1, u"quotes", CLOSE_INFLECTION, 1, 1,
		0);
	// THINKSAY [PRESENT SUB]
	cPattern::create(u"__INFPT2SUB{IVERB:_BLOCK}", u"", 1, u"coordinator{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		0);
	// to u"infinitive" phrase as a noun object [PRESENT]
	// to think he should go / to think he should go and she should also go.
	// simple infinitive nonfinite verb phrase Quirk CGEL (3.56) vIS
	// I shall have *to tell him you are sorry*.
	cPattern::create(u"__INFPT{IVERB:_BLOCK}", u"1",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"verbal_auxiliary{V_AGREE}", 0, 0, 1, // to help think this through
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		3, u"_NOUN_OBJ", u"__NOUN[*]", u"_PP", 0, 0, 1,
		3, u"_ADJECTIVE", u"_ADVERB", u"preposition*2", 0, 0, 1,
		3, u"demonstrative_determiner|that{S_IN_REL}", u"quantifier|all*-2", u",", 0, 0, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		3, u"__INFP5SUB", u"__INFPSUB", u"__INFPT2SUB", 0, 0, 1,
		0);
	// he had reason 'to be proud' -- removed as a duplicate of u"__INFP{VERB}",u"1" 5/10/2006
	//cPattern::create(u"__INFP{VERB}",u"2",1,u"to",0,1,1,
	//                  1,u"_ADVERB",0,0,2,
	//                  1,u"_BE{id}",0,1,1,
	//                  1,u"_ADJECTIVE",0,1,2,
	//                  0);
	// goes with next INFP [PAST SUB]
	cPattern::create(u"__INFP3SUB{IVERB:_BLOCK}", u"", 1, u"coordinator{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_VERBPASTPART{vB}", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		0);
	// to have sent him / to have sent him and Bob [PAST]
	// u"B" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"3",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		1, u"_VERBPASTPART{vB}", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		1, u"__INFP3SUB*1", 0, 0, 1, // prefer __S1 to __INFP3SUB
		0);
	/*THINKSAY*/
	// goes with next INFP [PAST SUB]
	cPattern::create(u"__INFP3TSUB{IVERB:_BLOCK}", u"",
		1, u"coordinator{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_THINKPASTPART{vB}", 0, 1, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		0);
	// to have thought he wanted to go / to have thought he wanted this and she didn't stop him. [PAST]
	cPattern::create(u"__INFPT{IVERB:_BLOCK}", u"3",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		1, u"_THINKPASTPART{vB}", 0, 1, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		1, u"__INFP3TSUB", 0, 0, 1,
		0);
	// goes with next INFP 
	cPattern::create(u"__INFP4SUB{IVERB:_BLOCK}", u"",
		1, u"coordinator{ITO}", 0, 1, 1,
		1, u"verb{vC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		0);
	// to be sending him / to be sending him and Bob [PRESENT]
	// u"C" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"4",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		1, u"verb{vC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		1, u"__INFP4SUB", 0, 0, 1,
		0);
	/*THINKSAY*/
	// goes with next INFP [PRESENT SUB]
	cPattern::create(u"__INFP4TSUB{IVERB:_BLOCK}", u"",
		1, u"coordinator{ITO}", 0, 1, 1,
		1, u"SYNTAX:Accepts S as Object{vC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		0);
	// to be thinking he should go / to be thinking he should go and she should too. [PRESENT]
	// u"C" infinitive nonfinite verb phrase Quirk CGEL (3.56) 
	cPattern::create(u"__INFPT{IVERB:_BLOCK}", u"4",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"SYNTAX:Accepts S as Object{vC:V_OBJECT}", u"adjective|sure", VERB_PRESENT_PARTICIPLE, 1, 1, // to be sure she really did sink!
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		1, u"__INFP4TSUB", 0, 0, 1,
		0);
	// goes with next INFP [PRESENT SUB]
	cPattern::create(u"__INFP5SUB{IVERB:_BLOCK}", u"",
		1, u"coordinator{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		// encourage __INFP5SUB to stick closely together / I was just in time to see him ring the bell and get admitted to the house
		2, u"_BE*-1{_BLOCK}", u"verb|get*-1{V_AGREE:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		1, u"verb{vD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		0);
	// PASSIVE
	// to be blended / to get admitted [PRESENT]
	// "D" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"5",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		2, u"_BE{_BLOCK}", u"verb|get{V_AGREE:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		2, u"verb{vD:V_OBJECT}", u"does{vD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		3, u"__INFP5SUB", u"__INFPSUB", u"__INFPT2SUB", 0, 0, 1,
		0);

	// to have been sending him 
	// u"BC" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"6",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"verb{vBC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		0);
	// to have been sent 
	// u"BD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"7",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"verb{vBD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		1, u"__INFP5SUB", 0, 0, 1,
		0);
	// to be being examined
	// u"CD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"8",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"verb{vCD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		1, u"__INFP5SUB", 0, 0, 1,
		0);
	// to have been being examined
	// u"BCD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"9",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"verb{vBCD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		1, u"__INFP5SUB", 0, 0, 1,
		0);
	// to being examined
	// u"vrD" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFP{IVERB:_BLOCK}", u"A",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		4, u"_NOUN_OBJ{IOBJECT}", u"__NOUN[*]{IOBJECT}", // _NOUN* includes NOUN[D] and NOUN[E]
		u"demonstrative_determiner{IOBJECT}", u"possessive_determiner{IOBJECT}", 0, 0, 1, // there was never much chance to their being given me
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"verb{vrD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		1, u"__INFP5SUB", 0, 0, 1,
		0);
	/*THINKSAY*/
	// to have been thinking he should go to the store
	// u"BC" infinitive nonfinite verb phrase Quirk CGEL (3.56)
	cPattern::create(u"__INFPT{IVERB:_BLOCK}", u"7",
		1, u"to{ITO}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE{_BLOCK}", 0, 1, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"SYNTAX:Accepts S as Object{vBC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		0);

	// to be him,
	cPattern::create(u"__INFP_S1", u"", 2, u"__INFP", u"__INFPT", 0, 1, 1,
		1, u",", 0, 1, 1, 0);
	// to be blended, to be hurried, and to have been washed
	// not only to be interested but also to be surprised.
	cPattern::create(u"_INFP{_FINAL_IF_ALONE}", u"1",
		1, u"__INFP_S1", 0, 0, 3,
		2, u"__INFP", u"__INFPT", 0, 1, 1,
		1, u",", 0, 0, 1,
		2, u"coordinator", u"conjunction|but", 0, 1, 1,
		2, u"_INFP", u"__INFPT", 0, 1, 1, 0);
	cPattern::create(u"_INFP{_FINAL_IF_ALONE}", u"2",
		1, u"_ADVERB", 0, 0, 1,
		2, u"__INFP", u"__INFPT", 0, 1, 1, 0);
}

// Verb-group family: _IS/_DO/_HAVE/_COND/_BEEN, tense/aspect wrappers
// (_VERBPAST, _VERBPASSIVE, _VERBONGOING, ...), identity _VERB_ID, and the
// start of the __ALLOBJECTS_0/1/2 buckets.  Bare-infinitive patterns are
// registered later from createSecondaryPatterns1.  Returns 0.
int createVerbPatterns(void)
{
	LFS
		//cPattern *p=NULL;
		// Simple verb phrase 3.54 Quirk CGEL
		cPattern::create(u"_IS", u"1",
			1, u"_ADVERB", 0, 0, 1,
			2, u"is{V_AGREE:V_OBJECT}", u"is_negation{not:V_AGREE:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
			3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
			0);
	cPattern::create(u"_IS", u"2",
		1, u"_ADVERB", 0, 0, 1,
		2, u"is{past:V_AGREE:V_OBJECT}", u"is_negation{past:not:V_AGREE:V_OBJECT}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	cPattern::create(u"_DO", u"1",
		2, u"does{imp:V_AGREE}", u"does_negation{imp:not:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	cPattern::create(u"_DO", u"2",
		2, u"does{imp:past:V_AGREE}", u"does_negation{imp:past:not:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	cPattern::create(u"_HAVE", u"1",
		2, u"have{V_AGREE}", u"have_negation{not:V_AGREE}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	cPattern::create(u"_HAVE", u"2",
		2, u"have{past:V_AGREE}", u"have_negation{past:not:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	cPattern::create(u"_GET", u"1",
		1, u"_ADVERB", 0, 0, 1,
		1, u"verb|get{V_AGREE:V_OBJECT}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	cPattern::create(u"_GET", u"2",
		1, u"_ADVERB", 0, 0, 1,
		1, u"verb|get{past:V_AGREE:V_OBJECT}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	// u"A" structure of verb phrases from Quirk CGEL
	cPattern::create(u"_COND", u"",
		1, u"_ADVERB", 0, 0, 1,
		4, u"future_modal_auxiliary{future:V_AGREE}", u"negation_future_modal_auxiliary{not:future:V_AGREE}",
		u"modal_auxiliary{conditional:V_AGREE}", u"negation_modal_auxiliary{not:conditional:V_AGREE}", 0, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2,// preposition use should be rare!
		0);
	// you better say sorry!
	// you had better not steal that car!
	cPattern::create(u"_COND", u"B",
		1, u"have|had", 0, 1, 1,
		2, u"adverb|better", u"adverb|best", 0, 1, 1,
		1, u"not{not}", 0, 0, 1,
		0);
	// MODAL of u"A" structure of verb phrases from Quirk CGEL
	/* _COND2 is just a combination of _COND and a verbal auxiliary, which is really a verb taking a bare infinitive phrase (Pattern 4, 9.4.2.1 LGSWE) */
	cPattern::create(u"_COND2", u"",
		1, u"_ADVERB", 0, 0, 1,
		6, u"future_modal_auxiliary{future:V_AGREE}", u"negation_future_modal_auxiliary{not:future:V_AGREE}",
		u"modal_auxiliary{conditional:V_AGREE}", u"negation_modal_auxiliary{not:conditional:V_AGREE}",
		u"verbal_auxiliary{vS:V_AGREE}", u"past_verbal_auxiliary{past:V_AGREE}", 0, 1, 1, // he dares eat this?
		1, u"verbal_auxiliary{V_AGREE}", 0, 0, 1, // he would dare eat this?
		2, u"_ADVERB", u"_PP*1{_BLOCK}", 0, 0, 2,
		0);
	// everywhere where there is _BEEN, there is an _ADVERB,not,_INTERPP before it.
	// _BEEN is never used alone without a semantic tag with it.  Therefore, no semantic tags are necessary in the pattern itself.
	cPattern::create(u"_BEEN", u"", 1, u"been{V_AGREE}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1, 0);
	// everywhere where there is _BE, there is an _ADVERB,not,_INTERPP before it.
	cPattern::create(u"_BE", u"",
		1, u"be{id:V_AGREE}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1, 0);
	cPattern::create(u"_BEING", u"", 1, u"being{id}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1, 0);
	// I go / I go up
	// possessive_determiner is actually an adjective modifying an object missing from the sentence
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(u"_VERBPRESENT", u"1",
		2, u"possessive_determiner*4", u"verb|go", 0, 0, 1, // removed _ADVERB and added it to later patterns // the hidden object use should be very rare! / You go get the wheel.
		5, u"verb{vS:V_AGREE:V_OBJECT}", u"does{vS:V_AGREE:V_OBJECT}",
		u"does_negation{vS:not:V_AGREE:V_OBJECT}",
		u"have{vS:V_AGREE:V_OBJECT}", u"have_negation{vS:not:V_AGREE:V_OBJECT}",
		VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		0); // preposition use should be rare!
// special case - V_AGREE has been removed
// He need not say anything. - need is subject independent.
	cPattern::create(u"_VERBPRESENT", u"2",
		1, u"verb|need", 0, 1, 1,
		1, u"not", 0, 1, 1,
		5, u"verb{vS:V_OBJECT}", u"does{vS:V_OBJECT}", u"does_negation{vS:not:V_OBJECT}",
		u"have{vS:V_OBJECT}", u"have_negation{vS:not:V_OBJECT}",
		VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	cPattern::create(u"_VERBPRESENTC{VERB}", u"",
		1, u"_VERBPRESENT", 0, 1, 1,
		2, u"coordinator|and", u"coordinator|or", 0, 1, 1,
		1, u"_VERBPRESENT", 0, 1, 1,
		0);
	cPattern::create(u"_VERBPRESENTC{VERB}", u"O",
		1, u"quantifier|either*-2", 0, 1, 1,
		1, u"_VERBPRESENT", 0, 1, 1,
		1, u"coordinator|or", 0, 1, 1,
		1, u"_VERBPRESENT", 0, 1, 1,
		0);
	cPattern::create(u"_VERBPRESENTC{VERB}", u"P",
		1, u"quantifier|neither", 0, 1, 1,
		1, u"_VERBPRESENT", 0, 1, 1,
		1, u"coordinator|nor", 0, 1, 1,
		1, u"_VERBPRESENT", 0, 1, 1,
		0);
	// eliminated as preposition is already included in _IS 2/20/2007
	//cPattern::create(u"_VERB_ID{VERB}",u"1",
	//                        1,u"_IS{vS:id}",0,1,1,
	//                        1,u"preposition*2",0,0,1,0); // preposition use should be rare!
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(u"_THINKPRESENT", u"1",
		1, u"SYNTAX:Accepts S as Object{V_AGREE:vS}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		2, u"_ADVERB", u"_PP*1", 0, 0, 2, 0);
	cPattern::create(u"_THINKPRESENTFIRST", u"",
		1, u"SYNTAX:Accepts S as Object{V_AGREE:vS}", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		2, u"_ADVERB", u"_PP*1", 0, 0, 2, 0);
	// he need not think such a thing is possible.  - this is special case, must match any case subject, he, she, it they, I, you
	cPattern::create(u"_THINKPRESENT{BLOCK}", u"2",
		1, u"verb|need", 0, 1, 1,
		1, u"not", 0, 1, 1,
		1, u"SYNTAX:Accepts S as Object", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// going / being / seeing
	cPattern::create(u"_VERBONGOING", u"", 2, u"_ADVERB", u"possessive_determiner*2", 0, 0, 2, // possessive determiner should be rare
		3, u"verb{V_AGREE:V_OBJECT}", u"does{V_AGREE:V_OBJECT}", u"have{V_AGREE:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
	cPattern::create(u"_THINKONGOING", u"",
		1, u"_ADVERB", 0, 0, 2,
		1, u"SYNTAX:Accepts S as Object{V_AGREE:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		0);
	// I quickly went lurchingly
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(u"_VERBPAST{VERB}", u"",//1,u"_ADVERB",0,0,1, added ADVERB to subsequent patterns
		7, u"verb{past:V_AGREE}",
		u"does{past:V_AGREE}", u"does_negation{not:past:V_AGREE}",
		u"have{past:V_AGREE}", u"have_negation{not:past:V_AGREE}",
		u"is{past:id:V_AGREE}", u"is_negation{not:past:id:V_AGREE}",
		VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		0);
	cPattern::create(u"_VERBPASTC{VERB}", u"",
		1, u"_VERBPAST", 0, 1, 1,
		2, u"coordinator|and", u"coordinator|or", 0, 1, 1,
		1, u"_VERBPAST", 0, 1, 1,
		0);
	cPattern::create(u"_VERBPASTC{VERB}", u"O",
		1, u"quantifier|either*-2", 0, 1, 1,
		1, u"_VERBPAST", 0, 1, 1,
		1, u"coordinator|or", 0, 1, 1,
		1, u"_VERBPAST", 0, 1, 1,
		0);
	cPattern::create(u"_VERBPASTC{VERB}", u"P",
		1, u"quantifier|neither", 0, 1, 1,
		1, u"_VERBPAST", 0, 1, 1,
		1, u"coordinator|nor", 0, 1, 1,
		1, u"_VERBPAST", 0, 1, 1,
		0);
	// She[tuppence] ran rather than walked down the street .
	cPattern::create(u"_VERBPASTCORR", u"", // this cannot have the VERB flag because _VERBPAST already has VERB
		1, u"_VERBPAST", 0, 1, 1,
		1, u"adverb|rather", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		1, u"_VERBPAST{_BLOCK}", 0, 1, 1,
		0);
	// I quickly went lurchingly
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(u"_VERBPASTPART", u"",//1,u"_ADVERB",0,0,1, added ADVERB to subsequent patterns
		7, u"verb{V_OBJECT}", u"does{V_OBJECT}", u"does_negation{V_OBJECT:not}", // for general verbs, prefer past over past_participle, if there is a choice.
		u"have{V_OBJECT}", u"have_negation{V_OBJECT:not}",                    // for the others, past_participle is unique, so there is no choice.
		u"is{V_OBJECT}", u"is_negation{V_OBJECT:not}", VERB_PAST_PARTICIPLE, 1, 1,
		0);
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(u"_THINKPAST", u"1",
		1, u"SYNTAX:Accepts S as Object{past:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2, 0);// preposition use should be rare!
// She[tuppence] felt rather than saw Julius wasn't even trying to hit the ball .
	cPattern::create(u"_THINKPAST", u"2",
		1, u"SYNTAX:Accepts S as Object{past:V_AGREE}", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		1, u"adverb|rather", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		1, u"SYNTAX:Accepts S as Object", VERB_PAST | VERB_PAST_THIRD_SINGULAR | VERB_PAST_PLURAL, 1, 1,
		0);
	// Simple verb phrase 3.54 Quirk CGEL
	cPattern::create(u"_THINKPASTPART", u"",
		1, u"SYNTAX:Accepts S as Object*1{V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1, // for think, prefer past over past_participle (past is the same form as past_participle).
		3, u"_ADVERB", u"preposition*2", u"_PP*1{_BLOCK}", 0, 0, 2, 0);// preposition use should be rare!
// I would go | would not go | I will go | I will not go
// I dare say | I make do
// I wouldn't go | I won't go
// I would be / [I would be going (to see him) removed - covered in VERBPASSIVE[3])
// u"A" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"1",
		1, u"_COND", 0, 1, 1,
		5, u"__INTERS1", u"_ADVERB*2", u"predeterminer|both", u"quantifier|each", u"reflexive_pronoun", 0, 0, 1, // we will *each* give of our fire .
		2, u"_VERBPRESENT", u"_BE{vS:V_OBJECT:id}", 0, 1, 1, 0);
	// u"A" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_THINK", u"1",
		1, u"_COND2", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*2", 0, 0, 1,
		1, u"_THINKPRESENTFIRST", 0, 1, 1, 0);
	// I do go | do not go
	// u"A" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"3",
		1, u"_DO{imp}", 0, 1, 1,
		3, u"__INTERS1", u"_ADVERB*1", u"reflexive_pronoun", 0, 0, 1,
		1, u"_VERBPRESENT", 0, 1, 1, 0);
	// I do think
	cPattern::create(u"_THINK", u"3",
		1, u"_DO{imp}", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"_THINKPRESENTFIRST", 0, 1, 1, 0);
	// I am/was going
	// I am/was being
	// u"CL" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"4",
		1, u"_IS", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		2, u"verb{vC:V_OBJECT}", u"being{vC:id:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
// I am/was thinking
	cPattern::create(u"_THINK", u"4",
		1, u"_IS", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"_ADVERB", 0, 0, 1, 0);
	// I have/had gone  / (had "his indecision of character" been - REMOVED - inappropriate for this pattern) 1/11/2006
	// Bill had been
	// "B" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"5",
		1, u"_HAVE", 0, 1, 1,
		3, u"__INTERS1", u"_ADVERB*1", u"reflexive_pronoun", 0, 0, 1,
		2, u"_VERBPASTPART{vB}", u"_BEEN{vB:id}", 0, 1, 1, 0);
	// I have thought
	cPattern::create(u"_THINK", u"5",
		1, u"_HAVE", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"_THINKPASTPART{vB}", 0, 1, 1, 0);
	// I would/will have gone | I would/will definitely have gone
	// u"AB" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"6",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"_VERBPASTPART{vAB}", 0, 1, 1, 0);
	// I would/will have thought
	cPattern::create(u"_THINK", u"6",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"_THINKPASTPART{vAB}", 0, 1, 1, 0);
	// I have been sending | I had been sending
	// u"BC" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"8",
		1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE", 0, 1, 1,
		1, u"_BEEN", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		3, u"verb{vBC:V_OBJECT}", u"does{vBC:V_OBJECT}", u"have{vBC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
// I had been telling
	cPattern::create(u"_THINK", u"8", 1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE", 0, 1, 1,
		1, u"_BEEN", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vBC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		2, u"_ADVERB", u"preposition*2", 0, 0, 1, 0); // preposition use should be rare!
// I will be sending | I would be sending | I will not be sending | I would not be sending
// u"AC" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"9",
		1, u"_COND", 0, 1, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		3, u"verb{vAC:V_OBJECT}", u"does{vAC:V_OBJECT}", u"have{vAC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
// I will be telling | I would be telling | I will not be telling | I would not be telling
	cPattern::create(u"_THINK", u"9",
		1, u"_COND", 0, 1, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vAC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		2, u"_ADVERB", u"preposition*2", 0, 0, 1, 0); // preposition use should be rare!
/*
// is/was going to be | is/was reputed to be | is reputed to be running
// this is not a verb phrase.   It is a present verb followed by an infinitive clause.
cPattern::create(u"_VERB_ID",u"8",
									1,u"_IS",0,1,1,
									1,u"_ADVERB",0,0,2,
									1,u"verb",VERB_PRESENT_PARTICIPLE|VERB_PAST,1,1,
									1,u"preposition|to",0,1,1,
									1,u"_ADVERB",0,0,2,
									1,u"_BE",0,1,1,
									1,u"_VERBONGOING",0,0,1,
									0);
*/
// I would/will have been telling
// u"ABC" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERB{VERB}", u"A",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1, // {_BLOCK} removed 8/17 will block negation
		1, u"_BEEN", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		3, u"verb{vABC:V_OBJECT}", u"does{vABC:V_OBJECT}", u"have{vABC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		0);// preposition use should be rare!
// I would have been telling / I will have been telling
	cPattern::create(u"_THINK", u"A",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1, // {_BLOCK} removed 8/17 will block negation
		1, u"_BEEN", 0, 1, 1,
		2, u"__INTERS1", u"_ADVERB*1", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vABC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		2, u"_ADVERB", u"preposition*2", 0, 0, 1, 0);// preposition use should be rare!
// PASSIVE CONSTRUCTION
// I am sent | I was sent
// I got sent | I got stuck
// I was told it was M. Fouquet .
// u"D" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"1",
		2, u"_IS", u"_GET", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"_VERBPASTPART{vD}", 0, 1, 1, 0);
	cPattern::create(u"_THINKPASSIVE{VERB}", u"1",
		1, u"_IS", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"_THINKPASTPART{vD}", 0, 1, 1, 0);
	// I am being sent | I was being sent
	// u"CD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"2",
		1, u"_IS", 0, 1, 1,
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"_VERBPASTPART{vCD}", 0, 1, 1, 0);
	// I am being told it was M. Fouquet .
	cPattern::create(u"_THINKPASSIVE{VERB}", u"2",
		1, u"_IS", 0, 1, 1,
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"_THINKPASTPART{vCD}", 0, 1, 1, 0);
	// I will be sent | I would be sent | I will not be sent | I would not be sent
	// u"AD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"3",
		1, u"_COND2", 0, 1, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		3, u"verb{vAD:V_OBJECT}", u"does{vAD:V_OBJECT}", u"have{vAD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
// I will be told | I would be told | I will not be told | I would not be told
	cPattern::create(u"_THINKPASSIVE{VERB}", u"3",
		1, u"_COND", 0, 1, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vAD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		2, u"_ADVERB", u"preposition*2", 0, 0, 1, 0); // preposition use should be rare!
// I have been sent | I had been sent
// also (incorrect) I been gone
// u"BD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"4", 1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE", 0, 1, 1,
		1, u"predeterminer|both", 0, 0, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		3, u"verb{vBD:V_OBJECT}", u"does{vBD:V_OBJECT}", u"have{vBD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
// I have been told | I had been told
	cPattern::create(u"_THINKPASSIVE{VERB}", u"4", 1, u"_ADVERB", 0, 0, 1,
		1, u"_HAVE", 0, 1, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vBD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		2, u"_ADVERB", u"preposition*2", 0, 0, 1, 0); // preposition use should be rare!
// I would/will have been crushed
// u"ABD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"5",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1, // {_BLOCK} removed 8/17 will block negation
		1, u"_BEEN", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		3, u"verb{vABD:V_OBJECT}", u"does{vABD:V_OBJECT}", u"have{vABD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
// I would/will have been told
	cPattern::create(u"_THINKPASSIVE{VERB}", u"5",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1, // {_BLOCK} removed 8/17 will block negation
		1, u"_BEEN", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"SYNTAX:Accepts S as Object{vABD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		2, u"_ADVERB", u"preposition*2", 0, 0, 1, 0); // preposition use should be rare!
// I may be being examined
// u"ACD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"6",
		1, u"_COND", 0, 1, 1,
		1, u"_BE{_BLOCK}", 0, 1, 1,
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		1, u"_VERBPASTPART{vACD}", 0, 1, 1, 0);
	// I have been being examined
	// u"BCD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"7",
		1, u"_HAVE", 0, 1, 1, // {_BLOCK} removed 8/17 will block negation, past
		1, u"_BEEN", 0, 1, 1,
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		3, u"verb{vBCD:V_OBJECT}", u"does{vBCD:V_OBJECT}", u"have{vBCD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!
// I may have been being examined
// u"ABCD" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(u"_VERBPASSIVE{VERB}", u"8",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1, // {_BLOCK} removed 8/17 will block negation
		1, u"_BEEN", 0, 1, 1,
		1, u"_BEING{_BLOCK}", 0, 1, 1,
		1, u"__INTERS1", 0, 0, 1,
		3, u"verb{vABCD:V_OBJECT}", u"does{vABCD:V_OBJECT}", u"have{vABCD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		0); // preposition use should be rare!

	cPattern::create(u"__BELIEVE", u"",
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		1, u"verb|believe*4", 0, 1, 1,
		0);

	// _BE should be rarely used as an active verb by itself (we be giants)
	cPattern::create(u"__ALLVERB", u"",
		2, u"_ADVERB", u"__BELIEVE", 0, 0, 1,
		10, u"_VERB", u"_VERBPAST{V_OBJECT}", u"_VERBPASTC{V_OBJECT}", u"_VERBPASTCORR{V_OBJECT}", u"_VERBPRESENT{VERB}", u"_VERBPRESENTC{VERB}", u"_BE*4{vS:V_OBJECT:id:VERB}", u"_VERB_BARE_INF",
		u"is{vS:id:V_AGREE:V_OBJECT:VERB}", u"is_negation{vS:id:not:V_AGREE:V_OBJECT:VERB}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		2, u"particle|in*1{PT}", u"particle|on{PT}", 0, 0, 1,
		0);
	// __ALLOBJECTS - should u"_ADJECTIVE" really be an object?  If so, it overlaps with _NOUN, so if it is added back this overlap
	// must be avoided.
	// _ALLOBJECTS should represent all cases of an object of a verb
	// speak/say - add, admit, announce, answer, argue, assert, ask, beg, boast, claim, comment, conclude, confess, cry,
	//             declare, exclaim, explain, hint, insist, maintain, not, object, observe, order, promise, protest, recall,
	//             remark, repeat, reply, report, say, shout, state, tell, think, urge, warn, whisper, wonder, write
	//             falter, mumble, murmur, mutter, snap, sneer, sob

	return 0;
}

// Mid-level constituents that need nouns and verbs already defined:
// __APPNOUN / __APPTCNOUN (appositives), __NOUNREL, _REL1 / _RELQ,
// _VERBREL1/_VERBREL2 (participial / "particularly for NP V-ing"),
// the rest of __ALLOBJECTS_*, then createBareInfinitives /
// createThinkBareInfinitives.  Relative-clause patterns are tagged EVAL
// so agreement costing walks them.
void createSecondaryPatterns1(void)
{
	LFS
		//cPattern *p=NULL;
		// introduced because otherwise __INTERPP can hide a noun inside itself and be missed
		// by considering the entire APPNOUN as an object in identifyObjects.  But if APPNOUN is not considered as an object,
		// and without the following pattern, __INTERPP will be identified but not the beginning noun.
		cPattern::create(u"__APPTCNOUN{NOUN:EVAL}", u"",
			4, u"determiner*2{DET}", u"demonstrative_determiner*2{DET}", u"possessive_determiner*2{DET}", u"__HIS_HER_DETERMINER*3", 0, 0, 1,
			1, u"_ADJECTIVE_AFTER", 0, 0, 2, // this cannot contain VERBPAST, as it is not possible yet to ascertain the difference between this and _S1.
																	// otherwise, _NOUN would win over _S1 because it has fewer subcomponents, which was incorrect.
			4, u"noun*4{N_AGREE}", u"_NAME*4{GNOUN:NAME}", u"personal_pronoun_nominative*3{N_AGREE}", u"personal_pronoun*3{N_AGREE}", NO_OWNER, 1, 1, // this is tightly controlled -
			0);
	// this pattern contains a relative clause!
	cPattern::create(u"__APPNOUN{EVAL}", u"2",
		1, u"__APPTCNOUN{SUBJECT}", 0, 1, 1,
		2, u"__INTERPP[*]*1{_BLOCK}", u"__INTERPPB[*]*1{_BLOCK}", 0, 0, 1,
		// no pronouns that could be adjectives -
					// previously incorrect - 'round and lowered his' with 'round and lowered' being an adjective and 'his' being a noun!
					// then the next noun would be considered a secondary noun, which is incorrect. 'looked round and lowered his voice'
		2, u"__ALLVERB", u"_VERBPASSIVE", 0, 1, 1,     // the news I brought
		1, u"adverb", 0, 0, 1, // bind the adverb to the preceding verb without the added cost 2 of the following OBJECTs
		//2,u"preposition*4",u"_ADVERB*2",0,0,1, // hanging preposition BNC A01 u"the power I dreamed of" already in ALLOBJECTS
		2, u"__ALLOBJECTS_0*2", u"__ALLOBJECTS_1*2", 0, 0, 1, // the books I bought Bob
		0);
	//cPattern::create(u"__APPNOUN2{NOUN}",u"",
	//  4,u"determiner*2{DET}",u"demonstrative_determiner*2{DET}",u"possessive_determiner*2{DET}",u"__HIS_HER_DETERMINER*3",0,0,1,
	//  1,u"_ADJECTIVE_AFTER*2",0,0,2, // this cannot contain VERBPAST, as it is not possible yet to ascertain the difference between this and _S1.
	//                                // otherwise, _NOUN would win over _S1 because it has fewer subcomponents, which was incorrect.
	//  4,u"noun*4{SUBJECT:N_AGREE}",u"_NAME*6{SUBJECT:GNOUN}",u"personal_pronoun_nominative*3{SUBJECT:N_AGREE}",u"personal_pronoun*3{SUBJECT:N_AGREE}",NO_OWNER,1,1, // this is tightly controlled -
	//  1,u"__INTERPP[*]{_BLOCK}",0,0,1,
			 // no pronouns that could be adjectives -
			 // previously incorrect - 'round and lowered his' with 'round and lowered' being an adjective and 'his' being a noun!
			 // then the next noun would be considered a secondary noun, which is incorrect. 'looked round and lowered his voice'
	//  2,u"__ALLVERB",u"_VERBPASSIVE",0,1,1,     // the news I brought
	//    1,u"__ALLOBJECTS",0,0,1,
	//  0);
	// REPLACED BY __NOUN "5" which was then replaced by NOUN[R]
	//cPattern::create(u"__NOUN{_FINAL_IF_ALONE}",u"",
	//                  1,u"__PNOUN",0,1,1,
	//                  1,u"__APPNOUN2{_BLOCK:RE_OBJECT}",0,1,1,
	//                  0);

	// encourage prepositional phrases to not have objects for the main noun after the prep phrase
	// by making the PP in ALLOBJECTS[1] very expensive.
	// I will give from the store Mark the book.
	cPattern::create(u"__ALLOBJECTS_0", u"1",
		// included adjective because we still want to have the possibility of multiple
		// adjectives and other possibilities included.  However, because a verb is included in
		// _ADJECTIVE, we must make this choice expensive - but not more than 1 (6/2/2007)
		// _PP already has a preposition and an adverb as its first elements (usually)
		5, u"_ADVERB{ADVOBJECT}", u"_PP", u"preposition*3", u"_ADJECTIVE*1{ADJOBJECT}", u"adjective{ADJOBJECT}", 0, 1, 2, // discourage hanging preposition! must be 2 because otherwise combinations are blocked!
		0);
	// a copy of __ADJECTIVE, without coordinators or commas and without a verb (because this would be directly after
	// another verb which is really not allowed
	cPattern::create(u"__ADJECTIVE_WITHOUT_VERB", u"",
		1, u"_ADVERB", 0, 0, 1,
		7, u"adjective|own*10", u"adjective", u"numeral_ordinal", u"_NUMBER", u"preposition|ex", u"noun*1", u"_NAMEOWNER*2", VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		1, u"adverb*1", 0, 0, 1,
		0);
	cPattern::create(u"__ALLOBJECTS_0", u"2", 1, u"__ADJECTIVE_WITHOUT_VERB{ADJ:ADJOBJECT}", 0, 1, 1, 0);
	// this is if a sentence has a prepositional phrase followed by an adverb and another prepositional phrase.  
	//   this pattern is necessary because a sentence can end like this and if the prep phrase is short enough, and the prep is also a separator, the 
	//   prep phrase is cut off from the rest of the sentence but because the word is not a separator after parsing is concluded it causes the __S1 pattern
	//   to be removed because it no longer follows the _FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN requirement
	cPattern::create(u"__ALLOBJECTS_0", u"3",
		// included adjective because we still want to have the possibility of multiple
		// adjectives and other possibilities included.  However, because a verb is included in
		// _ADJECTIVE, we must make this choice expensive - but not more than 1 (6/2/2007)
		// _PP already has a preposition and an adverb as its first elements (usually)
		1, u"_PP", 0, 1, 1, // hanging preposition! must be 2 because otherwise combinations are blocked!
		1, u"_ADVERB{ADVOBJECT}", 0, 1, 1,
		1, u"_PP", 0, 1, 1,
		0);
	// NEGATION
// CGEL 10.54-10.70
// encourage verbs to group their nouns (without using NOUN[5], which is set considerably higher in cost)
// (_NOUN[9] includes _INFP as a post-modifier)
// I want you to take Mrs. Smith back.
// I want Bill to remember to thank Mrs. Smith for taking us back today.
// I use language to suit the occasion.
	cPattern::create(u"__ALLOBJECTS_1", u"1",
		2, u"_ADVERB", u"_PP*4", 0, 0, 1, // removed u"preposition*2", because there is no difference at this level of processing between
															 // u"hang up" the phone and u"hang" up the phone, so resolve to a prepositional phrase and
															 // then reprocess later to analyze which prepositions actually belong with their verbs.
															 // __PP already has an adverb as the first element.
		1, u"_ADVERB", 0, 0, 1, // I seemed for him *almost* a recluse.
		6, u"_NOUN_OBJ{OBJECT}", u"__NOUN[*]{OBJECT}", u"__MNOUN[*]{OBJECT}", u"__NOUNREL*3{OBJECT}", u"__NOUNRU{OBJECT}", u"__VERBALPHRASENOUN[*]{OBJECT}", 0, 1, 1,
		0);
	// I want to remember to thank Mrs. Smith for taking us back today.
	// the kind of man who would be afraid to meet death!
	cPattern::create(u"__ALLOBJECTS_1", u"2",
		4, u"_ADVERB{ADVOBJECT}", u"_PP*1", u"preposition*2", u"_ADJECTIVE*1{ADJOBJECT}", 0, 0, 2, // hanging preposition!
		1, u"__INTERPPB[*]{_BLOCK}", 0, 0, 1,
		//2,u"_NOUN_OBJ{OBJECT}",u"__NOUN[*]{OBJECT}",0,0,1, // Bill
		2, u"_INFP", u"_REL1[*]", 0, 1, 1, // to remember (with its object which is [to thank Mrs. Smith for taking us back today])
		0);
	cPattern::create(u"__ALLOBJECTS_1", u"3",
		1, u"preposition", 0, 1, 1,
		3, u"personal_pronoun_accusative|her*6", u"personal_pronoun_accusative", u"personal_pronoun|you", 0, 1, 1, // her resulted in many false matches
		2, u"_NOUN_OBJ{OBJECT}", u"__NOUN[*]{OBJECT}", 0, 1, 1,
		0);
	// by creating a second pattern for two objects, we push one object or two objects up another notch to make it more costable
	// by secondaryCosts
	cPattern::create(u"__ALLOBJECTS_2", u"",
		2, u"_ADVERB", u"_PP*4", 0, 0, 1, // removed u"preposition*2", because there is no difference at this level of processing between
															 // u"hang up" the phone and u"hang" up the phone, so resolve to a prepositional phrase and
															 // then reprocess later to analyze which prepositions actually belong with their verbs.
															 // __PP already has an adverb as the first element.
		4, u"_NOUN_OBJ{OBJECT}", u"__NOUN[*]{OBJECT}", u"__MNOUN[*]{OBJECT}", u"__VERBALPHRASENOUN[*]{OBJECT}", 0, 1, 1,
		2, u"_ADVERB*1", u"preposition*2", 0, 0, 1, // hanging preposition!
		4, u"_NOUN_OBJ{OBJECT}", u"__NOUN[*]{OBJECT}", u"__MNOUN[*]{OBJECT}", u"__NOUNREL{OBJECT}", 0, 1, 1, // missing "__VERBALPHRASENOUN"?
		0);
	// It left her flushed and silent.
	cPattern::create(u"__ALLOBJECTS_2", u"2",
		1, u"personal_pronoun_accusative*1{OBJECT}", 0, 1, 1,
		1, u"adverb", 0, 0, 2,
		3, u"adjective|own*10{ADJ}", u"adjective{ADJ}", u"verb*1{ADJ}", VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE, 1, 2,
		0);
	// Thank you , ma'am
	cPattern::create(u"_NAME_INJECT", u"1",
		1, u",", 0, 1, 1,
		2, u"_NAME{HAIL}", u"_META_GROUP{HAIL}", 0, 1, 1, 0);
	cPattern::create(u"_PRE_VR1{_ONLY_BEGIN_MATCH}", u"",
		3, u"_ADVERB", u"_PP", u"_NAME", 0, 0, 1,
		1, u",", 0, 1, 1,  // _ADVERB is a redundant prefix to __ALLVERB - comma must be non-optional
		0);
	cPattern::create(u"_VERBPRESENT_CO", u"",
		1, u"_VERBPRESENT", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		1, u"_VERBPRESENT{VERB}", 0, 1, 1,
		0);
	// trained to lock and unlock
	// were pushing on to the ... / be easy!
	// shall press you ***
	// had not completed my letter that night.
	// do you? / have you? / are you? / did you / didn't you?
	cPattern::create(u"_VERBREL1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_BLOCK:EVAL}", u"1",
		2, u"_PRE_VR1", u"politeness_discourse_marker*-1", 0, 0, 1, // _PP removed - In a moment Tommy was inside. - Is moment an adjective OR noun?
		2, u"__ALLVERB", u"_VERBPRESENT_CO", 0, 1, 1,  //            _PP               VERBREL
		// removed u"not{not}",u"never{never}", 7/14/2006 - already included in ADVERB at the end of each subcomponent of __ALLVERB
		// removed REL1 and _PP - included with __ALLOBJECTS
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 1, 1, // u"there{OBJECT}" removed
											// ADVERB made more expensive because ALLOBJECTS already has ADVERB 2/17/2007
		2, u"_NAME_INJECT", u"adverb", 0, 0, 1, // Thank you , ma'am                 // removed _PP from above because __ALLOBJECTS already has _PP
		// ” agreed her *chum* enthusiastically . 
		0);
	cPattern::create(u"_VERBREL1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_BLOCK:EVAL}", u"4",
		2, u"__ALLVERB", u"_VERBPRESENT_CO", 0, 1, 1,  // this is used by  
		0);
	cPattern::create(u"_VERBREL1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_BLOCK:EVAL}", u"2",
		2, u"_PRE_VR1", u"politeness_discourse_marker", 0, 0, 1,
		1, u"__ALLTHINK*1{VERB}", 0, 1, 1, // u"think" for all active tenses! - possibly add INTRO_S1.
		2, u"__NOUN[*]{OBJECT}", u"_NOUN_OBJ{OBJECT}", 0, 0, 1, // missing "__VERBALPHRASENOUN"?
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		1, u"_NAME_INJECT", 0, 0, 1,
		0);
	// only after quotes
	cPattern::create(u"_VERBREL1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_BLOCK:EVAL:_AFTER_QUOTE}", u"3",
		1, u"_VERBPAST", 0, 1, 1,
		2, u"__NOUN[*]{SUBJECT}", u"__NOUNREL{SUBJECT}", 0, 1, 1, // missing "__VERBALPHRASENOUN"?
		0);
	// another voice which *Tommy rather thought* was that of Boris replied:
	cPattern::create(u"_SUBREL2{_BLOCK}", u"",
		1, u"__C1__S1", 0, 1, 1,
		1, u"__ALLTHINK", 0, 1, 1,
		0);
	// _REL1 "2", "3" and "4" use __S1 and are defined later...
	// 'what had been written' was plain to me
	// which underlay his tone / that had been written to him
	// this is a copy of __S1[4] with relativizer as SUBJECT
	cPattern::create(u"_REL1{_BLOCK:REL}", u"1",
		1, u"_ADJECTIVE", 0, 0, 1,
		2, u"relativizer{SUBJECT}", u"demonstrative_determiner|that{SUBJECT}", 0, 1, 1,
		3, u"_PP", u"_REL1[*]", u"_SUBREL2", 0, 0, 1, // 
		1, u"_ADJECTIVE", 0, 0, 1,
		2, u"__SUB_S2", u"_VERBPASSIVE_P", 0, 1, 1,
		0);
	// this is a copy of __S1[1] with relativizer as SUBJECT
	// GNOUN- What had happened wants.  REL1[5] matches the subject of the sentence, and agreement test requires usage of GNOUN.
	cPattern::create(u"_REL1{_BLOCK:REL:GNOUN}", u"5",
		1, u"_ADJECTIVE", 0, 0, 1,
		2, u"relativizer{SUBJECT}", u"demonstrative_determiner|that{SUBJECT}", 0, 1, 1,
		3, u"_PP", u"_REL1[*]", u"_SUBREL2", 0, 0, 1,
		3, u"__ALLVERB", u"_COND{VERB}", u"_VERBPASSIVE", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
		0);
	createBareInfinitives();
	createThinkBareInfinitives();


	// having sent
	// having given Bill the book
	cPattern::create(u"__VERBREL2{VERB}", u"1",
		1, u"_ADVERB", 0, 0, 1,
		1, u"have", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"_PP", 0, 0, 1, // having in his turn failed to persuade Tuppence...
		1, u"_ADVERB", 0, 0, 2,
		2, u"_VERBPASTPART{vrB}", u"_BEEN{vrB:V_OBJECT}", 0, 1, 1,
		0);
	// , having been giving Bob the book
	cPattern::create(u"__VERBREL2{VERB}", u"2",
		1, u"_ADVERB", 0, 0, 1,
		1, u"have", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"_PP", 0, 0, 1, // having in his turn failed to persuade Tuppence...
		1, u"_ADVERB", 0, 0, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 2,
		3, u"verb{vrBC:V_OBJECT}", u"does{vrBC:V_OBJECT}", u"have{vrBC:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		0);
	// , spreading over its Gothic houses, (VERBONGOING)
	// , spread over its
	// , being over the highest peak
	// , having the highest salary
	// giving bill the book
	cPattern::create(u"_VERBREL2{_FINAL_IF_ALONE}", u"1",
		2, u"_VERBONGOING{vE:VERB}", u"__VERBREL2", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1,
		0);
	// ‘ I am exactly the same , ’ Catherine repeated , *wishing her aunt were a little less sympathetic* .
	cPattern::create(u"_VERBREL2{_FINAL_IF_ALONE}", u"3",
		1, u"_THINKONGOING{vE:VERB}", 0, 1, 1,
		1, u"__S1*1{EVAL:_BLOCK:SUBJUNCTIVE}", 0, 1, 1,
		0);
	// 8. becoming...
	//cPattern::create(u"_VERBREL2{_FINAL_IF_ALONE}", u"4",
	//								1, u"verb|becoming{vE:VERB}", 0, 1, 1,
	//								1, u"__ADJECTIVE", 0, 0, 1, 0);
	// PASSIVE
	// , having been spread
	// , having been given
	// , having not been spread
	cPattern::create(u"__PVERBREL2{VERB}", u"1",
		1, u"_ADVERB", 0, 0, 1,
		1, u"have", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_BEEN", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 2,
		1, u"_VERBPASTPART{vrBD}", 0, 1, 1, 0);
	// PASSIVE
	// , being informed of the risks
	// , being given the book by Bill
	cPattern::create(u"__PVERBREL2{VERB}", u"2",
		1, u"_ADVERB", 0, 0, 1,
		1, u"being", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"verb{vrD:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1, 0);

	cPattern::create(u"_VERBREL2{_FINAL_IF_ALONE}", u"2",
		1, u"__PVERBREL2", 0, 1, 1,
		2, u"_PP", u"_REL1[*]", 0, 0, 1,
		// verbs in passive clauses only have one object maximum
		5, u"__NOUN[*]{OBJECT}", u"__MNOUN[*]{OBJECT}", u"_NOUN_OBJ{OBJECT}", u"__NOUNREL{OBJECT}", u"_ADJECTIVE", 0, 0, 1,// missing "__VERBALPHRASENOUN"?
		1, u"_ADVERB", 0, 0, 1,
		0);
}

// __PP / _PP family: preposition + NP / _REL / _VERBREL, including the
// "of NP SVO" shape (__PP[R]) that sets _EXPLICIT_SUBJECT_VERB_AGREEMENT.
// P tags the preposition; PREPOBJECT / PREP tag the phrase for role costing.
void createPrepositionalPhrases(void)
{
	LFS
		//cPattern *p=NULL;
		/*
		REMOVED - for is not used as a preposition in this pattern!
		// VERBREL2: particularly for (him, your, Bill's, Bill, the Catcher) being from NYC
		// VERBREL3: particularly for (him, your, Bill's, Bill, the Catcher) having been sent / between having been sent the book
		// VERBREL4: particularly for (him, your, Bill's, Bill, the Catcher) having sent / between having been the catcher
		// VERBREL5: particularly for (him, your, Bill's, Bill, the Catcher) being informed of the risks
		cPattern::create(u"__PP",u"2",1,u"_ADVERB",0,0,1,
										 1,u"preposition",0,1,1,
										 1,u"_ADVERB",0,0,1,
										 4,u"possessive_determiner",//u"demonstrative_determinerL"(included as a _NOUN),u"quantifier", (included as _N1)
											 u"interrogative_determiner",u"_NOUN_OBJ",u"__NOUN",0,0,1,
										 2,u"_VERBREL2",u"_VERBONGOING",0,1,1,
										 0);
		*/
		// of Mrs. Vandermeyer's.
		cPattern::create(u"__PP", u"2",
			1, u"preposition|of{P}", 0, 1, 1,
			1, u"__NAMEOWNER{PREPOBJECT}", 0, 1, 1, 0);
	// He loved himself (himself should be a direct object, not a PP)
	cPattern::create(u"__PP", u"3", 1, u"reflexive_pronoun*1", 0, 1, 1, 0); // prefer reflexive pronoun be a direct object than a preposition.
	// probability analysis - prepositions were being changed to adverbs because there/again are not nouns in LP
	cPattern::create(u"__PP", u"THAG",
		1, u"preposition{P}", 0, 1, 1,
		4, u"adverb|there{PREPOBJECT}", u"adverb|again{PREPOBJECT}", u"adverb|anywhere{PREPOBJECT}", u"adverb|everywhere{PREPOBJECT}", 0, 1, 1,
		0);
	// in which ... / for which...
	// by which you intend to leave the town
	cPattern::create(u"__PP", u"4",
		2, u"preposition{P}", u"verbalPreposition{P}", 0, 1, 1,
		1, u"interrogative_determiner", 0, 1, 1,
		1, u"__S1{PREPOBJECT:OBJECT:EVAL:_BLOCK}", 0, 1, 1, 0);
	// for he will be strong. INVALID for is not a preposition in this sentence
	//cPattern::create(u"__PP",u"5",1,u"preposition",0,1,1,
	//                1,u"__S1",0,1,1,0);
	// at old Red's
	cPattern::create(u"__PP", u"6", 1, u"_ADVERB*1", 0, 0, 1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
		3, u"preposition{P}", u"verbalPreposition{P}", u"__AS_AS", 0, 1, 1,
		1, u"preposition*1{P}", 0, 0, 1, // from within
		1, u"determiner", 0, 0, 1, // the
		1, u"Proper Noun{PREPOBJECT}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1,
		0);
	// copy of __PP 9
//cPattern::create(u"__PP",u"7",1,u"_ADVERB",0,0,1,
//                 2,u"preposition{P}",u"verbalPreposition{P}",0,1,1,
//                 1,u"preposition*1{P}",0,0,1, // from within
//                 1,u"_ADVERB*3",0,0,1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
//                 1,u"_VERBREL2{_BLOCK:EVAL}",0,1,1,  // we can keep the fact -of having done so quite secret .
//                 0);
// there was never much chance of -their ‘ letting me go ’
	cPattern::create(u"__PP", u"8",
		1, u"_ADVERB*1", 0, 0, 1,  // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
		3, u"preposition{P}", u"verbalPreposition{P}", u"__AS_AS", 0, 1, 1,
		1, u"preposition*1{P}", 0, 0, 1, // from within
		1, u"_ADVERB*3", 0, 0, 1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
		5, u"_NOUN_OBJ{PREPOBJECT}", u"__NOUN[*]{PREPOBJECT}", u"__MNOUN[*]{PREPOBJECT}", // _NOUN* includes NOUN[D] and NOUN[E]
		u"demonstrative_determiner", u"possessive_determiner", 0, 0, 1, // there was never much chance of -their ‘ letting me go ’
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		1, u"_VERBREL2{_BLOCK:EVAL}", 0, 1, 1,  //  the fact -of -them 'having done so'
		1, u"quotes", CLOSE_INFLECTION, 1, 1,
		0);
	cPattern::create(u"__PP", u"9",
		// Why , I am not only older than you in years , I am older in soul , *older in a thousand lives* . (adjective required to match 'older' below)
		2, u"_ADVERB*1", u"adjective*1", 0, 0, 1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
		3, u"preposition{P}", u"verbalPreposition{P}", u"__AS_AS", 0, 1, 1, // as much as Old Mr. Crow / As busy as chirpy cricket
		1, u"preposition*1{P}", 0, 0, 1, // from within
		1, u"_ADVERB*3", 0, 0, 1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
		5, u"_NOUN_OBJ{PREPOBJECT}", u"__NOUN[*]{PREPOBJECT}", u"__MNOUN[*]{PREPOBJECT}", // _NOUN* includes NOUN[D] and NOUN[E]
		u"demonstrative_determiner{PREPOBJECT}", u"possessive_determiner{PREPOBJECT}", 0, 0, 1, // there was never much chance of -their ‘ letting me go ’
		1, u"_VERBREL2*2{_BLOCK:EVAL}", 0, 1, 1,  // we can keep the fact -of having done so quite secret . / the fact -of -them having done so
		0);
	cPattern::create(u"_PP{_FINAL:_NO_REPEAT}", u"1", 1, u"__PP[*]{PREP:_BLOCK}", 0, 1, 5, 0);
	// NOTE reflexive_pronouns, possessive_pronouns and reciprocal_pronouns can also be used as _NOUN_OBJ
	// This pattern should not be repeated because _NOUN and _NOUN_OBJ already have _PP as a follow.
	// in the sentence below, the _PP containing moment or two Tommy's indignation should not be included
	// After a moment or two Tommy's indignation got the better of him.
	cPattern::create(u"_PP{_FINAL_IF_ALONE:_BLOCK:PREP:_NO_REPEAT}", u"2",
		// Why , I am not only older than you in years , I am older in soul , *older in a thousand lives* . (adjective required to match 'older' below)

		3, u"_ADVERB*1", u"adjective*1", // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
		u"relativizer|when", 0, 0, 1, // he was home but not to lose a watch below when at sea and not a moment's time in going ashore after work hours *when* in harbour . 
		3, u"preposition{P}", u"verbalPreposition{P}", u"__AS_AS", 0, 1, 1,  // this should be a particle
		1, u"preposition{P}", 0, 0, 1, // from within - complex prepositions
		1, u"_ADVERB*3", 0, 0, 1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
		// Under the original act, how many judges were to be on the court? - __NOUNREL should have at least a cost of 2 because it matches too much (with its comma)
		9, u"personal_pronoun_nominative|i*6", u"personal_pronoun_nominative|we*6", u"_NOUN_OBJ{PREPOBJECT}", u"__NOUN[*]{PREPOBJECT}", u"__MNOUN[*]{PREPOBJECT}", u"__NOUNREL*4{PREPOBJECT}", u"_ADJECTIVE[*]*4", u"__NOUNRU{PREPOBJECT}", u"__VERBALPHRASENOUN[*]{PREPOBJECT}", 0, 1, 1,
		0);
	// The pursuit of managers and of players had *left* her continually alone .
	cPattern::create(u"_PP{_FINAL_IF_ALONE:_BLOCK:PREP:_NO_REPEAT}", u"2C",
		2, u"_ADVERB*1", u"adjective*1", 0, 0, 1,
		1, u"preposition|of{P}", 0, 1, 1,
		2, u"__NOUN[*]{PREPOBJECT}", u"__VERBALPHRASENOUN[*]{PREPOBJECT}", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		1, u"preposition|of{P}", 0, 1, 1,
		2, u"__NOUN[*]{PREPOBJECT}", u"__VERBALPHRASENOUN[*]{PREPOBJECT}", 0, 1, 1,
		0);
	// We are nearer to finding Tuppence.
	//  This is not an infinitive phrase.  'to' is a participle, and finding is an verb with a subject Tuppence
	// it is not a prepositional phrase either, but it is a convenient place to put this.
	cPattern::create(u"_PP{_FINAL:_BLOCK:_NO_REPEAT}", u"3",
		1, u"preposition|to", 0, 1, 1,
		1, u"_VERBONGOING{VERB:vE}", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 1, 1, // there must only be one adjective and it must be last (not mixed in) see *
		0);
	// in case Albert should miss it. (special case?)
	cPattern::create(u"_PP{_FINAL:_BLOCK:_NO_REPEAT}", u"4",
		1, u"preposition|in", 0, 1, 1,
		1, u"noun|case", 0, 1, 1,
		1, u"__S1{PREPOBJECT:OBJECT:EVAL:_BLOCK}", 0, 1, 1, 0);
	// from there / to there (because there has no noun form)
	cPattern::create(u"__PP", u"C",
		2, u"preposition|from", u"preposition|to", 0, 1, 1,
		1, u"adverb|there", 0, 1, 1, 0);
	// with as brave a heart as possible
	cPattern::create(u"__PP{_FINAL_IF_ALONE:_BLOCK:PREP}", u"D",
		1, u"preposition{P}", 0, 1, 1,
		1, u"adverb|as*-2", 0, 1, 1,
		2, u"adverb", u"adjective", 0, 1, 1,
		1, u"__NOUN[*]", 0, 1, 1, // as brave hearts as / as silent a type as / as stern a voice as 
		1, u"preposition|as{P}", 0, 1, 1,  // when included in a _PP, mark this as a preposition
		2, u"adjective", u"__S1", 0, 1, 1,
		0);
	cPattern::create(u"_PP{_FINAL:_BLOCK:_NO_REPEAT}", u"T",
		3, u"preposition|on", u"preposition|at", u"preposition|from", 0, 1, 1,  // from the time your office closes / on the day your office closes
		1, u"determiner|the{TIMEMODIFIER}", 0, 1, 1,
		2, u"timeUnit*1{TIMECAPACITY}", u"noun|time", 0, 1, 1,
		1, u"__S1*1{PREPOBJECT:OBJECT:EVAL:_BLOCK}", 0, 1, 1, 0);
	//~~~Ellen Anderson Gholson Glasgow\The Wheel of Life[44590 - 44593]: FAIL
	//“ I merely wanted to let her know the kind of **man he is**, ” explained Perry .
	//~~~Ellen Anderson Gholson Glasgow\The Wheel of Life[27951 - 27955]: FAIL
	//But I am perfectly sure that it ishas not the kind of **thing you wouldhad like** .
	//~~~Ellen Anderson Gholson Glasgow\The Wheel of Life[33229 - 33233]: FAIL
	//Then suddenly her brow contracted with resolution, and she went through a long list of items as if the most important fact in life were the amount of **money she must pay** to her dressmaker . |||
	//~~~Ellen Anderson Gholson Glasgow\The Wheel of Life[52039 - 52043]: FAIL
	//The simple spirit of **contemplation we have come** to regard as a pauperising habit and it puts us out of patience .
	//~~~Ellen Anderson Gholson Glasgow\The Wheel of Life[68011 - 68014]: FAIL
	//He spoke with an entire unconsciousness of the amount of **work he asked** of her, and she liked him the better for the readiness with which he took for granted that she possessed the patience as well as the will to serve him . |||
	cPattern::create(u"__PP{_BLOCK:_EXPLICIT_SUBJECT_VERB_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS:PREP}", u"R",
		1, u"preposition|of{P}", 0, 1, 1,
		3, u"adjective*1", u"noun{N_AGREE}", u"Proper Noun", 0, 1, 1,  // adjective may be loosed from ProperNoun improperly - prevent match in 'The little Pilgrim was startled by this tone.'
		3, u"Proper Noun*3{ANY:NAME:SUBJECT:PREFER_S1}", u"personal_pronoun_nominative*2{SUBJECT:PREFER_S1}", u"personal_pronoun*3{SUBJECT:PREFER_S1}", NO_OWNER, 1, 1, // highly restrict and discourage to prevent unnecessary matches
		3, u"__ALLVERB", u"_COND{VERB}", u"_VERBPASSIVE", 0, 1, 1,
		0);

}

// Interruptions that sit between subject and verb of __C1__S1: comma-wrapped
// _VERBREL2, past-participial, restatement, "as NP", _PP, or _ADJECTIVE.
// Tagged EVAL/_BLOCK so they cost as part of the enclosing S but do not
// themselves collect descendant tags.  Returns 0.
int createSecondaryPatterns__C1_IP(void)
{
	LFS

		// C1_IP keeps the subject/verb together by chopping out the comma and verbal phrase
	// knowing full well the consequences
	// known to her intimate friends for some mysterious reason as u"Tuppence.u"
	// given her book by some of the greatest artists known.
	// My son, known throughout the lands as a great conquerer
		cPattern::create(u"__C1_IP", u"1",
			1, u",", 0, 1, 1,
			1, u"_VERBREL2{_BLOCK:EVAL}", 0, 1, 1,
			1, u",", 0, 0, 1,
			0);
	// the last comma should not be made optional, this causes the collowing sentence to be misparsed at *ebbing*
	// Gerty , as she mentioned the names of her callers , subsided with her *ebbing* green waves into the chair from which she had risen.
	cPattern::create(u"__C1_IP{_BLOCK:EVAL}", u"2",
		1, u",", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1, // _VERBPASTPART does not have an adverb
		2, u"_VERBPASTPART", u"being{vC:id:V_OBJECT}", VERB_PAST_PARTICIPLE, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 1, 1, // added _PP 8/28/2006
		1, u",", 0, 1, 1,
		0);
	// restatement / this woman, whoever she was, was saved
	// this woman, master of disguise,
	cPattern::create(u"__C1_IP{_BLOCK:EVAL}", u"3",
		1, u",", 0, 1, 1,
		4, u"__NOUN[*]{_BLOCK:RE_OBJECT:NAME_SECONDARY}", u"__MNOUN[*]{_BLOCK:RE_OBJECT:NAME_SECONDARY}", u"_REL1[*]", u"__VERBALPHRASENOUN[*]{_BLOCK:RE_OBJECT}", 0, 1, 1,
		1, u",", 0, 1, 1, // period deleted because of the '.' at end - swallows up the period
		0);
	// This woman, as an American, would always prevail.
	cPattern::create(u"__C1_IP{_BLOCK:EVAL}", u"4",
		1, u",", 0, 1, 1,
		1, u"preposition|as", 0, 1, 1,
		1, u"__NOUN[*]{_BLOCK:RE_OBJECT}", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	// A slatternly woman, with vicious eyes, opened the door.
	cPattern::create(u"__C1_IP{_BLOCK:EVAL}", u"5",
		1, u",", 0, 1, 1,
		1, u"_PP{_BLOCK}", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	// Albert, still round-eyed, demanded breathlessly:
	cPattern::create(u"__C1_IP{_BLOCK:EVAL}", u"6",
		1, u",", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	return 0;
}

// Subject slot of a simple sentence (__C1__S1): optional __INTRO_NP then one
// of __NOUN / __MNOUN / _INFP / there / _REL1 / _VERBREL2 / __QSUBJECT / ...
// SUBJECT / N_AGREE / GNOUN tags drive agreement and object identification.
// Returns 0.
int createSecondaryPatterns__C1__S1(void)
{
	LFS
		// INFP should be rare// REL1 should be rare// VERBREL2 should be rare
	// _ADJECTIVE should also cover NAMEOWNER
	// there added to prevent "but of Jane Finn there was no mention" - 'there' should be a SUBJECT
	// overwhelmingly 'there' may be parsed as an adverb (which it is), but then not matched to C1__S1 and then
	// parsed as a VERB clause, which then is more expensive.
	cPattern::create(u"__C1__S1", u"1",
		1, u"__INTRO_NP", 0, 0, 1,
		17, u"__NOUN[*]{SUBJECT}", u"__MNOUN[*]{SUBJECT}", u"_INFP*2{GNOUN:SINGULAR:SUBJECT}", u"interrogative_pronoun{N_AGREE:SINGULAR:SUBJECT}",
		u"interrogative_determiner{N_AGREE:SINGULAR:SUBJECT}",
		u"there*-1{N_AGREE:SINGULAR:PLURAL:SUBJECT}", u"adverb|here*-1{N_AGREE:SINGULAR:PLURAL:SUBJECT}",
		u"quantifier|neither{N_AGREE:PLURAL:SUBJECT}", u"quantifier|either*-1{N_AGREE:SINGULAR:SUBJECT}",
		u"_REL1[*]*2{SUBJECT:GNOUN:SINGULAR}",
		u"adjective*2{SUBJECT:GNOUN}", // **few** are the mortals to whom we give this lovely gift / **such** was not the case / **above** played the clear , soft light / **hers** is organic
		u"_ADJECTIVE*10{SUBJECT:GNOUN}",
		u"_VERBREL2*2{SUBJECT:GNOUN:SINGULAR:_BLOCK:EVAL}", u"__QSUBJECT{SUBJECT:GNOUN:SINGULAR}", u"__NOUNRU{SUBJECT}",
		u"noun{SUBJECT:N_AGREE}", u"__VERBALPHRASENOUN[*]{SUBJECT}", SINGULAR_OWNER | PLURAL_OWNER, 1, 1, // Poirot's were pleasantly vague .
		1, u"__INTERPPB", 0, 0, 1,
		1, u"__C1_IP", 0, 0, 1, // her father, Neptune, lives in a beautiful castle
		0);
	// with him was the evil-looking Number 14.
	// Beside him stood Annette.
	// Through Tommy's mind[mr] flashed the assurance:
	// on one side was ...
	cPattern::create(u"__C1__S1", u"2",
		5, u"preposition|with*1", u"preposition|beside*1", u"preposition|through", u"preposition|on*2", u"preposition|from*2", 0, 1, 1, // rare pattern 
		8, u"adjective{ADJ}", u"verb*1{ADJ}", u"numeral_ordinal{ADJ}", u"_NUMBER{ADJ}", u"preposition|ex{ADJ}", u"noun{ADJ}", u"no{ADJ:no}", u"__NAMEOWNER", // removed *1 as cost so that
		VERB_PRESENT_PARTICIPLE | VERB_PAST_PARTICIPLE | SINGULAR_OWNER | PLURAL_OWNER, 0, 1,                            // we can make cost of *1 for __NOUN[2]
		5, u"__N1{SUBJECT:NOUN}", u"__NOUN[*]*1{SUBJECT:NOUN}", u"_NAME{GNOUN:NAME:SUBJECT}", u"_NOUN_OBJ{SUBJECT:NOUN}", u"personal_pronoun_accusative{SUBJECT:NOUN}", 0, 1, 1,
		1, u"__C1_IP", 0, 0, 1,
		0);
	// my dear / my lord
	cPattern::create(u"_HAIL_OBJECT{_ONLY_END_MATCH}", u"1",
		1, u"possessive_determiner|my", 0, 1, 1, // my dear
		3, u"honorific{HON:HAIL}", u"noun{HAIL}", u"adjective|dear", MALE_GENDER | FEMALE_GENDER, 1, 1,
		0);
	// you fortunate dear / you poor boy
	cPattern::create(u"_HAIL_OBJECT{_ONLY_END_MATCH:_FINAL_IF_ALONE}", u"2",
		1, u"personal_pronoun|you", 0, 1, 1,
		1, u"adjective", 0, 0, 1,
		5, u"honorific{HON:HAIL}", u"noun{HAIL}", u"adjective|dear", u"noun|thing{HAIL}", u"adverb|too{HAIL}", MALE_GENDER | FEMALE_GENDER, 1, 1,
		0);
	return 0;
}

// Optional sentence-final tails: vocative ", sir", leftover infinitive
// fragment, dangling adverb/adjective/time (FLOATTIME), "not to".
// _ONLY_END_MATCH so they cannot win in the middle of a clause.  Returns 0.
int createSecondaryPatterns__CLOSING__S1(void)
{
	cPattern::create(u"__CLOSING__S1{_ONLY_END_MATCH}", u"1",
		1, u",", 0, 1, 1, // , ma'am // if this is made optional, _NOUN of C4 and _ALLOBJECT of C3 are identical
		7, u"_NAME{HAIL}", u"honorific{HON:HAIL}", u"_HON_ABB{HON:HAIL}", u"_META_GROUP{HAIL}", u"noun{HAIL}", u"_HAIL_OBJECT{HAIL:OBJECT}", u"politeness_discourse_marker", MALE_GENDER | FEMALE_GENDER, 1, 1, // , sir / , freak! noun includes _NAME, u"honorific",
		0);
	cPattern::create(u"__CLOSING__S1{_ONLY_END_MATCH}", u"2",
		1, u",", 0, 0, 1,
		2, u"__INFPSUB*1", u"__INFP3SUB*1", 0, 1, 1, // prefer __S1 to __INFP?SUB - verbs in INFP?SUB must agree with _INFP as object of verb!
		0);
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH:FLOATTIME}", u"3",
		6, u"_ADVERB", u"__ADJECTIVE_WITHOUT_VERB*1", u"_ADJECTIVE*4", u"preposition*2", u"_TIME", u"_DATE", 0, 1, 1, // dangling adverb or adjective // dangling adverb or adjective should be taken only if there is no other match
		0);
	// *I* shall help him not to .
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH:FLOATTIME}", u"TO",
		1, u"_ADVERB", 0, 0, 1,
		1, u"to", 0, 1, 1,
		0);
	// He likes me best *I know*.
	cPattern::create(u"__CLOSING__S1{_ONLY_END_MATCH}", u"4",
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		1, u"__ALLTHINK", 0, 1, 1,
		0);
	// already taken by _TIME
	//cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}",u"4",
	//                 2,u"demonstrative_determiner{DET}",u"quantifier{DET}",0,0,1,
	//                 1,u"timeUnit",0,1,1,
	//                 0);
		// said Tuppence, her spirits rising.
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}", u"5",
		1, u",", 0, 1, 1,
		1, u"__NOUN[*]{SUBJECT}", 0, 1, 1,
		1, u"_VERBONGOING{vE:VERB}", 0, 1, 1,
		0);
	// Boris remarked, glancing at the clock.
	// I ran to the bookstore, yelling all the way.
	// He wandered to close to the woods, wandering into the pit.
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}", u"6",
		1, u",", 0, 1, 1,
		1, u"_VERBREL2{_BLOCK:EVAL}", 0, 1, 1,
		0);
	// I would like to meet him -- Mr. Brown
	cPattern::create(u"__CLOSING__S1{_ONLY_END_MATCH}", u"7",
		1, u"dash", 0, 1, 1, // , ma'am // if this is made optional, _NOUN of C4 and _ALLOBJECT of C3 are identical
		3, u"_NAME", u"honorific{HON}", u"_HON_ABB{HON}", MALE_GENDER | FEMALE_GENDER, 1, 1, // , sir / , freak! noun includes _NAME, u"honorific",
		0);
	// rather than believe me guilty
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}", u"8",
		1, u"adverb|rather", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		1, u"__ALLTHINK*1{VERB}", 0, 1, 1,
		2, u"__NOUN[*]{OBJECT}", u"_NOUN_OBJ{OBJECT}", 0, 1, 1,
		1, u"adjective", 0, 0, 1, // guilty
		0);
	// as I please / as he pleases / as he pleased
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}", u"9",
		2, u"conjunction|as", u"interrogative_pronoun|wherever", 0, 1, 1,
		2, u"personal_pronoun", u"personal_pronoun_nominative{SUBJECT}", 0, 1, 1,
		3, u"verb|please*-2{VERB:V_AGREE:V_OBJECT}", u"verb|pleases*-2{VERB:V_AGREE:V_OBJECT}", u"verb|pleased*-2{VERB:V_AGREE:V_OBJECT}", 0, 1, 1, // incentivize against pi (politeness_discourse_marker)
		0);

	// but with extreme graciousness
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}", u"A",
		1, u",", 0, 0, 1,
		2, u"conjunction", u"coordinator", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 0, 1,
		1, u"_PP*1", 0, 0, 1,
		0);
	// as usual - to match ST, as should be an adverb
	cPattern::create(u"__CLOSING__S1{_BLOCK:_ONLY_END_MATCH}", u"B",
		1, u",", 0, 0, 1,
		1, u"adverb|as*-2", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 1, 1,
		1, u"_PP*1", 0, 0, 1,
		0);
	return 0;
}

// Core declarative __S1 family (Quirk SVC/SVA/SV/SVO/SVOC/SVOA/SVOO):
// __C1__S1 + __ALLVERB/_COND + optional __INTERS1 + __ALLOBJECTS_* +
// optional _PP/_REL/_INFP + __CLOSING__S1.  _FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN
// so a lone __S1 can win a sentence but a cheaper non-final wrapper can
// still absorb it as a child.  Returns 0.
int createSecondaryPatterns__S1(void)
{
	LFS
		// DECLARATIVES p.803 CGEL
	// The simple sentence (p. 754)
	// SVC, SVA, SV, SVO, SVOC, SVOA, SVOO
	// SVC: Subject, Verb, subject Complement
	// SVA: Subject, Verb, Adverbial
	// not only ..., but
	// he/The sheriff throws/threw the ball/him/himself off the porch
	// what threw him off the porch?
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"1",
		1, u"__C1__S1", 0, 1, 1,
		2, u"__ALLVERB", u"_COND{VERB}", 0, 1, 1,
		1, u"__INTERS1{BLOCK}", 0, 0, 1,
		// Over the stone he fell, -- right into the big pile of leaves under the oak tree .
		2, u":", u",*2", 0, 0, 1, // and they were: "dasher", "prancer", and "comet".  / ALSO another voice[boris] which Tommy rather thought was that[boris] of boris replied :
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	// *I* never saw so young a child with so perverse an inclination . 
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"SO",
		1, u"__C1__S1", 0, 1, 1,
		1, u"__ALLVERB", 0, 1, 1,
		1, u"adverb|so", 0, 1, 1,
		1, u"adjective", 0, 1, 1,
		1, u"__ALLOBJECTS_1", 0, 1, 1,
		1, u"preposition", 0, 1, 1,
		1, u"adverb|so", 0, 1, 1,
		1, u"adjective", 0, 1, 1,
		1, u"__ALLOBJECTS_1", 0, 1, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	// Again against her will she made *reply* .
	cPattern::create(u"__S1{_FINAL:_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"MR",
		1, u"__C1__S1", 0, 1, 1,
		1, u"verb|made*-2{VERB}", 0, 1, 1,
		2, u"noun|reply{OBJECT}", u"noun|answer{OBJECT}", 0, 1, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	// OBJVERB patterns - reverse
	// *This* she knew had caused a change in her own attitude 
	// ‘ That you can do next *time* . 
	//NOT
	// Freddie Firefly saw at last **that SUBJECT{he} VERB{V_AGREE{past{was}}} in a terrible fix** .
	// He says , poor man , **that SUBJECT{he} VERB{V_AGREE{past{went}}} out** and got you flowers . 
	cPattern::create(u"__S1{_ONLY_BEGIN_MATCH:_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"R",
		1, u"predeterminer|all", 0, 0, 1, // All *this* the governor of the prison affected to disbelieve
		2, u"demonstrative_determiner|this*2{OBJECT}", u"demonstrative_determiner|that*3{OBJECT}", 0, 1, 1, // cost of 2 because this can easily be misused, especially the very ambiguous 'that'! 
		2, u"personal_pronoun", u"personal_pronoun_nominative", 0, 1, 1,                                                                          // out of 106 sources, this is used 1275 times with a cost of 1 for this/that
		2, u"__ALLVERB", u"_COND{VERB}", 0, 1, 1,
		// *This* she had done solely to appease Marian Barber's wounded pride . 
		3, u"__ALLOBJECTS_0", u"_INFP", u"_REL1[*]", 0, 0, 1,
		1, u"__CLOSING__S1", 0, 0, 2,
		0);
	cPattern::create(u"__S1{_ONLY_BEGIN_MATCH:_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"R2",
		1, u"predeterminer|all", 0, 0, 1, // All *this* the governor of the prison affected to disbelieve
		2, u"demonstrative_determiner|this*2{OBJECT}", u"demonstrative_determiner|that*3{OBJECT}", 0, 1, 1,
		1, u"determiner", 0, 1, 1,
		1, u"adjective*1", 0, 0, 1,
		1, u"noun", 0, 1, 1,
		2, u"__ALLVERB", u"_COND{VERB}", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"_INFP", u"_REL1[*]", 0, 0, 1,
		1, u"__CLOSING__S1", 0, 0, 2,
		0);
	// One thing *I* would like to ask
	// Another thing *I* was mistaken about
	// One thing I will tell you
	cPattern::create(u"__S1{_ONLY_BEGIN_MATCH:_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"R3",
		3, u"personal_pronoun_nominative|one", u"pronoun|another", u"quantifier|some", 0, 1, 1,
		1, u"noun|thing{OBJECT}", 0, 1, 1,
		1, u"__C1__S1", 0, 1, 1,
		2, u"__ALLVERB", u"_COND{VERB}", 0, 1, 1,
		4, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"_INFP", u"_REL1[*]", 0, 0, 1,
		1, u"__CLOSING__S1", 0, 0, 2,
		0);
	// NOUN[R](The three thousand dollars I lent you) I regard as an investment - NOUN[R] is an object
	cPattern::create(u"__S1{_ONLY_BEGIN_MATCH:_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"R4",
		2, u"noun|here", u"__NOUN[*]*7{OBJECT}", 0, 1, 1,
		1, u"__C1__S1", 0, 1, 1,
		2, u"__ALLVERB", u"_COND{VERB}", 0, 1, 1,
		4, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"_INFP", u"_REL1[*]", 0, 0, 1,
		1, u"__CLOSING__S1", 0, 0, 2,
		0);
	// attempt it I must.
	cPattern::create(u"__S1{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"R7",
		1, u"_VERBPRESENT{V_OBJECT}", 0, 1, 1,
		1, u"personal_pronoun|it{OBJECT}", 0, 1, 1,
		1, u"personal_pronoun_nominative{SUBJECT}", 0, 1, 1,
		1, u"modal_auxiliary{V_AGREE}", 0, 1, 1,
		0);
	// END OBJVERB patterns - reverse END
	return 0;
}

// Late noun variants that need __S1 / __ALLVERB already defined: free-relative
// "whoever V OBJ" (__NOUN[Z]), measure "five dollars a week" ([MO]), and
// other S-containing noun shapes.  Returns 0.
int createSecondaryPatterns__NOUN(void)
{
	LFS
		// *I* shall have to speak to *whoever is in charge * .
	cPattern::create(u"__NOUN{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_BLOCK:GNOUN:VNOUN:_EXPLICIT_SUBJECT_VERB_AGREEMENT:_CHECK_IGNORABLE_FORMS}", u"Z",
		5, u"interrogative_determiner|whatever{SUBJECT}", u"interrogative_determiner|whichever{SUBJECT}", u"interrogative_determiner|whosoever{SUBJECT}", u"interrogative_determiner|whoever{SUBJECT}", u"interrogative_determiner|wherever{SUBJECT}", 0, 1, 1,
		1, u"__ALLVERB*1", 0, 1, 1, // make this more expensive than a normal__S1
		1, u"__INTERS1*1{BLOCK}", 0, 0, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
		0);
	// I will give you *five dollars a week*
	// You must produce 5 bushels of grain a day.
	cPattern::create(u"__NOUN{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_BLOCK:GNOUN:VNOUN}", u"MO",
		2, u"numeral_cardinal", u"Number", 0, 1, 1,
		1, u"noun*1", 0, 1, 1,
		1, u"_PP*2", 0, 0, 1,
		1, u"determiner|a", 0, 1, 1,
		2, u"timeUnit{TIMECAPACITY}", u"dayUnit{TIMECAPACITY}", 0, 1, 1,
		0);
	// __NOUN[D], __NOUN[E], and __NOUN[F] will function as subjects or objects by themselves.
	// they are split this way so that only "E" and "F" are checked for _AGREEMENT, and only "D" and "F" are checked for verb usage.
	// Otherwise, excessive time is wasted.
	cPattern::create(u"__VERBALPHRASENOUN{_FINAL_IF_ALONE:_BLOCK:GNOUN:VNOUN}", u"D",
		1, u"_VERBONGOING*1{VERB:vE}", 0, 1, 1,  // from C2__S1 - also matches _N1// this pattern should not be common
		//1,u"_INFP{OBJECT}",0,0,1,         // from C2__S1   RINFP 6/7/06
		//1,u"_PP",0,0,1,         // from C2__S1 - __PP after noun is absorbed into the noun itself through NOUN 9
		// if the following is made optional this pattern can match _NOUN[9] with an embedded _NOUN[2]
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 1, 1, // there must only be one adjective and it must be last (not mixed in) see *
		1, u"__ADJECTIVE", 0, 0, 1,
		0);
	// I can't help *wishing my legs were like Mr . Grouse's *. 
	cPattern::create(u"__VERBALPHRASENOUN{_BLOCK:_EXPLICIT_SUBJECT_VERB_AGREEMENT:NOUN:_CHECK_IGNORABLE_FORMS}", u"D2",
		1, u"SYNTAX:Accepts S as Object{vS:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		3, u"_NOUN_OBJ*1", u"__NOUN[*]*1", u"_PP*2", 0, 0, 1, // telling me you were with monsieur Poirot. 
		2, u"_ADJECTIVE", u"_ADVERB", 0, 0, 1, // feeling confident he was there.
		2, u"demonstrative_determiner|that*1{S_IN_REL}", u"quantifier|all", 0, 0, 1,
		2, u"__S1[R]*1{_BLOCK:OBJECT:EVAL}", u"_MS1[*]*2{_BLOCK:OBJECT:EVAL}", 0, 1, 1,
		0);
	// 'the man spending money' was their target.
	// pattern is not grouped with its other root patterns
	/* RINFP - INFP removed after VERB and placed in ALLOBJECTS as an object of the verb.
		 if a verb has an infinitive phrase after it, objects occurring after to-verb belong to the infinitive phrase NOT the main verb.
		 only one object is permitted before the infinitive phrase, and that is optional
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:_BLOCK:GNOUN:VNOUN}",u"E",
									 // if the following is made optional this pattern can match _NOUN[9] with an embedded _NOUN[2]
									 1,u"__C1__S1",0,1,1, // changed to cost *2 5/17 to avoid being the same cost as _NOUN[2]
									 1,u"_VERBONGOING*2{VERB:vE}",0,1,1,  // from C2__S1 - also matches _N1// this pattern should not be common
									 1,u"_INFP{OBJECT}",0,0,1,         // from C2__S1
									 2,u"_PP",u"_REL1[*]{_BLOCK}",0,0,1,         // from C2__S1 - __PP after noun is absorbed into the noun itself through NOUN 9
									 0);
 */
 // this pattern differentiator is specifically checked for in evaluateNounDeterminer!
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:_BLOCK:GNOUN:VNOUN:_CHECK_IGNORABLE_FORMS}", u"F",
		1, u"__C1__S1", 0, 1, 1, // changed to cost *2 5/17 to avoid being the same cost as _NOUN[2]
		1, u"_VERBONGOING*1{VERB:vE}", 0, 1, 1,  // from C2__S1 - also matches _N1// this pattern should not be common
		//1,u"_INFP{OBJECT}",0,0,1,         // from C2__S1   RINFP 6/7/06
		//1,u"_PP",0,0,1,         // from C2__S1 - __PP after noun is absorbed into the noun itself through NOUN 9
		// if the following is made optional this pattern can match _NOUN[9] with an embedded _NOUN[2] / this can still match because of __ALLOBJECTS_0
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1*1", u"__ALLOBJECTS_2*1", 0, 1, 1, // there must only be one adjective and it must be last (not mixed in) see *
		0);
	// My *coming* from the country to stay in Paris for good marked an epoch in my life .
	// Forgive me for not thinking of your *being* tired , mother
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:_BLOCK:GNOUN:VNOUN:_CHECK_IGNORABLE_FORMS}", u"COMING",
		1, u"possessive_determiner", 0, 1, 1,
		2, u"verb|coming", u"verb|being", 0, 1, 1,
		0);
	// Desborough was not so imposing and extensive *a* place as Pemberley 
	// How could he afford to retain so eminent *a* lawyer ? 
	// I did not believe that Turner had strength enough to fell so vigorous *a* man
	cPattern::create(u"__NOUN{_FINAL_IF_ALONE:_CHECK_IGNORABLE_FORMS}", u"SO",
		1, u"adverb|so", 0, 1, 1,
		1, u"adjective", 0, 1, 1,
		1, u"coordinator", 0, 0, 1,
		1, u"adjective", 0, 0, 1,
		1, u"determiner|a{DET}", 0, 1, 1,
		1, u"noun{N_AGREE}", 0, 1, 1,
		0);
	return 0;
}

// Extra verb/clause shapes that depend on __S1 (fronted adjective + be,
// more _VERBREL, coordinated verbs).  Several historical create() calls
// are left commented with the reason they were folded into __S1[1].
// Returns 0.
int createSecondaryPatterns__VERB(void)
{
	LFS

	// * When _ALLOBJECT was a single object:
	//     adjective must not be in __ALLOBJECT because __ALLOBJECT is repeated.  When this happens the adjective belonging to
	//     a noun is both an __ALLOBJECT and part of a _NOUN, which is itself an __ALLOBJECT, leading to a double match.
	//     absorbed was the king in all his splendor.
	/*
		should be processed by __S1[1] because C1__S1 now recognizes all adjectives.
	cPattern::create(u"__S1{_FINAL}",u"2",
									 1,u"_ADJECTIVE{SUBJECT:GNOUN}",0,1,1,
									 3,u"does{VERB:vS:V_AGREE}",u"have{VERB:vS:V_AGREE}",u"is{VERB:vS:V_AGREE}",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL,1,1,
									 2,u"_NOUN_OBJ{OBJECT}",u"__NOUN[*]{OBJECT}",0,1,1, // converted from __ALLOBJECTS because the previous list of verbs have one object maximum.
									 1,u"_ADJECTIVE",0,0,1,
									 0);

	cPattern::create(u"__S1{_FINAL}",u"3",
									 1,u"_ADJECTIVE{SUBJECT:GNOUN}*2",0,1,1,   // this pattern should be very rare
									 3,u"does{VERB:past:V_AGREE}",u"have{VERB:past:V_AGREE}",u"is{VERB:past:V_AGREE}",VERB_PAST|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL,1,1,
									 2,u"_ADVERB",u"preposition*2",0,0,2, // hanging preposition!
									 5,u"_NOUN_OBJ{OBJECT}",u"__NOUN[*]{OBJECT}",u"_INFP{OBJECT}",u"_PP",u"_ADJECTIVE",0,1,1, // converted from __ALLOBJECTS because the previous list of verbs have one object maximum.
									 1,u"_ADJECTIVE",0,0,1,
									 0);
	*/
	// treated similarly to VERBREL
	cPattern::create(u"_VERBPASSIVE_P{_FINAL_IF_ALONE}", u"1",
		// VERBPASTPART added 8/29/2006 to handle coordinators
		// they must be given the facts about AIDS and given -the -opportunity to discuss the issues
		1, u"_VERBPASSIVE", 0, 1, 1, // passive verbs take only one direct object (MAX) // john could be called -a -brilliant -conversationalist .
		2, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", 0, 0, 1, 0);
	cPattern::create(u"_VERBPASSIVE_P{_FINAL_IF_ALONE}", u"2",
		1, u"_THINKPASSIVE", 0, 1, 1, // passive verbs take only one direct object (MAX) // I was told it was M . Fouquet .
		2, u"_ADVERB", u"preposition*2", 0, 0, 2, // hanging preposition!
		1, u"__S1[*]{_BLOCK:OBJECT:EVAL}", 0, 1, 1,
		0);
	// VERBPASTPART added 8/29/2006 to handle coordinators
	// but 'forgotten to write it on the chart.' Secret Adversary
	cPattern::create(u"_VERBPASSIVE_ACP{_FINAL_IF_ALONE}", u"1",
		// they must be given the facts about AIDS and given -the -opportunity to discuss the issues
		1, u"_VERBPASTPART{vD:VERB}", 0, 1, 1, // passive verbs take only one direct object (MAX) // john could be called -a -brilliant -conversationalist .
		2, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", 0, 1, 1,  // make an object compulsory - a single past participle can be an adjective as in / English girls were just a mite moss-grown.
																									// otherwise _VERBPASSIVE_ACP can be a FINAL pattern by itself and the - grown is never incorporated into the rest of the sentence.
																									// also corrects this - / I've screwed and saved and pinched!
		0);
	// VERBPASTPART added 8/29/2006 to handle coordinators
	cPattern::create(u"_VERBPASSIVE_ACP{_FINAL_IF_ALONE}", u"2",
		// they must be given the facts about AIDS and given -the -opportunity to discuss the issues
		1, u"_THINKPASTPART{vD:VERB}", 0, 1, 1, // passive verbs take only one direct object (MAX) // john could be called -a -brilliant -conversationalist .
		2, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", 0, 1, 1,
		0);
	cPattern::create(u"__SUB_S1", u"1",
		1, u"_VERBPASSIVE_P", 0, 1, 1,
		1, u",", 0, 1, 1, 0);
	// block - too complex to evaluate subject-verb agreement (and passive forms don't agree)
	cPattern::create(u"__SUB_S2", u"1",
		1, u"__SUB_S1{_BLOCK:MVERB}", 0, 0, 3,
		1, u"_VERBPASSIVE_P", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		3, u"_VERBPASSIVE_ACP{_BLOCK:MVERB}", u"_VERBPASSIVE_P{_BLOCK:MVERB}", u"_VERBREL1{_BLOCK:MVERB}", 0, 1, 1,
		0);
	// passive sentence (only one direct object)
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"4",
		1, u"__C1__S1", 0, 1, 1,
		//1,u"_INFP{_BLOCK}",0,0,1, RINFP 6/7/06
		2, u"_PP", u"_REL1[*]", 0, 0, 1,
		1, u"_ADJECTIVE", 0, 0, 1,
		2, u"__SUB_S2", u"_VERBPASSIVE_P", 0, 1, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	// you do everything you could do.
	// you have done everything you could do.
	// you did everything you could do.
	// you will do everything you could do.
	//cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"D",
	//									0);
	// I will think you will be one forever.
	// I thought you were right.
	// I think you are right.
	// Lawrence told me you were with monsieur Poirot.
	cPattern::create(u"__ALLTHINK", u"",
		1, u"_ADVERB", 0, 0, 1,
		4, u"_THINK", u"_THINKPAST", u"_THINKPRESENT", u"_THINK_BARE_INF", 0, 1, 1,
		0);

	// She thinks that if you are going to carry her banner in the procession you ought to let her take your light .
	// XXIC Henty G. A. (George Alfred) 1832-1902\Dorothys Double Volume III (of 3)[29123-29128]:
	// “ Of course, no one man would attempt such a thing, ” Ned Hampton said, “ but [I believe in some of **the camps they have banded** together] and given the gamblers and the hard characters notice to quit, and have hung up those who refused to go .
	// verbverb cases have been removed since they were already covered (and better because they were more restrictive):I helped him cross the road. / I heard the car hit the tree. / I saw him hit the man.
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"5",
		1, u"__C1__S1", 0, 1, 1,
		1, u"__ALLTHINK{VERB}", 0, 1, 1, // u"think" for all active tenses! - possibly add INTRO_S1.
		// removed OBJECT from _NOUN as this will cause IS to have two objects (including the one from _S1) which is wrong and will cause elimination of this pattern
		4, u"_NOUN_OBJ", u"__NOUN[*]", u"_PP", u"__VERBALPHRASENOUN", 0, 0, 1, // Lawrence told *me* you were with monsieur Poirot. /  She felt confident he was there.
		2, u"_ADJECTIVE", u"_ADVERB", 0, 0, 1,  // I tell you frankly, she would hate you.
		3, u"demonstrative_determiner|that{S_IN_REL}", u"quantifier|all*-2", u",", 0, 0, 1,
		// __VERBREL2 below: she has long forgotten having given me an invitation . 
		3, u"__S1[R]{_BLOCK:OBJECT:EVAL:SUBJUNCTIVE}", u"_MS1[*]{_BLOCK:OBJECT:EVAL}", u"_VERBREL2{_BLOCK:OBJECT:EVAL}", 0, 1, 1,
		0);
	// I would have him fired
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"1P",
		1, u"__C1__S1", 0, 1, 1,
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1,
		1, u"__ALLOBJECTS_1", 0, 1, 1,
		1, u"_VERBPASTPART{vAB}", 0, 1, 1, 0);

	//cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"X",
	//									1, u"__C1__S1", 0, 1, 1,
	//									1, u"verbverb{VERB:V_AGREE}", 0, 1, 1, 
	//									2, u"_NOUN_OBJ{OBJECT}", u"__NOUN[*]{OBJECT}", 0, 1, 1, // Lawrence told me you were with monsieur Poirot. /  She felt confident he was there.
	//									2, u"demonstrative_determiner|that{S_IN_REL}", u"quantifier|all*-2", 0, 0, 1,
	//									2, u"__S1[R]{_BLOCK:OBJECT:EVAL}", u"_MS1[*]{_BLOCK:OBJECT:EVAL}", 0, 1, 1,
	//									0);
	// missing 'that' clause
	// Her hope was she would be allowed to skip soccer.
	// I am afraid we shan't require his services .
	// I am happy I am going to the house.
	// Bill would have been ecstatic his neice is graduating tomorrow.
	// I was afraid they would hear the beating of my heart .
	// It was extremely likely there would be no second taxi.
	// have been / had been
	// would be / will be
	cPattern::create(u"_WOULDBE{VERB}", u"",
		1, u"_COND", 0, 1, 1,
		1, u"_BE{vS:V_OBJECT:id}", 0, 1, 1, 0);
	cPattern::create(u"_HAVEBEEN{VERB}", u"",
		1, u"_HAVE", 0, 1, 1,
		1, u"_BEEN{vB:id}", 0, 1, 1, 0);
	cPattern::create(u"_COULDHAVEBEEN{VERB}", u"6",
		1, u"_COND", 0, 1, 1,
		1, u"_HAVE", 0, 1, 1,
		1, u"_BEEN{vAB:id}", 0, 1, 1, 0);
	cPattern::create(u"_WOULDMAKE{VERB}", u"",
		1, u"_COND", 0, 1, 1,
		1, u"verb|make{vS:V_OBJECT:id}", 0, 1, 1, 0);
	cPattern::create(u"_HAVEBEENMAKING{VERB}", u"",
		1, u"_HAVEBEEN", 0, 1, 1,
		1, u"verb|making{vB:id}", 0, 1, 1, 0);
	cPattern::create(u"_COULDHAVEBEENMAKING{VERB}", u"",
		1, u"_COULDHAVEBEEN", 0, 1, 1,
		1, u"verb|making", 0, 1, 1,
		0);
	return 0;
}

// Multi-clause _MS1/_MS wrappers and the "make NP ADJ S" __S1[M4] blend.
// _MS1 is the usual top-level sentence pattern (_FINAL / _ONLY_BEGIN_MATCH /
// _STRICT_NO_MIDDLE_MATCH).  Returns 0.
int createSecondaryPatterns_MS1(void)
{
	LFS
		// *I* shall make you sorry you ever came near my hickory tree
	// A blend of R3 and 5
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"M4",
		1, u"__C1__S1", 0, 1, 1,
		4, u"verb|make{VERB:vS:id}", u"_WOULDMAKE", u"_HAVEBEENMAKING", u"_COULDHAVEBEENMAKING", 0, 1, 1,
		3, u"_NOUN_OBJ", u"__NOUN[*]", u"__VERBALPHRASENOUN", 0, 1, 1, // Lawrence told me you were with monsieur Poirot. 
		1, u"_ADJECTIVE", 0, 1, 1,
		3, u"_PP", u"_ADVERB", u"demonstrative_determiner|that{S_IN_REL}", 0, 0, 2,
		1, u"__S1[*]*1{_BLOCK:OBJECT:EVAL}", 0, 1, 1,
		0);
	cPattern::create(u"_MS1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_STRICT_NO_MIDDLE_MATCH}", u"V",
		1, u"_VERBREL1", 0, 1, 1,
		1, u"conjunction", 0, 1, 1,
		1, u"__S1[*]*1{_BLOCK:OBJECT:EVAL}", 0, 1, 1,
		0);
	// *I* reckon the trouble is they are my kind. - A combination of S1[5] and MS1[2]
	cPattern::create(u"_MS1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_STRICT_NO_MIDDLE_MATCH}", u"H",
		1, u"_STEP", 0, 0, 1,
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"__C1__S1", 0, 1, 1,
		1, u"__ALLTHINK{VERB}", 0, 1, 1, // u"think" for all active tenses! - possibly add INTRO_S1.
		2, u"demonstrative_determiner|that{S_IN_REL}", u"quantifier|all*-2", 0, 0, 1,
		1, u"__C1__S1", 0, 1, 1,
		4, u"_IS{VERB:vS:id}", u"_WOULDBE", u"_HAVEBEEN", u"_COULDHAVEBEEN", 0, 1, 1,
		// removed OBJECT from _NOUN as this will cause IS to have two objects (including the one from _S1) which is wrong and will cause elimination of this pattern
		1, u"_ADJECTIVE", 0, 0, 1, // make NOUN more expensive because this is redundant with _NOUN[5] 
		1, u"__S1[*]*1{_BLOCK:OBJECT:EVAL}", 0, 1, 1, // rare             // and in general _NOUN[5] is correct when this is a NOUN
		1, u"_MSTAIL", 0, 0, 1,
		0);
	// I would rather suffer than have him suffer.  // centers on 'rather than' pattern
	cPattern::create(u"_MS1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_STRICT_NO_MIDDLE_MATCH}", u"RT",
		1, u"__C1__S1", 0, 1, 1,
		1, u"modal_auxiliary|would", 0, 1, 1,
		1, u"adverb|rather", 0, 1, 1,
		1, u"verb", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		1, u"have", 0, 1, 1,
		1, u"__ALLOBJECTS_1", 0, 1, 1,
		1, u"verb", VERB_PRESENT_FIRST_SINGULAR, 1, 1,
		0);
	// if _ADJECTIVE is not optional, DOES NOT match:
	// The fact is it is all *so* funny.   The trouble is I can't trust you.
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"R5",
		1, u"__C1__S1", 0, 1, 1,
		4, u"_IS{VERB:vS:id}", u"_WOULDBE", u"_HAVEBEEN", u"_COULDHAVEBEEN", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 1, 1,
		2, u"_PP", u"_ADVERB", 0, 0, 2,
		1, u"__S1[*]*2{_BLOCK:OBJECT:EVAL}", 0, 1, 1, // rare             
		0);
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"R6",
		1, u"determiner|the", 0, 1, 1,
		1, u"noun*15", 0, 1, 1, // fact, trouble, idea
		1, u"_IS{VERB:vS:id}", 0, 1, 1,
		2, u"_PP", u"_ADVERB", 0, 0, 2,
		1, u"__S1[*]*1{_BLOCK:OBJECT:EVAL}", 0, 1, 1, // rare             
		0);
	// it is me they are after.
	// It was Tilda they would go after next.
	// The infernal skunk , it is **a pity he didn't go** down twenty years ago . / prevent NOUN[R] from dominating
	// this is slightly different than MS1[2] to capture the verb objects proper word relation
	// It ishas a mercy you are better ! / mercy is the object here.  This is reflected in the below pattern, but not in the above pattern.
	// NOUN could be made optional like ADJECTIVE above, but then when it does appear, IS will have two objects, thus dooming the match.
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"9",
		1, u"__C1__S1", 0, 1, 1,
		4, u"_IS{VERB:vS:id}", u"_WOULDBE", u"_HAVEBEEN", u"_COULDHAVEBEEN", 0, 1, 1,
		// removed OBJECT from _S1 as this will cause IS to have two objects which is wrong and will cause elimination of this pattern
		// _ADJECTIVE: I am *sure* I would have been so *scared* I wouldhad have gone right ahead
		2, u"__NOUN[*]*1{OBJECT}", u"_NOUN_OBJ*1{OBJECT}", 0, 1, 1, // make NOUN more expensive because this is redundant with _NOUN[5] 
		1, u"__S1[*]*1{_BLOCK:EVAL}", 0, 1, 1, // rare             // and in general _NOUN[5] is correct when this is a NOUN
		0);
	// *I* have a notion there ishas a gully between her and us , ” he remarked .
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"9N",
		1, u"__C1__S1", 0, 1, 1,
		1, u"_HAVE{VERB:vS:id}", 0, 1, 1,
		1, u"determiner|a", 0, 1, 1,
		1, u"noun|notion", 0, 1, 1,
		1, u"__S1[*]*1{_BLOCK:EVAL}", 0, 1, 1, // rare             // and in general _NOUN[5] is correct when this is a NOUN
		0);
	// He is a little red.
	/// do not change this # "7" without changing the calculateVerbAfterVerbUsage code.
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"7",
		1, u"__C1__S1", 0, 1, 1,
		4, u"_IS{VERB:vS:id}", u"_WOULDBE", u"_HAVEBEEN", u"_COULDHAVEBEEN", 0, 1, 1,
		1, u"_ADJECTIVE*-1", 0, 1, 1,  // calculateVerbAfterVerbUsage specifically removes this -1 incentive by matching on this pattern!
		1, u"reflexive_pronoun", 0, 0, 1,
		3, u"_PP", u"_REL1[*]", u"_INFP", 0, 0, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	// Too expensive a piano for me.
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"TOO",
		1, u"adverb|too", 0, 1, 1,
		1, u"adjective", 0, 1, 1,
		1, u"__NOUN", 0, 1, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	return 0;
}

// Pre-nominal / pre-clausal introducers (__INTRO_N, __INTRO_N_ET, _STEP):
// time NPs, adverbs, interjections that must not be mistaken for the
// subject of a following Q1/VERBREL.  FLOATTIME tags feed timeRelations.
// Returns 0.
int createSecondaryPatterns__INTRO_N(void)
{
	LFS
		// Later, in her room, she sat and read.
	// restrict this to things relating to "time"? (see [9])
	// __INTRO_N was introduced to prevent meta patterns incorporating Q1 and VERBREL from 
	// acquiring subjects (through __INTRO_N) and then eliminating _S1 by being lower in cost.  __INTRO_S1 should be used
	// in these situations because it requires a separator for most cases.
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH:FLOATTIME}", u"1",
		1, u"__NOUN[*]*1{OBJECT}", 0, 0, 1,
		3, u"_ADVERB*1", u"interjection", u"conjunction|though*-3", 0, 1, 1, // could be a time / though is not used as a conjunction often, but should be one in this position
		1, u",", 0, 0, 1,
		0);
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH}", u"2",
		2, u"_ADJECTIVE*1", u"uncertainDurationUnit", 0, 1, 1, // afraid I don't. / while **the little Pilgrim was still gazing** , disappeared from her
		0);

	// Thank heaven you have never kept a fashion.
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH}", u"4",
		1, u"verb|thank", 0, 1, 1,
		1, u"noun|heaven", 0, 1, 1,
		0);
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH}", u"ID",
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		1, u"verb|doubt*-3", 0, 1, 1,
		1, u"conjunction|if", 0, 1, 1,
		0);
	// Good thing you know *I* am in the house.
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH}", u"5",
		1, u"adjective|good", 0, 1, 1,
		1, u"noun|thing", 0, 1, 1,
		0);
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH}", u"3",
		3, u"_ADVERB", u"_ADJECTIVE*1", u"interjection*-4", 0, 1, 1, // glibly, she refused. / New, isn't she?
		1, u",", 0, 1, 1,
		1, u"adverb", 0, 0, 1,
		0);
	cPattern::create(u"__INTRO_N{}", u"6",
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		1, u"verbverb|dare", 0, 1, 1,
		1, u"verb|say", 0, 1, 1,
		0);
	// the next thing *I* knew it was morning .
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH}", u"7",
		1, u"determiner|the", 0, 1, 1,
		1, u"adjective|next", 0, 1, 1,
		1, u"noun|thing", 0, 1, 1,
		0);
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH}", u"8",
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		1, u"verb|take", 0, 1, 1,
		1, u"personal_pronoun|it", 0, 1, 1,
		0);
	// *I* had no idea it was so late .
	// *I* have an idea our audience will be very large . 
	// *I* have no doubt you shall do the square thing.
	cPattern::create(u"__INTRO_N{}", u"9",
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		3, u"have|have", u"have|had", u"have|wouldhad", 0, 1, 1,
		2, u"verb|got", u"verb|gotten", 0, 0, 1,
		4, u"no", u"determiner|an", u"determiner|a", u"quantifier|some", 0, 0, 1, // optional: I had doubt the aliens would get us.
		2, u"noun|idea", u"noun|doubt", 0, 1, 1,
		0);
	// *I* don't doubt you shall do the right thing.
	cPattern::create(u"__INTRO_N{}", u"A",
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		1, u"does_negation|don't", 0, 0, 1,
		1, u"noun|doubt", 0, 1, 1,
		0);
	// I give you my solemn oath *I* shall be good to you both ! 
	// I give you my word *I* don't put it on ; 
	// “ I give you my word *I* don't understand it , ” said the man . 
	cPattern::create(u"__INTRO_N{}", u"B",
		2, u"personal_pronoun_nominative|i", u"personal_pronoun|you", 0, 1, 1,
		2, u"verb|give", u"verb|gave", 0, 1, 1,
		1, u"__NOUN", 0, 1, 1,  // him, Ben, the group
		2, u"possessive_determiner|my", u"possessive_determiner|your", 0, 1, 1,
		1, u"adjective", 0, 0, 1, // solemn
		2, u"noun|word", u"noun|oath", 0, 1, 1,
		0);
	return 0;
}

// Sentence introducers and a few extra __S1 vocative/interjection shapes
// ("Hullo, stranger", "Oh, well"): _INTRO_S1, __INTRO_S1, __AS_AS.
// Prefer _INTRO_S1 (separator-bounded) over __INTRO_N in Q1/VERBREL
// wrappers so a hidden subject cannot cheapen a non-S1 parse.  Returns 0.
int createSecondaryPatterns__INTRO_S1(void)
{
	LFS

	// Hullo, stranger
	// Oh, well! / Oh, very well!
	// Oh, dear mother earth! 
	// Oh, and my dearest father too?
	cPattern::create(u"__S1{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN}", u"8",
		2, u"possessive_determiner|my", u"interjection*-4", 0, 1, 1, // my dear
		1, u",", 0, 1, 1,
		1, u"coordinator", 0, 0, 1, // Oh, dear mother earth! / Oh, and my dearest father too?
		2, u"adverb", u"adjective", 0, 0, 1, // Oh, dear mother earth!
		4, u"possessive_determiner|my*-7", u"honorific{HON:HAIL}", u"noun{HAIL}", u"interjection*-4", MALE_GENDER | FEMALE_GENDER, 1, 1,
		1, u"noun*1", 0, 0, 1, // Oh, dear mother earth!
		1, u"adverb", 0, 0, 1, // Oh, and my dearest father too?
		0);
	// *But* doesn't some one of that name live here ?
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"C",
		2, u"conjunction", u"conjunction|for*-3", 0, 1, 1, // for is not usually a conjunction
		0);
	// added interjection to take care of 'Well' Stanford mismatch
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"2",
		2, u"interjection*-4", u"politeness_discourse_marker*-1", 0, 0, 1,
		1, u",", 0, 0, 1,
		9, u"_HAIL_OBJECT", u"_NAME{HAIL}", u"honorific{HON:HAIL}", u"_HON_ABB{HON:HAIL}", u"_META_GROUP{HAIL}", u"_VERBREL2{_BLOCK}", u"_VERBREL1{_BLOCK}", u"interjection*-4", u"politeness_discourse_marker*-1", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"2MD",
		1, u"possessive_determiner|my*-7", 0, 1, 1,
		1, u"noun|dear*-6", 0, 1, 1,
		1, u",", 0, 1, 1,
		0);
	cPattern::create(u"__INTRO2_S1{_ONLY_BEGIN_MATCH:FLOATTIME}", u"3",
		3, u"_TIME", u"_DATE", u"letter", 0, 1, 1,
		3, u"dash", u":", u",", 0, 0, 1,
		0);
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH:FLOATTIME}", u"4", // could contain _NOUN[J], which could contain a TIME
		1, u"__NOUN[*]{OBJECT:HAIL}", 0, 1, 1,
		3, u"dash", u":", u",", 0, 1, 1,
		0);
	// I *want* to ask him -- Oh , take me to where I can see His face !
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"S",
		1, u"__S1*1{_BLOCK}", 0, 1, 1,
		1, u"dash", 0, 1, 1,
		0);
	// the more likely/ the less likely
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"T",
		1, u"determiner|the", 0, 1, 1,
		2, u"adverb|more", u"adverb|less", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 1, 1,
		0);
	cPattern::create(u"__INTRO2_S1{_ONLY_BEGIN_MATCH:FLOATTIME}", u"5", // PP could match a time
		9, u"__AS_AS", u"_PP", u"_VERBREL2*1{_BLOCK:EVAL}", u"_REL1[*]*1", u"_INFP{BLOCK}", u"conjunction", u"coordinator", u"adverb|then", u"adverb|so", 0, 1, 1, // took out *1 from _PP - discouraged legitimate leading prepositional phrases
		5, u"dash", u":", u",", u"__ADVERB", u"conjunction|though*-3", 0, 0, 1, // though is not used as a conjunction often, but should be one in this position
		0);
	cPattern::create(u"__INTRO2_S1{_ONLY_BEGIN_MATCH}", u"6",
		1, u"conjunction|if", 0, 1, 1,
		1, u"pronoun|so", 0, 1, 1,
		1, u",", 0, 0, 1,
		0);
	// What I mean is, ...
	cPattern::create(u"__INTRO2_S1{_ONLY_BEGIN_MATCH}", u"7",
		1, u"what", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_IS", 0, 1, 1,
		1, u",", 0, 0, 1,
		0);
	// For that very reason 
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"REASON1",
		1, u"preposition|for", 0, 1, 1,
		1, u"demonstrative_determiner|that", 0, 1, 1,
		1, u"adjective|very", 0, 0, 1,
		1, u"noun|reason", 0, 1, 1,
		0);
	// All the more *reason*
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"REASON2",
		1, u"predeterminer|all", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"adjective|more", 0, 1, 1,
		1, u"noun|reason", 0, 1, 1,
		0);

	// simple command
	// come on
	// saddle up, we need to get going.
	cPattern::create(u"__INTRO2_S1{_ONLY_BEGIN_MATCH}", u"8",
		1, u"_VERBPRESENT*2{VERB}", 0, 1, 1,
		2, u"_ADVERB", u"preposition*1", 0, 0, 1,
		1, u",*-2", 0, 0, 1,
		0);
	// Every moment I fall in love with you.
	// A moment later she fell outside the window.
	// Wednesdays he practiced piano.
	// The minute she got through the door he was kissing her furiously.
	// One beautiful , warm , dark night 
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH:NOUN:OBJECT:FLOATTIME}", u"C",
		1, u"preposition", 0, 0, 1, // after a moment / after a while
		4, u"determiner{TIMEMODIFIER:DET}", u"demonstrative_determiner{TIMEMODIFIER:DET}", u"quantifier{TIMEMODIFIER:DET}", u"numeral_cardinal{TIMEMODIFIER:DET}", 0, 0, 1,
		1, u"_ADJECTIVE{TIMEMODIFIER:_BLOCK}", 0, 0, 1,
		7, u"month{MONTH:N_AGREE}", u"daysOfWeek{DAYWEEK:N_AGREE}", u"season{SEASON:N_AGREE}", u"timeUnit{TIMECAPACITY:N_AGREE}",
		u"dayUnit{TIMECAPACITY:N_AGREE}", u"uncertainDurationUnit{TIMECAPACITY:N_AGREE}", u"simultaneousUnit{TIMECAPACITY:N_AGREE}", 0, 1, 1, // A moment
		1, u"_ADVERB", 0, 0, 1, // later
		1, u",", 0, 0, 1,
		0);
	// TODO merge with _ADVERB[T] patterns?
	cPattern::create(u"__INTRO_N_ET{_ONLY_BEGIN_MATCH:FLOATTIME}", u"1",
		2, u"quantifier|every{TIMEMODIFIER:DET}", u"quantifier|each{TIMEMODIFIER:DET}", 0, 1, 1,
		1, u"noun|time{TIMECAPACITY:N_AGREE}", 0, 1, 1, // A moment
		0);
	// ~~~ XXIC Simak, Clifford D.\Hellhounds of the Cosmos[10969-10973]:
	// One moment it had been pitch dark , the next it was light
	cPattern::create(u"__INTRO_N{_ONLY_BEGIN_MATCH:NOUN:OBJECT:FLOATTIME}", u"D",
		1, u"determiner|the{TIMEMODIFIER:DET}", 0, 1, 1,
		1, u"numeral_ordinal|next*1{N_AGREE}", 0, 1, 1, // The next (*1 - don't interfere with next being used as an adjective)
		1, u",", 0, 0, 1,
		0);
	// XXIC Lee, Jennette\Aunt Jane[27134 - 27139]:
	// It ishas **a pity you didn't think** about that sooner , wasn't it ?
	cPattern::create(u"__INTRO_NS", u"",
		1, u"personal_pronoun|it*-2", 0, 1, 1, // make up for the subject verb agreement (-1) and the verb objects (-1)
		1, u"is", 0, 1, 1,
		0);
	// ~~~ XXIC Lee, Jennette\Aunt Jane[23531-23536]:
	// “ Seems a pity he can't see them , ” she thought , watching the faces . 
	cPattern::create(u"__INTRO_NP{NOUN:OBJECT}", u"B",
		2, u"verb|seems", u"__INTRO_NS", 0, 0, 1,
		1, u"determiner|a{DET}", 0, 1, 1,
		1, u"noun|pity{N_AGREE}", 0, 1, 1,
		0);
	// hardly able to contain his excitement,
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"A",
		1, u"_ADJECTIVE", 0, 1, 1, // hardly able
		1, u"_INFP", 0, 1, 1, // _INFP made optional would replicate _INTRO_S1[1]
		1, u",", 0, 1, 1,
		0);
	// The girl assenting,
	cPattern::create(u"__INTRO_S1{_ONLY_BEGIN_MATCH}", u"B",
		2, u"__NOUN[*]{SUBJECT}", u"__MNOUN[*]{SUBJECT}", 0, 1, 1, // The girl
		1, u"_VERBONGOING", 0, 1, 1, // assenting
		1, u",", 0, 1, 1,
		0);
	// One beautiful , warm , dark night early in the summer
	cPattern::create(u"_INTRO_S1{_NO_REPEAT}", u"1",
		4, u"__INTRO_S1{_BLOCK:EVAL}", u"__INTRO2_S1{_BLOCK:EVAL}", u"__INTRO_N{_BLOCK:EVAL}", u"_PP", 0, 1, 1,
		// the following line may be an error! - since each of these patterns are marked ONLY_BEGIN_MATCH, this will never match anything
		3, u"__INTRO_S1*1{_BLOCK:EVAL}", u"__INTRO2_S1*1{_BLOCK:EVAL}", u"__INTRO_N*1{_BLOCK:EVAL}", 0, 0, 2,
		1, u"_ADJECTIVE", 0, 0, 1,
		1, u"_PP", 0, 0, 1,
		0);
	return 0;
}

// Short / fragmentary sentences (__S2): "Please, Tommy.", "I never.", and
// other _FINAL+_ONLY_BEGIN+_ONLY_END_MATCH shapes that should win only as
// a whole utterance.  Returns 0.
int createSecondaryPatterns__S2(void)
{
	LFS
		// Please, Tommy?  Later, Tommy. Yes Mom. Sorry, Tuppence.
	cPattern::create(u"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"5",
		2, u"_ADVERB", u"interjection*-4", 0, 1, 1, // pray / please / Yes (ADVERB)
		1, u",", 0, 0, 1,
		1, u"_NAME_INJECT", 0, 1, 1, 0);
	cPattern::create(u"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"6",
		1, u"__INTRO_N{_BLOCK:EVAL}", 0, 0, 1,
		1, u"personal_pronoun_nominative|i", 0, 1, 1,
		1, u"never", 0, 1, 1,
		0);
	cPattern::create(u"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"S1",
		1, u"preposition|of", 0, 1, 1,
		1, u"noun|course", 0, 1, 1,
		1, u"not", 0, 1, 1,
		0);
	cPattern::create(u"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"S2",
		4, u"indefinite_pronoun|anything", u"indefinite_pronoun|anybody", u"indefinite_pronoun|anyone", u"indefinite_pronoun|nothing", 0, 1, 1,
		3, u"adverb|else", u"quantifier|more", u"interrogative_pronoun|whatever", 0, 1, 1,
		0);
	cPattern::create(u"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"S3",
		1, u"adjective|glad", 0, 1, 1,
		1, u"preposition|to", 0, 1, 1,
		1, u"verb|see", 0, 1, 1,
		1, u"personal_pronoun|you", 0, 1, 1,
		0);
	cPattern::create(u"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"S4",
		1, u"determiner|the", 0, 1, 1,
		1, u"adverb|sooner", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"adverb|better", 0, 1, 1,
		0);
	cPattern::create(u"__S2{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"S6",
		1, u"noun|goodness", 0, 1, 1,
		1, u"personal_pronoun_accusative|me", 0, 1, 1,
		0);
	// __S2 -- 8 and 9
	// if no pattern matches a plain interjection, that interjection will never be matched as one
	// if it can be matched as anything else, even though those other forms carry a higher cost.
	cPattern::create(u"__S2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"8",
		2, u"interjection*-4", u"politeness_discourse_marker", 0, 1, 1, // pray / please / Yes (ADVERB)
		1, u",", 0, 0, 1,
		0);
	cPattern::create(u"__S2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"9",
		1, u"quotes", OPEN_INFLECTION, 1, 1,
		2, u"interjection", u"politeness_discourse_marker", 0, 1, 1, // pray / please / Yes (ADVERB)
		2, u",", u".", 0, 0, 1,
		1, u"quotes", CLOSE_INFLECTION, 1, 1,
		0);
	return 0;
}

// Imperatives: _INTROCOMMAND1 ("let us"), _COMMAND1, and related
// verb-first shapes.  _QUESTION is not set (costing treats a missing '?'
// as a bonus for commands and a penalty for questions).  Returns 0.
int createSecondaryPatterns_COMMAND(void)
{
	LFS
		//cPattern::create(u"__S1{_FINAL}",u"7",
	//                 1,u"__C1__S1",0,1,1,
	//                 1,u"_VERB_ID{id}",0,1,1,
	//                 1,u"_ADJECTIVE",0,1,1,
	//                 1,u"_PP",0,1,1,
	//                 0);
	// let us
	cPattern::create(u"_INTROCOMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"1",
		1, u"verb|let", 0, 1, 1,
		1, u"personal_pronoun_accusative|us", 0, 1, 1,
		0);

	// IMPERATIVES p.803 CGEL
	// don't _BE _NOUN _NOUN_OBJ
	// don't -be -hasty .
	// don't -be a fool . "
	// stop it.
	// " don't -be too -disconsolate , Miss Tuppence , " he said in a low voice .
	// please be quiet.
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"1",
		2, u"_ADVERB", u"_NAME", 0, 0, 1, // pray / please / Yes (ADVERB) / Tommy
		1, u",", 0, 0, 1,
		2, u"_DO{imp}", u"_INTROCOMMAND1", 0, 0, 1,
		1, u"_BE{VERB:vS:id}", 0, 1, 1,
		6, u"__NOUN[*]{OBJECT}", u"_NOUN_OBJ{OBJECT}", u"__NOUNREL{OBJECT}", u"_VERBPASTPART", u"_ADJECTIVE", u"__VERBALPHRASENOUN[*]{OBJECT}", 0, 0, 1, 0);
	// make Ben help me.
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"2",
		2, u"_ADVERB", u"_NAME", 0, 0, 1, // pray / please / Yes (ADVERB) / Tommy
		1, u",", 0, 0, 1,
		1, u"_INTROCOMMAND1", 0, 0, 1,
		1, u"verbverb{vS:V_HOBJECT}", VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_SECOND_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		4, u"_NOUN_OBJ{HOBJECT}", u"__NOUN[*]{HOBJECT}", u"__MNOUN[*]{HOBJECT}", u"__VERBALPHRASENOUN[*]{HOBJECT}", 0, 1, 1,
		2, u"_ADVERB", u"_PP", 0, 0, 2,
		1, u"_VERBPRESENT{VERB}", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	// to the point, my friend.
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"3",
		1, u"_PP", 0, 1, 1,
		2, u"__NOUN[*]{OBJECT}", u"_VERBREL1*4", 0, 1, 1,
		0);
	// let us hope so.
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"7",
		1, u"_VERB_BARE_INF", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		0);
	// bake some buns with me, please.
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"4",
		1, u"_INTROCOMMAND1", 0, 0, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"_DO{imp}", 0, 0, 1,
		4, u"_VERBPRESENT{VERB:V_OBJECT}", u"_VERBPRESENT_CO", u"is{vS:id:V_OBJECT:VERB}", u"is_negation{vS:id:not:V_OBJECT:VERB}",
		VERB_PRESENT_FIRST_SINGULAR | VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_PLURAL, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // u"there{OBJECT}u" removed
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	// be sure you wait.
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"5",
		2, u"_ADVERB", u"_INTROCOMMAND1", 0, 0, 1, // pray / please / Yes (ADVERB) 
		2, u"be", u"verb|make", 0, 1, 1,
		1, u"_ADVERB", 0, 1, 1, // sure / certain
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		0);
	// never mind! // this pattern is specifically referenced in specials_main
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"8",
		2, u"conjunction|but", u"interjection", 0, 0, 1,
		1, u",", 0, 0, 1,
		1, u"never", 0, 1, 1,
		2, u"verb|mind*-3", u"verb|fear*-3", 0, 1, 1,
		2, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", 0, 0, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	cPattern::create(u"_THINKPRESENT_CO", u"",
		1, u"_THINKPRESENTFIRST", 0, 1, 1,
		1, u"coordinator", 0, 1, 1,
		1, u"_THINKPRESENTFIRST{VERB}", 0, 1, 1,
		0);
	// [don't you] say you will marry me.
	cPattern::create(u"_SUBCOM", u"",
		1, u"_DO{imp}", 0, 1, 1,
		3, u"__NOUN[*]{OBJECT}", u"_NOUN_OBJ{OBJECT}", u"__NOUNREL{OBJECT}", 0, 1, 1,
		0);
	cPattern::create(u"_COMMAND1{_FINAL:_ONLY_BEGIN_MATCH}", u"6",
		2, u"_ADVERB", u"_NAME", 0, 0, 1, // pray / please / Yes (ADVERB) / Tommy
		1, u",", 0, 0, 1,
		1, u"_SUBCOM", 0, 0, 1,
		2, u"_THINKPRESENTFIRST{VERB}", u"_THINKPRESENT_CO", 0, 1, 1,
		// removed OBJECT from _NOUN as this will cause IS to have two objects (including the one from _S1) which is wrong and will cause elimination of this pattern
		3, u"_NOUN_OBJ", u"__NOUN[*]", u"__MNOUN[*]", 0, 0, 1, // Tell me you were with monsieur Poirot.
		1, u"__S1[*]{_BLOCK:OBJECT:EVAL}", 0, 1, 1,
		0);
	return 0;
}

// Additional relative / comparative patterns (_REL1 variants, "more than S")
// that had to wait until __S1 and _PP were complete.  Returns 0.
int createSecondaryPatterns_REL(void)
{
	LFS

	//cPattern::create(u"__NOUN", u"H",
	//	1, u"_ADVERB", 0, 0, 1, // much more / ever more
	//	2, u"adverb|more", u"adverb|less", 0,1,1,
	//	2, u"_PP", u"_ADVERB",0, 0, 1, // ever more *out of life* than .. / more *unreservedly* than
	//	1, u"preposition|than",0,1,1,
	//	3, u"__S1[*]{_BLOCK:OBJECT:EVAL}", // he gets alot more out of life than *I do* .
	//	   u"__NOUN[*]", // She had expected more than a phrase.
	//		 u"_PP",0, 1, 1, // I care no more for her fortune than *for the ashes in that grate*. 
	//	0);
	// that one might believe every day to be Sunday
	// that such was not the case.
	// char *relativizer[] = {u"who",u"which",u"whom",u"whose",u"where",u"when",u"why",NULL};
	// pattern is not grouped with its other root patterns
	// this cost should be kept higher than the cost for a preposition at a postential end for __S1, like for _VERBPRESENT.
	// whoever he was
	// what he did do
	// she went to London, where she entered a children's hospital.
	// Do NOT add REL tag here - the REL tag is only for relative phrases that be attached to objects
	// REMOVED: as and __AS_AS - they are not relativizers! and cause overlap with __PP[A].  'as' is a subordinator
	// REMOVED __AS_AS = *As* soon as he saw that his companion was afraid of the dark (from ST comparison - ) -the second 'as' is the relativizer
	cPattern::create(u"_REL1{_FINAL_IF_ALONE:S_IN_REL}", u"2",//3,u"demonstrative_determiner",u"personal_pronoun_nominative",u"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
		2, u"_ADJECTIVE", u"_ADVERB", 0, 0, 1,
		// when matching as and __AS_AS, this REL1 is a subordinating clause, not a relative clause
		3, u"relativizer", u"interrogative_determiner", u"demonstrative_determiner|that", 0, 1, 1,
		2, u"_PP", u"__INTERS1{BLOCK}", 0, 0, 1,  // He knew that *after sunset* almost all the birds were asleep.
		4, u"_ADVERB", u"conjunction|though*-3", u"quantifier|all*-2", u"__INTERPP", 0, 0, 1, // where simply every one is bound to turn up sooner or later // though is not used as a conjunction often, but should be one in this position
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"preposition*4", 0, 0, 1, // that you are afraid 'of'// preposition use should be rare!
		0);
	// how fast he can give Tom books
	// how giving the author says he is
	// Do NOT add REL tag here - the REL tag is only for relative phrases that be attached to objects
	cPattern::create(u"_REL1{_FINAL_IF_ALONE:S_IN_REL}", u"3",//3,u"demonstrative_determiner",u"personal_pronoun_nominative",u"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
		1, u"how", 0, 1, 1,
		1, u"_ADJECTIVE", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		0);
	// which people he can influence the most
	// what things he boasts about
	// whose people they can boss about every day
	// Do NOT add REL tag here - the REL tag is only for relative phrases that be attached to objects
	cPattern::create(u"_REL1{_FINAL_IF_ALONE:S_IN_REL}", u"4",//3,u"demonstrative_determiner",u"personal_pronoun_nominative",u"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
		3, u"which", u"what", u"whose", 0, 1, 1,
		2, u"__NOUN[*]{_BLOCK}", u"__MNOUN[*]{_BLOCK}", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		0);
	// He thought *that* if he could only keep his dreadful companion TALKING , perhaps she would forget about FLYING -- and knocking him down . 
	// Benjamin Bat was afraid *that* if he touched Freddie Firefly he would get burned . 
	// She had thought *that* an angel might step between a soul on earth and sin , and *that* if one but prayed and prayed , the dear Lord would stand between and deliver the tempted . 
	// I should quite have given up hope had not the guides persisted *that* if you had got off the glacier you might have taken shelter somewhere under the lee of a rock , and *that* if so we might find you unharmed . 
	// Mr . Singleton made no reply , and mentally resolved *that* if it were necessary he would speak about it , whether or no . 
	// “ Yes , I am almost sure *that* if it had not been for the affair of the diamonds she would have done so any time during *that* last fortnight at Chamounix ; 
	// He told me before he started *that* if he found they had gone out there , he would follow , however long it might take . 
	// It has been an agreed matter in this ” ere camp , *that* girl is not to be interfered with by no one , and *that* if any one cuts in , in a way *that* ain't fair and right , it should be bad for him . 
	// “ This is to give you notice *that* if you are found in this camp after sunset to-night you do so at your peril . 
	// There was still a good deal of play going on , for cards formed one of the few amusements of the miners , but as long as they played against each other , the stakes were comparatively moderate , and men were pretty sure *that* if they lost they , at least , lost fairly . 
	// I am sure *that* if you will not go home with me he will himself come out to fetch you . 
	// and *that* if Dorothy is willing to take you , you will meet with no objection on the part of her father . 
	// She went out of her way to explain to people *that* if they noticed a change in Betsy Butterfly's appearance , they might thank her for it ... . 
	// “ And I warn you *that* if you so much as touch my lovely cousin with *that* brush you shall have every one of us fellows in your hair . 
	// “ She was afraid *that* if I knew she ate butter she would have to share it with me ... . 
	// I can tell you right now *that* if I had any idea she looked like this I never would have lost my appetite over her ! 
	// You remember *that* you said *that* if I wouldhad show you a picture of Betsy Butterfly you would stop pestering me about her . 
	// Old Mr . Crow often remarked *that* if Grumpy Weasel really wanted to be of some use in the world he would spend his time at the sawmill filling knot holes in boards . 
	// And now he insisted *that* if he “ went first ” he ought to be allowed to choose whatever hole he pleased . 
	// But everybody else complained about the noise *that* Turkey Proudfoot made , and said *that* if he must gobble they wished he would go off by himself , where people didn't have to listen to him . 
	// “ but can't you see *that* if I let them out now I will lose them ? 
	// I used to feel *that* if any of the other things looked too fierce I could always hop into *that* chair and be safe . 
	// We believe , for instance , *that* if Gutenberg had not invented movable types , somebody else would have given them to the world about *that* time . 
	// Now , it seems to me *that* if Mr . Ruskin could realize in some isolated nation this idea of a pastoral , simple existence , under a paternal government , he would have in time an ignorant , stupid , brutal community in a great deal worse case than the agricultural laborers of England are at present . 
	// There was a possibility *that* if the scientists moved him , his new tank would be shielded so *that* it would be impossible to enjoy himself as he now was . 
	// “ I can see *that* if she had , maybe a hundred dollars , say -- of her own , unexpected like -- when she left the hospital -- I can just see the things she would do with it ! 
	// and tell sister Ann , *that* if she can write as well as you tell of , I wish she would write me a letter . 
	cPattern::create(u"_REL1{_FINAL_IF_ALONE:S_IN_REL}", u"6",
		2, u"_ADJECTIVE", u"_ADVERB", 0, 0, 1,
		2, u"demonstrative_determiner|that", u"preposition|for", 0, 0, 1, // for when Mama returned she told them she had heard from Mrs . Clifford , who wrote she had that day sent off a box . 
		10, u"conjunction|if", u"conjunction|unless", u"conjunction|whether", u"relativizer|when", u"conjunction|before", u"conjunction|after", u"conjunction|since", u"conjunction|until", u"conjunction|while", u"relativizer|whenever", 0, 1, 1,
		1, u"__INTERS1{BLOCK}", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		4, u"adverb|then", u"_ADVERB", u"conjunction|though*-3", u"quantifier|all*-2", 0, 0, 1, // though is not used as a conjunction often, but should be one in this position
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		0);
	return 0;
}

// More __PP shapes (prep + _REL1, doubled prepositions) and further _MS1
// coordinations that need _REL1/_PP finalized.  Returns 0.
int createSecondaryPatterns_MS1_1(void)
{
	LFS

	cPattern::create(u"__PP", u"A",
		1, u"_ADVERB*1", 0, 0, 1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
		2, u"preposition{P}", u"verbalPreposition{P}", 0, 1, 1,
		1, u"preposition*1{P}", 0, 0, 1, // from within
		1, u"_ADVERB*3", 0, 0, 1, // I haven't seen you for SIMPLY centuries, my dear. // adverbial use should be rare - prefer adjectives attached to the nouns over adverbs.
		1, u"_REL1[*]{PREPOBJECT}", 0, 1, 1,   // my purpose IN (prep) what follows (_REL) (is to try to place him) A6U .
		0);
	cPattern::create(u"__PP", u"B",
		1, u"_ADVERB", 0, 0, 1,
		1, u"preposition|to{P}", 0, 1, 1,
		7, u"noun|lunch*-2{PREPOBJECT}", u"noun|breakfast*-2{PREPOBJECT}", u"noun|bed*-2{PREPOBJECT}", u"noun|supper*-2{PREPOBJECT}", u"noun|date*-2{PREPOBJECT}", u"noun|jail*-2{PREPOBJECT}", u"noun|reason*-2{PREPOBJECT}", 0, 1, 1,
		0);
	// this was cut because it is too ambiguous and allowed too many non-coherent parses
		// moved to __C1_IP
	// restatement / this woman, whoever she was, was saved
	// this woman, master of disguise,
	//cPattern::create(u"__NOUN{_FINAL_IF_ALONE}",u"",
	//                 1,u"__NOUN[*]",0,1,1,
	//                 1,u",",0,1,1,
	//                 2,u"__NOUN[*]{_BLOCK:RE_OBJECT}",u"_REL1[*]{_BLOCK}",0,1,1,
	//                 1,u",",0,1,1, // period deleted because of the '.' at end - swallows up the period
	//                  0);

	//cPattern::create(u"__S1",u"7",1,u"relativizer",0,1,1, // put into Q1[C]
	//                 1,u"not",0,1,1,0);
	// If you passed that way *any* time between dawn and sunset you could see me hanging by my heels from one of the branches . 
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"1",
		1, u"_STEP", 0, 0, 1,
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"_ADVERB", 0, 0, 1, // even if 
		13, u"noun|once", u"conjunction|although", u"conjunction|though", u"conjunction|if", u"relativizer|when", u"conjunction|before", u"conjunction|after", u"conjunction|because", u"conjunction|since", u"conjunction|until", u"conjunction|while", u"relativizer|whenever", u"conjunction|as", 0, 1, 1,
		2, u"__S1{_BLOCK:EVAL}", u"__NOUN*4", 0, 1, 1,  // __NOUN: when *a child* he suffered punishment rather than give up her society . 
		2, u"_ADVERB*1", u"_ADJECTIVE_AFTER*1", 0, 0, 1, // any time between dawn and sunset / I wish to see him *alive*
		1, u",", 0, 0, 1,
		1, u"adverb|then", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);
	cPattern::create(u"_MS1{_FINAL}", u"M",
		1, u"_STEP", 0, 0, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"adverb", u"adjective", ADVERB_COMPARATIVE | ADJECTIVE_COMPARATIVE, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"adverb|then", 0, 0, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"adverb", u"adjective", ADVERB_COMPARATIVE | ADJECTIVE_COMPARATIVE, 1, 1,
		2, u"__S1{_BLOCK:EVAL}", u"__NOUN*1", 0, 0, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);
	//cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"XX",
	//	1, u"_STEP", 0, 0, 1,
	//	1, u"_INTRO_S1", 0, 0, 1,
	//	1, u"_ADVERB", 0, 0, 1, // even if 
	//	1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
	//	2, u"_ADVERB*1", u"_ADJECTIVE_AFTER*1", 0, 0, 1, // any time between dawn and sunset / I wish to see him *alive*
	//	1, u",", 0, 0, 1,
	//	1, u"adverb|then", 0, 0, 1,
	//	1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
	//	1, u"_MSTAIL", 0, 0, 1,
	//	0);
	/* matches MS1[1]
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}",u"5",
											 1,u"_INTRO_S1",0,0,1,
											 1,u"conjunction|if",0,1,1,
											 1,u"__S1{_BLOCK:EVAL}",0,1,1,
											 1,u",",0,0,1,
											 1,u"adverb|then",0,0,1,
											 1,u"__S1{_BLOCK:EVAL}",0,1,1,
											 1,u"_MSTAIL",0,1,1,
											 0);
											 */
	return 0;
}

// _MSTAIL / __MSTAIL: the ", and then S" / "; so S" tail of a multi-clause
// sentence.  NO_MIDDLE_MATCH+_BLOCK+EVAL so a tail cannot win on its own
// and its internal S is costed onto the parent _MS1.  Returns 0.
int createSecondaryPatterns__MSTAIL(void)
{
	LFS
		cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}", u"1",
		1, u",", 0, 0, 1,
		1, u"_ADVERB", 0, 0, 1,
		4, u"adverb|then", u"adverb|so", u"conjunction", u"coordinator", 0, 1, 1,
		2, u"adverb|then", u"conjunction|if", 0, 0, 1, // or if
		3, u"_PP*2", u"_VERBREL2*2", u"_ADVERB", 0, 0, 1,  // Tommy did not hear Boris's reply , but [in response to it] Whittington said something that sounded like - *1 competition with __MSTAIL[9]
// NOUN*3 - helps with hanging nouns prevents 'eyes' used as a verb because _MS uses _MSTAIL to cover more even though it is more expensive
3, u"__S1{_BLOCK:EVAL}", u"_REL1[*]", u"__NOUN*3", 0, 1, 1, // __NOUN must be expensive to avoid absorbing the subject of the second sentence following another sentence (and a conjunction)
0);
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}", u"A",
		1, u",", 0, 1, 1,
		1, u"_ADVERB", 0, 1, 1,
		0);
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}", u"2",
		1, u"_ADVERB", 0, 0, 1,
		1, u",", 0, 0, 1,
		5, u"conjunction|but", u"adverb|then", u"coordinator|and", u"adverb|rather", // assumption: 'rather than' will exist independently of the other words in the same lines
		u"quantifier|more", 0, 1, 1,  // either of the captives could do *more than* give utterance to moans .
		2, u"adverb|then", u"preposition|than", 0, 0, 1,
		5, u"__ALLVERB{VERB2}", u"_COND{VERB2}", u"_VERBPASTPART*1{vB:VERB2}", u"_BEEN{vB:id:VERB2}", u"_VERBPASSIVE{VERB2}", 0, 1, 1,
		3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
		1, u"__CLOSING__S1", 0, 0, 3,
		0);
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}", u"T",
		1, u"_ADVERB", 0, 0, 1,
		1, u",", 0, 0, 1,
		4, u"conjunction|but", u"adverb|then", u"coordinator|and", u"adverb|rather", 0, 1, 1,  // assumption: 'rather than' will exist independently of the other words in the same lines
		2, u"adverb|then", u"preposition|than", 0, 0, 1,
		1, u"__ALLTHINK{VERB2}", 0, 1, 1,
		1, u"__S1{OBJECT:EVAL:_BLOCK}", 0, 1, 1,
		3, u"__INFP5SUB", u"__INFPSUB", u"__INFPT2SUB", 0, 0, 1,
		0);
	// Like all the darned lot of them, he wasn't going to commit himself till he was sure he could deliver the goods.
	// he meant well , but it was terrible he did it . 
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}", u"3",
		1, u",", 0, 0, 1,
		4, u"adverb|then", u"adverb|so", u"conjunction", u"coordinator", 0, 1, 1,
		1, u"adverb|then", 0, 0, 1,
		1, u"__C1__S1", 0, 1, 1,
		4, u"_IS{VERB:vS:id}", u"_WOULDBE", u"_HAVEBEEN", u"_COULDHAVEBEEN", 0, 1, 1,
		// removed OBJECT from _NOUN as this will cause IS to have two objects (including the one from _S1) which is wrong and will cause elimination of this pattern
		3, u"_ADJECTIVE", u"__NOUN[*]*1", u"_NOUN_OBJ*1", 0, 0, 1, // make NOUN more expensive because this is redundant with _NOUN[5]
		1, u"__S1[*]*2{_BLOCK:OBJECT:EVAL}", 0, 1, 1, // very rare             // and in general _NOUN[5] is correct when this is a NOUN
		0);
	// and happy like you.
	// but with extreme graciousness
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL}", u"4",
		1, u",", 0, 0, 1,
		2, u"conjunction", u"coordinator", 0, 1, 1,
		2, u"_ADJECTIVE", u"_ADVERB", 0, 0, 1,
		4, u"_PP*1", u"_VERBREL2*1{_BLOCK:EVAL}", u"_VERBREL1*1{_BLOCK:EVAL}", u"_INFP*1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		0);
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL:_ONLY_END_MATCH}", u"F",
		1, u",", 0, 0, 1,
		1, u"preposition|for", 0, 1, 1,
		1, u"conjunction|though", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		0);
	// to put it mildly.
	// _FINAL: ’ said Grace , snapping *her* finger resignedly . 
	cPattern::create(u"__MSTAIL{_FINAL:NO_MIDDLE_MATCH:_BLOCK:EVAL:_ONLY_END_MATCH}", u"G",
		1, u",", 0, 1, 1,
		2, u"conjunction", u"coordinator", 0, 0, 1,
		2, u"_ADVERB", u"_ADJECTIVE", 0, 0, 1, // he drove the car , *unable* to contain his blooper .
		3, u"_PP", u"_VERBREL2", u"_INFP", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		0);
	// as far as I am concerned.
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL:_ONLY_END_MATCH}", u"H",
		1, u",", 0, 0, 1,
		1, u"__AS_AS", 0, 1, 1,
		1, u"__S1", 0, 1, 1,
		0);
	// Joan could think of no suitable reply for this and they sat in silence , *the woman studying her face intently* .
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL:_ONLY_END_MATCH}", u"J",
		1, u",", 0, 1, 1,
		1, u"__NOUN", 0, 1, 1, // the woman
		1, u"verb", VERB_PRESENT_PARTICIPLE, 1, 1, // studying
		2, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", 0, 1, 1, // her face 
		1, u"_ADVERB", 0, 0, 1,
		0);
	// That ishas forty gold sovereigns , as doesn't belong to me , *nor father neither* , *but* to one of his mates as left it with him for safety . - but should not be an adverb!
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL:_ONLY_END_MATCH}", u"K",
		1, u",", 0, 0, 1,
		1, u"coordinator|nor", 0, 1, 1,
		1, u"noun", 0, 1, 1,
		1, u"adverb|neither", 0, 1, 1,
		0);
	cPattern::create(u"__MSTAIL{NO_MIDDLE_MATCH:_BLOCK:EVAL:_ONLY_END_MATCH}", u"u",
		1, u",", 0, 1, 1,
		1, u"personal_pronoun", 0, 1, 1,
		1, u"verb|said", 0, 1, 1,
		0);
	// prevents multiplicative nesting in _MS1
	cPattern::create(u"_MSTAIL{_NO_REPEAT}", u"", 1, u"__MSTAIL[*]", 0, 1, 4, 0);
	return 0;
}

// Remaining top-level _MS1/_MS2/_MTS1/_HAIL wrappers: comparatives
// ("the more S the less S"), "not only ... but also", "at the time S, S",
// vocative _HAIL.  Most are _FINAL_IF_ALONE + _ONLY_BEGIN_MATCH.  Returns 0.
int createSecondaryPatterns_MS1_2(void)
{
	LFS
		cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"3",
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"adverb|more", 0, 1, 1,
		1, u"adverb*1", 0, 0, 1, // he will hate us *more heartily than ever now *. 
		1, u"preposition|than", 0, 1, 1,
		5, u"__S1{_BLOCK:EVAL}", u"__NOUN", u"__MNOUN", u"_ADVERB", u"__VERBALPHRASENOUN", 0, 1, 1, // _ADVERB=*ever now*
		0);
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"4",
		1, u"_INTRO_S1", 0, 0, 1,
		5, u"__S1{_BLOCK:EVAL}", u"_COMMAND1{_BLOCK:EVAL}", u"_RELQ{_BLOCK:EVAL}", u"_REL1{_BLOCK:EVAL}", u"__SQ{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_MSTAIL", 0, 1, 1,
		0);
	cPattern::create(u"_MTS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"5",
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		3, u"conjunction|but", u"adverb|then", u"coordinator|and", 0, 1, 1,
		1, u"adverb|then", 0, 0, 1,
		1, u"__ALLTHINK", 0, 1, 1, // u"think" for all active tenses! - possibly add INTRO_S1.
		2, u"_PP", u"_REL1[*]", 0, 0, 1,
		2, u"_INFP{OBJECT}", u"__S1{_BLOCK:OBJECT:EVAL}", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
		0);
	// not only... but also...
	// for not only was he born in that city, but his family had been resident there for centuries.
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"6",
		1, u"not", 0, 1, 1,
		1, u"adverb|only", 0, 1, 1,
		1, u"_Q1PASSIVE{_BLOCK}", 0, 0, 1,
		1, u"_PP", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"conjunction|but", 0, 1, 1,
		1, u"adverb|also", 0, 0, 1,
		2, u"_PP", u"__S1{_BLOCK:EVAL}", 0, 1, 1, 0);
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"9",
		1, u"_STEP", 0, 0, 1,
		1, u"_INTRO_S1", 0, 1, 1,
		1, u"conjunction|if", 0, 0, 1,
		// REL1 - Oh , what fun *this* is !
		2, u"__S1[*]{_BLOCK:EVAL}", u"_REL1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		1, u"_REF", 0, 0, 1,
		0);
	// this was introduced to prevent Q1, VERBREL and so forth from having a NOUN subject
	// hidden by INTRO_S1 that was actually a subject.  As well as being incorrect, this situation 
	// would have removed the cost of VERBAFTERVERB (if the NOUN includes a verb), which would 
	// make it potentially even more inaccurate.
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"F",
		1, u"_STEP", 0, 0, 1,
		1, u"_INTRO_S1", 0, 1, 1,
		1, u"conjunction|if", 0, 0, 1,
		4, u"_Q1*1{_BLOCK:EVAL}", u"_COMMAND1{_BLOCK:EVAL}", u"__SQ{_BLOCK:EVAL}", u"politeness_discourse_marker*-1", 0, 1, 1, // Please, Jake, please 
		1, u"__CLOSING__S1", 0, 0, 3,
		1, u"_REF", 0, 0, 1,
		0); // In any case, what do you lose? (_Q1)
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"A",
		1, u"_STEP", 0, 1, 1,
		1, u"conjunction|if", 0, 0, 1,
		3, u"_Q1*1{_BLOCK:EVAL}", u"_VERBREL1", u"_COMMAND1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		1, u"_REF", 0, 0, 1,
		1, u"?*-1", 0, 0, 1,
		0); // (a) - slide the block over.
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"E",
		1, u"_STEP", 0, 1, 1,
		1, u"conjunction|if", 0, 0, 1,
		1, u"__S1[*]{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_REF", 0, 0, 1,
		1, u"?*-1", 0, 0, 1,
		0); // (a) - slide the block over.
// what he seeks to attain we do not know.
// When he thinks he has given an extraordinarily clever impersonation he shakes with laughter.
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"C",
		//3,u"demonstrative_determiner",u"personal_pronoun_nominative",u"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
		1, u"_INTRO_S1{_BLOCK:EVAL}", 0, 0, 1,
		12, u"__INTRO_N_ET", u"which", u"what", u"whose", u"relativizer|when", u"conjunction|before", u"conjunction|after", u"conjunction|since", u"conjunction|until", u"conjunction|while", u"preposition|during", u"__AS_AS*-1", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		2, u"adverb|then", u"_ADVERB", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);
	// At the time I made **the promise I thought** his request was not fair to you and was unwise
	// could this not be eliminated with use of the _ADVERB[T] patterns?
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"D",//3,u"demonstrative_determiner",u"personal_pronoun_nominative",u"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
		3, u"preposition|at", u"preposition|in", u"preposition|on", 0, 1, 1, // at the moment / in the July / on the Wednesday 
		1, u"determiner|the{TIMEMODIFIER:DET}", 0, 1, 1,
		1, u"_ADJECTIVE{TIMEMODIFIER:_BLOCK}", 0, 0, 1,
		5, u"month{MONTH:N_AGREE}", u"daysOfWeek{DAYWEEK:N_AGREE}", u"dayUnit{TIMECAPACITY:N_AGREE}", u"uncertainDurationUnit{TIMECAPACITY:N_AGREE}", u"simultaneousUnit{TIMECAPACITY:N_AGREE}", 0, 1, 1, // A moment
		1, u"_ADVERB", 0, 0, 1, // later
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,  // I made the promise
		1, u",", 0, 0, 1,
		2, u"adverb|then", u"_ADVERB", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1, // I thought his request was not fair to you.
		1, u"_MSTAIL", 0, 0, 1,
		0);
	// Morrow, W. C.\The Inmate Of The Dungeon 1894[1323-1329]:
	// In asking you to make a statement I am merely asking for your help to right a wrong
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"G",
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"_ADVERB", 0, 0, 1, // even if 
		1, u"preposition|in", 0, 1, 1,
		5, u"verb{vS:V_AGREE:V_OBJECT}", u"does{vS:V_AGREE:V_OBJECT}", u"does_negation{vS:not:V_AGREE:V_OBJECT}",
		u"have{vS:V_AGREE:V_OBJECT}", u"have_negation{vS:not:V_AGREE:V_OBJECT}", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"__NOUN{OBJECT}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);
	//  The more I know him the less I like him.
	//  The more people you suspected the more *I* could work .
	//	The more *I* know him, the less I like him .
	//	The more *I* strove to control myself -- the more I told myself that what such a creature as Hockley might say could not matter -- the more my passion grew .
	//	The more I strove to control myself -- the more *I* told myself that what such a creature as Hockley might say could not matter -- the more my passion grew .
	//	The more I think of that episode in Von Brent's office , the more *I* think you utterly failed to realize the dramatic possibilities of the situation . 
	//	The more incapable of it I feel myself, the more *I* believe it to be possible .
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"N",
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"adverb|more", u"adverb|less", 0, 1, 1,
		1, u"__NOUN*2", 0, 0, 1, //  The more people you suspected the more *I* could work .
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"determiner|the", 0, 1, 1,
		2, u"adverb|more", u"adverb|less", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);
	// I *want* to live here in the woods , where there is not so much deceit and treachery as there seems to be in the big towns .
	cPattern::create(u"_MS1{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"8",
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 1, 1,
		1, u"_REL1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);


	// I had undertaken the task , and when I once undertake **a thing I always carry** it through if I can . / preventing __NOUN[R]
	cPattern::create(u"_MS2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"1",//3,u"demonstrative_determiner",u"personal_pronoun_nominative",u"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"coordinator|and", 0, 1, 1,
		7, u"relativizer|when", u"conjunction|before", u"conjunction|after", u"conjunction|since", u"conjunction|until", u"conjunction|while", u"preposition|during", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 0, 1,
		3, u"adverb|then", u"_ADVERB", u"conjunction|though*-3", 0, 0, 1, // though is not used as a conjunction often, but should be one in this position
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"preposition*2", 0, 0, 1,
		2, u"conjunction|if", u"conjunction|as", 0, 1, 1,
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);
	// Yes, I understand, I *guess*.
	cPattern::create(u"_MS2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_ONLY_END_MATCH}", u"2",
		1, u"_STEP", 0, 0, 1,
		1, u"_INTRO_S1", 0, 1, 1,
		1, u"__S1[*]{_BLOCK:EVAL}", 0, 1, 1,
		1, u",", 0, 1, 1,
		3, u"adverb|then", u"_ADVERB", u"conjunction|though*-3", 0, 0, 1, // though is not used as a conjunction often, but should be one in this position
		1, u"__S1{_BLOCK:EVAL}", 0, 1, 1,
		1, u"__CLOSING__S1", 0, 0, 3,
		1, u"_REF", 0, 0, 1,
		0);
	// I said please don't hurt me.
	cPattern::create(u"_MS2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH}", u"3",//3,u"demonstrative_determiner",u"personal_pronoun_nominative",u"indefinite_pronoun",0,0,1, // *he* whom you await taken out - how is this different than _NOUN[9]?
		1, u"_INTRO_S1", 0, 0, 1,
		1, u"__C1__S1", 0, 1, 1,
		1, u"verb|said", 0, 1, 1,
		2, u"__S1{_BLOCK:EVAL}", u"_VERBREL1", 0, 1, 1,
		1, u"_MSTAIL", 0, 0, 1,
		0);
	cPattern::create(u"_HAIL{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"",
		4, u"_NAME{HAIL}", u"honorific{HAIL:HON}", u"_HON_ABB{HAIL:HON}", u"_META_GROUP{HAIL}", 0, 1, 1, // , sir / , freak! noun includes _NAME, u"honorific",
		1, u",", 0, 1, 1,
		1, u"__NOUN[*]", 0, 1, 1,
		0);
		return 0;
	}

	// Residual top-level fragments: __MODAUX ("He might."), __IMPLIEDIS
	// ("especially him."), __IMPLIEDVERBCOMMAND, _PLEA ("yes, please"),
	// _SECTIONHEADER.  All _STRICT_NO_MIDDLE_MATCH so they cannot steal a
	// span from a real __S1.  Returns 0.
	int createSecondaryPatterns_OTHER_VERB(void)
	{
		LFS

	//cPattern::create(u"_MS1{_FINAL:_ONLY_BEGIN_MATCH}",u"C",
	//                 1,u"_ADJECTIVE",0,1,1, // afraid I don't.
	//                 1,u"__S1[*]",0,1,1,
	//                 0); // In any case, what do you lose? (_Q1)
	// prevent NOUN[B] from matching a quoted sentence (because NOUN[B] will be the only final pattern matching everything including the quotes)
	//cPattern::create(u"_MS1{_FINAL}",u"B",
	//                1,u"quotes",OPEN_INFLECTION,1,1,
	//                1,u"__S1[*]",0,1,1,
	//                2,u",",u".",0,0,1,
	//                1,u"quotes",CLOSE_INFLECTION,1,1,0);
	// , might, / , could, / He might.
	// Beresford speaking (Agatha Christie)
	cPattern::create(u"__MODAUX{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"1",
		1, u"__NOUN", 0, 0, 1,
		1, u"_COND", 0, 1, 1, 0);
	cPattern::create(u"__MODAUX{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"2",
		1, u"__NOUN", 0, 1, 1,
		1, u"_VERBONGOING{vE:VERB}", 0, 1, 1, 0);
	// especially him.  especially this case.
	// you too? // me especially? / those?
	cPattern::create(u"__IMPLIEDIS{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"1",
		1, u"conjunction", 0, 0, 1, // and especially this case / but never mind.
		1, u"_ADVERB", 0, 1, 1,
		2, u"_NOUN_OBJ", u"__NOUN", 0, 1, 1,
		1, u"_ADVERB", 0, 0, 1,
		1, u"preposition", 0, 0, 1, // Rather a small blooper to begin the world *with* .
		0);
	// been empty for years. / been emptying for years
	cPattern::create(u"__IMPLIEDIS{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"3",
		1, u"_BEEN", 0, 1, 1,
		2, u"_ADJECTIVE", u"verb", VERB_PRESENT_PARTICIPLE, 1, 1,
		1, u"_PP", 0, 1, 1,
		0);
	// yes, little lady, out with it.
	// this pattern creates too many problems and doesn't even match the original sentence it was created for.
	cPattern::create(u"__IMPLIEDVERBCOMMAND{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"",
		1, u"interjection", 0, 1, 1,
		1, u",", 0, 1, 1,
		1, u"__NOUN{HAIL}", 0, 1, 1,
		1, u",", 0, 1, 1,
		1, u"preposition|out", 0, 1, 1,
		1, u"_PP", 0, 1, 1,
		0);
	// willing to do anything, go anywhere. (Secret Adversary)
	//cPattern::create(u"_PLEA{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"1",
	//												1, u"_VERBONGOING{VERB:vE}", 0, 1, 1,
	//												3, u"__ALLOBJECTS_0", u"__ALLOBJECTS_1", u"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
	//												1, u"__CLOSING__S1", 0, 0, 3,
	//												0);

	// yes, please.  Oh, please.  No, No, please!
	// No, please, sir
	cPattern::create(u"_PLEA{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH}", u"2",
		1, u"interjection*-4", 0, 1, 1, // this counters __INTRO_S1
		1, u",", 0, 1, 1,
		1, u"interjection*-4", 0, 0, 1,
		1, u",", 0, 0, 1,
		1, u"politeness_discourse_marker*-1", 0, 1, 1,
		1, u",", 0, 0, 1,
		4, u"_HAIL_OBJECT", u"honorific{HON:HAIL}", u"noun{HAIL}", u"adjective|dear", MALE_GENDER | FEMALE_GENDER, 0, 1,
		0);
	cPattern::create(u"_SECTIONHEADER{_FINAL:_ONLY_BEGIN_MATCH}", u"",
		1, u"sectionheader*-10", 0, 1, 1,
		3, u"numeral_cardinal", u"roman_numeral", u"Number", 0, 1, 1,
		0);
	return 0;
}

	// Dispatcher for every createSecondaryPatterns_* above, in dependency
	// order (C1_IP → C1__S1 → CLOSING → S1 → NOUN → VERB → MS1 → INTRO →
	// S2 → COMMAND → REL → MSTAIL → OTHER).  Called last from
	// initializePatterns so _MS1 can mention every subpattern.  Returns 0.
	int createSecondaryPatterns2(void)
	{
		createSecondaryPatterns__C1_IP();
		createSecondaryPatterns__C1__S1();
		createSecondaryPatterns__CLOSING__S1();
		createSecondaryPatterns__S1();
		createSecondaryPatterns__NOUN();
		createSecondaryPatterns__VERB();
		createSecondaryPatterns_MS1();
		createSecondaryPatterns__INTRO_N();
		createSecondaryPatterns__INTRO_S1();
		createSecondaryPatterns__S2();
		createSecondaryPatterns_COMMAND();
		createSecondaryPatterns_REL();
		createSecondaryPatterns_MS1_1();
		createSecondaryPatterns__MSTAIL();
		createSecondaryPatterns_MS1_2();
		createSecondaryPatterns_OTHER_VERB();
		return 0;
	}