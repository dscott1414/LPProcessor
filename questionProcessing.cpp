#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <set>

#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <winsock.h>
#include "Winhttp.h"
using namespace std;
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "time.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "vcXML.h"
#include "profile.h"

void defineNames(void);
void defineTimePatterns(void);
void createMetaNameEquivalencePatterns(void);
extern int logDetail;

void createQuestionPatterns(void)
{ LFS
	// When did Brown start working at FEMA?
	// the following is a copy of NOUN[D], and __ALLOBJECTS_1 includes it.  But in a question, the relativizer is already 
	// included as an object of the verb, so this would lead to the main verb having two objects, which is so expensive that it will never match.
	// this pattern will be included as an alternative to __ALLOBJECTS_1, and not as an OBJECT.
	cPattern::create(L"__QNOUN{_FINAL_IF_ALONE:_FORWARD_REFERENCE:_BLOCK:GNOUN:VNOUN}",L"D",
									 1,L"_VERBONGOING*1{VERB:vE}",0,1,1,  // from C2__S1 - also matches _N1// this pattern should not be common
									 // if the following is made optional this pattern can match _NOUN[9] with an embedded _NOUN[2]
									 3,L"__ALLOBJECTS_0",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,1,1, // there must only be one adjective and it must be last (not mixed in) see *
									 0);
	// moved to _VERBREL1
	//cPattern::create(L"_Q1",1,L"_ADVERB",0,0,1,
	//                6,L"does",L"does_negation",L"have",L"have_negation{:not}",L"is",L"is_negation{:not}",
	//                  VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_SECOND_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL|VERB_PAST,1,1,
	//                2,L"__NOUN",L"there",0,1,1,
	//                1,L"not",0,0,1,
	//                0);
	cPattern::create(L"_Q1{VERB}",L"H",
									1,L"_HAVE",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									2,L"_ADVERB",L"preposition*2",0,0,1,0);  // preposition use should be rare!
		// been in prison? // been there?
	cPattern::create(L"_Q1",L"1",
									1,L"_ADVERB",0,0,1,
									1,L"_BEEN{VERB:id:past}",0,1,1,
									2,L"there",L"_PP",0,0,1, // removed L"_NOUN_OBJ" - included in later _Q1 is it really you?
									1,L"_ADVERB",0,0,1,
									0);
		/* covered by _Q2[F], also 'that' is no longer a relativizer 4/10/2008
	// would that strike you?
	cPattern::create(L"_Q3{_FINAL_IF_ALONE}",L"1",
									1,L"_COND[*]",0,1,1,
									1,L"_REL1[*]",0,1,1,
									0);
	*/
		// would/will I have gone? | would/will I definitely have gone?
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1{VERB}",L"A",
									1,L"_COND",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_HAVE",0,1,1,
									2,L"_VERBPASTPART{vAB:V_OBJECT}",L"_BEEN{vAB:id}",0,1,1,
									0);

		// would you? / will there not?
	// will I?  would I? wouldn't I?
	cPattern::create(L"_Q1S{_FINAL_IF_ALONE}",L"2",
									1,L"_COND",0,1,1,
									3,L"_NOUN_OBJ{SUBJECT}",L"__NOUN[*]{SUBJECT}",L"__NOUNREL{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,1,
									0);
		// would you go? / will there not be?
	cPattern::create(L"_Q1{VERB}",L"3",1,L"_Q1S",0,1,1,
									2,L"_VERBPRESENT",L"_BE{vS:V_OBJECT:id}",0,1,1,0);
	// are you? // aren't you covered by a combination of 
	// also covered by _Q2"F" __ALLOBJECTS, except __ALLOBJECTS are objects, and this is a subject (which is correct)
	cPattern::create(L"_Q1",L"4", 
									1,L"_IS{VERB:vS:id}",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1, 
									1,L"__ALLOBJECTS_1",0,0,1, // is he president? / now covered in _Q2[J] because allowing _Q1[4] to have its own object will
									// allow the verb in a _Q2 to have two effective objects
									0);
	// would you be running?
	cPattern::create(L"_Q1{VERB}",L"5",
									1,L"_Q1S",0,1,1,
									1,L"_BE{_BLOCK}",0,1,1,
									1,L"_VERBONGOING{vAC}",0,1,1,0);
	// would you be thinking I would run away?
	cPattern::create(L"_QT1{_FINAL_IF_ALONE:VERB}",L"5",
									1,L"_Q1S",0,1,1,
									1,L"_BE{_BLOCK}",0,1,1,
									1,L"_THINKONGOING{vAC}",0,1,1,
									3,L"_ADVERB",L"preposition*2",L"_PP*1{_BLOCK}",0,0,2,
									1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									0);
	// wouldn't I go? / would I go? / let us be adventurers!
	//cPattern::create(L"_Q1{_FINAL_IF_ALONE:VERB}",L"6",
	//                1,L"_COND2",0,1,1,
	//                1,L"__NOUN[*]{SUBJECT}",0,1,1,
	//                1,L"_ADVERB",0,0,2,
	//                2,L"_VERBPRESENT",L"_BE",0,1,1,0);
	// wouldn't I think I should run away?
	cPattern::create(L"_QT1{_FINAL_IF_ALONE:VERB}",L"6",
									1,L"_COND2",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_THINKPRESENTFIRST",0,1,1,
									1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									0);
		// do I go? / don't you be afraid //
	cPattern::create(L"_Q1{VERB}",L"7",1,L"_ADVERB",0,0,1,
									1,L"_DO*-4{imp}",0,1,1, // -4 is to encourage _VERBPRESENT from not becoming a noun (the object of _DO) in an _SQ or _Q2[G]
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									2,L"_VERBPRESENT",L"_BE{vS:V_OBJECT:id}",0,1,1,
									0);
	// do I think I should go?
	cPattern::create(L"_QT1{_FINAL_IF_ALONE:VERB}",L"7",
									1,L"_ADVERB",0,0,1,
									1,L"_DO{imp}",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_THINKPRESENTFIRST",0,1,1,
									1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									0);
		// am/was I going? was I being?
	// "C" structure of verb phrases from Quirk CGEL (3.54)
	cPattern::create(L"_Q1{VERB}",L"8",
									1,L"_IS",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									2,L"verb{vC:V_OBJECT}",L"being{vC:id:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
									// _NAME is only included to prevent it from being an _ADJECTIVE in the _OBJECTS_0 of the parent _Q2 pattern
									3,L"_ADVERB",L"preposition*2",L"_INFP",0,0,1,0); // preposition use should be rare!  
	// am/was I given? was I given? moved to _Q1PASSIVE
	//cPattern::create(L"_Q1{VERB}",L"B",
	//								1,L"_IS",0,1,1,
	//								1,L"__NOUN[*]{SUBJECT}",0,1,1,
	//								1,L"_ADVERB",0,0,2,
	//								2,L"verb{vD:V_OBJECT}",L"being{vD:id:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
	//								2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
	// below eliminated because _Q1 8 above and _Q1 below with an additional object of _INFP took care of this.
	// FORM 1:am/was I going to be? | am/was I reputed to be? / am I supposed to run?
	// FORM 2:am I to run? am I to go?  am I to be?
	// am I to send him? / am I to be sending him?
	//cPattern::create(L"_Q1INFP",L"",
	//                    1,L"_IS",0,1,1,
	//                    1,L"_NOUN[*]",0,1,1,
	//                    1,L"_ADVERB",0,0,3,
	//                    1,L"verb",VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE,0,1, // FORM 1/2
	//                    1,L"_INFP",0,1,1,0);
	// am/was I thinking I should go? was I thinking the baby should go with me?
	cPattern::create(L"_QT1{_FINAL_IF_ALONE:VERB}",L"8",
									1,L"_IS",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"think{vC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
									1,L"_ADVERB",0,0,1,
									1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									0);
		// have/had I gone? / had I been?
	cPattern::create(L"_Q1{VERB}",L"9",1,L"_ADVERB",0,0,1,
									1,L"_HAVE*-2",0,1,1, // -2 is to encourage _VERBPASTPART from not becoming a noun (the object of _DO) in an _SQ or _Q2[G]
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									2,L"_VERBPASTPART{vB}",L"_BEEN{vB}",0,1,1,
									//1,L"EOS",0,1,1,
									0);
	// have/had I thought the baby would go with me?
	cPattern::create(L"_QT1{_FINAL_IF_ALONE:_BLOCK:VERB}",L"9",1,L"_ADVERB",0,0,1,
									1,L"_HAVE",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_THINKPASTPART{vB}",0,1,1,
									1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									0);
		// will I have though the baby would go with me? | will I definitely have thought this is the way to go?
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_QT1{_FINAL_IF_ALONE:VERB}",L"A",
									1,L"_COND",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_HAVE",0,1,1,
									1,L"_THINKPASTPART{vAB}",0,1,1,
									1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									0);
		// have I been sending | had I been sending
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1{VERB}",L"I",
										1,L"_ADVERB",0,0,1,
										1,L"_HAVE",0,1,1,
										1,L"__NOUN[*]{SUBJECT}",0,1,1,
										1,L"_ADVERB",0,0,2,
										1,L"_BEEN",0,1,1,
										3,L"verb{vBC:V_OBJECT}",L"does{vBC:V_OBJECT}",L"have{vBC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
										2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
		// will I be sending | would I be sending | will I not be sending | would I not be sending
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1{VERB}",L"C",
									1,L"_COND",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_BE{_BLOCK}",0,1,1,
									3,L"verb{vAC:V_OBJECT}",L"does{vAC:V_OBJECT}",L"have{vAC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
									2,L"_ADVERB",L"preposition*2",0,0,1,0);  // preposition use should be rare!
	// would I have gone?
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1{VERB}",L"D",
									1,L"_COND",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_HAVE",0,1,1,
									2,L"_VERBPASTPART{vAB:V_OBJECT}",L"_BEEN{vAB}",0,1,1,0);
	// would I have thought this is the way to go?
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_QT1{_FINAL_IF_ALONE:VERB}",L"D",
									1,L"_COND",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADVERB",0,0,2,
									1,L"_HAVE",0,1,1,
									1,L"_THINKPASTPART{vAB}",0,1,1,
									1,L"__S1{OBJECT:EVAL:_BLOCK}",0,1,1,
									0);
		// would I have been sending / will I have been sending
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1{VERB}",L"E",
												1,L"_COND",0,1,1,
												1,L"__NOUN[*]{SUBJECT}",0,1,1,
												1,L"_ADVERB",0,0,1,
												1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation
												1,L"_BEEN",0,1,1,
												3,L"verb{vABC:V_OBJECT}",L"does{vABC:V_OBJECT}",L"have{vABC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
												2,L"_ADVERB",L"preposition*2",0,0,1,0);// preposition use should be rare!
		// am I sent | was I sent / also _Q1"B" 	// am/was I given? was I given?
	cPattern::create(L"_Q1PASSIVE{VERB}",L"1",
												 1,L"_IS",0,1,1,
												 1,L"__NOUN[*]{SUBJECT}",0,1,1,
												 1,L"_ADVERB",0,0,2,
													3,L"verb{vD:V_OBJECT}",L"being{vD:id:V_OBJECT}",L"does{V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													2,L"_ADVERB",L"preposition*4",0,0,1,0); // preposition use should be rare!
		// am I being sent | was I being sent
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1PASSIVE{VERB}",L"2",
													1,L"_IS",0,1,1,
												 1,L"__NOUN[*]{SUBJECT}",0,1,1,
												 1,L"_ADVERB",0,0,2,
												 1,L"_BEING{_BLOCK}",0,1,1,
												 1,L"_VERBPASTPART{vCD}",0,1,1,0);
		// will I be sent | would I be sent | will I not be sent | would I not be sent
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1PASSIVE{VERB}",L"3",
												1,L"_COND",0,1,1,
												 1,L"__NOUN[*]{SUBJECT}",0,1,1,
												 1,L"_ADVERB",0,0,2,
												 1,L"_BE{_BLOCK}",0,1,1,
												 3,L"verb{vAD:V_OBJECT}",L"does{vAD:V_OBJECT}",L"have{vAD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
												 2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
		// have I been sent | had I been sent
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1PASSIVE{VERB}",L"4",1,L"_ADVERB",0,0,1,
													1,L"_HAVE",0,1,1,
													 1,L"__NOUN[*]{SUBJECT}",0,1,1,
													 1,L"_ADVERB",0,0,2,
													 1,L"_BEEN",0,1,1,
													 3,L"verb{vBD:V_OBJECT}",L"does{vBD:V_OBJECT}",L"have{vBD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
		// would/will I have been sent
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1PASSIVE{VERB}",L"5",
														1,L"_COND",0,1,1,
													 1,L"__NOUN[*]{SUBJECT}",0,1,1,
													 1,L"_ADVERB",0,0,1,
													 1,L"_HAVE",0,1,1, // {_BLOCK} removed 8/17 will block negation
													 1,L"_BEEN",0,1,1,
													 3,L"verb{vABD:V_OBJECT}",L"does{vABD:V_OBJECT}",L"have{vABD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 2,L"_ADVERB",L"preposition*2",0,0,1,0); // preposition use should be rare!
	// have I,L" he said, L"been sent | will I have been sent | will I have been sending
	// INTERROGATIVES p.803 CGEL
	cPattern::create(L"_Q1PASSIVE{VERB}",L"7",1,L"_ADVERB",0,0,1,
													1,L"_HAVE",0,1,1,
													 1,L"__NOUN[*]{SUBJECT}",0,1,1,
													 1,L"_ADVERB",0,0,1,
													 1,L"_BEEN",0,1,1,
													 3,L"verb{vBD:V_OBJECT}",L"does{vBD:V_OBJECT}",L"have{vBD:V_OBJECT}",VERB_PAST_PARTICIPLE,1,1,
													 2,L"_ADVERB",L"preposition*2",0,0,1,0);


	// At which university does Krugman teach?
	cPattern::create(L"_Q2PREP{_FINAL_IF_ALONE:_BLOCK:PREP:_NO_REPEAT:_ONLY_BEGIN_MATCH}",L"1",
									 1,L"_ADVERB*1",0,0,1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
									 // into how many languages...
									 4,L"relativizer|which{QTYPE}",L"relativizer|what{QTYPE}",L"relativizer|whose{QTYPE}",L"relativizer|how{QTYPE}",0,1,1, 
									 1,L"__ADJECTIVE",0,0,1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 6,L"_NOUN_OBJ{PREPOBJECT}",L"__NOUN[*]{PREPOBJECT}",L"__MNOUN[*]{PREPOBJECT}",L"__NOUNREL{PREPOBJECT}",L"_ADJECTIVE[*]*4",L"__NOUNRU{PREPOBJECT}",0,1,1,  // _NOUN* includes NOUN[D] and NOUN[E]
									 0);
	// With whom did Kurt Weill collaborate?
	cPattern::create(L"_Q2PREP{_FINAL_IF_ALONE:_BLOCK:PREP:_NO_REPEAT:_ONLY_BEGIN_MATCH}",L"2",
									 1,L"_ADVERB*1",0,0,1, // discourage ADVERBS if they can be picked up from ALLOBJECTS instead and bound to the previous verb
									 2,L"preposition{P}",L"verbalPreposition{P}",0,1,1,
									 7,L"relativizer|which*1{QTYPE:PREPOBJECT}",L"relativizer|where{QTYPE:PREPOBJECT}",L"relativizer|what*1{QTYPE:PREPOBJECT}",L"relativizer|whose*1{QTYPE:PREPOBJECT}",L"relativizer|how{QTYPE:PREPOBJECT}",L"relativizer|when{QTYPE:PREPOBJECT}",L"relativizer|whom{QTYPE:PREPOBJECT}",0,1,1, 
									 0);
		// is it really you? / why is it really you? / where shall we go? / can you give us the book?
	// if this is ever added to, then add FINAL_IF_ALONE to _Q1 and _Q1PASSIVE
	// re-added _ALLVERB because _Q2 is altered by the _QUESTION characteristic which alters the weight against _VERBREL1.
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_QUESTION}",L"F",
									1,L"_INTRO_S1{_BLOCK:EVAL}",0,0,1, 
									2,L"relativizer*-2{QTYPE:OBJECT}",L"_Q2PREP*-2",0,0,1, 
									1,L"__INTERPPB",0,0,1,
									2,L"_Q1",L"_Q1PASSIVE",0,1,1, // _VERBREL1 removed because ALLOBJECTS follow is redundant
									// __ALLOBJECTS_0 would be harmful here "(" ALLOBJECTS_0 could resolve to a NAME, which must be an object, but will not be registered as one if
									//   __ALLOBJECTS_0 is its parent. This is especially important because the relativizer will be registered also as an object
									1,L"adjective{ADJ}",0,0,1,
									4,L"__QNOUN",L"_PP", L"__ALLOBJECTS_1", L"__ALLOBJECTS_2*1", 0,0,1, // ,L"_INFP{OBJECT:_BLOCK}" RINFP 6/7/2006 -- *1 encourages the object to be in Q1, not outside.
									1,L"__CLOSING__S1",0,0,3,
									0);
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_QUESTION}",L"G",
									1,L"__INTRO_S1{_BLOCK:EVAL}",0,0,1, 
									1,L"_Q2PREP*-2",0,1,1, // don't add OBJECT to this relativizer - it will screw up agreement
									1,L"__INTERPPB",0,0,1,
									2,L"__ALLVERB*1", L"_VERBPASSIVE",0,1,1, // _VERBREL1 removed because ALLOBJECTS follow is redundant
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"__CLOSING__S1",0,0,3,
									0);
		// what earthly need could it have for her?
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_QUESTION}",L"J",
									1,L"__INTRO_S1{_BLOCK:EVAL}",0,0,1, 
									1,L"relativizer",0,1,1,
									2,L"__NOUN[*]{OBJECT}",L"_PP",0,1,1, // _PP what on God's earth have you been doing?
									2,L"_Q1",L"_Q1PASSIVE",0,1,1, // _VERBREL1 removed because ALLOBJECTS follow is redundant  *1 encourages the object to be in Q1, not outside.
									// __ALLOBJECTS_0 would be harmful here because ALLOBJECTS_0 could resolve to a NAME, which must be an object, but will not be registered as one if
									//   __ALLOBJECTS_0 is its parent. This is especially important because the beginning __NOUN will be registered also as an object
									4,L"__QNOUN",L"_PP",L"adjective{ADJ}",L"__ALLOBJECTS_1",0,0,1, 
									1,L"__CLOSING__S1",0,0,3, // which company is he president of? - preposition moved to [P]
									0);
	cPattern::create(L"_Q2EMBED",L"P",
									1,L"_IS{VERB:vS:id}",0,1,1,
									1,L"__NOUN[*]{SUBJECT}",0,1,1, // he C1__S1 matches too many things
									0);
	// The 'Object' in this instance is the object of the preposition.  In this case the __ALLOBJECTS_1 is not discouraged (unlike the previous patterns) because the first object is not associated directly with the verb.
	// which company is he president of?
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:PREP:_QUESTION}",L"P",
									1,L"relativizer",0,1,1,
									1,L"__NOUN[*]{PREPOBJECT}",0,1,1, // _PP what on God's earth have you been doing?
									3,L"_Q1",L"_Q1PASSIVE",L"_Q2EMBED",0,1,1, // _VERBREL1 removed because ALLOBJECTS follow is redundant  *1 encourages the object to be in Q1, not outside.
									// __ALLOBJECTS_0 would be harmful here because ALLOBJECTS_0 could resolve to a NAME, which must be an object, but will not be registered as one if
									//   __ALLOBJECTS_0 is its parent
									4,L"__QNOUN",L"_PP",L"adjective{ADJ}",L"__ALLOBJECTS_1",0,0,1, 
									1,L"preposition{P}",0,1,1, // which company is he president of?
									0);
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:PREP:_QUESTION}",L"Q",
									1,L"relativizer{PREPOBJECT}",0,1,1,
									3,L"_Q1",L"_Q1PASSIVE",L"_Q2EMBED",0,1,1, // _VERBREL1 removed because ALLOBJECTS follow is redundant  *1 encourages the object to be in Q1, not outside.
									// __ALLOBJECTS_0 would be harmful here because ALLOBJECTS_0 could resolve to a NAME, which must be an object, but will not be registered as one if
									//   __ALLOBJECTS_0 is its parent
									4,L"__QNOUN",L"_PP",L"adjective{ADJ}",L"__ALLOBJECTS_1",0,0,1, 
									1,L"preposition{P}",0,1,1, // which company is he president of?
									0);
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_QUESTION}",L"K",
									1,L"relativizer",0,1,1,
									3,L"__NOUN[*]",L"_PP",L"_ADJECTIVE{_BLOCK}",0,1,1, // _PP what on God's earth is it? / What good is it? / Which bottle is it?
									1,L"_IS{VERB:vS:id}",0,1,1, // _VERBREL1 removed because ALLOBJECTS follow is redundant
									1,L"__C1__S1",0,0,1,  // this is the subject
									1,L"__CLOSING__S1",0,0,3,
									0);
	// what is the tree green for // What is "WWE" short for?
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:PREP:_QUESTION}",L"I",
									1,L"relativizer{PREPOBJECT}",0,1,1,
									1,L"_IS{VERB:vS:id}",0,1,1, 
									1,L"__NOUN[*]{SUBJECT}",0,1,1,
									1,L"_ADJECTIVE{ADJ}",0,1,1, 
									1,L"preposition*-1{P}",0,1,1, 
									0);
	// what for?  / whose shoes?
	// char *interrogative_determiner[] = {L"what",L"which",L"whose",L"whatever",L"whichever",L"whosoever",NULL};
	// did Shakespeare cross -your -path early on ?
	//  char *relativizer[] = {L"who",L"which",L"that",L"whom",L"whose",L"where",L"when",L"why",NULL};
	// who me? why me? where to? when from?
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_QUESTION}",L"G",
									2,L"interrogative_determiner",L"relativizer",0,1,1,
									4,L"preposition*2",L"__NOUN[*]",L"_PP*1",L"not",0,1,1, // discourage hanging prepositions L"_PP" handled by __NOUN[9]
									1,L"__CLOSING__S1",0,0,3,
									0);
	// that you, Hersheimmer? / that yours, Johnny? / that him, Bill? 
	// the important issue with this is that 'that' has been separated from relativizers, which is usually OK, except with this
	// pattern, which is really short for 'is that you'.  If a _NOUN is allowed after 'that', it becomes very ambiguous because that is usually a modifier
	cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_QUESTION}",L"H",
									1,L"demonstrative_determiner|that",0,1,1, // this was separated from _Q2"F" because otherwise with optional relativizer it is identical with _VERBREL1
									3,L"personal_pronoun{NOUN}",L"possessive_pronoun{NOUN}",L"personal_pronoun_accusative{NOUN}",0,0,1, // ,L"_INFP{OBJECT:_BLOCK}L" RINFP 6/7/2006
									1,L"__CLOSING__S1",0,0,2,
									0);
	// is that your idea, Tuppence?
	//cPattern::create(L"_Q2{_FINAL_IF_ALONE:_ONLY_BEGIN_MATCH:_QUESTION}",L"L",
	//								1,L"_IS{VERB:vS:id}",0,1,1, 
	//								1,L"__NOUN[*]{SUBJECT}",0,1,1,
	//								1,L"__ALLOBJECTS_1",0,1,1, 
	//								1,L"__CLOSING__S1",0,0,3,
	//								0);
	// what would//could//will//should//must you say is the best?
	// who would you think is the worst?
	// which would they think runs the fastest?
	// who would he feel
	// What would you say -is -the -toughest -time after finishing drama school ?
	cPattern::create(L"__QSUBJECT{_BLOCK}",L"1",
						1,L"relativizer",0,1,1,
						2,L"_COND",L"_DO{imp}",0,1,1,
						1,L"__NOUN[*]{SUBJECT}",0,1,1,
						1,L"_THINKPRESENTFIRST",0,1,1,
						0);
	cPattern::create(L"__QSUBJECT{_BLOCK}",L"2",
						1,L"relativizer",0,1,1,
						1,L"_COND",0,1,1,
						1,L"__NOUN[*]{SUBJECT}",0,1,1,
						1,L"_HAVE",0,1,1,
						1,L"_THINKPASTPART{vAB}",0,1,1,
						0);
	cPattern::create(L"__QSUBJECT{_BLOCK}",L"3",
						1,L"relativizer",0,1,1,
						1,L"_COND",0,1,1,
						1,L"__NOUN[*]{SUBJECT}",0,1,1,
						1,L"_HAVE",0,1,1,
						1,L"_BEEN",0,1,1,
						1,L"think{vBC:V_OBJECT}",VERB_PRESENT_PARTICIPLE,1,1,
						0);

		cPattern::create(L"__SQ{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_QUESTION}",L"1",
									 1,L"relativizer{SUBJECT}",0,1,1,
									 2,L"__ALLVERB",L"_COND{VERB}",0,1,1,
									 1,L":",0,0,1, 
										// __ALLOBJECTS_0 would be harmful here because ALLOBJECTS_0 could resolve to a NAME, which must be an object, 
										// but will not be registered as one if __ALLOBJECTS_0 is its parent
									 5,L"__QNOUN",L"_PP",L"adjective{ADJ}",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
									 1,L"__CLOSING__S1",0,0,3,
									 0);
		cPattern::create(L"__SQ{_FINAL_IF_NO_MIDDLE_MATCH_EXCEPT_SUBPATTERN:_QUESTION}",L"2",
									 1,L"relativizer{SUBJECT}",0,1,1,
									 1,L"_VERBPASSIVE_P",0,1,1,
									 1,L"__CLOSING__S1",0,0,3,
									 0);
			// Nurse Edith, did you say her name was?
	cPattern::create(L"_DISPLACED_OBJECT{_FINAL:_ONLY_BEGIN_MATCH:_QUESTION}",L"1",
						1,L"__ALLOBJECTS_1",0,1,1,
						1,",",0,1,1,
						1,L"_ADVERB",0,0,1,
						1,L"_DO{imp}",0,1,1,
						1,L"__NOUN[*]{SUBJECT}",0,1,1,
						1,L"_ADVERB",0,0,2,
						1,L"_THINKPRESENTFIRST{V_HOBJECT}",0,1,1,
						1,L"__NOUN[*]{HOBJECT}",0,1,1,
						2,L"__ALLVERB",L"_COND{VERB}",0,1,1,
						0);
	cPattern::create(L"_RELQ{_FINAL_IF_ALONE:_FORWARD_REFERENCE:S_IN_REL:_QUESTION}", L"",
									3, L"_ADJECTIVE", L"_ADVERB", L"conjunction|but", 0, 0, 1,
									1, L"relativizer*-1", 0, 1, 1, // this is necessary to beat Q1[J] which matches the same but incorrectly
									1, L"_ADVERB", 0, 0, 1, // where simply every one is bound to turn up sooner or later
									1, L"__S1{_BLOCK:EVAL}", 0, 1, 1,
									1, L"preposition*4", 0, 0, 1, // that you are afraid 'of'// preposition use should be rare!
									0);
	//  Mrs . Edgar Keith lives here , does she[mrs] not ?
	cPattern::create(L"_MS1{_FINAL:_QUESTION}",L"7",
									1,L"conjunction|but", 0, 0, 1,
									1,L"__S1{_BLOCK:EVAL}",0,1,1,
									1,L",",0,1,1,
									1,L"does",0,1,1,
									2,L"personal_pronoun_nominative",L"personal_pronoun",0,1,1,
									1,L"not",0,0,1,
									0);
	// would you, Tommy?
	cPattern::create(L"_MQ1{_FINAL_IF_ALONE:_QUESTION}",L"1",
									1, L"conjunction|but", 0, 0, 1,
									1,L"relativizer",0,0,1,
									4,L"_Q1S",L"_Q1PASSIVE",L"_Q1",L"_QT1",0,1,1,
									1,L",",0,1,1, // , ma'am // if this is made optional, _NOUN of C4 and _ALLOBJECT of C3 are identical
									5,L"_NAME{HAIL}",L"_META_GROUP{HAIL}",L"honorific{HON}",L"_HON_ABB{HON}",L"_PP",0,1,1, // , sir / , freak! noun includes _NAME, L"honorific",
									0);
	// would you if you could?
	// What does he want from me, I wonder.
	cPattern::create(L"_MQ1{_FINAL_IF_ALONE:_QUESTION}",L"2",
									1,L"conjunction|but",0,0,1,
									1,L"relativizer",0,0,1,
									4,L"_Q1S",L"_Q1PASSIVE",L"_Q1",L"_QT1",0,1,1,
									// __ALLOBJECTS_0 would be harmful here because ALLOBJECTS_0 could resolve to a NAME, which must be an object, but will not be registered as one if
									//   __ALLOBJECTS_0 is its parent.  
									5,L"__QNOUN",L"_PP",L"adjective{ADJ}",L"__ALLOBJECTS_1",L"__ALLOBJECTS_2",0,0,1, // there must only be one adjective and it must be last (not mixed in) see *
									1,L",",0,0,1, 
									1,L"conjunction|if",0,0,1,
									3,L"__S1{EVAL:_BLOCK}", L"_PP",L"_INFP",0,1,1, // Why don't you get a showy tie , like *mine* ?
									0);
	// But if so, where was the girl, and what had she done with the papers?
	cPattern::create(L"_MQ1{_FINAL_IF_ALONE:_STRICT_NO_MIDDLE_MATCH:_QUESTION}",L"3",
									1,L"_STEP",0,0,1,
									1,L"_INTRO_S1",0,0,1,
									1,L"_Q2{EVAL:_BLOCK}",0,1,1,
									1,L",",0,0,1,
									1,L"and",0,1,1,
									1,L"_Q2{EVAL:_BLOCK}",0,1,1,
									0);
	// Do you think that I should care for a moment for such things as those , or *that* they have brought the slightest taint of disgrace upon you in the minds of those that know you ?
	cPattern::create(L"_MQ1{_FINAL_IF_ALONE:_QUESTION}", L"4",
									1, L"conjunction|but", 0, 0, 1,
									1, L"relativizer", 0, 0, 1,
									4, L"_Q1S", L"_Q1PASSIVE", L"_Q1", L"_QT1", 0, 1, 1,
									// __ALLOBJECTS_0 would be harmful here because ALLOBJECTS_0 could resolve to a NAME, which must be an object, but will not be registered as one if
									//   __ALLOBJECTS_0 is its parent.  
									5, L"__QNOUN", L"_PP", L"adjective{ADJ}", L"__ALLOBJECTS_1", L"__ALLOBJECTS_2", 0, 0, 1, // there must only be one adjective and it must be last (not mixed in) see *
									1, L"__MSTAIL", 0, 1, 1,
									0);
}

// after a question, a new paragraph, and a non-quote paragraph, 
//   the next subject matching should be the opposite of the speaker of the question.
//   AND the previous speaker of the question should be the opposite of the next subject.
// exception: if the last sentence ends in a ... .
void Source::setQuestion(vector <WordMatch>::iterator im,bool inQuote,int &questionSpeakerLastSentence,int &questionSpeaker,bool &currentIsQuestion)
{ LFS
	currentIsQuestion=false;
	int openingQuote=lastOpeningPrimaryQuote;
	bool forwardInQuote=inQuote;
  vector <WordMatch>::iterator imEOS;
  for (imEOS=im,imEOS++; imEOS!=m.end(); imEOS++)
	{
		// skip secondary quotes
    if (imEOS->word->first==L"‘")
		{
		  for (imEOS++; imEOS!=m.end() && imEOS->word->first!=L"’" && imEOS->word->first!=L"”"; imEOS++);
			if (imEOS==m.end() || imEOS->word->first==L"”")
			{
				if (forwardInQuote) 
				{
					questionSpeakerLastSentence=questionSpeaker;
					questionSpeaker=-1;
				  if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:QXQ questionSpeakerLastSentence set to %d. questionSpeaker set to %d.",im-m.begin(),questionSpeakerLastSentence,questionSpeaker);
				}
				break;
			}
			imEOS++;
		}
		if (imEOS==m.end()) break;
    if (imEOS->word->first==L"“" && !(imEOS->flags&WordMatch::flagQuotedString))
		{
			openingQuote=(int)(imEOS-m.begin());
			forwardInQuote=true;
		}
		// checking for the sectionWord makes it more likely ':' is not in the middle of a sentence.
		// The purpose is to detect the end of a sentence, not an utterance, because this section only 
		// starts on an EOS, which means if we stopped before the end of a sentence, we might miss a '?'.
		if (imEOS->word->first==L"?" || (imEOS->word->first==L":" && (imEOS+1)->word==Words.sectionWord) || 
			  imEOS->word->first==L"!" || (imEOS->word->first==L"." && !imEOS->PEMACount))
    {
			if (forwardInQuote) 
				questionSpeakerLastSentence=questionSpeaker;
			if (imEOS->word->first==L"?")
			{
				currentIsQuestion=true;
				// not needed because of logic in identifyObject
				//for (vector <WordMatch>::iterator imtmp=im; imtmp!=imEOS; imtmp++)
				//	if (imtmp->getObject()!=-1)
				//		imtmp->flags|=WordMatch::flagInQuestion;
				if (forwardInQuote) 
					questionSpeaker=openingQuote;
			}
			else 
			{
				if (forwardInQuote) 
					questionSpeaker=-1;
			}
		  if (debugTrace.traceSpeakerResolution && forwardInQuote)
				lplog(LOG_RESOLUTION,L"%06d:QXQ questionSpeakerLastSentence set to %d. questionSpeaker set to %d.",im-m.begin(),questionSpeakerLastSentence,questionSpeaker);
      break;
    }
	}
}

void Source::setSecondaryQuestion(vector <WordMatch>::iterator im)
{ LFS
  for (vector <WordMatch>::iterator imEOS=++im; imEOS!=m.end() && (imEOS->word->first!=L"’"); imEOS++)
	{
		// checking for the sectionWord makes it more likely ':' is not in the middle of a sentence.
		// The purpose is to detect the end of a sentence, not an utterance, because this section only 
		// starts on an EOS, which means if we stopped before the end of a sentence, we might miss a '?'.
		if (imEOS->word->first==L"?" || (imEOS->word->first==L":" && (imEOS+1)->word==Words.sectionWord) || 
			  imEOS->word->first==L"!" || (imEOS->word->first==L"." && !imEOS->PEMACount))
    {
			if (imEOS->word->first==L"?")
				for (; im!=imEOS; im++)
					if (im->getObject()!=-1)
						im->flags|=WordMatch::flagInQuestion;
      break;
    }
	}
}

// returns true if the question speaker is different than the subject of the next paragraph.
bool Source::questionAgreement(int where,int whereFirstSubjectInParagraph,int questionSpeakerLastParagraph,vector <cOM> &objectMatches,bool &subjectDefinitelyResolved,bool audience,wchar_t *fromWhere)
{ LFS
	if (whereFirstSubjectInParagraph>=0 && 
		  m[whereFirstSubjectInParagraph].getObject()>=0 && !objects[m[whereFirstSubjectInParagraph].getObject()].plural)
	{
		int subjectObjectClass=objects[m[whereFirstSubjectInParagraph].getObject()].objectClass;
		subjectDefinitelyResolved=(subjectObjectClass!=PRONOUN_OBJECT_CLASS && subjectObjectClass!=REFLEXIVE_PRONOUN_OBJECT_CLASS && subjectObjectClass!=RECIPROCAL_PRONOUN_OBJECT_CLASS);
		// if firstSubject in unquoted paragraph occurring after question = questionSpeaker, log conflict.
		bool allIn,oneIn,agree=intersect(whereFirstSubjectInParagraph,objectMatches,allIn,oneIn);
		if (agree ^ audience)
		{
			wstring tmpstr,tmpstr2,tmpstr3;
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s subject=%d: %s conflict: %s=%d:%s [%s] afterQuestionSubject=%d:%s.",
					where,fromWhere,whereFirstSubjectInParagraph,
						(audience) ? L"audience":L"speaker",
						(audience) ? L"questionAudience":L"questionSpeaker",
					questionSpeakerLastParagraph,
					objectString(objectMatches,tmpstr,true).c_str(),
					(questionSpeakerLastParagraph<0) ? L"":speakerResolutionFlagsString(m[questionSpeakerLastParagraph].flags,tmpstr3).c_str(),
					whereFirstSubjectInParagraph,
					(m[whereFirstSubjectInParagraph].objectMatches.size()) ?
						objectString(m[whereFirstSubjectInParagraph].objectMatches,tmpstr2,true).c_str() :
						objectString(m[whereFirstSubjectInParagraph].getObject(),tmpstr2,true).c_str());
			return false;
		}
	}
	return true;
}

// if a question is rhetorical, this inversion should be cancelled (future - how to tell whether a question is rhetorical)
void Source::correctBySpeakerInversionIfQuestion(int where,int whereFirstSubjectInParagraph)
{ LFS
	wstring tmpstr;
	bool subjectDefinitelyResolved;
	// a reliable speaker will stop resolveSpeakersUsingPreviousSubject from reaching the last question.
	bool reliableSpeakerEncountered=false;
	for (int urs=0; urs<(signed)unresolvedSpeakers.size() && !reliableSpeakerEncountered; urs++)
		if (m[abs(unresolvedSpeakers[urs])].flags&(moreReliableMatchedFlags|moreReliableNotMatchedFlags))
			reliableSpeakerEncountered=true;
	if (!reliableSpeakerEncountered && ((m[whereSubjectsInPreviousUnquotedSection].word->second.inflectionFlags&PLURAL)!=PLURAL || subjectsInPreviousUnquotedSection.size()==1))
	{
		// resolveSpeakersUsingPreviousSubject would be called with subjectsInPreviousUnquotedSection.
		// doesQuestionAgree=call questionAgreement on whereFirstSubjectInParagraph and subjectsInPreviousUnquotedSection.  
		vector <cOM> omSubjects;
		for (unsigned int s=0; s<subjectsInPreviousUnquotedSection.size(); s++)
			omSubjects.push_back(cOM(subjectsInPreviousUnquotedSection[s],SALIENCE_THRESHOLD));
		bool questionAgrees=questionAgreement(where,whereFirstSubjectInParagraph,-1,omSubjects,subjectDefinitelyResolved,false,L"TEST");
		// if unresolvedSpeakers.size() is ODD, then subjectsInPreviousUnquotedSection will be used for the last quote.
		//   If NOT doesQuestionAgree,
		//     reverse speakers in subjectsInPreviousUnquotedSection by computing the inverse of them with respect to the current speaker group
		// if unresolvedSpeakers.size() is EVEN, then subjectsInPreviousUnquotedSection will be the inverse of that used for the quote.
		//   If doesQuestionAgree,
		//     reverse speakers in subjectsInPreviousUnquotedSection by computing the inverse of them with respect to the current speaker group
		if ((unresolvedSpeakers.size()&1) ^ questionAgrees)
		{
	    if (debugTrace.traceSpeakerResolution)
			{
				lplog(LOG_RESOLUTION,L"%06d:   subjectsInPreviousUnquotedSection=%s.",where,objectString(subjectsInPreviousUnquotedSection,tmpstr).c_str());
				lplog(LOG_RESOLUTION,L"%06d:   whereFirstSubjectInParagraph=%d.",where,whereFirstSubjectInParagraph);
				lplog(LOG_RESOLUTION,L"%06d:   (unresolvedSpeakers.size()&1)=%s [SIZE=%d,@0=%d].",where,((unresolvedSpeakers.size()&1)==1) ? L"ODD":L"EVEN",unresolvedSpeakers.size(),(unresolvedSpeakers.size()>=1) ? unresolvedSpeakers[0] : -1);
				lplog(LOG_RESOLUTION,L"%06d:   questionAgrees=%s.",where,(questionAgrees) ? L"TRUE":L"FALSE");
			}
			int sgAt; 
			for (sgAt=0; sgAt<(signed)speakerGroups.size() && speakerGroups[sgAt].sgEnd<whereSubjectsInPreviousUnquotedSection; sgAt++);
			vector<int> invertedSubjects;
			for (set <int>::iterator si=speakerGroups[sgAt].speakers.begin(); si!=speakerGroups[sgAt].speakers.end(); si++)
				if (find(subjectsInPreviousUnquotedSection.begin(),subjectsInPreviousUnquotedSection.end(),*si)==subjectsInPreviousUnquotedSection.end())
					invertedSubjects.push_back(*si);
			if (invertedSubjects.size()==1 || unresolvedSpeakers.empty())
			{
				subjectsInPreviousUnquotedSection=invertedSubjects;
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d: ZXZ questionInversion set subjectsInPreviousUnquotedSection=%s.",where,objectString(subjectsInPreviousUnquotedSection,tmpstr).c_str());
			}
			else
			{
				int saveSubject=(subjectsInPreviousUnquotedSection.size()==1) ? subjectsInPreviousUnquotedSection[0] : -1;
				subjectsInPreviousUnquotedSection.clear();
				for (unsigned int is=0; is<invertedSubjects.size(); is++)
					if (in(invertedSubjects[is],m[abs(unresolvedSpeakers[0])].objectMatches)!=m[abs(unresolvedSpeakers[0])].objectMatches.end())
						subjectsInPreviousUnquotedSection.push_back(invertedSubjects[is]);
				// if this is plural, the question may be rhetorical, so cancel
				if (subjectsInPreviousUnquotedSection.size()!=1 && saveSubject>=0)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d: ZXZ questionInversion [2] cancelled rhetorical question? (invertedSubjects>1 - %s).",where,objectString(subjectsInPreviousUnquotedSection,tmpstr).c_str());
					subjectsInPreviousUnquotedSection.clear();
					subjectsInPreviousUnquotedSection.push_back(saveSubject);
				}
		    else if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d: ZXZ questionInversion [2] set subjectsInPreviousUnquotedSection=%s.",where,objectString(subjectsInPreviousUnquotedSection,tmpstr).c_str());
			}
		}
	}
}

bool Source::testQuestionType(int where,int &whereQuestionType,int &whereQuestionTypeFlags,int setType,set <int> &whereQuestionInformationSourceObjects)
{ LFS
	int oc;
	if (where<0)
		return false;
	wstring tmpstr;
	if (m[where].objectMatches.size()>0 && !(m[where].flags&WordMatch::flagRelativeHead))
	{
		for (unsigned int om=0; om<m[where].objectMatches.size(); om++)
		{
			oc=objects[m[where].objectMatches[om].object].objectClass;
			if (oc==NAME_OBJECT_CLASS || oc==GENDERED_DEMONYM_OBJECT_CLASS || oc==NON_GENDERED_BUSINESS_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS)
			{
				whereQuestionInformationSourceObjects.insert(where);
				if (logDetail)
					lplog(LOG_WHERE,L"picked %d:%s as resolved context suggestion",where,objectString(m[where].objectMatches[om].object,tmpstr,false).c_str());
				break;
			}
		}
	}
	else if (m[where].getObject()>=0 && !(m[where].flags&WordMatch::flagRelativeHead))
	{
			oc=objects[m[where].getObject()].objectClass;
			if (oc==NAME_OBJECT_CLASS || oc==GENDERED_DEMONYM_OBJECT_CLASS || oc==NON_GENDERED_BUSINESS_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS)
			{
				whereQuestionInformationSourceObjects.insert(where);
				if (logDetail)
					lplog(LOG_WHERE,L"picked %d:%s as context suggestion",where,objectString(m[where].getObject(),tmpstr,false).c_str());
			}
	}
	if (!(setType&QTAFlag) && m[where].beginObjectPosition >= 0)
	{
		if (m[where].beginObjectPosition>0)
			testQuestionType(m[where].beginObjectPosition-1,whereQuestionType,whereQuestionTypeFlags,setType|QTAFlag,whereQuestionInformationSourceObjects);
		for (int I=m[where].beginObjectPosition; I<where; I++)
			testQuestionType(I,whereQuestionType,whereQuestionTypeFlags,setType|QTAFlag,whereQuestionInformationSourceObjects);
	}
	if (whereQuestionType<0 && where>=0 && (m[where].queryForm(relativizerForm)>=0 || m[where].queryForm(interrogativeDeterminerForm)>=0))
	{
		whereQuestionType=where;
		whereQuestionTypeFlags=setType;
		return true;
	}
	return false;
}

int Source::getMinPosition(int where)
{ LFS
	int p=m[where].beginPEMAPosition;
	if (p<0) return 0;
	int mp=pema[p].begin;
	while (p!=m[where].endPEMAPosition)
	{
		mp=min(mp,pema[p].begin);
		p=pema[p].nextByPosition;
	}
	mp=min(mp,pema[p].begin);
	return mp;
}

int Source::maxBackwards(int where)
{ LFS
	int w=getMinPosition(where)+where;
	while (w<where)
	{
		where=w;
		w=getMinPosition(where)+where;
		if (m[w].pma.queryPattern(L"_REL1")!=-1 || m[w].pma.queryPattern(L"_Q2")!=-1)
		{
			int len=0;
			if (m[w].pma.queryPattern(L"__INTRO_S1",len)!=-1)
				w+=len;
			return w;
		}
	}
	return w;
}

// whereQuestionType flag 
//   referencingObject, subject, object, secondary object, prep object, etc
//   OR an adjective OF same
void Source::processQuestion(int whereVerb,int whereReferencingObject,__int64 &questionType,int &whereQuestionType,set <int> &whereQuestionInformationSourceObjects)
{ LFS
	int wp=m[whereVerb].relPrep,prepLoop=0,bp,wpo,whereQuestionTypeFlags=0,relObject;
	//bool wqtAdjective=false;
	while (wp>=0 && wp<(int)m.size())
	{
		if (m[wp].getRelObject()>=0 && (bp=m[wpo=m[wp].getRelObject()].beginObjectPosition)>0 &&
			  testQuestionType(wpo,whereQuestionType,whereQuestionTypeFlags,prepObjectQTFlag+prepLoop,whereQuestionInformationSourceObjects))
				break;
		if (prepLoop++>30)
		{
			wstring tmpstr;
			lplog(LOG_ERROR,L"%06d:Prep loop occurred (12) %s.",wp,loopString(wp,tmpstr));
			break;
		}
		wp=m[wp].relPrep;
	}
	testQuestionType(relObject=m[whereVerb].getRelObject(),whereQuestionType,whereQuestionTypeFlags,objectQTFlag,whereQuestionInformationSourceObjects);
	if (relObject>=0)
		testQuestionType(m[relObject].relNextObject,whereQuestionType,whereQuestionTypeFlags,secondaryObjectQTFlag,whereQuestionInformationSourceObjects);
	testQuestionType(whereReferencingObject,whereQuestionType,whereQuestionTypeFlags,referencingObjectQTFlag,whereQuestionInformationSourceObjects);
	testQuestionType(m[whereVerb].relSubject,whereQuestionType,whereQuestionTypeFlags,subjectQTFlag,whereQuestionInformationSourceObjects);
	if (m[whereVerb].endPEMAPosition>=0 && whereVerb+pema[m[whereVerb].endPEMAPosition].begin>0)
	{
		int mb;
		testQuestionType(mb=maxBackwards(whereVerb),whereQuestionType,whereQuestionTypeFlags,0,whereQuestionInformationSourceObjects);
		testQuestionType(mb-1,whereQuestionType,whereQuestionTypeFlags,0,whereQuestionInformationSourceObjects);
	}
	if (whereQuestionType>=0)
	{
		questionType=unknownQTFlag;
		wstring questionTypeWord=m[whereQuestionType].word->first;
		if (questionTypeWord==L"which")
			questionType=whichQTFlag; 
		else if (questionTypeWord==L"where")
			questionType=whereQTFlag;
		else if (questionTypeWord==L"what")
			questionType=whatQTFlag;
		else if (questionTypeWord==L"whose")
			questionType=whoseQTFlag;
		else if (questionTypeWord==L"how")
			questionType=howQTFlag;
		else if (questionTypeWord==L"when")
			questionType=whenQTFlag;
		else if (questionTypeWord==L"whom")
			questionType=whomQTFlag;
		else if (questionTypeWord==L"who")
			questionType=whomQTFlag;
		else if (questionTypeWord==L"why")
			questionType=whyQTFlag;
		questionType+=whereQuestionTypeFlags; // questionType is max 8 bits
	}
}

