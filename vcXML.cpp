#pragma warning(disable : 4786 ) // disable warning C4786
#include <windows.h>
#include <io.h>
#include "word.h"
#include "source.h"
#include <stdlib.h>
#include "wn.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "vcXML.h"
#include "profile.h"

int numMembers=0;
unordered_map <wstring,set <int> > vbNetVerbToClassMap;

bool tX(wchar_t *buf,__int64 &offset,wstring &s,wchar_t endChar)
{ LFS
	wchar_t *ch=wcschr(buf+offset,endChar);
	if (ch==NULL) return false;
	wchar_t savech=*ch;
	*ch=0;
	s=buf+offset;
	*ch=savech;
	offset=(ch-buf+1);
	return true;
}

// <VNCLASS 
bool aH(wchar_t *buf,__int64 &offset,wstring &s,wchar_t endChar)
{ LFS
	wchar_t *ch=wcschr(buf+offset,endChar);
	wchar_t *ech=wcschr(buf+offset,L'>');
	if (ch==NULL && ech==NULL) return false;
	if ((ch==NULL && ech!=NULL) || ech<ch)
		ch=ech;
	wchar_t savech=*ch;
	*ch=0;
	s=buf+offset;
	*ch=savech;
	offset=(ch-buf);
	if (*ch==endChar) offset++;
	return true;
}

// ID="say-37.7"
bool tA(wchar_t *buf,__int64 &offset,vector <cXMLAttribute> &attr)
{ LFS
	if ((buf[offset]==L'/' && buf[offset+1]==L'>') || (buf[offset]==L'?' && buf[offset+1]==L'>') || buf[offset]==L'>')
		return false;
	wstring a,as;
	while (iswspace(buf[offset])) offset++;
	if (!tX(buf,offset,a,L'=')) return false;
	while (iswspace(buf[offset])) offset++;
	offset++;
	if (!tX(buf,offset,as,L'\"')) return false;
	if (buf[offset]==L' ') offset++;
	attr.push_back(cXMLAttribute(a,as));
	return true;
}

// <VNCLASS ID="say-37.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="vn_schema-3.xsd">
bool lineX(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc,wstring expectedClass,bool absorbNonExpectedClassAnyway=false)
{ LFS
	while (iswspace(buf[offset])) offset++;
	if (buf[offset]!=L'<') return false;
	if (buf[offset+1]==L'/') return false; // end of some structure
	if (buf[offset+1]==L'!')
	{
		wstring endOfComment;
		while (buf[offset+2]==L'-') 
		{
			endOfComment+=buf[offset+2];
			offset++;
		}
		endOfComment+=L">";
		wchar_t *ch=wcsstr(buf+offset,endOfComment.c_str());
		if (ch==NULL) return false;
		offset=ch-buf+endOfComment.length()+1;
		return lineX(buf,offset,vxc,expectedClass,absorbNonExpectedClassAnyway);
	}
	cXMLClass xc;
	__int64 classOffset=offset+1;
	if (!aH(buf,classOffset,xc.XClass,' ')) return false;
	if (xc.XClass!=expectedClass && !absorbNonExpectedClassAnyway)
		return false;
	offset=classOffset;
	if (expectedClass[0]==L'?')
		return tX(buf,offset,expectedClass,L'>');
	else
	{
		while (tA(buf,offset,xc.av));
		if (!(buf[offset]==L'/' && buf[offset+1]==L'>') && !(buf[offset]==L'?' && buf[offset+1]==L'>') && buf[offset]!=L'>')
			return false;
		if (buf[offset]==L'>') 
			offset++;
		else
			offset+=2;
		if (xc.XClass==expectedClass)
			vxc.push_back(xc);
		return xc.XClass==expectedClass;
	}
}

//  </MEMBERS>
wstring endX(wchar_t *buf,__int64 &offset,wstring &tmp)
{ LFS
	while (iswspace(buf[offset])) offset++;
	if (buf[offset+1]==L'!')
	{
		tX(buf,offset,tmp,L'>');
		return endX(buf,offset,tmp);
	}
	if (buf[offset]!=L'<' || buf[offset+1]!=L'/') return L"";
	offset+=2;
	tX(buf,offset,tmp,L'>');
	return tmp;
}

bool aVNSEL(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc)
{ LFS
	int atLeastOne=false;
	while (lineX(buf,offset,vxc,L"SELRESTRS"))
	{
		while (lineX(buf,offset,vxc[vxc.size()-1].vxc,L"SELRESTR") || aVNSEL(buf,offset,vxc));
		atLeastOne=true;
	}
	wstring tmp;
	return lineX(buf,offset,vxc,L"SELRESTRS/") || (atLeastOne && endX(buf,offset,tmp)==L"SELRESTRS");
}

// <SYNRESTRS>
//   <SYNRESTR Value="+" type="quotation"/>
// </SYNRESTRS>
bool aVNSYN(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc)
{ LFS
	while (lineX(buf,offset,vxc,L"SYNRESTRS"))
		while (lineX(buf,offset,vxc[vxc.size()-1].vxc,L"SYNRESTR"));
	wstring tmp;
	return lineX(buf,offset,vxc,L"SYNRESTRS/") || endX(buf,offset,tmp)==L"SYNRESTRS";
}

bool aSUBCLASS(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc)
{ LFS
	while (lineX(buf,offset,vxc,L"SUBCLASSES"))
		while (lineX(buf,offset,vxc[vxc.size()-1].vxc,L"SUBCLASS"));
	wstring tmp;
	return lineX(buf,offset,vxc,L"SUBCLASSES/") || endX(buf,offset,tmp)==L"SUBCLASSES";
}

//      <NP value="Agent">
//        <SYNRESTRS>
//          <SYNRESTR Value="+" type="quotation"/>
//        </SYNRESTRS>
//      </NP>
bool aVNNP(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc)
{ LFS
	if (!lineX(buf,offset,vxc,L"NP")) return false;
	if (!aVNSYN(buf,offset,vxc) && !aVNSEL(buf,offset,vxc)) return false;
	wstring tmp;
	return endX(buf,offset,tmp)==L"NP";
}

bool aVNLEAF(wchar_t *buf,__int64 &offset,wchar_t *str)
{ LFS
	vector <cXMLClass> tempxc;
	__int64 advOffset=offset;
	if (lineX(buf,advOffset,tempxc,str))
	{
		offset=advOffset;
		return true;
	}
	return false;
}

// <PREP value="with">
//   <SELRESTRS/>
// </PREP>
bool aVNPREP(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc)
{ LFS
	if (!lineX(buf,offset,vxc,L"PREP")) return false;
	if (!aVNSEL(buf,offset,vxc)) return false;
	wstring tmp;
	return endX(buf,offset,tmp)==L"PREP";
}

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
bool aVNSyntax(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc)
{ LFS
	vector <cXMLClass> tempxc;
	if (!lineX(buf,offset,tempxc,L"SYNTAX")) return false;
	// NP, ADV, ADJ, PREP or LEX
	while (aVNNP(buf,offset,vxc) || aVNLEAF(buf,offset,L"ADV/") || aVNLEAF(buf,offset,L"ADJ/") || aVNPREP(buf,offset,vxc) || lineX(buf,offset,vxc,L"LEX"));
	// VERB
	if (!lineX(buf,offset,tempxc,L"VERB/")) return false;
	// NP, ADV, ADJ, PREP or LEX
	while (aVNNP(buf,offset,vxc) || aVNLEAF(buf,offset,L"ADV/") || aVNLEAF(buf,offset,L"ADJ/") || aVNPREP(buf,offset,vxc) || lineX(buf,offset,vxc,L"LEX"));
	wstring end;
	return endX(buf,offset,end)==L"SYNTAX";
}

//  <SEMANTICS>
//      <PRED value="transfer_info">
//          <ARGS>
//              <ARG type="Event" value="during(E)"/>
//          </ARGS>
//      </PRED>
//  </SEMANTICS>
bool aVNSemantics(wchar_t *buf,__int64 &offset,vector <cXMLClass> &vxc)
{ LFS
	vector <cXMLClass> tempxc;
	if (!lineX(buf,offset,tempxc,L"SEMANTICS")) return false;
	wstring tmp;
	while (lineX(buf,offset,vxc,L"PRED"))
	{
		while (lineX(buf,offset,tempxc,L"ARGS"))
		{
			while (lineX(buf,offset,vxc[vxc.size()-1].vxc,L"ARG"));
			if (endX(buf,offset,tmp)!=L"ARGS") return false;
		}
		if (endX(buf,offset,tmp)!=L"PRED") return false;
	}
	if (endX(buf,offset,tmp)!=L"SEMANTICS") return false;
	return true;
}

bool aVNCLASS(wchar_t *buf,__int64 &offset)
{ LFS
	cVerbNet vn;
	vector <cXMLClass> tempxc;
	// <?xml version="1.0" encoding="UTF-8"?>
	lineX(buf,offset,tempxc,L"?xml");
	// <!DOCTYPE VNCLASS SYSTEM "vn_class-3.dtd">
	lineX(buf,offset,tempxc,L"!DOCTYPE");
	if (!lineX(buf,offset,vn.id,L"VNCLASS",false) && !lineX(buf,offset,vn.id,L"VNSUBCLASS",false)) return false;
	wstring tmp;
	//  <MEMBERS>
  //      <MEMBER name="disclose" wn=""/>
  //  </MEMBERS>
	if (!lineX(buf,offset,tempxc,L"MEMBERS/")) 
	{
		if (!lineX(buf,offset,tempxc,L"MEMBERS")) return false;
		while (lineX(buf,offset,vn.members,L"MEMBER"))
		{
			vbNetVerbToClassMap[vn.members[vn.members.size()-1].av[0].as].insert(vbNetClasses.size());
			//lplog(LOG_TIME,L"mapped %s to %d.",vn.members[vn.members.size()-1].av[0].as.c_str(),vbNetClasses.size());
			numMembers++;
		}
		if (endX(buf,offset,tmp)!=L"MEMBERS") return false;
	}
	while (true)
	{
		if (!vn.establish && (vn.establish=lineX(buf,offset,tempxc,L"ESTABLISH/"))) continue;
		if (!vn.noPhysicalAction && (vn.noPhysicalAction=lineX(buf,offset,tempxc,L"NO_PHYSICAL_ACTION/"))) continue;
		if (!vn.control && (vn.control=lineX(buf,offset,tempxc,L"CONTROL/"))) continue;
		if (!vn.contact && (vn.contact=lineX(buf,offset,tempxc,L"CONTACT/"))) continue;
		if (!vn._near && (vn._near=lineX(buf,offset,tempxc,L"NEAR/"))) continue;
		if (!vn.objectMustBeLocation && (vn.objectMustBeLocation=lineX(buf,offset,tempxc,L"LOCATIONOBJECT/"))) continue;
		if (!vn.prepMustBeLocation && (vn.prepMustBeLocation=lineX(buf,offset,tempxc,L"LOCATIONPREPOBJECT/"))) continue;
		if (!vn.transfer && (vn.transfer=lineX(buf,offset,tempxc,L"TRANSFER/"))) continue;
		if (!vn.noPrepTo && (vn.noPrepTo=lineX(buf,offset,tempxc,L"NO_PREP_TO/"))) continue;
		if (!vn.noPrepFrom && (vn.noPrepFrom=lineX(buf,offset,tempxc,L"NO_PREP_FROM/"))) continue;
		if (!vn.move && (vn.move=lineX(buf,offset,tempxc,L"MOVE/"))) continue;
		if (!vn.moveObject && (vn.moveObject=lineX(buf,offset,tempxc,L"MOVE_OBJECT/"))) continue;
		if (!vn.moveInPlace && (vn.moveInPlace=lineX(buf,offset,tempxc,L"MOVE_IN_PLACE/"))) continue;
		if (!vn.exit && (vn.exit=lineX(buf,offset,tempxc,L"EXIT/"))) continue;
		if (!vn.enter && (vn.enter=lineX(buf,offset,tempxc,L"ENTER/"))) continue;
		if (!vn.contiguous && (vn.contiguous=lineX(buf,offset,tempxc,L"CONTIGUOUS/"))) continue;
		if (!vn.start && (vn.start=lineX(buf,offset,tempxc,L"START/"))) continue;
		if (!vn.stay && (vn.stay=lineX(buf,offset,tempxc,L"STAY/"))) continue;
		if (!vn.has && (vn.has=lineX(buf,offset,tempxc,L"HAS/"))) continue;
		if (!vn.communicate && (vn.communicate=lineX(buf,offset,tempxc,L"COMMUNICATE/"))) continue;
		if (!vn.think && (vn.think=lineX(buf,offset,tempxc,L"THINK/"))) continue;
		if (!vn.thinkObject && (vn.thinkObject=lineX(buf,offset,tempxc,L"THINK_OBJECT/"))) continue;
		if (!vn.sense && (vn.sense=lineX(buf,offset,tempxc,L"SENSE/"))) continue;
		if (!vn.create && (vn.create=lineX(buf,offset,tempxc,L"CREATE/"))) continue;
		if (!vn.consume && (vn.consume=lineX(buf,offset,tempxc,L"CONSUME/"))) continue; // to take in and change state 
		// change of state is physical (it can be visibly seen)
		if (!vn.changeState && (vn.changeState=lineX(buf,offset,tempxc,L"CHANGE_STATE/"))) continue;
		// if no object, then the change of state is in the subject
		if (!vn.agentChangeObjectInternalState && (vn.agentChangeObjectInternalState=lineX(buf,offset,tempxc,L"AGENT_CHANGE_OBJECT_INTERNAL_STATE/"))) continue;
		if (!vn.metaProfession && (vn.metaProfession=lineX(buf,offset,tempxc,L"META_PROFESSION/"))) continue;
		if (!vn.metaFutureHave && (vn.metaFutureHave=lineX(buf,offset,tempxc,L"META_FUTURE_HAVE/"))) continue;
		if (!vn.metaFutureContact && (vn.metaFutureContact=lineX(buf,offset,tempxc,L"META_FUTURE_CONTACT/"))) continue;
		if (!vn.metaInfo && (vn.metaInfo=lineX(buf,offset,tempxc,L"META_INFO/"))) continue;
		if (!vn.metaIfThen && (vn.metaIfThen=lineX(buf,offset,tempxc,L"META_IF_THEN/"))) continue;
		if (!vn.metaContains && (vn.metaContains=lineX(buf,offset,tempxc,L"META_CONTAINS/"))) continue;
		if (!vn.metaDesire && (vn.metaDesire=lineX(buf,offset,tempxc,L"META_DESIRE/"))) continue;
		if (!vn.metaRole && (vn.metaRole=lineX(buf,offset,tempxc,L"META_ROLE/"))) continue;
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
		if (!vn.metaBelief && (vn.metaBelief=lineX(buf,offset,tempxc,L"META_BELIEF/"))) continue;
		if (!vn.spatialOrientation && (vn.spatialOrientation=lineX(buf,offset,tempxc,L"SPATIAL_ORIENTATION/"))) continue;
		if (!vn.ignore && (vn.ignore=lineX(buf,offset,tempxc,L"IGNORE/"))) continue;
		break;
	}
	vn.prepLocation=false;
  // <THEMROLES>
  //   <THEMROLE type="Agent">
  //     <SELRESTRS logic="or">
  //       <SELRESTR Value="+" type="animate"/>
  //     </SELRESTRS>
  //   </THEMROLE>
	// </THEMROLES>
	if (!lineX(buf,offset,tempxc,L"THEMROLES/")) 
	{
		if (!lineX(buf,offset,tempxc,L"THEMROLES")) return false;
		while (lineX(buf,offset,vn.themroles,L"THEMROLE"))
		{
			aVNSEL(buf,offset,vn.themroles);
			if (endX(buf,offset,tmp)!=L"THEMROLE") return false;
		}
		if (endX(buf,offset,tmp)!=L"THEMROLES") return false;
	}
  //  <FRAMES>
  //    <FRAME>
  //        <DESCRIPTION descriptionNumber="0.2" primary="Basic Transitive" secondary="Topic Object" xtag="0.2"/>
  //        <EXAMPLES>
  //            <EXAMPLE>Ellen said a few words.</EXAMPLE>
  //        </EXAMPLES>
  //    </FRAME>
	if (!lineX(buf,offset,tempxc,L"FRAMES/")) 
	{
		cXMLFrame frame;
		if (!lineX(buf,offset,tempxc,L"FRAMES")) return false;
		while (lineX(buf,offset,frame.description,L"FRAME"))
		{
			lineX(buf,offset,frame.description,L"DESCRIPTION");
			if (lineX(buf,offset,tempxc,L"EXAMPLES"))
			{
				wstring example;
				tmp=L"EXAMPLE";
				while (lineX(buf,offset,frame.examples,L"EXAMPLE"))
				{
					if (!tX(buf,offset,example,L'<')) return false;
					frame.examples[frame.examples.size()-1].av.push_back(cXMLAttribute(tmp,example));
					offset--;
					if (endX(buf,offset,tmp)!=L"EXAMPLE") return false;
				}
				if (endX(buf,offset,tmp)!=L"EXAMPLES") return false;
			}
			aVNSyntax(buf,offset,frame.syntax);
			aVNSemantics(buf,offset,frame.semantics);
			vn.frames.push_back(frame);
			if (endX(buf,offset,tmp)!=L"FRAME") return false;
		}
		if (endX(buf,offset,tmp)!=L"FRAMES") return false;
	}
	//lplog(LOG_WCHECK,L"%s:%s",vn.id[0].av[0].as.c_str(),(vn.noPhysicalAction) ? L"true":L"false");
	vbNetClasses.push_back(vn);
	if (lineX(buf,offset,tempxc,L"SUBCLASSES",true))
	{
	  while (aVNCLASS(buf,offset));
		if (endX(buf,offset,tmp)!=L"SUBCLASSES") return false;
	}
	return endX(buf,offset,tmp)==L"VNCLASS" || tmp==L"VNSUBCLASS";
}

vector < cVerbNet > vbNetClasses;

void readVBNet(void)
{ LFS
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	if ((hFind = FindFirstFile(L"source\\lists\\VerbNet\\*.xml", &FindFileData)) == INVALID_HANDLE_VALUE) 
	{
		wprintf (L"FindFirstFile failed on directory %s (%d)\r", L"source\\lists\\VBNet\\",(int)GetLastError());
		return;
	}
	vbNetClasses.reserve(550);
	do
	{
		if (FindFileData.cFileName[0]=='.') continue;
		wchar_t original[4096];
		_snwprintf(original,4096,L"source\\lists\\VerbNet\\%s",FindFileData.cFileName);
		int fd=_wopen(original,O_RDONLY|O_BINARY);
		if (fd<0) 
			return;
		int bufferlen=filelength(fd);
		char *buffer=(char *)tmalloc(bufferlen+10);
		::read(fd,buffer,bufferlen);
		close(fd);
		buffer[bufferlen]=0;
		wstring wide;
		mTW(buffer,wide);
		free(buffer);
		__int64 offset=0;
		if (!aVNCLASS((wchar_t *)wide.c_str(),offset))
			wprintf(L"Error reading %s at offset %I64d:%lS..->\n%lS...\n",
				FindFileData.cFileName,offset,wide.substr((int)(offset-min(offset,64)),(int)(min(offset,64))).c_str(),wide.substr((int)offset,64).c_str());
	} while (FindNextFile(hFind, &FindFileData) != 0);
	FindClose(hFind);
	vbNetVerbToClassMap[L"am"].insert(vbNetClasses.size());
	vbNetVerbToClassMap[L"become"].insert(vbNetClasses.size());
	// who would be afraid to meet death 
	// he would be at the courthouse
	vbNetVerbToClassMap[L"be"].insert(vbNetClasses.size());
	wstring a,as=L"am";
	cXMLClass id;
	id.av.push_back(cXMLAttribute(a,as));
	cVerbNet vn;
	vn.prepLocation=true;
	vn.prepMustBeLocation=true;
	vn.noPrepTo=true;
	vn.am=true;
	vn.id.push_back(id);
	vbNetClasses.push_back(vn);
}

