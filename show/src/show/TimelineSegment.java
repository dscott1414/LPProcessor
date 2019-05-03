package show;

public class TimelineSegment {
	int parentTimeline;
	int begin;
	int end;
	int speakerGroup;
	int location;
	int linkage;
	int sr;
	int[] timeTransitions;
	int[] locationTransitions;
	int[] subTimelines;
	public TimelineSegment(LittleEndianDataInputStream rs) {
		parentTimeline = rs.readInteger();
		begin = rs.readInteger();
		end = rs.readInteger();
		speakerGroup = rs.readInteger();
		location = rs.readInteger();
		linkage = rs.readInteger();
		sr = rs.readInteger();
		int count = rs.readInteger();
		timeTransitions = new int[count];
		for (int I = 0; I < count; I++)
			timeTransitions[I]=rs.readInteger();
		count = rs.readInteger();
		locationTransitions = new int[count];
		for (int I = 0; I < count; I++)
			locationTransitions[I]=rs.readInteger();
		count = rs.readInteger();
		subTimelines = new int[count];
		for (int I = 0; I < count; I++)
			subTimelines[I]=rs.readInteger();
	}

}
