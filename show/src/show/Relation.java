package show;
	public class Relation {
		int where;
		int o;
		int whereControllingEntity;
		int whereSubject;
		int whereVerb;
		int wherePrep;
		int whereObject;
		int wherePrepObject;
		int whereSecondaryVerb;
		int whereSecondaryObject;
		int whereSecondaryPrep;
		int whereNextSecondaryObject;
		int movingRelativeTo;
		int relationType;
		int objectSubType;
		int prepSubType;
		int timeProgression;
		long questionType;
		int whereQuestionType;
		int sentenceNum;
		int lastOpeningPrimaryQuote;
		int duplicateTimeTransitionFromWhere;
		boolean changeStateAdverb;
		boolean skip;
		boolean genderedEntityMove;
		boolean genderedLocationRelation;
		boolean establishingLocation;
		boolean futureLocation;
		boolean speakerContinuation; // wherePrepObject least one speaker appears as a speaker before and after the establishing location, so don't age speakers
		boolean beyondLocalSpace; // movement beyond a room or a local area
		boolean story; // also includes future stories
		// futureInPastHappening is talking about the future, while the action is in the past.
		boolean beforePastHappening,pastHappening,presentHappening,futureHappening,futureInPastHappening;
		boolean agentLocationRelationSet;
		boolean timeInfoSet;
		boolean physicalRelation;
		boolean negation;
		boolean timeTransition;
		boolean nonPresentTimeTransition;
		boolean significantRelation;
		boolean speakerQuestionToAudience;
		boolean speakerCommand;

		cTimeInfo[] timeInfo;
		
	  String presType;
    String description;
    int nextSPR;

		public Relation(LittleEndianDataInputStream rs) {
			where = rs.readInteger();
			o = rs.readInteger();
			whereControllingEntity = rs.readInteger();
			whereSubject = rs.readInteger();
			whereVerb = rs.readInteger();
			wherePrep = rs.readInteger();
			whereObject = rs.readInteger();
			wherePrepObject = rs.readInteger();
			whereSecondaryVerb = rs.readInteger();
			whereSecondaryObject = rs.readInteger();
			whereSecondaryPrep = rs.readInteger();
			whereNextSecondaryObject = rs.readInteger();
			movingRelativeTo = rs.readInteger();
			relationType = rs.readInteger();
			objectSubType = rs.readInteger();
			prepSubType = rs.readInteger();
			timeProgression = rs.readInteger();
			questionType = rs.readLong();
			whereQuestionType = rs.readInteger();
			sentenceNum = rs.readInteger();
			// flags
			long flags = rs.readLong();
			changeStateAdverb=(flags & 1) == 1; flags>>=1;
			skip=(flags & 1) == 1; flags>>=1;
			physicalRelation=(flags & 1) == 1; flags>>=1;
			timeInfoSet=(flags & 1) == 1; flags>>=1;
			agentLocationRelationSet = (flags & 1) == 1;			flags >>= 1;
			negation = (flags & 1) == 1;			flags >>= 1;
			futureInPastHappening = (flags & 1) == 1;			flags >>= 1;
			futureHappening = (flags & 1) == 1;			flags >>= 1;
			presentHappening = (flags & 1) == 1;			flags >>= 1;
			pastHappening = (flags & 1) == 1;			flags >>= 1;
			beforePastHappening = (flags & 1) == 1;			flags >>= 1;
			nonPresentTimeTransition = (flags & 1) == 1;			flags >>= 1;
			timeTransition = (flags & 1) == 1;			flags >>= 1;
			//if (timeTransition)
			//	System.out.print(where+":timeTransition true!\n");
			story = (flags & 1) == 1;			flags >>= 1;
			beyondLocalSpace = (flags & 1) == 1;			flags >>= 1;
			significantRelation = (flags & 1) == 1;			flags >>= 1;
			speakerQuestionToAudience = (flags & 1) == 1;			flags >>= 1;
			speakerCommand = (flags & 1) == 1;			flags >>= 1;
			speakerContinuation = (flags & 1) == 1;			flags >>= 1;
			futureLocation = (flags & 1) == 1;	flags >>= 1;
			establishingLocation = (flags & 1) == 1;  flags >>= 1;
			genderedLocationRelation = (flags & 1) == 1;	flags >>= 1;
			genderedEntityMove = (flags & 1) == 1;	
			lastOpeningPrimaryQuote = rs.readInteger();
			duplicateTimeTransitionFromWhere = rs.readInteger();
      presType=rs.readString();
      description=rs.readString();
      nextSPR=rs.readInteger();
  		int count = rs.readInteger();
  		timeInfo = new cTimeInfo[count];
  		for (int I = 0; I < count; I++)
  			timeInfo[I] = new cTimeInfo(rs);
		}
	}

