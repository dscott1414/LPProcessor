#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "math.h"
#include "sys/stat.h"
#include <sys/types.h>
#include "profile.h"
#include "paice.h"
#include "internet.h"

tInflectionMap shortNounInflectionMap[]=
{
	{ SINGULAR,L"S"},
	{ PLURAL,L"P"},
	{ SINGULAR_OWNER,L"SO"},
	{ PLURAL_OWNER,L"PO"},
	{ MALE_GENDER,L"MG"},
	{ FEMALE_GENDER,L"FG"},
	{ NEUTER_GENDER,L"NG"},
	{ FIRST_PERSON,L"1P"},
	{ SECOND_PERSON,L"2P"},
	{ THIRD_PERSON,L"3P"},
	{ -1,NULL}
};

tInflectionMap shortVerbInflectionMap[]=
{
	{ VERB_PAST,L"PAST"},
	{ VERB_PAST_PARTICIPLE,L"PAST_PART"},
	{ VERB_PRESENT_PARTICIPLE,L"PRES_PART"},
	{ VERB_PRESENT_FIRST_SINGULAR,L"PRES_1ST"},
	{ VERB_PRESENT_SECOND_SINGULAR,L"PRES_2ND"}, // "are"
	{ VERB_PRESENT_THIRD_SINGULAR,L"PRES_3RD"},
	{ VERB_PAST_THIRD_SINGULAR,L"PAST_3RD"},
	{ VERB_PAST_PLURAL,L"PAST_PLURAL"}, // special cases like "were"
	{ VERB_PRESENT_PLURAL,L"PRES_PLURAL"}, // special cases like "are"
	{ -1,NULL}
};

tInflectionMap shortAdjectiveInflectionMap[]=
{
	{ ADJECTIVE_NORMATIVE,L"ADJ"},
	{ ADJECTIVE_COMPARATIVE,L"ADJ_COMP"},
	{ ADJECTIVE_SUPERLATIVE,L"ADJ_SUP"},
	{ -1,NULL}
};

tInflectionMap shortAdverbInflectionMap[]=
{
	{ ADVERB_NORMATIVE,L"ADV"},
	{ ADVERB_COMPARATIVE,L"ADV_COMP"},
	{ ADVERB_SUPERLATIVE,L"ADV_SUP"},
	{ -1,NULL}
};

tInflectionMap shortQuoteInflectionMap[]=
{
	{ OPEN_INFLECTION,L"OQ"},
	{ CLOSE_INFLECTION,L"CQ"},
	{ -1,NULL}
};

tInflectionMap shortBracketInflectionMap[]=
{
	{ OPEN_INFLECTION,L"OB"},
	{ CLOSE_INFLECTION,L"CB"},
	{ -1,NULL}
};

wchar_t *OCSubTypeStrings[]={
	L"canadian province city",
	L"country",
	L"island",
	L"mountain range peak landform",
	L"ocean sea",
	L"park monument",
	L"region",
	L"river lake waterway",
	L"us city town village",
	L"us state territory region",
	L"world city town village",
	L"geographical natural feature",
	L"geographical urban feature",
	L"geographical urban subfeature",
	L"geographical urban subsubfeature",
	L"travel",
	L"moving",
	L"moving natural",
	L"relative direction",
	L"absolute direction",
	L"by activity",
	L"unknown(place)",
	NULL
};
// get inflection for form - remember to prepend a space
// if there is no inflection , return an empty string
int WordMatch::getInflectionLength(int inflection,tInflectionMap *map)
{ LFS
	int len=0;
	for (int I=0; map[I].num>=0; I++)
		if (map[I].num&inflection)
			len+=1+wcslen(map[I].name);
	return len;
}

__int64 roles[]={ SUBOBJECT_ROLE,SUBJECT_ROLE,OBJECT_ROLE,META_NAME_EQUIVALENCE,MPLURAL_ROLE,HAIL_ROLE,
						IOBJECT_ROLE,PREP_OBJECT_ROLE,RE_OBJECT_ROLE,IS_OBJECT_ROLE,NOT_OBJECT_ROLE,NONPAST_OBJECT_ROLE,ID_SENTENCE_TYPE,NO_ALT_RES_SPEAKER_ROLE,
						IS_ADJ_OBJECT_ROLE,NONPRESENT_OBJECT_ROLE,PLACE_OBJECT_ROLE,MOVEMENT_PREP_OBJECT_ROLE,NON_MOVEMENT_PREP_OBJECT_ROLE,
						SUBJECT_PLEONASTIC_ROLE,IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE,UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE,SENTENCE_IN_REL_ROLE,
						PASSIVE_SUBJECT_ROLE, POV_OBJECT_ROLE, MNOUN_ROLE, PRIMARY_SPEAKER_ROLE,SECONDARY_SPEAKER_ROLE,FOCUS_EVALUATED,
						ID_SENTENCE_TYPE,DELAYED_RECEIVER_ROLE,IN_PRIMARY_QUOTE_ROLE,IN_SECONDARY_QUOTE_ROLE,IN_EMBEDDED_STORY_OBJECT_ROLE,EXTENDED_OBJECT_ROLE,
						NOT_ENCLOSING_ROLE,EXTENDED_ENCLOSING_ROLE,NONPAST_ENCLOSING_ROLE,NONPRESENT_ENCLOSING_ROLE,POSSIBLE_ENCLOSING_ROLE,THINK_ENCLOSING_ROLE};
wchar_t *r_c[]={ L"SUBOBJ",L"SUBJ",L"OBJ",L"META_EQUIV",L"MP",L"H",
						L"IOBJECT",L"PREP",L"RE",L"IS",L"NOT",L"NONPAST",L"ID",L"NO_ALT_RES_SPEAKER",
						L"IS_ADJ",L"NONPRESENT",L"PL",L"MOVE",L"NON_MOVE",
						L"PLEO",L"INQ_SELF_REF_SPEAKER",L"UNRES_FROM_IMPLICIT",L"S_IN_REL",
						L"PASS_SUBJ",L"POV",L"MNOUN",L"SP",L"SECONDARY_SP",L"EVAL",
						L"ID",L"DELAY",L"PRIM",L"SECOND",L"EMBED",L"EXT",
						L"NOT_ENC",L"EXT_ENC",L"NPAST_ENC",L"NPRES_ENC",L"POSS_ENC",L"THINK_ENC"};
wstring WordMatch::roleString(wstring &sRole)
{ LFS
	sRole.clear();
	for (unsigned int I=0; I<sizeof(roles)/sizeof(roles[0]); I++)
		if (objectRole&roles[I])
		{
			sRole+=L"[";
			sRole+=r_c[I];
			sRole+=L"]";
		}
	return sRole;
}
// get the longest combination of the short inflection + form
// get any inflections:
// if noun inflection, get noun inflection length, and if "like" noun: add length
//    and if flagNounOwner, add owner (singular or plural), if flagAddProperNoun || flagOnlyConsiderProperNounForms add proper noun short length
// if verb inflection and not flagOnlyConsiderProperNounForms, get verb inflection length, and if "like" verb: add length
// if adjective inflection and not flagOnlyConsiderProperNounForms, get adjective inflection length and adjective length
// if adverb inflection and not flagOnlyConsiderProperNounForms, get adverb inflection length and adverb length
// for all other forms not "like" adverb, adjective, verb, noun: max(shortlen) of the form.
unsigned int WordMatch::getShortAllFormAndInflectionLen(void)
{ LFS
	unsigned int fSize=formsSize();
	int allLen=0,formLen;
	for (unsigned int line=0; line<fSize; line++)
	{
		bool properNoun;
		int inflectionFlags=word->second.inflectionFlags;
		if ((properNoun=(flags&flagAddProperNoun) && line==word->second.formsSize()) ||
			word->second.Form(line)->inflectionsClass==L"noun")
		{
			if (properNoun)
				formLen=Forms[PROPER_NOUN_FORM_NUM]->shortName.length();
			else
				formLen=word->second.Form(line)->shortName.length();
			if (flags&flagNounOwner)
			{
				if (inflectionFlags&SINGULAR) inflectionFlags=SINGULAR_OWNER|(inflectionFlags&~SINGULAR);
				else if (inflectionFlags&PLURAL) inflectionFlags=PLURAL_OWNER|(inflectionFlags&~PLURAL);
				else inflectionFlags|=SINGULAR_OWNER;
			}
			formLen+=getInflectionLength(inflectionFlags&NOUN_INFLECTIONS_MASK,shortNounInflectionMap)+2;
			if (word->second.getUsageCost(line)>9) formLen++;
		}
		else
		{
			wstring inflectionsClass=word->second.Form(line)->inflectionsClass;
			formLen=word->second.Form(line)->shortName.length();
			if (inflectionsClass==L"verb" && (inflectionFlags&VERB_INFLECTIONS_MASK))
				formLen+=getInflectionLength(inflectionFlags&VERB_INFLECTIONS_MASK,shortVerbInflectionMap);
			else if (inflectionsClass==L"adverb" && (inflectionFlags&ADVERB_INFLECTIONS_MASK))
				formLen+=getInflectionLength(inflectionFlags&ADVERB_INFLECTIONS_MASK,shortAdverbInflectionMap);
			else if (inflectionsClass==L"adjective" && (inflectionFlags&ADJECTIVE_INFLECTIONS_MASK))
				formLen+=getInflectionLength(inflectionFlags&ADJECTIVE_INFLECTIONS_MASK,shortAdjectiveInflectionMap);
			else if (inflectionsClass==L"quotes" && (inflectionFlags&INFLECTIONS_MASK))
				formLen+=getInflectionLength(inflectionFlags&INFLECTIONS_MASK,shortQuoteInflectionMap);
			else if (inflectionsClass==L"brackets" && (inflectionFlags&INFLECTIONS_MASK))
				formLen+=getInflectionLength(inflectionFlags&INFLECTIONS_MASK,shortBracketInflectionMap);
			formLen+=2;
			if (word->second.getUsageCost(line)>9) formLen++;
		}
		allLen=max(allLen,formLen);
	}
	return allLen;
}

unsigned int WordMatch::getShortFormInflectionEntry(int line, wchar_t *entry)
{
	LFS
	int inflectionFlags=word->second.inflectionFlags;
	wstring temp;
	if (flags&flagOnlyConsiderProperNounForms)
	{
		if (flags&flagAddProperNoun)
		{
			if (line!=word->second.formsSize() && !word->second.Form(line)->properNounSubClass)
				return 0;
		}
		else
		{
			if (word->second.Form(line)->inflectionsClass!=L"noun" && !word->second.Form(line)->properNounSubClass)
				return 0;
		}
	}
	if (((flags&flagAddProperNoun) && line==word->second.formsSize()) ||
		word->second.Form(line)->inflectionsClass==L"noun")
	{
		if ((flags&flagAddProperNoun) && line==word->second.formsSize())
			wcscpy(entry,Forms[PROPER_NOUN_FORM_NUM]->shortName.c_str());
		else
			wcscpy(entry,word->second.Form(line)->shortName.c_str());
		if (flags&flagNounOwner)
		{
			if (inflectionFlags&SINGULAR) inflectionFlags=SINGULAR_OWNER|(inflectionFlags&~SINGULAR);
			else if (inflectionFlags&PLURAL) inflectionFlags=PLURAL_OWNER|(inflectionFlags&~PLURAL);
			else inflectionFlags|=SINGULAR_OWNER;
		}
		wcscat(entry,getInflectionName(inflectionFlags&NOUN_INFLECTIONS_MASK,shortNounInflectionMap,temp));
		// only accumulate or use usage costs IF word is not capitalized
		if (costable())
			wsprintf(entry+wcslen(entry),L"*%d",word->second.getUsageCost(line));
		return 0;
	}
	wstring inflectionsClass=word->second.Form(line)->inflectionsClass;
	wcscpy(entry,word->second.Form(line)->shortName.c_str());
	if (inflectionsClass==L"verb" && (inflectionFlags&VERB_INFLECTIONS_MASK))
		wcscat(entry,getInflectionName(inflectionFlags&VERB_INFLECTIONS_MASK,shortVerbInflectionMap,temp));
	else if (inflectionsClass==L"adverb" && (inflectionFlags&ADVERB_INFLECTIONS_MASK))
		wcscat(entry,getInflectionName(inflectionFlags&ADVERB_INFLECTIONS_MASK,shortAdverbInflectionMap,temp));
	else if (inflectionsClass==L"adjective" && (inflectionFlags&ADJECTIVE_INFLECTIONS_MASK))
		wcscat(entry,getInflectionName(inflectionFlags&ADJECTIVE_INFLECTIONS_MASK,shortAdjectiveInflectionMap,temp));
	else if (inflectionsClass==L"quotes" && (inflectionFlags&INFLECTIONS_MASK))
		wcscat(entry,getInflectionName(inflectionFlags&INFLECTIONS_MASK,shortQuoteInflectionMap,temp));
	else if (inflectionsClass==L"brackets" && (inflectionFlags&INFLECTIONS_MASK))
		wcscat(entry,getInflectionName(inflectionFlags&INFLECTIONS_MASK,shortBracketInflectionMap,temp));
	if (costable())
		wsprintf(entry+wcslen(entry),L"*%d",word->second.getUsageCost(line));
	return 0;
}

unsigned int WordMatch::formsSize(void)
{
	LFS
	if (flags&flagAddProperNoun) return word->second.formsSize() + 1;
	return word->second.formsSize();
}

unsigned int WordMatch::getFormNum(unsigned int formOffset)
{
	LFS
	if (formOffset >= word->second.formsSize()) return PROPER_NOUN_FORM_NUM; // (flags&flagAddProperNoun) later?
	return word->second.forms()[formOffset];
}

// gets all forms, including proper noun if set and properly checked
vector <int> WordMatch::getForms()
{
	vector <int> checkedForms;
	int form;
	for (unsigned int tagFormOffset = 0; tagFormOffset < formsSize(); tagFormOffset++)
		if (queryForm(form = getFormNum(tagFormOffset)) >= 0)
			checkedForms.push_back(form);
	return checkedForms;
}

// in the case of a proper noun, it increases the size of the form by 1
int WordMatch::queryForm(int form)
{ LFS
	if ((flags&flagAddProperNoun) && form==PROPER_NOUN_FORM_NUM)
		return word->second.formsSize();
	if (flags&flagOnlyConsiderProperNounForms)
	{
		if (form!=PROPER_NOUN_FORM_NUM && !Forms[form]->properNounSubClass) return -1;
	}
	else if (form==PROPER_NOUN_FORM_NUM && (!(flags&flagFirstLetterCapitalized) || (flags&flagRefuseProperNoun)))
		return -1;
	return word->second.query(form);
}

int WordMatch::queryForm(wstring sForm)
{ LFS
	int form;
	if ((form=FormsClass::findForm(sForm))<0) return -1;
	return queryForm(form);
}

// in the case of a proper noun, it increases the size of the form by 1
int WordMatch::queryWinnerForm(int form)
{ DLFS
	int picked;
	if (flags&flagAddProperNoun && form==PROPER_NOUN_FORM_NUM)
		picked=word->second.formsSize();
	else if (flags&flagOnlyConsiderProperNounForms)
	{
		if (form!=PROPER_NOUN_FORM_NUM && !Forms[form]->properNounSubClass) return -1;
		picked=word->second.query(form);
	}
	else picked=word->second.query(form);
	if (picked>=0 && isWinner(picked)) return picked;
	return -1;
}

wstring WordMatch::winnerFormString(wstring &formsString,bool withCost)
{
	LFS
	formsString.clear();
	for (int f = 0; f < (signed)word->second.formsSize(); f++)
		if (isWinner(f))
		{
			formsString += word->second.Form(f)->name;
			if (withCost)
			{
				wchar_t temp[10];
				_itow(word->second.getUsageCost(f), temp, 10);
				formsString += L"[" + wstring(temp) + L"] ";
			}
			else
				formsString += L" ";
		}
	if (formsString.size() > 1)
		formsString = formsString.substr(0, formsString.length() - 1);
	if ((flags&flagAddProperNoun) && isWinner(word->second.formsSize()))
	{
		if (formsString.size() > 0)
			formsString += L" ";
		formsString += wstring(Forms[PROPER_NOUN_FORM_NUM]->name);
	}
	return formsString;
}

void WordMatch::getWinnerForms(vector <int> &winnerForms)
{
	LFS
	for (int f = 0; f < (signed)word->second.formsSize(); f++)
		if (isWinner(f))
			winnerForms.push_back(word->second.getFormNum(f));
	if ((flags&flagAddProperNoun) && isWinner(word->second.formsSize()))
		winnerForms.push_back(PROPER_NOUN_FORM_NUM);
}

int WordMatch::getNumWinners()
{
	LFS
	int numWinners = 0;
	for (int f = 0; f < (signed)word->second.formsSize(); f++)
		if (isWinner(f))
			numWinners++;
	if ((flags&flagAddProperNoun) && isWinner(word->second.formsSize()))
		numWinners++;
	return numWinners;
}

wstring WordMatch::patternWinnerFormString(wstring &winnerForms)
{ LFS
	winnerForms.clear();
	for (int f=0; f<(signed)word->second.formsSize(); f++)
		if (isWinner(f))
		{
			if (winnerForms.length()>0)
				winnerForms+=L",";
			winnerForms+=word->second.Form(f)->name;
			winnerForms+=L"|"+word->first;
			wchar_t temp[10];
			_itow(word->second.getUsageCost(f),temp,10);
			winnerForms+=L"*"+wstring(temp);
		}
	return winnerForms;
}

bool WordMatch::hasWinnerVerbForm(void)
{ LFS
	return word->second.hasWinnerVerbForm(tmpWinnerForms);
}

int WordMatch::queryWinnerForm(wstring sForm)
{ DLFS
	int form;
	if ((form=FormsClass::findForm(sForm))<=0) return -1;
	return queryWinnerForm(form);
}

bool WordMatch::isPossessivelyGendered(bool &possessivePronoun)
{ LFS
	if (possessivePronoun=(queryWinnerForm(possessiveDeterminerForm)>=0)) return true;
	return ((queryWinnerForm(nounForm)>=0 || queryWinnerForm(PROPER_NOUN_FORM_NUM)>=0) && (flags&flagNounOwner));
}

bool WordMatch::isGendered(void)
{ LFS
	if (queryWinnerForm(nounForm)>=0 || queryForm(honorificForm)>=0) return true;
	if (queryWinnerForm(honorificForm)<0 && queryWinnerForm(honorificAbbreviationForm)<0 &&
		queryWinnerForm(nomForm)<0 &&
		queryWinnerForm(personalPronounForm)<0 &&
		queryWinnerForm(accForm)<0 &&
		queryWinnerForm(reflexivePronounForm)<0 &&
		queryWinnerForm(possessivePronounForm)<0 &&
		queryWinnerForm(PROPER_NOUN_FORM_NUM)<0 &&
		queryWinnerForm(pronounForm)<0 &&
		queryWinnerForm(indefinitePronounForm)<0)
		return false;
	// if this is not a proper noun, then this is a gendered noun.
	if (word->second.query(PROPER_NOUN_FORM_NUM)<0) return true;
	// if this is a proper noun, this is only gendered if it is capitalized.
	return ((flags&flagFirstLetterCapitalized) && !(flags&flagRefuseProperNoun));
}

char *wTM(wstring inString,string &outString);

// when changing this definition, the WNcache must be deleted
bool WordMatch::isPhysicalObject(void)
{ LFS
	tIWMM w=word;
	bool found=false;
	if (w->second.query(nounForm)<0) return false;
	if (w->second.mainEntry!=wNULL) w=w->second.mainEntry;
	if (!(w->second.flags&(tFI::physicalObjectByWN|tFI::notPhysicalObjectByWN|tFI::uncertainPhysicalObjectByWN)))
	{
		if (hasHyperNym(w->first,L"physical_object",found) || hasHyperNym(w->first,L"physical_entity",found))
			w->second.flags|=tFI::physicalObjectByWN;
		else if (hasHyperNym(word->first,L"psychological_feature",found) && !hasHyperNym(word->first,L"cognitive_state",found))
			w->second.flags|=tFI::notPhysicalObjectByWN;
		else
			w->second.flags|=tFI::uncertainPhysicalObjectByWN;
	}
	return (w->second.flags&tFI::physicalObjectByWN)!=0;
}

// purposefully ignores whether the noun has a flagNounOwner because of its use.
// also judges noun usage - if noun form is rarely used, it is ignored.
bool WordMatch::isNounType(void)
{
	LFS
		if (word->second.query(verbverbForm) >= 0 && (word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE))
			return false;
	int form;
	return
		(flags&flagOnlyConsiderProperNounForms) ||
		(flags&flagAddProperNoun) ||
		((form = word->second.query(PROPER_NOUN_FORM_NUM)) >= 0 && (flags&flagFirstLetterCapitalized)) ||
		((form = word->second.query(nounForm)) >= 0 && word->second.getUsageCost(form) < 3) ||
		((form = word->second.query(indefinitePronounForm)) >= 0 && word->second.getUsageCost(form) < 3) ||
		((form = word->second.query(relativizerForm)) >= 0 && word->second.getUsageCost(form) < 3);
}

bool WordMatch::isTopLevel(void)
{
	LFS
	for (unsigned int I = 0; I < word->second.formsSize(); I++)
	{
		if (word->second.Form(I)->isTopLevel)
			return true;
	}
	return false;
}

// we avoid processing verb past because the noun owner type could be used as a subject:
// Jane's corrupted love.  Could be either! [Jane has corrupted love] or [Jane's corrupted love]  Unsolved problem
bool WordMatch::isModifierType(void)
{ LFS
	int form;
	if (!(flags&flagOnlyConsiderProperNounForms))
	{
		if (word->second.query(verbverbForm)>=0 && (word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE))
			return false;
		int modifierForms[]={adverbForm,adjectiveForm,numeralOrdinalForm,NUMBER_FORM_NUM,coordinatorForm,NULL};
		for (int I=0; modifierForms[I]; I++)
			if ((form=word->second.query(modifierForms[I]))>=0 && word->second.getUsageCost(form)<3) return true;
	}
	return (((form=word->second.query(nounForm))>=0 || (form=word->second.query(PROPER_NOUN_FORM_NUM))>=0) && 
					word->second.getUsageCost(form)<3 && (flags&flagNounOwner));
}

bool WordMatch::read(char *buffer,int &where,int limit)
{ DLFS
	wstring temp;
	if (!copy(temp,buffer,where,limit)) return false;
	if ((word=Words.query(temp))==Words.end())
	{
		wstring sWord,comment;
		int sourceId=-1,nounOwner=0;
		__int64 bufferLength=temp.length(),bufferScanLocation=0;
		bool added=false;
		int result=Words.readWord((wchar_t *)temp.c_str(),bufferLength,bufferScanLocation,sWord,comment,nounOwner,false,false,t,NULL,-1);
		word=Words.end();
		if (result==PARSE_NUM)
			word=Words.addNewOrModify(NULL, sWord,0,NUMBER_FORM_NUM,0,0,L"",sourceId,added);
		else if (result==PARSE_PLURAL_NUM)
			word=Words.addNewOrModify(NULL, sWord,0,NUMBER_FORM_NUM,PLURAL,0,L"",sourceId,added);
		else if (result==PARSE_ORD_NUM)
			word=Words.addNewOrModify(NULL, sWord,0,numeralOrdinalForm,0,0,L"",sourceId,added);
		else if (result==PARSE_ADVERB_NUM)
			word=Words.addNewOrModify(NULL, sWord,0,adverbForm,0,0,sWord,sourceId,added);
		else if (result==PARSE_DATE)
			word=Words.addNewOrModify(NULL, sWord,0,dateForm,0,0,L"",sourceId,added);
		else if (result==PARSE_TIME)
			word=Words.addNewOrModify(NULL, sWord,0,timeForm,0,0,L"",sourceId,added);
		else if (result==PARSE_TELEPHONE_NUMBER)
			word=Words.addNewOrModify(NULL, sWord,0,telephoneNumberForm,0,0,L"",sourceId,added);
		else if (result==PARSE_MONEY_NUM)
			word=Words.addNewOrModify(NULL, sWord,0,moneyForm,0,0,L"",sourceId,added);
		else if (result == PARSE_WEB_ADDRESS)
			word = Words.addNewOrModify(NULL, sWord, 0, webAddressForm, 0, 0, L"", sourceId, added);
		else if (result<0 && result != PARSE_END_SENTENCE)
		{
			lplog(LOG_ERROR,L"Word %s cannot be added - error %d.",temp.c_str(),result);
			return false;
		}
		else
			if ((result=Words.parseWord(NULL,sWord,word,false,nounOwner, sourceId))<0)
			{
				string temp2;
				lplog(LOG_ERROR,L"Word %S cannot be added",wTM(temp,temp2,65001));
				return false;
			}
	}
	if (!copy(baseVerb,buffer,where,limit)) return false;
	if (!copy(flags,buffer,where,limit)) return false;
	if (!copy(maxMatch,buffer,where,limit)) return false;
	if (!copy(minAvgCostAfterAssessCost,buffer,where,limit)) return false;
	if (!copy(lowestAverageCost,buffer,where,limit)) return false;
	if (!copy(maxLACMatch,buffer,where,limit)) return false;
	if (!copy(maxLACAACMatch,buffer,where,limit)) return false;
	if (!copy(objectRole,buffer,where,limit)) return false;
	if (!copy(verbSense,buffer,where,limit)) return false;
	if (!copy(timeColor,buffer,where,limit)) return false;
	if (!copy(beginPEMAPosition,buffer,where,limit)) return false;
	if (!copy(endPEMAPosition,buffer,where,limit)) return false;
	if (!copy(PEMACount,buffer,where,limit)) return false;
	if (!pma.read(buffer,where,limit)) return false;
	if (!forms.read(buffer,where,limit)) return false;
	if (!forms.read(buffer,where,limit)) return false;
	//if (!winnerForms.read(buffer,where,limit)) return false;
	if (!patterns.read(buffer,where,limit)) return false;
	if (!copy(object,buffer,where,limit)) return false;
	if (!copy(principalWherePosition,buffer,where,limit)) return false;
	if (!copy(principalWhereAdjectivalPosition,buffer,where,limit)) return false;
	if (!copy(originalObject,buffer,where,limit)) return false;
	unsigned int omSize;
	if (!copy(omSize,buffer,where,limit)) return false;
	for (unsigned int I=0; I<omSize; I++)
		objectMatches.push_back(cOM(buffer,where,limit));
	unsigned int omnSize;
	if (!copy(omnSize,buffer,where,limit)) return false;
	for (unsigned int I=0; I<omnSize; I++)
		audienceObjectMatches.push_back(cOM(buffer,where,limit));
	if (!copy(quoteForwardLink,buffer,where,limit)) return false;
	if (!copy(endQuote,buffer,where,limit)) return false;
	if (!copy(nextQuote,buffer,where,limit)) return false;
	if (!copy(previousQuote,buffer,where,limit)) return false;
	if (!copy(relObject,buffer,where,limit)) return false;
	if (!copy(relSubject,buffer,where,limit)) return false;
	if (!copy(relVerb,buffer,where,limit)) return false;
	if (!copy(relPrep,buffer,where,limit)) return false;
	if (!copy(beginObjectPosition,buffer,where,limit)) return false;
	if (!copy(endObjectPosition,buffer,where,limit)) return false;
	if (!copy(tmpWinnerForms,buffer,where,limit)) return false;
	if (!copy(embeddedStorySpeakerPosition,buffer,where,limit)) return false;
	
	// flags
	__int64 tflags;
	if (!copy(tflags,buffer,where,limit)) return false;
	t.collectPerSentenceStats=tflags&1; tflags>>=1;
	t.traceTagSetCollection=tflags&1; tflags>>=1;
	t.traceIncludesPEMAIndex=tflags&1; tflags>>=1;
	t.traceUnmatchedSentences=tflags&1; tflags>>=1;
	t.traceMatchedSentences=tflags&1; tflags>>=1;
	t.traceSecondaryPEMACosting=tflags&1; tflags>>=1;
	t.tracePatternElimination=tflags&1; tflags>>=1;
	t.traceBNCPreferences=tflags&1; tflags>>=1;
	t.traceDeterminer=tflags&1; tflags>>=1;
	t.traceVerbObjects=tflags&1; tflags>>=1;
	t.traceObjectResolution=tflags&1; tflags>>=1;
	t.traceNameResolution=tflags&1; tflags>>=1;
	t.traceSpeakerResolution=tflags&1; tflags>>=1;
	t.traceRelations=tflags&1; tflags>>=1;
	t.traceAnaphors=tflags&1; tflags>>=1;
	t.traceEVALObjects=tflags&1; tflags>>=1;
	t.traceSubjectVerbAgreement=tflags&1; tflags>>=1;
	t.traceRole=tflags&1; tflags>>=1;
	t.printBeforeElimination=tflags&1; tflags >>= 1;
	t.traceTestSubjectVerbAgreement = tflags & 1; tflags >>= 1;
	if (!copy(logCache,buffer,where,limit)) return false;
	skipResponse=-1;
	if (!copy(relNextObject,buffer,where,limit)) return false;
	if (!copy(nextCompoundPartObject,buffer,where,limit)) return false;
	if (!copy(previousCompoundPartObject,buffer,where,limit)) return false;
	if (!copy(relInternalVerb,buffer,where,limit)) return false;
	if (!copy(relInternalObject,buffer,where,limit)) return false;
	if (!copy(quoteForwardLink,buffer,where,limit)) return false;
	if (!copy(quoteBackLink,buffer,where,limit)) return false;
	if (!copy(speakerPosition,buffer,where,limit)) return false;
	if (!copy(audiencePosition,buffer,where,limit)) return false;
	spaceRelation=false;
	andChainType=false;
	notFreePrep=false;
	hasVerbRelations=false;
	return true;
}

bool WordMatch::writeRef(void *buffer,int &where,int limit)
{ LFS
	if (!copy(buffer,word->first,where,limit)) return false;
	if (!copy(buffer,baseVerb,where,limit)) return false;
	if (!copy(buffer,flags,where,limit)) return false;
	if (!copy(buffer,maxMatch,where,limit)) return false;
	if (!copy(buffer,minAvgCostAfterAssessCost,where,limit)) return false;
	if (!copy(buffer,lowestAverageCost,where,limit)) return false;
	if (!copy(buffer,maxLACMatch,where,limit)) return false;
	if (!copy(buffer,maxLACAACMatch,where,limit)) return false;
	if (!copy(buffer,objectRole,where,limit)) return false;
	if (!copy(buffer,verbSense,where,limit)) return false;
	if (!copy(buffer,(unsigned char)timeColor,where,limit)) return false;
	if (!copy(buffer,beginPEMAPosition,where,limit)) return false;
	if (!copy(buffer,endPEMAPosition,where,limit)) return false;
	if (!copy(buffer,PEMACount,where,limit)) return false;
	pma.write(buffer,where,limit);
	forms.write(buffer,where,limit);
	forms.write(buffer,where,limit);
	//winnerForms.write(buffer,where,limit);
	patterns.write(buffer,where,limit);
	if (!copy(buffer,object,where,limit)) return false;
	if (!copy(buffer,principalWherePosition,where,limit)) return false;
	if (!copy(buffer,principalWhereAdjectivalPosition,where,limit)) return false;
	if (!copy(buffer,originalObject,where,limit)) return false;
	unsigned int omSize=objectMatches.size();
	if (!copy(buffer,omSize,where,limit)) return false;
	for (unsigned int I=0; I<omSize; I++)
		objectMatches[I].write(buffer,where,limit);
	unsigned int omnSize=audienceObjectMatches.size();
	if (!copy(buffer,omnSize,where,limit)) return false;
	for (unsigned int I=0; I<omnSize; I++)
		audienceObjectMatches[I].write(buffer,where,limit);
	if (!copy(buffer,quoteForwardLink,where,limit)) return false;
	if (!copy(buffer,endQuote,where,limit)) return false;
	if (!copy(buffer,nextQuote,where,limit)) return false;
	if (!copy(buffer,previousQuote,where,limit)) return false;
	if (!copy(buffer,relObject,where,limit)) return false;
	if (!copy(buffer,relSubject,where,limit)) return false;
	if (!copy(buffer,relVerb,where,limit)) return false;
	if (!copy(buffer,relPrep,where,limit)) return false;
	if (!copy(buffer,beginObjectPosition,where,limit)) return false;
	if (!copy(buffer,endObjectPosition,where,limit)) return false;
	if (!copy(buffer,tmpWinnerForms,where,limit)) return false;
	if (!copy(buffer,embeddedStorySpeakerPosition,where,limit)) return false;

	// flags
	// 110000000000
	__int64 tflags=0;
	tflags |= (t.traceTestSubjectVerbAgreement) ? 1 : 0; tflags <<= 1;
	tflags |= (t.printBeforeElimination) ? 1 : 0; tflags <<= 1;
	tflags|=(t.traceRole) ? 1:0; tflags<<=1;
	tflags|=(t.traceSubjectVerbAgreement) ? 1:0; tflags<<=1;
	tflags|=(t.traceEVALObjects) ? 1:0; tflags<<=1;
	tflags|=(t.traceAnaphors) ? 1:0; tflags<<=1;
	tflags|=(t.traceRelations) ? 1:0; tflags<<=1;
	tflags|=(t.traceSpeakerResolution) ? 1:0; tflags<<=1;
	tflags|=(t.traceNameResolution) ? 1:0; tflags<<=1;
	tflags|=(t.traceObjectResolution) ? 1:0; tflags<<=1;
	tflags|=(t.traceVerbObjects) ? 1:0; tflags<<=1;
	tflags|=(t.traceDeterminer) ? 1:0; tflags<<=1;
	tflags|=(t.traceBNCPreferences) ? 1:0; tflags<<=1;
	tflags|=(t.tracePatternElimination) ? 1:0; tflags<<=1;
	tflags|=(t.traceSecondaryPEMACosting) ? 1:0; tflags<<=1;
	tflags|=(t.traceMatchedSentences) ? 1:0; tflags<<=1;
	tflags|=(t.traceUnmatchedSentences) ? 1:0; tflags<<=1;
	tflags|=(t.traceIncludesPEMAIndex) ? 1:0; tflags<<=1;
	tflags|=(t.traceTagSetCollection) ? 1:0; tflags<<=1;
	tflags|=(t.collectPerSentenceStats) ? 1:0;
	if (!copy(buffer,tflags,where,limit)) return false;
	if (!copy(buffer,logCache,where,limit)) return false;
	if (!copy(buffer,relNextObject,where,limit)) return false;
	if (!copy(buffer,nextCompoundPartObject,where,limit)) return false;
	if (!copy(buffer,previousCompoundPartObject,where,limit)) return false;
	if (!copy(buffer,relInternalVerb,where,limit)) return false;
	if (!copy(buffer,relInternalObject,where,limit)) return false;
	if (!copy(buffer,quoteForwardLink,where,limit)) return false;
	if (!copy(buffer,quoteBackLink,where,limit)) return false;
	if (!copy(buffer,speakerPosition,where,limit)) return false;
	if (!copy(buffer,audiencePosition,where,limit)) return false;
	return true;
}

bool WordMatch::updateMaxMatch(int len,int avgCost)
{ LFS
	maxMatch=max(len,maxMatch);
	if (lowestAverageCost>avgCost)
	{
		maxLACMatch=len;
		lowestAverageCost=avgCost;
		return true;
	}
	if (lowestAverageCost==avgCost && len>maxLACMatch)
	{
		maxLACMatch=len;
		return true;
	}
	return false;
}

// lowerAverageCost is the original averageCost before it was set higher by assessCost
// if the lower (original) average cost is actually the lowest average cost set for the position,
//   it must be set to the higher avgCost to avoid eliminating all patterns in eliminateLoserPatterns
bool WordMatch::updateMaxMatch(int len,int avgCost,int lowerAvgCost)
{ LFS
	if (lowestAverageCost==lowerAvgCost && len==maxLACMatch)
	{
		lowestAverageCost=avgCost;
		return true;
	}
	return false;
}

void WordMatch::logFormUsageCosts(void)
{ LFS
	word->second.logFormUsageCosts(word->first);
}

bool Source::sumMaxLength(unsigned int begin,unsigned int end,unsigned int &matchedTripletSumTotal,int &matchedSentences,bool &containsUnmatchedElement)
{ LFS
	vector <WordMatch>::iterator im,imend=m.begin()+end;
	unsigned int matchedTripletSum=0;
	containsUnmatchedElement=false;
	for (im=m.begin()+begin; im!=imend; im++)
	{
		matchedTripletSum+=im->maxMatch;
		if ((!(im->flags&WordMatch::flagTopLevelPattern) &&
			!(im->flags&WordMatch::flagMetaData)) || im->maxMatch==-1)
		{
			if (im->word->second.isSeparator())
				im->setSeparatorWinner(); // make sure winner is set for wordForm counter increment (BNC)
			else
			{
				containsUnmatchedElement=true;
				im->flags|=WordMatch::flagNotMatched;
			}
		}
	}
	unsigned int len=end-begin;
	matchedTripletSumTotal+=matchedTripletSum;
	// don't print
	if ((len && (matchedTripletSum/len>=len)) || !containsUnmatchedElement)
	{
		matchedSentences++;
		if (!debugTrace.traceMatchedSentences)
			return false;
	}
	//if ((t.traceMatchedSentences || t.traceUnmatchedSentences) && len)
	//    lplog(L"Average pattern match length:%d (%d/%d)",matchedTripletSum/len,matchedTripletSum,len);
	return true;
}

void Source::setRelPrep(int where,int relPrep,int fromWhere,int setType)
{ LFS
	int original=m[where].relPrep;
	m[where].relPrep=relPrep;
	wchar_t *setTypeStr;
	if (setType==PREP_PREP_SET) setTypeStr=L"PREP_SET";
	if (setType==PREP_OBJECT_SET) setTypeStr=L"OBJECT_SET";
	if (setType==PREP_VERB_SET) setTypeStr=L"VERB_SET";
	//lplog(LOG_RESOLUTION,L"%06d:%s:Prep loop putting %d (replacing %d) resulting in %s (%d) [%s].",where,sourcePath.c_str(),relPrep,original,loopString(relPrep),fromWhere,setTypeStr);
	int prepLoop=0;
	while (relPrep!=-1) 
	{
		relPrep=m[relPrep].relPrep;
		if (prepLoop++>20)
		{
			wstring tmpstr;
			lplog(LOG_ERROR,L"%06d:Prep loop putting %d (replacing %d) resulting in %s (%d).",where,relPrep,original,loopString(relPrep,tmpstr),fromWhere);
			m[where].relPrep=original;
			break;
		}
	}
}

// PROLOGUE(0)[1]
// 000161 _VERBREL1[1](3,4)*0 __ALLVERB[*](4)
unsigned int Source::getMaxDisplaySize(vector <WordMatch>::iterator &im,int numPosition)
{ LFS
	size_t size=im->word->first.length()+3; // 2 is for the parens
	if (numPosition>9) size++;
	if (numPosition>99) size++;
	unsigned int len=im->getShortAllFormAndInflectionLen();
	size=max(size,len);
	for (int nextPEMAPosition=im->beginPEMAPosition; nextPEMAPosition!=-1; nextPEMAPosition=pema[nextPEMAPosition].nextByPosition)
	{
		patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+nextPEMAPosition;
		unsigned int pbegin=pem->begin+numPosition,pend=pem->end+numPosition;
		// name + pdiff + begin + end (5 for format characters + 1 for begin and 1 for end)
		unsigned int tmp=patterns[pem->getPattern()]->name.length()+patterns[pem->getPattern()]->differentiator.length()+7;
		if (pem->isChildPattern())
		{
			tmp+=patterns[pem->getChildPattern()]->name.length();
			tmp+=patterns[pem->getChildPattern()]->differentiator.length();
			tmp+=1+4; // (4 is for the format characters + 1 for begin)
			int childEnd=pem->getChildLen()+numPosition;
			if (childEnd>9) tmp++;
			if (childEnd>99) tmp++;
			if (childEnd>999) tmp++;
			if (childEnd>9999) tmp++;
		}
		else
			tmp+=Forms[im->getFormNum(pem->getChildForm())]->shortName.length()+1;
		if (pbegin>9) tmp++;
		if (pend>9) tmp++;
		if (pbegin>99) tmp++;
		if (pend>99) tmp++;
		if (pbegin>999) tmp++;
		if (pend>999) tmp++;
		if (pbegin>9999) tmp++;
		if (pend>9999) tmp++;
		int cost=pem->getOCost();
		tmp+=4;  // one for single digit cost, another for *, another for space between parent and child
		if (cost>9) tmp++;
		if (cost>99) tmp++;
		if (cost>999) tmp++;
		if (cost>9999) tmp++;
		if (debugTrace.traceIncludesPEMAIndex)
			tmp+=28;
		size=max(size,tmp);
	}
	return size;
}

bool Source::matchPattern(cPattern *p,int begin,int end,bool fill)
{ LFS // DLFS
	int expense=clock();
	bool matchFound=false;
	for (int pos=begin; pos<(int)end-1; pos++) // skip period
	{
#ifdef LOG_PATTERN_MATCHING
		lplog(L"%d:pattern %s[%s]:---------------------",pos,p->name.c_str(),p->differentiator.c_str());
#endif
		// do not put pos==begin here because we don't know whether we have split the sentence correctly!
		if (p->onlyBeginMatch && pos && (!m[pos-1].word->second.isSeparator() || m[pos-1].queryForm(relativizerForm)>=0)) continue; //  || m[pos-1].queryForm(relativeForm)
		if (p->strictNoMiddleMatch && pos && iswalpha(m[pos-1].word->first[0])) continue;
		// cannot come after a pronoun which would definitely be a subject (I, we, etc)
		if (p->notAfterPronoun && pos && (m[pos-1].queryForm(nomForm)>=0 || (m[pos-1].queryForm(personalPronounForm)>=0 && m[pos-1].word->first!=L"you" && m[pos - 1].word->first != L"it"))) continue;
		if (p->afterQuote && (pos<2 || 
				((m[pos-1].word->second.query(quoteForm)<0 || (m[pos-1].word->second.inflectionFlags&CLOSE_INFLECTION)!=CLOSE_INFLECTION)) && 
				(m[pos-2].word->second.query(quoteForm)<0 || m[pos-1].word->first==L","))) continue;
		if (p->matchPatternPosition(*this,pos,fill,debugTrace))
		{
#ifdef QLOGPATTERN
			if (pass>0) lplog(L"Pattern %s matched against sentence position %d",p->name.c_str(),pos);
#endif
			matchFound=true;
		}
	}
	p->evaluationTime+=clock()-expense;
	return matchFound;
}

bool Source::matchPatternAgainstSentence(cPattern *p,int s,bool fill)
{ LFS // DLFS
	return matchPattern(p,sentenceStarts[s],sentenceStarts[s+1],fill);
}


void Source::logOptimizedString(wchar_t *line,unsigned int lineBufferLen,unsigned int &linepos)
{ LFS
	wchar_t dest[2048];
	if (!linepos)
	{
		lplog(L"");
		//for (wchar_t *p=line,*pEnd=line+ lineBufferLen; p!=pEnd; p++) *p=' '; // cannot use _wcsnset_s!
		wmemset(line, L' ', lineBufferLen);
		return;
	}
	int len=linepos;
	while (len && line[len-1]==L' ') len--;
	if (!len)
	{
		linepos=0;
		return;
	}
	line[len]=0;
	int alignedLen=len&~3;
	int I,destI=0;
	for (I=0; I<alignedLen; I+=4)
	{
		if (*((int *)(line+I))==((32<<24)+(32<<16)+(32<<8)+32))
			dest[destI++]=L'\t';
		else
			for (int I2=I; I2<I+4 && I2<alignedLen; I2++,destI++)
				dest[destI]=line[I2];
	}
	while (I<len)
		dest[destI++]=line[I++];
	dest[destI]=L'\n';
	dest[destI+1]=0;
	logstring(LOG_INFO,dest);
	linepos=0;
	//for (wchar_t *p=line,*pEnd=line+ lineBufferLen; p!=pEnd; p++) *p=' '; // cannot use _wcsnset_s!
	wmemset(line, L' ', lineBufferLen);
}

int Source::getPEMAPosition(int position,int line)
{ LFS
	int PEMAPosition,offset=line;
	for (PEMAPosition=m[position].beginPEMAPosition; offset>0 && PEMAPosition!=-1; PEMAPosition=pema[PEMAPosition].nextByPosition,offset--);
	if (PEMAPosition>=(signed)pema.count)
		lplog(LOG_FATAL_ERROR,L"%d:Incorrect PEMA Position %d derived from line %d.",position,PEMAPosition,line);
	if (PEMAPosition<0)
		lplog(L"%d:Line %d not reachable",position,line);
	return PEMAPosition;
}

// print a sentence
// end should normally be set to words.size();
#define LINE_BUFFER_LEN 2048
int Source::printSentence(unsigned int rowsize,unsigned int begin,unsigned int end,bool containsNotMatched)
{ LFS
	wchar_t printLine[LINE_BUFFER_LEN];
	wchar_t bufferZone[2048];
	unsigned int totalSize,I=begin,maxLines,startword=begin,maxPhraseMatches,linepos=0;
	printMaxSize.reserve(end-begin);
	for (unsigned int fi=0; fi<end-begin; fi++) printMaxSize[fi]=0;
	bufferZone[0] = 0xFEFE;
	bufferZone[1] = 0xCECE;
	int printStart=begin;
#ifndef LOG_RELATIVE_LOCATION
	printStart=0;
#endif
	while (startword<end)
	{
		maxLines=0;
		maxPhraseMatches=0;
		startword=I;
		logOptimizedString(printLine, LINE_BUFFER_LEN,linepos);
		for (unsigned int line=0; (line<=(maxLines+maxPhraseMatches) || (maxLines==0 && I<end)); line++)
		{
			totalSize=0;
			logOptimizedString(printLine, LINE_BUFFER_LEN,linepos);
			vector <WordMatch>::iterator im;
			for (I=startword,im=m.begin()+startword; I<end && totalSize<rowsize; I++,im++)
			{
				if (im->word==Words.sectionWord) continue; // paragraph or sentence end
				maxPhraseMatches=max(maxPhraseMatches,im->PEMACount);
				maxLines=max(maxLines,im->formsSize());
				if (printMaxSize[I-begin]==0)
					printMaxSize[I-begin]=getMaxDisplaySize(im,I-printStart)+1;
				totalSize+=printMaxSize[I-begin]+1;
				if (totalSize>rowsize)
				{
					if (I == startword)
					{
						lplog(LOG_INFO | LOG_ERROR, L"ERROR:sqlrow too large size to print - word at position %d=%d totalSize=%d > rowSize=%d ...", I - begin, printMaxSize[I - begin], totalSize, rowsize);
						return 0;
					}
					break;
				}
				if (line==0)
				{
					size_t len=im->word->first.length();
					if (im->flags&WordMatch::flagNotMatched)
						printLine[linepos++]=(im->maxMatch<0) ? L'^' : L'-';
					wcscpy(printLine+linepos,im->word->first.c_str());
					if (im->flags&(WordMatch::flagAddProperNoun|WordMatch::flagOnlyConsiderProperNounForms|WordMatch::flagFirstLetterCapitalized))
						printLine[linepos]=towupper(printLine[linepos]);
					if (im->flags&(WordMatch::flagNounOwner))
					{
						printLine[linepos+len++]=L'\'';
						if (printLine[linepos+len-2]!=L's') printLine[linepos+len++]=L's';
					}
					if (im->flags&WordMatch::flagAllCaps)
						for (wchar_t *ch=printLine+linepos; *ch && (ch-printLine)<2047; ch++) *ch=towupper(*ch);
					if (containsNotMatched)
					{
						printLine[linepos+len]=0;
						logstring(LOG_NOTMATCHED,printLine+linepos - ((im->flags&WordMatch::flagNotMatched)? 1 : 0));
						logstring(LOG_NOTMATCHED,L" ");
					}
					wstring flags;
					if (im->flags&WordMatch::flagAddProperNoun) flags+=L"[PN]";
					if (im->flags&WordMatch::flagOnlyConsiderProperNounForms) flags+=L"[OPN]";
					if (im->flags&WordMatch::flagRefuseProperNoun) flags+=L"[RPN]";
					if (im->flags&WordMatch::flagOnlyConsiderOtherNounForms) flags+=L"[OON]";
					if (flags.size())
						len+=wsprintf(printLine+linepos+len,L"%s",flags.c_str());
					wsprintf(printLine+linepos+len,L"(%d)[%d]",I-printStart,im->lowestAverageCost);
				}
				else
				{
					if (im->formsSize()<=line-1)
					{
						int PEMAOffset;
						if (((line-1)>=maxLines+im->PEMACount) || (line-1)<maxLines || (PEMAOffset=getPEMAPosition(I,line-1-maxLines))<0)
							printLine[linepos]=0;
						else
						{
							patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+PEMAOffset;
							if (debugTrace.traceIncludesPEMAIndex)
							{
								wsprintf(printLine+linepos,L"%06d %06d %06d %06d ",
									pem-pema.begin(),pem->nextByPatternEnd,pem->nextByChildPatternEnd,pem->nextPatternElement);
								linepos+=28;
							}
							if (pem->isChildPattern())
								wsprintf(printLine+linepos,L"%s[%s](%d,%d)*%d %s[*](%d)%c",
												patterns[pem->getPattern()]->name.c_str(),patterns[pem->getPattern()]->differentiator.c_str(),pem->begin-printStart+I,pem->end-printStart+I,pem->getOCost(),
												patterns[pem->getChildPattern()]->name.c_str(),
												pem->getChildLen()+I-printStart,
												(pem->flagSet(patternElementMatchArray::ELIMINATED)) ? 'E':' ');
							else
								wsprintf(printLine+linepos,L"%s[%s](%d,%d)*%d %s%c",
												patterns[pem->getPattern()]->name.c_str(),patterns[pem->getPattern()]->differentiator.c_str(),pem->begin-printStart+I,pem->end-printStart+I,pem->getOCost(),
												Forms[im->getFormNum(pem->getChildForm())]->shortName.c_str(),
												(pem->flagSet(patternElementMatchArray::ELIMINATED)) ? 'E':' ');
							if (debugTrace.traceIncludesPEMAIndex)
								linepos-=28;
						}
					}
					else
					{
						if (im->isWinner(line-1) &&
							((im->word->second.formsSize()>line-1 && im->forms.isSet(im->word->second.forms()[line-1])) ||
							(im->word->second.formsSize()==line-1 && (im->flags&WordMatch::flagAddProperNoun))))
							im->getShortFormInflectionEntry(line-1,printLine+linepos);
						else
							printLine[linepos]=0;
					}
				}
				if (wcslen(printLine)<sizeof(printLine)-1) printLine[wcslen(printLine)]=L' ';
				linepos+=printMaxSize[I-begin];
			}
		}
	}
		if (containsNotMatched)
			logstring(LOG_NOTMATCHED,L"\n");
		if (bufferZone[0] != 0xFEFE || bufferZone[1] != 0xCECE)
			printf("STOP! BUFFER OVERRUN!");

	return 0;
}

bool aloneOnLine(wchar_t *buffer,wchar_t *loc,const wchar_t *pattern,wchar_t *&startScanningPosition,__int64 bufferLen,bool checkOnlyBeginning=false)
{ LFS
	// is sequence alone on line?
	wchar_t *after=loc+wcslen(pattern);
	if (loc>buffer) loc--;
	bool newline=false;
	while (loc>=buffer && iswspace(*loc))
	{
		loc--;
		if (*loc==13 || *loc==10) newline=true;
	}
	if (loc!=buffer) loc++;
	if (loc!=buffer && !newline)
	{
		//lplog(L"Sequence %s found not alone on line (back space).",pattern);
		return false;
	}
	if (checkOnlyBeginning) 
	{
		startScanningPosition=loc;
		return true;
	}
	while (after-buffer<bufferLen && *after!=13 && *after!=10)
	{
		if (!iswspace(*after))
		{
			//lplog(L"Sequence %s found not alone on line (after space).",pattern);
			return false;
		}
		after++;
	}
	startScanningPosition=loc;
	return true;
}

int Source::scanUntil(const wchar_t *start,int repeat,bool printError)
{ LFS
	wchar_t *loc=bookBuffer-1;
	while ((loc-bookBuffer)<bufferLen)
	{
		if (!(loc=wcsstr(loc+1,start)))
		{
			if (!wcscmp(start,L"~~BEGIN"))
				return 0;
			if (!(loc=wcsstr(bookBuffer,L"~~BEGIN")))
			{
				if (printError)
				{
					lplog(LOG_ERROR,L"Unable to find start '%s'.",start);
					wprintf(L"\nCould not find start '%s'.\n",start);
				}
				return -1;
			}
			else
				start=L"~~BEGIN";
		}
		wchar_t *startScanningPosition;
		if (aloneOnLine(bookBuffer,loc,start,startScanningPosition,bufferLen) && --repeat==0)
		{
			bufferScanLocation=startScanningPosition-bookBuffer;
			return 0;
		}
	}
	if (printError)
	{
		lplog(LOG_ERROR, L"Unable to find start '%s'.", start);
		wprintf(L"\nCould not find start '%s'.\n", start);
	}
	return -1;
}

bool checkIsolated(wchar_t *word,vector <WordMatch> &m,int I)
{
	return m[I].word == Words.sectionWord && m[I + 1].word->first == word && (m[I + 1].flags&WordMatch::flagAllCaps) && 
		(m[I + 2].word == Words.sectionWord || (m[I + 2].word->first == L":" && m[I + 3].word == Words.sectionWord) || (m[I + 2].word->first == L"." && m[I + 3].word == Words.sectionWord));
}

// gutenberg books end with:
// End of Project Gutenberg
// End of the Project Gutenberg
// End of this Project Gutenberg
// End Project Gutenberg
// End of Project Gutenberg's
// or THE END all in caps, on one line, alone
// or FOOTNOTES all in caps, on one line, alone
// or INDEX all in caps, on one line, alone
bool Source::analyzeEnd(wstring &path,int begin,int end,bool &multipleEnds)
{ LFS
	int w=0;
	bool endFound;
	for (w=begin; w<end; w++) 
		if (m[w].word->first==L"end") break;
	endFound=w!=end;
	if (w==end) 
	  w=begin;
	if (!multipleEnds)
	{
		// THE END
		if (endFound && (m[w].flags&WordMatch::flagAllCaps) && w > 3 && 
			m[w - 2].word == Words.sectionWord &&
			m[w - 1].word->first == L"the" && (m[w - 1].flags&WordMatch::flagAllCaps) &&
			(w+1>=m.size() || m[w + 1].word == Words.sectionWord || (w+2<m.size() && (m[w + 1].word->first == L"." && m[w + 2].word == Words.sectionWord))))
		{
			if (bookBuffer)
			{
				// if there is another THE END later, then forget it.
				multipleEnds = (wcsstr(bookBuffer + bufferScanLocation, L"THE END") != NULL);
				// eliminate false ENDings by requiring a section beyond it
				bool content = (wcsstr(bookBuffer + bufferScanLocation, L"CONTENTS") != NULL);
				bool index = (wcsstr(bookBuffer + bufferScanLocation, L"INDEX") != NULL);
				bool footnotes = (wcsstr(bookBuffer + bufferScanLocation, L"FOOTNOTES") != NULL);
				bool catalogue = (wcsstr(bookBuffer + bufferScanLocation, L"Catalogue of ") != NULL); // specifically for Horatio books
				bool transcriber = (wcsstr(bookBuffer + bufferScanLocation, L"Transcriber") != NULL); 
				return !multipleEnds && (content || index || footnotes || catalogue || transcriber);
			}
			else
			{
				for (int I = end; I < m.size() - 3; I++)
					if (multipleEnds = m[I].word == Words.sectionWord && m[I + 1].word->first == L"the" && (m[I + 1].flags&WordMatch::flagAllCaps) && m[I + 2].word->first == L"end" && (m[I + 2].flags&WordMatch::flagAllCaps) &&
						((m[I + 3].word == Words.sectionWord) || (m[I + 3].word->first == L"." && m[I + 4].word == Words.sectionWord)))
						break;
				bool content = false;
				for (int I = end; I < m.size() - 2; I++)
					if (content = checkIsolated(L"contents", m, I))
						break;
				bool index = false;
				for (int I = end; I < m.size() - 2; I++)
					if (index = checkIsolated(L"index", m, I))
						break;
				bool footnotes = false;
				for (int I = end; I < m.size() - 2; I++)
					if (checkIsolated(L"footnotes", m, I))
						break;
				return !multipleEnds && (content || index || footnotes);
			}
		}
	}
	for (; w<end; w++) if (m[w].word->first==L"project") break;
	if (w==end) return false;
	for (; w<end; w++) 
		if (m[w].word->first==L"gutenberg") 
		{
			if (!endFound)
			{
				wstring logres;
				lplog(LOG_ERROR,L"%s\n%s:%d:start of parse should be moved down!",phraseString(begin,end,logres,true).c_str(),path.c_str(),begin);
				return false;
			}
			return true;
		}
	
	return false;
}

// if date of dictionary or date of cache is before date of source, return true
// if date of dictionary > date of cache return true
bool Source::parseNecessary(wchar_t *path)
{ LFS
#ifdef ALWAYS_PARSE
	return true;
#endif
	storageLocation=path;
	struct _stat buffer;
	wstring locationCache= storageLocation +L".cache";
	int result=_wstat( locationCache.c_str(), &buffer );
	if (result<0) return true;
	__time64_t cacheLastModified=buffer.st_mtime;
	result=_wstat( L"WordCacheFile", &buffer );
	if (result<0) return true;
	__time64_t dictionaryLastModified=buffer.st_mtime;
	result=_wstat(storageLocation.c_str(), &buffer );
	if (result<0) return true;
	__time64_t sourceLastModified=buffer.st_mtime;
	return (//sourceLastModified>dictionaryLastModified ||
		sourceLastModified>cacheLastModified ||
		dictionaryLastModified>cacheLastModified);
}

#define ENCODING_STRING L"Character set encoding:"
// 0 -not set
#define HAS_BOM 1
#define FIND_START 2
#define FIND_START_BUFFER_CONVERT 4
#define QUOTE_IN_START 8
#define FIND_START_INITIAL_FAIL 16
#define FIND_START_INITIAL_SUCCESS 32
#define FIND_START_SUCCESS_AFTER_CONVERT 64
// UTF-8 // Unicode (UTF-8)
#define SOURCE_UTF8 128 // 65001
// Latin: Latin-1, Latin1, ISO Latin - 1, Latin 1, ISO - Latin - 1 // // ANSI Latin 1; Western European (Windows)
#define SOURCE_1252 256  // ANSI Latin 1; Western European (Windows) // 1252
// 8859: ISO - 8859 - 1, ISO 8859 - 1, ASCII, with some ISO - 8859 - 1 characters // ISO 8859-1 Latin 1; Western European (ISO)
#define SOURCE_8859 512 // ISO 8859-1 Latin 1; Western European (ISO) // 28591
#define SOURCE_ASCII 2048 // ASCII: ISO-646-US (US-ASCII), ASCII, US-ASCII  // US-ASCII (7-bit) // 20127
#define SOURCE_UNICODE 1024
#define ENCODING_MATCH_FAILED 4096
bool reDecodeNecessary(wstring encodingRecordedInDocument, int &codePage)
{
	int encodingRID= codePage;
	if (encodingRecordedInDocument.find(L"UTF") != wstring::npos)
		encodingRID = 65001;
	else if (encodingRecordedInDocument.find(L"Latin") != wstring::npos)
		encodingRID = 1252;
	else if (encodingRecordedInDocument.find(L"8859") != wstring::npos)
		encodingRID = 28591;
	else if (encodingRecordedInDocument.find(L"ASCII") != wstring::npos)
		encodingRID = 20127;
	if (encodingRID != codePage)
	{
		lplog(LOG_ERROR, L"Encoding error: %s (%d) embedded in source disagrees with decoding guess %d", encodingRecordedInDocument.c_str(), encodingRID, codePage);
		codePage = encodingRID;
		return true;
	}
	return false;
}


int Source::readSourceBuffer(wstring title, wstring etext, wstring path, wstring encodingFromDB, wstring &start, int &repeatStart)
{
	LFS
	beginClock=clock();
	int readBufferFlags = 0;
	//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path.c_str(), __FUNCTIONW__);
	HANDLE fd = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL| FILE_FLAG_SEQUENTIAL_SCAN, 0);
	if (fd== INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR,L"ERROR:Unable to open %s - %S. (3)",path.c_str(),_sys_errlist[errno]);
		return -1;
	}
	if (!GetFileSizeEx(fd, (PLARGE_INTEGER) &bufferLen))
	{
		lplog(LOG_ERROR, L"ERROR:Unable to get the file length of %s - %S. (3)", path.c_str(), _sys_errlist[errno]);
		return -1;
	}
	bookBuffer=(wchar_t *)tmalloc((size_t)bufferLen+10);
	DWORD numBytesRead;
	if (!ReadFile(fd,bookBuffer,(unsigned int)bufferLen,&numBytesRead,0) || bufferLen!=numBytesRead)
	{
		CloseHandle(fd);
		lplog(LOG_ERROR, L"ERROR:Unable to read %s - %S. (3)", path.c_str(), _sys_errlist[errno]);
		return -1;
	}
	CloseHandle(fd);
	if (bufferLen<0 || bufferLen==0) 
		return PARSE_EOF;
	sourcePath=path;
	bufferLen/=sizeof(bookBuffer[0]);
	bookBuffer[bufferLen+1]=0;
	bool hasBOM=bookBuffer[0] == 0xFEFF;
	bufferScanLocation=(hasBOM) ? 1 : 0; // detect BOM
	if (hasBOM)
		readBufferFlags += HAS_BOM;
	int bl = (int)bufferLen;
	bookBuffer = (wchar_t *)trealloc(20, bookBuffer, (bl + 10) << 1, (bl + 10) << 2);
	bufferLen <<= 1;
	wstring wb;
	int codepage;
	mTW((char *)bookBuffer, wb, codepage);
	wstring sourceEncoding = L"NOT FOUND";
	if (wb.length() <10)
	{
		readBufferFlags |= SOURCE_UNICODE;
		sourceEncoding = L"UNICODE";
		wb = bookBuffer;
	}
	else
	{
		int ew,weol;
		if ((ew = wb.find(ENCODING_STRING))!=wstring::npos && ((weol = wb.find(13, ew+ wcslen(ENCODING_STRING))) != wstring::npos || (weol = wb.find(10, ew + wcslen(ENCODING_STRING))) != wstring::npos))
		{
			ew += wcslen(ENCODING_STRING);
			sourceEncoding = wb.substr(ew,weol-ew);
			trim(sourceEncoding);
		}
		int error = 0,desiredCodePage=codepage;
		if (sourceEncoding != L"NOT FOUND" && (reDecodeNecessary(sourceEncoding, desiredCodePage)) && !mTWCodePage((char *)bookBuffer, wb, desiredCodePage, error))
		{
			desiredCodePage = 1252; // try ASCII
			if (mTWCodePage((char *)bookBuffer, wb, desiredCodePage, error))
				codepage = desiredCodePage;
			readBufferFlags |= ENCODING_MATCH_FAILED;
		}
		else
			codepage = desiredCodePage;
		error = wcscpy_s(bookBuffer, bufferLen + 10, wb.c_str());
		if (error)
			lplog(LOG_FATAL_ERROR, L"ERROR:Unable to copy string length %d into buffer of length %I64d wchar - (%d) %d.", (int)wb.length(), bufferLen, error, GetLastError());
		if (codepage == CP_UTF8)
			readBufferFlags |= SOURCE_UTF8;
		else if (codepage == 1252) // ANSI Latin 1; Western European (Windows)
			readBufferFlags |= SOURCE_1252;
		else if (codepage == 28591) // ISO 8859-1 Latin 1; Western European (ISO)
			readBufferFlags |= SOURCE_8859;
		else if (codepage == 20127) // ASCII: ISO-646-US (US-ASCII), ASCII, US-ASCII  // US-ASCII (7-bit)
			readBufferFlags |= SOURCE_ASCII;
	}
	bool startSet = true;
	if (start == L"**FIND**" || encodingFromDB!=sourceEncoding || (readBufferFlags & ENCODING_MATCH_FAILED)!=0)
	{
		readBufferFlags += FIND_START;
		startSet=findStart(wb, start, repeatStart, title);
		// write path back to DB
		updateSourceStart(start, repeatStart, etext, bufferLen);
		updateSourceEncoding(readBufferFlags, sourceEncoding, etext);
	}
	if (!startSet)
		return -1;
	size_t quoteEscapeFromDB = wstring::npos;
	while ((quoteEscapeFromDB = start.find(L"\\'")) != wstring::npos)
	{
		readBufferFlags |= QUOTE_IN_START;
		start.erase(start.begin() + quoteEscapeFromDB);
	}
	if (scanUntil(start.c_str(),repeatStart,false)<0)
	{
			lplog(LOG_ERROR, L"ERROR:Unable to find start in %s - start=%s, repeatStart=%d.", path.c_str(), start.c_str(), repeatStart);
			start = L"**START NOT FOUND**";
			updateSourceStart(start, -1, etext, 0);
			return -3;
	}
	if (sourceType!=PATTERN_TRANSFORM_TYPE) // patterns are included in variables which have _ in them
		for (unsigned int I=0; I<bufferLen; I++)
			if (bookBuffer[I]==L'_') bookBuffer[I]=L' ';
	return 0;
}

int Source::parseBuffer(wstring &path,unsigned int &unknownCount,bool newsBank)
{ LFS
	int lastProgressPercent=0,result=0,runOnSentences=0;
	bool alreadyAtEnd=false,previousIsProperNoun=false;
	bool webScrapeParse=sourceType==WEB_SEARCH_SOURCE_TYPE,multipleEnds=false;
	size_t lastSentenceEnd=m.size(),numParagraphsInSection=0;
	unordered_map <wstring, wstring> parseVariables;
	if (bufferScanLocation==0 && bookBuffer[0]==65279)
		bufferScanLocation=1;
	while (result==0 && !exitNow)
	{
		wstring sWord,comment;
		int nounOwner=0;
		bool flagAlphaBeforeHint=(bufferScanLocation && iswalpha(bookBuffer[bufferScanLocation-1]));
		bool flagNewLineBeforeHint=(bufferScanLocation && bookBuffer[bufferScanLocation-1]==13);
		result=Words.readWord(bookBuffer,bufferLen,bufferScanLocation,sWord,comment,nounOwner,false,webScrapeParse,debugTrace,&mysql,sourceId);
		if (comment.size() > 0)
			metaCommandsEmbeddedInSource[m.size()] = comment;
		bool flagAlphaAfterHint=(bufferScanLocation<bufferLen && iswalpha(bookBuffer[bufferScanLocation]));
		if (result==PARSE_EOF)
			break;
		if (result==PARSE_PATTERN)
		{
			temporaryPatternBuffer[(int)m.size()]=sWord;
			result=0;
			// [variable name]=[substitute word for parsing]:[pattern list]
			size_t equalsPos=sWord.find(L'='),colonPos=sWord.find(L':');
			if (equalsPos!=wstring::npos && colonPos!=wstring::npos)
			{
				wstring variable=sWord.substr(0,equalsPos);
				parseVariables[variable]=sWord=sWord.substr(equalsPos+1,colonPos-equalsPos-1);
				::lplog(LOG_WHERE,L"%d:parse created mapped variable %s=(%s)",m.size(),variable.c_str(),parseVariables[variable].c_str());
			}
			else 
			{
				::lplog(LOG_WHERE,L"%d:parse used mapped variable %s=(%s)",m.size(),sWord.c_str(),parseVariables[sWord].c_str());
				sWord=parseVariables[sWord];
			}
		}
		if (result==PARSE_END_SECTION || result==PARSE_END_PARAGRAPH || result==PARSE_END_BOOK || result==PARSE_DUMP_LOCAL_OBJECTS)
		{
			if (webScrapeParse && m.size()>1 && m[m.size()-1].word==Words.sectionWord)
			{
				result=0;
				continue;
			}
			if (analyzeEnd(path,lastSentenceEnd, m.size(), multipleEnds))
			{
				m.erase(m.begin()+lastSentenceEnd,m.end());
				alreadyAtEnd=true;
				break;
			}
			if (result==PARSE_END_PARAGRAPH && (!newsBank || numParagraphsInSection++<2))
			{
				sentenceStarts.push_back(lastSentenceEnd);
				lastSentenceEnd=m.size();
			}
			if (newsBank && result==PARSE_END_BOOK)
				numParagraphsInSection=0;
			if (m.size()==lastSentenceEnd) lastSentenceEnd++;
			m.push_back(WordMatch(Words.sectionWord,(result==PARSE_DUMP_LOCAL_OBJECTS) ? 1 : 0,debugTrace));
			result=0;
			continue;
		}
		size_t dash=wstring::npos,firstDash= wstring::npos;
		int numDash = 0,offset=0;
		for (wchar_t dq : sWord)
		{
			if (WordClass::isDash(dq))
			{
				numDash++;
				// if the word contains two dashes that follow right after each other or are of different types, split word immediately.
				if (numDash > 1 && (offset==dash+1 || dq!=sWord[dash]))
				{
					if (dq != sWord[dash] && offset>dash+1)
					{
						bufferScanLocation -= sWord.length() - offset;
						sWord.erase(offset, sWord.length() - offset);
						numDash = 1;
						break;
					}
					else if (dash>0)
					{
						bufferScanLocation -= sWord.length() - dash;
						sWord.erase(dash, sWord.length() - dash);
						dash = wstring::npos;
						numDash = 0;
						break;
					}
				}
				dash = offset;
				if (firstDash == wstring::npos)
					firstDash = offset;
			}
			offset++;
		}
		bool firstLetterCapitalized=iswupper(sWord[0])!=0;
		tIWMM w=(m.size()) ? m[m.size()-1].word : wNULL;
		// this logic is copied in doQuotesOwnershipAndContractions
		bool firstWordInSentence=m.size()==lastSentenceEnd || w->first==L"." || // include "." because the last word may be a contraction
			w==Words.sectionWord || w->first==L"--" /* Ulysses */|| w->first==L"?" || w->first==L"!" || 
			w->first==L":" /* Secret Adversary - Second month:  Promoted to drying aforesaid plates.*/||
			w->first==L";" /* I am a Soldier A jolly British Soldier ; You can see that I am a Soldier by my feet . . . */||
			w->second.query(quoteForm)>=0 || w->second.query(dashForm)>=0 || // BNC 4.00 PM - We
			w->second.query(bracketForm)>=0 || // BNC (c) Complete the ...
			(m.size()-3==lastSentenceEnd && m[m.size()-2].word->second.query(quoteForm)>=0 && w->first==L"(") || // Pay must be good.' (We might as well make that clear from the start.)
			(m.size()-2==lastSentenceEnd && w->first==L"(");   // The bag dropped.  (If you didn't know).
		// keep names like al-Jazeera or dashed words incorporating first words that are unknown like fierro-regni
		// preserve dashes by setting insertDashes to true.
		// Handle cases like 10-15 or 10-60,000 which are returned as PARSE_NUM
		if (dash!=wstring::npos && result==0 && // if (not date or number)
			sWord[1]!=0 && // not a single dash
			!WordClass::isDash(sWord[dash+1]) && // not '--'
			!(sWord[0]==L'a' && WordClass::isDash(sWord[1]) && numDash==1) // a-working
			 // state-of-the-art
			)
		{
			wstring lowerWord = sWord;
			transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), (int(*)(int)) tolower);
			unsigned int unknownWords=0,capitalizedWords=0,openWords=0, letters =0;
			vector <wstring> dashedWords = splitString(sWord, sWord[dash]);
			wstring removeDashesWord;
			for (wstring subWord : dashedWords)
			{
				wstring lowerSubWord = subWord;
				transform(lowerSubWord.begin(), lowerSubWord.end(), lowerSubWord.begin(), (int(*)(int)) tolower);
				tIWMM iWord=WordClass::fullQuery(&mysql, lowerSubWord,sourceId);
				if (iWord == WordClass::end() || iWord->second.query(UNDEFINED_FORM_NUM) >= 0)
					unknownWords++;
				if (iWord != WordClass::end() && Stemmer::wordIsNotUnknownAndOpen(iWord))
					openWords++;
				if (subWord.length() > 1 && iswupper(subWord[0]) && !iswupper(subWord[1]))  // al-Jazeera, not BALL-PLAYING
					capitalizedWords++;
				if (subWord.length() == 1)  // F-R-E-E-D-O-M  
					letters++;
				removeDashesWord += lowerSubWord;
			}
				// if this does not qualify as a dashed word, back up to just AFTER the dash
			tIWMM iWord = WordClass::fullQuery(&mysql, lowerWord, sourceId);
			tIWMM iWordNoDashes = WordClass::fullQuery(&mysql, removeDashesWord, sourceId);
			//bool notCapitalized = (capitalizedWords == 0 || (capitalizedWords == 1 && firstWordInSentence));
			// break apart if it is not capitalized, there are no unknown words, and the dashed word is unknown.
			// however, there may be cases like I-THE, which webster/dictionary.com incorrectly say are defined.
			if ((unknownWords <= capitalizedWords || unknownWords<=dashedWords.size()/2) && 
				  (iWord == Words.end() || iWord->second.query(UNDEFINED_FORM_NUM) >= 0) && 
				  (iWordNoDashes==Words.end() || iWordNoDashes->second.query(UNDEFINED_FORM_NUM) >= 0 || letters !=dashedWords.size() || letters <3))
			{
				bufferScanLocation -= sWord.length() - firstDash;
				sWord.erase(firstDash, sWord.length() - firstDash);
			}
			else if (sWord!=L"--" && debugTrace.traceParseInfo)
				lplog(LOG_INFO, L"%s NOT split (#unknownWords=%d #capitalizedWords=%d #letters=%d %s).", sWord.c_str(), unknownWords, capitalizedWords, letters, ((iWord == Words.end() || iWord->second.query(UNDEFINED_FORM_NUM) >= 0)) ? L"UNKNOWN" : L"NOT unknown");
			firstLetterCapitalized=(capitalizedWords>0);
		}
		bool allCaps=Words.isAllUpper(sWord);
		wcslwr((wchar_t *)sWord.c_str());
		bool added;
		bool endSentence=result==PARSE_END_SENTENCE;
		if (endSentence && m.size() && sWord==L".")
		{
			wchar_t *abbreviationForms[]={ L"letter",L"abbreviation",L"measurement_abbreviation",L"street_address_abbreviation",L"business_abbreviation",
				L"time_abbreviation",L"date_abbreviation",L"honorific_abbreviation",L"trademark",L"pagenum",NULL };
			for (unsigned int af=0; abbreviationForms[af] && endSentence; af++)
				if (m[m.size()-1].queryForm(abbreviationForms[af])>=0)
					endSentence=false;
		}
		tIWMM iWord=Words.end();
		if (result==PARSE_NUM)
			iWord=Words.addNewOrModify(&mysql,sWord,0,NUMBER_FORM_NUM,0,0,L"",sourceId,added);
		else if (result==PARSE_PLURAL_NUM)
			iWord=Words.addNewOrModify(&mysql, sWord,0,NUMBER_FORM_NUM,PLURAL,0,L"",sourceId,added);
		else if (result==PARSE_ORD_NUM)
			iWord=Words.addNewOrModify(&mysql, sWord,0,numeralOrdinalForm,0,0,L"",sourceId,added);
		else if (result==PARSE_ADVERB_NUM)
			iWord=Words.addNewOrModify(&mysql, sWord,0,adverbForm,0,0,sWord,sourceId,added);
		else if (result==PARSE_DATE)
			iWord=Words.addNewOrModify(&mysql, sWord,0,dateForm,0,0,L"",sourceId,added);
		else if (result==PARSE_TIME)
			iWord=Words.addNewOrModify(&mysql, sWord,0,timeForm,0,0,L"",sourceId,added);
		else if (result==PARSE_TELEPHONE_NUMBER)
			iWord=Words.addNewOrModify(&mysql, sWord,0,telephoneNumberForm,0,0,L"",sourceId,added);
		else if (result==PARSE_MONEY_NUM)
			iWord=Words.addNewOrModify(&mysql, sWord,0,moneyForm,0,0,L"",sourceId,added);
		else if (result == PARSE_WEB_ADDRESS)
			iWord = Words.addNewOrModify(&mysql, sWord, 0, webAddressForm, 0, 0, L"", sourceId, added);
		else if (result<0 && result != PARSE_END_SENTENCE)
			break;
		else
			if ((result=Words.parseWord(&mysql,sWord,iWord,firstLetterCapitalized,nounOwner, sourceId))<0)
				break;
		result=0;
		if (iWord->second.isUnknown())
		{
			lplog(LOG_INFO, L"UNKNOWN: %s", sWord.c_str());
			unknownCount++;
		}
		unsigned __int64 flags;
		iWord->second.adjustFormsInflections(sWord,flags,firstWordInSentence,nounOwner,allCaps,firstLetterCapitalized, false); 
		if (allCaps && m.size() && ((m[m.size()-1].word->first==L"the" && !(m[m.size()-1].flags&WordMatch::flagAllCaps)) || iWord->second.formsSize()==0))
		{
      flags|=WordMatch::flagAddProperNoun;
			iWord->second.zeroNewProperNounCostIfUsedAllCaps();
#ifdef LOG_PATTERN_COST_CHECK
      ::lplog(L"Added ProperNoun [from the] to word %s (form #%d) at cost %d.",originalWord.c_str(),formsSize(),usageCosts[formsSize()]);
#endif
		}
		if ((flags&WordMatch::flagAddProperNoun) && debugTrace.traceParseInfo)
			lplog(LOG_INFO,L"%d:%s:added flagAddProperNoun.",m.size(), sWord.c_str());
		if ((flags&WordMatch::flagOnlyConsiderProperNounForms) && debugTrace.traceParseInfo)
			lplog(LOG_INFO,L"%d:%s:added flagOnlyConsiderProperNounForms.", m.size(), sWord.c_str());
		if ((flags&WordMatch::flagOnlyConsiderOtherNounForms) && debugTrace.traceParseInfo)
			lplog(LOG_INFO,L"%d:%s:added flagOnlyConsiderOtherNounForms.", m.size(), sWord.c_str());
		// does not necessarily have to be a proper noun after a word with a . at the end (P.N.C.)
		// The description of a green toque , a coat with a handkerchief in the pocket marked P.L.C. He looked an agonized question at Mr . Carter .
		if ((flags&WordMatch::flagOnlyConsiderProperNounForms) && m.size() && m[m.size()-1].word->first.length()>1 && m[m.size()-1].word->first[m[m.size()-1].word->first.length()-1]==L'.')
		{
			flags&=~WordMatch::flagOnlyConsiderProperNounForms;
			if (debugTrace.traceParseInfo)
				lplog(LOG_INFO,L"%d:removed flagOnlyConsiderProperNounForms.",m.size());
		}
		// if a word is capitalized, but is always followed by another word that is also capitalized, then 
		// don't treat it as an automatic proper noun ('New' York)
		bool isProperNoun=(flags&WordMatch::flagAddProperNoun) && !allCaps && !firstWordInSentence;
		if (isProperNoun && previousIsProperNoun)
		{
			m[m.size() - 1].word->second.numProperNounUsageAsAdjective++;
			if (debugTrace.traceParseInfo)
				lplog(LOG_INFO, L"%d:%s:increased usage of proper noun as adjective to %d.", m.size()-1, m[m.size() - 1].word->first.c_str(), m[m.size() - 1].word->second.numProperNounUsageAsAdjective);
		}
		previousIsProperNoun=isProperNoun;
		// used in disambiguating abbreviated quotes from nested quotes
		if (sWord==secondaryQuoteType)
		{
			if (flagAlphaBeforeHint) flags|=WordMatch::flagAlphaBeforeHint;
			if (flagAlphaAfterHint) flags|=WordMatch::flagAlphaAfterHint;
		}
		if (newsBank && numParagraphsInSection<3)
			flags|=WordMatch::flagMetaData;
		// The description of a green toque , a coat with a handkerchief in the pocket marked P.L.C. He looked an agonized question at Mr . Carter .
		if (firstLetterCapitalized && (sWord==L"he" || sWord==L"she" || sWord==L"it" || sWord==L"they" || sWord==L"we" || sWord==L"you") && m.size() && 
				m[m.size()-1].word->first.length()>1 && m[m.size()-1].word->first[m[m.size()-1].word->first.length()-1]==L'.')
		{
			m[m.size()-1].word->second.flags|=tFI::topLevelSeparator;
			sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd=m.size();
		}
		if (flagNewLineBeforeHint) flags|=WordMatch::flagNewLineBeforeHint;
		m.emplace_back(iWord,flags,debugTrace);
		// check for By artist
		if (webScrapeParse && m.size()>2 && m[m.size()-3].word->first==L"by" && (m[m.size()-3].flags&WordMatch::flagNewLineBeforeHint) &&
			(m[m.size()-1].flags&WordMatch::flagFirstLetterCapitalized) && (m[m.size()-2].flags&WordMatch::flagFirstLetterCapitalized))
		{
			sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd=m.size()+1;
			m.push_back(WordMatch(Words.sectionWord,0,debugTrace));
		}
		// Last modified: 2012-01-29T21:38:56Z
		// Published: Saturday, Jan. 28, 2012 - 12:00 am | Page 11A
		// Last Modified: Sunday, Jan. 29, 2012 - 1:38 pm
		if (webScrapeParse && m.size()>2 && (m[m.size()-3].flags&WordMatch::flagNewLineBeforeHint) &&
			(m[m.size()-1].flags&WordMatch::flagFirstLetterCapitalized) && (m[m.size()-2].flags&WordMatch::flagFirstLetterCapitalized) && bookBuffer[bufferScanLocation]==L':')
		{
			sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd=m.size()+1;
			m.push_back(WordMatch(Words.sectionWord,0,debugTrace));
		}
		int numWords=m.size()-lastSentenceEnd;
		if (numWords>150 || (numWords>100 && (iWord->second.query(conjunctionForm)>=0 || iWord->first==L";")))
		{
			lplog(LOG_ERROR,L"ERROR:Terminating run-on sentence at word %d in source %s (word offset %d).",numWords,path.c_str(),m.size());
			runOnSentences++;
			endSentence=true;
		}
		if (endSentence)
		{
			if (analyzeEnd(path,lastSentenceEnd, m.size(), multipleEnds))
			{
				m.erase(m.begin()+lastSentenceEnd,m.end());
				alreadyAtEnd=true;
				break;
			}
			if (sentenceStarts.size() && lastSentenceEnd==sentenceStarts[sentenceStarts.size()-1]+1)
				sentenceStarts[sentenceStarts.size()-1]++;
			else
				sentenceStarts.push_back(lastSentenceEnd);
			lastSentenceEnd=m.size();
			if ((int)(bufferScanLocation*100/bufferLen)>lastProgressPercent)
			{
				lastProgressPercent=(int)(bufferScanLocation*100/bufferLen);
				wprintf(L"PROGRESS: %03d%% (%06zu words) %I64d out of %I64d bytes read with %d seconds elapsed (%I64d bytes) \r",lastProgressPercent,m.size(),bufferScanLocation,bufferLen,clocksec(),memoryAllocated);
			}
			continue;
		}
	}
	if (!alreadyAtEnd && analyzeEnd(path,lastSentenceEnd, m.size(), multipleEnds))
	{
		m.erase(m.begin() + lastSentenceEnd, m.end());
	}
	else
		sentenceStarts.push_back(lastSentenceEnd);
	lastProgressPercent=(int)(bufferScanLocation*100/bufferLen);
	wprintf(L"PROGRESS: %03d%% (%06zu words) %I64d out of %I64d bytes read with %d seconds elapsed (%I64d bytes) \r",lastProgressPercent,m.size(),bufferScanLocation,bufferLen,clocksec(),memoryAllocated);
	if (runOnSentences>0)
		lplog(LOG_ERROR,L"ERROR:%s:%d sentence early terminations (%d%%)...",path.c_str(),runOnSentences,100*runOnSentences/sentenceStarts.size());
	return 0;
}

int Source::tokenize(wstring title,wstring etext,wstring path,wstring encoding, wstring &start,int &repeatStart,unsigned int &unknownCount,bool newsBank)
{ LFS
	int ret=0;
  if ((ret=readSourceBuffer(title, etext, path, encoding, start, repeatStart))>=0)
	{
		if (wcsncmp(bookBuffer,L"%PDF-",wcslen(L"%PDF-")))
			ret=parseBuffer(path,unknownCount,newsBank);
		else
			lplog(LOG_ERROR,L"%s: Skipped parsing PDF file",path.c_str());
		tfree((int)bufferLen,bookBuffer);
		bufferScanLocation=bufferLen=0;
		bookBuffer=NULL;
	}
	return ret;
}

// 
// >= two blank lines before begin and >= two blank lines after end.
// doesn't begin with a ".
// not more than 10 words long.
// the end is an abbreviation
// it is an expected extension of another chapter heading 
// the end is NOT in a list of endings
bool Source::isSectionHeader(unsigned int begin,unsigned int end,unsigned int &sectionEnd)
{ LFS
	if (begin>0 && m[begin-1].word!=Words.sectionWord) return false; // less than two lines before section header - reject
	if (m[end].word!=Words.sectionWord) return false; // less than two lines after section header - reject
	if (m[begin].word->first==primaryQuoteType || m[begin].word->first==L"“") return false; // buffer begins with " or ' - probably a quote
	if (end-begin>10) return false; // too long
	bool abbreviation=(m[end-1].word->first==L"." && m[end-2].beginPEMAPosition>=0 && 
			(m[end-2].pma.queryPattern(L"_ABB")!=-1 || 
			 m[end-2].pma.queryPattern(L"_MEAS_ABB")!=-1 || 
			 m[end-2].pma.queryPattern(L"_STREET_ABB")!=-1 || 
			 m[end-2].pma.queryPattern(L"_BUS_ABB")!=-1 || 
			 m[end-2].pma.queryPattern(L"_TIME_ABB")!=-1 || 
			 m[end-2].pma.queryPattern(L"_DATE_ABB")!=-1 || 
			 m[end-2].pma.queryPattern(L"_HON_ABB")!=-1));
	if (!abbreviation && 
		(sections.size()<2 || begin-sections[sections.size()-1].endHeader>=2 ||
			sections[sections.size()-1].subHeadings.size()>=sections[sections.size()-2].subHeadings.size()))
	{
		wstring sWord=m[end-1].word->first;
		if (sWord==primaryQuoteType || // buffer ends with " or ' - probably a quote
				sWord==L":" ||              // buffer ends with : - probably the start of a list
				sWord==L"," || sWord == L"-" || sWord == L"—" || sWord==L")" || sWord==L"!" ||
				sWord==L"?" || sWord==L"." || sWord==L"--") return false; // buffer ends with , -, or ) probably not a title
	}
	sectionEnd=end;
	wstring sSection;
	for (unsigned int I=begin; I<end; I++) sSection=sSection+m[I].word->first+L" ";
	sSection.erase(sSection.length()-1);
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:Section header '%s' encountered",begin,sSection.c_str());
	return true;
}

int Source::reportUnmatchedElements(int begin,int end,bool logElements)
{ LFS
	if (!debugTrace.traceParseInfo)
		return 0;
	int len=-1,firstUnmatched=0,totalUnmatched=0;
	bool matched=true;
	for (int I=begin; I<end; )
		if ((m[I].flags&WordMatch::flagTopLevelPattern) || m[I].word->second.isSeparator() ||
			(m[I].flags&WordMatch::flagMetaData))
		{
			if (!matched && logElements)
			{
				if (debugTrace.traceUnmatchedSentences)
				{
					if (firstUnmatched!=I-1)
						lplog(L"Unmatched position %d-%d.",firstUnmatched,I-1);
					else
						lplog(L"Unmatched position %d.",firstUnmatched);
				}
				totalUnmatched+=I-firstUnmatched;
			}
			if (len<0) I++;
			else I+=len;
			matched=true;
		}
		else
		{
			if (matched) firstUnmatched=I;
			matched=false;
			I++;
		}
		if (!matched && logElements)
		{
			if (debugTrace.traceUnmatchedSentences)
			{
				if (firstUnmatched!=m.size()-1)
					lplog(L"Unmatched position %d-%d.",firstUnmatched,end-1);
				else
					lplog(L"Unmatched position %d.",firstUnmatched);
			}
			totalUnmatched+=end-firstUnmatched;
		}
		return totalUnmatched;
}

void Source::consolidateWinners(int begin)
{ LFS
	int *wa=NULL,numWinners=0,position=begin,maxMatch=0;
	pema.generateWinnerConsolidationArray(lastPEMAConsolidationIndex,wa,numWinners);
	vector <WordMatch>::iterator im,imend=m.end();
	for (im=m.begin()+begin; position<lastSourcePositionSet && im!=imend; im++,position++)
	{
		im->pma.consolidateWinners(lastPEMAConsolidationIndex,pema,wa,position,maxMatch,debugTrace); // bool consolidated=
		int position2=position;
		for (vector <WordMatch>::iterator im2=im,im2End=im+maxMatch; im2!=im2End; im2++,position2++)
			im2->maxMatch=max(im2->maxMatch,maxMatch);
		pema.getNextValidPosition(lastPEMAConsolidationIndex,wa,&im->beginPEMAPosition,patternElementMatchArray::BY_POSITION);
		pema.translate(lastPEMAConsolidationIndex,wa,&im->beginPEMAPosition,patternElementMatchArray::BY_POSITION);
		pema.getNextValidPosition(lastPEMAConsolidationIndex,wa,&im->endPEMAPosition,patternElementMatchArray::BY_POSITION);
		pema.translate(lastPEMAConsolidationIndex,wa,&im->endPEMAPosition,patternElementMatchArray::BY_POSITION);
	}
	pema.consolidateWinners(lastPEMAConsolidationIndex,wa,numWinners,debugTrace);
	position=begin;
	for (im=m.begin()+begin; position<lastSourcePositionSet && im!=imend; im++,position++)
	{
		if (im->endPEMAPosition<0 && im->beginPEMAPosition>=0)
		{
			im->endPEMAPosition=im->beginPEMAPosition;
			while (pema[im->endPEMAPosition].nextByPosition>=0)
				im->endPEMAPosition=pema[im->endPEMAPosition].nextByPosition;
		}
		im->PEMACount=pema.generatePEMACount(im->beginPEMAPosition);
		if (!im->PEMACount && (im->flags&WordMatch::flagTopLevelPattern))
		{
			im->flags&=~WordMatch::flagTopLevelPattern;
			if (debugTrace.tracePatternElimination)
				lplog(L"%d:lost all top-level patterns",position);
		}
	}
	lastPEMAConsolidationIndex=max(pema.count,1); // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet=-1;
}

void Source::clearTagSetMaps(void)
{ LFS
	for (unsigned int ts=0; ts<desiredTagSets.size(); ts++)
		pemaMapToTagSetsByPemaByTagSet[ts].clear();
}

int Source::printSentences(bool updateStatistics,unsigned int unknownCount,unsigned int quotationExceptions,unsigned int totalQuotations,int &globalOverMatchedPositionsTotal)
{ LFS
	if (!sentenceStarts.size() || sentenceStarts[sentenceStarts.size()-1]!=m.size())
		sentenceStarts.push_back(m.size());
	int matchedSentences=0,overMatchedPositionsTotal=0,lastProgressPercent=-1,where;
	unsigned int matchedTripletSumTotal=0;
	unsigned int totalUnmatched=0,patternsMatched=0,patternsTried=0;
	bool containsUnmatchedElement,printedHeader=false;
	section=0;
	set <int> wordIds;
	readWordIdsNeedingWordRelations(wordIds);
	if (Words.initializeWordRelationsFromDB(mysql, wordIds,true)<0)
		return -1;
	for (vector <WordMatch>::iterator im = m.begin(), imEnd = m.end(); im != imEnd; im++)
		im->getMainEntry()->second.flags &= ~tFI::inSourceFlag;
	// these patterns exclude NOUN patterns containing other NOUN patterns
	int memoryPerSentenceBySize[512],timePerSentenceBySize[512],sizePerSentenceBySize[512];
	if (debugTrace.collectPerSentenceStats)
	{
		memset(memoryPerSentenceBySize,0,512*sizeof(int));
		memset(timePerSentenceBySize,0,512*sizeof(int));
		memset(sizePerSentenceBySize,0,512*sizeof(int));
	}
	for (unsigned int s=0; s+1<sentenceStarts.size() && !exitNow; s++)
	{
		if ((where=s*100/sentenceStarts.size())>lastProgressPercent)
		{
			wprintf(L"PROGRESS: %d%% sentences printed with %04d seconds elapsed (%I64d bytes) \r",where,clocksec(),memoryAllocated);
			lastProgressPercent=where;
		}
		unsigned int begin=sentenceStarts[s];
		unsigned int end=sentenceStarts[s+1];
		while (end && m[end-1].word==Words.sectionWord)
			end--; // cut off end of paragraphs
		if (begin>=end)
		{
			matchedSentences++;
			continue;
		}
		debugTrace.traceSubjectVerbAgreement = m[begin].t.traceSubjectVerbAgreement;
		debugTrace.traceTestSubjectVerbAgreement = m[begin].t.traceTestSubjectVerbAgreement;
		debugTrace.traceAnaphors=m[begin].t.traceAnaphors;
		debugTrace.traceEVALObjects=m[begin].t.traceEVALObjects;
		debugTrace.traceRelations=m[begin].t.traceRelations;
		debugTrace.traceRole=m[begin].t.traceRole;
		debugTrace.traceVerbObjects=m[begin].t.traceVerbObjects;
		debugTrace.traceDeterminer=m[begin].t.traceDeterminer;
		debugTrace.printBeforeElimination=m[begin].t.printBeforeElimination;
		debugTrace.tracePatternElimination=m[begin].t.tracePatternElimination;
		debugTrace.traceBNCPreferences=m[begin].t.traceBNCPreferences;
		debugTrace.traceSecondaryPEMACosting=m[begin].t.traceSecondaryPEMACosting;
		debugTrace.traceMatchedSentences= m[begin].t.traceMatchedSentences;
		debugTrace.traceUnmatchedSentences= m[begin].t.traceUnmatchedSentences;
		debugTrace.tracePreposition = m[begin].t.tracePreposition;
		logCache=m[begin].logCache;

		// clear tag set maps
		clearTagSetMaps();
		int sentenceStartTime=clock();
		int sentenceStartMemory=pema.count;
		patternsMatched+=matchPatternsAgainstSentence(s,patternsTried);
		if (debugTrace.printBeforeElimination)
			printSentence(SCREEN_WIDTH,begin,end,false);  // print before elimination
		for (unsigned int I=begin; I<end && !exitNow; I++) 
		{
			patternMatchArray::tPatternMatch *pma=m[I].pma.content;
			for (int PMAElement=0,count=m[I].pma.count; PMAElement<count; PMAElement++,pma++)
				addCostFromRecalculatingAloneness(I,pma);
		}
		int overMatchedPositions=eliminateLoserPatterns(begin,end);
		if (debugTrace.collectPerSentenceStats)
			memoryPerSentenceBySize[end-begin]+=pema.count-sentenceStartMemory;
		if (debugTrace.printBeforeElimination)
			printSentence(SCREEN_WIDTH,begin,end,false);  // print after costing is assessed
		consolidateWinners(begin);
		// clear them because the PEMA references are invalid after consolidation.
		clearTagSetMaps();
		//if (section<sections.size() && sections[section].begin==begin)
		//  section++;
		unsigned int unmatchedElements=reportUnmatchedElements(begin,end,true);
		unsigned int ignoredPatternsTried=0;
		matchIgnoredPatternsAgainstSentence(s,ignoredPatternsTried,true);
		lastPEMAConsolidationIndex=max(pema.count,1); // minimum PEMA offset is 1 - not 0
		totalUnmatched+=unmatchedElements;
		if (sumMaxLength(begin,end,matchedTripletSumTotal,matchedSentences,containsUnmatchedElement))
		{
			if (debugTrace.traceUnmatchedSentences || debugTrace.traceMatchedSentences)
			{
				if (containsUnmatchedElement && !printedHeader)
				{
					lplog(LOG_NOTMATCHED,L"\n%s\n", storageLocation.c_str());
					printedHeader=true;
				}
				if (debugTrace.traceMatchedSentences)
					printSentence(SCREEN_WIDTH,begin,end,containsUnmatchedElement);  // change DOS Window Menu "Defaults" choice
			}
		}
#ifdef LOG_OVERMATCHED
		else if (overMatchedPositions)
			printSentence(SCREEN_WIDTH,begin,end,containsUnmatchedElement);  // change DOS Window Menu "Defaults" choice
#endif
		overMatchedPositionsTotal+=overMatchedPositions;
		if (debugTrace.collectPerSentenceStats)
		{
			timePerSentenceBySize[end-begin]+=clock()-sentenceStartTime;
			sizePerSentenceBySize[end-begin]+=end-begin;
		}
		unsigned int sectionEnd;
		if (isSectionHeader(begin,end,sectionEnd))
		{
			if (sections.size() && begin-sections[sections.size()-1].endHeader<2)
			{
				sections[sections.size()-1].endHeader=sectionEnd;
				sections[sections.size()-1].subHeadings.push_back(begin);
			}
			else
				sections.push_back(cSection(begin,sectionEnd));
		}
	}
	if (100>lastProgressPercent)
		wprintf(L"PROGRESS: 100%% sentences printed with %04d seconds elapsed (%I64d bytes) \r",clocksec(),memoryAllocated);
	// accumulate globals
	globalOverMatchedPositionsTotal+=overMatchedPositionsTotal;
	if (debugTrace.collectPerSentenceStats)
	{
		lplog(L"CPSS SIZE MEMT    TIMET MEM  TIME %%");
		for (unsigned I=0; I<256; I++)
			if (sizePerSentenceBySize[I])
				lplog(L"CPSS %03d: %07d %05d %04d %03d %02d%%",I,memoryPerSentenceBySize[I],timePerSentenceBySize[I]/CLOCKS_PER_SEC,memoryPerSentenceBySize[I]/sizePerSentenceBySize[I],timePerSentenceBySize[I]/sizePerSentenceBySize[I],sizePerSentenceBySize[I]*100/m.size());
	}
	// [222 out of 2374:009%] 03333606 words Parsing A75...
	if (updateStatistics)
		updateSourceStatistics(sentenceStarts.size()-1,matchedSentences,m.size(),unknownCount,
			totalUnmatched,overMatchedPositionsTotal,totalQuotations,quotationExceptions,clock()-beginClock,matchedTripletSumTotal);
	extern int numSourceLimit;
	if (m.size() && numSourceLimit==0)
		lplog(L"%-50.49s:Matched sentences=%06.2f%% Positions:(Total=%06d Unknown=%05d-%5.2f%% Unmatched=%04d-%5.2f%% Overmatched=%05d-%5.2f%%) Quotation exceptions=%03d-%5.2f%% MS/word=%05.3f Average pattern match:%02d ",
			storageLocation.c_str(),(float)matchedSentences*100/(sentenceStarts.size()-1),m.size(),
		unknownCount,(float)unknownCount*100/m.size(),totalUnmatched,(float)totalUnmatched*100.0/m.size(),overMatchedPositionsTotal,(float)overMatchedPositionsTotal*100/m.size(),
		quotationExceptions,(totalQuotations) ? (float)quotationExceptions*100/totalQuotations : 0,
		(float)(clock()-beginClock)/((float)CLOCKS_PER_SEC/1000*m.size()),matchedTripletSumTotal/m.size());
  #ifdef LOG_PATTERN_STATISTICS
	lplog(L"Matched Patterns=%d Tried Patterns=%d.",patternsMatched,patternsTried);
  #endif
	return totalUnmatched;
}

int Source::printSentencesCheck(bool skipCheck)
{ LFS
	struct __stat64 buffer;
	wchar_t logFilename[1024];
	extern wstring logFileExtension;
	wsprintf(logFilename,L"main%S.lplog",logFileExtension.c_str());
	vector <WordMatch>::iterator im=m.begin(),mend=m.end();
	for (unsigned int I=0; im!=mend; im++,I++)
	{
		if (im->flags&WordMatch::flagAddProperNoun)
		{
			im->word->second.setProperNounUsageCost();
		}
	}
	if (!skipCheck && _wstat64(logFilename,&buffer)>=0 && buffer.st_size>2*1024*1024) return 0;
	if (sentenceStarts.size()==0 || !(m[sentenceStarts[0]].t.traceMatchedSentences ^ (logMatchedSentences | logUnmatchedSentences))) return 0;
	for (unsigned int s=0; s+1<sentenceStarts.size() && !exitNow; s++)
	{
		unsigned int begin=sentenceStarts[s],end=sentenceStarts[s+1];
		while (end && m[end-1].word==Words.sectionWord)
			end--; // cut off end of paragraphs
		if (begin>=end)
			continue;
		logCache=m[begin].logCache;
		if (m[begin].t.traceMatchedSentences ^ (logMatchedSentences|logUnmatchedSentences))
			printSentence(SCREEN_WIDTH,begin,end,false);  // print before elimination
	}
	return 0;
}

int Source::matchIgnoredPatternsAgainstSentence(unsigned int s,unsigned int &patternsTried,bool fill)
{ LFS
	int numPatternsMatched=0;
	for (unsigned int p=0; p<patterns.size(); p++)
	{
		if (!patterns[p]->ignoreFlag) continue;
		patternsTried++;
		if (matchPatternAgainstSentence(patterns[p],s,fill))
			numPatternsMatched++;
	}
	return numPatternsMatched;
}

#ifdef LOG_OLD_MATCH
int Source::matchPatternsAgainstSentence(unsigned int s,unsigned int &patternsTried)
{ LFS
	bool match=false;
	pass=0;
	int patternsMatched=0;
	// run through every pattern up to the last pattern referred to by any pattern
	for (int p=0; p<=patternReference::lastPatternReference; p++,patternsTried++)
	{
		if (!patterns[p]->ignore && matchPatternAgainstSentence('A',patterns[p],s))
		{
#ifdef LOG_IPATTERN
			lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.",'A',p,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),s);
#endif
			match|=patterns[p]->isFutureReference;
			patternsMatched++;
		}
	}
	if (match && patternReference::lastPatternReference!=-1)
	{
		pass=1;
		// run through every pattern that refers to another future pattern
		match=false;
		for (int p=patternReference::firstPatternReference; p<=patternReference::lastPatternReference; p++)
			if (patterns[p]->containsFutureReference)
			{
				if (!patterns[p]->ignore && matchPatternAgainstSentence('B',patterns[p],s))
				{
					match=true;
#ifdef LOG_IPATTERN
					lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.",'B',p,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),s);
#endif
					patternsMatched++;
				}
				patternsTried++;
			}
			// run through every pattern that refers to the patterns that refer to other patterns
			if (match)
				for (int p=patternReference::firstPatternReference; p<=patternReference::lastPatternReference; p++)
					if (patterns[p]->indirectFutureReference)
					{
						if (!patterns[p]->ignore && matchPatternAgainstSentence('C',patterns[p],s))
						{
#ifdef LOG_IPATTERN
							lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.",'C',p,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),s);
#endif
							patternsMatched++;
						}
						patternsTried++;
					}
	}
	// run through the last patterns that are not referred to by any other pattern
	pass=0;
	for (int p=patternReference::lastPatternReference+1; p<(int)patterns.size(); p++,patternsTried++)
		if (!patterns[p]->ignore && matchPatternAgainstSentence('D',patterns[p],s))
		{
#ifdef LOG_IPATTERN
			lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.",'D',p,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),s);
#endif
			patternsMatched++;
		}
		return patternsMatched;
}
#else
// start with patterns having no children
// pass=0
// patternsToTryNext=patterns having no children  (childrenPatterns=0)
// primaryPatterns=patterns having no parents (ancestorPatterns=0)
// for two passes:
//   triedPatterns=patternsWithNoParents
//   for each of the patterns in patternsToTryNext not in tried patterns:
//     add pattern to triedPatterns
//     matchPattern
//     if it matches:
//       accumulate its ancestorPatterns into patternsToTryNext
//       tryAgainPatterns|=(ancestorPatterns&triedPatterns);
//   if pass==0
//     savePatternsToTryNext=patternsToTryNext
//     patternsToTryNext=tryAgainPatterns
//     pass++
// primaryPatterns&=(savePatternsToTryNext|patternsToTryNext);
// for each pattern in primaryPatterns
//   matchPattern
//#define LOG_IPATTERN
int Source::matchPatternsAgainstSentence(unsigned int s,unsigned int &patternsTried)
{ LFS
	int numPatternsMatched=0;
	bitObject<32, 5, unsigned int, 32> patternsToTry,patternsToTryLast,patternsToTryNext=patternsWithNoChildren,patternsMatched;
	for (pass=0; pass<6; pass++)
	{
		patternsToTry=patternsToTryNext;
#ifdef LOG_IPATTERN
		wstring patternsString;
		int lastP=-1;
		for (int p=patternsToTry.first(); p>=0; lastP=p,p=patternsToTry.next())
		{
			if (lastP!=-1 && patterns[p]->name==patterns[lastP]->name)
				patternsString+=","+patterns[p]->differentiator;
			else
			{
				if (lastP!=-1) patternsString+="] ";
				patternsString+=patterns[p]->name+"["+patterns[p]->differentiator;
			}
		}
		lplog(L"%c:Patterns %s matched against sentence %04d.",'A'+pass,patternsString.c_str(),s);
#endif
		patternsToTryNext.clear();
		for (int p=patternsToTry.first(); p>=0; p=patternsToTry.next())
		{
			patternsToTryNext.reset(p);
			if (!patterns[p]->mandatoryChildPatterns.hasAll(patternsMatched)) continue;
			patternsTried++;
			if (!patterns[p]->ignoreFlag && matchPatternAgainstSentence(patterns[p],s,true))
			{
				patternsToTryNext|=patterns[p]->ancestorPatterns;
				patternsMatched.set(p);
				numPatternsMatched++;
			}
		}
		patternsToTryLast|=patternsToTryNext;
		patternsToTryNext-=patternsWithNoParents;
	}
	patternsToTryLast&=patternsWithNoParents;
	pass=-1;
	for (int p=patternsToTryLast.first(); p>=0; p=patternsToTryLast.next())
	{
		if (!patterns[p]->mandatoryChildPatterns.hasAll(patternsMatched)) continue;
		patternsTried++;
		if (!patterns[p]->ignoreFlag && matchPatternAgainstSentence(patterns[p],s,true))
		{
#ifdef LOG_IPATTERN
			lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.",'A'+pass,p,patterns[p]->name.c_str(),patterns[p]->differentiator.c_str(),s);
#endif
			numPatternsMatched++;
		}
	}
	return numPatternsMatched;
}
#endif

void Source::setForms(void)
{ LFS
	m[0].forms.check(L"forms",Forms.size());
	m[0].patterns.check(L"patterns",patterns.size());
	for (vector <WordMatch>::iterator I=m.begin(),IEnd=m.end(); I!=IEnd; I++) I->setForm();
}

void Source::clearSource(void)
{ LFS
	storageLocation.clear();
	relatedObjectsMap.clear();
	for (unsigned int I=0; I<m.size(); I++)
		m[I].pma.clear();
	pema.clear();
	m.clear();
	subNarratives.clear();
	sentenceStarts.erase();
	primaryQuoteType=L"\"";
	secondaryQuoteType=L"\'";
	printMaxSize.clear();
	whatMatched.clear();
	objects.clear();
	localObjects.clear();
	unresolvedSpeakers.clear();
	lastPEMAConsolidationIndex=1; // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet=-1;
	primaryLocationLastMovingPosition=-1;
	primaryLocationLastPosition=-1;
	firstQuote=lastQuote=-1;
	gTraceSource=0;
	clearTagSetMaps();
	speakerGroups.clear();
	povInSpeakerGroups.clear(); // keeps track of point-of-view objects throughout text to be dropped into speakerGroups
	metaNameOthersInSpeakerGroups.clear(); // keeps track of objects named as a third person throughout text to be dropped into speakerGroups
	definitelyIdentifiedAsSpeakerInSpeakerGroups.clear(); // keeps track of definitely identified speakers to be dropped into speakerGroups
	nextNarrationSubjects.clear(); // only maintained during identifySpeakerGroups (no IS_OBJECTs)
	lastISNarrationSubjects.clear(); // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	whereLastISNarrationSubjects.clear(); // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	nextISNarrationSubjects.clear(); // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	whereNextISNarrationSubjects.clear(); // only maintained during identifySpeakerGroups (only subjects which are also IS_OBJECT)
	speakerAges.clear(); // associated with tempSpeakerGroup speakers
	speakerSections.clear();  // the sourcePosition of each speakerGroup piece encountered in the current speaker group
	subjectsInPreviousUnquotedSection.clear();
	introducedByReference.clear(); // objects that are introduced in a group that is non-gendered - The four paintings of Mephistopheles
	collectTagsFocusPositions.clear();
	secondaryPEMAPositions.clear();
	questionSubjectAgreementMap.clear();
	sentenceStarts.erase();
	masterSpeakerList.clear();
	sections.clear();
	singularizedObjects.clear();
	previousSpeakers.clear();
	beforePreviousSpeakers.clear();
	unresolvedSpeakers.clear();
	spaceRelations.clear();
	timelineSegments.clear();
	for (unordered_map <wstring, Source *>::iterator smi = sourcesMap.begin(); smi != sourcesMap.end();)
	{
		Source *source = smi->second;
		source->clearSource();
		delete source;
		sourcesMap.erase(smi);
		smi = sourcesMap.begin();
	}
	if (updateWordUsageCostsDynamically)
		WordClass::resetUsagePatternsAndCosts(debugTrace);
	else
		WordClass::resetCapitalizationAndProperNounUsageStatistics(debugTrace);
}

int read(string &str,IOHANDLE file)
{ LFS
	int size;
	_read(file,&size,sizeof(size));
	if (size<1023)
	{
		char contents[1024];
		_read(file,contents,size);
		contents[size]=0;
		if (size) str=contents;
	}
	else
	{
		char *contents=(char *)tmalloc(size+4);
		_read(file,contents,size);
		contents[size]=0;
		str=contents;
		free(contents);
	}
	return 0;
}

#define MAX_BUF (10*1024*1024)
bool Source::flush(int fd, void *buffer, int &where)
{
	LFS
		if (where > MAX_BUF - 64 * 1024)
		{
			if (::write(fd, buffer, where) < 0) return false;
			where = 0;
		}
	return true;
}

bool Source::FlushFile(HANDLE fd, void *buffer, int &where)
{
	LFS
	if (where > MAX_BUF - 64 * 1024)
	{
		DWORD bytesWritten;
		if (!WriteFile(fd, buffer, where,&bytesWritten,NULL) || bytesWritten!=where) 
			return false;
		where = 0;
	}
	return true;
}

bool Source::writeCheck(wstring path)
{ LFS
	path+=L".SourceCache";
	//return _waccess(path.c_str(),0)==0; // long path limitation
	return GetFileAttributesW(path.c_str())!= INVALID_FILE_ATTRIBUTES;
}

bool Source::writePatternUsage(wstring path,bool zeroOutPatternUsage)
{
	path += L".patternUsage";
	HANDLE fd = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (fd == INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR, L"Unable to open source %s - %s", path.c_str(), lastErrorMsg().c_str());
		return false;
	}
	char buffer[MAX_BUF];
	int where = 0;
	for (auto p : patterns)
	{
		p->copyUsage(buffer, where, MAX_BUF);
		if (zeroOutPatternUsage)
			p->zeroUsage();
		if (!FlushFile(fd, buffer, where)) return false;
	}
	if (where)
	{
		DWORD dwBytesWritten;
		if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
			return false;
		where = 0;
	}
	CloseHandle(fd);
	return true;
}

// if returning false, the file will not be closed.
bool Source::write(wstring path,bool S2, bool makeCopyBeforeSourceWrite, wstring specialExtension)
{ LFS
	path+=L".SourceCache"+specialExtension;
	// int fd=_wopen(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD | _S_IWRITE); subject to short path restriction
	if (makeCopyBeforeSourceWrite)
	{
		wstring renamePath = path + L".old";
		if (_wremove(renamePath.c_str()) < 0 && errno!=ENOENT)
			lplog(LOG_FATAL_ERROR, L"REMOVE %s - %S", renamePath.c_str(), sys_errlist[errno]);
		else if (_wrename(path.c_str(), renamePath.c_str()) && errno != ENOENT)
			lplog(LOG_FATAL_ERROR, L"RENAME %s to %s - %S", path.c_str(), renamePath.c_str(), sys_errlist[errno]);
	}
	HANDLE fd = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,0);
	if (fd== INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR, L"Unable to open source %s - %s", path.c_str(),lastErrorMsg().c_str());
		return false;
	}
	char buffer[MAX_BUF];
	int where=0;
	DWORD dwBytesWritten;
	int sourceVersion=SOURCE_VERSION;
	if (!copy(buffer,sourceVersion,where,MAX_BUF)) return false;
	if (!copy(buffer, storageLocation,where,MAX_BUF)) return false;
	unsigned int count=m.size();
	if (!copy((void *)buffer,count,where,MAX_BUF)) return false;
	vector <WordMatch>::iterator im=m.begin(),mend=m.end();
	for (int I=0; im!=mend; im++,I++)
	{
		getBaseVerb(I,14,im->baseVerb);
		if (!im->writeRef(buffer,where,MAX_BUF)) return false;
		if (!FlushFile(fd,buffer,where)) return false;
	}
	if (where)
	{
		if (!WriteFile(fd, buffer, where, &dwBytesWritten,NULL) || where != dwBytesWritten)
			return false;
		where=0;
	}
	if (!sentenceStarts.write(buffer,where,MAX_BUF)) return false;
	if (where>=MAX_BUF-1024)
		return false;
	count=sections.size();
	if (!copy((void *)buffer,count,where,MAX_BUF)) return false;
	for (unsigned int I=0; I<count; I++)
	{
		if (!sections[I].write(buffer,where,MAX_BUF)) return false;
		if (!FlushFile(fd,buffer,where)) return false;
	}
	count=speakerGroups.size();
	if (!copy((void *)buffer,count,where,MAX_BUF)) return false;
	for (unsigned int I=0; I<count; I++)
	{
		if (!speakerGroups[I].copy(buffer,where,MAX_BUF)) return false;
		if (!FlushFile(fd,buffer,where)) return false;
	}
	if (where)
	{
		if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
			return false;
		where=0;
	}
	pema.WriteFile(fd);
	if (S2)
	{
		count=objects.size();
		if (!copy((void *)buffer,count,where,MAX_BUF)) return false;
		for (vector <cObject>::iterator o=objects.begin(),oEnd=objects.end(); o!=oEnd; o++)
		{
			o->write(buffer,where,MAX_BUF);
			if (!FlushFile(fd,buffer,where)) return false;
		}
		if (where)
		{
			if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
				return false;
			where=0;
		}
		copy(buffer,(int)spaceRelations.size(),where,MAX_BUF);
		for (vector <cSpaceRelation>::iterator sri=spaceRelations.begin(), sriEnd=spaceRelations.end(); sri!=sriEnd; sri++)
		{
			sri->nextSPR=(int) (sri-spaceRelations.begin());
			srToText(sri->nextSPR,sri->description);
			if (!sri->write(buffer,where,MAX_BUF)) break;
			if (!FlushFile(fd,buffer,where)) return false;
		}
		count=timelineSegments.size();
		if (!copy((void *)buffer,count,where,MAX_BUF)) return false;
		for (vector <cTimelineSegment>::iterator tl=timelineSegments.begin(),tlEnd=timelineSegments.end(); tl!=tlEnd; tl++)
		{
			if (!tl->copy(buffer,where,MAX_BUF)) return false;
			if (!FlushFile(fd,buffer,where)) return false;
		}
		if (where)
		{
			if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
				return false;
			where=0;
		}
	}
	CloseHandle(fd);
	return true;
}
	 
bool Source::read(char *buffer,int &where,unsigned int total, bool &parsedOnly, bool printProgress, bool readOnlyParsed, wstring specialExtension)
{ LFS
  int sourceVersion;
	if (!copy(sourceVersion,buffer,where,total)) return false;
	if (sourceVersion != SOURCE_VERSION)
	{
		lplog(LOG_WHERE, L"%s rejected as old.  Reparsing.", sourcePath.c_str());
		return false;
	}
	if (!copy(storageLocation,buffer,where,total)) return false;
	unsigned int count;
	unsigned int lastProgressPercent=0;
	if (!copy(count,buffer,where,total)) return false;
	if (!count) return true;
	m.reserve(m.size()+count);
	bool error=false;
	while (count-- && !error && where<(signed)total)
	{ LFSL
		if ((where*100/total)>lastProgressPercent && printProgress)
			wprintf(L"PROGRESS: %03d%% source read with %d seconds elapsed \r",lastProgressPercent=where*100/total,clocksec());
		m.emplace_back(buffer,where,total,error);
	}
	if (error || where >= (signed)total)
	{
		lplog(LOG_ERROR, L"%s read error at position %d, read buffer location %d (out of %d) - error %d.", sourcePath.c_str(),m.size(),where,total,error);
		return false;
	}
	sentenceStarts.read(buffer,where,total);
	if ((where*100/total)>lastProgressPercent && printProgress)
		wprintf(L"PROGRESS: %03d%% source read with %d seconds elapsed \r",lastProgressPercent=where*100/total,clocksec());
	if (!copy(count,buffer,where,total)) return false;
	for (unsigned int I=0; I<count && !error; I++)
		sections.push_back(cSection(buffer,where,total,error));
	if ((where*100/total)>lastProgressPercent && printProgress)
		wprintf(L"PROGRESS: %03d%% source read with %d seconds elapsed \r",lastProgressPercent=where*100/total,clocksec());
	if (!copy(count,buffer,where,total)) return false;
	for (unsigned int I=0; I<count && !error; I++)
		speakerGroups.push_back(cSpeakerGroup(buffer,where,total,error));
	if (error || !pema.read(buffer,where,total)) return false;
	if (where == total || readOnlyParsed)
	{
		parsedOnly = where == total;
		if (printProgress)
			wprintf(L"PROGRESS: 100%% source read with %d seconds elapsed \n", clocksec());
		return true;
	}
	if (!copy(count,buffer,where,total)) return false;
	for (unsigned int I=0; I<count && !error; I++)
		objects.push_back(cObject(buffer,where,total,error));
	if (!copy(count, buffer, where, total)) return false;
	for (unsigned int I = 0; I < count && !error; I++)
	{
		spaceRelations.push_back(cSpaceRelation(buffer, where, total, error));
		getSRIMinMax(&spaceRelations[spaceRelations.size() - 1]);
	}
	if (!copy(count, buffer, where, total)) return false;
	for (unsigned int I = 0; I < count && !error; I++)
		timelineSegments.push_back(cTimelineSegment(buffer, where, total, error));
	// fill locations
	unsigned int I=0;
	for (vector <WordMatch>::iterator im=m.begin(),imEnd=m.end(); im!=imEnd; im++,I++)
	{
		if (im->getObject()>=0 && im->getObject()<(signed)objects.size())
			objects[im->getObject()].locations.push_back(cObject::cLocation(I));
		for (vector <cOM>::iterator omi=im->objectMatches.begin(),omiEnd=im->objectMatches.end(); omi!=omiEnd; omi++)
			if (omi->object>=0 && omi->object<(signed)objects.size())
				objects[omi->object].locations.push_back(cObject::cLocation(I));
	}
	if (printProgress)
		wprintf(L"PROGRESS: 100%% source read with %d seconds elapsed \n",clocksec());
	return !error;
}

// find the first title that is followed by two consecutive sentences in a paragraph.
// *** END OF THE PROJECT 
wchar_t *ignoreWords[]={L"GUTENBERG",L"COPYRIGHT",L"THIS HEADER SHOULD BE THE FIRST THING",L"THE WORLD OF FREE PLAIN VANILLA ELECTRONIC TEXTS",L"EBOOKS ",L"ETEXT",
L"PLEASE TAKE A LOOK AT THE IMPORTANT INFORMATION IN THIS HEADER",
L"WE ARE NOW TRYING TO RELEASE ALL OUR BOOKS",
L"FOR THESE AND OTHER MATTERS, PLEASE MAIL TO",
L"PLEASE DO NOT REMOVE THIS",
L"THIS SHOULD BE THE FIRST THING SEEN",
L"EBOOK",L"TITLE:",L"AUTHOR:",L"RELEASE DATE:",L"MOST RECENTLY UPDATED:",L"EDITION:",L"LANGUAGE:",
L"CHARACTER SET ENCODING:",L"THIS FILE SHOULD BE NAMED",L"VERSIONS BASED ON SEPARATE SOURCES",L"WEB SITES",L"HTTP",
L"FTP",L"NEWSLETTERS",L"HERE IS THE BRIEFEST RECORD OF OUR PROGRESS ",L"ANY MONTH",L"DONATIONS",L"DONATION",L"CONTRIBUTIONS",
L"STATES",L"QUESTIONS",L"809 NORTH 1500 WEST",L"SALT LAKE CITY",L"CONTACT US IF YOU WANT TO ARRANGE",L"***",L"HART",
L"EMAIL",L"**THE LEGAL SMALL PRINT**",L"(THREE PAGES)",L"LIMITED WARRANTY; DISCLAIMER OF DAMAGES",
L"INDEMNITY",L"ILLUSTRATION:",
L"ASCII CHARACTER SET",
L"MAC USERS, DO NOT POINT AND CLICK",NULL};

//end of line is \r (13) then \n (10)
bool getNextLine(int &where,wstring &buffer,wstring &line,bool &eofEncountered,int &firstNonBlank)
{ LFS
	size_t nextLineStart=buffer.find(L'\n',where);
	if (nextLineStart==wstring::npos) eofEncountered=true;
	nextLineStart++;
	//if (buffer[nextLineStart]==L'\n') nextLineStart++;
	line=buffer.substr(where,nextLineStart-where);
	where=nextLineStart;
	firstNonBlank=0;
	while (firstNonBlank<(signed)line.length() && iswspace(line[firstNonBlank]))
		firstNonBlank++;
	return firstNonBlank<(signed)line.length();
}

bool containsSectionHeader(wstring &line,int firstNonBlank)
{ LFS
		bool csh=false;
		if (line.length()>0)
		{
			wstring upline=line;
			wcsupr((wchar_t *)upline.c_str());
			wchar_t *sectionheader[] = {L"BOOK",L"CHAPTER",L"PART",L"PROLOGUE",L"EPILOGUE",L"VOLUME",L"STAVE",L"I.",NULL};
			for (int I=0; sectionheader[I]; I++)
			{
				if (csh=(!wcsncmp(upline.c_str()+firstNonBlank,sectionheader[I],wcslen(sectionheader[I]))))
					break;
			}
		}
		return csh;
}

bool containsSectionHeader(wstring &line)
{ LFS
		int fnb=0;
		while (fnb<(signed)line.length() && iswspace(line[fnb]))
			fnb++;
		return containsSectionHeader(line,fnb);
}

bool containsAttribution(wstring &line,int firstNonBlank)
{ LFS
		bool ca=false;
		if (line.length()>0)
		{
			wstring upline=line;
			wcsupr((wchar_t *)upline.c_str());
			wchar_t *attribution[] = {L"SCANNED BY",L"PRODUCED BY",L"WRITTEN BY",L"TRANSCRIBED BY",L"PRODUCED BY",L"PREFACE BY",
				L"TRANSCRIBER'S NOTE",L"AUTHOR'S NOTE",L"WITH ILLUSTRATIONS BY",L"DEDICATION",L"AUTHOR'S INTRODUCTION",L"PREFATORY NOTE",L"TO MY READERS",
				L"ADDRESSED TO THE READER",NULL};
			for (int I=0; attribution[I]; I++)
			{
				if (ca=(!wcsncmp(upline.c_str()+firstNonBlank,attribution[I],wcslen(attribution[I]))))
					break;
			}
		}
		return ca;
}

bool isRomanNumeral(wstring line,int where)
{ LFS
	bool matches=false;
	extern wchar_t *roman_numeral[];
	wstring rn;
	while (where<(signed)line.length() && line[where]!=L'.' && !iswspace(line[where]))
		rn+=towlower(line[where++]);
	for (int I=0; roman_numeral[I]; I++)
		if (matches=rn==roman_numeral[I])
			break;
	return matches;
}

bool getNextParagraph(int &where,wstring &buffer,bool &eofEncountered,int &firstNonBlank,wstring &paragraph,int &numLines,int &numIndentedLines,int &numHeaderLines,int &numStartLines,int &numShortLines)
{ LFS
		int fnb=0;
		while (where<(signed)buffer.length() && iswspace(buffer[where]))
			where++;
		wstring line;
		while (getNextLine(where,buffer,line,eofEncountered,fnb) && !eofEncountered)
		{
			if (numLines==0) firstNonBlank=fnb;
			if (fnb>0) numIndentedLines++;
			if (containsSectionHeader(line,fnb)) numHeaderLines++;
			if (line.length()<40) numShortLines++;
			if (iswdigit(line[fnb]) || (isRomanNumeral(line,fnb) && wcsncmp(line.c_str()+fnb,L"I ",2))) 
			{
				numStartLines++;
				if (_wtoi((wchar_t *)line.c_str())==1)
				{
					size_t whereSeparator=line.find(L'.');
					if (whereSeparator == wstring::npos)
						whereSeparator = line.find(L'-');
					if (whereSeparator == wstring::npos)
						whereSeparator = line.find(L'—');
					if (whereSeparator!=wstring::npos)
					{
						whereSeparator++;
						while (whereSeparator<(int)line.length() && iswspace(line[whereSeparator]))
							whereSeparator++;
						wstring firstTitle=line.substr(whereSeparator,line.length());
						if ((whereSeparator=buffer.find(firstTitle,where))!=wstring::npos && whereSeparator<(int)buffer.length()/5)
						{
							// go to beginning of line
							while (whereSeparator>0 && buffer[whereSeparator-1]!=L'\n')
								whereSeparator--;
							if (whereSeparator>0)
							{
								paragraph.clear();
								where=whereSeparator;
								numLines=numIndentedLines=numStartLines=numHeaderLines=0;
								continue;
							}
						}
					}
				}
			}
			paragraph+=line;
			numLines++;
		}
		return !paragraph.empty();
}

bool ci_equal(wchar_t ch1, wchar_t ch2)
{ LFS
	return towupper(ch1) == towupper(ch2);
}

size_t noCasefind(const wstring& str1, const wstring& str2,int position)
{ LFS
	wstring::const_iterator pos = search(str1. begin()+position, str1.end(), str2.begin(), str2.end(), ci_equal);
	if (pos == str1.end())
		return wstring::npos;
	else
		return pos - str1.begin();
}

// get next line, accumulating lines until a blank line
// if contains ignoreWord, set lastLine to '' and go to beginning
// if there is no period, save in lastLine and goto beginning.
// otherwise return true.
bool findNextParagraph(int &where,wstring &buffer,wstring &lastLine,int &whereLastLine)
{ LFS
	wstring paragraph;
	bool alreadySkipped=false;
	int lastNumLines=0,lastShortLines=0;
	while (true)
	{
		paragraph.clear();
		whereLastLine=where;
		int indentedLines=0,numLines=0,headerLines=0,numStartLines=0,firstNonBlank=0,numShortLines=0;
		bool eofEncountered=false;
		getNextParagraph(where,buffer,eofEncountered,firstNonBlank,paragraph,numLines,indentedLines,headerLines,numStartLines,numShortLines);
		if (eofEncountered) return false;
		if (paragraph.empty()) continue;
		if (indentedLines>1 || headerLines>1 || numStartLines>1) continue; // index or contents
		wstring saveParagraph=paragraph;
		wcsupr((wchar_t *)paragraph.c_str());
		bool containsIgnoreSequence=false;
		for (int I=0; ignoreWords[I]; I++)
		{
			if (containsIgnoreSequence=(paragraph.find(ignoreWords[I])!=wstring::npos))
				break;
		}
		if (containsIgnoreSequence) continue;
		if (!wcsncmp(paragraph.c_str()+firstNonBlank,L"AUTHOR OF",wcslen(L"AUTHOR OF")))
			continue;
		if (!wcsncmp(paragraph.c_str()+firstNonBlank,L"AUTHORS OF",wcslen(L"AUTHORS OF")))
			continue;
		while (iswspace(saveParagraph[saveParagraph.length()-1]))
			saveParagraph.erase(saveParagraph.length()-1);
		// contains at least two periods?  if so, break.
		int numPeriods=0;
		for (int p=3; p<(signed)saveParagraph.length(); p++)
		{
			if (saveParagraph[p]==L'.' && saveParagraph[p-2]!=L'.' && saveParagraph[p-3]!=L'.' && (!iswspace(saveParagraph[p-2]) || iswspace(saveParagraph[p+1])))
				numPeriods++;
			if (saveParagraph[p]==L'?' || saveParagraph[p]==L'!')
				numPeriods++;
		}
		bool endsWithEOS=saveParagraph[saveParagraph.length()-1]==L'.' || saveParagraph[saveParagraph.length()-1]==L'?' || saveParagraph[saveParagraph.length()-1]==L'!' ||
			               WordClass::isSingleQuote(saveParagraph[saveParagraph.length()-1]) || WordClass::isDoubleQuote(saveParagraph[saveParagraph.length() - 1]) ||
										 WordClass::isDash(saveParagraph[saveParagraph.length() - 1]);
		bool ccsh=containsSectionHeader(paragraph);
		if (numPeriods<2 || !endsWithEOS || saveParagraph==paragraph || lastLine.empty() || ccsh)
		{
			// contains at least two lines, and there is only one period at the end of the line
			if (numLines>1 && endsWithEOS && headerLines<=1 && !lastLine.empty() && lastLine.length()<255 && !containsAttribution(lastLine,firstNonBlank))
				break;
			bool csh=containsSectionHeader(lastLine);
			// if contains only one line, and it has a period at the end, and the previous lastLine contains a section header
			if (csh && endsWithEOS && !ccsh)
				break;
			// if the lastLine doesn't contain a section header
			if (!csh || alreadySkipped || ccsh)
			{
				alreadySkipped=false;
				lastLine=saveParagraph;
				lastNumLines=numLines;
				lastShortLines=numShortLines;
				whereLastLine=where;
			}
			else
				alreadySkipped=true;
			continue;
		}
		if (headerLines==1 || numStartLines==1)
		{
			// check if the next paragraph also has a header.  Then if it does, reject the whole thing.
			int w2=where;
			whereLastLine=where;
			bool csh=containsSectionHeader(paragraph,firstNonBlank);
			bool ld=!csh && iswdigit(paragraph[firstNonBlank]);
			bool rn=!csh && isRomanNumeral(paragraph,firstNonBlank);
			if (rn && paragraph[0]==L'I' && paragraph[1]==L' ' && iswalpha(paragraph[2]))
				rn=false; 
			indentedLines=numLines=headerLines=numStartLines=numShortLines=firstNonBlank=0;
			bool contents=false;
			paragraph.clear();
			while (getNextParagraph(w2,buffer,eofEncountered,firstNonBlank,paragraph,numLines,indentedLines,headerLines,numStartLines,numShortLines) &&
						 ((csh && containsSectionHeader(paragraph,firstNonBlank)) || (ld && iswdigit(paragraph[firstNonBlank])) || (rn && isRomanNumeral(paragraph,firstNonBlank))))
			{
				if (paragraph.find(L"CHAPTER I.")!=wstring::npos || paragraph.find(saveParagraph)!=wstring::npos) break;
				paragraph.clear();
				lastLine.clear();
				indentedLines=numLines=headerLines=numStartLines=numShortLines=firstNonBlank=0;
				contents=true;
			}
			if (contents) 
			{
				where=w2;
				lastLine=paragraph;
				lastNumLines=numLines;
				lastShortLines=numShortLines;
				continue;
			}
		}
		if (lastNumLines>2 && lastShortLines<=1) 
		{
			lplog(LOG_INFO,L"Rejecting %s.",lastLine.c_str());
			continue;
		}
		if (containsAttribution(lastLine,firstNonBlank)) continue;
		if (lastLine.length()>255) continue;
		if (noCasefind(lastLine,L"preface",0)!=wstring::npos)
		{
			size_t w3;
			wchar_t *startScanningPosition;
			if ((w3=noCasefind(buffer,L"contents",where))!=wstring::npos && 
					aloneOnLine((wchar_t *)buffer.c_str(),(wchar_t *)buffer.c_str()+w3,L"contents",startScanningPosition,buffer.length()) &&
					w3<(int)buffer.length()/10)
			{
				where=w3;
				continue;
			}
		}
		break;
	}
	return true;
}

// You can use the predicate version of std::search
bool testStart(wstring &buffer,wchar_t *str,int &where,bool checkAlone=true)
{ LFS
	int wt;
	wchar_t *startScanningPosition;
	if (((wt=buffer.find(str,where))!=wstring::npos) && wt<20000 &&
			aloneOnLine((wchar_t *)buffer.c_str(),(wchar_t *)buffer.c_str()+wt,str,startScanningPosition,buffer.length(),!checkAlone))
	{
	  where=wt;
		return true;
	}
	return false;
}

bool Source::findStart(wstring &buffer,wstring &start,int &repeatStart,wstring &title)
{ LFS
	// go to the first paragraph without any ignoreWords, with at least two consecutive sentences (periods).
	// then go to the previous title, if it doesn't have any ignoreWords.  If the start occurs before, count repeatStart.
	int where=0,whereLastLine=-1,wt=0;
	bufferLen=buffer.length();
	testStart(buffer,L"*END THE SMALL PRINT",where,false);
	testStart(buffer,L"*END*THE SMALL PRINT",where,false);
	testStart(buffer,L"START OF THE PROJECT GUTENBERG EBOOK",where,false);
	testStart(buffer,L"START OF THIS PROJECT GUTENBERG EBOOK",where,false);
	testStart(buffer,L"CONTENTS",where,false);
	if (!testStart(buffer,L"PROLOGUE",where) &&
		  !testStart(buffer,L"CHAPTER 1",where) &&
			!testStart(buffer,L"CHAPTER I",where) && // to prevent CHAPTER IV to be found after CHAPTER 1
			!testStart(buffer,L"CHAPTER I-",where,false) &&
			!testStart(buffer,L"CHAPTER I.",where,false) &&
			!testStart(buffer,L"CHAPTER I:",where,false) &&
			!testStart(buffer,L"CHAPTER ONE",where))
		testStart(buffer,L"CHAPTER ONE.",where,false);
	wchar_t *startScanningPosition;
	while (((wt=noCasefind(buffer,title,wt+1))!=wstring::npos) && wt<(int)buffer.length()/10 &&
		aloneOnLine((wchar_t *)buffer.c_str(),(wchar_t *)buffer.c_str()+wt,title.c_str(),startScanningPosition,bufferLen))
		where=wt;
	wstring lastLine;
	if (!findNextParagraph(where, buffer, lastLine, whereLastLine))
	{
		start = L"**START NOT FOUND**";
		return false;
	}
	// previous non-blank line is lastLine.
	int wll=0;
	repeatStart=0;
	while (wll>=0 && wll<where)
	{
		wll=buffer.find(lastLine,wll+1);
		if (wll<0 || (wll>where && (wll*100/bufferLen)>10)) break;
		if (aloneOnLine((wchar_t *)buffer.c_str(),(wchar_t *)buffer.c_str()+wll,lastLine.c_str(),startScanningPosition,bufferLen))
			repeatStart++;
	}
	start=lastLine;
	return true;
}

bool readWikiPage(wstring webAddress,wstring &buffer)
{ LFS
	int ret;
	if (ret=Internet::readPage(webAddress.c_str(),buffer)) return false;
	if (buffer.find(L"Sorry, but the page or book you tried to access is unavailable")!=wstring::npos ||
			buffer.find(L"<title>403 Forbidden</title>")!=wstring::npos ||
			buffer.find(L"<h1>404 Not Found</h1>")!=wstring::npos ||
			buffer.empty())
	{
		buffer.clear();
		return false;
	}
	return true;
}

void unescapeStr(wstring &str)
{ LFS
	wstring ess;
	for (unsigned int I=0; I<str.length(); I++)
	{
		if (str[I]=='\\' && (str[I+1]=='\'' || str[I+1]=='\\')) I++;
		ess+=str[I];
	}
	str=ess;
}

bool Source::readSource(wstring &path,bool checkOnly,bool &parsedOnly,bool printProgress,bool readOnlyParsed, wstring specialExtension)
{ LFS
	//unescapeStr(path); // doesn't work on 'Twixt Land & Sea: Tales
	wstring locationCache=path+L".SourceCache"+specialExtension;
	if (checkOnly)
		//return _waccess(locationCache.c_str(),0)==0; // long path limitation
		return GetFileAttributesW(locationCache.c_str()) != INVALID_FILE_ATTRIBUTES;
	//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path.c_str(), __FUNCTIONW__);
	// IOHANDLE fd = _wopen(locationCache.c_str(), O_RDWR | O_BINARY);   // MAX_PATH limitation
	HANDLE fd = CreateFile(locationCache.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	///if (fd<0) return false;
	if (fd== INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR, L"Unable to open source %s - %s", locationCache.c_str(), lastErrorMsg().c_str());
		CloseHandle(fd);
		return false;
	}
	void *buffer;
	//int bufferlen=filelength(fd);
	DWORD bufferlen;
	if ((bufferlen = GetFileSize(fd, NULL))== INVALID_FILE_SIZE)
	{
		lplog(LOG_ERROR, L"Could not get length of file %s.", path.c_str());
		CloseHandle(fd);
		return false;
	}
	buffer=(void *)tmalloc(bufferlen+10);
	//::read(fd,buffer,bufferlen);
	DWORD NumberOfBytesRead;
	if (!ReadFile(fd, buffer, bufferlen, &NumberOfBytesRead, 0) || bufferlen != NumberOfBytesRead)
	{
		lplog(LOG_ERROR, L"Could not get read file %s.", path.c_str());
		CloseHandle(fd);
		return false;
	}
	//close(fd);
	CloseHandle(fd);
	int where=0;
	sourcePath = path;
	bool success = read((char *)buffer, where, bufferlen, parsedOnly,printProgress,readOnlyParsed,specialExtension);
	if (!success)
		lplog(LOG_ERROR, L"Error while reading file %s at position %d.", path.c_str(),m.size());
	tfree(bufferlen,buffer);
	if (!success)
	{
		clearSource();
		if (updateWordUsageCostsDynamically)
			WordClass::resetUsagePatternsAndCosts(debugTrace);
		else
			WordClass::resetCapitalizationAndProperNounUsageStatistics(debugTrace);
	}
	return success;
}

// Updates costs of parent patterns when a child PMA element has been reduced after parent PEMA costs have been calculated.
int Source::updatePEMACosts(int PEMAPosition,int pattern,int begin,int end,int position,
														vector<patternElementMatchArray::tPatternElementMatch *> &ppema)
{ LFS
#ifdef LOG_PATTERN_COST_CHECK
	int scanPP=PEMAPosition;
	for (patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+scanPP;
		scanPP>=0; scanPP=pem->nextByPatternEnd,pem=pema.begin()+scanPP)
		if (pem->isChildPattern())
			lplog(L"%d:RP PEMA SCAN %s[%s](%d,%d) child %s[*](%d,%d)",
			position,patterns[pem->getPattern()]->name.c_str(),patterns[pem->getPattern()]->differentiator.c_str(),position+pem->begin,position+pem->end,
			patterns[pem->getChildPattern()]->name.c_str(),position,position+pem->getChildLen());
		else
			lplog(L"%d:RP PEMA %s[%s](%d,%d) child form %s",
			position,patterns[pem->getPattern()]->name.c_str(),patterns[pem->getPattern()]->differentiator.c_str(),position+pem->begin,position+pem->end,
			Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str());
#endif
	int relativeEnd=end-position,relativeBegin=begin-position,minCost=10000;
	vector <WordMatch>::iterator im=m.begin()+position;
	vector<patternElementMatchArray::tPatternElementMatch *> tpema;
	patternElementMatchArray::tPatternElementMatch *tpem=NULL;
	for (patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+PEMAPosition;
		PEMAPosition>=0 && pem->getPattern()==pattern && pem->end==relativeEnd;
		PEMAPosition=pem->nextByPatternEnd,pem=pema.begin()+PEMAPosition)
		if (pem->begin==relativeBegin)
		{
			int cost;
			int nextPEMAPosition=pema[PEMAPosition].nextPatternElement;
			if (pem->isChildPattern())
			{
				int childEnd=pem->getChildLen();
#ifdef LOG_PATTERN_COST_CHECK
				lplog(L"%d:RP PEMA %06d %s[%s](%d,%d) child %s[*](%d,%d) element #%d resulted in a minCost of %d.",
					position,PEMAPosition,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,
					patterns[pem->getChildPattern()]->name.c_str(),position,position+childEnd,pem->getElement(),pem->iCost);
#endif
				cost=pem->getIncrementalCost();
				if (nextPEMAPosition>=0 && !patterns[pattern]->nextPossibleElementRange(pem->getElement()))
						lplog(LOG_FATAL_ERROR,L"Inconsistency!");
				if (nextPEMAPosition>=0)
					cost+=updatePEMACosts(nextPEMAPosition,pattern,begin,end,position+childEnd,tpema);
			}
			else
			{
				cost=(pem->getElementIndex()==(unsigned char)-2) ? 0 : patterns[pattern]->getCost(pem->getElement(),pem->getElementIndex(),pem->isChildPattern());
				// only accumulate or use usage costs IF word is not capitalized
				if (im->costable())
					cost+=im->word->second.getUsageCost(pem->getChildForm());
#ifdef LOG_PATTERN_COST_CHECK
				lplog(L"%d:RP PEMA %06d %s[%s](%d,%d) child form %s element #%d resulted in a minCost of %d.",
					position,PEMAPosition,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,
					Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str(),pem->getElement(),cost);
#endif
				if (nextPEMAPosition>=0 && !patterns[pattern]->nextPossibleElementRange(pem->getElement()))
						lplog(LOG_FATAL_ERROR,L"Inconsistency!");
				if (nextPEMAPosition>=0)
					cost+=updatePEMACosts(nextPEMAPosition,pattern,begin,end,position+1,tpema);
			}
			if (cost<minCost)
			{
				minCost=cost;
				ppema=tpema;
				tpem=pem;
			}
		}
#ifdef LOG_PATTERN_COST_CHECK
		else
			lplog(L"%d:RP PEMA %s[%s](%d,%d) child %s[*](%d,%d) element #%d NO MATCH",
			position,patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),position+pem->begin,position+pem->end,
			patterns[pem->getChildPattern()]->name.c_str(),position,position+pem->getChildLen(),pem->getElement());
#endif
	if (tpem==NULL)
	{
		if (!m[position].word->second.isIgnore())
		{
			lplog(L"%d:Update PEMA ERROR with pattern %s[%s](%d,%d) [out of %d total positions]!",position,
				patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),begin,end,m.size());
			int scanPP=PEMAPosition;
			for (patternElementMatchArray::tPatternElementMatch *pem=pema.begin()+scanPP;
				scanPP>=0; scanPP=pem->nextByPatternEnd,pem=pema.begin()+scanPP)
				if (pem->isChildPattern())
					lplog(L"%d:RP PEMA SCAN %s[%s](%d,%d) child %s[*](%d,%d)",
					position,patterns[pem->getPattern()]->name.c_str(),patterns[pem->getPattern()]->differentiator.c_str(),position+pem->begin,position+pem->end,
					patterns[pem->getChildPattern()]->name.c_str(),position,position+pem->getChildLen());
				else
					lplog(L"%d:RP PEMA %s[%s](%d,%d) child form %s",
					position,patterns[pem->getPattern()]->name.c_str(),patterns[pem->getPattern()]->differentiator.c_str(),position+pem->begin,position+pem->end,
					Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str());
			lplog();
			return minCost;
		}
#ifdef LOG_PATTERN_COST_CHECK
		lplog(L"%d: IGNORED",position);
#endif
		return updatePEMACosts(PEMAPosition,pattern,begin,end,position+1,ppema);
	}
	ppema.push_back(tpem);
	return minCost;
}

// insertionPoints is a list of offsets into the pma array at the source position.
// this was created because if we are matching with multiple passes, costs for some patterns
// may be reduced (they are not increased because patterns always take the minimal cost path).
//  therefore, the parent (ancestor) patterns of these insertionPoints may be adjusted downwards.
// algorithm:
// set reducedCost.
// for each parent pattern,
//  call updatePEMACosts
// for each parent pattern B,
//  for each parent pattern A after in list,
//    if pattern A, begin and end match ,
//      if current cost A <= current cost B, skip B (continue with next B)
//      if current cost A > current cost B,
//        remove A
//  call reduceParent with B and reducedCost=current cost B
void Source::reduceParent(int position,unsigned int PMAOffset,int reducedCost)
{ LFS // DLFS
	patternMatchArray::tPatternMatch *pm=m[position].pma.content;
	if (PMAOffset>=m[position].pma.count)
		lplog(LOG_FATAL_ERROR,L"    %d:RP PMA pattern offset %d is >= %d - ILLEGAL",position,PMAOffset,m[position].pma.count);
	unsigned int childPattern=pm[PMAOffset].getPattern();;
	int childLen=pm[PMAOffset].len,originalCost=pm[PMAOffset].getCost();
	if (childPattern>=patterns.size())
		lplog(LOG_FATAL_ERROR,L"    %d:RP PMA pattern %d ILLEGAL",position,childPattern);
	if (originalCost>reducedCost)
	{
#ifdef LOG_PATTERN_COST_CHECK
		lplog(L"%d:RP PMA pattern %s[%s](%d,%d) reducing cost from %d to %d (offset=%d).",position,
			patterns[childPattern]->name.c_str(),patterns[childPattern]->differentiator.c_str(),
			position,position+childLen,originalCost,reducedCost,PMAOffset);
#endif
		pm[PMAOffset].cost=reducedCost; // could updateMaxMatch but this could unduly restrict the patterns surviving to stage two of eliminateLoserPatterns
	}
	else
	{
#ifdef LOG_PATTERN_COST_CHECK
		lplog(L"%d:RP PMA pattern %s[%s](%d,%d) cost reduction REJECTED (pm[PMAOffset=%d].getCost() %d<=reducedCost %d).",position,
			patterns[childPattern]->name.c_str(),patterns[childPattern]->differentiator.c_str(),
			position,position+childLen,PMAOffset,originalCost,reducedCost);
#endif
		return;
	}
	vector <patternElementMatchArray::tPatternElementMatch *> savePositions;
	for (int p=patterns[childPattern]->rootPattern; p>=0; p=patterns[p]->nextRoot)
	{
		if (p==childPattern)
			pm=m[position].pma.content+PMAOffset;
		else if (!m[position].patterns.isSet(p) || !(pm=m[position].pma.find(p,childLen)))
			continue;
		// CHILD=rootPattern starting at position (position) and ending at childEnd
		patternElementMatchArray::tPatternElementMatch *pem;
		for (int PEMAPosition=pm->pemaByChildPatternEnd; PEMAPosition!=-1; PEMAPosition=pem->nextByChildPatternEnd)
		{
			pem=pema.begin()+PEMAPosition;
			// this is needed because iCost is the reducedCost of the child + any costs added by the parent pattern ("_INFP*1")
						int incrementalCost=reducedCost+patterns[pem->getPattern()]->getCost(pem->getElement(),pem->getElementIndex(),pem->isChildPattern());
			if (pem->getPattern()!=childPattern && // parent patterns never match themselves
				pem->getIncrementalCost()>incrementalCost)  // the reducedCost of ONE instance of the pattern may not be less than the lowest cost of all children
			{
#ifdef LOG_PATTERN_COST_CHECK
				char temp[1024];
				lplog(L"    %d:RP PMA pattern %s[%s](%d,%d) reducing cost (%d->%d) up into parent %s",position,
					patterns[childPattern]->name.c_str(),patterns[childPattern]->differentiator.c_str(),
					position,position+childLen,pem->iCost,incrementalCost,pem->toText(position+pem->begin,temp,m));
#endif
				pem->setIncrementalCost(incrementalCost);
				int finalCost=0;
				vector<patternElementMatchArray::tPatternElementMatch *> ppema;
				if (patterns[pem->getPattern()]->nextPossibleElementRange(-1))
					finalCost=updatePEMACosts(pem->origin,pem->getPattern(),position+pem->begin,position+pem->end,position+pem->begin,ppema);
				vector <patternElementMatchArray::tPatternElementMatch *>::iterator ipem=ppema.begin(),ipemEnd=ppema.end();
				for (; ipem!=ipemEnd; ipem++)
				{
#ifdef LOG_PATTERN_COST_CHECK
					if ((*ipem)->isChildPattern())
						lplog(L"    %d:pattern %s[%s](%d,%d)*%d child %s[%s](%d,%d) reduced to cost %d.",position,
						patterns[(*ipem)->getPattern()]->name.c_str(),patterns[(*ipem)->getPattern()]->differentiator.c_str(),position+(*ipem)->begin,position+(*ipem)->end,(*ipem)->getOCost(),
						patterns[(*ipem)->getChildPattern()]->name.c_str(),patterns[(*ipem)->getChildPattern()]->differentiator.c_str(),position,position+(*ipem)->getChildLen(),
						finalCost);
					else
						lplog(L"    %d:pattern %s[%s](%d,%d)*%d child %d form reduced to cost %d.",position,
						patterns[(*ipem)->getPattern()]->name.c_str(),patterns[(*ipem)->getPattern()]->differentiator.c_str(),position+(*ipem)->begin,position+(*ipem)->end,(*ipem)->getOCost(),
						(*ipem)->getChildForm(),finalCost);
#endif
					(*ipem)->setOCost(finalCost);
				}
				savePositions.push_back(pem);
			}
		}
	}
	// look up parent of each pem.  get lowest cost of the group of children.
	vector <patternElementMatchArray::tPatternElementMatch *>::iterator ipem=savePositions.begin(),ipemEnd=savePositions.end();
	for (; ipem!=ipemEnd; ipem++)
	{
		int newPosition=position+(*ipem)->begin;
		if (!m[newPosition].patterns.isSet((*ipem)->getPattern())) continue;
		patternMatchArray::tPatternMatch *pmParent=m[newPosition].pma.find((*ipem)->getPattern(),(*ipem)->end-(*ipem)->begin);
		if (pmParent)
		{
			int lowestCost=MAX_COST;
			for (int PEMAOffset=pmParent->pemaByPatternEnd; PEMAOffset>=0; PEMAOffset=pema[PEMAOffset].nextByPatternEnd)
				lowestCost=min(lowestCost,pema[PEMAOffset].getOCost());
			if (pmParent->getCost()>lowestCost)
			{
				//      pmParent->setCost(lowestCost);  NOT needed!  screws up recursion because this is done as the first step of the recursive call on the following line!
				reduceParent(newPosition,(int)(pmParent-m[newPosition].pma.content),lowestCost);
			}
		}
	}
}

void Source::reduceParents(int position,vector <unsigned int> &insertionPoints,vector <int> &reducedCosts)
{ LFS
	for (unsigned int I=0; I<insertionPoints.size(); I++)
		reduceParent(position,insertionPoints[I],reducedCosts[I]);
}

void Source::logPatternChain(int sourcePosition,int insertionPoint,enum patternElementMatchArray::chainType patternChainType)
{ LFS
	wchar_t *chPT=L"";
	int chain=-1,originPattern,originEnd;
	switch (patternChainType)
	{
	case patternElementMatchArray::BY_PATTERN_END:
		chPT=L"PE";
		chain=m[sourcePosition].pma[insertionPoint].pemaByPatternEnd;
		originPattern=m[sourcePosition].pma[insertionPoint].getPattern();
		originEnd=m[sourcePosition].pma[insertionPoint].len;
		::lplog(L"%d:%s CHAIN (%03d,%02d) PMA %d %s[%s]*%d(%d,%d)",sourcePosition,chPT,
			originPattern,originEnd,insertionPoint,
			patterns[m[sourcePosition].pma[insertionPoint].getPattern()]->name.c_str(),
			patterns[m[sourcePosition].pma[insertionPoint].getPattern()]->differentiator.c_str(),
			m[sourcePosition].pma[insertionPoint].getCost(),
			sourcePosition,
			sourcePosition+m[sourcePosition].pma[insertionPoint].len);
		break;
	case patternElementMatchArray::BY_POSITION:
		chPT=L"PO";
		chain=insertionPoint;
		originPattern=pema[insertionPoint].getPattern();
		originEnd=pema[insertionPoint].end;
		break;
	case patternElementMatchArray::BY_CHILD_PATTERN_END:
		chPT=L"PA";
		chain=insertionPoint;
		originPattern=pema[insertionPoint].getPattern();
		originEnd=pema[insertionPoint].end;
		break;
	}
	while (chain>=0)
	{
		if (pema[chain].isChildPattern())
			::lplog(L"%d:%s CHAIN (%03d,%02d) %d:%s[%s]*%d(%d,%d) child %s[%s](%d,%d)",sourcePosition,chPT,
			originPattern,originEnd,chain,
			patterns[pema[chain].getPattern()]->name.c_str(),
			patterns[pema[chain].getPattern()]->differentiator.c_str(),
			pema[chain].getOCost(),
			sourcePosition+pema[chain].begin,
			sourcePosition+pema[chain].end,
			patterns[pema[chain].getChildPattern()]->name.c_str(),
			patterns[pema[chain].getChildPattern()]->differentiator.c_str(),
			sourcePosition,
			sourcePosition+pema[chain].getChildLen());
		else
			::lplog(L"%d:%s CHAIN (%03d,%02d) %d:%s[%s]*%d(%d,%d) child form %s",sourcePosition,chPT,
			originPattern,originEnd,chain,
			patterns[pema[chain].getPattern()]->name.c_str(),
			patterns[pema[chain].getPattern()]->differentiator.c_str(),
			pema[chain].getOCost(),
			sourcePosition+pema[chain].begin,
			sourcePosition+pema[chain].end,
			Forms[m[sourcePosition].getFormNum(pema[chain].getChildForm())]->shortName.c_str());
		switch (patternChainType)
		{
		case patternElementMatchArray::BY_PATTERN_END:chain=pema[chain].nextByPatternEnd; break;
		case patternElementMatchArray::BY_POSITION:chain=pema[chain].nextByPosition; break;
		case patternElementMatchArray::BY_CHILD_PATTERN_END:chain=pema[chain].nextByChildPatternEnd; break;
		}
	}
}

Source::Source(wchar_t *databaseServer,int _sourceType,bool generateFormStatistics,bool skipWordInitialization,bool printProgress)
{ LFS
	sourceType=_sourceType;
	primaryQuoteType=L"\"";
	secondaryQuoteType=L"\'";
	bookBuffer=NULL;
	lastPEMAConsolidationIndex=1; // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet=-1;
	gTraceSource=0;
	alreadyConnected=false;
	numTotalNarratorFullVerbTenses=numTotalNarratorVerbTenses=0;
	numTotalSpeakerFullVerbTenses=numTotalSpeakerVerbTenses=0;
	debugTrace.printBeforeElimination=false;
	debugTrace.traceSubjectVerbAgreement = false;
	debugTrace.traceTestSubjectVerbAgreement = false;
	debugTrace.traceEVALObjects=false;
	debugTrace.traceAnaphors=false;
	debugTrace.traceRelations=false;
	debugTrace.traceNyms=false;
	debugTrace.traceRole=false;
	debugTrace.traceWhere=false;
	debugTrace.traceTime=false;
	debugTrace.traceWikipedia=false;
	debugTrace.traceWebSearch=false;
	debugTrace.traceQCheck=false;
	debugTrace.traceTransitoryQuestion=false;
	debugTrace.traceConstantQuestion=false;
	debugTrace.traceMapQuestion=false;
	debugTrace.traceCommonQuestion=false;
	debugTrace.traceQuestionPatternMap=false;
	debugTrace.traceVerbObjects=false;
	debugTrace.traceDeterminer=false;
	debugTrace.traceBNCPreferences=false;
	debugTrace.tracePatternElimination=false;
	debugTrace.traceSecondaryPEMACosting=false;
	debugTrace.traceSpeakerResolution=false;
	debugTrace.traceObjectResolution=false;
	debugTrace.traceMatchedSentences=false;
	debugTrace.traceUnmatchedSentences=false;
	debugTrace.traceIncludesPEMAIndex=false;
	debugTrace.traceTagSetCollection=false;
	debugTrace.collectPerSentenceStats=false;
	debugTrace.traceNameResolution=false;
	debugTrace.traceParseInfo = false;
	debugTrace.tracePreposition = false;
	pass=-1;
	repeatReplaceObjectInSectionPosition=-1;
	accumulateLocationLastLocation=-1;
	tagSetTimeArray=NULL;
	tagSetTimeArraySize=0;
	if (initializeDatabaseHandle(mysql,databaseServer,alreadyConnected)<0)
	{
		createDatabase(databaseServer);
		Words.createWordCategories();
	}
	else 
	{
		if (Words.readWordsFromDB(mysql,generateFormStatistics, printProgress, skipWordInitialization))
				lplog(LOG_FATAL_ERROR,L"Cannot read database.");
	}
	unlockTables();
	#ifdef CHECK_WORD_CACHE
		Words.testWordCacheFileRoutines();
		exit(0);
	#endif
	if (!skipWordInitialization)
	{
		Words.initialize();
		Words.readWordsOfMultiWordObjects(multiWordStrings,multiWordObjects);
		Words.addMultiWordObjects(multiWordStrings,multiWordObjects);
	}
	parentSource=NULL;
	lastSense=-1;
	firstQuote=lastQuote=-1;
	speakersMatched=speakersNotMatched=counterSpeakersMatched=counterSpeakersNotMatched=0;
	unordered_map <int, vector < vector <tTagLocation> > > emptyMap;
	for (unsigned int ts=0; ts<desiredTagSets.size(); ts++)
		pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
	primaryLocationLastMovingPosition=-1;
	primaryLocationLastPosition=-1;
	updateWordUsageCostsDynamically = false;
}

Source::Source(MYSQL *parentMysql,int _sourceType,int _sourceConfidence)
{ LFS
	sourceType=_sourceType;
	sourceConfidence=_sourceConfidence;
	primaryQuoteType=L"\"";
	secondaryQuoteType=L"\'";
	bookBuffer=NULL;
	lastPEMAConsolidationIndex=1; // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet=-1;
	gTraceSource=0;
	numTotalNarratorFullVerbTenses=numTotalNarratorVerbTenses=0;
	numTotalSpeakerFullVerbTenses=numTotalSpeakerVerbTenses=0;
	debugTrace.printBeforeElimination=false;
	debugTrace.traceSubjectVerbAgreement = false;
	debugTrace.traceTestSubjectVerbAgreement = false;
	debugTrace.traceEVALObjects=false;
	debugTrace.traceAnaphors=false;
	debugTrace.traceRelations=false;
	debugTrace.traceNyms=false;
	debugTrace.traceRole=false;
	debugTrace.traceWhere=false;
	debugTrace.traceTime=false;
	debugTrace.traceWikipedia=false;
	debugTrace.traceWebSearch=false;
	debugTrace.traceQCheck=false;
	debugTrace.traceTransitoryQuestion=false;
	debugTrace.traceConstantQuestion=false;
	debugTrace.traceMapQuestion=false;
	debugTrace.traceCommonQuestion=false;
	debugTrace.traceQuestionPatternMap=false;
	debugTrace.traceVerbObjects=false;
	debugTrace.traceDeterminer=false;
	debugTrace.traceBNCPreferences=false;
	debugTrace.tracePatternElimination=false;
	debugTrace.traceSecondaryPEMACosting=false;
	debugTrace.traceSpeakerResolution=false;
	debugTrace.traceObjectResolution=false;
	debugTrace.traceMatchedSentences=false;
	debugTrace.traceUnmatchedSentences=false;
	debugTrace.traceIncludesPEMAIndex=false;
	debugTrace.traceTagSetCollection=false;
	debugTrace.collectPerSentenceStats=false;
	debugTrace.traceNameResolution=false;
	debugTrace.traceParseInfo = false;
	debugTrace.tracePreposition = false;
	alreadyConnected=true;
	pass=-1;
	repeatReplaceObjectInSectionPosition=-1;
	accumulateLocationLastLocation=-1;
	tagSetTimeArray=NULL;
	tagSetTimeArraySize=0;
	mysql=*parentMysql;
	lastSense=-1;
	firstQuote=lastQuote=-1;
	speakersMatched=speakersNotMatched=counterSpeakersMatched=counterSpeakersNotMatched=0;
	primaryLocationLastMovingPosition=-1;
	primaryLocationLastPosition=-1;
	unordered_map <int, vector < vector <tTagLocation> > > emptyMap;
	for (unsigned int ts=0; ts<desiredTagSets.size(); ts++)
		pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
	updateWordUsageCostsDynamically = false;
}

void Source::writeWords(wstring oPath, wstring specialExtension)
{
	LFS
		wchar_t path[1024];
	wsprintf(path, L"%s.wordCacheFile", oPath.c_str());
	int fd = _wopen(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0)
	{
		lplog(L"Cannot open wordCacheFile - %S.", _sys_errlist[errno]);
		return;
	}
	Words.writeFormsCache(fd);
	unordered_set<wstring> sourceWords;
	char buffer[MAX_BUF];
	int where = 0,numWordInSource=0;
	for (WordMatch im:m)
	{
		tIWMM iSave = im.word;
		if (iSave->second.isNonCachedWord() && !iSave->second.isUnknown())
		{
			//if (iSave->first == L"fewer" || iSave->first == L"afore" || iSave->first == L"thyself")
			//{
			//	wstring forms;
			//	for (unsigned int f = 0; f < iSave->second.formsSize(); f++)
			//		forms += iSave->second.Form(f)->name + L" ";
			//	lplog(LOG_INFO, L"%s:TEMP WRITE %s@%d is a not a cached word [%s].", oPath.c_str(), iSave->first.c_str(), numWordInSource, forms.c_str());
			//}
			continue;
		}
		if (sourceWords.find(iSave->first) != sourceWords.end())
			continue;
		sourceWords.insert(iSave->first);
		Words.writeWord(iSave, buffer, where, MAX_BUF);
		// mainEntries can exist in chains
		int mainLoop = 0;
		while (iSave->second.mainEntry != wNULL && mainLoop++ < 4)
		{
			iSave = iSave->second.mainEntry;
			if (sourceWords.find(iSave->first) != sourceWords.end())
				break;
			sourceWords.insert(iSave->first);
			Words.writeWord(iSave, buffer, where, MAX_BUF);
		}
		if (where > MAX_BUF - 1024)
		{
			::write(fd, buffer, where);
			where = 0;
		}
		numWordInSource++;
	}
	if (where)
		::write(fd, buffer, where);
	close(fd);
}

