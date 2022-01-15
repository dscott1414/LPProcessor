#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "profile.h"

void cSource::pushSpeaker(int where, int s, int sf, const wchar_t* fromWhere)
{
	LFS
		wstring tmpstr;
	m[where].objectMatches.push_back(cOM(s, sf));
	m[where].beginObjectPosition = where;
	m[where].endObjectPosition = where + 1;
	objects[s].locations.push_back(cObject::cLocation(where));
	objects[s].updateFirstLocation(where);
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION, L"%06d:FPP %s of %s", where, fromWhere, objectString(s, tmpstr, true).c_str());
}

bool cSource::matchObjectToSpeakers(int mI, vector <cOM>& currentSpeaker, vector <cOM>& previousSpeaker, int inflectionFlags, unsigned __int64 quoteFlags, int lastEmbeddedStoryBegin)
{
	LFS
		if (m[mI].flags & cWordMatch::flagObjectResolved) return false; // if secondary speaker
	m[mI].flags |= cWordMatch::flagObjectResolved;
	wstring tmpstr;
	// PLURAL: we, us, our, ourselves, ours
	if ((inflectionFlags & (FIRST_PERSON | SECOND_PERSON)) == (FIRST_PERSON | SECOND_PERSON))
	{
		m[mI].objectMatches.insert(m[mI].objectMatches.begin(), currentSpeaker.begin(), currentSpeaker.end());
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:FPP (1) of %s", mI, objectString(currentSpeaker, tmpstr, true).c_str());
		// if embedded and past, search for grouping with the speaker
		if ((m[mI].objectRole & (IN_EMBEDDED_STORY_OBJECT_ROLE | IN_PRIMARY_QUOTE_ROLE | NONPRESENT_OBJECT_ROLE)) == (IN_EMBEDDED_STORY_OBJECT_ROLE | IN_PRIMARY_QUOTE_ROLE | NONPRESENT_OBJECT_ROLE) &&
			!(quoteFlags & cWordMatch::flagSecondEmbeddedStory) && currentEmbeddedSpeakerGroup >= 0 &&
			currentEmbeddedSpeakerGroup < (int)speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size()) // the story is about a shared experience - special treatment not necessary
		{
			for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.end(); si != siEnd; si++)
				if (in(*si, currentSpeaker) == currentSpeaker.end() && in(*si, previousSpeaker) == previousSpeaker.end() &&
					objects[*si].originalLocation<mI && locationBefore(*si, mI)>lastEmbeddedStoryBegin)
					pushSpeaker(mI, *si, SALIENCE_THRESHOLD, L"(1)");
			eliminateBodyObjectRedundancy(mI, m[mI].objectMatches);
		}
		else
			for (vector <cOM>::iterator psi = previousSpeaker.begin(); psi != previousSpeaker.end(); psi++)
				if (in(psi->object, currentSpeaker) == currentSpeaker.end())
					pushSpeaker(mI, psi->object, psi->salienceFactor, L"(2)");
		return true;
	}
	// currentSpeaker - SINGULAR: I, me, my, myself, mine
	if (inflectionFlags & FIRST_PERSON)
		for (vector <cOM>::iterator s = currentSpeaker.begin(), sEnd = currentSpeaker.end(); s != sEnd; s++)
			if (objects[s->object].plural == ((inflectionFlags & PLURAL) == PLURAL || (inflectionFlags & PLURAL_OWNER) == PLURAL_OWNER))
				pushSpeaker(mI, s->object, s->salienceFactor, L"(3)");
	// lastSpeaker - SINGULAR: yourself PLURAL:our, your, thy, you, they, thou, you, thee, yourselves, yours, thine
	// some of the pronouns are both singular and plural
	if (inflectionFlags & SECOND_PERSON)
		for (vector <cOM>::iterator s = previousSpeaker.begin(), sEnd = previousSpeaker.end(); s != sEnd; s++)
			if ((objects[s->object].plural && ((inflectionFlags & PLURAL) == PLURAL || (inflectionFlags & PLURAL_OWNER) == PLURAL_OWNER)) ||
				(!objects[s->object].plural && ((inflectionFlags & SINGULAR) == SINGULAR || (inflectionFlags & SINGULAR_OWNER) == SINGULAR_OWNER)))
				pushSpeaker(mI, s->object, s->salienceFactor, L"(4)");
	return true;
}

// THIRD_PERSON cannot involve either speaker.
void cSource::removeSpeakers(int mI, vector <cOM>& speakers)
{
	LFS
		if (m[mI].objectMatches.size() <= 1) return;
	for (vector <cOM>::iterator I = m[mI].objectMatches.begin(); I != m[mI].objectMatches.end(); )
	{
		bool allIn, oneIn;
		// remove the speaker and any of speakers body
		if (in(I->object, speakers) != speakers.end() || (objects[I->object].getOwnerWhere() >= 0 && objects[I->object].objectClass == BODY_OBJECT_CLASS && intersect(m[objects[I->object].getOwnerWhere()].objectMatches, speakers, allIn, oneIn)))
		{
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr;
				lplog(LOG_RESOLUTION, L"%06d:erased match of %s", mI, objectString(*I, tmpstr, true).c_str());
			}
			I = m[mI].objectMatches.erase(I);
		}
		else
			I++;
	}
}

void cSource::removeSpeakers(int mI, set <int>& speakers)
{
	LFS
		for (vector <cOM>::iterator I = m[mI].objectMatches.begin(); I != m[mI].objectMatches.end(); )
		{
			bool allIn, oneIn;
			// remove the speaker and any of speakers body
			if (speakers.find(I->object) != speakers.end() || (objects[I->object].getOwnerWhere() >= 0 && objects[I->object].objectClass == BODY_OBJECT_CLASS && intersect(m[objects[I->object].getOwnerWhere()].objectMatches, speakers, allIn, oneIn)))
			{
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr;
					lplog(LOG_RESOLUTION, L"%06d:erased match of %s", mI, objectString(*I, tmpstr, true).c_str());
				}
				I = m[mI].objectMatches.erase(I);
			}
			else
				I++;
		}
}

void cSource::resolveFirstSecondPersonPronoun(int where, unsigned __int64 flags, int lastEmbeddedStoryBegin, vector <cOM>& currentSpeaker, vector <cOM>& previousSpeaker)
{
	LFS
		if (currentSpeaker.size() == 1 && currentSpeaker[0].object == -1) return; // Quoted String
	vector <cObject>::iterator object = objects.begin() + m[where].getObject();
	int inflectionFlags = m[where].word->second.inflectionFlags, objectClass = (m[where].getObject() >= 0) ? object->objectClass : -1;
	// 'The look of you' / 'one of us'
	if (objectClass >= 0 && objectClass != REFLEXIVE_PRONOUN_OBJECT_CLASS && objectClass != PRONOUN_OBJECT_CLASS)
		for (vector <cOM>::iterator oi = m[where].objectMatches.begin(); oi != m[where].objectMatches.end(); oi++)
		{
			if (objects[oi->object].objectClass == PRONOUN_OBJECT_CLASS &&
				(m[objects[oi->object].originalLocation].word->second.inflectionFlags & SECOND_PERSON) != 0)
			{
				m[where].objectMatches.erase(oi);
				objectClass = PRONOUN_OBJECT_CLASS;
				inflectionFlags = m[objects[oi->object].originalLocation].word->second.inflectionFlags;
				m[where].flags &= ~cWordMatch::flagObjectResolved;
				break;
			}
		}
	bool firstPerson = (inflectionFlags & FIRST_PERSON) != 0, secondPerson = (inflectionFlags & SECOND_PERSON) != 0;
	if (objectClass == REFLEXIVE_PRONOUN_OBJECT_CLASS)
	{
		// FIRST_PERSON:  "myself", "ourselves",
		// SECOND_PERSON: "ourselves","yourself","yourselves",
		if (firstPerson || secondPerson)
			matchObjectToSpeakers(where, currentSpeaker, previousSpeaker, inflectionFlags, flags, lastEmbeddedStoryBegin);
		else
		{
			// THIRD_PERSON:  "himself","herself","itself","themselves",
			if (inflectionFlags & THIRD_PERSON)
			{
				removeSpeakers(where, currentSpeaker);
				removeSpeakers(where, previousSpeaker);
				if (m[where].objectMatches.size() > 1)
					removeSpeakers(where, speakerGroups[currentSpeakerGroup].speakers);
			}
		}
	}
	else if (m[where].getObject() < cObject::eOBJECTS::UNKNOWN_OBJECT || objectClass == PRONOUN_OBJECT_CLASS)
	{
		if (firstPerson || secondPerson)
			// FIRST_PERSON:  "my", "i", "me", "mine" -- restrict to speaker or narrator
			// FIRST&SECOND_PERSON: "our", "we", "us", "ours" - restrict to all speakers
			// SECOND_PERSON: "your", "thy", "you", "thee", "thou", "yours","thine" - restrict to all other speakers but the speaker
			matchObjectToSpeakers(where, currentSpeaker, previousSpeaker, inflectionFlags, flags, lastEmbeddedStoryBegin);
		else
		{
			// THIRD_PERSON:  "her","his","its","their","he","she","it","they","him","her","them" - restrict to all non-speakers
			//                "his","hers","theirs"
			if ((inflectionFlags & THIRD_PERSON) && m[where].word->first != L"one") // one is totally generic and should not be restricted
			{
				int saveObject = -1;
				if (m[where].getObject() >= 0 &&
					currentSpeakerGroup < speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.size()>2 &&
					(m[where].objectMatches.empty() ||
						(m[where].objectMatches.size() == 1 && (in(saveObject = m[where].objectMatches[0].object, currentSpeaker) != currentSpeaker.end() || in(saveObject, previousSpeaker) != previousSpeaker.end()))))
				{
					m[where].objectMatches.clear();
					for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].speakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].speakers.end(); si != siEnd; si++)
						if (in(*si, currentSpeaker) == currentSpeaker.end() && in(*si, previousSpeaker) == previousSpeaker.end() &&
							objects[m[where].getObject()].matchGenderIncludingNeuter(objects[*si]))
						{
							m[where].objectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
							objects[*si].locations.push_back(cObject::cLocation(where));
							objects[*si].updateFirstLocation(where);
						}
					// if there is 'they' and there aren't enough people for 'they' then perhaps 'they' is not anything.
					if ((inflectionFlags & PLURAL) && m[where].objectMatches.size() < 2)
						m[where].objectMatches.clear();
				}
				else
				{
					removeSpeakers(where, currentSpeaker);
					removeSpeakers(where, previousSpeaker);
					if (m[where].objectMatches.size() > 1)
						removeSpeakers(where, speakerGroups[currentSpeakerGroup].speakers);
				}
			}
		}
	}
	// you are a young couple
	if ((m[where].objectRole & (IS_OBJECT_ROLE | OBJECT_ROLE | NOT_OBJECT_ROLE)) == (IS_OBJECT_ROLE | OBJECT_ROLE) && m[where].relSubject >= 0 &&
		m[m[where].relSubject].getObject() >= 0 &&
		(m[m[where].relSubject].word->second.inflectionFlags & (FIRST_PERSON | SECOND_PERSON)) != 0 &&
		m[where].objectMatches.empty() &&
		!(m[where].flags & (cWordMatch::flagInQuestion | cWordMatch::flagInPStatement)))
	{
		bool allIn, oneIn;
		if ((m[m[where].relSubject].word->second.inflectionFlags & FIRST_PERSON) != 0 && !intersect(where, currentSpeaker, allIn, oneIn))
		{
			m[where].objectMatches.insert(m[where].objectMatches.begin(), currentSpeaker.begin(), currentSpeaker.end());
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Resolved IS_OBJECT to speaker [relSubject@%d].", where, m[where].relSubject);
		}
		if ((m[m[where].relSubject].word->second.inflectionFlags & SECOND_PERSON) != 0 && !intersect(where, previousSpeaker, allIn, oneIn))
		{
			m[where].objectMatches.insert(m[where].objectMatches.begin(), previousSpeaker.begin(), previousSpeaker.end());
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Resolved IS_OBJECT to audience [relSubject@%d].", where, m[where].relSubject);
		}
	}
	bool singular = (inflectionFlags & (SINGULAR | SINGULAR_OWNER)) != 0, plural = (inflectionFlags & (PLURAL | PLURAL_OWNER)) != 0;
	// if plural and not singular (we, not you), or if not resolved definitely, AND only one speaker
	if (((plural && !singular) ||
		((!firstPerson || !(flags & cWordMatch::flagDefiniteResolveSpeakers)) &&
			(!secondPerson || !(flags & (cWordMatch::flagFromLastDefiniteResolveAudience | cWordMatch::flagSpecifiedResolveAudience))))) &&
		currentSpeaker.size() == 1 &&
		// if this is a story not about a shared experience, don't subgroupMatch because it is based on the speakerGroup, not the embedded speaker group.
		(!(m[where].objectRole & (IN_EMBEDDED_STORY_OBJECT_ROLE) || (flags & cWordMatch::flagSecondEmbeddedStory))) &&
		// if there is only one previous speaker, there is no choice for 'you' or 'yourself'
		(!secondPerson || firstPerson || previousSpeaker.size() > 1 || m[where].objectMatches.empty()))
		preferSubgroupMatch(where, objectClass, inflectionFlags, true, currentSpeaker[0].object, false);
}

bool masterCompare(const vector<cObject>::iterator& lhs, const vector<cObject>::iterator& rhs)
{
	LFS
		return lhs->numIdentifiedAsSpeaker + lhs->numDefinitelyIdentifiedAsSpeaker > rhs->numIdentifiedAsSpeaker + rhs->numDefinitelyIdentifiedAsSpeaker;
}

void cSource::resolveFirstSecondPersonPronouns(vector <int>& secondaryQuotesResolutions)
{
	LFS
		if (speakerGroups.empty()) return;
	bool inPrimaryQuote = false, inSecondaryQuote = false;
	section = 0;
	usePIS = true;
	currentSpeakerGroup = 0;
	currentEmbeddedSpeakerGroup = -1;
	currentEmbeddedTimelineSegment = -1;
	vector <cWordMatch>::iterator im = m.begin(), imend = m.end(), lastOpeningPrimaryQuoteIM = wmNULL, lastOpeningSecondaryQuoteIM = wmNULL;
	lastOpeningPrimaryQuote = lastOpeningSecondaryQuote = -1;
	int sqr = 0, lastEmbeddedStoryBegin = -1; //howMany=0;
	initializeTimelineSegments();
	for (int I = 0; im != imend; im++, I++)
	{
		debugTrace = im->t;
		if (section + 1 < sections.size() && I == sections[section + 1].begin)
			section++;
		while (currentSpeakerGroup + 1 < speakerGroups.size() && I == speakerGroups[currentSpeakerGroup + 1].sgBegin)
		{
			currentSpeakerGroup++;
			currentEmbeddedSpeakerGroup = -1;
		}
		if (currentSpeakerGroup < speakerGroups.size() && speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size()>0)
		{
			//if (currentEmbeddedSpeakerGroup<0) currentEmbeddedSpeakerGroup=0;
			while (currentEmbeddedSpeakerGroup + 1 < (signed)speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size() &&
				I == speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup + 1].sgBegin)
				currentEmbeddedSpeakerGroup++;
		}
		else
		{
			currentEmbeddedSpeakerGroup = -1;
			currentEmbeddedTimelineSegment = -1;
		}
		createTimelineSegment(I);
		if (im->getObject() >= 0 && objects[im->getObject()].objectClass == META_GROUP_OBJECT_CLASS && inPrimaryQuote)
		{
			// my friend / your friend / friend of mine
			if (objects[im->getObject()].getOwnerWhere() >= 0 && (m[objects[im->getObject()].getOwnerWhere()].word->second.inflectionFlags & (FIRST_PERSON | SECOND_PERSON)) != 0 &&
				// if identity relationship created a match, don't rematch
				(m[I].objectMatches.empty() ||
					(!(m[I].objectRole & RE_OBJECT_ROLE) && (m[I].objectRole & (SUBJECT_ROLE | IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) != (SUBJECT_ROLE | IS_OBJECT_ROLE)))) // Copular
			{
				if (objects[im->getObject()].getOwnerWhere() > I)
					resolveFirstSecondPersonPronoun(objects[im->getObject()].getOwnerWhere(),
						(inSecondaryQuote) ? lastOpeningSecondaryQuoteIM->flags : lastOpeningPrimaryQuoteIM->flags, lastEmbeddedStoryBegin,
						(inSecondaryQuote) ? lastOpeningSecondaryQuoteIM->objectMatches : lastOpeningPrimaryQuoteIM->objectMatches,
						(inSecondaryQuote) ? lastOpeningSecondaryQuoteIM->audienceObjectMatches : lastOpeningPrimaryQuoteIM->audienceObjectMatches);
				if (!(m[I].flags & cWordMatch::flagResolveMetaGroupByGender))
					m[I].objectMatches.clear(); // also set in speakerResolution
				vector <cOM> objectMatches;
				wstring tmpstr, tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:RESOLVING metagroup %s%s", I, objectString(im->getObject(), tmpstr, false).c_str(), m[I].roleString(tmpstr2).c_str());
				resolveMetaGroupByAssociation(I, (im->objectRole & IN_PRIMARY_QUOTE_ROLE) != 0, objectMatches, objects[im->getObject()].getOwnerWhere());
				if (objectMatches.size()) im->objectMatches = objectMatches;
			}
			else if (im->objectMatches.empty() && objects[im->getObject()].wordOrderSensitive(I, m) != 0)
			{
				m[I].flags &= ~cWordMatch::flagObjectResolved;
				resolveObject(I, true, inPrimaryQuote, inSecondaryQuote, -1, -1, -1, -1, true, true, false);
			}
		}
		bool allIn, oneIn;
		if ((im->objectRole & HAIL_ROLE) && im->objectMatches.size() > 1 && inPrimaryQuote && lastOpeningPrimaryQuoteIM->audienceObjectMatches.size() &&
			intersect(lastOpeningPrimaryQuoteIM->audienceObjectMatches, im->objectMatches, allIn, oneIn))
			unMatchObjects(I, lastOpeningPrimaryQuoteIM->audienceObjectMatches, false);
		if ((im->getObject() >= 0 || im->getObject() < cObject::eOBJECTS::UNKNOWN_OBJECT) && inSecondaryQuote && (im->objectRole & HAIL_ROLE) &&
			lastOpeningSecondaryQuoteIM->audienceObjectMatches.size())
		{
			unMatchObjects(I, lastOpeningSecondaryQuoteIM->audienceObjectMatches, false);
			if (m[I].getObject() != lastOpeningSecondaryQuoteIM->audienceObjectMatches[0].object &&
				(m[I].objectMatches.empty() || lastOpeningSecondaryQuoteIM->audienceObjectMatches[0].object != m[I].objectMatches[0].object))
				pushSpeaker(I, lastOpeningSecondaryQuoteIM->audienceObjectMatches[0].object, lastOpeningSecondaryQuoteIM->audienceObjectMatches[0].salienceFactor, L"(5)");
		}
		// narrative you and I are resolved earlier.  But may not include both speakers.
		if ((im->word->second.inflectionFlags & (FIRST_PERSON | SECOND_PERSON)) == (FIRST_PERSON | SECOND_PERSON) &&
			(im->queryWinnerForm(PROPER_NOUN_FORM_NUM) < 0 && im->queryWinnerForm(nounForm) < 0) && // US can be a proper noun, mine can be a noun
			(im->getObject() >= 0 || im->getObject() < cObject::eOBJECTS::UNKNOWN_OBJECT) && !inPrimaryQuote && !inSecondaryQuote && im->objectMatches.size() <= 1)
		{
			// are any speakers grouped with the Narrator?  If so, include only this.
			if (speakerGroups[currentSpeakerGroup].groupedSpeakers.find(0) != speakerGroups[currentSpeakerGroup].groupedSpeakers.end())
			{
				for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].groupedSpeakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].groupedSpeakers.end(); si != siEnd; si++)
					if (*si)
						im->objectMatches.push_back(cOM(*si, 0));
			}
			else
			{
				// if still only 1 match, match with other speaker.
				for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].speakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].speakers.end(); si != siEnd; si++)
					if (*si)
						im->objectMatches.push_back(cOM(*si, 0));
			}
		}
		if ((im->getObject() >= 0 || im->getObject() < cObject::eOBJECTS::UNKNOWN_OBJECT) && (inPrimaryQuote || inSecondaryQuote))
			resolveFirstSecondPersonPronoun(I,
				(inSecondaryQuote) ? lastOpeningSecondaryQuoteIM->flags : lastOpeningPrimaryQuoteIM->flags, lastEmbeddedStoryBegin,
				(inSecondaryQuote) ? lastOpeningSecondaryQuoteIM->objectMatches : lastOpeningPrimaryQuoteIM->objectMatches,
				(inSecondaryQuote) ? lastOpeningSecondaryQuoteIM->audienceObjectMatches : lastOpeningPrimaryQuoteIM->audienceObjectMatches);
		if (sqr < (signed)secondaryQuotesResolutions.size() && secondaryQuotesResolutions[sqr] == I)
		{
			//  0:(openingSecondaryQuote);
			//  1:(closingSecondaryQuote);
			//  2:(secondarySpeaker);
			//  3:(audienceObjectPosition); 
			int secondarySpeaker = secondaryQuotesResolutions[sqr + 2];
			int audienceObjectPosition = secondaryQuotesResolutions[sqr + 3];
			bool audienceFilled = false;
			if (secondarySpeaker >= 0)
			{
				if ((m[secondarySpeaker].getObject() >= 0 || m[secondarySpeaker].getObject() < cObject::eOBJECTS::UNKNOWN_OBJECT) && lastOpeningPrimaryQuoteIM != wmNULL)
					resolveFirstSecondPersonPronoun(secondarySpeaker,
						lastOpeningPrimaryQuoteIM->flags, lastEmbeddedStoryBegin,
						lastOpeningPrimaryQuoteIM->objectMatches,
						lastOpeningPrimaryQuoteIM->audienceObjectMatches);
				secondarySpeaker = m[secondarySpeaker].principalWherePosition;
				if (secondarySpeaker >= 0)
				{
					if (m[secondarySpeaker].objectMatches.size())
						m[I].objectMatches = m[secondarySpeaker].objectMatches;
					else if (m[secondarySpeaker].getObject() >= 0)
						m[I].objectMatches.push_back(cOM(m[secondarySpeaker].getObject(), SALIENCE_THRESHOLD));
				}
			}
			else
			{
				int selfReferringSpeakerFound = -1, referringAudienceFound = -1;
				for (int J = secondaryQuotesResolutions[sqr + 0]; J < secondaryQuotesResolutions[sqr + 1] && selfReferringSpeakerFound < 0; J++)
					if ((m[J].objectRole & IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE) && (m[J].getObject() >= 0 || m[J].objectMatches.size()))
						selfReferringSpeakerFound = J;
					else if ((m[J].objectRole & IN_QUOTE_REFERRING_AUDIENCE_ROLE) && (m[J].getObject() >= 0 || m[J].objectMatches.size()))
						referringAudienceFound = J;
				// hail cannot match speaker / He[mr] then says : ‘[mr:mr] Please take a seat , Mr[mr] . -- er ? ’ 
				if (selfReferringSpeakerFound >= 0 && (m[selfReferringSpeakerFound].objectMatches.empty() || m[I].objectMatches.empty() ||
					m[selfReferringSpeakerFound].objectMatches[0].object != m[I].objectMatches[0].object))
				{
					if (m[selfReferringSpeakerFound].objectMatches.size())
						m[I].objectMatches = m[selfReferringSpeakerFound].objectMatches;
					else
						m[I].objectMatches.push_back(cOM(m[selfReferringSpeakerFound].getObject(), SALIENCE_THRESHOLD));
				}
				// if the beginning of this quote is immediately after the last one, then assume that the other secondary speaker is talking
				else if (audienceFilled = sqr >= 4 && secondaryQuotesResolutions[sqr - 4 + 1] == I - 1)
				{
					m[I].objectMatches = m[secondaryQuotesResolutions[sqr - 4]].audienceObjectMatches;
					m[I].audienceObjectMatches = m[secondaryQuotesResolutions[sqr - 4]].objectMatches;
				}
				// if the beginning of this quote is immediately after the speaker of the last secondary quote +EOS, then copy the last one.
				// 33920-33935 NQS ’ explained the doctor . ‘ nothing serious . you shall be about again in a couple of days . ’
				else if (audienceFilled = sqr >= 4 && lastOpeningSecondaryQuoteIM != wmNULL) // && secondaryQuotesResolutions[sqr-4+2]+1==I-1)
				{
					m[I].objectMatches = m[secondaryQuotesResolutions[sqr - 4]].objectMatches;
					m[I].audienceObjectMatches = m[secondaryQuotesResolutions[sqr - 4]].audienceObjectMatches;
				}
				if (referringAudienceFound >= 0 && (m[referringAudienceFound].objectMatches.empty() || m[I].objectMatches.empty() ||
					m[referringAudienceFound].objectMatches[0].object != m[I].objectMatches[0].object))
				{
					if (m[referringAudienceFound].objectMatches.size())
						m[I].objectMatches = m[referringAudienceFound].objectMatches;
					else
						m[I].objectMatches.push_back(cOM(m[referringAudienceFound].getObject(), SALIENCE_THRESHOLD));
				}
			}
			for (unsigned int J = 0; J < m[I].objectMatches.size(); J++)
			{
				objects[m[I].objectMatches[J].object].locations.push_back(I);
				objects[m[I].objectMatches[J].object].updateFirstLocation(I);
			}
			if (audienceObjectPosition >= 0)
			{
				if ((m[audienceObjectPosition].getObject() >= 0 || m[audienceObjectPosition].getObject() < cObject::eOBJECTS::UNKNOWN_OBJECT) && lastOpeningPrimaryQuoteIM != wmNULL)
					resolveFirstSecondPersonPronoun(audienceObjectPosition,
						lastOpeningPrimaryQuoteIM->flags, lastEmbeddedStoryBegin,
						lastOpeningPrimaryQuoteIM->objectMatches,
						lastOpeningPrimaryQuoteIM->audienceObjectMatches);
				if (m[audienceObjectPosition].principalWherePosition >= 0)
					audienceObjectPosition = m[audienceObjectPosition].principalWherePosition;
				if (m[audienceObjectPosition].objectMatches.size())
					m[I].audienceObjectMatches = m[audienceObjectPosition].objectMatches;
				else if (m[audienceObjectPosition].getObject() >= 0)
					m[I].audienceObjectMatches.push_back(cOM(m[audienceObjectPosition].getObject(), SALIENCE_THRESHOLD));
			}
			// if speakerObject doesn't match lastOpeningPrimaryQuoteIM objectMatches, and there is no resolved hail object, assume person is talking to speaker 
			else if (!audienceFilled && m[I].objectMatches.size())
			{
				int hailFound = -1;
				for (int J = secondaryQuotesResolutions[sqr + 0]; J < secondaryQuotesResolutions[sqr + 1] && hailFound < 0; J++)
					if ((m[J].objectRole & HAIL_ROLE) && (m[J].getObject() >= 0 || m[J].objectMatches.size()))
						hailFound = J;
				// hail cannot match speaker / He[mr] then says : ‘[mr:mr] Please take a seat , Mr[mr] . -- er ? ’ 
				if (hailFound >= 0 && (m[hailFound].objectMatches.empty() || m[I].objectMatches.empty() ||
					m[hailFound].objectMatches[0].object != m[I].objectMatches[0].object))
				{
					if (m[hailFound].objectMatches.size())
						m[I].audienceObjectMatches = m[hailFound].objectMatches;
					else
						m[I].audienceObjectMatches.push_back(cOM(m[hailFound].getObject(), SALIENCE_THRESHOLD));
				}
				else if (lastOpeningPrimaryQuoteIM != wmNULL && lastOpeningPrimaryQuoteIM->objectMatches.size() == 1 &&
					in(lastOpeningPrimaryQuoteIM->objectMatches[0].object, m[I].objectMatches) == m[I].objectMatches.end())
					m[I].audienceObjectMatches = lastOpeningPrimaryQuoteIM->objectMatches;
				// if still not specified, and present speaker is the same as last, then assume the audience is also the same as last
				if (m[I].audienceObjectMatches.empty() && m[I].objectMatches.size() == 1 && lastOpeningSecondaryQuoteIM != wmNULL && lastOpeningSecondaryQuoteIM->objectMatches.size() == 1 &&
					m[I].objectMatches[0].object == lastOpeningSecondaryQuoteIM->objectMatches[0].object)
					m[I].audienceObjectMatches = lastOpeningSecondaryQuoteIM->audienceObjectMatches;
				// if STILL not specified, and speaker known, subtract speaker from embeddedSpeakerGroup and assign to audience
				if (m[I].audienceObjectMatches.empty() && m[I].objectMatches.size()) //  && currentSpeakerGroup>=0
				{
					if (currentEmbeddedSpeakerGroup >= 0 && ((unsigned)currentEmbeddedSpeakerGroup) < speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size() &&
						I >= speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin &&
						I < speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd)
					{
						for (set <int>::iterator s = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.begin(),
							sEnd = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.end(); s != sEnd; s++)
							if (in(*s, m[I].objectMatches) == m[I].objectMatches.end())
								m[I].audienceObjectMatches.push_back(cOM(*s, SALIENCE_THRESHOLD));
					}
					// if STILL not specified, and speaker known, subtract speaker from speakerGroup and assign to audience
					else if (I >= speakerGroups[currentSpeakerGroup].sgBegin && I < speakerGroups[currentSpeakerGroup].sgEnd)
					{
						for (set <int>::iterator s = speakerGroups[currentSpeakerGroup].speakers.begin(), sEnd = speakerGroups[currentSpeakerGroup].speakers.end(); s != sEnd; s++)
							if (in(*s, m[I].objectMatches) == m[I].objectMatches.end())
								m[I].audienceObjectMatches.push_back(cOM(*s, SALIENCE_THRESHOLD));
					}
				}
			}
			for (unsigned int J = 0; J < m[I].audienceObjectMatches.size(); J++)
			{
				objects[m[I].audienceObjectMatches[J].object].locations.push_back(I);
				objects[m[I].audienceObjectMatches[J].object].updateFirstLocation(I);
			}
			wstring tmpstr, tmpstr2;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%d-%d:Secondary speaker resolution %d:%s audience %d:%s.",
					secondaryQuotesResolutions[sqr], secondaryQuotesResolutions[sqr + 1], secondaryQuotesResolutions[sqr + 2],
					objectString(m[I].objectMatches, tmpstr, true).c_str(), secondaryQuotesResolutions[sqr + 3], objectString(m[I].audienceObjectMatches, tmpstr2, true).c_str());
			sqr += 4;
		}
		if (im->word->first == L"“" && !(im->flags & cWordMatch::flagQuotedString))
		{
			if (im->flags & cWordMatch::flagEmbeddedStoryBeginResolveSpeakers)
				lastEmbeddedStoryBegin = I;
			inPrimaryQuote = true;
			lastOpeningPrimaryQuoteIM = im;
			lastOpeningPrimaryQuote = I;
			if (section < sections.size())
			{
				if (im->objectMatches.size() == 1)
				{
					speakersMatched++;
					sections[section].speakersMatched++;
				}
				else
				{
					speakersNotMatched++;
					sections[section].speakersNotMatched++;
				}
				if (im->audienceObjectMatches.size() != 0 && im->audienceObjectMatches.size() < speakerGroups[currentSpeakerGroup].speakers.size())
				{
					counterSpeakersMatched++;
					sections[section].counterSpeakersMatched++;
				}
				else
				{
					counterSpeakersNotMatched++;
					sections[section].counterSpeakersNotMatched++;
				}
			}
		}
		else if (im->word->first == L"”" && !(im->flags & cWordMatch::flagQuotedString))
		{
			lastOpeningSecondaryQuoteIM = lastOpeningPrimaryQuoteIM = wmNULL;
			lastOpeningPrimaryQuote = -1;
			lastQuote = -1;
			inSecondaryQuote = inPrimaryQuote = false;
		}
		else if (im->word->first == L"‘" && !(im->flags & cWordMatch::flagQuotedString))
		{
			lastOpeningSecondaryQuoteIM = im;
			lastOpeningSecondaryQuote = I;
			inSecondaryQuote = true;
			inPrimaryQuote = false;
		}
		else if (im->word->first == L"’" && !(im->flags & cWordMatch::flagQuotedString))
		{
			inSecondaryQuote = false;
			inPrimaryQuote = (lastOpeningPrimaryQuote >= 0);
		}
		for (vector <cOM>::iterator oi = im->objectMatches.begin(), oiEnd = im->objectMatches.end(); oi != oiEnd; oi++)
			followObjectChain(oi->object);
		for (vector <cOM>::iterator oi = im->audienceObjectMatches.begin(), oiEnd = im->audienceObjectMatches.end(); oi != oiEnd; oi++)
			followObjectChain(oi->object);
		int o;
		if ((o = im->getObject()) >= 0)
		{
			if (followObjectChain(o))
			{
				im->originalObject = im->getObject();
				im->setObject(o);
			}
		}
	}
	timelineSegments[currentTimelineSegment].end = m.size();
	for (vector <cSpeakerGroup>::iterator sgi = speakerGroups.begin(), sgiEnd = speakerGroups.end(); sgi != sgiEnd; sgi++)
		for (set <int>::iterator si = sgi->speakers.begin(); si != sgi->speakers.end(); si++)
			if (objects[*si].masterSpeakerIndex < 0)
			{
				vector < vector<cObject>::iterator >::iterator msi;
				bool aliasAlreadyInList = false;
				for (unsigned int a = 0; a < objects[*si].aliases.size() && !aliasAlreadyInList; a++)
					if ((msi = find(masterSpeakerList.begin(), masterSpeakerList.end(), objects.begin() + objects[*si].aliases[a])) != masterSpeakerList.end())
					{
						if (!(aliasAlreadyInList = (*msi)->numDefinitelyIdentifiedAsSpeaker >= objects[objects[*si].aliases[a]].numDefinitelyIdentifiedAsSpeaker))
							masterSpeakerList.erase(msi);
					}
				if (!aliasAlreadyInList)
				{
					masterSpeakerList.push_back(objects.begin() + *si);
					objects[*si].masterSpeakerIndex = 0;
				}
			}
	sort(masterSpeakerList.begin(), masterSpeakerList.end(), masterCompare);
	for (unsigned int ms = 0; ms < masterSpeakerList.size(); ms++)
		masterSpeakerList[ms]->masterSpeakerIndex = ms;
}

