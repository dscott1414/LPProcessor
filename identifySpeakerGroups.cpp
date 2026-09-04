/*
	identifySpeakerGroups.cpp - first-pass conversation-cast: who is in the scene, grouped how.

	Overview:
		Walks the tokenized document once and partitions it into cSpeakerGroup spans
		(continuous stretches of m[] that share a cast of speakers).  A temporary
		group is accumulated across paragraphs; at each paragraph / chapter boundary
		createSpeakerGroup() either extends the previous span or pushes a new one.
		Along the way it records POV, observers, syntactic subgroups ("Bob and Bill"),
		embedded stories (a character narrating other characters), and hail vocatives.
		No quote is given a final speaker/audience pair here - that is resolveSpeakers.

	Pipeline position:
		Stage 7a, immediately after object/pronoun identification.  Called from
		processSource() in main.cpp as identifySpeakerGroups() then resolveSpeakers().
		Reads objects[], localObjects, syntacticRelationGroups; writes speakerGroups,
		tempSpeakerGroup, povInSpeakerGroups, and various cObject speaker counters.

	Key entry points:
		- identifySpeakerGroups() - the single document scan
		- createSpeakerGroup() - decide merge-vs-new at a paragraph / section boundary
		- determinePreviousSubgroup() / determineSpeakerRemoval() - grouping and aging-out
		- distributePOV() - post-pass that fills povSpeakers / observers / dnSpeakers
		- embeddedStory() - mark quotes that are a character telling a story

	Key data structures / globals:
		- speakerGroups / tempSpeakerGroup - finished vs in-progress casts (cSource members)
		- speakerAges / speakerSections - age of each speaker inside the current span
		- povInSpeakerGroups / definitelyIdentifiedAsSpeakerInSpeakerGroups /
		  metaNameOthersInSpeakerGroups - position lists later dropped into each group
		- nextNarrationSubjects / nextISNarrationSubjects - unquoted subjects parked
		  until the next group is closed, then used as audience candidates
		- cOM copy() overloads - binary (de)serialization of object-match records
		  used by cSpeakerGroup's cache I/O

	Dependencies:
		cSource members (m, objects, localObjects, sections, syntacticRelationGroups),
		VerbNet (associatePossessions), WordNet synonyms (associateNyms), Words lexicon.
		Windows-only (windows.h / Winhttp.h) via the rest of the build.

	Notes / gotchas:
		- Object 0 is Narrator, object 1 is Audience; o <= 1 is treated as unmergable.
		- sgEnd of -2 / -3 is legal on embedded groups (CURRENT_SUBSET_SG / open span).
		- unMergable() returns true when the candidate is NOT already in the set
		  (and optionally inserts it).  The name is easy to read backwards.
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
#include "time.h"
#include "vcXML.h"
#include <wn.h>
#include "profile.h"

extern set<int>::iterator sNULL;

// Deserialize one cOM (object index + salience) from buf at 'where' and advance
// where by sizeof(cOM).  Refuses (FATALs, which exits the process) rather than
// reading past 'limit'.
bool copy(cOM& num, char* buf, int& where, int limit)
{
	DLFS
	if (where + (int)sizeof(cOM) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached (3)!", limit);
	num = *((cOM*)(buf + where));
	where += sizeof(num);
	return true;
}

// Deserialize a counted vector<cOM> from buf.  Returns false if the count itself
// cannot be read inside 'limit'.  Each element is then read via the bounds-checked
// copy(cOM&) above, so a corrupted count still cannot walk past the buffer.
bool copy(vector <cOM>& s, char* buf, int& where, int limit)
{
	DLFS
	int count;
	if (!copy(count, buf, where, limit)) return false;
	for (int I = 0; I < count; I++)
	{
		cOM i;
		if (!copy(i, buf, where, limit))
			return false;
		s.push_back(i);
	}
	return true;
}

// Serialize one cOM into buf at 'where'.  The overflow check (where > limit)
// runs BEFORE the write, so LOG_FATAL_ERROR (which exits the process) fires
// before any out-of-bounds store.  Returns true on success.
bool copy(void* buf, cOM num, int& where, int limit)
{
	DLFS
	if (where + sizeof(num) > limit)
		lplog(LOG_FATAL_ERROR, u"Maximum copy limit of %d bytes reached (3)!", limit);
	* ((cOM*)(((char*)buf) + where)) = num;
	where += sizeof(num);
	return true;
}

// Serialize a vector<cOM> as count + packed elements.  Returns false if the
// count write fails; element writes abort on their own limit check.
bool copy(void* buf, vector <cOM>& s, int& where, int limit)
{
	DLFS
		int count = s.size();
	if (!copy(buf, count, where, limit)) return false;
	for (vector<cOM>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit))
			return false;
	return true;
}

// Default-construct an empty speaker group: zero span, section -1, no speakers,
// speakersAreNeverGroupedTogether true (until determinePreviousSubgroup proves
// otherwise), tlTransition false.
cSource::cSpeakerGroup::cSpeakerGroup(void)
{
	LFS
		sgBegin = sgEnd = 0;
	section = -1;
	previousSubsetSpeakerGroup = -1;
	saveNonNameObject = -1;
	conversationalQuotes = 0;
	speakersAreNeverGroupedTogether = true;
	tlTransition = false;
}

// Deserialize a speaker group (and recursively its embeddedSpeakerGroups and
// groups) from the SourceCache buffer.  Sets error and returns early on any
// short read.  The two bools are packed in a flags qword (bit 0 = never-grouped,
// bit 1 = tlTransition).  error is also set if where overshoots limit at the end.
cSource::cSpeakerGroup::cSpeakerGroup(char* buffer, int& where, unsigned int limit, bool& error)
{
	LFS
		if (error = !::copy(sgBegin, buffer, where, limit)) return;
	if (error = !::copy(sgEnd, buffer, where, limit)) return;
	if (error = !::copy(section, buffer, where, limit)) return;
	if (error = !::copy(previousSubsetSpeakerGroup, buffer, where, limit)) return;
	if (error = !::copy(saveNonNameObject, buffer, where, limit)) return;
	if (error = !::copy(conversationalQuotes, buffer, where, limit)) return;
	if (error = !::copy(speakers, buffer, where, limit)) return;
	if (error = !::copy(fromNextSpeakerGroup, buffer, where, limit)) return;
	if (error = !::copy(replacedSpeakers, buffer, where, limit)) return;
	if (error = !::copy(singularSpeakers, buffer, where, limit)) return;
	if (error = !::copy(groupedSpeakers, buffer, where, limit)) return;
	if (error = !::copy(povSpeakers, buffer, where, limit)) return;
	if (error = !::copy(dnSpeakers, buffer, where, limit)) return;
	if (error = !::copy(metaNameOthers, buffer, where, limit)) return;
	if (error = !::copy(observers, buffer, where, limit)) return;
	unsigned int count;
	if (error = !::copy(count, buffer, where, limit)) return;
	for (unsigned int I = 0; I < count && !error; I++)
		embeddedSpeakerGroups.push_back(cSpeakerGroup(buffer, where, limit, error));
	if (error) return;
	if (error = !::copy(count, buffer, where, limit)) return;
	for (unsigned int I = 0; I < count && !error; I++)
		groups.push_back(cSpeakerGroup::cGroup(buffer, where, limit, error));
	if (error) return;
	int64_t flags;
	if (error = !::copy(flags, buffer, where, limit)) return;
	speakersAreNeverGroupedTogether = (flags & 1) ? true : false;
	tlTransition = (flags & 2) ? true : false;
	error = where > (signed)limit;
}

/*

cSource::cSpeakerGroup::cSpeakerGroup(const cSpeakerGroup& obj) {
	sgBegin = obj.sgBegin;
	sgEnd = obj.sgEnd;
	section = obj.section;
	previousSubsetSpeakerGroup = obj.previousSubsetSpeakerGroup;
	saveNonNameObject = obj.saveNonNameObject;
	conversationalQuotes = obj.conversationalQuotes;
	speakers = obj.speakers;
	fromNextSpeakerGroup = obj.fromNextSpeakerGroup;
	replacedSpeakers = obj.replacedSpeakers;
	singularSpeakers = obj.singularSpeakers;
	groupedSpeakers = obj.groupedSpeakers;
	povSpeakers = obj.povSpeakers;
	dnSpeakers = obj.dnSpeakers;
	metaNameOthers = obj.metaNameOthers;
	observers = obj.observers;
	embeddedSpeakerGroups = obj.embeddedSpeakerGroups;
	groups = obj.groups;
	speakersAreNeverGroupedTogether = obj.speakersAreNeverGroupedTogether;
	tlTransition = obj.tlTransition;
}
*/
bool cSource::cSpeakerGroup::copy(void* buffer, int& where, int limit)
{
	LFS
		if (!::copy(buffer, sgBegin, where, limit)) return false;
	if (!::copy(buffer, sgEnd, where, limit)) return false;
	if (!::copy(buffer, section, where, limit)) return false;
	if (!::copy(buffer, previousSubsetSpeakerGroup, where, limit)) return false;
	if (!::copy(buffer, saveNonNameObject, where, limit)) return false;
	if (!::copy(buffer, conversationalQuotes, where, limit)) return false;
	if (!::copy(buffer, speakers, where, limit)) return false;
	if (!::copy(buffer, fromNextSpeakerGroup, where, limit)) return false;
	if (!::copy(buffer, replacedSpeakers, where, limit)) return false;
	if (!::copy(buffer, singularSpeakers, where, limit)) return false;
	if (!::copy(buffer, groupedSpeakers, where, limit)) return false;
	if (!::copy(buffer, povSpeakers, where, limit)) return false;
	if (!::copy(buffer, dnSpeakers, where, limit)) return false;
	if (!::copy(buffer, metaNameOthers, where, limit)) return false;
	if (!::copy(buffer, observers, where, limit)) return false;
	if (!::copy(buffer, (int)embeddedSpeakerGroups.size(), where, limit)) return false;
	for (vector <cSource::cSpeakerGroup>::iterator esgi = embeddedSpeakerGroups.begin(), esgEnd = embeddedSpeakerGroups.end(); esgi != esgEnd; esgi++)
		if (!esgi->copy(buffer, where, limit)) return false;
	if (!::copy(buffer, (int)groups.size(), where, limit)) return false;
	for (unsigned int I = 0; I < groups.size(); I++)
		if (!groups[I].copy(buffer, where, limit)) return false;
	int64_t flags = (speakersAreNeverGroupedTogether) ? 1 : 0;
	flags |= (tlTransition) ? 2 : 0;
	if (!::copy(buffer, flags, where, limit)) return false;
	return true;
}

// Age one local-focus entry and, if it is a nameless entity that has never been
// identified as a speaker, has < 2 encounters and is older than MAX_AGE, erase
// it from localObjects (lfi is then the next entry).  Otherwise advances lfi.
// Used when we want recency decay without touching speaker-identification stats.
void cSource::ageSpeakerWithoutSpeakerInfo(int where, bool inPrimaryQuote, bool inSecondaryQuote, vector <cLocalFocus>::iterator& lfi, int amount)
{
	LFS
		lfi->increaseAge(inPrimaryQuote, inSecondaryQuote, amount);
	if (objects[lfi->om.object].objectClass != NAME_OBJECT_CLASS &&
		lfi->getTotalAge() > MAX_AGE &&
		lfi->numIdentifiedAsSpeaker == 0 &&
		lfi->numEncounters < 2)
	{
		lpwstring tmpstr;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d:%02d     object %s eliminated from local objects",
				where, section, objectString(lfi->om.object, tmpstr, true).c_str());
		lfi = localObjects.erase(lfi);
	}
	else
		lfi++;
}

// If object o confidently matches another speaker in 'speakers', replace o with
// that speaker (replaceObjectWithObject) and rewrite o in-place.  Restarts the
// scan after each merge so a chain of aliases collapses to one survivor.
void cSource::mergeName(int where, int& o, set <int>& speakers)
{
	LFS
		vector <cObject>::iterator object = objects.begin() + o;
	for (set<int>::iterator i = speakers.begin(); i != speakers.end(); i++)
		if (o != *i && object->confidentMatch(objects[*i], debugTrace))
		{
			replaceObjectWithObject(where, object, *i, u"mergeName");
			speakers.erase(o);
			o = *i;
			object = objects.begin() + *i;
			i = speakers.begin();
		}
}

// True if object o can merge with at least one speaker by gender (including
// neuter).  A hit on an unresolvable, physically-present speaker is accepted
// only if that speaker first appeared before o.  uniquelyMergable is set false
// when more than one speaker matches; mergedObject is the last (or only) hit,
// or sNULL if none.
bool cSource::mergableBySex(int o, set <int>& speakers, bool& uniquelyMergable, set<int>::iterator& mergedObject)
{
	LFS
		bool genderUncertainMatch, physicallyEvaluated;
	mergedObject = sNULL;
	uniquelyMergable = true;
	// object must either be equal, or match sex and the object being matched against must either not be unresolvable or must occur before the object being matched
	for (set<int>::iterator i = speakers.begin(), iEnd = speakers.end(); i != iEnd; i++)
		if (o == *i || (objects[o].matchGenderIncludingNeuter(objects[*i], genderUncertainMatch) &&
			(!(unResolvablePosition(objects[*i].begin) && physicallyPresentPosition(objects[*i].originalLocation, objects[*i].begin, physicallyEvaluated, false)) ||
				objects[*i].firstLocation < objects[o].firstLocation)))
		{
			if (mergedObject != sNULL) uniquelyMergable = false;
			mergedObject = i;
		}
	return mergedObject != sNULL;
}

// set<> overload.  Returns true if o cannot be identified with any member of
// 'speakers' (and, if insertObject, inserts o then still returns true with
// save == end()).  Returns false when a merge candidate was found; save is
// that iterator and uniquelyMergable says whether it was the only one.
// o<=1 (Narrator/Audience) is always unmergable.  Unresolvable names that
// failed implicit resolution are not merged with prior casts; body objects
// may substitute for their owner.  crossedSection restricts merges to
// speakers already seen in the new chapter.
bool cSource::unMergable(int where, int o, set <int>& speakers, bool& uniquelyMergable, bool insertObject, bool crossedSection, bool allowBothToBeSpeakers, bool checkUnmergableSpeaker, set <int>::iterator& save)
{
	LFS
		if (o <= 1) return true;
	followObjectChain(o);
	int at = m[objects[o].begin].principalWherePosition;
	if (at < 0) at = m[objects[o].begin].principalWhereAdjectivalPosition;
	// if "The chauffeur" attempts to merge with the previous speakerGroup, Thomas and Boris, that should fail,
	//   because "The chauffeur" did not match either of those when it was first matched (so Thomas and Boris are not chauffeurs)
	//   BUT if Annette attempts to merge with "a girl" of the previous speakerGroup, that is OK, because Annette as a name will
	//     not match a girl because it is not allowed (downclass - a gendered noun doesn't match a name, because the name class is primary)
	bool implicitlyUnresolvable = at >= 0 && (m[at].objectRole & UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE) != 0 && objects[o].objectClass == NAME_OBJECT_CLASS;
	// if meta group objects aren't matched, then they will automatically be inserted
	if (where >= 0 && objects[o].objectClass == META_GROUP_OBJECT_CLASS && m[where].objectMatches.empty())
		save = speakers.end();
	else if (!implicitlyUnresolvable && unResolvablePosition(objects[o].begin)) // && isPhysicallyPresent - o@where already went through this check
	{                                                //  but o by itself may not because o begin may not have been
		// only resolve with body objects              // physically present
		uniquelyMergable = true;
		save = speakers.find(o);
		if (save == speakers.end() && objects[o].getOwnerWhere() >= 0 && objects[o].objectClass == BODY_OBJECT_CLASS)
		{
			save = speakers.find(m[objects[o].getOwnerWhere()].getObject());
			if (save != speakers.end())
			{
				speakers.erase(save);
				pair< set<int>::iterator, bool > pr = speakers.insert(o);
				save = pr.first;
			}
		}
		// check for aliases
		for (set<int>::iterator i = speakers.begin(), iEnd = speakers.end(); i != iEnd; i++)
			if (matchAliases(where, o, *i))
			{
				save = i;
				break;
			}
	}
	else
	{
		save = speakers.end();
		uniquelyMergable = true;
		int ownerObject = -1;
		if (objects[o].getOwnerWhere() >= 0 && m[objects[o].getOwnerWhere()].objectMatches.size() == 1)
			ownerObject = m[objects[o].getOwnerWhere()].objectMatches[0].object;
		if (objects[o].getOwnerWhere() >= 0 && m[objects[o].getOwnerWhere()].objectMatches.empty() && m[objects[o].getOwnerWhere()].getObject() >= 0)
			ownerObject = m[objects[o].getOwnerWhere()].getObject();
		for (set<int>::iterator i = speakers.begin(), iEnd = speakers.end(); i != iEnd; i++)
		{
			int speakerOwnerObject = -1;
			if (objects[*i].getOwnerWhere() >= 0 && m[objects[*i].getOwnerWhere()].objectMatches.size() == 1)
				speakerOwnerObject = m[objects[*i].getOwnerWhere()].objectMatches[0].object;
			if (objects[*i].getOwnerWhere() >= 0 && m[objects[*i].getOwnerWhere()].objectMatches.empty() && m[objects[*i].getOwnerWhere()].getObject() >= 0)
				speakerOwnerObject = m[objects[*i].getOwnerWhere()].getObject();
			// if the current speaker group has crossed over into a new section, only merge with
			// those objects which have already appeared in the new section.
			if ((!crossedSection || objects[*i].numEncountersInSection || objects[*i].numIdentifiedAsSpeakerInSection) &&
				speakerOwnerObject != o && ownerObject != *i &&
				(o == *i || potentiallyMergable(where, objects.begin() + o, objects.begin() + *i, allowBothToBeSpeakers, checkUnmergableSpeaker)))
			{
				if (save != iEnd) uniquelyMergable = false;
				save = i;
			}
		}
	}
	if (save == speakers.end() && insertObject)
	{
		speakers.insert(o);
		save = speakers.end();
	}
	return save == speakers.end();
}

// vector<int> overload of unMergable - same contract, used for narration-
// subject lists (nextNarrationSubjects / nextISNarrationSubjects) which are
// ordered rather than unique.  Body-object owner substitution appends o
// instead of inserting into a set.
bool cSource::unMergable(int where, int o, vector <int>& speakers, bool& uniquelyMergable, bool insertObject, bool crossedSection, bool allowBothToBeSpeakers, bool checkUnmergableSpeaker, vector <int>::iterator& save)
{
	LFS
		if (o <= 1) return true;
	followObjectChain(o);
	int at = m[objects[o].begin].principalWherePosition;
	if (at < 0) at = m[objects[o].begin].principalWhereAdjectivalPosition;
	bool implicitlyUnresolvable = at >= 0 && (m[at].objectRole & UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE) != 0 && objects[o].objectClass == NAME_OBJECT_CLASS;
	// isPhysicallyPresent - o@where already went through this check
	// but o by itself may not because o@begin may not have been
	// physically present
	bool positionUnresolvable = unResolvablePosition(m[where].beginObjectPosition);
	bool matchPositionUnresolvable = !implicitlyUnresolvable && unResolvablePosition(objects[o].begin);
	if (positionUnresolvable || matchPositionUnresolvable)
	{
		// only resolve with body objects
		uniquelyMergable = true;
		save = find(speakers.begin(), speakers.end(), o);
		if (save == speakers.end() && objects[o].getOwnerWhere() >= 0 && objects[o].objectClass == BODY_OBJECT_CLASS)
		{
			save = find(speakers.begin(), speakers.end(), m[objects[o].getOwnerWhere()].getObject());
			if (save != speakers.end())
			{
				speakers.erase(save);
				speakers.push_back(o);
				save = speakers.begin() + speakers.size() - 1;
			}
		}
	}
	else
	{
		save = speakers.end();
		uniquelyMergable = true;
		for (vector <int>::iterator i = speakers.begin(), iEnd = speakers.end(); i != iEnd; i++)
		{
			if ((!crossedSection || objects[*i].numEncountersInSection || objects[*i].numIdentifiedAsSpeakerInSection) &&
				(o == *i || potentiallyMergable(where, objects.begin() + o, objects.begin() + *i, allowBothToBeSpeakers, checkUnmergableSpeaker)))
			{
				if (save != iEnd) uniquelyMergable = false;
				save = i;
			}
		}
	}
	if (save == speakers.end() && insertObject)
	{
		speakers.push_back(o);
		save = speakers.end();
	}
	return save == speakers.end();
}

// Walk speaker groups whose sgBegin >= begin backwards from the last group
// and, wherever fromObject is a speaker, swap it for toObject.  Also rewrites
// m[].objectMatches / locations in [begin,end) so later resolveSpeakers will
// not reject the replacement as an unknown.  replacedSpeakers records
// (fromObject, toObject) as cOM(object, salienceFactor=toObject).
void cSource::replaceSpeaker(int begin, int end, int fromObject, int toObject)
{
	LFS
		lpwstring tmpstr, tmpstr2, tmpstr3;
	vector <cSpeakerGroup>::iterator lastSG = (speakerGroups.size()) ? speakerGroups.begin() + speakerGroups.size() - 1 : speakerGroups.end();
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"%06d-%06d:%02d replaced speaker %s with %s in %s", begin, end, section,
			objectString(fromObject, tmpstr, true).c_str(), objectString(toObject, tmpstr2, true).c_str(), toText(*lastSG, tmpstr3));
	if (objects[fromObject].originalLocation >= begin && objects[fromObject].originalLocation < end)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d-%06d:RS %02d AT %d pushed %s (1)", begin, end, section, objects[fromObject].originalLocation, objectString(toObject, tmpstr2, true).c_str());
		m[objects[fromObject].originalLocation].objectMatches.clear();
		m[objects[fromObject].originalLocation].objectMatches.push_back(cOM(toObject, SALIENCE_THRESHOLD));
		objects[toObject].locations.push_back(objects[fromObject].originalLocation);
		objects[toObject].updateFirstLocation(objects[fromObject].originalLocation);
		m[objects[fromObject].originalLocation].flags |= cWordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup | cWordMatch::flagObjectResolved;
	}
	for (vector <cObject::cLocation>::iterator li = objects[fromObject].locations.begin(), liEnd = objects[fromObject].locations.end(); li != liEnd; li++)
		if (li->at >= begin && li->at < end && m[li->at].getObject() == fromObject && // include all other chances for matching this unresolvable object
			!(m[li->at].flags & cWordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup)) // but not where this object is matched against others!
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:RS %02d AT %d pushed %s (2)", begin, end, section, li->at, objectString(toObject, tmpstr2, true).c_str());
			m[li->at].objectMatches.clear();
			m[li->at].objectMatches.push_back(cOM(toObject, SALIENCE_THRESHOLD));
			objects[toObject].locations.push_back(li->at);
			objects[toObject].updateFirstLocation(li->at);
			m[li->at].flags |= cWordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup | cWordMatch::flagObjectResolved;
		}
	int o = fromObject;
	for (vector <cSpeakerGroup>::iterator sg = lastSG; sg->sgBegin >= begin; sg--)
	{
		if (sg->speakers.erase(o))
		{
			// this remembers speakers which are replaced backwards
			// this is used to also prevent these speakers from being rejected in the next stage (resolveSpeakers)
			// if this wasn't used, these speakers would be rejected before being evaluated.
			sg->replacedSpeakers.push_back(cOM(o, toObject)); // for unresolvable speakers which are only replaced in section
			sg->speakers.insert(toObject);
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d replaced -> %s", begin, end, section, toText(*sg, tmpstr3));
		}
		else if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d-%06d:%02d %s NOT FOUND", begin, end, section, toText(*sg, tmpstr3));
		if (sg == speakerGroups.begin()) break;
	}
}

// for each speaker in lastSG:
//   if speaker is not in tempSpeakerGroup (the next speaker group to be added)
//   and has a speakerAge>2, add to oldSpeakers.
// if oldSpeakers is empty, return.
// until oldSpeakers is empty:
//   find oldest speaker S (speaker with largest speakerAge) in oldSpeakers
//   find last speakerSection in speakerSections by (age of S -1) - the speaker section AFTER the last encounter of the speaker
//	 copy lastSG to speakerGroups (newSG)
//   set lastSG.end=speakerSection.
//   newSG.begin=speakerSection.
//   remove S from newSG.
//   make lastSG point to newSG.
// When the last speaker group has 3+ speakers and some of them have aged
// out of tempSpeakerGroup (speakerAge > 2), split the last group at the
// speakerSection after that speaker's last encounter, dropping the aged
// speaker from the newer half.  Refuses the split if it would leave a
// one-speaker group that still contains a quote.  See the block comment
// immediately above the function for the step-by-step.
void cSource::determineSpeakerRemoval(int where)
{
	LFS
		if (speakerGroups.empty()) return;
	vector <cSpeakerGroup>::iterator lastSG = speakerGroups.begin() + speakerGroups.size() - 1;
	if (lastSG->speakers.size() < 3) return;
	lpwstring tmpstr;
	vector <int> oldSpeakers;
	for (set <int>::iterator s = lastSG->speakers.begin(); s != lastSG->speakers.end(); s++)
		if (speakerAges[*s] > 2 && tempSpeakerGroup.speakers.find(*s) == tempSpeakerGroup.speakers.end() && objects[*s].objectClass != META_GROUP_OBJECT_CLASS)
			oldSpeakers.push_back(*s);
	if (oldSpeakers.empty())
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%d:Ages of %s are all < 2.", where, objectString(tempSpeakerGroup.speakers, tmpstr).c_str());
		return;
	}
	while (oldSpeakers.size())
	{
		if (lastSG->speakers.size() < 3) return;
		int oldestAge = -1, oldestSpeaker = -1;
		for (vector <int>::iterator s = oldSpeakers.begin(); s != oldSpeakers.end(); s++)
			if (speakerAges[*s] > oldestAge)
				oldestAge = speakerAges[oldestSpeaker = *s];
		if (oldestAge > (int)speakerSections.size())
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%d:Age of %s is illegal (%d>=%d)", where, objectString(oldestSpeaker, tmpstr, true).c_str(), oldestAge, speakerSections.size());
			break;
		}
		if (lastSG->sgEnd == speakerSections[speakerSections.size() - oldestAge])
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%d:Oldest age of %s (%d) reached the end of the speaker group (%d)", where, objectString(oldestSpeaker, tmpstr, true).c_str(), oldestAge, lastSG->sgEnd);
			break;
		}
		// sanity check - is there a conversation in the speakergroup with only one speaker?
		int speakersToErase = 0;
		for (set <int>::iterator s = lastSG->speakers.begin(); s != lastSG->speakers.end(); s++)
			if (speakerAges[*s] >= oldestAge)
				speakersToErase++;
		if (lastSG->speakers.size() - speakersToErase <= 1)
		{
			// is there a quote from speakerSections[speakerSections.size()-oldestAge] to the end of lastSG?
			for (int I = speakerSections[speakerSections.size() - oldestAge]; I < lastSG->sgEnd; I++)
				if (m[I].objectRole & IN_PRIMARY_QUOTE_ROLE)
				{
					if (debugTrace.traceSpeakerResolution)
					{
						lplog(LOG_SG, u"%d:speaker removal from %s leading to only one speaker in a group %d-%d with a quote is rejected.", where,
							toText(*lastSG, tmpstr), lastSG->sgBegin, speakerSections[speakerSections.size() - oldestAge]);
						lplog(LOG_SG, u"%d:%s", where, objectString(oldSpeakers, tmpstr).c_str());
					}
					return;
				}
		}
		// end sanity check
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d-%06d:%02d   %s added (speakerRemoval)", lastSG->sgBegin, lastSG->sgEnd, section, toText(*lastSG, tmpstr));
		speakerGroups.push_back(*lastSG);
		lastSG = speakerGroups.begin() + speakerGroups.size() - 2;
		vector <cSpeakerGroup>::iterator newSG = speakerGroups.begin() + speakerGroups.size() - 1;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"section demarcation=%d from age %d.", speakerSections[speakerSections.size() - oldestAge], oldestAge);
		newSG->sgBegin = lastSG->sgEnd = speakerSections[speakerSections.size() - oldestAge];
		// if the new speaker group also contains meta-group class speakers that are older (they aren't in the new speaker group at all)
		for (set <int>::iterator s = newSG->speakers.begin(); s != newSG->speakers.end(); )
			if (speakerAges[*s] >= oldestAge)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%d:%s removed (olderAge=%d).", where, objectString(*s, tmpstr, true).c_str(), speakerAges[*s]);
				newSG->speakers.erase(s++);
			}
			else
				s++;
		for (vector <int>::iterator s = oldSpeakers.begin(); s != oldSpeakers.end(); )
			if (speakerAges[*s] == oldestAge)
				s = oldSpeakers.erase(s);
			else
				s++;
		// distribute embedded speaker groups
		for (int esg = 0; esg < (signed)lastSG->embeddedSpeakerGroups.size(); )
			if (lastSG->embeddedSpeakerGroups[esg].sgEnd <= 0 || lastSG->embeddedSpeakerGroups[esg].sgBegin > lastSG->sgEnd)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%06d-%06d:%02d   embedded erased (lastSG) %s", lastSG->embeddedSpeakerGroups[esg].sgBegin, lastSG->embeddedSpeakerGroups[esg].sgEnd, section, toText(lastSG->embeddedSpeakerGroups[esg], tmpstr));
				lastSG->embeddedSpeakerGroups.erase(lastSG->embeddedSpeakerGroups.begin() + esg);
			}
			else
				esg++;
		// distribute embedded speaker groups
		for (int esg = 0; esg < (signed)newSG->embeddedSpeakerGroups.size(); )
			if (newSG->embeddedSpeakerGroups[esg].sgBegin < newSG->sgBegin)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%06d-%06d:%02d   embedded erased (newSG) %s", newSG->embeddedSpeakerGroups[esg].sgBegin, newSG->embeddedSpeakerGroups[esg].sgEnd, section, toText(newSG->embeddedSpeakerGroups[esg], tmpstr));
				newSG->embeddedSpeakerGroups.erase(newSG->embeddedSpeakerGroups.begin() + esg);
			}
			else
				esg++;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d-%06d:%02d   %s added", lastSG->sgBegin, lastSG->sgEnd, section, toText(*lastSG, tmpstr));
		determinePreviousSubgroup(where, speakerGroups.size() - 2, &speakerGroups[speakerGroups.size() - 2]);
		lastSG = newSG;
	}
}

// resultSet = bigSet \ subtractSet.  Does not clear resultSet first.
void cSource::subtract(set <int>& bigSet, set <int>& subtractSet, set <int>& resultSet)
{
	LFS
		for (set <int>::iterator bi = bigSet.begin(), biEnd = bigSet.end(); bi != biEnd; bi++)
			if (subtractSet.find(*bi) == subtractSet.end())
				resultSet.insert(*bi);
}

// In-place: drop every cOM in bigSet whose object also appears in subtractSet.
void cSource::subtract(vector <cOM>& bigSet, vector <cOM>& subtractSet)
{
	LFS
		vector <cOM> resultSet;
	for (vector <cOM>::iterator bi = bigSet.begin(), biEnd = bigSet.end(); bi != biEnd; bi++)
		if (in(bi->object, subtractSet) == subtractSet.end())
			resultSet.push_back(*bi);
	bigSet = resultSet;
}

// In-place: drop every cOM in bigSet whose object is in the int vector.
void cSource::subtract(vector <cOM>& bigSet, vector <int>& subtractSet)
{
	LFS
		vector <cOM> resultSet;
	for (vector <cOM>::iterator bi = bigSet.begin(), biEnd = bigSet.end(); bi != biEnd; bi++)
		if (find(subtractSet.begin(), subtractSet.end(), bi->object) == subtractSet.end())
			resultSet.push_back(*bi);
	bigSet = resultSet;
}

// In-place: drop every cOM in bigSet whose object is in the int set.
void cSource::subtract(vector <cOM>& bigSet, set <int>& subtractSet)
{
	LFS
		vector <cOM> resultSet;
	for (vector <cOM>::iterator bi = bigSet.begin(), biEnd = bigSet.end(); bi != biEnd; bi++)
		if (subtractSet.find(bi->object) == subtractSet.end())
			resultSet.push_back(*bi);
	bigSet = resultSet;
}

// Erase the first cOM whose object == o, if any.
void cSource::subtract(int o, vector <cOM>& objectMatches)
{
	LFS
		vector <cOM>::iterator w = in(o, objectMatches);
	if (w != objectMatches.end())
		objectMatches.erase(w);
}

// Walk speakerGroups backwards from lastSG-1 and return the first index that
// still lists o as a speaker, or -1 if none.
int cSource::getLastSpeakerGroup(int o, int lastSG)
{
	LFS
		lastSG--;
	// indexed rather than begin()+lastSG: when the caller passes lastSG==0 (a legal
	// "no prior groups" case) that would form an out-of-range iterator (begin()-1)
	// before the loop condition is ever checked - UB, and an assert under MSVC's
	// debug-mode checked iterators.
	for (; lastSG >= 0; lastSG--)
		if (speakerGroups[lastSG].speakers.find(o) != speakerGroups[lastSG].speakers.end())
			return lastSG;
	return -1;
}

// If any syntactic cGroup is a proper subset of sg.speakers, promote those
// members to groupedSpeakers and put the remainder in singularSpeakers.
// First such group wins.
void cSource::determineSubgroupFromGroups(cSpeakerGroup& sg)
{
	LFS
		lpwstring tmpstr;
	for (vector < cSpeakerGroup::cGroup >::iterator gi = sg.groups.begin(), giEnd = sg.groups.end(); gi != giEnd; gi++)
	{
		bool allIn, oneIn;
		// if group only contains speakers
		if (gi->objects.size() >= sg.speakers.size())
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   groupSpeakers %s rejected (group==all speakers)", sg.sgBegin, sg.sgEnd, section, objectString(*gi, tmpstr).c_str());
		}
		else if (intersect(gi->objects, sg.speakers, allIn, oneIn) && allIn)
		{
			for (vector <int>::iterator si = gi->objects.begin(), siEnd = gi->objects.end(); si != siEnd; si++)
				sg.groupedSpeakers.insert(*si);
			sg.singularSpeakers.clear();
			subtract(sg.speakers, sg.groupedSpeakers, sg.singularSpeakers);
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   groupSpeakers %s added (2)", sg.sgBegin, sg.sgEnd, section, objectString(*gi, tmpstr).c_str());
			break;
		}
	}
}

// Recover lastSG's "they" subgroup by looking backwards through earlier
// speaker groups: a previous group's speakers (or groupedSpeakers) that are
// entirely contained in lastSG become lastSG.groupedSpeakers, and
// previousSubsetSpeakerGroup is set to that index (or CURRENT_SUBSET_SG = -2
// when the leftover members of lastSG themselves form the subgroup).
// Also sets speakersAreNeverGroupedTogether and may override groupedSpeakers
// with a preferred MPLURAL_ROLE group.  whichSG is lastSG's index.
void cSource::determinePreviousSubgroup(int where, int whichSG, cSpeakerGroup* lastSG)
{
	LFS
		if (whichSG < 0) return;
	vector <int> preferredMPluralGroup;
	bool headerPrinted = !debugTrace.traceSpeakerResolution, possibleCurrentSubgroup = false, allIn, oneIn;
	if (lastSG->speakers.size() >= 2)
	{
		lpwstring tmpstr, tmpstr2, tmpstr3;
		set <int> speakers = lastSG->speakers;
		// if there are any mplural grouped entities, use them first.
		for (vector < cSpeakerGroup::cGroup >::iterator gi = lastSG->groups.begin(), giEnd = lastSG->groups.end(); gi != giEnd; gi++)
		{
			bool groupAllIn, groupOneIn;
			if ((m[gi->where].objectRole & MPLURAL_ROLE) && intersect(gi->objects, lastSG->speakers, groupAllIn, groupOneIn) && groupAllIn)
			{
				if (preferredMPluralGroup != gi->objects)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG, u"SGP:   %d:Subgroup [FROM MPLURAL group] is %s.", where, objectString(gi->objects, tmpstr2).c_str());
					preferredMPluralGroup = gi->objects;
				}
			}
		}
		// search all previous speaker groups in reverse order
		for (int sg = whichSG - 1; sg >= 0 && speakers.size() >= 2; sg--)
		{
			vector <cSpeakerGroup>::iterator beforeLastSG = speakerGroups.begin() + sg;
			if (beforeLastSG->speakers.size() > lastSG->speakers.size() &&
				(!beforeLastSG->groupedSpeakers.size() || beforeLastSG->groupedSpeakers.size() > lastSG->speakers.size()))
				continue;
			int numFound = 0, numSubgroupFound = 0;
			// if there is no conversation in the speakerGroup, there is much less of a chance that the entities should be considered a "group"
			// They[streets,boris,tommy] reached at length[length] a small dilapidated square .
			// lastSG must contain all members of beforeLastSG
			for (set <int>::iterator blsi = beforeLastSG->speakers.begin(), blsiEnd = beforeLastSG->speakers.end(); blsi != blsiEnd; blsi++)
				if (speakers.find(*blsi) != speakers.end())
					numFound++;
			for (set <int>::iterator blsi = beforeLastSG->groupedSpeakers.begin(), blsiEnd = beforeLastSG->groupedSpeakers.end(); blsi != blsiEnd; blsi++)
				if (speakers.find(*blsi) != speakers.end())
					numSubgroupFound++;
			if (numSubgroupFound > 1 && numSubgroupFound == beforeLastSG->groupedSpeakers.size())
			{
				lastSG->previousSubsetSpeakerGroup = beforeLastSG->previousSubsetSpeakerGroup;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%06d:SGP[1]:   Subgroup is %s->%s.", where, objectString(lastSG->groupedSpeakers, tmpstr2).c_str(), objectString(beforeLastSG->groupedSpeakers, tmpstr3).c_str());
				lastSG->groupedSpeakers = beforeLastSG->groupedSpeakers;
				lastSG->singularSpeakers.clear();
				subtract(lastSG->speakers, lastSG->groupedSpeakers, lastSG->singularSpeakers);
				if (!headerPrinted)
				{
					lplog(LOG_SG, u"SGP[1]:------ %s -------", toText(*lastSG, tmpstr));
					headerPrinted = true;
				}
				break;
			}
			// now mark all members of lastSG NOT in beforeLastSG
			else if (numFound > 1 && (numFound == beforeLastSG->speakers.size()) && lastSG->speakers.size() > beforeLastSG->speakers.size() && !beforeLastSG->speakersAreNeverGroupedTogether &&
				(lastSG->povSpeakers.empty() || lastSG->povSpeakers == beforeLastSG->speakers || !intersect(lastSG->povSpeakers, beforeLastSG->speakers, allIn, oneIn)))
			{
				lastSG->previousSubsetSpeakerGroup = sg;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%06d:SGP[2]:   Subgroup is %s->%s.", where, objectString(lastSG->groupedSpeakers, tmpstr2).c_str(), objectString(beforeLastSG->groupedSpeakers, tmpstr3).c_str());
				lastSG->groupedSpeakers = beforeLastSG->speakers;
				lastSG->singularSpeakers.clear();
				subtract(lastSG->speakers, lastSG->groupedSpeakers, lastSG->singularSpeakers);
				if (!headerPrinted)
				{
					lplog(LOG_SG, u"SGP[2]:------ %s -------", toText(*lastSG, tmpstr));
					headerPrinted = true;
				}
				break;
			}
			if (!beforeLastSG->speakersAreNeverGroupedTogether && numFound)
			{
				possibleCurrentSubgroup |= (lastSG->speakers.size() >= 3 && sg == whichSG - 1 && numFound == 1);
				if (!headerPrinted)
				{
					lplog(LOG_SG, u"SGP[3]:------ %s -------", toText(*lastSG, tmpstr));
					headerPrinted = true;
				}
				else if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"SGP:   left:%s", objectString(speakers, tmpstr2).c_str());
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"SGP:   %d found in #%d:%s.", numFound, sg, toText(*beforeLastSG, tmpstr));
				set <int>::iterator si;
				// if any members found in previous speaker groups, but not all
				for (set <int>::iterator blsi = beforeLastSG->speakers.begin(), blsiEnd = beforeLastSG->speakers.end(); blsi != blsiEnd; blsi++)
					if ((si = speakers.find(*blsi)) != speakers.end())
						speakers.erase(si);
			}
		}
		// check if current group contains subgroup
		// lastSG must contain one and only one member of the last speaker group
		// must have three or more speakers
		if (lastSG->previousSubsetSpeakerGroup < 0 && possibleCurrentSubgroup)
		{
			lastSG->groupedSpeakers.clear();
			subtract(lastSG->speakers, speakerGroups[whichSG - 1].speakers, lastSG->groupedSpeakers);
			if (lastSG->speakers.size() - lastSG->groupedSpeakers.size() != 1 || lastSG->groupedSpeakers.size() < 2)
			{
				lastSG->groupedSpeakers.clear();
				return;
			}
			// if these speakers are never grouped in the current speaker group, reject.
			bool groupingFound = false;
			for (vector < cSpeakerGroup::cGroup >::iterator gi = lastSG->groups.begin(), giEnd = lastSG->groups.end(); gi != giEnd && !groupingFound; gi++)
				if (intersect(lastSG->groupedSpeakers, gi->objects, allIn, oneIn) && allIn && lastSG->groupedSpeakers.size() == gi->objects.size())
					groupingFound = true;
			if (!groupingFound)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"SGP:   Subgroup rejected [current subgroup - no group found] #%d:%s.", whichSG, objectString(lastSG->groupedSpeakers, tmpstr2).c_str());
				lastSG->groupedSpeakers.clear();
				return;
			}
			if (lastSG->povSpeakers.size() > 1 && lastSG->groupedSpeakers.size() == lastSG->povSpeakers.size() && lastSG->speakers.size() < lastSG->povSpeakers.size() * 2 &&
				lastSG->povSpeakers != lastSG->groupedSpeakers)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"SGP:   Subgroup rejected [current subgroup - no match with povSpeakers] #%d:%s.", whichSG, objectString(lastSG->groupedSpeakers, tmpstr2).c_str());
				lastSG->groupedSpeakers = lastSG->povSpeakers;
			}
			lastSG->singularSpeakers.clear();
			subtract(lastSG->speakers, lastSG->groupedSpeakers, lastSG->singularSpeakers);
			lastSG->previousSubsetSpeakerGroup = CURRENT_SUBSET_SG;
			if (!headerPrinted)
			{
				lplog(LOG_SG, u"SGP[4]:------ %s -------", toText(*lastSG, tmpstr));
				headerPrinted = true;
			}
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"SGP:   Subgroup is [current subgroup] #%d:%s.", whichSG, objectString(lastSG->groupedSpeakers, tmpstr2).c_str());
		}
		allIn = oneIn = false;
		lastSG->speakersAreNeverGroupedTogether = true;
		if (lastSG->conversationalQuotes > 0)
			for (vector < cSpeakerGroup::cGroup >::iterator gi = lastSG->groups.begin(), giEnd = lastSG->groups.end(); gi != giEnd && lastSG->speakersAreNeverGroupedTogether; gi++)
				if (intersect(lastSG->speakers, gi->objects, allIn, oneIn) && allIn)
					lastSG->speakersAreNeverGroupedTogether = false;
		if (lastSG->speakersAreNeverGroupedTogether)
		{
			// get common lastSpeakerGroup
			int lastSpeakerGroup = -1;
			bool notGrouped = false;
			for (set <int>::iterator si = lastSG->speakers.begin(), siEnd = lastSG->speakers.end(); si != siEnd && !notGrouped; si++)
			{
				//int tmp=getLastSpeakerGroup(*si,whichSG);
				if (lastSpeakerGroup == -1)
					lastSpeakerGroup = getLastSpeakerGroup(*si, whichSG);
				else
					notGrouped = (getLastSpeakerGroup(*si, whichSG) != lastSpeakerGroup);
			}
			if (lastSpeakerGroup == -1) notGrouped = true;
			if (!notGrouped)
			{
				// so in this lastSpeakerGroup, are all the speakers in groupedSpeakers?
				for (set <int>::iterator si = lastSG->speakers.begin(), siEnd = lastSG->speakers.end(); si != siEnd && !notGrouped; si++)
					notGrouped = speakerGroups[lastSpeakerGroup].groupedSpeakers.find(*si) == speakerGroups[lastSpeakerGroup].groupedSpeakers.end();
				// OK so not in grouped speakers.  Maybe in groups?
				if (notGrouped)
				{
					for (vector < cSpeakerGroup::cGroup >::iterator gi = speakerGroups[lastSpeakerGroup].groups.begin(), giEnd = speakerGroups[lastSpeakerGroup].groups.end(); gi != giEnd && notGrouped; gi++)
						if (intersect(lastSG->speakers, gi->objects, allIn, oneIn) && allIn)
							notGrouped = false;
					// OK so not in groups either.  Perhaps in the past (if the number of speakers match)?
					if (notGrouped && lastSG->speakers.size() == speakerGroups[lastSpeakerGroup].speakers.size() && !speakerGroups[lastSpeakerGroup].speakersAreNeverGroupedTogether)
						notGrouped = false;
				}
			}
			lastSG->speakersAreNeverGroupedTogether = notGrouped;
		}
		if (preferredMPluralGroup.size() && preferredMPluralGroup.size() != lastSG->speakers.size() &&
			(!intersect(preferredMPluralGroup, lastSG->groupedSpeakers, allIn, oneIn) || !allIn || preferredMPluralGroup.size() != lastSG->groupedSpeakers.size()))
		{
			// see if preferredMPluralGroup + lastSG->groupedSpeakers have an empty intersection, and both have full intersection with speakers set.
			// if this is true, then preferredMPluralGroup is merely the flip side of groupedSpeakers and can be ignored.
			if (intersect(preferredMPluralGroup, lastSG->groupedSpeakers, allIn, oneIn) || preferredMPluralGroup.size() + lastSG->groupedSpeakers.size() < speakers.size())
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG | LOG_RESOLUTION, u"%06d:Subgrouping disagreement preferred %s", where, objectString(preferredMPluralGroup, tmpstr2).c_str());
				lastSG->groupedSpeakers.clear();
				lastSG->singularSpeakers.clear();
				for (int I = 0; I < (signed)preferredMPluralGroup.size(); I++)
					lastSG->groupedSpeakers.insert(preferredMPluralGroup[I]);
				subtract(lastSG->speakers, lastSG->groupedSpeakers, lastSG->singularSpeakers);
			}
		}
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG | LOG_RESOLUTION, u"%06d:Subgrouping resulted in %s", where, toText(*lastSG, tmpstr));
	}
}

// remove hail objects that have not been detected any other way than through hail.
// Drop speakers whose only evidence is a hail vocative (PISHail, no definite
// / subject hits, low encounter counts).  Parsing errors produce a lot of
// these.  Kept when the group just crossed a chapter boundary and deleting
// them would leave no physically-present speakers at all.
void cSource::eliminateSpuriousHailSpeakers(int begin, int end, cSpeakerGroup& sg, bool speakerGroupCrossesSectionBoundary)
{
	LFS
		lpwstring tmpstr;
	// eliminate all speakers having only PISHail counts
	// remove hail objects that have not been detected any other way.  This is to minimize the effect of parsing errors.
	// how many gendered PP non-name objects are there?
	int physicallyPresentSpeakers = 0, hailSpeakersToDelete = 0;
	for (vector <cLocalFocus>::iterator lsi = localObjects.begin(); lsi != localObjects.end(); lsi++)
		if (lsi->physicallyPresent && objects[lsi->om.object].isAgent(false) && objects[lsi->om.object].objectClass != BODY_OBJECT_CLASS && objects[lsi->om.object].objectClass != META_GROUP_OBJECT_CLASS)
		{
			//lplog(LOG_SG,u"%s",objectString(lsi->om,tmpstr,true).c_str());
			physicallyPresentSpeakers++;
		}
	for (set <int>::iterator s = sg.speakers.begin(); s != sg.speakers.end(); s++)
	{
		vector <cLocalFocus>::iterator lsi = in(*s);
		if (objects[*s].PISHail && !objects[*s].PISDefinite && !objects[*s].PISSubject &&
			(objects[*s].PISHail <= 2 || objects[*s].numDefinitelyIdentifiedAsSpeakerInSection <= 2) &&
			(objects[*s].objectClass != NAME_OBJECT_CLASS || objects[*s].name.hon == wNULL || objects[*s].name.justHonorific() || objects[*s].numEncountersInSection == 0 || (lsi != localObjects.end() && lsi->previousWhere < 0)))
			hailSpeakersToDelete++;
	}
	// if speaker group just crossed over a boundary (like a chapter marker), then people may be hailed without introduction
	if (speakerGroupCrossesSectionBoundary && (physicallyPresentSpeakers - hailSpeakersToDelete) == 0)
		return;
	for (set <int>::iterator s = sg.speakers.begin(); s != sg.speakers.end(); )
	{
		vector <cLocalFocus>::iterator lsi = in(*s);
		if (objects[*s].PISHail && !objects[*s].PISDefinite && !objects[*s].PISSubject &&
			(objects[*s].PISHail <= 2 || objects[*s].numDefinitelyIdentifiedAsSpeakerInSection <= 2) &&
			(objects[*s].objectClass != NAME_OBJECT_CLASS || objects[*s].name.hon == wNULL || objects[*s].name.justHonorific() || objects[*s].numEncountersInSection == 0 || (lsi != localObjects.end() && lsi->previousWhere < 0)))
		{
			if (debugTrace.traceSpeakerResolution)
			{
				lplog(LOG_SG, u"%06d-%06d:%02d   hail deleted: %s (HAIL=%d,%d,%d,%d:%d,%d,%d,%d) [physicallyPresentSpeakers=%d] (%s)", begin, end, section,
					objectString(*s, tmpstr, true).c_str(), objects[*s].PISHail,
					objects[*s].numEncounters, objects[*s].numIdentifiedAsSpeaker, objects[*s].numDefinitelyIdentifiedAsSpeaker,
					objects[*s].numEncountersInSection, objects[*s].numSpokenAboutInSection, objects[*s].numIdentifiedAsSpeakerInSection, objects[*s].numDefinitelyIdentifiedAsSpeakerInSection,
					physicallyPresentSpeakers, (speakerGroupCrossesSectionBoundary) ? u"speakerGroupCrossesSectionBoundary" : u"");
				if (lsi != localObjects.end())
					lplog(LOG_SG, u"%06d-%06d:%02d   LW=%d,PW=%d,%s", begin, end, section,
					lsi->lastWhere, lsi->previousWhere, (lsi->physicallyPresent) ? u"PP" : u"not PP");
			}
			sg.groupedSpeakers.erase(*s);
			sg.singularSpeakers.erase(*s);
			sg.povSpeakers.erase(*s);
			sg.speakers.erase(s++);
		}
		else
			s++;
	}
}

// take the intersection between the current and future speaker groups.  If the current has an unresolvable object,
// and the next speakerGroup has an object not contained in the previous speakerGroup, allow the current to be merged into the future.
// Compare the last finished group with tempSpeakerGroup.  If the last group
// has exactly one still-unresolvable speaker and the next group introduces
// exactly one new, gender-matching, non-body speaker, return that future
// object index so mergeTempSpeakerGroupWithLastSG can treat it as the
// resolution of "a voice" / "the man".  Returns -1 if no such pair.
int cSource::detectUnresolvableObjectsResolvableThroughSpeakerGroup(void)
{
	LFS
		if (speakerGroups.empty()) return -1;
	set <int> futureSpeakers = tempSpeakerGroup.speakers, currentSpeakers = speakerGroups[speakerGroups.size() - 1].speakers;
	for (set <int>::iterator si = currentSpeakers.begin(); si != currentSpeakers.end(); )
		if (futureSpeakers.erase(*si))
		{
			currentSpeakers.erase(*si);
			si = currentSpeakers.begin();
		}
		else
			si++;
	if (futureSpeakers.size() > 1)
	{
		// erase all objects that have ever appeared in the past, but only if all present objects have already been erased.
		for (set <int>::iterator si = futureSpeakers.begin(); si != futureSpeakers.end(); )
			if (objects[*si].firstPhysicalManifestation >= 0 && objects[*si].firstPhysicalManifestation < speakerGroups[speakerGroups.size() - 1].sgBegin)
			{
				futureSpeakers.erase(*si);
				currentSpeakers.erase(*si);
				si = futureSpeakers.begin();
			}
			else
				si++;
	}
	int unresolvableObject = -1;
	for (set <int>::iterator si = currentSpeakers.begin(), siEnd = currentSpeakers.end(); si != siEnd; si++)
		if (unResolvablePosition(objects[*si].begin) && m[objects[*si].originalLocation].objectMatches.empty())
			unresolvableObject = *si;
	if (futureSpeakers.size() == 1 && unresolvableObject >= 0 &&
		objects[*futureSpeakers.begin()].objectClass != BODY_OBJECT_CLASS &&
		objects[unresolvableObject].objectClass != GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && // if the unresolvableObject is an occupation, this is always wrong or unnecessary (24 cases)
		objects[*futureSpeakers.begin()].matchGender(objects[unresolvableObject]))
	{
		lpwstring tmpstr, tmpstr2;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"Speaker %s resolvable by future speakerGroup object %s.",
				objectString(unresolvableObject, tmpstr, false).c_str(), objectString(*futureSpeakers.begin(), tmpstr2, false).c_str());
		return *futureSpeakers.begin();
	}
	return -1;
}

// If tempSpeakerGroup is a singleton POV speaker who is not among
// subjectsInPreviousUnquotedSection, scan [begin,end) for a genuine MOVE /
// EXIT / ENTER (not reflexive, not acting on a body part, not a figurative
// prep).  Returns the location of that move, or -1 if the speaker stayed.
// A move means the previous-paragraph subjects should NOT be imported.
int cSource::determineIfSpeakerMoved(int begin, int end, bool endOfSection)
{
	bool allIn = false, oneIn = false;
	// is one or all of subjectsInPreviousUnquotedSection in the speakers of tempSpeakerGroup?
	intersect(subjectsInPreviousUnquotedSection, tempSpeakerGroup.speakers, allIn, oneIn);
	// if this speaker group starts with an unquoted paragraph, and the speakerGroup only consists of a single speaker, 
	//  determine if that speaker exits or moves during that unquoted paragraph in the next lines
	//  and if so don't import previous speakers into this speaker group.
	if (tempSpeakerGroup.speakers.size() == 1 && !allIn)
	{
		int object = *tempSpeakerGroup.speakers.begin(), wherePOV = -1;
		// is object not POV?
		for (int I = povInSpeakerGroups.size() - 1; I >= 0 && section < sections.size() && povInSpeakerGroups[I] >= (signed)sections[section].begin && wherePOV < 0; I--)
			if (in(object, povInSpeakerGroups[I]))
				wherePOV = povInSpeakerGroups[I];
		// if the speaker IS POV
		if (wherePOV >= 0)
		{
			bool exitOnly = false;
			// while in this section, does this POV object MOVE itself (self move verb) and is not a body object?
			for (vector <cObject::cLocation>::iterator loc = objects[object].locations.begin(), locEnd = objects[object].locations.end(); loc != locEnd; loc++)
				if (loc->at >= begin && loc->at < end && m[loc->at].getRelVerb() >= 0 && isSelfMoveVerb(m[loc->at].getRelVerb(), exitOnly) && m[loc->at].getObject() >= 0 && objects[m[loc->at].getObject()].objectClass != BODY_OBJECT_CLASS)
				{
					// find associated syntactic relationship.
					vector <cSyntacticRelationGroup>::iterator sr = findSyntacticRelationGroup(loc->at);
					if (sr == syntacticRelationGroups.end())
					{
						// perhaps the location has not been made into a syntactic relationship yet?  
						// try to create one
						vector <int> lastSubjects;
						detectSyntacticRelationGroup(loc->at, -1, lastSubjects);
						// find it.
						sr = findSyntacticRelationGroup(loc->at);
					}
					// consider using cSourceWordInfo::prepMoveType on the prep also
					// 75711: Tommy came to himself with a start.
					bool isReflexive = m[m[loc->at].getRelVerb()].relPrep >= 0 && m[m[m[loc->at].getRelVerb()].relPrep].getRelObject() >= 0 && m[m[m[m[loc->at].getRelVerb()].relPrep].getRelObject()].queryWinnerForm(reflexivePronounForm) >= 0;
					// 91790: Tuppence flushed , then opened her[tuppence] mouth[tuppence] impulsively ?
					bool actingUponBodyPart = m[m[loc->at].getRelVerb()].getRelObject() >= 0 && objects[m[m[m[loc->at].getRelVerb()].getRelObject()].getObject()].objectClass == BODY_OBJECT_CLASS;
					// 42241: Tuppence went through a momentary struggle
					bool prepObjectIsNotPhysical = m[loc->at].relPrep >= 0 && m[m[loc->at].relPrep].getRelObject() >= 0 && (m[m[m[loc->at].relPrep].getRelObject()].word->second.flags & cSourceWordInfo::notPhysicalObjectByWN);
					// 48865: Suddenly he[julius] came out of his[julius] brown study 
					// 52010: Tuppence went upstairs to her[tuppence] room
					// 57862: Tommy came forward eagerly . 
					// 60715: He[tommy] turned the corner of the square . 
					// 69899: Sir James went at once to the root of the matter . 
					// if a POV and definitely a MOVE, EXIT or ENTER, then this object which was in the previous unquoted section to this speaker group
					// has moved out and should not be considered a current speaker.
					if (!isReflexive && !actingUponBodyPart && !prepObjectIsNotPhysical && sr != syntacticRelationGroups.end() && (sr->relationType == stMOVE || sr->relationType == stEXIT || sr->relationType == stENTER))
					{
						lpwstring tmpstr, tmpstr2;
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_SG, u"%06d-%06d:%02d rejected subjectsInPreviousUnquotedSection [%s subjectMoved@%d %s POV@%d] %s into %s prepObjectIsNotPhysical=%s", begin, end, section,
								(endOfSection) ? u"EOS" : u"not EOS", loc->at, relationString(sr->relationType).c_str(), wherePOV,
								objectString(subjectsInPreviousUnquotedSection, tmpstr).c_str(), objectString(tempSpeakerGroup.speakers, tmpstr2).c_str(),
								(prepObjectIsNotPhysical) ? u"true" : u"false");
						return loc->at;
					}
				}
		}
	}
	return -1;
}

// Merge or insert each subjectsInPreviousUnquotedSection member into
// tempSpeakerGroup and bump their identified-as-speaker counters (and the
// section's speakerObjects list on first identification).
void cSource::insertPreviousUnquotedSpeakers(int begin, int end)
{
	for (int spusi = 0; spusi < (signed)subjectsInPreviousUnquotedSection.size(); spusi++)
	{
		set <int>::iterator stsi;
		// if the next sentence is not fully quoted, this subject will not be used
		bool uniquelyMergable, inserted = (unMergable(-1, subjectsInPreviousUnquotedSection[spusi], tempSpeakerGroup.speakers, uniquelyMergable, true, false, false, false, stsi));
		lpwstring tmpstr, tmpstr2;
		//vector <cLocalFocus>::iterator lsi=in(subjectsInPreviousUnquotedSection[spusi]);
		//lpchar_t *physicallyPresent=(lsi!=localObjects.end() && lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent>=0) ? u"[PP]":u"[NPP]";
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d-%06d:%02d   %s subjectsInPreviousUnquotedSection %s into %s", begin, end, section, (inserted) ? u"inserted" : u"merged",
				objectString(subjectsInPreviousUnquotedSection[spusi], tmpstr, true).c_str(), objectString(tempSpeakerGroup.speakers, tmpstr2).c_str());
		if (inserted)
		{
			objects[subjectsInPreviousUnquotedSection[spusi]].numIdentifiedAsSpeaker++;
			objects[subjectsInPreviousUnquotedSection[spusi]].numIdentifiedAsSpeakerInSection++;
			if (objects[subjectsInPreviousUnquotedSection[spusi]].numDefinitelyIdentifiedAsSpeakerInSection + objects[subjectsInPreviousUnquotedSection[spusi]].numIdentifiedAsSpeakerInSection == 1 && sections.size())
				sections[section].speakerObjects.push_back(cOM(subjectsInPreviousUnquotedSection[spusi], SALIENCE_THRESHOLD));
			vector <cLocalFocus>::iterator lsi = in(subjectsInPreviousUnquotedSection[spusi]);
			if (lsi != localObjects.end())
				lsi->numIdentifiedAsSpeaker++;
		}
	}
}

// Count how many tempSpeakerGroup speakers cannot merge into lastSG
// (speakersNotMergable) vs. are hail-only and so should not force a new
// group (onlyHailSpeakers).  Does not mutate lastSG; the actual merge is
// mergeTempSpeakerGroupWithLastSG2.  Skips speakers already in replacedSpeakers.
void cSource::mergeTempSpeakerGroupWithLastSG(int begin, int end, int& speakersNotMergable, int& onlyHailSpeakers, vector <cSpeakerGroup>::iterator lastSG, int resolvableByFutureSpeaker, bool speakerGroupCrossesSectionBoundary)
{
	for (set <int>::iterator s = tempSpeakerGroup.speakers.begin(), sEnd = tempSpeakerGroup.speakers.end(); s != sEnd; s++)
	{
		lpwstring tmpstr, tmpstr2, tmpstr3;
		if (in(*s, lastSG->replacedSpeakers) != lastSG->replacedSpeakers.end())
			continue;
		bool uniquelyMergable;
		set <int>::iterator mergedSpeakerObject;
		if (unMergable(-1, *s, lastSG->speakers, uniquelyMergable, false, speakerGroupCrossesSectionBoundary, resolvableByFutureSpeaker == *s, true, mergedSpeakerObject))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   Speaker %s of current group unmergable with last speaker group %s.",
					begin, end, section, objectString(*s, tmpstr, true).c_str(), objectString(lastSG->speakers, tmpstr2).c_str());
			if (lastSG->speakers.size() > 1 && objects[*s].PISHail >= 0 && objects[*s].numEncounters == 0 && objects[*s].numEncountersInSection == 0)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%06d-%06d:%02d  just hail speaker don't create a new group - %s (HAIL=%d,%d,%d,%d:%d,%d,%d,%d)", begin, end, section,
						objectString(*s, tmpstr, true).c_str(), objects[*s].PISHail,
						objects[*s].numEncounters, objects[*s].numIdentifiedAsSpeaker, objects[*s].numDefinitelyIdentifiedAsSpeaker,
						objects[*s].numEncountersInSection, objects[*s].numSpokenAboutInSection, objects[*s].numIdentifiedAsSpeakerInSection, objects[*s].numDefinitelyIdentifiedAsSpeakerInSection);
				onlyHailSpeakers++;
			}
			else
				speakersNotMergable++;
		}
		// check to see whether an object that is only mentioned with a gendered object is
		// new object must be a name, previous object must not be a name
		// new object's first mention must be after the previous one
		else if (uniquelyMergable && mergedSpeakerObject != lastSG->speakers.end() &&
			objects[*s].objectClass == NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass != NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass != BODY_OBJECT_CLASS &&
			objects[*s].getFirstSpeakerGroup() >= 0 &&
			speakerGroups[objects[*s].getFirstSpeakerGroup()].sgBegin < objects[*mergedSpeakerObject].begin)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   Speaker %s of current group unmergable (Unique but without new object) with last speaker group %s.",
					begin, end, section, objectString(*s, tmpstr, true).c_str(), objectString(lastSG->speakers, tmpstr2).c_str());
			speakersNotMergable++;
		}
		else
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   Speaker %s of current group merged with %s in last speaker group %s.",
					begin, end, section, objectString(*s, tmpstr, true).c_str(), objectString(*mergedSpeakerObject, tmpstr3, false).c_str(), objectString(lastSG->speakers, tmpstr2).c_str());
	}
}


// Apply the merge: uniquely-mergable NAME vs non-NAME pairs call
// replaceSpeaker (and copy gender if the name is still M+F); unmatched
// speakers are inserted into lastSG; already-known speakers reset
// speakerAges to 0.  META_GROUP_OBJECT_CLASS hits also zero every other
// meta-group's age so they are not aged out before later resolution.
// Returns true if at least one replaceSpeaker occurred (caller loops).
bool cSource::mergeTempSpeakerGroupWithLastSG2(int begin, int end, vector <cSpeakerGroup>::iterator lastSG, int resolvableByFutureSpeaker, bool speakerGroupCrossesSectionBoundary)
{
	bool atLeastOneMerged = false;
	for (set <int>::iterator s = tempSpeakerGroup.speakers.begin(); s != tempSpeakerGroup.speakers.end(); s++)
	{
		lpwstring tmpstr, tmpstr2;
		if (in(*s, lastSG->replacedSpeakers) != lastSG->replacedSpeakers.end())
			continue;
		bool uniquelyMergable;
		set <int>::iterator mergedSpeakerObject;
		unMergable(-1, *s, lastSG->speakers, uniquelyMergable, false, speakerGroupCrossesSectionBoundary, resolvableByFutureSpeaker == *s, true, mergedSpeakerObject);
		// addNewSpeaker - possibly!
		if (uniquelyMergable && mergedSpeakerObject != lastSG->speakers.end() &&
			// preventing occupation object stops faust from becoming a gaoler speaker
			objects[*s].objectClass == NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass != NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass != GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
			objects[*s].matchGender(objects[*mergedSpeakerObject]))
		{
			int saveObject = *mergedSpeakerObject;
			if (objects[*s].male && objects[*s].female && !(objects[saveObject].male && objects[saveObject].female))
			{
				objects[*s].male = objects[saveObject].male;
				objects[*s].female = objects[saveObject].female;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG | LOG_RESOLUTION, u"%06d-%06d:%02d %s narrowed to %s.", begin, end, section, objectString(*s, tmpstr, true).c_str(), (objects[*s].male) ? u"male" : u"female");
			}
			tempSpeakerGroup.speakers.erase(saveObject); // must use *mergedSpeakerObject before it is replaced
			replaceSpeaker(lastSG->sgBegin, end, saveObject, *s);
			// if the replaced object is 'man's voice' then replace man as well as man's voice
			if (objects[saveObject].objectClass == BODY_OBJECT_CLASS && objects[saveObject].getOwnerWhere() >= 0 && m[objects[saveObject].getOwnerWhere()].getObject() >= 0)
				replaceSpeaker(lastSG->sgBegin, end, m[objects[saveObject].getOwnerWhere()].getObject(), *s);
			speakerAges[*s] = 0;
			atLeastOneMerged = true;
			s = tempSpeakerGroup.speakers.begin();
			continue;
		}
		else if (mergedSpeakerObject == lastSG->speakers.end())
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d %s inserted -> %s", begin, end, section, objectString(*s, tmpstr, true).c_str(), toText(*lastSG, tmpstr2));
			lastSG->speakers.insert(*s);
			speakerAges.insert(tSA(*s, 0));
		}
		else
			speakerAges[*s] = 0;
		// if a metagroup object is encountered, also zero all other meta-group objects
		// because it is not certain whether this metagroup object matches another until later processing
		// and if this is not done then metaGroup objects will be aged out of the speaker group.
		if (objects[*s].objectClass == META_GROUP_OBJECT_CLASS)
		{
			for (set <int>::iterator ls = lastSG->speakers.begin(), lsEnd = lastSG->speakers.end(); ls != lsEnd; ls++)
				if (objects[*ls].objectClass == META_GROUP_OBJECT_CLASS)
					speakerAges[*ls] = 0;
		}
	}
	return atLeastOneMerged;
}

// Fold tempSpeakerGroup into lastSG (looping mergeTempSpeakerGroupWithLastSG2
// until no more replacements), steal its groups / conversationalQuotes, set
// lastSG.sgEnd = end, and clear temp.  If the extended group is a lone
// speaker with quotes, also fold lastISNarrationSubjects in as audience.
void cSource::extendLastSGToEndOfSection(int begin, int end, vector <cSpeakerGroup>::iterator lastSG, int resolvableByFutureSpeaker, int& lastSpeakerGroupOfPreviousSection, bool speakerGroupCrossesSectionBoundary, bool endOfSection)
{
	lpwstring tmpstr, tmpstr2;
	while (mergeTempSpeakerGroupWithLastSG2(begin, end, lastSG, resolvableByFutureSpeaker, speakerGroupCrossesSectionBoundary));
	lastSG->groups.insert(lastSG->groups.end(), tempSpeakerGroup.groups.begin(), tempSpeakerGroup.groups.end());
	for (vector < cSpeakerGroup::cGroup >::iterator gi = tempSpeakerGroup.groups.begin(), giEnd = tempSpeakerGroup.groups.end(); gi != giEnd; gi++)
	{
		bool allIn, oneIn;
		// if group only contains speakers
		// if there is no conversation in the speakerGroup (conversationalQuotes>0), there is much less of a chance that the entities should be considered a "group"
		// They[streets,boris,tommy] reached at length[length] a small dilapidated square .
		if (intersect(gi->objects, lastSG->speakers, allIn, oneIn) && allIn && lastSG->conversationalQuotes > 0)
		{
			for (vector <int>::iterator si = gi->objects.begin(), siEnd = gi->objects.end(); si != siEnd; si++)
				lastSG->groupedSpeakers.insert(*si);
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   groupSpeakers %s added (1)", begin, end, section, objectString(*gi, tmpstr).c_str());
			break;
		}
	}
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"%06d-%06d:%02d   PIS speakerGroup %s extended to %d%s", lastSG->sgBegin, lastSG->sgEnd, section, toText(*lastSG, tmpstr), end, (endOfSection) ? u" end of section" : u"");
	lastSG->sgEnd = end;
	if (debugTrace.traceSpeakerResolution)
		for (set <int>::iterator s = lastSG->speakers.begin(); s != lastSG->speakers.end(); s++)
			lplog(LOG_SG, u"%06d-%06d:%02d %s speakerAge=%d.", lastSG->sgBegin, lastSG->sgEnd, section, objectString(*s, tmpstr, true).c_str(), speakerAges[*s]);
	lastSG->conversationalQuotes += tempSpeakerGroup.conversationalQuotes;
	tempSpeakerGroup.clearTemp(end);
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"%06d-%06d:%02d insert nextISNarrationSubjects %s into lastISNarrationSubjects %s.", begin, end, section, objectString(nextISNarrationSubjects, tmpstr).c_str(), objectString(lastISNarrationSubjects, tmpstr).c_str());
	lastISNarrationSubjects.insert(lastISNarrationSubjects.end(), nextISNarrationSubjects.begin(), nextISNarrationSubjects.end());
	whereLastISNarrationSubjects.insert(whereLastISNarrationSubjects.end(), whereNextISNarrationSubjects.begin(), whereNextISNarrationSubjects.end());
	// if there has been a conversation, but there is no audience
	if (lastSG->speakers.size() == 1 && lastSG->conversationalQuotes)
	{
		int offset = 0;
		for (vector <int>::iterator s = lastISNarrationSubjects.begin(), sEnd = lastISNarrationSubjects.end(); s != sEnd; s++, offset++)
		{
			set <int>::iterator stsi;
			bool uniquelyMergable = false, inserted = (unMergable(-1, *s, lastSG->speakers, uniquelyMergable, true, false, false, false, stsi));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   lastISNarrationSubjects [IS] %d:%s %s into %d:%s", begin, end, section, whereLastISNarrationSubjects[offset], objectString(*s, tmpstr, true).c_str(), (inserted) ? u"inserted" : u"merged", tempSpeakerGroup.sgBegin, objectString(tempSpeakerGroup.speakers, tmpstr2).c_str());
		}
		lastISNarrationSubjects.clear();
		whereLastISNarrationSubjects.clear();
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d-%06d:%02d cleared lastISNarrationSubjects.", begin, end, section);
	}
	if (endOfSection)
	{
		lastSpeakerGroupOfPreviousSection = speakerGroups.size() - 1;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"set lastSpeakerGroupOfPreviousSection=%d.", lastSpeakerGroupOfPreviousSection);
	}
}

// Close the last group (hail cleanup, speaker-removal split, subgrouping),
// push tempSpeakerGroup onto speakerGroups, stamp each speaker's
// first/lastSpeakerGroup, reset speakerAges / speakerSections, and start a
// fresh temp at 'end'.
void cSource::pushTemporarySpeakerGroupAndErase(int begin, int end, bool endOfSection, int& lastSpeakerGroupOfPreviousSection)
{
	lpwstring tmpstr;
	vector <cSpeakerGroup>::iterator lastSG = (speakerGroups.size()) ? speakerGroups.begin() + speakerGroups.size() - 1 : speakerGroups.end();
	bool speakerGroupCrossesSectionBoundary = speakerGroups.size() > 0 && lastSpeakerGroupOfPreviousSection == speakerGroups.size() - 1;
	lastSG = (speakerGroups.size()) ? speakerGroups.begin() + speakerGroups.size() - 1 : speakerGroups.end();
	if (speakerGroups.size())
		eliminateSpuriousHailSpeakers(begin, end, *lastSG, speakerGroupCrossesSectionBoundary);
	determineSpeakerRemoval(begin);
	tempSpeakerGroup.section = section;
	lastSG = (speakerGroups.size()) ? speakerGroups.begin() + speakerGroups.size() - 1 : speakerGroups.end();
	if (speakerGroups.size())
		determinePreviousSubgroup(end, speakerGroups.size() - 1, &speakerGroups[speakerGroups.size() - 1]);
	if (debugTrace.traceSpeakerResolution)
	{
		if (speakerGroups.size())
			lplog(LOG_SG, u"%06d-%06d:%02d**** speakerGroup %s CLOSED%s", lastSG->sgBegin, lastSG->sgEnd, section, toText(*lastSG, tmpstr), (endOfSection) ? u" end of section" : u"");
		lplog(LOG_SG, u"%06d-%06d:%02d   %s added%s", begin, end, section, toText(tempSpeakerGroup, tmpstr), (endOfSection) ? u" end of section" : u"");
	}
	determineSubgroupFromGroups(tempSpeakerGroup);
	speakerGroups.push_back(tempSpeakerGroup);
	currentSpeakerGroup = speakerGroups.size();
	speakerAges.clear();
	for (set <int>::iterator s = tempSpeakerGroup.speakers.begin(), sEnd = tempSpeakerGroup.speakers.end(); s != sEnd; s++)
	{
		if (objects[*s].getFirstSpeakerGroup() == -1)
			objects[*s].setFirstSpeakerGroup(currentSpeakerGroup - 1);
		objects[*s].lastSpeakerGroup = currentSpeakerGroup - 1;
		objects[*s].ageSinceLastSpeakerGroup = 0;
		speakerAges.insert(tSA(*s, 0));
	}
	tempSpeakerGroup.clearTemp(end);
	speakerSections.clear();
	speakerSections.push_back(begin);
	lastISNarrationSubjects = nextISNarrationSubjects;
	whereLastISNarrationSubjects = whereNextISNarrationSubjects;
	introducedByReference.clear();
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"lastISNarrationSubjects=nextISNarrationSubjects=%s.", objectString(nextISNarrationSubjects, tmpstr).c_str());
	if (endOfSection)
	{
		lastSpeakerGroupOfPreviousSection = speakerGroups.size() - 1;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"set lastSpeakerGroupOfPreviousSection=%d.", lastSpeakerGroupOfPreviousSection);
	}
}

// begin - beginning new section
// end -   ending new section
// spNarrationSubjects - narration subjects all of which were unMergable with the lastSpeakerGroup
// nextNarrationSubjects - narration subjects inbetween begin and end
// tempSpeakerGroup - collection of all speakers inbetween begin and end
// endOfSection - end = end of chapter or other section
// subjectsInPreviousUnquotedSection - subject or plural subject(s) immediately before tmpSpeakerGroup

// if tempSpeakerGroup speakers<2, add narrators that came before, narrators that occurred in the section and add subjectsInPreviousUnquotedSection
// if all of tempSpeakerGroup occurred in the last speaker group, merge all and extend last speaker group.
// if all but one occurred in last speaker group and last speaker group only contained one speaker, add the one that did not occur to last speaker group and extend last speaker group
// if more than one did not occur in last speaker group or last speaker group contained more than one speaker, create a new speaker group.
// Decide, at a paragraph or chapter boundary [begin,end), whether to extend
// the last speaker group or start a new one.  Imports nextNarrationSubjects
// (and, if the cast is still < 2, nextISNarrationSubjects and the previous
// unquoted subjects unless determineIfSpeakerMoved says they left).  Extends
// lastSG when every (or all-but-one, if lastSG is a singleton) temp speaker
// merges, or when lastSG is a single quoted paragraph that cannot close.
// Returns true if a group was created or extended.  begin==end is a no-op.
bool cSource::createSpeakerGroup(int begin, int end, bool endOfSection, int& lastSpeakerGroupOfPreviousSection)
{
	LFS
		if (begin == end)
			return false;
	set <int> saveSpeakers = tempSpeakerGroup.speakers;
	tempSpeakerGroup.sgEnd = end;
	lpwstring tmpstr, tmpstr2, tmpstr3;
	bool uniquelyMergable, inserted;
	set <int>::iterator stsi;
	//int sectionBegin=(sections.size()) ? sections[section].begin:0;
	vector <cSpeakerGroup>::iterator lastSG = (speakerGroups.size()) ? speakerGroups.begin() + speakerGroups.size() - 1 : speakerGroups.end();
	bool conversationOccurred = tempSpeakerGroup.speakers.size() != 0;
	// nextNarrationSubjects - narration subjects in-between begin and end
	for (vector <int>::iterator s = nextNarrationSubjects.begin(), sEnd = nextNarrationSubjects.end(); s != sEnd; s++)
	{
		inserted = (unMergable(-1, *s, tempSpeakerGroup.speakers, uniquelyMergable, true, false, false, false, stsi));
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d-%06d:%02d   spNextNarrationSubject %s %s into %s", begin, end, section, objectString(*s, tmpstr, true).c_str(), (inserted) ? u"inserted" : u"merged", objectString(tempSpeakerGroup.speakers, tmpstr2).c_str());
	}
	// nextNarrationSubjects - narration subjects in-between begin and end AND only subjects which are also IS_OBJECT
	if (tempSpeakerGroup.speakers.size() < 2 && conversationOccurred)
	{
		int offset = 0;
		for (vector <int>::iterator s = nextISNarrationSubjects.begin(), sEnd = nextISNarrationSubjects.end(); s != sEnd; s++, offset++)
		{
			inserted = (unMergable(-1, *s, tempSpeakerGroup.speakers, uniquelyMergable, true, false, false, false, stsi));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d-%06d:%02d   nextISNarrationSubjects [IS] %d:%s %s into %d:%s", begin, end, section, whereNextISNarrationSubjects[offset], objectString(*s, tmpstr, true).c_str(), (inserted) ? u"inserted" : u"merged", tempSpeakerGroup.sgBegin, objectString(tempSpeakerGroup.speakers, tmpstr2).c_str());
		}
		nextISNarrationSubjects.clear();
		whereNextISNarrationSubjects.clear();
	}
	// this does not actually include another more thorough way - is the speaker specified after the quote?
	// if there are less than 2 speakers, there were subjects in the previous unquoted section and the section ended with an open double quote
	if (tempSpeakerGroup.speakers.size() < 2 && subjectsInPreviousUnquotedSection.size() >= 1 && end + 1 < (signed)m.size() && m[end + 1].word->first == u"�")
	{
		// bool BF=(tempSpeakerGroup.begin+1<m.size() && m[tempSpeakerGroup.begin+1].forms.isSet(quoteForm)); all speaker groups at this point start with an unquoted paragraph
		if (determineIfSpeakerMoved(begin, end, endOfSection) < 0)
			insertPreviousUnquotedSpeakers(begin, end);
	}
	bool speakerGroupCrossesSectionBoundary = speakerGroups.size() > 0 && lastSpeakerGroupOfPreviousSection == speakerGroups.size() - 1;
	// remove hail objects that have not been detected any other way.
	eliminateSpuriousHailSpeakers(begin, end, tempSpeakerGroup, speakerGroupCrossesSectionBoundary);
	set <int>::iterator mergedSpeakerObject;
	if (speakerGroups.size())
	{
		int speakersNotMergable = 0, onlyHailSpeakers = 0;
		int resolvableByFutureSpeaker = detectUnresolvableObjectsResolvableThroughSpeakerGroup();
		mergeTempSpeakerGroupWithLastSG(begin, end, speakersNotMergable, onlyHailSpeakers, lastSG, resolvableByFutureSpeaker, speakerGroupCrossesSectionBoundary);
		if (onlyHailSpeakers > 1)
		{
			speakersNotMergable += onlyHailSpeakers;
			onlyHailSpeakers = 0;
		}
		// if lastSG is not composed of a single quoted paragraph
		bool lastSGNotClosable = false;
		if (lastSG != speakerGroups.end() && m[lastSG->sgBegin + 1].word->first == u"�")
		{
			int q = lastSG->sgBegin + 1;
			while (m[q].getQuoteForwardLink() >= 0) q = m[q].getQuoteForwardLink();
			lastSGNotClosable = m[q].endQuote + 1 == lastSG->sgEnd;
		}
		// extend: lastSG is a quote-only span, OR every temp speaker merged,
		// OR exactly one new speaker and lastSG was a singleton (the new audience).
		if (lastSGNotClosable || !speakersNotMergable || (speakersNotMergable == 1 && lastSG->speakers.size() == 1))
		{
			if (debugTrace.traceSpeakerResolution)
			{
				if (lastSGNotClosable)
					lplog(LOG_SG, u"%06d-%06d:%02d   lastSG %s is a single quote.", begin, end, section, objectString(lastSG->speakers, tmpstr2).c_str());
				else if (!speakersNotMergable)
					lplog(LOG_SG, u"%06d-%06d:%02d   All of tempSpeakerGroup %s were found in %s.", begin, end, section, objectString(tempSpeakerGroup.speakers, tmpstr).c_str(), objectString(lastSG->speakers, tmpstr2).c_str());
				else
					lplog(LOG_SG, u"%06d-%06d:%02d   All but one of tempSpeakerGroup %s were found in %s.", begin, end, section, objectString(tempSpeakerGroup.speakers, tmpstr).c_str(), objectString(lastSG->speakers, tmpstr2).c_str());
			}
			extendLastSGToEndOfSection(begin, end, lastSG, resolvableByFutureSpeaker, lastSpeakerGroupOfPreviousSection, speakerGroupCrossesSectionBoundary, endOfSection);
			return true;
		}
	}
	else
		tempSpeakerGroup.sgBegin = 0;
	if (tempSpeakerGroup.speakers.size() || endOfSection)
	{
		// push temporary speaker group into speakerGroups and erase it to prepare for accumulating the speakers in the next section.
		pushTemporarySpeakerGroupAndErase(begin, end, endOfSection, lastSpeakerGroupOfPreviousSection);
		return true;
	}
	tempSpeakerGroup.speakers = saveSpeakers;
	return false;
}

const lpchar_t* metaResponse[] = { u"reply",u"response", u"answer", NULL }; // initialized

// Mark POV_OBJECT_ROLE (and push where / owner into povInSpeakerGroups) when
// the mention is an internal body-part of an owner, the subject of an
// internal body-part object, or the subject of an internal-state description
// ("he was conscious").  Returns true if POV was assigned.  Used later by
// distributePOV to fill sg.povSpeakers.
bool cSource::setPOVStatus(int where, bool inPrimaryQuote, bool inSecondaryQuote)
{
	LFS
		lpwstring tmpstr;
	if (m[where].flags & cWordMatch::flagAdjectivalObject)
	{
		int I = where;
		while (I >= 0 && m[I].principalWherePosition < 0) I--;
		if (m[I].principalWherePosition > where)
			where = m[I].principalWherePosition;
	}
	// To have boasted that she[tuppence] knew a lot might have raised doubts in his[mr] mind[mr] . (tuppence is POV,but not mr)
	if (m[where].getObject() >= 0 && objects[m[where].getObject()].getOwnerWhere() >= 0 && isInternalBodyPart(where) &&
		(!(m[where].objectRole & (OBJECT_ROLE | PREP_OBJECT_ROLE)) || !(m[where].flags & cWordMatch::flagInPStatement)))
	{
		povInSpeakerGroups.push_back(objects[m[where].getObject()].getOwnerWhere());
		m[where].objectRole |= POV_OBJECT_ROLE; // used in determining point of view/observer status for speakerGroups
		m[objects[m[where].getObject()].getOwnerWhere()].objectRole |= POV_OBJECT_ROLE;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   speaker %s has pov status [1].", where, section, objectString(m[objects[m[where].getObject()].getOwnerWhere()].getObject(), tmpstr, true).c_str());
		return true;
	}
	if (!inPrimaryQuote && !inSecondaryQuote && m[where].getObject() >= 0 && objects[m[where].getObject()].getOwnerWhere() < 0 && m[where].getRelObject() >= 0 &&
		m[m[where].getRelObject()].relNextObject < 0 &&   // rules out Jules gave Tuppence a feeling of support (Tuppence had the feeling)
		m[m[where].getRelObject()].getObject() >= 0 && objects[m[m[where].getRelObject()].getObject()].getOwnerWhere() < 0 &&
		isInternalBodyPart(m[where].getRelObject()))
	{
		povInSpeakerGroups.push_back(where);
		m[where].objectRole |= POV_OBJECT_ROLE; // used in determining point of view/observer status for speakerGroups
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   speaker %s has pov status [SUBJECT of internal object].", where, section, objectString(m[where].getObject(), tmpstr, true).c_str());
		return true;
	}
	// he was conscious
	if (!inPrimaryQuote && !inSecondaryQuote && m[where].getObject() >= 0 && objects[m[where].getObject()].getOwnerWhere() < 0 && m[where].getRelObject() < 0 &&
		((m[where].objectRole & (SUBJECT_ROLE | IS_OBJECT_ROLE | PREP_OBJECT_ROLE)) == (SUBJECT_ROLE | IS_OBJECT_ROLE)) && m[where].getRelVerb() >= 0 && m[where].getRelVerb() + 1 < (signed)m.size() &&
		isInternalDescription(m[where].getRelVerb() + 1))
	{
		povInSpeakerGroups.push_back(where);
		m[where].objectRole |= POV_OBJECT_ROLE; // used in determining point of view/observer status for speakerGroups
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   speaker %s has pov status [SUBJECT of internal description].", where, section, objectString(m[where].getObject(), tmpstr, true).c_str());
		return true;
	}
	return false;
}

// True when the mention is "in <letter/appeal/reply/response/answer>" and
// that prep object is not a place, time, or gendered entity.  Those
// constructions ("in the letter he pointed out") do not put the subject
// physically in the scene.
bool cSource::notPhysicallyPresentByMissive(int where)
{
	LFS
		if (m[where].getRelVerb() >= 0 && m[m[where].getRelVerb()].relPrep >= 0 && m[m[m[where].getRelVerb()].relPrep].word->first == u"in" &&
			m[m[m[where].getRelVerb()].relPrep].getRelObject() >= 0 && m[m[m[m[where].getRelVerb()].relPrep].getRelObject()].getObject() >= 0)
		{
			int wpo = m[m[m[where].getRelVerb()].relPrep].getRelObject(), prepObject = m[wpo].getObject();
			bool isPlace = false, isLetterWord = false, isPhysical = false, isGendered = false, isTime = false;
			for (int I = 0; I < (signed)m[wpo].objectMatches.size(); I++)
			{
				isPlace |= objects[m[wpo].objectMatches[I].object].getSubType() >= 0;
				int lastLetterBegin = -1, wmo = objects[m[wpo].objectMatches[I].object].originalLocation;
				isLetterWord |= detectLetterAsObject(wmo, lastLetterBegin) || m[wmo].word->first == u"appeal";
				for (int p = 0; metaResponse[p]; p++)
					isLetterWord |= (m[wmo].word->first == metaResponse[p]);
				isPhysical |= (m[wmo].word->second.flags & cSourceWordInfo::physicalObjectByWN) != 0;
				isGendered |= objects[m[wpo].objectMatches[I].object].male | objects[m[wpo].objectMatches[I].object].female;
				isTime |= (m[wmo].word->second.timeFlags & T_UNIT) != 0;
			}
			if (m[wpo].objectMatches.empty())
			{
				isPlace |= objects[prepObject].getSubType() >= 0;
				int lastLetterBegin = -1;
				isLetterWord |= detectLetterAsObject(wpo, lastLetterBegin) || m[wpo].word->first == u"appeal";
				for (int p = 0; metaResponse[p]; p++)
					isLetterWord |= (m[wpo].word->first == metaResponse[p]);
				isPhysical |= (m[wpo].word->second.flags & cSourceWordInfo::physicalObjectByWN) != 0;
				isGendered |= objects[prepObject].male | objects[prepObject].female;
				isTime |= (m[wpo].word->second.timeFlags & T_UNIT) != 0;
			}
			if (isLetterWord && !isPlace && !isTime && !isGendered)
			{
				lpwstring tmpstr;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s not physically present (in non-physical object/letter %d:%s).", where, section, whereString(where, tmpstr, true).c_str(), wpo, whereString(wpo, tmpstr, true).c_str());
				return true;
			}
		}
	return false;
}

// in secondary quotes, inPrimaryQuote=false
// True if object o at 'where' is a candidate for speaker-group / narration
// focus: gendered (or acceptable meta-group), in a subject / object / "to"
// prep role, FOCUS_EVALUATED, not a question or probability statement, not
// "each", not an adjectival modifier of a non-body head.  Sets
// isNotPhysicallyPresent for NOT / NONPAST / NONPRESENT / SENTENCE_IN_REL
// roles (still accepted if singular).  May also stamp POV.  Returns false
// for kind-of objects and for o < 0.
bool cSource::isFocus(int where, bool inPrimaryQuote, bool inSecondaryQuote, int o, bool& isNotPhysicallyPresent, bool subjectAllowPrep)
{
	LFS
		if (o < 0 || objects[o].isKindOf) return false;
	lpwstring tmpstr, tmpstr2;
	int objectClass = objects[o].objectClass;
	//vector <cLocalFocus>::iterator lsi=in(o);
	// he[tommy] rattled off the formula to the elderly woman , looking more like a housekeeper than a servant , who opened the door to him[tommy]
	//if (lsi!=localObjects.end() && lsi->notSpeaker) return false; must not restrict in this way because of the above sentence.
	// if preposition is 'to', then discard PREP_OBJECT_ROLE
	// this leads to correct results for such examples as 'to the elderly woman' but avoids 'request for Mr. Carter'
	uint64_t objectRole = m[where].objectRole;
	tIWMM before = (m[where].beginObjectPosition > 0) ? m[m[where].beginObjectPosition - 1].word : wNULL;
	if ((objectRole & PREP_OBJECT_ROLE) &&
		(before != wNULL && (before->first == u"to" || before->first == u"over")) &&
		m[where].principalWhereAdjectivalPosition < 0 &&
		// prevents "Whittington directed the driver to go to Waterloo"
		(objectClass != NAME_OBJECT_CLASS || objects[o].numIdentifiedAsSpeaker) &&
		// prevents " She[tuppence] went straight back to the Ritz and wrote a few brief words to mr . Carter"
		!(objectRole & DELAYED_RECEIVER_ROLE) &&
		// She[tuppence] remembered that[table] this was one of the men[guest] Tommy was shadowing 
		!(objectRole & RE_OBJECT_ROLE) &&
		(objectRole & FOCUS_EVALUATED))
	{
		objectRole &= ~PREP_OBJECT_ROLE;
		objectRole |= SUBJECT_ROLE;
	}
	//int tmp3=m[where].principalWhereAdjectivalPosition;
	//int tmp4=objects[o].at;
	// the first, his first etc.
	if (objectClass == META_GROUP_OBJECT_CLASS && cObject::whichOrderWord(m[objects[o].originalLocation].word) != -1 && !inPrimaryQuote && !(objectRole & SENTENCE_IN_REL_ROLE))
	{
		if ((isNotPhysicallyPresent = (objectRole & NONPAST_OBJECT_ROLE) != 0) && debugTrace.traceSpeakerResolution)
			lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s accepted for focus although OUTSIDE_QUOTE_NONPAST.", where, section, objectString(o, tmpstr, true).c_str());
		return (m[where].objectRole & UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE) != 0;
	}
	bool acceptableMetaGroupObject =
		objectClass == META_GROUP_OBJECT_CLASS && !objects[o].neuter &&
		((cObject::whichOrderWord(m[where].word) == -1 ||
			(objectRole & (IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) != IS_OBJECT_ROLE &&
			(objects[o].getOwnerWhere() != -1 || m[m[where].beginObjectPosition].word->first == u"the")));
	// exclude "the memory of Aunt Jane" and "picture of Jane Finn"
	// but include META_GROUP objects because they are not resolved till later, and most of the time they will be resolved and disappear
	bool allowablePrepObject = subjectAllowPrep &&
		(objectRole & PREP_OBJECT_ROLE) && m[where].principalWhereAdjectivalPosition < 0 &&
		before != wNULL && before->first != u"of" &&
		(objects[o].firstLocation < where || objectClass == META_GROUP_OBJECT_CLASS);
	int beginObjectPosition = (o == m[where].getObject() && objects[o].begin >= m[where].beginObjectPosition) ? m[where].beginObjectPosition : objects[o].begin;
	vector <cLocalFocus>::iterator lsi;
	bool isExit = false;
	if (m[where].hasSyntacticRelationGroup)
	{
		vector <cSyntacticRelationGroup>::iterator location = findSyntacticRelationGroup(where);
		if (location != syntacticRelationGroups.end() && location->where == where && (isExit = location->relationType == stEXIT) && location->genderedLocationRelation && (location + 1)->whereSubject == location->whereSubject)
			isExit = false;
	}
	if (((inPrimaryQuote && (objectRole & IN_EMBEDDED_STORY_OBJECT_ROLE)) || (!inPrimaryQuote && !inSecondaryQuote)) &&
		// exclude "the memory of Aunt Jane"
		(((objectRole & SUBJECT_ROLE) && !(objectRole & PREP_OBJECT_ROLE)) ||
			// exclude Tommy's ring
			((objectRole & OBJECT_ROLE) && m[where].principalWhereAdjectivalPosition < 0 &&
				(!(objectRole & PREP_OBJECT_ROLE) || (before == wNULL || before->first != u"of") || ((lsi = in(o)) != localObjects.end() && lsi->physicallyPresent))) ||
			allowablePrepObject) &&
		((objectClass == NAME_OBJECT_CLASS && (m[beginObjectPosition].queryWinnerForm(determinerForm) < 0 || objects[o].numIdentifiedAsSpeaker > 0)) ||
			objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
			objectClass == BODY_OBJECT_CLASS ||
			objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			objectClass == GENDERED_DEMONYM_OBJECT_CLASS ||
			objectClass == GENDERED_RELATIVE_OBJECT_CLASS || acceptableMetaGroupObject) &&
		// WHEN EXIT Tommy set forth on the trail of the two men[knocks] , it[trail] took all[tommy] Tuppence's self - command to refrain from accompanying him[tommy] .
		(!isExit))
	{
		if (m[where].flags & cWordMatch::flagAdjectivalObject)
		{
			int I;
			for (I = where + 1;  I < (signed)m.size() && (m[I].beginObjectPosition < 0 || (m[I].flags & cWordMatch::flagAdjectivalObject)); I++);
			if (I == m.size() || !(m[I].objectRole & SUBJECT_ROLE) || m[I].getObject() < 0 || objects[m[I].getObject()].objectClass != BODY_OBJECT_CLASS)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s rejected - adjective", where, section, objectString(o, tmpstr, true).c_str());
				return false;
			}
		}
		// there came the accents of Number One
		if ((objectRole & (SUBJECT_ROLE | PREP_OBJECT_ROLE | OBJECT_ROLE)) == OBJECT_ROLE &&
			(m[where].relSubject < 0 || m[m[where].relSubject].getObject() < 0 || objects[m[m[where].relSubject].getObject()].neuter) &&
			m[where].getRelVerb() >= 0 && m[m[where].getRelVerb()].word->first == u"came")
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s accepted through special 'came' object", where, section, objectString(o, tmpstr, true).c_str());
			objectRole |= SUBJECT_ROLE;
		}
		if ((!(objectRole & SUBJECT_ROLE) || (objectRole & PREP_OBJECT_ROLE)) && !objects[o].numIdentifiedAsSpeakerInSection && objects[o].objectClass != META_GROUP_OBJECT_CLASS)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s rejected for focus (not a prior speaker)", where, section, objectString(o, tmpstr, true).c_str());
			return false;
		}
		if ((objectRole & RE_OBJECT_ROLE) && objects[o].whereRelativeClause < 0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s rejected for focus (repeat object)", where, section, objectString(o, tmpstr, true).c_str());
			return false;
		}
		if ((m[where].flags & cWordMatch::flagInQuestion) != 0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s rejected for focus (question).", where, section, objectString(o, tmpstr, true).c_str());
			return false;
		}
		if ((m[where].flags & cWordMatch::flagInPStatement) != 0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s rejected for focus (probability statement).", where, section, objectString(o, tmpstr, true).c_str());
			return false;
		}
		if (!(objectRole & (FOCUS_EVALUATED | PRIMARY_SPEAKER_ROLE)))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s ROLES=%s rejected for focus (not evaluated).", where, section, objectString(o, tmpstr, true).c_str(), m[where].roleString(tmpstr2).c_str());
			return false;
		}
		if (isNotPhysicallyPresent = (objectRole & NOT_OBJECT_ROLE) != 0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s accepted for focus although NOT_ROLE.", where, section, objectString(o, tmpstr, true).c_str());
			return !objects[o].plural;
		}
		if (isNotPhysicallyPresent = (!inPrimaryQuote && !inSecondaryQuote && (objectRole & NONPAST_OBJECT_ROLE) != 0))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s accepted for focus although OUTSIDE_QUOTE_NONPAST (%s).", where, section, objectString(o, tmpstr, true).c_str(), m[where].roleString(tmpstr2).c_str());
			return !objects[o].plural;
		}
		if (!(objectRole & IN_EMBEDDED_STORY_OBJECT_ROLE) && (isNotPhysicallyPresent = (inPrimaryQuote && (objectRole & NONPRESENT_OBJECT_ROLE) != 0)))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s accepted for focus although IN_QUOTE_NONPRESENT.", where, section, objectString(o, tmpstr, true).c_str());
			return !objects[o].plural;
		}
		if (inPrimaryQuote && (objectRole & IN_EMBEDDED_STORY_OBJECT_ROLE) && (isNotPhysicallyPresent = (inPrimaryQuote && (objectRole & NONPAST_OBJECT_ROLE) != 0)))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s accepted for focus although IN_QUOTE_NONPAST (inside story).", where, section, objectString(o, tmpstr, true).c_str());
			return !objects[o].plural;
		}
		if (isNotPhysicallyPresent = (objectRole & SENTENCE_IN_REL_ROLE) != 0) // 
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s accepted for focus although SENTENCE_IN_REL.", where, section, objectString(o, tmpstr, true).c_str());
			return !objects[o].plural;
		}
		if (!objects[o].plural && (objectRole & POV_OBJECT_ROLE) && !(objectRole & EXTENDED_OBJECT_ROLE) && !(m[where].flags & cWordMatch::flagInQuestion) && m[where].objectMatches.size() <= 1)
		{
			povInSpeakerGroups.push_back(where);
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   speaker %s has pov status [2].", where, section, objectString(o, tmpstr, true).c_str());
		}
		if (m[objects[o].begin].word->first == u"each")
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s rejected for focus (each).", where, section, objectString(o, tmpstr, true).c_str());
			return false;
		}
		// in 'the answer', in 'the letter' in 'the missive' - assertion of place that is not physical
		// in it he pointed out that the Young Adventurers...
		if (notPhysicallyPresentByMissive(where))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d   object %s rejected for focus (in non-physical object/letter).", where, section, objectString(o, tmpstr, true).c_str());
			return false;
		}
		if (objects[o].firstPhysicalManifestation == -1)
			objects[o].firstPhysicalManifestation = where;
		// help in determining POV
		// is subject the first physically present speaker?  if there is no other POV since the beginning of the section, make this subject POV.
		if (!speakerGroupsEstablished && (povInSpeakerGroups.empty() || (section < sections.size() && povInSpeakerGroups[povInSpeakerGroups.size() - 1] < (signed)sections[section].endHeader)) &&
			m[where].getObject() >= 0 && !objects[m[where].getObject()].plural && objects[m[where].getObject()].objectClass != PRONOUN_OBJECT_CLASS && m[where].objectMatches.size() <= 1 &&
			// not in a compound object
			m[where].nextCompoundPartObject < 0 && m[where].previousCompoundPartObject < 0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:First present in section - %s is POV?", where, whereString(where, tmpstr, true).c_str());
		}
		return !objects[o].plural;
	}
	return false;
}

// in secondary quotes, inPrimaryQuote=false
// If o is focus-worthy and not introduced-by-reference, park it on
// nextISNarrationSubjects (IS_OBJECT already physically manifested) or
// nextNarrationSubjects (otherwise), and append it to lastSubjects (clearing
// that vector first if clearBeforeSet).  Place-IS_OBJECT subjects are
// demoted to neuter and rejected.  Returns whether this mention is a
// subject (anySubject) - used to pick whereFirstSubjectInParagraph.
bool cSource::mergeFocus(bool inPrimaryQuote, bool inSecondaryQuote, int o, int where, vector <int>& lastSubjects, bool& clearBeforeSet)
{
	LFS
		// if subject or (object of preposition "to", if mentioned previously)
		bool isNotPhysicallyPresent = false, anySubject = false;
	if (isFocus(where, inPrimaryQuote, inSecondaryQuote, o, isNotPhysicallyPresent, false) &&
		find(introducedByReference.begin(), introducedByReference.end(), o) == introducedByReference.end() &&
		(!objects[o].neuter || objects[o].male || objects[o].female))
	{
		if ((m[where].objectRole & SUBJECT_ROLE) && !(m[where].objectRole & NO_ALT_RES_SPEAKER_ROLE))
			anySubject = true;
		if (isNotPhysicallyPresent) return anySubject;
		bool uniquelyMergable;
		set <int>::iterator stsi;
		if (unMergable(where, o, tempSpeakerGroup.speakers, uniquelyMergable, false, false, false, false, stsi))
		{
			lpwstring tmpstr, tmpstr2, tmpstr3;
			vector <int>::iterator vtsi;
			if ((m[where].objectRole & (IS_OBJECT_ROLE | SUBJECT_ROLE)) == (IS_OBJECT_ROLE | SUBJECT_ROLE) && objects[o].getSubType() >= 0 && m[where].getRelObject() >= 0 &&
				m[m[where].getRelObject()].getObject() >= 0 && objects[m[m[where].getRelObject()].getObject()].getSubType() >= 0)
			{
				objects[o].male = objects[o].female = false;
				objects[o].neuter = true;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%06d:%02d   subject %d:%s%s rejected (IS_OBJECT and PLACE)", where, section, where, objectString(o, tmpstr, true).c_str(), m[where].roleString(tmpstr3).c_str());
				return false;
			}
			else if ((m[where].objectRole & IS_OBJECT_ROLE) && objects[o].firstPhysicalManifestation >= 0 && objects[o].firstPhysicalManifestation < where &&
				speakerGroups.size()>0 && speakerGroups[speakerGroups.size() - 1].speakers.find(o) == speakerGroups[speakerGroups.size() - 1].speakers.end())
			{
				bool inserted = (unMergable(where, o, nextISNarrationSubjects, uniquelyMergable, true, false, false, false, vtsi));
				if (inserted) whereNextISNarrationSubjects.push_back(where);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, u"%06d:%02d   subject %d:%s %s %s into ZXZ nextISNarrationSubjects %s%s", where, section, where, objectString(o, tmpstr, true).c_str(), m[where].roleString(tmpstr3).c_str(),
						(inserted) ? u"inserted" : u"merged", (inserted) ? objectString(nextISNarrationSubjects, tmpstr2).c_str() : objectString(*vtsi, tmpstr2, true).c_str(), (clearBeforeSet) ? u" ZXZ cleared lastSubjects beforehand" : u"");
			}
			else
			{
				// if ambiguously matched, is not plural and at least one of the matches is physically present AND if this one is NOT physically present, then reject.
				vector <cLocalFocus>::iterator lsi;
				bool atLeastOnePhysicallyPresent = false, isPhysicallyPresent = (lsi = in(o)) != localObjects.end() && lsi->physicallyPresent;
				if (m[where].objectMatches.size() > 1 && (m[where].word->second.inflectionFlags & PLURAL) != PLURAL && !isPhysicallyPresent)
				{
					for (int omi = 0; omi < (signed)m[where].objectMatches.size() && !atLeastOnePhysicallyPresent; omi++)
					{
						lsi = in(m[where].objectMatches[omi].object);
						atLeastOnePhysicallyPresent = (lsi != localObjects.end() && lsi->physicallyPresent);
					}
				}
				// They[words] were uttered by Boris and they[words] were : �QS Mr . Brown . � (Mr. Brown) should not be a narrative subject.
				if ((isPhysicallyPresent || !atLeastOnePhysicallyPresent) && !(where && (m[where - 1].flags & cWordMatch::flagQuotedString)))
				{
					bool inserted = (unMergable(where, o, nextNarrationSubjects, uniquelyMergable, true, false, false, false, vtsi));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG, u"%06d:%02d   subject %s %s %s into ZXZ nextNarrationSubjects %s%s", where, section, objectString(o, tmpstr, true).c_str(), m[where].roleString(tmpstr3).c_str(),
							(inserted) ? u"inserted" : u"merged", (inserted) ? objectString(nextNarrationSubjects, tmpstr2).c_str() : objectString(*vtsi, tmpstr2, true).c_str(), (clearBeforeSet) ? u" ZXZ cleared lastSubjects beforehand" : u"");
				}
				else
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG, u"%06d:%02d   subject %s %s REJECTED (not PP) from ZXZ nextNarrationSubjects %s", where, section, objectString(o, tmpstr, true).c_str(), m[where].roleString(tmpstr3).c_str(),
							objectString(nextNarrationSubjects, tmpstr2).c_str());
			}
		}
		// A plane swept over Ann.
		// A table fell onto him.
		if (!(m[where].objectRole & SUBJECT_ROLE) && lastSubjects.size()) return anySubject;
		objects[o].PISSubject++;
		if (clearBeforeSet)
		{
			lastSubjects.clear();
			clearBeforeSet = false;
		}
		lastSubjects.push_back(o);
	}
	return anySubject;
}

// Add a definitely-identified speaker (said X / X said) to tempSpeakerGroup
// and to the section's preIdentifiedSpeakerObjects.  Names replace a uniquely
// mergable pronoun; pronouns that cannot merge by sex with the current or
// last group are inserted as new speakers; other classes are taken from
// preIdentifiedSpeakerObjects if already known.
void cSource::mergeObjectIntoSpeakerGroup(int where, int speakerObject)
{
	LFS
		lpwstring tmpstr, tmpstr2;
	set <int>::iterator mergedSpeakerObject;
	bool uniquelyMergable;
	if (objects[speakerObject].objectClass == META_GROUP_OBJECT_CLASS && cObject::whichOrderWord(m[objects[speakerObject].originalLocation].word) != -1)
		return;
	if (objects[speakerObject].objectClass == NAME_OBJECT_CLASS ||
		objects[speakerObject].objectClass == GENDERED_GENERAL_OBJECT_CLASS ||
		objects[speakerObject].objectClass == BODY_OBJECT_CLASS ||
		objects[speakerObject].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
		objects[speakerObject].objectClass == GENDERED_DEMONYM_OBJECT_CLASS ||
		objects[speakerObject].objectClass == GENDERED_RELATIVE_OBJECT_CLASS ||
		objects[speakerObject].objectClass == META_GROUP_OBJECT_CLASS)
	{
		if (section < sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(speakerObject);
		if (unMergable(where, speakerObject, tempSpeakerGroup.speakers, uniquelyMergable, true, false, false, false, mergedSpeakerObject))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d:%02d     definite %s (1)", where, section, objectString(speakerObject, tmpstr, true).c_str());//,(isSpeakerObjectNonResolvable) ? "nonResolvable":"");
		}
		else
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d:%02d     definite %s %smergable with %s", where, section, objectString(speakerObject, tmpstr, true).c_str(), (uniquelyMergable) ? u"uniquely " : u"", objectString(*mergedSpeakerObject, tmpstr2, true).c_str());
			if (uniquelyMergable && objects[speakerObject].objectClass == NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass == PRONOUN_OBJECT_CLASS)
			{
				tempSpeakerGroup.speakers.erase(mergedSpeakerObject);
				tempSpeakerGroup.speakers.insert(speakerObject);
			}
		}
	}
	// the definite pronoun represents something that has not been seen before (a speaker referred to by a pronoun that has not been previously introduced)
	else if (objects[speakerObject].objectClass == PRONOUN_OBJECT_CLASS)
	{
		if (!mergableBySex(speakerObject, tempSpeakerGroup.speakers, uniquelyMergable, mergedSpeakerObject) &&
			(!speakerGroups.size() || speakerGroups[speakerGroups.size() - 1].section != section ||
				!mergableBySex(speakerObject, speakerGroups[speakerGroups.size() - 1].speakers, uniquelyMergable, mergedSpeakerObject)))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d:%02d     definite %s (2)", where, section, objectString(speakerObject, tmpstr, true).c_str());
			if (section < sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(speakerObject);
			tempSpeakerGroup.speakers.insert(speakerObject);
		}
		else if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d:%02d     definite %s %smergable with %s (2)", where, section, objectString(speakerObject, tmpstr, true).c_str(), (uniquelyMergable) ? u"uniquely " : u"", (mergedSpeakerObject == sNULL) ? u"" : objectString(*mergedSpeakerObject, tmpstr2, true).c_str());
	}
	else if (section < sections.size() && !unMergable(where, speakerObject, sections[section].preIdentifiedSpeakerObjects, uniquelyMergable, false, false, false, false, mergedSpeakerObject))
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%06d:%02d     definite %s unMergable discovered from predefined", where, section, objectString(*mergedSpeakerObject, tmpstr, true).c_str());
		tempSpeakerGroup.speakers.insert(*mergedSpeakerObject);
	}
	else if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"%06d:%02d     definite %s rejected (wrong class)", where, section, objectString(speakerObject, tmpstr, true).c_str());
}

// if this is punctuation, but it is actually matched by a pattern, then
//    the punctuation is really not the end of a sentence. (abbreviation)
// also if there is : followed by an paragraph end or a dash or period followed by a quote.
// True if position where is a real sentence end: ? ! ; a period that did
// not match a pattern (so not an abbreviation); a colon before a section
// word; or a dash/period immediately before a closing quote.  The "--"
// comparison is marked /*BUG*/ in the source (the token is usually an
// em-dash form, not the two-char string).
bool cSource::isEOS(int where)
{
	LFS
		vector <cWordMatch>::iterator im = m.begin() + where;
	return (im->word->first == u"?" || im->word->first == u"!" || im->word->first == u";" || (im->word->first == u"." && !im->PEMACount) ||
		(im->word->first == u":" && (im + 1)->word == Words.sectionWord) ||
		(where + 1 < (signed)m.size() && (im->queryForm(dashForm) >= 0 || im->word->first == u"--"/*BUG*/ || im->word->first == u".") &&
			(m[where + 1].word->first == u"�" || m[where + 1].word->first == u"�")));
}

// True for dummy/existential subjects "what" / "where" / "there" / "here"
// that should not be treated as speakers.  "it" and "that" are deliberately
// excluded (see the commented-out tests).
bool cSource::isPleonastic(tIWMM w)
{
	LFS
		return /*w->first==u"it" || */w->first == u"what" || w->first == u"where" || w->first == u"there" || w->first == u"here"; // || w->first==u"that"; // 'this and 'that' refers to something immediately before, and so are resolvable. || subjectWord->first==u"this" || subjectWord->first==u"that"))
}

// the following has two subchains: (Siebel and his flowers), (Faust and Mephistopheles)
// Marguerite with her box of jewels, the church scene, Siebel and his flowers, and Faust and Mephistopheles.
// True if objectPositions is a single syntactic sub-chain of a larger
// compound ("Siebel and his flowers" inside a longer list).  Used so
// grouping does not treat the whole list as one speaker set.
bool cSource::compoundObjectSubChain(vector < int >& objectPositions)
{
	LFS
		if (objectPositions.empty()) return false;
	if (m[objectPositions[0]].previousCompoundPartObject >= 0 && m[objectPositions[0]].nextCompoundPartObject < 0 && objectPositions.size() == 2)
		return true;
	if (m[objectPositions[0]].nextCompoundPartObject < 0) return false;
	int limit = m[objectPositions[0]].nextCompoundPartObject, I;
	for (I = 1; I < (signed)objectPositions.size() && objectPositions[I] < limit; I++);
	return I == objectPositions.size();
}

// Replace every BODY_OBJECT_CLASS speaker ("Tommy's voice") with its owner
// object when the owner is a single resolved match.  Ambiguous or empty
// owner matches are left as the body object.
void cSource::translateBodyObjects(cSpeakerGroup& sg)
{
	LFS
		lpwstring tmpstr;
	vector <int> translatedBodyObjects;
	for (set<int>::iterator i = sg.speakers.begin(); i != sg.speakers.end(); )
		if (objects[*i].objectClass == BODY_OBJECT_CLASS && objects[*i].getOwnerWhere() >= 0)
		{
			int o;
			if (m[objects[*i].getOwnerWhere()].getObject() < 0)
			{
				if (m[objects[*i].getOwnerWhere()].objectMatches.size() > 1 || m[objects[*i].getOwnerWhere()].objectMatches.empty())
				{
					i++;
					continue;
				}
				o = m[objects[*i].getOwnerWhere()].objectMatches[0].object;
			}
			else
				o = m[objects[*i].getOwnerWhere()].getObject();
			translatedBodyObjects.push_back(o);
			sg.speakers.erase(i++);
		}
		else
			i++;
	for (vector<int>::iterator i = translatedBodyObjects.begin(); i != translatedBodyObjects.end(); i++)
		sg.speakers.insert(*i);
}

// if the subject of the only sentence in the paragraph indicates the character of the response, then don't split speakerGroup
// If the only sentence of the next paragraph is a reply/response/answer
// attribution ("Boris replied:"), return the source position after that
// paragraph so the speaker group is not split across it.  Caches the result
// on m[I].skipResponse.  Returns -1 if this is not a meta-response (or if
// the verb has a non-"question" object, or the quote already closed).
int cSource::detectMetaResponse(int I, int element)
{
	LFS
		lpwstring tmpstr;
	if (m[I].skipResponse >= 0)
	{
		if (m[I].relSubject >= 0)
		{
			if (m[m[I].relSubject].objectMatches.size())
				objectString(m[m[I].relSubject].objectMatches, tmpstr, true);
			else
				objectString(m[m[I].relSubject].getObject(), tmpstr, true);
		}
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d-%06d:Detected meta-response [subject %d:%s].", I, m[I].skipResponse, m[I].relSubject, tmpstr.c_str());
		return m[I].skipResponse;
	}
	int where = m[I].principalWherePosition, skipResponse = I + m[I].pma[element].len;
	bool endsParagraph = false;
	while (true && skipResponse + 1 < (signed)m.size())
	{
		endsParagraph = isEOS(skipResponse) && m[skipResponse + 1].word == Words.sectionWord;
		if (endsParagraph) skipResponse += 2;
		// another voice[boris] which Tommy rather thought was that of Boris replied :
		if (!endsParagraph && (endsParagraph = m[skipResponse - 1].word->first == u":" && m[skipResponse].word == Words.sectionWord))
			skipResponse++;
		if (endsParagraph) break;
		int nextSubSentence = skipResponse;
		while (nextSubSentence + 1 < (signed)m.size() && !isEOS(nextSubSentence) && ((element = m[nextSubSentence].pma.queryPattern(u"__S1")) == -1))
			nextSubSentence++;
		if (nextSubSentence + 1 >= (signed)m.size()) return -1;
		if (isEOS(nextSubSentence)) break;
		where = m[nextSubSentence].principalWherePosition;
		skipResponse = nextSubSentence + m[nextSubSentence].pma[element & ~cMatchElement::patternFlag].len;
	}
	// tommy felt that[interference,america] Boris had shrugged his[boris] shoulders[boris] as he[boris] answered :
	if (where < 0 || !(m[where].objectRole & SUBJECT_ROLE) || (m[where].objectRole & (OBJECT_ROLE | PREP_OBJECT_ROLE)) || !endsParagraph)
		return -1;
	if (m[where].relSubject >= 0)
	{
		if (m[m[where].relSubject].objectMatches.size())
			objectString(m[m[where].relSubject].objectMatches, tmpstr, true);
		else
			objectString(m[m[where].relSubject].getObject(), tmpstr, true);
	}
	for (unsigned int J = 0; metaResponse[J]; J++)
		if (m[where].word->first == metaResponse[J])
		{
			m[I].skipResponse = skipResponse;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d-%06d:Detected meta-response (1) [subject %d:%s].", I, skipResponse, m[where].relSubject, tmpstr.c_str());
			if (m[where].relSubject >= 0 && !(m[m[where].relSubject].objectRole & NONPAST_OBJECT_ROLE))
				m[I].relSubject = m[where].relSubject;
			return skipResponse;
		}
	int whereVerb = m[where].getRelVerb();
	// if the verb of the only sentence indicates the character of the response, don't split.  If verb has an object, return false, unless the object is 'question'.
	if (whereVerb < 0 || m[whereVerb].word->second.mainEntry == wNULL) return -1;
	// Boris asked a question:
	if (m[whereVerb].getRelObject() >= 0 && m[m[whereVerb].getRelObject()].word->first != u"question") return -1;
	// �Unresolved I am not sure where she[jane] is at the present moment[moment] , � she[jane] replied .
	if (m[whereVerb].relSubject > 1 && m[m[whereVerb].relSubject - 1].word->first == u"�") return -1;
	// the Sinn feiner[irish] was speaking . his[irish] rich Irish voice[irish] was unmistakable :
	// another voice[number] , which Tommy fancied was that[number] of the tall , commanding - looking man[number] whose face[number] had seemed familiar to him[number,tommy] , said :
	// the Russian[boris] seemed to consider :
	// if subject is new, then this doesn't represent a continuation of a conversation
	//if (unResolvablePosition(I)) return -1;
	lpwstring mainEntry = m[whereVerb].word->second.mainEntry->first;
	if (m[whereVerb].relSubject >= 0)
	{
		if (m[m[whereVerb].relSubject].objectMatches.size())
			objectString(m[m[whereVerb].relSubject].objectMatches, tmpstr, true);
		else
			objectString(m[m[whereVerb].relSubject].getObject(), tmpstr, true);
	}
	for (unsigned int J = 0; metaResponse[J]; J++)
		if (mainEntry == metaResponse[J])
		{
			m[I].skipResponse = skipResponse;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d-%06d:Detected meta-response (2) [subject %d:%s].", I, skipResponse, m[whereVerb].relSubject, tmpstr.c_str());
			if (m[whereVerb].relSubject >= 0 && !(m[m[whereVerb].relSubject].objectRole & NONPAST_OBJECT_ROLE))
				m[I].relSubject = m[whereVerb].relSubject;
			return skipResponse;
		}
	return -1;
}

// If the subject at 'where' has a VerbNet "has" verb and a resolved object,
// record that object on objects[o].possessions and set flagUsedPossessionRelation.
// Skipped for ambiguous matches and probability statements.
void cSource::associatePossessions(int where)
{
	LFS
		if (m[where].objectMatches.size() > 1 || (m[where].flags & cWordMatch::flagInPStatement)) return;
	int o = m[where].getObject(), ro = m[where].getRelObject(), wv = m[where].getRelVerb(); // rs=m[where].relSubject,
	int64_t objectRole = m[where].objectRole;
	if (o >= 0 && (objectRole & SUBJECT_ROLE) && wv >= 0 && ro >= 0 && m[ro].getObject() >= 0)
	{
		lpwstring verb;
		bool has = false;
		unordered_map <lpwstring, set <int> >::iterator lvtoCi = getVerbClasses(wv, verb);
		if (lvtoCi != vbNetVerbToClassMap.end())
			for (set <int>::iterator vbi = lvtoCi->second.begin(), vbiEnd = lvtoCi->second.end(); vbi != vbiEnd; vbi++)
				has = vbNetClasses[*vbi].has;
		if (has)
		{
			m[where].flags |= cWordMatch::flagUsedPossessionRelation;
			if (m[where].objectMatches.size() == 1)
				o = m[where].objectMatches[0].object;
			objects[o].possessions.push_back(ro);
		}
	}
}

// Copy associated adjectives/nouns and generic gender from an IS_OBJECT onto
// its subject ("Whittington was a big man" / "Whittington was big"), and run
// ageDetection on age-in-years patterns.  Rejected on number mismatch,
// neuter-vs-gendered, nymNoMatch contradiction, or the wrong tense (present
// inside quotes, past outside).  Runs before object resolution, so it uses
// getObject() not objectMatches.
void cSource::associateNyms(int where)
{
	LFS
		if (m[where].objectMatches.size() > 1 || (m[where].flags & cWordMatch::flagInPStatement)) return;
	lpwstring tmpstr;
	//int o=(m[where].objectMatches.size()==1) ? m[where].objectMatches[0].object : m[where].getObject(); this routine should be used BEFORE object is resolved
	// at speakerResolution, because this is used before resolveObject the object matched is the old object from the last phase, so it is invalid.
	int o = m[where].getObject(), wv = m[where].getRelVerb(), tsSense = (wv >= 0) ? m[wv].verbSense : 0; // rs=m[where].relSubject,ro=m[where].getRelObject(),
	int64_t objectRole = m[where].objectRole, ror = -1;
	// vS                               simple examine                     VT_PRESENT                                          R=E=S
	// vS+past                          examined                           VT_PAST                                             R=E<S
	// vB                               has examined                       VT_PRESENT_PERFECT                                  E<S=R
	// vC                               is examining                       VT_EXTENDED+ VT_PRESENT                             S=R E<=>R
	// vC+past													was examining                      VT_EXTENDED+ VT_PAST                                R=E<S
	// vBC                              has been examining                 VT_EXTENDED+ VT_PRESENT_PERFECT                     E<S=R
	// vD                               is examined                        VT_PASSIVE+ VT_PRESENT
	// vD+past													was examined                       VT_PASSIVE+ VT_PAST
	// vBD                              has been examined                  VT_PASSIVE+ VT_PRESENT_PERFECT
	// vCD                              is being examined                  VT_PASSIVE+ VT_PRESENT+VT_EXTENDED
	// vCD+past                         was being examined                 VT_PASSIVE+ VT_PAST+VT_EXTENDED
	// vBCD                             has been being examined            VT_PASSIVE+ VT_PRESENT_PERFECT+VT_EXTENDED
	// neither talking about the past (he had been) nor about the future (he will be)
	bool nymIsUseful = !(tsSense & (VT_POSSIBLE | VT_NEGATION)) &&
		((objectRole & IN_PRIMARY_QUOTE_ROLE) ? ((tsSense & VT_TENSE_MASK) == VT_PRESENT || (tsSense & VT_TENSE_MASK) == VT_PRESENT_PERFECT) : ((tsSense & VT_TENSE_MASK) == VT_PAST));
	// collect adjectives using is-a relation
	// Whittington was a big man
	// subject can also be object of containing sentence, so we must make sure that the subject of the contained sentence is not an IS object of the containing sentence.
	if (o >= 0 && (objectRole & (SUBJECT_ROLE | IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) == (SUBJECT_ROLE | IS_OBJECT_ROLE) && m[where].getRelObject() >= 0 &&
		((ror = m[m[where].getRelObject()].objectRole) & (OBJECT_ROLE | IS_OBJECT_ROLE | NOT_OBJECT_ROLE)) == (OBJECT_ROLE | IS_OBJECT_ROLE) &&
		(!(objectRole & OBJECT_ROLE) || m[where].relSubject < 0 || !(m[m[where].relSubject].objectRole & IS_OBJECT_ROLE)))
	{
		if (m[m[where].getRelObject()].getObject() >= 0)
		{
			if ((objects[o].plural ^ objects[m[m[where].getRelObject()].getObject()].plural) &&
				m[where].word->first != u"you" && m[m[where].getRelObject()].word->first != u"you")
			{
				lpwstring tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:objects %s and %s differ in number - adjectival association rejected.", where, whereString(m[where].getRelObject(), tmpstr, true).c_str(), objectString(o, tmpstr2, true).c_str());
				return;
			}
			int ao = (m[m[where].getRelObject()].objectMatches.size() == 1) ? m[m[where].getRelObject()].objectMatches[0].object : m[m[where].getRelObject()].getObject();
			// Julius was a great deal younger
			// 'great' does not apply to Julius, only to 'deal'
			// also may reject possible aliases ('a hustler')
			if (objects[ao].neuter && !objects[o].neuter && objects[o].objectClass != BODY_OBJECT_CLASS)
			{
				lpwstring tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:Neuter object %s.  Adjectives associated with %s are rejected.", where, objectString(ao, tmpstr, true).c_str(), objectString(o, tmpstr2, true).c_str());
				return;
			}
			lpwstring logMatch;
			tIWMM fromMatch, toMatch, toMapMatch;
			if (nymNoMatch(where, objects.begin() + o, objects.begin() + ao, true, false, logMatch, fromMatch, toMatch, toMapMatch, u"NoMatchSelf"))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:Contradictory (2) object %s (on %s)!", where, objectString(o, tmpstr, true).c_str(), logMatch.c_str());
				return;
			}
			if ((objects[ao].associatedNouns.size() || objects[ao].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
			{
				lpwstring nouns, adjectives;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:Object %s original associated nouns (%s) and adjectives (%s) taking from %d:%s (3)",
						where, objectString(o, tmpstr, false).c_str(), wordString(objects[o].associatedNouns, nouns).c_str(), wordString(objects[o].associatedAdjectives, adjectives).c_str(),
						m[where].getRelObject(), objectString(ao, tmpstr, false).c_str());
			}
			narrowGender(m[where].getRelObject(), o);
			m[where].flags |= cWordMatch::flagUsedBeRelation;
			if (m[where].getRelVerb() >= 0)
				m[m[where].getRelVerb()].flags |= cWordMatch::flagUsedBeRelation;
			for (vector <tIWMM>::iterator ai = objects[ao].associatedAdjectives.begin(), aiEnd = objects[ao].associatedAdjectives.end(); ai != aiEnd; ai++)
				if (find(objects[o].associatedAdjectives.begin(), objects[o].associatedAdjectives.end(), *ai) == objects[o].associatedAdjectives.end())
					objects[o].associatedAdjectives.push_back(*ai);
			for (vector <tIWMM>::iterator ai = objects[ao].associatedNouns.begin(), aiEnd = objects[ao].associatedNouns.end(); ai != aiEnd; ai++)
				if (find(objects[o].associatedNouns.begin(), objects[o].associatedNouns.end(), *ai) == objects[o].associatedNouns.end())
					objects[o].associatedNouns.push_back(*ai);
			if (!(m[objects[o].originalLocation].word->second.flags & cSourceWordInfo::genericGenderIgnoreMatch))
				objects[o].updateGenericGender(where, m[objects[ao].originalLocation].word, objects[ao].objectGenericAge, u"associateNyms", debugTrace);
			if ((objects[ao].associatedNouns.size() || objects[ao].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
			{
				lpwstring nouns, adjectives;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:Object %s associated nouns (%s) and adjectives (%s) (3)",
						where, objectString(o, tmpstr, false).c_str(), wordString(objects[ao].associatedNouns, nouns).c_str(), wordString(objects[ao].associatedAdjectives, adjectives).c_str());
			}
			// He[Boris] was probably fifty years old
			//   ALLOBJECTS_1 (old is __ADJECTIVE_WITHOUT_VERB) - about [prep] fifty [card] years [noun] old [adj]
			// He[Boris] was probably fifty years of age
			//   ALLOBJECTS_1 fifty [card] years [noun] of [prep] age [noun]
			ageDetection(where, o, ao);
		}
	}
	// Whittington was big.
	// He is very well off.
	if (o >= 0 && (objectRole & (SUBJECT_ROLE | IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) == (SUBJECT_ROLE | IS_OBJECT_ROLE) && m[where].getRelObject() < 0 && nymIsUseful)
	{
		m[where].flags |= cWordMatch::flagUsedBeRelation;
		for (unsigned int aow = m[where].getRelVerb() + 1; aow < m.size() && (m[aow].objectRole & IS_ADJ_OBJECT_ROLE); aow++)
			if (m[aow].queryWinnerForm(adjectiveForm) >= 0 && find(objects[o].associatedAdjectives.begin(), objects[o].associatedAdjectives.end(), m[aow].word) == objects[o].associatedAdjectives.end() &&
				!nymNoMatch(objects.begin() + o, m[aow].word))
			{
				objects[o].associatedAdjectives.push_back(m[aow].word);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:Object %s associated adjective (%s) (1)", where, objectString(o, tmpstr, false).c_str(), m[aow].word->first.c_str());
			}
		// look for expressions
		for (unsigned int aow = m[where].getRelVerb() + 1; aow + 1 < m.size() && (m[aow + 1].objectRole & IS_ADJ_OBJECT_ROLE); aow++)
		{
			lpwstring word = m[aow].word->first + u"_" + m[aow + 1].word->first;
			unordered_set <lpwstring> synonyms;
			getSynonyms(word, synonyms, ADJ);
			for (auto s = synonyms.begin(), sEnd = synonyms.end(); s != sEnd; s++)
			{
				tIWMM ws = Words.query(*s);
				if (ws != Words.end() && find(objects[o].associatedAdjectives.begin(), objects[o].associatedAdjectives.end(), ws) == objects[o].associatedAdjectives.end() &&
					!nymNoMatch(objects.begin() + o, ws))
				{
					objects[o].associatedAdjectives.push_back(ws);
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, u"%06d:Object %s associated multi-word adjective (%s) from %s (2)", where, objectString(o, tmpstr, false).c_str(), ws->first.c_str(), word.c_str());
				}
			}
			if (synonyms.size()) aow++;
		}
		// He[Boris] was probably about fifty years of age 
		//   ALLOBJECTS_0 - probably [adv] about [prep] fifty [card] years [noun] of [prep] age [noun]
		if (m[where].getRelVerb() >= 0 && m[m[where].getRelVerb()].relPrep >= 0 && m[m[m[where].getRelVerb()].relPrep].getRelObject() >= 0 && m[m[m[m[where].getRelVerb()].relPrep].getRelObject()].getObject() >= 0 &&
			(m[m[m[m[where].getRelVerb()].relPrep].getRelObject()].word->second.timeFlags & T_LENGTH))
			ageDetection(where, o, m[m[m[m[where].getRelVerb()].relPrep].getRelObject()].getObject());
	}
}

// recognize environmentally implicit objects
// True for environmentally implicit subjects that should not stay neuter:
// "another knock", "the front door bell rang".  The caller then flips the
// object to gendered-general so it can later resolve to a person.
bool cSource::implicitObject(int where)
{
	LFS
		if (where >= 0 && m[where].getObject() >= 0 && ((m[where].objectRole & SUBJECT_ROLE) || (m[where].objectRole & (IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) == (IS_OBJECT_ROLE | SUBJECT_PLEONASTIC_ROLE)) &&
			unResolvablePosition(m[where].beginObjectPosition))
		{
			const lpchar_t* implicitObjects[] = { u"knock",NULL };
			for (unsigned int J = 0; implicitObjects[J]; J++)
				if (m[where].word->second.mainEntry != wNULL && m[where].word->second.mainEntry->first == implicitObjects[J])
					return true;
		}
	// the front door bell rang
	if (where >= 0 && m[where].getObject() >= 0 && m[where].word->first == u"bell" && m[where].getRelVerb() >= 0 && m[m[where].getRelVerb()].getMainEntry()->first == u"ring" &&
		m[where - 1].word->first == u"door")
	{
		m[where].objectRole |= UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
		m[where].flags |= cWordMatch::flagObjectResolved;
		return true;
	}
	return false;
}

// Format povInSpeakerGroups[startPOVI, povi) as a space-separated index
// list for LOG_RESOLUTION.  If the range is empty it still prints
// povInSpeakerGroups[startPOVI]
const lpchar_t* intString(int startPOVI, int povi, vector <int>& povInSpeakerGroups, lpwstring& tmpstr)
{
	LFS
		tmpstr.clear();
	lpwstring tmp;
	for (int I = startPOVI; I < povi; I++)
		tmpstr += itos(povInSpeakerGroups[I], tmp) + u" ";
	if (tmpstr.empty() && startPOVI < (signed)povInSpeakerGroups.size())
		tmpstr = itos(povInSpeakerGroups[startPOVI], tmp);
	return tmpstr.c_str();
}

// Consume povInSpeakerGroups entries that fall inside speakerGroups[sgi]
// and insert the matching speaker into sg.povSpeakers.  Ambiguous
// objectMatches are accepted only when that gender is unique in the group,
// or remembered from the previous group's single povSpeaker.
void cSource::dropPOVIntoSpeakerGroup(const int sgi, int &povi, const int maleSpeakers, const int femaleSpeakers)
{
	lpwstring tmpstr, tmpstr2, tmpstr3;
	int mi;
	int startPOVI = povi;
	set <int> povAmbiguousSpeakers;
	while (povi < (signed)povInSpeakerGroups.size() && (mi = povInSpeakerGroups[povi]) < speakerGroups[sgi].sgEnd)
	{
		int o = m[mi].getObject();
		bool found = speakerGroups[sgi].speakers.find(o) != speakerGroups[sgi].speakers.end();
		int povAS = -1;
		for (unsigned int I = 0; !found && I < m[mi].objectMatches.size(); I++)
			if (speakerGroups[sgi].speakers.find(o = m[mi].objectMatches[I].object) != speakerGroups[sgi].speakers.end() &&
				!objects[o].plural && (objects[o].male ^ objects[o].female) &&
				!(found = (objects[o].male && maleSpeakers == 1) || (objects[o].female && femaleSpeakers == 1)))
			{
				int oldPOV = -1;
				if (povAS >= 0 && (!sgi || speakerGroups[sgi - 1].povSpeakers.size() != 1 ||
					((oldPOV = *speakerGroups[sgi - 1].povSpeakers.begin()) != o && oldPOV != povAS)))
				{
					povAS = -1;
					break;
				}
				if (povAS < 0 || oldPOV < 0 || oldPOV != povAS)
					povAS = o;
			}
		// if povSpeaker is not certain (it is matched to the object, rather than being the object)
		// check if the object being matched is singular gendered, and if there are no other objects of that
		// gender in the speakerGroup.  If so, it is not ambiguous.
		if (!found && povAS >= 0)
		{
			if (sgi && speakerGroups[sgi - 1].povSpeakers.find(povAS) == speakerGroups[sgi - 1].povSpeakers.end() && speakerGroups[sgi - 1].povSpeakers.size())
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:ambiguous pov speaker %s in speakerGroup %s rejected (change in pov).", mi, objectString(povAS, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
			}
			else
				povAmbiguousSpeakers.insert(povAS);
		}
		if (found)
		{
			speakerGroups[sgi].povSpeakers.insert(o);
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:POVI speaker %s (1) inserted into speakerGroup %s.", mi, objectString(o, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
		}
		povi++;
	}
	// if there were ambiguous speakers and no nonambiguous ones
	if (povAmbiguousSpeakers.size() && speakerGroups[sgi].povSpeakers.empty())
	{
		bool allIn, oneIn;
		if (povAmbiguousSpeakers.size() == 1)
		{
			// if there is only one ambiguous speaker (s1), get the gender, and go back to the last povSpeaker with the same gender, in the same section.
			// if that povSpeaker is in the current speaker group, make it the povSpeaker instead.
			for (int I = sgi - 1; I >= 0; I--)
				if (speakerGroups[I].povSpeakers.size())
				{
					if (speakerGroups[I].povSpeakers.size() == 1 && objects[*speakerGroups[I].povSpeakers.begin()].matchGender(objects[*povAmbiguousSpeakers.begin()]) &&
						speakerGroups[sgi].speakers.find(*speakerGroups[I].povSpeakers.begin()) != speakerGroups[sgi].speakers.end())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, u"%s:POVI speaker %s remembered and inserted into speakerGroup %s.", intString(startPOVI, povi, povInSpeakerGroups, tmpstr3), objectString(speakerGroups[I].povSpeakers, tmpstr).c_str(), toText(speakerGroups[sgi], tmpstr2));
						speakerGroups[sgi].povSpeakers = speakerGroups[I].povSpeakers;
						break;
					}
				}
			if (speakerGroups[sgi].povSpeakers.empty())
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%s:POVI speaker %s (2) inserted into speakerGroup %s.", intString(startPOVI, povi, povInSpeakerGroups, tmpstr3), objectString(povAmbiguousSpeakers, tmpstr).c_str(), toText(speakerGroups[sgi], tmpstr2));
				speakerGroups[sgi].povSpeakers = povAmbiguousSpeakers;
			}
		}
		// at least one but not all ambiguous speakers were found in the previous speaker group
		else if (sgi && intersect(speakerGroups[sgi - 1].speakers, povAmbiguousSpeakers, allIn, oneIn) && !allIn && oneIn)
		{
			for (set <int>::iterator si = speakerGroups[sgi - 1].speakers.begin(), siEnd = speakerGroups[sgi - 1].speakers.end(); si != siEnd; si++)
				if (povAmbiguousSpeakers.find(*si) != povAmbiguousSpeakers.end())
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, u"%s:POVI speaker %s (3) inserted into speakerGroup %s.", intString(startPOVI, povi, povInSpeakerGroups, tmpstr3), objectString(*si, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
					speakerGroups[sgi].povSpeakers.insert(*si);
				}
		}
	}
	/* this leads to Boris having POVI incorrectly @22965
	if (speakerGroups[sgi].povSpeakers.empty() && speakerGroups[sgi].speakers.size()==1 && conversationalQuotes==0 &&
			(!sgi || speakerGroups[sgi-1].povSpeakers.empty() || speakerGroups[sgi-1].povSpeakers==speakerGroups[sgi].speakers) &&
			(!sgi || speakerGroups[sgi-1].observers.empty() || find(speakerGroups[sgi-1].observers.begin(),speakerGroups[sgi-1].observers.end(),*speakerGroups[sgi].speakers.begin())!=speakerGroups[sgi-1].observers.end()))
	{
		speakerGroups[sgi].povSpeakers.insert(speakerGroups[sgi].speakers.begin(),speakerGroups[sgi].speakers.end());
		lplog(LOG_RESOLUTION,u"%s:POVI speakers (4) previous speakerGroup %s.",intString(startPOVI,povi,povInSpeakerGroups,tmpstr3),toText(speakerGroups[sgi-1],tmpstr2));
		lplog(LOG_RESOLUTION,u"%s:POVI speakers (4) inserted into speakerGroup %s.",intString(startPOVI,povi,povInSpeakerGroups,tmpstr3),toText(speakerGroups[sgi],tmpstr2));
	}
	*/
}

// Consume definitelyIdentifiedAsSpeakerInSpeakerGroups entries inside
// speakerGroups[sgi] into sg.dnSpeakers.  A brand-new name mentioned only
// inside quotes may replace the group's single non-name speaker
// (replaceObjectInSection + flagUnresolvableObjectResolvedThroughSpeakerGroup).
void cSource::dropDefinitelyIdentifiedSpeakersIntoSpeakerGroup(const int sgi, int &dni, const int maleSpeakers, const int femaleSpeakers)
{
	int mi;
	while (dni < (signed)definitelyIdentifiedAsSpeakerInSpeakerGroups.size() && (mi = definitelyIdentifiedAsSpeakerInSpeakerGroups[dni]) < speakerGroups[sgi].sgEnd)
	{
		lpwstring tmpstr, tmpstr2, tmpstr3;
		int o = m[mi].getObject();
		bool found = speakerGroups[sgi].speakers.find(o) != speakerGroups[sgi].speakers.end(), ambiguous = false;
		if (!found && m[mi].objectMatches.size() == 1 &&
			(speakerGroups[sgi].speakers.find(o = m[mi].objectMatches[0].object) != speakerGroups[sgi].speakers.end()) &&
			!objects[o].plural && (objects[o].male ^ objects[o].female) &&
			!(found = (objects[o].male && maleSpeakers == 1) || (objects[o].female && femaleSpeakers == 1)) && debugTrace.traceSpeakerResolution)
		{
			if (objects[m[mi].getObject()].objectClass != GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS) // must set 'found' - something that matches occupation is much less likely to be ambiguous
			{
				lplog(LOG_RESOLUTION, u"%06d:ambiguous dn speaker %s rejected from speakerGroup %s.", mi, objectString(o, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
				ambiguous = true;
			}
			else
				found = true;
		}
		if (!found && o >= 0 && objects[o].objectClass == BODY_OBJECT_CLASS && objects[o].getOwnerWhere() >= 0)
		{
			int oo = m[objects[o].getOwnerWhere()].getObject(), oow;
			found = speakerGroups[sgi].speakers.find(oo) != speakerGroups[sgi].speakers.end();
			if (!found && oo >= 0 && (oow = objects[oo].getOwnerWhere()) >= 0 && m[oow].objectMatches.size() == 1 &&
				(found = speakerGroups[sgi].speakers.find(m[oow].objectMatches[0].object) != speakerGroups[sgi].speakers.end()))
				o = m[oow].objectMatches[0].object;
		}
		// Ivan - a new name only mentioned in quotes.  Also not mergable to any previous name
		if (!found && o >= 0 && !ambiguous && objects[o].objectClass == NAME_OBJECT_CLASS && !objects[o].plural && (objects[o].male ^ objects[o].female) && objects[o].getSubType() == -1 &&
			objects[o].originalLocation > speakerGroups[sgi].sgBegin)
		{
			int nonNameObjectFound = -1, numFound = 0;
			// search for a non-name object this name object could be
			for (set <int>::iterator si = speakerGroups[sgi].speakers.begin(), siEnd = speakerGroups[sgi].speakers.end(); si != siEnd; si++)
				if (objects[*si].objectClass != NAME_OBJECT_CLASS)
				{
					nonNameObjectFound = *si;
					numFound++;
				}
			if (numFound == 1 && speakerGroups[sgi].metaNameOthers.find(o) == speakerGroups[sgi].metaNameOthers.end() && sections.size())
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:new dn speaker %s - replacing %s in speakerGroup %s", mi, objectString(o, tmpstr, false).c_str(), objectString(nonNameObjectFound, tmpstr2, false).c_str(), toText(speakerGroups[sgi], tmpstr3));
				for (section = 0; section + 1 < sections.size() && (signed)sections[section + 1].begin < mi; section++);
				currentSpeakerGroup = sgi;
				replaceObjectInSection(mi, o, nonNameObjectFound, u"new name mentioned only in quotes");
				int beginLimit = sections[section].begin, untilLimit = sections[section + 1].begin;
				for (vector <cObject::cLocation>::iterator li = objects[nonNameObjectFound].locations.begin(), liEnd = objects[nonNameObjectFound].locations.end(); li != liEnd; li++)
					if (li->at >= beginLimit && li->at <= untilLimit && m[li->at].getObject() == nonNameObjectFound && !(m[li->at].flags & cWordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup))
					{
						m[li->at].flags |= cWordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup;
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_SG, u"%06d:flagUnresolvableObjectResolvedThroughSpeakerGroup:%s", li->at, objectString(o, tmpstr2, true).c_str());
						if (m[li->at].objectMatches.empty())
						{
							m[li->at].objectMatches.push_back(cOM(o, SALIENCE_THRESHOLD));
							objects[o].locations.push_back(li->at);
						}
					}
			}
			dni++;
			continue;
		}
		if (found)
		{
			speakerGroups[sgi].dnSpeakers.insert(o);
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:dn speaker %s found in speakerGroup %s.", mi, objectString(o, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
		}
		else if (!ambiguous && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:dn speaker %s rejected from speakerGroup %s.", mi, objectString(o, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
		dni++;
	}
}

// An observer is a POV speaker who is not a definite speaker, was not a
// speaking member of the previous conversational group, and is not an
// "uncertain" extra in a 3+ cast (unless they are a chase/follow follower).
// Sets isAnyObserverSpeakerNew / isAnyNonObserverSpeakerNew vs. the previous
// group; nonObserver is the last non-observer when conversationRestricted.
void cSource::determineObserverStatus(const int sgi, int &nonObserver, const bool conversationRestricted, bool &isAnyNonObserverSpeakerNew, bool &isAnyObserverSpeakerNew)
{
	lpwstring tmpstr, tmpstr2;
	vector <cSyntacticRelationGroup>::iterator location = findSyntacticRelationGroup((sgi > 0) ? speakerGroups[sgi - 1].sgBegin : speakerGroups[sgi].sgBegin);
	for (set <int>::iterator mo = speakerGroups[sgi].speakers.begin(), moEnd = speakerGroups[sgi].speakers.end(); mo != moEnd; mo++)
	{
		bool isPOV = speakerGroups[sgi].povSpeakers.find(*mo) != speakerGroups[sgi].povSpeakers.end();
		bool isDN = speakerGroups[sgi].dnSpeakers.find(*mo) != speakerGroups[sgi].dnSpeakers.end();
		bool isPreviousObserver = sgi && find(speakerGroups[sgi - 1].observers.begin(), speakerGroups[sgi - 1].observers.end(), *mo) != speakerGroups[sgi - 1].observers.end();
		// speaker part of previous speakerGroup and conversationalQuotes
		bool isPreviousSpeaker = sgi && speakerGroups[sgi - 1].speakers.find(*mo) != speakerGroups[sgi - 1].speakers.end() && speakerGroups[sgi - 1].conversationalQuotes && !isPreviousObserver;
		bool allIn, oneIn, currentSpeakerGroupSuperGroupToPrevious = sgi && intersect(speakerGroups[sgi - 1].speakers, speakerGroups[sgi].speakers, allIn, oneIn) && allIn;
		// follower? - search for hasSyntacticRelationGroup 'follow'
		bool follower = false;
		for (vector <cSyntacticRelationGroup>::iterator li = location; li != syntacticRelationGroups.end() && li->where < speakerGroups[sgi].sgEnd && !follower; li++)
			follower = (in(*mo, li->whereSubject) && li->whereVerb >= 0 && isVerbClass(li->whereVerb, u"chase"));
		// if there is more than one conversational quote, and there is only one definitively named speaker, and there are more than two speakers, and
		//   the speaker was not an observer in a pervious group, then speaker is not an observer (because we don't know whether the other speaker is actually an observer)
		bool uncertainObserver = speakerGroups[sgi].conversationalQuotes && speakerGroups[sgi].dnSpeakers.size() <= 1 && speakerGroups[sgi].speakers.size() > 2 && !isPreviousObserver;
		// if the speaker was not an observer in previous group and dnSpeakers is empty, then speaker is not an observer
		if (isPOV && !isDN && (!speakerGroups[sgi].dnSpeakers.empty() || isPreviousObserver) && !(isPreviousSpeaker && currentSpeakerGroupSuperGroupToPrevious) && (!uncertainObserver || follower))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"OBS observer %s found in speakerGroup %s.", objectString(*mo, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
			speakerGroups[sgi].observers.insert(*mo);
			if (sgi)
				isAnyObserverSpeakerNew |= speakerGroups[sgi - 1].speakers.find(*mo) == speakerGroups[sgi - 1].speakers.end();
		}
		// is this non-observer speaker new?
		else if (sgi && conversationRestricted)
		{
			isAnyNonObserverSpeakerNew |= speakerGroups[sgi - 1].speakers.find(*mo) == speakerGroups[sgi - 1].speakers.end();
			nonObserver = *mo;
		}
	}
}

// True if this group's observers are the same set as the previous group's
// (and, when conversationRestricted with a single continuing observer, that
// observer was also present in the last group that contained nonObserver).
bool cSource::determineObserverContinuing(const int sgi, const int nonObserver, const bool conversationRestricted)
{
	bool observerContinuing = sgi && speakerGroups[sgi].observers == speakerGroups[sgi - 1].observers;
	if (conversationRestricted && speakerGroups[sgi].observers.size() == 1 && observerContinuing && nonObserver >= 0)
	{
		// get last speakerGroup of non-observer that contains observer
		observerContinuing = false;
		for (int sgi2 = sgi - 1; sgi2 >= 0; sgi2--)
			if (speakerGroups[sgi2].speakers.find(nonObserver) != speakerGroups[sgi2].speakers.end())
			{
				if (observerContinuing = speakerGroups[sgi].observers == speakerGroups[sgi2].observers)
					break;
				if (speakerGroups[sgi2].speakers.find(*speakerGroups[sgi].observers.begin()) != speakerGroups[sgi2].speakers.end())
					break;
			}
	}
	return observerContinuing;
}

// also an observer could be the observer of the previous group, who also doesn't speak in the current group, and there are no other povSpeakers of the current group.
// also a section cannot be crossed with this assumption.
// If this group has no POV yet, copy previous-group observers who are still
// speakers here (but not dnSpeakers) into observers and povSpeakers.  Same
// section only - observers do not cross chapter boundaries.
void cSource::addPreviousObserver(const int sgi, const bool previousSpeakerGroupInSameSection)
{
	if (speakerGroups[sgi].povSpeakers.empty() && sgi && speakerGroups[sgi - 1].observers.size())
	{
		// both speakergroups must belong to the same section
		if (previousSpeakerGroupInSameSection)
		{
			for (set <int>::iterator mo = speakerGroups[sgi - 1].observers.begin(), moEnd = speakerGroups[sgi - 1].observers.end(); mo != moEnd; mo++)
			{
				// if not a definite speaker, but is a speaker, and is not already an observer
				if (speakerGroups[sgi].dnSpeakers.find(*mo) == speakerGroups[sgi].dnSpeakers.end() &&
					speakerGroups[sgi].speakers.find(*mo) != speakerGroups[sgi].speakers.end() &&
					find(speakerGroups[sgi].observers.begin(), speakerGroups[sgi].observers.end(), *mo) == speakerGroups[sgi].observers.end())
				{
					lpwstring tmpstr, tmpstr2;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, u"POVI OBS observer %s found in speakerGroup %s (2).", objectString(*mo, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
					speakerGroups[sgi].observers.insert(*mo);
					speakerGroups[sgi].povSpeakers.insert(*mo);
				}
			}
		}
	}
}

// If this group has no POV yet and every previous-group povSpeaker is still
// a speaker here, inherit that POV set.  Those inherited POV speakers who
// are not dnSpeakers become observers, unless the single observer is also
// in groupedSpeakers (then observers are cleared).
void cSource::addPreviousPOV(const int sgi, const bool previousSpeakerGroupInSameSection)
{
	// also a pov could be the pov of the previous group, who also doesn't speak in the current group, and there are no other povSpeakers of the current group.
// also all the povSpeakers must be also of the current group.
	if (speakerGroups[sgi].povSpeakers.empty() && sgi && speakerGroups[sgi - 1].povSpeakers.size() && previousSpeakerGroupInSameSection)
	{
		lpwstring tmpstr, tmpstr2;
		// all the previous povSpeakers also have to be in the current group
		bool oneIn, allIn;
		intersect(speakerGroups[sgi - 1].povSpeakers, speakerGroups[sgi].speakers, allIn, oneIn);
		if (allIn)
		{
			speakerGroups[sgi].povSpeakers = speakerGroups[sgi - 1].povSpeakers;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"pov speakers from %s moved forward into speakerGroup %s.", toText(speakerGroups[sgi - 1], tmpstr), toText(speakerGroups[sgi], tmpstr2));
			// if there are no definite speakers in speakerGroup, then the povSpeaker would probably NOT be the one not speaking, so skip this observer test
			if (speakerGroups[sgi].dnSpeakers.size() || !speakerGroups[sgi].conversationalQuotes)
			{
				for (set <int>::iterator mo = speakerGroups[sgi].povSpeakers.begin(), moEnd = speakerGroups[sgi].povSpeakers.end(); mo != moEnd; mo++)
				{
					// if not a definite speaker, but is a speaker, and is not already an observer
					if (speakerGroups[sgi].dnSpeakers.find(*mo) == speakerGroups[sgi].dnSpeakers.end())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, u"OBS observer %s derived from pov in previous speakerGroup in speakerGroup %s (3).", objectString(*mo, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
						speakerGroups[sgi].observers.insert(*mo);
					}
				}
				if (speakerGroups[sgi].observers.size() == 1 && speakerGroups[sgi].groupedSpeakers.find(*speakerGroups[sgi].observers.begin()) != speakerGroups[sgi].groupedSpeakers.end())
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, u"OBS observer %s derived from pov in previous speakerGroup in speakerGroup %s CANCELLED (in group).", objectString(speakerGroups[sgi].observers, tmpstr).c_str(), toText(speakerGroups[sgi], tmpstr2));
					speakerGroups[sgi].observers.clear();
				}
			}
		}
	}
}

// If some but not all observers sit in groupedSpeakers and at least one
// grouped speaker is a dnSpeaker, drop the observers that are in the
// speaking subgroup (they were talking, not watching).
void cSource::removeObserverAssociatedWithSpeakingGroup(const int sgi)
{
	// if an observer belongs to a group and that any of the group definitely speaks, then remove the observer. (error check)
	bool allObserversInGroup = true, oneObserverInGroup;
	// are observers grouped?
	intersect(speakerGroups[sgi].groupedSpeakers, speakerGroups[sgi].observers, allObserversInGroup, oneObserverInGroup);
	// if at least one observer is in a group, but not all the observers are in the group
	if (oneObserverInGroup && !allObserversInGroup)
	{
		bool allGroupedAreDefiniteSpeakers, oneGroupedIsDefiniteSpeaker;
		// if any of the groupedSpeakers is a definite speaker, then erase all observers that are in the group.
		intersect(speakerGroups[sgi].groupedSpeakers, speakerGroups[sgi].dnSpeakers, allGroupedAreDefiniteSpeakers, oneGroupedIsDefiniteSpeaker);
		if (oneGroupedIsDefiniteSpeaker)
			for (set <int>::iterator mo = speakerGroups[sgi].observers.begin(); mo != speakerGroups[sgi].observers.end(); )
			{
				if (speakerGroups[sgi].groupedSpeakers.find(*mo) == speakerGroups[sgi].groupedSpeakers.end())
					mo++;
				else
				{
					lpwstring tmpstr, tmpstr2;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, u"observer %s erased from speakerGroup %s [observer is in speaking group].", objectString(*mo, tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
					speakerGroups[sgi].observers.erase(mo++);
				}
			}
	}
}

// Count quotes in this group that are not self-addressed (speaker !=
// audience) by walking the nextQuote chain from currentQuote.  Writes
// sg.conversationalQuotes and advances currentQuote past sgEnd.
void cSource::setConversationalQuotes(const int sgi, int &currentQuote)
{
	int conversationalQuotes = 0;
	while (currentQuote >= 0 && currentQuote < speakerGroups[sgi].sgEnd)
	{
		// determine whether the speaker was just talking to him/herself
		if (m[currentQuote].objectMatches.size() != 1 || m[currentQuote].audienceObjectMatches.size() != 1 ||
			m[currentQuote].objectMatches[0].object != m[currentQuote].audienceObjectMatches[0].object)
			conversationalQuotes++;
		currentQuote = m[currentQuote].nextQuote;
	}
	speakerGroups[sgi].conversationalQuotes = conversationalQuotes;
}

// distribute people named as others in conversations (to lessen the risk of named supposedly hailed objects actually being talked about, rather than physically there)
// Drop every metaNameOthersInSpeakerGroups position into the speaker group
// whose span contains it.  Those names were talked about, not hailed, and
// resolveSpeakers must not treat them as physically present addressees.
void cSource::distributeMetaNameOthers()
{
	int sgi = 0, mi;
	lpwstring tmpstr, tmpstr2;
	for (unsigned int I = 0; I < metaNameOthersInSpeakerGroups.size() && sgi < (int)speakerGroups.size(); sgi++)
		for (; I < (signed)metaNameOthersInSpeakerGroups.size() && (mi = metaNameOthersInSpeakerGroups[I]) < speakerGroups[sgi].sgEnd; I++)
		{
			speakerGroups[sgi].metaNameOthers.insert(m[mi].getObject());
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:MNO speaker %s inserted into speakerGroup %s.", mi, objectString(m[mi].getObject(), tmpstr, true).c_str(), toText(speakerGroups[sgi], tmpstr2));
		}
}

// distribute povInSpeakerGroups and definitelyIdentifiedAsSpeakerInSpeakerGroups
// to povSpeakers and dnSpeakers in speakerGroups.
// Post-pass over every speaker group: fill conversationalQuotes, povSpeakers,
// dnSpeakers, observers.  A two-speaker conversational group that was split
// off only because of an observer is merged back with the previous group;
// otherwise observers are cleared under the conversation-restricted rule.
// Lonely singleton casts, or casts containing Narrator (object 0), get a
// default POV if none was found.
void cSource::distributePOV()
{
	LFS
	int povi = 0, dni = 0, sgi = 0, currentQuote = firstQuote;
	distributeMetaNameOthers();
	lpwstring tmpstr, tmpstr2, tmpstr3;
	while (sgi < (signed)speakerGroups.size())
	{
		int maleSpeakers = 0, femaleSpeakers = 0;
		for (set <int>::iterator si = speakerGroups[sgi].speakers.begin(), siEnd = speakerGroups[sgi].speakers.end(); si != siEnd; si++)
			if (objects[*si].male)
				maleSpeakers++;
			else if (objects[*si].female)
				femaleSpeakers++;
		setConversationalQuotes(sgi, currentQuote);
		dropPOVIntoSpeakerGroup(sgi, povi, maleSpeakers, femaleSpeakers);
		dropDefinitelyIdentifiedSpeakersIntoSpeakerGroup(sgi, dni, maleSpeakers, femaleSpeakers);
		// an observer is one that is mentioned as having an internal state, but doesn't speak.

		// if the speakerGroup has only two speakers, there is a conversation, and the observer is the only new speaker,
		// add the previous speakers to this speakerGroup (new speakerGroup was created by mistake - the observer triggered a new speakerGroup)
		// if there are only two speakers, and a conversation has taken place, and the observer is not the only new speaker, then assume no one is an observer.
		// ALSO if this speaker is part of the previous speakerGroup, and is not an observer, and every member of the previous speaker group is in present
		// speakerGroup and the previous speakerGroup had conversational quotes, then this is not an observer
		bool conversationRestricted = (speakerGroups[sgi].speakers.size() == 2 && speakerGroups[sgi].conversationalQuotes), isAnyNonObserverSpeakerNew = false, isAnyObserverSpeakerNew = false;
		int nonObserver = -1;
		determineObserverStatus(sgi, nonObserver, conversationRestricted, isAnyNonObserverSpeakerNew, isAnyObserverSpeakerNew);
		// if all observers are old and all non observers are new and there is only one non-observer (!isAnyObserverSpeakerNew && isAnyNonObserverSpeakerNew && observerContinuing)
		// and the present observer was also the last observer AND in the last speakergroup which had the non-observer, that observer was also the present observer
		bool observerContinuing = determineObserverContinuing(sgi, nonObserver, conversationRestricted);
		// if only one quote is recorded, and the only definitive speaker is the observer, then conversationalQuotes should be 0 (no conversation took place?)
		if (speakerGroups[sgi].conversationalQuotes == 1 && speakerGroups[sgi].observers.size() == 1 && speakerGroups[sgi].dnSpeakers.size() == 1 &&
			speakerGroups[sgi].dnSpeakers == speakerGroups[sgi].observers)
		{
			speakerGroups[sgi].conversationalQuotes = 0;
			conversationRestricted = false;
		}
		// The German and Thomas - When the German is re-introduced, The German creates a new speakerGroup, which is not then merged with the next one either
		// OR
		// if any observers are new and all non observers are old (isAnyObserverSpeakerNew && !isAnyNonObserverSpeakerNew)
		// OR (deleted) or all observers were in the last speaker group as observers
		if (conversationRestricted && speakerGroups[sgi].observers.size() && sgi &&
			((isAnyObserverSpeakerNew && !isAnyNonObserverSpeakerNew) || (!isAnyObserverSpeakerNew && isAnyNonObserverSpeakerNew && observerContinuing)))
		{
			conversationRestricted = false;
			// add previous speakerGroup
			speakerGroups[sgi].speakers.insert(speakerGroups[sgi - 1].speakers.begin(), speakerGroups[sgi - 1].speakers.end());
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION | LOG_SG, u"speakers added to observer-created speakerGroup %s (conversation restriction isAnyObserverSpeakerNew=%s isAnyNonObserverSpeakerNew=%s observerContinuing=%s).",
					toText(speakerGroups[sgi], tmpstr2), (isAnyObserverSpeakerNew) ? u"true" : u"false", (isAnyNonObserverSpeakerNew) ? u"true" : u"false", (observerContinuing) ? u"true" : u"false");
		}
		for (section = 0; section + 1 < sections.size() && (signed)sections[section + 1].begin < speakerGroups[sgi].sgBegin; section++);
		if (conversationRestricted)
		{
			if (speakerGroups[sgi].observers.size())
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"all observers erased in speakerGroup %s (conversation restriction).", toText(speakerGroups[sgi], tmpstr2));
				speakerGroups[sgi].observers.clear();
			}
		}
		else
		{
			// both speakergroups must belong to the same section
			bool previousSpeakerGroupInSameSection = section < sections.size() && sgi>0 && ((signed)sections[section].begin < speakerGroups[sgi - 1].sgBegin);
			addPreviousObserver(sgi, previousSpeakerGroupInSameSection);
			addPreviousPOV(sgi, previousSpeakerGroupInSameSection);
			removeObserverAssociatedWithSpeakingGroup(sgi);
		}
		if (speakerGroups[sgi].povSpeakers.empty())
		{
			if (speakerGroups[sgi].speakers.size() == 1)
			{
				speakerGroups[sgi].povSpeakers = speakerGroups[sgi].speakers;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"POVI only lonely in speakerGroup %s.", toText(speakerGroups[sgi], tmpstr2));
			}
			else if (speakerGroups[sgi].speakers.find(0) != speakerGroups[sgi].speakers.end())
			{
				speakerGroups[sgi].povSpeakers.insert(0);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"POVI Narrator in speakerGroup %s.", toText(speakerGroups[sgi], tmpstr2));
			}
		}
		sgi++;
	}
}

// True if this object class cannot belong to a speaker subgroup (places,
// pronouns, verbs, non-gendered general, ...).  The complement is the
// gendered / name / body / meta-group set used by accumulateGroups.
bool cSource::invalidGroupObjectClass(int oc)
{
	LFS
		return oc != NAME_OBJECT_CLASS && oc != GENDERED_GENERAL_OBJECT_CLASS &&
		oc != BODY_OBJECT_CLASS && oc != GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
		oc != GENDERED_DEMONYM_OBJECT_CLASS && oc != GENDERED_RELATIVE_OBJECT_CLASS &&
		oc != META_GROUP_OBJECT_CLASS;
}

// Collect syntactic speaker subgroups into tempSpeakerGroup.groups:
// MPLURAL_ROLE chains, "X accompanied by Y" / "with him was Y", plural
// objectMatches that are all physically-present narration subjects or
// already speakers, and previously grouped speakers.  Places, pronouns,
// and neuter-only objects are rejected.
void cSource::accumulateGroups(int where, vector <int>& groupedObjects, int& lastWhereMPluralGroupedObject)
{
	LFS
		vector <cWordMatch>::iterator im = m.begin() + where;
	lpwstring tmpstr, tmpstr2;
	// accumulate groups of objects for speakerGroup subgroups
	if (groupedObjects.size() && !(im->objectRole & MPLURAL_ROLE))
	{
		// only allow if each object is new to this group
		bool invalidObject = false;
		for (unsigned int om = 0; om < groupedObjects.size() && !invalidObject; om++)
			invalidObject = groupedObjects[om] < 0 || invalidGroupObjectClass(objects[groupedObjects[om]].objectClass) || objects[groupedObjects[om]].getSubType() >= 0; // places should not be grouped, because these groups are used to group speakers
		set <int> repeatedObjects;
		// check for repeats
		for (int I = 0; I < (signed)groupedObjects.size(); I++) repeatedObjects.insert(groupedObjects[I]);
		if (!invalidObject && groupedObjects.size() == repeatedObjects.size() && groupedObjects.size() > 1)
		{
			tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(lastWhereMPluralGroupedObject, groupedObjects));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION | LOG_SG, u"%d:Grouped gendered objects %s in tempSpeakerGroup %s (1).", lastWhereMPluralGroupedObject, objectString(groupedObjects, tmpstr).c_str(), toText(tempSpeakerGroup, tmpstr2));
		}
		groupedObjects.clear();
	}
	// Tommy, accompanied by Albert, explored the grounds.
	int element = -1;
	int whereWithObject = -1;
	if ((m[where].objectRole & SUBJECT_ROLE) && where + 3 < (signed)m.size() && (element = m[where + 1].pma.queryPattern(u"__C1_IP")) != -1 && m[where + 2].getMainEntry()->first == u"accompany" &&
		m[where + 3].word->first == u"by")
	{
		whereWithObject = m[where + 3].getRelObject();
		if (m[where].getRelVerb() >= 0 && whereWithObject >= 0 && m[whereWithObject].getRelVerb() < 0)
			m[whereWithObject].setRelVerb(m[where].getRelVerb());
	}
	// With him[conrad] was the evil - looking Number 14 .
	if ((m[where].objectRole & SUBJECT_ROLE) && m[where].getRelObject() < 0 && m[where].getRelVerb() >= 0 && m[m[where].getRelVerb()].queryWinnerForm(isForm) >= 0 &&
		m[m[where].getRelVerb()].relPrep >= 0 && m[m[m[where].getRelVerb()].relPrep].word->first == u"with" && m[m[m[where].getRelVerb()].relPrep].getRelObject() >= 0)
		whereWithObject = m[m[m[where].getRelVerb()].relPrep].getRelObject();
	if (whereWithObject >= 0 && m[whereWithObject].getObject() >= 0 && im->getObject() >= 0 &&
		objects[m[whereWithObject].getObject()].objectClass != BODY_OBJECT_CLASS && m[whereWithObject].objectMatches.size() <= 1 &&
		objects[im->getObject()].objectClass != BODY_OBJECT_CLASS && im->objectMatches.size() <= 1)
	{
		int withObject = (m[whereWithObject].objectMatches.size() == 1) ? m[whereWithObject].objectMatches[0].object : m[whereWithObject].getObject();
		int o = (im->objectMatches.size() == 1) ? im->objectMatches[0].object : im->getObject();
		if (!invalidGroupObjectClass(objects[withObject].objectClass) && objects[withObject].getSubType() < 0 &&
			!invalidGroupObjectClass(objects[o].objectClass) && objects[o].getSubType() < 0 &&
			o != withObject)
		{
			groupedObjects.push_back(withObject);
			groupedObjects.push_back(o);
			tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where, groupedObjects));
			im->objectRole |= MPLURAL_ROLE; // so that this group is assigned the highest preference grouping
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION | LOG_SG, u"%d:Grouped gendered objects %s in tempSpeakerGroup %s (5).", where, objectString(groupedObjects, tmpstr).c_str(), toText(tempSpeakerGroup, tmpstr2));
			groupedObjects.clear();
		}
	}
	if (im->getObject() != -1 && !(im->flags & cWordMatch::flagAdjectivalObject))
	{
		if ((im->objectRole & MPLURAL_ROLE))
		{
			lastWhereMPluralGroupedObject = where;
			if (im->objectMatches.empty() && !invalidGroupObjectClass(objects[im->getObject()].objectClass) && objects[im->getObject()].getSubType() < 0)
				groupedObjects.push_back(im->getObject());
			else if ((im->word->second.inflectionFlags & PLURAL) || (im->objectMatches.size() == 1))
			{
				for (unsigned int om = 0; om < im->objectMatches.size(); om++)
					if (!invalidGroupObjectClass(objects[im->objectMatches[om].object].objectClass) && objects[im->objectMatches[om].object].getSubType() < 0)
						groupedObjects.push_back(im->objectMatches[om].object);
			}
		}
		else if ((im->word->second.inflectionFlags & PLURAL) && (im->objectMatches.size() > 1) &&
			(im->getObject() < 0 || objects[im->getObject()].objectClass != BODY_OBJECT_CLASS))
		{
			vector <int> groupedPluralMatchedObjects;
			bool invalidObject = false;
			for (unsigned int om = 0; om < im->objectMatches.size() && !invalidObject; om++)
				// this disallows groups that are more obviously incorrect.  It is a little more conservative algorithm.
				invalidObject = im->objectMatches[om].object < 0 || objects[im->objectMatches[om].object].firstPhysicalManifestation < tempSpeakerGroup.sgBegin ||
				invalidGroupObjectClass(objects[im->objectMatches[om].object].objectClass) ||
				objects[im->objectMatches[om].object].getSubType() >= 0;
			// or if there are only two objects, and both are physically present narrative subjects
			if (invalidObject)
			{
				for (vector <cOM>::iterator omi = im->objectMatches.begin(), omiEnd = im->objectMatches.end(); omi != omiEnd; omi++)
					if (find(nextNarrationSubjects.begin(), nextNarrationSubjects.end(), omi->object) != nextNarrationSubjects.end())
						groupedPluralMatchedObjects.push_back(omi->object);
				if (groupedPluralMatchedObjects.size() >= 2)
					invalidObject = false;
				else
					groupedPluralMatchedObjects.clear();
			}
			if (invalidObject)
			{
				// or if there are only two objects, and both are speakers
				for (vector <cOM>::iterator omi = im->objectMatches.begin(), omiEnd = im->objectMatches.end(); omi != omiEnd; omi++)
					if (tempSpeakerGroup.speakers.find(omi->object) != tempSpeakerGroup.speakers.end())
						groupedPluralMatchedObjects.push_back(omi->object);
				if (groupedPluralMatchedObjects.size() >= 2)
					invalidObject = false;
				else
					groupedPluralMatchedObjects.clear();
			}
			if (invalidObject)
			{
				// have these objects ever been grouped in the past?
				bool allIn, oneIn;
				for (int sg = speakerGroups.size() - 1; sg >= 0 && invalidObject; sg--)
					if (intersect(where, speakerGroups[sg].groupedSpeakers, allIn, oneIn) && allIn)
						invalidObject = false;
			}
			// all, any, etc. are too vague or don't imply a group
			// also 'people', 'everybody' etc also doesn't imply a group [of associates or friends]
			// don't accumulate groups of neuter objects (that might be picked up as groups of gendered objects later)
			invalidObject |= (im->queryForm(pronounForm) >= 0 || im->queryForm(indefinitePronounForm) >= 0 || (objects[im->getObject()].neuter && !(objects[im->getObject()].male || objects[im->getObject()].female)));
			// all the others
			if (im->beginObjectPosition != where)
				invalidObject |= (m[im->beginObjectPosition].queryForm(pronounForm) >= 0 || m[im->beginObjectPosition].queryForm(indefinitePronounForm) >= 0); // all, any, etc. are too vague or don't imply a group
			if (!invalidObject)
			{
				if (groupedPluralMatchedObjects.size())
				{
					tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where, groupedPluralMatchedObjects));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION | LOG_SG, u"%d:Grouped gendered objects %s in tempSpeakerGroup %s (2).", where, objectString(groupedPluralMatchedObjects, tmpstr).c_str(), toText(tempSpeakerGroup, tmpstr2));
				}
				else
				{
					groupedObjects.clear();
					for (unsigned int om = 0; om < im->objectMatches.size(); om++)
						groupedObjects.push_back(im->objectMatches[om].object);
					tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where, groupedObjects));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION | LOG_SG, u"%d:Grouped gendered objects %s in tempSpeakerGroup %s (3).", where, objectString(groupedObjects, tmpstr).c_str(), toText(tempSpeakerGroup, tmpstr2));
				}
			}
			groupedObjects.clear();
		}
		else if ((im->word->second.inflectionFlags & PLURAL) != 0 && im->objectMatches.size() <= 1 &&
			im->getObject() >= 0 && objects[im->getObject()].objectClass != BODY_OBJECT_CLASS)
		{
			int o = im->getObject(), oc = objects[o].objectClass;
			// this disallows groups that are more obviously incorrect.  It is a little more conservative algorithm.
			if (o >= 0 && objects[o].firstPhysicalManifestation >= tempSpeakerGroup.sgBegin && !invalidGroupObjectClass(oc) && objects[o].getSubType() < 0)
			{
				groupedObjects.clear();
				groupedObjects.push_back(o);
				tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where, groupedObjects));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION | LOG_SG, u"%d:Grouped gendered objects %s in tempSpeakerGroup %s (4).", where, objectString(groupedObjects, tmpstr).c_str(), toText(tempSpeakerGroup, tmpstr2));
			}
			groupedObjects.clear();
		}
	}
}

// True if the two source positions resolve to overlapping speakers.
// -1 on either side is treated as "same" (imposed / unknown speaker).
bool cSource::sameSpeaker(int sWhere1, int sWhere2)
{
	LFS
		if (sWhere1 == -1 || sWhere2 == -1)
			return true;
	if (m[sWhere1].objectMatches.empty() && m[sWhere2].objectMatches.empty())
		return m[sWhere1].getObject() == m[sWhere2].getObject();
	if (m[sWhere1].objectMatches.empty())
		return in(m[sWhere1].getObject(), m[sWhere2].objectMatches) != m[sWhere2].objectMatches.end();
	if (m[sWhere2].objectMatches.empty())
		return in(m[sWhere2].getObject(), m[sWhere1].objectMatches) != m[sWhere1].objectMatches.end();
	bool allIn, oneIn;
	intersect(m[sWhere1].objectMatches, m[sWhere2].objectMatches, allIn, oneIn);
	return oneIn;
}

// CMREADME019
// Detect a character-told story (past-tense-heavy quote, or an inserted
// continuation quote with no imposed speaker).  Marks the previous quote
// flagEmbeddedStoryBeginResolveSpeakers / flagEmbeddedStoryResolveSpeakers,
// records it in subNarratives, stamps IN_EMBEDDED_STORY_OBJECT_ROLE on the
// span, and distinguishes 1st- vs 2nd-person stories.  A gap of more than
// one intervening non-story quote, or a 1st/2nd-person flip, starts a new
// story rather than extending.  Resets the four counters before return.
void cSource::embeddedStory(int where, int& numPastSinceLastQuote, int& numNonPastSinceLastQuote, int& numSecondInQuote, int& numFirstInQuote,
	int& lastEmbeddedStory, int& lastEmbeddedImposedSpeakerPosition, int lastSpeakerPosition)
{
	LFS
		bool mustBeExtension = false;
	// if speaker is not imposed, and the quote is inserted at the end (thus implying continuation)
	if ((numPastSinceLastQuote > 1 && numPastSinceLastQuote >= numNonPastSinceLastQuote * 2) ||
		(mustBeExtension = (lastSpeakerPosition < 0 && lastEmbeddedStory >= 0 && m[lastOpeningPrimaryQuote].endQuote >= 0 &&
			(m[m[lastOpeningPrimaryQuote].endQuote].flags & cWordMatch::flagInsertedQuote) != 0)))
	{
		lpwstring tmpstr;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d-%06d:LQ past=%03d	nonPast=%03d first=%03d second=%03d lastEmbeddedStory=%d %d:%d %s",
				lastQuote, m[lastOpeningPrimaryQuote].endQuote, numPastSinceLastQuote, numNonPastSinceLastQuote,
				numFirstInQuote, numSecondInQuote, lastEmbeddedStory, lastEmbeddedImposedSpeakerPosition, lastSpeakerPosition,
				objectString(m[lastQuote].objectMatches, tmpstr, true).c_str());
		if (lastEmbeddedStory >= 0 && sameSpeaker(lastEmbeddedImposedSpeakerPosition, lastSpeakerPosition))
		{
			// gap - allow a gap of 1 (must be non story, which is to say, in the present tense)
			int gap = 0, lastGap = -1;
			for (int es = lastEmbeddedStory; es != lastQuote && gap <= 1; es = m[es].nextQuote)
			{
				if (m[es].flags & cWordMatch::flagEmbeddedStoryResolveSpeakers)
					gap = 0;
				else
				{
					gap++;
					lastGap = es;
				}
			}
			// previous = 63124 - [tommy:julius] By Jove , that explains why they[james,tuppence,interest] looked at me[tommy]
			// current = 63184 -  [julius:tommy] They[fellow,tuppence,james,interest...] didn't give you[tommy] any sort
			bool switchPersonGap;
			if (switchPersonGap = (((m[lastEmbeddedStory].flags & (cWordMatch::flagSecondEmbeddedStory | cWordMatch::flagFirstEmbeddedStory)) == cWordMatch::flagFirstEmbeddedStory && numSecondInQuote && !numFirstInQuote) ||
				((m[lastEmbeddedStory].flags & (cWordMatch::flagSecondEmbeddedStory | cWordMatch::flagFirstEmbeddedStory)) == cWordMatch::flagSecondEmbeddedStory && numFirstInQuote && !numSecondInQuote)) &&
				gap <= 1)
				lplog(LOG_RESOLUTION, u"%06d,%06d:LQ person violation", lastEmbeddedStory, lastQuote);
			if (gap > 1 || switchPersonGap)
			{
				if (mustBeExtension) return;
				m[lastQuote].flags |= cWordMatch::flagEmbeddedStoryBeginResolveSpeakers;
				subNarratives.push_back(lastQuote);
				lastEmbeddedStory = lastQuote;
				m[where].embeddedStorySpeakerPosition = -1;
				lastEmbeddedImposedSpeakerPosition = lastSpeakerPosition;
			}
			else
			{
				if (debugTrace.traceSpeakerResolution)
				{
					if (gap)
						lplog(LOG_RESOLUTION, u"%06d,%06d:LQ EXTENDED (GAP@%d)", lastEmbeddedStory, m[lastOpeningPrimaryQuote].endQuote, lastGap);
					else
						lplog(LOG_RESOLUTION, u"%06d,%06d:LQ EXTENDED", lastEmbeddedStory, m[lastOpeningPrimaryQuote].endQuote);
				}
				if (lastEmbeddedImposedSpeakerPosition < 0)
					lastEmbeddedImposedSpeakerPosition = lastSpeakerPosition;
				m[where].embeddedStorySpeakerPosition = lastEmbeddedImposedSpeakerPosition;
				if (gap && lastGap >= 0)
					m[lastGap].flags |= cWordMatch::flagEmbeddedStoryResolveSpeakersGap;
				// if a story starts, and the very next quote has a definite attribution, then the previous quote is not speaker contiguous
				if (gap == 0 && m[lastEmbeddedStory].nextQuote == lastQuote && lastSpeakerPosition >= 0 && m[lastQuote].speakerPosition == lastSpeakerPosition)
				{
					numPastSinceLastQuote = numNonPastSinceLastQuote = numSecondInQuote = numFirstInQuote = 0;
					return;
				}
			}
		}
		else
		{
			m[lastQuote].flags |= cWordMatch::flagEmbeddedStoryBeginResolveSpeakers;
			subNarratives.push_back(lastQuote);
			lastEmbeddedStory = lastQuote;
			lastEmbeddedImposedSpeakerPosition = lastSpeakerPosition;
		}
		// an inserted quote is used in the speaker resolution to assert that this quote is the same as last quote.
		// however when a story is being related this is not necessarily true as the other speaker can occasionally interject (gap==1).
		m[m[lastQuote].endQuote].flags &= ~cWordMatch::flagInsertedQuote; // overrides inserted quote - more authoritative
		m[lastQuote].flags |= cWordMatch::flagEmbeddedStoryResolveSpeakers;
		for (int I = lastQuote + 1; I < m[lastOpeningPrimaryQuote].endQuote; I++)
			m[I].objectRole |= IN_EMBEDDED_STORY_OBJECT_ROLE;
		if (numSecondInQuote)
			m[lastQuote].flags |= cWordMatch::flagSecondEmbeddedStory;
		if (numFirstInQuote)
			m[lastQuote].flags |= cWordMatch::flagFirstEmbeddedStory;
	}
	else if (debugTrace.traceSpeakerResolution && lastOpeningPrimaryQuote >= 0)
		lplog(LOG_RESOLUTION, u"%06d-%06d:L past=%03d	nonPast=%03d first=%03d second=%03d", lastQuote, m[lastOpeningPrimaryQuote].endQuote, numPastSinceLastQuote, numNonPastSinceLastQuote, numFirstInQuote, numSecondInQuote);
	numPastSinceLastQuote = numNonPastSinceLastQuote = numSecondInQuote = numFirstInQuote = 0;
}

// Correct HAIL_ROLE on the fly: strip it from place-name lists ("Ebury,
// Yorks") and from self-addressed quotes (audience is reflexive); add it
// for a definite/physically-present name that is the entire quoted
// utterance ("Miss Finn,").
void cSource::adjustHailRoleDuringScan(int where)
{
	LFS
		vector <cWordMatch>::iterator im = m.begin() + where;
	uint64_t objectRole = im->objectRole & (HAIL_ROLE | MPLURAL_ROLE | RE_OBJECT_ROLE);
	int oc = (im->getObject() >= 0) ? objects[im->getObject()].objectClass : -1;
	// Here[here] we[tommy,julius] are . Ebury , Yorks .
	// Come at once , Moat House , Ebury , Yorkshire , great developments -- Tommy
	// where==Ebury
	int so;
	if ((oc == NAME_OBJECT_CLASS || oc == NON_GENDERED_NAME_OBJECT_CLASS) && !objects[im->getObject()].PISDefinite &&
		im->beginObjectPosition > 0 &&
		(m[im->beginObjectPosition - 1].word->second.isSeparator() || (m[where].objectRole & MOVEMENT_PREP_OBJECT_ROLE)) &&
		im->endObjectPosition + 2 < (signed)m.size() && m[im->endObjectPosition + 1].endObjectPosition < (signed)m.size() &&
		m[im->endObjectPosition].word->first == u"," &&
		(so = m[im->endObjectPosition + 1].getObject()) >= 0 && !objects[so].PISDefinite &&
		(objects[so].getSubType() >= 0 || (objects[so].name.hon == wNULL && !objects[so].isNotAPlace && objects[im->getObject()].getSubType() >= 0)) &&
		objects[so].objectClass == NAME_OBJECT_CLASS &&
		m[m[im->endObjectPosition + 1].endObjectPosition].word->second.isSeparator())
	{
		if (objectRole & HAIL_ROLE)
		{
			objectRole &= ~HAIL_ROLE;
			im->objectRole &= ~HAIL_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE, u"%06d:Removed HAIL role (PLACE).", where);
		}
		objects[im->getObject()].setSubType(WORLD_CITY_TOWN_VILLAGE);
		if (m[im->endObjectPosition + 1].objectRole & HAIL_ROLE)
		{
			m[im->endObjectPosition + 1].objectRole &= ~HAIL_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE, u"%06d:Removed HAIL role (PLACE).", im->endObjectPosition + 1);
		}
	}
	// guarded like 'oc' above: getObject()==-1 must not form an out-of-range iterator
	// (UB, and an assert under MSVC's debug-mode checked iterators) - o is only
	// dereferenced below once im->getObject()>=0 is confirmed.
	vector <cObject>::iterator o = objects.begin() + ((im->getObject() >= 0) ? im->getObject() : 0);
	if (im->getObject() >= 0 && !(im->objectRole & HAIL_ROLE) && o->objectClass == NAME_OBJECT_CLASS && (im->objectRole & IN_PRIMARY_QUOTE_ROLE) &&
		im->beginObjectPosition && m[im->beginObjectPosition - 1].word->first == u"�" && m[im->endObjectPosition].word->first == u"," && m[im->endObjectPosition + 1].word->first == u"�" &&
		(o->PISDefinite || o->PISHail > 1 || (o->name.hon != wNULL && !o->name.justHonorific() && o->numEncountersInSection > 1))) // encounters already at least one because of resolveObject
	{
		vector <cLocalFocus>::iterator lsi = in(im->getObject());
		if ((lsi != localObjects.end() && lsi->physicallyPresent) ||
			// if this is the first time the speaker is mentioned in the speaker group.
			(speakerGroupsEstablished && currentSpeakerGroup < speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.find(im->getObject()) != speakerGroups[currentSpeakerGroup].speakers.end()))
		{
			im->objectRole |= HAIL_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE, u"%06d:Acquired HAIL role (4) definite=%d subject=%d encountered=%d.", where, o->PISDefinite, o->PISSubject, o->numEncountersInSection);
		}
	}
	// if the last quote has the speaker talking to himself/herself, remove hail
	if ((objectRole & HAIL_ROLE) && (im->objectRole & IN_PRIMARY_QUOTE_ROLE) && lastOpeningPrimaryQuote >= 0 &&
		m[lastOpeningPrimaryQuote].audiencePosition >= 0 && m[m[lastOpeningPrimaryQuote].audiencePosition].queryWinnerForm(reflexivePronounForm) >= 0)
	{
		objectRole &= ~HAIL_ROLE;
		im->objectRole &= ~HAIL_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE, u"%06d:Removed HAIL role (REFLEXIVE).", where);
	}
}

// Collapse alias/replaced speakers inside each group.  If that leaves a
// singleton group that has conversationalQuotes > 1 and whose only speaker
// is already in the next (larger) group, merge the two spans and rewind
// previousSubsetSpeakerGroup / firstSpeakerGroup / lastSpeakerGroup indexes.
void cSource::eraseAliasesAndReplacementsInSpeakerGroups(void)
{
	LFS
		for (vector <cSpeakerGroup>::iterator sg = speakerGroups.begin(); sg != speakerGroups.end() && (sg + 1) != speakerGroups.end(); )
			if (eraseAliasesAndReplacementsInSpeakerGroup(sg, false) && sg->speakers.size() == 1 &&
				// if a replacement occurs, reducing it from 2 to 1 (and so a non-self conversation is not possible)
				(sg + 1)->speakers.size() > 1 && (sg + 1)->speakers.find(*sg->speakers.begin()) != (sg + 1)->speakers.end() && sg->conversationalQuotes > 1)
			{
				(sg + 1)->sgBegin = sg->sgBegin;
				(sg + 1)->conversationalQuotes += sg->conversationalQuotes;
				(sg + 1)->dnSpeakers.insert(sg->dnSpeakers.begin(), sg->dnSpeakers.end());
				int numSG = (int)(sg - speakerGroups.begin());
				for (auto& tsg : speakerGroups)
					if (tsg.previousSubsetSpeakerGroup >= numSG)
						tsg.previousSubsetSpeakerGroup--;
				for (cObject& o : objects)
				{
					if (o.getFirstSpeakerGroup() >= numSG)
						o.setFirstSpeakerGroup(o.getFirstSpeakerGroup() - 1);
					if (o.lastSpeakerGroup >= numSG)
						o.lastSpeakerGroup--;
				}
				lplog(LOG_RESOLUTION, u"speakerGroup erased: %d", sg - speakerGroups.begin());
				sg = speakerGroups.erase(sg);
			}
			else
				sg++;
}

// True when a new speaker group should NOT be closed at this paragraph
// boundary: we are mid-conversation (next paragraph opens with a quote),
// the next paragraph is only a speaker attribution, the previous group
// would become a quote-only span, or this paragraph ends with a colon.
bool cSource::blockSpeakerGroupCreation(int endSection, bool quotesSeenSinceLastSentence, int nsAfter)
{
	LFS
		bool block = false;
	if (quotesSeenSinceLastSentence) // if the previous paragraph was a quote
	{
		block = (nsAfter < (int)m.size() && m[nsAfter].word->first == u"�"); // in the middle of a conversation
		// also block if the present paragraph's only sentence is a speaker attribution
		if (!block)
		{
			// (search for next quote)
			int beginQuote = nsAfter;
			for (; beginQuote < (int)m.size() && m[beginQuote].word->first != u"�" &&
				m[beginQuote].word->first != u"?" && m[beginQuote].word->first != u"!" && (m[beginQuote].word->first != u"." || m[beginQuote].PEMACount); beginQuote++);
			if (beginQuote < (int)m.size() && m[beginQuote].word->first == u"�")
			{
				// (search for the speaker position)
				bool definitelySpeaker = true, previousParagraph = false, crossedSectionBoundary = false; // checkCataSpeaker=false,
				int speakerPosition = -1, beforeLocation = speakerBefore(beginQuote, previousParagraph), audienceObjectPosition = -1; // unknown
				if (beforeLocation >= 0)
					speakerPosition = scanForSpeaker(beforeLocation, definitelySpeaker, crossedSectionBoundary, audienceObjectPosition);
				block = (speakerPosition >= 0 && speakerPosition == nsAfter);
			}
			// do not allow a speaker group to be a quote without including either the previous paragraph or the next paragraph
			// also block if there is a quote in the paragraph immediately after lastSpeakerGroupPositionConsidered, and 
			// the quote ends (or through forwardLinks) immediately before I.
			if (!block && speakerGroups.size() && lastOpeningPrimaryQuote >= 0 && m[lastOpeningPrimaryQuote].endQuote + 1 == endSection)
			{
				int firstQuoteAfter = speakerGroups[speakerGroups.size() - 1].sgBegin + 1;
				for (; firstQuoteAfter < lastOpeningPrimaryQuote && m[firstQuoteAfter].word->first != u"�"; firstQuoteAfter++);
				while (m[firstQuoteAfter].getQuoteForwardLink() >= 0) firstQuoteAfter = m[firstQuoteAfter].getQuoteForwardLink();
				block |= firstQuoteAfter == lastOpeningPrimaryQuote;
			}
		}
	}
	// also block if the last paragraph ends with a colon (so the subject is saying something)
	return block | (endSection && m[endSection - 1].word->first == u":");
}

// Pick a physically-present speaker position to retain across a time /
// location transition: prefer sr->whereSubject if it is an agent, else the
// sole PP speaker in localObjects, else sr->whereObject if that is an agent.
// Returns a source position, or -1 if the subject is unusable and nothing
// else qualifies.
int cSource::getSpeakersToKeep(vector<cSyntacticRelationGroup>::iterator sr)
{
	LFS
		vector <cLocalFocus>::iterator llsi;
	int numPPSpeakers = 0, keepPPSpeakerWhere = sr->whereSubject; // lastPPSpeaker=-1,
	// subject cannot be used
	if (sr->whereSubject >= 0 && m[sr->whereSubject].getObject() >= 0 && !objects[m[sr->whereSubject].getObject()].isAgent(true))
		keepPPSpeakerWhere = -1;
	if (keepPPSpeakerWhere < 0)
	{
		// if there is only one PP speaker, and whereSubject is not a speaker, and hasSyntacticRelationGroup is not a TIME type, then set where to the PP speaker.
		for (vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsiEnd = localObjects.end(); lsi != lsiEnd; lsi++)
			if (lsi->physicallyPresent && lsi->numIdentifiedAsSpeaker > 0)
			{
				numPPSpeakers++;
				llsi = lsi;
			}
		if (numPPSpeakers == 1)
			keepPPSpeakerWhere = llsi->whereBecamePhysicallyPresent;
		else if (sr->whereObject >= 0 && m[sr->whereObject].getObject() >= 0 && objects[m[sr->whereObject].getObject()].isAgent(true))
			keepPPSpeakerWhere = sr->whereObject;
	}
	return keepPPSpeakerWhere;
}

// Chapter / section boundary: age every local-focus entry by 5, drop hail-
// only preIdentifiedSpeakerObjects, close the current speaker group with
// endOfSection=true, and reset narration-subject / lastSubjects state for
// the new section.
void cSource::beginSection(int& lastSpeakerGroupPositionConsidered, int& lastSpeakerGroupOfPreviousSection, int I, vector <int>& previousLastSubjects, vector <int>& lastSubjects)
{
	lpwstring tmpstr, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION, u"%06d:%02d     aging speakers %s End Of Section", I, section, (currentSpeakerGroup == 0) ? u"" : objectString(speakerGroups[currentSpeakerGroup - 1].speakers, tmpstr).c_str());
	for (vector <cLocalFocus>::iterator lfi = localObjects.begin(); lfi != localObjects.end(); lfi++)
	{
		m[I].flags |= cWordMatch::flagAge;
		lfi->numEncounters = lfi->numDefinitelyIdentifiedAsSpeaker = 0;
		lfi->increaseAge(5);
		if (lfi->numIdentifiedAsSpeaker)
			lfi->numIdentifiedAsSpeaker = 1;
	}
	for (set <int>::iterator s = sections[section].preIdentifiedSpeakerObjects.begin(); s != sections[section].preIdentifiedSpeakerObjects.end(); )
	{
		if (objects[*s].PISHail && !objects[*s].PISDefinite && !objects[*s].PISSubject)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, u"%06d:%02d     hail %s deleted from preidentified", I, section, objectString(*s, tmpstr, true).c_str());
			sections[section].preIdentifiedSpeakerObjects.erase(s++);
		}
		else
			s++;
	}
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"%d-%d:---------------%s---------------\n nextNarrationSubjects=[%s]\n nextISNarrationSubjects=[%s]\n endOfSection=[true]\n lastSpeakerGroupOfPreviousSection=#%03d of %03d\n lastSpeakerGroup=[%s]\n currentSpeakerGroup=[%s]\n subjectsInPreviousUnquotedSection=[%s]",
			lastSpeakerGroupPositionConsidered, I, (I == m.size() - 1) ? u"EOF" : u"EOS", objectString(nextNarrationSubjects, tmpstr2).c_str(), objectString(nextISNarrationSubjects, tmpstr6).c_str(),
			lastSpeakerGroupOfPreviousSection, speakerGroups.size(), (speakerGroups.size()) ? toText(speakerGroups[speakerGroups.size() - 1], tmpstr3) : u"", toText(tempSpeakerGroup, tmpstr4), objectString(subjectsInPreviousUnquotedSection, tmpstr5).c_str());
	speakerSections.push_back(lastSpeakerGroupPositionConsidered);
	if (speakerGroups.size())
		for (set <int>::iterator s = speakerGroups[speakerGroups.size() - 1].speakers.begin(); s != speakerGroups[speakerGroups.size() - 1].speakers.end(); s++)
			speakerAges[*s]++;
	createSpeakerGroup(lastSpeakerGroupPositionConsidered, I, true, lastSpeakerGroupOfPreviousSection);
	nextNarrationSubjects.clear();
	nextISNarrationSubjects.clear();
	whereNextISNarrationSubjects.clear();
	lastSpeakerGroupPositionConsidered = I;
	subjectsInPreviousUnquotedSection.clear();
	previousLastSubjects.clear();
	lastSubjects.clear();
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"%d:ZXZ reset subjectsInPreviousUnquotedSection, previousLastSubjects, lastSubjects etc", I);
	section++;
	clearNextSection(I, section - 1);
}

// Paragraph-end (sectionWord) handler: skip a following meta-response,
// optionally block group creation, age last-group speakers (and mark them
// not-PP if speakerAge > 10), then createSpeakerGroup unless blocked.
// If the closing paragraph was unquoted, remember previousLastSubjects as
// subjectsInPreviousUnquotedSection for the next group.
void cSource::endSection(int& questionSpeakerLastParagraph, int& questionSpeakerLastSentence, int& whereFirstSubjectInParagraph, int& lastSpeakerGroupPositionConsidered, int& lastSpeakerGroupOfPreviousSection, int I,
	bool& endOfSentence, bool& immediatelyAfterEndOfParagraph, bool& quotesSeenSinceLastSentence, bool inSecondaryQuote, bool inPrimaryQuote, bool& quotesSeen, bool& firstQuotedSentenceOfSpeakerGroupNotSeen,
	vector <int> previousLastSubjects)
{
	lpwstring tmpstr, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6;
	int element;
	// question tracking
	if (questionSpeakerLastParagraph >= 0 && whereFirstSubjectInParagraph >= 0)
		questionSubjectAgreementMap[whereFirstSubjectInParagraph] = questionSpeakerLastParagraph;
	endOfSentence = false;
	immediatelyAfterEndOfParagraph = true;
	subjectsInPreviousUnquotedSectionUsableForImmediateResolution = false;
	// does the proposed end (I) of the current speaker group end between characters dialog?  If so, reject
	int nsAfter, nsSkipAfter = -1;
	for (nsAfter = I + 1; nsAfter < (int)m.size() && m[nsAfter].word == Words.sectionWord; nsAfter++);
	// skip / another voice[boris] which Tommy rather thought was that of Boris replied :
	bool metaResponseDetected = false;
	if (nsAfter != m.size() && (element = m[nsAfter].pma.queryPattern(u"__S1")) != -1)
		metaResponseDetected = (nsSkipAfter = detectMetaResponse(nsAfter, element & ~cMatchElement::patternFlag)) >= 0;
	if (metaResponseDetected) nsAfter = nsSkipAfter;
	// detect if we should not create a speaker group
	bool block = blockSpeakerGroupCreation(I, quotesSeenSinceLastSentence, nsAfter);
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG, u"%d-%d:--------%s-------------------\n nextNarrationSubjects=[%s]\n nextISNarrationSubjects=[%s]\n endOfSection=[false]\n lastSpeakerGroupOfPreviousSection=#%03d of %03d\n lastSpeakerGroup=[%s]\n currentSpeakerGroup=[%s]\n subjectsInPreviousUnquotedSection=[%s]",
			lastSpeakerGroupPositionConsidered, I, (block) ? u"BLOCKED" : u"-------", objectString(nextNarrationSubjects, tmpstr2).c_str(), objectString(nextISNarrationSubjects, tmpstr6).c_str(),
			lastSpeakerGroupOfPreviousSection, speakerGroups.size(), (speakerGroups.size()) ? toText(speakerGroups[speakerGroups.size() - 1], tmpstr3) : u"", toText(tempSpeakerGroup, tmpstr4), objectString(subjectsInPreviousUnquotedSection, tmpstr5).c_str());
	speakerSections.push_back(lastSpeakerGroupPositionConsidered);
	// age speakers
	cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || inPrimaryQuote, true, objectToBeMatchedInQuote, quoteIndependentAge);
	if (speakerGroups.size())
		for (set <int>::iterator s = speakerGroups[speakerGroups.size() - 1].speakers.begin(); s != speakerGroups[speakerGroups.size() - 1].speakers.end(); s++)
		{
			speakerAges[*s]++;
			vector <cLocalFocus>::iterator lsi;
			if (speakerAges[*s] > 10 && (lsi = in(*s)) != localObjects.end() && lsi->physicallyPresent && lsi->getAge(false, objectToBeMatchedInQuote, quoteIndependentAge) > 10)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION | LOG_SG, u"%06d:Setting primary speaker %s to not physically present (age %d>10, getAge=%d).", I, objectString(*s, tmpstr, true).c_str(), speakerAges[*s], lsi->getAge(false, objectToBeMatchedInQuote, quoteIndependentAge));
				lsi->physicallyPresent = false; // this prevents speakers from being mentioned later in the dialogue (but not actually appearing) and then not removed from the speakerGroup
			}
		}
	if (block && (m[I].flags & 1))
		block = false;
	if (!block && createSpeakerGroup(lastSpeakerGroupPositionConsidered, I, false, lastSpeakerGroupOfPreviousSection))
	{
		nextNarrationSubjects.clear();
		nextISNarrationSubjects.clear();
		whereNextISNarrationSubjects.clear();
	}
	// age speakers that have accumulated since the current speaker group has started
	for (set <int>::iterator s = tempSpeakerGroup.speakers.begin(), sEnd = tempSpeakerGroup.speakers.end(); s != sEnd; s++)
	{
		objects[*s].ageSinceLastSpeakerGroup++;
		if (block)
			speakerAges[*s] = 0;
	}
	if (!quotesSeenSinceLastSentence)
	{
		subjectsInPreviousUnquotedSection = previousLastSubjects;
		subjectsInPreviousUnquotedSectionUsableForImmediateResolution = true;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%d:ZXZ set subjectsInPreviousUnquotedSection=previousLastSubjects=%s", I, objectString(subjectsInPreviousUnquotedSection, tmpstr).c_str());
	}
	firstQuotedSentenceOfSpeakerGroupNotSeen = true;
	lastSpeakerGroupPositionConsidered = I;
	if (!inPrimaryQuote)
	{
		quotesSeenSinceLastSentence = false;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, u"%d:(4) quotesSeenSinceLastSentence set to false", I);
		quotesSeen = false;
	}
	questionSpeakerLastParagraph = questionSpeakerLastSentence;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION, u"%06d:QXQ whereFirstSubjectInParagraph,questionSpeakerLastSentence reset from %d,%d.", I, whereFirstSubjectInParagraph, questionSpeakerLastSentence);
	questionSpeakerLastSentence = -1;
	whereFirstSubjectInParagraph = -1;
	if (m[I].flags & 1)
	{
		localObjects.clear();
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:cleared local objects", I);
	}
}

// On each __S1 (main-clause) start that is not inside a meta-response,
// age every local-focus entry by 1 via ageSpeaker and remember lastBeginS1.
void cSource::ageSpeakersPerSentence(int where, int & endMetaResponse, unsigned int & agingStructuresSeen, int & lastBeginS1, const bool inPrimaryQuote, const bool inSecondaryQuote)
{
	int element;
	if ((element = m[where].pma.queryPattern(u"__S1")) != -1)
	{
		if (endMetaResponse < where && (endMetaResponse = detectMetaResponse(where, element & ~cMatchElement::patternFlag)) < 0)
		{
			lastBeginS1 = where;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d     aging speakers (%s) BeginS1", where, section, (inPrimaryQuote) ? u"inQuote" : u"outsideQuote");
			m[where].flags |= cWordMatch::flagAge;
			for (vector <cLocalFocus>::iterator lfi = localObjects.begin(); lfi != localObjects.end(); )
				ageSpeaker(where, inPrimaryQuote, inSecondaryQuote, lfi, 1);
			agingStructuresSeen++;
		}
	}
}

// Feed the current mention into mergeFocus (and mergeName for names).
// Records the first gendered subject of the paragraph on
// whereFirstSubjectInParagraph for question-subject agreement.
void cSource::getWhereFirstSubjectInParagraph(int where, const bool inPrimaryQuote, const bool inSecondaryQuote, const bool currentIsQuestion, vector <int> &lastSubjects, int & whereFirstSubjectInParagraph)
{
	if (!currentIsQuestion && !(m[where].flags & cWordMatch::flagAdjectivalObject) && !(m[where].flags & cWordMatch::flagRelativeHead) &&
		// don't introduce gendered objects that have matched plural neuter objects (pictures)
		(m[where].getObject() < 0 || !objects[m[where].getObject()].plural || objects[m[where].getObject()].male || 
			objects[m[where].getObject()].female || !objects[m[where].getObject()].neuter || objects[m[where].getObject()].objectClass == BODY_OBJECT_CLASS))
	{
		bool clearBeforeSet = true, genderedSubjectOccurred = false;
		int currentObject = (m[where].objectMatches.size() == 1) ? m[where].objectMatches[0].object : m[where].getObject();
		if (currentObject >= 0 && m[where].objectMatches.empty())
		{
			genderedSubjectOccurred = mergeFocus(inPrimaryQuote, inSecondaryQuote, currentObject, where, lastSubjects, clearBeforeSet);
			if (objects[currentObject].objectClass == NAME_OBJECT_CLASS) mergeName(where, currentObject, tempSpeakerGroup.speakers);
		}
		else
			for (unsigned int s = 0; s < m[where].objectMatches.size(); s++)
				genderedSubjectOccurred |= mergeFocus(inPrimaryQuote, inSecondaryQuote, m[where].objectMatches[s].object, where, lastSubjects, clearBeforeSet);
		// gendered body objects can still match 'it', so genderedSubjectOccurred could still be set to true, but it should be false
		if (currentObject >= 0 && objects[currentObject].neuter && !objects[currentObject].male && !objects[currentObject].female)
			genderedSubjectOccurred = false;
		if (whereFirstSubjectInParagraph == -1 && genderedSubjectOccurred)
		{
			whereFirstSubjectInParagraph = where;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:QXQ whereFirstSubjectInParagraph set to %d.", where, whereFirstSubjectInParagraph);
		}
	}
}

// Replace any eliminated object index in sg.speakers with its followObjectChain
// survivor so later passes do not hold a dead object number.
void cSource::updateSpeakerObjects(cSpeakerGroup &sg)
{
	// Make sure tempSpeakerGroup has objects up-to-date
	for (set <int>::iterator q = sg.speakers.begin(), qEnd = sg.speakers.end(); q != qEnd; )
		if (objects[*q].eliminated)
		{
			int tmp = *q;
			followObjectChain(tmp);
			sg.speakers.erase(q++);
			sg.speakers.insert(tmp);
		}
		else q++;
}

// identify definite speakers and subjects in narration.
// speakers and subjects must be name objects only.
// no objects are actually resolved.
// establish speaker groups
// text is broken into series of quotes
// each series has a collection of definite speakers
// a series is begun by a paragraph ending with no quotes.
// if the series ends with only one speaker identified, and that speaker is
// in the previous series, then extend the previous series to the end of the current one.
// if the series has the same speakers as the previous, extend the previous one.
// resolve all objects outside quotes.
// Single left-to-right scan of m[].  Per position: age local focus, resolve
// objects outside quotes (and possible hails), associate nyms/possessions,
// detect POV / hail / groups / embedded stories, and at quote / EOS /
// paragraph / chapter boundaries call the helpers above.  After the scan,
// eraseAliasesAndReplacementsInSpeakerGroups and distributePOV finalize the
// groups.  Does not write speakerPosition / audiencePosition (resolveSpeakers).
void cSource::identifySpeakerGroups()
{
	LFS
		bool quotesSeen = false, quotesSeenSinceLastSentence = false, endOfSentence = false, immediatelyAfterEndOfParagraph = true, inPrimaryQuote = false, inSecondaryQuote = false, currentIsQuestion = false;
	bool firstQuotedSentenceOfSpeakerGroupNotSeen = true, transitionSinceEOS = false;
	int lastSentenceEnd = 0, uqLastSentenceEnd = 0, uqPreviousToLastSentenceEnd = 0, lastQuotedString = -1, lastSpeakerGroupOfPreviousSection = -1, lastBeginS1 = -1, lastRelativePhrase = -1, lastQ2 = -1, lastVerb = -1, lastCommand = -1;
	int lastSentenceEndBeforeAndNotIncludingCurrentQuote = -1, lastProgressPercent = -1;
	int questionSpeaker = -1, questionSpeakerLastParagraph = -1, questionSpeakerLastSentence = -1, whereFirstSubjectInParagraph = -1; // question processing
	int quotedObjectCounter = 0, lastSpeakerGroupPositionConsidered = 0; // lastSectionEnd=-1,
	int endMetaResponse = -1, lastSpeakerPosition = -1, lastEmbeddedImposedSpeakerPosition = -1;
	int lastWhereMPluralGroupedObject = -1;
	// embedded stories
	int numPastSinceLastQuote = 0, numNonPastSinceLastQuote = 0, numFirstInQuote = 0, numSecondInQuote = 0, lastEmbeddedStory = -1;
	vector <int> lastSubjects, previousLastSubjects, groupedObjects, secondaryQuotesResolutions;
	vector < vector <cTagLocation> > tagSets;
	set <int>::iterator stsi;
	unsigned int agingStructuresSeen = 0;
	lpwstring tmpstr, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6;
	lastBeginS1 = -1;
	lastRelativePhrase = -1;
	lastQ2 = -1;
	lastVerb = -1;
	lastCommand = -1;
	lastOpeningSecondaryQuote = -1;
	lastOpeningPrimaryQuote = -1;
	section = 0;
	for (int I = 0; I < m.size() && !exitNow; I++)
	{
		if ((int)(I * 100 / m.size()) > lastProgressPercent)
		{
			lastProgressPercent = (int)I * 100 / m.size();
			lp_wprintf(u"PROGRESS: %03d%% speakers identified with %d seconds elapsed \r", lastProgressPercent, clocksec());
		}
		debugTrace = m[I].t;
		logCache = m[I].logCache;
		// this must be done before resolveObject, because we don't want such an object to be in localObjects:
		// an object that has PLEONASTIC_SUBJECT set, yet has a relative clause with a head that has the POS role set
		//   should likely not be added to localObjects.
		// There was a man who would unerringly ferret out Tuppence's whereabouts .
		if ((m[I].objectRole & SUBJECT_PLEONASTIC_ROLE) && m[I].getObject() >= 0 && objects[m[I].getObject()].whereRelativeClause >= 0 &&
			m[objects[m[I].getObject()].whereRelativeClause].getRelVerb() >= 0 && (m[m[objects[m[I].getObject()].whereRelativeClause].getRelVerb()].verbSense & VT_POSSIBLE))
		{
			m[I].objectRole &= ~SUBJECT_PLEONASTIC_ROLE;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:Removed pleonastic role", I);
		}
		if (m[I].pma.queryPattern(u"_REL1") != -1)
		{
			lastRelativePhrase = I;
			if (m[I].word->first == u"whose")
			{
				cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || inPrimaryQuote, m[I].getObject() == cObject::eOBJECTS::OBJECT_UNKNOWN_NEUTER, objectToBeMatchedInQuote, quoteIndependentAge);
				resolveAdjectivalObject(I, false, inPrimaryQuote, inSecondaryQuote, lastBeginS1, false);
			}
		}
		if (m[I].pma.queryPattern(u"_Q2") != -1)
			lastQ2 = I;
		if (m[I].pma.queryPattern(u"_COMMAND1") != -1)
			lastCommand = I;
		if (m[I].hasVerbRelations)
			lastVerb = I;
		ageSpeakersPerSentence(I, endMetaResponse, agingStructuresSeen, lastBeginS1, inPrimaryQuote, inSecondaryQuote);
		// implicit objects - another knock
		if (implicitObject(I))
		{
			objects[m[I].getObject()].neuter = false;
			objects[m[I].getObject()].male = objects[m[I].getObject()].female = true;
			objects[m[I].getObject()].objectClass = GENDERED_GENERAL_OBJECT_CLASS;
		}
		associateNyms(I); // (ASSOC)
		associatePossessions(I);
		setPOVStatus(I, inPrimaryQuote, inSecondaryQuote);
		// detect whether the name is possibly a hail candidate, and possibly needs to be resolved with other former names before being detected as a hail
		// �[st:dr] Miss Finn , � he said
		// �[st:julius] Mr . Hersheimmer , � he[st] said at last , �[st:julius] that is a very large sum . � 
		bool possibleHail = inPrimaryQuote && m[I].getObject() >= 0 && !(m[I].objectRole & HAIL_ROLE) && objects[m[I].getObject()].objectClass == NAME_OBJECT_CLASS &&
			m[I].beginObjectPosition && m[m[I].beginObjectPosition - 1].word->first == u"�" && m[m[I].endObjectPosition].word->first == u"," && m[m[I].endObjectPosition + 1].word->first == u"�";
		if ((!inPrimaryQuote && !inSecondaryQuote) || (possibleHail && !objects[m[I].getObject()].PISDefinite))
			resolveObject(I, false, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, false, false, false); // could change object at I!
		if (m[I].getObject() != cObject::eOBJECTS::UNKNOWN_OBJECT && lastCommand >= 0)
			m[I].objectRole |= IN_COMMAND_OBJECT_ROLE;
		if (!narrativeIsQuoted)
			adjustHailRoleDuringScan(I);
		if (m[I].objectMatches.size() == 1 && m[I].getObject() >= 0)
			moveNyms(I, m[I].objectMatches[0].object, m[I].getObject(), u"identifySpeakerGroups");
		identifyMetaNameEquivalence(I, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb);
		identifyMetaSpeaker(I, inPrimaryQuote | inSecondaryQuote);
		identifyAnnounce(I, inPrimaryQuote | inSecondaryQuote);
		identifyMetaGroup(I, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb);
		// Wikipedia
		if (!inPrimaryQuote && !inSecondaryQuote && sourceType != PATTERN_TRANSFORM_TYPE)
			identifyISARelation(I, true, RDFFileCaching);
		detectTimeTransition(I, lastSubjects);
		defineObjectAsSpatial(I);
		detectTenseAndFirstPersonUsage(I, lastBeginS1, lastRelativePhrase, numPastSinceLastQuote, numNonPastSinceLastQuote, numFirstInQuote, numSecondInQuote, inPrimaryQuote);
		// CMREADME20
		if (!currentIsQuestion && !(m[I].flags & cWordMatch::flagAdjectivalObject) && !(m[I].flags & cWordMatch::flagRelativeHead) &&
			!(m[I].getObject() < 0 || !objects[m[I].getObject()].plural || objects[m[I].getObject()].male || objects[m[I].getObject()].female || !objects[m[I].getObject()].neuter || objects[m[I].getObject()].objectClass == BODY_OBJECT_CLASS) &&
			debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:rejected all objects matched to %s for speaker focus.", I, objectString(m[I].getObject(), tmpstr, false).c_str());
		getWhereFirstSubjectInParagraph(I, inPrimaryQuote, inSecondaryQuote, currentIsQuestion, lastSubjects, whereFirstSubjectInParagraph);
		// CMREADME21
		if (!inPrimaryQuote && !inSecondaryQuote) // avoid accumulating groups of objects in quotes that have not been disambiguated or resolved yet (and might not match or be invalid)
			accumulateGroups(I, groupedObjects, lastWhereMPluralGroupedObject);
		// CMREADME22
		updateSpeakerObjects(tempSpeakerGroup);
		// CMREADME23
		identifyHailObjects(I, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, inPrimaryQuote, inSecondaryQuote);
		// CMREADME24
		if (I < endMetaResponse) continue;
		if (m[I].word->first == u"lptable" || m[I].word->first == u"lpendcolumn")
		{
			localObjects.clear();
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:cleared local objects (%s)", I, m[I].word->first.c_str());
		}
		// CMREADME25
		if (m[I].word->first == u"�")
		{
			if (lastOpeningSecondaryQuote >= 0)
			{
				m[lastOpeningSecondaryQuote].nextQuote = I;
				m[I].previousQuote = lastOpeningSecondaryQuote;
			}
			lastOpeningSecondaryQuote = I;
			inSecondaryQuote = true;
			inPrimaryQuote = false;
		}
		else if (m[I].word->first == u"�")
		{
			inSecondaryQuote = false;
			inPrimaryQuote = true;
			if (lastOpeningSecondaryQuote >= 0)
			{
				m[lastOpeningSecondaryQuote].endQuote = I;
				setSecondaryQuoteString(I, secondaryQuotesResolutions);
			}
		}
		else if (m[I].word->first == u"�")
		{
			if (immediatelyAfterEndOfParagraph && lastQuote >= 0)
				embeddedStory(I, numPastSinceLastQuote, numNonPastSinceLastQuote, numSecondInQuote, numFirstInQuote,
					lastEmbeddedStory, lastEmbeddedImposedSpeakerPosition, lastSpeakerPosition);
			lastOpeningPrimaryQuote = I;
			lastSentenceEndBeforeAndNotIncludingCurrentQuote = lastSentenceEnd;
			inPrimaryQuote = true;
			quotesSeen = quotesSeenSinceLastSentence = true;
		}
		else if (m[I].word->first == u"�" && lastOpeningPrimaryQuote >= 0)
		{
			processEndOfPrimaryQuote(I, lastSentenceEndBeforeAndNotIncludingCurrentQuote,
				lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, lastSpeakerPosition, lastQuotedString, quotedObjectCounter,
				inPrimaryQuote, immediatelyAfterEndOfParagraph, firstQuotedSentenceOfSpeakerGroupNotSeen, quotesSeenSinceLastSentence,
				lastSubjects);
		}
		// CMREADME26
		else if (isEOS(I))
		{
			processEndOfSentence(I, lastBeginS1, lastRelativePhrase, lastCommand, lastSentenceEnd, uqPreviousToLastSentenceEnd, uqLastSentenceEnd,
				questionSpeakerLastSentence, questionSpeaker, currentIsQuestion, inPrimaryQuote, inSecondaryQuote, endOfSentence, transitionSinceEOS,
				agingStructuresSeen, quotesSeenSinceLastSentence, lastSubjects, previousLastSubjects);
		}
		// CMREADME27
		else if (m[I].word == Words.sectionWord)
		{
			endSection(questionSpeakerLastParagraph, questionSpeakerLastSentence, whereFirstSubjectInParagraph, lastSpeakerGroupPositionConsidered, lastSpeakerGroupOfPreviousSection, I,
				endOfSentence, immediatelyAfterEndOfParagraph, quotesSeenSinceLastSentence, inSecondaryQuote, inPrimaryQuote, quotesSeen, firstQuotedSentenceOfSpeakerGroupNotSeen,
				previousLastSubjects);
		}
		// CMREADME28
		if ((section + 1 < sections.size() && I == sections[section + 1].begin) || (section < sections.size() && I == m.size() - 1))
		{
			beginSection(lastSpeakerGroupPositionConsidered, lastSpeakerGroupOfPreviousSection, I, previousLastSubjects, lastSubjects);
		}
	}
	// CMREADME29
	eraseAliasesAndReplacementsInSpeakerGroups();
	distributePOV();
	if (section < sections.size())
		clearNextSection(m.size(), section);
	lp_wprintf(u"PROGRESS: 100%% speakers identified with %d seconds elapsed \n", clocksec());
	if (debugTrace.traceSpeakerResolution || TSROverride)
	{
		lplog(LOG_SG, u"SPEAKER GROUPS [LIST]");
		for (unsigned int I = 0; I < speakerGroups.size(); I++)
		{
			translateBodyObjects(speakerGroups[I]);
			lplog(LOG_SG, u"%d: %s", I, toText(speakerGroups[I], tmpstr));
		}
	}
}
