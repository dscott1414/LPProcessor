package show;

import javax.swing.tree.DefaultMutableTreeNode;

public class TimelineTreeNode extends DefaultMutableTreeNode {

	private static final long serialVersionUID = 1L;
	TimelineSegment timelineSegment;
	public TimelineTreeNode(String s,TimelineSegment tl) {
		super(s);
		timelineSegment=tl;
		
	}

	public TimelineTreeNode() {
		super();
	}

}
