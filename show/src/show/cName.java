package show;
	public class cName {
  	enum nameType { HON,HON2,HON3,FIRST,MIDDLE,MIDDLE2,LAST,SUFFIX,ANY };
  	int nickName;
  	String hon,hon2,hon3;
  	String first;
  	String middle,middle2;
  	String last;
  	String suffix;
  	String any;

 		// identifySpeakers
  	String hn(String namePartName,String namePart,boolean printShort)
  	{
  	  if (namePart.isEmpty()) return "";
 	    return ((printShort) ? "" : (namePartName+":") ) + namePart +" ";
  	}

  	String print(boolean printShort)
  	{
  		String  message="";
  		message+=hn("H1",hon,printShort);
  		message+=hn("H2",hon2,printShort);
  		message+=hn("H3",hon3,printShort);
  		message+=hn("F",first,printShort);
  		message+=hn("M1",middle,printShort);
  		message+=hn("M2",middle2,printShort);
  		message+=hn("L",last,printShort);
  		message+=hn("S",suffix,printShort);
  		message+=hn("A",any,printShort);
  	  if (nickName>=0 && !printShort)
  	    message+="["+nickName+"]";
  	  return message;
  	}

		public cName(LittleEndianDataInputStream rs) {
			nickName = rs.readInteger();
			hon = rs.readString();
			hon2 = rs.readString();
			hon3 = rs.readString();
			first = rs.readString();
			middle = rs.readString();
			middle2 = rs.readString();
			last = rs.readString();
			suffix = rs.readString();
			any = rs.readString();
		}
	}

