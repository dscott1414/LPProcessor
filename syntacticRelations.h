/*
	syntacticRelations.h - word-pair relation groups and complementary-relation
	                       lookup used by the (currently unused) clustering path.

	Overview:
		Declares cWordGroup (a many-from-words / many-to-words cluster for one
		relationWOType) and cRelationCombo (a pair of cRMap iterators tagged
		with a relationComboTypes value such as SVO / VPO).  getComplementary-
		Relationship() flips SubjectWordWithVerb <-> VerbWithSubjectWord etc.
		The live implementations of addRelation / getRelStr live in
		syntacticRelations.cpp; the group-accumulation bodies are behind
		#ifdef ACCUMULATE_GROUPS and are not linked in the default build.

	Pipeline position:
		Supporting types for Stage 5 word-relation maps.  relationCombos and
		groups[] are declared but unused unless ACCUMULATE_GROUPS is on.

	Key data structures / globals:
		- cWordGroup::fromWords / toWords - cluster members; otherFlag must
		  be false outside intersect()
		- cWordGroup::index - DB row id, -1 if not yet written
		- relationCombos - planned combo table; not populated
		- MAX_TAGSETS 500 - cap referenced by tag-set collectors elsewhere

	Notes / gotchas:
		- Header incorporateMapping takes wstring, but the ACCUMULATE_GROUPS
		  body in the .cpp takes tIWMM; enabling that ifdef will not compile
		  against this header without a signature change.
		- Only the default cWordGroup() ctor sets index; the other ctors
		  leave index and otherFlag uninitialized.
*/
#pragma once
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
	// Add word to toWords if every fromWord already maps to it; else fill subGroup
	// with the fromWords that do.  Returns true iff the whole group now contains word.
	bool incorporateMapping(relationWOTypes relationType, wstring word,vector <wstring> &subGroup);
	cWordGroup(vector <wstring> &subGroup,set <wstring> &toWords, wstring word);
  cWordGroup(wstring fromWord1, wstring fromWord2, wstring toWord1, wstring toWord2);
	cWordGroup(wstring self,cSourceWordInfo::cRMap::tcRMap *toWords);
	cWordGroup(void);
	// "from1 from2 -> to1 to2" debug line; used only under ACCUMULATE_GROUPS.
	wstring summary(void);
};

class cRelationCombo 
{
public:
	relationComboTypes rcType;
	cSourceWordInfo::cRMap::tIcRMap rel1;
	cSourceWordInfo::cRMap::tIcRMap rel2;
	// Bundle two complementary cRMap iterators (e.g. VerbWithDirectWord +
	// DirectWordWithVerb) under an SVO/VPO/... combo type.  Unused at runtime.
	cRelationCombo(relationComboTypes inrcType,cSourceWordInfo::cRMap::tIcRMap inrel1,cSourceWordInfo::cRMap::tIcRMap inrel2)
	{
		rcType=inrcType;
		rel1=inrel1;
		rel2=inrel2;
	}
};

extern vector <cRelationCombo> relationCombos;
// Flip even/odd relationWOTypes pairs: SubjectWordWithVerb <-> VerbWithSubjectWord.
// Assumes the enum is laid out as adjacent complements; extended Next-verb types
// reuse the same even/odd rule.
relationWOTypes getComplementaryRelationship(int rType);
