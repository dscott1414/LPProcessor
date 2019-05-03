package show;
	public class cOM {
		int object;
		int salienceFactor;

		public int read(byte[] b, int offset) {
			return offset;
		}

		public cOM(int o, int sf) {
			object = o;
			salienceFactor = sf;
		}

		public cOM(LittleEndianDataInputStream rs) {
			object = rs.readInteger();
			salienceFactor = rs.readInteger();
		}
	}

