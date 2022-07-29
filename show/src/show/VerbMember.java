package show;

import java.util.Arrays;
import java.util.Map;
import java.util.Vector;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

import com.sun.org.apache.xerces.internal.dom.DeferredCommentImpl;
import com.sun.org.apache.xerces.internal.dom.DeferredTextImpl;

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
			for (int i = 0; i < childNodes.getLength(); i++) {
				if (!(childNodes.item(i) instanceof DeferredTextImpl)
						&& !(childNodes.item(i) instanceof DeferredCommentImpl)) {
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
