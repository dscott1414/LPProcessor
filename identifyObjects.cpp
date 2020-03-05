#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "fcntl.h"
#include "sys/stat.h"
#include <wn.h>
#include "profile.h"
#include "bitObject.h"
#define MAX_BUF 10240000

bool Source::findSpecificAnaphor(wstring tagName, int where, int element, int &specificWhere, bool &pluralNounOverride, bool &embeddedName)
{
	LFS
		specificWhere = where;
	if (!(element&matchElement::patternFlag)) return true;
	if (tagName == L"NOUN" || tagName == L"VNOUN" || tagName == L"ADJOBJECT")
	{
		vector < vector <tTagLocation> > tagSets;
		patternMatchArray::tPatternMatch *pm = m[where].pma.content + (element&~matchElement::patternFlag);
		if (!startCollectTags(debugTrace.traceAnaphors, specificAnaphorTagSet, where, pm->pemaByPatternEnd, tagSets, true, false,L"find specific anaphor")) return false;
		for (unsigned int J = 0; J < tagSets.size(); J++)
		{
			if (debugTrace.traceAnaphors)
				printTagSet(LOG_RESOLUTION, L"SA", J, tagSets[J], where, pm->pemaByPatternEnd);
			int whereSubject = -1, nextSubject = -1;
			int whereNoun = -1, whereGNoun = -1, nextNoun = -1, nextGNoun = -1, nextPlural = -1;
			if (tagName == L"VNOUN")
			{
				if ((whereSubject = findTag(tagSets[J], L"SUBJECT", nextSubject)) >= 0)
				{
					whereNoun = findTagConstrained(tagSets[J], L"N_AGREE", nextNoun, tagSets[J][whereSubject]);
					if (whereNoun < 0) whereGNoun = findTagConstrained(tagSets[J], L"GNOUN", nextGNoun, tagSets[J][whereSubject]);
					if (whereGNoun < 0) whereGNoun = findTagConstrained(tagSets[J], L"MNOUN", nextGNoun, tagSets[J][whereSubject]);
				}
				if (whereNoun < 0 && whereGNoun < 0)
				{
					int nextVerb = -1, whereVerb = findTag(tagSets[J], L"V_AGREE", nextVerb);
					if (whereVerb >= 0)
					{
						specificWhere = tagSets[J][whereVerb].sourcePosition;
						if (debugTrace.traceAnaphors)
							lplog(LOG_RESOLUTION, L"%06d:Search for specific anaphor returned V_AGREE=(%d,%d) specificWhere=%d plural=%s.",
						where, whereVerb, nextVerb, specificWhere, (pluralNounOverride) ? L"TRUE" : L"FALSE");
						return true;
					}
					else
						continue;
				}
			}
			else
			{
				whereNoun = findTag(tagSets[J], L"N_AGREE", nextNoun);
				if (whereNoun < 0) whereGNoun = findTag(tagSets[J], L"GNOUN", nextGNoun);
				if (whereGNoun < 0) whereGNoun = findTag(tagSets[J], L"MNOUN", nextGNoun);
				if (whereNoun < 0 && whereGNoun < 0) continue;
			}
			if (whereNoun < 0) whereNoun = whereGNoun;
			embeddedName = whereGNoun >= 0 && pema[abs(tagSets[J][whereGNoun].PEMAOffset)].hasTag(NAME_TAG);
			specificWhere = tagSets[J][whereNoun].sourcePosition;
			// DEAR HERSHEIMMER
			if (!embeddedName && specificWhere != where && tagName == L"NOUN" && m[specificWhere].pma.queryTag(NAME_TAG))
				embeddedName = true;
			// the small lift-boy here...
			if ((m[specificWhere].word->first == L"here" || m[specificWhere].word->first == L"there") && where < specificWhere &&
				m[specificWhere - 1].queryWinnerForm(nounForm) >= 0)
				specificWhere--;
			// a huge bus bearing down on us
			if (specificWhere - where > 2 && m[specificWhere].queryWinnerForm(verbForm) >= 0 && m[specificWhere - 1].queryWinnerForm(nounForm) >= 0 &&
				!(m[specificWhere - 1].word->second.inflectionFlags&(PLURAL_OWNER | SINGULAR_OWNER)) && !(m[specificWhere - 1].flags&WordMatch::flagNounOwner))
				specificWhere--;
			// the doctor most [of all] / least [of all]
			if (specificWhere - where >= 2 && m[specificWhere - 1].queryWinnerForm(nounForm) >= 0 &&
				(m[specificWhere].word->first == L"most" || m[specificWhere].word->first == L"more" || m[specificWhere].word->first == L"least" || m[specificWhere].word->first == L"less"))
				specificWhere--;
			pluralNounOverride = findTag(tagSets[J], L"PLURAL", nextPlural) >= 0;
			if (debugTrace.traceAnaphors)
				lplog(LOG_RESOLUTION, L"%06d:Search for specific anaphor returned N_AGREE=(%d,%d) GNOUN=(%d,%d) specificWhere=%d plural=%s embedded=%s.",
			where, whereNoun, nextNoun, whereGNoun, nextGNoun, specificWhere, (pluralNounOverride) ? L"TRUE" : L"FALSE", (embeddedName) ? L"TRUE" : L"FALSE");
			return true;
		}
	}
	return false;
}

// Lappin and Leass 2.1.2
// A quick glance shows these clearly do not cover many cases of pleonastic it
// MA ModalAdj:
//    necessary,possible,certain,likely,important,good,useful,advisable,convenient,
//    sufficient,economical,easy,desirable,difficult,legal - [should also add - surprising]
// COG Cogv-ed:
//    recommend think believe know anticipate assume expect
// MEANS:
//    seems appears means follows
// It is MA that S  [It is MA REL1]
// It is MA for (NP) to VP [It is MA (PP) INFP]
// It is COG that S [It is COG REL1]
// It is time to VP
// It is thanks to NP that S [It is thanks _PP _REL1]
// It MEANS (that) S [ It MEANS S1 or REL1]
// NP makes/finds it MA (for NP) to VP
bool Source::isPleonastic(unsigned int where)
{
	LFS
		if (m[where].word->first != L"it" || where + 4 > m.size()) return false;
	wchar_t *MA[] = { L"necessary",L"possible",L"certain",L"likely",L"important",L"good",L"useful",L"advisable",L"convenient",
	  L"sufficient",L"economical",L"easy",L"desirable",L"difficult",L"legal",L"surprising",NULL };
	if (m[where + 1].word->first == L"is")
	{
		int I;
		for (I = 0; MA[I] && m[where + 2].word->first != MA[I]; I++);
		if (MA[I])
		{
			if (m[where + 3].word->first == L"that" && m[where + 4].pma.queryPattern(L"__S1") != -1) return true;
			if (m[where + 3].word->first == L"for")
			{
				if (m[where + 4].pma.queryPattern(L"_INFP") != -1) return true;
				int tmpPP;
				if ((tmpPP = m[where + 4].pma.queryPattern(L"_PP")) == -1) return false;
				return m[where + 4 + m[where + 4].pma[tmpPP&~matchElement::patternFlag].len].pma.queryPattern(L"_INFP") != -1;
			}
			return false;
		}
		wchar_t *COG[] = { L"recommended",L"thought",L"believed",L"known",L"anticipated",L"assumed",L"expected",NULL };
		for (I = 0; COG[I] && m[where + 2].word->first != COG[I]; I++);
		if (COG[I] && m[where + 3].pma.queryPattern(L"_REL1") != -1) return true;
		if (m[where + 2].word->first == L"time") return true;
		if (m[where + 2].word->first == L"thanks" && m[where + 3].word->first == L"to")
		{
			int tmpPP;
			if ((tmpPP = m[where + 3].pma.queryPattern(L"_PP")) == -1) return false;
			return m[where + 3 + m[where + 3].pma[tmpPP&~matchElement::patternFlag].len].pma.queryPattern(L"_REL1") != -1;
		}
		return false;
	}
	// It MEANS (that) S [ It MEANS S1 or REL1]
	wchar_t *MEANS[] = { L"seems",L"appears",L"means",L"follows",NULL };
	int I;
	for (I = 0; MEANS[I] && m[where + 2].word->first != MEANS[I]; I++);
	if (MEANS[I] && (m[where + 3].pma.queryPattern(L"__S1") != -1 || m[where + 3].pma.queryPattern(L"_REL1") != -1)) return true;
	// NP makes/finds it MA (for NP) to VP
	for (I = 0; MA[I] && m[where + 1].word->first != MA[I]; I++);
	if (!MA[I] || where < 1 || (m[where - 1].word->first != L"makes" && m[where - 1].word->first != L"finds")) return false;
	return true;
}

bool Source::searchExactMatch(cObject &object, int position)
{
	LFS
		if (object.objectClass != NAME_OBJECT_CLASS &&
			object.objectClass != NON_GENDERED_NAME_OBJECT_CLASS &&
			object.objectClass != REFLEXIVE_PRONOUN_OBJECT_CLASS &&
			object.objectClass != RECIPROCAL_PRONOUN_OBJECT_CLASS)
			return false;
	set <int> relatedObjects;
	for (int I = object.begin; I < object.end; I++)
		relatedObjects.insert(relatedObjectsMap[m[I].word].begin(), relatedObjectsMap[m[I].word].end());

	set<int>::iterator s, sEnd = relatedObjects.end();
	for (s = relatedObjects.begin(); s != sEnd; s++)
		if (!object.eliminated && object.equals(objects[*s], m))
		{
			m[position].setObject(*s);
			objects[*s].locations.push_back(cObject::cLocation(object.originalLocation));
			objects[*s].updateFirstLocation(object.originalLocation);
			return true;
		}
	return false;
}

void Source::accumulateAdjective(const wstring &fromWord, set <wstring> &words, vector <tIWMM> &validList, bool isAdjective, wstring &aa, bool &containsMale, bool &containsFemale)
{
	LFS
		for (set <wstring>::iterator wi = words.begin(), wiEnd = words.end(); wi != wiEnd; wi++)
		{
			wstring tmp = *wi;
			bool properNounDetected = false;
			for (unsigned int I = 0; I < wi->length() && !properNounDetected; I++) if (iswupper((*wi)[I])) properNounDetected = true;
			if (properNounDetected) continue;
			int beginWord = 0, endWord = wi->find(' ');
			while (endWord != wstring::npos)
			{
				wstring subword = wi->substr(beginWord, endWord - beginWord);
				if (subword == L"male" || subword == L"female")
				{
					containsMale |= subword == L"male";
					containsFemale |= subword == L"female";
				}
				else
				{
					tIWMM exists = Words.query(subword);
					if (exists != Words.end() && find(validList.begin(), validList.end(), exists) == validList.end() &&
						exists->second.query(adjectiveForm) >= 0 && subword != fromWord)
					{
						validList.push_back(exists);
						aa += subword + L" ";
					}
				}
				beginWord = endWord + 1;
				endWord = wi->find(L' ', beginWord);
			}
			if (beginWord || !isAdjective) // if last word or noun
				continue;
			wstring subword = wi->substr(beginWord, wi->length());
			if (subword == L"male" || subword == L"female")
			{
				containsMale |= subword == L"male";
				containsFemale |= subword == L"female";
			}
			else
			{
				tIWMM exists = Words.query(subword);
				if (exists != Words.end() && find(validList.begin(), validList.end(), exists) == validList.end() &&
					exists->second.query(adjectiveForm) >= 0 && subword != fromWord)
				{
					validList.push_back(exists);
					aa += subword + L" ";
				}
			}
		}
}




void Source::addWNExtensions(void)
{
	LFS
		tIWMM w_tall = Words.query(L"tall"), w_small = Words.query(L"small"), mainEntry;
	if (w_tall != Words.end() && w_small != Words.end())
	{
		if ((mainEntry = w_tall->second.mainEntry) == wNULL) mainEntry = w_tall;
		if (!(mainEntry->second.flags&tFI::genericGenderIgnoreMatch) && wnAntonymsAdjectiveMap.find(mainEntry) == wnAntonymsAdjectiveMap.end())
			fillWNMaps(-1, mainEntry, true);
		wnAntonymsAdjectiveMap[w_tall].push_back(w_small);
		if ((mainEntry = w_small->second.mainEntry) == wNULL) mainEntry = w_small;
		if (!(mainEntry->second.flags&tFI::genericGenderIgnoreMatch) && wnAntonymsAdjectiveMap.find(mainEntry) == wnAntonymsAdjectiveMap.end())
			fillWNMaps(-1, mainEntry, true);
		wnAntonymsAdjectiveMap[w_small].push_back(w_tall);
	}
}

// if set through a first name that is male, or an honorific, make sure that head noun is set to a man or woman
// to make it equal with other objects that have matched to generic gendered objects
		// if set through a first name that is male, or an honorific, make sure that head noun is set to a man or woman
		// to make it equal with other objects that have matched to generic gendered objects
		// "Mr. Whittington" should match to "man" the same as "Tommy", even though Tommy may have acquired "man"
		// in a previous match.  Other gendered objects like "A young American" should not have "man" associated with them
		// even though it acquires male characteristics because "Tommy" will match against "young" and "man" and 
		// Julius against "man" and "American" and so both will be equal, even though "Julius" should be preferred.
void Source::addDefaultGenderedAssociatedNouns(int o)
{
	LFS
		if ((objects[o].objectClass == NAME_OBJECT_CLASS ||
			objects[o].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			objects[o].objectClass == GENDERED_DEMONYM_OBJECT_CLASS ||
			objects[o].objectClass == GENDERED_RELATIVE_OBJECT_CLASS) &&
			(objects[o].male ^ objects[o].female))
		{
			vector <tIWMM> *an = &objects[o].associatedNouns;
			if (objects[o].male)
			{
				if (find(an->begin(), an->end(), Words.gquery(L"man")) == an->end())
					an->push_back(Words.gquery(L"man"));
				if (find(an->begin(), an->end(), Words.gquery(L"fellow")) == an->end())
					an->push_back(Words.gquery(L"fellow"));
				if (find(an->begin(), an->end(), Words.gquery(L"gentleman")) == an->end())
					an->push_back(Words.gquery(L"gentleman"));
				if (find(an->begin(), an->end(), Words.gquery(L"chap")) == an->end())
					an->push_back(Words.gquery(L"chap"));
			}
			else
			{
				if (find(an->begin(), an->end(), Words.gquery(L"woman")) == an->end())
					an->push_back(Words.gquery(L"woman"));
				if (find(an->begin(), an->end(), Words.gquery(L"lady")) == an->end())
					an->push_back(Words.gquery(L"lady"));
				// Vendermeyer's cook 29055
				if (find(an->begin(), an->end(), Words.gquery(L"girl")) == an->end())
					an->push_back(Words.gquery(L"girl"));
			}
		}
}

void Source::fillWNMaps(int where, tIWMM word, bool isAdjective)
{
	LFS
		set <wstring> synonyms, antonyms;
	vector <tIWMM> wnSynonyms, wnAntonyms;
	int gender = 0;
	wstring aa;
	bool containsMale = false, containsFemale = false;
	getSynonyms(word->first, synonyms, (isAdjective) ? ADJ : NOUN);
	accumulateAdjective(word->first, synonyms, wnSynonyms, isAdjective, aa, containsMale, containsFemale);
	if (containsMale)
		gender |= MALE_GENDER;
	if (containsFemale)
		gender |= FEMALE_GENDER;
	if (aa.length() && debugTrace.traceSpeakerResolution)
		lplog(LOG_WORDNET, L"%d:SYN %s[%s]", where, word->first.c_str(), aa.c_str());
	aa.clear();
	if (isAdjective)
	{
		getAntonyms(word->first, antonyms, debugTrace);
		accumulateAdjective(word->first, antonyms, wnAntonyms, isAdjective, aa, containsMale, containsFemale);
	}
	if (wnAntonyms.empty())
	{
		// if no antonyms, get antonyms of more common synonyms
		// but these synonyms may be multiple words which won't be found again in WordNet
		int familiarity = getFamiliarity(word->first, isAdjective);
		for (set <wstring>::iterator si = synonyms.begin(), siEnd = synonyms.end(); si != siEnd; si++)
			if (getFamiliarity(*si, isAdjective) >= familiarity)
			{
				antonyms.clear();
				getAntonyms(*si, antonyms, debugTrace);
				accumulateAdjective(*si, antonyms, wnAntonyms, isAdjective, aa, containsMale, containsFemale);
			}
		// these synonyms are guaranteed to be found again in WordNet
		for (vector <tIWMM>::iterator si = wnSynonyms.begin(), siEnd = wnSynonyms.end(); si != siEnd; si++)
			if (synonyms.find((*si)->first) == synonyms.end() && getFamiliarity((*si)->first, true) >= familiarity)
			{
				antonyms.clear();
				getAntonyms((*si)->first, antonyms, debugTrace);
				accumulateAdjective((*si)->first, antonyms, wnAntonyms, true, aa, containsMale, containsFemale);
			}
	}
	if (aa.length() && debugTrace.traceSpeakerResolution)
		lplog(LOG_WORDNET, L"%d:ANT %s[%s]", where, word->first.c_str(), aa.c_str());
	if (isAdjective)
	{
		wnSynonymsAdjectiveMap[word] = wnSynonyms;
		wnGenderAdjectiveMap[word] = gender;
		wnAntonymsAdjectiveMap[word] = wnAntonyms;
	}
	else
	{
		wnSynonymsNounMap[word] = wnSynonyms;
		wnGenderNounMap[word] = gender;
		wnAntonymsNounMap[word] = wnAntonyms;
	}
}

// for each adjective leading up to principalWhere, look up
//    synonyms, antonyms
// for the principalWhere, for each sense, for each adjective associated with the sense, look up
//    synonyms, antonyms
void Source::accumulateAdjectives(int where)
{
	LFS
		wstring tmpstr;
	int adjectiveObject = m[where].getObject(), objectClass = (adjectiveObject >= 0) ? objects[adjectiveObject].objectClass : -1;
	bool originallyGendered, genderSet = false; // containsMale = false, containsFemale = false, 
	if (adjectiveObject >= 0 && objectClass != REFLEXIVE_PRONOUN_OBJECT_CLASS && objectClass != RECIPROCAL_PRONOUN_OBJECT_CLASS &&
		// block 'The little he knew...'
		(objectClass != PRONOUN_OBJECT_CLASS || ((m[where].endObjectPosition - m[where].beginObjectPosition) > 1 && m[where].queryWinnerForm(personalPronounForm) < 0)))
	{
		originallyGendered = objects[adjectiveObject].male ^ objects[adjectiveObject].female;
		int endNameAdjectives = m[where].endObjectPosition;
		if (objectClass == NAME_OBJECT_CLASS)
		{
			if (objects[adjectiveObject].name.justHonorific())
			{
				addDefaultGenderedAssociatedNouns(adjectiveObject);
				return;
			}
			vector <cObject>::iterator object = objects.begin() + adjectiveObject;
			// that Russian chap Kramenin
			for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition; I++)
			{
				if (m[I].word->second.flags&tFI::genericGenderIgnoreMatch)
				{
					endNameAdjectives = I;
					break;
				}
				if (object->name.first == m[I].word || object->name.any == m[I].word)
					break;
			}
			if (endNameAdjectives == m[where].endObjectPosition)
			{
				addDefaultGenderedAssociatedNouns(adjectiveObject);
				return;
			}
		}
		for (int I = m[where].beginObjectPosition; I < endNameAdjectives; I++)
		{
			bool isAdjective = m[I].queryWinnerForm(adjectiveForm) >= 0 && m[I].queryForm(commonProfessionForm) < 0;
			// demonym could be a proper noun (but still should be associated)
			// example:  the young American
			if (!isAdjective && m[I].queryWinnerForm(nounForm) < 0 && m[I].queryForm(demonymForm) < 0)
			{
				// an Irish Sinn Feiner - Irish should still be associated
				if (m[I].word->second.query(adjectiveForm) < 0) continue;
				int flc = m[I].word->second.getLowestCost();
				if (flc < 0 || (m[I].word->second.forms()[flc] != nounForm && m[I].word->second.forms()[flc] != adjectiveForm))
					continue;
			}
			// because body objects can be associated with people, only associate adjectives that are definitely 
			// associated only with people.  Other adjectives are only associated with the object 'big' eyes should not be associated with 'big' man
			// otherwise 'close' 'cropped' hair and 'dear' Boris will be associated with one another
			// but the 'Irish' voice and 'Irish' Sein Fiener should still become associated
			//if (objectClass==BODY_OBJECT_CLASS && m[I].queryForm(demonymForm)<0 && m[I].queryForm(PROPER_NOUN_FORM_NUM)<0)
			//	continue; 
			// don't associate 'the other brother' with 'other'
			// don't accumulate words like 'much', 'all', 'several' etc because these words are not descriptive and 
			//    also will be transferred to matching proper nouns like 'Tommy' 
			if ((cObject::whichOrderWord(m[I].word) != -1) || isMetaGroupWord(I) || isGroupJoiner(m[I].word) || m[I].queryForm(pronounForm) >= 0 || m[I].queryForm(quantifierForm) >= 0 ||
				m[I].word->first == L"dear") // my dear sir / Dear Tommy
				continue;
			//lplog(LOG_RESOLUTION,L"%06d:%s  PERSX    SYN %s",where,objectString(adjectiveObject,tmpstr,true).c_str(),m[I].word->first.c_str());
			map <tIWMM, vector <tIWMM>, tFI::cRMap::wordMapCompare > *wnsMap;
			map <tIWMM, int, tFI::cRMap::wordMapCompare> *wnsGenderMap;
			if (isAdjective &= (I != where))
			{
				wnsMap = &wnSynonymsAdjectiveMap;
				wnsGenderMap = &wnGenderAdjectiveMap;
			}
			else
			{
				wnsMap = &wnSynonymsNounMap;
				wnsGenderMap = &wnGenderNounMap;
			}
			tIWMM mainEntry = m[I].word->second.mainEntry;
			if (mainEntry == wNULL) mainEntry = m[I].word;
			if (!(mainEntry->second.flags&tFI::genericGenderIgnoreMatch) && wnsMap->find(mainEntry) == wnsMap->end())
				fillWNMaps(I, mainEntry, isAdjective);
			if (isAdjective)
			{
				if (find(objects[adjectiveObject].associatedAdjectives.begin(), objects[adjectiveObject].associatedAdjectives.end(), mainEntry) == objects[adjectiveObject].associatedAdjectives.end())
					objects[adjectiveObject].associatedAdjectives.push_back(mainEntry);
			}
			else
			{
				if (find(objects[adjectiveObject].associatedNouns.begin(), objects[adjectiveObject].associatedNouns.end(), mainEntry) == objects[adjectiveObject].associatedNouns.end() &&
					m[I].queryWinnerForm(honorificForm) < 0)
					objects[adjectiveObject].associatedNouns.push_back(mainEntry);
			}
			int gender = (*wnsGenderMap)[mainEntry];
			if (gender && objects[adjectiveObject].objectClass != NON_GENDERED_NAME_OBJECT_CLASS)
			{
				genderSet = (gender&(MALE_GENDER | FEMALE_GENDER)) != 0;
				objects[adjectiveObject].male = (gender&MALE_GENDER) != 0;
				objects[adjectiveObject].female = (gender&FEMALE_GENDER) != 0;
			}
		}
		if (objects[adjectiveObject].objectClass == NAME_OBJECT_CLASS && debugTrace.traceSpeakerResolution)
		{
			wstring nouns, adjectives;
			for (vector <tIWMM>::iterator si = objects[adjectiveObject].associatedNouns.begin(), siEnd = objects[adjectiveObject].associatedNouns.end(); si != siEnd; si++)
				nouns += (*si)->first + L" ";
			for (vector <tIWMM>::iterator si = objects[adjectiveObject].associatedAdjectives.begin(), siEnd = objects[adjectiveObject].associatedAdjectives.end(); si != siEnd; si++)
				adjectives += (*si)->first + L" ";
			if (nouns.size() || adjectives.size())
				lplog(LOG_RESOLUTION, L"%06d:object %s has original nouns (%s) original adjectives (%s)!", where, objectString(adjectiveObject, tmpstr, false).c_str(), nouns.c_str(), adjectives.c_str());
		}
		// if set through a first name that is male, or an honorific, make sure that head noun is set to a man or woman
		// to make it equal with other objects that have matched to generic gendered objects
		// "Mr. Whittington" should match to "man" the same as "Tommy", even though Tommy may have acquired "man"
		// in a previous match.  Other gendered objects like "A young American" should not have "man" associated with them
		// even though it acquires male characteristics because "Tommy" will match against "young" and "man" and 
		// Julius against "man" and "American" and so both will be equal, even though "Julius" should be preferred.
		if (originallyGendered && !genderSet)
			addDefaultGenderedAssociatedNouns(adjectiveObject);
		// is there an intersection between the adjectives in nouns & antonyms for adjectives
		// OR adjectives in adjectives and antonyms in adjectives?
		wstring logMatch;
		tIWMM fromMatch, toMatch, toMapMatch;
		vector <cObject>::iterator o = objects.begin() + adjectiveObject;
		while (nymNoMatch(where, o, o, true, false, logMatch, fromMatch, toMatch, toMapMatch, L"NoMatchSelf"))
		{
			// get order of adjectives or nouns
			int fromPosition = -1, toPosition = -1;
			for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition; I++)
			{
				if (m[I].word == fromMatch || m[I].word->second.mainEntry == fromMatch) fromPosition = I;
				if (m[I].word == toMatch || m[I].word->second.mainEntry == toMatch) toPosition = I;
			}
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Contradictory object %s (on %s)!", where, objectString(o, tmpstr, true).c_str(), logMatch.c_str());
			bool removeTo = false, removeFrom = false;
			if (fromPosition == -1 || toPosition == -1)
				removeFrom = removeTo = true;
			else if (fromPosition < toPosition)
				removeFrom = true;
			else
				removeTo = true;
			// a great deal younger - assume that the earlier adjective is less important
			// old girl
			vector <tIWMM>::iterator n, a;
			if (removeFrom)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:    Removing %s [FROM]", fromPosition, fromMatch->first.c_str());
				n = find(o->associatedNouns.begin(), o->associatedNouns.end(), fromMatch);
				if (n != o->associatedNouns.end()) o->associatedNouns.erase(n);
				a = find(o->associatedAdjectives.begin(), o->associatedAdjectives.end(), fromMatch);
				if (a != o->associatedAdjectives.end()) o->associatedAdjectives.erase(a);
			}
			if (removeTo)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:    Removing %s [TO]", toPosition, toMatch->first.c_str());
				n = find(o->associatedNouns.begin(), o->associatedNouns.end(), toMatch);
				if (n != o->associatedNouns.end()) o->associatedNouns.erase(n);
				a = find(o->associatedAdjectives.begin(), o->associatedAdjectives.end(), toMatch);
				if (a != o->associatedAdjectives.end()) o->associatedAdjectives.erase(a);
			}
		}
	}
	if (adjectiveObject >= 0)
		addDefaultGenderedAssociatedNouns(adjectiveObject);
}

bool Source::objectClassComparable(vector <cObject>::iterator o, vector <cObject>::iterator lso)
{
	LFS
		int primaryObjectClass = o->objectClass, secondaryObjectClass = lso->objectClass;
	if (primaryObjectClass == PRONOUN_OBJECT_CLASS || primaryObjectClass == REFLEXIVE_PRONOUN_OBJECT_CLASS || primaryObjectClass == RECIPROCAL_PRONOUN_OBJECT_CLASS ||
		primaryObjectClass == VERB_OBJECT_CLASS)
		return false;
	if (secondaryObjectClass == PRONOUN_OBJECT_CLASS || secondaryObjectClass == REFLEXIVE_PRONOUN_OBJECT_CLASS || secondaryObjectClass == RECIPROCAL_PRONOUN_OBJECT_CLASS ||
		secondaryObjectClass == VERB_OBJECT_CLASS)
		return false;
	if (o->associatedAdjectives.empty() && o->associatedNouns.empty())
		return false;
	if (lso->associatedAdjectives.empty() && lso->associatedNouns.empty())
		return false;
	bool isPrimaryGendered = (primaryObjectClass == NAME_OBJECT_CLASS || primaryObjectClass == GENDERED_GENERAL_OBJECT_CLASS ||
		primaryObjectClass == BODY_OBJECT_CLASS || primaryObjectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
		primaryObjectClass == GENDERED_DEMONYM_OBJECT_CLASS || primaryObjectClass == META_GROUP_OBJECT_CLASS ||
		primaryObjectClass == GENDERED_RELATIVE_OBJECT_CLASS);
	bool isSecondaryGendered = (secondaryObjectClass == NAME_OBJECT_CLASS || secondaryObjectClass == GENDERED_GENERAL_OBJECT_CLASS ||
		secondaryObjectClass == BODY_OBJECT_CLASS || secondaryObjectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
		secondaryObjectClass == GENDERED_DEMONYM_OBJECT_CLASS || secondaryObjectClass == META_GROUP_OBJECT_CLASS ||
		secondaryObjectClass == GENDERED_RELATIVE_OBJECT_CLASS);
	return !(isPrimaryGendered ^ isSecondaryGendered);
}

bool Source::hasDemonyms(vector <cObject>::iterator o)
{
	LFS
		if (o->objectClass == GENDERED_DEMONYM_OBJECT_CLASS) return true;
	for (vector <tIWMM>::iterator ani = o->associatedNouns.begin(), aniEnd = o->associatedNouns.end(); ani != aniEnd; ani++)
		if ((*ani)->second.query(demonymForm) >= 0)
			return true;
	return false;
}

bool Source::sharedDemonyms(int where, bool traceNymMatch, vector <cObject>::iterator o, vector <cObject>::iterator lso, tIWMM &fromMatch, tIWMM &toMatch, tIWMM &toMapMatch)
{
	LFS
		wstring tmpstr, tmpstr2;
	if (o->objectClass == GENDERED_DEMONYM_OBJECT_CLASS && lso->objectClass == GENDERED_DEMONYM_OBJECT_CLASS)
	{
		if (traceNymMatch)
			lplog(LOG_RESOLUTION, L"%06d:SD 1) o=%s lso=%s %s=%s", where, objectString(o, tmpstr, true).c_str(), objectString(lso, tmpstr2, true).c_str(),
				m[m[o->originalLocation].endObjectPosition - 1].word->first.c_str(), m[m[lso->originalLocation].endObjectPosition - 1].word->first.c_str());
		fromMatch = toMatch = toMapMatch = m[o->originalLocation].word;
		return m[m[o->originalLocation].endObjectPosition - 1].word == m[m[lso->originalLocation].endObjectPosition - 1].word;
	}
	if (o->objectClass == GENDERED_DEMONYM_OBJECT_CLASS)
	{
		if (traceNymMatch)
			lplog(LOG_RESOLUTION, L"%06d:SD 2) o=%s lso=%s %s=%s", where, objectString(o, tmpstr, true).c_str(), objectString(lso, tmpstr2, true).c_str(),
				m[m[o->originalLocation].endObjectPosition - 1].word->first.c_str(), m[m[lso->originalLocation].endObjectPosition - 1].word->first.c_str());
		fromMatch = toMatch = toMapMatch = m[m[o->originalLocation].endObjectPosition - 1].word;
		return find(lso->associatedNouns.begin(), lso->associatedNouns.end(), m[m[o->originalLocation].endObjectPosition - 1].word) != lso->associatedNouns.end();
	}
	if (lso->objectClass == GENDERED_DEMONYM_OBJECT_CLASS)
	{
		if (traceNymMatch)
			lplog(LOG_RESOLUTION, L"%06d:SD 3) o=%s lso=%s %s=%s", where, objectString(o, tmpstr, true).c_str(), objectString(lso, tmpstr2, true).c_str(),
				m[m[o->originalLocation].endObjectPosition - 1].word->first.c_str(), m[m[lso->originalLocation].endObjectPosition - 1].word->first.c_str());
		fromMatch = toMatch = toMapMatch = m[m[lso->originalLocation].endObjectPosition - 1].word;
		if (find(o->associatedNouns.begin(), o->associatedNouns.end(), m[m[lso->originalLocation].endObjectPosition - 1].word) != o->associatedNouns.end())
			return true;
		for (unsigned int I = 0; I < lso->associatedNouns.size(); I++)
			if (lso->associatedNouns[I]->second.query(demonymForm) >= 0 &&
				find(o->associatedNouns.begin(), o->associatedNouns.end(), lso->associatedNouns[I]) != o->associatedNouns.end())
				return true;
		return false;
	}
	for (vector <tIWMM>::iterator ani = o->associatedNouns.begin(), aniEnd = o->associatedNouns.end(); ani != aniEnd; ani++)
		if ((*ani)->second.query(demonymForm) >= 0 &&
			find(lso->associatedNouns.begin(), lso->associatedNouns.end(), *ani) != lso->associatedNouns.end())
		{
			fromMatch = toMatch = toMapMatch = *ani;
			if (traceNymMatch)
				lplog(LOG_RESOLUTION, L"%06d:SD 4) o=%s lso=%s MATCH %s", where, objectString(o, tmpstr, true).c_str(), objectString(lso, tmpstr2, true).c_str(),
				(*ani)->first.c_str());
			return true;
		}
	for (vector <tIWMM>::iterator ani = o->associatedNouns.begin(), aniEnd = o->associatedNouns.end(); ani != aniEnd; ani++)
		if ((*ani)->second.query(demonymForm) >= 0)
			fromMatch = *ani;
	for (vector <tIWMM>::iterator ani = lso->associatedNouns.begin(), aniEnd = lso->associatedNouns.end(); ani != aniEnd; ani++)
		if ((*ani)->second.query(demonymForm) >= 0)
			toMatch = toMapMatch = *ani;
	if (traceNymMatch)
		lplog(LOG_RESOLUTION, L"%06d:SD 4) o=%s lso=%s NO MATCH %s!=%s", where, objectString(o, tmpstr, true).c_str(), objectString(lso, tmpstr2, true).c_str(),
		(fromMatch != wNULL) ? fromMatch->first.c_str() : L"(None)", (toMatch != wNULL) ? toMatch->first.c_str() : L"(None)");
	return false;
}

// A. one object is 'young' having synonyms 'little' 'young' and antonyms 'old'
// B. one object is 'old' having synonyms 'aged' 'old' and antonyms 'young'
// C. one other object is 'big' having synonyms 'astronomic' 'big' and antonyms 'little'
// so A&B are truly opposites: A's synonyms and B's antonyms AND A's antonyms and B's synonyms have common members
//    A&C should be ignored: A's synonyms and B's antonyms are common but NOT A's antonyms and B's synonyms
bool Source::nymNoMatch(vector <cObject>::iterator o, tIWMM adj)
{
	LFS
		vector <tIWMM> associatedAdjectives;
	bool getFromMatch = false, traceThisMatch = false;
	wstring logMatch;
	tIWMM fromMatch, toMatch, toMapMatch;
	associatedAdjectives.push_back(adj);
	wchar_t *type = L"single";
	return  (nymMapMatch(o->associatedAdjectives, wnSynonymsAdjectiveMap, associatedAdjectives, wnAntonymsAdjectiveMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o adj syn <-> lso adj ant") ||
		nymMapMatch(o->associatedNouns, wnSynonymsNounMap, associatedAdjectives, wnAntonymsAdjectiveMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o noun syn <-> lso adj ant")) &&

		(nymMapMatch(associatedAdjectives, wnSynonymsAdjectiveMap, o->associatedAdjectives, wnAntonymsAdjectiveMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"lso adj syn <-> o adj ant") ||
			nymMapMatch(associatedAdjectives, wnSynonymsAdjectiveMap, o->associatedNouns, wnAntonymsNounMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"lso adj syn <-> o noun ant"));
}

// A. one object is 'young' having synonyms 'little' 'young' and antonyms 'old'
// B. one object is 'old' having synonyms 'aged' 'old' and antonyms 'young'
// C. one other object is 'big' having synonyms 'astronomic' 'big' and antonyms 'little'
// so A&B are truly opposites: A's synonyms and B's antonyms AND A's antonyms and B's synonyms have common members
//    A&C should be ignored: A's synonyms and B's antonyms are common but NOT A's antonyms and B's synonyms
bool Source::nymNoMatch(int where, vector <cObject>::iterator o, vector <cObject>::iterator lso, bool getFromMatch, bool traceThisMatch, wstring &logMatch, tIWMM &fromMatch, tIWMM &toMatch, tIWMM &toMapMatch, wchar_t *type)
{
	LFS
		//if (!objectClassComparable(o,lso)) return false;
		// if both have associated Nouns that are demonyms (or are of demonym type), 
		// if there are no shared demonyms, reject.
		if (hasDemonyms(o) && hasDemonyms(lso) && !sharedDemonyms(where, traceThisMatch, o, lso, fromMatch, toMatch, toMapMatch)) return true;
	// don't compare adjectives from body parts like 'low' to GENDERED_OBJECTS like 'young'.  Too many false matches result.  Demonyms (above) are acceptable.
	if (o->objectClass == BODY_OBJECT_CLASS && (lso->objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
		lso->objectClass == NAME_OBJECT_CLASS || lso->objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
		lso->objectClass == GENDERED_DEMONYM_OBJECT_CLASS || lso->objectClass == GENDERED_RELATIVE_OBJECT_CLASS))
		return false;
	if (lso->objectClass == BODY_OBJECT_CLASS && (o->objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
		o->objectClass == NAME_OBJECT_CLASS || o->objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
		o->objectClass == GENDERED_DEMONYM_OBJECT_CLASS || o->objectClass == GENDERED_RELATIVE_OBJECT_CLASS))
		return false;
	return  (nymMapMatch(o->associatedAdjectives, wnSynonymsAdjectiveMap, lso->associatedAdjectives, wnAntonymsAdjectiveMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o adj syn <-> lso adj ant") ||
		nymMapMatch(o->associatedNouns, wnSynonymsNounMap, lso->associatedAdjectives, wnAntonymsAdjectiveMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o noun syn <-> lso adj ant") ||
		nymMapMatch(o->associatedAdjectives, wnSynonymsAdjectiveMap, lso->associatedNouns, wnAntonymsNounMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o adj syn <-> lso noun ant") ||
		nymMapMatch(o->associatedNouns, wnSynonymsNounMap, lso->associatedNouns, wnAntonymsNounMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o noun syn <-> lso noun ant")) &&

		(nymMapMatch(lso->associatedAdjectives, wnSynonymsAdjectiveMap, o->associatedAdjectives, wnAntonymsAdjectiveMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"lso adj syn <-> o adj ant") ||
			nymMapMatch(lso->associatedNouns, wnSynonymsNounMap, o->associatedAdjectives, wnAntonymsAdjectiveMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"lso noun syn <-> o adj ant") ||
			nymMapMatch(lso->associatedAdjectives, wnSynonymsAdjectiveMap, o->associatedNouns, wnAntonymsNounMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"lso adj syn <-> o noun ant") ||
			nymMapMatch(lso->associatedNouns, wnSynonymsNounMap, o->associatedNouns, wnAntonymsNounMap, true, getFromMatch, traceThisMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"lso noun syn <-> o noun ant"));
}

// heavily limit gendered body object comparisons
int Source::limitedNymMatch(vector <cObject>::iterator o, vector <cObject>::iterator lso, bool traceNymMatch)
{
	LFS
		// count a match of the head to be more than a match of adjectives
		int nounsMatched = 0, adjectivesMatched = 0;
	for (unsigned int I = 0; I < o->associatedNouns.size(); I++)
		if ((o->associatedNouns[I]->second.query(demonymForm) >= 0 || o->associatedNouns[I]->second.query(PROPER_NOUN_FORM_NUM) >= 0) &&
			!(o->associatedNouns[I]->second.flags&tFI::genericGenderIgnoreMatch) &&
			find(lso->associatedNouns.begin(), lso->associatedNouns.end(), o->associatedNouns[I]) != lso->associatedNouns.end())
			nounsMatched++;
	for (unsigned int I = 0; I < o->associatedAdjectives.size(); I++)
		if ((o->associatedAdjectives[I]->second.query(demonymForm) >= 0 || o->associatedAdjectives[I]->second.query(PROPER_NOUN_FORM_NUM) >= 0) &&
			find(lso->associatedAdjectives.begin(), lso->associatedAdjectives.end(), o->associatedAdjectives[I]) != lso->associatedAdjectives.end())
			adjectivesMatched++;
	int total = adjectivesMatched * 3 + nounsMatched * 3;
	if (traceNymMatch)
		lplog(LOG_RESOLUTION, L"nounsMatched[*3]=%d adjectivesMatched[*3]=%d yields a total [%d]", nounsMatched, adjectivesMatched, total);
	return total;
}

int Source::nymMatch(vector <cObject>::iterator o, vector <cObject>::iterator lso, bool getFromMatch, bool traceNymMatch, bool &explicitOccupationMatch, wstring &logMatch, tIWMM &fromMatch, tIWMM &toMatch, tIWMM &toMapMatch, wchar_t *type)
{
	LFS
		// limit flow of adjectives and nouns from body objects
		if ((o->objectClass == BODY_OBJECT_CLASS && (lso->objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
			lso->objectClass == NAME_OBJECT_CLASS || lso->objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			lso->objectClass == GENDERED_DEMONYM_OBJECT_CLASS || lso->objectClass == GENDERED_RELATIVE_OBJECT_CLASS)) ||
			(lso->objectClass == BODY_OBJECT_CLASS && (o->objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
				o->objectClass == NAME_OBJECT_CLASS || o->objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
				o->objectClass == GENDERED_DEMONYM_OBJECT_CLASS || o->objectClass == GENDERED_RELATIVE_OBJECT_CLASS)))
			return limitedNymMatch(o, lso, traceNymMatch);
	// count a match of the head to be more than a match of adjectives
	int nounsMatched = 0, adjectivesMatched = 0;
	vector <tIWMM> marked;
	for (unsigned int I = 0; I < o->associatedNouns.size(); I++)
	{
		if (o->associatedNouns[I]->second.flags&tFI::genericGenderIgnoreMatch) continue;
		if (find(lso->associatedNouns.begin(), lso->associatedNouns.end(), o->associatedNouns[I]) != lso->associatedNouns.end())
		{
			marked.push_back(o->associatedNouns[I]);
			o->associatedNouns[I]->second.flags |= tFI::alreadyTaken;
			explicitOccupationMatch = (o->associatedNouns[I]->second.query(commonProfessionForm) >= 0);
			nounsMatched++;
		}
	}
	for (unsigned int I = 0; I < o->associatedAdjectives.size(); I++)
		if (find(lso->associatedAdjectives.begin(), lso->associatedAdjectives.end(), o->associatedAdjectives[I]) != lso->associatedAdjectives.end())
		{
			marked.push_back(o->associatedAdjectives[I]);
			o->associatedAdjectives[I]->second.flags |= tFI::alreadyTaken;
			adjectivesMatched++;
		}
	// 'dr. hall' will match 'the doctor' but 'mrs. edgar keith' won't head match 'missus'
	// Mrs. edgar keith won't get an edge with mrs. Vandermeyer's cook
	bool headsMatch = abbreviationEquivalent(m[o->originalLocation].word, m[lso->originalLocation].word);
	int total = (nounsMatched * 3) + (adjectivesMatched * 3) +
		((nymMapMatch(o->associatedAdjectives, wnSynonymsAdjectiveMap, lso->associatedAdjectives, wnSynonymsAdjectiveMap, false, getFromMatch, traceNymMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o adj syn <-> lso adj syn")) ? 1 : 0) +
		((nymMapMatch(o->associatedNouns, wnSynonymsNounMap, lso->associatedAdjectives, wnSynonymsAdjectiveMap, false, getFromMatch, traceNymMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o noun syn <-> lso adj syn")) ? 1 : 0) +
		((nymMapMatch(o->associatedAdjectives, wnSynonymsAdjectiveMap, lso->associatedNouns, wnSynonymsNounMap, false, getFromMatch, traceNymMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o adj syn <-> lso noun syn")) ? 1 : 0) +
		((nymMapMatch(o->associatedNouns, wnSynonymsNounMap, lso->associatedNouns, wnSynonymsNounMap, false, getFromMatch, traceNymMatch, logMatch, fromMatch, toMatch, toMapMatch, type, L"o noun syn <-> lso noun syn")) ? 1 : 0) +
		// this should not check for genericGenderIgnoreMatch because we still want 'man' to match a 'man' more than someone named 'Tommy'
		// don't allow 'the voice' to force match (because of matching heads) to 'his voice' 
		((headsMatch && ((o->objectClass != BODY_OBJECT_CLASS || o->ownerWhere < 0))) ? 2 : 0); // && !(m[o->originalLocation].word->second.flags&tFI::genericGenderIgnoreMatch)) ? 1 : 0);
   // matching against original locations will work if they are the same class.  But if they are different classes
	if (traceNymMatch)
		lplog(LOG_RESOLUTION, L"nounsMatched[*3]=%d adjectivesMatched[*3]=%d originals match (%d,%d) %s=%s yields a total [%d] - explicitOccupationMatch=%s", nounsMatched, adjectivesMatched,
			o->originalLocation, lso->originalLocation, m[o->originalLocation].word->first.c_str(), (headsMatch) ? L"true" : L"false", total, (explicitOccupationMatch) ? L"true" : L"false");
	for (unsigned int mi = 0; mi < marked.size(); mi++)
		marked[mi]->second.flags &= ~tFI::alreadyTaken;
	return total;
}

int Source::identifySubType(int principalWhere, bool &partialMatch)
{
	LFS
		unsigned int oc = objects[m[principalWhere].getObject()].objectClass, maxObjectsPerType = 0;
	if (oc != NAME_OBJECT_CLASS && oc != NON_GENDERED_GENERAL_OBJECT_CLASS && oc != NON_GENDERED_NAME_OBJECT_CLASS)
		return -1;
	// already established to be a speaker?
	if (objects[m[principalWhere].getObject()].PISDefinite)
		return -1;
	// has honorific (Mr. Dr. unlikely to be a place)
	if (oc == NAME_OBJECT_CLASS && objects[m[principalWhere].getObject()].name.hon != wNULL)
		return -1;
	// of course?
	if (m[principalWhere].word->first == L"course" && principalWhere > 0 && m[principalWhere - 1].word->first == L"of")
		return -1;
	// state - must have a determiner
	if (m[principalWhere].word->first == L"state" && m[principalWhere].beginObjectPosition == principalWhere)
		return -1;
	// create a maximum index to create a unique object # = subType*maxObjectsPerType+objectOffset
	for (unsigned int st = 0; st < NUM_SUBTYPES; st++)
		maxObjectsPerType = max(maxObjectsPerType, multiWordObjects[st].size());
	unordered_map <int, int> objectMappings;
	int maxSubType = -1;
	unsigned int maxObjectMatchValue = 0, op = m[principalWhere].beginObjectPosition;
	vector < tIWMM > *maxMWO=0;
	vector <WordMatch>::iterator lastPosition = (oc == NAME_OBJECT_CLASS || oc == NON_GENDERED_NAME_OBJECT_CLASS) ? m.begin() + m[principalWhere].endObjectPosition - 1 : m.begin() + principalWhere;
	for (vector <WordMatch>::iterator im = m.begin() + op, imEnd = m.begin() + m[principalWhere].endObjectPosition; im != imEnd; im++, op++)
	{
		if (im->queryWinnerForm(adjectiveForm) >= 0 || im->queryWinnerForm(nounForm) >= 0 || im->forms.isSet(PROPER_NOUN_FORM_NUM))
		{
			bool isProperNoun = im->forms.isSet(PROPER_NOUN_FORM_NUM);
			vector <int>::iterator subType = im->word->second.relatedSubTypes.begin();
			vector < tIWMM > *mwo;
			unordered_map <int, int>::iterator omi;
			for (vector <int>::iterator pii = im->word->second.relatedSubTypeObjects.begin(), piiEnd = im->word->second.relatedSubTypeObjects.end(); pii != piiEnd; pii++, subType++)
			{
				// if this is a city or town name etc., the word must be capitalized
				if (*subType <= WORLD_CITY_TOWN_VILLAGE && !isProperNoun)
					continue;
				mwo = &multiWordObjects[*subType][*pii];
				tIWMM mwoPWPosition = (*mwo)[mwo->size() - 1];  // principalWhere position
				for (vector <tIWMM>::iterator imwo = mwo->begin(), imwoEnd = mwo->end(); imwo != imwoEnd; imwo++)
				{
					if ((*imwo)->first == L"of") // sloop of war - war is a modifier, not the principal object
						break;
					mwoPWPosition = *imwo;
				}
				int uniqueObjectIndex = (*subType)*maxObjectsPerType + *pii, positionValue = (im == lastPosition && im->word == mwoPWPosition) ? 10 : 1;
				unsigned int objectValue = ((omi = objectMappings.find(uniqueObjectIndex)) == objectMappings.end()) ? (objectMappings[uniqueObjectIndex] = positionValue) : omi->second += positionValue;
				if (objectValue > maxObjectMatchValue ||
					((*subType == GEOGRAPHICAL_URBAN_SUBFEATURE || *subType == BY_ACTIVITY || *subType == GEOGRAPHICAL_URBAN_SUBSUBFEATURE) && maxSubType == GEOGRAPHICAL_URBAN_FEATURE && objectValue == maxObjectMatchValue))
				{
					// Darrell Hammond / Ella Fitzgerald
					bool rejected = (m[principalWhere].endObjectPosition - m[principalWhere].beginObjectPosition > 1 && mwo->size() == 1 && (objects[m[principalWhere].getObject()].male ^ objects[m[principalWhere].getObject()].female));
					wstring tmpstr, tmpstr2;
					if (debugTrace.traceObjectResolution)
						lplog(LOG_RESOLUTION, L"%06d:object %s@%d matched multiWordObject %s with subType %s position value %s[%d] (%s).",
							principalWhere, objectString(m[principalWhere].getObject(), tmpstr, false).c_str(), op, wordString(*mwo, tmpstr).c_str(), OCSubTypeStrings[*subType],
							(objectValue < 10) ? L"MODIFIER" : L"PRINCIPAL", objectValue, (rejected) ? L"REJECTED: matching 2 against 1, principal only and uniquely gendered" : L"");
					if (!rejected)
					{
						maxSubType = *subType;
						if (objectValue > maxObjectMatchValue)
							maxObjectMatchValue = objectValue;
						maxMWO = mwo;
					}
				}
			}
			if (im->word->second.mainEntry != wNULL && im->word->second.mainEntry != Words.end() && im->word->second.mainEntry != im->word)
			{
				tIWMM w = im->word->second.mainEntry;
				subType = w->second.relatedSubTypes.begin();
				for (vector <int>::iterator pii = w->second.relatedSubTypeObjects.begin(), piiEnd = w->second.relatedSubTypeObjects.end(); pii != piiEnd; pii++, subType++)
				{
					// if this is a city or town name etc., the word must be capitalized
					if (*subType <= WORLD_CITY_TOWN_VILLAGE && !isProperNoun)
						continue;
					mwo = &multiWordObjects[*subType][*pii];
					int uniqueObjectIndex = (*subType)*maxObjectsPerType + *pii, positionValue = (im == lastPosition && w == (*mwo)[mwo->size() - 1]) ? 10 : 1;
					unsigned int objectValue = ((omi = objectMappings.find(uniqueObjectIndex)) == objectMappings.end()) ? (objectMappings[uniqueObjectIndex] = positionValue) : omi->second += positionValue;
					if (objectValue > maxObjectMatchValue || ((*subType == GEOGRAPHICAL_URBAN_SUBFEATURE || *subType == BY_ACTIVITY || *subType == GEOGRAPHICAL_URBAN_SUBSUBFEATURE) && maxSubType == GEOGRAPHICAL_URBAN_FEATURE))
					{
						if (debugTrace.traceObjectResolution)
						{
							wstring tmpstr, tmpstr2;
							lplog(LOG_RESOLUTION, L"%06d:main entry object %s matched multiWordObject %s with subType %s position value %s[%d].",
								principalWhere, objectString(m[principalWhere].getObject(), tmpstr, true).c_str(), wordString(*mwo, tmpstr).c_str(), OCSubTypeStrings[*subType], (objectValue < 10) ? L"MODIFIER" : L"PRINCIPAL", objectValue);
						}
						maxSubType = *subType;
						maxObjectMatchValue = objectValue;
						maxMWO = mwo;
					}
				}
			}
		}
	}
	// if this object matches the other fully, it will match all matchable positions + 9
	if (maxObjectMatchValue < 10)
		maxSubType = -1;
	if (maxSubType >= 0)
	{
		partialMatch = false;
		for (vector < tIWMM >::iterator mwoi = maxMWO->begin(), mwoiEnd = maxMWO->end(); mwoi != mwoiEnd && !partialMatch; mwoi++)
			if (((*mwoi)->second.query(adjectiveForm) >= 0 || (*mwoi)->second.query(nounForm) >= 0 || (*mwoi)->second.query(PROPER_NOUN_FORM_NUM) >= 0))
			{
				bool matchFound = false;
				for (vector <WordMatch>::iterator im = m.begin() + m[principalWhere].beginObjectPosition, imEnd = m.begin() + m[principalWhere].endObjectPosition; im != imEnd && !matchFound; im++)
					matchFound = im->word == *mwoi || im->word->second.mainEntry == *mwoi;
				partialMatch = (!matchFound);
			}
	}
	return maxSubType;
}

// this is to be used in relative phrases that ARE NOT relative clauses used as objects,
// Subject and object pronouns cannot be distinguished by their forms - who, which, that are used for subject and object pronouns.
// You can, however, distinguish them as follows:
//   If the relative pronoun is followed by a verb, the relative pronoun is a subject pronoun. Subject pronouns must always be used.
//     the apple which is lying on the table
//   If the relative pronoun is not followed by a verb (but by a noun or pronoun), the relative pronoun is an object pronoun.
//   Object pronouns can be dropped in defining relative clauses, which are then called Contact Clauses.
//     the apple (which) George lay on the table
// SO
//   with respect to verb objects,
//   if we count the verb 'gave' in the following sentence:
//       The book Thomas gave was expensive.
//     The verb 'gave' actually has an object 'the book'.  Therefore, we must count that possibility in the verbobject statistics and
//     costs.
bool Source::assignRelativeClause(int where)
{
	LFS
		int checkEnd, o = m[where].getObject();
	bool relAsObject = false;
	if (where >= 0 && // objects[o].objectClass!=NAME_OBJECT_CLASS && 
		(checkEnd = m[where].endObjectPosition) >= 0 && checkEnd < (int)m.size() && m[where].principalWhereAdjectivalPosition < 0)
	{
		if (objects[o].relativeClausePM >= 0) return true;
		int pmWhere = -1, whereRelClause = -1;
		scanForLocation(m[checkEnd].queryForm(relativizerForm) >= 0 || m[checkEnd].queryForm(demonstrativeDeterminerForm) >= 0, relAsObject, whereRelClause, pmWhere, checkEnd);
		scanForLocation(pmWhere < 0 && (m[checkEnd].word->first == L"," || m[checkEnd].queryForm(dashForm) != -1), relAsObject, whereRelClause, pmWhere, checkEnd + 1);
		// scan past asides 
		int C1Len = -1;
		scanForLocation(pmWhere < 0 && m[checkEnd].pma.queryPattern(L"__C1_IP", C1Len) != -1, relAsObject, whereRelClause, pmWhere, checkEnd + C1Len);
		// scan past trailing prepositional phrases
		// a tall man with close-cropped hair and a short, pointed, naval-looking beard, who sat where the head of the table with papers in front of him.
		for (patternMatchArray::tPatternMatch *pm = m[checkEnd].pma.content, *pmend = pm + m[checkEnd].pma.count; pm != pmend && pmWhere < 0; pm++)
			if (patterns[pm->getPattern()]->name == L"_PP" && checkEnd + pm->len < (int)m.size())
			{
				scanForLocation(true, relAsObject, whereRelClause, pmWhere, checkEnd + pm->len);
				scanForLocation(pmWhere < 0 && m[checkEnd + pm->len].word->first == L",", relAsObject, whereRelClause, pmWhere, checkEnd + pm->len + 1);
			}
		// skip relative phrases which have embedded sentences (because they do not necessarily bind to the previous object)
		//   // she went to London, where she entered a children's hospital.
		// there is a certain man[brown] , a man whose real name is unknown to us[tuppence,tommy] , who is working in the dark for his[brown] own ends
		for (patternMatchArray::tPatternMatch *pm = m[checkEnd].pma.content, *pmend = pm + m[checkEnd].pma.count; pm != pmend && pmWhere < 0; pm++)
			if (patterns[pm->getPattern()]->name == L"_REL1")
			{
				if ((m[checkEnd + 1].objectRole&SUBJECT_ROLE) && (m[checkEnd].word->first == L"who" || m[checkEnd].word->first == L"whom") &&
					(objects[o].male || objects[o].female) && !objects[o].neuter)
					objects[o].whereRelSubjectClause = checkEnd + 1; // this points to an object that is not yet identified
				scanForLocation(true, relAsObject, whereRelClause, pmWhere, checkEnd + pm->len);
				scanForLocation(pmWhere < 0 && checkEnd + pm->len < (int)m.size() && m[checkEnd + pm->len].word->first == L",", relAsObject, whereRelClause, pmWhere, checkEnd + pm->len + 1);
			}
		// number check
		if (pmWhere >= 0)
		{
			// scan from relativizer to relVerb, looking for a SINGULAR or PLURAL verb
			// pause at end of relative clause or when OBJECT is hit.
			bool isSingular = false, isPlural = false;
			int relEnd = whereRelClause + m[whereRelClause].pma[pmWhere].len;
			for (int I = whereRelClause + 1; I < relEnd && !(m[I].objectRole&OBJECT_ROLE) && (!isSingular && !isPlural); I++)
			{
				isSingular = (m[I].word->second.inflectionFlags&(VERB_PRESENT_THIRD_SINGULAR | VERB_PRESENT_FIRST_SINGULAR | VERB_PAST_THIRD_SINGULAR | VERB_PRESENT_SECOND_SINGULAR)) != 0;
				isPlural = (m[I].word->second.inflectionFlags&(VERB_PAST_PLURAL | VERB_PRESENT_PLURAL)) != 0;
			}
			if ((isSingular && objects[o].plural) || (isPlural && !objects[o].plural))
			{
				pmWhere = -1;
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr;
					lplog(LOG_RESOLUTION, L"%06d:%s rejected a relative clause at %d (incorrect number).", where, objectString(o, tmpstr, true).c_str(), whereRelClause);
				}
			}
		}
		// gender check
		bool isNeuter = objects[o].neuter || objects[o].objectClass == BODY_OBJECT_CLASS;
		if (pmWhere >= 0 && whereRelClause >= 0 &&
			((isNeuter && m[whereRelClause].word->first != L"which" && m[whereRelClause].word->first != L"that") ||
			(!isNeuter && (m[whereRelClause].word->first == L"which" || m[whereRelClause].word->first == L"that"))))
		{
			pmWhere = -1;
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr;
				lplog(LOG_RESOLUTION, L"%06d:%s rejected a relative clause at %d (incorrect gender).", where, objectString(o, tmpstr, true).c_str(), whereRelClause);
			}
		}
		int oo; // other object - some relativizers may get their own object before this - but 
		// prefer the gender matched noun that is the closest to the relative clause - this may be aided by matching relatives later!
		// the objects will not have relative clauses assigned to them.
		if (pmWhere >= 0 && (oo = m[whereRelClause].getObject()) >= 0 && objects[oo].relativeClausePM >= 0 &&
			(oo != o || objects[oo].whereRelativeClause != whereRelClause ||
				m[whereRelClause].pma[objects[oo].relativeClausePM].len >= m[whereRelClause].pma[pmWhere].len))
		{
			// the elderly woman, looking more like a housekeeper than a servant, who opened the door 
			// if either object is an RE_OBJ, reject that object.  reject the object farthest away from relative phrase.
			int oBefore = locationBefore(oo, whereRelClause);
			if (oBefore >= 0)
			{
				wstring tmpstr;
				if (!(m[oBefore].objectRole&RE_OBJECT_ROLE) && (m[where].objectRole&RE_OBJECT_ROLE))
				{
					pmWhere = -1;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:%s rejected a relative clause at %d (RE_OBJECT).", where, objectString(o, tmpstr, true).c_str(), whereRelClause);
				}
				else if ((m[oBefore].objectRole&RE_OBJECT_ROLE) && !(m[where].objectRole&RE_OBJECT_ROLE))
				{
					objects[oo].relativeClausePM = objects[oo].whereRelativeClause = -1;
					if (oo >= 0 && debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:%s rejected a relative clause at %d (RE_OBJECT).", where, objectString(oo, tmpstr, true).c_str(), whereRelClause);
				}
				else if (oBefore > where)
				{
					pmWhere = -1;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:%s rejected a relative clause at %d (POSITION).", where, objectString(o, tmpstr, true).c_str(), whereRelClause);
				}
				else if (where > oBefore)
				{
					objects[oo].relativeClausePM = objects[oo].whereRelativeClause = -1;
					if (oo >= 0 && debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:%s rejected a relative clause at %d (POSITION).", where, objectString(oo, tmpstr, true).c_str(), whereRelClause);
				}
			}
		}
		// 'what' may be used in some relative phrases used as subjects, but otherwise does not indicate assignment
		if (pmWhere >= 0 && !relAsObject)
		{
			objects[o].relativeClausePM = pmWhere;
			objects[o].whereRelativeClause = whereRelClause;
			//m[whereRelClause].objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
			objects[o].locations.push_back(whereRelClause);
			m[whereRelClause].setObject(o);
			m[whereRelClause].beginObjectPosition = whereRelClause;
			m[whereRelClause].endObjectPosition = whereRelClause + 1;
			m[whereRelClause].principalWherePosition = whereRelClause;
			m[whereRelClause].flags |= WordMatch::flagRelativeHead;
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr;
				lplog(LOG_RESOLUTION, L"%06d:%s was assigned a relative clause at %d.", where, objectString(o, tmpstr, true).c_str(), whereRelClause);
			}
			return true;
		}
		if (pmWhere >= 0 && relAsObject)
		{
			m[whereRelClause].setRelObject(where);
			m[whereRelClause].flags |= WordMatch::flagRelativeObject;
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr;
				lplog(LOG_RESOLUTION, L"%06d:%s was assigned a relative clause as object at %d.", where, objectString(o, tmpstr, true).c_str(), whereRelClause);
			}
			return true;
		}
	}
	return false;
}

// if adjectival is false, where and element point to the full multi-word object
// if adjectival is true, previousOwnerWhere points to the ownerWhere of the previous adjective (if any)
int Source::identifyObject(int tag, int where, int element, bool adjectival, int previousOwnerWhere, int ownerBegin)
{
	LFS
		wstring tagName = (tag < 0) ? L"NAME" : patternTagStrings[tag];
	if (m[where].queryForm(L"--") >= 0) // m[where].queryForm(indefinitePronounForm)>=0 ||
	{
		lplog(LOG_RESOLUTION, L"%06d:word %s is an indefinite pronoun and so not eligible for tracking.", where, m[where].word->first.c_str());
		return -1;
	}
	int principalWhere = where, begin = where;
	unsigned int end;
	enum OC objectClass = NON_GENDERED_GENERAL_OBJECT_CLASS;
	bool comparableName = false, comparableNameAdjective = false, plural = false, embeddedName = false, requestWikiAgreement = false;
	bool isMale = false, isFemale = false, isNeuter = false, isOwnerGendered = false, isOwnerMale = false, isOwnerFemale = false, isOwnerPlural = false, isBusiness = false, hasDeterminer = false;
	cName name;
	if (tagName != L"VNOUN" && tagName != L"GNOUN")
	{
		// "NOUN","PNOUN","NAME","NAMEOWNER"
		if (element != -1)
			findSpecificAnaphor(tagName, where, element, principalWhere, plural, embeddedName);
		// if it is an ordinal having a noun as an adjective, make the noun the primary (the car first)
		// this is to correct parsing errors that inadvertently classify the noun as modifying the ordinal
		if (where < principalWhere && m[principalWhere].queryWinnerForm(numeralOrdinalForm) >= 0 && m[principalWhere - 1].queryWinnerForm(nounForm) >= 0)
			principalWhere--;
		end = (element == -1) ? where + 1 : (element&matchElement::patternFlag) ? m[where].pma[element&~matchElement::patternFlag].len + where : where + 1;
		if ((signed)end > where + 1 && m[end - 1].word->first == L"--")
		{
			end--; // subtract -- from the end of a noun (see NOUN[E])
			embeddedName = false;
		}
		int nameElement = -1;
		if (tagName == L"NAME" || embeddedName)
			nameElement = element;
		else if (tagName == L"NOUN")
		{
			int I = principalWhere, pnwf, nwf;
			// consider being a name IF
			// all words are in uppercase, and (each word doesn't have a proper noun form OR it DOES have a noun form that is less expensive than the proper noun form)
			// prevents all caps words (titles) from being confused with names, if the names would be at least as expensive
			for (; I < (int)end; I++)
			{
				/*
				int tmp1=m[I].flags&WordMatch::flagAllCaps;
				pnwf=m[I].queryWinnerForm(PROPER_NOUN_FORM_NUM);
				nwf=m[I].queryWinnerForm(nounForm);
				int tmp2=(pnwf>=0) ? m[I].word->second.usageCosts[pnwf] : -1;
				int tmp3=(nwf>=0) ? m[I].word->second.usageCosts[nwf] : -1;
				*/
				if (!(m[I].flags&WordMatch::flagAllCaps) ||
					(pnwf = m[I].queryWinnerForm(PROPER_NOUN_FORM_NUM)) >= 0 &&
					(((nwf = m[I].queryWinnerForm(nounForm)) < 0) || m[I].word->second.getUsageCost(pnwf) <= m[I].word->second.getUsageCost(nwf)))
					break;
			}
			if (I == end)
			{
				nameElement = m[principalWhere].pma.queryPatternWithLen(L"_NAME", end - principalWhere);
				if (nameElement == -1 && (nameElement = m[principalWhere].queryWinnerForm(PROPER_NOUN_FORM_NUM)) < 0)
				{
					if ((nameElement = m[principalWhere].queryWinnerForm(honorificForm)) < 0)
						nameElement = m[principalWhere].queryWinnerForm(honorificAbbreviationForm);
				}
			}
		}
		// you are English, aren't you?
		if (m[begin].queryForm(demonymForm) >= 0 && end - begin == 1 && (m[begin].word->second.inflectionFlags&PLURAL) == PLURAL &&
			(m[begin].objectRole&(SUBJECT_ROLE | OBJECT_ROLE)) == OBJECT_ROLE)
			return -1;
		if (!identifyName(begin, principalWhere, end, nameElement, name, isMale, isFemale, isNeuter, plural, isBusiness, comparableName, comparableNameAdjective, requestWikiAgreement, objectClass) &&
			m[principalWhere].isGendered())
		{
			int inflectionFlags = m[principalWhere].word->second.inflectionFlags;
			if (!isBusiness && (m[principalWhere].queryWinnerForm(PROPER_NOUN_FORM_NUM) >= 0 ||
				(m[principalWhere].flags&WordMatch::flagFirstLetterCapitalized) != 0 ||
				(inflectionFlags&(MALE_GENDER_ONLY_CAPITALIZED | FEMALE_GENDER_ONLY_CAPITALIZED)) == 0))
			{
				isMale = (inflectionFlags&(MALE_GENDER | MALE_GENDER_ONLY_CAPITALIZED)) != 0;
				isFemale = (inflectionFlags&(FEMALE_GENDER | FEMALE_GENDER_ONLY_CAPITALIZED)) != 0;
			}
			else
				isMale = isFemale = false;
		}
		if (objectClass != NAME_OBJECT_CLASS && objectClass != NON_GENDERED_NAME_OBJECT_CLASS)
		{
			// an Irish Sinn feiner (a name, so principalWhere is set to Irish, but it should be set to Sinn feiner, because Irish is a plural noun)
			if (objectClass == GENDERED_DEMONYM_OBJECT_CLASS && (m[principalWhere].queryForm(demonymForm) < 0 || m[end - 1].queryForm(demonymForm) >= 0)) principalWhere = end - 1;
			plural = (m[principalWhere].word->second.inflectionFlags&PLURAL) == PLURAL;
			bool singular = (m[principalWhere].word->second.inflectionFlags&SINGULAR) == SINGULAR;
			isNeuter = (m[principalWhere].word->second.inflectionFlags&NEUTER_GENDER) == NEUTER_GENDER;
			// prevent 'that' and 'this' from relative phrases from concatenating onto the beginning of objects
			if ((plural ^ singular) && (principalWhere - where < 2 || !(m[where + 1].flags&WordMatch::flagNounOwner)))
			{
				if (plural && (m[where].word->first == L"that" || m[where].word->first == L"this"))
				where++;
				else if (!plural && (m[where].word->first == L"these" || m[where].word->first == L"those"))
				where++;
			}
		}
		if (tagName == L"NOUN" || tagName == L"PNOUN")
		{
			if (m[principalWhere].queryWinnerForm(reflexivePronounForm) >= 0)
				objectClass = REFLEXIVE_PRONOUN_OBJECT_CLASS;
			else if (m[principalWhere].queryWinnerForm(reciprocalPronounForm) >= 0)
				objectClass = RECIPROCAL_PRONOUN_OBJECT_CLASS;
		}
	}
	else
	{
		isNeuter = true;
		principalWhere = m[where].pma[element&~matchElement::patternFlag].len + where - 1;
		end = principalWhere + 1;
		if (m[principalWhere].queryForm(quoteForm) >= 0) principalWhere--;
		// the(95557) much- heralded “Labour Day,”
		if (m[principalWhere].word->first == L"." || m[principalWhere].word->first == L",") principalWhere--;
	}
	int ownerWhere = previousOwnerWhere;
	bool singularBodyPart=false;
	if (adjectival)
	{
		bool possessivePronoun = false;
		if (m[principalWhere].isPossessivelyGendered(possessivePronoun))
		{
			if (possessivePronoun)
				objectClass = PRONOUN_OBJECT_CLASS;
			int testOwnerWhere = -2 - cObject::whichOrderWord(m[principalWhere].word);
			if (testOwnerWhere != -1)
			{
				objectClass = META_GROUP_OBJECT_CLASS;
				ownerWhere = testOwnerWhere;
			}
			int element2 = m[principalWhere].pma.queryPattern(L"__NAMEOWNER");
			if (element2 != -1) element2 &= ~matchElement::patternFlag;
			plural = plural || (m[principalWhere].word->second.inflectionFlags&PLURAL_OWNER) == PLURAL_OWNER ||
				(element2 >= 0 && patterns[m[principalWhere].pma[element2].getPattern()]->tags.find(PLURAL_TAG) != patterns[m[principalWhere].pma[element2].getPattern()]->tags.end());
		}
		//  The efficient German's voice but NOT 'The efficient German master' - The efficient German in the second example is not necessarily an independent entity
		if (principalWhere && m[principalWhere].queryForm(demonymForm) >= 0 && m[ownerBegin].queryForm(determinerForm) >= 0)
		{
			if ((m[where].word->second.inflectionFlags&(PLURAL_OWNER | SINGULAR_OWNER)) || (m[where].flags&WordMatch::flagNounOwner))
			{
				begin = ownerBegin;
				objectClass = GENDERED_DEMONYM_OBJECT_CLASS;
				if (!isMale && !isFemale) isMale = isFemale = true;
			}
			else
				return -1;
		}
		if (principalWhere && m[principalWhere].queryForm(relativeForm) >= 0 && !(m[principalWhere].flags&WordMatch::flagFirstLetterCapitalized))
			objectClass = GENDERED_RELATIVE_OBJECT_CLASS;
	}
	else
	{
		unsigned int nameTag = findTag(L"NAME"), nounTag = findTag(L"NOUN");
		for (unsigned int I = where; I < (unsigned)principalWhere; I++)
		{
			hasDeterminer |= (m[I].queryWinnerForm(determinerForm) >= 0);
			patternMatchArray::tPatternMatch *pma = m[I].pma.content;
			int gElement = -1, maxLen = -1;
			for (unsigned int PMAElement = 0; PMAElement < m[I].pma.count; PMAElement++, pma++)
				if (patterns[pma->getPattern()]->hasTag(GNOUN_TAG) && pma->len > maxLen)
				{
					gElement = PMAElement;
					maxLen = pma->len;
					break;
				}
			if (I == where && where + maxLen == end) break; // don't analyze part of name
	  // if _NAME (skip rest of _NAME)
			int nameLen, nelement, ow = cObject::whichOrderWord(m[I].word);
			if ((nelement = m[I].pma.queryPattern(L"_NAME", nameLen)) != -1 && nameLen >= maxLen)
			{
				identifyObject(nameTag, I, nelement | matchElement::patternFlag, true, -1, where);
				I += nameLen - 1;
			}
			else if ((nelement = m[I].pma.queryPattern(L"_NAMEOWNER", nameLen)) != -1)
			{
				if (identifyObject(nameTag, I, nelement | matchElement::patternFlag, true, ownerWhere, where) >= 0 && m[I].getObject() >= 0 && (objects[m[I].getObject()].male || objects[m[I].getObject()].female))
					ownerWhere = I;
				I += nameLen - 1;
				hasDeterminer = true;
			}
			// if single proper noun or gendered noun
			else if ((nelement = m[I].queryWinnerForm(PROPER_NOUN_FORM_NUM)) >= 0 || ((nelement = m[I].queryWinnerForm(L"noun")) >= 0))
			{
				// Number One
				if (gElement == -1 || maxLen == 1)
				{
					// his banker's check - banker is not male or female.  ownerWhere must be reset here because otherwise it will be set
					// to 'his' which is not correct.  It is not 'his' check, it is 'his' banker's check.
					// so this has to be either a pronoun to own or it has an OWNER flag on it.
					if (identifyObject(nounTag, I, nelement, true, ownerWhere, where) >= 0 && m[I].getObject() >= 0 &&
						(m[I].word->second.inflectionFlags&(PLURAL_OWNER | SINGULAR_OWNER)) || (m[I].flags&WordMatch::flagNounOwner))
						ownerWhere = I;
				}
				else if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:word %s rejected as an individual object (in gnoun).", I, m[I].word->first.c_str());
			}
			// if possessive_determiner
			else if (m[I].queryWinnerForm(possessiveDeterminerForm) >= 0)
			{
				hasDeterminer = true;
				int inflectionFlags = m[I].word->second.inflectionFlags;
				if ((inflectionFlags&(MALE_GENDER | FEMALE_GENDER)) == MALE_GENDER)
					m[I].setObject(OBJECT_UNKNOWN_MALE);
				else if ((inflectionFlags&(MALE_GENDER | FEMALE_GENDER)) == FEMALE_GENDER)
					m[I].setObject(OBJECT_UNKNOWN_FEMALE);
				else if ((inflectionFlags&(MALE_GENDER | FEMALE_GENDER)) == (MALE_GENDER | FEMALE_GENDER))
					m[I].setObject(OBJECT_UNKNOWN_MALE_OR_FEMALE);
				else if ((inflectionFlags&NEUTER_GENDER) == NEUTER_GENDER)
					m[I].setObject(OBJECT_UNKNOWN_NEUTER);
				else if ((inflectionFlags&(PLURAL | PLURAL_OWNER)) != 0)
					m[I].setObject(OBJECT_UNKNOWN_PLURAL);
				else
					m[I].setObject(OBJECT_UNKNOWN_MALE_OR_FEMALE);
				if ((inflectionFlags&(MALE_GENDER | FEMALE_GENDER)) != 0 || (inflectionFlags&(FIRST_PERSON | SECOND_PERSON)) != 0)
					ownerWhere = I;
				m[I].beginObjectPosition = I;
				m[I].endObjectPosition = I + 1;
				m[I].flags |= WordMatch::flagAdjectivalObject;
				m[I].principalWhereAdjectivalPosition = principalWhere;
			}
			else if (ow >= 0 && ((m[I].word->second.inflectionFlags&(PLURAL_OWNER | SINGULAR_OWNER)) || (m[I].flags&WordMatch::flagNounOwner)))
			{
				hasDeterminer = true;
				if (identifyObject(nounTag, I, nelement, true, ownerWhere, where) >= 0 && m[I].getObject() >= 0)
					ownerWhere = I;
			}
			isOwnerGendered = ownerWhere != -1;
			if (ownerWhere >= 0 && tagName != L"VNOUN" && tagName != L"GNOUN" && isOwnerGendered && isExternalBodyPart(principalWhere, singularBodyPart, (m[ownerWhere].word->second.inflectionFlags&(PLURAL_OWNER | PLURAL)) != 0))
			{
				if (m[ownerWhere].getObject() <= UNKNOWN_OBJECT)
				{
					isOwnerMale = m[ownerWhere].getObject() == OBJECT_UNKNOWN_MALE;
					isOwnerFemale = m[ownerWhere].getObject() == OBJECT_UNKNOWN_FEMALE;
					if (m[ownerWhere].getObject() == OBJECT_UNKNOWN_MALE_OR_FEMALE)
						isOwnerMale = isOwnerFemale = true;
				}
				else
				{
					isOwnerMale = objects[m[ownerWhere].getObject()].male;
					isOwnerFemale = objects[m[ownerWhere].getObject()].female;
				}
				isOwnerPlural = (m[ownerWhere].word->second.inflectionFlags&PLURAL) == PLURAL;
			}
			// her former manner - the owner should be 'her' not 'former', since 'her' allows more information for resolution
			if (ow >= 0 && ownerWhere < 0) ownerWhere = -2 - ow;
		}
	}
	if (tagName == L"VNOUN")
	{
		objectClass = VERB_OBJECT_CLASS;
		// but not tagName=="GNOUN" "the other" should be set to "other" not "the"!
		principalWhere = where;
	}
	else if (tagName != L"GNOUN")
	{
		if ((objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS || objectClass == NAME_OBJECT_CLASS) &&
			(m[principalWhere].queryWinnerForm(nomForm) >= 0 || m[principalWhere].queryWinnerForm(accForm) >= 0 ||
				m[principalWhere].queryWinnerForm(personalPronounForm) >= 0 || m[principalWhere].queryWinnerForm(quantifierForm) >= 0 ||
				m[principalWhere].queryWinnerForm(possessivePronounForm) >= 0 ||
				m[principalWhere].queryWinnerForm(indefinitePronounForm) >= 0 || m[principalWhere].queryWinnerForm(pronounForm) >= 0 ||
				m[principalWhere].queryWinnerForm(demonstrativeDeterminerForm) >= 0) &&
			// "another"
				(cObject::whichOrderWord(m[principalWhere].word) == -1))
			objectClass = (isPleonastic(principalWhere)) ? PLEONASTIC_OBJECT_CLASS : PRONOUN_OBJECT_CLASS;
		if (end - begin == 2 && m[begin].word->first == L"some" && m[begin + 1].word->first == L"one")
			objectClass = PRONOUN_OBJECT_CLASS;
		// don't recognize 'that' in: He had never recognized that he was a good baseball player.
		if (objectClass != NAME_OBJECT_CLASS && scanForPatternTag(where, SENTENCE_IN_REL_TAG) != -1 && end - where == 1)
			return -1;
		if (objectClass == NAME_OBJECT_CLASS && !(m[principalWhere].flags&WordMatch::flagFirstLetterCapitalized) &&
			(m[principalWhere].queryForm(relativeForm) >= 0 ||
			(m[principalWhere].word->second.mainEntry != wNULL && m[principalWhere].word->second.mainEntry->second.query(relativeForm) >= 0)))
		{
			objectClass = GENDERED_RELATIVE_OBJECT_CLASS;
			if (!isMale && !isFemale) isMale = isFemale = true;
		}
		if (objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS)
		{
			if (!adjectival)
			{
				// check for cardinal numbers
				if (m[principalWhere].queryWinnerForm(numeralCardinalForm) >= 0 && m[principalWhere].word->first != L"one")
					plural = true;
			}
			if (isMale || isFemale)
				objectClass = GENDERED_GENERAL_OBJECT_CLASS;
			// a lot - a singular noun that is actually a plural and if a subject, then it should have gender 
			if ((m[principalWhere].word->first == L"lot" /* || m[principalWhere].word->first==L"little"*/) && m[principalWhere - 1].word->first == L"a")
			{
				objectClass = GENDERED_GENERAL_OBJECT_CLASS;
				isMale = isFemale = isNeuter = plural = true;
			}
			// common professions are only recorded as singular
			if (m[principalWhere].queryForm(commonProfessionForm) >= 0 ||
				(m[principalWhere].word->second.mainEntry != wNULL && m[principalWhere].word->second.mainEntry->second.query(commonProfessionForm) >= 0))
			{
				// common professions always use a determiner after a preposition  / on guard / on a guard
				// by hook or by crook / for hire
				if (adjectival || !principalWhere || m[principalWhere - 1].queryWinnerForm(prepositionForm) < 0)
				{
					objectClass = GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS;
					if (!isMale && !isFemale) isMale = isFemale = true;
				}
				else
				{
					objectClass = VERB_OBJECT_CLASS;
					isMale = isFemale = false;
					isNeuter = true;
				}
			}
			// demonyms are only recorded as singular
			if ((m[principalWhere].queryForm(demonymForm) >= 0 ||
				(m[principalWhere].word->second.mainEntry != wNULL && m[principalWhere].word->second.mainEntry->second.query(demonymForm) >= 0)) &&
				m[where].queryForm(determinerForm) >= 0)
			{
				objectClass = GENDERED_DEMONYM_OBJECT_CLASS;
				if (!isMale && !isFemale) isMale = isFemale = true;
			}
			if (!(m[principalWhere].flags&WordMatch::flagFirstLetterCapitalized) && (m[principalWhere].queryForm(relativeForm) >= 0 ||
				(m[principalWhere].word->second.mainEntry != wNULL && m[principalWhere].word->second.mainEntry->second.query(relativeForm) >= 0)))
			{
				objectClass = GENDERED_RELATIVE_OBJECT_CLASS;
				if (!isMale && !isFemale) isMale = isFemale = true;
			}
			// the accents of Number One / but not 'at hand' or 'on hand' 
			if ((end - begin) == 1 && m[where].word->first == L"hand")
				return -1;
			if ((isOwnerGendered || (m[principalWhere].objectRole&(SUBJECT_ROLE | OBJECT_ROLE)) ||
				((end - begin) > 1 && (m[principalWhere].objectRole&PREP_OBJECT_ROLE) && begin && m[begin].word->first != L"the" && m[begin - 1].word->first == L"with") ||
				(principalWhere + 1 < (signed)m.size() && m[principalWhere + 1].word->first == L"of")) &&
				isExternalBodyPart(principalWhere, singularBodyPart, ownerWhere < 0 || (m[ownerWhere].word->second.inflectionFlags&(PLURAL_OWNER | PLURAL)) != 0))
			{
				objectClass = BODY_OBJECT_CLASS;
				if (isOwnerMale || isOwnerFemale)
				{
					isMale = isOwnerMale;
					isFemale = isOwnerFemale;
				}
				// that glance
				if (!isNeuter && !isMale && !isFemale)
					isNeuter = isMale = isFemale = true;
			}
			// first           -1                 wordOrder word at prinpalWhere, no determiner - neuter
			// the other       -1                 wordOrder word is in the principalWhere position, no resolvable owner
			// his last        positive           wordOrder word at principalWhere, resolvable owner
			// the companion   -1                 metagroup word at principalWhere, no resolvable owner
			// his companion   positive           metagroup word at principalWhere, with a resolvable owner
			// another ally    -3                 metagroup word at principalWhere, with a wordOrder word (another) as modifier
			bool isWordOrder;
			// first-comer, last-comer, new-comer
			if ((isWordOrder = cObject::whichOrderWord(m[principalWhere].word) != -1) ||
				// my fellow, but not "the fellow" (fellow is also a generic gender word)
				(isMetaGroupWord(principalWhere) && (!(m[principalWhere].word->second.flags&tFI::genericGenderIgnoreMatch) || ownerWhere != -1)) ||
				isGroupJoiner(m[principalWhere].word))
			{
				// if the object is only numbers, and more than one word, skip this.
				bool allNumber=false;
				for (int I = where; I < (signed)end && (allNumber = m[I].queryWinnerForm(numeralCardinalForm) >= 0 || m[I].queryWinnerForm(dashForm) >= 0); I++);
				// either not entirely a number, or only of length one, and not primarily a number, or not an object of a preposition
				if ((!allNumber || end - where == 1) && (m[where].queryWinnerForm(numeralCardinalForm) < 0 || !where ||
					(m[where - 1].word->first != L"till" && m[where - 1].word->first != L"until" && m[where - 1].word->first != L"at") || m[where + 1].word->first == L"of"))
				{
					objectClass = META_GROUP_OBJECT_CLASS;
					if (!isMale && !isFemale) isMale = isFemale = true;
					if (isWordOrder && where == principalWhere)
					{
						isMale = (m[principalWhere].word->second.inflectionFlags&MALE_GENDER) == MALE_GENDER;
						isFemale = (m[principalWhere].word->second.inflectionFlags&FEMALE_GENDER) == FEMALE_GENDER;
						isNeuter = (m[principalWhere].word->second.inflectionFlags&NEUTER_GENDER) == NEUTER_GENDER;
						isNeuter |= (!isMale && !isFemale);
					}
				}
			}
			// the other man   -2                 gendered word at principalWhere, wordOrder owner as modifier (another man)
			if ((objectClass == GENDERED_GENERAL_OBJECT_CLASS || (objectClass == BODY_OBJECT_CLASS && singularBodyPart) ||
				objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || objectClass == GENDERED_DEMONYM_OBJECT_CLASS) && ownerWhere < -1)
				objectClass = META_GROUP_OBJECT_CLASS;
			if (objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS)
			{
				// SNL / IBM
				if (m[where].word->first.length() > 1 && (end - begin) == 1 && (m[where].flags&WordMatch::flagAllCaps))
				{
					objectClass = NON_GENDERED_NAME_OBJECT_CLASS;
					name.any = m[where].word;
				}
				isNeuter = true;
			}
		}
	}
	// the last
	if (objectClass == PRONOUN_OBJECT_CLASS && !isOwnerGendered &&
		m[where].queryWinnerForm(determinerForm) >= 0 &&
		((ownerWhere = -2 - cObject::whichOrderWord(m[principalWhere].word)) != -1))
		objectClass = META_GROUP_OBJECT_CLASS;
	if (isBusiness || (objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS && m[principalWhere].queryWinnerForm(businessForm) >= 0))
		objectClass = NON_GENDERED_BUSINESS_OBJECT_CLASS;
	cObject thisObject(objectClass, name, where, end, principalWhere, ((element&matchElement::patternFlag) && element != -1) ? element & ~matchElement::patternFlag : -1, ownerWhere,
		isMale, isFemale, isNeuter, plural, false);
	if (ownerWhere >= 0)
	{
		if (m[ownerWhere].getObject() <= UNKNOWN_OBJECT)
		{
			thisObject.ownerMale = m[ownerWhere].getObject() == OBJECT_UNKNOWN_MALE;
			thisObject.ownerFemale = m[ownerWhere].getObject() == OBJECT_UNKNOWN_FEMALE;
			if (m[ownerWhere].getObject() == OBJECT_UNKNOWN_MALE_OR_FEMALE)
				thisObject.ownerMale = thisObject.ownerFemale = true;
		}
		else
		{
			thisObject.ownerMale = objects[m[ownerWhere].getObject()].male;
			thisObject.ownerFemale = objects[m[ownerWhere].getObject()].female;
		}
		thisObject.ownerPlural = (m[ownerWhere].word->second.inflectionFlags&PLURAL) == PLURAL;
	}
	if (!searchExactMatch(thisObject, principalWhere))
	{
		/*
		NAME_OBJECT_CLASS:associated with the first, middle or last
		NON_GENDERED_GENERAL_OBJECT_CLASS:linked to its main noun N_AGREE
		GENDERED_GENERAL_OBJECT_CLASS:linked to its main noun and its Proper Noun
		*/
		for (int I = thisObject.begin; I < thisObject.end; I++)
		{
			bitObject<> *forms = &m[I].forms;
			if ((forms->isSet(nounForm) || forms->isSet(adjectiveForm) || forms->isSet(adverbForm) ||
				forms->isSet(verbForm) || forms->isSet(PROPER_NOUN_FORM_NUM) || forms->isSet(NUMBER_FORM_NUM) || forms->isSet(numeralOrdinalForm) ||
				(forms->isSet(honorificForm) && m[I].queryForm(L"pinr") < 0)) &&
				((!forms->isSet(determinerForm) && !forms->isSet(pronounForm) &&
					!forms->isSet(indefinitePronounForm) && !forms->isSet(reciprocalPronounForm) &&
					!forms->isSet(accForm) && !forms->isSet(nomForm)) || I == principalWhere))
				relatedObjectsMap[m[I].word].insert(objects.size());
		}
		objects.push_back(thisObject);
		m[principalWhere].setObject(objects.size() - 1);
		objects[m[principalWhere].getObject()].locations.push_back(cObject::cLocation(principalWhere));
		objects[m[principalWhere].getObject()].updateFirstLocation(principalWhere);
	}
	if (adjectival)
	{
		m[principalWhere].flags |= WordMatch::flagAdjectivalObject;
		m[where].principalWhereAdjectivalPosition = principalWhere;
	}
	else
	{
		m[where].principalWherePosition = principalWhere;
		//m[end-1].principalWherePosition=principalWhere;  this causes 'medical man' not to be resolved correctly and other problems because of parsing ambiguity
	}
	m[principalWhere].beginObjectPosition = where;
	m[principalWhere].endObjectPosition = end;
	accumulateAdjectives(principalWhere);
	objects[m[principalWhere].getObject()].setSubType(identifySubType(principalWhere, objects[m[principalWhere].getObject()].partialMatch));
	if (objects[m[principalWhere].getObject()].getSubType() < 0 && (end - where) == 1 && (m[where].word->second.timeFlags&T_UNIT))
		objects[m[principalWhere].getObject()].isNotAPlace = true; // this is a time, not a place
	assignRelativeClause(principalWhere);
	addAssociatedNounsFromTitle(m[principalWhere].getObject());
	objects[m[principalWhere].getObject()].setGenericAge(m);
	if ((objects[m[principalWhere].getObject()].objectClass == NAME_OBJECT_CLASS || objects[m[principalWhere].getObject()].objectClass == NON_GENDERED_NAME_OBJECT_CLASS) &&
		objects[m[principalWhere].getObject()].name.isCompletelyNull())
		lplog(L"null name!");
	// cannot identifyISARelation because this depends on syntactic relations which are not established yet.
	return 0;
}

// suspect - for any of the Form elements at the bottom of what the Noun comes down to, if any Form is of cost 4.
// very suspect - if all of the elements are of cost 4
// ambiguous - if any of the elements have more than one form
void Source::checkObject(vector <cObject>::iterator o)
{
	LFS
		bool unambiguousHighCost = false, highCost = false;
	for (vector <WordMatch>::iterator im = m.begin() + o->begin, imEnd = m.begin() + o->end; im != imEnd && !o->ambiguous; im++)
	{
		unsigned int numWinners = 0;
		bool highCostElement = false;
		for (unsigned int formOffset = 0; formOffset < im->word->second.formsSize(); formOffset++)
			if (im->isWinner(formOffset) && im->costable())
			{
				if (im->word->second.getUsageCost(formOffset) >= 4)
					highCostElement = true;
				numWinners++;
			}
		o->ambiguous |= (numWinners > 1);
		if (numWinners == 1) unambiguousHighCost |= highCostElement;
		highCost |= highCostElement;
	}
	wstring tmp;
	if (o->PMAElement < 0)
	{
		o->verySuspect = o->suspect = highCost;
		return;
	}
	if ((unsigned)o->PMAElement >= m[o->begin].pma.count)
	{
		lplog(LOG_RESOLUTION, L"ERROR object %s has an illegal PMAElement of %d.", objectString(o, tmp, false).c_str(), o->PMAElement);
		return;
	}
	o->verySuspect = (m[o->begin].pma[o->PMAElement].cost / m[o->begin].pma[o->PMAElement].len >= 4);
	if (highCost && preferS1(o->begin, o->PMAElement))
		lplog(LOG_RESOLUTION, L"%06d:suspect object %s cancelled", o->begin, objectString(o, tmp, false).c_str());
	if (!o->ambiguous || o->verySuspect || !highCost)
	{
		o->suspect = highCost;
		return;
	}
	if (unambiguousHighCost || m[o->begin].pma[o->PMAElement].cost < 4)
	{
		o->suspect = unambiguousHighCost;
		return;
	}
	// ambiguousness is most difficult.  At some point, there was a high cost incurred for an element
	//  that had two or more winners.
	o->suspect = highCost;
}

void Source::printObjects(void)
{
	LFS
		wstring tmp;
	vector <cObject>::iterator o = objects.begin(), oEnd = objects.end();
	unsigned int numSuspectObjects = 0, numVerySuspectObjects = 0, numAmbiguousObjects = 0;
	for (unsigned int object = 0; o != oEnd; o++, object++)
	{
		if (object <= 1) continue;
		cancelSubType(object);
		if (!(m[o->begin].t.traceObjectResolution ^ flipTOROverride)) continue;
		checkObject(o);
		if (o->suspect) numSuspectObjects++;
		if (o->verySuspect) numVerySuspectObjects++;
		if (o->ambiguous) numAmbiguousObjects++;
		if (o->objectClass == PRONOUN_OBJECT_CLASS || o->objectClass == RECIPROCAL_PRONOUN_OBJECT_CLASS ||
			o->objectClass == REFLEXIVE_PRONOUN_OBJECT_CLASS || o->eliminated) continue;
		if (o->ambiguous || o->objectClass == NAME_OBJECT_CLASS)
		{
			lplog(LOG_RESOLUTION, L"%06d:%s", o->begin, objectString(o, tmp, false).c_str());
			for (set <int>::iterator di = o->duplicates.begin(), diEnd = o->duplicates.end(); di != diEnd; di++)
				lplog(LOG_RESOLUTION, L"  REPLACED #%d %s", *di, objectString(*di, tmp, false).c_str());
			if (o->objectClass == NAME_OBJECT_CLASS)
			{
				set <int> relatedObjects;
				accumulateRelatedObjects(object, relatedObjects);
				set<int>::iterator s = relatedObjects.begin(), end = relatedObjects.end();
				for (; s != end; s++)
					if (objects[*s].objectClass == NAME_OBJECT_CLASS && *s != object && !objects[*s].eliminated)
						lplog(LOG_RESOLUTION, L"    #%d %s", *s, objectString(*s, tmp, false).c_str());
			}
		}
	}
	if (numSuspectObjects || numVerySuspectObjects || numAmbiguousObjects)
		lplog(LOG_RESOLUTION, L"%d suspect objects (%d%), %d verySuspect objects (%d%), %d ambiguous objects (%d%), %d total objects",
			numSuspectObjects, numSuspectObjects * 100 / objects.size(),
			numVerySuspectObjects, numVerySuspectObjects * 100 / objects.size(),
			numAmbiguousObjects, numAmbiguousObjects * 100 / objects.size(), objects.size());
}

// determine whether a pattern that is not owned by any other pattern and has finalIfAlone set
// still has isSeparator on both sides.  If not, flag to eliminate.
bool Source::eraseWinnerFromRecalculatingAloneness(int where, patternMatchArray::tPatternMatch *pma)
{
	LFS
	cPattern *p = patterns[pma->getPattern()];
	if ((p->fillIfAloneFlag || p->onlyAloneExceptInSubPatternsFlag) &&
		!pema.ownedByOtherWinningPattern(m[where].beginPEMAPosition, pma->getPattern(), pma->len) &&
		((where && !m[where - 1].isWinnerSeparator()) ||
		(where + pma->len < (int)m.size() && !m[where + pma->len].isWinnerSeparator())))
	{
		int nPEMAPosition = pma->pemaByPatternEnd, np;
		patternElementMatchArray::tPatternElementMatch *pem = pema.begin() + nPEMAPosition;
		// check all children of this pattern
		for (; nPEMAPosition >= 0 && pem->getPattern() == pma->getPattern() && (pem->end - pem->begin) == pma->len; nPEMAPosition = pem->nextPatternElement, pem = pema.begin() + nPEMAPosition)
		{
			// go to where the child begin position is. scan all 
			for (np = m[where - pem->begin].beginPEMAPosition; np >= 0 && (np == nPEMAPosition || !pema[np].isWinner()); np = pema[np].nextByPosition);
			if (np < 0)
			{
				if (debugTrace.tracePatternElimination)
					lplog(L"position %d:pma %d:%s[%s]*%d(%d,%d) not eliminated because even though it is a FINAL_IF_ALONE and it is not alone, it will orphan position %d.",
						where, pma - m[where].pma.content, p->name.c_str(), p->differentiator.c_str(), pma->cost, where, where + pma->len, where - pem->begin);
				return false;
			}
		}
		vector <__int64> alreadyCovered;
		removeWinnerFlag(where, pma,2, alreadyCovered);
		return true;
	}
	else if (debugTrace.tracePatternElimination)
	{
		if (!(p->fillIfAloneFlag || p->onlyAloneExceptInSubPatternsFlag))
			lplog(L"position %d:pma %d:%s[%s]*%d(%d,%d) not eliminated because it does not have alone flags.",
				where, pma - m[where].pma.content, p->name.c_str(), p->differentiator.c_str(), pma->cost, where, where + pma->len);
		else if (pema.ownedByOtherWinningPattern(m[where].beginPEMAPosition, pma->getPattern(), pma->len))
			lplog(L"position %d:pma %d:%s[%s]*%d(%d,%d) not eliminated because it is owned by another winner pattern.",
				where, pma - m[where].pma.content, p->name.c_str(), p->differentiator.c_str(), pma->cost, where, where + pma->len);
		else if (!(where && !m[where - 1].isWinnerSeparator()) && !(where + pma->len < (int)m.size() && !m[where + pma->len].isWinnerSeparator()))
			lplog(L"position %d:pma %d:%s[%s]*%d(%d,%d) not eliminated because it is not alone.",
				where, pma - m[where].pma.content, p->name.c_str(), p->differentiator.c_str(), pma->cost, where, where + pma->len);
	}
	return false;
}

void Source::removeWinnerFlag(int where, patternMatchArray::tPatternMatch *pma,int recursionSpaces,vector <__int64> &alreadyCovered)
{
	__int64 wherepma = ((__int64)where << 32) + (pma - m[where].pma.content);
	if (find(alreadyCovered.begin(), alreadyCovered.end(), wherepma) != alreadyCovered.end())
		return;
	alreadyCovered.push_back(wherepma);
	cPattern *p = patterns[pma->getPattern()];
	if (!pma->isWinner()) /* OPTION */
	{
		if (debugTrace.tracePatternElimination)
			lplog(L"%*sposition %d:pma %d:%s[%s](%d,%d)*%d not a winner!", recursionSpaces - 2, L" ",
				where, pma - m[where].pma.content, p->name.c_str(), p->differentiator.c_str(), where, where + pma->len, pma->cost);
		//return;
	}
	if (debugTrace.tracePatternElimination && pma->isWinner())
		lplog(L"%*sposition %d:pma %d:%s[%s](%d,%d)*%d eliminated because it is a %s.  Winner removed.", recursionSpaces - 2, L" ",
			where, pma - m[where].pma.content, p->name.c_str(), p->differentiator.c_str(), where, where + pma->len, pma->cost,
			(recursionSpaces == 2) ? L"FINAL_IF_ALONE and it is not alone" : L"descendant of a FINAL_IF_ALONE pattern");
	for (int nPEMAPositionByPatternEnd = pma->pemaByPatternEnd; nPEMAPositionByPatternEnd >= 0; nPEMAPositionByPatternEnd = pema[nPEMAPositionByPatternEnd].nextByPatternEnd)
	{
		int nPEMAPositionByPatternElement = nPEMAPositionByPatternEnd;
		patternElementMatchArray::tPatternElementMatch *pem = pema.begin() + nPEMAPositionByPatternElement;
		if (!pem->isWinner()) /* OPTION */
		{
			if (debugTrace.tracePatternElimination)
			{
				cPattern *pemp = patterns[pem->getPattern()];
				lplog(L"%*sposition %d:pema %d:%s[%s](%d,%d)*%d not a winner!", recursionSpaces - 2, L" ",
					where, pem - pema.begin(), pemp->name.c_str(), pemp->differentiator.c_str(), where+pem->begin, where + pem->end, pem->getOCost());
			}
			//continue;
		}
		for (; nPEMAPositionByPatternElement >= 0 && pem->getPattern() == pma->getPattern() && (pem->end - pem->begin) == pma->len; nPEMAPositionByPatternElement = pem->nextPatternElement, pem = pema.begin() + nPEMAPositionByPatternElement)
		{
			// go to where the child begin position is. scan all 
			int np;
			for (np = m[where - pem->begin].beginPEMAPosition; np >= 0 && (np == nPEMAPositionByPatternElement || !pema[np].isWinner()); np = pema[np].nextByPosition);
			if (np < 0)
			{
				if (debugTrace.tracePatternElimination)
					lplog(L"%*sposition %d:pma %d:%s[%s](%d,%d)*%d not eliminated because even though it is a FINAL_IF_ALONE and it is not alone, it will orphan position %d.", recursionSpaces - 2, L" ",
						where, pma - m[where].pma.content, p->name.c_str(), p->differentiator.c_str(), where, where + pma->len, where - pem->begin, pma->cost);
				return;
			}
		}
		nPEMAPositionByPatternElement = nPEMAPositionByPatternEnd;
		pem = pema.begin() + nPEMAPositionByPatternElement;
		for (; nPEMAPositionByPatternElement >= 0 && pem->getPattern() == pma->getPattern() && (pem->end - pem->begin) == pma->len; nPEMAPositionByPatternElement = pem->nextPatternElement, pem = pema.begin() + nPEMAPositionByPatternElement)
		{
			if (!pem->isWinner()) /* OPTION */
			{
				if (debugTrace.tracePatternElimination)
				{
					cPattern *pemp = patterns[pem->getPattern()];
					lplog(L"%*sposition %d:pema %d:%s[%s](%d,%d)*%d not a winner!", recursionSpaces - 2, L" ",
						where, pem - pema.begin(), pemp->name.c_str(), pemp->differentiator.c_str(), where + pem->begin, where + pem->end, pem->getOCost());
				}
				//continue;
			}
			bool isWinner = pem->isWinner();
			pem->removeWinnerFlag();
			if (pem->isChildPattern())
			{
				if (debugTrace.tracePatternElimination && isWinner)
					lplog(L"%*sposition %d:pema %d:%s[%s](%d,%d)*%d %s[*](%d)%c eliminated because it is a descendant of a FINAL_IF_ALONE pattern.  Winner removed.", recursionSpaces, L" ", where - pem->begin, nPEMAPositionByPatternElement,
						p->name.c_str(), p->differentiator.c_str(), where, where + pma->len, pem->getOCost(),
						patterns[pem->getChildPattern()]->name.c_str(),
						pem->getChildLen() + where - pem->begin,
						(pem->flagSet(patternElementMatchArray::ELIMINATED)) ? 'E' : ' ');
				// is this the only winner parent of this child? 
				if (!pema.ownedByOtherWinningPattern(m[where - pem->begin].beginPEMAPosition, pem->getChildPattern(), pem->getChildLen()))
				{
					int pmaOffset = m[where - pem->begin].pma.queryPatternWithLen(pem->getChildPattern(), pem->getChildLen());
					if (pmaOffset == -1)
					{
						if (debugTrace.tracePatternElimination)
							lplog(L"%d:Could not find pattern %s[*][%d].", where - pem->begin, patterns[pem->getChildPattern()]->name.c_str(), pem->getChildLen() + where - pem->begin);
					}
					else
						removeWinnerFlag(where - pem->begin, m[where - pem->begin].pma.content + (pmaOffset&~matchElement::patternFlag), recursionSpaces + 2, alreadyCovered);
				}
			}
			else 	if (debugTrace.tracePatternElimination && isWinner)
				lplog(L"%*sposition %d:pema %d:%s[%s](%d,%d)*%d %s%c eliminated because it is a descendant of a FINAL_IF_ALONE pattern.  Winner removed.", recursionSpaces, L" ", where - pem->begin, nPEMAPositionByPatternElement,
					p->name.c_str(), p->differentiator.c_str(), where, where + pma->len, pem->getOCost(),
					Forms[m[where].getFormNum(pem->getChildForm())]->shortName.c_str(),
					(pem->flagSet(patternElementMatchArray::ELIMINATED)) ? 'E' : ' ');
		}
	}
	pma->removeWinnerFlag();
}

// used before winners are determined.  
// if there are any patterns matching a separator form, or 
// if there are no patterns matching on position and the separator for has lowest cost
bool Source::isAnySeparator(int where)
{
	DLFS
		for (int np = m[where].beginPEMAPosition; np >= 0; np = pema[np].nextByPosition)
			if (!pema[np].isChildPattern() && m[where].word->second.Form(pema[np].getChildForm())->isTopLevel)
				return true;
	return (m[where].word->second.flags&tFI::topLevelSeparator) != 0 && m[where].beginPEMAPosition < 0;
}

// determine whether a pattern that is not owned by any other pattern and has finalIfAlone set
// still has isSeparator on both sides.  separator is determined by all patterns matching.  If there are no patterns matching, then if lowest cost.
// if not, add high cost.
bool Source::addCostFromRecalculatingAloneness(int where, patternMatchArray::tPatternMatch *pma)
{
	LFS // DLFS
		cPattern *p = patterns[pma->getPattern()];
	//lplog(L"%d:%s[%s]*%d(%d,%d) not alone? fill=%s not owned=%s sep begin=%s sep end=%s.",
	//			where,p->name.c_str(),p->differentiator.c_str(),pma->cost,where,where+pma->len,
	//				(p->fillIfAloneFlag || p->onlyAloneExceptInSubPatternsFlag) ? L"true":L"false",
	//								(!pema.ownedByOtherPattern(m[where].beginPEMAPosition,pma->getPattern(),pma->len)) ? L"true":L"false",
	//			(where && !isAnySeparator(where-1)) ? L"true":L"false",
	//			(where+pma->len<(int)m.size() && !isAnySeparator(where+pma->len)) ? L"true":L"false");
	if ((p->fillIfAloneFlag || p->onlyAloneExceptInSubPatternsFlag) &&
		!pema.ownedByOtherPattern(m[where].beginPEMAPosition, pma->getPattern(), pma->len) &&
		((where && !isAnySeparator(where - 1)) ||
		(where + pma->len < (int)m.size() && !isAnySeparator(where + pma->len))))
	{
		int cost = 0;
		if (where && !isAnySeparator(where - 1)) cost += m[where - 1].word->second.lowestSeparatorCost() / 1000;
		if (where + pma->len < (int)m.size() && !isAnySeparator(where + pma->len)) cost += m[where + pma->len].word->second.lowestSeparatorCost() / 1000;
		if (cost)
		{
			int nPEMAPosition = pma->pemaByPatternEnd;
			patternElementMatchArray::tPatternElementMatch *pem = pema.begin() + nPEMAPosition;
			if (debugTrace.traceParseInfo)
				lplog(L"%d:%s[%s]*%d(%d,%d) added cost %d because it is a FINAL_IF_ALONE and it is not alone.",
			where, p->name.c_str(), p->differentiator.c_str(), pma->cost, where, where + pma->len, cost);
			for (; nPEMAPosition >= 0 && pem->getPattern() == pma->getPattern() && (pem->end - pem->begin) == pma->len; nPEMAPosition = pem->nextPatternElement, pem = pema.begin() + nPEMAPosition)
				pem->addOCostTillMax(cost);
			pma->addCostTillMax(cost);
			return true;
		}
	}
	return false;
}

// acumulate information about all objects and sort by index of m
// TAGS: NOUN, VNOUN, PNOUN, NAME, NAMEOWNER, N_AGREE where N_AGREE is possessive_pronoun, pronoun, indefinite_pronoun, personal_pronoun_nominative, noun, country OR GNOUN==NAME
// NOUN gives  XXX    __NOUN "2" (includes _NAME as GNOUN)    "The Pool"
//             XXX           "3" (includes _NAME as GNOUN)    "All these old things"
//             XXX           "5" (includes BLOCKED __APPNOUN) "My son the doctor", "The news I gave"
//             XXX           "B"                              "The 'Cordettes'"
//             XXX           "N"                              "The L-8"
//             XXX           "E"                              "the man spending"
//             XXX           "F"                              "the man spending money"
//             XXX           "X"                              "The news I gave Bill"
//                 __APPNOUN "1"                              required by __NOUN "5"
//                 __APPNOUN "2"                              required by __NOUN "5"
//                 __APPNOUN2                                 required by __NOUN "X"
// PNOUN gives XXX  _NOUN_OBJ                                 "yourself", "each other", "what", "whom", etc
//             XXX    __NOUN "C"                              "you", "he", "she", "them"
// GNOUN gives XXX _NAME(PLURAL)
//                 _ADJECTIVE, _INFP, __NAMEOWNER,
//                 _REL1, _VERBREL2
//                    __NOUN "4"(PLURAL)                      "All the best"
//                           "7"(PLURAL)                      "The oranges and the apples"
//                           "8"                              addresses, dates, times, and telephone numbers
//                           "D"                              "stealing oranges"
// NO TAG GIVES       __NOUN "A"                              "this woman, whoever she was,"
// SO the acceptable concrete objects are marked in XXX
// the largest NOUN that is not of type '7', 'A', or 'B', a DATE, or in a sectionHEADER
//                  or is a NOUN_OBJ or NAME or NAMEOWNER 'A' or 'B'
// identify second objects of type '5'
void Source::identifyObjects(void)
{
	LFS
		preparePrepMap();
	vector <WordMatch>::iterator im = m.begin(), imend = m.end();
	int lastProgressPercent = -1;
	//int lastEnd = -1;
	unsigned int tagInSet;
	// pre-process patterns
	for (unsigned int I = 0; I < patterns.size(); I++)
	{
		cPattern *p = patterns[I];
		tagInSet = 0;
		p->objectContainer = (p->allElementTags.isSet(PREP_TAG) || p->allElementTags.isSet(SUBOBJECT_TAG) ||
			p->allElementTags.isSet(PREP_OBJECT_TAG) || p->allElementTags.isSet(VERB_TAG) ||
			p->hasTag(PLURAL_TAG) || p->allElementTags.isSet(REOBJECT_TAG));
		if (p->tagSetMemberInclusion[objectTagSet])  // patterns having these subpatterns will hide other objects in them
			p->objectTag = p->hasTagInSet(objectTagSet, tagInSet);
	}
	// create a narrator object = 0
	cObject narratorObject;
	objects.push_back(narratorObject);
	objects[0].objectClass = NAME_OBJECT_CLASS;
	objects[0].begin = objects[0].end = objects[0].originalLocation = 0;
	// create an audience object = 1
	cObject audienceObject;
	objects.push_back(audienceObject);
	objects[1].objectClass = NAME_OBJECT_CLASS;
	objects[1].begin = objects[1].end = objects[1].originalLocation = 0;
	im = m.begin();
	for (unsigned int I = 0; I < m.size() && !exitNow; im++, I++)
	{
		if (im->word->first == L"?")
		{
			int J = I - 1;
			vector <WordMatch>::iterator imtmp = im;
			for (imtmp--; J >= 0 && !isEOS(J) && imtmp->word != Words.sectionWord && imtmp->word->second.query(quoteForm) < 0; imtmp--, J--)
			{
				if (imtmp->getObject() != -1 || imtmp->queryWinnerForm(relativizerForm) != -1)
					imtmp->flags |= WordMatch::flagInQuestion;
				// sit down, will you?
				if (imtmp->pma.queryPattern(L"_Q1S") != -1 && J > 0 && m[J - 1].word->first == L",")
				{
					break;
				}
				// Look here[here] , Tuppence , old girl[tuppence] , EXITwhat is this going to lead to ? ”
				if (imtmp->pma.queryPattern(L"_Q2") != -1 && J > 0 && m[J - 1].word->first == L",")
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:question backwards propagation stopped.", J);
					break;
				}
			}
			imtmp++;
			imtmp->flags |= WordMatch::flagInQuestion;
		}
		if (im->getObject() >= 0) continue;
		debugTrace.traceSpeakerResolution = im->t.traceSpeakerResolution;
		debugTrace.traceObjectResolution = im->t.traceObjectResolution;
		debugTrace.traceNameResolution = im->t.traceNameResolution;
		if ((int)(I * 100 / m.size()) > lastProgressPercent)
		{
			lastProgressPercent = (int)I * 100 / m.size();
			wprintf(L"PROGRESS: %03d%% objects identified with %d seconds elapsed \r", lastProgressPercent, clocksec());
		}
		unsigned int element = -1, elementContainer = -1;
		int maxLen = -1, maxContainerEnd = -1, lastTag = -1, lastContainerTag = -1, lowestCost = 10000;
		bool nameFound = false;
		// find a pattern that is a noun, vnoun, pnoun, gnoun, name, without containing a prepositional phrase or subobject
		// give preference to names and the greatest length
		patternMatchArray::tPatternMatch *pma = im->pma.content, *saveOverride = NULL;
		for (unsigned int PMAElement = 0; PMAElement < im->pma.count; PMAElement++, pma++)
		{
			cPattern *p = patterns[pma->getPattern()];
			if (p->name == L"_VERBREL1") saveOverride = pma;
			if (p->objectTag >= 0 && !p->objectContainer && pma->len >= maxLen && !(pma->len == maxLen && nameFound))
			{
				// don't extend noun if the additional word is very high cost (4)
				if (pma->len == maxLen + 1 && pma->cost >= lowestCost + 4)
					continue;
				// get lowest cost
				if (pma->len == maxLen && pma->cost >= lowestCost)
					continue;
				// if there is another pattern matching begin and end with a lower cost, continue.
				// this eliminates NOUNs that have been matched incorrectly such as NOUNs that have been
				//  matched as part of another NOUN[B] which matches quotes that are not part of quoted strings
				// but rather primary quotes which should not have been matched (but are because quoted strings are not analyzed until later)
				patternMatchArray::tPatternMatch *pm2 = im->pma.content;
				for (unsigned int PE2 = 0; PE2 < im->pma.count; PE2++, pm2++)
					if (pm2->len == pma->len &&
						(pm2->cost < pma->cost || (pm2->cost == pma->cost && patterns[pm2->getPattern()]->name == L"__S1")) &&
						patterns[pm2->getPattern()]->isTopLevelMatch(*this, I, I + pm2->len))
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, L"%06d:%s[%s]*%d(%d,%d) eliminated because it has a higher cost than %s[%s]*%d(%d,%d).",
								I, p->name.c_str(), p->differentiator.c_str(), pma->cost, I, I + pma->len,
								patterns[pm2->getPattern()]->name.c_str(), patterns[pm2->getPattern()]->differentiator.c_str(), pm2->cost, I, I + pma->len);
						break;
					}
				if (pm2 != im->pma.content + im->pma.count) continue;
				// sanity check - do not allow nouns to have dashes in the middle 'Whittington -- there' prevents inappropriate adjectives
				// allow close-cropped hair
				if (pma->len >= 3 && m[I].queryWinnerForm(PROPER_NOUN_FORM_NUM) >= 0 && m[I + 1].word->second.query(dashForm) >= 0) continue;
				nameFound = (patternTagStrings[lastTag = p->objectTag] == L"NAME");
				maxLen = pma->len;
				lowestCost = pma->cost;
				element = PMAElement | matchElement::patternFlag;
			}
			else if (p->objectTag >= 0 && p->objectContainer && (int)I + pma->len >= maxContainerEnd)
			{
				lastContainerTag = p->objectTag;
				maxContainerEnd = I + pma->len;
				elementContainer = PMAElement;
			}
		}
		// prevent objects containing other objects to overwrite their subobjects if in the same position
		if (lastTag >= 0) lastContainerTag = -1;
		// this is to prevent portions of VERBREL's being taken as objects, and then the object is improperly taken as including the preceding verb,
		// which can mess up speaker resolution 29414:"thought Tuppence to herself"
		if (saveOverride && lastTag >= 0 && maxLen > 1 && saveOverride->len >= maxLen && saveOverride->cost < lowestCost && m[I].queryWinnerForm(verbForm) >= 0)
		{
			cPattern *p = patterns[im->pma[element&~matchElement::patternFlag].getPattern()];
			cPattern *vp = patterns[saveOverride->getPattern()];
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:%s[%s]*%d(%d,%d) eliminated because of lower cost VERBREL %s[%s]*%d(%d,%d).",
					I, p->name.c_str(), p->differentiator.c_str(), lowestCost, I, I + maxLen,
					vp->name.c_str(), vp->differentiator.c_str(), saveOverride->cost, I, I + saveOverride->len);
			lastTag = -1;
		}
		// accusative pronouns never have adverb post-modifiers - this is a misparse.  'her' may be used however in a long noun phrase  
		// otherwise 'as' will be an adverb and Rita Vandermeyer will be split into two names
		// We[tommy,woman] had her[marguerite] down as Rita Vandemeyer 
		// I[julius] came over here determined to find her[jane] and fix it[here] all[all] up , and take her[jane] back as Mrs . Julius P . Hersheimmer ! 
		if (m[I].queryWinnerForm(accForm) >= 0 && I + 1 < m.size() && m[I + 1].queryWinnerForm(adverbForm) >= 0 && maxLen > 4)
			element = maxLen = lastTag = -1;
		if (lastTag >= 0 || m[I].queryWinnerForm(PROPER_NOUN_FORM_NUM) >= 0 ||
			m[I].queryWinnerForm(personalPronounForm) >= 0 ||
			m[I].queryWinnerForm(possessivePronounForm) >= 0 ||
			(m[I].queryForm(pronounForm) >= 0 && (m[I].queryWinnerForm(adverbForm) < 0 || I + 1 >= m.size() || m[I + 1].queryWinnerForm(PROPER_NOUN_FORM_NUM) < 0)) || // snags pronouns like 'another' which are more commonly used as adjectives but are valid objects but not 'so'
			m[I].queryWinnerForm(accForm) >= 0)
		{
			identifyObject(lastTag, I, element, false, -1, -1);
			if (m[I].getObject() >= 0 && objects[m[I].getObject()].objectClass == NAME_OBJECT_CLASS && debugTrace.traceNameResolution)
			{
				wstring tmpstr;
				lplog(LOG_RESOLUTION, L"%06d:name %s encountered", I, objectString(m[I].getObject(), tmpstr, false).c_str());
			}
			if (maxLen <= 0) maxLen = 1;
			I += maxLen - 1;
			im += maxLen - 1;
		}
		else if (lastContainerTag >= 0)
		{
			cName name;
			cObject thisObject(VERB_OBJECT_CLASS, name, I, maxContainerEnd, I, elementContainer, -1, false, false, true, false, true);
			int principalWhere = I;
			objects.push_back(thisObject);
			m[principalWhere].setObject(objects.size() - 1);
			objects[m[principalWhere].getObject()].locations.push_back(cObject::cLocation(principalWhere));
			objects[m[principalWhere].getObject()].updateFirstLocation(principalWhere);
			m[I].principalWherePosition = principalWhere;
			m[principalWhere].beginObjectPosition = I;
			m[principalWhere].endObjectPosition = maxContainerEnd;
			objects[m[principalWhere].getObject()].resetSubType();
		}
	}
	wprintf(L"PROGRESS: 100%% objects identified with %d seconds elapsed \n", clocksec());
}
