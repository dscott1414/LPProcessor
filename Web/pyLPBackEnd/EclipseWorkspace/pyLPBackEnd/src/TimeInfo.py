from WordClass import WordClass
from TFI import TFI
import json
from json import JSONEncoder

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

    def relativeTime(self):
        if (self.absoluteTime()):
            return False
        if (self.absSeason>=0 or self.absMonth>=0 or self.absDayOfWeek>=0 or
                self.absDayOfMonth>=0 or    self.absHour>=0 or    self.absMinute>=0 or    self.absHoliday>=0):
            return True
        if (self.timeCapacity!=self.cUnspecified):
            return True
        return False
        
    def absoluteTime(self):
        if self.absYear>=0:
            return True
        return False
        
    def timeString(self,timeWordFlags):
        ws = { "SEQ","before","after","present","throughout","recurring","at","midway",
                "in","on","interval","start","stop","resume","finish","range","meta","unit"}
        ws2 = {    " time"," date"," vague"," length"," cardtime" }
        if ((timeWordFlags&31)<ws.length):
            s=ws[timeWordFlags&31]
        else:
            s=""+timeWordFlags
        for I in range(5,10):
            if ((timeWordFlags&(1<<I))>0):
                s += ws2[I-5]
        return s

    def capacityString(self,capacityFlags):
        ws = { "Millenium","Century","Decade","Year","Semester","Season","Quarter","Month","Week","Day",
                "Hour","Minute","Second","Moment",
                "Morning","Noon","Afternoon","Evening","Dusk","Night","Midnight","Dawn",
                "Tonight","Today","Tomorrow","morrow","Yesterday",
                "NamedMonth","NamedDay","NamedSeason","NamedHoliday",
                "Unspecified" }
        if (capacityFlags>=ws.length or capacityFlags<=-1):
            return ""+capacityFlags
        return ws[capacityFlags]

    def like(self, str1,str2):
        minLength = min(len(str1),len(str2))
        return str1[:minLength] == str2[:minLength]

    def isVerbClass(self, source, where, verbClass):
        vms = source.getVerbClasses(where)
        if vms != None:
            for vm in vms:
                if self.like(vm.name,verbClass):
                    return True
        return False

    def toString(self,source,r):
        timeInfo="      [";
        timeInfo += self.determineTimeProgression(source,r)+"] ";
        for I in range(r.timeInfo.length):
            timeInfo += r.timeInfo[I].toString(source);
        return timeInfo;
        
    def timeProgressionString(self, tp):
        adv = [ "advances", "not advances - state", "new time", "new relative time", "extended - not advancing", "past", "future", "recurring"]
        return adv[tp]
        
    def determineTimeProgression(self,source,r):
        tp=0;
        if (r.presentHappening and r.whereVerb>=0):
            tp=4 if ((source.m[r.whereVerb].quoteForwardLink&source.VT_EXTENDED)==source.VT_EXTENDED) else 0;
            if (self.isVerbClass(source,r.whereVerb,"am")):
                if (tp==4 and r.relationType==source.stCONTACT):
                    tp=0;
                else:
                    tp=1;
            tfi=WordClass.words.get(source.m[r.whereVerb].baseVerb)
            if (tfi!=None and ((tfi.flags&TFI.stateVerb)!=0)):
                tp=1;
            if (r.establishingLocation):
                tp=2;
            for I in range(r.timeInfo.length):
                if (r.timeInfo[I].relativeTime() and tp!=2):
                    tp=3;
                elif (r.timeInfo[I].absoluteTime()):
                    tp=2;
        if (r.beforePastHappening or r.pastHappening):
            tp=5;
        if (r.futureHappening or r.futureInPastHappening):
            tp=6;
        for I in range (r.timeInfo.length):
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
        return self.timeProgressionString(tp)

    def toString2(self,source):
            timeInfo="";
            timeInfo += self.tWhere+":";
            if (self.timeModifier>0):
                timeInfo += "Modifier="+source.m[self.timeModifier].word+" ";
            if (self.timeModifier2>0):
                timeInfo += "Modifier2="+source.m[self.timeModifier2].word+" ";
            if (self.absMetaRelation>0):
                timeInfo += "absMetaRelation="+self.absMetaRelation+" ";
            if (self.timeETAnchor>0):
                timeInfo += "ET="+self.timeETAnchor+" ";
            #if (timeRTAnchor>0)
            #    timeInfo += "RT="+timeRTAnchor+" ";
            if (self.timeSPTAnchor>0):
                timeInfo += "SPT="+self.timeSPTAnchor+" ";
            if (self.timeRelationType>0):
                timeInfo += "RelType="+self.timeString(self.timeRelationType)+" ";
            tc=self.capacityString(self.timeCapacity);
            if (tc.contains("Named") and self.timeRTAnchor>=0):
                timeInfo += "Capacity="+tc.replace("Named", "")+"["+source.m[self.timeRTAnchor].word+"] ";
            elif (self.timeCapacity>0 and not tc.equals("Unspecified")):
                timeInfo += "Capacity="+tc+" ";
            seasons = {"winter","wintertime","spring","springtime","summer","summertime","fall",
                            "winters","wintertimes","springs","springtimes","summers","summertimes","falls"};
            daysOfWeek = {"sunday","monday","tuesday","wednesday","thursday","friday","saturday","weekend",
           "sundays","mondays","tuesdays","wednesdays","thursdays","fridays","saturdays","weekends"};
            if (self.absDayOfWeek>=0):
                timeInfo += "absDayOfWeek="+self.absDayOfWeek+"["+daysOfWeek[self.absDayOfWeek]+"] ";
            timeFrequenciesValues = {self.cHour, self.cDay, self.cWeek, self.cMonth, self.cQuarter, self.cSeason, self.cYear,self.cYear, -2, -3, -4, -5, -6, -7, self.cUnspecified};
            timeFrequencies = {"hourly","daily","weekly","monthly","quarterly","seasonally","yearly","annually","twice","thrice","once","times","every","each",""};
            if (self.absSeason>=0):
                timeInfo += "absSeason="+self.absSeason+"["+ seasons[self.absSeason]+"] ";
            if (self.absDateSpec>=0):
                timeInfo += "absDateSpec="+self.absDateSpec+" ";
            months = {"january","february","march","april","may","june","july","august","september","october","november","december"};
            if (self.absMonth>=0):
                timeInfo += "absMonth="+self.absMonth+"["+months[self.absMonth]+"] ";
            if (self.absDayOfMonth>=0):
                timeInfo += "absDayOfMonth="+self.absDayOfMonth+" ";
            if (self.absYear>=0):
                timeInfo += "absYear="+self.absYear+" ";
            if (self.timeOfDay>=0):
                timeInfo += "timeOfDay="+self.timeOfDay+" ";
            if (self.absHour>=0):
                timeInfo += "absHour="+self.absHour+" ";
            if (self.absMinute>=0):
                timeInfo += "absMinute="+self.absMinute+" ";
            if (self.absSecond>=0):
                timeInfo += "absSecond="+self.absSecond+" ";
            if (self.absTimeSpec>=0):
                timeInfo += "absTimeSpec="+self.absTimeSpec+" ";
            if self.timeFrequency!=-1:
                tOffset=0;
                for ti in timeFrequenciesValues:
                    if ti==self.timeFrequency:
                        break;
                    else:
                        tOffset += 1
                if (tOffset<timeFrequencies.length and timeFrequencies[tOffset].length()>0):
                    timeInfo += "frequency=" + (timeFrequencies[tOffset]+" ") if (tOffset<timeFrequencies.length) else self.timeFrequency+" ";
            if (self.absHoliday>=0):
                timeInfo += "Holiday="+self.absHoliday+" "
            if (self.metaDescriptive):
                timeInfo += "metaDescriptive="+self.metaDescriptive+" "
            timeInfo += "]";
            return timeInfo;

    def __init__(self,rs):
        self.tWhere = rs.readInteger();
        self.timePreviousLink = rs.readInteger();
        self.timeSPTAnchor = rs.readInteger();
        self.timeETAnchor = rs.readInteger();
        self.timeRTAnchor = rs.readInteger();
        self.timeRelationType = rs.readInteger();
        self.timeModifier = rs.readInteger();
        self.timeModifier2 = rs.readInteger();
        self.absMetaRelation = rs.readInteger();
        self.timeCapacity = rs.readInteger();
        self.absYear = rs.readShort();
        self.absSeason = rs.readByte();
        self.absDateSpec = rs.readByte(); # A.D. B.C.
        self.absMonth = rs.readByte();
        self.absDayOfWeek = rs.readByte();
        self.absDayOfMonth = rs.readByte();
        self.timeOfDay = rs.readByte(); # morning/evening/noon etc
        self.absHour = rs.readByte();
        self.absMinute = rs.readByte();
        self.absSecond = rs.readByte();
        self.absTimeSpec = rs.readByte(); # A.M. P.M.
        self.timeFrequency = rs.readByte(); # daily
        self.metaDescriptive = rs.readByte()!=0;
        self.absHoliday= rs.readShort();
        self.absMoment = rs.readByte();
        self.absNamedDay = rs.readByte();
        self.absNamedHoliday = rs.readByte();
        self.absNamedMonth = rs.readByte();
        self.absNamedSeason = rs.readByte();
        self.absToday = rs.readByte();
        self.absTomorrow = rs.readByte();
        self.absTonight = rs.readByte();
        self.absUnspecified = rs.readByte();
        self.absYesterday = rs.readByte();
        
        
class TimeInfoEncoder(JSONEncoder):
        def default(self, o):
            return o.__dict__
