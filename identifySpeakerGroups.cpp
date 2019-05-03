#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "vcXML.h"
#include <wn.h>
#include "profile.h"

extern set<int>::iterator sNULL;

int copy(cOM &num,char *buf,int &where)
{ DLFS
  num=*((cOM *)(buf+where));
  where+=sizeof(num);
  return 0;
}

bool copy(vector <cOM> &s,char *buf,int &where,int limit)
{ DLFS
  int count;
  if (!copy(count,buf,where,limit)) return false;
  for (int I=0; I<count; I++)
  {
    cOM i;
		if (!copy(i, buf, where))
			return false;
    s.push_back(i);
  }
  return true;
}

bool copy(void *buf,cOM num,int &where,int limit)
{ DLFS
  *((cOM *)(((char *)buf)+where))=num;
  where+=sizeof(num);
	if (where>limit) 
		lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached (3)!",limit);
  return true;
}

bool copy(void *buf,vector <cOM> &s,int &where,int limit)
{ DLFS
  int count=s.size();
  if (!copy(buf,count,where,limit)) return false;
	for (vector<cOM>::iterator is = s.begin(), isEnd = s.end(); is != isEnd; is++)
		if (!copy(buf, *is, where, limit))
			return false;
  return true;
}

Source::cSpeakerGroup::cSpeakerGroup(void)
{ LFS
  sgBegin=sgEnd=0;
  section=0;
  previousSubsetSpeakerGroup=-1;
	saveNonNameObject=-1;
	conversationalQuotes=0;
	speakersAreNeverGroupedTogether=true;
}

Source::cSpeakerGroup::cSpeakerGroup(char *buffer,int &where,unsigned int limit,bool &error)
{ LFS
	if (error=!::copy(sgBegin,buffer,where,limit)) return;
  if (error=!::copy(sgEnd,buffer,where,limit)) return;
  if (error=!::copy(section,buffer,where,limit)) return;
  if (error=!::copy(previousSubsetSpeakerGroup,buffer,where,limit)) return;
  if (error=!::copy(saveNonNameObject,buffer,where,limit)) return;
  if (error=!::copy(conversationalQuotes,buffer,where,limit)) return;
  if (error=!::copy(speakers,buffer,where,limit)) return;
  if (error=!::copy(fromNextSpeakerGroup,buffer,where,limit)) return;
  if (error=!::copy(replacedSpeakers,buffer,where,limit)) return;
  if (error=!::copy(singularSpeakers,buffer,where,limit)) return;
  if (error=!::copy(groupedSpeakers,buffer,where,limit)) return;
  if (error=!::copy(povSpeakers,buffer,where,limit)) return;
  if (error=!::copy(dnSpeakers,buffer,where,limit)) return;
  if (error=!::copy(metaNameOthers,buffer,where,limit)) return;
  if (error=!::copy(observers,buffer,where,limit)) return;
  unsigned int count;
  if (error=!::copy(count,buffer,where,limit)) return;
	for (unsigned int I=0; I<count && !error; I++)
    embeddedSpeakerGroups.push_back(cSpeakerGroup(buffer,where,limit,error));
	if (error) return;
  if (error=!::copy(count,buffer,where,limit)) return;
  for (unsigned int I=0; I<count && !error; I++)
		groups.push_back(cSpeakerGroup::cGroup(buffer,where,limit,error));
	if (error) return;
	__int64 flags;
  if (error=!::copy(flags,buffer,where,limit)) return;
	speakersAreNeverGroupedTogether=(flags&1) ? true : false;
	tlTransition=(flags&2) ? true : false;
  error=where>(signed)limit;
}

bool Source::cSpeakerGroup::copy(void *buffer,int &where,int limit)
{ LFS
	if (!::copy(buffer,sgBegin,where,limit)) return false;
  if (!::copy(buffer,sgEnd,where,limit)) return false;
  if (!::copy(buffer,section,where,limit)) return false;
  if (!::copy(buffer,previousSubsetSpeakerGroup,where,limit)) return false;
  if (!::copy(buffer,saveNonNameObject,where,limit)) return false;
  if (!::copy(buffer,conversationalQuotes,where,limit)) return false;
  if (!::copy(buffer,speakers,where,limit)) return false;
  if (!::copy(buffer,fromNextSpeakerGroup,where,limit)) return false;
  if (!::copy(buffer,replacedSpeakers,where,limit)) return false;
  if (!::copy(buffer,singularSpeakers,where,limit)) return false;
  if (!::copy(buffer,groupedSpeakers,where,limit)) return false;
  if (!::copy(buffer,povSpeakers,where,limit)) return false;
  if (!::copy(buffer,dnSpeakers,where,limit)) return false;
  if (!::copy(buffer,metaNameOthers,where,limit)) return false;
  if (!::copy(buffer,observers,where,limit)) return false;
  if (!::copy(buffer,(int)embeddedSpeakerGroups.size(),where,limit)) return false;
	for (vector <Source::cSpeakerGroup>::iterator esgi=embeddedSpeakerGroups.begin(), esgEnd=embeddedSpeakerGroups.end(); esgi!=esgEnd; esgi++)
    if (!esgi->copy(buffer,where,limit)) return false;
  if (!::copy(buffer,(int)groups.size(),where,limit)) return false;
	for (unsigned int I=0; I<groups.size(); I++)
		if (!groups[I].copy(buffer,where,limit)) return false;
	__int64 flags=(speakersAreNeverGroupedTogether) ? 1 : 0;
	flags|=(tlTransition) ? 2 : 0;
  if (!::copy(buffer,flags,where,limit)) return false;
  return true;
}

void Source::ageSpeakerWithoutSpeakerInfo(int where,bool inPrimaryQuote,bool inSecondaryQuote,vector <cLocalFocus>::iterator &lfi,int amount)
{ LFS
  lfi->increaseAge(inPrimaryQuote,inSecondaryQuote,amount);
  if (objects[lfi->om.object].objectClass!=NAME_OBJECT_CLASS &&
      lfi->getTotalAge()>MAX_AGE &&
      lfi->numIdentifiedAsSpeaker==0 &&
      lfi->numEncounters<2)
  {
    wstring tmpstr;
	  if (debugTrace.traceSpeakerResolution)
		  lplog(LOG_SG,L"%06d:%02d     object %s eliminated from local objects",
			  where,section,objectString(lfi->om.object,tmpstr,true).c_str());
    lfi=localObjects.erase(lfi);
  }
  else
    lfi++;
}

void Source::mergeName(int where,int &o,set <int> &speakers)
{ LFS
  vector <cObject>::iterator object=objects.begin()+o;
  for (set<int>::iterator i=speakers.begin(); i!=speakers.end(); i++)
    if (o!=*i && object->confidentMatch(objects[*i],debugTrace))
    {
      replaceObjectWithObject(where,object,*i,L"mergeName");
			speakers.erase(o);
      o=*i;
			object=objects.begin()+*i;
			i=speakers.begin();
    }
}

bool Source::mergableBySex(int o,set <int> &speakers,bool &uniquelyMergable,set<int>::iterator &mergedObject)
{ LFS
  bool genderUncertainMatch,physicallyEvaluated;
  mergedObject=sNULL;
  uniquelyMergable=true;
	// object must either be equal, or match sex and the object being matched against must either not be unresolvable or must occur before the object being matched
  for (set<int>::iterator i=speakers.begin(),iEnd=speakers.end(); i!=iEnd; i++)
    if (o==*i || (objects[o].matchGenderIncludingNeuter(objects[*i],genderUncertainMatch) && 
			(!(unResolvablePosition(objects[*i].begin) && physicallyPresentPosition(objects[*i].originalLocation,objects[*i].begin,physicallyEvaluated,false)) || 
			  objects[*i].firstLocation<objects[o].firstLocation)))
    {
      if (mergedObject!=sNULL) uniquelyMergable=false;
      mergedObject=i;
    }
  return mergedObject!=sNULL;
}

bool Source::unMergable(int where,int o,set <int> &speakers,bool &uniquelyMergable,bool insertObject,bool crossedSection,bool allowBothToBeSpeakers,bool checkUnmergableSpeaker,set <int>::iterator &save)
{ LFS
	if (o<=1) return true;
  followObjectChain(o);
	int at=m[objects[o].begin].principalWherePosition;
	if (at<0) at=m[objects[o].begin].principalWhereAdjectivalPosition;
	// if "The chauffeur" attempts to merge with the previous speakerGroup, Thomas and Boris, that should fail,
	//   because "The chauffeur" did not match either of those when it was first matched (so Thomas and Boris are not chauffeurs)
	//   BUT if Annette attempts to merge with "a girl" of the previous speakerGroup, that is OK, because Annette as a name will
	//     not match a girl because it is not allowed (downclass - a gendered noun doesn't match a name, because the name class is primary)
	bool implicitlyUnresolvable = at >= 0 && (m[at].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE) != 0 && objects[o].objectClass == NAME_OBJECT_CLASS;
	// if meta group objects aren't matched, then they will automatically be inserted
	if (where>=0 && objects[o].objectClass==META_GROUP_OBJECT_CLASS && m[where].objectMatches.empty())
		save=speakers.end();
  else if (!implicitlyUnresolvable && unResolvablePosition(objects[o].begin)) // && isPhysicallyPresent - o@where already went through this check
  {                                                //  but o by itself may not because o begin may not have been
    // only resolve with body objects              // physically present
    uniquelyMergable=true;
    save=speakers.find(o);
    if (save==speakers.end() && objects[o].ownerWhere>=0 && objects[o].objectClass==BODY_OBJECT_CLASS)
    {
      save=speakers.find(m[objects[o].ownerWhere].getObject());
      if (save!=speakers.end())
      {
        speakers.erase(save);
        pair< set<int>::iterator, bool > pr=speakers.insert(o);
        save=pr.first;
      }
    }
		// check for aliases
    for (set<int>::iterator i=speakers.begin(),iEnd=speakers.end(); i!=iEnd; i++)
		  if (matchAliases(where,o,*i))
			{
				save=i;
			  break;
			}
  }
  else
  {
    save=speakers.end();
    uniquelyMergable=true;
		int ownerObject=-1;
		if (objects[o].ownerWhere>=0 && m[objects[o].ownerWhere].objectMatches.size()==1) 
			ownerObject=m[objects[o].ownerWhere].objectMatches[0].object;
		if (objects[o].ownerWhere>=0 && m[objects[o].ownerWhere].objectMatches.empty() && m[objects[o].ownerWhere].getObject()>=0) 
			ownerObject=m[objects[o].ownerWhere].getObject();
    for (set<int>::iterator i=speakers.begin(),iEnd=speakers.end(); i!=iEnd; i++)
		{
			int speakerOwnerObject=-1;
			if (objects[*i].ownerWhere>=0 && m[objects[*i].ownerWhere].objectMatches.size()==1) 
				speakerOwnerObject=m[objects[*i].ownerWhere].objectMatches[0].object;
			if (objects[*i].ownerWhere>=0 && m[objects[*i].ownerWhere].objectMatches.empty() && m[objects[*i].ownerWhere].getObject()>=0) 
				speakerOwnerObject=m[objects[*i].ownerWhere].getObject();
			// if the current speaker group has crossed over into a new section, only merge with
			// those objects which have already appeared in the new section.
      if ((!crossedSection || objects[*i].numEncountersInSection || objects[*i].numIdentifiedAsSpeakerInSection) && 
				  speakerOwnerObject!=o && ownerObject!=*i &&
					(o==*i || potentiallyMergable(where,objects.begin()+o,objects.begin()+*i,allowBothToBeSpeakers,checkUnmergableSpeaker)))
      {
        if (save!=iEnd) uniquelyMergable=false;
        save=i;
      }
		}
  }
  if (save==speakers.end() && insertObject)
  {
    speakers.insert(o);
    save=speakers.end();
  }
  return save==speakers.end();
}

bool Source::unMergable(int where,int o,vector <int> &speakers,bool &uniquelyMergable,bool insertObject,bool crossedSection,bool allowBothToBeSpeakers,bool checkUnmergableSpeaker,vector <int>::iterator &save)
{ LFS
	if (o<=1) return true;
  followObjectChain(o);
	int at=m[objects[o].begin].principalWherePosition;
	if (at<0) at=m[objects[o].begin].principalWhereAdjectivalPosition;
  bool implicitlyUnresolvable=at>=0 && (m[at].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE)!=0 && objects[o].objectClass==NAME_OBJECT_CLASS;
	// isPhysicallyPresent - o@where already went through this check
	// but o by itself may not because o@begin may not have been
  // physically present
	bool positionUnresolvable=unResolvablePosition(m[where].beginObjectPosition);
	bool matchPositionUnresolvable=!implicitlyUnresolvable && unResolvablePosition(objects[o].begin);
  if (positionUnresolvable || matchPositionUnresolvable)
  {
    // only resolve with body objects
    uniquelyMergable=true;
    save=find(speakers.begin(),speakers.end(),o);
    if (save==speakers.end() && objects[o].ownerWhere>=0 && objects[o].objectClass==BODY_OBJECT_CLASS)
    {
      save=find(speakers.begin(),speakers.end(),m[objects[o].ownerWhere].getObject());
      if (save!=speakers.end())
      {
        speakers.erase(save);
        speakers.push_back(o);
        save=speakers.begin()+speakers.size()-1;
      }
    }
  }
  else
  {
    save=speakers.end();
    uniquelyMergable=true;
    for (vector <int>::iterator i=speakers.begin(),iEnd=speakers.end(); i!=iEnd; i++)
    {
      if ((!crossedSection || objects[*i].numEncountersInSection || objects[*i].numIdentifiedAsSpeakerInSection) && 
					(o==*i || potentiallyMergable(where,objects.begin()+o,objects.begin()+*i,allowBothToBeSpeakers,checkUnmergableSpeaker)))
      {
        if (save!=iEnd) uniquelyMergable=false;
        save=i;
      }
    }
  }
  if (save==speakers.end() && insertObject)
  {
    speakers.push_back(o);
    save=speakers.end();
  }
  return save==speakers.end();
}

void Source::replaceSpeaker(int begin,int end,int fromObject,int toObject)
{ LFS
  wstring tmpstr,tmpstr2,tmpstr3;
  vector <cSpeakerGroup>::iterator lastSG=(speakerGroups.size()) ? speakerGroups.begin()+speakerGroups.size()-1 : speakerGroups.end();
  if (debugTrace.traceSpeakerResolution)
    lplog(LOG_SG,L"%06d-%06d:%02d replaced speaker %s with %s in %s",begin,end,section,
          objectString(fromObject,tmpstr,true).c_str(),objectString(toObject,tmpstr2,true).c_str(),toText(*lastSG,tmpstr3));
	if (objects[fromObject].originalLocation>=begin && objects[fromObject].originalLocation<end)
	{
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_SG,L"%06d-%06d:RS %02d AT %d pushed %s (1)",begin,end,section,objects[fromObject].originalLocation,objectString(toObject,tmpstr2,true).c_str());
    m[objects[fromObject].originalLocation].objectMatches.clear();
    m[objects[fromObject].originalLocation].objectMatches.push_back(cOM(toObject,SALIENCE_THRESHOLD));
    objects[toObject].locations.push_back(objects[fromObject].originalLocation);
		objects[toObject].updateFirstLocation(objects[fromObject].originalLocation);
    m[objects[fromObject].originalLocation].flags|=WordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup|WordMatch::flagObjectResolved;
  }
  for (vector <cObject::cLocation>::iterator li=objects[fromObject].locations.begin(),liEnd=objects[fromObject].locations.end(); li!=liEnd; li++)
    if (li->at>=begin && li->at<end && m[li->at].getObject()==fromObject && // include all other chances for matching this unresolvable object
			  !(m[li->at].flags&WordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup)) // but not where this object is matched against others!
    {                                                                                                      
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_SG,L"%06d-%06d:RS %02d AT %d pushed %s (2)",begin,end,section,li->at,objectString(toObject,tmpstr2,true).c_str());
      m[li->at].objectMatches.clear();
      m[li->at].objectMatches.push_back(cOM(toObject,SALIENCE_THRESHOLD));
      objects[toObject].locations.push_back(li->at);
			objects[toObject].updateFirstLocation(li->at);
      m[li->at].flags|=WordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup|WordMatch::flagObjectResolved;
    }
  int o=fromObject;
  for (vector <cSpeakerGroup>::iterator sg=lastSG; sg->sgBegin>=begin; sg--)
  {
    if (sg->speakers.erase(o))
    {
      // this remembers speakers which are replaced backwards
      // this is used to also prevent these speakers from being rejected in the next stage (resolveSpeakers)
      // if this wasn't used, these speakers would be rejected before being evaluated.
      sg->replacedSpeakers.push_back(cOM(o,toObject)); // for unresolvable speakers which are only replaced in section
      sg->speakers.insert(toObject);
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_SG,L"%06d-%06d:%02d replaced -> %s",begin,end,section,toText(*sg,tmpstr3));
    }
    else if (debugTrace.traceSpeakerResolution)
        lplog(LOG_SG,L"%06d-%06d:%02d %s NOT FOUND",begin,end,section,toText(*sg,tmpstr3));
    if (sg==speakerGroups.begin()) break;
  }
}

// for each speaker in lastSG:
//   if speaker is not in tempSpeakerGroup (the next speaker group to be added)
//   and has a speakerAge>2, add to oldSpeakers.
// if oldSpeakers is empty, return.
// until oldSpeakers is empty:
//   find oldest speaker S (speaker with largest speakerAge) in oldSpeakers
//   find last speakerSection in speakerSections by (age of S -1) - the speaker section AFTER the last encounter of the speaker
//	 copy lastSG to speakerGroups (newSG)
//   set lastSG.end=speakerSection.
//   newSG.begin=speakerSection.
//   remove S from newSG.
//   make lastSG point to newSG.
void Source::determineSpeakerRemoval(int where)
{ LFS
	if (speakerGroups.empty()) return;
  vector <cSpeakerGroup>::iterator lastSG=speakerGroups.begin()+speakerGroups.size()-1;
	if (lastSG->speakers.size()<3) return;
	wstring tmpstr;
	vector <int> oldSpeakers;
	for (set <int>::iterator s=lastSG->speakers.begin(); s!=lastSG->speakers.end(); s++)
	  if (speakerAges[*s]>2 && tempSpeakerGroup.speakers.find(*s)==tempSpeakerGroup.speakers.end() && objects[*s].objectClass!=META_GROUP_OBJECT_CLASS)
			oldSpeakers.push_back(*s);
	if (oldSpeakers.empty()) 
	{
	  if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG,L"%d:Ages of %s are all < 2.",where,objectString(tempSpeakerGroup.speakers,tmpstr).c_str(),speakerSections.size());
		return;
	}
	while (oldSpeakers.size())
	{
		if (lastSG->speakers.size()<3) return;
		int oldestAge=-1,oldestSpeaker=-1;
		for (vector <int>::iterator s=oldSpeakers.begin(); s!=oldSpeakers.end(); s++)
			if (speakerAges[*s]>oldestAge)
				oldestAge=speakerAges[oldestSpeaker=*s];
		if (oldestAge>(int)speakerSections.size())
		{
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"%d:Age of %s is illegal (%d>=%d)",where,objectString(oldestSpeaker,tmpstr,true).c_str(),oldestAge,speakerSections.size());
			break;
		}
		if (lastSG->sgEnd==speakerSections[speakerSections.size()-oldestAge])
		{
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"%d:Oldest age of %s (%d) reached the end of the speaker group (%d)",where,objectString(oldestSpeaker,tmpstr,true).c_str(),oldestAge,lastSG->sgEnd);
			break;
		}
		// sanity check - is there a conversation in the speakergroup with only one speaker?
		int speakersToErase=0;
		for (set <int>::iterator s=lastSG->speakers.begin(); s!=lastSG->speakers.end(); s++)
			if (speakerAges[*s]>=oldestAge) 
				speakersToErase++;
		if (lastSG->speakers.size()-speakersToErase<=1)
		{
			// is there a quote from speakerSections[speakerSections.size()-oldestAge] to the end of lastSG?
			for (int I=speakerSections[speakerSections.size()-oldestAge]; I<lastSG->sgEnd; I++)
				if (m[I].objectRole&IN_PRIMARY_QUOTE_ROLE)
				{
				  if (debugTrace.traceSpeakerResolution)
					{
						lplog(LOG_SG,L"%d:speaker removal from %s leading to only one speaker in a group %d-%d with a quote is rejected.",where,
							toText(*lastSG,tmpstr),lastSG->sgBegin,speakerSections[speakerSections.size()-oldestAge]);
					lplog(LOG_SG,L"%d:%s",where,objectString(oldSpeakers,tmpstr).c_str());
					}
					return;
				}
		}
		// end sanity check
	  if (debugTrace.traceSpeakerResolution)
      lplog(LOG_SG,L"%06d-%06d:%02d   %s added (speakerRemoval)",lastSG->sgBegin,lastSG->sgEnd,section,toText(*lastSG,tmpstr));
		speakerGroups.push_back(*lastSG);
		lastSG=speakerGroups.begin()+speakerGroups.size()-2;
	  vector <cSpeakerGroup>::iterator newSG=speakerGroups.begin()+speakerGroups.size()-1;
	  if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG,L"section demarcation=%d from age %d.",speakerSections[speakerSections.size()-oldestAge],oldestAge);
		newSG->sgBegin=lastSG->sgEnd=speakerSections[speakerSections.size()-oldestAge];
		// if the new speaker group also contains meta-group class speakers that are older (they aren't in the new speaker group at all)
		for (set <int>::iterator s=newSG->speakers.begin(); s!=newSG->speakers.end(); )
			if (speakerAges[*s]>=oldestAge) 
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%d:%s removed (olderAge=%d).",where,objectString(*s,tmpstr,true).c_str(),speakerAges[*s]);
				newSG->speakers.erase(s++);
			}
			else
				s++;
		for (vector <int>::iterator s=oldSpeakers.begin(); s!=oldSpeakers.end(); )
			if (speakerAges[*s]==oldestAge)
				s=oldSpeakers.erase(s);
			else
			  s++;
		// distribute embedded speaker groups
		for (int esg=0; esg<(signed)lastSG->embeddedSpeakerGroups.size(); )
			if (lastSG->embeddedSpeakerGroups[esg].sgEnd<=0 || lastSG->embeddedSpeakerGroups[esg].sgBegin>lastSG->sgEnd)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d   embedded erased (lastSG) %s",lastSG->embeddedSpeakerGroups[esg].sgBegin,lastSG->embeddedSpeakerGroups[esg].sgEnd,section,toText(lastSG->embeddedSpeakerGroups[esg],tmpstr));
				lastSG->embeddedSpeakerGroups.erase(lastSG->embeddedSpeakerGroups.begin()+esg);
			}
			else
				esg++;
		// distribute embedded speaker groups
		for (int esg=0; esg<(signed)newSG->embeddedSpeakerGroups.size(); )
			if (newSG->embeddedSpeakerGroups[esg].sgBegin<newSG->sgBegin)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d   embedded erased (newSG) %s",newSG->embeddedSpeakerGroups[esg].sgBegin,newSG->embeddedSpeakerGroups[esg].sgEnd,section,toText(newSG->embeddedSpeakerGroups[esg],tmpstr));
				newSG->embeddedSpeakerGroups.erase(newSG->embeddedSpeakerGroups.begin()+esg);
			}
			else
				esg++;
	  if (debugTrace.traceSpeakerResolution)
		  lplog(LOG_SG,L"%06d-%06d:%02d   %s added",lastSG->sgBegin,lastSG->sgEnd,section,toText(*lastSG,tmpstr));
		determinePreviousSubgroup(where,speakerGroups.size()-2,&speakerGroups[speakerGroups.size()-2]);
		lastSG=newSG;
	}
}

void Source::subtract(set <int> &bigSet,set <int> &subtractSet,set <int> &resultSet)
{ LFS
	for (set <int>::iterator bi=bigSet.begin(),biEnd=bigSet.end(); bi!=biEnd; bi++)
		if (subtractSet.find(*bi)==subtractSet.end())
			resultSet.insert(*bi);
}

void Source::subtract(vector <cOM> &bigSet,vector <cOM> &subtractSet)
{ LFS
	vector <cOM> resultSet;
	for (vector <cOM>::iterator bi=bigSet.begin(),biEnd=bigSet.end(); bi!=biEnd; bi++)
		if (in(bi->object,subtractSet)==subtractSet.end())
			resultSet.push_back(*bi);
	bigSet=resultSet;
}

void Source::subtract(vector <cOM> &bigSet,vector <int> &subtractSet)
{ LFS
	vector <cOM> resultSet;
	for (vector <cOM>::iterator bi=bigSet.begin(),biEnd=bigSet.end(); bi!=biEnd; bi++)
		if (find(subtractSet.begin(),subtractSet.end(),bi->object)==subtractSet.end())
			resultSet.push_back(*bi);
	bigSet=resultSet;
}

void Source::subtract(vector <cOM> &bigSet,set <int> &subtractSet)
{ LFS
	vector <cOM> resultSet;
	for (vector <cOM>::iterator bi=bigSet.begin(),biEnd=bigSet.end(); bi!=biEnd; bi++)
		if (subtractSet.find(bi->object)==subtractSet.end())
			resultSet.push_back(*bi);
	bigSet=resultSet;
}

void Source::subtract(int o,vector <cOM> &objectMatches)
{ LFS
	vector <cOM>::iterator w=in(o,objectMatches);
	if (w!=objectMatches.end())
		objectMatches.erase(w);
}

int Source::getLastSpeakerGroup(int o,int lastSG)
{ LFS
	lastSG--;
  for (vector <cSpeakerGroup>::iterator sg=speakerGroups.begin()+lastSG; lastSG>=0; sg--,lastSG--)
		if (sg->speakers.find(o)!=sg->speakers.end())
			return lastSG;
	return -1;
}

void Source::determineSubgroupFromGroups(cSpeakerGroup &sg)
{ LFS
	wstring tmpstr;
	for (vector < cSpeakerGroup::cGroup >::iterator gi=sg.groups.begin(),giEnd=sg.groups.end(); gi!=giEnd; gi++)
	{
		bool allIn,oneIn;
		// if group only contains speakers
		if (gi->objects.size()>=sg.speakers.size())
		{
		  if (debugTrace.traceSpeakerResolution)
			  lplog(LOG_SG,L"%06d-%06d:%02d   groupSpeakers %s rejected (group==all speakers)",sg.sgBegin,sg.sgEnd,section,objectString(*gi,tmpstr).c_str());
		}
		else if (intersect(gi->objects,sg.speakers,allIn,oneIn) && allIn)
		{
			for (vector <int>::iterator si=gi->objects.begin(),siEnd=gi->objects.end(); si!=siEnd; si++)
				sg.groupedSpeakers.insert(*si);
			sg.singularSpeakers.clear();
		  subtract(sg.speakers,sg.groupedSpeakers,sg.singularSpeakers);
		  if (debugTrace.traceSpeakerResolution)
			  lplog(LOG_SG,L"%06d-%06d:%02d   groupSpeakers %s added (2)",sg.sgBegin,sg.sgEnd,section,objectString(*gi,tmpstr).c_str());
			break;
		}
	}
}

void Source::determinePreviousSubgroup(int where,int whichSG,cSpeakerGroup *lastSG)
{ LFS
	if (whichSG<0) return;
	vector <int> preferredMPluralGroup;
	bool headerPrinted=!debugTrace.traceSpeakerResolution,possibleCurrentSubgroup=false,allIn,oneIn;
  if (lastSG->speakers.size()>=2)
  {
		wstring tmpstr,tmpstr2;
		set <int> speakers=lastSG->speakers;
		// if there are any mplural grouped entities, use them first.
		for (vector < cSpeakerGroup::cGroup >::iterator gi=lastSG->groups.begin(),giEnd=lastSG->groups.end(); gi!=giEnd; gi++)
		{
			bool groupAllIn,groupOneIn;
			if ((m[gi->where].objectRole&MPLURAL_ROLE) && intersect(gi->objects,lastSG->speakers,groupAllIn,groupOneIn) && groupAllIn)
			{
				if (preferredMPluralGroup!=gi->objects)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG,L"SGP:   %d:Subgroup [FROM MPLURAL group] is %s.",where,objectString(gi->objects,tmpstr2).c_str());
					preferredMPluralGroup=gi->objects;
				}
			}	
		}
    // search all previous speaker groups in reverse order
    for (int sg=whichSG-1; sg>=0 && speakers.size()>=2; sg--)
    {
      vector <cSpeakerGroup>::iterator beforeLastSG=speakerGroups.begin()+sg;
			if (beforeLastSG->speakers.size()>lastSG->speakers.size() && 
				  (!beforeLastSG->groupedSpeakers.size() || beforeLastSG->groupedSpeakers.size()>lastSG->speakers.size()))
				 continue;
      int numFound=0,numSubgroupFound=0;
			// if there is no conversation in the speakerGroup, there is much less of a chance that the entities should be considered a "group"
			// They[streets,boris,tommy] reached at length[length] a small dilapidated square .
			// lastSG must contain all members of beforeLastSG
			for (set <int>::iterator blsi=beforeLastSG->speakers.begin(),blsiEnd=beforeLastSG->speakers.end(); blsi!=blsiEnd; blsi++)
				if (speakers.find(*blsi)!=speakers.end())
					numFound++;
			for (set <int>::iterator blsi=beforeLastSG->groupedSpeakers.begin(),blsiEnd=beforeLastSG->groupedSpeakers.end(); blsi!=blsiEnd; blsi++)
					if (speakers.find(*blsi)!=speakers.end())
						numSubgroupFound++;
			if (numSubgroupFound>1 && numSubgroupFound==beforeLastSG->groupedSpeakers.size())
      {
        lastSG->previousSubsetSpeakerGroup=beforeLastSG->previousSubsetSpeakerGroup;
				if (!headerPrinted)
				{
					lplog(LOG_SG,L"SGP[1]:------ %s -------",toText(*lastSG,tmpstr));
					headerPrinted=true;
				}
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d:SGP:   Subgroup is %s.",where,objectString(beforeLastSG->groupedSpeakers,tmpstr2).c_str());
				lastSG->groupedSpeakers=beforeLastSG->groupedSpeakers;
				lastSG->singularSpeakers.clear();
			  subtract(lastSG->speakers,lastSG->groupedSpeakers,lastSG->singularSpeakers);
        break;
      }
      // now mark all members of lastSG NOT in beforeLastSG
      else if (numFound>1 && (numFound==beforeLastSG->speakers.size()) && lastSG->speakers.size()>beforeLastSG->speakers.size() && !beforeLastSG->speakersAreNeverGroupedTogether &&
				       (lastSG->povSpeakers.empty() || lastSG->povSpeakers==beforeLastSG->speakers || !intersect(lastSG->povSpeakers,beforeLastSG->speakers,allIn,oneIn)))
      {
        lastSG->previousSubsetSpeakerGroup=sg;
				if (!headerPrinted)
				{
					lplog(LOG_SG,L"SGP[2]:------ %s -------",toText(*lastSG,tmpstr));
					headerPrinted=true;
				}
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"SGP:   Subgroup #%d: %s.",sg,toText(*beforeLastSG,tmpstr2));
				lastSG->groupedSpeakers=beforeLastSG->speakers;
				lastSG->singularSpeakers.clear();
			  subtract(lastSG->speakers,lastSG->groupedSpeakers,lastSG->singularSpeakers);
        break;
      }
			if (!beforeLastSG->speakersAreNeverGroupedTogether && numFound)
			{
				possibleCurrentSubgroup|=(lastSG->speakers.size()>=3 && sg==whichSG-1 && numFound==1);
				if (!headerPrinted)
				{
					lplog(LOG_SG,L"SGP[3]:------ %s -------",toText(*lastSG,tmpstr));
					headerPrinted=true;
				}
				else if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"SGP:   left:%s",objectString(speakers,tmpstr2).c_str());
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"SGP:   %d found in #%d:%s.",numFound,sg,toText(*beforeLastSG,tmpstr));
				set <int>::iterator si;
		    // if any members found in previous speaker groups, but not all
				for (set <int>::iterator blsi=beforeLastSG->speakers.begin(),blsiEnd=beforeLastSG->speakers.end(); blsi!=blsiEnd; blsi++)
					if ((si=speakers.find(*blsi))!=speakers.end())
						speakers.erase(si);
			}
    }
		// check if current group contains subgroup
		// lastSG must contain one and only one member of the last speaker group
		// must have three or more speakers
		if (lastSG->previousSubsetSpeakerGroup<0 && possibleCurrentSubgroup)
		{
			lastSG->groupedSpeakers.clear();
			subtract(lastSG->speakers,speakerGroups[whichSG-1].speakers,lastSG->groupedSpeakers);
			if (lastSG->speakers.size()-lastSG->groupedSpeakers.size()!=1 || lastSG->groupedSpeakers.size()<2)
			{
				lastSG->groupedSpeakers.clear();
				return;
			}
			// if these speakers are never grouped in the current speaker group, reject.
			bool groupingFound=false;
			for (vector < cSpeakerGroup::cGroup >::iterator gi=lastSG->groups.begin(),giEnd=lastSG->groups.end(); gi!=giEnd && !groupingFound; gi++)
				if (intersect(lastSG->groupedSpeakers,gi->objects,allIn,oneIn) && allIn && lastSG->groupedSpeakers.size()==gi->objects.size())
					groupingFound=true;
			if (!groupingFound)
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"SGP:   Subgroup rejected [current subgroup - no group found] #%d:%s.",whichSG,objectString(lastSG->groupedSpeakers,tmpstr2).c_str());
				lastSG->groupedSpeakers.clear();
				return;
			}
			if (lastSG->povSpeakers.size()>1 && lastSG->groupedSpeakers.size()==lastSG->povSpeakers.size() && lastSG->speakers.size()<lastSG->povSpeakers.size()*2 &&
				  lastSG->povSpeakers!=lastSG->groupedSpeakers)
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"SGP:   Subgroup rejected [current subgroup - no match with povSpeakers] #%d:%s.",whichSG,objectString(lastSG->groupedSpeakers,tmpstr2).c_str());
				lastSG->groupedSpeakers=lastSG->povSpeakers;
			}
			lastSG->singularSpeakers.clear();
		  subtract(lastSG->speakers,lastSG->groupedSpeakers,lastSG->singularSpeakers);
			lastSG->previousSubsetSpeakerGroup=CURRENT_SUBSET_SG;
			if (!headerPrinted)
			{
				lplog(LOG_SG,L"SGP[4]:------ %s -------",toText(*lastSG,tmpstr));
				headerPrinted=true;
			}
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"SGP:   Subgroup is [current subgroup] #%d:%s.",whichSG,objectString(lastSG->groupedSpeakers,tmpstr2).c_str());
		}
		allIn=oneIn=false;
		lastSG->speakersAreNeverGroupedTogether=true;
		if (lastSG->conversationalQuotes>0)
			for (vector < cSpeakerGroup::cGroup >::iterator gi=lastSG->groups.begin(),giEnd=lastSG->groups.end(); gi!=giEnd && lastSG->speakersAreNeverGroupedTogether; gi++)
				if (intersect(lastSG->speakers,gi->objects,allIn,oneIn) && allIn)
					lastSG->speakersAreNeverGroupedTogether=false;
		if (lastSG->speakersAreNeverGroupedTogether)
		{
			// get common lastSpeakerGroup
			int lastSpeakerGroup=-1;
			bool notGrouped=false;
			for (set <int>::iterator si=lastSG->speakers.begin(),siEnd=lastSG->speakers.end(); si!=siEnd && !notGrouped; si++)
			{
				//int tmp=getLastSpeakerGroup(*si,whichSG);
				if (lastSpeakerGroup==-1)
					lastSpeakerGroup=getLastSpeakerGroup(*si,whichSG);
				else
					notGrouped=(getLastSpeakerGroup(*si,whichSG)!=lastSpeakerGroup);
			}
			if (lastSpeakerGroup==-1) notGrouped=true;
			if (!notGrouped)
			{
				// so in this lastSpeakerGroup, are all the speakers in groupedSpeakers?
				for (set <int>::iterator si=lastSG->speakers.begin(),siEnd=lastSG->speakers.end(); si!=siEnd && !notGrouped; si++)
					notGrouped=speakerGroups[lastSpeakerGroup].groupedSpeakers.find(*si)==speakerGroups[lastSpeakerGroup].groupedSpeakers.end();
				// OK so not in grouped speakers.  Maybe in groups?
				if (notGrouped)
				{
					for (vector < cSpeakerGroup::cGroup >::iterator gi=speakerGroups[lastSpeakerGroup].groups.begin(),giEnd=speakerGroups[lastSpeakerGroup].groups.end(); gi!=giEnd && notGrouped; gi++)
						if (intersect(lastSG->speakers,gi->objects,allIn,oneIn) && allIn)
							notGrouped=false;
					// OK so not in groups either.  Perhaps in the past (if the number of speakers match)?
					if (notGrouped && lastSG->speakers.size()==speakerGroups[lastSpeakerGroup].speakers.size() && !speakerGroups[lastSpeakerGroup].speakersAreNeverGroupedTogether)
						notGrouped=false;
				}
			}
			lastSG->speakersAreNeverGroupedTogether=notGrouped;
		}
		if (preferredMPluralGroup.size() && preferredMPluralGroup.size()!=lastSG->speakers.size() &&
			  (!intersect(preferredMPluralGroup,lastSG->groupedSpeakers,allIn,oneIn) || !allIn || preferredMPluralGroup.size()!=lastSG->groupedSpeakers.size()))
		{
			// see if preferredMPluralGroup + lastSG->groupedSpeakers have an empty intersection, and both have full intersection with speakers set.
			// if this is true, then preferredMPluralGroup is merely the flip side of groupedSpeakers and can be ignored.
			if (intersect(preferredMPluralGroup,lastSG->groupedSpeakers,allIn,oneIn) || preferredMPluralGroup.size()+lastSG->groupedSpeakers.size()<speakers.size())
			{
				if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:Subgrouping disagreement preferred %s",where,objectString(preferredMPluralGroup,tmpstr2).c_str());
				lastSG->groupedSpeakers.clear();
				lastSG->singularSpeakers.clear();
				for (int I=0; I<(signed)preferredMPluralGroup.size(); I++)
					lastSG->groupedSpeakers.insert(preferredMPluralGroup[I]);
				subtract(lastSG->speakers,lastSG->groupedSpeakers,lastSG->singularSpeakers);
			}
		}
	  if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG|LOG_RESOLUTION,L"%06d:Subgrouping resulted in %s",where,toText(*lastSG,tmpstr));
  }
}

// remove hail objects that have not been detected any other way than through hail.
void Source::eliminateSpuriousHailSpeakers(int begin,int end,cSpeakerGroup &sg,bool speakerGroupCrossesSectionBoundary)
{ LFS
	wstring tmpstr;
	// eliminate all speakers having only PISHail counts
	// remove hail objects that have not been detected any other way.  This is to minimize the effect of parsing errors.
	// how many gendered PP non-name objects are there?
	int physicallyPresentSpeakers=0,hailSpeakersToDelete=0;
  for (vector <cLocalFocus>::iterator lsi=localObjects.begin(); lsi!=localObjects.end(); lsi++)
		if (lsi->physicallyPresent && objects[lsi->om.object].isAgent(false) && objects[lsi->om.object].objectClass!=BODY_OBJECT_CLASS && objects[lsi->om.object].objectClass!=META_GROUP_OBJECT_CLASS)
		{
			//lplog(LOG_SG,L"%s",objectString(lsi->om,tmpstr,true).c_str());
			physicallyPresentSpeakers++;
		}
	for (set <int>::iterator s=sg.speakers.begin(); s!=sg.speakers.end(); s++)
	{
		vector <cLocalFocus>::iterator lsi=in(*s);
		if (objects[*s].PISHail && !objects[*s].PISDefinite && !objects[*s].PISSubject &&
			  (objects[*s].PISHail<=2 || objects[*s].numDefinitelyIdentifiedAsSpeakerInSection<=2) &&
			  (objects[*s].objectClass!=NAME_OBJECT_CLASS || objects[*s].name.hon==wNULL || objects[*s].name.justHonorific() || objects[*s].numEncountersInSection==0 || (lsi!=localObjects.end() && lsi->previousWhere<0)))
			hailSpeakersToDelete++;
	}
	// if speaker group just crossed over a boundary (like a chapter marker), then people may be hailed without introduction
	if (speakerGroupCrossesSectionBoundary && (physicallyPresentSpeakers-hailSpeakersToDelete)==0)
		return;
	for (set <int>::iterator s=sg.speakers.begin(); s!=sg.speakers.end(); )
	{
		vector <cLocalFocus>::iterator lsi=in(*s);
		if (objects[*s].PISHail && !objects[*s].PISDefinite && !objects[*s].PISSubject &&
			  (objects[*s].PISHail<=2 || objects[*s].numDefinitelyIdentifiedAsSpeakerInSection<=2) &&
			  (objects[*s].objectClass!=NAME_OBJECT_CLASS || objects[*s].name.hon==wNULL || objects[*s].name.justHonorific() || objects[*s].numEncountersInSection==0 || (lsi != localObjects.end() && lsi->previousWhere < 0)))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"%06d-%06d:%02d   hail deleted: %s (HAIL=%d,%d,%d,%d:%d,%d,%d,%d) [LW=%d,PW=%d,%s,physicallyPresentSpeakers=%d] (%s)",begin,end,section,
							objectString(*s,tmpstr,true).c_str(),objects[*s].PISHail,
				      objects[*s].numEncounters,objects[*s].numIdentifiedAsSpeaker,objects[*s].numDefinitelyIdentifiedAsSpeaker,
							objects[*s].numEncountersInSection,objects[*s].numSpokenAboutInSection,objects[*s].numIdentifiedAsSpeakerInSection,objects[*s].numDefinitelyIdentifiedAsSpeakerInSection,
							lsi->lastWhere,lsi->previousWhere,(lsi->physicallyPresent) ? L"PP":L"not PP",physicallyPresentSpeakers,(speakerGroupCrossesSectionBoundary) ? L"speakerGroupCrossesSectionBoundary":L"");
			sg.groupedSpeakers.erase(*s);
			sg.singularSpeakers.erase(*s);
			sg.povSpeakers.erase(*s);
			sg.speakers.erase(s++);
		}
		else
			s++;
	}
}

// take the intersection between the current and future speaker groups.  If the current has an unresolvable object,
// and the next speakerGroup has an object not contained in the previous speakerGroup, allow the current to be merged into the future.
int Source::detectUnresolvableObjectsResolvableThroughSpeakerGroup(void)
{ LFS
	if (speakerGroups.empty()) return false;
	set <int> futureSpeakers=tempSpeakerGroup.speakers,currentSpeakers=speakerGroups[speakerGroups.size()-1].speakers;
	for (set <int>::iterator si=currentSpeakers.begin(); si!=currentSpeakers.end(); )
		if (futureSpeakers.erase(*si))
		{
			currentSpeakers.erase(*si);
			si=currentSpeakers.begin();
		}
		else
			si++;
	if (futureSpeakers.size()>1)
	{
		// erase all objects that have ever appeared in the past, but only if all present objects have already been erased.
		for (set <int>::iterator si=futureSpeakers.begin(); si!=futureSpeakers.end(); )
			if (objects[*si].firstPhysicalManifestation>=0 && objects[*si].firstPhysicalManifestation<speakerGroups[speakerGroups.size()-1].sgBegin)
			{
				futureSpeakers.erase(*si);
				currentSpeakers.erase(*si);
				si=futureSpeakers.begin();
			}
			else
				si++;
	}
	int unresolvableObject=-1;
	for (set <int>::iterator si=currentSpeakers.begin(),siEnd=currentSpeakers.end(); si!=siEnd; si++)
		if (unResolvablePosition(objects[*si].begin) && m[objects[*si].originalLocation].objectMatches.empty())
			unresolvableObject=*si;
	if (futureSpeakers.size()==1 && unresolvableObject>=0 &&
		  objects[*futureSpeakers.begin()].objectClass!=BODY_OBJECT_CLASS &&
			objects[unresolvableObject].objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && // if the unresolvableObject is an occupation, this is always wrong or unnecessary (24 cases)
			objects[*futureSpeakers.begin()].matchGender(objects[unresolvableObject]))
	{
		wstring tmpstr,tmpstr2;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"Speaker %s resolvable by future speakerGroup object %s.",
				objectString(unresolvableObject,tmpstr,false).c_str(),objectString(*futureSpeakers.begin(),tmpstr2,false).c_str());
		return *futureSpeakers.begin();
	}
	return -1;
}

// begin - beginning new section
// end -   ending new section
// spNarrationSubjects - narration subjects all of which were unMergable with the lastSpeakerGroup
// nextNarrationSubjects - narration subjects inbetween begin and end
// tempSpeakerGroup - collection of all speakers inbetween begin and end
// endOfSection - end = end of chapter or other section
// subjectsInPreviousUnquotedSection - subject or plural subject(s) immediately before tmpSpeakerGroup

// if tempSpeakerGroup speakers<2, add narrators that came before, narrators that occurred in the section and add subjectsInPreviousUnquotedSection
// if all of tempSpeakerGroup occurred in the last speaker group, merge all and extend last speaker group.
// if all but one occurred in last speaker group and last speaker group only contained one speaker, add the one that did not occur to last speaker group and extend last speaker group
// if more than one did not occur in last speaker group or last speaker group contained more than one speaker, create a new speaker group.
bool Source::createSpeakerGroup(int begin,int end,bool endOfSection,int &lastSpeakerGroupOfPreviousSection)
{ LFS
	if (begin==end)
		return false;
  set <int> saveSpeakers=tempSpeakerGroup.speakers;
  tempSpeakerGroup.sgEnd=end;
  wstring tmpstr,tmpstr2,tmpstr3;
  bool uniquelyMergable,inserted;
  set <int>::iterator stsi;
  //int sectionBegin=(sections.size()) ? sections[section].begin:0;
  vector <cSpeakerGroup>::iterator lastSG=(speakerGroups.size()) ? speakerGroups.begin()+speakerGroups.size()-1 : speakerGroups.end();
	bool conversationOccurred=tempSpeakerGroup.speakers.size()!=0;
  for (vector <int>::iterator s=nextNarrationSubjects.begin(),sEnd=nextNarrationSubjects.end(); s!=sEnd; s++)
  {
		inserted=(unMergable(-1,*s,tempSpeakerGroup.speakers,uniquelyMergable,true,false,false,false,stsi));
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG,L"%06d-%06d:%02d   spNextNarrationSubject %s %s into %s",begin,end,section,objectString(*s,tmpstr,true).c_str(),(inserted) ? L"inserted" : L"merged",objectString(tempSpeakerGroup.speakers,tmpstr2).c_str());
  }
	if (tempSpeakerGroup.speakers.size()<2 && conversationOccurred)
	{
		int offset=0;
		for (vector <int>::iterator s=nextISNarrationSubjects.begin(),sEnd=nextISNarrationSubjects.end(); s!=sEnd; s++,offset++)
		{
			inserted=(unMergable(-1,*s,tempSpeakerGroup.speakers,uniquelyMergable,true,false,false,false,stsi));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"%06d-%06d:%02d   nextISNarrationSubjects [IS] %d:%s %s into %d:%s",begin,end,section,whereNextISNarrationSubjects[offset],objectString(*s,tmpstr,true).c_str(),(inserted) ? L"inserted" : L"merged",tempSpeakerGroup.sgBegin,objectString(tempSpeakerGroup.speakers,tmpstr2).c_str());
		}
		nextISNarrationSubjects.clear();
		whereNextISNarrationSubjects.clear();
	}
	// this does not actually include another more thorough way - is the speaker specified after the quote?
  if (tempSpeakerGroup.speakers.size()<2 && subjectsInPreviousUnquotedSection.size()>=1 && end+1<(signed)m.size() && m[end+1].word->first==L"“")
  {
		// bool BF=(tempSpeakerGroup.begin+1<m.size() && m[tempSpeakerGroup.begin+1].forms.isSet(quoteForm)); all speaker groups at this point start with an unquoted paragraph
		int whereSubjectMove=-1;
		bool allIn=false,oneIn=false;
	  intersect(subjectsInPreviousUnquotedSection,tempSpeakerGroup.speakers,allIn,oneIn);
		// if this speaker group starts with an unquoted paragraph, and the speakerGroup only consists of a single speaker, and that speaker exits or moves during that unquoted paragraph,
		//  then don't import previous speakers into this speaker group.
		if (tempSpeakerGroup.speakers.size()==1 && !allIn)
		{
			int object=*tempSpeakerGroup.speakers.begin(),wherePOV=-1;
			// is object not POV?
			for (int I=povInSpeakerGroups.size()-1; I>=0 && section<sections.size() && povInSpeakerGroups[I]>=(signed)sections[section].begin && wherePOV<0; I--)
				if (in(object,povInSpeakerGroups[I]))
					wherePOV=povInSpeakerGroups[I];
			if (wherePOV>=0)
			{
				bool exitOnly=false;
				for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end(); loc!=locEnd; loc++)
					if (loc->at>=begin && loc->at<end && m[loc->at].relVerb>=0 && isSelfMoveVerb(m[loc->at].relVerb,exitOnly) && m[loc->at].getObject()>=0 && objects[m[loc->at].getObject()].objectClass!=BODY_OBJECT_CLASS) 
					{
						vector <cSpaceRelation>::iterator sr=findSpaceRelation(loc->at);
						vector <int> lastSubjects;
						if (sr==spaceRelations.end())
						{
							detectSpaceRelation(loc->at,-1,lastSubjects);
							sr=findSpaceRelation(loc->at);
						}
						if (sr!=spaceRelations.end() && (sr->relationType==stMOVE || sr->relationType==stEXIT || sr->relationType==stENTER))
						{
							whereSubjectMove=loc->at;
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_SG,L"%06d-%06d:%02d rejected subjectsInPreviousUnquotedSection [%s subjectMoved@%d POV@%d] %s into %s",begin,end,section,
											(endOfSection) ? L"EOS":L"not EOS",whereSubjectMove,wherePOV,
											objectString(subjectsInPreviousUnquotedSection,tmpstr).c_str(),objectString(tempSpeakerGroup.speakers,tmpstr2).c_str());
							break;
						}
					}
			}
		}
		if (whereSubjectMove<0)
			for (int spusi=0; spusi<(signed)subjectsInPreviousUnquotedSection.size(); spusi++)
			{
				// if the next sentence is not fully quoted, this subject will not be used
				inserted=(unMergable(-1,subjectsInPreviousUnquotedSection[spusi],tempSpeakerGroup.speakers,uniquelyMergable,true,false,false,false,stsi));
				//vector <cLocalFocus>::iterator lsi=in(subjectsInPreviousUnquotedSection[spusi]);
				//wchar_t *physicallyPresent=(lsi!=localObjects.end() && lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent>=0) ? L"[PP]":L"[NPP]";
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d   %s subjectsInPreviousUnquotedSection %s into %s",begin,end,section,(inserted) ? L"inserted" : L"merged",
								objectString(subjectsInPreviousUnquotedSection[spusi],tmpstr,true).c_str(),objectString(tempSpeakerGroup.speakers,tmpstr2).c_str());
				if (inserted) 
				{
					objects[subjectsInPreviousUnquotedSection[spusi]].numIdentifiedAsSpeaker++;
					objects[subjectsInPreviousUnquotedSection[spusi]].numIdentifiedAsSpeakerInSection++;
					if (objects[subjectsInPreviousUnquotedSection[spusi]].numDefinitelyIdentifiedAsSpeakerInSection+objects[subjectsInPreviousUnquotedSection[spusi]].numIdentifiedAsSpeakerInSection==1 && sections.size())
						sections[section].speakerObjects.push_back(cOM(subjectsInPreviousUnquotedSection[spusi],SALIENCE_THRESHOLD));
					vector <cLocalFocus>::iterator lsi=in(subjectsInPreviousUnquotedSection[spusi]);
					if (lsi!=localObjects.end())
						lsi->numIdentifiedAsSpeaker++;
				}
			}
  }
	bool speakerGroupCrossesSectionBoundary=speakerGroups.size()>0 && lastSpeakerGroupOfPreviousSection==speakerGroups.size()-1;
	// remove hail objects that have not been detected any other way.
	eliminateSpuriousHailSpeakers(begin,end,tempSpeakerGroup,speakerGroupCrossesSectionBoundary);
  set <int>::iterator mergedSpeakerObject;
  if (speakerGroups.size())
  {
		int speakersNotMergable=0,onlyHailSpeakers=0;
		int resolvableByFutureSpeaker=detectUnresolvableObjectsResolvableThroughSpeakerGroup();
		for (set <int>::iterator s=tempSpeakerGroup.speakers.begin(),sEnd=tempSpeakerGroup.speakers.end(); s!=sEnd; s++)
		{
			if (in(*s,lastSG->replacedSpeakers)!=lastSG->replacedSpeakers.end())
				continue;
			if (unMergable(-1,*s,lastSG->speakers,uniquelyMergable,false,speakerGroupCrossesSectionBoundary,resolvableByFutureSpeaker==*s,true,mergedSpeakerObject))
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d   Speaker %s of current group unmergable with last speaker group %s.",
							begin,end,section,objectString(*s,tmpstr,true).c_str(),objectString(lastSG->speakers,tmpstr2).c_str());
				if (lastSG->speakers.size()>1 && objects[*s].PISHail>=0 && objects[*s].numEncounters==0 && objects[*s].numEncountersInSection==0)
				{
					if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d  just hail speaker don't create a new group - %s (HAIL=%d,%d,%d,%d:%d,%d,%d,%d)",begin,end,section,
							objectString(*s,tmpstr,true).c_str(),objects[*s].PISHail,
				      objects[*s].numEncounters,objects[*s].numIdentifiedAsSpeaker,objects[*s].numDefinitelyIdentifiedAsSpeaker,
							objects[*s].numEncountersInSection,objects[*s].numSpokenAboutInSection,objects[*s].numIdentifiedAsSpeakerInSection,objects[*s].numDefinitelyIdentifiedAsSpeakerInSection);
					onlyHailSpeakers++;
				}
				else
					speakersNotMergable++;
			}
			// check to see whether an object that is only mentioned with a gendered object is
			// new object must be a name, previous object must not be a name
			// new object's first mention must be after the previous one
			else if (uniquelyMergable && mergedSpeakerObject!=lastSG->speakers.end() &&
					objects[*s].objectClass==NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass!=NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass!=BODY_OBJECT_CLASS &&
					objects[*s].firstSpeakerGroup>=0 &&
					speakerGroups[objects[*s].firstSpeakerGroup].sgBegin<objects[*mergedSpeakerObject].begin)
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d   Speaker %s of current group unmergable (Unique but without new object) with last speaker group %s.",
							begin,end,section,objectString(*s,tmpstr,true).c_str(),objectString(lastSG->speakers,tmpstr2).c_str());
				speakersNotMergable++;
			}
			else
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d   Speaker %s of current group merged with %s in last speaker group %s.",
							begin,end,section,objectString(*s,tmpstr,true).c_str(),objectString(*mergedSpeakerObject,tmpstr3,false).c_str(),objectString(lastSG->speakers,tmpstr2).c_str());
		}
		if (onlyHailSpeakers>1) 
		{
			speakersNotMergable+=onlyHailSpeakers;
			onlyHailSpeakers=0;
		}
		// if lastSG is not composed of a single quoted paragraph
		bool lastSGNotClosable=false;
		if (lastSG!=speakerGroups.end() && m[lastSG->sgBegin+1].word->first==L"“")
		{
			int q=lastSG->sgBegin+1;
			while (m[q].quoteForwardLink>=0) q=m[q].quoteForwardLink;
			lastSGNotClosable=m[q].endQuote+1==lastSG->sgEnd;
		}
		if (lastSGNotClosable || !speakersNotMergable || (speakersNotMergable==1 && lastSG->speakers.size()==1))
		{
			if (debugTrace.traceSpeakerResolution)
			{
				if (lastSGNotClosable)
					lplog(LOG_SG,L"%06d-%06d:%02d   lastSG %s is a single quote.",begin,end,section,objectString(lastSG->speakers,tmpstr2).c_str());
				else if (!speakersNotMergable)
					lplog(LOG_SG,L"%06d-%06d:%02d   All of tempSpeakerGroup %s were found in %s.",begin,end,section,objectString(tempSpeakerGroup.speakers,tmpstr).c_str(),objectString(lastSG->speakers,tmpstr2).c_str());
				else
					lplog(LOG_SG,L"%06d-%06d:%02d   All but one of tempSpeakerGroup %s were found in %s.",begin,end,section,objectString(tempSpeakerGroup.speakers,tmpstr).c_str(),objectString(lastSG->speakers,tmpstr2).c_str());
			}
			bool atLeastOneMerged=true;
			while (atLeastOneMerged)
			{
				// are there any unresolvable speakers in lastSG that are resolved in tempSpeakerGroup?
				atLeastOneMerged=false;
				for (set <int>::iterator s=tempSpeakerGroup.speakers.begin(); s!=tempSpeakerGroup.speakers.end(); s++)
				{
					if (in(*s,lastSG->replacedSpeakers)!=lastSG->replacedSpeakers.end())
						continue;
					unMergable(-1,*s,lastSG->speakers,uniquelyMergable,false,speakerGroupCrossesSectionBoundary,resolvableByFutureSpeaker==*s,true,mergedSpeakerObject);
					// addNewSpeaker - possibly!
					if (uniquelyMergable && mergedSpeakerObject!=lastSG->speakers.end() &&
							// preventing occupation object stops faust from becoming a gaoler speaker
							objects[*s].objectClass==NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass!=NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
							objects[*s].matchGender(objects[*mergedSpeakerObject]))
					{
						int saveObject=*mergedSpeakerObject;
						if (objects[*s].male && objects[*s].female && !(objects[saveObject].male && objects[saveObject].female))
						{
							objects[*s].male=objects[saveObject].male;
							objects[*s].female=objects[saveObject].female;
					    if (debugTrace.traceSpeakerResolution)
								lplog(LOG_SG|LOG_RESOLUTION,L"%06d-%06d:%02d %s narrowed to %s.",begin,end,section,objectString(*s,tmpstr,true).c_str(),(objects[*s].male) ? L"male":L"female");
						}
						tempSpeakerGroup.speakers.erase(saveObject); // must use *mergedSpeakerObject before it is replaced
						replaceSpeaker(lastSG->sgBegin,end,saveObject,*s);
						// if the replaced object is 'man's voice' then replace man as well as man's voice
						if (objects[saveObject].objectClass==BODY_OBJECT_CLASS && objects[saveObject].ownerWhere>=0 && m[objects[saveObject].ownerWhere].getObject()>=0)
							replaceSpeaker(lastSG->sgBegin,end,m[objects[saveObject].ownerWhere].getObject(),*s);
						speakerAges[*s]=0;
						atLeastOneMerged=true;
						s=tempSpeakerGroup.speakers.begin();
						continue;
					}
					else if (mergedSpeakerObject==lastSG->speakers.end())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_SG,L"%06d-%06d:%02d %s inserted -> %s",begin,end,section,objectString(*s,tmpstr,true).c_str(),toText(*lastSG,tmpstr2));
						lastSG->speakers.insert(*s);
						speakerAges.insert(tSA(*s,0));
					}
					else
						speakerAges[*s]=0;
					// if a metagroup object is encountered, also zero all other meta-group objects
					// because it is not certain whether this metagroup object matches another until later processing
					// and if this is not done then metaGroup objects will be aged out of the speaker group.
					if (objects[*s].objectClass==META_GROUP_OBJECT_CLASS)
					{
						for (set <int>::iterator ls=lastSG->speakers.begin(),lsEnd=lastSG->speakers.end(); ls!=lsEnd; ls++)
							if (objects[*ls].objectClass==META_GROUP_OBJECT_CLASS)
								speakerAges[*ls]=0;
					}
				}
			}
			lastSG->groups.insert(lastSG->groups.end(),tempSpeakerGroup.groups.begin(),tempSpeakerGroup.groups.end());
			for (vector < cSpeakerGroup::cGroup >::iterator gi=tempSpeakerGroup.groups.begin(),giEnd=tempSpeakerGroup.groups.end(); gi!=giEnd; gi++)
			{
				bool allIn,oneIn;
				// if group only contains speakers
				// if there is no conversation in the speakerGroup (conversationalQuotes>0), there is much less of a chance that the entities should be considered a "group"
				// They[streets,boris,tommy] reached at length[length] a small dilapidated square .
				if (intersect(gi->objects,lastSG->speakers,allIn,oneIn) && allIn && lastSG->conversationalQuotes>0)
				{
					for (vector <int>::iterator si=gi->objects.begin(),siEnd=gi->objects.end(); si!=siEnd; si++)
						lastSG->groupedSpeakers.insert(*si);
				  if (debugTrace.traceSpeakerResolution)
					  lplog(LOG_SG,L"%06d-%06d:%02d   groupSpeakers %s added (1)",begin,end,section,objectString(*gi,tmpstr).c_str());
					break;
				}
			}
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"%06d-%06d:%02d   PIS speakerGroup %s extended to %d%s",lastSG->sgBegin,lastSG->sgEnd,section,toText(*lastSG,tmpstr),end,(endOfSection) ? L" end of section" : L"");
			lastSG->sgEnd=end;
			if (debugTrace.traceSpeakerResolution)
				for (set <int>::iterator s=lastSG->speakers.begin(); s!=lastSG->speakers.end(); s++)
					lplog(LOG_SG,L"%06d-%06d:%02d %s speakerAge=%d.",lastSG->sgBegin,lastSG->sgEnd,section,objectString(*s,tmpstr,true).c_str(),speakerAges[*s]);
			tempSpeakerGroup.speakers.clear();
			tempSpeakerGroup.groupedSpeakers.clear();
			tempSpeakerGroup.singularSpeakers.clear();
			tempSpeakerGroup.povSpeakers.clear();
			tempSpeakerGroup.metaNameOthers.clear();
			tempSpeakerGroup.sgBegin=end;
			tempSpeakerGroup.sgEnd=end;
			tempSpeakerGroup.groups.clear();
			lastSG->conversationalQuotes+=tempSpeakerGroup.conversationalQuotes;
			tempSpeakerGroup.conversationalQuotes=0;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"%06d-%06d:%02d insert nextISNarrationSubjects %s into lastISNarrationSubjects %s.",begin,end,section,objectString(nextISNarrationSubjects,tmpstr).c_str(),objectString(lastISNarrationSubjects,tmpstr).c_str());
			lastISNarrationSubjects.insert(lastISNarrationSubjects.end(),nextISNarrationSubjects.begin(),nextISNarrationSubjects.end());
			whereLastISNarrationSubjects.insert(whereLastISNarrationSubjects.end(),whereNextISNarrationSubjects.begin(),whereNextISNarrationSubjects.end());
			// if there has been a conversation, but there is no audience
			if (lastSG->speakers.size()==1 && lastSG->conversationalQuotes)
			{
				int offset=0;
				for (vector <int>::iterator s=lastISNarrationSubjects.begin(),sEnd=lastISNarrationSubjects.end(); s!=sEnd; s++,offset++)
				{
					inserted=(unMergable(-1,*s,lastSG->speakers,uniquelyMergable,true,false,false,false,stsi));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG,L"%06d-%06d:%02d   lastISNarrationSubjects [IS] %d:%s %s into %d:%s",begin,end,section,whereLastISNarrationSubjects[offset],objectString(*s,tmpstr,true).c_str(),(inserted) ? L"inserted" : L"merged",tempSpeakerGroup.sgBegin,objectString(tempSpeakerGroup.speakers,tmpstr2).c_str());
				}
				lastISNarrationSubjects.clear();
				whereLastISNarrationSubjects.clear();
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d-%06d:%02d cleared lastISNarrationSubjects.",begin,end,section);
			}
			if (endOfSection)
			{
				lastSpeakerGroupOfPreviousSection=speakerGroups.size()-1;
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"set lastSpeakerGroupOfPreviousSection=%d.",lastSpeakerGroupOfPreviousSection);
			}
			return true;
		}
	}
	else
		tempSpeakerGroup.sgBegin=0;
  if (tempSpeakerGroup.speakers.size() || endOfSection)
  {
	  lastSG=(speakerGroups.size()) ? speakerGroups.begin()+speakerGroups.size()-1 : speakerGroups.end();
		if (speakerGroups.size())
			eliminateSpuriousHailSpeakers(begin,end,*lastSG,speakerGroupCrossesSectionBoundary);
		determineSpeakerRemoval(begin);
    tempSpeakerGroup.section=section;
	  lastSG=(speakerGroups.size()) ? speakerGroups.begin()+speakerGroups.size()-1 : speakerGroups.end();
		determinePreviousSubgroup(end,speakerGroups.size()-1,&speakerGroups[speakerGroups.size()-1]);
    if (debugTrace.traceSpeakerResolution)
    {
      if (speakerGroups.size())
        lplog(LOG_SG,L"%06d-%06d:%02d**** speakerGroup %s CLOSED%s",lastSG->sgBegin,lastSG->sgEnd,section,toText(*lastSG,tmpstr),(endOfSection) ? L" end of section" : L"");
      lplog(LOG_SG,L"%06d-%06d:%02d   %s added%s",begin,end,section,toText(tempSpeakerGroup,tmpstr),(endOfSection) ? L" end of section" : L"");
    }
		determineSubgroupFromGroups(tempSpeakerGroup);
    speakerGroups.push_back(tempSpeakerGroup);
    currentSpeakerGroup=speakerGroups.size();
		speakerAges.clear();
    for (set <int>::iterator s=tempSpeakerGroup.speakers.begin(),sEnd=tempSpeakerGroup.speakers.end(); s!=sEnd; s++)
    {
      if (objects[*s].firstSpeakerGroup==-1)
        objects[*s].firstSpeakerGroup=currentSpeakerGroup-1;
      objects[*s].lastSpeakerGroup=currentSpeakerGroup-1;
      objects[*s].ageSinceLastSpeakerGroup=0;
			speakerAges.insert(tSA(*s,0));
    }
    tempSpeakerGroup.speakers.clear();
    tempSpeakerGroup.sgBegin=end;
    tempSpeakerGroup.sgEnd=end;
		tempSpeakerGroup.groups.clear();
		tempSpeakerGroup.metaNameOthers.clear();
		tempSpeakerGroup.groupedSpeakers.clear();
		tempSpeakerGroup.conversationalQuotes=0;
		speakerSections.clear();
		speakerSections.push_back(begin);
		lastISNarrationSubjects=nextISNarrationSubjects;
		whereLastISNarrationSubjects=whereNextISNarrationSubjects;
		introducedByReference.clear();
	  if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG,L"lastISNarrationSubjects=nextISNarrationSubjects=%s.",objectString(nextISNarrationSubjects,tmpstr).c_str());
		if (endOfSection)
		{
      lastSpeakerGroupOfPreviousSection=speakerGroups.size()-1;
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"set lastSpeakerGroupOfPreviousSection=%d.",lastSpeakerGroupOfPreviousSection);
		}
    return true;
  }
  tempSpeakerGroup.speakers=saveSpeakers;
  return false;
}

wchar_t *metaResponse[]={ L"reply",L"response", L"answer", NULL }; // initialized

bool Source::setPOVStatus(int where,bool inPrimaryQuote,bool inSecondaryQuote)
{ LFS
	wstring tmpstr;
	if (m[where].flags&WordMatch::flagAdjectivalObject)
	{
		int I=where;
		while (I>=0 && m[I].principalWherePosition<0) I--;
		if (m[I].principalWherePosition>where)
			where=m[I].principalWherePosition;
	}
	// To have boasted that she[tuppence] knew a lot might have raised doubts in his[mr] mind[mr] . (tuppence is POV,but not mr)
	if (m[where].getObject()>=0 && objects[m[where].getObject()].ownerWhere>=0 && isInternalBodyPart(where) &&
		  (!(m[where].objectRole&(OBJECT_ROLE|PREP_OBJECT_ROLE)) || !(m[where].flags&WordMatch::flagInPStatement)))
	{
		povInSpeakerGroups.push_back(objects[m[where].getObject()].ownerWhere);
		m[where].objectRole|=POV_OBJECT_ROLE; // used in determining point of view/observer status for speakerGroups
		m[objects[m[where].getObject()].ownerWhere].objectRole|=POV_OBJECT_ROLE;
    if (debugTrace.traceSpeakerResolution)
	    lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   speaker %s has pov status [1].",where,section,objectString(m[objects[m[where].getObject()].ownerWhere].getObject(),tmpstr,true).c_str());
		return true;
	}
	if (!inPrimaryQuote && !inSecondaryQuote && m[where].getObject()>=0 && objects[m[where].getObject()].ownerWhere<0 && m[where].getRelObject()>=0 && 
		  m[m[where].getRelObject()].relNextObject<0 &&   // rules out Jules gave Tuppence a feeling of support (Tuppence had the feeling)
		  m[m[where].getRelObject()].getObject()>=0 && objects[m[m[where].getRelObject()].getObject()].ownerWhere<0 &&
			isInternalBodyPart(m[where].getRelObject()))
	{
		povInSpeakerGroups.push_back(where);
		m[where].objectRole|=POV_OBJECT_ROLE; // used in determining point of view/observer status for speakerGroups
    if (debugTrace.traceSpeakerResolution)
	    lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   speaker %s has pov status [SUBJECT of internal object].",where,section,objectString(m[where].getObject(),tmpstr,true).c_str());
		return true;
	}
	// he was conscious
	if (!inPrimaryQuote && !inSecondaryQuote && m[where].getObject()>=0 && objects[m[where].getObject()].ownerWhere<0 && m[where].getRelObject()<0 && 
		  ((m[where].objectRole&(SUBJECT_ROLE|IS_OBJECT_ROLE|PREP_OBJECT_ROLE))==(SUBJECT_ROLE|IS_OBJECT_ROLE)) && m[where].relVerb>=0 && m[where].relVerb+1<(signed)m.size() &&
		  isInternalDescription(m[where].relVerb+1))
	{
		povInSpeakerGroups.push_back(where);
		m[where].objectRole|=POV_OBJECT_ROLE; // used in determining point of view/observer status for speakerGroups
    if (debugTrace.traceSpeakerResolution)
	    lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   speaker %s has pov status [SUBJECT of internal description].",where,section,objectString(m[where].getObject(),tmpstr,true).c_str());
		return true;
	}
	return false;
}

bool Source::notPhysicallyPresentByMissive(int where)
{ LFS
	if (m[where].relVerb>=0 && m[m[where].relVerb].relPrep>=0 && m[m[m[where].relVerb].relPrep].word->first==L"in" && 
		  m[m[m[where].relVerb].relPrep].getRelObject()>=0 && m[m[m[m[where].relVerb].relPrep].getRelObject()].getObject()>=0)
	{
		int wpo=m[m[m[where].relVerb].relPrep].getRelObject(),prepObject=m[wpo].getObject();
		bool isPlace=false,isLetterWord=false,isPhysical=false,isGendered=false,isTime=false;
		for (int I=0; I<(signed)m[wpo].objectMatches.size(); I++)
		{
			isPlace|=objects[m[wpo].objectMatches[I].object].getSubType()>=0;
			int lastLetterBegin=-1,wmo=objects[m[wpo].objectMatches[I].object].originalLocation;
			isLetterWord|=detectLetterAsObject(wmo,lastLetterBegin) || m[wmo].word->first==L"appeal";
			for (int p=0; metaResponse[p]; p++)
				isLetterWord|=(m[wmo].word->first==metaResponse[p]);
			isPhysical|=(m[wmo].word->second.flags&tFI::physicalObjectByWN)!=0;
			isGendered|=objects[m[wpo].objectMatches[I].object].male | objects[m[wpo].objectMatches[I].object].female;
			isTime|=(m[wmo].word->second.timeFlags&T_UNIT)!=0;
		}
		if (m[wpo].objectMatches.empty())
		{
			isPlace|=objects[prepObject].getSubType()>=0;
			int lastLetterBegin=-1;
			isLetterWord|=detectLetterAsObject(wpo,lastLetterBegin) || m[wpo].word->first==L"appeal";
			for (int p=0; metaResponse[p]; p++)
				isLetterWord|=(m[wpo].word->first==metaResponse[p]);
			isPhysical|=(m[wpo].word->second.flags&tFI::physicalObjectByWN)!=0;
			isGendered|=objects[prepObject].male | objects[prepObject].female;
			isTime|=(m[wpo].word->second.timeFlags&T_UNIT)!=0;
		}
		if (isLetterWord && !isPlace && !isTime && !isGendered)
		{
			wstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s not physically present (in non-physical object/letter %d:%s).",where,section,whereString(where,tmpstr,true).c_str(),wpo,whereString(wpo,tmpstr,true).c_str());
			return true;
		}
	}
	return false;
}

// in secondary quotes, inPrimaryQuote=false
bool Source::isFocus(int where,bool inPrimaryQuote,bool inSecondaryQuote,int o,bool &isNotPhysicallyPresent,bool subjectAllowPrep)
{ LFS
	if (o<0 || objects[o].isKindOf) return false;
	wstring tmpstr,tmpstr2;
	int objectClass=objects[o].objectClass;
	//vector <cLocalFocus>::iterator lsi=in(o);
	// he[tommy] rattled off the formula to the elderly woman , looking more like a housekeeper than a servant , who opened the door to him[tommy]
	//if (lsi!=localObjects.end() && lsi->notSpeaker) return false; must not restrict in this way because of the above sentence.
	// if preposition is 'to', then discard PREP_OBJECT_ROLE
	// this leads to correct results for such examples as 'to the elderly woman' but avoids 'request for Mr. Carter'
	unsigned __int64 objectRole=m[where].objectRole;
	tIWMM before=(m[where].beginObjectPosition>0) ? m[m[where].beginObjectPosition-1].word : wNULL;
	if ((objectRole&PREP_OBJECT_ROLE) && 
		  (before!=wNULL && (before->first==L"to" || before->first==L"over")) && 
		  m[where].principalWhereAdjectivalPosition<0 && 
			// prevents "Whittington directed the driver to go to Waterloo"
			(objectClass!=NAME_OBJECT_CLASS || objects[o].numIdentifiedAsSpeaker) &&
			// prevents " She[tuppence] went straight back to the Ritz and wrote a few brief words to mr . Carter"
			!(objectRole&DELAYED_RECEIVER_ROLE) && 
			// She[tuppence] remembered that[table] this was one of the men[guest] Tommy was shadowing 
			!(objectRole&RE_OBJECT_ROLE) && 
			(objectRole&FOCUS_EVALUATED))
	{
		objectRole&=~PREP_OBJECT_ROLE;
		objectRole|=SUBJECT_ROLE;
	}
	//int tmp3=m[where].principalWhereAdjectivalPosition;
	//int tmp4=objects[o].at;
	// the first, his first etc.
	if (objectClass==META_GROUP_OBJECT_CLASS && cObject::whichOrderWord(m[objects[o].originalLocation].word)!=-1 && !inPrimaryQuote && !(objectRole&SENTENCE_IN_REL_ROLE))
	{
		if ((isNotPhysicallyPresent=(objectRole&NONPAST_OBJECT_ROLE)!=0) && debugTrace.traceSpeakerResolution)
			lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s accepted for focus although OUTSIDE_QUOTE_NONPAST.",where,section,objectString(o,tmpstr,true).c_str());	
		return (m[where].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE)!=0;
	}
	bool acceptableMetaGroupObject=
		objectClass==META_GROUP_OBJECT_CLASS && !objects[o].neuter &&
		((cObject::whichOrderWord(m[where].word)==-1 || 
		 (objectRole&(IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))!=IS_OBJECT_ROLE && 
		  (objects[o].ownerWhere!=-1 || m[m[where].beginObjectPosition].word->first==L"the")));
	 // exclude "the memory of Aunt Jane" and "picture of Jane Finn"
	 // but include META_GROUP objects because they are not resolved till later, and most of the time they will be resolved and disappear
	bool allowablePrepObject=subjectAllowPrep &&
		     (objectRole&PREP_OBJECT_ROLE) && m[where].principalWhereAdjectivalPosition<0 && 
				  before!=wNULL && before->first!=L"of" &&
				 (objects[o].firstLocation<where || objectClass==META_GROUP_OBJECT_CLASS);
	int beginObjectPosition=(o==m[where].getObject() && objects[o].begin>=m[where].beginObjectPosition) ? m[where].beginObjectPosition : objects[o].begin;
	vector <cLocalFocus>::iterator lsi;
	bool isExit=false;
	if (m[where].spaceRelation) 
	{
		vector <cSpaceRelation>::iterator location = findSpaceRelation(where);
		if (location!=spaceRelations.end() && location->where==where && (isExit=location->relationType==stEXIT) && location->genderedLocationRelation && (location+1)->whereSubject==location->whereSubject)
			isExit=false;
	}
	if (((inPrimaryQuote && (objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE)) || (!inPrimaryQuote && !inSecondaryQuote)) && 
		// exclude "the memory of Aunt Jane"
		 (((objectRole&SUBJECT_ROLE) && !(objectRole&PREP_OBJECT_ROLE)) || 
	  // exclude Tommy's ring
		  ((objectRole&OBJECT_ROLE) && m[where].principalWhereAdjectivalPosition<0 && 
			 (!(objectRole&PREP_OBJECT_ROLE) || (before==wNULL || before->first!=L"of") || ((lsi=in(o))!=localObjects.end() && lsi->physicallyPresent))) || 
		  allowablePrepObject) &&
    ((objectClass==NAME_OBJECT_CLASS && (m[beginObjectPosition].queryWinnerForm(determinerForm)<0 || objects[o].numIdentifiedAsSpeaker>0)) ||
     objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
     objectClass==BODY_OBJECT_CLASS ||
     objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
     objectClass==GENDERED_DEMONYM_OBJECT_CLASS || 
		 objectClass==GENDERED_RELATIVE_OBJECT_CLASS || acceptableMetaGroupObject) &&
		// WHEN EXIT Tommy set forth on the trail of the two men[knocks] , it[trail] took all[tommy] Tuppence's self - command to refrain from accompanying him[tommy] .
		(!isExit))
	{
		if (m[where].flags&WordMatch::flagAdjectivalObject)
		{
			int I;
			for (I=where+1; (m[I].beginObjectPosition<0 || (m[I].flags&WordMatch::flagAdjectivalObject)) && I<(signed)m.size(); I++); 
			if (I==m.size() || !(m[I].objectRole&SUBJECT_ROLE) || m[I].getObject()<0 || objects[m[I].getObject()].objectClass!=BODY_OBJECT_CLASS)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s rejected - adjective",where,section,objectString(o,tmpstr,true).c_str());
				return false;
			}
		}
		// there came the accents of Number One
		if ((objectRole&(SUBJECT_ROLE|PREP_OBJECT_ROLE|OBJECT_ROLE))==OBJECT_ROLE && 
			  (m[where].relSubject<0 || m[m[where].relSubject].getObject()<0 || objects[m[m[where].relSubject].getObject()].neuter) &&
				m[where].relVerb>=0 && m[m[where].relVerb].word->first==L"came")
		{
	    if (debugTrace.traceSpeakerResolution)
		    lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s accepted through special 'came' object",where,section,objectString(o,tmpstr,true).c_str());
			objectRole|=SUBJECT_ROLE;
		}
		if ((!(objectRole&SUBJECT_ROLE) || (objectRole&PREP_OBJECT_ROLE)) && !objects[o].numIdentifiedAsSpeakerInSection && objects[o].objectClass!=META_GROUP_OBJECT_CLASS) 
		{
	   if (debugTrace.traceSpeakerResolution)
		    lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s rejected for focus (not a prior speaker)",where,section,objectString(o,tmpstr,true).c_str());
			return false;
		}
		if ((objectRole&RE_OBJECT_ROLE) && objects[o].whereRelativeClause<0)
		{
	    if (debugTrace.traceSpeakerResolution)
		    lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s rejected for focus (repeat object)",where,section,objectString(o,tmpstr,true).c_str());
			return false;
		}
		if ((m[where].flags&WordMatch::flagInQuestion)!=0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s rejected for focus (question).",where,section,objectString(o,tmpstr,true).c_str());
			return false;
		}
		if ((m[where].flags&WordMatch::flagInPStatement)!=0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s rejected for focus (probability statement).",where,section,objectString(o,tmpstr,true).c_str());
			return false;
		}
		if (!(objectRole&(FOCUS_EVALUATED|PRIMARY_SPEAKER_ROLE)))
		{
	    if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s ROLES=%s rejected for focus (not evaluated).",where,section,objectString(o,tmpstr,true).c_str(),m[where].roleString(tmpstr2).c_str());
			return false;
		}
		if (isNotPhysicallyPresent=(objectRole&NOT_OBJECT_ROLE)!=0)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s accepted for focus although NOT_ROLE.",where,section,objectString(o,tmpstr,true).c_str());
			return !objects[o].plural;
		}
		if (isNotPhysicallyPresent=(!inPrimaryQuote && !inSecondaryQuote && (objectRole&NONPAST_OBJECT_ROLE)!=0))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s accepted for focus although OUTSIDE_QUOTE_NONPAST (%s).",where,section,objectString(o,tmpstr,true).c_str(),m[where].roleString(tmpstr2).c_str());
			return !objects[o].plural;
		}
		if (!(objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE) && (isNotPhysicallyPresent=(inPrimaryQuote && (objectRole&NONPRESENT_OBJECT_ROLE)!=0)))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s accepted for focus although IN_QUOTE_NONPRESENT.",where,section,objectString(o,tmpstr,true).c_str());
			return !objects[o].plural;
		}
		if (inPrimaryQuote && (objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE) && (isNotPhysicallyPresent=(inPrimaryQuote && (objectRole&NONPAST_OBJECT_ROLE)!=0)))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s accepted for focus although IN_QUOTE_NONPAST (inside story).",where,section,objectString(o,tmpstr,true).c_str());
			return !objects[o].plural;
		}
		if (isNotPhysicallyPresent=(objectRole&SENTENCE_IN_REL_ROLE)!=0) // 
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s accepted for focus although SENTENCE_IN_REL.",where,section,objectString(o,tmpstr,true).c_str());
			return !objects[o].plural;
		}
		if (!objects[o].plural && (objectRole&POV_OBJECT_ROLE) && !(objectRole&EXTENDED_OBJECT_ROLE) && !(m[where].flags&WordMatch::flagInQuestion) && m[where].objectMatches.size()<=1)
		{
			povInSpeakerGroups.push_back(where);
			if (debugTrace.traceSpeakerResolution)
		    lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   speaker %s has pov status [2].",where,section,objectString(o,tmpstr,true).c_str());
		}
		if (m[objects[o].begin].word->first==L"each")
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s rejected for focus (each).",where,section,objectString(o,tmpstr,true).c_str());
			return false;
		}
		// in 'the answer', in 'the letter' in 'the missive' - assertion of place that is not physical
		// in it he pointed out that the Young Adventurers...
		if (notPhysicallyPresentByMissive(where))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d   object %s rejected for focus (in non-physical object/letter).",where,section,objectString(o,tmpstr,true).c_str());
			return false;
		}	  
		if (objects[o].firstPhysicalManifestation==-1)
			objects[o].firstPhysicalManifestation=where;
		// help in determining POV
		// is subject the first physically present speaker?  if there is no other POV since the beginning of the section, make this subject POV.
		if (!speakerGroupsEstablished && (povInSpeakerGroups.empty() || (section<sections.size() && povInSpeakerGroups[povInSpeakerGroups.size()-1]<(signed)sections[section].endHeader)) && 
				m[where].getObject()>=0 && !objects[m[where].getObject()].plural && objects[m[where].getObject()].objectClass!=PRONOUN_OBJECT_CLASS && m[where].objectMatches.size()<=1 &&
				// not in a compound object
				m[where].nextCompoundPartObject<0 && m[where].previousCompoundPartObject<0)
		{
			if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:First present in section - %s is POV?",where,whereString(where,tmpstr,true).c_str());
		}
		return !objects[o].plural;
	}
	return false;
}

// in secondary quotes, inPrimaryQuote=false
bool Source::mergeFocus(bool inPrimaryQuote,bool inSecondaryQuote,int o,int where,vector <int> &lastSubjects,bool &clearBeforeSet)
{ LFS
  // if subject or (object of preposition "to", if mentioned previously)
	bool isNotPhysicallyPresent=false,anySubject=false;
  if (isFocus(where,inPrimaryQuote,inSecondaryQuote,o,isNotPhysicallyPresent,false) && 
		  find(introducedByReference.begin(),introducedByReference.end(),o)==introducedByReference.end() &&
			(!objects[o].neuter || objects[o].male || objects[o].female))
  {
	  if ((m[where].objectRole&SUBJECT_ROLE) && !(m[where].objectRole&NO_ALT_RES_SPEAKER_ROLE))
			anySubject=true;
		if (isNotPhysicallyPresent) return anySubject;
    bool uniquelyMergable;
    set <int>::iterator stsi;
    if (unMergable(where,o,tempSpeakerGroup.speakers,uniquelyMergable,false,false,false,false,stsi))
		{
	    wstring tmpstr,tmpstr2,tmpstr3;
		  vector <int>::iterator vtsi;
			if ((m[where].objectRole&(IS_OBJECT_ROLE|SUBJECT_ROLE))==(IS_OBJECT_ROLE|SUBJECT_ROLE) && objects[o].getSubType()>=0 && m[where].getRelObject()>=0 && 
				  m[m[where].getRelObject()].getObject()>=0 && objects[m[m[where].getRelObject()].getObject()].getSubType()>=0)
			{
				objects[o].male=objects[o].female=false;
				objects[o].neuter=true;
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d:%02d   subject %d:%s%s rejected (IS_OBJECT and PLACE)",where,section,where,objectString(o,tmpstr,true).c_str(),m[where].roleString(tmpstr3).c_str());
				return false;
			}
			else if ((m[where].objectRole&IS_OBJECT_ROLE) && objects[o].firstPhysicalManifestation>=0 && objects[o].firstPhysicalManifestation<where && 
				  speakerGroups.size()>0 && speakerGroups[speakerGroups.size()-1].speakers.find(o)==speakerGroups[speakerGroups.size()-1].speakers.end())
			{
	      bool inserted=(unMergable(where,o,nextISNarrationSubjects,uniquelyMergable,true,false,false,false,vtsi));
				if (inserted) whereNextISNarrationSubjects.push_back(where);
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d:%02d   subject %d:%s %s %s into ZXZ nextISNarrationSubjects %s%s",where,section,where,objectString(o,tmpstr,true).c_str(),m[where].roleString(tmpstr3).c_str(),
							(inserted) ? L"inserted" : L"merged",(inserted) ? objectString(nextISNarrationSubjects,tmpstr2).c_str() : objectString(*vtsi,tmpstr2,true).c_str(),(clearBeforeSet) ? L" ZXZ cleared lastSubjects beforehand":L"");
			}
			else
			{
				// if ambiguously matched, is not plural and at least one of the matches is physically present AND if this one is NOT physically present, then reject.
				vector <cLocalFocus>::iterator lsi;
				bool atLeastOnePhysicallyPresent=false,isPhysicallyPresent=(lsi=in(o))!=localObjects.end() && lsi->physicallyPresent;
				if (m[where].objectMatches.size()>1 && (m[where].word->second.inflectionFlags&PLURAL)!=PLURAL && !isPhysicallyPresent)
				{
					for (int omi=0; omi<(signed)m[where].objectMatches.size() && !atLeastOnePhysicallyPresent; omi++)
					{
						lsi=in(m[where].objectMatches[omi].object);
						atLeastOnePhysicallyPresent=(lsi!=localObjects.end() && lsi->physicallyPresent);
					}
				}	  
				// They[words] were uttered by Boris and they[words] were : “QS Mr . Brown . ” (Mr. Brown) should not be a narrative subject.
				if ((isPhysicallyPresent || !atLeastOnePhysicallyPresent) && !(where && (m[where-1].flags&WordMatch::flagQuotedString)))
				{
					bool inserted=(unMergable(where,o,nextNarrationSubjects,uniquelyMergable,true,false,false,false,vtsi));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG,L"%06d:%02d   subject %s %s %s into ZXZ nextNarrationSubjects %s%s",where,section,objectString(o,tmpstr,true).c_str(),m[where].roleString(tmpstr3).c_str(),
								(inserted) ? L"inserted" : L"merged",(inserted) ? objectString(nextNarrationSubjects,tmpstr2).c_str() : objectString(*vtsi,tmpstr2,true).c_str(),(clearBeforeSet) ? L" ZXZ cleared lastSubjects beforehand":L"");
				}
				else
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_SG,L"%06d:%02d   subject %s %s REJECTED (not PP) from ZXZ nextNarrationSubjects %s",where,section,objectString(o,tmpstr,true).c_str(),m[where].roleString(tmpstr3).c_str(),
								objectString(nextNarrationSubjects,tmpstr2).c_str());
			}
		}
		// A plane swept over Ann.
		// A table fell onto him.
		if (!(m[where].objectRole&SUBJECT_ROLE) && lastSubjects.size()) return anySubject;
    objects[o].PISSubject++;
    if (clearBeforeSet)
		{
      lastSubjects.clear();
			clearBeforeSet=false;
		}
    lastSubjects.push_back(o);
  }
	return anySubject;
}

void Source::mergeObjectIntoSpeakerGroup(int where,int speakerObject)
{ LFS
  wstring tmpstr,tmpstr2;
  set <int>::iterator mergedSpeakerObject;
  bool uniquelyMergable;
	if (objects[speakerObject].objectClass==META_GROUP_OBJECT_CLASS && cObject::whichOrderWord(m[objects[speakerObject].originalLocation].word)!=-1)
		return;
  if (objects[speakerObject].objectClass==NAME_OBJECT_CLASS ||
      objects[speakerObject].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
      objects[speakerObject].objectClass==BODY_OBJECT_CLASS ||
      objects[speakerObject].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
      objects[speakerObject].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
      objects[speakerObject].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
      objects[speakerObject].objectClass==META_GROUP_OBJECT_CLASS)
  {
    if (section<sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(speakerObject);
    if (unMergable(where,speakerObject,tempSpeakerGroup.speakers,uniquelyMergable,true,false,false,false,mergedSpeakerObject))
    {
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_SG,L"%06d:%02d     definite %s (1)",where,section,objectString(speakerObject,tmpstr,true).c_str());//,(isSpeakerObjectNonResolvable) ? "nonResolvable":"");
    }
    else
    {
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_SG,L"%06d:%02d     definite %s %smergable with %s",where,section,objectString(speakerObject,tmpstr,true).c_str(),(uniquelyMergable) ? L"uniquely ":L"",objectString(*mergedSpeakerObject,tmpstr2,true).c_str());
      if (uniquelyMergable && objects[speakerObject].objectClass==NAME_OBJECT_CLASS && objects[*mergedSpeakerObject].objectClass==PRONOUN_OBJECT_CLASS)
      {
        tempSpeakerGroup.speakers.erase(mergedSpeakerObject);
        tempSpeakerGroup.speakers.insert(speakerObject);
      }
    }
  }
  // the definite pronoun represents something that has not been seen before (a speaker referred to by a pronoun that has not been previously introduced)
  else if (objects[speakerObject].objectClass==PRONOUN_OBJECT_CLASS)
  {
    if (!mergableBySex(speakerObject,tempSpeakerGroup.speakers,uniquelyMergable,mergedSpeakerObject) &&
        (!speakerGroups.size() || speakerGroups[speakerGroups.size()-1].section!=section ||
         !mergableBySex(speakerObject,speakerGroups[speakerGroups.size()-1].speakers,uniquelyMergable,mergedSpeakerObject)))
    {
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_SG,L"%06d:%02d     definite %s (2)",where,section,objectString(speakerObject,tmpstr,true).c_str());
      if (section<sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(speakerObject);
      tempSpeakerGroup.speakers.insert(speakerObject);
    }
    else if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG,L"%06d:%02d     definite %s %smergable with %s (2)",where,section,objectString(speakerObject,tmpstr,true).c_str(),(uniquelyMergable) ? L"uniquely ":L"",(mergedSpeakerObject==sNULL) ? L"" : objectString(*mergedSpeakerObject,tmpstr2,true).c_str());
  }
  else if (section<sections.size() && !unMergable(where,speakerObject,sections[section].preIdentifiedSpeakerObjects,uniquelyMergable,false,false,false,false,mergedSpeakerObject))
  {
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_SG,L"%06d:%02d     definite %s unMergable discovered from predefined",where,section,objectString(*mergedSpeakerObject,tmpstr,true).c_str());
    tempSpeakerGroup.speakers.insert(*mergedSpeakerObject);
  }
  else if (debugTrace.traceSpeakerResolution)
    lplog(LOG_SG,L"%06d:%02d     definite %s rejected (wrong class)",where,section,objectString(speakerObject,tmpstr,true).c_str());
}

// if this is punctuation, but it is actually matched by a pattern, then
//    the punctuation is really not the end of a sentence. (abbreviation)
// also if there is : followed by an paragraph end or a dash or period followed by a quote.
bool Source::isEOS(int where)
{ LFS
  vector <WordMatch>::iterator im=m.begin()+where;
  return (im->word->first==L"?" || im->word->first==L"!" || im->word->first==L";" || (im->word->first==L"." && !im->PEMACount) ||
					(im->word->first==L":" && (im+1)->word==Words.sectionWord) ||
           (where+1<(signed)m.size() && (im->queryForm(dashForm)>=0 || im->word->first==L"--"/*BUG*/ || im->word->first==L".") &&
            (m[where+1].word->first==L"”" || m[where+1].word->first==L"’")));
}

bool Source::isPleonastic(tIWMM w)
{ LFS
  return /*w->first==L"it" || */w->first==L"what" || w->first==L"where" || w->first==L"there" || w->first==L"here"; // || w->first==L"that"; // 'this and 'that' refers to something immediately before, and so are resolvable. || subjectWord->first==L"this" || subjectWord->first==L"that"))
}

// the following has two subchains: (Siebel and his flowers), (Faust and Mephistopheles)
// Marguerite with her box of jewels, the church scene, Siebel and his flowers, and Faust and Mephistopheles.
bool Source::compoundObjectSubChain(vector < int > &objectPositions)
{ LFS
	if (objectPositions.empty()) return false;
	if (m[objectPositions[0]].previousCompoundPartObject>=0 && m[objectPositions[0]].nextCompoundPartObject<0 && objectPositions.size()==2)
		return true;
	if (m[objectPositions[0]].nextCompoundPartObject<0) return false;
	int limit=m[objectPositions[0]].nextCompoundPartObject,I;
	for (I=1; I<(signed)objectPositions.size() && objectPositions[I]<limit; I++);
	return I==objectPositions.size();
}

void Source::translateBodyObjects(cSpeakerGroup &sg)
{ LFS
	wstring tmpstr;
	vector <int> translatedBodyObjects;
  for (set<int>::iterator i=sg.speakers.begin(); i!=sg.speakers.end(); )
		if (objects[*i].objectClass==BODY_OBJECT_CLASS && objects[*i].ownerWhere>=0)
		{
			int o;
			if (m[objects[*i].ownerWhere].getObject()<0)
			{
				if (m[objects[*i].ownerWhere].objectMatches.size()>1 || m[objects[*i].ownerWhere].objectMatches.empty())
				{
					i++;
					continue;
				}
				o=m[objects[*i].ownerWhere].objectMatches[0].object;
			}
			else
				o=m[objects[*i].ownerWhere].getObject();
			translatedBodyObjects.push_back(o);
			sg.speakers.erase(i++);
		}
		else
			i++;
  for (vector<int>::iterator i=translatedBodyObjects.begin(); i!=translatedBodyObjects.end(); i++)
		sg.speakers.insert(*i);
}

// if the subject of the only sentence in the paragraph indicates the character of the response, then don't split speakerGroup
int Source::detectMetaResponse(int I,int element)
{ LFS
	wstring tmpstr;
	if (m[I].skipResponse>=0) 
	{
		if (m[I].relSubject>=0) 
		{
			if (m[m[I].relSubject].objectMatches.size())
				objectString(m[m[I].relSubject].objectMatches,tmpstr,true);
			else
				objectString(m[m[I].relSubject].getObject(),tmpstr,true);
		}
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d-%06d:Detected meta-response [subject %d:%s].",I,m[I].skipResponse,m[I].relSubject,tmpstr.c_str());
		return m[I].skipResponse;
	}
	int where=m[I].principalWherePosition,skipResponse=I+m[I].pma[element].len;
	bool endsParagraph=false;
	while (true && skipResponse+1<(signed)m.size())
	{
		endsParagraph=isEOS(skipResponse) && m[skipResponse+1].word==Words.sectionWord;
		if (endsParagraph) skipResponse+=2;
		// another voice[boris] which Tommy rather thought was that of Boris replied :
		if (!endsParagraph && (endsParagraph=m[skipResponse-1].word->first==L":" && m[skipResponse].word==Words.sectionWord))
			skipResponse++;
		if (endsParagraph) break;
		int nextSubSentence=skipResponse;
		while (nextSubSentence+1<(signed)m.size() && !isEOS(nextSubSentence) && ((element=m[nextSubSentence].pma.queryPattern(L"__S1"))==-1)) 
			nextSubSentence++;
		if (nextSubSentence+1>=(signed)m.size()) return -1;
		if (isEOS(nextSubSentence)) break;
		where=m[nextSubSentence].principalWherePosition;
		skipResponse=nextSubSentence+m[nextSubSentence].pma[element&~patternFlag].len;
	}
	// tommy felt that[interference,america] Boris had shrugged his[boris] shoulders[boris] as he[boris] answered :
	if (where<0 || !(m[where].objectRole&SUBJECT_ROLE) || (m[where].objectRole&(OBJECT_ROLE|PREP_OBJECT_ROLE)) || !endsParagraph)
		return -1;
	if (m[where].relSubject>=0) 
	{
		if (m[m[where].relSubject].objectMatches.size())
			objectString(m[m[where].relSubject].objectMatches,tmpstr,true);
		else
			objectString(m[m[where].relSubject].getObject(),tmpstr,true);
	}
	for (unsigned int J=0; metaResponse[J]; J++)
		if (m[where].word->first==metaResponse[J])
		{
			m[I].skipResponse=skipResponse;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d-%06d:Detected meta-response (1) [subject %d:%s].",I,skipResponse,m[where].relSubject,tmpstr.c_str());
			if (m[where].relSubject>=0 && !(m[m[where].relSubject].objectRole&NONPAST_OBJECT_ROLE))
				m[I].relSubject=m[where].relSubject;
			return skipResponse;
		}
	int whereVerb=m[where].relVerb;
	// if the verb of the only sentence indicates the character of the response, don't split.  If verb has an object, return false, unless the object is 'question'.
	if (whereVerb<0 || m[whereVerb].word->second.mainEntry==wNULL) return -1;
	// Boris asked a question:
	if (m[whereVerb].getRelObject()>=0 && m[m[whereVerb].getRelObject()].word->first!=L"question") return -1;
  // “Unresolved I am not sure where she[jane] is at the present moment[moment] , ” she[jane] replied .
	if (m[whereVerb].relSubject>1 && m[m[whereVerb].relSubject-1].word->first==L"”") return -1;
	// the Sinn feiner[irish] was speaking . his[irish] rich Irish voice[irish] was unmistakable :
	// another voice[number] , which Tommy fancied was that[number] of the tall , commanding - looking man[number] whose face[number] had seemed familiar to him[number,tommy] , said :
	// the Russian[boris] seemed to consider :
	// if subject is new, then this doesn't represent a continuation of a conversation
	//if (unResolvablePosition(I)) return -1;
	wstring mainEntry=m[whereVerb].word->second.mainEntry->first;
	if (m[whereVerb].relSubject>=0) 
	{
		if (m[m[whereVerb].relSubject].objectMatches.size())
			objectString(m[m[whereVerb].relSubject].objectMatches,tmpstr,true);
		else
			objectString(m[m[whereVerb].relSubject].getObject(),tmpstr,true);
	}
	for (unsigned int J=0; metaResponse[J]; J++)
		if (mainEntry==metaResponse[J])
		{
			m[I].skipResponse=skipResponse;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d-%06d:Detected meta-response (2) [subject %d:%s].",I,skipResponse,m[whereVerb].relSubject,tmpstr.c_str());
			if (m[whereVerb].relSubject>=0 && !(m[m[whereVerb].relSubject].objectRole&NONPAST_OBJECT_ROLE))
				m[I].relSubject=m[whereVerb].relSubject;
			return skipResponse;
		}
	return -1;
}

bool Source::skipMetaResponse(int &I)
{ LFS
	if (m[I].skipResponse<0) return false;
	I=m[I].skipResponse;
	return true;
}

void Source::associatePossessions(int where)
{ LFS
	if (m[where].objectMatches.size()>1 || (m[where].flags&WordMatch::flagInPStatement)) return;
  int o=m[where].getObject(),ro=m[where].getRelObject(),wv=m[where].relVerb; // rs=m[where].relSubject,
	__int64 or=m[where].objectRole;
	if (o>=0 && (or&SUBJECT_ROLE) && wv>=0 && ro>=0 && m[ro].getObject()>=0)
	{
		wstring verb;
		bool has=false;
		unordered_map <wstring, set <int> >::iterator lvtoCi=getVerbClasses(wv,verb);
		if (lvtoCi!=vbNetVerbToClassMap.end())
			for (set <int>::iterator vbi=lvtoCi->second.begin(),vbiEnd=lvtoCi->second.end(); vbi!=vbiEnd; vbi++)
				has=vbNetClasses[*vbi].has;
		if (has)
		{
			m[where].flags|=WordMatch::flagUsedPossessionRelation;
			if (m[where].objectMatches.size()==1)
				o=m[where].objectMatches[0].object;
			objects[o].possessions.push_back(ro);
		}
	}
}

void Source::associateNyms(int where)
{ LFS
	if (m[where].objectMatches.size()>1 || (m[where].flags&WordMatch::flagInPStatement)) return;
	wstring tmpstr;
  //int o=(m[where].objectMatches.size()==1) ? m[where].objectMatches[0].object : m[where].getObject(); this routine should be used BEFORE object is resolved
	// at speakerResolution, because this is used before resolveObject the object matched is the old object from the last phase, so it is invalid.
  int o=m[where].getObject(),wv=m[where].relVerb,tsSense=(wv>=0) ? m[wv].quoteForwardLink : 0; // rs=m[where].relSubject,ro=m[where].getRelObject(),
	__int64 or=m[where].objectRole,ror=-1;
	// vS                               simple examine                     VT_PRESENT                                          R=E=S
	// vS+past                          examined                           VT_PAST                                             R=E<S
	// vB                               has examined                       VT_PRESENT_PERFECT                                  E<S=R
	// vC                               is examining                       VT_EXTENDED+ VT_PRESENT                             S=R E<=>R
	// vC+past													was examining                      VT_EXTENDED+ VT_PAST                                R=E<S
	// vBC                              has been examining                 VT_EXTENDED+ VT_PRESENT_PERFECT                     E<S=R
	// vD                               is examined                        VT_PASSIVE+ VT_PRESENT
	// vD+past													was examined                       VT_PASSIVE+ VT_PAST
	// vBD                              has been examined                  VT_PASSIVE+ VT_PRESENT_PERFECT
	// vCD                              is being examined                  VT_PASSIVE+ VT_PRESENT+VT_EXTENDED
	// vCD+past                         was being examined                 VT_PASSIVE+ VT_PAST+VT_EXTENDED
	// vBCD                             has been being examined            VT_PASSIVE+ VT_PRESENT_PERFECT+VT_EXTENDED
	// neither talking about the past (he had been) nor about the future (he will be)
	bool nymIsUseful=!(tsSense&(VT_POSSIBLE|VT_NEGATION)) && 
		   ((or&IN_PRIMARY_QUOTE_ROLE) ? ((tsSense&VT_TENSE_MASK)==VT_PRESENT || (tsSense&VT_TENSE_MASK)==VT_PRESENT_PERFECT) : ((tsSense&VT_TENSE_MASK)==VT_PAST)); 
	// collect adjectives using is-a relation
	// Whittington was a big man
	// subject can also be object of containing sentence, so we must make sure that the subject of the contained sentence is not an IS object of the containing sentence.
	if (o>=0 && (or&(SUBJECT_ROLE|IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))==(SUBJECT_ROLE|IS_OBJECT_ROLE) && m[where].getRelObject()>=0 &&
		  ((ror=m[m[where].getRelObject()].objectRole)&(OBJECT_ROLE|IS_OBJECT_ROLE|NOT_OBJECT_ROLE))==(OBJECT_ROLE|IS_OBJECT_ROLE) &&
			(!(or&OBJECT_ROLE) || m[where].relSubject<0 || !(m[m[where].relSubject].objectRole&IS_OBJECT_ROLE)))
	{
		if (m[m[where].getRelObject()].getObject()>=0)
		{
			if ((objects[o].plural ^ objects[m[m[where].getRelObject()].getObject()].plural) && 
				  m[where].word->first!=L"you" && m[m[where].getRelObject()].word->first!=L"you")
			{
				wstring tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:objects %s and %s differ in number - adjectival association rejected.",where,whereString(m[where].getRelObject(),tmpstr,true).c_str(),objectString(o,tmpstr2,true).c_str());
				return;
			}
			int ao=(m[m[where].getRelObject()].objectMatches.size()==1) ? m[m[where].getRelObject()].objectMatches[0].object : m[m[where].getRelObject()].getObject();
			// Julius was a great deal younger
			// 'great' does not apply to Julius, only to 'deal'
			// also may reject possible aliases ('a hustler')
			if (objects[ao].neuter && !objects[o].neuter && objects[o].objectClass!=BODY_OBJECT_CLASS)
			{
				wstring tmpstr2;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Neuter object %s.  Adjectives associated with %s are rejected.",where,objectString(ao,tmpstr,true).c_str(),objectString(o,tmpstr2,true).c_str());
				return;
			}
			wstring logMatch;
			tIWMM fromMatch,toMatch,toMapMatch;
			if (nymNoMatch(where,objects.begin()+o,objects.begin()+ao,true,false,logMatch,fromMatch,toMatch,toMapMatch,L"NoMatchSelf"))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Contradictory (2) object %s (on %s)!",where,objectString(o,tmpstr,true).c_str(),logMatch.c_str());
				return;
			}
			if ((objects[ao].associatedNouns.size() || objects[ao].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution) 
			{
				wstring nouns,adjectives; 
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Object %s original associated nouns (%s) and adjectives (%s) taking from %d:%s (3)",
						where,objectString(o,tmpstr,false).c_str(),wordString(objects[o].associatedNouns,nouns).c_str(),wordString(objects[o].associatedAdjectives,adjectives).c_str(),
							m[where].getRelObject(),objectString(ao,tmpstr,false).c_str());
			}
			narrowGender(m[where].getRelObject(),o);
			m[where].flags|=WordMatch::flagUsedBeRelation;
			if (m[where].relVerb>=0)
				m[m[where].relVerb].flags|=WordMatch::flagUsedBeRelation;
			for (vector <tIWMM>::iterator ai=objects[ao].associatedAdjectives.begin(),aiEnd=objects[ao].associatedAdjectives.end(); ai!=aiEnd; ai++)
				if (find(objects[o].associatedAdjectives.begin(),objects[o].associatedAdjectives.end(),*ai)==objects[o].associatedAdjectives.end())
					objects[o].associatedAdjectives.push_back(*ai);
			for (vector <tIWMM>::iterator ai=objects[ao].associatedNouns.begin(),aiEnd=objects[ao].associatedNouns.end(); ai!=aiEnd; ai++)
				if (find(objects[o].associatedNouns.begin(),objects[o].associatedNouns.end(),*ai)==objects[o].associatedNouns.end())
					objects[o].associatedNouns.push_back(*ai);
			if (!(m[objects[o].originalLocation].word->second.flags&tFI::genericGenderIgnoreMatch)) 
				objects[o].updateGenericGender(where,m[objects[ao].originalLocation].word,objects[ao].objectGenericAge,L"associateNyms",debugTrace);
			if ((objects[ao].associatedNouns.size() || objects[ao].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
			{
				wstring nouns,adjectives;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Object %s associated nouns (%s) and adjectives (%s) (3)",
						where,objectString(o,tmpstr,false).c_str(),wordString(objects[ao].associatedNouns,nouns).c_str(),wordString(objects[ao].associatedAdjectives,adjectives).c_str());
			}
			// He[Boris] was probably fifty years old
			//   ALLOBJECTS_1 (old is __ADJECTIVE_WITHOUT_VERB) - about [prep] fifty [card] years [noun] old [adj]
			// He[Boris] was probably fifty years of age
			//   ALLOBJECTS_1 fifty [card] years [noun] of [prep] age [noun]
			ageDetection(where,o,ao);
		}
	}
	// Whittington was big.
	// He is very well off.
	if (o>=0 && (or&(SUBJECT_ROLE|IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))==(SUBJECT_ROLE|IS_OBJECT_ROLE) && m[where].getRelObject()<0 && nymIsUseful)
	{
		m[where].flags|=WordMatch::flagUsedBeRelation;
		for (unsigned int aow=m[where].relVerb+1; (m[aow].objectRole&IS_ADJ_OBJECT_ROLE) && aow<m.size(); aow++)
			if (m[aow].queryWinnerForm(adjectiveForm)>=0 && find(objects[o].associatedAdjectives.begin(),objects[o].associatedAdjectives.end(),m[aow].word)==objects[o].associatedAdjectives.end() &&
					!nymNoMatch(objects.begin()+o,m[aow].word))
			{
				objects[o].associatedAdjectives.push_back(m[aow].word);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Object %s associated adjective (%s) (1)",where,objectString(o,tmpstr,false).c_str(),m[aow].word->first.c_str());
			}
		// look for expressions
		for (unsigned int aow=m[where].relVerb+1;  aow+1<m.size() && (m[aow+1].objectRole&IS_ADJ_OBJECT_ROLE); aow++)
		{
			wstring word=m[aow].word->first+L"_"+m[aow+1].word->first;
			set <wstring> synonyms;
  		getSynonyms(word,synonyms,ADJ);
			for (set <wstring>::iterator s=synonyms.begin(),sEnd=synonyms.end(); s!=sEnd; s++)
			{
				tIWMM ws=Words.query(*s);
				if (ws!=Words.end() && find(objects[o].associatedAdjectives.begin(),objects[o].associatedAdjectives.end(),ws)==objects[o].associatedAdjectives.end() &&
					  !nymNoMatch(objects.begin()+o,ws))
				{
					objects[o].associatedAdjectives.push_back(ws);
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Object %s associated multi-word adjective (%s) from %s (2)",where,objectString(o,tmpstr,false).c_str(),ws->first.c_str(),word.c_str());
				}
			}
			if (synonyms.size()) aow++;
		}
    // He[Boris] was probably about fifty years of age 
		//   ALLOBJECTS_0 - probably [adv] about [prep] fifty [card] years [noun] of [prep] age [noun]
		if (m[where].relVerb>=0 && m[m[where].relVerb].relPrep>=0 && m[m[m[where].relVerb].relPrep].getRelObject()>=0 && m[m[m[m[where].relVerb].relPrep].getRelObject()].getObject()>=0 &&
			  (m[m[m[m[where].relVerb].relPrep].getRelObject()].word->second.timeFlags&T_LENGTH))
			ageDetection(where,o,m[m[m[m[where].relVerb].relPrep].getRelObject()].getObject());
	}
}

// recognize environmentally implicit objects
bool Source::implicitObject(int where)
{ LFS
	if (where>=0 && m[where].getObject()>=0 && ((m[where].objectRole&SUBJECT_ROLE) || (m[where].objectRole&(IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))==(IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE)) && 
		  unResolvablePosition(m[where].beginObjectPosition))
	{
		wchar_t *implicitObjects[]={ L"knock",NULL };
		for (unsigned int J=0; implicitObjects[J]; J++) 
			if (m[where].word->second.mainEntry!=wNULL && m[where].word->second.mainEntry->first==implicitObjects[J])
				return true;
	}
	// the front door bell rang
	if (where>=0 && m[where].getObject()>=0 && m[where].word->first==L"bell" && m[where].relVerb>=0 && m[m[where].relVerb].getMainEntry()->first==L"ring" &&
		  m[where-1].word->first==L"door")
	{
		m[where].objectRole|=UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
		m[where].flags|=WordMatch::flagObjectResolved;
		return true;
	}
	return false;
}

const wchar_t *intString(int startPOVI,int povi,vector <int> &povInSpeakerGroups,wstring &tmpstr)
{ LFS
	tmpstr.clear();
	wstring tmp;
  for (int I=startPOVI; I<povi; I++)
		tmpstr+=itos(povInSpeakerGroups[I],tmp)+L" ";
	if (tmpstr.empty())
		tmpstr=itos(povInSpeakerGroups[startPOVI],tmp);
	return tmpstr.c_str();
}

// distribute povInSpeakerGroups and definitelyIdentifiedAsSpeakerInSpeakerGroups
// to povSpeakers and dnSpeakers in speakerGroups.
void Source::distributePOV(void)
{ LFS
	// distribute people named as others in conversations (to lessen the risk of named supposedly hailed objects actually being talked about, rather than physically there)
	int povi=0,dni=0,sgi=0,mi,currentQuote=firstQuote;
	wstring tmpstr,tmpstr2,tmpstr3;
	for (unsigned int I=0; I<metaNameOthersInSpeakerGroups.size() && sgi<(int)speakerGroups.size(); sgi++)
		for (; I<(signed)metaNameOthersInSpeakerGroups.size() && (mi=metaNameOthersInSpeakerGroups[I])<speakerGroups[sgi].sgEnd; I++)
		{
			speakerGroups[sgi].metaNameOthers.insert(m[mi].getObject());
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:MNO speaker %s inserted into speakerGroup %s.",mi,objectString(m[mi].getObject(),tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
		}
	sgi=0;
	while (sgi<(signed)speakerGroups.size())
	{
		int maleSpeakers=0,femaleSpeakers=0;
		for (set <int>::iterator si=speakerGroups[sgi].speakers.begin(),siEnd=speakerGroups[sgi].speakers.end();  si!=siEnd; si++)
			if (objects[*si].male)
				maleSpeakers++;
			else if (objects[*si].female)
				femaleSpeakers++;
		int conversationalQuotes=0;
		while (currentQuote>=0 && currentQuote<speakerGroups[sgi].sgEnd)
		{
			// determine whether the speaker was just talking to him/herself
			if (m[currentQuote].objectMatches.size()!=1 || m[currentQuote].audienceObjectMatches.size()!=1 ||
				m[currentQuote].objectMatches[0].object!=m[currentQuote].audienceObjectMatches[0].object)
				conversationalQuotes++;
			currentQuote=m[currentQuote].nextQuote;
		}
		speakerGroups[sgi].conversationalQuotes=conversationalQuotes;
		set <int> povAmbiguousSpeakers;
		int startPOVI=povi;
		while (povi<(signed)povInSpeakerGroups.size() && (mi=povInSpeakerGroups[povi])<speakerGroups[sgi].sgEnd)
		{
			int o=m[mi].getObject();
			bool found=speakerGroups[sgi].speakers.find(o)!=speakerGroups[sgi].speakers.end();
			int povAS=-1;
			for (unsigned int I=0; !found && I<m[mi].objectMatches.size(); I++)
				if (speakerGroups[sgi].speakers.find(o=m[mi].objectMatches[I].object)!=speakerGroups[sgi].speakers.end() &&
					!objects[o].plural && (objects[o].male ^ objects[o].female) &&
					!(found=(objects[o].male && maleSpeakers==1) || (objects[o].female && femaleSpeakers==1)))
				{
					int oldPOV=-1;
					if (povAS>=0 && (!sgi || speakerGroups[sgi-1].povSpeakers.size()!=1 || 
						  ((oldPOV=*speakerGroups[sgi-1].povSpeakers.begin())!=o && oldPOV!=povAS)))
					{
						povAS=-1;
						break;
					}
					if (povAS<0 || oldPOV<0 || oldPOV!=povAS)
						povAS=o;
				}
				// if povSpeaker is not certain (it is matched to the object, rather than being the object)
				// check if the object being matched is singular gendered, and if there are no other objects of that
				// gender in the speakerGroup.  If so, it is not ambiguous.
				if (!found && povAS>=0)
				{
					if (sgi && speakerGroups[sgi-1].povSpeakers.find(povAS)==speakerGroups[sgi-1].povSpeakers.end() && speakerGroups[sgi-1].povSpeakers.size())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:ambiguous pov speaker %s in speakerGroup %s rejected (change in pov).",mi,objectString(povAS,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
					}
					else
						povAmbiguousSpeakers.insert(povAS);
				}
				if (found)
				{
					speakerGroups[sgi].povSpeakers.insert(o);
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:POVI speaker %s (1) inserted into speakerGroup %s.",mi,objectString(o,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
				}
				povi++;
		}
		// if there were ambiguous speakers and no nonambiguous ones
		if (povAmbiguousSpeakers.size() && speakerGroups[sgi].povSpeakers.empty())
		{
			bool allIn,oneIn;
			if (povAmbiguousSpeakers.size()==1)
			{
				// if there is only one ambiguous speaker (s1), get the gender, and go back to the last povSpeaker with the same gender, in the same section.
				// if that povSpeaker is in the current speaker group, make it the povSpeaker instead.
				for (int I=sgi-1; I>=0; I--)
					if (speakerGroups[I].povSpeakers.size())
					{
						if (speakerGroups[I].povSpeakers.size()==1 && objects[*speakerGroups[I].povSpeakers.begin()].matchGender(objects[*povAmbiguousSpeakers.begin()]) &&
							speakerGroups[sgi].speakers.find(*speakerGroups[I].povSpeakers.begin())!=speakerGroups[sgi].speakers.end())
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"%s:POVI speaker %s remembered and inserted into speakerGroup %s.",intString(startPOVI,povi,povInSpeakerGroups,tmpstr3),objectString(speakerGroups[I].povSpeakers,tmpstr).c_str(),toText(speakerGroups[sgi],tmpstr2));
							speakerGroups[sgi].povSpeakers=speakerGroups[I].povSpeakers;
							break;
						}
					}
					if (speakerGroups[sgi].povSpeakers.empty())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%s:POVI speaker %s (2) inserted into speakerGroup %s.",intString(startPOVI,povi,povInSpeakerGroups,tmpstr3),objectString(povAmbiguousSpeakers,tmpstr).c_str(),toText(speakerGroups[sgi],tmpstr2));
						speakerGroups[sgi].povSpeakers=povAmbiguousSpeakers;
					}
			}
			// at least one but not all ambiguous speakers were found in the previous speaker group
			else if (sgi && intersect(speakerGroups[sgi-1].speakers,povAmbiguousSpeakers,allIn,oneIn) && !allIn && oneIn)
			{
				for (set <int>::iterator si=speakerGroups[sgi-1].speakers.begin(),siEnd=speakerGroups[sgi-1].speakers.end(); si!=siEnd; si++)
					if (povAmbiguousSpeakers.find(*si)!=povAmbiguousSpeakers.end())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%s:POVI speaker %s (3) inserted into speakerGroup %s.",intString(startPOVI,povi,povInSpeakerGroups,tmpstr3),objectString(*si,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
						speakerGroups[sgi].povSpeakers.insert(*si);
					}
			}
		}
		/* this leads to Boris having POVI incorrectly @22965
		if (speakerGroups[sgi].povSpeakers.empty() && speakerGroups[sgi].speakers.size()==1 && conversationalQuotes==0 &&
			  (!sgi || speakerGroups[sgi-1].povSpeakers.empty() || speakerGroups[sgi-1].povSpeakers==speakerGroups[sgi].speakers) &&
				(!sgi || speakerGroups[sgi-1].observers.empty() || find(speakerGroups[sgi-1].observers.begin(),speakerGroups[sgi-1].observers.end(),*speakerGroups[sgi].speakers.begin())!=speakerGroups[sgi-1].observers.end()))
		{
			speakerGroups[sgi].povSpeakers.insert(speakerGroups[sgi].speakers.begin(),speakerGroups[sgi].speakers.end());
			lplog(LOG_RESOLUTION,L"%s:POVI speakers (4) previous speakerGroup %s.",intString(startPOVI,povi,povInSpeakerGroups,tmpstr3),toText(speakerGroups[sgi-1],tmpstr2));
			lplog(LOG_RESOLUTION,L"%s:POVI speakers (4) inserted into speakerGroup %s.",intString(startPOVI,povi,povInSpeakerGroups,tmpstr3),toText(speakerGroups[sgi],tmpstr2));
		}
		*/
		while (dni<(signed)definitelyIdentifiedAsSpeakerInSpeakerGroups.size() && (mi=definitelyIdentifiedAsSpeakerInSpeakerGroups[dni])<speakerGroups[sgi].sgEnd)
		{
			int o=m[mi].getObject();
			bool found=speakerGroups[sgi].speakers.find(o)!=speakerGroups[sgi].speakers.end(),ambiguous=false;
			if (!found && m[mi].objectMatches.size()==1 &&
				(speakerGroups[sgi].speakers.find(o=m[mi].objectMatches[0].object)!=speakerGroups[sgi].speakers.end()) &&
				!objects[o].plural && (objects[o].male ^ objects[o].female) &&
				!(found=(objects[o].male && maleSpeakers==1) || (objects[o].female && femaleSpeakers==1)) && debugTrace.traceSpeakerResolution)
			{
				if (objects[m[mi].getObject()].objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS) // must set 'found' - something that matches occupation is much less likely to be ambiguous
				{
					lplog(LOG_RESOLUTION,L"%06d:ambiguous dn speaker %s rejected from speakerGroup %s.",mi,objectString(o,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
					ambiguous=true;
				}
				else
					found=true;
			}
			if (!found && o>=0 && objects[o].objectClass==BODY_OBJECT_CLASS && objects[o].ownerWhere>=0)
			{
				int oo=m[objects[o].ownerWhere].getObject(),oow;
				found=speakerGroups[sgi].speakers.find(oo)!=speakerGroups[sgi].speakers.end();
				if (!found && oo>=0 && (oow=objects[oo].ownerWhere)>=0 && m[oow].objectMatches.size()==1 &&
					(found=speakerGroups[sgi].speakers.find(m[oow].objectMatches[0].object)!=speakerGroups[sgi].speakers.end()))
					o=m[oow].objectMatches[0].object;
			}
			// Ivan - a new name only mentioned in quotes.  Also not mergable to any previous name
			if (!found && o>=0 && !ambiguous && objects[o].objectClass==NAME_OBJECT_CLASS && !objects[o].plural && (objects[o].male ^ objects[o].female) && objects[o].getSubType()==-1 &&
				  objects[o].originalLocation>speakerGroups[sgi].sgBegin)
			{
				int nonNameObjectFound=-1,numFound=0;
				// search for a non-name object this name object could be
				for (set <int>::iterator si=speakerGroups[sgi].speakers.begin(),siEnd=speakerGroups[sgi].speakers.end(); si!=siEnd; si++)
					if (objects[*si].objectClass!=NAME_OBJECT_CLASS)
					{
						nonNameObjectFound=*si;
						numFound++;
					}
				if (numFound==1 && speakerGroups[sgi].metaNameOthers.find(o)==speakerGroups[sgi].metaNameOthers.end() && sections.size())
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:new dn speaker %s - replacing %s in speakerGroup %s",mi,objectString(o,tmpstr,false).c_str(),objectString(nonNameObjectFound,tmpstr2,false).c_str(),toText(speakerGroups[sgi],tmpstr3));
					for (section=0; section+1<sections.size() && (signed)sections[section+1].begin<mi; section++);
					currentSpeakerGroup=sgi;
					replaceObjectInSection(mi,o,nonNameObjectFound,L"new name mentioned only in quotes");
					int beginLimit=sections[section].begin,untilLimit=sections[section+1].begin;
					for (vector <cObject::cLocation>::iterator li=objects[nonNameObjectFound].locations.begin(),liEnd=objects[nonNameObjectFound].locations.end(); li!=liEnd; li++)
						if (li->at>=beginLimit && li->at<=untilLimit && m[li->at].getObject()==nonNameObjectFound && !(m[li->at].flags&WordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup))
						{
						  m[li->at].flags|=WordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup;
				      if (debugTrace.traceSpeakerResolution)
								lplog(LOG_SG,L"%06d:flagUnresolvableObjectResolvedThroughSpeakerGroup:%s",li->at,objectString(o,tmpstr2,true).c_str());
							if (m[li->at].objectMatches.empty()) 
							{
								m[li->at].objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
								objects[o].locations.push_back(li->at);
							}
						}
				}
				dni++;
				continue;
			}
			if (found)
			{
				speakerGroups[sgi].dnSpeakers.insert(o);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:dn speaker %s found in speakerGroup %s.",mi,objectString(o,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
			}
			else if (!ambiguous && debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:dn speaker %s rejected from speakerGroup %s.",mi,objectString(o,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
			dni++;
		}
		// an observer is one that is mentioned as having an internal state, but doesn't speak.

		// if the speakerGroup has only two speakers, there is a conversation, and the observer is the only new speaker,
		// add the previous speakers to this speakerGroup (new speakerGroup was created by mistake - the observer triggered a new speakerGroup)
		// if there are only two speakers, and a conversation has taken place, and the observer is not the only new speaker, then assume no one is an observer.
		// ALSO if this speaker is part of the previous speakerGroup, and is not an observer, and every member of the previous speaker group is in present
		// speakerGroup and the previous speakerGroup had conversational quotes, then this is not an observer
		bool conversationRestricted=(speakerGroups[sgi].speakers.size()==2 && speakerGroups[sgi].conversationalQuotes),isAnyNonObserverSpeakerNew=false,isAnyObserverSpeakerNew=false;
		int nonObserver=-1;
		vector <cSpaceRelation>::iterator location = findSpaceRelation((sgi>0) ? speakerGroups[sgi-1].sgBegin : speakerGroups[sgi].sgBegin);
		for (set <int>::iterator mo=speakerGroups[sgi].speakers.begin(),moEnd=speakerGroups[sgi].speakers.end(); mo!=moEnd; mo++)
		{
			bool isPOV=speakerGroups[sgi].povSpeakers.find(*mo)!=speakerGroups[sgi].povSpeakers.end();
			bool isDN=speakerGroups[sgi].dnSpeakers.find(*mo)!=speakerGroups[sgi].dnSpeakers.end();
			bool isPreviousObserver=sgi && find(speakerGroups[sgi-1].observers.begin(),speakerGroups[sgi-1].observers.end(),*mo)!=speakerGroups[sgi-1].observers.end();
			// speaker part of previous speakerGroup and conversationalQuotes
			bool isPreviousSpeaker=sgi && speakerGroups[sgi-1].speakers.find(*mo)!=speakerGroups[sgi-1].speakers.end() && speakerGroups[sgi-1].conversationalQuotes && !isPreviousObserver;
			bool allIn,oneIn,currentSpeakerGroupSuperGroupToPrevious=sgi && intersect(speakerGroups[sgi-1].speakers,speakerGroups[sgi].speakers,allIn,oneIn) && allIn;
			// follower? - search for spaceRelation 'follow'
			bool follower=false;
			for (vector <cSpaceRelation>::iterator li=location; li!=spaceRelations.end() && li->where<speakerGroups[sgi].sgEnd && !follower; li++)
			  follower=(in(*mo,li->whereSubject) && li->whereVerb>=0 && isVerbClass(li->whereVerb,L"chase"));
			// if there is more than one conversational quote, and there is only one definitively named speaker, and there are more than two speakers, and
			//   the speaker was not an observer in a pervious group, then speaker is not an observer (because we don't know whether the other speaker is actually an observer)
			bool uncertainObserver=speakerGroups[sgi].conversationalQuotes && speakerGroups[sgi].dnSpeakers.size()<=1 && speakerGroups[sgi].speakers.size()>2 && !isPreviousObserver;
			// if the speaker was not an observer in previous group and dnSpeakers is empty, then speaker is not an observer
			if (isPOV && !isDN && (!speakerGroups[sgi].dnSpeakers.empty() || isPreviousObserver) && !(isPreviousSpeaker && currentSpeakerGroupSuperGroupToPrevious) && (!uncertainObserver || follower)) 
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"OBS observer %s found in speakerGroup %s.",objectString(*mo,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
				speakerGroups[sgi].observers.insert(*mo);
				if (sgi)
					isAnyObserverSpeakerNew|=speakerGroups[sgi-1].speakers.find(*mo)==speakerGroups[sgi-1].speakers.end();
			}
			// is this non-observer speaker new?
			else if (sgi && conversationRestricted)
			{
				isAnyNonObserverSpeakerNew|=speakerGroups[sgi-1].speakers.find(*mo)==speakerGroups[sgi-1].speakers.end();
				nonObserver=*mo;
			}
		}
		// if all observers are old and all non observers are new and there is only one non-observer (!isAnyObserverSpeakerNew && isAnyNonObserverSpeakerNew && observerContinuing)
		// and the present observer was also the last observer AND in the last speakergroup which had the non-observer, that observer was also the present observer
		bool observerContinuing=sgi && speakerGroups[sgi].observers==speakerGroups[sgi-1].observers;
		if (conversationRestricted && speakerGroups[sgi].observers.size()==1 && observerContinuing && nonObserver>=0)
		{
			// get last speakerGroup of non-observer that contains observer
			observerContinuing=false;
			for (int sgi2=sgi-1; sgi2>=0; sgi2--)
				if (speakerGroups[sgi2].speakers.find(nonObserver)!=speakerGroups[sgi2].speakers.end())
				{
					if (observerContinuing=speakerGroups[sgi].observers==speakerGroups[sgi2].observers)
						break;
					if (speakerGroups[sgi2].speakers.find(*speakerGroups[sgi].observers.begin())!=speakerGroups[sgi2].speakers.end())
						break;
				}
		}
		// if only one quote is recorded, and the only definitive speaker is the observer, then conversationalQuotes should be 0 (no conversation took place?)
		if (conversationalQuotes==1 && speakerGroups[sgi].observers.size()==1 && speakerGroups[sgi].dnSpeakers.size()==1 &&
			speakerGroups[sgi].dnSpeakers==speakerGroups[sgi].observers)
		{
			conversationalQuotes=0;
			conversationRestricted=false;
		}
		// The German and Thomas - When the German is re-introduced, The German creates a new speakerGroup, which is not then merged with the next one either
		// OR
		// if any observers are new and all non observers are old (isAnyObserverSpeakerNew && !isAnyNonObserverSpeakerNew)
		// OR (deleted) or all observers were in the last speaker group as observers
		if (conversationRestricted && speakerGroups[sgi].observers.size() && sgi && 
			((isAnyObserverSpeakerNew && !isAnyNonObserverSpeakerNew) || (!isAnyObserverSpeakerNew && isAnyNonObserverSpeakerNew && observerContinuing)))
			//speakerGroups[sgi].observers==speakerGroups[sgi-1].observers))
		{
			conversationRestricted=false;
			// add previous speakerGroup
			speakerGroups[sgi].speakers.insert(speakerGroups[sgi-1].speakers.begin(),speakerGroups[sgi-1].speakers.end());
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION|LOG_SG,L"speakers added to observer-created speakerGroup %s (conversation restriction isAnyObserverSpeakerNew=%s isAnyNonObserverSpeakerNew=%s observerContinuing=%s).",
							toText(speakerGroups[sgi],tmpstr2),(isAnyObserverSpeakerNew) ? L"true":L"false",(isAnyNonObserverSpeakerNew) ? L"true":L"false",(observerContinuing) ? L"true":L"false");
		}
		// both speakergroups must belong to the same section
		for (section=0; section+1<sections.size() && (signed)sections[section+1].begin<speakerGroups[sgi].sgBegin; section++);
		bool previousSpeakerGroupInSameSection=section<sections.size() && sgi>0 && ((signed)sections[section].begin<speakerGroups[sgi-1].sgBegin);
		if (conversationRestricted)
		{
			if (speakerGroups[sgi].observers.size())
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"all observers erased in speakerGroup %s (conversation restriction).",toText(speakerGroups[sgi],tmpstr2));
				speakerGroups[sgi].observers.clear();
			}
		}
		else
		{
			// also an observer could be the observer of the previous group, who also doesn't speak in the current group, and there are no other povSpeakers of the current group.
			// also a section cannot be crossed with this assumption.
			if (speakerGroups[sgi].povSpeakers.empty() && sgi && speakerGroups[sgi-1].observers.size())
			{
				// both speakergroups must belong to the same section
				if (previousSpeakerGroupInSameSection)
				{
					for (set <int>::iterator mo=speakerGroups[sgi-1].observers.begin(),moEnd=speakerGroups[sgi-1].observers.end(); mo!=moEnd; mo++)
					{
						// if not a definite speaker, but is a speaker, and is not already an observer
						if (speakerGroups[sgi].dnSpeakers.find(*mo)==speakerGroups[sgi].dnSpeakers.end() &&
							speakerGroups[sgi].speakers.find(*mo)!=speakerGroups[sgi].speakers.end() &&
							find(speakerGroups[sgi].observers.begin(),speakerGroups[sgi].observers.end(),*mo)==speakerGroups[sgi].observers.end())
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"POVI OBS observer %s found in speakerGroup %s (2).",objectString(*mo,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
							speakerGroups[sgi].observers.insert(*mo);
							speakerGroups[sgi].povSpeakers.insert(*mo);
						}
					}
				}
			}
			// also an pov could be the pov of the previous group, who also doesn't speak in the current group, and there are no other povSpeakers of the current group.
			// also all the povSpeakers must be also of the current group.
			if (speakerGroups[sgi].povSpeakers.empty() && sgi && speakerGroups[sgi-1].povSpeakers.size() && previousSpeakerGroupInSameSection)
			{
				// all the previous povSpeakers also have to be in the current group
				bool oneIn,allIn;
				intersect(speakerGroups[sgi-1].povSpeakers,speakerGroups[sgi].speakers,allIn,oneIn);
				if (allIn)
				{
					speakerGroups[sgi].povSpeakers=speakerGroups[sgi-1].povSpeakers;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"pov speakers from %s moved forward into speakerGroup %s.",toText(speakerGroups[sgi-1],tmpstr),toText(speakerGroups[sgi],tmpstr2));
					// if there are no definite speakers in speakerGroup, then the povSpeaker would probably NOT be the one not speaking, so skip this observer test
					if (speakerGroups[sgi].dnSpeakers.size() || !speakerGroups[sgi].conversationalQuotes) 
					{
						for (set <int>::iterator mo=speakerGroups[sgi].povSpeakers.begin(),moEnd=speakerGroups[sgi].povSpeakers.end(); mo!=moEnd; mo++)
						{
							// if not a definite speaker, but is a speaker, and is not already an observer
							if (speakerGroups[sgi].dnSpeakers.find(*mo)==speakerGroups[sgi].dnSpeakers.end())
							{
								if (debugTrace.traceSpeakerResolution)
									lplog(LOG_RESOLUTION,L"OBS observer %s derived from pov in previous speakerGroup in speakerGroup %s (3).",objectString(*mo,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
								speakerGroups[sgi].observers.insert(*mo);
							}
						}
						if (speakerGroups[sgi].observers.size()==1 && speakerGroups[sgi].groupedSpeakers.find(*speakerGroups[sgi].observers.begin())!=speakerGroups[sgi].groupedSpeakers.end())
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"OBS observer %s derived from pov in previous speakerGroup in speakerGroup %s CANCELLED (in group).",objectString(speakerGroups[sgi].observers,tmpstr).c_str(),toText(speakerGroups[sgi],tmpstr2));
							speakerGroups[sgi].observers.clear();
						}
					}
				}
			}
			// if an observer belongs to a group and that any of the group definitely speaks, then remove the observer. (error check)
			bool allObserversInGroup=true,oneObserverInGroup;
			// are observers grouped?
			intersect(speakerGroups[sgi].groupedSpeakers,speakerGroups[sgi].observers,allObserversInGroup,oneObserverInGroup);
			// if at least one observer is in a group, but not all the observers are in the group
			if (oneObserverInGroup && !allObserversInGroup)
			{
				bool allGroupedAreDefiniteSpeakers,oneGroupedIsDefiniteSpeaker;
				// if any of the groupedSpeakers is a definite speaker, then erase all observers that are in the group.
				intersect(speakerGroups[sgi].groupedSpeakers,speakerGroups[sgi].dnSpeakers,allGroupedAreDefiniteSpeakers,oneGroupedIsDefiniteSpeaker);
				if (oneGroupedIsDefiniteSpeaker)
					for (set <int>::iterator mo=speakerGroups[sgi].observers.begin(); mo!=speakerGroups[sgi].observers.end(); )
					{
						if (speakerGroups[sgi].groupedSpeakers.find(*mo)==speakerGroups[sgi].groupedSpeakers.end()) 
							mo++;
						else
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"observer %s erased from speakerGroup %s [observer is in speaking group].",objectString(*mo,tmpstr,true).c_str(),toText(speakerGroups[sgi],tmpstr2));
							speakerGroups[sgi].observers.erase(mo++);
						}
					}
			}
		}
		if (speakerGroups[sgi].povSpeakers.empty())
		{
			if (speakerGroups[sgi].speakers.size()==1)
			{
				speakerGroups[sgi].povSpeakers=speakerGroups[sgi].speakers;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"POVI only lonely in speakerGroup %s.",toText(speakerGroups[sgi],tmpstr2));
			}
			else if (speakerGroups[sgi].speakers.find(0)!=speakerGroups[sgi].speakers.end())
			{
				speakerGroups[sgi].povSpeakers.insert(0);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"POVI Narrator in speakerGroup %s.",toText(speakerGroups[sgi],tmpstr2));
			}
		}
		sgi++;
	}
}

bool Source::invalidGroupObjectClass(int oc)
{ LFS
	  return oc!=NAME_OBJECT_CLASS && oc!=GENDERED_GENERAL_OBJECT_CLASS && 
	   oc!=BODY_OBJECT_CLASS && oc!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && 
		 oc!=GENDERED_DEMONYM_OBJECT_CLASS && oc!=GENDERED_RELATIVE_OBJECT_CLASS && 
		 oc!=META_GROUP_OBJECT_CLASS;
}

void Source::accumulateGroups(int where, vector <int> &groupedObjects, int &lastWhereMPluralGroupedObject)
{
	LFS
	vector <WordMatch>::iterator im = m.begin() + where;
	wstring tmpstr,tmpstr2;
	// accumulate groups of objects for speakerGroup subgroups
	if (groupedObjects.size() && !(im->objectRole&MPLURAL_ROLE))
	{
		// only allow if each object is new to this group
		bool invalidObject=false;
		for (unsigned int om=0; om<groupedObjects.size() && !invalidObject; om++)
			invalidObject=groupedObjects[om]<0 || invalidGroupObjectClass(objects[groupedObjects[om]].objectClass) || objects[groupedObjects[om]].getSubType()>=0; // places should not be grouped, because these groups are used to group speakers
		set <int> repeatedObjects;
		// check for repeats
		for (int I=0; I<(signed)groupedObjects.size(); I++) repeatedObjects.insert(groupedObjects[I]);
		if (!invalidObject && groupedObjects.size()==repeatedObjects.size() && groupedObjects.size()>1)
		{
			tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(lastWhereMPluralGroupedObject,groupedObjects));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION|LOG_SG,L"%d:Grouped gendered objects %s in tempSpeakerGroup %s (1).",lastWhereMPluralGroupedObject,objectString(groupedObjects,tmpstr).c_str(),toText(tempSpeakerGroup,tmpstr2));
		}
	  groupedObjects.clear();
	}
	// Tommy, accompanied by Albert, explored the grounds.
	int element=-1;
	int whereWithObject=-1;
	if ((m[where].objectRole&SUBJECT_ROLE) && where+3<(signed)m.size() && (element=m[where+1].pma.queryPattern(L"__C1_IP"))!=-1 && m[where+2].getMainEntry()->first==L"accompany" &&
			m[where+3].word->first==L"by")
	{
		whereWithObject=m[where+3].getRelObject();
		if (m[where].relVerb>=0 && whereWithObject>=0 && m[whereWithObject].relVerb<0)
			m[whereWithObject].relVerb=m[where].relVerb;
	}
	// With him[conrad] was the evil - looking Number 14 .
	if ((m[where].objectRole&SUBJECT_ROLE) && m[where].getRelObject()<0 && m[where].relVerb>=0 && m[m[where].relVerb].queryWinnerForm(isForm)>=0 &&
		  m[m[where].relVerb].relPrep>=0 && m[m[m[where].relVerb].relPrep].word->first==L"with" && m[m[m[where].relVerb].relPrep].getRelObject()>=0)
		whereWithObject=m[m[m[where].relVerb].relPrep].getRelObject();
	if (whereWithObject>=0 && m[whereWithObject].getObject()>=0 && im->getObject()>=0 &&
			objects[m[whereWithObject].getObject()].objectClass!=BODY_OBJECT_CLASS && m[whereWithObject].objectMatches.size()<=1 &&
			objects[im->getObject()].objectClass!=BODY_OBJECT_CLASS && im->objectMatches.size()<=1)
	{
		int withObject=(m[whereWithObject].objectMatches.size()==1) ? m[whereWithObject].objectMatches[0].object : m[whereWithObject].getObject();
		int o=(im->objectMatches.size()==1) ? im->objectMatches[0].object : im->getObject();
		if (!invalidGroupObjectClass(objects[withObject].objectClass) && objects[withObject].getSubType()<0 &&
				!invalidGroupObjectClass(objects[o].objectClass) &&  objects[o].getSubType()<0 &&
				o!=withObject)
		{
			groupedObjects.push_back(withObject);
			groupedObjects.push_back(o);
			tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where,groupedObjects));
			im->objectRole|=MPLURAL_ROLE; // so that this group is assigned the highest preference grouping
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION|LOG_SG,L"%d:Grouped gendered objects %s in tempSpeakerGroup %s (5).",where,objectString(groupedObjects,tmpstr).c_str(),toText(tempSpeakerGroup,tmpstr2));
			groupedObjects.clear();
		}
	}
	if (im->getObject()!=-1 && !(im->flags&WordMatch::flagAdjectivalObject))
	{
		if ((im->objectRole&MPLURAL_ROLE))
		{
			lastWhereMPluralGroupedObject=where;
			if (im->objectMatches.empty() && !invalidGroupObjectClass(objects[im->getObject()].objectClass) &&  objects[im->getObject()].getSubType()<0) 
				groupedObjects.push_back(im->getObject());
			else if ((im->word->second.inflectionFlags&PLURAL) || (im->objectMatches.size()==1))
			{
				for (unsigned int om=0; om<im->objectMatches.size(); om++)
					if (!invalidGroupObjectClass(objects[im->objectMatches[om].object].objectClass) && objects[im->objectMatches[om].object].getSubType()<0)
					groupedObjects.push_back(im->objectMatches[om].object);
			}
		}
		else if ((im->word->second.inflectionFlags&PLURAL) && (im->objectMatches.size()>1) && 
			       (im->getObject()<0 || objects[im->getObject()].objectClass!=BODY_OBJECT_CLASS))
		{
			vector <int> groupedPluralMatchedObjects;
			bool invalidObject=false;
			for (unsigned int om=0; om<im->objectMatches.size() && !invalidObject; om++)
				// this disallows groups that are more obviously incorrect.  It is a little more conservative algorithm.
				invalidObject=im->objectMatches[om].object<0 || objects[im->objectMatches[om].object].firstPhysicalManifestation<tempSpeakerGroup.sgBegin ||
						invalidGroupObjectClass(objects[im->objectMatches[om].object].objectClass) ||
						objects[im->objectMatches[om].object].getSubType()>=0;
			// or if there are only two objects, and both are physically present narrative subjects
			if (invalidObject)
			{
				for (vector <cOM>::iterator omi=im->objectMatches.begin(),omiEnd=im->objectMatches.end(); omi!=omiEnd; omi++)
						if (find(nextNarrationSubjects.begin(),nextNarrationSubjects.end(),omi->object)!=nextNarrationSubjects.end())
							groupedPluralMatchedObjects.push_back(omi->object);
				if (groupedPluralMatchedObjects.size()>=2)
					invalidObject=false;
				else
					groupedPluralMatchedObjects.clear();
			}
			if (invalidObject)
			{
				// or if there are only two objects, and both are speakers
				for (vector <cOM>::iterator omi=im->objectMatches.begin(),omiEnd=im->objectMatches.end(); omi!=omiEnd; omi++)
						if (tempSpeakerGroup.speakers.find(omi->object)!=tempSpeakerGroup.speakers.end())
							groupedPluralMatchedObjects.push_back(omi->object);
				if (groupedPluralMatchedObjects.size()>=2)
					invalidObject=false;
				else
					groupedPluralMatchedObjects.clear();
			}
			if (invalidObject)
			{
				// have these objects ever been grouped in the past?
				bool allIn,oneIn;
				for (int sg=speakerGroups.size()-1; sg>=0 && invalidObject; sg--)
					if (intersect(where,speakerGroups[sg].groupedSpeakers,allIn,oneIn) && allIn)
						invalidObject=false;
			}
			// all, any, etc. are too vague or don't imply a group
			// also 'people', 'everybody' etc also doesn't imply a group [of associates or friends]
			// don't accumulate groups of neuter objects (that might be picked up as groups of gendered objects later)
			invalidObject|=(im->queryForm(pronounForm)>=0 || im->queryForm(indefinitePronounForm)>=0 || (objects[im->getObject()].neuter && !(objects[im->getObject()].male || objects[im->getObject()].female))); 
			// all the others
			if (im->beginObjectPosition!=where)
				invalidObject|=(m[im->beginObjectPosition].queryForm(pronounForm)>=0 || m[im->beginObjectPosition].queryForm(indefinitePronounForm)>=0); // all, any, etc. are too vague or don't imply a group
			if (!invalidObject)
			{
				if (groupedPluralMatchedObjects.size())
				{
					tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where,groupedPluralMatchedObjects));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION|LOG_SG,L"%d:Grouped gendered objects %s in tempSpeakerGroup %s (2).",where,objectString(groupedPluralMatchedObjects,tmpstr).c_str(),toText(tempSpeakerGroup,tmpstr2));
				}
				else
				{
					groupedObjects.clear();
					for (unsigned int om=0; om<im->objectMatches.size(); om++)
						groupedObjects.push_back(im->objectMatches[om].object);
					tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where,groupedObjects));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION|LOG_SG,L"%d:Grouped gendered objects %s in tempSpeakerGroup %s (3).",where,objectString(groupedObjects,tmpstr).c_str(),toText(tempSpeakerGroup,tmpstr2));
				}
			}
		  groupedObjects.clear();
		}
		else if ((im->word->second.inflectionFlags&PLURAL)!=0 && im->objectMatches.size()<=1 && 
						 im->getObject()>=0 && objects[im->getObject()].objectClass!=BODY_OBJECT_CLASS)
		{
			int o=im->getObject(),oc=objects[o].objectClass;
			// this disallows groups that are more obviously incorrect.  It is a little more conservative algorithm.
			if (o>=0 && objects[o].firstPhysicalManifestation>=tempSpeakerGroup.sgBegin && !invalidGroupObjectClass(oc) && objects[o].getSubType()<0)
			{
				groupedObjects.clear();
				groupedObjects.push_back(o);
				tempSpeakerGroup.groups.push_back(cSpeakerGroup::cGroup(where,groupedObjects));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION|LOG_SG,L"%d:Grouped gendered objects %s in tempSpeakerGroup %s (4).",where,objectString(groupedObjects,tmpstr).c_str(),toText(tempSpeakerGroup,tmpstr2));
			}
			groupedObjects.clear();
		}
	}
}

bool Source::sameSpeaker(int sWhere1,int sWhere2)
{ LFS
	if (sWhere1==-1 || sWhere2==-1)
		return true;
	if (m[sWhere1].objectMatches.empty() && m[sWhere2].objectMatches.empty())
		return m[sWhere1].getObject()==m[sWhere2].getObject();
	if (m[sWhere1].objectMatches.empty())
		return in(m[sWhere1].getObject(),m[sWhere2].objectMatches)==m[sWhere2].objectMatches.end();
	if (m[sWhere2].objectMatches.empty())
		return in(m[sWhere2].getObject(),m[sWhere1].objectMatches)==m[sWhere1].objectMatches.end();
	bool allIn,oneIn;
	intersect(m[sWhere1].objectMatches,m[sWhere2].objectMatches,allIn,oneIn);
	return oneIn;
}

void Source::embeddedStory(int where,int &numPastSinceLastQuote,int &numNonPastSinceLastQuote,int &numSecondInQuote,int &numFirstInQuote,
													 int &lastEmbeddedStory,int &lastEmbeddedImposedSpeakerPosition,int lastSpeakerPosition)
{ LFS
	bool mustBeExtension=false;
	// if speaker is not imposed, and the quote is inserted at the end (thus implying continuation)
	if ((numPastSinceLastQuote>1 && numPastSinceLastQuote >= numNonPastSinceLastQuote*2) ||
			(mustBeExtension=(lastSpeakerPosition<0 && lastEmbeddedStory>=0 && m[lastOpeningPrimaryQuote].endQuote>=0 && 
			 (m[m[lastOpeningPrimaryQuote].endQuote].flags&WordMatch::flagInsertedQuote)!=0)))
	{
		wstring tmpstr;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d-%06d:LQ past=%03d	nonPast=%03d first=%03d second=%03d lastEmbeddedStory=%d %d:%d %s",
				lastQuote,m[lastOpeningPrimaryQuote].endQuote,numPastSinceLastQuote,numNonPastSinceLastQuote,
				numFirstInQuote,numSecondInQuote,lastEmbeddedStory,lastEmbeddedImposedSpeakerPosition,lastSpeakerPosition,
				objectString(m[lastQuote].objectMatches,tmpstr,true).c_str());
		if (lastEmbeddedStory>=0 && sameSpeaker(lastEmbeddedImposedSpeakerPosition,lastSpeakerPosition))
		{
			// gap - allow a gap of 1 (must be non story, which is to say, in the present tense)
			int gap=0,lastGap=-1;
			for (int es=lastEmbeddedStory; es!=lastQuote && gap<=1; es=m[es].nextQuote)
			{
				if (m[es].flags&WordMatch::flagEmbeddedStoryResolveSpeakers)
					gap=0;
				else
				{
					gap++;
					lastGap=es;
				}
			}
			// previous = 63124 - [tommy:julius] By Jove , that explains why they[james,tuppence,interest] looked at me[tommy]
			// current = 63184 -  [julius:tommy] They[fellow,tuppence,james,interest...] didn't give you[tommy] any sort
			bool switchPersonGap;
			if (switchPersonGap=(((m[lastEmbeddedStory].flags&(WordMatch::flagSecondEmbeddedStory|WordMatch::flagFirstEmbeddedStory))==WordMatch::flagFirstEmbeddedStory && numSecondInQuote && !numFirstInQuote) ||
					((m[lastEmbeddedStory].flags&(WordMatch::flagSecondEmbeddedStory|WordMatch::flagFirstEmbeddedStory))==WordMatch::flagSecondEmbeddedStory && numFirstInQuote && !numSecondInQuote)) &&
					gap<=1)
				lplog(LOG_RESOLUTION,L"%06d,%06d:LQ person violation",lastEmbeddedStory,lastQuote);
			if (gap>1 || switchPersonGap)
			{
				if (mustBeExtension) return;
				m[lastQuote].flags|=WordMatch::flagEmbeddedStoryBeginResolveSpeakers;
				subNarratives.push_back(lastQuote);
				lastEmbeddedStory=lastQuote;
				m[where].embeddedStorySpeakerPosition=-1; 
				lastEmbeddedImposedSpeakerPosition=lastSpeakerPosition;
			}
			else 
			{
				if (debugTrace.traceSpeakerResolution)
				{
					if (gap)
						lplog(LOG_RESOLUTION,L"%06d,%06d:LQ EXTENDED (GAP@%d)",lastEmbeddedStory,m[lastOpeningPrimaryQuote].endQuote,lastGap);
					else
						lplog(LOG_RESOLUTION,L"%06d,%06d:LQ EXTENDED",lastEmbeddedStory,m[lastOpeningPrimaryQuote].endQuote);
				}
				if (lastEmbeddedImposedSpeakerPosition<0) 
					lastEmbeddedImposedSpeakerPosition=lastSpeakerPosition;
				m[where].embeddedStorySpeakerPosition=lastEmbeddedImposedSpeakerPosition;
				if (gap && lastGap>=0)
					m[lastGap].flags|=WordMatch::flagEmbeddedStoryResolveSpeakersGap;
			  // if a story starts, and the very next quote has a definite attribution, then the previous quote is not speaker contiguous
				if (gap==0 && m[lastEmbeddedStory].nextQuote==lastQuote && lastSpeakerPosition>=0 && m[lastQuote].speakerPosition==lastSpeakerPosition)
				{
					numPastSinceLastQuote=numNonPastSinceLastQuote=numSecondInQuote=numFirstInQuote=0;
					return;
				}
			}
		}
		else
		{
			m[lastQuote].flags|=WordMatch::flagEmbeddedStoryBeginResolveSpeakers;
			subNarratives.push_back(lastQuote);
			lastEmbeddedStory=lastQuote;
			lastEmbeddedImposedSpeakerPosition=lastSpeakerPosition;
		}
		// an inserted quote is used in the speaker resolution to assert that this quote is the same as last quote.
		// however when a story is being related this is not necessarily true as the other speaker can occasionally interject (gap==1).
		m[m[lastQuote].endQuote].flags&=~WordMatch::flagInsertedQuote; // overrides inserted quote - more authoritative
		m[lastQuote].flags|=WordMatch::flagEmbeddedStoryResolveSpeakers;
		for (int I=lastQuote+1; I<m[lastOpeningPrimaryQuote].endQuote; I++)
			m[I].objectRole|=IN_EMBEDDED_STORY_OBJECT_ROLE;
		if (numSecondInQuote)
			m[lastQuote].flags|=WordMatch::flagSecondEmbeddedStory;
		if (numFirstInQuote)
			m[lastQuote].flags|=WordMatch::flagFirstEmbeddedStory;
	}
	else if (debugTrace.traceSpeakerResolution && lastOpeningPrimaryQuote>=0)
		lplog(LOG_RESOLUTION,L"%06d-%06d:L past=%03d	nonPast=%03d first=%03d second=%03d",lastQuote,m[lastOpeningPrimaryQuote].endQuote,numPastSinceLastQuote,numNonPastSinceLastQuote,numFirstInQuote,numSecondInQuote);
	numPastSinceLastQuote=numNonPastSinceLastQuote=numSecondInQuote=numFirstInQuote=0;
}

void Source::adjustHailRoleDuringScan(int where)
{ LFS
	vector <WordMatch>::iterator im=m.begin()+where;
	unsigned __int64 or=im->objectRole&(HAIL_ROLE|MPLURAL_ROLE|RE_OBJECT_ROLE);
	int oc=(im->getObject()>=0) ? objects[im->getObject()].objectClass:-1;
	// Here[here] we[tommy,julius] are . Ebury , Yorks .
	// Come at once , Moat House , Ebury , Yorkshire , great developments -- Tommy
	// where==Ebury
	int so;
	if ((oc==NAME_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS) && !objects[im->getObject()].PISDefinite &&
		  im->beginObjectPosition>0 && 
			(m[im->beginObjectPosition-1].word->second.isSeparator() || (m[where].objectRole&MOVEMENT_PREP_OBJECT_ROLE)) && 
      im->endObjectPosition+2<(signed)m.size() && m[im->endObjectPosition+1].endObjectPosition<(signed)m.size() &&
			m[im->endObjectPosition].word->first==L"," &&
			(so=m[im->endObjectPosition+1].getObject())>=0 && !objects[so].PISDefinite &&
			(objects[so].getSubType()>=0 || (objects[so].name.hon==wNULL && !objects[so].isNotAPlace && objects[im->getObject()].getSubType()>=0)) &&
			objects[so].objectClass==NAME_OBJECT_CLASS && 
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
	vector <cObject>::iterator o=objects.begin()+im->getObject();
	if (im->getObject()>=0 && !(im->objectRole&HAIL_ROLE) && o->objectClass==NAME_OBJECT_CLASS && (im->objectRole&IN_PRIMARY_QUOTE_ROLE) &&
			im->beginObjectPosition && m[im->beginObjectPosition-1].word->first==L"“" &&	m[im->endObjectPosition].word->first==L"," && m[im->endObjectPosition+1].word->first==L"”" &&
			(o->PISDefinite || o->PISHail>1 || (o->name.hon!=wNULL && !o->name.justHonorific() && o->numEncountersInSection>1))) // encounters already at least one because of resolveObject
	{
		vector <cLocalFocus>::iterator lsi=in(im->getObject());
		if ((lsi!=localObjects.end() && lsi->physicallyPresent) || 
			  // if this is the first time the speaker is mentioned in the speaker group.
			  (speakerGroupsEstablished && currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.find(im->getObject())!=speakerGroups[currentSpeakerGroup].speakers.end()))
		{
			im->objectRole|=HAIL_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%06d:Acquired HAIL role (4) definite=%d subject=%d encountered=%d.",where,o->PISDefinite,o->PISSubject,o->numEncountersInSection);
		}
	}
	// if the last quote has the speaker talking to himself/herself, remove hail
	if ((or&HAIL_ROLE) && (im->objectRole&IN_PRIMARY_QUOTE_ROLE) && lastOpeningPrimaryQuote>=0 && 
		  m[lastOpeningPrimaryQuote].audiencePosition>=0 && m[m[lastOpeningPrimaryQuote].audiencePosition].queryWinnerForm(reflexiveForm)>=0)
	{
			or&=~HAIL_ROLE;
			im->objectRole&=~HAIL_ROLE;
			if (debugTrace.traceRole)
				lplog(LOG_ROLE,L"%06d:Removed HAIL role (REFLEXIVE).",where);
	}
}

void Source::eraseAliasesAndReplacementsInSpeakerGroups(void)
{ LFS
	for (vector <cSpeakerGroup>::iterator sg=speakerGroups.begin(); sg!=speakerGroups.end() && (sg+1)!=speakerGroups.end(); )
		if (eraseAliasesAndReplacementsInSpeakerGroup(sg,false) && sg->speakers.size()==1 && 
			// if a replacement occurs, reducing it from 2 to 1 (and so a non-self conversation is not possible)
			  (sg+1)->speakers.size()>1 && (sg+1)->speakers.find(*sg->speakers.begin())!=(sg+1)->speakers.end() && sg->conversationalQuotes>1)
		{
			(sg+1)->sgBegin=sg->sgBegin;
			(sg+1)->conversationalQuotes+=sg->conversationalQuotes;
			(sg+1)->dnSpeakers.insert(sg->dnSpeakers.begin(),sg->dnSpeakers.end());
			lplog(LOG_RESOLUTION,L"speakerGroup erased: %d",sg-speakerGroups.begin());
			sg=speakerGroups.erase(sg);
		}
		else
			sg++;
}

bool Source::blockSpeakerGroupCreation(int endSection,bool quotesSeenSinceLastSentence,int nsAfter)
{ LFS
	bool block=false;
	if (quotesSeenSinceLastSentence) // if the previous paragraph was a quote
	{
		block=(nsAfter<(int)m.size() && m[nsAfter].word->first==L"“"); // in the middle of a conversation
		// also block if the present paragraph's only sentence is a speaker attribution
		if (!block)
		{
			// (search for next quote)
			int beginQuote=nsAfter;
			for (; beginQuote<(int)m.size() && m[beginQuote].word->first!=L"“" &&
				     m[beginQuote].word->first!=L"?" && m[beginQuote].word->first!=L"!" && (m[beginQuote].word->first!=L"." || m[beginQuote].PEMACount); beginQuote++);
		  if (beginQuote<(int)m.size() && m[beginQuote].word->first==L"“")
			{
				// (search for the speaker position)
				bool definitelySpeaker=true,previousParagraph=false,crossedSectionBoundary=false; // checkCataSpeaker=false,
				int speakerPosition=-1,beforeLocation=speakerBefore(beginQuote,previousParagraph),audienceObjectPosition=-1; // unknown
				if (beforeLocation>=0)
					speakerPosition=scanForSpeaker(beforeLocation,definitelySpeaker,crossedSectionBoundary,audienceObjectPosition);
				block=(speakerPosition>=0 && speakerPosition==nsAfter);
			}
			// do not allow a speaker group to be a quote without including either the previous paragraph or the next paragraph
			// also block if there is a quote in the paragraph immediately after lastSpeakerGroupPositionConsidered, and 
			// the quote ends (or through forwardLinks) immediately before I.
			if (!block && speakerGroups.size() && lastOpeningPrimaryQuote>=0 && m[lastOpeningPrimaryQuote].endQuote+1==endSection)
			{
				int firstQuoteAfter=speakerGroups[speakerGroups.size()-1].sgBegin+1;
				for (; firstQuoteAfter<lastOpeningPrimaryQuote && m[firstQuoteAfter].word->first!=L"“"; firstQuoteAfter++);
				while (m[firstQuoteAfter].quoteForwardLink>=0) firstQuoteAfter=m[firstQuoteAfter].quoteForwardLink;
				block|=firstQuoteAfter==lastOpeningPrimaryQuote;
			}
		}
	}
	// also block if the last paragraph ends with a colon (so the subject is saying something)
	return block|(endSection && m[endSection-1].word->first==L":");
}

int Source::getSpeakersToKeep(vector<cSpaceRelation>::iterator sr)
{ LFS
	vector <cLocalFocus>::iterator llsi;
	int numPPSpeakers=0,keepPPSpeakerWhere=sr->whereSubject; // lastPPSpeaker=-1,
	// subject cannot be used
	if (sr->whereSubject>=0 && m[sr->whereSubject].getObject()>=0 && !objects[m[sr->whereSubject].getObject()].isAgent(true))
		keepPPSpeakerWhere=-1;
	if (keepPPSpeakerWhere<0)
	{
		// if there is only one PP speaker, and whereSubject is not a speaker, and spaceRelation is not a TIME type, then set where to the PP speaker.
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
			if (lsi->physicallyPresent && lsi->numIdentifiedAsSpeaker>0)
			{
				numPPSpeakers++;
				llsi=lsi;
			}
		if (numPPSpeakers==1)
			keepPPSpeakerWhere=llsi->whereBecamePhysicallyPresent;
		else if (sr->whereObject>=0 && m[sr->whereObject].getObject()>=0 && objects[m[sr->whereObject].getObject()].isAgent(true))
			keepPPSpeakerWhere=sr->whereObject;
	}
	return keepPPSpeakerWhere;
}

// identify definite speakers and subjects in narration.
// speakers and subjects must be name objects only.
// no objects are actually resolved.
// establish speaker groups
// text is broken into series of quotes
// each series has a collection of definite speakers
// a series is begun by a paragraph ending with no quotes.
// if the series ends with only one speaker identified, and that speaker is
// in the previous series, then extend the previous series to the end of the current one.
// if the series has the same speakers as the previous, extend the previous one.
// resolve all objects outside quotes.
void Source::identifySpeakerGroups()
{ LFS 
  bool quotesSeen=false,quotesSeenSinceLastSentence=false,endOfSentence=false,immediatelyAfterEndOfParagraph=true,inPrimaryQuote=false,inSecondaryQuote=false,uniquelyMergable,currentIsQuestion=false;
	bool firstQuotedSentenceOfSpeakerGroupNotSeen=true,transitionSinceEOS=false;
  int lastSentenceEnd=0,uqLastSentenceEnd=0,uqPreviousToLastSentenceEnd=0,lastQuotedString=-1,lastSpeakerGroupOfPreviousSection=-1,lastBeginS1=-1,lastRelativePhrase=-1,lastQ2=-1,lastVerb=-1,lastCommand=-1;
  int lastSentenceEndBeforeAndNotIncludingCurrentQuote=-1,lastProgressPercent=-1;
	int questionSpeaker=-1,questionSpeakerLastParagraph=-1,questionSpeakerLastSentence=-1,whereFirstSubjectInParagraph=-1; // question processing
  int quotedObjectCounter=0,lastSpeakerGroupPositionConsidered=0; // lastSectionEnd=-1,
	int endMetaResponse=-1,lastSpeakerPosition=-1,lastEmbeddedImposedSpeakerPosition=-1;
	int lastWhereMPluralGroupedObject=-1;
	// embedded stories
	int numPastSinceLastQuote=0,numNonPastSinceLastQuote=0,numFirstInQuote=0,numSecondInQuote=0,lastEmbeddedStory=-1;
  vector <int> lastSubjects,previousLastSubjects,groupedObjects,secondaryQuotesResolutions;
  vector < vector <tTagLocation> > tagSets;
  set <int>::iterator stsi;
  unsigned int agingStructuresSeen=0;
  wstring tmpstr,tmpstr2,tmpstr3,tmpstr4,tmpstr5,tmpstr6;
	lastBeginS1=-1;
	lastRelativePhrase=-1;
	lastQ2=-1;
	lastVerb=-1;
	lastCommand=-1;
	lastOpeningSecondaryQuote=-1;
	lastOpeningPrimaryQuote=-1;
  section=0;
  for (int I=0; I<m.size() && !exitNow; I++)
  {
		if ((int)(I*100/m.size())>lastProgressPercent)
    {
      lastProgressPercent=(int)I*100/m.size();
      wprintf(L"PROGRESS: %03d%% speakers identified with %d seconds elapsed \r",lastProgressPercent,clocksec());
    }
    debugTrace.traceSpeakerResolution=m[I].t.traceSpeakerResolution || TSROverride;
    logCache=m[I].logCache;
		// this must be done before resolveObject, because we don't want such an object to be in localObjects:
		// an object that has PLEONASTIC_SUBJECT set, yet has a relative clause with a head that has the POS role set
		//   should likely not be added to localObjects.
		// There was a man who would unerringly ferret out Tuppence's whereabouts .
		if ((m[I].objectRole&SUBJECT_PLEONASTIC_ROLE) && m[I].getObject()>=0 && objects[m[I].getObject()].whereRelativeClause>=0 &&
			  m[objects[m[I].getObject()].whereRelativeClause].relVerb>=0 && (m[m[objects[m[I].getObject()].whereRelativeClause].relVerb].quoteForwardLink&VT_POSSIBLE))
		{
			m[I].objectRole&=~SUBJECT_PLEONASTIC_ROLE;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Removed pleonastic role",I);
		}
		int element;
    if (m[I].pma.queryPattern(L"_REL1")!=-1)
			lastRelativePhrase=I;
		if (m[I].pma.queryPattern(L"_Q2")!=-1)
			lastQ2=I;
		if (m[I].pma.queryPattern(L"_COMMAND1")!=-1)
			lastCommand=I;
		if (m[I].hasVerbRelations)
			lastVerb=I;
    if ((element=m[I].pma.queryPattern(L"__S1"))!=-1)
    {
			if (endMetaResponse<I && (endMetaResponse=detectMetaResponse(I,element&~patternFlag))<0)
			{
				lastBeginS1=I;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d     aging speakers (%s) BeginS1",I,section,(inPrimaryQuote) ? L"inQuote":L"outsideQuote");
				m[I].flags|=WordMatch::flagAge;
				for (vector <cLocalFocus>::iterator lfi=localObjects.begin(); lfi!=localObjects.end(); )
					ageSpeaker(I,inPrimaryQuote,inSecondaryQuote,lfi,1);
				agingStructuresSeen++;
			}
    }
		// implicit objects - another knock
		if (implicitObject(I))
		{
			objects[m[I].getObject()].neuter=false;
			objects[m[I].getObject()].male=objects[m[I].getObject()].female=true;
			objects[m[I].getObject()].objectClass=GENDERED_GENERAL_OBJECT_CLASS;
		}
		associateNyms(I); // (ASSOC)
		associatePossessions(I);
		setPOVStatus(I,inPrimaryQuote,inSecondaryQuote);
		// detect whether the name is possibly a hail candidate, and possibly needs to be resolved with other former names before being detected as a hail
		// “[st:dr] Miss Finn , ” he said
		// “[st:julius] Mr . Hersheimmer , ” he[st] said at last , “[st:julius] that is a very large sum . ” 
		bool possibleHail=inPrimaryQuote && m[I].getObject()>=0 && !(m[I].objectRole&HAIL_ROLE) && objects[m[I].getObject()].objectClass==NAME_OBJECT_CLASS && 
					m[I].beginObjectPosition && m[m[I].beginObjectPosition-1].word->first==L"“" &&	m[m[I].endObjectPosition].word->first==L"," && m[m[I].endObjectPosition+1].word->first==L"”";
    if ((!inPrimaryQuote && !inSecondaryQuote) || (possibleHail && !objects[m[I].getObject()].PISDefinite))
      resolveObject(I,false,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,false,false,false); // could change object at I!
		if (m[I].getObject()!=UNKNOWN_OBJECT && lastCommand>=0)
			m[I].objectRole|=IN_COMMAND_OBJECT_ROLE;
		if (!narrativeIsQuoted)
			adjustHailRoleDuringScan(I);
		if (m[I].objectMatches.size()==1 && m[I].getObject()>=0) 
			moveNyms(I,m[I].objectMatches[0].object,m[I].getObject(),L"identifySpeakerGroups");
    identifyMetaNameEquivalence(I,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb);
    identifyMetaSpeaker(I,inPrimaryQuote|inSecondaryQuote);
    identifyAnnounce(I,inPrimaryQuote|inSecondaryQuote);
    identifyMetaGroup(I,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb);
		// Wikipedia
    if (!inPrimaryQuote && !inSecondaryQuote && sourceType!=PATTERN_TRANSFORM_TYPE)
		  identifyISARelation(I,true);
		detectTimeTransition(I,lastSubjects);
		defineObjectAsSpatial(I);
		if (inPrimaryQuote && m[I].relVerb>=0 && lastBeginS1>lastRelativePhrase && (m[I].objectRole&(OBJECT_ROLE|SUBJECT_ROLE))>0)
		{
			bool isSubject=(m[I].objectRole&(OBJECT_ROLE|SUBJECT_ROLE))==SUBJECT_ROLE; // only count the verb tense once
			// quoteForwardLink is overloaded with tsSense only for verbs
			int tsSense=m[m[I].relVerb].quoteForwardLink;
			//lplog(LOG_RESOLUTION,L"%06d:L Sense %s",I,senseString(tmpstr,tsSense).c_str());
			if ((tsSense&VT_TENSE_MASK)==VT_PAST || (tsSense&VT_TENSE_MASK)==VT_PAST_PERFECT)
			{
				if (isSubject)
					numPastSinceLastQuote++;
				int person=m[I].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON|THIRD_PERSON);
				bool plural=(m[I].word->second.inflectionFlags&PLURAL)==PLURAL;
				if (((person&FIRST_PERSON) && plural) || (person==FIRST_PERSON))  // I, we
					numFirstInQuote++;
				if (m[I].word->first==L"you")  // you (not we)
				{
					numSecondInQuote++;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:L past you detected verb %d:%s",I,m[I].relVerb,senseString(tmpstr,tsSense).c_str());
				}
			}
			else if (isSubject)
				numNonPastSinceLastQuote++;
		}
    int o=(m[I].objectMatches.size()==1) ? m[I].objectMatches[0].object : m[I].getObject();
		if (!currentIsQuestion && !(m[I].flags&WordMatch::flagAdjectivalObject) && !(m[I].flags&WordMatch::flagRelativeHead) &&
			!(m[I].getObject()<0 || !objects[m[I].getObject()].plural || objects[m[I].getObject()].male || objects[m[I].getObject()].female || !objects[m[I].getObject()].neuter || objects[m[I].getObject()].objectClass==BODY_OBJECT_CLASS) &&
				debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:rejected all objects matched to %s for speaker focus.",I,objectString(m[I].getObject(),tmpstr,false).c_str());
		if (!currentIsQuestion && !(m[I].flags&WordMatch::flagAdjectivalObject) && !(m[I].flags&WordMatch::flagRelativeHead) &&
			  // don't introduce gendered objects that have matched plural neuter objects (pictures)
			  (m[I].getObject()<0 || !objects[m[I].getObject()].plural || objects[m[I].getObject()].male || objects[m[I].getObject()].female || !objects[m[I].getObject()].neuter || objects[m[I].getObject()].objectClass==BODY_OBJECT_CLASS))
    {
			bool clearBeforeSet=true,genderedSubjectOccurred=false;
      if (o>=0 && m[I].objectMatches.empty())
      {
        genderedSubjectOccurred=mergeFocus(inPrimaryQuote,inSecondaryQuote,o,I,lastSubjects,clearBeforeSet);
        if (objects[o].objectClass==NAME_OBJECT_CLASS) mergeName(I,o,tempSpeakerGroup.speakers);
      }
      else
        for(unsigned int s=0; s<m[I].objectMatches.size(); s++)
          genderedSubjectOccurred|=mergeFocus(inPrimaryQuote,inSecondaryQuote,m[I].objectMatches[s].object,I,lastSubjects,clearBeforeSet);
			// gendered body objects can still match 'it', so genderedSubjectOccurred could still be set to true, but it should be false
			if (o>=0 && objects[o].neuter && !objects[o].male && !objects[o].female)
				genderedSubjectOccurred=false;
			if (whereFirstSubjectInParagraph==-1 && genderedSubjectOccurred)
			{
				whereFirstSubjectInParagraph=I;
	      if (debugTrace.traceSpeakerResolution)
		      lplog(LOG_RESOLUTION,L"%06d:QXQ whereFirstSubjectInParagraph set to %d.",I,whereFirstSubjectInParagraph);
			}
    }
    if (!inPrimaryQuote && !inSecondaryQuote) // avoid accumulating groups of objects in quotes that have not been disambiguated or resolved yet (and might not match or be invalid)
			accumulateGroups(I,groupedObjects,lastWhereMPluralGroupedObject);
    // Make sure tempSpeakerGroup has objects up-to-date
    for (set <int>::iterator q=tempSpeakerGroup.speakers.begin(),qEnd=tempSpeakerGroup.speakers.end(); q!=qEnd; )
      if (objects[*q].eliminated)
      {
        int tmp=*q;
				followObjectChain(tmp);
        tempSpeakerGroup.speakers.erase(q++);
        tempSpeakerGroup.speakers.insert(tmp);
      }
      else q++;
    if (inPrimaryQuote && o>=0 && (m[I].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE|IN_QUOTE_REFERRING_AUDIENCE_ROLE)) && 
			  !(m[I].flags&WordMatch::flagAdjectivalObject) &&
        objects[o].objectClass==NAME_OBJECT_CLASS && !objects[o].plural)
    {
			bool appMatch=false;
			int appFirstObject=m[I].beginObjectPosition;
			if ((m[I].objectRole&HAIL_ROLE) && appFirstObject>2 && m[appFirstObject-1].word->first==L",")
			{
				appFirstObject-=2;
				while (appFirstObject && m[appFirstObject].getObject()==-1) appFirstObject--;
				if (m[appFirstObject].endObjectPosition+1==I && (appMatch=matchByAppositivity(appFirstObject,I)) && debugTrace.traceSpeakerResolution)
					lplog(LOG_SG,L"%06d:%02d     REJECTED hail %s-matched appositively to %d:%s",I,section,objectString(o,tmpstr,true).c_str(),appFirstObject,objectString(m[appFirstObject].getObject(),tmpstr2,true).c_str());
			}
			if (!appMatch)
			{
				resolveObject(I,true,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
				if (m[I].objectMatches.empty())
				{
					if (!objects[o].name.justHonorific() || m[I].queryForm(L"pinr")<0)
					{
						o=m[I].getObject();
						if (section<sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(o);
						bool inserted=(unMergable(I,o,tempSpeakerGroup.speakers,uniquelyMergable,true,false,false,false,stsi));
						vector <cLocalFocus>::iterator lsi=in(o);
						if (debugTrace.traceSpeakerResolution && lsi!=localObjects.end())
							lplog(LOG_SG,L"%06d:%02d     hail %s %s [%d %d]",I,section,objectString(o,tmpstr,true).c_str(),(inserted) ? L"inserted":L"merged",lsi->lastWhere,lsi->previousWhere);
						objects[o].PISHail++;
						// make sure this hail is not deleted afterward because of lack of definite references
						if (m[I].objectRole&(IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE|IN_QUOTE_REFERRING_AUDIENCE_ROLE))
							objects[o].PISDefinite++;
					}
					definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(I);
					objects[o].numDefinitelyIdentifiedAsSpeaker++;
					objects[o].numDefinitelyIdentifiedAsSpeakerInSection++;
					if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection+objects[o].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
						sections[section].speakerObjects.push_back(cOM(o,SALIENCE_THRESHOLD));
				}
				else
					for (vector <cOM>::iterator omi=m[I].objectMatches.begin(); omi!=m[I].objectMatches.end(); omi++)
					{
						o=omi->object;
						if (section<sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(o);
						bool inserted=(unMergable(-1,o,tempSpeakerGroup.speakers,uniquelyMergable,true,false,false,false,stsi));
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_SG,L"%06d:%02d     hail %s %s",I,section,objectString(o,tmpstr,true).c_str(),(inserted) ? L"inserted":L"merged");
						objects[o].PISHail++;
						if (m[I].objectMatches.size()==1)
						{
							definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(I);
							objects[o].numDefinitelyIdentifiedAsSpeaker++;
							objects[o].numDefinitelyIdentifiedAsSpeakerInSection++;
							if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection+objects[o].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
								sections[section].speakerObjects.push_back(cOM(o,SALIENCE_THRESHOLD));
						}
					}
			}
    }
		if (I<endMetaResponse) continue;
		if (m[I].word->first==L"lptable" || m[I].word->first==L"lpendcolumn")
		{
			localObjects.clear();
			lplog(LOG_RESOLUTION,L"%06d:cleared local objects (%s)",I,m[I].word->first.c_str()); 
		}
    if (m[I].word->first==L"‘")
		{
			if (lastOpeningSecondaryQuote>=0)
			{
				m[lastOpeningSecondaryQuote].nextQuote=I;
				m[I].previousQuote=lastOpeningSecondaryQuote;
			}
      lastOpeningSecondaryQuote=I;
      inSecondaryQuote=true;
			inPrimaryQuote=false;
		}
    else if (m[I].word->first==L"’")
		{
      inSecondaryQuote=false;
			inPrimaryQuote=true;
			if (lastOpeningSecondaryQuote>=0)
			{
				m[lastOpeningSecondaryQuote].endQuote=I;
				setSecondaryQuoteString(I,secondaryQuotesResolutions);
			}
		}
    else if (m[I].word->first==L"“")
    {
			if (immediatelyAfterEndOfParagraph && lastQuote>=0)
				embeddedStory(I,numPastSinceLastQuote,numNonPastSinceLastQuote,numSecondInQuote,numFirstInQuote,
													 lastEmbeddedStory,lastEmbeddedImposedSpeakerPosition,lastSpeakerPosition);
      lastOpeningPrimaryQuote=I;
      lastSentenceEndBeforeAndNotIncludingCurrentQuote=lastSentenceEnd;
      inPrimaryQuote=true;
      quotesSeen=quotesSeenSinceLastSentence=true;
    }
    else if (m[I].word->first==L"”" && lastOpeningPrimaryQuote>=0)
    {
			m[lastOpeningPrimaryQuote].endQuote=I;
      inPrimaryQuote=false;
			// if the previous end quote was inserted, and this end quote was also inserted, then this quote has the same speakers as the last
			bool previousEndQuoteInserted=previousPrimaryQuote>=0 && (m[m[previousPrimaryQuote].endQuote].flags&WordMatch::flagInsertedQuote)!=0 && (m[I].flags&WordMatch::flagInsertedQuote)!=0;
      bool noTextBeforeOrAfter,quotedStringSeen=false,multipleQuoteInSentence=(previousPrimaryQuote>lastSentenceEndBeforeAndNotIncludingCurrentQuote),noSpeakerAfterward=false;
      if (!(quotedStringSeen=quotedString(lastOpeningPrimaryQuote,I,noTextBeforeOrAfter,noSpeakerAfterward)))
      {
        if (!multipleQuoteInSentence && immediatelyAfterEndOfParagraph && !previousEndQuoteInserted)
        {
					int audienceObjectPosition=-1;
					bool definitelySpeaker=false;
					lastSpeakerPosition=determineSpeaker(lastOpeningPrimaryQuote,I,true,noSpeakerAfterward,definitelySpeaker,audienceObjectPosition);
					// the boots of Albert
					if (lastSpeakerPosition>=0 && m[lastSpeakerPosition].getObject()>=0 && objects[m[lastSpeakerPosition].getObject()].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS &&
						  objects[m[lastSpeakerPosition].getObject()].ownerWhere>=0 && m[objects[m[lastSpeakerPosition].getObject()].ownerWhere].getObject()>=0 && 
							(objects[m[objects[m[lastSpeakerPosition].getObject()].ownerWhere].getObject()].male || objects[m[objects[m[lastSpeakerPosition].getObject()].ownerWhere].getObject()].female))
						lastSpeakerPosition=objects[m[lastSpeakerPosition].getObject()].ownerWhere;						  
					if (lastSpeakerPosition>=0)
          {
						int definiteSpeakerUnnecessary=-1;
            imposeSpeaker(-1,I,definiteSpeakerUnnecessary,lastSpeakerPosition,definitelySpeaker,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,false,audienceObjectPosition,lastSubjects,-1);
						if (m[lastSpeakerPosition].principalWherePosition>=0) 
							lastSpeakerPosition=m[lastSpeakerPosition].principalWherePosition;
            int speakerObject=m[lastSpeakerPosition].getObject();
            if (m[lastSpeakerPosition].objectMatches.size()==1)
              speakerObject=m[lastSpeakerPosition].objectMatches[0].object;
            if (speakerObject>=0) 
						{
							if (audienceObjectPosition<0 || m[audienceObjectPosition].queryForm(reflexiveForm)<0)
								tempSpeakerGroup.conversationalQuotes++;
							objects[speakerObject].PISDefinite++;
						}
						if (m[lastSpeakerPosition].objectMatches.empty())
							mergeObjectIntoSpeakerGroup(lastSpeakerPosition,speakerObject);
						else
						{
							for (unsigned int s=0; s<m[lastSpeakerPosition].objectMatches.size(); s++)
								mergeObjectIntoSpeakerGroup(lastSpeakerPosition,m[lastSpeakerPosition].objectMatches[s].object);
						}
          }
          else if ((m[I].flags&WordMatch::flagInsertedQuote)==0) // definitely not a quoted string if it is completed only by an inserted quote.
					{
						// but it is an inserted quote from the speaker before the definite speaker.
						// “[tuppence:julius] Well , luckily for me[tuppence] , I[tuppence] pitched down into a good soft bed of earth -- but it[bed,earth] put me[tuppence] out of action for the time[time] , sure enough .
						// no speaker seen.  Insert last subject into speaker group (to make sure it is included as a possibility in the next stage)
            quotedStringSeen|=(!noTextBeforeOrAfter); 
						if (firstQuotedSentenceOfSpeakerGroupNotSeen)
							for (unsigned int ls=0; ls<subjectsInPreviousUnquotedSection.size(); ls++)
								mergeObjectIntoSpeakerGroup(lastSpeakerPosition,subjectsInPreviousUnquotedSection[ls]);
						firstQuotedSentenceOfSpeakerGroupNotSeen=false;
						if (audienceObjectPosition>=0 && m[audienceObjectPosition].audienceObjectMatches.size()<=1)
						{
							definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(audienceObjectPosition);
							int audienceObject=(m[audienceObjectPosition].audienceObjectMatches.size()) ? m[audienceObjectPosition].audienceObjectMatches[0].object : m[audienceObjectPosition].getObject();
							objects[audienceObject].numDefinitelyIdentifiedAsSpeaker++;
							objects[audienceObject].numDefinitelyIdentifiedAsSpeakerInSection++;
							if (objects[audienceObject].numDefinitelyIdentifiedAsSpeakerInSection+objects[audienceObject].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
								sections[section].speakerObjects.push_back(cOM(audienceObject,SALIENCE_THRESHOLD));
						}
					}
					// is there a close quote immediately before the opening quote?  If so, increase conversationalQuotes.
					if (previousPrimaryQuote>=0 && lastOpeningPrimaryQuote-m[previousPrimaryQuote].endQuote<=2) 
						tempSpeakerGroup.conversationalQuotes++;
				}
				else if (!multipleQuoteInSentence && immediatelyAfterEndOfParagraph)
					lastSpeakerPosition=-1; // if inserted quote, lastSpeakerPosition should be unknown (see quote below) - this quote is after a definite speaker
			}
      if (lastQuotedString>lastSentenceEndBeforeAndNotIncludingCurrentQuote)
        quotedStringSeen=true;
      if (quotedStringSeen)
      {
        m[lastOpeningPrimaryQuote].flags|=WordMatch::flagQuotedString;
        lastQuotedString=lastOpeningPrimaryQuote;
      }
      else
      {
        if ((multipleQuoteInSentence || !immediatelyAfterEndOfParagraph || previousEndQuoteInserted) && previousPrimaryQuote>=0)
        {
					if (previousPrimaryQuote!=lastOpeningPrimaryQuote)
					{
						m[previousPrimaryQuote].quoteForwardLink=lastOpeningPrimaryQuote; // resolve unknown speakers
						m[lastOpeningPrimaryQuote].quoteBackLink=previousPrimaryQuote; // resolve unknown speakers
					}
					else
						lplog(LOG_ERROR,L"%06d:previousPrimaryQuote and lastOpeningPrimaryQuote are the same (1)!",previousPrimaryQuote);
					if (m[previousPrimaryQuote].flags&WordMatch::flagEmbeddedStoryResolveSpeakers)
						m[lastOpeningPrimaryQuote].flags|=WordMatch::flagEmbeddedStoryResolveSpeakers;
          if (debugTrace.traceSpeakerResolution)
          {
            wstring reason;
            if (multipleQuoteInSentence) reason=L"multipleQuoteInSentence previousPrimaryQuote="+itos(previousPrimaryQuote,tmpstr)+L" > lastSentenceEndBeforeAndNotIncludingCurrentQuote="+itos(lastSentenceEndBeforeAndNotIncludingCurrentQuote,tmpstr2);
            if (!immediatelyAfterEndOfParagraph) reason+=L"!immediatelyAfterEndOfParagraph ";
            if (previousEndQuoteInserted) reason+=L"inserted quote ";
					  if (debugTrace.traceSpeakerResolution)
						  lplog(LOG_SG,L"%d:quote at %d linked forward to %d (%s)",lastOpeningPrimaryQuote,previousPrimaryQuote,lastOpeningPrimaryQuote,reason.c_str());
          }
        }
        if (!multipleQuoteInSentence && immediatelyAfterEndOfParagraph && !previousEndQuoteInserted) // include inserted quotes
				{
					if (firstQuote==-1)
						firstQuote=lastOpeningPrimaryQuote;
					if (lastQuote!=-1)
					{
						m[lastQuote].nextQuote=lastOpeningPrimaryQuote;
						m[lastOpeningPrimaryQuote].previousQuote=lastQuote;
					}
					lastQuote=lastOpeningPrimaryQuote;
				}
        // remove any objects referring to a quote which is not a quoted string
        for (; quotedObjectCounter<(int)objects.size(); quotedObjectCounter++)
          if (objects[quotedObjectCounter].begin==lastOpeningPrimaryQuote)
          {
            if (debugTrace.traceObjectResolution)
              lplog(LOG_SG,L"%06d:%02d     object %s eliminated because it includes a primary quote",
                lastOpeningPrimaryQuote,section,objectString(quotedObjectCounter,tmpstr,true).c_str());
            objects[quotedObjectCounter].eliminated=true;
            for (vector <cSection>::iterator is=sections.begin(),isEnd=sections.end(); is!=isEnd; is++)
              is->preIdentifiedSpeakerObjects.erase(quotedObjectCounter);
            for (vector <cSpeakerGroup>::iterator is=speakerGroups.begin(),isEnd=speakerGroups.end(); is!=isEnd; is++)
              is->speakers.erase(quotedObjectCounter);
          }
          else if (objects[quotedObjectCounter].begin>lastOpeningPrimaryQuote)
            break;
        immediatelyAfterEndOfParagraph=false;
        quotesSeenSinceLastSentence=true;
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_SG,L"%d:(2) quotesSeenSinceLastSentence set to true",I);
        previousPrimaryQuote=lastOpeningPrimaryQuote;
      }
    }
    else if (isEOS(I))
    {
			lastBeginS1=-1;
			lastRelativePhrase=-1;
			lastCommand=-1;
      endOfSentence=true;
			transitionSinceEOS=false;
			lastSentenceEnd=I;
			// use uqLastSentenceEnd so that space relations use disambiguated objects, and also that SPEAKER_ROLEs that come before their quotes are recognized
			if (!(m[I].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE)) || narrativeIsQuoted)
			{
				int lastTransitionSR=-1,keepPPSpeakerWhere=-1;
				for (int J=uqPreviousToLastSentenceEnd; J<uqLastSentenceEnd; J++)
				{
					if (!(m[J].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE)) || narrativeIsQuoted)
						detectSpaceRelation(J,I,lastSubjects); 
					vector<cSpaceRelation>::iterator sr;
					if (m[J].spaceRelation && (sr=findSpaceRelation(J))!=spaceRelations.end() && sr->where==J && sr->tft.timeTransition)
					{
						int tmpKeepPPSpeakerWhere=getSpeakersToKeep(sr);
						if (tmpKeepPPSpeakerWhere>=0 && keepPPSpeakerWhere<0)
							keepPPSpeakerWhere=tmpKeepPPSpeakerWhere;
						lastTransitionSR=J; // subsequent space relations can remove the time transition from previous ones
					}
				}
				if (lastTransitionSR!=-1)
				{
					if (keepPPSpeakerWhere>=0 && m[keepPPSpeakerWhere].objectMatches.size()>1)
						lplog(LOG_RESOLUTION,L"%06d:transition is vague - %d.",lastTransitionSR,keepPPSpeakerWhere);
					else
					{
						vector<cSpaceRelation>::iterator sr=findSpaceRelation(lastTransitionSR);
						//keepPPSpeakerWhere=getSpeakersToKeep(sr);
						if (sr!=spaceRelations.end())
							ageTransition(sr->where,true,transitionSinceEOS,sr->tft.duplicateTimeTransitionFromWhere,keepPPSpeakerWhere,lastSubjects,L"TSR 1");
					}
				}
				uqPreviousToLastSentenceEnd=uqLastSentenceEnd;
				uqLastSentenceEnd=I;
			}
			if (!inSecondaryQuote || (I+1<(signed)m.size() && m[I+1].word->first==L"’"))
				setQuestion(m.begin()+I,inPrimaryQuote,questionSpeakerLastSentence,questionSpeaker,currentIsQuestion);
			else
				setSecondaryQuestion(m.begin() + I);
			if (!inPrimaryQuote && !inSecondaryQuote && subjectsInPreviousUnquotedSectionUsableForImmediateResolution)
			{
				subjectsInPreviousUnquotedSectionUsableForImmediateResolution=false;
        if (debugTrace.traceSpeakerResolution && subjectsInPreviousUnquotedSection.size())
          lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d Cancelling subjectsInPreviousUnquotedSectionUsableForImmediateResolution",I,section);
			}
      if (!inPrimaryQuote && !agingStructuresSeen)
      {
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_SG|LOG_RESOLUTION,L"%06d:%02d     aging speakers (%s) EOS",I,section,(inPrimaryQuote) ? L"inQuote":L"outsideQuote");
				m[I].flags|=WordMatch::flagAge;
        for (vector <cLocalFocus>::iterator lfi=localObjects.begin(); lfi!=localObjects.end(); )
          ageSpeakerWithoutSpeakerInfo(I,inPrimaryQuote,inSecondaryQuote,lfi,1);
      }
      agingStructuresSeen=0;
      if (!inPrimaryQuote)
      {
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_SG,L"%d:ZXZ set previousLastSubjects=%s (previous value %s). Cleared lastSubjects",I,objectString(lastSubjects,tmpstr).c_str(),objectString(previousLastSubjects,tmpstr).c_str());
        previousLastSubjects=lastSubjects;
        lastSubjects.clear();
      }
      // look ahead - is this the last sentence in the paragraph?
      // the sentence ends with a period, or a period and a quote.
      if (I+3<(signed)m.size() &&
          m[I+1].word!=Words.sectionWord && // period
          !(m[I+1].word->first==L"”" && m[I+2].word==Words.sectionWord) && // period and quote
          !(m[I+1].word->first==L"’" && m[I+2].word->first==L"”" && m[I+3].word==Words.sectionWord)) // period, single quote and double quote
      {
        // is the period in the middle of a quote?  if then, set to true.
        // is the period not in a quote, or at the end of a quote? then set to false.
        quotesSeenSinceLastSentence=inPrimaryQuote && (m[I+1].word->first!=L"”" && (m[I+1].word->first!=L"’" && m[I+2].word->first!=L"”"));
        // in the case where a .?! is followed by a quote and a speaker designation,
        // the speaker designation does not count as a sentence.
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_SG,L"%d:(1)quotesSeenSinceLastSentence=%s",I,(quotesSeenSinceLastSentence) ? L"true":L"false");
      }
    }
    else if (m[I].word==Words.sectionWord)
    {
			if (questionSpeakerLastParagraph>=0 && whereFirstSubjectInParagraph>=0)
				questionSubjectAgreementMap[whereFirstSubjectInParagraph]=questionSpeakerLastParagraph;
      endOfSentence=false;
      immediatelyAfterEndOfParagraph=true;
			subjectsInPreviousUnquotedSectionUsableForImmediateResolution=false;
      // does the proposed end (I) of the current speaker group end between characters dialog?  If so, reject
      int nsAfter,nsSkipAfter=-1;
      for (nsAfter=I+1; nsAfter<(int)m.size() && m[nsAfter].word==Words.sectionWord; nsAfter++);
			// skip / another voice[boris] which Tommy rather thought was that of Boris replied :
			bool metaResponseDetected=false;
	    if (nsAfter!=m.size() && (element=m[nsAfter].pma.queryPattern(L"__S1"))!=-1)
				metaResponseDetected=(nsSkipAfter=detectMetaResponse(nsAfter,element&~patternFlag))>=0;
			if (metaResponseDetected) nsAfter=nsSkipAfter;
			bool block=blockSpeakerGroupCreation(I,quotesSeenSinceLastSentence,nsAfter);
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_SG,L"%d-%d:--------%s-------------------\n nextNarrationSubjects=[%s]\n nextISNarrationSubjects=[%s]\n endOfSection=[false]\n lastSpeakerGroupOfPreviousSection=#%03d of %03d\n lastSpeakerGroup=[%s]\n currentSpeakerGroup=[%s]\n subjectsInPreviousUnquotedSection=[%s]",
						  lastSpeakerGroupPositionConsidered,I,(block) ? L"BLOCKED":L"-------",objectString(nextNarrationSubjects,tmpstr2).c_str(),objectString(nextISNarrationSubjects,tmpstr6).c_str(),
							lastSpeakerGroupOfPreviousSection,speakerGroups.size(),(speakerGroups.size()) ? toText(speakerGroups[speakerGroups.size()-1],tmpstr3):L"",toText(tempSpeakerGroup,tmpstr4),objectString(subjectsInPreviousUnquotedSection,tmpstr5).c_str());
			speakerSections.push_back(lastSpeakerGroupPositionConsidered);
			cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || inPrimaryQuote,true,objectToBeMatchedInQuote,quoteIndependentAge);
			if (speakerGroups.size())
				for (set <int>::iterator s=speakerGroups[speakerGroups.size()-1].speakers.begin(); s!=speakerGroups[speakerGroups.size()-1].speakers.end(); s++)
				{
					speakerAges[*s]++;
					vector <cLocalFocus>::iterator lsi;
					if (speakerAges[*s]>10 && (lsi=in(*s))!=localObjects.end() && lsi->physicallyPresent && lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>10)
					{
				    if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION|LOG_SG,L"%06d:Setting primary speaker %s to not physically present (age %d>10, getAge=%d).",I,objectString(*s,tmpstr,true).c_str(),speakerAges[*s],lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge));
						lsi->physicallyPresent=false; // this prevents speakers from being mentioned later in the dialogue (but not actually appearing) and then not removed from the speakerGroup
					}
				}
			if (block && m[I].flags&1)
				block=false;
      if (!block && createSpeakerGroup(lastSpeakerGroupPositionConsidered,I,false,lastSpeakerGroupOfPreviousSection))
			{
			  nextNarrationSubjects.clear();
				nextISNarrationSubjects.clear();
				whereNextISNarrationSubjects.clear();
			}
			for (set <int>::iterator s=tempSpeakerGroup.speakers.begin(),sEnd=tempSpeakerGroup.speakers.end(); s!=sEnd; s++)
			{
        objects[*s].ageSinceLastSpeakerGroup++;
				if (block)
					speakerAges[*s]=0;
			}
      if (!quotesSeenSinceLastSentence)
      {
        subjectsInPreviousUnquotedSection=previousLastSubjects;
				subjectsInPreviousUnquotedSectionUsableForImmediateResolution=true;
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_SG,L"%d:ZXZ set subjectsInPreviousUnquotedSection=previousLastSubjects=%s",I,objectString(subjectsInPreviousUnquotedSection,tmpstr).c_str());
      }
			firstQuotedSentenceOfSpeakerGroupNotSeen=true;
      lastSpeakerGroupPositionConsidered=I;
      if (!inPrimaryQuote)
      {
        quotesSeenSinceLastSentence=false;
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_SG,L"%d:(4) quotesSeenSinceLastSentence set to false",I);
        quotesSeen=false;
      }
			questionSpeakerLastParagraph=questionSpeakerLastSentence;
      if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:QXQ whereFirstSubjectInParagraph,questionSpeakerLastSentence reset from %d,%d.",I,whereFirstSubjectInParagraph,questionSpeakerLastSentence);
			questionSpeakerLastSentence=-1;
			whereFirstSubjectInParagraph=-1;
			if (m[I].flags&1)
			{
				localObjects.clear();
				lplog(LOG_RESOLUTION,L"%06d:cleared local objects",I); 
			}
    }
    if ((section+1<sections.size() && I==sections[section+1].begin) || (section<sections.size() && I==m.size()-1))
    {
      if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%02d     aging speakers %s End Of Section",I,section,(currentSpeakerGroup==0) ? L"":objectString(speakerGroups[currentSpeakerGroup-1].speakers,tmpstr).c_str());
			for (vector <cLocalFocus>::iterator lfi=localObjects.begin(); lfi!=localObjects.end(); lfi++)
			{
				m[I].flags|=WordMatch::flagAge;
				lfi->numEncounters=lfi->numDefinitelyIdentifiedAsSpeaker=0;
				lfi->increaseAge(5);
				if (lfi->numIdentifiedAsSpeaker)
					lfi->numIdentifiedAsSpeaker=1;
			}
      for (set <int>::iterator s=sections[section].preIdentifiedSpeakerObjects.begin(); s!=sections[section].preIdentifiedSpeakerObjects.end(); )
      {
        if (objects[*s].PISHail && !objects[*s].PISDefinite && !objects[*s].PISSubject)
        {
          if (debugTrace.traceSpeakerResolution)
            lplog(LOG_SG,L"%06d:%02d     hail %s deleted from preidentified",I,section,objectString(*s,tmpstr,true).c_str());
          sections[section].preIdentifiedSpeakerObjects.erase(s++);
        }
        else
          s++;
      }
		  if (debugTrace.traceSpeakerResolution)
			  lplog(LOG_SG,L"%d-%d:---------------%s---------------\n nextNarrationSubjects=[%s]\n nextISNarrationSubjects=[%s]\n endOfSection=[true]\n lastSpeakerGroupOfPreviousSection=#%03d of %03d\n lastSpeakerGroup=[%s]\n currentSpeakerGroup=[%s]\n subjectsInPreviousUnquotedSection=[%s]",
							lastSpeakerGroupPositionConsidered,I,(I==m.size()-1) ? L"EOF":L"EOS",objectString(nextNarrationSubjects,tmpstr2).c_str(),objectString(nextISNarrationSubjects,tmpstr6).c_str(),
							lastSpeakerGroupOfPreviousSection,speakerGroups.size(),(speakerGroups.size()) ? toText(speakerGroups[speakerGroups.size()-1],tmpstr3):L"",toText(tempSpeakerGroup,tmpstr4),objectString(subjectsInPreviousUnquotedSection,tmpstr5).c_str());
			speakerSections.push_back(lastSpeakerGroupPositionConsidered);
			if (speakerGroups.size())
				for (set <int>::iterator s=speakerGroups[speakerGroups.size()-1].speakers.begin(); s!=speakerGroups[speakerGroups.size()-1].speakers.end(); s++)
					speakerAges[*s]++;
      createSpeakerGroup(lastSpeakerGroupPositionConsidered,I,true,lastSpeakerGroupOfPreviousSection);
      nextNarrationSubjects.clear();
			nextISNarrationSubjects.clear();
			whereNextISNarrationSubjects.clear();
      lastSpeakerGroupPositionConsidered=I;
      subjectsInPreviousUnquotedSection.clear();
      previousLastSubjects.clear();
      lastSubjects.clear();
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_SG,L"%d:ZXZ reset subjectsInPreviousUnquotedSection, previousLastSubjects, lastSubjects etc",I);
      section++;
			clearNextSection(I,section-1);
    }
  }
	eraseAliasesAndReplacementsInSpeakerGroups();
	distributePOV();
  if (section<sections.size())
    clearNextSection(m.size(),section);
  wprintf(L"PROGRESS: 100%% speakers identified with %d seconds elapsed \n",clocksec());
  if (debugTrace.traceSpeakerResolution || TSROverride)
  {
    lplog(LOG_SG,L"SPEAKER GROUPS [LIST]");
    for (unsigned int I=0; I<speakerGroups.size(); I++)
		{
			translateBodyObjects(speakerGroups[I]);
      lplog(LOG_SG,L"%d: %s",I,toText(speakerGroups[I],tmpstr));
		}
  }
}
