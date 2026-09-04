#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
/*
	vcXML.h - VerbNet XML tree model and per-class semantic flags

	Overview:
		Holds the in-memory VerbNet class after vcXML.cpp's hand-rolled XML walk:
		nested cXMLClass trees (ID / members / thematic roles / frames) plus a large
		set of boolean "incorporated" semantic flags (move, think, metaBelief, …)
		that map onto semanticRelations.h st* codes.

	Pipeline position:
		Loaded once at initialization (readVBNet). Later stages look up a verb in
		vbNetVerbToClassMap and read flags / getRelationType() to assign semantic
		relations and location/motion constraints.

	Key data structures / globals:
		- cXMLAttribute - name/value pair from an XML attribute (a / as)
		- cXMLClass - one element: XClass name, attributes, nested children
		- cXMLFrame - one VerbNet FRAME (description, examples, syntax, semantics)
		- cVerbNet - one VNCLASS/VNSUBCLASS plus semantic flags and member frequencies
		- vbNetVerbToClassMap - lemma -> set of indexes into vbNetClasses
		- vbNetClasses - all loaded classes; index is what the map stores

	Notes / gotchas:
		name() indexes id[0].av[0] with no empty check. getRelationType() returns the
		first flag that is set (priority = declaration order), or -1 if none.
		_near is named that way because 'near' is a predefined token.
*/
#include "semanticRelations.h"

// One XML attribute as wide strings: a is the name, as is the quoted value.
class cXMLAttribute 
{
public:
	lpwstring a;
	lpwstring as;
	cXMLAttribute(lpwstring &ina,lpwstring &inas)
	{
		a=ina;
		as=inas;
	}
};

// One XML element: tag name XClass, attributes av, nested elements vxc.
class cXMLClass {
public:
	lpwstring XClass;
	vector <cXMLAttribute> av;
	vector <cXMLClass> vxc;
};

// One VerbNet <FRAME>: description/examples plus the SYNTAX and SEMANTICS subtrees.
class cXMLFrame
{
public:
	vector <cXMLClass> description;
	vector <cXMLClass> examples;
	vector <cXMLClass> syntax;
	vector <cXMLClass> semantics;
};

// One VerbNet class: XML pieces plus the boolean semantic tags LP overlays on the class.
class cVerbNet {
public:
	vector <cXMLClass> id;
	vector <cXMLClass> members;
	vector <cXMLClass> themroles;
	vector <cXMLFrame> frames;
	bool establish;
	bool noPhysicalAction;
	bool control;
	bool contact;
	bool _near; // near is a predefine
	bool transfer;
	bool prepLocation;
	bool noPrepTo;
	bool noPrepFrom;
	bool objectMustBeLocation; // if combined with prepMustBeLocation, then either object OR prep must be location
	bool move;
	bool moveObject;
	bool moveInPlace;
	bool exit;
	bool enter;
	bool contiguous;
	bool start;
	bool stay;
	bool has;
	bool prepMustBeLocation;
	bool think;
	bool thinkObject;
	bool communicate;
	bool changeState;
	bool agentChangeObjectInternalState;
	bool sense;
	bool create;
	bool consume;
	bool metaProfession;
	bool metaFutureHave;
	bool metaFutureContact;
	bool metaInfo;
	bool metaIfThen;
	bool metaContains;
	bool metaDesire;
	bool metaBelief;
	bool metaRole;
	bool spatialOrientation;
	bool ignore;
	bool am;
	int totalFrequency;
	unordered_map <lpwstring,int> frequencyByMember;
	cVerbNet()
	{
		establish=false;
		noPhysicalAction=false;
		control=false;
		prepLocation=false;
		objectMustBeLocation=false; 
		prepMustBeLocation=false;
		noPrepTo=false;
		noPrepFrom=false;
		transfer=false;
		contact=false;
		_near=false;
		move=false;
		moveObject=false;
		moveInPlace=false;
		exit=false;
		enter=false;
		contiguous=false;
		start=false;
		stay=false;
		has=false;
		think=false;
		thinkObject=false;
		communicate=false;
		consume=false;
		changeState=false;
		agentChangeObjectInternalState=false;
		sense=false;
		create=false;
		metaProfession=false;
		metaFutureHave=false;
		metaFutureContact=false;
		metaInfo=false;
		metaIfThen=false;
		metaContains=false;
		metaDesire=false;
		metaRole=false;
		metaBelief=false;
		spatialOrientation=false;
		ignore=false;
		am=false;
		totalFrequency=0;
	}
	// True if this class is a location/motion/"be" verb (used by where-resolution).
	bool whereVerbClass(void)
	{
		return move || moveInPlace || moveObject || exit || enter || contiguous || start || stay || transfer || contact || _near || am;
	}
	// True if any of the LP-overlaid semantic flags is set (class has been tagged).
	bool incorporatedVerbClass(void)
	{
		return move || moveInPlace || moveObject || exit || enter || contiguous || start || stay || has || establish ||
			     transfer || contact || _near || think || thinkObject || communicate || control || changeState || agentChangeObjectInternalState || sense || 
					 create || consume || 
					 metaProfession ||	metaFutureHave ||	metaFutureContact || metaInfo ||	metaIfThen ||	
					 metaContains || metaDesire || metaBelief || metaRole || spatialOrientation || ignore || am;
	}
	// Appends a space-separated list of set flag names onto tmpstr and returns it.
	lpwstring incorporatedVerbClassString(lpwstring &tmpstr)
	{
		if (establish) tmpstr+=u"establish "; 
		if (move) tmpstr+=u"move ";
		if (moveInPlace) tmpstr+=u"moveInPlace ";
		if (moveObject) tmpstr+=u"moveObject "; 
		if (exit) tmpstr+=u"exit "; 
		if (enter) tmpstr+=u"enter ";
		if (contiguous) tmpstr+=u"contiguous "; 
		if (start) tmpstr+=u"start "; 
		if (stay) tmpstr+=u"stay "; 
		if (has) tmpstr+=u"has "; 
		if (transfer) tmpstr+=u"transfer "; 
		if (contact) tmpstr+=u"contact "; 
		if (_near) tmpstr+=u"_near "; 
		if (think) tmpstr+=u"think "; 
		if (thinkObject) tmpstr+=u"thinkObject "; 
		if (communicate) tmpstr+=u"communicate "; 
		if (control) tmpstr+=u"control "; 
		if (changeState) tmpstr+=u"changeState "; 
		if (agentChangeObjectInternalState) tmpstr+=u"agentChangeObjectInternalState "; 
		if (sense) tmpstr+=u"sense "; 
		if (create) tmpstr+=u"create "; 
		if (consume) tmpstr+=u"consume "; 
		if (metaProfession) tmpstr+=u"metaProfession "; 
		if (metaFutureHave) tmpstr+=u"metaFutureHave "; 
		if (metaFutureContact) tmpstr+=u"metaFutureContact "; 
		if (metaInfo) tmpstr+=u"metaInfo "; 
		if (metaIfThen) tmpstr+=u"metaIfThen "; 
		if (metaContains) tmpstr+=u"metaContains "; 
		if (metaDesire) tmpstr+=u"metaDesire "; 
		if (metaBelief) tmpstr+=u"metaBelief "; 
		if (metaRole) tmpstr+=u"metaRole "; 
		if (spatialOrientation) tmpstr+=u"spatialOrientation "; 
		if (ignore) tmpstr+=u"ignore "; 
		if (am) tmpstr+=u"am ";
		return tmpstr;
	}

	// First set flag mapped to its st* semantic-relation code; -1 if none are set.
	int getRelationType(void)
	{
		if (establish) return stESTAB;
		if (move) return stMOVE;
		if (moveInPlace) return stMOVE_IN_PLACE;
		if (moveObject) return stMOVE_OBJECT;
		if (exit) return stEXIT;
		if (enter) return stENTER;
		if (contiguous) return stCONTIGUOUS; 
		if (start) return stSTART; 
		if (stay) return stSTAY; 
		if (has) return stHAVE; 
		if (transfer) return stTRANSFER; 
		if (contact) return stCONTACT; 
		if (_near) return stNEAR; 
		if (think) return stTHINK; 
		if (thinkObject) return stTHINKOBJECT; 
		if (communicate) return stCOMMUNICATE; 
		if (control) return stCONTROL; 
		if (changeState) return stCHANGE_STATE; 
		if (agentChangeObjectInternalState) return stAGENTCHANGEOBJECTINTERNALSTATE; 
		if (sense) return stSENSE; 
		if (create) return stCREATE; 
		if (consume) return stCONSUME; 
		if (metaProfession) return stMETAPROFESSION; 
		if (metaFutureHave) return stMETAFUTUREHAVE; 
		if (metaFutureContact) return stMETAFUTURECONTACT; 
		if (metaInfo) return stMETAINFO; 
		if (metaIfThen) return stMETAIFTHEN; 
		if (metaContains) return stMETACONTAINS; 
		if (metaDesire) return stMETADESIRE; 
		if (metaBelief) return stMETABELIEF; 
		if (metaRole) return stMETAROLE; 
		if (spatialOrientation) return stSPATIALORIENTATION; 
		if (ignore) return stIGNORE; 
		if (am) return stBE;
		return -1;
	}

	// True for motion that changes location of subject or object (excludes moveInPlace).
	bool moveVerbClass(void)
	{
		return move || moveObject || exit || enter || transfer;
	}
	// VerbNet class ID string (id[0].av[0].as); crashes if id/av is empty.
	lpwstring name(void)
	{
		return id[0].av[0].as;
	}
};

extern unordered_map <lpwstring,set <int> > vbNetVerbToClassMap;
extern vector < cVerbNet > vbNetClasses;
