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
  vector <wstring> fromWords;
	set <wstring> toWords;
	bool incorporateMapping(relationWOTypes relationType, wstring word,vector <wstring> &subGroup);
	cWordGroup(vector <wstring> &subGroup,set <wstring> &toWords, wstring word);
  cWordGroup(wstring fromWord1, wstring fromWord2, wstring toWord1, wstring toWord2);
	cWordGroup(wstring self,cSourceWordInfo::cRMap::tcRMap *toWords);
	cWordGroup(void);
	wstring summary(void);
};

class cRelationCombo 
{
public:
	relationComboTypes rcType;
	cSourceWordInfo::cRMap::tIcRMap rel1;
	cSourceWordInfo::cRMap::tIcRMap rel2;
	cRelationCombo(relationComboTypes inrcType,cSourceWordInfo::cRMap::tIcRMap inrel1,cSourceWordInfo::cRMap::tIcRMap inrel2)
	{
		rcType=inrcType;
		rel1=inrel1;
		rel2=inrel2;
	}
};

extern vector <cRelationCombo> relationCombos;
relationWOTypes getComplementaryRelationship(int rType);
