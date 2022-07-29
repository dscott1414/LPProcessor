package show;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Vector;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

public class VerbNet {

	private Vector<VerbMember> vms;
	private Map<String, Vector<VerbMember>> vbNetVerbToClassMap;

	private void parseXmlFile(String pathName) {
		// get the factory
		DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
		try {

			// Using factory get an instance of document builder
			DocumentBuilder db = dbf.newDocumentBuilder();
			// parse using builder to get DOM representation of the XML file
			Document dom = db.parse(pathName);
			// get the root element
			Element docElement = dom.getDocumentElement();
			// get a node list of elements
			NodeList nl = docElement.getElementsByTagName("MEMBERS");
			if (nl != null && nl.getLength() > 0) {
				for (int i = 0; i < nl.getLength(); i++) {
					Node parent = ((Element) nl.item(i)).getParentNode();
					NamedNodeMap nm = parent.getAttributes();
					Node n = nm.getNamedItem("ID");
					// System.out.print(n.getNodeValue()+"
					// length="+((Element)nl.item(i)).getElementsByTagName("MEMBER").getLength()+"\n");
					vms.add(new VerbMember(n.getNodeValue(), ((Element) nl.item(i)).getElementsByTagName("MEMBER"),
							parent.getChildNodes(), vbNetVerbToClassMap));
				}
			}
		} catch (ParserConfigurationException pce) {
			pce.printStackTrace();
		} catch (SAXException se) {
			se.printStackTrace();
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}
	}

	public VerbNet() {
		File vnDir = new File("F:\\lp\\source\\lists\\VerbNet");
		FilenameFilter onlyXMLFiles = new FilenameFilter() {
			public boolean accept(File dir, String name) {
				return name.endsWith(".xml");
			}
		};
		vbNetVerbToClassMap = new HashMap<String, Vector<VerbMember>>();
		vms = new Vector<VerbMember>();
		for (String vnXMLFile : vnDir.list(onlyXMLFiles))
			parseXmlFile(vnDir.getAbsolutePath() + "\\" + vnXMLFile);
	}
	
	private  Vector<VerbMember> getVerbClasses(String baseVerb, String phrasalVerb) {
		Vector<VerbMember> vms = Show.vn.vbNetVerbToClassMap.get(baseVerb);
		if (phrasalVerb.length() > 0)
		{
			Vector<VerbMember> vmsParticiple = Show.vn.vbNetVerbToClassMap.get(phrasalVerb);
			if (vmsParticiple != null) 
				vms = vmsParticiple;
		}
		return vms;
	}

	private  boolean like(String str1,String str2)
	{
		int min=Math.min(str1.length(),str2.length());
		return str1.substring(0, min).equals(str2.substring(0,min));
	}
	
	public String getClassNames(String baseVerb) {
		String names = "";
		if (baseVerb.length() == 0)
			return names;
		Vector<VerbMember> vmsl = vbNetVerbToClassMap.get(baseVerb);
		if (vmsl != null) {
			for (VerbMember vm : vmsl)
				names += vm.getName() + " ";
		}
		return names;
	}
	
	public String getClassNames2(String baseVerb) {
		String names = "";
		if (vbNetVerbToClassMap.get(baseVerb) == null)
			System.out.println("Cannot find verb " + baseVerb);
		else			
			for (VerbMember vm : vbNetVerbToClassMap.get(baseVerb)) 
				names += vm.getNameWithKinds();
		return names;
	}
	
	public  boolean isVerbClass(String baseVerb, String phrasalVerb, String verbClass)
	{
		Vector <VerbMember> vms=getVerbClasses(baseVerb, phrasalVerb);
		if (vms!=null)
			for (VerbMember vm:vms)
				if (like(vm.getName(),verbClass)) return true;
		return false;
	}
}
