package show;

public class bitObject {
		int[] bits = new int[16];
		short byteIndex, bitIndex;
		static int sizeOfInteger=32;
	  void set(int bit)
	  {
	    bits[bit>>5]|=1<<(bit&(sizeOfInteger-1));
	  }
	  void reset(int bit)
	  {
	    bits[bit>>5]&=~(1<<(bit&(sizeOfInteger-1)));
	  }
	  boolean isSet(int bit)
	  {
	    return (bits[bit>>5]&(1<<(bit&(sizeOfInteger-1))))!=0;
	  }

		public bitObject(LittleEndianDataInputStream rs) {
			for (int I = 0; I < 16; I++)
				bits[I] = rs.readInteger();
			byteIndex = bitIndex = -1;
		}
	}

