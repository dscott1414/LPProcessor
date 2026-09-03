"""TimeInfo.py - One temporal annotation (C++ cTimeInfo) plus formatters.

Overview:
    Capacity constants cMillenium..cUnspecified match the C++ time
    categories. __init__ unpacks a 66-byte record. Helper methods
    classify relative vs absolute time, format flag words, and
    compute a 0–7 time-progression label for a Relation (advances /
    state / new time / past / future / recurring).

Pipeline position:
    Embedded in Relation.timeInfo[]; used when the web backend
    renders the timeline.

Key entry points:
    - relative_time / absolute_time
    - time_string / capacity_string / time_progression_string
    - determine_time_progression(source, r)
    - to_string / to_string_2
    - __init__(rs)

Notes / gotchas:
    (fixed) Several helpers used Java APIs (.length, .contains, .equals)
    on Python sets/strs, which raised at runtime; and several lookup
    tables (ws/ws2 in time_string, ws in capacity_string, seasons/
    daysOfWeek/months/timeFrequencies/timeFrequenciesValues in
    to_string_2) were {} set literals indexed like arrays, which raised
    TypeError ('set' object is not subscriptable) - all converted to
    list literals in the same order and indexed with len(). capacity_string's
    lookup list also had a stray extra "morrow" entry after "Tomorrow"
    that shifted every later index out of alignment with the
    cTonight..cUnspecified constants above (e.g. cNamedHoliday=29 would
    have resolved to "NamedMonth", and cUnspecified=30 was unreachable);
    removed to restore 1:1 alignment. to_string also called
    r.timeInfo[I].to_string(source) where TimeInfo only defines
    to_string_2(self, source) - the loop now calls to_string_2 as the
    inline comment already said it should.
    None of to_string / determine_time_progression / to_string_2 /
    time_string / capacity_string is reachable today regardless of this
    fix: their only outside callers, in Source.py (not in this batch's
    file list), invoke TimeInfo.to_string(self, r) and
    TimeInfo.determine_time_progression(self, self.relations[tl]) as
    unbound calls missing the required third argument, which raises
    TypeError before TimeInfo's own code ever runs. Left as a documented
    risk in Source.py, out of scope for this pass.
"""
from WordClass import WordClass
from TFI import TFI
import json
from json import JSONEncoder
import struct

class TimeInfo:
    cMillenium = 0
    cCentury = 1
    cDecade = 2
    cYear = 3
    cSemester = 4
    cSeason = 5
    cQuarter = 6
    cMonth = 7
    cWeek = 8
    cDay = 9
    cHour = 10
    cMinute = 11
    cSecond = 12
    cMoment = 13
    cMorning = 14
    cNoon = 15
    cAfternoon = 16
    cEvening = 17
    cDusk = 18
    cNight = 19
    cMidnight = 20
    cDawn = 21
    cTonight = 22
    cToday = 23
    cTomorrow = 24
    cYesterday = 25
    cNamedMonth = 26
    cNamedDay = 27
    cNamedSeason = 28
    cNamedHoliday = 29
    cUnspecified = 30

    # True if not year-absolute but some abs* field or a specified capacity
    # is set (season/month/dow/dom/hour/minute/holiday).
    def relative_time(self):
        if (self.absolute_time()):
            return False
        if (self.absSeason>=0 or self.absMonth>=0 or self.absDayOfWeek>=0 or
                self.absDayOfMonth>=0 or    self.absHour>=0 or    self.absMinute>=0 or    self.absHoliday>=0):
            return True
        if (self.timeCapacity!=self.cUnspecified):
            return True
        return False
        
    # True if absYear was filled (>=0).
    def absolute_time(self):
        if self.absYear>=0:
            return True
        return False
        
    # Decode low 5 bits as a relation word (SEQ/before/…) plus bits 5–9
    # as time/date/vague/length/cardtime.
    def time_string(self,timeWordFlags):
        ws = [ "SEQ","before","after","present","throughout","recurring","at","midway",
                "in","on","interval","start","stop","resume","finish","range","meta","unit"]
        ws2 = [    " time"," date"," vague"," length"," cardtime" ]
        if ((timeWordFlags&31)<len(ws)):
            s=ws[timeWordFlags&31]
        else:
            s=""+str(timeWordFlags)
        for I in range(5,10):
            if ((timeWordFlags&(1<<I))>0):
                s += ws2[I-5]
        return s

    # Map a cMillenium..cUnspecified code to its name.
    def capacity_string(self,capacityFlags):
        ws = [ "Millenium","Century","Decade","Year","Semester","Season","Quarter","Month","Week","Day",
                "Hour","Minute","Second","Moment",
                "Morning","Noon","Afternoon","Evening","Dusk","Night","Midnight","Dawn",
                "Tonight","Today","Tomorrow","Yesterday",
                "NamedMonth","NamedDay","NamedSeason","NamedHoliday",
                "Unspecified" ]
        if (capacityFlags>=len(ws) or capacityFlags<=-1):
            return ""+str(capacityFlags)
        return ws[capacityFlags]

    # Prefix-equal of the shorter string (Python-correct, unlike VerbNet.like).
    def like(self, str1,str2):
        minLength = min(len(str1),len(str2))
        return str1[:minLength] == str2[:minLength]

    # True if any VerbNet class name of the verb at `where` is a prefix
    # of verbClass (or vice versa).
    def is_verb_class(self, source, where, verbClass):
        vms = source.get_verb_classes(where)
        if vms != None:
            for vm in vms:
                if self.like(vm.name,verbClass):
                    return True
        return False

    # "[<progression>] " plus each r.timeInfo[I].to_string_2.
    def to_string(self,source,r):
        timeInfo="      [";
        timeInfo += self.determine_time_progression(source,r)+"] ";
        for I in range(len(r.timeInfo)):
            timeInfo += r.timeInfo[I].to_string_2(source);
        return timeInfo;
        
    # Label for determine_time_progression codes 0–7. adv is a set.
    def time_progression_string(self, tp):
        adv = [ "advances", "not advances - state", "new time", "new relative time", "extended - not advancing", "past", "future", "recurring"]
        return adv[tp]
        
    # Classify relation r's tense/verb-class/timeInfo into 0–7 (see
    # comments in the body) and return the label string.
    def determine_time_progression(self,source,r):
        tp=0;
        if (r.presentHappening and r.whereVerb>=0):
            tp=4 if ((source.m[r.whereVerb].quoteForwardLink&source.VT_EXTENDED)==source.VT_EXTENDED) else 0;
            if (self.is_verb_class(source,r.whereVerb,"am")):
                if (tp==4 and r.relationType==source.stCONTACT):
                    tp=0;
                else:
                    tp=1;
            tfi=WordClass.words.get(source.m[r.whereVerb].baseVerb)
            if (tfi!=None and ((tfi.flags&TFI.stateVerb)!=0)):
                tp=1;
            if (r.establishingLocation):
                tp=2;
            for I in range(len(r.timeInfo)):
                if (r.timeInfo[I].relative_time() and tp!=2):
                    tp=3;
                elif (r.timeInfo[I].absolute_time()):
                    tp=2;
        if (r.beforePastHappening or r.pastHappening):
            tp=5;
        if (r.futureHappening or r.futureInPastHappening):
            tp=6;
        for I in range (len(r.timeInfo)):
            if (r.timeInfo[I].timeRelationType==source.T_RECURRING):
                tp=7;
        # 0 - advances a action based amount of time from the last statement / He brushed his teeth.   [ +15 minutes? ]
        # 1 - still keeping the current time flow but not advancing the action / He was fat / He preferred the ham.
        # 2 - establishes a new time not related to the previous time flow / At Christmas, they opened fifty presents - each.
        # 3 - establishes a new time relative to the current time flow / after two weeks, he was tired. / He ran for two hours.
        # 4 - an action that describes something happening up to and including the moment ; not advancing time / He was running past the stop sign. / He was driving his new car.
        # 5 - an action that describes something that happened or may have happened in the past ; not advancing time / He had wandered down that road many times
        # 6 - an action that describes something that may or will happen in the future ; not advancing time / She will pay for this! / I might run for office
        # 7 - an action that describes recurring action ; not advancing the current time flow / I always visit her / I drive the van every Monday / She plays soccer twice a week.
        return self.time_progression_string(tp)

    # Human-readable dump of this TimeInfo against source.m[].
    def to_string_2(self,source):
            timeInfo="";
            timeInfo += str(self.tWhere)+":";
            if (self.timeModifier>0):
                timeInfo += "Modifier="+source.m[self.timeModifier].word+" ";
            if (self.timeModifier2>0):
                timeInfo += "Modifier2="+source.m[self.timeModifier2].word+" ";
            if (self.absMetaRelation>0):
                timeInfo += "absMetaRelation="+str(self.absMetaRelation)+" ";
            if (self.timeETAnchor>0):
                timeInfo += "ET="+str(self.timeETAnchor)+" ";
            #if (timeRTAnchor>0)
            #    timeInfo += "RT="+timeRTAnchor+" ";
            if (self.timeSPTAnchor>0):
                timeInfo += "SPT="+str(self.timeSPTAnchor)+" ";
            if (self.timeRelationType>0):
                timeInfo += "RelType="+self.time_string(self.timeRelationType)+" ";
            tc=self.capacity_string(self.timeCapacity);
            if ("Named" in tc and self.timeRTAnchor>=0):
                timeInfo += "Capacity="+tc.replace("Named", "")+"["+source.m[self.timeRTAnchor].word+"] ";
            elif (self.timeCapacity>0 and tc!="Unspecified"):
                timeInfo += "Capacity="+tc+" ";
            seasons = ["winter","wintertime","spring","springtime","summer","summertime","fall",
                            "winters","wintertimes","springs","springtimes","summers","summertimes","falls"];
            daysOfWeek = ["sunday","monday","tuesday","wednesday","thursday","friday","saturday","weekend",
           "sundays","mondays","tuesdays","wednesdays","thursdays","fridays","saturdays","weekends"];
            if (self.absDayOfWeek>=0):
                timeInfo += "absDayOfWeek="+str(self.absDayOfWeek)+"["+daysOfWeek[self.absDayOfWeek]+"] ";
            timeFrequenciesValues = [self.cHour, self.cDay, self.cWeek, self.cMonth, self.cQuarter, self.cSeason, self.cYear,self.cYear, -2, -3, -4, -5, -6, -7, self.cUnspecified];
            timeFrequencies = ["hourly","daily","weekly","monthly","quarterly","seasonally","yearly","annually","twice","thrice","once","times","every","each",""];
            if (self.absSeason>=0):
                timeInfo += "absSeason="+str(self.absSeason)+"["+ seasons[self.absSeason]+"] ";
            if (self.absDateSpec>=0):
                timeInfo += "absDateSpec="+str(self.absDateSpec)+" ";
            months = ["january","february","march","april","may","june","july","august","september","october","november","december"];
            if (self.absMonth>=0):
                timeInfo += "absMonth="+str(self.absMonth)+"["+months[self.absMonth]+"] ";
            if (self.absDayOfMonth>=0):
                timeInfo += "absDayOfMonth="+str(self.absDayOfMonth)+" ";
            if (self.absYear>=0):
                timeInfo += "absYear="+str(self.absYear)+" ";
            if (self.timeOfDay>=0):
                timeInfo += "timeOfDay="+str(self.timeOfDay)+" ";
            if (self.absHour>=0):
                timeInfo += "absHour="+str(self.absHour)+" ";
            if (self.absMinute>=0):
                timeInfo += "absMinute="+str(self.absMinute)+" ";
            if (self.absSecond>=0):
                timeInfo += "absSecond="+str(self.absSecond)+" ";
            if (self.absTimeSpec>=0):
                timeInfo += "absTimeSpec="+str(self.absTimeSpec)+" ";
            if self.timeFrequency!=-1:
                tOffset=0;
                for ti in timeFrequenciesValues:
                    if ti==self.timeFrequency:
                        break;
                    else:
                        tOffset += 1
                if (tOffset<len(timeFrequencies) and len(timeFrequencies[tOffset])>0):
                    timeInfo += ("frequency=" + (timeFrequencies[tOffset]+" ")) if (tOffset<len(timeFrequencies)) else (str(self.timeFrequency)+" ");
            if (self.absHoliday>=0):
                timeInfo += "Holiday="+str(self.absHoliday)+" "
            if (self.metaDescriptive):
                timeInfo += "metaDescriptive="+str(self.metaDescriptive)+" "
            timeInfo += "]";
            return timeInfo;

    # Unpack 10 ints + 1 short + 12 bytes + 1 short + 10 bytes (66 bytes).
    def __init__(self,rs):
        self.tWhere, self.timePreviousLink, self.timeSPTAnchor, self.timeETAnchor, self.timeRTAnchor, \
        self.timeRelationType, self.timeModifier, self.timeModifier2, self.absMetaRelation, self.timeCapacity, \
        self.absYear, self.absSeason, self.absDateSpec, self.absMonth, self.absDayOfWeek, \
        self.absDayOfMonth, self.timeOfDay, self.absHour, self.absMinute, self.absSecond, \
        self.absTimeSpec, self.timeFrequency, self.metaDescriptive, self.absHoliday, self.absMoment, \
        self.absNamedDay, self.absNamedHoliday, self.absNamedMonth, self.absNamedSeason, self.absToday, \
        self.absTomorrow, self.absTonight, self.absUnspecified, self.absYesterday = struct.unpack('<10ih12bh10b', rs.f.read(66))        
        
class TimeInfoEncoder(JSONEncoder):
        # json.dumps fallback: emit the instance __dict__.
        def default(self, o):
            return o.__dict__
