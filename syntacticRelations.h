#define MAX_TAGSETS 500

// each of the 'from Words' are related to ALL of the toWords by a relation in relationWOType.
// subgroups have more toWords than supergroups.  supergroups do not include the words in their subgroups.
class cWordGroup
{
public:
  int index; // -1 if not represented in DB yet
  bool otherFlag; // used with intersect - must always be set to false outside this routine
  bool addedFromWords,addedToWords,addedSubGroups;
  vector <int> subGroups;
  vector <tIWMM> fromWords;
	set <tIWMM,tFI::wordSetCompare> toWords;
	bool incorporateMapping(relationWOTypes relationType,tIWMM word,vector <tIWMM> &subGroup);
	cWordGroup(vector <tIWMM> &subGroup,set <tIWMM,tFI::wordSetCompare> &toWords,tIWMM word);
  cWordGroup(tIWMM fromWord1,tIWMM fromWord2,tIWMM toWord1,tIWMM toWord2);
	cWordGroup(tIWMM self,tFI::cRMap::tcRMap *toWords);
	cWordGroup(void);
	wstring summary(void);
};

class relc 
{
public:
	relationComboTypes rcType;
	tFI::cRMap::tIcRMap rel1;
	tFI::cRMap::tIcRMap rel2;
	relc(relationComboTypes inrcType,tFI::cRMap::tIcRMap inrel1,tFI::cRMap::tIcRMap inrel2)
	{
		rcType=inrcType;
		rel1=inrel1;
		rel2=inrel2;
	}
};

extern vector <relc> relationCombos;
relationWOTypes getComplementaryRelationship(int rType);
