enum eCapacity { cMillenium, cCentury, cDecade, cYear, cSemester, cSeason, cQuarter, cMonth, cWeek, cDay,
                 cHour, cMinute, cSecond, cMoment,
								 cMorning, cNoon, cAfternoon, cEvening, cDusk, cNight, cMidnight, cDawn,
								 cTonight,cToday,cTomorrow,cYesterday,
								 cNamedMonth,cNamedDay,cNamedSeason,cNamedHoliday,
								 cUnspecified };

// all groups of words that refer almost exclusively to time.
// 1. specific time or a specific relative time
// 2. time order (one event happening before another)
// 3. time direction (one event happening in the past, is happening now or will happen in the future)
// 4. recurring time
// include also Points:  ON_OR_BEFORE, ON_OR_AFTER,  Durations: EQUAL_OR_LESS, EQUAL_OR_MORE
enum eTimeWordFlags {
	T_ASSUME_SEQUENTIAL = 0,
	// on the following lines, flags cannot be combined, only one may be active at a time
	T_BEFORE,T_AFTER,T_PRESENT,T_THROUGHOUT,T_RECURRING, T_AT,T_MIDWAY,
	T_IN, T_ON, T_INTERVAL, T_START, T_STOP, T_RESUME,T_FINISH,T_RANGE, T_META_RELATION, T_UNIT,
	T_MODIFIER,
	// on the following lines, flags can be combined
	T_TIME=32,T_DATE=64,T_TIMEDATE=T_TIME+T_DATE,
	T_VAGUE = 128, T_LENGTH = 256, T_CARDTIME = 512,
	T_UNSPECIFIED = -1
};
int whichCapacity(wstring w);
int whichMonth(wstring w);
int whichSeason(wstring w);
int whichDayOfWeek(wstring w);
int whichHoliday(wstring w);
int whichRecurrence(wstring w);
wstring capacityString(int capacityFlags);
wstring timeString(int timeWordFlags,wstring &s);
wstring senseString(wstring &s,int verbSense);
wstring holidayString(int holiday);
// TENSE (vB, vC etc) from Quirk
// Tense flags from Reichenbach

// I did not expect that he would win the race R<E<S

// vS                               simple examine                     VT_PRESENT                                          R=E=S
// vS+past                          examined                           VT_PAST                                             R=E<S
// vB                               has examined                       VT_PRESENT_PERFECT                                  E<S=R
// vrB															having examined                    VT_PRESENT_PERFECT+VT_VERB_CLAUSE                   E<S=R
// vB+past													had examined                       VT_PAST_PERFECT                                       E<R<S
// vS+future                        will examine                       VT_FUTURE                                           S=R<E
// vAB+future                       will have examined                 VT_FUTURE_PERFECT                                   S<E<R

//// EXTENDED E is approximate
// vC                               is examining                       VT_EXTENDED+ VT_PRESENT                             S=R E<=>R
// vE (non-Quirk)                   naked progressive spreading        VT_EXTENDED+ VT_PRESENT+VT_VERB_CLAUSE              S=R E<=>R
// vC+past													was examining                      VT_EXTENDED+ VT_PAST                                R=E<S
// vBC                              has been examining                 VT_EXTENDED+ VT_PRESENT_PERFECT                     E<S=R
// vrBC                             having been examining              VT_EXTENDED+ VT_PRESENT_PERFECT+VT_VERB_CLAUSE      E<S=R
// vBC+past                         had been examining                 VT_EXTENDED+ VT_PAST_PERFECT                        E<R<S
// vAC+future                       will be examining                  VT_EXTENDED+ VT_FUTURE                              S=R<E
// vABC+future                      will have been examining           VT_EXTENDED+ VT_FUTURE_PERFECT                      S<E<R

//// PASSIVE
// vD                               is examined                        VT_PASSIVE+ VT_PRESENT
// vrD															being examined                     VT_PASSIVE+ VT_PRESENT+VT_EXTENDED+VT_VERB_CLAUSE
// vD+past													was examined                       VT_PASSIVE+ VT_PAST
// vBD                              has been examined                  VT_PASSIVE+ VT_PRESENT_PERFECT
// vrBD                             having been examined               VT_PASSIVE+ VT_PRESENT_PERFECT+VT_VERB_CLAUSE
// vBD+past                         had been examined                  VT_PASSIVE+ VT_PAST_PERFECT
// vCD                              is being examined                  VT_PASSIVE+ VT_PRESENT+VT_EXTENDED
// vCD+past                         was being examined                 VT_PASSIVE+ VT_PAST+VT_EXTENDED
// vBCD                             has been being examined            VT_PASSIVE+ VT_PRESENT_PERFECT+VT_EXTENDED
// vBCD+past                        had been being examined            VT_PASSIVE+ VT_PAST_PERFECT+VT_EXTENDED
// vAD+future                       will be examined                   VT_PASSIVE+ VT_FUTURE
// vACD+future                      will be being examined             VT_PASSIVE+ VT_FUTURE+VT_EXTENDED
// vABD+future                      will have been examined            VT_PASSIVE+ VT_FUTURE_PERFECT
// vABCD+future                     will have been being examined      VT_PASSIVE+ VT_FUTURE_PERFECT+VT_EXTENDED

//// POSSIBLE + COERCION
// may [70% possible, 20% COERCION],
// could [60/40], should [40/60], must[10/90], do[20/80]
// vS+conditional                   may examine                        VT_POSSIBLE+ VT_PRESENT
// vS+imp                           do examine                         VT_IMPERATIVE+ VT_PRESENT
// vS+imp+past                      did examine                        VT_IMPERATIVE+ VT_PAST
// vS                               must examine                       VT_POSSIBLE+ VT_PRESENT
// vACD+conditional                 may be being examined              VT_POSSIBLE+ VT_PRESENT+VT_PASSIVE
// vAC+conditional                  may be examining, do be examining  VT_POSSIBLE+ VT_PRESENT+VT_EXTENDED
// vAB+conditional                  may have examined                  VT_POSSIBLE+ VT_PAST
// vABD+conditional                 may have been examined             VT_POSSIBLE+ VT_PAST+VT_PASSIVE
// vABCD+conditional                may have been being examined       VT_POSSIBLE+ VT_PAST+VT_PASSIVE+VT_EXTENDED
// vAD+conditional                  may be examined                    VT_POSSIBLE+ VT_PRESENT_PERFECT+VT_PASSIVE
// vABC+conditional                 may have been examining            VT_POSSIBLE+ VT_PRESENT_PERFECT+VT_EXTENDED

enum verbDimensions {
	// verbTense
	VT_PRESENT=1,
	VT_PRESENT_PERFECT,
	VT_PAST,
	VT_PAST_PERFECT,
	VT_FUTURE,
	VT_FUTURE_PERFECT,
	// verbAspect
    VT_POSSIBLE=8,
    VT_PASSIVE=16,
    VT_EXTENDED=32,
    VT_VERB_CLAUSE=64,
    VT_NEGATION=128,
    VT_IMPERATIVE=256
};

#define VT_TENSE_MASK 7
#define TENSE_NOT_SPECIFIED -2

typedef struct 
{ 
  wchar_t *prep;
  wchar_t *equivalent;
} tPrepEquivalent;

extern tPrepEquivalent prepEquivalents[];

enum ePrepRel { tprNEAR, tprX, tprIN, tprAT, tprY, tprSPAT, tprZ, tprMATH, tprLOGIC, tprFROM, tprTO, tprOF, tprAS, tprFOR, tprPER, tprVS};

typedef struct 
{
	wchar_t *prep;
	int prepRelationType;
} tPrepRelation;

extern tPrepRelation prepRelations[];

class cTimeInfo
{
	public:
	//int timeType; // 	enum RENUM { R_SimplePresent=1,R_SimplePast=2,R_SimpleFuture=3,R_PresentPerfect=4,R_PastPerfect=5,R_FuturePerfect=6 };
	/*
		analysis of tenses: (from Pari.pdf)
	  Speech Time (SPT), Reference Time (RT), Event Time (ET)

  	bounded events (a) end at the next sentence.  Unbounded events (d) do not end at any specific time.
	  Simple Present: S = E = R John studies now.  (sounds strange without progressive aspect, ok in a stage direction in a play)
 			(d) Mary was knitting on the train.          Past Progressive: RT < SpT, unbounded ET=RT
		Simple Past (E = R) < S John studied yesterday. R = yesterday
 			(a) Mary arrived.                            Past:         RT < SpT, ET = RT
		Simple Future S < (E = R) John will study tomorrow R = tomorrow
		Present Perfect E < (S = R) John has studied by now. R = now
 			Mary has arrived.                        Present:      RT = SpT, ET = RT
		Past Perfect E < R < S John had studied before the exam started. R = the time the exam started.
 			On Sunday, Mary had (already) arrived.   Past Perfect: RT < SpT, ET < RT
		Future Perfect S < E < R John will have studied before the exam starts. R = the time the exam starts (in the future)
	*/
	int tWhere;
	int timePreviousLink; // a link to the previous statement (by time) 
	int timeSPTAnchor; // speech time
	int timeETAnchor; // event time
	int timeRTAnchor; // reference time
	eTimeWordFlags timeRelationType; // T_BEFORE, T_AFTER
	int timeModifier; // early in the evening/ a few hours after - this can be a chain (The store is closed from July to August.) ( 2 p.m. on the afternoon of May 7 , 1915)
	int timeModifier2; // secondary modifier
	int absMetaRelation; // a time relation between two separate relations / he left after I spoke.
	eCapacity timeCapacity; // day/hour/evening etc
	short absYear;
	char absSeason;
	char absDateSpec; // A.D. B.C.
	char absMonth;
	char absDayOfWeek;
	char absDayOfMonth;
	char timeOfDay; // morning/evening/noon etc
	char absHour;
	char absMinute;
	char absSecond;
	char absTimeSpec; // A.M. P.M.
	char absMoment;
	char absNamedDay;
	char absNamedHoliday;
	char absNamedMonth;
	char absNamedSeason;
	char absToday;
	char absTomorrow;
	char absTonight;
	char absUnspecified;
	char absYesterday;
	char timeFrequency; // hourly/daily/weekly etc
	short absHoliday;
	bool metaDescriptive;  // she described the events of last Tuesday.

	cTimeInfo()
	{
		clear();
	}
	bool empty()
	{
		return absYear==-1 && absSeason==-1 && absDateSpec==-1 && absMonth==-1 && 
					 absDayOfWeek==-1 && absDayOfMonth==-1 && timeOfDay==-1 && absHour==-1 && 
					 absMinute==-1 && absSecond==-1 && absTimeSpec==-1 && timeFrequency<=-1 && absHoliday==-1;
	}
	cTimeInfo(char *buffer,int &where,unsigned int total,bool &error)
  {
    if (error=!copy(tWhere,buffer,where,total)) return;
    if (error=!copy(timePreviousLink,buffer,where,total)) return;
    if (error=!copy(timeSPTAnchor,buffer,where,total)) return;
    if (error=!copy(timeETAnchor,buffer,where,total)) return;
    if (error=!copy(timeRTAnchor,buffer,where,total)) return;
		int tmp;
    if (error=!copy(tmp,buffer,where,total)) return;
		timeRelationType=(eTimeWordFlags)tmp;
    if (error=!copy(timeModifier,buffer,where,total)) return;
    if (error=!copy(timeModifier2,buffer,where,total)) return;
    if (error=!copy(absMetaRelation,buffer,where,total)) return;
    if (error=!copy(tmp,buffer,where,total)) return;
		timeCapacity=(eCapacity)tmp;
    if (error=!copy(absYear,buffer,where,total)) return;
		if (error=!copy(absSeason,buffer,where,total)) return;
		if (error=!copy(absDateSpec,buffer,where,total)) return; // A.D. B.C.
		if (error=!copy(absMonth,buffer,where,total)) return;
		if (error=!copy(absDayOfWeek,buffer,where,total)) return;
		if (error=!copy(absDayOfMonth,buffer,where,total)) return;
		if (error=!copy(timeOfDay,buffer,where,total)) return; // morning/evening/noon etc
		if (error=!copy(absHour,buffer,where,total)) return;
		if (error=!copy(absMinute,buffer,where,total)) return;
		if (error=!copy(absSecond,buffer,where,total)) return;
		if (error=!copy(absTimeSpec,buffer,where,total)) return; // A.M. P.M.
		if (error=!copy(timeFrequency,buffer,where,total)) return; // daily
		char md;
		if (error=!copy(md,buffer,where,total)) return; 
		metaDescriptive=(md!=0);
		if (error=!copy(absHoliday,buffer,where,total)) return; 
		if (error = !copy(absMoment, buffer, where, total)) return;
		if (error = !copy(absNamedDay, buffer, where, total)) return;
		if (error = !copy(absNamedHoliday, buffer, where, total)) return;
		if (error = !copy(absNamedMonth, buffer, where, total)) return;
		if (error = !copy(absNamedSeason, buffer, where, total)) return;
		if (error = !copy(absToday, buffer, where, total)) return;
		if (error = !copy(absTomorrow, buffer, where, total)) return;
		if (error = !copy(absTonight, buffer, where, total)) return;
		if (error = !copy(absUnspecified, buffer, where, total)) return;
		if (error = !copy(absYesterday, buffer, where, total)) return;
  }
  bool write(void *buffer,int &where,int limit)
  {
    if (!copy(buffer,tWhere,where,limit)) return false;
    if (!copy(buffer,timePreviousLink,where,limit)) return false;
    if (!copy(buffer,timeSPTAnchor,where,limit)) return false;
    if (!copy(buffer,timeETAnchor,where,limit)) return false;
    if (!copy(buffer,timeRTAnchor,where,limit)) return false;
    if (!copy(buffer,(int)timeRelationType,where,limit)) return false;
    if (!copy(buffer,timeModifier,where,limit)) return false;
    if (!copy(buffer,timeModifier2,where,limit)) return false;
    if (!copy(buffer,absMetaRelation,where,limit)) return false;
    if (!copy(buffer,(int)timeCapacity,where,limit)) return false;
    if (!copy(buffer,absYear,where,limit)) return false;
		if (!copy(buffer,absSeason,where,limit)) return false;
		if (!copy(buffer,absDateSpec,where,limit)) return false; // A.D. B.C.
		if (!copy(buffer,absMonth,where,limit)) return false;
		if (!copy(buffer,absDayOfWeek,where,limit)) return false;
		if (!copy(buffer,absDayOfMonth,where,limit)) return false;
		if (!copy(buffer,timeOfDay,where,limit)) return false; // morning/evening/noon etc
		if (!copy(buffer,absHour,where,limit)) return false;
		if (!copy(buffer,absMinute,where,limit)) return false;
		if (!copy(buffer,absSecond,where,limit)) return false;
		if (!copy(buffer,absTimeSpec,where,limit)) return false; // A.M. P.M.
		if (!copy(buffer,timeFrequency,where,limit)) return false; // daily
		char md=(metaDescriptive) ? 1 : 0;
		if (!copy(buffer,md,where,limit)) return false; // the events of the last two days
		if (!copy(buffer,absHoliday,where,limit)) return false; 
		if (!copy(buffer, absMoment, where, limit)) return false;
		if (!copy(buffer, absNamedDay, where, limit)) return false;
		if (!copy(buffer, absNamedHoliday, where, limit)) return false;
		if (!copy(buffer, absNamedMonth, where, limit)) return false;
		if (!copy(buffer, absNamedSeason, where, limit)) return false;
		if (!copy(buffer, absToday, where, limit)) return false;
		if (!copy(buffer, absTomorrow, where, limit)) return false;
		if (!copy(buffer, absTonight, where, limit)) return false;
		if (!copy(buffer, absUnspecified, where, limit)) return false;
		if (!copy(buffer, absYesterday, where, limit)) return false;
    return true;
  }

	void clear()
	{
		tWhere=-1;
		timePreviousLink=-1;
		timeSPTAnchor=-1; // speech time
		timeETAnchor=-1; // event time
		timeRTAnchor=-1; // reference time
		/*
		   IT[p.m.] was 2 p.m. on the afternoon of May 7 , 1915
		     T_AT CHAIN modifier = 14 capacity=cHour, modifier = 1915 capacity=cYear, modifier = 5 capacity=cMonth, modifier = 7 capacity=cDay
		   129: She had noticed 
		     timeRelationType=T_BEFORE modifier=-1 capacity=cUnspecified
		   199: now
		     timeRelationType=T_ASSUME_SEQUENTIAL modifier=-1 capacity=cUnspecified
		   Will MOVE_OBJECTyou[girl] take them[papers,more,woman] ?
		     timeRelationType=T_AFTER modifier=-1 capacity=cUnspecified
		   ESTAB I[man] shall advertise in the personal column of the Times , beginning ‘QS Shipmate . ’ 
		     timeRelationType=T_AFTER modifier=-1 capacity=cUnspecified
		   At the end of three days if there ishas nothing
		     timeRelationType=T_AFTER modifier=3 capacity=cMultiDays

		// with a new completely unrelated speakergroup, start a new timeRelations
		// The two young people[tuppence,tommy] greeted each other affectionately , and momentarily ESTABblocked the Dover Street Tube exit in doing so
		// timeRelationType=T_VAGUE modifier=-1 capacity=cUnspecified
		// 1. specific time or a specific relative time
		// 2. time order (one event happening before another)
		// 3. time direction (one event happening in the past, is happening now or will happen in the future)
		// 4. recurring time
		*/
		timeRelationType=T_ASSUME_SEQUENTIAL; // T_BEFORE, T_AFTER - this can be a chain (The store is closed from July to August.) ( 2 p.m. on the afternoon of May 7 , 1915)
		// include also Points:  ON_OR_BEFORE, ON_OR_AFTER,  Durations: EQUAL_OR_LESS, EQUAL_OR_MORE
		timeModifier=-1; // early in the evening/ a few hours after 
		timeModifier2=-1; // secondary modifier
		absMetaRelation=-1; // a time relation between two separate relations / he left after I spoke.
		timeCapacity=cUnspecified; // day/hour/evening etc
    absYear=-1;
		absSeason=-1;
		absDateSpec=-1; // A.D. B.C.
		absMonth=-1;
		absDayOfWeek=-1;
		absDayOfMonth=-1;
		timeOfDay=-1; // morning/evening/noon etc
		absHour=-1;
		absMinute=-1;
		absSecond=-1;
		absTimeSpec=-1; // A.M. P.M.
		timeFrequency=-1;
		metaDescriptive=false;
		absHoliday=-1;
	}
	// only if not -1
	void af(wchar_t *name,int field,wstring &appendStr)
	{
		if (field!=-1)
			itos(name,field,appendStr,L" ");
	}
	wstring toString(vector <WordMatch> &m,wstring &tmpstr);
};