package show;

import java.awt.Color;
import java.awt.event.ActionListener;
import java.util.ArrayList;
import java.util.List;

import javax.swing.text.Element;
import javax.swing.text.Segment;
import javax.swing.text.SimpleAttributeSet;
import javax.swing.text.StyleConstants;
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.text.AttributeSet;
import javax.swing.text.BadLocationException;
import javax.swing.text.DefaultStyledDocument;

/**
 * DefaultDocument subclass that supports batching inserts.
 */
public class BatchDocument extends DefaultStyledDocument {
	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;

	/**
	 * EOL tag that we re-use when creating ElementSpecs
	 */
	private static final char[] EOL_ARRAY = { '\n' };

	/**
	 * Batched ElementSpecs
	 */
	private ArrayList<ElementSpec> batch = null;

	public BatchDocument(int minimumNum) {
		batch = new ArrayList<ElementSpec>(minimumNum);
	}

	/**
	 * Adds a String (assumed to not contain line feeds) for later batch insertion.
	 */
	public void insertString(String str, AttributeSet a) {
		// We could synchronize this if multiple threads
		// would be in here. Since we're trying to boost speed,
		// we'll leave it off for now.

		// Make a copy of the attributes, since we will hang onto
		// them indefinitely and the caller might change them
		// before they are processed.
		if (str.equals("\n"))
			appendBatchLineFeed(a);
		else {
			if (a != null)
				a = a.copyAttributes();
			batch.add(new ElementSpec(a, ElementSpec.ContentType, str.toCharArray(), 0, str.length()));
		}
	}

	/**
	 * Adds a linefeed for later batch processing
	 */
	public void appendBatchLineFeed(AttributeSet a) {
		// See sync notes above. In the interest of speed, this
		// isn't synchronized.

		// Add a spec with the linefeed characters
		batch.add(new ElementSpec(a, ElementSpec.ContentType, EOL_ARRAY, 0, 1));

		// Then add attributes for element start/end tags. Ideally
		// we'd get the attributes for the current position, but we
		// don't know what those are yet if we have unprocessed
		// batch inserts. Alternatives would be to get the last
		// paragraph element (instead of the first), or to process
		// any batch changes when a linefeed is inserted.
		Element paragraph = getParagraphElement(0);
		AttributeSet pattr = paragraph.getAttributes();
		batch.add(new ElementSpec(null, ElementSpec.EndTagType));
		batch.add(new ElementSpec(pattr, ElementSpec.StartTagType));
	}

	public void processBatchUpdates(int offs) {
		// As with insertBatchString, this could be synchronized if
		// there was a chance multiple threads would be in here.
		ElementSpec[] inserts = new ElementSpec[batch.size()];
		batch.toArray(inserts);
		try {
			// Process all of the inserts in bulk
			super.insert(offs, inserts);
		} catch (BadLocationException e) {
			e.printStackTrace();
		}
	}

	public String removeBracketedItems(String str)
	{
		int bracketLevel=0;
		String removed="";
		for (int I=0; I<str.length(); I++)
		{
			if (str.charAt(I)=='[' || str.charAt(I)=='{')
				bracketLevel++;
			else if (str.charAt(I)==']' || str.charAt(I)=='}')
			{
				if (bracketLevel>0)
					bracketLevel--;
			}
			else if (bracketLevel==0)
				removed+=str.charAt(I);
		}
		return removed;
	}
	
	void addSurroundingWords(String searchString,String str,int surroundWords,int masterIndex, JMenu searchSourceMenu, ActionListener searchSourceListener)
	{
		String noBracketStr=removeBracketedItems(str.substring(Math.max(masterIndex-100,0),Math.min(masterIndex+100,str.length())));
		masterIndex=0;
		while (true)
		{
			String menuString=searchString;
			masterIndex=noBracketStr.indexOf(searchString,masterIndex+1);
			if (masterIndex<0)
				break;
			int numWords=0,index=masterIndex-1;
			// get previous three words
			while (index>0 && numWords<surroundWords)
			{
				int previousIndex=noBracketStr.lastIndexOf(" ", index);
				if (previousIndex<0)
					break;
				menuString=noBracketStr.substring(previousIndex, index+1)+" "+menuString;
				index=previousIndex-1;
				numWords++;
			}
			numWords=0;
			index=masterIndex+searchString.length();
			// get next three words
			while (index<noBracketStr.length() && numWords<surroundWords)
			{
				int nextIndex=noBracketStr.indexOf(" ", index);
				if (nextIndex<0)
					break;
				menuString+=" "+noBracketStr.substring(index,nextIndex);
				index=nextIndex+1;
				numWords++;
			}
			if (menuString.length()<100)
			{
				JMenuItem menuItem=new JMenuItem(menuString);
				menuItem.addActionListener(searchSourceListener);
				searchSourceMenu.add(menuItem);
			}
		}
	}
	
	public void addTextSegments(String searchString, JMenu searchSourceMenu, ActionListener searchSourceListener) {
		searchSourceMenu.removeAll();
		int nleft = getLength();
		Segment text = new Segment();
		int offs = 0;
		text.setPartialReturn(true);
		while (nleft > 0) {
			try {
				getText(offs, nleft, text);
			} catch (BadLocationException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			String str=text.toString();
			//str=removeBracketedItems(str);
			int masterIndex=-200;
			while (true)
			{
				masterIndex=str.indexOf(searchString, masterIndex+200);
				if (masterIndex<0)
					break;
				addSurroundingWords(searchString,str,6,masterIndex,searchSourceMenu,searchSourceListener);
				masterIndex+=searchString.length();
			}
			nleft -= text.count;
			offs += text.count;
		}

	}

	// find the position of the iteration of the text within the document. 
	public int findWordPosition(String searchfor, int iteration) {
		try {
			String text=getText(0, getLength());
			int lastindex=0;
			for (int I=0; I<iteration+1; I++)
				lastindex=text.indexOf(searchfor,lastindex+1);
			System.out.println("iteration "+iteration+" of "+searchfor+" is "+lastindex);
			Element previousElement=getCharacterElement(lastindex);
			AttributeSet as=previousElement.getAttributes();
			String attributeString;
			for ( name:as.getAttributeNames())
			{
			
			}
			System.out.println("start="+previousElement.getStartOffset()+" end="+previousElement.getEndOffset()+" attributes="+previousElement.getAttributes().toString());
			SimpleAttributeSet attributes = new SimpleAttributeSet();
		    attributes = new SimpleAttributeSet();
		    //attributes.addAttribute(StyleConstants.CharacterConstants.Bold, Boolean.FALSE);
		    //attributes.addAttribute(StyleConstants.CharacterConstants.Italic, Boolean.FALSE);
		    attributes.addAttribute(StyleConstants.CharacterConstants.Background, Color.RED);
		    attributes.addAttribute(StyleConstants.CharacterConstants.Foreground, Color.WHITE);
			setCharacterAttributes(lastindex,searchfor.length(),
					attributes,
                    false);

			return lastindex;
		} catch (BadLocationException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return 0;
	}
}
