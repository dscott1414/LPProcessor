#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include "math.h"
#include "sys/stat.h"
#include <sys/types.h>
#include "profile.h"
#include "paice.h"
#include "internet.h"
#include "wn.h"
#include "QuestionAnswering.h"

bool unlockTables(MYSQL& mysql);

tInflectionMap shortNounInflectionMap[] =
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

tInflectionMap shortVerbInflectionMap[] =
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

tInflectionMap shortAdjectiveInflectionMap[] =
{
	{ ADJECTIVE_NORMATIVE,L"ADJ"},
	{ ADJECTIVE_COMPARATIVE,L"ADJ_COMP"},
	{ ADJECTIVE_SUPERLATIVE,L"ADJ_SUP"},
	{ -1,NULL}
};

tInflectionMap shortAdverbInflectionMap[] =
{
	{ ADVERB_NORMATIVE,L"ADV"},
	{ ADVERB_COMPARATIVE,L"ADV_COMP"},
	{ ADVERB_SUPERLATIVE,L"ADV_SUP"},
	{ -1,NULL}
};

tInflectionMap shortQuoteInflectionMap[] =
{
	{ OPEN_INFLECTION,L"OQ"},
	{ CLOSE_INFLECTION,L"CQ"},
	{ -1,NULL}
};

tInflectionMap shortBracketInflectionMap[] =
{
	{ OPEN_INFLECTION,L"OB"},
	{ CLOSE_INFLECTION,L"CB"},
	{ -1,NULL}
};

const wchar_t* OCSubTypeStrings[] = {
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
int cWordMatch::getInflectionLength(int inflection, tInflectionMap* map)
{
	LFS
		int len = 0;
	for (int I = 0; map[I].num >= 0; I++)
		if (map[I].num & inflection)
			len += 1 + wcslen(map[I].name);
	return len;
}

__int64 roles[] = { SUBOBJECT_ROLE,SUBJECT_ROLE,OBJECT_ROLE,META_NAME_EQUIVALENCE,MPLURAL_ROLE,HAIL_ROLE,
						IOBJECT_ROLE,PREP_OBJECT_ROLE,RE_OBJECT_ROLE,IS_OBJECT_ROLE,NOT_OBJECT_ROLE,NONPAST_OBJECT_ROLE,ID_SENTENCE_TYPE,NO_ALT_RES_SPEAKER_ROLE,
						IS_ADJ_OBJECT_ROLE,NONPRESENT_OBJECT_ROLE,PLACE_OBJECT_ROLE,MOVEMENT_PREP_OBJECT_ROLE,NON_MOVEMENT_PREP_OBJECT_ROLE,
						SUBJECT_PLEONASTIC_ROLE,IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE,UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE,SENTENCE_IN_REL_ROLE,
						PASSIVE_SUBJECT_ROLE, POV_OBJECT_ROLE, MNOUN_ROLE, PRIMARY_SPEAKER_ROLE,SECONDARY_SPEAKER_ROLE,FOCUS_EVALUATED,
						ID_SENTENCE_TYPE,DELAYED_RECEIVER_ROLE,IN_PRIMARY_QUOTE_ROLE,IN_SECONDARY_QUOTE_ROLE,IN_EMBEDDED_STORY_OBJECT_ROLE,EXTENDED_OBJECT_ROLE,
						NOT_ENCLOSING_ROLE,EXTENDED_ENCLOSING_ROLE,NONPAST_ENCLOSING_ROLE,NONPRESENT_ENCLOSING_ROLE,POSSIBLE_ENCLOSING_ROLE,THINK_ENCLOSING_ROLE };
const wchar_t* r_c[] = { L"SUBOBJ",L"SUBJ",L"OBJ",L"META_EQUIV",L"MP",L"H",
						L"IOBJECT",L"PREP",L"RE",L"IS",L"NOT",L"NONPAST",L"ID",L"NO_ALT_RES_SPEAKER",
						L"IS_ADJ",L"NONPRESENT",L"PL",L"MOVE",L"NON_MOVE",
						L"PLEO",L"INQ_SELF_REF_SPEAKER",L"UNRES_FROM_IMPLICIT",L"S_IN_REL",
						L"PASS_SUBJ",L"POV",L"MNOUN",L"SP",L"SECONDARY_SP",L"EVAL",
						L"ID",L"DELAY",L"PRIM",L"SECOND",L"EMBED",L"EXT",
						L"NOT_ENC",L"EXT_ENC",L"NPAST_ENC",L"NPRES_ENC",L"POSS_ENC",L"THINK_ENC" };
wstring cWordMatch::roleString(wstring& sRole)
{
	LFS
		sRole.clear();
	for (unsigned int I = 0; I < sizeof(roles) / sizeof(roles[0]); I++)
		if (objectRole & roles[I])
		{
			sRole += L"[";
			sRole += r_c[I];
			sRole += L"]";
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
unsigned int cWordMatch::getShortAllFormAndInflectionLen(void)
{
	LFS
		unsigned int fSize = formsSize();
	int allLen = 0, formLen;
	for (unsigned int line = 0; line < fSize; line++)
	{
		bool properNoun;
		int inflectionFlags = word->second.inflectionFlags;
		if ((properNoun = (flags & flagAddProperNoun) && line == word->second.formsSize()) ||
			word->second.Form(line)->inflectionsClass == L"noun")
		{
			if (properNoun)
				formLen = Forms[PROPER_NOUN_FORM_NUM]->shortName.length();
			else
				formLen = word->second.Form(line)->shortName.length();
			if (flags & flagNounOwner)
			{
				if (inflectionFlags & SINGULAR) inflectionFlags = SINGULAR_OWNER | (inflectionFlags & ~SINGULAR);
				else if (inflectionFlags & PLURAL) inflectionFlags = PLURAL_OWNER | (inflectionFlags & ~PLURAL);
				else inflectionFlags |= SINGULAR_OWNER;
			}
			formLen += getInflectionLength(inflectionFlags & NOUN_INFLECTIONS_MASK, shortNounInflectionMap) + 2;
			if (word->second.getUsageCost(line) > 9) formLen++;
		}
		else
		{
			wstring inflectionsClass = word->second.Form(line)->inflectionsClass;
			formLen = word->second.Form(line)->shortName.length();
			if (inflectionsClass == L"verb" && (inflectionFlags & VERB_INFLECTIONS_MASK))
				formLen += getInflectionLength(inflectionFlags & VERB_INFLECTIONS_MASK, shortVerbInflectionMap);
			else if (inflectionsClass == L"adverb" && (inflectionFlags & ADVERB_INFLECTIONS_MASK))
				formLen += getInflectionLength(inflectionFlags & ADVERB_INFLECTIONS_MASK, shortAdverbInflectionMap);
			else if (inflectionsClass == L"adjective" && (inflectionFlags & ADJECTIVE_INFLECTIONS_MASK))
				formLen += getInflectionLength(inflectionFlags & ADJECTIVE_INFLECTIONS_MASK, shortAdjectiveInflectionMap);
			else if (inflectionsClass == L"quotes" && (inflectionFlags & INFLECTIONS_MASK))
				formLen += getInflectionLength(inflectionFlags & INFLECTIONS_MASK, shortQuoteInflectionMap);
			else if (inflectionsClass == L"brackets" && (inflectionFlags & INFLECTIONS_MASK))
				formLen += getInflectionLength(inflectionFlags & INFLECTIONS_MASK, shortBracketInflectionMap);
			formLen += 2;
			if (word->second.getUsageCost(line) > 9) formLen++;
		}
		allLen = max(allLen, formLen);
	}
	return allLen;
}

unsigned int cWordMatch::getShortFormInflectionEntry(int line, wchar_t* entry)
{
	LFS
		int inflectionFlags = word->second.inflectionFlags;
	wstring temp;
	if (flags & flagOnlyConsiderProperNounForms)
	{
		if (flags & flagAddProperNoun)
		{
			if (line != word->second.formsSize() && !word->second.Form(line)->properNounSubClass)
				return 0;
		}
		else
		{
			if (word->second.Form(line)->inflectionsClass != L"noun" && !word->second.Form(line)->properNounSubClass)
				return 0;
		}
	}
	if (((flags & flagAddProperNoun) && line == word->second.formsSize()) ||
		word->second.Form(line)->inflectionsClass == L"noun")
	{
		if ((flags & flagAddProperNoun) && line == word->second.formsSize())
			wcscpy(entry, Forms[PROPER_NOUN_FORM_NUM]->shortName.c_str());
		else
			wcscpy(entry, word->second.Form(line)->shortName.c_str());
		if (flags & flagNounOwner)
		{
			if (inflectionFlags & SINGULAR) inflectionFlags = SINGULAR_OWNER | (inflectionFlags & ~SINGULAR);
			else if (inflectionFlags & PLURAL) inflectionFlags = PLURAL_OWNER | (inflectionFlags & ~PLURAL);
			else inflectionFlags |= SINGULAR_OWNER;
		}
		wcscat(entry, getInflectionName(inflectionFlags & NOUN_INFLECTIONS_MASK, shortNounInflectionMap, temp));
		// only accumulate or use usage costs IF word is not capitalized
		if (costable())
			wsprintf(entry + wcslen(entry), L"*%d", word->second.getUsageCost(line));
		return 0;
	}
	wstring inflectionsClass = word->second.Form(line)->inflectionsClass;
	wcscpy(entry, word->second.Form(line)->shortName.c_str());
	if (inflectionsClass == L"verb" && (inflectionFlags & VERB_INFLECTIONS_MASK))
		wcscat(entry, getInflectionName(inflectionFlags & VERB_INFLECTIONS_MASK, shortVerbInflectionMap, temp));
	else if (inflectionsClass == L"adverb" && (inflectionFlags & ADVERB_INFLECTIONS_MASK))
		wcscat(entry, getInflectionName(inflectionFlags & ADVERB_INFLECTIONS_MASK, shortAdverbInflectionMap, temp));
	else if (inflectionsClass == L"adjective" && (inflectionFlags & ADJECTIVE_INFLECTIONS_MASK))
		wcscat(entry, getInflectionName(inflectionFlags & ADJECTIVE_INFLECTIONS_MASK, shortAdjectiveInflectionMap, temp));
	else if (inflectionsClass == L"quotes" && (inflectionFlags & INFLECTIONS_MASK))
		wcscat(entry, getInflectionName(inflectionFlags & INFLECTIONS_MASK, shortQuoteInflectionMap, temp));
	else if (inflectionsClass == L"brackets" && (inflectionFlags & INFLECTIONS_MASK))
		wcscat(entry, getInflectionName(inflectionFlags & INFLECTIONS_MASK, shortBracketInflectionMap, temp));
	if (costable())
		wsprintf(entry + wcslen(entry), L"*%d", word->second.getUsageCost(line));
	return 0;
}

unsigned int cWordMatch::formsSize(void)
{
	LFS
		if (flags & flagAddProperNoun) return word->second.formsSize() + 1;
	return word->second.formsSize();
}

unsigned int cWordMatch::getFormNum(unsigned int formOffset)
{
	LFS
		if (formOffset >= word->second.formsSize()) return PROPER_NOUN_FORM_NUM; // (flags&flagAddProperNoun) later?
	return word->second.forms()[formOffset];
}

// gets all forms, including proper noun if set and properly checked
vector <int> cWordMatch::getForms()
{
	vector <int> checkedForms;
	int form;
	for (unsigned int tagFormOffset = 0; tagFormOffset < formsSize(); tagFormOffset++)
		if (queryForm(form = getFormNum(tagFormOffset)) >= 0)
			checkedForms.push_back(form);
	return checkedForms;
}

// in the case of a proper noun, it increases the size of the form by 1
int cWordMatch::queryForm(int form)
{
	LFS
		if ((flags & flagAddProperNoun) && form == PROPER_NOUN_FORM_NUM)
			return word->second.formsSize();
	if (flags & flagOnlyConsiderProperNounForms)
	{
		if (form != PROPER_NOUN_FORM_NUM && !Forms[form]->properNounSubClass) return -1;
	}
	else if (form == PROPER_NOUN_FORM_NUM && (!(flags & flagFirstLetterCapitalized) || (flags & flagRefuseProperNoun)))
		return -1;
	return word->second.query(form);
}

int cWordMatch::queryForm(wstring sForm)
{
	LFS
		int form;
	if ((form = cForms::findForm(sForm)) < 0) return -1;
	return queryForm(form);
}

// in the case of a proper noun, it increases the size of the form by 1
int cWordMatch::queryWinnerForm(int form)
{
	DLFS
		int picked;
	if (flags & flagAddProperNoun && form == PROPER_NOUN_FORM_NUM)
		picked = word->second.formsSize();
	else if (flags & flagOnlyConsiderProperNounForms)
	{
		if (form != PROPER_NOUN_FORM_NUM && !Forms[form]->properNounSubClass) return -1;
		picked = word->second.query(form);
	}
	else picked = word->second.query(form);
	if (picked >= 0 && isWinner(picked)) return picked;
	return -1;
}

wstring cWordMatch::winnerFormString(wstring& formsString, bool withCost)
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
	if ((flags & flagAddProperNoun) && isWinner(word->second.formsSize()))
	{
		if (formsString.size() > 0)
			formsString += L" ";
		formsString += wstring(Forms[PROPER_NOUN_FORM_NUM]->name);
	}
	return formsString;
}

void cWordMatch::getWinnerForms(vector <int>& winnerForms)
{
	LFS
		for (int f = 0; f < (signed)word->second.formsSize(); f++)
			if (isWinner(f))
				winnerForms.push_back(word->second.getFormNum(f));
	if ((flags & flagAddProperNoun) && isWinner(word->second.formsSize()))
		winnerForms.push_back(PROPER_NOUN_FORM_NUM);
}

int cWordMatch::getNumWinners()
{
	LFS
		int numWinners = 0;
	for (int f = 0; f < (signed)word->second.formsSize(); f++)
		if (isWinner(f))
			numWinners++;
	if ((flags & flagAddProperNoun) && isWinner(word->second.formsSize()))
		numWinners++;
	return numWinners;
}

wstring cWordMatch::patternWinnerFormString(wstring& winnerForms)
{
	LFS
		winnerForms.clear();
	for (int f = 0; f < (signed)word->second.formsSize(); f++)
		if (isWinner(f))
		{
			if (winnerForms.length() > 0)
				winnerForms += L",";
			winnerForms += word->second.Form(f)->name;
			winnerForms += L"|" + word->first;
			wchar_t temp[10];
			_itow(word->second.getUsageCost(f), temp, 10);
			winnerForms += L"*" + wstring(temp);
		}
	return winnerForms;
}

bool cWordMatch::hasWinnerVerbForm(void)
{
	LFS
		return word->second.hasWinnerVerbForm(tmpWinnerForms);
}

bool cWordMatch::hasWinnerNounForm(void)
{
	LFS
		return word->second.hasWinnerNounForm(tmpWinnerForms);
}

int cWordMatch::queryWinnerForm(wstring sForm)
{
	DLFS
		int form;
	if ((form = cForms::findForm(sForm)) <= 0) return -1;
	return queryWinnerForm(form);
}

bool cWordMatch::isPossessivelyGendered(bool& possessivePronoun)
{
	LFS
		if (possessivePronoun = (queryWinnerForm(possessiveDeterminerForm) >= 0)) return true;
	return ((queryWinnerForm(nounForm) >= 0 || queryWinnerForm(PROPER_NOUN_FORM_NUM) >= 0) && (flags & flagNounOwner));
}

bool cWordMatch::isGendered(void)
{
	LFS
		if (queryWinnerForm(nounForm) >= 0 || queryForm(honorificForm) >= 0) return true;
	if (queryWinnerForm(honorificForm) < 0 && queryWinnerForm(honorificAbbreviationForm) < 0 &&
		queryWinnerForm(nomForm) < 0 &&
		queryWinnerForm(personalPronounForm) < 0 &&
		queryWinnerForm(personalPronounAccusativeForm) < 0 &&
		queryWinnerForm(reflexivePronounForm) < 0 &&
		queryWinnerForm(possessivePronounForm) < 0 &&
		queryWinnerForm(PROPER_NOUN_FORM_NUM) < 0 &&
		queryWinnerForm(pronounForm) < 0 &&
		queryWinnerForm(indefinitePronounForm) < 0)
		return false;
	// if this is not a proper noun, then this is a gendered noun.
	if (word->second.query(PROPER_NOUN_FORM_NUM) < 0) return true;
	// if this is a proper noun, this is only gendered if it is capitalized.
	return ((flags & flagFirstLetterCapitalized) && !(flags & flagRefuseProperNoun));
}

char* wTM(wstring inString, string& outString);

// when changing this definition, the WNcache must be deleted
bool cWordMatch::isPhysicalObject(void)
{
	LFS
		tIWMM w = word;
	bool found = false;
	if (w->second.query(nounForm) < 0) return false;
	if (w->second.mainEntry != wNULL) w = w->second.mainEntry;
	if (!(w->second.flags & (cSourceWordInfo::physicalObjectByWN | cSourceWordInfo::notPhysicalObjectByWN | cSourceWordInfo::uncertainPhysicalObjectByWN)))
	{
		if (hasHyperNym(w->first, L"physical_object", found, false) || hasHyperNym(w->first, L"physical_entity", found, false))
			w->second.flags |= cSourceWordInfo::physicalObjectByWN;
		else if (hasHyperNym(word->first, L"psychological_feature", found, false) && !hasHyperNym(word->first, L"cognitive_state", found, false))
			w->second.flags |= cSourceWordInfo::notPhysicalObjectByWN;
		else
			w->second.flags |= cSourceWordInfo::uncertainPhysicalObjectByWN;
	}
	return (w->second.flags & cSourceWordInfo::physicalObjectByWN) != 0;
}

// purposefully ignores whether the noun has a flagNounOwner because of its use.
// also judges noun usage - if noun form is rarely used, it is ignored.
bool cWordMatch::isNounType(void)
{
	LFS
		if (word->second.query(verbverbForm) >= 0 && (word->second.inflectionFlags & VERB_PRESENT_PARTICIPLE))
			return false;
	int form;
	return
		(flags & flagOnlyConsiderProperNounForms) ||
		(flags & flagAddProperNoun) ||
		((form = word->second.query(PROPER_NOUN_FORM_NUM)) >= 0 && (flags & flagFirstLetterCapitalized)) ||
		((form = word->second.query(nounForm)) >= 0 && word->second.getUsageCost(form) < 3) ||
		((form = word->second.query(indefinitePronounForm)) >= 0 && word->second.getUsageCost(form) < 3) ||
		((form = word->second.query(relativizerForm)) >= 0 && word->second.getUsageCost(form) < 3);
}

bool cWordMatch::isTopLevel(void)
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
bool cWordMatch::isModifierType(void)
{
	LFS
		int form;
	if (!(flags & flagOnlyConsiderProperNounForms))
	{
		if (word->second.query(verbverbForm) >= 0 && (word->second.inflectionFlags & VERB_PRESENT_PARTICIPLE))
			return false;
		int modifierForms[] = { adverbForm,adjectiveForm,numeralOrdinalForm,NUMBER_FORM_NUM,coordinatorForm,NULL };
		for (int I = 0; modifierForms[I]; I++)
			if ((form = word->second.query(modifierForms[I])) >= 0 && word->second.getUsageCost(form) < 3) return true;
	}
	return (((form = word->second.query(nounForm)) >= 0 || (form = word->second.query(PROPER_NOUN_FORM_NUM)) >= 0) &&
		word->second.getUsageCost(form) < 3 && (flags & flagNounOwner));
}

bool cWordMatch::read(char* buffer, int& where, int limit, int sourceType)
{
	DLFS
		wstring temp;
	if (!copy(temp, buffer, where, limit)) return false;
	if ((word = Words.query(temp)) == Words.end())
	{
		wstring sWord, comment;
		int sourceId = -1, nounOwner = 0;
		__int64 bufferLength = temp.length(), bufferScanLocation = 0;
		bool added = false;
		int result = Words.readWord((wchar_t*)temp.c_str(), bufferLength, bufferScanLocation, sWord, comment, nounOwner, false, false, t, NULL, -1, sourceType);
		word = Words.end();
		if (result == PARSE_NUM)
			word = Words.addNewOrModify(NULL, sWord, 0, NUMBER_FORM_NUM, 0, 0, L"", sourceId, added);
		else if (result == PARSE_PLURAL_NUM)
			word = Words.addNewOrModify(NULL, sWord, 0, NUMBER_FORM_NUM, PLURAL, 0, L"", sourceId, added);
		else if (result == PARSE_ORD_NUM)
			word = Words.addNewOrModify(NULL, sWord, 0, numeralOrdinalForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_ADVERB_NUM)
			word = Words.addNewOrModify(NULL, sWord, 0, adverbForm, 0, 0, sWord, sourceId, added);
		else if (result == PARSE_DATE)
			word = Words.addNewOrModify(NULL, sWord, 0, dateForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_TIME)
			word = Words.addNewOrModify(NULL, sWord, 0, timeForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_TELEPHONE_NUMBER)
			word = Words.addNewOrModify(NULL, sWord, 0, telephoneNumberForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_MONEY_NUM)
			word = Words.addNewOrModify(NULL, sWord, 0, moneyForm, 0, 0, L"", sourceId, added);
		else if (result == PARSE_WEB_ADDRESS)
			word = Words.addNewOrModify(NULL, sWord, 0, webAddressForm, 0, 0, L"", sourceId, added);
		else if (result < 0 && result != PARSE_END_SENTENCE)
		{
			lplog(LOG_ERROR, L"Word %s cannot be added - error %d.", temp.c_str(), result);
			return false;
		}
		else
			if ((result = Words.parseWord(NULL, sWord, word, false, nounOwner, sourceId, false)) < 0)
			{
				string temp2;
				lplog(LOG_ERROR, L"Word %S cannot be added at buffer location %d", wTM(temp, temp2, 65001), bufferScanLocation);
				return false;
			}
	}
	if (!copy(baseVerb, buffer, where, limit)) return false;
	if (!copy(flags, buffer, where, limit)) return false;
	if (!copy(maxMatch, buffer, where, limit)) return false;
	if (!copy(minAvgCostAfterAssessCost, buffer, where, limit)) return false;
	if (!copy(lowestAverageCost, buffer, where, limit)) return false;
	if (!copy(maxLACMatch, buffer, where, limit)) return false;
	if (!copy(maxLACAACMatch, buffer, where, limit)) return false;
	if (!copy(objectRole, buffer, where, limit)) return false;
	if (!copy(verbSense, buffer, where, limit)) return false;
	if (!copy(timeColor, buffer, where, limit)) return false;
	if (!copy(beginPEMAPosition, buffer, where, limit)) return false;
	if (!copy(endPEMAPosition, buffer, where, limit)) return false;
	if (!copy(PEMACount, buffer, where, limit)) return false;
	if (!pma.read(buffer, where, limit)) return false;
	if (!forms.read(buffer, where, limit)) return false;
	if (!forms.read(buffer, where, limit)) return false;
	//if (!winnerForms.read(buffer,where,limit)) return false;
	if (!patterns.read(buffer, where, limit)) return false;
	if (!copy(object, buffer, where, limit)) return false;
	if (!copy(principalWherePosition, buffer, where, limit)) return false;
	if (!copy(principalWhereAdjectivalPosition, buffer, where, limit)) return false;
	if (!copy(originalObject, buffer, where, limit)) return false;
	unsigned int omSize;
	if (!copy(omSize, buffer, where, limit)) return false;
	bool readError = false;
	for (unsigned int I = 0; I < omSize && !readError; I++)
		objectMatches.push_back(cOM(buffer, where, limit, readError));
	if (readError)
		return false;
	unsigned int omnSize;
	if (!copy(omnSize, buffer, where, limit)) return false;
	for (unsigned int I = 0; I < omnSize && !readError; I++)
		audienceObjectMatches.push_back(cOM(buffer, where, limit, readError));
	if (readError)
		return false;
	if (!copy(quoteForwardLink, buffer, where, limit)) return false;
	if (!copy(endQuote, buffer, where, limit)) return false;
	if (!copy(nextQuote, buffer, where, limit)) return false;
	if (!copy(previousQuote, buffer, where, limit)) return false;
	if (!copy(relObject, buffer, where, limit)) return false;
	if (!copy(relSubject, buffer, where, limit)) return false;
	if (!copy(relVerb, buffer, where, limit)) return false;
	if (!copy(relPrep, buffer, where, limit)) return false;
	if (!copy(beginObjectPosition, buffer, where, limit)) return false;
	if (!copy(endObjectPosition, buffer, where, limit)) return false;
	if (!copy(tmpWinnerForms, buffer, where, limit)) return false;
	if (!copy(embeddedStorySpeakerPosition, buffer, where, limit)) return false;

	// flags
	__int64 tflags;
	if (!copy(tflags, buffer, where, limit)) return false;
	t.collectPerSentenceStats = tflags & 1; tflags >>= 1;
	t.traceTagSetCollection = tflags & 1; tflags >>= 1;
	t.traceIncludesPEMAIndex = tflags & 1; tflags >>= 1;
	t.traceUnmatchedSentences = tflags & 1; tflags >>= 1;
	t.traceMatchedSentences = tflags & 1; tflags >>= 1;
	t.traceSecondaryPEMACosting = tflags & 1; tflags >>= 1;
	t.tracePatternElimination = tflags & 1; tflags >>= 1;
	t.traceBNCPreferences = tflags & 1; tflags >>= 1;
	t.traceDeterminer = tflags & 1; tflags >>= 1;
	t.traceVerbObjects = tflags & 1; tflags >>= 1;
	t.traceObjectResolution = tflags & 1; tflags >>= 1;
	t.traceNameResolution = tflags & 1; tflags >>= 1;
	t.traceSpeakerResolution = tflags & 1; tflags >>= 1;
	t.traceRelations = tflags & 1; tflags >>= 1;
	t.traceAnaphors = tflags & 1; tflags >>= 1;
	t.traceEVALObjects = tflags & 1; tflags >>= 1;
	t.traceSubjectVerbAgreement = tflags & 1; tflags >>= 1;
	t.traceRole = tflags & 1; tflags >>= 1;
	t.printBeforeElimination = tflags & 1; tflags >>= 1;
	t.traceTestSubjectVerbAgreement = tflags & 1; tflags >>= 1;
	t.traceTestSyntacticRelations = tflags & 1; tflags >>= 1;
	t.traceTime = tflags & 1; tflags >>= 1;
	if (!copy(logCache, buffer, where, limit)) return false;
	skipResponse = -1;
	if (!copy(relNextObject, buffer, where, limit)) return false;
	if (!copy(nextCompoundPartObject, buffer, where, limit)) return false;
	if (!copy(previousCompoundPartObject, buffer, where, limit)) return false;
	if (!copy(relInternalVerb, buffer, where, limit)) return false;
	if (!copy(relInternalObject, buffer, where, limit)) return false;
	if (!copy(quoteForwardLink, buffer, where, limit)) return false;
	if (!copy(quoteBackLink, buffer, where, limit)) return false;
	if (!copy(speakerPosition, buffer, where, limit)) return false;
	if (!copy(audiencePosition, buffer, where, limit)) return false;
	hasSyntacticRelationGroup = false;
	andChainType = false;
	notFreePrep = false;
	hasVerbRelations = false;
	sameSourceCopy = -1;
	return true;
}

bool cWordMatch::writeRef(void* buffer, int& where, int limit)
{
	LFS
		if (!copy(buffer, word->first, where, limit)) return false;
	if (!copy(buffer, baseVerb, where, limit)) return false;
	if (!copy(buffer, flags, where, limit)) return false;
	if (!copy(buffer, maxMatch, where, limit)) return false;
	if (!copy(buffer, minAvgCostAfterAssessCost, where, limit)) return false;
	if (!copy(buffer, lowestAverageCost, where, limit)) return false;
	if (!copy(buffer, maxLACMatch, where, limit)) return false;
	if (!copy(buffer, maxLACAACMatch, where, limit)) return false;
	if (!copy(buffer, objectRole, where, limit)) return false;
	if (!copy(buffer, verbSense, where, limit)) return false;
	if (!copy(buffer, (unsigned char)timeColor, where, limit)) return false;
	if (!copy(buffer, beginPEMAPosition, where, limit)) return false;
	if (!copy(buffer, endPEMAPosition, where, limit)) return false;
	if (!copy(buffer, PEMACount, where, limit)) return false;
	pma.write(buffer, where, limit);
	forms.write(buffer, where, limit);
	forms.write(buffer, where, limit);
	//winnerForms.write(buffer,where,limit);
	patterns.write(buffer, where, limit);
	if (!copy(buffer, object, where, limit)) return false;
	if (!copy(buffer, principalWherePosition, where, limit)) return false;
	if (!copy(buffer, principalWhereAdjectivalPosition, where, limit)) return false;
	if (!copy(buffer, originalObject, where, limit)) return false;
	unsigned int omSize = objectMatches.size();
	if (!copy(buffer, omSize, where, limit)) return false;
	for (unsigned int I = 0; I < omSize; I++)
		objectMatches[I].write(buffer, where, limit);
	unsigned int omnSize = audienceObjectMatches.size();
	if (!copy(buffer, omnSize, where, limit)) return false;
	for (unsigned int I = 0; I < omnSize; I++)
		audienceObjectMatches[I].write(buffer, where, limit);
	if (!copy(buffer, getQuoteForwardLink(), where, limit)) return false;
	if (!copy(buffer, endQuote, where, limit)) return false;
	if (!copy(buffer, nextQuote, where, limit)) return false;
	if (!copy(buffer, previousQuote, where, limit)) return false;
	if (!copy(buffer, relObject, where, limit)) return false;
	if (!copy(buffer, relSubject, where, limit)) return false;
	if (!copy(buffer, relVerb, where, limit)) return false;
	if (!copy(buffer, relPrep, where, limit)) return false;
	if (!copy(buffer, beginObjectPosition, where, limit)) return false;
	if (!copy(buffer, endObjectPosition, where, limit)) return false;
	if (!copy(buffer, tmpWinnerForms, where, limit)) return false;
	if (!copy(buffer, embeddedStorySpeakerPosition, where, limit)) return false;

	// flags
	// 110000000000
	__int64 tflags = 0;
	tflags |= (t.traceTime) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceTestSyntacticRelations) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceTestSubjectVerbAgreement) ? 1 : 0; tflags <<= 1;
	tflags |= (t.printBeforeElimination) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceRole) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceSubjectVerbAgreement) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceEVALObjects) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceAnaphors) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceRelations) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceSpeakerResolution) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceNameResolution) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceObjectResolution) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceVerbObjects) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceDeterminer) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceBNCPreferences) ? 1 : 0; tflags <<= 1;
	tflags |= (t.tracePatternElimination) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceSecondaryPEMACosting) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceMatchedSentences) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceUnmatchedSentences) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceIncludesPEMAIndex) ? 1 : 0; tflags <<= 1;
	tflags |= (t.traceTagSetCollection) ? 1 : 0; tflags <<= 1;
	tflags |= (t.collectPerSentenceStats) ? 1 : 0;
	if (!copy(buffer, tflags, where, limit)) return false;
	if (!copy(buffer, logCache, where, limit)) return false;
	if (!copy(buffer, relNextObject, where, limit)) return false;
	if (!copy(buffer, nextCompoundPartObject, where, limit)) return false;
	if (!copy(buffer, previousCompoundPartObject, where, limit)) return false;
	if (!copy(buffer, relInternalVerb, where, limit)) return false;
	if (!copy(buffer, relInternalObject, where, limit)) return false;
	if (!copy(buffer, getQuoteForwardLink(), where, limit)) return false;
	if (!copy(buffer, quoteBackLink, where, limit)) return false;
	if (!copy(buffer, speakerPosition, where, limit)) return false;
	if (!copy(buffer, audiencePosition, where, limit)) return false;
	return true;
}

bool cWordMatch::updateMaxMatch(int len, int avgCost)
{
	LFS
		maxMatch = max(len, maxMatch);
	if (lowestAverageCost > avgCost)
	{
		maxLACMatch = len;
		lowestAverageCost = avgCost;
		return true;
	}
	if (lowestAverageCost == avgCost && len > maxLACMatch)
	{
		maxLACMatch = len;
		return true;
	}
	return false;
}

// lowerAverageCost is the original averageCost before it was set higher by assessCost
// if the lower (original) average cost is actually the lowest average cost set for the position,
//   it must be set to the higher avgCost to avoid eliminating all patterns in eliminateLoserPatterns
bool cWordMatch::updateMaxMatch(int len, int avgCost, int lowerAvgCost)
{
	LFS
		if (lowestAverageCost == lowerAvgCost && len == maxLACMatch)
		{
			lowestAverageCost = avgCost;
			return true;
		}
	return false;
}

void cWordMatch::logFormUsageCosts(void)
{
	LFS
		word->second.logFormUsageCosts(word->first);
}

bool cSource::sumMaxLength(unsigned int begin, unsigned int end, unsigned int& matchedTripletSumTotal, int& matchedSentences, bool& containsUnmatchedElement)
{
	LFS
		vector <cWordMatch>::iterator im, imend = m.begin() + end;
	unsigned int matchedTripletSum = 0;
	containsUnmatchedElement = false;
	for (im = m.begin() + begin; im != imend; im++)
	{
		matchedTripletSum += im->maxMatch;
		if ((!(im->flags & cWordMatch::flagTopLevelPattern) &&
			!(im->flags & cWordMatch::flagMetaData)) || im->maxMatch == -1)
		{
			if (im->word->second.isSeparator())
				im->setSeparatorWinner(); // make sure winner is set for wordForm counter increment (BNC)
			else
			{
				containsUnmatchedElement = true;
				im->flags |= cWordMatch::flagNotMatched;
			}
		}
	}
	unsigned int len = end - begin;
	matchedTripletSumTotal += matchedTripletSum;
	// don't print
	if ((len && (matchedTripletSum / len >= len)) || !containsUnmatchedElement)
	{
		matchedSentences++;
		if (!debugTrace.traceMatchedSentences)
			return false;
	}
	//if ((t.traceMatchedSentences || t.traceUnmatchedSentences) && len)
	//    lplog(L"Average pattern match length:%d (%d/%d)",matchedTripletSum/len,matchedTripletSum,len);
	return true;
}

void cSource::setRelPrep(int where, int relPrep, int fromWhere, int setType, int whereVerb)
{
	LFS
		int original = m[where].relPrep;
	m[where].relPrep = relPrep;
	m[where].setRelVerb(whereVerb);
	const wchar_t* setTypeStr;
	wstring tmpstr;
	switch (setType)
	{
	case PREP_PREP_SET: setTypeStr = L"PREP_SET"; break;
	case PREP_OBJECT_SET: setTypeStr = L"OBJECT_SET"; break;
	case PREP_VERB_SET: setTypeStr = L"VERB_SET"; break;
	default: setTypeStr = L"NOT_SET"; break;
	}
	if (debugTrace.traceRelations)
		lplog(LOG_RESOLUTION, L"%06d:%s:Prep loop putting %d (replacing %d) resulting in %s (%d) [%s].", where, sourcePath.c_str(), relPrep, original, loopString(relPrep, tmpstr), fromWhere, setTypeStr);
	int prepLoop = 0;
	while (relPrep != -1)
	{
		relPrep = m[relPrep].relPrep;
		if (prepLoop++ > 20)
		{
			lplog(LOG_ERROR, L"%06d:Prep loop putting %d (replacing %d) resulting in %s (%d).", where, relPrep, original, loopString(relPrep, tmpstr), fromWhere);
			m[where].relPrep = original;
			break;
		}
	}
}

// PROLOGUE(0)[1]
// 000161 _VERBREL1[1](3,4)*0 __ALLVERB[*](4)
unsigned int cSource::getMaxDisplaySize(vector <cWordMatch>::iterator& im, int numPosition)
{
	LFS
		size_t size = im->word->first.length() + 3; // 2 is for the parens
	if (numPosition > 9) size++;
	if (numPosition > 99) size++;
	unsigned int len = im->getShortAllFormAndInflectionLen();
	size = max(size, len);
	for (int nextPEMAPosition = im->beginPEMAPosition; nextPEMAPosition != -1; nextPEMAPosition = pema[nextPEMAPosition].nextByPosition)
	{
		cPatternElementMatchArray::tPatternElementMatch* pem = pema.begin() + nextPEMAPosition;
		unsigned int pbegin = pem->begin + numPosition, pend = pem->end + numPosition;
		// name + pdiff + begin + end (5 for format characters + 1 for begin and 1 for end)
		unsigned int tmp = patterns[pem->getParentPattern()]->name.length() + patterns[pem->getParentPattern()]->differentiator.length() + 7;
		if (pem->isChildPattern())
		{
			tmp += patterns[pem->getChildPattern()]->name.length();
			tmp += patterns[pem->getChildPattern()]->differentiator.length();
			tmp += 1 + 4; // (4 is for the format characters + 1 for begin)
			int childEnd = pem->getChildLen() + numPosition;
			if (childEnd > 9) tmp++;
			if (childEnd > 99) tmp++;
			if (childEnd > 999) tmp++;
			if (childEnd > 9999) tmp++;
		}
		else
			tmp += Forms[im->getFormNum(pem->getChildForm())]->shortName.length() + 1;
		if (pbegin > 9) tmp++;
		if (pend > 9) tmp++;
		if (pbegin > 99) tmp++;
		if (pend > 99) tmp++;
		if (pbegin > 999) tmp++;
		if (pend > 999) tmp++;
		if (pbegin > 9999) tmp++;
		if (pend > 9999) tmp++;
		int cost = pem->getOCost();
		tmp += 4;  // one for single digit cost, another for *, another for space between parent and child
		if (cost > 9) tmp++;
		if (cost > 99) tmp++;
		if (cost > 999) tmp++;
		if (cost > 9999) tmp++;
		if (debugTrace.traceIncludesPEMAIndex)
			tmp += 28;
		size = max(size, tmp);
	}
	return size;
}

bool cSource::matchPattern(cPattern* p, int begin, int end, bool fill)
{
	LFS // DLFS
		int expense = clock();
	bool matchFound = false;
	for (int pos = begin; pos < (int)end - 1; pos++) // skip period
	{
#ifdef LOG_PATTERN_MATCHING
		if (debugTrace.tracePatternMatching)
			lplog(L"%d:pattern %s[%s]:---------------------", pos, p->name.c_str(), p->differentiator.c_str());
#endif
		// do not put pos==begin here because we don't know whether we have split the sentence correctly!
		if (p->onlyBeginMatch && pos && (!m[pos - 1].word->second.isSeparator() || m[pos - 1].queryForm(relativizerForm) >= 0)) continue; //  || m[pos-1].queryForm(relativeForm)
		if (p->strictNoMiddleMatch && pos && iswalpha(m[pos - 1].word->first[0])) continue;
		// cannot come after a pronoun which would definitely be a subject (I, we, etc)
		if (p->notAfterPronoun && pos && (m[pos - 1].queryForm(nomForm) >= 0 || (m[pos - 1].queryForm(personalPronounForm) >= 0 && m[pos - 1].word->first != L"you" && m[pos - 1].word->first != L"it"))) continue;
		if (p->afterQuote && (pos < 2 ||
			((m[pos - 1].word->second.query(quoteForm) < 0 || (m[pos - 1].word->second.inflectionFlags & CLOSE_INFLECTION) != CLOSE_INFLECTION)) &&
			(m[pos - 2].word->second.query(quoteForm) < 0 || m[pos - 1].word->first == L","))) continue;
		if (p->matchPatternPosition(*this, pos, fill, debugTrace))
		{
#ifdef QLOGPATTERN
			if (pass > 0) lplog(L"Pattern %s matched against sentence position %d", p->name.c_str(), pos);
#endif
			matchFound = true;
		}
	}
	p->evaluationTime += clock() - expense;
	return matchFound;
}

bool cSource::matchPatternAgainstSentence(cPattern* p, int s, bool fill)
{
	LFS // DLFS
		return matchPattern(p, sentenceStarts[s], sentenceStarts[s + 1], fill);
}


void cSource::logOptimizedString(wchar_t* line, unsigned int lineBufferLen, unsigned int& linepos)
{
	LFS
		wchar_t dest[2048];
	if (!linepos)
	{
		lplog(L"");
		//for (wchar_t *p=line,*pEnd=line+ lineBufferLen; p!=pEnd; p++) *p=' '; // cannot use _wcsnset_s!
		wmemset(line, L' ', lineBufferLen);
		return;
	}
	int len = linepos;
	while (len && line[len - 1] == L' ') len--;
	if (!len)
	{
		linepos = 0;
		return;
	}
	line[len] = 0;
	int alignedLen = len & ~3;
	int I, destI = 0;
	for (I = 0; I < alignedLen; I += 4)
	{
		if (*((int*)(line + I)) == ((32 << 24) + (32 << 16) + (32 << 8) + 32))
			dest[destI++] = L'\t';
		else
			for (int I2 = I; I2 < I + 4 && I2 < alignedLen; I2++, destI++)
				dest[destI] = line[I2];
	}
	while (I < len)
		dest[destI++] = line[I++];
	dest[destI] = L'\n';
	dest[destI + 1] = 0;
	logstring(LOG_INFO, dest);
	linepos = 0;
	//for (wchar_t *p=line,*pEnd=line+ lineBufferLen; p!=pEnd; p++) *p=' '; // cannot use _wcsnset_s!
	wmemset(line, L' ', lineBufferLen);
}

int cSource::getPEMAPosition(int position, int line)
{
	LFS
		int PEMAPosition, offset = line;
	for (PEMAPosition = m[position].beginPEMAPosition; offset > 0 && PEMAPosition != -1; PEMAPosition = pema[PEMAPosition].nextByPosition, offset--);
	if (PEMAPosition >= (signed)pema.count)
		lplog(LOG_FATAL_ERROR, L"%d:Incorrect PEMA Position %d derived from line %d.", position, PEMAPosition, line);
	if (PEMAPosition < 0)
		lplog(L"%d:Line %d not reachable", position, line);
	return PEMAPosition;
}

// print a sentence
// end should normally be set to words.size();
#define LINE_BUFFER_LEN 2048
int cSource::printSentence(unsigned int rowsize, unsigned int begin, unsigned int end, bool containsNotMatched)
{
	LFS
		wchar_t printLine[LINE_BUFFER_LEN];
	wchar_t bufferZone[2048];
	unsigned int totalSize, I = begin, maxLines, startword = begin, maxPhraseMatches, linepos = 0;
	printMaxSize.reserve(end - begin);
	for (unsigned int fi = 0; fi < end - begin; fi++) printMaxSize[fi] = 0;
	bufferZone[0] = 0xFEFE;
	bufferZone[1] = 0xCECE;
	int printStart = begin;
#ifndef LOG_RELATIVE_LOCATION
	printStart = 0;
#endif
	while (startword < end)
	{
		maxLines = 0;
		maxPhraseMatches = 0;
		startword = I;
		logOptimizedString(printLine, LINE_BUFFER_LEN, linepos);
		for (unsigned int line = 0; (line <= (maxLines + maxPhraseMatches) || (maxLines == 0 && I < end)); line++)
		{
			totalSize = 0;
			logOptimizedString(printLine, LINE_BUFFER_LEN, linepos);
			vector <cWordMatch>::iterator im;
			for (I = startword, im = m.begin() + startword; I < end && totalSize < rowsize; I++, im++)
			{
				if (im->word == Words.sectionWord) continue; // paragraph or sentence end
				maxPhraseMatches = max(maxPhraseMatches, im->PEMACount);
				maxLines = max(maxLines, im->formsSize());
				if (printMaxSize[I - begin] == 0)
					printMaxSize[I - begin] = getMaxDisplaySize(im, I - printStart) + 1;
				totalSize += printMaxSize[I - begin] + 1;
				if (totalSize > rowsize)
				{
					if (I == startword)
					{
						lplog(LOG_INFO | LOG_ERROR, L"ERROR:sqlrow too large size to print - word at position %d=%d totalSize=%d > rowSize=%d ...", I - begin, printMaxSize[I - begin], totalSize, rowsize);
						return 0;
					}
					break;
				}
				if (line == 0)
				{
					size_t len = im->word->first.length();
					if (im->flags & cWordMatch::flagNotMatched)
						printLine[linepos++] = (im->maxMatch < 0) ? L'^' : L'-';
					wcscpy(printLine + linepos, im->word->first.c_str());
					if (im->flags & (cWordMatch::flagAddProperNoun | cWordMatch::flagOnlyConsiderProperNounForms | cWordMatch::flagFirstLetterCapitalized))
						printLine[linepos] = towupper(printLine[linepos]);
					if (im->flags & (cWordMatch::flagNounOwner))
					{
						printLine[linepos + len++] = L'\'';
						if (printLine[linepos + len - 2] != L's') printLine[linepos + len++] = L's';
					}
					if (im->flags & cWordMatch::flagAllCaps)
						for (wchar_t* ch = printLine + linepos; *ch && (ch - printLine) < 2047; ch++) *ch = towupper(*ch);
					if (containsNotMatched)
					{
						printLine[linepos + len] = 0;
						logstring(LOG_NOTMATCHED, printLine + linepos - ((im->flags & cWordMatch::flagNotMatched) ? 1 : 0));
						logstring(LOG_NOTMATCHED, L" ");
					}
					wstring flags;
					if (im->flags & cWordMatch::flagAddProperNoun) flags += L"[PN]";
					if (im->flags & cWordMatch::flagOnlyConsiderProperNounForms) flags += L"[OPN]";
					if (im->flags & cWordMatch::flagRefuseProperNoun) flags += L"[RPN]";
					if (im->flags & cWordMatch::flagOnlyConsiderOtherNounForms) flags += L"[OON]";
					if (flags.size())
						len += wsprintf(printLine + linepos + len, L"%s", flags.c_str());
					wsprintf(printLine + linepos + len, L"(%d)[%d]", I - printStart, im->lowestAverageCost);
				}
				else
				{
					if (im->formsSize() <= line - 1)
					{
						int PEMAOffset;
						if (((line - 1) >= maxLines + im->PEMACount) || (line - 1) < maxLines || (PEMAOffset = getPEMAPosition(I, line - 1 - maxLines)) < 0)
							printLine[linepos] = 0;
						else
						{
							cPatternElementMatchArray::tPatternElementMatch* pem = pema.begin() + PEMAOffset;
							if (debugTrace.traceIncludesPEMAIndex)
							{
								wsprintf(printLine + linepos, L"%06d %06d %06d %06d ",
									pem - pema.begin(), pem->nextByPatternEnd, pem->nextByChildPatternEnd, pem->nextPatternElement);
								linepos += 28;
							}
							if (pem->isChildPattern())
								wsprintf(printLine + linepos, L"%s[%s](%d,%d)*%d %s[*](%d)%c",
									patterns[pem->getParentPattern()]->name.c_str(), patterns[pem->getParentPattern()]->differentiator.c_str(), pem->begin - printStart + I, pem->end - printStart + I, pem->getOCost(),
									patterns[pem->getChildPattern()]->name.c_str(),
									pem->getChildLen() + I - printStart,
									(pem->flagSet(cPatternElementMatchArray::ELIMINATED)) ? 'E' : ' ');
							else
								wsprintf(printLine + linepos, L"%s[%s](%d,%d)*%d %s%c",
									patterns[pem->getParentPattern()]->name.c_str(), patterns[pem->getParentPattern()]->differentiator.c_str(), pem->begin - printStart + I, pem->end - printStart + I, pem->getOCost(),
									Forms[im->getFormNum(pem->getChildForm())]->shortName.c_str(),
									(pem->flagSet(cPatternElementMatchArray::ELIMINATED)) ? 'E' : ' ');
							if (debugTrace.traceIncludesPEMAIndex)
								linepos -= 28;
						}
					}
					else
					{
						if (im->isWinner(line - 1) &&
							((im->word->second.formsSize() > line - 1 && im->forms.isSet(im->word->second.forms()[line - 1])) ||
								(im->word->second.formsSize() == line - 1 && (im->flags & cWordMatch::flagAddProperNoun))))
							im->getShortFormInflectionEntry(line - 1, printLine + linepos);
						else
							printLine[linepos] = 0;
					}
				}
				if (wcslen(printLine) < sizeof(printLine) - 1) printLine[wcslen(printLine)] = L' ';
				linepos += printMaxSize[I - begin];
			}
		}
	}
	if (containsNotMatched)
		logstring(LOG_NOTMATCHED, L"\n");
	if (bufferZone[0] != 0xFEFE || bufferZone[1] != 0xCECE)
		printf("STOP! BUFFER OVERRUN!");

	return 0;
}

bool aloneOnLine(wchar_t* buffer, wchar_t* loc, const wchar_t* pattern, wchar_t*& startScanningPosition, __int64 bufferLen, bool checkOnlyBeginning = false)
{
	LFS
		// is sequence alone on line?
		wchar_t* after = loc + wcslen(pattern);
	if (loc > buffer) loc--;
	bool newline = false;
	while (loc >= buffer && iswspace(*loc))
	{
		loc--;
		if (*loc == 13 || *loc == 10) newline = true;
	}
	if (loc != buffer) loc++;
	if (loc != buffer && !newline)
	{
		//lplog(L"Sequence %s found not alone on line (back space).",pattern);
		return false;
	}
	if (checkOnlyBeginning)
	{
		startScanningPosition = loc;
		return true;
	}
	while (after - buffer < bufferLen && *after != 13 && *after != 10)
	{
		if (!iswspace(*after))
		{
			//lplog(L"Sequence %s found not alone on line (after space).",pattern);
			return false;
		}
		after++;
	}
	startScanningPosition = loc;
	return true;
}

int cSource::scanUntil(const wchar_t* start, int repeat, bool printError)
{
	LFS
		wchar_t* loc = bookBuffer - 1;
	while ((loc - bookBuffer) < bufferLen)
	{
		if (!(loc = wcsstr(loc + 1, start)))
		{
			if (!wcscmp(start, L"~~BEGIN"))
				return 0;
			if (!(loc = wcsstr(bookBuffer, L"~~BEGIN")))
			{
				if (printError)
				{
					lplog(LOG_ERROR, L"Unable to find start '%s'.", start);
					wprintf(L"\nCould not find start '%s'.\n", start);
				}
				return -1;
			}
			else
				start = L"~~BEGIN";
		}
		wchar_t* startScanningPosition;
		if (aloneOnLine(bookBuffer, loc, start, startScanningPosition, bufferLen) && --repeat == 0)
		{
			bufferScanLocation = startScanningPosition - bookBuffer;
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

bool checkIsolated(const wchar_t* word, vector <cWordMatch>& m, int I)
{
	return m[I].word == Words.sectionWord && m[I + 1].word->first == word && (m[I + 1].flags & cWordMatch::flagAllCaps) &&
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
bool cSource::analyzeEnd(wstring& path, int begin, int end, bool& multipleEnds)
{
	LFS
		int w = 0;
	bool endFound;
	for (w = begin; w < end; w++)
		if (m[w].word->first == L"end") break;
	endFound = w != end;
	if (w == end)
		w = begin;
	if (!multipleEnds)
	{
		// THE END
		if (endFound && (m[w].flags & cWordMatch::flagAllCaps) && w > 3 &&
			m[w - 2].word == Words.sectionWord &&
			m[w - 1].word->first == L"the" && (m[w - 1].flags & cWordMatch::flagAllCaps) &&
			(w + 1 >= m.size() || m[w + 1].word == Words.sectionWord || (w + 2 < m.size() && (m[w + 1].word->first == L"." && m[w + 2].word == Words.sectionWord))))
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
					if (multipleEnds = m[I].word == Words.sectionWord && m[I + 1].word->first == L"the" && (m[I + 1].flags & cWordMatch::flagAllCaps) && m[I + 2].word->first == L"end" && (m[I + 2].flags & cWordMatch::flagAllCaps) &&
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
	for (; w < end; w++) if (m[w].word->first == L"project") break;
	if (w == end) return false;
	for (; w < end; w++)
		if (m[w].word->first == L"gutenberg")
		{
			if (!endFound)
			{
				wstring logres;
				lplog(LOG_ERROR, L"%s\n%s:%d:start of parse should be moved down!", phraseString(begin, end, logres, true).c_str(), path.c_str(), begin);
				return false;
			}
			return true;
		}

	return false;
}

// if date of dictionary or date of cache is before date of source, return true
// if date of dictionary > date of cache return true
bool cSource::parseNecessary(wchar_t* path)
{
	LFS
#ifdef ALWAYS_PARSE
		return true;
#endif
	storageLocation = path;
	struct _stat buffer;
	wstring locationCache = storageLocation + L".cache";
	int result = _wstat(locationCache.c_str(), &buffer);
	if (result < 0) return true;
	__time64_t cacheLastModified = buffer.st_mtime;
	result = _wstat(L"WordCacheFile", &buffer);
	if (result < 0) return true;
	__time64_t dictionaryLastModified = buffer.st_mtime;
	result = _wstat(storageLocation.c_str(), &buffer);
	if (result < 0) return true;
	__time64_t sourceLastModified = buffer.st_mtime;
	return (//sourceLastModified>dictionaryLastModified ||
		sourceLastModified > cacheLastModified ||
		dictionaryLastModified > cacheLastModified);
}

// 
// >= two blank lines before begin and >= two blank lines after end.
// doesn't begin with a ".
// not more than 10 words long.
// the end is an abbreviation
// it is an expected extension of another chapter heading 
// the end is NOT in a list of endings
bool cSource::isSectionHeader(unsigned int begin, unsigned int end, unsigned int& sectionEnd)
{
	LFS
		if (begin > 0 && m[begin - 1].word != Words.sectionWord) return false; // less than two lines before section header - reject
	if (m[end].word != Words.sectionWord) return false; // less than two lines after section header - reject
	if (m[begin].word->first == primaryQuoteType || m[begin].word->first == L"“") return false; // buffer begins with " or ' - probably a quote
	if (end - begin > 10) return false; // too long
	bool abbreviation = (m[end - 1].word->first == L"." && m[end - 2].beginPEMAPosition >= 0 &&
		(m[end - 2].pma.queryPattern(L"_ABB") != -1 ||
			m[end - 2].pma.queryPattern(L"_MEAS_ABB") != -1 ||
			m[end - 2].pma.queryPattern(L"_STREET_ABB") != -1 ||
			m[end - 2].pma.queryPattern(L"_BUS_ABB") != -1 ||
			m[end - 2].pma.queryPattern(L"_TIME_ABB") != -1 ||
			m[end - 2].pma.queryPattern(L"_DATE_ABB") != -1 ||
			m[end - 2].pma.queryPattern(L"_HON_ABB") != -1));
	if (!abbreviation &&
		(sections.size() < 2 || begin - sections[sections.size() - 1].endHeader >= 2 ||
			sections[sections.size() - 1].subHeadings.size() >= sections[sections.size() - 2].subHeadings.size()))
	{
		wstring sWord = m[end - 1].word->first;
		if (sWord == primaryQuoteType || // buffer ends with " or ' - probably a quote
			sWord == L":" ||              // buffer ends with : - probably the start of a list
			sWord == L"," || sWord == L"-" || sWord == L"—" || sWord == L")" || sWord == L"!" ||
			sWord == L"?" || sWord == L"." || sWord == L"--") return false; // buffer ends with , -, or ) probably not a title
	}
	sectionEnd = end;
	wstring sSection;
	for (unsigned int I = begin; I < end; I++) sSection = sSection + m[I].word->first + L" ";
	sSection.erase(sSection.length() - 1);
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION, L"%06d:Section header '%s' encountered", begin, sSection.c_str());
	return true;
}

int cSource::reportUnmatchedElements(int begin, int end, bool logElements)
{
	LFS
		if (!debugTrace.traceParseInfo)
			return 0;
	int len = -1, firstUnmatched = 0, totalUnmatched = 0;
	bool matched = true;
	for (int I = begin; I < end; )
		if ((m[I].flags & cWordMatch::flagTopLevelPattern) || m[I].word->second.isSeparator() ||
			(m[I].flags & cWordMatch::flagMetaData))
		{
			if (!matched && logElements)
			{
				if (debugTrace.traceUnmatchedSentences)
				{
					if (firstUnmatched != I - 1)
						lplog(L"Unmatched position %d-%d.", firstUnmatched, I - 1);
					else
						lplog(L"Unmatched position %d.", firstUnmatched);
				}
				totalUnmatched += I - firstUnmatched;
			}
			if (len < 0) I++;
			else I += len;
			matched = true;
		}
		else
		{
			if (matched) firstUnmatched = I;
			matched = false;
			I++;
		}
	if (!matched && logElements)
	{
		if (debugTrace.traceUnmatchedSentences)
		{
			if (firstUnmatched != m.size() - 1)
				lplog(L"Unmatched position %d-%d.", firstUnmatched, end - 1);
			else
				lplog(L"Unmatched position %d.", firstUnmatched);
		}
		totalUnmatched += end - firstUnmatched;
	}
	return totalUnmatched;
}

void cSource::consolidateWinners(int begin)
{
	LFS
		int* wa = NULL, numWinners = 0, position = begin, maxMatch = 0;
	pema.generateWinnerConsolidationArray(lastPEMAConsolidationIndex, wa, numWinners);
	vector <cWordMatch>::iterator im, imend = m.end();
	for (im = m.begin() + begin; position < lastSourcePositionSet && im != imend; im++, position++)
	{
		im->pma.consolidateWinners(lastPEMAConsolidationIndex, pema, wa, position, maxMatch, debugTrace); // bool consolidated=
		int position2 = position;
		for (vector <cWordMatch>::iterator im2 = im, im2End = im + maxMatch; im2 != im2End; im2++, position2++)
			im2->maxMatch = max(im2->maxMatch, maxMatch);
		pema.getNextValidPosition(lastPEMAConsolidationIndex, wa, &im->beginPEMAPosition, cPatternElementMatchArray::BY_POSITION);
		pema.translate(lastPEMAConsolidationIndex, wa, &im->beginPEMAPosition, cPatternElementMatchArray::BY_POSITION);
		pema.getNextValidPosition(lastPEMAConsolidationIndex, wa, &im->endPEMAPosition, cPatternElementMatchArray::BY_POSITION);
		pema.translate(lastPEMAConsolidationIndex, wa, &im->endPEMAPosition, cPatternElementMatchArray::BY_POSITION);
	}
	pema.consolidateWinners(lastPEMAConsolidationIndex, wa, numWinners, debugTrace);
	position = begin;
	for (im = m.begin() + begin; position < lastSourcePositionSet && im != imend; im++, position++)
	{
		if (im->endPEMAPosition < 0 && im->beginPEMAPosition >= 0)
		{
			im->endPEMAPosition = im->beginPEMAPosition;
			while (pema[im->endPEMAPosition].nextByPosition >= 0)
				im->endPEMAPosition = pema[im->endPEMAPosition].nextByPosition;
		}
		im->PEMACount = pema.generatePEMACount(im->beginPEMAPosition);
		if (!im->PEMACount && (im->flags & cWordMatch::flagTopLevelPattern))
		{
			im->flags &= ~cWordMatch::flagTopLevelPattern;
			if (debugTrace.tracePatternElimination)
				lplog(L"%d:lost all top-level patterns", position);
		}
	}
	lastPEMAConsolidationIndex = max(pema.count, 1); // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet = -1;
}

void cSource::clearTagSetMaps(void)
{
	LFS
		for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
			pemaMapToTagSetsByPemaByTagSet[ts].clear();
}

int cSource::printSentences(bool updateStatistics, unsigned int unknownCount, unsigned int quotationExceptions, unsigned int totalQuotations, int& globalOverMatchedPositionsTotal)
{
	LFS
		if (!sentenceStarts.size() || sentenceStarts[sentenceStarts.size() - 1] != m.size())
			sentenceStarts.push_back(m.size());
	int matchedSentences = 0, overMatchedPositionsTotal = 0, lastProgressPercent = -1, where;
	unsigned int matchedTripletSumTotal = 0;
	unsigned int totalUnmatched = 0, patternsMatched = 0, patternsTried = 0;
	bool containsUnmatchedElement, printedHeader = false;
	section = 0;
	set <int> wordIds;
	readWordIdsNeedingWordRelations(wordIds);
	if (Words.initializeWordRelationsFromDB(mysql, wordIds, true, debugTrace.traceParseInfo) < 0)
		return -1;
	for (vector <cWordMatch>::iterator im = m.begin(), imEnd = m.end(); im != imEnd; im++)
		im->getMainEntry()->second.flags &= ~cSourceWordInfo::inSourceFlag;
	// these patterns exclude NOUN patterns containing other NOUN patterns
	int memoryPerSentenceBySize[512], timePerSentenceBySize[512], sizePerSentenceBySize[512];
	if (debugTrace.collectPerSentenceStats)
	{
		memset(memoryPerSentenceBySize, 0, 512 * sizeof(int));
		memset(timePerSentenceBySize, 0, 512 * sizeof(int));
		memset(sizePerSentenceBySize, 0, 512 * sizeof(int));
	}
	for (unsigned int s = 0; s + 1 < sentenceStarts.size() && !exitNow; s++)
	{
		if ((where = s * 100 / sentenceStarts.size()) > lastProgressPercent)
		{
			wprintf(L"PROGRESS: %d%% sentences printed with %04d seconds elapsed (%I64d bytes) \r", where, clocksec(), memoryAllocated);
			lastProgressPercent = where;
		}
		unsigned int begin = sentenceStarts[s];
		unsigned int end = sentenceStarts[s + 1];
		while (end && m[end - 1].word == Words.sectionWord)
			end--; // cut off end of paragraphs
		if (begin >= end)
		{
			matchedSentences++;
			continue;
		}
		debugTrace = m[begin].t;
		logCache = m[begin].logCache;

		// clear tag set maps
		clearTagSetMaps();
		int sentenceStartTime = clock();
		int sentenceStartMemory = pema.count;
		patternsMatched += matchPatternsAgainstSentence(s, patternsTried);
		if (debugTrace.printBeforeElimination)
			printSentence(SCREEN_WIDTH, begin, end, false);  // print before elimination
		for (unsigned int I = begin; I < end && !exitNow; I++)
		{
			cPatternMatchArray::tPatternMatch* pma = m[I].pma.content;
			for (int PMAElement = 0, count = m[I].pma.count; PMAElement < count; PMAElement++, pma++)
				addCostFromRecalculatingAloneness(I, pma);
		}
		int overMatchedPositions = eliminateLoserPatterns(begin, end);
		if (debugTrace.collectPerSentenceStats)
			memoryPerSentenceBySize[end - begin] += pema.count - sentenceStartMemory;
		if (debugTrace.printBeforeElimination)
			printSentence(SCREEN_WIDTH, begin, end, false);  // print after costing is assessed
		consolidateWinners(begin);
		// clear them because the PEMA references are invalid after consolidation.
		clearTagSetMaps();
		//if (section<sections.size() && sections[section].begin==begin)
		//  section++;
		// correct source from probabilistic Stanford testing
		for (unsigned int I = begin; I < end && !exitNow; I++)
			ruleCorrectLPClass(I, begin);
		unsigned int unmatchedElements = reportUnmatchedElements(begin, end, true);
		unsigned int ignoredPatternsTried = 0;
		matchIgnoredPatternsAgainstSentence(s, ignoredPatternsTried, true);
		lastPEMAConsolidationIndex = max(pema.count, 1); // minimum PEMA offset is 1 - not 0
		totalUnmatched += unmatchedElements;
		if (sumMaxLength(begin, end, matchedTripletSumTotal, matchedSentences, containsUnmatchedElement))
		{
			if (debugTrace.traceUnmatchedSentences || debugTrace.traceMatchedSentences)
			{
				if (containsUnmatchedElement && !printedHeader)
				{
					lplog(LOG_NOTMATCHED, L"\n%s\n", storageLocation.c_str());
					printedHeader = true;
				}
				if (debugTrace.traceMatchedSentences)
					printSentence(SCREEN_WIDTH, begin, end, containsUnmatchedElement);  // change DOS Window Menu "Defaults" choice
			}
		}
#ifdef LOG_OVERMATCHED
		else if (overMatchedPositions)
			printSentence(SCREEN_WIDTH, begin, end, containsUnmatchedElement);  // change DOS Window Menu "Defaults" choice
#endif
		overMatchedPositionsTotal += overMatchedPositions;
		if (debugTrace.collectPerSentenceStats)
		{
			timePerSentenceBySize[end - begin] += clock() - sentenceStartTime;
			sizePerSentenceBySize[end - begin] += end - begin;
		}
		unsigned int sectionEnd;
		if (isSectionHeader(begin, end, sectionEnd))
		{
			if (sections.size() && begin - sections[sections.size() - 1].endHeader < 2)
			{
				sections[sections.size() - 1].endHeader = sectionEnd;
				sections[sections.size() - 1].subHeadings.push_back(begin);
			}
			else
				sections.push_back(cSection(begin, sectionEnd));
		}
	}
	if (sections.size())
		sections.push_back(cSection(sections[sections.size() - 1].endHeader, sentenceStarts[sentenceStarts.size() - 1]));
	else if (sentenceStarts.size() > 1)
		sections.push_back(cSection(sentenceStarts[0], sentenceStarts[sentenceStarts.size() - 1]));
	if (100 > lastProgressPercent)
		wprintf(L"PROGRESS: 100%% sentences printed with %04d seconds elapsed (%I64d bytes) \r", clocksec(), memoryAllocated);
	// accumulate globals
	globalOverMatchedPositionsTotal += overMatchedPositionsTotal;
	if (debugTrace.collectPerSentenceStats)
	{
		lplog(L"CPSS SIZE MEMT    TIMET MEM  TIME %%");
		for (unsigned I = 0; I < 256; I++)
			if (sizePerSentenceBySize[I])
				lplog(L"CPSS %03d: %07d %05d %04d %03d %02d%%", I, memoryPerSentenceBySize[I], timePerSentenceBySize[I] / CLOCKS_PER_SEC, memoryPerSentenceBySize[I] / sizePerSentenceBySize[I], timePerSentenceBySize[I] / sizePerSentenceBySize[I], sizePerSentenceBySize[I] * 100 / m.size());
	}
	// [222 out of 2374:009%] 03333606 words Parsing A75...
	if (updateStatistics)
		updateSourceStatistics(sentenceStarts.size() - 1, matchedSentences, m.size(), unknownCount,
			totalUnmatched, overMatchedPositionsTotal, totalQuotations, quotationExceptions, clock() - beginClock, matchedTripletSumTotal);
	extern int numSourceLimit;
	if (m.size() && numSourceLimit == 0)
		lplog(L"%-50.49s:Matched sentences=%06.2f%% Positions:(Total=%06d Unknown=%05d-%5.2f%% Unmatched=%04d-%5.2f%% Overmatched=%05d-%5.2f%%) Quotation exceptions=%03d-%5.2f%% MS/word=%05.3f Average pattern match:%02d ",
			storageLocation.c_str(), (float)matchedSentences * 100 / (sentenceStarts.size() - 1), m.size(),
			unknownCount, (float)unknownCount * 100 / m.size(), totalUnmatched, (float)totalUnmatched * 100.0 / m.size(), overMatchedPositionsTotal, (float)overMatchedPositionsTotal * 100 / m.size(),
			quotationExceptions, (totalQuotations) ? (float)quotationExceptions * 100 / totalQuotations : 0,
			(float)(clock() - beginClock) / ((float)CLOCKS_PER_SEC / 1000 * m.size()), matchedTripletSumTotal / m.size());
#ifdef LOG_PATTERN_STATISTICS
	lplog(L"Matched Patterns=%d Tried Patterns=%d.", patternsMatched, patternsTried);
#endif
	return totalUnmatched;
}

int cSource::printSentencesCheck(bool skipCheck)
{
	LFS
		struct __stat64 buffer;
	wchar_t logFilename[1024];
	extern wstring logFileExtension;
	wsprintf(logFilename, L"main%S.lplog", logFileExtension.c_str());
	vector <cWordMatch>::iterator im = m.begin(), mend = m.end();
	for (unsigned int I = 0; im != mend; im++, I++)
	{
		if (im->flags & cWordMatch::flagAddProperNoun)
		{
			im->word->second.setProperNounUsageCost();
		}
	}
	if (!skipCheck && _wstat64(logFilename, &buffer) >= 0 && buffer.st_size > 2 * 1024 * 1024) return 0;
	if (sentenceStarts.size() == 0 || !(m[sentenceStarts[0]].t.traceMatchedSentences ^ (logMatchedSentences | logUnmatchedSentences))) return 0;
	for (unsigned int s = 0; s + 1 < sentenceStarts.size() && !exitNow; s++)
	{
		unsigned int begin = sentenceStarts[s], end = sentenceStarts[s + 1];
		while (end && m[end - 1].word == Words.sectionWord)
			end--; // cut off end of paragraphs
		if (begin >= end)
			continue;
		logCache = m[begin].logCache;
		if (m[begin].t.traceMatchedSentences ^ (logMatchedSentences | logUnmatchedSentences))
			printSentence(SCREEN_WIDTH, begin, end, false);  // print before elimination
	}
	return 0;
}

int cSource::matchIgnoredPatternsAgainstSentence(unsigned int s, unsigned int& patternsTried, bool fill)
{
	LFS
		int numPatternsMatched = 0;
	for (unsigned int p = 0; p < patterns.size(); p++)
	{
		if (!patterns[p]->ignoreFlag) continue;
		patternsTried++;
		if (matchPatternAgainstSentence(patterns[p], s, fill))
			numPatternsMatched++;
	}
	return numPatternsMatched;
}

#ifdef LOG_OLD_MATCH
int cSource::matchPatternsAgainstSentence(unsigned int s, unsigned int& patternsTried)
{
	LFS
		bool match = false;
	pass = 0;
	int patternsMatched = 0;
	// run through every pattern up to the last pattern referred to by any pattern
	for (int p = 0; p <= cPatternReference::lastPatternReference; p++, patternsTried++)
	{
		if (!patterns[p]->ignore && matchPatternAgainstSentence('A', patterns[p], s))
		{
#ifdef LOG_IPATTERN
			lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.", 'A', p, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), s);
#endif
			match |= patterns[p]->isFutureReference;
			patternsMatched++;
		}
	}
	if (match && cPatternReference::lastPatternReference != -1)
	{
		pass = 1;
		// run through every pattern that refers to another future pattern
		match = false;
		for (int p = cPatternReference::firstPatternReference; p <= cPatternReference::lastPatternReference; p++)
			if (patterns[p]->containsFutureReference)
			{
				if (!patterns[p]->ignore && matchPatternAgainstSentence('B', patterns[p], s))
				{
					match = true;
#ifdef LOG_IPATTERN
					lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.", 'B', p, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), s);
#endif
					patternsMatched++;
				}
				patternsTried++;
			}
		// run through every pattern that refers to the patterns that refer to other patterns
		if (match)
			for (int p = cPatternReference::firstPatternReference; p <= cPatternReference::lastPatternReference; p++)
				if (patterns[p]->indirectFutureReference)
				{
					if (!patterns[p]->ignore && matchPatternAgainstSentence('C', patterns[p], s))
					{
#ifdef LOG_IPATTERN
						lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.", 'C', p, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), s);
#endif
						patternsMatched++;
					}
					patternsTried++;
				}
	}
	// run through the last patterns that are not referred to by any other pattern
	pass = 0;
	for (int p = cPatternReference::lastPatternReference + 1; p < (int)patterns.size(); p++, patternsTried++)
		if (!patterns[p]->ignore && matchPatternAgainstSentence('D', patterns[p], s))
		{
#ifdef LOG_IPATTERN
			lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.", 'D', p, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), s);
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
int cSource::matchPatternsAgainstSentence(unsigned int s, unsigned int& patternsTried)
{
	LFS
		int numPatternsMatched = 0;
	cBitObject<32, 5, unsigned int, 32> patternsToTry, patternsToTryLast, patternsToTryNext = patternsWithNoChildren, patternsMatched;
	for (pass = 0; pass < 6; pass++)
	{
		patternsToTry = patternsToTryNext;
#ifdef LOG_IPATTERN
		wstring patternsString;
		int lastP = -1;
		for (int p = patternsToTry.first(); p >= 0; lastP = p, p = patternsToTry.next())
		{
			if (lastP != -1 && patterns[p]->name == patterns[lastP]->name)
				patternsString += "," + patterns[p]->differentiator;
			else
			{
				if (lastP != -1) patternsString += "] ";
				patternsString += patterns[p]->name + "[" + patterns[p]->differentiator;
			}
		}
		lplog(L"%c:Patterns %s matched against sentence %04d.", 'A' + pass, patternsString.c_str(), s);
#endif
		patternsToTryNext.clear();
		for (int p = patternsToTry.first(); p >= 0; p = patternsToTry.next())
		{
			patternsToTryNext.reset(p);
			if (!patterns[p]->mandatoryChildPatterns.hasAll(patternsMatched)) continue;
			patternsTried++;
			if (!patterns[p]->ignoreFlag && matchPatternAgainstSentence(patterns[p], s, true))
			{
				patternsToTryNext |= patterns[p]->ancestorPatterns;
				patternsMatched.set(p);
				numPatternsMatched++;
			}
		}
		patternsToTryLast |= patternsToTryNext;
		patternsToTryNext -= patternsWithNoParents;
	}
	patternsToTryLast &= patternsWithNoParents;
	pass = -1;
	for (int p = patternsToTryLast.first(); p >= 0; p = patternsToTryLast.next())
	{
		if (!patterns[p]->mandatoryChildPatterns.hasAll(patternsMatched)) continue;
		patternsTried++;
		if (!patterns[p]->ignoreFlag && matchPatternAgainstSentence(patterns[p], s, true))
		{
#ifdef LOG_IPATTERN
			lplog(L"%c:Pattern %03d %s[%s] matched against sentence %04d.", 'A' + pass, p, patterns[p]->name.c_str(), patterns[p]->differentiator.c_str(), s);
#endif
			numPatternsMatched++;
		}
	}
	return numPatternsMatched;
}
#endif

void cSource::setForms(void)
{
	LFS
		m[0].forms.check(L"forms", Forms.size());
	m[0].patterns.check(L"patterns", patterns.size());
	for (vector <cWordMatch>::iterator I = m.begin(), IEnd = m.end(); I != IEnd; I++) I->setForm();
}

void cSource::clearSource(void)
{
	LFS
		storageLocation.clear();
	relatedObjectsMap.clear();
	for (unsigned int I = 0; I < m.size(); I++)
		m[I].pma.clear();
	pema.clear();
	m.clear();
	subNarratives.clear();
	sentenceStarts.erase();
	primaryQuoteType = L"\"";
	secondaryQuoteType = L"\'";
	printMaxSize.clear();
	whatMatched.clear();
	objects.clear();
	localObjects.clear();
	unresolvedSpeakers.clear();
	lastPEMAConsolidationIndex = 1; // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet = -1;
	primaryLocationLastMovingPosition = -1;
	primaryLocationLastPosition = -1;
	firstQuote = lastQuote = -1;
	gTraceSource = 0;
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
	syntacticRelationGroups.clear();
	timelineSegments.clear();
	for (int objectLastTense = 0; objectLastTense < VERB_HISTORY; objectLastTense++)
		lastVerbTenses[objectLastTense].clear();
	clearWNMaps();
	if (updateWordUsageCostsDynamically)
		cWord::resetUsagePatternsAndCosts(debugTrace);
	else
		cWord::resetCapitalizationAndProperNounUsageStatistics(debugTrace);
}

int read(string& str, IOHANDLE file)
{
	LFS
		int size;
	_read(file, &size, sizeof(size));
	if (size < 1023)
	{
		char contents[1024];
		_read(file, contents, size);
		contents[size] = 0;
		if (size) str = contents;
	}
	else
	{
		char* contents = (char*)tmalloc(size + 4);
		_read(file, contents, size);
		contents[size] = 0;
		str = contents;
		free(contents);
	}
	return 0;
}

#define MAX_BUF (10*1024*1024)
bool cSource::flush(int fd, void* buffer, int& where)
{
	LFS
		if (where > MAX_BUF - 64 * 1024)
		{
			if (::write(fd, buffer, where) < 0) return false;
			where = 0;
		}
	return true;
}

bool cSource::FlushFile(HANDLE fd, void* buffer, int& where)
{
	LFS
		if (where > MAX_BUF - 64 * 1024)
		{
			DWORD bytesWritten;
			if (!WriteFile(fd, buffer, where, &bytesWritten, NULL) || bytesWritten != where)
				return false;
			where = 0;
		}
	return true;
}

bool cSource::writeCheck(wstring path)
{
	LFS
		path += L".SourceCache";
	//return _waccess(path.c_str(),0)==0; // long path limitation
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool cSource::writePatternUsage(wstring path, bool zeroOutPatternUsage)
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
bool cSource::write(wstring path, bool S2, bool makeCopyBeforeSourceWrite, wstring specialExtension)
{
	LFS
		int sanityReturnCode = 0, generalizedIndex = 0;
	if (sanityReturnCode = sanityCheck(generalizedIndex))
	{
		wprintf(L"PROGRESS: source sanity check fail (%d@%d) with %d seconds elapsed \n", sanityReturnCode, generalizedIndex, clocksec());
		lplog(LOG_ERROR, L"Sanity check failed (%d@%d): source %s!", sanityReturnCode, generalizedIndex, sourcePath.c_str());
		return false;
	}
	path += L".SourceCache" + specialExtension;
	// int fd=_wopen(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD | _S_IWRITE); subject to short path restriction
	if (makeCopyBeforeSourceWrite)
	{
		wstring renamePath = path + L".old";
		if (_wremove(renamePath.c_str()) < 0 && errno != ENOENT)
			lplog(LOG_FATAL_ERROR, L"REMOVE %s - %S", renamePath.c_str(), sys_errlist[errno]);
		else if (_wrename(path.c_str(), renamePath.c_str()) && errno != ENOENT)
			lplog(LOG_ERROR, L"RENAME %s to %s - %S", path.c_str(), renamePath.c_str(), sys_errlist[errno]);
	}
	HANDLE fd = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (fd == INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR, L"Unable to open source %s - %s", path.c_str(), lastErrorMsg().c_str());
		return false;
	}
	char buffer[MAX_BUF];
	int where = 0;
	DWORD dwBytesWritten;
	int sourceVersion = SOURCE_VERSION;
	if (!copy(buffer, sourceVersion, where, MAX_BUF)) return false;
	if (!copy(buffer, storageLocation, where, MAX_BUF)) return false;
	unsigned int count = m.size();
	if (!copy((void*)buffer, count, where, MAX_BUF)) return false;
	vector <cWordMatch>::iterator im = m.begin(), mend = m.end();
	for (int I = 0; im != mend; im++, I++)
	{
		getBaseVerb(I, 14, im->baseVerb);
		if (!im->writeRef(buffer, where, MAX_BUF)) return false;
		if (!FlushFile(fd, buffer, where)) return false;
	}
	if (where)
	{
		if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
			return false;
		where = 0;
	}
	if (!sentenceStarts.write(buffer, where, MAX_BUF)) return false;
	if (where >= MAX_BUF - 1024)
		return false;
	count = sections.size();
	if (!copy((void*)buffer, count, where, MAX_BUF)) return false;
	for (unsigned int I = 0; I < count; I++)
	{
		if (!sections[I].write(buffer, where, MAX_BUF)) return false;
		if (!FlushFile(fd, buffer, where)) return false;
	}
	count = speakerGroups.size();
	if (!copy((void*)buffer, count, where, MAX_BUF)) return false;
	for (unsigned int I = 0; I < count; I++)
	{
		if (!speakerGroups[I].copy(buffer, where, MAX_BUF)) return false;
		if (!FlushFile(fd, buffer, where)) return false;
	}
	if (where)
	{
		if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
			return false;
		where = 0;
	}
	pema.WriteFile(fd);
	count = objects.size();
	if (!copy((void*)buffer, count, where, MAX_BUF)) return false;
	for (vector <cObject>::iterator o = objects.begin(), oEnd = objects.end(); o != oEnd; o++)
	{
		o->write(buffer, where, MAX_BUF);
		if (!FlushFile(fd, buffer, where)) return false;
	}
	if (where)
	{
		if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
			return false;
		where = 0;
	}
	if (S2)
	{
		copy(buffer, (int)syntacticRelationGroups.size(), where, MAX_BUF);
		for (vector <cSyntacticRelationGroup>::iterator srg = syntacticRelationGroups.begin(), sriEnd = syntacticRelationGroups.end(); srg != sriEnd; srg++)
		{
			srg->nextSPR = (int)(srg - syntacticRelationGroups.begin());
			srToText(srg->nextSPR, srg->description);
			if (!srg->write(buffer, where, MAX_BUF)) break;
			if (!FlushFile(fd, buffer, where)) return false;
		}
		count = timelineSegments.size();
		if (!copy((void*)buffer, count, where, MAX_BUF)) return false;
		for (vector <cTimelineSegment>::iterator tl = timelineSegments.begin(), tlEnd = timelineSegments.end(); tl != tlEnd; tl++)
		{
			if (!tl->copy(buffer, where, MAX_BUF)) return false;
			if (!FlushFile(fd, buffer, where)) return false;
		}
		if (where)
		{
			if (!WriteFile(fd, buffer, where, &dwBytesWritten, NULL) || where != dwBytesWritten)
				return false;
			where = 0;
		}
	}
	CloseHandle(fd);
	return true;
}

// concentrates on index underflow and overflow
int cSource::sanityCheck(int& generalizedIndex)
{
	generalizedIndex = 0;
	for (auto& mi : m)
	{
		if (mi.beginPEMAPosition < -1 || mi.beginPEMAPosition >= (signed)pema.count) return 1;
		if (mi.endPEMAPosition < -1 || mi.endPEMAPosition >= (signed)pema.count) return 2;
		for (unsigned int pmi = 0; pmi < mi.pma.count; pmi++)
		{
			if (generalizedIndex + mi.pma[pmi].len > m.size()) return 3; // the end of a pm may be = m.size()
			if (mi.pma[pmi].pemaByPatternEnd < -1 || mi.pma[pmi].pemaByPatternEnd >= (signed)pema.count) return 4;
			if (mi.pma[pmi].pemaByChildPatternEnd < -1 || mi.pma[pmi].pemaByChildPatternEnd >= (signed)pema.count) return 5;
			if (mi.pma[pmi].getPattern() >= patterns.size()) return 6;
		}
		// enum eOBJECTS { UNKNOWN_OBJECT=-1,OBJECT_UNKNOWN_MALE=-2, OBJECT_UNKNOWN_FEMALE=-3, OBJECT_UNKNOWN_MALE_OR_FEMALE = -4, OBJECT_UNKNOWN_NEUTER = -5, OBJECT_UNKNOWN_PLURAL = -6, OBJECT_UNKNOWN_ALL = -7
		if (mi.getObject() < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || mi.getObject() >= (int)objects.size())
		{
			lplog(LOG_ERROR, L"Sanity check failure for source position %d.  object=%d out of %d objects.", generalizedIndex, mi.getObject(), objects.size());
			return 7;
		}
		if (mi.principalWherePosition < -1 || mi.principalWherePosition >= (int)m.size()) return 8;
		if (mi.principalWhereAdjectivalPosition < -1 || (int)mi.principalWhereAdjectivalPosition >= (int)m.size()) return 9;
		if (mi.originalObject < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || mi.originalObject >= (int)objects.size()) return 10;
		for (auto& om : mi.objectMatches)
			if (om.object < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || om.object >= (int)objects.size()) return 11;
		for (auto& om : mi.audienceObjectMatches)
			if (om.object < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || om.object >= (int)objects.size()) return 12;
		if (mi.isQuote() && (mi.getQuoteForwardLink() < -1 || mi.getQuoteForwardLink() >= (int)m.size()))
			return 13;
		if (mi.endQuote < -1 || mi.endQuote >= (int)m.size()) return 14;
		if (mi.nextQuote < -1 || mi.nextQuote >= (int)m.size()) return 15;
		if (mi.previousQuote < -1 || mi.previousQuote >= (int)m.size()) return 16;
		if (mi.getRelObject() < -1 || mi.getRelObject() >= (int)m.size()) return 17;
		if (mi.relSubject < -1 || mi.relSubject >= (int)m.size()) return 18;
		if (mi.getRelVerb() < -1 || mi.getRelVerb() >= (int)m.size()) return 19;
		if (mi.relPrep < -1 || mi.relPrep >= (int)m.size()) return 20;
		if (mi.beginObjectPosition < -1 || mi.beginObjectPosition >= (int)m.size()) return 21;
		if (mi.endObjectPosition < -1 || mi.endObjectPosition >(int)m.size()) return 22; // end position can be = m.size()
		if (mi.embeddedStorySpeakerPosition < -1 || mi.embeddedStorySpeakerPosition >= (int)m.size()) return 23;
		if (mi.relNextObject < -1 || mi.relNextObject >= (int)m.size()) return 24;
		if (mi.nextCompoundPartObject < -1 || mi.nextCompoundPartObject >= (int)m.size()) return 25;
		if (mi.previousCompoundPartObject < -1 || mi.previousCompoundPartObject >= (int)m.size()) return 26;
		if (mi.relInternalVerb < -1 || mi.relInternalVerb >= (int)m.size()) return 27;
		if (mi.relInternalObject < -1 || mi.relInternalObject >= (int)m.size()) return 28;
		if (mi.quoteBackLink < -1 || mi.quoteBackLink >= (int)m.size()) return 30;
		if (mi.speakerPosition < -1 || mi.speakerPosition >= (int)m.size()) return 31;
		if (mi.audiencePosition < -1 || mi.audiencePosition >= (int)m.size()) return 32;
		if (mi.getObject() >= 0 && (mi.beginObjectPosition < 0 || mi.endObjectPosition < 0)) return 33;
		for (int np = mi.beginPEMAPosition; np >= 0; np = pema[np].nextByPosition)
			if (np < 0 || np >= (signed)pema.count) return 50;
			else
			{
				if (pema[np].getParentPattern() >= patterns.size()) return 201;
				if (pema[np].isChildPattern())
				{
					if (pema[np].getChildPattern() >= patterns.size()) return 202;
					if (generalizedIndex + pema[np].getChildLen() > m.size()) return 203; // may = m.size()
				}
				else
					if (pema[np].getChildForm() >= Forms.size())
						return 204;
			}
		for (int np = mi.beginPEMAPosition; np >= 0; np = pema[np].nextByPatternEnd)
			if (np < 0 || np >= (signed)pema.count) return 51;
			else
			{
				if (pema[np].getParentPattern() >= patterns.size()) return 205;
				if (pema[np].isChildPattern())
				{
					if (pema[np].getChildPattern() >= patterns.size()) return 206;
					if (generalizedIndex + pema[np].getChildLen() > m.size()) return 207; // may = m.size()
				}
				else
					if (pema[np].getChildForm() >= Forms.size())
						return 208;
			}
		for (int np = mi.beginPEMAPosition; np >= 0; np = pema[np].nextByChildPatternEnd)
			if (np < 0 || np >= (signed)pema.count) return 52;
			else
			{
				if (pema[np].getParentPattern() >= patterns.size()) return 209;
				if (pema[np].isChildPattern())
				{
					if (pema[np].getChildPattern() >= patterns.size()) return 210;
					if (generalizedIndex + pema[np].getChildLen() > m.size()) return 211; // may = m.size()
				}
				else
					if (pema[np].getChildForm() >= Forms.size())
						return 212;
			}
		for (int np = mi.beginPEMAPosition; np >= 0; np = pema[np].nextPatternElement)
			if (np < 0 || np >= (signed)pema.count) return 53;
			else
			{
				if (pema[np].getParentPattern() >= patterns.size()) return 213;
				if (pema[np].isChildPattern())
				{
					if (pema[np].getChildPattern() >= patterns.size()) return 214;
					if (generalizedIndex + pema[np].getChildLen() > m.size()) return 215; // may = m.size()
				}
				else
					if (pema[np].getChildForm() >= Forms.size())
						return 216;
			}
		generalizedIndex++;
	}
	generalizedIndex = 0;
	for (unsigned int si = 0; si < sentenceStarts.size(); si++, generalizedIndex++)
		if (sentenceStarts[si] < 0 || sentenceStarts[si] > (int)m.size()) return 33; // a sentence start may be = m.size()
	generalizedIndex = 0;
	for (unsigned int I = 0; I < sections.size(); I++, generalizedIndex++)
	{
		if (sections[I].begin >= (int)m.size())
			return 34;
		if (sections[I].endHeader > (int)m.size()) return 35; // endHeader of last section may = m.size()
		for (unsigned int shi = 0; shi < sections[I].subHeadings.size(); shi++)
			if (sections[I].subHeadings[shi] < 0 || sections[I].subHeadings[shi] >= (int)m.size()) return 36;
		for (unsigned int dfoi = 0; dfoi < sections[I].definiteSpeakerObjects.size(); dfoi++)
			if (sections[I].definiteSpeakerObjects[dfoi].object < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || sections[I].definiteSpeakerObjects[dfoi].object >= (int)objects.size()) return 37;
		for (unsigned int dfoi = 0; dfoi < sections[I].speakerObjects.size(); dfoi++)
			if (sections[I].speakerObjects[dfoi].object < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || sections[I].speakerObjects[dfoi].object >= (int)objects.size()) return 38;
		for (unsigned int dfoi = 0; dfoi < sections[I].objectsSpokenAbout.size(); dfoi++)
			if (sections[I].objectsSpokenAbout[dfoi].object < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || sections[I].objectsSpokenAbout[dfoi].object >= (int)objects.size()) return 39;
		for (unsigned int dfoi = 0; dfoi < sections[I].objectsInNarration.size(); dfoi++)
			if (sections[I].objectsInNarration[dfoi].object < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || sections[I].objectsInNarration[dfoi].object >= (int)objects.size()) return 40;
	}
	generalizedIndex = 0;
	int returnSanity = 0;
	for (auto& sgi : speakerGroups)
	{
		if (returnSanity = sgi.sanityCheck(m.size(), sections.size(), objects.size(), (int)speakerGroups.size())) return returnSanity;
		generalizedIndex++;
	}
	generalizedIndex = 0;
	for (auto& o : objects)
	{
		if (returnSanity = o.sanityCheck(m.size(), objects.size(), speakerGroups.size(), m)) return returnSanity;
		generalizedIndex++;
	}
	generalizedIndex = 0;
	for (auto& srg : syntacticRelationGroups)
	{
		if (returnSanity = srg.sanityCheck(m.size(), objects.size())) return returnSanity;
		generalizedIndex++;
	}
	return 0;
}

bool cSource::read(char* buffer, int& where, unsigned int total, bool& parsedOnly, bool printProgress, wstring specialExtension)
{
	LFS
		int sourceVersion;
	if (!copy(sourceVersion, buffer, where, total)) return false;
	if (sourceVersion != SOURCE_VERSION)
	{
		lplog(LOG_WHERE, L"%s rejected as old.  Reparsing.", sourcePath.c_str());
		return false;
	}
	if (!copy(storageLocation, buffer, where, total)) return false;
	unsigned int count;
	unsigned int lastProgressPercent = 0;
	if (!copy(count, buffer, where, total)) return false;
	if (!count) return true;
	m.reserve(m.size() + count);
	bool error = false;
	while (count-- && !error && where < (signed)total)
	{
		LFSL
			if ((where * 100 / total) > lastProgressPercent && printProgress)
				wprintf(L"PROGRESS: %03d%% source read with %d seconds elapsed \r", lastProgressPercent = where * 100 / total, clocksec());
		m.emplace_back(buffer, where, total, sourceType, error);
	}
	if (error || where >= (signed)total)
	{
		lplog(LOG_ERROR, L"%s read error at position %d, read buffer location %d (out of %d) - error %d.", sourcePath.c_str(), m.size(), where, total, error);
		return false;
	}
	sentenceStarts.read(buffer, where, total);
	if ((where * 100 / total) > lastProgressPercent && printProgress)
		wprintf(L"PROGRESS: %03d%% source read with %d seconds elapsed \r", lastProgressPercent = where * 100 / total, clocksec());
	if (!copy(count, buffer, where, total)) return false;
	for (unsigned int I = 0; I < count && !error; I++)
		sections.push_back(cSection(buffer, where, total, error));
	if ((where * 100 / total) > lastProgressPercent && printProgress)
		wprintf(L"PROGRESS: %03d%% source read with %d seconds elapsed \r", lastProgressPercent = where * 100 / total, clocksec());
	if (!copy(count, buffer, where, total)) return false;
	for (unsigned int I = 0; I < count && !error; I++)
		speakerGroups.push_back(cSpeakerGroup(buffer, where, total, error));
	if (error || !pema.read(buffer, where, total)) return false;
	if (where == total)
	{
		parsedOnly = where == total;
		if (printProgress)
			wprintf(L"PROGRESS: 100%% source read with %d seconds elapsed \n", clocksec());
		return true;
	}
	if (!copy(count, buffer, where, total)) return false;
	for (unsigned int I = 0; I < count && !error; I++)
		objects.push_back(cObject(buffer, where, total, error));
	if (!copy(count, buffer, where, total)) return false;
	for (unsigned int I = 0; I < count && !error; I++)
	{
		syntacticRelationGroups.push_back(cSyntacticRelationGroup(buffer, where, total, error));
		getSRIMinMax(&syntacticRelationGroups[syntacticRelationGroups.size() - 1]);
	}
	if (!copy(count, buffer, where, total)) return false;
	for (unsigned int I = 0; I < count && !error; I++)
		timelineSegments.push_back(cTimelineSegment(buffer, where, total, error));
	// fill locations
	unsigned int I = 0;
	for (vector <cWordMatch>::iterator im = m.begin(), imEnd = m.end(); im != imEnd; im++, I++)
	{
		if (im->getObject() >= 0 && im->getObject() < (signed)objects.size())
			objects[im->getObject()].locations.push_back(cObject::cLocation(I));
		for (vector <cOM>::iterator omi = im->objectMatches.begin(), omiEnd = im->objectMatches.end(); omi != omiEnd; omi++)
			if (omi->object >= 0 && omi->object < (signed)objects.size())
				objects[omi->object].locations.push_back(cObject::cLocation(I));
	}
	if (printProgress)
		wprintf(L"PROGRESS: 100%% source read with %d seconds elapsed \n", clocksec());
	return !error;
}

// find the first title that is followed by two consecutive sentences in a paragraph.
// *** END OF THE PROJECT 
const wchar_t* ignoreWords[] = { L"GUTENBERG",L"COPYRIGHT",L"THIS HEADER SHOULD BE THE FIRST THING",L"THE WORLD OF FREE PLAIN VANILLA ELECTRONIC TEXTS",L"EBOOKS ",L"ETEXT",
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
L"MAC USERS, DO NOT POINT AND CLICK",NULL };

//end of line is \r (13) then \n (10)
bool getNextLine(int& where, wstring& buffer, wstring& line, bool& eofEncountered, int& firstNonBlank)
{
	LFS
		size_t nextLineStart = buffer.find(L'\n', where);
	if (nextLineStart == wstring::npos) eofEncountered = true;
	nextLineStart++;
	//if (buffer[nextLineStart]==L'\n') nextLineStart++;
	line = buffer.substr(where, nextLineStart - where);
	where = nextLineStart;
	firstNonBlank = 0;
	while (firstNonBlank < (signed)line.length() && iswspace(line[firstNonBlank]))
		firstNonBlank++;
	return firstNonBlank < (signed)line.length();
}

bool containsSectionHeader(wstring& line, int firstNonBlank)
{
	LFS
		bool csh = false;
	if (line.length() > 0)
	{
		wstring upline = line;
		wcsupr((wchar_t*)upline.c_str());
		const wchar_t* sectionheader[] = { L"BOOK",L"CHAPTER",L"PART",L"PROLOGUE",L"EPILOGUE",L"VOLUME",L"STAVE",L"I.",NULL };
		for (int I = 0; sectionheader[I]; I++)
		{
			if (csh = (!wcsncmp(upline.c_str() + firstNonBlank, sectionheader[I], wcslen(sectionheader[I]))))
				break;
		}
	}
	return csh;
}

bool containsSectionHeader(wstring& line)
{
	LFS
		int fnb = 0;
	while (fnb < (signed)line.length() && iswspace(line[fnb]))
		fnb++;
	return containsSectionHeader(line, fnb);
}

bool containsAttribution(wstring& line, int firstNonBlank)
{
	LFS
		bool ca = false;
	if (line.length() > 0)
	{
		wstring upline = line;
		wcsupr((wchar_t*)upline.c_str());
		const wchar_t* attribution[] = { L"SCANNED BY",L"PRODUCED BY",L"WRITTEN BY",L"TRANSCRIBED BY",L"PRODUCED BY",L"PREFACE BY",
			L"TRANSCRIBER'S NOTE",L"AUTHOR'S NOTE",L"WITH ILLUSTRATIONS BY",L"DEDICATION",L"AUTHOR'S INTRODUCTION",L"PREFATORY NOTE",L"TO MY READERS",
			L"ADDRESSED TO THE READER",NULL };
		for (int I = 0; attribution[I]; I++)
		{
			if (ca = (!wcsncmp(upline.c_str() + firstNonBlank, attribution[I], wcslen(attribution[I]))))
				break;
		}
	}
	return ca;
}

bool isRomanNumeral(wstring line, int where)
{
	LFS
		bool matches = false;
	extern const wchar_t* roman_numeral[];
	wstring rn;
	while (where < (signed)line.length() && line[where] != L'.' && !iswspace(line[where]))
		rn += towlower(line[where++]);
	for (int I = 0; roman_numeral[I]; I++)
		if (matches = rn == roman_numeral[I])
			break;
	return matches;
}

bool getNextParagraph(int& where, wstring& buffer, bool& eofEncountered, int& firstNonBlank, wstring& paragraph, int& numLines, int& numIndentedLines, int& numHeaderLines, int& numStartLines, int& numShortLines)
{
	LFS
		int fnb = 0;
	while (where < (signed)buffer.length() && iswspace(buffer[where]))
		where++;
	wstring line;
	while (getNextLine(where, buffer, line, eofEncountered, fnb) && !eofEncountered)
	{
		if (numLines == 0) firstNonBlank = fnb;
		if (fnb > 0) numIndentedLines++;
		if (containsSectionHeader(line, fnb)) numHeaderLines++;
		if (line.length() < 40) numShortLines++;
		if (iswdigit(line[fnb]) || (isRomanNumeral(line, fnb) && wcsncmp(line.c_str() + fnb, L"I ", 2)))
		{
			numStartLines++;
			if (_wtoi((wchar_t*)line.c_str()) == 1)
			{
				size_t whereSeparator = line.find(L'.');
				if (whereSeparator == wstring::npos)
					whereSeparator = line.find(L'-');
				if (whereSeparator == wstring::npos)
					whereSeparator = line.find(L'—');
				if (whereSeparator != wstring::npos)
				{
					whereSeparator++;
					while (whereSeparator < (int)line.length() && iswspace(line[whereSeparator]))
						whereSeparator++;
					wstring firstTitle = line.substr(whereSeparator, line.length());
					if ((whereSeparator = buffer.find(firstTitle, where)) != wstring::npos && whereSeparator < (int)buffer.length() / 5)
					{
						// go to beginning of line
						while (whereSeparator > 0 && buffer[whereSeparator - 1] != L'\n')
							whereSeparator--;
						if (whereSeparator > 0)
						{
							paragraph.clear();
							where = whereSeparator;
							numLines = numIndentedLines = numStartLines = numHeaderLines = 0;
							continue;
						}
					}
				}
			}
		}
		paragraph += line;
		numLines++;
	}
	return !paragraph.empty();
}

bool ci_equal(wchar_t ch1, wchar_t ch2)
{
	LFS
		return towupper(ch1) == towupper(ch2);
}

size_t noCasefind(const wstring& str1, const wstring& str2, int position)
{
	LFS
		wstring::const_iterator pos = search(str1.begin() + position, str1.end(), str2.begin(), str2.end(), ci_equal);
	if (pos == str1.end())
		return wstring::npos;
	else
		return pos - str1.begin();
}

// get next line, accumulating lines until a blank line
// if contains ignoreWord, set lastLine to '' and go to beginning
// if there is no period, save in lastLine and goto beginning.
// otherwise return true.
bool findNextParagraph(int& where, wstring& buffer, wstring& lastLine, int& whereLastLine)
{
	LFS
		wstring paragraph;
	bool alreadySkipped = false;
	int lastNumLines = 0, lastShortLines = 0;
	while (true)
	{
		paragraph.clear();
		whereLastLine = where;
		int indentedLines = 0, numLines = 0, headerLines = 0, numStartLines = 0, firstNonBlank = 0, numShortLines = 0;
		bool eofEncountered = false;
		getNextParagraph(where, buffer, eofEncountered, firstNonBlank, paragraph, numLines, indentedLines, headerLines, numStartLines, numShortLines);
		if (eofEncountered) return false;
		if (paragraph.empty()) continue;
		if (indentedLines > 1 || headerLines > 1 || numStartLines > 1) continue; // index or contents
		wstring saveParagraph = paragraph;
		wcsupr((wchar_t*)paragraph.c_str());
		bool containsIgnoreSequence = false;
		for (int I = 0; ignoreWords[I]; I++)
		{
			if (containsIgnoreSequence = (paragraph.find(ignoreWords[I]) != wstring::npos))
				break;
		}
		if (containsIgnoreSequence) continue;
		if (!wcsncmp(paragraph.c_str() + firstNonBlank, L"AUTHOR OF", wcslen(L"AUTHOR OF")))
			continue;
		if (!wcsncmp(paragraph.c_str() + firstNonBlank, L"AUTHORS OF", wcslen(L"AUTHORS OF")))
			continue;
		while (iswspace(saveParagraph[saveParagraph.length() - 1]))
			saveParagraph.erase(saveParagraph.length() - 1);
		// contains at least two periods?  if so, break.
		int numPeriods = 0;
		for (int p = 3; p < (signed)saveParagraph.length(); p++)
		{
			if (saveParagraph[p] == L'.' && saveParagraph[p - 2] != L'.' && saveParagraph[p - 3] != L'.' && (!iswspace(saveParagraph[p - 2]) || iswspace(saveParagraph[p + 1])))
				numPeriods++;
			if (saveParagraph[p] == L'?' || saveParagraph[p] == L'!')
				numPeriods++;
		}
		bool endsWithEOS = saveParagraph[saveParagraph.length() - 1] == L'.' || saveParagraph[saveParagraph.length() - 1] == L'?' || saveParagraph[saveParagraph.length() - 1] == L'!' ||
			cWord::isSingleQuote(saveParagraph[saveParagraph.length() - 1]) || cWord::isDoubleQuote(saveParagraph[saveParagraph.length() - 1]) ||
			cWord::isDash(saveParagraph[saveParagraph.length() - 1]);
		bool ccsh = containsSectionHeader(paragraph);
		if (numPeriods < 2 || !endsWithEOS || saveParagraph == paragraph || lastLine.empty() || ccsh)
		{
			// contains at least two lines, and there is only one period at the end of the line
			if (numLines > 1 && endsWithEOS && headerLines <= 1 && !lastLine.empty() && lastLine.length() < 255 && !containsAttribution(lastLine, firstNonBlank))
				break;
			bool csh = containsSectionHeader(lastLine);
			// if contains only one line, and it has a period at the end, and the previous lastLine contains a section header
			if (csh && endsWithEOS && !ccsh)
				break;
			// if the lastLine doesn't contain a section header
			if (!csh || alreadySkipped || ccsh)
			{
				alreadySkipped = false;
				lastLine = saveParagraph;
				lastNumLines = numLines;
				lastShortLines = numShortLines;
				whereLastLine = where;
			}
			else
				alreadySkipped = true;
			continue;
		}
		if (headerLines == 1 || numStartLines == 1)
		{
			// check if the next paragraph also has a header.  Then if it does, reject the whole thing.
			int w2 = where;
			whereLastLine = where;
			bool csh = containsSectionHeader(paragraph, firstNonBlank);
			bool ld = !csh && iswdigit(paragraph[firstNonBlank]);
			bool rn = !csh && isRomanNumeral(paragraph, firstNonBlank);
			if (rn && paragraph[0] == L'I' && paragraph[1] == L' ' && iswalpha(paragraph[2]))
				rn = false;
			indentedLines = numLines = headerLines = numStartLines = numShortLines = firstNonBlank = 0;
			bool contents = false;
			paragraph.clear();
			while (getNextParagraph(w2, buffer, eofEncountered, firstNonBlank, paragraph, numLines, indentedLines, headerLines, numStartLines, numShortLines) &&
				((csh && containsSectionHeader(paragraph, firstNonBlank)) || (ld && iswdigit(paragraph[firstNonBlank])) || (rn && isRomanNumeral(paragraph, firstNonBlank))))
			{
				if (paragraph.find(L"CHAPTER I.") != wstring::npos || paragraph.find(saveParagraph) != wstring::npos) break;
				paragraph.clear();
				lastLine.clear();
				indentedLines = numLines = headerLines = numStartLines = numShortLines = firstNonBlank = 0;
				contents = true;
			}
			if (contents)
			{
				where = w2;
				lastLine = paragraph;
				lastNumLines = numLines;
				lastShortLines = numShortLines;
				continue;
			}
		}
		if (lastNumLines > 2 && lastShortLines <= 1)
		{
			lplog(LOG_INFO, L"Rejecting %s.", lastLine.c_str());
			continue;
		}
		if (containsAttribution(lastLine, firstNonBlank)) continue;
		if (lastLine.length() > 255) continue;
		if (noCasefind(lastLine, L"preface", 0) != wstring::npos)
		{
			size_t w3;
			wchar_t* startScanningPosition;
			if ((w3 = noCasefind(buffer, L"contents", where)) != wstring::npos &&
				aloneOnLine((wchar_t*)buffer.c_str(), (wchar_t*)buffer.c_str() + w3, L"contents", startScanningPosition, buffer.length()) &&
				w3 < (int)buffer.length() / 10)
			{
				where = w3;
				continue;
			}
		}
		break;
	}
	return true;
}

// You can use the predicate version of std::search
bool testStart(wstring& buffer, const wchar_t* str, int& where, bool checkAlone = true)
{
	LFS
		int wt;
	wchar_t* startScanningPosition;
	if (((wt = buffer.find(str, where)) != wstring::npos) && wt < 20000 &&
		aloneOnLine((wchar_t*)buffer.c_str(), (wchar_t*)buffer.c_str() + wt, str, startScanningPosition, buffer.length(), !checkAlone))
	{
		where = wt;
		return true;
	}
	return false;
}

bool cSource::findStart(wstring& buffer, wstring& start, int& repeatStart, wstring& title)
{
	LFS
		// go to the first paragraph without any ignoreWords, with at least two consecutive sentences (periods).
		// then go to the previous title, if it doesn't have any ignoreWords.  If the start occurs before, count repeatStart.
		int where = 0, whereLastLine = -1, wt = 0;
	bufferLen = buffer.length();
	testStart(buffer, L"*END THE SMALL PRINT", where, false);
	testStart(buffer, L"*END*THE SMALL PRINT", where, false);
	testStart(buffer, L"START OF THE PROJECT GUTENBERG EBOOK", where, false);
	testStart(buffer, L"START OF THIS PROJECT GUTENBERG EBOOK", where, false);
	testStart(buffer, L"CONTENTS", where, false);
	if (!testStart(buffer, L"PROLOGUE", where) &&
		!testStart(buffer, L"CHAPTER 1", where) &&
		!testStart(buffer, L"CHAPTER I", where) && // to prevent CHAPTER IV to be found after CHAPTER 1
		!testStart(buffer, L"CHAPTER I-", where, false) &&
		!testStart(buffer, L"CHAPTER I.", where, false) &&
		!testStart(buffer, L"CHAPTER I:", where, false) &&
		!testStart(buffer, L"CHAPTER ONE", where))
		testStart(buffer, L"CHAPTER ONE.", where, false);
	wchar_t* startScanningPosition;
	while (((wt = noCasefind(buffer, title, wt + 1)) != wstring::npos) && wt < (int)buffer.length() / 10 &&
		aloneOnLine((wchar_t*)buffer.c_str(), (wchar_t*)buffer.c_str() + wt, title.c_str(), startScanningPosition, bufferLen))
		where = wt;
	wstring lastLine;
	if (!findNextParagraph(where, buffer, lastLine, whereLastLine))
	{
		start = L"**START NOT FOUND**";
		return false;
	}
	// previous non-blank line is lastLine.
	int wll = 0;
	repeatStart = 0;
	while (wll >= 0 && wll < where)
	{
		wll = buffer.find(lastLine, wll + 1);
		if (wll < 0 || (wll > where && (wll * 100 / bufferLen) > 10)) break;
		if (aloneOnLine((wchar_t*)buffer.c_str(), (wchar_t*)buffer.c_str() + wll, lastLine.c_str(), startScanningPosition, bufferLen))
			repeatStart++;
	}
	start = lastLine;
	return true;
}

bool readWikiPage(wstring webAddress, wstring& buffer)
{
	LFS
		int ret;
	if (ret = cInternet::readPage(webAddress.c_str(), buffer)) return false;
	if (buffer.find(L"Sorry, but the page or book you tried to access is unavailable") != wstring::npos ||
		buffer.find(L"<title>403 Forbidden</title>") != wstring::npos ||
		buffer.find(L"<h1>404 Not Found</h1>") != wstring::npos ||
		buffer.empty())
	{
		buffer.clear();
		return false;
	}
	return true;
}

void unescapeStr(wstring& str)
{
	LFS
		wstring ess;
	for (unsigned int I = 0; I < str.length(); I++)
	{
		if (str[I] == '\\' && (str[I + 1] == '\'' || str[I + 1] == '\\')) I++;
		ess += str[I];
	}
	str = ess;
}

bool cSource::readSource(wstring& path, bool checkOnly, bool& parsedOnly, bool printProgress, wstring specialExtension)
{
	LFS
		//unescapeStr(path); // doesn't work on 'Twixt Land & Sea: Tales
		wstring locationCache = path + L".SourceCache" + specialExtension;
	if (checkOnly)
		//return _waccess(locationCache.c_str(),0)==0; // long path limitation
		return GetFileAttributesW(locationCache.c_str()) != INVALID_FILE_ATTRIBUTES;
	//lplog(LOG_WHERE, L"TRACEOPEN %s %s", path.c_str(), __FUNCTIONW__);
	// IOHANDLE fd = _wopen(locationCache.c_str(), O_RDWR | O_BINARY);   // MAX_PATH limitation
	HANDLE fd = CreateFile(locationCache.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	///if (fd<0) return false;
	if (fd == INVALID_HANDLE_VALUE)
	{
		lplog(LOG_ERROR, L"Unable to open source %s - %s", locationCache.c_str(), lastErrorMsg().c_str());
		CloseHandle(fd);
		return false;
	}
	void* buffer;
	//int bufferlen=filelength(fd);
	DWORD bufferlen;
	if ((bufferlen = GetFileSize(fd, NULL)) == INVALID_FILE_SIZE)
	{
		lplog(LOG_ERROR, L"Could not get length of file %s.", path.c_str());
		CloseHandle(fd);
		return false;
	}
	// if unable to parse previously, the source is saved as an empty file.
	if (!bufferlen)
	{
		lplog(LOG_INFO, L"empty file %s - previous attempt at parsing failed so not retrying.", path.c_str());
		CloseHandle(fd);
		return true;
	}
	buffer = (void*)tmalloc(bufferlen + 10);
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
	int where = 0;
	sourcePath = path;
	bool success = read((char*)buffer, where, bufferlen, parsedOnly, printProgress, specialExtension);
	if (!success)
		lplog(LOG_ERROR, L"Error while reading file %s at position %d.", path.c_str(), m.size());
	tfree(bufferlen, buffer);
	if (!success)
	{
		clearSource();
		if (updateWordUsageCostsDynamically)
			cWord::resetUsagePatternsAndCosts(debugTrace);
		else
			cWord::resetCapitalizationAndProperNounUsageStatistics(debugTrace);
	}
	return success;
}

// Updates costs of parent patterns when a child PMA element has been reduced after parent PEMA costs have been calculated.
int cSource::updatePEMACosts(int PEMAPosition, int pattern, int begin, int end, int position,
	vector<cPatternElementMatchArray::tPatternElementMatch*>& ppema)
{
	LFS
#ifdef LOG_PATTERN_COST_CHECK
		int scanPP = PEMAPosition;
	for (cPatternElementMatchArray::tPatternElementMatch* pem = pema.begin() + scanPP;
		scanPP >= 0; scanPP = pem->nextByPatternEnd, pem = pema.begin() + scanPP)
		if (pem->isChildPattern())
			lplog(L"%d:RP PEMA SCAN %s[%s](%d,%d) child %s[*](%d,%d)",
				position, patterns[pem->getPattern()]->name.c_str(), patterns[pem->getPattern()]->differentiator.c_str(), position + pem->begin, position + pem->end,
				patterns[pem->getChildPattern()]->name.c_str(), position, position + pem->getChildLen());
		else
			lplog(L"%d:RP PEMA %s[%s](%d,%d) child form %s",
				position, patterns[pem->getPattern()]->name.c_str(), patterns[pem->getPattern()]->differentiator.c_str(), position + pem->begin, position + pem->end,
				Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str());
#endif
	int relativeEnd = end - position, relativeBegin = begin - position, minCost = 10000;
	vector <cWordMatch>::iterator im = m.begin() + position;
	vector<cPatternElementMatchArray::tPatternElementMatch*> tpema;
	cPatternElementMatchArray::tPatternElementMatch* tpem = NULL;
	for (cPatternElementMatchArray::tPatternElementMatch* pem = pema.begin() + PEMAPosition;
		PEMAPosition >= 0 && pem->getParentPattern() == pattern && pem->end == relativeEnd;
		PEMAPosition = pem->nextByPatternEnd, pem = pema.begin() + PEMAPosition)
		if (pem->begin == relativeBegin)
		{
			int cost;
			int nextPEMAPosition = pema[PEMAPosition].nextPatternElement;
			if (pem->isChildPattern())
			{
				int childEnd = pem->getChildLen();
#ifdef LOG_PATTERN_COST_CHECK
				lplog(L"%d:RP PEMA %06d %s[%s](%d,%d) child %s[*](%d,%d) element #%d resulted in a minCost of %d.",
					position, PEMAPosition, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end,
					patterns[pem->getChildPattern()]->name.c_str(), position, position + childEnd, pem->getElement(), pem->iCost);
#endif
				cost = pem->getIncrementalCost();
				if (nextPEMAPosition >= 0 && !patterns[pattern]->nextPossibleElementRange(pem->getElement()))
					lplog(LOG_FATAL_ERROR, L"Inconsistency!");
				if (nextPEMAPosition >= 0)
					cost += updatePEMACosts(nextPEMAPosition, pattern, begin, end, position + childEnd, tpema);
			}
			else
			{
				cost = (pem->getElementIndex() == (unsigned char)-2) ? 0 : patterns[pattern]->getCost(pem->getElement(), pem->getElementIndex(), pem->isChildPattern());
				// only accumulate or use usage costs IF word is not capitalized
				if (im->costable())
					cost += im->word->second.getUsageCost(pem->getChildForm());
#ifdef LOG_PATTERN_COST_CHECK
				lplog(L"%d:RP PEMA %06d %s[%s](%d,%d) child form %s element #%d resulted in a minCost of %d.",
					position, PEMAPosition, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end,
					Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str(), pem->getElement(), cost);
#endif
				if (nextPEMAPosition >= 0 && !patterns[pattern]->nextPossibleElementRange(pem->getElement()))
					lplog(LOG_FATAL_ERROR, L"Inconsistency!");
				if (nextPEMAPosition >= 0)
					cost += updatePEMACosts(nextPEMAPosition, pattern, begin, end, position + 1, tpema);
			}
			if (cost < minCost)
			{
				minCost = cost;
				ppema = tpema;
				tpem = pem;
			}
		}
#ifdef LOG_PATTERN_COST_CHECK
		else
			lplog(L"%d:RP PEMA %s[%s](%d,%d) child %s[*](%d,%d) element #%d NO MATCH",
				position, patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), position + pem->begin, position + pem->end,
				patterns[pem->getChildPattern()]->name.c_str(), position, position + pem->getChildLen(), pem->getElement());
#endif
	if (tpem == NULL)
	{
		if (!m[position].word->second.isIgnore())
		{
			lplog(L"%d:Update PEMA ERROR with pattern %s[%s](%d,%d) [out of %d total positions]!", position,
				patterns[pattern]->name.c_str(), patterns[pattern]->differentiator.c_str(), begin, end, m.size());
			int scanPP = PEMAPosition;
			for (cPatternElementMatchArray::tPatternElementMatch* pem = pema.begin() + scanPP;
				scanPP >= 0; scanPP = pem->nextByPatternEnd, pem = pema.begin() + scanPP)
				if (pem->isChildPattern())
					lplog(L"%d:RP PEMA SCAN %s[%s](%d,%d) child %s[*](%d,%d)",
						position, patterns[pem->getParentPattern()]->name.c_str(), patterns[pem->getParentPattern()]->differentiator.c_str(), position + pem->begin, position + pem->end,
						patterns[pem->getChildPattern()]->name.c_str(), position, position + pem->getChildLen());
				else
					lplog(L"%d:RP PEMA %s[%s](%d,%d) child form %s",
						position, patterns[pem->getParentPattern()]->name.c_str(), patterns[pem->getParentPattern()]->differentiator.c_str(), position + pem->begin, position + pem->end,
						Forms[m[position].getFormNum(pem->getChildForm())]->shortName.c_str());
			lplog();
			return minCost;
		}
#ifdef LOG_PATTERN_COST_CHECK
		lplog(L"%d: IGNORED", position);
#endif
		return updatePEMACosts(PEMAPosition, pattern, begin, end, position + 1, ppema);
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
void cSource::reduceParent(int position, unsigned int PMAOffset, int reducedCost)
{
	LFS // DLFS
		cPatternMatchArray::tPatternMatch* pm = m[position].pma.content;
	if (PMAOffset >= m[position].pma.count)
		lplog(LOG_FATAL_ERROR, L"    %d:RP PMA pattern offset %d is >= %d - ILLEGAL", position, PMAOffset, m[position].pma.count);
	unsigned int childPattern = pm[PMAOffset].getPattern();;
	int childLen = pm[PMAOffset].len, originalCost = pm[PMAOffset].getCost();
	if (childPattern >= patterns.size())
		lplog(LOG_FATAL_ERROR, L"    %d:RP PMA pattern %d ILLEGAL", position, childPattern);
	if (originalCost > reducedCost)
	{
#ifdef LOG_PATTERN_COST_CHECK
		lplog(L"%d:RP PMA pattern %s[%s](%d,%d) reducing cost from %d to %d (offset=%d).", position,
			patterns[childPattern]->name.c_str(), patterns[childPattern]->differentiator.c_str(),
			position, position + childLen, originalCost, reducedCost, PMAOffset);
#endif
		pm[PMAOffset].cost = reducedCost; // could updateMaxMatch but this could unduly restrict the patterns surviving to stage two of eliminateLoserPatterns
	}
	else
	{
#ifdef LOG_PATTERN_COST_CHECK
		lplog(L"%d:RP PMA pattern %s[%s](%d,%d) cost reduction REJECTED (pm[PMAOffset=%d].getCost() %d<=reducedCost %d).", position,
			patterns[childPattern]->name.c_str(), patterns[childPattern]->differentiator.c_str(),
			position, position + childLen, PMAOffset, originalCost, reducedCost);
#endif
		return;
	}
	vector <cPatternElementMatchArray::tPatternElementMatch*> savePositions;
	for (int p = patterns[childPattern]->rootPattern; p >= 0; p = patterns[p]->nextRoot)
	{
		if (p == childPattern)
			pm = m[position].pma.content + PMAOffset;
		else if (!m[position].patterns.isSet(p) || !(pm = m[position].pma.find(p, childLen)))
			continue;
		// CHILD=rootPattern starting at position (position) and ending at childEnd
		cPatternElementMatchArray::tPatternElementMatch* pem;
		for (int PEMAPosition = pm->pemaByChildPatternEnd; PEMAPosition != -1; PEMAPosition = pem->nextByChildPatternEnd)
		{
			pem = pema.begin() + PEMAPosition;
			// this is needed because iCost is the reducedCost of the child + any costs added by the parent pattern ("_INFP*1")
			int incrementalCost = reducedCost + patterns[pem->getParentPattern()]->getCost(pem->getElement(), pem->getElementIndex(), pem->isChildPattern());
			if (pem->getParentPattern() != childPattern && // parent patterns never match themselves
				pem->getIncrementalCost() > incrementalCost)  // the reducedCost of ONE instance of the pattern may not be less than the lowest cost of all children
			{
#ifdef LOG_PATTERN_COST_CHECK
				char temp[1024];
				lplog(L"    %d:RP PMA pattern %s[%s](%d,%d) reducing cost (%d->%d) up into parent %s", position,
					patterns[childPattern]->name.c_str(), patterns[childPattern]->differentiator.c_str(),
					position, position + childLen, pem->iCost, incrementalCost, pem->toText(position + pem->begin, temp, m));
#endif
				pem->setIncrementalCost(incrementalCost);
				int finalCost = 0;
				vector<cPatternElementMatchArray::tPatternElementMatch*> ppema;
				if (patterns[pem->getParentPattern()]->nextPossibleElementRange(-1))
					finalCost = updatePEMACosts(pem->origin, pem->getParentPattern(), position + pem->begin, position + pem->end, position + pem->begin, ppema);
				vector <cPatternElementMatchArray::tPatternElementMatch*>::iterator ipem = ppema.begin(), ipemEnd = ppema.end();
				for (; ipem != ipemEnd; ipem++)
				{
#ifdef LOG_PATTERN_COST_CHECK
					if ((*ipem)->isChildPattern())
						lplog(L"    %d:pattern %s[%s](%d,%d)*%d child %s[%s](%d,%d) reduced to cost %d.", position,
							patterns[(*ipem)->getPattern()]->name.c_str(), patterns[(*ipem)->getPattern()]->differentiator.c_str(), position + (*ipem)->begin, position + (*ipem)->end, (*ipem)->getOCost(),
							patterns[(*ipem)->getChildPattern()]->name.c_str(), patterns[(*ipem)->getChildPattern()]->differentiator.c_str(), position, position + (*ipem)->getChildLen(),
							finalCost);
					else
						lplog(L"    %d:pattern %s[%s](%d,%d)*%d child %d form reduced to cost %d.", position,
							patterns[(*ipem)->getPattern()]->name.c_str(), patterns[(*ipem)->getPattern()]->differentiator.c_str(), position + (*ipem)->begin, position + (*ipem)->end, (*ipem)->getOCost(),
							(*ipem)->getChildForm(), finalCost);
#endif
					(*ipem)->setOCost(finalCost);
				}
				savePositions.push_back(pem);
			}
		}
	}
	// look up parent of each pem.  get lowest cost of the group of children.
	vector <cPatternElementMatchArray::tPatternElementMatch*>::iterator ipem = savePositions.begin(), ipemEnd = savePositions.end();
	for (; ipem != ipemEnd; ipem++)
	{
		int newPosition = position + (*ipem)->begin;
		if (!m[newPosition].patterns.isSet((*ipem)->getParentPattern())) continue;
		cPatternMatchArray::tPatternMatch* pmParent = m[newPosition].pma.find((*ipem)->getParentPattern(), (*ipem)->end - (*ipem)->begin);
		if (pmParent)
		{
			int lowestCost = MAX_COST;
			for (int PEMAOffset = pmParent->pemaByPatternEnd; PEMAOffset >= 0; PEMAOffset = pema[PEMAOffset].nextByPatternEnd)
				lowestCost = min(lowestCost, pema[PEMAOffset].getOCost());
			if (pmParent->getCost() > lowestCost)
			{
				//      pmParent->setCost(lowestCost);  NOT needed!  screws up recursion because this is done as the first step of the recursive call on the following line!
				reduceParent(newPosition, (int)(pmParent - m[newPosition].pma.content), lowestCost);
			}
		}
	}
}

void cSource::reduceParents(int position, vector <unsigned int>& insertionPoints, vector <int>& reducedCosts)
{
	LFS
		for (unsigned int I = 0; I < insertionPoints.size(); I++)
			reduceParent(position, insertionPoints[I], reducedCosts[I]);
}

void cSource::logPatternChain(int sourcePosition, int insertionPoint, enum cPatternElementMatchArray::chainType patternChainType)
{
	LFS
		const wchar_t* chPT = L"";
	int chain = -1, originPattern = 0, originEnd = 0;
	switch (patternChainType)
	{
	case cPatternElementMatchArray::BY_PATTERN_END:
		chPT = L"PE";
		chain = m[sourcePosition].pma[insertionPoint].pemaByPatternEnd;
		originPattern = m[sourcePosition].pma[insertionPoint].getPattern();
		originEnd = m[sourcePosition].pma[insertionPoint].len;
		::lplog(L"%d:%s CHAIN (%03d,%02d) PMA %d %s[%s]*%d(%d,%d)", sourcePosition, chPT,
			originPattern, originEnd, insertionPoint,
			patterns[m[sourcePosition].pma[insertionPoint].getPattern()]->name.c_str(),
			patterns[m[sourcePosition].pma[insertionPoint].getPattern()]->differentiator.c_str(),
			m[sourcePosition].pma[insertionPoint].getCost(),
			sourcePosition,
			sourcePosition + m[sourcePosition].pma[insertionPoint].len);
		break;
	case cPatternElementMatchArray::BY_POSITION:
		chPT = L"PO";
		chain = insertionPoint;
		originPattern = pema[insertionPoint].getParentPattern();
		originEnd = pema[insertionPoint].end;
		break;
	case cPatternElementMatchArray::BY_CHILD_PATTERN_END:
		chPT = L"PA";
		chain = insertionPoint;
		originPattern = pema[insertionPoint].getParentPattern();
		originEnd = pema[insertionPoint].end;
		break;
	}
	while (chain >= 0)
	{
		if (pema[chain].isChildPattern())
			::lplog(L"%d:%s CHAIN (%03d,%02d) %d:%s[%s]*%d(%d,%d) child %s[%s](%d,%d)", sourcePosition, chPT,
				originPattern, originEnd, chain,
				patterns[pema[chain].getParentPattern()]->name.c_str(),
				patterns[pema[chain].getParentPattern()]->differentiator.c_str(),
				pema[chain].getOCost(),
				sourcePosition + pema[chain].begin,
				sourcePosition + pema[chain].end,
				patterns[pema[chain].getChildPattern()]->name.c_str(),
				patterns[pema[chain].getChildPattern()]->differentiator.c_str(),
				sourcePosition,
				sourcePosition + pema[chain].getChildLen());
		else
			::lplog(L"%d:%s CHAIN (%03d,%02d) %d:%s[%s]*%d(%d,%d) child form %s", sourcePosition, chPT,
				originPattern, originEnd, chain,
				patterns[pema[chain].getParentPattern()]->name.c_str(),
				patterns[pema[chain].getParentPattern()]->differentiator.c_str(),
				pema[chain].getOCost(),
				sourcePosition + pema[chain].begin,
				sourcePosition + pema[chain].end,
				Forms[m[sourcePosition].getFormNum(pema[chain].getChildForm())]->shortName.c_str());
		switch (patternChainType)
		{
		case cPatternElementMatchArray::BY_PATTERN_END:chain = pema[chain].nextByPatternEnd; break;
		case cPatternElementMatchArray::BY_POSITION:chain = pema[chain].nextByPosition; break;
		case cPatternElementMatchArray::BY_CHILD_PATTERN_END:chain = pema[chain].nextByChildPatternEnd; break;
		}
	}
}

cSource::cSource(const wchar_t* databaseServer, int _sourceType, bool generateFormStatistics, bool skipWordInitialization, bool printProgress)
{
	LFS
		sourceType = _sourceType;
	primaryQuoteType = L"\"";
	secondaryQuoteType = L"\'";
	bookBuffer = NULL;
	lastPEMAConsolidationIndex = 1; // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet = -1;
	gTraceSource = 0;
	alreadyConnected = false;
	answerContainedInSource = 0;
	numTotalNarratorFullVerbTenses = numTotalNarratorVerbTenses = 0;
	numTotalSpeakerFullVerbTenses = numTotalSpeakerVerbTenses = 0;
	debugTrace.printBeforeElimination = false;
	debugTrace.traceSubjectVerbAgreement = false;
	debugTrace.traceTestSubjectVerbAgreement = false;
	debugTrace.traceEVALObjects = false;
	debugTrace.traceAnaphors = false;
	debugTrace.traceRelations = false;
	debugTrace.traceTestSyntacticRelations = false;
	debugTrace.traceNyms = false;
	debugTrace.traceRole = false;
	debugTrace.traceWhere = false;
	debugTrace.traceTime = false;
	debugTrace.traceWikipedia = false;
	debugTrace.traceWebSearch = false;
	debugTrace.traceQCheck = false;
	debugTrace.traceTransformDestinationQuestion = false;
	debugTrace.traceLinkQuestion = false;
	debugTrace.traceMapQuestion = false;
	debugTrace.traceQuestionPatternMap = false;
	debugTrace.traceVerbObjects = false;
	debugTrace.traceDeterminer = false;
	debugTrace.traceBNCPreferences = false;
	debugTrace.tracePatternElimination = false;
	debugTrace.traceSecondaryPEMACosting = false;
	debugTrace.traceSpeakerResolution = false;
	debugTrace.traceObjectResolution = false;
	debugTrace.traceMatchedSentences = false;
	debugTrace.traceUnmatchedSentences = false;
	debugTrace.traceIncludesPEMAIndex = false;
	debugTrace.traceTagSetCollection = false;
	debugTrace.collectPerSentenceStats = false;
	debugTrace.traceNameResolution = false;
	debugTrace.traceParseInfo = false;
	debugTrace.tracePreposition = false;
	debugTrace.tracePatternMatching = false;
	pass = -1;
	repeatReplaceObjectInSectionPosition = -1;
	accumulateLocationLastLocation = -1;
	tagSetTimeArray = NULL;
	tagSetTimeArraySize = 0;
	sourceInPast = false;
	if (initializeDatabaseHandle(mysql, databaseServer, alreadyConnected) < 0)
	{
		createDatabase(databaseServer);
		Words.createWordCategories();
	}
	else
	{
		// only read words if it hasn't already been done (lastReadfromDBTime).  This will have to change if we want to read words that have been changed during program execution.
		if (Words.lastReadfromDBTime < 0 && Words.readWordsFromDB(mysql, generateFormStatistics, printProgress, skipWordInitialization))
			lplog(LOG_FATAL_ERROR, L"Cannot read database.");
	}
	unlockTables(mysql);
#ifdef CHECK_WORD_CACHE
	Words.testWordCacheFileRoutines();
	exit(0);
#endif
	if (!skipWordInitialization)
	{
		Words.initialize();
		Words.readWordsOfMultiWordObjects(multiWordStrings, multiWordObjects);
		Words.addMultiWordObjects(multiWordStrings, multiWordObjects);
	}
	parentSource = NULL;
	lastSense = -1;
	firstQuote = lastQuote = -1;
	speakersMatched = speakersNotMatched = counterSpeakersMatched = counterSpeakersNotMatched = 0;
	unordered_map <int, vector < vector <cTagLocation> > > emptyMap;
	for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
		pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
	primaryLocationLastMovingPosition = -1;
	primaryLocationLastPosition = -1;
	updateWordUsageCostsDynamically = false;
	RDFFileCaching = true;
}

cSource::cSource(MYSQL* parentMysql, int _sourceType, int _sourceConfidence)
{
	LFS
		sourceType = _sourceType;
	sourceConfidence = _sourceConfidence;
	primaryQuoteType = L"\"";
	secondaryQuoteType = L"\'";
	bookBuffer = NULL;
	lastPEMAConsolidationIndex = 1; // minimum PEMA offset is 1 - not 0
	lastSourcePositionSet = -1;
	gTraceSource = 0;
	alreadyConnected = true;
	answerContainedInSource = 0;
	numTotalNarratorFullVerbTenses = numTotalNarratorVerbTenses = 0;
	numTotalSpeakerFullVerbTenses = numTotalSpeakerVerbTenses = 0;
	debugTrace.printBeforeElimination = false;
	debugTrace.traceSubjectVerbAgreement = false;
	debugTrace.traceTestSubjectVerbAgreement = false;
	debugTrace.traceEVALObjects = false;
	debugTrace.traceAnaphors = false;
	debugTrace.traceRelations = false;
	debugTrace.traceTestSyntacticRelations = false;
	debugTrace.traceNyms = false;
	debugTrace.traceRole = false;
	debugTrace.traceWhere = false;
	debugTrace.traceTime = false;
	debugTrace.traceWikipedia = false;
	debugTrace.traceWebSearch = false;
	debugTrace.traceQCheck = false;
	debugTrace.traceTransformDestinationQuestion = false;
	debugTrace.traceLinkQuestion = false;
	debugTrace.traceMapQuestion = false;
	debugTrace.traceQuestionPatternMap = false;
	debugTrace.traceVerbObjects = false;
	debugTrace.traceDeterminer = false;
	debugTrace.traceBNCPreferences = false;
	debugTrace.tracePatternElimination = false;
	debugTrace.traceSecondaryPEMACosting = false;
	debugTrace.traceSpeakerResolution = false;
	debugTrace.traceObjectResolution = false;
	debugTrace.traceMatchedSentences = false;
	debugTrace.traceUnmatchedSentences = false;
	debugTrace.traceIncludesPEMAIndex = false;
	debugTrace.traceTagSetCollection = false;
	debugTrace.collectPerSentenceStats = false;
	debugTrace.traceNameResolution = false;
	debugTrace.traceParseInfo = false;
	debugTrace.tracePreposition = false;
	debugTrace.tracePatternMatching = false;
	pass = -1;
	repeatReplaceObjectInSectionPosition = -1;
	accumulateLocationLastLocation = -1;
	tagSetTimeArray = NULL;
	tagSetTimeArraySize = 0;
	mysql = *parentMysql;
	lastSense = -1;
	firstQuote = lastQuote = -1;
	speakersMatched = speakersNotMatched = counterSpeakersMatched = counterSpeakersNotMatched = 0;
	primaryLocationLastMovingPosition = -1;
	primaryLocationLastPosition = -1;
	unordered_map <int, vector < vector <cTagLocation> > > emptyMap;
	for (unsigned int ts = 0; ts < desiredTagSets.size(); ts++)
		pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
	updateWordUsageCostsDynamically = false;
	sourceInPast = false;
}

void cSource::writeWords(wstring oPath, wstring specialExtension)
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
	int where = 0, numWordInSource = 0;
	for (cWordMatch im : m)
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

bool cSource::matchChildSourcePositionSynonym(tIWMM parentWord, cSource* childSource, int childWhere)
{
	LFS
		if (parentWord == Words.end())
		{
			lplog(LOG_ERROR, L"parent word is not in dictionary!");
			return false;
		}
	unordered_set <wstring> childSynonyms, parentSynonyms;
	getSynonyms(parentWord->first, parentSynonyms, NOUN);
	tIWMM parentME = parentWord->second.mainEntry;
	if (parentME == wNULL)
		parentME = parentWord;
	if (childSource->m[childWhere].objectMatches.empty())
	{
		if (childSource->m[childWhere].endObjectPosition >= 0)
			childWhere = childSource->m[childWhere].endObjectPosition - 1;
		wstring childWord = childSource->m[childWhere].getMainEntry()->first;
		if (childWord.find('%') != wstring::npos)
			return false;
		wstring tmpstr;
		if (parentWord->first == childWord || parentME->first == childWord)
		{
			if (logSynonymDetail && childSource->debugTrace.traceWhere)
				lplog(LOG_WHERE, L"TSYM [1] comparing PARENT %s[%s] against CHILD [%s]", parentWord->first.c_str(), parentME->first.c_str(), childWord.c_str());
			return true;
		}
		if (logSynonymDetail && childSource->debugTrace.traceWhere)
			lplog(LOG_WHERE, L"TSYM [1] comparing CHILD %s against synonyms [%s]%s", childWord.c_str(), parentWord->first.c_str(), setString(parentSynonyms, tmpstr, L"|").c_str());
		if (parentSynonyms.find(childWord) != parentSynonyms.end())
			return true;
		getSynonyms(childWord, childSynonyms, NOUN);
		if (logSynonymDetail && childSource->debugTrace.traceWhere)
			lplog(LOG_WHERE, L"TSYM [1] comparing PARENT %s against synonyms [%s]%s", parentME->first.c_str(), childWord.c_str(), setString(childSynonyms, tmpstr, L"|").c_str());
		if (childSynonyms.find(parentME->first) != childSynonyms.end())
			return true;
	}
	for (unsigned int mo = 0; mo < childSource->m[childWhere].objectMatches.size(); mo++)
	{
		int cw, ownerWhere;
		if ((cw = childSource->objects[childSource->m[childWhere].objectMatches[mo].object].originalLocation) >= 0 && !childSource->isDefiniteObject(cw, L"CHILD MATCHED", ownerWhere, false))
		{
			if (childSource->m[cw].endObjectPosition >= 0)
				cw = childSource->m[cw].endObjectPosition - 1;
			wstring tmpstr;
			getSynonyms(childSource->m[cw].word->first, childSynonyms, NOUN);
			bool childFound = childSynonyms.find(parentWord->first) != childSynonyms.end();
			if (logSynonymDetail && childSource->debugTrace.traceWhere)
				lplog(LOG_WHERE, L"TSYM [2CHILD] comparing %s against synonyms [%s]%s (%s)", parentWord->first.c_str(), childSource->m[cw].word->first.c_str(), setString(childSynonyms, tmpstr, L"|").c_str(), (childFound) ? L"true" : L"false");
			if (childFound)
				return true;
			bool parentFound = parentSynonyms.find(childSource->m[cw].word->first) != parentSynonyms.end();
			if (logSynonymDetail && childSource->debugTrace.traceWhere)
				lplog(LOG_WHERE, L"TSYM [2PARENT] comparing %s against synonyms [%s]%s (%s)", childSource->m[cw].word->first.c_str(), parentWord->first.c_str(), setString(parentSynonyms, tmpstr, L"|").c_str(), (parentFound) ? L"true" : L"false");
			if (parentFound)
				return true;
		}
	}
	return false;
}

// does this represent a definite noun, like Sting or MIT?  If so, disallow synonyms (or perhaps use another kind of synonym which is not yet supported)
// also people are not allowed
// also include subjects which follow immediately after relativizers which have resolved to an object.
bool cSource::isDefiniteObject(int where, const wchar_t* definiteObjectType, int& ownerWhere, bool recursed)
{
	LFS
		int object = m[where].getObject();
	if (object < 0) return false;
	wstring tmpstr;
	int bp = m[where].beginObjectPosition;
	switch (objects[object].objectClass)
	{
	case PRONOUN_OBJECT_CLASS:
		if (m[bp].queryWinnerForm(indefinitePronounForm) >= 0)
		{
			if (logSynonymDetail)
				lplog(LOG_WHERE, L"%06d:TSYM %s %s is %sa definite object [byIndefiniteClass].", where, definiteObjectType, objectString(object, tmpstr, false).c_str(), L"NOT ");
			return false;
		}
	case REFLEXIVE_PRONOUN_OBJECT_CLASS:
	case RECIPROCAL_PRONOUN_OBJECT_CLASS:
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is %sa definite object [byClassGender].", where, definiteObjectType, objectString(object, tmpstr, false).c_str(), (objects[object].neuter && !objects[object].male && !objects[object].female) ? L"NOT " : L"");
		return !objects[object].neuter || objects[object].male || objects[object].female;
	case GENDERED_DEMONYM_OBJECT_CLASS: // the Italians
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is %sa definite object [byDemonym].", where, definiteObjectType, objectString(object, tmpstr, false).c_str(), (m[bp].word->first == L"a" || m[bp].word->first == L"an") ? L"" : L"NOT ");
		return (m[bp].word->first == L"a" || m[bp].word->first == L"an");
	case PLEONASTIC_OBJECT_CLASS: // it, which is not matched and so is meaningless
		return false;
	case NON_GENDERED_BUSINESS_OBJECT_CLASS: // G.M.
	case GENDERED_RELATIVE_OBJECT_CLASS: // sister ; brother / these have different kinds of synonyms which are more like isKindOf
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byClass].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
		return true;
	case VERB_OBJECT_CLASS: // the news I brought / running -- objects that contain verbs
	case GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS: // occupation=plumber;role=leader;activity=runner
	case META_GROUP_OBJECT_CLASS: // different kinds of friend words friendForm/groupJoiner/etc
	case BODY_OBJECT_CLASS: // arm/leg
	case GENDERED_GENERAL_OBJECT_CLASS: // the man/the woman
	case NON_GENDERED_GENERAL_OBJECT_CLASS: // anything else that is not capitalized and not in any other category
		// check for an 'owned' object: his plumber / my specialty / her ship // this organization
		// his rabbit hole / the baby's name
		// my academic [S My academic specialty[17-20]{OWNER:UNK_M_OR_F}[19][nongen][N][OGEN]]  is international constitutional [O international constitutional law[21-24][23][nongen][N]]for   [PO 20[28-29][28][nongen][N]].
		if (m[where].endObjectPosition - bp > 1 &&
			(m[ownerWhere = bp].queryWinnerForm(possessiveDeterminerForm) >= 0 ||
				m[bp].queryWinnerForm(demonstrativeDeterminerForm) >= 0 ||
				(m[bp].word->second.inflectionFlags & (SINGULAR_OWNER | PLURAL_OWNER)) ||
				(m[bp].flags & cWordMatch::flagNounOwner) ||
				(m[ownerWhere = where].endObjectPosition - bp > 2 &&
					(m[m[where].endObjectPosition - 2].word->second.inflectionFlags & (SINGULAR_OWNER | PLURAL_OWNER)) ||
					(m[m[where].endObjectPosition - 2].flags & cWordMatch::flagNounOwner))))
		{
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byOwner].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
			return true;
		}
		if (bp > 0 && m[bp - 1].queryWinnerForm(relativizerForm) != -1 && m[bp - 1].objectMatches.size() > 0)
		{
			ownerWhere = bp - 1;
			wstring tmpstr2;
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byRelativizer - %s].", where, definiteObjectType, objectString(object, tmpstr, false).c_str(), objectString(m[bp - 1].objectMatches[0].object, tmpstr2, false).c_str());
			return true;
		}
		ownerWhere = -1;
		{
			bool accumulatedDefiniteObject = false, accumulatedNotDefiniteObject = false;
			int omsize = m[where].objectMatches.size();
			for (int I = 0; I < omsize; I++)
				if (m[where].objectMatches[I].object != object && objects[m[where].objectMatches[I].object].originalLocation != where && !recursed)
				{
					if (isDefiniteObject(objects[m[where].objectMatches[I].object].originalLocation, definiteObjectType, ownerWhere, true))
						accumulatedDefiniteObject |= true;
					else
						accumulatedNotDefiniteObject |= true;
				}
			if (logSynonymDetail)
			{
				if (omsize > 0)
					lplog(LOG_WHERE, L"%06d:TSYM %s %s is%s a definite object [byClass (2)].", where, definiteObjectType, objectString(object, tmpstr, false).c_str(), (accumulatedDefiniteObject) ? L"" : L" NOT");
				else
					lplog(LOG_WHERE, L"%06d:TSYM %s %s is NOT a definite object [byClass].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
			}
			if (omsize && accumulatedDefiniteObject)
				return true;
		}
		return false;
	case NAME_OBJECT_CLASS:
	case NON_GENDERED_NAME_OBJECT_CLASS:
	default:;
	}
	if (!objects[object].dbPediaAccessed)
		identifyISARelation(where, false, RDFFileCaching);
	if (objects[object].isWikiBusiness || objects[object].isWikiPerson || objects[object].isWikiPlace || objects[object].isWikiWork)
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byWiki].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
		return true;
	}
	if (objects[object].name.hon != wNULL)
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byHonorific].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
		return true;
	}
	switch (objects[object].getSubType())
	{
	case CANADIAN_PROVINCE_CITY:
	case COUNTRY:
	case ISLAND:
	case MOUNTAIN_RANGE_PEAK_LANDFORM:
	case OCEAN_SEA:
	case PARK_MONUMENT:
	case REGION:
	case RIVER_LAKE_WATERWAY:
	case US_CITY_TOWN_VILLAGE:
	case US_STATE_TERRITORY_REGION:
	case WORLD_CITY_TOWN_VILLAGE:
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [bySubType].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
		return true;
	}
	if (m[where].endObjectPosition - m[where].beginObjectPosition > 1 &&
		(m[m[where].beginObjectPosition].queryWinnerForm(possessiveDeterminerForm) >= 0 ||
			(m[m[where].beginObjectPosition].word->second.inflectionFlags & (SINGULAR_OWNER | PLURAL_OWNER)) ||
			(m[m[where].beginObjectPosition].flags & cWordMatch::flagNounOwner)))
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byOwner].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
		ownerWhere = m[where].beginObjectPosition;
		return true;
	}
	// the Nobel Prize
	if (m[m[where].beginObjectPosition].word->first == L"the")
	{
		if (logSynonymDetail)
			lplog(LOG_WHERE, L"%06d:TSYM %s %s is a definite object [byThe].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
		return true;
	}
	if (logSynonymDetail)
		lplog(LOG_WHERE, L"%06d:TSYM %s %s is NOT a definite object [default].", where, definiteObjectType, objectString(object, tmpstr, false).c_str());
	return false;
}

int cSource::determineKindBitFieldFromObject(cSource* source, int object, int& wikiBitField)
{
	LFS
		if (source->objects[object].isWikiBusiness) wikiBitField |= 1;
	if (source->objects[object].isWikiPerson) wikiBitField |= 2;
	if (source->objects[object].isWikiPlace) wikiBitField |= 4;
	if (source->objects[object].isWikiWork) wikiBitField |= 8;
	return 0;
}

int cSource::determineKindBitField(cSource* source, int where, int& wikiBitField)
{
	LFS
		if (source->m[where].getObject() < 0 && source->m[where].objectMatches.size() == 0)
		{
			if (matchChildSourcePositionSynonym(Words.query(L"business"), source, where) || source->m[where].getMainEntry()->first == L"business")
				return wikiBitField |= 1;
			if (matchChildSourcePositionSynonym(Words.query(L"person"), source, where) || source->m[where].getMainEntry()->first == L"person")
				return wikiBitField |= 2;
			if (matchChildSourcePositionSynonym(Words.query(L"place"), source, where) || source->m[where].getMainEntry()->first == L"place")
				return wikiBitField |= 4;
			return -1;
		}
	if (source->m[where].objectMatches.size() >= 1)
	{
		for (unsigned int om = 0; om < source->m[where].objectMatches.size(); om++)
			determineKindBitFieldFromObject(source, source->m[where].objectMatches[om].object, wikiBitField);
	}
	else
	{
		determineKindBitFieldFromObject(source, source->m[where].getObject(), wikiBitField);
		if (source->objects[source->m[where].getObject()].objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
			(source->m[where].endObjectPosition - source->m[where].beginObjectPosition == 1 ||
				(source->m[where].endObjectPosition - source->m[where].beginObjectPosition == 2 && source->m[source->m[where].beginObjectPosition].queryForm(determinerForm) != -1)))
		{
			if (matchChildSourcePositionSynonym(Words.query(L"business"), source, where) || source->m[where].getMainEntry()->first == L"business")
				return wikiBitField |= 1;
			if (matchChildSourcePositionSynonym(Words.query(L"person"), source, where) || source->m[where].getMainEntry()->first == L"person")
				return wikiBitField |= 2;
			if (matchChildSourcePositionSynonym(Words.query(L"place"), source, where) || source->m[where].getMainEntry()->first == L"place")
				return wikiBitField |= 4;
		}
	}
	return 0;
}

void cSource::checkParticularPartSemanticMatchWord(int logType, int parentWhere, bool& synonym, unordered_set <wstring>& parentSynonyms, wstring pw, wstring pwme, int& lowestConfidence, unordered_map <wstring, int >::iterator ami)
{
	LFS // DLFS
 //if (logQuestionDetail)
 //	lplog(logType,L"checkParticularPartSemanticMatchWord child=%s",ami->first.c_str());	
		bool rememberSynonym = false;
	if (ami->first == pw || ami->first == pwme || (rememberSynonym = parentSynonyms.find(ami->first) != parentSynonyms.end()))
	{
		if (lowestConfidence > ami->second)
		{
			lowestConfidence = (ami->second);
			synonym = rememberSynonym;
			if (logQuestionDetail)
			{
				wstring tmpstr;
				lplog(logType, L"object %s:(primary match:%s[%s]) MATCH %s%s", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), ami->first.c_str(), (synonym) ? L" SYNONYM" : L"");
			}
		}
	}
}

// may be passed a childObject which is -1, in which case it will try to derive it from childWhere.  This is only recommended if the resolution of the object location is simple (no multiple matches).
int cSource::checkParticularPartSemanticMatch(int logType, int parentWhere, cSource* childSource, int childWhere, int childObject, bool& synonym, int& semanticMismatch, bool fileCaching)
{
	LFS
		if (childWhere < 0)
			return CONFIDENCE_NOMATCH;
	if (childObject < 0)
		childObject = (childSource->m[childWhere].objectMatches.size() > 0) ? childSource->m[childWhere].objectMatches[0].object : childSource->m[childWhere].getObject();
	if (childObject < 0)
		return CONFIDENCE_NOMATCH;
	if (childSource->objects[childObject].originalLocation < 0 || childSource->m[childSource->objects[childObject].originalLocation].beginObjectPosition < 0 || childSource->m[childSource->objects[childObject].originalLocation].endObjectPosition < 0)
		lplog(LOG_FATAL_ERROR, L"Incorrect object information detected.");
	unordered_map <wstring, int > associationMap;
	childSource->getAssociationMapMaster(childSource->objects[childObject].originalLocation, -1, associationMap, TEXT(__FUNCTION__), fileCaching);
	wstring tmpstr, tmpstr2, pw = m[parentWhere].word->first;
	transform(pw.begin(), pw.end(), pw.begin(), (int(*)(int)) tolower);
	wstring pwme = m[parentWhere].getMainEntry()->first;
	unordered_set <wstring> parentSynonyms;
	getSynonyms(pw, parentSynonyms, NOUN);
	getSynonyms(pwme, parentSynonyms, NOUN);
	int lowestConfidence = CONFIDENCE_NOMATCH;
	setString(parentSynonyms, tmpstr2, L"|");
	tmpstr.clear();
	for (unordered_map <wstring, int >::iterator ami = associationMap.begin(), amiEnd = associationMap.end(); ami != amiEnd; ami++)
		tmpstr += ami->first + L"|";
	if (logSynonymDetail)
	{
		wstring parentObject, childObjectStr, childObjectOriginalLocation;
		whereString(parentWhere, parentObject, false);
		childSource->whereString(childWhere, childObjectStr, false);
		childSource->whereString(childSource->objects[childObject].originalLocation, childObjectOriginalLocation, false);
		lplog(logType, L"%d:Comparing [%d, %d] %s and %s(%s)\nassociationMap for %s: %s\nparentSynonyms for %s: %s.", parentWhere,
			parentWhere, childWhere, parentObject.c_str(), childObjectStr.c_str(), childObjectOriginalLocation.c_str(),
			childObjectStr.c_str(), tmpstr.c_str(), pw.c_str(), tmpstr2.c_str());
	}
	for (unordered_map <wstring, int >::iterator ami = associationMap.begin(), amiEnd = associationMap.end(); ami != amiEnd && lowestConfidence > 1; ami++)
		checkParticularPartSemanticMatchWord(logType, parentWhere, synonym, parentSynonyms, pw, pwme, lowestConfidence, ami);
	//if (childWhere==886)
	//{
	//	logQuestionDetail=0;
	//	if (saveConfidence>lowestConfidence)
	//		lplog(LOG_WHERE,L"Comparing [%d, %d] %s and %s(%s)\nassociationMap for %s: %s\nparentSynonyms for %s: %s.",
	//				parentWhere,childWhere,whereString(parentWhere,tmp1,false).c_str(),childSource->whereString(childWhere,tmp2,false).c_str(),childSource->whereString(childSource->objects[childObject].originalLocation,tmp3,false).c_str(),
	//				tmp2.c_str(),tmpstr.c_str(),pw.c_str(),tmpstr2.c_str());
	//}
	if (lowestConfidence == CONFIDENCE_NOMATCH)
	{
		int lastChildWhere = childSource->objects[childObject].originalLocation;
		if (childSource->objects[childObject].objectClass == NAME_OBJECT_CLASS || childSource->objects[childObject].objectClass == NON_GENDERED_NAME_OBJECT_CLASS)
			lastChildWhere = childSource->m[childSource->objects[childObject].originalLocation].endObjectPosition - 1;
		wstring cw = childSource->m[lastChildWhere].word->first;
		transform(cw.begin(), cw.end(), cw.begin(), (int(*)(int)) tolower);
		wstring cwme = childSource->m[lastChildWhere].getMainEntry()->first;
		if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
			lplog(logType, L"checkParticularPartSemanticMatch child alternate=%s[%s]", cw.c_str(), cwme.c_str());
		if (pw == cw || pwme == cwme)
		{
			if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"parent word %s:(primary match:%s[%s]) MATCH %s[%s]", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), cw.c_str(), cwme.c_str());
			lowestConfidence = CONFIDENCE_NOMATCH / 4;
		}
		if (synonym = lowestConfidence == CONFIDENCE_NOMATCH && (parentSynonyms.find(cw) != parentSynonyms.end() || parentSynonyms.find(cwme) != parentSynonyms.end()))
		{
			if (logSynonymDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"word %s MATCH %s[%s] SYNONYM", whereString(parentWhere, tmpstr, false).c_str(), cw.c_str(), cwme.c_str());
			lowestConfidence = CONFIDENCE_NOMATCH / 2;
		}
	}
	if (lowestConfidence == CONFIDENCE_NOMATCH)
	{
		int parentWikiBitField = 0, childWikiBitField = 0;
		determineKindBitField(this, parentWhere, parentWikiBitField);
		determineKindBitField(childSource, childWhere, childWikiBitField);
		if (parentWikiBitField && childWikiBitField && !(parentWikiBitField & childWikiBitField))
		{
			semanticMismatch = 10;
			if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"object parent [%s]:(primary match:%s[%s]) child [%s] BitField mismatch", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), childSource->objectString(childObject, tmpstr2, false).c_str(), parentWikiBitField, childWikiBitField);
		}
		// profession
		const wchar_t* professionLimitedSynonyms[] = { L"avocation", L"calling", L"career", L"employment", L"occupation", L"vocation", L"job", L"livelihood", L"profession", L"work", NULL };
		bool parentIsProfession = false;
		for (int I = 0; professionLimitedSynonyms[I] && !parentIsProfession; I++)
			parentIsProfession |= m[parentWhere].word->first == professionLimitedSynonyms[I];
		if (parentIsProfession && childSource->m[childWhere].queryForm(commonProfessionForm) < 0)
		{
			semanticMismatch = 11;
			if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
				lplog(logType, L"object parent [%s]:(primary match:%s[%s]) child [%s] profession mismatch", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), childSource->objectString(childObject, tmpstr2, false).c_str(), parentWikiBitField, childWikiBitField);
		}
		if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
			lplog(logType, L"object parent [%s]:(primary match:%s[%s]) child [%s] NO MATCH", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), childSource->objectString(childObject, tmpstr2, false).c_str(), parentWikiBitField, childWikiBitField);
	}
	else if (logQuestionDetail && ((logType == LOG_WHERE && debugTrace.traceWhere) || (logType == LOG_RESOLUTION && debugTrace.traceSpeakerResolution)))
		lplog(logType, L"object parent [%s]:(primary match:%s[%s]) child [%s] lowest confidence %d", whereString(parentWhere, tmpstr, false).c_str(), pw.c_str(), pwme.c_str(), childSource->objectString(childObject, tmpstr2, false).c_str(), lowestConfidence);
	return lowestConfidence;
}

void cSource::copySource(cSource* childSource, int begin, int end, unordered_map <int, int>& sourceIndexMap)
{
	LFS
		for (int I = begin; I < end; I++)
		{
			sourceIndexMap[I] = m.size();
			m.push_back(childSource->m[I]);
		}
}

int cSource::copyDirectlyAttachedPrepositionalPhrase(cSource* childSource, int relPrep, unordered_map <int, int>& sourceIndexMap, bool clear)
{
	LFS
		int relObject = childSource->m[relPrep].getRelObject();
	if (relObject >= relPrep && childSource->m[relObject].endObjectPosition >= 0)
	{
		copySource(childSource, relPrep, childSource->m[relObject].endObjectPosition, sourceIndexMap);
		if (clear)
		{
			for (int I = m.size() + relPrep - childSource->m[relObject].endObjectPosition; I < m.size(); I++)
			{
				m[I].clearAfterCopy();
				m[I].principalWherePosition = -1;
				m[I].principalWhereAdjectivalPosition = -1;
			}
		}
		return childSource->m[relObject].endObjectPosition - relPrep;
	}
	return 0;
}

bool cSource::isObjectCapitalized(int where)
{
	LFS
		if (where < 0)
			return false;
	int lastWord = m[where].endObjectPosition - 1;
	return (lastWord >= 0 && (m[lastWord].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (m[lastWord].flags & cWordMatch::flagFirstLetterCapitalized) || (m[lastWord].flags & cWordMatch::flagAllCaps)));
}

// if getUntilNumPP==-1, this gets the maximum number of available directly attached non-mixed case prepositional phrases
bool cSource::ppExtensionAvailable(int where, int& getUntilNumPP, bool nonMixed)
{
	LFS
		if (getUntilNumPP == 0) return true;
	int relPrep = m[where].endObjectPosition;
	if (where >= 0 && where < (signed)m.size() && relPrep >= 0 && relPrep < (signed)m.size() && m[relPrep].queryWinnerForm(prepositionForm) >= 0)
	{
		if (m[relPrep].nextQuote == -1)
			m[relPrep].nextQuote = where;
		if (m[relPrep].nextQuote == where || m[relPrep].relNextObject == where)
		{
			bool isCapitalized = isObjectCapitalized(where);
			int numPP;
			for (numPP = 0; relPrep >= 0 && m[relPrep].getRelObject() >= 0 && m[relPrep].getRelObject() < (signed)m.size() && (m[relPrep].nextQuote == where || m[relPrep].relNextObject == where) && (numPP < getUntilNumPP || getUntilNumPP == -1); numPP++)
			{
				if ((getUntilNumPP == -1 || numPP < getUntilNumPP) && nonMixed && (isObjectCapitalized(m[relPrep].getRelObject()) ^ isCapitalized))
				{
					if (getUntilNumPP == -1)
						getUntilNumPP = numPP;
					return false;
				}
				relPrep = m[relPrep].relPrep;
			}
			if (getUntilNumPP == -1)
				getUntilNumPP = numPP;
			return numPP >= getUntilNumPP;
		}
	}
	if (getUntilNumPP == -1)
		getUntilNumPP = 0;
	return false;
}

int cSource::numWordsOfDirectlyAttachedPrepositionalPhrases(int whereChild)
{
	int relPrep = m[whereChild].endObjectPosition;
	if (relPrep < 0 || relPrep >= (signed)m.size() || m[relPrep].queryWinnerForm(prepositionForm) < 0 || m[relPrep].nextQuote != whereChild || relPrep < m[whereChild].endObjectPosition) return 0;
	while (m[relPrep].relPrep >= 0 && m[relPrep].relPrep > m[whereChild].endObjectPosition && (m[relPrep].nextQuote == whereChild || m[relPrep].relNextObject == whereChild))
		relPrep = m[relPrep].relPrep;
	return relPrep - m[whereChild].endObjectPosition;
}

int cSource::copyDirectlyAttachedPrepositionalPhrases(int whereParentObject, cSource* childSource, int whereChild, unordered_map <int, int>& sourceIndexMap, bool clear)
{
	LFS
		int relPrep = childSource->m[whereChild].endObjectPosition;
	if (relPrep < 0 || relPrep >= (signed)childSource->m.size() || childSource->m[relPrep].queryWinnerForm(prepositionForm) < 0 || childSource->m[relPrep].nextQuote != whereChild) return 0;
	m[whereParentObject].relPrep = relPrep;
	while (relPrep >= 0 && (childSource->m[relPrep].nextQuote == whereChild || childSource->m[relPrep].relNextObject == whereChild))
	{
		copyDirectlyAttachedPrepositionalPhrase(childSource, relPrep, sourceIndexMap, clear);
		relPrep = childSource->m[relPrep].relPrep;
	}
	return m.size() - m[whereParentObject].relPrep;
}

void cWordMatch::adjustValue(int& val, wstring valString, unordered_map <int, int>& sourceIndexMap)
{
	if (val < 0)
		return;
	if (sourceIndexMap.find(val) != sourceIndexMap.end())
		val = sourceIndexMap[val];
	else
	{
		lplog(LOG_WHERE, L"Unable to adjust index %s of %d. Setting to -1.", valString.c_str(), val);
		val = -1;
	}
}

void cWordMatch::adjustReferences(int index, bool keepObjects, unordered_map <int, int>& sourceIndexMap)
{
	LFS
		if (beginObjectPosition >= 0 && sourceIndexMap.find(beginObjectPosition) == sourceIndexMap.end())
			lplog(LOG_FATAL_ERROR, L"beginObjectPosition has illegal value of %d!", beginObjectPosition);
	adjustValue(beginObjectPosition, L"beginObjectPosition", sourceIndexMap);
	if (endObjectPosition >= 0)
	{
		if (sourceIndexMap.find(endObjectPosition - 1) == sourceIndexMap.end())
			lplog(LOG_FATAL_ERROR, L"endObjectPosition has illegal value of %d!", endObjectPosition);
		endObjectPosition = sourceIndexMap[endObjectPosition - 1] + 1;
	}
	adjustValue(relPrep, L"relPrep", sourceIndexMap);
	adjustValue(relObject, L"relObject", sourceIndexMap);
	adjustValue(relSubject, L"relSubject", sourceIndexMap);
	adjustValue(relVerb, L"relVerb", sourceIndexMap);
	adjustValue(nextQuote, L"nextQuote", sourceIndexMap);
	adjustValue(principalWherePosition, L"principalWherePosition", sourceIndexMap);
	beginPEMAPosition = -1;
	endPEMAPosition = -1;
	keepObjects = false;
	//if (!keepObjects)
	//{
	//	setObject(-1);
	//	objectMatches.clear();
	//}
	if (logQuestionDetail)
		lplog(LOG_WHERE, L"%d:COPY CHILD->PARENT %s beginObjectPosition=%d endObjectPosition=%d relSubject=%d relVerb=%d relPrep=%d relObject=%d nextQuote=%d principalWherePosition=%d",
			index, word->first.c_str(), beginObjectPosition, endObjectPosition, relSubject, relVerb, relPrep, getRelObject(), nextQuote, principalWherePosition);
}

void cWordMatch::adjustReferences(int index, int offset)
{
	LFS
		if (beginObjectPosition >= 0)
			beginObjectPosition += offset;
	if (endObjectPosition >= 0)
		endObjectPosition += offset;
	if (relPrep >= 0)
		relPrep += offset;
	if (getRelObject() >= 0)
		setRelObject(getRelObject() + offset);
	if (relSubject >= 0)
		relSubject += offset;
	if (getRelVerb() >= 0)
		setRelVerb(getRelVerb() + offset);
	if (nextQuote >= 0)
		nextQuote += offset;
	if (principalWherePosition >= 0)
		principalWherePosition += offset;
	beginPEMAPosition = -1;
	endPEMAPosition = -1;
	if (logQuestionDetail)
	{
		lplog(LOG_WHERE, L"parentWhere %d:COPY SELF %s beginObjectPosition=%d endObjectPosition=%d relSubject=%d relVerb=%d relPrep=%d relObject=%d nextQuote=%d principalWherePosition=%d offset=%d",
			index, word->first.c_str(), beginObjectPosition, endObjectPosition, relSubject, relVerb, relPrep, getRelObject(), nextQuote, principalWherePosition, offset);
	}
}

// copy the answer into the parent
vector <int> cSource::copyChildrenIntoParent(cSource* childSource, int whereChild, unordered_map <int, int>& sourceIndexMap, bool enclosingLoop)
{
	LFS
		vector <int> parentLocations;
	if (childSource->m[whereChild].getObject() < 0 || childSource->m[whereChild].beginObjectPosition < 0 || childSource->m[whereChild].endObjectPosition < 0)
	{
		sourceIndexMap[whereChild] = m.size();
		m.push_back(childSource->m[whereChild]);
		parentLocations.push_back(m.size() - 1);
		wstring tmpstr, tmpstr2;
		lplog(LOG_WHERE, L"Transferred %d:%s to %d:%s", whereChild, childSource->whereString(whereChild, tmpstr, true).c_str(), m.size() - 1, whereString(m.size() - 1, tmpstr2, true).c_str());
	}
	else
	{
		vector <cOM> childObjects;
		if (childSource->m[whereChild].objectMatches.size() > 0)
			childObjects = childSource->m[whereChild].objectMatches;
		else
			childObjects.push_back(cOM(childSource->m[whereChild].getObject(), 0));
		int lowestBeginPosition = m.size();
		int highestEndPosition = -1;
		for (cOM coOM : childObjects)
		{
			int co = coOM.object;
			int whereObject = childSource->objects[co].originalLocation;
			// cases of object overlap:
			// 0. object is local to this position, so this object's positions will be copied already into parent - only applicable if this is in an enclosing loop iterating over source positions.
			// 1. object beginObjectPosition > the largest endObjectPosition
			// 2. object endObjectPosition < the smallest beginObjectPosition
			// 3. all positions must be scanned.
			bool makeCopy = false, skipObject = false;
			// object is local to this position, so this object's positions will be copied already into parent.
			if (enclosingLoop && childSource->m[whereObject].beginObjectPosition <= whereObject && childSource->m[whereObject].endObjectPosition > whereObject)
			{
				makeCopy = false;
				skipObject = false;
				copySource(childSource, whereChild, whereChild + 1, sourceIndexMap); // copy just this additional word.
			}
			// case 1
			else if (childSource->m[whereObject].beginObjectPosition > highestEndPosition || childSource->m[whereObject].endObjectPosition < lowestBeginPosition)
			{
				makeCopy = true;
				skipObject = false;
			}
			else
			{
				int overlappedPositions = 0;
				for (int I = childSource->m[whereObject].beginObjectPosition; I < childSource->m[whereObject].endObjectPosition; I++)
					if (sourceIndexMap.find(I) != sourceIndexMap.end())
						overlappedPositions++;
				// in between all the other objects but not overlapping any
				if (overlappedPositions == 0)
				{
					makeCopy = true;
					skipObject = false;
				}
				// overlapping completely, so copy has already been made
				else if (overlappedPositions == childSource->m[whereObject].endObjectPosition - childSource->m[whereObject].beginObjectPosition)
				{
					makeCopy = false;
					skipObject = false;
				}
				else
				{
					makeCopy = false;
					skipObject = true;
				}
			}
			if (skipObject)
			{
				wstring tmpstr, tmpstr2;
				lplog(LOG_WHERE, L"Did NOT transfer %d:%s", whereObject, childSource->objectString(co, tmpstr, true).c_str());
				continue;
			}
			lowestBeginPosition = min(lowestBeginPosition, childSource->m[whereObject].beginObjectPosition);
			highestEndPosition = max(highestEndPosition, childSource->m[whereObject].endObjectPosition);
			// copy the words associated with the childObject
			if (makeCopy)
				copySource(childSource, childSource->m[whereObject].beginObjectPosition, childSource->m[whereObject].endObjectPosition, sourceIndexMap);
			// copy childObject
			objects.push_back(childSource->objects[co]);
			int whereParentObject;
			if (sourceIndexMap.find(whereObject) == sourceIndexMap.end())
			{
				if (makeCopy)
					lplog(LOG_FATAL_ERROR, L"whereObject not found when making copy of object.");
				else
					whereParentObject = sourceIndexMap[whereObject - 1] + 1; // if not making copy, and whereObject not found, then case 0 must apply (see above)
			}
			else
				whereParentObject = sourceIndexMap[whereObject];
			parentLocations.push_back(whereParentObject);
			int parentObject = objects.size() - 1;
			objects[parentObject].begin = whereParentObject - (whereObject - childSource->m[whereObject].beginObjectPosition);
			objects[parentObject].end = whereParentObject + (childSource->m[whereObject].endObjectPosition - whereObject);
			m[whereParentObject].setObject(parentObject);
			if (childSource->objects[co].getOwnerWhere() >= 0)
			{
				if (sourceIndexMap.find(childSource->objects[co].getOwnerWhere()) == sourceIndexMap.end())
				{
					sourceIndexMap[childSource->objects[co].getOwnerWhere()] = m.size();
					m.push_back(childSource->m[childSource->objects[co].getOwnerWhere()]);
					int I = m.size() - 1;
					objects[parentObject].setOwnerWhere(I);
				}
				else
					objects[parentObject].setOwnerWhere(sourceIndexMap[childSource->objects[co].getOwnerWhere()]);
			}
			objects[parentObject].originalLocation = whereParentObject;
			objects[parentObject].relativeClausePM = -1; // should copy later
			objects[parentObject].whereRelativeClause = -1; // should copy later
			objects[parentObject].locations.clear();
			objects[parentObject].replacedBy = -1;
			objects[parentObject].duplicates.clear();
			objects[parentObject].eliminated = false;
			if (makeCopy)
				copyDirectlyAttachedPrepositionalPhrases(whereParentObject, childSource, whereObject, sourceIndexMap, false);
			wstring tmpstr, tmpstr2;
			lplog(LOG_WHERE, L"Transferred %d:%s (and associated objects) to %d:%s [%s]", whereObject, childSource->objectString(co, tmpstr, true).c_str(), whereParentObject, whereString(whereParentObject, tmpstr2, true).c_str(), (makeCopy) ? L"copy made" : L"no copy made");
			// negative objects allowed in some cases, so make it REALLY negative (past wordOrderWords and eOBJECTS)
			lplog(LOG_WHERE, L"Setting negative object - %d object %d->%d!", whereParentObject, parentObject, -parentObject - 1000);
			m[whereParentObject].setObject(-parentObject - 1000); // set to negative - will be reversed by scan which must always be called after this when clear is false - negative objects are allowed in some cases SON[setObjectNegative]
		}
	}
	return parentLocations;
}

int cSource::detectAttachedPhrase(cSyntacticRelationGroup* srg, int& relVerb)
{
	LFS
		int collectionWhere = srg->whereQuestionTypeObject;
	if (m[collectionWhere].beginObjectPosition >= 0 && m[m[collectionWhere].beginObjectPosition].pma.queryPatternDiff(L"__NOUN", L"F") != -1 && (relVerb = m[collectionWhere].getRelVerb()) >= 0 && m[relVerb].relSubject == collectionWhere)
		return 0;
	return -1;
}

bool cSource::compareObjectString(int whereObject1, int whereObject2)
{
	LFS
		wstring whereObject1Str, whereObject2Str;
	whereString(whereObject1, whereObject1Str, true);
	whereString(whereObject2, whereObject2Str, true);
	return whereObject1Str == whereObject2Str;
}

bool cSource::objectContainedIn(int whereObject, set <int> whereObjects)
{
	LFS
		for (set <int>::iterator woi = whereObjects.begin(), woiEnd = whereObjects.end(); woi != woiEnd; woi++)
		{
			if (compareObjectString(whereObject, *woi))
				return true;
			for (unsigned int I = 0; I < m[*woi].objectMatches.size(); I++)
				if (compareObjectString(whereObject, objects[m[*woi].objectMatches[I].object].originalLocation))
					return true;
		}
	return false;
}

void cSource::initializePemaMap(size_t numTagSets)
{
	unordered_map <int, vector < vector <cTagLocation> > > emptyMap;
	pemaMapToTagSetsByPemaByTagSet.reserve(numTagSets);
	for (unsigned int ts = 0; ts < numTagSets; ts++)
		pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
}

// correct from Stanford analysis - see specials_main::ruleCorrectLPClass
// see if LP class can be corrected.
// return code:
//   -1: LP class corrected.  ST prefers something other than correct class, so LP is correct
//   -2: LP class corrected.  ST prefers correct class, so this entry should simply be removed from the output file.
//   -3: test whether to change to correct class - set to disagree, and add an arbitrary string to partofspeech to search for whatever string added as a test.
//    0: unable to determine whether class should be corrected, or the class has been corrected. Normal processing should continue.  
int cSource::ruleCorrectLPClass(int wordSourceIndex, int startOfSentence)
{
	if (wordSourceIndex + 1 >= m.size())
		return 0;
	int adverbFormOffset = m[wordSourceIndex].word->second.query(adverbForm);
	int adjectiveFormOffset = m[wordSourceIndex].word->second.query(adjectiveForm);
	int particleFormOffset = m[wordSourceIndex].word->second.query(particleForm);
	int nounPlusOneFormOffset = m[wordSourceIndex + 1].word->second.query(nounForm);
	int conjunctionFormOffset = m[wordSourceIndex].word->second.query(conjunctionForm);
	// RULE CHANGE - change an adjective to an adverb?
	if (m[wordSourceIndex].word->first != L"that" && // 'that' is very ambiguous
		m[wordSourceIndex].isOnlyWinner(adjectiveForm) &&
		m[wordSourceIndex + 1].queryWinnerForm(L"noun") < 0 &&
		m[wordSourceIndex + 1].queryWinnerForm(L"Proper Noun") < 0 &&
		m[wordSourceIndex + 1].queryWinnerForm(L"indefinite_pronoun") < 0 &&
		m[wordSourceIndex + 1].queryWinnerForm(L"numeral_cardinal") < 0 &&
		m[wordSourceIndex + 1].queryWinnerForm(L"adjective") < 0 && // only an adjective, not before a noun
		(nounPlusOneFormOffset < 0 || m[wordSourceIndex + 1].word->second.getUsageCost(nounPlusOneFormOffset) == 4) &&
		(adverbFormOffset = m[wordSourceIndex].queryForm(L"adverb")) >= 0 && m[wordSourceIndex].word->second.getUsageCost(adverbFormOffset) < 2 &&
		m[wordSourceIndex].queryForm(L"interjection") < 0 && // interjection acts similarly to adverb
		(iswalpha(m[wordSourceIndex + 1].word->first[0]) || wordSourceIndex == 0 || iswalpha(m[wordSourceIndex - 1].word->first[0])) && // not alone in the sentence
		(wordSourceIndex <= 0 || (m[wordSourceIndex - 1].queryForm(L"is") < 0 && m[wordSourceIndex - 1].word->first != L"be" && m[wordSourceIndex - 1].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 1 || (m[wordSourceIndex - 2].queryForm(L"is") < 0 && m[wordSourceIndex - 2].word->first != L"be" && m[wordSourceIndex - 2].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 2 || (m[wordSourceIndex - 3].queryForm(L"is") < 0 && m[wordSourceIndex - 3].word->first != L"be" && m[wordSourceIndex - 3].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex <= 3 || (m[wordSourceIndex - 4].queryForm(L"is") < 0 && m[wordSourceIndex - 4].word->first != L"be" && m[wordSourceIndex - 4].word->first != L"being")) && // is/ishas before means it really is an adjective!
		(wordSourceIndex < m.size() - 1 || (m[wordSourceIndex + 1].queryForm(L"is") < 0 && m[wordSourceIndex + 1].word->first != L"be")) && // is/ishas before means it really is an adjective!
		(m[wordSourceIndex].queryForm(L"preposition") < 0) && (m[wordSourceIndex].queryForm(L"relativizer") < 0)) // || m[wordSourceIndex + 1].queryWinnerForm(L"numeral_cardinal") < 0)) // before one o'clock
	{
		bool isDeterminer = false;
		if (wordSourceIndex > 0)
		{
			vector<wstring> determinerTypes = { L"determiner",L"demonstrative_determiner",L"possessive_determiner",L"interrogative_determiner", L"quantifier", L"numeral_cardinal" };
			for (wstring dt : determinerTypes)
				if (isDeterminer = m[wordSourceIndex - 1].queryWinnerForm(dt) >= 0)
					break;
		}
		// The door that faced her stood *open*
		if (!isDeterminer && !m[wordSourceIndex].forms.isSet(PROPER_NOUN_FORM_NUM) && wordSourceIndex > 0 && m[wordSourceIndex - 1].queryWinnerForm(L"verb") >= 0 && adverbFormOffset >= 0 && adjectiveFormOffset >= 0) // Proper Noun is already well controlled
		{
			m[wordSourceIndex].setWinner(adverbFormOffset);
			m[wordSourceIndex].unsetWinner(adjectiveFormOffset);
			return -1;
		}
	}
	// RULE CHANGE - change an adverb to an adjective?
	if (m[wordSourceIndex].isOnlyWinner(adverbForm) && adjectiveFormOffset >= 0 && m[wordSourceIndex].word->second.getUsageCost(adverbFormOffset) - m[wordSourceIndex].word->second.getUsageCost(adjectiveFormOffset) >= 3 &&
		m[wordSourceIndex + 1].queryWinnerForm(determinerForm) >= 0 && wordSourceIndex > 0 && m[wordSourceIndex - 1].queryWinnerForm(verbForm) < 0)
	{
		m[wordSourceIndex].setWinner(adjectiveFormOffset);
		m[wordSourceIndex].unsetWinner(adverbFormOffset);
		return -1;
	}
	// cannot be preposition, conjunction, verb, determiner, particle
	if (m[wordSourceIndex].isOnlyWinner(adverbForm) && adjectiveFormOffset >= 0 &&
		(m[wordSourceIndex + 1].queryWinnerForm(nounForm) >= 0 || m[wordSourceIndex + 1].queryWinnerForm(L"dayUnit") >= 0) &&
		m[wordSourceIndex + 1].queryWinnerForm(adjectiveForm) < 0 &&
		wordSourceIndex > 0 &&
		m[wordSourceIndex - 1].queryWinnerForm(verbForm) < 0 &&
		m[wordSourceIndex].queryForm(prepositionForm) < 0 &&
		m[wordSourceIndex].queryForm(conjunctionForm) < 0 &&
		m[wordSourceIndex].queryForm(verbForm) < 0 &&
		m[wordSourceIndex].queryForm(determinerForm) < 0 &&
		m[wordSourceIndex].queryForm(particleForm) < 0 &&
		m[wordSourceIndex + 2].word->first != L"-" && // There is no getting in or out of them without the greatest difficulty , and a patient , slow navigation , which is *very* heart - rending .
		queryPattern(wordSourceIndex, L"_TIME") == -1) // An hour *later* supper was served . 
	{
		// LP correct - 1039
		// ST correct - 19 < 2%
		m[wordSourceIndex].setWinner(adjectiveFormOffset);
		m[wordSourceIndex].unsetWinner(adverbFormOffset);
		return -1;
	}
	if (wordSourceIndex < m.size() - 3 && m[wordSourceIndex].word->first == L"most" &&
		(m[wordSourceIndex + 1].hasWinnerNounForm() ||
			(m[wordSourceIndex + 1].word->first == L"of" &&
				(m[wordSourceIndex + 2].word->first == L"the" || m[wordSourceIndex + 2].queryWinnerForm(demonstrativeDeterminerForm) != -1 || m[wordSourceIndex + 2].queryWinnerForm(possessiveDeterminerForm) != -1 || m[wordSourceIndex + 2].queryWinnerForm(interrogativeDeterminerForm) != -1))
			))
	{
		m[wordSourceIndex].setWinner(adjectiveFormOffset);
		m[wordSourceIndex].unsetAllFormWinners();
		return -1;
	}
	if (m[wordSourceIndex].word->first == L"only")
	{
		if (m[wordSourceIndex + 1].pma.queryPattern(L"__S1") != -1)
		{
			if (wordSourceIndex == startOfSentence || wordSourceIndex == startOfSentence + 1)
			{
				m[wordSourceIndex].setWinner(adverbFormOffset);
				m[wordSourceIndex].unsetAllFormWinners();
				return -1;
			}
			else
			{
				m[wordSourceIndex].setWinner(conjunctionFormOffset);
				m[wordSourceIndex].unsetAllFormWinners();
				return -1;

			}
		}
		else if (m[wordSourceIndex + 1].pma.queryPattern(L"__INFP") != -1)
		{
			m[wordSourceIndex].setWinner(adverbFormOffset);
			m[wordSourceIndex].unsetAllFormWinners();
			return -1;
		}
		else if (m[wordSourceIndex + 1].queryWinnerForm(determinerForm) != -1)
		{
			m[wordSourceIndex].setWinner(adjectiveFormOffset);
			m[wordSourceIndex].unsetAllFormWinners();
			return -1;
		}
		return 0;
	}
	if (m[wordSourceIndex].word->first == L"better" || m[wordSourceIndex].word->first == L"further")
	{
		bool sentenceOfBeing =				// 4 words or less before the word must be an 'is' verb
			((wordSourceIndex <= 0 || (m[wordSourceIndex - 1].queryForm(L"is") >= 0 || m[wordSourceIndex - 1].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 1 || (m[wordSourceIndex - 2].queryForm(L"is") >= 0 || m[wordSourceIndex - 2].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 2 || (m[wordSourceIndex - 3].queryForm(L"is") >= 0 || m[wordSourceIndex - 3].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 3 || (m[wordSourceIndex - 4].queryForm(L"is") >= 0 || m[wordSourceIndex - 4].queryForm(L"be") >= 0))); // is/ishas before means it really is an adjective!
		if (m[wordSourceIndex].word->first == L"better" && sentenceOfBeing && m[wordSourceIndex + 1].queryWinnerForm(verbForm) == -1)
		{
			m[wordSourceIndex].setWinner(adjectiveFormOffset);
			m[wordSourceIndex].unsetAllFormWinners();
			return -1;
		}
		else if (m[wordSourceIndex].word->first == L"better" && m[wordSourceIndex + 1].queryWinnerForm(nounForm) == -1 && m[wordSourceIndex + 1].queryWinnerForm(determinerForm) == -1)
		{
			m[wordSourceIndex].setWinner(adverbFormOffset);
			m[wordSourceIndex].unsetAllFormWinners();
			return -1;
		}
	}
	if (m[wordSourceIndex].isOnlyWinner(prepositionForm) && m[wordSourceIndex].getRelObject() < 0 && !iswalpha(m[wordSourceIndex + 1].word->first[0]))
	{
		int relVerb = m[wordSourceIndex].getRelVerb();
		bool sentenceOfBeing =				// 4 words or less before the word must be an 'is' verb
			((wordSourceIndex <= 0 || (m[wordSourceIndex - 1].queryForm(L"is") >= 0 || m[wordSourceIndex - 1].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 1 || (m[wordSourceIndex - 2].queryForm(L"is") >= 0 || m[wordSourceIndex - 2].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 2 || (m[wordSourceIndex - 3].queryForm(L"is") >= 0 || m[wordSourceIndex - 3].queryForm(L"be") >= 0)) || // is/ishas before means it really is an adjective!
				(wordSourceIndex <= 3 || (m[wordSourceIndex - 4].queryForm(L"is") >= 0 || m[wordSourceIndex - 4].queryForm(L"be") >= 0))); // is/ishas before means it really is an adjective!
		if (adverbFormOffset < 0)
		{
			if (!(cWord::isSingleQuote(m[wordSourceIndex + 1].word->first[0]) || cWord::isDoubleQuote(m[wordSourceIndex + 1].word->first[0])) && m[wordSourceIndex].word->first == L"to")
			{
				return -1;
			}
			if (m[wordSourceIndex].word->first == L"like" && sentenceOfBeing &&
				// the word before must NOT be a dash
				(wordSourceIndex <= 0 || !cWord::isDash((m[wordSourceIndex - 1].word->first[0]))))
			{
				m[wordSourceIndex].setWinner(adjectiveFormOffset);
				m[wordSourceIndex].unsetWinner(m[wordSourceIndex].queryForm(prepositionForm));
				return -1;
			}
			else
				return 0; // In other cases the STLPMatch is already a preposition, so ST and LP agree anyway
		}
		else
		{
			wstring nextWord = m[wordSourceIndex + 1].word->first;
			if (nextWord == L"." || nextWord == L"," || nextWord == L";" || nextWord == L"--")
			{
				if (relVerb >= 0 && (m[relVerb].queryForm(L"is") >= 0 || m[relVerb].queryForm(L"be") >= 0))
				{
					if (adjectiveFormOffset < 0)
					{
						if (particleFormOffset > 0 && !cWord::isDash((m[wordSourceIndex + 1].word->first[0])))
						{
							m[wordSourceIndex].setWinner(particleFormOffset);
							m[wordSourceIndex].unsetWinner(m[wordSourceIndex].queryForm(prepositionForm));
							return -1;
						}
						if (adverbFormOffset >= 0)
						{
							m[wordSourceIndex].setWinner(adverbFormOffset);
							m[wordSourceIndex].unsetWinner(m[wordSourceIndex].queryForm(prepositionForm));
							return -1;
						}
						return 0;
					}
					//if (relVerb >= 0)
					//	partofspeech += m[relVerb].word->first;
					m[wordSourceIndex].setWinner(adjectiveFormOffset);
					m[wordSourceIndex].unsetWinner(m[wordSourceIndex].queryForm(prepositionForm));
					return -1;
				}
				else
				{
					m[wordSourceIndex].setWinner(adverbFormOffset);
					m[wordSourceIndex].unsetWinner(m[wordSourceIndex].queryForm(prepositionForm));
					return -1;
				}
			}
			else
			{
				if (particleFormOffset > 0 && !cWord::isDash((m[wordSourceIndex + 1].word->first[0])))
				{
					m[wordSourceIndex].setWinner(particleFormOffset);
					m[wordSourceIndex].unsetWinner(m[wordSourceIndex].queryForm(prepositionForm));
					return -1;
				}
				return 0;
			}
		}
	}
	if (m[wordSourceIndex].word->first == L"that" && (((m[wordSourceIndex].flags & cWordMatch::flagInQuestion) && wordSourceIndex > 0 &&
		wordSourceIndex > 0 && (m[wordSourceIndex - 1].queryForm(L"is") >= 0 || m[wordSourceIndex - 1].queryForm(L"is_negation") >= 0) &&
		m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0 && m[wordSourceIndex + 1].queryWinnerForm(nounForm) < 0) ||
		(m[wordSourceIndex].word->first == L"that" && !(m[wordSourceIndex].flags & cWordMatch::flagInQuestion) && wordSourceIndex > 0 && (m[wordSourceIndex + 1].queryForm(L"is") >= 0 || m[wordSourceIndex + 1].queryForm(L"is_negation") >= 0) &&
			(!iswalpha(m[wordSourceIndex - 1].word->first[0]) || wordSourceIndex == startOfSentence)) ||
		(m[wordSourceIndex].queryWinnerForm(demonstrativeDeterminerForm) >= 0 && !iswalpha(m[wordSourceIndex + 1].word->first[0]))))
	{
		m[wordSourceIndex].setWinner(m[wordSourceIndex].queryForm(pronounForm));
		m[wordSourceIndex].unsetWinner(m[wordSourceIndex].queryForm(demonstrativeDeterminerForm));
	}
	// a word which LP thinks is an adverb, which is before a determiner, which has a predeterminer form
	if (m[wordSourceIndex].isOnlyWinner(adverbForm) && m[wordSourceIndex + 1].isOnlyWinner(determinerForm) && m[wordSourceIndex].queryForm(predeterminerForm) != -1)
	{
		m[wordSourceIndex].setWinner(m[wordSourceIndex].queryForm(predeterminerForm));
		m[wordSourceIndex].unsetWinner(adverbFormOffset);
		return -2;
	}
	int primaryPMAOffset = m[wordSourceIndex].pma.queryPattern(L"__ALLOBJECTS_1");
	int secondaryPMAOffset = m[wordSourceIndex].pma.queryPattern(L"_ADVERB");
	if (primaryPMAOffset != -1 && secondaryPMAOffset != -1)
	{
		primaryPMAOffset = primaryPMAOffset & ~cMatchElement::patternFlag;
		secondaryPMAOffset = secondaryPMAOffset & ~cMatchElement::patternFlag;
		set <wstring> particles = { L"down",L"out",L"off",L"up" };
		if (particles.find(m[wordSourceIndex].word->first) == particles.end() && m[wordSourceIndex].word->second.getUsageCost(m[wordSourceIndex].queryForm(prepositionForm)) < 4 && m[wordSourceIndex].pma[secondaryPMAOffset].len == 1 && queryPattern(wordSourceIndex + 1, L"__NOUN") != -1 && m[wordSourceIndex].queryForm(prepositionForm) != -1)
		{
			m[wordSourceIndex].setWinner(m[wordSourceIndex].queryForm(prepositionForm));
			m[wordSourceIndex].unsetWinner(adverbFormOffset);
			return 0;
		}
	}
	set <wstring> notObjects = { L"we",L"i",L"he",L"they" };
	if (wordSourceIndex < m.size() - 2 && m[wordSourceIndex + 1].hasWinnerNounForm() && m[wordSourceIndex].isOnlyWinner(adverbForm) &&
		m[wordSourceIndex].queryForm(prepositionForm) != -1 && m[wordSourceIndex].word->first != L"as" && m[wordSourceIndex + 1].queryWinnerForm(PROPER_NOUN_FORM) == -1)
	{
		if (notObjects.find(m[wordSourceIndex + 1].word->first) == notObjects.end() &&
			(!iswalpha(m[wordSourceIndex + 2].word->first[0]) || m[wordSourceIndex + 2].queryWinnerForm(coordinatorForm) != -1 || m[wordSourceIndex + 2].queryWinnerForm(determinerForm) != -1))
		{
			m[wordSourceIndex].setWinner(m[wordSourceIndex].queryForm(prepositionForm));
			m[wordSourceIndex].unsetWinner(adverbFormOffset);
			return -1;
		}
		if (notObjects.find(m[wordSourceIndex + 1].word->first) != notObjects.end() && (conjunctionFormOffset = m[wordSourceIndex].queryForm(conjunctionForm)) != -1)
		{
			m[wordSourceIndex].setWinner(conjunctionFormOffset);
			m[wordSourceIndex].unsetWinner(adverbFormOffset);
			return -1;
		}
	}
	int nounPMAIndex = -1;
	if (adverbFormOffset >= 0 && wordSourceIndex > 0 && (nounPMAIndex = m[wordSourceIndex - 1].pma.queryPatternDiff(L"__NOUN", L"2")) != -1 && m[wordSourceIndex - 1].word->first == L"the" && m[wordSourceIndex - 1].pma[nounPMAIndex & ~cMatchElement::patternFlag].len == 3 &&
		m[wordSourceIndex + 1].pma.queryPattern(L"_ADJECTIVE_AFTER") != -1)
	{
		m[wordSourceIndex].unsetAllFormWinners();
		m[wordSourceIndex].setWinner(adverbFormOffset);
		return 0;
	}
	return 0;
}

