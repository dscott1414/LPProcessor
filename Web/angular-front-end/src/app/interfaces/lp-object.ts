export enum PermissionFlags {
  None = 0,
  identified = 1 << 0,
  plural = 1 << 1,
  male = 1 << 2,
  female = 1 << 3,
  neuter = 1 << 4,
  ownerPlural = 1 << 5,
  ownerMale = 1 << 6,
  ownerFemale = 1 << 7,
  ownerNeuter = 1 << 8,
  eliminated = 1 << 9,
  multiSource = 1 << 10,
  suspect = 1 << 11,
  verySuspect = 1 << 12,
  ambiguous = 1 << 13,
  partialMatch = 1 << 14,
  isNotAPlace = 1 << 15,
  genderNarrowed = 1 << 16,
  isKindOf = 1 << 17,
  wikipediaAccessed = 1 << 18,
  container = 1 << 19,
  dbPediaAccessed = 1 << 20,
  isTimeObject = 1 << 21,
  isLocationObject = 1 << 22,
  isWikiPlace = 1 << 23,
  isWikiPerson = 1 << 24,
  isWikiBusiness = 1 << 25,
  isWikiWork = 1 << 26,
}


export interface LpObject {
  index: number,
  objectClass: number,
  subType: number,
  begin: number,
  end: number,
  originalLocation: number,
  PMAElement: number,
  numEncounters: number,
  numIdentifiedAsSpeaker: number,
  numDefinitelyIdentifiedAsSpeaker: number,
  numEncountersInSection: number,
  numSpokenAboutInSection: number,
  numIdentifiedAsSpeakerInSection: number,
  numDefinitelyIdentifiedAsSpeakerInSection: number,
  PISSubject: number,
  PISHail: number,
  PISDefinite: number,
  replacedBy: number,
  ownerWhere: number,
  firstLocation: number,
  firstSpeakerGroup: number,
  firstPhysicalManifestation: number,
  lastSpeakerGroup: number,
  ageSinceLastSpeakerGroup: number,
  masterSpeakerIndex: number,
  htmlLinkCount: number,
  relativeClausePM: number,
  whereRelativeClause: number,
  whereRelSubjectClause: number,
  usedAsLocation: number,
  lastWhereLocation: number,
  spaceRelations: number[],
  duplicates: number[];
  aliases: number[];
  associatedNouns: string[];
  associatedAdjectives: string[];
  possessions: number[];
  genericNounMap: {}; // map <string, integer>
  mostMatchedGeneric: string;
  genericAge: number[]; // 4
  age: number;
  mostMatchedAge: number;
  flags: bigint;
  name: string;
}
