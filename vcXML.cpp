/*
	vcXML.cpp - Hand-rolled VerbNet XML loader into cVerbNet / vbNetClasses

	Overview:
		Walks VerbNet 3.x class XML with pointer/offset helpers (no XML library):
		tX/aH/tA/lineX/endX parse tags and attributes; aVN* helpers consume
		SELRESTRS, SYNRESTRS, NP, PREP, SYNTAX, SEMANTICS; aVNCLASS loads one
		VNCLASS/VNSUBCLASS recursively (subclasses nest). Custom empty tags
		(MOVE/, THINK/, META_BELIEF/, …) set the cVerbNet semantic flags.
		readVBNet() glob-loads source\lists\VerbNet\*.xml and appends a synthetic
		"am/become/be" class.

	Pipeline position:
		Initialization, before pattern construction. Later stages query
		vbNetVerbToClassMap and vbNetClasses.

	Key entry points:
		- tX / aH / tA / lineX / endX - low-level tag/attribute readers
		- absorbCommonVerbNetClasses - LP overlay flags
		- aVNCLASS - one class + nested subclasses
		- readVBNet - directory scan + synthetic BE class

	Dependencies:
		source/lists/VerbNet/*.xml; tmalloc; mTW (MBCS->wide); lpDirectoryEntries.

	Notes / gotchas:
		Parser assumes well-formed VerbNet XML and mutates the wide buffer in place
		(temporarily zeros delimiters). readVBNet enumerates the directory up front on
		every return path, including a mid-scan lp_wopen failure.
*/
#pragma warning(disable : 4786 ) // disable warning C4786
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include <stdlib.h>
#include "wn.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "vcXML.h"
#include "profile.h"

int numMembers = 0;
unordered_map <lpwstring, set <int> > vbNetVerbToClassMap;

// Copies buf[offset..endChar) into s and advances offset past endChar. Returns false if endChar is absent.
bool tX(lpchar_t* buf, int64_t& offset, lpwstring& s, lpchar_t endChar)
{
	LFS
		lpchar_t* ch = lp_strchr(buf + offset, endChar);
	if (ch == NULL) return false;
	lpchar_t savech = *ch;
	*ch = 0;
	s = buf + offset;
	*ch = savech;
	offset = (ch - buf + 1);
	return true;
}

// Reads a tag name from buf[offset] up to endChar or '>'. Advances offset to the delimiter
// (and past it if it was endChar). Returns false only if both delimiters are missing.
// <VNCLASS
bool aH(lpchar_t* buf, int64_t& offset, lpwstring& s, lpchar_t endChar)
{
	LFS
		lpchar_t* ch = lp_strchr(buf + offset, endChar);
	lpchar_t* ech = lp_strchr(buf + offset, u'>');
	if (ch == NULL && ech == NULL) return false;
	// ech may be NULL (no '>' left in the buffer) even when ch was found; only compare the two
	// pointers when both are non-NULL, otherwise a NULL ech would compare as "less than" any
	// valid ch and *ch=0 below would write through a NULL ch.
	if (ch == NULL || (ech != NULL && ech < ch))
		ch = ech;
	lpchar_t savech = *ch;
	*ch = 0;
	s = buf + offset;
	*ch = savech;
	offset = (ch - buf);
	if (*ch == endChar) offset++;
	return true;
}

// Parses one name="value" attribute at offset into attr. Returns false at /> or > or on bad syntax.
// ID="say-37.7"
bool tA(lpchar_t* buf, int64_t& offset, vector <cXMLAttribute>& attr)
{
	LFS
		if ((buf[offset] == u'/' && buf[offset + 1] == u'>') || (buf[offset] == u'?' && buf[offset + 1] == u'>') || buf[offset] == u'>')
			return false;
	lpwstring a, as;
	while (iswspace(buf[offset])) offset++;
	if (!tX(buf, offset, a, u'=')) return false;
	while (iswspace(buf[offset])) offset++;
	offset++;
	if (!tX(buf, offset, as, u'\"')) return false;
	if (buf[offset] == u' ') offset++;
	attr.push_back(cXMLAttribute(a, as));
	return true;
}

// Parses one start tag. If the name equals expectedClass (or absorbNonExpectedClassAnyway),
// pushes a cXMLClass onto vxc. Skips <!-- comments --> recursively. Returns false on mismatch
// or malformed tag. expectedClass starting with '?' is treated as an <?xml ...?> PI.
// <VNCLASS ID="say-37.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="vn_schema-3.xsd">
bool lineX(lpchar_t* buf, int64_t& offset, vector <cXMLClass>& vxc, lpwstring expectedClass, bool absorbNonExpectedClassAnyway = false)
{
	LFS
		while (iswspace(buf[offset])) offset++;
	if (buf[offset] != u'<') return false;
	if (buf[offset + 1] == u'/') return false; // end of some structure
	if (buf[offset + 1] == u'!')
	{
		lpwstring endOfComment;
		while (buf[offset + 2] == u'-')
		{
			endOfComment += buf[offset + 2];
			offset++;
		}
		endOfComment += u">";
		lpchar_t* ch = lp_strstr(buf + offset, endOfComment.c_str());
		if (ch == NULL) return false;
		offset = ch - buf + endOfComment.length() + 1;
		return lineX(buf, offset, vxc, expectedClass, absorbNonExpectedClassAnyway);
	}
	cXMLClass xc;
	int64_t classOffset = offset + 1;
	if (!aH(buf, classOffset, xc.XClass, ' ')) return false;
	if (xc.XClass != expectedClass && !absorbNonExpectedClassAnyway)
		return false;
	offset = classOffset;
	if (expectedClass[0] == u'?')
		return tX(buf, offset, expectedClass, u'>');
	else
	{
		while (tA(buf, offset, xc.av));
		if (!(buf[offset] == u'/' && buf[offset + 1] == u'>') && !(buf[offset] == u'?' && buf[offset + 1] == u'>') && buf[offset] != u'>')
			return false;
		if (buf[offset] == u'>')
			offset++;
		else
			offset += 2;
		if (xc.XClass == expectedClass)
			vxc.push_back(xc);
		return xc.XClass == expectedClass;
	}
}

// Consumes a </TAG> (skipping a '!' comment first). Returns the tag name in tmp, or u"" if not an end tag.
//  </MEMBERS>
lpwstring endX(lpchar_t* buf, int64_t& offset, lpwstring& tmp)
{
	LFS
		while (iswspace(buf[offset])) offset++;
	if (buf[offset + 1] == u'!')
	{
		tX(buf, offset, tmp, u'>');
		return endX(buf, offset, tmp);
	}
	if (buf[offset] != u'<' || buf[offset + 1] != u'/') return u"";
	offset += 2;
	tX(buf, offset, tmp, u'>');
	return tmp;
}

// Consumes <SELRESTRS>…</SELRESTRS> (nested SELRESTR / SELRESTRS). Returns true if at least one
// self-closing or closed block was absorbed.
bool aVNSEL(lpchar_t* buf, int64_t& offset, vector <cXMLClass>& vxc)
{
	LFS
		int atLeastOne = false;
	while (lineX(buf, offset, vxc, u"SELRESTRS"))
	{
		while (lineX(buf, offset, vxc[vxc.size() - 1].vxc, u"SELRESTR") || aVNSEL(buf, offset, vxc));
		atLeastOne = true;
	}
	lpwstring tmp;
	return lineX(buf, offset, vxc, u"SELRESTRS/") || (atLeastOne && endX(buf, offset, tmp) == u"SELRESTRS");
}

// Consumes <SYNRESTRS> with nested <SYNRESTR/> children, or a self-closing SELRESTRS-style tag.
// <SYNRESTRS>
//   <SYNRESTR Value="+" type="quotation"/>
// </SYNRESTRS>
bool aVNSYN(lpchar_t* buf, int64_t& offset, vector <cXMLClass>& vxc)
{
	LFS
		while (lineX(buf, offset, vxc, u"SYNRESTRS"))
			while (lineX(buf, offset, vxc[vxc.size() - 1].vxc, u"SYNRESTR"));
	lpwstring tmp;
	return lineX(buf, offset, vxc, u"SYNRESTRS/") || endX(buf, offset, tmp) == u"SYNRESTRS";
}


// Consumes <NP> plus either SYNRESTRS or SELRESTRS, then </NP>. Returns false if either piece is missing.
//      <NP value="Agent">
//        <SYNRESTRS>
//          <SYNRESTR Value="+" type="quotation"/>
//        </SYNRESTRS>
//      </NP>
bool aVNNP(lpchar_t* buf, int64_t& offset, vector <cXMLClass>& vxc)
{
	LFS
		if (!lineX(buf, offset, vxc, u"NP")) return false;
	if (!aVNSYN(buf, offset, vxc) && !aVNSEL(buf, offset, vxc)) return false;
	lpwstring tmp;
	return endX(buf, offset, tmp) == u"NP";
}

// Tries to consume a self-closing leaf named str (e.g. u"ADV/") without committing offset on failure.
bool aVNLEAF(lpchar_t* buf, int64_t& offset, const lpchar_t* str)
{
	LFS
		vector <cXMLClass> tempxc;
	int64_t advOffset = offset;
	if (lineX(buf, advOffset, tempxc, str))
	{
		offset = advOffset;
		return true;
	}
	return false;
}

// Consumes <PREP> plus SELRESTRS then </PREP>.
// <PREP value="with">
//   <SELRESTRS/>
// </PREP>
bool aVNPREP(lpchar_t* buf, int64_t& offset, vector <cXMLClass>& vxc)
{
	LFS
		if (!lineX(buf, offset, vxc, u"PREP")) return false;
	if (!aVNSEL(buf, offset, vxc)) return false;
	lpwstring tmp;
	return endX(buf, offset, tmp) == u"PREP";
}

// Consumes <SYNTAX> NP/ADV/ADJ/PREP/LEX* <VERB/> NP/ADV/ADJ/PREP/LEX* </SYNTAX>.
// Pre-verb tokens go into vxc; the SYNTAX wrapper itself is discarded into a temp.
//  <SYNTAX>
//      <NP value="Agent">
//        <SYNRESTRS>
//          <SYNRESTR Value="+" type="quotation"/>
//        </SYNRESTRS>
//      </NP>
//      <VERB/>
//      <NP value="Topic">
//          <SYNRESTRS/>
//      </NP>
//  </SYNTAX>
bool aVNSyntax(lpchar_t* buf, int64_t& offset, vector <cXMLClass>& vxc)
{
	LFS
		vector <cXMLClass> tempxc;
	if (!lineX(buf, offset, tempxc, u"SYNTAX")) return false;
	// NP, ADV, ADJ, PREP or LEX
	while (aVNNP(buf, offset, vxc) || aVNLEAF(buf, offset, u"ADV/") || aVNLEAF(buf, offset, u"ADJ/") || aVNPREP(buf, offset, vxc) || lineX(buf, offset, vxc, u"LEX"));
	// VERB
	if (!lineX(buf, offset, tempxc, u"VERB/")) return false;
	// NP, ADV, ADJ, PREP or LEX
	while (aVNNP(buf, offset, vxc) || aVNLEAF(buf, offset, u"ADV/") || aVNLEAF(buf, offset, u"ADJ/") || aVNPREP(buf, offset, vxc) || lineX(buf, offset, vxc, u"LEX"));
	lpwstring end;
	return endX(buf, offset, end) == u"SYNTAX";
}

// Consumes <SEMANTICS><PRED>…<ARGS><ARG/>…</ARGS></PRED>…</SEMANTICS>. PREDs land in vxc.
//  <SEMANTICS>
//      <PRED value="transfer_info">
//          <ARGS>
//              <ARG type="Event" value="during(E)"/>
//          </ARGS>
//      </PRED>
//  </SEMANTICS>
bool aVNSemantics(lpchar_t* buf, int64_t& offset, vector <cXMLClass>& vxc)
{
	LFS
		vector <cXMLClass> tempxc;
	if (!lineX(buf, offset, tempxc, u"SEMANTICS")) return false;
	lpwstring tmp;
	while (lineX(buf, offset, vxc, u"PRED"))
	{
		while (lineX(buf, offset, tempxc, u"ARGS"))
		{
			while (lineX(buf, offset, vxc[vxc.size() - 1].vxc, u"ARG"));
			if (endX(buf, offset, tmp) != u"ARGS") return false;
		}
		if (endX(buf, offset, tmp) != u"PRED") return false;
	}
	if (endX(buf, offset, tmp) != u"SEMANTICS") return false;
	return true;
}

// Tries each LP overlay empty-tag (ESTABLISH/, MOVE/, META_BELIEF/, …) once if that flag
// is still false. Returns true if one tag was consumed so the caller can loop.
bool absorbCommonVerbNetClasses(cVerbNet &vn, vector <cXMLClass> &tempxc, lpchar_t* buf, int64_t& offset)
{
	if (!vn.establish && (vn.establish = lineX(buf, offset, tempxc, u"ESTABLISH/"))) return true;
	if (!vn.noPhysicalAction && (vn.noPhysicalAction = lineX(buf, offset, tempxc, u"NO_PHYSICAL_ACTION/"))) return true;
	if (!vn.control && (vn.control = lineX(buf, offset, tempxc, u"CONTROL/"))) return true;
	if (!vn.contact && (vn.contact = lineX(buf, offset, tempxc, u"CONTACT/"))) return true;
	if (!vn._near && (vn._near = lineX(buf, offset, tempxc, u"NEAR/"))) return true;
	if (!vn.objectMustBeLocation && (vn.objectMustBeLocation = lineX(buf, offset, tempxc, u"LOCATIONOBJECT/"))) return true;
	if (!vn.prepMustBeLocation && (vn.prepMustBeLocation = lineX(buf, offset, tempxc, u"LOCATIONPREPOBJECT/"))) return true;
	if (!vn.transfer && (vn.transfer = lineX(buf, offset, tempxc, u"TRANSFER/"))) return true;
	if (!vn.noPrepTo && (vn.noPrepTo = lineX(buf, offset, tempxc, u"NO_PREP_TO/"))) return true;
	if (!vn.noPrepFrom && (vn.noPrepFrom = lineX(buf, offset, tempxc, u"NO_PREP_FROM/"))) return true;
	if (!vn.move && (vn.move = lineX(buf, offset, tempxc, u"MOVE/"))) return true;
	if (!vn.moveObject && (vn.moveObject = lineX(buf, offset, tempxc, u"MOVE_OBJECT/"))) return true;
	if (!vn.moveInPlace && (vn.moveInPlace = lineX(buf, offset, tempxc, u"MOVE_IN_PLACE/"))) return true;
	if (!vn.exit && (vn.exit = lineX(buf, offset, tempxc, u"EXIT/"))) return true;
	if (!vn.enter && (vn.enter = lineX(buf, offset, tempxc, u"ENTER/"))) return true;
	if (!vn.contiguous && (vn.contiguous = lineX(buf, offset, tempxc, u"CONTIGUOUS/"))) return true;
	if (!vn.start && (vn.start = lineX(buf, offset, tempxc, u"START/"))) return true;
	if (!vn.stay && (vn.stay = lineX(buf, offset, tempxc, u"STAY/"))) return true;
	if (!vn.has && (vn.has = lineX(buf, offset, tempxc, u"HAS/"))) return true;
	if (!vn.communicate && (vn.communicate = lineX(buf, offset, tempxc, u"COMMUNICATE/"))) return true;
	if (!vn.think && (vn.think = lineX(buf, offset, tempxc, u"THINK/"))) return true;
	if (!vn.thinkObject && (vn.thinkObject = lineX(buf, offset, tempxc, u"THINK_OBJECT/"))) return true;
	if (!vn.sense && (vn.sense = lineX(buf, offset, tempxc, u"SENSE/"))) return true;
	if (!vn.create && (vn.create = lineX(buf, offset, tempxc, u"CREATE/"))) return true;
	if (!vn.consume && (vn.consume = lineX(buf, offset, tempxc, u"CONSUME/"))) return true; // to take in and change state 
	// change of state is physical (it can be visibly seen)
	if (!vn.changeState && (vn.changeState = lineX(buf, offset, tempxc, u"CHANGE_STATE/"))) return true;
	// if no object, then the change of state is in the subject
	if (!vn.agentChangeObjectInternalState && (vn.agentChangeObjectInternalState = lineX(buf, offset, tempxc, u"AGENT_CHANGE_OBJECT_INTERNAL_STATE/"))) return true;
	if (!vn.metaProfession && (vn.metaProfession = lineX(buf, offset, tempxc, u"META_PROFESSION/"))) return true;
	if (!vn.metaFutureHave && (vn.metaFutureHave = lineX(buf, offset, tempxc, u"META_FUTURE_HAVE/"))) return true;
	if (!vn.metaFutureContact && (vn.metaFutureContact = lineX(buf, offset, tempxc, u"META_FUTURE_CONTACT/"))) return true;
	if (!vn.metaInfo && (vn.metaInfo = lineX(buf, offset, tempxc, u"META_INFO/"))) return true;
	if (!vn.metaIfThen && (vn.metaIfThen = lineX(buf, offset, tempxc, u"META_IF_THEN/"))) return true;
	if (!vn.metaContains && (vn.metaContains = lineX(buf, offset, tempxc, u"META_CONTAINS/"))) return true;
	if (!vn.metaDesire && (vn.metaDesire = lineX(buf, offset, tempxc, u"META_DESIRE/"))) return true;
	if (!vn.metaRole && (vn.metaRole = lineX(buf, offset, tempxc, u"META_ROLE/"))) return true;
	/*
	transferring belief or reveal of internal belief or attempt to change another's belief
	000016:acquiesce-95        subject aligns belief with another
	000016:advise-37.9         subject tells something to someone
	000018:appeal-31.4-3       subject suggests something to object
	000025:confront-98         subject asserts something to someone
	000028:cooperate-73-3      subject aligns belief with another
	000034:deduce-97.2         subject knows something
	000007:interrogate-37.1.3  subject desires to know something
	*/
	if (!vn.metaBelief && (vn.metaBelief = lineX(buf, offset, tempxc, u"META_BELIEF/"))) return true;
	if (!vn.spatialOrientation && (vn.spatialOrientation = lineX(buf, offset, tempxc, u"SPATIAL_ORIENTATION/"))) return true;
	if (!vn.ignore && (vn.ignore = lineX(buf, offset, tempxc, u"IGNORE/"))) return true;
	return false;
}

// Parses one VNCLASS or VNSUBCLASS from offset: optional xml/DOCTYPE, ID, MEMBERS
// (each MEMBER lemma is mapped in vbNetVerbToClassMap to the upcoming vbNetClasses.size()),
// overlay flags, THEMROLES, FRAMES, then nested SUBCLASSES via recursion. Pushes vn onto
// vbNetClasses. Returns false if any required closer is missing.
bool aVNCLASS(lpchar_t* buf, int64_t& offset)
{
	LFS
		cVerbNet vn;
	vector <cXMLClass> tempxc;
	// <?xml version="1.0" encoding="UTF-8"?>
	lineX(buf, offset, tempxc, u"?xml");
	// <!DOCTYPE VNCLASS SYSTEM "vn_class-3.dtd">
	lineX(buf, offset, tempxc, u"!DOCTYPE");
	if (!lineX(buf, offset, vn.id, u"VNCLASS", false) && !lineX(buf, offset, vn.id, u"VNSUBCLASS", false)) return false;
	lpwstring tmp;
	//  <MEMBERS>
	//      <MEMBER name="disclose" wn=""/>
	//  </MEMBERS>
	if (!lineX(buf, offset, tempxc, u"MEMBERS/"))
	{
		if (!lineX(buf, offset, tempxc, u"MEMBERS")) return false;
		while (lineX(buf, offset, vn.members, u"MEMBER"))
		{
			vbNetVerbToClassMap[vn.members[vn.members.size() - 1].av[0].as].insert(vbNetClasses.size());
			//lplog(LOG_TIME,u"mapped %s to %d.",vn.members[vn.members.size()-1].av[0].as.c_str(),vbNetClasses.size());
			numMembers++;
		}
		if (endX(buf, offset, tmp) != u"MEMBERS") return false;
	}
	while (absorbCommonVerbNetClasses(vn, tempxc, buf, offset));
	vn.prepLocation = false;
	// <THEMROLES>
	//   <THEMROLE type="Agent">
	//     <SELRESTRS logic="or">
	//       <SELRESTR Value="+" type="animate"/>
	//     </SELRESTRS>
	//   </THEMROLE>
	// </THEMROLES>
	if (!lineX(buf, offset, tempxc, u"THEMROLES/"))
	{
		if (!lineX(buf, offset, tempxc, u"THEMROLES")) return false;
		while (lineX(buf, offset, vn.themroles, u"THEMROLE"))
		{
			aVNSEL(buf, offset, vn.themroles);
			if (endX(buf, offset, tmp) != u"THEMROLE") return false;
		}
		if (endX(buf, offset, tmp) != u"THEMROLES") return false;
	}
	//  <FRAMES>
	//    <FRAME>
	//        <DESCRIPTION descriptionNumber="0.2" primary="Basic Transitive" secondary="Topic Object" xtag="0.2"/>
	//        <EXAMPLES>
	//            <EXAMPLE>Ellen said a few words.</EXAMPLE>
	//        </EXAMPLES>
	//    </FRAME>
	if (!lineX(buf, offset, tempxc, u"FRAMES/"))
	{
		cXMLFrame frame;
		if (!lineX(buf, offset, tempxc, u"FRAMES")) return false;
		while (lineX(buf, offset, frame.description, u"FRAME"))
		{
			lineX(buf, offset, frame.description, u"DESCRIPTION");
			if (lineX(buf, offset, tempxc, u"EXAMPLES"))
			{
				lpwstring example;
				tmp = u"EXAMPLE";
				while (lineX(buf, offset, frame.examples, u"EXAMPLE"))
				{
					if (!tX(buf, offset, example, u'<')) return false;
					frame.examples[frame.examples.size() - 1].av.push_back(cXMLAttribute(tmp, example));
					offset--;
					if (endX(buf, offset, tmp) != u"EXAMPLE") return false;
				}
				if (endX(buf, offset, tmp) != u"EXAMPLES") return false;
			}
			aVNSyntax(buf, offset, frame.syntax);
			aVNSemantics(buf, offset, frame.semantics);
			vn.frames.push_back(frame);
			if (endX(buf, offset, tmp) != u"FRAME") return false;
		}
		if (endX(buf, offset, tmp) != u"FRAMES") return false;
	}
	//lplog(LOG_WCHECK,u"%s:%s",vn.id[0].av[0].as.c_str(),(vn.noPhysicalAction) ? u"true":u"false");
	vbNetClasses.push_back(vn);
	if (lineX(buf, offset, tempxc, u"SUBCLASSES", true))
	{
		while (aVNCLASS(buf, offset));
		if (endX(buf, offset, tmp) != u"SUBCLASSES") return false;
	}
	return endX(buf, offset, tmp) == u"VNCLASS" || tmp == u"VNSUBCLASS";
}

vector < cVerbNet > vbNetClasses;

// Loads every *.xml under source\lists\VerbNet\ via aVNCLASS, then appends a synthetic
// am/become/be class (prepMustBeLocation, noPrepTo). FindClose(hFind) runs on every
// return path, including when a file fails to open partway through the directory scan.
void readVBNet(void)
{
	LFS
		// Batch B10: lpDirectoryEntries replaces FindFirstFile/FindNextFile/FindClose.
		// The directory separator is '/' now; the path itself is otherwise unchanged.
		const lpwstring verbNetDirectory = u"source/lists/VerbNet";
	std::vector<lpwstring> verbNetFiles = lpDirectoryEntries(verbNetDirectory, u"*.xml");
	if (verbNetFiles.empty())
	{
		lp_wprintf(u"No VerbNet .xml files found in directory %s\r", verbNetDirectory.c_str());
		return;
	}
	vbNetClasses.reserve(550);
	for (const lpwstring& verbNetFile : verbNetFiles)
	{
		if (verbNetFile.empty() || verbNetFile[0] == '.') continue;
		lpchar_t original[4096];
		lp_snprintf(original, 4096, u"%s/%s", verbNetDirectory.c_str(), verbNetFile.c_str());
		int fd = lp_wopen(original, O_RDONLY | O_BINARY);
		if (fd < 0)
			return;
		int bufferlen = lp_filelength(fd);
		char* buffer = (char*)tmalloc(bufferlen + 10);
		::read(fd, buffer, bufferlen);
		close(fd);
		buffer[bufferlen] = 0;
		lpwstring wide;
		mTW(buffer, wide);
		tfree(bufferlen + 10, buffer);
		int64_t offset = 0;
		if (!aVNCLASS((lpchar_t*)wide.c_str(), offset))
			lp_wprintf(u"Error reading %s at offset %I64d:%lS..->\n%lS...\n",
				verbNetFile.c_str(), offset, wide.substr((int)(offset - min(offset, (int64_t)64)), (int)(min(offset, (int64_t)64))).c_str(), wide.substr((int)offset, 64).c_str());
	}
	vbNetVerbToClassMap[u"am"].insert(vbNetClasses.size());
	vbNetVerbToClassMap[u"become"].insert(vbNetClasses.size());
	// who would be afraid to meet death 
	// he would be at the courthouse
	vbNetVerbToClassMap[u"be"].insert(vbNetClasses.size());
	lpwstring a, as = u"am";
	cXMLClass id;
	id.av.push_back(cXMLAttribute(a, as));
	cVerbNet vn;
	vn.prepLocation = true;
	vn.prepMustBeLocation = true;
	vn.noPrepTo = true;
	vn.am = true;
	vn.id.push_back(id);
	vbNetClasses.push_back(vn);
}

