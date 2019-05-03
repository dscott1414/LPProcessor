package show;

import java.awt.Dimension;
import java.awt.Rectangle;
import javax.swing.ComboBoxModel;
import javax.swing.JComboBox;
import javax.swing.plaf.basic.BasicComboPopup;
import javax.swing.plaf.basic.ComboPopup;
import javax.swing.plaf.metal.MetalComboBoxUI;

class SteppedComboBoxUI extends MetalComboBoxUI {
	protected ComboPopup createPopup() {
		BasicComboPopup popup = new BasicComboPopup(comboBox) {

			/**
				 * 
				 */
			private static final long serialVersionUID = 1L;

			public void show() {
				Dimension popupSize = ((SteppedComboBox) comboBox).getPopupSize();
				popupSize.setSize(popupSize.width, getPopupHeightForRowCount(comboBox.getMaximumRowCount()));
				Rectangle popupBounds = computePopupBounds(0, comboBox.getBounds().height, popupSize.width, popupSize.height);
				scroller.setMaximumSize(popupBounds.getSize());
				scroller.setPreferredSize(popupBounds.getSize());
				scroller.setMinimumSize(popupBounds.getSize());
				System.out.println("height="+popupBounds.getSize().height+" width="+popupBounds.getSize().width);
				System.out.println("height="+comboBox.getSize().height+" width="+popupBounds.getSize().width);
				list.invalidate();
				int selectedIndex = comboBox.getSelectedIndex();
				if (selectedIndex == -1) {
					list.clearSelection();
				} else {
					list.setSelectedIndex(selectedIndex);
				}
				list.ensureIndexIsVisible(list.getSelectedIndex());
				setLightWeightPopupEnabled(comboBox.isLightWeightPopupEnabled());

				show(comboBox, popupBounds.x, popupBounds.y);
			}
		};
		popup.getAccessibleContext().setAccessibleParent(comboBox);
		return popup;
	}
}

class SteppedComboBox extends JComboBox<Object> {
	/**
		 * 
		 */
	private static final long serialVersionUID = 1L;
	protected int popupWidth;

	public SteppedComboBox(ComboBoxModel<?> aModel) {
		super((ComboBoxModel<Object>) aModel);
		setUI(new SteppedComboBoxUI());
		popupWidth = 0;
	}

	public SteppedComboBox() {
		super();
		setUI(new SteppedComboBoxUI());
		popupWidth = 0;
	}

	public SteppedComboBox(final Object[] items) {
		super(items);
		setUI(new SteppedComboBoxUI());
		popupWidth = 0;
	}

	public void setPopupWidth(int width) {
		popupWidth = width;
	}

	public Dimension getPopupSize() {
		Dimension size = getSize();
		if (popupWidth < 1)
			popupWidth = size.width;
		return new Dimension(popupWidth, size.height);
	}
}
