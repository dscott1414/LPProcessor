#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "profile.h"
#include <iterator>

// meta group object types:
// example:        latestOwnerWhere
// another ally    -3                 metagroup word at principalWhere, with a wordOrder word (another) as modifier
bool cSource::resolveMetaGroupWordOrderedFutureObject(int where, vector <cOM>& objectMatches)
{
	LFS
		// for now, just match all speakers in the currentSpeakerGroup that appear for the first time in the speakerGroup after where.
		if (currentSpeakerGroup >= speakerGroups.size()) return false;
	for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].speakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].speakers.end(); si != siEnd; si++)
	{
		if (locationBefore(*si, where) < speakerGroups[currentSpeakerGroup].sgBegin)
			objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
	}
	if (objectMatches.size() == 1)
		replaceObjectInSection(where, objectMatches[0].object, m[where].getObject(), L"resolveMetaGroupWordOrderedFutureObject");
	return objectMatches.size() > 0;
}

bool cSource::resolveMetaGroupFormerLatter(int where, int previousS1, int latestOwnerWhere, vector <cOM>& objectMatches)
{
	LFS
		int secondaryChoice = previousS1, pw = -1, pwo = -1, element = -1;
	for (secondaryChoice--; secondaryChoice >= 0 && !isEOS(secondaryChoice) && !m[secondaryChoice].pma.findMaxLen(L"__INFPSUB", element); secondaryChoice--);
	// Shall ESTABwe[julius,tuppence] have it[lunch] up here , or EXITgo down to the restaurant ? 
	if (secondaryChoice >= 0 && !isEOS(secondaryChoice) && element >= 0 && m[secondaryChoice].pma[element].len + secondaryChoice < where && latestOwnerWhere == -8)
	{
		if (m[secondaryChoice + 1].getObject() >= 0)
			objectMatches.push_back(cOM(m[secondaryChoice + 1].getObject(), SALIENCE_THRESHOLD));
		else if (m[secondaryChoice + 1].getRelObject() >= 0 && m[m[secondaryChoice + 1].getRelObject()].getObject() >= 0)
			objectMatches.push_back(cOM(m[m[secondaryChoice + 1].getRelObject()].getObject(), SALIENCE_THRESHOLD));
		if (m[secondaryChoice + 1].relPrep >= 0 && m[m[secondaryChoice + 1].relPrep].getRelObject() >= 0 && m[m[m[secondaryChoice + 1].relPrep].getRelObject()].getObject() >= 0)
			objectMatches.push_back(cOM(m[m[m[secondaryChoice + 1].relPrep].getRelObject()].getObject(), SALIENCE_THRESHOLD));
		wstring tmpstr;
		lplog(LOG_RESOLUTION, L"%06d:%S (latter) matched %s.", where, __FUNCTION__, objectString(objectMatches, tmpstr, true).c_str());
		return false;
	}
	for (previousS1--; previousS1 >= 0 && !isEOS(previousS1) && !m[previousS1].pma.findMaxLen(L"__S1", element) && !m[previousS1].pma.findMaxLen(L"_Q2", element); previousS1--);
	bool allIn, oneIn;
	if (previousS1 >= 0 && element != -1 && (m[previousS1].objectRole & SUBJECT_ROLE) &&
		m[previousS1].pma[element].len + previousS1 < where &&
		(pw = m[previousS1].principalWherePosition) >= 0 && m[pw].getObject() >= 0)
	{
		if ((pwo = m[pw].getRelObject()) < 0 && (pwo = m[pw].getRelVerb()) >= 0)
		{
			pwo++;
			while (m[pwo].queryWinnerForm(adverbForm) >= 0) pwo++;
			pwo = (m[pwo].queryWinnerForm(prepositionForm) >= 0) ? m[pwo].getRelObject() : -1;
		}
		if (pwo >= 0 &&
			intersect(pw, speakerGroups[currentSpeakerGroup].speakers, allIn, oneIn) &&
			intersect(pwo, speakerGroups[currentSpeakerGroup].speakers, allIn, oneIn))
		{
			if (latestOwnerWhere == -7)
			{
				if (m[pw].objectMatches.empty())
					objectMatches.push_back(cOM(m[pw].getObject(), SALIENCE_THRESHOLD));
				else
					objectMatches = m[pw].objectMatches;
			}
			else if (latestOwnerWhere == -8)
			{
				if (m[pwo].objectMatches.empty())
					objectMatches.push_back(cOM(m[pwo].getObject(), SALIENCE_THRESHOLD));
				else
					objectMatches = m[pwo].objectMatches;
			}
			wstring tmpstr;
			lplog(LOG_RESOLUTION, L"%06d:%S (latter & former) matched %s.", where, __FUNCTION__, objectString(objectMatches, tmpstr, true).c_str());
			return false;
		}
	}
	return true;
}

// match against "former, "latter" or "first", "second" against plural objects
// example:        latestOwnerWhere
// first           -5                 wordOrder word at principalWhere
// wchar_t *wordOrderWords[]={L"other",L"another",L"second",L"first",L"third",L"former",L"latter",L"that",L"this",L"two",L"three",NULL};
//                            -2       -3         -4        -5       -6       -7        -8        -9      -10     -11    -12
// the latter, the first
bool cSource::resolveMetaGroupFirstSecondThirdWordOrderedObject(int where, int lastBeginS1, vector <cOM>& objectMatches, int latestOwnerWhere)
{
	LFS
		if (latestOwnerWhere > -4 || latestOwnerWhere < -8) return false;
	if ((latestOwnerWhere == -7 || latestOwnerWhere == -8) && currentSpeakerGroup < speakerGroups.size() && lastBeginS1 >= 0)
	{
		int previousS1 = lastBeginS1;
		if (!resolveMetaGroupFormerLatter(where, previousS1, latestOwnerWhere, objectMatches)) return false;
		for (; previousS1 >= 0 && !isEOS(previousS1) && m[previousS1].word != Words.sectionWord; previousS1--);
		if (previousS1 > 2 && m[previousS1].word == Words.sectionWord && m[previousS1 - 1].queryForm(quoteForm) != -1)
			previousS1 -= 2;
		if (previousS1 >= 0 && isEOS(previousS1))
		{
			if (!resolveMetaGroupFormerLatter(where, previousS1, latestOwnerWhere, objectMatches)) return false;
		}
	}
	bool physicallyEvaluated, matchPhysicallyEvaluated, physicallyPresent = physicallyPresentPosition(where, physicallyEvaluated), matchPhysicallyPresent;
	// scan for plural objects and objects having MPLURAL role.  Go back two sentences, max.
	// if there is an mplural object with enough objects to satisfy latestOwnerWhere, pick this and return.
	// if there is a plural object, pick this, and continue.
	vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsEnd = localObjects.end();
	for (; lsi != lsEnd; lsi++)
		if (lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge))
			objects[lsi->om.object].lsiOffset = lsi;
	int age = 0;
	for (int I = where - 1; I >= 0 && age < 2; I--)
	{
		int o = m[I].getObject();
		if ((o < 0 || (lsi = objects[o].lsiOffset) == cNULL) && m[I].objectMatches.size())
		{
			for (unsigned int J = 0; J < m[I].objectMatches.size(); J++)
				if ((lsi = objects[o = m[I].objectMatches[J].object].lsiOffset) != cNULL)
					break;
		}
		if (o >= 0 && (lsi = objects[o].lsiOffset) != cNULL && (objects[o].plural || (m[I].objectRole & MPLURAL_ROLE)))
		{
			matchPhysicallyPresent = physicallyPresentPosition(where, matchPhysicallyEvaluated);
			if (physicallyEvaluated && matchPhysicallyEvaluated && physicallyPresent != matchPhysicallyPresent)
				continue;
			if (objects[o].plural)
				objectMatches.push_back(cOM(o, SALIENCE_THRESHOLD));
			else if ((m[I].previousCompoundPartObject >= 0 && (latestOwnerWhere == -4 || latestOwnerWhere == -6 || latestOwnerWhere == -8)) ||
				(m[I].nextCompoundPartObject >= 0 && (latestOwnerWhere == -5 || latestOwnerWhere == -7)))
			{
				objectMatches.push_back(cOM(o, SALIENCE_THRESHOLD));
				break;
			}
		}
		if (m[I].pma.queryPattern(L"__S1") != -1)
			age++;
	}
	for (lsi = localObjects.begin(); lsi != lsEnd; lsi++)
		objects[lsi->om.object].lsiOffset = cNULL;
	if (debugTrace.traceSpeakerResolution)
	{
		wstring tmpstr;
		lplog(LOG_RESOLUTION, L"%06d:%S matched %s.", where, __FUNCTION__, objectString(objectMatches, tmpstr, true).c_str());
	}
	return false;
}

bool cSource::resolveMetaGroupPlural(int latestOwnerWhere, bool inQuote, vector <cOM>& objectMatches)
{
	LFS
		vector <cSpeakerGroup>::iterator csg = speakerGroups.begin() + currentSpeakerGroup;
	// a young couple
	// the two men - latestOwnerWhere is -11.  The two - latestOwnerWhere is -1.
	if (latestOwnerWhere < -1) return false;
	set <int>* speakers = (csg->groupedSpeakers.size()) ? &csg->groupedSpeakers : &csg->speakers;
	// if inQuote and grouped speakers are the current speaker, back out of this and go to something more sophisticated.
	if (inQuote)
	{
		if (previousSpeakers.empty()) return false;
		for (set<int>::iterator si = speakers->begin(), siEnd = speakers->end(); si != siEnd; si++)
			if (find(previousSpeakers.begin(), previousSpeakers.end(), *si) != previousSpeakers.end())
				return false;
	}
	for (set<int>::iterator si = speakers->begin(), siEnd = speakers->end(); si != siEnd; si++)
		objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
	return objectMatches.size() != 0;
}

#define MAX_WORD_ORDER_SEARCH 200
// the other crook
// not used for generic others, like 'the other man'
// not used for ownership, like 'the other's face'
bool cSource::resolveMetaGroupSpecifiedOther(int where, int latestOwnerWhere, bool inQuote, vector <cOM>& objectMatches)
{
	LFS
		// make sure it is an 'other' metagroup object and other is not the principal word.
		if (latestOwnerWhere != -2 || m[where].word->first == L"other" || (m[where].word->second.flags & cSourceWordInfo::genericGenderIgnoreMatch))
			return false;
	int otherWhere = -1;
	for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition; I++)
		if (m[I].word->first == L"other")
			otherWhere = I;
	if (otherWhere < 0 || (m[otherWhere].flags & cWordMatch::flagNounOwner)) return false;
	// get principal word and search through local objects to see whether there is a plural of that
	tIWMM word = m[where].word->second.mainEntry;
	int whereMatches = -1;
	printLocalFocusedObjects(where, 0);
	for (vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsEnd = localObjects.end(); lsi != lsEnd; lsi++)
		if (objects[lsi->om.object].objectClass != NAME_OBJECT_CLASS &&
			lsi->om.object != m[where].getObject() && lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge) && m[objects[lsi->om.object].originalLocation].word->second.mainEntry == word &&
			objects[lsi->om.object].plural && lsi->lastWhere >= 0)
		{
			if (m[lsi->lastWhere].objectMatches.empty()) return false;
			// get the matches for the plural.
			whereMatches = lsi->lastWhere;
			break;
		}
	if (whereMatches < 0) return false;
	// search backwards for the most recent mention of any of the matches.
	//int o=m[where].getObject();
	bool inBQuote = inQuote, allIn, oneIn; // in backwards quote
	int wordsTraversed = 0;
	wstring tmpstr, tmpstr2, tmpstr3;
	for (int I = where - 1; I >= 0; I--)
	{
		if (m[I].word->first == L"“")
			inBQuote = false;
		if (m[I].word->first == L"”")
			inBQuote = true;
		if (inBQuote != inQuote) continue;
		if (wordsTraversed++ > MAX_WORD_ORDER_SEARCH) break;
		if (intersect(I, m[whereMatches].objectMatches, allIn, oneIn))
		{
			// return objectMatches=matches - the most recent mention.
			objectMatches = m[whereMatches].objectMatches;
			subtract(m[I].getObject(), objectMatches);
			subtract(objectMatches, m[I].objectMatches);
			return objectMatches.size() > 0;
		}
	}
	return false;
}

int cSource::checkIfOne(int I, int latestObject, set <int>* speakers)
{
	LFS
		if (m[I].word->first == L"one" && m[I].objectMatches.size() && speakers->find(latestObject) != speakers->end())
		{
			for (set <int>::iterator si = speakers->begin(), siEnd = speakers->end(); si != siEnd; si++)
				if (*si != latestObject && in(*si, m[I].objectMatches) == m[I].objectMatches.end())
					return *si;
		}
	// one side
	bool containsOne = false;
	for (int J = m[I].beginObjectPosition; J < m[I].endObjectPosition && !containsOne; J++)
		containsOne = (J != I && m[J].word->first == L"one");
	if (containsOne)
		return m[I].getObject();
	return -1;
}

bool cSource::resolveMetaGroupGenericOther(int where, int latestOwnerWhere, bool inQuote, vector <cOM>& objectMatches)
{
	LFS
		// the other hand, the other leg, etc should be matched to another body object only
		bool singularBodyPart;
	if (m[where].getObject() >= 0 && isExternalBodyPart(where, singularBodyPart, true))
		return false;
	// the other first means 'the other' first /  STAYWe[tuppence,tommy] shall keep it to the last and ENTERopen the other first . ” 
	if (m[where].queryWinnerForm(numeralOrdinalForm) >= 0)
		return false;
	vector <cSpeakerGroup>::iterator csg = speakerGroups.begin() + currentSpeakerGroup;
	bool physicallyEvaluated, physicallyPresent = physicallyPresentPosition(where, m[where].beginObjectPosition, physicallyEvaluated, false) && physicallyEvaluated;
	wstring tmpstr, tmpstr2, tmpstr3;
	int numEOS = 0, numSectionWord = 0;
	if (lastOpeningPrimaryQuote >= 0 && m[lastOpeningPrimaryQuote].endQuote >= 0)
		for (int I = m[lastOpeningPrimaryQuote].endQuote; I < where && numEOS < 2 && numSectionWord < 2; I++)
		{
			if (isEOS(I)) numEOS++;
			if (m[I].word == Words.sectionWord) numSectionWord++;
		}
	// if physically present subject of first sentence after quote
	if (!inQuote && physicallyPresent && (m[where].objectRole & SUBJECT_ROLE) && numEOS == 0 && numSectionWord == 1 && lastOpeningPrimaryQuote >= 0 &&
		(m[lastOpeningPrimaryQuote].audienceObjectMatches.size() == 1 || in(*csg->povSpeakers.begin(), m[lastOpeningPrimaryQuote].audienceObjectMatches) != m[lastOpeningPrimaryQuote].audienceObjectMatches.end()))
	{
		objectMatches = m[lastOpeningPrimaryQuote].audienceObjectMatches;
		if (objectMatches.size() > 1 && csg->povSpeakers.size())
			subtract(objectMatches, csg->povSpeakers);
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther: audience immediately after speaker lastOpeningPrimaryQuote=%d:%s", where, lastOpeningPrimaryQuote, objectString(objectMatches, tmpstr3, true).c_str());
		return true;
	}
	if ((m[where].objectRole & PRIMARY_SPEAKER_ROLE) ||
		(!(m[where].flags & cWordMatch::flagAdjectivalObject) && !inQuote && physicallyPresent && csg->povSpeakers.size() &&
			(csg->speakers.size() == 2 || (csg->speakers.size() == 3 && csg->speakers.find(m[where].getObject()) != csg->speakers.end()))))
	{
		// go back to the preceding sentence.  If this sentence passes across a sectionWord, and the preceding sentence has beginning quote, with a definite speaker, this is the last speaker. 
		// if immediately preceding last speaker was not povSpeaker, then skip this routine.
		// “[edgerton:dr] That remains to be seen , ” said Sir James gravely .
		// The other[dr] hesitated .
		if (lastOpeningPrimaryQuote >= 0 && (m[lastOpeningPrimaryQuote].flags & cWordMatch::flagDefiniteResolveSpeakers) &&
			m[lastOpeningPrimaryQuote].speakerPosition >= 0 && csg->povSpeakers.size() == 1)
		{
			if (numEOS == 1 && numSectionWord == 1 && !in(*csg->povSpeakers.begin(), m[lastOpeningPrimaryQuote].speakerPosition))
			{
				objectMatches.push_back(cOM(*csg->povSpeakers.begin(), SALIENCE_THRESHOLD));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther:pov override csg=%s lastOpeningPrimaryQuote=%d = %s", where, toText(*csg, tmpstr), lastOpeningPrimaryQuote, objectString(objectMatches, tmpstr3, true).c_str());
				return true;
			}
		}
		// can't look back to other sentences because we could skip back an odd # of sentences.  Much more reliable to match every speaker, then let other routines sort it out.
		vector <cOM> omlo, omplo;
		for (set <int>::iterator si = csg->speakers.begin(), siEnd = csg->speakers.end(); si != siEnd; si++)
		{
			vector <cLocalFocus>::iterator lsi;
			if ((lsi = in(*si)) != localObjects.end())
			{
				omlo.push_back(cOM(*si, SALIENCE_THRESHOLD));
				if (lsi->physicallyPresent)
					omplo.push_back(cOM(*si, SALIENCE_THRESHOLD));
			}
		}
		if (omplo.size() > 1)
			objectMatches = omplo;
		else if (omlo.size() > 1)
			objectMatches = omlo;
		else
			for (set <int>::iterator si = csg->speakers.begin(), siEnd = csg->speakers.end(); si != siEnd; si++)
				objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
		if ((m[where].objectRole & PRIMARY_SPEAKER_ROLE) && previousPrimaryQuote >= 0 && m[previousPrimaryQuote].objectMatches.size() == 1)
			subtract(m[previousPrimaryQuote].objectMatches[0].object, objectMatches);
		subtract(m[where].getObject(), objectMatches);
		if (objectMatches.size() > 1 && csg->povSpeakers.size())
			subtract(objectMatches, csg->povSpeakers);
		bool allIn, oneIn;
		if (previousPrimaryQuote >= 0)
		{
			if (intersect(m[previousPrimaryQuote].audienceObjectMatches, objectMatches, allIn, oneIn) && allIn)
				objectMatches = m[previousPrimaryQuote].audienceObjectMatches;
			int pq = m[previousPrimaryQuote].previousQuote;
			if (pq < 0 && m[previousPrimaryQuote].quoteBackLink >= 0)
				pq = m[m[previousPrimaryQuote].quoteBackLink].previousQuote;
			if (objectMatches.size() > 1 && pq >= 0 &&
				intersect(m[pq].objectMatches, objectMatches, allIn, oneIn) && oneIn)
			{
				vector <cOM> om;
				for (vector <cOM>::iterator omi = objectMatches.begin(), omiEnd = objectMatches.end(); omi != omiEnd; omi++)
					if (in(omi->object, m[pq].objectMatches) != m[pq].objectMatches.end())
						om.push_back(*omi);
				lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther:SOH objectMatches=%s before previous=%s -> %s", where,
					objectString(objectMatches, tmpstr, true).c_str(),
					objectString(m[pq].objectMatches, tmpstr2, true).c_str(),
					objectString(om, tmpstr, true).c_str());
				objectMatches = om;
			}
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther:csg=%s %s previousPrimaryQuote=%d:%s = %s", where,
					toText(*csg, tmpstr), (m[where].objectRole & PRIMARY_SPEAKER_ROLE) ? L"PRIMARY" : L"NOT_PRIMARY", previousPrimaryQuote,
					(previousPrimaryQuote >= 0) ? objectString(m[previousPrimaryQuote].objectMatches, tmpstr2, true).c_str() : L"",
					objectString(objectMatches, tmpstr3, true).c_str());
			return objectMatches.size() == 1;
		}
	}
	set <int>* speakers = (csg->groupedSpeakers.size()) ? &csg->groupedSpeakers : &csg->speakers;
	int o = m[where].getObject();
	bool inBQuote = inQuote, crossQuotes = false; // in backwards quote
	int wordsTraversed = 0;
	for (int I = where - 1; I >= 0 && objectMatches.empty(); I--)
	{
		if (m[I].word->first == L"“")
		{
			inBQuote = false;
			crossQuotes = true;
		}
		if (m[I].word->first == L"”")
		{
			inBQuote = true;
			crossQuotes = true;
		}
		if (inBQuote != inQuote) continue;
		if (wordsTraversed++ > MAX_WORD_ORDER_SEARCH) break;
		int latestObject = m[I].getObject();
		if (m[I].objectMatches.size() == 1)
			latestObject = m[I].objectMatches[0].object;
		if (m[I].objectMatches.size() > 1)
			return false;
		// one or the other // one to the other / one side
		if (latestObject >= 0 && ((wordsTraversed < 30 && !crossQuotes) || (wordsTraversed < 10)))
		{
			int co = checkIfOne(I, latestObject, speakers);
			if (co < 0 && (m[I].objectRole & OBJECT_ROLE) && m[I].relSubject >= 0 && m[m[I].relSubject].getObject() >= 0)
				co = checkIfOne(m[I].relSubject, m[m[I].relSubject].getObject(), speakers);
			if (co >= 0)
			{
				objectMatches.push_back(cOM(co, SALIENCE_THRESHOLD));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther (one) %s", where, objectString(co, tmpstr, true).c_str());
				return true;
			}
		}
		if (latestObject < 0 || objects[latestObject].neuter || (!objects[latestObject].male && !objects[latestObject].female)) continue;
		int lastSpeakerGroup = getLastSpeakerGroup(latestObject, currentSpeakerGroup + 1);
		if (lastSpeakerGroup < 0) continue;
		vector <cSpeakerGroup>::iterator sg = speakerGroups.begin() + lastSpeakerGroup;
		vector <int> otherObjects;
		if (sg->groupedSpeakers.find(latestObject) != sg->groupedSpeakers.end())
		{
			for (set <int>::iterator sgi = sg->groupedSpeakers.begin(), sgiEnd = sg->groupedSpeakers.end(); sgi != sgiEnd; sgi++)
				if (*sgi != latestObject && *sgi != o && objects[o].matchGender(objects[*sgi]))
				{
					objectMatches.push_back(cOM(*sgi, SALIENCE_THRESHOLD));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther (grouped) with %d:%s - %s in %s", where, I, objectString(latestObject, tmpstr, true).c_str(), objectString(*sgi, tmpstr2, true).c_str(), toText(*sg, tmpstr3));
				}
		}
		if (objectMatches.empty() && !(m[where].objectRole & PRIMARY_SPEAKER_ROLE))
		{
			for (vector < cSpeakerGroup::cGroup >::iterator sgg = sg->groups.begin(), sggEnd = sg->groups.end(); sgg != sggEnd; sgg++)
				if (find(sgg->objects.begin(), sgg->objects.end(), latestObject) != sgg->objects.end())
				{
					bool allMatchGender = true;
					if (objects[latestObject].male ^ objects[latestObject].female)
						for (vector <int>::iterator sggi = sgg->objects.begin(), sggiEnd = sgg->objects.end(); sggi != sggiEnd && allMatchGender; sggi++)
							allMatchGender = objects[latestObject].matchGender(objects[*sggi]);
					if (allMatchGender)
						for (vector <int>::iterator sggi = sgg->objects.begin(), sggiEnd = sgg->objects.end(); sggi != sggiEnd; sggi++)
							if (*sggi != latestObject && *sggi != o)
							{
								objectMatches.push_back(cOM(*sggi, SALIENCE_THRESHOLD));
								if (debugTrace.traceSpeakerResolution)
									lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther (grouped) with %d:%s - %s in %s", where, I, objectString(latestObject, tmpstr, true).c_str(), objectString(*sggi, tmpstr2, true).c_str(), toText(*sg, tmpstr3));
							}
					break;
				}
		}
		// speaker group:
		// A naturalized German[12858-12861][12860][gendem][M]
		// boris ivanovitch [19627][name][M][OBJ][PREP_OBJ][MOVE_PREP][F:boris L:ivanovitch ]
		// the other[23011-23013]{OWNER: WO -2 other}[23012][mg][M][OGEN][SUBJ][ambiguous]
		// the German[german] seemed to pull himself[german] together . 
		// he[german,boris] indicated the place he[german] had been occupying at the head[head] of the table[table] . 
		// the Russian[boris] demurred , but the other insisted .
		// if the speakerGroup contains 2 or 3 (and the 3rd one is the generic other)
		if (!inQuote && latestOwnerWhere == -2 && m[where].word->first == L"other" && (sg->speakers.size() == 2 ||
			(sg->speakers.size() == 3 && sg->speakers.find(o) != sg->speakers.end()) ||
			(sg->speakers.size() - sg->observers.size() == 2)))
		{
			for (set <int>::iterator si = sg->speakers.begin(), siEnd = sg->speakers.end(); si != siEnd; si++)
				if (*si != latestObject && *si != o && find(sg->observers.begin(), sg->observers.end(), *si) == sg->observers.end())
				{
					objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther (groupedSpeakers - observer) with %d:%s - %s in %s", where, I, objectString(latestObject, tmpstr, true).c_str(), objectString(*si, tmpstr2, true).c_str(), toText(*sg, tmpstr3));
				}
			return true;
		}
		// if she[cook] missed the other[tuppence] (29868:match even though POV)
		if (objectMatches.empty() && sg->speakers.size() == 2 && m[where].relSubject >= 0 && m[m[where].relSubject].objectMatches.size() <= 1)
		{
			int whereSubject = m[where].relSubject, subject = (m[whereSubject].objectMatches.empty()) ? m[whereSubject].getObject() : m[whereSubject].objectMatches[0].object;
			if (sg->speakers.find(subject) != sg->speakers.end())
			{
				for (set <int>::iterator si = sg->speakers.begin(), siEnd = sg->speakers.end(); si != siEnd; si++)
					if (*si != subject)
						objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther (2 speakers subject/object L&L) based on sg %s", where, toText(*sg, tmpstr3));
			}
		}
		if (objectMatches.empty())
		{
			bool matchGender = (objects[o].male ^ objects[o].female);
			// When referring to 'the other woman', it is probably not referring to another woman other than
			// the POV, but rather choosing between two women other than the POV, unless there is only one other woman.
			if (sg->povSpeakers.find(latestObject) != sg->povSpeakers.end() ||
				(matchGender && !objects[o].matchGender(objects[latestObject]))) continue;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther based on sg %s", where, toText(*sg, tmpstr3));
			// The latest object agreeing with the gender of 'o' is at the current location (I) - latestObject
			// Now look for the object occurring closest to and before latestObject in the speakerGroup agreeing with the gender of o.
			int latestOtherObjectWhere = -1, latestOtherObject = -1;
			vector <cLocalFocus>::iterator lsi;
			for (set <int>::iterator si = sg->speakers.begin(), siEnd = sg->speakers.end(); si != siEnd; si++)
				if ((!matchGender || objects[o].matchGender(objects[*si])) && *si != latestObject && *si != o &&
					sg->povSpeakers.find(*si) == sg->povSpeakers.end() &&
					(lsi = in(*si)) != localObjects.end() && (latestOtherObjectWhere == -1 || latestOtherObjectWhere < lsi->lastWhere))
				{
					latestOtherObjectWhere = lsi->lastWhere;
					latestOtherObject = *si;
				}
			if (latestOtherObjectWhere != -1)
			{
				objectMatches.push_back(cOM(latestOtherObject, SALIENCE_THRESHOLD));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupGenericOther (speakers - any '%s' than %d:%s) resolved to %d:%s",
						where, objectString(o, tmpstr3, true).c_str(),
						I, objectString(latestObject, tmpstr, true).c_str(),
						latestOtherObjectWhere, objectString(objectMatches, tmpstr2, true).c_str());
			}
		}
	}
	return true;
}

// if no grouped speakers, is there a meta
// look for matching metagroup object in localObjects - does "his companion" match "the second man"?
// if this is a "second" was there a "first"?
bool cSource::resolveMetaGroupInLocalObjects(int where, int o, vector <cOM>& objectMatches, bool onlyFirst)
{
	LFS
		int latestOwnerWhere = objects[o].getOwnerWhere();
	if (latestOwnerWhere == -1 && (latestOwnerWhere = cObject::whichOrderWord(m[objects[o].originalLocation].word)) != -1)
		latestOwnerWhere = -2 - latestOwnerWhere;
	if (latestOwnerWhere != -4 && onlyFirst) return objectMatches.size() > 0; // -4 ="second"
	for (vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsEnd = localObjects.end(); lsi != lsEnd; lsi++)
		if (objects[lsi->om.object].objectClass == META_GROUP_OBJECT_CLASS && lsi->om.object != o && lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge) &&
			objects[o].matchGenderIncludingNeuter(objects[lsi->om.object]) && objects[o].plural == objects[lsi->om.object].plural &&
			locationBefore(lsi->om.object, where) >= speakerGroups[currentSpeakerGroup].sgBegin)
		{
			wstring tmpstr;
			if (objects[lsi->om.object].getOwnerWhere() == -5 && latestOwnerWhere == -4 && // was there a 'first' if this is 'second'?
				((m[objects[lsi->om.object].originalLocation].objectMatches.size() && objects[m[objects[lsi->om.object].originalLocation].objectMatches[0].object].plural) ||
					(m[objects[lsi->om.object].originalLocation].objectRole & UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE)))
			{
				// was there a first man mentioned anywhere?  If so, match the same object if the matched object is plural
				objectMatches = m[objects[lsi->om.object].originalLocation].objectMatches;
				// if the first object is made unresolvable (and thus a new object), then the second is also unresolvable
				if (m[objects[lsi->om.object].originalLocation].objectRole & UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE)
					m[where].objectRole |= UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
				return true;
			}
			if (!onlyFirst)
				objectMatches.push_back(lsi->om);
		}
	return objectMatches.size() > 0;
}

// a metagroup name may be matched by another metagroup name
bool cSource::resolveMetaGroupNonNameObject(int where, bool inQuote, vector <cOM>& objectMatches, int& latestOwnerWhere)
{
	LFS
		if (inQuote) return false;
	int o = m[where].getObject(), nonNameObject = -1;
	vector <cSpeakerGroup>::iterator csg = speakerGroups.begin() + currentSpeakerGroup;
	set <int>* speakers = (csg->groupedSpeakers.size()) ? &csg->groupedSpeakers : &csg->speakers;
	// find latest subject or speaker in singularSpeakers or speakers
	wstring tmpstr, tmpstr2, tmpstr3;
	// scan for non-named speakers in latest group reported
	for (set <int>::iterator s = speakers->begin(), sEnd = speakers->end(); s != sEnd; s++)
		if (objects[*s].objectClass != NAME_OBJECT_CLASS)
		{
			if (nonNameObject != -1)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:speakers %s have more than one non-name object", where, objectString(*speakers, tmpstr2).c_str());
				nonNameObject = -1;
				break;
			}
			nonNameObject = *s;
		}
	if (nonNameObject >= 0 && objects[nonNameObject].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && m[where].queryForm(friendForm) >= 0)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:The one non-name object %s is an occupation", where, objectString(nonNameObject, tmpstr2, true).c_str());
		return false;
	}
	// found itself in speakers.  locate other speaker in smallest group. 
	if (nonNameObject == m[where].getObject() && speakers->size() == 2)
	{
		for (set <int>::iterator s = speakers->begin(), sEnd = speakers->end(); s != sEnd; s++)
			if (*s != nonNameObject)
			{
				nonNameObject = *s;
				break;
			}
		// now nonNameObject = friend of object.  Look for the last group containing this friend.
		latestOwnerWhere = objects[nonNameObject].originalLocation;
	}
	if (nonNameObject != m[where].getObject() && nonNameObject != -1)
	{
		objectMatches.push_back(cOM(nonNameObject, SALIENCE_THRESHOLD));
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:Resolving meta group object [%s] - nonNameObject from speakerGroup %s are %s.", where, objectString(o, tmpstr, true).c_str(),
				objectString(*speakers, tmpstr2).c_str(), objectString(nonNameObject, tmpstr3, true).c_str());
		replaceObjectInSection(where, nonNameObject, m[where].getObject(), L"resolveOtherMetaGroupObject");
		return true;
	}
	return false;
}

// the two men OR the other two OR the two
bool cSource::resolveMetaGroupTwo(int where, bool inQuote, vector <cOM>& objectMatches)
{
	LFS
		int o = m[where].getObject();
	if (o < 0) return false;
	int latestOwnerWhere = objects[o].getOwnerWhere();
	if (latestOwnerWhere != -11 &&
		((latestOwnerWhere != -2 && latestOwnerWhere != -1) || (m[where].word->first != L"two" && m[where].word->first != L"couple"))) // "two"
		return false;
	if (m[where].endObjectPosition + 1 < (signed)m.size() && m[m[where].endObjectPosition + 1].word->first == L"of")
		return false;
	// you are a curious couple.
	if (inQuote && (m[where].objectRole & IS_OBJECT_ROLE) && m[where].relSubject >= 0 && (m[m[where].relSubject].word->second.inflectionFlags & SECOND_PERSON) &&
		currentSpeakerGroup < speakerGroups.size() && speakerGroups[currentSpeakerGroup].groupedSpeakers.size() == 2)
	{
		for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].groupedSpeakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].groupedSpeakers.end(); si != siEnd; si++)
			objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
		return true;
	}
	// search the latest two speakerGroups
	// search for groupedSpeakers where groupedSpeaker size==2 and matches sex
	// or groups where groups=2 AND matches sex
	// if the current group has a POV, choose only those groups with the same POV, but not including the POV
	// this is to search groups from the same point of view.  If a group of two men were there while the current POV wasn't, the POV
	// speaker wouldn't know about the group.
	set <int> povSpeakers;
	tIWMM fromMatch, toMatch, toMapMatch;
	wstring logMatch;
	// override pov (or create pov) if in quote, because if in quote, then pov is automatically from the speakers viewpoint
	if (inQuote)
	{
		if (previousSpeakers.empty())
		{
			/*
			if (whereSubjectsInPreviousUnquotedSection>=0)
			{
				if (m[whereSubjectsInPreviousUnquotedSection].objectMatches.empty())
					povSpeakers.insert(m[whereSubjectsInPreviousUnquotedSection].getObject());
				else
					for (unsigned int omi=0; omi<m[whereSubjectsInPreviousUnquotedSection].objectMatches.size(); omi++)
						povSpeakers.insert(m[whereSubjectsInPreviousUnquotedSection].objectMatches[omi].object);
			}
			*/
		}
		else
			// get speaker
			for (unsigned int omi = 0; omi < previousSpeakers.size(); omi++)
				povSpeakers.insert(previousSpeakers[omi]);
	}
	else getPOVSpeakers(povSpeakers);
	// don't use POV on a transition, like:
	// the two young men[tommy,james] were seated in a first - class carriage en route for Chester . 
	// because a transition is automatically the POV (temporary) of the narrator
	bool transition = false;
	if (m[where].hasSyntacticRelationGroup)
	{
		vector <cSyntacticRelationGroup>::iterator sr = findSyntacticRelationGroup(where);
		if (sr != syntacticRelationGroups.end())
		{
			transition = (sr->genderedLocationRelation && sr->physicalRelation) || sr->tft.timeTransition;
		}
	}
	bool hasPOV = (povSpeakers.size() > 0) && (!(m[where].objectRole & SUBJECT_ROLE) || !transition), allIn, oneIn;
	bool useGender = (objects[o].male ^ objects[o].female), onlyNeuter = objects[o].neuter && !objects[o].male && !objects[o].female;
	bool fromGroupedSpeakers = false, fromGroups = false, fromOnePluralObject = false, fromCurrent = false;// , inQuoteSpeaker = false;
	int searchUntil = (signed)currentSpeakerGroup - 2, lastSGFound = -1;
	if (hasPOV) searchUntil = 0;
	if (m[where].word->first == L"two") searchUntil = currentSpeakerGroup; // make it immediate don't let the "two" match two elderly ladies, etc.
	// are there already two objects matching the description in the current speaker group other than the speaker?
	if (useGender && hasPOV && !inQuote)
	{
		set <int> speakers, tPovSpeakers;
		if (speakerGroupsEstablished)
			speakers = speakerGroups[currentSpeakerGroup].speakers;
		else
			getCurrentSpeakers(speakers, tPovSpeakers);
		/*
	if (where==68320)
	{
		wstring tmpstr,tmpstr2,tmpstr3;
		lplog(LOG_RESOLUTION,L"%s %s",toText(tempSpeakerGroup,tmpstr),objectString(nextNarrationSubjects,tmpstr2).c_str());
		lplog(LOG_RESOLUTION,L"%s",toText(speakerGroups[currentSpeakerGroup],tmpstr3));
		for (unsigned int I=0; I<povInSpeakerGroups.size(); I++)
			if (povInSpeakerGroups[I]>speakerGroups[currentSpeakerGroup].begin)
				lplog(LOG_RESOLUTION,L"POV %d:%s [%s]",povInSpeakerGroups[I],objectString(m[povInSpeakerGroups[I]].getObject(),tmpstr2,true).c_str(),objectString(m[povInSpeakerGroups[I]].objectMatches,tmpstr3,true).c_str());
		lplog(LOG_RESOLUTION,L"povSpeakers %s",objectString(povSpeakers,tmpstr2).c_str());
		for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
			if (*si!=o && povSpeakers.find(*si)==povSpeakers.end())
				lplog(LOG_RESOLUTION,L"%06d:speaker %s",locationBefore(*si,where),objectString(*si,tmpstr2,true).c_str());
	}
	*/
		if (speakers.size() >= 3)
		{
			for (set <int>::iterator sgi = speakers.begin(), sgiEnd = speakers.end(); sgi != sgiEnd; sgi++)
			{
				if (povSpeakers.find(*sgi) == povSpeakers.end() && objects[o].exactGenderMatch(objects[*sgi]) &&
					!nymNoMatch(where, objects.begin() + o, objects.begin() + *sgi, false, false, logMatch, fromMatch, toMatch, toMapMatch, L"NoMatch MG(6) 1"))
					objectMatches.push_back(cOM(*sgi, SALIENCE_THRESHOLD));
			}
			if (objectMatches.size() >= 2)
			{
				fromCurrent = true;
				lastSGFound = currentSpeakerGroup;
			}
			else
				objectMatches.clear();
		}
	}
	for (int I = currentSpeakerGroup; I >= 0 && I >= searchUntil && objectMatches.empty(); I--)
	{
		vector <cSpeakerGroup>::iterator sg = speakerGroups.begin() + I;
		if (hasPOV && !intersect(sg->speakers, povSpeakers, allIn, oneIn)) continue;
		vector <int> otherObjects;
		bool inQuoteSpeaker = false;
		if (sg->groupedSpeakers.size() == 2 && (!inQuote || previousSpeakers.size()))
		{
			bool allMatchGender = true;
			for (set <int>::iterator sgi = sg->groupedSpeakers.begin(), sgiEnd = sg->groupedSpeakers.end(); sgi != sgiEnd && allMatchGender && !inQuoteSpeaker; sgi++)
			{
				allMatchGender = (!useGender || objects[o].matchGender(objects[*sgi])) && (!onlyNeuter || objects[*sgi].neuter) &&
					!nymNoMatch(where, objects.begin() + o, objects.begin() + *sgi, false, false, logMatch, fromMatch, toMatch, toMapMatch, L"NoMatch MG(2) 1");
				inQuoteSpeaker |= inQuote && find(previousSpeakers.begin(), previousSpeakers.end(), *sgi) != previousSpeakers.end();
			}
			if (fromGroupedSpeakers = allMatchGender && !inQuoteSpeaker && (!hasPOV || !intersect(sg->groupedSpeakers, povSpeakers, allIn, oneIn)))
				for (set <int>::iterator sgi = sg->groupedSpeakers.begin(), sgiEnd = sg->groupedSpeakers.end(); sgi != sgiEnd; sgi++)
					if (*sgi != o)
					{
						objectMatches.push_back(cOM(*sgi, SALIENCE_THRESHOLD));
						lastSGFound = I;
					}
		}
		if (objectMatches.empty())
		{
			for (vector < cSpeakerGroup::cGroup >::iterator sgg = sg->groups.begin(), sggEnd = sg->groups.end(); sgg != sggEnd; sgg++)
			{
				inQuoteSpeaker = false;
				if (sgg->objects.size() == 2)
				{
					bool allMatchGender = true;
					for (vector <int>::iterator sggi = sgg->objects.begin(), sggiEnd = sgg->objects.end(); sggi != sggiEnd && allMatchGender && !inQuoteSpeaker; sggi++)
					{
						allMatchGender = (!useGender || objects[o].matchGender(objects[*sggi])) && (!onlyNeuter || objects[*sggi].neuter) &&
							!nymNoMatch(where, objects.begin() + o, objects.begin() + *sggi, false, false, logMatch, fromMatch, toMatch, toMapMatch, L"NoMatch MG(2) 2");
						if (previousSpeakers.size())
							inQuoteSpeaker |= inQuote && find(previousSpeakers.begin(), previousSpeakers.end(), *sggi) != previousSpeakers.end();
						else
							inQuoteSpeaker |= inQuote && sg->speakers.find(*sggi) != sg->speakers.end();
					}
					if (fromGroups = allMatchGender && !inQuoteSpeaker && (!hasPOV || !intersect(sgg->objects, povSpeakers, allIn, oneIn)) &&
						(!inQuote || !speakerGroupsEstablished || speakerGroups[currentSpeakerGroup].speakers.size() > 2 || !intersect(sgg->objects, speakerGroups[currentSpeakerGroup].speakers, allIn, oneIn)))
						for (vector <int>::iterator sggi = sgg->objects.begin(), sggiEnd = sgg->objects.end(); sggi != sggiEnd; sggi++)
							if (*sggi != o && objects[*sggi].originalLocation < where)
							{
								objectMatches.push_back(cOM(*sggi, SALIENCE_THRESHOLD));
								lastSGFound = I;
							}
					if (objectMatches.size() >= 2)
						break;
				}
				else
					if (fromOnePluralObject = sgg->objects.size() == 1 && sgg->objects[0] != o && objects[o].matchGender(objects[sgg->objects[0]]) &&
						(!onlyNeuter || objects[sgg->objects[0]].neuter) &&
						!nymNoMatch(where, objects.begin() + o, objects.begin() + sgg->objects[0], false, false, logMatch, fromMatch, toMatch, toMapMatch, L"NoMatch MG(2) PL") &&
						(objects[sgg->objects[0]].getOwnerWhere() == -11 ||
							((objects[sgg->objects[0]].getOwnerWhere() == -2 || objects[sgg->objects[0]].getOwnerWhere() == -1) &&
								m[objects[sgg->objects[0]].originalLocation].word->first == L"two")) &&
						objects[sgg->objects[0]].originalLocation < where) // "two"
					{
						objectMatches.push_back(cOM(sgg->objects[0], SALIENCE_THRESHOLD));
						lastSGFound = I;
					}
			}
		}
	}
	wstring tmpstr;
	if (debugTrace.traceSpeakerResolution && objectMatches.size())
	{
		const wchar_t* fromWhere = L"?";
		if (fromGroupedSpeakers) fromWhere = L"fromGroupedSpeakers";
		else if (fromGroups) fromWhere = L"fromGroups";
		else if (fromOnePluralObject) fromWhere = L"fromOnePluralObject";
		else if (fromCurrent) fromWhere = L"fromCurrent";
		wstring tmpstr2, tmpstr3;
		lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupTwo [%s] of %s is %s from speakerGroup %s", where, fromWhere, objectString(o, tmpstr3, true).c_str(), objectString(objectMatches, tmpstr, true).c_str(),
			(lastSGFound >= 0) ? toText(speakerGroups[lastSGFound], tmpstr2) : L"None");
		if (currentSpeakerGroup < speakerGroups.size())
			lplog(LOG_RESOLUTION, L"%06d:resolveMetaGroupTwo [%s] povSpeakers %s %s%s", where, fromWhere,
				objectString(povSpeakers, tmpstr).c_str(), (lastSGFound == currentSpeakerGroup) ? L"" : L"currentSpeakerGroup ", (lastSGFound == currentSpeakerGroup) ? L"" : toText(speakerGroups[currentSpeakerGroup], tmpstr2));
	}
	return true;
}

// the one man OR the big one
// NOT this one man
// in secondary quotes, inPrimaryQuote=false
bool cSource::resolveMetaGroupOne(int where, bool inPrimaryQuote, vector <cOM>& objectMatches, bool& chooseBest)
{
	LFS
		int o = m[where].getObject(), latestOwnerWhere = objects[o].getOwnerWhere();
	// rule out some one and twenty one
	if (m[m[where].beginObjectPosition].queryWinnerForm(numeralCardinalForm) >= 0 || m[m[where].beginObjectPosition].word->first == L"some")
		return false;
	tIWMM fromMatch, toMatch, toMapMatch;
	wstring logMatch, word = m[where].word->first;
	bool useGender = (objects[o].male ^ objects[o].female), onlyNeuter = objects[o].neuter && !objects[o].male && !objects[o].female, notNeuter = !objects[o].neuter;
	// look through very recent items looking for groups 
	vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsiEnd = localObjects.end();
	for (; lsi != lsiEnd; lsi++)
		if (lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge) && lsi->getTotalAge() < 3 && lsi->om.object != o &&
			(!useGender || objects[o].matchGender(objects[lsi->om.object])) &&
			(!onlyNeuter || objects[lsi->om.object].neuter) &&
			(!notNeuter || !objects[lsi->om.object].neuter) &&
			!nymNoMatch(where, objects.begin() + o, objects.begin() + lsi->om.object, true, false, logMatch, fromMatch, toMatch, toMapMatch, L"NoMatch MG(1) PL") &&
			(latestOwnerWhere < 0 || (m[latestOwnerWhere].getObject() != lsi->om.object && in(lsi->om.object, m[latestOwnerWhere].objectMatches) == m[latestOwnerWhere].objectMatches.end())) &&
			// exclude audience if in an ES1 story
			(!inPrimaryQuote || (m[lastOpeningPrimaryQuote].flags & (cWordMatch::flagFirstEmbeddedStory | cWordMatch::flagSecondEmbeddedStory)) != cWordMatch::flagFirstEmbeddedStory ||
				find(beforePreviousSpeakers.begin(), beforePreviousSpeakers.end(), lsi->om.object) == beforePreviousSpeakers.end()) &&
			// one does not refer to the speaker if in quotes
			(!inPrimaryQuote || find(previousSpeakers.begin(), previousSpeakers.end(), lsi->om.object) == previousSpeakers.end())
			)
		{
			lsi->om.salienceFactor = 500;
			if (lsi->getTotalAge() < 2) lsi->om.salienceFactor += 500;
			bool explicitOccupationMatch = false;
			lsi->numMatchedAdjectives = nymMatch(objects.begin() + o, objects.begin() + lsi->om.object, false, false, explicitOccupationMatch, logMatch, fromMatch, toMatch, toMapMatch, L"Match MG(1) PL");
			// if a singular object and last where was either matching a plural object or part of an MPLURAL_NOUN, boost.
			if (!objects[lsi->om.object].plural && lsi->lastWhere >= 0 &&
				((m[lsi->lastWhere].word->second.inflectionFlags & PLURAL) || (m[lsi->lastWhere].objectRole & MPLURAL_ROLE)))
			{
				lsi->om.salienceFactor += 1000;
				lsi->res += L"+MGO_SING[+1000]";
			}
			// prefer plural nouns that match head, if o has a head.
			if (objectMatches.size() && word != L"one" && m[objects[lsi->om.object].originalLocation].word->first == word)
			{
				lsi->om.salienceFactor += 1000;
				lsi->res += L"+MGO_MATCHING_HEAD[+1000]";
			}
		}
		else
			lsi->clear();
	wstring tmpstr;
	if (debugTrace.traceSpeakerResolution)
		printLocalFocusedObjects(where, META_GROUP_OBJECT_CLASS);
	chooseBest = true;
	return true;
}

bool cSource::resolveMetaGroupJoiner(int where, vector <cOM>& objectMatches)
{
	LFS
		int latestOwnerWhere = objects[m[where].getObject()].getOwnerWhere();
	if (latestOwnerWhere == -10 && currentSpeakerGroup < speakerGroups.size()) // this
	{
		vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsiEnd = localObjects.end();
		for (; lsi != lsiEnd; lsi++)
			if (lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge) && objects[lsi->om.object].objectClass == META_GROUP_OBJECT_CLASS &&
				isGroupJoiner(m[objects[lsi->om.object].originalLocation].word) && locationBefore(lsi->om.object, where) > speakerGroups[currentSpeakerGroup].sgBegin)
				objectMatches.push_back(lsi->om);
		if (objectMatches.size() > 0) return true;
		for (int I = where - 1; I > speakerGroups[currentSpeakerGroup].sgBegin; I--)
			if (m[I].getObject() >= 0 && objects[m[I].getObject()].objectClass == META_GROUP_OBJECT_CLASS && isGroupJoiner(m[I].word))
			{
				if (m[I].objectMatches.size())
					objectMatches = m[I].objectMatches;
				else
					objectMatches.push_back(cOM(m[I].getObject(), SALIENCE_THRESHOLD));
				if (objectMatches.size() == 1)
					replaceObjectInSection(where, objectMatches[0].object, m[where].getObject(), L"groupJoiner");
				return true;
			}
	}
	return false;
}

void cSource::getPOVSpeakers(set <int>& povSpeakers)
{
	LFS
		if (speakerGroupsEstablished)
			povSpeakers = speakerGroups[currentSpeakerGroup].povSpeakers;
		else if (povInSpeakerGroups.size() > 0)
		{
			int numPhysicallyPresent = 0;
			vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsEnd = localObjects.end(), ppLsi;
			for (; lsi != lsEnd; lsi++)
				if (lsi->physicallyPresent && objects[lsi->om.object].PISDefinite > 0)
				{
					numPhysicallyPresent++;
					ppLsi = lsi;
				}
			if (numPhysicallyPresent == 1)
			{
				povSpeakers.insert(ppLsi->om.object);
				return;
			}
			int povWhere = -1, which = povInSpeakerGroups.size() - 1, lastSectionHeader = (section < sections.size()) ? sections[section].begin : -1;
			numPhysicallyPresent = 0;
			while (which >= 0 && (m[povInSpeakerGroups[which]].objectRole & (IN_PRIMARY_QUOTE_ROLE | IN_SECONDARY_QUOTE_ROLE))) which--;
			if (which >= 0)
			{
				povWhere = povInSpeakerGroups[which];
				for (int I = 0; I < (signed)m[povWhere].objectMatches.size(); I++)
					if ((lsi = in(m[povWhere].objectMatches[I].object)) != localObjects.end() && lsi->physicallyPresent)
						numPhysicallyPresent++;
				if (numPhysicallyPresent > 1)
				{
					which--;
					if (which >= 0)
						povWhere = povInSpeakerGroups[which];
				}
			}
			if (povWhere < lastSectionHeader)
				return;
			if (povWhere >= 0 && (m[povWhere].getObject() >= 0 || numPhysicallyPresent == 1))
			{
				if ((lsi = in(m[povWhere].getObject())) != localObjects.end())
					povSpeakers.insert(lsi->om.object);
				for (vector <cOM>::iterator povi = m[povWhere].objectMatches.begin(), poviEnd = m[povWhere].objectMatches.end(); povi != poviEnd; povi++)
					if ((lsi = in(povi->object)) != localObjects.end())
						povSpeakers.insert(lsi->om.object);
			}
			if (povSpeakers.size() > 1)
				povSpeakers.clear();
		}
}

void cSource::getPOVSpeakers2(const int where, vector <cSyntacticRelationGroup>::iterator srg, set <int>& povSpeakers, bool& nonPOVSpeakerOverride)
{
	vector <cLocalFocus>::iterator lsi;
	bool allIn, oneIn, physicallyEvaluated;
	if (povSpeakers.empty() && !speakerGroupsEstablished)
	{
		// if whereSubject is present between where and backInitialPosition, they are povSpeakers.
		if (m[srg->whereSubject].objectMatches.empty() && (lsi = in(m[srg->whereSubject].getObject())) != localObjects.end() && lsi->lastWhere > where &&
			physicallyPresentPosition(lsi->lastWhere, physicallyEvaluated) && physicallyEvaluated &&
			// a page-boy went in search of him.
			!(srg->relationType == stEXIT && objects[lsi->om.object].originalLocation == srg->whereSubject)) // if stEXIT, and whereSubject is the first mention of the subject, skip.
			povSpeakers.insert(m[srg->whereSubject].getObject());
		else if (m[srg->whereSubject].objectMatches.size())
		{
			for (int omi = 0; omi < (signed)m[srg->whereSubject].objectMatches.size(); omi++)
			{
				if ((lsi = in(m[srg->whereSubject].objectMatches[omi].object)) != localObjects.end() && lsi->lastWhere > where &&
					physicallyPresentPosition(lsi->lastWhere, physicallyEvaluated) && physicallyEvaluated)
					povSpeakers.insert(m[srg->whereSubject].objectMatches[omi].object);
			}
			if (m[srg->whereSubject].objectMatches.size() != povSpeakers.size())
				povSpeakers.clear();
		}
	}
	wstring tmpstr;
	int csg = currentSpeakerGroup;
	if (csg > 0 && ((unsigned)csg) < speakerGroups.size() && speakerGroups[csg].sgBegin > where) csg--;
	// if no POV, and several PP speakers, and only one speaker exists in next speaker group, pick that one as a POV
	if (povSpeakers.empty() && speakerGroupsEstablished && ((unsigned)csg + 1) < speakerGroups.size() &&
		(m[srg->whereSubject].objectMatches.empty() || m[srg->whereSubject].objectMatches.size() == 1)) // definite match 
	{
		if (intersect(speakerGroups[csg].speakers, speakerGroups[csg + 1].speakers, allIn, oneIn) && !allIn) // at least one doesn't match
		{
			// if this speaker exists in the next speaker group, it must be the POV.
			if (intersect(srg->whereSubject, speakerGroups[csg + 1].speakers, allIn, oneIn) && allIn)
			{
				// make the POV the one that survives to the next speaker group (the one who exits and keeps the point-of-view
				povSpeakers.insert((m[srg->whereSubject].objectMatches.empty()) ? m[srg->whereSubject].getObject() : m[srg->whereSubject].objectMatches[0].object);
				lplog(LOG_RESOLUTION, L"%06d:Made lingering speaker %s POV.", where, whereString(srg->whereSubject, tmpstr, true).c_str());
			}
			// this speaker doesn't exist in the next speaker group. if the other speaker does, then the other speaker must be the POV.
			else
			{
				set <int> s;
				set_intersection(speakerGroups[csg].speakers.begin(), speakerGroups[csg].speakers.end(),
					speakerGroups[csg + 1].speakers.begin(), speakerGroups[csg + 1].speakers.end(),
					std::inserter(s, s.begin()));
				if (s.size() == 1)
				{
					povSpeakers.insert(*s.begin());
					lplog(LOG_RESOLUTION, L"%06d:Made lingering speaker %s POV (2).", where, objectString(*s.begin(), tmpstr, true).c_str());
				}
			}
		}
		else // no speakers match.  set nonPOVSpeakerOverride.
			nonPOVSpeakerOverride = true;
	}
}

void cSource::getCurrentSpeakers(set <int>& speakers, set <int>& povSpeakers)
{
	LFS
		int csg = currentSpeakerGroup;
	if (((unsigned)csg) >= speakerGroups.size()) csg--;
	if (csg >= 0)
		speakers = speakerGroups[csg].speakers;
	speakers.insert(tempSpeakerGroup.speakers.begin(), tempSpeakerGroup.speakers.end());
	for (unsigned int s = 0; s < nextNarrationSubjects.size(); s++)
		speakers.insert(nextNarrationSubjects[s]);
	wstring tmpstr, tmpstr2, tmpstr3;
	bool genderSpecific = false;
	int maleSpeakers = 0, femaleSpeakers = 0;
	for (set <int>::iterator si = speakers.begin(), siEnd = speakers.end(); si != siEnd; si++)
	{
		if (objects[*si].male) maleSpeakers++;
		if (objects[*si].female) femaleSpeakers++;
	}
	genderSpecific = (maleSpeakers == 1 && femaleSpeakers == 1 && speakers.size() == 2);
	//int largestPOVAfterSection=-1;
	if (csg >= 0)
		for (int I = povInSpeakerGroups.size() - 1; I >= 0 && povInSpeakerGroups[I] >= speakerGroups[csg].sgBegin; I--)
		{
			if (genderSpecific && m[povInSpeakerGroups[I]].objectMatches.size())
			{
				for (int J = 0; J < (signed)m[povInSpeakerGroups[I]].objectMatches.size(); J++)
					povSpeakers.insert(m[povInSpeakerGroups[I]].objectMatches[J].object);
			}
			else if (m[povInSpeakerGroups[I]].getObject() >= 0)
				povSpeakers.insert(m[povInSpeakerGroups[I]].getObject());
		}
	if (povSpeakers.empty())
		for (int I = povInSpeakerGroups.size() - 1; I >= 0 && (sections.empty() || povInSpeakerGroups[I] >= (signed)sections[section].begin); I--)
		{
			if (genderSpecific && m[povInSpeakerGroups[I]].objectMatches.size())
			{
				for (int J = 0; J < (signed)m[povInSpeakerGroups[I]].objectMatches.size(); J++)
					povSpeakers.insert(m[povInSpeakerGroups[I]].objectMatches[J].object);
			}
			else if (m[povInSpeakerGroups[I]].getObject() >= 0)
				povSpeakers.insert(m[povInSpeakerGroups[I]].getObject());
		}
}

// OK, final chance for a generic 'the other'
// speaker group:
// A naturalized German[12858-12861][12860][gendem][M]
// boris ivanovitch [19627][name][M][OBJ][PREP_OBJ][MOVE_PREP][F:boris L:ivanovitch ]
// the other[23011-23013]{OWNER: WO -2 other}[23012][mg][M][OGEN][SUBJ][ambiguous]
// the German[german] seemed to pull himself[german] together . 
// he[german,boris] indicated the place he[german] had been occupying at the head[head] of the table[table] . 
// the Russian[boris] demurred , but the other insisted .
// if the speakerGroup contains 2 or 3 (and the 3rd one is the generic other)
bool cSource::resolveMetaGroupOther(int where, vector <cOM>& objectMatches)
{
	LFS
		set <int> speakers, povSpeakers;
	if (speakerGroupsEstablished && currentSpeakerGroup < speakerGroups.size())
	{
		speakers = speakerGroups[currentSpeakerGroup].speakers;
		povSpeakers = speakerGroups[currentSpeakerGroup].povSpeakers;
	}
	else
	{
		getCurrentSpeakers(speakers, povSpeakers);
	}
	// get last mentioned that is not an observer nor the object currently being matched
	int latestWhere = -1, latestObject = -1, nextLatestWhere = -1, nextLatestObject = -1, tmpWhere;
	int o = m[where].getObject();
	for (set <int>::iterator si = speakers.begin(), siEnd = speakers.end(); si != siEnd; si++)
		if (*si != o && povSpeakers.find(*si) == povSpeakers.end() && (tmpWhere = locationBefore(*si, where)) < where)
		{
			if (tmpWhere > latestWhere)
			{
				nextLatestWhere = latestWhere;
				nextLatestObject = latestObject;
				latestWhere = tmpWhere;
				latestObject = *si;
			}
			else if (tmpWhere > nextLatestWhere)
			{
				nextLatestWhere = tmpWhere;
				nextLatestObject = *si;
			}
		}
	if (nextLatestObject >= 0)
		objectMatches.push_back(cOM(nextLatestObject, SALIENCE_THRESHOLD));
	return false;
}

// get position of owner object before and nearest the principalWhere of the object.  If not found, return false.
// find latest speakerGroup containing any of the speakers of latestOwnerWhere.
// if that speakerGroup also contains the rest of the speakers of latestOwnerWhere,
//   check if previousSubsetSpeakerGroup exists and also contains all the speakers and at least one more.
//     If so use this group instead.
//   fill objectMatches with this speaker group except for all speakers of latestOwnerWhere.
// else return false.
// Resolving Other-Anaphora - Natalia N. Modjeska
// Apposition: (a) NP preceding an appositive, if appositive contains “(an)other”; (b) appositive NP following an other-anaphor:
// (47) a. Mary Elizabeth Ariail, another social-studies teacher
//      b. The other social studies teacher, Mary Ariail . . .
//         (both not “other than Mary Elizabeth Ariail”)
// Copular clauses: (a) subject NP of a copular clause, if the anaphor is predicate; (b) predicate NP if the other-anaphor is the subject:
// (48) a. The reputed wealth of the Unification Church is another matter of contention.
//      b. The other matter of contention is the reputed wealth of the Unification Church.
//         (both not “other than the wealth”)
// Possessives S/OF: (a) the possessor NP, if other-anaphor realizes the possessed entity ; (b) possessive PP complement of an other-anaphor:
// (49) a. Koito’s other shareholders
//      b. other shareholders of Koito
//         (both not “other than Koito”)
// Constructions with spatio-temporal “there”, in which the anaphor is the head of the sentence:
// (50) a. In London, there are other locations where we could meet. (not “other than London”)
//      b. On Tuesday, there are other times when we could meet. (not “other than Tuesday”)
// The below routine is for all metagroup objects where latestOwnerWhere<0.
// This would be where the primary word is either a wordOrder object (other,another etc).
//     or a primary word having a friendForm, and a wordOrder word as a direct modifier.
// meta group object types:
// example:        latestOwnerWhere
// the other       -1                 wordOrder word is in the principalWhere position, no resolvable owner
// his last        positive           wordOrder word at principalWhere, resolvable owner
// the companion   -1                 metagroup word at principalWhere, no resolvable owner
// his companion   positive           metagroup word at principalWhere, with a resolvable owner
// another ally    -3                 metagroup word at principalWhere, with a wordOrder word (another) as modifier
// the other man   -2                 gendered word at principalWhere, wordOrder owner as modifier

// gendered object type:
//   the man      -1                 gendered word at principalWhere, no owner
//   his girl     positive           gendered word at principalWhere, resolvable owner
// in secondary quotes, inPrimaryQuote=false
bool cSource::resolveMetaGroupSpecificObject(int where, bool inPrimaryQuote, bool inSecondaryQuote, bool definitelyResolveSpeaker, int lastBeginS1, int lastRelativePhrase, vector <cOM>& objectMatches, bool& chooseFromLocalFocus)
{
	LFS
		int o = m[where].getObject();
	int latestOwnerWhere = objects[o].getOwnerWhere();
	if (currentSpeakerGroup >= speakerGroups.size())
	{
		if (latestOwnerWhere >= 0)
		{
			bool allIn = false, oneIn = false;
			unsigned int minimumSize = min(1, m[latestOwnerWhere].objectMatches.size()) + 1;  // group must have at least one more member than the owner
			for (vector < cSpeakerGroup::cGroup >::iterator gi = tempSpeakerGroup.groups.begin(), giEnd = tempSpeakerGroup.groups.end(); gi != giEnd; gi++)
				if (intersect(latestOwnerWhere, gi->objects, allIn, oneIn) && allIn && minimumSize <= gi->objects.size())
				{
					for (vector <int>::iterator si = gi->objects.begin(), siEnd = gi->objects.end(); si != siEnd; si++)
						if (*si != m[latestOwnerWhere].getObject() && in(*si, m[latestOwnerWhere].objectMatches) == m[latestOwnerWhere].objectMatches.end())
							objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
					if (objectMatches.size()) break;
				}
			if (objectMatches.size()) return true;
		}
		if (!currentSpeakerGroup) return false;
		currentSpeakerGroup--;
		bool retValue = resolveMetaGroupSpecificObject(where, inPrimaryQuote, inSecondaryQuote, definitelyResolveSpeaker, lastBeginS1, lastRelativePhrase, objectMatches, chooseFromLocalFocus);
		currentSpeakerGroup++;
		return retValue;
	}
	if (!objects[o].male && !objects[o].female) return false;
	// generic 'other' like 'other man' or 'the other' - see above notes
	int end, wo;
	if (latestOwnerWhere < 0 && (end = m[where].endObjectPosition) >= 0 && end + 1 < (signed)m.size() &&
		m[end].word->first == L"of" && (wo = m[end + 1].getObject()) != -1 && (wo < 0 || objects[wo].isAgent(true)))
		latestOwnerWhere = end + 1;
	if (latestOwnerWhere < 0)
	{
		if (!(objects[o].male ^ objects[o].female))
		{
			objects[o].neuter = false;
			objects[o].male = objects[o].female = true;
		}
		// the visitor
		if (isGroupJoiner(m[where].word))
			return resolveMetaGroupJoiner(where, objectMatches);
		// "another"
		if (latestOwnerWhere == -3)
			return resolveMetaGroupWordOrderedFutureObject(where, objectMatches);
		if (objects[o].plural)
		{
			if (resolveMetaGroupPlural(latestOwnerWhere, inPrimaryQuote, objectMatches))
				return true;
		}
		if (!(m[where].objectRole & RE_OBJECT_ROLE) && // Apposition
			(m[where].objectRole & (SUBJECT_ROLE | IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) != IS_OBJECT_ROLE) // Copular
		{
			if (where + 2 < (signed)m.size() && m[where + 1].word->first == L"," && m[where + 2].principalWherePosition >= 0 && m[m[where + 2].principalWherePosition].objectRole & RE_OBJECT_ROLE)
				objectMatches.push_back(cOM(m[m[where + 2].principalWherePosition].getObject(), SALIENCE_THRESHOLD));
			// wchar_t *wordOrderWords[]={L"other",L"another",L"second",L"first",L"third",L"former",L"latter"
			//                            -2       -3         -4        -5       -6       -7        -8       
			// don't handle other cases yet
			else if (latestOwnerWhere >= -8)
			{
				int selfOrdered = -1;
				// "the latter" rather than "the latter man"
				if (latestOwnerWhere == -1 && (selfOrdered = cObject::whichOrderWord(m[objects[o].originalLocation].word)) != -1)
					selfOrdered = -2 - selfOrdered;
				if (selfOrdered == -4 || selfOrdered == -5 || selfOrdered == -6 || selfOrdered == -7 || selfOrdered == -8)
					// try matching against multiple objects in localObjects not related to gender (because that will be tried in resolveGenderedObject)
					resolveMetaGroupFirstSecondThirdWordOrderedObject(where, lastBeginS1, objectMatches, selfOrdered);
				// one
				if (selfOrdered == -13)
				{
					if (resolveMetaGroupOne(where, inPrimaryQuote, objectMatches, chooseFromLocalFocus)) return true;
				}
				else
				{
					if (latestOwnerWhere == -5 || latestOwnerWhere == -6)
						return false;  // try again with gender match as well - don't try matching with grouped speakers.
					vector <cSpeakerGroup>::iterator csg = speakerGroups.begin() + currentSpeakerGroup;
					if (csg->groupedSpeakers.empty() && csg->speakers.size() > 2 &&
						(csg->speakers.size() > 3 || (csg->speakers.find(o) == csg->speakers.end() && csg->observers.empty())))
					{
						resolveMetaGroupInLocalObjects(where, o, objectMatches, false);
						if (objectMatches.size() != 0 && latestOwnerWhere != -4) // if 'second', try again with gendered match (match second man against 'two men').
							return true;
					}
					//else
					//{
					if (!inSecondaryQuote && resolveMetaGroupTwo(where, inPrimaryQuote, objectMatches))
						return objectMatches.size() > 0;
					if (resolveMetaGroupInLocalObjects(where, o, objectMatches, true))
						return true;
					if (resolveMetaGroupNonNameObject(where, inPrimaryQuote, objectMatches, latestOwnerWhere))
						return true;
					// the other crook
					if (resolveMetaGroupSpecifiedOther(where, latestOwnerWhere, inPrimaryQuote, objectMatches))
						return true;
					// Scan backwards and look for any speaker.  Don't do this for specific friend forms (associate, master, etc)
					if (m[where].queryForm(friendForm) < 0 && resolveMetaGroupGenericOther(where, latestOwnerWhere, inPrimaryQuote, objectMatches))
						return true;
					//}
				}
			}
		}
		// the two men OR the other two OR the two
		if (!inPrimaryQuote && !inSecondaryQuote && resolveMetaGroupTwo(where, false, objectMatches))
			return objectMatches.size() > 0;
		if (latestOwnerWhere == -2 && m[where].word->first == L"other")
			return resolveMetaGroupOther(where, objectMatches);
		return false;
	}
	if (m[where].word->first == L"one")
		return resolveMetaGroupOne(where, inPrimaryQuote, objectMatches, chooseFromLocalFocus);
	if (latestOwnerWhere >= 0) // latestOwnerWhere may have been redefined by attaching 'of'
		return resolveMetaGroupByAssociation(where, inPrimaryQuote, objectMatches, latestOwnerWhere);
	return false;
}

void cSource::setMatched(int where, vector <int>& objs)
{
	LFS
		for (unsigned int I = 0; I < objs.size(); I++)
			m[where].objectMatches.push_back(cOM(objs[I], SALIENCE_THRESHOLD));
}

// Is all of ownerWhere in the speakerGroup?
// Also skip (return false) for any speakerGroups containing the original.
//  example:
//    Bob's friend.
//    If speakers contain Bob, but also contain 'friend', then 
//    the metagroup object resolution will return other speaker in the speaker group other than Bob,
//    which will be 'friend', which will not be useful.  We want the last speaker group
//    to contain Bob and one other entity which is not self (friend).
bool cSource::isSubsetOfSpeakers(int where, int ownerWhere, set <int>& speakers, bool inPrimaryQuote, bool& atLeastOneInSpeakerGroup)
{
	LFS
		atLeastOneInSpeakerGroup = false;
	bool allInSpeakerGroup = false, selfInSpeakerGroup = speakers.find(m[where].getObject()) != speakers.end();
	unsigned int numOwnerSpeakers = max(1, m[ownerWhere].objectMatches.size());
	intersect(ownerWhere, speakers, allInSpeakerGroup, atLeastOneInSpeakerGroup);
	if (rejectSG(ownerWhere, speakers, inPrimaryQuote))
	{
		atLeastOneInSpeakerGroup = false;
		return false;
	}
	// during the second scan (after speakerGroups have been defined), where is resolved to possibly the correct value.
	// if intersect with where is used, the (possibly correct) matched values will rule out the correct answer on the second scan.
	return (allInSpeakerGroup && !selfInSpeakerGroup && speakers.size() > numOwnerSpeakers);
}

// A=group of other objects other than ownerWhere in the speakerGroup
// if inPrimaryQuote, is the current speaker (from lastOpeningPrimaryQuote) the only object in A?  If so reject.
// if !inPrimaryQuote, is the current povSpeaker group matching A?  If so, reject.
bool cSource::rejectSG(int ownerWhere, set <int>& speakers, bool inPrimaryQuote)
{
	LFS
		if (inPrimaryQuote && (lastOpeningPrimaryQuote < 0 || m[lastOpeningPrimaryQuote].objectMatches.empty()))
			return false;
	if (!inPrimaryQuote && speakerGroups[currentSpeakerGroup].povSpeakers.empty())
		return false;
	for (set <int>::iterator si = speakers.begin(), siEnd = speakers.end(); si != siEnd; si++)
		if (!in(*si, ownerWhere) &&
			((inPrimaryQuote && in(*si, m[lastOpeningPrimaryQuote].objectMatches) == m[lastOpeningPrimaryQuote].objectMatches.end()) ||
				(!inPrimaryQuote && speakerGroups[currentSpeakerGroup].povSpeakers.find(*si) == speakerGroups[currentSpeakerGroup].povSpeakers.end())))
			return false;
	return true;
}

bool cSource::resolveMetaGroupByAssociation(int where, bool inPrimaryQuote, vector <cOM>& objectMatches, int latestOwnerWhere)
{
	LFS
		int o = m[where].getObject(), saveCSG = -1;
	bool eraseOwnerWhereMatches = false;
	if (m[latestOwnerWhere].objectMatches.empty())
	{
		bool firstPerson = (m[latestOwnerWhere].word->second.inflectionFlags & FIRST_PERSON) != 0;
		bool secondPerson = (m[latestOwnerWhere].word->second.inflectionFlags & SECOND_PERSON) != 0;
		bool inSecondaryLink = previousPrimaryQuote >= 0 && m[previousPrimaryQuote].getQuoteForwardLink() != -1;
		// my friend
		if ((firstPerson && inSecondaryLink) || (secondPerson && !inSecondaryLink))
			setMatched(latestOwnerWhere, previousSpeakers);
		// your friend
		if ((firstPerson && !inSecondaryLink) || (secondPerson && inSecondaryLink))
			setMatched(latestOwnerWhere, beforePreviousSpeakers);
		wstring tmpstr;
		if ((eraseOwnerWhereMatches = m[latestOwnerWhere].objectMatches.size() != 0) && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:matched %d temporarily to %s previousPrimaryQuote=%d[NL=%d FL=%d].",
				where, latestOwnerWhere, objectString(m[latestOwnerWhere].objectMatches, tmpstr, true).c_str(),
				previousPrimaryQuote, m[previousPrimaryQuote].nextQuote, m[previousPrimaryQuote].getQuoteForwardLink());
	}
	// find latest speakerGroup
	// does the currentSpeakerGroup contain every one of the latestOwnerWhere?
	bool atLeastOneInSpeakerGroup = false, restrictSGToGrouped = false, restrictCSGToGrouped = false, isSubset = false; // atLeastOneInPOVSpeakerGroup=false,
	// is the owner in speakers, and not in povSpeakers?
	int sg, csg = (isSubsetOfSpeakers(where, latestOwnerWhere, speakerGroups[currentSpeakerGroup].speakers, inPrimaryQuote, atLeastOneInSpeakerGroup)) ? currentSpeakerGroup : -1;
	// do any of the previous speakerGroups contain every one of the latestOwnerWhere?
	// prefer groupedSpeakers over just speakers
	for (sg = currentSpeakerGroup - 1; sg >= 0; sg--)
		if ((csg < 0 || speakerGroups[sg].speakers != speakerGroups[csg].speakers) &&
			((isSubset = isSubsetOfSpeakers(where, latestOwnerWhere, speakerGroups[sg].groupedSpeakers, inPrimaryQuote, atLeastOneInSpeakerGroup)) || atLeastOneInSpeakerGroup))
			break;
	// prefer groupedSpeakers over just speakers
	if (sg < 0)
		for (sg = currentSpeakerGroup - 1; sg >= 0; sg--)
			if ((csg < 0 || speakerGroups[sg].speakers != speakerGroups[csg].speakers) &&
				((isSubset = isSubsetOfSpeakers(where, latestOwnerWhere, speakerGroups[sg].speakers, inPrimaryQuote, atLeastOneInSpeakerGroup)) || atLeastOneInSpeakerGroup))
				break;
	// took best guess based on gender (Thomas Beresford) at 6159: instead of guessing friends based on last association (clerk)
	if ((m[where].flags & cWordMatch::flagResolveMetaGroupByGender) && m[where].objectMatches.size() == 1 && sg >= 0)
	{
		int preferGenderMatchSG, preferredGenderObject = m[where].objectMatches[0].object;
		for (preferGenderMatchSG = sg - 1; preferGenderMatchSG >= 0; preferGenderMatchSG--)
			if ((csg < 0 || speakerGroups[preferGenderMatchSG].speakers != speakerGroups[csg].speakers) &&
				speakerGroups[preferGenderMatchSG].speakers.find(preferredGenderObject) != speakerGroups[preferGenderMatchSG].speakers.end() &&
				((isSubset = isSubsetOfSpeakers(where, latestOwnerWhere, speakerGroups[preferGenderMatchSG].speakers, inPrimaryQuote, atLeastOneInSpeakerGroup)) || atLeastOneInSpeakerGroup))
				break;
		if (preferGenderMatchSG >= 0)
		{
			wstring tmpstr, tmpstr2, tmpstr3;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Preferred SG %s over SG %s because of preferred gender object %s.", where,
					toText(speakerGroups[preferGenderMatchSG], tmpstr2), toText(speakerGroups[sg], tmpstr), objectString(preferredGenderObject, tmpstr3, true).c_str());
			sg = preferGenderMatchSG;
		}
	}
	if (inPrimaryQuote && !(m[where].objectRole & HAIL_ROLE)) // if we are inQuote, they are unlikely to talk about someone who is in the current speaker group
	{
		saveCSG = csg; // but save just in case there is no other reference
		csg = -1;                                          // refer to them by friend, and say they are doing something.
	}
	if ((m[where].flags & cWordMatch::flagResolveMetaGroupByGender) && m[where].objectMatches.size() == 1 && csg >= 0 &&
		speakerGroups[csg].speakers.find(m[where].objectMatches[0].object) != speakerGroups[csg].speakers.end())
		sg = csg;
	if (inPrimaryQuote && (m[where].objectRole & HAIL_ROLE) && csg >= 0) // if we are inQuote, the speaker must be in the current speaker group
		sg = -1;
	if (!isSubset) sg = -1;
	if (csg < 0 && sg < 0)
	{
		if (saveCSG < 0)
		{
			if (eraseOwnerWhereMatches) m[latestOwnerWhere].objectMatches.clear();
			return false;
		}
		csg = saveCSG;
	}
	// see which group (sg,currentSpeakerGroup,subgroup of sg, or subgroup of currentSpeakerGroup)
	//  has all the speakers + a minimum number of other speakers that is >= 1
	int numSG = (sg >= 0) ? speakerGroups[sg].speakers.size() : 0, numCSG = (csg >= 0) ? speakerGroups[csg].speakers.size() : 0;
	if (sg >= 0 && isSubsetOfSpeakers(where, latestOwnerWhere, speakerGroups[sg].groupedSpeakers, inPrimaryQuote, atLeastOneInSpeakerGroup))
	{
		restrictSGToGrouped = true;
		numSG = speakerGroups[sg].groupedSpeakers.size();
	}
	if (csg >= 0 && isSubsetOfSpeakers(where, latestOwnerWhere, speakerGroups[csg].groupedSpeakers, inPrimaryQuote, atLeastOneInSpeakerGroup))
	{
		restrictCSGToGrouped = true;
		numCSG = speakerGroups[csg].groupedSpeakers.size();
	}
	if (sg >= 0 && csg >= 0)
	{
		// prefer csg over sg, so eliminate sg first, then if sg is not eliminated, try to eliminate csg.
		if (!restrictSGToGrouped && speakerGroups[sg].speakersAreNeverGroupedTogether) sg = -1;
		else if (!restrictCSGToGrouped && speakerGroups[csg].speakersAreNeverGroupedTogether) csg = -1;
	}
	// now sg and csg are guaranteed to be minimum.  Compare them against each other.
	// the next comparison is very tailored don't change unless you have thoroughly tested.
	if (csg >= 0 && ((sg >= 0 && numCSG <= numSG) || sg < 0))
	{
		sg = csg;
		restrictSGToGrouped = restrictCSGToGrouped;
	}
	// now sg is the minimum winner
	bool oneIn = false, allIn = false; // metaGroupObjectInSpeakerGroup=false,
	set <int>* speakers = (restrictSGToGrouped) ? &speakerGroups[sg].groupedSpeakers : &speakerGroups[sg].speakers;
	wstring tmpstr, tmpstr2, tmpstr3;
	int numInSpeakers = 0;
	bool friendOfObserver = intersect(latestOwnerWhere, speakerGroups[sg].observers, allIn, oneIn);
	if (allIn)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:Unable to resolve meta group object from speakers %s - all observers", where, objectString(*speakers, tmpstr2).c_str());
		if (eraseOwnerWhereMatches) m[latestOwnerWhere].objectMatches.clear();
		return false;
	}
	// put all speakers in sg that are NOT in latestOwnerWhere nor in objectMatches and ARE in the currentSpeakerGroup
	if (m[latestOwnerWhere].objectMatches.size())
	{
		for (set<int>::iterator si = speakers->begin(), siEnd = speakers->end(); si != siEnd; si++)
			if (in(*si, m[latestOwnerWhere].objectMatches) == m[latestOwnerWhere].objectMatches.end())
			{
				if (!restrictSGToGrouped && (friendOfObserver ^ (find(speakerGroups[sg].observers.begin(), speakerGroups[sg].observers.end(), *si) != speakerGroups[sg].observers.end())))
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:Rejected %s - must %sbe observer", where, objectString(*si, tmpstr2, true).c_str(), (friendOfObserver) ? L"" : L"NOT ");
					continue;
				}
				if (*si == m[where].getObject())
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, L"%06d:Unable to resolve meta group object from speakers %s - found the meta group in speakers", where,
							objectString(*speakers, tmpstr2).c_str());
					if (eraseOwnerWhereMatches) m[latestOwnerWhere].objectMatches.clear();
					return false; // meta group object is in the speaker group
				}
				else if (objects[o].matchGender(objects[*si]))
				{
					// check if *si was in groupedSpeakers or groups in any speakerGroup after sg.
					// if we are finding the meta-group companion of 'Whittington' and the only group found was 
					// Whittington and Tuppence but a group after sg but before the current speaker group
					// had Tuppence in it also, then (by definition) it does not have Whittington (because otherwise this
					// group after sg would actually be sg) so reject this 'Tuppence'.
					vector <cSpeakerGroup>::iterator sgi = speakerGroups.begin() + sg + 1, sgiEnd = speakerGroups.begin() + currentSpeakerGroup;
					if (sg + 1 > (int)currentSpeakerGroup) sgi = sgiEnd;
					for (; sgi != sgiEnd; sgi++)
						if (sgi->groupedSpeakers.find(*si) != sgi->groupedSpeakers.end())
							break;
					if (sgi != sgiEnd)
					{
						if (debugTrace.traceSpeakerResolution)
						{
							lplog(LOG_RESOLUTION, L"%06d:Rejected object %s as meta group match because of speaker group conflict between:", where, objectString(*si, tmpstr, true).c_str());
							lplog(LOG_RESOLUTION, L"%06d:speakerGroup %s", where, toText(speakerGroups[sg], tmpstr2));
							lplog(LOG_RESOLUTION, L"%06d:speakerGroup %s", where, toText(*sgi, tmpstr2));
						}
					}
					else
						objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
				}
				else if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:Rejected object %s as meta group match because of gender mismatch with %s.", where, objectString(*si, tmpstr, false).c_str(), objectString(o, tmpstr2, false).c_str());
			}
			else
				numInSpeakers++;
		if (numInSpeakers == 0)
			objectMatches.clear();
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:Resolving meta group object (1) - %s's friends from speakerGroup %s are %s.", where,
				objectString(m[latestOwnerWhere].objectMatches, tmpstr, true).c_str(), toText(speakerGroups[sg], tmpstr2), objectString(objectMatches, tmpstr3, true).c_str());
		if (objectMatches.size() > 1 && inPrimaryQuote && (m[where].objectRole & HAIL_ROLE))
		{
			bool inSecondaryLink = m[previousPrimaryQuote].getQuoteForwardLink() != -1;
			if (!inSecondaryLink && intersect(objectMatches, previousSpeakers, oneIn, allIn))
			{
				objectMatches.clear();
				for (vector<int>::iterator si = previousSpeakers.begin(), siEnd = previousSpeakers.end(); si != siEnd; si++)
					objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
			}
			if (inSecondaryLink && intersect(objectMatches, beforePreviousSpeakers, oneIn, allIn))
			{
				objectMatches.clear();
				for (vector<int>::iterator si = beforePreviousSpeakers.begin(), siEnd = beforePreviousSpeakers.end(); si != siEnd; si++)
					objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
			}
		}
	}
	else
	{
		for (set<int>::iterator si = speakers->begin(), siEnd = speakers->end(); si != siEnd; si++)
			if (*si != m[latestOwnerWhere].getObject())
			{
				if (*si == m[where].getObject())
				{
					if (eraseOwnerWhereMatches) m[latestOwnerWhere].objectMatches.clear();
					return false; // meta group object is in the speaker group
				}
				else if (speakerGroups[currentSpeakerGroup].speakers.size() < 3 || speakerGroups[currentSpeakerGroup].speakers.find(*si) != speakerGroups[currentSpeakerGroup].speakers.end())
				{
					if (objects[o].matchGender(objects[*si]))
						objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
					else
						lplog(LOG_RESOLUTION, L"%06d:Rejected object %s as meta group match because of gender mismatch (2).", where, objectString(*si, tmpstr, true).c_str());
				}
			}
			else
				numInSpeakers++;
		if (numInSpeakers == 0)
			objectMatches.clear();
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:Resolving meta group object (2) - %s's friends from speakerGroup %s are %s.", where,
				objectString(m[latestOwnerWhere].getObject(), tmpstr, true).c_str(), toText(speakerGroups[sg], tmpstr2), objectString(objectMatches, tmpstr3, true).c_str());
	}
	if (eraseOwnerWhereMatches) m[latestOwnerWhere].objectMatches.clear();
	if (objectMatches.size() && (m[where].flags & cWordMatch::flagResolveMetaGroupByGender)) m[where].objectMatches.clear();
	if (objectMatches.size() == 1)
		replaceObjectInSection(where, objectMatches[0].object, m[where].getObject(), L"resolveMetaGroupObject");
	return true;
}

bool cSource::resolveMetaGroupObject(int where, bool inPrimaryQuote, bool inSecondaryQuote, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb,
	bool definitelySpeaker, bool resolveForSpeaker, bool avoidCurrentSpeaker, bool& mixedPlurality, bool limitTwo, vector <cOM>& objectMatches, bool& chooseFromLocalFocus)
{
	LFS
		if (m[where].word->first == L"this" || m[where].word->first == L"that")
			return false;
	wstring tmpstr, tmpstr2;
	// a friend of mine
	int wo, end;
	vector <cObject>::iterator object = objects.begin() + m[where].getObject();
	int beginEntirePosition = m[where].beginObjectPosition; // if this is an adjectival object 
	if (m[where].flags & cWordMatch::flagAdjectivalObject)
		for (; beginEntirePosition >= 0 && m[beginEntirePosition].principalWherePosition < 0; beginEntirePosition--);
	bool partiallySpecified = object->getOwnerWhere() == -1 && isMetaGroupWord(where) && m[end = m[where].endObjectPosition].word->first == L"of" &&
		(wo = m[end + 1].getObject()) != -1 && (wo < 0 || objects[wo].isAgent(true));
	int wordOrderSensitiveModifier = object->wordOrderSensitive(where, m);
	int subjectCataRestriction = -1;
	bool physicallyEvaluated, isPhysicallyPresent = physicallyPresentPosition(where, beginEntirePosition, physicallyEvaluated, false);
	if (m[beginEntirePosition].word->first == L"no")
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:RESOLVING negative metagroup %s rejected", where, objectString(object, tmpstr, false).c_str());
		return false;
	}
	if (!unResolvablePosition(beginEntirePosition) || partiallySpecified)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:RESOLVING metagroup %s%s", where, objectString(object, tmpstr, false).c_str(), m[where].roleString(tmpstr2).c_str());
		int toMatch = -1;
		vector <cOM> identityMatches;
		if ((m[where].objectRole & (SUBJECT_ROLE | IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) == (SUBJECT_ROLE | IS_OBJECT_ROLE) && m[where].getRelObject() >= 0 &&
			(toMatch = m[m[where].getRelObject()].getObject()) >= 0 &&
			// disallow 'The big one was mine.'
			m[m[where].getRelObject()].queryWinnerForm(possessivePronounForm) < 0 &&
			// He was here.
			(m[m[where].getRelObject()].word->first != L"here" && m[m[where].getRelObject()].word->first != L"there"))
		{
			resolveObject(m[where].getRelObject(), definitelySpeaker, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, resolveForSpeaker, avoidCurrentSpeaker, limitTwo);
			identityMatches = m[m[where].getRelObject()].objectMatches;
			vector <cLocalFocus>::iterator lsi;
			if (m[m[where].getRelObject()].objectMatches.empty())
			{
				identityMatches.push_back(cOM(toMatch, SALIENCE_THRESHOLD));
				if ((lsi = in(toMatch)) != localObjects.end())
				{
					if (!lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge)) lsi->setInSalience();
					lsi->om.salienceFactor += IS_SALIENCE_BOOST;
					if (debugTrace.traceSpeakerResolution)
						itos(L"+IS_PREF[+", IS_SALIENCE_BOOST, lsi->res, L"]");
				}
			}
			vector <cOM>::iterator omi, omEnd = m[m[where].getRelObject()].objectMatches.end();
			for (omi = m[m[where].getRelObject()].objectMatches.begin(); omi != omEnd; omi++)
			{
				if ((lsi = in(omi->object)) != localObjects.end())
				{
					if (!lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge)) lsi->setInSalience();
					lsi->om.salienceFactor += IS_SALIENCE_BOOST;
					if (debugTrace.traceSpeakerResolution)
					{
						itos(L"+IS_PREF[+", IS_SALIENCE_BOOST, lsi->res, L"]");
						lplog(LOG_RESOLUTION, L"%06d:Boosting %s", where, objectString(lsi->om, tmpstr, false).c_str());
					}
				}
			}
		}
		// the second of the two men
		end = m[where].endObjectPosition;
		if (object->getOwnerWhere() == -1 && end >= 0 && end + 1 < (signed)m.size() && (wo = cObject::whichOrderWord(m[where].word)) != -1 && m[end].word->first == L"of" && m[end + 1].principalWherePosition >= 0)
		{
			resolveObject(m[end + 1].principalWherePosition, definitelySpeaker, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, resolveForSpeaker, avoidCurrentSpeaker, limitTwo);
			if (resolveWordOrderOfObject(where, wo, m[end + 1].principalWherePosition, objectMatches)) return true;
		}
		checkSubsequent(where, definitelySpeaker, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, resolveForSpeaker, avoidCurrentSpeaker, objectMatches);
		chooseFromLocalFocus = false;
		bool singularBodyPart;
		if (partiallySpecified && m[where].endObjectPosition >= 0)
			object->setOwnerWhere(m[where].endObjectPosition + 1);
		if (!resolveMetaGroupSpecificObject(where, inPrimaryQuote, inSecondaryQuote, definitelySpeaker | resolveForSpeaker, lastBeginS1, lastRelativePhrase, objectMatches, chooseFromLocalFocus) &&
			// the efficient German master of ceremonies should be matched by elimination from a list of physically present objects to 'a tall man',
			// but 'the master of the house' must not be matched to 'a butler' - the object must not already be doing something on first introduction.
			(object->male || object->female) && cObject::whichOrderWord(m[where].word) < 0 && toMatch < 0 &&
			// must not match 'other hand' to anything other than another body object, unless it is a subject ('that basilisk glance' - 28602)
			(!isExternalBodyPart(where, singularBodyPart, true) || (m[where].objectRole & (SUBJECT_ROLE | PREP_OBJECT_ROLE)) == SUBJECT_ROLE) &&
			// a meta group speaker that was already there doing something
			(m[where].getRelVerb() < 0 || (m[m[where].getRelVerb()].verbSense & (VT_PAST | VT_EXTENDED)) != (VT_PAST | VT_EXTENDED)))
		{
			chooseFromLocalFocus = resolveGenderedObject(where, definitelySpeaker | resolveForSpeaker, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, objectMatches, object, wordOrderSensitiveModifier, subjectCataRestriction, mixedPlurality, limitTwo, isPhysicallyPresent, physicallyEvaluated);
			m[where].flags |= cWordMatch::flagResolveMetaGroupByGender;
		}
		// if this is a definite speaker, yet the answer is not within the current speaker group, make an equivalence, if a unique mapping can be established through gender.
		if (definitelySpeaker && !chooseFromLocalFocus && objectMatches.size() == 1 && currentSpeakerGroup < speakerGroups.size() &&
			speakerGroups[currentSpeakerGroup].speakers.find(objectMatches[0].object) == speakerGroups[currentSpeakerGroup].speakers.end() &&
			(objects[objectMatches[0].object].male ^ objects[objectMatches[0].object].female))
		{
			int uniqueGenderMatch = -1;
			// if only one of the speakers in the current speaker group match the gender of the object, replace.
			for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].speakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].speakers.end(); si != siEnd; si++)
				if (objects[objectMatches[0].object].matchGender(objects[*si]))
				{
					if (uniqueGenderMatch != -1)
					{
						uniqueGenderMatch = -1;
						break;
					}
					else
						uniqueGenderMatch = *si;
				}
			if (uniqueGenderMatch != -1)
			{
				replaceObjectInSection(where, uniqueGenderMatch, objectMatches[0].object, L"uniqueGenderMetaObjectSpeaker");
				objectMatches[0].object = uniqueGenderMatch;
			}
		}
		if (!chooseFromLocalFocus && objectMatches.empty() && identityMatches.size())
		{
			objectMatches = identityMatches;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:matched by identity to %s (1)", where, objectString(objectMatches, tmpstr, true).c_str());
		}
		excludePOVSpeakers(where, L"1");
	}
	else
	{
		int toMatch;
		if ((m[where].objectRole & (OBJECT_ROLE | IS_OBJECT_ROLE | PREP_OBJECT_ROLE)) == (OBJECT_ROLE | IS_OBJECT_ROLE) &&
			m[where].relSubject >= 0 && (toMatch = m[m[where].relSubject].getObject()) >= 0 &&
			(objects[toMatch].objectClass != PRONOUN_OBJECT_CLASS || m[m[where].relSubject].objectMatches.size()))
		{
			objectMatches = m[m[where].relSubject].objectMatches;
			vector <cLocalFocus>::iterator lsi;
			if (m[m[where].relSubject].objectMatches.empty())
			{
				objectMatches.push_back(cOM(toMatch, SALIENCE_THRESHOLD));
				if ((lsi = in(toMatch)) != localObjects.end())
				{
					if (!lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge)) lsi->setInSalience();
					lsi->om.salienceFactor += IS_SALIENCE_BOOST;
					if (debugTrace.traceSpeakerResolution)
						itos(L"+IS_PREF[+", IS_SALIENCE_BOOST, lsi->res, L"]");
				}
			}
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:matched by identity to %s (2)", where, objectString(objectMatches, tmpstr, true).c_str());
		}
		// Mrs . Vandemeyer was expecting a guest to dinner
		if (m[where].getObject() >= 0 && objects[m[where].getObject()].getOwnerWhere() < 0 && currentSpeakerGroup + 1 < speakerGroups.size() &&
			(m[m[where].beginObjectPosition].pma.queryPattern(L"_META_NAME_EQUIVALENCE") == -1))
		{
			// Another voice[german] , which Tommy fancied was that[german] of the tall , commanding - looking man[man] whose face[german] had seemed familiar to him[man,boris,tommy,irish] , said :
			if (isGroupJoiner(m[where].word) && !inPrimaryQuote && !inSecondaryQuote) addNewSpeaker(where, objectMatches);
			// two men / three men
			if (m[m[where].beginObjectPosition].queryWinnerForm(numeralCardinalForm) >= 0 && !inPrimaryQuote && !inSecondaryQuote)
				addNewNumberedSpeakers(where, objectMatches);
			if (object->getOwnerWhere() == -1 && m[end = m[where].endObjectPosition].word->first == L"of")
			{
				if ((wo = cObject::whichOrderWord(m[where].word)) != -1 && m[end + 1].principalWherePosition >= 0)
				{
					resolveObject(m[end + 1].principalWherePosition, definitelySpeaker, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, resolveForSpeaker, avoidCurrentSpeaker, limitTwo);
					if (resolveWordOrderOfObject(where, wo, m[end + 1].principalWherePosition, objectMatches)) return true;
				}
			}
		}
		int originalMatches = objectMatches.size();
		checkSubsequent(where, definitelySpeaker, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, resolveForSpeaker, avoidCurrentSpeaker, objectMatches);
		// match one match to the other, by adding adjectives
		if (originalMatches == 1 && objectMatches.size() == 2 && ((objects[objectMatches[0].object].objectClass == NAME_OBJECT_CLASS) ^ (objects[objectMatches[1].object].objectClass == NAME_OBJECT_CLASS)))
		{
			if (objects[objectMatches[0].object].objectClass == NAME_OBJECT_CLASS)
				moveNyms(where, objectMatches[0].object, objectMatches[1].object, L"IS_META");
			else
				moveNyms(where, objectMatches[1].object, objectMatches[0].object, L"IS_META");
		}
		// look ahead for the next paragraph(s) and look for the first new matching object
		// "another ally" = Julius (20406 Agatha) not inQuote
		// "another man" = Boris inQuote
		if (objectMatches.empty() && wordOrderSensitiveModifier >= 0 && cObject::wordOrderWords[wordOrderSensitiveModifier] == L"another" && currentSpeakerGroup < speakerGroups.size())
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:RESOLVING gendered [future] %s", where, objectString(object, tmpstr, false).c_str());
			scanFutureGenderedMatchingObjects(where, inPrimaryQuote, object, objectMatches);
			if (objectMatches.size() == 1)
				replaceObjectInSection(where, objectMatches[0].object, m[where].getObject(), L"resolveMetaGroupWordOrderedFutureObject");
		}
		if (objectMatches.empty() && (m[where].objectRole & (SUBJECT_ROLE | PREP_OBJECT_ROLE)) == SUBJECT_ROLE &&
			!(m[where].flags & cWordMatch::flagAdjectivalObject)) // see isFocus
			m[where].objectRole |= UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
	}
	return objectMatches.size() > 0;
}
