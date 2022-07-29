package show;

import java.util.Vector;
	public class cTimeInfo {
		int tWhere;  
		int timePreviousLink; // a link to the previous statement (by time) 
		int timeSPTAnchor; // speech time
		int timeETAnchor; // event time
		int timeRTAnchor; // reference time
		int  timeRelationType; // T_BEFORE, T_AFTER
		int timeModifier; // early in the evening/ a few hours after - this can be a chain (The store is closed from July to August.) ( 2 p.m. on the afternoon of May 7 , 1915)
		int timeModifier2; // secondary modifier
		int absMetaRelation;
		int  timeCapacity; // day/hour/evening etc
		short absYear;
		byte absSeason;
		byte absDateSpec; // A.D. B.C.
		byte absMonth;
		byte absDayOfWeek;
		byte absDayOfMonth;
		byte timeOfDay; // morning/evening/noon etc
		byte absHour;
		byte absMinute;
		byte absSecond;
		byte absTimeSpec; // A.M. P.M.
		byte timeFrequency;
		boolean metaDescriptive;
		short absHoliday;
		byte absMoment;
		byte absNamedDay;
		byte absNamedHoliday;
		byte absNamedMonth;
		byte absNamedSeason;
		byte absToday;
		byte absTomorrow;
		byte absTonight;
		byte absUnspecified;
		byte absYesterday;


		public final static int cMillenium = 0;
		public final static int cCentury = 1;
		public final static int cDecade = 2;
		public final static int cYear = 3;
		public final static int cSemester = 4;
		public final static int cSeason = 5;
		public final static int cQuarter = 6;
		public final static int cMonth = 7;
		public final static int cWeek = 8;
		public final static int cDay = 9;
		public final static int cHour = 10;
		public final static int cMinute = 11;
		public final static int cSecond = 12;
		public final static int cMoment = 13;
		public final static int cMorning = 14;
		public final static int cNoon = 15;
		public final static int cAfternoon = 16;
		public final static int cEvening = 17;
		public final static int cDusk = 18;
		public final static int cNight = 19;
		public final static int cMidnight = 20;
		public final static int cDawn = 21;
		public final static int cTonight = 22;
		public final static int cToday = 23;
		public final static int cTomorrow = 24;
		public final static int cYesterday = 25;
		public final static int cNamedMonth = 26;
		public final static int cNamedDay = 27;
		public final static int cNamedSeason = 28;
		public final static int cNamedHoliday = 29;
		public final static int cUnspecified = 30;

		boolean relativeTime()
		{
			if (absoluteTime())
				return false;
			if (absSeason>=0 || absMonth>=0 || absDayOfWeek>=0 ||
					absDayOfMonth>=0 ||	absHour>=0 ||	absMinute>=0 ||	absHoliday>=0)
				return true;
			if (timeCapacity!=cUnspecified)
				return true;
			return false;
		}
		
		boolean absoluteTime()
		{
			if (absYear>=0)
				return true;
			return false;
		}
		
		String timeString(int timeWordFlags)
		{
			String ws[]={ "SEQ","before","after","present","throughout","recurring","at","midway",
					"in","on","interval","start","stop","resume","finish","range","meta","unit"};
			String ws2[]={	" time"," date"," vague"," length"," cardtime" };
			String s;
			if ((timeWordFlags&31)<ws.length)
				s=ws[timeWordFlags&31];
			else
				s=""+timeWordFlags;
			for (int I=5; I<10; I++)
				if ((timeWordFlags&(1<<I))>0)
			    s+=ws2[I-5];
			return s;
		}

		String capacityString(int capacityFlags)
		{
		 String ws[]={ "Millenium","Century","Decade","Year","Semester","Season","Quarter","Month","Week","Day",
				 "Hour","Minute","Second","Moment",
				 "Morning","Noon","Afternoon","Evening","Dusk","Night","Midnight","Dawn",
				 "Tonight","Today","Tomorrow","morrow","Yesterday",
				 "NamedMonth","NamedDay","NamedSeason","NamedHoliday",
				 "Unspecified" };
		 if (capacityFlags>=ws.length || capacityFlags<=-1)
			 return ""+capacityFlags;
		 return ws[capacityFlags];
		}

		static String toString(Source source,Relation r)
		{
			String timeInfo="      [";
			
			timeInfo+=determineTimeProgression(source,r)+"] ";
			for (int I=0; I<r.timeInfo.length; I++)
				timeInfo+=r.timeInfo[I].toString(source);
			return timeInfo;
		}
		
		static String timeProgressionString(int tp)
		{
			switch (tp)
			{
			case 0: return "advances";
			case 1: return "not advances - state";
			case 2: return "new time";
			case 3: return "new relative time";
			case 4: return "extended - not advancing";
			case 5: return "past";
			case 6: return "future";
			case 7: return "recurring";
			}
			return "unknown";
		}
		
		static String determineTimeProgression(Source source,Relation r)
		{
			int tp=0;
			if (r.presentHappening && r.whereVerb>=0)
			{
				tp=((source.m[r.whereVerb].quoteForwardLink&Source.VT_EXTENDED)==Source.VT_EXTENDED) ? 4 : 0;
				if (source.isVerbClass(r.whereVerb,"am"))
				{
					if (tp==4 && r.relationType==Source.stCONTACT)
						tp=0;
					else
						tp=1;
				}
				tFI tfi=WordClass.Words.get(source.m[r.whereVerb].baseVerb);
				if (tfi!=null && ((tfi.flags&tFI.stateVerb)!=0))
					tp=1;
				if (r.establishingLocation)
					tp=2;
				for (int I=0; I<r.timeInfo.length; I++)
					if (r.timeInfo[I].relativeTime() && tp!=2)
						tp=3;
					else if (r.timeInfo[I].absoluteTime())
						tp=2;
			}
			if (r.beforePastHappening || r.pastHappening)
				tp=5;
			if (r.futureHappening || r.futureInPastHappening)
				tp=6;
			for (int I=0; I<r.timeInfo.length; I++)
				if (r.timeInfo[I].timeRelationType==Source.T_RECURRING)
					tp=7;
			// 0 - advances a action based amount of time from the last statement / He brushed his teeth.   [ +15 minutes? ]
			// 1 - still keeping the current time flow but not advancing the action / He was fat / He preferred the ham.
			// 2 - establishes a new time not related to the previous time flow / At Christmas, they opened fifty presents - each.
			// 3 - establishes a new time relative to the current time flow / after two weeks, he was tired. / He ran for two hours.
			// 4 - an action that describes something happening up to and including the moment ; not advancing time / He was running past the stop sign. / He was driving his new car.
			// 5 - an action that describes something that happened or may have happened in the past ; not advancing time / He had wandered down that road many times
			// 6 - an action that describes something that may or will happen in the future ; not advancing time / She will pay for this! / I might run for office
			// 7 - an action that describes recurring action ; not advancing the current time flow / I always visit her / I drive the van every Monday / She plays soccer twice a week.
			return timeProgressionString(tp);
		}

		public String toString(Source source)
		{
			String timeInfo="";
			timeInfo+=tWhere+":";
			if (timeModifier>0)
				timeInfo+="Modifier="+source.m[timeModifier].word+" ";
			if (timeModifier2>0)
				timeInfo+="Modifier2="+source.m[timeModifier2].word+" ";
			if (absMetaRelation>0)
				timeInfo+="absMetaRelation="+absMetaRelation+" ";
			if (timeETAnchor>0)
				timeInfo+="ET="+timeETAnchor+" ";
			//if (timeRTAnchor>0)
			//	timeInfo+="RT="+timeRTAnchor+" ";
			if (timeSPTAnchor>0)
				timeInfo+="SPT="+timeSPTAnchor+" ";
			if (timeRelationType>0)
				timeInfo+="RelType="+timeString(timeRelationType)+" ";
			String tc=capacityString(timeCapacity);
			if (tc.contains("Named") && timeRTAnchor>=0)
				timeInfo+="Capacity="+tc.replace("Named", "")+"["+source.m[timeRTAnchor].word+"] ";
			else if (timeCapacity>0 && !tc.equals("Unspecified"))
				timeInfo+="Capacity="+tc+" ";
			String seasons[] = {"winter","wintertime","spring","springtime","summer","summertime","fall",
							"winters","wintertimes","springs","springtimes","summers","summertimes","falls"};
			String daysOfWeek[] = {"sunday","monday","tuesday","wednesday","thursday","friday","saturday","weekend",
           "sundays","mondays","tuesdays","wednesdays","thursdays","fridays","saturdays","weekends"};
			if (absDayOfWeek>=0)
				timeInfo+="absDayOfWeek="+absDayOfWeek+"["+daysOfWeek[absDayOfWeek]+"] ";
			int timeFrequenciesValues[] = {cTimeInfo.cHour, cTimeInfo.cDay, cTimeInfo.cWeek, cTimeInfo.cMonth, cTimeInfo.cQuarter, cTimeInfo.cSeason, cTimeInfo.cYear,cTimeInfo.cYear, -2, -3, -4, -5, -6, -7, cUnspecified};
			String timeFrequencies[] = {"hourly","daily","weekly","monthly","quarterly","seasonally","yearly","annually","twice","thrice","once","times","every","each",""};
			if (absSeason>=0)
				timeInfo+="absSeason="+absSeason+"["+seasons[absSeason]+"] ";
			if (absDateSpec>=0)
				timeInfo+="absDateSpec="+absDateSpec+" ";
			String months[] = {"january","february","march","april","may","june","july","august","september","october","november","december"};
			if (absMonth>=0)
				timeInfo+="absMonth="+absMonth+"["+months[absMonth]+"] ";
			if (absDayOfMonth>=0)
				timeInfo+="absDayOfMonth="+absDayOfMonth+" ";
			if (absYear>=0)
				timeInfo+="absYear="+absYear+" ";
			if (timeOfDay>=0)
				timeInfo+="timeOfDay="+timeOfDay+" ";
			if (absHour>=0)
				timeInfo+="absHour="+absHour+" ";
			if (absMinute>=0)
				timeInfo+="absMinute="+absMinute+" ";
			if (absSecond>=0)
				timeInfo+="absSecond="+absSecond+" ";
			if (absTimeSpec>=0)
				timeInfo+="absTimeSpec="+absTimeSpec+" ";
			if (timeFrequency!=-1)
			{
				int tOffset=0;
				for (int ti:timeFrequenciesValues)
					 if (ti==timeFrequency)
						 break;
					 else
						 tOffset++;
				if (tOffset<timeFrequencies.length && timeFrequencies[tOffset].length()>0)
					timeInfo+="frequency="+((tOffset<timeFrequencies.length) ? timeFrequencies[tOffset]+" " : timeFrequency+" ");
			}
			if (absHoliday>=0)
			{
				timeInfo+="Holiday="+absHoliday+" ";
			}
			if (metaDescriptive)
			{
				timeInfo+="metaDescriptive="+metaDescriptive+" ";
			}
			timeInfo+="]";
			return timeInfo;
		}
		public cTimeInfo(LittleEndianDataInputStream rs) {
			tWhere = rs.readInteger();
			timePreviousLink = rs.readInteger();
			timeSPTAnchor = rs.readInteger();
			timeETAnchor = rs.readInteger();
			timeRTAnchor = rs.readInteger();
			timeRelationType = rs.readInteger();
			timeModifier = rs.readInteger();
			timeModifier2 = rs.readInteger();
			absMetaRelation = rs.readInteger();
			timeCapacity = rs.readInteger();
			absYear = rs.readShort();
			absSeason = rs.readByte();
			absDateSpec = rs.readByte(); // A.D. B.C.
			absMonth = rs.readByte();
			absDayOfWeek = rs.readByte();
			absDayOfMonth = rs.readByte();
			timeOfDay = rs.readByte(); // morning/evening/noon etc
			absHour = rs.readByte();
			absMinute = rs.readByte();
			absSecond = rs.readByte();
			absTimeSpec = rs.readByte(); // A.M. P.M.
			timeFrequency = rs.readByte(); // daily
			metaDescriptive = rs.readByte()!=0;
		  absHoliday= rs.readShort();
			absMoment = rs.readByte();
			absNamedDay = rs.readByte();
			absNamedHoliday = rs.readByte();
			absNamedMonth = rs.readByte();
			absNamedSeason = rs.readByte();
			absToday = rs.readByte();
			absTomorrow = rs.readByte();
			absTonight = rs.readByte();
			absUnspecified = rs.readByte();
			absYesterday = rs.readByte();
		}
	}

