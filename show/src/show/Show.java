package show;

import java.awt.Component;
import java.awt.Dimension;
import java.awt.EventQueue;
import java.awt.Font;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.GridLayout;
import java.awt.Insets;
import java.awt.Menu;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.AdjustmentEvent;
import java.awt.event.AdjustmentListener;
import java.awt.event.ComponentAdapter;
import java.awt.event.ComponentEvent;
import java.awt.event.ComponentListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.event.MouseMotionListener;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.Reader;
import java.util.HashMap;
import java.util.Map;
import java.util.Vector;

import javax.swing.BoxLayout;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JMenu;
import javax.swing.JPanel;
import javax.swing.JPopupMenu;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.JTextPane;
import javax.swing.SwingUtilities;
import javax.swing.TransferHandler;
import javax.swing.event.CaretEvent;
import javax.swing.event.CaretListener;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;
import javax.swing.event.MenuEvent;
import javax.swing.event.MenuListener;
import javax.swing.table.DefaultTableModel;
import javax.swing.text.BadLocationException;
import javax.swing.text.Position;
import java.awt.datatransfer.Transferable;
import javax.swing.JMenuItem;
import javax.swing.JMenuBar;
//import javax.swing.ImageIcon;

import show.Source.SourceMapType;

import java.awt.event.*;

public class Show implements ActionListener, ItemListener {
	public static Source source;
	public static int lastPosition = 0;
	public static VerbNet vn;
	static JFrame frame;
	static JFrame wordInfoFrame;
	static JFrame agentFrame;
	static JFrame timelineFrame;
	static String sourcePath = null;
	static SteppedComboBox pickChapter;
	static JMenu pickSourceMenu;
	static JMenu searchSourceMenu;
	static JTextField searchField;
	static MenuScroller MS;
	static JMenuBar menuBar;
	static JTextPane mainPane;
	static JTextPane agentPane;
	static TimelineTreePanel timelinePane;
	static JTextArea wordInfo;
	static JTextArea relationsInfo;
	static JTextArea roleInfo;
	static JScrollPane mainTextScrollPane;
	static JScrollPane wordInfoScrollPane;
	static JScrollPane agentInfoScrollPane;
	static JPanel pickPreferences;
	static ComboCheckBox ccb;
	static JButton reloadButton;
	static JTextField whereField;
	static File sourceDir;
	static JTable wordInfoSelectionTable;
	static Vector<Vector<String>> wordInfoTableData = new Vector<Vector<String>>();
	static Vector<String> wordInfoTableColumnNames = new Vector<String>();
	static WordClass Words;

	static int gotoPosition(int position) throws BadLocationException {
		mainPane.setCaretPosition(position);
		Rectangle pos = mainPane.getUI().modelToView(mainPane, position);
		pos.height = 0;
		pos.width = 100;
		mainPane.scrollRectToVisible(pos);
		return position;
	}

	public static void main(String args[]) {

		Runnable runner = new Runnable() {

			public void initializeAgentDisplay() {
				agentFrame = new JFrame("show agents");
				agentPane = new JTextPane();
				agentInfoScrollPane = new JScrollPane(agentPane);
				agentFrame.setLayout(new GridLayout(1, 0));
				GridBagConstraints c = new GridBagConstraints();
				c.fill = GridBagConstraints.BOTH;
				agentFrame.add(agentInfoScrollPane, c);
				agentFrame.setBounds(600, 600, 600, 500);
				agentFrame.setVisible(true);
			}

			public void initializeTimelineDisplay() {
				timelineFrame = new JFrame("show timelines");
//        timelinePane = new TimelineTreePanel(source);
//        timelinePane.setOpaque(true); //content panes must be opaque
//        timelineFrame.setContentPane(timelinePane);
				timelineFrame.setBounds(1200, 600, 600, 500);
				timelineFrame.pack();
				timelineFrame.setVisible(true);
			}

			public void initializeWordInfoDisplay() {
				wordInfoFrame = new JFrame("show wordinfo");
				wordInfoTableColumnNames.add("word");
				wordInfoTableColumnNames.add("role/verbClass");
				wordInfoTableColumnNames.add("relations");
				wordInfoTableColumnNames.add("object info");
				wordInfoTableColumnNames.add("matching objects");
				wordInfoTableColumnNames.add("flags");

				DefaultTableModel model = new DefaultTableModel(wordInfoTableData, wordInfoTableColumnNames) {
					private static final long serialVersionUID = 1L;

					public boolean isCellEditable(int row, int column) {
						return false;
					}

					public String getColumnName(int col) {
						return columnIdentifiers.get(col).toString();
					}

					public int getRowCount() {
						return dataVector.size();
					}

					public int getColumnCount() {
						return columnIdentifiers.size();
					}

					public Object getValueAt(int row, int col) {
						return ((Vector<?>) dataVector.elementAt(row)).elementAt(col);
					}

					public void setValueAt(Object value, int row, int col) {
						((Vector<Object>) dataVector.elementAt(row)).set(col, value);
						fireTableCellUpdated(row, col);
					}

				};

				class CorrectStrangeBehaviourListener extends ComponentAdapter {
					public void componentResized(ComponentEvent e) {
						if (wordInfoSelectionTable.getPreferredSize().width <= wordInfoScrollPane.getViewport()
								.getExtentSize().width)
							wordInfoSelectionTable.setAutoResizeMode(JTable.AUTO_RESIZE_ALL_COLUMNS);
						else
							wordInfoSelectionTable.setAutoResizeMode(JTable.AUTO_RESIZE_OFF);
					}
				}

				// DefaultTableModel model = new DefaultTableModel(wordInfoTableData,
				// wordInfoTableColumnNames);
				wordInfoSelectionTable = new JTable(model); // model
				// wordInfoSelectionTable.setSortable(false);
				wordInfoSelectionTable.setAutoResizeMode(JTable.AUTO_RESIZE_OFF);
				wordInfoSelectionTable.setFont(new Font("Serif", Font.PLAIN, 12));
				wordInfoScrollPane = new JScrollPane(wordInfoSelectionTable);
				wordInfoScrollPane.addComponentListener(new CorrectStrangeBehaviourListener());

				wordInfoFrame.setLayout(new GridLayout(1, 0));
				GridBagConstraints c = new GridBagConstraints();
				c.fill = GridBagConstraints.BOTH;
				wordInfoFrame.add(wordInfoScrollPane, c);
				wordInfoFrame.setBounds(600, 0, 600, 500);
				wordInfoFrame.setVisible(true);
			}

			public void initializeDisplay() {
				frame = new JFrame("show source");
				frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				frame.setLayout(new GridBagLayout());
				pickChapter = new SteppedComboBox();
				pickChapter.addItemListener(new ItemListener() {
					@Override
					public void itemStateChanged(ItemEvent e) {
						try {
							if (source.chapters != null && source.chapters.size() > 0)
								gotoPosition(source.sourceToPosition
										.get(source.chapters.get(pickChapter.getSelectedIndex()) + 40));
						} catch (BadLocationException e1) {
							// TODO Auto-generated catch block
							e1.printStackTrace();
						}
					}
				});
				wordInfo = new JTextArea();
				relationsInfo = new JTextArea(2, 1);
				roleInfo = new JTextArea(3, 1);
				mainPane = new JTextPane();
				mainTextScrollPane = new JScrollPane(mainPane);
				mainTextScrollPane.setVerticalScrollBarPolicy(JScrollPane.VERTICAL_SCROLLBAR_ALWAYS);
				class DisplayTextPositionListener implements AdjustmentListener {
					public void adjustmentValueChanged(AdjustmentEvent ae) {
						Point lowpt = new Point(0, ae.getValue());
						Point highpt = new Point(mainPane.getWidth(), ae.getValue() + mainTextScrollPane.getHeight());
						Position.Bias[] biasRet = new Position.Bias[1];
						int lowpos = mainPane.getUI().viewToModel(mainPane, lowpt, biasRet), lowindex = 0;
						int highpos = mainPane.getUI().viewToModel(mainPane, highpt, biasRet), highindex = 0;
						if (source == null || source.positionToSource == null || source.positionToSource.isEmpty()
								|| lowpos < 0 || highpos < 0)
							return;
						try {
							lowindex = source.positionToSource.floorKey(lowpos);
							highindex = source.positionToSource.floorKey(highpos);
						} catch (RuntimeException e1) {
							e1.printStackTrace();
						}
						Source.WhereSource lowws = null, highws = null;
						for (; lowindex < highindex; lowindex++) {
							lowws = source.positionToSource.get(lowindex);
							if (lowws != null && lowws.flags == SourceMapType.wordType)
								break;
						}
						for (; highindex > lowindex; highindex--) {
							highws = source.positionToSource.get(highindex);
							if (highws != null && highws.flags == SourceMapType.wordType)
								break;
						}
						if (lowws != null && highws != null)
							whereField.setText("" + lowws.index + " - " + highws.index);
					}
				}
				mainTextScrollPane.getVerticalScrollBar().addAdjustmentListener(new DisplayTextPositionListener());
				String[] preferenceStrings = { "time", "space", "all other", "story", "quote", "non-time transition" };
				Boolean[] values = { Boolean.TRUE, Boolean.FALSE, Boolean.FALSE, Boolean.FALSE, Boolean.FALSE,
						Boolean.FALSE };
				ccb = new ComboCheckBox();
				pickPreferences = ccb.getContent(new ActionListener() {
					@Override
					public void actionPerformed(ActionEvent e) {
						ccb.actionPerformed(e);
					}
				}, preferenceStrings, values);
				reloadButton = new JButton("reload");
				whereField = new JTextField("0", 30);
				createSourceMenu();
				// public GridBagConstraints(int gridx = RELATIVE, int gridy= RELATIVE,int
				// gridwidth=1, int gridheight=1,
				// double weightx=0, double weighty=0,int anchor=CENTER, int fill=NONE,
				// Insets insets, int ipadx=0, int ipady=0)
				frame.add(pickPreferences, new GridBagConstraints(0, 0, 1, 1, 0, 0, GridBagConstraints.CENTER, 0,
						new Insets(0, 0, 0, 0), 0, 0));
				frame.add(whereField, new GridBagConstraints(1, 0, 1, 1, 1, 0, GridBagConstraints.CENTER,
						GridBagConstraints.HORIZONTAL, new Insets(0, 0, 0, 0), 0, 0));
				frame.add(pickChapter, new GridBagConstraints(2, 0, 1, 1, 0, 0, GridBagConstraints.CENTER, 0,
						new Insets(0, 0, 0, 0), 0, 0));
				frame.add(reloadButton, new GridBagConstraints(3, 0, 1, 1, 0, 0, GridBagConstraints.CENTER, 0,
						new Insets(0, 0, 0, 0), 0, 0));
				// Dimension d1=pickChapter.getPreferredSize(),d2=whereField.getPreferredSize();

				pickChapter.setMaximumSize(new Dimension(25, pickChapter.getPreferredSize().height));
				whereField.setMinimumSize(new Dimension(300, whereField.getPreferredSize().height));
				frame.setJMenuBar(menuBar);
				JPanel infoPanel = new JPanel();
				infoPanel.setLayout(new BoxLayout(infoPanel, BoxLayout.Y_AXIS));
				infoPanel.add(wordInfo);
				relationsInfo.setLineWrap(true);
				relationsInfo.setMinimumSize(new Dimension(30, 32));
				infoPanel.add(relationsInfo);
				roleInfo.setLineWrap(true);
				roleInfo.setMinimumSize(new Dimension(30, 32));
				infoPanel.add(roleInfo);
				frame.add(infoPanel, new GridBagConstraints(0, 1, 4, 5, 0, 0, GridBagConstraints.CENTER,
						GridBagConstraints.HORIZONTAL, new Insets(0, 3, 0, 3), 0, 0));
				frame.add(mainTextScrollPane, new GridBagConstraints(0, 6, 4, 4, 0, 1, GridBagConstraints.PAGE_END,
						GridBagConstraints.BOTH, new Insets(4, 0, 0, 0), 0, 0));
				mainPane.setFont(new Font("Serif", Font.PLAIN, 16));
				mainPane.setDragEnabled(true);

				UberHandler uh = new UberHandler();
				uh.setOutput(wordInfo);
				wordInfo.setTransferHandler(uh);
				uh.setOutput(relationsInfo);
				relationsInfo.setTransferHandler(uh);
				uh.setOutput(roleInfo);
				roleInfo.setTransferHandler(uh);
				ButtonListener bl = new ButtonListener();
				reloadButton.addActionListener(bl);
				initializeWordInfoDisplay();
				initializeAgentDisplay();
				initializeTimelineDisplay();
			}

			void writeState(JMenuItem book, DynamicMenu author, int sourceOffset) throws IOException {
				DataOutputStream sourceWriter = new DataOutputStream(
						new BufferedOutputStream(new FileOutputStream("lpGUIState")));
				sourceWriter.writeUTF(author.getText());
				sourceWriter.writeUTF(book.getText());
				sourceWriter.writeInt(sourceOffset);
				sourceWriter.writeInt(MS.getFirstIndex());
				sourceWriter.close();
			}

			void readState() throws IOException {
				DataInputStream sourceReader = new DataInputStream(
						new BufferedInputStream(new FileInputStream("lpGUIState")));
				String author = sourceReader.readUTF();
				String book = sourceReader.readUTF();
				// int sourceOffset=sourceReader.readInt();
				int firstIndex = sourceReader.readInt();
				sourceReader.close();
				sourcePath = sourceDir.getAbsolutePath() + "\\" + author + "\\" + book + ".txt";
				frame.setTitle(author + ": " + book);
				MS.setFirstIndex(firstIndex);
			}

			class DynamicMenu extends JMenu
			implements ChangeListener, ComponentListener, ActionListener, FocusListener, MenuListener {

		private static final long serialVersionUID = 1L;

		void populateSources() {
			File authorDir = new File(sourceDir.getAbsolutePath() + "\\" + getText());
			FilenameFilter onlySCFiles = new FilenameFilter() {
				public boolean accept(File dir, String name) {
					return name.endsWith(".SourceCache");
				}
			};
			this.removeAll();
			for (String book : authorDir.list(onlySCFiles)) {
				int li;
				if ((li = book.lastIndexOf(".txt.SourceCache")) >= 0) {
					JMenuItem ji = new JMenuItem(book.substring(0, li));
					ji.addActionListener(this);
					add(ji);
				}
			}
		}

		public DynamicMenu(String author) {
			super(author);
			MenuScroller.setScrollerFor(this, 10, 50, 0, 0);
			addFocusListener(this);
			addMenuListener(this);
		}

		@Override
		public void componentResized(ComponentEvent e) {
			JMenuItem mi = (JMenuItem) e.getSource();
			JPopupMenu o = (JPopupMenu) mi.getParent();
			DynamicMenu dm = (DynamicMenu) o.getInvoker();
			System.out.println("component resized " + mi.getText() + "[" + dm.getText() + "]");
			// dm.populateSources();
		}

		@Override
		public void componentMoved(ComponentEvent e) {
			JMenuItem mi = (JMenuItem) e.getSource();
			JPopupMenu o = (JPopupMenu) mi.getParent();
			DynamicMenu dm = (DynamicMenu) o.getInvoker();
			System.out.println("component moved " + mi.getText() + "[" + dm.getText() + "]");
		}

		@Override
		public void componentShown(ComponentEvent e) {
			JMenuItem mi = (JMenuItem) e.getSource();
			JPopupMenu o = (JPopupMenu) mi.getParent();
			DynamicMenu dm = (DynamicMenu) o.getInvoker();
			System.out.println("component shown " + mi.getText() + "[" + dm.getText() + "]");
		}

		@Override
		public void componentHidden(ComponentEvent e) {
		}

		@Override
		public void stateChanged(ChangeEvent e) {
			JMenuItem mi = (JMenuItem) e.getSource();
			JPopupMenu o = (JPopupMenu) mi.getParent();
			DynamicMenu dm = (DynamicMenu) o.getInvoker();
			System.out.println("state changed " + mi.getText() + "[" + dm.getText() + "]");
		}

		@Override
		public void actionPerformed(ActionEvent e) {
			JMenuItem book = (JMenuItem) e.getSource();
			JPopupMenu o = (JPopupMenu) book.getParent();
			DynamicMenu author = (DynamicMenu) o.getInvoker();
			// save author and book in last loaded file
			try {
				writeState(book, author, 0);
			} catch (IOException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}
			sourcePath = sourceDir.getAbsolutePath() + "\\" + author.getText() + "\\" + book.getText() + ".txt";
			loadSelectedSource();
			frame.setTitle(author.getText() + ": " + book.getText());
		}

		@Override
		public void focusGained(FocusEvent e) {
			JMenuItem mi = (JMenuItem) e.getSource();
			JPopupMenu o = (JPopupMenu) mi.getParent();
			DynamicMenu dm = (DynamicMenu) o.getInvoker();
			System.out.println("focusGained  " + mi.getText() + "[" + dm.getText() + "]");
		}

		@Override
		public void focusLost(FocusEvent e) {
			JMenuItem mi = (JMenuItem) e.getSource();
			JPopupMenu o = (JPopupMenu) mi.getParent();
			DynamicMenu dm = (DynamicMenu) o.getInvoker();
			System.out.println("focusLost  " + mi.getText() + "[" + dm.getText() + "]");
		}

		@Override
		public void menuCanceled(MenuEvent e) {

		}

		@Override
		public void menuDeselected(MenuEvent e) {

		}

		@Override
		public void menuSelected(MenuEvent e) {
			DynamicMenu mi = (DynamicMenu) e.getSource();
			JPopupMenu o = (JPopupMenu) mi.getParent();
			JMenu dm = (JMenu) o.getInvoker();
			System.out.println("menuSelected  " + mi.getText() + "[" + dm.getText() + "]");
			mi.populateSources();
		}

	}

			/** Returns an ImageIcon, or null if the path was invalid. */
//	    protected ImageIcon createImageIcon(String path) {
//	        java.net.URL imgURL = Show.class.getResource(path);
//	        if (imgURL != null) {
//	            return new ImageIcon(imgURL);
//	        } else {
//	            System.err.println("Couldn't find file: " + path);
//	            return null;
//	        }
//	    }
			void createSourceMenu() {
				menuBar = new JMenuBar();
				// Build the first menu.
				pickSourceMenu = new JMenu("Pick a book");
				pickSourceMenu.setMnemonic(KeyEvent.VK_A);
				pickSourceMenu.getAccessibleContext().setAccessibleDescription("Authors and their books");
				searchSourceMenu = new JMenu("Search results");
				searchSourceMenu.setMnemonic(KeyEvent.VK_S);
				MS = MenuScroller.setScrollerFor(pickSourceMenu, 30, 10, 0, 0);
				MenuScroller.setScrollerFor(searchSourceMenu, 30, 20, 0, 0);
				menuBar.add(pickSourceMenu);
				menuBar.add(searchField = new JTextField(20));
				menuBar.add(searchSourceMenu);
				ActionListener searchListener=new ActionListener() {

					@Override
					public void actionPerformed(ActionEvent arg0) {
						// get # item
						JMenuItem item=(JMenuItem)arg0.getSource();
						for (int itempos=0; itempos<searchSourceMenu.getItemCount(); itempos++)
							if (item==searchSourceMenu.getItem(itempos))
							{
								try {
									gotoPosition(source.batchDoc.findWordPosition(searchField.getText(),itempos));
								} catch (BadLocationException e) {
									// TODO Auto-generated catch block
									e.printStackTrace();
								}

							}
						
					}
					
				};
				searchField.getDocument().addDocumentListener(new DocumentListener() {
					  public void changedUpdate(DocumentEvent e) {
					  }
					  public void removeUpdate(DocumentEvent e) {
							source.batchDoc.addSurroundingContextAndListenerToSearchMenu(searchField.getText(), searchSourceMenu, searchListener);
					  }
					  public void insertUpdate(DocumentEvent e) {
							source.batchDoc.addSurroundingContextAndListenerToSearchMenu(searchField.getText(), searchSourceMenu, searchListener);
					  }

					});

				sourceDir = new File("M:\\caches\\texts");
				if (!sourceDir.exists())
					return;
				FileFilter onlyDirectories = new FileFilter() {
					public boolean accept(File file) {
						return (file.isDirectory());
					}
				};
				Map<String, JMenu> letterMenuMap = new HashMap<String, JMenu>();
				for (char c = 'A'; c <= 'Z'; c++) {
					letterMenuMap.put("" + c, new JMenu("" + c));
					pickSourceMenu.add(letterMenuMap.get("" + c));
					MS = MenuScroller.setScrollerFor(letterMenuMap.get("" + c), 20, 10, 0, 0);
				}
				for (File author : sourceDir.listFiles(onlyDirectories)) {
					String authorMenuIndex = "" + author.getName().toUpperCase().charAt(0);
					if (letterMenuMap.get(authorMenuIndex) == null) {
						letterMenuMap.put(authorMenuIndex, new JMenu(authorMenuIndex));
						pickSourceMenu.add(letterMenuMap.get(authorMenuIndex));
						MS = MenuScroller.setScrollerFor(letterMenuMap.get(authorMenuIndex), 30, 10, 0, 0);
					}
					letterMenuMap.get(authorMenuIndex).add(new DynamicMenu(author.getName()));
				}
			}

			void loadSelectedSource() {
				frame.setVisible(false);
				mainPane.setVisible(false);
				timelineFrame.setVisible(false);
				agentFrame.setVisible(false);
				wordInfoFrame.setVisible(false);
				Words.readSpecificWordCache(new LittleEndianDataInputStream(sourcePath + ".wordCacheFile"));
				source = new Source(Words, new LittleEndianDataInputStream(sourcePath + ".SourceCache"));
				SwingUtilities.invokeLater(new Runnable() {
					public void run() {
						mainPane.setStyledDocument(source.print(Words, pickChapter, ccb.states));
						agentPane.setStyledDocument(source.fillAgents());
						timelineFrame.setContentPane(timelinePane = new TimelineTreePanel(source));
						timelineFrame.setBounds(1200, 600, 600, 500);
						mainPane.setVisible(true);
						timelineFrame.setVisible(true);
						agentFrame.setVisible(true);
						wordInfoFrame.setVisible(true);
						frame.setVisible(true);
					}
				});
			}

			class ButtonListener implements ActionListener {

				@Override
				public void actionPerformed(ActionEvent e) {

					Rectangle b = mainPane.getBounds();
					Point pt = new Point(b.x, -b.y);
					Position.Bias[] biasRet = new Position.Bias[1];
					int pos = mainPane.getUI().viewToModel(mainPane, pt, biasRet);
					try {
						source.positionToSource.floorKey(pos);
					} catch (RuntimeException e1) {
						e1.printStackTrace();
					}
					loadSelectedSource();
				}
			}

			class UberHandler extends TransferHandler {
				/**
				 * 
				 */
				private static final long serialVersionUID = 1L;

				public boolean canImport(JComponent dest, DataFlavor[] flavors) {
					// you bet we can!
					return true;
				}

				public boolean importData(JComponent src, Transferable transferable) {
					// Ok, here's the tricky part...
					println("Receiving data from " + src);
					println("Transferable object is: " + transferable);
					// println("Valid data flavors: ");
					DataFlavor[] flavors = transferable.getTransferDataFlavors();
					DataFlavor listFlavor = null;
					DataFlavor objectFlavor = null;
					DataFlavor readerFlavor = null;
					int lastFlavor = flavors.length - 1;

					// Check the flavors and see if we find one we like.
					// If we do, save it.
					for (int f = 0; f <= lastFlavor; f++) {
						// println(" " + flavors[f]);
						if (flavors[f].isFlavorJavaFileListType()) {
							listFlavor = flavors[f];
						}
						if (flavors[f].isFlavorSerializedObjectType()) {
							objectFlavor = flavors[f];
						}
						if (flavors[f].isRepresentationClassReader()) {
							readerFlavor = flavors[f];
						}
					}

					// Ok, now try to display the content of the drop.
					try {
						DataFlavor bestTextFlavor = DataFlavor.selectBestTextFlavor(flavors);
						BufferedReader br = null;
						String line = null, tline;
						if (bestTextFlavor != null) {
							bestTextFlavor = DataFlavor.getTextPlainUnicodeFlavor();
							// println("Best text flavor: " + bestTextFlavor.getMimeType());
							// println("Content:");
							Reader r = null;
							try {
								r = bestTextFlavor.getReaderForText(transferable);
							} catch (UnsupportedFlavorException e) {
								bestTextFlavor = null;
							}
							if (bestTextFlavor != null) {
								br = new BufferedReader(r);
								line = tline = br.readLine();
								while (tline != null) {
									if ((tline = br.readLine()) != null)
										line += tline;
								}
								println("Content:" + line);
								int sourceIndex = -1;
								try {
									sourceIndex = Integer.decode(line);
									gotoPosition(source.sourceToPosition.get(sourceIndex + 40));
								} catch (NumberFormatException e) {
									println(" NumberFormatException");
								} catch (IllegalArgumentException e2) {
									println(" IllegalArgumentException");
								}
								br.close();
							}
						}
						if (bestTextFlavor == null) {
							if (listFlavor != null) {
								java.util.List list = (java.util.List) transferable.getTransferData(listFlavor);
								println(list);
								if (list.size() == 1 && list.get(0) instanceof File) {
									String path = ((File) list.get(0)).getAbsolutePath();
									if (path.endsWith("SourceCache")) {
										sourcePath = path.substring(0, path.lastIndexOf(".SourceCache"));
										loadSelectedSource();
										frame.setTitle(path);
									}
								}
							} else if (objectFlavor != null) {
								line = transferable.getTransferData(objectFlavor).toString();
								println("Data is a java object:\n" + line);
								int sourceIndex = -1;
								try {
									sourceIndex = Integer.decode(line);
									gotoPosition(source.sourceToPosition.get(sourceIndex + 40));
								} catch (NumberFormatException e) {
									println(" NumberFormatException");
								} catch (IllegalArgumentException e2) {
									println(" IllegalArgumentException");
								}
							} else if (readerFlavor != null) {
								println("Data is an InputStream:");
								br = new BufferedReader((Reader) transferable.getTransferData(readerFlavor));
								line = br.readLine();
								while (line != null) {
									println(line);
								}
								br.close();
							} else {
								// Don't know this flavor type yet...
								println("No text representation to show.");
							}
						}
						println("\n\n");
					} catch (Exception e) {
						println("Caught exception decoding transfer:");
						println(e);
						e.printStackTrace();
						return false;
					}
					return false;
				}

				public void exportDone(JComponent source, Transferable data, int action) {
					// Just let us know when it occurs...
					System.err.println("Export Done.");
				}

				public void setOutput(JTextArea wordInfo) {
				}

				protected void println(Object o) {
					println(o.toString());
				}

				protected void println(String s) {
					System.out.println(s);
				}

			}

			public void run() {
				vn = new VerbNet();
				initializeDisplay();
				boolean found = false;
				try {
					readState();
				} catch (IOException e2) {
					// TODO Auto-generated catch block
					System.out.println("GUI state file not found.");
				}
				if (sourcePath == null || !(found = new File(sourcePath + ".wordCacheFile").exists()
						&& new File(sourcePath + ".SourceCache").exists())) {
					// find first entry which has wordCacheFile and SourceCache
					for (Component author : pickSourceMenu.getMenuComponents()) {
						if (!author.getClass().getName().equals("DynamicMenu"))
							continue;
						((DynamicMenu) author).populateSources();
						for (Component book : ((DynamicMenu) author).getMenuComponents()) {
							sourcePath = sourceDir.getAbsolutePath() + "\\" + ((DynamicMenu) author).getText() + "\\"
									+ ((JMenuItem) book).getText() + ".txt";
							frame.setTitle(((DynamicMenu) author).getText() + ": " + ((JMenuItem) book).getText());
							if (found = new File(sourcePath + ".wordCacheFile").exists()
									&& new File(sourcePath + ".SourceCache").exists())
								break;
						}
						if (found)
							break;
					}
				}
				// path= "C:\\lp\\tests\\timeExpressions.txt";
				// path="G:\\wikipediaCache\\_Llanelly.txt";
				Words = new WordClass(new LittleEndianDataInputStream("F:\\lp\\wordFormCache"));
				Words.readSpecificWordCache(new LittleEndianDataInputStream(sourcePath + ".wordCacheFile"));
				source = new Source(Words, new LittleEndianDataInputStream(sourcePath + ".SourceCache"));
				mainPane.setStyledDocument(source.print(Words, pickChapter, ccb.states));
				agentPane.setStyledDocument(source.fillAgents());
				timelineFrame.setContentPane(timelinePane = new TimelineTreePanel(source));
				timelineFrame.setBounds(1200, 600, 600, 500);
				mainPane.setCaretPosition(0);
				// Define ActionListener
				MouseListener actionListener = new MouseListener() {

					@Override
					public void mouseClicked(MouseEvent e) {
						// TODO Auto-generated method stub

					}

					@Override
					public void mousePressed(MouseEvent e) {
						Point pt = e.getPoint();
						Position.Bias[] biasRet = new Position.Bias[1];
						int pos = mainPane.getUI().viewToModel(mainPane, pt, biasRet), index = 0;
						try {
							index = source.positionToSource.floorKey(pos);
						} catch (RuntimeException e1) {
							e1.printStackTrace();
						}
						Source.WhereSource ws = source.positionToSource.get(index);
						if (ws == null) {
							System.out.println("source not found for index " + index);
							return;
						}
						switch (ws.flags) {
						case transitionSpeakerGroupType:
							for (int sg = ws.index + 1; sg < Source.speakerGroups.length; sg++)
								if (Source.speakerGroups[sg].tlTransition) {
									try {
										gotoPosition(source.sourceToPosition.get(Source.speakerGroups[sg].begin + 40));
									} catch (BadLocationException e1) {
										// TODO Auto-generated catch block
										e1.printStackTrace();
									}
									break;
								}
						case headerType:
							if (ws.index2 + 1 < Source.sections.length)
								try {
									gotoPosition(
											source.sourceToPosition.get(Source.sections[ws.index2 + 1].begin + 40));
								} catch (BadLocationException e1) {
									// TODO Auto-generated catch block
									e1.printStackTrace();
								} catch (NullPointerException e1) {
									// e1.printStackTrace();
								}
						default:
							break;
						}
					}

					@Override
					public void mouseReleased(MouseEvent e) {
						// TODO Auto-generated method stub

					}

					@Override
					public void mouseEntered(MouseEvent e) {
						// TODO Auto-generated method stub

					}

					@Override
					public void mouseExited(MouseEvent e) {
						// TODO Auto-generated method stub

					}

				};
				MouseMotionListener actionMotionListener = new MouseMotionListener() {
					@Override
					public void mouseDragged(MouseEvent e) {
					}

					@Override
					public void mouseMoved(MouseEvent e) {
						if (source.positionToSource.isEmpty())
							return;
						Point pt = e.getPoint();
						Position.Bias[] biasRet = new Position.Bias[1];
						int pos = mainPane.getUI().viewToModel(mainPane, pt, biasRet), index = 0;
						try {
							index = source.positionToSource.floorKey(pos);
						} catch (RuntimeException e1) {
							e1.printStackTrace();
						}
						Source.WhereSource ws = source.positionToSource.get(index);
						if (ws == null) {
							System.out.println("source not found for index " + index);
							return;
						}
						switch (ws.flags) {
						case wordType:
							wordInfo.setText(
									ws.index + ": " + source.m[ws.index].getWinnerForms() + source.m[ws.index].word);
							mainPane.setToolTipText(source.m[ws.index].word);
							roleInfo.setText(source.m[ws.index].roleString());
							relationsInfo.setText(source.m[ws.index].relations());
							if (source.m[ws.index].baseVerb.length() > 0) {
								roleInfo.setText(
										VerbMember.toString(vn.vbNetVerbToClassMap.get(source.m[ws.index].baseVerb)));
								String verbSenseStr = source.getVerbSenseString(source.m[ws.index].quoteForwardLink);
								wordInfo.setText(ws.index + ": " + source.m[ws.index].word + "(" + verbSenseStr + ")");
							}
							break;
						case relType:
							Relation r = Source.relations[ws.index2];
							if (r.beforePastHappening)
								wordInfo.setText("beforePast");
							if (r.futureHappening)
								wordInfo.setText("future");
							if (r.futureInPastHappening)
								wordInfo.setText("futureInPast");
							if (r.futureLocation)
								wordInfo.setText("futureLocation");
							if (r.pastHappening)
								wordInfo.setText("past");
							if (r.presentHappening)
								wordInfo.setText("present");
							int firstMultiple = ws.index2 - 1;
							while (firstMultiple >= 0 && Source.relations[firstMultiple].where == r.where)
								firstMultiple--;
							firstMultiple++;
							int lastMultiple = firstMultiple + 1;
							while (lastMultiple < Source.relations.length
									&& Source.relations[lastMultiple].where == r.where)
								lastMultiple++;
							lastMultiple--;
							relationsInfo.setText(((firstMultiple != lastMultiple) ? "M " : "")
									+ ((r.timeTransition) ? "TT " : "") + r.description);
							roleInfo.setText(cTimeInfo.toString(source, r));
							break;

						case transitionSpeakerGroupType:
						case relType2:
						case speakerGroupType:
						case QSWordType:
						case ESType:
						case URWordType:
						case matchingSpeakerType:
						case matchingType:
						case audienceMatchingType:
						case matchingObjectType:
						case headerType:
							wordInfo.setText(
									ws.index + ": index2=" + ws.index2 + " index3=" + ws.index3 + " flags=" + ws.flags);
							break;
						case beType:
							int object = source.m[ws.index].object;
							if (source.m[ws.index].objectMatches.length == 1)
								object = source.m[ws.index].objectMatches[0].object;
							wordInfo.setText(ws.index + ":" + source.objectString(object, false, false));
							relationsInfo.setText("acquired through BE relation:");
							if (object >= 0) {
								String asadj = "";
								for (int I = 0; I < source.objects[object].associatedAdjectives.length; I++)
									asadj += source.objects[object].associatedAdjectives[I];
								roleInfo.setText(asadj);
							}
							break;
						case possessionType:
							object = source.m[ws.index].object;
							if (source.m[ws.index].objectMatches.length == 1)
								object = source.m[ws.index].objectMatches[0].object;
							wordInfo.setText(ws.index + ":" + source.objectString(object, false, false));
							relationsInfo.setText("HAS-A relation:");
							if (object >= 0) {
								String asadj = "";
								for (int I = 0; I < source.objects[object].possessions.length; I++)
									asadj += source.objects[object].possessions[I];
								roleInfo.setText(asadj);
							}
						default:
							break;
						}
					}
				};
				CaretListener caretListener = new CaretListener() {
					public void caretUpdate(CaretEvent e) {
						int fromSelection = e.getMark();
						int toSelection = e.getDot();
						if (fromSelection != toSelection) {
							int fromIndex = source.positionToSource.ceilingKey(fromSelection), toIndex;
							try {
								toIndex = source.positionToSource.ceilingKey(toSelection);
							} catch (Exception ect) {
								System.out.println("Caught exception decoding transfer:");
								System.out.println(ect);
								ect.printStackTrace();
								return;
							}
							int tableRow = 0;
							Source.WhereSource lastWS = null;
							for (int I = fromIndex; I < toIndex; I++) {
								Source.WhereSource ws = source.positionToSource.get(I);
								if (ws != null && (lastWS == null || lastWS.index != ws.index)) {
									while (wordInfoTableData.size() <= tableRow) {
										wordInfoTableData.add(new Vector<String>());
										for (int col = 0; col < wordInfoTableColumnNames.size(); col++)
											wordInfoTableData.get(wordInfoTableData.size() - 1).add(col, new String());
									}
									String role = "";
									wordInfoSelectionTable.setValueAt(ws.index + ":"
											+ source.m[ws.index].getWinnerForms() + source.m[ws.index].word, tableRow,
											0);
									wordInfoSelectionTable.setValueAt(role = source.m[ws.index].roleString(), tableRow,
											1);
									wordInfoSelectionTable.setValueAt(source.m[ws.index].relations(), tableRow, 2);
									wordInfoSelectionTable.setValueAt(
											source.objectString(source.m[ws.index].object, false, false), tableRow, 3);
									wordInfoSelectionTable.setValueAt(
											source.objectString(source.m[ws.index].objectMatches, false, true),
											tableRow, 4);
									if (source.m[ws.index].object >= 0
											&& source.objects[source.m[ws.index].object].locations != null)
										System.out.print(source.objectString(source.m[ws.index].object, false, false)
												+ ":" + source.objects[source.m[ws.index].object].locations);
									wordInfoSelectionTable.setValueAt(source.m[ws.index].printFlags(), tableRow, 5);
									// System.out.println(ws.index + ": " + source.m[ws.index].word +
									// ":"+source.m[ws.index].relations());
									if (source.m[ws.index].baseVerb.length() > 0 && role.length() == 0) {
										Vector<VerbMember> vms = (source.m[ws.index].baseVerb.length() > 0)
												? vn.vbNetVerbToClassMap.get(source.m[ws.index].baseVerb)
												: null;
										if (vms != null) {
											String names = "";
											for (VerbMember vm : vms)
												names += vm.name + " ";
											wordInfoSelectionTable.setValueAt(names, tableRow, 1);
										}
									}
									tableRow++;
									lastWS = ws;
								}
							}
							for (int I = tableRow; I < wordInfoSelectionTable.getRowCount(); I++)
								for (int J = 0; J < wordInfoSelectionTable.getColumnCount(); J++)
									wordInfoSelectionTable.setValueAt(null, I, J);
							wordInfoSelectionTable.revalidate();
						}
					}

				};
				// Attach listeners
				mainPane.addMouseMotionListener(actionMotionListener);
				mainPane.addMouseListener(actionListener);
				mainPane.addCaretListener(caretListener);
				frame.setSize(600, 1160);
				frame.setVisible(true);
			}
		};
		EventQueue.invokeLater(runner);
	}

	@Override
	public void itemStateChanged(ItemEvent e) {
		// TODO Auto-generated method stub

	}

	@Override
	public void actionPerformed(ActionEvent e) {
		// TODO Auto-generated method stub

	}
}