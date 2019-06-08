package show;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.ComponentEvent;
import java.awt.event.ComponentListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;

import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.JPopupMenu;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.event.MenuEvent;
import javax.swing.event.MenuListener;

class FindMenu extends JMenu implements ActionListener, MenuListener {

	private static final long serialVersionUID = 1L;

	public FindMenu(String title) {
		super(title);
		MenuScroller.setScrollerFor(this, 10, 50, 0, 0);
		//addFocusListener(this);
		//addMenuListener(this);
	}

//	@Override
//	public void componentResized(ComponentEvent e) {
//		JMenuItem mi = (JMenuItem) e.getSource();
//		JPopupMenu o = (JPopupMenu) mi.getParent();
//		FindMenu dm = (FindMenu) o.getInvoker();
//		System.out.println("component resized " + mi.getText() + "[" + dm.getText() + "]");
//// dm.populateSources();
//	}
//
//	@Override
//	public void componentMoved(ComponentEvent e) {
//		JMenuItem mi = (JMenuItem) e.getSource();
//		JPopupMenu o = (JPopupMenu) mi.getParent();
//		FindMenu dm = (FindMenu) o.getInvoker();
//		System.out.println("component moved " + mi.getText() + "[" + dm.getText() + "]");
//	}
//
//	@Override
//	public void componentShown(ComponentEvent e) {
//		JMenuItem mi = (JMenuItem) e.getSource();
//		JPopupMenu o = (JPopupMenu) mi.getParent();
//		FindMenu dm = (FindMenu) o.getInvoker();
//		System.out.println("component shown " + mi.getText() + "[" + dm.getText() + "]");
//	}
//
//	@Override
//	public void componentHidden(ComponentEvent e) {
//	}

//	@Override
//	public void stateChanged(ChangeEvent e) {
//		JMenuItem mi = (JMenuItem) e.getSource();
//		JPopupMenu o = (JPopupMenu) mi.getParent();
//		FindMenu dm = (FindMenu) o.getInvoker();
//		System.out.println("state changed " + mi.getText() + "[" + dm.getText() + "]");
//	}

	@Override
	public void actionPerformed(ActionEvent e) {
		JMenuItem book = (JMenuItem) e.getSource();
		JPopupMenu o = (JPopupMenu) book.getParent();
		FindMenu author = (FindMenu) o.getInvoker();
	}

//	@Override
//	public void focusGained(FocusEvent e) {
//		JMenuItem mi = (JMenuItem) e.getSource();
//		JPopupMenu o = (JPopupMenu) mi.getParent();
//		FindMenu dm = (FindMenu) o.getInvoker();
//		System.out.println("focusGained  " + mi.getText() + "[" + dm.getText() + "]");
//	}
//
//	@Override
//	public void focusLost(FocusEvent e) {
//		JMenuItem mi = (JMenuItem) e.getSource();
//		JPopupMenu o = (JPopupMenu) mi.getParent();
//		FindMenu dm = (FindMenu) o.getInvoker();
//		System.out.println("focusLost  " + mi.getText() + "[" + dm.getText() + "]");
//	}
//
	@Override
	public void menuCanceled(MenuEvent e) {

	}

	@Override
	public void menuDeselected(MenuEvent e) {

	}

	@Override
	public void menuSelected(MenuEvent e) {
		FindMenu mi = (FindMenu) e.getSource();
		System.out.println("menuSelected  " + mi.getText());
		
	}

}
