package show;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

	public class LittleEndianDataInputStream {
		int offset;
		byte[] b;

		// Reads the whole file into `b` up front (same approach as the
		// Python sibling LPIO.py.__init__). On open/read failure, `b` is
		// left null - WordClass.readSpecificWordCache() relies on exactly
		// this to treat a missing optional .wordCacheFile as "nothing more
		// to read" (it checks rs.b==null itself before touching the reader),
		// so the constructor intentionally does NOT throw here; that would
		// turn that one legitimately-tolerated missing-file case into a
		// hard crash. Other callers that pass a required file (e.g.
		// .SourceCache) and never check rs.b will still fail - but via a
		// NullPointerException at their first read, same as before this
		// pass; making every caller fail-fast safely would mean touching
		// call sites outside this file's scope, so it isn't attempted here.
		LittleEndianDataInputStream(String path)
		{
			BufferedInputStream sourceInputStream;
			b=null;
			try {
				sourceInputStream = new BufferedInputStream(new FileInputStream(path));
				int available = sourceInputStream.available();
				b = new byte[available];
				// available() is only a hint, not a guarantee of how many
				// bytes a single read() call returns - loop until the
				// buffer is full or the stream is exhausted, rather than
				// silently keeping whatever a short first read happened to
				// fill (same short-read concern as LPIO.py's read_string).
				int totalRead = 0;
				while (totalRead < available) {
					int n = sourceInputStream.read(b, totalRead, available - totalRead);
					if (n < 0)
						break;
					totalRead += n;
				}
				sourceInputStream.close();
			} catch (FileNotFoundException e) {
				System.out.println(path + " not found.");
			} catch (IOException e) {
				e.printStackTrace();
			}
			offset=0;
		}

		// Treats a failed-to-open stream (b==null) as already at EOF rather
		// than NullPointerException-ing, so callers like
		// WordClass.readSpecificWordCache() that loop "while
		// (!rs.EndOfBufferReached())" stay safe even without their own
		// explicit null check.
		boolean EndOfBufferReached()
		{
			return b == null || offset==b.length;
		}

		// Reads UTF-16LE code units until a 0x0000 terminator. A short
		// read at EOF (offset+1 out of bounds) is treated as an implicit
		// terminator instead of throwing ArrayIndexOutOfBoundsException -
		// same fix as the Python sibling LPIO.py.read_string().
		String readString() {
			String temp = new String();
			for (; offset + 1 < b.length && (b[offset]!=0 || b[offset + 1]!= 0); offset += 2)
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

