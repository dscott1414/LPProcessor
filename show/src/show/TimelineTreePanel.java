package show;

	import javax.swing.*;
import javax.swing.text.BadLocationException;
import javax.swing.text.SimpleAttributeSet;
import javax.swing.text.StyleConstants;
import javax.swing.tree.*;
	import javax.swing.event.*;

import java.awt.*;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.util.HashSet;

	public class TimelineTreePanel extends JPanel {
	    /**
		 * 
		 */
		private static final long serialVersionUID = 1L;
		TreeScrollPane treeScrollPane;
		JTextPane textPane;
	    final static String newline = "\n";

	    public TimelineTreePanel(Source source) {
	        super(new GridBagLayout());
	        GridBagLayout gridbag = (GridBagLayout)getLayout();
	        GridBagConstraints c = new GridBagConstraints();

	        c.fill = GridBagConstraints.BOTH;
	        c.gridwidth = GridBagConstraints.REMAINDER;
	        c.weightx = 1.0;
	        c.weighty = 1.0;
	        c.insets = new Insets(1, 1, 1, 1);
	        treeScrollPane = new TreeScrollPane(source);
	        gridbag.setConstraints(treeScrollPane, c);
	        add(treeScrollPane);

	        c.insets = new Insets(0, 0, 0, 0);
	        textPane = new JTextPane();
	        textPane.setFont(new Font("Serif", Font.PLAIN, 14));
	        textPane.setDragEnabled(true);
	        //textArea.setEditable(false);
	        JScrollPane scrollPane = new JScrollPane(textPane);
	        scrollPane.setVerticalScrollBarPolicy(JScrollPane.VERTICAL_SCROLLBAR_ALWAYS);
	        scrollPane.setPreferredSize(new Dimension(200, 75));
	        gridbag.setConstraints(scrollPane, c);
	        add(scrollPane);
	        setOpaque(true);
	    }

	    class TreeScrollPane extends JScrollPane
	                   implements TreeExpansionListener,
	                              TreeWillExpandListener,
	                              TreeSelectionListener {
	        /**
				 * 
				 */
				private static final long serialVersionUID = 1L;
					Dimension minSize = new Dimension(100, 100);
	        JTree tree;
	        Source source;
	    
	        public TreeScrollPane(final Source s) {
	        	  source=s;
	            tree = new JTree(resetNodes(source));
	            tree.addTreeExpansionListener(this);
	            tree.addTreeWillExpandListener(this);
	            tree.setRootVisible(false);
	            tree.getSelectionModel().setSelectionMode(TreeSelectionModel.SINGLE_TREE_SELECTION);
	            tree.addTreeSelectionListener(this);
	            MouseAdapter ml = new MouseAdapter() {
	            	public void mousePressed(MouseEvent e) {
	            		if(e.getClickCount() == 2) {
	            			TreePath selPath = tree.getPathForLocation(e.getX(), e.getY());
	            			if (selPath != null) {
		            			TimelineTreeNode node = (TimelineTreeNode)selPath.getLastPathComponent();
		            			if (node!=null)
		            			{
		            				try {
													Show.gotoPosition(s.sourceToPosition.get(node.timelineSegment.begin+40));
												} catch (BadLocationException e1) {
													// TODO Auto-generated catch block
													e1.printStackTrace();
												}
		            			}
		            		}
		            	}
	            	}
	            };
	            tree.addMouseListener(ml);
	            setViewportView(tree);
	        }

	        private TreeNode resetNodes(Source source) {
	        	TimelineTreeNode root=new TimelineTreeNode();
	        	if (source==null || source.timelineSegments==null)
	        		return root; 
	        	TimelineTreeNode lastParent=null;
	        	for (TimelineSegment tl:source.timelineSegments)
  	    		{
  	    			String s = "["+tl.begin+"-"+tl.end+"]";
  	    			HashSet<Integer> speakers=new HashSet<Integer>();
  	    			if (tl.begin==Source.speakerGroups[tl.speakerGroup].begin)
  	    			{
  	    				int tsg=tl.speakerGroup;
  	    				while (tsg<Source.speakerGroups.length && Source.speakerGroups[tsg].end<tl.end)
  	    				{
    	    				for (int mo : Source.speakerGroups[tsg].speakers) 
    	    					if (!speakers.contains(mo))
    	    					{
    	    						speakers.add(mo);
      	    					s += source.objectString(mo, true, false).trim()+",";
    	    					}
    	    				tsg++;
  	    				}
  	    			}
  	    			else
  	    			{
  	    			  for (cSpeakerGroup esg: Source.speakerGroups[tl.speakerGroup].embeddedSpeakerGroups)
  	    			  {
  	    			  	if (tl.begin==esg.begin)
  	    			  	{
  	  	    				for (int mo : esg.speakers)
      	    					if (!speakers.contains(mo))
      	    					{
      	    						speakers.add(mo);
        	    					s += source.objectString(mo, true, false).trim()+",";
      	    					}
  	  	    				WordMatch wm=source.m[tl.begin];
  	  	  					if ((wm.flags & WordMatch.flagEmbeddedStoryResolveSpeakers) != 0) {
    	  	    				if ((wm.flags & WordMatch.flagFirstEmbeddedStory) != 0)
      	  	    				for (cOM mo : wm.objectMatches)
          	    					if (!speakers.contains(mo.object))
          	    					{
          	    						speakers.add(mo.object);
            	    					s += source.objectString(mo, true, false).trim()+",";
          	    					}
    	  	    				if ((wm.flags & WordMatch.flagSecondEmbeddedStory) != 0)
      	  	    				for (cOM mo : wm.audienceObjectMatches)
          	    					if (!speakers.contains(mo.object))
          	    					{
          	    						speakers.add(mo.object);
            	    					s += source.objectString(mo, true, false).trim()+",";
          	    					}
  	  	  					}
  	    			  		break;
  	    			  	}
  	    			  }
  	    			}
  	    			if (s.endsWith(","))
  	    				s=s.substring(0,s.length()-1);
  	    	  	if (tl.parentTimeline==-1 || lastParent==null)
  	            root.add(lastParent=new TimelineTreeNode(s,tl));
  	    	  	else
  	    	  		lastParent.add(new TimelineTreeNode(s,tl));
  	    		}
            return root;
	        }
	    
	        public Dimension getMinimumSize() {
	            return minSize;
	        }

	        public Dimension getPreferredSize() {
	            return minSize;
	        }

	        //Required by TreeWillExpandListener interface.
	        public void treeWillExpand(TreeExpansionEvent e) 
	                    throws ExpandVetoException {
	            //saySomething("Tree-will-expand event detected", e);
	                //User said cancel expansion.
	                //saySomething("Tree expansion cancelled", e);
	                //throw new ExpandVetoException(e);
	        }

	        //Required by TreeWillExpandListener interface.
	        public void treeWillCollapse(TreeExpansionEvent e) {
	        	//textPane.append("path = " + e.getPath()+ newline);
	        }

	        // Required by TreeExpansionListener interface.
	        public void treeExpanded(TreeExpansionEvent e) {
	            //saySomething("Tree-expanded event detected", e);
	        }

	        // Required by TreeExpansionListener interface.
	        public void treeCollapsed(TreeExpansionEvent e) {
	            //saySomething("Tree-collapsed event detected", e);
	        }

					@Override
					public void valueChanged(TreeSelectionEvent e) {
						// expand transitions
						TimelineTreeNode node = (TimelineTreeNode) tree.getLastSelectedPathComponent();

						if (node == null)
							return;
						BatchDocument batchDoc=new BatchDocument(node.timelineSegment.timeTransitions.length);
						for (int tl:node.timelineSegment.timeTransitions)
						{
							SimpleAttributeSet keyWord = new SimpleAttributeSet();
							if (Source.relations[tl].timeTransition)
							{
								//StyleConstants.setBold(keyWord, true);
								StyleConstants.setForeground(keyWord,new Color(0x00, 0x00, 0xFF));
							}
							else if (Source.relations[tl].nonPresentTimeTransition)
							{
								StyleConstants.setFontSize(keyWord,10);
								StyleConstants.setForeground(keyWord,new Color(0x20, 0x20, 0xDF));
							}
							else
							{
								StyleConstants.setFontSize(keyWord,10);
								StyleConstants.setForeground(keyWord,new Color(0x20, 0x20, 0x20));
							}
	  	        String s=source.printFullRelationString(tl, -1)+ cTimeInfo.determineTimeProgression(source,Source.relations[tl])+newline;
						  batchDoc.insertString(s, keyWord);
						  batchDoc.insertString("\n", keyWord);
						}
						batchDoc.processBatchUpdates(0);
						textPane.setStyledDocument(batchDoc);
					}
	    }
	}
