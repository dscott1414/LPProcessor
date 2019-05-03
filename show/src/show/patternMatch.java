package show;
	public class patternMatch {
		short len;
		short cost; 
		short flags;
		int pemaByPatternEnd; // first PEMA entry in this source position the
		// PMA array belongs to that has the same
		// pattern and end
		int pemaByChildPatternEnd; // first PEMA entry in this source position
		// that has a child pattern = pattern and
		// childEnd = end
		short descendantRelationships;
		short pattern;

		public patternMatch(LittleEndianDataInputStream rs) {
			len = rs.readShort();
			cost = rs.readShort();
			flags = rs.readShort();
			rs.readShort(); // must read this because C++ compiled has 32 bit alignment
			pemaByPatternEnd = rs.readInteger();
			pemaByChildPatternEnd = rs.readInteger();
			descendantRelationships = rs.readShort();
			pattern = rs.readShort();
		}
	}

