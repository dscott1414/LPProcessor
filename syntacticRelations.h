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
		- fromWords/toWords and the ctors below were retyped from lpwstring to
		  tIWMM (word-map iterators) to match how summary() / incorporateMapping()
		  / cSourceWordInfo::addRelation() actually use them under
		  ACCUMULATE_GROUPS (they call ->second on group members). Even with
		  that fixed, cSourceWordInfo::addRelation()'s
		  `cWordGroup(this, fromWord, word, toWord)` call still does not match
		  any constructor here (`this` is a cSourceWordInfo*, not a tIWMM) --
		  ACCUMULATE_GROUPS is an unfinished feature (see the TODO block above
		  intersect() in the .cpp) and was never made to compile end-to-end;
		  that call needs a real constructor design from whoever finishes it.
		- Every ctor now initializes index and otherFlag.
*/
#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
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
	set <tIWMM, cSourceWordInfo::wordSetCompare> toWords;
	// Add word to toWords if every fromWord already maps to it; else fill subGroup
	// with the fromWords that do.  Returns true iff the whole group now contains word.
	bool incorporateMapping(relationWOTypes relationType, tIWMM word,vector <tIWMM> &subGroup);
	cWordGroup(vector <tIWMM> &subGroup,set <tIWMM, cSourceWordInfo::wordSetCompare> &toWords, tIWMM word);
  cWordGroup(tIWMM fromWord1, tIWMM fromWord2, tIWMM toWord1, tIWMM toWord2);
	cWordGroup(tIWMM self,cSourceWordInfo::cRMap::tcRMap *toWords);
	cWordGroup(void);
	// "from1 from2 -> to1 to2" debug line; used only under ACCUMULATE_GROUPS.
	lpwstring summary(void);
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
