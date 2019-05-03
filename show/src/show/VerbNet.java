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
	
	Vector <VerbMember> vms;
  Map<String,Vector <VerbMember>> vbNetVerbToClassMap;

	private void parseXmlFile(String pathName){
		//get the factory
		DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
		try {

			//Using factory get an instance of document builder
			DocumentBuilder db = dbf.newDocumentBuilder();
			//parse using builder to get DOM representation of the XML file
			Document dom = db.parse(pathName);
			//get the root element
			Element docElement = dom.getDocumentElement();
			//get a node list of elements
			NodeList nl=docElement.getElementsByTagName("MEMBERS");
			if(nl != null && nl.getLength() > 0) {
				for(int i = 0 ; i < nl.getLength();i++) {
					Node parent=((Element)nl.item(i)).getParentNode();
					NamedNodeMap nm=parent.getAttributes();
					Node n=nm.getNamedItem("ID");
					//System.out.print(n.getNodeValue()+" length="+((Element)nl.item(i)).getElementsByTagName("MEMBER").getLength()+"\n");
					vms.add(new VerbMember(n.getNodeValue(),((Element)nl.item(i)).getElementsByTagName("MEMBER"),parent.getChildNodes(),vbNetVerbToClassMap));
				}
			}
		}catch(ParserConfigurationException pce) {
			pce.printStackTrace();
		}catch(SAXException se) {
			se.printStackTrace();
		}catch(IOException ioe) {
			ioe.printStackTrace();
		}
	}

	public VerbNet()
	{
		File vnDir = new File("F:\\lp\\source\\lists\\VerbNet");
		FilenameFilter onlyXMLFiles = new FilenameFilter() {
			public boolean accept(File dir, String name) {
				return name.endsWith(".xml");
			}
		};
		vbNetVerbToClassMap=new HashMap<String,Vector <VerbMember>>();
		vms=new Vector <VerbMember>();
		for (String vnXMLFile : vnDir.list(onlyXMLFiles))
			parseXmlFile(vnDir.getAbsolutePath() + "\\"+ vnXMLFile);
	}
}
