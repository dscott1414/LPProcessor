package show;
import java.util.HashMap;
import java.util.Map;

	public class WordClass {
    int maxWordId;
    int[] words;
    int[] counts;
    int numWordForms;
    int[] wordForms;
		static Map<String, tFI> Words = new HashMap<String, tFI>();
    public WordClass(LittleEndianDataInputStream rs)
    {
      maxWordId = rs.readInteger();
      words=new int[maxWordId];
      counts=new int[maxWordId];
      for (int I=0; I<maxWordId; I++)
        words[I]=rs.readInteger();
      for (int I=0; I<maxWordId; I++)
      	counts[I]=rs.readInteger();
      numWordForms = rs.readInteger();
      wordForms=new int[numWordForms<<1];
      for (int I=0; I<(numWordForms<<1); I++)
      	wordForms[I]=rs.readInteger();
      }
    void readSpecificWordCache(LittleEndianDataInputStream rs)
		{
    	if (rs.b==null) return;
      int numForms=rs.readInteger();
      Form.forms=new Form[numForms];
	      for (int I=0; I<numForms; I++)
	        Form.forms[I]=new Form(rs);
	    Form.initializeForms();
      while (!rs.EndOfBufferReached())
      {
      	String word=rs.readString();
        Words.put(word, new tFI(rs));
      }
		}
	  public void addMultiWordObjects()
	  {
	  	
	  }
		public int query(String temp)
		{
			return 1;
		}
	}
