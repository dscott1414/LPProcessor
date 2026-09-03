package show;

import java.util.Arrays;
import java.util.Map;
import java.util.Vector;

import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

public class VerbMember {
	private String name;
	private Vector<String> kinds;
	private Vector<String> members;
	private static String[] vbClassTypesArray = { "NO_PHYSICAL_ACTION", "CONTROL", "CONTACT", "NEAR", "LOCATIONOBJECT",
			"LOCATIONPREPOBJECT", "TRANSFER", "NO_PREP_TO", "NO_PREP_FROM", "MOVE", "MOVE_OBJECT", "MOVE_IN_PLACE",
			"EXIT", "ENTER", "CONTIGUOUS", "START", "STAY", "COMMUNICATE", "THINK", "THINK_OBJECT", "SENSE", "CREATE",
			"CONSUME", "CHANGE_STATE", "AGENT_CHANGE_OBJECT_INTERNAL_STATE", "META_PROFESSION", "META_FUTURE_HAVE",
			"META_FUTURE_CONTACT", "META_INFO", "META_IF_THEN", "META_CONTAINS", "META_DESIRE", "META_BELIEF",
			"META_ROLE", "SPATIAL_ORIENTATION", "IGNORE" };
	private static boolean sorted = false;

	public VerbMember(String n, NodeList nl, NodeList childNodes, Map<String, Vector<VerbMember>> vbNetVerbToClassMap) {
		if (!sorted) {
			Arrays.sort(vbClassTypesArray);
			sorted = true;
		}
		name = n;
		members = new Vector<String>();
		kinds = new Vector<String>();
		String member;
		if (nl != null && nl.getLength() > 0) {
			for (int i = 0; i < nl.getLength(); i++) {
				members.add(member = ((Element) nl.item(i)).getAttribute("name"));
				Vector<VerbMember> key = vbNetVerbToClassMap.get(member);
				if (key == null)
					key = new Vector<VerbMember>();
				key.add(this);
				vbNetVerbToClassMap.put(member, key);
			}
		}
		if (childNodes != null && childNodes.getLength() > 0) {
			// (fixed) used to exclude text/comment nodes by checking
			// `instanceof` against internal JDK impl classes
			// (com.sun.org.apache.xerces.internal.dom.Deferred{Text,Comment}Impl),
			// which are not part of the public API and are inaccessible
			// under the JDK 9+ module system (this file fails to compile
			// at all on a modern JDK as a result). Checking
			// getNodeType()==ELEMENT_NODE is the portable, standard DOM
			// way to ask "is this actually an Element", and is strictly
			// more correct too (it also excludes CDATA/PI/etc. nodes that
			// the old instanceof pair didn't account for).
			for (int i = 0; i < childNodes.getLength(); i++) {
				if (childNodes.item(i).getNodeType() == Node.ELEMENT_NODE) {
					member = ((Element) childNodes.item(i)).getNodeName();
					if (Arrays.binarySearch(vbClassTypesArray, member) >= 0)
						kinds.add(member);
				}
			}
		}
	}

	String getName() {
		return name;
	}
	
	String getNameWithKinds() {
		String names = name + " ";
		if (kinds.size()>0)
			names += "[";
		for (String kind : kinds)
			names += kind + " ";
		if (kinds.size() > 0)
			names += "] ";
		return names;
	}
}
