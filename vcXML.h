class cXMLAttribute 
{
public:
	wstring a;
	wstring as;
	cXMLAttribute(wstring &ina,wstring &inas)
	{
		a=ina;
		as=inas;
	}
};

class cXMLClass {
public:
	wstring XClass;
	vector <cXMLAttribute> av;
	vector <cXMLClass> vxc;
};

class cXMLFrame
{
public:
	vector <cXMLClass> description;
	vector <cXMLClass> examples;
	vector <cXMLClass> syntax;
	vector <cXMLClass> semantics;
};

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
	unordered_map <wstring,int> frequencyByMember;
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
	bool whereVerbClass(void)
	{
		return move || moveInPlace || moveObject || exit || enter || contiguous || start || stay || transfer || contact || _near || am;
	}
	bool incorporatedVerbClass(void)
	{
		return move || moveInPlace || moveObject || exit || enter || contiguous || start || stay || has || establish ||
			     transfer || contact || _near || think || thinkObject || communicate || control || changeState || agentChangeObjectInternalState || sense || 
					 create || consume || 
					 metaProfession ||	metaFutureHave ||	metaFutureContact || metaInfo ||	metaIfThen ||	
					 metaContains || metaDesire || metaBelief || metaRole || spatialOrientation || ignore || am;
	}
	wstring incorporatedVerbClassString(wstring &tmpstr)
	{
		if (establish) tmpstr+=L"establish "; 
		if (move) tmpstr+=L"move ";
		if (moveInPlace) tmpstr+=L"moveInPlace ";
		if (moveObject) tmpstr+=L"moveObject "; 
		if (exit) tmpstr+=L"exit "; 
		if (enter) tmpstr+=L"enter ";
		if (contiguous) tmpstr+=L"contiguous "; 
		if (start) tmpstr+=L"start "; 
		if (stay) tmpstr+=L"stay "; 
		if (has) tmpstr+=L"has "; 
		if (transfer) tmpstr+=L"transfer "; 
		if (contact) tmpstr+=L"contact "; 
		if (_near) tmpstr+=L"_near "; 
		if (think) tmpstr+=L"think "; 
		if (thinkObject) tmpstr+=L"thinkObject "; 
		if (communicate) tmpstr+=L"communicate "; 
		if (control) tmpstr+=L"control "; 
		if (changeState) tmpstr+=L"changeState "; 
		if (agentChangeObjectInternalState) tmpstr+=L"agentChangeObjectInternalState "; 
		if (sense) tmpstr+=L"sense "; 
		if (create) tmpstr+=L"create "; 
		if (consume) tmpstr+=L"consume "; 
		if (metaProfession) tmpstr+=L"metaProfession "; 
		if (metaFutureHave) tmpstr+=L"metaFutureHave "; 
		if (metaFutureContact) tmpstr+=L"metaFutureContact "; 
		if (metaInfo) tmpstr+=L"metaInfo "; 
		if (metaIfThen) tmpstr+=L"metaIfThen "; 
		if (metaContains) tmpstr+=L"metaContains "; 
		if (metaDesire) tmpstr+=L"metaDesire "; 
		if (metaBelief) tmpstr+=L"metaBelief "; 
		if (metaRole) tmpstr+=L"metaRole "; 
		if (spatialOrientation) tmpstr+=L"spatialOrientation "; 
		if (ignore) tmpstr+=L"ignore "; 
		if (am) tmpstr+=L"am ";
		return tmpstr;
	}

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

	bool moveVerbClass(void)
	{
		return move || moveObject || exit || enter || transfer;
	}
	wstring name(void)
	{
		return id[0].av[0].as;
	}
};

extern unordered_map <wstring,set <int> > vbNetVerbToClassMap;
extern vector < cVerbNet > vbNetClasses;
