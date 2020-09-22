#pragma warning(disable : 4786 ) // disable warning C4786
#include <windows.h>
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "vcXML.h"
#include "time.h"
#include "profile.h"
#include <iterator>
#include "QuestionAnswering.h"

wstring relationString(int r)
{ LFS
	switch (r)
	{
	case stESTAB:return L"ESTABLISH";
	case stEXIT:return L"EXIT";
	case stENTER:return L"ENTER";
	case stSTAY:return L"STAY";
	case stESTABLISH:return L"ESTAB";
	case stMOVE:return L"MOVE";
	case stMOVE_OBJECT:return L"MOVE_OBJECT";
	case stMOVE_IN_PLACE:return L"MOVE_IN_PLACE";
	case stMETAWQ:return L"METAWQ";
	case stCONTACT:return L"CONTACT";
	case stNEAR:return L"NEAR";
	case stTRANSFER:return L"TRANSFER";
	case stLOCATION:return L"LOCATION";
	case -stLOCATION:return L"LOCATION";
	case stPREPTIME:return L"PREP TIME";
	case stPREPDATE:return L"PREP DATE";
	case stSUBJDAYOFMONTHTIME:return L"SUB DAY TIME";
	case stABSTIME:return L"ABSOLUTE TIME";
	case stABSDATE:return L"ABSOLUTE DATE";
	case stADVERBTIME:return L"ADVERB TIME";
	case stTHINK:return L"THINK";
	case stCOMMUNICATE:return L"COMMUNICATE";
	case stSTART:return L"START";
	case stCHANGE_STATE:return L"COS";
	case stBE:return L"BE";
	case stHAVE:return L"HAVE";
	case stMETAINFO:return L"METAINFO";
	case stMETAPROFESSION:return L"METAPROFESSION";
	case stMETABELIEF:return L"METABELIEF";
	case stTHINKOBJECT:return L"THINKOBJECT";
	case stCHANGESTATE:return L"CHANGESTATE";
	case stCONTIGUOUS:return L"CONTIGUOUS";
	case stCONTROL:return L"CONTROL";
	case stAGENTCHANGEOBJECTINTERNALSTATE:return L"AGENTCHANGEOBJECTINTERNALSTATE";
	case stSENSE:return L"SENSE";
	case stCREATE:return L"CREATE";
	case stCONSUME:return L"CONSUME";
	case stMETAFUTUREHAVE:return L"METAFUTUREHAVE";
	case stMETAFUTURECONTACT:return L"METAFUTURECONTACT";
	case stMETAIFTHEN:return L"METAIFTHEN";
	case stMETACONTAINS:return L"METACONTAINS";
	case stMETADESIRE:return L"METADESIRE";
	case stMETAROLE:return L"METAROLE";
	case stSPATIALORIENTATION:return L"SPATIALORIENTATION";
	case stLOCATIONRP:return L"LOCATION RELATIVE PHRASE";
	case stIGNORE:return L"IGNORE";
	case stNORELATION:return L"NR";
	case stOTHER:return L"OTHER";
	}
	return L"UNKNOWN";
}

// objects are either direct, indirect or prepositional.
// they are nonphysical or physical.  
// If physical they can be nonlocational or locational.  
// locational is if the object in a scene by itself represents a location.  examples: "room" or a person.
// an object may also be locational by reference.  A "chair" is locational because it is located on the floor and
//   it is unnecessary to say where it is exactly (it is also common).  A banana is not locational because the listener
//   has no idea where it might be in relation to the other objects.  It may be locational by reference if we have a previously known location
//   of a banana truck.


bool cSource::like(wstring str1,wstring str2)
{ LFS
	return wcsncmp(str1.c_str(),str2.c_str(),min(str1.length(),str2.length()))==0;
}

bool comparesr( const cSyntacticRelationGroup &s1, const cSyntacticRelationGroup &s2 )
{ LFS
	return s1.where<s2.where;
}

void cSource::getMaxWhereSR(vector <cSyntacticRelationGroup>::iterator csr,int &begin,int &end)
{ LFS
		begin=100000000;
		end=-1;
		if (csr->whereControllingEntity>=0) begin=min(begin,csr->whereControllingEntity);
		end=max(end,csr->whereControllingEntity);
		if (csr->whereSubject>=0) begin=min(begin,csr->whereSubject);
		end=max(end,csr->whereSubject);
		if (csr->whereVerb>=0) begin=min(begin,csr->whereVerb);
		end=max(end,csr->whereVerb);
		if (csr->wherePrep>=0) begin=min(begin,csr->wherePrep);
		end=max(end,csr->wherePrep);
		if (csr->whereObject>=0) begin=min(begin,csr->whereObject);
		end=max(end,csr->whereObject);
		if (csr->wherePrepObject>=0) begin=min(begin,csr->wherePrepObject);
		end=max(end,csr->wherePrepObject);
}

vector <cSyntacticRelationGroup>::iterator cSource::findSyntacticRelationGroup(int where)
{ LFS
		cSyntacticRelationGroup sr(where,-1,-1,-1,-1,-1,-1,-1,-1,-1,false,false,-1,-1,false);
		return lower_bound(syntacticRelationGroups.begin(), syntacticRelationGroups.end(), sr, comparesr);
}

// space relation component
const wchar_t *cSource::src(int where,wstring description,wstring &tmpstr)
{ LFS
	if (where<0) return L"";
	wchar_t temp[100];
	_itow(where,temp,10);
	wstring tmpstr2;
	tmpstr=L"["+description+L" "+wstring(temp)+L":"+whereString(where,tmpstr2,true)+L"]";
	return tmpstr.c_str();
}

bool cSource::followerPOVToObserverConversion(vector <cSyntacticRelationGroup>::iterator sr,int sg)
{ LFS
	if (sr->whereVerb<0 || sr->whereObject<0) return false;
	bool allIn,oneIn,converted=false;
	vector <cOM> subjects;
	if (sr->whereSubject>=0)
	{
		if (m[sr->whereSubject].objectMatches.empty())
			subjects.push_back(cOM(m[sr->whereSubject].getObject(),SALIENCE_THRESHOLD));
		else
			subjects=m[sr->whereSubject].objectMatches;
	}
	// follow them!
	else if ((m[sr->where].objectRole&IN_PRIMARY_QUOTE_ROLE) && 
		     (m[sr->whereVerb].verbSense&(VT_TENSE_MASK|VT_EXTENDED))==VT_PRESENT && !(m[sr->whereVerb].flags&cWordMatch::flagInInfinitivePhrase) && 
				 sr->whereControllingEntity<0 && sr->tft.lastOpeningPrimaryQuote>=0 && m[sr->tft.lastOpeningPrimaryQuote].audienceObjectMatches.size()>=1)
		subjects=m[sr->tft.lastOpeningPrimaryQuote].audienceObjectMatches;
	if ((sr->tft.futureHappening || sr->tft.presentHappening) && sg<(signed)speakerGroups.size() && 
		  sr->whereObject>=0 && sr->whereVerb>=0 &&
		  intersect(subjects,speakerGroups[sg].speakers,allIn,oneIn) &&
		  intersect(sr->whereObject,speakerGroups[sg].speakers,allIn,oneIn) &&
		  intersect(subjects,speakerGroups[sg].povSpeakers,allIn,oneIn) &&
			!intersect(subjects,speakerGroups[sg].observers,allIn,oneIn) &&
			!intersect(subjects,speakerGroups[sg].dnSpeakers,allIn,oneIn) &&
			isVerbClass(sr->whereVerb,L"chase"))
	{
		for (int si=0; si<(signed)subjects.size(); si++)
			if (speakerGroups[sg].povSpeakers.find(subjects[si].object)!=speakerGroups[sg].povSpeakers.end())
			{
				converted=true;
				speakerGroups[sg].observers.insert(subjects[si].object);
				wstring tmpstr,tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION|LOG_SG,L"OBS observer %s converted from POV [follow] in speakerGroup %s.",objectString(subjects[si].object,tmpstr,true).c_str(),toText(speakerGroups[sg],tmpstr2));
			}
	}
	return converted;
}

bool cSource::isSpeaker(int where,int esg,int tempCSG)
{ LFS
	bool isSpeaker=false,allIn,oneIn;
	if (where>=0)
	{
		vector <cSpeakerGroup>::iterator csg=speakerGroups.begin()+tempCSG;
		isSpeaker|=m[where].getObject()>=0 && objects[m[where].getObject()].numIdentifiedAsSpeaker>0;
		isSpeaker|=(currentSpeakerGroup>1 && intersect(where,csg->speakers,allIn,oneIn));
		isSpeaker|=(m[where].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON))!=0;
		isSpeaker|=(esg>=0 && ((unsigned)esg)<csg->embeddedSpeakerGroups.size() && intersect(where,csg->embeddedSpeakerGroups[esg].speakers,allIn,oneIn));
		isSpeaker|=(isVoice(where) && m[where].getObject()>=0 && objects[m[where].getObject()].getOwnerWhere()>=0 &&
		  (currentSpeakerGroup>0 && intersect(objects[m[where].getObject()].getOwnerWhere(),csg->speakers,allIn,oneIn)));
		for (int oi=0; oi<(signed)m[where].objectMatches.size() && !isSpeaker; oi++) 
			isSpeaker|=objects[m[where].objectMatches[oi].object].numIdentifiedAsSpeaker>0;
	}
	return isSpeaker;
}

void cSource::srSetTimeFlowTense(int spri)
{ LFS
	if (syntacticRelationGroups[spri].agentLocationRelationSet) return;
	vector <cSyntacticRelationGroup>::iterator sr=syntacticRelationGroups.begin()+spri;
	if (setTimeFlowTense(sr->where,sr->whereControllingEntity,sr->whereSubject,sr->whereVerb,sr->whereObject,
sr->wherePrepObject,
			sr->prepObjectSubType,sr->objectSubType,sr->establishingLocation || sr->relationType==-stLOCATION,sr->futureLocation,sr->genderedLocationRelation,sr->tft))
	{
		sr->agentLocationRelationSet=true;
		if (currentSpeakerGroup<speakerGroups.size())
		{
			followerPOVToObserverConversion(sr,currentSpeakerGroup);
			// if the current space relation is within the current speaker group, search the next one (otherwise skip).
			//   a command like "follow them" would most likely be followed by another speaker group.
			if (speakerGroups[currentSpeakerGroup].sgBegin<sr->where)
				followerPOVToObserverConversion(sr,currentSpeakerGroup+1);
		}
	}
	appendTime(sr);
}

// returns true if VT_NEGATIVE
bool cSource::setTimeFlowTense(int where,int whereControllingEntity,int whereSubject,int whereVerb,int whereObject,
int wherePrepObject,
	int prepObjectSubType,int objectSubType,bool establishingLocation,bool futureLocation,bool genderedLocationRelation,cTimeFlowTense &tft)
{ LFS
	int location=where,esg=-1,tmp;
	vector <cSpeakerGroup>::iterator csg=speakerGroups.begin()+currentSpeakerGroup;
	if (currentSpeakerGroup<speakerGroups.size())
	{
		for (esg=0; esg<(signed)csg->embeddedSpeakerGroups.size(); esg++)
			if (csg->embeddedSpeakerGroups[esg].sgBegin<location && csg->embeddedSpeakerGroups[esg].sgEnd>location)
				break;
		if (esg==csg->embeddedSpeakerGroups.size()) esg=-1;
	}
	// We are in Lyon's.
	// You are standing in the most important building in Egypt.
	// NOT ESTAB I[tuppence] always help old ladies over crossings
	// NOT You tread on her foot
	// vB                               has examined                       VT_PRESENT_PERFECT                                  E<S=R
	// vrB															having examined                    VT_PRESENT_PERFECT+VT_VERB_CLAUSE                   E<S=R
	// vC                               is examining                       VT_EXTENDED+ VT_PRESENT                             S=R E<=>R
	// vCD                              is being examined                  VT_PASSIVE+ VT_PRESENT+VT_EXTENDED
	tft.speakerCommand=whereVerb>=0 && ((m[whereVerb].objectRole&IN_PRIMARY_QUOTE_ROLE) && 
		 (whereSubject<0 || (m[whereSubject].word->first==L"you")) && 
		(m[whereVerb].verbSense&(VT_TENSE_MASK|VT_EXTENDED))==VT_PRESENT) &&
			(m[whereVerb].word->second.inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR))!=0 &&  // NOT / calls herself Rita.
		!(m[whereVerb].flags&(cWordMatch::flagInInfinitivePhrase)) &&
		(!(m[whereVerb].flags&(cWordMatch::flagInQuestion))); //  || isEOS(whereVerb-1) || m[whereVerb-1].queryForm(quoteForm)>=0
	tft.speakerQuestionToAudience=whereVerb>=0 && ((m[whereVerb].objectRole&IN_PRIMARY_QUOTE_ROLE) && 
		(whereSubject<0 || (m[whereSubject].word->first==L"you")) && 
		(m[whereVerb].verbSense&(VT_TENSE_MASK|VT_EXTENDED))==VT_PAST) &&
		!(m[whereVerb].flags&(cWordMatch::flagInInfinitivePhrase)) &&
		((m[whereVerb].flags&(cWordMatch::flagInQuestion))); //  || isEOS(whereVerb-1) || m[whereVerb-1].queryForm(quoteForm)>=0
	tft.significantRelation=((genderedLocationRelation && currentSpeakerGroup<speakerGroups.size() &&
		  (isSpeaker(whereSubject,esg,currentSpeakerGroup) || isSpeaker(whereObject,esg,currentSpeakerGroup) || isSpeaker(whereControllingEntity,esg,currentSpeakerGroup) || 
			 (whereSubject>=0 && m[whereSubject].getObject()>=0 && isAgentObject(m[whereSubject].getObject())))) || 
			establishingLocation || futureLocation || tft.speakerCommand || tft.speakerQuestionToAudience);
	//if (tft.significantRelation || !forSyntacticGroupRelation)
	{
		int vt=(whereVerb>=0) ? m[whereVerb].verbSense : 0;
		if (whereControllingEntity>=0 && m[whereControllingEntity].getRelVerb()>=0 && m[whereControllingEntity].getRelVerb()!=whereVerb)
			vt=m[tmp=m[whereControllingEntity].getRelVerb()].verbSense;
		// if infinitive phrase, take the tense of the main clause
		else if (whereVerb>=0 && (m[whereVerb].flags&cWordMatch::flagInInfinitivePhrase))
		{
			if (whereSubject>=0 && m[whereSubject].getRelVerb()>=0 && m[whereSubject].getRelVerb()!=whereVerb)
				vt=m[m[whereSubject].getRelVerb()].verbSense;
			if (m[whereVerb].previousCompoundPartObject>=0)
				vt=m[m[whereVerb].previousCompoundPartObject].verbSense;
		}
		if (vt==-1) vt=0;
		bool inStory=(m[location].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE)!=0;
		bool inPrimaryQuote=(m[location].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0 && !inStory;
		bool inSecondaryQuote=(m[location].objectRole&IN_SECONDARY_QUOTE_ROLE)!=0;
		//if ((vt&VT_NEGATION) && forSyntacticRelationGroup)
		//	return false;
		// narration (which talks about what is presently happening in the past tense) OR 
		bool presentlyHappeningSR=!inSecondaryQuote && whereVerb>=0 &&
			 ((!inPrimaryQuote && whereSubject>=0 && !(m[whereSubject].objectRole&NONPAST_OBJECT_ROLE)) ||
				(!inPrimaryQuote && !(vt&VT_POSSIBLE) && (((vt&VT_TENSE_MASK)==VT_PAST || (vt&VT_TENSE_MASK)==VT_EXTENDED+ VT_PAST || (vt&VT_TENSE_MASK)==VT_PASSIVE+ VT_PAST || (vt&VT_TENSE_MASK)==VT_PASSIVE+ VT_PAST+VT_EXTENDED))) ||
			 // talking about what is happening
				(inPrimaryQuote && whereVerb>=0 && 
				 ((m[whereVerb].queryWinnerForm(isForm)>=0 && (vt&VT_TENSE_MASK)==VT_PRESENT) || (vt&~VT_VERB_CLAUSE)==VT_EXTENDED+ VT_PRESENT || (vt&~VT_VERB_CLAUSE)==VT_PASSIVE+ VT_PRESENT+VT_EXTENDED)));
		// talking about what happened in the past
		bool beforePastHappeningSR=(inSecondaryQuote || inPrimaryQuote) && (vt&VT_TENSE_MASK)==VT_PAST_PERFECT && !(vt&VT_POSSIBLE);
// vB                               has examined                       VT_PRESENT_PERFECT                                  E<S=R
// vBC                              has been examining                 VT_EXTENDED+ VT_PRESENT_PERFECT                     E<S=R
// vrBC                             having been examining              VT_EXTENDED+ VT_PRESENT_PERFECT+VT_VERB_CLAUSE      E<S=R
// vBD                              has been examined                  VT_PASSIVE+ VT_PRESENT_PERFECT
// vrBD                             having been examined               VT_PASSIVE+ VT_PRESENT_PERFECT+VT_VERB_CLAUSE
// vBCD                             has been being examined            VT_PASSIVE+ VT_PRESENT_PERFECT+VT_EXTENDED
// vAD+conditional                  may be examined                    VT_POSSIBLE+ VT_PRESENT_PERFECT+VT_PASSIVE
// vABC+conditional                 may have been examining            VT_POSSIBLE+ VT_PRESENT_PERFECT+VT_EXTENDED

// vS+past                          examined                           VT_PAST                                             R=E<S
// vC+past													was examining                      VT_EXTENDED+ VT_PAST                                R=E<S
// vD+past													was examined                       VT_PASSIVE+ VT_PAST
// vCD+past                         was being examined                 VT_PASSIVE+ VT_PAST+VT_EXTENDED
// vAB+conditional                  may have examined                  VT_POSSIBLE+ VT_PAST
// vABD+conditional                 may have been examined             VT_POSSIBLE+ VT_PAST+VT_PASSIVE
// vABCD+conditional                may have been being examined       VT_POSSIBLE+ VT_PAST+VT_PASSIVE+VT_EXTENDED
  		bool pastHappeningSR=(inSecondaryQuote || inPrimaryQuote) && ((vt&VT_TENSE_MASK)==VT_PAST || (vt&VT_TENSE_MASK)==VT_PRESENT_PERFECT);
		pastHappeningSR|=!(inSecondaryQuote || inPrimaryQuote) && (vt&VT_TENSE_MASK)==VT_PAST_PERFECT;
		// talking about what will happen in the future
		bool futureHappeningSR=((vt&VT_TENSE_MASK)==VT_FUTURE || (vt&VT_TENSE_MASK)==VT_FUTURE_PERFECT);
		// 'the house may go on the market' but NOT 'she may be dead' - the second phrase is in reference to what is a present state (or perhaps even past state).
		futureHappeningSR|=futureLocation || tft.speakerCommand || ((vt&VT_POSSIBLE) && whereVerb>=0 && !isVerbClass(whereVerb,L"am"));
		bool futureInPastHappeningSR=false;
		if (whereSubject>=0 && whereVerb>=0 && m[whereSubject].getRelVerb()==whereVerb && (m[whereVerb].flags&cWordMatch::flagInInfinitivePhrase))
		{
			m[whereVerb].flags&=~cWordMatch::flagInInfinitivePhrase;
			if (debugTrace.traceWhere)
			lplog(LOG_WHERE,L"%06d:infinitive phrase cancelled.",whereVerb,whereSubject,m[whereSubject].getRelVerb());
		}
		// I happened to overhear you - happened is an occurrence, and so is dominant over the infinitive phrase
		if (whereVerb>=0 && (m[whereVerb].flags&cWordMatch::flagInInfinitivePhrase) && whereSubject>=0 && m[whereSubject].getRelVerb()>=0 && !isVerbClass(m[whereSubject].getRelVerb(),L"occurrence-48.3"))
		{
			if ((beforePastHappeningSR || pastHappeningSR) && !futureHappeningSR)
			{
				futureInPastHappeningSR=true;
				beforePastHappeningSR=pastHappeningSR=false;
			}
			else
				futureHappeningSR=true;
		}
		// lastOpeningPrimaryQuote is set at the beginning of the section, and this goes through a section, so update it based on location
		bool story=false,moveFromPastToFuture=false;
		if (lastOpeningPrimaryQuote>=0 && (inPrimaryQuote || narrativeIsQuoted))
		{
			int lOPQ=lastOpeningPrimaryQuote;
			for (; m[lOPQ].quoteBackLink>=0 && lOPQ>location; lOPQ=m[lOPQ].quoteBackLink);
			for (; lOPQ>location; lOPQ=m[lOPQ].previousQuote);
			for (; lOPQ>=0 && m[lOPQ].getQuoteForwardLink()<location && m[lOPQ].getQuoteForwardLink()>=0; lOPQ=m[lOPQ].getQuoteForwardLink());
			tft.lastOpeningPrimaryQuote=lOPQ;
			story=lOPQ>=0 && (m[lOPQ].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers)!=0;
		}
		else
			tft.lastOpeningPrimaryQuote=-1;
		// ESTABUshered into the presence of Mr . Carter , he[carter] and I[tommy] *wish* each other good morning as is customary
		bool monitorPresentToFutureHappeningSR=futureHappeningSR;
		futureHappeningSR|=(inPrimaryQuote && !presentlyHappeningSR && !pastHappeningSR && (vt&~VT_VERB_CLAUSE)==VT_PRESENT) && 
			whereSubject>=0 && whereVerb>=0 && (m[whereVerb].queryForm(thinkForm)<0) && !(m[whereSubject].flags&cWordMatch::flagInQuestion);
		monitorPresentToFutureHappeningSR=(futureHappeningSR && !monitorPresentToFutureHappeningSR);
		// ESTABUshered into the presence of Mr . Carter , he[carter] and I[tommy] wish each other good morning as is customary
		// BUT NOT /  MOVETook a car across the town -- mighty pretty place by the way , I[julius] guess MOVE_OBJECTI[julius] shall take Jane there for a spell when CONTACTI[julius] find her[jane]
		if (inPrimaryQuote && pastHappeningSR && whereSubject>whereVerb && m[whereSubject].getRelVerb()>=0 && 
			  (m[m[whereSubject].getRelVerb()].verbSense&(VT_TENSE_MASK|VT_EXTENDED))==VT_PRESENT && !story)
			moveFromPastToFuture=true;
		// it is time I strolled around to the Ritz. - future happening even though the tense is past (inPrimaryQuote)
		else if (whereSubject>1 && (m[whereSubject-1].word->first==L"time") && (m[whereSubject-2].getMainEntry()->first==L"am") && pastHappeningSR)
			moveFromPastToFuture=true;
		// when I come out, / when he comes out of the building I will ...
		else if ((inSecondaryQuote || inPrimaryQuote) && (vt&VT_TENSE_MASK)==VT_PRESENT && whereSubject>0 && 
			       (m[whereSubject-1].word->first==L"when" || m[whereSubject-1].word->first==L"till" || 
						  m[whereSubject-1].word->first==L"until" || m[whereSubject-1].word->first==L"if" || 
							m[whereSubject-1].word->first==L"let" || // let us go to lunch!
							m[whereSubject-1].word->first==L"before" || m[whereSubject-1].word->first==L"after" ||
							m[whereSubject-1].word->first==L"then"))
			moveFromPastToFuture=true;
		else
			story|=(((vt&VT_TENSE_MASK)==VT_PAST || (vt&VT_TENSE_MASK)==VT_PAST_PERFECT) && inPrimaryQuote);
		if (moveFromPastToFuture)
		{
			pastHappeningSR=false;
		  futureHappeningSR=true;
			monitorPresentToFutureHappeningSR=false;
		}
		if (monitorPresentToFutureHappeningSR)
		{
			int maxSR=max(location,whereObject);
			maxSR=max(maxSR,whereVerb);
			maxSR=max(maxSR,wherePrepObject);
			wstring s;
			for (int w=location-4; w<=maxSR+4 && ((unsigned)w)<m.size(); w++) 
			{
				if (w==whereControllingEntity) s+=L"[CONTROL]";
				if (w==whereSubject) s+=L"[SUBJECT]";
				if (w==whereVerb) s+=L"[VERB]";
				if (w==whereObject) s+=L"[OBJECT]";
				s+=m[w].word->first+L" ";
			}
			bool inRelativeClause=(m[whereSubject].objectRole&(EXTENDED_ENCLOSING_ROLE|NONPAST_ENCLOSING_ROLE|NONPRESENT_ENCLOSING_ROLE|SENTENCE_IN_REL_ROLE|SENTENCE_IN_ALT_REL_ROLE))!=0;
			if (logDetail)
				lplog(LOG_RESOLUTION,L"%06d:FUTURE [%s]%s?",location,s.c_str(),(inRelativeClause) ? L"*":L"");
		}
		presentlyHappeningSR|=(establishingLocation && !inPrimaryQuote && !inSecondaryQuote);
		pastHappeningSR|=(establishingLocation && inPrimaryQuote);
		int maxSR=max(where,whereObject);
		maxSR=max(maxSR,whereVerb);
		maxSR=max(maxSR,wherePrepObject);
		/*
		wstring s; 	
		for (int w=where-4; w<=maxSR+4; w++) 
		{
			if (w==whereControllingEntity) s+=L"[CONTROL]";
			if (w==whereSubject) s+=L"[SUBJECT]";
			if (w==whereVerb) s+=L"[VERB]";
			if (w==whereObject) s+=L"[OBJECT]";
			s+=m[w].word->first+L" ";
		}
		*/
		int maxWO=max(whereObject,whereSubject);
		if (m[maxWO].getObject()<0 && whereSubject>=0)
			maxWO=whereSubject;
		if (maxWO>=0)
		{ 
			if (m[maxWO].objectRole&(SENTENCE_IN_REL_ROLE|SENTENCE_IN_ALT_REL_ROLE))
				tft.presType+=L"[REL]";
			if (m[maxWO].objectRole&IN_COMMAND_OBJECT_ROLE)
				tft.presType+=L"[COMMAND]";
			if (m[maxWO].flags&cWordMatch::flagInQuestion)
				tft.presType+=L"[Q]";
			if (m[maxWO].flags&cWordMatch::flagInPStatement)
				tft.presType+=L"[P]";
		}
		if (whereSubject>=0 && m[whereSubject].beginObjectPosition>0 && m[m[whereSubject].beginObjectPosition-1].queryForm(conjunctionForm)!=-1 &&
			  (m[m[whereSubject].beginObjectPosition-1].word->second.timeFlags&(T_BEFORE|T_AFTER))!=0)
			tft.presType+=L"[BEFORE/AFTER_CONJ]";					
		if (whereControllingEntity>=0 && m[whereControllingEntity].getRelVerb()>=0)
		{
			wstring verb;
			unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(m[whereControllingEntity].getRelVerb(),verb);
			if (lvtoCi!=vbNetVerbToClassMap.end())
				for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
				{
					wstring id=vbNetClasses[*vbi].name();
					if (like(verb,L"have") || like(id,L"want") || like(id,L"wish")) // id of 'have' is 'own' which is the wrong sense - we want to be more specific
					{
						tft.presType+=L"[FUTURE_DESIRE]";					
						break;
					}
					else if ((like(id,L"tell") || like(id,L"beg") || like(id,L"urge")) && whereObject>=0)
					{
						tft.presType+=L"[COMMAND_INDIRECT]";					
						break;
					}
				}
		}
		// is the subject moving within a local space, like a room, or is the subject moving from a space to another, like from the building to the street?
		tft.beyondLocalSpace=(prepObjectSubType!=GEOGRAPHICAL_URBAN_SUBSUBFEATURE && (prepObjectSubType>=0 || objectSubType!=GEOGRAPHICAL_URBAN_SUBSUBFEATURE));
		tft.futureHappening=futureHappeningSR;
		tft.futureInPastHappening=futureInPastHappeningSR;
		tft.pastHappening=pastHappeningSR;
		tft.beforePastHappening=beforePastHappeningSR;
		tft.presentHappening=presentlyHappeningSR;
		tft.story=story;
		tft.timeTransition=false;
		tft.negation=(vt&VT_NEGATION)!=0;
		return true;
	}
}

bool cSource::isRelativeLocation(wstring word)
{ LFS
		bool location=false;
		// in front of the hotel / at the end of the street / at the angle of the corner / at the bend of the staircase    
		wchar_t *locations[]={L"front",L"middle",L"side",L"back",L"end",L"corner",L"angle",L"bend",L"curve",L"top",NULL};
		for (int p=0; locations[p] && !(location=(word==locations[p])); p++);
		return location;
}

void cSource::insertCompoundObjects(int wo,set <int> &relPreps, unordered_map <int,int> &principalObjectEndPoints)
{ LFS
	int numLoops = 0;
	for (; wo>=0 && numLoops<30; wo=m[wo].nextCompoundPartObject, numLoops++)
	{
		if (m[wo].endObjectPosition>=0)
			principalObjectEndPoints[m[wo].endObjectPosition]=wo;
		for (int wp=m[wo].relPrep; wp>=0 && wp+1<(int)m.size() && numLoops<30; wp=m[wp].relPrep, numLoops++)
		{
			if (relPreps.find(wp)!=relPreps.end())
				break;
			relPreps.insert(wp);
		}
	}
}

/*
CASE 1. he is going to make a mess of things.
    referencingEntity=-1.  verbIndex@966, secondaryVerbIndex@968, subject@964
964: he:relVerb=966
965: ishas:
966: going:relSubject=964 relVerb=968
967: to:
968: make:relSubject=964 relObject=970 relPrep=971 previousCompoundPartObject=966
969: a:
970: mess:relSubject=964 relVerb=968 relPrep=971
971: of:relVerb=968 relObject=972
972: things:relPrep=971 relNextObject=970

CASE 2. I felt I could not possibly remain at close quarters
  referencingEntity@4454. subject@4456. verbIndex@4455. secondaryVerbIndex@4460.
4454: i:relVerb=4455 relInternalObject=4456
4455: felt:relSubject=4454 relInternalObject=4456
4456: i:relVerb=4460 relPrep=4482
4457: could:relSubject=4456
4458: not:
4459: possibly:
4460: remain:relSubject=4456 relPrep=4461
4461: at:relVerb=4460 relObject=4463 relPrep=4464
4462: close:
4463: quarters:relPrep=4461

I want you to spread yourself this evening
  referencingEntity@8763. subject@8765. verbIndex@8764. secondaryVerbIndex@8767.
8763: i:relVerb=8764 relObject=8765
8764: want:relSubject=8763 relVerb=8767 relObject=8765
8765: you:relSubject=8763 relVerb=8764 relInternalVerb=8767
8766: to:
8767: spread:relSubject=8765 relObject=8768 previousCompoundPartObject=8764
8768: yourself:relVerb=8767
8769: this:
8770: evening:

*/
// will change source.m (invalidate all iterators)
void cSource::correctSRIEntry(cSyntacticRelationGroup &sri)
{ LFS
	if (sri.relationType==stNORELATION)
	{
		if (sri.whereSubject<0 && m[sri.where].relSubject<0)
			sri.whereSubject=sri.where;
		else if (sri.whereObject<0 && m[sri.where].getRelObject()<0)
			sri.whereObject=sri.where;
		if (sri.whereVerb<0 && m[sri.where].getRelVerb()>=0)
			sri.whereVerb=m[sri.where].getRelVerb();
	}
	// CASE 1. he is going to make a mess of things.
	if (sri.whereControllingEntity==sri.whereSubject && sri.whereSubject>=0) // CASE 1
	{
		sri.whereControllingEntity=-1;
		sri.whereObject=m[sri.whereSubject].getRelObject();
	}
	// CASE 2. I felt I could not possibly remain at close quarters
	// but not 'What is Danny's profession?'
	if (sri.whereControllingEntity<0 && sri.whereSubject>=0 && m[sri.whereSubject].relInternalObject>=0 && m[sri.whereSubject].getRelVerb()>=0 // CASE 2
		 && sri.whereSubject<sri.whereVerb) // not a question, where the subject and object are inverted - subject must come before verb.
	{
		sri.whereControllingEntity=sri.whereSubject;
		sri.whereSubject=m[sri.whereSubject].relInternalObject;
		sri.whereVerb=m[sri.whereSubject].getRelVerb();
		if (sri.whereVerb<0)
			sri.whereObject=-1;
		else
			sri.whereObject=m[sri.whereVerb].getRelObject();
	}
	if (sri.whereControllingEntity<0 && sri.whereObject>=0 && m[sri.whereObject].relInternalVerb>=0) // CASE 3
	{
		sri.whereControllingEntity=sri.whereSubject;
		sri.whereSubject=sri.whereObject;
		int saveWhereObject=m[sri.whereSubject].getRelObject();
		sri.whereVerb=m[sri.whereObject].relInternalVerb;
		sri.whereObject=saveWhereObject;
	}
	// CASE 4. is he planning to marry?
	if (sri.whereControllingEntity<0 && sri.whereSubject>=0 && sri.whereVerb>=0 && m[sri.whereVerb].previousCompoundPartObject>=0 && (m[sri.whereSubject].flags&cWordMatch::flagInQuestion)) // CASE 4
	{
		sri.whereObject=m[sri.whereSubject].getRelObject();
		sri.whereVerb=m[sri.whereSubject].getRelVerb();
	}
	if (sri.whereControllingEntity>=0 && m[sri.whereControllingEntity].getRelVerb()==sri.whereVerb && sri.whereSubject>=0) // CASE 5
	{
		sri.whereVerb=m[sri.whereSubject].getRelVerb();
		if (m[sri.whereControllingEntity].getRelVerb()==sri.whereVerb)
		{
			sri.whereVerb=m[sri.whereSubject].relInternalVerb;
			if (sri.whereVerb<0)
			{
				sri.whereSubject=sri.whereControllingEntity;
				sri.whereControllingEntity=-1;
				sri.whereObject=m[sri.whereSubject].getRelObject();
				sri.whereVerb=m[sri.whereSubject].getRelVerb();
			}
		}
	}
	if (sri.whereSubject>=0 && sri.whereVerb<0) 
	{
		sri.whereVerb=m[sri.whereSubject].getRelVerb();
	}
	if (sri.whereObject<0 && sri.whereVerb>=0) 
		sri.whereObject=m[sri.whereVerb].getRelObject();
	// How many years transpired between world championships for the winning team ? CASE 6
	if (sri.whereControllingEntity<0 && sri.whereSubject>=0 && m[sri.whereSubject].queryWinnerForm(relativizerForm)!=-1 && 
			sri.whereVerb>=0 && m[sri.whereVerb].relSubject>=0 && m[sri.whereVerb].relSubject!=sri.whereSubject)
			sri.whereSubject=m[sri.whereVerb].relSubject;
	if (sri.wherePrep<0 && sri.whereVerb>=0) 
		sri.wherePrep=m[sri.whereVerb].relPrep;
	if (sri.wherePrep>=0 && sri.wherePrepObject<0)
		sri.wherePrepObject=m[sri.wherePrep].getRelObject();
	if (sri.whereObject==sri.whereSubject && sri.whereVerb>=0)
	{
		sri.whereObject=m[sri.whereVerb].getRelObject();
		sri.whereSubject=m[sri.whereVerb].relSubject;
	}
	// On what date did the court begin screening potential jurors? CASE 7
	sri.changeStateAdverb=false;
	if (sri.whereVerb>=0 && m[sri.whereVerb].getRelVerb()<0 && sri.whereVerb+1<(signed)m.size() && (m[sri.whereVerb+1].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE)!=0)
	{
		int timeFlag=(m[sri.whereVerb].word->second.timeFlags&31);
		if (timeFlag==T_START || timeFlag==T_STOP || timeFlag==T_FINISH || timeFlag==T_RESUME)
		{
			sri.whereVerb++;
			if (sri.whereObject>=0 && m[sri.whereObject].beginObjectPosition==sri.whereVerb)
			{
				m[sri.whereObject].beginObjectPosition++;
				m[m[sri.whereObject].beginObjectPosition].principalWherePosition=m[m[sri.whereObject].beginObjectPosition-1].principalWherePosition;
				m[m[sri.whereObject].beginObjectPosition-1].principalWherePosition=-1;
				sri.changeStateAdverb=true;
			}
		}
	}
	if (sri.whereVerb>=0 && m[sri.whereVerb].getRelVerb()>=0 && m[m[sri.whereVerb].getRelVerb()].previousCompoundPartObject==sri.whereVerb) // CASE 8
	{
		sri.whereSecondaryVerb=m[sri.whereVerb].getRelVerb();
		sri.whereSecondaryObject=m[m[sri.whereVerb].getRelVerb()].getRelObject();
		if (sri.whereObject==sri.whereSecondaryObject)
			sri.whereObject=-1;
		if (sri.whereSecondaryObject>=0)
			sri.whereNextSecondaryObject=m[sri.whereSecondaryObject].relNextObject;
	}
	sri.whereSecondaryPrep=(sri.wherePrep>=0) ? m[sri.wherePrep].relPrep : -1;
	if (sri.whereSecondaryPrep<0 && sri.whereVerb>=0 && sri.wherePrep!=m[sri.whereVerb].relPrep)
	{
		sri.whereSecondaryPrep=sri.wherePrep;
		sri.wherePrep=m[sri.whereVerb].relPrep;
		if (sri.wherePrep>=0) sri.wherePrepObject=m[sri.wherePrep].getRelObject();
	}
	if (sri.whereSecondaryPrep<0 && sri.whereSecondaryVerb>=0 && sri.wherePrep!=m[sri.whereSecondaryVerb].relPrep)
		sri.whereSecondaryPrep=m[sri.whereSecondaryVerb].relPrep;
	transformQuestionRelation(sri);
	// sets relNextObject of a prep to be the object immediately before the preposition
	// sets principalWherePosition of a prep to be the object which is at the head of a chain of prepositions, which is itself not an object of a preposition
	int ro;
	set <int> relPreps;
	unordered_map <int,int> principalObjectEndPoints;
	getAllPreps(&sri,relPreps,-1);
	insertCompoundObjects(sri.whereObject,relPreps,principalObjectEndPoints);
	if (sri.whereObject>=0)
		insertCompoundObjects(m[sri.whereObject].relNextObject,relPreps,principalObjectEndPoints);
	insertCompoundObjects(sri.whereControllingEntity,relPreps,principalObjectEndPoints);
	insertCompoundObjects(sri.whereSubject,relPreps,principalObjectEndPoints);
	insertCompoundObjects(sri.whereSecondaryObject,relPreps,principalObjectEndPoints);
	insertCompoundObjects(sri.whereNextSecondaryObject,relPreps,principalObjectEndPoints);
	int whereLastPrep=-1,whereLastPrincipalObject=-1;
	for (set <int>::iterator rpi=relPreps.begin(),rpiEnd=relPreps.end(); rpi!=rpiEnd; rpi++)
	{
		if (whereLastPrep>=0 && (ro=m[whereLastPrep].getRelObject())>=0 && (m[ro].endObjectPosition==*rpi || ((ro=m[ro].nextCompoundPartObject)>=0 && m[ro].endObjectPosition==*rpi)))
			m[*rpi].relNextObject=ro;
		else
		{
			unordered_map <int,int>::iterator pOEPi=principalObjectEndPoints.find(*rpi);
			if (pOEPi!=principalObjectEndPoints.end())
				whereLastPrincipalObject=pOEPi->second;
			else
				whereLastPrincipalObject=-1;
		}
		m[*rpi].nextQuote=whereLastPrincipalObject;
		whereLastPrep=*rpi;
	}
	getSRIMinMax(&sri);
	//wstring tmpstr;
	//phraseString(sri.printMin,sri.printMax,tmpstr,true);
}

// will change source.m (invalidate all iterators)
void cSource::newSR(int where,int _o,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int wherePrepObject,int whereMovingRelativeTo,int relationType,const wchar_t *whereType,bool physicalRelation)
{ LFS
	if (whereSubject>=0 && m[whereSubject].queryWinnerForm(prepositionForm)>=0 && m[whereSubject].queryWinnerForm(nounForm)<0)
	{
		lplog(LOG_ERROR,L"%d:subject@%d is preposition!",where,whereSubject);
		return;
	}
	if (wherePrepObject>=0 && wherePrep==-1)
		lplog(LOG_ERROR,L"%d:subject@%d wherePrepObject=%d but no prep!",where,whereSubject,wherePrepObject);

	bool found=false,convertToStay=(relationType==stENTER && wherePrep>=0 && m[wherePrep].word->first==L"to" && wherePrepObject>=0 && (hasHyperNym(m[wherePrepObject].word->first,L"inaction",found,false) || found));
	if (speakerGroupsEstablished && m[where].hasSyntacticRelationGroup && !convertToStay) 
	{
		cSyntacticRelationGroup sr(where,_o,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wherePrepObject,whereMovingRelativeTo,relationType,false,false,-1,-1,physicalRelation);
		vector <cSyntacticRelationGroup>::iterator location = lower_bound(syntacticRelationGroups.begin(), syntacticRelationGroups.end(), sr, comparesr),sl=location;
		for (; location!=syntacticRelationGroups.end() && location->where==where; location++)
		{
			if (location->relationType!=relationType && 
					location->relationType!=stPREPTIME && location->relationType!=stPREPDATE && location->relationType!=stSUBJDAYOFMONTHTIME &&
				 location->relationType!=stABSTIME && location->relationType!=stABSDATE && location->relationType!=stADVERBTIME &&
				!whereSubType(whereSubject))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:relationType changed from %s to %s",where,getRelStr(location->relationType),getRelStr(relationType));
				location->relationType=relationType;
			}
			srSetTimeFlowTense((int) (location-syntacticRelationGroups.begin()));
		}
		if (sl!=syntacticRelationGroups.end() && physicalRelation && !sl->physicalRelation)
			sl->physicalRelation=true;
		if (sl!=syntacticRelationGroups.end() && sl->where==where && !sl->tft.futureHappening && !sl->tft.pastHappening)
		{
			vector <cLocalFocus>::iterator lsi;
			if (relationType==stENTER && !(m[whereVerb].objectRole&IN_PRIMARY_QUOTE_ROLE) && whereSubject>=0)
			{
				int so=m[whereSubject].getObject();
				if (m[whereSubject].objectMatches.size()>=1)
					so=m[whereSubject].objectMatches[0].object;
				if (so>=0 && (lsi=in(so))!=localObjects.end() && lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent<where)
				{
					sl->relationType=stMOVE;
					wstring tmpstr;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Already PP object %s entering - changed to move",where,whereString(whereSubject,tmpstr,false).c_str());
				}
			}
			return;
		}
	}
	bool shouldLogSyntacticRelationGroup=false,genderedEntityMove=false,convertToMove=false;
	wstring tmpstr,tmpstr2,tmpstr3,tmpstr4,tmpstr5,tmpstr6;
	// came to a halt
	if (convertToStay) relationType=stSTAY;
	// convert enter to move, if the subject is already physically present.
	if (speakerGroupsEstablished && !convertToStay && relationType==stENTER && whereSubject>=0)
	{
		if (convertToMove=(m[whereSubject].word->second.inflectionFlags&FIRST_PERSON)==FIRST_PERSON)
		{
			relationType=stMOVE;
			if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:Already PP object %s entering - changed to move (2)",where,whereString(whereSubject,tmpstr,false).c_str());
		}
		else if (!(m[whereVerb].objectRole&IN_PRIMARY_QUOTE_ROLE))
		{
			vector <cLocalFocus>::iterator lsi;
			int so=m[whereSubject].getObject();
			if (m[whereSubject].objectMatches.size()>=1)
				so=m[whereSubject].objectMatches[0].object;
			if (convertToMove=so>=0 && (lsi=in(so))!=localObjects.end() && lsi->physicallyPresent)
			{
				relationType=stMOVE;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Already PP object %s entering - changed to move (3)",where,whereString(whereSubject,tmpstr,false).c_str());
			}
		}
	}
	int so=-1,po=-1,o=-1,ce=-1;
	bool subjectGendered=false;
	if (whereSubject>=0) 
	{
		so=(m[whereSubject].objectMatches.size()==1) ? m[whereSubject].objectMatches[0].object : m[whereSubject].getObject();
		subjectGendered=m[whereSubject].objectMatches.empty() && so>=0 && (objects[so].male || objects[so].female);
		for (int si=0; si<(signed)m[whereSubject].objectMatches.size(); si++)
			subjectGendered|=(objects[m[whereSubject].objectMatches[si].object].male || objects[m[whereSubject].objectMatches[si].object].female);
		// one, two, three, go!
		if ((whereSubject+2<(signed)m.size() && m[whereSubject].word->first==L"two" && m[whereSubject+2].word->first==L"three") ||
				(whereSubject+2<(signed)m.size() && m[whereSubject].word->first==L"one" && m[whereSubject+2].word->first==L"two") ||
				(whereSubject>=2 && m[whereSubject-2].word->first==L"two" && m[whereSubject].word->first==L"three"))
			subjectGendered=false;
	}	
	else subjectGendered=(whereVerb>=0 && m[whereVerb].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0 && (m[whereVerb].verbSense&VT_TENSE_MASK)==VT_PRESENT && !(m[whereVerb].flags&cWordMatch::flagInInfinitivePhrase);
	bool controllerGendered=false;
	if (whereControllingEntity>=0) 
	{
		ce=(m[whereControllingEntity].objectMatches.size()==1) ? m[whereControllingEntity].objectMatches[0].object : m[whereControllingEntity].getObject();
		controllerGendered=m[whereControllingEntity].objectMatches.empty() && ce>=0 && (objects[ce].male || objects[ce].female);
		for (int si=0; si<(signed)m[whereControllingEntity].objectMatches.size(); si++)
			controllerGendered|=(objects[m[whereControllingEntity].objectMatches[si].object].male || objects[m[whereControllingEntity].objectMatches[si].object].female);
	}	
	if (wherePrepObject>=0) po=(m[wherePrepObject].objectMatches.size()==1) ? m[wherePrepObject].objectMatches[0].object : m[wherePrepObject].getObject();
	int prepObjectSubType=(po>=0) ? objects[po].getSubType() : -1;
	if (prepObjectSubType<0 && po>=0 && objects[po].isPossibleSubType(false)) prepObjectSubType=UNKNOWN_PLACE_SUBTYPE;
	bool prepTypeCancelled;
	int relPrep=-1,relObject=-1;
	// filter out 'state of confusion' but keep 
	// 'she ran to the door of No. 20.'
	if (prepTypeCancelled=prepObjectSubType>=0 && (relPrep=m[wherePrepObject].endObjectPosition)<(signed)m.size() && relPrep>=0 &&
		  m[relPrep].word->first==L"of" && (relObject=m[relPrep].getRelObject())>=0 && 
		  (m[relObject].getObject()<0 || (objects[m[relObject].getObject()].getSubType()<0 && m[relObject].queryForm(NUMBER_FORM_NUM)<0 && !isAgentObject(m[relObject].getObject()))))
		prepObjectSubType=-1;
	if (whereMovingRelativeTo<0 && relObject>=0 && m[relObject].getObject()>=0 && (objects[m[relObject].getObject()].getSubType()>=0 || m[relObject].queryForm(NUMBER_FORM_NUM)>=0))
		whereMovingRelativeTo=relObject;
	if (whereObject>=0) o=(m[whereObject].objectMatches.size()==1) ? m[whereObject].objectMatches[0].object : m[whereObject].getObject();
	int objectSubType=(o>=0) ? objects[o].getSubType() : -1;
	relPrep=relObject=-1;
	if (objectSubType>=0 && (relPrep = m[whereObject].endObjectPosition)>=0 && m[relPrep].word->first==L"of" && (relObject=m[relPrep].getRelObject())>=0 &&
		  (m[relObject].getObject()<0 || (objects[m[relObject].getObject()].getSubType()<0 && m[relObject].queryForm(NUMBER_FORM_NUM)<0 && !isAgentObject(m[relObject].getObject()))))
		objectSubType=-1;
	if (whereMovingRelativeTo<0 && relObject>=0 && m[relObject].getObject()>=0 && (objects[m[relObject].getObject()].getSubType()>=0 || m[relObject].queryForm(NUMBER_FORM_NUM)>=0))
		whereMovingRelativeTo=relObject;
	bool objectIsAcceptable=o>=0 && (((objects[o].male || objects[o].female) && 
		    (m[whereObject].getObject()<0 || objects[m[whereObject].getObject()].objectClass!=BODY_OBJECT_CLASS) && 
		    objects[o].objectClass!=BODY_OBJECT_CLASS) || objectSubType!=-1);
	// he arrived first, last, etc - this is not really an object
	if (!objectIsAcceptable && o>=0 && m[whereObject].queryWinnerForm(numeralOrdinalForm)>=0 && (m[whereObject].endObjectPosition-m[whereObject].beginObjectPosition)==1)
		objectIsAcceptable=true;
	// She set foot in England
	if (relationType==stMOVE && whereObject>=0 && (m[whereObject].word->first==L"foot" && m[whereObject-1].queryWinnerForm(determinerForm)==-1))
		objectIsAcceptable=true;
	//  ESTABDanvers was seen speaking to a young American girl[jane]
	if (!objectIsAcceptable && o>=0 && ((m[whereObject].word->second.timeFlags&T_UNIT) || (m[whereObject].word->second.inflectionFlags&VERB_PRESENT_PARTICIPLE)))
		o=-1;
	if (!objectIsAcceptable && o<0 && whereObject>=0 && adverbialPlace(whereObject))
		objectIsAcceptable=true;
	// the corner of the street
	if (wherePrep>=0 && po<0 && whereMovingRelativeTo>=0) po=m[whereMovingRelativeTo].getObject();
	bool prepObjectIsAcceptable=po>=0 && (((objects[po].male || objects[po].female) && objects[po].objectClass!=BODY_OBJECT_CLASS) || prepObjectSubType!=-1);
	if (whereObject>=0 && whereVerb>=0 && whereObject+1==m[whereVerb].relPrep && 
		  m[m[whereVerb].relPrep].getRelObject()>=0 && m[m[m[whereVerb].relPrep].getRelObject()].getObject()>=0)
	{
		wstring word=m[whereObject].word->first;
		bool location=isRelativeLocation(word);
		if (location=(location && objects[m[m[m[whereVerb].relPrep].getRelObject()].getObject()].getSubType()>=0))
		{
			objectIsAcceptable=true;
			if (word==L"corner" && m[whereVerb].getMainEntry()->first==L"turn")
			{
				prepObjectIsAcceptable=true;
				prepObjectSubType=objectSubType=objects[m[m[m[whereVerb].relPrep].getRelObject()].getObject()].getSubType();
			}
		}
	}
	if (wherePrepObject>=0 && !prepObjectIsAcceptable && whereVerb>=0)
	{
		bool timeUnit;
		wherePrep=-1;
		wherePrepObject=findAnyLocationPrepObject(whereVerb,wherePrep,prepObjectIsAcceptable,timeUnit);
		if (wherePrepObject>=0) po=(m[wherePrepObject].objectMatches.size()==1) ? m[wherePrepObject].objectMatches[0].object : m[wherePrepObject].getObject();
		prepObjectSubType=(po>=0) ? objects[po].getSubType() : -1;
		if (prepObjectSubType<0 && po>=0 && objects[po].isPossibleSubType(false)) prepObjectSubType=UNKNOWN_PLACE_SUBTYPE;
		prepObjectIsAcceptable|=po>=0 && (((objects[po].male || objects[po].female) && objects[po].objectClass!=BODY_OBJECT_CLASS) || prepObjectSubType!=-1);
	}
	if (prepObjectIsAcceptable && po>=0 && prepObjectSubType==UNKNOWN_PLACE_SUBTYPE && !objects[po].isNotAPlace && objects[po].lastWhereLocation!=wherePrepObject)
	{
		objects[po].usedAsLocation++;
		objects[po].lastWhereLocation=wherePrepObject;
		bool subjectAgentGendered=whereSubject>=0 && m[whereSubject].getObject()>=0 && objects[m[whereSubject].getObject()].isAgent(true);
		if (subjectAgentGendered && relationType!=stESTABLISH)
			objects[po].usedAsLocation+=500;
		else if (objects[po].usedAsLocation<5)
			lplog(LOG_RESOLUTION,L"%06d:%d:prepObject %s used as place subject %s type %s.",wherePrepObject,objects[po].usedAsLocation,objectString(po,tmpstr,false).c_str(),
				(subjectAgentGendered) ? L"gendered":L"nongendered",relationString(relationType).c_str());

	}
	// ESTABUshered into the presence of Mr . Carter , he[carter] and I[tommy] wish each other good morning as is customary
	// post L&L resolution on multiple compound subjects
	if (whereSubject>whereVerb && m[whereSubject].nextCompoundPartObject>=0 && (whereObject>=0 || wherePrepObject>=0))
	{
		if (whereObject>=0 && in(m[whereObject].getObject(),whereSubject))
			whereSubject=m[whereSubject].nextCompoundPartObject;
		else if (wherePrepObject>=0 && in(m[wherePrepObject].getObject(),whereSubject))
			whereSubject=m[whereSubject].nextCompoundPartObject;
	}
	// half-way across the Park
	if (o>=0 && objectSubType<0 && prepObjectIsAcceptable && m[whereObject].word->first==L"way")
		objectIsAcceptable=true;
	bool ofNegation=(prepTypeCancelled && !prepObjectIsAcceptable && !objectIsAcceptable);
//	vector <cLocalFocus>::iterator lsi;
	bool not=whereVerb>=0 && (m[whereVerb].verbSense&VT_NEGATION)!=0;
	not|=whereVerb>=0 && m[whereVerb].previousCompoundPartObject>=0 && (m[m[whereVerb].previousCompoundPartObject].verbSense&VT_NEGATION)!=0; // trace back to main verb
	//bool alreadyTaken=whereVerb>=0 && m[whereVerb].hasSyntacticRelationGroup;
	bool there=(whereSubject>=0 && m[whereSubject].word->first==L"there" && (m[whereSubject].objectRole&IN_PRIMARY_QUOTE_ROLE));
	there|=(whereControllingEntity>=0 && m[whereControllingEntity].word->first==L"there" && (m[whereControllingEntity].objectRole&IN_PRIMARY_QUOTE_ROLE));
	// where his[tommy] CONTACTcolleague[tuppence] would meet him[tommy] at ten o'clock . 
	if (whereVerb>=0 && (m[whereVerb].verbSense&VT_POSSIBLE) && whereVerb>0 && m[whereVerb-1].word->first==L"would")
	{
		m[whereVerb].verbSense &= ~VT_POSSIBLE; 
		m[whereVerb].verbSense |= VT_FUTURE;
	}
	int vt=(whereVerb>=0) ? m[whereVerb].verbSense : 0;
	if (vt==VT_PAST && whereSubject<0 && whereControllingEntity<0 && whereVerb>0 && isEOS(whereVerb-1) && 
		  (m[whereVerb].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0 && (whereObject<0 || m[whereObject].word->first!=L"me"))
	{
		// do a quick scan through local objects, and see whether any are in the current quote, with a verb that is also in the past.
		for (int I=whereVerb-2; I>=0 && m[I].queryForm(quoteForm)==-1; I--)
			if (m[I].getObject()>=0 && in(m[I].getObject())!=localObjects.end() && 
				(m[I].objectRole&SUBJECT_ROLE)!=0 && m[I].getRelVerb()>=0)
			{
				whereSubject=m[whereVerb].relSubject=I;
				if (isAgentObject(m[I].getObject())) subjectGendered=true;
				break;
			}
		if (whereSubject<0)
			subjectGendered=true;
	}
	// VT_POSSIBLE cancellation
	// we wouldhad better take a taxi / but NOT I wouldhad rather take a taxi
	if ((vt&VT_POSSIBLE) && whereVerb>1 && m[whereVerb-1].word->first==L"better" && m[whereVerb-2].word->first==L"wouldhad")
		vt&=~VT_POSSIBLE;
	// or moving, special case (taking a taxi, set foot in England)
	bool movingSpecialCase=o>=0 && 
			 ((objectSubType==MOVING && whereVerb>=0 && isVerbClass(whereVerb,L"bring-11.3")) || 
			  (m[whereObject].word->first==L"foot" && m[whereObject-1].queryWinnerForm(determinerForm)==-1));
	movingSpecialCase|=(objectSubType>=0 && objectSubType<UNKNOWN_PLACE_SUBTYPE);
	// moved down the stairs / down may be misparsed as a particle
	if (relationType==stMOVE && !prepObjectIsAcceptable && objectIsAcceptable && whereVerb>=0 && 
		  (m[whereVerb+1].word->second.flags&cSourceWordInfo::prepMoveType) && m[whereVerb+1].queryWinnerForm(adverbForm)!=-1)
		movingSpecialCase=true;
	bool genderedLocationRelation=whereVerb>=0 && 
		// state of confusion - objects of a subType that are negated by being 'of' an object which indicates they are not
		!ofNegation &&
		// "there are seven of us at home"
		!there &&
		!not && 
		relationType!=stTRANSFER && 
		// moving an object is OK if the object being moved is a place (She took her place in the boat) or the subject (He moved himself over the water)
		// or SECOND or FIRST person He moved you; They sent me
		(relationType!=stMOVE_OBJECT || objectSubType>=0 || o==so || (po==so && o<0) || (whereObject>=0 && (m[whereObject].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON)) ||
		 (o>=0 && (objects[o].numIdentifiedAsSpeaker>0 || objects[o].PISHail>0 || objects[o].PISDefinite>0)))) && 
		 // object must be physical
	  //((relationType!=stMOVE_OBJECT && relationType!=stMOVE) || whereObject<0 || objectSubType>=0 || !((tmp1=m[whereObject].word->second.getMainEntry(m[whereObject].word)->second.flags)&cSourceWordInfo::notPhysicalObjectByWN)) && 
		// but 'not' He thrust his hands in his pockets
		(relationType!=stMOVE_OBJECT || whereObject<0 || m[whereObject].getObject()<0 || objects[m[whereObject].getObject()].objectClass!=BODY_OBJECT_CLASS) && 
		// gendered subject, controller
		(subjectGendered || controllerGendered || (objectIsAcceptable && so>=0 && objects[so].getSubType()>=0)) &&
		// gendered object or place object
	  ((relationType!=stMOVE && relationType!=stESTABLISH && relationType!=stSTAY && relationType!=stLOCATION && relationType!=-stLOCATION) || o<0 || objectIsAcceptable) &&
		// if contact, must be contacting something.  If ESTAB, must be somewhere
		((relationType!=stCONTACT && relationType!=stNEAR && relationType!=stESTABLISH) || objectIsAcceptable || prepObjectIsAcceptable) &&
		// if moving, must be moving somewhere?
		(relationType!=stMOVE || prepObjectIsAcceptable || movingSpecialCase || convertToMove || (po<0 && o<0 && !(m[whereVerb].flags&cWordMatch::flagInInfinitivePhrase)));
		// if entering, must be the first time this is locally mentioned.
		// if contact, if both object and subject are already mentioned, this is not a significant movement (it doesn't add any information), unless a location is specific
		//((relationType!=stENTER && relationType!=stCONTACT) || (lsi=in(m[whereSubject].getObject()))==localObjects.end() || lsi->previousWhere<0 || lsi->previousWhere>=where || prepObjectSubType>=0);
	if (speakerGroupsEstablished)
	{
		cSyntacticRelationGroup sr(where,_o,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wherePrepObject,whereMovingRelativeTo,relationType,genderedEntityMove,genderedLocationRelation,objectSubType,prepObjectSubType,physicalRelation);
		if (debugTrace.traceRelations)
			logSyntacticRelationGroup(sr, L"BEFORE CORRECTION");
		correctSRIEntry(sr);
		vector <cSyntacticRelationGroup>::iterator location = lower_bound(syntacticRelationGroups.begin(), syntacticRelationGroups.end(), sr, comparesr),forward=location,firstLocation=location;
		// there can be multiple space relations in one position - one for each object
		while (forward!=syntacticRelationGroups.end() && forward->where==where)
		{
			if (sr==*forward)
			{
			  location=forward;
			  break;
			}
			forward++;
		}
		if (syntacticRelationGroups.empty() || location==syntacticRelationGroups.end() || (shouldLogSyntacticRelationGroup=sr!=*location))
		{
			if (syntacticRelationGroups.empty())
				location=syntacticRelationGroups.insert(syntacticRelationGroups.begin(),sr);
			else if (location!=syntacticRelationGroups.end() && sr.canUpdate(*location))
			{
				location->o=sr.o;
				location->wherePrep=sr.wherePrep;
				location->wherePrepObject=sr.wherePrepObject;
				if (location->timeInfo.empty() && sr.timeInfo.size()>0)
				{
					location->timeInfo=sr.timeInfo;
					location->timeInfoSet=sr.timeInfoSet;
				}
			}
			else
				location=syntacticRelationGroups.insert(location,sr);
			srSetTimeFlowTense((int) (location-syntacticRelationGroups.begin()));
			if (location->timeInfo.empty())
			{
				forward=location;
				while (forward!=syntacticRelationGroups.begin() && forward->where==where) forward--;
				forward++;
				while (forward!=syntacticRelationGroups.end() && forward->where==where)
				{
					if (forward->timeInfo.size())
					{
						location->timeInfo=forward->timeInfo;
						location->tft.timeTransition|=forward->tft.timeTransition;
						break;
					}
					forward++;
				}
			}
			if (convertToMove && (location->tft.futureHappening || location->tft.pastHappening || !location->tft.presentHappening))
			{
				location->relationType=stENTER;
				convertToMove=false;
			}
			if (whereSubject>=0)
			{
				for (int si=0; si<(signed)m[whereSubject].objectMatches.size(); si++)
					objects[m[whereSubject].objectMatches[si].object].syntacticRelationGroups.push_back((int) (location-syntacticRelationGroups.begin()));
				if (m[whereSubject].getObject()>=0 && m[whereSubject].objectMatches.empty())
					objects[m[whereSubject].getObject()].syntacticRelationGroups.push_back((int) (location-syntacticRelationGroups.begin()));
			}
		}
		if (debugTrace.traceRelations)
			logSyntacticRelationGroup(sr, whereType);
	}
	else
	{
		if (syntacticRelationGroups.empty() || syntacticRelationGroups[syntacticRelationGroups.size()-1].where!=where)
		{
			int insertionPoint=syntacticRelationGroups.size();
			while (insertionPoint>0 && syntacticRelationGroups[insertionPoint-1].where>where) insertionPoint--;
			cSyntacticRelationGroup sr(where,_o,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wherePrepObject,whereMovingRelativeTo,relationType,genderedEntityMove,genderedLocationRelation,objectSubType,prepObjectSubType,physicalRelation);
			correctSRIEntry(sr);
			syntacticRelationGroups.insert(syntacticRelationGroups.begin()+insertionPoint,sr);
			srSetTimeFlowTense(insertionPoint);
			if (convertToMove && (syntacticRelationGroups[insertionPoint].tft.futureHappening || syntacticRelationGroups[insertionPoint].tft.pastHappening))
			{
				syntacticRelationGroups[insertionPoint].relationType=stENTER;
				convertToMove=false;
			}
			shouldLogSyntacticRelationGroup=true;
			if (debugTrace.traceRelations)
				logSyntacticRelationGroup(sr,whereType);
		}
	}
	if (whereVerb>=0) m[whereVerb].hasSyntacticRelationGroup=true;
	if (wherePrepObject>=0) m[wherePrepObject].hasSyntacticRelationGroup=true;
	m[where].hasSyntacticRelationGroup=true;
}

void cSource::logSyntacticRelationGroup(cSyntacticRelationGroup &sr,const wchar_t *whereType)
{
	set <int> relPreps;
	getAllPreps(&sr, relPreps, sr.whereObject);
	wstring tmpstr, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6, extendedPrep;
	for (set <int>::iterator rpi = relPreps.begin(), rpiEnd = relPreps.end(); rpi != rpiEnd; rpi++)
		if (*rpi!=sr.wherePrep && *rpi>=0)
			extendedPrep += m[*rpi].word->first + L" "+src(m[*rpi].getRelObject(), L"PO", tmpstr4)+L" ";

	lplog(LOG_INFO, L"%06d:%s %s %s:%s V[%d:%s] %s%s%s %s%s%s%s%s%s",
		sr.where,
		whereType,
		(sr.genderedLocationRelation) ? L" GENDERED" : L"",
		relationString(sr.relationType).c_str(),
		src(sr.whereSubject, L"S", tmpstr),
		sr.whereVerb,
		(sr.whereVerb < 0 || sr.relationType == -stLOCATION) ? L"" : m[sr.whereVerb].word->first.c_str(),
		src(sr.whereObject, L"O", tmpstr2),
		(sr.objectSubType >= 0) ? OCSubTypeStrings[sr.objectSubType] : ((sr.whereObject < 0) ? L"" : L"SubtypeUndefined "),
		(sr.whereObject < 0) ? L"" : src(m[sr.whereObject].relNextObject, L"nextObject", tmpstr3),
		(sr.wherePrep < 0) ? L"" : m[sr.wherePrep].word->first.c_str(),
		src(sr.wherePrepObject, L"PO", tmpstr4),
		(sr.prepObjectSubType >= 0) ? OCSubTypeStrings[sr.prepObjectSubType] : ((sr.wherePrepObject < 0) ? L"" : L"SubtypeUndefined "),
		extendedPrep.c_str(),
		src(sr.whereControllingEntity, L"controller", tmpstr5),
		src(sr.whereMovingRelativeTo, L"moving relative to", tmpstr6));
}

// subject is moving to a destination
// will change source.m (invalidate all iterators through the use of newSR)
bool cSource::moveIdentifiedSubject(int where,bool inPrimaryQuote,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int at,int whereMovingRelativeTo,int hasSyntacticRelationGroup,const wchar_t *whereType,bool physicalRelation)
{ LFS
	wstring tmpstr;
	set <int> speakers,povSpeakers;
	getCurrentSpeakers(speakers,povSpeakers);
	bool povAllIn=false,povOneIn=false,sAllIn=false,sOneIn=false,allIn=true,oneIn=false;
	intersect(whereSubject,speakers,sAllIn,sOneIn);
	bool povNotFound=false;
	if (povSpeakers.empty())
	{
		if (intersect(whereSubject,speakers,allIn,oneIn) && allIn)
		{
			for (unsigned int omi=0; omi<m[whereSubject].objectMatches.size(); omi++)
				povSpeakers.insert(m[whereSubject].objectMatches[omi].object);
			if (povSpeakers.empty())
				povSpeakers.insert(m[whereSubject].getObject());
			//povSpeakers=speakers;
			povOneIn=true;
		}
	}
	else
	{
	for (set <int>::iterator pi=povSpeakers.begin(),piEnd=povSpeakers.end(); pi!=piEnd && !povNotFound; pi++)
		if (speakers.find(*pi)==speakers.end())
		{
			if (objects[*pi].objectClass==PRONOUN_OBJECT_CLASS && !objects[*pi].plural && (objects[*pi].male ^ objects[*pi].female))
			{
				bool povFound=false;
				for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd && !povFound; si++)
					povFound|=(objects[*si].male==objects[*pi].male || objects[*si].female==objects[*pi].female);
				if (!povFound) povNotFound=true;
			}						
			else
				povNotFound=true;
		}
	intersect(whereSubject,povSpeakers,povAllIn,povOneIn);
	if (debugTrace.traceSpeakerResolution && (speakers.size() || povSpeakers.size()))
	{
		lplog(LOG_RESOLUTION,L"%06d:PLACE transition:speakers=%s intersect=%s",where,objectString(speakers,tmpstr).c_str(),(sOneIn) ? L"true":L"false");
		lplog(LOG_RESOLUTION,L"%06d:PLACE transition:povSpeakers=%s intersect=%s",where,objectString(povSpeakers,tmpstr).c_str(),(povOneIn) ? L"true":L"false");
		lplog(LOG_RESOLUTION,L"%06d:PLACE transition:not all povSpeakers in speakers",where);
	}
	}
	// if no pov speakers, do nothing.
	// if povSpeaker, age all other objects.  
	bool nowTense=(inPrimaryQuote || !(m[whereSubject].objectRole&NONPAST_OBJECT_ROLE));
	if (inPrimaryQuote && whereVerb>=0) 
		nowTense=m[whereVerb].queryWinnerForm(isForm)>=0 || m[whereVerb].verbSense==VT_EXTENDED+ VT_PRESENT || m[whereVerb].verbSense==VT_PASSIVE+ VT_PRESENT+VT_EXTENDED;
	if (povOneIn)
	{
		for (set<int>::iterator pvi=povSpeakers.begin(),pviEnd=povSpeakers.end(); pvi!=pviEnd; pvi++)
			newSR(where,*pvi,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,at,whereMovingRelativeTo,hasSyntacticRelationGroup,whereType,physicalRelation);
	}
	else
	{
		// If non-pov speaker, age that subject.
		for (int I=0; I<(signed)m[whereSubject].objectMatches.size(); I++)
			newSR(where,m[whereSubject].objectMatches[I].object,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,at,whereMovingRelativeTo,hasSyntacticRelationGroup,whereType,physicalRelation);
		if (m[whereSubject].objectMatches.empty())
			newSR(where,m[whereSubject].getObject(),whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,at,whereMovingRelativeTo,hasSyntacticRelationGroup,whereType,physicalRelation);
	}
	return true;
}

// subject will move the object at whereObject to a destination
// will change source.m (invalidate all iterators through the use of newSR)
bool cSource::srMoveObject(int where,int whereControllingEntity,int whereSubject,int whereVerb,int wherePrep,int whereObject,int wherePrepObject,int whereMovingRelativeTo,int hasSyntacticRelationGroup,wchar_t *whereType,bool physicalRelation)
{ LFS
	wstring tmpstr;
	for (int I=0; I<(signed)m[whereObject].objectMatches.size(); I++)
		newSR(where,m[whereObject].objectMatches[I].object,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wherePrepObject,whereMovingRelativeTo,hasSyntacticRelationGroup,whereType,physicalRelation);
	if (m[whereObject].objectMatches.empty())
		newSR(where,m[whereObject].getObject(),whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wherePrepObject,whereMovingRelativeTo,hasSyntacticRelationGroup,whereType,physicalRelation);
	return true;
}

int cSource::findAnyLocationPrepObject(int whereVerb,int &wherePrep,bool &location,bool &timeUnit)
{ LFS
	if (wherePrep<0)
		wherePrep=m[whereVerb].relPrep;
	else
		wherePrep=m[wherePrep].relPrep;
	int wherePrepObject=-1,po,prepLoop=0;
	timeUnit=false;
	for (; wherePrep>=0; wherePrep=m[wherePrep].relPrep)
	{
		if (prepLoop++>30)
		{
			wstring tmpstr;
			lplog(LOG_ERROR,L"%06d:Prep loop occurred (5) %s.",wherePrep,loopString(wherePrep,tmpstr));
			return wherePrepObject;
		}
		if (((m[wherePrep].word->second.flags&cSourceWordInfo::prepMoveType) || m[wherePrep].word->first==L"for") && !rejectPrepPhrase(wherePrep))
		{
			wherePrepObject=m[wherePrep].getRelObject();
			if (m[wherePrep].relPrep>=0 && m[m[wherePrep].relPrep].getRelObject()>=0 && m[m[m[wherePrep].relPrep].getRelObject()].getObject()>=0 &&
				  m[m[wherePrep].relPrep].word->first==L"of" && !location)
			{
				wstring word=m[wherePrepObject].word->first;
				location=isRelativeLocation(word);
				if (location=(location && objects[m[m[m[wherePrep].relPrep].getRelObject()].getObject()].getSubType()>=0))
				  return wherePrepObject;
				// in the presence of Mr. Carter
				if (word==L"presence" && isAgentObject(m[m[m[wherePrep].relPrep].getRelObject()].getObject()))
				{
					wherePrep=m[wherePrep].relPrep;
					return wherePrepObject=m[wherePrep].getRelObject();
				}
			}
			// Tuppence's hostel was situated in what was charitably called Southern Belgravia.
			if (m[wherePrepObject].queryWinnerForm(relativizerForm)>=0 && m[wherePrepObject].getRelObject()>=0 && m[wherePrepObject].getRelVerb()>=0 &&
					(m[whereVerb=m[wherePrepObject].getRelVerb()].getMainEntry()->first==L"call" || m[whereVerb=m[wherePrepObject].getRelVerb()].getMainEntry()->first==L"name"))
				wherePrepObject=m[wherePrepObject].getRelObject();
			po=m[wherePrepObject].getObject();
			if (//(m[wherePrepObject].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN)==0 ||
				  (timeUnit=(m[wherePrepObject].word->second.timeFlags&T_UNIT)!=0) || // she remained for some minutes
					(timeUnit=(m[wherePrepObject].queryWinnerForm(numeralCardinalForm)>=0) && (m[wherePrep].word->second.timeFlags&T_CARDTIME)) || // I will arrive at 10.
					(timeUnit=(m[wherePrepObject].beginObjectPosition>=0 && m[m[wherePrepObject].beginObjectPosition].pma.queryPattern(L"_TIME")!=-1)) ||
						(po>=0 && (objects[po].isPossibleSubType(false) || objects[po].isAgent(true))) ||
						(m[wherePrepObject].objectMatches.size() && (objects[m[wherePrepObject].objectMatches[0].object].getSubType()>=0 || objects[m[wherePrepObject].objectMatches[0].object].isAgent(false))))
			{
				location=true;
				return wherePrepObject;
			}
		}
	}
	location=false;
	return -1;
}

// reject in the end/at the end of three days/on the other hand
bool cSource::rejectPrepPhrase(int wherePrep)
{ LFS
	if (m[wherePrep].getRelObject()<0) return true;
	wstring po=m[m[wherePrep].getRelObject()].word->first;
	// face to face
	if (wherePrep>0 && m[wherePrep-1].word->first==L"face" && m[wherePrep].word->first==L"to" && po==L"face")
		return true;
	// in the end
	if (m[wherePrep].word->first==L"in" && (po==L"end" || po==L"beginning") &&
		  (m[wherePrep].relPrep<0 || abs(m[wherePrep].relPrep-m[wherePrep].getRelObject())>2))
		return true;
	// at the end of three days [days must be a location]
	if (m[wherePrep].word->first==L"at" && (po==L"end" || po==L"beginning") &&
		  m[wherePrep].relPrep>=0 && (m[m[wherePrep].relPrep].word->first!=L"of" ||
			(m[m[wherePrep].relPrep].getRelObject()>=0 && m[m[m[wherePrep].relPrep].getRelObject()].getObject()>=0 && objects[m[m[m[wherePrep].relPrep].getRelObject()].getObject()].getSubType()<0)))
		return true;
	// at all OR at bay
	if (m[wherePrep].word->first==L"at" && (po==L"all" || po==L"bay"))
		return true;
	// by hook or by crook
	if (m[wherePrep].word->first==L"by" && 
		  m[wherePrep+1].word->first==L"hook" &&
		  m[wherePrep+2].word->first==L"or" &&
		  m[wherePrep+3].word->first==L"by" &&
			m[wherePrep+4].word->first==L"crook")
		return true;
	// on the other hand
	if ((m[wherePrep].getRelObject()-wherePrep)==3 && 
		  m[wherePrep].word->first==L"on" && 
		  m[wherePrep+1].word->first==L"the" &&
			m[wherePrep+2].word->first==L"other" &&
			po==L"hand")
		return true;
	return false;
}

bool cSource::adverbialPlace(int where)
{ LFS
	if (where<0) return false;
	wchar_t *locations[]={L"there",L"where",L"here",L"ashore",L"alight",L"aboard",L"abroad",L"back",L"away",NULL}; // ,L"out" may have negative conseuqnces
	wstring word=m[where].word->first;
	for (int p=0; locations[p]; p++) if (locations[p]==word) return true;
	return (word==L"down" || word==L"up") && where+1<(signed)m.size() && (m[where+1].word->first==L"there" || m[where+1].word->first==L"here");
}

bool cSource::adjectivalExit(int where)
{ LFS
	if (m[where].word->first==L"off" && m[where-1].word->first==L"well") return false;
	wchar_t *gone[]={L"gone",L"kaput",L"absent",L"deceased",L"departed",L"dead",L"lost",L"off",NULL};
	wstring word=m[where].word->first;
	for (int p=0; gone[p]; p++) if (gone[p]==word) return true;
	return false;
}

bool cSource::intersect(int where1,int where2)
{ LFS
	if (where1<0 || where2<0) return false;
	if (m[where1].objectMatches.empty() && m[where2].objectMatches.empty())
		return m[where1].getObject()==m[where2].getObject();
	if (m[where1].objectMatches.empty())
		return in(m[where1].getObject(),where2);
	if (m[where2].objectMatches.empty())
		return in(m[where2].getObject(),where1);
	bool allIn,oneIn;
	return intersect(m[where1].objectMatches,m[where2].objectMatches,allIn,oneIn);
}

bool cSource::exitConversion(int whereObject,int whereSubject,int wherePrepObject)
{ LFS
	int st;
	if (wherePrepObject<0 || m[wherePrepObject].getObject()<0) return false;
	if ((st=objects[m[wherePrepObject].getObject()].getSubType())<0)
	{
		for (int I=0; I<(signed)m[wherePrepObject].objectMatches.size(); I++)
			if ((st=objects[m[wherePrepObject].objectMatches[I].object].getSubType())>=0)
				break;
	}
	return st>=0 && st<=GEOGRAPHICAL_URBAN_FEATURE && //==COUNTRY &&
			// He took himself to England. // change-of-place
			((whereObject>=0 && intersect(whereObject,whereSubject))  ||
			// He went to England. // change-of-place
			whereObject<0);
}

bool cSource::whereSubType(int where)
{ LFS
	if (where<0) return false;
	int st;
	if (m[where].getObject()>=0 && (st=objects[m[where].getObject()].getSubType())>=0 && st!=BY_ACTIVITY && st!=UNKNOWN_PLACE_SUBTYPE)
		return true;
	for (int I=0; I<(signed)m[where].objectMatches.size(); I++)
		if ((st=objects[m[where].objectMatches[I].object].getSubType())>=0 && st!=BY_ACTIVITY && st!=UNKNOWN_PLACE_SUBTYPE)
			return true;
	return false;
}

// The two young people[tommy,tuppence] greeted each other affectionately , and momentarily **blocked the Dover Street Tube exit
// verbs of looking : (looked)
//   He looked into the room
// transitioning prepositions with intervening direct objects self owned 'place or position'
//   the girl[girl] went forward to take her[girl] place in the boat .
//   the girl[girl] took her[girl] place in the boat .
//   *put it into the office (into applies to the direct object, not the subject!)
// spacially hierarchical structures 
//   a house is larger than a room, so when a person moves into a room, they are still in the same house.
// will change source.m (invalidate all iterators through the use of newSR)
bool cSource::placeIdentification(int where,bool inPrimaryQuote,int whereControllingEntity,int whereSubject,int whereVerb,int vnClass)
{ LFS
	wstring id=vbNetClasses[vnClass].name(),tmpstr,tmpstr2,tmpstr3;
	bool acceptableVerbForm=!vbNetClasses[vnClass].noPhysicalAction;
	bool prepLocation=vbNetClasses[vnClass].prepLocation;
	bool objectMustBeLocation=vbNetClasses[vnClass].objectMustBeLocation;
	bool prepMustBeLocation=vbNetClasses[vnClass].prepMustBeLocation;
	bool contact=vbNetClasses[vnClass].contact;
	bool _near=vbNetClasses[vnClass]._near;
	bool transfer=vbNetClasses[vnClass].transfer;
	bool noPrepTo=vbNetClasses[vnClass].noPrepTo;
	bool noPrepFrom=vbNetClasses[vnClass].noPrepFrom;
	bool move=vbNetClasses[vnClass].move;
	bool moveInPlace=vbNetClasses[vnClass].moveInPlace;
	bool moveObject=vbNetClasses[vnClass].moveObject;
	bool exit=vbNetClasses[vnClass].exit;
	bool enter=vbNetClasses[vnClass].enter;
	bool contiguous=vbNetClasses[vnClass].contiguous;
	bool start=vbNetClasses[vnClass].start;
	bool stay=vbNetClasses[vnClass].stay;
	//bool has=vbNetClasses[vnClass].has;
	//bool think=vbNetClasses[vnClass].think;
	//bool communicate=vbNetClasses[vnClass].communicate;
	//bool startRelationType=vbNetClasses[vnClass].start;
	//bool cos=vbNetClasses[vnClass].changeState;
	//bool metaInfo=vbNetClasses[vnClass].metaInfo;
	bool proLocation=false;
	bool pr=true;
	int st=-1,whereObject=-1,o=-1,pmaOffset,wpd=-1,pd=-1;
	if ((whereObject=m[whereVerb].getRelObject())>=0)
	{
		if  (m[whereObject].objectMatches.empty() && (o=m[whereObject].getObject())>=0)
			st=objects[o].getSubType();
		for (int I=0; I<(signed)m[whereObject].objectMatches.size(); I++)
			if (objects[m[whereObject].objectMatches[I].object].getSubType()>=0)
			{
				st=objects[o=m[whereObject].objectMatches[I].object].getSubType();
				break;
			}
		// turned the corner of the street
		if (m[whereVerb].getRelObject()>=0 && m[whereVerb].getRelObject()+1==m[whereVerb].relPrep && 
			  m[m[whereVerb].relPrep].getRelObject()>=0 && m[m[m[whereVerb].relPrep].getRelObject()].getObject()>=0)
		{
			wstring word=m[m[whereVerb].getRelObject()].word->first;
			bool location=isRelativeLocation(word);
			if (location=(location && objects[m[m[m[whereVerb].relPrep].getRelObject()].getObject()].getSubType()>=0))
			  st=objects[m[m[m[whereVerb].relPrep].getRelObject()].getObject()].getSubType();
		}
		if (st<0 && m[whereObject].word->first==L"where")
			st=UNKNOWN_PLACE_SUBTYPE;
	}
	if (move && moveObject)
	{
		move=(whereObject<0);
		moveObject=(whereObject>=0);
	}
	// relations need to be all inclusive - no longer limiting to physical relations
	//if (whereSubject<0 && !(inPrimaryQuote || inSecondaryQuote) && 
	//	  !contact && !_near && !transfer && !move && !moveObject && !moveInPlace && !exit && !enter && !contiguous && !start && !stay)
	//{
	//	if (t.traceSpeakerResolution)
	//		lplog(LOG_WHERE,L"%06d:rejected no subject:[%s] subject %s verb %s object %s",where,vbNetClasses[vnClass].name().c_str(),
	//				(whereSubject<0) ? L"(None)":objectString(m[whereSubject].getObject(),tmpstr,false).c_str(),m[whereVerb].word->first.c_str(),
	//				(whereObject<0) ? L"(None)": objectString(m[whereObject].getObject(),tmpstr2,false).c_str());
	//	return false;
	//}
	//if (move && whereObject>=0) move=false;
	// if sentence is a relative phrase, locate previous object before relativizer
	// London where he obtained the fish
	if (whereObject<0 && (m[where].objectRole&SENTENCE_IN_REL_ROLE) && whereSubject>=0 && m[whereSubject].beginObjectPosition>0 && 
		  m[m[whereSubject].beginObjectPosition-1].queryWinnerForm(relativizerForm)>=0 && 
			(m[m[whereSubject].beginObjectPosition-1].flags&cWordMatch::flagRelativeObject))
		whereObject=m[m[whereSubject].beginObjectPosition-1].getRelObject();
	int afterVerb=whereVerb+1;
	while (m[afterVerb].queryWinnerForm(adverbForm)>=0 && m[afterVerb].queryWinnerForm(prepositionForm)<0 && afterVerb+1<(signed)m.size() && !adverbialPlace(afterVerb)) afterVerb++;
	if (whereObject<0 && adverbialPlace(afterVerb))
	{
		st=0;
		whereObject=afterVerb;
		prepMustBeLocation=false;
		proLocation=true;
	}
	// he passed inside OR he gave the book inside.
	// NOT: appeared over - eager. / walk up
	// NOT: engaged in wondering whether ...
	if (m[whereVerb].relPrep<0)
	{
		if (m[whereVerb].getRelObject()<0 && m[whereVerb+1].pma.queryPattern(L"_PP")==-1 && whereVerb+1<(signed)m.size())
			afterVerb=whereVerb+1;
		else if (m[whereVerb].getRelObject()>=0 && m[m[whereVerb].getRelObject()].endObjectPosition>=0 && m[m[m[whereVerb].getRelObject()].endObjectPosition].pma.queryPattern(L"_PP")==-1)
			afterVerb=m[m[whereVerb].getRelObject()].endObjectPosition;
		while (afterVerb>=0 && (m[afterVerb].queryWinnerForm(adverbForm)>=0 || m[afterVerb].queryWinnerForm(prepositionForm)>=0) && 
			     afterVerb+1<(signed)m.size() && afterVerb<whereVerb+3 && !(m[afterVerb].word->second.flags&cSourceWordInfo::prepMoveType)) afterVerb++;
		// is there a recent location associated with the subject?
		if (afterVerb>=0 && (prepTypesMap[m[afterVerb].word->first]==tprIN || (adverbialPlace(afterVerb) && m[afterVerb].queryForm(prepositionForm)!=-1)) && 
			  m[afterVerb].word->first!=L"out" && (whereSubject<0 || !(m[whereSubject].objectRole&PRIMARY_SPEAKER_ROLE) || isSpecialVerb(whereVerb,false)))
		{
			// scan for a previous LOCATION associated with this subject
			vector <cSyntacticRelationGroup>::iterator location = findSyntacticRelationGroup(where);
			if (location==syntacticRelationGroups.end() && syntacticRelationGroups.size()>0 && syntacticRelationGroups[syntacticRelationGroups.size()-1].where<where)
				location=syntacticRelationGroups.begin()+syntacticRelationGroups.size()-1;
			if (location!=syntacticRelationGroups.end())
			{
				int backwardSearchLimit=0;
				if (whereSubject>=0)
				{
					vector <cLocalFocus>::iterator lsi=in(m[whereSubject].getObject());
					for (int s=0; s<(signed)m[whereSubject].objectMatches.size() && lsi==localObjects.end(); s++)
						lsi=in(m[whereSubject].objectMatches[s].object);
					if (lsi!=localObjects.end() && lsi->whereBecamePhysicallyPresent>=0)
						backwardSearchLimit=lsi->whereBecamePhysicallyPresent;
				}
				if (backwardSearchLimit==0 && currentSpeakerGroup<speakerGroups.size())
					backwardSearchLimit=speakerGroups[currentSpeakerGroup].sgBegin;
				if (backwardSearchLimit==0 && speakerGroups.size())
					backwardSearchLimit=speakerGroups[speakerGroups.size()-1].sgBegin;
				int associatedLocation=-1;
				bool pe,pp=whereSubject>=0 && physicallyPresentPosition(whereSubject,pe) && pe;
				for (vector <cSyntacticRelationGroup>::iterator li=location; li>syntacticRelationGroups.begin() && li->where>=backwardSearchLimit && associatedLocation==-1; li--)
				{
					// does the hasSyntacticRelationGroup have a subType>=0 associated with it?
					if (li->whereSubject>=0 && li->tft.presentHappening && (intersect(li->whereSubject,whereSubject) || 
						  (physicallyPresentPosition(li->whereSubject,pe) && pp && !(m[li->whereSubject].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE))))
					{
						// is object, prepObject or whereMovingRelativeTo a place?
						if (whereSubType(li->whereObject))
							associatedLocation=li->whereObject;
						else if (whereSubType(li->wherePrepObject))
							associatedLocation=li->wherePrepObject;
						else if (whereSubType(li->whereMovingRelativeTo))
							associatedLocation=li->whereMovingRelativeTo;
					}
					if ((li->relationType==stLOCATION || li->relationType==-stLOCATION) && whereSubType(li->whereSubject) && !(m[li->whereSubject].objectRole&IN_PRIMARY_QUOTE_ROLE) &&
						(m[li->whereSubject].objectRole&SUBJECT_ROLE))
							associatedLocation=li->whereSubject;
				}
				if (associatedLocation!=-1 && associatedLocation<where)
				{
					// the steps of the porch
					if (m[associatedLocation].relPrep>=0 && m[m[associatedLocation].relPrep].word->first==L"of" && m[m[associatedLocation].relPrep].getRelObject()>=0 &&
							whereSubType(m[m[associatedLocation].relPrep].getRelObject()))
						associatedLocation=m[m[associatedLocation].relPrep].getRelObject();
					lplog(LOG_RESOLUTION,L"%06d:space relation: hanging prep@%d from verb %d:%s [%s] %d:%s?",where,afterVerb,whereVerb,m[whereVerb].word->first.c_str(),m[afterVerb].word->first.c_str(),associatedLocation,whereString(associatedLocation,tmpstr,false).c_str());
					setRelPrep(whereVerb,afterVerb,2,PREP_VERB_SET,whereVerb);
					m[afterVerb].setRelObject(associatedLocation);
				}
			}			
		}
	}
	if (whereSubject>=0 && (m[whereSubject].objectRole&PRIMARY_SPEAKER_ROLE) && whereObject<0)
		move=moveObject=moveInPlace=exit=start=stay=enter=contiguous=false;
	//  I[tommy] somehow feel ESTABthat in real life one[tommy] will feel a bit of "an ass CONTACTstanding" in the street for hours with nothing to do 
	if (whereSubject==whereVerb)
		return false;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:test:[%s]%s%s%s%s%s%s%s%s %ssubject %s verb %s object %s",where,vbNetClasses[vnClass].name().c_str(),
					(move) ? L"MOVE":L"",(moveObject) ? L"MOVE_OBJECT":L"",(moveInPlace) ? L"MOVE_IN_PLACE":L"",(exit) ? L"EXIT":L"",(start) ? L"START":L"",(stay) ? L"STAY":L"",(enter) ? L"ENTER":L"",(contiguous) ? L"CONTIGUOUS":L"",
					(whereSubject>=0 && (m[whereSubject].objectRole&PRIMARY_SPEAKER_ROLE)) ? L"PRIMARY ":L"",
					(whereSubject<0) ? L"(None)":objectString(m[whereSubject].getObject(),tmpstr,false).c_str(),m[whereVerb].word->first.c_str(),
					(whereObject<0) ? L"(None)": objectString(m[whereObject].getObject(),tmpstr2,false).c_str());
	// they[tuppence,tommy] started walking down Dover Street towards Piccadilly
	if (start && // only beginning
			// verbclass afterward (NOUN[D])
			whereObject>=0 && m[whereObject].relNextObject<0 && (pmaOffset=scanForPatternTag(whereObject,VNOUN_TAG))>=0)
	{
		vector < vector <cTagLocation> > tagSets;
		startCollectTags(debugTrace.traceVerbObjects,verbObjectsTagSet,whereObject,m[whereObject].pma[pmaOffset].pemaByPatternEnd,tagSets,true,false,L"place identification");
		for (int J=0; J<(signed)tagSets.size(); J++)
		{
			tIWMM vWord=wNULL,oWord;
			int vTag=findOneTag(tagSets[J],L"V_OBJECT",-1),vObject=-1,oTag=findOneTag(tagSets[J],L"OBJECT",-1),oObject,whereVObject=-1,whereOObject=-1;
			if (!resolveTag(tagSets[J],vTag,vObject,whereVObject,vWord) || !resolveTag(tagSets[J],oTag,oObject,whereOObject,oWord) || oObject<0) continue;
			whereObject=tagSets[J][oTag].sourcePosition;
			whereVerb=tagSets[J][vTag].sourcePosition;
			st=objects[o=oObject].getSubType();
			if (whereObject>=0 && m[whereObject].endObjectPosition<(signed)m.size() && m[whereObject].endObjectPosition>=0 && 
				  m[m[whereObject].endObjectPosition].queryWinnerForm(prepositionForm)>=0 && 
					((m[m[whereObject].endObjectPosition].word->second.flags&cSourceWordInfo::prepMoveType) || (m[m[whereObject].endObjectPosition].word->first==L"for")) &&
					(wpd=m[m[whereObject].endObjectPosition].getRelObject())>=0 &&
					(pd=m[wpd].getObject())>=0)
			{
				identifyISARelation(wpd,false, RDFFileCaching);
				if (objects[pd].getSubType()<0)
					pd=wpd=-1;
			}
			break;
		}
	}
	if (start && m[whereVerb].getMainEntry()->first==L"start" && 
			whereSubject>=0 && whereObject<0 && whereVerb+1<(signed)m.size() && (m[whereVerb+1].word->second.flags&cSourceWordInfo::prepMoveType) && m[whereVerb+1].word->first!=L"to")
	{
		newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,whereVerb+1,whereObject,-1,-1,stMOVE,L"start",pr);
		return true;
	}
	bool physicalSubject=true;
	if (whereSubject>=0 && m[whereSubject].getObject()>=0 && objects[m[whereSubject].getObject()].neuter && !objects[m[whereSubject].getObject()].female && !objects[m[whereSubject].getObject()].male)
	{
		if (m[whereSubject].flags&cWordMatch::flagRelativeHead)
			whereSubject=objects[m[whereSubject].getObject()].firstLocation;
		tIWMM me=(m[whereSubject].word->second.mainEntry==wNULL) ? m[whereSubject].word : m[whereSubject].word->second.mainEntry;
		physicalSubject=m[whereSubject].getObject()>=0 && objects[m[whereSubject].getObject()].getSubType()>=0 || !(me->second.flags&cSourceWordInfo::notPhysicalObjectByWN);
		if (m[whereSubject].objectMatches.size())
		{
			if (m[whereSubject].getObject()>=0)
				physicalSubject=objects[m[whereSubject].getObject()].getSubType()>=0;
			int subType=-1,w=-1,flags=-1;
			for (unsigned int I=0; I<m[whereSubject].objectMatches.size() && !physicalSubject; I++)
			{
				w=objects[m[whereSubject].objectMatches[I].object].originalLocation;
				me=(m[w].word->second.mainEntry==wNULL) ? m[w].word : m[w].word->second.mainEntry;
				physicalSubject=(subType=objects[m[whereSubject].objectMatches[I].object].getSubType())>=0 || !(flags=me->second.flags&cSourceWordInfo::notPhysicalObjectByWN);
			}
		}
		if (!physicalSubject)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:PLACE identification:rejected subject %s (mainEntry %s) - not physical",where,whereString(whereSubject,tmpstr,true).c_str(),me->first.c_str());
			pr=false;
			//if (!(think || communicate || startRelationType || cos || (m[whereVerb].flags&cWordMatch::flagUsedBeRelation) || vbNetClasses[vnClass].am || stay ||
			//	m[whereVerb].flags&cWordMatch::flagUsedPossessionRelation || metaInfo || vbNetClasses[vnClass].enter || vbNetClasses[vnClass].metaProfession))
			//	return false;
		}
	}
	//  My[tuppence] plan is this , ” Tuppence went on calmly 
	// verbs of self movement with direct objects 51.1
	//	 He scaled the tree  - verbs of movement that don't require prepositions to indicate movement
	//   He exited the room 
	// Tuppence remained for a few minutes
	// Tommy kept it
	// which Tuppence had retained
	// Tuppence took (bring-11.3) it.  Tuppence received (obtain) it
	if ((!(m[where].objectRole&PRIMARY_SPEAKER_ROLE) || whereObject>=0) && 
		  (whereSubject>=0 || ((m[where].flags&cWordMatch::flagInQuestion)!=0 && inPrimaryQuote)) && (exit || enter || stay || move || moveObject || moveInPlace || contiguous)) 
	{
		bool wpoPhysicalObject=true,wpoTimeUnit;
		int wherePrep=-1,wherePrepObject=findAnyLocationPrepObject(whereVerb,wherePrep,wpoPhysicalObject,wpoTimeUnit);
		bool woPhysicalObject=whereObject<0 || proLocation || (m[whereObject].getObject()>=0 && objects[m[whereObject].getObject()].getSubType()>=0) ||
			  (m[whereObject].relNextObject<0 && !(m[whereObject].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN) && m[whereObject].getObject()>=0 && objects[m[whereObject].getObject()].objectClass!=BODY_OBJECT_CLASS);
		// get is special case - for 'get' MUST be going somewhere which is a 'place'
		if (id==L"escape-51.1-5" && m[whereVerb].getMainEntry()->first==L"get" && !proLocation && (whereObject<0 || m[whereObject].relNextObject<0))
		{
			if (wherePrepObject>=0 && (m[wherePrepObject].getObject()<0 || objects[m[wherePrepObject].getObject()].getSubType()<0))
				wpoPhysicalObject=false;
			if (wherePrepObject<0) wpoPhysicalObject=woPhysicalObject=false;
		}
		// if it has an object, there must be only one object and that object must not be nonphysical.
		// if it has a prepobject, that object must be physical or a time
		if ((id!=L"escape-51.1-5" || whereObject<0 || (m[whereObject].word->second.timeFlags&T_UNIT)!=0 || proLocation || m[whereObject].relNextObject>=0) && 
			  ((wherePrepObject<0 || whereObject>=0) && woPhysicalObject) || 
				 (wherePrepObject>=0 && (wpoPhysicalObject || (wpoTimeUnit && id!=L"escape-51.1-5"))))
		{
			if (wpoTimeUnit) wherePrepObject=-1;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:PLACE transition accepted:SUBJ[%s] V[%s] O[%s %d:%s] PO[%s%s %d:%s]",where,
					whereString(whereSubject,tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),
					(whereObject>=0) ? ((!woPhysicalObject) ? L"P":L"NP") : L"",
					whereObject,whereString(whereObject,tmpstr2,true).c_str(),
					(wherePrepObject>=0) ? ((!wpoPhysicalObject) ? L"P":L"NP") : L"",
					(wpoTimeUnit) ? L"T":L"",
					wherePrepObject,whereString(wherePrepObject,tmpstr3,true).c_str());
			bool woTimeUnit=(whereObject>=0 && ((m[whereObject].word->second.timeFlags&T_UNIT) || (m[whereObject].beginObjectPosition>=0 && m[m[whereObject].beginObjectPosition].pma.queryPattern(L"_TIME")!=-1)));
			int srType=(stay) ? stSTAY:stESTABLISH;
			if (move && srType==stESTABLISH) srType=stMOVE;
			if (moveObject && !move && srType==stESTABLISH) srType=stMOVE_OBJECT;
			if (moveInPlace && !move && srType==stESTABLISH) srType=stMOVE_IN_PLACE;
			if (exit) srType=(whereObject>=0 && st<0) ? stMOVE : stEXIT;
			if (exit && whereObject>=0 && st<0 && !woTimeUnit &&
				  m[whereObject].getObject()>=0 && !objects[m[whereObject].getObject()].isAgent(true) && objects[m[whereObject].getObject()].objectClass!=VERB_OBJECT_CLASS)
				srType=stMOVE_OBJECT;
			if (exit && whereObject>=0 && st>=0)
			{
				wstring strtmp2,strtmp3;
				lplog(LOG_RESOLUTION,L"%06d:EXIT? %s %s %s? whereObject>=0(%d) && st>=0(%d)",
							where,whereString(whereSubject,strtmp2,true).c_str(),
							m[whereVerb].word->first.c_str(),
							whereString(whereObject,strtmp2,true).c_str(),
							whereObject,st);
				if (st==UNKNOWN_PLACE_SUBTYPE) srType=stMOVE_OBJECT;
			}
			if ((contiguous || contact) && (srType!=stMOVE_IN_PLACE || whereObject>=0)) srType=stCONTACT;
			if ((srType==stMOVE_OBJECT || srType==stCONTACT) && (whereObject>=0 && (m[whereObject].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN)) && (!wpoPhysicalObject || wpoTimeUnit))
				pr=false;
			if (_near) srType=stNEAR;
			if (enter) srType=stENTER;
			if (((id==L"escape-51.1-5" && enter) || id==L"give-13.1-1") && whereObject>=0 && m[whereObject].relNextObject>=0)
				srType=stTRANSFER;
			// she took the bus
			// She set foot in England
			if (srType==stMOVE_OBJECT && whereObject>=0 && 
				  (((st==MOVING || isRelativeLocation(m[whereObject].word->first)) && (id==L"bring-11.3" || m[whereVerb].getMainEntry()->first==L"turn")) || 
					 (m[whereObject].word->first==L"foot" && m[whereObject-1].queryWinnerForm(determinerForm)==-1) || 
					 (whereSubject>=0 && intersect(whereSubject,whereObject) && m[whereObject].getObject()>=0 && objects[m[whereObject].getObject()].objectClass!=BODY_OBJECT_CLASS)))
				srType=stMOVE;
			// they were at once taken up to his suite.
			// MOVE_OBJECT with no object, acceptable prep object and passive verb
			// MOVE_OBJECT Tuppence took up their[tuppence,tommy] abode forthwith at the Ritz 
			if (srType==stMOVE_OBJECT && whereVerb>=0 && 
				  ((whereObject<0  && (m[whereVerb].verbSense&VT_PASSIVE) && wherePrepObject>=0 && m[wherePrepObject].getObject()>=0 && objects[m[wherePrepObject].getObject()].getSubType()>=0) ||
					 (st>=0 && st!=UNKNOWN_PLACE_SUBTYPE)))
				srType=stMOVE;
			// He went ashore.  
			if ((srType==stMOVE || srType==stMOVE_OBJECT) && 
				  ((proLocation && m[whereVerb].getMainEntry()->first!=L"turn") || // She[annette] turned away. (57842)
					// He swung himself aboard.  
					(whereObject>=0 && adverbialPlace(m[whereObject].endObjectPosition) && intersect(whereObject,whereSubject)) ||
					// He took himself to England. // change-of-place
					exitConversion(whereObject,whereSubject,wherePrepObject)))
				srType=stEXIT;
		  //if (whereSubject>=0 && (m[whereSubject].objectRole&SUBJECT_PLEONASTIC_ROLE))
			//	return false;
			if (!inPrimaryQuote && (m[whereSubject].getObject()<0 || objects[m[whereSubject].getObject()].male || objects[m[whereSubject].getObject()].female))
				moveIdentifiedSubject(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wherePrepObject,-1,srType,L"exitMoveExist MIS",pr);
			else 
				newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wherePrepObject,-1,srType,L"exitMoveExist",pr);
			return true;
		}
		else if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:PLACE transition rejected:SUBJ[%s] V[%s] O[%s %d:%s] PO[%s%s %d:%s]",where,
					whereString(whereSubject,tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),
					(whereObject>=0) ? ((!woPhysicalObject) ? L"P":L"NP") : L"",
					whereObject,whereString(whereObject,tmpstr2,true).c_str(),
					(wherePrepObject>=0) ? ((!wpoPhysicalObject) ? L"P":L"NP") : L"",
					(wpoTimeUnit) ? "T":"",
					wherePrepObject,whereString(wherePrepObject,tmpstr3,true).c_str());
	}
	// check if object is neuter and not a place.
	bool isMatchedLocation=true;
	if (whereObject>=0 && (o=m[whereObject].getObject())>=0 && objects[o].male && objects[o].female && objects[o].neuter && m[whereObject].objectMatches.size())
	{
		isMatchedLocation=adverbialPlace(whereObject) || objects[o].getSubType()>=0;
		for (int I=0; I<(signed)m[whereObject].objectMatches.size() && !isMatchedLocation; I++)
			isMatchedLocation=objects[m[whereObject].objectMatches[I].object].male || objects[m[whereObject].objectMatches[I].object].female || objects[m[whereObject].objectMatches[I].object].getSubType()>=0;
	}
	if (whereObject>=0 && m[whereObject].relNextObject<0 && isMatchedLocation && id!=L"escape-51.1-5" &&
		  acceptableVerbForm && !prepLocation && !prepMustBeLocation &&
			((st>=0 && st!=UNKNOWN_PLACE_SUBTYPE) || ((move || moveObject || moveInPlace || transfer || contact || _near || contiguous || stay) && !(m[whereObject].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN)) || 
			 (o>=0 && (objects[o].objectClass==PRONOUN_OBJECT_CLASS || objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS) && (objects[o].male || objects[o].female))))
	{
		wchar_t *whereType;
		// North of the Andes.  The Tube exit.  
		if (st==RELATIVE_DIRECTION || st==ABSOLUTE_DIRECTION)
		{
			// if Proper Noun, check for 'of' following, with a place object.
			// North of the Andes.  
			if (o>=0 && objects[o].objectClass==NON_GENDERED_NAME_OBJECT_CLASS && m[whereObject].endObjectPosition<(signed)m.size() && 
				  m[m[whereObject].endObjectPosition].word->first==L"of" && m[m[whereObject].endObjectPosition].getRelObject()>=0 && m[m[m[whereObject].endObjectPosition].getRelObject()].getObject()>=0 &&
					objects[m[m[m[whereObject].endObjectPosition].getRelObject()].getObject()].isPossibleSubType(false))
			  o=m[whereObject=m[m[whereObject].endObjectPosition].getRelObject()].getObject();
			// if not Proper Noun, check for a NONGENDERED_NAME place adjective
			// The Tube exit.  
			if (o>=0 && objects[o].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS)
			{
				for (int I=m[whereObject].beginObjectPosition,tpo; I<whereObject; I++)
					if ((tpo=m[I].getObject())>=0 && tpo!=o && objects[tpo].objectClass==NON_GENDERED_NAME_OBJECT_CLASS &&
							objects[tpo].isPossibleSubType(false))
					{
						o=tpo;
						whereObject=I;
						break;
					}
			}
			whereType=L"place single object direction";
		}
		else
			whereType=L"place single object";
		if (id==L"am" && (m[whereObject].queryWinnerForm(indefinitePronounForm)>=0 || m[whereObject].queryWinnerForm(pronounForm)>=0) && 
			  m[whereObject].word->first!=L"there")
		{
			if (debugTrace.traceWhere)
			lplog(LOG_RESOLUTION,L"%06d:PLACE place single object:rejected (is with indefinite pronoun)",where);
			return false;
		}
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:PLACE place single object:subject %s verb %s object %s - st type %s",where,
				(whereSubject<0) ? L"(None)" : whereString(whereSubject,tmpstr,true).c_str(),
				m[whereVerb].word->first.c_str(),whereString(whereObject,tmpstr2,true).c_str(),
				(st<0) ? L"" : OCSubTypeStrings[st]);
		int srType=(stay) ? stSTAY:stESTABLISH;
		if (move && srType==stESTABLISH) srType=stMOVE;
		if (moveObject && !move && srType==stESTABLISH) srType=stMOVE_OBJECT;
		if (moveInPlace && !move && srType==stESTABLISH) srType=stMOVE_IN_PLACE;
		if (exit) srType=(whereObject>=0) ? stMOVE : stEXIT;
		if ((contiguous || contact) && (srType!=stMOVE_IN_PLACE || whereObject>=0)) srType=stCONTACT;
		if (_near) srType=stNEAR;
		if (enter) srType=stENTER;
		if (whereSubject>=0 && !inPrimaryQuote)
			moveIdentifiedSubject(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,-1,whereObject,-1,wpd,srType,whereType,pr);
		else 
			newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,-1,whereObject,-1,wpd,srType,whereType,pr);
		return true;
	}
	else if (whereObject>=0 && debugTrace.traceWhere)
		lplog(LOG_WHERE,L"%06d:PLACE place single object:rejected [object is gendered or location?](%d:%d acceptableVerb=%s prepLocation=%s prepMustBeLocation=%s st=%d class=%s isMatchedLocation=%s)",where,
			whereObject,m[whereObject].relNextObject,(acceptableVerbForm) ? L"true":L"false",
			(prepLocation) ? L"true":L"false",(prepMustBeLocation) ? L"true":L"false",st,
			(m[whereObject].getObject()>=0) ? getClass(objects[m[whereObject].getObject()].objectClass).c_str() : L"",
			(isMatchedLocation) ? L"true":L"false");
	if (objectMustBeLocation && !prepMustBeLocation) return false;
	// She went away
	if (whereObject<0 && m[whereVerb].getRelObject()<0 && afterVerb>whereVerb+1 && m[whereVerb+1].word->first==L"away")
	{
		if (whereSubject>=0 && !inPrimaryQuote)
			moveIdentifiedSubject(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,-1,-1,whereObject,-1,stEXIT,L"away",pr);
		else 
			newSR(where,(whereSubject>0) ? m[whereSubject].getObject() : -1,whereControllingEntity,whereSubject,whereVerb,-1,-1,whereObject,-1,stEXIT,L"away",pr);
		return true;
	}				  
	// physical object has been defined above
	bool physicalObject=whereObject>=0 && !(m[whereObject].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN);
	if (physicalObject && m[whereObject].objectMatches.size())
	{
		physicalObject=false;
		for (int I=0; I<(signed)m[whereObject].objectMatches.size(); I++)
			if (!(m[objects[m[whereObject].objectMatches[I].object].originalLocation].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN) &&
					m[objects[m[whereObject].objectMatches[I].object].originalLocation].queryWinnerForm(demonstrativeDeterminerForm)<0)
				physicalObject=true;
	}
	// if preposition, check for relObject.
	// transitioning prepositions 
	//   the girl[girl] went in the boat .
	//   He moved into the room
	//   Behind the station.
	//   Tommy sat down opposite her.
	int wherePrep=m[whereVerb].relPrep,whereLastPrepObject=-1,prepLoop=0;
	bool specificVerb=contact || _near || transfer || move || moveObject || moveInPlace || contiguous || id==L"am";
	for (; wherePrep>=0; wherePrep=m[wherePrep].relPrep)
	{
		if (prepLoop++>30)
		{
			lplog(LOG_ERROR,L"%06d:Prep loop occurred (4) %s.",wherePrep,loopString(wherePrep,tmpstr));
			return false;
		}
		// in the presence of XX
		bool multiWordMovement=m[wherePrep].word->first==L"of" && wherePrep>4 &&
			  (m[wherePrep-3].word->first==L"in" || m[wherePrep-3].word->first==L"into") && m[wherePrep-2].word->first==L"the" && m[wherePrep-1].word->first==L"presence";
		if (whereLastPrepObject!=-1 && m[whereLastPrepObject].endObjectPosition==wherePrep && !multiWordMovement)
			continue; // reject prep phrases immediately following other prep phrases from consideration (change in future based on prep binding)
		multiWordMovement|=(wherePrep>=1 && m[wherePrep].word->first==L"of" && 
			  (m[wherePrep-1].word->first==L"out" || m[wherePrep-1].word->first==L"inside" || m[wherePrep-1].word->first==L"outside" || 
				 m[wherePrep-1].word->first==L"ahead" || m[wherePrep-1].word->first==L"abreast"));  // he comes out of the building
		// en route for Chester
		multiWordMovement|=wherePrep>=2 && m[wherePrep].word->first==L"for" && m[wherePrep-1].word->first==L"route" && m[wherePrep-2].word->first==L"en";
		multiWordMovement|=wherePrep>=2 && m[wherePrep].word->first==L"with" && (m[wherePrep-1].word->first==L"level" || m[wherePrep-1].word->first==L"even");
		bool withStart=start && m[wherePrep].word->first==L"with"; // We[tuppence,tommy] shall start with the London area 
		if (!multiWordMovement && !withStart && !(m[wherePrep].word->second.flags&cSourceWordInfo::prepMoveType) && (contact || _near || m[wherePrep].word->first!=L"for")) continue;
		int prepType=prepTypesMap[m[wherePrep].word->first],wpoo=-1,wherePrepObject=m[wherePrep].getRelObject(),stType;
		if (wherePrepObject<0 || rejectPrepPhrase(wherePrep)) continue;
		whereLastPrepObject=wherePrepObject;
		bool acceptvAM=(!noPrepTo || (prepType!=tprTO && prepType!=tprFOR)) && (!noPrepFrom || prepType!=tprFROM);
		// He was to the left
		acceptvAM|=(prepType==tprTO && wherePrepObject>=0 && m[wherePrepObject].getObject()>=0 && objects[m[wherePrepObject].getObject()].getSubType()==RELATIVE_DIRECTION);
		// I am away to Paris! / I will be out to lunch.
		acceptvAM=wherePrep>=1 && (acceptvAM || m[wherePrep-1].word->first==L"away" || m[wherePrep-1].word->first==L"off" || m[wherePrep-1].word->first==L"out" || m[wherePrep-1].word->first==L"opposite");
		bool ot1=false,ot2=false,ot3=false,ot4=false;
		if (whereObject<0 && 
				(((o=m[wherePrepObject].getObject())>=0 && acceptvAM &&
				 ((ot1=objects[o].isPossibleSubType(false)) || 
				  // at the top of the stairs
				  (ot4=m[wherePrep].relPrep>=0 && (m[wherePrep].relPrep-wherePrepObject)<2 && m[m[wherePrep].relPrep].word->first==L"of" && m[m[wherePrep].relPrep].getRelObject()>=0 && 
							// not 'through the survivors of the Lusitania'
							(!objects[o].male && !objects[o].female) &&
					     m[m[m[wherePrep].relPrep].getRelObject()].getObject()>=0 && objects[m[m[m[wherePrep].relPrep].getRelObject()].getObject()].isPossibleSubType(false)) ||
				  (ot2=((objects[o].male || objects[o].female) && m[wherePrepObject].queryWinnerForm(indefinitePronounForm)<0 && acceptableVerbForm && (!prepMustBeLocation || specificVerb))) ||
				  (ot3=((m[wherePrepObject].word->second.flags&cSourceWordInfo::physicalObjectByWN) && !prepMustBeLocation && acceptableVerbForm && 
					  (m[m[wherePrepObject].beginObjectPosition].queryForm(determinerForm)>=0 || m[m[wherePrepObject].beginObjectPosition].queryForm(possessiveDeterminerForm)>=0 || 
						 m[m[wherePrepObject].beginObjectPosition].queryForm(demonstrativeDeterminerForm)>=0 || objects[o].plural))))) ||
				 (m[wherePrepObject].queryWinnerForm(relativizerForm)>=0 && (wpoo=m[wherePrepObject].getRelObject())>=0 && (o=m[wpoo].getObject())>=0 && objects[o].isPossibleSubType(false))))
		{
			if (wpoo>=0) wherePrepObject=wpoo;
			if ((contiguous || contact) && (!moveInPlace || whereObject>=0)) 
				stType=(prepType==tprFROM) ? stMOVE : stCONTACT;
			else if (exit)
				stType=stEXIT;
			else if (move)
				stType=stMOVE;
			else if (moveObject)
				stType=stMOVE_OBJECT;
			else if (moveInPlace)
				stType=stMOVE_IN_PLACE;
			else if (enter) 
				stType=stENTER;
			else if (_near)
				stType=stNEAR;
			else
				stType=vbNetClasses[vnClass].getRelationType();
			wstring otTypeStr=L"prep no direct object ";
			if (ot1) otTypeStr+=L"[1]";
			if (ot2) otTypeStr+=L"[2]";
			if (ot3) otTypeStr+=L"[3]";
			if (ot4) otTypeStr+=L"[4]";
			if (m[wherePrepObject].word->second.flags&cSourceWordInfo::physicalObjectByWN) otTypeStr+=L"[PO]";
			if (m[wherePrepObject].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN) otTypeStr+=L"[NPO]";
			if (m[wherePrepObject].word->second.flags&cSourceWordInfo::uncertainPhysicalObjectByWN) otTypeStr+=L"[UPO]";
			int stpo;
			if (stType==stESTABLISH && wherePrepObject>=0 && m[wherePrepObject].getObject()>=0 && ((stpo=objects[m[wherePrepObject].getObject()].getSubType())==MOVING || stpo==MOVING_NATURAL)) 
				// the two young ESTABmen[man,conrad] were seated in a first - class carriage en route for Chester . 
			{
				int alternatePrep = wherePrep;
				prepLoop = 0;
				bool location=false,timeUnit=false;
				while (findAnyLocationPrepObject(whereVerb,alternatePrep,location,timeUnit)>=0 && alternatePrep>=0 && prepLoop<20)
				{
					prepLoop++;
					if (location && exitConversion(whereObject,whereSubject,m[alternatePrep].getRelObject()))
					{
						lplog(LOG_RESOLUTION,L"%06d:found exit @%d instead of weaker establishment @%d.",where,alternatePrep,m[alternatePrep].getRelObject());
						stType=stEXIT;
						if (ot4) otTypeStr+=L"[+EXIT"+itos(alternatePrep,tmpstr)+L"]";
						break;
					}
				}
			}
			if (whereSubject>=0 && !inPrimaryQuote)
				moveIdentifiedSubject(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,wherePrep,-1,wherePrepObject,-1,stType,otTypeStr.c_str(),pr);
			else 
				newSR(where,(whereSubject>0) ? m[whereSubject].getObject() : -1,whereControllingEntity,whereSubject,whereVerb,wherePrep,-1,wherePrepObject,-1,stType,otTypeStr.c_str(),pr);
			m[wherePrep].hasSyntacticRelationGroup=true; // so that the prepositions bound to an object won't pick this one up.
			return true;
		}
		if (whereObject==wherePrepObject) whereObject=-1;
		if (!acceptvAM || (objectMustBeLocation && whereObject<0)) continue;
		// if qualifying preposition after object, then this pertains only to the object
		//  take the packet to the American Embassy
		//  deliver it[packet] into the Ambassador's own hands[ambassador]
		if (m[wpd=wherePrepObject].objectMatches.size()==1)
			pd=m[wpd].objectMatches[0].object;
		else
			pd=m[wpd].getObject();
		if (whereObject>=0 && acceptableVerbForm && physicalObject && pd>=0 && (id!=L"escape-51.1-5" || whereObject<0 || (m[whereObject].word->second.timeFlags&T_UNIT)!=0 || proLocation) &&
				(objects[pd].isPossibleSubType(false) || objects[pd].male || objects[pd].female || (!prepMustBeLocation && !(m[wpd].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN))))
		{
			if ((m[wpd].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN) || (m[wpd].word->second.timeFlags&T_UNIT)!=0 || objects[pd].objectClass==VERB_OBJECT_CLASS)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:PLACE direct object and prep: rejected (prep object not physical) subject %s verb %s object %d:%s prep object %d:%s",where,
						whereString(whereSubject,tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),whereObject,whereString(whereObject,tmpstr2,true).c_str(),wpd,whereString(wpd,tmpstr3,true).c_str());
				continue;
			}
			bool rejectObject=true,rejectPrep=true;
			if (objectMustBeLocation && (rejectObject=(!(m[whereObject].word->second.flags&cSourceWordInfo::physicalObjectByWN) && (m[whereObject].getObject()<0 || !objects[m[whereObject].getObject()].isPossibleSubType(false)))))
			{

				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:PLACE direct object and prep: rejected (direct object not location) subject %s verb %s object %d:%s",where,
						whereString(whereSubject,tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),whereObject,whereString(whereObject,tmpstr2,true).c_str());
				if (!prepMustBeLocation) continue;
			}
			if (prepMustBeLocation && (rejectPrep=(!(m[wpd].word->second.flags&cSourceWordInfo::physicalObjectByWN) && !objects[pd].isPossibleSubType(false))))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:PLACE direct object and prep: rejected (prep object not location) subject %s verb %s prep object %d:%s",where,
						whereString(whereSubject,tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),wpd,whereString(pd,tmpstr2,true).c_str());
				if (!objectMustBeLocation || rejectObject) continue;
			}
			// permitted objects: // half-way TODO
			if ((contiguous || contact) && (!moveInPlace || whereObject>=0)) 
				stType=(prepType==tprFROM) ? stMOVE : stCONTACT;
			else if (exit)
				stType=stEXIT;
			else if (move)
				stType=stMOVE;
			else if (moveObject)
				stType=stMOVE_OBJECT;
			else if (moveInPlace)
				stType=stMOVE_IN_PLACE;
			else if (enter) 
				stType=stENTER;
			else if (_near)
				stType=stNEAR;
			else 
				stType=vbNetClasses[vnClass].getRelationType();
			if (like(id,L"am") && (whereSubject<0 || m[whereSubject].word->first!=L"where") && m[whereObject].queryWinnerForm(numeralOrdinalForm)<0) 
			{
				if (whereSubject<0 || adverbialPlace(whereSubject))
				{
					whereControllingEntity=whereSubject;
					whereSubject=whereObject;
					whereObject=-1;
				}
				newSR(where,(whereSubject>0) ? m[whereSubject].getObject() : -1,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wpd,-1,stESTABLISH,L"am",pr);
			}
			else if (m[whereObject].endObjectPosition<(signed)m.size() && m[whereObject].endObjectPosition>=0 && 
				       prepTypesMap[m[m[whereObject].endObjectPosition].word->first]==tprTO)
				srMoveObject(where,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,-1,wpd,stType,L"direct object and prep to",pr);
			else
				srMoveObject(where,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,wpd,-1,stType,L"direct object and prep",pr);
			return true;
		}
	}
	//  take the packet there
	//  go there
	if (whereObject>=0 && acceptableVerbForm && physicalObject && (adverbialPlace(m[whereObject].endObjectPosition) || adverbialPlace(whereObject)))
	{
		int stType=stESTABLISH;
			if ((contiguous || contact) && (!moveInPlace || whereObject>=0)) 
			stType=stCONTACT;
		else if (exit)
			stType=stEXIT;
		else if (move)
			stType=stMOVE;
		else if (moveObject)
			stType=stMOVE_OBJECT;
		else if (moveInPlace)
			stType=stMOVE_IN_PLACE;
		else if (enter) 
			stType=stENTER;
		else if (_near)
			stType=stNEAR;
		else
			stType=vbNetClasses[vnClass].getRelationType();
		if (id==L"escape-51.1-5" && !adverbialPlace(whereObject))
			stType=stMOVE_OBJECT;
		newSR(where,(whereSubject>0) ? m[whereSubject].getObject() : -1,whereControllingEntity,whereSubject,whereVerb,-4,whereObject,m[whereObject].endObjectPosition,-1,stType,(m[whereObject].endObjectPosition>=0) ? m[m[whereObject].endObjectPosition].word->first.c_str():L"",pr);
		return true;
	}
	// she was gone.  she was dead.
	if (whereSubject>=0 && (m[whereSubject].objectRole&IS_OBJECT_ROLE) && whereObject<0 && whereVerb>=0)
	{
		for (int wa=whereVerb+1; wa<(signed)m.size() && (m[wa].queryWinnerForm(adverbForm)!=-1 || m[wa].queryWinnerForm(adjectiveForm)!=-1); wa++)
		{
			if (adjectivalExit(wa))
			{
				lplog(LOG_RESOLUTION,L"%06d:found adjectival exit @%d",where,whereVerb+1);
				if (!inPrimaryQuote)
					moveIdentifiedSubject(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,-1,-1,whereObject,-1,stEXIT,L"adverbialExit",pr);
				else 
					newSR(where,m[whereSubject].getObject(),whereControllingEntity,whereSubject,whereVerb,-1,-1,whereObject,-1,stEXIT,L"adverbialExit",pr);
				return true;
			}
			// together
			if (m[wa].word->first==L"together")
			{
				newSR(where,m[whereSubject].getObject(),whereControllingEntity,whereSubject,whereVerb,-1,-1,whereObject,-1,stCONTACT,L"adverbialContact",pr);
				return true;
			}
		}
	}
	// don't you know where she is?
	// you could be where she is.
	if (whereSubject>=0 && m[whereSubject].getObject()>=0 && m[whereSubject].beginObjectPosition>0 && m[m[whereSubject].beginObjectPosition-1].word->first==L"where")
	{
		newSR(where,m[whereSubject].getObject(),whereControllingEntity,whereSubject,whereVerb,-3,whereObject,m[whereSubject].beginObjectPosition-1,-1,stLOCATIONRP,L"location relative phrase",pr);
		return true;
	}
	// [tuppence] gave him the book
	// Tuppence sent him the book
	int nwo=-1;
	if (transfer && whereObject>=0 && (nwo=m[whereObject].relNextObject)>=0)
	{
		if (m[nwo].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN) // || objects[m[nwo].getObject()].isAgent(true))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:PLACE transfer: rejected (nwo not physical) subject %s verb %s object %d:%s %d:%s",where,
					whereString(whereSubject,tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),whereObject,whereString(whereObject,tmpstr2,true).c_str(),nwo,whereString(nwo,tmpstr3,true).c_str());
			return false;
		}
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:PLACE transfer: subject %s verb %s object %d:%s %d:%s",where,
				whereString(whereSubject,tmpstr,true).c_str(),m[whereVerb].word->first.c_str(),whereObject,whereString(whereObject,tmpstr2,true).c_str(),nwo,whereString(nwo,tmpstr3,true).c_str());
		newSR(where,(whereSubject>0) ? m[whereSubject].getObject() : -1,whereControllingEntity,whereSubject,whereVerb,-2,whereObject,nwo,-1,stTRANSFER,L"indirect object",pr);
		return true;
	}
	// commands
	// come with me.
	bool tmp1=false,tmp2=false,tmp3=false,tmp4=false,tmp5=false; // debugging flags
	int flags;
	if ((tmp1=(m[where].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0) && 
	    //(whereObject<0 || (whereObject>=0 && m[whereObject].relNextObject<0 && (m[whereObject].word->second.timeFlags&T_UNIT))) && // command - come to-morrow
		  (tmp2=whereSubject<0 && !(m[whereVerb].flags&cWordMatch::flagInInfinitivePhrase)) &&
		  (tmp3=(exit || stay || move || moveObject || moveInPlace || contiguous || enter || contact || _near)) && 
			(tmp4=(m[whereVerb].verbSense&(VT_TENSE_MASK|VT_EXTENDED))==VT_PRESENT) &&
			(tmp5=((flags=m[whereVerb].word->second.inflectionFlags)&(VERB_PRESENT_FIRST_SINGULAR))!=0 || m[whereVerb].word->first==L"be"))
	{
		if (debugTrace.traceSpeakerResolution)
		{
			wstring ss;
			lplog(LOG_RESOLUTION,L"%06d:PLACE command:verb %s [%s]",where,m[whereVerb].word->first.c_str(),senseString(ss,m[whereVerb].verbSense).c_str());
		}
		int srType=(stay) ? stSTAY:stESTABLISH;
		if (move && srType==stESTABLISH) srType=stMOVE;
		if (moveObject && !move && srType==stESTABLISH) srType=stMOVE_OBJECT;
		if (moveInPlace && !move && srType==stESTABLISH) srType=stMOVE_IN_PLACE;
		if (exit) srType=(whereObject>=0) ? stMOVE : stEXIT;
		if ((contiguous || contact) && (srType!=stMOVE_IN_PLACE || whereObject>=0)) srType=stCONTACT;
		if (_near)	srType=stNEAR;
		if (enter) srType=stENTER;
		newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,-1,-1,-1,-1,srType,L"exitMoveExist command",pr);
		return true;
	}
	// verb=go prep=to prepObject=XXX
	// also 'where is XXX'
	// go to lunch
	//if ((move || exit) && speakerGroupsEstablished)
	//{
	//	for (wherePrep=m[whereVerb].relPrep; wherePrep>=0; wherePrep=m[wherePrep].relPrep)
	//		if (((m[wherePrep].word->second.flags&cSourceWordInfo::prepMoveType) || m[wherePrep].word->first==L"for") && !rejectPrepPhrase(wherePrep))
	//			lplog(LOG_WHERE,L"%06d:activity noun PLACE:verb %s %s object %s",where,m[whereVerb].word->first.c_str(),m[wherePrep].word->first.c_str(),whereString(m[wherePrep].relObject,tmpstr,false).c_str());
	//}
	vector <cSyntacticRelationGroup>::iterator sri=findSyntacticRelationGroup(where);
	if (sri!=syntacticRelationGroups.end() && sri->where==where) 
		return false;
	
	pr=contact;
	int relationType=vbNetClasses[vnClass].getRelationType();
	newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,wherePrep,whereObject,-1,-1,relationType,vbNetClasses[vnClass].incorporatedVerbClassString(tmpstr).c_str(),pr);
	return false;
}

void cSource::defineObjectAsSpatial(int where)
{ LFS
		wstring tmpstr,tmpstr2;
	int o=m[where].getObject();
	if (o>=0 && !objects[o].isNotAPlace && 
			(objects[o].getSubType()>=0) && // || (objects[o].originalLocation==where && objects[o].male && objects[o].female && m[where].objectMatches.empty())) && 
			objects[o].objectClass==NAME_OBJECT_CLASS &&
			!objects[o].neuter && !objects[o].numIdentifiedAsSpeaker)
	{
		bool verbIsMoveClass=false;
		if (m[where].getRelVerb()>=0 && (m[where].objectRole&OBJECT_ROLE))
		{
			wstring verb,id;
			unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(m[where].getRelVerb(),verb);
			if (lvtoCi!=vbNetVerbToClassMap.end())
				for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
					verbIsMoveClass= ((id=vbNetClasses[*vbi].name())==L"escape-51.1-3" || id==L"withdraw-82-3" || id==L"withdraw-82-1"); // exiting and entering
		}						
		if ((m[where].objectRole&MOVEMENT_PREP_OBJECT_ROLE) || verbIsMoveClass)
		{
			objects[o].objectClass=NON_GENDERED_NAME_OBJECT_CLASS;
			objects[o].neuter=true;
			objects[o].male=objects[o].female=false;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:object %s narrowed to neuter through movement prep and subType.",where,objectString(o,tmpstr,false).c_str());
		}
	} 
}     

void cSource::detectTenseAndFirstPersonUsage(int where, int lastBeginS1, int lastRelativePhrase, int &numPastSinceLastQuote, int &numNonPastSinceLastQuote, int &numFirstInQuote, int &numSecondInQuote, bool inPrimaryQuote)
{
	if (inPrimaryQuote && m[where].getRelVerb() >= 0 && lastBeginS1 > lastRelativePhrase && (m[where].objectRole&(OBJECT_ROLE | SUBJECT_ROLE)) > 0)
	{
		wstring tmpstr;
		bool isSubject = (m[where].objectRole&(OBJECT_ROLE | SUBJECT_ROLE)) == SUBJECT_ROLE; // only count the verb tense once
		// getQuoteForwardLink() is overloaded with tsSense only for verbs
		int tsSense = m[m[where].getRelVerb()].verbSense;
		//lplog(LOG_RESOLUTION,L"%06d:L Sense %s",where,senseString(tmpstr,tsSense).c_str());
		if ((tsSense&VT_TENSE_MASK) == VT_PAST || (tsSense&VT_TENSE_MASK) == VT_PAST_PERFECT)
		{
			if (isSubject)
				numPastSinceLastQuote++;
			int person = m[where].word->second.inflectionFlags&(FIRST_PERSON | SECOND_PERSON | THIRD_PERSON);
			bool plural = (m[where].word->second.inflectionFlags&PLURAL) == PLURAL;
			if (((person&FIRST_PERSON) && plural) || (person == FIRST_PERSON))  // I, we
				numFirstInQuote++;
			if (m[where].word->first == L"you")  // you (not we)
			{
				numSecondInQuote++;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:L past you detected verb %d:%s", where, m[where].getRelVerb(), senseString(tmpstr, tsSense).c_str());
			}
		}
		else if (isSubject)
			numNonPastSinceLastQuote++;
	}
}

void cSource::identifyHailObjects(int where, int o, int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, bool inPrimaryQuote, bool inSecondaryQuote)
{
	LFS
	bool uniquelyMergable;
	wstring tmpstr,tmpstr2;
	set <int>::iterator stsi;
	if (inPrimaryQuote && o >= 0 && (m[where].objectRole&(HAIL_ROLE | IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE | IN_QUOTE_REFERRING_AUDIENCE_ROLE)) &&
		!(m[where].flags&cWordMatch::flagAdjectivalObject) &&
		objects[o].objectClass == NAME_OBJECT_CLASS && !objects[o].plural)
	{
		bool appMatch = false;
		int appFirstObject = m[where].beginObjectPosition;
		if ((m[where].objectRole&HAIL_ROLE) && appFirstObject > 2 && m[appFirstObject - 1].word->first == L",")
		{
			appFirstObject -= 2;
			while (appFirstObject && m[appFirstObject].getObject() == -1) appFirstObject--;
			if (m[appFirstObject].endObjectPosition + 1 == where && (appMatch = matchByAppositivity(appFirstObject, where)) && debugTrace.traceSpeakerResolution)
				lplog(LOG_SG, L"%06d:%02d     REJECTED hail %s-matched appositively to %d:%s", where, section, objectString(o, tmpstr, true).c_str(), appFirstObject, objectString(m[appFirstObject].getObject(), tmpstr2, true).c_str());
		}
		if (!appMatch)
		{
			resolveObject(where, true, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, true, false, false);
			if (m[where].objectMatches.empty())
			{
				if (!objects[o].name.justHonorific() || m[where].queryForm(L"pinr") < 0)
				{
					o = m[where].getObject();
					if (section < sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(o);
					bool inserted = (unMergable(where, o, tempSpeakerGroup.speakers, uniquelyMergable, true, false, false, false, stsi));
					vector <cLocalFocus>::iterator lsi = in(o);
					if (debugTrace.traceSpeakerResolution && lsi != localObjects.end())
						lplog(LOG_SG, L"%06d:%02d     hail %s %s [%d %d]", where, section, objectString(o, tmpstr, true).c_str(), (inserted) ? L"inserted" : L"merged", lsi->lastWhere, lsi->previousWhere);
					objects[o].PISHail++;
					// make sure this hail is not deleted afterward because of lack of definite references
					if (m[where].objectRole&(IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE | IN_QUOTE_REFERRING_AUDIENCE_ROLE))
						objects[o].PISDefinite++;
				}
				definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(where);
				objects[o].numDefinitelyIdentifiedAsSpeaker++;
				objects[o].numDefinitelyIdentifiedAsSpeakerInSection++;
				if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection + objects[o].numIdentifiedAsSpeakerInSection == 1 && section < sections.size())
					sections[section].speakerObjects.push_back(cOM(o, SALIENCE_THRESHOLD));
			}
			else
				for (vector <cOM>::iterator omi = m[where].objectMatches.begin(); omi != m[where].objectMatches.end(); omi++)
				{
					o = omi->object;
					if (section < sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(o);
					bool inserted = (unMergable(-1, o, tempSpeakerGroup.speakers, uniquelyMergable, true, false, false, false, stsi));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG, L"%06d:%02d     hail %s %s", where, section, objectString(o, tmpstr, true).c_str(), (inserted) ? L"inserted" : L"merged");
					objects[o].PISHail++;
					if (m[where].objectMatches.size() == 1)
					{
						definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(where);
						objects[o].numDefinitelyIdentifiedAsSpeaker++;
						objects[o].numDefinitelyIdentifiedAsSpeakerInSection++;
						if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection + objects[o].numIdentifiedAsSpeakerInSection == 1 && section < sections.size())
							sections[section].speakerObjects.push_back(cOM(o, SALIENCE_THRESHOLD));
					}
				}
		}
	}
}

void cSource::processEndOfSentence(int where,int &lastBeginS1, int &lastRelativePhrase, int &lastCommand, int &lastSentenceEnd, int &uqPreviousToLastSentenceEnd, int &uqLastSentenceEnd,
																	int &questionSpeakerLastSentence,int &questionSpeaker,bool &currentIsQuestion,
																	bool inPrimaryQuote, bool inSecondaryQuote,bool &endOfSentence, bool &transitionSinceEOS,
																	unsigned int &agingStructuresSeen,bool quotesSeenSinceLastSentence,
																	vector <int> &lastSubjects, vector <int> &previousLastSubjects)
{
	wstring tmpstr;
	lastBeginS1 = -1;
	lastRelativePhrase = -1;
	lastCommand = -1;
	endOfSentence = true;
	transitionSinceEOS = false;
	lastSentenceEnd = where;
	// use uqLastSentenceEnd so that space relations use disambiguated objects, and also that SPEAKER_ROLEs that come before their quotes are recognized
	if (!(m[where].objectRole&(IN_PRIMARY_QUOTE_ROLE | IN_SECONDARY_QUOTE_ROLE)) || narrativeIsQuoted)
	{
		int lastTransitionSR = -1, keepPPSpeakerWhere = -1;
		for (int J = uqPreviousToLastSentenceEnd; J < uqLastSentenceEnd; J++)
		{
			if (!(m[J].objectRole&(IN_PRIMARY_QUOTE_ROLE | IN_SECONDARY_QUOTE_ROLE)) || narrativeIsQuoted)
				detectSyntacticRelationGroup(J, where, lastSubjects);
			vector<cSyntacticRelationGroup>::iterator sr;
			if (m[J].hasSyntacticRelationGroup && (sr = findSyntacticRelationGroup(J)) != syntacticRelationGroups.end() && sr->where == J && sr->tft.timeTransition)
			{
				int tmpKeepPPSpeakerWhere = getSpeakersToKeep(sr);
				if (tmpKeepPPSpeakerWhere >= 0 && keepPPSpeakerWhere < 0)
					keepPPSpeakerWhere = tmpKeepPPSpeakerWhere;
				lastTransitionSR = J; // subsequent space relations can remove the time transition from previous ones
			}
		}
		if (lastTransitionSR != -1)
		{
			if (keepPPSpeakerWhere >= 0 && m[keepPPSpeakerWhere].objectMatches.size() > 1)
				lplog(LOG_RESOLUTION, L"%06d:transition is vague - %d.", lastTransitionSR, keepPPSpeakerWhere);
			else
			{
				vector<cSyntacticRelationGroup>::iterator sr = findSyntacticRelationGroup(lastTransitionSR);
				//keepPPSpeakerWhere=getSpeakersToKeep(sr);
				if (sr != syntacticRelationGroups.end())
					ageTransition(sr->where, true, transitionSinceEOS, sr->tft.duplicateTimeTransitionFromWhere, keepPPSpeakerWhere, lastSubjects, L"TSR 1");
			}
		}
		uqPreviousToLastSentenceEnd = uqLastSentenceEnd;
		uqLastSentenceEnd = where;
	}
	if (!inSecondaryQuote || (where + 1 < (signed)m.size() && m[where + 1].word->first == L"’"))
		setQuestion(m.begin() + where, inPrimaryQuote, questionSpeakerLastSentence, questionSpeaker, currentIsQuestion);
	else
		setSecondaryQuestion(m.begin() + where);
	if (!inPrimaryQuote && !inSecondaryQuote && subjectsInPreviousUnquotedSectionUsableForImmediateResolution)
	{
		subjectsInPreviousUnquotedSectionUsableForImmediateResolution = false;
		if (debugTrace.traceSpeakerResolution && subjectsInPreviousUnquotedSection.size())
			lplog(LOG_SG | LOG_RESOLUTION, L"%06d:%02d Cancelling subjectsInPreviousUnquotedSectionUsableForImmediateResolution", where, section);
	}
	if (!inPrimaryQuote && !agingStructuresSeen)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG | LOG_RESOLUTION, L"%06d:%02d     aging speakers (%s) EOS", where, section, (inPrimaryQuote) ? L"inQuote" : L"outsideQuote");
		m[where].flags |= cWordMatch::flagAge;
		for (vector <cLocalFocus>::iterator lfi = localObjects.begin(); lfi != localObjects.end(); )
			ageSpeakerWithoutSpeakerInfo(where, inPrimaryQuote, inSecondaryQuote, lfi, 1);
	}
	agingStructuresSeen = 0;
	if (!inPrimaryQuote)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, L"%d:ZXZ set previousLastSubjects=%s (previous value %s). Cleared lastSubjects", where, objectString(lastSubjects, tmpstr).c_str(), objectString(previousLastSubjects, tmpstr).c_str());
		previousLastSubjects = lastSubjects;
		lastSubjects.clear();
	}
	// look ahead - is this the last sentence in the paragraph?
	// the sentence ends with a period, or a period and a quote.
	if (where + 3 < (signed)m.size() &&
		m[where + 1].word != Words.sectionWord && // period
		!(m[where + 1].word->first == L"”" && m[where + 2].word == Words.sectionWord) && // period and quote
		!(m[where + 1].word->first == L"’" && m[where + 2].word->first == L"”" && m[where + 3].word == Words.sectionWord)) // period, single quote and double quote
	{
		// is the period in the middle of a quote?  if then, set to true.
		// is the period not in a quote, or at the end of a quote? then set to false.
		quotesSeenSinceLastSentence = inPrimaryQuote && (m[where + 1].word->first != L"”" || (m[where + 1].word->first != L"’" && m[where + 2].word->first != L"”"));
		// in the case where a .?! is followed by a quote and a speaker designation,
		// the speaker designation does not count as a sentence.
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, L"%d:(1)quotesSeenSinceLastSentence=%s", where, (quotesSeenSinceLastSentence) ? L"true" : L"false");
	}
}

void cSource::processEndOfPrimaryQuote(int where, int lastSentenceEndBeforeAndNotIncludingCurrentQuote,
																			int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, int &lastSpeakerPosition, int &lastQuotedString, int &quotedObjectCounter,
																			bool &inPrimaryQuote, bool &immediatelyAfterEndOfParagraph, bool &firstQuotedSentenceOfSpeakerGroupNotSeen, bool &quotesSeenSinceLastSentence,
																			vector <int> &lastSubjects)
{
	wstring tmpstr;

	m[lastOpeningPrimaryQuote].endQuote = where;
	inPrimaryQuote = false;
	// if the previous end quote was inserted, and this end quote was also inserted, then this quote has the same speakers as the last
	bool previousEndQuoteInserted = previousPrimaryQuote >= 0 && (m[m[previousPrimaryQuote].endQuote].flags&cWordMatch::flagInsertedQuote) != 0 && (m[where].flags&cWordMatch::flagInsertedQuote) != 0;
	bool noTextBeforeOrAfter, quotedStringSeen = false, multipleQuoteInSentence = (previousPrimaryQuote > lastSentenceEndBeforeAndNotIncludingCurrentQuote), noSpeakerAfterward = false;
	if (!(quotedStringSeen = quotedString(lastOpeningPrimaryQuote, where, noTextBeforeOrAfter, noSpeakerAfterward)))
	{
		if (!multipleQuoteInSentence && immediatelyAfterEndOfParagraph && !previousEndQuoteInserted)
		{
			int audienceObjectPosition = -1;
			bool definitelySpeaker = false;
			lastSpeakerPosition = determineSpeaker(lastOpeningPrimaryQuote, where, true, noSpeakerAfterward, definitelySpeaker, audienceObjectPosition);
			// the boots of Albert
			if (lastSpeakerPosition >= 0 && m[lastSpeakerPosition].getObject() >= 0 && objects[m[lastSpeakerPosition].getObject()].objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
				objects[m[lastSpeakerPosition].getObject()].getOwnerWhere() >= 0 && m[objects[m[lastSpeakerPosition].getObject()].getOwnerWhere()].getObject() >= 0 &&
				(objects[m[objects[m[lastSpeakerPosition].getObject()].getOwnerWhere()].getObject()].male || objects[m[objects[m[lastSpeakerPosition].getObject()].getOwnerWhere()].getObject()].female))
				lastSpeakerPosition = objects[m[lastSpeakerPosition].getObject()].getOwnerWhere();
			if (lastSpeakerPosition >= 0)
			{
				int definiteSpeakerUnnecessary = -1;
				imposeSpeaker(-1, where, definiteSpeakerUnnecessary, lastSpeakerPosition, definitelySpeaker, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, false, audienceObjectPosition, lastSubjects, -1);
				if (m[lastSpeakerPosition].principalWherePosition >= 0)
					lastSpeakerPosition = m[lastSpeakerPosition].principalWherePosition;
				int speakerObject = m[lastSpeakerPosition].getObject();
				if (m[lastSpeakerPosition].objectMatches.size() == 1)
					speakerObject = m[lastSpeakerPosition].objectMatches[0].object;
				if (speakerObject >= 0)
				{
					if (audienceObjectPosition < 0 || m[audienceObjectPosition].queryForm(reflexivePronounForm) < 0)
						tempSpeakerGroup.conversationalQuotes++;
					objects[speakerObject].PISDefinite++;
				}
				if (m[lastSpeakerPosition].objectMatches.empty())
					mergeObjectIntoSpeakerGroup(lastSpeakerPosition, speakerObject);
				else
				{
					for (unsigned int s = 0; s < m[lastSpeakerPosition].objectMatches.size(); s++)
						mergeObjectIntoSpeakerGroup(lastSpeakerPosition, m[lastSpeakerPosition].objectMatches[s].object);
				}
			}
			else if ((m[where].flags&cWordMatch::flagInsertedQuote) == 0) // definitely not a quoted string if it is completed only by an inserted quote.
			{
				// but it is an inserted quote from the speaker before the definite speaker.
				// “[tuppence:julius] Well , luckily for me[tuppence] , I[tuppence] pitched down into a good soft bed of earth -- but it[bed,earth] put me[tuppence] out of action for the time[time] , sure enough .
				// no speaker seen.  Insert last subject into speaker group (to make sure it is included as a possibility in the next stage)
				quotedStringSeen |= (!noTextBeforeOrAfter);
				if (firstQuotedSentenceOfSpeakerGroupNotSeen)
					for (unsigned int ls = 0; ls < subjectsInPreviousUnquotedSection.size(); ls++)
						mergeObjectIntoSpeakerGroup(lastSpeakerPosition, subjectsInPreviousUnquotedSection[ls]);
				firstQuotedSentenceOfSpeakerGroupNotSeen = false;
				if (audienceObjectPosition >= 0 && m[audienceObjectPosition].audienceObjectMatches.size() <= 1)
				{
					definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(audienceObjectPosition);
					int audienceObject = (m[audienceObjectPosition].audienceObjectMatches.size()) ? m[audienceObjectPosition].audienceObjectMatches[0].object : m[audienceObjectPosition].getObject();
					objects[audienceObject].numDefinitelyIdentifiedAsSpeaker++;
					objects[audienceObject].numDefinitelyIdentifiedAsSpeakerInSection++;
					if (objects[audienceObject].numDefinitelyIdentifiedAsSpeakerInSection + objects[audienceObject].numIdentifiedAsSpeakerInSection == 1 && section < sections.size())
						sections[section].speakerObjects.push_back(cOM(audienceObject, SALIENCE_THRESHOLD));
				}
			}
			// is there a close quote immediately before the opening quote?  If so, increase conversationalQuotes.
			if (previousPrimaryQuote >= 0 && lastOpeningPrimaryQuote - m[previousPrimaryQuote].endQuote <= 2)
				tempSpeakerGroup.conversationalQuotes++;
		}
		else if (!multipleQuoteInSentence && immediatelyAfterEndOfParagraph)
			lastSpeakerPosition = -1; // if inserted quote, lastSpeakerPosition should be unknown (see quote below) - this quote is after a definite speaker
	}
	if (lastQuotedString > lastSentenceEndBeforeAndNotIncludingCurrentQuote)
		quotedStringSeen = true;
	if (quotedStringSeen)
	{
		m[lastOpeningPrimaryQuote].flags |= cWordMatch::flagQuotedString;
		lastQuotedString = lastOpeningPrimaryQuote;
	}
	else
	{
		if ((multipleQuoteInSentence || !immediatelyAfterEndOfParagraph || previousEndQuoteInserted) && previousPrimaryQuote >= 0)
		{
			if (previousPrimaryQuote != lastOpeningPrimaryQuote)
			{
				m[previousPrimaryQuote].setQuoteForwardLink(lastOpeningPrimaryQuote); // resolve unknown speakers
				m[lastOpeningPrimaryQuote].quoteBackLink = previousPrimaryQuote; // resolve unknown speakers
			}
			else
				lplog(LOG_ERROR, L"%06d:previousPrimaryQuote and lastOpeningPrimaryQuote are the same (1)!", previousPrimaryQuote);
			if (m[previousPrimaryQuote].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers)
				m[lastOpeningPrimaryQuote].flags |= cWordMatch::flagEmbeddedStoryResolveSpeakers;
			if (debugTrace.traceSpeakerResolution)
			{
				wstring reason,tmpstr2;
				if (multipleQuoteInSentence) reason = L"multipleQuoteInSentence previousPrimaryQuote=" + itos(previousPrimaryQuote, tmpstr) + L" > lastSentenceEndBeforeAndNotIncludingCurrentQuote=" + itos(lastSentenceEndBeforeAndNotIncludingCurrentQuote, tmpstr2);
				if (!immediatelyAfterEndOfParagraph) reason += L"!immediatelyAfterEndOfParagraph ";
				if (previousEndQuoteInserted) reason += L"inserted quote ";
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG, L"%d:quote at %d linked forward to %d (%s)", lastOpeningPrimaryQuote, previousPrimaryQuote, lastOpeningPrimaryQuote, reason.c_str());
			}
		}
		if (!multipleQuoteInSentence && immediatelyAfterEndOfParagraph && !previousEndQuoteInserted) // include inserted quotes
		{
			if (firstQuote == -1)
				firstQuote = lastOpeningPrimaryQuote;
			if (lastQuote != -1)
			{
				m[lastQuote].nextQuote = lastOpeningPrimaryQuote;
				m[lastOpeningPrimaryQuote].previousQuote = lastQuote;
			}
			lastQuote = lastOpeningPrimaryQuote;
		}
		// remove any objects referring to a quote which is not a quoted string
		for (; quotedObjectCounter < (int)objects.size(); quotedObjectCounter++)
			if (objects[quotedObjectCounter].begin == lastOpeningPrimaryQuote)
			{
				if (debugTrace.traceObjectResolution)
					lplog(LOG_SG, L"%06d:%02d     object %s eliminated because it includes a primary quote",
						lastOpeningPrimaryQuote, section, objectString(quotedObjectCounter, tmpstr, true).c_str());
				objects[quotedObjectCounter].eliminated = true;
				for (vector <cSection>::iterator is = sections.begin(), isEnd = sections.end(); is != isEnd; is++)
					is->preIdentifiedSpeakerObjects.erase(quotedObjectCounter);
				for (vector <cSpeakerGroup>::iterator is = speakerGroups.begin(), isEnd = speakerGroups.end(); is != isEnd; is++)
					is->speakers.erase(quotedObjectCounter);
			}
			else if (objects[quotedObjectCounter].begin > lastOpeningPrimaryQuote)
				break;
		immediatelyAfterEndOfParagraph = false;
		quotesSeenSinceLastSentence = true;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG, L"%d:(2) quotesSeenSinceLastSentence set to true", where);
		previousPrimaryQuote = lastOpeningPrimaryQuote;
	}

}

void cSource::evaluateMetaWhereQuery(int where, bool inPrimaryQuote, int &currentMetaWhereQuery)
{
	LFS
	vector <cWordMatch>::iterator im=m.begin()+where;
	// Tuppence expressed a preference for the latter[go down to the restaurant]
	if (m[where].hasSyntacticRelationGroup && !inPrimaryQuote && m[where].getRelVerb()>=0 && lastOpeningPrimaryQuote>=0 &&
		  isVerbClass(m[where].getRelVerb(),L"reflexive_appearance") && isNounClass(where,L"liking"))
	{
		cSyntacticRelationGroup sr(where,-1,-1,currentMetaWhereQuery,m[lastOpeningPrimaryQuote].previousQuote,-1,-1,-1,-1,stMETAWQ,false,false,-1,-1,true);
		vector <cSyntacticRelationGroup>::iterator location = lower_bound(syntacticRelationGroups.begin(), syntacticRelationGroups.end(), sr, comparesr);
		if (location->where==where && location->relationType!=stMETAWQ)
		{
			syntacticRelationGroups.insert(location,sr);
			wstring sRole;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:PLACE meta query found at %06d (%s)",where,currentMetaWhereQuery,m[where].roleString(sRole).c_str());
		}
	}
	if (inPrimaryQuote)
	{
		// is currentMetaWhereQuery before the previous speakers' quote?
		if (currentMetaWhereQuery>=0 && currentMetaWhereQuery<m[lastOpeningPrimaryQuote].previousQuote)
			currentMetaWhereQuery=-1;
		if (currentMetaWhereQuery>=0 && currentMetaWhereQuery<=lastOpeningPrimaryQuote)
		{
			if (m[currentMetaWhereQuery].word->first==L"when")
			{
				lplog(LOG_RESOLUTION,L"%06d:TIME meta query answer found@%d?",where,currentMetaWhereQuery);
				// possibilities:
				//  1. TIME/DATE
				bool rtSet=false;
				cTimeInfo timeInfo,rt;
				int maxLen=-1;
				vector <cSyntacticRelationGroup>::iterator location = findSyntacticRelationGroup(where);
				if (location!=syntacticRelationGroups.end() && location!=syntacticRelationGroups.begin() && location->where>where)
					location--;
				if (!(m[where].flags&cWordMatch::flagAlreadyTimeAnalyzed) && evaluateTimePattern(where,maxLen,timeInfo,rt,rtSet))
				{
					cSyntacticRelationGroup sr(where,-1,-1,-1,-1,-1,-1,-1,-1,stABSTIME,false,false,-1,-1,true);
					rt.tWhere=timeInfo.tWhere=timeInfo.timeRTAnchor=rt.timeRTAnchor=where;
					sr.timeInfo.push_back(timeInfo);
					if (rtSet)
						sr.timeInfo.push_back(rt);
					markTime(where,where,maxLen);
					location = lower_bound(syntacticRelationGroups.begin(), syntacticRelationGroups.end(), sr, comparesr);
					syntacticRelationGroups.insert(location,sr);
					if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:TIME meta query answer found [direct time expression]@%d.",where,currentMetaWhereQuery);
				}
				//  2. sr with associated time expression
				else if (location!=syntacticRelationGroups.end() && location->timeInfo.size()>0 && 
					 ((location->where==where || (location->whereSubject>=0 && m[location->whereSubject].beginObjectPosition==where)) ||
					  location->timeInfo[0].tWhere==where))
				{
					if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:TIME meta query answer found [linked time expression]@%d.",where,currentMetaWhereQuery);
				}
				//  3. another 'when' expression
				else if (m[where].word->first==L"when")
				{
					if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:TIME meta query answer found [linked when]@%d.",where,currentMetaWhereQuery);
				}
				else 
				{
					postProcessLinkedTimeExpressions[where]=currentMetaWhereQuery;
				}
				currentMetaWhereQuery=-1;
			}
			// is this a position?
			// is this a statement of position?
			int o=m[where].getObject();
			__int64 or=m[where].objectRole;
			if (((o>=0 && objects[o].isPossibleSubType(false) && !(or&(OBJECT_ROLE|SUBJECT_ROLE))) &&
					 (!(or&PREP_OBJECT_ROLE) || (or&MOVEMENT_PREP_OBJECT_ROLE))) ||
					(syntacticRelationGroups.size() && syntacticRelationGroups[syntacticRelationGroups.size()-1].where==where) ||
					(syntacticRelationGroups.size() && syntacticRelationGroups[syntacticRelationGroups.size()-1].whereSubject==where))
			{
				cSyntacticRelationGroup sr(where,-1,-1,currentMetaWhereQuery,m[lastOpeningPrimaryQuote].previousQuote,-1,-1,-1,-1,stMETAWQ,false,false,-1,-1,true);
				vector <cSyntacticRelationGroup>::iterator location = lower_bound(syntacticRelationGroups.begin(), syntacticRelationGroups.end(), sr, comparesr);
				if (location->where!=where || location->relationType!=stMETAWQ)
				{
					syntacticRelationGroups.insert(location,sr);
					wstring sRole;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:PLACE meta query found at %06d (%s)",where,currentMetaWhereQuery,m[where].roleString(sRole).c_str());
				}
			}
		}
		if (im->flags&cWordMatch::flagInQuestion)
		{
			if ((im->word->first==L"where" || im->word->first==L"when") && (where<1 || m[where-1].getObject()<0))
				currentMetaWhereQuery=where;
			// what about some lunch?
			if ((im->objectRole&PREP_OBJECT_ROLE) && im->getObject()>=0 && objects[im->getObject()].getSubType()==BY_ACTIVITY)
				currentMetaWhereQuery=where;
			// A pensionnat?
			if (currentMetaWhereQuery<0 && im->pma.queryPattern(L"_META_SPEAKER_QUERY_RESPONSE")!=-1 && 
					im->principalWherePosition>=0 && m[im->principalWherePosition].getObject()>=0 && objects[m[im->principalWherePosition].getObject()].getSubType()>=0)
				currentMetaWhereQuery=where;
			// what depot / which depot?
			if ((im->word->first==L"what" || im->word->first==L"which") && 
					m[where+1].principalWherePosition>=0 && m[m[where+1].principalWherePosition].getObject()>=0 && objects[m[m[where+1].principalWherePosition].getObject()].getSubType()>=0)
				currentMetaWhereQuery=where;
		}
	}
}

void cSource::srd(int where,wstring spd,wstring &description)
{ LFS
	if (where>=0 && (m[where].objectMatches.size() || m[where].getObject()!=-1))
	{
		description+=spd;
		for (unsigned int J=0; J<m[where].objectMatches.size(); J++)
		{
			int mObject=m[where].objectMatches[J].object;
			int objectWhere=objects[mObject].originalLocation;
			// don't just print the honorific
			if ((m[objectWhere].queryWinnerForm(honorificForm)>=0 || m[objectWhere].queryWinnerForm(honorificAbbreviationForm)>=0) && objectWhere+1<m[objectWhere].endObjectPosition) 
			{
				objectWhere++;
				if (m[objectWhere].word->first==L".") objectWhere++;
			}
			if (objects[mObject].isPossibleSubType(false) && (objects[mObject].objectClass==NAME_OBJECT_CLASS || objects[mObject].objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
				objectWhere=m[objectWhere].endObjectPosition-1;
			if (objectWhere>=0)
			description+=m[objectWhere].word->first;
			if (J<m[where].objectMatches.size()-1) description+=L" ";
		}
		if (m[where].objectMatches.empty() && m[where].getObject()!=-1)
		{
			int mObject=m[where].getObject();
			switch (mObject)
			{
			case cObject::eOBJECTS::UNKNOWN_OBJECT:  description+=L"UNK"; break;
			case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE:  description+=L"UNK_M"; break;
			case cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE: description+=L"UNK_F"; break;
			case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE_OR_FEMALE: description+=L"UNK_M_OR_F"; break;
			case cObject::eOBJECTS::OBJECT_UNKNOWN_NEUTER: description+=L"UNK_N"; break;
			case cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL: description+=L"UNK_P"; break;
			case cObject::eOBJECTS::OBJECT_UNKNOWN_ALL: description+=L"ALL"; break;
			default:
				int objectWhere=objects[mObject].originalLocation;
				if (objectWhere<0)
					return;
				// don't just print the honorific
				if ((m[objectWhere].queryWinnerForm(honorificForm)>=0 || m[objectWhere].queryWinnerForm(honorificAbbreviationForm)>=0) && objectWhere+1<m[objectWhere].endObjectPosition) 
				{
					objectWhere++;
					if (m[objectWhere].word->first==L".") objectWhere++;
				}
				if (objects[mObject].isPossibleSubType(false) && m[objectWhere].endObjectPosition>=0 && (objects[mObject].objectClass==NAME_OBJECT_CLASS || objects[mObject].objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
					objectWhere=m[objectWhere].endObjectPosition-1;
				description+=m[objectWhere].word->first;
			}
		}
		description+=L"]";
	}
}

wstring cSource::wsrToText(int where,wstring &description)
{ LFS
	cSyntacticRelationGroup sr(where,-1,-1,-1,-1,-1,-1,-1,-1,stEXIT,false,false,-1,-1,false);
	vector <cSyntacticRelationGroup>::iterator location = lower_bound(syntacticRelationGroups.begin(), syntacticRelationGroups.end(), sr, comparesr);
	if (location==syntacticRelationGroups.end()) return L"";
	int spr=(int) (location-syntacticRelationGroups.begin());
	return srToText(spr,description);
}

wstring cSource::srToText(int &spr,wstring &description)
{ LFS
	wstring names,tmpstr;
	vector <cSyntacticRelationGroup>::iterator spri=syntacticRelationGroups.begin()+spr,keep=spri;
	description=relationString(spri->relationType)+L":";
	if (!spri->physicalRelation)
		description+=L"npr:";
	if (spri->relationType==stMETAWQ)
	{
		description+=L"Answer to query @"+itos(spri->whereSubject,names)+L"[previous quote "+itos(spri->whereVerb,tmpstr)+L"]";
		spr++;
		return description;
	}
	if (spri->genderedEntityMove)
		description+=L"[GMOVE]";
	srd(spri->whereControllingEntity,L"C[",description);
  // take the packet - a command - but NOT takes the packet / calls herself Rita
	bool present=spri->whereVerb>=0 && (m[spri->whereVerb].verbSense&(VT_TENSE_MASK|VT_EXTENDED))==VT_PRESENT && 
		spri->whereSubject<0 && !(m[spri->whereVerb].flags&cWordMatch::flagInInfinitivePhrase) &&
		(m[spri->whereVerb].word->second.inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR))!=0;
	if (spri->whereControllingEntity<0 && spri->tft.lastOpeningPrimaryQuote>=0 && m[spri->tft.lastOpeningPrimaryQuote].objectMatches.size()>=1)
		srd(spri->tft.lastOpeningPrimaryQuote,(present) ? L"C[" : L"SP[",description);
	srd(spri->whereSubject,L"S[",description);
	if (spri->whereControllingEntity<0 && spri->tft.lastOpeningPrimaryQuote>=0 && (present || (spri->whereSubject<0 && (m[spri->where].flags&cWordMatch::flagInQuestion)!=0)) &&
		   m[spri->tft.lastOpeningPrimaryQuote].audienceObjectMatches.size()>=1)
	{
		description+=L"S[";
		for (unsigned int J=0; J<m[spri->tft.lastOpeningPrimaryQuote].audienceObjectMatches.size(); J++)
		{
			int mObject=m[spri->tft.lastOpeningPrimaryQuote].audienceObjectMatches[J].object;
			int objectWhere=objects[mObject].originalLocation;
			// don't just print the honorific
			if ((m[objectWhere].queryWinnerForm(honorificForm)>=0 || m[objectWhere].queryWinnerForm(honorificAbbreviationForm)>=0) && objectWhere+1<m[objectWhere].endObjectPosition) 
			{
				objectWhere++;
				if (m[objectWhere].word->first==L".") objectWhere++;
			}
			if (m[objectWhere].endObjectPosition && objects[mObject].isPossibleSubType(false) && (objects[mObject].objectClass==NAME_OBJECT_CLASS || objects[mObject].objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
				objectWhere=m[objectWhere].endObjectPosition-1;
			description+=m[objectWhere].word->first;
			if (J<m[spri->tft.lastOpeningPrimaryQuote].audienceObjectMatches.size()-1) description+=L" ";
		}
		description+=L"]";
	}
	if (spri->whereVerb>=0 && spri->relationType!=-stLOCATION)
		description+=L"V["+m[spri->whereVerb].word->first+L"]";
	srd(spri->whereObject,L"O[",description);
	if (spri->relationType==stMOVE_OBJECT && spri->whereVerb>=0 && m[spri->whereVerb].getRelObject()>=0 && m[m[spri->whereVerb].getRelObject()].relNextObject>=0 &&
		  spri->whereObject>=0 &&
		  ((m[spri->whereObject].objectMatches.size() && m[spri->whereObject].objectMatches[0].object>=0 && objects[m[spri->whereObject].objectMatches[0].object].isAgent(true)) || 
		  m[spri->whereObject].getObject()>=0 && objects[m[spri->whereObject].getObject()].isAgent(true)))
		srd(spri->whereObject,L"O[",description);
	if (spri->wherePrep>=0)
	{
		description+=L"P["+m[spri->wherePrep].word->first;
		if (spri->wherePrepObject<0 && m[spri->wherePrep].getRelObject()>=0)
			description+=L" "+m[m[spri->wherePrep].getRelObject()].word->first;
		description+=L"]";
	}
	srd(spri->wherePrepObject,L"PO[",description);
	srd(spri->whereMovingRelativeTo,L"M[",description);
	for (int ti=0; ti<(signed)spri->timeInfo.size(); ti++)
		description+=L" "+spri->timeInfo[ti].toString(m,tmpstr);
	for (; spr<(signed)syntacticRelationGroups.size() && spri->where==keep->where; spr++,spri++);
	/*
	wstring tmpstr2;
	bool allShare
	for (; spr<syntacticRelationGroups.size() && spri->where==keep->where; spr++,spri++)
		names+=whereString(objects[spri->o].originalLocation,tmpstr,true)+L" ";
	if (names.length()) names.erase(names.begin()+names.length()-1);
	whereString(spri->whereSubject,tmpstr,true);
	if (names.size() && tmpstr!=names)
		description+=L" PERTAINS TO:"+names;
	*/
	int di=0;
	while ((di=description.find(L'\'',di))!=wstring::npos)
		description.erase(description.begin()+di);
	return description;
}

void cSource::cancelSubType(int object)
{ LFS
  wstring tmpstr,tmpstr2;
	vector <cObject>::iterator o=objects.begin()+object;
	for (vector <cObject::cLocation>::iterator li=o->locations.begin(),liEnd=o->locations.end(); li!=liEnd && o->getSubType()>=0; li++)
	{
		vector <cWordMatch>::iterator im=m.begin()+li->at;
		if (im->objectRole&NON_MOVEMENT_PREP_OBJECT_ROLE)
		{
			if (o->originalLocation==li->at && o->getSubType()==UNKNOWN_PLACE_SUBTYPE)
				o->resetSubType();
			if (o->objectClass==NAME_OBJECT_CLASS && o->name.hon!=wNULL)
				o->resetSubType();
			if (o->objectClass!=NAME_OBJECT_CLASS && o->objectClass!=NON_GENDERED_NAME_OBJECT_CLASS && debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:question place %s is [%s]?", li->at, objectString(object, tmpstr, false).c_str(), (o->getSubType() <= UNKNOWN_PLACE_SUBTYPE) ? OCSubTypeStrings[o->getSubType()] : itos(o->getSubType(), tmpstr2).c_str());
		}
		if (o->getSubType()==UNKNOWN_PLACE_SUBTYPE && !(im->objectRole&MOVEMENT_PREP_OBJECT_ROLE) &&
				o->usedAsLocation<5 &&
			  ((im->endObjectPosition-im->beginObjectPosition==2 && m[im->beginObjectPosition].queryWinnerForm(determinerForm)>=0) ||
				  im->endObjectPosition-im->beginObjectPosition==1) && (im->flags&cSourceWordInfo::notPhysicalObjectByWN))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%d:%s is NOT PLACE?",*li,o->usedAsLocation,objectString(object,tmpstr,false).c_str());
			o->resetSubType();
		}
	}
}

unordered_map <wstring, set <int> >::iterator cSource::getVerbClasses(int whereVerb,wstring &verb)
{ LFS
unordered_map <wstring, set <int> >::iterator lvtoCi=vbNetVerbToClassMap.find(getBaseVerb(whereVerb,13,verb));
	// get_out is very different from get by itself
	if (whereVerb+1<(signed)m.size() && (m[whereVerb+1].queryWinnerForm(adverbForm)>=0 || m[whereVerb+1].queryWinnerForm(prepositionForm)>=0 || m[whereVerb+1].queryWinnerForm(nounForm)>=0))
	{
		wstring verbParticiple=verb+L"_"+m[whereVerb+1].word->first;
		unordered_map <wstring, set <int> >::iterator lvtoCiParticiple=vbNetVerbToClassMap.find(verbParticiple);
		if (lvtoCiParticiple!=vbNetVerbToClassMap.end())
		{
			if (debugTrace.traceWhere)
			lplog(LOG_WHERE,L"%06d:verb %d:%s skipped in favor of verb+participle %s.",whereVerb,
		    whereVerb,m[whereVerb].word->first.c_str(),verbParticiple.c_str());
			if (m[whereVerb].getRelObject()==whereVerb+1)
				m[whereVerb].setRelObject(-1);
			lvtoCi=lvtoCiParticiple;
		}
	}
	return lvtoCi;
}

wstring cSource::getBaseVerb(int where,int fromWhere,wstring &verb)
{ LFS
	if (m[where].queryWinnerForm(beForm)==-1 &&  
		  (!(m[where].word->second.inflectionFlags&(VERB_PAST|VERB_PAST_PARTICIPLE|VERB_PRESENT_PARTICIPLE|VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR|VERB_PRESENT_PLURAL|VERB_PAST_THIRD_SINGULAR|VERB_PAST_PLURAL|VERB_PRESENT_SECOND_SINGULAR)) ||
			 !m[where].hasWinnerVerbForm()))
		return verb=L"";
	tIWMM v=m[where].word,w=(v->second.mainEntry==wNULL) ? v : v->second.mainEntry;
	verb=w->first;
	int inflectionFlags=w->second.inflectionFlags;
	int whereVerb=where,whereSubject=m[whereVerb].relSubject;
	//  present, past, past participle, present participle, and the present 3rd singular 
	//  I lay, he laid, I have laid,         I am laying,                he lays
	//	I lie, he lay,  I have lain          I am lying,                 he lies
	if (m[whereVerb].word->first==L"lay" && whereSubject>=0)
		// main entry
		verb=((m[whereSubject].word->second.inflectionFlags&(FIRST_PERSON|SINGULAR))==(FIRST_PERSON|SINGULAR)) ? L"lay" : L"lie";
	else 
		deriveMainEntry(where,fromWhere,verb,inflectionFlags,true,false,lastNounNotFound,lastVerbNotFound);
	if (verb==L"ishas")
		verb=(m[where].queryWinnerForm(isForm)>=0) ? L"am" : L"have";
	if (verb==L"wouldhad")
		verb=(m[whereVerb].queryWinnerForm(modalAuxiliaryForm)>=0) ? L"would" : L"have";
	return verb;
}

bool cSource::isSpecialVerb(int where,bool moveOnly)
{ LFS
	wstring verb;
unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(where,verb);
	if (lvtoCi!=vbNetVerbToClassMap.end())
	{
		for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
		{
			if (moveOnly && vbNetClasses[*vbi].moveVerbClass()) return true;
			if (!moveOnly && vbNetClasses[*vbi].whereVerbClass()) return true;
		}
	}
	return false;
}

bool cSource::isPhysicalActionVerb(int where)
{ LFS
	wstring verb;
unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(where,verb);
	if (lvtoCi!=vbNetVerbToClassMap.end())
		for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
			if (!vbNetClasses[*vbi].noPhysicalAction) return true;
	return false;
}

bool cSource::isSelfMoveVerb(int where,bool &exitOnly)
{ LFS
	wstring verb;
	bool moveOrExit=false;
	unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(where,verb);
	int tmp;
	if (lvtoCi!=vbNetVerbToClassMap.end())
	{
		for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
		{
			tmp=*vbi;
			moveOrExit|=(vbNetClasses[*vbi].move || vbNetClasses[*vbi].exit || vbNetClasses[*vbi].enter); // return is ENTER, not EXIT, but you can return to the kitchen, which is an EXIT
			exitOnly|=vbNetClasses[*vbi].exit;
		}
	}
	return moveOrExit;
}

bool cSource::isControlVerb(int where)
{ LFS
	wstring verb;
unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(where,verb);
	if (lvtoCi!=vbNetVerbToClassMap.end())
		for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
			if (vbNetClasses[*vbi].control) return true;
	return false;
}

bool cSource::isVerbClass(int where,int verbClass)
{ LFS
	wstring verb;
unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(where,verb);
	if (lvtoCi!=vbNetVerbToClassMap.end())
	{
		for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
		{
			if ((vbNetClasses[*vbi].contact || vbNetClasses[*vbi]._near) && verbClass==stEXIT) return true;
			if (vbNetClasses[*vbi].enter && verbClass==stENTER) return true;
			if (vbNetClasses[*vbi].stay && verbClass==stSTAY) return true;
			//if (vbNetClasses[*vbi].establish && verbClass==stESTABLISH) return true;
			if (vbNetClasses[*vbi].move && verbClass==stMOVE) return true;
			if (vbNetClasses[*vbi].moveObject && verbClass==stMOVE_OBJECT) return true;
			if (vbNetClasses[*vbi].moveInPlace && verbClass==stMOVE_IN_PLACE) return true;
			//if (vbNetClasses[*vbi].metawq && verbClass==stMETAWQ) return true;
			if (vbNetClasses[*vbi].contact && verbClass==stCONTACT) return true;
			if (vbNetClasses[*vbi]._near && verbClass==stNEAR) return true;
			if (vbNetClasses[*vbi].transfer && verbClass==stTRANSFER) return true;
			//if (vbNetClasses[*vbi].location && verbClass==stLOCATION) return true;
			//if (vbNetClasses[*vbi].relTime && verbClass==stRELTIME) return true;
			//if (vbNetClasses[*vbi].absTime && verbClass==stABSTIME) return true;
		}
	}
	return false;
}

bool cSource::isVerbClass(int where,wstring verbClass)
{ LFS
	wstring verb;
unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(where,verb);
	if (lvtoCi!=vbNetVerbToClassMap.end())
		for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
			if (like(vbNetClasses[*vbi].name(),verbClass)) return true;
	return false;
}

bool cSource::isNounClass(int where,wstring group)
{ LFS
	tIWMM word=m[where].word;
	if (word->first.length()==1) return false;
	if (word->second.query(numeralCardinalForm)>=0 || word->second.query(NUMBER_FORM_NUM)>=0) return false;
	if (word->second.query(nomForm)>=0 || word->second.query(personalPronounAccusativeForm)>=0 || word->second.query(personalPronounForm)>=0) return false;
	if (word->second.query(possessiveDeterminerForm)>=0 || word->second.query(possessivePronounForm)>=0 || word->second.query(reflexivePronounForm)>=0) return false;
	if (word->second.query(nounForm)<0) return false;
	string groupStr;
	return inWordNetClass(where,m[where].word->first,m[where].word->second.inflectionFlags,wTM(group,groupStr),lastNounNotFound,lastVerbNotFound);
}

// does the object, or its matches, designate a location, or a verb clause leading to a location?
bool cSource::locationMatched(int where)
{ LFS
	if (m[where].getObject()<0) return false;
	if (objects[m[where].getObject()].getSubType()>=0) return true;
	for (int omi=0; omi<(signed)m[where].objectMatches.size(); omi++)
	{
		if (objects[m[where].objectMatches[omi].object].getSubType()>=0) return true;
		if (objects[m[where].objectMatches[omi].object].objectClass==VERB_OBJECT_CLASS) 
		{
			int whereVerb=objects[m[where].objectMatches[omi].object].originalLocation,wo;
			if (m[whereVerb].relPrep>=0 && (wo=m[m[whereVerb].relPrep].getRelObject())>=0 && (m[wo].objectRole&MOVEMENT_PREP_OBJECT_ROLE) &&
			  where!=wo && locationMatched(wo)) return true;
		}
	}
	return false;
}

// corral prepositional references to objects that wouldn't already be picked up by detectSyntacticRelationGroup (which keys off of verbs)
// will change source.m (invalidate all iterators through the use of newSR)
void cSource::detectSpaceLocation(int where,int lastBeginS1)
{ LFS
	int wp,wpo,prepType,whereVerb=m[where].getRelVerb();
	if (m[where].getObject()>=0 && m[wp=m[where].endObjectPosition].queryWinnerForm(prepositionForm)>=0 && (wpo=m[wp].getRelObject())>=0 &&
		 ((m[where].word->second.flags&cSourceWordInfo::physicalObjectByWN) || objects[m[where].getObject()].getSubType()>=0 || objects[m[where].getObject()].isAgent(false)))
	{
		//bool setPhysicalRelation=((whereVerb>=0 && m[whereVerb].hasSyntacticRelationGroup) || m[wp].hasSyntacticRelationGroup);
		// moment is also a 'force', which is considered a physical object.  But overrule it by including the T_UNIT restriction.
		if ((m[wp].word->second.flags&cSourceWordInfo::prepMoveType) && (prepType=prepTypesMap[m[wp].word->first])!= tprTO && prepType!=tprFROM && !(m[wpo].word->second.timeFlags&T_UNIT))
		{
			if ((m[wpo].word->second.flags&cSourceWordInfo::physicalObjectByWN) || (m[wpo].getObject()>=0 && objects[m[wpo].getObject()].isPossibleSubType(false)))
				newSR(where,-1,-1,-1,whereVerb,wp,wpo,-1,-1,-stLOCATION,L"spaceLocation",true);
			else if (m[wpo].endObjectPosition>=0 && m[m[wpo].endObjectPosition].word->first==L"of" && (wpo=m[m[wpo].endObjectPosition].getRelObject())>=0 && 
				       ((m[wpo].word->second.flags&cSourceWordInfo::physicalObjectByWN) || (m[wpo].getObject()>=0 && objects[m[wpo].getObject()].isPossibleSubType(false))))
				newSR(where,-1,-1,-1,whereVerb,wp,-1,wpo,-1,-stLOCATION,L"spaceLocation (OF2)",true);
		}
		if (objects[m[where].getObject()].isAgent(false) && ((prepType=prepTypesMap[m[wp].word->first])==tprFROM || prepType==tprOF) &&
				(m[wpo].getObject()>=0 && objects[m[wpo].getObject()].isPossibleSubType(false)) && m[where].word->first!=L"one")
			newSR(where,-1,-1,where,whereVerb,wp,-1,wpo,-1,-stLOCATION,L"spaceLocation OF/FROM",true);
	}
	// How about the Savoy?
	// the Ritz?
	if ((m[where].flags&cWordMatch::flagInQuestion)!=0 && m[where].getObject()>=0 && objects[m[where].getObject()].getSubType()>=0 && whereVerb<0 &&
		  !(m[where].flags&cWordMatch::flagAdjectivalObject) && m[m[where].beginObjectPosition].pma.queryPattern(L"__INTRO_S1")==-1 &&
			// if in prepositional phrase, must be a preposition of movement
			(!(m[where].objectRole&PREP_OBJECT_ROLE) || (m[where].objectRole&MOVEMENT_PREP_OBJECT_ROLE) || objects[m[where].getObject()].getSubType()!=UNKNOWN_PLACE_SUBTYPE) &&
			!m[where].hasSyntacticRelationGroup)
	{
		// if in lastBeginS1 or lastQ2 then cancel too
		if (lastBeginS1<0 || lastBeginS1+m[lastBeginS1].maxMatch<where)
			newSR(where,-1,-1,where,-1,-1,-1,-1,-1,-stLOCATION,L"questionLocation",true);
	}
	// I prefer the Piccadilly.
	if ((m[where].flags&cWordMatch::flagInQuestion)==0 && whereVerb>=0 && m[whereVerb].getRelObject()==where &&
		  m[where].getObject()>=0 && objects[m[where].getObject()].getSubType()>=0 && objects[m[where].getObject()].getSubType()!=BY_ACTIVITY &&
		  isVerbClass(whereVerb,L"want"))
		newSR(where,-1,-1,where,-1,-1,-1,-1,-1,-stLOCATION,L"locationPreference",true);
	// Tuppence expressed a preference for the latter[go down to the restaurant]
	if ((m[where].flags&cWordMatch::flagInQuestion)==0 && whereVerb>=0 && m[whereVerb].relPrep>=0 && m[whereVerb].getRelObject()==where && m[m[whereVerb].relPrep].getRelObject()>=0 &&
		   locationMatched(m[m[whereVerb].relPrep].getRelObject()) && isVerbClass(whereVerb,L"reflexive_appearance") && isNounClass(where,L"liking"))
		newSR(where,-1,-1,m[m[whereVerb].relPrep].getRelObject(),-1,-1,-1,-1,-1,-stLOCATION,L"locationExtendedPreference",true);
}

bool cSource::isSpeakerContinued(int where,int o,int lastWherePP,bool &sgOccurredAfter,bool &audienceOccurredAfter,bool &speakerOccurredAfter)
{ LFS
	bool physicallyEvaluated;
	vector <cLocalFocus>::iterator lsi=in(o);
	lastWherePP=(lsi!=localObjects.end() && lsi->lastWhere>=0) ? physicallyPresentPosition(lsi->lastWhere,physicallyEvaluated) && physicallyEvaluated : false;
	sgOccurredAfter=lsi!=localObjects.end() && lsi->lastWhere>where && lastWherePP;
	// speaking or being spoken to after exit
	audienceOccurredAfter=(lastOpeningPrimaryQuote>where && m[lastOpeningPrimaryQuote].audiencePosition>=0 && in(o,m[lastOpeningPrimaryQuote].audiencePosition));
	speakerOccurredAfter=(lastOpeningPrimaryQuote>where && m[lastOpeningPrimaryQuote].speakerPosition>=0 && in(o,m[lastOpeningPrimaryQuote].speakerPosition));
	if (speakerGroupsEstablished && lastOpeningPrimaryQuote>where)
	{
		audienceOccurredAfter|=in(o,m[lastOpeningPrimaryQuote].audienceObjectMatches)!=m[lastOpeningPrimaryQuote].audienceObjectMatches.end();
		speakerOccurredAfter|=in(o,m[lastOpeningPrimaryQuote].objectMatches)!=m[lastOpeningPrimaryQuote].objectMatches.end();
	}
	return sgOccurredAfter || audienceOccurredAfter || speakerOccurredAfter;
}

// in opposite directions
// separately
// their own way
bool cSource::isSpatialSeparation(int whereVerb)
{ LFS
	if (whereVerb<0 || m[whereVerb].relSubject<0 || m[m[whereVerb].relSubject].getObject()<0 || !objects[m[m[whereVerb].relSubject].getObject()].plural) return false;
	int prepLoop=0;
	for (int relPrep=m[whereVerb].relPrep; relPrep>=0; relPrep=m[relPrep].relPrep)
	{
		if (m[relPrep].word->first==L"in" && m[relPrep+1].word->first==L"opposite" && m[relPrep+2].word->first==L"directions")
			return true;
		if (prepLoop++>20)
		{
			wstring tmpstr;
			lplog(LOG_ERROR,L"%06d:Prep loop occurred (12) %s.",relPrep,loopString(relPrep,tmpstr));
			break;
		}
	}
	return false;
}

// will change source.m (invalidate all iterators through the use of newSR)
void cSource::detectSyntacticRelationGroup(int where,int backInitialPosition,vector <int> &lastSubjects)
{ LFS
	wstring tmpstr,tmpstr2,tmpstr3,tmpstr4;
	bool syntacticRelationGroupDetected=false;
	bool inPrimaryQuote=(m[where].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0;
	int whereSubject=-1,whereVerb=-1,syntacticRelationGroupsOriginalSize=syntacticRelationGroups.size();
	if (m[where].objectRole&SUBJECT_ROLE)
	{
		whereSubject=where;
		whereVerb=m[where].getRelVerb();
		if (whereVerb>=0 && (m[where].objectRole&OBJECT_ROLE) && m[whereVerb].relSubject!=where)
			return;
		if (m[whereSubject].queryWinnerForm(prepositionForm)>=0) // prevent prepositions within subject from being considered the subject itself
			return;
	}
	else if ((m[where].objectRole&OBJECT_ROLE) && m[where].verbSense==0) // getQuoteForwardLink() is tsSense for a verb
	{
		return; // should have already been taken care of with a subject or verb
	}
	else if (m[where].relSubject>=0 && m[m[where].relSubject].getRelVerb()!=where && m[where].verbSense>=0)
	{
		whereSubject=m[where].relSubject;
		whereVerb=where;
	}
	else if (m[where].relSubject<0 && m[where].getRelObject()>=0 && m[m[where].getRelObject()].getRelVerb()==where)
	{
		whereVerb=where;
	}
	else if (m[where].relSubject<0 && m[where].relPrep>2 && !inPrimaryQuote)
	{
		int wherePrep=m[where].relPrep;
		bool multiWordMovement=(m[wherePrep].word->first==L"of" && 
			  (m[wherePrep-1].word->first==L"out" || m[wherePrep-1].word->first==L"inside" || m[wherePrep-1].word->first==L"outside" || 
				 m[wherePrep-1].word->first==L"ahead" || m[wherePrep-1].word->first==L"abreast"));  // he comes out of the building
		// en route for Chester
		multiWordMovement|=m[wherePrep].word->first==L"for" && m[wherePrep-1].word->first==L"route" && m[wherePrep-2].word->first==L"en";
		// in the presence of XX
		multiWordMovement|=m[wherePrep].word->first==L"of" && wherePrep>4 &&
			  m[wherePrep-3].word->first==L"in" && m[wherePrep-2].word->first==L"the" && m[wherePrep-1].word->first==L"presence";
		if (!rejectPrepPhrase(wherePrep) && (multiWordMovement || (m[wherePrep].word->second.flags&cSourceWordInfo::prepMoveType)))
			whereVerb=where;
	}
	else if (m[where].relSubject<0 && inPrimaryQuote)
	{
		if (isSpecialVerb(where,false) || m[where].getRelObject()>=0 || m[where].relPrep>=0)
			whereVerb=where;
		// promoted to drying plates
		if (m[where].getRelObject()<0 && where+2<(signed)m.size() && m[where].pma.queryPattern(L"_VERBREL1")!=-1 && m[where+1].pma.queryPatternDiff(L"_PP",L"3")!=-1)
			whereVerb=where;
	}
	if (whereVerb>=0 && !m[whereVerb].hasWinnerVerbForm()) // includes verbverbForm
		return;
	if (whereSubject>=0 && m[whereSubject].principalWherePosition>whereSubject) 
		return;
	if (whereSubject < 0 && m[where].getRelVerb() >= 0 && m[m[where].getRelVerb()].relSubject >= 0)
		whereSubject = m[m[where].getRelVerb()].relSubject;
	int maxEnd=-1;
	// evil-looking house
	if (whereVerb>=0 && (m[whereVerb].verbSense&VT_VERB_CLAUSE) && queryPattern(whereVerb,L"__ADJECTIVE",maxEnd)>=0)
		return;
	if (whereSubject>=0 && whereVerb>=0 && m[whereVerb].getRelObject()>=0 && m[whereSubject].word->first==L"where" && 
		  (m[whereVerb].word->first==L"is" || m[whereVerb].word->first==L"was" || m[whereVerb].word->first==L"be") &&
			m[m[whereVerb].getRelObject()].getObject()>=0 && objects[m[m[whereVerb].getRelObject()].getObject()].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS &&
			objects[m[m[whereVerb].getRelObject()].getObject()].getSubType()<0)
		lplog(LOG_RESOLUTION,L"%06d:location activity noun %s",where,whereString(m[whereVerb].getRelObject(),tmpstr,false).c_str());
	bool physicallyEvaluated = false, allIn, oneIn;
	if (whereSubject>=0 && m[whereSubject].principalWherePosition>=0) 
		whereSubject=m[whereSubject].principalWherePosition;
	if (whereSubject >= 0 && m[whereSubject].beginObjectPosition>=0)
		physicallyPresentPosition(whereSubject,m[whereSubject].beginObjectPosition,physicallyEvaluated,true);
	int o=(whereSubject>=0) ? m[whereSubject].getObject() : -1,ws=whereSubject;
	bool acceptableThere=whereSubject>=0 && m[whereSubject].word->first==L"there" && m[whereSubject].getRelObject()>=0 && m[m[whereSubject].getRelObject()].getObject()>=0 && 
		   (m[m[whereSubject].getRelObject()].word->second.flags&cSourceWordInfo::physicalObjectByWN);
	if (o>=0 && m[whereSubject].objectMatches.size())
	{
		o=m[whereSubject].objectMatches[0].object;
		ws=objects[o].originalLocation;
	}
	int st=(o>=0) ? objects[o].getSubType() : -1,tmp1=-1,tmp2=-1,tmp3=-1,tmp4=-1;
	if (o>=0 && (tmp1=whereVerb<0 || primaryLocationLastPosition<0 || (tmp4=m[whereVerb].verbSense&VT_TENSE_MASK)==VT_PAST) &&
			 !(m[ws].flags&cWordMatch::flagAdjectivalObject) && 
			 // the ship
			 (m[m[ws].beginObjectPosition].word->first==L"the" || m[m[ws].beginObjectPosition].queryWinnerForm(adjectiveForm)>=0 || 
			 //  just opposite to that window , there was a tree growing.
			 acceptableThere ||
			 // Tommy's taxi
			 (tmp2=(m[m[ws].beginObjectPosition].word->second.inflectionFlags&(SINGULAR_OWNER|PLURAL_OWNER))) ||
			 // an outlying picture house - if a/an, it must be a qualified noun
			 ((m[m[ws].beginObjectPosition].word->first==L"a" || m[m[ws].beginObjectPosition].word->first==L"an") &&
			   (m[ws].endObjectPosition-m[ws].beginObjectPosition>2 || whereVerb>=0)) ||
			 // Carshalton Terrace proved to be an unimpeachable row
			 objects[o].objectClass==NON_GENDERED_NAME_OBJECT_CLASS || 
			 // Tuppence's hostel
			  (objects[o].getOwnerWhere()>=0 && 
				 ((currentSpeakerGroup<speakerGroups.size() && intersect(objects[o].getOwnerWhere(),speakerGroups[currentSpeakerGroup].speakers,allIn,oneIn)) ||
				   (m[objects[o].getOwnerWhere()].getObject()>=0 && objects[m[objects[o].getOwnerWhere()].getObject()].numIdentifiedAsSpeaker>0)))) &&
				// The MayFair streets
			 (tmp3=!objects[o].plural || (m[ws].endObjectPosition-m[ws].beginObjectPosition)>2 || acceptableThere) &&
		  (st==MOVING || st==TRAVEL || st==GEOGRAPHICAL_URBAN_FEATURE || st==GEOGRAPHICAL_URBAN_SUBFEATURE || st==BY_ACTIVITY || st==COUNTRY || st==ISLAND || 
			 st==MOUNTAIN_RANGE_PEAK_LANDFORM || st==US_STATE_TERRITORY_REGION || st==OCEAN_SEA || st==WORLD_CITY_TOWN_VILLAGE || 
			 st==CANADIAN_PROVINCE_CITY || st==REGION || st==RIVER_LAKE_WATERWAY || st==US_CITY_TOWN_VILLAGE || st==PARK_MONUMENT || acceptableThere))
	{
		// the train stopped.
		bool continues=((primaryLocationLastPosition>=0 && m[primaryLocationLastPosition].word==m[where].word)) || st==TRAVEL;
		//genderedLocationRelation=primaryLocationLastPosition<0;
		if (st!=TRAVEL)
			primaryLocationLastPosition=where;
		// right now limit it to only things that move
		bool continuesMoving=(primaryLocationLastMovingPosition>=0 && m[primaryLocationLastMovingPosition].word==m[where].word),cancel=false;
		bool lastWherePP=false,sgOccurredAfter=false,audienceOccurredAfter=false,speakerOccurredAfter=false,noMove=false,speakerContinuation=false;
		syntacticRelationGroupDetected=!continuesMoving && st==MOVING;
		// the hostel was put in Bagravia - allow passives
		noMove=st!=MOVING && whereVerb>=0 && isSpecialVerb(whereVerb,true) && !(m[whereVerb].verbSense&VT_PASSIVE);
		cancel=noMove;
		set <int> speakers,povSpeakers;
		getCurrentSpeakers(speakers,povSpeakers);
		wstring ss,sRole;
		for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd && !cancel && !speakerContinuation; si++)
			speakerContinuation=isSpeakerContinued(where,*si,lastWherePP,sgOccurredAfter,audienceOccurredAfter,speakerOccurredAfter);
		int tense=(m[where].getRelVerb()>=0) ? m[m[where].getRelVerb()].verbSense&(VT_TENSE_MASK|VT_POSSIBLE|VT_NEGATION) : 0;
		bool wrongTense=(tense&VT_PASSIVE) || (m[where].objectRole&THINK_ENCLOSING_ROLE);
		cancel|=wrongTense;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:(%s%s%s%s%s%s) location %s %s (old location %s@%d) object %s tense %s.",where,
			  (speakerGroupsEstablished) ? L"SRSRL":L"ISGSRL", // speaker resolution space relations location
				(sgOccurredAfter) ? L" sgOccurredAfter":L"",
				(audienceOccurredAfter) ? L" audienceOccurredAfter":L"",
				(speakerOccurredAfter) ? L" speakerOccurredAfter":L"",
				(wrongTense) ? L" wrongTense":L"",
				(noMove) ? L" noMove":L"",
				whereString(where,tmpstr,false).c_str(),
				(continues) ? L"continues":L"establishes",
				(primaryLocationLastPosition<0) ? L"NULL":whereString(primaryLocationLastPosition,tmpstr2,false).c_str(),primaryLocationLastPosition,
				whereString(m[where].getRelObject(),tmpstr3,false).c_str(),(m[where].getRelVerb()>=0) ? senseString(tmpstr4,m[m[where].getRelVerb()].verbSense).c_str() : L"(no verb)");
		bool location=false,timeUnit=false;
		int wherePrepObject=-1,wherePrep=-1;
		if (whereVerb>=0 && m[whereVerb].getRelVerb()<0 && m[whereVerb].relPrep>=0) 
		{
			wherePrepObject=findAnyLocationPrepObject(whereVerb,wherePrep,location,timeUnit);
			if (!location || timeUnit)
				wherePrepObject=-1;
		}
		int originalSize=syntacticRelationGroups.size();
		for (set<int>::iterator pvi=speakers.begin(),pviEnd=speakers.end(); pvi!=pviEnd; pvi++)
			newSR(where,*pvi,-1,whereSubject,whereVerb,wherePrep,m[where].getRelObject(),wherePrepObject,-1,stLOCATION,L"location move",true);
		if (speakers.empty())
			newSR(where, cObject::eOBJECTS::OBJECT_UNKNOWN_ALL,-1,whereSubject,whereVerb,wherePrep,m[where].getRelObject(),wherePrepObject,-1,stLOCATION,L"location move",true);
		for (; originalSize<(signed)syntacticRelationGroups.size(); originalSize++)
		{
			syntacticRelationGroups[originalSize].establishingLocation=!cancel;
			syntacticRelationGroups[originalSize].speakerContinuation=speakerContinuation;
		}
	}
	bool acceptableSubject=whereSubject>=0 && (m[whereSubject].objectRole&SUBJECT_ROLE) && ((m[whereSubject].getObject())>=0 || m[whereSubject].word->first==L"who");
	// command - don't make me do this!
	if (inPrimaryQuote && whereSubject<0 && whereVerb>=0 && (m[whereVerb].verbSense&(VT_TENSE_MASK|VT_EXTENDED))==VT_PRESENT &&
			(m[whereVerb].word->second.inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR))!=0)
		acceptableSubject=true;
		   /*(!(m[whereSubject].objectRole&PRIMARY_SPEAKER_ROLE) || m[whereSubject].relObject>=0) && */ //  && !(m[where].objectRole&PRIMARY_SPEAKER_ROLE)
	// ESTAB You[mr] want me[tuppence] to go to Madame[colombier] Colombier's
	// stMOVE Subject=You, me=object AT:Madame Columbier's
	// letters might be expected to arrive at Tommy's rooms
	int whereControllingEntity=-1;
	if (whereVerb>=0 && acceptableSubject && (whereSubject<0 || (m[whereSubject].getObject()>=0 &&
		  ((objects[m[whereSubject].getObject()].male || objects[m[whereSubject].getObject()].female) || (m[whereVerb].verbSense&VT_PASSIVE)))))
	{
		wstring verb;
		unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(whereVerb,verb);
		// 041073:role=([PRIM]) relSubject=-1,relVerb=-1,relObject=41074,relPrep=-1,relInternalVerb=41075
		// 041074:role=([EVAL][PRIM]) relSubject=-1,relVerb=41073,relObject=-1,relPrep=-1,relInternalVerb=-1
		// 041075:role=([PRIM]) relSubject=41074,relVerb=-1,relObject=41076,relPrep=-1,relInternalVerb=-1
		// 041076:role=([OBJ][NOT][NONPAST][NONPRESENT][EVAL][PRIM]) relSubject=41074,relVerb=41075,relObject=-1,relPrep=-1,relInternalVerb=-1
		if ((m[whereVerb].getRelVerb()>=0 || m[whereVerb].relInternalVerb>=0) && !m[whereVerb].andChainType)
		{
			if (m[whereVerb].getRelObject()>=0)
			{
				bool setControl=false;
				if (lvtoCi!=vbNetVerbToClassMap.end())
					for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
						setControl|=vbNetClasses[*vbi].control;
				if (setControl || m[whereVerb].getMainEntry()->first==L"make") // make is the only verbverbForm that when used this way, implies control
				{
					whereControllingEntity=whereSubject;
					whereSubject=m[whereVerb].getRelObject();
					int whereSecondaryVerb=(m[whereVerb].relInternalVerb>=0) ? m[whereVerb].relInternalVerb: m[whereVerb].getRelVerb();
					if (debugTrace.traceWhere)
						lplog(LOG_WHERE,L"%06d:main want verb %d:%s skipped in favor of infinitive complement verb %d:%s.",where,
							whereVerb,m[whereVerb].word->first.c_str(),whereSecondaryVerb,m[whereSecondaryVerb].word->first.c_str());
					whereVerb=whereSecondaryVerb;
				}
			}		
			else if (m[whereVerb].getRelVerb()>=0 && (!isSpecialVerb(whereVerb,false) || (m[whereVerb].getRelVerb()>=0 && isSpecialVerb(m[whereVerb].getRelVerb(),false) && m[m[whereVerb].getRelVerb()].getMainEntry()->first!=L"be" && m[m[whereVerb].getRelVerb()].getMainEntry()->first!=L"am") || verb==L"go")) // I am going to say...
			{
				if (debugTrace.traceWhere)
					lplog(LOG_WHERE,L"%06d:main verb %d:%s skipped in favor of infinitive complement verb %d:%s.",where,
					    whereVerb,m[whereVerb].word->first.c_str(),m[whereVerb].getRelVerb(),m[m[whereVerb].getRelVerb()].word->first.c_str());
				whereControllingEntity=whereSubject;
				whereVerb=m[whereVerb].getRelVerb();
			}
		}
		// She[girl] had noticed the speaker more than once amongst the first - class passengers .
		// NOT inQuote: CONTACTyou[sir] could call and LOCATIONsee me[carter] at the above LOCATIONaddress . whereControllingEntity=unknown. subject=sir
		// ALSO NOT I[whittington] happened to overhear part of your[tuppence] conversation with the young LOCATIONgentleman[glance] in Lyons's
		bool inSamePlaceByActivity=false;
		if (lvtoCi!=vbNetVerbToClassMap.end())
			for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
			{
				wstring id=vbNetClasses[*vbi].name();
				inSamePlaceByActivity=(like(id,L"see") || like(id,L"sight"));
			}
		// not at same place - He watched him go / From the shelter of the doorway LOCATIONhe[tommy] watched him[boris] EXITgo up the steps of a particularly evil - ESTABlooking house
		int tmpdebug=-1;
		if (m[whereVerb].getRelObject()>=0 && whereVerb>=0 && m[whereVerb].relInternalVerb>=0 && m[tmpdebug=m[whereVerb].relInternalVerb].relSubject==m[whereVerb].getRelObject() && isSpecialVerb(m[whereVerb].relInternalVerb,true))
			inSamePlaceByActivity=false;
		if (inSamePlaceByActivity)
		{
			bool location,timeUnit,found;
			int wherePrep=-1,wherePrepObject=findAnyLocationPrepObject(whereVerb,wherePrep,location,timeUnit),relObject=m[whereVerb].getRelObject();
			if (m[whereVerb].getRelVerb()<0 && relObject>=0 && wherePrepObject>=0 && location && !timeUnit && !inPrimaryQuote)
			{
				whereControllingEntity=whereSubject;
				whereSubject=relObject;
				newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,wherePrep,wherePrepObject,-1,-1,stLOCATION,L"by prep phrase",true);
				return;
			}		
			// I[whittington] happened to overhear part of your[tuppence] conversation with the young LOCATIONgentleman[glance] in Lyons's
			// subject 'see, sight' object's conversation[hyperNym auditory communication] in Lyon's
			int relPartObject=-1,whereOwner=-1;
			if (m[whereVerb].getRelVerb()<0 && relObject>=0 && wherePrepObject>=0 && location && !timeUnit &&
				  ((hasHyperNym(m[relObject].word->first,L"auditory_communication",found,false) && 
					 hasAgentObjectOwner(relObject,whereOwner)) ||
					((m[relObject].word->first==L"part" || m[relObject].word->first==L"portion") &&
					 m[relObject+1].word->first==L"of" && (relPartObject=m[relObject+1].getRelObject())>=0 &&
					(hasHyperNym(m[relPartObject].word->first,L"auditory_communication",found,false) && 
					 hasAgentObjectOwner(relPartObject,whereOwner)))))
			{
				whereControllingEntity=whereSubject;
				whereSubject=whereOwner;
				newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,wherePrep,wherePrepObject,-1,-1,stLOCATION,L"by prep phrase and owner",true);
				return;
			}		
		}
	}
	// this makes sure 'get' or 'got' is not misinterpreted as an stMOVE
	// these papers have got to be saved
	if (whereVerb>=0 && (m[whereVerb].word->first==L"get" || m[whereVerb].word->first==L"got") &&
		  m[whereVerb].getRelVerb()>=0 && m[whereVerb].getRelObject()<0)
	{
		if (debugTrace.traceWhere)
		lplog(LOG_WHERE,L"%06d:[GET] main verb %d:%s skipped in favor of infinitive complement verb %d:%s.",where,
			    whereVerb,m[whereVerb].word->first.c_str(),m[whereVerb].getRelVerb(),m[m[whereVerb].getRelVerb()].word->first.c_str());
		whereVerb=m[whereVerb].getRelVerb();
	}
	// Tuppence's hostel
	// take the packet to the American Embassy 
	// deliver it[packet,american] into the Ambassador's own hands[ambassador]
	// must have a physically present gendered subject that is not a body object.
	if (whereVerb>=0)
	{
		wstring verb;
		unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(whereVerb,verb);
		if (lvtoCi!=vbNetVerbToClassMap.end())
		{
			bool taken=false;
			int moveClass=-1,moveObjectClass=-1;
			for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
			{
				if (vbNetClasses[*vbi].move) moveClass=*vbi;
				if (vbNetClasses[*vbi].moveObject) moveObjectClass=*vbi;
			}
			if (moveClass>=0 && moveObjectClass>=0)
				taken=placeIdentification(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,(m[whereVerb].getRelObject()>=0) ? moveObjectClass:moveClass);
			if (!taken)
				for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
					if (vbNetClasses[*vbi].whereVerbClass() && (taken=placeIdentification(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,*vbi))) 
						break;
			if (!taken)
				for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
					if (!vbNetClasses[*vbi].whereVerbClass() && (taken=placeIdentification(where,inPrimaryQuote,whereControllingEntity,whereSubject,whereVerb,*vbi))) 
						break;
		}
		else 
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:PLACE transition not found:subject %s verb %s VBNET class not found",where,
						(whereSubject<0) ? L"(None)":objectString(m[whereSubject].getObject(),tmpstr,false).c_str(),verb.c_str());
			newSR(where,-1,whereControllingEntity,whereSubject,whereVerb,m[whereVerb].relPrep,m[whereVerb].getRelObject(),-1,-1,stOTHER,L"",false);
		}
	}
	bool transitionSinceEOS=false;
	if (syntacticRelationGroupDetected && ageTransition(whereSubject,false,transitionSinceEOS,-1,-1,lastSubjects,L"DSR"))
		primaryLocationLastMovingPosition=where;
	else if (syntacticRelationGroupsOriginalSize !=syntacticRelationGroups.size() || (speakerGroupsEstablished && m[where].hasSyntacticRelationGroup))
	{
		vector <cSyntacticRelationGroup>::iterator sri=syntacticRelationGroups.end();
		if (syntacticRelationGroupsOriginalSize==syntacticRelationGroups.size() || (sri=syntacticRelationGroups.begin()+(syntacticRelationGroups.size()-1))->where!=where)
			sri=findSyntacticRelationGroup(where);
		if (sri==syntacticRelationGroups.end() || sri->where!=where) return;
		if (!sri->agentLocationRelationSet && speakerGroupsEstablished)
			srSetTimeFlowTense((int) (sri-syntacticRelationGroups.begin()));
		processExit(where,sri,backInitialPosition,lastSubjects);
	}
}

bool addVCFrequency(int where,int fromWhere,vector <cWordMatch>::iterator im,bool helperVerb,sTrace &t,wstring &lastNounNotFound,wstring &lastVerbNotFound)
{ LFS
	tIWMM w=(im->word->second.mainEntry==wNULL) ? im->word : im->word->second.mainEntry;
	wstring in=w->first;
	int inflectionFlags=w->second.inflectionFlags;
	deriveMainEntry(where,fromWhere,in,inflectionFlags,true,false,lastNounNotFound,lastVerbNotFound);
	unordered_map <wstring, set<int> >::iterator inlvtoCi=vbNetVerbToClassMap.find(in);
	if (inlvtoCi!=vbNetVerbToClassMap.end())
	{
		im->flags|=cWordMatch::flagTempVNReAnalysis;
		for (set<int>::iterator vbi=inlvtoCi->second.begin(),vbiEnd=inlvtoCi->second.end(); vbi!=vbiEnd; vbi++)
		{
			if (helperVerb)
			{
				vbNetClasses[*vbi].totalFrequency--;
				vbNetClasses[*vbi].frequencyByMember[in]--;
			}
			else
			{
				vbNetClasses[*vbi].totalFrequency++;
				vbNetClasses[*vbi].frequencyByMember[in]++;
			}
			//lplog(LOG_RESOLUTION,L"%d:%s:%s %d %d",where,vbNetClasses[*vbi].name().c_str(),in.c_str(),vbNetClasses[*vbi].totalFrequency,vbNetClasses[*vbi].frequencyByMember[in]);
		}
		return true;
	}
	else if (!helperVerb && !(im->word->second.timeFlags&T_UNIT) && im->queryWinnerForm(verbForm)>=0 && t.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"Not found - %s - mainEntry %s?",im->word->first.c_str(),in.c_str());
	return false;
}

extern int numVbNetClassFound,numOneSenseVbNetClassFound,numMultiSenseVbNetClassFound,verbsMappedToVerbNet;
void cSource::analyzeWordSenses(void)
{ LFS 
	int endObject=0,inObject=-1,numAdjectivesAdverbs=0,numTimeNouns=0,numTimeVerbs=0,numIrregular=0,numPhysicalObjects=0,numNotPhysicalObjects=0,numBodyPartNouns=0;
	set <wstring> differentWords,differentVerbs,differentNouns,differentCommonWords;
	vector <cWordMatch>::iterator im=m.begin(),imend=m.end();
	int lastProgressPercent=-1,tmpPercent;
	for (int I=0; im!=imend; im++,I++)
	{
		if ((tmpPercent=I*100/m.size())>lastProgressPercent)
		{
			lastProgressPercent=tmpPercent;
			wprintf(L"PROGRESS: %03d%% words analyzed with %d seconds elapsed \r",lastProgressPercent,clocksec());
		}
		bool isNoun=im->queryWinnerForm(nounForm)>=0,isVerb=im->hasWinnerVerbForm(),isAdjective=im->queryWinnerForm(adjectiveForm)>=0,isAdverb=im->queryWinnerForm(adverbForm)>=0;
		if (!isNoun && !isVerb && (isAdjective || isAdverb)) 
		{
			numAdjectivesAdverbs++;
			continue;
		}
		if (im->getObject()>=0)
		{
			inObject=im->getObject();
			int nextEndObject=im->endObjectPosition;
			if (endObject<nextEndObject)
				endObject=nextEndObject;
		}
		if (I>=endObject)
			inObject=-1;
		int oc;
		if ((inObject<0 || ((oc=objects[inObject].objectClass)!=NAME_OBJECT_CLASS && 
			                   oc!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&  // oc!=BODY_OBJECT_CLASS && (glance [non-physical] vs arm [physical])
												 oc!=GENDERED_DEMONYM_OBJECT_CLASS && oc!=META_GROUP_OBJECT_CLASS &&
												 oc!=GENDERED_RELATIVE_OBJECT_CLASS)) && (isNoun || isVerb))
		{
			tIWMM w=(im->word->second.mainEntry==wNULL) ? im->word : im->word->second.mainEntry;
			set <wstring>::iterator dwi=(isNoun) ? differentNouns.find(w->first) : differentVerbs.find(w->first);
			if (dwi!=((isNoun) ? differentNouns.end() : differentVerbs.end())) 
			{
				if (!isNoun && addVCFrequency(I,1,im,false,debugTrace,lastNounNotFound,lastVerbNotFound))
				{
					verbsMappedToVerbNet++;
					im->flags|=cWordMatch::flagVAnalysis;
				}
				continue;
			}
			if ((im->word->second.timeFlags&T_UNIT) && !isVerb)
			{
				if (isNoun)
					numTimeNouns++;
			  continue;
			}
			if (isNoun && isVerb)
				isVerb=false;
			wstring proposedSubstitute;
			if (isNoun)
			{
				bool singular;
				if (isInternalBodyPart(I) || isExternalBodyPart(I,singular,true))
				{
					numBodyPartNouns++;
					numPhysicalObjects++;
				}
				differentNouns.insert(dwi,w->first);
				bool measurableObject=false,notMeasurableObject=false,grouping=false;				
				analyzeNounClass(I,1,w->first,w->second.inflectionFlags,measurableObject,notMeasurableObject,grouping,debugTrace,lastNounNotFound,lastVerbNotFound);
				if (measurableObject)
				{
					numPhysicalObjects++;
					w->second.flags|=cSourceWordInfo::physicalObjectByWN;
				}
				else if (notMeasurableObject)
				{
					numNotPhysicalObjects++;
					w->second.flags|=cSourceWordInfo::notPhysicalObjectByWN;
				}
				else
					w->second.flags|=cSourceWordInfo::uncertainPhysicalObjectByWN;
			}
			if (isVerb)
			{
				differentVerbs.insert(dwi,w->first);
				analyzeVerbNetClass(I,w->first,proposedSubstitute,numIrregular,w->second.inflectionFlags,debugTrace,lastNounNotFound,lastVerbNotFound);
				if (addVCFrequency(I,1,im,false,debugTrace,lastNounNotFound,lastVerbNotFound))
				{
					verbsMappedToVerbNet++;
					im->flags|=cWordMatch::flagVAnalysis;
				}
			}
			if (proposedSubstitute.length())
				differentCommonWords.insert(proposedSubstitute);
			else
				differentCommonWords.insert(w->first);
		}
		else if (inObject>=0)
		{
			I=endObject-1;
			im=m.begin()+I;
			inObject=-1;
		}
	}
	lplog(LOG_RESOLUTION,L"%d words total. %d adjectives & adverbs.  %d common words, %d common verbs %d common nouns %d winnowed words. \n"
		L"VERBS:%d vbNet class. %d 1 sense vbNet class. %d multi sense vbNet class. %d irregular. %d left\n"
		L"NOUNS:%d physical objects. %d not physical objects. %d left",
		m.size(),numAdjectivesAdverbs,differentWords.size()-numTimeNouns-numTimeVerbs,differentVerbs.size(),differentNouns.size(),differentCommonWords.size(),
		numVbNetClassFound,numOneSenseVbNetClassFound,numMultiSenseVbNetClassFound,numIrregular,differentVerbs.size()-numVbNetClassFound-numOneSenseVbNetClassFound-numMultiSenseVbNetClassFound-numIrregular,
		numPhysicalObjects,numNotPhysicalObjects,differentNouns.size()-numPhysicalObjects-numNotPhysicalObjects);
	wprintf(L"PROGRESS: 100%% words analyzed with %d seconds elapsed \n",clocksec());
}

void cSource::printVerbFrequency()
{ LFS
	int totalNumVerbs=0;
	vector <cWordMatch>::iterator im=m.begin(),imend=m.end();
	for (int I=0; im!=imend; im++,I++)
	{
		if ((im->flags&cWordMatch::flagTempVNReAnalysis) && 
			  ((im->relSubject<0 && im->getRelObject()<0 && im->queryWinnerForm(verbForm)<0 && im->queryWinnerForm(thinkForm)<0) || im->queryWinnerForm(verbverbForm)>=0))
		{
			// eliminate helper verbs
			addVCFrequency(I,1,im,true,debugTrace,lastNounNotFound,lastVerbNotFound);
			verbsMappedToVerbNet--;
			im->flags&=~cWordMatch::flagVAnalysis;
		}
		else if (im->hasWinnerVerbForm() && im->queryWinnerForm(nounForm)<0 && // don't count confused words (in reference to verbNet mapping)
			!((im->relSubject<0 && im->getRelObject()<0 && im->queryWinnerForm(verbForm)<0 && im->queryWinnerForm(thinkForm)<0) || im->queryWinnerForm(verbverbForm)>=0)) 
		{
			totalNumVerbs++;
			if (im->word->first==L"ishas" || im->word->first==L"wouldhad")
			{
				verbsMappedToVerbNet++;
				im->flags|=cWordMatch::flagVAnalysis;
			}
			// words that are not covered by verbNet
			//if (!(im->flags&cWordMatch::flagVAnalysis))
			//	lplog(LOG_TIME,L"XXR %06d %s",I,im->getMainEntry()->first.c_str());
		}
	}
	if (debugTrace.traceSpeakerResolution)
	{
		int numVerbNetVerbsFound=0,totalFrequency=0;
		unordered_map <int,int> fVBClassMap;
		for (int I=0; I<(signed)vbNetClasses.size(); I++)
			if (vbNetClasses[I].totalFrequency)
				fVBClassMap[vbNetClasses[I].totalFrequency]=I;
		for (unordered_map <int,int>::iterator mI=fVBClassMap.begin(); mI!=fVBClassMap.end(); mI++)
		{
			if (vbNetClasses[mI->second].incorporatedVerbClass())
				numVerbNetVerbsFound+=mI->first;
			totalFrequency+=mI->first;
			wstring words,tmpstr;
			unordered_map <int,wstring> fVBClassMembersMap;
			for (unordered_map<wstring,int>::iterator vi=vbNetClasses[mI->second].frequencyByMember.begin(),viEnd=vbNetClasses[mI->second].frequencyByMember.end(); vi!=viEnd; vi++)
				fVBClassMembersMap[vi->second]=vi->first;
			for (unordered_map <int,wstring>::iterator mcI=fVBClassMembersMap.begin(); mcI!=fVBClassMembersMap.end(); mcI++)
				words=mcI->second+L":"+itos(mcI->first,tmpstr)+L" "+words;
			wstring vcs;
			vbNetClasses[mI->second].incorporatedVerbClassString(vcs);
			lplog(LOG_RESOLUTION,L"%06d:%s%s[%s] (%s)",mI->first,(!vbNetClasses[mI->second].incorporatedVerbClass()) ? L"!":L"",vbNetClasses[mI->second].name().c_str(),vcs.c_str(),words.c_str());
		}
		if (totalFrequency && totalNumVerbs)
		{
			lplog(LOG_RESOLUTION,L"%% number of verbs in VerbNet=%d/%d %03d%%.",verbsMappedToVerbNet,totalNumVerbs,verbsMappedToVerbNet*100/totalNumVerbs);
			lplog(LOG_RESOLUTION,L"%% VerbNet covered under higher categories=%d/%d %03d%%.",numVerbNetVerbsFound,totalFrequency,numVerbNetVerbsFound*100/totalFrequency);
		}
	}
}

void cSource::processExit(int where,vector <cSyntacticRelationGroup>::iterator sri,int backInitialPosition,vector <int> &lastSubjects)
{ LFS
	bool inPrimaryQuote=(m[where].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0;
	bool inSecondaryQuote=(m[where].objectRole&IN_SECONDARY_QUOTE_ROLE)!=0;
	bool physicallyEvaluated=false,allIn,oneIn;
	wstring tmpstr;
	int tense;
	if ((sri->relationType==stEXIT || sri->relationType==stENTER) && !inPrimaryQuote && !inSecondaryQuote && 
			sri->whereSubject>=0 && !(m[sri->whereSubject].objectRole&THINK_ENCLOSING_ROLE) && !(m[sri->whereSubject].flags&cWordMatch::flagInQuestion) &&
		  sri->whereVerb>=0 && ((tense=m[sri->whereVerb].verbSense)&(VT_TENSE_MASK|VT_POSSIBLE|VT_NEGATION))==VT_PAST)
	{
		// For a minute Tuppence [thought] she was going to spring upon her - if EXIT is owned by a "think" verb, don't exit
		int so=m[sri->whereSubject].getObject();
		bool acceptableSubject=so>=0 && (objects[so].male || objects[so].female || objects[so].getSubType()==MOVING);
		for (vector <cOM>::iterator omi=m[sri->whereSubject].objectMatches.begin(),omiEnd=m[sri->whereSubject].objectMatches.end(); omi!=omiEnd && !acceptableSubject; omi++)
			acceptableSubject=(objects[omi->object].male || objects[omi->object].female || objects[omi->object].getSubType()==MOVING);
		// The footsteps died away .
		if (m[sri->whereSubject].word->first==L"all" || (so>=0 && objects[so].objectClass==BODY_OBJECT_CLASS && m[sri->whereSubject].word->first!=L"footsteps")) acceptableSubject=false;
		// 'Left to himself', Tommy would probably have sat down to think things out for a good half-hour before he decided on a plan of action.
		if (m[sri->whereSubject].getRelVerb()>=0 && sri->whereVerb<sri->whereSubject && m[sri->whereSubject].getRelVerb()!=sri->whereVerb &&
			  ((m[m[sri->whereSubject].getRelVerb()].verbSense)&(VT_TENSE_MASK|VT_POSSIBLE|VT_NEGATION))!=VT_PAST)
			acceptableSubject=false;
		if (acceptableSubject)
		{
			vector <cLocalFocus>::iterator lsi;
			if (sri->relationType==stENTER)
			{
				for (int omi=0; omi<(signed)m[sri->whereSubject].objectMatches.size(); omi++)
					if ((lsi=in(m[sri->whereSubject].objectMatches[omi].object))!=localObjects.end())
						lsi->lastEntrance=where;
				return;
			}
			if (m[sri->whereSubject].objectMatches.size()>0)
				so=m[sri->whereSubject].objectMatches[0].object;
			// is the object POV?
			set <int> povSpeakers;
			getPOVSpeakers(povSpeakers);
			if (povSpeakers.empty() && !speakerGroupsEstablished) 
			{
				// if whereSubject is present between where and backInitialPosition, they are povSpeakers.
				if (m[sri->whereSubject].objectMatches.empty() && (lsi=in(m[sri->whereSubject].getObject()))!=localObjects.end() && lsi->lastWhere>where && 
					  physicallyPresentPosition(lsi->lastWhere,physicallyEvaluated) && physicallyEvaluated &&
						// a page-boy went in search of him.
						!(sri->relationType==stEXIT && objects[lsi->om.object].originalLocation==sri->whereSubject)) // if stEXIT, and whereSubject is the first mention of the subject, skip.
					povSpeakers.insert(m[sri->whereSubject].getObject());
				else if (m[sri->whereSubject].objectMatches.size())
				{
					for (int omi=0; omi<(signed)m[sri->whereSubject].objectMatches.size(); omi++)
					{
						if ((lsi=in(m[sri->whereSubject].objectMatches[omi].object))!=localObjects.end() && lsi->lastWhere>where && 
						  physicallyPresentPosition(lsi->lastWhere,physicallyEvaluated) && physicallyEvaluated)
							povSpeakers.insert(m[sri->whereSubject].objectMatches[omi].object);
					}
					if (m[sri->whereSubject].objectMatches.size()!=povSpeakers.size())
						povSpeakers.clear();
				}
			}
			wstring povStr,ss,sRole;
			bool allPOVSpeakersInSubject=false,nonPOVSpeakerOverride=false;
			int csg=currentSpeakerGroup;
			if (csg>0 && ((unsigned)csg)<speakerGroups.size() && speakerGroups[csg].sgBegin>where) csg--;
			// if no POV, and several PP speakers, and only one speaker exists in next speaker group, pick that one as a POV
			if (povSpeakers.empty() && speakerGroupsEstablished && ((unsigned)csg+1)<speakerGroups.size() && 
				  (m[sri->whereSubject].objectMatches.empty() || m[sri->whereSubject].objectMatches.size()==1)) // definite match 
			{
				if (intersect(speakerGroups[csg].speakers,speakerGroups[csg+1].speakers,allIn,oneIn) && !allIn) // at least one doesn't match
				{
					// if this speaker exists in the next speaker group, it must be the POV.
					if (intersect(sri->whereSubject,speakerGroups[csg+1].speakers,allIn,oneIn) && allIn)
					{
						// make the POV the one that survives to the next speaker group (the one who exits and keeps the point-of-view
						povSpeakers.insert((m[sri->whereSubject].objectMatches.empty()) ? m[sri->whereSubject].getObject() : m[sri->whereSubject].objectMatches[0].object);
						lplog(LOG_RESOLUTION,L"%06d:Made lingering speaker %s POV.",where,whereString(sri->whereSubject,tmpstr,true).c_str());
					}
					// this speaker doesn't exist in the next speaker group. if the other speaker does, then the other speaker must be the POV.
					else 
					{
						set <int> s;
						set_intersection(speakerGroups[csg].speakers.begin(),speakerGroups[csg].speakers.end(),
														 speakerGroups[csg+1].speakers.begin(),speakerGroups[csg+1].speakers.end(),
														 std::inserter(s, s.begin()));
						if (s.size()==1)
						{
							povSpeakers.insert(*s.begin());
							lplog(LOG_RESOLUTION,L"%06d:Made lingering speaker %s POV (2).",where,objectString(*s.begin(),tmpstr,true).c_str());
						}
					}
				}
				else // no speakers match.  set nonPOVSpeakerOverride.
					nonPOVSpeakerOverride=true;
			}
			if (povSpeakers.empty()) 
				povStr=L"noPOV";
			else
				povStr=(intersect(sri->whereSubject,povSpeakers,allPOVSpeakersInSubject,oneIn)) ? L"notPOV":L"POV";
			lsi=localObjects.end();
			bool cancel=false,lastWherePP=false;
			if (!allPOVSpeakersInSubject)
			{
				//	uncancellable if person gets in MOVING **020887:Julius swung aboard - correct
				//	uncancellable if PLURAL **043475:All three moved away -  correct
				lastWherePP=(lsi=in(so))!=localObjects.end() && lsi->lastWhere>=0 && physicallyPresentPosition(lsi->lastWhere,physicallyEvaluated) && physicallyEvaluated;
				// Tuppence stared after him - character has left the immediate area, but is still visible.  A strong suggestion that they will leave, but not conclusive!
				bool lookFromAfar=lsi!=localObjects.end() && lsi->lastWhere>=0 && ((m[lsi->lastWhere].objectRole&PREP_OBJECT_ROLE)==PREP_OBJECT_ROLE && m[lsi->lastWhere].relPrep>=0 && m[m[lsi->lastWhere].relPrep].getRelVerb()>=0 && isVerbClass(m[m[lsi->lastWhere].relPrep].getRelVerb(),L"peer"));
						 lookFromAfar|=lsi!=localObjects.end() && lsi->lastWhere>=0 && ((m[lsi->lastWhere].objectRole&SUBJECT_ROLE)!=SUBJECT_ROLE && m[lsi->lastWhere].getRelVerb()>=0 && isVerbClass(m[lsi->lastWhere].getRelVerb(),L"peer"));
				cancel=(lsi!=localObjects.end() && lsi->lastWhere>where && lsi->lastWhere>m[where].getRelObject() && lastWherePP && !lookFromAfar);
				if (lsi!=localObjects.end() && lsi->lastWhere>where && lsi->lastWhere>m[where].getRelObject() && lastWherePP && lookFromAfar)
					lplog(LOG_RESOLUTION,L"%06d:lookFromAfar means this character really has left (cancels the PP at %d).",where,lsi->lastWhere);
				// speaking or being spoken to after exit
				cancel|=(lastOpeningPrimaryQuote>where && m[lastOpeningPrimaryQuote].audiencePosition>=0 && intersect(sri->whereSubject,m[lastOpeningPrimaryQuote].audiencePosition));
				cancel|=(lastOpeningPrimaryQuote>where && m[lastOpeningPrimaryQuote].speakerPosition>=0 && intersect(sri->whereSubject,m[lastOpeningPrimaryQuote].speakerPosition));
				if (speakerGroupsEstablished && lastOpeningPrimaryQuote>where)
				{
					cancel|=intersect(sri->whereSubject,m[lastOpeningPrimaryQuote].audienceObjectMatches,allIn,oneIn);
					cancel|=intersect(sri->whereSubject,m[lastOpeningPrimaryQuote].objectMatches,allIn,oneIn);
				}
			}
			intersect(sri->whereSubject,povSpeakers,allPOVSpeakersInSubject,oneIn);
			// only cancel if no povSpeakers or there are no povSpeakers in the subject.
			cancel=cancel && (povSpeakers.empty() || !oneIn) && m[sri->whereSubject].objectMatches.size()<=2;
			// one of the subjects is in the pov, the other isn't, but both are mentioned as exiting.  So cancel.
			// cancel|=oneIn && !allIn;
			cancel|=(m[sri->whereSubject].objectRole&THINK_ENCLOSING_ROLE)!=0;
			if (!(tense&VT_PASSIVE) && !(m[sri->whereSubject].objectRole&THINK_ENCLOSING_ROLE) && lsi!=localObjects.end() && debugTrace.traceWhere)
				lplog(LOG_WHERE,L"%06d:SGPEXIT %s- %s (lastWhere=%d:previousWhere=%d:%s %s) tense %s subject role=%s.",
							where,(cancel) ? L"CANCEL ":L"",povStr.c_str(),lsi->lastWhere,lsi->previousWhere,
							(lastWherePP) ? L"PP":L"notPP",(physicallyEvaluated) ? L"eval":L"not eval",
							senseString(ss,tense).c_str(),m[sri->whereSubject].roleString(sRole).c_str());
			int subjectObject=m[sri->whereSubject].getObject();
			if (subjectObject>=0) 
			{
				int wo=objects[subjectObject].getOwnerWhere();
				/* the subject is an exact count of people and the number of people matched does not equal the count */
				if (wo<0 && wo!=-1 && cObject::wordOrderWords[-2-wo]==L"two" && m[sri->whereSubject].objectMatches.size()!=2)
					cancel=true;
				if (wo<0 && wo!=-1 && cObject::wordOrderWords[-2-wo]==L"three" && m[sri->whereSubject].objectMatches.size()!=3)
					cancel=true;
				if (m[where].queryWinnerForm(numeralCardinalForm)>=0 && m[where].word->first==L"two" && m[sri->whereSubject].objectMatches.size()!=2)
					cancel=true;
				if (m[where].queryWinnerForm(numeralCardinalForm)>=0 && m[where].word->first==L"three" && m[sri->whereSubject].objectMatches.size()!=3)
					cancel=true;
			}
			bool allSpeakers=false;
			// is everyone dispersing? / The two young EXITpeople[tommy,tuppence] went off in opposite directions 
			if (!cancel && !allPOVSpeakersInSubject && subjectObject>=0 && objects[subjectObject].plural && m[sri->whereSubject].objectMatches.size()>1)
			{
				allSpeakers=true;
				for (lsi=localObjects.begin(); lsi!=localObjects.end(); lsi++)
					if (lsi->physicallyPresent && lsi->numIdentifiedAsSpeaker>0 && in(lsi->om.object,m[sri->whereSubject].objectMatches)==m[sri->whereSubject].objectMatches.end())
					{
						if (lsi->lastWhere>=0 && m[lsi->lastWhere].objectMatches.size()) continue;
						if (lsi->lastWhere<0 || m[lsi->lastWhere].objectMatches.size()!=1 || in(m[lsi->lastWhere].objectMatches[0].object,m[sri->whereSubject].objectMatches)==m[sri->whereSubject].objectMatches.end())
							allSpeakers=false;
					}
				if (allSpeakers && debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:All speakers exit.",where);
			}
			// attempt to disambiguate a subject which is exiting (21092 with verb 21103)
			// if one is POV, and the other is not POV, then if notPOV is found after exit, then it must not be exiting.
			// also don't erase if numbered metagroup (all three)
			if (!cancel && m[sri->whereSubject].objectMatches.size()>1 && !allSpeakers && !allPOVSpeakersInSubject &&
				  m[sri->whereSubject].getObject()>=0 && !objects[m[sri->whereSubject].getObject()].plural)
			{
				int eraseOtherEntry=-1,numFoundAfter=0,foundAfterEntry=-1;
				for (int omi=0; omi<(signed)m[sri->whereSubject].objectMatches.size(); omi++)
				{
					if ((lsi=in(m[sri->whereSubject].objectMatches[omi].object))!=localObjects.end())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:exit disambiguation - %s:last exit in paragraph=%d POV=%s",
								where,objectString(m[sri->whereSubject].objectMatches[omi].object,tmpstr,true).c_str(),lsi->lastWhere,(povSpeakers.find(lsi->om.object)!=povSpeakers.end()) ? L"true":L"false");
						if (povSpeakers.find(lsi->om.object)==povSpeakers.end())
						{
							// notPOV is found after exit - erase entry
							if (lsi->lastWhere>where)
							{
								foundAfterEntry=omi;
								numFoundAfter++;
							}
							// notPOV is NOT found after exit - eraseOtherEntry if found after exit
							else
							{
								eraseOtherEntry=omi;
								if (debugTrace.traceSpeakerResolution)
									lplog(LOG_RESOLUTION,L"%06d:exit disambiguation - eraseOtherEntry preference set to %s",
											where,objectString(m[sri->whereSubject].objectMatches[omi].object,tmpstr,true).c_str());
							}
						}
					}
				}
				if (numFoundAfter==1)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:exit disambiguation - erased nonPOV %s",
							where,objectString(m[sri->whereSubject].objectMatches[foundAfterEntry].object,tmpstr,true).c_str());
					m[sri->whereSubject].objectMatches.erase(m[sri->whereSubject].objectMatches.begin()+foundAfterEntry);
				}
				if (numFoundAfter==m[sri->whereSubject].objectMatches.size())
				{
					lplog(LOG_RESOLUTION,L"%06d:EXIT cancelled (nonPOV comes back) (numFoundAfter=%d)!",sri->where,numFoundAfter);
					return;
				}
				if (eraseOtherEntry>=0 && m[sri->whereSubject].objectMatches.size()>1)
				{
					for (int omi=0; omi<(signed)m[sri->whereSubject].objectMatches.size(); )
					{
						if ((lsi=in(m[sri->whereSubject].objectMatches[omi].object))!=localObjects.end() && 
							  // POV cannot disappear - the only thing POV can do is make nonPOV exit
							  (lsi->lastWhere>where || povSpeakers.find(lsi->om.object)!=povSpeakers.end()))
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"%06d:exit disambiguation - eraseOtherEntry preference erased %s",
										where,objectString(m[sri->whereSubject].objectMatches[omi].object,tmpstr,true).c_str());
							m[sri->whereSubject].objectMatches.erase(m[sri->whereSubject].objectMatches.begin()+omi);
						}
						else
							omi++;												
					}
				}
			}
			if (!cancel && ((speakerGroupsEstablished  && (povSpeakers.size() || nonPOVSpeakerOverride)) || 
				              (!speakerGroupsEstablished && (m[sri->whereSubject].objectMatches.size()<=1 || allSpeakers || allPOVSpeakersInSubject))))
			{
				bool spatialSeparation=isSpatialSeparation(sri->whereVerb);
				bool sgOccurredAfter=false,audienceOccurredAfter=false,speakerOccurredAfter=false;
				if (!intersect(sri->whereSubject,povSpeakers,allIn,oneIn))
				{
					// does the subject exit and then come back, or continue the action?
					// There EXITthey[whittington,boris,chance] went up to the first floor , and CONTACTsat at a small LOCATIONtable in the window .
					// The EXITbutler[butler] retired , ENTERreturning a moment or two later. 
					if (sri+1<syntacticRelationGroups.end() && (sri+1)->whereSubject==sri->whereSubject && (sri+1)->relationType!=stEXIT)
					{
						lplog(LOG_RESOLUTION,L"%06d:EXIT cancelled (nonPOV comes back)!",sri->where);
						return;
					}
					if (sri->speakerContinuation && isSpeakerContinued(where,so,lastWherePP,sgOccurredAfter,audienceOccurredAfter,speakerOccurredAfter))
						lplog(LOG_RESOLUTION,L"%06d:reject [%s %s %s]?",sri->whereSubject,(sgOccurredAfter) ? L"sgOccurredAfter":L"",(audienceOccurredAfter) ? L"audienceOccurredAfter":L"",(speakerOccurredAfter) ? L"speakerOccurredAfter":L"");
					if (lsi==localObjects.end())
					{
						lplog(LOG_RESOLUTION,L"%06d:exit processing - illegal lsi found",sri->whereSubject);
						return;
					}
					if (lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent>backInitialPosition) 
						return;
					if (lsi->physicallyPresent && debugTrace.traceSpeakerResolution)
						lplog(LOG_SG|LOG_RESOLUTION,L"%06d:Made %s not present 3 [%s][%s]",where,objectString(lsi->om,tmpstr,true).c_str(),
							(lsi->occurredInPrimaryQuote) ? L"PRIM":L"",(lsi->occurredOutsidePrimaryQuote) ? L"OUTSIDE":L"");
					if (lsi->physicallyPresent)
					{
						lsi->physicallyPresent=false;
						lsi->lastExit=where;
					}
					// if tempSpeakerGroup begins before the transition, remote from tempSpeakerGroup
					// also remove from previousLastSpeakers, lastSubjects & previousLastSubjects, and nextNarrationSubjects
					if (tempSpeakerGroup.sgBegin>where)
						tempSpeakerGroup.removeSpeaker(lsi->om.object);
					for (vector <int>::iterator si=nextNarrationSubjects.begin(),siEnd=nextNarrationSubjects.end(); si!=siEnd; si++)
						if (*si==lsi->om.object)
						{
							nextNarrationSubjects.erase(si);
							break;
						}
					for (vector <int>::iterator si=lastSubjects.begin(),siEnd=lastSubjects.end(); si!=siEnd; si++)
						if (*si==lsi->om.object)
						{
							lastSubjects.erase(si);
							break;
						}
					ageSpeaker(where,lsi,EOS_AGE);
				}
				else
				{
					// Tuppence brought in the coffee and liqueurs and unwillingly EXITretired . As she[tuppence] did so , she[tuppence] heard Boris say
					// cancel this EXIT regardless of whether if it is a POV
					if (m[sri->whereSubject].flags&cWordMatch::flagInLingeringStatement)
					{
						lplog(LOG_RESOLUTION,L"%06d:EXIT cancelled (lingering)!",sri->where);
						return;
					}
					for (int S1=sri->whereSubject; S1<(signed)m.size() && S1<sri->whereSubject+50; S1++)
					{
						if (m[S1].pma.queryPattern(L"__S1")!=-1 && (m[S1].flags&cWordMatch::flagInLingeringStatement))
							for (int S2=S1; S2<(signed)m.size() && S2<sri->whereSubject+50; S2++)
							{
								if ((m[S2].objectRole&SUBJECT_ROLE) && m[S2].getObject()>=0)
								{
									if (intersect(sri->whereSubject,S2) && m[S2].getRelVerb()>=0 && m[S2].getRelObject()>=0 && m[m[S2].getRelVerb()].getMainEntry()->first==L"do" && m[m[S2].getRelObject()].word->first==L"so")
									{
										lplog(LOG_RESOLUTION,L"%06d:EXIT cancelled (lingering 2)!",sri->where);
										return;
									}
									break;
								}
							}
					}
					int excludeWhere=sri->whereSubject;
					// if all speakers exiting, preserve all the speakers in the next speaker group (if intersecting).
					if (speakerGroupsEstablished  && allPOVSpeakersInSubject && currentSpeakerGroup+1<speakerGroups.size() && intersect(sri->whereSubject,speakerGroups[currentSpeakerGroup+1].speakers,allIn,oneIn))
					{
						if (allIn) 
							spatialSeparation=false;
						else // this assumes a subject is no more than two people
							for (set <int>::iterator s=speakerGroups[currentSpeakerGroup+1].speakers.begin(); s!=speakerGroups[currentSpeakerGroup+1].speakers.end(); s++)
							{
								if (in(*s,sri->whereSubject))
								{
									spatialSeparation=false;
									excludeWhere=objects[*s].originalLocation;
								}
							}
					}
					bool transitionSinceEOS=false;
					ageTransition(sri->whereSubject,false,transitionSinceEOS,-1,(spatialSeparation) ? -1 : excludeWhere,lastSubjects,L"DSR 2");
				}
			}
		}
	}
}

void cSource::logSpaceCheck(void)
{ LFS
	wstring tmpstr,tmpstr2;
	int lastSPTAnchor=-1;
	for (int I=0; I<(signed)syntacticRelationGroups.size(); I++)
	{
		if (debugTrace.traceWhere)
		lplog(LOG_WCHECK,L"%06d:%s",syntacticRelationGroups[I].where,syntacticRelationGroups[I].description.c_str());
		for (int J=0; J<(signed)syntacticRelationGroups[I].timeInfo.size(); J++)
		{
			//if (syntacticRelationGroups[I].tft.presentHappening)
			syntacticRelationGroups[I].timeInfo[J].timeSPTAnchor=lastSPTAnchor;
			//if (!syntacticRelationGroups[I].tft.presentHappening && syntacticRelationGroups[I].timeInfo[J].timeRTAnchor<0)
			//	syntacticRelationGroups[I].timeInfo[J].timeRTAnchor=lastSPTAnchor;
			if (debugTrace.traceTime)
			lplog(LOG_TIME,L"%06d:%s %s",syntacticRelationGroups[I].where,(lastSPTAnchor>=0) ? itos(lastSPTAnchor,tmpstr2).c_str():L"",syntacticRelationGroups[I].timeInfo[J].toString(m,tmpstr).c_str());
		}
		if (syntacticRelationGroups[I].timeProgression>0 || syntacticRelationGroups[I].relationType==stABSTIME)
			lastSPTAnchor=syntacticRelationGroups[I].where;
	}
}