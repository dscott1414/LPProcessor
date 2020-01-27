#include <windows.h>
#include <stdio.h>
#include "string.h"
#include <io.h>
#include "word.h"
#include "source.h"
#include "bncc.h"
#include "time.h"

bool preTaggedSource=false;
int clocksec();

struct
{
  wchar_t *SGML;
  wchar_t *substitute;
} bncChars[]=
{
{ L"&Aacute;",L"A"},
{ L"&aacute;",L"a"}, //small a, acute accent  2145
{ L"&abreve;",L"a"}, //small A, breve  4
{ L"&Acirc;",L"A"}, //capital A, circumflex accent  8
{ L"&acirc;",L"a"}, //small a, circumflex accent  772
{ L"&acute;",L"a"}, //acute accent  3
{ L"&AElig;",L"A"}, //capital AE diphthong (ligature)  324
{ L"&aelig;",L"a"}, //small ae diphthong (ligature)  232
{ L"&agr;",L"a"},   //small alpha, Greek  1237
{ L"&Agrave;",L"A"}, //capital A, grave accent  43
{ L"&agrave;",L"a"}, //small a, grave accent  878
{ L"&Amacr;",L"A"}, //capital A, macron  20
{ L"&amacr;",L"a"}, //small a, macron  581
{ L"&amp;",L"&"}, //ampersand  18931
{ L"&amp&",L"&"}, //ampersand  18931 - added because of special newsbank processing in parseWord
{ L"&aogon;",L"a"},// small a, ogonek  2
{ L"&ape;",L"="},//approximate, equals  5
{ L"&Aring;",L"A"},//  capital A, ring  141
{ L"&aring;",L"a"},//  small a, ring  65
{ L"&ast;",L"*"},// asterisk  919
{ L"&atilde;",L"a"},// small a, tilde  244
{ L"&Auml;",L"A"},// capital A, dieresis or umlaut mark  6
{ L"&auml;",L"a"},// small a, dieresis or umlaut mark  967
{ L"&Bgr;",L"B"},// capital Beta, Greek  186
{ L"&bgr;",L"b"},// small beta, Greek  1122
{ L"&bquo;",L"“"},// normalized begin quote mark  771009
{ L"&bsol;",L" "},// reverse solidus  226
{ L"&bull;",L"*"},// round bullet, filled  2150
{ L"&cacute;",L"c"},// small c, acute accent  186
{ L"&Ccaron;",L"C"},// capital C, caron  31
{ L"&ccaron;",L"c"},// small c, caron  143
{ L"&Ccedil;",L"C"},// capital C, cedilla  17
{ L"&ccedil;",L"c"},// small c, cedilla  1320
{ L"&ccirc;",L"c"},// small c, circumflex accent  2
{ L"&cent;",L" "},// cent sign  3
{ L"&check;",L"X"},// tick, check mark  13
{ L"&cir;",L" "},// circle, open  21
{ L"&circ;",L"c"},// circumflex accent  100
{ L"&commat;",L"@"},// commercial at  189
{ L"&copy;",L" "},// copyright sign  65
{ L"&dash;",L"-"},//  hyphen (true graphic)  1
{ L"&dcaron;",L"d"},//  small d, caron  1
{ L"&Dgr;",L"D"},//  capital Delta, Greek  251
{ L"&dgr;",L"d"},//  small delta, Greek  151
{ L"&deg;",L""},// degree?
{ L"&divide;",L"/"},//  divide sign  58
{ L"&dollar;",L"$"},//  dollar sign  24552
{ L"&dstrok;",L"d"},//  small d, stroke  18
{ L"&Eacute;",L"E"},// capital E, acute accent  269
{ L"&eacute;",L"e"},// small e, acute accent  16078
{ L"&Ecaron;",L"E"},// capital E, caron  2
{ L"&ecaron;",L"e"},// small e, caron  67
{ L"&Ecirc;",L"E"},// capital E, circumflex accent  2
{ L"&ecirc;",L"e"},// small e, circumflex accent  718
{ L"&eegr;",L"e"},// small eta, Greek  48
{ L"&Egr;",L"E"},// capital Epsilon, Greek  170
{ L"&egr;",L"e"},// small epsilon, Greek  207
{ L"&Egrave;",L"E"},// capital E, grave accent  14
{ L"&egrave;",L"e"},// small e, grave accent  2577
{ L"&emacr;",L"e"},// small e, macron  4
{ L"&eogon;",L"e"},// small e, ogonek  6
{ L"&equals;",L"="},// equals sign  3
{ L"&equo;",L"”"},// normalized end quote mark  752621
{ L"&eth;",L"e"},// small eth, Icelandic  4
{ L"&Euml;",L"E"},// capital E, dieresis or umlaut mark  15
{ L"&euml;",L"e"},// small e, dieresis or umlaut mark  482
{ L"&flat;",L"--"}, // from BNCC
{ L"&formula;",L"--"}, // from BNCC
{ L"&frac12;",L"1/2"},// fraction one-half  2795
{ L"&frac13;",L"1/3"},// fraction one-third  68
{ L"&frac14;",L"1/4"},// fraction one-quarter  575
{ L"&frac15;",L"1/5"},// fraction one-fifth  20
{ L"&frac16;",L"1/6"},// fraction one-sixth  6
{ L"&frac17;",L"1/7"},// fraction one-seventh  2
{ L"&frac18;",L"1/8"},// fraction one-eighth  60
{ L"&frac19;",L"1/9"},// fraction one-ninth  1
{ L"&frac23;",L"2/3"},// fraction two-thirds  50
{ L"&frac25;",L"2/5"},// fraction two-fifths  9
{ L"&frac34;",L"3/4"},// fraction three-quarters  325
{ L"&frac35;",L"3/5"},// fraction three-fifths  5
{ L"&frac38;",L"3/8"},// fraction three-eighths  52
{ L"&frac45;",L"4/5"},// fraction four-fifths  5
{ L"&frac47;",L"4/7"},// fraction four-sevenths  1
{ L"&frac56;",L"5/6"},// fraction five-sixths  1
{ L"&frac58;",L"5/8"},// fraction five-eighths  33
{ L"&frac78;",L"7/8"},// fraction seven-eighths  7
{ L"&ft;",L"ft"},//  feet indicator  630
{ L"&ge;",L">="},//  greater-than-or-equal  18
{ L"&Ggr;",L"G"},//  capital Gamma, Greek  33
{ L"&ggr;",L"g"},//  small gamma, Greek  499
{ L"&grave;",L""},//  grave accent  2
{ L"&Gt;",L">>"},//  dbl greater-than sign  8
{ L"&gt;",L">"},//  greater-than sign  1102
{ L"&gt>",L">"},//  greater-than sign  1102 - added because of special newsbank processing in parseWord
{ L"&half;",L"1/2"},//  fraction one-half  74
{ L"&hearts;",L""},//  heart suit symbol  1
{ L"&hellip;",L"|"},//  ellipsis (horizontal)  77286
{ L"&horbar;",L"_"},//  horizontal bar  2
{ L"&hstrok;",L"h"},//  small h, stroke  2
{ L"&Iacute;",L"I"},//  capital I, acute accent  3
{ L"&iacute;",L"i"},// small i, acute accent  1278
{ L"&Icirc;",L"I"},// capital I, circumflex accent  21
{ L"&icirc;",L"i"},// small i, circumflex accent  225
{ L"&iexcl;",L""},// inverted exclamation mark  21
{ L"&igr;",L"i"},// small iota, Greek  2
{ L"&igrave;",L"i"},// small i, grave accent  39
{ L"&imacr;",L"i"},// small i, macron  10
{ L"&infin;",L"~"},// infinity  15
{ L"&ins;",L"\""},// inches indicator  2306
{ L"&iquest;",L"?"},// inverted question mark  11
{ L"&Iuml;",L"I"},// capital I, dieresis or umlaut mark  1
{ L"&iuml;",L"i"},// small i, dieresis or umlaut mark  507
{ L"&kgr;",L""},// small kappa, Greek  29
{ L"&khgr;",L""},// small chi, Greek  300
{ L"&Lacute;",L"L"},// capital L, acute accent  2
{ L"&lacute;",L"l"},// small l, acute accent  2
{ L"&larr;",L""},// leftward arrow  1
{ L"&lcub;",L"{L"},// left curly bracket  345
{ L"&le;",L"<="},// less-than-or-equal  23
{ L"&lgr;",L"l"},// small lambda, Greek  104
{ L"&lowbar;",L"_"},// low line  134
{ L"&lsqb;",L"["},// left square bracket  34752
{ L"&Lstrok;",L"L"},// capital L, stroke  3
{ L"&lstrok;",L"l"},// small l, stroke  25
{ L"&Lt;",L"<<"},// double less-than sign  3
{ L"&lt;",L"<"},// less-than sign  2295
{ L"&lt<",L"<"},// less-than sign  2295 - added because of special newsbank processing in parseWord
{ L"&mdash;",L"-"},// em dash  275695
{ L"&Mgr;",L""},// capital Mu, Greek  1
{ L"&mgr;",L""},// small mu, Greek  376
{ L"&micro;",L""},// micro sign  1487
{ L"&middot;",L""},// middle dot  253
{ L"&nacute;",L"n"},// small n, acute accent  21
{ L"&natur;",L"n"},//  music natural  6
{ L"&ncaron;",L"n"},//  small n, caron  27
{ L"&ncedil;",L"n"},//  small n, cedilla  2
{ L"&ndash;",L"-"},//  en dash  43489
{ L"&ngr;",L"n"},//  small nu, Greek  88
{ L"&Ntilde;",L"N"},//  capital N, tilde  4
{ L"&ntilde;",L"n"},//  small n, tilde  771
{ L"&num;",L"#"},// number sign  138
{ L"&Oacute;",L"O"},// capital O, acute accent  17
{ L"&oacute;",L"o"},// small o, acute accent  1328
{ L"&Ocirc;",L"O"},// capital O, circumflex accent  7
{ L"&ocirc;",L"o"},// small o, circumflex accent  754
{ L"&OElig;",L"O"},// capital OE ligature  2
{ L"&oelig;",L"o"},// small oe ligature  55
{ L"&Ogr;",L"O"},// capital Omicron, Greek  11
{ L"&ogr;",L"o"},// small omicron, Greek  52
{ L"&ograve;",L"o"},// small o, grave accent  73
{ L"&OHgr;",L"O"},// capital Omega, Greek  1
{ L"&ohgr;",L"o"},// small omega, Greek  23
{ L"&ohm;",L"o"},// ohm sign  15
{ L"&omacr;",L"o"},// small o, macron  4
{ L"&Oslash;",L"O"},// capital O, slash  16
{ L"&oslash;",L"o"},// small o, slash  304
{ L"&Otilde;",L"O"},// capital O, tilde  1
{ L"&otilde;",L"o"},// small o, tilde  4
{ L"&Ouml;",L"O"},// capital O, dieresis or umlaut mark  344
{ L"&ouml;",L"o"},// small o, dieresis or umlaut mark  1284
{ L"&percnt;",L"%"},// percent sign  144
{ L"&Pgr;",L""},// capital Pi, Greek  37
{ L"&pgr;",L""},// small pi, Greek  95
{ L"&PHgr;",L""},// capital Phi, Greek  9
{ L"&phgr;",L""},// small phi, Greek  107
{ L"&plus;",L"+"},// plus sign  198
{ L"&plusmn;",L"+-"},// plus-or-minus sign  122
{ L"&pound;",L"#"},// pound sign  71698
{ L"&Prime;",L"''"},// double prime or second  59
{ L"&prime;",L"'"},// prime or minute  128
{ L"&PSgr;",L""},// capital Psi, Greek  15
{ L"&psgr;",L""},// small psi, Greek  16
{ L"&quot;",L"'"},// quotation mark  142126
{ L"&racute;",L"r"},//  small r, acute accent  3
{ L"&radic;",L""},//  surd =radical (square root)  9
{ L"&rarr;",L""},//  rightward arrow  182
{ L"&Rcaron;",L"R"},//  capital R, caron  1
{ L"&rcaron;",L"r"},//  small r, caron  103
{ L"&rcub;",L"}"},//  right curly bracket  343
{ L"&reg;",L"R"},//  registered sign  11
{ L"&rehy;",L"-"},//  maps to soft hyphen  3905
{ L"&rgr;",L""},//  small rho, Greek  81
{ L"&rsqb;",L"]"},//  right square bracket  34807
{ L"&Sacute;",L"S"},//  capital S, acute accent  13
{ L"&sacute;",L"s"},//  small s, acute accent  24
{ L"&Scaron;",L"S"},//  capital S, caron  85
{ L"&scaron;",L"s"},//  small s, caron  257
{ L"&Scedil;",L"S"},//  capital S, cedilla  7
{ L"&scedil;",L"s"},//  small s, cedilla  465
{ L"&scirc;",L"s"},//  small s, circumflex accent  14
{ L"&sect;",L"-"},//  section sign  52
{ L"&Sgr;",L"S"},//  capital Sigma, Greek  13
{ L"&sgr;",L"s"},//  small sigma, Greek  150
{ L"&sharp;",L"#"},//  musical sharp  93
{ L"&shilling;",L""},//  British shilling  228
{ L"&sim;",L"~"},//  similar  68
{ L"&sol;",L""},//  solidus  355
{ L"&sup1;",L"1"},//  superscript one  2
{ L"&sup2;",L"2"},//  superscript two  45
{ L"&sup3;",L"3"},//  superscript three  10
{ L"&szlig;",L"s"},//  small sharp s, German (sz ligature)  19
{ L"&tcaron;",L"t"},//  small t, caron  1
{ L"&tcedil;",L"t"},//  small t, cedilla  26
{ L"&tgr;",L""},//  small tau, Greek  68
{ L"&THgr;",L""},//  capital Theta, Greek  13
{ L"&thgr;",L""},//  small theta, Greek  193
{ L"&THORN;",L""},//  capital THORN, Icelandic  15
{ L"&thorn;",L""},//  small thorn, Icelandic  13
{ L"&times;",L"x"},//  multiply sign  2300
{ L"&trade;",L"tm"},//  trade mark sign  12
{ L"&Uacute;",L"U"},// capital U, acute accent  7
{ L"&uacute;",L"u"},// small u, acute accent  326
{ L"&Ucirc;",L"U"},// capital U, circumflex accent  1
{ L"&ucirc;",L"u"},// small u, circumflex accent  107
{ L"&Ugr;",L"U"},// capital Upsilon, Greek  1
{ L"&ugr;",L"u"},// small upsilon, Greek  3
{ L"&ugrave;",L"u"},// small u, grave accent  38
{ L"&umacr;",L"u"},// small u, macron  3
{ L"&uml;",L"u"},// umlaut mark  3
{ L"&uring;",L"u"},// small u, ring  9
{ L"&Uuml;",L"U"},// capital U, dieresis or umlaut mark  25
{ L"&uuml;",L"u"},// small u, dieresis or umlaut mark  2246
{ L"&verbar;",L"-"},// vertical bar  269
{ L"&wcirc;",L"w"},// small w, circumflex accent  2
{ L"&xgr;",L"x"},// small xi, Greek  8
{ L"&yacute;",L"y"},// small y, acute accent  138
{ L"&Ycirc;",L"Y"},// capital Y, circumflex accent  1
{ L"&ycirc;",L"y"},// small y, circumflex accent  6
{ L"&yen;",L""},// yen sign  120
{ L"&Yuml;",L"Y"},// capital Y, dieresis or umlaut mark  2
{ L"&yuml;",L"y"},// small y, dieresis or umlaut mark  47
{ L"&zacute;",L"z"},// small z, acute accent  3
{ L"&Zcaron;",L"Z"},// capital Z, caron  9
{ L"&zcaron;",L"z"},// small z, caron  67
{ L"&zdot;",L"z"},// small z, dot above  1
{ L"&Zgr;",L"Z"},// capital Zeta, Greek  3
{ L"&zgr;",L"z"},// small zeta, Greek  33
{ NULL,NULL }
};

void translateBuffer(wchar_t *buffer,wstring location)
{
  size_t afterXMLIndex=0;
  for (unsigned int I=0; buffer[I]; )
    if (buffer[I]!=L'&' || (I && buffer[I-1]==L'\''))
    {
      if (afterXMLIndex!=I)
        buffer[afterXMLIndex]=buffer[I];
      afterXMLIndex++;
      I++;
    }
    else
    {
      wchar_t *ch;
      if (!(ch=wcschr(buffer+I,L';')))
      {
        lplog(L"illegal SGML string encountered in %s: %.60s",location.c_str(),buffer+I);
        buffer[afterXMLIndex++]=buffer[I++];
        continue;
      }
      size_t len=ch-(buffer+I)+1;
      unsigned int w;
      for (w=0; bncChars[w].SGML && wcsncmp(buffer+I,bncChars[w].SGML,len); w++);
      if (bncChars[w].SGML==NULL)
      {
        lplog(L"illegal SGML string encountered in %s (2): %.60s",location.c_str(),buffer+I);
        buffer[afterXMLIndex++]=buffer[I++];
        continue;
      }
      wcscpy(buffer+afterXMLIndex,bncChars[w].substitute);
      afterXMLIndex+=wcslen(bncChars[w].substitute);
      I+=wcslen(bncChars[w].SGML);
    }
  buffer[afterXMLIndex]=0;
}

#define TAG_NOT_SET -1
struct
{
  wchar_t *tag;
  wchar_t *sForm;
  int form;
  int inflection;
} tagList[]=
{
  // Total number of wordclass tags in the BNC basic tagset = 57, plus 4 punctuation tags
  {L"AJ0",L"adjective",0,ADJECTIVE_NORMATIVE}, // Adjective (general or positive) (e.g. good, old, beautiful)
  {L"AJC",L"adjective",0,ADJECTIVE_COMPARATIVE}, // Comparative adjective (e.g. better, older)
  {L"AJS",L"adjective",0,ADJECTIVE_SUPERLATIVE}, // Superlative adjective (e.g. best, oldest)
  {L"AT0",L"determiner//demonstrative_determiner//possessive_determiner//interrogative_determiner//quantifier",0,0}, // Article (e.g. the, a, an, no, every, )
  {L"AV0",L"adverb//predeterminer//time//abbreviation//time_abbreviation//date_abbreviation",0,0}, // General adverb: an adverb not subclassified as AVP or AVQ (see below) (e.g. often, well, longer (adv.), furthest. OR abbreviations like ie, i.e. ibid etc
  {L"AVP",L"adverb",0,0}, // Adverb particle (e.g. up, off, out)
  {L"AVQ",L"relativizer//interrogative_pronoun",0,0}, // Wh-adverb (e.g. when, where, how, why, wherever)
  {L"CJC",L"coordinator//conjunction",0,0}, // Coordinating conjunction (e.g. and, or, but) - 'but' should not be a coordinator (2/2/2007) Jack but Jill went up the hill.
  {L"CJS",L"conjunction//coordinator//preposition//relativizer//adverb",0,0}, // Subordinating conjunction (e.g. although, when)
  {L"CJT",L"relativizer",0,0}, // The subordinating conjunction that
  {L"CRD",L"numeral_cardinal//numeral_ordinal//Number//roman_numeral//date//time",0,0}, // Cardinal number (e.g. one, 3, fifty-five, 3609, '90s)
  {L"DPS",L"possessive_determiner",0,0}, // Possessive determiner-pronoun (e.g. your, their, his)
  {L"DT0",L"pronoun//quantifier//demonstrative_determiner//predeterminer//adjective",0,0}, // General determiner-pronoun: i.e. a determiner-pronoun which is not a DTQ or an AT0.
  {L"DTQ",L"relativizer//interrogative_determiner//interrogative_pronoun",0,0}, // Wh-determiner-pronoun (e.g. which, what, whose, whichever)
  {L"EX0",NULL,0,0}, // Existential there, i.e. there occurring in the there is ... or there are ... construction
  {L"ITJ",L"interjection",0,0}, // Interjection or other isolate (e.g. oh, yes, mhm, wow)
  {L"NN0",L"noun",0,0}, // Common noun, neutral for number (e.g. aircraft, data, committee)
  {L"NN1",L"noun",0,SINGULAR}, // Singular common noun (e.g. pencil, goose, time, revelation)
  {L"NN2",L"noun",0,PLURAL}, // Plural common noun (e.g. pencils, geese, times, revelations)
  {L"NP0",PROPER_NOUN_FORM,0,0}, // Proper noun (e.g. London, Michael, Mars, IBM)
  {L"ORD",L"numeral_ordinal",0,0}, // Ordinal numeral (e.g. first, sixth, 77th, last) .
  {L"PNI",NULL,0,0}, // Indefinite pronoun (e.g. none, everything, one [as pronoun], nobody)
  {L"PNP",L"personal_pronoun_nominative//personal_pronoun//possessive_pronoun//personal_pronoun_accusative",0,0}, // Personal pronoun (e.g. I, you, them, ours)
  {L"PNQ",NULL,0,0}, // Wh-pronoun (e.g. who, whoever, whom)
  {L"PNX",NULL,0,0}, // Reflexive pronoun (e.g. myself, yourself, itself, ourselves)
  {L"POS",L"COMBINE",0,0}, // The possessive or genitive marker 's or '
  {L"PRF",NULL,0,0}, // The preposition of
  {L"PRP",L"preposition",0,0}, // Preposition (except for of) (e.g. about, at, in, on, on behalf of, with)
  {L"PUL",NULL,0,0}, // Punctuation: left bracket - i.e. ( or [
  {L"PUN",NULL,0,0}, // Punctuation: general separating mark - i.e. . , ! , : ; - or ?
  {L"PUQ",NULL,0,0}, // Punctuation: quotation mark - i.e. ' or \"
  {L"PUR",NULL,0,0}, // Punctuation: right bracket - i.e. ) or ]
  {L"TO0",L"to//preposition",0,0}, // Infinitive marker to
  {L"UNC",NULL,0,0}, // Unclassified items which are not appropriately considered as items of the English lexicon.
  {L"VBB",L"is",0,0}, // The present tense forms of the verb BE, except for is, 's: i.e. am, are, 'm, 're and be [subjunctive or imperative]
  {L"VBD",L"is",0,0}, // The past tense forms of the verb BE: was and were
  {L"VBG",NULL,0,0}, // The -ing form of the verb BE: being
  {L"VBI",NULL,0,0}, // The infinitive form of the verb BE: be
  {L"VBN",NULL,0,0}, // The past participle form of the verb BE: been
  {L"VBZ",L"is",0,0}, // The -s form of the verb BE: is, 's
  {L"VDB",L"does",0,0}, // The finite base form of the verb DO: do
  {L"VDD",L"does",0,0}, // The past tense form of the verb DO: did
  {L"VDG",L"does",0,0}, // The -ing form of the verb DO: doing
  {L"VDI",NULL,0,0}, // The infinitive form of the verb DO: do
  {L"VDN",L"does",0,0}, // The past participle form of the verb DO: done
  {L"VDZ",L"does",0,0}, // The -s form of the verb DO: does, 's
  {L"VHB",L"have",0,0}, // The finite base form of the verb HAVE: have, 've
  {L"VHD",L"have",0,0}, // The past tense form of the verb HAVE: had, 'd
  {L"VHG",L"have",0,0}, // The -ing form of the verb HAVE: having
  {L"VHI",L"have",0,0}, // The infinitive form of the verb HAVE: have
  {L"VHN",L"have",0,0}, // The past participle form of the verb HAVE: had
  {L"VHZ",L"have",0,0}, // The -s form of the verb HAVE: has, 's
  {L"VM0",L"modal_auxiliary//future_modal_auxiliary//verbverb",0,0}, // Modal auxiliary verb (e.g. will, would, can, could, 'll, 'd) or "let",L"dare"
  {L"VVB",L"verbverb//verb//SYNTAX:Accepts S as Object",0,0}, // The finite base form of lexical verbs (e.g. forget, send, live, return) [Including the imperative and present subjunctive]
  {L"VVD",L"verbverb//verb//SYNTAX:Accepts S as Object",0,VERB_PAST}, // The past tense form of lexical verbs (e.g. forgot, sent, lived, returned)
  {L"VVG",L"verbverb//verb//SYNTAX:Accepts S as Object",0,VERB_PRESENT_PARTICIPLE}, // The -ing form of lexical verbs (e.g. forgetting, sending, living, returning)
  {L"VVI",L"verbverb//verb//SYNTAX:Accepts S as Object",0,0}, // The infinitive form of lexical verbs (e.g. forget, send, live, return)
  {L"VVN",L"verbverb//verb//SYNTAX:Accepts S as Object",0,VERB_PAST_PARTICIPLE}, // The past participle form of lexical verbs (e.g. forgotten, sent, lived, returned)
  {L"VVZ",L"verbverb//verb//SYNTAX:Accepts S as Object",0,VERB_PRESENT_THIRD_SINGULAR}, // The -s form of lexical verbs (e.g. forgets, sends, lives, returns)
  {L"XX0",L"COMBINE",0,0}, // The negative particle not or n't
  {L"ZZ0",NULL,0,0}, // Alphabetical symbols (e.g. A, a, B, b, c, d)
  };
// 30 "Ambiguity Tags", applied wherever the probabilities assigned by the CLAWS automatic tagger to its first and second choice tags
//                      were considered too low for reliable disambiguation. So, for example, the ambiguity tag AJ0-AV0 indicates that the
//                      choice between adjective (AJ0) and adverb (AV0) is left open, although the tagger has a preference for an adjective reading.
//                      The mirror tag, AV0-AJ0, again shows adjective-adverb ambiguity, but this time the more likely reading is the adverb.
// Total number of wordclass tags including punctuation and ambiguity tags = 91.
/*
select w.word,f.name from wordforms wf,words w,forms f where wf.wordId=w.id and w.word in ("insofar","albeit","although","because","cos","even","given","immediately","like","now","though","whereupon","whether","while") and wf.formId<32750 and f.id=wf.formId;
*/
struct
{
  wchar_t *tag;
  wchar_t *tag2;
} ambTagList[]=
{
  {L"AJ0",L"NN1"},
  {L"AJ0",L"VVD"},
  {L"AJ0",L"VVG"},
  {L"AJ0",L"VVN"},
  {L"AV0",L"AJ0"},
  {L"AVP",L"PRP"},
  {L"AVQ",L"CJS"},
  {L"CJS",L"AVQ"},
  {L"CJS",L"PRP"},
  {L"CJT",L"DT0"},
  {L"CRD",L"PNI"},
  {L"DT0",L"CJT"},
  {L"NN1",L"AJ0"},
  {L"NN1",L"NP0"},
  {L"NN1",L"VVB"},
  {L"NN1",L"VVG"},
  {L"NN2",L"VVZ"},
  {L"NP0",L"NN1"},
  {L"PNI",L"CRD"},
  {L"PRP",L"AVP"},
  {L"PRP",L"CJS"},
  {L"VVB",L"NN1"},
  {L"VVD",L"AJ0"},
  {L"VVD",L"VVN"},
  {L"VVG",L"AJ0"},
  {L"VVG",L"NN1"},
  {L"VVN",L"AJ0"},
  {L"VVN",L"VVD"},
  {L"VVZ",L"NN2"}
};

bool bncc::findMultiplePreferredForm(vector <WordMatch>::iterator im,int tag,const wchar_t *location,int sentence,int &f,bool reportNotFound)
{
  f=-1;
  wstring sForm=tagList[tag].sForm,nextForm;
  int where=0;
  bool foundForm=false;
  while (true)
  {
    size_t next=sForm.find(L"//",where);
    if (next!=wstring::npos)
      nextForm=sForm.substr(where,next-where);
    else
      nextForm=sForm.substr(where,sForm.length()-where);
    int tf=FormsClass::findForm(nextForm);
    if (tf<0)
      lplog(LOG_FATAL_ERROR,L"Form %s not found in BNC processing.",nextForm.c_str());
    if (im->word->second.query(tf)>=0)
    {
      foundForm=true;
      im->forms.set(tf);
      f=tf;
    }
    else if ((tf==adjectiveForm || tf==verbForm || tf==nounForm) && f==-1)
      f=tf;
    where=next+2;
    if (next==wstring::npos) break;
  }
  if (foundForm) return true;
  if (im->word->second.isUnknown() || (sForm.find(L"determiner")!=wstring::npos && sForm.find(L"predeterminer")==wstring::npos))
  {
    where=0;
    int next=sForm.find(L"//",where);
    nextForm=sForm.substr(where,next-where);
    int nf=FormsClass::findForm(nextForm);
    if (nf<0)
      lplog(LOG_FATAL_ERROR,L"Form %s not found in BNC processing.",nextForm.c_str());
    im->word->second.addForm(nf,im->word->first);
    lplog(L"Form string %s assigned to unknown word %s (%s #%d)",nextForm.c_str(),im->word->first.c_str(),location,sentence);
    im->forms.set(nf);
    return true;
  }
  else
  {
    wchar_t *confusingWords[]={L"here",L"however",L"never",L"no",L"need",L"used",NULL};
    for (unsigned int I=0; confusingWords[I]; I++)
      if (im->word->first==confusingWords[I]) return false;
    // a time like 5pm is legal
    if (im->word->first.length()==1 && im->word->first[0]>=L'1' && im->word->first[0]<=L'9' && sForm.find(L"adverb")!=wstring::npos)
      return false;
    if (reportNotFound)
      lplog(L"No forms from form string %s found for word %s (%s #%d)",sForm.c_str(),im->word->first.c_str(),location,sentence);
    return false;
  }
}

int bncc::findPreferredForm(vector <WordMatch>::iterator im,int tag,bool optional,const wchar_t *location,int sentence,bool depositPreferences,bool reportNotFound)
{
  if (tag==TAG_NOT_SET)
  {
    im->flags|=WordMatch::flagBNCFormNotCertain;
    return -1;
  }
  int f=tagList[tag].form;
  if (f==-3 && findMultiplePreferredForm(im,tag,location,sentence,f,reportNotFound)) return f;
  if (f<0 || im->word->first.length()==1) return -1;
  if (im->word->second.query(f)>=0) return f;
  if (optional || !depositPreferences) return -31;
  if (f==adjectiveForm)
  {
    im->flags|=WordMatch::flagBNCPreferAdjectivePatternMatch;
    // refuse adding adjective to:
    // "verb*1",
    if (im->word->second.query(verbForm)>=0 && (im->word->second.inflectionFlags&(VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE))!=0)
    {
      #ifdef LOG_BNC_PATTERNS_CHECK
        lplog(L"Adding pattern preference (1) for %s (%s) to word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
      #endif
      return -32;
    }
    //  "numeral_ordinal",L"Number",L"ex"
    if (im->word->second.query(numeralOrdinalForm)>=0 || im->word->second.query(numberForm)>=0)
    {
      #ifdef LOG_BNC_PATTERNS_CHECK
        lplog(L"Adding pattern preference (2)  for %s (%s) to word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
      #endif
      return -33;
    }
    if (im->word->second.query(nounForm)>=0 && (im->word->second.inflectionFlags&(SINGULAR_OWNER|PLURAL_OWNER))!=0)
    {
      #ifdef LOG_BNC_PATTERNS_CHECK
        lplog(L"Adding pattern preference (3)  for %s (%s) to word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
      #endif
      return -34;
    }
    im->flags&=~WordMatch::flagBNCPreferAdjectivePatternMatch;
  }
  if (f==nounForm)
  {
    // refuse adding noun to
    // ordinal, number, cardinal,"quantifier",L"time",L"date",abbreviations,months
    if (im->word->second.query(numeralOrdinalForm)>=0 || im->word->second.query(numberForm)>=0 || im->word->second.query(numeralCardinalForm)>=0 ||
      im->word->second.query(quantifierForm)>=0 || im->word->second.query(timeForm)>=0 ||
      im->word->second.query(dateForm)>=0 || im->word->second.query(abbreviationForm)>=0 || im->word->second.query(sa_abbForm)>=0 ||
      im->word->second.query(telephoneNumberForm)>=0 ||
      im->word->second.query(monthForm)>=0)
      return -35;
    im->flags|=WordMatch::flagBNCPreferNounPatternMatch;
    // refuse adding noun to
    // "verb*1",L"does*1",VERB_PRESENT_PARTICIPLE
    if ((im->word->second.query(verbForm)>=0 || im->word->second.query(doesForm)>=0) &&
      (im->word->second.inflectionFlags&(VERB_PRESENT_PARTICIPLE))!=0)
    {
      #ifdef LOG_BNC_PATTERNS_CHECK
        lplog(L"Adding pattern preference (5)  for %s (%s) to word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
      #endif
      return -36;
    }
    im->flags&=~WordMatch::flagBNCPreferNounPatternMatch;
  }
  if (letterForm<0) letterForm=FormsClass::gFindForm(L"letter");
  // refuse adding Proper Noun to:
  // letters
  if (f==PROPER_NOUN_FORM_NUM && im->word->second.query(letterForm)>=0)
    return -37;
  // refuse adding adverb to
  // Number
  if (f==adverbForm)
  {
    im->flags|=WordMatch::flagBNCPreferAdverbPatternMatch;
    if (im->word->second.query(numberForm)>=0)
    {
      #ifdef LOG_BNC_PATTERNS_CHECK
        lplog(L"Adding pattern preference (6) for %s (%s) to word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
      #endif
      return -38;
    }
    if (im->word->first==L"however" || im->word->first==L"etc")
    {
      #ifdef LOG_BNC_PATTERNS_CHECK
        lplog(L"Adding pattern preference (7) for %s (%s) to word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
      #endif
      return -41;
    }
    im->flags&=~WordMatch::flagBNCPreferAdverbPatternMatch;
  }
  // refuse adding modal_auxiliary to
  // future_modal_auxiliary, negation_modal_auxiliary, negation_future_modal_auxiliary
  if (!wcscmp(tagList[tag].sForm,L"modal_auxiliary"))
  {
    if (im->word->second.query(futureModalAuxiliaryForm)>=0 || im->word->second.query(negationModalAuxiliaryForm)>=0 ||
      im->word->second.query(negationFutureModalAuxiliaryForm)>=0)
      return -39;
  }
  if (!wcscmp(tagList[tag].sForm,L"is") && im->word->first==L"be")
    return -40;
  // when adding PROPER_NOUN_FORM_NUM, add flags flagAddProperNoun
  if (f==PROPER_NOUN_FORM_NUM) im->flags|=WordMatch::flagAddProperNoun;
  else
  {
    if (f==adjectiveForm) im->flags|=WordMatch::flagBNCPreferAdjectivePatternMatch;
    else if (f==nounForm) im->flags|=WordMatch::flagBNCPreferNounPatternMatch;
    else if (f==adverbForm) im->flags|=WordMatch::flagBNCPreferAdverbPatternMatch;
    else if (f==verbForm) im->flags|=WordMatch::flagBNCPreferVerbPatternMatch;
    else
    {
      if (f==prepositionForm && im->word->second.query(verbForm)>=0 && (im->word->second.inflectionFlags&(VERB_PRESENT_PARTICIPLE)))
      {
          int vp=FormsClass::gFindForm(L"verbalPreposition");
        if (vp<0) lplog(LOG_FATAL_ERROR,L"verbalPreposition form not found.");
        if (im->word->second.query(vp)<0) im->word->second.addForm(vp,im->word->first);
        return -43;
      }
      if (f==numeralOrdinalForm && im->word->second.query(NUMBER_FORM_NUM)>=0)
        return -1; // for example, the number 25
      if (im->word->first!=L"dare" && im->word->first!=L"let" && im->word->first!=L"need" && im->word->first!=L"ought" && im->word->first!=L"used")
        lplog(L"Illegal BNC preference for %s (%s) encountered at word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
      return -41;
    }
    #ifdef LOG_BNC_PATTERNS_CHECK
      lplog(L"Adding pattern preference (8) for %s (%s) to word %s (%s #%d).",tagList[tag].sForm,tagList[tag].tag,im->word->first.c_str(),location,sentence);
    #endif
    if (f==nounForm && im->word->second.query(adjectiveForm)>=0)
    {
      im->word->second.addForm(nounForm,im->word->first);
      lplog(L"Added noun form to adjective %s (%s #%d).",im->word->first.c_str(),location,sentence);
      return f;
    }
    return -42;
  }
  return f;
}

void printWord(tIWMM iWord,int flags,int &printLocation,bool firstWordInSentence)
{
  wchar_t temp[100];
  size_t len=iWord->first.length();
  wcscpy(temp,iWord->first.c_str());
  if ((flags&(WordMatch::flagAddProperNoun|WordMatch::flagOnlyConsiderProperNounForms)) || firstWordInSentence)
    *temp=towupper(*temp);
  if (flags&(WordMatch::flagNounOwner))
  {
    temp[len++]=L'\'';
    if (temp[len-2]!=L's') temp[len++]=L's';
  }
  if (flags&WordMatch::flagAllCaps)
    for (wchar_t *ch=temp; *ch && (ch-temp)<100; ch++) *ch=towupper(*ch);
  if (printLocation>100)
  {
    wprintf(L"\n");
    printLocation=0;
  }
  wprintf(L"%s ",temp);
  printLocation+=wcslen(temp)+1;
}

void WordMatch::setPreferredForm(void)
{
  if (flags&WordMatch::flagOnlyConsiderProperNounForms)
  {
    forms.set(PROPER_NOUN_FORM_NUM);
    for (unsigned int I=0; I<word->second.formsSize(); I++)
      if (word->second.Form(I)->properNounSubClass)
        forms.set(word->second.forms()[I]);
    return;
  }
  if (flags&WordMatch::flagAddProperNoun)
    forms.set(PROPER_NOUN_FORM_NUM);
  // determine highest use pattern count
  /* removed - not reliable
  int up=-1,whichForm=-1;
  if (word->second.isUnknown())
    for (unsigned int I=0; I<word->second.formsSize(); I++)
    {
      if (up<word->second.usagePatterns[I])
      {
        up=word->second.usagePatterns[I];
        whichForm=I;
      }
    }
  if (whichForm>=0)
    forms.set(word->second.forms()[whichForm]);
  else
  */
    for (unsigned int f=0,*fp=word->second.forms(),*fpEnd=word->second.forms()+word->second.formsSize(); fp!=fpEnd; fp++,f++)
      forms.set(*fp);
  if (!(flags&WordMatch::flagFirstLetterCapitalized) || (flags&WordMatch::flagRefuseProperNoun))
    forms.reset(PROPER_NOUN_FORM_NUM);
}

void WordMatch::setForm(void)
{
  // adjust words that are honorifics that are capitalized so that they are only recognized as honorifics
  if (flags&WordMatch::flagOnlyConsiderOtherNounForms)
  {
		// since it is a determiner, and also capitalized, the flagOnlyConsiderOtherNounForms was set, since we do not usually want a determiner to be considered a proper noun.
		// HOWEVER, the word no is both a determiner, which has block proper noun on it, and an abbreviation, which is a proper noun subclass.  In this case, recognize it as an abbreviation.
		if (word->first == L"no")
		{
			forms.set(abbreviationForm);
		}
		else
			for (unsigned int I=0; I<word->second.formsSize(); I++)
				if (word->second.Form(I)->blockProperNounRecognition)
					forms.set(word->second.forms()[I]);
    return;
  }
  if (flags&WordMatch::flagOnlyConsiderProperNounForms)
  {
    forms.set(PROPER_NOUN_FORM_NUM);
    for (unsigned int I=0; I<word->second.formsSize(); I++)
      if (word->second.Form(I)->properNounSubClass)
        forms.set(word->second.forms()[I]);
    return;
  }
  if (flags&WordMatch::flagAddProperNoun)
    forms.set(PROPER_NOUN_FORM_NUM);
  // if something has a 's after it, it can only be a noun
  if (queryForm(nounForm)>=0 && (flags&WordMatch::flagNounOwner)!=0)
  {
    for (unsigned int f=0,*fp=word->second.forms(),*fpEnd=word->second.forms()+word->second.formsSize(); fp!=fpEnd; fp++,f++)
      if (Forms[*fp]->properNounSubClass || *fp==nounForm || *fp==PROPER_NOUN_FORM_NUM)
        forms.set(*fp);
    return;
  }
  for (unsigned int f=0,*fp=word->second.forms(),*fpEnd=word->second.forms()+word->second.formsSize(); fp!=fpEnd; fp++,f++)
	{
		if (*fp>=Forms.size())
		{
			lplog(LOG_ERROR,L"Illegal form #%d found in word %s.",*fp,word->first.c_str());
			*fp=0;
		}
		else
			forms.set(*fp);
	}
  if (!(flags&WordMatch::flagFirstLetterCapitalized) || (flags&WordMatch::flagRefuseProperNoun))
    forms.reset(PROPER_NOUN_FORM_NUM);
}


int bncc::processWord(Source &source,int sourceId,wchar_t *buffer,int tag,int secondTag,int &lastSentenceEnd,int &printLocation,int sentence)
{
  bool anotherWord=false;
  wstring sWord,comment;
  int nounOwner=0;
  size_t numWords=source.m.size();
  __int64 bufferScanLocation=0;
  if (buffer[wcslen(buffer)-1]==L' ') buffer[wcslen(buffer)-1]=0;
  if (!wcscmp(buffer,L"-"))
    anotherWord=false;
  if (!wcscmp(L"(s)",buffer+wcslen(buffer)-3)) // dilemma(s) member(s) finger(s) just delete the (s) for now
    buffer[wcslen(buffer)-3]=0;
  int result=Words.readWord((wchar_t *)buffer,wcslen(buffer),bufferScanLocation,sWord,comment,nounOwner,false,false,source.debugTrace);
  if (buffer[bufferScanLocation]==L'.')
  {
    // only assigns the whole buffer if every entry is a single letter
    unsigned int I;
    for (I=0; buffer[I]; I+=2)
    {
      if (buffer[I]<0 || !iswalpha(buffer[I]))
        break;
      if (!buffer[I+1] || buffer[I+1]!=L'.')
        break;
    }
    if (!buffer[I])
    {
      sWord=buffer;
      if (buffer[wcslen(buffer)-1]!=L'.')
        sWord+=L".";
    }
  }
  if (result==PARSE_EOF && !wcsncmp(buffer,L"~~END",wcslen(L"~~END")))
    return PARSE_EOF;
  // drinkin' and swearin'
  if (buffer[bufferScanLocation]==L'\'' && bufferScanLocation>2 && buffer[bufferScanLocation-1]==L'n' && buffer[bufferScanLocation-2]==L'i')
  {
    if (Words.query(sWord)==Words.end())
      sWord+=L'g';
    if (!buffer[bufferScanLocation+1])
      buffer[bufferScanLocation]=0;
  }
  // bleed'n
  if (buffer[bufferScanLocation-2]==L'\'' && buffer[bufferScanLocation-1]==L'n' && Words.query(sWord)==Words.end())
    sWord+=L"ing";
  if (!wcscmp(buffer,L"'em"))
  {
    sWord=L"'em";
    bufferScanLocation=3;
  }
  if (sWord[0]==L'\'' && tag!=TAG_NOT_SET)
  {
    if (!wcscmp(tagList[tag].tag,L"VBB") && towlower(sWord[1])==L'm')
      sWord=L"am";
    if (!wcscmp(tagList[tag].tag,L"PNP") && towlower(sWord[1])==L'e' && towlower(sWord[2])==L'm')
      sWord=L"them";
    if (!wcscmp(tagList[tag].tag,L"VBB") && towlower(sWord[1])==L'r')
      sWord=L"are";
    if (!wcscmp(tagList[tag].tag,L"VBZ"))
      sWord=L"is";
    if (!wcscmp(tagList[tag].tag,L"VDZ"))
      sWord=L"does";
    if (!wcscmp(tagList[tag].tag,L"VHB"))
      sWord=L"have";
    if (!wcscmp(tagList[tag].tag,L"VHD"))
      sWord=L"had";
    if (!wcscmp(tagList[tag].tag,L"VHZ"))
      sWord=L"has";
    if (!wcscmp(tagList[tag].tag,L"VM0") && towlower(sWord[1])==L'l')
      sWord=L"will";
    if (!wcscmp(tagList[tag].tag,L"VM0") && towlower(sWord[1])==L'd')
      sWord=L"would";
    if (!wcscmp(tagList[tag].tag,L"PNP") && towlower(sWord[1])==L't')
      sWord=L"it";
  }
  if (sWord==L"wouldhad" && source.m[source.m.size()-1].word->first==L"wou")
  {
    source.m[source.m.size()-1].word=Words.gquery(L"would");
    source.m[source.m.size()-1].forms.clear();
    source.m[source.m.size()-1].forms.set(FormsClass::gFindForm(L"modal_auxiliary"));
    return 0;
  }
  if (!wcsicmp(sWord.c_str(),L"aged") && tag!=TAG_NOT_SET && !wcscmp(tagList[tag].tag,L"PRP"))
  {
    unsigned int I=0;
    for (; I<sizeof(tagList)/sizeof(*tagList) && wcscmp(tagList[I].tag,L"VVN"); I++);
    tag=I;
  }
  if ((!wcsicmp(sWord.c_str(),L"t") || !wcsicmp(buffer,L"'t")) && tag!=TAG_NOT_SET && !wcscmp(tagList[tag].tag,L"PNP"))
  {
    sWord=L"it";
    if (buffer[0]==L'\'') bufferScanLocation++;
  }
  //if (!wcscmp(tagList[tag].tag,"XX0") && !wcsicmp(buffer,"n't") && (!source.m.size() || source.m[source.m.size()-1].word->first!="is"))
  //{
  //  sWord="can't";
  //  bufferScanLocation+=2; // advance to end of word
  //}
  if (sWord[0]==L'-' && sWord[1]) sWord.erase(sWord.begin()+0);
  wchar_t *dash=(wchar_t *)wcschr(sWord.c_str(),L'-');
  // catch a word which has a period as its last letter, is not already an abbreviation, has no more than one period, is not found and is found without its period
  if (sWord[sWord.length()-1]==L'.' && sWord.length()>3 && sWord.find(L'.')==sWord.length()-1 && Words.query(sWord)==Words.end() && Words.query(sWord.substr(0,sWord.length()-1))!=Words.end())
    sWord.erase(sWord.length()-1,1);
  if (result==PARSE_EOF)
    return result;
  // test for valid dash words
  // a word must have at least one unknown sub-word, or any of its subwords capitalized
  bool firstLetterCapitalized=iswupper((wchar_t)sWord[0])!=0;
  bool insertDashes=false;
  // keep names like al-Jazeera or dashed words incorporating first words that are unknown like fierro-regni
  // preserve dashes by setting insertDashes to true.
  // Handle cases like 10-15 or 10-60,000 which are returned as PARSE_NUM
  if (dash!=NULL && (result==0 || sWord!=buffer)) // if (not date or number) OR not fully processed
  {
    if (sWord[0]==L'-' && sWord[1]==0)
    {
      source.m.push_back(WordMatch(Words.gquery(L"-"),0,source.debugTrace));
      source.m[source.m.size()-1].setForm();
      if (buffer[bufferScanLocation] && buffer[bufferScanLocation]!=L'*')
        return processWord(source,sourceId,buffer+bufferScanLocation,tag,secondTag,lastSentenceEnd,printLocation,sentence);
      return 0;
    }
    int p=0,unknownWords=0,capitalizedWords=0,numDashes=0;
    for (unsigned int I=0; I<bufferScanLocation; I++)
    {
      if (buffer[I]==L'-')
      {
        buffer[I]=0;
        if (Words.query(buffer+p)==Words.end())
          unknownWords++;
        if (p && iswupper(buffer[p]) && !iswupper(buffer[p+1]))  // al-Jazeera, not BALL-PLAYING, not Three-year
          capitalizedWords++;
        buffer[I]=L'-';
        p=I+1;
        numDashes++;
      }
      if (capitalizedWords>0) break;
    }
    if ((capitalizedWords==0 && unknownWords==0) || numDashes>1)
    {
      dash=wcschr(buffer,L'-');
      for (unsigned int I=0; buffer[I]; I++)
        if (buffer[I]==L'-') buffer[I]=L' ';
      bufferScanLocation=dash-buffer;
      insertDashes=true;
    }
    firstLetterCapitalized=(capitalizedWords>0);
  }
  if (insertDashes || (sWord!=buffer && result==0 && bufferScanLocation!=wcslen(buffer)))
  {
    if (buffer[bufferScanLocation]==L' ')
    {
        tag=secondTag=TAG_NOT_SET;
      buffer[bufferScanLocation]=0;
      if ((result=processWord(source,sourceId,buffer,tag,secondTag,lastSentenceEnd,printLocation,sentence))<0)
        return result;
      if (buffer[bufferScanLocation+1]==0)
        return 0;
      if (insertDashes)
      {
        source.m.push_back(WordMatch(Words.gquery(L"-"),0,source.debugTrace));
        source.m[source.m.size()-1].setForm();
      }
      return processWord(source,sourceId,buffer+bufferScanLocation+1,tag,secondTag,lastSentenceEnd,printLocation,sentence);
    }
    // A01(433):<s n="170"><w UNC>* <hi rend=bo><w AVQ>How <w VM0>can <w PNP>you <w VBI>be <w AJ0>sure<c PUN>?
    // get rid of asterisks marking notes
    if ((wcschr(buffer,L'\'') || buffer[0]==L'"' || buffer[bufferScanLocation]<0 || !iswalpha(buffer[bufferScanLocation])) && buffer[bufferScanLocation]!=L'*')
      anotherWord=true;
    //if (buffer[bufferScanLocation]==L'*')
    //  sWord=buffer; // dashes or something else embedded in the word
  }
  // this logic is copied in doQuotesOwnershipAndContractions
  bool firstWordInSentence=source.m.size()==lastSentenceEnd;
  if (numWords>=1)
    firstWordInSentence|=source.m[numWords-1].word==Words.sectionWord || source.m[numWords-1].word->first==L"--" /* Ulysses */||
    source.m[numWords-1].word->first==L":" /* Secret Adversary - Second month:  Promoted to drying aforesaid plates.*/||
    source.m[numWords-1].word->second.query(quoteForm)>=0 ||
    source.m[numWords-1].word->second.query(dashForm)>=0 ||
    source.m[numWords-1].word->second.query(bracketForm)>=0; // BNC A00 4.00 PM - We ...
  if (numWords>=2)
  {
    firstWordInSentence|=(numWords-3==lastSentenceEnd && source.m[numWords-2].word->second.query(quoteForm)>=0 && source.m[numWords-1].word->first==L"(");
   // Pay must be good.' (We might as well make that clear from the start.)
    firstWordInSentence|=(numWords-2==lastSentenceEnd && source.m[numWords-1].word->first==L"(");   // The bag dropped.  (If you didn't know).
  }
  // d'Armide d'Armitage l'Isle etc
  if (!firstLetterCapitalized && sWord[1]==L'\'' && sWord[2]>0 && iswupper(sWord[2]))
    firstLetterCapitalized=true;
  bool allCaps=Words.isAllUpper(sWord);
  wcslwr((wchar_t *)sWord.c_str());
  bool added;
  bool endSentence=result==PARSE_END_SENTENCE;
  if (endSentence && numWords && source.m[numWords-1].queryForm(L"letter")>=0)
    endSentence=false;
  tIWMM iWord=Words.end();
  if (result==PARSE_NUM)
    iWord=Words.addNewOrModify(NULL,sWord,0,NUMBER_FORM_NUM,0,0,L"",sourceId,added);
  else if (result==PARSE_PLURAL_NUM)
    iWord=Words.addNewOrModify(NULL, sWord,0,NUMBER_FORM_NUM,PLURAL,0,L"",sourceId,added);
  else if (result==PARSE_ORD_NUM)
    iWord=Words.addNewOrModify(NULL, sWord,0,numeralOrdinalForm,0,0,L"",sourceId,added);
  else if (result==PARSE_ADVERB_NUM)
    iWord=Words.addNewOrModify(NULL, sWord,0,adverbForm,0,0,sWord,sourceId,added);
  else if (result==PARSE_DATE)
    iWord=Words.addNewOrModify(NULL, sWord,0,dateForm,0,0,L"",sourceId,added);
  else if (result==PARSE_TIME)
    iWord=Words.addNewOrModify(NULL, sWord,0,timeForm,0,0,L"",sourceId,added);
  else if (result==PARSE_TELEPHONE_NUMBER)
    iWord=Words.addNewOrModify(NULL, sWord,0,telephoneNumberForm,0,0,L"",sourceId,added);
	else if (result == PARSE_MONEY_NUM)
		iWord = Words.addNewOrModify(NULL, sWord, 0, moneyForm, 0, 0, L"", sourceId, added);
	else if (result == PARSE_WEB_ADDRESS)
		iWord = Words.addNewOrModify(NULL, sWord, 0, webAddressForm, 0, 0, L"", sourceId, added);
	else if (result<0 && result != PARSE_END_SENTENCE)
    return -40;
  else
    if ((result=Words.parseWord(&source.mysql,sWord,iWord,firstLetterCapitalized,nounOwner, sourceId))<0)
    {
      lplog(L"Cannot parse word %s (%s #%d).",sWord.c_str(),source.storageLocation.c_str(),sentence);
      return 0;
  }
  // re-process if word with dash not found
  if (iWord->second.isUnknown() && !firstLetterCapitalized && dash!=NULL)
  {
    dash=wcschr(buffer,L'-');
    *dash=L' ';
      tag=secondTag=TAG_NOT_SET;
    return processWord(source,sourceId,buffer,tag,secondTag,lastSentenceEnd,printLocation,sentence);
  }
  if (iWord->second.isUnknown()) unknownCount++;
  unsigned __int64 flags;
  iWord->second.adjustFormsInflections(sWord,flags,firstWordInSentence,nounOwner,allCaps,firstLetterCapitalized);
  //source.m.push_back(WordMatch(iWord,flags));
  if (!wcsicmp(iWord->first.c_str(),L"ca") || !wcsicmp(iWord->first.c_str(),L"wo") || !wcsicmp(iWord->first.c_str(),L"sha"))
  {
    source.m.push_back(WordMatch(iWord,flags,source.debugTrace));
    return 0; // BNC has ca as the stem of n't, which is fixed in the next word.
  }
  //  ignore | and $, skip only in BNCC
  if (tag>=0 && !wcscmp(tagList[tag].tag,L"UNC") && (iWord->first[0]<0 || !iswalpha(iWord->first[0])))
    return 0;
  // make "both" possibly a predeterminer and more an adjective
  source.m.push_back(WordMatch(iWord,flags,source.debugTrace));
  vector <WordMatch>::iterator im=source.m.begin()+source.m.size()-1;
  int f2=-1,f=findPreferredForm(im,tag,false,source.storageLocation.c_str(),sentence,secondTag<0,secondTag<0);
  if (secondTag>=0) f2=findPreferredForm(im,secondTag,true,source.storageLocation.c_str(),sentence,false,f<0);
  if (f<0 && f2<0 && tag>=0 && (iWord->second.isUnknown() || !iWord->second.formsSize()) && tagList[tag].form>=0)
  {
    iWord->second.addForm(tagList[tag].form,im->word->first);
    f=tagList[tag].form;
  }
  if (f>=0 || f2>=0)
  {
    if (f>=0) im->forms.set(f);
    if (f2>=0) im->forms.set(f2);

    if (im->flags&WordMatch::flagOnlyConsiderProperNounForms)
    {
      im->flags|=WordMatch::flagAddProperNoun;
      im->flags&=~WordMatch::flagOnlyConsiderProperNounForms;
    }
    if (im->flags&WordMatch::flagAddProperNoun)
    {
      im->forms.set(PROPER_NOUN_FORM_NUM);
      if (im->word->second.query(PROPER_NOUN_FORM_NUM)>=0)
        im->flags&=~WordMatch::flagAddProperNoun; // flag is unnecessary if already has form
    }
    if (!(im->flags&WordMatch::flagFirstLetterCapitalized) || (im->flags&WordMatch::flagRefuseProperNoun))
    {
      im->forms.reset(PROPER_NOUN_FORM_NUM);
      im->flags&=~WordMatch::flagAddProperNoun;
    }
    if (!im->word->second.blockProperNounRecognition() && im->word->second.query(PROPER_NOUN_FORM_NUM)==-1 && im->word->first!=L"there" &&
      // if a proper noun is not all caps, and not the first word, it probably needs to be a noun, abbreviation or proper noun sub-class.
       firstLetterCapitalized && !allCaps &&
       (im->forms.isSet(nounForm) || im->forms.isSet(abbreviationForm) || im->forms.isSet(sa_abbForm) || im->word->second.isProperNounSubClass())) // more general in the future?
    {
      flags|=WordMatch::flagAddProperNoun;
      im->forms.set(PROPER_NOUN_FORM_NUM);
    }

    /*
    removed this because it is too restrictive:
      <s n="42"><w CRD>Two <w VBB>are <w NP0-NN1>Tourneur<w POS>'s <w AT0>The <w NP0-NN1>Revenger<w POS>'s <w NN1>Tragedy <c PUN>.
    if ((f>=0 && Forms[f]->inflectionsClass!="noun" && Forms[f]->inflectionsClass!="adjective") &&
        (f2<0 || (Forms[f2]->inflectionsClass!="noun" && Forms[f2]->inflectionsClass!="adjective")))
      im->forms.reset(PROPER_NOUN_FORM_NUM);
    */
  }
  else
    im->setPreferredForm();
  int selfForm=FormsClass::findForm(im->word->first);
  if (selfForm>=0 && im->word->first!=L"to")
    im->forms.set(selfForm);
  if (im->word->first==L"here")
    im->forms.set(nounForm);
  if ((f>=0 && Forms[f]->inflectionsClass==L"noun") || (f2>=0 && Forms[f2]->inflectionsClass==L"noun"))
    for (unsigned int I=0; I<im->word->second.formsSize(); I++)
      if (Forms[im->word->second.forms()[I]]->properNounSubClass)
        im->forms.set(im->word->second.forms()[I]);
  //wprintf(L"sentence=%05d\r",sentence);
  //printWord(iWord,flags,printLocation,firstWordInSentence);
  if ((result==PARSE_NUM || result==PARSE_TIME) && buffer[bufferScanLocation])
  {
    anotherWord=true;
    tag=TAG_NOT_SET;
  }
  if (anotherWord)
    return processWord(source,sourceId,buffer+bufferScanLocation,tag,secondTag,lastSentenceEnd,printLocation,sentence);
  return 0;
}

int bncc::processSentence(Source &source,int sourceId,wchar_t *s,int &lastSentenceEnd,int &printLocation,int sentence)
{
  //unsigned int where=0;
  wchar_t *beginTag=wcschr(s,L'<');
  if (!beginTag) return -4;
  wchar_t concatWord[1024];
  concatWord[0]=0;
  while (true)
  {
    wchar_t *endTag=wcschr(beginTag,L'>');
    if (!endTag) return -5;
    *endTag=0;
    wchar_t *nextTag=wcschr(endTag+1,L'<');
    if (nextTag) *nextTag=0;
    wchar_t tagKind=beginTag[1];
    if ((tagKind!=L'w' && tagKind!=L'c') || beginTag[2]!=L' ')
    {
      wchar_t *ch=wcschr(beginTag,L' ');
      if (ch) *ch=0;
      //if (wcscmp(beginTag+1,"hi") && beginTag[1]!=L'/' && wcscmp(beginTag+1,"pb"))
      //  lplog(L"tag %s skipped",beginTag+1);
      if (!nextTag) return 0;
      *nextTag=L'<';
      beginTag=nextTag;
      continue;
    }
    beginTag+=3;
    int tag=-1,secondTag=-1;
    if (beginTag[3]==L'-')
    {
      for (secondTag=0; secondTag<sizeof(tagList)/sizeof(*tagList); secondTag++)
        if (!wcscmp(tagList[secondTag].tag,beginTag+4))
          break;
      beginTag[3]=0;
      if (secondTag==sizeof(tagList)/sizeof(*tagList))
      {
        lplog(L"tag %s at %s #%d not recognized.",beginTag+4,source.storageLocation.c_str(),sentence);
        return -7;
    }
    }
    for (tag=0; tag<sizeof(tagList)/sizeof(*tagList); tag++)
      if (!wcscmp(tagList[tag].tag,beginTag))
        break;
    if (tag==sizeof(tagList)/sizeof(*tagList))
    {
      lplog(L"tag %s at %s #%d not recognized.",beginTag,source.storageLocation.c_str(),sentence);
      return -8;
    }
    if (tagList[tag].sForm!=NULL && !wcscmp(tagList[tag].sForm,L"COMBINE") && !wcscmp(tagList[tag].tag,L"POS"))
    {
      source.m[source.m.size()-1].flags|=WordMatch::flagNounOwner;
      //wprintf(L"'s ");
      printLocation+=2;
      if (!nextTag) return 0;
      *nextTag=L'<';
      beginTag=nextTag;
      continue;
    }
    wchar_t *ch;
    if ((ch=wcschr(endTag+1,L'\''))!=NULL && ch-(endTag+1)<3 && iswlower(endTag[1]) && iswlower(ch[1]) &&  // ||  //n't but not Brink's or d'Art
        tagList[tag].sForm!=NULL && !wcscmp(tagList[tag].sForm,L"COMBINE") && !wcscmp(tagList[tag].tag,L"XX0"))
    {
      wstring newWord=source.m[source.m.size()-1].word->first;
      newWord+=endTag+1;
      tag=secondTag=TAG_NOT_SET;
      source.m.erase(source.m.begin()+source.m.size()-1);
      processWord(source,sourceId,(wchar_t *)newWord.c_str(),tag,secondTag,lastSentenceEnd,printLocation,sentence);
      //wprintf(L"%s ",endTag+1);
      printLocation+=wcslen(endTag+1)+1;
      if (!nextTag) return 0;
      *nextTag=L'<';
      beginTag=nextTag;
      continue;
    }
    while (true)
    {
      wchar_t *slash=wcschr(endTag+1,L'/');
      if (slash==NULL || slash==endTag+1) break;
      *slash=0;
      int returnWord=processWord(source,sourceId,endTag+1,tag,secondTag,lastSentenceEnd,printLocation,sentence);
      if (returnWord<0) return returnWord;
      endTag=slash;
      tag=secondTag=TAG_NOT_SET;
      wchar_t *comma=L",";
      returnWord=processWord(source,sourceId,comma,tag,secondTag,lastSentenceEnd,printLocation,sentence);
      if (returnWord<0) return returnWord;
    }
    // the word must end with an alpha and the next but begin with an alpha and not be an XX0 (this case is handled elsewhere)
    // also the second word must not start with a capital letter
    if (endTag[wcslen(endTag+1)]!=L' ' && endTag[wcslen(endTag+1)]>0 && iswalpha(endTag[wcslen(endTag+1)]) &&
                              nextTag && nextTag[7]>0 && iswalpha(nextTag[7]) && !wcsstr(nextTag+1,L"XX0") &&
                              iswlower(nextTag[7]))
      wcscpy(concatWord,endTag+1);
    else
    {
      int returnWord;
      if (concatWord[0])
      {
        wcscat(concatWord,endTag+1);
        tag=secondTag=TAG_NOT_SET;
        returnWord=processWord(source,sourceId,concatWord,tag,secondTag,lastSentenceEnd,printLocation,sentence);
        concatWord[0]=0;
      }
      else
        returnWord=processWord(source,sourceId,endTag+1,tag,secondTag,lastSentenceEnd,printLocation,sentence);
    if (returnWord<0 && returnWord!=PARSE_EOF) return returnWord;
    if (returnWord==PARSE_EOF && !wcsncmp(endTag+1,L"~~END",wcslen(L"~~END")))
      return PARSE_EOF;
    }
    if (!nextTag) return 0;
    *nextTag=L'<';
    beginTag=nextTag;
  }
}

int bncc::process(Source &source,int sourceId,wstring id)
{
  preTaggedSource=true;
  int printLocation=0;
  wchar_t path[1024];
  wsprintf(path,L"%s\\BNC-world\\Texts\\%lC\\%lC%lC\\%lC%lC%lC",LMAINDIR,id[0],id[0],id[1],id[0],id[1],id[2]);
  if (Words.readWithLock(source.mysql,sourceId,path,false, true,false,false)<0)
    lplog(LOG_FATAL_ERROR,L"Cannot read dictionary.");
  Words.addMultiWordObjects(source.multiWordStrings,source.multiWordObjects);
  source.storageLocation =id;
  HANDLE hFile = CreateFile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
		wstring bcct;
    if (GetLastError()!=ERROR_PATH_NOT_FOUND)
      lplog(LOG_FATAL_ERROR,L"GetPath cannot open BNC path %s - %s",path,getLastErrorMessage(bcct));
    return -10;
  }
  unsigned int actualLen=GetFileSize(hFile,NULL);
  if (actualLen<=0)
  {
    lplog(LOG_FATAL_ERROR,L"ERROR:filelength of file %s yields an invalid filelength (%d).",path,actualLen);
    CloseHandle(hFile);
    return -11;
  }
  wchar_t *buffer=(wchar_t *)tmalloc(actualLen+1);
  DWORD lenRead=0;
  if (!ReadFile(hFile,buffer,actualLen,&lenRead,NULL) || actualLen!=lenRead)
  {
    lplog(LOG_FATAL_ERROR,L"ERROR:read error of file %s.",path);
    CloseHandle(hFile);
    return -12;
  }
  buffer[actualLen]=0;
  CloseHandle(hFile);
  translateBuffer(buffer,id);
  id=path;
  int where=0,s=1,lastSentenceEnd=0,lastProgressPercent=-1;
  unknownCount=0;
  while (!exitNow)
  {
    if ((int)(where*100/actualLen)>lastProgressPercent)
    {
      lastProgressPercent=(int)(where*100/actualLen);
      wprintf(L"PROGRESS: %03d%% (%06zu words) %d out of %d bytes read with %d seconds elapsed (%I64d bytes) \r",lastProgressPercent,source.m.size(),where,actualLen,clocksec(),memoryAllocated);
    }
    wchar_t *sentenceStart=wcsstr(buffer+where,L"<s n=\"");
    if (!sentenceStart) break;
    sentenceStart+=wcslen(L"<s n=\"");
    wchar_t *end=wcschr(sentenceStart,L'"');
    if (!end) return -13;
    *end=0;
    unsigned int check=_wtoi(sentenceStart);
    if (check!=s)
    {
      if (check<(unsigned int)s)
        lplog(L"**Skipped backwards from sentence #%d to sentence #%d.",s,check);
      else
      lplog(L"Skipped from sentence #%d to sentence #%d.",s,check);
      s=check;
    }
    s++;
    wchar_t *sentenceEnd=wcschr(end+1,L'\n');
    if (!sentenceEnd) 
			return -15;
    *sentenceEnd=0;
    where=(int)(sentenceEnd-buffer)+1;
    int returnSentence=processSentence(source,sourceId,end+2,lastSentenceEnd,printLocation,check);
    if (!source.m[source.m.size()-1].word->second.isSeparator())
      source.m.push_back(WordMatch(Words.sectionWord,PARSE_END_SENTENCE,source.debugTrace));
    source.sentenceStarts.push_back(lastSentenceEnd);
    if (returnSentence==PARSE_EOF) 
			break;
    lastSentenceEnd=source.m.size();
    if (returnSentence<0) return returnSentence;
  }
  tfree(actualLen+1,buffer);
  wprintf(L"PROGRESS: 100%% (%06zu words) %d out of %d bytes read with %d seconds elapsed (%I64d bytes) \n",source.m.size(),where,actualLen,clocksec(),memoryAllocated);
  return 0;//source.write(sourceId,path);
}

bncc::bncc(void)
{
  tIWMM q;
  for (unsigned int I=0; I<sizeof(tagList)/sizeof(*tagList); I++)
    if (!tagList[I].sForm)
      tagList[I].form=-1;
    else if (!wcscmp(tagList[I].sForm,L"COMBINE"))
      tagList[I].form=-2;
    else if (wcsstr(tagList[I].sForm,L"//"))
      tagList[I].form=-3;
    else
      tagList[I].form=FormsClass::findForm(tagList[I].sForm);
}

/*
treatment of the word 'THAT':
THAT:
aj0:425
av0:10219
cjc:0
cjt:632242
crd:0
dt0:312612
nn0:4
nn1:361
nn2:50
np0:3280
pnp:0
tle:0
unc:37
vvb:1
vvg:19
vvi:6
vvz:1

pronoun                   0
demonstrative_determiner  312612
conjunction               1
noun                      415
adjective                 425
adverb                    10219
relativizer               632242
*/