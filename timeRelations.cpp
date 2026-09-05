/*
	timeRelations.cpp - TimeML-style date/time patterns, cTimeInfo fill, tense
		decode, and speaker-group aging on time transitions

	Overview:
		Registers the _TIME / _DATE / _MLT hand patterns, maps matched tags
		(HOUR, MONTH, DAYWEEK, TIMETYPE, ?) onto a cTimeInfo (capacity,
		absolute fields, T_BEFORE/T_AFTER/T_RANGE, ?), and hangs that
		cTimeInfo on the current cSyntacticRelationGroup. Also seeds the
		lexicon with time-word flags (createTimeCategories / addTimeFlags),
		decodes VerbNet-style vS/vB/vC tags into VT_* verbSense bits
		(getVerbTense), and ages local-focus entities when a clause is a
		time or space transition (ageTransition). Timeline segments are
		opened at speaker-group boundaries.

	Pipeline position:
		Initialization: defineTimePatterns() with the other pattern
		builders; createTimeCategories() from dictionary setup.
		Stage 5: appendTime() is called from srSetTimeFlowTense() after an
		SRG is inserted. detectTimeTransition() (the lastSubjects overload)
		runs in the speaker-resolution walk. getVerbTense() is used when
		verbSense is assigned during parse.

	Key entry points:
		- defineTimePatterns() - _TIME / _DATE pattern table
		- identifyDateTime() / evaluateDateTime() - tag-set -> cTimeInfo
		- appendTime() - attach timeInfo to one SRG and set timeProgression
		- detectTimeTransition() - flip tft.timeTransition / age speakers
		- getVerbTense() - pattern tags -> VT_* (Quirk / Reichenbach)
		- createTimeCategories() / addTimeFlags() - lexicon time flags
		- ageTransition() - drop physicallyPresent / salience on a jump

	Key data structures / globals:
		- twsCapacity / months / daysOfWeek / seasons / holidayDays -
		  lookup tables for whichCapacity / whichMonth / ?
		- tagSetTimeArray - scratch buffer for getVerbTense (tmalloc)
		- timelineSegments - cSource member, seeded by
		  initializeTimelineSegments()

	Dependencies:
		cPattern, Words lexicon, VerbNet vbNetClasses, MySQL only
		indirectly (via cSource). Holiday names are compiled in.

	Notes / gotchas:
		- twsCapacity is index-aligned with eCapacity; adding to one requires adding
		  to the other (it was previously missing NamedHoliday).
		- months_abb deliberately excludes may/jun/jul as lexicon entries (see the
		  comment above its definition); months_abb_index maps each remaining
		  abbreviation onto months[] (aug->7, not 4).
		- LFS at every function entry.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "ontology.h"
#include "source.h"
#include "profile.h"

// Register _TIME / _DATE / _MLT / INTERVAL patterns (2:37 A.M., June 20th,
// ?two weeks ago today?, ?between now and Monday?, ?). Called once at init.
void defineTimePatterns()
{
	LFS
		// 2:37 A.M.
		cPattern::create(u"_TIME", u"1", 1, u"time{HOUR}", 0, 1, 1,
			1, u"_TIME_ABB{TIMESPEC}", 0, 1, 1, 0);
	// 2 A.M. / six p.m.
	cPattern::create(u"__PRETIME", u"",
		1, u"Number{HOUR}", 0, 1, 1,
		1, u"dash", 0, 1, 1, 0);
	cPattern::create(u"_TIME", u"2",
		1, u"__PRETIME", 0, 0, 1,
		2, u"numeral_cardinal{HOUR}", u"Number{HOUR}", 0, 1, 1,
		1, u"_TIME_ABB{TIMESPEC}", 0, 1, 1, 0);
	// nine thirty
	cPattern::create(u"_TIME", u"C",
		1, u"numeral_cardinal{HOUR}", 0, 1, 1,
		1, u"dash", 0, 0, 1,
		1, u"numeral_cardinal{MINUTE}", 0, 1, 1,
		1, u"_TIME_ABB{TIMESPEC}", 0, 0, 1, 0);
	// 2 o'clock / two o'clock 
	cPattern::create(u"_TIME", u"3",
		3, u"adverb|well{TIMEMODIFIER}", u"adverb|almost{TIMEMODIFIER}", u"adverb|nearly{TIMEMODIFIER}", 0, 0, 1,
		3, u"adverb|before{TIMEMODIFIER}", u"adverb|after{TIMEMODIFIER}", u"preposition|past*-1{TIMEMODIFIER}", 0, 0, 1,
		2, u"numeral_cardinal{HOUR}", u"Number{HOUR}", 0, 1, 1,
		1, u"adverb|o'clock*-2", 0, 1, 1, 0);
	// 'some' 5 minutes to/till _TIME / 12
	// a few minutes past 11
	// two days from now / two days after Friday evening / two days from next Tuesday
	// five days after Christmas
	// the summer of 1964 
	// twenty after twelve
	cPattern::create(u"_TIME", u"4",
		1, u"adverb", 0, 0, 1, // they had lunch [together] two weeks ago today
		1, u"determiner{DET}", 0, 0, 1,
		2, u"quantifier{TIMEMODIFIER}", u"adjective{TIMEMODIFIER}", 0, 0, 1, // a little after half-past 5
		3, u"numeral_cardinal*-1{TIMEMODIFIER}", u"Number*-1{TIMEMODIFIER}", u"adjective{TIMEMODIFIER}", 0, 0, 1,
		9, u"month*-2{MONTH}", u"daysOfWeek*-2{DAYWEEK}", u"season*-2{SEASON}", u"timeUnit*-2{TIMECAPACITY}", u"dayUnit*-2{TIMECAPACITY}", u"numeral_cardinal*-1{MINUTE}",
		u"adjective|half*-2{TIMEMODIFIER}", u"adverb|little{TIMEMODIFIER}", u"noun|quarter{TIMEMODIFIER}", 0, 1, 1, // half past noon // a little after 11
	// to, past, before, till, etc
		3, u"_ADVERB{TIMETYPE}", u"preposition|past*-1{TIMETYPE}", u"preposition{TIMETYPE}", 0, 1, 1,
		9, u"_TIME[*]*-1", u"_DATE[*]*-1", u"numeral_cardinal*-1{HOUR}", u"Number*-1{HOUR}", u"adverb|now{HOUR}", u"daysOfWeek*-2{DAYWEEK}", u"season*-2{SEASON}", u"holiday*-2{HOLIDAY}", u"dayUnit*-2{TIMECAPACITY}", 0, 1, 1,
		0);
	// early one Friday night in Fall 1998.
	cPattern::create(u"_TIME", u"D",
		1, u"determiner{DET}", 0, 0, 1,
		1, u"_ADJECTIVE{TIMEMODIFIER}", 0, 0, 1,
		1, u"quantifier{TIMEMODIFIER}", 0, 0, 1,
		1, u"daysOfWeek*-2{DAYWEEK}", 0, 1, 1,
		1, u"dayUnit*-2{TIMECAPACITY}", 0, 1, 1,
		// to, past, before, till, etc
		3, u"_ADVERB{TIMETYPE}", u"preposition|past*-1{TIMETYPE}", u"preposition{TIMETYPE}", 0, 1, 1,
		8, u"_TIME[*]*-1", u"_DATE[*]*-1", u"numeral_cardinal*-1{HOUR}", u"Number*-1{HOUR}", u"adverb|now{HOUR}", u"daysOfWeek*-2{DAYWEEK}", u"season*-2{SEASON}", u"holiday*-2{HOLIDAY}", 0, 1, 1,
		0);
	// three hours after the meeting 
	// the first day of each month in 1999

	// 4 times a week / 4 times each week / 4 times every week
	cPattern::create(u"_TIME", u"5",
		1, u"adverb", 0, 0, 1, // they had lunch [together] two weeks ago today
		2, u"numeral_cardinal{TIMEMODIFIER}", u"Number{TIMEMODIFIER}", 0, 1, 1,
		1, u"noun|times{TIMEMODIFIER}", 0, 1, 1,
		2, u"determiner|a", u"quantifier", 0, 1, 1,
		4, u"month*-2{MONTH}", u"season*-2{SEASON}", u"timeUnit*-2{TIMECAPACITY}", u"dayUnit*-2{TIMECAPACITY}", 0, 1, 1,
		0);
	// a quarter of an hour ago
	cPattern::create(u"_TIME", u"6",
		1, u"adverb", 0, 0, 1, // they had lunch [together] two weeks ago today
		1, u"determiner{DET}", 0, 0, 1,
		2, u"noun|quarter{TIMEMODIFIER}", u"predeterminer|half{TIMEMODIFIER}", 0, 0, 1,
		1, u"preposition|of", 0, 0, 1,
		1, u"determiner{DET}", 0, 1, 1,
		3, u"noun|hour*-2{TIMECAPACITY}", u"noun|minute*-2{TIMECAPACITY}", u"noun|second*-2{TIMECAPACITY}", 0, 1, 1,
		// to, past, before, till, ago, etc
		3, u"adverb{TIMETYPE}", u"to{TIMETYPE}", u"adjective|past{TIMETYPE}", 0, 1, 1,
		3, u"_TIME[*]*-1", u"numeral_cardinal*-1{HOUR}", u"Number*-1{HOUR}", 0, 0, 1,
		0);
	// the stroke of 11
	cPattern::create(u"_TIME", u"E",
		1, u"determiner{DET}", 0, 1, 1,
		1, u"noun|stroke", 0, 1, 1,
		1, u"preposition|of", 0, 1, 1,
		2, u"numeral_cardinal{HOUR}", u"Number{HOUR}", 0, 1, 1,
		0);
	/* already taken by _TIME[9]
	cPattern::create(u"_TIME",u"7",
		1,u"adverb",0,0,1, // they had lunch [together] two weeks ago today
		4,u"numeral_cardinal{TIMEMODIFIER}",u"Number{TIMEMODIFIER}",u"quantifier|some",u"determiner|a",0,1,1,
		5,u"month*-2{MONTH}",u"daysOfWeek*-2{DAYWEEK}",u"season*-2{SEASON}",u"timeUnit*-2{TIMECAPACITY}",u"dayUnit*-2{TIMECAPACITY}",0,1,1,
		0);
		*/
		// Friday evening/afternoon/night
	cPattern::create(u"_TIME", u"8",
		1, u"adverb", 0, 0, 1, // they had lunch [together] two weeks ago today
		1, u"daysOfWeek{DAYWEEK}", 0, 1, 1,
		1, u"dayUnit*-1{TIMECAPACITY}", 0, 1, 1, 0);
	// lunch-time, dinner-time
	cPattern::create(u"_TIME", u"F",
		2, u"noun|lunch{TIMECAPACITY}", u"noun|dinner{TIMECAPACITY}", 0, 1, 1,
		1, u"dash|-*-2", 0, 1, 1,
		1, u"noun|time", 0, 1, 1, 0);
	// part of the time, some of the time, half of the time, all of the time
	cPattern::create(u"_TIME", u"G",
		4, u"noun|part", u"quantifier|some", u"adjective|half", u"predeterminer|all", 0, 1, 1,
		1, u"preposition|of", 0, 1, 1,
		1, u"determiner|the", 0, 1, 1,
		1, u"noun|time", 0, 1, 1, 0);
	// half past noon / 2 / 12
	// next Tuesday - cannot make numeral_ordinal optional because this will create hanging CLOSING_S1s
	// 6 months
	// five days ago / a year ago 
	cPattern::create(u"_MLT", u"9",
		2, u"adverb|more{TIMEMODIFIER}", u"adverb|less{TIMEMODIFIER}", 0, 1, 1,
		1, u"preposition|than", 0, 1, 1,
		0);
	cPattern::create(u"_TIME", u"9",
		1, u"_MLT*-2", 0, 0, 1,
		1, u"determiner|the{DET}", 0, 0, 1, // the past few years
		3, u"adverb{TIMEMODIFIER}", u"quantifier{TIMEMODIFIER}", u"adjective|past*-1{TIMETYPE}", 0, 0, 1, // they had lunch [together] two weeks ago today / [every] two weeks
		7, u"numeral_ordinal*-1{TIMEMODIFIER}", u"numeral_cardinal*-1{TIMEMODIFIER}", u"Number*-1{TIMEMODIFIER}", u"demonstrative_determiner{TIMEMODIFIER}", u"quantifier{TIMEMODIFIER}", u"determiner|a", u"adjective|other{TIMEMODIFIER}", 0, 1, 1,
		6, u"month*-1{MONTH}", u"daysOfWeek*-1{DAYWEEK}", u"season*-1{SEASON}", u"timeUnit*-1{TIMECAPACITY}", u"dayUnit*-1{TIMECAPACITY}", u"holiday*-1{HOLIDAY}", 0, 1, 1,
		// to, past, before, till, ago, etc
		3, u"adverb{TIMETYPE}", u"to{TIMETYPE}", u"preposition|past*-1{TIMETYPE}", 0, 0, 1,
		1, u"adverb|today{TIMECAPACITY}", 0, 0, 1, // two weeks ago today
		0);
	// 11 in the morning
	cPattern::create(u"_TIME", u"A",
		1, u"adverb", 0, 0, 1, // they had lunch [together] two weeks ago today
		2, u"numeral_cardinal*-1{HOUR}", u"Number*-1{HOUR}", 0, 1, 1,
		1, u"preposition|in", 0, 1, 1,
		1, u"determiner{DET}", 0, 1, 1,
		1, u"dayUnit*-1{TIMECAPACITY}", 0, 1, 1, 0);
	// two weeks every three months
	cPattern::create(u"_TIME", u"B",
		2, u"numeral_cardinal{TIMEMODIFIER}", u"Number{TIMEMODIFIER}", 0, 1, 1,
		5, u"month*-2{MONTH}", u"daysOfWeek*-2{DAYWEEK}", u"season*-2{SEASON}", u"timeUnit*-2{TIMECAPACITY}", u"dayUnit*-2{TIMECAPACITY}", 0, 1, 1,
		1, u"quantifier|every", 0, 1, 1,
		2, u"numeral_cardinal{TIMEMODIFIER}", u"Number{TIMEMODIFIER}", 0, 0, 1,
		5, u"month*-2{MONTH}", u"daysOfWeek*-2{DAYWEEK}", u"season*-2{SEASON}", u"timeUnit*-2{TIMECAPACITY}", u"dayUnit*-2{TIMECAPACITY}", 0, 1, 1,
		0);
	// between now and Monday morning 
	// 6 months or a year from now
	cPattern::create(u"_TIME{MNOUN}", u"INTERVAL",
		1, u"_TIME{MOBJECT}", 0, 1, 1,
		1, u"coordinator*-1", 0, 1, 1, // prefer over _NOUN[O]
		1, u"_TIME{MOBJECT}", 0, 1, 1,
		0);

	// the 20th of June / the 20th of June, 1980 / the fourteenth of June, 1980
	cPattern::create(u"_DATE", u"1", 1, u"determiner", 0, 0, 1,
		1, u"numeral_ordinal{DAYMONTH}", 0, 1, 1,
		1, u"of", 0, 0, 1,
		1, u"month*-2{MONTH}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"Number{YEAR}", 0, 0, 1, 0);
	// fall 1964 
	cPattern::create(u"_DATE", u"2",
		1, u"season*-2{SEASON}", 0, 1, 1,
		1, u"Number{YEAR}", 0, 1, 1, 0);
	// June 20th / May 7 / June twentieth, 1984
	cPattern::create(u"_DATE", u"3",
		1, u"_TIME", 0, 0, 1,
		1, u",", 0, 0, 1,
		2, u"quantifier|every{TIMEMODIFIER}", u"quantifier|each{TIMEMODIFIER}", 0, 0, 1,
		1, u"month*-2{MONTH}", 0, 1, 1,
		2, u"numeral_ordinal{DAYMONTH}", u"Number{DAYMONTH}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"Number*-1{YEAR}", 0, 0, 1, 0);
	// 2967 A.D.
	cPattern::create(u"_DATE", u"4", 1, u"Number{YEAR}", 0, 1, 1,
		1, u"_DATE_ABB{DATESPEC}", 0, 1, 1, 0);
	// Friday, 8 p.m.
	cPattern::create(u"_DATE", u"5", 1, u"daysOfWeek*-1{DAYWEEK}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"_TIME", 0, 1, 1, 0);
	// 1st October 1980
	cPattern::create(u"_DATE", u"6", 2, u"numeral_ordinal{DAYMONTH}", u"Number{DAYMONTH}", 0, 1, 1,
		1, u"month*-1{MONTH}", 0, 1, 1,
		1, u"Number{YEAR}", 0, 1, 1, 0);
	// June 20th, 1980 / May 7, 1980 / May 6-8
	cPattern::create(u"_DATE", u"7", 1, u"month*-1{MONTH}", 0, 1, 1,
		2, u"numeral_ordinal{DAYMONTH}", u"Number{DAYMONTH}", 0, 1, 1,
		1, u",", 0, 0, 1,
		1, u"Number{YEAR}", 0, 1, 1, 0);
	cPattern::create(u"_DATE", u"8", 1, u"month*-1{MONTH}", 0, 1, 1,
		2, u"numeral_ordinal{DAYMONTH}", u"Number{DAYMONTH}", 0, 1, 1,
		1, u"-*-1", 0, 1, 1,
		2, u"numeral_ordinal{DAYMONTH}", u"Number{DAYMONTH}", 0, 1, 1, 0);
	cPattern::create(u"_DATE", u"9", 1, u"to", 0, 1, 1,
		1, u"dash|-*-2", 0, 1, 1,
		2, u"noun|day{TIMECAPACITY}", u"noun|morrow{TIMECAPACITY}", 0, 1, 1, 0);
}

// Parse m[where] as an hour (1?12 cardinal/Number) or HH:MM via processTime.
// A bare Number outside 1?12 is treated as a year instead. False if the
// cardinal is out of range or looks like ?a hundred to one?.
bool cSource::evaluateHOUR(int where, cTimeInfo& t)
{
	LFS
		if (m[where].queryWinnerForm(u"Number") >= 0)
		{
			int num;
			if ((num = lp_wtoi(m[where].word->first.c_str())) >= 1 && num <= 12)
				t.absHour = num;
			else
				t.absYear = num;
		}
		else if (m[where].queryWinnerForm(u"numeral_cardinal") >= 0)
		{
			t.absHour = mapNumeralCardinal(m[where].word->first);
			if (t.absHour < 1 || t.absHour>12)
			{
				t.absHour = -1;
				return false;
			}
			// a hundred to one / a thousand to one
			if (where > 2 && m[where - 1].word->first == u"to" &&
				m[where - 2].queryWinnerForm(u"numeral_cardinal") >= 0 && mapNumeralCardinal(m[where - 2].word->first) > t.absHour)
				return false;
		}
		else
			cWord::processTime(m[where].word->first, t.absHour, t.absMinute);
	return true;
}

// TIMESPEC tag: A.M. -> absTimeSpec 0, anything else (P.M.) -> 1. Sets tSet.
void cSource::evaluateDateTimeTimeSpec(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet)
{
	int ti;
	if ((ti = findOneTag(tagSet, u"TIMESPEC", -1)) >= 0) // A.M. / P.M.
	{
		tSet = true;
		if (!lp_wcscasecmp(m[tagSet[ti].sourcePosition].word->first.c_str(), u"A.M."))
			t.absTimeSpec = 0;
		else
			t.absTimeSpec = 1;
	}
}

// DATESPEC tag: A.D. -> absDateSpec 0, otherwise B.C. -> 1. Sets tSet.
void cSource::evaluateDateTimeDateSpec(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet)
{
	int ti;
	if ((ti = findOneTag(tagSet, u"DATESPEC", -1)) >= 0) // A.D. / B.C.
	{
		tSet = true;
		if (!lp_wcscasecmp(m[tagSet[ti].sourcePosition].word->first.c_str(), u"A.D."))
			t.absDateSpec = 0;
		else
			t.absDateSpec = 1;
	}
}

// TIMEMODIFIER tags -> timeModifier / timeModifier2. Recurrence words
// (?daily?) clear the modifier and set T_RECURRING + timeFrequency.
void cSource::evaluateDateTimeModifier(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet)
{
	int nextTag = -1, ti;
	if ((ti = findTag(tagSet, u"TIMEMODIFIER", nextTag)) >= 0)
	{  // text offset
		tSet = true;
		t.timeModifier = tagSet[ti].sourcePosition;
		if ((t.timeFrequency = whichRecurrence(m[t.timeModifier].word->first)) != cUnspecified)
		{
			t.timeModifier = -1;
			t.timeRelationType = T_RECURRING;
		}
		else
			t.timeFrequency = -1;
	}
	if (nextTag >= 0)
	{
		tSet = true;
		t.timeModifier2 = tagSet[nextTag].sourcePosition;
		if (t.timeFrequency == -1)
		{
			if ((t.timeFrequency = whichRecurrence(m[t.timeModifier2].word->first)) != cUnspecified)
			{
				t.timeModifier2 = -1;
				t.timeRelationType = T_RECURRING;
			}
			else
				t.timeFrequency = -1;
		}
	}
}

// TIMECAPACITY tags -> t.timeCapacity / rt.timeCapacity via whichCapacity.
void cSource::evaluateDateTimeCapacity(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti;
	if ((ti = findTag(tagSet, u"TIMECAPACITY", nextTag)) >= 0) // hour / minute/ second/ dayUnit
	{
		tSet = true;
		t.timeCapacity = (eCapacity)whichCapacity(m[tagSet[ti].sourcePosition].deriveMainEntry(tagSet[ti].sourcePosition, 31, false, true, lastNounNotFound, lastVerbNotFound)->first);
		if (nextTag >= 0)
		{
			rtSet = true;
			rt.timeCapacity = (eCapacity)whichCapacity(m[tagSet[nextTag].sourcePosition].deriveMainEntry(tagSet[nextTag].sourcePosition, 32, false, true, lastNounNotFound, lastVerbNotFound)->first);
		}
	}
}

// TIMETYPE tag: ?to?/?past? or the word?s timeFlags become timeRelationType.
// Flagless adverbs are stored as timeModifier instead.
void cSource::evaluateDateTimeType(vector <cTagLocation>& tagSet, cTimeInfo& t, bool& tSet)
{
	int ti = -1, s;
	if ((ti = findOneTag(tagSet, u"TIMETYPE", -1)) >= 0) // adverb/to/past
	{
		tSet = true;
		lpwstring w = m[s = tagSet[ti].sourcePosition].word->first;
		int timeFlags = m[s].word->second.timeFlags;
		if (w == u"to")
			t.timeRelationType = T_BEFORE;
		else if (w == u"past")
			t.timeRelationType = T_AFTER;
		else if (timeFlags > 0)
			t.timeRelationType = (eTimeWordFlags)timeFlags;
		if (timeFlags == 0) // adverb
		{
			if (t.timeModifier < 0)
				t.timeModifier = s;
			else
				t.timeModifier2 = s;
		}
	}
}

// HOUR tags through evaluateHOUR. Default capacity is cHour. False if the
// first hour token is rejected.
bool cSource::evaluateDateTimeHour(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti;
	if ((ti = findTag(tagSet, u"HOUR", nextTag)) >= 0)
	{
		// Number / numeral_cardinal / time
		if (!evaluateHOUR(tagSet[ti].sourcePosition, t))
			return false;
		tSet = true;
		if (t.timeCapacity == cUnspecified)
			t.timeCapacity = cHour;
		if (nextTag >= 0) // Number / numeral_cardinal / time
			rtSet = evaluateHOUR(tagSet[nextTag].sourcePosition, rt);
	}
	return true;
}

// DAYMONTH tags -> absDayOfMonth (Number or numeral_ordinal).
void cSource::evaluateDateTimeDayMonth(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti, s;
	if ((ti = findTag(tagSet, u"DAYMONTH", nextTag)) >= 0) // numeral_cardinal / Number
	{
		tSet = true;
		if (m[s = tagSet[ti].sourcePosition].queryWinnerForm(u"Number") >= 0)
			t.absDayOfMonth = lp_wtoi(m[s].word->first.c_str());
		else if (m[s].queryWinnerForm(u"numeral_ordinal") >= 0)
			t.absDayOfMonth = mapNumeralOrdinal(m[s].word->first);
		if (nextTag >= 0)
		{
			rtSet = true;
			if (m[s = tagSet[nextTag].sourcePosition].queryWinnerForm(u"Number") >= 0)
				rt.absDayOfMonth = lp_wtoi(m[s].word->first.c_str());
			else if (m[s].queryWinnerForm(u"numeral_ordinal") >= 0)
				rt.absDayOfMonth = mapNumeralOrdinal(m[s].word->first);
		}
	}
}

// MINUTE tags -> absMinute. False if out of 1..59, or the token is
// followed by ?o'clock? (then the minute is rewritten as an hour).
bool cSource::evaluateDateTimeDayMinute(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti, s;
	// at five to 6 tomorrow.
// NOT from 5 to 6 tomorrow.
// NOT 12 o'clock _DATE
// not one of these days
	if ((ti = findTag(tagSet, u"MINUTE", nextTag)) >= 0) // numeral_cardinal / Number
	{
		tSet = true;
		if (m[s = tagSet[ti].sourcePosition].queryWinnerForm(u"Number") >= 0)
			t.absMinute = lp_wtoi(m[s].word->first.c_str());
		else if (m[s].queryWinnerForm(u"numeral_cardinal") >= 0)
			t.absMinute = mapNumeralCardinal(m[s].word->first);
		if (nextTag >= 0)
		{
			rtSet = true;
			if (m[s = tagSet[nextTag].sourcePosition].queryWinnerForm(u"Number") >= 0)
				rt.absMinute = lp_wtoi(m[s].word->first.c_str());
			else if (m[s].queryWinnerForm(u"numeral_cardinal") >= 0)
				rt.absMinute = mapNumeralCardinal(m[s].word->first);
		}
		if (t.absMinute < 1 || t.absMinute >= 60)
			return false;
		// NOT 12 o'clock _DATE
		if (!rtSet && t.absMinute >= 0 && s >= 0 && m[s + 1].word->first == u"o'clock")
		{
			t.absHour = t.absMinute;
			t.absMinute = -1;
		}
	}
	return true;
}

// MONTH tags -> absMonth via whichMonth.
void cSource::evaluateDateTimeMonth(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti;
	if ((ti = findTag(tagSet, u"MONTH", nextTag)) >= 0) // month
	{
		tSet = true;
		t.absMonth = whichMonth(m[tagSet[ti].sourcePosition].getMainEntry()->first);
		if (nextTag >= 0) // month
		{
			rtSet = true;
			rt.absMonth = whichMonth(m[tagSet[nextTag].sourcePosition].getMainEntry()->first);
		}
	}
}

// Capitalized SEASON -> absSeason; YEAR Number -> absYear (both t and rt).
void cSource::evaluateDateTimeSeason(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti, s;
	if ((ti = findTag(tagSet, u"SEASON", nextTag)) >= 0 && (m[tagSet[ti].sourcePosition].flags & cWordMatch::flagFirstLetterCapitalized)) // season
	{
		tSet = (t.absSeason = whichSeason(m[tagSet[ti].sourcePosition].getMainEntry()->first)) >= 0;
		if (nextTag >= 0 && (m[tagSet[nextTag].sourcePosition].flags & cWordMatch::flagFirstLetterCapitalized)) // season
			rtSet = (rt.absSeason = whichSeason(m[tagSet[nextTag].sourcePosition].getMainEntry()->first)) >= 0;
	}
	nextTag = -1;
	if ((ti = findTag(tagSet, u"YEAR", nextTag)) >= 0) // numeral_cardinal / Number
	{
		tSet = true;
		if (m[s = tagSet[ti].sourcePosition].queryWinnerForm(u"Number") >= 0)
			t.absYear = lp_wtoi(m[s].word->first.c_str());
		if (nextTag >= 0 && m[s = tagSet[nextTag].sourcePosition].queryWinnerForm(u"Number") >= 0)
		{
			rtSet = true;
			rt.absYear = lp_wtoi(m[s].word->first.c_str());
		}
	}
}

// HOLIDAY tags -> absHoliday via whichHoliday.
void cSource::evaluateDateTimeHoliday(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti;
	if ((ti = findTag(tagSet, u"HOLIDAY", nextTag)) >= 0) // numeral_cardinal / Number
	{
		tSet = true;
		t.absHoliday = whichHoliday(m[tagSet[ti].sourcePosition].getMainEntry()->first);
		if (nextTag >= 0)
		{
			rtSet = true;
			rt.absHoliday = whichHoliday(m[tagSet[nextTag].sourcePosition].getMainEntry()->first);
		}
	}
}

// DAYWEEK tags -> absDayOfWeek via whichDayOfWeek.
void cSource::evaluateDateTimeDayWeek(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& tSet, bool& rtSet)
{
	int nextTag = -1, ti;
	if ((ti = findTag(tagSet, u"DAYWEEK", nextTag)) >= 0) // daysOfWeek
	{
		tSet = true;
		t.absDayOfWeek = whichDayOfWeek(m[tagSet[ti].sourcePosition].getMainEntry()->first);
	}
	if (nextTag >= 0) // daysOfWeek
	{
		rtSet = true;
		rt.absDayOfWeek = whichDayOfWeek(m[tagSet[nextTag].sourcePosition].getMainEntry()->first);
	}
}

// ?from 5 to 6?: if a MINUTE is preceded by ?from?, treat t as the start
// hour and rt as the range end (T_RANGE).
void cSource::evaluateDateTimeMinute(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& rtSet)
{
	int nextTag = -1, ti, s;
	// from 5 to 6 tomorrow (see _TIME[4])
	if ((ti = findTag(tagSet, u"MINUTE", nextTag)) >= 0 && m[s = tagSet[ti].sourcePosition - 1].word->first == u"from" && t.absMinute > 0 && !rtSet)
	{
		rtSet = true;
		rt = t;
		t.clear();
		t.absHour = rt.absMinute;
		rt.absMinute = -1;
		rt.timeRelationType = T_RANGE;
	}
}

// Fill t (and optionally rt, with rt.timeRelationType = T_RANGE) from a
// collected _TIME/_DATE tag-set. False if no time tag fired or an hour/minute
// parse rejected the match.
bool cSource::evaluateDateTime(vector <cTagLocation>& tagSet, cTimeInfo& t, cTimeInfo& rt, bool& rtSet)
{
	LFS
	t.clear();
	rt.clear();
	rtSet = false;
	bool tSet = false;
	rt.timeRelationType = T_RANGE;
	evaluateDateTimeTimeSpec(tagSet, t, tSet);
	evaluateDateTimeDateSpec(tagSet, t, tSet);
	evaluateDateTimeModifier(tagSet, t, tSet);
	evaluateDateTimeCapacity(tagSet, t, rt, tSet, rtSet);
	evaluateDateTimeType(tagSet, t, tSet);
	if (!evaluateDateTimeHour(tagSet, t, rt, tSet, rtSet))
		return false;
	evaluateDateTimeDayMonth(tagSet, t, rt, tSet, rtSet);
	if (!evaluateDateTimeDayMinute(tagSet, t, rt, tSet, rtSet))
		return false;
	evaluateDateTimeMonth(tagSet, t, rt, tSet, rtSet);
	evaluateDateTimeSeason(tagSet, t, rt, tSet, rtSet);
	evaluateDateTimeHoliday(tagSet, t, rt, tSet, rtSet);
	evaluateDateTimeDayWeek(tagSet, t, rt, tSet, rtSet);
	evaluateDateTimeMinute(tagSet, t, rt, rtSet);
	return tSet;
}

// Copy capacity from previousTime and write `num` into the same kind of
// field (day/hour/year/?), used for ?the next year or two?.
// only copy previous time
// the next year or two.
void cSource::copyTimeInfoNum(vector <cTimeInfo>::iterator previousTime, cTimeInfo& t, int num)
{
	LFS
		t.timeCapacity = previousTime->timeCapacity;
	if (previousTime->absDayOfMonth >= 0)
		t.absDayOfMonth = num;
	else if (previousTime->absHour >= 0)
		t.absHour = num;
	else if (previousTime->absYear >= 0)
		t.absYear = num;
	else switch (t.timeCapacity)
	{
	case cMillenium: t.absYear = num; break;
	case cCentury: t.absYear = num; break;
	case cDecade: t.absYear = num; break;
	case cYear: t.absYear = num; break;
	case cSemester: t.absYear = num; break;
	case cSeason: t.absSeason = num; break;
	case cQuarter: t.absSeason = num; break;
	case cMonth: t.absMonth = num; break;
	case cWeek: t.absTimeSpec = num; break;
	case cDay: t.absTimeSpec = num; break;
	case cHour: t.absHour = num; break;
	case cMinute: t.absMinute = num; break;
	case cMorning: t.absTimeSpec = num; break;
	case cNoon: t.absTimeSpec = num; break;
	case cAfternoon: t.absTimeSpec = num; break;
	case cEvening: t.absTimeSpec = num; break;
	case cDusk: t.absTimeSpec = num; break;
	case cNight: t.absTimeSpec = num; break;
	case cMidnight: t.absTimeSpec = num; break;
	case cDawn: t.absTimeSpec = num; break;
	case cMoment: t.absMoment = num; break;
	case cNamedDay: t.absNamedDay = num; break;
	case cNamedHoliday: t.absNamedHoliday = num; break;
	case cNamedMonth: t.absNamedMonth = num; break;
	case cNamedSeason: t.absNamedSeason = num; break;
	case cSecond: t.absSecond = num; break;
	case cToday: t.absToday = num; break;
	case cTomorrow: t.absTomorrow = num; break;
	case cTonight: t.absTonight = num; break;
	case cUnspecified: t.absUnspecified = num; break;
	case cYesterday: t.absYesterday = num; break;

	default: break;
	}
}

// Mark the object at `where` as a time object, then set tft.timeTransition
// (present happening + a concrete calendar/capacity) or
// nonPresentTimeTransition. Returns tft.timeTransition. BE-clauses with
// ?it was a beautiful day? are rejected.
bool cSource::detectTimeTransition(int where, vector <cSyntacticRelationGroup>::iterator csr, cTimeInfo& timeInfo)
{
	LFS
		if (m[where].getObject() >= 0)
		{
			int oc = objects[m[where].getObject()].objectClass;
			if (oc != NON_GENDERED_GENERAL_OBJECT_CLASS &&
				oc != NON_GENDERED_NAME_OBJECT_CLASS &&
				oc != META_GROUP_OBJECT_CLASS)
				return false;
			objects[m[where].getObject()].setIsTimeObject(true);
			lpwstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, u"%06d:made time object [DTT] %s", where, objectString(m[where].getObject(), tmpstr, false).c_str());
		}
	if (csr->relationType == stPREPTIME || csr->relationType == stPREPDATE || csr->relationType == stSUBJDAYOFMONTHTIME ||
		csr->relationType == stABSTIME || csr->relationType == stABSDATE || csr->relationType == stADVERBTIME)
	{
		csr->tft.timeTransition = true;
		int sr = (int)(csr - syntacticRelationGroups.begin());
		lpwstring description = u"detectTimeTransition";
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, u"%06d:time transition %s", where, srToText(sr, description).c_str());
	}
	else
		if (!timeInfo.metaDescriptive)
		{
			// BE
			// 003944:BE time transition BE:npr:SP[tuppence]S[that pay hire]V[ishas]O[so] 3944:type=SEQ M=this Capacity=morning 
			// 5082:It was a long time before ... [YES]
			// 031024:BE time transition BE:npr:SP[marguerite]S[day]V[is]O[friday] 31024:type=SEQ Capacity=NamedDay absDayOfWeek=5 
			// 36619: Sunday was her afternoon out. [NO - ambiguous] 
			// 048858:BE time transition BE:npr:SP[hall]S[it]V[is]O[matter] 48847:type=at Capacity=month metaDescriptive  48841:type=recurring  48849:type=recurring  48858:type=SEQ M=as Modifier2=48859 Capacity=year 
			// 61026: It was a little after half-past five. [YES]
			if (csr->relationType == stBE &&
				((csr->whereSubject < 0 || (m[csr->whereSubject].word->first != u"it" && !(m[csr->whereSubject].word->second.timeFlags & T_UNIT))) ||
					m[csr->whereSubject].getRelVerb() != csr->whereVerb ||
					// 51060: It was a beautiful day. [NO - ambiguous]
					(timeInfo.timeCapacity == cDay && (m[timeInfo.tWhere].objectRole & OBJECT_ROLE) && (timeInfo.timeModifier < 0 || m[timeInfo.timeModifier].queryWinnerForm(adjectiveForm) != -1)) ||
					(timeInfo.timeModifier >= 0 && (m[timeInfo.timeModifier].word->second.timeFlags & T_BEFORE))))
			{
				int sr = (int)(csr - syntacticRelationGroups.begin());
				lpwstring description = u"detectTimeTransition";
				if (timeInfo.absSeason >= 0 || timeInfo.absMonth >= 0 || timeInfo.absDayOfWeek >= 0 ||
					timeInfo.absDayOfMonth >= 0 || timeInfo.absHour >= 0 || timeInfo.absHoliday >= 0 ||
					(timeInfo.timeCapacity != cUnspecified && timeInfo.timeCapacity != cMoment && timeInfo.timeCapacity != cMinute && timeInfo.timeCapacity != cSecond &&
						// no daily / monthly  
						timeInfo.timeFrequency == -1))
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION, u"%06d:BE time transition rejected: %s", where, srToText(sr, description).c_str());
				}
				return false;
			}
			if (csr->whereVerb >= 0 && m[csr->whereVerb].previousCompoundPartObject >= 0 &&
				(m[m[csr->whereVerb].previousCompoundPartObject].verbSense & VT_POSSIBLE))
			{
				lpwstring description = u"detectTimeTransition";
				int sr = (int)(csr - syntacticRelationGroups.begin());
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, u"%06d:probable root time transition %s?", where, srToText(sr, description).c_str());
				csr->tft.nonPresentTimeTransition = true;
				return false;
			}

			if (timeInfo.absSeason >= 0 || timeInfo.absMonth >= 0 || timeInfo.absDayOfWeek >= 0 ||
				timeInfo.absDayOfMonth >= 0 || timeInfo.absHour >= 0 || timeInfo.absHoliday >= 0 ||
				(timeInfo.timeCapacity != cUnspecified && timeInfo.timeCapacity != cMoment && timeInfo.timeCapacity != cMinute && timeInfo.timeCapacity != cSecond &&
					// no daily / monthly  
					timeInfo.timeFrequency == -1))
			{
				int sr = (int)(csr - syntacticRelationGroups.begin());
				lpwstring description = u"detectTimeTransition";
				if (debugTrace.traceSpeakerResolution)
				{
					if (!csr->tft.timeTransition &&
						!(csr->relationType == stPREPTIME || csr->relationType == stPREPDATE || csr->relationType == stSUBJDAYOFMONTHTIME ||
							csr->relationType == stABSTIME || csr->relationType == stABSDATE || csr->relationType == stADVERBTIME))
						lplog(LOG_RESOLUTION, u"%06d:time transition %s", where, srToText(sr, description).c_str());
					sr = (int)(csr - syntacticRelationGroups.begin());
					if (!csr->tft.timeTransition && (m[csr->where].objectRole & (IN_PRIMARY_QUOTE_ROLE | IN_SECONDARY_QUOTE_ROLE)))
						lplog(LOG_RESOLUTION, u"%06d:time transition [IN QUOTE] %s", csr->where, srToText(sr, description).c_str());
				}
				if (csr->tft.presentHappening)
					csr->tft.timeTransition = true;
				else
					csr->tft.nonPresentTimeTransition = true;
			}
		}
	return csr->tft.timeTransition;
}

// Paint timeColor (from timeFlags, else T_UNIT) and flagAlreadyTimeAnalyzed
// on [begin, begin+len) and the relPrep, so identifyDateTime will skip them.
void cSource::markTime(int where, int begin, int len)
{
	LFS
		int color = m[where].getMainEntry()->second.timeFlags;
	if (!color && m[where].relPrep >= 0)
		color = m[m[where].relPrep].word->second.timeFlags;
	if (!color && len > 1)
		color = m[begin + len - 1].getMainEntry()->second.timeFlags;
	if (!color) color = T_UNIT;
	if (m[where].relPrep >= 0)
	{
		if (color)
			m[m[where].relPrep].timeColor = color;
		m[m[where].relPrep].flags |= cWordMatch::flagAlreadyTimeAnalyzed;
	}
	for (vector <cWordMatch>::iterator im = m.begin() + begin, imEnd = m.begin() + begin + len; im != imEnd; im++, begin++) // so begin can match a location!
	{
		if (color)
			im->timeColor = color;
		im->flags |= cWordMatch::flagAlreadyTimeAnalyzed;
	}
}

// If _TIME / _DATE / __INTRO_N / _ADVERB matches at beginObjectPosition,
// collect timeTagSet and evaluateDateTime. True on the first successful set.
bool cSource::evaluateTimePattern(int beginObjectPosition, int& maxLen, cTimeInfo& t, cTimeInfo& rt, bool& rtSet)
{
	LFS
		int element;
	if ((element = m[beginObjectPosition].pma.queryPattern(u"_TIME", maxLen)) != -1 ||
		(element = m[beginObjectPosition].pma.queryPattern(u"_DATE", maxLen)) != -1 ||
		(element = m[beginObjectPosition].pma.queryPattern(u"__INTRO_N", maxLen)) != -1 ||
		(element = m[beginObjectPosition].pma.queryPattern(u"_ADVERB", maxLen)) != -1)
	{
		vector < vector <cTagLocation> > tagSets;
		if (startCollectTags(false, timeTagSet, beginObjectPosition, m[beginObjectPosition].pma[element & ~cMatchElement::patternFlag].pemaByPatternEnd, tagSets, true, true, u"time pattern") > 0)
			for (unsigned int J = 0; J < tagSets.size(); J++)
			{
				//printTagSet(LOG_TIME,u"LT",J,tagSets[J],beginObjectPosition,m[beginObjectPosition].pma[element&~patternFlag].pemaByPatternEnd);
				if (evaluateDateTime(tagSets[J], t, rt, rtSet))
					return true;
			}
	}
	return false;
}

// evaluateTimePattern + markTime; may rewrite a BE ?it was ?? SRG to
// stABSTIME / stABSDATE. True if timeInfo was pushed.
bool cSource::identifyTimePattern(const int where, vector <cSyntacticRelationGroup>::iterator csr, const int beginObjectPosition, int& maxLen)
{
	cTimeInfo t, rt;
	rt.tWhere = t.tWhere = where;
	t.timeRTAnchor = rt.timeRTAnchor = where;
	bool rtSet = false;
	if (evaluateTimePattern(beginObjectPosition, maxLen, t, rt, rtSet) && m[beginObjectPosition].timeColor == 0 && beginObjectPosition + maxLen > where)
	{
		rt.tWhere = t.tWhere = where;
		t.timeRTAnchor = rt.timeRTAnchor = where;
		if (m[where].relPrep >= 0)
		{
			t.timeRelationType = (eTimeWordFlags)m[m[where].relPrep].word->second.timeFlags;
			t.metaDescriptive = (m[m[where].relPrep].word->first == u"of");
		}
		if (csr->relationType == stBE && m[csr->where].word->first == u"it" && csr->whereObject >= 0 &&
			(csr->whereObject == where || m[csr->whereObject].beginObjectPosition == where))
		{
			if (m[beginObjectPosition].pma.queryPattern(u"_DATE") != -1)
				csr->relationType = stABSDATE;
			else //if (m[whereTime].pma.queryPattern(u"_TIME")!=-1)
				csr->relationType = stABSTIME;
		}
		csr->timeInfo.push_back(t);
		detectTimeTransition(where, csr, t);
		if (rtSet)
		{
			csr->timeInfo.push_back(rt);
			detectTimeTransition(where, csr, rt);
		}
		markTime(where, beginObjectPosition, maxLen);
		return true;
	}
	return false;
}

// ?, 1915? after a comma: _NUMBER of length 1 in 1000..2499 becomes cYear.
bool cSource::identifyYear(const int where, vector <cSyntacticRelationGroup>::iterator csr, const int beginObjectPosition)
{
	int element, maxLen;
	if ((element = m[beginObjectPosition].pma.queryPattern(u"_NUMBER", maxLen)) != -1 && maxLen == 1 && beginObjectPosition > 2 &&
		m[beginObjectPosition - 1].word->first == u",")
	{
		cTimeInfo t;
		t.tWhere = where;
		t.timeRTAnchor = where;
		int year = lp_wtoi(m[beginObjectPosition].word->first.c_str());
		if (year > 1000 && year < 2500)
		{
			t.timeRelationType = T_UNIT;
			t.timeCapacity = cYear;
			t.absYear = year;
			csr->timeInfo.push_back(t);
			detectTimeTransition(where, csr, t);
			markTime(where, beginObjectPosition, maxLen);
			return true;
		}
	}
	return false;
}

// Fill capacity / abs* from the word form at `where` (timeUnit, month,
// daysOfWeek, season, holiday, time, date). False if none apply.
bool cSource::identifyTimeType(const int where, cTimeInfo &t)
{
	lpwstring word = m[where].deriveMainEntry(where, 34, false, true, lastNounNotFound, lastVerbNotFound)->first;
	if (m[where].queryForm(u"timeUnit") >= 0 || m[where].queryForm(u"dayUnit") >= 0)
		t.timeCapacity = (eCapacity)whichCapacity(word);
	else if (m[where].queryForm(u"month") >= 0)
	{
		t.timeCapacity = cNamedMonth;
		t.absMonth = whichMonth(m[where].word->first);
	}
	else if (m[where].queryForm(u"daysOfWeek") >= 0)
	{
		t.timeCapacity = cNamedDay;
		t.absDayOfWeek = whichDayOfWeek(m[where].word->first);
	}
	else if (m[where].queryForm(u"season") >= 0 && (m[where].flags & cWordMatch::flagFirstLetterCapitalized))
	{
		t.timeCapacity = cNamedSeason;
		t.absSeason = whichSeason(m[where].word->first);
	}
	else if (m[where].queryForm(u"simultaneousUnit") >= 0)
		t.timeCapacity = cMoment;
	else if (m[where].queryForm(u"holiday") >= 0)
	{
		t.timeCapacity = cNamedHoliday;
		t.absHoliday = whichHoliday(m[where].word->first);
	}
	else if (m[where].queryForm(timeForm) >= 0)
	{
		cWord::processTime(m[where].word->first, t.absHour, t.absMinute);
	}
	else if (m[where].queryForm(dateForm) >= 0)
	{
		cWord::processDate(m[where].word->first, t.absYear, t.absMonth, t.absDayOfMonth);
	}
	else
		return false;
	return true;
}

// Adjectival month/day/season/holiday/unit/year at `where` as a T_MODIFIER
// (the 1994 crisis). Recurring adverbs only set timeFrequency.
bool cSource:: processModifierTime(const int where, cTimeInfo& t)
{
	if ((m[where].forms.isSet(NUMBER_FORM_NUM) ||
		m[where].word->second.query(u"month") >= 0 || m[where].word->second.query(u"daysOfWeek") >= 0 || m[where].word->second.query(u"season") >= 0 ||
		m[where].getMainEntry()->second.query(u"timeUnit") >= 0 || m[where].getMainEntry()->second.query(u"dayUnit") >= 0) &&
		!m[where].forms.isSet(numeralOrdinalForm) && // not 'second'
		(m[where].pma.queryPatternWithLen(u"__ADJECTIVE", 1) != -1 || m[where].pma.queryPatternWithLen(u"__NADJECTIVE", 1) != -1))
	{
		if (m[where].word->second.query(u"month") >= 0)
		{
			t.timeCapacity = cNamedMonth;
			t.absMonth = whichMonth(m[where].word->first);
			t.timeRelationType = T_MODIFIER;
			return true;
		}
		else if (m[where].word->second.query(u"daysOfWeek") >= 0)
		{
			t.timeCapacity = cNamedDay;
			t.absDayOfWeek = whichDayOfWeek(m[where].word->first);
			t.timeRelationType = T_MODIFIER;
			return true;
		}
		else if (m[where].word->second.query(u"season") >= 0 && (m[where].flags & cWordMatch::flagFirstLetterCapitalized))
		{
			t.timeCapacity = cNamedSeason;
			t.absSeason = whichSeason(m[where].word->first);
			t.timeRelationType = T_MODIFIER;
			return true;
		}
		else if (m[where].word->second.query(u"holiday") >= 0)
		{
			t.timeCapacity = cNamedHoliday;
			t.absHoliday = whichHoliday(m[where].word->first);
			t.timeRelationType = T_MODIFIER;
			return true;
		}
		else if (m[where].getMainEntry()->second.query(u"timeUnit") >= 0 || m[where].getMainEntry()->second.query(u"dayUnit") >= 0)
		{
			if (where > 0 && (m[where - 1].forms.isSet(numeralCardinalForm) || m[where - 1].forms.isSet(numeralOrdinalForm)) && t.timeModifier < 0)
				t.timeModifier = where - 1;
			t.timeCapacity = (eCapacity)whichCapacity(m[where].getMainEntry()->first);
			t.timeRelationType = T_MODIFIER;
			return true;
		}
		else
		{
			int year = lp_wtoi(m[where].word->first.c_str());
			if (year > 1200 && year < 2100)
			{
				t.timeCapacity = cYear;
				t.absYear = year;
				t.timeRelationType = T_MODIFIER;
				return true;
			}
		}
	}
	else if (m[where].word->second.timeFlags & T_RECURRING) // daily
		t.timeCapacity = (eCapacity)(t.timeFrequency = whichRecurrence(m[where].word->first));
	return false;
}

// Bare Number / cardinal / ordinal as year (1200?2100), hour (1?12), or
// day-of-month (ordinal 1?31 as a prep object). inMultiObject==2 copies the
// previous timeInfo via copyTimeInfoNum when csr->timeInfo is non-empty
// (haveTi); otherwise it falls back to the normal single-value heuristics.
void cSource::interpretNumberAsDateTime(const int where, vector <cSyntacticRelationGroup>::iterator csr, cTimeInfo& t, const int beginObjectPosition, const int inMultiObject)
{
	// must not be used as an adjective, and must be an object of a preposition
	if ((m[where].forms.isSet(numeralCardinalForm) || m[where].forms.isSet(numeralOrdinalForm) || m[where].forms.isSet(NUMBER_FORM_NUM)) &&
		m[beginObjectPosition].pma.queryPatternWithLen(u"__NOUN", where - beginObjectPosition + 1) != -1 &&
		m[beginObjectPosition].pma.queryPatternDiff(u"__NOUN", u"Q") == -1)
	{
		vector <cTimeInfo>::iterator ti;
		// only form the iterator (and only treat inMultiObject==2 as "copy the
		// previous time") when csr->timeInfo actually has a previous entry;
		// csr->timeInfo.size()-1 on an empty vector would underflow (size_t).
		bool haveTi = inMultiObject == 2 && !csr->timeInfo.empty();
		if (haveTi)
			ti = csr->timeInfo.begin() + csr->timeInfo.size() - 1;
		if (m[where].forms.isSet(NUMBER_FORM_NUM))
		{
			int num = lp_wtoi(m[where].word->first.c_str());
			if (haveTi)
				copyTimeInfoNum(ti, t, num); // only copy previous time
			else if (num > 1200 && num < 2100)
			{
				t.timeCapacity = cYear;
				t.absYear = num;
			}
			else if (num >= 1 && num <= 12)
			{
				t.timeCapacity = cHour;
				t.absHour = num;
			}
		}
		if (m[where].forms.isSet(numeralOrdinalForm))
		{
			int num = mapNumeralOrdinal(m[where].word->first.c_str());
			if (haveTi)
				copyTimeInfoNum(ti, t, num); // only copy previous time
			// don't make 'the first' be a time if it is not an object of a preposition
			else if (num >= 1 && num <= 31 && m[where].relPrep >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1)
			{
				t.timeCapacity = cDay;
				t.absDayOfMonth = num;
			}
			else if (inMultiObject == 1)
			{
				t.timeCapacity = cUnspecified;
				t.timeModifier = where;
			}
		}
		if (m[where].forms.isSet(numeralCardinalForm) &&
			// events held one up.
			m[where].relPrep >= 0 &&
			m[where + 1].word->first != u"of" &&
			// we got to one that seemed...
			m[where + 1].word->first != u"that" &&
			// a hiding place for one
			(m[where].relPrep < 0 || (m[m[where].relPrep].word->second.timeFlags != T_THROUGHOUT)) &&
			// one who he could trust
			(m[where].getObject() < 0 || (objects[m[where].getObject()].whereRelativeClause < 0 &&
				// he wanted a new one
				(m[where].endObjectPosition - m[where].beginObjectPosition) == 1)) &&
			// from one to the other
			(m[where].relPrep < 0 || m[m[where].relPrep].relPrep < 0 ||
				m[m[where].relPrep].word->first != u"from" || !(m[m[m[where].relPrep].relPrep].word->second.timeFlags & T_BEFORE) ||
				m[m[m[where].relPrep].relPrep].getRelObject() < 0 ||
				m[m[m[m[where].relPrep].relPrep].getRelObject()].forms.isSet(numeralCardinalForm)))
		{
			int num = mapNumeralCardinal(m[where].word->first.c_str());
			if (haveTi)
				copyTimeInfoNum(ti, t, num); // only copy previous time
			else if (num >= 1 && num <= 12)
			{
				t.timeCapacity = cHour;
				t.absHour = num;
				if (where + 2 < (int)m.size() && m[where + 1].queryWinnerForm(dashForm) != -1 && m[where + 2].forms.isSet(numeralCardinalForm))
				{
					num = mapNumeralCardinal(m[where + 2].word->first.c_str());
					if (num >= 1 && num < 60)
						t.absMinute = num;
				}
			}
		}
	}
}

// Adjective / ?half an hour? / ?ago? / leading _NAME become timeModifier
// or override timeRelationType (T_BEFORE/T_AFTER/T_PRESENT).
void cSource::setTimeModifier(const int where, cTimeInfo& t, const int beginObjectPosition)
{
	int len;
	// fall semester / this weekend (encoded as_ADVERB[T]) / 4th quarter / it was only yesterday morning (yesterday is incorrectly flagged as adverb)
	if (where > 0 && t.timeRelationType != T_MODIFIER &&
		(m[where - 1].pma.queryPattern(u"_ADJECTIVE") != -1 || (m[where - 1].queryWinnerForm(adverbForm) != -1 && m[where - 1].queryForm(adjectiveForm) != -1) ||
			((m[where - 1].queryWinnerForm(u"noun") >= 0 && m[where - 1].queryForm(u"season") >= 0) || m[where - 1].queryForm(demonstrativeDeterminerForm) >= 0)))
		t.timeModifier = where - 1;
	// half an hour
	else if (where > 1 && m[where - 1].queryWinnerForm(determinerForm) >= 0 && m[where - 2].queryWinnerForm(u"predeterminer") >= 0)
		t.timeModifier = where - 2;
	else if (where + 1 < (int)m.size() && m[where + 1].queryWinnerForm(adverbForm) >= 0 &&
		(m[where + 1].word->second.timeFlags & (T_BEFORE | T_AFTER | T_PRESENT)) &&  // ago
		!(t.timeRelationType & (T_BEFORE | T_AFTER | T_PRESENT))) // Since then - since should take precedence (AFTER)
		t.timeRelationType = (eTimeWordFlags)(m[where + 1].word->second.timeFlags & (T_BEFORE | T_AFTER | T_PRESENT));
	else if (m[beginObjectPosition].pma.queryPattern(u"_NAME", len) != -1 && beginObjectPosition + len < m[where].endObjectPosition)
		t.timeModifier = beginObjectPosition;
	if (t.timeRelationType != T_MODIFIER && t.timeModifier >= 0 && where > 1 && where - 2 >= beginObjectPosition && m[where - 2].pma.queryPattern(u"_ADJECTIVE") != -1)
		t.timeModifier2 = where - 2;
	// the time was from the where position, and NAME objects have the principalWherePosition at the beginning (Saturday Night Live)
	if (t.timeRelationType != T_MODIFIER && where == beginObjectPosition && where < m[where].endObjectPosition && m[where].getObject() >= 0 &&
		(objects[m[where].getObject()].objectClass == NAME_OBJECT_CLASS || objects[m[where].getObject()].objectClass == NON_GENDERED_NAME_OBJECT_CLASS))
	{
		t.timeModifier = where;
		t.timeRelationType = T_MODIFIER;
	}
}

// True if this hit should be discarded: empty + no relation (unless
// inMultiObject==1), ?the Times?, or ?five times better?.
bool cSource::cancelTimeDateIdentification(const int where, cTimeInfo& t, const int inMultiObject)
{
	auto tt = t.timeRelationType;
	if (!tt && (t.timeCapacity == cUnspecified || t.empty()) && inMultiObject != 1)
	{
		return true;
	}
	lpwstring word = m[where].deriveMainEntry(where, 34, false, true, lastNounNotFound, lastVerbNotFound)->first;
	if (t.timeRelationType == T_RECURRING || word == u"time")
	{
		t.timeFrequency = whichRecurrence(m[where].word->first);
		if (m[where].word->first == u"times" && (m[where].flags & cWordMatch::flagFirstLetterCapitalized))
			return true; // the Times (of London)
		// five times better!
		if (where + 1 < (int)m.size() && m[where].word->first == u"times" && m[where - 1].queryWinnerForm(numeralCardinalForm) >= 0 &&
			m[where + 1].queryWinnerForm(adjectiveForm) >= 0 && (m[where + 1].word->second.inflectionFlags & ADJECTIVE_COMPARATIVE) != 0)
			return true;
	}
	return false;
}

// Main per-token time recognizer: skip already-analyzed tokens, try
// resolveTimeRange / identifyTimePattern / identifyYear, else build a
// cTimeInfo from identifyTimeType + modifiers + the word?s timeFlags.
// inMultiObject 0=normal, 1=first of a range, 2=second of a range.
bool cSource::identifyDateTime(int where, vector <cSyntacticRelationGroup>::iterator csr, int& maxLen, int inMultiObject)
{
	LFS
		if (where < 0 || (m[where].flags & cWordMatch::flagAlreadyTimeAnalyzed)) return false;
	int beginObjectPosition = m[where].beginObjectPosition, element;
	if (beginObjectPosition < 0) beginObjectPosition = where;
	if (inMultiObject == 0 && (element = m[beginObjectPosition].pma.queryTag(MNOUN_TAG)) >= 0 && resolveTimeRange(beginObjectPosition, element, csr))
		return true;
	maxLen = -1;
	if (identifyTimePattern(where, csr, beginObjectPosition, maxLen))
		return true;
	if (identifyYear(where, csr, beginObjectPosition))
		return true;
	cTimeInfo t, rt;
	rt.tWhere = t.tWhere = where;
	t.timeRTAnchor = rt.timeRTAnchor = where;
	if (m[where].relPrep >= 0)
	{
		if (m[m[where].relPrep].getRelObject() == where)
		{
			t.timeRelationType = (eTimeWordFlags)m[m[where].relPrep].word->second.timeFlags;
			t.metaDescriptive = (m[m[where].relPrep].word->first == u"of");
		}
		else if (m[m[where].relPrep].word->first == u"of" && (m[where].word->first == u"second" || m[where].getMainEntry()->first == u"shake"))
			return false;
	}
	if (!identifyTimeType(where,t))
	{ // the 1994 crisis
		t.timeCapacity = cUnspecified;
		for (int I = m[where].beginObjectPosition; I >= 0 && I < where; I++)
			if (processModifierTime(I, t))
				break;
		interpretNumberAsDateTime(where, csr, t, beginObjectPosition, inMultiObject);
		if ((m[where].relPrep < 0 || t.timeRelationType == T_UNSPECIFIED || !t.timeRelationType) && t.timeRelationType != T_MODIFIER &&
			//coming of the light / following of the Druids
			(m[where].queryWinnerForm(verbForm) == -1 || m[where + 1].word->first != u"of"))
			t.timeRelationType = (eTimeWordFlags)(m[where].word->second.timeFlags);
		auto tt = t.timeRelationType;
		// cancel if this word is supposed to be a preposition (its timeFlag only applies to the preposition form)
		if ((tt == T_THROUGHOUT || tt == T_AT || tt == T_ON || tt == T_MIDWAY || tt == T_INTERVAL ||
			tt == (T_BEFORE | T_CARDTIME) || tt == (T_AT | T_CARDTIME) || tt == (T_AFTER | T_CARDTIME)) &&
			(m[where].queryWinnerForm(prepositionForm) == -1 || t.empty()))
			tt = t.timeRelationType = T_UNSPECIFIED;
		if (cancelTimeDateIdentification(where, t, inMultiObject))
			return false;
	}
	setTimeModifier(where, t, beginObjectPosition);
	maxLen = 1;
	if (m[where].endObjectPosition >= 0) maxLen = m[where].endObjectPosition - beginObjectPosition;
	if (maxLen == 1 && m[where].queryWinnerForm(conjunctionForm) != -1 && m[where].word->second.timeFlags && m[where + 1].pma.queryPattern(u"__S1") != -1)
	{
		t.timeRelationType = T_META_RELATION;
		t.absMetaRelation = where + 1;
	}
	// 7, 10 and 'time'
	if (t.empty() && m[where].relPrep >= 0 && (t.timeCapacity == cUnspecified) && !((eTimeWordFlags)(m[where].word->second.timeFlags)))
		return false;
	if (t.empty() && m[where].word->first == u"past" && m[where - 1].queryWinnerForm(numeralCardinalForm) != -1)
	{
		int num = mapNumeralCardinal(m[where - 1].word->first.c_str());
		if (num >= 1 && num <= 59)
		{
			t.absMinute = num;
			t.timeCapacity = cMinute;
			t.timeRelationType = T_BEFORE;
		}
	}
	csr->timeInfo.push_back(t);
	if (((t.timeRelationType || t.timeCapacity != cUnspecified) && !t.empty() && t.timeRelationType != T_MODIFIER) ||
		(t.timeRelationType != T_UNSPECIFIED && t.timeRelationType != T_MODIFIER &&
			t.timeCapacity != cUnspecified && t.timeCapacity != cMoment && t.timeCapacity != cMinute && t.timeCapacity != cSecond))
		detectTimeTransition(where, csr, t);
	if (t.timeRelationType == T_MODIFIER)
		maxLen--;
	if (!inMultiObject || inMultiObject == 2)
		markTime(where, beginObjectPosition, maxLen);
	return true;
}

// Index-aligned with eCapacity; keep the two in step.  "morrow" is a synonym of
// "tomorrow" and is mapped by whichCapacity() rather than given its own slot.
const lpchar_t* twsCapacity[] = { u"millenium",u"century",u"decade",u"year",u"semester",u"season",u"quarter",u"month",u"week",u"day",
	 u"hour",u"minute",u"second",u"moment",
	 u"morning",u"noon",u"afternoon",u"evening",u"dusk",u"night",u"midnight",u"dawn",
	 u"tonight",u"today",u"tomorrow",u"yesterday",
	 u"NamedMonth",u"NamedDay",u"NamedSeason",u"NamedHoliday",
	 u"unspecified",NULL };

// Index of `w` in twsCapacity, or -1.
int whichCapacity(lpwstring w)
{
	LFS
	if (w == u"morrow") return cTomorrow;
	for (int I = 0; twsCapacity[I]; I++)
		if (twsCapacity[I] == w)
			return I;
	return -1;
}



// twsCapacity[capacityFlags], or u"illegal" if out of range / -1.
lpwstring capacityString(int capacityFlags)
{
	LFS
		// the comparison was unsigned, so -1 wrapped and indexed out of range
		if (capacityFlags >= 0 && capacityFlags < (int)(sizeof(twsCapacity) / sizeof(lpchar_t*)) && twsCapacity[capacityFlags] != NULL)
			return twsCapacity[capacityFlags];
	return u"illegal";
}

/*
	State Verbs
State verbs are a small group of verbs in English which don�t usually have continuous
forms, but use only simple verb forms. They are sometimes called �stative� verbs or �non-
progressive verbs�. They describe rather state than an action. States which are either
continuous or permanent �- without using a continuous tense state. Often stative verbs are
about liking or disliking something, or about a mental state, not about an action.
State verbs are different from active verbs(also called dynamic verbs), which describe
deliberate physical actions, e.g.run,eat,put, etc.  Some verbs can be both state and action verbs
depending on the context in which they�re being used (*).

state verbs cannot take up any time, and cannot belong to speech time.

www.eclecticenglish.com/grammar/PresentContinuous1H.html
www.ecenglish.com/learnenglish/lessons/what-are-state-verbs
http://www.englishtenseswithcartoons.com/state_verbs
http://www.perfect-english-grammar.com/support-files/stative-verbs-list.pdf
http://www.scribd.com/doc/24725250/Comprehensive-List-of-State-Verbs-in-English

categories:
a) general thought processes
b) negotiations with other people
c) likes and dislikes: emotional states
d) involuntary continuous usage (senses - smell, hear, see)
e) general states of being
f) possession
g) contents
h) measurement

************** list of state verbs below ***********************
b) accept
b) agree She didn�t agree with us. She wasn�t agreeing with us.
appear It appears to be raining. It is appearing to be raining.
e) be* is usually a stative verb, but when it is used in the continuous it means �behaving� or �acting�
		you are stupid = it�s part of your personality
		you are being stupid = only now, not usually
		I am the king of Kongo.
		We say: �Sue is nearly forty years old.� not �Sue is being nearly forty years old.�
a) believe I don�t believe the news. I am not believing the news.
f) belong This book belonged to my grandfather. This book was belonging to my grandfather.
concern This concerns you. This is concerning you.
g) consist Bread consists of flour, water and yeast. Bread is consisting of flour, water and yeast.
g) contain This box contains a cake. This box is containing a cake.
depend It depends on the weather. It�s depending on the weather.
e) deserve He deserves to pass the exam. He is deserving to pass the exam.
b) disagree I disagree with you. I am disagreeing with you.
c) dislike I have disliked mushrooms for years.  I have been disliking mushrooms for years.
b) doubt I doubt what you are saying. I am doubting what you are saying.
c) fancy
d) feel * (=have an opinion) I don�t feel that this is a good idea. I am not feeling that this is a good idea.
e) fit (clothes) * This shirt fits me well. This shirt is fitting me well.
a) forget *
c) hate Julie�s always hated dogs. Julie�s always been hating dogs.
f) have *
		have (stative) = own  I have a car
			'I have a car.� � state verb showing possession
			�I have two garages.�(general state of ownership) not:�I�m having two garages.�
		have (dynamic) = part of an expression  I�m having a party / a picnic / a bath / a good time / a break
			�We�re having dinner at Emily�s house.� (deliberate action )
			'I am having a bath.� � action verb which, in this case, means �taking�.
d) hear Do you hear music? Are you hearing music? [wrong]
a) imagine I imagine you must be tired. I am imagining you must be tired.
b) impress He impressed me with his story. He was impressing me with his story.
g) include This cookbook includes a recipe for bread. This cookbook is including a recipe for bread.
e) involve The job involves a lot of travelling. The job is involving a lot of travelling.
a) judge *
e) keep (continue) *
a) know I�ve known Julie for ten years. I�ve been knowing Julie for ten years.
e) lie (position) *
c) like I like reading detective stories. I am liking reading detective stories.
e) last (duration) *
c) loathe
c) love I love chocolate. I�m loving chocolate.*
e) matter It doesn�t matter. It isn�t mattering.
b) mean �Enormous� means �very big�. �Enormous� is meaning �very big�.
h) measure (=be long) This window measures 150cm. This window is measuring 150cm.
b) mind She doesn�t mind the noise. She isn�t minding the noise.
b) need At three o�clock yesterday I needed a taxi. At three o�clock yesterday I was needing a taxi.
b) notice
f) owe I owe you �20. I am owing you �20.
f) own She owns two cars. She is owning two cars.
f) possess
c) prefer I prefer chocolate ice cream. I am preferring chocolate ice cream.
b) promise I promise to help you tomorrow. I am promising to help you tomorrow.
a) realise I didn�t realise the problem. I wasn�t realising the problem.
a) recognise I didn�t recognise my old friend. I wasn�t recognising my old friend.
b) refuse
a) remember He didn�t remember my name. He wasn�t remembering my name.
d) see *
		see (stative) = see with your eyes / understand  I see what you mean  I see her
		We say: �I saw a bird sitting on a branch.� not �I was seeing a bird sitting on a branch.�
		see (dynamic) = meet / have a relationship with  I�ve been seeing my boyfriend for three years  I�m seeing Robert tomorrow
e) seem The weather seems to be improving.  The weather is seeming to be improving.
d) sense *
d) smell * I can smell something burning.
e) sound Your idea sounds great. Your idea is sounding great.
b) suppose I suppose John will be late. I�m supposing John will be late.
surprise The noise surprised me. The noise was surprising me.
b) suspect
d) taste (also: smell, feel, look) *
		(stative) = has a certain taste   This soup tastes great
		(dynamic) = the action of tasting  The chef is tasting the soup
a) think *
		(stative) = have an opinion  I think that coffee is great
			I think you are cool.�� state verb meaning �in my opinion�.
		(dynamic) = consider, have in my head  what are you thinking about? I�m thinking about my next holiday
b) trust *
a) understand I don�t understand this question. I�m not understanding this question.
a) want I want to go to the cinema tonight. I am wanting to go to the cinema tonight.
h) weigh (=have weight) This cake weighs 450g. This cake is weighing 450g.
a) wish I wish I had studied more. I am wishing I had studied more.

*/

const lpchar_t* stateVerbs[] =
{ u"accept",u"agree",u"believe",u"belong",u"consist",u"contain",u"deserve",u"disagree",u"dislike",u"doubt",u"fancy",u"hate",
	u"imagine",u"impress",u"include",u"involve",u"know",u"like",u"loathe",u"love",u"matter",u"mean",u"measure",u"mind",u"need",u"notice",
	u"owe",u"own",u"possess",u"prefer",u"refuse",u"sound",u"suppose",u"suspect",u"weigh",NULL
};

const lpchar_t* possibleStateVerbs[] =
{ u"am", u"feel", u"fit", u"forget", u"have", u"judge",u"keep",u"lie",u"last",u"promise",u"realize",u"recognize",u"remember",u"see",u"seem",
	u"sense",u"smell",u"taste",u"think",u"trust",u"understand",u"want",u"wish",NULL
};

tPrepEquivalent prepEquivalents[] =
{
{u"abaft",  u"behind"},       {u"acrost",  u"across"},       {u"adown",  u"down"},         {u"aff",    u"off"},
{u"agin",   u"against"},      {u"again",   u"against"},      {u"air",    u"before"},       {u"alang",  u"along"},
{u"amang",  u"among"},        {u"anear",   u"near"},         {u"anent",  u"about"},        {u"anigh",  u"near"},
{u"ben",    u"within"},       {u"beyant",  u"beyond"},       {u"bit",    u"but"},          {u"bout",   u"but"},
{u"cep",    u"except"},       {u"circiter", u"about"},       {u"cross",  u"across"},       {u"forby",  u"near"},
{u"forbye", u"near"},         {u"fore",    u"before"},       {u"forth",  u"from"},         {u"frae",   u"from"},
{u"neath",  u"beneath"},      {u"malgre",  u"despite"},      {u"outside",u"by"},           {u"sith",   u"since"}, // 'outside' may also be 'except'
{u"thro",   u"through"},      {u"withal",  u"with"},         {u"aloft",  u"above"},        {u"amongst", u"among"},
{u"amidst", u"amid"},         {u"anti",    u"against"},      {u"bar",    u"except"},       {u"despite", u"in spite of"},
{u"fro",    u"from"},         {u"nigh",    u"near"},         {u"o'er",   u"over"},         {u"aside",  u"past"},
{u"mid",    u"amid"},         {u"round",   u"throughout"},   {u"sans",   u"without"},      {u"thru",   u"through"},
{u"til",    u"till"},         {u"till",    u"until"},        {u"tween",  u"between"},      {u"twixt",  u"between"},
{u"betwixt",u"between"},      {u"toward",  u"towards"},      {u"ere",    u"before"},       {u"con",    u"against"},
{u"de",     u"of"},           {u"abroad",  u"throughout"},   {u"circa",  u"around"},       {u"contra", u"against"},
{u"re",     u"regarding"},    {u"save",    u"except"},       {u"upside", u"against"},      {u"qua",    u"as"},
{u"along",  u"alongside"},    {u"underneath", u"beneath"},   {u"unto",   u"towards"},      {u"astraddle", u"astride"},
{NULL, NULL}
};

tPrepRelation prepRelations[] =
{
	// PRIMARILY SPATIAL - near but not in
	// Tommy sat down opposite her.
	{u"near",tprNEAR},{u"next",tprNEAR},{u"about",tprNEAR},{u"along",tprNEAR},{u"amid",tprNEAR},{u"among",tprNEAR},{u"around",tprNEAR},{u"by",tprNEAR},{u"opposite",tprNEAR}, // {u"with",tprNEAR},
	// x direction
	{u"abreast",tprX},{u"beside",tprX},{u"across",tprX},{u"aslant",tprX},
	// inside dim 2 or 3 CGEL
	// The face appeared in the window.
	// area: in the world, in the village, in Asia - She is IN Oxford
	// volume: in a box, in the bathroom, in the cathedral
	{u"inside",tprIN},{u"into",tprIN},{u"in",tprIN},{u"out",tprIN},{u"within",tprIN},{u"without",tprIN},
	// in the same location or time as
	// dim 0 CGEL at the bus stop - She is AT Oxford
	{u"at",tprAT},
	// y direction (on or under) - dim 1 or 2 CGEL
	// line:
	// The city is situated on the River Thames.
	// The city is on the coast.
	// surface:
	// A notice was pasted on the wall.
	// The frost made patterns on the window.
	{u"above",tprY},{u"below",tprY},{u"atop",tprY},{u"beneath",tprY},{u"up",tprY},{u"down",tprY},{u"over",tprY},{u"under",tprY},{u"onto",tprY},{u"upon",tprY},
	{u"astride",tprY},{u"aboard",tprY},{u"on",tprY},{u"off",tprY},
	// otherwise spatially related
	{u"between",tprSPAT},{u"athwart",tprSPAT},{u"beyond",tprSPAT},{u"behind",tprSPAT},{u"past",tprSPAT},
	{u"through",tprSPAT},{u"midway",tprSPAT},{u"throughout",tprSPAT},
	// z direction (or time)
	{u"before",tprZ},{u"after",tprZ},{u"since",tprZ},{u"until",tprZ},{u"while",tprZ},{u"meanwhile",tprZ},{u"during",tprZ},
	// MATH
	{u"plus",tprMATH},{u"times",tprMATH},{u"minus",tprMATH},{u"less",tprMATH},{u"mod",tprMATH},{u"modulo",tprMATH},
	// LOGIC
	{u"but",tprLOGIC},{u"like",tprLOGIC},{u"unlike",tprLOGIC},{u"only",tprLOGIC},{u"except",tprLOGIC},{u"than",tprLOGIC},{u"or",tprLOGIC},{u"then",tprLOGIC},
	// tprFROM
	{u"from",tprFROM},{u"ex",tprFROM}, // from or without a starting point or source
	{u"to",tprTO}, {u"towards",tprTO}, // towards but not including
	{u"of",tprOF}, //        significantly linked modifier
	{u"as",tprAS}, //        positively related to
	{u"for",tprFOR}, //       serve the needs of OR concerning
	{u"per",tprPER}, //
	{u"against",tprVS}, {u"versus",tprVS}, //   against or in contrast to
	{NULL,-1}
};

// OR `flag` into timeFlags for each Inflections entry (parseWord if missing).
void cWord::addTimeFlag(int flag, Inflections words[])
{
	LFS
		for (int I = 0; words[I].word[0]; I++)
		{
			tIWMM iWord = query(words[I].word);
			if (iWord == end())
				if (parseWord(NULL, words[I].word, iWord, false, false, -1, false) < 0)
					lplog(LOG_FATAL_ERROR, u"Error getting forms for time word %s", words[I].word);
			iWord->second.timeFlags |= flag;
			iWord->second.flags |= cSourceWordInfo::updateMainInfo;
		}
}

// Copy the noun form?s usage cost onto nounSubclass for each word (so
// ?january? as month costs the same as a plain noun).
void cWord::usageCostToNoun(Inflections words[], const lpchar_t* nounSubclass)
{
	LFS
		int nounSubclassForm = cForms::gFindForm(nounSubclass);
	for (int I = 0; words[I].word[0]; I++)
	{
		tIWMM iWord = query(words[I].word);
		if (iWord == end())
			continue;
		iWord->second.costEquivalentSubClass(nounSubclassForm, nounForm);
	}
}

// Force formClass to the word?s lowest usage cost (prefer that form).
void cWord::toLowestUsageCost(Inflections words[], const lpchar_t* formClass)
{
	LFS
		int form = cForms::gFindForm(formClass);
	for (int I = 0; words[I].word[0]; I++)
	{
		tIWMM iWord = query(words[I].word);
		if (iWord == wNULL) continue;
		iWord->second.toLowestCost(form);
	}
}

// Copy usagePatterns / usageCosts from parentForm onto subclassForm.
// False if either form is absent.
bool cSourceWordInfo::costEquivalentSubClass(int subclassForm, int parentForm)
{
	LFS
		int subclassFormNum = query(subclassForm);
	if (subclassFormNum < 0) return false;
	int parentformNum = query(parentForm);
	if (parentformNum < 0) return false;
	usagePatterns[subclassFormNum] = usagePatterns[parentformNum];
	usageCosts[subclassFormNum] = usageCosts[parentformNum];
	return true;
}

// addTimeFlag for a NULL-terminated const lpchar_t* list.
void cWord::addTimeFlag(int flag, const lpchar_t* words[])
{
	LFS
		for (int I = 0; words[I] != NULL; I++)
		{
			if (words[I][0])
			{
				tIWMM iWord = query(words[I]);
				if (iWord == end())
					if (parseWord(NULL, words[I], iWord, false, false, -1, false) < 0)
						lplog(LOG_FATAL_ERROR, u"Error getting forms for time word %s", words[I]);
				iWord->second.timeFlags |= flag;
			}
		}
}

// usageCostToNoun for a NULL-terminated const lpchar_t* list.
void cWord::usageCostToNoun(const lpchar_t* words[], const lpchar_t* nounSubclass)
{
	LFS
		int nounSubclassForm = cForms::gFindForm(nounSubclass);
	for (int I = 0; words[I]; I++)
	{
		tIWMM iWord = query(words[I]);
		if (iWord == end()) continue;
		iWord->second.costEquivalentSubClass(nounSubclassForm, nounForm);
	}
}

// Print VT_* verbSense into s (?past EXT NEG?). verbSense == -1 -> u"-1".
lpwstring senseString(lpwstring& s, int verbSense)
{
	LFS
		if (verbSense == -1) return s = u"-1";
	if ((verbSense & VT_TENSE_MASK) == VT_PRESENT) s = u"present ";
	else if ((verbSense & VT_TENSE_MASK) == VT_PRESENT_PERFECT) s = u"present perfect ";
	else if ((verbSense & VT_TENSE_MASK) == VT_PAST) s = u"past ";
	else if ((verbSense & VT_TENSE_MASK) == VT_PAST_PERFECT) s = u"past perfect ";
	else if ((verbSense & VT_TENSE_MASK) == VT_FUTURE) s = u"future ";
	else if ((verbSense & VT_TENSE_MASK) == VT_FUTURE_PERFECT) s = u"future perfect ";
	else s = u"Illegal time sense ";

	if (verbSense & VT_POSSIBLE) s += u"POS ";
	if (verbSense & VT_PASSIVE) s += u"PAS ";
	if (verbSense & VT_EXTENDED) s += u"EXT ";
	if (verbSense & VT_VERB_CLAUSE) s += u"VC ";
	if (verbSense & VT_NEGATION) s += u"NEG ";
	return s;
};

// Age every local-focus entity (except exceptWhere) across a time or space
// jump: clear physicallyPresent, drop from lastSubjects / nextNarrationSubjects,
// ageSpeaker(EOS_AGE). Skips embedded-story quotes and a duplicate transition
// in the same sentence. Sets transitionSinceEOS. fromWhere is a log tag.
bool cSource::ageTransition(int where, bool timeTransition, bool& transitionSinceEOS, int duplicateFromWhere, int exceptWhere, vector <int>& lastSubjects, const lpchar_t* fromWhere)
{
	LFS
		if (duplicateFromWhere >= 0 && transitionSinceEOS)
			return false;
	transitionSinceEOS = true;
	lpwstring tmpstr, sRole;
	bool inPrimaryQuote = (m[where].objectRole & IN_PRIMARY_QUOTE_ROLE) != 0;
	bool inSecondaryQuote = (m[where].objectRole & IN_SECONDARY_QUOTE_ROLE) != 0;
	if (inPrimaryQuote && lastOpeningPrimaryQuote >= 0 && (m[lastOpeningPrimaryQuote].flags & cWordMatch::flagEmbeddedStoryResolveSpeakers))
		return false;  // don't age out any speakers
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG | LOG_RESOLUTION, u"%06d:%02d %s aging - %s %s transition %s%s", where, section,
			(inPrimaryQuote) ? u"PQ" : (inSecondaryQuote) ? u"SQ" : u"NQ", fromWhere,
			(timeTransition) ? u"time" : u"space",
			whereString((exceptWhere >= 0) ? exceptWhere : where, tmpstr, false).c_str(), m[(exceptWhere >= 0) ? exceptWhere : where].roleString(sRole).c_str());
	int so = (m[where].objectMatches.size() > 0) ? m[where].objectMatches[0].object : m[where].getObject();
	cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || (inPrimaryQuote && !(m[where].objectRole & (HAIL_ROLE | IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE))), 
									  (so >= 0) ? (objects[so].neuter && !(objects[so].male || objects[so].female)) : false, 
									  objectToBeMatchedInQuote, quoteIndependentAge);
	for (vector <cLocalFocus>::iterator lsi = localObjects.begin(); lsi != localObjects.end(); )
		if (((!inPrimaryQuote && !inSecondaryQuote) || lsi->includeInSalience(objectToBeMatchedInQuote, quoteIndependentAge)) && (exceptWhere < 0 || !in(lsi->om.object, exceptWhere)))
		{
			if (lsi->occurredOutsidePrimaryQuote && lsi->physicallyPresent && inPrimaryQuote)
			{
				lsi++;
				continue;
			}
			bool physicallyEvaluatedLW = false, ppLW = (lsi->lastWhere >= 0 && physicallyPresentPosition(lsi->lastWhere, physicallyEvaluatedLW) && physicallyEvaluatedLW);
			if (ppLW && m[lsi->lastWhere].objectMatches.size() > 1)
				ppLW = false;
			bool physicallyEvaluatedPW = false, ppPW = (lsi->previousWhere >= 0 && physicallyPresentPosition(lsi->previousWhere, physicallyEvaluatedPW) && physicallyEvaluatedPW);
			if (ppPW && m[lsi->previousWhere].objectMatches.size() > 1)
				ppPW = false;
			if (lsi->physicallyPresent && ((lsi->whereBecamePhysicallyPresent > where) || (ppLW && lsi->lastWhere > where) || (ppPW && lsi->previousWhere > where))) // DEBUG TSG
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG | LOG_RESOLUTION, u"%06d:Skipped making %s not present, as became present afterward [%s][%s][lastWhere %d %s][previousWhere %d %s]", where, objectString(lsi->om, tmpstr, true).c_str(),
						(lsi->occurredInPrimaryQuote) ? u"PRIM" : u"", (lsi->occurredOutsidePrimaryQuote) ? u"OUTSIDE" : u"",
						lsi->lastWhere, (physicallyEvaluatedLW) ? u"PP" : u"PP",
						lsi->previousWhere, (physicallyEvaluatedPW) ? u"PP" : u"PP");
				lsi++;
				continue;
			}
			if (lsi->physicallyPresent && debugTrace.traceSpeakerResolution)
				lplog(LOG_SG | LOG_RESOLUTION, u"%06d:Made %s not present 4 [%s][%s][lastWhere %d %s][previousWhere %d %s]", where, objectString(lsi->om, tmpstr, true).c_str(),
					(lsi->occurredInPrimaryQuote) ? u"PRIM" : u"", (lsi->occurredOutsidePrimaryQuote) ? u"OUTSIDE" : u"",
					lsi->lastWhere, (physicallyEvaluatedLW) ? u"PP" : u"PP",
					lsi->previousWhere, (physicallyEvaluatedPW) ? u"PP" : u"PP");
			if (lsi->physicallyPresent)
			{
				lsi->physicallyPresent = false;
				lsi->lastExit = where;
			}
			if (!inPrimaryQuote)
			{
				// if tempSpeakerGroup begins before the transition, remove from tempSpeakerGroup
				// also remove from previousLastSpeakers, lastSubjects & previousLastSubjects, and nextNarrationSubjects
				if (tempSpeakerGroup.sgBegin > where)
					tempSpeakerGroup.removeSpeaker(lsi->om.object);
				for (vector <int>::iterator si = nextNarrationSubjects.begin(), siEnd = nextNarrationSubjects.end(); si != siEnd; si++)
					if (*si == lsi->om.object)
					{
						nextNarrationSubjects.erase(si);
						break;
					}
				for (vector <int>::iterator si = lastSubjects.begin(), siEnd = lastSubjects.end(); si != siEnd; si++)
					if (*si == lsi->om.object)
					{
						lastSubjects.erase(si);
						break;
					}
			}
			ageSpeaker(where, lsi, EOS_AGE); // can remove object from localObjects completely
		}
		else
			lsi++;
	return true;
}

// True if this time NP should not age speakers: second/minute/moment,
// ?this/all/each/any ??, ?the time?, ?five times better?, single ?day?.
bool cSource::rejectTimeWord(int where, int begin)
{
	LFS
		lpwstring moment = m[where].getMainEntry()->first;
	// second or minute doesn't work because the assumption of everyone being somewhere else is not necessarily true
	const lpchar_t* momentUnits[] = { u"second",u"minute",u"moment",u"flash",NULL }; // see simultaneous units
	for (int I = 0; momentUnits[I]; I++)
		if (moment == momentUnits[I])
			return true;
	// reject - this time / this morning - 'this' means it is already here; there is no transition
	// all the time it was a bluff!
	// each day saw them set out...
	// any time he chose
	lpwstring detTime = m[begin].word->first;
	const lpchar_t* anyTime[] = { u"this",u"all",u"each",u"any",NULL };
	for (int I = 0; anyTime[I]; I++)
		if (detTime == anyTime[I])
			return true;
	if (moment == u"time" && m[where - 1].queryWinnerForm(determinerForm) >= 0)
		return true;
	// five times better
	if (where + 1 < (int)m.size() && m[where].word->first == u"times" && m[where - 1].queryWinnerForm(numeralCardinalForm) >= 0 &&
		m[where + 1].queryWinnerForm(adjectiveForm) >= 0 && (m[where + 1].word->second.inflectionFlags & ADJECTIVE_COMPARATIVE) != 0)
		return true;
	if ((moment == u"day" || moment == u"time") && m[where].beginObjectPosition == m[where].endObjectPosition - 1)
		return true;
	if (m[where].beginObjectPosition == m[where].endObjectPosition && where > 2 && m[where - 1].word->first != u"-" && !(m[where].flags & cWordMatch::flagFirstLetterCapitalized))
		return true;
	return false;
}

// MNOUN of two time objects (?June and July?, ?from 5 to 6?): identifyDateTime
// each side; mark T_RANGE on ?and?. True if both sides parsed.
bool cSource::resolveTimeRange(int where, int pmaOffset, vector <cSyntacticRelationGroup>::iterator csr)
{
	LFS
		vector <int> objectPositions;
	vector < vector <cTagLocation> > mobjectTagSets;
	if (startCollectTags(true, mobjectTagSet, where, m[where].pma[pmaOffset].pemaByPatternEnd, mobjectTagSets, true, false, u"resolve time range") > 0)
		for (unsigned int J = 0; J < mobjectTagSets.size(); J++)
		{
			//if (t.traceSpeakerResolution)
			//	::printTagSet(LOG_TIME,u"MOBJECT",J,mobjectTagSets[J]);
			for (int oTag = findOneTag(mobjectTagSets[J], u"MOBJECT", -1); oTag >= 0; oTag = findOneTag(mobjectTagSets[J], u"MOBJECT", oTag))
				if (mobjectTagSets[J][oTag].PEMAOffset < 0)
					objectPositions.push_back((m[mobjectTagSets[J][oTag].sourcePosition].principalWherePosition >= 0) ? m[mobjectTagSets[J][oTag].sourcePosition].principalWherePosition : mobjectTagSets[J][oTag].sourcePosition);
			if (objectPositions.size() == 2)
			{
				int maxLen;
				if (!identifyDateTime(objectPositions[0], csr, maxLen, 1)) return false;
				if (identifyDateTime(objectPositions[1], csr, maxLen, 2))
				{
					if (m[objectPositions[0]].endObjectPosition >= 0 && m[m[objectPositions[0]].endObjectPosition].word->first == u"and")
						csr->timeInfo[csr->timeInfo.size() - 1].timeRelationType = T_RANGE;
					markTime(objectPositions[0], m[objectPositions[0]].beginObjectPosition, m[objectPositions[0]].endObjectPosition - m[objectPositions[0]].beginObjectPosition);
					return true;
				}
				else // erase previous time if empty()
				{
					vector <cTimeInfo>::iterator cti = csr->timeInfo.begin() + csr->timeInfo.size() - 1;
					if (!cti->timeRelationType && (cti->timeCapacity == cUnspecified || cti->empty()))
					{
						lpwstring tmpstr;
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, u"%06d: time relation rejected: %s", cti->tWhere, cti->toString(m, tmpstr).c_str());
						csr->timeInfo.erase(cti);
					}
				}
			}
		}
	return false;
}

// True at EOS, a section word, or a real (non-string) curly quote ? the
// backward scan for a previous SRG to steal timeInfo from must stop here.
bool cSource::stopSearch(int I)
{
	LFS
		return isEOS(I) || m[I].word == Words.sectionWord || ((m[I].word->first == u"�" || m[I].word->first == u"�") && !(m[I].flags & cWordMatch::flagQuotedString));
}

// Copy or split previousRelation.timeInfo onto csr. conjunctionPassed == -1
// copies everything and records duplicateTimeTransitionFromWhere; otherwise
// moves expressions after the conjunction / after csr->where.
// the previous relation has already taken everything in the sentence.
// if there is no conjunction between them, simply copy all time expressions.
// if there is a conjunction between them, then move all time expressions after the current space relation
//    from the previous time relation to the current one.
void cSource::distributeTimeRelations(vector <cSyntacticRelationGroup>::iterator csr, vector <cSyntacticRelationGroup>::iterator previousRelation, int conjunctionPassed)
{
	LFS
		if (conjunctionPassed == -1)
		{
			csr->timeInfo = previousRelation->timeInfo;
			csr->tft.timeTransition |= previousRelation->tft.timeTransition;
			csr->tft.duplicateTimeTransitionFromWhere = previousRelation->where;
			return;
		}
	bool gainedTransition = false;
	for (vector <cTimeInfo>::iterator I = previousRelation->timeInfo.begin(); I != previousRelation->timeInfo.end(); )
	{
		if ((conjunctionPassed < I->tWhere && m[I->tWhere].pma.queryPattern(u"_INTRO_S1") != -1) ||
			I->tWhere > csr->where)
		{
			csr->timeInfo.push_back(*I);
			bool saveTransition = csr->tft.timeTransition;
			if (I->timeRelationType != T_MODIFIER)
				detectTimeTransition(I->tWhere, csr, *I);
			if (!saveTransition && csr->tft.timeTransition)
				gainedTransition = true;
			previousRelation->timeInfo.erase(I);
		}
		else
			I++;
	}
	if (previousRelation->timeInfo.empty() && gainedTransition)
		previousRelation->tft.timeTransition = false;
}

// Attach cTimeInfo to csr (subject / verb PPs / object / intro adverb /
// previous SRG) and set timeProgression 0?2 from state-verb / tense.
// No-op if timeInfoSet. Marks timeInfoSet at the end of the scan.
void cSource::appendTime(vector <cSyntacticRelationGroup>::iterator csr)
{
	LFS
		if (csr->timeInfoSet) return;
	int maxLen = -1;
	if (csr->whereSubject >= 0 && m[csr->whereSubject].getObject() >= 0)
	{
		if (csr->relationType != stABSTIME && m[csr->whereSubject].relPrep < 0 && objects[m[csr->whereSubject].getObject()].getIsTimeObject())
		{
			lpwstring tmpstr;
			if (debugTrace.traceTime)
				lplog(LOG_TIME, u"%06d:Shifted timeRelation with subject %s to stABSTIME.", csr->where, whereString(csr->whereSubject, tmpstr, false).c_str());
			csr->relationType = stABSTIME;
			csr->tft.timeTransition = true;
		}
		if (objects[m[csr->whereSubject].getObject()].objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS ||
			objects[m[csr->whereSubject].getObject()].getIsTimeObject())
			identifyDateTime(csr->whereSubject, csr, maxLen, 0);
	}
	if (csr->relationType == stADVERBTIME)
	{
		identifyDateTime(csr->where, csr, maxLen, 0);
		return;
	}
	if (csr->whereVerb < 0) return;
	int wherePrep = m[csr->whereVerb].relPrep, prepLoop = 0, progression = -1;
	if (csr->relationType == stABSTIME)
		progression = 0;
	if (m[csr->whereVerb].word->second.flags & cSourceWordInfo::stateVerb)
		progression = 0;
	else if (m[csr->whereVerb].word->second.flags & cSourceWordInfo::possibleStateVerb)
		progression = 1;
	else
		progression = 2;
	if (csr->tft.presentHappening)
	{
		progression = (m[csr->whereVerb].verbSense & VT_EXTENDED) ? 0 : 2;
		if (progression == 2 &&
			(isVerbClass(csr->whereVerb, u"am") || csr->relationType == stCONTACT))
			progression = 0;
	}
	// 0 - advances a action based amount of time from the last statement / He brushed his teeth.   [ +15 minutes? ]
	// 1 - still keeping the current time flow but not advancing the action / He was fat / He lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms the ham.
	// 2 - establishes a new time not related to the previous time flow / At Christmas, they opened fifty presents - each.
	// 3 - establishes a new time relative to the current time flow / after two weeks, he was tired. / He ran for two hours.
	// 4 - an action that describes something happening up to and including the moment ; not advancing time / He was running past the stop sign. / He was driving his new car.
	// 5 - an action that describes something that happened in the past ; not advancing time / He had wandered down that road many times
	// 6 - an action that describes something that will happen in the future ; not advancing time / She will pay for this!
	// 7 - an action that may have happened in the past ; not advancing time / I may have misplaced my wallet
	// 8 - an action that may happen in the future ; not advancing time / I might run for office
	// 9 - an action that describes recurring action ; not advancing the current time flow / I always visit her / I drive the van every Monday / She plays soccer twice a week.
	csr->timeProgression = progression;
	bool directlyFollowingLinked = false;
	for (; wherePrep >= 0; wherePrep = m[wherePrep].relPrep)
	{
		if (prepLoop++ > 30)
		{
			lpwstring tmpstr;
			lplog(LOG_ERROR, u"%06d:Prep loop occurred (8) %s.", wherePrep, loopString(wherePrep, tmpstr));
			return;
		}
		// John ran in May.
		if (identifyDateTime(m[wherePrep].getRelObject(), csr, maxLen, 0))
		{
			if (directlyFollowingLinked && (m[wherePrep].word->first == u"to" || m[wherePrep].word->first == u"through"))
				csr->timeInfo[csr->timeInfo.size() - 1].timeRelationType = T_RANGE;
			directlyFollowingLinked = true;
		}
	}
	// He departs this weekend for Baghdad.
	maxLen = -1;
	if (csr->whereObject >= 0)
	{
		int beginObjectPosition = m[csr->whereObject].beginObjectPosition, endObjectPosition = m[csr->whereObject].endObjectPosition;
		if ((beginObjectPosition >= 0 && m[beginObjectPosition].pma.queryPattern(u"__CLOSING__S1", maxLen) == -1 || maxLen != (endObjectPosition - beginObjectPosition)))
		{
			if (identifyDateTime(csr->whereObject, csr, maxLen, 0) && csr->relationType == stBE && m[csr->where].word->first == u"it")
			{
				int whereTime = m[csr->whereObject].beginObjectPosition;
				if (whereTime >= 0)
				{
					if (m[whereTime].pma.queryPattern(u"_DATE") != -1)
						csr->relationType = stABSDATE;
					else if (m[whereTime].pma.queryPattern(u"_TIME") != -1)
						csr->relationType = stABSTIME;
				}
			}
		}
	}
	if (csr->relationType != stABSTIME)
	{
		// _INTRO_S1, __CLOSING__S1 OR _ADVERB
		unsigned int I = csr->where;
		if (csr->whereSubject < (signed)I && csr->whereSubject != -1)
		{
			I = csr->whereSubject;
			if (m[I].hasSyntacticRelationGroup) I++; // make sure a previous space relation with the same subject isn't skipped, for time relation movement
		}
		// go back to last EOS or last beginning quote.
		int conjunctionPassed = -1;
		for (I = (I > 0) ? I - 1 : I; !stopSearch(I) && I > 0; I--)
		{
			if (m[I].hasSyntacticRelationGroup)
			{
				vector <cSyntacticRelationGroup>::iterator previousRelation;
				if ((previousRelation = findSyntacticRelationGroup(I)) != syntacticRelationGroups.end() && csr != previousRelation)
				{
					distributeTimeRelations(csr, previousRelation, conjunctionPassed);
					break;
				}
			}
			if (m[I].queryWinnerForm(conjunctionForm) != -1 || m[I].queryWinnerForm(coordinatorForm) != -1)
				conjunctionPassed = I;
		}
		int maxEnd = -1, element;
		for (I++; I < m.size() && !stopSearch(I); I++)
		{
			// The company he had invested in went bankrupt within minutes [after] the stock market closed for the day.
			// We were still talking about work three hours [after] the meeting broke up.
			if ((element = m[I].queryWinnerForm(conjunctionForm)) != -1 && m[I].word->second.timeFlags && debugTrace.traceTime)
				lplog(LOG_TIME, u"%06d:conjunction!", I);
			if (m[I].flags & cWordMatch::flagAlreadyTimeAnalyzed)
				continue;
			if ((maxEnd == -1 || I >= (unsigned)maxEnd) && m[I].relPrep < 0)
			{
				if ((element = pema.queryTag(m[I].beginPEMAPosition, FLOAT_TIME_TAG)) >= 0)
					maxEnd = I + pema[element].getChildLen();
				else if ((element = m[I].pma.queryTag(FLOAT_TIME_TAG)) >= 0)
					maxEnd = I + m[I].pma[element].len;
			}
			if (maxEnd != -1 && I < (unsigned)maxEnd && identifyDateTime(I, csr, maxEnd, 0) && maxEnd>0)
			{
				//int pattern=pema[element].getParentPattern();
				//lplog(LOG_TIME,u"%d:%s[%s](%d,%d)",csr->where,
				//	patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),I,I+pema[element].end);
				I += maxEnd - 1;
			}
		}
	}
	csr->timeInfoSet = true;
}

// Subject-side time NP (?That evening Tommy sat??): emit stADVERBTIME /
// stPREPTIME / stSUBJDAYOFMONTHTIME and optionally ageTransition.
// will change source.m (invalidate all iterators through the use of newSR)
void cSource::detectTimeTransition(int where, vector <int>& lastSubjects)
{
	LFS
		int timeType = -1, whereTime = -1;
	bool timeTransition = false;
	int wt = where, wot = -1;
	if ((m[wt].objectRole & (SUBJECT_ROLE | OBJECT_ROLE)) != SUBJECT_ROLE)
		return;
	if (m[wt].principalWherePosition >= 0) wt = m[wt].principalWherePosition;
	if (m[wt].objectMatches.size()) wt = objects[m[wt].objectMatches[0].object].originalLocation;
	if ((m[wt].objectRole & (SUBJECT_ROLE | OBJECT_ROLE)) != SUBJECT_ROLE)
		return;
	if (((m[wt].getObject() >= 0 && (m[wt].word->second.timeFlags & T_UNIT)) ||
		((m[where].objectRole & IS_OBJECT_ROLE) && (wot = m[where].getRelObject()) >= 0 && m[wot].getObject() >= 0 && (m[wot].word->second.timeFlags & T_UNIT))))
	{
		if (wot >= 0) wt = wot;
		int findSubject = wt;
		if ((m[wt].objectRole & SUBJECT_ROLE) && m[wt].getRelVerb() < 0)
		{
			for (; findSubject < (signed)m.size() && (m[findSubject].objectRole & SUBJECT_ROLE) && m[findSubject].getRelVerb() < 0; findSubject++);
			if (!(m[findSubject].objectRole & SUBJECT_ROLE) || m[findSubject].getRelVerb() < 0)
				findSubject = wt;
		}
		int tense = -1;
		if ((m[findSubject].objectRole & SUBJECT_ROLE) && (m[findSubject].getRelVerb() < 0 || // about an hour had passed
			((tense = m[m[findSubject].getRelVerb()].verbSense & (VT_TENSE_MASK | VT_POSSIBLE)) != VT_PAST) && (tense != VT_PAST_PERFECT)))
			return;
		if (tense == VT_PAST_PERFECT && debugTrace.traceTime)
			lplog(LOG_RESOLUTION, u"%06d:VT_PAST_PERFECT time subject passed.", findSubject);
		if (rejectTimeWord(wt, m[wt].beginObjectPosition)) return;
		// Sunday was her afternoon out.
		// Night and day were the same in this prison room.
		// Three hours were more than enough for Mr. Brown.
		// The first time was in Italy.
		if ((m[wt].objectRole & (IS_OBJECT_ROLE | SUBJECT_ROLE)) == (IS_OBJECT_ROLE | SUBJECT_ROLE))
		{
			// Today was the 27th. - this is a meta-expression for time
			if (m[wt].getRelObject() >= 0 && m[m[wt].getRelObject()].queryWinnerForm(numeralOrdinalForm) >= 0)
			{
				timeType = stSUBJDAYOFMONTHTIME;
				objects[m[m[wt].getRelObject()].getObject()].setIsTimeObject(true);
			}
			else
			{
				if (m[wt].getObject() >= 0)
					objects[m[wt].getObject()].setIsTimeObject(true);
				m[wt].timeColor = T_UNIT;
				return;
			}
		}
		else
		{
			// this morning post
			// the 1994 crisis
			if (findSubject != wt && m[findSubject].beginObjectPosition <= wt)
			{
				// that evening Tommy sat on the bed.
				//int tmp=m[findSubject].getObject(),tmp2=objects[m[findSubject].getObject()].PISDefinite;
				if (findSubject != wt + 1 || m[findSubject].getObject() < 0 || objects[m[findSubject].getObject()].PISDefinite <= 0)
				{
					if (debugTrace.traceTime)
						lplog(u"%06d:enhanced time transition trueSubject=%d rejected (adjective).", where, findSubject);
					return;
				}
				else if (debugTrace.traceTime)
					lplog(u"%06d:enhanced time transition trueSubject=%d (kept).", where, findSubject);
			}
			timeType = stADVERBTIME;
			if (m[where].pma.queryPattern(u"_INTRO_S1") != -1)
			{
				if (debugTrace.traceTime)
					lplog(u"%06d:intro time transition rejected (will be picked up later).", where);
				return;
			}
			// the day/the evening etc
			if (wt > 2 && (m[wt - 1].word->first != u"-" || (m[wt - 2].word->first != u"lunch" && m[wt - 2].word->first != u"dinner")) && m[wt - 1].queryWinnerForm(determinerForm) < 0)
				timeTransition = true;
		}
		if (findSubject != wt && debugTrace.traceTime)
			lplog(u"%06d:enhanced time transition trueSubject=%d.", where, findSubject);
		where = wt;
		if (m[where].getObject() >= 0)
		{
			objects[m[where].getObject()].setIsTimeObject(true);
			lpwstring tmpstr;
			if (debugTrace.traceTime)
				lplog(LOG_RESOLUTION, u"%06d:Marking %s as timeObject", where, objectString(m[where].getObject(), tmpstr, false).c_str());
		}
	}
	// A neighbouring clock showed the time to be five minutes to twelve . 
	// A neighbouring clock showed 5 o'clock . 
	else if (m[wt].getRelVerb() >= 0 && isVerbClass(m[wt].getRelVerb(), u"indicate") &&
		(m[m[wt].getRelVerb()].verbSense & (VT_TENSE_MASK | VT_POSSIBLE)) == VT_PAST && m[wt].getRelObject() >= 0 && m[m[wt].getRelObject()].word->first == u"time")
	{
		int saveWhere = -1;
		if (m[m[wt].getRelVerb()].getRelVerb() >= 0 && m[m[m[wt].getRelVerb()].getRelVerb()].word->first == u"be" && m[m[m[wt].getRelVerb()].getRelVerb()].getRelObject() >= 0 &&
			m[m[saveWhere = m[m[m[wt].getRelVerb()].getRelVerb()].getRelObject()].beginObjectPosition].pma.queryPattern(u"_TIME") != -1)
			timeType = stABSTIME;
		else if (m[wt].getRelObject() >= 0 && m[m[saveWhere = m[wt].getRelObject()].beginObjectPosition].pma.queryPattern(u"_TIME") != -1)
			timeType = stABSTIME;
		if (timeType >= 0)
		{
			where = whereTime = saveWhere;
			objects[m[where].getObject()].setIsTimeObject(true);
		}
	}
	else if (m[where].pma.queryPattern(u"_TIME") != -1 && (m[where].objectRole & SUBJECT_PLEONASTIC_ROLE))
	{
		whereTime = where;
		timeType = stABSTIME;
		if (m[where].getObject() >= 0)
			objects[m[where].getObject()].setIsTimeObject(true);
	}
	// IT was May 7 , 1915
	else if (m[where].pma.queryPattern(u"_DATE") != -1 && (m[where].objectRole & SUBJECT_PLEONASTIC_ROLE))
	{
		whereTime = where;
		timeType = stABSDATE;
		if (m[where].getObject() >= 0)
			objects[m[where].getObject()].setIsTimeObject(true);
	}
	if (timeType >= 0)
	{
		bool transitionSinceEOS = false; // transitionSinceEOS only applies to transitions with timeInfo, not the relation itself (stABSTIME)
		if (timeTransition)
			ageTransition(where, true, transitionSinceEOS, -1, -1, lastSubjects, u"TSR 2");
		newSR(where, -1, -1, where, m[where].getRelVerb(), -1, m[where].getRelObject(), -1, -1, timeType, u"time", true);
		if (m[where].getObject() >= 0 && !m[where].timeColor)
			m[where].timeColor = T_UNIT;
		vector <cSyntacticRelationGroup>::iterator sr;
		if ((sr = findSyntacticRelationGroup(where)) != syntacticRelationGroups.end() && sr->where == where)
		{
			sr->tft.timeTransition |= timeTransition;
			int offset = (int)(sr - syntacticRelationGroups.begin());
			lpwstring description = u"detectTimeTransition";
			if (debugTrace.traceTime)
				lplog(LOG_RESOLUTION, u"%06d:time transition %s (%s)", where, srToText(offset, description).c_str(), (sr->tft.timeTransition) ? u"true" : u"false");
		}
	}
}

// Map verbSenseTagSet tags (vS/vB/vC/vD/past/future/conditional/?) onto a
// VT_* bitset (Quirk table in this header). Writes m[].verbSense over the
// verb span. isId is the ?id? (be) tag. Grows tagSetTimeArray via tmalloc.
int cSource::getVerbTense(vector <cTagLocation>& tagSet, int verbTagIndex, bool& isId)
{
	LFS
		if (!tagSetTimeArraySize)
			tagSetTimeArray = (char*)tmalloc(tagSetTimeArraySize = desiredTagSets[verbSenseTagSet].tags.size());
		else if (tagSetTimeArraySize < desiredTagSets[verbSenseTagSet].tags.size())
		{
			unsigned int previousSize = tagSetTimeArraySize;
			tagSetTimeArray = (char*)trealloc(20, tagSetTimeArray, previousSize, tagSetTimeArraySize = desiredTagSets[verbSenseTagSet].tags.size());
		}
	memset(tagSetTimeArray, 0, desiredTagSets[verbSenseTagSet].tags.size());
	if (verbTagIndex < 0)
		findTagSet(tagSet, verbSenseTagSet, tagSetTimeArray);
	else
		findTagSetConstrained(tagSet, verbSenseTagSet, tagSetTimeArray, tagSet[verbTagIndex]);
	int tense = 0;
	// 0    1       2     3      4      5        6      7           8    9     10  11   12   13    14    15     16    17    18     19     20     21      22   23     24    25    26    27
	// "no","never","not","past","imp","future","id","conditional","vS","vAC","vC","vD","vB","vAB","vBC","vABC","vCD","vBD","vABD","vACD","vBCD","vABCD","vE","vrBD","vrB","vrBC","vrD","vAD"
	enum constVerbTag {
		vt_no, vt_never, vt_not, vt_past, vt_imp, vt_future, vt_id, vt_conditional, vt_vS, vt_vAC, vt_vC, vt_vD, vt_vB, vt_vAB, vt_vBC, vt_vABC, vt_vCD, vt_vBD,
		vt_vABD, vt_vACD, vt_vBCD, vt_vABCD, vt_vE, vt_vrBD, vt_vrB, vt_vrBC, vt_vrD, vt_vAD
	};
	isId = tagSetTimeArray[vt_id] != 0;
	// SIMPLE
	if (tagSetTimeArray[vt_vS]) tense = VT_PRESENT;
	if (tagSetTimeArray[vt_past]) tense = VT_PAST;
	if (tagSetTimeArray[vt_vB]) tense = VT_PRESENT_PERFECT;
	if (tagSetTimeArray[vt_vrB]) tense = VT_PRESENT_PERFECT + VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vB] && tagSetTimeArray[vt_past]) tense = VT_PAST_PERFECT;
	if (tagSetTimeArray[vt_vS] && tagSetTimeArray[vt_future]) tense = VT_FUTURE;
	if (tagSetTimeArray[vt_vAB] && tagSetTimeArray[vt_future]) tense = VT_FUTURE_PERFECT;
	// EXTENDED
	if (tagSetTimeArray[vt_vC]) tense = VT_EXTENDED + VT_PRESENT;
	if (tagSetTimeArray[vt_vE]) tense = VT_EXTENDED + VT_PRESENT + VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vC] && tagSetTimeArray[vt_past]) tense = VT_EXTENDED + VT_PAST;
	if (tagSetTimeArray[vt_vBC]) tense = VT_EXTENDED + VT_PRESENT_PERFECT;
	if (tagSetTimeArray[vt_vrBC]) tense = VT_EXTENDED + VT_PRESENT_PERFECT + VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vBC] && tagSetTimeArray[vt_past]) tense = VT_EXTENDED + VT_PAST_PERFECT;
	if (tagSetTimeArray[vt_vAC] && tagSetTimeArray[vt_future]) tense = VT_EXTENDED + VT_FUTURE;
	if (tagSetTimeArray[vt_vABC] && tagSetTimeArray[vt_future]) tense = VT_EXTENDED + VT_FUTURE_PERFECT;
	// PASSIVE
	if (tagSetTimeArray[vt_vD]) tense = VT_PASSIVE + VT_PRESENT;
	if (tagSetTimeArray[vt_vrD]) tense = VT_PASSIVE + VT_PRESENT + VT_EXTENDED + VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vD] && tagSetTimeArray[vt_past]) tense = VT_PASSIVE + VT_PAST;
	if (tagSetTimeArray[vt_vBD]) tense = VT_PASSIVE + VT_PRESENT_PERFECT;
	if (tagSetTimeArray[vt_vrBD]) tense = VT_PASSIVE + VT_PRESENT_PERFECT + VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vAD] && tagSetTimeArray[vt_past]) tense = VT_PASSIVE + VT_PAST; // dared be examined?
	if (tagSetTimeArray[vt_vBD] && tagSetTimeArray[vt_past]) tense = VT_PASSIVE + VT_PAST_PERFECT;
	if (tagSetTimeArray[vt_vCD]) tense = VT_PASSIVE + VT_PRESENT + VT_EXTENDED;
	if (tagSetTimeArray[vt_vCD] && tagSetTimeArray[vt_past]) tense = VT_PASSIVE + VT_PAST + VT_EXTENDED;
	if (tagSetTimeArray[vt_vBCD]) tense = VT_PASSIVE + VT_PRESENT_PERFECT + VT_EXTENDED;
	if (tagSetTimeArray[vt_vBCD] && tagSetTimeArray[vt_past]) tense = VT_PASSIVE + VT_PAST_PERFECT + VT_EXTENDED;
	if (tagSetTimeArray[vt_vAD] && tagSetTimeArray[vt_future]) tense = VT_PASSIVE + VT_FUTURE;
	if (tagSetTimeArray[vt_vACD] && tagSetTimeArray[vt_future]) tense = VT_PASSIVE + VT_FUTURE + VT_EXTENDED;
	if (tagSetTimeArray[vt_vABD] && tagSetTimeArray[vt_future]) tense = VT_PASSIVE + VT_FUTURE_PERFECT;
	if (tagSetTimeArray[vt_vABCD] && tagSetTimeArray[vt_future]) tense = VT_PASSIVE + VT_FUTURE_PERFECT + VT_EXTENDED;
	if (tagSetTimeArray[vt_vAD] && !tense) tense = VT_PASSIVE; // dares be examined?
	// POSSIBLE
	if ((tagSetTimeArray[vt_vS] || tagSetTimeArray[vt_id]) && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PRESENT;
	if (tagSetTimeArray[vt_vS] && !tagSetTimeArray[vt_past] && tagSetTimeArray[vt_imp]) tense = VT_POSSIBLE + VT_PRESENT;
	// did you mean that? (IMPERATIVE)
	if (tagSetTimeArray[vt_past] && tagSetTimeArray[vt_imp]) tense = VT_IMPERATIVE + VT_PAST;
	if (tagSetTimeArray[vt_vACD] && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PRESENT + VT_PASSIVE;
	if (tagSetTimeArray[vt_vAC] && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PRESENT + VT_EXTENDED;
	if (tagSetTimeArray[vt_vAB] && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PAST;
	if (tagSetTimeArray[vt_vABD] && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PAST + VT_PASSIVE;
	if (tagSetTimeArray[vt_vABCD] && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PAST + VT_PASSIVE + VT_EXTENDED;
	if (tagSetTimeArray[vt_vAD] && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PRESENT_PERFECT + VT_PASSIVE;
	if (tagSetTimeArray[vt_vABC] && tagSetTimeArray[vt_conditional]) tense = VT_POSSIBLE + VT_PRESENT_PERFECT + VT_EXTENDED;
	if (tagSetTimeArray[vt_conditional] && !tense) tense = VT_POSSIBLE + VT_PRESENT;
	if (tagSetTimeArray[vt_future] && !tense) tense = VT_FUTURE;
	if (tagSetTimeArray[vt_no] || tagSetTimeArray[vt_never] || tagSetTimeArray[vt_not]) tense |= VT_NEGATION;
	if (verbTagIndex >= 0)
		for (unsigned int I = tagSet[verbTagIndex].sourcePosition; I < tagSet[verbTagIndex].sourcePosition + tagSet[verbTagIndex].len; I++)
			m[I].verbSense = tense;
	if (!(tense & VT_TENSE_MASK))
		tense |= VT_PRESENT;
	if (debugTrace.traceRelations)
	{
		lpwstring tagString;
		for (unsigned int I = 0; I < desiredTagSets[verbSenseTagSet].tags.size(); I++)
			if (tagSetTimeArray[I])
				tagString = tagString + patternTagStrings[desiredTagSets[verbSenseTagSet].tags[I]] + u" ";
		lpwstring tmp;
		if (verbTagIndex >= 0)
			lplog(u"Set verb tags (%d to %d) found tags %s, sense=%s.", tagSet[verbTagIndex].sourcePosition, tagSet[verbTagIndex].sourcePosition + tagSet[verbTagIndex].len, tagString.c_str(), senseString(tmp, tense).c_str());
		else
			lplog(u"Set verb tags (infinitive starting at %d) found tags %s, sense=%s.", tagSet[0].sourcePosition, tagString.c_str(), senseString(tmp, tense).c_str());
	}
	return tense;
}

// Collapse VT_* to 0?7 (tense) + 8 if VT_EXTENDED. -1 stays -1. Unknown
// combinations LOG_FATAL_ERROR.
int cSource::getSimplifiedTense(int tense)
{
	LFS
		if (tense == -1) return -1;
	lpwstring tmp;
	int simplifiedTense = 0;
	switch (tense & ~(VT_POSSIBLE | VT_PASSIVE | VT_NEGATION | VT_EXTENDED | VT_IMPERATIVE))
	{
	case VT_PRESENT: simplifiedTense = 0; break;
	case VT_PRESENT_PERFECT: simplifiedTense = 1; break;
	case VT_PAST: simplifiedTense = 2; break;
	case VT_PAST_PERFECT: simplifiedTense = 3; break;
	case VT_FUTURE: simplifiedTense = 4; break;
	case VT_FUTURE_PERFECT: simplifiedTense = 5; break;
	case VT_PRESENT_PERFECT + VT_VERB_CLAUSE: simplifiedTense = 6; break;
	case VT_PRESENT + VT_VERB_CLAUSE: simplifiedTense = 7; break;
	default:
		lplog(LOG_FATAL_ERROR, u"getSimplifiedTense:Unhandled verb tense %s %d.", senseString(tmp, tense).c_str(), tense);
	}
	bool extended = (tense & VT_EXTENDED) == VT_EXTENDED;
	if (extended) simplifiedTense += 8;
	return simplifiedTense;
}

/*
http://www.wwnorton.com/write/waor.htm
The subjunctive mood is for statements of hypothetical conditions or of wishes, recommendations, requirements, or suggestions. To express the
subjunctive, you often need one of the modal auxiliaries, which include can, could, may, might, must, ought, should, and would. Use them as follows:
1. USE CAN TO EXPRESS
CAPABILITY: Can the Israelis and the Palestinians ever make peace?
PERMISSION: Why can�t first-year college students live off campus?
In formal writing, permission is normally signified by may rather than can,
which is reserved for capability. But can may be used informally to express
permission and is actually better than may in requests for permission involving
the negative. The only alternative to can�t in such questions is the
awkward term mayn�t.
2. USE COULD TO EXPRESS
THE OBJECT OF A WISH: I wish I could climb Mount Everest.
	A CONDITION: If all countries of the world could set aside their antagonism once every four years, the Olympics would be truly international.
	A DISTINCT POSSIBILITY: A major earthquake could strike California within the next ten years.
On the distinction between would and could, see item 8 below.
3. USE MAY TO EXPRESS
	A MILD POSSIBILITY: The next president of the United States may be a woman.
PERMISSION: Students who cannot afford tuition may apply for loans.
4. USE MIGHT TO EXPRESS
	A REMOTE POSSIBILITY: Biogenetic experiments might produce some horribly dangerous new form of life.
	THE RESULT OF A CONTRARY-TO-FACT CONDITION: If I had driven all night, I might have fallen asleep at the wheel.
5. USE OUGHT TO EXPRESS
	A STRONG RECOMMENDATION: The Pentagon ought to eliminate waste in defense spending.
LIKELIHOOD: The new museum ought to be ready by next fall.
Ought is normally followed by the infinitive.
6. USE MUST TO EXPRESS
	AN ABSOLUTE OBLIGATION: Firefighters must be ready for action at any hour of the day or night.
	A FIRM CONCLUSION: William Bligh, who sailed a small boat nearly four thousand miles, must have been an extraordinary seaman.
7. USE SHOULD TO EXPRESS
	ADVICE: Students who hope to get into medical school should take biology.
	EXPECTATION: By the year 2050, the population of the world should exceed eight billion.
8. USE WOULD TO EXPRESS
	THE RESULT OF A CONDITION OR EVENT: If a one-kiloton neutron bomb were exploded a few hundred feet over the Earth, it would kill everyone
within a radius of three hundred yards.
	THE OBJECT OF A WISH: Some people wish the federal government would support them for the rest of their lives.
	// pp 452 Longman grammar:
	// [can/could/may/might
	//  must/should/(had) better/have (got) to/got to/need to/ought to/(be) supposed to/gotta
	//  will/would/shall/should/be going to/gonna
	//  dare to/used to]


	Both would and could may be used to express the object of a wish. But �I wish you could go� means �I wish you were able to go�; �I wish you would
go� means �I wish you were willing to go.�

� Proper name (unique identifier for temporally-defined event): Monday, January, New Year�s Eve, Washington�s birthday
� Number: 3 (as in �He arrived at 3.�), three

We globally refer to names of festivals, holidays and other occasions of religious observance, remembrance of famous massacres, etc. as �holidays�. Some of these expressions, like �Shrove Tuesday� and �Thanksgiving Day� contain trigger words. Others, like �Thanksgiving�, �Christmas�, and �Diwali�, do not.
A tagger is allowed to tag any holiday it wants (sorry, there is NO fixed list of holidays!), but is to assign it a value only when that value can be inferred from the local and global context of the text, rather than from cultural and world knowledge. For example, given �Christmas is celebrated in December�, the value of December is assigned to Christmas, but given only �Christmas left me poor�, �Christmas� is to be tagged without a value.

state changes:
REMEMBER COPULAR VERBS 5.5 p.435 "to be" verbs
time present
Bill:Bill no longer has the ball
ball:the ball is not 'at' Bill
you:you have the ball
ball:the ball is 'at' you
______________________________________
1. collect all verbs having an prepositional phrase INFP as an object
Marla gave the ball to me.

2. With these verbs, consider these cases also
The ball was given to Marla.
Marle gave me the ball.

3.  With all other cases with 'to' and 'from'
and PRIMARILY SPATIAL, collect objects, determining whether
the object is a PLACE, proper noun, time or 'other noun'.  Also
record tense of verb associated with preposition and
whether a 'think' verb was used before it, thus creating
a private instance of this case.

A PLACE may be defined as the object of inside, into, in or within
BUT this does not mean that every object in the sentence is located
at that object.  Considering example A below.  We must consider
the relationship between 'trash' and kitchen and understand that
the 'trash' is contained in the kitchen, so therefore the knife is
in the 'trash' which is in the kitchen.

_______________________________________

combo prepositions
near by
maugre    in spite of
next
next to
atop
in
out a
out down
out except
out for
out of
outta
out while
out with
over again
over cross
over near
over nigh
pace    contrary to
upto
besides   except
vice in the place of
pro in favor of
chez in the home of
absent in the absence of
apropos concerning
apropos of       about
via              by way of

Quirk CGEL p.674
dimension types 0,1,2,3


problem space:
multiple time sources - I want to think that she would like to run tomorrow.
present tense in past tense narrative - don't advance time
narrative assumption-any expression automatically follows the previous one unless a tense change or time word appears.
any plural time category is also considered T_RECURRING

*/
Inflections months[] = { {u"january",SINGULAR},{u"february",SINGULAR},{u"march",SINGULAR},{u"april",SINGULAR},{u"may",SINGULAR},
{u"june",SINGULAR},{u"july",SINGULAR},{u"august",SINGULAR},{u"september",SINGULAR},{u"october",SINGULAR},
{u"november",SINGULAR},{u"december",SINGULAR},{NULL,0} };
// Lexicon abbreviations only - do not add u"may"/u"jun"/u"jul" here: predefineWords
// registers every entry in this array as a lexicon word via the unguarded
// const-lpchar_t*[] overload (no properNounSubClass/inflectionsClass safeguards), and
// "may" is a common modal verb elsewhere in the grammar. months_abb_index maps each
// entry onto months[] (must stay the same length as months_abb, excluding NULL).
const lpchar_t* months_abb[] = { u"jan",u"feb",u"mar",u"apr",u"aug",u"sept",u"oct",u"nov",u"dec",NULL };
static const int months_abb_index[] = { 0, 1, 2, 3, 7, 8, 9, 10, 11 };
// 0-based index into months[], or -1. Abbreviation "aug" correctly returns 7 (August).
int whichMonth(lpwstring w)
{
	LFS
		for (int I = 0; months[I].inflection; I++)
			if (w == months[I].word)
				return I;
	for (int I = 0; months_abb[I]; I++)
		if (w == months_abb[I])
			return months_abb_index[I];
	return -1;
}

const lpchar_t* daysOfWeek_abb[] = { u"sun",u"mon",u"tues",u"wed",u"thurs",u"fri",u"sat",NULL };
Inflections daysOfWeek[] = { {u"sunday",SINGULAR},{u"monday",SINGULAR},{u"tuesday",SINGULAR},{u"wednesday",SINGULAR},
		{u"thursday",SINGULAR},{u"friday",SINGULAR},{u"saturday",SINGULAR},{u"weekend",SINGULAR},
{u"sundays",PLURAL},{u"mondays",PLURAL},{u"tuesdays",PLURAL},{u"wednesdays",PLURAL},
		{u"thursdays",PLURAL},{u"fridays",PLURAL},{u"saturdays",PLURAL},{u"weekends",PLURAL},{NULL,0} };
// 0=Sunday ? 6=Saturday, then weekend / plurals. Abbreviations sun..sat
// map to 0..6. -1 if unknown.
int whichDayOfWeek(lpwstring w)
{
	LFS
		for (int I = 0; daysOfWeek[I].inflection; I++)
			if (w == daysOfWeek[I].word)
				return I;
	for (int I = 0; daysOfWeek_abb[I]; I++)
		if (w == daysOfWeek_abb[I])
			return I;
	return -1;
}

const lpchar_t* twr_ara[] = { u"hourly",u"daily",u"weekly",u"monthly",u"quarterly",u"seasonally",u"yearly",u"annual",u"twice",u"thrice",u"once",u"times",u"every",u"each",u"",NULL };
int recurrence_flags[] = { cHour, cDay, cWeek, cMonth, cQuarter, cSeason, cYear, cYear, -2, -3, -4, -5, -6, -7, cUnspecified,0 };
// Recurrence word -> eCapacity (hourly=cHour, ?) or a negative code
// (-2 twice ? -7 each). Unknown -> cUnspecified.
int whichRecurrence(lpwstring w)
{
	LFS
		for (int I = 0; twr_ara[I]; I++)
			if (w == twr_ara[I])
				return recurrence_flags[I];
	return cUnspecified;
}

// Human-readable dump of this cTimeInfo into tmpstr (type, modifiers,
// capacity, calendar fields, frequency).
lpwstring cTimeInfo::toString(vector <cWordMatch>& m, lpwstring& tmpstr)
{
	LFS
		tmpstr.clear();
	lpwstring s;
	itos(u"", tWhere, tmpstr, u":");
	af(u"timePreviousLink=", timePreviousLink, tmpstr);
	af(u"SPTAnchor=", timeSPTAnchor, tmpstr);
	af(u"ETAnchor=", timeETAnchor, tmpstr);
	//af(u"RTAnchor=",timeRTAnchor,tmpstr);
	tmpstr += u"type=" + timeString(timeRelationType, s) + u" ";
	if (timeModifier != -1) tmpstr += u"M=" + m[timeModifier].word->first + u" ";
	af(u"Modifier2=", timeModifier2, tmpstr);
	af(u"absMetaRelation=", absMetaRelation, tmpstr);
	if (timeCapacity != cUnspecified)
		tmpstr += u"Capacity=" + capacityString(timeCapacity) + u" ";
	af(u"absSeason=", absSeason, tmpstr);
	af(u"absDateSpec=", absDateSpec, tmpstr);
	if (absMonth != -1) tmpstr += months[absMonth].word;
	if (absMonth != -1 && absDayOfMonth != -1)
	{
		itos(u"", absDayOfMonth, tmpstr, (absYear != -1) ? u"," : u" ");
		af(u"", absYear, tmpstr);
	}
	else
	{
		af(u"absDayOfMonth=", absDayOfMonth, tmpstr);
		af(u"absYear=", absYear, tmpstr);
	}
	af(u"absDayOfWeek=", absDayOfWeek, tmpstr);
	af(u"timeOfDay=", timeOfDay, tmpstr);
	af(u"absHour=", absHour, tmpstr);
	af(u"absMinute=", absMinute, tmpstr);
	af(u"absSecond=", absSecond, tmpstr);
	if (absTimeSpec != -1)
		tmpstr += (absTimeSpec == 1) ? u"P.M. " : u"A.M. ";
	if (timeFrequency != -1)
	{
		for (int I = 0; recurrence_flags[I]; I++)
			if (timeFrequency == recurrence_flags[I])
			{
				if (twr_ara[I][0])
					tmpstr += lpwstring(u"frequency=") + twr_ara[I];
				break;
			}
	}
	if (metaDescriptive)
		tmpstr += u"metaDescriptive ";
	af(u"absHoliday=", absHoliday, tmpstr);
	return tmpstr;
}

Inflections seasons[] = { {u"winter",SINGULAR},{u"wintertime",SINGULAR},{u"spring",SINGULAR},{u"springtime",SINGULAR},{u"summer",SINGULAR},{u"summertime",SINGULAR},{u"fall",SINGULAR},
			{u"winters",PLURAL},{u"wintertimes",PLURAL},{u"springs",PLURAL},{u"springtimes",PLURAL},{u"summers",PLURAL},{u"summertimes",PLURAL},{u"falls",PLURAL},{NULL,0} };
// Index into seasons[] (winter=0, wintertime=1, spring=2, ?), or -1.
int whichSeason(lpwstring w)
{
	LFS
		for (int I = 0; seasons[I].inflection; I++)
			if (w == seasons[I].word)
				return I;
	return -1;
}

// If normalize, only copy noun usage costs onto month/day/unit forms.
// Otherwise predefine those forms, OR T_UNIT[/T_LENGTH], then addTimeFlags().
void cWord::createTimeCategories(bool normalize)
{
	LFS
		// TYPE 1 - expressions that denote a specific time or a specific relative time
		// In an expression (_NOUN), this is the main noun
		Inflections timeUnits[] = { {u"second",SINGULAR},{u"minute",SINGULAR},{u"hour",SINGULAR},{u"day",SINGULAR},{u"week",SINGULAR},
			{u"month",SINGULAR},{u"season",SINGULAR},{u"semester",SINGULAR},{u"quarter",SINGULAR},{u"year",SINGULAR},{u"decade",SINGULAR},{u"century",SINGULAR},{u"millenium",SINGULAR},
			{u"seconds",PLURAL},{u"minutes",PLURAL},{u"hours",PLURAL},{u"days",PLURAL},{u"weeks",PLURAL},
			{u"months",PLURAL},{u"seasons",PLURAL},{u"semesters",PLURAL},{u"quarters",PLURAL},{u"years",PLURAL},{u"decades",PLURAL},{u"centuries",PLURAL},{u"millenia",PLURAL},{NULL,0} };
	Inflections dayUnits[] = {
			{u"morning",SINGULAR},{u"afternoon",SINGULAR},{u"evening",SINGULAR},{u"night",SINGULAR},
			{u"mornings",PLURAL},{u"afternoons",PLURAL},{u"evenings",PLURAL},{u"nights",PLURAL},
			{u"tonight",SINGULAR},{u"today",SINGULAR},{u"tomorrow",SINGULAR},{u"yesterday",SINGULAR},{u"midnight",SINGULAR},
			{u"tomorrows",PLURAL},{u"yesterdays",PLURAL},{u"midnights",PLURAL},
			{u"dawn",SINGULAR},{u"dusk",SINGULAR},{u"noon",SINGULAR},{u"sunset",SINGULAR},
			{u"dawns",PLURAL},{u"dusks",PLURAL},{u"noons",PLURAL},{u"sunsets",PLURAL},{NULL,0} };
	Inflections simultaneousUnits[] = { {u"moment",SINGULAR},{u"instant",SINGULAR},{u"flash",SINGULAR},{u"shake",SINGULAR},
	{u"moments",PLURAL},{u"instants",PLURAL},{u"shakes",PLURAL},{NULL,0} };
	Inflections uncertainDurationUnits[] = { {u"while",SINGULAR},{u"time",SINGULAR},{NULL,0} };
	if (normalize)
	{
		usageCostToNoun(months, u"month");
		usageCostToNoun(months_abb, u"month");
		usageCostToNoun(daysOfWeek_abb, u"daysOfWeek");
		usageCostToNoun(daysOfWeek, u"daysOfWeek");
		usageCostToNoun(seasons, u"season");
		usageCostToNoun(timeUnits, u"timeUnit");
		usageCostToNoun(dayUnits, u"dayUnit");
		toLowestUsageCost(uncertainDurationUnits, u"uncertainDurationUnit");
		return;
	}
	predefineWords(months, u"month", u"month", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, months);
	predefineWords(months_abb, u"month", u"month", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, months_abb);
	predefineWords(daysOfWeek_abb, u"daysOfWeek", u"daysOfWeek", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, daysOfWeek_abb);
	predefineWords(daysOfWeek, u"daysOfWeek", u"daysOfWeek", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, daysOfWeek);
	predefineWords(seasons, u"season", u"season", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, seasons);
	predefineWords(timeUnits, u"timeUnit", u"timeUnit", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT | T_LENGTH, timeUnits);
	predefineWords(dayUnits, u"dayUnit", u"dayUnit", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, simultaneousUnits);
	predefineWords(simultaneousUnits, u"simultaneousUnit", u"simultaneousUnit", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, dayUnits);
	predefineWords(uncertainDurationUnits, u"uncertainDurationUnit", u"uncertainDurationUnit", u"noun", cSourceWordInfo::queryOnAnyAppearance, true);
	addTimeFlag(T_UNIT, uncertainDurationUnits);
	const lpchar_t* times[] = { u"3:45",NULL };
	predefineWords(times, u"time", u"time", 0, false);
	const lpchar_t* dates[] = { u"3/4/90",NULL };
	predefineWords(dates, u"date", u"date", 0, false);
	addTimeFlags();
}

// Seed timeFlags on closed-class time words (ago/now/daily/before/since/?).
void cWord::addTimeFlags()
{
	LFS
		/******************************************************************************************/
		// ABSOLUTE TIME
		// words implying a time in the past, present or future by themselves.
		/////////////////////////////////////////////////
		// Then, I couldn't see the logic of it.
		// Recently I have begun having nightmares.
		// Long ago the dinosaurs ruled the earth.
		// I couldn't see yet the wisdom of his words.
		//************ adverbs
		const lpchar_t* twr_ap[] = { u"recently", u"previously", u"already",u"ago",u"then",NULL };
	addTimeFlag(T_BEFORE, twr_ap);
	const lpchar_t* twr_ac[] = { u"currently",u"still",u"now",u"immediately",NULL };
	addTimeFlag(T_PRESENT, twr_ac);
	const lpchar_t* twr_af[] = { u"yet",u"soon",u"straightaway",u"directly",u"shortly",NULL };
	addTimeFlag(T_AFTER, twr_af);
	//************* nouns
	const lpchar_t* twr_u[] = { u"date", u"then", u"anytime",u"late",u"veronal",u"o'clock",NULL };
	addTimeFlag(T_UNIT, twr_u);
	const lpchar_t* twr_l[] = { u"period", u"term", u"age", u"moment", u"time", u"era", u"weekend",u"for ever",NULL };
	addTimeFlag(T_LENGTH | T_UNIT, twr_l);
	// Past her mother's death she couldn't decide.
	const lpchar_t* twr_uap[] = { u"past",u"yesterday",NULL };
	addTimeFlag(T_BEFORE | T_UNIT, twr_uap);
	const lpchar_t* twr_uac[] = { u"present",u"today",u"to-day",NULL };
	addTimeFlag(T_PRESENT | T_UNIT, twr_uac);
	const lpchar_t* twr_uaf[] = { u"future",u"tomorrow",u"tonight",u"to-morrow",u"morrow",u"to-night",NULL };
	addTimeFlag(T_AFTER | T_UNIT, twr_uaf);
	// recurring adverbs having an independent absolute recurring interval
	addTimeFlag(T_RECURRING, twr_ara);
	const lpchar_t* twr_c[] = { u"currently",u"existing",NULL };
	addTimeFlag(T_PRESENT, twr_c);
	/******************************************************************************************/

	/******************************************************************************************/
	// RELATIVE TIME
	// words lending a direction or modification of another time word
	//************ adjectives
	// must modify time expression
	const lpchar_t* twr_r[] = { u"every",u"several",u"each",NULL };
	addTimeFlag(T_RECURRING, twr_r);
	const lpchar_t* twr_v[] = { u"around",u"about",NULL };
	addTimeFlag(T_VAGUE, twr_v);
	const lpchar_t* twr_p[] = { u"prior",u"previous",u"preceding",u"former",u"earlier",u"already",u"recent",u"earliest",u"early",u"medieval",NULL };
	addTimeFlag(T_BEFORE, twr_p);
	const lpchar_t* twr_artc[] = { u"current",NULL };
	addTimeFlag(T_PRESENT, twr_artc);
	//************ conjunctions
	// The next week he was filled with surprises.
	// The next year he couldn't wait to leave.
	// I will do this magic trick next.
	// Don't be in the library past 2 o'clock!
	// Beyond July he was entirely booked.
	const lpchar_t* twr_f[] = { u"after",u"afterwards",u"next",u"following",u"latter",u"later",u"coming",u"latest",u"late",u"soon",u"beyond",NULL };
	addTimeFlag(T_AFTER, twr_f);
	// adverbs
	// then, just
	const lpchar_t* twr_ar[] = { u"again",u"often",u"frequently", u"oft", u"oftentimes", u"ofttimes", u"always", u"ever", u"never", // removed u"much" - if you knew how much I loved you
		u"repeatedly", u"infrequently", u"rarely", u"occasionally", u"seldom",u"usually",u"sometimes",NULL };
	addTimeFlag(T_RECURRING, twr_ar);
	// prepositions
	// must have time expression as object or modifying it
	const lpchar_t* twr_vp[] = { u"around",u"about",u"near",u"along",u"amid",u"than",NULL }; // more than / less than
	addTimeFlag(T_VAGUE, twr_vp);
	// You will be here by noon.
	// The store is closed till August each year.
	// The store is closed from July to August.
	const lpchar_t* twr_b[] = { u"by",u"till",u"until",u"to",u"before",u"ere",NULL };
	addTimeFlag(T_BEFORE, twr_b);
	const lpchar_t* twr_a[] = { u"since",u"from",NULL }; // also "after" under conjunctions
	addTimeFlag(T_AFTER, twr_a);

	// introductory time expressions
	// function: immediately advance timeline by or till the introductory object

	// He ran through the opening door in a second.
	// In the next month he wrote two dozen chapters.
	// Throughout that summer he thought only of her.
	// During August she ate almost nothing.
	// Meanwhile, she fretted.
	// While she played tennis, he golfed.
	const lpchar_t* tw_in[] = { u"inside",u"in",u"within",u"throughout",u"through",u"during",u"meanwhile",u"while",u"for",NULL };
	addTimeFlag(T_THROUGHOUT, tw_in);
	// At the end of July, he was running three miles daily.
	// He decided on December the eighth.
	// Upon discovering the murderer, he set out once again.
	const lpchar_t* tw_at[] = { u"at",u"upon",u"on",u"of",NULL };
	addTimeFlag(T_AT, tw_at);
	// Three time meanings for 'over'
	// He was seen in the vicinity over two weeks ago. (1)
	// Over the two weeks he was supposedly working for FEMA, he actually played pool. (2)
	// He completed the test in over two hours. (3)
	// He completed the race in under 2 minutes.
	const lpchar_t* tw_on[] = { u"over",u"under",NULL };
	addTimeFlag(T_ON, tw_on);
	// Midway through August he quit.
	const lpchar_t* tw_mid[] = { u"midway",NULL };
	addTimeFlag(T_MIDWAY, tw_mid);
	// Between the 2nd and 3rd week in July he played tennis frequently.
	const lpchar_t* tw_int[] = { u"between",NULL };
	addTimeFlag(T_INTERVAL, tw_int);

	const lpchar_t* tw_start[] = { u"start",u"begin",u"commence",u"embark",u"initiate",u"open",u"originate",NULL };
	addTimeFlag(T_START, tw_start);
	const lpchar_t* tw_stop[] = { u"stop",u"halt",u"conclude",u"discontinue",u"close",u"cease",u"quit",u"interrupt",u"suspend",u"pause",NULL };
	addTimeFlag(T_STOP, tw_stop);
	const lpchar_t* tw_finish[] = { u"finish",u"end",u"terminate",u"complete",u"conclude",NULL };
	addTimeFlag(T_FINISH, tw_finish);
	const lpchar_t* tw_resume[] = { u"resume",u"continue",u"recommence",u"renew",u"reopen",u"restart",NULL };
	addTimeFlag(T_RESUME, tw_resume);
	const lpchar_t* tw_card[] = { u"around",u"about",u"by",u"till",u"until",u"to",u"before",u"since",u"from",u"before",u"after",u"at",NULL };
	addTimeFlag(T_CARDTIME, tw_card);


}

/*
names of festivals, holidays and other occasions of religious observance, remembrance of famous massacres, etc. as �holidays�.
Some of these expressions, like �Shrove Tuesday� and �Thanksgiving Day� contain trigger words. Others, like �Thanksgiving�, �Christmas�, and �Diwali�, do not.
A tagger is allowed to tag any holiday it wants (sorry, there is NO fixed list of holidays!), but is to assign it a value only when that value can be inferred
from the local and global context of the text, rather than from cultural and world knowledge. For example, given �Christmas is celebrated in December�,
the value of December is assigned to Christmas, but given only �Christmas left me poor�, �Christmas� is to be tagged without a value.
*/
// per day:
/* http://www.earthcalendar.net/_php/lookup.php?mode=date&m=2&d=2&y=2006 */
// per week or month:
// http://www.earthcalendar.net/_php/lookup.php?mode=datespan

	// Months:
struct
{
	const lpchar_t* name;
	const lpchar_t* beginDate;
	const lpchar_t* endDate;
} holidayMonths[] = {
{u"black history month",u"2/1",u"2/30"},
{u"cinco de mayo",u"5/1",u"5/30"},
{u"national hispanic heritage month",u"9/15",u"10/14 "},
{u"ramadan",u"9/24",u"10/23"},
{u"oktoberfest",u"9/1",u"9/30"},
{u"tu b'shvat",u"2/2",u"2/3"},
{u"passover",u"4/3",u"4/10"},
{u"shavuot",u"5/23",u"5/24"},
{u"rosh hashanah",u"9/22",u"9/24"},
{u"ramadan",u"9/24",u"10/23"},
{u"yom kippur",u"10/1",u"10/2"},
{u"sukkot",u"10/6",u"10/13"},
{u"simchat torah",u"10/14",u"10/15"},
{u"day of the dead",u"11/1",u"11/2"},
{u"advent",u"12/3",u"12/24"},
{u"hanukkah",u"12/16",u"12/23"},
{u"kwanzaa",u"12/26",u"1/1"},
{NULL,NULL,NULL}
};


// Single Days:
struct
{
	const lpchar_t* name;
	const lpchar_t* dayDate;
} holidayDays[] = {
{u"new year's day",u"1/1"},
{u"new years day",u"1/1"},
{u"new years eve",u"12/29"},
{u"new year's eve",u"12/29"},
{u"martin luther king day",u"1/15"},
{u"groundhog day",u"2/2"},
{u"valentine's day",u"2/14"},
{u"chinese new year",u"2/18"},
{u"presidents' day",u"2/19"},
{u"mardi gras",u"2/20"},
{u"holi",u"3/3"},
{u"purim",u"3/4"},
{u"pulaski day",u"3/5"},
{u"st. urho's day",u"3/16"},
{u"st. patrick's day",u"3/17"},
{u"mother's day, uk",u"3/18"},
{u"persian new year",u"3/20"},
{u"april fools' day",u"4/1"},
{u"easter",u"4/8"},
{u"tax day",u"4/15"},
{u"earth day",u"4/22"},
{u"administrative professionals day",u"4/25"},
{u"arbor day",u"4/27"},
{u"may day",u"5/1"},
{u"national day of prayer",u"5/4"},
{u"kentucky derby day",u"5/5"},
{u"nurses day",u"5/6"},
{u"receptionist day",u"5/9"},
{u"mother's day",u"5/13"},
{u"victoria day",u"5/21"},
{u"memorial day",u"5/28"},
{u"flag day",u"6/14"},
{u"father's day",u"6/17"},
{u"independence day",u"7/4"},
{u"bastille day",u"7/14"},
{u"chinese valentine",u"8/19"},
{u"father-in-law's day",u"7/30"},
{u"labor day",u"9/3"},
{u"grandparents day",u"9/9"},
{u"columbus day",u"10/9"},
{u"thanksgiving, canada",u"10/9"},
{u"boss's day",u"10/16"},
{u"diwali",u"10/21"},
{u"sweetest day",u"10/21"},
{u"mother-in-law's day",u"10/22"},
{u"halloween",u"10/31"},
{u"election day",u"11/7"},
{u"veteran's day",u"11/11"},
{u"thanksgiving",u"11/23"},
{u"santa's list day",u"12/4"},
{u"st. nicholas day",u"12/6"},
{u"christmas eve",u"12/24"},
{u"christmas",u"12/25"},
{u"election day",u""},
{u"vacation",u""},
{u"super bowl sunday",u""},
{NULL,NULL}
};

// Index into holidayDays, then holidayMonths (offset by days count), or -1.
int whichHoliday(lpwstring w)
{
	LFS
		int I = 0;
	for (; holidayDays[I].name != NULL; I++)
		if (!lp_wcscasecmp(holidayDays[I].name, w.c_str()))
			return I;
	int addOffset = I;
	for (I = 0; holidayMonths[I].name != NULL; I++)
		if (!lp_wcscasecmp(holidayMonths[I].name, w.c_str()))
			return I + addOffset;
	return -1;
}


// handleExtendedParseWords for every compiled holiday name (multi-word).
void cWord::extendedParseHolidays()
{
	LFS
		for (int I = 0; holidayDays[I].name; I++)
		{
			handleExtendedParseWords(holidayDays[I].name);
		}
	for (int I = 0; holidayMonths[I].name; I++)
	{
		handleExtendedParseWords(holidayMonths[I].name);
	}
}


// Add the ?holiday? form, insert every holiday name into Words with T_UNIT.
// Returns the new form id.
int cWord::predefineHolidays()
{
	LFS
		unsigned int iForm = cForms::addNewForm(u"holiday", u"hol", false, true);
	for (int I = 0; holidayDays[I].name; I++)
	{
		handleExtendedParseWords(holidayDays[I].name);
		bool added;
		addNewOrModify(NULL, holidayDays[I].name, 0, iForm, 0, 0, u"", -1, added);
		tIWMM iWord = query(holidayDays[I].name);
		iWord->second.timeFlags |= T_UNIT;
	}
	for (int I = 0; holidayMonths[I].name; I++)
	{
		handleExtendedParseWords(holidayMonths[I].name);
		bool added;
		addNewOrModify(NULL, holidayMonths[I].name, 0, iForm, 0, 0, u"", -1, added);
		tIWMM iWord = query(holidayMonths[I].name);
		iWord->second.timeFlags |= T_UNIT;
	}
	return iForm;
}

int sts[] = { VT_PRESENT,VT_PRESENT_PERFECT,VT_PAST,VT_PAST_PERFECT,VT_FUTURE,VT_FUTURE_PERFECT,
VT_PRESENT_PERFECT + VT_VERB_CLAUSE,VT_PRESENT + VT_VERB_CLAUSE,
VT_PRESENT + VT_EXTENDED,VT_PRESENT_PERFECT + VT_EXTENDED,VT_PAST + VT_EXTENDED,VT_PAST_PERFECT + VT_EXTENDED,
VT_FUTURE + VT_EXTENDED,VT_FUTURE_PERFECT + VT_EXTENDED,VT_PRESENT_PERFECT + VT_VERB_CLAUSE + VT_EXTENDED,
VT_PRESENT + VT_VERB_CLAUSE + VT_EXTENDED };

int sits[] = { VT_PRESENT,VT_PRESENT_PERFECT,VT_EXTENDED + VT_PRESENT,VT_PASSIVE + VT_PRESENT,
VT_EXTENDED + VT_PRESENT_PERFECT,VT_PASSIVE + VT_PRESENT_PERFECT,VT_PASSIVE + VT_PRESENT + VT_EXTENDED,
VT_PASSIVE + VT_PRESENT_PERFECT + VT_EXTENDED,VT_PASSIVE + VT_PRESENT + VT_EXTENDED + VT_VERB_CLAUSE };


// One tense-statistic line: occurrence, %, passives, followedBy / infinitive.
void cSource::printTenseStatistic(cTenseStat& tenseStatistics, int sense, int numTotal)
{
	LFS
		lpwstring followedByStr, infinitiveStr;
	for (unsigned int J = 0; J < NUM_SIMPLE_TENSE; J++)
	{
		lpchar_t followedBy[1024];
		if (tenseStatistics.followedBy[J])
		{
			lpwstring tmp;
			lp_wsprintf(followedBy, u"%s(%d),", senseString(tmp, sts[J]).c_str(), tenseStatistics.followedBy[J]);
			followedByStr += followedBy;
		}
	}
	for (unsigned int J = 0; J < NUM_INF_TENSE; J++)
	{
		lpchar_t infinitive[1024];
		if (tenseStatistics.infinitive[J])
		{
			lpwstring tmp;
			lp_wsprintf(infinitive, u"%s(%d),", senseString(tmp, sits[J]).c_str(), tenseStatistics.infinitive[J]);
			infinitiveStr += infinitive;
		}
	}
	if (followedByStr.length())
	{
		followedByStr[followedByStr.length() - 1] = u']';
		followedByStr = u" [" + followedByStr;
	}
	if (infinitiveStr.length())
	{
		infinitiveStr[infinitiveStr.length() - 1] = u']';
		infinitiveStr = u" INF[" + infinitiveStr;
	}
	lpwstring tmp;
	if (numTotal && tenseStatistics.occurrence)
		lplog(u"%29s:%05d %03d%% %05d %s %s", senseString(tmp, sense).c_str(),
			tenseStatistics.occurrence, tenseStatistics.occurrence * 100 / numTotal, tenseStatistics.passiveOccurrence, followedByStr.c_str(), infinitiveStr.c_str());
}

// Print NUM_SIMPLE_TENSE slots of tenseStatistics[].
void cSource::printTenseStatistics(const lpchar_t* fromWhere, cTenseStat tenseStatistics[], int numTotal)
{
	LFS
		int numPrintTotal = 0;
	for (unsigned int I = 0; I < NUM_SIMPLE_TENSE; I++)
		numPrintTotal += tenseStatistics[I].occurrence;
	if (!numPrintTotal) return;
	lplog(u"%s: %d Total", fromWhere, numTotal);
	for (unsigned int I = 0; I < NUM_SIMPLE_TENSE; I++)
		printTenseStatistic(tenseStatistics[I], sts[I], numTotal);
}

// Print a sparse tense-statistic map (key = raw verbSense).
void cSource::printTenseStatistics(const lpchar_t* fromWhere, unordered_map <int, cTenseStat>& tenseStatistics, int numTotal)
{
	LFS
		if (!tenseStatistics.size()) return;
	lplog(u"%s: %d Total", fromWhere, numTotal);
	for (unordered_map <int, cTenseStat>::iterator I = tenseStatistics.begin(), IEnd = tenseStatistics.end(); I != IEnd; I++)
		printTenseStatistic(I->second, I->first, numTotal);
}

// Print T_* flags into s. Uses timeWordFlags & 15 as the exclusive type
// (T_META_RELATION=16 and above wrap) plus bits 4?9 as unit/time/date/?.
lpwstring timeString(int timeWordFlags, lpwstring& s)
{
	LFS
		const lpchar_t* tws[] = { u"SEQ",u"before",u"after",u"present",
			u"throughout",u"recurring",u"at",u"midway",
			u"in",u"on",u"interval",u"start",u"stop",u"resume",u"finish",u"range" };
	const lpchar_t* tws2[] = { u" unit",u" time",u" date",u" vague",u" length",u" cardtime" };
	s = tws[timeWordFlags & 15];
	for (int I = 4; I < 10; I++)
		if (timeWordFlags & (1 << I))
			s += tws2[I - 4];
	return s;
}

// Deliberately unimplemented stub: always false. Intended to search
// timelineSegments for the most recent prior segment that shares speakers
// with the one createTimelineSegment() is about to open, so that segment's
// linkage can point back to it. Doing that for real needs the new segment's
// speaker set (available in createTimelineSegment()'s caller-side context,
// e.g. speakerGroups[currentSpeakerGroup]) threaded into this function, which
// takes no parameters today - a signature change that also touches the
// declaration in source.h, out of scope for this pass. ts.linkage (the sole
// caller of this function, in createTimelineSegment()) is serialized to the
// source cache and read back by the pyLPBackEnd Python port, but nothing in
// this C++ pipeline currently reads it back, so leaving this stubbed does not
// silently corrupt any in-process decision - only the persisted field stays
// a placeholder (always 0/false) until this is implemented.
bool cSource::determineTimelineSegmentLink()
{
	LFS
		return false;
}

// Set speakerGroups[sg].tlTransition if the new group is a new cast and/or
// none of them are still physicallyPresent.
// determine whether this speaker group is really a change in perspective from one group of 
// people to another separate group in another location/time.
// executed before marking any speaker non-physical (as part of the transition aging to any new speaker group).
// sets tlTransition flag of speaker group.
bool cSource::speakerGroupTransition(int where, int sg, bool forwardTransition)
{
	LFS
		if (!sg) return false;
	bool allNew = true, allNotPhysicallyPresent = true;
	lpwstring tmpstr;
	// is the newSG composed of people who have not been seen in any previous speaker group?
	if (debugTrace.traceTime)
		lplog(LOG_RESOLUTION | LOG_SG, u"%06d:SGT for %s - %s", where, toText(speakerGroups[sg], tmpstr), (forwardTransition) ? u"forward" : u"");
	for (set <int>::iterator si = speakerGroups[sg].speakers.begin(), siEnd = speakerGroups[sg].speakers.end(); si != siEnd; si++)
	{
		// has this speaker been in any previous speaker group?
		int lastSG = -1;
		for (int I = sg - 1; I >= 0 && lastSG < 0; I--)
			if (speakerGroups[I].speakers.find(*si) != speakerGroups[I].speakers.end())
				lastSG = I;
		speakerGroups[sg].lastSGSpeakerMap[*si] = lastSG;
		if (lastSG >= 0)
			allNew = false;
		vector <cLocalFocus>::iterator lsi = in(*si);
		bool previouslyPhysicallyPresent = (lsi != localObjects.end() && lsi->physicallyPresent);
		speakerGroups[sg].pppMap[*si] = previouslyPhysicallyPresent;
		if (previouslyPhysicallyPresent)
			allNotPhysicallyPresent = false;
		if (debugTrace.traceTime)
			lplog(LOG_RESOLUTION | LOG_SG, u"%06d:SGT speaker %s:lastSG=%d PPP=%s", where, objectString(*si, tmpstr, false).c_str(), lastSG, (previouslyPhysicallyPresent) ? u"true" : u"false");
	}
	if (!forwardTransition)
		speakerGroups[sg].tlTransition = (allNew || allNotPhysicallyPresent) && section < sections.size() && sections[section].endHeader == where;
	if (debugTrace.traceTime)
		lplog(LOG_RESOLUTION | LOG_SG, u"%06d:SGT for %s - transition=%s (section begin=%d) tlTransition=%s", where, toText(speakerGroups[sg], tmpstr), (allNew || allNotPhysicallyPresent) ? u"true" : u"false", (section < sections.size()) ? sections[section].begin : -1, (speakerGroups[sg].tlTransition) ? u"true" : u"false");
	return (allNew || allNotPhysicallyPresent);
}

// create the highest level of time analysis
// anchored and separated by timeTransitions AND location transitions
// also separated by changes in speakerGroups (but not all changes)
// secondary timelines are determined by stories and secondary speaker groups
/*
	on change in speaker group, SPT should be set to the last time set for either of the speakers, or
	if no speaker has a time association, then the last SPT of the last thing spoken in general.
	associate each time with a speaker group.  keep each object associated with the last time index.
3. if start of speakergroup - check all speakers.
		a. If speakers are at all common with the last speaker group, then continue existing SPT.
		b. If speakers are all unknown, if at start of chapter, reset SPT.  If not at start of chapter, continue existing SPT.
		c. otherwise, if at start of chapter, set SPT to last SPT for any speaker.
				If not at start of chapter, check speaker entrance.  If all speakers entered, and if no place is established that is different then current place,
				then continue existing SPT.  If no speaker entered, or if established place (must be near the beginning of the SG) is different than current
				place, then set SPT to last SPT for any speaker.
*/
// Seed timelineSegments with one segment covering the start of the source.
void cSource::initializeTimelineSegments(void)
{
	LFS
		cTimelineSegment ts;
	ts.begin = 0;
	ts.speakerGroup = 0;
	ts.sr = -1;
	ts.parentTimeline = -1;
	timelineSegments.push_back(ts);
}

// Record this SRG as a time/location transition on the current (or
// embedded) timeline segment; open a new segment at an embedded-SG start
// or a tlTransition speaker-group boundary.
void cSource::createTimelineSegment(int where)
{
	LFS
		vector <cSyntacticRelationGroup>::iterator sr = findSyntacticRelationGroup(where);
	if (sr != syntacticRelationGroups.end() && sr->where == where)
	{
		int ctl = currentTimelineSegment;
		if (currentEmbeddedTimelineSegment >= 0 && currentEmbeddedSpeakerGroup >= 0 &&
			where >= speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin &&
			where < speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd &&
			// embedded story will only be set in quote, but an embedded story is set in the past, so it is equivalent to out of quote.
			(m[where].objectRole & (IN_PRIMARY_QUOTE_ROLE | IN_SECONDARY_QUOTE_ROLE)) != 0)
			ctl = currentEmbeddedTimelineSegment;
		if (sr->tft.timeTransition || sr->tft.nonPresentTimeTransition || sr->timeInfo.size() > 0)
			timelineSegments[ctl].timeTransitions.insert((int)(sr - syntacticRelationGroups.begin()));
		if (sr->establishingLocation)
			timelineSegments[ctl].locationTransitions.push_back((int)(sr - syntacticRelationGroups.begin()));
	}
	if (currentEmbeddedSpeakerGroup >= 0 && currentEmbeddedSpeakerGroup < (signed)speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size() && speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin == where)
	{
		if (where < speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd)
		{
			currentEmbeddedTimelineSegment = timelineSegments.size();
			cTimelineSegment ts;
			ts.begin = where;
			ts.end = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd;
			ts.speakerGroup = currentSpeakerGroup;
			ts.sr = (int)(sr - syntacticRelationGroups.begin());
			ts.linkage = determineTimelineSegmentLink();
			ts.parentTimeline = currentTimelineSegment;
			timelineSegments.push_back(ts);
		}
		else
		{
			lpwstring tmpstr, tmpstr2;
			if (debugTrace.traceTime)
				lplog(LOG_RESOLUTION, u"%06d:[%d,%d]embedded speaker group rejected [%s], in %s.",
					where, currentSpeakerGroup, currentEmbeddedSpeakerGroup,
					toText(speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup], tmpstr),
					toText(speakerGroups[currentSpeakerGroup], tmpstr2));
		}
	}
	if (currentSpeakerGroup < speakerGroups.size() && speakerGroups[currentSpeakerGroup].sgBegin == where && speakerGroups[currentSpeakerGroup].tlTransition && where>0)
	{
		timelineSegments[currentTimelineSegment].end = where;
		currentTimelineSegment = timelineSegments.size();
		cTimelineSegment ts;
		ts.begin = where;
		ts.speakerGroup = currentSpeakerGroup;
		ts.sr = (int)(sr - syntacticRelationGroups.begin());
		ts.linkage = determineTimelineSegmentLink();
		ts.parentTimeline = -1;
		timelineSegments.push_back(ts);
	}
}

// 1535: ""           since 1916 ""                ""      AFTER
// 1767:Had not seen for 5 years PRESENT_PERFECT NEGATION THROUGHOUT
// early in the war? [leave till later]