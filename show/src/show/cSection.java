package show;
	public class cSection {
		int begin, endHeader;
		int speakersMatched, speakersNotMatched, counterSpeakersMatched, counterSpeakersNotMatched;
		cOM[] definiteSpeakerObjects, speakerObjects, objectsSpokenAbout, objectsInNarration;
		int[] subHeadings;
		int[] preIdentifiedSpeakerObjects; // temporary - used before

		// identifySpeakers

		public cSection(LittleEndianDataInputStream rs) {
			begin = rs.readInteger();
			endHeader = rs.readInteger();
			int count = rs.readInteger();
			subHeadings = new int[count];
			for (int I = 0; I < count; I++)
				subHeadings[I] = rs.readInteger();
			count = rs.readInteger();
			definiteSpeakerObjects = new cOM[count];
			for (int I = 0; I < count; I++)
				definiteSpeakerObjects[I] = new cOM(rs);
			count = rs.readInteger();
			speakerObjects = new cOM[count];
			for (int I = 0; I < count; I++)
				speakerObjects[I] = new cOM(rs);
			count = rs.readInteger();
			objectsSpokenAbout = new cOM[count];
			for (int I = 0; I < count; I++)
				objectsSpokenAbout[I] = new cOM(rs);
			count = rs.readInteger();
			objectsInNarration = new cOM[count];
			for (int I = 0; I < count; I++)
				objectsInNarration[I] = new cOM(rs);
			speakersMatched = speakersNotMatched = counterSpeakersMatched = counterSpeakersNotMatched = 0;
		}
	}

