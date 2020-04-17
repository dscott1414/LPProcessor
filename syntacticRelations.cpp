#include <windows.h>
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "source.h"
#include "time.h"
#include "vcXML.h"
#include "profile.h"

cWordGroup::cWordGroup(void)
{ LFS
	index=-1;
	addedFromWords=addedToWords=addedSubGroups=false;
}

cWordGroup::cWordGroup(vector <tIWMM> &inFromWords,set <tIWMM,tFI::wordSetCompare> &inToWords,tIWMM word)
{ LFS
	fromWords=inFromWords;
	toWords=inToWords;
	if (word!=wNULL) toWords.insert(word);
#ifdef LOG_RELATION_GROUPING
	::lplog(L"Created group %s",summary().c_str());
#endif
}

cWordGroup::cWordGroup(tIWMM fromWord1,tIWMM fromWord2,tIWMM toWord1,tIWMM toWord2)
{ LFS
	fromWords.push_back(fromWord1);
	fromWords.push_back(fromWord2);
	toWords.insert(toWord1);
	toWords.insert(toWord2);
}

cWordGroup::cWordGroup(tIWMM self,tFI::cRMap::tcRMap *inToWords)
{ LFS
	fromWords.push_back(self);
	for (tFI::cRMap::tIcRMap twi=inToWords->begin(),twiEnd=inToWords->end(); twi!=twiEnd; twi++)
		toWords.insert(twi->first);
}

wchar_t *relationWOTypeStrings[]={
	L"SubjectWordWithVerb",L"VerbWithSubjectWord",
	L"SubjectWordWithNotVerb",L"NotVerbWithSubjectWord",
	L"VerbWithDirectWord",L"DirectWordWithVerb",L"VerbWithIndirectWord",L"IndirectWordWithVerb",L"DirectWordWithIndirectWord",L"IndirectWordWithDirectWord",
	L"VerbWithInfinitive",L"InfinitiveWithVerb",L"WordWithInfinitive",L"InfinitiveWithWord", // when infinitive is attached to verb or object
	L"AVerbWithInfinitive",L"AInfinitiveWithVerb",L"AWordWithInfinitive",L"AInfinitiveWithWord", // when infinitive is attached to verb or object
	//IWordWithInfinitive,InfinitiveWithIWord, // the object of the infinitive - NOT USED
	L"VerbWithPrep",L"PrepWithVerb",L"WordWithPrep",L"PrepWithWord", // when preposition is attached to verb or object
	L"AVerbWithPrep",L"APrepWithVerb",L"AWordWithPrep",L"APrepWithWord", // when preposition is attached to verb or object
	L"PWordWithPrep",L"PrepWithPWord", // the object of the preposition
	L"MasterVerbWithVerb",L"VerbWithMasterVerb",
	L"AdjectiveWithWord",L"WordWithAdjective", // may also be an identity relation He is fast
	L"AdjectiveIRNotWord",L"WordIRNotAdjective", // a negated identity relation He is NOT fast
	L"AdverbWithVerb",L"VerbWithAdverb",
	L"RelativeWithWord",L"WordWithRelative",
	L"VerbWithTimePrep",L"TimePrepWithVerb", // when a preposition associated with a time is attached to a verb
	L"VerbWithParticle",L"ParticleWithVerb" // track when a participle is really a particle
};

wchar_t *relationExtWOTypeStrings[]={
	L"*1*VerbWithNextMainVerbSameSubject",L"*1*NextMainVerbSameSubjectWithVerb",  // verbs are same tense and close to one another
	L"*1*VerbWithNextMainVerb",L"*1*NextMainVerbWithVerb"  // verbs are same tense and close to one another
};

wchar_t *getRelStr(int relationType)
{ LFS
	if (relationType<sizeof(relationWOTypeStrings)/sizeof(*relationWOTypeStrings))
		return relationWOTypeStrings[relationType];
	relationType-=sizeof(relationWOTypeStrings)/sizeof(*relationWOTypeStrings);
	int numExt=(relationType&(VERB_HISTORY-1));
	return relationExtWOTypeStrings[numExt];
	//static wchar_t temp[1024];
	//int numNext=(relationType&~(VERB_HISTORY-1))/VERB_HISTORY;
	//wcscpy(temp,relationExtWOTypeStrings[numExt]);
	//temp[1]='0'+numNext;
	//return temp;
}

relationWOTypes getComplementaryRelationship(int rType)
{ LFS
	return (relationWOTypes)((rType&1) ? rType-1 : rType+1);
}

vector <cWordGroup> groups[numRelationWOTypes]; // not used yet

vector <relc> relationCombos; // not used yet

#ifdef ACCUMULATE_GROUPS
/*
TODO

compare object against group
simple comparison
simple extended comparison

EXAMPLE: THROW THE BALL
word: THROW
if word known and relation known, increment relation.
for each group of THROW having BALL as a relation, increment group and recalculate group difference
if new word, create word and word relation structure
if new relation,
for each relation
for each word, pick back relation (ex:) or back extended relation (see EX2)
THROW THE BALL
for the word throw, pick the word ball, and pick vObject:
for every relation in pickObject
do group formation for relation
for every group associated with the relation
do GROUP MEMBERSHIP

GROUP MEMBERSHIP
for each group
if already in child group, skip.
compare every relationship to the group relationship in order of frequency
if all objects are matched in group membership in group confirmed
if in parent group, remove parent group affiliation
add object to group and modify group statistics.
calculate group difference for object:
GROUP O VERB RELATION THROW has count 300
GROUP O total count is 10100
OBJECT BALL VERB RELATION THROW has count 200
OBJECT BALL total count is 5020
VERB RELATION  simple comparison
GROUP O       THROW          300 / 10100 = 0.0297
OBJECT BALL   THROW          200 / 5020  = 0.04

absolute difference = | 0.0297 - 0.04 | = 0.01
add 0.01*10100 to total 'difference'
add all differences for all relations to group for each type of relation
divide by total count of each type of relation
store group difference.
END GROUP MEMBERSHIP

EX & EX2
JACK THREW THE BALL.
the word JACK (SUBJECT)
SIMPLE:for each oSubject OF THREW
EXTENDED:for each cr of each oObject (where oObject=BALL) of THROWS [which objects 'THROW' BALLS?]
EXTENDED:for each cr of each vObject (where vObject=THREW) of BALL [which objects THROW 'BALLS'?]
BILLY HURT JACK
the word JACK (OBJECT)
SIMPLE:for each oObject (of Relation) of 'HURT'
EXTENDED:for each cr of each oSubject (where oSubject=BILLY) of HURT [which objects were 'HURT' by BILLY?]
EXTENDED:for each cr of each vSubject (where vSubject=HURT) of BILLY [which objects were HURT by 'BILLY'?]
THE BALL WENT TO JACK
the word JACK (OBJECT OF PREP)
SIMPLE:for each object with objectOfPrep of 'BALL'
I MEANT JACK FROM THE UNIVERSITY.
the word JACK (MODIFIED BY PREP)
SIMPLE:for each object with modifiedByPrep OF 'UNIVERSITY'
I LOVE COMPLEX THINGS.
the word THINGS (MODIFIED BY ADJECTIVE)
SIMPLE:for each object

simple comparison:
for each relation (verb-subject, verb-object, verb prepobject etc):
search by frequency (linked list) in object 1 (or group)
binary search in object 2
for each object in both lists, increase comparison #.
sort objects by comparison #.
simple extended comparison (only subjects and objects):
for each verb with the greatest simple comparison # 's,
compare object of verb if subject, or subject of verb if object.
for each object in both lists, increase extended comparison #.

complex comparison:
THE BALL (OA)
THROW THE BALL    210 times
KICK  THE BALL    205
PITCH THE BALL    150
PUT   THE BALL    100
BURY  THE BALL    5   [NOT included because < 5% of total]
all others afterward are ignored
TOTAL             665

THROW (VA)
THROW THE ELECTION  30
THROW THE MAN       10
THROW THE BASEBALL  50
THROW THE MOUNTAIN  2
all others afterward are ignored
TOTAL               90

THE BASEBALL (OB)              MATCHED ENTRY    COMP
THROW THE BASEBALL 200 times      X             1-|(200/380)-(210/665)| = 0.789
BUNT THE BASEBALL   50
PITCH THE BASEBALL 100            X             1-|(100/380)-(150/665)| = 0.962
DROP THE BASEBALL   30
DIP  THE BASEBALL    2
all others afterward are ignored
TOTAL              380            2             1.751

accumulate THE BASEBALL (OB) in list with (2,1.751)

GROUP (THROW, PITCH) with members (BASEBALL, BALL)

compare group OB and OA
for every object OB!=OA exceeding 5% of the total count for VA
compare verb list of OB and OA.  Insert OB into vector along with # of verbs matched and COMP calculation.

for each object OA in sentence,
increase verb VA.  (increase new count && total count)
If VA NEWLY exceeds 5% of the total count
compare every object of VA (OB) with OA
for each OB,
find a group having all MATCHED ENTRIES GA
if no group found,
create a group sharing OB and OA, with the MATCHED ENTRIES and COMP #s
else
add OB to group GA and add COMP #s to group COMP #s, divide by # in group to maintain average COMP #s for group
If new count > 10% of total count:
zero new count
for every group OA belongs to,
calculate COMP for each entry
total COMP, rerank object within GROUP based on COMP, reaverage COMP in group

the COMPANY LISTED its SHARES.
G.M. LISTED its SHARES.
extended groups:
if a group is created in previous process:
for the group created (LISTED) and the secondary object (SHARES) :
for each object member in group having GROUP verb and secondary object
COMP for each object member and deposit into extended group

*/
wstring cWordGroup::summary(void)
{ LFS
	wstring temp;
	for (vector<tIWMM>::iterator fw=fromWords.begin(),fwEnd=fromWords.end(); fw!=fwEnd; fw++)
		temp+=(*fw)->first+L" ";
	temp+=L"-> ";
	for (set <tIWMM,tFI::wordSetCompare>::iterator tw=toWords.begin(),twEnd=toWords.end(); tw!=twEnd; tw++)
		temp+=(*tw)->first+L" ";
	return temp;
}

// does any group containing fromWord 'stop' need to add 'banker' as a toWord?
// which words in this group contain word as a toWord?
// return: whether group now contains the relation
bool cWordGroup::incorporateMapping(relationWOTypes relationType,tIWMM word,vector <tIWMM> &subGroup)
{ LFS
	if (toWords.find(word)!=toWords.end()) return true; // group already contains mapping
	for (vector<tIWMM>::iterator twi=fromWords.begin(),twiEnd=fromWords.end(); twi!=twiEnd; twi++)
		if ((*twi)->second.relationMaps[relationType] && (*twi)->second.relationMaps[relationType]->r.find(word)!=(*twi)->second.relationMaps[relationType]->r.end())
			subGroup.push_back(word);
#ifdef LOG_RELATION_GROUPING
	if (subGroup.size()==fromWords.size())
		::lplog(L"Group %s (%s) adds toWord %s",getRelStr(relationType),summary().c_str(),word->first.c_str());
	else if (subGroup.size()>1)
	{
		wstring subGroupWords;
		for (unsigned int I=0; I<subGroup.size(); I++)
			subGroupWords+=subGroup[I]->first+" ";
		subGroupWords.erase(subGroupWords.length()-1);
		::lplog(L"Create a subGroup %s (%s) out of Group (%s) with toWord %s",getRelStr(relationType),subGroupWords.c_str(),summary().c_str(),word->first.c_str());
	}
#endif
	if (subGroup.size()==fromWords.size())
	{
		toWords.insert(word);
		return true;
	}
	return false;
}

// is there another relationship X -> Y of relationType such that also self->Y and X -> word ?
// if so, fromWord=X and toWord=Y.
// ---------------------
// these relations already existed:
// self=stop, word=banker
// stop -> Y                                            stop->cook
// X -> banker                                          sit->banker
// X -> Y                                               sit->cook
// now comes this relation:
// stop -> banker
// ---------------------
// stop -> setY
// set each word in complementary relationship to banker - exclude stop.
// for each word in setY (Y) except banker:
//   if there is exactly one word in complementary relation of Y (X2) already set, successful group creation.
//
bool tFI::intersect(relationWOTypes relationType,tIWMM word,tIWMM self,tIWMM &fromWord,tIWMM &toWord)
{ LFS
	relationWOTypes crType=getComplementaryRelationship(relationType);
	if (!word->second.relationMaps[relationType] || word->second.relationMaps[relationType]->r.size()<2 ||
		!self->second.relationMaps[relationType] || self->second.relationMaps[relationType]->r.size()<2 ||
		!word->second.relationMaps[crType] || word->second.relationMaps[crType]->r.size()<2 ||
		!self->second.relationMaps[crType] || self->second.relationMaps[crType]->r.size()<2)
		return false;
	// word=banker | set a flag on everything a banker does
	for (cRMap::tIcRMap wi=word->second.relationMaps[crType].r.begin(),wiEnd=word->second.relationMaps[crType].r.end(); wi!=wiEnd; wi++)
		wi->first->second.flags|=intersectionGroup;
	// exclude stop
	self->second.flags&=~intersectionGroup;
	toWord=wNULL;
	// set flag of all groups of self
	vector <int>::iterator gi=self->second.groupList[relationType].begin(),giEnd=self->second.groupList[relationType].end();
	for (; gi!=giEnd; gi++)
		groups[relationType][*gi].otherFlag=true;
	// for everyone who stops (except for a banker)
	for (wi=self->second.relationMaps[relationType].r.begin(),wiEnd=self->second.relationMaps[relationType].r.end(); wi!=wiEnd; wi++)
	{
		fromWord=wNULL;
		bool moreThanOne=false;
		// for everything a cook (for example) does, check whether the flag for everything a banker does is set
		for (cRMap::tIcRMap wwi=wi->first->second.relationMaps[crType].r.begin(),wwiEnd=wi->first->second.relationMaps[crType].r.end(); wwi!=wwiEnd; wwi++)
			if (wwi->first->second.flags&intersectionGroup)
			{
				if (moreThanOne=(fromWord!=wNULL)) break;
				fromWord=wwi->first;
			}
			if (!moreThanOne && fromWord!=wNULL)
			{
				// check that self and fromWord (cook and banker) do not belong to the same group of relationType
				gi=fromWord->second.groupList[relationType].begin(),giEnd=fromWord->second.groupList[relationType].end();
				for (; gi!=giEnd; gi++)
					if (groups[relationType][*gi].otherFlag)
						break;
				if (gi==giEnd)
				{
					toWord=wi->first;
					break;
				}
			}
	}
	if (toWord==wNULL) return false;
	// erase flag of all groups of self
	gi=self->second.groupList[relationType].begin(),giEnd=self->second.groupList[relationType].end();
	for (; gi!=giEnd; gi++)
		groups[relationType][*gi].otherFlag=false;
#ifdef LOG_RELATION_GROUPING
	wstring selftemp,wordtemp,intertemp;
	for (wi=self->second.relationMaps[relationType].r.begin(),wiEnd=self->second.relationMaps[relationType].r.end(); wi!=wiEnd; wi++)
		selftemp+=wi->first->first+" ";
	selftemp+="->";
	for (wi=self->second.relationMaps[crType].r.begin(),wiEnd=self->second.relationMaps[crType].r.end(); wi!=wiEnd; wi++)
		selftemp+=wi->first->first+" ";
	for (wi=word->second.relationMaps[relationType].r.begin(),wiEnd=word->second.relationMaps[relationType].r.end(); wi!=wiEnd; wi++)
		wordtemp+=wi->first->first+" ";
	wordtemp+="->";
	for (wi=word->second.relationMaps[crType].r.begin(),wiEnd=word->second.relationMaps[crType].r.end(); wi!=wiEnd; wi++)
		wordtemp+=wi->first->first+" ";
	intertemp=self->first + " " + fromWord->first+" -> " + word->first + " " + toWord->first;
	::lplog(L"INTERSECT (%s) %s [%s] with %s [%s]: %s",getRelStr(relationType),self->first.c_str(),selftemp.c_str(),
		word->first.c_str(),wordtemp.c_str(),
		intertemp.c_str());
#endif
	return true;
}
#endif // ACCUMULATE_GROUPS

// this return value is pointer to iterator (and mri is static) because
// tIcRMap refers to wordMapCompare, which refers to tIWMM, which refers to tFI, which has a map of cRMaps in it
// which refer to tIcRMap, and then again.
tFI::cRMap::tIcRMap tFI::cRMap::addRelation(int sourceId,int fromWhere,tIWMM toWord,bool &isNew,int count,bool fromDB)
{ LFS
	typedef pair <tIWMM,tRelation> ptcRMap;
	tIcRMap mri;
	if (isNew=(mri=r.find(toWord))==r.end())
	{
		pair <tIcRMap,bool> mr=r.insert(ptcRMap(toWord,tRelation(sourceId,fromWhere,count,fromDB)));
		mri=mr.first;
	}
	else
	{
		//bySequence.erase(mri); // constantly maintained map of most frequent relations - not currently used
		mri->second.increaseCount(sourceId,fromWhere,count,fromDB);
	}
	//bySequence.insert(mri);
	return mri;
}

/*
The banker stopped. (new relation) VerbWithSubjectWord (stop,banker)
collect all SubjectWordWithVerb groups having toWord stop.

chef stops
banker stops         As (chef, banker) stop
plumber carries
electrician carries  Bs (plumber, electrician) carry
plumber throws                                                 Av plumber (throw, carry)
electrician throws   Bs (plumber, electrician) carry, throw  | Av plumber, electrician (throw, carry)
banker throws        Cs (plumber, electrician, banker) throw | Bv plumber, electrician, banker (throw) Bv subgroup of Av
chef throws          As (chef, banker) stop, throw           | Bv plumber, electrician, banker, chef (throw)

printer takes
carpenter takes      Ds (printer, carpenter) take
electrician takes    Ds (printer, carpenter, electrician) take
chef takes           Ds (printer, carpenter, electrician, chef) take
banker takes         Ds (printer, carpenter, electrician, chef, banker) take
As (chef, banker) stop, throw, take
Es (electrician, banker) throw, take
plumber tarries                                               Cv plumber (throw, carry, tarry)
chef    tarries      Fs (plumber, chef) tarry                 Dv plumber, chef (throw, tarry)

scan all groups with chef.  if any part of the group uses stop, create a subgroup with that, but
if the entire group uses it, add stop to the group.
scan all groups with stop.  if any part of the group uses chef, create a subgroup with that, but
if the entire group uses it, add chef to the group.

treat individual words as groups.
*/
tFI::cRMap::tIcRMap tFI::addRelation(int where,int relationType,tIWMM word)
{ LFS
	allocateMap(relationType);
	bool isNew;
	tFI::cRMap::tIcRMap p=relationMaps[relationType]->addRelation(sourceId,where,word,isNew,1,false);
#ifdef ACCUMULATE_GROUPS
	if (!isNew || word==this) return p;
	// example: The banker stopped.  self=stopped rType=VerbWithSubjectWord word=banker
	// all VerbWithSubjectWord groups of 'stop' - all groups of verbs that are used with the same subjects
	// add banker to the 'toWord' list of any group of verbs of the relation VerbWithSubjectWord containing all the fromWords
	for (unsigned int I=0,originalSize=groupList[relationType].size(); I<originalSize; I++)
	{
		vector <tIWMM> subGroup;
		int groupNum=groups[relationType].size();
		int gi=groupList[relationType][I];
		if (gi>=groupNum || gi<0)
			::lplog(LOG_FATAL_ERROR,L"Group %s #%d is illegal (0 - %d)!",getRelStr(relationType),gi,groupNum);
		// do any subject groups containing 'stop' need to add 'banker' as a toWord?
		// from:stop,drop,kick->to:podiatrist,electrician do stop, drop and kick all have a toWord of 'banker'
		// from:stop,drop->to:podiatrist,electrician,banker
		if (!groups[relationType][gi].incorporateMapping(relationType,word,subGroup) && subGroup.size()>1)
		{
			// if only some of the words have 'banker' as a toWord, make a new group with fromWords=subGroup and toWords=groups[relationType][*gi].toWords
			groups[relationType].push_back(cWordGroup(subGroup,groups[relationType][gi].toWords,word));
			groups[relationType][gi].subGroups.push_back(groupNum);
			for (vector<tIWMM>::iterator sgi=subGroup.begin(),sgiEnd=subGroup.end(); sgi!=sgiEnd; sgi++)
				(*sgi)->second.groupList[relationType].push_back(groupNum);
		}
	}
	tIWMM fromWord,toWord;
	if (intersect(relationType,word,this,fromWord,toWord))
	{
		int groupNum=groups[relationType].size();
		groups[relationType].push_back(cWordGroup(this,fromWord,word,toWord));
#ifdef LOG_RELATION_GROUPING
		::lplog(L"Created %s group #%d:%s",getRelStr(relationType),groupNum,groups[relationType][groupNum].summary().c_str());
#endif
		fromWord->second.groupList[relationType].push_back(groupNum);
		//toWord->second.groupList[getComplementaryRelationship(relationType)].push_back(groupNum);
	}
#endif
	return p;
}

int Source::makeRelationHash(int num1,int num2,int num3)
{ LFS
	return num1+(num2<<14)+(num3<<28); // num3 must be small
}

void Source::addDelayedWordRelations(int where,int fromWhere,int toWhere,int relationType)
{ LFS
	delayedWordRelations.push_back(where);
	delayedWordRelations.push_back(fromWhere);
	delayedWordRelations.push_back(toWhere);
	delayedWordRelations.push_back(relationType);
}

/*
  int where
	int fromWhere
	int toWhere
	int relationType
	int relationCount
	int finalRelation (default value -1)

*/
void Source::createProbableRelationsList()
{ LFS
	// for each sentence
  //   create a map tIWMM->where for each word in sentence
	//   for each word in sentence
	//     for each syntactic relation relating the word to another word in the sentence
	//       fill in above structure.
	// resolveRelations will fill in finalRelation which is an index into delayedWordRelations.
}

// 
void Source::reportProbableRelationsAccuracy()
{ LFS
	// for each sentence
	//   (1) count how many relations were proper and add up their count.
	//   (2) add up count of relations that were missed.
	//   log per sentence
	// log sum (1), sum (2), average (1), average (2), low (1), high (1), low (2), high (2)
}

void Source::resolveWordRelations()
{ LFS 
	unsigned int sni=0;
	//logCache=0;
	for (int I=0; I<(int)delayedWordRelations.size(); I+=4)
	{
		int where=delayedWordRelations[I];
		int fromWhere=delayedWordRelations[I+1];
		int toWhere=delayedWordRelations[I+2];
		int relationType=delayedWordRelations[I+3];
		if (I>4 && 
				where==delayedWordRelations[I-4] && 
				fromWhere==delayedWordRelations[I-3] && 
				toWhere==delayedWordRelations[I-2] && 
				relationType==delayedWordRelations[I-1])
			continue;
		//lplog(L"where=%d from=%d to=%d rt=%d",where,fromWhere,toWhere,relationType);
		while (sni+1<subNarratives.size() && subNarratives[sni+1]<=where)
			sni++;
		// if not in narrative but in quotes, narrativeNum=-1
		// if in narrative and in quotes, narrativeNum=sni
		// if not in quotes, narrativeNum=0
		switch (relationType)
		{
		case PrepWithPWord:						
			addRelations(where,m[fromWhere].word,fullyResolveToClass(toWhere),relationType);
			break;
		case AWordWithPrep: 			
		case WordWithPrep:
			addRelations(where,fullyResolveToClass(fromWhere),m[toWhere].word,relationType);
			break;
		case DirectWordWithIndirectWord:
			addRelations(where,fullyResolveToClass(fromWhere),fullyResolveToClass(toWhere),relationType);
			break;
		case VerbWithPrep:
			addRelations(where,m[fromWhere].getVerbME(fromWhere,13,lastNounNotFound,lastVerbNotFound),m[toWhere].word,VerbWithPrep);
			break;
		case VerbWithIndirectWord:
		case VerbWithDirectWord:
		case NotVerbWithSubjectWord:
		case VerbWithSubjectWord:
			unsigned __int64 or=m[toWhere].objectRole;
			int narrativeNum=(or&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE)) ? -1:0;
			if (or&IN_EMBEDDED_STORY_OBJECT_ROLE) narrativeNum=(signed)sni;
			int subject=-1;
			if (m[fromWhere].relSubject>=0) 
			{
				subject=m[m[fromWhere].relSubject].getObject();
				if (m[m[fromWhere].relSubject].objectMatches.size()>0)
					subject=m[m[fromWhere].relSubject].objectMatches[0].object;
			}
			tFI::cRMap::tIcRMap r=addRelations(where,m[fromWhere].getVerbME(fromWhere,1,lastNounNotFound,lastVerbNotFound),fullyResolveToClass(toWhere),relationType);
			bool physicallyEvaluated;
			int pp=(toWhere>=0 && m[toWhere].getObject()>=0 && m[toWhere].beginObjectPosition>=0 && m[m[toWhere].beginObjectPosition].principalWherePosition>=0 &&
				  (physicallyPresentPosition(toWhere,physicallyEvaluated) && physicallyEvaluated));
			relationHistory.push_back(cRelationHistory(r,toWhere,narrativeNum,subject,pp));
			break;
		}
	}
	delayedWordRelations.clear();
}

// the beginning preposition binds to subject, if in subject
//   if not in subject, binds to subject and object
// if not beginning prep, binds to immediately previous object of prep.
// fills in these relation tags:
//   VerbWithTimePrep: if subobject a DATE or TIME
//   AWordWithPrep (A=ambiguous, is if binding objects>1), WordWithPrep; preposition with its binding object
//   SubjectWordWithNotVerb,SubjectWordWithVerb: if prep is 'by' and verb sense is passive
//	 PrepWithPWord: preposition with its object
//	 AVerbWithPrep : VerbWithPrep: verb with its preposition
tFI::cRMap::tIcRMap Source::addRelations(int where,tIWMM from,tIWMM to,int relationType)
{ LFS
	//tIWMM fromME=from->second.mainEntry,toME=to->second.mainEntry;
	//tIWMM fromFinal=(fromME==wNULL) ? from:fromME,toFinal=(toME==wNULL) ? to:toME;
	/*
	pair< set<int>::iterator, bool > pr = nrr.insert(makeRelationHash(fromFinal->second.index,toFinal->second.index,relationType));
	tFI::cRMap::tIcRMap tmp;
	if (!pr.second && fromFinal->second.relationMaps[relationType]!=NULL &&
		(tmp=fromFinal->second.relationMaps[relationType]->r.find(toFinal))!=
		fromFinal->second.relationMaps[relationType]->r.end())
		return tmp;
		*/
   from->second.changedSinceLastWordRelationFlush=true;
   to->second.changedSinceLastWordRelationFlush=true;
	 if (debugTrace.traceRelations) 
		lplog(L"%06d:TRQQQ %s:%s -> %s",where,getRelStr(relationType),from->first.c_str(),to->first.c_str());
	to->second.addRelation(where,getComplementaryRelationship(relationType),from);
	return from->second.addRelation(where,relationType,to);
}

bool Source::inTag(tTagLocation &innerTag,tTagLocation &outerTag)
{ LFS
	return innerTag.sourcePosition>=outerTag.sourcePosition && (innerTag.sourcePosition+innerTag.len<=outerTag.sourcePosition+outerTag.len);
}

void Source::adjustToHailRole(int where)
{ LFS
	vector <WordMatch>::iterator im=m.begin()+where;
	// additional hail check if gendered object is in between two commas and is not in a larger pattern (lastBeginS1==-1)
	// “[tuppence:tommy] yes , little lady[cousin,jane] , out with it[yarn] . ”  Secret Adversary
	// cannot already be a hail, or part of a multiple noun construction, or an appositive
	unsigned __int64 or=im->objectRole&(HAIL_ROLE|MPLURAL_ROLE|RE_OBJECT_ROLE);
	int oc=(im->getObject()>=0) ? objects[im->getObject()].objectClass:-1;
	if ((or&HAIL_ROLE) && ((im->flags&WordMatch::flagAdjectivalObject)  ||
		  (im->getObject()>=0 && objects[im->getObject()].ownerWhere>=0 && (m[objects[im->getObject()].ownerWhere].word->second.inflectionFlags&SECOND_PERSON)) || // your cousin
		  ((/*oc==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && */im->beginObjectPosition>=0 && m[im->beginObjectPosition].queryForm(determinerForm)>=0) || // a maid
			 oc==NON_GENDERED_GENERAL_OBJECT_CLASS || oc==NON_GENDERED_BUSINESS_OBJECT_CLASS || oc==VERB_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS) ||
			 oc==PRONOUN_OBJECT_CLASS ||
			 (oc==META_GROUP_OBJECT_CLASS && im->endObjectPosition-im->beginObjectPosition==1 && m[im->beginObjectPosition].queryForm(numeralCardinalForm)>=0))) // one
	{
		or&=~HAIL_ROLE;
		im->objectRole&=~HAIL_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%06d:Removed HAIL role.",where);
	}
	// Boris , here , knows pretty ways of making people speak ! ”
	// _HAIL will match this, but _S1 should overrule
	if ((or&HAIL_ROLE) && im->pma.queryPattern(L"_HAIL")!=-1 && im->pma.queryPattern(L"__S1")!=-1)
	{
		or&=~HAIL_ROLE;
		im->objectRole&=~HAIL_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%06d:Removed HAIL role (_S1 priority).",where);
	}
	// Elementary, my dear Watson
  if ((where>=4 && m[where].word->first==L"watson" && m[where-1].word->first==L"dear" && m[where-2].word->first==L"my" && 
			m[where-3].word->first==L"," && m[where-4].word->first==L"elementary") && !objects[im->getObject()].numIdentifiedAsSpeaker)
	{
		or&=~HAIL_ROLE;
		im->objectRole&=~HAIL_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%06d:Removed HAIL role (saying).",where);
	}
	if (!(or&(HAIL_ROLE|MPLURAL_ROLE)) && 
		  im->beginObjectPosition>1 && im->getObject()>=0 &&
			// Ever heard of the word ‘QS graft , ’ sir ?
			(m[im->beginObjectPosition-1].word->first==L"," || (m[im->beginObjectPosition-1].word->first==L"’" && m[im->beginObjectPosition-2].word->first==L",")) && 
			// You are a clever woman, Rita;
      im->endObjectPosition<(signed)m.size() && (m[im->endObjectPosition].word->first==L"," || m[im->endObjectPosition].word->first==L";" || m[im->endObjectPosition].word->first==L"." || m[im->endObjectPosition].word->first==L"?") &&
			objects[im->getObject()].getSubType()<0 && 
		  (oc==NAME_OBJECT_CLASS || oc==GENDERED_GENERAL_OBJECT_CLASS ||
			 oc==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || oc==GENDERED_DEMONYM_OBJECT_CLASS || oc==GENDERED_RELATIVE_OBJECT_CLASS))
	{
		// Your story, little lady, can be ...
		// if this object that is claimed to be an appositive to the previous object (which must be before the ,), the previous object cannot be neuter.
		if ((or&RE_OBJECT_ROLE) && 
			  ((im->beginObjectPosition>1 && m[im->beginObjectPosition-2].getObject()>0 && objects[m[im->beginObjectPosition-2].getObject()].neuter) ||
		     (im->endObjectPosition-im->beginObjectPosition==1)))
			// me, child, // you're telling me , lady, that ...
			im->objectRole&=~RE_OBJECT_ROLE;
		// G U E - 18661 - spelling out 
		cName *name;
		bool spelling=(oc==NAME_OBJECT_CLASS && (name=&objects[im->getObject()].name)->first!=wNULL && 
				name->first->first.length()==1 && name->last!=wNULL && name->last->first.length()==1 &&
				m[where+1].word->first!=L".");
		if (!(im->objectRole&RE_OBJECT_ROLE) && !spelling)
		{
			im->objectRole|=HAIL_ROLE;
			or|=HAIL_ROLE;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Acquired HAIL role.",where);
		}
	}
	// Here[here] we[tommy,julius] are . Ebury , Yorks .
	// Come at once , Moat House , Ebury , Yorkshire , great developments -- Tommy
	int so;
	if (oc==NAME_OBJECT_CLASS && objects[im->getObject()].numDefinitelyIdentifiedAsSpeaker==0 &&
		  im->beginObjectPosition>0 && 
			m[im->beginObjectPosition-1].word->second.isSeparator() && 
      im->endObjectPosition+2<(signed)m.size() && m[im->endObjectPosition].word->first==L"," &&
			(so=m[im->endObjectPosition+1].getObject())>=0 && objects[so].numDefinitelyIdentifiedAsSpeaker==0 &&
			(objects[so].getSubType()>=0 || (objects[so].name.hon==wNULL && !objects[so].isNotAPlace && !objects[so].PISDefinite && objects[im->getObject()].getSubType()>=0)) &&
			objects[so].objectClass==NAME_OBJECT_CLASS && 
			m[im->endObjectPosition+1].endObjectPosition>=0 && m[im->endObjectPosition+1].endObjectPosition<(signed)m.size() &&
			m[m[im->endObjectPosition+1].endObjectPosition].word->second.isSeparator())
	{
		if (or&HAIL_ROLE)
		{
			or&=~HAIL_ROLE;
			im->objectRole&=~HAIL_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%06d:Removed HAIL role (PLACE).",where);
		}
		objects[im->getObject()].setSubType(WORLD_CITY_TOWN_VILLAGE);
		if (m[im->endObjectPosition+1].objectRole&HAIL_ROLE)
		{
			m[im->endObjectPosition+1].objectRole&=~HAIL_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%06d:Removed HAIL role (PLACE).",im->endObjectPosition+1);
		}
	}
	// “[st:tommy] Mr . Hersheimmer -- Mr . Beresford -- Dr . Roylance . How ishas the patient[patient] ? ”
	// if at the beginning, and there is a --, then a name, and then another -- and a name, and then a EOS, make the last name HAIL.
	int w;
	if (!(or&HAIL_ROLE) && m[where].beginObjectPosition>0 && m[m[where].beginObjectPosition-1].queryForm(quoteForm)>=0 && 
		  m[where].getObject()>=0 && objects[m[where].getObject()].objectClass==NAME_OBJECT_CLASS &&
			(w=m[where].endObjectPosition)>=0 && w+1<(signed)m.size())
	{
		if (m[w].word->first==L"--" && 
				m[w+1].getObject()>=0 && objects[m[w+1].getObject()].objectClass==NAME_OBJECT_CLASS)
		{
			int secondObject=w+1;
			if ((w=m[w+1].endObjectPosition)>=0 && m[w].word->first==L"--" && 
					m[w+1].getObject()>=0 && objects[m[w+1].getObject()].objectClass==NAME_OBJECT_CLASS)
			{
				m[where].objectRole|=IN_QUOTE_REFERRING_AUDIENCE_ROLE;
				m[where].flags&=~WordMatch::flagAdjectivalObject; // caused by misparse - this object is not an adjective of the next object
				m[secondObject].objectRole|=IN_QUOTE_REFERRING_AUDIENCE_ROLE;
				m[w+1].objectRole|=HAIL_ROLE;
				if (debugTrace.traceRole)
				{
					lplog(LOG_ROLE,L"%06d:Acquired IN_QUOTE_REFERRING_AUDIENCE_ROLE role (Introduction).",where);
					lplog(LOG_ROLE,L"%06d:Acquired IN_QUOTE_REFERRING_AUDIENCE_ROLE role (Introduction).",secondObject);
					lplog(LOG_ROLE,L"%06d:Acquired HAIL role (Introduction).",w+1);
				}
			}
		}
	}
	// Miss Tuppence is wrongly parsed as the object of a preposition
	//  Lock the door on the outside , please , Miss Tuppence , and take out the key .
	if ((im->objectRole&(HAIL_ROLE|PREP_OBJECT_ROLE))==(HAIL_ROLE|PREP_OBJECT_ROLE) && m[where-1].word->first!=L",")
	{
		or&=~HAIL_ROLE;
		im->objectRole&=~HAIL_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%06d:Removed HAIL role (PREP).",where);
	}
}

// is innerTag in outerTag?
bool Source::checkAmbiguousVerbTense(int whereVerb,int &sense,bool inQuote,tIWMM masterVerbWord)
{ LFS
	// tense statistics
	// if the tense is ambiguous between past and present (due to verb form being identical between present and past forms)
	// make the tense = the last tense by past or present.
	if ((sense==VT_PRESENT || sense==VT_PAST) &&
		(masterVerbWord!=wNULL && (masterVerbWord->second.inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST))==(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST)) ||
		(masterVerbWord==wNULL && (m[whereVerb].word->second.inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST))==(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST)))
	{
		int tmpLastSense=lastSense&~(VT_POSSIBLE|VT_PASSIVE|VT_NEGATION|VT_EXTENDED|VT_VERB_CLAUSE);
		// if narrator, and currently ambiguous and sense is present, then set sense to past
		// because the narrator is usually in the past
		if (!inQuote && sense==VT_PRESENT)
			sense=VT_PAST;
		// if it is ambiguous and it doesn't match the lastSimplifiedTense, switch.
		else if (sense==VT_PAST && (tmpLastSense==VT_PRESENT || tmpLastSense==VT_PRESENT_PERFECT))
			sense=VT_PRESENT;
		else if (sense==VT_PRESENT && (tmpLastSense==VT_PAST || tmpLastSense==VT_PAST_PERFECT))
			sense=VT_PAST;
		else
			return true;
	}
	return false;
}

void Source::trackVerbTenses(int where,vector <tTagLocation> &tagSet,bool inQuote,bool inQuotedString,bool inSectionHeader,
														 bool ambiguousSense,int sense,bool &tenseError)
{ LFS
	if (sense<0 || ambiguousSense) // not TENSE_NOT_SPECIFIED
	{
		if (sense!=TENSE_NOT_SPECIFIED)
		{
			lastSense=-1;
			if (debugTrace.traceRelations)
				lplog(L"%06d:set lastSense to %d (TVT 1).",where,lastSense);
		}
		return;
	}
	int simplifiedTense=getSimplifiedTense(sense),lastSimplifiedTense=getSimplifiedTense(lastSense);
	wstring tmpstr;
	// Being that he was a porter, he is/was still a good person. -- VERB_CLAUSE has no effect on tense flow
	// would and could etc also have no impact on actual time flow
	// quoted strings are separate from the rest of the sentence and don't represent a change to time flow
	// section headers can be in present tense, but don't represent a change in time flow
	// don't repeat a tense in the same position
	// don't attempt to estimate a verb tense appearing in the second position after another verb - this
	// requires more sophisticated processing to get the tense of the verb immediately before this one - TODO
	if (!(sense&VT_VERB_CLAUSE) && !(sense&VT_POSSIBLE) && !inQuotedString && !inSectionHeader)
	{
		if (inQuote)
		{
			speakerTenseStatistics[simplifiedTense].occurrence++;
			if (sense&VT_PASSIVE) speakerTenseStatistics[simplifiedTense].passiveOccurrence++;
			if (lastSimplifiedTense>=0) speakerTenseStatistics[lastSimplifiedTense].followedBy[simplifiedTense]++;
			numTotalSpeakerVerbTenses++;
		}
		else
		{
			narratorTenseStatistics[simplifiedTense].occurrence++;
			if (sense&VT_PASSIVE) narratorTenseStatistics[simplifiedTense].passiveOccurrence++;
			if (lastSimplifiedTense>=0) narratorTenseStatistics[lastSimplifiedTense].followedBy[simplifiedTense]++;
			numTotalNarratorVerbTenses++;
		}
	}
	if (!(sense&VT_VERB_CLAUSE) && !(sense&VT_POSSIBLE) && !inQuotedString && !inSectionHeader)
	{
		if (inQuote)
		{
			speakerFullTenseStatistics[sense].occurrence++;
			if (lastSense>=0) speakerFullTenseStatistics[lastSense].followedBy[simplifiedTense]++;
			numTotalSpeakerFullVerbTenses++;
		}
		else
		{
			if (sense==VT_PRESENT)
			{
				if (debugTrace.traceSpeakerResolution)
				{
					::printTagSet(LOG_INFO,L"VERBTENSE",-1,tagSet);
					lplog(L"%06d:Present narrator tense from sense %s lastSense=%s simplifiedTense=%d lastSimplifiedTense=%d ambiguousSense=%s\n",where,
						senseString(tmpstr,sense).c_str(),senseString(tmpstr,lastSense).c_str(),
						simplifiedTense,lastSimplifiedTense,(ambiguousSense) ? L"true":L"false");
				}
				tenseError=true;
			}
			narratorFullTenseStatistics[sense].occurrence++;
			if (lastSense>=0) narratorFullTenseStatistics[lastSense].followedBy[simplifiedTense]++;
			numTotalNarratorFullVerbTenses++;
		}
	}
	bool inRelativeClause=(m[where].objectRole&(EXTENDED_ENCLOSING_ROLE|NONPAST_ENCLOSING_ROLE|NONPRESENT_ENCLOSING_ROLE|SENTENCE_IN_REL_ROLE|SENTENCE_IN_ALT_REL_ROLE))!=0;
	if (!(sense&VT_VERB_CLAUSE) && !inQuotedString && !inSectionHeader && !inRelativeClause)
	{
		lastSense=sense;
		if (debugTrace.traceRelations)
			lplog(L"%06d:set lastSense to %s (TVT 2) (inQuote=%s).",where,senseString(tmpstr,lastSense).c_str(),(inQuote) ? L"true":L"false");
	}
}

void Source::recordVerbTenseRelations(int where,int sense,int subjectObject,int whereVerb)
{ LFS
	// just present, past or future
	int pureTense=(getSimplifiedTense(sense)&7)/2;
	// skip should/could verbs
	if (subjectObject>=0 && !(sense&(VT_POSSIBLE|VT_VERB_CLAUSE)))
	{
		cLastVerbTenses *lastVerbTense=objects[subjectObject].lastVerbTenses;
		for (int objectLastTense=0; objectLastTense<VERB_HISTORY; objectLastTense++,lastVerbTense++)
		{
			if (lastVerbTense->lastTense!=pureTense ||
				lastVerbTense->lastVerb<0 ||
				abs(whereVerb-lastVerbTense->lastVerb)<200)
				break;
			// not delayed because the arguments are two verbs
			addRelations(where,m[whereVerb].getVerbME(where,4,lastNounNotFound,lastVerbNotFound),m[lastVerbTense->lastVerb].getVerbME(where,20,lastNounNotFound,lastVerbNotFound),VerbWithNext1MainVerbSameSubject+objectLastTense*2);
		}
		lastVerbTense=objects[subjectObject].lastVerbTenses;
		memmove(lastVerbTense+1,lastVerbTense,(VERB_HISTORY-1)*sizeof(lastVerbTense[0]));
		lastVerbTense[0].lastVerb=whereVerb;
		lastVerbTense[0].lastTense=pureTense;
		// must have a subject, though doesn't have to match.
		for (int objectLastTense=0; objectLastTense<VERB_HISTORY; objectLastTense++)
		{
			if (lastVerbTenses[objectLastTense].lastTense!=pureTense ||
				lastVerbTenses[objectLastTense].lastVerb<0 ||
				abs(whereVerb-lastVerbTenses[objectLastTense].lastVerb)<200)
				break;
			// not delayed because the arguments are two verbs
			addRelations(where,m[whereVerb].getVerbME(where,5,lastNounNotFound,lastVerbNotFound),m[lastVerbTenses[objectLastTense].lastVerb].getVerbME(where,6,lastNounNotFound,lastVerbNotFound),VerbWithNext1MainVerb+objectLastTense*2);
		}
		memmove(lastVerbTenses+1,lastVerbTenses,(VERB_HISTORY-1)*sizeof(lastVerbTenses[0]));
		lastVerbTenses[0].lastVerb=whereVerb;
		lastVerbTenses[0].lastTense=pureTense;
	}
}

void Source::getCompoundPositions(int where,vector <tTagLocation> &multipleObjectTagSet,vector < int > &objectPositions)
{ LFS
	objectPositions.clear();
	tIWMM w;
	for (int oTag=findOneTag(multipleObjectTagSet,L"MOBJECT",-1); oTag>=0; oTag=findOneTag(multipleObjectTagSet,L"MOBJECT",oTag)) 
	{
		int o,wo=-1,wob=-1,traceSource=-1;
		if (resolveTag(multipleObjectTagSet,oTag,o,wo,w) && multipleObjectTagSet[oTag].PEMAOffset<0 && 
				(wo=m[wob=multipleObjectTagSet[oTag].sourcePosition].principalWherePosition)>=0 && 
				(objectPositions.empty() || wo>objectPositions[objectPositions.size()-1]) &&
				o!=-1)
		{
			vector < vector <tTagLocation> > ndTagSets;
			ndTagSets.clear();
			// and medical man written all over him
			//   source: a hospital nurse ( not Whittington's one[tuppence,nurse] ) on one side of me[julius] , and a little black - bearded man[mr] with gold glasses , and medical man[mr] written all[all] over him
			// examine if this position is actually an RE_OBJECT by testing for the lack of a determiner
			// this phrase must be preceded by a comma (because otherwise it cannot be an RE_OBJECT)
			if (wob<=1 || (m[wob-1].word->first!=L"," && m[wob-2].word->first!=L",") ||
					startCollectTagsFromTag(true,nounDeterminerTagSet,multipleObjectTagSet[oTag],ndTagSets,-1,true, true, L"compound position - noun determiner")<=0 ||
					findOneTag(ndTagSets[0],L"DET",-1)>=0 ||
					!evaluateNounDeterminer(ndTagSets[0],true,traceSource,wob,wob+multipleObjectTagSet[oTag].len,-1))
			{
				objectPositions.push_back(wo);
				//if (objects[o].neuter)
				//	allGendered=false;
				//if (objects[o].male || objects[o].female)
				//	allNeuter=false;
			}
			else if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%d:compound chain rejected %d-%d position (missing determiner, possible RE_OBJECT).",where,wob,wob+multipleObjectTagSet[oTag].len);
		}
		else if (wo>=0 && debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:compound chain rejected %d-%d position (missing object pwp=%d, o=%d).",where,wob,wob+multipleObjectTagSet[oTag].len,wo,o);
	}
}

#define MAX_COMBINANT_SCORE 100000
void Source::markMultipleObjects(int where)
{
	LFS
  vector <WordMatch>::iterator im=m.begin()+where;
	patternMatchArray::tPatternMatch *pma=im->pma.content;
	for (unsigned PMAElement=0; PMAElement<im->pma.count; PMAElement++,pma++)
	{
		// link together multiple nouns.  If lists overlap, prefer the lists that have more items, and if they contain the same # of items, prefer those that having
		//   gender-matched objects (all neuter, or all gendered).  If both are gender matched, prefer those that are less spaced out.
		if (patterns[pma->getPattern()]->hasTag(MNOUN_TAG))
		{
			bool allNeuter=true,allGendered=true,mixedGender,nonCombinant,nameGenderUncertainty=false;
			vector < vector <tTagLocation> > mobjectTagSets,ndTagSets;
			vector < int > objectPositions;
			if (startCollectTags(true,mobjectTagSet,where,pma->pemaByPatternEnd,mobjectTagSets,true,false,L"multiobjects")>0)
				for (unsigned int J=0; J<mobjectTagSets.size(); J++)
				{
					int o,wo,wob=0,traceSource=-1,numNumberObjects=0,numPluralObjects=0,numVerbObjects=0;
					objectPositions.clear();
					tIWMM w;
					wstring objectStr;
					nameGenderUncertainty=false;
					for (int oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",-1); oTag>=0; oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",oTag)) 
					{
						wo=-1;
						if (resolveTag(mobjectTagSets[J],oTag,o,wo,w) && mobjectTagSets[J][oTag].PEMAOffset<0 && 
								(wo=m[wob=mobjectTagSets[J][oTag].sourcePosition].principalWherePosition)>=0 && 
								(objectPositions.empty() || wo>objectPositions[objectPositions.size()-1]) &&
								o!=-1)
						{
							ndTagSets.clear();
							// and medical man written all over him
							//   source: a hospital nurse ( not Whittington's one[tuppence,nurse] ) on one side of me[julius] , and a little black - bearded man[mr] with gold glasses , and medical man[mr] written all[all] over him
							// examine if this position is actually an RE_OBJECT by testing for the lack of a determiner
							// this phrase must be preceded by a comma (because otherwise it cannot be an RE_OBJECT)
							if (wob<=1 || (m[wob-1].word->first!=L"," && m[wob-2].word->first!=L",") ||
								  startCollectTagsFromTag(true,nounDeterminerTagSet,mobjectTagSets[J][oTag],ndTagSets,-1,true, true, L"multiple object - noun determiner")<=0 ||
								  findOneTag(ndTagSets[0],L"DET",-1)>=0 ||
									!evaluateNounDeterminer(ndTagSets[0],true,traceSource,wob,wob+mobjectTagSets[J][oTag].len,-1))
							{
								objectPositions.push_back(wo);
								if (objects[o].objectClass==NAME_OBJECT_CLASS)
								{
									if (objects[o].male ^ objects[o].female)
										allNeuter=false;
									else
										nameGenderUncertainty=true;
								}
								else
								{
									if (objects[o].neuter)
										allGendered=false;
									if (objects[o].male || objects[o].female)
										allNeuter=false;
								}
								objectStr+=L" "+m[wo].word->first;
								if (m[wo].endObjectPosition-m[wo].beginObjectPosition==1 && m[wo].queryWinnerForm(NUMBER_FORM_NUM)>=0)
									numNumberObjects++;
								if ((m[wo].word->second.inflectionFlags&PLURAL)==PLURAL)
									numPluralObjects++;
								if (m[wo].queryWinnerForm(verbForm)>=0)
									numVerbObjects++;
							}
							else if (debugTrace.traceRole)
								lplog(LOG_ROLE,L"%d:compound chain rejected %d-%d position (missing determiner, possible RE_OBJECT).",where,wob,wob+mobjectTagSets[J][oTag].len);
						}
						else if (wo>=0 && debugTrace.traceRole)
							lplog(LOG_ROLE,L"%d:compound chain rejected %d-%d position (missing object pwp=%d, o=%d).",where,wob,wob+mobjectTagSets[J][oTag].len,wo,o);
					}
					mixedGender=(!allGendered && !allNeuter) && !nameGenderUncertainty;
					nonCombinant=(numNumberObjects!=0 && numNumberObjects!=objectPositions.size()) || (numVerbObjects!=0 && numVerbObjects!=objectPositions.size());
					if (objectPositions.size()>1)
					{
						int maxPosition=-1,minPosition=-1;
						unsigned int spread;
						for (vector <int>::iterator to=objectPositions.begin(),toend=objectPositions.end(); to!=toend; to++)
						{
							if (maxPosition==-1 || *to>maxPosition)
								maxPosition=*to;
							if (minPosition==-1 || *to<minPosition)
								minPosition=*to;
						}
						nonCombinant|=(objectPositions.size()==2 && m[minPosition].endObjectPosition>=0 && m[m[minPosition].endObjectPosition].word->first!=L"and" && 
							            // m[minPosition].relPrep>=0 && m[m[minPosition].relPrep].getRelObject()==minPosition); relPrep/relObject are not set yet
													m[minPosition].beginObjectPosition>0 && m[m[minPosition].beginObjectPosition-1].queryWinnerForm(prepositionForm)>=0);
						spread=maxPosition-minPosition;
						wstring cstr,tmpstr;
						int combinantScore=10000*objectPositions.size(); cstr+=L"+OBJ_POS["+itos(combinantScore,tmpstr)+L"]";
						combinantScore+=(nonCombinant) ? -10000 : 0;      if (nonCombinant) cstr+=L"+NON_COM["+itos(-10000,tmpstr)+L"]";
						combinantScore+=(mixedGender) ? -500 : 0;        if (mixedGender) cstr+=L"+MIX_GEN["+itos(-500,tmpstr)+L"]";
						combinantScore-=spread;                          cstr+=L"+SPREAD["+itos(-(signed)spread,tmpstr)+L"]";
						wstring tt;
						if (debugTrace.traceRole)
						{
							tmpstr.clear();
							if (m[minPosition].beginObjectPosition>=0) minPosition=m[minPosition].beginObjectPosition;
							if (m[maxPosition].endObjectPosition>=0) maxPosition=m[maxPosition].endObjectPosition;
							for (int p=minPosition; p<maxPosition; p++)
							{
								for (unsigned int t2=0; t2<objectPositions.size(); t2++) 
									if (p==objectPositions[t2])
										tmpstr+=L"*";
									tmpstr+=m[p].word->first+L" ";
							}
						}
						// scan for former chains
						wstring formerCombinantStr,fcstr;
						int competingFormerChainBeginPosition=-1,formerCombinantScore=-MAX_COMBINANT_SCORE,fcs;
						for (int K=0; K<(signed)objectPositions.size(); K++)
							if ((getNumCompoundObjects(objectPositions[K],fcs,fcstr)) && fcs>formerCombinantScore)
							{
								formerCombinantStr=fcstr;
								formerCombinantScore=fcs;
								competingFormerChainBeginPosition=objectPositions[K];
							}
						bool subChain=compoundObjectSubChain(objectPositions);
						if (formerCombinantScore<combinantScore || subChain)
						{
							bool subset=true;
							for (int K=0; K<((signed)objectPositions.size())-1 && (subset=m[objectPositions[K]].nextCompoundPartObject==objectPositions[K+1]); K++);
							if (debugTrace.traceRole)
								lplog(LOG_ROLE,L"%d:compound chain %s (combinantScore=%d, subset=%s, subChain=%s %s) %s (formerCombinantScore=%d %s).",
							        where,tmpstr.c_str(),combinantScore,(subset) ? L"true":L"false",(subChain) ? L"true":L"false",cstr.c_str(),
												(subChain || subset) ? L"skipped":((formerCombinantScore>0) ? L"overwrote":L"written"),formerCombinantScore,formerCombinantStr.c_str());
							if (!subset && !subChain)
							{
								if (competingFormerChainBeginPosition>=0)
								{
									// erase former chain that is inferior
									vector <int> formerObjectPositions;
									formerObjectPositions.push_back(competingFormerChainBeginPosition);
									for (int chainCount=0,opw=competingFormerChainBeginPosition; m[opw].previousCompoundPartObject>=0 && chainCount<10; opw=m[opw].previousCompoundPartObject)
										formerObjectPositions.push_back(m[opw].previousCompoundPartObject);
									for (int chainCount=0,opw=competingFormerChainBeginPosition; m[opw].nextCompoundPartObject>=0 && chainCount<10; opw=m[opw].nextCompoundPartObject)
										formerObjectPositions.push_back(m[opw].nextCompoundPartObject);
									for (int K=0; K<((signed)formerObjectPositions.size()); K++)
										m[formerObjectPositions[K]].previousCompoundPartObject=m[formerObjectPositions[K]].nextCompoundPartObject=-1;
									if (debugTrace.traceRole)
									{
										wstring tmpstr2;
										for (unsigned int t=0; t<formerObjectPositions.size(); t++)
											tmpstr2+=itos(formerObjectPositions[t],tt)+L" ";
										lplog(LOG_ROLE,L"%d:cancelling former compound chain %s.",where,tmpstr2.c_str());
									}
								}
								bool andChainType=patterns[pma->getPattern()]->hasTag(MPLURAL_TAG);
								m[objectPositions[0]].previousCompoundPartObject=m[objectPositions.size()-1].nextCompoundPartObject=-1;
								for (int K=0; K<((signed)objectPositions.size())-1; K++)
									m[objectPositions[K]].nextCompoundPartObject=objectPositions[K+1];
								for (int K=1; K<((signed)objectPositions.size()); K++)
									m[objectPositions[K]].previousCompoundPartObject=objectPositions[K-1];
								for (int K=0; K<((signed)objectPositions.size()); K++)
									m[objectPositions[K]].andChainType=andChainType;
							}
						}
						else 
							if (debugTrace.traceRole)
								lplog(LOG_ROLE,L"%d:compound chain %s (combinantScore=%d, subChain=%s %s) rejected (formerCombinantScore=%d %s).",
									where,tmpstr.c_str(),combinantScore,(subChain) ? L"true":L"false",cstr.c_str(),formerCombinantScore,formerCombinantStr.c_str());
					}
					else if (debugTrace.traceRole)
						lplog(LOG_ROLE,L"%d:compound chain rejected.",where);
				}
		}
	}
}

void Source::evaluateSubjectRoleTag(int where,int which,vector <int> whereSubjects,int whereObject,int whereHObject,int whereVerb,int whereHVerb,vector <int> subjectObjects,int tsSense,
														bool ignoreSpeaker,bool isNot,bool isNonPast,bool isNonPresent,bool isId,bool subjectIsPleonastic,bool inPrimaryQuote,bool inSecondaryQuote,bool backwardsSubjects)
{ LFS
	// don't overwrite the subjects relationships with verbs and objects if they have already been established with other verbs and objects.
	wstring tmpstr,tmpstr2;
	int s=whereSubjects[which];
	if (whereVerb>=0)
	{
		m[whereVerb].hasVerbRelations=true;
		if (whereHObject<0)
		{
			// prefer marking the first subject
			if (m[whereVerb].relSubject<0 || m[s].previousCompoundPartObject!=m[whereVerb].relSubject)
				m[whereVerb].relSubject=s;
			// mark subject of infinitive verb & infinitive objects - but don't overwrite preferential objects of parent verbs with subject, except when object is not gendered.
			// He wanted her to skip school. (don't overwrite 'her' with 'He'
			int relTraceVerb=m[whereVerb].getRelVerb(),tmp;
			bool preferGenderedSubject=m[s].getObject()>=0 && relTraceVerb>=0 && m[relTraceVerb].relSubject>=0 && m[m[relTraceVerb].relSubject].getObject()>=0 &&
				  ((objects[m[s].getObject()].isAgent(true)) || m[s].word->first==L"who") && !isAgentObject(m[tmp=m[relTraceVerb].relSubject].getObject());
			if (debugTrace.traceRelations && relTraceVerb>=0 && m[relTraceVerb].relSubject>=0 && m[s].previousCompoundPartObject!=m[relTraceVerb].relSubject && m[relTraceVerb].relSubject!=s &&
					!preferGenderedSubject)
				lplog(LOG_RESOLUTION,L"%06d:avoided overwriting subject of infinitive@%d as %d when it was set to %d.",where,relTraceVerb,s,m[relTraceVerb].relSubject);
			while (relTraceVerb>=0 && (m[relTraceVerb].relSubject<0 || preferGenderedSubject))
			{
				m[relTraceVerb].relSubject=s;
				if (m[relTraceVerb].getRelObject()>=0)
				{
					m[m[relTraceVerb].getRelObject()].relSubject=s;
					if (m[m[relTraceVerb].getRelObject()].relNextObject>=0)
						m[m[m[relTraceVerb].getRelObject()].relNextObject].relSubject=s;
				}
				if (m[relTraceVerb].getRelVerb()<=relTraceVerb)
					break;
				relTraceVerb=m[relTraceVerb].getRelVerb();	
			}
		}
		else if (whereHVerb>=0)
		{
			// prefer marking the first subject
			if (m[whereHVerb].relSubject<0 || m[s].previousCompoundPartObject!=m[whereHVerb].relSubject)
				m[whereHVerb].relSubject=s;
		}
		/* rejected - too inaccurate
		if (subjectObjects[which]>=0 && (m[whereVerb].queryForm(thinkForm)>=0 || m[whereVerb].queryForm(internalStateForm)>=0) && objects[subjectObjects[which]].getSubType()>=0 &&
			((m[s].endObjectPosition-m[s].beginObjectPosition)<=1 || m[m[s].beginObjectPosition].word->first!=L"the"))
		{
			objects[subjectObjects[which]].resetSubType();
			objects[subjectObjects[which]].isNotAPlace=true; 
			if (t.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%d:Removing place designation (1) from subject %s.",where,objectString(subjectObjects[which],tmpstr,false).c_str());
		}
		*/
		if (m[whereVerb].queryForm(internalStateForm)>=0 && !inPrimaryQuote && !inSecondaryQuote && !(tsSense&VT_PASSIVE))
		{
			m[s].objectRole|=POV_OBJECT_ROLE; // used in determining point of view/observer status for speakerGroups
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%d:Set subject %s to pov role from internal state verb %s (tense=%s).",s,objectString(m[s].getObject(),tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),senseString(tmpstr2,tsSense).c_str());
		}
	}
	bool rolePreviouslySetAsSubject=m[s].getRelObject()!=-1;
	if (backwardsSubjects && (m[s].objectRole&FOCUS_EVALUATED))
	{
		if (m[where].word->first==L"to" && m[s].getRelVerb()>=0 && m[m[s].getRelVerb()].getRelVerb()<0)
			m[m[s].getRelVerb()].setRelVerb(whereVerb); // link relVerb with relVerb of infinitive phrase
		return;
	}
	if (whereHObject<0)
	{
		if (m[s].getRelObject()<0 && whereObject>=0)
			m[s].setRelObject(whereObject); // used for accumulatingAdjectives - overwritten with object # in identifyRelationships
		else if (m[s].getRelObject()<s && whereObject>s)
		{
			m[s].relInternalObject=m[s].getRelObject();
			m[s].setRelObject(whereObject);
		}
		else if (whereObject>=0 && m[s].getRelObject()>s && whereObject<s)
			m[s].relInternalObject=whereObject;
		if (s!=whereVerb)
		{
			int flags;
			if (m[s].getRelVerb()!=-1 && m[s].getRelVerb()!=whereVerb && (m[s].objectRole&OBJECT_ROLE) && m[s].getRelVerb()<s && whereVerb>s && 
				  (flags=(m[whereVerb].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE)))
			{
				if (debugTrace.traceRole)
					lplog(LOG_RESOLUTION,L"%06d:subject %d - verb (%d,%d) conflict [%s]",where,s,m[s].getRelVerb(),whereVerb,(flags&VERB_PRESENT_PARTICIPLE) ? L"infinitive":L"present");
				m[s].relInternalVerb=whereVerb;
			}
			else
				m[s].setRelVerb(whereVerb);
		}
	}
	else
	{
		m[s].setRelObject(whereHObject); // used for accumulatingAdjectives - overwritten with object # in identifyRelationships
		if (s!=whereHVerb)
			m[s].setRelVerb(whereHVerb);
	}
	m[s].relNextObject=((signed)whereSubjects.size()>which+1) ? whereSubjects[which+1] : ((which) ? whereSubjects[0] : -1);
	if (ignoreSpeaker)
		m[s].flags|=WordMatch::flagIgnoreAsSpeaker; // used in scanSpeaker
	if (tsSense&VT_EXTENDED)
		m[s].objectRole|=EXTENDED_OBJECT_ROLE;
	if (isNot)
		m[s].objectRole|=NOT_OBJECT_ROLE; // used in mergeFocus, for identifying speaker groups
	if (!inPrimaryQuote && (m[s].objectRole&NONPAST_OBJECT_ROLE) && !isNonPast)
	{
		m[s].objectRole&=~NONPAST_OBJECT_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Removed object %s nonpast (%s) role (SUBJ) [tag set origin=%d].",
				s,objectString(m[s].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str(),where);
	}
	bool inRelativeClause=(m[s].objectRole&(EXTENDED_ENCLOSING_ROLE|NONPAST_ENCLOSING_ROLE|NONPRESENT_ENCLOSING_ROLE|SENTENCE_IN_REL_ROLE|SENTENCE_IN_ALT_REL_ROLE))!=0;
	// if the verb is simply extended, it acts like a modifier
	//  another knock[comer] sent him[tommy] *scuttling* back to cover
	bool verbInPhraseIsPresentExtended=m[s].relInternalVerb>=0 && (m[m[s].relInternalVerb].quoteForwardLink&(VT_PRESENT|VT_EXTENDED))==(VT_PRESENT|VT_EXTENDED);
	if (isNonPast && (inPrimaryQuote || inSecondaryQuote || !(m[s].objectRole&FOCUS_EVALUATED)))
	{
		m[s].objectRole|=NONPAST_OBJECT_ROLE; // used in mergeFocus, for identifying speaker groups
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set subject %s to nonpast (%s) role (SUBJ) [tag set origin=%d].",
						s,objectString(m[s].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str(),where);
	}
	else if (isNonPast && !rolePreviouslySetAsSubject && !(m[s].objectRole&NONPAST_OBJECT_ROLE) && (m[s].objectRole&FOCUS_EVALUATED) && 
		      (m[s].objectRole&(MOVEMENT_PREP_OBJECT_ROLE|NON_MOVEMENT_PREP_OBJECT_ROLE))!=0 && inRelativeClause && !verbInPhraseIsPresentExtended)
	{
		lplog(LOG_ROLE,L"%d: nonpast of subject set because of object that is simple past where object is in a relative phrase.",s);
		m[s].objectRole|=NONPAST_OBJECT_ROLE; // used in mergeFocus, for identifying speaker groups
	}
	if (isNonPresent)
	{
		m[s].objectRole|=NONPRESENT_OBJECT_ROLE; // used in mergeFocus, for identifying speaker groups
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set subject %s to nonpresent (%s) role (SUBJ) [tag set origin=%d].",
						s,objectString(m[s].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str(),where);
	}
	if (isId)
	{
		m[s].objectRole|=IS_OBJECT_ROLE; // used in resolveMetaGroupObject
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set subject %s to \"is\" object role.",s,objectString(m[s].getObject(),tmpstr,true).c_str());
	}
	else if (((m[s].objectRole&(IS_OBJECT_ROLE|OBJECT_ROLE))==(IS_OBJECT_ROLE|OBJECT_ROLE)) && m[s].relSubject<0)
	{
		m[s].objectRole&=~IS_OBJECT_ROLE; 
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Reset subject %s to REMOVE \"is\" object role.",s,objectString(m[s].getObject(),tmpstr,true).c_str());
	}
	if (subjectIsPleonastic)
	{
		m[s].objectRole|=SUBJECT_PLEONASTIC_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set subject %s to subject pleonastic role.",s,objectString(m[s].getObject(),tmpstr,true).c_str());
	}
	m[s].objectRole|=FOCUS_EVALUATED;
}

// return true if we can continue with the scan backwards for subject.
bool Source::skipQuote(int &where)
{ LFS
	if (m[where].word->first!=L"”") return true;
	int maxEnd,maxLen,quoteCheck=queryPattern(where,L"__NOUN",maxEnd);
	if (quoteCheck==-1 || where<(maxLen=pema[quoteCheck].end-pema[quoteCheck].begin) || m[where-maxLen+1].word->first!=L"“") return false;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%d:Skipping quoted string (%d-%d) when scanning for subjects backwards.",where,where-maxLen+1,where);
	where-=maxLen; // go past the quote
	return true;
}

void Source::scanForSubjectsBackwardsInSentence(int where,int whereVerb,bool isId,bool &objectAsSubject,bool &subjectIsPleonastic,vector <tIWMM> &subjectWords,
																								vector <int> &subjectObjects,vector <int> &whereSubjects,int tsSense,bool &multiSubject,bool preferInfinitive)
{ LFS
	int I=where,maxEnd=-1,PEMAOffset,MSTechnique=-1,saveBeginMST=-1;
	// if where matches _MSTAIL and find _MS1, find the beginning and see if SUBJECT_ROLE.
	if ((PEMAOffset=queryPattern(where,L"_MS1",maxEnd))>=0 && pema[PEMAOffset].isChildPattern() && patterns[pema[PEMAOffset].getChildPattern()]->name==L"_MSTAIL" && 
		  (m[pema[PEMAOffset].begin+where].objectRole&SUBJECT_ROLE) && m[saveBeginMST=MSTechnique=pema[PEMAOffset].begin+where].principalWherePosition>=0)
		MSTechnique=m[MSTechnique].principalWherePosition;
	// designatedAsInternalSubject in the following example is 'him' for the search of the subject of the verb 'rap'
	// From the shelter of the doorway ESTABhe[tommy] watched him[boris] EXITgo up the steps of a particularly evil - looking house and MOVErap sharply , with a peculiar rhythm , on the door
	int whereDesignatedAsInternalSubject=-1;
	while (I>0 && skipQuote(I) && !isEOS(I) && m[I].word->first!=L"’" && m[I].word!=Words.sectionWord && m[I].word->first!=L":" && m[I].word->first!=L"--" && 
	    (!(m[I].objectRole&SUBJECT_ROLE) || (m[I].objectRole&PASSIVE_SUBJECT_ROLE) || m[I].getObject()==UNKNOWN_OBJECT) && I!=whereDesignatedAsInternalSubject) 
	{
		if (m[I].verbSense>=0 && m[I].relSubject>=0 && !(m[m[I].relSubject].objectRole&SUBJECT_ROLE) && !(m[m[I].relSubject].objectRole&PASSIVE_SUBJECT_ROLE))
			whereDesignatedAsInternalSubject=m[I].relSubject;
		I--;
	}
	bool searchValid=I>=0 && ((m[I].objectRole&SUBJECT_ROLE) && !(m[I].objectRole&PASSIVE_SUBJECT_ROLE) && m[I].getObject()!=UNKNOWN_OBJECT) || I==whereDesignatedAsInternalSubject;
	if (searchValid || MSTechnique>=0)
	{
		if (!searchValid) 
			I=MSTechnique;
		// That man[danvers] , Danvers , was shadowed on the way over , wasn't he[danvers] ? 
		if (I>=0 && (m[I].flags&WordMatch::flagInQuestion) && (m[where].pma.queryPattern(L"_Q1")>=0 || m[where].pma.queryPattern(L"_Q2")>=0)) return;
		bool infinitiveObjectOfPrep=(m[whereVerb].objectRole&(PREP_OBJECT_ROLE|OBJECT_ROLE)) && (m[whereVerb].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE);
		if (infinitiveObjectOfPrep && whereVerb>2 && m[whereVerb-1].queryWinnerForm(possessiveDeterminerForm)>=0)
			I=whereVerb-1;
		// he found Albert discharging his professional duties and introduced [himself]
		else if (I>=0 && m[I].getRelVerb()>=0 && (m[m[I].getRelVerb()].quoteForwardLink!=tsSense || m[I].relInternalVerb>=0) &&
			       (whereDesignatedAsInternalSubject!=I || tsSense!=VT_PRESENT))
		{
			// if there is another subject, that is not the same subject, return.
			int J=I-1,element;
			while (J>=0 && !isEOS(J) && m[J].word->first!=L"”" && m[J].word->first!=L"’" && m[J].word!=Words.sectionWord && m[J].word->first!=L":" && m[J].queryWinnerForm(coordinatorForm)<0 && 
						(!(m[J].objectRole&SUBJECT_ROLE) || m[J].getObject()==UNKNOWN_OBJECT || (m[J].flags&WordMatch::flagAdjectivalObject))) J--;
			if (multiSubject=(J>=0 && m[J].objectRole&SUBJECT_ROLE) && m[J].getObject()!=UNKNOWN_OBJECT && m[J].getObject()!=m[I].getObject() && m[J].word!=m[I].word && m[J].pma.queryPatternDiff(L"__S1",L"5")==-1) 
			{
				if (m[I].relInternalVerb>=0 && (m[m[I].relInternalVerb].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE) &&
					  m[J].getRelVerb()>=0 && m[m[J].getRelVerb()].quoteForwardLink==tsSense)
				{
					if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:object infinitive rejection of %d in favor of subject=%d whereVerb=%d?",where,I,J,whereVerb); 
					I=J;
				}
				else if (MSTechnique<0) // too inconsistent
				{
					if (!infinitiveObjectOfPrep || (infinitiveObjectOfPrep && (m[I].getObject()<0 || !objects[m[I].getObject()].isAgent(true))))
					{
						if (!preferInfinitive && I!=whereDesignatedAsInternalSubject) return;
						if (I==whereDesignatedAsInternalSubject && m[J].getObject()>=0 && !objects[m[J].getObject()].neuter &&
							  m[J].getRelVerb()>=0 && m[I].getRelVerb()>=0 && 
								(((m[whereVerb].verbSense&VT_TENSE_MASK)==(m[m[J].getRelVerb()].verbSense&VT_TENSE_MASK)) || 
								 ((m[whereVerb].verbSense&VT_TENSE_MASK)!=(m[m[I].getRelVerb()].verbSense&VT_TENSE_MASK))))
						{
							int JVerbTense=m[m[J].getRelVerb()].verbSense;
							int IVerbTense=m[m[I].getRelVerb()].verbSense;
							wstring tmpstr1,tmpstr2,tmpstr3;
							if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d (tense=%s) overrides %d (tense=%s) as a subject of verb %d (tense=%s) (whereDesignatedAsInternalSubject)?",
										J,senseString(tmpstr1,JVerbTense).c_str(),I,senseString(tmpstr2,IVerbTense).c_str(),whereVerb,senseString(tmpstr3,m[whereVerb].verbSense).c_str()); 
							I=J;
						}
						int K=-1,originalI=I;
						if (m[I].getRelVerb()<0 || (m[m[I].getRelVerb()].getRelVerb()<0 && !(m[m[I].getRelVerb()].flags&WordMatch::flagInInfinitivePhrase)))
						{
							if (m[J].getRelVerb()>=0 && m[m[J].getRelVerb()].getRelVerb()>=0) I=J;
							else
							{
								// try really really hard to find that infinitive subject
								K=J-1;
								while (K>=0 && !isEOS(J) && m[K].word->first!=L"”" && m[K].word->first!=L"’" && m[K].word!=Words.sectionWord && m[K].word->first!=L":" && m[K].queryWinnerForm(coordinatorForm)<0 && 
											(!(m[K].objectRole&(SUBJECT_ROLE|PREP_OBJECT_ROLE)) || m[K].getObject()==UNKNOWN_OBJECT || (m[K].flags&WordMatch::flagAdjectivalObject))) K--;
								if (K>=0 && m[K].getRelVerb()>=0 && m[m[K].getRelVerb()].getRelVerb()>=0) I=K;
								else return;
							}
						}
						if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:long range infinitive subject assignment subject=%d whereVerb=%d (I=%d J=%d K=%d)?",where,I,whereVerb,originalI,J,K); 
					}
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:DIFF subject=%d whereVerb=%d?",where,I,whereVerb); 
				}
				// I and J are within the same subject, but J is a time that should not be considered a subject
				// the following morning the indefatigable Albert ...
				else if ((element=m[m[J].beginObjectPosition].pma.queryPatternDiff(L"__NOUN",L"5"))!=-1 && 
					  I>J && I<m[J].beginObjectPosition+m[m[J].beginObjectPosition].pma[element&~matchElement::patternFlag].len &&
						(m[J].word->second.timeFlags&T_UNIT)!=0)
				{
					if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Prefer later compound subject@%d for verb @%d if earlier co-subject@%d is a time.",where,I,whereVerb,J); 
				}
				else if (m[saveBeginMST].pma.queryPatternDiff(L"__S1",L"5")==-1)
					I=MSTechnique;
			}
		}
		if (I<0)
			return;
		// subject must be gendered if infinitive
		if (infinitiveObjectOfPrep && (m[I].getObject()<0 || !objects[m[I].getObject()].isAgent(true)))
			return;
		if ((m[I].objectRole&(PREP_OBJECT_ROLE|MOVEMENT_PREP_OBJECT_ROLE|NON_MOVEMENT_PREP_OBJECT_ROLE)) && I!=whereDesignatedAsInternalSubject) return;
		tIWMM subjectWord=m[I].word;
		subjectWords.push_back(subjectWord);
		subjectObjects.push_back(m[I].getObject());
		whereSubjects.push_back(I);
		objectAsSubject=false;
		if (isId && (isPleonastic(subjectWord) || isPleonastic(I)))
		{
			m[I].flags|=WordMatch::flagObjectPleonastic; // it was a good day.  What was New England?
			subjectIsPleonastic=true;
		}
		wstring tmpstr;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Found previous subject %d:%s.",where,I,objectString(m[I].getObject(),tmpstr,true).c_str());
	}
	/*
	// VERB is prep object (must also be -ing) - main SUBJECT, if gendered, is SUBJECT, otherwise SUBJECT is NULL
	if (whereSubjects.empty() && (m[whereVerb].objectRole&PREP_OBJECT_ROLE) && (m[whereVerb].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE))
	{
		//  whereSubjects.size() && objects[m[whereSubjects[0]].getObject()].isAgent(true))
		while (I>=0 && !isEOS(I) && m[I].word->first!=L"”" && m[I].word->first!=L"’" && m[I].word!=Words.sectionWord && m[I].word->first!=L":" && 
	    (!(m[I].objectRole&SUBJECT_ROLE) || (m[I].objectRole&PASSIVE_SUBJECT_ROLE) || m[I].getObject()==UNKNOWN_OBJECT)) I--;
	}
	*/
}

void Source::discoverSubjects(int where,vector <tTagLocation> &tagSet,int subjectTag,bool isId,bool &objectAsSubject,bool &subjectIsPleonastic,vector <tIWMM> &subjectWords,vector <int> &subjectObjects,vector <int> &whereSubjects)
{ LFS
	tIWMM subjectWord=wNULL;
	int subjectObject,whereSubject=-1,nextTag=-1,whereObject=-1,mnounTag;

	if ((mnounTag=findTagConstrained(tagSet,L"MNOUN",nextTag,tagSet[subjectTag]))>=0)
	{	
		whereObject=-1;
		vector < vector <tTagLocation> > mobjectTagSets;
		if (startCollectTagsFromTag(true,mobjectTagSet,tagSet[mnounTag],mobjectTagSets,false,true, true, L"discover subjects - compound objects")>0)
		{
			for (unsigned int J=0; J<mobjectTagSets.size(); J++)
			{
				//if (t.traceSpeakerResolution)
				//	::printTagSet(L"MOBJECT",J,mobjectTagSets[J]);
				if (mobjectTagSets[J].empty()) continue; // shouldn't happen - but does
				for (int oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",-1); oTag>=0; oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",oTag)) 
				{
					whereSubject=m[mobjectTagSets[J][oTag].sourcePosition].principalWherePosition;
					if (whereSubject<0) whereSubject=tagSet[subjectTag].sourcePosition;
					if (resolveTag(mobjectTagSets[J],oTag,subjectObject,whereSubject,subjectWord) && isId && (isPleonastic(subjectWord) || isPleonastic(whereSubject)))
					{
						m[whereSubject].flags|=WordMatch::flagObjectPleonastic; // it was a good day.  What was New England?
						subjectIsPleonastic=true;
					}
					// if subject is a time // three minutes later came another
					objectAsSubject=(!isId && subjectWord!=wNULL && ((subjectWord->second.timeFlags&T_UNIT)!=0 || subjectWord==Words.TIME));
					// with him was the evil-looking Number 14.
					objectAsSubject|=whereSubject>0 && m[whereSubject-1].queryWinnerForm(prepositionForm)>=0; 
					if (whereSubject>=0 && find(whereSubjects.begin(),whereSubjects.end(),whereSubject)==whereSubjects.end())
					{
						subjectWords.push_back(subjectWord);
						subjectObjects.push_back(subjectObject);
						whereSubjects.push_back(whereSubject);
					}
				}
			}
		}
	}
	else 
	{
		whereSubject=tagSet[subjectTag].sourcePosition;
		if (m[whereSubject].principalWherePosition>=0)
			whereSubject=m[whereSubject].principalWherePosition;
		//  [So Tuppence] decided to walk to the end of the street - so is not part of the object, yet is the first element of the tag.   Skip.
		if (whereSubject<0 && m[tagSet[subjectTag].sourcePosition].getObject()<0 && tagSet[subjectTag].len>1 && 
			  m[tagSet[subjectTag].sourcePosition+1].getObject()>=0) 
			whereSubject=tagSet[subjectTag].sourcePosition+1;
		if (whereSubject<0) whereSubject=tagSet[subjectTag].sourcePosition;
		if (resolveTag(tagSet,subjectTag,subjectObject,whereSubject,subjectWord) && isId && (isPleonastic(subjectWord) || isPleonastic(whereSubject)))
		{
			m[whereSubject].flags|=WordMatch::flagObjectPleonastic; // it was a good day.  What was New England?
			subjectIsPleonastic=true;
		}
		// The following morning the indefatigable Albert , having cemented an alliance with the greengrocer's boy[tommy,albert] 
		if (subjectWord!=wNULL && ((subjectWord->second.timeFlags&T_UNIT)!=0 || subjectWord==Words.TIME) && m[where].pma.queryPatternDiff(L"__NOUN",L"5")!=-1 && 
			  ((signed)tagSet[subjectTag].sourcePosition+tagSet[subjectTag].len>whereSubject) && m[whereSubject+1].principalWherePosition>=0 && 
				m[m[whereSubject+1].principalWherePosition].endObjectPosition<=(signed)(tagSet[subjectTag].sourcePosition+tagSet[subjectTag].len))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:introductory time expression@%d skipped infavor of actual subject@%d.",where,whereSubject,m[whereSubject+1].principalWherePosition);
			whereSubject=m[whereSubject+1].principalWherePosition;
			subjectObject=m[whereSubject].getObject();
			subjectWord=m[whereSubject].word;
		}
		// if subject is a time // three minutes later came another
		objectAsSubject=(!isId && subjectWord!=wNULL && ((subjectWord->second.timeFlags&T_UNIT)!=0 || subjectWord==Words.TIME));
		// with him was the evil-looking Number 14.
		objectAsSubject|=whereSubject>0 && m[whereSubject-1].queryWinnerForm(prepositionForm)>=0; 
		if (subjectTag>=0 && subjectWord!=wNULL && whereSubject>=0) 
		{
			subjectWords.push_back(subjectWord);
			subjectObjects.push_back(subjectObject);
			whereSubjects.push_back(whereSubject);
		}
	}
}

// each preposition points to the next preposition by relPrep.
// each prep points to its object by relObject.
// each prep points to the verb in its master sentence (or sentence fragment) by relVerb.
// each object points to its preposition by relPrep.
// if a preposition directly follows another object of a preposition, the object of the preposition points to the previous object by relNextObject.
// an infinitive verb points to its owning verb with previousCompoundPartObject
void Source::markPrepositionalObjects(int where,int whereVerb,bool flagInInfinitivePhrase,bool subjectIsPleonastic,bool objectAsSubject,bool isId,bool inPrimaryQuote,bool inSecondaryQuote,bool isNot,bool isNonPast,bool isNonPresent,bool noObjects,bool delayedReceiver,int tsSense,vector <tTagLocation> &tagSet)
{ LFS
	wstring tmpstr;
	for (int prepTag=findOneTag(tagSet,L"PREP",-1); prepTag>=0; prepTag=findOneTag(tagSet,L"PREP",prepTag))
	{
		vector < vector <tTagLocation> > tagSets;
		if (startCollectTagsFromTag(debugTrace.traceRelations,prepTagSet,tagSet[prepTag],tagSets,-1,true, false, L"mark prepositional objects")>0)
			for (unsigned int K=0; K<tagSets.size(); K++)
			{
				if (debugTrace.traceRole)
					::printTagSet(LOG_ROLE,L"PREP",K,tagSets[K]);
				tIWMM subObjectWord=wNULL;
				bool isRelativeClauseObject=false;
				int subobject=-1,poTag=findOneTag(tagSets[K],L"PREPOBJECT",-1),pTag=findOneTag(tagSets[K],L"P",-1),wpo=-1,wp=(pTag>=0) ? tagSets[K][pTag].sourcePosition : -1;
				if ((poTag>=0 && resolveTag(tagSets[K],poTag,subobject,wpo,subObjectWord) && m[wpo].endObjectPosition>=0) ||
						(isRelativeClauseObject=findOneTag(tagSets[K],L"REL",-1)>=0 && m[wpo=wp+1].queryWinnerForm(relativizerForm)>=0) ||
						// with additional info as prep object, force object to exist
						(poTag>=0 && forcePrepObject(tagSets[K],poTag,subobject,wpo,subObjectWord) &&	(wpo=m[tagSets[K][poTag].sourcePosition].principalWherePosition)>=0))
				{
					if (pTag>=0)
					{
						m[wp].setRelObject(wpo);
						setRelPrep(wpo,wp,3,PREP_OBJECT_SET,whereVerb);
						m[wp].setRelVerb(whereVerb);
						if (m[wp].word->second.flags&tFI::prepMoveType)
							m[wpo].objectRole|=MOVEMENT_PREP_OBJECT_ROLE;
						else
							m[wpo].objectRole|=NON_MOVEMENT_PREP_OBJECT_ROLE;
						if (m[wp].word->first==L"of" || m[wp].word->first==L"for" || m[wp].word->first==L"from")
							m[wpo].objectRole|=NO_PP_PREP_ROLE;
						int whereLastPrep=whereVerb,prepLoop=0;
						while (m[whereLastPrep].relPrep>=0 && m[whereLastPrep].relPrep!=wp)
						{
							if (prepLoop++>20)
							{
								lplog(LOG_ERROR,L"%06d:Prep loop occurred (1) %s.",whereLastPrep,loopString(whereLastPrep,tmpstr));
								break;
							}
							whereLastPrep=m[whereLastPrep].relPrep;
						}
						if (m[whereLastPrep].getRelObject()>=0 && m[m[whereLastPrep].getRelObject()].endObjectPosition==wp)
							m[wpo].relNextObject=m[whereLastPrep].getRelObject();
						if (m[whereLastPrep].relPrep<0 && m[wp].relPrep<0)
							setRelPrep(whereLastPrep,wp,4,PREP_PREP_SET,whereVerb);
						// 'to the Ritz' would be bound to the outermost verb (is), and not the inner verb (strolled)
						// it is time ESTABI[tommy] strolled round to the Ritz
						// about 21 valid matches per 100000 words
						int maxEnd;
						if (wp>0 && wp-1!=whereVerb && m[wp-1].relPrep<0 && queryPattern(wp-1,L"__ALLVERB",maxEnd)>=0)
							setRelPrep(wp-1,wp,5,PREP_VERB_SET,whereVerb);
					}
					if (isNot)
						m[wpo].objectRole|=NOT_OBJECT_ROLE; // used in mergeFocus, for identifying speaker groups
					if (isNonPast && (inPrimaryQuote || inSecondaryQuote || !(m[wpo].objectRole&FOCUS_EVALUATED)))
					{
						wstring tmpstr2;
						m[wpo].objectRole|=NONPAST_OBJECT_ROLE; // used in mergeFocus, for identifying speaker groups
						if (debugTrace.traceRole)
							lplog(LOG_ROLE,L"%d:Set subject %s to nonpast (%s) role (PREP) [tag set origin=%d].",
										wpo,objectString(m[wpo].getObject(),tmpstr,false).c_str(),senseString(tmpstr2,tsSense).c_str(),where);
					}
					if (isNonPresent)
					{
						wstring tmpstr2;
						m[wpo].objectRole|=NONPRESENT_OBJECT_ROLE; // used in mergeFocus, for identifying speaker groups
						if (debugTrace.traceRole)
							lplog(LOG_ROLE,L"%d:Set subject %s to nonpresent (%s) role (PREP) [tag set origin=%d].",
										wpo,objectString(m[wpo].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str(),where);
					}
					m[wpo].objectRole|=FOCUS_EVALUATED;
					if (delayedReceiver) m[wpo].objectRole|=DELAYED_RECEIVER_ROLE;
					if ((tsSense&VT_PASSIVE) && pTag>=0 && m[wp].word->first==L"by")
					{
						// preposition may have a GNOUN object - that peace had been effected by [following their counsels]
						m[wpo].objectRole|=SUBJECT_ROLE|PASSIVE_SUBJECT_ROLE|FOCUS_EVALUATED;
						// taken out because resolveToClass must be delayed because of speaker resolution - also passive verb usage may blur relation results
						//addRelations(where,resolveToClass(wpo),m[whereVerb].getVerbME(),(m[whereVerb].quoteForwardLink&VT_NEGATION) ? SubjectWordWithNotVerb:SubjectWordWithVerb,nrr);
						if (debugTrace.traceRole)
						{
							wstring tmpstr2,tmpstr3;
							lplog(LOG_ROLE,L"%d:Set passive (%s) object %s to SUBJECT role (%s).",
								wpo,senseString(tmpstr2,tsSense).c_str(),objectString(subobject,tmpstr,true).c_str(),m[wpo].roleString(tmpstr3).c_str());
						}
					}
					else
					{
						addDelayedWordRelations(where,wp,wpo,PrepWithPWord);
					}
					__int64 or=m[wpo].objectRole;
					while ((wpo=m[wpo].nextCompoundPartObject)>=0)
						m[wpo].objectRole|=or;
					// find relative clauses that do not agree by gender with their subobjects.  If this is true,
					// the relative clause does not attach to the prepositions object but rather to the sentence containing
					// the prepositional phrase.
					if (subobject>=0)
					{
						int rel;
						if (((rel=findOneTag(tagSets[K],L"REL",-1))>=0 || (rel=findOneTag(tagSets[K],L"S_IN_REL",-1))>=0) && 
								!(m[tagSets[K][rel].sourcePosition].objectRole&FOCUS_EVALUATED) && objects[subobject].whereRelativeClause<0)
						{
							int begin=tagSets[K][rel].sourcePosition,end=begin+tagSets[K][rel].len;
							if (debugTrace.traceRole)
								lplog(LOG_ROLE,L"%d:Roles extended to relative clause %d-%d broken from prepositional phrase.",where,begin,end);
							bool inThinkSay=m[whereVerb].queryForm(thinkForm)>=0;
							for (int I=begin; I<end; I++)
								if (m[I].getObject()!=-1)
								{
									addRoleTagsAt(where,I,true,flagInInfinitivePhrase,subjectIsPleonastic,isNot,false,tsSense,isNonPast,isNonPresent,objectAsSubject,isId,inPrimaryQuote,inSecondaryQuote,L"MPO");
									if (inThinkSay) m[I].objectRole|=THINK_ENCLOSING_ROLE;
								}
						}
					}
				}
				markPrepositionalObjects(where,whereVerb,flagInInfinitivePhrase,subjectIsPleonastic,objectAsSubject,isId,inPrimaryQuote,inSecondaryQuote,isNot,isNonPast,isNonPresent,noObjects,delayedReceiver,tsSense,tagSets[K]);
			}
	}
}

void Source::addRoleTagsAt(int where,int I,bool inRelativeClause,bool withinInfinitivePhrase,bool subjectIsPleonastic,bool isNot,bool objectNot,int tsSense,bool isNonPast,bool isNonPresent,bool objectAsSubject,bool isId,bool inPrimaryQuote,bool inSecondaryQuote,wchar_t *fromWhere)
{ LFS
	wstring tmpstr,tmpstr2;
	if (isNot || objectNot)
		m[I].objectRole|=(inRelativeClause) ? NOT_ENCLOSING_ROLE : NOT_OBJECT_ROLE;
	if (tsSense&VT_EXTENDED)
		m[I].objectRole|=(inRelativeClause) ? EXTENDED_ENCLOSING_ROLE : EXTENDED_OBJECT_ROLE;
	if (!inPrimaryQuote && (m[I].objectRole&NONPAST_OBJECT_ROLE) && !isNonPast && !inRelativeClause)
	{
		m[I].objectRole&=~NONPAST_OBJECT_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Removed object %s nonpast (%s) role (%s) [tag set origin=%d].",
				I,objectString(m[I].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str(),fromWhere,where);
	}		 
	if (isNonPast && (inPrimaryQuote || inSecondaryQuote || !inRelativeClause || !(m[I].objectRole&FOCUS_EVALUATED))) 
	{
		m[I].objectRole|=(inRelativeClause) ? NONPAST_ENCLOSING_ROLE:NONPAST_OBJECT_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set object %s to nonpast (%s) role%s (%s) [tag set origin=%d].",
						I,objectString(m[I].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str(),(inRelativeClause) ? L" in relative clause":L"",fromWhere,where);
	}
	if (isNonPresent) // && !(tsSense&VT_VERB_CLAUSE))
	{
		m[I].objectRole|=(inRelativeClause) ? NONPRESENT_ENCLOSING_ROLE:NONPRESENT_OBJECT_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set object %s to nonpresent (%s) role%s (%s) [tag set origin=%d].",
				I,objectString(m[I].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str(),(inRelativeClause) ? L" in relative clause":L"",fromWhere,where);
	}
	if (withinInfinitivePhrase)
		m[I].flags|=WordMatch::flagInInfinitivePhrase;
	if (!inRelativeClause)
		m[I].objectRole|=FOCUS_EVALUATED;
	if ((tsSense&VT_POSSIBLE) && inRelativeClause)
		m[I].objectRole|=POSSIBLE_ENCLOSING_ROLE;
	if ((!inPrimaryQuote || !isNonPresent) && (inPrimaryQuote || inSecondaryQuote || !isNonPast)) 
	{
		if (objectAsSubject)
		{
			m[I].objectRole|=SUBJECT_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%d:Set object %s to subject role (objectAsSubject %s) [tag set origin=%d].",
					I,objectString(m[I].getObject(),tmpstr,true).c_str(),fromWhere,where);
		}
	}
	if (isId && !inRelativeClause)
	{
		m[I].objectRole|=IS_OBJECT_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set object %s to \"is\" object role (%s) [tag set origin=%d].",I,objectString(m[I].getObject(),tmpstr,true).c_str(),fromWhere,where);
	}
	if (subjectIsPleonastic && !inRelativeClause)
	{
		m[I].objectRole|=SUBJECT_PLEONASTIC_ROLE;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set object %s to subject pleonastic role (%s) [tag set origin=%d].",I,objectString(m[I].getObject(),tmpstr,true).c_str(),fromWhere,where);
	}
}

// an infinitive verb points to its owning verb with previousCompoundPartObject
// find any infinitive phrases
// cases:
// relation to immediately preceding verb:
//   I want to run.  (no object, infinitive phrase to verb)
//   I want you to take Mrs. Smith back. (single object, infinitive phrase)
//   I want Bill to remember to thank Mrs. Smith for taking us back today. (single object, multiple infinitive phrase and so on)
// relation to noun: I use language to suit the occasion.
// relation as subjectObject to main verb: An extra candle to give away is always a good idea.
int Source::processInternalInfinitivePhrase(int where,int whereVerb,int whereParentObject,int iverbTag,int firstFreePrep,vector <int> &futureBoundPrepositions,
																						bool inPrimaryQuote,bool inSecondaryQuote,bool &nextVerbInSeries,int &sense,
																						int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int begin,int end,int infpElement,vector <tTagLocation> &tagSet)
{ LFS
	int whereIVerb=-1,whereHVerb=-1,nextTag=-1,parentTagLen=-1;
	vector < vector <tTagLocation> > tagSets;
	if (iverbTag>=0)
	{
		startCollectTagsFromTag(debugTrace.traceRelations,iverbTagSet,tagSet[iverbTag],tagSets,-1,true, false, L"internal infinitive phrase");
		parentTagLen=tagSet[iverbTag].len;
	}
	else
	{
		startCollectTags(debugTrace.traceRelations,iverbTagSet,where,m[where].pma[infpElement&~matchElement::patternFlag].pemaByPatternEnd,tagSets,true,false,L"internal infinitive phrase");
		parentTagLen=m[where].pma[infpElement&~matchElement::patternFlag].len;
	}
	for (unsigned int K=0; K<tagSets.size(); K++)
	{
		if (debugTrace.traceRole)
			::printTagSet(LOG_ROLE,L"IVERB",K,tagSets[K]);
		int itoTag=findOneTag(tagSets[K],L"ITO",-1),itoWhere;
		// INFPSUB has ITO on a coordinator, so continue if that is true
		if (itoTag<0 || m[(itoWhere=tagSets[K][itoTag].sourcePosition)].word->first!=L"to") continue;
		nextTag=-1;
		int iverbInnerTag=findTag(tagSets[K],L"V_OBJECT",nextTag);
		if (nextTag>=0) iverbInnerTag=nextTag; // get last V_OBJECT
		nextTag=-1;
		if (iverbInnerTag<0)
			iverbInnerTag=findTag(tagSets[K],L"V_AGREE",nextTag);
		if (nextTag>=0) iverbInnerTag=nextTag; // get last V_AGREE
		if (iverbInnerTag>=0) 
			whereIVerb=tagSets[K][iverbInnerTag].sourcePosition;
		if (whereIVerb>=0)
		{
			m[whereIVerb].hasVerbRelations=true;
			m[whereIVerb].flags|=WordMatch::flagInInfinitivePhrase;
		}
		if (whereIVerb > whereLastVerb)
		{
			if (debugTrace.traceRelations)
				lplog(LOG_RESOLUTION, L"%d: set whereLastVerb to infinitive verb location %d (+1)", where, whereIVerb);
			whereLastVerb = whereIVerb + 1;
		}
		// I[whereParentObject] shall be[whereVerb] able to CONTACTmake[V_HOBJECT] Mrs . Vandemeyer[HOBJECT] tell[whereIVerb] me[james] 
		nextTag=-1;
		int hverbTagIndex=findTag(tagSets[K],L"V_HOBJECT",nextTag);
		if (hverbTagIndex>=0) 
		{
			whereHVerb=tagSets[K][hverbTagIndex].sourcePosition;
  		m[whereHVerb].hasVerbRelations=true;
		}
		int hObjectTag=findOneTag(tagSets[K],L"HOBJECT",-1);
		int whereHObject=(hObjectTag>=0) ? m[tagSets[K][hObjectTag].sourcePosition].principalWherePosition : -1;
		bool outsideQuoteTruth,inQuoteTruth;
		if (whereHObject>=0 && whereHVerb>=0)
		{
			if (whereParentObject>=0)
			{
				m[whereParentObject].relInternalVerb=whereHVerb;
				m[whereHVerb].relSubject=whereParentObject;
			}
			m[whereHObject].setRelVerb(whereIVerb);
			m[whereIVerb].relSubject=whereHObject;
			m[itoWhere].flags|=WordMatch::flagInInfinitivePhrase;
			if (whereVerb>=0)
			{
				m[whereVerb].setRelVerb(whereHVerb);
				if (m[whereHVerb].previousCompoundPartObject!=-1 && m[whereHVerb].previousCompoundPartObject<whereVerb)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:verb@%d having infinitive forward bare infinitive relVerb@%d is rejected in favor of closer parent verb@%d.",where,m[whereIVerb].previousCompoundPartObject,whereIVerb,whereVerb);
					m[m[whereHVerb].previousCompoundPartObject].setRelVerb(-1); // IVERB mistakenly attached to verb of previous parent
				}
				m[whereHVerb].previousCompoundPartObject=whereVerb;
			}
			evaluateAdditionalRoleTags(itoWhere,tagSets[K],parentTagLen,firstFreePrep,futureBoundPrepositions,inPrimaryQuote,inSecondaryQuote,outsideQuoteTruth,inQuoteTruth,true,true,nextVerbInSeries,sense,whereLastVerb,ambiguousSense,inQuotedString,inSectionHeader,begin,end);
			whereIVerb=whereHVerb;
			break;
		}
		if (whereIVerb>=0 && whereVerb>=0 && whereIVerb!=whereVerb)
		{
			m[whereVerb].setRelVerb(whereIVerb);
			if (m[whereIVerb].previousCompoundPartObject!=-1 && m[whereIVerb].previousCompoundPartObject<whereVerb)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:verb@%d having infinitive forward relVerb@%d is rejected in favor of closer parent verb@%d.",where,m[whereIVerb].previousCompoundPartObject,whereIVerb,whereVerb);
				m[m[whereIVerb].previousCompoundPartObject].setRelVerb(-1); // IVERB mistakenly attached to verb of previous parent
			}
			m[whereIVerb].previousCompoundPartObject=whereVerb;
			if (whereParentObject>=0)
			{
				m[whereParentObject].relInternalVerb=whereIVerb;
				m[whereIVerb].relSubject=whereParentObject;
			}
			m[itoWhere].flags|=WordMatch::flagInInfinitivePhrase;
			// not delayed because the arguments are two verbs
			addRelations(where,m[whereVerb].getVerbME(where,8,lastNounNotFound,lastVerbNotFound),m[whereIVerb].getVerbME(where,7,lastNounNotFound,lastVerbNotFound),VerbWithInfinitive);
			evaluateAdditionalRoleTags(itoWhere,tagSets[K],parentTagLen,firstFreePrep,futureBoundPrepositions,inPrimaryQuote,inSecondaryQuote,outsideQuoteTruth,inQuoteTruth,true,true,nextVerbInSeries,sense,whereLastVerb,ambiguousSense,inQuotedString,inSectionHeader,begin,end);
			break;
		}
		if (whereIVerb>=0 && whereParentObject>=0)
		{
			m[whereParentObject].relInternalVerb=whereIVerb;
			m[whereIVerb].relSubject=whereParentObject;
			m[itoWhere].flags|=WordMatch::flagInInfinitivePhrase;
			evaluateAdditionalRoleTags(itoWhere,tagSets[K],parentTagLen,firstFreePrep,futureBoundPrepositions,inPrimaryQuote,inSecondaryQuote,outsideQuoteTruth,inQuoteTruth,true,true,nextVerbInSeries,sense,whereLastVerb,ambiguousSense,inQuotedString,inSectionHeader,begin,end);
			break;
		}
	}
	return whereIVerb;
}

// if whereLastPrep==-1, return -1.
//   the first prep that contains the role, return.
//   if no prep that contains the role, return a prep that does not contain the rejectRole
int Source::findPrepRole(int whereLastPrep,int role,int rejectRole)
{ LFS
	int save=-1,prepLoop=0;
	while ((whereLastPrep=m[whereLastPrep].relPrep)>=0) 
	{
		if (m[whereLastPrep].objectRole&role) return whereLastPrep;
		if (!(m[whereLastPrep].objectRole&rejectRole)) save=whereLastPrep;
		if (prepLoop++>20)
		{
			wstring tmpstr;
			lplog(LOG_ERROR,L"%06d:Prep loop occurred (2) %s.",whereLastPrep,loopString(whereLastPrep,tmpstr));
			return -1;
		}
	}
	return save;
}

int Source::attachAdjectiveRelation(vector <tTagLocation> &tagSet,int whereObject)
{ LFS
	int begin=m[whereObject].beginObjectPosition,end=m[whereObject].endObjectPosition;
	tIWMM modifiedWord=resolveToClass(whereObject);
	int nextAdjectiveTag=-1,adjectiveTag=findTagConstrained(tagSet,L"ADJ",nextAdjectiveTag,begin,end);
	while (adjectiveTag>=0 && tagIsCertain(tagSet[adjectiveTag].sourcePosition))
	{
		tIWMM adjectiveWord=m[tagSet[adjectiveTag].sourcePosition].word;
		if (m[tagSet[adjectiveTag].sourcePosition].isPPN())
			adjectiveWord=Words.PPN;
		// not delayed because the arguments are not subject to enhanced resolution (not an object)
		addRelations(tagSet[adjectiveTag].sourcePosition,modifiedWord,adjectiveWord,WordWithAdjective);
		adjectiveTag=nextAdjectiveTag;
		nextAdjectiveTag=findTagConstrained(tagSet,L"ADJ",nextAdjectiveTag,begin,end);
	}
	return 0;
}

int Source::attachAdverbRelation(vector <tTagLocation> &tagSet,int verbTagIndex,tIWMM verbWord)
{ LFS
	int nextAdverbTag=-1,adverbTag=findTagConstrained(tagSet,L"ADV",nextAdverbTag,tagSet[verbTagIndex]);
	tIWMM adverbWord;
	if (adverbTag>=0 && tagIsCertain(tagSet[adverbTag].sourcePosition) && (adverbWord=resolveToClass(tagSet[adverbTag].sourcePosition))!=wNULL)
	{
		// not delayed because the arguments are not subject to enhanced resolution (not an object)
		addRelations(tagSet[adverbTag].sourcePosition,verbWord,adverbWord,VerbWithAdverb);
		if (nextAdverbTag>=0 && tagIsCertain(tagSet[adverbTag].sourcePosition) && (adverbWord=resolveToClass(tagSet[nextAdverbTag].sourcePosition))!=wNULL)
			addRelations(tagSet[nextAdverbTag].sourcePosition,verbWord,adverbWord,VerbWithAdverb);
	}
	return 0;
}

// mark OBJECT with NONPAST_OBJECT_ROLE if VERB is NOT a simple non-negative past tense (VT_PAST, VT_EXTENDED+VT_PAST, VT_PASSIVE+VT_PAST, VT_PASSIVE+VT_PAST+VT_EXTENDED)
// mark OBJECT with NONPRESENT_OBJECT_ROLE if VERB is NOT a simple non-negative present tense (VT_PRESENT, VT_EXTENDED+VT_PRESENT, VT_PASSIVE+VT_PRESENT, VT_PASSIVE+VT_PRESENT+VT_EXTENDED)
// mark OBJECT with IS_OBJECT_ROLE if an id verb and the SUBJECT is not "it"
// each preposition points to the next preposition by relPrep.
// each prep points to its object by relObject.
// each prep points to the verb in its master sentence (or sentence fragment) by relVerb.
// each object points to its preposition by relPrep.
// if a preposition directly follows another object of a preposition, the object of the preposition points to the previous object by relNextObject.
// an infinitive verb points to its owning verb with previousCompoundPartObject
// He watched him go up the steps of a particularly evil house
//		He - relVerb [watched] relObject [him] 
//		watched - relSubject [He] relVerb XX relObject [him] relInternalVerb [go]
//		him - relSubject [He] relVerb [watched] relObject XX 
//		go - relSubject [him] relVerb XX relObject [steps] relInternalVerb XX
//		steps - relSubject XX relVerb [go] relPrep [house]
//    house - relNextObject [steps]
bool Source::evaluateAdditionalRoleTags(int where,vector <tTagLocation> &tagSet,int len,int firstFreePrep,vector <int> &futureBoundPrepositions,
																				bool inPrimaryQuote,bool inSecondaryQuote,bool &outsideQuoteTruth,bool &inQuoteTruth,bool withinInfinitivePhrase,bool internalInfinitivePhrase,
																				bool &nextVerbInSeries,int &sense,int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int begin,int end)
{ LFS
	outsideQuoteTruth=!inPrimaryQuote && !inSecondaryQuote;
	inQuoteTruth=inPrimaryQuote;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%d outsideQuoteTruth=%s inQuoteTruth=%s inPrimaryQuote=%s inSecondaryQuote=%s",where,
			(outsideQuoteTruth) ? L"true":L"false",
			(inQuoteTruth) ? L"true":L"false",
			(inPrimaryQuote) ? L"true":L"false",
			(inSecondaryQuote) ? L"true":L"false");
	int nextTag=-1,verbTagIndex,tsSense,whereVerb=-1,notTag,whereHVerb=-1,hverbTagIndex;
	bool isId=false,isNot;
	if ((verbTagIndex=findOneTag(tagSet,L"VERB",-1))>=0) 
	{
		// I am not your enemy.
		// I will never be your enemy.
		// I am no enemy of yours.
		tsSense=getVerbTense(tagSet,verbTagIndex,isId);
		isNot=findTagConstrained(tagSet,L"not",nextTag,tagSet[verbTagIndex])>=0 || findTagConstrained(tagSet,L"never",nextTag,tagSet[verbTagIndex])>=0;
		//(directObjectTag>=0 && findTagConstrained(tagSet,L"no",nextTag,tagSet[directObjectTag])>=0) ||
		//(indirectObjectTag>=0 && findTagConstrained(tagSet,L"no",nextTag,tagSet[indirectObjectTag])>=0);
		nextTag=-1;
		whereVerb=findTagConstrained(tagSet,L"V_OBJECT",nextTag,tagSet[verbTagIndex]);
		if (nextTag>=0) whereVerb=nextTag; // get last V_OBJECT
		nextTag=-1;
		if (whereVerb<0)
			whereVerb=findTagConstrained(tagSet,L"V_AGREE",nextTag,tagSet[verbTagIndex]);
		if (nextTag>=0) whereVerb=nextTag; // get last V_AGREE
		if (whereVerb>=0) 
			whereVerb=tagSet[whereVerb].sourcePosition;
		// She made[V_HOBJECT] you[HOBJECT] read[V_OBJECT] a book[OBJECT]
		hverbTagIndex=findTagConstrained(tagSet,L"V_HOBJECT",nextTag,tagSet[verbTagIndex]);
		if (hverbTagIndex>=0) 
		{
			whereHVerb=tagSet[hverbTagIndex].sourcePosition;
			attachAdverbRelation(tagSet,hverbTagIndex,m[whereHVerb].getVerbME(where,9,lastNounNotFound,lastVerbNotFound));
  		m[whereHVerb].hasVerbRelations=true;
		}
		if (whereVerb<0)
			return false;
		if (whereVerb > whereLastVerb)
			whereLastVerb = whereVerb + 1;
		if (whereHVerb > whereLastVerb)
			whereLastVerb = whereHVerb + 1;
		attachAdverbRelation(tagSet,verbTagIndex,m[whereVerb].getVerbME(where,10,lastNounNotFound,lastVerbNotFound));
		for (int mverbTag=findOneTag(tagSet,L"MVERB",-1); mverbTag>=0; mverbTag=findOneTag(tagSet,L"MVERB",mverbTag))
		{
			vector < vector <tTagLocation> > mverbTagSets;
			if (startCollectTagsFromTag(true,verbObjectRelationTagSet,tagSet[mverbTag],mverbTagSets,false,true, true, L"additional role tags - compound verbs")>0)
			{
				for (unsigned int J=0; J<mverbTagSets.size(); J++)
				{
					if (debugTrace.traceRole)
						::printTagSet(LOG_ROLE,L"MVERB",J,mverbTagSets[J]);
					if (mverbTagSets[J].empty()) continue; // shouldn't happen - but does
					int mverbTagIndex,whereMVerb,whereHMVerb,lastWhereMVerb=whereVerb;
					if ((mverbTagIndex=findOneTag(mverbTagSets[J],L"VERB",-1))>=0) 
					{
						bool misId;
						int mtsSense=getVerbTense(mverbTagSets[J],mverbTagIndex,misId);
						isNot=findTagConstrained(mverbTagSets[J],L"not",nextTag,mverbTagSets[J][mverbTagIndex])>=0 || findTagConstrained(mverbTagSets[J],L"never",nextTag,mverbTagSets[J][mverbTagIndex])>=0;
						nextTag=-1;
						whereMVerb=findTagConstrained(mverbTagSets[J],L"V_OBJECT",nextTag,mverbTagSets[J][mverbTagIndex]);
						if (nextTag>=0) whereMVerb=nextTag; // get last V_OBJECT
						nextTag=-1;
						if (whereMVerb<0)
							whereMVerb=findTagConstrained(mverbTagSets[J],L"V_AGREE",nextTag,mverbTagSets[J][mverbTagIndex]);
						if (nextTag>=0) whereMVerb=nextTag; // get last V_AGREE
						if (whereMVerb>=0) 
							whereMVerb=mverbTagSets[J][whereMVerb].sourcePosition;
						if (whereMVerb > whereLastVerb)
							whereLastVerb = whereMVerb + 1;
						// She made[V_HOBJECT] you[HOBJECT] read[V_OBJECT] a book[OBJECT]
						whereHMVerb=findTagConstrained(mverbTagSets[J],L"V_HOBJECT",nextTag,mverbTagSets[J][mverbTagIndex]);
						isNot|=(notTag=findOneTag(mverbTagSets[J],L"not"  ,-1))>=0 && (mverbTagSets[J][notTag].sourcePosition==mverbTagSets[J][verbTagIndex].sourcePosition+mverbTagSets[J][verbTagIndex].len || mverbTagSets[J][notTag].sourcePosition==mverbTagSets[J][verbTagIndex].sourcePosition-1);
						isNot|=(notTag=findOneTag(mverbTagSets[J],L"never",-1))>=0 && (mverbTagSets[J][notTag].sourcePosition==mverbTagSets[J][verbTagIndex].sourcePosition+mverbTagSets[J][verbTagIndex].len || mverbTagSets[J][notTag].sourcePosition==mverbTagSets[J][verbTagIndex].sourcePosition-1);
						//bool isNonPast=((mtsSense!=VT_PAST && mtsSense!=VT_EXTENDED+ VT_PAST && mtsSense!=VT_PASSIVE+ VT_PAST && mtsSense!=VT_PASSIVE+ VT_PAST+VT_EXTENDED));
						//bool isNonPresent=((mtsSense!=VT_PRESENT && mtsSense!=VT_EXTENDED+ VT_PRESENT && mtsSense!=VT_PASSIVE+ VT_PRESENT && mtsSense!=VT_PASSIVE+ VT_PRESENT+VT_EXTENDED));
						// correction for VERBVERB I have come to you - ambiguous
						if (mtsSense==VT_PRESENT && whereVerb>0 && (m[whereVerb].word->second.inflectionFlags&VERB_PAST_PARTICIPLE)!=0 && m[whereVerb-1].word->first==L"have")
							mtsSense=VT_PAST;
						if (isPossible(whereMVerb)) mtsSense|=VT_POSSIBLE;
						if (whereMVerb>=0) 
						{
							m[whereMVerb].quoteForwardLink=mtsSense | ((isNot) ? VT_NEGATION:0);
							m[whereMVerb].hasVerbRelations=true;
						}
						if (whereHMVerb>=0) 
						{
							m[whereHMVerb].quoteForwardLink=mtsSense | ((isNot) ? VT_NEGATION:0);
							m[whereHMVerb].hasVerbRelations=true;
						}
						m[lastWhereMVerb].setRelVerb(whereMVerb);
						m[lastWhereMVerb].andChainType=true;
						lastWhereMVerb=whereMVerb;
					}
				}
			}
		}
	}
	else if (withinInfinitivePhrase)
	{
		tsSense=getVerbTense(tagSet,verbTagIndex,isId);
		isNot=findTag(tagSet,L"not",nextTag)>=0 || findTag(tagSet,L"never",nextTag)>=0;
		nextTag=-1;
		verbTagIndex=findTag(tagSet,L"V_OBJECT",nextTag);
		if (nextTag>=0) verbTagIndex=nextTag; // get last V_OBJECT
		nextTag=-1;
		if (verbTagIndex<0)
			verbTagIndex=findTag(tagSet,L"V_AGREE",nextTag);
		if (nextTag>=0) verbTagIndex=nextTag; // get last V_AGREE
		if (verbTagIndex>=0) 
			whereVerb=tagSet[verbTagIndex].sourcePosition;
		// She made[V_HOBJECT] you[HOBJECT] read[V_OBJECT] a book[OBJECT]
		hverbTagIndex=findTag(tagSet,L"V_HOBJECT",nextTag);
		if (hverbTagIndex>=0) 
		{
			whereHVerb=tagSet[hverbTagIndex].sourcePosition;
			attachAdverbRelation(tagSet,hverbTagIndex,m[whereHVerb].getVerbME(where,11,lastNounNotFound,lastVerbNotFound));
  		m[whereHVerb].hasVerbRelations=true;
		}
		if (whereVerb<0)
			return false;
		if (whereVerb > whereLastVerb)
			whereLastVerb = whereVerb + 1;
		attachAdverbRelation(tagSet,verbTagIndex,m[whereVerb].getVerbME(where,12,lastNounNotFound,lastVerbNotFound));
		nextTag=-1;
		// check for INFPSUB - pattern may match non-infinitive phrases
		int itoTag=findTag(tagSet,L"ITO",nextTag),sp=(itoTag>=0) ? tagSet[itoTag].sourcePosition : -1;
		if (withinInfinitivePhrase&=((sp>=0 && m[sp].word->first==L"to") || (sp>=1 && m[sp-1].flags&WordMatch::flagInInfinitivePhrase) || 
			  (sp>=2 && m[sp-2].flags&WordMatch::flagInInfinitivePhrase) || (sp>=3 && m[sp-3].flags&WordMatch::flagInInfinitivePhrase)))
		{
			m[whereVerb].flags|=WordMatch::flagInInfinitivePhrase;
			if (whereHVerb>=0)
				m[whereHVerb].flags|=WordMatch::flagInInfinitivePhrase;
		}
	}
	else return false;
	int infpElement=(where+len<(signed)m.size()) ? m[where+len].pma.queryPattern(L"__INFP") : -1;
	int iverbTag=findOneTag(tagSet,L"IVERB",-1),whereIVerb=(iverbTag>=0 || infpElement!=-1) ? processInternalInfinitivePhrase(where+len,whereVerb,-1,iverbTag,firstFreePrep,futureBoundPrepositions,inPrimaryQuote,inSecondaryQuote,nextVerbInSeries,sense,whereLastVerb,ambiguousSense,inQuotedString,inSectionHeader,begin,end,infpElement,tagSet) : -1;
	if (isPossible(whereVerb)) tsSense|=VT_POSSIBLE;
	// the purpose was not to direct but to succeed.
	// "not" can also come right after the verb which unfortunately is captured by the object after the verb and not the verb itself because adverbs so often come
	// before the verb and not after.  Also the adverb may be more associated with the verb in the infinitive.
	isNot|=(notTag=findOneTag(tagSet,L"not"  ,-1))>=0 && (tagSet[notTag].sourcePosition==tagSet[verbTagIndex].sourcePosition+tagSet[verbTagIndex].len || tagSet[notTag].sourcePosition==tagSet[verbTagIndex].sourcePosition-1);
	isNot|=(notTag=findOneTag(tagSet,L"never",-1))>=0 && (tagSet[notTag].sourcePosition==tagSet[verbTagIndex].sourcePosition+tagSet[verbTagIndex].len || tagSet[notTag].sourcePosition==tagSet[verbTagIndex].sourcePosition-1);
	bool isNonPast=((tsSense!=VT_PAST && tsSense!=VT_EXTENDED+ VT_PAST && tsSense!=VT_PASSIVE+ VT_PAST && tsSense!=VT_PASSIVE+ VT_PAST+VT_EXTENDED));
	bool isNonPresent=((tsSense!=VT_PRESENT && tsSense!=VT_EXTENDED+ VT_PRESENT && tsSense!=VT_PASSIVE+ VT_PRESENT && tsSense!=VT_PASSIVE+ VT_PRESENT+VT_EXTENDED));
	// correction for VERBVERB I have come to you - ambiguous
	if (tsSense==VT_PRESENT && whereVerb>0 && (m[whereVerb].word->second.inflectionFlags&VERB_PAST_PARTICIPLE)!=0 && m[whereVerb-1].word->first==L"have")
		tsSense=VT_PAST;
	if (whereVerb>=0) 
	{
		m[whereVerb].verbSense=m[whereVerb].quoteForwardLink=tsSense | ((isNot) ? VT_NEGATION:0);
		m[whereVerb].hasVerbRelations=true;
	}
	if (whereHVerb>=0) 
	{
		m[whereHVerb].verbSense=m[whereHVerb].quoteForwardLink=tsSense | ((isNot) ? VT_NEGATION:0);
		m[whereHVerb].hasVerbRelations=true;
		m[whereHVerb].relInternalVerb=whereVerb;
	}
	if (whereVerb > whereLastVerb)
		whereLastVerb = whereVerb + 1;
	if (whereHVerb > whereLastVerb)
		whereLastVerb = whereHVerb + 1;
	attachAdverbRelation(tagSet,verbTagIndex,m[whereVerb].getVerbME(where,30,lastNounNotFound,lastVerbNotFound));
	tIWMM masterVerbWord=wNULL;
	bool possibleCompoundVerb=(whereVerb>0 && m[whereVerb-1].queryWinnerForm(coordinatorForm)>=0 && m[whereVerb-1].pma.queryPattern(L"__INFPSUB")!=-1);
	if (!checkAmbiguousVerbTense(whereVerb,tsSense,inPrimaryQuote,masterVerbWord) && tsSense!=-1 && !withinInfinitivePhrase && !possibleCompoundVerb)
	{
		// find sense
		int nextVerb2Tag=-1;
		int verb2TagIndex=findTag(tagSet,L"VERB2",nextVerb2Tag); // is this the second verb in a series?
		if (sense!=TENSE_NOT_SPECIFIED && sense!=tsSense)
			ambiguousSense=true;
		else
		{
			sense=tsSense;
			wstring tmpstr;
			if (debugTrace.traceRelations)
				lplog(L"%d:sense set to %s lastSense=%s.  Set nextVerbInSeries to %s.",
				whereVerb,senseString(tmpstr,sense).c_str(),senseString(tmpstr,lastSense).c_str(),(verb2TagIndex!=-1 && (sense&(VT_EXTENDED|VT_POSSIBLE|VT_PASSIVE))==0) ? L"true":L"false");
			// verb may use some part of previous verb, so don't record tense if verb is not complex (multi-word)
			nextVerbInSeries|=verb2TagIndex!=-1 && (sense&(VT_EXTENDED|VT_POSSIBLE|VT_PASSIVE))==0;
		}
	}
	if (whereVerb>whereLastVerb && !withinInfinitivePhrase && !possibleCompoundVerb)
	{
		if (!nextVerbInSeries)
		{
			if (debugTrace.traceRelations)
				lplog(L"Setting verb tenses.  whereVerb=%d whereLastVerb=%d. Setting whereLastVerb to %d.",whereVerb,whereLastVerb,where+len);
			bool senseError=false;
			trackVerbTenses(whereVerb,tagSet,lastOpeningPrimaryQuote>=0 || inPrimaryQuote,inQuotedString,inSectionHeader,ambiguousSense,sense,senseError);
			if (senseError && debugTrace.traceRelations)
				printSentence(SCREEN_WIDTH,begin,end,true);
		}
		else if (debugTrace.traceRelations)
			lplog(L"Verb tense rejected - verbInSeries (0).  whereVerb=%d whereLastVerb=%d. nextVerbInSeries=%s. Setting whereLastVerb to %d.",whereVerb,whereLastVerb,(nextVerbInSeries)?L"true":L"false",where+len);
		whereLastVerb=whereVerb+1;
	}
	else if (debugTrace.traceRelations)
		lplog(L"Verb tense rejected (1) - whereVerb<whereLastVerb.  whereVerb=%d whereLastVerb=%d. nextVerbInSeries=%s",whereVerb,whereLastVerb,(nextVerbInSeries)?L"true":L"false");
	nextTag=-1;
	int subjectTag=findTag(tagSet,L"SUBJECT",nextTag),maxLen=-1;
	vector <tIWMM> subjectWords;
	vector <int> subjectObjects,whereSubjects;
	bool objectAsSubject=false,subjectIsPleonastic=false,backwardsSubjects=false,noObjects=findOneTag(tagSet,L"OBJECT",-1)<0;
	if (subjectTag>=0)
	{
		discoverSubjects(where,tagSet,subjectTag,isId,objectAsSubject,subjectIsPleonastic,subjectWords,subjectObjects,whereSubjects);
		//  Tommy did likewise , and [a few CONTACTmoments] later was following the two men[whittington,boris] down the stairs .
		if (subjectWords.size()==1 && subjectWords[0]!=wNULL && (subjectWords[0]->second.timeFlags&T_UNIT) && whereSubjects[0]+1<(signed)m.size() && 
				m[whereSubjects[0]].endObjectPosition>=0 && m[whereSubjects[0]].beginObjectPosition>0 &&
			  m[m[whereSubjects[0]].endObjectPosition].queryWinnerForm(adverbForm)!=-1 && m[m[whereSubjects[0]].beginObjectPosition-1].queryForm(coordinatorForm)!=-1)
		{
			if (debugTrace.traceRelations)
				lplog(LOG_RESOLUTION,L"%06d:coordinate time subject rejected.",whereSubjects[0]);
			subjectWords.clear();
			subjectObjects.clear();
			whereSubjects.clear();
			subjectTag=-1;
		}
		else
		{
			// Did he[julius] mean that , after all[tuppence,julius] , he[julius] had not abandoned the case (no question mark)
			int imperativeTag=-1;
			if (!(m[tagSet[subjectTag].sourcePosition].flags&WordMatch::flagInQuestion) && (tsSense&VT_IMPERATIVE) && 
					(imperativeTag=findOneTag(tagSet,L"imp",-1))>=0 && (tagSet[imperativeTag].sourcePosition<tagSet[subjectTag].sourcePosition))
				m[tagSet[subjectTag].sourcePosition].flags|=WordMatch::flagInQuestion;
			if (m[tagSet[subjectTag].sourcePosition].flags&WordMatch::flagInQuestion)
				m[whereVerb].flags|=WordMatch::flagInQuestion;
		}
	}
	// subject has not already been found for this verb
	// don't search if this is an internal infinitive phrase as thhe subject is already known by parent
	// if within parentless "infinitive" phrase, make sure it is actually just an INFPSUB and not an actual infinitive phrase, as
	//   an actual infinitive phrase might be bound to the object or subject
	if (subjectTag<0 && (backwardsSubjects=m[whereVerb].relSubject<0 && !internalInfinitivePhrase) )
	{
		bool multiSubject=false;
		scanForSubjectsBackwardsInSentence(where,whereVerb,isId,objectAsSubject,subjectIsPleonastic,subjectWords,subjectObjects,whereSubjects,tsSense,multiSubject,withinInfinitivePhrase);
		// scan forwards
		// don't scan forwards in questions [MOVE_OBJECTBrought a message from Mrs . Vandemeyer , I[master] suppose ? ]
		// but accept forwards in these questions: Brought a telephone message to the man Whittington , did he[brown,whittington] ?
		if (whereSubjects.empty() && 
			  (m[where].pma.queryPattern(L"_INTRO_S1",maxLen)!=-1 && pema.queryTag(m[where+maxLen].beginPEMAPosition,SUBJECT_TAG)!=-1 && !(m[where].flags&WordMatch::flagInQuestion)) ||
			  ((m[where].flags&WordMatch::flagInQuestion) && m[where].pma.queryPattern(L"__INTRO_S1",maxLen)!=-1 && m[where+maxLen].word->first==L"did" && m[where+maxLen+1].getObject()>=0))
		{
			int whereSubject=where+maxLen;
			if (m[where].flags&WordMatch::flagInQuestion)
				whereSubject++;
			if (m[whereSubject].principalWherePosition>=0)
				whereSubject=m[whereSubject].principalWherePosition;
			if ((inPrimaryQuote || inSecondaryQuote) && ((tsSense&VT_TENSE_MASK)==VT_PRESENT) && !(tsSense&VT_EXTENDED))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:verb@%d having forward subject@%d is rejected [ in-quote command ].",where,whereVerb,whereSubject);
			}
			else if (!inPrimaryQuote && !inSecondaryQuote && m[whereVerb-1].queryForm(quoteForm)>=0 && ((tsSense&VT_TENSE_MASK)==VT_PAST) && m[whereVerb].pma.queryPattern(L"_VERBREL1")!=-1)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:verb@%d having forward subject@%d is rejected [ possible speaker verb ].",where,whereVerb,whereSubject);
			}
			else
			{
				tIWMM subjectWord=m[whereSubject].word;
				// if subject is a time // three minutes later came another
				objectAsSubject=(!isId && ((subjectWord->second.timeFlags&T_UNIT)!=0 || subjectWord==Words.TIME));
				subjectWords.push_back(subjectWord);
				subjectObjects.push_back(m[whereSubject].getObject());
				whereSubjects.push_back(whereSubject);
				// ESTABUshered into the presence of Mr . Carter , he[carter] and I[tommy] wish each other good morning as is customary
				while (m[whereSubject].nextCompoundPartObject>=0)
				{
					whereSubject=m[whereSubject].nextCompoundPartObject;
					subjectWord=m[whereSubject].word;
					subjectWords.push_back(subjectWord);
					subjectObjects.push_back(m[whereSubject].getObject());
					whereSubjects.push_back(whereSubject);
				}
				wstring tmpstr;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:verb@%d having forward subject@%d [%s] is accepted.",where,whereVerb,whereSubject,objectString(subjectObjects,tmpstr).c_str());
			}
		}
		if (withinInfinitivePhrase && m[where].word->first!=L"to" && whereSubjects.size() && debugTrace.traceSpeakerResolution)
		{
			for (int is=0; is<(signed)whereSubjects.size(); is++)
				lplog(LOG_RESOLUTION,L"%06d:parent-less conjunctive verb@%d having subject@%d is accepted.",where,whereVerb,whereSubjects[is]);
		}
		int maxEnd;
		if (withinInfinitivePhrase && m[where].word->first==L"to" && whereSubjects.size() && queryPattern(whereVerb,L"__NOUNRU",maxEnd)==-1)
		{
			int previousWhereObject=where-1;
			while (m[previousWhereObject].queryWinnerForm(adverbForm)>=0 && previousWhereObject>1) previousWhereObject--;
			if (m[previousWhereObject].word->first==L",")
			{
				while (m[previousWhereObject].word->first!=L"," && previousWhereObject>1) previousWhereObject--;
				previousWhereObject--;
			}
			if (m[previousWhereObject].getObject()>=0) whereSubjects.clear();
			// these following three are too complex so they yield the wrong subject! (solve later)  
			// DEBUG NOK 005341:verb@5342 subj@5337 [NO OBJ=false MSUBJ=false P=false IPO=false].
			// NOK 063684:verb@63685 subj@63675 [NO OBJ=true MSUBJ=false P=false IPO=false]. 
			// NOK 094247:verb@94248 subj@94239 [NO OBJ=true MSUBJ=false P=false IPO=false].
			if (debugTrace.traceRelations)
				for (int is=0; is<(signed)whereSubjects.size(); is++)
				{
					lplog(LOG_RESOLUTION,L"%06d:parent-less infinitive verb@%d subj@%d [NO OBJ=%s MSUBJ=%s P=%s].",where,whereVerb,whereSubjects[is],
								(noObjects) ? L"true":L"false",(multiSubject) ? L"true":L"false",
								((m[whereSubjects[is]].getRelVerb()>=0 && m[m[whereSubjects[is]].getRelVerb()].relPrep>=0) || (m[where-1].objectRole&PREP_OBJECT_ROLE)) ? L"true":L"false");
				}
		}
	}
	if (withinInfinitivePhrase && whereSubjects.size() && m[whereSubjects[0]].getRelObject()>=0 && m[whereSubjects[0]].getRelVerb()>=0)
	{
		wstring verb;
		unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(m[whereSubjects[0]].getRelVerb(),verb);
		if (lvtoCi!=vbNetVerbToClassMap.end())
			for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
				if (vbNetClasses[*vbi].control)
				{
					int whereParentObject=m[whereSubjects[0]].getRelObject();
					if (debugTrace.traceRelations)
						lplog(LOG_RESOLUTION,L"%06d:infinitive verb@%d - parent object@%d as infinitive subject overrules parent subject@%d because of parent control@%d.",
					      where,whereVerb,whereParentObject,whereSubjects[0],m[whereSubjects[0]].getRelVerb());
					subjectWords.clear();
					subjectObjects.clear();
					whereSubjects.clear();
					subjectWords.push_back(m[whereParentObject].word);
					subjectObjects.push_back(m[whereParentObject].getObject());
					whereSubjects.push_back(whereParentObject);
					break;
				}
	}
	// prevent PRESENT from overwriting PAST / Tuppence beat a hasty retreat . 
	bool agreementTestable;
	if (isNonPast && inPrimaryQuote && !(tsSense&(VT_POSSIBLE|VT_PASSIVE|VT_EXTENDED|VT_VERB_CLAUSE)) && 
		 (tsSense&VT_TENSE_MASK)!=VT_PAST_PERFECT && (tsSense&VT_TENSE_MASK)!=VT_PRESENT_PERFECT && (tsSense&VT_TENSE_MASK)!=VT_FUTURE && (tsSense&VT_TENSE_MASK)!=VT_FUTURE_PERFECT && 
		whereSubjects.size()==1 && subjectObjects[0]>=0 && !objects[subjectObjects[0]].plural && !evaluateSubjectVerbAgreement(whereVerb,whereSubjects[0], agreementTestable))
		isNonPast=false;
	wstring tmpstr,tmpstr2,tmpstr3;
	// if passive, locate "by" preposition and relocate SUBJECT there (with SUBJECT_ROLE)
	// also mark objects of prepositional phrases
	bool delayedReceiver=whereVerb>=0 && 
		isDelayedReceiver((m[whereVerb].word->second.mainEntry==wNULL) ? m[whereVerb].word : m[whereVerb].word->second.mainEntry);
	// make sure that this is not a quoted subject followed by a 'reply' verb and an object: this is probably not a sentence
	if (whereSubjects.size()==1 && (tsSense&VT_PAST)==VT_PAST && inPrimaryQuote && !inSecondaryQuote && !noObjects && whereVerb>=0 && subjectTag>=0 &&
		m[tagSet[subjectTag].sourcePosition].word->first==L"“" && m[tagSet[subjectTag].sourcePosition+tagSet[subjectTag].len-1].word->first==L"”" && 
		(m[whereVerb].queryForm(thinkForm)>=0 || m[whereVerb].queryForm(internalStateForm)>=0))
		whereSubjects.clear();
	markPrepositionalObjects(where,whereVerb,withinInfinitivePhrase,subjectIsPleonastic,objectAsSubject,isId,inPrimaryQuote,inSecondaryQuote,isNot,isNonPast,isNonPresent,noObjects,delayedReceiver,tsSense,tagSet);
	// if this is the only prepositional phrase, and it is not in the subject, then record a verb-prep relation
	int relPrep;
	if ((relPrep=m[whereVerb].relPrep)!=-1 && m[relPrep].relPrep==-1)
	{
		bool ambiguous;
		// not delayed because the arguments are not subject to enhanced resolution (not an object)
		if (ambiguous=!(m[relPrep].objectRole&SUBJECT_ROLE))
			addDelayedWordRelations(where,whereVerb,relPrep,VerbWithPrep);
		// if a preposition (PREP2) directly follows another object (O1) of another preposition (PREP1), the object (O2) of the preposition PREP2 points to the previous object (O1) by relNextObject.
		int relObject=m[relPrep].getRelObject();
		if (relObject>=0 && m[relObject].relNextObject>=0)
		{
			// delayed because the second argument is subject to enhanced resolution 
			addDelayedWordRelations(where,m[relObject].relNextObject,relPrep,(ambiguous) ? AWordWithPrep : WordWithPrep);
		}
	}
	// Occasionally PREP may not be visible from the S1 structure
	// He[man] indicated the place he[man] had been occupying at the head of the table[table] .
	if (m[whereVerb].relPrep<0 && whereVerb+2<(signed)m.size() && m[whereVerb+1].pma.queryPattern(L"_PP")!=-1 && m[whereVerb+1].isOnlyWinner(prepositionForm) &&
		  m[whereVerb+2].queryWinnerForm(prepositionForm)<0 && m[whereVerb+2].queryWinnerForm(relativizerForm)<0)
		futureBoundPrepositions.push_back(whereVerb);
	if (firstFreePrep>=0)
	{
		// From the shelter of the doorway he[boris,julius] watched him[julius,boris,tommy] ESTABgo up the steps of a particularly evil - looking house
		// hverb=watched verb=go
		// 'From the shelter of the doorway' should preferentially bind with the main verb 'watched'
		int nextPrep=(whereHVerb>=0) ? whereHVerb : whereVerb,prepLoop=0;
		if (whereHVerb>=0 && debugTrace.traceRelations)
			lplog(LOG_RESOLUTION,L"%06d:Preferentially binding prep@%d to main verb@%d rather than verb@%d.",where,firstFreePrep,whereHVerb,whereVerb);
		wstring chain,tmp;
		while (m[nextPrep].relPrep>=0 && prepLoop<20)
		{
			if (chain.length()) chain+=L"->";
			chain+=itos(m[nextPrep].relPrep,tmp);
			nextPrep=m[nextPrep].relPrep;
			if (nextPrep==firstFreePrep) break;
			if (prepLoop++>20)
			{
				lplog(LOG_ERROR,L"%06d:Prep loop occurred (7) %s.",nextPrep,loopString(nextPrep,tmp));
				break;
			}
		}
		if (nextPrep!=firstFreePrep) 
		{
			int nextFreePrep=firstFreePrep;
			wstring appChain=itos(nextFreePrep,tmp);
			while (m[nextFreePrep].relPrep>=0 && appChain.length()<100)
			{
				appChain+=L"->";
				appChain+=itos(m[nextFreePrep].relPrep,tmp);
				nextFreePrep=m[nextFreePrep].relPrep;
				if (nextFreePrep==nextPrep) break;
			}
			if (appChain.length()>100 && debugTrace.traceRelations)
				lplog(LOG_ERROR,L"%06d:verb=%d ERROR appended prep chain %s to chain %s at %d.",where,whereVerb,appChain.c_str(),chain.c_str(),nextPrep);
			if (nextPrep!=nextFreePrep) 
			{
				setRelPrep(nextPrep,firstFreePrep,6,PREP_PREP_SET,whereVerb);
				m[firstFreePrep].notFreePrep=true;
				if (debugTrace.traceRelations)
					lplog(LOG_RESOLUTION,L"%06d:verb=%d appended prep chain %s to chain %s at %d.",where,whereVerb,appChain.c_str(),chain.c_str(),nextPrep);
			}
		}
	}
	outsideQuoteTruth=!isNonPast && !isNot && !inPrimaryQuote && !inSecondaryQuote;
	inQuoteTruth=!isNonPresent && !isNot && inPrimaryQuote;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION, L"%d outsideQuoteTruth=%s inQuoteTruth=%s inPrimaryQuote=%s inSecondaryQuote=%s isNonPast=%s isNonPresent=%s isNot=%s", where,
			(outsideQuoteTruth) ? L"true":L"false",
			(inQuoteTruth) ? L"true":L"false",
			(inPrimaryQuote) ? L"true":L"false",
			(inSecondaryQuote) ? L"true":L"false",
			(isNonPast) ? L"true":L"false",
			(isNonPresent) ? L"true":L"false",
			(isNot) ? L"true":L"false");
	int numObjects=0,whereObject=-1,whereNextObject=-1,hObjectTag=findOneTag(tagSet,L"HOBJECT",-1);
	int whereHObject=(hObjectTag>=0) ? m[tagSet[hObjectTag].sourcePosition].principalWherePosition : -1;
	int wherePrepInObject=findPrepRole(whereVerb,OBJECT_ROLE,SUBJECT_ROLE);
	bool reverseObjectSpeaker=false;
	for (int objectTag=findOneTag(tagSet,L"OBJECT",-1); objectTag>=0; objectTag=findOneTag(tagSet,L"OBJECT",objectTag),numObjects++)
	{
		bool objectNot=findTagConstrained(tagSet,L"not",nextTag,tagSet[objectTag])>=0; // neither a man nor a woman
		// is object a subsentence?  if so, skip.
		if (tagSet[objectTag].isPattern && patterns[tagSet[objectTag].pattern]->name==L"__S1") 
		{
			if (m[whereVerb].queryWinnerForm(thinkForm)>=0)
				for (int I=tagSet[objectTag].sourcePosition; I<(signed)(tagSet[objectTag].sourcePosition+tagSet[objectTag].len); I++)
					if (m[I].getObject()>=0)
					{
						m[I].objectRole|=THINK_ENCLOSING_ROLE; 
						if (m[I].getRelVerb()>=0) 
							m[m[I].getRelVerb()].objectRole|=THINK_ENCLOSING_ROLE;
					}
			whereObject=tagSet[objectTag].sourcePosition;
			if (m[whereObject].principalWherePosition>=0) whereObject=m[whereObject].principalWherePosition;
			m[whereVerb].relInternalObject=whereObject;
			for (int s=0; s<(signed)whereSubjects.size(); s++)
				m[whereSubjects[s]].relInternalObject=whereObject;
			continue;
		}
		if (numObjects==1)
			whereNextObject=whereObject;
		int mnounTag;
		vector <int> whereMObjects;
		// neither Whittington{not} nor Boris{not} , but a man of striking appearance{but} .
		if ((mnounTag=findTagConstrained(tagSet,L"MNOUN",nextTag,tagSet[objectTag]))>=0)
		{	
			bool notbutFound=false;
			whereObject=-1;
			vector < vector <tTagLocation> > notTagSets;
			if (startCollectTagsFromTag(false,notbutTagSet,tagSet[mnounTag],notTagSets,false,true, false, L"compound objects - 'not' detection")>0)
			{
				for (unsigned int J=0; J<notTagSets.size(); J++)
				{
					if (notTagSets[J].empty()) continue; // shouldn't happen - but does
					notbutFound=true;
					for (int oTag=findOneTag(notTagSets[J],L"not",-1); oTag>=0; oTag=findOneTag(notTagSets[J],L"not",oTag)) // neither a man
					{
						if ((whereObject=m[notTagSets[J][oTag].sourcePosition].principalWherePosition)<0) continue;
						m[whereObject].objectRole|=NOT_OBJECT_ROLE;
						if (debugTrace.traceRole)
							lplog(LOG_ROLE,L"%d:Set object %s to NOT (%s) role [EART MNOUN].",
										whereObject,objectString(m[whereObject].getObject(),tmpstr,true).c_str(),senseString(tmpstr2,tsSense).c_str());
					}
					whereObject=-1;
					int oTag=findOneTag(notTagSets[J],L"but",-1); // but a man of striking appearance
					if (oTag<0 || (whereObject=m[notTagSets[J][oTag].sourcePosition].principalWherePosition)<0) continue;
					if (debugTrace.traceRole)
						lplog(LOG_ROLE,L"%d:surviving object %s [EART MNOUN] found.",
									whereObject,objectString(m[whereObject].getObject(),tmpstr,true).c_str());
				}
			}
			vector < vector <tTagLocation> > mobjectTagSets;
			if (startCollectTagsFromTag(true,mobjectTagSet,tagSet[mnounTag],mobjectTagSets,false,true, true, L"compound objects")>0)
			{
				for (unsigned int J=0; J<mobjectTagSets.size(); J++)
				{
					//if (t.traceSpeakerResolution)
					//	::printTagSet(LOG_ROLE,L"MOBJECT",J,mobjectTagSets[J]);
					for (int oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",-1); oTag>=0; oTag=findOneTag(mobjectTagSets[J],L"MOBJECT",oTag)) 
						if (mobjectTagSets[J][oTag].PEMAOffset<0)
							whereMObjects.push_back((m[mobjectTagSets[J][oTag].sourcePosition].principalWherePosition>=0) ? m[mobjectTagSets[J][oTag].sourcePosition].principalWherePosition : mobjectTagSets[J][oTag].sourcePosition);
				}
			}
			if (whereObject==-1 && debugTrace.traceRole && notbutFound)
				lplog(LOG_ROLE,L"%d:main object not found [EART MNOUN].",tagSet[objectTag].sourcePosition);
			if (whereObject<0 && (whereObject=m[tagSet[objectTag].sourcePosition].principalWherePosition)<0) continue;
		}
		else 
		{
			int sourcePosition=tagSet[objectTag].sourcePosition;
			if ((whereObject=m[sourcePosition].principalWherePosition)<0) 
			{
				if (m[sourcePosition].queryForm(quoteForm)<0 || (whereObject=m[sourcePosition+1].principalWherePosition)<0)
				{
					if (numObjects==1) 
					{
						whereObject=whereNextObject;
						whereNextObject=-1;
						break;
					}
					continue;
				}
			}
		}
		// the {document} itself , ” [said] the *german* bluntly.
		m[whereObject].objectRole|=FOCUS_EVALUATED;
		if (m[whereObject].flags&WordMatch::flagInQuestion)
			m[whereVerb].flags|=WordMatch::flagInQuestion;
		attachAdjectiveRelation(tagSet,whereObject);
		if (whereVerb>0 && (reverseObjectSpeaker=!(m[whereObject].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE)) && m[whereVerb-1].word->first==L"”" && 
				((tsSense&VT_TENSE_MASK)==VT_PAST) && m[whereVerb].pma.queryPattern(L"_VERBREL1")!=-1 && m[whereObject].getObject()>=0 && objects[m[whereObject].getObject()].isAgent(true)))
		{
			if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%06d:object %s rejected [possible speaker].",where,whereString(whereObject,tmpstr,true).c_str());
			whereSubjects.clear();
			whereSubjects.push_back(whereObject);
			subjectWords.clear();
			subjectWords.push_back(m[whereObject].word);
			subjectObjects.clear();
			subjectObjects.push_back(m[whereObject].getObject());
			whereObject=-1;
			break;
		}
		// She made[whereHVerb V_HOBJECT] you[whereHObject HOBJECT] read[whereVerb V_OBJECT] a book[whereObject OBJECT]
		if (whereHObject>=0)
		{
			m[whereObject].relSubject=whereHObject; 
			for (unsigned int mo=0; mo<whereMObjects.size(); mo++)
				m[whereMObjects[mo]].relSubject=whereHObject;
			attachAdjectiveRelation(tagSet,whereHObject);
		}
		else if (whereSubjects.size())
		{
			m[whereObject].relSubject=whereSubjects[0]; // used for plural/singular subgroup in pronoun resolution - overwritten with object # in identifyRelationships
			for (unsigned int mo=0; mo<whereMObjects.size(); mo++)
				m[whereMObjects[mo]].relSubject=whereSubjects[0];
		}
		if (whereNextObject>=0)
		{
			m[whereObject].relNextObject=whereNextObject;
			m[whereNextObject].relNextObject=whereObject;
		}
		if (whereObject!=whereVerb)
		{
			if (m[whereObject].getRelVerb()>=0 && m[whereObject].relInternalVerb<0 && (m[where].objectRole&SENTENCE_IN_REL_ROLE))
				m[whereObject].relInternalVerb=m[whereObject].getRelVerb();
			m[whereObject].setRelVerb(whereVerb);
		}
		if (m[whereObject].relPrep<0 && whereObject!=wherePrepInObject)
			setRelPrep(whereObject,wherePrepInObject,7,PREP_OBJECT_SET, whereVerb);
		for (unsigned int mo=0; mo<whereMObjects.size(); mo++)
			m[whereMObjects[mo]].setRelVerb(whereVerb);
		if (whereVerb>=0 && (m[whereVerb].getRelObject()<0 || m[whereVerb].getRelObject()==whereNextObject))
			m[whereVerb].setRelObject(whereObject); // used for accumulatingAdjectives 
		int wp,wpo;
		if ((wp=m[whereObject].endObjectPosition)<(signed)m.size() && wp>=0 && m[wp].queryWinnerForm(prepositionForm)>=0 && (wpo=m[wp].getRelObject())>=0 && m[wpo].relPrep==wp)
			m[wpo].relNextObject=whereObject;
		if (!isId && !isNonPast && !isNonPresent && !isNot && !objectNot) continue;
		// mark all objects within the object.  If there is a relative phrase, then only mark nonpast/nonpresent roles.  This allows a relative phrase to 
		//   reflect the tense of its parent.  is-object role should only be marked outside of the relative clause, because that can cause the object within
		//   the relative clause to gain focus (and physical presence)
		bool inRelativeClause=false;
		int whereLastEncounteredObject=-1,whereLastEncounteredGenderedObject=-1;
		if (m[whereObject].getObject()>=0)
		{
			if (objects[m[whereLastEncounteredObject=whereObject].getObject()].isAgent(true))
				whereLastEncounteredGenderedObject=whereObject;
		}
		for (unsigned int I=tagSet[objectTag].sourcePosition; I<tagSet[objectTag].sourcePosition+tagSet[objectTag].len; I++)
		{
			if (m[I].getObject()>=0 && I!=whereObject)
			{
				if (objects[m[whereLastEncounteredObject=I].getObject()].isAgent(true))
					whereLastEncounteredGenderedObject=I;
			}
			if ((infpElement=m[I].pma.queryPattern(L"__INFP"))!=-1)
				processInternalInfinitivePhrase(I,-1,(whereLastEncounteredGenderedObject>=0) ? whereLastEncounteredGenderedObject : whereLastEncounteredObject,-1,firstFreePrep,futureBoundPrepositions,inPrimaryQuote,inSecondaryQuote,nextVerbInSeries,sense,whereLastVerb,ambiguousSense,inQuotedString,inSectionHeader,begin,end,infpElement,tagSet);
			if (!inRelativeClause && (scanForPatternTag(I,REL_TAG)!=-1 || scanForPatternTag(I,SENTENCE_IN_REL_TAG)!=-1))
				inRelativeClause=true;
			if (m[I].getObject()==-1) continue;
			addRoleTagsAt(where,I,inRelativeClause,withinInfinitivePhrase,subjectIsPleonastic,isNot,objectNot,tsSense,isNonPast,isNonPresent,objectAsSubject,isId,inPrimaryQuote,inSecondaryQuote,L"MO");
		}
	}
	//  One restores what the other took away . 
	if (whereSubjects.size()==1 && whereVerb>=0 && whereObject<0 && m[whereVerb].getRelObject()<0 && (m[whereSubjects[0]].objectRole&SENTENCE_IN_REL_ROLE) && 
			m[whereSubjects[0]].beginObjectPosition>0 && m[m[whereSubjects[0]].beginObjectPosition-1].queryWinnerForm(relativizerForm)>=0 &&
		// of those charming pensionnats de jeunes filles with which Paris abounds
		  !(m[m[whereSubjects[0]].beginObjectPosition-1].objectRole&PREP_OBJECT_ROLE))
	{
		//static int last=0;
		//if (last!=where)
		//	lplog(LOG_RESOLUTION,L"%d:Assign object=%d to verb %d?",where,whereSubjects[0]-1,whereVerb);
		//last=where;
	}
	// with him was the evil-looking Number 14.
	if (objectAsSubject && whereSubjects.size()>0 && whereSubjects[0]>0 && m[whereSubjects[0]-1].queryWinnerForm(prepositionForm)>=0 && m[whereVerb].getRelObject()>=0 && whereSubjects[0]>0 &&
		  (m[whereSubjects[0]-1].pma.queryPatternDiff(L"__C1__S1",L"2")!=-1))
	{
		int whereSubject=m[whereVerb].getRelObject();
		int wherePrepObject=whereSubjects[0];
		m[wherePrepObject].relSubject=m[whereVerb].setRelObject(m[whereSubject].setRelObject(m[whereSubject].relSubject=-1));
		m[wherePrepObject-1].setRelObject(wherePrepObject);
		setRelPrep(whereVerb,wherePrepObject-1,8,PREP_VERB_SET,whereVerb);
		whereSubjects[0]=m[whereVerb].relSubject=whereSubject;
		m[wherePrepObject].objectRole&=~SUBJECT_ROLE;
		whereObject=-1;
		if (debugTrace.traceRole)
			lplog(LOG_RESOLUTION,L"%06d:Reassign subject/object (leading PP)",where);
	} 
	// if meta-speaker verbrel clause, ignore object relations
	if (whereObject>=0 && !(whereVerb>0 && m[whereVerb-1].forms.isSet(quoteForm) && (m[whereVerb-1].word->second.inflectionFlags&CLOSE_INFLECTION)==CLOSE_INFLECTION &&
			m[whereVerb].forms.isSet(thinkForm) && numObjects==1))
	{
		addDelayedWordRelations(where,whereVerb,whereObject,VerbWithDirectWord);
		if (whereNextObject>=0)
		{
			addDelayedWordRelations(where,whereVerb,whereNextObject,VerbWithIndirectWord);
			addDelayedWordRelations(where,whereObject,whereNextObject,DirectWordWithIndirectWord);
		}
	}
	if (whereHObject>=0)
	{
		m[whereHObject].objectRole|=FOCUS_EVALUATED;
		m[whereHObject].relSubject=(whereSubjects.empty()) ? -1 : whereSubjects[0]; // used for plural/singular subgroup in pronoun resolution - overwritten with object # in identifyRelationships
		if (whereHObject!=whereHVerb)
			m[whereHObject].setRelVerb(whereHVerb);
		if (whereHVerb>=0)
			m[whereHVerb].setRelObject(whereHObject);
		m[whereVerb].relSubject=whereHObject;
		m[whereVerb].hasVerbRelations=true;
	}
	if (whereIVerb>=0 && m[whereIVerb].relSubject<0)
	{
		int DO=(m[whereVerb].getRelObject()>=0) ? m[m[whereVerb].getRelObject()].getObject() : -1;
		// is main DO gendered?  if not IS_OBJECT, SUBJECT of infinitive.
		if (DO>=0 && objects[DO].isAgent(true) && !(m[m[whereVerb].getRelObject()].objectRole&IS_OBJECT_ROLE))
			m[whereIVerb].relSubject=m[whereVerb].getRelObject();
		// is main DO non-gendered (or no DO), but immediately preceding prepObject gendered?  if so, SUBJECT of infinitive.
		else if ((m[whereVerb].getRelObject()<0 || !(DO>=0 && objects[DO].isAgent(true))) && whereIVerb>2 && m[whereIVerb-2].getObject()>=0 &&
			  m[whereIVerb-2].getObject()>=0 && (m[whereIVerb-2].objectRole&PREP_OBJECT_ROLE) && objects[m[whereIVerb-2].getObject()].isAgent(true))
			m[whereIVerb].relSubject=whereIVerb-2;
		// otherwise, main SUBJECT is SUBJECT of infinitive.
		else if (whereSubjects.size())
			m[whereIVerb].relSubject=whereSubjects[0];
		// is DO gendered pronoun matching potential SUBJECT?  if so, reject SUBJECT.
		if (m[whereIVerb].relSubject>=0 && DO>=0 && whereSubjects.size()==1 && objects[DO].objectClass==PRONOUN_OBJECT_CLASS && m[whereSubjects[0]].getObject()>=0 && objects[DO].matchGender(objects[m[whereSubjects[0]].getObject()]))
			m[whereIVerb].relSubject=-1;
		if (m[whereIVerb].getRelObject()>=0 && m[whereIVerb].relSubject>=0)
			m[m[whereIVerb].getRelObject()].relSubject=m[whereIVerb].relSubject;
	}
	// take care of double negatives.  He was not unknown to the watcher.
	int adjObjectTag=(numObjects==0) ? findOneTag(tagSet,L"ADJOBJECT",-1) : -1;
	if (adjObjectTag>=0 && isNot && isId)
	{
		wstring w=m[tagSet[adjObjectTag].sourcePosition].word->first;
		if (w.size()>4 && w[0]==L'u' && w[1]==L'n')
			isNot=false;
	}
	if (adjObjectTag>=0 && objectAsSubject)
	{
		int whereAdjObject=tagSet[adjObjectTag].sourcePosition;
		m[whereAdjObject].objectRole|=SUBJECT_ROLE|FOCUS_EVALUATED;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%d:Set object %s to subject role (objectAsSubject [adjective]).",
				whereAdjObject,objectString(m[whereAdjObject].getObject(),tmpstr,true).c_str());
	}
	// hObjects are for use with _VERB_BARE_INF - I make/made you approach him, where there is only a relationship between the subject (I) and (you)
	int wherePrepInSubject=findPrepRole(whereVerb,SUBJECT_ROLE,0); // don't necessarily reject a prep phrase on the object side of the sentence
	for (unsigned int K=0; K<whereSubjects.size(); K++)
	{
		attachAdjectiveRelation(tagSet,whereSubjects[K]);
		evaluateSubjectRoleTag(where,K,whereSubjects,whereObject,whereHObject,whereVerb,whereHVerb,subjectObjects,tsSense,!inPrimaryQuote && !inSecondaryQuote && (isNonPast || isNot || numObjects>0),isNot,isNonPast,isNonPresent,isId,subjectIsPleonastic,inPrimaryQuote,inSecondaryQuote,backwardsSubjects);
		if (m[whereSubjects[K]].relPrep<0 && whereSubjects[K]!=wherePrepInSubject)
			setRelPrep(whereSubjects[K],wherePrepInSubject,9,PREP_OBJECT_SET,whereVerb);
		if (whereSubjects[K]!=whereVerb) 
		{
			if (tsSense&VT_PASSIVE)
			{
				addDelayedWordRelations(where,whereVerb,whereSubjects[K],VerbWithDirectWord);
			}
			else
			{
				addDelayedWordRelations(where,whereVerb,whereSubjects[K],(isNot) ? NotVerbWithSubjectWord:VerbWithSubjectWord);
				recordVerbTenseRelations(where,tsSense,subjectObjects[K],whereVerb);
			}
		}
	}
	int qTagIndex=findTag(tagSet,L"QTYPE",nextTag);
	// relativizer doesn't have an object, so will not be assigned unless through this special case
	// also see whether a prepPhrase has an object that is an infinitive and is after any objects and the verb
	if (qTagIndex>=0 && whereVerb>=0)
	{
		int wv=-1;
		if (m[whereVerb].relPrep>=0 && (wv=m[m[whereVerb].relPrep].getRelObject())>=0 && 
		  m[wv].queryWinnerForm(verbForm)>=0 && (m[wv].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE)!=0 &&
			wv>whereVerb && wv>whereObject)
		{
			m[wv].setRelObject(tagSet[qTagIndex].sourcePosition);
			m[tagSet[qTagIndex].sourcePosition].setRelVerb(wv);
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%d:prepositional object verb@%d assigned relative object @%d.",where,wv,tagSet[qTagIndex].sourcePosition);
		}
		else if (noObjects || whereObject<0)
		{ 
			if (m[whereVerb].getRelVerb()>=0)
			{
				m[m[whereVerb].getRelVerb()].setRelObject(tagSet[qTagIndex].sourcePosition);
				m[tagSet[qTagIndex].sourcePosition].setRelVerb(m[whereVerb].getRelVerb());
				if (debugTrace.traceRole)
					lplog(LOG_ROLE,L"%d:secondary verb@%d assigned relative object @%d.",where,m[whereVerb].getRelVerb(),tagSet[qTagIndex].sourcePosition);
			}
			else
			{
				m[whereVerb].setRelObject(tagSet[qTagIndex].sourcePosition);
				m[tagSet[qTagIndex].sourcePosition].setRelVerb(whereVerb);
				if (debugTrace.traceRole)
					lplog(LOG_ROLE,L"%d:verb@%d assigned relative object @%d.",where,whereVerb,tagSet[qTagIndex].sourcePosition);
			}
			for (unsigned int K=0; K<whereSubjects.size(); K++)
				m[whereSubjects[K]].setRelObject(tagSet[qTagIndex].sourcePosition);
		}
	}
	if (whereObject<0 && adjObjectTag>=0) 
	{
		whereObject=tagSet[adjObjectTag].sourcePosition;
		for (int I=whereObject; I<whereObject+tagSet[adjObjectTag].len; I++)
			m[I].objectRole|=IS_ADJ_OBJECT_ROLE; // used in resolveMetaGroupObject
	}
	// He is very well off. (misparse - well off should be an adjective).  Use this for accumulating adjectives.
	int advObjectTag=(numObjects==0) ? findOneTag(tagSet,L"ADVOBJECT",-1) : -1;
	if (whereObject<0 && advObjectTag>=0 && isId) 
	{
		whereObject=tagSet[advObjectTag].sourcePosition;
		for (int I=whereObject; I<whereObject+tagSet[advObjectTag].len; I++)
			m[I].objectRole|=IS_ADJ_OBJECT_ROLE; // used in resolveMetaGroupObject
	}
	if (numObjects==0)
	{
		int rel;
		if (((rel=findOneTag(tagSet,L"REL",-1))>=0 || (rel=findOneTag(tagSet,L"S_IN_REL",-1))>=0) && 
			  (signed)tagSet[rel].sourcePosition>whereVerb &&
			  !(m[tagSet[rel].sourcePosition].objectRole&FOCUS_EVALUATED))
		{
			int relBegin=tagSet[rel].sourcePosition,relEnd=relBegin+tagSet[rel].len;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%d:Roles extended to relative clause %d-%d.",where,relBegin,relEnd);
			bool inThinkSay=m[whereVerb].queryForm(thinkForm)>=0;
			for (int I=relBegin; I<relEnd; I++)
				if (m[I].getObject()!=-1)
				{
					addRoleTagsAt(where,I,true,withinInfinitivePhrase,subjectIsPleonastic,isNot,false,tsSense,isNonPast,isNonPresent,objectAsSubject,isId,inPrimaryQuote,inSecondaryQuote,L"MNO");
					if (inThinkSay) m[I].objectRole|=THINK_ENCLOSING_ROLE;
				}
		}
	}
	// His[tommy] face[tommy] was pleasantly ugly -- nondescript , yet unmistakably the face[gentleman] of a gentleman and a sportsman .
	int extendedLen,extendedBegin=-1;
	isNot=false;
	if (where+len+1<(signed)m.size()) adjustToHailRole(where+len+1);
	if (isId && where+len+2<(signed)m.size() && m[where+len].word->first==L"," && !(m[where+len+1].objectRole&HAIL_ROLE))
	{
		if (m[where+len+1].pma.queryPattern(L"__IMPLIEDIS",extendedLen)!=-1) extendedBegin=where+len+1;
		else if (m[where+len+1].pma.queryPattern(L"__NOUN",extendedLen)!=-1) extendedBegin=where+len+1;
		else if (m[where+len+1].pma.queryPattern(L"__MNOUN",extendedLen)!=-1) extendedBegin=where+len+1;
		else if (m[where+len+1].queryForm(coordinatorForm)>=0 && m[where+len+2].pma.queryPattern(L"__NOUN",extendedLen)!=-1) extendedBegin=where+len+2;
		if (extendedBegin!=-1 && extendedBegin+extendedLen<(signed)m.size() && isEOS(extendedBegin+extendedLen) && scanForPatternTag(extendedBegin,VNOUN_TAG)==-1)
		{
			bool inRelativeClause=false,objectNot=false;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%d:Roles extended to noun %d-%d.",where,extendedBegin,extendedBegin+extendedLen);
			for (int I=extendedBegin; I<extendedBegin+extendedLen; I++)
			{
				if (m[I].word->first==L"not") isNot=true;
				if (!inRelativeClause && (scanForPatternTag(I,REL_TAG)!=-1 || scanForPatternTag(I,SENTENCE_IN_REL_TAG)!=-1))
					inRelativeClause=true;
				if (m[I].getObject()==-1) continue;
				addRoleTagsAt(where,I,inRelativeClause,withinInfinitivePhrase,subjectIsPleonastic,isNot,objectNot,tsSense,isNonPast,isNonPresent,objectAsSubject,isId,inPrimaryQuote,inSecondaryQuote,L"ME");
				// And there is a certain man[brown] , a man[brown] whose real name is unknown to us[tuppence,tommy] , who is working in the dark for his[brown] own ends . 
				// don't mark 'us' as belonging to the same phrase as 'there is'
				//if (inRelativeClause) break;
				isNot=false;
			}
		}
	}
	return isId;
}

bool Source::setAdditionalRoleTags(int where,int &firstFreePrep,vector <int> &futureBoundPrepositions,bool inPrimaryQuote,bool inSecondaryQuote,
																	 bool &nextVerbInSeries,int &sense,int &whereLastVerb,bool &ambiguousSense,bool inQuotedString,bool inSectionHeader,int begin,int end,vector < vector <tTagLocation> > &tagSets)
{ LFS
  vector <WordMatch>::iterator im=m.begin()+where;
	patternMatchArray::tPatternMatch *pma=im->pma.content;
	bool idType=false;
	for (unsigned PMAElement=0; PMAElement<im->pma.count; PMAElement++,pma++)
	{
		//  desiredTagSets.push_back(tTS(subjectVerbRelationTagSet,"_SUBJECT_VERB_RELATION",3,"VERB","V_OBJECT","SUBJECT","OBJECT","PREP","IVERB","REL","ADJ","ADV","HOBJECT","V_AGREE","V_HOBJECT",NULL));
		//  desiredTagSets.push_back(tTS(verbObjectRelationTagSet,"_VERB_OBJECT_RELATION",3,"VERB","V_OBJECT","OBJECT","SUBJECT","PREP","IVERB","REL","ADJ","ADV","HOBJECT","V_AGREE","V_HOBJECT","VERB2",NULL));
		__int64 descendants=patterns[pma->getPattern()]->includesOnlyDescendantsAllOfTagSet;
		int preferredTagSet=-1;
		if (descendants&((__int64)1<<subjectVerbRelationTagSet)) preferredTagSet=subjectVerbRelationTagSet;
		else if (descendants&((__int64)1<<verbObjectRelationTagSet)) preferredTagSet=verbObjectRelationTagSet;
		else if (descendants&((__int64)1<<iverbTagSet)) preferredTagSet=iverbTagSet;
		if (preferredTagSet!=-1)
		{
			tagSets.clear();
			if (startCollectTags(debugTrace.traceRelations,preferredTagSet,where,pma->pemaByPatternEnd,tagSets,true,false,L"relations - additional role tags")>0)
			{
				for (unsigned int J=0; J<tagSets.size(); J++)
				{
					if (debugTrace.traceRole)  
						printTagSet(LOG_ROLE|LOG_RESOLUTION,L"ART",J,tagSets[J],where,pma->pemaByPatternEnd);
					bool outsideQuoteTruth,inQuoteTruth;
					idType|=evaluateAdditionalRoleTags(where,tagSets[J],pma->len,firstFreePrep,futureBoundPrepositions,inPrimaryQuote,inSecondaryQuote,outsideQuoteTruth,inQuoteTruth,preferredTagSet==iverbTagSet,false,
						nextVerbInSeries,sense,whereLastVerb,ambiguousSense,inQuotedString,inSectionHeader,begin,end);
					accumulateLocation(where,tagSets[J],-1,outsideQuoteTruth || inQuoteTruth);
					// He thought to move to the other section. - not guaranteed that the character will actually do that
					//checkInfinitivePhraseForLocation(where,tagSets[J],outsideQuoteTruth || inQuoteTruth);
				}
			}
		}
		if (patterns[pma->getPattern()]->name==L"_PP" || patterns[pma->getPattern()]->hasTag(PREP_TAG))
		{
			//lplog(LOG_RESOLUTION,L"Prep loop putting [firstFreePrep=%d whereLastVerb-1==%d]",firstFreePrep,whereLastVerb-1);
			// question pattern _Q2 is both a prepositional phrase and a subject verb pattern
			if (whereLastVerb>0 && firstFreePrep<0 && whereLastVerb<where && (patterns[pma->getPattern()]->questionFlag || m[whereLastVerb-1].relPrep<0)) 
			{
				// current position and whereLastVerb must belong to the same sentence
				bool differenceSentence=false;
				for (int s1=whereLastVerb-1; s1<where && !differenceSentence; s1++)
					differenceSentence=(isEOS(s1) || m[s1].word->first==L"”" || m[s1].word->first==L"’" || m[s1].word==Words.sectionWord);
				if (!differenceSentence)
					firstFreePrep=whereLastVerb-1; // whereLastVerb is actually set to the last verb +1
			}
			tagSets.clear();
			tIWMM prepWord=wNULL,subObjectWord=wNULL;
			if (startCollectTags(false,prepTagSet,where,pma->pemaByPatternEnd,tagSets,true,false,L"role tags - prepositional phrase relation processing")>0)
			{
				for (unsigned int J=0; J<tagSets.size(); J++)
				{
					if (debugTrace.traceRole)  
						printTagSet(LOG_ROLE,L"ARTPP",J,tagSets[J],where,pma->pemaByPatternEnd);
					// find preposition
					int tag,nextTag=-1;
					if ((tag=findTag(tagSets[J],L"P",nextTag))<0 || !tagIsCertain(tagSets[J][tag].sourcePosition)) continue;
					int wp=tagSets[J][tag].sourcePosition;
					if (m[wp].queryWinnerForm(prepositionForm)<0 || m[wp].getRelObject()>=0) continue;
					nextTag=-1;
					// find PREPOBJECT
					if ((tag=findTag(tagSets[J],L"PREPOBJECT",nextTag))<0) continue;
					int wpo=tagSets[J][tag].sourcePosition;
					if (m[wpo].queryWinnerForm(relativizerForm)!=-1 && m[wpo].principalWherePosition<0)
						m[wpo].principalWherePosition=wpo;
					if (m[wpo].queryWinnerForm(relativizerForm)==-1 && (m[wpo].principalWherePosition<0 || m[m[wpo].principalWherePosition].getObject()<0)) continue;
					// is there a previous verb that is unlinked?
					if (where>=1)
					{
						int whereVerb=where-1;
						if (m[whereVerb].queryWinnerForm(prepositionForm)>=0) whereVerb--;
						if (whereVerb>=0 && m[whereVerb].queryWinnerForm(verbForm)>=0 && m[whereVerb].relPrep<0)
							setRelPrep(whereVerb,wp,10,PREP_VERB_SET,whereVerb);
					}
					m[wp].setRelObject(wpo=m[wpo].principalWherePosition);
					setRelPrep(wpo,wp,11,PREP_OBJECT_SET, (whereLastVerb<0) ? -1 : whereLastVerb-1);
					if (firstFreePrep>=0)
					{
						int lastInChain=firstFreePrep,prepLoop=0;
						while (m[lastInChain].relPrep>=0) 
						{
							lastInChain=m[lastInChain].relPrep;
							if (prepLoop++>20)
							{
								wstring tmpstr;
								lplog(LOG_ERROR,L"%06d:Prep loop occurred (3) %s.",lastInChain,loopString(lastInChain,tmpstr));
								break;
							}
						}
						setRelPrep(lastInChain,wp,12,PREP_PREP_SET, (whereLastVerb < 0) ? -1 : whereLastVerb - 1);
					}
					if (firstFreePrep<0)
						firstFreePrep=wp;
					m[wpo].objectRole|=PREP_OBJECT_ROLE;
					if (im->word->second.flags&tFI::prepMoveType)
						m[wpo].objectRole|=MOVEMENT_PREP_OBJECT_ROLE;
					else
						m[wpo].objectRole|=NON_MOVEMENT_PREP_OBJECT_ROLE;
					if (m[wp].word->first==L"of" || m[wp].word->first==L"for" || m[wp].word->first==L"from")
						m[wpo].objectRole|=NO_PP_PREP_ROLE;
					break;
				}
			}
		}
		// big house it was, with a whole lot of piny grounds ...
		// must not be owned by any pattern and must be a __PNOUN followed by an __APPNOUN
		int whereObject=-1;
		if (patterns[pma->getPattern()]->name==L"__APPNOUN" && patterns[pma->getPattern()]->differentiator==L"2" && pma->pemaByChildPatternEnd>=0 &&
				im->getRelVerb()>=0 && im->getRelObject()<0 && (whereObject=m[where+pema[pma->pemaByChildPatternEnd].begin].principalWherePosition)>=0 && m[whereObject].getObject()>=0 &&
				m[whereObject].getRelVerb()<0)
		{
			im->setRelObject(whereObject);
			m[whereObject].setRelVerb(im->getRelVerb());
			m[whereObject].relSubject=where;
			m[im->getRelVerb()].setRelObject(whereObject);
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%06d:Appositive subject@%d - verb@%d - object@%d.",where,where,im->getRelVerb(),whereObject);
		}
	}
	return idType;
}

/*
  relations:
	VerbWithMasterVerb: verb with hverb
	DirectWordWithVerb: subject with master verb
	VerbWithDirectWord: with subject and verb if verb is passive
	SubjectWordWithNotVerb:SubjectWordWithVerb: subject with verb
	VerbWithDirectWord: direct object with verb
	IndirectWordWithVerb: indirect object with verb
	DirectWordWithIndirectWord: indirect object with direct object

*/
// if not noun, verb, adjective, adverb or preposition, no relations
// if relativeObject is not NULL, and this phrase has a SUBJECT, then this is a object relative phrase
//   in this case, the relativeObject should count as an extra object, and if an indirect object is detected, the analysis should be
//     flagged as erroneous.
/*
    updates form usage per word
		evaluates noun/determiner
		tracks verb senses, prints errors
		evaluateVerbObjects
		attachInfinitivePhrase
		attachAdditionalPrepositionalPhrases
		*/
void Source::syntacticRelations()
{ LFS 
	vector < vector <tTagLocation> > tagSets;
  vector <WordMatch>::iterator im=m.begin(),imend=m.end();
	vector <int> futureBoundPrepositions;
	bool inPrimaryQuote=narrativeIsQuoted,inSecondaryQuote=false;
	int whereLastVerb=-1,s=0;
	bool inQuotedString=false,inSectionHeader=false;
	unsigned int begin=0,end=0;
	lastOpeningPrimaryQuote=-1;
	lastQuote=-1;
	previousPrimaryQuote=-1;
  section=0;
  usePIS=false;
  currentSpeakerGroup=0;
	currentTimelineSegment=0;
	currentEmbeddedSpeakerGroup=-1;
	currentEmbeddedTimelineSegment=-1;
	speakerGroupsEstablished=false;
	tempSpeakerGroup.clear();
	subjectsInPreviousUnquotedSectionUsableForImmediateResolution=false;
  debugTrace.traceSpeakerResolution=m.size()>3 && (m[3].t.traceSpeakerResolution || TSROverride);
  for (int I=0; im!=imend; im++,I++)
	{
		markMultipleObjects(I);
		im->objectRole=0; 
	}
	im=m.begin();
	int lastBeginS1=-1,lastRelativePhrase=-1,lastQ2=-1;
	int lastVerb=-1;
	int firstFreePrep=-1;
  for (int I=0; im!=imend; im++,I++)
	{
		debugTrace = m[begin].t;
		if (I==sentenceStarts[s])
		{
			begin=sentenceStarts[s];
			end=sentenceStarts[s+1];
			while (end && m[end-1].word==Words.sectionWord)
				end--; // cut off end of paragraphs
			s++;
		}
		// this sets inSectionHeader, because in sections verb tense flow is skipped over 
		if (!inSectionHeader && section<(signed)sections.size() && I>=(signed)sections[section].begin && I<(signed)sections[section].endHeader)
			inSectionHeader=true;
		while (section<(signed)sections.size() && I>=(signed)sections[section].endHeader)
		{
			section++;
			inSectionHeader=false;
		}
		if (im->pma.queryPattern(L"__S1")!=-1)
		{
			if (I && (m[I-1].word->first==L"if" || m[I-1].word->first==L"unless"))
			{
				for (unsigned int J=I; J<m.size() && !isEOS(J) && m[J].word!=Words.sectionWord; J++)
					m[J].flags|=WordMatch::flagInPStatement;
			}
			if (I && (m[I-1].word->first==L"as"))
			{
				for (unsigned int J=I; J<m.size() && !isEOS(J) && m[J].word!=Words.sectionWord; J++)
					m[J].flags|=WordMatch::flagInLingeringStatement;
			}
      lastBeginS1=I;
		}
    if (im->pma.queryPattern(L"_REL1")!=-1 || im->pma.queryPattern(L"_Q2") != -1 || im->pma.queryPattern(L"_INFP") != -1)
			lastVerb=-1;
		if (m[I].hasVerbRelations) lastVerb=I;
		// At the end of each sentence, link any free preposition clauses to other preposition clauses in the sentence / CMREADME015
		if (isEOS(I) || im->word==Words.sectionWord)
		{
			if (lastVerb>=0 && firstFreePrep>=0 && !m[firstFreePrep].notFreePrep)
			{
				bool alreadyInLoop=false;
				int lastInChain=lastVerb,prepLoop=0;
				while (m[lastInChain].relPrep>=0) 
				{
					alreadyInLoop|=(lastInChain==firstFreePrep);
					lastInChain=m[lastInChain].relPrep;
					if (prepLoop++>20)
					{
						wstring tmpstr;
						lplog(LOG_ERROR,L"%06d:Prep loop occurred (3) %s.",lastInChain,loopString(lastInChain,tmpstr));
						break;
					}
				}
				alreadyInLoop|=(lastInChain==firstFreePrep);
				// if a preposition directly follows another object of a prep, the object of the preposition points to the previous object by relNextObject.
				// make sure this is not after a verb that was missed because it is compound or MTS
				if (!alreadyInLoop && (!firstFreePrep || (m[firstFreePrep-1].getObject()<0 && m[firstFreePrep-1].queryWinnerForm(verbForm)<0 && m[firstFreePrep-1].queryWinnerForm(thinkForm)<0)))
				{
					setRelPrep(lastInChain,firstFreePrep,13,PREP_PREP_SET, lastVerb);
					if (debugTrace.traceRelations)
						lplog(LOG_RESOLUTION,L"%06d:Prep@%d bound to %d verb=%d ZZZ.",I,firstFreePrep,lastInChain,lastVerb);
				}
			}
			lastBeginS1=lastRelativePhrase=lastQ2=-1;
			lastVerb=firstFreePrep=-1;
			whereLastVerb = -1;
		}
		// CMREADME016
    if (im->word->first==L"“")
		{
      inPrimaryQuote=true;
			lastVerb=firstFreePrep=-1;
			if (im->flags&WordMatch::flagQuotedString)
				inQuotedString=true;
			else
			{
				lastOpeningPrimaryQuote=I;
				lastSense=-1;
				if (debugTrace.traceRelations)
					lplog(L"set lastSense to %d (BQ).",lastSense);
			}
		}
    if (im->word->first==L"”")
		{
			inPrimaryQuote=false;
			lastVerb=firstFreePrep=-1;
			if (inQuotedString)
				inQuotedString=false;
			else
			{
				lastOpeningPrimaryQuote=-1;
				lastSense=-1;
				if (debugTrace.traceRelations)
					lplog(L"set lastSense to %d (EQ).",lastSense);
			}
		}
    if (im->word->first==L"‘")
		{
			if (im->flags&WordMatch::flagQuotedString)
				inQuotedString=true;
			else
			{
	      inSecondaryQuote=true;
				inPrimaryQuote=narrativeIsQuoted;
				lastVerb=firstFreePrep=-1;
			}
		}
    else if (im->word->first==L"’")
		{
			if (inQuotedString)
				inQuotedString=false;
			else
			{
	      inSecondaryQuote=false;
				inPrimaryQuote=true;
				lastVerb=firstFreePrep=-1;
			}
		}
		if (updateWordUsageCostsDynamically && im->updateFormUsagePatterns())
		{
#ifdef LOG_USAGE_PATTERNS
			im->logFormUsageCosts();
#endif
		}
		if (inPrimaryQuote)
			im->objectRole|=IN_PRIMARY_QUOTE_ROLE;
		if (inSecondaryQuote)
			im->objectRole|=IN_SECONDARY_QUOTE_ROLE;
	  for (int nextPEMAPosition=im->beginPEMAPosition; nextPEMAPosition!=-1; nextPEMAPosition=pema[nextPEMAPosition].nextByPosition)
			setRole(I,pema.begin()+nextPEMAPosition); // (CMREADME018)
    if (inPrimaryQuote || inSecondaryQuote)		
			adjustToHailRole(I);
		// And Sir James?
		if (im->objectRole&HAIL_ROLE) 
			im->objectRole|=FOCUS_EVALUATED;
		if ((im->objectRole&HAIL_ROLE)  && !inPrimaryQuote && !inSecondaryQuote) 
		{
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%06d:Removed HAIL role (outside quote).",I);
			im->objectRole&=~HAIL_ROLE;
		}
		bool ambiguousSense=false,nextVerbInSeries=false;
		int sense=TENSE_NOT_SPECIFIED;
    if (setAdditionalRoleTags(I,firstFreePrep,futureBoundPrepositions,inPrimaryQuote,inSecondaryQuote,nextVerbInSeries,sense,whereLastVerb,ambiguousSense,inQuotedString,inSectionHeader,begin,end,tagSets) && lastBeginS1>=0)
      m[lastBeginS1].objectRole|=ID_SENTENCE_TYPE;
		patternMatchArray::tPatternMatch *pma=im->pma.content;
		for (unsigned PMAElement=0; PMAElement<im->pma.count; PMAElement++,pma++)
		{
			int voRelationsFound,traceSource;
			if (preTaggedSource)
			{
				tagSets.clear();
				if (startCollectTags(false,nounDeterminerTagSet,I,pma->pemaByPatternEnd,tagSets,true,true, L"syntactic relations - record noun determiner usage")>0)
					for (unsigned int J=0; J<tagSets.size(); J++)
					{
						if (debugTrace.traceDeterminer)
							printTagSet(LOG_INFO,L"ND3",J,tagSets[J],I,pma->pemaByPatternEnd);
						evaluateNounDeterminer(tagSets[J],false,traceSource,I,I+pma->len, pma->pemaByPatternEnd);
					}
			}
			tagSets.clear();
			if (startCollectTags(debugTrace.traceVerbObjects,verbObjectsTagSet,I,pma->pemaByPatternEnd,tagSets,true,false, L"syntactic relations - record verb objects usage")>0)
			{
				for (unsigned int J=0; J<tagSets.size(); J++)
				{
					// if this is an objective relative clause, a relativeObject must be computed and taken into account.
					// Otherwise, incorrect verb object statistics will be recorded.  IVOS
					evaluateVerbObjects(NULL,pma,-1,I,tagSets[J],false,false,voRelationsFound,traceSource, L"syntactic relations - record verb objects usage");
				}
			}
		}
		// bind even loose prepositions with no object to the main verb - helps in stanford analysis
		if (whereLastVerb>=0 && m[I].queryWinnerForm(prepositionForm) >= 0 && m[I].getRelVerb() == -1 && find(futureBoundPrepositions.begin(), futureBoundPrepositions.end(), I) == futureBoundPrepositions.end())
		{
			m[I].setRelVerb(whereLastVerb-1);
			if (debugTrace.traceRelations)
				lplog(LOG_INFO, L"bound prep at %d to verb position %d.", I, whereLastVerb-1);
		}
	}
	for (unsigned int p=0; p<futureBoundPrepositions.size(); p++)
	{
		int v=futureBoundPrepositions[p];
		if (m[v+1].getRelObject()>=0)
		{
			bool setAlready=false;
			int prepLoop=0;
			lastVerb = m[m[v + 1].getRelObject()].getRelVerb();
			for (int w=m[v].relPrep; w>=0 && !setAlready; w=m[w].relPrep)
			{
				if (prepLoop++>20)
				{
					wstring tmpstr;
					lplog(LOG_ERROR,L"%06d:Prep loop occurred (6) %s.",w,loopString(w,tmpstr));
					break;
				}
				setAlready=w==v+1;
			}
			if (!setAlready)
			{
				setRelPrep(v+1,m[v].relPrep,14,PREP_PREP_SET, lastVerb);
				setRelPrep(v,v+1,15,PREP_VERB_SET, lastVerb);
				if (debugTrace.traceRelations)
					lplog(LOG_RESOLUTION,L"%06d:PV verb@%d prep@%d near preposition bound.",v,v,v+1);
			}
		}
	}
}

void Source::testSyntacticRelations()
{
	tIWMM primaryQuoteCloseWord = Words.gquery(L"”");
	tIWMM secondaryQuoteCloseWord = Words.gquery(L"’");
	for (unsigned int s = 0; s<sentenceStarts.size(); s++)
	{
		unsigned int begin = sentenceStarts[s];
		if (m[begin].word == primaryQuoteCloseWord || m[begin].word == secondaryQuoteCloseWord)
			begin++;
		unsigned int end = (s+1==sentenceStarts.size()) ? m.size() : sentenceStarts[s + 1];
		while (end && m[end - 1].word == Words.sectionWord)
			end--; // cut off end of paragraphs
		if (m[end].word == primaryQuoteCloseWord || m[end].word == secondaryQuoteCloseWord)
			end++;
		debugTrace = m[begin].t;
		if (debugTrace.traceTestSyntacticRelations)
		{
			wstring sentence, originalIWord;
			for (unsigned int where = begin; where < end; where++)
			{
				getOriginalWord(where, originalIWord, false, false);
				sentence += originalIWord + L" ";
			}
			auto mc = metaCommandsEmbeddedInSource.find(begin);
			if (mc != metaCommandsEmbeddedInSource.end())
				lplog(LOG_INFO, L"\n*****  %s  *****", mc->second.c_str());
			else if (end - begin > 1)
				lplog(LOG_INFO, L"\n FAILED to find comment at sentence offset %d.", begin);
			bool printedSentence = false;
			for (unsigned int where = begin; where < end; where++)
			{
				wstring relationsPerWord;
				if (m[where].relSubject != -1) relationsPerWord += L"relSubject='" + m[m[where].relSubject].word->first + L"' ";
				if (m[where].getRelVerb() != -1) relationsPerWord += L"relVerb='" + m[m[where].getRelVerb()].word->first + L"' ";
				if (m[where].getRelObject() != -1) relationsPerWord += L"relObject='" + m[m[where].getRelObject()].word->first + L"' ";
				if (m[where].relPrep != -1) relationsPerWord += L"relPrep='" + m[m[where].relPrep].word->first + L"' ";
				if (m[where].relInternalObject != -1) relationsPerWord += L"relInternalObject='" + m[m[where].relInternalObject].word->first + L"' ";
				if (m[where].relInternalVerb != -1) relationsPerWord += L"relInternalVerb='" + m[m[where].relInternalVerb].word->first + L"' ";
				if (m[where].nextCompoundPartObject != -1) relationsPerWord += L"nextCompoundPartObject='" + m[m[where].nextCompoundPartObject].word->first + L"' ";
				if (m[where].previousCompoundPartObject != -1) relationsPerWord += L"previousCompoundPartObject='" + m[m[where].previousCompoundPartObject].word->first + L"' ";
				if (relationsPerWord.length() > 0)
				{
					if (!printedSentence)
					{
						lplog(LOG_INFO, L"%s", sentence.c_str());
						printedSentence = true;
					}
					lplog(LOG_INFO, L"~~~ %s: %s", m[where].word->first.c_str(), relationsPerWord.substr(0,relationsPerWord.length()-1).c_str());
				}
			}
			if (!printedSentence && end-begin>1)
				lplog(LOG_INFO, L"%d-%d:%s: no relations found", begin,end,sentence.c_str());
		}
	}
}
