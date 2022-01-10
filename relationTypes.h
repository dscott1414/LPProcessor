#define VERB_HISTORY 4 // number of verbs back to analyze for relations
const wchar_t *getRelStr(int relationType);
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

extern const wchar_t *relationWOTypeStrings[];

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
