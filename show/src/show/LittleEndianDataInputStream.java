package show;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

	public class LittleEndianDataInputStream {
		int offset;
		byte[] b;

		LittleEndianDataInputStream(String path)
		{
			BufferedInputStream sourceInputStream;
			b=null;
			try {
				sourceInputStream = new BufferedInputStream(new FileInputStream(path));
				b = new byte[sourceInputStream.available()];
				sourceInputStream.read(b, 0, sourceInputStream.available());
				sourceInputStream.close();
			} catch (FileNotFoundException e) {
				System.out.println(path + " not found.");
			} catch (IOException e) {
				e.printStackTrace();
			} 
			offset=0;
		}

		boolean EndOfBufferReached()
		{
			return offset==b.length;
		}
		
		String readString() {
			String temp = new String();
			for (; b[offset]!=0 || b[offset + 1]!= 0; offset += 2)
				temp += (char) ((b[offset]&0xff) + ((b[offset + 1]&0xff) << 8));
			offset+=2;
			return temp;
		}

		byte readByte() {
			offset += 1;
			return b[offset - 1];
		}

		short readShort() {
			offset += 2;
			try {
			return (short) ((b[offset - 2]&0xff) + ((b[offset - 1]&0xff) << 8));
      } catch(ArrayIndexOutOfBoundsException e)
      {
      	System.out.println(" NumberFormatException");
      }
			return 0;
		}

		int readInteger() {
			offset += 4;
			if (offset>b.length)
				System.out.println("Out of bounds!");
			return (b[offset - 4]&0xff) + ((b[offset - 3]&0xff) << 8) + ((b[offset - 2]&0xff) << 16) + ((b[offset - 1]&0xff) << 24);
		}

		long readLong() {
			offset += 8;
			if (offset>b.length)
				System.out.println("Out of bounds!");
			return (b[offset - 8]&0xff) + ((long)((long)b[offset - 7]&0xff) << 8) + ((long)((long)b[offset - 6]&0xff) << 16) + ((long)((long)b[offset - 5]&0xff) << 24) + ((long)((long)b[offset - 4]&0xff) << 32) + ((long)((long)b[offset - 3]&0xff) << 40) + ((long)((long)b[offset - 2]&0xff) << 48) + ((long)((long)b[offset - 1]&0xff) << 56);
		}
		
		int[] readIntArray() {
			int count = readInteger();
			int[] a = new int[count];
			for (int I = 0; I < count; I++)
				a[I] = readInteger();
			return a;
		}
		
		int bytesLeft()
		{
			return b.length-offset;
		}
	}

