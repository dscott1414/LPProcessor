package show;

	public class cSpeakerGroup {
		int begin, end, section;
		int previousSubsetSpeakerGroup; // speaker group index
		int saveNonNameObject; // speaker group index
		int conversationalQuotes;
		boolean speakersAreNeverGroupedTogether; // speakers never appear anywhere
		boolean tlTransition; // speakers never appear anywhere
																							// in single group
		int[] speakers; // objects
		// 'a voice' may be resolved by using information from the next
		// speakerGroup. But the reader does not yet necessarily know
		// of this speaker. So subsequent resolving may need to discard this future
		// object because it really doesn't exist yet.
		// 
		int[] fromNextSpeakerGroup; // objects resolved by speakers from the next
																// speaker group
		cOM[] replacedSpeakers; // speakers replaced by other speakers (in
														// speakergroup only, so replacedBy is still -1)
		int[] singularSpeakers;
		int[] groupedSpeakers;
		int[] povSpeakers; // point-of-view
		int[] dnSpeakers; // definitely named speakers
		// metaNameOthers is to prevent confusion when names which are not otherwise
		// mentioned outside of quotes seem to be hailed - but only because the name
		// is being talked about
		int[] metaNameOthers; // people specifically named as being not present - a
													// person being asked someone else's name 'her name is
													// Marguerite'
		cSpeakerGroup[] embeddedSpeakerGroups;
		int[] observers;

		public class cGroup {
			int where;
			int[] objects;
		}

		cGroup[] groups; // subgroups of speakers grouped syntactically

		public cSpeakerGroup(LittleEndianDataInputStream rs) {
			begin = rs.readInteger();
			end = rs.readInteger();
			section = rs.readInteger();
			previousSubsetSpeakerGroup = rs.readInteger();
			saveNonNameObject = rs.readInteger();
			conversationalQuotes = rs.readInteger();
			speakers = rs.readIntArray();
			fromNextSpeakerGroup = rs.readIntArray();
			int count = rs.readInteger();
			if (count<0)
				System.out.println("count="+count);
			replacedSpeakers = new cOM[count];
			for (int I = 0; I < count; I++)
				replacedSpeakers[I] = new cOM(rs);
			singularSpeakers = rs.readIntArray();
			groupedSpeakers = rs.readIntArray();
			povSpeakers = rs.readIntArray();
			dnSpeakers = rs.readIntArray();
			metaNameOthers = rs.readIntArray();
			observers = rs.readIntArray();
			count = rs.readInteger();
			embeddedSpeakerGroups=new cSpeakerGroup[count];
			for (int I = 0; I < count; I++)
				embeddedSpeakerGroups[I] = new cSpeakerGroup(rs);
			count = rs.readInteger();
			groups = new cGroup[count];
			for (int I = 0; I < count; I++)
			{
				groups[I]=new cGroup();
				groups[I].where= rs.readInteger();
				groups[I].objects=rs.readIntArray();
			}
			long flags=rs.readLong();
			speakersAreNeverGroupedTogether=((flags&1)>0) ? true:false;
			tlTransition=((flags&2)>0) ? true:false;
		}
	}

