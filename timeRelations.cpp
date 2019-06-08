#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <winsock.h>
#include "Winhttp.h"
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "source.h"
#include "profile.h"

void defineTimePatterns()
{ LFS
  // 2:37 A.M.
	cPattern::create(L"_TIME",L"1",1,L"time{HOUR}",0,1,1,
		1,L"_TIME_ABB{TIMESPEC}",0,1,1,0);
  // 2 A.M. / six p.m.
	cPattern::create(L"__PRETIME",L"",
		1,L"Number{HOUR}",0,1,1,
		1,L"dash",0,1,1,0);
	cPattern::create(L"_TIME",L"2",
		1,L"__PRETIME",0,0,1,
		2,L"numeral_cardinal{HOUR}",L"Number{HOUR}",0,1,1,
		1,L"_TIME_ABB{TIMESPEC}",0,1,1,0);
	// nine thirty
	cPattern::create(L"_TIME",L"C",
		1,L"numeral_cardinal{HOUR}",0,1,1,
		1,L"dash",0,0,1,
		1,L"numeral_cardinal{MINUTE}",0,1,1,
		1,L"_TIME_ABB{TIMESPEC}",0,0,1,0);
  // 2 o'clock / two o'clock 
  cPattern::create(L"_TIME",L"3",
		3,L"adverb|well{TIMEMODIFIER}",L"adverb|almost{TIMEMODIFIER}",L"adverb|nearly{TIMEMODIFIER}",0,0,1,
		3,L"adverb|before{TIMEMODIFIER}",L"adverb|after{TIMEMODIFIER}",L"adjective|past{TIMEMODIFIER}",0,0,1,
		2,L"numeral_cardinal{HOUR}",L"Number{HOUR}",0,1,1,
    1,L"adverb|o'clock*-2",0,1,1,0);
  // 'some' 5 minutes to/till _TIME / 12
	// a few minutes past 11
	// two days from now / two days after Friday evening / two days from next Tuesday
	// five days after Christmas
  // the summer of 1964 
	// twenty after twelve
  cPattern::create(L"_TIME",L"4",
		1,L"adverb",0,0,1, // they had lunch [together] two weeks ago today
		1,L"determiner{DET}",0,0,1,
		2,L"quantifier{TIMEMODIFIER}",L"adjective{TIMEMODIFIER}",0,0,1, // a little after half-past 5
		3,L"numeral_cardinal*-1{TIMEMODIFIER}",L"Number*-1{TIMEMODIFIER}",L"adjective{TIMEMODIFIER}",0,0,1,
		9,L"month*-2{MONTH}",L"daysOfWeek*-2{DAYWEEK}",L"season*-2{SEASON}",L"timeUnit*-2{TIMECAPACITY}",L"dayUnit*-2{TIMECAPACITY}",L"numeral_cardinal*-1{MINUTE}",
		  L"adjective|half*-2{TIMEMODIFIER}",L"adjective|little{TIMEMODIFIER}",L"noun|quarter{TIMEMODIFIER}",0,1,1, // half past noon // a little after 11
		// to, past, before, till, etc
		3,L"_ADVERB{TIMETYPE}",L"adjective|past{TIMETYPE}",L"preposition{TIMETYPE}",0,1,1,
		9,L"_TIME[*]*-1",L"_DATE[*]*-1",L"numeral_cardinal*-1{HOUR}",L"Number*-1{HOUR}",L"adverb|now{HOUR}",L"daysOfWeek*-2{DAYWEEK}",L"season*-2{SEASON}",L"holiday*-2{HOLIDAY}",L"dayUnit*-2{TIMECAPACITY}",0,1,1,
		0);
	// early one Friday night in Fall 1998.
  cPattern::create(L"_TIME",L"D",
		1,L"determiner{DET}",0,0,1,
		1,L"_ADJECTIVE{TIMEMODIFIER}",0,0,1,
		1,L"quantifier{TIMEMODIFIER}",0,0,1,
		1,L"daysOfWeek*-2{DAYWEEK}",0,1,1,
		1,L"dayUnit*-2{TIMECAPACITY}",0,1,1,
		// to, past, before, till, etc
		3,L"_ADVERB{TIMETYPE}",L"adjective|past{TIMETYPE}",L"preposition{TIMETYPE}",0,1,1,
		8,L"_TIME[*]*-1",L"_DATE[*]*-1",L"numeral_cardinal*-1{HOUR}",L"Number*-1{HOUR}",L"adverb|now{HOUR}",L"daysOfWeek*-2{DAYWEEK}",L"season*-2{SEASON}",L"holiday*-2{HOLIDAY}",0,1,1,
		0);
	// three hours after the meeting 
  // the first day of each month in 1999

	// 4 times a week / 4 times each week / 4 times every week
  cPattern::create(L"_TIME",L"5",
		1,L"adverb",0,0,1, // they had lunch [together] two weeks ago today
		2,L"numeral_cardinal{TIMEMODIFIER}",L"Number{TIMEMODIFIER}",0,1,1,
		1,L"noun|times{TIMEMODIFIER}",0,1,1,
		2,L"determiner|a",L"quantifier",0,1,1,
		4,L"month*-2{MONTH}",L"season*-2{SEASON}",L"timeUnit*-2{TIMECAPACITY}",L"dayUnit*-2{TIMECAPACITY}",0,1,1,
		0);
	// a quarter of an hour ago
  cPattern::create(L"_TIME",L"6",
		1,L"adverb",0,0,1, // they had lunch [together] two weeks ago today
		1,L"determiner{DET}",0,0,1,
		2,L"noun|quarter{TIMEMODIFIER}",L"noun|half{TIMEMODIFIER}",0,0,1,
		1,L"preposition|of",0,0,1,
		1,L"determiner{DET}",0,1,1,
		3,L"noun|hour*-2{TIMECAPACITY}",L"noun|minute*-2{TIMECAPACITY}",L"noun|second*-2{TIMECAPACITY}",0,1,1,
		// to, past, before, till, ago, etc
		3,L"adverb{TIMETYPE}",L"to{TIMETYPE}",L"adjective|past{TIMETYPE}",0,1,1,
		3,L"_TIME[*]*-1",L"numeral_cardinal*-1{HOUR}",L"Number*-1{HOUR}",0,0,1,
		0);
	// the stroke of 11
  cPattern::create(L"_TIME",L"E",
		1,L"determiner{DET}",0,1,1,
		1,L"noun|stroke",0,1,1,
		1,L"preposition|of",0,1,1,
		2,L"numeral_cardinal{HOUR}",L"Number{HOUR}",0,1,1,
		0);
	/* already taken by _TIME[9]
  cPattern::create(L"_TIME",L"7",
		1,L"adverb",0,0,1, // they had lunch [together] two weeks ago today
		4,L"numeral_cardinal{TIMEMODIFIER}",L"Number{TIMEMODIFIER}",L"quantifier|some",L"determiner|a",0,1,1,
		5,L"month*-2{MONTH}",L"daysOfWeek*-2{DAYWEEK}",L"season*-2{SEASON}",L"timeUnit*-2{TIMECAPACITY}",L"dayUnit*-2{TIMECAPACITY}",0,1,1,
		0);
		*/
	// Friday evening/afternoon/night
	cPattern::create(L"_TIME",L"8",
		1,L"adverb",0,0,1, // they had lunch [together] two weeks ago today
		1,L"daysOfWeek{DAYWEEK}",0,1,1,
		1,L"dayUnit*-1{TIMECAPACITY}",0,1,1,0);
	// lunch-time, dinner-time
	cPattern::create(L"_TIME",L"F",
		2,L"noun|lunch{TIMECAPACITY}",L"noun|dinner{TIMECAPACITY}",0,1,1,
    1,L"dash|-*-2",0,1,1,
		1,L"noun|time",0,1,1,0);
	// half past noon / 2 / 12
	// next Tuesday - cannot make numeral_ordinal optional because this will create hanging CLOSING_S1s
	// 6 months
	// five days ago / a year ago 
	cPattern::create(L"_MLT",L"9",
		2,L"adverb|more{TIMEMODIFIER}",L"adverb|less{TIMEMODIFIER}",0,1,1,
		1,L"preposition|than",0,1,1,
		0);
	cPattern::create(L"_TIME",L"9",
		1,L"_MLT*-2",0,0,1,
		1,L"determiner|the{DET}",0,0,1, // the past few years
		2,L"adverb{TIMEMODIFIER}",L"quantifier{TIMEMODIFIER}",0,0,1, // they had lunch [together] two weeks ago today / [every] two weeks
		7,L"numeral_ordinal*-1{TIMEMODIFIER}",L"numeral_cardinal*-1{TIMEMODIFIER}",L"Number*-1{TIMEMODIFIER}",L"demonstrative_determiner{TIMEMODIFIER}",L"quantifier{TIMEMODIFIER}",L"determiner|a",L"adjective|other{TIMEMODIFIER}",0,1,1,
		6,L"month*-1{MONTH}",L"daysOfWeek*-1{DAYWEEK}",L"season*-1{SEASON}",L"timeUnit*-1{TIMECAPACITY}",L"dayUnit*-1{TIMECAPACITY}",L"holiday*-1{HOLIDAY}",0,1,1,
		// to, past, before, till, ago, etc
		3,L"adverb{TIMETYPE}",L"to{TIMETYPE}",L"adjective|past{TIMETYPE}",0,0,1,
		1,L"adverb|today{TIMECAPACITY}",0,0,1, // two weeks ago today
		0);
	// 11 in the morning
	cPattern::create(L"_TIME",L"A",
		1,L"adverb",0,0,1, // they had lunch [together] two weeks ago today
										2,L"numeral_cardinal*-1{HOUR}",L"Number*-1{HOUR}",0,1,1,
                    1,L"preposition|in",0,1,1,
										1,L"determiner{DET}",0,1,1,
										1,L"dayUnit*-1{TIMECAPACITY}",0,1,1,0);
	// two weeks every three months
  cPattern::create(L"_TIME",L"B",
		2,L"numeral_cardinal{TIMEMODIFIER}",L"Number{TIMEMODIFIER}",0,1,1,
		5,L"month*-2{MONTH}",L"daysOfWeek*-2{DAYWEEK}",L"season*-2{SEASON}",L"timeUnit*-2{TIMECAPACITY}",L"dayUnit*-2{TIMECAPACITY}",0,1,1,
		1,L"quantifier|every",0,1,1,
		2,L"numeral_cardinal{TIMEMODIFIER}",L"Number{TIMEMODIFIER}",0,0,1,
		5,L"month*-2{MONTH}",L"daysOfWeek*-2{DAYWEEK}",L"season*-2{SEASON}",L"timeUnit*-2{TIMECAPACITY}",L"dayUnit*-2{TIMECAPACITY}",0,1,1,
		0);
	// between now and Monday morning 
	// 6 months or a year from now
	cPattern::create(L"_TIME{MNOUN}",L"C",
		1,L"_TIME{MOBJECT}",0,1,1,
		1,L"coordinator*-1",0,1,1, // prefer over _NOUN[O]
		1,L"_TIME{MOBJECT}",0,1,1,
		0);

  // the 20th of June / the 20th of June, 1980 / the fourteenth of June, 1980
  cPattern::create(L"_DATE",L"1",1,L"determiner",0,0,1,
										1,L"numeral_ordinal{DAYMONTH}",0,1,1,
                    1,L"of",0,0,1,
										1,L"month*-2{MONTH}",0,1,1,
                    1,L",",0,0,1,
										1,L"Number{YEAR}",0,0,1,0);
  // fall 1964 
  cPattern::create(L"_DATE",L"2",
										1,L"season*-2{SEASON}",0,1,1,
										1,L"Number{YEAR}",0,1,1,0);
  // June 20th / May 7 / June twentieth, 1984
	cPattern::create(L"_DATE",L"3",
		1,L"_TIME",0,0,1,
		1,L",",0,0,1,
		2,L"quantifier|every{TIMEMODIFIER}",L"quantifier|each{TIMEMODIFIER}",0,0,1,
		1,L"month*-2{MONTH}",0,1,1,
		2,L"numeral_ordinal{DAYMONTH}",L"Number{DAYMONTH}",0,1,1,
		1,L",",0,0,1,
		1,L"Number*-1{YEAR}",0,0,1,0);
  // 2967 A.D.
	cPattern::create(L"_DATE",L"4",1,L"Number{YEAR}",0,1,1,
		1,L"_DATE_ABB{DATESPEC}",0,1,1,0);
  // Friday, 8 p.m.
	cPattern::create(L"_DATE",L"5",1,L"daysOfWeek*-1{DAYWEEK}",0,1,1,
                        1,L",",0,0,1,
                        1,L"_TIME",0,1,1,0);
  // 1st October 1980
	cPattern::create(L"_DATE",L"6",2,L"numeral_ordinal{DAYMONTH}",L"Number{DAYMONTH}",0,1,1,
												1,L"month*-1{MONTH}",0,1,1,
												1,L"Number{YEAR}",0,1,1,0);
  // June 20th, 1980 / May 7, 1980 / May 6-8
	cPattern::create(L"_DATE",L"7",1,L"month*-1{MONTH}",0,1,1,
												2,L"numeral_ordinal{DAYMONTH}",L"Number{DAYMONTH}",0,1,1,
												1,L",",0,0,1,
												1,L"Number{YEAR}",0,1,1,0);
	cPattern::create(L"_DATE",L"8",1,L"month*-1{MONTH}",0,1,1,
												2,L"numeral_ordinal{DAYMONTH}",L"Number{DAYMONTH}",0,1,1,
												1,L"-*-1",0,1,1,
												2,L"numeral_ordinal{DAYMONTH}",L"Number{DAYMONTH}",0,1,1,0);
	cPattern::create(L"_DATE",L"9",1,L"to",0,1,1,
                        1,L"dash|-*-2",0,1,1,
												2,L"noun|day{TIMECAPACITY}",L"noun|morrow{TIMECAPACITY}",0,1,1,0);
}

bool Source::evaluateHOUR(int where,cTimeInfo &t)
{ LFS
	if (m[where].queryWinnerForm(L"Number")>=0)
	{
		int num;
		if ((num=_wtoi(m[where].word->first.c_str()))>=1 && num<=12)
			t.absHour=num;
		else
			t.absYear=num;
	}
	else if (m[where].queryWinnerForm(L"numeral_cardinal")>=0)
	{
		t.absHour=mapNumeralCardinal(m[where].word->first);
		if (t.absHour<1 || t.absHour>12)
		{
			t.absHour=-1;
			return false;
		}
		// a hundred to one / a thousand to one
		if (where>2 && m[where-1].word->first==L"to" &&
			  m[where-2].queryWinnerForm(L"numeral_cardinal")>=0 && mapNumeralCardinal(m[where-2].word->first)>t.absHour)
			return false;
	}
	else 
		WordClass::processTime(m[where].word->first,t.absHour,t.absMinute);
	return true;
}

bool Source::evaluateDateTime(vector <tTagLocation> &tagSet,cTimeInfo &t,cTimeInfo &rt,bool &rtSet)
{ LFS
	t.clear();
	rt.clear();
	rtSet=false;
	bool tSet=false;
	rt.timeRelationType=T_RANGE;
	int ti=-1,s;
  if ((ti=findOneTag(tagSet,L"TIMESPEC",-1))>=0) // A.M. / P.M.
	{
		tSet=true;
		if (!wcsicmp(m[tagSet[ti].sourcePosition].word->first.c_str(),L"A.M."))
			t.absTimeSpec=0;
		else
			t.absTimeSpec=1;
	}
  if ((ti=findOneTag(tagSet,L"DATESPEC",-1))>=0) // A.D. / B.C.
	{
		tSet=true;
		if (!wcsicmp(m[tagSet[ti].sourcePosition].word->first.c_str(),L"A.D."))
			t.absDateSpec=0;
		else
			t.absDateSpec=1;
	}
	int nextTag=-1;
	if ((ti=findTag(tagSet,L"TIMEMODIFIER",nextTag))>=0)
	{  // text offset
		tSet=true;
		t.timeModifier=tagSet[ti].sourcePosition;
		if ((t.timeFrequency=whichRecurrence(m[t.timeModifier].word->first))!=cUnspecified)
		{
			t.timeModifier=-1;
			t.timeRelationType=T_RECURRING;
		}
		else
			t.timeFrequency=-1;
	}
	if (nextTag>=0)
	{
		tSet=true;
		t.timeModifier2=tagSet[nextTag].sourcePosition;
		if (t.timeFrequency==-1)
		{
			if ((t.timeFrequency=whichRecurrence(m[t.timeModifier2].word->first))!=cUnspecified)
			{
				t.timeModifier2=-1;
				t.timeRelationType=T_RECURRING;
			}
			else
				t.timeFrequency=-1;
		}
	}
	nextTag=-1;
  if ((ti=findTag(tagSet,L"TIMECAPACITY",nextTag))>=0) // hour / minute/ second/ dayUnit
	{
		tSet=true;
		t.timeCapacity=(eCapacity)whichCapacity(m[tagSet[ti].sourcePosition].deriveMainEntry(tagSet[ti].sourcePosition,31,false,true,lastNounNotFound,lastVerbNotFound)->first);
		if (nextTag>=0)
		{
			rtSet=true;
			rt.timeCapacity=(eCapacity)whichCapacity(m[tagSet[nextTag].sourcePosition].deriveMainEntry(tagSet[nextTag].sourcePosition,32,false,true,lastNounNotFound,lastVerbNotFound)->first);
		}
	}
	if ((ti=findOneTag(tagSet,L"TIMETYPE",-1))>=0) // adverb/to/past
	{
		tSet=true;
		wstring w=m[s=tagSet[ti].sourcePosition].word->first;
		int timeFlags=m[s].word->second.timeFlags;
		if (w==L"to")
			t.timeRelationType=T_BEFORE;
		else if (w==L"past")
			t.timeRelationType=T_AFTER;
		else if (timeFlags>0)
			t.timeRelationType=(eTimeWordFlags)timeFlags;
		if (timeFlags==0) // adverb
		{
			if (t.timeModifier<0)
				t.timeModifier=s;
			else 
				t.timeModifier2=s;
		}
	}
	nextTag=-1;
	if ((ti=findTag(tagSet,L"HOUR",nextTag))>=0)
	{
		// Number / numeral_cardinal / time
		if (!evaluateHOUR(tagSet[ti].sourcePosition,t))
		  return false;
		tSet=true;
		if (t.timeCapacity==cUnspecified)
			t.timeCapacity=cHour;
		if (nextTag>=0) // Number / numeral_cardinal / time
			rtSet=evaluateHOUR(tagSet[nextTag].sourcePosition,rt);
	}
	nextTag=-1;
  if ((ti=findTag(tagSet,L"DAYMONTH",nextTag))>=0) // numeral_cardinal / Number
	{
		tSet=true;
		if (m[s=tagSet[ti].sourcePosition].queryWinnerForm(L"Number")>=0)
			t.absDayOfMonth=_wtoi(m[s].word->first.c_str());
		else if (m[s].queryWinnerForm(L"numeral_ordinal")>=0)
			t.absDayOfMonth=mapNumeralOrdinal(m[s].word->first);
		if (nextTag>=0)
		{
			rtSet=true;
			if (m[s=tagSet[nextTag].sourcePosition].queryWinnerForm(L"Number")>=0)
				rt.absDayOfMonth=_wtoi(m[s].word->first.c_str());
			else if (m[s].queryWinnerForm(L"numeral_ordinal")>=0)
				rt.absDayOfMonth=mapNumeralOrdinal(m[s].word->first);
		}
	}
	nextTag=-1;
	// at five to 6 tomorrow.
	// NOT from 5 to 6 tomorrow.
	// NOT 12 o'clock _DATE
	// not one of these days
  if ((ti=findTag(tagSet,L"MINUTE",nextTag))>=0) // numeral_cardinal / Number
	{
		tSet=true;
		if (m[s=tagSet[ti].sourcePosition].queryWinnerForm(L"Number")>=0)
			t.absMinute=_wtoi(m[s].word->first.c_str());
		else if (m[s].queryWinnerForm(L"numeral_cardinal")>=0)
			t.absMinute=mapNumeralCardinal(m[s].word->first);
		if (nextTag>=0)
		{
			rtSet=true;
			if (m[s=tagSet[nextTag].sourcePosition].queryWinnerForm(L"Number")>=0)
				rt.absMinute=_wtoi(m[s].word->first.c_str());
			else if (m[s].queryWinnerForm(L"numeral_cardinal")>=0)
				rt.absMinute=mapNumeralCardinal(m[s].word->first);
		}
		if (t.absMinute<1 || t.absMinute>=60)
			return false;
		// NOT 12 o'clock _DATE
		if (!rtSet && t.absMinute>=0 && s>=0 && m[s+1].word->first==L"o'clock")
		{
			t.absHour=t.absMinute;
			t.absMinute=-1;
		}
	}
	nextTag=-1;
  if ((ti=findTag(tagSet,L"MONTH",nextTag))>=0) // month
	{
		tSet=true;
		t.absMonth=whichMonth(m[tagSet[ti].sourcePosition].getMainEntry()->first);
  if (nextTag>=0) // month
	{
		rtSet=true;
		rt.absMonth=whichMonth(m[tagSet[nextTag].sourcePosition].getMainEntry()->first);
	}
	}
	nextTag=-1;
	if ((ti=findTag(tagSet,L"SEASON",nextTag))>=0 && (m[tagSet[ti].sourcePosition].flags&WordMatch::flagFirstLetterCapitalized)) // season
	{
		tSet=(t.absSeason=whichSeason(m[tagSet[ti].sourcePosition].getMainEntry()->first))>=0;
		if (nextTag>=0 && (m[tagSet[nextTag].sourcePosition].flags&WordMatch::flagFirstLetterCapitalized)) // season
			rtSet=(rt.absSeason=whichSeason(m[tagSet[nextTag].sourcePosition].getMainEntry()->first))>=0;
	}
	nextTag=-1;
  if ((ti=findTag(tagSet,L"YEAR",nextTag))>=0) // numeral_cardinal / Number
	{
		tSet=true;
		if (m[s=tagSet[ti].sourcePosition].queryWinnerForm(L"Number")>=0)
			t.absYear=_wtoi(m[s].word->first.c_str());
		if (nextTag>=0 && m[s=tagSet[nextTag].sourcePosition].queryWinnerForm(L"Number")>=0)
		{
			rtSet=true;
			rt.absYear=_wtoi(m[s].word->first.c_str());
		}
	}
	nextTag=-1;
  if ((ti=findTag(tagSet,L"HOLIDAY",nextTag))>=0) // numeral_cardinal / Number
	{
		tSet=true;
		t.absHoliday=whichHoliday(m[tagSet[ti].sourcePosition].getMainEntry()->first);
		if (nextTag>=0)
		{
			rtSet=true;
			rt.absHoliday=whichHoliday(m[tagSet[nextTag].sourcePosition].getMainEntry()->first);
		}
	}
	nextTag=-1;
  if ((ti=findTag(tagSet,L"DAYWEEK",nextTag))>=0) // daysOfWeek
	{
		tSet=true;
		t.absDayOfWeek=whichDayOfWeek(m[tagSet[ti].sourcePosition].getMainEntry()->first);
	}
  if (nextTag>=0) // daysOfWeek
	{
		rtSet=true;
		rt.absDayOfWeek=whichDayOfWeek(m[tagSet[nextTag].sourcePosition].getMainEntry()->first);
	}
	// from 5 to 6 tomorrow (see _TIME[4])
	if ((ti=findTag(tagSet,L"MINUTE",nextTag))>=0 && m[s=tagSet[ti].sourcePosition-1].word->first==L"from" && t.absMinute>0 && !rtSet)
	{
		rtSet=true;
		rt=t;
		t.clear();
		t.absHour=rt.absMinute;
		rt.absMinute=-1;
		rt.timeRelationType=T_RANGE;
	}
	return tSet;
}

// only copy previous time
// the next year or two.
void Source::copyTimeInfoNum(vector <cTimeInfo>::iterator previousTime,cTimeInfo &t,int num)
{ LFS
	t.timeCapacity=previousTime->timeCapacity;
	if (previousTime->absDayOfMonth>=0)
		t.absDayOfMonth=num;
	else if (previousTime->absHour>=0)
		t.absHour=num;
	else if (previousTime->absYear>=0)
		t.absYear=num;
	else switch (t.timeCapacity)
	{
		case cMillenium: t.absYear=num; break;
		case cCentury: t.absYear=num; break;
		case cDecade: t.absYear=num; break;
		case cYear: t.absYear=num; break;
		case cSemester: t.absYear=num; break;
		case cSeason: t.absSeason=num; break;
		case cQuarter: t.absSeason=num; break;
		case cMonth: t.absMonth=num; break;
		case cWeek: t.absTimeSpec=num; break;
		case cDay: t.absTimeSpec=num; break;
		case cHour: t.absHour=num; break;
		case cMinute: t.absMinute=num; break;
		case cMorning: t.absTimeSpec=num; break;
		case cNoon: t.absTimeSpec=num; break;
		case cAfternoon: t.absTimeSpec=num; break;
		case cEvening: t.absTimeSpec=num; break; 
		case cDusk: t.absTimeSpec=num; break;
		case cNight: t.absTimeSpec=num; break;
		case cMidnight: t.absTimeSpec=num; break;
		case cDawn: t.absTimeSpec=num; break;
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

bool Source::detectTimeTransition(int where,vector <cSpaceRelation>::iterator csr,cTimeInfo &timeInfo)
{ LFS
	if (m[where].getObject()>=0)
	{
		int oc=objects[m[where].getObject()].objectClass;
		if (oc!=NON_GENDERED_GENERAL_OBJECT_CLASS &&
				oc!=NON_GENDERED_NAME_OBJECT_CLASS &&
				oc!=META_GROUP_OBJECT_CLASS)
			return false;
		objects[m[where].getObject()].isTimeObject=true;
		wstring tmpstr;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:made time object [DTT] %s",where,objectString(m[where].getObject(),tmpstr,false).c_str());
	}
	if (csr->relationType==stPREPTIME || csr->relationType==stPREPDATE || csr->relationType==stSUBJDAYOFMONTHTIME || 
		  csr->relationType==stABSTIME || csr->relationType==stABSDATE || csr->relationType==stADVERBTIME)
	{
		csr->tft.timeTransition=true;
		int sr=(int) (csr-spaceRelations.begin());
		wstring description=L"detectTimeTransition";
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:time transition %s",where,srToText(sr,description).c_str());
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
			if (csr->relationType==stBE && 
				  ((csr->whereSubject<0 || (m[csr->whereSubject].word->first!=L"it" && !(m[csr->whereSubject].word->second.timeFlags&T_UNIT))) ||
					 m[csr->whereSubject].relVerb!=csr->whereVerb ||
					// 51060: It was a beautiful day. [NO - ambiguous]
					(timeInfo.timeCapacity==cDay && (m[timeInfo.tWhere].objectRole&OBJECT_ROLE) && (timeInfo.timeModifier<0 || m[timeInfo.timeModifier].queryWinnerForm(adjectiveForm)!=-1)) ||
					 (timeInfo.timeModifier>=0 && (m[timeInfo.timeModifier].word->second.timeFlags&T_BEFORE))))
			{
				int sr=(int) (csr-spaceRelations.begin());
				wstring description=L"detectTimeTransition";
				if (timeInfo.absSeason>=0 || timeInfo.absMonth>=0 || timeInfo.absDayOfWeek>=0 ||
					timeInfo.absDayOfMonth>=0 ||	timeInfo.absHour>=0 ||	timeInfo.absHoliday>=0 ||
					(timeInfo.timeCapacity!=cUnspecified && timeInfo.timeCapacity!=cMoment && timeInfo.timeCapacity!=cMinute && timeInfo.timeCapacity!=cSecond && 
					 // no daily / monthly  
					 timeInfo.timeFrequency==-1))
				{
					if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:BE time transition rejected: %s",where,srToText(sr,description).c_str());
				}
				return false;
			}
			if (csr->whereVerb>=0 && m[csr->whereVerb].previousCompoundPartObject>=0 && 
					(m[m[csr->whereVerb].previousCompoundPartObject].verbSense&VT_POSSIBLE))
			{
				wstring description=L"detectTimeTransition";
				int sr=(int) (csr-spaceRelations.begin());
				if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:probable root time transition %s?",where,srToText(sr,description).c_str());
				csr->tft.nonPresentTimeTransition=true;
				return false;
			}

			if (timeInfo.absSeason>=0 || timeInfo.absMonth>=0 || timeInfo.absDayOfWeek>=0 ||
					timeInfo.absDayOfMonth>=0 ||	timeInfo.absHour>=0 ||	timeInfo.absHoliday>=0 ||
					(timeInfo.timeCapacity!=cUnspecified && timeInfo.timeCapacity!=cMoment && timeInfo.timeCapacity!=cMinute && timeInfo.timeCapacity!=cSecond && 
					 // no daily / monthly  
					 timeInfo.timeFrequency==-1))
			{
				int sr=(int) (csr-spaceRelations.begin());
				wstring description=L"detectTimeTransition";
				if (debugTrace.traceSpeakerResolution)
				{
				if (!csr->tft.timeTransition &&
						!(csr->relationType==stPREPTIME || csr->relationType==stPREPDATE || csr->relationType==stSUBJDAYOFMONTHTIME || 
							csr->relationType==stABSTIME || csr->relationType==stABSDATE || csr->relationType==stADVERBTIME))
					lplog(LOG_RESOLUTION,L"%06d:time transition %s",where,srToText(sr,description).c_str());
					sr=(int) (csr-spaceRelations.begin());
				if (!csr->tft.timeTransition && (m[csr->where].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE)))
					lplog(LOG_RESOLUTION,L"%06d:time transition [IN QUOTE] %s",csr->where,srToText(sr,description).c_str());
				}
				if (csr->tft.presentHappening)
					csr->tft.timeTransition=true;
				else
					csr->tft.nonPresentTimeTransition=true;
			}
		}
	return csr->tft.timeTransition;
}

void Source::markTime(int where,int begin,int len)
{ LFS
	int color=m[where].getMainEntry()->second.timeFlags;
	if (!color && m[where].relPrep>=0)
		color=m[m[where].relPrep].word->second.timeFlags;
	if (!color && len>1)
		color=m[begin+len-1].getMainEntry()->second.timeFlags;
	if (!color) color=T_UNIT;
	if (m[where].relPrep>=0)
	{
		if (color)
			m[m[where].relPrep].timeColor=color;
		m[m[where].relPrep].flags|=WordMatch::flagAlreadyTimeAnalyzed;
	}
	for (vector <WordMatch>::iterator im=m.begin()+begin,imEnd=m.begin()+begin+len; im!=imEnd; im++,begin++) // so begin can match a location!
	{
		if (color)
			im->timeColor=color;
		im->flags|=WordMatch::flagAlreadyTimeAnalyzed;
	}
}

bool Source::evaluateTimePattern(int beginObjectPosition,int &maxLen,cTimeInfo &t,cTimeInfo &rt,bool &rtSet)
{ LFS
	int element;
	if ((element=m[beginObjectPosition].pma.queryPattern(L"_TIME",maxLen))!=-1 ||
			(element=m[beginObjectPosition].pma.queryPattern(L"_DATE",maxLen))!=-1 || 
			(element=m[beginObjectPosition].pma.queryPattern(L"__INTRO_N",maxLen))!=-1 || 
			(element=m[beginObjectPosition].pma.queryPattern(L"_ADVERB",maxLen))!=-1)
	{
		vector < vector <tTagLocation> > tagSets;
		if (startCollectTags(false,timeTagSet,beginObjectPosition,m[beginObjectPosition].pma[element&~patternFlag].pemaByPatternEnd,tagSets,true,true)>0)
			for (unsigned int J=0; J<tagSets.size(); J++)
			{
				//printTagSet(LOG_TIME,L"LT",J,tagSets[J],beginObjectPosition,m[beginObjectPosition].pma[element&~patternFlag].pemaByPatternEnd);
				if (evaluateDateTime(tagSets[J],t,rt,rtSet))
					return true;
			}
	}
	return false;
}

bool Source::identifyDateTime(int where,vector <cSpaceRelation>::iterator csr,int &maxLen,int inMultiObject)
{ LFS
	if (where<0 || (m[where].flags&WordMatch::flagAlreadyTimeAnalyzed)) return false;
	bool rtSet=false;
	cTimeInfo t,rt;
	rt.tWhere=t.tWhere=where;
	t.timeRTAnchor=rt.timeRTAnchor=where;
	int beginObjectPosition,element;
	beginObjectPosition=m[where].beginObjectPosition;
	if (beginObjectPosition<0) beginObjectPosition=where;
	if (inMultiObject==0 && (element=m[beginObjectPosition].pma.queryTag(MNOUN_TAG))>=0 && resolveTimeRange(beginObjectPosition,element,csr))
		return true;
	maxLen=-1;
	if (evaluateTimePattern(beginObjectPosition,maxLen,t,rt,rtSet) && m[beginObjectPosition].timeColor==0 && beginObjectPosition+maxLen>where)
	{
		rt.tWhere=t.tWhere=where;
		t.timeRTAnchor=rt.timeRTAnchor=where;
		if (m[where].relPrep>=0)
		{ 
			t.timeRelationType=(eTimeWordFlags)m[m[where].relPrep].word->second.timeFlags;
			t.metaDescriptive=(m[m[where].relPrep].word->first==L"of");
		}
		if (csr->relationType==stBE && m[csr->where].word->first==L"it" && csr->whereObject>=0 && 
			  (csr->whereObject==where || m[csr->whereObject].beginObjectPosition==where))
		{
			if (m[beginObjectPosition].pma.queryPattern(L"_DATE")!=-1)
				csr->relationType=stABSDATE;
			else //if (m[whereTime].pma.queryPattern(L"_TIME")!=-1)
				csr->relationType=stABSTIME;
		}
		csr->timeInfo.push_back(t);
		detectTimeTransition(where,csr,t);
		if (rtSet)
		{
			csr->timeInfo.push_back(rt);
			detectTimeTransition(where,csr,rt);
		}
		markTime(where,beginObjectPosition,maxLen);
		return true;
	}
	if ((element=m[beginObjectPosition].pma.queryPattern(L"_NUMBER",maxLen))!=-1 && maxLen==1 && beginObjectPosition>2 &&
		  m[beginObjectPosition-1].word->first==L",") 
	{
		int year=_wtoi(m[beginObjectPosition].word->first.c_str());
		if (year>1000 && year<2500)
		{
			t.timeRelationType=T_UNIT;
			t.timeCapacity=cYear;
			t.absYear=year;
			csr->timeInfo.push_back(t);
			detectTimeTransition(where,csr,t);
			markTime(where,beginObjectPosition,maxLen);
			return true;
		}
	}
	t.clear();
	rt.clear();
	rt.tWhere=t.tWhere=where;
	t.timeRTAnchor=rt.timeRTAnchor=where;
	if (m[where].relPrep>=0)
	{
		if (m[m[where].relPrep].getRelObject()==where)
		{
			t.timeRelationType=(eTimeWordFlags)m[m[where].relPrep].word->second.timeFlags;
			t.metaDescriptive=(m[m[where].relPrep].word->first==L"of");
		}
		else if (m[m[where].relPrep].word->first==L"of" && (m[where].word->first==L"second" || m[where].getMainEntry()->first==L"shake"))
			return false;
	}
	wstring word=m[where].deriveMainEntry(where,34,false,true,lastNounNotFound,lastVerbNotFound)->first;
	if (m[where].queryForm(L"timeUnit")>=0 || m[where].queryForm(L"dayUnit")>=0)
		t.timeCapacity=(eCapacity)whichCapacity(word);
	else if (m[where].queryForm(L"month")>=0)
	{
		t.timeCapacity=cNamedMonth;
		t.absMonth=whichMonth(m[where].word->first);
	}
	else if (m[where].queryForm(L"daysOfWeek")>=0)
	{
		t.timeCapacity=cNamedDay;
		t.absDayOfWeek=whichDayOfWeek(m[where].word->first);
	}
	else if (m[where].queryForm(L"season")>=0 && (m[where].flags&WordMatch::flagFirstLetterCapitalized))
	{
		t.timeCapacity=cNamedSeason;
		t.absSeason=whichSeason(m[where].word->first);
	}
	else if (m[where].queryForm(L"simultaneousUnit")>=0)
		t.timeCapacity=cMoment;
	else if (m[where].queryForm(L"holiday")>=0)
	{
		t.timeCapacity=cNamedHoliday;
		t.absHoliday=whichHoliday(m[where].word->first);
	}
	else if (m[where].queryForm(timeForm)>=0)
	{
		WordClass::processTime(m[where].word->first,t.absHour,t.absMinute);
	}
	else if (m[where].queryForm(dateForm)>=0)
	{
		WordClass::processDate(m[where].word->first,t.absYear,t.absMonth,t.absDayOfMonth);
	}
	else 
	{ // the 1994 crisis
		t.timeCapacity=cUnspecified;
		for (int I=m[where].beginObjectPosition; I>=0 && I<where; I++)
			if ((m[I].forms.isSet(NUMBER_FORM_NUM) || 
				m[I].word->second.query(L"month")>=0 || m[I].word->second.query(L"daysOfWeek")>=0 || m[I].word->second.query(L"season")>=0 || 
				m[I].getMainEntry()->second.query(L"timeUnit")>=0 || m[I].getMainEntry()->second.query(L"dayUnit")>=0) && 
				!m[I].forms.isSet(numeralOrdinalForm) && // not 'second'
				  (m[I].pma.queryPatternWithLen(L"__ADJECTIVE",1)!=-1 || m[I].pma.queryPatternWithLen(L"__NADJECTIVE",1)!=-1))
			{
				if (m[I].word->second.query(L"month")>=0)
				{
					t.timeCapacity=cNamedMonth;
					t.absMonth=whichMonth(m[I].word->first);
					t.timeRelationType=T_MODIFIER;
					break;
				}
				else if (m[I].word->second.query(L"daysOfWeek")>=0)
				{
					t.timeCapacity=cNamedDay;
					t.absDayOfWeek=whichDayOfWeek(m[I].word->first);
					t.timeRelationType=T_MODIFIER;
					break;
				}
				else if (m[I].word->second.query(L"season")>=0 && (m[I].flags&WordMatch::flagFirstLetterCapitalized))
				{
					t.timeCapacity=cNamedSeason;
					t.absSeason=whichSeason(m[I].word->first);
					t.timeRelationType=T_MODIFIER;
					break;
				}
				else if (m[I].word->second.query(L"holiday")>=0)
				{
					t.timeCapacity=cNamedHoliday;
					t.absHoliday=whichHoliday(m[I].word->first);
					t.timeRelationType=T_MODIFIER;
					break;
				}
				else if (m[I].getMainEntry()->second.query(L"timeUnit")>=0 || m[I].getMainEntry()->second.query(L"dayUnit")>=0)
				{
					if (I>0 && (m[I-1].forms.isSet(numeralCardinalForm) || m[I-1].forms.isSet(numeralOrdinalForm)) && t.timeModifier<0)
						t.timeModifier=I-1;
					t.timeCapacity=(eCapacity)whichCapacity(m[I].getMainEntry()->first);
					t.timeRelationType=T_MODIFIER;
					break;
				}
				else
				{
					int year=_wtoi(m[I].word->first.c_str());
					if (year>1200 && year<2100)
					{
						t.timeCapacity=cYear;
						t.absYear=year;
						t.timeRelationType=T_MODIFIER;
						break;
					}
				}
			}
			else if (m[I].word->second.timeFlags&T_RECURRING) // daily
				t.timeCapacity=(eCapacity)(t.timeFrequency=whichRecurrence(m[I].word->first));

		// must not be used as an adjective, and must be an object of a preposition
		if ((m[where].forms.isSet(numeralCardinalForm) || m[where].forms.isSet(numeralOrdinalForm) || m[where].forms.isSet(NUMBER_FORM_NUM)) &&
				m[beginObjectPosition].pma.queryPatternWithLen(L"__NOUN",where-beginObjectPosition+1)!=-1 &&
				m[beginObjectPosition].pma.queryPattern(L"__NOUN",L"Q")==-1)
		{
			vector <cTimeInfo>::iterator ti;
			if (inMultiObject==2)
				ti=csr->timeInfo.begin()+csr->timeInfo.size()-1;
			if (m[where].forms.isSet(NUMBER_FORM_NUM))
			{
				int num=_wtoi(m[where].word->first.c_str());
				if (inMultiObject==2)
				  copyTimeInfoNum(ti,t,num); // only copy previous time
				else if (num>1200 && num<2100)
				{
					t.timeCapacity=cYear;
					t.absYear=num;
				}
				else if (num>=1 && num<=12)
				{
					t.timeCapacity=cHour;
					t.absHour=num;
				}
			}
			if (m[where].forms.isSet(numeralOrdinalForm))
			{
				int num=mapNumeralOrdinal(m[where].word->first.c_str());
				if (inMultiObject==2)
				  copyTimeInfoNum(ti,t,num); // only copy previous time
				// don't make 'the first' be a time if it is not an object of a preposition
				else if (num>=1 && num<=31 && m[where].relPrep>=0 && m[where].endObjectPosition-m[where].beginObjectPosition>1)
				{
					t.timeCapacity=cDay;
					t.absDayOfMonth=num;
				}
				else if (inMultiObject==1)
				{
					t.timeCapacity=cUnspecified;
					t.timeModifier=where;
				}
			}
			if (m[where].forms.isSet(numeralCardinalForm) && 
				  // events held one up.
				  m[where].relPrep>=0 &&
				  m[where+1].word->first!=L"of" && 
					// we got to one that seemed...
					m[where+1].word->first!=L"that" && 
				  // a hiding place for one
				  (m[where].relPrep<0 || (m[m[where].relPrep].word->second.timeFlags!=T_THROUGHOUT)) &&
					// one who he could trust
					(m[where].getObject()<0 || (objects[m[where].getObject()].whereRelativeClause<0 && 
					// he wanted a new one
					 (m[where].endObjectPosition-m[where].beginObjectPosition)==1)) &&
					// from one to the other
					(m[where].relPrep<0 || m[m[where].relPrep].relPrep<0 || 
					 m[m[where].relPrep].word->first!=L"from" || !(m[m[m[where].relPrep].relPrep].word->second.timeFlags&T_BEFORE) ||
					 m[m[m[where].relPrep].relPrep].getRelObject()<0 ||
					 m[m[m[m[where].relPrep].relPrep].getRelObject()].forms.isSet(numeralCardinalForm)))
			{
				int num=mapNumeralCardinal(m[where].word->first.c_str());
				if (inMultiObject==2)
				  copyTimeInfoNum(ti,t,num); // only copy previous time
				else if (num>=1 && num<=12)
				{
					t.timeCapacity=cHour;
					t.absHour=num;
					if (where+2<(int)m.size() && m[where+1].queryWinnerForm(dashForm)!=-1 && m[where+2].forms.isSet(numeralCardinalForm))
					{
						num=mapNumeralCardinal(m[where+2].word->first.c_str());
						if (num>=1 && num<60)
							t.absMinute=num;
					}
				}
			}
		}
		if ((m[where].relPrep<0 || !t.timeRelationType) && t.timeRelationType!=T_MODIFIER &&
					//coming of the light / following of the Druids
			  (m[where].queryWinnerForm(verbForm)==-1 || m[where+1].word->first!=L"of"))
			t.timeRelationType=(eTimeWordFlags)(m[where].word->second.timeFlags);
		int tt=t.timeRelationType;
		// cancel if this word is supposed to be a preposition (its timeFlag only applies to the preposition form)
		if ((tt==T_THROUGHOUT || tt==T_AT || tt==T_ON || tt==T_MIDWAY || tt==T_INTERVAL || 
				 tt==(T_BEFORE|T_CARDTIME) || tt==(T_AT|T_CARDTIME) || tt==(T_AFTER|T_CARDTIME)) && 
			  (m[where].queryWinnerForm(prepositionForm)==-1 || t.empty()))
			tt=t.timeRelationType=(eTimeWordFlags)0;
		if (!tt && (t.timeCapacity==cUnspecified || t.empty()) && inMultiObject!=1) 
		{
			return false;
		}
		if (t.timeRelationType==T_RECURRING || word==L"time")
		{
			t.timeFrequency=whichRecurrence(m[where].word->first);
			if (m[where].word->first==L"times" && (m[where].flags&WordMatch::flagFirstLetterCapitalized))
				return false; // the Times (of London)
			// five times better!
			if (where+1<(int)m.size() && m[where].word->first==L"times" && m[where-1].queryWinnerForm(numeralCardinalForm)>=0 && 
				  m[where+1].queryWinnerForm(adjectiveForm)>=0 && (m[where+1].word->second.inflectionFlags&ADJECTIVE_COMPARATIVE)!=0)
				return false;
		}
	}
	int len;
	// fall semester / this weekend (encoded as_ADVERB[T]) / 4th quarter / it was only yesterday morning (yesterday is incorrectly flagged as adverb)
	if (where>0 && t.timeRelationType!=T_MODIFIER && 
		  (m[where-1].pma.queryPattern(L"_ADJECTIVE")!=-1 || (m[where-1].queryWinnerForm(adverbForm)!=-1 && m[where-1].queryForm(adjectiveForm)!=-1) ||
		              ((m[where-1].queryWinnerForm(L"noun")>=0 && m[where-1].queryForm(L"season")>=0) || m[where-1].queryForm(demonstrativeDeterminerForm)>=0)))
		t.timeModifier=where-1;
	// half an hour
	else if (where>1 && m[where-1].queryWinnerForm(determinerForm)>=0 && m[where-2].queryWinnerForm(L"predeterminer")>=0)
		t.timeModifier=where-2;
	else if (where+1<(int)m.size() && m[where+1].queryWinnerForm(adverbForm)>=0 && 
		       (m[where+1].word->second.timeFlags&(T_BEFORE|T_AFTER|T_PRESENT)) &&  // ago
					 !(t.timeRelationType&(T_BEFORE|T_AFTER|T_PRESENT))) // Since then - since should take precedence (AFTER)
		t.timeRelationType=(eTimeWordFlags)(m[where+1].word->second.timeFlags&(T_BEFORE|T_AFTER|T_PRESENT));
	else if (m[beginObjectPosition].pma.queryPattern(L"_NAME",len)!=-1 && beginObjectPosition+len<m[where].endObjectPosition)
		t.timeModifier=beginObjectPosition;
	if (t.timeRelationType!=T_MODIFIER && t.timeModifier>=0 && where>1 && where-2>=beginObjectPosition && m[where-2].pma.queryPattern(L"_ADJECTIVE")!=-1 )
		t.timeModifier2=where-2;
	// the time was from the where position, and NAME objects have the principalWherePosition at the beginning (Saturday Night Live)
	if (t.timeRelationType!=T_MODIFIER && where==beginObjectPosition && where<m[where].endObjectPosition && m[where].getObject()>=0 && 
		  (objects[m[where].getObject()].objectClass==NAME_OBJECT_CLASS || objects[m[where].getObject()].objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
	{
		t.timeModifier=where;
		t.timeRelationType=T_MODIFIER;
	}
	maxLen=1;
	if (m[where].endObjectPosition>=0) maxLen=m[where].endObjectPosition-beginObjectPosition;
	if (maxLen==1 && m[where].queryWinnerForm(conjunctionForm)!=-1 && m[where].word->second.timeFlags && m[where+1].pma.queryPattern(L"__S1")!=-1)
	{
		t.timeRelationType=T_META_RELATION;
		t.absMetaRelation=where+1;
	}
	// 7, 10 and 'time'
	if (t.empty() && m[where].relPrep>=0 && (t.timeCapacity==cUnspecified) && !((eTimeWordFlags)(m[where].word->second.timeFlags)))
		return false;
	if (t.empty() && m[where].word->first==L"past" && m[where-1].queryWinnerForm(numeralCardinalForm)!=-1)
	{
		int num=mapNumeralCardinal(m[where-1].word->first.c_str());
		if (num>=1 && num<=59)
		{
			t.absMinute=num;
			t.timeCapacity=cMinute;
			t.timeRelationType=T_BEFORE;
		}
	}
	csr->timeInfo.push_back(t);
	if (((t.timeRelationType || t.timeCapacity!=cUnspecified) && !t.empty() && t.timeRelationType!=T_MODIFIER) || 
		(t.timeRelationType!=cUnspecified && t.timeRelationType!=T_MODIFIER && 
		 t.timeCapacity!=cUnspecified && t.timeCapacity!=cMoment && t.timeCapacity!=cMinute && t.timeCapacity!=cSecond))
		detectTimeTransition(where,csr,t);
	if (rtSet)
	{
		csr->timeInfo.push_back(rt);
		detectTimeTransition(where,csr,rt);
	}
	if (t.timeRelationType==T_MODIFIER)
		maxLen--;
	if (!inMultiObject || inMultiObject==2)
		markTime(where,beginObjectPosition,maxLen);
	return true;
}

wchar_t *twsCapacity[]={ L"millenium",L"century",L"decade",L"year",L"semester",L"season",L"quarter",L"month",L"week",L"day",
	 L"hour",L"minute",L"second",L"moment",
	 L"morning",L"noon",L"afternoon",L"evening",L"dusk",L"night",L"midnight",L"dawn",
	 L"tonight",L"today",L"tomorrow",L"morrow",L"yesterday",
	 L"NamedMonth",L"NamedDay",L"NamedSeason",
	 L"unspecified",NULL };

int whichCapacity(wstring w)
{ LFS
	for (int I=0; twsCapacity[I]; I++)
		if (twsCapacity[I]==w) 
			return I;
	return -1;
}



wstring capacityString(int capacityFlags)
{ LFS
  if (capacityFlags<sizeof(twsCapacity)/sizeof(wchar_t *) && twsCapacity[capacityFlags]!=NULL)
  	return twsCapacity[capacityFlags];
	return L"illegal";
}

/* 
	State Verbs 
State verbs are a small group of verbs in English which don’t usually have continuous
forms, but use only simple verb forms. They are sometimes called “stative” verbs or “non-
progressive verbs”. They describe rather state than an action. States which are either 
continuous or permanent –- without using a continuous tense state. Often stative verbs are 
about liking or disliking something, or about a mental state, not about an action.
State verbs are different from active verbs(also called dynamic verbs), which describe 
deliberate physical actions, e.g.run,eat,put, etc.  Some verbs can be both state and action verbs 
depending on the context in which they’re being used (*).

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
b) agree She didn’t agree with us. She wasn’t agreeing with us.
appear It appears to be raining. It is appearing to be raining.
e) be* is usually a stative verb, but when it is used in the continuous it means ‘behaving’ or ‘acting’
		you are stupid = it’s part of your personality
		you are being stupid = only now, not usually
		I am the king of Kongo.
		We say: “Sue is nearly forty years old.” not “Sue is being nearly forty years old.” 
a) believe I don’t believe the news. I am not believing the news.
f) belong This book belonged to my grandfather. This book was belonging to my grandfather.
concern This concerns you. This is concerning you.
g) consist Bread consists of flour, water and yeast. Bread is consisting of flour, water and yeast.
g) contain This box contains a cake. This box is containing a cake.
depend It depends on the weather. It’s depending on the weather.
e) deserve He deserves to pass the exam. He is deserving to pass the exam.
b) disagree I disagree with you. I am disagreeing with you.
c) dislike I have disliked mushrooms for years.  I have been disliking mushrooms for years.
b) doubt I doubt what you are saying. I am doubting what you are saying.
c) fancy
d) feel * (=have an opinion) I don’t feel that this is a good idea. I am not feeling that this is a good idea.
e) fit (clothes) * This shirt fits me well. This shirt is fitting me well.
a) forget *
c) hate Julie’s always hated dogs. Julie’s always been hating dogs.
f) have *
		have (stative) = own  I have a car
			'I have a car.’ – state verb showing possession
			“I have two garages.”(general state of ownership) not:“I’m having two garages.” 
		have (dynamic) = part of an expression  I’m having a party / a picnic / a bath / a good time / a break
			“We’re having dinner at Emily’s house.” (deliberate action ) 
			'I am having a bath.’ – action verb which, in this case, means ‘taking’.
d) hear Do you hear music? Are you hearing music? [wrong]
a) imagine I imagine you must be tired. I am imagining you must be tired.
b) impress He impressed me with his story. He was impressing me with his story.
g) include This cookbook includes a recipe for bread. This cookbook is including a recipe for bread.
e) involve The job involves a lot of travelling. The job is involving a lot of travelling.
a) judge *
e) keep (continue) *
a) know I’ve known Julie for ten years. I’ve been knowing Julie for ten years.
e) lie (position) *
c) like I like reading detective stories. I am liking reading detective stories.
e) last (duration) *
c) loathe 
c) love I love chocolate. I’m loving chocolate.*
e) matter It doesn’t matter. It isn’t mattering.
b) mean ‘Enormous’ means ‘very big’. ‘Enormous’ is meaning ‘very big’.
h) measure (=be long) This window measures 150cm. This window is measuring 150cm.
b) mind She doesn’t mind the noise. She isn’t minding the noise.
b) need At three o’clock yesterday I needed a taxi. At three o’clock yesterday I was needing a taxi.
b) notice
f) owe I owe you £20. I am owing you £20.
f) own She owns two cars. She is owning two cars.
f) possess
c) prefer I prefer chocolate ice cream. I am preferring chocolate ice cream.
b) promise I promise to help you tomorrow. I am promising to help you tomorrow.
a) realise I didn’t realise the problem. I wasn’t realising the problem.
a) recognise I didn’t recognise my old friend. I wasn’t recognising my old friend.
b) refuse
a) remember He didn’t remember my name. He wasn’t remembering my name.
d) see *
		see (stative) = see with your eyes / understand  I see what you mean  I see her 
		We say: “I saw a bird sitting on a branch.” not “I was seeing a bird sitting on a branch.” 
		see (dynamic) = meet / have a relationship with  I’ve been seeing my boyfriend for three years  I’m seeing Robert tomorrow
e) seem The weather seems to be improving.  The weather is seeming to be improving.
d) sense *
d) smell * I can smell something burning.
e) sound Your idea sounds great. Your idea is sounding great.
b) suppose I suppose John will be late. I’m supposing John will be late.
surprise The noise surprised me. The noise was surprising me.
b) suspect 
d) taste (also: smell, feel, look) *
		(stative) = has a certain taste   This soup tastes great
		(dynamic) = the action of tasting  The chef is tasting the soup
a) think *
		(stative) = have an opinion  I think that coffee is great
			I think you are cool.’– state verb meaning ‘in my opinion’.
		(dynamic) = consider, have in my head  what are you thinking about? I’m thinking about my next holiday
b) trust *
a) understand I don’t understand this question. I’m not understanding this question.
a) want I want to go to the cinema tonight. I am wanting to go to the cinema tonight.
h) weigh (=have weight) This cake weighs 450g. This cake is weighing 450g.
a) wish I wish I had studied more. I am wishing I had studied more.
 
*/

wchar_t *stateVerbs[] = 
{ L"accept",L"agree",L"believe",L"belong",L"consist",L"contain",L"deserve",L"disagree",L"dislike",L"doubt",L"fancy",L"hate",
  L"imagine",L"impress",L"include",L"involve",L"know",L"like",L"loathe",L"love",L"matter",L"mean",L"measure",L"mind",L"need",L"notice",
	L"owe",L"own",L"possess",L"prefer",L"refuse",L"sound",L"suppose",L"suspect",L"weigh",NULL
};

wchar_t *possibleStateVerbs[] = 
{ L"am", L"feel", L"fit", L"forget", L"have", L"judge",L"keep",L"lie",L"last",L"promise",L"realize",L"recognize",L"remember",L"see",L"seem",
	L"sense",L"smell",L"taste",L"think",L"trust",L"understand",L"want",L"wish",NULL
};

tPrepEquivalent prepEquivalents[] = 
{
{L"abaft",  L"behind"},       {L"acrost",  L"across"},       {L"adown",  L"down"},         {L"aff",    L"off"},
{L"agin",   L"against"},      {L"again",   L"against"},      {L"air",    L"before"},       {L"alang",  L"along"},
{L"amang",  L"among"},        {L"anear",   L"near"},         {L"anent",  L"about"},        {L"anigh",  L"near"},
{L"ben",    L"within"},       {L"beyant",  L"beyond"},       {L"bit",    L"but"},          {L"bout",   L"but"},
{L"cep",    L"except"},       {L"circiter", L"about"},       {L"cross",  L"across"},       {L"forby",  L"near"},
{L"forbye", L"near"},         {L"fore",    L"before"},       {L"forth",  L"from"},         {L"frae",   L"from"},
{L"neath",  L"beneath"},      {L"malgre",  L"despite"},      {L"outside",L"by"},           {L"sith",   L"since"}, // 'outside' may also be 'except'
{L"thro",   L"through"},      {L"withal",  L"with"},         {L"aloft",  L"above"},        {L"amongst", L"among"},
{L"amidst", L"amid"},         {L"anti",    L"against"},      {L"bar",    L"except"},       {L"despite", L"in spite of"},
{L"fro",    L"from"},         {L"nigh",    L"near"},         {L"o'er",   L"over"},         {L"aside",  L"past"},
{L"mid",    L"amid"},         {L"round",   L"throughout"},   {L"sans",   L"without"},      {L"thru",   L"through"},
{L"til",    L"till"},         {L"till",    L"until"},        {L"tween",  L"between"},      {L"twixt",  L"between"},
{L"betwixt",L"between"},      {L"toward",  L"towards"},      {L"ere",    L"before"},       {L"con",    L"against"},
{L"de",     L"of"},           {L"abroad",  L"throughout"},   {L"circa",  L"around"},       {L"contra", L"against"},
{L"re",     L"regarding"},    {L"save",    L"except"},       {L"upside", L"against"},      {L"qua",    L"as"},
{L"along",  L"alongside"},    {L"underneath", L"beneath"},   {L"unto",   L"towards"},      {L"astraddle", L"astride"},
{NULL, NULL}
};

tPrepRelation prepRelations[] =
{
  // PRIMARILY SPATIAL - near but not in
	// Tommy sat down opposite her.
  {L"near",tprNEAR},{L"next",tprNEAR},{L"about",tprNEAR},{L"along",tprNEAR},{L"amid",tprNEAR},{L"among",tprNEAR},{L"around",tprNEAR},{L"by",tprNEAR},{L"opposite",tprNEAR}, // {L"with",tprNEAR},
  // x direction
  {L"abreast",tprX},{L"beside",tprX},{L"across",tprX},{L"aslant",tprX},
  // inside dim 2 or 3 CGEL
  // The face appeared in the window.
  // area: in the world, in the village, in Asia - She is IN Oxford
  // volume: in a box, in the bathroom, in the cathedral
  {L"inside",tprIN},{L"into",tprIN},{L"in",tprIN},{L"out",tprIN},{L"within",tprIN},{L"without",tprIN},
  // in the same location or time as
  // dim 0 CGEL at the bus stop - She is AT Oxford
  {L"at",tprAT},
  // y direction (on or under) - dim 1 or 2 CGEL
  // line:
  // The city is situated on the River Thames.
  // The city is on the coast.
  // surface:
  // A notice was pasted on the wall.
  // The frost made patterns on the window.
  {L"above",tprY},{L"below",tprY},{L"atop",tprY},{L"beneath",tprY},{L"up",tprY},{L"down",tprY},{L"over",tprY},{L"under",tprY},{L"onto",tprY},{L"upon",tprY},
  {L"astride",tprY},{L"aboard",tprY},{L"on",tprY},{L"off",tprY},
  // otherwise spatially related
  {L"between",tprSPAT},{L"athwart",tprSPAT},{L"beyond",tprSPAT},{L"behind",tprSPAT},{L"past",tprSPAT},
  {L"through",tprSPAT},{L"midway",tprSPAT},{L"throughout",tprSPAT},
  // z direction (or time)
  {L"before",tprZ},{L"after",tprZ},{L"since",tprZ},{L"until",tprZ},{L"while",tprZ},{L"meanwhile",tprZ},{L"during",tprZ},
  // MATH
  {L"plus",tprMATH},{L"times",tprMATH},{L"minus",tprMATH},{L"less",tprMATH},{L"mod",tprMATH},{L"modulo",tprMATH},
  // LOGIC
  {L"but",tprLOGIC},{L"like",tprLOGIC},{L"unlike",tprLOGIC},{L"only",tprLOGIC},{L"except",tprLOGIC},{L"than",tprLOGIC},{L"or",tprLOGIC},{L"then",tprLOGIC},
  // tprFROM
  {L"from",tprFROM},{L"ex",tprFROM}, // from or without a starting point or source
  {L"to",tprTO}, {L"towards",tprTO}, // towards but not including
  {L"of",tprOF}, //        significantly linked modifier
  {L"as",tprAS}, //        positively related to
  {L"for",tprFOR}, //       serve the needs of OR concerning
  {L"per",tprPER}, //
  {L"against",tprVS}, {L"versus",tprVS}, //   against or in contrast to
  {NULL,-1}
};

void WordClass::addTimeFlag(int flag,Inflections words[])
{ LFS
	for (int I=0; words[I].word[0]; I++)
	{
		tIWMM iWord=query(words[I].word);
		if (iWord==end())
			if (parseWord(NULL,words[I].word,iWord,false,false, -1)<0)
				lplog(LOG_FATAL_ERROR,L"Error getting forms for time word %s",words[I].word);
		iWord->second.timeFlags|=flag;
    iWord->second.flags|=tFI::updateMainInfo;
	}
}

void WordClass::usageCostToNoun(Inflections words[],wchar_t *nounSubclass)
{ LFS
	int nounSubclassForm=FormsClass::gFindForm(nounSubclass);
	for (int I=0; words[I].word[0]; I++)
	{
		tIWMM iWord=query(words[I].word);
		if (iWord==wNULL) continue;
		iWord->second.costEquivalentSubClass(nounSubclassForm,nounForm);
	}
}

bool tFI::costEquivalentSubClass(int subclassForm,int parentForm)
{ LFS
	int subclassFormNum=query(subclassForm);
	if (subclassFormNum<0) return false;
	int parentformNum=query(parentForm);
	if (parentformNum<0) return false;
	usagePatterns[subclassFormNum]=usagePatterns[parentformNum];
	usageCosts[subclassFormNum]=usageCosts[parentformNum];
	return true;
}

void WordClass::addTimeFlag(int flag,wchar_t *words[])
{ LFS
	for (int I=0; words[I]!=NULL; I++)
	{
		if (words[I][0])
		{
			tIWMM iWord = query(words[I]);
			if (iWord == end())
				if (parseWord(NULL, words[I], iWord, false, false, -1) < 0)
					lplog(LOG_FATAL_ERROR, L"Error getting forms for time word %s", words[I]);
			iWord->second.timeFlags |= flag;
		}
	}
}

void WordClass::usageCostToNoun(wchar_t *words[],wchar_t *nounSubclass)
{ LFS
	int nounSubclassForm=FormsClass::gFindForm(nounSubclass);
	for (int I=0; words[I]; I++)
	{
		tIWMM iWord=query(words[I]);
		if (iWord==wNULL) continue;
		iWord->second.costEquivalentSubClass(nounSubclassForm,nounForm);
	}
}

wstring senseString(wstring &s,int verbSense)
{ LFS
	if (verbSense==-1) return s=L"-1";
	if ((verbSense&VT_TENSE_MASK)==VT_PRESENT) s=L"present ";
	else if ((verbSense&VT_TENSE_MASK)==VT_PRESENT_PERFECT) s=L"present perfect ";
	else if ((verbSense&VT_TENSE_MASK)==VT_PAST) s=L"past ";
	else if ((verbSense&VT_TENSE_MASK)==VT_PAST_PERFECT) s=L"past perfect ";
	else if ((verbSense&VT_TENSE_MASK)==VT_FUTURE) s=L"future ";
	else if ((verbSense&VT_TENSE_MASK)==VT_FUTURE_PERFECT) s=L"future perfect ";
	else s=L"Illegal time sense ";

	if (verbSense&VT_POSSIBLE) s+=L"POS ";
	if (verbSense&VT_PASSIVE) s+=L"PAS ";
	if (verbSense&VT_EXTENDED) s+=L"EXT ";
	if (verbSense&VT_VERB_CLAUSE) s+=L"VC ";
	if (verbSense&VT_NEGATION) s+=L"NEG ";
	return s;
};

bool Source::ageTransition(int where,bool timeTransition,bool &transitionSinceEOS,int duplicateFromWhere,int exceptWhere,vector <int> &lastSubjects,wchar_t *fromWhere)
{ LFS
	if (duplicateFromWhere>=0 && transitionSinceEOS)
		return false;
	transitionSinceEOS=true;
	wstring tmpstr,sRole;
	bool inPrimaryQuote=(m[where].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0;
	bool inSecondaryQuote=(m[where].objectRole&IN_SECONDARY_QUOTE_ROLE)!=0;
	if (inPrimaryQuote && lastOpeningPrimaryQuote>=0 && (m[lastOpeningPrimaryQuote].flags&WordMatch::flagEmbeddedStoryResolveSpeakers))
		return false;  // don't age out any speakers
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d %s aging - %s %s transition %s%s",where,section,
				(inPrimaryQuote) ? L"PQ":(inSecondaryQuote) ? L"SQ":L"NQ",fromWhere,
				(timeTransition) ? L"time":L"space",
				whereString((exceptWhere>=0) ? exceptWhere : where,tmpstr,false).c_str(),m[(exceptWhere>=0) ? exceptWhere : where].roleString(sRole).c_str());
	int so=(m[where].objectMatches.size()>0) ? m[where].objectMatches[0].object : m[where].getObject();
	vector <cObject>::iterator object=objects.begin()+so;
	cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || (inPrimaryQuote && !(m[where].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE))),(so>=0) ? (object->neuter && !(object->male || object->female)) : false,objectToBeMatchedInQuote,quoteIndependentAge);
	for (vector <cLocalFocus>::iterator lsi=localObjects.begin(); lsi!=localObjects.end(); )
		if (((!inPrimaryQuote && !inSecondaryQuote) || lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge)) && (exceptWhere<0 || !in(lsi->om.object,exceptWhere)))
		{
			if (lsi->occurredOutsidePrimaryQuote && lsi->physicallyPresent && inPrimaryQuote)
			{
				lsi++;
				continue;
			}
			bool physicallyEvaluatedLW=false,ppLW=(lsi->lastWhere>=0     && physicallyPresentPosition(lsi->lastWhere    ,physicallyEvaluatedLW) && physicallyEvaluatedLW);
			if (ppLW && m[lsi->lastWhere].objectMatches.size()>1)
				ppLW=false;
			bool physicallyEvaluatedPW=false,ppPW=(lsi->previousWhere>=0 && physicallyPresentPosition(lsi->previousWhere,physicallyEvaluatedPW) && physicallyEvaluatedPW);
			if (ppPW && m[lsi->previousWhere].objectMatches.size()>1)
				ppPW=false;
			if (lsi->physicallyPresent && ((lsi->whereBecamePhysicallyPresent>where) || (ppLW && lsi->lastWhere>where) || (ppPW && lsi->previousWhere>where))) // DEBUG TSG
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG|LOG_RESOLUTION,L"%06d:Skipped making %s not present, as became present afterward [%s][%s][lastWhere %d %s][previousWhere %d %s]",where,objectString(lsi->om,tmpstr,true).c_str(),
							(lsi->occurredInPrimaryQuote) ? L"PRIM":L"",(lsi->occurredOutsidePrimaryQuote) ? L"OUTSIDE":L"",
							lsi->lastWhere,    (physicallyEvaluatedLW) ? L"PP" : L"PP",
							lsi->previousWhere,(physicallyEvaluatedPW) ? L"PP" : L"PP");
				lsi++;
				continue;
			}
			if (lsi->physicallyPresent && debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:Made %s not present 4 [%s][%s][lastWhere %d %s][previousWhere %d %s]",where,objectString(lsi->om,tmpstr,true).c_str(),
							(lsi->occurredInPrimaryQuote) ? L"PRIM":L"",(lsi->occurredOutsidePrimaryQuote) ? L"OUTSIDE":L"",
							lsi->lastWhere,    (physicallyEvaluatedLW) ? L"PP" : L"PP",
							lsi->previousWhere,(physicallyEvaluatedPW) ? L"PP" : L"PP");
			if (lsi->physicallyPresent)
			{
				lsi->physicallyPresent=false;
				lsi->lastExit=where;
			}
		  if (!inPrimaryQuote)
			{
				// if tempSpeakerGroup begins before the transition, remove from tempSpeakerGroup
				// also remove from previousLastSpeakers, lastSubjects & previousLastSubjects, and nextNarrationSubjects
				if (tempSpeakerGroup.sgBegin>where)
					tempSpeakerGroup.removeSpeaker(lsi->om.object);
				for (vector <int>::iterator si=nextNarrationSubjects.begin(),siEnd=nextNarrationSubjects.end(); si!=siEnd; si++)
					if (*si==lsi->om.object)
					{
						nextNarrationSubjects.erase(si);
						break;
					}
				for (vector <int>::iterator si=lastSubjects.begin(),siEnd=lastSubjects.end(); si!=siEnd; si++)
					if (*si==lsi->om.object)
					{
						lastSubjects.erase(si);
						break;
					}
			}
			ageSpeaker(where,lsi,EOS_AGE); // can remove object from localObjects completely
		}
		else
			lsi++;
	return true;
}

bool Source::rejectTimeWord(int where,int begin)
{ LFS
		wstring moment=m[where].getMainEntry()->first;
		// second or minute doesn't work because the assumption of everyone being somewhere else is not necessarily true
		wchar_t *momentUnits[] = {L"second",L"minute",L"moment",L"flash",NULL}; // see simultaneous units
		for (int I=0; momentUnits[I]; I++) 
			if (moment==momentUnits[I])
				return true;
		// reject - this time / this morning - 'this' means it is already here; there is no transition
		// all the time it was a bluff!
		// each day saw them set out...
		// any time he chose
		wstring detTime=m[begin].word->first;
		wchar_t *anyTime[] = {L"this",L"all",L"each",L"any",NULL}; 
		for (int I=0; anyTime[I]; I++) 
			if (detTime==anyTime[I])
				return true;
		if (moment==L"time" && m[where-1].queryWinnerForm(determinerForm)>=0)
			return true;
		// five times better
		if (where+1<(int)m.size() && m[where].word->first==L"times" && m[where-1].queryWinnerForm(numeralCardinalForm)>=0 && 
			  m[where+1].queryWinnerForm(adjectiveForm)>=0 && (m[where+1].word->second.inflectionFlags&ADJECTIVE_COMPARATIVE)!=0)
			return true;
		if ((moment==L"day" || moment==L"time") && m[where].beginObjectPosition==m[where].endObjectPosition-1)
			return true;
		if (m[where].beginObjectPosition==m[where].endObjectPosition && where>2 && m[where-1].word->first!=L"-" && !(m[where].flags&WordMatch::flagFirstLetterCapitalized))
			return true;
		return false;
}

bool Source::resolveTimeRange(int where,int pmaOffset,vector <cSpaceRelation>::iterator csr)
{ LFS
	vector <int> objectPositions;
	vector < vector <tTagLocation> > mobjectTagSets;
	if (startCollectTags(true,mobjectTagSet,where,m[where].pma[pmaOffset].pemaByPatternEnd,mobjectTagSets,true,false)>0)
		for (unsigned int J=0; J<mobjectTagSets.size(); J++)
		{
			//if (t.traceSpeakerResolution)
			//	::printTagSet(LOG_TIME,L"MOBJECT",J,mobjectTagSets[J]);
			for (int oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",-1); oTag>=0; oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",oTag)) 
				if (mobjectTagSets[J][oTag].PEMAOffset<0)
					objectPositions.push_back((m[mobjectTagSets[J][oTag].sourcePosition].principalWherePosition>=0) ? m[mobjectTagSets[J][oTag].sourcePosition].principalWherePosition : mobjectTagSets[J][oTag].sourcePosition);
			if (objectPositions.size()==2)
			{
				int maxLen;
				if (!identifyDateTime(objectPositions[0],csr,maxLen,1)) return false;
				if (identifyDateTime(objectPositions[1],csr,maxLen,2))
				{
					if (m[objectPositions[0]].endObjectPosition>=0 && m[m[objectPositions[0]].endObjectPosition].word->first==L"and")
						csr->timeInfo[csr->timeInfo.size()-1].timeRelationType=T_RANGE;
					markTime(objectPositions[0],m[objectPositions[0]].beginObjectPosition,m[objectPositions[0]].endObjectPosition-m[objectPositions[0]].beginObjectPosition);
					return true;
				}
				else // erase previous time if empty()
				{
					vector <cTimeInfo>::iterator cti=csr->timeInfo.begin()+csr->timeInfo.size()-1;
					if (!cti->timeRelationType && (cti->timeCapacity==cUnspecified || cti->empty())) 
					{
						wstring tmpstr;
						if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d: time relation rejected: %s",cti->tWhere,cti->toString(m,tmpstr).c_str());
						csr->timeInfo.erase(cti);
					}
				}
			}
		}
	return false;
}

bool Source::stopSearch(int I)
{ LFS
	return isEOS(I) || m[I].word==Words.sectionWord || ((m[I].word->first==L"“" || m[I].word->first==L"”") && !(m[I].flags&WordMatch::flagQuotedString));
}

// the previous relation has already taken everything in the sentence.
// if there is no conjunction between them, simply copy all time expressions.
// if there is a conjunction between them, then move all time expressions after the current space relation
//    from the previous time relation to the current one.
void Source::distributeTimeRelations(vector <cSpaceRelation>::iterator csr,vector <cSpaceRelation>::iterator previousRelation,int conjunctionPassed)
{ LFS
	if (conjunctionPassed==-1)
	{
		csr->timeInfo=previousRelation->timeInfo;
		csr->tft.timeTransition|=previousRelation->tft.timeTransition;
		csr->tft.duplicateTimeTransitionFromWhere=previousRelation->where;
		return;
	}
	bool gainedTransition=false;
	for (vector <cTimeInfo>::iterator I=previousRelation->timeInfo.begin(); I!=previousRelation->timeInfo.end(); )
	{
		if ((conjunctionPassed<I->tWhere && m[I->tWhere].pma.queryPattern(L"_INTRO_S1")!=-1) ||
	      I->tWhere>csr->where)
		{
			csr->timeInfo.push_back(*I);
			bool saveTransition=csr->tft.timeTransition;
			if (I->timeRelationType!=T_MODIFIER)
			detectTimeTransition(I->tWhere,csr,*I);
			if (!saveTransition && csr->tft.timeTransition)
				gainedTransition=true;
			previousRelation->timeInfo.erase(I);
		}
		else 
			I++;
	}
	if (previousRelation->timeInfo.empty() && gainedTransition)
		previousRelation->tft.timeTransition=false;
}

void Source::appendTime(vector <cSpaceRelation>::iterator csr)
{ LFS
	if (csr->timeInfoSet) return;
	int maxLen=-1;
	if (csr->whereSubject>=0 && m[csr->whereSubject].getObject()>=0)
	{
		if (csr->relationType!=stABSTIME && m[csr->whereSubject].relPrep<0 && objects[m[csr->whereSubject].getObject()].isTimeObject)
		{
			wstring tmpstr;
			if (debugTrace.traceTime)
				lplog(LOG_TIME,L"%06d:Shifted timeRelation with subject %s to stABSTIME.",csr->where,whereString(csr->whereSubject,tmpstr,false).c_str());
			csr->relationType=stABSTIME;
			csr->tft.timeTransition=true;
		}
		if (objects[m[csr->whereSubject].getObject()].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS ||
				objects[m[csr->whereSubject].getObject()].isTimeObject)
			identifyDateTime(csr->whereSubject,csr,maxLen,0);
	}
  if (csr->relationType==stADVERBTIME)
	{
		identifyDateTime(csr->where,csr,maxLen,0);
		return;			
	}
	if (csr->whereVerb<0) return;
	int wherePrep=m[csr->whereVerb].relPrep,prepLoop=0,progression=-1;
	if (csr->relationType==stABSTIME)
		progression=0;
	if (m[csr->whereVerb].word->second.flags&tFI::stateVerb)
		progression=0;
	else if (m[csr->whereVerb].word->second.flags&tFI::possibleStateVerb)
		progression=1;
	else
		progression=2;
	if (csr->tft.presentHappening)
	{
		progression=(m[csr->whereVerb].quoteForwardLink&VT_EXTENDED) ? 0 : 2;
		if (progression==2 && 
			(isVerbClass(csr->whereVerb,L"am") || csr->relationType==stCONTACT))
			progression=0;
	}
	// 0 - advances a action based amount of time from the last statement / He brushed his teeth.   [ +15 minutes? ]
	// 1 - still keeping the current time flow but not advancing the action / He was fat / He preferred the ham.
	// 2 - establishes a new time not related to the previous time flow / At Christmas, they opened fifty presents - each.
	// 3 - establishes a new time relative to the current time flow / after two weeks, he was tired. / He ran for two hours.
	// 4 - an action that describes something happening up to and including the moment ; not advancing time / He was running past the stop sign. / He was driving his new car.
	// 5 - an action that describes something that happened in the past ; not advancing time / He had wandered down that road many times
	// 6 - an action that describes something that will happen in the future ; not advancing time / She will pay for this!
	// 7 - an action that may have happened in the past ; not advancing time / I may have misplaced my wallet
	// 8 - an action that may happen in the future ; not advancing time / I might run for office
	// 9 - an action that describes recurring action ; not advancing the current time flow / I always visit her / I drive the van every Monday / She plays soccer twice a week.
	csr->timeProgression=progression;
	bool directlyFollowingLinked=false;
	for (; wherePrep>=0; wherePrep=m[wherePrep].relPrep)
	{
		if (prepLoop++>30)
		{
			wstring tmpstr;
			lplog(LOG_ERROR,L"%06d:Prep loop occurred (8) %s.",wherePrep,loopString(wherePrep,tmpstr));
			return;
		}
		// John ran in May.
		if (identifyDateTime(m[wherePrep].getRelObject(),csr,maxLen,0))
		{
			if (directlyFollowingLinked && (m[wherePrep].word->first==L"to" ||  m[wherePrep].word->first==L"through"))
				csr->timeInfo[csr->timeInfo.size()-1].timeRelationType=T_RANGE;
			directlyFollowingLinked=true;
		}
	} 
	// He departs this weekend for Baghdad.
	maxLen=-1;
	if (csr->whereObject>=0)
	{
		int beginObjectPosition=m[csr->whereObject].beginObjectPosition,endObjectPosition=m[csr->whereObject].endObjectPosition;
		if ((beginObjectPosition>=0 && m[beginObjectPosition].pma.queryPattern(L"__CLOSING__S1",maxLen)==-1 || maxLen!=(endObjectPosition-beginObjectPosition)))
		{
			if (identifyDateTime(csr->whereObject,csr,maxLen,0) && csr->relationType==stBE && m[csr->where].word->first==L"it")
			{
				int whereTime=m[csr->whereObject].beginObjectPosition;
				if (m[whereTime].pma.queryPattern(L"_DATE")!=-1)
					csr->relationType=stABSDATE;
				else if (m[whereTime].pma.queryPattern(L"_TIME")!=-1)
					csr->relationType=stABSTIME;
			}
		}
	}
	if (csr->relationType!=stABSTIME)
	{
		// _INTRO_S1, __CLOSING__S1 OR _ADVERB
		unsigned int I=csr->where;
		if (csr->whereSubject<(signed)I && csr->whereSubject!=-1)
		{
			I=csr->whereSubject;
			if (m[I].spaceRelation) I++; // make sure a previous space relation with the same subject isn't skipped, for time relation movement
		}
		// go back to last EOS or last beginning quote.
		int conjunctionPassed=-1;
		for (I=(I>0) ? I-1:I; !stopSearch(I) && I>0; I--) 
		{
			if (m[I].spaceRelation)
			{
				vector <cSpaceRelation>::iterator previousRelation;
				if ((previousRelation=findSpaceRelation(I))!=spaceRelations.end() && csr!=previousRelation)
				{
					distributeTimeRelations(csr,previousRelation,conjunctionPassed);
					break;
				}
			}
			if (m[I].queryWinnerForm(conjunctionForm)!=-1 || m[I].queryWinnerForm(coordinatorForm)!=-1)
				conjunctionPassed=I;
		}
		int maxEnd=-1,element;
		for (I++; I<m.size() && !stopSearch(I); I++)
		{
			// The company he had invested in went bankrupt within minutes [after] the stock market closed for the day.
			// We were still talking about work three hours [after] the meeting broke up.
			if ((element=m[I].queryWinnerForm(conjunctionForm))!=-1 && m[I].word->second.timeFlags && debugTrace.traceTime)
				lplog(LOG_TIME,L"%06d:conjunction!",I);
			if (m[I].flags&WordMatch::flagAlreadyTimeAnalyzed)
				continue;
			if ((maxEnd==-1 || I>=(unsigned)maxEnd) && m[I].relPrep<0)
			{
				if ((element=pema.queryTag(m[I].beginPEMAPosition,FLOAT_TIME_TAG))>=0)
					maxEnd=I+pema[element].getChildLen();
				else if ((element=m[I].pma.queryTag(FLOAT_TIME_TAG))>=0)
					maxEnd=I+m[I].pma[element].len;
			}
			if (maxEnd!=-1 && I<(unsigned)maxEnd && identifyDateTime(I,csr,maxEnd,0) && maxEnd>0)
			{
				//int pattern=pema[element].getPattern();
				//lplog(LOG_TIME,L"%d:%s[%s](%d,%d)",csr->where,
				//	patterns[pattern]->name.c_str(),patterns[pattern]->differentiator.c_str(),I,I+pema[element].end);
			  I+=maxEnd-1;
			}
		}
	}
	csr->timeInfoSet=true;
}

// will change source.m (invalidate all iterators through the use of newSR)
void Source::detectTimeTransition(int where,vector <int> &lastSubjects)
{ LFS
	int timeType=-1,whereTime=-1;
	bool timeTransition=false;
	int wt=where,wot=-1;
	if ((m[wt].objectRole&(SUBJECT_ROLE|OBJECT_ROLE))!=SUBJECT_ROLE)
		return;
	if (m[wt].principalWherePosition>=0) wt=m[wt].principalWherePosition;
	if (m[wt].objectMatches.size()) wt=objects[m[wt].objectMatches[0].object].originalLocation;
	if ((m[wt].objectRole&(SUBJECT_ROLE|OBJECT_ROLE))!=SUBJECT_ROLE)
		return;
	if (((m[wt].getObject()>=0 && (m[wt].word->second.timeFlags&T_UNIT)) || 
			((m[where].objectRole&IS_OBJECT_ROLE) && (wot=m[where].getRelObject())>=0 && m[wot].getObject()>=0 && (m[wot].word->second.timeFlags&T_UNIT))))
	{
		if (wot>=0) wt=wot;
		int findSubject=wt;
		if ((m[wt].objectRole&SUBJECT_ROLE) && m[wt].relVerb<0)
		{
			for (; findSubject<(signed)m.size() && (m[findSubject].objectRole&SUBJECT_ROLE) && m[findSubject].relVerb<0; findSubject++);
			if (!(m[findSubject].objectRole&SUBJECT_ROLE) || m[findSubject].relVerb<0)
				findSubject=wt;
		}
		int tense=-1;
		if ((m[findSubject].objectRole&SUBJECT_ROLE) && (m[findSubject].relVerb<0 || // about an hour had passed
			   ((tense=m[m[findSubject].relVerb].quoteForwardLink&(VT_TENSE_MASK|VT_POSSIBLE))!=VT_PAST) && (tense!=VT_PAST_PERFECT)))
			return;
		if (tense==VT_PAST_PERFECT && debugTrace.traceTime)
			lplog(LOG_RESOLUTION,L"%06d:VT_PAST_PERFECT time subject passed.",findSubject);
		if (rejectTimeWord(wt,m[wt].beginObjectPosition)) return;
		// Sunday was her afternoon out.
		// Night and day were the same in this prison room.
		// Three hours were more than enough for Mr. Brown.
		// The first time was in Italy.
		if ((m[wt].objectRole&(IS_OBJECT_ROLE|SUBJECT_ROLE))==(IS_OBJECT_ROLE|SUBJECT_ROLE))
		{
			// Today was the 27th. - this is a meta-expression for time
			if (m[wt].getRelObject()>=0 && m[m[wt].getRelObject()].queryWinnerForm(numeralOrdinalForm)>=0)
			{
				timeType=stSUBJDAYOFMONTHTIME;
				objects[m[m[wt].getRelObject()].getObject()].isTimeObject=true;
			}
			else 
			{
				if (m[wt].getObject()>=0)
					objects[m[wt].getObject()].isTimeObject=true;
				m[wt].timeColor=T_UNIT;
				return;
			}
		}
		else
		{
			// this morning post
			// the 1994 crisis
			if (findSubject!=wt && m[findSubject].beginObjectPosition<=wt)
			{
				// that evening Tommy sat on the bed.
				//int tmp=m[findSubject].getObject(),tmp2=objects[m[findSubject].getObject()].PISDefinite;
				if (findSubject!=wt+1 || m[findSubject].getObject()<0 || objects[m[findSubject].getObject()].PISDefinite<=0)
				{
					if (debugTrace.traceTime)
					lplog(L"%06d:enhanced time transition trueSubject=%d rejected (adjective).",where,findSubject);
					return;
				}
				else if (debugTrace.traceTime)
					lplog(L"%06d:enhanced time transition trueSubject=%d (kept).",where,findSubject);
			}
			timeType=stADVERBTIME;
			if (m[where].pma.queryPattern(L"_INTRO_S1")!=-1)
			{
				if (debugTrace.traceTime)
				lplog(L"%06d:intro time transition rejected (will be picked up later).",where);
				return;
			}
			// the day/the evening etc
			if (wt>2 && (m[wt-1].word->first!=L"-" || (m[wt-2].word->first!=L"lunch" && m[wt-2].word->first!=L"dinner")) && m[wt-1].queryWinnerForm(determinerForm)<0)
				timeTransition=true;
		}
		if (findSubject!=wt && debugTrace.traceTime)
			lplog(L"%06d:enhanced time transition trueSubject=%d.",where,findSubject);
		where=wt;
		if (m[where].getObject()>=0)
		{
			objects[m[where].getObject()].isTimeObject=true;
			wstring tmpstr;
			if (debugTrace.traceTime)
				lplog(LOG_RESOLUTION,L"%06d:Marking %s as timeObject",where,objectString(m[where].getObject(),tmpstr,false).c_str());
		}
	}
	// A neighbouring clock showed the time to be five minutes to twelve . 
	// A neighbouring clock showed 5 o'clock . 
	else if (m[wt].relVerb>=0 && isVerbClass(m[wt].relVerb,L"indicate") && 
					 (m[m[wt].relVerb].quoteForwardLink&(VT_TENSE_MASK|VT_POSSIBLE))==VT_PAST && m[wt].getRelObject()>=0 && m[m[wt].getRelObject()].word->first==L"time")
	{
		int saveWhere=-1;
		if (m[m[wt].relVerb].relVerb>=0 && m[m[m[wt].relVerb].relVerb].word->first==L"be" && m[m[m[wt].relVerb].relVerb].getRelObject()>=0 &&
				m[m[saveWhere=m[m[m[wt].relVerb].relVerb].getRelObject()].beginObjectPosition].pma.queryPattern(L"_TIME")!=-1)
			timeType=stABSTIME;
		else if (m[wt].getRelObject()>=0 && m[m[saveWhere=m[wt].getRelObject()].beginObjectPosition].pma.queryPattern(L"_TIME")!=-1)
			timeType=stABSTIME;
		if (timeType>=0)
		{
			where=whereTime=saveWhere;
			objects[m[where].getObject()].isTimeObject=true;
		}
	}
	else if (m[where].pma.queryPattern(L"_TIME")!=-1 && (m[where].objectRole&SUBJECT_PLEONASTIC_ROLE))
	{
		whereTime=where;
		timeType=stABSTIME;
		if (m[where].getObject()>=0)
			objects[m[where].getObject()].isTimeObject=true;
	}
	// IT was May 7 , 1915
	else if (m[where].pma.queryPattern(L"_DATE")!=-1 && (m[where].objectRole&SUBJECT_PLEONASTIC_ROLE))
	{
		whereTime=where;
		timeType=stABSDATE;
		if (m[where].getObject()>=0)
			objects[m[where].getObject()].isTimeObject=true;
	}
	if (timeType>=0)
	{
		bool transitionSinceEOS=false; // transitionSinceEOS only applies to transitions with timeInfo, not the relation itself (stABSTIME)
		if (timeTransition)
			ageTransition(where,true,transitionSinceEOS,-1,-1,lastSubjects,L"TSR 2"); 
		newSR(where,-1,-1,where,m[where].relVerb,-1,m[where].getRelObject(),-1,-1,timeType,L"time",true);
		if (m[where].getObject()>=0 && !m[where].timeColor)
			m[where].timeColor=T_UNIT;
		vector <cSpaceRelation>::iterator sr;
		if ((sr=findSpaceRelation(where))!=spaceRelations.end() && sr->where==where)
		{
			sr->tft.timeTransition|=timeTransition;
			int offset=(int) (sr-spaceRelations.begin());
			wstring description=L"detectTimeTransition";
			if (debugTrace.traceTime)
				lplog(LOG_RESOLUTION,L"%06d:time transition %s (%s)",where,srToText(offset,description).c_str(),(sr->tft.timeTransition) ? L"true":L"false");
		}
	}
}

int Source::getVerbTense(vector <tTagLocation> &tagSet,int verbTagIndex,bool &isId)
{ LFS
	if (!tagSetTimeArraySize)
		tagSetTimeArray=(char *)tmalloc(tagSetTimeArraySize=desiredTagSets[verbSenseTagSet].tags.size());
	else if (tagSetTimeArraySize<desiredTagSets[verbSenseTagSet].tags.size())
	{
		unsigned int previousSize=tagSetTimeArraySize;
		tagSetTimeArray=(char *)trealloc(20,tagSetTimeArray,previousSize,tagSetTimeArraySize=desiredTagSets[verbSenseTagSet].tags.size());
	}
	memset(tagSetTimeArray,0,desiredTagSets[verbSenseTagSet].tags.size());
	if (verbTagIndex<0)
		findTagSet(tagSet,verbSenseTagSet,tagSetTimeArray);
	else
		findTagSetConstrained(tagSet,verbSenseTagSet,tagSetTimeArray,tagSet[verbTagIndex]);
	int tense=0;
	// 0    1       2     3      4      5        6      7           8    9     10  11   12   13    14    15     16    17    18     19     20     21      22   23     24    25    26    27
	// "no","never","not","past","imp","future","id","conditional","vS","vAC","vC","vD","vB","vAB","vBC","vABC","vCD","vBD","vABD","vACD","vBCD","vABCD","vE","vrBD","vrB","vrBC","vrD","vAD"
	enum constVerbTag {
		vt_no,vt_never,vt_not,vt_past,vt_imp,vt_future,vt_id,vt_conditional,vt_vS,vt_vAC,vt_vC,vt_vD,vt_vB,vt_vAB,vt_vBC,vt_vABC,vt_vCD,vt_vBD,
		vt_vABD,vt_vACD,vt_vBCD,vt_vABCD,vt_vE,vt_vrBD,vt_vrB,vt_vrBC,vt_vrD,vt_vAD
	};
	isId=tagSetTimeArray[vt_id]!=0;
	// SIMPLE
	if (tagSetTimeArray[vt_vS]) tense=VT_PRESENT;
	if (tagSetTimeArray[vt_past]) tense=VT_PAST;
	if (tagSetTimeArray[vt_vB]) tense=VT_PRESENT_PERFECT;
	if (tagSetTimeArray[vt_vrB]) tense=VT_PRESENT_PERFECT+VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vB] && tagSetTimeArray[vt_past]) tense=VT_PAST_PERFECT;
	if (tagSetTimeArray[vt_vS] && tagSetTimeArray[vt_future]) tense=VT_FUTURE;
	if (tagSetTimeArray[vt_vAB] && tagSetTimeArray[vt_future]) tense=VT_FUTURE_PERFECT;
	// EXTENDED
	if (tagSetTimeArray[vt_vC]) tense=VT_EXTENDED+ VT_PRESENT;
	if (tagSetTimeArray[vt_vE]) tense=VT_EXTENDED+ VT_PRESENT+VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vC] && tagSetTimeArray[vt_past]) tense=VT_EXTENDED+ VT_PAST;
	if (tagSetTimeArray[vt_vBC]) tense=VT_EXTENDED+ VT_PRESENT_PERFECT;
	if (tagSetTimeArray[vt_vrBC]) tense=VT_EXTENDED+ VT_PRESENT_PERFECT+VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vBC] && tagSetTimeArray[vt_past]) tense=VT_EXTENDED+ VT_PAST_PERFECT;
	if (tagSetTimeArray[vt_vAC] && tagSetTimeArray[vt_future]) tense=VT_EXTENDED+ VT_FUTURE;
	if (tagSetTimeArray[vt_vABC] && tagSetTimeArray[vt_future]) tense=VT_EXTENDED+ VT_FUTURE_PERFECT;
	// PASSIVE
	if (tagSetTimeArray[vt_vD]) tense=VT_PASSIVE+ VT_PRESENT;
	if (tagSetTimeArray[vt_vrD]) tense=VT_PASSIVE+ VT_PRESENT+VT_EXTENDED+VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vD] && tagSetTimeArray[vt_past]) tense=VT_PASSIVE+ VT_PAST;
	if (tagSetTimeArray[vt_vBD]) tense=VT_PASSIVE+ VT_PRESENT_PERFECT;
	if (tagSetTimeArray[vt_vrBD]) tense=VT_PASSIVE+ VT_PRESENT_PERFECT+VT_VERB_CLAUSE;
	if (tagSetTimeArray[vt_vAD] && tagSetTimeArray[vt_past]) tense=VT_PASSIVE+ VT_PAST; // dared be examined?
	if (tagSetTimeArray[vt_vBD] && tagSetTimeArray[vt_past]) tense=VT_PASSIVE+ VT_PAST_PERFECT;
	if (tagSetTimeArray[vt_vCD]) tense=VT_PASSIVE+ VT_PRESENT+VT_EXTENDED;
	if (tagSetTimeArray[vt_vCD] && tagSetTimeArray[vt_past]) tense=VT_PASSIVE+ VT_PAST+VT_EXTENDED;
	if (tagSetTimeArray[vt_vBCD]) tense=VT_PASSIVE+ VT_PRESENT_PERFECT+VT_EXTENDED;
	if (tagSetTimeArray[vt_vBCD] && tagSetTimeArray[vt_past]) tense=VT_PASSIVE+ VT_PAST_PERFECT+VT_EXTENDED;
	if (tagSetTimeArray[vt_vAD] && tagSetTimeArray[vt_future]) tense=VT_PASSIVE+ VT_FUTURE;
	if (tagSetTimeArray[vt_vACD] && tagSetTimeArray[vt_future]) tense=VT_PASSIVE+ VT_FUTURE+VT_EXTENDED;
	if (tagSetTimeArray[vt_vABD] && tagSetTimeArray[vt_future]) tense=VT_PASSIVE+ VT_FUTURE_PERFECT;
	if (tagSetTimeArray[vt_vABCD] && tagSetTimeArray[vt_future]) tense=VT_PASSIVE+ VT_FUTURE_PERFECT+VT_EXTENDED;
	if (tagSetTimeArray[vt_vAD] && !tense) tense=VT_PASSIVE; // dares be examined?
	// POSSIBLE
	if ((tagSetTimeArray[vt_vS] || tagSetTimeArray[vt_id]) && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PRESENT;
	if (tagSetTimeArray[vt_vS] && !tagSetTimeArray[vt_past] && tagSetTimeArray[vt_imp]) tense=VT_POSSIBLE+ VT_PRESENT;
	// did you mean that? (IMPERATIVE)
	if (tagSetTimeArray[vt_past] && tagSetTimeArray[vt_imp]) tense=VT_IMPERATIVE+ VT_PAST;
	if (tagSetTimeArray[vt_vACD] && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PRESENT+VT_PASSIVE;
	if (tagSetTimeArray[vt_vAC] && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PRESENT+VT_EXTENDED;
	if (tagSetTimeArray[vt_vAB] && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PAST;
	if (tagSetTimeArray[vt_vABD] && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PAST+VT_PASSIVE;
	if (tagSetTimeArray[vt_vABCD] && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PAST+VT_PASSIVE+VT_EXTENDED;
	if (tagSetTimeArray[vt_vAD] && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PRESENT_PERFECT+VT_PASSIVE;
	if (tagSetTimeArray[vt_vABC] && tagSetTimeArray[vt_conditional]) tense=VT_POSSIBLE+ VT_PRESENT_PERFECT+VT_EXTENDED;
	if (tagSetTimeArray[vt_conditional] && !tense) tense=VT_POSSIBLE+ VT_PRESENT;
	if (tagSetTimeArray[vt_future] && !tense) tense=VT_FUTURE;
	if (tagSetTimeArray[vt_no] || tagSetTimeArray[vt_never] || tagSetTimeArray[vt_not]) tense|=VT_NEGATION;
	if (verbTagIndex>=0)
		for (unsigned int I=tagSet[verbTagIndex].sourcePosition; I<tagSet[verbTagIndex].sourcePosition+tagSet[verbTagIndex].len; I++)
			m[I].verbSense=tense;
	if (!(tense&VT_TENSE_MASK))
		tense|=VT_PRESENT;
	if (debugTrace.traceRelations)
	{
		wstring tagString;
		for (unsigned int I=0; I<desiredTagSets[verbSenseTagSet].tags.size(); I++)
			if (tagSetTimeArray[I])
				tagString=tagString+patternTagStrings[desiredTagSets[verbSenseTagSet].tags[I]]+L" ";
		wstring tmp;
		if (verbTagIndex>=0)
			lplog(L"Set verb tags (%d to %d) found tags %s, sense=%s.",tagSet[verbTagIndex].sourcePosition,tagSet[verbTagIndex].sourcePosition+tagSet[verbTagIndex].len,tagString.c_str(),senseString(tmp,tense).c_str());
		else
			lplog(L"Set verb tags (infinitive starting at %d) found tags %s, sense=%s.",tagSet[0].sourcePosition,tagString.c_str(),senseString(tmp,tense).c_str());
	}
	return tense;
}

int Source::getSimplifiedTense(int tense)
{ LFS
	if (tense==-1) return -1;
	wstring tmp;
	int simplifiedTense=0;
	switch (tense&~(VT_POSSIBLE|VT_PASSIVE|VT_NEGATION|VT_EXTENDED|VT_IMPERATIVE))
	{
	case VT_PRESENT: simplifiedTense=0; break;
	case VT_PRESENT_PERFECT: simplifiedTense=1; break;
	case VT_PAST: simplifiedTense=2; break;
	case VT_PAST_PERFECT: simplifiedTense=3; break;
	case VT_FUTURE: simplifiedTense=4; break;
	case VT_FUTURE_PERFECT: simplifiedTense=5; break;
	case VT_PRESENT_PERFECT+VT_VERB_CLAUSE: simplifiedTense=6; break;
	case VT_PRESENT+VT_VERB_CLAUSE: simplifiedTense=7; break;
	default:
		lplog(LOG_FATAL_ERROR,L"getSimplifiedTense:Unhandled verb tense %s %d.",senseString(tmp,tense).c_str(),tense);
	}
	bool extended=(tense&VT_EXTENDED)==VT_EXTENDED;
	if (extended) simplifiedTense+=8;
	return simplifiedTense;
}

/*
http://www.wwnorton.com/write/waor.htm
The subjunctive mood is for statements of hypothetical conditions or of wishes, recommendations, requirements, or suggestions. To express the
subjunctive, you often need one of the modal auxiliaries, which include can, could, may, might, must, ought, should, and would. Use them as follows:
1. USE CAN TO EXPRESS
CAPABILITY: Can the Israelis and the Palestinians ever make peace?
PERMISSION: Why can’t first-year college students live off campus?
In formal writing, permission is normally signified by may rather than can,
which is reserved for capability. But can may be used informally to express
permission and is actually better than may in requests for permission involving
the negative. The only alternative to can’t in such questions is the
awkward term mayn’t.
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


	Both would and could may be used to express the object of a wish. But “I wish you could go” means “I wish you were able to go”; “I wish you would
go” means “I wish you were willing to go.”

• Proper name (unique identifier for temporally-defined event): Monday, January, New Year’s Eve, Washington’s birthday
• Number: 3 (as in “He arrived at 3.”), three

We globally refer to names of festivals, holidays and other occasions of religious observance, remembrance of famous massacres, etc. as “holidays”. Some of these expressions, like “Shrove Tuesday” and “Thanksgiving Day” contain trigger words. Others, like “Thanksgiving”, “Christmas”, and “Diwali”, do not.
A tagger is allowed to tag any holiday it wants (sorry, there is NO fixed list of holidays!), but is to assign it a value only when that value can be inferred from the local and global context of the text, rather than from cultural and world knowledge. For example, given “Christmas is celebrated in December”, the value of December is assigned to Christmas, but given only “Christmas left me poor”, “Christmas” is to be tagged without a value.

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
	Inflections months[] = {{L"january",SINGULAR},{L"february",SINGULAR},{L"march",SINGULAR},{L"april",SINGULAR},{L"may",SINGULAR},
	{L"june",SINGULAR},{L"july",SINGULAR},{L"august",SINGULAR},{L"september",SINGULAR},{L"october",SINGULAR},
	{L"november",SINGULAR},{L"december",SINGULAR},{NULL,0}};
	wchar_t *months_abb[] = {L"jan",L"feb",L"mar",L"apr",L"aug",L"sept",L"oct",L"nov",L"dec",NULL};
int whichMonth(wstring w)
{ LFS
	for (int I=0; months[I].inflection; I++)
		if (w==months[I].word)
			return I;
	for (int I=0; months_abb[I]; I++)
		if (w==months_abb[I])
			return I;
	return -1;
}

	wchar_t *daysOfWeek_abb[] = {L"sun",L"mon",L"tues",L"wed",L"thurs",L"fri",L"sat",NULL};
	Inflections daysOfWeek[] = {{L"sunday",SINGULAR},{L"monday",SINGULAR},{L"tuesday",SINGULAR},{L"wednesday",SINGULAR},
			{L"thursday",SINGULAR},{L"friday",SINGULAR},{L"saturday",SINGULAR},{L"weekend",SINGULAR},
	{L"sundays",PLURAL},{L"mondays",PLURAL},{L"tuesdays",PLURAL},{L"wednesdays",PLURAL},
			{L"thursdays",PLURAL},{L"fridays",PLURAL},{L"saturdays",PLURAL},{L"weekends",PLURAL},{NULL,0}};
int whichDayOfWeek(wstring w)
{ LFS
	for (int I=0; daysOfWeek[I].inflection; I++)
		if (w==daysOfWeek[I].word)
			return I;
	for (int I=0; daysOfWeek_abb[I]; I++)
		if (w==daysOfWeek_abb[I])
			return I;
	return -1;
}

wchar_t *twr_ara[]={L"hourly",L"daily",L"weekly",L"monthly",L"quarterly",L"seasonally",L"yearly",L"annual",L"twice",L"thrice",L"once",L"times",L"every",L"each",L"",NULL};
int recurrence_flags[]={cHour, cDay, cWeek, cMonth, cQuarter, cSeason, cYear, cYear, -2, -3, -4, -5, -6, -7, cUnspecified,0 };
int whichRecurrence(wstring w)
{ LFS
	for (int I=0; twr_ara[I]; I++)
		if (w==twr_ara[I])
			return recurrence_flags[I];
	return cUnspecified;
}

wstring cTimeInfo::toString(vector <WordMatch> &m,wstring &tmpstr)
{ LFS
	tmpstr.clear();
	wstring s;
	itos(L"",tWhere,tmpstr,L":");
	af(L"timePreviousLink=",timePreviousLink,tmpstr);
	af(L"SPTAnchor=",timeSPTAnchor,tmpstr);
	af(L"ETAnchor=",timeETAnchor,tmpstr);
	//af(L"RTAnchor=",timeRTAnchor,tmpstr);
	tmpstr+=L"type="+timeString(timeRelationType,s)+L" ";
	if (timeModifier!=-1) tmpstr+=L"M="+m[timeModifier].word->first+L" ";
	af(L"Modifier2=",timeModifier2,tmpstr);
	af(L"absMetaRelation=",absMetaRelation,tmpstr);
	if (timeCapacity!=cUnspecified)
		tmpstr+=L"Capacity="+capacityString(timeCapacity)+L" ";
	af(L"absSeason=",absSeason,tmpstr);
	af(L"absDateSpec=",absDateSpec,tmpstr);
	if (absMonth!=-1) tmpstr+=months[absMonth].word;
	if (absMonth!=-1 && absDayOfMonth!=-1)
	{
		itos(L"",absDayOfMonth,tmpstr,(absYear!=-1) ? L",":L" ");
		af(L"",absYear,tmpstr);
	}
	else
	{
		af(L"absDayOfMonth=",absDayOfMonth,tmpstr);
		af(L"absYear=",absYear,tmpstr);
	}
	af(L"absDayOfWeek=",absDayOfWeek,tmpstr);
	af(L"timeOfDay=",timeOfDay,tmpstr);
	af(L"absHour=",absHour,tmpstr);
	af(L"absMinute=",absMinute,tmpstr);
	af(L"absSecond=",absSecond,tmpstr);
	if (absTimeSpec!=-1)
		tmpstr+=(absTimeSpec==1) ? L"P.M. ":L"A.M. ";
	if (timeFrequency!=-1)
	{
		for (int I=0; recurrence_flags[I]; I++)
			if (timeFrequency==recurrence_flags[I])
			{
				if (twr_ara[I][0])
					tmpstr+=wstring(L"frequency=")+twr_ara[I];
				break;
			}
	}
	if (metaDescriptive)
		tmpstr+=L"metaDescriptive ";
	af(L"absHoliday=",absHoliday,tmpstr);
	return tmpstr;
}

Inflections seasons[] = {{L"winter",SINGULAR},{L"wintertime",SINGULAR},{L"spring",SINGULAR},{L"springtime",SINGULAR},{L"summer",SINGULAR},{L"summertime",SINGULAR},{L"fall",SINGULAR},
			{L"winters",PLURAL},{L"wintertimes",PLURAL},{L"springs",PLURAL},{L"springtimes",PLURAL},{L"summers",PLURAL},{L"summertimes",PLURAL},{L"falls",PLURAL},{NULL,0}};
int whichSeason(wstring w)
{ LFS
	for (int I=0; seasons[I].inflection; I++)
		if (w==seasons[I].word)
			return I;
	return -1;
}

void WordClass::createTimeCategories(bool normalize)
{ LFS
	// TYPE 1 - expressions that denote a specific time or a specific relative time
	// In an expression (_NOUN), this is the main noun
	Inflections timeUnits[] = {{L"second",SINGULAR},{L"minute",SINGULAR},{L"hour",SINGULAR},{L"day",SINGULAR},{L"week",SINGULAR},
		{L"month",SINGULAR},{L"season",SINGULAR},{L"semester",SINGULAR},{L"quarter",SINGULAR},{L"year",SINGULAR},{L"decade",SINGULAR},{L"century",SINGULAR},{L"millenium",SINGULAR},
		{L"seconds",PLURAL},{L"minutes",PLURAL},{L"hours",PLURAL},{L"days",PLURAL},{L"weeks",PLURAL},
		{L"months",PLURAL},{L"seasons",PLURAL},{L"semesters",PLURAL},{L"quarters",PLURAL},{L"years",PLURAL},{L"decades",PLURAL},{L"centuries",PLURAL},{L"millenia",PLURAL},{NULL,0}};
	Inflections dayUnits[] = {{L"morning",SINGULAR},{L"afternoon",SINGULAR},{L"evening",SINGULAR},{L"night",SINGULAR},
			{L"tonight",SINGULAR},{L"today",SINGULAR},{L"tomorrow",SINGULAR},{L"yesterday",SINGULAR},{L"midnight",SINGULAR},
	{L"dawn",SINGULAR},{L"dusk",SINGULAR},{L"noon",SINGULAR},
			{L"mornings",PLURAL},{L"afternoons",PLURAL},{L"evenings",PLURAL},{L"nights",PLURAL},{L"tomorrows",PLURAL},{L"yesterdays",PLURAL},{L"midnights",PLURAL},
	{L"dawns",PLURAL},{L"dusks",PLURAL},{L"noons",PLURAL},{NULL,0}};
	Inflections simultaneousUnits[] = {{L"moment",SINGULAR},{L"instant",SINGULAR},{L"flash",SINGULAR},{L"shake",SINGULAR},
	{L"moments",PLURAL},{L"instants",PLURAL},{L"shakes",PLURAL},{NULL,0}};
	if (normalize)
	{
		usageCostToNoun(months,L"month");
		usageCostToNoun(months_abb,L"month");
		usageCostToNoun(daysOfWeek_abb,L"daysOfWeek");
		usageCostToNoun(daysOfWeek,L"daysOfWeek");
		usageCostToNoun(seasons,L"season");
		usageCostToNoun(timeUnits,L"timeUnit");
		usageCostToNoun(dayUnits,L"dayUnit");
		addTimeFlags();
		return;
	}
	predefineWords(months,L"month",L"month",L"noun",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT,months);
	predefineWords(months_abb,L"month",L"month",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT,months_abb);
	predefineWords(daysOfWeek_abb,L"daysOfWeek",L"daysOfWeek",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT,daysOfWeek_abb);
	predefineWords(daysOfWeek,L"daysOfWeek",L"daysOfWeek",L"noun",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT,daysOfWeek);
	predefineWords(seasons,L"season",L"season",L"noun",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT,seasons);
	predefineWords(timeUnits,L"timeUnit",L"timeUnit",L"noun",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT|T_LENGTH,timeUnits);
	predefineWords(dayUnits,L"dayUnit",L"dayUnit",L"noun",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT,simultaneousUnits);
	predefineWords(simultaneousUnits,L"simultaneousUnit",L"simultaneousUnit",L"noun",tFI::queryOnAnyAppearance,true);
	addTimeFlag(T_UNIT,dayUnits);
	wchar_t *times[]={L"3:45",NULL};
	predefineWords(times,L"time",L"time",0,false);
	wchar_t *dates[]={L"3/4/90",NULL};
	predefineWords(dates,L"date",L"date",0,false);
	addTimeFlags();
}

void WordClass::addTimeFlags()
{ LFS
	/******************************************************************************************/
	// ABSOLUTE TIME
	// words implying a time in the past, present or future by themselves.
	/////////////////////////////////////////////////
	// Then, I couldn't see the logic of it.
	// Recently I have begun having nightmares.
	// Long ago the dinosaurs ruled the earth.
	// I couldn't see yet the wisdom of his words.
	//************ adverbs
	wchar_t *twr_ap[]={L"recently", L"previously", L"already",L"ago",L"then",NULL};
	addTimeFlag(T_BEFORE,twr_ap);
	wchar_t *twr_ac[]={L"currently",L"still",L"now",L"immediately",NULL};
	addTimeFlag(T_PRESENT,twr_ac);
	wchar_t *twr_af[]={L"yet",L"soon",L"straightaway",L"directly",L"shortly",NULL};
	addTimeFlag(T_AFTER,twr_af);
	//************* nouns
	wchar_t *twr_u[]={L"date", L"then", L"anytime",L"late",L"veronal",L"o'clock",NULL};
	addTimeFlag(T_UNIT,twr_u);
	wchar_t *twr_l[]={L"period", L"term", L"age", L"moment", L"time", L"era", L"weekend",L"for ever",NULL};
	addTimeFlag(T_LENGTH|T_UNIT,twr_l);
	// Past her mother's death she couldn't decide.
	wchar_t *twr_uap[]={L"past",L"yesterday",NULL};
	addTimeFlag(T_BEFORE|T_UNIT,twr_uap);
	wchar_t *twr_uac[]={L"present",L"today",L"to-day",NULL};
	addTimeFlag(T_PRESENT|T_UNIT,twr_uac);
	wchar_t *twr_uaf[]={L"future",L"tomorrow",L"tonight",L"to-morrow",L"morrow",L"to-night",NULL};
	addTimeFlag(T_AFTER|T_UNIT,twr_uaf);
	// recurring adverbs having an independent absolute recurring interval
	addTimeFlag(T_RECURRING,twr_ara);
	wchar_t *twr_c[]={L"currently",L"existing",NULL};
	addTimeFlag(T_PRESENT,twr_c);
	/******************************************************************************************/

	/******************************************************************************************/
	// RELATIVE TIME
	// words lending a direction or modification of another time word
	//************ adjectives
	// must modify time expression
	wchar_t *twr_r[]={L"every",L"several",L"each",NULL};
	addTimeFlag(T_RECURRING,twr_r);
	wchar_t *twr_v[]={L"around",L"about",NULL};
	addTimeFlag(T_VAGUE,twr_v);
	wchar_t *twr_p[]={L"prior",L"previous",L"preceding",L"former",L"earlier",L"already",L"recent",L"earliest",L"early",L"medieval",NULL};
	addTimeFlag(T_BEFORE,twr_p);
	wchar_t *twr_artc[]={L"current",NULL};
	addTimeFlag(T_PRESENT,twr_artc);
	//************ conjunctions
	// The next week he was filled with surprises.
	// The next year he couldn't wait to leave.
	// I will do this magic trick next.
	// Don't be in the library past 2 o'clock!
	// Beyond July he was entirely booked.
	wchar_t *twr_f[]={L"after",L"afterwards",L"next",L"following",L"latter",L"later",L"coming",L"latest",L"late",L"soon",L"beyond",NULL};
	addTimeFlag(T_AFTER,twr_f);
	// adverbs
	// then, just
	wchar_t *twr_ar[]={L"again",L"often",L"frequently", L"oft", L"oftentimes", L"ofttimes", L"always", L"ever", L"never", // removed L"much" - if you knew how much I loved you
		L"repeatedly", L"infrequently", L"rarely", L"occasionally", L"seldom",L"usually",L"sometimes",NULL};
	addTimeFlag(T_RECURRING,twr_ar);
	// prepositions
	// must have time expression as object or modifying it
	wchar_t *twr_vp[]={L"around",L"about",L"near",L"along",L"amid",L"than",NULL}; // more than / less than
	addTimeFlag(T_VAGUE,twr_vp);
	// You will be here by noon.
	// The store is closed till August each year.
	// The store is closed from July to August.
	wchar_t *twr_b[]={L"by",L"till",L"until",L"to",L"before",L"ere",NULL};
	addTimeFlag(T_BEFORE,twr_b);
	wchar_t *twr_a[]={L"since",L"from",NULL}; // also "after" under conjunctions
	addTimeFlag(T_AFTER,twr_a);

	// introductory time expressions
	// function: immediately advance timeline by or till the introductory object

	// He ran through the opening door in a second.
	// In the next month he wrote two dozen chapters.
	// Throughout that summer he thought only of her.
	// During August she ate almost nothing.
	// Meanwhile, she fretted.
	// While she played tennis, he golfed.
	wchar_t *tw_in[]={L"inside",L"in",L"within",L"throughout",L"through",L"during",L"meanwhile",L"while",L"for",NULL};
	addTimeFlag(T_THROUGHOUT,tw_in);
	// At the end of July, he was running three miles daily.
	// He decided on December the eighth.
	// Upon discovering the murderer, he set out once again.
	wchar_t *tw_at[]={L"at",L"upon",L"on",L"of",NULL};
	addTimeFlag(T_AT,tw_at);
	// Three time meanings for 'over'
	// He was seen in the vicinity over two weeks ago. (1)
	// Over the two weeks he was supposedly working for FEMA, he actually played pool. (2)
	// He completed the test in over two hours. (3)
	// He completed the race in under 2 minutes.
	wchar_t *tw_on[]={L"over",L"under",NULL};
	addTimeFlag(T_ON,tw_on);
	// Midway through August he quit.
	wchar_t *tw_mid[]={L"midway",NULL};
	addTimeFlag(T_MIDWAY,tw_mid);
	// Between the 2nd and 3rd week in July he played tennis frequently.
	wchar_t *tw_int[]={L"between",NULL};
	addTimeFlag(T_INTERVAL,tw_int);

	wchar_t *tw_start[]={L"start",L"begin",L"commence",L"embark",L"initiate",L"open",L"originate",NULL};
	addTimeFlag(T_START,tw_start);
	wchar_t *tw_stop[]={L"stop",L"halt",L"conclude",L"discontinue",L"close",L"cease",L"quit",L"interrupt",L"suspend",L"pause",NULL};
	addTimeFlag(T_STOP,tw_stop);
	wchar_t *tw_finish[]={L"finish",L"end",L"terminate",L"complete",L"conclude",NULL};
	addTimeFlag(T_FINISH,tw_finish);
	wchar_t *tw_resume[]={L"resume",L"continue",L"recommence",L"renew",L"reopen",L"restart",NULL};
	addTimeFlag(T_RESUME,tw_resume);
	wchar_t *tw_card[]={L"around",L"about",L"by",L"till",L"until",L"to",L"before",L"since",L"from",L"before",L"after",L"at",NULL};
	addTimeFlag(T_CARDTIME,tw_card);


}

/*
names of festivals, holidays and other occasions of religious observance, remembrance of famous massacres, etc. as “holidays”.
Some of these expressions, like “Shrove Tuesday” and “Thanksgiving Day” contain trigger words. Others, like “Thanksgiving”, “Christmas”, and “Diwali”, do not.
A tagger is allowed to tag any holiday it wants (sorry, there is NO fixed list of holidays!), but is to assign it a value only when that value can be inferred
from the local and global context of the text, rather than from cultural and world knowledge. For example, given “Christmas is celebrated in December”,
the value of December is assigned to Christmas, but given only “Christmas left me poor”, “Christmas” is to be tagged without a value.
*/
// per day:
/* http://www.earthcalendar.net/_php/lookup.php?mode=date&m=2&d=2&y=2006 */
// per week or month:
// http://www.earthcalendar.net/_php/lookup.php?mode=datespan

	// Months:
	struct
	{
		wchar_t *name;
		wchar_t *beginDate;
		wchar_t *endDate;
	} holidayMonths[] = {
	{L"black history month",L"2/1",L"2/30"},
	{L"cinco de mayo",L"5/1",L"5/30"},
	{L"national hispanic heritage month",L"9/15",L"10/14 "},
	{L"ramadan",L"9/24",L"10/23"},
	{L"oktoberfest",L"9/1",L"9/30"},
	{L"tu b'shvat",L"2/2",L"2/3"},
	{L"passover",L"4/3",L"4/10"},
	{L"shavuot",L"5/23",L"5/24"},
	{L"rosh hashanah",L"9/22",L"9/24"},
	{L"ramadan",L"9/24",L"10/23"},
	{L"yom kippur",L"10/1",L"10/2"},
	{L"sukkot",L"10/6",L"10/13"},
	{L"simchat torah",L"10/14",L"10/15"},
	{L"day of the dead",L"11/1",L"11/2"},
	{L"advent",L"12/3",L"12/24"},
	{L"hanukkah",L"12/16",L"12/23"},
	{L"kwanzaa",L"12/26",L"1/1"},
	{NULL,NULL,NULL}
	};


	// Single Days:
	struct
	{
		wchar_t *name;
		wchar_t *dayDate;
	} holidayDays[] = {
	{L"new year's day",L"1/1"},
	{L"new years day",L"1/1"},
	{L"new years eve",L"12/29"},
	{L"new year's eve",L"12/29"},
	{L"martin luther king day",L"1/15"},
	{L"groundhog day",L"2/2"},
	{L"valentine's day",L"2/14"},
	{L"chinese new year",L"2/18"},
	{L"presidents' day",L"2/19"},
	{L"mardi gras",L"2/20"},
	{L"holi",L"3/3"},
	{L"purim",L"3/4"},
	{L"pulaski day",L"3/5"},
	{L"st. urho's day",L"3/16"},
	{L"st. patrick's day",L"3/17"},
	{L"mother's day, uk",L"3/18"},
	{L"persian new year",L"3/20"},
	{L"april fools' day",L"4/1"},
	{L"easter",L"4/8"},
	{L"tax day",L"4/15"},
	{L"earth day",L"4/22"},
	{L"administrative professionals day",L"4/25"},
	{L"arbor day",L"4/27"},
	{L"may day",L"5/1"},
	{L"national day of prayer",L"5/4"},
	{L"kentucky derby day",L"5/5"},
	{L"nurses day",L"5/6"},
	{L"receptionist day",L"5/9"},
	{L"mother's day",L"5/13"},
	{L"victoria day",L"5/21"},
	{L"memorial day",L"5/28"},
	{L"flag day",L"6/14"},
	{L"father's day",L"6/17"},
	{L"independence day",L"7/4"},
	{L"bastille day",L"7/14"},
	{L"chinese valentine",L"8/19"},
	{L"father-in-law's day",L"7/30"},
	{L"labor day",L"9/3"},
	{L"grandparents day",L"9/9"},
	{L"columbus day",L"10/9"},
	{L"thanksgiving, canada",L"10/9"},
	{L"boss's day",L"10/16"},
	{L"diwali",L"10/21"},
	{L"sweetest day",L"10/21"},
	{L"mother-in-law's day",L"10/22"},
	{L"halloween",L"10/31"},
	{L"election day",L"11/7"},
	{L"veteran's day",L"11/11"},
	{L"thanksgiving",L"11/23"},
	{L"santa's list day",L"12/4"},
	{L"st. nicholas day",L"12/6"},
	{L"christmas eve",L"12/24"},
	{L"christmas",L"12/25"},
	{L"election day",L""},
	{L"vacation",L""},
	{L"super bowl sunday",L""},
	{NULL,NULL}
	};

int whichHoliday(wstring w)
{ LFS
	int I=0;
	for (; holidayDays[I].name!=NULL; I++)
		if (!wcsicmp(holidayDays[I].name,w.c_str())) 
			return I;
	int addOffset=I;
	for (I=0; holidayMonths[I].name!=NULL; I++)
		if (!wcsicmp(holidayMonths[I].name,w.c_str())) 
			return I+addOffset;
	return -1;
}

wstring holidayString(int holiday)
{ LFS
  if (holiday<sizeof(holidayDays)/sizeof(holidayDays[0]))
		return holidayDays[holiday].name;
	holiday-=sizeof(holidayDays);
  if (holiday<sizeof(holidayMonths)/sizeof(holidayMonths[0]))
		return holidayMonths[holiday].name;
	return L"illegal";
}

int WordClass::predefineHolidays()
{ LFS
  unsigned int iForm=FormsClass::addNewForm(L"holiday",L"hol",false,true);
  for (int I=0; holidayDays[I].name; I++)
  {
    handleExtendedParseWords(holidayDays[I].name);
    bool added;
    addNewOrModify(NULL,holidayDays[I].name,0,iForm,0,0,L"",-1,added);
		tIWMM iWord=query(holidayDays[I].name);
		iWord->second.timeFlags|=T_UNIT;
  }
  for (int I=0; holidayMonths[I].name; I++)
  {
    handleExtendedParseWords(holidayMonths[I].name);
    bool added;
    addNewOrModify(NULL, holidayMonths[I].name,0,iForm,0,0,L"",-1,added);
		tIWMM iWord=query(holidayMonths[I].name);
		iWord->second.timeFlags|=T_UNIT;
  }
  return iForm;
}

int sts[]={ VT_PRESENT,VT_PRESENT_PERFECT,VT_PAST,VT_PAST_PERFECT,VT_FUTURE,VT_FUTURE_PERFECT,
VT_PRESENT_PERFECT+VT_VERB_CLAUSE,VT_PRESENT+VT_VERB_CLAUSE,
VT_PRESENT+VT_EXTENDED,VT_PRESENT_PERFECT+VT_EXTENDED,VT_PAST+VT_EXTENDED,VT_PAST_PERFECT+VT_EXTENDED,
VT_FUTURE+VT_EXTENDED,VT_FUTURE_PERFECT+VT_EXTENDED,VT_PRESENT_PERFECT+VT_VERB_CLAUSE+VT_EXTENDED,
VT_PRESENT+VT_VERB_CLAUSE+VT_EXTENDED };

int sits[]={ VT_PRESENT,VT_PRESENT_PERFECT,VT_EXTENDED + VT_PRESENT,VT_PASSIVE + VT_PRESENT,
VT_EXTENDED + VT_PRESENT_PERFECT,VT_PASSIVE+ VT_PRESENT_PERFECT,VT_PASSIVE+ VT_PRESENT+VT_EXTENDED,
VT_PASSIVE+ VT_PRESENT_PERFECT+VT_EXTENDED,VT_PASSIVE+ VT_PRESENT+VT_EXTENDED+VT_VERB_CLAUSE};


void Source::printTenseStatistic(cTenseStat &tenseStatistics,int sense,int numTotal)
{ LFS
	wstring followedByStr,infinitiveStr;
	for (unsigned int J=0; J<NUM_SIMPLE_TENSE; J++)
	{
		wchar_t followedBy[1024];
		if (tenseStatistics.followedBy[J])
		{
			wstring tmp;
			wsprintf(followedBy,L"%s(%d),",senseString(tmp,sts[J]).c_str(),tenseStatistics.followedBy[J]);
			followedByStr+=followedBy;
		}
	}
	for (unsigned int J=0; J<NUM_INF_TENSE; J++)
	{
		wchar_t infinitive[1024];
		if (tenseStatistics.infinitive[J])
		{
			wstring tmp;
			wsprintf(infinitive,L"%s(%d),",senseString(tmp,sits[J]).c_str(),tenseStatistics.infinitive[J]);
			infinitiveStr+=infinitive;
		}
	}
	if (followedByStr.length())
	{
		followedByStr[followedByStr.length()-1]=L']';
		followedByStr=L" ["+followedByStr;
	}
	if (infinitiveStr.length())
	{
		infinitiveStr[infinitiveStr.length()-1]=L']';
		infinitiveStr=L" INF["+infinitiveStr;
	}
	wstring tmp;
	if (numTotal && tenseStatistics.occurrence)
		lplog(L"%29s:%05d %03d%% %05d %s %s",senseString(tmp,sense).c_str(),
					tenseStatistics.occurrence,tenseStatistics.occurrence*100/numTotal,tenseStatistics.passiveOccurrence,followedByStr.c_str(),infinitiveStr.c_str());
}

void Source::printTenseStatistics(wchar_t *fromWhere,cTenseStat tenseStatistics[],int numTotal)
{ LFS
	int numPrintTotal=0;
	for (unsigned int I=0; I<NUM_SIMPLE_TENSE; I++)
		numPrintTotal+=tenseStatistics[I].occurrence;
	if (!numPrintTotal) return;
	lplog(L"%s: %d Total",fromWhere,numTotal);
	for (unsigned int I=0; I<NUM_SIMPLE_TENSE; I++)
		printTenseStatistic(tenseStatistics[I],sts[I],numTotal);
}

void Source::printTenseStatistics(wchar_t *fromWhere, unordered_map <int,cTenseStat> &tenseStatistics,int numTotal)
{ LFS
	if (!tenseStatistics.size()) return;
	lplog(L"%s: %d Total",fromWhere,numTotal);
	for (unordered_map <int,cTenseStat>::iterator I=tenseStatistics.begin(),IEnd=tenseStatistics.end(); I!=IEnd; I++)
		printTenseStatistic(I->second,I->first,numTotal);
}

wstring timeString(int timeWordFlags,wstring &s)
{ LFS
	wchar_t *tws[]={ L"SEQ",L"before",L"after",L"present",
		L"throughout",L"recurring",L"at",L"midway",
		L"in",L"on",L"interval",L"start",L"stop",L"resume",L"finish",L"range"};
	wchar_t *tws2[]={	L" unit",L" time",L" date",L" vague",L" length",L" cardtime" };
	s=tws[timeWordFlags&15];
	for (int I=4; I<10; I++)
		if (timeWordFlags&(1<<I))
	    s+=tws2[I-4];
	return s;
}

// look up last timeline segment belonging to speakers
bool Source::determineTimelineSegmentLink()
{ LFS
	return false;
}

// determine whether this speaker group is really a change in perspective from one group of 
// people to another separate group in another location/time.
// executed before marking any speaker non-physical (as part of the transition aging to any new speaker group).
// sets tlTransition flag of speaker group.
bool Source::speakerGroupTransition(int where,int sg,bool forwardTransition)
{ LFS
	if (!sg) return false;
	bool allNew=true,allNotPhysicallyPresent=true;
	wstring tmpstr;
	// is the newSG composed of people who have not been seen in any previous speaker group?
	if (debugTrace.traceTime)
	lplog(LOG_RESOLUTION|LOG_SG,L"%06d:SGT for %s - %s",where,toText(speakerGroups[sg],tmpstr),(forwardTransition) ? L"forward":L"");
	for (set <int>::iterator si=speakerGroups[sg].speakers.begin(),siEnd=speakerGroups[sg].speakers.end(); si!=siEnd; si++)
	{
		// has this speaker been in any previous speaker group?
		int lastSG=-1;
		for (int I=sg-1; I>=0 && lastSG<0; I++)
			if (speakerGroups[I].speakers.find(*si)!=speakerGroups[I].speakers.end())
				lastSG=I;
		speakerGroups[sg].lastSGSpeakerMap[*si]=lastSG;
		if (lastSG>=0)
			allNew=false;
		vector <cLocalFocus>::iterator lsi=in(*si);
		bool previouslyPhysicallyPresent=(lsi!=localObjects.end() && lsi->physicallyPresent);
	  speakerGroups[sg].pppMap[*si]=previouslyPhysicallyPresent;
		if (previouslyPhysicallyPresent)
			allNotPhysicallyPresent=false;
		if (debugTrace.traceTime)
		lplog(LOG_RESOLUTION|LOG_SG,L"%06d:SGT speaker %s:lastSG=%d PPP=%s",where,objectString(*si,tmpstr,false).c_str(),lastSG,(previouslyPhysicallyPresent) ? L"true":L"false");
	}
	if (!forwardTransition)
		speakerGroups[sg].tlTransition=(allNew || allNotPhysicallyPresent) && section<sections.size() && sections[section].endHeader==where;
	if (debugTrace.traceTime)
		lplog(LOG_RESOLUTION|LOG_SG,L"%06d:SGT for %s - transition=%s (section begin=%d)",where,toText(speakerGroups[sg],tmpstr),(allNew || allNotPhysicallyPresent) ? L"true":L"false",(section<sections.size()) ? sections[section].begin:-1);
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
void Source::initializeTimelineSegments(void)
{ LFS
	cTimelineSegment ts;
	ts.begin=0;
	ts.speakerGroup=0;
	ts.sr=-1;
	ts.parentTimeline=-1;
	timelineSegments.push_back(ts);
}

void Source::createTimelineSegment(int where)
{ LFS
  vector <cSpaceRelation>::iterator sr = findSpaceRelation(where);
	if (sr!=spaceRelations.end() && sr->where==where)
	{
		int ctl=currentTimelineSegment;
		if (currentEmbeddedTimelineSegment>=0 && currentEmbeddedSpeakerGroup>=0 && 
			  where>=speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin &&
			  where<speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd &&
				// embedded story will only be set in quote, but an embedded story is set in the past, so it is equivalent to out of quote.
				(m[where].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE))!=0)
			ctl=currentEmbeddedTimelineSegment;
		if (sr->tft.timeTransition || sr->tft.nonPresentTimeTransition || sr->timeInfo.size()>0)
			timelineSegments[ctl].timeTransitions.insert((int) (sr-spaceRelations.begin()));
		if (sr->establishingLocation)
			timelineSegments[ctl].locationTransitions.push_back((int) (sr-spaceRelations.begin()));
	}
	if (currentEmbeddedSpeakerGroup>=0 && currentEmbeddedSpeakerGroup<(signed)speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size() && speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin==where)
	{
		if (where<speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd)
		{
			currentEmbeddedTimelineSegment=timelineSegments.size();
			cTimelineSegment ts;
			ts.begin=where;
			ts.end=speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd;
			ts.speakerGroup=currentSpeakerGroup;
			ts.sr=(int) (sr-spaceRelations.begin());
			ts.linkage=determineTimelineSegmentLink();
			ts.parentTimeline=currentTimelineSegment;
			timelineSegments.push_back(ts);
		}
		else
		{
			wstring tmpstr,tmpstr2;
			if (debugTrace.traceTime)
			lplog(LOG_RESOLUTION,L"%06d:[%d,%d]embedded speaker group rejected [%s], in %s.",
			where,currentSpeakerGroup,currentEmbeddedSpeakerGroup,
				toText(speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup],tmpstr),
				toText(speakerGroups[currentSpeakerGroup],tmpstr2));
		}
	}
	if (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].sgBegin==where && speakerGroups[currentSpeakerGroup].tlTransition && where>0)
	{
		timelineSegments[currentTimelineSegment].end=where;
		currentTimelineSegment=timelineSegments.size();
		cTimelineSegment ts;
		ts.begin=where;
		ts.speakerGroup=currentSpeakerGroup;
		ts.sr=(int) (sr-spaceRelations.begin());
		ts.linkage=determineTimelineSegmentLink();
		ts.parentTimeline=-1;
		timelineSegments.push_back(ts);
	}
}

// 1535: ""           since 1916 ""                ""      AFTER
// 1767:Had not seen for 5 years PRESENT_PERFECT NEGATION THROUGHOUT
// early in the war? [leave till later]