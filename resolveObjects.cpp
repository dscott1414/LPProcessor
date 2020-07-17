#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "source.h"
#include "profile.h"

// for a NAME that is both male and female (uncertain) make this NAME a male if the NAME is the only NAME matching the current object.
void cSource::resolveNameGender(int where,bool male,bool female)
{ LFS
  if ((male && female) || (!male && !female)) return;
  int onlyOneName=-1;
  for (unsigned int om=0; om<localObjects.size(); om++)
    if (localObjects[om].includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) &&
        objects[localObjects[om].om.object].objectClass==NAME_OBJECT_CLASS && localObjects[om].om.salienceFactor>500)
    {
      if (!objects[localObjects[om].om.object].male || !objects[localObjects[om].om.object].female)
      {
        if ((objects[localObjects[om].om.object].male && male) || (objects[localObjects[om].om.object].female && female))
          return; // if there is one unambiguous name that matches this criteria, return.
        continue; // only catch ambiguous names
      }
      if (onlyOneName!=-1) return; // only one ambiguous name allowed
      onlyOneName=om;
    }
  if (onlyOneName==-1) return;
	bool ambiguousGender=objects[localObjects[onlyOneName].om.object].female && objects[localObjects[onlyOneName].om.object].male;
  if (male)
    objects[localObjects[onlyOneName].om.object].female=false;
  if (female)
    objects[localObjects[onlyOneName].om.object].male=false;
	if (ambiguousGender)
	{
		addDefaultGenderedAssociatedNouns(localObjects[onlyOneName].om.object);
		objects[localObjects[onlyOneName].om.object].genderNarrowed=true;
	}
  if (debugTrace.traceSpeakerResolution && ambiguousGender)
  {
    wstring tmpstr;
    lplog(LOG_RESOLUTION,L"%06d:Resolution of %s narrowed to %s",where,objectString(localObjects[onlyOneName].om,tmpstr,true).c_str(),
      (male) ? L"male" : L"female");
  }
}

bool cSource::resolveNonGenderedGeneralObjectAgainstOneObject(int where,vector <cObject>::iterator object,vector <cOM> &objectMatches,int o,int sf,int lastWhere,int &mostRecentMatch)
{ LFS
  vector <cObject>::iterator genS=objects.begin() + o;
	// don't match anything to time objects or location objects
	if (genS->isTimeObject) return false;
	// don't match "being" with "being a Russian or a Pole"
	if (genS->objectClass==VERB_OBJECT_CLASS) return false;
	// Don't match "the War Office" with "the War"
	int ol=(genS->objectClass==NAME_OBJECT_CLASS || genS->objectClass==NON_GENDERED_NAME_OBJECT_CLASS) ? genS->end-1 : genS->originalLocation;
	int ol2=(object->objectClass==NAME_OBJECT_CLASS || object->objectClass==NON_GENDERED_NAME_OBJECT_CLASS) ? object->end-1 : object->originalLocation;
	// match IBM against International Business Machines or SNL against Saturday Night Live [o=International Business Machines, object=IBM]
	if (object->end-object->begin==1 && genS->end-genS->begin>1 && m[object->originalLocation].word->first.length()>1 && (m[object->originalLocation].flags&WordMatch::flagAllCaps))
	{
		int beginS=m[genS->originalLocation].beginObjectPosition,endS=m[genS->originalLocation].endObjectPosition,firstContinuousWordCapitalized=-1,continuousWordsCapitalized=0;
		for (int I=beginS; I<endS; I++)
			if (m[I].queryForm(PROPER_NOUN_FORM_NUM)>=0 || (m[I].flags&WordMatch::flagFirstLetterCapitalized) || (m[I].flags&WordMatch::flagAllCaps))
			{
				continuousWordsCapitalized++;
				if (firstContinuousWordCapitalized<0)
					firstContinuousWordCapitalized=I;
			}
			else if (firstContinuousWordCapitalized>=0)
			{
				continuousWordsCapitalized=0;
				firstContinuousWordCapitalized=-1;
			}
		if (continuousWordsCapitalized==m[object->originalLocation].word->first.length())
		{
			bool firstLetterMatch=true;
			for (int I=firstContinuousWordCapitalized,letter=0; I<endS && firstLetterMatch; I++,letter++)
				firstLetterMatch=(m[object->originalLocation].word->first[letter]==m[I].word->first[0]);
			if (firstLetterMatch && (mostRecentMatch==-1 || mostRecentMatch<lastWhere))
			{
				if (mostRecentMatch!=-1)
					objectMatches.erase(objectMatches.begin()+objectMatches.size()-1);
				objectMatches.push_back(cOM(o,sf));
				mostRecentMatch=lastWhere;
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr,tmpstr2;
					lplog(LOG_RESOLUTION,L"%06d:Capitalization resolution mapped %s to (unknown) %s.",where,
						objectString(object,tmpstr,true).c_str(),objectString(o,tmpstr2,false).c_str());
				}
				return true;
			}
		}
	}
	//object parent[show's[62-63][62][nongen][N]]:(primary match:show[show]) child [Saturday Night Live[26-29][26][ngname][N][WikiWork]] NO MATCH
	// PARENT [o]=Saturday Night Live[36-39][36][ngname][N] Child [object]=show's[72-73][72][nongen][N]
	// 000072:added local object show's[72-73][72][nongen][N] chooseBest (1) notPP.
	// 000072:Unknown resolution mapped show's to (unknown) Saturday Night Live[36-39][36][ngname][N].
	//		"%06d:Unknown resolution mapped %s to (unknown) %s.",where,objectString(object,tmpstr,true).c_str(),objectString(o,tmpstr2,false).c_str());
	bool synonym=false;
	int semanticMismatch=0;
	if (genS->objectClass == NON_GENDERED_NAME_OBJECT_CLASS && object->objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
			(m[object->begin].word->first==L"the" || (object->begin>0 && m[object->begin-1].word->first==L"the")) &&
		checkParticularPartSemanticMatch(LOG_RESOLUTION,object->originalLocation,this,genS->originalLocation,(int)(genS-objects.begin()),synonym,semanticMismatch)<CONFIDENCE_NOMATCH && 
		(mostRecentMatch==-1 || mostRecentMatch<lastWhere))
	{
		if (mostRecentMatch!=-1)
			objectMatches.erase(objectMatches.begin()+objectMatches.size()-1);
		objectMatches.push_back(cOM(o,sf));
		mostRecentMatch=lastWhere;
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr,tmpstr2;
			lplog(LOG_RESOLUTION,L"%06d:Semantic resolution mapped %s to (unknown) %s.",where,
				objectString(object,tmpstr,true).c_str(),objectString(o,tmpstr2,false).c_str());
		}
		return true;
	}
	// but match Esthonia Glassware with Esthonia Glassware Co.
	if (object->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && genS->objectClass==NON_GENDERED_BUSINESS_OBJECT_CLASS)
	{
	  // match from the first capitalized word to originalLocation
		int begin=genS->originalLocation,begin2=object->originalLocation;
		bool match=false;
		for (; begin<genS->end && m[begin].pma.queryPattern(L"_BUS_ABB")==-1 && (match=m[begin].word==m[begin2].word); begin++,begin2++);
		if (!match) return false;
	}
	// but match Esthonia Glassware with Esthonia Glassware Co.
	else if (genS->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && object->objectClass==NON_GENDERED_BUSINESS_OBJECT_CLASS)
	{
	  // match from the first capitalized word to originalLocation
		int begin=object->originalLocation,begin2=genS->originalLocation;
		bool match=false;
		for (; begin<object->end && m[begin].pma.queryPattern(L"_BUS_ABB")==-1 && (match=m[begin].word==m[begin2].word); begin++,begin2++);
		if (!match) return false;
	}
	// originalLocation is the first capitalized word in the object (for NON_GENDERED_NAME_OBJECT_CLASS)
	// match the Ritz against the Ritz Hotel
	// also match the Mansions against South Audley Mansions
	else if (genS->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && object->objectClass==NON_GENDERED_NAME_OBJECT_CLASS &&
		       genS->getSubType()==object->getSubType() && genS->getSubType()>=0 && abs((genS->end-genS->originalLocation)-(object->end-object->originalLocation))==1)
	{
		int begin=object->originalLocation,begin2=genS->originalLocation;
		bool match=false;
		for (; begin<object->end && begin2<genS->end && (match=m[begin].word==m[begin2].word); begin++,begin2++);
		if (!match) return false;
	}
  else if (genS==object || m[ol].word!=m[ol2].word) return false;
	// don't match "the hall" with "Dr. Hall"
	if ((object->objectClass==NAME_OBJECT_CLASS && genS->objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS) ||
		  (genS->objectClass==NAME_OBJECT_CLASS && object->objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS))
		return false;
  if (genS->originalLocation-genS->begin>0 &&
    m[genS->begin].word->first!=L"a" && m[genS->begin].word->first!=L"the" &&
    m[genS->begin].queryForm(demonstrativeDeterminerForm)<0 &&
    m[genS->begin].queryForm(possessiveDeterminerForm)<0 &&
    m[genS->begin].queryForm(quantifierForm)<0 &&
    m[genS->begin].queryForm(numeralCardinalForm)<0) 
		return false;
  bool matchObject=false;
  if (object->originalLocation-object->begin<=1) matchObject=true;
  if (genS->originalLocation-genS->begin<=1) matchObject=true;
  if (!matchObject) return false;
	if (mostRecentMatch==-1 || mostRecentMatch<lastWhere)
	{
		if (mostRecentMatch!=-1)
			objectMatches.erase(objectMatches.begin()+objectMatches.size()-1);
		objectMatches.push_back(cOM(o,sf));
		mostRecentMatch=lastWhere;
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr,tmpstr2;
			lplog(LOG_RESOLUTION,L"%06d:Unknown resolution mapped %s to (unknown) %s.",where,
				objectString(object,tmpstr,true).c_str(),objectString(o,tmpstr2,false).c_str());
		}
	}
	return true;
}

// try harder to accurately group compound objects.
// group preferentially by gender, whether the objects are numbers,plural, or 
// if only 2 objects - if chain is not minimal (the first object endPosition!=coordinator [and]) and the first object is not an object of a preposition)
unsigned int cSource::getNumCompoundObjects(int where,int &combinantScore,wstring &combinantStr)
{ LFS
	unsigned int chainCount=0;
	int minPosition=-1,maxPosition=-1,numNumberObjects=0,numPluralObjects=0,noObjectCount=0,numVerbObjects=0;
	bool allGendered=true,allNeuter=true,nameGenderUncertainty=false;
	if (m[where].previousCompoundPartObject>=0 || m[where].nextCompoundPartObject>=0)
	{
		int opw=where;
		chainCount=1;
		if (m[opw].getObject()>=0)
		{
			if (maxPosition==-1 || opw>maxPosition)	maxPosition=opw;
			if (minPosition==-1 || opw<minPosition)	minPosition=opw;
			if (objects[m[opw].getObject()].objectClass==NAME_OBJECT_CLASS)
			{
				if (objects[m[opw].getObject()].male ^ objects[m[opw].getObject()].female)
					allNeuter=false;
				else
					nameGenderUncertainty=true;
			}
			else
			{
				if (objects[m[opw].getObject()].neuter)
					allGendered=false;
				if (objects[m[opw].getObject()].male || objects[m[opw].getObject()].female)
					allNeuter=false;
			}
			if (m[opw].endObjectPosition-m[opw].beginObjectPosition==1 && m[opw].queryWinnerForm(NUMBER_FORM_NUM)>=0)
				numNumberObjects++;
			if ((m[opw].word->second.inflectionFlags&PLURAL)==PLURAL)
				numPluralObjects++;
		}
		else
			noObjectCount++;
		if (m[opw].queryWinnerForm(verbForm)>=0)
			numVerbObjects++;
		while (m[opw].previousCompoundPartObject>=0 && chainCount<10) 
		{
			opw=m[opw].previousCompoundPartObject;
			if (m[opw].getObject()>=0)
			{
				if (maxPosition==-1 || opw>maxPosition)	maxPosition=opw;
				if (minPosition==-1 || opw<minPosition)	minPosition=opw;
				if (objects[m[opw].getObject()].objectClass==NAME_OBJECT_CLASS)
				{
					if (objects[m[opw].getObject()].male ^ objects[m[opw].getObject()].female)
						allNeuter=false;
					else
						nameGenderUncertainty=true;
				}
				else
				{
					if (objects[m[opw].getObject()].neuter)
						allGendered=false;
					if (objects[m[opw].getObject()].male || objects[m[opw].getObject()].female)
						allNeuter=false;
				}
				if (m[opw].endObjectPosition-m[opw].beginObjectPosition==1 && m[opw].queryWinnerForm(NUMBER_FORM_NUM)>=0)
					numNumberObjects++;
				if ((m[opw].word->second.inflectionFlags&PLURAL)==PLURAL)
					numPluralObjects++;
			}
			else
				noObjectCount++;
			if (m[opw].queryWinnerForm(verbForm)>=0)
				numVerbObjects++;
			chainCount++;
		}
		opw=where;
		while (m[opw].nextCompoundPartObject>=0 && chainCount<10) 
		{
			opw=m[opw].nextCompoundPartObject;
			if (m[opw].getObject()>=0)
			{
				if (maxPosition==-1 || opw>maxPosition)	maxPosition=opw;
				if (minPosition==-1 || opw<minPosition)	minPosition=opw;
				if (objects[m[opw].getObject()].objectClass==NAME_OBJECT_CLASS)
				{
					if (objects[m[opw].getObject()].male ^ objects[m[opw].getObject()].female)
						allNeuter=false;
					else
						nameGenderUncertainty=true;
				}
				else
				{
					if (objects[m[opw].getObject()].neuter)
						allGendered=false;
					if (objects[m[opw].getObject()].male || objects[m[opw].getObject()].female)
						allNeuter=false;
				}
				if (m[opw].endObjectPosition-m[opw].beginObjectPosition==1 && m[opw].queryWinnerForm(NUMBER_FORM_NUM)>=0)
					numNumberObjects++;
				if ((m[opw].word->second.inflectionFlags&PLURAL)==PLURAL)
					numPluralObjects++;
			}
			else
				noObjectCount++;
			if (m[opw].queryWinnerForm(verbForm)>=0)
				numVerbObjects++;
			chainCount++;
		}
	}
	bool mixedGender=(!allGendered && !allNeuter) && !nameGenderUncertainty;
	bool nonCombinant=(numNumberObjects!=0 && numNumberObjects!=chainCount) || (numVerbObjects!=0 && numVerbObjects!=chainCount);
	// if only 2 objects - if chain is not minimal (the first object endPosition!=coordinator [and]) and the first object is not an object of a preposition)
	nonCombinant|=(chainCount==2 && minPosition>=0 && m[minPosition].endObjectPosition>=0 && m[minPosition].endObjectPosition<(signed)m.size() && 
	          m[m[minPosition].endObjectPosition].word->first!=L"and" && 
						m[minPosition].beginObjectPosition>0 && m[m[minPosition].beginObjectPosition-1].queryWinnerForm(prepositionForm)>=0);
	int spread=maxPosition-minPosition;
	combinantStr.clear();
	wstring tmpstr;
	combinantScore=10000*chainCount;                combinantStr+=L"+OBJ_POS["+itos(combinantScore,tmpstr)+L"]";
	combinantScore+=(nonCombinant) ? -10000 : 0;    if (nonCombinant) combinantStr+=L"+NON_COM["+itos(-10000,tmpstr)+L"]";
	combinantScore+=(mixedGender) ? -500 : 0;       if (mixedGender) combinantStr+=L"+MIX_GEN["+itos(-500,tmpstr)+L"]";
	combinantScore+=-spread;                        combinantStr+=L"+SPREAD["+itos(-(signed)spread,tmpstr)+L"]";
	return chainCount;
}

void cSource::resolveNonGenderedGeneralObject(int where,vector <cObject>::iterator &object,vector <cOM> &objectMatches,int wordOrderSensitiveModifier)
{ LFS
  // mappings: OBJECT             acceptable ls mapping                               unacceptable
  //           a doctor           NONE
  //           doctors            doctors (matchExact)
  //           the doctor         a doctor, an old doctor, the old doctor,doctor's     the nurse
  //           the old doctor     a doctor, an old doctor, the doctor                  the new doctor, the nurse
  //           the doctors        the old doctors                                      the nurses
  //           the old doctors    the doctors                                          the new doctors
  //           Love (uncountable) The love, A love etc  UNIMPLEMENTED
  //           old papers         papers
  //           the doctor         Dr. Watson
  //           the nurse          Nurse Jane
  // "the" may also be demonstrative_determiner, possessive_determiner, quantifier
  // one noun phrase has adjectives and the other does not, otherwise reject.
  // compare the principal - this assumes unknown speakers have determiners.
	int ep=m[where].endObjectPosition,o;
	if (object->getOwnerWhere()<0 && ep+1<(signed)m.size() && m[ep].word->first==L"of" && 
		  m[ep+1].principalWherePosition>=0 && m[m[ep+1].principalWherePosition].getObject()>=0 &&
			objects[o=m[m[ep+1].principalWherePosition].getObject()].objectClass==NAME_OBJECT_CLASS &&
			// The chair of Aunt Amy's / The State Park of New Jersey
			((m[m[ep+1].principalWherePosition].endObjectPosition>=0 && (m[m[m[ep+1].principalWherePosition].endObjectPosition-1].flags&WordMatch::flagNounOwner)) ||
			objects[o].isLocationObject))
		object->setOwnerWhere(m[ep+1].principalWherePosition);
  wstring tmpstr,tmpstr2;
  if (object->plural)
  {
    for (unsigned int s=0; s<localObjects.size(); s++)
    {
			if (localObjects[0].om.object<=1) continue;
      //if (localObjects[s].inQuote!=inQuote) continue; GO_NEUTRAL
      vector <cObject>::iterator lso=objects.begin()+localObjects[s].om.object;
      if (lso!=object && (lso->originalLocation-lso->begin<=1 || object->originalLocation-object->begin<=1) && m[lso->originalLocation].word==m[object->originalLocation].word && lso->plural)
      {
				if (localObjects[s].lastWhere>=0 && m[localObjects[s].lastWhere].objectMatches.size() && m[localObjects[s].lastWhere].getObject()==localObjects[s].om.object)
					objectMatches=m[localObjects[s].lastWhere].objectMatches;
				else
					objectMatches.push_back(localObjects[s].om);
        if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Unknown resolution mapped %s to (unknown) %d:%s [plural mapping].",where,
            objectString(object,tmpstr,true).c_str(),localObjects[s].lastWhere,objectString(objectMatches,tmpstr2,true).c_str());
        return;
      }
    }
		// four pictures
		// search for all local objects in a group matching the # of objects in the owner.
		// if not found, search in the current and next sentence for the same.
		int groupSize=-1,latest=-1,begin,end,len,combinantScore;
		wchar_t *fromWhere=L"";
		wstring cstr;
		if (object->getOwnerWhere()<0 && (groupSize=mapNumeralCardinal(m[m[where].beginObjectPosition].word->first))>=1)
		{
			vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
			for (; lsi!=lsiEnd; lsi++)
				if (lsi->om.object>1 && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.object!=m[where].getObject() && lsi->lastWhere>=0 && getNumCompoundObjects(lsi->lastWhere,combinantScore,cstr)==groupSize)
					if (lsi->lastWhere>latest)
						latest=lsi->lastWhere;
			if (latest>=0)
			{
				int compoundLoop=0;
				while (m[latest].previousCompoundPartObject>=0) 
				{
					if (compoundLoop++>10) break;
					latest=m[latest].previousCompoundPartObject;
				}
			}
			fromWhere=L"local";
		}
		if (latest<0)
		{
			int numEOS=0,I;
			// if not found in local objects, then search in the current and next sentence
			for (I=where; numEOS<=1 && I<(signed)m.size() && (getNumCompoundObjects(I,combinantScore,cstr)!=groupSize || m[I].getObject()<0 || !objects[m[I].getObject()].matchGenderIncludingNeuter(*object)); I++)
				if (isEOS(I)) numEOS++;
			if (numEOS<=1 && I<(signed)m.size()) latest=I;
			fromWhere=L"cata";
		}
		if (latest>=0)
		{
			for (end=latest; end>=0; end=m[end].nextCompoundPartObject)
			{
				if (m[end].objectMatches.size())
				{
					objectMatches.insert(objectMatches.end(),m[end].objectMatches.begin(),m[end].objectMatches.end());
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION|LOG_SG,L"%d:Nongendered group mapped %s to %d:%s [%s].",where,
							objectString(object,tmpstr,true).c_str(),end,objectString(m[end].objectMatches,tmpstr2,true).c_str(),fromWhere);
				}
				else if (m[end].getObject()>=0)
				{
					objectMatches.push_back(cOM(m[end].getObject(),SALIENCE_THRESHOLD));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION|LOG_SG,L"%d:Nongendered group mapped %s to %d:%s [%s].",where,
							objectString(object,tmpstr,true).c_str(),end,objectString(m[end].getObject(),tmpstr2,true).c_str(),fromWhere);
				}
			}
			if (m[begin=latest].pma.queryPattern(L"__MNOUN",len)!=-1)
				end=begin+len;
			for (int I=begin; I<=end; I++)
			{
				if (m[I].getObject()<0) continue;
				if (m[I].objectMatches.size())
				{
					for (unsigned int J=0; J<m[I].objectMatches.size(); J++)
						introducedByReference.push_back(m[I].objectMatches[J].object);
				}
				else if (m[I].getObject()>=0)
					introducedByReference.push_back(m[I].getObject());
				I=m[I].endObjectPosition-1;
			}
		}
  }
	// the pity on Mr. Carter's face
	int ww;
	if (isFacialExpression(where) && 
		  (m[where].objectRole&(SUBJECT_ROLE|PREP_OBJECT_ROLE))==SUBJECT_ROLE &&
			m[m[where].endObjectPosition].queryForm(prepositionForm)>=0 && 
			(ww=m[m[where].endObjectPosition+1].principalWherePosition)>=0 && m[ww].getObject()>=0 &&
		  (m[ww].word->first==L"face" || (m[ww].word->second.mainEntry!=wNULL && m[ww].word->second.mainEntry->first==L"eye")) && 
	    (ww=objects[m[ww].getObject()].getOwnerWhere())>=0 && m[ww].getObject()>=0)
	{
		objectMatches.push_back(cOM(m[ww].getObject(),SALIENCE_THRESHOLD));
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:Unknown resolution mapped %s to (unknown) %d:%s [expression].",where,
        objectString(object,tmpstr,true).c_str(),ww,objectString(m[ww].getObject(),tmpstr2,true).c_str());
		return;
	}
	// No. 27 - does it match an address?
	if (m[m[where].beginObjectPosition].pma.queryPatternDiff(L"__NOUN",L"Q")!=-1)
	{
		tIWMM numWord=m[where].word;
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
		for (; lsi!=lsiEnd; lsi++)
    {
			if (lsi->om.object<=1) continue;
      vector <cObject>::iterator lso=objects.begin()+lsi->om.object;
			if (lso!=object && !lso->isTimeObject)
      {
				int lsoi;
				for (lsoi=lso->begin; lsoi<lso->end && m[lsoi].word!=numWord; lsoi++);
				if (lsoi!=lso->end)
				{
					if (lsi->lastWhere>=0 && m[lsi->lastWhere].objectMatches.size() && m[lsi->lastWhere].getObject()==lsi->om.object)
						objectMatches=m[lsi->lastWhere].objectMatches;
					else
						objectMatches.push_back(lsi->om);
					lplog(LOG_RESOLUTION,L"%06d:Unknown resolution mapped %s to (unknown) %d:%s [num/address mapping].",where,
								objectString(object,tmpstr,true).c_str(),lsi->lastWhere,objectString(lsi->om,tmpstr2,true).c_str());
				}
      }
			if (objectMatches.empty())
				for (set<int>::iterator s=relatedObjectsMap[numWord].begin(),end=relatedObjectsMap[numWord].end(); s!=end; s++)
				{
					int ro=*s;
					if (objects[ro].eliminated)
						followObjectChain(ro);
					vector <cObject>::iterator rObject=objects.begin()+ro;
					if (rObject!=object && rObject->firstLocation<where && !rObject->eliminated && !rObject->isTimeObject &&
							(rObject->objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS || rObject->objectClass==NON_GENDERED_NAME_OBJECT_CLASS))
						objectMatches.push_back(cOM(ro,SALIENCE_THRESHOLD));
				}
			if (debugTrace.traceSpeakerResolution && objectMatches.size())
				lplog(LOG_RESOLUTION,L"%06d:Unknown resolution mapped %s to (unknown) %s [num/address mapping 2].",where,
					objectString(object,tmpstr,true).c_str(),objectString(objectMatches,tmpstr2,true).c_str());
      return;
    }
	}
  if (m[object->begin].word->first==L"the" || m[object->begin].queryForm(demonstrativeDeterminerForm)>=0 ||
			m[object->begin].queryForm(possessiveDeterminerForm)>=0 || m[object->begin].queryForm(quantifierForm)>=0 ||
			object->begin==object->originalLocation)
  {
		int mostRecentMatch=-1;
    for (unsigned int s=0; s<localObjects.size(); s++)
			if (localObjects[s].om.object>1)
				resolveNonGenderedGeneralObjectAgainstOneObject(where,object,objectMatches,localObjects[s].om.object,localObjects[s].om.salienceFactor,localObjects[s].lastWhere,mostRecentMatch);
		// match against aliases
		if (objectMatches.empty())
		{
	    for (unsigned int s=0; s<localObjects.size(); s++)
				if (localObjects[s].om.object>1)
					for (vector <int>::iterator a=objects[localObjects[s].om.object].aliases.begin(),aEnd=objects[localObjects[s].om.object].aliases.end(); a!=aEnd; a++)
						if (*a>1)
							resolveNonGenderedGeneralObjectAgainstOneObject(where,object,objectMatches,*a,SALIENCE_THRESHOLD,locationBefore(*a,where),mostRecentMatch);
    }
  }
	// a pensionnat? - refers to "pensionnats" in the last speaker's utterance
	else if (m[object->begin].word->first==L"a" && (m[where].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0 && (m[where].flags&WordMatch::flagInQuestion))
	{
		tIWMM word=m[where].word;
		// search in local objects for a plural of the word - covers cases that wouldn't be covered in normal match
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
		for (; lsi!=lsiEnd; lsi++)
			if (lsi->om.object>1 && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && objects[lsi->om.object].plural && lsi->lastWhere>=0 && m[lsi->lastWhere].word->second.mainEntry==word)
			{
				objectMatches.push_back(cOM(lsi->om.object,SALIENCE_THRESHOLD));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Unknown unique singular mapped %s to (unknown plural) %d:%s.",where,
						objectString(object,tmpstr,true).c_str(),lsi->lastWhere,whereString(lsi->lastWhere,tmpstr2,false).c_str());
				break;
			}
	}
	if (wordOrderSensitiveModifier>=0 && objectMatches.size()>0)
	{
		if (cObject::wordOrderWords[wordOrderSensitiveModifier]==L"another")
			objectMatches.clear();
		else
		{
			vector <int> locations;
			for (unsigned int I=0; I<objectMatches.size(); I++)
				locations.push_back(locationBefore(objectMatches[I].object,where));
			int tmp=preferWordOrder(wordOrderSensitiveModifier,locations);
			if (tmp==0 && objectMatches.size()>=2)
				objectMatches.erase(objectMatches.begin()+1);
			else if (tmp==1)
				objectMatches.erase(objectMatches.begin());
			else if (tmp==-2)
				objectMatches.clear();
		}
	}
	// Curveball is an Iraqi defector
	if ((m[where].objectRole&(SUBJECT_ROLE|IS_OBJECT_ROLE))==(SUBJECT_ROLE|IS_OBJECT_ROLE) && 
		m[where].getRelObject()>=0 && m[m[where].getRelObject()].getObject()>=0 && 
		objects[m[m[where].getRelObject()].getObject()].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
	{
		o=m[where].getObject();
		if (objects[o].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS || objects[o].objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
		{
			objects[o].objectClass=GENDERED_GENERAL_OBJECT_CLASS;
			objects[o].male=objects[o].female=true;
		}
	}
}

bool cName::matchHonorifics(wstring sHon)
{ LFS
  return (hon!=wNULL && hon->first==sHon) || (hon2!=wNULL && hon2->first==sHon) || (hon3!=wNULL && hon3->first==sHon);
}

bool cName::isNull()
{ LFS
  return first==wNULL && last==wNULL && any==wNULL;
}

bool cName::isCompletelyNull()
{ LFS
  return hon==wNULL && first==wNULL && last==wNULL && any==wNULL;
}

// m[where]=="doctor"
struct tHonMap
{
  wchar_t *shortForm;
  wchar_t *longForm;
} honorificMap[] ={
  { L"dr",L"doctor" },
  { L"dr",L"doc" },
  { L"doc",L"doctor" }
};

// is an object compatible with a certain word?
bool cObject::hasAttribute(int where,vector <WordMatch> &m)
{ LFS
  if (name.matchHonorifics(m[where].word->first))
    return true;
  for (unsigned int h=0; h<sizeof(honorificMap)/sizeof(honorificMap[0]); h++)
  {
    if (m[where].word->first==honorificMap[h].longForm && name.matchHonorifics(honorificMap[h].shortForm))
			return true;
    if (m[where].word->first==honorificMap[h].longForm && m[originalLocation].word->first==honorificMap[h].shortForm)
			return true;
    if (m[where].word->first==honorificMap[h].shortForm && m[originalLocation].word->first==honorificMap[h].longForm)
			return true;
  }
  return false;
}

vector <cSource::cSpeakerGroup>::iterator cSource::containingSpeakerGroup()
{ LFS
	for (int I=0; I<(signed)speakerGroups.size(); I++)
		if (speakerGroups[I].sgBegin>=I && speakerGroups[I].sgEnd<I)
			return speakerGroups.begin()+I;
	return speakerGroups.end();
}

// match any object which has a matching attribute
// the doctor would match Dr. Watson, or a doctor//young doctor etc.
// the nurse would match Nurse Jane
// the colonel would match Colonel Wilson
// mappings: OBJECT             acceptable ls mapping                               unacceptable
//           a doctor           NONE
//           the doctor         Dr. Watson
//           the nurse          Nurse Jane
// "the" may also be demonstrative_determiner, possessive_determiner, quantifier
// one noun phrase has adjectives and the other does not, otherwise reject.
// compare the principal - this assumes unknown speakers have determiners.
bool cSource::resolveOccRoleActivityObject(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int wordOrderSensitiveModifier,bool physicallyPresent)
{ LFS
	bool chooseFromLocalFocus=false,traceThisNym=(where==13031);
	// look for localObjects having the occupation
	tIWMM fromMatch,toMatch,toMapMatch;
	wstring logMatch,word=m[where].word->first,tmpstr,tmpstr2,assa,assn;
	int o=m[where].getObject(),latestOwnerWhere=objects[o].getOwnerWhere(),numOccupationMatch=0;
	bool useGender=(object->male ^ object->female),onlyNeuter=object->neuter && !object->male && !object->female,notNeuter=!object->neuter;
	if (traceThisNym)
		lplog(LOG_RESOLUTION,L"%06d:[ROO] Object %s has associatedAdjectives (%s) associatedNouns (%s)",where,objectString(o,tmpstr,false).c_str(),
			wordString(object->associatedAdjectives,assa).c_str(),wordString(object->associatedNouns,assn).c_str());
	// look through very recent items looking for groups 
	vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
	vector < vector <cLocalFocus>::iterator > lsMatchedObjects,lsNotPPMatchedObjects;
	if (traceThisNym)
	{
		for (; lsi!=lsiEnd; lsi++)
			lplog(LOG_RESOLUTION,L"%06d:[LSI] Object %s includeInSalience=%s useGender=%s matchSex=%s onlyNeuter=%s notNeuter=%s neuter=%s PP=%s LPP=%s NNM=%s",
		  where,objectString(lsi->om.object,tmpstr,false).c_str(),
			(lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge)) ? L"true":L"false",(useGender) ? L"true":L"false",(object->matchGender(objects[lsi->om.object])) ? L"true":L"false",
			(onlyNeuter) ? L"true":L"false",(notNeuter) ? L"true":L"false",(objects[lsi->om.object].neuter) ? L"true":L"false",
			(physicallyPresent) ? L"true":L"false",(lsi->physicallyPresent) ? L"true":L"false",
			(nymNoMatch(where,object,objects.begin()+lsi->om.object,false,false,logMatch,fromMatch,toMatch,toMapMatch,L"occupation")) ? L"true":L"false");
		lsi=localObjects.begin();
	}
	for (; lsi!=lsiEnd; lsi++)
		if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.object!=o && 
			  (!useGender || object->matchGender(objects[lsi->om.object])) && 
			  (!onlyNeuter || objects[lsi->om.object].neuter) &&
				(!notNeuter || !objects[lsi->om.object].neuter) &&
			  !nymNoMatch(where,object,objects.begin()+lsi->om.object,false,false,logMatch,fromMatch,toMatch,toMapMatch,L"occupation") &&
				lsi->getTotalAge()<100 &&  // if TA>=100, this hasn't been mentioned in a long time
				(latestOwnerWhere<0 || (m[latestOwnerWhere].getObject()!=lsi->om.object && in(lsi->om.object,m[latestOwnerWhere].objectMatches)==m[latestOwnerWhere].objectMatches.end())))
		{
			if (traceThisNym)
				lplog(LOG_RESOLUTION,L"%06d:[LSI] Object %s has associatedAdjectives (%s) associatedNouns (%s)",where,objectString(lsi->om.object,tmpstr,false).c_str(),
					wordString(objects[lsi->om.object].associatedAdjectives,assa).c_str(),wordString(objects[lsi->om.object].associatedNouns,assn).c_str());
				bool explicitOccupationMatch=false;
			if (lsi->numMatchedAdjectives=nymMatch(objects.begin()+o,objects.begin()+lsi->om.object,traceThisNym,traceThisNym,explicitOccupationMatch,logMatch,fromMatch,toMatch,toMapMatch,L"occupation"))
			{
				if (!physicallyPresent || lsi->physicallyPresent)
				{
					lsMatchedObjects.push_back(lsi);
					if (lsi->numMatchedAdjectives>3 || explicitOccupationMatch) numOccupationMatch++;
				}
				else if (lsi->numMatchedAdjectives>3 || explicitOccupationMatch)
					lsNotPPMatchedObjects.push_back(lsi);
				else
			    lsi->clear();
			}
			else if (physicallyPresent && !lsi->physicallyPresent)
		    lsi->clear();
		}
		else
	    lsi->clear();
	if (!numOccupationMatch && lsNotPPMatchedObjects.size())
		lsMatchedObjects=lsNotPPMatchedObjects;
	for (vector < vector <cLocalFocus>::iterator >::iterator lsii=lsMatchedObjects.begin(),lsiiEnd=lsMatchedObjects.end(); lsii!=lsiiEnd; lsii++)
	{
		(*lsii)->om.salienceFactor=500;
		if ((*lsii)->getTotalAge()<6) (*lsii)->om.salienceFactor+=500;
		if ((*lsii)->getTotalAge()<3) (*lsii)->om.salienceFactor+=500;
		// if there are more than two cooks, choose the nearest one
		if (numOccupationMatch>=2)
			(*lsii)->numMatchedAdjectives-=(*lsii)->getTotalAge()/10; // this is perhaps a little hack to make Dr. Hall, which is just mentioned, resolve over a brisk young doctor (because young matches little)
		chooseFromLocalFocus=true;
	}
	// look ahead for the next paragraph and compile all new matching objects
	if (wordOrderSensitiveModifier>=0 && cObject::wordOrderWords[wordOrderSensitiveModifier]==L"another")
	{
		int paragraph=0;
		for (unsigned int I=where; paragraph<2 && I<m.size() && objectMatches.size()==0; I++)
		{
			if (m[I].word==Words.sectionWord) paragraph++;
			int nmo=m[I].getObject();
			// if the object is not in localObjects (and so is new) or is old and out of the current speaker section,
			//   and is the same occupation, or a person in the same profession
			if (nmo>=0 && ((lsi=in(nmo))==localObjects.end() || (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>EOS_AGE && lsi->lastWhere<speakerGroups[currentSpeakerGroup].sgBegin)) && 
				  ((objects[nmo].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && m[objects[nmo].originalLocation].word==m[object->originalLocation].word) ||
					 (objects[nmo].objectClass==NAME_OBJECT_CLASS && objects[nmo].hasAttribute(object->originalLocation,m))))
				objectMatches.push_back(cOM(nmo,SALIENCE_THRESHOLD));
		}
		return chooseFromLocalFocus;
	}
	if (wordOrderSensitiveModifier>=0)
	{
		vector <int> locations;
		for (unsigned int I=0; I<objectMatches.size(); I++)
			locations.push_back(locationBefore(objectMatches[I].object,where));
		int tmp=preferWordOrder(wordOrderSensitiveModifier,locations);
		if (tmp==0)
			objectMatches.erase(objectMatches.begin()+1);
		else if (tmp==1)
			objectMatches.erase(objectMatches.begin());
		else if (tmp==-2)
			objectMatches.clear();
	}
	// if speaker groups not established, avoid bringing distant objects into local focus without knowing context
	// also when identifying speaker groups objects mentioned in quotes are not scanned - 'a doctor' mentioned in quote will not match 'the doctor' mentioned shortly thereafter
	if (objectMatches.empty() && !chooseFromLocalFocus && speakerGroupsEstablished)
	{
		// look through ALL items looking for groups 
		int mo=0,numMatchedAdjectives,latestWhere=-1,tmpWhere,topAdjectiveMatch=2,embeddedStoryBegin=0;
		if (m[where].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE) 
		{
			embeddedStoryBegin=lastOpeningPrimaryQuote;
			while (m[embeddedStoryBegin].previousQuote>=0 && ((m[embeddedStoryBegin].flags&WordMatch::flagEmbeddedStoryResolveSpeakers) || (m[embeddedStoryBegin].flags&WordMatch::flagEmbeddedStoryResolveSpeakersGap)))
				embeddedStoryBegin=m[embeddedStoryBegin].previousQuote;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Embedded story last location %d",where,embeddedStoryBegin);
		}
		for (vector <cObject>::iterator oi=objects.begin(),oiEnd=objects.end(); oi!=oiEnd; oi++,mo++)
		{
			if (mo!=o && !oi->eliminated && oi->originalLocation<where && oi->originalLocation>=embeddedStoryBegin && 
				  // don't introduce objects that haven't been resolved yet - if they aren't unresolvable, and they don't have the IMPLICIT flag,
					// then unresolvable will be false, but this could be introduced into a speakerGroup, and then everything would merge into this object 
					// see potentiallyMergable - o2IsNonResolvable will be false, so o1->firstPhysicalManifestation<o2->begin will not apply
				  ((m[oi->originalLocation].flags&WordMatch::flagObjectResolved)!=0 || in((int)(oi-objects.begin()))!=localObjects.end()) &&  
				  (oi->objectClass==NAME_OBJECT_CLASS || 
					 (oi->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && m[oi->originalLocation].objectMatches.empty())) &&
					(!useGender || object->matchGender(*oi)) && 
					(!onlyNeuter || oi->neuter) &&
					(!notNeuter || !oi->neuter) &&
					!nymNoMatch(where,object,oi,false,false,logMatch,fromMatch,toMatch,toMapMatch,L"occupation") &&
					(latestOwnerWhere<0 || !in(mo,latestOwnerWhere)))
			{
				// if gendered occupation, then since it is not in local salience any more,
				// then there must be an overlap in speakerGroups.
				bool allIn,oneIn;
				vector <cSpeakerGroup>::iterator moSG=containingSpeakerGroup();
				if (oi->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && moSG!=speakerGroups.end() &&
					  !intersect(moSG->speakers,speakerGroups[currentSpeakerGroup].speakers,allIn,oneIn))
				{
					wstring tmpstr3;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:%s rejected speakerGroup=%d:%s currentSpeakerGroup=%d:%s",where,
							objectString(mo,tmpstr,true).c_str(),
							locationBefore(mo,where),toText(*containingSpeakerGroup(),tmpstr2),
							where,toText(speakerGroups[currentSpeakerGroup],tmpstr3));
					continue;
				}
				if (traceThisNym)
					lplog(LOG_RESOLUTION,L"%06d:[EO] Object %s has associatedAdjectives (%s) associatedNouns (%s)",where,objectString(mo,tmpstr,false).c_str(),
						wordString(oi->associatedAdjectives,assa).c_str(),wordString(oi->associatedNouns,assn).c_str());
				bool explicitOccupationMatch=false;
				numMatchedAdjectives=nymMatch(object,oi,traceThisNym,traceThisNym,explicitOccupationMatch,logMatch,fromMatch,toMatch,toMapMatch,L"occupation");
				int objectClass=oi->objectClass;
				// if a name is matched to another gendered object which has an alias, those adjectives may not be carried over
				if (objectClass==NAME_OBJECT_CLASS)
				{
					for (vector <int>::iterator alias=oi->aliases.begin(),aliasEnd=oi->aliases.end(); alias!=aliasEnd && !numMatchedAdjectives; alias++)
						if (numMatchedAdjectives=nymMatch(object,objects.begin()+*alias,traceThisNym,traceThisNym,explicitOccupationMatch,logMatch,fromMatch,toMatch,toMapMatch,L"occupation"))
						{
							objectClass=objects[*alias].objectClass;
							if (debugTrace.traceSpeakerResolution && numMatchedAdjectives>=3)
								lplog(LOG_RESOLUTION,L"%06d:resolveOccRoleActivityObject allObjects matched alias %s (matched=%d)",where,objectString(*alias,tmpstr,false).c_str(),numMatchedAdjectives);
							break;
						}
					if (abbreviationEquivalent(m[object->originalLocation].word,m[oi->originalLocation].word))
						numMatchedAdjectives-=2; // Dr. Hall matches the doctor no more than if Sam was identified as 'a doctor', so this deletes the headMatch bonus that nymMatch creates
				}
				if (objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && numMatchedAdjectives<5)
				{
					wstring tmpstr3;
					if (debugTrace.traceSpeakerResolution && numMatchedAdjectives>2)
						lplog(LOG_RESOLUTION,L"%06d:rejected %s (numMatchedAdjectives=%d)",where,
							objectString(mo,tmpstr,true).c_str(),numMatchedAdjectives);
					continue;
				}
				if (objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS) numMatchedAdjectives-=2;
				// default to the most matched and then the last mentioned before where
				tmpWhere=locationBefore(mo,where);
		    if (debugTrace.traceSpeakerResolution && numMatchedAdjectives>=3)
					lplog(LOG_RESOLUTION,L"%06d:resolveOccRoleActivityObject allObjects candidate %d:%s (matched=%d in localObjects=%s)",
								where,tmpWhere,objectString(mo,tmpstr,false).c_str(),numMatchedAdjectives,(in(mo)==localObjects.end()) ? L"false":L"true");
				if (numMatchedAdjectives>topAdjectiveMatch || (numMatchedAdjectives==topAdjectiveMatch && tmpWhere>latestWhere))
				{
					if (latestWhere<0)
						objectMatches.push_back(cOM(mo,numMatchedAdjectives*500));
					else
					{
						objectMatches[0].object=mo;
						objectMatches[0].salienceFactor=numMatchedAdjectives*500;
					}
					latestWhere=tmpWhere;
					topAdjectiveMatch=numMatchedAdjectives;
				}
			}
		}
		if (objectMatches.size())
			m[where].objectRole|=UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
		if (objectMatches.size()==1)
			narrowGender(where,objectMatches[0].object);
    if (debugTrace.traceSpeakerResolution && objectMatches.size())
			lplog(LOG_RESOLUTION,L"%06d:resolveOccRoleActivityObject allObjects matched %s",where,objectString(objectMatches,tmpstr,false).c_str());
	}
  if (objectMatches.size()==1 && objectMatches[0].object!=(object-objects.begin()))
    replaceObjectInSection(where,objectMatches[0].object,(int)(object-objects.begin()),L"resolveOccRoleActivityObject");
	return chooseFromLocalFocus;
}

//create more sophisticated gendered matching process using attributes - my cousin's house
//  If A is B's sister, then B is A's brother.  And vice-versa.
struct {
	char *r1;
	char *r2;
} symmetricRelations[]=
{
	{ "cousin","cousin" }, // Jack's cousin=Joan; Joan's cousin=Jack
	{ "sibling","sibling" },
	{ "spouse","spouse" },
	{ "relative","relative" },
	{ "relation","relation" },
  { "coworker","coworker" },
	{ "brother:sister","brother:sister" }, // each : dependent on gender
	{ "wife:husband","wife:husband" },
	{ "bride:groom","bride:groom" },
	{ "niece:nephew","uncle:aunt" }, // Jack's niece = Joan; Joan's uncle = Jack
	{ "son:daughter","father:mother" },
	{ "boss","employee" },
	{ "grandmother:grandfather","granddaughter:grandson" }
};
void cSource::resolveRelativeObject(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int wordOrderSensitiveModifier)
{ LFS
	// look ahead for the next paragraph and compile all new matching objects
	if (wordOrderSensitiveModifier>=0 && cObject::wordOrderWords[wordOrderSensitiveModifier]==L"another")
	{
		int paragraph=0;
		for (unsigned int I=where; paragraph<2 && I<m.size() && objectMatches.size()==0; I++)
		{
			if (m[I].word==Words.sectionWord) paragraph++;
			int o=m[I].getObject();
			// if the object is not in localObjects (and so is new) or is old and out of the current speaker section,
			//   and is the same occupation, or a person in the same profession
			vector <cLocalFocus>::iterator lsi;
			if (o>=0 && object->originalLocation>=0 && 
				  ((lsi=in(o))==localObjects.end() || 
					 (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>EOS_AGE && currentSpeakerGroup<speakerGroups.size() && lsi->lastWhere<speakerGroups[currentSpeakerGroup].sgBegin)) && 
				  ((objects[o].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && 
					  objects[o].originalLocation>=0 && m[objects[o].originalLocation].word==m[object->originalLocation].word) ||
					 (objects[o].objectClass==NAME_OBJECT_CLASS && objects[o].hasAttribute(object->originalLocation,m))))
				objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
		}
		return;
	}
	if (wordOrderSensitiveModifier>=0)
	{
		vector <int> locations;
		for (unsigned int I=0; I<objectMatches.size(); I++)
			locations.push_back(locationBefore(objectMatches[I].object,where));
		int tmp=preferWordOrder(wordOrderSensitiveModifier,locations);
		if (tmp==0)
			objectMatches.erase(objectMatches.begin()+1);
		else if (tmp==1)
			objectMatches.erase(objectMatches.begin());
		else if (tmp==-2)
			objectMatches.clear();
	}
}

// the two men
bool cSource::tryGenderedSubgroup(int where,vector <cOM> &objectMatches,vector <cObject>::iterator object,int whereGenderedSubgroupCount,bool limitTwo)
{ LFS
	int genderedSubgroupCount;
	wstring tmpstr,tmpstr2;
	if (limitTwo)
		genderedSubgroupCount=2;
	else if (m[whereGenderedSubgroupCount].queryWinnerForm(NUMBER_FORM_NUM)>=0)
	{
		// 0,1,2,3,4,5,6,7...
		genderedSubgroupCount=m[whereGenderedSubgroupCount].word->first[0]-'0';
		if (!genderedSubgroupCount) return false;
	}
	else	// one, two, three, four...
	{
		wchar_t *num[]={L"one",L"two",L"three",L"four",L"five",L"six",L"seven",L"eight",L"nine",NULL};
		unsigned int I=0;
		for (; num[I] && m[whereGenderedSubgroupCount].word->first!=num[I]; I++);
		if (!num[I]) return false;
		genderedSubgroupCount=I+1;
	}
  // find latest speakerGroup containing exactly genderedSubgroupCount members
	int sg,sgg;
	//bool grouped=false;
	sg=(currentSpeakerGroup<speakerGroups.size()) ? currentSpeakerGroup : speakerGroups.size()-1;
	bool hasPOV=((currentSpeakerGroup<speakerGroups.size()) && speakerGroups[currentSpeakerGroup].povSpeakers.size()>0),allIn,oneIn;
  for (; sg>=0; sg--)
	{
		if (hasPOV && !intersect(speakerGroups[sg].speakers,speakerGroups[currentSpeakerGroup].povSpeakers,allIn,oneIn)) continue;
		sgg=-1;
		bool genderDisagreement=false;
	  if (speakerGroups[sg].speakers.size()==genderedSubgroupCount)
		{
			// is every object in group limited gender to 'object'?
			// the two men
			// both of them
			for (set <int>::iterator si=speakerGroups[sg].speakers.begin(),siEnd=speakerGroups[sg].speakers.end(); si!=siEnd && !genderDisagreement; si++)
				genderDisagreement=(!object->male && objects[*si].male) || (!object->female && objects[*si].female);
			if (!genderDisagreement) 
			{
				for (set <int>::iterator si=speakerGroups[sg].speakers.begin(),siEnd=speakerGroups[sg].speakers.end(); si!=siEnd && !genderDisagreement; si++)
					objectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:tryGenderedSubgroup matched %s [1] out of speakerGroup %s",where,objectString(objectMatches,tmpstr,false).c_str(),toText(speakerGroups[sg],tmpstr2));
				return true;
			}
		}
		if (speakerGroups[sg].groupedSpeakers.size()==genderedSubgroupCount)
		{
			genderDisagreement=false;
			for (set <int>::iterator si=speakerGroups[sg].groupedSpeakers.begin(),siEnd=speakerGroups[sg].groupedSpeakers.end(); si!=siEnd && !genderDisagreement; si++)
				genderDisagreement=(!object->male && objects[*si].male) || (!object->female && objects[*si].female);
			if (!genderDisagreement) 
			{
				for (set <int>::iterator si=speakerGroups[sg].groupedSpeakers.begin(),siEnd=speakerGroups[sg].groupedSpeakers.end(); si!=siEnd && !genderDisagreement; si++)
					objectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:tryGenderedSubgroup matched %s [2] out of speakerGroup %s",where,objectString(objectMatches,tmpstr,false).c_str(),toText(speakerGroups[sg],tmpstr2));
				return true;
			}
		}
		for (sgg=0; sgg<(signed)speakerGroups[sg].groups.size(); sgg++)
			if (speakerGroups[sg].groups[sgg].objects.size()==genderedSubgroupCount)
			{
				genderDisagreement=false;
				for (vector <int>::iterator gsi=speakerGroups[sg].groups[sgg].objects.begin(),gsiEnd=speakerGroups[sg].groups[sgg].objects.end(); gsi!=gsiEnd && !genderDisagreement; gsi++)
					genderDisagreement=(!object->male && objects[*gsi].male) || (!object->female && objects[*gsi].female);
				if (genderDisagreement) break;
				for (vector <int>::iterator gsi=speakerGroups[sg].groups[sgg].objects.begin(),gsiEnd=speakerGroups[sg].groups[sgg].objects.end(); gsi!=gsiEnd && !genderDisagreement; gsi++)
					objectMatches.push_back(cOM(*gsi,SALIENCE_THRESHOLD));
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:tryGenderedSubgroup matched %s [3] out of speakerGroup %s",where,objectString(objectMatches,tmpstr,false).c_str(),toText(speakerGroups[sg],tmpstr2));
				return true;
			}
	}
	return sg>=0;
}

bool cSource::matchOtherObjects(vector <int> &otherGroupedObjects,int speakerGroup,vector <cOM> &objectMatches)
{ LFS
	vector <cSpeakerGroup>::iterator sg=speakerGroups.begin()+speakerGroup;
	// get any groups in current speaker group otherGroupedObjects are in.
	bool allFound=true;
	for (unsigned int J=0; J<otherGroupedObjects.size(); J++)
		if (sg->groupedSpeakers.find(otherGroupedObjects[J])==sg->groupedSpeakers.end())
			allFound=false;
	if (allFound)
	{
		// match all the other objects other than otherGroupedObjects 
		for (set <int>::iterator gsi=sg->groupedSpeakers.begin(),gsiEnd=sg->groupedSpeakers.end(); gsi!=gsiEnd; gsi++)
			if (find(otherGroupedObjects.begin(),otherGroupedObjects.end(),*gsi)==otherGroupedObjects.end())
				objectMatches.push_back(cOM(*gsi,SALIENCE_THRESHOLD));
	}
	for (vector < cSpeakerGroup::cGroup >::iterator sgg=sg->groups.begin(),sggEnd=sg->groups.end(); sgg!=sggEnd; sgg++)
	{
		allFound=true;
		for (unsigned int J=0; J<otherGroupedObjects.size(); J++)
			if (find(sgg->objects.begin(),sgg->objects.end(),otherGroupedObjects[J])==sgg->objects.end())
				allFound=false;
		if (allFound)
		{
			// match all the other objects other than otherGroupedObjects 
			for (vector <int>::iterator sgoi=sgg->objects.begin(),sgoiEnd=sgg->objects.end(); sgoi!=sgoiEnd; sgoi++)
				if (find(otherGroupedObjects.begin(),otherGroupedObjects.end(),*sgoi)==otherGroupedObjects.end())
					objectMatches.push_back(cOM(*sgoi,SALIENCE_THRESHOLD));
		}
	}
	return objectMatches.size()>0;
}

// look ahead for the next paragraph and compile all new matching objects
// this is for 'another' 
// with "another" the matching object must not have appeared before:
//   1.  If not inQuotes, the object has not appeared after the beginning of the section or speakerGroup.
//   2.  If inQuotes, the object is new ONLY to the person being spoken to.  If that person spoken to
//       is not present at the time the quote happens (the person is talking on the phone) then this object is
//       really treated like "other" and its owner should change.
//   if "another" is used in a plural object (MPLURAL_ROLE) AND if the other object is in a group in the current 
//   speaker group then use this object instead.
bool cSource::scanFutureGenderedMatchingObjects(int where,bool inQuote,vector <cObject>::iterator object,vector <cOM> &objectMatches)
{ LFS
	// if in a group get the other member
	// if not inQuote, search only future groupings
	if ((m[where].objectRole&MPLURAL_ROLE) && currentSpeakerGroup<speakerGroups.size())
	{
		// collect all non-adjectival objects grouped together
		vector <int> otherGroupedObjects;
		for (unsigned int I=where+1; (m[I].objectRole&MPLURAL_ROLE) && I<m.size(); I++)
			if (m[I].getObject()>=0 && ((m[I].flags&WordMatch::flagAdjectivalObject)==0))
				otherGroupedObjects.push_back(m[I].getObject());
		for (int I=where-1; (m[I].objectRole&MPLURAL_ROLE) && I>=0; I--)
			if (m[I].getObject()>=0 && ((m[I].flags&WordMatch::flagAdjectivalObject)==0))
				otherGroupedObjects.push_back(m[I].getObject());
		if (inQuote)
			for (int I=currentSpeakerGroup; I>=0 && objectMatches.empty(); I--)
				matchOtherObjects(otherGroupedObjects,I,objectMatches);
		else
		{
			matchOtherObjects(otherGroupedObjects,currentSpeakerGroup,objectMatches);
			if (currentSpeakerGroup+1<speakerGroups.size())
				matchOtherObjects(otherGroupedObjects,currentSpeakerGroup+1,objectMatches);
		}
		if (objectMatches.size())
			return true;
	}
	int paragraph=0;
	for (unsigned int I=where; paragraph<2 && I<m.size(); I++)
	{
		if (m[I].word==Words.sectionWord) paragraph++;
		int o=m[I].getObject();
		// if the object is not in localObjects (and so is new) or is old and out of the current speaker section,
		//   and is the same occupation, or a person in the same profession
		vector <cLocalFocus>::iterator lsi;
		if (o>=0 && ((lsi=in(o))==localObjects.end() || (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>EOS_AGE && lsi->lastWhere<speakerGroups[currentSpeakerGroup].sgBegin)) && 
			  !objects[o].plural &&
			  (objects[o].objectClass==NAME_OBJECT_CLASS || 
				 objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
				 objects[o].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || 
				 objects[o].objectClass==GENDERED_DEMONYM_OBJECT_CLASS || 
				 objects[o].objectClass==GENDERED_RELATIVE_OBJECT_CLASS))
		{
			// if object o has an opposite adjective, skip.
			tIWMM fromMatch,toMatch,toMapMatch;
			wstring logMatch;
			if (nymNoMatch(I,objects.begin()+o,object,false,false,logMatch,fromMatch,toMatch,toMapMatch,L"NoMatch"))
				continue;
			// if object o has an opposite gender, skip.
			if ((objects[o].male ^ objects[o].female) && !(object->male && object->female) &&
				  ((objects[o].male && object->female) || (objects[o].female && object->male)))
				continue;
			// if local object is neuter, skip.
			if (objects[o].neuter)
				continue;
			objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
			return true;
		}
	}
	return false;
}

void cSource::includeWordOrderPreferences(int where,int wordOrderSensitiveModifier)
{ LFS
	bool plural=objects[m[where].getObject()].plural;
	int mostRecentLocation=-1,mostRecentLsiOffset=-1,ca=-1;
	vector < vector <cLocalFocus>::iterator > lsiOffsets;
	vector <int> locations;
	vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
	int largestSalience=-1;
	for (; lsi!=lsiEnd; lsi++)
	{
		ca=max(ca,lsi->numMatchedAdjectives);
		if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=SALIENCE_THRESHOLD && !lsi->notSpeaker &&
			// resolvables with word order sensitives cannot be pov speakers
			  (currentSpeakerGroup>=speakerGroups.size() || speakerGroups[currentSpeakerGroup].povSpeakers.find(lsi->om.object)==speakerGroups[currentSpeakerGroup].povSpeakers.end()))
		{
			lsiOffsets.push_back(lsi);
			locations.push_back(locationBefore(lsi->om.object,where,plural));
			if (mostRecentLocation<locations[locations.size()-1])
			{
				mostRecentLocation=locations[locations.size()-1];
				mostRecentLsiOffset=lsiOffsets.size()-1;
			}
			largestSalience=max(largestSalience,lsi->om.salienceFactor);
		}
	}
	int tmp;
	if (wordOrderSensitiveModifier==0 && largestSalience>=0 && locations.size()==2)
		tmp=(lsiOffsets[0]->om.salienceFactor==largestSalience) ? 1 : 0;
	// if the object being resolved is not plural, but the most recent positive salient localObject is,
	// accept this object instead.
	else
		tmp=preferWordOrder(wordOrderSensitiveModifier,locations);
	if (tmp>=0)
	{
		if (!plural && mostRecentLocation>=0 && objects[lsiOffsets[mostRecentLsiOffset]->om.object].plural)
			tmp=mostRecentLsiOffset;
		wstring tmpstr;
		// only bump if what is being matched is not a profession
		if (lsiOffsets[tmp]->numMatchedAdjectives<ca && m[where].queryForm(commonProfessionForm)<0)
		{
	    if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Bumped numMatchedAdjectives of %s from %d to %d.",where,objectString(lsiOffsets[tmp]->om,tmpstr,true).c_str(),lsiOffsets[tmp]->numMatchedAdjectives,ca);
			lsiOffsets[tmp]->numMatchedAdjectives=ca;
		}
		lsiOffsets[tmp]->om.salienceFactor+=10000;
		lsiOffsets[tmp]->res+=L"+WORDORDER[+10000]";
	}
	else if (tmp==-2)
	{
		for (unsigned int I=0; I<lsiOffsets.size(); I++)
		{
			lsiOffsets[I]->om.salienceFactor-=10000;
			lsiOffsets[I]->res+=L"-WORDORDER[-10000]";			
		}
	}
}

// definitelySpeaker is from scanForSpeaker.  It is set to true unless the speaker was detected after the
// quote in an _S1 pattern with a verb with a nonpast tense, or the verb had one or more objects.
// in secondary quotes, inPrimaryQuote=false
bool cSource::resolveGenderedObject(int where,bool definitelyResolveSpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,
vector <cOM> &objectMatches,
                                   vector <cObject>::iterator object,
int wordOrderSensitiveModifier,
																	 int &subjectCataRestriction,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated)
{ LFS
  if (objectMatches.size() || 
		  (object->objectClass!=GENDERED_GENERAL_OBJECT_CLASS && 
			 object->objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
			 object->objectClass!=BODY_OBJECT_CLASS &&
			 object->objectClass!=GENDERED_DEMONYM_OBJECT_CLASS &&
			 object->objectClass!=GENDERED_RELATIVE_OBJECT_CLASS &&
			 object->objectClass!=META_GROUP_OBJECT_CLASS && // only should be here if metagroup resolution returned false
			 // treat 'sir' as a gendered object, but not 'general' or 'archdeacon' - all are classed as honorific-only names
			 (object->objectClass!=NAME_OBJECT_CLASS || !object->name.justHonorific() || object->name.hon->second.query(L"pinr")<0)))
    return false;
	wstring tmpstr;
	// look ahead for the next paragraph(s) and look for the first new matching object
	// "another ally" = Julius (20406 Agatha) not inQuote
	// "another man" = Boris inQuote
	if (wordOrderSensitiveModifier>=0 && cObject::wordOrderWords[wordOrderSensitiveModifier]==L"another" && currentSpeakerGroup<speakerGroups.size())
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING gendered [future] %s",where,objectString(object,tmpstr,false).c_str());
		return scanFutureGenderedMatchingObjects(where,inPrimaryQuote,object,objectMatches);
	}
	// not generic 'the young man' and not used as an adjective
	// also don't penalize if it is "a" because this indicates an IS_OBJECT
	// if it is a first person possessive, 'my lady', 'my man', then it is probably a speaker, so allow.
	int begin=m[where].beginObjectPosition;
	/* took out - my dear girl, etc can be resolved if it is a HAIL */
	if ((currentSpeakerGroup>=speakerGroups.size() || object->objectClass==META_GROUP_OBJECT_CLASS) && 
		  begin<(signed)m.size() &&
		  m[begin].queryForm(possessiveDeterminerForm)>=0 && (m[where].objectRole&HAIL_ROLE) && 
		  (!(m[begin].word->second.inflectionFlags&FIRST_PERSON) ||
		  object->objectClass!=NAME_OBJECT_CLASS || !object->name.justHonorific() || object->name.hon->second.query(L"pinr")<0))
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING Possessive gendered hailed %s is not yet possible [%s]",where,objectString(object,tmpstr,false).c_str(),(definitelyResolveSpeaker) ? L"speaker":L"object");
		return false;
	}
	// search for numbered adjectives - two, three 'the two men'
	int genderedSubgroup=-1;
	for (int I=begin; I<m[where].endObjectPosition-1 && genderedSubgroup<0; I++)
		if (m[I].queryWinnerForm(NUMBER_FORM_NUM)>=0 || (m[I].queryWinnerForm(numeralCardinalForm)>=0 && m[I].word->first!=L"one"))
			genderedSubgroup=I;
	if ((genderedSubgroup>=0 || limitTwo) && tryGenderedSubgroup(where,objectMatches,object,genderedSubgroup,limitTwo))
	  return false;  // don't choose from local focus
	int lastGenderedAge;
  vector <int> disallowedReferences;
	if (m[begin].queryForm(possessiveDeterminerForm)>=0 && m[begin].objectMatches.size())
	{
		for (vector <cOM>::iterator omi=m[begin].objectMatches.begin(),omiEnd=m[begin].objectMatches.end(); omi!=omiEnd; omi++)
			disallowedReferences.push_back(omi->object);
	}
	// he put her hand in his. (not an adjectival object)
  if ((m[where].flags&WordMatch::flagAdjectivalObject)==0)
    coreferenceFilterLL2345(where,m[where].getObject(),disallowedReferences,lastBeginS1,lastRelativePhrase,lastQ2,mixedPlurality,subjectCataRestriction);
  adjustSaliencesByGenderNumberAndOccurrenceAgeAdjust(where,m[where].getObject(),inPrimaryQuote,inSecondaryQuote,definitelyResolveSpeaker,lastGenderedAge,disallowedReferences,false,isPhysicallyPresent,physicallyEvaluated);
  adjustSaliencesByParallelRoleAndPlurality(where,inPrimaryQuote,definitelyResolveSpeaker,lastGenderedAge);
	// even though 'miss' is a pinr, if the matching object lso has a Mrs as the honorific, we must fail it.
	if (object->objectClass==NAME_OBJECT_CLASS && object->name.justHonorific() && object->name.hon->first==L"miss")
	{
		tIWMM mrs=Words.gquery(L"mrs");
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
		for (; lsi!=lsiEnd; lsi++)
		  if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && objects[lsi->om.object].objectClass==NAME_OBJECT_CLASS &&
				  (objects[lsi->om.object].name.hon==mrs || objects[lsi->om.object].name.hon2==mrs || objects[lsi->om.object].name.hon3==mrs))
			{
				if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
					itos(L"-HONCONFLICT[",-100000,lsi->res,L"]");
				lsi->om.salienceFactor-=100000;
			}
	}
	// Could be a name - 'Carter'
	if (object->objectClass==GENDERED_GENERAL_OBJECT_CLASS && m[where].endObjectPosition-m[where].beginObjectPosition==1 && 
		  (m[where].flags&WordMatch::flagFirstLetterCapitalized))
	{
		tIWMM w=m[where].word;
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
		for (; lsi!=lsiEnd; lsi++)
		  if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && objects[lsi->om.object].objectClass==NAME_OBJECT_CLASS &&
				  (objects[lsi->om.object].name.first==w || objects[lsi->om.object].name.last==w))
			{
				objectMatches.push_back(cOM(lsi->om.object,SALIENCE_THRESHOLD));
				return false;
			}
	}
  // make sure this does not resolve to a NON_GENDERED_GENERAL_OBJECT_CLASS
  for (unsigned int I=0; I<localObjects.size(); I++)
    if (localObjects[I].includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) &&
        objects[localObjects[I].om.object].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS ||
        objects[localObjects[I].om.object].objectClass==NON_GENDERED_BUSINESS_OBJECT_CLASS ||
        objects[localObjects[I].om.object].objectClass==VERB_OBJECT_CLASS)
    {
      if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
        localObjects[I].res+=L"-DOWNCLASS["+itos(-10000,tmpstr)+L"]";
      localObjects[I].om.salienceFactor-=10000;
    }
  // if inQuote and !HAIL, diminish match of any objects in currentSpeakerGroup
	if (inPrimaryQuote && ((unsigned)currentSpeakerGroup)<speakerGroups.size() && !(m[where].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE)))
  {
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
		{
      if ((!(m[lastOpeningPrimaryQuote].flags&WordMatch::flagQuoteContainsSpeaker) || 
				   !in(lsi->om.object,m[lastOpeningPrimaryQuote].getRelObject())) &&
				   speakerGroups[currentSpeakerGroup].speakers.find(lsi->om.object)!=speakerGroups[currentSpeakerGroup].speakers.end() &&
					 lsi->physicallyPresent)
					 //find(speakerGroups[currentSpeakerGroup].observers.begin(),speakerGroups[currentSpeakerGroup].observers.end(),lsi->om.object)==speakerGroups[currentSpeakerGroup].observers.end()))
      {
				int salience=3000;
				if (m[where].word->second.flags&tFI::genericGenderIgnoreMatch)
					salience=2000;
        lsi->om.salienceFactor-=salience;
        if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
          lsi->res+=L"-INPQUOTE SPEAKER (1)["+itos(-salience,tmpstr)+L"]";
      }
		}
  }
	if (inSecondaryQuote && speakerGroupsEstablished && !(m[where].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE)) && 
			((unsigned)currentSpeakerGroup)<speakerGroups.size() && ((unsigned)currentEmbeddedSpeakerGroup)<speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size())
	{
		vector <cSpeakerGroup>::iterator esg=speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.begin()+currentEmbeddedSpeakerGroup;
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
		{
      if ((!(m[lastOpeningSecondaryQuote].flags&WordMatch::flagQuoteContainsSpeaker) || 
				   !in(lsi->om.object,m[lastOpeningSecondaryQuote].getRelObject())) &&
				   esg->speakers.find(lsi->om.object)!=esg->speakers.end())
			{
				lsi->om.salienceFactor-=2000;
				if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
					lsi->res+=L"-INPQUOTE SECONDARY SPEAKER (1)[-2000]";
			}
		}
	}
	if (objects[m[where].getObject()].objectClass==GENDERED_GENERAL_OBJECT_CLASS)
	{
		// match against all aliases 'my cousin', 'your cousin', 'her mother', 'the young man'
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
		for (; lsi!=lsiEnd; lsi++)
		{
			if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && objects[lsi->om.object].objectClass==NAME_OBJECT_CLASS && matchAliases(where,lsi->om.object,m[where].getObject()))
			{
				lsi->om.salienceFactor+=10000;
				lsi->res+=L"+ALIASMATCH[+10000]";
			}
		}
	}
	excludeObservers(where,inPrimaryQuote,definitelyResolveSpeaker);
	includeGenericGenderPreferences(where,object);
	if (wordOrderSensitiveModifier>=0)
		includeWordOrderPreferences(where,wordOrderSensitiveModifier);
	// if more than one positive reference and !inQuote, and one is a POV, decrease the POV
	if (!inPrimaryQuote && currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].povSpeakers.size()==1 &&
		  // generic 
		  m[where].endObjectPosition-m[where].beginObjectPosition==2 && m[m[where].beginObjectPosition].word->first==L"the" &&
			(m[m[where].beginObjectPosition+1].word->second.flags&tFI::genericGenderIgnoreMatch))
	{
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
		int numPOV=0,numNonPOV=0;
		for (; lsi!=lsiEnd; lsi++)
		{
			if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=SALIENCE_THRESHOLD && objects[lsi->om.object].objectClass!=BODY_OBJECT_CLASS)
			{
				if (speakerGroups[currentSpeakerGroup].povSpeakers.find(lsi->om.object)==speakerGroups[currentSpeakerGroup].povSpeakers.end())
					numNonPOV++;
				else
					numPOV++;
			}
		}
		if (numNonPOV>0 && numPOV>0)
		{
			if ((lsi=in(*speakerGroups[currentSpeakerGroup].povSpeakers.begin()))!=localObjects.end())
			{
				if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
					itos(L"-GENPOV[",-5000,lsi->res,L"]");
				lsi->om.salienceFactor-=5000;
			}
		}
	}
	if (debugTrace.traceSpeakerResolution)
	{
		wstring tmpstr2;
		lplog(LOG_RESOLUTION,L"%06d:RESOLVING gendered %s%s%s%s [%s minAge %d]",
			where,objectString(object,tmpstr,false).c_str(),m[where].roleString(tmpstr2).c_str(),(isPhysicallyPresent) ? L"[PP]":L"[NPP]",
			(physicallyEvaluated) ? L"[PE]":L"[NPE]",(definitelyResolveSpeaker) ? L"speaker":L"object",lastGenderedAge);
		printLocalFocusedObjects(where,GENDERED_GENERAL_OBJECT_CLASS);
	}
  return true;
}

void cSource::addPreviousDemonyms(int where)
{ LFS
	// if in analyzing "the russian", there are russians that have been mentioned before in the text (and may not be in local objects)
	// if there are other russians, and none of the localObjects are russian, then add the most recently mentioned russian.
	bool physicallyEvaluated,physicallyPresent=physicallyPresentPosition(where,physicallyEvaluated);
	if (relatedObjectsMap[m[m[where].endObjectPosition-1].word].size())
	{
		vector < vector <cLocalFocus>::iterator > locallyRelatedObjects;
		vector <cLocalFocus>::iterator lsi=localObjects.begin();
		wstring tmpstr;
		int maxCA=0,ca;
		bool atLeastOnePP=false;
		for (vector <cLocalFocus>::iterator lsiEnd=localObjects.end(); 
			   lsi!=lsiEnd;
				 lsi++)
		{
			if 	(lsi->om.object!=m[where].getObject() && 
  		      (ca=lsi->numMatchedAdjectives)>0 && 
					  // head match, or find head in commonAdjectives
					  (abbreviationEquivalent(m[where].word,m[objects[lsi->om.object].originalLocation].word) ||
						 find(objects[lsi->om.object].associatedNouns.begin(),objects[lsi->om.object].associatedNouns.end(),m[where].word)!=objects[lsi->om.object].associatedNouns.end()) &&
					  lsi->om.salienceFactor>(MINIMUM_SALIENCE_WITH_MATCHED_ADJECTIVES+ca*MORE_SALIENCE_PER_ADJECTIVE) &&
						// this purposely excludes the 'quoteIndependentAge' option - otherwise 'a woman[17263]' in quote matches to 'the elderly woman[18618]', out of quote
				 (lsi->occurredInPrimaryQuote==objectToBeMatchedInQuote || lsi->occurredOutsidePrimaryQuote!=objectToBeMatchedInQuote))
			{
				maxCA=max(maxCA,ca);
				locallyRelatedObjects.push_back(lsi);
				atLeastOnePP|=lsi->physicallyPresent; 
			}
			else 
			{
				lsi->om.salienceFactor-=4000;
				lsi->res+=L"-REQUIRES_DEMONYM[-4000]";
			}
		}
		if (locallyRelatedObjects.empty())
		{
			for (set <int>::iterator roi=relatedObjectsMap[m[m[where].endObjectPosition-1].word].begin(),roiEnd=relatedObjectsMap[m[m[where].endObjectPosition-1].word].end(); roi!=roiEnd; roi++)
			{
				if (objects[*roi].firstLocation<where && objects[*roi].objectClass==GENDERED_DEMONYM_OBJECT_CLASS)
				{
					int lastWhere=atBefore(*roi,where); // locationBefore(*roi,where,false,true);
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Found local demonym %d:%s@%d",where,objects[*roi].firstLocation,objectString(*roi,tmpstr,false).c_str(),lastWhere);
					// if all occurrences are in questions, probability statements or are marked RE, reject.
					if (m[lastWhere].objectMatches.empty())
					{
						if ((lsi=in(*roi))==localObjects.end() && anyAcceptableLocations(where,*roi))
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"%06d:Adding most recent demonym %s to local objects (1)",where,objectString(*roi,tmpstr,false).c_str());
							localObjects.push_back(cLocalFocus(cOM(*roi,SALIENCE_THRESHOLD),false,false,true,physicallyPresent));
							localObjects[localObjects.size()-1].numMatchedAdjectives=maxCA+1;
						}
					}
					else
					{
						for (vector <cOM>::iterator omi=m[lastWhere].objectMatches.begin(),omiEnd=m[lastWhere].objectMatches.end(); omi!=omiEnd; omi++)
						{
							if ((lsi=in(omi->object))==localObjects.end() && anyAcceptableLocations(where,*roi))
							{
								if (debugTrace.traceSpeakerResolution)
									lplog(LOG_RESOLUTION,L"%06d:Adding most recent demonym %d:%s to local objects (2)",where,lastWhere,objectString(omi->object,tmpstr,false).c_str());
								localObjects.push_back(cLocalFocus(cOM(omi->object,SALIENCE_THRESHOLD),false,false,true,physicallyPresent));
								localObjects[localObjects.size()-1].numMatchedAdjectives=maxCA+1;
							}
							// if already in localObjects, then make sure it has salience
							else
							{
								//lsi->om.salienceFactor=SALIENCE_THRESHOLD; 
							}
						}
					}
				}
			}
		}
		else 
		{
			// wipe out all non-physically present, if there is at least one physically present match
			for (vector < vector <cLocalFocus>::iterator >::iterator lro=locallyRelatedObjects.begin(),lroEnd=locallyRelatedObjects.end(); lro!=lroEnd; lro++)
			{
				lsi=*lro;
				if (atLeastOnePP && physicallyPresent && !lsi->physicallyPresent) continue;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Found local demonym %d:%s",where,lsi->lastWhere,objectString(lsi->om,tmpstr,false).c_str());
			int lastWhere=atBefore(lsi->om.object,where);
			if (lastWhere<0) return;
			lsi->physicallyPresent|=physicallyPresent;
			for (vector <cOM>::iterator omi=m[lastWhere].objectMatches.begin(),omiEnd=m[lastWhere].objectMatches.end(); omi!=omiEnd; omi++)
			{
				vector <cLocalFocus>::iterator omlsi;
				if ((omlsi=in(omi->object))==localObjects.end())
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Adding most recent demonym %d:%s to local objects (3)",where,lastWhere,objectString(omi->object,tmpstr,false).c_str());
					localObjects.push_back(cLocalFocus(cOM(omi->object,SALIENCE_THRESHOLD),false,false,true,physicallyPresent));
					localObjects[localObjects.size()-1].numMatchedAdjectives=lsi->numMatchedAdjectives+1;
				}
				// if already in localObjects, then make sure it has salience
				else
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Bumping latest match of most recent demonym %d:%s",where,lastWhere,objectString(omlsi->om,tmpstr,false).c_str());
					omlsi->om.salienceFactor=SALIENCE_THRESHOLD;
					omlsi->numMatchedAdjectives=lsi->numMatchedAdjectives+1;
				}
			}
		}
	}
}
}

bool cSource::addNewNumberedSpeakers(int where,vector <cOM> &objectMatches)
{ LFS
	if (m[where].getObject()<0 || (!objects[m[where].getObject()].male && !objects[m[where].getObject()].female) || 
		  (m[where].endObjectPosition-m[where].beginObjectPosition)==1)
		return false;
	wstring tmpstr,tmpstr2;
	set <int> speakers=speakerGroups[currentSpeakerGroup+1].speakers;
	// erase all objects in the future that already appear in the present
	for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
		speakers.erase(*si);
  if ((m[m[where].beginObjectPosition].word->first==L"two" && speakers.size()!=2) || 
		  (m[m[where].beginObjectPosition].word->first==L"three" && speakers.size()!=3)) return false;
	// match gender?
	if (objects[m[where].getObject()].male ^ objects[m[where].getObject()].female)
		for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
			if ((objects[*si].male ^ objects[*si].female) && 
				  (objects[*si].male!=objects[m[where].getObject()].male || objects[*si].female!=objects[m[where].getObject()].female))
				return false;
	for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
	{
		narrowGender(where,*si);
		objectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
	}
	speakerGroups[currentSpeakerGroup].fromNextSpeakerGroup.insert(speakers.begin(),speakers.end());
	// replace 'two men' with the future speakers in groups, so resolves looking backwards into this speakerGroup will see the objects matched (Tuppence 25204)
	for (vector < cSpeakerGroup::cGroup >::iterator sgg=speakerGroups[currentSpeakerGroup].groups.begin(),sggEnd=speakerGroups[currentSpeakerGroup].groups.end(); sgg!=sggEnd; sgg++)
		if (sgg->objects.size()==1 && sgg->objects[0]==m[where].getObject())
		{
			speakerGroups[currentSpeakerGroup].groups.erase(sgg);
			vector <int> gObjects;
			for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
				gObjects.push_back(*si);
			speakerGroups[currentSpeakerGroup].groups.push_back(cSpeakerGroup::cGroup(where,gObjects));
			break;
		}
	if (debugTrace.traceSpeakerResolution && objectMatches.size())
		lplog(LOG_RESOLUTION,L"%06d:multiple subject %s resolved by (new) speakers %s",where,objectString(m[where].getObject(),tmpstr,true).c_str(),objectString(objectMatches,tmpstr2,true).c_str());
	return true;
}

// this object is new, so subtract all the current speakers from the future speakers, and if there is one left, then assign.
bool cSource::addNewSpeaker(int where,vector <cOM> &objectMatches)
{ LFS
	wstring tmpstr,tmpstr2;
	bool physicallyEvaluated;
	int csg = currentSpeakerGroup, o = m[where].getObject();// , numSpeakersNotYetPP = 0;
	if (csg+1>=(signed)speakerGroups.size())
		return false;
	set <int> speakers=speakerGroups[csg+1].speakers;
	while (speakers.find(m[where].getObject())!=speakers.end() && csg+2<(signed)speakerGroups.size())
		speakers=speakerGroups[++csg+1].speakers;
	if (currentSpeakerGroup>0 && debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:previous speakerGroup %s",where,toText(speakerGroups[currentSpeakerGroup-1],tmpstr2));
	for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
	{
		vector <cLocalFocus>::iterator lsi=in(*si); 
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr3;
			if (lsi==localObjects.end())
				lplog(LOG_RESOLUTION,L"%06d:current speaker %s[%s][%s] in speakerGroup %s",
						where,objectString(*si,tmpstr,false).c_str(),L"NLO",L"NLO",toText(speakerGroups[currentSpeakerGroup],tmpstr3));
			else
				lplog(LOG_RESOLUTION,L"%06d:current speaker %s[%s][%s]lw=%d,pw=%d in speakerGroup %s",
						where,objectString(*si,tmpstr,false).c_str(),(lsi->notSpeaker) ? L"object":L"speaker",(lsi->physicallyPresent) ? L"PP":L"notPP",
							lsi->lastWhere,lsi->previousWhere,toText(speakerGroups[currentSpeakerGroup],tmpstr3));
		}
		if (lsi != localObjects.end() && lsi->lastWhere<speakerGroups[currentSpeakerGroup].sgBegin && currentSpeakerGroup>0 && speakerGroups[currentSpeakerGroup-1].speakers.find(*si)==speakerGroups[currentSpeakerGroup-1].speakers.end() &&
			  objects[o].matchGender(objects[*si]) && objects[*si].objectClass!=BODY_OBJECT_CLASS)
			objectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
	}
	if (objectMatches.size())
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:speakers not yet PP %s match from speakerGroup %s",
	      where,objectString(objectMatches,tmpstr,false).c_str(),toText(speakerGroups[currentSpeakerGroup],tmpstr2));
		if (objectMatches.size()==1 && speakerGroups[currentSpeakerGroup].speakers.find(o)!=speakerGroups[currentSpeakerGroup].speakers.end())
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:speaker %s erased in speakerGroup [post matched]",where,objectString(o,tmpstr,true).c_str());
			vector <cLocalFocus>::iterator lsi = in(o);
			if (lsi!=localObjects.end())
				localObjects.erase(lsi);
			speakerGroups[csg].speakers.erase(o);
			speakers.erase(o);
			if (replaceObjectInSection(where,objectMatches[0].object,o,L"lookAhead"))
			  return true;
			objectMatches.clear();
			//return true;
		}
	}
	// erase all objects in the future that already appear in the present
	for (set <int>::iterator si=speakerGroups[csg].speakers.begin(),siEnd=speakerGroups[csg].speakers.end(); si!=siEnd; si++)
		speakers.erase(*si);
	if (speakers.size()>1)
	{
		// erase all objects that have ever appeared in the past, but only if all present objects have already been erased.
		for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; )
			if (objects[*si].firstPhysicalManifestation>=0 && objects[*si].firstPhysicalManifestation<speakerGroups[csg].sgBegin)
				speakers.erase(si++);
			else
				si++;
	}
	if (objects[o].male ^ objects[o].female)
		for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; )
			if (!objects[o].matchGender(objects[*si]))
				speakers.erase(si++);
			else
				si++;
	if (speakers.size()==1 && objects[*speakers.begin()].objectClass!=BODY_OBJECT_CLASS)
	{
		// what if the speaker also was resolved as something else earlier?
		int toSpeaker=*speakers.begin(),toSpeakerResolved=toSpeaker;
		int lastWhere=atBefore(toSpeaker,where);
		if (lastWhere>=0 && m[lastWhere].objectMatches.size()==1)
			toSpeakerResolved=m[lastWhere].objectMatches[0].object;
		objectMatches.push_back(cOM(toSpeakerResolved,SALIENCE_THRESHOLD));
		speakerGroups[csg].fromNextSpeakerGroup.insert(toSpeakerResolved);
		set <int>::iterator keep;
		if ((keep=speakerGroups[csg].speakers.find(m[where].getObject()))!=speakerGroups[csg].speakers.end())
		{
			// if 'a voice' is resolved to 'The German' in the next speakerGroup, and 'The German' was resolved to another speaker earlier,
			// then a voice must be replaced by 'The German', then 'The German' replaced by its resolved object.
			wstring tmpstr3;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:speaker %s replaced by future speaker %s in speakerGroup %s",
			    where,objectString(*keep,tmpstr,true).c_str(),objectString(toSpeaker,tmpstr2,true).c_str(),toText(speakerGroups[csg],tmpstr3));
			replaceObjectInSection(where,toSpeaker,*keep,L"lookAheadSpeaker");
			if (toSpeaker!=toSpeakerResolved)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:speaker %s replaced by future speaker %s in speakerGroup %s",
				    where,objectString(toSpeaker,tmpstr,true).c_str(),objectString(toSpeakerResolved,tmpstr2,true).c_str(),toText(speakerGroups[csg],tmpstr3));
				replaceObjectInSection(where,toSpeakerResolved,toSpeaker,L"lookAheadSpeaker");  // don't change "lookAheadSpeaker" because of replaceObjectInSection repeat limitation
			}
		}
	}
	// subtract all speakers that have appeared in the speakerGroup before now and subtract any pov speakers.
	// A voice inside called out something
	else
	{
		//28184:present=28163-28259 [section 009]: A smart young woman a clear voice [no Previous Subset SG][no non-name object][speakers never grouped]
		//  	   future=28259-28621 [section 009]: miss tuppence  mrs rita vandemeyer  povSpeakers (miss tuppence ) dnSpeakers () [no Previous Subset SG][no non-name object][speakers never grouped] 
		speakers=speakerGroups[csg].speakers;
		int stopLoop = where-speakerGroups[csg].sgBegin;
		for (int I=speakerGroups[csg].sgBegin; I<where && speakers.size() && stopLoop>=0; I++,stopLoop--)
			if (m[I].principalWherePosition>=0 && physicallyPresentPosition(m[I].principalWherePosition,I,physicallyEvaluated,false))
			{
				I=m[I].principalWherePosition;
				if (I<where)
				{
					speakers.erase(m[I].getObject());
					for (vector <cOM>::iterator omi=m[I].objectMatches.begin(),omiEnd=m[I].objectMatches.end(); omi!=omiEnd; omi++)
						speakers.erase(omi->object);
				}
			}
		bool objectInSpeakers=speakers.erase(m[where].getObject())>0;
		for (set <int>::iterator si=speakerGroups[csg].povSpeakers.begin(),siEnd=speakerGroups[csg].povSpeakers.end(); si!=siEnd; si++)
			speakers.erase(*si);
		if (objects[o].male ^ objects[o].female)
			for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; )
				if (!objects[o].matchGender(objects[*si]))
					speakers.erase(si++);
				else
					si++;
		speakerGroups[csg].fromNextSpeakerGroup=speakers;
		for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
			objectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
		if (speakers.size())
		{
			if (objectInSpeakers)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:speaker %s erased in speakerGroup [post matched]",where,objectString(m[where].getObject(),tmpstr,true).c_str());
				speakerGroups[csg].speakers.erase(m[where].getObject());
			}
		}
	}
	if (objectMatches.size()==1 && objects[o].male && objects[o].female && (objects[objectMatches[0].object].male ^ objects[objectMatches[0].object].female))
	{
		objects[o].male=objects[objectMatches[0].object].male;
		objects[o].female=objects[objectMatches[0].object].female;
		if (debugTrace.traceSpeakerResolution)
	    lplog(LOG_RESOLUTION,L"%06d:%s becomes %s from match object %s (4).",where,
		    objectString(o,tmpstr,false).c_str(),(objects[o].male) ? L"male" : L"female",
			  objectString(objectMatches[0].object,tmpstr2,false).c_str());
	}
	if (debugTrace.traceSpeakerResolution && objectMatches.size())
		lplog(LOG_RESOLUTION,L"%06d:subject %s resolved by (new) speakers %s",where,objectString(m[where].getObject(),tmpstr,true).c_str(),objectString(objectMatches,tmpstr2,true).c_str());
	return objectMatches.size()>0;
}

// in secondary quotes, inPrimaryQuote=false
bool cSource::resolveBodyObjectClass(int where,int beginEntirePosition,vector <cObject>::iterator object,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool resolveForSpeaker,bool avoidCurrentSpeaker,int wordOrderSensitiveModifier,int subjectCataRestriction,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated,bool &changeClass,vector <cOM> &objectMatches)
{ LFS
	changeClass=false;
	wstring tmpstr,tmpstr2;
	// check 'the voice of the German'
	int forwardObjectPosition,o,whereObject;
	if (object->getOwnerWhere()<0 && where+2<(signed)m.size() && m[where+1].word->first==L"of" && ((forwardObjectPosition=m[where+2].principalWherePosition)>=0) && 
		  m[where].word->first!=L"thoughts" && m[where].word->first!=L"thought")
	{
		resolveObject(forwardObjectPosition,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,m[where].word->first==L"both");
		o=m[forwardObjectPosition].getObject();
		if (o>=0 && (objects[o].male || objects[o].female) && !objects[o].neuter)
		{
			if (m[forwardObjectPosition].objectMatches.size())
				objectMatches=m[forwardObjectPosition].objectMatches;
			else
				objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:added 'of' match %s.",where,objectString(objectMatches,tmpstr,false).c_str());
			//object->getOwnerWhere()=forwardObjectPosition;  may lead to incorrect resolution because of disagreements between identify speaker phases and resolve speaker
			return false;
		}
		if (o>=0 && !(objects[o].male || objects[o].female) && objects[o].neuter)
		{
			changeClass=true;
			return false;
		}
	}
	// check 'the man with the beard'
	int ownerObjectPosition;
	if (object->getOwnerWhere()<0 && (ownerObjectPosition=m[where].beginObjectPosition)>=2 && m[ownerObjectPosition-1].word->first==L"with")
	{
		o=-1;
		for (int I=ownerObjectPosition-2; I>=0 && I>=where-5 && !isEOS(I); I--)
			if (m[I].getObject()>=0 && m[I].endObjectPosition==m[where].beginObjectPosition-1)
			{
				ownerObjectPosition=I;
				bool ownerResolved=false;
				o=m[ownerObjectPosition].getObject();
				if (ownerResolved=o>=0 && (objects[o].male || objects[o].female) && !objects[o].neuter)
				{
					if (m[ownerObjectPosition].objectMatches.size())
						objectMatches=m[ownerObjectPosition].objectMatches;
					else
						objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:added 'with' match %s.",where,objectString(objectMatches,tmpstr,false).c_str());
				}
				if ((m[ownerObjectPosition].objectRole&OBJECT_ROLE) && (ownerObjectPosition=m[ownerObjectPosition].relSubject)>=0)
				{
					o=m[ownerObjectPosition].getObject();
					if (o>=0 && (objects[o].male || objects[o].female) && !objects[o].neuter)
					{
						if (m[ownerObjectPosition].objectMatches.size())
							objectMatches.insert(objectMatches.end(),m[ownerObjectPosition].objectMatches.begin(),m[ownerObjectPosition].objectMatches.end());
						else
							objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:added 'with' subject match %s.",where,objectString(objectMatches,tmpstr,false).c_str());
						ownerResolved=true;
					}
				}
				if (ownerResolved) return false;
				break;
			}
		if (o>=0 && !(objects[o].male || objects[o].female) && objects[o].neuter)
		{
			changeClass=true;
			return false;
		}
	}
	bool urp=unResolvablePosition(beginEntirePosition);
	if (isInternalBodyPart(where) && object->getOwnerWhere()<0 && m[beginEntirePosition].pma.queryPattern(L"_META_NAME_EQUIVALENCE")==-1) 
	{
		changeClass=true;
		return false;
	}
	if (urp && object->getOwnerWhere()<0 &&
		  m[beginEntirePosition].pma.queryPattern(L"_META_NAME_EQUIVALENCE")==-1) // Another voice[german] , which Tommy fancied was that[german] of the tall , commanding - looking man[man] whose face[german] had seemed familiar to him[man,boris,tommy,irish] , said :
	{
		// a new voice
		for (int I=m[where].beginObjectPosition; I<m[where].endObjectPosition; I++)
			if (m[I].word->first==L"new")
				return false;
		if ((m[where].objectRole&(SUBJECT_ROLE|PREP_OBJECT_ROLE))==SUBJECT_ROLE || 
				(m[where].objectRole&(OBJECT_ROLE|PREP_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))==(OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE) ||
			  (m[where].objectRole&PRIMARY_SPEAKER_ROLE)==PRIMARY_SPEAKER_ROLE || 
			  (speakerGroupsEstablished && currentSpeakerGroup+1<speakerGroups.size() && 
				 speakerGroups[currentSpeakerGroup+1].speakers.find(m[where].getObject())!=speakerGroups[currentSpeakerGroup+1].speakers.end()))
		{
			int whereVerb=m[where].getRelVerb();
			whereObject=m[where].getRelObject();
			if (whereObject<0 && whereVerb>=0)
				while (whereVerb+1<(signed)m.size() && m[whereVerb+1].queryWinnerForm(adverbForm)>=0 && m[whereVerb+1].queryWinnerForm(prepositionForm)<0) 
					whereVerb++;
			if (debugTrace.traceSpeakerResolution)
			{
				if (whereObject>=0 && whereVerb>=0)
					lplog(LOG_RESOLUTION,L"%06d:facial expression %s %d %s | %d %d %d",where,(isFacialExpression(where)) ? L"true":L"false",whereObject,m[whereObject].word->first.c_str(),
														whereVerb,m[whereVerb+1].queryWinnerForm(prepositionForm),m[whereVerb+1].getRelObject());
				else if (whereObject>=0)
					lplog(LOG_RESOLUTION,L"%06d:facial expression %s %d %s | %d",where,(isFacialExpression(where)) ? L"true":L"false",whereObject,m[whereObject].word->first.c_str(),
														whereVerb);
				else if (whereVerb>=0)
					lplog(LOG_RESOLUTION,L"%06d:facial expression %s %d %d %d",where,(isFacialExpression(where)) ? L"true":L"false",
														whereVerb,m[whereVerb+1].queryWinnerForm(prepositionForm),m[whereVerb+1].getRelObject());
			}
			// a smile overspread his face
			// a frown was on his face.  a sparkle twinkled in his eye.
			// A peculiar smile lingered for a moment on Julius's face.
			if (isFacialExpression(where) && 
				  (isFace(whereObject) ||
					  // A delighted grin spread slowly over Albert's countenance.
				   (whereObject<0 && whereVerb>=0 && whereVerb+1<(signed)m.size() && m[whereVerb+1].queryWinnerForm(prepositionForm)>=0 && 
						// A peculiar smile lingered for a moment on Julius's face.
					  (isFace(whereObject=m[whereVerb+1].getRelObject()) || 
						 (whereObject>=0 && (whereObject=m[whereObject].endObjectPosition)>=0 && m[whereObject].queryWinnerForm(prepositionForm)>=0 && isFace(whereObject=m[whereObject].getRelObject())))) ||
						// there was a look in the German's eyes
					 (m[whereObject=m[where].endObjectPosition].queryForm(prepositionForm)>=0 && isFace(whereObject=m[whereObject].getRelObject()))))
			{
				resolveObject(whereObject,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,m[where].word->first==L"both");
				objectMatches=m[whereObject].objectMatches;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Resolved through facial expression to %s.",where,objectString(objectMatches,tmpstr,true).c_str());
			}
			// don't resolve if the subject has no owner and is not definitively a subject
			// A cold hand seemed to close round her heart
			else if (currentSpeakerGroup+1<speakerGroups.size() && (inPrimaryQuote || !(m[where].objectRole&(NONPAST_OBJECT_ROLE|NOT_OBJECT_ROLE))))
				addNewSpeaker(where,objectMatches);
		}
		else if ((m[where].objectRole&(SUBJECT_ROLE|PREP_OBJECT_ROLE))==(SUBJECT_ROLE|PREP_OBJECT_ROLE))
		{
			int whereLastObject=-1;
			for (int I=where-1; I>=0 && (m[I].objectRole&SUBJECT_ROLE); I--) 
				if (m[I].getObject()!=-1) whereLastObject=I;
			if (whereLastObject>=0 && (m[whereLastObject].objectRole&(SUBJECT_ROLE|PREP_OBJECT_ROLE))==SUBJECT_ROLE && 
				  (m[whereLastObject].getObject()>=0 || m[whereLastObject].objectMatches.size()))
			{
				if (m[whereLastObject].objectMatches.empty())
					objectMatches.push_back(cOM(m[whereLastObject].getObject(),SALIENCE_THRESHOLD));
				else
					objectMatches=m[whereLastObject].objectMatches;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Resolved ownerless body object through subject/object match.",where);
			}
		}
		// Tuppence raised a trembling left hand to the glass .
		else if ((m[where].objectRole&(OBJECT_ROLE|PREP_OBJECT_ROLE))==OBJECT_ROLE && m[where].relSubject>=0 && 
							(o=m[whereObject=m[where].relSubject].getObject())>=0 && isAgentObject(o) && !objects[o].plural)
		{
			if (m[whereObject].objectMatches.empty())
				objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
			else
				objectMatches=m[whereObject].objectMatches;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Resolved ownerless body object through subject/object match.",where);
		}
		else if (!(m[where].objectRole&NO_ALT_RES_SPEAKER_ROLE))
			changeClass=true;
		return false;
	}
	if (urp) return false;
	if (object->getOwnerWhere()>=0 && m[object->getOwnerWhere()].objectMatches.empty() && 
		  m[object->getOwnerWhere()].queryForm(possessiveDeterminerForm)>=0 && (m[object->getOwnerWhere()].word->second.inflectionFlags&THIRD_PERSON)==THIRD_PERSON &&
		  m[beginEntirePosition].pma.queryPattern(L"_META_NAME_EQUIVALENCE")==-1 &&
			currentSpeakerGroup+1<speakerGroups.size() && !(m[where].objectRole&(NONPAST_OBJECT_ROLE|NOT_OBJECT_ROLE)) &&
			addNewSpeaker(where,objectMatches))
		return false;
	if (object->getOwnerWhere()>=0 && m[object->getOwnerWhere()].getObject()!= cObject::eOBJECTS::UNKNOWN_OBJECT)
	{
		vector <cLocalFocus>::iterator lsi=localObjects.begin();
		for (unsigned int I=0; I<localObjects.size(); I++,lsi++) 
			lsi->clear();
		for (vector <cOM>::iterator omi=m[object->getOwnerWhere()].objectMatches.begin(),omiEnd=m[object->getOwnerWhere()].objectMatches.end(); omi!=omiEnd; omi++)
			if (!objects[omi->object].neuter)
				objectMatches.push_back(*omi);
		if (objectMatches.empty() && m[object->getOwnerWhere()].objectMatches.size())
			objectMatches=m[object->getOwnerWhere()].objectMatches;
		if (objectMatches.empty() && m[object->getOwnerWhere()].getObject()>=0)
			objectMatches.push_back(cOM(m[object->getOwnerWhere()].getObject(),getRoleSalience(m[object->getOwnerWhere()].objectRole)));
		return false;
	}
	else if ((m[where].objectRole&(SUBJECT_ROLE|OBJECT_ROLE|PREP_OBJECT_ROLE))==OBJECT_ROLE && !(m[where].objectRole&NO_ALT_RES_SPEAKER_ROLE))
	{
		changeClass=true;
		return false;
	}
	return resolveGenderedObject(where,definitelySpeaker|resolveForSpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,objectMatches,object,wordOrderSensitiveModifier,subjectCataRestriction,mixedPlurality,limitTwo,isPhysicallyPresent,physicallyEvaluated);
}

void cSource::excludePOVSpeakers(int where,wchar_t *fromWhere)
{ LFS
	vector <cLocalFocus>::iterator lsi;
	wstring tmpstr;
	if (!(m[where].objectRole&(HAIL_ROLE|IN_PRIMARY_QUOTE_ROLE))) // DEBUG TSG  
	{
		if (currentSpeakerGroup<speakerGroups.size())
		{
			for (set <int>::iterator povi=speakerGroups[currentSpeakerGroup].povSpeakers.begin(),poviEnd=speakerGroups[currentSpeakerGroup].povSpeakers.end(); povi!=poviEnd; povi++)
				if ((lsi=in(*povi))!=localObjects.end())
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:meta group object cannot match pov speaker (1 %s) %s",where,fromWhere,objectString(lsi->om,tmpstr,true).c_str());
					lsi->om.salienceFactor-=DISALLOW_SALIENCE;
					itos(L"-META_NOT_POV[-",DISALLOW_SALIENCE,lsi->res,L"]");
				}
		}
		else
		{
			int povWhere=-1;
			if (povInSpeakerGroups.size()>0 && m[povWhere=povInSpeakerGroups[povInSpeakerGroups.size()-1]].getObject()>=0 && section<sections.size() && povWhere>(signed)sections[section].begin)
			{
				if ((lsi=in(m[povWhere].getObject()))!=localObjects.end())
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:meta group object cannot match pov speaker (2 %s) %d:%s",where,fromWhere,povWhere,objectString(lsi->om,tmpstr,true).c_str());
					lsi->om.salienceFactor-=DISALLOW_SALIENCE;
					itos(L"-META_NOT_POV[-",DISALLOW_SALIENCE,lsi->res,L"]");
				}
				if (m[povWhere].objectMatches.size()==1)
					for (vector <cOM>::iterator povi=m[povWhere].objectMatches.begin(),poviEnd=m[povWhere].objectMatches.end(); povi!=poviEnd; povi++)
						if ((lsi=in(povi->object))!=localObjects.end())
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"%06d:meta group object cannot match pov speaker (3 %s) %d:%s",where,fromWhere,povWhere,objectString(lsi->om,tmpstr,true).c_str());
							lsi->om.salienceFactor-=DISALLOW_SALIENCE;
							itos(L"-META_NOT_POV[-",DISALLOW_SALIENCE,lsi->res,L"]");
						}
			}
		}
	}
}

void cSource::excludeObservers(int where,bool inQuote,bool definitelySpeaker)
{ LFS
	if (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].observers.size() && !inQuote)
	{
		// any plural (because the observer is only one person)
		if ((speakerGroups[currentSpeakerGroup].observers.size()==1 && objects[m[where].getObject()].plural) ||
				// observer is definitely not a speaker
				definitelySpeaker || objects[m[where].getObject()].getOwnerWhere()<-1 ||
				// when an unresolvable object is an "IS" object, then the subject is not an observer. / He was a big man. 
				((m[where].objectRole&(SUBJECT_ROLE|IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))==(SUBJECT_ROLE|IS_OBJECT_ROLE) && m[where].getRelObject()>=0 &&
				m[m[where].getRelObject()].beginObjectPosition>=0 && unResolvablePosition(m[m[where].getRelObject()].beginObjectPosition) && 
				m[m[where].getRelObject()].getObject()>=0 && !objects[m[m[where].getRelObject()].getObject()].neuter))
		{
			wstring tmpstr;
			for (set <int>::iterator oi=speakerGroups[currentSpeakerGroup].observers.begin(),oiEnd=speakerGroups[currentSpeakerGroup].observers.end(); oi!=oiEnd; oi++)
			{
				for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end(); lsi!=lsEnd; lsi++)
				{
					// disallow the observer or BODY OBJECTS owned by the observer
					if (lsi->om.object==*oi ||
							(objects[lsi->om.object].objectClass==BODY_OBJECT_CLASS && 
							 objects[lsi->om.object].getOwnerWhere()>=0 && 
							 m[objects[lsi->om.object].getOwnerWhere()].objectMatches.size()==1 && 
							 m[objects[lsi->om.object].getOwnerWhere()].objectMatches[0].object==*oi))
					{
						//if (definitelySpeaker && resolveForSpeaker // detect whether this is a single quote with no immediate previous or next quote
						// if so, allow an observer to talk to him/herself 
						if (debugTrace.traceSpeakerResolution)
						{
							lplog(LOG_RESOLUTION,L"%06d:observer %s should not match object",where,objectString(*oi,tmpstr,true).c_str());
							itos(L"-OBSERVER[",DISALLOW_SALIENCE,lsi->res,L"]");
						}
						lsi->om.salienceFactor=-DISALLOW_SALIENCE;
					}
				}
			}
		}
	}
}

// if this not in a quote, not a speaker and is a descriptive sentence (IS) which does not specifically match the POV
// and if there is a new entity having been introduced within 2 sentences (newPPAge<2)
// discourage POV
void cSource::discouragePOV(int where,bool inQuote,bool definitelySpeaker)
{ LFS
	set <int> povSpeakers;
	int o=-1,wo;
	// if not inquote and plural and subject and an object is a singular pronoun, and there is no groups in the current speaker group, continue.
	bool pluralSubject=!inQuote && objects[m[where].getObject()].plural && objects[m[where].getObject()].objectClass==PRONOUN_OBJECT_CLASS &&	(m[where].objectRole&SUBJECT_ROLE);
	// they VERB him
	bool mixedPluralityObject=pluralSubject && m[where].getRelObject()>=0 && (o=m[wo=m[where].getRelObject()].getObject())>=0 && 
			!objects[o].plural && !objects[o].neuter && (objects[o].male || objects[o].female) && objects[o].objectClass==PRONOUN_OBJECT_CLASS;
	// they VERB on him.
	bool mixedPluralityPrepObject=pluralSubject && m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].relPrep>=0 && (wo=m[m[m[where].getRelVerb()].relPrep].getRelObject())>=0 && (o=m[wo].getObject())>=0;
	if (mixedPluralityPrepObject)
	//{
		mixedPluralityPrepObject=!objects[o].plural && !objects[o].neuter && objects[o].objectClass==PRONOUN_OBJECT_CLASS;
			//for (int I=m[wo].beginObjectPosition; I<m[wo].endObjectPosition && !mixedPluralityPrepObject; I++)
				// they VERB on his XXX.
				//mixedPluralityPrepObject=(o=m[I].getObject())==OBJECT_UNKNOWN_MALE || o==OBJECT_UNKNOWN_FEMALE;
	//}
	bool noGroupsAvailable=(currentSpeakerGroup>=speakerGroups.size() && tempSpeakerGroup.groups.empty()) || (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].groupedSpeakers.empty());
	bool mixedPlurality=pluralSubject && (mixedPluralityObject || mixedPluralityPrepObject) && noGroupsAvailable;
	if (!mixedPlurality && (inQuote || definitelySpeaker ||	((m[where].objectRole&(SUBJECT_ROLE|IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))!=(SUBJECT_ROLE|IS_OBJECT_ROLE)) ||
		  (m[where].objectRole&POV_OBJECT_ROLE) || (m[where].getRelObject()>=0 && (m[m[where].getRelObject()].objectRole&POV_OBJECT_ROLE))))
		return;
	getPOVSpeakers(povSpeakers);
	vector <cLocalFocus>::iterator lsi,lsiEnd=localObjects.end(),povLSI;
	if (povSpeakers.empty() || povSpeakers.size()>1 || (povLSI=in(*povSpeakers.begin()))==lsiEnd || povLSI->numMatchedAdjectives>1 || povLSI->om.salienceFactor<0) return;
	if (mixedPlurality)
	{
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr;
			lplog(LOG_RESOLUTION,L"%06d:POV %s should be disallowed (mixedPlurality)",where,objectString(povLSI->om,tmpstr,true).c_str());
			itos(L"-POV_MIXED[",DISALLOW_SALIENCE,povLSI->res,L"]");
		}
		povLSI->om.salienceFactor-=DISALLOW_SALIENCE;
	}
	else
	{
		// is there a new entity?
		for (lsi=localObjects.begin(); lsi!=lsiEnd; lsi++)
			if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->newPPAge<=2 && lsi->newPPAge>=0 && lsi->om.salienceFactor>=SALIENCE_THRESHOLD)
				break;
		if (lsi!=lsiEnd && lsi!=povLSI)
		{
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr,tmpstr2;
				lplog(LOG_RESOLUTION,L"%06d:POV %s should be disallowed in favor of new object %s",where,objectString(povLSI->om,tmpstr,true).c_str(),objectString(lsi->om,tmpstr2,true).c_str());
				itos(L"-POV[",DISALLOW_SALIENCE,povLSI->res,L"]");
			}
				povLSI->om.salienceFactor-=DISALLOW_SALIENCE;
		}
	}
}

void cSource::excludeSpeakers(int where,bool inPrimaryQuote,bool inSecondaryQuote)
{ LFS
	//mixedPluralityUsageSubGroupEnhancement(where);
	// if inQuote and !HAIL, diminish match of any objects in currentSpeakerGroup
	// is there anyone else they could be talking about?
	if (inPrimaryQuote && speakerGroupsEstablished)
	{
		vector <cSpeakerGroup>::iterator sg=speakerGroups.begin()+currentSpeakerGroup;
		bool nonSpeakerCandidate=false,quoteDoesNotContainSpeaker=lastOpeningPrimaryQuote<0 || !(m[lastOpeningPrimaryQuote].flags&WordMatch::flagQuoteContainsSpeaker);
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(); lsi!=localObjects.end() && !nonSpeakerCandidate; lsi++)
			nonSpeakerCandidate=(lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=0 && sg->speakers.find(lsi->om.object)==sg->speakers.end());
		if ((sg->speakers.size()-sg->observers.size()<=2 || nonSpeakerCandidate) &&
		  !(m[where].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE|PP_OBJECT_ROLE)))
		{
			for (vector <cLocalFocus>::iterator lsi=localObjects.begin(); lsi!=localObjects.end(); lsi++)
			{
				if (sg->speakers.find(lsi->om.object)!=sg->speakers.end() && sg->observers.find(lsi->om.object)==sg->observers.end() &&
						(quoteDoesNotContainSpeaker || !in(lsi->om.object,m[lastOpeningPrimaryQuote].getRelObject())) &&
						lsi->physicallyPresent)
				{
					lsi->om.salienceFactor-=2000;
					if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
						lsi->res+=L"-INPQUOTE SPEAKER (2)[-2000]";
				}
			}
		}
	}
	if (inSecondaryQuote && speakerGroupsEstablished && 
			((unsigned)currentSpeakerGroup)<speakerGroups.size() && ((unsigned)currentEmbeddedSpeakerGroup)<speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size())
	{
		vector <cSpeakerGroup>::iterator esg=speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.begin()+currentEmbeddedSpeakerGroup;
		bool nonSpeakerCandidate=false,quoteDoesNotContainSpeaker=lastOpeningSecondaryQuote<0 || !(m[lastOpeningSecondaryQuote].flags&WordMatch::flagQuoteContainsSpeaker);
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(); lsi!=localObjects.end() && !nonSpeakerCandidate; lsi++)
			nonSpeakerCandidate=(lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=0 && esg->speakers.find(lsi->om.object)==esg->speakers.end());
		if ((esg->speakers.size()-esg->observers.size()<=2 || nonSpeakerCandidate) &&
		  !(m[where].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE|PP_OBJECT_ROLE)))
		{
			for (vector <cLocalFocus>::iterator lsi=localObjects.begin(); lsi!=localObjects.end(); lsi++)
			{
				if (esg->speakers.find(lsi->om.object)!=esg->speakers.end() && esg->observers.find(lsi->om.object)==esg->observers.end() &&
						(quoteDoesNotContainSpeaker || !in(lsi->om.object,m[lastOpeningSecondaryQuote].getRelObject())))
				{
					lsi->om.salienceFactor-=2000;
					if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
						lsi->res+=L"-INPQUOTE SECONDARY SPEAKER (2)[-2000]";
				}
			}
		}
	}
}

// wchar_t *wordOrderWords[]={L"other",L"another",L"second",L"first",L"third",L"former",L"latter",L"that",L"this",L"two",L"three",NULL};
//                            -2       -3         -4        -5       -6       -7        -8        -9      -10     -11    -12
bool cSource::resolveWordOrderOfObject(int where,int wo,int ofObjectWhere,vector <cOM> &objectMatches)
{ LFS
	int ofObjectWhereObject=m[ofObjectWhere].getObject(),object=m[where].getObject();
	if (ofObjectWhereObject>=0 && (objects[ofObjectWhereObject].male ^ objects[ofObjectWhereObject].female))
	{
		objects[object].male=objects[ofObjectWhereObject].male;
		objects[object].female=objects[ofObjectWhereObject].female;
	}
	if (m[ofObjectWhere].objectMatches.size()<2 || m[ofObjectWhere].objectMatches.size()>=4)
		return false;
	int begin;
	if (currentSpeakerGroup>=speakerGroups.size() && currentSpeakerGroup)
		begin=speakerGroups[currentSpeakerGroup-1].sgEnd;
	else if (!currentSpeakerGroup)
		begin=0;
	else
		begin=speakerGroups[currentSpeakerGroup-1].sgBegin; // make this currentSpeakerGroup-1 for 'the second of the two men'
	bool isPreviouslyMentionedWithinSG[3],allMentioned=true,noneMentioned=true;
	int whereMentioned[3],lastMentioned=-1,beforeLastMentioned=-1,notMentioned=-1;
	memset(whereMentioned,-1,sizeof(whereMentioned));
	for (unsigned int I=0; I<m[ofObjectWhere].objectMatches.size(); I++)
	{
		if (!(isPreviouslyMentionedWithinSG[I]=((whereMentioned[I]=locationBeforeExceptFuture(m[ofObjectWhere].objectMatches[I].object,where))>begin)))
		{
			allMentioned=false;
			notMentioned=I;
			continue;
		}
		noneMentioned=false;
		if (lastMentioned==-1)
			lastMentioned=I;
		else if (whereMentioned[lastMentioned]<whereMentioned[I])
		{
			beforeLastMentioned=lastMentioned;
			lastMentioned=I;
		}
		else if (beforeLastMentioned==-1 || whereMentioned[beforeLastMentioned]<whereMentioned[I])
			beforeLastMentioned=I;
	}
	wstring tmpstr,tmpstr1,tmpstr2;
	wo=-2-wo;
  if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:resolveWordOrderOfObject [from begin=%d] wo=%d:(%d:%s %d:%s %d:%s) allMentioned=%s noneMentioned=%s lastMentioned=%d beforeLastMentioned=%d notMentioned=%d",
		 where,begin,wo,whereMentioned[0],(whereMentioned[0]<0) ? L"":objectString(m[ofObjectWhere].objectMatches[0].object,tmpstr,true).c_str(),
										whereMentioned[1],(whereMentioned[1]<0) ? L"":objectString(m[ofObjectWhere].objectMatches[1].object,tmpstr1,true).c_str(),
				 					  whereMentioned[2],(whereMentioned[2]<0) ? L"":objectString(m[ofObjectWhere].objectMatches[2].object,tmpstr2,true).c_str(),
							      (allMentioned) ? L"true":L"false",(noneMentioned) ? L"true":L"false",lastMentioned,beforeLastMentioned,notMentioned);
	if (noneMentioned || wo==-11 || wo==-12)
	{
		objectMatches=m[ofObjectWhere].objectMatches;
		return true;
	}
	switch (wo)
	{
	case -2: // the other of the two men
		if (allMentioned)
			objectMatches.push_back(m[ofObjectWhere].objectMatches[beforeLastMentioned]);
		else
			objectMatches.push_back(m[ofObjectWhere].objectMatches[notMentioned]);
		return true;
	case -3: // another of the bicyclers
		return false;
	case -4:// the second of the two men
	case -8:// the latter of the two men
		if (allMentioned)
			objectMatches.push_back(m[ofObjectWhere].objectMatches[lastMentioned]);
		else
			objectMatches.push_back(m[ofObjectWhere].objectMatches[notMentioned]);
		return true;
	case -5:// the first of the two men
	case -7:// the former of the two men
		if (allMentioned)
			objectMatches.push_back(m[ofObjectWhere].objectMatches[beforeLastMentioned]);
		return true;
	case -6:// the third of the trio
		if (allMentioned)
			objectMatches.push_back(m[ofObjectWhere].objectMatches[lastMentioned]);
		else
			objectMatches.push_back(m[ofObjectWhere].objectMatches[notMentioned]);
		return true;
	case -9:// the that of the two men
		return false;
	case -10:// the this of the two men
		return false;
	case -11:// two of the two men - already addressed by above
		return false;
	case -12:// three of the two men - already addressed by above
		return false;
	case -13:// one of the two men - not applicable
		return false;
	}
	return false;
}

// check for a phrase immediately after the object that agrees in gender and number and is not a subject
int cSource::checkSubsequent(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool resolveForSpeaker,bool avoidCurrentSpeaker,vector <cOM> &objectMatches)
{ LFS
  int nextPosition=m[where].endObjectPosition;
	wstring word=m[where].word->first,tmpstr;
    // handle 'one' of the vacant seats
  if ((m[where].queryWinnerForm(numeralCardinalForm)>=0 || word==L"both" || word==L"most" || word==L"more" || word==L"less" || word==L"any" || word==L"all" || word==L"couple" || 
		  (where>0 && word==L"lot" && m[where-1].word->first==L"a")) && 
		where+2<(signed)m.size() && m[where+1].word->first==L"of" && m[where+2].principalWherePosition>=0)
  {
		int forwardObjectPosition=m[where+2].principalWherePosition;
		resolveObject(forwardObjectPosition,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,word==L"both");
		// Tommy was one of those englishmen
		if (word==L"one" && m[forwardObjectPosition].getObject()>=0 && objects[m[forwardObjectPosition].getObject()].plural &&
			  m[forwardObjectPosition].word->second.mainEntry!=wNULL && m[forwardObjectPosition].word->second.mainEntry!=m[forwardObjectPosition].word &&
				!(m[forwardObjectPosition].word->second.mainEntry->second.inflectionFlags&PLURAL))
		{
			// this will potentially move every pointer to the source map, so forget it for now.
			//m.push_back(WordMatch(m[forwardObjectPosition].word->second.mainEntry,0));
			//vector <cObject>::iterator co=objects.begin()+m[forwardObjectPosition].getObject();
			//objects.push_back(cObject(co->objectClass,co->name,co->firstLocation,co->begin,co->end,co->PMAElement,co->getOwnerWhere(),
      //               co->male,co->female,co->neuter,false));
			objectMatches.push_back(cOM(m[forwardObjectPosition].getObject(),SALIENCE_THRESHOLD));
			if (objects[m[forwardObjectPosition].getObject()].neuter)
			{
				objects[m[where].getObject()].neuter=true;
				objects[m[where].getObject()].male=objects[m[where].getObject()].female=false;
			}
			singularizedObjects.push_back(where);
			singularizedObjects.push_back(m[forwardObjectPosition].getObject());
		}
		else
		{
			int o=m[forwardObjectPosition].getObject();
			if (o>=0 && (!objects[o].male && !objects[o].female && objects[o].neuter))
			{
				objects[m[where].getObject()].neuter=true;
				objects[m[where].getObject()].male=objects[m[where].getObject()].female=false;
			}
			if (m[forwardObjectPosition].objectMatches.size())
				objectMatches=m[forwardObjectPosition].objectMatches;
			else if (o>=0)
				objectMatches.push_back(cOM(o,SALIENCE_THRESHOLD));
		}
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:added match (2) %s.",where,objectString(objectMatches,tmpstr,false).c_str());
    nextPosition=m[forwardObjectPosition].endObjectPosition;
  }
	if (nextPosition>=(signed)m.size() || nextPosition<0) return -1;
	if (m[nextPosition].word->first!=L",")
	{
		if (m[nextPosition].principalWherePosition<0 || !(m[m[nextPosition].principalWherePosition].objectRole&RE_OBJECT_ROLE)) return false;
	}
	else 
		nextPosition++;
	if ((nextPosition=m[nextPosition].principalWherePosition)<0)
		return -1;
	bool nextWasHail=(m[nextPosition].objectRole&HAIL_ROLE)!=0;
	if (matchByAppositivity(where,nextPosition))
	{
		if (m[where].getObject()<0 || objects[m[where].getObject()].objectClass!=NAME_OBJECT_CLASS)
			objectMatches.push_back(cOM(m[nextPosition].getObject(),SALIENCE_THRESHOLD));
		else 
		{
			// if replaceInSection used, hail expressions may be incorporated - 'Tuppence, old girl,...'  but Tuppence is not old!
			objects[m[where].getObject()].aliases.push_back(m[nextPosition].getObject());
			if (m[where].beginObjectPosition>0 && m[m[where].beginObjectPosition-1].queryForm(L"quotes")>=0 && nextWasHail)
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Acquired HAIL role (3).",nextPosition);
				m[nextPosition].objectRole|=HAIL_ROLE;
			}
			wstring tmpstr2;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Object %s gained alias %s (5).",where,objectString(m[where].getObject(),tmpstr,true).c_str(),objectString(m[nextPosition].getObject(),tmpstr2,true).c_str());
		}
		return nextPosition;
	}
	return -1;
}

bool cSource::matchByAppositivity(int where,int nextPosition)
{ LFS
  if (m[nextPosition].objectRole&(SUBJECT_ROLE|MNOUN_ROLE) || (m[where].objectRole&MNOUN_ROLE))
		return false;
	// reject 'one of them, sir.' but not 'a man, a certain man ...'
	if (m[nextPosition].endObjectPosition-m[nextPosition].beginObjectPosition==1 && (m[nextPosition].word->second.flags&tFI::genericGenderIgnoreMatch))
		return false;
	int object=m[where].getObject(),reObject=m[nextPosition].getObject();
	if (reObject<0) return false;
	// 'him, the man of my dreams' but not 'him, Ivan.'
	if (m[where].queryWinnerForm(personalPronounAccusativeForm)>=0 && objects[reObject].objectClass==NAME_OBJECT_CLASS)
		return false;
	// an appositive match for the use of inQuote, wiping out the HAIL_ROLE, must not have an I or you (but 'one' is OK)
	if ((m[where].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON)) && !(m[where].word->second.inflectionFlags&THIRD_PERSON))
		return false;
	// cannot both be numbers - one, two, three, go!
	if (m[where].endObjectPosition-m[where].beginObjectPosition==1 && m[nextPosition].endObjectPosition-m[nextPosition].beginObjectPosition==1 && 
		  m[where].queryWinnerForm(numeralCardinalForm)>=0 && m[nextPosition].queryWinnerForm(numeralCardinalForm)>=0)
		return false;
	// they cannot both be places
	if (objects[object].getSubType()>=0 && objects[reObject].getSubType()>=0)
		return false;
	// agree in gender (neuter and non-neuter)?
	bool objectGendered=(objects[object].male || objects[object].female);
	bool reObjectGendered=(objects[reObject].male || objects[reObject].female);
	if ((objectGendered && !objects[object].neuter) && !reObjectGendered)
		return false;
	if ((reObjectGendered && !objects[reObject].neuter) && !objectGendered)
		return false;
	// agree in gender (male/female)?
	if ((objects[object].male ^ objects[object].female) && (objects[reObject].male ^ objects[reObject].female) && 
		  objects[object].male!=objects[reObject].male)
		return false;
	// agree in number?
	if (objects[object].plural!=objects[reObject].plural)
		return false;
	// don't match two separate speakers together, no matter what
	if (objects[object].objectClass==NAME_OBJECT_CLASS && objects[reObject].objectClass==NAME_OBJECT_CLASS &&
		  objects[object].numIdentifiedAsSpeaker && objects[reObject].numIdentifiedAsSpeaker)
		return false;
	wstring tmpstr,tmpstr2;
  if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION|LOG_SG,L"%d:resolved object %d:%s to %d:%s by appositivity",where,where,objectString(object,tmpstr,false).c_str(),nextPosition,objectString(reObject,tmpstr2,false).c_str());
	m[nextPosition].objectRole|=RE_OBJECT_ROLE;
	// you are a clever woman, Rita;
	if ((m[nextPosition].objectRole&HAIL_ROLE) && (m[where].objectRole&IS_OBJECT_ROLE) && m[where].relSubject>=0 && 
		  (m[m[where].relSubject].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON|THIRD_PERSON))==SECOND_PERSON)
	{
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%06d:Kept HAIL role (appositivity with SECOND_PERSON).",nextPosition);
	}
	else
	{
		if (debugTrace.traceRole && (m[nextPosition].objectRole&HAIL_ROLE))
			lplog(LOG_ROLE,L"%06d:Removed HAIL role (appositivity).",nextPosition);
		m[nextPosition].objectRole&=~HAIL_ROLE;
	}
	/*
	// because this is a reference to a third person, this person cannot be in the speakerGroup, unless the speaker is referring to the audience
	//  Besides I[tuppence] have seen that man[tommy] , Boris Something , since .
	if (inPrimaryQuote && currentSpeakerGroup<speakerGroups.size() && lastOpeningPrimaryQuote!=-1 && 
		  in(reObject,m[lastOpeningPrimaryQuote].audienceObjectMatches)==m[lastOpeningPrimaryQuote].audienceObjectMatches.end() && 
			speakerGroups[currentSpeakerGroup].speakers.find(reObject)!=speakerGroups[currentSpeakerGroup].speakers.end())
	{
	  if (t.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:removed speaker %s from speakerGroup %s by appositivity (insufficient reference)",
		        where,objectString(reObject,tmpstr,true).c_str(),toText(speakerGroups[currentSpeakerGroup],tmpstr2));
		speakerGroups[currentSpeakerGroup].speakers.erase(reObject);
	}
	*/
	return true;
}

void cSource::setQuoteContainsSpeaker(int where,bool inPrimaryQuote)
{ LFS
	bool allIn,oneIn;
	if (inPrimaryQuote && speakerGroupsEstablished && !(m[where].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE)) && 
			(m[where].objectRole&FOCUS_EVALUATED) && currentSpeakerGroup<speakerGroups.size() &&
		  intersect(where,speakerGroups[currentSpeakerGroup].speakers,allIn,oneIn) && allIn)
	{
		wstring tmpstr;
		m[lastOpeningPrimaryQuote].flags|=WordMatch::flagQuoteContainsSpeaker;
	m[lastOpeningPrimaryQuote].setRelObject(where);
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:%d:Speaker %s talking about other speakers/audience",where,lastOpeningPrimaryQuote,objectString(m[where].getObject(),tmpstr,false).c_str());
	}
}

vector <cLocalFocus>::iterator cSource::ownerObjectInLocal(int o)
{ LFS
	if (objects[o].getOwnerWhere()<0 || objects[o].objectClass!=BODY_OBJECT_CLASS) return localObjects.end();
	vector <cLocalFocus>::iterator olsi;
	int ow,oo;
	if ((oo=m[ow=objects[o].getOwnerWhere()].getObject())>=0 && (olsi=in(oo))!=localObjects.end() && olsi->om.salienceFactor>=0)
		return olsi;
	for (unsigned int I=0; I<m[ow].objectMatches.size(); I++)
		if ((olsi=in(m[ow].objectMatches[I].object))!=localObjects.end() && olsi->om.salienceFactor>=0)
			return olsi;
	return localObjects.end();
}

bool cObject::hasAgeModifier(vector <WordMatch> &m,wchar_t *modifiers[])
{ LFS
	for (int I=0; modifiers[I]; I++)
		for (int J=begin; J<end; J++)
			if (m[J].word->first==modifiers[I])
				return true;
	return false;
}

wchar_t *olderAgeModifiers[]={ L"old",L"elderly",L"older",L"aged",NULL };
wchar_t *youngerAgeModifiers[]={ L"young",L"teen",L"preteen",L"adolescent",L"miss",NULL };
int cObject::setGenericAge(vector <WordMatch> &m)
{ LFS
	tIWMM w=m[originalLocation].getMainEntry();
	if ((objectClass==GENDERED_GENERAL_OBJECT_CLASS || objectClass==GENDERED_RELATIVE_OBJECT_CLASS || 
		   objectClass==NAME_OBJECT_CLASS || objectClass==META_GROUP_OBJECT_CLASS) && 
		  ((w->second.flags&tFI::genericGenderIgnoreMatch) || (w->second.flags&tFI::genericAgeGender)))
	{
		bool hasYoung=hasAgeModifier(m,youngerAgeModifiers),hasOld=hasAgeModifier(m,olderAgeModifiers);
		// four layers:
		// 0:young girl or boy/child
		// 1:boy/young man/young fellow/young gentleman/young woman/young lady
		// 2:man/fellow/gentleman/woman/lady/husband/wife
		// 3:old man
		
		if (w->first==L"girl" || w->first==L"boy" || w->first==L"child")
		{
			if (hasOld) 
			{
				associatedAdjectives.clear();  // don't let other objects assume these adjectives as they are contradictory
				return objectGenericAge=-1;
			}
			if (hasYoung) return objectGenericAge=0;
			return objectGenericAge=1;
		}
		if (w->first==L"baby")
			return objectGenericAge=0;
		if (hasYoung)
			return objectGenericAge=1;
		else if (hasOld)
			return objectGenericAge=3;
		return objectGenericAge=2;
	}
	return objectGenericAge=-1;
}

// wchar_t *genericGender[]={ L"man", L"fellow", L"gentleman", L"sir", L"woman", L"lady", L"madam", L"girl", L"miss",L"missus",NULL };
bool cObject::updateGenericGender(int where,tIWMM w,int fromAge,wchar_t *fromWhere,sTrace &t)
{ LFS
  if (objectClass!=PRONOUN_OBJECT_CLASS && (w->second.flags&tFI::genericGenderIgnoreMatch))
	{
		int keep;
		if (genericNounMap.find(w)==genericNounMap.end())
			keep=genericNounMap[w]=1;
		else
			keep=genericNounMap[w]++;
		wstring tmpstr;
		if (mostMatchedGeneric==wNULL || genericNounMap[mostMatchedGeneric]<keep)
			mostMatchedGeneric=w;
		if (t.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:Incorporated generic noun %s (%s) (AGE %d)",where,w->first.c_str(),fromWhere,fromAge);
		if (fromAge>=0)
		{
			genericAge[fromAge]++;
			if (mostMatchedAge==-1 || (mostMatchedAge!=fromAge && genericAge[fromAge]>genericAge[mostMatchedAge]))
				mostMatchedAge=fromAge;
		}
		return true;
	}
	return false;
}

void cObject::updateGenericGenders(map <tIWMM,int,tFI::cRMap::wordMapCompare> &replacedGenericNounMap,int *replacedGenericAge)
{ LFS
	for (map <tIWMM,int,tFI::cRMap::wordMapCompare>::iterator gnmi=replacedGenericNounMap.begin(),gnmiEnd=replacedGenericNounMap.end(); gnmi!=gnmiEnd; gnmi++)
	{
		if (genericNounMap.find(gnmi->first)==genericNounMap.end())
			genericNounMap[gnmi->first]=gnmi->second;
		else
			genericNounMap[gnmi->first]+=gnmi->second;
	}
	for (int I=0; I<4; I++)
	{
		genericAge[I]+=replacedGenericAge[I];
		if (genericAge[I] && (mostMatchedAge==-1 || (mostMatchedAge!=I && genericAge[I]>genericAge[mostMatchedAge])))
			mostMatchedAge=I;
	}
}

// if object being matched has a generic gendered head (HEAD)
// if more than one local object has > SALIENCE_THRESHOLD and mostMatchedGeneric (MMG) != NULL
// for each of these objects, 
//   if HEAD is not mapped, or if HEAD<MMG/2, AND numHavingSalience==numGenericObjects decrease salience.
//   if HEAD == MMG, increase salience.
void cSource::includeGenericGenderPreferences(int where,vector <cObject>::iterator object)
{ LFS
	tIWMM w=m[object->originalLocation].getMainEntry(),localw;
	wstring word=w->first;
	//int flags=w->second.flags;
	if ((object->objectClass!=GENDERED_GENERAL_OBJECT_CLASS && object->objectClass!=GENDERED_RELATIVE_OBJECT_CLASS && object->objectClass!=NAME_OBJECT_CLASS && object->objectClass!=META_GROUP_OBJECT_CLASS) || 
		  (!(w->second.flags&tFI::genericGenderIgnoreMatch) && !(w->second.flags&tFI::genericAgeGender))) return;
	int numGenericObjects=0,numSalienceObjects=0,numAlternateGenericObjects=0;
	vector < vector <cLocalFocus>::iterator > decreaseSalience;
	vector < vector <cLocalFocus>::iterator > increaseSalience;
	vector <cLocalFocus>::iterator genericLocalObject=localObjects.end(),salientLocalObject=localObjects.end(),alternateGenericLocalObject=localObjects.end();
  for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
	{
		if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && ownerObjectInLocal(lsi->om.object)==localObjects.end())
		{
			if (lsi->om.salienceFactor>=0)
			{
				numSalienceObjects++;
				if (objects[lsi->om.object].mostMatchedGeneric!=wNULL)
				{
					numGenericObjects++;
					genericLocalObject=lsi;
				}
				else
					salientLocalObject=lsi;
				if (objects[lsi->om.object].objectGenericAge>=0 && object->objectGenericAge>=0 && 
					  objects[lsi->om.object].objectClass!=NAME_OBJECT_CLASS && object->objectClass!=NAME_OBJECT_CLASS)
				{
						localw=m[objects[lsi->om.object].originalLocation].getMainEntry();
						if (((localw->second.flags&tFI::genericGenderIgnoreMatch) || (localw->second.flags&tFI::genericAgeGender)))
						{
							if (objects[lsi->om.object].objectGenericAge==object->objectGenericAge)
							{
								lsi->om.salienceFactor+=500;
								itos(L"+MATCHING_GENERIC[+",500,lsi->res,L"]");
							}
							else if (localw!=w) // not the same word
								decreaseSalience.push_back(lsi);
						}
				}
			}
			if (lsi->om.salienceFactor>=MINIMUM_SALIENCE_WITH_MATCHED_ADJECTIVES && object->objectGenericAge>=0)
			{
				wstring w2;
				wordString(objects[lsi->om.object].associatedNouns,w2);
				if ((objects[lsi->om.object].mostMatchedAge==object->objectGenericAge) ||  
					  (abs(objects[lsi->om.object].mostMatchedAge-object->objectGenericAge)<=1 && 
					   find(objects[lsi->om.object].associatedNouns.begin(),objects[lsi->om.object].associatedNouns.end(),m[where].word)!=objects[lsi->om.object].associatedNouns.end()))
				{
					numAlternateGenericObjects++;
					alternateGenericLocalObject=lsi;
				}
			}
		}
	}
	vector <cObject>::iterator genericObject=(genericLocalObject==localObjects.end()) ? objects.begin() : objects.begin()+genericLocalObject->om.object;
	if (numGenericObjects==1 && numSalienceObjects==1 && !numAlternateGenericObjects && !(m[where].objectRole&HAIL_ROLE))
	{
		if (genericObject->objectClass!=NAME_OBJECT_CLASS) return;
		if (object->objectGenericAge>=0 && genericObject->genericAge[object->objectGenericAge]==0 && genericObject->mostMatchedAge>=0 && genericObject->genericAge[genericObject->mostMatchedAge]>1)
			decreaseSalience.push_back(genericLocalObject);
	  if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr2;
			lplog(LOG_RESOLUTION,L"%06d:GENERIC:%s[sf=%d,AGE=%d(%d)] matched age=%d(%d)%s",where,
				objectString(genericLocalObject->om,tmpstr2,true).c_str(),genericLocalObject->om.salienceFactor,
				genericObject->mostMatchedAge,(genericObject->mostMatchedAge>=0) ? genericObject->genericAge[genericObject->mostMatchedAge]:-1,
				object->objectGenericAge,(object->objectGenericAge>=0) ? genericObject->genericAge[object->objectGenericAge]:-1,
				(decreaseSalience.size()) ? L" DECREASED":L"");
		}
	}
	if (numGenericObjects==1 && numSalienceObjects==2)
	{
		if ((genericObject->objectClass!=NAME_OBJECT_CLASS && genericObject->objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && 
			   genericObject->objectClass!=GENDERED_DEMONYM_OBJECT_CLASS && genericObject->objectClass!=GENDERED_RELATIVE_OBJECT_CLASS) || 
			  (objects[salientLocalObject->om.object].objectClass!=NAME_OBJECT_CLASS && objects[salientLocalObject->om.object].objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
			   objects[salientLocalObject->om.object].objectClass!=GENDERED_DEMONYM_OBJECT_CLASS && objects[salientLocalObject->om.object].objectClass!=GENDERED_RELATIVE_OBJECT_CLASS))
			return;
		int HEAD=(genericObject->genericNounMap.find(w)==genericObject->genericNounMap.end()) ? 0 : genericObject->genericNounMap[w];
		int MMG=genericObject->genericNounMap[genericObject->mostMatchedGeneric];
	  if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr,tmpstr2;
			lplog(LOG_RESOLUTION,L"%06d:GENERIC:%s[sf=%d,HEAD=%d,MMG=%d,AGE=%d] salient=%s[sf=%d] matched age=%d",where,
				objectString(genericLocalObject->om,tmpstr2,true).c_str(),genericLocalObject->om.salienceFactor,HEAD,MMG,genericObject->mostMatchedAge,
				objectString(salientLocalObject->om,tmpstr,true).c_str(),salientLocalObject->om.salienceFactor,
				object->objectGenericAge);
		}
		if (HEAD<MMG/2 && salientLocalObject->om.salienceFactor/2<genericLocalObject->om.salienceFactor && 
			  salientLocalObject->om.salienceFactor>SALIENCE_THRESHOLD && object->objectGenericAge!=genericObject->mostMatchedAge) 
			decreaseSalience.push_back(genericLocalObject);
		if (salientLocalObject->om.salienceFactor/2>genericLocalObject->om.salienceFactor && object->objectGenericAge==genericObject->mostMatchedAge) //w==genericObject->mostMatchedGeneric)
			increaseSalience.push_back(genericLocalObject);
	}
	// a battle between generics, one non-salient, but right and the other salient, but wrong (48712: the girl shouldn't match to Rita)
	else if (numAlternateGenericObjects==1 && numGenericObjects==1 && numSalienceObjects==1 && 
					 (objects[alternateGenericLocalObject->om.object].mostMatchedGeneric==w || (objects[alternateGenericLocalObject->om.object].mostMatchedAge==object->objectGenericAge && object->objectGenericAge>=0)) &&
					 (object->objectGenericAge>=0 && genericObject->genericAge[object->objectGenericAge]==0 && genericObject->mostMatchedAge>=0 && genericObject->genericAge[genericObject->mostMatchedAge]>1))
	{
		wstring tmpstr;
		decreaseSalience.push_back(genericLocalObject);
    alternateGenericLocalObject->om.salienceFactor+=3500;
    alternateGenericLocalObject->res+=L"+MATCHING_ALT_GENERIC[+"+itos(3500,tmpstr)+L"]";
	  if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr2;
			genericObject=objects.begin()+genericLocalObject->om.object;
			lplog(LOG_RESOLUTION,L"%06d:GENERIC:%s[sf=%d,AGE=%d(%d)] alternative=%s[sf=%d] matched age=%d(%d)",where,
				objectString(genericLocalObject->om,tmpstr2,true).c_str(),genericLocalObject->om.salienceFactor,
				genericObject->mostMatchedAge,(genericObject->mostMatchedAge>=0) ? genericObject->genericAge[genericObject->mostMatchedAge]:-1,
				objectString(alternateGenericLocalObject->om,tmpstr,true).c_str(),alternateGenericLocalObject->om.salienceFactor,
				object->objectGenericAge,(object->objectGenericAge>=0) ? genericObject->genericAge[object->objectGenericAge]:-1);
		}
	}
	// a battle between generics, all salient
	else if (numGenericObjects>1 && numSalienceObjects>1)
	{
		bool cancelGenericProcessing=false;
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
		{
			if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=0 && objects[lsi->om.object].mostMatchedGeneric!=wNULL)
			{
				vector <cObject>::iterator lso=objects.begin()+lsi->om.object;
				int lsoMMA=lso->mostMatchedAge;
				bool allIn,oneIn;
				if (object->objectGenericAge==1 && lsoMMA==2 && intersect(lso->associatedAdjectives,youngerAgeModifiers,allIn,oneIn)) 
					lsoMMA=1;
				// don't punish if most matched is 2 (generic man)?
				else if (object->objectGenericAge>=0 && lso->genericAge[object->objectGenericAge]==0 && lsoMMA>=0 && lso->genericAge[lsoMMA]>1) // && lso->mostMatchedAge!=2)
					decreaseSalience.push_back(lsi);
				if (object->objectGenericAge==lsoMMA)
					increaseSalience.push_back(lsi);
				// don't reward Julius for matching 'sir' if Tommy is also sometimes called 'sir' (63649)
				if (object->objectGenericAge!=lsoMMA && lso->genericAge[lsoMMA]>1 && lso->genericAge[object->objectGenericAge]>lso->genericAge[lsoMMA]/3)
				{
					wstring tmpstr;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Cancel positive generic processing because of uncertainty might lead to bias against %s",where,objectString(lsi->om,tmpstr,true).c_str());
					cancelGenericProcessing=true;
				}
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr2;
					lplog(LOG_RESOLUTION,L"%06d:GENERIC:%s[sf=%d] MOAGE=%d:%d AGE=%d:%d",where,objectString(lsi->om,tmpstr2,true).c_str(),lsi->om.salienceFactor,
						object->objectGenericAge,lso->genericAge[object->objectGenericAge],lsoMMA,lso->genericAge[lsoMMA]);
				}
			}
		}
		if (cancelGenericProcessing)
			increaseSalience.clear();
	}
	wstring tmpstr;
	for (vector < vector <cLocalFocus>::iterator >::iterator dsi=decreaseSalience.begin(),dsiEnd=decreaseSalience.end(); dsi!=dsiEnd; dsi++)
	{
		vector <cLocalFocus>::iterator lsi=*dsi;
    lsi->om.salienceFactor-=DISALLOW_SALIENCE;
    itos(L"-WRONG_GENERIC[-",DISALLOW_SALIENCE,lsi->res,L"]");
		for (vector <cLocalFocus>::iterator llsi=localObjects.begin(),llsiEnd=localObjects.end(); llsi!=llsiEnd; llsi++)
		{
			if (objects[llsi->om.object].getOwnerWhere()>=0 && in(lsi->om.object,objects[llsi->om.object].getOwnerWhere()))
			{
				llsi->om.salienceFactor-=DISALLOW_SALIENCE;
				itos(L"-WRONG_GENERIC[-",DISALLOW_SALIENCE,llsi->res,L"]");
			}
		}
	}
	if (numSalienceObjects==numGenericObjects && numGenericObjects==increaseSalience.size())
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:Cancel generic processing because all objects match",where);
		return;
	}
	for (vector < vector <cLocalFocus>::iterator >::iterator isi=increaseSalience.begin(),isiEnd=increaseSalience.end(); isi!=isiEnd; isi++)
	{
		vector <cLocalFocus>::iterator lsi=*isi;
    lsi->om.salienceFactor+=5000;
    itos(L"+MATCHING_GENERIC[+",5000,lsi->res,L"]");
	}
}

// in secondary quotes, inPrimaryQuote=false
int cSource::resolveAdjectivalObject(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,bool resolveForSpeaker)
{ LFS
  if (m[where].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON)) 
	{
		m[where].flags&=~WordMatch::flagObjectResolved;
		return -1;
	}
	int beginPosition=where,lastGenderedAge;
	while (beginPosition>=0 && m[beginPosition].principalWherePosition<0) beginPosition--;
	if (beginPosition<0) return where;
  if (unResolvablePosition(beginPosition)) return -1;
  vector <int> disallowedReferences;
  pronounCoreferenceFilterLL6(where,lastBeginS1,disallowedReferences);
  adjustSaliencesByGenderNumberAndOccurrenceAgeAdjust(where,-2,inPrimaryQuote,inSecondaryQuote,definitelySpeaker|resolveForSpeaker,lastGenderedAge,disallowedReferences,false,false,false);
  if (m[where].objectRole&(SUBJECT_ROLE|OBJECT_ROLE|SUBOBJECT_ROLE|RE_OBJECT_ROLE|PREP_OBJECT_ROLE|IOBJECT_ROLE))
    adjustSaliencesBySubjectRole(where,lastBeginS1); // the last noun in SUBJECT role in the current sentence gets a bump in salience - especially if there is only one
  adjustSaliencesByParallelRoleAndPlurality(where,inPrimaryQuote,resolveForSpeaker,lastGenderedAge);
	// to his amazement, Tommy ...
	int pw=m[beginPosition].principalWherePosition,end=m[pw].endObjectPosition,subjectWhere=-1,s=-1;
	vector <cLocalFocus>::iterator lss;
	wstring tmpstr;
  if (lastBeginS1<0 && (m[where].objectRole&(SUBJECT_ROLE|OBJECT_ROLE|SUBOBJECT_ROLE|RE_OBJECT_ROLE|PREP_OBJECT_ROLE|IOBJECT_ROLE))==PREP_OBJECT_ROLE &&
		  m[beginPosition-1].pma.queryPattern(L"_INTRO_S1")!=-1 && pw>=0 && end>=0 && m[end].word->first==L"," && (subjectWhere=m[end+1].principalWherePosition)>=0 && 
			(m[subjectWhere].objectRole&SUBJECT_ROLE) && (s=m[subjectWhere].getObject())>=0 && (lss=in(s))!=localObjects.end() && 
			lss->om.salienceFactor>=SALIENCE_THRESHOLD)
	{
		lss->om.salienceFactor+=2000;
		lss->res+=L"LEADING_PP_SUBJECT[+"+itos(2000,tmpstr)+L"]";
	}
	if (m[m[beginPosition].principalWherePosition].objectRole&POV_OBJECT_ROLE)
		m[where].objectRole|=POV_OBJECT_ROLE;
	// make sure body objects aren't being used to possess other body objects
	if (objects[m[m[beginPosition].principalWherePosition].getObject()].objectClass==BODY_OBJECT_CLASS)
	{
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
	  bool preferGenderedOwners=((m[where].word->second.inflectionFlags&(NEUTER_GENDER|MALE_GENDER|FEMALE_GENDER))==(NEUTER_GENDER|MALE_GENDER|FEMALE_GENDER));
		for (; lsi!=lsiEnd; lsi++)
		{
			if (objects[lsi->om.object].objectClass==BODY_OBJECT_CLASS)
			{
				lsi->om.salienceFactor-=DISALLOW_SALIENCE;
				lsi->numMatchedAdjectives=0;
				itos(L"BODY_OWNING_BODY[-",-DISALLOW_SALIENCE,lsi->res,L"]");
			}
			else if (preferGenderedOwners && lsi->om.salienceFactor>SALIENCE_THRESHOLD && (objects[lsi->om.object].male || objects[lsi->om.object].female))
			{
				lsi->om.salienceFactor+=10000;
				itos(L"GENDERED_BODY_OWNER_PREFERENCE[+",10000,lsi->res,L"]");
			}

		}

	}
	excludeSpeakers(where,inPrimaryQuote,inSecondaryQuote);
  if (debugTrace.traceSpeakerResolution)
  {
    wstring tmpstr2;
		lplog(LOG_RESOLUTION,L"%06d:RESOLVING possessive determiner %s%s [%s minAge %d]",where,phraseString(where,where+1,tmpstr,true).c_str(),m[where].roleString(tmpstr2).c_str(),(resolveForSpeaker) ? L"speaker":L"object",lastGenderedAge);
    printLocalFocusedObjects(where,PRONOUN_OBJECT_CLASS);
  }
  chooseBest(where,false,inPrimaryQuote,inSecondaryQuote,resolveForSpeaker,false);
  return where;
}

// Lappin and Leass 2.1.3
//  reflexive_pronoun
//    {"himself",SINGULAR|MALE_GENDER|THIRD_PERSON},{"herself",SINGULAR|FEMALE_GENDER|THIRD_PERSON},
//    {"itself",SINGULAR|NEUTER_GENDER|THIRD_PERSON},{"themselves",PLURAL|THIRD_PERSON},{NULL,0}};
//  A reflexive anaphor must be c-commanded by its antecedent (p. 59 Anaphora resolution)
//    Sylvia admires herself
//
// Lappin and Leass 2.1.3 (p. 539) - not implemented
// subj > agent > obj > (iobjlpobj)
// subj is the surface subject slot, agent is the deep subject slot of a verb heading a passive VP,
// obj is the direct object slot, iobj is the indirect object slot, and pobj is the object of a PP complement
// of a verb, as in put NP on NP.
// A noun phrase N is a possible antecedent binder for a lexical anaphor A (i.e., reciprocal or reflexive pronoun)
// iff N and A do not have incompatible agreement features, and one of the following five conditions holds.
//1. A is in the argument domain of N, and N fills a higher argument slot than A.
//   They wanted to see themselves.
//   Mary knows the people who John introduced to each other.
//2. A is in the adjunct domain of N.
//   Hei worked by himself.
//   Which friends plan to travel with each other?
//3. A is in the NP domain of N.
//   John likes Bill's portrait of himself.
//4. N is an argument of a verb V, there is an NP Q in the argument domain or the adjunct domain of N such that
//   Q has no noun determiner, and (i) A is an argument of Q, or
//   (ii) A is an argument of a preposition PREP and PREP is an adjunct of Q.
//   They told stories about themselves.
//5. A is a determiner of a noun Q, and (i) Q is in the argument domain of N and N fills a higher argument slot than Q,
//   or (ii) Q is in the adjunct domain of N.
//   John and Mary like each other's portraits.
// Possible parents: _NOUN_OBJ, _NOUN"C", single element __PP"3"
int cSource::reflexivePronounCoreference(int where, int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool inPrimaryQuote,bool inSecondaryQuote)
{ LFS
	int futureReference=where,verbRel=-1;
	if (lastBeginS1<0) 
	{
		// if at the beginning of the sentence, beginS1 may immediately follow
		// to herself she[tuppence] said: 
		int element;
		if (where && m[where-1].queryForm(prepositionForm)>=0 && (where==1 || isEOS(where-2)) && (element=m[where+1].pma.queryPattern(L"__S1"))!=-1)
		{
			lastBeginS1=where+1;
			futureReference=where+1+m[where+1].pma[element&~matchElement::patternFlag].len;
		}
		else
		{
			// said Tommy to himself.
			if (where>2 && m[where-1].word->first==L"to" && (m[where-2].objectRole&NO_ALT_RES_SPEAKER_ROLE))
				verbRel=where-2;
			else
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:RESOLVING reflexive %s - no lastBeginS1",where,m[where].word->first.c_str());
				// if resolveObject is called before the last containing S1 is hit (in audience resolution)
				// then we want to reresolve at later time.  
				m[where].flags&=~WordMatch::flagObjectResolved; 
				return -1;
			}
		}
	}
  wchar_t *allowedPronouns[]={L"himself",L"herself",L"itself",L"themselves",NULL};
  int I;
  for (I=0; allowedPronouns[I] && m[where].word->first!=allowedPronouns[I]; I++);
  if (!allowedPronouns[I]) 
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING reflexive %s - not in list of resolvable pronouns",where,m[where].word->first.c_str());
		m[where].flags&=~WordMatch::flagObjectResolved;
		return -1;
	}
  wstring tmpstr;
  bool isMale=false,isFemale=false,isNeuter=false,isPlural=false;
  switch (I)
  {
  case 0: isMale=true; break;
  case 1: isFemale=true; break;
  case 2: isNeuter=true; break;
  case 3: isMale=isFemale=isPlural=true; break;
  }
  vector <cOM> localReferenceObjects;
	if (verbRel>=0)
	{
		if (m[verbRel].objectMatches.empty()) 
			localReferenceObjects.push_back(cOM(m[verbRel].getObject(),SALIENCE_THRESHOLD)); 
		else
			localReferenceObjects=m[verbRel].objectMatches;
	}
  // cover the case He himself / She herself / They themselves
  else if ((I==0 && m[where-1].word->first==L"he") ||
    (I==1 && m[where-1].word->first==L"she") ||
    (I==3 && m[where-1].word->first==L"they"))
    localReferenceObjects=m[where-1].objectMatches;
  // a reflexive pronoun is bounded tightly.
  // George hurt himself.
  // Nelly thinks that George hurt himself.
  // Vicky admires Elitza's picture of herself.
  // Vicky wanted the bicycle more than Elitza herself.
  else
	{
		int IP=m[where].relSubject;
		if ((m[where].objectRole&(OBJECT_ROLE|PREP_OBJECT_ROLE|SUBJECT_ROLE))==OBJECT_ROLE && IP>=0 && m[IP].getObject()>=0 &&
			  (objects[m[IP].getObject()].male==isMale || objects[m[IP].getObject()].female==isFemale))
		{
			int object=m[IP].getObject();
			if (futureReference!=where)
				resolveObject(IP,false,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,false,false,false);
      if (m[IP].objectMatches.size())
        localReferenceObjects.insert(localReferenceObjects.end(),m[IP].objectMatches.begin(),m[IP].objectMatches.end());
      else
        localReferenceObjects.push_back(cOM(object,SALIENCE_THRESHOLD));
		}
		else
		{
			for (IP=lastBeginS1; IP<futureReference; IP++)
			{
				int object=m[IP].getObject();
				if (object>=0 && objects[object].objectClass!=REFLEXIVE_PRONOUN_OBJECT_CLASS &&
					(isPlural==objects[object].plural) && // if a singular reflexive pronoun only match with singular objects
					(!isNeuter || (!objects[object].male && !objects[object].female)) && // if neutral (itself) only match with nongendered objects
					(isNeuter || (objects[object].objectClass!=NON_GENDERED_GENERAL_OBJECT_CLASS &&
												objects[object].objectClass!=NON_GENDERED_BUSINESS_OBJECT_CLASS &&
												objects[object].objectClass!=NON_GENDERED_NAME_OBJECT_CLASS)) && // if not neutral, don't match nongendered objects
					(isNeuter ||
					(isMale   && (objects[object].male   || objects[object].objectClass==NAME_OBJECT_CLASS)) ||
					(isFemale && (objects[object].female || objects[object].objectClass==NAME_OBJECT_CLASS)))) // if gendered, the gender must match
				{
					if (futureReference!=where)
						resolveObject(IP,false,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,false,false,false);
					if (m[IP].objectMatches.size())
						localReferenceObjects.insert(localReferenceObjects.end(),m[IP].objectMatches.begin(),m[IP].objectMatches.end());
					else
						localReferenceObjects.push_back(cOM(object,SALIENCE_THRESHOLD));
				}
			}
		}
	}
  if (debugTrace.traceSpeakerResolution)
  {
    if (localReferenceObjects.empty())
      lplog(LOG_RESOLUTION,L"%06d:No local references from begin S1 position %d.",where,lastBeginS1);
    else
      lplog(LOG_RESOLUTION,L"%06d:Local references from begin S1 position %d:%s",where,lastBeginS1,objectString(localReferenceObjects,tmpstr,true).c_str());
  }
  vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end();
  for (; lsi!=lsEnd; lsi++)
  {
    vector <cOM>::iterator s=in(lsi->om.object,localReferenceObjects);
    lsi->clear();
    if (s!=localReferenceObjects.end())
    {
      vector <cObject>::iterator lso=objects.begin()+s->object;
      bool sexUncertain=(!isMale && !isFemale) || (isMale && isFemale);
      bool otherSexUncertain=(!lso->male && !lso->female) || (lso->male && lso->female);
      if (sexUncertain || otherSexUncertain || isMale==lso->male)
        lsi->om.salienceFactor+=s->salienceFactor+SALIENCE_THRESHOLD;
    }
  }
  if (localReferenceObjects.size()==1 &&
    (objects[localReferenceObjects[0].object].male && objects[localReferenceObjects[0].object].female) &&
    (isMale ^ isFemale))
  {
    objects[localReferenceObjects[0].object].male=isMale;
    objects[localReferenceObjects[0].object].female=isFemale;
		addDefaultGenderedAssociatedNouns(localReferenceObjects[0].object);
  }
  return 0;
}

wchar_t *cSource::loopString(int where, wstring &tmpstr)
{ LFS
	int relPrep=where;
	wstring tmp2;
	tmpstr.clear();
	while (relPrep!=-1)
	{
		tmpstr+=itos(relPrep,tmp2)+L" ";
		relPrep=m[relPrep].relPrep;
		if (relPrep==where) break;
	}
	return (wchar_t *)tmpstr.c_str();
}
// Lappin and Leass 2.1.1
// Syntactic filter on Pronoun - NP coreference
// P stands for a pronoun.  N stands for any Noun Phrase, including a pronoun
// 1. incompatible agreement features (agrees in number, person and gender) - already passed

//  personal_pronoun_nominative
//    {"one",SINGULAR|NEUTER_GENDER|THIRD_PERSON},
//    {"he",SINGULAR|MALE_GENDER|THIRD_PERSON},{"she",SINGULAR|FEMALE_GENDER|THIRD_PERSON},
//    {"it",SINGULAR|NEUTER_GENDER|THIRD_PERSON},{"they",PLURAL|THIRD_PERSON}
//  personal_pronoun_accusative
//    {"him",SINGULAR|MALE_GENDER|THIRD_PERSON},{"her",SINGULAR|FEMALE_GENDER|THIRD_PERSON},
//    {"it",SINGULAR|NEUTER_GENDER|THIRD_PERSON},{"them",PLURAL|THIRD_PERSON}
// For 2, 3, 4, and 5:
//      P and N in the same __S1.  P has THIRD_PERSON (above classes) or N has THIRD_PERSON (above classes).

// For 2, 3: P is in SUBJECT, and N is in OBJECT or vice-versa
// 2. P is in argument domain of N (P is subject, N is object or vice versa)
//    She likes her.  John seems to want to see him.
//      OBJECT P has SUBJECT_ROLE, N has OBJECT_ROLE or vice-versa
// 3. P is in the adjunct domain of N (P is in subject slot of __S1 and _PP containing N is in the object slot or vica versa)
//    She sat near her.  Sally sat next to her.  She sat next to Jennifer.
//      P has SUBJECT_ROLE and N is a PREPOBJECT_ROLE.  _PP is owned by either __ALLOBJECTS or C2__S1.

// 4. P is an argument of a head H, N is not a pronoun and N is contained in H.
//    He believes that the man is amusing.
//    That is the man he said John wrote about.
//
//  NOT That is the man John said he wrote about.
// 5. P is in the NP domain of N (included in the same _NOUN pattern)
//    John's portait of him in interesting.
// LL is Lappin and Leass 2345 are the rule numbers
// lastBeginS1 is the index of the start of the pattern S1 which contains P.
// rObject is P in LL procedure outlined above.
// subjectCataRestriction occurs when the subject is singular and has more than one match.
int cSource::coreferenceFilterLL2345(int where,int rObject,vector <int> &disallowedReferences,
																		int lastBeginS1,int lastRelativePhrase,int lastQ2,bool &mixedPlurality,int &subjectCataRestriction)
{ LFS
	if (lastRelativePhrase==-1 && lastBeginS1==-1 && lastQ2==-1) return 0;
	int subjectObject=-1,indirectObject=-1,directObject=-1,element,begin=where,end=-1,endS1=-1,wcpo,compoundLoop=0;
  wstring tmpstr;
	for (int wcp=m[where].previousCompoundPartObject; wcp>=0; wcp=m[wcp].previousCompoundPartObject,compoundLoop++)
	{
		if (compoundLoop>10)
			break;
		if (disallowReference(wcpo=m[wcp].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 5 ruled out %d:%s (1)",where,wcp,objectString(wcpo,tmpstr,false).c_str());
		if (wcpo>=0 && (m[wcp].objectMatches.size()==1 || objects[wcpo].plural) && objects[wcpo].objectClass!=BODY_OBJECT_CLASS)
			for (unsigned int I=0; I<m[wcp].objectMatches.size(); I++)
				if (disallowReference(wcpo=m[wcp].objectMatches[I].object,disallowedReferences) && debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 5 ruled out %d:%s (1m)",where,wcp,objectString(wcpo,tmpstr,false).c_str());
	}
	for (int wcp=m[where].nextCompoundPartObject; wcp>=0; wcp=m[wcp].nextCompoundPartObject,compoundLoop++)
	{
		if (compoundLoop>10)
			break;
		if (disallowReference(wcpo=m[wcp].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 5 ruled out %d:%s (2)",where,wcp,objectString(wcpo,tmpstr,false).c_str());
	}
	if (lastRelativePhrase>lastBeginS1 && lastRelativePhrase>=lastQ2)
	{
		if ((element=m[lastRelativePhrase].pma.queryPattern(L"_REL1",end))==-1)
			return -1;
		endS1=lastRelativePhrase+m[lastRelativePhrase].pma[element&~matchElement::patternFlag].len;
		begin=lastRelativePhrase;
	}
	if (lastBeginS1>=lastRelativePhrase && lastBeginS1>=lastQ2)
	{
		if (lastBeginS1<0) return 0;
		if ((element=m[lastBeginS1].pma.queryPattern(L"__S1",end))==-1)
			return -1;
		endS1=lastBeginS1+m[lastBeginS1].pma[element&~matchElement::patternFlag].len;
		begin=lastBeginS1;
	}
	if (lastQ2>lastRelativePhrase && lastQ2>lastBeginS1)
	{
		if (lastQ2<0) return 0;
		if ((element=m[lastQ2].pma.queryPattern(L"_Q2",end))==-1)
			return -1;
		endS1=lastQ2+m[lastQ2].pma[element&~matchElement::patternFlag].len;
		begin=lastQ2;
	}
	int whereSubject=-1,directObjectPosition=-1,indirectObjectPosition=-1;

	mixedPlurality=false;
	if (endS1<where)
	{
		// where is past the end of the last S1, but not past an EOS (because lastBeginS1 is reset on EOS)
		// if no conjunction after sentence end, return.
		if (m[endS1].queryWinnerForm(conjunctionForm)<0 && m[endS1].queryWinnerForm(coordinatorForm)<0 && m[endS1].word->first!=L"and") return 0;
		int searchPosition=endS1;
		// get the pattern that covers the most of the rest of the sentence.
		element=m[searchPosition].pma.findMaxLen(); // at conjunction
		int element2=m[searchPosition+1].pma.findMaxLen(); // one afterward
		if (element2>=0 && (element<0 || m[searchPosition].pma[element&~matchElement::patternFlag].len<m[searchPosition+1].pma[element2].len))
		{
			searchPosition++;
			element=element2;
		}
		// if the where still has not been reached, return.
		if (element<0 || searchPosition+m[searchPosition].pma[element&~matchElement::patternFlag].len<where) return 0;
	}	
	int whereVerb=m[where].getRelVerb();
	if (whereVerb<0)
	{
		for (whereVerb=where; whereVerb>=begin && !m[whereVerb].hasVerbRelations; whereVerb--);
		if (whereVerb>=0 && !m[whereVerb].hasVerbRelations)
			for (whereVerb=where+1; whereVerb<endS1 && !m[whereVerb].hasVerbRelations; whereVerb++);
	}
	if (whereVerb>=0 && m[whereVerb].relSubject<0 && m[whereVerb+1].hasVerbRelations && m[whereVerb+1].relSubject>=0) 
		whereVerb++;
	if (whereVerb>=0 && (whereSubject=m[whereVerb].relSubject)>=0)
			subjectObject=m[whereSubject].getObject();
		else
			subjectObject=-1;
	if (whereVerb>=0 && (directObjectPosition=m[whereVerb].getRelObject())>=0)
		{
			directObject=m[directObjectPosition].getObject();
			if ((indirectObjectPosition=m[m[whereVerb].getRelObject()].relNextObject)>=0)
				indirectObject=m[indirectObjectPosition].getObject();
			else
				indirectObject=-1;
		}
		else
			directObject=indirectObjectPosition=indirectObject=-1;
  if (subjectObject>=0)
  {
		bool isIdentityRelation=(m[whereSubject].objectRole&IS_OBJECT_ROLE)!=0;
    if (rObject==subjectObject)
    {
			checkForPreviousPP(where,disallowedReferences);
			if (!isIdentityRelation)
			{
				// where is subject
				// Rule 2: add object1 and object2 to disallowedReferences
				if (disallowReference(directObject,disallowedReferences) && debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 2 ruled out directObject %s",
									where,objectString(directObject,tmpstr,false).c_str());
				if (disallowReference(indirectObject,disallowedReferences) && debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 2 ruled out indirectObject %s at %d",
									where,objectString(indirectObject,tmpstr,false).c_str(),indirectObjectPosition);
			}
      // Rule 3: if there is no directObject, and a PREP is in the sentence not in SUBJECT,
      //         then add the PREPOBJECT to disallowedReferences
      // She sat near her.  NOT She sat him near her.
			if (directObjectPosition<0 && whereVerb>=0 && m[whereVerb].relPrep>=0)
			{
				// get the first prep immediately after the verb
				int relPrep=m[whereVerb].relPrep,prepLoop=0;
				while (whereVerb>=relPrep && relPrep!=-1) 
				{
					relPrep=m[relPrep].relPrep;
					if (prepLoop++>20)
					{
						lplog(LOG_ERROR,L"%06d:Prep loop occurred (8) %s.",relPrep,loopString(relPrep,tmpstr));
						break;
					}
				}
				while (whereVerb<relPrep && relPrep!=-1)
				{
					int object,I=m[relPrep].getRelObject();
					if (I>=0 && (m[I].objectRole&PREP_OBJECT_ROLE) && disallowReference(object=m[I].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 3 ruled out prepObject %s at %d(%d)",
									where,objectString(object,tmpstr,false).c_str(),I,endS1);
					relPrep=m[relPrep].relPrep;
					if (prepLoop++>20)
					{
						lplog(LOG_ERROR,L"%06d:Prep loop occurred (9) %s.",relPrep,loopString(relPrep,tmpstr));
						break;
					}
				}
			}
    }
		// this is the direct object, or there is no direct object and this is an object of the first preposition.
		else if ((!isIdentityRelation && whereVerb>=0 && (rObject==directObject || (m[where].objectRole&(OBJECT_ROLE|PREP_OBJECT_ROLE))==OBJECT_ROLE)) ||
			       ((directObjectPosition<0 || where<directObjectPosition) && ((m[where].objectRole&PREP_OBJECT_ROLE) && !(m[where].objectRole&SUBJECT_ROLE))))
    {
      // rObject is object or a prep object with no intervening object
      // Rule 2: add subject to disallowedReferences
      // Rule 3: if there is no directObject, and a PREP is in the sentence not in SUBJECT,
      //         then add the PREPOBJECT to disallowedReferences
			// this has an exception: there must not be any more than one PREP after the verb, otherwise the prepositional phrase may
			//   modify the object of the previous prepositional phrase and so this should NOT be excluded:
			//   he had not yet acquired the habit of going about with any considerable sum of money on him. (Agatha Christie)  
			int numPrepositionalEmbeddings=0;
			if (m[where].objectRole&PREP_OBJECT_ROLE)
			{
				// count how many embeddings there are potentially between verb and object.  If there is more than one preposition, return.
				int relPrep=m[whereVerb].relPrep,prepLoop=0;
				while (whereVerb>=relPrep && relPrep!=-1) 
				{
					relPrep=m[relPrep].relPrep;
					if (prepLoop++>20)
					{
						lplog(LOG_ERROR,L"%06d:Prep loop occurred (10) %s.",relPrep,loopString(relPrep,tmpstr));
						break;
					}
				}
				while (relPrep>whereVerb && relPrep<where)
				{
					numPrepositionalEmbeddings++;
					relPrep=m[relPrep].relPrep;
					if (prepLoop++>20)
					{
						lplog(LOG_ERROR,L"%06d:Prep loop occurred (11) %s.",relPrep,loopString(relPrep,tmpstr));
						break;
					}
				}
			}
			// if rObject==directObject override check if object is subject, because we are certain that subject and object are linked.
			// another knock sent him[knock] scuttling back to cover .  / him is both subject and object, yet 'another knock' should be eliminated
			if (numPrepositionalEmbeddings<=1 && 
				  (rObject==directObject || m[where].relSubject==whereSubject || 
					 (!(m[where].objectRole&SUBJECT_ROLE) && (directObject<0 || !(m[directObjectPosition].objectRole&SUBJECT_ROLE)))))
			{
				if (disallowReference(subjectObject,disallowedReferences) && debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 2 or 3 ruled out subjectObject %s",where,objectString(subjectObject,tmpstr,false).c_str());
				// handle compound subjects
				compoundLoop=0;
				for (int wcp=m[where].previousCompoundPartObject; wcp>=0 && compoundLoop<10; wcp=m[wcp].previousCompoundPartObject,compoundLoop++)
					if (wcp!=whereSubject &&
							disallowReference(m[wcp].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 2 or 3 (compoundSubject) ruled out subjectObject %s",where,objectString(m[wcp].getObject(),tmpstr,false).c_str());
				for (int wcp=m[where].nextCompoundPartObject; wcp>=0 && compoundLoop<10; wcp=m[wcp].nextCompoundPartObject,compoundLoop++)
					if (wcp!=whereSubject &&
							disallowReference(m[wcp].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 2 or 3 (compoundSubject) ruled out subjectObject %s",where,objectString(m[wcp].getObject(),tmpstr,false).c_str());
				// more multiple subjects - MNOUN
				for (unsigned int I=whereSubject+1; I<(signed)m.size() && (m[I].objectRole&MNOUN_ROLE)!=0; I++)
					if (disallowReference(m[I].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 2 or 3 (multipleSubject MNOUN) ruled out subjectObject %s",where,objectString(m[I].getObject(),tmpstr,false).c_str());
				// if personalPronoun, this must not be of form she..her or he..him, where subject has been matched with more than one object.
				if ((m[whereSubject].queryForm(personalPronounForm)>=0 && m[where].queryForm(personalPronounAccusativeForm)<0) ||// must tightly constrict this, because it must not be an indefinite pronoun
					  (m[whereSubject].objectMatches.size()==1 &&
						 (!objects[subjectObject].plural || objects[subjectObject].objectClass==BODY_OBJECT_CLASS)))
					for (vector<cOM>::iterator mo=m[whereSubject].objectMatches.begin(),moEnd=m[whereSubject].objectMatches.end(); mo!=moEnd; mo++)
						if (disallowReference(mo->object,disallowedReferences) && debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 2 or 3 ruled out subjectObject matchingObject %s",
										where,objectString(*mo,tmpstr,false).c_str());
				// if subject is not definitely determined (more than one match)
			  // if subject is not plural
				// if object is not itself a subject (usually a subclause or a question that has been multiple parses)
				if (m[whereSubject].objectMatches.size()>1 && 
					  (!objects[subjectObject].plural || objects[subjectObject].objectClass==BODY_OBJECT_CLASS))
					subjectCataRestriction=whereSubject;
			}
    }
  }
  // rule 4 - collect all nouns that are under the same _NOUN pattern
  // if BODY_OBJECT_CLASS, then ignore
  if (objects[rObject].objectClass==BODY_OBJECT_CLASS)
    return 0;
	int maxEnd,nameEnd=-1,idExemption=-1;
	if ((element=queryPattern(where,L"_META_NAME_EQUIVALENCE",maxEnd))!=-1)
	{
		int whereMNE=where+pema[element].begin;
		if ((element=m[whereMNE].pma.queryPattern(L"_META_NAME_EQUIVALENCE",nameEnd))!=-1) 
		{
			vector < vector <tTagLocation> > tagSets;
			if (startCollectTags(true,metaNameEquivalenceTagSet,whereMNE,m[whereMNE].pma[element&~matchElement::patternFlag].pemaByPatternEnd,tagSets,true,true,L"name equivalence coref filter")>0)
				for (unsigned int J=0; J<tagSets.size(); J++)
				{
					if (debugTrace.traceNameResolution)
						printTagSet(LOG_RESOLUTION,L"MNE",J,tagSets[J],whereMNE,m[whereMNE].pma[element&~matchElement::patternFlag].pemaByPatternEnd);
					int primaryTag=findOneTag(tagSets[J],L"NAME_PRIMARY",-1),secondaryTag=findOneTag(tagSets[J],L"NAME_SECONDARY",-1);
					if (primaryTag<0 || secondaryTag<0) return false;
					int wherePrimary=tagSets[J][primaryTag].sourcePosition,whereSecondary=tagSets[J][secondaryTag].sourcePosition;
					if (tagSets[J][primaryTag].len>1 && m[wherePrimary].principalWherePosition>=0) // could be an adjective
						wherePrimary=m[wherePrimary].principalWherePosition;
					if (tagSets[J][secondaryTag].len>1 && m[whereSecondary].principalWherePosition>=0)
						whereSecondary=m[whereSecondary].principalWherePosition;
					if (m[wherePrimary].getObject()<0 || m[whereSecondary].getObject()<0 || objects[m[wherePrimary].getObject()].plural==objects[m[whereSecondary].getObject()].plural)
					{
						if (wherePrimary==where) idExemption=whereSecondary;
						if (whereSecondary==where) idExemption=wherePrimary;
					}
				}
		}
	}
  int IP,NEnd=objects[rObject].end,o;
  for (IP=objects[rObject].begin; IP<NEnd; IP++)
    if (m[IP].getObject()!=rObject && IP!=idExemption && disallowReference(m[IP].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
        lplog(LOG_RESOLUTION,L"%06d:Disallowed coreferences from begin S1 where %d Lappin and Leass 2.1.1 RULE 4: %s",
			        where,lastBeginS1,objectString(m[IP].getObject(),tmpstr,false).c_str());
	bool singularPronounEncountered=false,pluralPronounEncountered=false;
	// He looked at them.
	// NOT: He looked for all that was good in the world.
	for (IP=begin; IP<endS1; IP++)
		if ((o=m[IP].getObject())>=0 && objects[o].objectClass==PRONOUN_OBJECT_CLASS && 
			  (m[IP].queryForm(personalPronounForm)>=0 || m[IP].queryForm(personalPronounAccusativeForm)>=0))
		{
			singularPronounEncountered|=!objects[o].plural;
			pluralPronounEncountered|=objects[o].plural;
		}
	mixedPlurality=singularPronounEncountered && pluralPronounEncountered;
	int wcpBegin,wcpEnd;
	compoundLoop=0;
	for (wcpBegin=where; m[wcpBegin].previousCompoundPartObject>=0 && compoundLoop<10; wcpBegin=m[wcpBegin].previousCompoundPartObject,compoundLoop++);
	for (wcpEnd=where; m[wcpEnd].nextCompoundPartObject>=0 && compoundLoop<10; wcpEnd=m[wcpEnd].nextCompoundPartObject,compoundLoop++);
	if ((m[wcpBegin].objectRole&PREP_OBJECT_ROLE) && m[wcpBegin].relPrep>=0 && ((m[m[wcpBegin].relPrep-1].objectRole)&(OBJECT_ROLE|SUBJECT_ROLE)))
	{
		wcpBegin=m[wcpBegin].relPrep-1;
		while (wcpBegin>0 && m[wcpBegin].objectRole&(OBJECT_ROLE|SUBJECT_ROLE)) 
		{
			if (!(m[wcpBegin].objectRole&PREP_OBJECT_ROLE) && m[wcpBegin].getObject()>=0)
			{
				wcpBegin=m[wcpBegin].beginObjectPosition-1;
				break;
			}
			if (m[wcpBegin].queryWinnerForm(verbForm)!=-1)
				break;
			wcpBegin--;
		}
		wcpBegin++;
	}
	else
		wcpBegin=m[wcpBegin].beginObjectPosition;
	if (queryPattern(m[wcpEnd].beginObjectPosition,L"__NOUN",maxEnd)!=-1)
		wcpEnd=m[wcpEnd].beginObjectPosition+maxEnd;
	else
		wcpEnd=m[wcpEnd].endObjectPosition;
	for (IP=where; IP>=wcpBegin; IP--)
	{
		if (m[IP].word->first==L",") break;
    if ((o=m[IP].getObject())!=rObject && IP!=idExemption && disallowReference(o,disallowedReferences) && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:%d:Disallowed coreference L&L 2.1.1 RULE 4: %s",where,IP,objectString(o,tmpstr,true).c_str());
	}
	for (IP=where+1; IP<wcpEnd; IP++)
	{
		if (m[IP].word->first==L",") break;
    if ((o=m[IP].getObject())!=rObject && IP!=idExemption && disallowReference(o,disallowedReferences) && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:%d:Disallowed coreference L&L 2.1.1 RULE 4: %s",where,IP,objectString(o,tmpstr,true).c_str());
	}
  return 0;
}

// 5. P is in the NP domain of N (included in the same _NOUN pattern)
//    John's portait of him in interesting.
// for possessives in a prepositional clause 'her hand in his' OR 'a relative of hers'
void cSource::coreferenceFilterLL5(int where,vector <int> &disallowedReferences)
{ LFS
	if ((m[where].objectRole&PREP_OBJECT_ROLE))
	{
		int I=where-1;
		for (; I>=0 && !isEOS(I); I--)
			if (m[I].getObject()>=0 && (m[I].objectRole&PREP_OBJECT_ROLE|OBJECT_ROLE)==OBJECT_ROLE)
				break;
		wstring tmpstr;
		if ((m[I].objectRole&PREP_OBJECT_ROLE|OBJECT_ROLE)==OBJECT_ROLE && disallowReference(m[I].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 RULE 5 ruled out object of possessive %d:%s",
							where,I,objectString(m[I].getObject(),tmpstr,false).c_str());
	}
}

// Lappin and Leass 2.1.1
//  possessive_determiner
//    {"her",SINGULAR_OWNER|FEMALE_GENDER|THIRD_PERSON},{"his",SINGULAR_OWNER|MALE_GENDER|THIRD_PERSON},
//    {"its",SINGULAR_OWNER|NEUTER_GENDER|THIRD_PERSON},{"their",PLURAL_OWNER|THIRD_PERSON}
// 6. P is the determiner of a noun Q and N is contained in Q (P is the determiner of N in a _NOUN pattern).
//    His portrait of John is interesting.
//    His description of the portrait by John is interesting.
// Additional - based on Lappin and Leass 2.1.1 (2)
//             (His eyes were fixed on Mr. Carter)
//             If body object in subject, then this determiner cannot resolve to object, or prep object for the rest of _S1.
int cSource::pronounCoreferenceFilterLL6(int P,int lastBeginS1, vector <int> &disallowedReferences)
{ LFS
	wstring tmpstr;
	if (lastBeginS1<0) 
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING pronounCoreferenceFilterLL6 %s - no lastBeginS1",P,m[P].word->first.c_str());
		return -1;
	}
  wchar_t *allowedPronouns[]={L"her",L"his",L"its",L"their",NULL};
  int I;
  for (I=0; allowedPronouns[I] && m[P].word->first!=allowedPronouns[I]; I++);
  if (!allowedPronouns[I]) 
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING pronounCoreferenceFilterLL6 %s - not in list of resolvable pronouns",P,m[P].word->first.c_str());
		return -1;
	}
  int NEnd=-1,element;
  if ((element=queryPattern(P,L"__NOUN",NEnd))==-1) 
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING pronounCoreferenceFilterLL6 %s - not in NOUN pattern",P,m[P].word->first.c_str());
		return -1;
	}
  // This was restricted to preposition phrases 6/1/2007 because of NOUNs like "F", in which this rule is harmful.
  NEnd+=P;
  // search out all children of NPattern from NBegin to NEnd that are subobjects
  for (int IP=P+pema[element].begin; IP<NEnd; IP++)
    if (IP!=P && m[IP].getObject()>=0 && objects[m[IP].getObject()].end<NEnd && m[IP].objectRole==SUBOBJECT_ROLE)
      disallowedReferences.push_back(m[IP].getObject());

  // get length of S1.
  // Make sure P is a SUBJECT
	int where=m[P].principalWherePosition,PObject=(where>=0) ? m[where].getObject():-1,SLen=-1;
  //if (m[where].principalWherePosition>=0)
  // PObject=m[where].principalWherePosition;
  if (PObject<0) 
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING pronounCoreferenceFilterLL6 %s - object is not referencable",P,m[P].word->first.c_str());
		return -1;
	}
  if ((m[P].objectRole&SUBJECT_ROLE) && !(m[P].objectRole&PREP_OBJECT_ROLE) && objects[PObject].objectClass==BODY_OBJECT_CLASS)
  {
    // collect all objects into disallowedReferences
    for (int IP=P; IP<lastBeginS1+SLen; IP++)
      if (IP!=P && m[IP].getObject()>=0)
        disallowedReferences.push_back(m[IP].getObject());
  }
  if (debugTrace.traceSpeakerResolution && disallowedReferences.size())
  {
    lplog(LOG_RESOLUTION,L"%06d:LL6 Disallowed references:",P);
    for (unsigned int J=0; J<disallowedReferences.size(); J++)
    {
      lplog(LOG_RESOLUTION,L"%06d:%s",J,objectString(disallowedReferences[J],tmpstr,false).c_str());
    }
  }
  return 0;
}

bool cSource::resolvePronoun(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,int beginEntirePosition,
													  bool resolveForSpeaker,bool avoidCurrentSpeaker,bool &mixedPlurality,bool limitTwo,bool isPhysicallyPresent,bool physicallyEvaluated,
														int &subjectCataRestriction,vector <cOM> &objectMatches)
{ LFS
	wstring tmpstr,tmpstr2;
  vector <cObject>::iterator object=objects.begin()+m[where].getObject();
  vector <int> disallowedReferences;
	wstring word=(m[where].principalWherePosition>=0) ? m[m[where].principalWherePosition].word->first : m[where].word->first;
	// you are a very important person
	if (inPrimaryQuote && (m[where].objectRole&IS_OBJECT_ROLE) && m[where].relSubject>=0 && 
		  (m[m[where].relSubject].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON|THIRD_PERSON))==SECOND_PERSON)
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:rejected match (IS) with second person subject.",where);
		return false;
	}
  // handle 'one' of the vacant seats
  if ((word==L"one" || word==L"both" || word==L"most" || word==L"more" || word==L"less" || word==L"any" || word==L"all") && 
		m.size()>(unsigned)where+2 && m[where+1].word->first==L"of" && m[where+2].principalWherePosition>=0)
  {
		int forwardObjectPosition=m[where+2].principalWherePosition;
	  if (m[forwardObjectPosition].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON))
			tryGenderedSubgroup(forwardObjectPosition,objectMatches,objects.begin()+m[forwardObjectPosition].getObject(),-1,true);
		else
		{
			resolveObject(forwardObjectPosition,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,word==L"both");
			narrowGender(forwardObjectPosition,m[where].getObject());
			if (m[forwardObjectPosition].objectMatches.size())
			{
				objectMatches=m[forwardObjectPosition].objectMatches;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:added match (3) %s.",where,objectString(objectMatches,tmpstr,false).c_str());
			}
			else if (m[forwardObjectPosition].getObject()>=0)
			{
				objectMatches.push_back(cOM(m[forwardObjectPosition].getObject(),SALIENCE_THRESHOLD));
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:added match (4) %s.",where,objectString(m[forwardObjectPosition].getObject(),tmpstr,false).c_str());
			}
		}
    return false;
  }
	// you both 
	if (word==L"both" && where>0 && m[where-1].word->first==L"you")
		return false;
	// all others - too ambiguous, introduces irrelevant objects into localObjects
	if (word==L"others" && where>0 && m[where-1].word->first==L"all")
		return false;
	// at least,... at all.
  if ((word==L"least" || word==L"most" || word==L"all") && where>0 && m[where-1].word->first==L"at")
		return false;
  if (word==L"all" && where>0 && m[where-1].word->first==L"about")
		return false;
	// a night's rest
	if (word==L"rest" && m[where].beginObjectPosition!=where && m[where].beginObjectPosition>=0 &&
		  m[m[where].beginObjectPosition].word->first==L"a")
		return false;
	// such a look of ...
	if (word==L"such" && where+1<(signed)m.size() && (m[where+1].word->first==L"a" || m[where+1].word->first==L"an"))
		return false;
	if (limitTwo && tryGenderedSubgroup(where,objectMatches,object,-1,limitTwo))
		return false;
	// many is the time
	if (((!object->male && !object->female) || object->neuter) && 
		  (m[where].objectRole&IS_OBJECT_ROLE) && m[where].getRelObject()>=0 && m[m[where].getRelObject()].getObject()>=0 &&
			// the object cannot be unresolvable - this will not be useful / they were an essentially modern looking couple
			!unResolvablePosition(m[m[where].getRelObject()].beginObjectPosition) &&
			object->matchGenderIncludingNeuter(objects[m[m[where].getRelObject()].getObject()]) &&
			// They[words] were uttered by Boris and they[words] were : “QS Mr . Brown . ”
			(!object->plural || objects[m[m[where].getRelObject()].getObject()].plural))
	{
		// it was opposite the door / He was near Bobby 
		// no preposition, but incorrect parsing means prep was turned into an adverb (opposite, near means the object is NOT the subject)
		if (m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].relPrep<0 && (m[m[m[where].getRelObject()].beginObjectPosition-1].word->second.flags&tFI::prepMoveType))
		{
			int wpo=m[where].getRelObject();
			m[wpo].objectRole&=~IS_OBJECT_ROLE;
			m[wpo].objectRole|=PREP_OBJECT_ROLE;
			setRelPrep(m[where].getRelVerb(),m[wpo].beginObjectPosition-1,1,PREP_VERB_SET, m[where].getRelVerb());
			m[m[wpo].beginObjectPosition-1].setRelObject(wpo);
			m[where].setRelObject(-1);
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:rejected match (IS) %s with %s.",where,objectString(objectMatches,tmpstr,false).c_str(),objectString(object,tmpstr2,false).c_str());
		}
		else
		{
			int forwardObjectPosition=m[where].getRelObject();
			resolveObject(forwardObjectPosition,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,word==L"both");
			narrowGender(forwardObjectPosition,m[where].getObject());
			if (m[forwardObjectPosition].objectMatches.size())
				objectMatches=m[forwardObjectPosition].objectMatches;
			else
				objectMatches.push_back(cOM(m[forwardObjectPosition].getObject(),SALIENCE_THRESHOLD));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:added match (IS) %s with %s.",where,objectString(objectMatches,tmpstr,false).c_str(),objectString(object,tmpstr2,false).c_str());
			return false;
		}
	}
	// pleonastic it
	// It was at that moment that the full realization of his[man] folly began to STAYcome home
	int maxEnd;
	if ((!(object->male || object->female) && object->neuter && !object->plural) && 
		  m[where].relPrep>=0 && m[where].relPrep==m[where].getRelVerb()+1 &&
			m[m[where].relPrep].getRelObject()>=0 && m[m[m[where].relPrep].getRelObject()].endObjectPosition>=0 && queryPattern(m[m[m[where].relPrep].getRelObject()].endObjectPosition,L"_REL1",maxEnd)!=-1 &&
		  (m[where].objectRole&(IS_OBJECT_ROLE|SUBJECT_ROLE))==(IS_OBJECT_ROLE|SUBJECT_ROLE) && 
			 m[where].getRelObject()<0)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:no match for pleonastic it",where);
		m[where].objectRole|=SUBJECT_PLEONASTIC_ROLE;
		return false;
	}
	// it was a few minutes past 11.
	int tf=0,relObjectDEBUG=0;
	if (m[where].getRelObject()>=0 && m[m[where].getRelObject()].beginObjectPosition>=0)
			tf=m[relObjectDEBUG=m[where].getRelObject()].word->second.timeFlags;

	if ((!(object->male || object->female) && object->neuter && !object->plural) && 
		  (m[where].objectRole&(IS_OBJECT_ROLE|SUBJECT_ROLE))==(IS_OBJECT_ROLE|SUBJECT_ROLE) && 
			 m[where].getRelObject()>=0 && m[m[where].getRelObject()].beginObjectPosition>=0 &&
			((m[m[where].getRelObject()].word->second.timeFlags&T_UNIT) || 
			 (m[m[m[where].getRelObject()].beginObjectPosition].pma.queryPattern(L"_DATE")!=-1 || m[m[m[where].getRelObject()].beginObjectPosition].pma.queryPattern(L"_TIME")!=-1)))
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:time match for pleonastic it (2)",where);
		objectMatches.push_back(cOM(m[m[where].getRelObject()].getObject(),SALIENCE_THRESHOLD));
		objects[m[m[where].getRelObject()].getObject()].isTimeObject=true;
		m[where].objectRole|=SUBJECT_PLEONASTIC_ROLE;
		return false;
	}
	// get the nearest localObject that has a subType
	// it was opposite the door
	int wp=m[where].getRelObject();
	if ((m[where].objectRole&IS_OBJECT_ROLE) && object->neuter && !(object->male || object->female || object->plural) && m[where].getRelObject()<0 && m[where].getRelVerb()>=0 &&
		  (wp=m[m[where].getRelVerb()].relPrep)>=0 && (m[wp].word->second.flags&tFI::prepMoveType) && m[wp].getRelObject()>=0 &&
			m[m[wp].getRelObject()].getObject()>=0 && objects[m[m[wp].getRelObject()].getObject()].getSubType()>=0)
	{
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(); lsi!=localObjects.end(); lsi++)
			if (lsi->physicallyPresent && objects[lsi->om.object].getSubType()>=0 && lsi->getTotalAge()<4)
				objectMatches.push_back(cOM(lsi->om.object,SALIENCE_THRESHOLD));
		if (debugTrace.traceSpeakerResolution && objectMatches.size())
			lplog(LOG_RESOLUTION,L"%06d:added place match (IS) %s.",where,objectString(objectMatches,tmpstr,false).c_str());
		if (objectMatches.size()) return false;
	}
	if (((object->male || object->female) && !object->neuter) && 
		  (m[where].objectRole&(IS_OBJECT_ROLE|SUBJECT_ROLE))==(IS_OBJECT_ROLE|SUBJECT_ROLE) && 
			 m[where].getRelObject()>=0 && m[m[where].getRelObject()].getObject()>=0 &&
			// the object cannot be unresolvable - this will not be useful / they were an essentially modern looking couple
			!unResolvablePosition(m[m[where].getRelObject()].beginObjectPosition) &&
			object->matchGenderIncludingNeuter(objects[m[m[where].getRelObject()].getObject()]) &&
			objects[m[m[where].getRelObject()].getObject()].objectClass==NAME_OBJECT_CLASS && 
			objects[m[m[where].getRelObject()].getObject()].numEncounters>1)
	{
		if (m[m[where].getRelObject()].objectMatches.size())
			objectMatches=m[m[where].getRelObject()].objectMatches;
		else
			objectMatches.push_back(cOM(m[m[where].getRelObject()].getObject(),SALIENCE_THRESHOLD));
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:match gendered (IS) %s to %s",where,objectString(object,tmpstr2,false).c_str(),whereString(m[where].getRelObject(),tmpstr,false).c_str());
		return false;
	}
	//if (unResolvablePosition(m[where].beginObjectPosition)) return false;
	// he put her hand in his. (not an adjectival object, yet not applicable to LL2345)
  if ((m[where].flags&WordMatch::flagAdjectivalObject)==0 && m[where].queryWinnerForm(possessivePronounForm)<0)
    coreferenceFilterLL2345(where,m[where].getObject(),disallowedReferences,lastBeginS1,lastRelativePhrase,lastQ2,mixedPlurality,subjectCataRestriction);
	else if (m[where].queryWinnerForm(possessivePronounForm)>=0)
	{
		if ((m[where].flags&WordMatch::flagAdjectivalObject)==0 && lastBeginS1>=0 && !(m[where].objectRole&SUBJECT_ROLE))
		{
			int o=m[where].getObject(),s;
			// search for last subject.  If found, and agent with gender agreement, match and exit.
			for (int subjectWhere=where-1; subjectWhere>=0 && !isEOS(subjectWhere) && m[subjectWhere].word->second.query(quoteForm)<0; subjectWhere--)
				if ((m[subjectWhere].objectRole&SUBJECT_ROLE) && (s=m[subjectWhere].getObject())>=0 && 
					  !(m[subjectWhere].flags&WordMatch::flagAdjectivalObject) && objects[s].objectClass!=BODY_OBJECT_CLASS)
				{
					if (m[subjectWhere].objectMatches.size()<=1 && (s=(m[subjectWhere].objectMatches.size()==1) ? m[subjectWhere].objectMatches[0].object : m[subjectWhere].getObject())>=0 &&
							objects[s].matchGender(objects[o]) && objects[s].plural==objects[o].plural)
					{
						lplog(LOG_RESOLUTION,L"%06d:possessive subject=%d:%s match to %d:%s.",
						      where,subjectWhere,objectString(s,tmpstr,true).c_str(),where,objectString(o,tmpstr2,true).c_str());
						objectMatches.push_back(cOM(s,SALIENCE_THRESHOLD));
						return false;
					}
				}
		}
		coreferenceFilterLL5(where,disallowedReferences);
	}
	// if plural pronoun, in subject position, and speakerGroup has more than 1 person, and not OUTSIDE_QUOTE_NONPAST_OBJECT_ROLE
	// this prevents people from being aged out and plural neuter objects taking their place
	bool disallowOnlyNeuterMatches;
	disallowOnlyNeuterMatches=object->plural && 
		   (m[where].objectRole&SUBJECT_ROLE) && ((m[where].objectRole&IN_PRIMARY_QUOTE_ROLE) || !(m[where].objectRole&NONPAST_OBJECT_ROLE)) &&
			 (currentSpeakerGroup>=speakerGroups.size() || speakerGroups[currentSpeakerGroup].speakers.size()>1);
	int lastGenderedAge;
  adjustSaliencesByGenderNumberAndOccurrenceAgeAdjust(where,m[where].getObject(),inPrimaryQuote,inSecondaryQuote,definitelySpeaker|resolveForSpeaker,lastGenderedAge,disallowedReferences,disallowOnlyNeuterMatches,isPhysicallyPresent,physicallyEvaluated);
	// if this position is an aging marker, then any object in lsi would have been aged, but in adjustSaliencesByParallelRoleAndPlurality, we go by age
	// from where-1, so then this would lead to objects that have just appeared (having an age of 0) and objects that have an age of 1 to be both treated
	// as an age of 0
	if (lastGenderedAge && m[beginEntirePosition].flags&WordMatch::flagAge)
	{
		lastGenderedAge--;
	  if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"lastGenderedAge is de-aged by one (%d).",lastGenderedAge);
	}
	// He spoke to them
	// them does not include he... - but not included in the usual L&L restrictions
	// each preposition points to the next preposition by relPrep.
	// each prep points to its object by relObject.
	// each object points to its preposition by relPrep.
	// if a preposition directly follows another object, the object of the preposition points to the previous object by relNextObject.
	vector <cLocalFocus>::iterator lsi;
	int whereSubject=-1;
	if ((m[where].objectRole&PREP_OBJECT_ROLE) && m[where].relPrep>=0 && m[m[where].relPrep].getRelVerb()>=0 && m[m[m[where].relPrep].getRelVerb()].relSubject>=0 && 
		m[whereSubject=m[m[m[where].relPrep].getRelVerb()].relSubject].getObject()>=0 && 
		objects[m[whereSubject].getObject()].objectClass==PRONOUN_OBJECT_CLASS && 
		m[where].getObject()>=0 &&
		(objects[m[whereSubject].getObject()].plural ^ objects[m[where].getObject()].plural) && 
		(objects[m[whereSubject].getObject()].plural || m[whereSubject].objectMatches.size()==1) &&
		m[whereSubject].queryWinnerForm(pronounForm)<0) // all, etc are not descriptive enough - must be 'they' etc
	{
		for (int o=0; o<(signed)m[whereSubject].objectMatches.size(); o++)
			if ((lsi=in(m[whereSubject].objectMatches[o].object))!=localObjects.end())
			{
				lsi->om.salienceFactor-=10000;
				lsi->res+=L"MIXED_PLURALITY[-10000]";
			}
	}
  adjustSaliencesByParallelRoleAndPlurality(where,inPrimaryQuote,resolveForSpeaker,lastGenderedAge);
	{
		unordered_map <int,int>::iterator sqi;
		if ((m[where].objectRole&SUBJECT_ROLE) && !object->plural && (sqi=questionSubjectAgreementMap.find(where))!=questionSubjectAgreementMap.end() &&
			(m[sqi->second].flags&WordMatch::flagDefiniteResolveSpeakers) && (lsi=in(m[sqi->second].objectMatches[0].object))!=localObjects.end())
		{
			wstring tmpstr3;
	    if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:subject %s should not match question speaker %d:%s - %s",where,objectString(object,tmpstr,true).c_str(),sqi->second,objectString(lsi->om,tmpstr2,true).c_str(),speakerResolutionFlagsString(m[sqi->second].flags,tmpstr3).c_str());
      lsi->om.salienceFactor-=DISALLOW_SALIENCE;
      itos(L"-SPEAKER_OF_PREVIOUS_QUESTION[-",DISALLOW_SALIENCE,lsi->res,L"]");
		}
	}
	if ((m[where].beginObjectPosition<0 || m[m[where].beginObjectPosition].word->first!=L"the") &&
		  m[where].queryForm(indefinitePronounForm)>=0 && currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].povSpeakers.size() && !inPrimaryQuote)
	{
		for (set <int>::iterator oi=speakerGroups[currentSpeakerGroup].povSpeakers.begin(),oiEnd=speakerGroups[currentSpeakerGroup].povSpeakers.end(); oi!=oiEnd; oi++)
			if ((lsi=in(*oi))!=localObjects.end())
			{
				wstring tmpstr3;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:POV %s should not match",where,objectString(*oi,tmpstr,true).c_str());
				lsi->om.salienceFactor-=DISALLOW_SALIENCE;
				itos(L"-POVIND[-",DISALLOW_SALIENCE,lsi->res,L"]");
			}
	}
	excludeObservers(where,inPrimaryQuote,definitelySpeaker);
  resolveNameGender(where,object->male,object->female);
	excludeSpeakers(where,inPrimaryQuote,inSecondaryQuote);
  if (debugTrace.traceSpeakerResolution)
  {
    lplog(LOG_RESOLUTION,L"%06d:RESOLVING pronoun %s%s [%s minAge %d]",where,objectString(object,tmpstr,false).c_str(),m[where].roleString(tmpstr2).c_str(),(resolveForSpeaker) ? L"speaker":L"object",lastGenderedAge);
    printLocalFocusedObjects(where,PRONOUN_OBJECT_CLASS);
  }
	return true;
}

void cSource::setResolved(int where,vector <cLocalFocus>::iterator lsi,bool isPhysicallyPresent)
{ LFS
	if (m[where].getObject()!=lsi->om.object || objects[lsi->om.object].firstLocation!=where) // if this is not the first time the object is encountered
		lsi->resetAge(objectToBeMatchedInQuote);
	lsi->lastRoleSalience=getRoleSalience(m[where].objectRole);
	wstring tmpstr;
	if (!lsi->physicallyPresent && isPhysicallyPresent)
	{
		if (!lsi->physicallyPresent && isPhysicallyPresent && 
			  ((m[where].objectRole&SECONDARY_SPEAKER_ROLE) || 
				 ((m[where].objectRole&IN_SECONDARY_QUOTE_ROLE) && m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].getMainEntry()->first==L"am") ||
				 ((m[where].objectRole&IN_SECONDARY_QUOTE_ROLE) && m[where].getObject()>=0 && objects[m[where].getObject()].whereRelativeClause>=0 && 
				  m[objects[m[where].getObject()].whereRelativeClause].getRelVerb()>=0 && m[m[objects[m[where].getObject()].whereRelativeClause].getRelVerb()].getMainEntry()->first==L"am")))
			isPhysicallyPresent=false;
		else
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s became physically present (1)",where,objectString(lsi->om,tmpstr,true).c_str());
			if (recentExit(lsi))
				lplog(LOG_RESOLUTION,L"%06d:%s physically present after exit @%d (1)",where,objectString(lsi->om,tmpstr,true).c_str(),lsi->lastExit);
			lsi->whereBecamePhysicallyPresent=where;
		}
	}
	lsi->physicallyPresent=lsi->physicallyPresent || isPhysicallyPresent;
	lsi->previousWhere=lsi->lastWhere;
	lsi->lastWhere=where;
	if (lsi->om.object<=1 && objects[lsi->om.object].originalLocation==0)
		objects[lsi->om.object].originalLocation=where;
}

// subject has not been resolved, but the object has been.
// Tom was Bob.
// The man was Bob.
// The plumber was Bob.
// The Russian was Bob.
// The visitor was Bob.

// Tom was the man.
// The man was the man.
// The plumber was the man.
// The Russian was the man.
// The visitor was the man who left them.

// Tom was the plumber.
// The man was the plumber.
// The plumber was the plumber.
// The Russian was the plumber.
// The visitor was the plumber.

// Tom was the Russian.
// The man was the Russian.
// The plumber was the Russian.
// The Russian was the Russian.
// The visitor was the Russian.

// Tom was the visitor.
// The man was the visitor.
// The plumber was the visitor.
// The Russian was the visitor.
// The visitor was a guest.
// if proper noun, resolves to only one reference.
// matches proper nouns to each other
// if it cannot resolve the object, it passes back m[where].object.
// the object that is resolved is the object set at the position 'where', field 'object' in WordMatch.
// in secondary quotes, inPrimaryQuote=false
void cSource::resolveObject(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,
													 bool resolveForSpeaker,bool avoidCurrentSpeaker,bool limitTwo)
{ LFS
  if (m[where].getObject()== cObject::eOBJECTS::UNKNOWN_OBJECT)
		return;
	int beginEntirePosition=m[where].beginObjectPosition; // if this is an adjectival object 
	if (m[where].flags&WordMatch::flagAdjectivalObject) 
		for (; beginEntirePosition>=0 && m[beginEntirePosition].principalWherePosition<0; beginEntirePosition--);
	bool physicallyEvaluated;
	bool isPhysicallyPresent=physicallyPresentPosition(where,beginEntirePosition,physicallyEvaluated,false);
	if (!physicallyEvaluated && (m[where].previousCompoundPartObject>=0 || m[where].nextCompoundPartObject>=0))
		isPhysicallyPresent=evaluateCompoundObjectAsGroup(where,physicallyEvaluated);
	if (resolveForSpeaker) isPhysicallyPresent=physicallyEvaluated=true;
	if (m[where].flags&WordMatch::flagObjectResolved)
	{
		vector <cLocalFocus>::iterator lsi;
		for (vector <cOM>::iterator om=m[where].objectMatches.begin(),omEnd=m[where].objectMatches.end(); om!=omEnd; om++)
		  if ((lsi=in(om->object))!=localObjects.end()) // expected to already be on stack
				setResolved(where,lsi,isPhysicallyPresent);
		// don't mark an object which is matched to some other object (like a meta object) as physically present
		if (resolveForSpeaker && m[where].getObject()>=0 && objects[m[where].getObject()].objectClass!=NAME_OBJECT_CLASS)
			isPhysicallyPresent=false;
	  if ((lsi=in(m[where].getObject()))!=localObjects.end()) // expected to already be on stack
			setResolved(where,lsi,isPhysicallyPresent);
		return;
	}
  if (m[where].flags&WordMatch::flagObjectPleonastic)
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:object is pleonastic.",where);
    return;
	}
	// What women have worn Chanel clothing to award ceremonies ?
	if (m[beginEntirePosition].queryForm(relativizerForm)>=0 ||
		  (beginEntirePosition>0 && m[beginEntirePosition-1].queryForm(relativizerForm)>=0 && m[beginEntirePosition-1].pma.queryQuestionFlagPattern()!=-1))
	{
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:object begins with a relativizer.",where);
    return;
	}
  wstring tmpstr,tmpstr2;
  m[where].flags|=WordMatch::flagObjectResolved;
  if (m[where].flags&WordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup)
  {
		vector <cLocalFocus>::iterator lsi;
		if (m[where].getObject()>=0 && m[where].objectMatches.empty())
      pushObjectIntoLocalFocus(where,m[where].getObject(),true,false,inPrimaryQuote,inSecondaryQuote,L"keep original resolution through speaker group", lsi);
    eliminateBodyObjectRedundancy(where,m[where].objectMatches);
    for (vector <cOM>::iterator om=m[where].objectMatches.begin(),omEnd=m[where].objectMatches.end(); om!=omEnd; om++)
      pushObjectIntoLocalFocus(where,om->object,true,false,inPrimaryQuote,inSecondaryQuote,L"keep original resolution through speaker group (2)", lsi);
		if (m[where].objectMatches.size())
			moveNyms(where,m[where].objectMatches[0].object,m[where].getObject(),L"UnresolvableObjectResolvedThroughSpeakerGroup");
    return;
  }
  if (m[where].getObject()< cObject::eOBJECTS::UNKNOWN_OBJECT)
  {
    cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || inPrimaryQuote,m[where].getObject()== cObject::eOBJECTS::OBJECT_UNKNOWN_NEUTER,objectToBeMatchedInQuote,quoteIndependentAge); // only neuter
		resolveAdjectivalObject(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,resolveForSpeaker);
    return;
  }
	if ((m[where].flags&WordMatch::flagAdjectivalObject) && 
	    m[where].endObjectPosition-m[where].beginObjectPosition==1 && 
		  !(m[where].word->second.inflectionFlags&(PLURAL_OWNER|SINGULAR_OWNER)) && !(m[where].flags&WordMatch::flagNounOwner))
	{
		// the American girl  - "American" is not used as an object and should not be entered as one to go into the locally focused stack.
		//objects[m[where].getObject()].neuter=true;
		//objects[m[where].getObject()].male=objects[m[where].getObject()].female=false;
		//if (t.traceSpeakerResolution)
		//	lplog(LOG_RESOLUTION,L"%06d:Set gender of object %s used as an adjective but non-owner to neuter.",where,objectString(m[where].getObject(),tmpstr,true).c_str());
		return;
	}
  vector <cObject>::iterator object=objects.begin()+m[where].getObject();
	if (m[where].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE)
		m[where].objectRole&=~UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
	// if inQuotes, and relVerb is a present tense, and speakerGroupsEstablished, and immediately before lastOpeningPrimaryQuote is an unquoted paragraph,
	//   AND there is a local object that is newly physically present in that unquoted paragraph, then allow out of quote matches
	bool presentAssertion=false;
	if (inPrimaryQuote && speakerGroupsEstablished && m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].verbSense==VT_PRESENT && lastOpeningPrimaryQuote>3 &&
		  !(m[lastOpeningPrimaryQuote-3].objectRole&IN_PRIMARY_QUOTE_ROLE) && m[lastOpeningPrimaryQuote].previousQuote>=0) // -1 is section break.  -2 is the end quote.  -3 is in quote (or not)
	{
		int begin=m[m[lastOpeningPrimaryQuote].previousQuote].endQuote,end=lastOpeningPrimaryQuote-2;
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end(); lsi!=lsEnd && !presentAssertion; lsi++)
		{
			presentAssertion=(lsi->whereBecamePhysicallyPresent>begin && lsi->whereBecamePhysicallyPresent<end);
		}
	}
	// if inQuote but hail position, resolve as if out of quotes
  cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || (inPrimaryQuote && !(m[where].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE))),presentAssertion || (object->neuter && !(object->male || object->female)),objectToBeMatchedInQuote,quoteIndependentAge);
  if (object->eliminated)
    lplog(LOG_RESOLUTION,L"%06d:eliminated object #%d %s found!",where,m[where].getObject(),objectString(object,tmpstr,false).c_str());
  int person=m[where].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON|THIRD_PERSON);
  int begin=m[where].beginObjectPosition;
	int wordOrderSensitiveModifier=object->wordOrderSensitive(where,m);
	int subjectCataRestriction=-1;
  bool chooseFromLocalFocus=false,mixedPlurality=false;
	if (object->relativeClausePM>=0) 
	{
		m[object->whereRelativeClause].flags|=WordMatch::flagObjectResolved;
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:%s@%d matched a relative head",where,objectString(object,tmpstr,true).c_str(),object->whereRelativeClause);
	}
	// Carter nodded thoughtfully .
  bool hasDeterminer=
    m[beginEntirePosition].word->first==L"a" || m[beginEntirePosition].word->first==L"the" ||
    m[beginEntirePosition].queryForm(demonstrativeDeterminerForm)>=0 ||
		m[beginEntirePosition].queryForm(possessiveDeterminerForm)>=0 ||
    m[beginEntirePosition].queryForm(quantifierForm)>=0 ||
    m[beginEntirePosition].queryForm(numeralCardinalForm)>=0;
	if (object->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && !inPrimaryQuote && !inSecondaryQuote && !hasDeterminer && (m[where].flags&(WordMatch::flagFirstLetterCapitalized|WordMatch::flagAllCaps)))
	{
		object->objectClass=NAME_OBJECT_CLASS;
		object->name.any=m[where].word;
	}
  vector <cOM> objectMatches;
	switch (object->objectClass)
  {
  case PRONOUN_OBJECT_CLASS:
		{
			wstring word=(m[where].principalWherePosition>=0) ? m[m[where].principalWherePosition].word->first : m[where].word->first;
			// 'there' and 'so' should have special handlers which have not been written yet
			// also pronouns that never resolve to anything
			// somebody or anybody may be resolved, but more often they lead to objects having an incorrect salience
			wchar_t *doNotResolve[]={L"no one",L"none",L"nothing",L"nobody",L"someone",L"somebody",L"anyone",L"anybody",L"there",L"so",L"more",L"less",L"this",L"that",NULL};
			for (unsigned int dNR=0; doNotResolve[dNR]; dNR++)
				if (word==doNotResolve[dNR])
					return;
			if ((m[where].flags&WordMatch::flagAdjectivalObject) && 
					(word==L"both" || word==L"either" || word==L"neither" || word==L"any" || word==L"all" || word==L"each" || word==L"some"))
				return;
			if (word==L"one" && m[where].endObjectPosition-begin>1 && m[begin].word->first==L"some")
				return;
		}
		// FIRST_PERSON:  "my", "our", "i", "we", "me", "us", "mine", "ours" -- restrict to speaker or narrator
		// SECOND_PERSON: "our", "your", "thy", "we", "you", "thee", "thou", "us", "ours","yours","thine" - restrict to all speakers
		if (person&(FIRST_PERSON|SECOND_PERSON))
		{
			if (!inPrimaryQuote && !inSecondaryQuote)
			{
				vector <cLocalFocus>::iterator lsi;
				if (person&FIRST_PERSON)
				{
					if (pushObjectIntoLocalFocus(where, 0, true, false, inPrimaryQuote, inSecondaryQuote, L"chooseBest (FS)", lsi))
					{
						lsi->om.salienceFactor = 0;
						pushLocalObjectOntoMatches(where, lsi, L"chooseBest (FPS)");
						if (!(person&SECOND_PERSON))
							return;
					}
				}
				if ((person&SECOND_PERSON) && !(person&FIRST_PERSON))
				{
					if (pushObjectIntoLocalFocus(where, 1, true, false, inPrimaryQuote, inSecondaryQuote, L"chooseBest (FS)", lsi))
					{
						lsi->om.salienceFactor = 0;
						pushLocalObjectOntoMatches(where, lsi, L"chooseBest (SPS)");
						return;
					}
				}
				// at this point has both first and second person
				if ((lsi=in(1))!=localObjects.end())
					pushLocalObjectOntoMatches(where,lsi,L"chooseBest (SPS)");
			}
			m[where].flags&=~WordMatch::flagObjectResolved;
			return; // Cannot resolve these pronouns until speakers are resolved
		}
		// one person who knows who he is
		if (object->whereRelativeClause<0)
			chooseFromLocalFocus=resolvePronoun(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,beginEntirePosition,resolveForSpeaker,avoidCurrentSpeaker,mixedPlurality,limitTwo,isPhysicallyPresent,physicallyEvaluated,subjectCataRestriction,objectMatches);
		break;
  case REFLEXIVE_PRONOUN_OBJECT_CLASS:
    // FIRST_PERSON:  "myself", "ourselves",
    // SECOND_PERSON: "ourselves","yourself","yourselves",
    if ((person&FIRST_PERSON) || (person&SECOND_PERSON))
		{
			if (!inPrimaryQuote && !inSecondaryQuote)
			{
				vector <cLocalFocus>::iterator lsi;
				if (person&FIRST_PERSON)
				{
					if (pushObjectIntoLocalFocus(where, 0, true, false, inPrimaryQuote, inSecondaryQuote, L"chooseBest (FS)", lsi))
					{
						lsi->om.salienceFactor = 0;
						pushLocalObjectOntoMatches(where, lsi, L"chooseBest (FPS)");
						if (!(person&SECOND_PERSON))
							return;
					}
				}
				if ((person&SECOND_PERSON) && !(person&FIRST_PERSON))
				{
					if (pushObjectIntoLocalFocus(where, 1, true, false, inPrimaryQuote, inSecondaryQuote, L"chooseBest (FS)", lsi))
					{
						lsi->om.salienceFactor = 0;
						pushLocalObjectOntoMatches(where, lsi, L"chooseBest (SPS)");
						return;
					}
				}
				// at this point has both first and second person
				if ((lsi=in(1))!=localObjects.end())
					pushLocalObjectOntoMatches(where,lsi,L"chooseBest (SPS)");
			}
			m[where].flags&=~WordMatch::flagObjectResolved;
      return; // Cannot resolve these pronouns until speakers are resolved
		}
      // THIRD_PERSON:  "himself","herself","itself","themselves",
    if ((chooseFromLocalFocus=reflexivePronounCoreference(where,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,inPrimaryQuote,inSecondaryQuote)>=0) && debugTrace.traceSpeakerResolution)
		{
			lplog(LOG_RESOLUTION,L"%06d:RESOLVING reflexive %s",where,objectString(object,tmpstr,false).c_str());
			printLocalFocusedObjects(where,GENDERED_GENERAL_OBJECT_CLASS);
		}
		if (!chooseFromLocalFocus) return;
    break;
  case NAME_OBJECT_CLASS:
	  if (debugTrace.traceSpeakerResolution)
		  lplog(LOG_RESOLUTION,L"%06d:[BEGIN] RESOLVING name %s%s",where,objectString(object,tmpstr,false).c_str(),m[where].roleString(tmpstr2).c_str());
		{
			checkSubsequent(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,objectMatches);
			// if returns true, then successfully resolved, with or without objectMatches.
			bool resolveSuccess=resolveNameObject(where,object,objectMatches,-1);
			object=objects.begin()+m[where].getObject();
			if (resolveSuccess) break;
		}
    // flow on to cataphora resolution
  case GENDERED_GENERAL_OBJECT_CLASS:
    if ((m[where].flags&WordMatch::flagAdjectivalObject)==0)
    {
      if (unResolvablePosition(begin)) 
			{
				checkSubsequent(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,objectMatches);
				break;
			}
			// only cataphorically match if not an adjectival object and not discovering speaker groups
			// because if this is a hail object in a quote, then objects before it in a quote would never have been matched
			// (because we are discovering speaker groups) so this will lead to an inappropriate match.
			if (currentSpeakerGroup<speakerGroups.size())
			{
				// match cataphorically (match a pronoun already encountered in text)
				// if there are no matches for an object, it is third person,
				//   for every m > beginSection that has object==object matched, set objectMatched of m to object.
				for (int I=lastBeginS1; I>=0 && I<where; I++)
					if (!m[I].objectMatches.size() && m[I].getObject()>=0 &&
						!(m[I].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON)) && // don't match pronouns to be matched in resolveFirstSecondPersonPronouns
						object->cataphoricMatch(&objects[m[I].getObject()]))
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:added cataphoric match (4) %s.",I,objectString(object,tmpstr,false).c_str());
						m[I].objectMatches.push_back(cOM(m[where].getObject(),getRoleSalience(m[where].objectRole)));
						object->locations.push_back(cObject::cLocation(I));
						object->updateFirstLocation(I);
					}
			}
    }
    else if (unResolvablePosition(beginEntirePosition))
      break;
    chooseFromLocalFocus=resolveGenderedObject(where,definitelySpeaker|resolveForSpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,objectMatches,object,wordOrderSensitiveModifier,subjectCataRestriction,mixedPlurality,limitTwo,isPhysicallyPresent,physicallyEvaluated);
    break;
  case BODY_OBJECT_CLASS:
		{
			vector <int> noRestriction;
			unMatchObjects(where,noRestriction,false);
			bool changeClass=false;
			chooseFromLocalFocus=resolveBodyObjectClass(where,beginEntirePosition,object,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,wordOrderSensitiveModifier,subjectCataRestriction,mixedPlurality,limitTwo,isPhysicallyPresent,physicallyEvaluated,changeClass,objectMatches);
			if (changeClass)
			{
				object->objectClass=NON_GENDERED_GENERAL_OBJECT_CLASS;
				object->neuter=true;
				object->male=object->female=false;
			  m[where].flags&=~WordMatch::flagObjectResolved;
				resolveObject(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,limitTwo);
				return;
			}
			bool allIn,oneIn;
			if (m[where].objectMatches.size() && speakerGroupsEstablished && speakerGroups[currentSpeakerGroup].speakers.find(m[where].getObject())!=speakerGroups[currentSpeakerGroup].speakers.end() &&
				  intersect(where,speakerGroups[currentSpeakerGroup].speakers,allIn,oneIn) && allIn)
				speakerGroups[currentSpeakerGroup].speakers.erase(m[where].getObject());
			break;
		}
  case GENDERED_DEMONYM_OBJECT_CLASS:
		// Plural gendered demonyms (The Irish, The French) are not resolved.
		if (object->plural)
			break;
    if (!unResolvablePosition(beginEntirePosition)) 
		{
			chooseFromLocalFocus=resolveGenderedObject(where,definitelySpeaker|resolveForSpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,objectMatches,object,wordOrderSensitiveModifier,subjectCataRestriction,mixedPlurality,limitTwo,isPhysicallyPresent,physicallyEvaluated);
			addPreviousDemonyms(where);
		}
		else
			checkSubsequent(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,objectMatches);
    break;
  case META_GROUP_OBJECT_CLASS:
	  if ((m[where].objectRole&PREP_OBJECT_ROLE) && beginEntirePosition && m[beginEntirePosition-1].word->first==L"as" && unResolvablePosition(beginEntirePosition))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s not put in local objects - 'as' prep",where,objectString(object,tmpstr,false).c_str());
			// scan backwards for immediately preceding object
			if (beginEntirePosition>5)
			{
				for (int I=beginEntirePosition-1; I>=beginEntirePosition-5; I--)
					if (m[I].getObject()>=0 && m[I].endObjectPosition>=beginEntirePosition-1)
					{
						if (m[I].objectMatches.size())
							objectMatches=m[I].objectMatches;
						else
							objectMatches.push_back(cOM(m[I].getObject(),SALIENCE_THRESHOLD));
						break;
					}
			}
		}
		else
			resolveMetaGroupObject(where,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,definitelySpeaker,resolveForSpeaker,avoidCurrentSpeaker,mixedPlurality,limitTwo,objectMatches,chooseFromLocalFocus);
		break;
  case GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS:
  case GENDERED_RELATIVE_OBJECT_CLASS:
    if (unResolvablePosition(beginEntirePosition))
		{
			checkSubsequent(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,objectMatches);
			eliminateExactMatchingLocalObject(where); // a hospital nurse
      break;
		}
    // reset weights
    for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end(); lsi!=lsEnd; lsi++)
      lsi->clear();
		if (object->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
		{
			// if an occupation is introduced, only match against local objects with attributes, not against sex!
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:RESOLVING occupation %s",where,objectString(object,tmpstr,false).c_str());
			if (!object->plural && !unResolvablePosition(m[where].beginObjectPosition))
			{
				chooseFromLocalFocus=resolveOccRoleActivityObject(where,objectMatches,object,wordOrderSensitiveModifier,isPhysicallyPresent && physicallyEvaluated);
				if (objectMatches.empty() && m[where].word->first==L"speaker" && lastOpeningPrimaryQuote>=0 && m[lastOpeningPrimaryQuote].objectMatches.size())
				{
					int lastSpeaker=m[lastOpeningPrimaryQuote].objectMatches[0].object;
					vector <cLocalFocus>::iterator lsi=in(lastSpeaker);
					if (objects[lastSpeaker].objectClass==BODY_OBJECT_CLASS && objects[lastSpeaker].getOwnerWhere()>=0 && 
						  m[objects[lastSpeaker].getOwnerWhere()].getObject()>=0)
						lsi=in(lastSpeaker=m[objects[lastSpeaker].getOwnerWhere()].getObject());
					if (lsi!=localObjects.end())
					{
						objectMatches.push_back(lsi->om);
						chooseFromLocalFocus=false;
					}
				}
				printLocalFocusedObjects(where,GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS);
			}
		}
		else
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:RESOLVING relative %s",where,objectString(object,tmpstr,false).c_str());
			resolveRelativeObject(where,objectMatches,object,wordOrderSensitiveModifier);
			// must not be matched or specific
			if (objectMatches.empty() && object->getOwnerWhere()==-1 && object->whereRelativeClause<0 && 
				  // daughters of the archdeacon
				  (m[where].endObjectPosition>=(signed)m.size() || m[m[where].endObjectPosition].word->first!=L"of"))
		    chooseFromLocalFocus=resolveGenderedObject(where,definitelySpeaker|resolveForSpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,objectMatches,object,wordOrderSensitiveModifier,subjectCataRestriction,mixedPlurality,limitTwo,isPhysicallyPresent,physicallyEvaluated);
			printLocalFocusedObjects(where,GENDERED_RELATIVE_OBJECT_CLASS);
		}
		if (objectMatches.size()) break;
  case NON_GENDERED_GENERAL_OBJECT_CLASS:
		if (isKindOf(where) && where+2<(int)m.size() && m[where+1].word->first==L"of" && m[where+2].principalWherePosition>=0)
		{
			int principalWhere=m[where+2].principalWherePosition,pobject=m[principalWhere].getObject(),objectClass=(pobject>=0) ? objects[pobject].objectClass : -1;
			if (objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || objectClass==GENDERED_GENERAL_OBJECT_CLASS)
			{
				// change future gendered object so that it incorporates the adjectives of this object
				m[principalWhere].beginObjectPosition=begin;
				m[begin].principalWherePosition=principalWhere;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:PERSX Incorporating KindOf adjectives into object %s",where,objectString(object,tmpstr,false).c_str());
				objects[pobject].isKindOf=true;
			}
			// the bigger of the two was Whittington
			// if a subject, and IS_OBJECT, and the relObject of the subject is valid, add the relObject to not only this object, but also the of-object and its matches
			if ((m[where].objectRole&(SUBJECT_ROLE|IS_OBJECT_ROLE))==(SUBJECT_ROLE|IS_OBJECT_ROLE) && m[where].getRelObject()>=0 && m[m[where].getRelObject()].getObject()>=0)
			{
				int relObject=m[m[where].getRelObject()].getObject();
				objectMatches.push_back(cOM(relObject,SALIENCE_THRESHOLD));
				// resolve 'the two'
				resolveObject(principalWhere,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,resolveForSpeaker,avoidCurrentSpeaker,limitTwo);
				// if the two matched to 'the two men', then match Whittington to the two men
				for (unsigned int J=0; J<m[principalWhere].objectMatches.size(); J++)
				{
					int omWhere=objects[m[principalWhere].objectMatches[J].object].firstLocation;
					if (omWhere>=0 && !in(relObject,omWhere)) // omWhere is negative if the obejct is audience or narrator
					{
						m[omWhere].objectMatches.push_back(cOM(relObject,SALIENCE_THRESHOLD));
						objects[relObject].locations.push_back(omWhere); 
					}
				}
				if (!in(relObject,principalWhere))
				{
					m[principalWhere].objectMatches.push_back(cOM(relObject,SALIENCE_THRESHOLD));
					objects[relObject].locations.push_back(principalWhere);
				}
			}
			else
				return;
		}
  case NON_GENDERED_NAME_OBJECT_CLASS:
  case NON_GENDERED_BUSINESS_OBJECT_CLASS:
    // match 'the old doctor' to 'the doctor' but not to 'the new doctor'
    // right now do not support merging of nouns used as adjectives?
    resolveNonGenderedGeneralObject(where,object,objectMatches,wordOrderSensitiveModifier);
		if (chooseFromLocalFocus && object->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
			for (vector <cLocalFocus>::iterator lsi=localObjects.begin(), lsiEnd=localObjects.end(); lsi!=lsiEnd && !(chooseFromLocalFocus=lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->numMatchedAdjectives>1); lsi++);
  case PLEONASTIC_OBJECT_CLASS:
  case VERB_OBJECT_CLASS:
	case RECIPROCAL_PRONOUN_OBJECT_CLASS:
	default:
    break;
  }
  // reresolve of an object in a quote that cannot match the speaker
  vector <cLocalFocus>::iterator lsi;
  if (avoidCurrentSpeaker)
  {
		// if any speakers are of the highest salience, decrease the salience to the next highest salience (if greater than SALIENCE_THRESHOLD)
		// ths avoids third persons being resolved to the speaker, unless there is no alternative
		vector < vector <cLocalFocus>::iterator > psls;
    for (unsigned int J=0; J<previousSpeakers.size(); J++)
      if ((lsi=in(previousSpeakers[J]))!=localObjects.end() && lsi->om.salienceFactor>=SALIENCE_THRESHOLD)
				psls.push_back(lsi);
		if (psls.size())
    {
			int maxsf=-1;
	    for (unsigned int J=0; J<localObjects.size(); J++)
				if (find(previousSpeakers.begin(),previousSpeakers.end(),localObjects[J].om.object)==previousSpeakers.end())
					maxsf=max(maxsf,localObjects[J].om.salienceFactor);
			if (maxsf>=SALIENCE_THRESHOLD)
			{
				for (unsigned int J=0; J<psls.size(); J++)
				{
					if (psls[J]->om.salienceFactor>maxsf)
		      {
				    if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:avoidCurrentSpeaker: reducing %s from %d to %d.",where,objectString(psls[J]->om,tmpstr,true).c_str(),psls[J]->om.salienceFactor,maxsf);
						psls[J]->om.salienceFactor=maxsf;
					}
				}
			}
    }
  }
	// the guy Tommy wanted to say hello to.
	if (object->whereRelSubjectClause>=0 && m[object->whereRelSubjectClause].getObject()>=0 && m[object->whereRelSubjectClause].principalWherePosition>=0)
	{
		int o=m[m[object->whereRelSubjectClause].principalWherePosition].getObject();
		vector <cOM>::iterator omi;
		  if (chooseFromLocalFocus && (lsi=in(o))!=localObjects.end())
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:avoidFollowingRelativeClauseSubject: avoiding %s.",where,objectString(lsi->om,tmpstr,true).c_str());
				lsi->om.salienceFactor-=DISALLOW_SALIENCE;
				itos(L"-AVOIDRELCLAUSE[-",DISALLOW_SALIENCE,lsi->res,L"]");
			}
			else if (objectMatches.size() && ((omi=in(o,objectMatches))!=objectMatches.end()))
				objectMatches.erase(omi);
	}
	discouragePOV(where,inPrimaryQuote || inSecondaryQuote,definitelySpeaker);
	// prefer parallel object
  if (chooseFromLocalFocus && chooseBest(where,definitelySpeaker,inPrimaryQuote,inSecondaryQuote,resolveForSpeaker,mixedPlurality)) 
	{
		setQuoteContainsSpeaker(where,inPrimaryQuote);
	  if (object->objectClass==NAME_OBJECT_CLASS && debugTrace.traceSpeakerResolution)
		{
			lplog(LOG_RESOLUTION,L"%06d:[END from local focus] RESOLVING name %s",where,objectString(object,tmpstr,false).c_str());
			printLocalFocusedObjects(where,NAME_OBJECT_CLASS);
		}
		processSubjectCataRestriction(where,subjectCataRestriction);
		return;
	}
  // NAME, GENDERED_GENERAL, NON_GENDERED_GENERAL, DET_NAME, PLEONASTIC, DEICTIC, VERB
  // if nongendered object and if there is more than one name, or gendered object already in local speakers,
  // and this is not identified as speaker, don't add to localSpeakers.
  bool notSpeaker=false;
  if ((object->objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS || object->objectClass==NON_GENDERED_BUSINESS_OBJECT_CLASS || 
		   object->objectClass==VERB_OBJECT_CLASS) && !definitelySpeaker)
  {
    int acceptableSpeakers=0;
		vector <cLocalFocus>::iterator lsiEnd;
		for (lsi=localObjects.begin(), lsiEnd=localObjects.end(); lsi!=lsiEnd && acceptableSpeakers<1; lsi++)
      if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) &&
          (lsi->numIdentifiedAsSpeaker>0 ||
          objects[lsi->om.object].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
          objects[lsi->om.object].objectClass==BODY_OBJECT_CLASS ||
          objects[lsi->om.object].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
          objects[lsi->om.object].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
          objects[lsi->om.object].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
          objects[lsi->om.object].objectClass==META_GROUP_OBJECT_CLASS ||
          objects[lsi->om.object].objectClass==NAME_OBJECT_CLASS))
        acceptableSpeakers++;
    if (acceptableSpeakers>=1)
      notSpeaker=true;
  }
	if (object->getSubType()>=0 && !(m[where].objectRole&MOVEMENT_PREP_OBJECT_ROLE) && (object->male ^ object->female))
	{
		object->resetSubType();
		if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
			lplog(LOG_RESOLUTION,L"%06d:Removing place designation (5) from object %s.",where,objectString(object,tmpstr,false).c_str());
	}
	if ((object->getSubType()>=0 && !(m[where].objectRole&NON_MOVEMENT_PREP_OBJECT_ROLE) && object->neuter) || (m[where].objectRole&PLACE_OBJECT_ROLE))
		notSpeaker=true;
	if (object->isTimeObject || object->isLocationObject)
		notSpeaker=true;
  // if this is a name and it is not identified as a speaker and it is adjectival and the object it modifies is NOT
  // a body part, then it is not a speaker.
  if (object->objectClass==NAME_OBJECT_CLASS && !definitelySpeaker && (m[where].flags&WordMatch::flagAdjectivalObject))
  {
    unsigned int I;
    for (I=object->originalLocation; I<m.size(); I++)
      if (m[I].getObject()>=0 && (m[I].flags&WordMatch::flagAdjectivalObject)!=WordMatch::flagAdjectivalObject)
        break;
		bool singularBodyPart;
    if (I<m.size() && !isExternalBodyPart(I,singularBodyPart,false))
      notSpeaker=true;
  }
  // set notSpeaker if an OBJECT with a VERB which is not a simple past tense (VT_PAST, VT_EXTENDED+ VT_PAST, VT_PASSIVE+ VT_PAST, VT_PASSIVE+ VT_PAST+VT_EXTENDED)
  // OR if a id verb (IS and subject is not "it") also mark object with IS_OBJECT_ROLE
  // Sam is a nice man.  (unresolvable attribute - don't enter into local objects)
  // This boat is a great boat.  That man is a wonderful person.
  // Bart was a great napper.
  // NOT: There was a great man.  It was a boat of a kind never seen.
  // If the attribute is an object which is:
  // 1. unresolvable
  // 2. an object (not a subject)
  // 3. in an S1 - must be an identity pattern   idRelationTagSet="id","OBJECT","SUBJECT"
  // 4. the subject is not in class (PRONOUN_OBJECT_CLASS,REFLEXIVE_PRONOUN_OBJECT_CLASS,RECIPROCAL_PRONOUN_OBJECT_CLASS)
	__int64 or=m[where].objectRole;
  notSpeaker|=(or&NOT_OBJECT_ROLE)!=0;
	if ((or&(IN_EMBEDDED_STORY_OBJECT_ROLE|IN_PRIMARY_QUOTE_ROLE))==(IN_EMBEDDED_STORY_OBJECT_ROLE|IN_PRIMARY_QUOTE_ROLE)) // embedded story can only be in quotes
		notSpeaker|=(or&NONPAST_OBJECT_ROLE)!=0;
	else
	{
		notSpeaker|=(or&IN_PRIMARY_QUOTE_ROLE)!=0 && (or&NONPRESENT_OBJECT_ROLE)!=0;
		notSpeaker|=(or&IN_PRIMARY_QUOTE_ROLE)==0 && (or&NONPAST_OBJECT_ROLE)!=0;
	}
	notSpeaker|=((or&(SUBJECT_ROLE|IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE|PREP_OBJECT_ROLE))==IS_OBJECT_ROLE);
  // if the role is a PREPOBJECT (object of a preposition), IOBJECT (object of an infinitive), or RE_OBJECT (restated object)
  notSpeaker|=(or&PREP_OBJECT_ROLE) || (or&IOBJECT_ROLE) || (or&RE_OBJECT_ROLE) || (or&THINK_ENCLOSING_ROLE);
	// if this has been identified as a speaker in the past, don't mark it as not a speaker
  if (object->numIdentifiedAsSpeaker || object->numDefinitelyIdentifiedAsSpeaker ||
		  (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.find(m[where].getObject())!=speakerGroups[currentSpeakerGroup].speakers.end()))
    notSpeaker=false;
	// if this is a HAIL, and never identified, mark as notSpeaker.
  if (in(m[where].getObject())==localObjects.end() && (or&HAIL_ROLE)!=0 && 
		  (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.find(m[where].getObject())==speakerGroups[currentSpeakerGroup].speakers.end()))
    notSpeaker=true;
	lsi=in(m[where].getObject());
	if ((lsi==localObjects.end() || (!lsi->physicallyPresent && !lsi->numIdentifiedAsSpeaker)) && (m[where].endObjectPosition-m[where].beginObjectPosition)==1 &&
			(m[where].word->second.timeFlags&T_UNIT) && (m[where].flags&WordMatch::flagFirstLetterCapitalized))
	{
		notSpeaker=true;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:%s not speaker (time)",where,objectString(object,tmpstr,false).c_str());
	}
	// Jay-Z says he will marry Beyonce Knowles "one day soon." - Jay-Z is the first word of the entire document
	if (object->originalLocation==where && (or&SUBJECT_ROLE) && object->neuter && !object->male && !object->female && m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].queryForm(thinkForm)>=0)
	{
		identifyISARelation(where,false);
		if ((m[where].flags&WordMatch::flagFirstLetterCapitalized) && object->isWikiPerson)
		{
			object->male=object->female=true;
			if (object->objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
				object->objectClass=NAME_OBJECT_CLASS;
			if (object->objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS)
			{
				object->objectClass=NAME_OBJECT_CLASS;
				if (object->len()==1)
					object->name.any=m[where].word;
				else
				{
					object->name.first=m[m[where].beginObjectPosition].word;
					object->name.last=m[m[where].endObjectPosition-1].word;
				}
			}
		}
	}
	if (m[where].getObject()>=0 && lsi==localObjects.end())
	{
		bool noLocalMatch=false;
		// if role cannot be found (so parsing is suspect)
		// don't include if inside of an IS sentence, but make sure names are included: 
		// The words uttered by Boris were: "Mr. Brown".
		if (!(or&(SUBJECT_ROLE|OBJECT_ROLE|SUBOBJECT_ROLE|HAIL_ROLE)) && lastBeginS1>=0 && 
			  (m[lastBeginS1].objectRole&ID_SENTENCE_TYPE) && !(m[lastBeginS1].objectRole&SUBJECT_PLEONASTIC_ROLE))
		{
			// don't make this a speaker if it is new or it was previously not a speaker
			if (objects[m[where].getObject()].objectClass==NAME_OBJECT_CLASS)
				notSpeaker=lsi==localObjects.end() || lsi->notSpeaker;
			else
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:%s not put in local objects - possible IS_OBJECT [lastBeginS1=%d]",where,objectString(object,tmpstr,false).c_str(),lastBeginS1);
				noLocalMatch=true;
			}
		}
	  if ((or&PREP_OBJECT_ROLE) && beginEntirePosition && m[beginEntirePosition-1].word->first==L"as" && unResolvablePosition(beginEntirePosition))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s not put in local objects - 'as' prep",where,objectString(object,tmpstr,false).c_str());
			noLocalMatch=true;
		}
		// if unmatched, don't allow a noun beginning with 'no' to be matched (but not 'No' abbreviation)
		noLocalMatch|=(m[beginEntirePosition].word->first==L"no" && m[beginEntirePosition+1].word->first!=L".");
		// don't allow an appositive noun to be a name (should be a description)  My son the doctor
		if (((or&RE_OBJECT_ROLE) && object->objectClass!=NAME_OBJECT_CLASS))
		{
	    if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s not put in local objects - RE_OBJECT",where,objectString(object,tmpstr,false).c_str());
			noLocalMatch=true;
		}
		if (((or&(SUBJECT_ROLE|IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))==IS_OBJECT_ROLE) && m[where].relSubject>=0 && m[m[where].relSubject].getObject()>=0)
		{
	    if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s not put in local objects - IS_OBJECT [SUBJ=%d]",where,objectString(object,tmpstr,false).c_str(),m[where].relSubject);
			// it was Annette
			if (lsi==localObjects.end() && objects[m[where].getObject()].objectClass==NAME_OBJECT_CLASS && 
					m[m[where].beginObjectPosition].queryWinnerForm(determinerForm)<0 && objects[m[where].getObject()].PISDefinite>0 && m[where].relSubject>=0 && m[m[where].relSubject].word->first==L"it")
			{
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Introduction IS_OBJECT?",where);
			}
			else
				noLocalMatch=true;
		}
		if (noLocalMatch)
		{
			eliminateBodyObjectRedundancy(where,objectMatches);
		  unMatchObjects(where,objectMatches,definitelySpeaker);
			for (vector <cOM>::iterator om=objectMatches.begin(),omEnd=objectMatches.end(); om!=omEnd; om++)
			{
				if (om->object<0)
					break;
				if (objects[om->object].objectClass!=NAME_OBJECT_CLASS)
				{
					// if cata speaker was invoked, an object may be matched by itself because it was already on the localObjects stack.
					if (m[where].getObject()!=om->object && in(om->object,m[where].objectMatches)==m[where].objectMatches.end())
					{
						m[where].objectMatches.push_back(*om);
						objects[om->object].locations.push_back(cObject::cLocation(where));
						objects[om->object].updateFirstLocation(where);
					}
				}
				else
				{
					// even if object is already on stack, update other fields associated with the localSpeaker.
					if (pushObjectIntoLocalFocus(where,om->object,definitelySpeaker,notSpeaker,inPrimaryQuote,inSecondaryQuote,L"chooseBest (IS_OBJECT)",lsi))
					{
						// if cata speaker was invoked, an object may be matched by itself because it was already on the localObjects stack.
						if (m[where].getObject()!=om->object)
						{
							pushLocalObjectOntoMatches(where,lsi,L"chooseBest (IS_OBJECT)");
							lsi->om.salienceFactor=SALIENCE_THRESHOLD;
						}
					}
				}
			}
		  if (object->objectClass==NAME_OBJECT_CLASS && debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:[END with no local match] RESOLVING name %s with %s",where,objectString(object,tmpstr,false).c_str(),objectString(objectMatches,tmpstr2,false).c_str());
			setQuoteContainsSpeaker(where,inPrimaryQuote);
			return;
		}
	}
  if (objectMatches.empty() && m[where].getObject()>=0 && !avoidCurrentSpeaker)
  {
		if ((m[where].objectRole&SUBJECT_ROLE) && m[where].word->first==L"it")
		{
			m[where].flags|=WordMatch::flagObjectPleonastic;
			m[where].objectRole|=SUBJECT_PLEONASTIC_ROLE;
			if (m[where].getRelObject()>=0)
				m[m[where].getRelObject()].objectRole|=SUBJECT_PLEONASTIC_ROLE;
	    if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:subject is set pleonastic (object@%d).",where,m[where].getRelObject());
			return;
		}
		if (m[where].objectMatches.size())
		  unMatchObjects(where,objectMatches,definitelySpeaker);
		// do not allow an unresolved quantifier or an unresolved numeralCardinal to get into localObjects
		if (!object->eliminated && (object->end-object->begin>1 || (m[where].queryWinnerForm(quantifierForm)<0 && m[where].queryWinnerForm(numeralCardinalForm)<0))  )
		{
			if (pushObjectIntoLocalFocus(where,m[where].getObject(),definitelySpeaker,notSpeaker,inPrimaryQuote,inSecondaryQuote,L"chooseBest (1)",lsi))
			{
				vector <cLocalFocus>::iterator tlsi=substituteAlias(where,lsi);
				if (tlsi!=lsi && tlsi!=localObjects.end())
					pushLocalObjectOntoMatches(where,tlsi,L"alias (2)");
				if (object->objectClass==NAME_OBJECT_CLASS)
					lsi->om.salienceFactor=SALIENCE_THRESHOLD;
			}
		}
		if (object->objectClass==NAME_OBJECT_CLASS && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:[END with self match] RESOLVING name %s",where,objectString(object,tmpstr,false).c_str());
		setQuoteContainsSpeaker(where,inPrimaryQuote);
    return;
  }
  if (object->objectClass==BODY_OBJECT_CLASS) 
    pushObjectIntoLocalFocus(where,m[where].getObject(),definitelySpeaker,notSpeaker,inPrimaryQuote,inSecondaryQuote,L"BODY",lsi);
  eliminateBodyObjectRedundancy(where,objectMatches);
  unMatchObjects(where,objectMatches,definitelySpeaker);
	bool inSpeakerGroup=false;
	if (inSpeakerGroup=currentSpeakerGroup<speakerGroups.size() && objectMatches.size() && speakerGroups[currentSpeakerGroup].speakers.find(m[where].getObject())!=speakerGroups[currentSpeakerGroup].speakers.end())
		speakerGroups[currentSpeakerGroup].speakers.erase(m[where].getObject());
  for (vector <cOM>::iterator om=objectMatches.begin(),omEnd=objectMatches.end(); om!=omEnd; om++)
  {
		// even if object is already on stack, update other fields associated with the localSpeaker.
		if (!pushObjectIntoLocalFocus(where, om->object, definitelySpeaker, notSpeaker, inPrimaryQuote, inSecondaryQuote, L"chooseBest (2)", lsi)) continue;
		// if cata speaker was invoked, an object may be matched by itself because it was already on the localObjects stack.
		if (m[where].getObject()!=om->object)
		{
			pushLocalObjectOntoMatches(where,lsi,L"chooseBest (3)");
			if (object->objectClass==NAME_OBJECT_CLASS)
				lsi->om.salienceFactor=SALIENCE_THRESHOLD;
			if (inSpeakerGroup)
			{
				wstring tmpstr3;
				speakerGroups[currentSpeakerGroup].speakers.insert(om->object);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION|LOG_SG,L"%d:Substituted %s for %s in speakerGroup %s.",where,objectString(om->object,tmpstr,false).c_str(),objectString(m[where].getObject(),tmpstr2,false).c_str(),toText(speakerGroups[currentSpeakerGroup],tmpstr3));
			}
		}
  }
	processSubjectCataRestriction(where,subjectCataRestriction);
  if (object->objectClass==NAME_OBJECT_CLASS && debugTrace.traceSpeakerResolution)
  {
    lplog(LOG_RESOLUTION,L"%06d:[END] RESOLVING name %s",where,objectString(object,tmpstr,false).c_str());
    printLocalFocusedObjects(where,NAME_OBJECT_CLASS);
  }
	setQuoteContainsSpeaker(where,inPrimaryQuote);
}

