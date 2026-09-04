/*
	relationTypes.h - Word-order syntactic relation type IDs used by the relation pipeline

	Overview:
		Declares the bidirectional word-order relation enum (SubjectWordWithVerb, VerbWithPrep,
		etc.) plus a smaller combo enum (SVO, VPO, …) that names common pairs of those
		relations. VERB_HISTORY spaces the "next N main verb" slots so nearby same-tense
		verbs can be linked without colliding with later relation IDs.

	Pipeline position:
		Consumed after parse, when syntacticRelations / syntacticRelationGroups assign
		roles. getRelStr() is implemented elsewhere and is used for logging/debug.

	Key data structures / globals:
		- relationWOTypes - one ID per directed word-to-word relation; NextRelation is the
		  first unused ID after VERB_HISTORY-expanded verb-adjacency slots
		- relationComboTypes - named pairs of WO types used when collapsing SVO/prep frames
		- relationWOTypeStrings[] - parallel display names (defined in another TU)

	Notes / gotchas:
		Adding a new WO type in the middle of the enum shifts every later ID, including
		the VerbWithNext1MainVerb family (computed as VerbWithNext1MainVerbSameSubject +
		VERB_HISTORY*2). Do not reorder without migrating stored relation tables.
*/
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
#define VERB_HISTORY 4 // number of verbs back to analyze for relations
// Maps a relationWOTypes value to its display string; unknown IDs are the caller's problem.
const lpchar_t *getRelStr(int relationType);
enum relationWOTypes { firstRelationType=0,
	SubjectWordWithVerb=firstRelationType,VerbWithSubjectWord,
	SubjectWordWithNotVerb,NotVerbWithSubjectWord,
  VerbWithDirectWord,DirectWordWithVerb,VerbWithIndirectWord,IndirectWordWithVerb,DirectWordWithIndirectWord,IndirectWordWithDirectWord,
  VerbWithInfinitive,InfinitiveWithVerb,WordWithInfinitive,InfinitiveWithWord, // when infinitive is attached to verb or object
  AVerbWithInfinitive,AInfinitiveWithVerb,AWordWithInfinitive,AInfinitiveWithWord, // when infinitive is attached to verb or object
  //IWordWithInfinitive,InfinitiveWithIWord, // the object of the infinitive - NOT USED
	VerbWithPrep,PrepWithVerb,WordWithPrep,PrepWithWord, // when preposition is attached to verb or object
	AVerbWithPrep,APrepWithVerb,AWordWithPrep,APrepWithWord, // AMBIGUOUS when preposition is attached to verb or object
  PWordWithPrep,PrepWithPWord, // the object of the preposition
	MasterVerbWithVerb,VerbWithMasterVerb,
	AdjectiveWithWord,WordWithAdjective, // may also be an identity relation He is fast
	AdjectiveIRNotWord,WordIRNotAdjective, // a negated identity relation He is NOT fast
	AdverbWithVerb,VerbWithAdverb,
	RelativeWithWord,WordWithRelative, // when relative clause is attached to word
	VerbWithTimePrep,TimePrepWithVerb, // when a preposition associated with a time is attached to a verb 
	VerbWithParticle,ParticleWithVerb, // track when a participle is really a particle
	VerbWithNext1MainVerbSameSubject,Next1MainVerbSameSubjectWithVerb,  // verbs are same tense and close to one another
	VerbWithNext1MainVerb=VerbWithNext1MainVerbSameSubject+VERB_HISTORY*2,Next1MainVerbWithVerb,  // verbs are same tense and close to one another
	WordWithPrepObjectWord,PrepObjectWordWithWord, // the object of a preposition is linked to object
	NextRelation=VerbWithNext1MainVerb+VERB_HISTORY*2,
	//ClauseWithVerb,VerbWithClause, // when general clause is attached to a thinksay verb clause index is a 
	numRelationWOTypes };

extern const lpchar_t *relationWOTypeStrings[];

enum relationComboTypes { SVOO, // SubjectWordWithVerb, DirectWordWithIndirectWord
                          SVO,  // SubjectWordWithVerb, VerbWithDirectWord
													SVIO, // SubjectWordWithVerb, InfinitiveWithWord
													VIO,  // VerbWithInfinitive, InfinitiveWithWord
													VIOO,  // VerbWithInfinitive, DirectWordWithIndirectWord
													OIO,  // WordWithInfinitive, InfinitiveWithWord
													OIOO,  // WordWithInfinitive, DirectWordWithIndirectWord
													VPO,  // VerbWithPrep, PrepWithPWord
													OPO   // WordWithPrep, PrepWithPWord 
};
