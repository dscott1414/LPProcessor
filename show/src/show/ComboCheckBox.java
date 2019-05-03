package show;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

public class ComboCheckBox {
	 
	public Boolean states[];
	    public void actionPerformed(ActionEvent e)
	    {
	        JComboBox<?> cb = (JComboBox<?>)e.getSource();
	        CheckComboStore store = (CheckComboStore)cb.getSelectedItem();
	        CheckComboRenderer ccr = (CheckComboRenderer)cb.getRenderer();
	        ccr.checkBox.setSelected((store.state = !store.state));
	       states[cb.getSelectedIndex()]=!states[cb.getSelectedIndex()]; 
	    }

	    public JPanel getContent(ActionListener al, String ids[] , Boolean values[] )
	    {
	    	states=values;
	        CheckComboStore[] stores = new CheckComboStore[ids.length];
	        for(int j = 0; j < ids.length; j++)
	            stores[j] = new CheckComboStore(ids[j], values[j]);
	        JComboBox<?> combo = new JComboBox<Object>(stores);
	        combo.setRenderer(new CheckComboRenderer());
	        combo.addActionListener(al);
	        JPanel panel = new JPanel();
	        panel.add(combo);
	        return panel;
	    }
	 
	}
	 
	/** adapted from comment section of ListCellRenderer api */
	class CheckComboRenderer implements ListCellRenderer<Object>
	{
	    JCheckBox checkBox;
	 
	    public CheckComboRenderer()
	    {
	        checkBox = new JCheckBox();
	    }
	    public Component getListCellRendererComponent(JList<?> list,
	                                                  Object value,
	                                                  int index,
	                                                  boolean isSelected,
	                                                  boolean cellHasFocus)
	    {
	        CheckComboStore store = (CheckComboStore)value;
	        checkBox.setText(store.id);
	        checkBox.setSelected(((Boolean)store.state).booleanValue());
	        checkBox.setBackground(isSelected ? Color.red : Color.white);
	        checkBox.setForeground(isSelected ? Color.white : Color.black);
	        return checkBox;
	    }
	}
	 
	class CheckComboStore
	{
	    String id;
	    Boolean state;
	 
	    public CheckComboStore(String id, Boolean state)
	    {
	        this.id = id;
	        this.state = state;
	    }
	}


