#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "source.h"
#include "time.h"
#include "math.h"
#include <wn.h>
#include "profile.h"

// the below are all initialized
int commaForm=-1,periodForm=-1,reflexivePronounForm=-1,possessivePronounForm=-1,nomForm=-1,personalPronounAccusativeForm=-1;
int possessiveDeterminerForm=-1,interrogativeDeterminerForm=-1,particleForm=-1;
int nounForm=-1,quoteForm=-1,conjunctionForm=-1,quantifierForm=-1,dashForm=-1,bracketForm=-1;
int demonstrativeDeterminerForm=-1,numeralCardinalForm=-1,numeralOrdinalForm=-1,romanNumeralForm=-1,moneyForm=-1,internalStateForm=-1,webAddressForm=-1;
int indefinitePronounForm=-1,reciprocalPronounForm=-1,pronounForm=-1,verbForm=-1,thinkForm=-1,demonymForm=-1,relativeForm=-1,commonProfessionForm=-1,businessForm=-1,friendForm=-1;
int adjectiveForm=-1,adverbForm=-1,honorificForm=-1,honorificAbbreviationForm=-1,determinerForm=-1,doesForm=-1,doesNegationForm=-1,dateForm=-1,timeForm=-1,coordinatorForm=-1,predeterminerForm=-1;
int telephoneNumberForm=-1,verbverbForm=-1,abbreviationForm=-1,numberForm=-1,beForm=-1,haveForm=-1,haveNegationForm=-1,relativizerForm=-1;
int isForm=-1,isNegationForm=-1,doForm=-1,doNegationForm=-1,prepositionForm=-1,telenumForm=-1,sa_abbForm=-1,toForm=-1,interjectionForm=-1,personalPronounForm=-1;
int monthForm=-1,letterForm=-1,modalAuxiliaryForm=-1,futureModalAuxiliaryForm=-1,negationModalAuxiliaryForm=-1,negationFutureModalAuxiliaryForm=-1;

bool showAllSpeakersResolution=false; // temporary
// in secondary quotes, inPrimaryQuote=false
void cSource::printLocalFocusedObjects(int where,int forObjectClass)
{ LFS
  showAllSpeakersResolution=(where==3); // also change at 'traceThisNym'
  if (!debugTrace.traceSpeakerResolution) return;
  bool headerShown=false;
  vector <cLocalFocus>::iterator lsi=localObjects.begin();
  for (unsigned int I=0; I<localObjects.size(); I++,lsi++)
  {
    vector <cObject>::iterator lso=objects.begin()+lsi->om.object;
    if (!showAllSpeakersResolution)
    {
      if (!lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) || lsi->om.salienceFactor<0 ||
				  lso->objectClass==VERB_OBJECT_CLASS || lsi->notSpeaker ||
          (lsi->numEncounters<=1 &&
           lsi->numIdentifiedAsSpeaker<=0 &&
           lsi->numDefinitelyIdentifiedAsSpeaker<=0 &&
           (lsi->getTotalAge()>3 && lso->neuter)))
        continue;
      // an object will never match an object of a 'higher' class
      if ((forObjectClass==NAME_OBJECT_CLASS || forObjectClass==GENDERED_GENERAL_OBJECT_CLASS) &&
          lso->objectClass!=NAME_OBJECT_CLASS && 
					lso->objectClass!=GENDERED_GENERAL_OBJECT_CLASS &&
          lso->objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS && 
					lso->objectClass!=GENDERED_DEMONYM_OBJECT_CLASS && 
					lso->objectClass!=GENDERED_RELATIVE_OBJECT_CLASS)
        continue;
    }
    if (!headerShown)
    {
			lplog(LOG_RESOLUTION,L"**%s**",(cLocalFocus::salienceIndependent(quoteIndependentAge)) ? L"** ANY **" : ((cLocalFocus::salienceInQuote(objectToBeMatchedInQuote)) ? L" IN QUOTES ":L" OUTSIDE QUOTES "));
      lplog(LOG_RESOLUTION,L"%6d[%6s]:%-35s #! #I #U %6s M F N p NS qAGE pqA uqA puq aAG paA CA PP SF     ROLE",where,L" O#",L"localSpeaker",L"class");
      headerShown=true;
    }
    wstring tmpstr,sclass=getClass(lso->objectClass);
    lplog(LOG_RESOLUTION,L"   %03d[%06d]:%-35.35s %2d %2d %2d %6s %s %s %s %s %s %3d %3d %3d %3d %3d %3d %2d %2s %6d %s %s",
      I,lsi->om.object,
      phraseString(lso->begin,lso->end,tmpstr,false).c_str(),
      lsi->numDefinitelyIdentifiedAsSpeaker,
      lsi->numIdentifiedAsSpeaker,
      lsi->numEncounters,
      sclass.c_str(),
      (lso->male)?L"M":L" ",
      (lso->female)?L"F":L" ",
      (lso->neuter)?L"N":L" ",
      (lso->plural)?L"P":L" ",
      (lsi->notSpeaker)?L"NS":L"  ",
      lsi->getQuotedAge(),
      lsi->getQuotedPreviousAge(),
      lsi->getUnquotedAge(),
      lsi->getUnquotedPreviousAge(),
      lsi->getTotalAge(),
      lsi->getTotalPreviousAge(),
			lsi->numMatchedAdjectives,
			(lsi->physicallyPresent) ? L"P ":L"  ",
      lsi->om.salienceFactor,
      m[locationBefore(lsi->om.object,where)].roleString(tmpstr).c_str(),
      lsi->res.c_str()
      );
  }
  if (!headerShown)
		lplog(LOG_RESOLUTION,L"** No local objects %s**",(cLocalFocus::salienceIndependent(quoteIndependentAge)) ? L"" : ((cLocalFocus::salienceInQuote(objectToBeMatchedInQuote)) ? L" IN QUOTES ":L" OUTSIDE QUOTES "));
}

// integrate the LL procedures into the resolveObject
// finish the RAP description
wstring cSource::objectString(int object,wstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag)
{ LFS
  if (object>=(int)objects.size())
    return logres=L"ILLEGAL!";
  switch (object)
  {
	case 0:return logres=L"Narrator";
	case 1:return logres=L"Audience";
  case cObject::eOBJECTS::UNKNOWN_OBJECT: return logres=L"UNK";
  case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE: return logres=L"UNK_M";
  case cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE: return logres=L"UNK_F";
  case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE_OR_FEMALE: return logres=L"UNK_M_OR_F";
  case cObject::eOBJECTS::OBJECT_UNKNOWN_NEUTER: return logres=L"UNK_N";
  case cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL: return logres=L"UNK_P";
  case cObject::eOBJECTS::OBJECT_UNKNOWN_ALL: return logres=L"ALL";
  default:
    return objectString(objects.begin()+object,logres,shortNameFormat,objectOwnerRecursionFlag);
  }
}

wstring cSource::objectString(cOM om,wstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag)
{ LFS
  objectString(om.object,logres,shortNameFormat,objectOwnerRecursionFlag);
  if (shortNameFormat) return logres;
  wchar_t temp[15];
  _itow(om.salienceFactor,temp,10);
  logres+=temp;
  return logres;
}

wstring cSource::objectString(vector <cObject>::iterator object,wstring &logres,bool shortFormat,bool objectOwnerRecursionFlag)
{ LFS
  if (object->objectClass==NAME_OBJECT_CLASS && !object->name.notNull() && (object->end-object->begin)>1) // (added object length consideration because otherwise this will print US as Us - correct printing of acronyms that are names)
    object->name.print(logres,true);
  else
    phraseString(object->begin,object->end,logres,shortFormat);
	if (!shortFormat)
	{
		int ow = object->getOwnerWhere();
		if (ow >= 0)
		{
			wstring tmpstr;
			bool selfOwn = object == (objects.begin() + m[ow].getObject());
			for (unsigned int I = 0; I < m[ow].objectMatches.size(); I++)
				if (object == (objects.begin() + m[ow].objectMatches[I].object))
				{
					tmpstr = L"Self-owning object!";
					selfOwn = true;
				}
			if (!selfOwn && !objectOwnerRecursionFlag)
			{
				if (m[ow].objectMatches.size())
					objectString(m[ow].objectMatches, tmpstr, true, true);
				else
					objectString(m[ow].getObject(), tmpstr, true, true);
			}
			logres += L"{OWNER:" + tmpstr + L"}";
		}
		if (object->getOwnerWhere() < -1)
		{
			logres += wstring(L"{WO:") + cObject::wordOrderWords[-2 - object->getOwnerWhere()] + L"}";
		}
		wchar_t temp[1024];
		_itow(object->originalLocation, temp, 10);
		logres += L"[" + wstring(temp) + L"]";
		logres += L"[" + getClass(object->objectClass) + L"]";
		if (object->male) logres += L"[M]";
		if (object->female) logres += L"[F]";
		if (object->neuter) logres += L"[N]";
		if (object->plural) logres += L"[PL]";
		if (object->getOwnerWhere() != -1) logres += L"[OGEN]";
		//logres+=roleString(object->originalLocation,tmpstr);
		if (object->objectClass == NAME_OBJECT_CLASS)
		{
			wstring tmpstr;
			logres += L"[" + object->name.print(tmpstr, false) + L"]";
		}
		if (object->suspect) logres += L"[suspect]";
		if (object->verySuspect) logres += L"[verySuspect]";
		if (object->ambiguous) logres += L"[ambiguous]";
		if (object->eliminated) logres += L"[ELIMINATED]";
		if (object->isTimeObject) logres += L"[TimeObject]";
		if (object->isLocationObject) logres += L"[LocationObject]";
		if (object->isWikiBusiness) logres += L"[WikiBusiness]";
		if (object->isWikiPerson) logres += L"[WikiPerson]";
		if (object->isWikiPlace) logres += L"[WikiPlace]";
		if (object->isWikiWork) logres += L"[WikiWork]";
		if (object->getSubType() >= 0) logres += wstring(L"[") + OCSubTypeStrings[object->getSubType()] + L"]";
		if (object->relativeClausePM >= 0 && object->objectClass != NAME_OBJECT_CLASS)
		{
			wstring tmpstr2;
			logres += L"[" + phraseString(object->whereRelativeClause, object->whereRelativeClause + m[object->whereRelativeClause].pma[object->relativeClausePM].len, tmpstr2, shortFormat) + L"]";
		}
	}
  return logres;
}

const wchar_t *cSource::getOriginalWord(int I, wstring &out, bool concat, bool mostCommon)
{	DLFS
	if (!concat)
		out.clear();
	int len=out.length();
	if (mostCommon)
	{
		wstring tmpCommon;
		bool isNoun=m[I].queryWinnerForm(nounForm)>=0,isVerb=m[I].queryWinnerForm(verbForm)>=0,isAdjective=m[I].queryWinnerForm(adjectiveForm)>=0,isAdverb=m[I].queryWinnerForm(adverbForm)>=0;
		out+=getMostCommonSynonym((m[I].word->second.mainEntry==wNULL) ? m[I].word->first : m[I].word->second.mainEntry->first,tmpCommon,isNoun,isVerb,isAdjective,isAdverb,debugTrace);
	}
	else
		out+=m[I].word->first;
  if (m[I].queryForm(PROPER_NOUN_FORM_NUM)>=0 || (m[I].flags&cWordMatch::flagFirstLetterCapitalized))
    out[len]=towupper(out[len]);
  if (m[I].flags&cWordMatch::flagNounOwner)
    out+=L"'s";
  if (m[I].flags&cWordMatch::flagAllCaps)
    for (; out[len]; len++) out[len]=towupper(out[len]);
  return out.c_str();
}

void cSource::getOriginalWords(int where,vector <wstring> &wsoStrs,bool notFirst)
{ LFS
	wstring oStr;
	oStr[0]=0;
	if (notFirst)
		oStr+=L"+";
	if (m[where].flags&cWordMatch::flagAdjectivalObject)
	{
		vector <wstring> originalWsoStrs=wsoStrs;
		bool makeCopy=false;
		for (unsigned int I=0; I<m[where].objectMatches.size(); I++)
		{
			int beginCopy=0;
			if (originalWsoStrs.empty())
				wsoStrs.push_back(L"");
			else if (makeCopy)
			{
				beginCopy=wsoStrs.size();
				wsoStrs.insert(wsoStrs.begin(),originalWsoStrs.begin(),originalWsoStrs.end());
			}
			makeCopy=true;
			wstring logres;
			objectString(objects.begin()+m[where].objectMatches[I].object,logres,true);
			int oc=objects[m[where].objectMatches[I].object].objectClass;
			if (oc==NAME_OBJECT_CLASS || oc==NON_GENDERED_BUSINESS_OBJECT_CLASS || oc==NON_GENDERED_NAME_OBJECT_CLASS)
			{
				if (logres[logres.size()-1]==L' ')
					logres.erase(logres.size()-1);
				logres+=L"'s";
			}
			replace(logres.begin(), logres.end(), L' ', L'+');
			for (unsigned int J=beginCopy; J<wsoStrs.size(); J++)
				wsoStrs[J]+=oStr+logres;
		}
		if (m[where].objectMatches.empty() && m[where].getObject()>=0)
		{
			if (wsoStrs.empty())
				wsoStrs.push_back(L"");
			wstring logres;
			objectString(objects.begin()+m[where].getObject(),logres,true);
			if (m[m[where].endObjectPosition-1].flags&cWordMatch::flagNounOwner)
			{
				if (logres[logres.size()-1]==L' ')
					logres.erase(logres.size()-1);
				logres+=L"'s";
			}
			replace(logres.begin(), logres.end(), L' ', L'+');
			for (unsigned int J=0; J<wsoStrs.size(); J++)
				wsoStrs[J]+=oStr+logres;
		}
	}
	else
	{
		getOriginalWord(where,oStr,true);
		if (wsoStrs.empty())
			wsoStrs.push_back(L"");
		for (unsigned int I=0; I<wsoStrs.size(); I++)
			wsoStrs[I]+=oStr;
	}
}

wstring cSource::phraseString(int begin,int end,wstring &logres,bool shortFormat,wchar_t *separator)
{ LFS
	if (end>=(signed)m.size())
		end=m.size();
	logres.clear();
	for (int K = begin; K<end && logres.length()<900; K++)
  {
    getOriginalWord(K,logres,true);
		if (K<end - 1) logres += separator;
  }
	if (begin != end && !shortFormat)
	{
		wchar_t temp[1024];
		wsprintf(temp, L"[%d-%d]", begin, end);
		logres += temp;
	}
  return logres;
}

wstring cSource::whereString(int where,wstring &logres,bool shortFormat)
{ LFS
	int numWords=0;
	return whereString(where,logres,shortFormat,0,L" ",numWords);
}

wstring cSource::whereString(vector <int> &wheres,wstring &logres)
{ LFS
  logres.clear();
  for (unsigned int s=0; s<wheres.size(); s++)
  {
    wstring tls;
    whereString(wheres[s],tls,true);
    logres+=tls;
    if (s!=wheres.size()-1) logres+=L" ";
  }
  return logres;
}

wstring cSource::whereString(set <int> &wheres,wstring &logres)
{ LFS
  logres.clear();
	for (set <int>::iterator s=wheres.begin(),sEnd=wheres.end(); s!=sEnd; s++)
  {
    wstring tls;
    whereString(*s,tls,true);
    logres+=tls;
    logres+=L" ";
  }
  return logres;
}

wstring cSource::whereString(int where,wstring &logres,bool shortFormat,int includeNonMixedCaseDirectlyAttachedPrepositionalPhrases,wchar_t *separator,int &numWords)
{ LFS
	if (where<0) return logres=L"(None)";
	if (where>=(int)m.size()) return logres=L"(ILLEGAL)";
	if (m[where].objectMatches.size())
		objectString(m[where].objectMatches,logres,shortFormat);
	else if (m[where].getObject()!=-1)
		objectString(m[where].getObject(),logres,shortFormat);
	else
		logres+=m[where].word->first;
	if (includeNonMixedCaseDirectlyAttachedPrepositionalPhrases>0)
	{
		numWords=0;
		vector <wstring> prepPhraseStrings;
		appendPrepositionalPhrases(where,logres,prepPhraseStrings,numWords,true,separator,includeNonMixedCaseDirectlyAttachedPrepositionalPhrases);
		if (prepPhraseStrings.size()==1)
			logres=prepPhraseStrings[0];
	}
	else 
		return logres;
	// if capitalized, also include all capitalized words after this, skipping over nonalphanumeric words, and allowing prepositions, numbers and coordinators to be uncapitalized.  Ending at a period or TABLE constant.
	// The Spatial Economy – Cities , Regions and International Trade ( July 1999 ) , with Masahisa Fujita and Anthony Venables .
	// EMU and the Regions ( December 1995 ) , with Guillermo de la Dehesa .
	// Development , Geography , and Economic Theory ( Ohlin Lectures ) ( September 1995 )
	int lastWord=m[where].endObjectPosition;
	if (lastWord>=0)
		lastWord+=numWords;
	lastWord--;
	if (lastWord<0) lastWord=where;
	bool isLastUpper=m[lastWord].queryForm(PROPER_NOUN_FORM_NUM)>=0 || (m[lastWord].flags&cWordMatch::flagFirstLetterCapitalized) || (m[lastWord].flags&cWordMatch::flagAllCaps);
	if (!isLastUpper) return logres;
	int I=where;
	if (m[I].endObjectPosition>(signed)I)
		I=m[I].endObjectPosition+numWords;
	int start=I;
	for (; I<(signed)m.size() && isLastUpper && m[I].word!=Words.TABLE && m[I].word!=Words.END_COLUMN && m[I].word!=Words.END_COLUMN_HEADERS && m[I].word!=Words.MISSING_COLUMN  && !m[I].word->second.isSeparator() && m[I].word->first[0]!=L'.'; I++)
	{
		isLastUpper=!iswalpha(m[I].word->first[0]) || m[I].queryWinnerForm(prepositionForm)>=0 || m[I].queryWinnerForm(coordinatorForm)>=0 || m[I].queryWinnerForm(determinerForm)>=0 || 
			((m[I].queryForm(PROPER_NOUN_FORM_NUM)>=0 || (m[I].flags&cWordMatch::flagFirstLetterCapitalized) || (m[I].flags&cWordMatch::flagAllCaps)));
		if (!isLastUpper)
			break;
		//if (!isUpper)
		//{
		//	lplog(LOG_WHERE,L"%d:%s:%s",I,m[I].word->first.c_str(),(iswalpha(m[I].word->first[0])) ? L"true":L"false");
		//}
		if (m[I].getObject()>0)
			I=m[I].endObjectPosition-1; // skip objects and periods associated with abbreviations and names
	}
	while (start<I && (m[I-1].queryWinnerForm(prepositionForm)>=0 || m[I].queryWinnerForm(coordinatorForm)>=0 || m[I].queryWinnerForm(determinerForm)>=0 || (!iswalpha(m[I].word->first[0]) && m[I].word->first!=L"”")))
		I--;
	if (start!=I)
	{
		wstring tmpstr;
		logres+=L" "+phraseString(start,I,tmpstr,shortFormat,separator);
	}
	numWords=I-start;
	return logres;
}

wstring cSource::objectString(vector <cOM> &oms,wstring &logres,bool shortNameFormat,bool objectOwnerRecursionFlag)
{ LFS
  logres.clear();
  for (unsigned int s=0; s<oms.size(); s++)
  {
	  wstring tmp;
    objectString(oms[s],tmp,shortNameFormat,objectOwnerRecursionFlag);
    logres+=tmp;
    if (s!=oms.size()-1) logres+=L" ";
  }
  return logres;
}

bool oCompare(const cOM &lhs,const cOM &rhs)
{ LFS
	return lhs.object>rhs.object;
}

wstring cSource::objectSortedString(vector <cOM> &objs,wstring &logres)
{ LFS
	vector <cOM> sortedObjects=objs;
	sort(sortedObjects.begin(),sortedObjects.end(),oCompare);
	return objectString(sortedObjects,logres,true);
}

wstring cSource::objectString(vector <int> &objectVector,wstring &logres)
{ LFS
  logres.clear();
  for (unsigned int s=0; s<objectVector.size(); s++)
  {
    wstring tls;
    objectString(objectVector[s],tls,true);
    logres+=tls;
    if (s!=objectVector.size()-1) logres+=L" ";
  }
  return logres;
}

wstring cSource::objectString(cSpeakerGroup::cGroup &group,wstring &logres)
{ LFS
	itos(group.where,logres);
	wstring tmpstr;
	logres+=L":"+objectString(group.objects,tmpstr);
  return logres;
}

wstring cSource::objectSortedString(vector <int> &objs,wstring &logres)
{ LFS
	vector <int> sortedObjects= objs;
	sort(sortedObjects.begin(),sortedObjects.end());
	return objectString(sortedObjects,logres);
}

wstring cSource::wordString(vector <tIWMM> &words,wstring &logres)
{ LFS
  logres.clear();
	for (vector <tIWMM>::iterator wi=words.begin(),wiEnd=words.end(); wi!=wiEnd; wi++)
    logres+=(*wi)->first+L" ";
  return logres;
}

wstring cSource::objectString(set <int> &objs,wstring &logres,bool shortNameFormat)
{ LFS
  logres.clear();
  for (set <int>::iterator o= objs.begin(),oEnd= objs.end(); o!=oEnd; o++)
  {
    wstring tls;
    objectString(*o,tls,shortNameFormat);
    logres+=tls+L" ";
  }
  if (logres.length()) logres.erase(logres.size()-1,1);
  return logres;
}

void cSource::printSpeakerQueue(int where)
{ LFS
  if (debugTrace.traceSpeakerResolution)
  {
    wstring s1,s2;
    lplog(LOG_RESOLUTION,L"%06d:beforePreviousSpeakers=%s previousSpeakers=%s",where,
      objectString(beforePreviousSpeakers,s1).c_str(),objectString(previousSpeakers,s2).c_str());
  }
}

// if there is no EOS or '.' at the end inside of the quote:
//    there is text on both sides of the quote, return true.    [the adjective "old" was misleading] [he crossed to a door named 'private' on it]
//                                                              [Below the name were the words “Esthonia Glassware Co . , ” and the address of a city office .]
//    there is only text after the quote,                       ["Hilly" was someone to avoid.]
//    there is only text before the quote                       [she was known to her friends as "Tuppence".]
//    there is no text on either side                           return false
// if there is an EOS or '.' at the end inside of the quote:
//    there is text before the quote:                           [she was known to her friends as "Tuppence."] [It bore the inscription , “mr. Edward Whittington . ”]
//    there is no text before the quote:                        return false

// if "" occurs and there is no , in it or right after it, and no EOS or . in it, it is a quoted string, don't resolve.
// if "" occurs and there is only one word, or a _NOUN match and there is no speaker matches before or after in the same
//   paragraph, ignore.
// if !immediatelyAfterEndOfParagraph (if in a paragraph after the first speaker quote in the paragraph)
// and textBefore OR textAfter, quotedString is true.
bool cSource::quotedString(unsigned int beginQuote,unsigned int endQuote,bool &noTextBeforeOrAfter,bool &noSpeakerAfterward)
{ LFS
  if (noSpeakerAfterward=((m[endQuote].flags&cWordMatch::flagInsertedQuote)!=0)) return false;
  bool EOSEmbeddedInside=m[endQuote-1].word->first==L"." || m[endQuote-1].word->first==L"?" || m[endQuote-1].word->first==L"!";
	if (endQuote-beginQuote>5 && !EOSEmbeddedInside)
	{
		for (vector <cWordMatch>::iterator im=m.begin()+beginQuote+1,imEnd=m.begin()+endQuote-1; im!=imEnd; im++)
			if (EOSEmbeddedInside=im->word->first==L"." || im->word->first==L"?" || im->word->first==L"!")
				break;
	}
	bool textBeforeQuote=beginQuote>0 && m[beginQuote-1].word->first!=L"." && m[beginQuote-1].word->first!=L"?" && m[beginQuote-1].word->first!=L"!" && m[beginQuote-1].word!=Words.sectionWord;
	bool textAfterQuote=!EOSEmbeddedInside && endQuote+1<m.size() && m[endQuote+1].word->first!=L"." && m[endQuote+1].word->first!=L"?" && m[endQuote+1].word->first!=L"!" && m[endQuote+1].word!=Words.sectionWord;
  noTextBeforeOrAfter=(!textBeforeQuote && !textAfterQuote);
  noSpeakerAfterward=textBeforeQuote && EOSEmbeddedInside;
  //for (unsigned int I=beginQuote; I<endQuote-2; I++)
  //  if (m[I].word->first==L"." || m[I].word->second.query(EOSForm)>=0)
  //    return false;
  if (textBeforeQuote && textAfterQuote)
    return true;
  return false;
}

// a word indicating it is an object which is in the same speakerGroup as its owner
bool cSource::isMetaGroupWord(int where)
{ LFS
	return m[where].queryForm(friendForm)>=0;
}

wchar_t *nonTransferrableAdjectives[]={L"new",NULL};
// adjectives that do not denote an enduring trait applying to the appearance or demeanor of the owner
bool cSource::isNonTransferrableAdjective(tIWMM word)
{ LFS
	for (int p=0; nonTransferrableAdjectives[p]; p++)
		if (word->first==nonTransferrableAdjectives[p])
			return true;
	return false;
}

// voices can signify more than one person and so should be a plural body object
// pluralAllowed is set if the owner is plural, so head[s] is acceptable even if a person has only one head.
bool cSource::isExternalBodyPart(int where,bool &singular,bool pluralAllowed)
{ LFS
	singular=false;
	wstring word=m[where].word->first;
	bool plural=(m[where].word->second.inflectionFlags&PLURAL)==PLURAL;
	if (plural && m[where].word->second.mainEntry!=wNULL)
		word=m[where].word->second.mainEntry->first;
	wchar_t *externalPluralBodyParts[]={L"arm",L"leg",L"eye",L"hair",L"hand",L"finger",L"foot",L"toe",L"elbow",L"knee",L"ankle",L"shoulder",
		L"wrist",L"ear",L"cheek",L"lip",L"fist",L"palm",L"thumb",L"eyebrow",L"brow",L"thigh",L"shin",L"gum",L"rib",L"foot",L"gesture",L"look",
		L"heel",L"eyebrow",L"voice",L"accent",L"tone",L"emotion",L"thought",L"footstep",NULL};
	for (int p=0; externalPluralBodyParts[p]; p++)
		if (word==externalPluralBodyParts[p])
			return true;
	/*
	if (word[word.length()-1]==L's')
	{
		word.erase(word.length()-1,1);
		for (int p=0; externalPluralBodyParts[p]; p++)
			if (word==externalPluralBodyParts[p])
				return true;
		word=m[where].word->first;
	}
	*/
	if ((m[where].word->second.inflectionFlags&SINGULAR)==SINGULAR || pluralAllowed) // an object can be both singular and plural
	{
		singular=true;
		wchar_t *externalSingularBodyParts[]={L"accent",L"appearance",L"attitude",L"back",L"beard",L"body",L"brain",L"breath",L"brow",L"chest",L"chin",
			L"eyebrow",L"face",L"gaze",L"glance",L"grin",L"goatee",L"head",L"heart",L"jaw",L"look",L"manner",L"mind",L"mouth",L"mustache",L"neck",L"nose",
			L"smile",L"stomach",L"tone",L"tongue",L"torso",L"voice",L"waist",L"countenance",L"figure",
			NULL};
		for (int p=0; externalSingularBodyParts[p]; p++)
			if (word==externalSingularBodyParts[p])
				return true;
	}
  return false;
}

bool writeStringVector(wchar_t *path,set <wstring> &v)
{ LFS
	FILE *fp=_wfopen(path,L"wb"); // binary mode reads unicode
	if (!fp) return false;
	fputwc(0xFEFF,fp);
	for (set <wstring>::iterator vi=v.begin(),viEnd=v.end(); vi!=viEnd; vi++)
	{
		fputws(vi->c_str(),fp);
		fputws(L"\r\n",fp);
	}
	fclose(fp);
	return true;
}

bool readStringVector(wchar_t *path,set <wstring> &v)
{ LFS
	FILE *fp=_wfopen(path,L"rb"); // binary mode reads unicode
	if (!fp) return false;
	wchar_t buf[1024];
	while (fgetws(buf,100,fp))
	{
		if (buf[0]==0xFEFF) // detect BOM
			memcpy(buf,buf+1,(wcslen(buf+1)+1)*sizeof(buf[0]));
		if (buf[wcslen(buf)-1]=='\n') buf[wcslen(buf)-1]=0;
		if (buf[wcslen(buf)-1]=='\r') buf[wcslen(buf)-1]=0; // in binary mode, cr/lf is not translated
		v.insert(buf);
	}
	fclose(fp);
	return true;
}

// all abstract entities that are psychological attributes
// L"communication" includes such things as screaming, etc.
// L"measure" includes milkshake and other things which are measured
// L"attribute" includes all diseases
// L"relation" includes all familial relations (husband)
// L"group" includes all groups like bikers, rockers, skinheads
// L"set" includes tormentor and radio

/*
// when changing this definition, the WNcache must be deleted
wchar_t *measurableObject[]={NULL};
wchar_t *notMeasurableObject[]={L"mind",NULL};
bool cSource::isPsychologicalFeature(int where)
{ LFS
	tIWMM w;
	static bool pfInit=false;
	if (!pfInit)
	{
		pfInit=true;
		set <wstring> cs,physical,psychological;
		if (!readStringVector(L"source\\lists\\physical_cache.txt",physical))
		{
			addHyponyms(L"physical_object",physical);
			addHyponyms(L"physical_entity",physical);
			writeStringVector(L"source\\lists\\physical_cache.txt",physical);
		}
		// The chauffeur showed no interest.
		if (!readStringVector(L"source\\lists\\cognitive_state.txt",cs))
		{
			addHyponyms(L"cognitive_state",cs);
			writeStringVector(L"source\\lists\\cognitive_state.txt",cs);
		}
		physical.insert(cs.begin(),cs.end());
		if (!readStringVector(L"source\\lists\\psychological_feature_cache.txt",psychological))
		{
			addHyponyms(L"psychological_feature",psychological);
			writeStringVector(L"source\\lists\\psychological_feature_cache.txt",psychological);
		}
		for (set <wstring>::iterator pi=physical.begin(),piEnd=physical.end(); pi!=piEnd; pi++)
		{
			w=Words.query(*pi);
			if (w!=Words.end() && w->second.mainEntry!=wNULL) 
			{
				if (w!=Words.end() && w->second.query(nounForm)<0) continue;
				w->second.flags|=cSourceWordInfo::physicalObjectByWN;
				w=w->second.mainEntry;
			}
			if (w!=Words.end() && w->second.query(nounForm)<0) continue;
			if (w!=Words.end()) 
				w->second.flags|=cSourceWordInfo::physicalObjectByWN;
		}
		for (unsigned int I=0; measurableObject[I]; I++)
		{
			w=Words.query(measurableObject[I]);
			if (w!=Words.end() && w->second.mainEntry!=wNULL) 
			{
				w->second.flags|=cSourceWordInfo::physicalObjectByWN;
				w=w->second.mainEntry;
			}
			if (w!=Words.end()) 
				w->second.flags|=cSourceWordInfo::physicalObjectByWN;
		}
		for (set <wstring>::iterator pi=psychological.begin(),piEnd=psychological.end(); pi!=piEnd; pi++)
		{
			w=Words.query(*pi);
			if (w!=Words.end() && w->second.mainEntry!=wNULL) 
			{
				if (w!=Words.end() && w->second.query(nounForm)<0) continue;
				if (!(w->second.flags&cSourceWordInfo::physicalObjectByWN)) 
					w->second.flags|=cSourceWordInfo::notPhysicalObjectByWN;
				w=w->second.mainEntry;
			}
			if (w!=Words.end() && w->second.query(nounForm)<0) continue;
			if (w!=Words.end() && !(w->second.flags&cSourceWordInfo::physicalObjectByWN)) 
				w->second.flags|=cSourceWordInfo::notPhysicalObjectByWN;
		}
		for (unsigned int I=0; notMeasurableObject[I]; I++)
		{
			w=Words.query(notMeasurableObject[I]);
			if (w!=Words.end() && w->second.mainEntry!=wNULL) 
			{
				w->second.flags&=~cSourceWordInfo::physicalObjectByWN;
				w->second.flags|=cSourceWordInfo::notPhysicalObjectByWN;
				w=w->second.mainEntry;
			}
			if (w!=Words.end()) 
			{
				w->second.flags&=~cSourceWordInfo::physicalObjectByWN;
				w->second.flags|=cSourceWordInfo::notPhysicalObjectByWN;
			}
		}
	}
	w=m[where].word;
	if (w->second.mainEntry!=wNULL) return (w->second.mainEntry->second.flags&cSourceWordInfo::notPhysicalObjectByWN)!=0;
	return (w->second.flags&cSourceWordInfo::notPhysicalObjectByWN)!=0;
}
*/

bool cSource::isFace(int where)
{ LFS
	if (where<0) return false;
	wstring word=(m[where].word->second.mainEntry==wNULL) ? m[where].word->first : m[where].word->second.mainEntry->first;
	wchar_t *faces[]={L"face",L"countenance",L"eye",NULL};
	for (int p=0; faces[p]; p++)
		if (word==faces[p])
			return true;
	return false;
}

bool cSource::isVoice(int where)
{ LFS
	if (where<0) return false;
	wstring word=(m[where].word->second.mainEntry==wNULL) ? m[where].word->first : m[where].word->second.mainEntry->first;
	wchar_t *voices[]={L"voice",L"accent",L"tone",NULL};
	for (int p=0; voices[p]; p++)
		if (word==voices[p])
			return true;
	return false;
}

bool cSource::isFacialExpression(int where)
{ LFS
	if (where<0) return false;
	wstring word=m[where].word->first;
	wchar_t *facialExpressions[]={L"gape",L"rictus",L"grimace",L"pout",L"moue",L"frown",L"scowl",L"smile",L"grin",L"simper",L"smirk",L"laugh",L"snarl",L"wink",L"wince",L"leer",L"sparkle",L"look",L"pity",NULL};
	for (int p=0; facialExpressions[p]; p++)
		if (word==facialExpressions[p])
			return true;
	return false;
}

bool cSource::isInternalBodyPart(int where)
{ LFS
	wstring word=m[where].word->first;
	wchar_t *internalSingularBodyParts[]={L"imagination",L"dream",L"mind",L"attention",L"conscience",L"thought",L"thoughts",L"feeling",NULL};
	for (int p=0; internalSingularBodyParts[p]; p++)
		if (word==internalSingularBodyParts[p])
			return true;
	return false;
}

// he was conscious
bool cSource::isInternalDescription(int where)
{ LFS
	wstring word=m[where].word->first;
	wchar_t *internalDescriptions[]={L"conscious",NULL};
	for (int p=0; internalDescriptions[p]; p++)
		if (word==internalDescriptions[p])
			return true;
	return false;
}

// verbs that express possibility directly (although the tense is past and would normally be treated as definite)
bool cSource::isPossible(int where)
{ LFS
	wstring word=(m[where].word->second.mainEntry!=wNULL) ? m[where].word->second.mainEntry->first : m[where].word->first;
	wchar_t *possibleVerbs[]={L"seem",NULL};
	for (int p=0; possibleVerbs[p]; p++)
		if (word==possibleVerbs[p])
			return true;
	return false;
}

wchar_t *groupJoiner[] = { L"comer",L"guest",L"visitor",NULL };
bool cSource::isGroupJoiner(tIWMM word)
{ LFS
	for (unsigned int I=0; groupJoiner[I]; I++)
		if (word->first==groupJoiner[I])
			return true;
	return false;
}

wchar_t *delayedReceiver[] = { L"write",NULL };
bool cSource::isDelayedReceiver(tIWMM word)
{ LFS
	for (unsigned int I=0; delayedReceiver[I]; I++)
		if (word->first==delayedReceiver[I])
			return true;
	return false;
}

// sort, kind, style, type, manner
bool cSource::isKindOf(int where)
{ LFS
	wchar_t *kindWords[]={L"sort",L"kind",L"style",L"type",L"manner",NULL};
  for (int p=0; kindWords[p]; p++)
    if (m[where].word->first==kindWords[p])
      return true;
	// the bigger of the two men
	return (m[where].queryWinnerForm(adjectiveForm)>=0);
}

// Websters
// see, behold, descry, discern, distinguish, espy, eye, note, notice, observe, perceive, regard, sight, spy, view, witness
// identify, watch, examine, inspect, scan, scrutinize, survey, glimpse 
bool cSource::isVision(int where)
{ LFS
	wchar_t *visionWords[]={ L"see", L"behold", L"descry", L"discern", L"distinguish", L"espy", L"eye", L"note", L"notice", L"observe", L"perceive", L"regard", L"sight", 
		L"spy", L"view", L"witness", L"identify", L"watch", L"examine", L"inspect", L"scan", L"scrutinize", L"survey", L"glimpse", NULL};
  for (int p=0; visionWords[p]; p++)
    if (m[where].word->first==visionWords[p] || (m[where].word->second.mainEntry!=wNULL && m[where].word->second.mainEntry->first==visionWords[p]))
      return true;
	return false;
}

// does object overlap matching object or vice-versa?  If so, they do not match.
bool cSource::overlaps(vector <cObject>::iterator object,vector <cObject>::iterator matchingObject)
{ LFS
  if (object->objectClass==BODY_OBJECT_CLASS)
    return false;
  int begin1=object->begin,end1=object->end;
  int begin2=matchingObject->begin,end2=matchingObject->end;
	// begin and end should NOT include relative clauses / the man who ate his hat
  // 1-2 3-4 1<4 && 3<2 = false
  // 3-4 1-2 3<2 && 1<4 = false
  // 1-4 2-6 1<6 && 2<4 = true
  // 2-6 1-4 2<4 && 1<6 = true
  return begin1<end2 && begin2<end1;
}

#define MAX_ENCOUNTERS 3
bool cSource::getHighestEncounters(int &highestDefinitelyIdentifiedEncounters,int &highestIdentifiedEncounters,int &highestEncounters)
{ LFS
  for (unsigned int I=0; I<localObjects.size(); I++)
  {
    //if (inQuote!=localObjects[I].inQuote) continue; GO_NEUTRAL
    highestDefinitelyIdentifiedEncounters=max(highestDefinitelyIdentifiedEncounters,localObjects[I].numDefinitelyIdentifiedAsSpeaker);
    highestIdentifiedEncounters=max(highestIdentifiedEncounters,localObjects[I].numIdentifiedAsSpeaker);
    highestEncounters=max(highestEncounters,localObjects[I].numEncounters);
  }
  highestDefinitelyIdentifiedEncounters=min(highestDefinitelyIdentifiedEncounters,MAX_ENCOUNTERS);
  highestIdentifiedEncounters=min(highestIdentifiedEncounters,MAX_ENCOUNTERS);
  highestEncounters=min(highestEncounters,MAX_ENCOUNTERS);
  if (highestIdentifiedEncounters==-1 && highestEncounters==-1)
  {
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"No previous speakers found.");
    return false;
  }
  return true;
}

void cSource::getOwnerSex(int ownerWhere,bool &matchMale,bool &matchFemale,bool &matchNeuter, bool &matchPlural)
{ LFS
  if (ownerWhere<0) return;
  int ownerObject=m[ownerWhere].getObject();
  switch (ownerObject)
  {
  case cObject::eOBJECTS::UNKNOWN_OBJECT: return;
  case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE: matchMale=true; return;
  case cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE: matchFemale=true; return;
  case cObject::eOBJECTS::OBJECT_UNKNOWN_MALE_OR_FEMALE: matchMale=matchFemale=true; return;
  case cObject::eOBJECTS::OBJECT_UNKNOWN_NEUTER: matchNeuter=true; return;
  case cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL: matchPlural=true; return;
  default:
    if (ownerObject>=0)
    {
      matchMale=objects[ownerObject].male;
      matchFemale=objects[ownerObject].female;
			matchPlural=objects[ownerObject].plural;
    }
  }
}

int ageEncounterCostArray[]={ 100, 80, 60, 50, 40 };
int ageSpeakerCostArray[]={ 100, 80, 60, 50, 40 };
//bool cLocalFocus::quoteIndependentAge,cLocalFocus::objectToBeMatchedInQuote;
int cLocalFocus::rAge(bool previous,bool speaker,bool objectToBeMatchedInQuote,bool quoteIndependentAge)
{ LFS
  int finalAge;
  if (quoteIndependentAge)
    finalAge=(previous) ? unquotedPreviousAge+quotedPreviousAge : unquotedAge+quotedAge;
  else
    finalAge=(previous) ? ((objectToBeMatchedInQuote) ? quotedPreviousAge : unquotedPreviousAge) : ((objectToBeMatchedInQuote) ? quotedAge : unquotedAge);
  if (finalAge<3)
    finalAge=0;
  else if (finalAge>=3+(sizeof(ageEncounterCostArray)/sizeof(*ageEncounterCostArray)))
    finalAge=(sizeof(ageEncounterCostArray)/sizeof(*ageEncounterCostArray))-1;
  else
    finalAge-=3;
  return (speaker) ? ageSpeakerCostArray[finalAge] : ageEncounterCostArray[finalAge];
}

int cLocalFocus::allAge(bool speaker,bool objectToBeMatchedInQuote,bool quoteIndependentAge)
{ LFS
  int finalAge;
  if (quoteIndependentAge)
    finalAge=totalPreviousAge;
  else
    finalAge=(objectToBeMatchedInQuote) ? quotedPreviousAge : unquotedPreviousAge;
  int tmpAge=rAge(false,speaker,objectToBeMatchedInQuote,quoteIndependentAge);
  if (finalAge<0) return tmpAge;
  return tmpAge+rAge(true,speaker,objectToBeMatchedInQuote,quoteIndependentAge);
}

int cLocalFocus::getAge(bool previous,bool objectToBeMatchedInQuote,bool quoteIndependentAge)
{ LFS
  if (previous)
    return quoteIndependentAge ?
      totalPreviousAge : (objectToBeMatchedInQuote) ? quotedPreviousAge : unquotedPreviousAge;
  else
    return quoteIndependentAge ?
      totalAge : (objectToBeMatchedInQuote) ? quotedAge : unquotedAge;
}

// used during salience to retry if no objects match
void cLocalFocus::decreaseAge(bool objectToBeMatchedInQuote,bool quoteIndependentAge)
{ LFS
  if (!includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge))
    return;
  if (occurredInPrimaryQuote)
  {
    if (quotedAge>0) quotedAge>>=1;
    if (quotedPreviousAge>0) quotedPreviousAge>>=1;
  }
  if (occurredOutsidePrimaryQuote)
  {
    if (unquotedAge>0) unquotedAge>>=1;
    if (unquotedPreviousAge>0) unquotedPreviousAge>>=1;
  }
  if (totalAge>0) totalAge>>=1;
  if (totalPreviousAge>0) totalPreviousAge>>=1;
}

void cLocalFocus::increaseAge(bool sentenceInPrimaryQuote,bool sentenceInSecondaryQuote,int amount)
{ LFS
	if (sentenceInSecondaryQuote) return;
  if (sentenceInPrimaryQuote)
  {
    quotedAge+=amount;
    if (quotedPreviousAge>=0) quotedPreviousAge+=amount;
  }
  else
  {
    unquotedAge+=amount;
    if (unquotedPreviousAge>=0) unquotedPreviousAge+=amount;
  }
  totalAge+=amount;
  if (totalPreviousAge>=0) totalPreviousAge+=amount;
}

void cLocalFocus::increaseAge(int amount)
{ LFS
  if (occurredInPrimaryQuote) quotedAge+=amount;
  if (occurredOutsidePrimaryQuote) unquotedAge+=amount;
  if (quotedPreviousAge>=0) quotedPreviousAge+=amount;
  if (unquotedPreviousAge>=0) unquotedPreviousAge+=amount;
  totalAge+=amount;
  if (totalPreviousAge>=0) totalPreviousAge+=amount;
}

// reset an object that was seen previously
void cLocalFocus::resetAge(bool objectToBeMatchedInQuote)
{ LFS
  if (objectToBeMatchedInQuote && !occurredInPrimaryQuote)
    occurredInPrimaryQuote=true;
  if (!objectToBeMatchedInQuote && !occurredOutsidePrimaryQuote)
    occurredOutsidePrimaryQuote=true;
  if (objectToBeMatchedInQuote)
  {
    quotedPreviousAge=quotedAge;
    quotedAge=0;
  }
  else
  {
    unquotedPreviousAge=unquotedAge;
    unquotedAge=0;
  }
  totalPreviousAge=totalAge;
  totalAge=0;
}

void cLocalFocus::saveAge(void)
{ LFS
  saveUnquotedAge=unquotedAge; // age from last encounter outside quotes counting only sentences outside quotes
  saveQuotedAge=quotedAge; // age from last encounter inside quotes counting only sentences inside quotes
  saveTotalAge=totalAge; // age from last encounter anywhere counting all sentences
  saveUnquotedPreviousAge=unquotedPreviousAge;
  saveQuotedPreviousAge=quotedPreviousAge;
  saveTotalPreviousAge=totalPreviousAge;
}

void cLocalFocus::restoreAge(void)
{ LFS
  unquotedAge=saveUnquotedAge; // age from last encounter outside quotes counting only sentences outside quotes
  quotedAge=saveQuotedAge; // age from last encounter inside quotes counting only sentences inside quotes
  totalAge=saveTotalAge; // age from last encounter anywhere counting all sentences
  unquotedPreviousAge=saveUnquotedPreviousAge;
  quotedPreviousAge=saveQuotedPreviousAge;
  totalPreviousAge=saveTotalPreviousAge;
}

// reset a speaker at the beginning of a section
void cLocalFocus::resetAgeBeginSection(bool isObserver)
{ LFS
  unquotedPreviousAge=quotedPreviousAge=totalPreviousAge=-1;
	unquotedAge=totalAge=0;
	// make sure observer doesn't beat out objects that have actually occurred in quote (Mr. Brown - 19807)
	if (isObserver && (!occurredInPrimaryQuote || quotedAge>=100))
		quotedAge=100;
	else
		quotedAge=0;
	numIdentifiedAsSpeaker=1; // don't change if observer - Also used for out-of-quote salience
  numEncounters=1;
  numDefinitelyIdentifiedAsSpeaker=0;
	// 29668 Boris and Rita talking about Tuppence / "New, isn't she?" / an observer can be talked about, even though they are a speaker
	occurredInPrimaryQuote=(isObserver) ? true : false; // a speaker shouldn't appear in a quote as a reference (shouldn't be a salient local object) from a word like 'he' or 'she'
}

wstring cSource::getClass(int objectClass)
{ LFS
  switch (objectClass)
  {
  case PRONOUN_OBJECT_CLASS: return L"pron"; 
  case REFLEXIVE_PRONOUN_OBJECT_CLASS: return L"reflex"; 
  case RECIPROCAL_PRONOUN_OBJECT_CLASS: return L"recip"; 
  case NAME_OBJECT_CLASS: return L"name"; break;
  case GENDERED_GENERAL_OBJECT_CLASS: return L"gender";
  case BODY_OBJECT_CLASS: return L"genbod"; 
  case GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS: return L"genocc"; 
  case GENDERED_DEMONYM_OBJECT_CLASS: return L"gendem"; 
  case GENDERED_RELATIVE_OBJECT_CLASS: return L"genrel"; 
  case NON_GENDERED_GENERAL_OBJECT_CLASS: return L"nongen"; 
  case NON_GENDERED_BUSINESS_OBJECT_CLASS: return L"ngbus"; 
  case NON_GENDERED_NAME_OBJECT_CLASS: return L"ngname"; 
  case VERB_OBJECT_CLASS: return L"vb"; 
  case PLEONASTIC_OBJECT_CLASS: return L"pleona"; 
  case META_GROUP_OBJECT_CLASS: return L"mg"; 
  }
  return L"ILLEGAL";
}

#define MISMATCHING_SEX 5000
#define SYNONYM_SALIENCE 125
//#define PS_SALIENCE 5000

void cSource::adjustForPhysicalPresence()
{ LFS
	int numPP=0,numNPP=0,maxPPSalience=-1,maxNPPSalience=-1;
	wstring tmpstr;
  vector <cLocalFocus>::iterator lsi,lsEnd=localObjects.end();
	for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
		if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=SALIENCE_THRESHOLD)
		{
			if (lsi->physicallyPresent)
			{
	      maxPPSalience=max(maxPPSalience,lsi->om.salienceFactor);
				numPP++;
			}
			else
			{
	      maxNPPSalience=max(maxNPPSalience,lsi->om.salienceFactor);
				numNPP++;
			}
		}
	// non physically-present objects are already removed at the end, this is just to prevent 
	// NPP objects from overshadowing the PP objects and exiting the Adjust procedure prematurely
	if (maxPPSalience<0) // && numPP>0)
		for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
			if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge))
			{ 
				if (!lsi->physicallyPresent)
				{
					lsi->om.salienceFactor-=10000;
					itos(L"-NPP[",-10000,lsi->res,L"]");
				}
				else 
				{
					lsi->om.salienceFactor+=1000;
					itos(L"+PP[",1000,lsi->res,L"]");
				}
			}
}

// in secondary quotes, inPrimaryQuote=false
void cSource::adjustSaliencesByGenderNumberAndOccurrenceAgeAdjust(int where,int object,bool inPrimaryQuote,bool inSecondaryQuote,bool forSpeakerIdentification,int &lastGenderedAge,vector <int> &disallowedReferences,bool disallowOnlyNeuterMatches,bool isPhysicallyPresent,bool physicallyEvaluated)
{ LFS
  for (int tries=0; tries<3; tries++)
  {
    // reset weights
    vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end();
    for (; lsi!=lsEnd; lsi++)
    {
      lsi->om.salienceFactor=lsi->lastRoleSalience;
			lsi->numMatchedAdjectives=0;
      lsi->res.clear();
    }
		if (object>=0 && objects[object].objectClass==META_GROUP_OBJECT_CLASS)
			excludePOVSpeakers(where,L"2");
		if (object>=0 && objects[object].objectClass==BODY_OBJECT_CLASS && objects[object].plural && currentSpeakerGroup<speakerGroups.size() &&
			  speakerGroups[currentSpeakerGroup].povSpeakers.size()==1)
			excludePOVSpeakers(where,L"3");
		if (object>=0 && objects[object].objectClass!=BODY_OBJECT_CLASS && objects[object].getOwnerWhere()>=0 && 
			  m[objects[object].getOwnerWhere()].objectMatches.size()==1 && (lsi=in(m[objects[object].getOwnerWhere()].objectMatches[0].object))!=localObjects.end())
		{
			wstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:owned object cannot match owner (1) %s",where,objectString(lsi->om,tmpstr,true).c_str());
			lsi->om.salienceFactor-=DISALLOW_SALIENCE;
			itos(L"-OWNER[-",DISALLOW_SALIENCE,lsi->res,L"]");
		}
		int tmpLastGenderedAge=6;
    adjustSaliencesByGenderNumberAndOccurrence(where,object,inPrimaryQuote,inSecondaryQuote,forSpeakerIdentification,tmpLastGenderedAge,isPhysicallyPresent);
		for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
			if (find(disallowedReferences.begin(),disallowedReferences.end(),lsi->om.object)!=disallowedReferences.end() ||
				  (objects[lsi->om.object].objectClass==BODY_OBJECT_CLASS && objects[lsi->om.object].getOwnerWhere()>=0 && 
					 find(disallowedReferences.begin(),disallowedReferences.end(),m[objects[lsi->om.object].getOwnerWhere()].getObject())!=disallowedReferences.end()))
			{
				wstring tmpstr;
				lsi->om.salienceFactor-=DISALLOW_SALIENCE;
				itos(L"-DISALLOWED[-",DISALLOW_SALIENCE,lsi->res,L"]");
			}
		if (!inPrimaryQuote && !inSecondaryQuote && isPhysicallyPresent && physicallyEvaluated && tries<2)
			adjustForPhysicalPresence();
		if (!tries) lastGenderedAge=tmpLastGenderedAge;
    int highestSalience=-1,numGenderedMatchingObjects=0;
    for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
      if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge))
			{
        highestSalience=max(highestSalience,lsi->om.salienceFactor);
				if (!objects[lsi->om.object].neuter && lsi->om.salienceFactor>=SALIENCE_THRESHOLD) 
					numGenderedMatchingObjects++;
			}
    if (highestSalience>=SALIENCE_THRESHOLD && (!disallowOnlyNeuterMatches || numGenderedMatchingObjects>=1))
    {
      if (tries)
			{
		    printLocalFocusedObjects(where,0);
        for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
          lsi->restoreAge();
			}
      return;
    }
    if (tries==2) 
		{
      for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
        lsi->restoreAge();
			break;
		}
    if (m[where].getObject()== cObject::eOBJECTS::OBJECT_UNKNOWN_NEUTER ||
        (m[where].getObject()>=0 && objects[m[where].getObject()].neuter && !(objects[m[where].getObject()].male || objects[m[where].getObject()].female)))
      return;
    if (!tries)
		{
			// if a resolvable gendered object while forming speakergroups "the lady", see whether any of the previous speakergroup matches.
			if (object>=0 && objects[object].objectClass==GENDERED_GENERAL_OBJECT_CLASS && !unResolvablePosition(m[where].beginObjectPosition) &&
				currentSpeakerGroup>0 && currentSpeakerGroup==speakerGroups.size())
			{
				vector <cSpeakerGroup>::iterator lastSG=speakerGroups.begin()+currentSpeakerGroup-1;
				for (set <int>::iterator si=lastSG->speakers.begin(),siEnd=lastSG->speakers.end(); si!=siEnd; si++)
					if (in(*si)==localObjects.end() && objects[object].matchGender(objects[*si]))
						pushObjectIntoLocalFocus(where,*si,forSpeakerIdentification,false,inPrimaryQuote,inSecondaryQuote,L"from previous SG",lsi);
				lsEnd=localObjects.end();
			}
      for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
        lsi->saveAge();
		}
    printLocalFocusedObjects(where,0);
    if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
      lplog(LOG_RESOLUTION,L"%06d:No resolution.  Halving ages.",where);
	  cLocalFocus::setSalienceAgeMethod(inSecondaryQuote || inPrimaryQuote,true,objectToBeMatchedInQuote,quoteIndependentAge); // set to match any object regardless of inPrimaryQuote or not
    for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
      lsi->decreaseAge(objectToBeMatchedInQuote,quoteIndependentAge);
  }
}

void cSource::printNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap,wchar_t *type,wchar_t *subtype,wchar_t *subsubtype)
{ LFS
	wstring nyms;
	for (vector <tIWMM>::iterator ni=nyms1.begin(),niEnd=nyms1.end(); ni!=niEnd; ni++)
	{
		if ((*ni)->second.flags&cSourceWordInfo::genericGenderIgnoreMatch) continue;
		nyms+=(*ni)->first+L" (";
		for (vector <tIWMM>::iterator cni=wnMap[*ni].begin(),cniEnd=wnMap[*ni].end(); cni!=cniEnd; cni++)
			nyms+=(*cni)->first+L" ";
		nyms+=L" )";
	}
	lplog(LOG_RESOLUTION,L"NYMS %s %s %s: %s",type,subtype,subsubtype,nyms.c_str());
}

void cSource::printNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap1,
											 vector <tIWMM> &nyms2, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap2,wchar_t *type,wchar_t *subtype,
											 int sharedMembers,wstring &logMatch)
{ LFS
	wstring nyms,nextLine;
	for (vector <tIWMM>::iterator ni=nyms1.begin(),niEnd=nyms1.end(); ni!=niEnd; ni++)
	{
		if ((*ni)->second.flags&cSourceWordInfo::genericGenderIgnoreMatch) continue;
		nyms+=(*ni)->first+L" ";
		nextLine+=L"     "+(*ni)->first+L":(";
		for (vector <tIWMM>::iterator cni=wnMap1[*ni].begin(),cniEnd=wnMap1[*ni].end(); cni!=cniEnd; cni++)
			nextLine+=(*cni)->first+L" ";
		nextLine+=L" )\n";
	}
	nyms+=L" <-> ";
	for (vector <tIWMM>::iterator ni=nyms2.begin(),niEnd=nyms2.end(); ni!=niEnd; ni++)
	{
		if ((*ni)->second.flags&cSourceWordInfo::genericGenderIgnoreMatch) continue;
		nyms+=(*ni)->first+L" ";
		nextLine+=L"     "+(*ni)->first+L":(";
		for (vector <tIWMM>::iterator cni=wnMap2[*ni].begin(),cniEnd=wnMap2[*ni].end(); cni!=cniEnd; cni++)
			nextLine+=(*cni)->first+L" ";
		nextLine+=L" )\n";
	}
	lplog(LOG_RESOLUTION,L"NYMS %s %s: %s [%d %s]\n%s",type,subtype,nyms.c_str(),sharedMembers,logMatch.c_str(),nextLine.c_str());
}

bool cSource::setNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap)
{ LFS
	for (vector <tIWMM>::iterator ni=nyms1.begin(),niEnd=nyms1.end(); ni!=niEnd; ni++)
	{
		if (!((*ni)->second.flags&cSourceWordInfo::alreadyTaken))
		{
			(*ni)->second.flags|=cSourceWordInfo::intersectionGroup;
			for (vector <tIWMM>::iterator cni=wnMap[*ni].begin(),cniEnd=wnMap[*ni].end(); cni!=cniEnd; cni++)
				if (!((*cni)->second.flags&cSourceWordInfo::alreadyTaken))
					(*cni)->second.flags|=cSourceWordInfo::intersectionGroup;
		}
	}
	return !nyms1.empty();
}

void cSource::clearNyms(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap)
{ LFS
	for (vector <tIWMM>::iterator ni=nyms1.begin(),niEnd=nyms1.end(); ni!=niEnd; ni++)
	{
		(*ni)->second.flags&=~cSourceWordInfo::intersectionGroup;
		for (vector <tIWMM>::iterator cni=wnMap[*ni].begin(),cniEnd=wnMap[*ni].end(); cni!=cniEnd; cni++)
			(*cni)->second.flags&=~cSourceWordInfo::intersectionGroup;
	}
}

int cSource::nymMapMatch(vector <tIWMM> &nyms1, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap1,
										 vector <tIWMM> &nyms2, map <tIWMM,vector <tIWMM>,cSourceWordInfo::cRMap::wordMapCompare > &wnMap2,
										 bool mapOnly,bool getFromMatch,bool traceNymMatch,
										 wstring &logMatch,tIWMM &fromMatch,tIWMM &toMatch,tIWMM &toMapMatch,wchar_t *type,wchar_t *subtype)
{ LFS
	vector <tIWMM> markedTaken;
	if (!setNyms(nyms1,wnMap1)) return 0; // set source
	int sharedMembers=0;
	// are there any destinations set?
	for (vector <tIWMM>::iterator ni2=nyms2.begin(),ni2End=nyms2.end(); ni2!=ni2End; ni2++)
	{
		if ((*ni2)->second.flags&cSourceWordInfo::genericGenderIgnoreMatch) continue;
		if (!mapOnly && ((*ni2)->second.flags&(cSourceWordInfo::intersectionGroup|cSourceWordInfo::alreadyTaken))==cSourceWordInfo::intersectionGroup)
		{
			if (getFromMatch)
				for (vector <tIWMM>::iterator ni1=nyms1.begin(),ni1End=nyms1.end(); ni1!=ni1End; ni1++)
				{
					if (*ni1==*ni2 || (find(wnMap1[*ni1].begin(),wnMap1[*ni1].end(),*ni2))!=wnMap1[*ni1].end())
					{
						fromMatch=*ni1;
						if (traceNymMatch)
							logMatch+=L"FROM[O]:"+(*ni1)->first+L" ";
						break;
					}
				}
			sharedMembers++;
			toMatch=*ni2;
			toMapMatch=*ni2;
			if (traceNymMatch)
				logMatch+=L"TO[O]:"+(*ni2)->first+L" ";
			(*ni2)->second.flags|=cSourceWordInfo::alreadyTaken;
			markedTaken.push_back(*ni2);
		}
		if (!((*ni2)->second.flags&cSourceWordInfo::alreadyTaken))
		{
			for (vector <tIWMM>::iterator cni2=wnMap2[*ni2].begin(),cni2End=wnMap2[*ni2].end(); cni2!=cni2End; cni2++)
				if (((*cni2)->second.flags&(cSourceWordInfo::intersectionGroup|cSourceWordInfo::alreadyTaken|cSourceWordInfo::genericGenderIgnoreMatch))==cSourceWordInfo::intersectionGroup)
				{
					if (getFromMatch)
						for (vector <tIWMM>::iterator ni1=nyms1.begin(),ni1End=nyms1.end(); ni1!=ni1End; ni1++)
						{
							if (*ni1==*cni2 || (find(wnMap1[*ni1].begin(),wnMap1[*ni1].end(),*cni2))!=wnMap1[*ni1].end())
							{
								fromMatch=*ni1;
								if (traceNymMatch)
									logMatch+=L"FROM[M]:"+(*ni1)->first+L" ";
								break;
							}
						}
					sharedMembers++;
					toMatch=*ni2;
					toMapMatch=*cni2;
					if (traceNymMatch)
						logMatch+=L"TO[M]:"+(*ni2)->first+L" ("+(*cni2)->first+L") ";
					if (!mapOnly)
					{
						(*cni2)->second.flags|=cSourceWordInfo::alreadyTaken;
						markedTaken.push_back(*cni2);
					}
				}
				else if (traceNymMatch)
					lplog(LOG_RESOLUTION,L"(*cni2)->first=%s ((*cni2)->second.flags&(cSourceWordInfo::intersectionGroup|cSourceWordInfo::alreadyTaken|cSourceWordInfo::genericGenderIgnoreMatch))=%s %s %s",
					      (*cni2)->first.c_str(),
								((*cni2)->second.flags&(cSourceWordInfo::intersectionGroup)) ? L"true" : L"false",
								((*cni2)->second.flags&(cSourceWordInfo::alreadyTaken)) ? L"true" : L"false",
								((*cni2)->second.flags&(cSourceWordInfo::genericGenderIgnoreMatch)) ? L"true" : L"false");
		}
		else if (traceNymMatch)
			lplog(LOG_RESOLUTION,L"(*ni2)->first=%s ((*ni2)->second.flags&cSourceWordInfo::alreadyTaken)=%s",(*ni2)->first.c_str(),((*ni2)->second.flags&cSourceWordInfo::alreadyTaken) ? L"true" : L"false");

	}
	clearNyms(nyms1,wnMap1); // clear source
	if (traceNymMatch)
    printNyms(nyms1,wnMap1,nyms2,wnMap2,type,subtype,sharedMembers,logMatch);
	for (vector <tIWMM>::iterator mi=markedTaken.begin(),miEnd=markedTaken.end(); mi!=miEnd; mi++)
		(*mi)->second.flags&=~cSourceWordInfo::alreadyTaken;
	return sharedMembers;	
}

#define SALIENCE_PER_SPEAKER_ENCOUNTER 300
#define SALIENCE_PER_IDENTIFIED_ENCOUNTER 200
#define SALIENCE_PER_ENCOUNTER 100
/*
#define SALIENCE_PER_SPEAKER_ENCOUNTER 800
#define SALIENCE_PER_IDENTIFIED_ENCOUNTER 600
#define SALIENCE_PER_ENCOUNTER 500
*/
// match gender and number with local objects considering recency and focus.
// the object being matched against localObjects can match by its owner's characteristics, or by the main object's characteristics.
// the owner's characteristics for the objects being matched against are ignored.
void cSource::adjustSaliencesByGenderNumberAndOccurrence(int where,int object,bool inPrimaryQuote,bool inSecondaryQuote,bool forSpeakerIdentification,int &lastGenderedAge,bool isPhysicallyPresent)
{ LFS
  wstring tmpstr;
  int highestDefinitelyIdentifiedEncounters=-1,highestIdentifiedEncounters=-1,highestEncounters=-1;
  if (!getHighestEncounters(highestDefinitelyIdentifiedEncounters,highestIdentifiedEncounters,highestEncounters)) return;
	// the minimum age of any object which agrees with the 'object' in gender
  vector <cObject>::iterator o=objects.end();
  vector <cSpeakerGroup>::iterator sg=speakerGroups.begin()+currentSpeakerGroup;
	vector <cSpeakerGroup>::iterator esg=(((unsigned)currentSpeakerGroup)<speakerGroups.size() && currentEmbeddedSpeakerGroup>=0 && 
		((unsigned)currentEmbeddedSpeakerGroup)<sg->embeddedSpeakerGroups.size()) ? sg->embeddedSpeakerGroups.begin()+currentEmbeddedSpeakerGroup : sgNULL;
  bool plural,male,female,neuter;
  bool matchMale=false,matchFemale=false,matchNeuter=false,matchPlural=false;
	bool traceThisNym=debugTrace.traceNyms && where==56245;
  if (object>=0)
  {
    o=objects.begin()+object;
    matchPlural=plural=o->plural;
    matchMale=male=o->male;
    matchFemale=female=o->female;
    matchNeuter=neuter=o->neuter;
		if (objects[object].objectClass==BODY_OBJECT_CLASS) 
			getOwnerSex(o->getOwnerWhere(),matchMale,matchFemale,matchNeuter,matchPlural);
  }
  else
  {
    int inflectionFlags=m[where].word->second.inflectionFlags; // only valid if object<0
    plural=(inflectionFlags&PLURAL_OWNER)==PLURAL_OWNER;
    if (m[where].queryWinnerForm(PROPER_NOUN_FORM_NUM)>=0 || (m[where].flags&cWordMatch::flagFirstLetterCapitalized)!=0 ||
      (inflectionFlags&(MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED))==0)
    {
      matchMale=male=(inflectionFlags&(MALE_GENDER|MALE_GENDER_ONLY_CAPITALIZED))!=0;
      matchFemale=female=(inflectionFlags&(FEMALE_GENDER|FEMALE_GENDER_ONLY_CAPITALIZED))!=0;
    }
    else
      male=female=false;
    neuter=(inflectionFlags&NEUTER_GENDER)==NEUTER_GENDER;
  }
	if (traceThisNym && object>=0)
	{
		wstring tmpstr2,tmpstr3;
		lplog(LOG_RESOLUTION,L"%06d:***** Object %s has associatedAdjectives (%s) associatedNouns (%s)",where,
			objectString(object,tmpstr,false).c_str(),wordString(objects[object].associatedAdjectives,tmpstr2).c_str(),wordString(objects[object].associatedNouns,tmpstr3).c_str());
	}
	lastGenderedAge=(neuter || plural) ? 0 : 6;
	// if adjectival object, use the associated adjectives of its principalObject if a GENDERED_BODY_OBJECT
	// associate any co-located adjectives / his[irish] rich Irish voice[irish] was unmistakable:
	vector <cObject>::iterator oColocate=o;
	if (object<0)
	{
		int beginPosition=where;
		while (beginPosition>0 && m[beginPosition].principalWherePosition<0) beginPosition--;
		if (m[beginPosition].principalWherePosition)
		{
			oColocate=objects.begin()+m[m[beginPosition].principalWherePosition].getObject();
			if (oColocate->objectClass!=BODY_OBJECT_CLASS)
				oColocate=objects.end();
			if (traceThisNym && oColocate!=objects.end())
			{
				wstring assa,assn;
				lplog(LOG_RESOLUTION,L"%06d:***** Colocated body object %s has associatedAdjectives (%s) associatedNouns (%s)",where,
					objectString(oColocate,tmpstr,false).c_str(),wordString(oColocate->associatedAdjectives,assa).c_str(),wordString(oColocate->associatedNouns,assn).c_str());
			}
		}
	}
  vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end(),unambiguousSexMatch=localObjects.end();
	vector < vector <cLocalFocus>::iterator > urps;
	tIWMM fromMatch,toMatch,toMapMatch;
	wstring logMatch;
  int numUnambiguousSexMatch=0;
	vector <tIWMM> marked;
  for (; lsi!=lsEnd; lsi++)
  {
		/* syn track */
		if (traceThisNym)
		{
			wstring assa,assn;
			lplog(LOG_RESOLUTION,L"%06d: Object %s has associatedAdjectives (%s) associatedNouns (%s)",where,objectString(lsi->om.object,tmpstr,false).c_str(),
				wordString(objects[lsi->om.object].associatedAdjectives,assa).c_str(),wordString(objects[lsi->om.object].associatedNouns,assn).c_str());
		}
    if (!lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge)) continue; // GO_NEUTRAL
		lsi->res.reserve(240);
    vector <cObject>::iterator lso=objects.begin()+lsi->om.object;
		// if something is a gendered body object, and its owner is not in salience, then the body object is not in salience either.
    if (lso->objectClass==BODY_OBJECT_CLASS && lso->getOwnerWhere()>=0 && !neuter)
		{
			vector <cLocalFocus>::iterator lsbo=localObjects.end();
			if (m[lso->getOwnerWhere()].objectMatches.size()==1) lsbo=in(m[lso->getOwnerWhere()].objectMatches[0].object);
			if (m[lso->getOwnerWhere()].objectMatches.empty()) lsbo=in(m[lso->getOwnerWhere()].getObject());
			if (lsbo!=localObjects.end() && !lsbo->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge))
				continue;
		}
    bool localMale=lso->male,localFemale=lso->female,localNeuter=lso->neuter,localPlural;
    if (lso->objectClass==BODY_OBJECT_CLASS)
      getOwnerSex(lso->getOwnerWhere(),localMale,localFemale,localNeuter,localPlural);
		if (lso->getSubType()>=0 && !lso->numIdentifiedAsSpeaker && !lso->numDefinitelyIdentifiedAsSpeaker && !lso->PISSubject && !lso->PISHail && !lso->PISDefinite)
		{
			localMale=localFemale=false;
			localNeuter=true;
			lsi->res+=L"[PLACENEUTER]";
		}
		// MPLURAL negation
		// if an object is not part of a group according to speaker groups, ignore whether they are in MPLURAL or not.
		bool ignoreMPlural=currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].singularSpeakers.find(lsi->om.object)!=speakerGroups[currentSpeakerGroup].singularSpeakers.end();
		bool ignorePlural=false,singularBodyPart;
		// if an object has a word order adjective (the first man) allow it to match against a plural (two men)
		if ((object>=0 && o->getOwnerWhere()<-1) || (lsi->lastWhere>=0 && m[lsi->lastWhere].word->first==L"all")) ignoreMPlural=ignorePlural=true;
		bool allowedPlural=(object<0 || o->objectClass!=BODY_OBJECT_CLASS) && plural; // don't count body objects (his two arms) as representing a plural object
    if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
    {
      if ((object>=0 && overlaps(o,lso)) || (object<0 && where<lso->end && lso->begin<where)) 
					itos(L"-OVERLAP[-",DISALLOW_SALIENCE,lsi->res,L"]");
      if (lso->plural && !ignorePlural) lsi->res+=(allowedPlural) ? L"+PLURAL[1000]" : L"-PLURAL[-10000]";
      else if (lsi->lastWhere>=0 && !ignoreMPlural)
      {
				int lwo;
        if ((m[lsi->lastWhere].objectRole&MPLURAL_ROLE))
          lsi->res+=((allowedPlural) ? L"+MPLURAL[600]" : L"-MPLURAL[-500]")+wstring(L"@")+itos(lsi->lastWhere,tmpstr);
        // object is single, the last matched is plural and 
				// object is not a plural body part (her eyebrows)
				// object does not equal last matched (if first name has plural alternate meaning - Tuppence)
        // and objectMatches>1
        else if ((m[lsi->lastWhere].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER)) &&
					       ((lwo=m[lsi->lastWhere].getObject())<0 || objects[lwo].objectClass!=BODY_OBJECT_CLASS || !isExternalBodyPart(lsi->lastWhere,singularBodyPart,true) || singularBodyPart) &&
                  lwo!=lsi->om.object && m[lsi->lastWhere].objectMatches.size()>1)
          lsi->res+=((allowedPlural) ? L"+PPLURAL[600]" : L"-PPLURAL[-500]")+wstring(L"@")+itos(lsi->lastWhere,tmpstr);
        if (lsi->previousWhere>=0)
        {
          // don't let it go negative, previousAge should only enhance chances of being picked
          if (m[lsi->previousWhere].objectRole&MPLURAL_ROLE)
            lsi->res+=((allowedPlural) ? L"+MPLURAL[600]" : L"-MPLURAL[0]")+wstring(L"@")+itos(lsi->previousWhere,tmpstr);
          else if ((m[lsi->previousWhere].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER)) &&
                  m[lsi->previousWhere].getObject()!=lsi->om.object && m[lsi->previousWhere].objectMatches.size()>1)
            lsi->res+=((allowedPlural) ? L"+PPLURAL[600]" : L"-PPLURAL[0]")+wstring(L"@")+itos(lsi->previousWhere,tmpstr);
        }
      }
			// if speaker groups formed, speakers <=2 (to make sure that one is not talking about the other aside), and talking about someone else outside the group
			// but the current localObject is inside the current speaker group, then discourage this match heavily.
			// in primary quotes, the person may be talking about themselves, so DON'T widen this to all gendered object classes (1659:wardmaid) (8020:My dear girl)
			if (inPrimaryQuote && currentSpeakerGroup<speakerGroups.size() && sg->speakers.size()<=2 && 
			   (((object>=0 && o->objectClass==PRONOUN_OBJECT_CLASS) || object<0) && (m[where].word->second.inflectionFlags&THIRD_PERSON)) &&
					(sg->speakers.find(lsi->om.object)!=sg->speakers.end() || matchAliases(where,lsi->om.object,sg->speakers)) &&
          (!(m[lastOpeningPrimaryQuote].flags&cWordMatch::flagQuoteContainsSpeaker) || !in(lsi->om.object,m[lastOpeningPrimaryQuote].getRelObject())))
				lsi->res+=L"-3rdPersonInSPGroup[10000]";
			if (inSecondaryQuote && esg!=sgNULL && esg->speakers.size()<=2 &&
			   ((((object>=0 && o->objectClass==PRONOUN_OBJECT_CLASS) || object<0) && (m[where].word->second.inflectionFlags&THIRD_PERSON)) || 
				     o->objectClass==GENDERED_GENERAL_OBJECT_CLASS || o->objectClass==BODY_OBJECT_CLASS || o->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || o->objectClass==GENDERED_DEMONYM_OBJECT_CLASS) &&
					(esg->speakers.find(lsi->om.object)!=esg->speakers.end() || matchAliases(where,lsi->om.object,esg->speakers)))
				lsi->res+=L"-3rdPersonInESPGroup[10000]";
      // if either male OR female but not both
      if ((localMale ^ localFemale) && !(matchMale && matchFemale))
      {
        if (localMale)
        {
          if (matchMale)
            lsi->res+=L"+M[400]";
          else if (matchFemale)
            itos(L"-F[-",MISMATCHING_SEX,lsi->res,L"]");
        }
        if (localFemale)
        {
          if (matchFemale)
            lsi->res+=L"+F[400]";
          else if (matchMale)
            itos(L"-M[-",MISMATCHING_SEX,lsi->res,L"]");
        }
      }
      if (neuter && !(matchFemale || matchMale))
      {
        // if gendered body object a reference by 'it' may be acceptable
        // the man's voice... it...
        if (lso->objectClass!=BODY_OBJECT_CLASS)
        {
          if (localMale || localFemale)
            itos(L"-GENDERED[",MISMATCHING_SEX,lsi->res,L"]");
          else if (!forSpeakerIdentification)
            lsi->res+=L"+N[500]";
        }
        else if (forSpeakerIdentification)
          lsi->res+=L"+N[500]";
      }
      if (localNeuter && !localMale && !localFemale && !neuter && (male || female))
        itos(L"-N[",MISMATCHING_SEX,lsi->res,L"]");
      if (forSpeakerIdentification)
      {
        if (highestDefinitelyIdentifiedEncounters)
          itos(L"+DIS[",SALIENCE_PER_SPEAKER_ENCOUNTER*min(lsi->numDefinitelyIdentifiedAsSpeaker,MAX_ENCOUNTERS)/highestDefinitelyIdentifiedEncounters,lsi->res,L"]");
			}
      if (highestIdentifiedEncounters)
        itos(L"+IdEn[",SALIENCE_PER_IDENTIFIED_ENCOUNTER*min(lsi->numIdentifiedAsSpeaker,MAX_ENCOUNTERS)/highestIdentifiedEncounters,lsi->res,L"]");
      if (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>0)
        itos(L"*Age",lsi->rAge(false,forSpeakerIdentification,objectToBeMatchedInQuote,quoteIndependentAge),lsi->res,L"%%");
      if (lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>0)
        itos(L"+*Age",lsi->rAge(true,forSpeakerIdentification,objectToBeMatchedInQuote,quoteIndependentAge),lsi->res,L"%%");
		// if the localObject OR the last object it matched to is unresolvable (29036:Mrs. Vandermeyer's cook)
		bool unresolvableByImplicit=false;
    if ((unResolvablePosition(lso->begin) || (unresolvableByImplicit=(lsi->lastWhere>=0 && unResolvablePosition(m[lsi->lastWhere].beginObjectPosition)))) && 
			  (!isPhysicallyPresent || lsi->physicallyPresent) && // object to be matched is not necessarily physically present OR object matched is physically present
			  (!forSpeakerIdentification || !lsi->notSpeaker) &&
				//((lso->objectClass==NAME_OBJECT_CLASS && lsi->lastWhere>=0 && (m[lsi->lastWhere].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE)) ||
         (lso->objectClass==GENDERED_GENERAL_OBJECT_CLASS || 
				 lso->objectClass==BODY_OBJECT_CLASS || 
				 lso->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
				 lso->objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
				 lso->objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
				 lso->objectClass==META_GROUP_OBJECT_CLASS) && // first man, after being set as unresolvable by an implicit object (22395)
        (unresolvableByImplicit || abs(where-lso->originalLocation)<80))
      {
        int ageInSpeakerGroup=0;
				if (!unresolvableByImplicit)
				{
					if (currentSpeakerGroup<speakerGroups.size())
					{
							int begin=lso->begin; // not from the beginning of the speaker group, because that will include cataphoric matches
							for (vector <cObject::cLocation>::iterator li=lso->locations.begin(),liEnd=lso->locations.end(); li!=liEnd; li++)
							if (li->at>=begin && li->at<where)
								ageInSpeakerGroup++;
							if (ageInSpeakerGroup) ageInSpeakerGroup--;
					}
				}
        switch (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)+ageInSpeakerGroup)
        {
        case 0:itos(L"+NONRES[",2000,lsi->res,L"]"); break;
        case 1:itos(L"+NONRES[",1500,lsi->res,L"]"); break;
        case 2:itos(L"+NONRES[",500,lsi->res,L"]"); break;
				default:itos(L" NONRES[AGE=",lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)+ageInSpeakerGroup,lsi->res,L"]"); break;
        }
      }
      if (highestEncounters)
        itos(L"+En[",SALIENCE_PER_ENCOUNTER*min(lsi->numEncounters,MAX_ENCOUNTERS)/highestEncounters,lsi->res,L"]");
      if (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>3)
        itos(L"*EnAge",lsi->rAge(false,false,objectToBeMatchedInQuote,quoteIndependentAge),lsi->res,L"%%");
      if (lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>=0)
        itos(L"+*EnAge",lsi->rAge(true,false,objectToBeMatchedInQuote,quoteIndependentAge),lsi->res,L"%%");
      // consider the most recent object mentioned in the current sentence?
			itos(L"(AGE ",lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge),lsi->res,L" ");
      if (neuter && !(male || female))
        itos(L"",(1-((lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>5) ? 5 : lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)))*500,lsi->res,L")"); // -2500 to +500
      else
        itos(L"",(3-((lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>15) ? 15 : lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)))*200,lsi->res,L")"); // -2400 to +600
      if (lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>=0)
      {
				itos(L"(+AGE ",lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge),lsi->res,L" ");
        // don't let it go negative, previousAge should only enhance chances of being picked
        if (neuter && !(male || female))
          itos(L"",(1-((lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>1) ? 1 : lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)))*500,lsi->res,L")"); // 0 to +500
        else
          itos(L"",(3-((lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>3) ? 3 : lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)))*200,lsi->res,L")"); // 0 to +600
      }
      if (lso->objectClass==PRONOUN_OBJECT_CLASS || lso->objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS || lso->objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS)
        itos(L"-PRO[-",DISALLOW_SALIENCE,lsi->res,L"]");
    }
    if (object>=0)
      if (overlaps(o,lso)) lsi->om.salienceFactor-=DISALLOW_SALIENCE;
    if (lso->plural && !ignorePlural)
      lsi->om.salienceFactor+=(allowedPlural) ? 1000 : -10000;
    else if (lsi->lastWhere>=0 && !ignoreMPlural)
    {
      if ((m[lsi->lastWhere].objectRole&MPLURAL_ROLE))
        lsi->om.salienceFactor+=(allowedPlural) ? 600 : -500;
      else if ((m[lsi->lastWhere].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER)) &&
              m[lsi->lastWhere].getObject()!=lsi->om.object && m[lsi->lastWhere].objectMatches.size()>1)
        lsi->om.salienceFactor+=(allowedPlural) ? 600 : -500;
      if (lsi->previousWhere>=0)
      {
        // don't let it go negative, previousAge should only enhance chances of being picked
        if (m[lsi->previousWhere].objectRole&MPLURAL_ROLE)
          lsi->om.salienceFactor+=(allowedPlural) ? 600 : 0;
        else if ((m[lsi->previousWhere].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER)) &&
            m[lsi->previousWhere].getObject()!=lsi->om.object && m[lsi->previousWhere].objectMatches.size()>1)
          lsi->om.salienceFactor+=(allowedPlural) ? 600 : 0;
      }
    }
    if (inPrimaryQuote && currentSpeakerGroup<speakerGroups.size() && sg->speakers.size()<=2 && 
			  ((object>=0 && o->objectClass==PRONOUN_OBJECT_CLASS) || object<0) && 
			  (m[where].word->second.inflectionFlags&THIRD_PERSON) && 
				(sg->speakers.find(lsi->om.object)!=sg->speakers.end() || matchAliases(where,lsi->om.object,sg->speakers)) &&
        (!(m[lastOpeningPrimaryQuote].flags&cWordMatch::flagQuoteContainsSpeaker) || !in(lsi->om.object,m[lastOpeningPrimaryQuote].getRelObject())))
			lsi->om.salienceFactor-=10000;
		if (inSecondaryQuote && esg!=sgNULL && esg->speakers.size()<=2 &&
		   ((((object>=0 && o->objectClass==PRONOUN_OBJECT_CLASS) || object<0) && (m[where].word->second.inflectionFlags&THIRD_PERSON)) || 
			     o->objectClass==GENDERED_GENERAL_OBJECT_CLASS || o->objectClass==BODY_OBJECT_CLASS || o->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || o->objectClass==GENDERED_DEMONYM_OBJECT_CLASS) &&
			(esg->speakers.find(lsi->om.object)!=esg->speakers.end() || matchAliases(where,lsi->om.object,esg->speakers)))
			lsi->om.salienceFactor-=10000;
    int sexFactor=0;
    // if either male OR female but not both
    if ((localMale ^ localFemale) && !(matchMale && matchFemale))
    {
      if (localMale)
      {
        if (matchMale)
        {
          sexFactor+=SALIENCE_THRESHOLD;
          unambiguousSexMatch=lsi;
          numUnambiguousSexMatch++;

        }
        else if (matchFemale)
          sexFactor-=MISMATCHING_SEX;
      }
      if (localFemale)
      {
        if (matchFemale)
        {
          sexFactor+=SALIENCE_THRESHOLD;
          unambiguousSexMatch=lsi;
          numUnambiguousSexMatch++;
        }
        else if (matchMale)
          sexFactor-=MISMATCHING_SEX;
      }
    }
    if (neuter && !(matchFemale || matchMale))
    {
      if (lso->objectClass!=BODY_OBJECT_CLASS)
      {
        if (localMale || localFemale)
          sexFactor-=MISMATCHING_SEX;
        else if (!forSpeakerIdentification)
          sexFactor+=500;
      }
      else if (forSpeakerIdentification)
        sexFactor+=500;
    }
    if (localNeuter && !localMale && !localFemale && !neuter && (male || female))
      sexFactor-=MISMATCHING_SEX;
		if (sexFactor>=0)
		{
			// lastGenderedAge is used in adjustSaliencesByGenderNumberAndOccurrence
			// it is used in cases where we want to resolve against a gendered object, but no such object
			// has been mentioned in the text for a long time, leading to match failure.  lastGenderedAge
			// adjusts the age of each gendered object by the last appearance of any gendered object,
			// thus putting all the objects that could match firmly within salience.
			// in addition, if this is for speaker identification, we don't count any gendered objects that
			// have not shown up as speakers before.  This doesn't mean they couldn't be picked anyway,
			// but it doesn't give non-speaker objects an advantage.
			if (!forSpeakerIdentification || 
				  (currentSpeakerGroup<speakerGroups.size() && (lso->numDefinitelyIdentifiedAsSpeaker || lso->numIdentifiedAsSpeaker)) ||
				  (currentSpeakerGroup>=speakerGroups.size() && (lso->PISSubject || lso->PISDefinite)))
				lastGenderedAge=min(lastGenderedAge,lsi->getTotalAge());
			if (oColocate!=objects.end())
			{
				if (nymNoMatch(where,oColocate,lso,false,traceThisNym,logMatch,fromMatch,toMatch,toMapMatch,L"NoMatch"))
				{
					lsi->om.salienceFactor-=DISALLOW_SALIENCE;
					lsi->numMatchedAdjectives=-1;
					// A. one object is 'young' having synonyms 'little' 'young' and antonyms 'old'
					// B. one object is 'old' having synonyms 'aged' 'old' and antonyms 'young'
					// C. one other object is 'big' having synonyms 'astronomic' 'big' and antonyms 'little'
					// so A&B are truly opposites: A's synonyms and B's antonyms AND A's antonyms and B's synonyms have common members
					//    A&C should be ignored: A's synonyms and B's antonyms are common but NOT A's antonyms and B's synonyms
					if (debugTrace.traceSpeakerResolution)
						itos(L"-NYM[-",DISALLOW_SALIENCE,lsi->res,logMatch+L"]");
				}
				else 
				{
					bool explicitOccupationMatch=false;
					lsi->numMatchedAdjectives=nymMatch(oColocate,lso,false,traceThisNym,explicitOccupationMatch,logMatch,fromMatch,toMatch,toMapMatch,L"Match");
					if (debugTrace.traceSpeakerResolution && lsi->numMatchedAdjectives)
						itos(L"+NYM[+",lsi->numMatchedAdjectives,lsi->res,logMatch+L"]");
				}
			}
		}
    if (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>3)
      sexFactor=sexFactor*lsi->allAge(false,objectToBeMatchedInQuote,quoteIndependentAge)/100;
    lsi->om.salienceFactor+=sexFactor;
    if (forSpeakerIdentification)
    {
      int ageSpeaker=0;
      if (highestDefinitelyIdentifiedEncounters && forSpeakerIdentification)
        ageSpeaker+=SALIENCE_PER_SPEAKER_ENCOUNTER*min(lsi->numDefinitelyIdentifiedAsSpeaker,MAX_ENCOUNTERS)/highestDefinitelyIdentifiedEncounters;
      if (highestIdentifiedEncounters)
        ageSpeaker+=SALIENCE_PER_IDENTIFIED_ENCOUNTER*min(lsi->numIdentifiedAsSpeaker,MAX_ENCOUNTERS)/highestIdentifiedEncounters;
      ageSpeaker=ageSpeaker*lsi->allAge(true,objectToBeMatchedInQuote,quoteIndependentAge)/100;
      lsi->om.salienceFactor+=ageSpeaker;
    }
    else
    {
      int ageIdEncounter=0;
      if (highestIdentifiedEncounters)
        ageIdEncounter+=SALIENCE_PER_IDENTIFIED_ENCOUNTER*min(lsi->numIdentifiedAsSpeaker,MAX_ENCOUNTERS)/highestIdentifiedEncounters;
      ageIdEncounter=ageIdEncounter*(lsi->allAge(false,objectToBeMatchedInQuote,quoteIndependentAge))/100;
      lsi->om.salienceFactor+=ageIdEncounter;
    }
		// if the localObject OR the last object it matched to is unresolvable (29036:Mrs. Vandermeyer's cook)
		lsi->newPPAge=-1;
		bool unresolvableByImplicit=false;
    if ((unResolvablePosition(lso->begin) || 
			   (unresolvableByImplicit=(lsi->lastWhere>=0 && m[lsi->lastWhere].beginObjectPosition>=0 && unResolvablePosition(m[lsi->lastWhere].beginObjectPosition)))) && 
			  (!isPhysicallyPresent || lsi->physicallyPresent) && // object to be matched is not necessarily physically present OR object matched is physically present
			  (!forSpeakerIdentification || !lsi->notSpeaker) &&
				//((lso->objectClass==NAME_OBJECT_CLASS && lsi->lastWhere>=0 && (m[lsi->lastWhere].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE)) ||
         (lso->objectClass==GENDERED_GENERAL_OBJECT_CLASS || 
				 lso->objectClass==BODY_OBJECT_CLASS || 
				 lso->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
				 lso->objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
				 lso->objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
				 lso->objectClass==META_GROUP_OBJECT_CLASS) && // first man, after being set as unresolvable by an implicit object (22395)
        (unresolvableByImplicit || abs(where-lso->originalLocation)<80))
    {
			urps.push_back(lsi);
			int ageInSpeakerGroup=0;
			if (!unresolvableByImplicit && currentSpeakerGroup<speakerGroups.size())
			{
				int begin=lso->begin; // not from the beginning of the speaker group, because that will include cataphoric matches
				for (vector <cObject::cLocation>::iterator li=lso->locations.begin(),liEnd=lso->locations.end(); li!=liEnd; li++)
					if (li->at>=begin && li->at<where)
						ageInSpeakerGroup++;
				if (ageInSpeakerGroup) ageInSpeakerGroup--;
			}
      switch (lsi->newPPAge=lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)+ageInSpeakerGroup)
      {
      case 0:lsi->om.salienceFactor+=2000; break;
      case 1:lsi->om.salienceFactor+=1500; break;
      case 2:lsi->om.salienceFactor+=500; break;
      }
    }
    int ageEncounter=0;
    if (highestEncounters) ageEncounter+=SALIENCE_PER_ENCOUNTER*min(lsi->numEncounters,MAX_ENCOUNTERS)/highestEncounters;
    if (lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>3) ageEncounter=ageEncounter*(lsi->allAge(false,objectToBeMatchedInQuote,quoteIndependentAge))/100;
    lsi->om.salienceFactor+=ageEncounter;
    // consider the most recent object mentioned in the current sentence?
    if (neuter && !(male || female))
      lsi->om.salienceFactor+=(1-((lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>5) ? 5 : lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)))*500; // -2500 to +500
    else
      lsi->om.salienceFactor+=(3-((lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>15) ? 15 : lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)))*200; // -2400 to +600
    if (lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>=0)
    {
      // don't let it go negative, previousAge should only enhance chances of being picked
      if (neuter && !(male || female))
        lsi->om.salienceFactor+=(1-((lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>1) ? 1 : lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)))*500; // 0 to +500
      else
        lsi->om.salienceFactor+=(3-((lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)>3) ? 3 : lsi->getAge(true,objectToBeMatchedInQuote,quoteIndependentAge)))*200; // 0 to +600
    }
    if (lso->objectClass==PRONOUN_OBJECT_CLASS ||
      lso->objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS ||
      lso->objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS)
      lsi->om.salienceFactor-=DISALLOW_SALIENCE; // don't match a pronoun with another pronoun - these will be matched cataphorically
  }
  if ((forSpeakerIdentification || (m[where].objectRole&HAIL_ROLE))  && currentSpeakerGroup<speakerGroups.size())
  {
		// embedded speakers don't have enough context to set numDefinitelyIdentifiedAsSpeaker (marked **)
		// ‘[man:julius] I[man] think that[growing,last] shall do for the present , **sister** , ’ said the little man[man]
    bool detractNonSpeakers=(m[where].objectRole&(IN_EMBEDDED_STORY_OBJECT_ROLE|IN_SECONDARY_QUOTE_ROLE))==(IN_EMBEDDED_STORY_OBJECT_ROLE|IN_SECONDARY_QUOTE_ROLE);
    for (lsi=localObjects.begin(); lsi!=lsEnd && !detractNonSpeakers; lsi++)
      if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>1000 && lsi->numDefinitelyIdentifiedAsSpeaker)
        detractNonSpeakers=true;
    if (detractNonSpeakers)
      for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
			{
				if (lsi->notSpeaker)
				{
					lsi->om.salienceFactor-=DISALLOW_SALIENCE;
					if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
						itos(L"-NOTSPEAKER[-",DISALLOW_SALIENCE,lsi->res,L"]");
				}
				if ((m[where].objectRole&(IN_EMBEDDED_STORY_OBJECT_ROLE|IN_SECONDARY_QUOTE_ROLE))==(IN_EMBEDDED_STORY_OBJECT_ROLE|IN_SECONDARY_QUOTE_ROLE))
				{ 
					if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && currentEmbeddedSpeakerGroup>=0 &&
							speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.find(lsi->om.object)==speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.end())
					{
						lsi->om.salienceFactor-=DISALLOW_SALIENCE;
						if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
							itos(L"-NOTEMBEDDEDSPEAKER[-",DISALLOW_SALIENCE,lsi->res,L"]");
					}
				}
				else
				{
					if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && !lsi->numDefinitelyIdentifiedAsSpeaker &&
							speakerGroups[currentSpeakerGroup].speakers.find(lsi->om.object)==speakerGroups[currentSpeakerGroup].speakers.end())
					{
						lsi->om.salienceFactor-=DISALLOW_SALIENCE;
						if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
							itos(L"-NOTSPEAKER[-",DISALLOW_SALIENCE,lsi->res,L"]");
					}
				}
			}
		// if we have previously known speaker, boost that.
		/*
		if ((m[where].objectRole&HAIL_ROLE) && previousSpeakers.size()==1 && (lsi=in(previousSpeakers[0]))!=localObjects.end() &&
			   currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.size()==2)
    {
      lsi->om.salienceFactor+=PS_SALIENCE;
      if (t.traceObjectResolution || t.traceSpeakerResolution)
        lsi->res+=L"+PREVIOUS_SPEAKER[+"+itos(PS_SALIENCE,tmpstr)+L"]";
    }
		*/
    if ((male ^ female) && !neuter && numUnambiguousSexMatch==1)
    {
      if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
        unambiguousSexMatch->res+=L"+UNAMBIGSM[1000]";
      unambiguousSexMatch->om.salienceFactor+=1000;
    }
  }
	if (urps.size()>1)
	{
		vector <cLocalFocus>::iterator highestUrp=urps[0],nearestUrp=urps[0],preferUrp;
		for (unsigned int I=1; I<urps.size(); I++) 
		{
			if (highestUrp->om.salienceFactor<urps[I]->om.salienceFactor)
				highestUrp=urps[I];
			if (nearestUrp->lastWhere<urps[I]->lastWhere)
				nearestUrp=urps[I];
		}
    if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:multiple unresolvable objects - highest is %s nearest is %s",where,objectString(highestUrp->om.object,tmpstr,false).c_str(),objectString(nearestUrp->om.object,tmpstr,false).c_str());
		// prefer nearest, unless highest is *2 more
		preferUrp=((highestUrp->om.salienceFactor>>1)>nearestUrp->om.salienceFactor) ? highestUrp : nearestUrp;
		for (unsigned int I=0; I<urps.size(); I++)
			if (urps[I]!=preferUrp && urps[I]->om.salienceFactor>(preferUrp->om.salienceFactor>>1))
			{
				wchar_t tmp[10];
				_itow(urps[I]->om.salienceFactor,tmp,10);
	      if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
		      urps[I]->res+=L"-FARTHER_URP[orig "+wstring(tmp)+L"]";
				urps[I]->om.salienceFactor=preferUrp->om.salienceFactor>>1;
			}
	}
}

bool cSource::disallowReference(int object,vector <int> &disallowedReferences)
{ LFS
	if (object>=0)
	{
		disallowedReferences.push_back(object);
		return true;
	}
	return false;
}

// scan before __S1 to interpret a possible _PP sequence.
void cSource::checkForPreviousPP(int where,vector <int> &disallowedReferences)
{ LFS
	if (where>=2 && ((m[where-1].objectRole&PREP_OBJECT_ROLE) || m[where-2].objectRole&PREP_OBJECT_ROLE))
	{
		wstring tmpstr;
		int I;
		for (I=(where>=2 && m[where-1].word->first==L",") ? where-2 : where-1; I>=0 && m[I].queryWinnerForm(prepositionForm)<0; I--);
		if (I>=0 && m[I].pma.queryPattern(L"_INTRO_S1")!=-1) // make sure this prep phrase is part of the next sentence
		{
			int begin=I;
			for (int J=(where>=2 && m[where-1].word->first==L",") ? where-2 : where-1; J>=begin; J--)
				if (m[J].getObject()>=0	&& disallowReference(m[J].getObject(),disallowedReferences) && debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d: L&L 2.1.1 ruled out predecessor prepositional object %s at %d",
									where,objectString(m[J].getObject(),tmpstr,false).c_str(),J);
		}
	}
}

// Lappin and Leass 2.1.4
// Salience Weighting
// Factor type Initial weight
// Sentence recency 100
// Subject emphasis 80
// Existential emphasis 70
// Accusative emphasis 50 - direct object (i.e., verbal complement in accusative case)
// Indirect object and oblique complement emphasis 40
// Head noun emphasis 80
// Non-adverbial emphasis 50
// These factors get recorded into the localObjects (qoutedLocalObjects) and reduced by half every sentence.
// roles are listed in the next procedure below (findRole)
// a noun is subordinated when it is the last noun in patterns
//   __NOUN"5", __NOUN"A" or under __ALLOBJECTS in pattern __NOUN"D"
int cSource::getRoleSalience(unsigned __int64 role)
{ LFS
  int sf=0; // (Deleted) [Every noun gets 100 to start (Sentence Recency)]
  if ((role&SUBJECT_ROLE) && !(role&PREP_OBJECT_ROLE)) sf+=80; // subject emphasis
  // existential emphasis not implemented
  if (role==OBJECT_ROLE+2) sf+=50; // accusative emphasis
  if (role==OBJECT_ROLE+5) sf+=40; // Indirect object and oblique complement emphasis
  if (!(role&SUBOBJECT_ROLE)) sf+=80; // Head noun emphasis
  if (role&META_NAME_EQUIVALENCE) sf+=200; // Indirect object and oblique complement emphasis
  // Non-adverbial emphasis not implemented (not sure about whether this emphasis works)
  return sf;
}

#define MAX_PARALLEL_ROLE_AGE 4
// this was changed from using fractions (2.5, 1.5) because displaying fractions was taking up 4% of the
// total execution time.
void cSource::localRoleBoost(vector <cLocalFocus>::iterator lsi,int I,unsigned __int64 objectRole,int age)
{ LFS
  int roleBoosts[MAX_PARALLEL_ROLE_AGE*3]={ 400, 250, 150, 130,  250, 170, 150, 110,   150, 130, 110, 100  };
  wchar_t *roleStrings[3]={ L"AGREE_SUBJ", L"AGREE_OBJ", L"AGREE_SUBOBJ" };
  unsigned __int64 matchingObjectRole=m[I].objectRole,roleBoost;
  //if ((matchingObjectRole&SUBJECT_ROLE) && !(matchingObjectRole&PREP_OBJECT_ROLE) && (objectRole&SUBJECT_ROLE) && !(objectRole&PREP_OBJECT_ROLE))
  //  roleBoost=0;
  //else if ((matchingObjectRole&OBJECT_ROLE) && (objectRole&OBJECT_ROLE))
  if ((matchingObjectRole&(SUBJECT_ROLE|NO_ALT_RES_SPEAKER_ROLE)) && !(matchingObjectRole&PREP_OBJECT_ROLE) && (objectRole&SUBJECT_ROLE) && !(objectRole&PREP_OBJECT_ROLE))
    roleBoost=0;
  else if ((matchingObjectRole&OBJECT_ROLE) && !(matchingObjectRole&NO_ALT_RES_SPEAKER_ROLE) && (objectRole&OBJECT_ROLE))
    roleBoost=1;
  else if ((matchingObjectRole==OBJECT_ROLE+5) && (objectRole==OBJECT_ROLE+5))
    roleBoost=2;
  else if ((matchingObjectRole&HAIL_ROLE) && (objectRole&SUBJECT_ROLE) && !(objectRole&PREP_OBJECT_ROLE))
    roleBoost=0;
  else
    return;
	lsi->om.salienceFactor=(int)((__int64)lsi->om.salienceFactor*roleBoosts[roleBoost*MAX_PARALLEL_ROLE_AGE+age]/100);
	//else
	//	lsi->om.salienceFactor=(int)(lsi->om.salienceFactor/min(roleBoosts[roleBoost*MAX_PARALLEL_ROLE_AGE+age],1.5));
  if (lsi->om.salienceFactor>=0 && (debugTrace.traceObjectResolution || debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution))
  {
    wstring tmpstr;
    lsi->res+=L"+" + wstring(roleStrings[roleBoost])+L"@"+itos(I,tmpstr) + L"[*" + itos(roleBoosts[roleBoost*MAX_PARALLEL_ROLE_AGE+age],tmpstr)+L"%%]";
  }
}

#define MAX_LAST_GENDERED_AGE 2
// set all localObjects with special flag
// search for all local objects in the last four sentences (age)
// if any found with the same role, multiplication factor is roleBoost[age]
// if forSpeakerIdentification==true, then if localRole does not have SUBJECT_ROLE, add it
//    this is to correct in situations like "said Jane", where Jane is really a subject, not object, for these purposes.
void cSource::adjustSaliencesByParallelRoleAndPlurality(int where,bool inPrimaryQuote,bool forSpeakerIdentification,int lastGenderedAge)
{ LFS
  vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end();
  for (; lsi!=lsEnd; lsi++)
		if (lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=0)
      objects[lsi->om.object].lsiOffset=lsi;
  int age=0,o=m[where].getObject(),tense=-1;
	if (m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].verbSense>=0) tense=m[m[where].getRelVerb()].verbSense;
	unsigned __int64 objectRole=m[where].objectRole;
	bool matchingToPlural=(o== cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL || (o>=0 && objects[o].plural)),localInPrimaryQuote=inPrimaryQuote,physicallyEvaluated,passedParagraph=false;
	if (forSpeakerIdentification) objectRole|=SUBJECT_ROLE;
	if (inPrimaryQuote && (m[where].objectRole&HAIL_ROLE)==HAIL_ROLE) inPrimaryQuote=false;
	//bool PE=false,PP=physicallyPresentPosition(where,PE); salienceNPP
  for (int I=where-1; I>=0 && age<MAX_PARALLEL_ROLE_AGE+lastGenderedAge; I--)
  {
		localInPrimaryQuote=((m[I].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0   && (m[I].objectRole&HAIL_ROLE)==0) ||
		                    ((m[I].objectRole&IN_SECONDARY_QUOTE_ROLE)!=0 && (m[I].objectRole&HAIL_ROLE)!=0);
    if (age>=MAX_PARALLEL_ROLE_AGE+lastGenderedAge) break;
		o=m[I].getObject();
		// don't allow subjects that have been matched against plurals like 'they' to be boosted against singular objects
		// o='he' m[I]=='they' - don't boost
		// Boris should match 'The emotions of Boris', where Boris matches 'emotions' and should be considered a singular match
		bool plural=(o== cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL || (o>=0 && objects[o].plural)),allowEitherPlural=(m[I].objectRole&MPLURAL_ROLE)!=0 || (o>=0 && objects[o].objectClass==BODY_OBJECT_CLASS && objects[o].plural);
		bool pluralMatch=(matchingToPlural==plural || allowEitherPlural);
		bool quoteMatch=(quoteIndependentAge || localInPrimaryQuote==inPrimaryQuote);
    lsi=cNULL;
    if ((o>=0 && (lsi=objects[o].lsiOffset)!=cNULL) || 
			  ((m[I].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE) && m[I].objectMatches.size() && (lsi=objects[m[I].objectMatches[0].object].lsiOffset)!=cNULL))
		{
			if (quoteMatch && pluralMatch)
				localRoleBoost(lsi,I,objectRole,age-min(lastGenderedAge,age));
			if (age<=1 && !passedParagraph && (objectRole&(SUBJECT_ROLE|PREP_OBJECT_ROLE|IN_PRIMARY_QUOTE_ROLE))==SUBJECT_ROLE && lsi->om.salienceFactor>=0)
			{
				unsigned __int64 matchingObjectRole=m[I].objectRole;
				if ((matchingObjectRole&(SUBJECT_ROLE|NO_ALT_RES_SPEAKER_ROLE)) && (matchingObjectRole&(PREP_OBJECT_ROLE|PRIMARY_SPEAKER_ROLE))==PRIMARY_SPEAKER_ROLE)
				{
					lsi->om.salienceFactor<<=2;
					if (debugTrace.traceObjectResolution || debugTrace.traceNameResolution || debugTrace.traceSpeakerResolution)
						lsi->res+=L"[SP*2]";
				}
			}
			// if anyone is competing against a matchable unresolvable object, don't allow previous
			//   mentions of another object to make that object override the unresolvable one.
			//   previously this was taken care of by just adding X to the unresolvable object but since
			//   an object may have an arbitrary # of mentions before the unresolvable object X may not be enough.
			// Also, make sure plural matches, because 'they' is neuter also and so otherwise this test will succeed with any
			// unresolvable object, when 'they' should match multiple gendered nouns (boris and whittington)
			//  they[matters,thoughts,tommy] spoke low and the din of the traffic drowned their[tommy,matters,thoughts] voices effectually . 
			bool tenseMatch=(m[I].getRelVerb()>=0 && m[m[I].getRelVerb()].verbSense>=0) ? m[m[I].getRelVerb()].verbSense==tense : false;
			/*
			bool lPP=physicallyPresentPosition(I,physicallyEvaluated); salienceNPP
			if (where==36533 || where==18103 || where==25439)
			{
				wstring sRole;
				lplog(LOG_RESOLUTION,L"%06d:%d %d %d %d pluralMatch=%s unResolvablePosition=%s lPP=%s PP=%s tenseMatch=%s lRole=%s",
							I,m[I].getRelVerb(),(m[I].getRelVerb()>=0)?m[m[I].getRelVerb()].getQuoteForwardLink():-1,tense,lsi->om.salienceFactor,
							(pluralMatch) ? L"true":L"false",
							(unResolvablePosition(m[I].beginObjectPosition)) ? L"true":L"false",
							(lPP) ? L"true" : L"false",
							(PP) ? L"true" : L"false",
							(tenseMatch) ? L"true":L"false",
							roleString(I,sRole).c_str());
			}
			*/
			if (lsi->om.salienceFactor>=0 && pluralMatch &&
				  unResolvablePosition(m[I].beginObjectPosition) && 
					(physicallyPresentPosition(I,physicallyEvaluated) || tenseMatch))// || 
					 //((m[I].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE) && objects[o].numDefinitelyIdentifiedAsSpeaker>0)))
					// be very careful about limiting this -- the object must have a great chance of being salient
					// this change introduced errors! salienceNPP
					//(((!PP && (!objects[o].neuter || (m[I].objectRole&(OBJECT_ROLE|SUBJECT_ROLE))==SUBJECT_ROLE)) || lPP) || tenseMatch))
			{
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr;
					lplog(LOG_RESOLUTION,L"%06d:parallel role and plurality boosting stopped at %d:%s (unresolvable position).",where,
						I,objectString(lsi->om,tmpstr,true).c_str());
				}
				break;
			}
		}
		if (quoteMatch && pluralMatch && 
			  (o<0 || objects[o].objectClass!=BODY_OBJECT_CLASS || objects[o].getOwnerWhere()<0))
			for (unsigned int omi=0; omi<m[I].objectMatches.size(); omi++)
		    if ((lsi=objects[m[I].objectMatches[omi].object].lsiOffset)!=cNULL && m[I].objectMatches[omi].object!=o)
				    localRoleBoost(lsi,I,objectRole,age-min(lastGenderedAge,age));
		if (m[I].word==Words.sectionWord)	
		{
			age++;
			passedParagraph=true;
		}
		if (quoteMatch)
		{
			if (m[I].pma.queryPattern(L"__S1")!=-1)
				age++;
			if (((m[I].word->first==L"." || m[I].word->first==L"?" || m[I].word->first==L"!") && m[I].beginPEMAPosition==-1)  || m[I].word->first==L":")//  || m[I].word==Words.sectionWord
			{
				// is there any __S1 in the sentence (backwards)?
				bool s1Seen=false,inPQ;
			  for (int bI=I-1; bI>=0; bI--)
				{
					inPQ=((m[bI].objectRole&IN_PRIMARY_QUOTE_ROLE)!=0   && (m[bI].objectRole&HAIL_ROLE)==0) ||
						   ((m[bI].objectRole&IN_SECONDARY_QUOTE_ROLE)!=0 && (m[bI].objectRole&HAIL_ROLE)!=0);
					if ((quoteIndependentAge || inPQ==inPrimaryQuote) &&
							((s1Seen=m[bI].pma.queryPattern(L"__S1")!=-1) || m[bI].word->first==L":" ||
							((m[bI].word->first==L"." || m[bI].word->first==L"?" || m[bI].word->first==L"!") && m[bI].beginPEMAPosition==-1))) //  || m[I].word==Words.sectionWord
						break;
					if (m[bI].word==Words.sectionWord)
						break;
				}				
				if (!s1Seen)
					age++;
			}
		}
  }
  for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
    objects[lsi->om.object].lsiOffset=cNULL;
}

// 3 cases:
// 1. both names
// 2. one name and one gendered object or ogen w/body part
// 3. both gendered objects
// if o1 is a NAME it cannot match o2 if o2 is NOT a name and if o1 occurs BEFORE o2 AND o2 is a nonResolvable object
bool cSource::potentiallyMergable(int where,vector <cObject>::iterator o1,vector <cObject>::iterator o2,bool allowBothToBeSpeakers,bool checkUnmergableSpeaker)
{ LFS
  if (o1==o2 || matchAliases(where,(int)(o1-objects.begin()),(int)(o2-objects.begin())))
    return true;
	bool switched=false;
  if (switched=o1->objectClass==BODY_OBJECT_CLASS && o1->getOwnerWhere()>=0 && m[o1->getOwnerWhere()].getObject()>=0) o1=objects.begin()+m[o1->getOwnerWhere()].getObject();
  if (o2->objectClass==BODY_OBJECT_CLASS && o2->getOwnerWhere()>=0 && m[o2->getOwnerWhere()].getObject()>=0) 
	{
		switched=true;
		o2=objects.begin()+m[o2->getOwnerWhere()].getObject();
	}
  if (switched && (o1==o2 || matchAliases(where,(int)(o1-objects.begin()),(int)(o2-objects.begin()))))
    return true;
  bool o2IsNonResolvable=unResolvablePosition(o2->begin);
  if (o1->objectClass==NAME_OBJECT_CLASS && o2->objectClass==NAME_OBJECT_CLASS)
    return o1->plural==o2->plural && o1->exactGenderMatch(*o2) && o1->name.like(o2->name,debugTrace);
	// if o1 is a name, o2 is not a name (like a gendered object), the name has been in a speaker group which begins before the nonresolvable object,
	// then o1 and o2 are not compatible.
	// this is to take care of cases where the character is mentioned in passing but does not actually appear.  In these cases the first appearance of
	// the object can be before the unresolved object and that is still matchable, because the character has never physically appeared on the scene.
  if (o1->objectClass==NAME_OBJECT_CLASS && o2->objectClass!=NAME_OBJECT_CLASS &&
      o1->firstPhysicalManifestation!=-1 && o1->firstPhysicalManifestation<o2->begin && o2IsNonResolvable)
    return false;
	// a Naturalized German to Number One are not mergable
	// but if Number One was previously mentioned as being German, that is OK.
	// if Number One is mentioned as being anything other than German, that will fail the nymNoMatch below.
	if (o1->objectClass==GENDERED_DEMONYM_OBJECT_CLASS && o2->objectClass!=GENDERED_DEMONYM_OBJECT_CLASS && !hasDemonyms(o2))
		return false;
	if (o2->objectClass==GENDERED_DEMONYM_OBJECT_CLASS && o1->objectClass!=GENDERED_DEMONYM_OBJECT_CLASS && !hasDemonyms(o1))
		return false;
	tIWMM fromMatch,toMatch,toMapMatch;
	wstring logMatch;
  if ((o1->objectClass!=NAME_OBJECT_CLASS &&
       o1->objectClass!=GENDERED_GENERAL_OBJECT_CLASS &&
       o1->objectClass!=BODY_OBJECT_CLASS &&
       o1->objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
       o1->objectClass!=GENDERED_DEMONYM_OBJECT_CLASS &&
       o1->objectClass!=GENDERED_RELATIVE_OBJECT_CLASS &&
       o1->objectClass!=PRONOUN_OBJECT_CLASS) ||
      (o2->objectClass!=NAME_OBJECT_CLASS &&
       o2->objectClass!=GENDERED_GENERAL_OBJECT_CLASS &&
       o2->objectClass!=BODY_OBJECT_CLASS &&
       o2->objectClass!=GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
       o2->objectClass!=GENDERED_DEMONYM_OBJECT_CLASS &&
       o2->objectClass!=GENDERED_RELATIVE_OBJECT_CLASS &&
       o2->objectClass!=PRONOUN_OBJECT_CLASS))
		 return false;
  bool mergable=o1->plural==o2->plural && o1->matchGenderIncludingNeuter(*o2) && !nymNoMatch(-1,o1,o2,false,false,logMatch,fromMatch,toMatch,toMapMatch,L"NoMatchPM");
	wstring tmpstr,tmpstr2;
	// this is to verify that an unresolvable speaker in a previous speaker group is not merged with a future compatible object
	if (mergable && checkUnmergableSpeaker && o2IsNonResolvable)
	{
		int firstConsecutivelyLinkedSpeakerGroup=speakerGroups.size()-1;
		while (firstConsecutivelyLinkedSpeakerGroup>=0 && 
			     speakerGroups[firstConsecutivelyLinkedSpeakerGroup].speakers.find((int)(o2-objects.begin()))!=speakerGroups[firstConsecutivelyLinkedSpeakerGroup].speakers.end())
			firstConsecutivelyLinkedSpeakerGroup--;
		// if *mergedSpeakerObject, traced backwards consecutively, is not new for the first consecutive speakergroup
		// and has PISDefinite>0, disallow.
		if (firstConsecutivelyLinkedSpeakerGroup>=0 && o2->firstPhysicalManifestation<speakerGroups[firstConsecutivelyLinkedSpeakerGroup].sgEnd &&
				o2->PISDefinite)
		{
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s AND %s[%d>0,%d<%d] not mergable (previously established object).",where,
					objectString(o1,tmpstr,false).c_str(),
					objectString(o2,tmpstr2,false).c_str(),o2->PISDefinite,o2->firstPhysicalManifestation,speakerGroups[firstConsecutivelyLinkedSpeakerGroup].sgEnd);
			return false;
		}
	}
	if (mergable && o1->PISDefinite && o2->PISDefinite && o2IsNonResolvable)
	{
	  if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:%s[%d,%d] AND %s[%d,%d] not mergable (previous established speaker).",where,
			  objectString(o1,tmpstr,false).c_str(),o1->PISDefinite,o1->firstPhysicalManifestation,
				objectString(o2,tmpstr2,false).c_str(),o2->PISDefinite,o2->firstPhysicalManifestation);
		return allowBothToBeSpeakers;
	}
	return mergable;
}

vector <cOM>::iterator cSource::in(int o,vector <cOM> &objs)
{ LFS
  vector <cOM>::iterator omi= objs.end();
  for (omi= objs.begin(); omi!= objs.end() && o!=omi->object; omi++);
  return omi;
}

bool cSource::in(int o,int where)
{ LFS
	if (where<0) return false;
	if (m[where].getObject()==o) return true;
  vector <cOM>::iterator omi=m[where].objectMatches.end();
  for (omi=m[where].objectMatches.begin(); omi!=m[where].objectMatches.end() && o!=omi->object; omi++);
  return omi!=m[where].objectMatches.end();
}

vector <cLocalFocus>::iterator cSource::in(int o)
{ LFS
  vector <cLocalFocus>::iterator omi;
  for (omi=localObjects.begin(); omi!=localObjects.end() && o!=omi->om.object; omi++);
  return omi;
}

bool cSource::replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,wchar_t *fromWhat)
{ LFS
  vector <cOM>::iterator omi,omEnd=objectList.end(),keep;
  if ((omi=in(objectToBeReplaced,objectList))==omEnd) return false;
  keep=omi;
  if ((omi=in(replacementObject,objectList))==omEnd)
    keep->object=replacementObject;
  else
    objectList.erase(keep);
  if (debugTrace.traceObjectResolution)
  {
    wstring tmpstr;
    lplog(LOG_RESOLUTION,L"Replaced local focus %s with %s (%s)",objectString(objectToBeReplaced,tmpstr,true).c_str(),objectString(replacementObject,tmpstr,true).c_str(),fromWhat);
  }
	return true;
}

bool cSource::replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,vector <cObject::cLocation>::iterator ol)
{ LFS
  vector <cOM>::iterator omi,omEnd=objectList.end(),keep;
  if ((omi=in(objectToBeReplaced,objectList))==omEnd) return false;
  keep=omi;
  if ((omi=in(replacementObject,objectList))==omEnd)
  {
    keep->object=replacementObject;
    objects[replacementObject].locations.push_back(*ol);
    objects[replacementObject].updateFirstLocation(ol->at);
    if (debugTrace.traceNameResolution)
      lplog(LOG_RESOLUTION,L"  replaced AT %d.",ol->at);
  }
  else
  {
    objectList.erase(keep);
    if (debugTrace.traceNameResolution)
      lplog(LOG_RESOLUTION,L"  erased AT %d",ol->at);
  }
	return true;
}

bool cSource::replaceObject(int replacementObject,int objectToBeReplaced,vector <cOM> &objectList,int ol)
{ LFS
  vector <cOM>::iterator omi,omEnd=objectList.end(),keep;
  if ((omi=in(objectToBeReplaced,objectList))==omEnd) return false;
  keep=omi;
  if ((omi=in(replacementObject,objectList))==omEnd)
  {
    keep->object=replacementObject;
    objects[replacementObject].speakerLocations.insert(ol);
    if (debugTrace.traceNameResolution)
      lplog(LOG_RESOLUTION,L"  replaced AT %d.",ol);
  }
  else
  {
    objectList.erase(keep);
    if (debugTrace.traceNameResolution)
      lplog(LOG_RESOLUTION,L"  erased AT %d",ol);
  }
	return true;
}

void cSource::replaceObject(int replacementObject, int objectToBeReplaced,wchar_t *fromWhat)
{ LFS
  bool eraseNotReplace=false;
  vector <cLocalFocus>::iterator lsi;
  for (lsi=localObjects.begin(); lsi!=localObjects.end() && lsi->om.object!=replacementObject; lsi++);
  eraseNotReplace=(lsi!=localObjects.end());
  for (lsi=localObjects.begin(); lsi!=localObjects.end(); lsi++)
  {
    if (lsi->om.object==objectToBeReplaced)
    {
      if (debugTrace.traceObjectResolution || debugTrace.traceNameResolution)
      {
        wstring tmpstr;
        lplog(LOG_RESOLUTION,L"%06d:%s (%s) local speaker %s",objects[objectToBeReplaced].originalLocation,(eraseNotReplace) ? L"erased":L"replaced",fromWhat,objectString(objectToBeReplaced,tmpstr,false).c_str());
      }
      if (eraseNotReplace)
			{
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr;
					lplog(LOG_RESOLUTION,L"%06d:Removed %s from localObjects",objects[objectToBeReplaced].originalLocation,objectString(lsi->om.object,tmpstr,true).c_str());
				}
        localObjects.erase(lsi);
			}
      else
      {
        lsi->om.object=replacementObject;
        lsi->notSpeaker=false;
      }
      break;
    }
  }
}

bool cSource::replaceObject(int where,int replacementObject, int objectToBeReplaced,set <int> &objectSet, wchar_t *description, int begin, int end,wchar_t *fromWhat)
{ LFS
	if (replacementObject==objectToBeReplaced)
		return false;
  set <int>::iterator w,wEnd=objectSet.end();
  bool matchFound=(w=objectSet.find(objectToBeReplaced))!=wEnd;
	// check context sensitive object classes
	if (!matchFound && objects[objectToBeReplaced].objectClass==META_GROUP_OBJECT_CLASS)
		for (w=objectSet.begin(); w!=wEnd; w++)
			if (matchFound=matchAliases(where,objectToBeReplaced,*w)) 
				break;
	if (matchFound)
	{
		wstring tmpstr,tmpstr2;
		objectToBeReplaced=*w;
		objectSet.erase(w);
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG|LOG_RESOLUTION,L"RO %s [%d-%d] (%s) speaker %s replaced with %s",description,begin,end,fromWhat,objectString(objectToBeReplaced,tmpstr,false).c_str(),objectString(replacementObject,tmpstr2,false).c_str());
		objectSet.insert(replacementObject);
		// try again ( for metaname objects that have accumulated: his companion, his friend, the other, etc)
		replaceObject(where,replacementObject, objectToBeReplaced,objectSet, description, begin,end,fromWhat);
		return true;
	}
	return false;
}

bool cSource::replaceObject(int where,int replacementObject, int objectToBeReplaced,vector <int> &objectList, wchar_t *description, int begin, int end,wchar_t *fromWhat)
{ LFS
	if (replacementObject==objectToBeReplaced)
		return false;
  vector <int>::iterator w,wEnd=objectList.end();
  bool matchFound=(w=find(objectList.begin(),objectList.end(),objectToBeReplaced))!=wEnd;
	// check context sensitive object classes
	if (!matchFound && objects[objectToBeReplaced].objectClass==META_GROUP_OBJECT_CLASS)
		for (w=objectList.begin(); w!=wEnd; w++)
			if (matchFound=matchAliases(where,objectToBeReplaced,*w)) 
				break;
	if (matchFound)
	{
		wstring tmpstr,tmpstr2;
		objectToBeReplaced=*w;
		objectList.erase(w);
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_SG|LOG_RESOLUTION,L"RO %s [%d-%d] (%s) speaker %s replaced with %s",description,begin,end,fromWhat,objectString(objectToBeReplaced,tmpstr,false).c_str(),objectString(replacementObject,tmpstr2,false).c_str());
		objectList.push_back(replacementObject);
		// try again ( for metaname objects that have accumulated: his companion, his friend, the other, etc)
		replaceObject(where,replacementObject, objectToBeReplaced,objectList, description, begin,end,fromWhat);
		return true;
	}
	return false;
}

void cSource::moveNyms(int where,int toObject,int fromObject,wchar_t *fromWhere)
{ LFS
	wstring tmpstr,tmpstr2;
	// because body objects can be associated with people, only associate adjectives that are definitely 
	// associated only with people.  Other adjectives are only associated with the object 'big' eyes should not be associated with 'big' man
	// otherwise 'close' 'cropped' hair and 'dear' Boris will be associated with one another
	// but the 'Irish' voice and 'Irish' Sein Fiener should still become associated
	// do not associate nouns from body objects with anything except other things
	bool transferFromBodyObject=objects[fromObject].objectClass!=BODY_OBJECT_CLASS || (objects[toObject].neuter && !objects[toObject].male && !objects[toObject].female);
	if (objects[fromObject].objectClass!=META_GROUP_OBJECT_CLASS || (m[where].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE))
	{
		for (vector <tIWMM>::iterator si=objects[fromObject].associatedNouns.begin(),siEnd=objects[fromObject].associatedNouns.end(); si!=siEnd; si++)
			// if something is both a common profession and a body object (like 'head'), reject.
			if ((transferFromBodyObject || (((*si)->second.query(demonymForm)>=0 || (*si)->second.query(commonProfessionForm)>=0) && *si!=m[objects[fromObject].originalLocation].word)) &&
				  find(objects[toObject].associatedNouns.begin(),objects[toObject].associatedNouns.end(),*si)==objects[toObject].associatedNouns.end())
				objects[toObject].associatedNouns.push_back(*si);
	}
	if (objects[fromObject].objectClass==META_GROUP_OBJECT_CLASS)
	{
		// don't include the head word (companion, friend, etc)
		for (vector <tIWMM>::iterator si=objects[fromObject].associatedNouns.begin(),siEnd=objects[fromObject].associatedNouns.end(); si!=siEnd; si++)
			if (*si!=m[objects[fromObject].originalLocation].word && find(objects[toObject].associatedNouns.begin(),objects[toObject].associatedNouns.end(),*si)==objects[toObject].associatedNouns.end())
				objects[toObject].associatedNouns.push_back(*si);
	}
	for (vector <tIWMM>::iterator si=objects[fromObject].associatedAdjectives.begin(),siEnd=objects[fromObject].associatedAdjectives.end(); si!=siEnd; si++)
		if ((transferFromBodyObject || (*si)->second.query(demonymForm)>=0 || (*si)->second.query(PROPER_NOUN_FORM_NUM)>=0 || (*si)->second.query(commonProfessionForm)>=0) &&
				find(objects[toObject].associatedAdjectives.begin(),objects[toObject].associatedAdjectives.end(),*si)==objects[toObject].associatedAdjectives.end() &&
				!isNonTransferrableAdjective(*si))
			objects[toObject].associatedAdjectives.push_back(*si);
	objects[toObject].duplicates.insert(objects[fromObject].duplicates.begin(),objects[fromObject].duplicates.end());
	objects[toObject].updateGenericGenders(objects[fromObject].genericNounMap,objects[fromObject].genericAge);
	if (!(m[objects[toObject].originalLocation].word->second.flags&cSourceWordInfo::genericGenderIgnoreMatch) && 
		  objects[toObject].updateGenericGender(where,m[objects[fromObject].originalLocation].word,objects[fromObject].objectGenericAge,L"replaceObject",debugTrace))
	{
		if (objects[toObject].objectGenericAge==-1 && objects[fromObject].objectGenericAge!=-1)
			objects[toObject].objectGenericAge=objects[fromObject].objectGenericAge;
		// keeps track of how many times an object specifically matches a generic noun (woman, man, lady, girl, boy, etc)
		objects[toObject].genericNounMap.insert(objects[fromObject].genericNounMap.begin(),objects[fromObject].genericNounMap.end());
		for (int I=0; I<4; I++) objects[toObject].genericAge[I]+=objects[fromObject].genericAge[I];
		// girl, woman, boy, man, etc
		if (objects[toObject].mostMatchedGeneric==wNULL && objects[fromObject].mostMatchedGeneric!=wNULL)
			objects[toObject].mostMatchedGeneric=objects[fromObject].mostMatchedGeneric;
		cObject *fo=&objects[fromObject],*to=&objects[toObject];
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:%s:GENERIC to:%s: from=%s:AGE=%d[MMA=%d] [%d %d %d %d] MMG=%s to=%s:AGE=%d[MMA=%d] [%d %d %d %d] MMG=%s",where,fromWhere,
			   objectString(toObject,tmpstr,true).c_str(),
				 m[fo->originalLocation].word->first.c_str(),fo->objectGenericAge,fo->mostMatchedAge,
				 fo->genericAge[0],fo->genericAge[1],fo->genericAge[2],fo->genericAge[3],
				 (fo->mostMatchedGeneric==wNULL) ? L"NULL":fo->mostMatchedGeneric->first.c_str(),

				 to->mostMatchedGeneric->first.c_str(),to->objectGenericAge,to->mostMatchedAge,
				 to->genericAge[0],to->genericAge[1],to->genericAge[2],to->genericAge[3],
				 (to->mostMatchedGeneric==wNULL) ? L"NULL":to->mostMatchedGeneric->first.c_str());
	}
	if ((objects[fromObject].associatedNouns.size() || objects[fromObject].associatedAdjectives.size()) && debugTrace.traceSpeakerResolution)
	{
		wstring nouns,adjectives;
		if (objects[fromObject].objectClass!=META_GROUP_OBJECT_CLASS)
		{
			for (vector <tIWMM>::iterator si=objects[fromObject].associatedNouns.begin(),siEnd=objects[fromObject].associatedNouns.end(); si!=siEnd; si++)
				if (transferFromBodyObject || (((*si)->second.query(demonymForm)>=0 || (*si)->second.query(commonProfessionForm)>=0) && *si!=m[objects[fromObject].originalLocation].word))
					nouns+=(*si)->first+L" ";
		}
		else
			// don't include the head word (companion, friend, etc)
			for (vector <tIWMM>::iterator si=objects[fromObject].associatedNouns.begin(),siEnd=objects[fromObject].associatedNouns.end(); si!=siEnd; si++)
				if (*si!=m[objects[fromObject].originalLocation].word)
					nouns+=(*si)->first+L" ";
		for (vector <tIWMM>::iterator si=objects[fromObject].associatedAdjectives.begin(),siEnd=objects[fromObject].associatedAdjectives.end(); si!=siEnd; si++)
			if (transferFromBodyObject || (*si)->second.query(demonymForm)>=0 || (*si)->second.query(PROPER_NOUN_FORM_NUM)>=0 || (*si)->second.query(commonProfessionForm)>=0)
				adjectives+=(*si)->first+L" ";
		if (nouns.size() || adjectives.size())
			lplog(LOG_RESOLUTION,L"%06d:%s:Object %s associated nouns (%s) and adjectives (%s)",
				where,fromWhere,objectString(toObject,tmpstr,false).c_str(),nouns.c_str(),adjectives.c_str());
	}
}

bool cSource::replaceObjectInSpeakerGroup(int where,int replacementObject,int objectToBeReplaced,int sg,wchar_t *fromWhat)
{ LFS
	vector <cSpeakerGroup>::iterator is=speakerGroups.begin()+sg;
  bool recomputeGroup=replaceObject(where,replacementObject,objectToBeReplaced,is->speakers,L"SPEAKERGROUP",is->sgBegin,is->sgEnd,fromWhat);
  replaceObject(where,replacementObject,objectToBeReplaced,is->groupedSpeakers,L"SPEAKERGROUP [GROUPED]",is->sgBegin,is->sgEnd,fromWhat);
  replaceObject(where,replacementObject,objectToBeReplaced,is->singularSpeakers,L"SPEAKERGROUP [SINGULAR]",is->sgBegin,is->sgEnd,fromWhat);
  replaceObject(where,replacementObject,objectToBeReplaced,is->povSpeakers,L"SPEAKERGROUP [POV]",is->sgBegin,is->sgEnd,fromWhat);
  replaceObject(where,replacementObject,objectToBeReplaced,is->dnSpeakers,L"SPEAKERGROUP [DN]",is->sgBegin,is->sgEnd,fromWhat);
  replaceObject(where,replacementObject,objectToBeReplaced,is->fromNextSpeakerGroup,L"SPEAKERGROUP [FROMNEXT]",is->sgBegin,is->sgEnd,fromWhat);
	for (vector < cSpeakerGroup::cGroup >::iterator sgg=is->groups.begin(); sgg!=is->groups.end(); )
	{
	  replaceObject(where,replacementObject,objectToBeReplaced,sgg->objects,L"SPEAKERGROUP [groups]",is->sgBegin,is->sgEnd,fromWhat);
		// check if group now contains duplicate objects
		set<int> duplicates;
		for (int I=0; I<(signed)sgg->objects.size(); I++)
			duplicates.insert(sgg->objects[I]);
		if (duplicates.size()<sgg->objects.size())
		{
			int offset=(int) (sgg-is->groups.begin());
			is->groups.erase(sgg);
			sgg=is->groups.begin()+offset;
		}
		else 
			sgg++;
	}
	for (vector <cSpeakerGroup>::iterator esg=is->embeddedSpeakerGroups.begin(),esgEnd=is->embeddedSpeakerGroups.end(); esg!=esgEnd; esg++)
	{
		replaceObject(where,replacementObject,objectToBeReplaced,esg->speakers,L"SPEAKERGROUP [ESG]",esg->sgBegin,esg->sgEnd,fromWhat);
		replaceObject(where,replacementObject,objectToBeReplaced,esg->groupedSpeakers,L"SPEAKERGROUP [ESG GS]",esg->sgBegin,esg->sgEnd,fromWhat);
		replaceObject(where,replacementObject,objectToBeReplaced,esg->singularSpeakers,L"SPEAKERGROUP [ESG SS]",esg->sgBegin,esg->sgEnd,fromWhat);
	}
	return recomputeGroup;
}

void cSource::replaceObjectWithObject(int where,vector <cObject>::iterator &object,int replacementObject,wchar_t *fromWhat)
{ LFS
  wstring tmpstr,tmpstr2;
	if (objects[replacementObject].eliminated)
  {
		lplog(LOG_ERROR,L"%06d:[%s]Replacing %s WITH %s",where,fromWhat,objectString(object,tmpstr2,false).c_str(),objectString(replacementObject,tmpstr,false).c_str());
    return;
  }
  int objectToBeReplaced=m[where].getObject();
  if (objectToBeReplaced==replacementObject || objectToBeReplaced<0 || replacementObject<0)
  {
    lplog(LOG_RESOLUTION,L"%06d:Redundant replace %s!",where,objectString(object,tmpstr2,false).c_str());
    return;
  }
	// don't replace Massachusetts with another object like Massachusetts Institute of Technology!
	if (objects[objectToBeReplaced].objectClass==NAME_OBJECT_CLASS && 
			(//objects[objectToBeReplaced].getSubType()==CANADIAN_PROVINCE_CITY || 
			 objects[objectToBeReplaced].getSubType()==COUNTRY || 
			 objects[objectToBeReplaced].getSubType()==ISLAND || 
			 objects[objectToBeReplaced].getSubType()==MOUNTAIN_RANGE_PEAK_LANDFORM || 
			 objects[objectToBeReplaced].getSubType()==OCEAN_SEA || 
			 objects[objectToBeReplaced].getSubType()==PARK_MONUMENT || 
			 objects[objectToBeReplaced].getSubType()==REGION || 
			 objects[objectToBeReplaced].getSubType()==RIVER_LAKE_WATERWAY || 
			 //objects[objectToBeReplaced].getSubType()==US_CITY_TOWN_VILLAGE ||  HAMMOND should match Darryl Hammond - needs more sophisticated test!
			 objects[objectToBeReplaced].getSubType()==US_STATE_TERRITORY_REGION || 
			 objects[objectToBeReplaced].getSubType()==WORLD_CITY_TOWN_VILLAGE))
  {
    lplog(LOG_RESOLUTION,L"%06d:Replacement disallowed because this is a location object (%s) with a limited scope subtype.",where,objectString(object,tmpstr2,false).c_str());
    return;
  }
	// Jay-Z Getting Married vs Jay-Z - noise suppression
	int nameCount=0;
	if (object->objectClass==NAME_OBJECT_CLASS && object->name.any!=wNULL && (nameCount=objects[replacementObject].suspiciousVerbMerge()))
	{
		vector <cTreeCat *> rdfTypes;
		unordered_map <wstring ,int > topHierarchyClassIndexes;
		getObjectRDFTypes((int)(object-objects.begin()),rdfTypes,topHierarchyClassIndexes,TEXT(__FUNCTION__));
		vector <cTreeCat *> rdfTypesReplacement;
		unordered_map <wstring ,int > topHierarchyClassIndexesReplacement;
		getObjectRDFTypes(replacementObject,rdfTypesReplacement,topHierarchyClassIndexesReplacement,TEXT(__FUNCTION__));
		if (rdfTypesReplacement.empty() && !objects[replacementObject].wikiDefinition() && rdfTypes.size()>0)
		{
			lplog(LOG_RESOLUTION,L"%06d:Replacement of %s stopped because the replacement object %s has no identification and has suspicious verb (%d).",
						where,objectString(object,tmpstr,false).c_str(),objectString(replacementObject,tmpstr2,false).c_str(),nameCount);
		  __int64 objectTmp=object-objects.begin();
			object=objects.begin()+replacementObject;
			replacementObject=(int)objectTmp;
			return;
		}
		lplog(LOG_RESOLUTION,L"%06d:Replacement of %s (rdfTypes=%d) allowed even though replacement object %s (rdfTypes=%d) has suspicious verb (%d)%s.",
					where,objectString(object,tmpstr,false).c_str(),rdfTypes.size(),objectString(replacementObject,tmpstr2,false).c_str(),rdfTypesReplacement.size(),nameCount,(objects[replacementObject].wikiDefinition()) ? L" wikiDefinition":L"");
	}
  if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
    lplog(LOG_RESOLUTION,L"%06d:[%s]Replacing %s WITH %s",where,fromWhat,objectString(object,tmpstr2,false).c_str(),objectString(replacementObject,tmpstr,false).c_str());
	if (!objects[replacementObject].wikipediaAccessed)
	{
		if (object->wikipediaAccessed)
		{
			if (objects[replacementObject].getSubType()==-1 || objects[replacementObject].getSubType()==UNKNOWN_PLACE_SUBTYPE ||
				  (object->getSubType()!=-1 && object->getSubType()!=UNKNOWN_PLACE_SUBTYPE))
			{
				objects[replacementObject].setSubType(object->getSubType());
				objects[replacementObject].wikipediaAccessed=true;
			}
		  else if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
				lplog(LOG_RESOLUTION,L"%06d:[%s]subtype from %s refused for %s",where,fromWhat,objectString(object,tmpstr2,false).c_str(),objectString(replacementObject,tmpstr,false).c_str());
		}
		else if (object->getSubType()<0 && objects[replacementObject].getSubType()==UNKNOWN_PLACE_SUBTYPE) objects[replacementObject].resetSubType(); // not a place
	}
	if (object->isNotAPlace) objects[replacementObject].isNotAPlace=true; // not a place
	if (objects[replacementObject].isNotAPlace)
		objects[replacementObject].resetSubType();
	if (objects[replacementObject].objectClass==NAME_OBJECT_CLASS || objects[replacementObject].objectClass==NON_GENDERED_NAME_OBJECT_CLASS)
	{
		if (object->suspiciousVerbMerge())
			lplog(LOG_RESOLUTION,L"%06d:Not merging of %s into %s because of suspicious verb.",where,objectString(object,tmpstr,false).c_str(),objectString(replacementObject,tmpstr2,false).c_str());
		else
			objects[replacementObject].name.merge(object->name,debugTrace);
	}
	if (object->objectClass==NON_GENDERED_NAME_OBJECT_CLASS && objects[replacementObject].objectClass==NAME_OBJECT_CLASS)
	{
		objects[replacementObject].objectClass=NON_GENDERED_NAME_OBJECT_CLASS;
		objects[replacementObject].male=objects[replacementObject].female=false;
		objects[replacementObject].neuter=true;
	}
  objects[replacementObject].numEncounters+=object->numEncounters;
  objects[replacementObject].numIdentifiedAsSpeaker+=object->numIdentifiedAsSpeaker;
  objects[replacementObject].numDefinitelyIdentifiedAsSpeaker+=object->numDefinitelyIdentifiedAsSpeaker;
  objects[replacementObject].numSpokenAboutInSection+=object->numSpokenAboutInSection;
  objects[replacementObject].numIdentifiedAsSpeakerInSection+=object->numIdentifiedAsSpeakerInSection;
  objects[replacementObject].numDefinitelyIdentifiedAsSpeakerInSection+=object->numDefinitelyIdentifiedAsSpeakerInSection;
  objects[replacementObject].numEncountersInSection+=object->numEncountersInSection;
	if (objects[replacementObject].getFirstSpeakerGroup() < 0)
		objects[replacementObject].setFirstSpeakerGroup(object->getFirstSpeakerGroup());
  if (objects[replacementObject].lastSpeakerGroup<object->lastSpeakerGroup) objects[replacementObject].lastSpeakerGroup=object->lastSpeakerGroup;
  m[where].setObject(replacementObject);
  m[where].originalObject=objectToBeReplaced;
  if ((object->male ^ object->female) && objects[replacementObject].objectClass!=NON_GENDERED_NAME_OBJECT_CLASS)
  {
		bool ambiguousGender=objects[replacementObject].male && objects[replacementObject].female;
    objects[replacementObject].male=object->male;
    objects[replacementObject].female=object->female;
		if (ambiguousGender)
			addDefaultGenderedAssociatedNouns(replacementObject);
  }
  if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
    lplog(LOG_RESOLUTION,L"%06d:     Resulting in %s",where,objectString(replacementObject,tmpstr,false).c_str());
  vector <cObject::cLocation>::iterator ol=objects[objectToBeReplaced].locations.begin(),olEnd=objects[objectToBeReplaced].locations.end();
  for (; ol!=olEnd; ol++)
  {
    //int begin=m[ol->at].beginObjectPosition;
    //if (begin>=0 && m[begin].principalWherePosition==objectToBeReplaced)
    //  m[begin].principalWherePosition=replacementObject;
    //if (begin>=0 && m[begin].principalWhereAdjectivalPosition==objectToBeReplaced)
    //  m[begin].principalWhereAdjectivalPosition=replacementObject;
    if (m[ol->at].getObject()==objectToBeReplaced)
    {
      m[ol->at].setObject(replacementObject);
      m[ol->at].originalObject=objectToBeReplaced;
      objects[replacementObject].locations.push_back(*ol);
	    objects[replacementObject].updateFirstLocation(ol->at);
      continue;
    }
    replaceObject(replacementObject,objectToBeReplaced,m[ol->at].objectMatches,ol);
    objects[replacementObject].locations.push_back(*ol);
    objects[replacementObject].updateFirstLocation(ol->at);
  }
  set <int>::iterator olsl=objects[objectToBeReplaced].speakerLocations.begin(),olslEnd=objects[objectToBeReplaced].speakerLocations.end();
  for (; olsl!=olslEnd; olsl++)
  {
    replaceObject(replacementObject,objectToBeReplaced,m[*olsl].objectMatches,*olsl);
    objects[replacementObject].speakerLocations.insert(*olsl);
    replaceObject(replacementObject,objectToBeReplaced,m[*olsl].audienceObjectMatches,*olsl);
  }
	if (objects[objectToBeReplaced].aliases.size())
	{
		objects[replacementObject].aliases.insert(objects[replacementObject].aliases.end(),objects[objectToBeReplaced].aliases.begin(),objects[objectToBeReplaced].aliases.end());
		if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:Object %s gained alias %s (2).",where,objectString(replacementObject,tmpstr,true).c_str(),objectString(objects[objectToBeReplaced].aliases,tmpstr2).c_str());
	}
	moveNyms(where,replacementObject,objectToBeReplaced,L"replaceObjectWithObject");
	objects[replacementObject].duplicates.insert(objects[objectToBeReplaced].duplicates.begin(),objects[objectToBeReplaced].duplicates.end());
  object->eliminated=true;
  object->replacedBy=replacementObject;
  objects[replacementObject].duplicates.insert(objectToBeReplaced);
  replaceObject(replacementObject,objectToBeReplaced,fromWhat);
  //replaceObject(replacementObject,objectToBeReplaced,quotedLocalObjects,fromWhat);
  int I=0;
  for (vector <cSection>::iterator is=sections.begin(),isEnd=sections.end(); is!=isEnd; is++,I++)
    replaceObject(where,replacementObject,objectToBeReplaced,is->preIdentifiedSpeakerObjects,L"SECTION",is->begin,is->endHeader,fromWhat);
  I=0;
  for (vector <cSpeakerGroup>::iterator is=speakerGroups.begin(),isEnd=speakerGroups.end(); is!=isEnd; is++,I++)
		replaceObjectInSpeakerGroup(where,replacementObject,objectToBeReplaced,I,fromWhat);
	vector <cLocalFocus>::iterator lsi=in(objectToBeReplaced);
	if (lsi!=localObjects.end())
	{
		if (debugTrace.traceSpeakerResolution)
		{
			lplog(LOG_RESOLUTION,L"%06d:Removed %s from localObjects",where,objectString(lsi->om.object,tmpstr,true).c_str());
		}
		localObjects.erase(lsi);
	}
}

bool cObject::cataphoricMatch(cObject *obj)
{ LFS
  return obj->isPronounLike() && ((obj->male && male) || (obj->female && female)) && obj->plural==plural;
}

bool cSource::recentExit(vector <cLocalFocus>::iterator lsi)
{ LFS
	return lsi->lastExit>=0 && lsi->lastExit>lsi->lastEntrance &&
		((speakerGroupsEstablished && lsi->lastExit>speakerGroups[currentSpeakerGroup].sgBegin) ||
		((!speakerGroupsEstablished && currentSpeakerGroup>0 && lsi->lastExit>speakerGroups[currentSpeakerGroup-1].sgBegin)));
}

void cSource::testLocalFocus(int where,vector <cLocalFocus>::iterator lsi)
{ LFS
	if (!objects[lsi->om.object].neuter || lsi->numIdentifiedAsSpeaker>0 || objects[lsi->om.object].isLocationObject) return;
	wstring tmpstr;
	vector <cSpaceRelation>::iterator csr=findSpaceRelation(where);
	if (csr!=spaceRelations.end() && csr!=spaceRelations.begin() && csr->where>where)
		csr--;
	if (csr!=spaceRelations.end() && where==csr->where && (csr->relationType==stLOCATION))
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:made location object %s PP and NS.",where,objectString(lsi->om,tmpstr,false).c_str());
		lsi->physicallyPresent=true;
		lsi->notSpeaker=true;
		objects[lsi->om.object].isLocationObject=true;
	}
}

// push matchingObject onto matchingObjects at 'where' (unless already there) AND
// push matchingObject onto localSpeakers (unless already there).
// in secondary quotes, inPrimaryQuote=false
bool cSource::pushObjectIntoLocalFocus(int where, int matchingObject, bool identifiedAsSpeaker, bool notSpeaker, bool inPrimaryQuote, bool inSecondaryQuote, wchar_t *fromWhere, vector <cLocalFocus>::iterator &lsi)
{ LFS
	if ((objects[matchingObject].objectClass==META_GROUP_OBJECT_CLASS && (m[where].queryWinnerForm(numeralCardinalForm)!=-1 || m[where].word->first==L"that")) || (where==0 && m[where].word->first==wstring(L"start")))
		return false;
	wstring tmpstr;
	int beginEntirePosition=m[where].beginObjectPosition; // if this is an adjectival object 
	if (m[where].flags&cWordMatch::flagAdjectivalObject) 
		for (; beginEntirePosition>=0 && m[beginEntirePosition].principalWherePosition<0; beginEntirePosition--);
	bool physicallyEvaluated;
  if ((lsi=in(matchingObject))==localObjects.end() || (lsi->numEncounters==0 && lsi->numIdentifiedAsSpeaker==0))
  {
		// if speaker object, subject, physically present and new, mark. see salienceNPP
		int oc;
		// only occupation AND demonym classes have a match algorithm that pulls in non-local objects 
		if ((m[where].getObject()==matchingObject || objects[matchingObject].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || objects[matchingObject].objectClass==GENDERED_DEMONYM_OBJECT_CLASS) && 
			  !identifiedAsSpeaker && !notSpeaker && !inPrimaryQuote && 
			  !unResolvablePosition(beginEntirePosition) && 
				(physicallyPresentPosition(where,beginEntirePosition,physicallyEvaluated,false) || objects[matchingObject].numIdentifiedAsSpeaker || objects[matchingObject].numDefinitelyIdentifiedAsSpeaker) &&
				(oc=objects[matchingObject].objectClass)!=BODY_OBJECT_CLASS && 
				oc!=REFLEXIVE_PRONOUN_OBJECT_CLASS && oc!=RECIPROCAL_PRONOUN_OBJECT_CLASS && 
				oc!=PRONOUN_OBJECT_CLASS &&
				(oc!=META_GROUP_OBJECT_CLASS || currentSpeakerGroup<speakerGroups.size()) &&
				// pushObjectIntoLocalFocus may be called in a hail situation with inPrimaryQuote==false, though it really is in a quote.
				!(m[where].objectRole&HAIL_ROLE) &&
				!(m[where].flags&cWordMatch::flagAdjectivalObject) &&
				!objects[matchingObject].neuter && 
			  (section>=sections.size() || where>=(signed)sections[section].endHeader))
		{
			if (debugTrace.traceSpeakerResolution && section<sections.size())
				lplog(LOG_RESOLUTION,L"%06d:added implicitly unresolvable role to object %s (%s) [%d].",where,objectString(matchingObject,tmpstr,false).c_str(),fromWhere,sections[section].endHeader);
			m[where].objectRole|=UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
		}
	}
	if (lsi==localObjects.end())
  {
    localObjects.push_back(cLocalFocus(matchingObject,inPrimaryQuote,inSecondaryQuote,
			                     notSpeaker && !objects[matchingObject].numIdentifiedAsSpeaker && !objects[matchingObject].numDefinitelyIdentifiedAsSpeaker,
													 physicallyPresentPosition(where,beginEntirePosition,physicallyEvaluated,false) && physicallyEvaluated && !objects[matchingObject].isTimeObject));
    lsi=localObjects.begin()+localObjects.size()-1;
		if (lsi->physicallyPresent && notSpeaker && (m[where].endObjectPosition-m[where].beginObjectPosition)==1 &&
			(m[where].word->second.timeFlags&T_UNIT) && (m[where].flags&cWordMatch::flagFirstLetterCapitalized))
			lsi->physicallyPresent=false;
		testLocalFocus(where,lsi);
		if (!notSpeaker && 
			  ((m[where].flags&cWordMatch::flagUnresolvableObjectResolvedThroughSpeakerGroup) || 
			   ((m[where].objectRole&IN_PRIMARY_QUOTE_ROLE) && (m[where].objectRole&(HAIL_ROLE|PP_OBJECT_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE|IN_QUOTE_REFERRING_AUDIENCE_ROLE)))))
			lsi->physicallyPresent=lsi->occurredOutsidePrimaryQuote=true;
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:added local %s %s %s %s.",where,(lsi->notSpeaker) ? L"object":L"speaker",objectString(matchingObject,tmpstr,false).c_str(),fromWhere,(lsi->physicallyPresent) ? L"PP":L"notPP");
		if (lsi->physicallyPresent)
		{
			lsi->whereBecamePhysicallyPresent=where;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:%s became physically present (%s) (4).",where,objectString(lsi->om,tmpstr,false).c_str(),fromWhere);
			if (recentExit(lsi))
				lplog(LOG_RESOLUTION,L"%06d:%s physically present after exit @%d (4)",where,objectString(lsi->om,tmpstr,true).c_str(),lsi->lastExit);
		}
		if (!notSpeaker && (m[where].objectRole&IN_SECONDARY_QUOTE_ROLE) && 
			  (m[where].objectRole&(HAIL_ROLE|PP_OBJECT_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE|IN_QUOTE_REFERRING_AUDIENCE_ROLE)))
			lsi->occurredInPrimaryQuote=true;
  }
  else
	{
		bool currentlyPhysicallyPresent=(physicallyPresentPosition(where,beginEntirePosition,physicallyEvaluated,false) && physicallyEvaluated);
		if (identifiedAsSpeaker && !notSpeaker && !currentlyPhysicallyPresent && m[where].getObject()>=0 && objects[m[where].getObject()].objectClass==NAME_OBJECT_CLASS) 
			currentlyPhysicallyPresent=physicallyEvaluated=true;
		if (!lsi->physicallyPresent && currentlyPhysicallyPresent && notSpeaker && (m[where].endObjectPosition-m[where].beginObjectPosition)==1 &&
			(m[where].word->second.timeFlags&T_UNIT) && (m[where].flags&cWordMatch::flagFirstLetterCapitalized))
			currentlyPhysicallyPresent=false;
		testLocalFocus(where,lsi);
		//if (m[where].getObject()>=0 && objects[m[where].getObject()].objectClass==BODY_OBJECT_CLASS && (m[where].word->second.flags&cSourceWordInfo::physicalObjectByWN)==0)
		//{
		//	currentlyPhysicallyPresent=false;
		//}
		//  Ushered into the presence of Mr . Carter , he[carter] and I[tommy] wish each other good morning as is customary . 
		//    He[carter] then says : ‘[carter:tommy] Please MOVE_OBJECTtake a seat , Mr[tommy] . -- er ? ’ 
		// a secondary speaker is never physically present, even though the verb is present tense.
		wstring word; // for debugging
		int ir=-1,irv=-1; // for debugging // i=m[where].getRelVerb(),
		if (!lsi->physicallyPresent && currentlyPhysicallyPresent && 
			  ((m[where].objectRole&SECONDARY_SPEAKER_ROLE) || 
				 (inSecondaryQuote && m[where].getRelVerb()>=0 && (word=m[m[where].getRelVerb()].getMainEntry()->first)==L"am") ||
				 (inSecondaryQuote && m[where].getObject()>=0 && (ir=objects[m[where].getObject()].whereRelativeClause)>=0 && (irv=m[objects[m[where].getObject()].whereRelativeClause].getRelVerb())>=0 && (word=m[m[objects[m[where].getObject()].whereRelativeClause].getRelVerb()].getMainEntry()->first)==L"am")))
			currentlyPhysicallyPresent=false;
		// adaptation for not breaking up secondary speaker groups
		// if subject has not been physically present for a while, make this not physically present (but not if in speakergroup).
		if (((m[where].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_EMBEDDED_STORY_OBJECT_ROLE))==(IN_PRIMARY_QUOTE_ROLE|IN_EMBEDDED_STORY_OBJECT_ROLE)) &&
			lsi->physicallyPresent && !currentlyPhysicallyPresent && lsi->getAge(false,objectToBeMatchedInQuote,quoteIndependentAge)>=10 && 
			(currentSpeakerGroup>=speakerGroups.size() || speakerGroups[currentSpeakerGroup].speakers.find(lsi->om.object)==speakerGroups[currentSpeakerGroup].speakers.end()))
		{
	    if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION|LOG_SG,L"%d:Setting secondary speaker %s to not physically present (age>=10).",where,objectString(lsi->om,tmpstr,true).c_str());
			 lsi->physicallyPresent=false;
		}
		lsi->resetAge(objectToBeMatchedInQuote);
		if (inPrimaryQuote && ((m[where].objectRole&HAIL_ROLE) || (m[where].objectRole&PP_OBJECT_ROLE)))
			lsi->occurredOutsidePrimaryQuote=true;
		if (inSecondaryQuote && (m[where].objectRole&HAIL_ROLE))
			lsi->occurredInPrimaryQuote=true;
		lsi->lastRoleSalience=getRoleSalience(m[where].objectRole);
		lsi->numMatchedAdjectives=0;
		lsi->notSpeaker=lsi->notSpeaker && notSpeaker;
		wstring tmpstr2;
		if (!lsi->physicallyPresent && currentlyPhysicallyPresent)
		{
			bool re=recentExit(lsi);
			if (re && (m[where].objectRole&HAIL_ROLE))
				currentlyPhysicallyPresent=false;
			else
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:%s became physically present (3)",where,objectString(lsi->om,tmpstr,true).c_str());
				if (re)
				{
					wchar_t *deleted=L"";
					if (!speakerGroupsEstablished && lsi->lastExit>=0 && (m[lsi->lastExit].objectRole&SUBJECT_ROLE) && 
						  m[lsi->lastExit].getObject()>=0 && objects[m[lsi->lastExit].getObject()].getSubType()<0 &&
						  // if the subject has an object, it must be a place
							((m[lsi->lastExit].getRelObject()>=0 && 
							  m[m[lsi->lastExit].getRelObject()].getObject()>=0 &&
							  objects[m[m[lsi->lastExit].getRelObject()].getObject()].getSubType()<0) ||
							// if the subject has an atached prepositional phrase, the object of the prepositional phrase must be a place or a time (retired for the night)
							 (m[lsi->lastExit].getRelVerb()>=0 && m[m[lsi->lastExit].getRelVerb()].relPrep>=0 && 
							  m[m[m[lsi->lastExit].getRelVerb()].relPrep].getRelObject()>=0 &&
							  m[m[m[m[lsi->lastExit].getRelVerb()].relPrep].getRelObject()].getObject()>=0 && 
								objects[m[m[m[m[lsi->lastExit].getRelVerb()].relPrep].getRelObject()].getObject()].getSubType()<0 && !(m[m[m[m[lsi->lastExit].getRelVerb()].relPrep].getRelObject()].word->second.timeFlags&T_UNIT))))
					{
						cSpaceRelation sr(lsi->lastExit,-1,-1,-1,-1,-1,-1,-1,-1,stEXIT,false,false,-1,-1,true);
						bool comparesr( const cSpaceRelation &s1, const cSpaceRelation &s2 );
						vector <cSpaceRelation>::iterator location = lower_bound(spaceRelations.begin(), spaceRelations.end(), sr, comparesr);
						if (location!=spaceRelations.end() && location->relationType==stEXIT)
						{
							// or turn the exit off
							m[lsi->lastExit].spaceRelation=false;
							deleted=L"[DELETED]";
						}
					}
					if (isAgentObject(lsi->om.object) && (m[where].getRelVerb()<0 || !isVerbClass(m[where].getRelVerb(),stENTER)))
					{
						/*
						int oc=objects[m[where].getObject()].objectClass;
						int lastExitSubject=lsi->lastExit;
						if (m[lsi->lastExit].relSubject>=0) lastExitSubject=m[lsi->lastExit].relSubject;
						if (m[lastExitSubject].getObject()>=0 && objects[m[lastExitSubject].getObject()].objectClass==META_GROUP_OBJECT_CLASS && 
							  (oc==NAME_OBJECT_CLASS || oc==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || oc==GENDERED_DEMONYM_OBJECT_CLASS))
						*/
						if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:%s physically present after exit%s @%d (3) [%s]",where,objectString(lsi->om,tmpstr,true).c_str(),deleted,lsi->lastExit,wsrToText(lsi->lastExit,tmpstr2).c_str());
					}
				}
				lsi->whereBecamePhysicallyPresent=where;
			}
		}
		lsi->physicallyPresent=lsi->physicallyPresent || currentlyPhysicallyPresent;
		lsi->previousWhere=lsi->lastWhere;
	}
  lsi->lastWhere=where;
	if (lsi->om.object<=1 && objects[lsi->om.object].originalLocation==0)
		objects[lsi->om.object].originalLocation=where;
  if (identifiedAsSpeaker)
  {
    objects[lsi->om.object].numIdentifiedAsSpeaker++;
		if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[lsi->om.object].getSubType()>=0)
			lplog(LOG_RESOLUTION,L"%06d:Removing place designation (6) from object %s.",where,objectString(lsi->om.object,tmpstr,false).c_str());
		objects[lsi->om.object].resetSubType(); // not a place
		objects[lsi->om.object].isNotAPlace=true;
    lsi->numIdentifiedAsSpeaker++;
    objects[lsi->om.object].numIdentifiedAsSpeakerInSection++;
    if (objects[lsi->om.object].numDefinitelyIdentifiedAsSpeakerInSection+objects[lsi->om.object].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
      sections[section].speakerObjects.push_back(cOM(lsi->om.object,SALIENCE_THRESHOLD));
  }
  else
  {
		vector <cObject>::iterator o=objects.begin()+lsi->om.object;
    objects[lsi->om.object].numEncounters++;
    objects[lsi->om.object].numEncountersInSection++;
    lsi->numEncounters++;
    if (++objects[lsi->om.object].numSpokenAboutInSection==1 && section<sections.size())
      sections[section].objectsSpokenAbout.push_back(cOM(lsi->om.object,SALIENCE_THRESHOLD));
  }
  return true;
}

vector <cLocalFocus>::iterator cSource::substituteAlias(int where,vector <cLocalFocus>::iterator lsi)
{ LFS
	bool preferSpeaker=(objects[lsi->om.object].objectClass==NAME_OBJECT_CLASS);
	// if the primary is a name, prefer whichever the primary or alias is a speaker.
	if (preferSpeaker && (currentSpeakerGroup>=speakerGroups.size() || speakerGroups[currentSpeakerGroup].speakers.find(lsi->om.object)!=speakerGroups[currentSpeakerGroup].speakers.end()))
		return lsi;
	for (vector <int>::iterator ai=objects[lsi->om.object].aliases.begin(),aEnd=objects[lsi->om.object].aliases.end(); ai!=aEnd; ai++)
		if (objects[*ai].objectClass==NAME_OBJECT_CLASS)
		{
			if (preferSpeaker && speakerGroups[currentSpeakerGroup].speakers.find(*ai)==speakerGroups[currentSpeakerGroup].speakers.end())
				continue;
			vector <cLocalFocus>::iterator tlsi=in(*ai);
			if (tlsi == localObjects.end())
			{
				if (!pushObjectIntoLocalFocus(where, *ai, lsi->numIdentifiedAsSpeaker > 0, false, (m[where].objectRole&IN_PRIMARY_QUOTE_ROLE) != 0, (m[where].objectRole&IN_SECONDARY_QUOTE_ROLE) != 0, L"alias", lsi))
					return localObjects.end();
			}
			else
				lsi = tlsi;
		}
	return lsi;
}

// push object onto matchingObjects at 'where' (unless it is already there) AND
void cSource::pushLocalObjectOntoMatches(int where,vector <cLocalFocus>::iterator lsi,wchar_t *reason)
{ LFS
  wstring tmpstr;
	vector <cOM>::iterator rr; // replacement record
  if (currentSpeakerGroup<speakerGroups.size() &&
      (rr=in(lsi->om.object,speakerGroups[currentSpeakerGroup].replacedSpeakers))!=speakerGroups[currentSpeakerGroup].replacedSpeakers.end())
  {
    if (in(rr->salienceFactor,m[where].objectMatches)!=m[where].objectMatches.end()) return;
		followObjectChain(rr->salienceFactor);
    m[where].objectMatches.push_back(cOM(rr->salienceFactor,lsi->om.salienceFactor));
    objects[rr->salienceFactor].locations.push_back(cObject::cLocation(where));
    objects[rr->salienceFactor].updateFirstLocation(where);
    if (debugTrace.traceSpeakerResolution)
    {
      wstring tmpstr2;
      lplog(LOG_RESOLUTION,L"%06d:Speakergroup replaced %s with %s.",where,objectString(lsi->om.object,tmpstr,true).c_str(),objectString(rr->salienceFactor,tmpstr2,true).c_str());
      lplog(LOG_RESOLUTION,L"%06d:added match (%s) %s: Salience %d - %s",where,reason,objectString(rr->salienceFactor,tmpstr,false).c_str(),lsi->om.salienceFactor,lsi->res.c_str());
    }
  }
  else
  {
    if (in(lsi->om.object,m[where].objectMatches)!=m[where].objectMatches.end()) return;
		if (matchAliases(where,lsi->om.object,m[where].objectMatches))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:alias of %s already matched",where,objectString(lsi->om.object,tmpstr,false).c_str());
			return;
		}
		// substitute a lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms class alias ('Boris' for 'a Russian')
		if ((lsi=substituteAlias(where,lsi))!=localObjects.end())
		{
			m[where].objectMatches.push_back(lsi->om);
			objects[lsi->om.object].locations.push_back(cObject::cLocation(where));
			objects[lsi->om.object].updateFirstLocation(where);
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr2;
				lplog(LOG_RESOLUTION,L"%06d:added match (%s) %s: Salience %d - %s",where,reason,objectString(lsi->om.object,tmpstr,false).c_str(),lsi->om.salienceFactor,lsi->res.c_str());
			}
		}
  }
}

// where is original object having gender characteristics to transfer to toObject.
void cSource::narrowGender(int where,int toObject)
{ LFS
	if (!objects[toObject].male || !objects[toObject].female || (objects[toObject].male ^ objects[toObject].female)) return;
  int object=m[where].getObject();
  if (objects[toObject].objectClass==BODY_OBJECT_CLASS && objects[toObject].getOwnerWhere()>=0)
  {
    toObject=m[objects[toObject].getOwnerWhere()].getObject();
    if (toObject<0 || !(objects[toObject].male && objects[toObject].female))
      return;
  }
	wstring tmpstr;
  if (object< cObject::eOBJECTS::UNKNOWN_OBJECT)
  {
    int inflectionFlags=m[where].word->second.inflectionFlags;
    if ((inflectionFlags&(MALE_GENDER|MALE_GENDER_ONLY_CAPITALIZED)) ^ (inflectionFlags&(FEMALE_GENDER|FEMALE_GENDER_ONLY_CAPITALIZED)))
    {
      if (m[where].queryWinnerForm(PROPER_NOUN_FORM_NUM)>=0 ||  (m[where].flags&cWordMatch::flagFirstLetterCapitalized)!=0 ||
        (inflectionFlags&(MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED))==0)
      {
        objects[toObject].male=(inflectionFlags&(MALE_GENDER|MALE_GENDER_ONLY_CAPITALIZED))!=0;
        objects[toObject].female=(inflectionFlags&(FEMALE_GENDER|FEMALE_GENDER_ONLY_CAPITALIZED))!=0;
      }
      else
        objects[toObject].male=objects[toObject].female=false;
			addDefaultGenderedAssociatedNouns(toObject);
      if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
      {
        lplog(LOG_RESOLUTION,L"%06d:Match %s becomes %s from object %s (2).",where,
          objectString(objects.begin()+toObject,tmpstr,false).c_str(),(objects[toObject].male) ? L"male" : L"female",
          m[where].word->first.c_str());
				if (objects[toObject].getSubType()>=0)
					lplog(LOG_RESOLUTION,L"%06d:Removing place designation (7) from object %s.",where,objectString(toObject,tmpstr,false).c_str());
      }
			objects[toObject].resetSubType(); // not a place
			objects[toObject].isNotAPlace=true; 
			objects[toObject].genderNarrowed=true; 
    }
  }
  if (object>=0 &&
    (objects[object].objectClass==PRONOUN_OBJECT_CLASS ||
    objects[object].objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS ||
    objects[object].objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS ||
    objects[object].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
    objects[object].objectClass==BODY_OBJECT_CLASS ||
    objects[object].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
    objects[object].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
    objects[object].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
    objects[object].objectClass==META_GROUP_OBJECT_CLASS))
  {
    if (objects[object].male ^ objects[object].female)
    {
      objects[toObject].male=objects[object].male;
      objects[toObject].female=objects[object].female;
			addDefaultGenderedAssociatedNouns(toObject);
      if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
      {
        lplog(LOG_RESOLUTION,L"%06d:Match %s becomes %s from object %s (3).",where,
          objectString(objects.begin()+toObject,tmpstr,false).c_str(),(objects[toObject].male) ? L"male" : L"female",
          objectString(objects.begin()+object,tmpstr,false).c_str());
				if (objects[toObject].getSubType()>=0)
					lplog(LOG_RESOLUTION,L"%06d:Removing place designation (8) from object %s.",where,objectString(toObject,tmpstr,false).c_str());
      }
			objects[toObject].resetSubType(); // not a place
			objects[toObject].isNotAPlace=true; 
			objects[toObject].genderNarrowed=true; 
    }
  }
}

// make sure that there are no BODY_OBJECT_CLASS objects
// that have their owner objects also in objectMatches
void cSource::eliminateBodyObjectRedundancy(int where,vector <cOM> &objectMatches)
{ LFS
  for (vector <cOM>::iterator omi=objectMatches.begin(); omi!=objectMatches.end(); )
  {
    bool allIn,oneIn;
		int ownerWhere =objects[omi->object].getOwnerWhere();
    if (objects[omi->object].objectClass==BODY_OBJECT_CLASS &&
        ((ownerWhere >=0 && (in(m[ownerWhere].getObject(),objectMatches)!=objectMatches.end() ||
         intersect(m[ownerWhere].objectMatches,objectMatches,allIn,oneIn))) ||
         intersect(m[objects[omi->object].originalLocation].objectMatches,objectMatches,allIn,oneIn))) // She[tuppence] saw Julius throw a sideways glance[julius] at her .
    {
      if (debugTrace.traceSpeakerResolution)
      {
        wstring tmpstr;
        lplog(LOG_RESOLUTION,L"%06d:Eliminated body object redundancy %s.",where,objectString(omi->object,tmpstr,true).c_str());
      }
      omi=objectMatches.erase(omi);
    }
    else
      omi++;
  }
}

bool cSource::evaluateCompoundObjectAsGroup(int where,bool &physicallyEvaluated)
{ LFS
	int numPhysicallyPresent=0,numTotal=1,numObjectsResolved,opw;
	vector <cLocalFocus>::iterator lsi;
	for (opw=m[where].previousCompoundPartObject; opw>=0 && numTotal<10; numTotal++)
	{
		if ((lsi=in(m[opw].getObject()))!=localObjects.end() && lsi->physicallyPresent)
			numPhysicallyPresent++;
		else
			for (vector <cOM>::iterator omi=m[opw].objectMatches.begin(),omiEnd=m[opw].objectMatches.end(); omi!=omiEnd; omi++)
				if ((lsi=in(omi->object))!=localObjects.end() && lsi->physicallyPresent)
				{
					numPhysicallyPresent++;
					break;
				}
		opw=m[opw].previousCompoundPartObject;
	}
	numObjectsResolved=numTotal;
	// the objects afterward are not resolved yet.
	for (opw=m[where].nextCompoundPartObject; opw>=0; numTotal++)
	{
		if ((lsi=in(m[opw].getObject()))!=localObjects.end() && lsi->physicallyPresent)
			numPhysicallyPresent++;
		opw=m[opw].nextCompoundPartObject;
	}
	// if at least half of the resolved objects are physically present, then most likely this is a list of people who are all physically present
	if (numTotal>1 && numPhysicallyPresent>=(numObjectsResolved-1)/2)
	{
		wstring tmpstr,tmpstr2;
		for (opw=where; opw>=0; opw=m[opw].previousCompoundPartObject)
		{
			tmpstr+=itos(opw,tmpstr2)+L" ";
			m[opw].objectRole|=FOCUS_EVALUATED;
		}
		for (opw=m[where].nextCompoundPartObject; opw>=0; opw=m[opw].nextCompoundPartObject)
		{
			tmpstr+=itos(opw,tmpstr2)+L" ";
			m[opw].objectRole|=FOCUS_EVALUATED;
		}
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:Compound object list %s becomes physically present (numPhysicallyPresent=%d,numObjectsResolved=%d).",where,tmpstr.c_str(),numPhysicallyPresent,numObjectsResolved);
		return physicallyEvaluated=true;
	}
	return physicallyEvaluated=false;
}

#define HIGHEST 8
void cSource::matchAdditionalObjectsIfPlural(int where,bool isPlural,bool atLeastOneReference,
																						bool physicallyEvaluated,bool physicallyPresent,
																						bool inPrimaryQuote,bool inSecondaryQuote,
																						bool definitelySpeaker,bool mixedPlurality,
    vector <cLocalFocus>::iterator *highest,int numNeuterObjects,int numGenderedObjects,
		vector <int> &lsOffsets)
{ LFS
	int I;
  vector <cWordMatch>::iterator im=m.begin()+where;
  int object=im->getObject();
	// if a plural pronoun, and at least one object has matched, and what is being matched is not a gendered body object,
	// and what has been matched is single and either only one object has been matched or one object could merge into the other object
	// OR one object is neuter and the other object is not neuter. (they could either be two people, or two neuter things, but not one person and one thing, usually)
	// don't consider if notSpeaker is set because this could leak into speakerGroups.
  if (isPlural && atLeastOneReference && (im->getObject()<0 || objects[im->getObject()].objectClass!=BODY_OBJECT_CLASS) &&
      !objects[im->objectMatches[0].object].plural && (im->objectMatches.size()<2 ||
      (im->objectMatches.size()==2 && potentiallyMergable(where,objects.begin()+im->objectMatches[0].object,objects.begin()+im->objectMatches[1].object,false,false)) ||
			(numNeuterObjects==1 && numGenderedObjects==1)))
  {
    bool otherMatchFound=false;
		int highestNextSF=0;
    for (I=0; I<HIGHEST; I++)
    {
			if (physicallyEvaluated && physicallyPresent && highest[I]!=localObjects.end() && !highest[I]->physicallyPresent)
				continue;
      if (highest[I]!=localObjects.end() && highest[I]->om.salienceFactor>=highestNextSF/2 && // still high salience
					in(highest[I]->om.object,im->objectMatches)== im->objectMatches.end() && // not already selected
					(currentSpeakerGroup<speakerGroups.size() || !highest[I]->notSpeaker)) // don't consider if notSpeaker is set because this could leak into speakerGroups.
      {
        pushLocalObjectOntoMatches(where,highest[I],L"chooseBest PLURAL");
        lsOffsets.push_back((int)(highest[I]-localObjects.begin()));
        otherMatchFound=true;
				if (highestNextSF==0) highestNextSF=highest[I]->om.salienceFactor;
      }
    }
    vector <cSpeakerGroup>::iterator lastSG=speakerGroups.begin()+currentSpeakerGroup-1;
    // If in identifySpeakerGroups, so speaker groups are BEING established
    // If there is still only one speaker found, and that speaker is in the last speaker group,
    // add other members of that speaker group that are also in localObjects. REF:SUBGROUPS
    if (!otherMatchFound && currentSpeakerGroup && currentSpeakerGroup==speakerGroups.size())
		{
			if (tempSpeakerGroup.speakers.find(im->objectMatches[0].object)!=tempSpeakerGroup.speakers.end() && tempSpeakerGroup.speakers.size()>1)
			{
				for (set <int>::iterator si=tempSpeakerGroup.speakers.begin(),siEnd=tempSpeakerGroup.speakers.end(); si!=siEnd; si++)
				{
					if (in(*si,im->objectMatches)==im->objectMatches.end())
					{
						vector <cLocalFocus>::iterator lsi=in(*si);
						if (lsi!=localObjects.end() && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge))
						{
							pushLocalObjectOntoMatches(where,lsi,L"from current SG");
							lsOffsets.push_back((int)(lsi-localObjects.begin()));
						}
					}
				}
			}
			else if (find(nextNarrationSubjects.begin(),nextNarrationSubjects.end(),im->objectMatches[0].object)!=nextNarrationSubjects.end() && nextNarrationSubjects.size()>1)
			{
				for (vector <int>::iterator si=nextNarrationSubjects.begin(),siEnd=nextNarrationSubjects.end(); si!=siEnd; si++)
				{
					if (in(*si,im->objectMatches)==im->objectMatches.end())
					{
						vector <cLocalFocus>::iterator lsi=in(*si);
						if (lsi!=localObjects.end() && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=-1000) // if <-1000, probably doesn't match gender
						{
							pushLocalObjectOntoMatches(where,lsi,L"from current SG nextNarrationSubjects");
							lsOffsets.push_back((int)(lsi-localObjects.begin()));
						}
					}
				}
			}
			else if (lastSG->speakers.find(im->objectMatches[0].object)!=lastSG->speakers.end() && lastSG->speakers.size()>1 && object>=0)
			{
				vector <cLocalFocus>::iterator lsi;
				wstring tmpstr;
				for (set <int>::iterator si=lastSG->speakers.begin(),siEnd=lastSG->speakers.end(); si!=siEnd; si++)
					if (in(*si,im->objectMatches)==im->objectMatches.end() && (lsi=in(*si))!=localObjects.end() && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && objects[object].matchGender(objects[*si]))
					{
						pushLocalObjectOntoMatches(where,lsi,L"from previous SG");
						lsOffsets.push_back((int)(lsi-localObjects.begin()));
					}
			}
    }
    // If in resolveSpeaker (not identifySpeakerGroups, so speaker groups are already established)
    // If there is still only one speaker found, and that speaker is in the last speaker group,
    // If the current group is a superset of the last speaker group,
    // add the other members of that speaker group. REF:SUBGROUPS
    int speakersIn=0;
    if (currentSpeakerGroup && currentSpeakerGroup<speakerGroups.size())
		{
			set <int> *speakers=(speakerGroups[currentSpeakerGroup].groupedSpeakers.size()) ? &speakerGroups[currentSpeakerGroup].groupedSpeakers:&speakerGroups[currentSpeakerGroup].speakers;
      for (set <int>::iterator si=speakers->begin(),siEnd=speakers->end(); si!=siEnd; si++)
        if (in(*si,im->objectMatches)!=im->objectMatches.end())
					speakersIn++;			
			if (speakersIn==1)
			{
				for (set <int>::iterator si=speakers->begin(),siEnd=speakers->end(); si!=siEnd; si++)
				{
					if (in(*si,im->objectMatches)==im->objectMatches.end())
					{
						vector <cLocalFocus>::iterator lsi;
						if (!pushObjectIntoLocalFocus(where, *si, definitelySpeaker, false, inPrimaryQuote, inSecondaryQuote, L"from previous SG SUBGROUP",lsi)) continue;
						if (lsi->om.salienceFactor>=-1000 && 
							  (object<0 || objects[object].objectClass!=GENDERED_GENERAL_OBJECT_CLASS || speakerGroups[currentSpeakerGroup].povSpeakers.find(lsi->om.object)==speakerGroups[currentSpeakerGroup].povSpeakers.end()))
						{
							pushLocalObjectOntoMatches(where,lsi,L"from previous SG SUBGROUP");
							lsOffsets.push_back((int)(lsi-localObjects.begin()));
						}
						else if (debugTrace.traceSpeakerResolution)
						{
							wstring tmpstr;
							lplog(LOG_RESOLUTION,L"%06d:%s rejected even though it is in the previous SG SUBGROUP because it has a too negative salience or it is POV (%d).",where,objectString(lsi->om,tmpstr,true).c_str(),lsi->om.salienceFactor);
						}
					}
				}
			}
		}
	  if (!inPrimaryQuote && !inSecondaryQuote && !(m[where].objectRole&PRIMARY_SPEAKER_ROLE)) 
			preferSubgroupMatch(where,(m[where].getObject()>=0) ? objects[m[where].getObject()].objectClass:-1,m[where].word->second.inflectionFlags,false,-1,!mixedPlurality);
		eliminateBodyObjectRedundancy(where,m[where].objectMatches);
  }
}

// use probability of word relations to prefer some to other matches
void cSource::preferRelatedObjects(int where)
{ LFS
	if (m[where].getRelVerb()<0) return;
	int wordsWithFrequency=0,offsetWithFrequency=-1,highestFrequency=-1,lowestFrequency=-1;
	tIWMM verb=m[m[where].getRelVerb()].getMainEntry(),winner;
	if (verb->first==L"am") // too common; unreliable without usage of adjectives or adverbs (later?)
		return;
	cSourceWordInfo::cRMap *rm;
	int relationType=(m[where].objectRole&SUBJECT_ROLE) ? SubjectWordWithVerb : DirectWordWithVerb;
	vector <int> frequencies;
	for (int I=0; I<(signed)m[where].objectMatches.size(); I++)
	{
		tIWMM w=resolveToClass(objects[m[where].objectMatches[I].object].originalLocation);
		cSourceWordInfo::cRMap::tIcRMap tr=tNULL;
		if ((rm=w->second.relationMaps[relationType]) && ((tr=rm->r.find(verb->first))!=rm->r.end()))
		{
			wordsWithFrequency++;
			offsetWithFrequency=I;
			winner=w;
			highestFrequency=max(highestFrequency,tr->second.frequency);
			lowestFrequency=(lowestFrequency<0) ? tr->second.frequency : min(lowestFrequency,tr->second.frequency);
		}
		else
			frequencies.push_back(-1);
	}
	if (wordsWithFrequency==1 && objects[m[where].objectMatches[offsetWithFrequency].object].end-objects[m[where].objectMatches[offsetWithFrequency].object].begin==1 &&
		  winner->first==L"that")
		return; // 'that' for some reason is erroneously high in relations
	wstring tmpstr,tmpstr2;
	if (debugTrace.traceSpeakerResolution && wordsWithFrequency>1)
		lplog(LOG_RESOLUTION,L"%06d:frequencyAnalysis [%s] of relatedObjects=%s -> %s wordsWithFrequency=%d offsetWithFrequency=%d highestFrequency=%d lowestFrequency=%d.",where,
		      getRelStr(relationType),whereString(where,tmpstr,true).c_str(),verb->first.c_str(),
					wordsWithFrequency,offsetWithFrequency,highestFrequency,lowestFrequency);
	if (debugTrace.traceSpeakerResolution && wordsWithFrequency==1)
		lplog(LOG_RESOLUTION,L"%06d:frequencyAnalysis [%s] of relatedObjects=%s winner[%s] -> %s",where,
		      getRelStr(relationType),whereString(where,tmpstr,true).c_str(),
					objectString(m[where].objectMatches[offsetWithFrequency].object,tmpstr2,true).c_str(),verb->first.c_str());
	if (wordsWithFrequency==1)
	{
		m[where].objectMatches.clear();
		m[where].objectMatches.push_back(cOM(m[where].objectMatches[offsetWithFrequency].object,SALIENCE_THRESHOLD));
	}
}

// use partial_sort?
// in secondary quotes, inPrimaryQuote=false
bool cSource::chooseBest(int where,bool definitelySpeaker,bool inPrimaryQuote,bool inSecondaryQuote,bool resolveForSpeaker,bool mixedPlurality)
{ LFS
  // get three highest choices > 0 in ls
	int beginPosition=where;
	while (beginPosition>0 && m[beginPosition].principalWherePosition<0) beginPosition--;
	bool physicallyEvaluated,physicallyPresent=physicallyPresentPosition(where,beginPosition,physicallyEvaluated,false);
	bool genericGender=(m[where].word->second.flags&cSourceWordInfo::genericGenderIgnoreMatch) && (m[where].endObjectPosition-m[where].beginObjectPosition)==2 && m[m[where].beginObjectPosition].word->first==L"the";
	bool genericGenderOverride=false;
	// an introduction...  rule out all objects that are physically present or recently PP.  But this is not an unresolvable object either.
	// there lay the girl
	//bool newPhysicallyPresentObject=m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].word->first==L"lay" && !(m[where].objectRole&SUBJECT_ROLE);
  vector <cLocalFocus>::iterator highest[HIGHEST];
	int sf,I,ca;
  for (I=0; I<HIGHEST; I++) highest[I]=localObjects.end();
  vector <cWordMatch>::iterator im=m.begin()+where;
  int object=im->getObject();
  bool neuter=(object>=0 && objects[object].neuter) || (im->word->second.inflectionFlags&NEUTER_GENDER)==NEUTER_GENDER;
  vector <cSpeakerGroup>::iterator csg=speakerGroups.begin()+currentSpeakerGroup;
	for (vector <cLocalFocus>::iterator lsi=localObjects.begin(), lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
  {
		if (lsi->om.object<=1) continue; // never let anything match Narrator or Audience objects
		if (!lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) || lsi->om.object==object || 
			  (object>=0 && objects[object].objectClass==PRONOUN_OBJECT_CLASS && objects[lsi->om.object].objectClass==PRONOUN_OBJECT_CLASS)) continue; // GO_NEUTRAL
    int lso=lsi->om.object,lsow=objects[lso].getOwnerWhere();
		bool inEmbeddedStory=(m[where].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE)!=0;
    if (resolveForSpeaker && usePIS)
    {
			bool embeddedNotFound=true;
			if (inEmbeddedStory && currentEmbeddedSpeakerGroup>=0 && currentEmbeddedSpeakerGroup<(int)csg->embeddedSpeakerGroups.size())
			{
			  vector <cSpeakerGroup>::iterator esg=csg->embeddedSpeakerGroups.begin()+currentEmbeddedSpeakerGroup;
				int lsoo;
				// make sure object is in embedded speaker group
				embeddedNotFound=((esg->speakers.find(lso)==esg->speakers.end() && in(lso,esg->replacedSpeakers)==esg->replacedSpeakers.end() && !objects[lso].numDefinitelyIdentifiedAsSpeakerInSection) &&
						(lsow<0 || (lsoo=m[lsow].getObject())<0 ||
						(esg->speakers.find(lsoo)==esg->speakers.end() && in(lsoo,esg->replacedSpeakers)==esg->replacedSpeakers.end() && !objects[lsoo].numDefinitelyIdentifiedAsSpeakerInSection)));
			}
			if (embeddedNotFound)
			{
				int lsoo;
				// make sure object is in speaker group
				if ((csg->speakers.find(lso)==csg->speakers.end() && in(lso,csg->replacedSpeakers)==csg->replacedSpeakers.end() && !objects[lso].numDefinitelyIdentifiedAsSpeakerInSection) &&
						(lsow<0 || (lsoo=m[lsow].getObject())<0 ||
						(csg->speakers.find(lsoo)==csg->speakers.end() && in(lsoo,csg->replacedSpeakers)==csg->replacedSpeakers.end() && !objects[lsoo].numDefinitelyIdentifiedAsSpeakerInSection)))
					continue;
			}
    }
		if (resolveForSpeaker && lsi->notSpeaker) continue;
    // don't allow an object to match its owner
    if (lsow>=0 && (m[lsow].getObject()==object || in(object,m[lsow].objectMatches)!=m[lsow].objectMatches.end()))
      continue;
		//if (newPhysicallyPresentObject && lsi->physicallyPresent)
		//	continue;
		// don't allow a body object to match if not neutral and owned and (its owner is negative OR is first or second person)
		vector <cLocalFocus>::iterator olsi;
		if (objects[lso].objectClass==BODY_OBJECT_CLASS && !neuter && lsow>=0 && 
			  (((m[lsow].objectMatches.size()==1 && (olsi=in(m[lsow].objectMatches[0].object))!=localObjects.end() && olsi->om.salienceFactor<0)) ||
				 (m[lsow].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON))!=0))
			continue;
		sf=lsi->om.salienceFactor;
		ca=lsi->numMatchedAdjectives; 
		// if numMatchedAdjectives>0 and not physicallyPresent, but matching to a physically present position, and salience<0, continue.
		// otherwise the matched adjectives would allow this non-present object to leap into physical presence.
		// If this is an embedded story, and physically present, then also require the localObject to have been in the embedded story,
		//   otherwise it will block local objects introduced in the speech prior to the embedded story.
		bool loInEmbeddedStory=lsi->lastWhere>=0 && (m[lsi->lastWhere].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE);
		// if notSpeaker && adjective, then don't match (lady==lady's)@18246 should be matched to Mrs. Vandermeyer
		if (ca && !lsi->physicallyPresent && physicallyPresent && (sf<0 || (lsi->notSpeaker && lsi->lastWhere>=0 && (m[lsi->lastWhere].flags&cWordMatch::flagAdjectivalObject))) && 
			 (loInEmbeddedStory || !inEmbeddedStory))
			continue;
		// if this object has a negative salience and positive common adjectives (so it is an old object that wouldn't
		// have been picked except that it has common adjectives), but it has a match on its original location that is
		// also in local salience but has a positive salience factor, ignore this object because it could have been replaced by its matching
		// object.  If it had been replaced in its original context, by including it here the object will be out of context
		// and including it will reintroduce it when it has already been replaced by something else.
		vector <cLocalFocus>::iterator rlsi;
		if (sf<0 && ca>0 && m[objects[lso].originalLocation].objectMatches.size()==1 && (rlsi=in(m[objects[lso].originalLocation].objectMatches[0].object))!=localObjects.end() &&
			  rlsi->om.salienceFactor>SALIENCE_THRESHOLD)
			continue;
		// still allow even if generic gender if the matching object is an exact match
		if (genericGender && ca>0 && sf<0)
		{
			bool allMatched=objects[lsi->om.object].len()==objects[m[where].getObject()].len();
			int J;
			for (I=objects[lsi->om.object].begin,J=objects[m[where].getObject()].begin; I<objects[lsi->om.object].end && allMatched; I++,J++)
				allMatched=(m[I].word==m[J].word);
			if (allMatched) 
			{
				genericGender=false;
				genericGenderOverride=true;
				// this will be nullified if there is any other plausible physically present object
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:genericGender override",where);
			}
		}
		// all wordOrderWords except for "other" and "another" are short-range - make minimum salience much greater
		//    but if salience is independent, then we have already gone through one try with no matches...
		// only one common adjective is not enough to overcome negative salience
		// also no adjective and just a common generic gender head is also not enough to overcome negative salience (the man)
		// this prevents objects that have been introduced well before from 'jumping' into the next section
		// a new voice - don't allow unresolvable objects to jump into new sections - they are short range
		int ms=(ca<=1 || (object>=0 && objects[object].getOwnerWhere()<-3 && !cLocalFocus::salienceIndependent(quoteIndependentAge)) || (ca==2 && genericGender) ||
						(objects[lso].objectClass==BODY_OBJECT_CLASS && objects[object].objectClass==BODY_OBJECT_CLASS) ||
			      (unResolvablePosition(objects[lso].begin) && objects[lso].objectClass==BODY_OBJECT_CLASS)) ? 0 : (MINIMUM_SALIENCE_WITH_MATCHED_ADJECTIVES+ca*MORE_SALIENCE_PER_ADJECTIVE);
		// when speaker groups are established, in quotes, don't allow a too far off pronoun to match a single observer [Tommy shouldn't match 'they'] 24453
		// this is because of another change which allows observers to have been mentioned inQuote, to allow people to talk about observers
		if (inPrimaryQuote && currentSpeakerGroup<speakerGroups.size() && 
			  find(speakerGroups[currentSpeakerGroup].observers.begin(),speakerGroups[currentSpeakerGroup].observers.end(),lsi->om.object)!=speakerGroups[currentSpeakerGroup].observers.end())
			ms=0;
		if (ca && sf<ms)
			continue;
		// this purposely excludes the 'quoteIndependentAge' option - otherwise 'a woman[17263]' in quote matches to 'the elderly woman[18618]', out of quote
		// if out of quote, can match inQuote or outOfQuote.  If occurred only inQuote, then only match inQuote.
		bool includeInSalience=lsi->occurredInPrimaryQuote==objectToBeMatchedInQuote || lsi->occurredOutsidePrimaryQuote; //lsi->occurredOutsidePrimaryQuote!=lsi->objectToBeMatchedInQuote;
		for (int h=0; h<HIGHEST; h++)
      if (highest[h]==localObjects.end() || (ca>highest[h]->numMatchedAdjectives  && sf>ms && includeInSalience) ||  
				 (ca==highest[h]->numMatchedAdjectives && sf>highest[h]->om.salienceFactor))
			{
				for (I=HIGHEST-1; I>h; I--) highest[I]=highest[I-1];
				highest[h]=lsi;
				break;
	    }
  }
	if (highest[0]!=localObjects.end())
	{
		if (genericGenderOverride && highest[0]->numMatchedAdjectives>0 && highest[0]->om.salienceFactor<0)
		{
			int h=1;
			while (h<HIGHEST && highest[h]->numMatchedAdjectives>0 && highest[h]->om.salienceFactor<0) h++;
			// any other physicallyPresent object having sf>0?
			if (h<HIGHEST && highest[h]!=localObjects.end() && highest[h]->physicallyPresent && highest[h]->om.salienceFactor>0)
			{
				memmove(highest,highest+h,(HIGHEST-h-1)*sizeof(highest[0]));
				highest[HIGHEST-h]=localObjects.end();
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:genericGender override nullified",where);
			}
		}
		wstring tmpstr;
		int h=0;
		while ((ca=highest[h]->numMatchedAdjectives) && highest[h]->om.salienceFactor<(MINIMUM_SALIENCE_WITH_MATCHED_ADJECTIVES+ca*MORE_SALIENCE_PER_ADJECTIVE))
		{
			lplog(LOG_RESOLUTION,L"%06d:NOTE ignoring object %s having CA of %d but sf of %d (salience too low).",where,objectString(highest[h]->om.object,tmpstr,false).c_str(),ca,highest[h]->om.salienceFactor);
			h++;
		}
		for (h++; h<HIGHEST; h++)
			if (highest[h]!=localObjects.end() && highest[h]->numMatchedAdjectives<ca)
				highest[h]=localObjects.end();
	}
	bool atLeastOneReference = false;
  int highestsf=(highest[0]!=localObjects.end()) ? highest[0]->om.salienceFactor : 0,highestActualsf=-1;
  bool isPlural=(object>=0 && objects[object].plural) || (im->word->second.inflectionFlags&PLURAL_OWNER)==PLURAL_OWNER;
  vector <cOM> objectMatches=im->objectMatches;
  im->objectMatches.clear();
  // if choosing between two speakers
	if (highest[0]!=localObjects.end() && highest[1]!=localObjects.end() &&
      highest[0]->om.salienceFactor>=SALIENCE_THRESHOLD && highest[1]->om.salienceFactor>=SALIENCE_THRESHOLD)
	{
		int audiencePosition;
		// one is audience and the other one is not
		if (resolveForSpeaker && lastOpeningPrimaryQuote>=0 && 
			  (audiencePosition=m[lastOpeningPrimaryQuote].audiencePosition)>=0 && audiencePosition!=m[lastOpeningPrimaryQuote].speakerPosition && 
			  (m[audiencePosition].getObject()==highest[0]->om.object || m[audiencePosition].getObject()==highest[1]->om.object))
		{
			if (m[audiencePosition].getObject()==highest[0]->om.object)
				highest[0]=highest[1];
			highest[1]=localObjects.end();
			wstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Non-audience speaker preference forced %s (audience@%d:%d).",where,objectString(highest[0]->om,tmpstr,true).c_str(),lastOpeningPrimaryQuote,m[lastOpeningPrimaryQuote].audiencePosition);
		}
		// one has lastSubject set and the other doesn't, then choose only the lastSubject
		else if (resolveForSpeaker && highest[1]->om.salienceFactor>highest[0]->om.salienceFactor/2 && (highest[0]->lastSubject ^ highest[1]->lastSubject))
		{
			if (highest[1]->lastSubject) highest[0]=highest[1];
			highest[1]=localObjects.end();
			wstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Last subject choice forced %s.",where,objectString(highest[0]->om,tmpstr,true).c_str());
		}
		/*
		// if choosing between two speakers, and quote has embeddedStory and the quote before also has embedded story, then prefer the previous speaker
		else if (resolveForSpeaker && 
				(m[lastOpeningPrimaryQuote].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers) && previousPrimaryQuote>=0 && (m[previousPrimaryQuote].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers) &&
				m[previousPrimaryQuote].objectMatches.size()==1 &&
				((highest[0]->om.object==m[previousPrimaryQuote].objectMatches[0].object) ^ (highest[1]->om.object==m[previousPrimaryQuote].objectMatches[0].object)))
		{
			if (highest[1]->om.object==m[previousPrimaryQuote].objectMatches[0].object) highest[0]=highest[1];
			highest[1]=localObjects.end();
			wstring tmpstr;
			if (t.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Last embedded story speaker choice @%d forced %s.",where,previousPrimaryQuote,objectString(highest[0]->om,tmpstr,true).c_str());
		}
		*/
		// if where is a OBJECT_ROLE and relSubject>=0 and there is highest[0] and highest[1], and prefer the lastWhere having the same subject.
		else if ((m[where].objectRole&OBJECT_ROLE) && m[where].relSubject>=0 && highest[0]->lastWhere>=0 && highest[1]->lastWhere>=0 &&
				((m[highest[0]->lastWhere].relSubject==m[where].relSubject) ^ (m[highest[1]->lastWhere].relSubject==m[where].relSubject)))
		{
			if (m[highest[1]->lastWhere].relSubject==m[where].relSubject)
				highest[0]=highest[1];
			highest[1]=localObjects.end();
			wstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Parallel object with same subject forced %s.",where,objectString(highest[0]->om,tmpstr,true).c_str());
		}
		// if this is an ambiguous HAIL and does not include the last definite quote match 
		if (previousPrimaryQuote>=0 && (m[where].objectRole&HAIL_ROLE))
		{
			int trackBack=previousPrimaryQuote;
			while (m[trackBack].quoteBackLink>=0) trackBack=m[trackBack].quoteBackLink;
			vector <cLocalFocus>::iterator ambiguousHail;
			/*
			if (where==78019)
			{
				wstring tmpstr,tmpstr2,tmpstr3;
				lplog(LOG_RESOLUTION,L"trackBack=%d previousPrimaryQuote=%d %s %d!=%d  %d==%d  %s %s %s",trackBack,previousPrimaryQuote,
							speakerResolutionFlagsString(m[trackBack].flags,tmpstr3).c_str(),
							m[previousPrimaryQuote].getQuoteForwardLink(),lastOpeningPrimaryQuote,
							m[trackBack].nextQuote,lastOpeningPrimaryQuote,
							(m[previousPrimaryQuote].flags&cWordMatch::flagLastContinuousQuote) ? L"true":L"false",
							roleString(where,tmpstr).c_str(),
							objectString(m[previousPrimaryQuote].objectMatches,tmpstr2,true).c_str());
			}
			*/
			if (((m[trackBack].flags&cWordMatch::flagDefiniteResolveSpeakers) || 
				   (m[trackBack].speakerPosition>=0 && m[m[trackBack].speakerPosition].getObject()>=0 && objects[m[m[trackBack].speakerPosition].getObject()].objectClass==NAME_OBJECT_CLASS)) && 
				m[previousPrimaryQuote].getQuoteForwardLink()!=lastOpeningPrimaryQuote && // just in case this is introduced in the same paragraph but in a second quote
				m[trackBack].nextQuote==lastOpeningPrimaryQuote && !(m[previousPrimaryQuote].flags&cWordMatch::flagLastContinuousQuote) && // quote immediately before - no intervening paragraph
				m[previousPrimaryQuote].objectMatches.size()==1 && (ambiguousHail=in(m[previousPrimaryQuote].objectMatches[0].object))!=localObjects.end() &&
				ambiguousHail->om.salienceFactor>=SALIENCE_THRESHOLD)
			{
				highest[0]=ambiguousHail;
				for (int h=1; h<HIGHEST && highest[h]!=localObjects.end(); h++) highest[h]=localObjects.end();
				wstring tmpstr;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Ambiguous hail disagreeing with last definite speaker (@%d) resolved to %s.",where,previousPrimaryQuote,objectString(highest[0]->om,tmpstr,true).c_str());
			}
			// if this is an ambiguous HAIL - delete the last definite quote audience match 
			if ((m[trackBack].flags&(cWordMatch::flagHailedResolveAudience|cWordMatch::flagSpecifiedResolveAudience)) && 
					m[previousPrimaryQuote].getQuoteForwardLink()!=lastOpeningPrimaryQuote && // just in case this is introduced in the same paragraph but in a second quote
					m[trackBack].nextQuote==lastOpeningPrimaryQuote && !(m[previousPrimaryQuote].flags&cWordMatch::flagLastContinuousQuote) && // quote immediately before - no intervening paragraph
					m[previousPrimaryQuote].audienceObjectMatches.size()==1 && (ambiguousHail=in(m[previousPrimaryQuote].audienceObjectMatches[0].object))!=localObjects.end() &&
					ambiguousHail->om.salienceFactor>=SALIENCE_THRESHOLD)
			{
				if (highest[0]==ambiguousHail) highest[0]=highest[1];
				for (int h=1; h<HIGHEST && highest[h]!=localObjects.end(); h++) highest[h]=localObjects.end();
				wstring tmpstr;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Ambiguous hail agreeing with last definite audience (@%d) resolved to %s.",where,previousPrimaryQuote,objectString(highest[0]->om,tmpstr,true).c_str());
			}
		}
		// if where is a GENDERED_BODY_OBJECT, ownerless and belongs to a compound and the previous compound is a GENDERED_BODY object and has an owner,
		//   prefer the highest[0] or highest[1] that matches this owner, if any.
		int ow;
		if (object>=0 && objects[object].objectClass==BODY_OBJECT_CLASS && objects[object].getOwnerWhere()<0 && m[where].previousCompoundPartObject>=0 &&
				m[m[where].previousCompoundPartObject].getObject()>=0 && (ow=objects[m[m[where].previousCompoundPartObject].getObject()].getOwnerWhere())>=0 &&
				(in(highest[0]->om.object,ow) ^ in(highest[1]->om.object,ow)))
		{
			if (in(highest[1]->om.object,ow))
				highest[0]=highest[1];
			highest[1]=localObjects.end();
			wstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Compound gendered object inherits previous compound object's owner %s.",where,objectString(highest[0]->om,tmpstr,true).c_str());
		}
		/*
		if (where==70409)
		{
			wstring tmpstr,tmpstr2,sRole;
			lplog(LOG_RESOLUTION,L"%06d:beforePreviousSpeakers=%s previousSpeakers=%s resolveForSpeaker=%s role=%s lastOpeningPrimaryQuote=%d fl=%s previousPrimaryQuote=%d m[previousPrimaryQuote].getQuoteForwardLink()=%d whereSubjectsInPreviousUnquotedSection=%d.",
				where,objectString(beforePreviousSpeakers,tmpstr).c_str(),objectString(previousSpeakers,tmpstr2).c_str(),(resolveForSpeaker) ? L"true":L"false",
					roleString(where,sRole).c_str(),lastOpeningPrimaryQuote,(m[lastOpeningPrimaryQuote].flags&cWordMatch::flagForwardLinkResolveSpeakers) ? L"true":L"false",
					previousPrimaryQuote,m[previousPrimaryQuote].getQuoteForwardLink(),whereSubjectsInPreviousUnquotedSection);
		}
		*/
		if ((m[where].objectRole&HAIL_ROLE) && previousPrimaryQuote>=0 && m[previousPrimaryQuote].getQuoteForwardLink()==lastOpeningPrimaryQuote && m[previousPrimaryQuote].audienceObjectMatches.size()==1)
		{
			wstring tmpstr;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Prefer previous audience %s in forwardLink.",where,objectString(m[previousPrimaryQuote].audienceObjectMatches,tmpstr,true).c_str());
			if (highest[0]->om.object==m[previousPrimaryQuote].audienceObjectMatches[0].object)
				highest[1]=localObjects.end();
			if (highest[1]->om.object==m[previousPrimaryQuote].audienceObjectMatches[0].object)
			{
				highest[0]=highest[1];
				highest[1]=localObjects.end();
			}
		}	  	  
		// this is for speakers only (he said, she said) that are ambiguous between the speakers in the speakerGroup because they are the same gender.
		// if speakerGroups defined and definitelySpeaker, and resolveForSpeaker, and has >1 localObjects having salience > 0, 
		// and one is in the previous audience OR one is the lastSubject, pick that one.
		if (((resolveForSpeaker && !(m[where].objectRole&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE))) || 
			  ((m[where].objectRole&HAIL_ROLE) && !(m[where].objectRole&IN_SECONDARY_QUOTE_ROLE) && beforePreviousSpeakers.size()<=1)) &&
			   currentSpeakerGroup<speakerGroups.size() && previousPrimaryQuote>=0 &&
			   m[previousPrimaryQuote].getQuoteForwardLink()!=lastOpeningPrimaryQuote && // beforePreviousSpeakers is not useful if a forward link because it is set in the first link
				!(m[lastOpeningPrimaryQuote].flags&cWordMatch::flagForwardLinkResolveSpeakers) && // must have both this AND the previous test (for HAIL and SPEAKER_ROLE)
				(beforePreviousSpeakers.size() || 
				 (subjectsInPreviousUnquotedSectionUsableForImmediateResolution && subjectsInPreviousUnquotedSection.size()==1 && m[whereSubjectsInPreviousUnquotedSection].objectMatches.size()<=1 &&
				// subjectsInPreviousUnquotedSection must not be plural!
				  (whereSubjectsInPreviousUnquotedSection<0 || m[whereSubjectsInPreviousUnquotedSection].getObject()<0 || 
					 objects[m[whereSubjectsInPreviousUnquotedSection].getObject()].objectClass==BODY_OBJECT_CLASS || 
					 !objects[m[whereSubjectsInPreviousUnquotedSection].getObject()].plural))))
		{
			wstring tmpstr,tmpstr2,sRole;
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:beforePreviousSpeakers=%s previousSpeakers=%s resolveForSpeaker=%s role=%s lastOpeningPrimaryQuote=%d fl=%s previousPrimaryQuote=%d m[previousPrimaryQuote].getQuoteForwardLink()=%d.",
					where,objectString(beforePreviousSpeakers,tmpstr).c_str(),objectString(previousSpeakers,tmpstr2).c_str(),(resolveForSpeaker) ? L"true":L"false",
						m[where].roleString(sRole).c_str(),lastOpeningPrimaryQuote,(m[lastOpeningPrimaryQuote].flags&cWordMatch::flagForwardLinkResolveSpeakers) ? L"true":L"false",
						previousPrimaryQuote,m[previousPrimaryQuote].getQuoteForwardLink());
			//bool hailPreviousSpeaker=false;
			for (int h=0; h<HIGHEST; h++)
			{
				if (highest[h]!=localObjects.end() && highest[h]->om.salienceFactor>=SALIENCE_THRESHOLD && 
						find(previousSpeakers.begin(),previousSpeakers.end(),highest[h]->om.object)!=previousSpeakers.end() &&
						previousSpeakers.size()==1 && (m[where].objectRole&HAIL_ROLE))
				{
					highest[h]->om.salienceFactor+=10000;
					highest[h]->res+=L"+RECENT[+10000]";
				}
				if (highest[h]!=localObjects.end() && highest[h]->om.salienceFactor>=SALIENCE_THRESHOLD && 
					(find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),highest[h]->om.object)!=beforePreviousSpeakers.end() ||
					 (subjectsInPreviousUnquotedSectionUsableForImmediateResolution && 
						find(subjectsInPreviousUnquotedSection.begin(),subjectsInPreviousUnquotedSection.end(),highest[h]->om.object)!=subjectsInPreviousUnquotedSection.end())))
				{
					wstring tmpstr3;
					if (m[where].objectRole&HAIL_ROLE)
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:Removing audience=%s, previous subject objects=%s(%s).",
										where,objectString(beforePreviousSpeakers,tmpstr).c_str(),objectString(subjectsInPreviousUnquotedSection,tmpstr2).c_str(),
											(subjectsInPreviousUnquotedSectionUsableForImmediateResolution) ? L"USED":L"IGNORED");
						memmove(highest+h,highest+h+1,(HIGHEST-h-1)*sizeof(highest[0]));
						highest[HIGHEST-1]=localObjects.end();
						continue;
					}
					else
					{
						if (beforePreviousSpeakers.size()==speakerGroups[currentSpeakerGroup].speakers.size())
						{
							int trackBack=previousPrimaryQuote;
							while (m[trackBack].quoteBackLink>=0) trackBack=m[trackBack].quoteBackLink;
							vector <int>::iterator bs;
							if (((m[trackBack].flags&cWordMatch::flagDefiniteResolveSpeakers) || 
									 (m[trackBack].speakerPosition>=0 && m[m[trackBack].speakerPosition].getObject()>=0 && objects[m[m[trackBack].speakerPosition].getObject()].objectClass==NAME_OBJECT_CLASS)) && 
									m[previousPrimaryQuote].getQuoteForwardLink()!=lastOpeningPrimaryQuote && // just in case this is introduced in the same paragraph but in a second quote
									m[trackBack].nextQuote==lastOpeningPrimaryQuote && !(m[previousPrimaryQuote].flags&cWordMatch::flagLastContinuousQuote) && // quote immediately before - no intervening paragraph
									m[previousPrimaryQuote].objectMatches.size()==1 && 
									(bs=find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),m[previousPrimaryQuote].objectMatches[0].object))!=beforePreviousSpeakers.end())
								beforePreviousSpeakers.erase(bs);
						}
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:Found %s: removing non-audience, non-previous subject objects with audience=%s and previous subjects=%s(%s).",
										where,objectString(highest[h]->om,tmpstr3,true).c_str(),objectString(beforePreviousSpeakers,tmpstr).c_str(),objectString(subjectsInPreviousUnquotedSection,tmpstr2).c_str(),
											(subjectsInPreviousUnquotedSectionUsableForImmediateResolution) ? L"USED":L"IGNORED");
						for (int lh=0; lh<HIGHEST-1; )
							if (highest[lh]!=localObjects.end() && (highest[lh]->om.salienceFactor<SALIENCE_THRESHOLD ||
									(find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),highest[lh]->om.object)==beforePreviousSpeakers.end() &&
									 (!subjectsInPreviousUnquotedSectionUsableForImmediateResolution ||
										find(subjectsInPreviousUnquotedSection.begin(),subjectsInPreviousUnquotedSection.end(),highest[lh]->om.object)==subjectsInPreviousUnquotedSection.end()))))
							{
								if (debugTrace.traceSpeakerResolution)
									lplog(LOG_RESOLUTION,L"%06d:Removed non-audience, non-previous subject object %s.",where,objectString(highest[lh]->om,tmpstr,true).c_str());
								memmove(highest+lh,highest+lh+1,(HIGHEST-lh-1)*sizeof(highest[0]));
								highest[HIGHEST-1]=localObjects.end();
							}
							else 
								lh++;
					}
					break;
				}
			}
		}
	}
	if (highest[1]!=localObjects.end() && isGroupJoiner(m[where].word))
	{
		int lastPhysicallyManifested=-1;
		vector <cLocalFocus>::iterator which=localObjects.end();
		for (int h=0; h<HIGHEST && highest[h]!=localObjects.end() && highest[h]->om.salienceFactor>=SALIENCE_THRESHOLD; h++)
			if (objects[highest[h]->om.object].firstPhysicalManifestation>lastPhysicallyManifested)
				lastPhysicallyManifested=objects[(which=highest[h])->om.object].firstPhysicalManifestation;
		if (which!=localObjects.end())
		{
			highest[0]=which;
			highest[1]=localObjects.end();
		}
	}
	// if this is a physically present position, and more than one object is matched, prefer only physically present local salience
  if (highest[1]!=localObjects.end() && ((physicallyPresent && physicallyEvaluated) || (m[where].objectRole&HAIL_ROLE)))
	{
		bool atLeastOnePhysicallyPresentObject=false,atLeastOneTPPO=false;
		for (int h=0; h<HIGHEST && !atLeastOnePhysicallyPresentObject; h++)
		{
		  if (highest[h]!=localObjects.end() && highest[h]->om.salienceFactor>=SALIENCE_THRESHOLD && highest[h]->physicallyPresent)
			{
				atLeastOnePhysicallyPresentObject=true;
				atLeastOneTPPO|=highest[h]->whereBecamePhysicallyPresent>=0;
			}
		}
		if (atLeastOnePhysicallyPresentObject)
			for (int h=0; h<HIGHEST-1; )
				if (highest[h]!=localObjects.end() && (!highest[h]->physicallyPresent || (highest[h]->whereBecamePhysicallyPresent<0 && atLeastOneTPPO)))
				{
					wstring tmpstr;
			    if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Removed non-physically present object %s.",where,objectString(highest[h]->om,tmpstr,true).c_str());
					memmove(highest+h,highest+h+1,(HIGHEST-h-1)*sizeof(highest[0]));
					highest[HIGHEST-1]=localObjects.end();
				}
				else 
					h++;
	}
	// if speakerGroups defined, and subject has POV status, and is a PRONOUN_CLASS, and a pov has a salience>SALIENCE_THRESHOLD, limit to this speaker.
	//  he thought there must be about four or five people[man,voice] seated round a long table
	if (currentSpeakerGroup<speakerGroups.size() && (m[where].objectRole&POV_OBJECT_ROLE) && speakerGroups[currentSpeakerGroup].povSpeakers.size())
	{
		// if pov defined, and pronoun subject has POV status, and pov is singular and subject is plural, reject POV status.  {they}
		// He[tommy] looked confidently round , and was glad {they[tommy,boris]} could not hear the persistent beating of his[tommy] heart[tommy] which gave the lie to his[tommy] words .
		bool atLeastOnePOVObject=false;
		for (int h=0; h<HIGHEST && !atLeastOnePOVObject; h++)
		  atLeastOnePOVObject=(highest[h]!=localObjects.end() && highest[h]->om.salienceFactor>=SALIENCE_THRESHOLD && speakerGroups[currentSpeakerGroup].povSpeakers.find(highest[h]->om.object)!=speakerGroups[currentSpeakerGroup].povSpeakers.end());
		if (atLeastOnePOVObject)
			for (int h=0; h<HIGHEST-1; )
				if (highest[h]!=localObjects.end() && (highest[h]->om.salienceFactor<SALIENCE_THRESHOLD || speakerGroups[currentSpeakerGroup].povSpeakers.find(highest[h]->om.object)==speakerGroups[currentSpeakerGroup].povSpeakers.end()))
				{
					wstring tmpstr;
			    if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Removed non-pov object %s.",where,objectString(highest[h]->om,tmpstr,true).c_str());
					memmove(highest+h,highest+h+1,(HIGHEST-h-1)*sizeof(highest[0]));
					highest[HIGHEST-1]=localObjects.end();
				}
				else 
					h++;
	}
	// the previous two selections may have changed highest list, so recompute highestsf.
  highestsf=(highest[0]!=localObjects.end()) ? highest[0]->om.salienceFactor : 0,highestActualsf=-1;
	int numGenderedObjects=0,numNeuterObjects=0;
  vector <int> lsOffsets;
  for (I=0; I<HIGHEST; I++)
  {
    if (highest[I]!=localObjects.end() && 
			  ((highest[I]->om.salienceFactor>=SALIENCE_THRESHOLD && 
				  highest[I]->om.salienceFactor>highestsf/2) ||
					// this second part only applies to objects picked because of commonAdjectives,
					// but having a low salience.  Otherwise these would never be picked at all, leading to nothing being picked.
				 (lsOffsets.empty() && (ca=highest[I]->numMatchedAdjectives)>0 && highest[I]->om.salienceFactor>=(MINIMUM_SALIENCE_WITH_MATCHED_ADJECTIVES+ca*MORE_SALIENCE_PER_ADJECTIVE))))
    {
			// object like 'the first man' has matched onto an implicit object like 'a knock on the door'
			// mark the object 'the first man' as unresolvable as if it had a determiner 'a' or 'an'
			if (implicitObject(highest[I]->lastWhere) && object>=0 && !objects[object].plural && objects[object].getOwnerWhere()<-1)
			{
				m[where].objectRole|=UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE;
				wstring tmpstr;
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Resolution from implicit object %s resulted in unresolvable status.",where,objectString(highest[I]->om,tmpstr,true).c_str());
				vector <cLocalFocus>::iterator lsi=highest[I];
				lsi->om.object=object;
				lsi->occurredInPrimaryQuote=inPrimaryQuote || inSecondaryQuote;
				lsi->occurredOutsidePrimaryQuote=!inPrimaryQuote && !inSecondaryQuote;
				lsi->occurredInSecondaryQuote=inSecondaryQuote;
				lsi->physicallyPresent|=physicallyPresent;
	      pushLocalObjectOntoMatches(highest[I]->lastWhere,lsi,L"implicit");
				return true;
			}
      highestActualsf=highest[I]->om.salienceFactor;
      pushLocalObjectOntoMatches(where,highest[I],L"chooseBest (4)");
	    vector <cObject>::iterator lso=objects.begin()+highest[I]->om.object;
			if (neuter && (lso->male || lso->female) && lso->getSubType()>=0 && !lso->numIdentifiedAsSpeaker && !lso->numDefinitelyIdentifiedAsSpeaker && !lso->PISSubject && !lso->PISHail && !lso->PISDefinite)
			{
				wstring tmpstr;
				lso->neuter=true;
				lso->male=lso->female=false;
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Narrowed place %s to neuter.",where,objectString(highest[I]->om,tmpstr,true).c_str());
			}
      lsOffsets.push_back((int)(highest[I]-localObjects.begin()));
      atLeastOneReference=true;
			if (objects[highest[I]->om.object].neuter)
				numNeuterObjects++;
			else
				numGenderedObjects++;
    }
  }
  if (!inPrimaryQuote && !(m[where].objectRole&PRIMARY_SPEAKER_ROLE)) 
		preferSubgroupMatch(where,(m[where].getObject()>=0) ? objects[m[where].getObject()].objectClass:-1,m[where].word->second.inflectionFlags,false,-1,!mixedPlurality);
	matchAdditionalObjectsIfPlural(where,isPlural,atLeastOneReference,physicallyEvaluated,physicallyPresent,
																 inPrimaryQuote,inSecondaryQuote,definitelySpeaker,mixedPlurality,
																 highest,numNeuterObjects,numGenderedObjects,lsOffsets);
	if (numNeuterObjects>=2 && numGenderedObjects==0 && neuter && !isPlural && m[where].getObject()>=0 && objects[m[where].getObject()].objectClass==PRONOUN_OBJECT_CLASS)
		preferRelatedObjects(where);
  // setting of lsiOffset is done at this location because objects are potentially pushed into focus above.
  // this eliminates searching when updatingLocalObject.
  for (unsigned int J=0; J<lsOffsets.size(); J++)
    objects[localObjects[lsOffsets[J]].om.object].lsiOffset=localObjects.begin()+lsOffsets[J];
  // prefer subgrouping with the last speaker group (prefer the smallest group, except if aged)
  // wait until now to update local objects because preferSubgroupMatch changes the objectsMatched.
	vector <cLocalFocus>::iterator lsi;
  for (vector <cOM>::iterator omi=im->objectMatches.begin(),omiEnd=im->objectMatches.end(); omi!=omiEnd; omi++)
    pushObjectIntoLocalFocus(where,omi->object,definitelySpeaker,false,inPrimaryQuote,inSecondaryQuote,L"from object matches", lsi);
  for (unsigned int J=0; J<lsOffsets.size(); J++)
    objects[localObjects[lsOffsets[J]].om.object].lsiOffset=cNULL;
  for (vector <cOM>::iterator omi=objectMatches.begin(),omiEnd=objectMatches.end(); omi!=omiEnd; omi++)
  {
    // erase location from *omi
    vector <cObject::cLocation>::iterator wloc=find(objects[omi->object].locations.begin(),objects[omi->object].locations.end(),cObject::cLocation(where));
    if (wloc!=objects[omi->object].locations.end())
      objects[omi->object].locations.erase(wloc);
    if (definitelySpeaker)
    {
      objects[omi->object].numIdentifiedAsSpeaker--;
      objects[omi->object].numIdentifiedAsSpeakerInSection--;
    }
    else
      objects[omi->object].numEncounters--;
  }
	// replace objects matched with replaced speaker
  if (usePIS) // resolveForSpeaker && 
    for (vector <cOM>::iterator omi=im->objectMatches.begin(),omiEnd=im->objectMatches.end(); omi!=omiEnd; omi++)
    {
			vector <cOM>::iterator rr; // replacement record
      if ((rr=in(omi->object,speakerGroups[currentSpeakerGroup].replacedSpeakers))!=speakerGroups[currentSpeakerGroup].replacedSpeakers.end())
			{
				followObjectChain(rr->salienceFactor);
        omi->object=rr->salienceFactor;
			}
    }
  if (im->objectMatches.size()==1 && !(objects[im->objectMatches[0].object].male ^ objects[im->objectMatches[0].object].female) &&
    !isPlural && highestActualsf>=SALIENCE_THRESHOLD)
    narrowGender(where,m[where].objectMatches[0].object);
  eliminateBodyObjectRedundancy(where,im->objectMatches);
  if (im->objectMatches.size()==1 && object>=0 && im->objectMatches[0].object!=object)
    replaceObjectInSection(where,im->objectMatches[0].object,object,L"chooseBest (5)");
  if (im->objectMatches.size()>1 && object>=0 && (m[where].endObjectPosition-m[where].beginObjectPosition)>1 && objects[object].associatedAdjectives.size())
	{
		moveNyms(where,im->objectMatches[0].object,object,L"multiAssociated");
		moveNyms(where,im->objectMatches[1].object,object,L"multiAssociated");
	}
	// if the object being matched is a body object and it has a single match, substitute
  if (im->objectMatches.size()==1 && object>=0 && !isPlural && highestActualsf>=SALIENCE_THRESHOLD && objects[object].objectClass==BODY_OBJECT_CLASS)
    genderedBodyPartSubstitution(where,object);
	// if the object being matched is a single gendered pronoun, and the object matched is a body object,
	// and it is owned by a gendered object which has salience, substitute
  if (im->objectMatches.size()==1 && object>=0 && !isPlural && highestActualsf>=SALIENCE_THRESHOLD && 
		  (objects[object].male ^ objects[object].female) && objects[object].objectClass==PRONOUN_OBJECT_CLASS &&
			objects[im->objectMatches[0].object].objectClass==BODY_OBJECT_CLASS && 
			objects[im->objectMatches[0].object].getOwnerWhere()>=0 &&
			((lsi=in(m[objects[im->objectMatches[0].object].getOwnerWhere()].getObject()))!=localObjects.end() ||
			 (m[objects[im->objectMatches[0].object].getOwnerWhere()].objectMatches.size()==1 && 
			  (lsi=in(m[objects[im->objectMatches[0].object].getOwnerWhere()].objectMatches[0].object))!=localObjects.end())) &&
			lsi->om.salienceFactor>0)
	{
		wstring tmpstr,tmpstr2;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:Substituted owner %s of body object %s.",where,objectString(lsi->om.object,tmpstr,true).c_str(),objectString(im->objectMatches[0].object,tmpstr,true).c_str());
		im->objectMatches[0].object=lsi->om.object;
		objects[lsi->om.object].locations.push_back(where);
	}
	if ((im->objectRole&POV_OBJECT_ROLE) && currentSpeakerGroup<speakerGroups.size() && im->objectMatches.size()==1 && 
		  speakerGroups[currentSpeakerGroup].speakers.find(im->objectMatches[0].object)!=speakerGroups[currentSpeakerGroup].speakers.end() &&
		  //speakerGroups[currentSpeakerGroup].observers.find(im->objectMatches[0].object)==speakerGroups[currentSpeakerGroup].observers.end() &&
		  speakerGroups[currentSpeakerGroup].povSpeakers.empty() &&
		  !objects[im->objectMatches[0].object].plural && !(im->flags&cWordMatch::flagInQuestion))
	{
		speakerGroups[currentSpeakerGroup].povSpeakers.insert(im->objectMatches[0].object);
		wstring tmpstr,tmpstr2;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:POVI speaker %s (6) inserted into speakerGroup %s.",where,objectString(im->objectMatches[0].object,tmpstr,true).c_str(),toText(speakerGroups[currentSpeakerGroup],tmpstr2));
	}
  return atLeastOneReference;
}

// the last noun in SUBJECT role in the current sentence gets a bump in salience - especially if there is only one
void cSource::adjustSaliencesBySubjectRole(int where,int lastBeginS1)
{ LFS
	bool syntheticSubject=false;
	if (lastBeginS1<0)
	{
		for (int I=where; I>=0 && !isEOS(I) && m[I].queryForm(quoteForm)<0; I--)
		{
			 if (m[I].objectRole&(SUBJECT_ROLE|NO_ALT_RES_SPEAKER_ROLE))
			 {
				 lastBeginS1=I;
				 syntheticSubject=true;
			 }
		}
		if (!syntheticSubject) return;
		
	}
  vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsEnd=localObjects.end(),lsiMatched;
  for (; lsi!=lsEnd; lsi++)
    objects[lsi->om.object].lsiOffset=lsi;

  int sf=1000,o,numSubjects=0;
	bool pluralAdjective=(m[where].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER))!=0;
  for (int I=lastBeginS1; I<where; I++)
    if ((m[I].objectRole&(SUBJECT_ROLE|NO_ALT_RES_SPEAKER_ROLE)) && !(m[I].objectRole&PREP_OBJECT_ROLE) && (o=m[I].getObject())>=0 &&
        (objects[o].objectClass==NAME_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
         objects[o].objectClass==BODY_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
         objects[o].objectClass==META_GROUP_OBJECT_CLASS ||
				 m[I].objectMatches.size()==1))
    {
      wstring tmp;
      sf+=500;
			// reject when the subject is an unowned body object and the adjective is single gendered.  
			// a hand fell on his lap.  the eyes flitted over his shirt.
			if (objects[o].objectClass==BODY_OBJECT_CLASS && objects[o].getOwnerWhere()==-1 && 
				  (m[where].word->second.inflectionFlags&(MALE_GENDER|FEMALE_GENDER)) &&
				  !pluralAdjective)
			{
				numSubjects=0;
				break;
			}
			// if this is a plural body object (eyes), then don't boost owner if where is plural, because eyes is actually correct...
			// his [eyes] opened to [their] fullest extent
			// where=='their' I=='eyes'
			if (((pluralAdjective && objects[o].plural) || m[I].objectMatches.empty()) && 
				  (lsi=objects[o].lsiOffset)!=cNULL && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=0)
      {
        lsi->om.salienceFactor+=sf;
        itos(L"+SUBJADJ[+",sf,lsi->res,L"]");
				numSubjects++;
				lsiMatched=lsi;
      }
			if (!pluralAdjective || !objects[o].plural)
				for (unsigned int J=0; J<m[I].objectMatches.size(); J++)
				{
					o=m[I].objectMatches[J].object;
					if ((lsi=objects[o].lsiOffset)!=cNULL && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=0)
					{
						lsi->om.salienceFactor+=sf;
						itos(L"+SUBJADJ[+",sf,lsi->res,L"]");
						numSubjects++;
						lsiMatched=lsi;
					}
					if (objects[o].objectClass==BODY_OBJECT_CLASS && objects[o].getOwnerWhere()>=0 && 
							(m[where].word->second.inflectionFlags&(MALE_GENDER|FEMALE_GENDER)) && m[objects[o].getOwnerWhere()].getObject()>=0 &&
							(lsi=objects[m[objects[o].getOwnerWhere()].getObject()].lsiOffset)!=cNULL && lsi->includeInSalience(objectToBeMatchedInQuote,quoteIndependentAge) && lsi->om.salienceFactor>=0)
					{
						lsi->om.salienceFactor+=sf;
						itos(L"+SUBJBOADJ[+",sf,lsi->res,L"]");
						numSubjects++;
						lsiMatched=lsi;
					}
				}
    }
  if (numSubjects==1 && !syntheticSubject)
  {
    lsiMatched->om.salienceFactor+=10000;
    lsiMatched->res+=L"+SUBJADJ[+10000]";
  }
  for (lsi=localObjects.begin(); lsi!=lsEnd; lsi++)
    objects[lsi->om.object].lsiOffset=cNULL;
}

int cSource::scanForPatternTag(int where, int tag)
{
	LFS
		if (where >= (int)m.size()) return -1;
	int maxLen = -1;
	cPatternMatchArray::tPatternMatch *maxpm = NULL;
	for (cPatternMatchArray::tPatternMatch *pm = m[where].pma.content, *pmend = pm + m[where].pma.count; pm != pmend; pm++)
		if (patterns[pm->getPattern()]->tags.find(tag) != patterns[pm->getPattern()]->tags.end() && pm->len > maxLen)
		{
			maxpm = pm;
			maxLen = pm->len;
		}
	return (maxLen < 0) ? -1 : (int)(maxpm - m[where].pma.content);
}

int cSource::scanForPatternElementTag(int where, int tag)
{
	LFS
	if (where >= (int)m.size()) return -1;
	for (int pemaPosition = m[where].beginPEMAPosition; pemaPosition >= 0; pemaPosition = pema[pemaPosition].nextByPosition)
		if (patterns[pema[pemaPosition].getParentPattern()]->getElement(pema[pemaPosition].getElement())->hasTag(tag))
			return pemaPosition;
	return -1;
}

void cSource::scanForLocation(bool check,bool &relAsObject,int &whereRelClause,int &pmWhere,int checkEnd)
{ LFS
		if (check)
		{
			pmWhere=scanForPatternTag(whereRelClause=checkEnd,REL_TAG);
			if (pmWhere<0) 
			{
				relAsObject=((pmWhere=scanForPatternTag(whereRelClause=checkEnd,SENTENCE_IN_REL_TAG))>=0);
				// whose is a adjectival relativizer - the only one, and so this is not a valid procedure (relAsObject) for it.
				if (pmWhere>=0 && m[whereRelClause].word->first==L"whose") pmWhere=-1;
			}
		}
		// what is not valid; 
		if (pmWhere>=0 && m[whereRelClause].word->first==L"what") pmWhere=-1;
}

bool cSource::physicallyPresentPosition(int where,bool &physicallyEvaluated)
{ LFS
	int beginEntirePosition=m[where].beginObjectPosition; // if this is an adjectival object 
  if (m[where].flags&cWordMatch::flagAdjectivalObject) 
		for (; beginEntirePosition>=0 && m[beginEntirePosition].principalWherePosition<0; beginEntirePosition--);
	return physicallyPresentPosition(where,beginEntirePosition,physicallyEvaluated,false);
}

// if an object is in a relative phrase, if the accompanying subjects are past perfect, then the object in the relative phrase (if when) is also past perfect, even if the object itself is
//   accompanied by a non-past perfect verb.
//  Sir James and his[st] young friends had been paying a call upon her[marguerite] , when she[marguerite] was suddenly stricken down and they[st,friends,servants,marguerite] had spent the night in the flat
bool cSource::accompanyingRolePP(int where)
{ LFS
	// find a previous subject before EOS.
	//   skip all consecutive SUBJECT_ROLES
	int I;
	// find relativizer
	for (I=where-1; I>0 && !isEOS(I) && m[I].queryWinnerForm(relativizerForm)<0; I--);
	if (I<0 || isEOS(I) || m[I].word->first!=L"when") return false;
	for (I=where-1; I>0 && !isEOS(I) && (m[I].objectRole&SUBJECT_ROLE); I--);
	if (I>0 && !isEOS(I)) 
	{
		for (; I>0 && !isEOS(I) && (!(m[I].objectRole&SUBJECT_ROLE) || m[I].getRelVerb()<0); I--);
		if (I>0 && !isEOS(I) && m[I].getRelVerb()>=0)
		{
			if ((m[m[I].getRelVerb()].verbSense&VT_TENSE_MASK)==VT_PAST_PERFECT)
				lplog(LOG_RESOLUTION,L"%d:accompanyingRolePP blocked physical presence because of relative clause & surrounding past perfect subject=%d, PP verb=%d.",where,I,m[I].getRelVerb());
			return (m[m[I].getRelVerb()].verbSense&VT_TENSE_MASK)==VT_PAST_PERFECT;
		}
	}
	// find subsequent subject before EOS at end of sentence.
	for (I=where+1; I<(signed)m.size() && !isEOS(I) && (m[I].objectRole&SUBJECT_ROLE); I++);
	if (I<(signed)m.size() && !isEOS(I)) 
	{
		for (; I<(signed)m.size() && !isEOS(I) && (!(m[I].objectRole&SUBJECT_ROLE) || m[I].getRelVerb()<0); I++);
		if (I<(signed)m.size() && !isEOS(I) && m[I].getRelVerb()>=0)
		{
			if ((m[m[I].getRelVerb()].verbSense&VT_TENSE_MASK)==VT_PAST_PERFECT)
				lplog(LOG_RESOLUTION,L"%d:accompanyingRolePP blocked physical presence because of relative clause & surrounding past perfect subject=%d, PP verb=%d.",where,I,m[I].getRelVerb());
			return (m[m[I].getRelVerb()].verbSense&VT_TENSE_MASK)==VT_PAST_PERFECT;
		}
	}
  return false;
}

bool cSource::physicallyPresentPosition(int where,int beginObjectPosition,bool &physicallyEvaluated,bool ignoreTense)
{ LFS
	if (beginObjectPosition<0) return false;
  vector <cWordMatch>::iterator im=m.begin()+beginObjectPosition;
	int at=im->principalWherePosition;
	if (at>=0 && m[at].getObject()>=0 && objects[m[at].getObject()].isTimeObject) 
	{
		//wstring tmpstr;
		//lplog(LOG_RESOLUTION,L"%06d:time object %s not present.",where,objectString(m[at].getObject(),tmpstr,true).c_str());
		return false;
	}
	//if (m[at].getObject()>=0 && objects[m[at].getObject()].isLocationObject) return true;
	if (at<0) at=im->principalWhereAdjectivalPosition;
	if (at < 0)
		return false;
	unsigned __int64 or=m[at].objectRole;
	__int64 flags=m[at].flags;
	physicallyEvaluated=(or&FOCUS_EVALUATED)!=0;
	if (or&SUBJECT_PLEONASTIC_ROLE) or&=~IS_OBJECT_ROLE; // if the subject of an object is pleonastic (there, etc), then ignore the verb being 'is'
	bool inQuote=((or&IN_PRIMARY_QUOTE_ROLE)!=0 && !(or&IN_EMBEDDED_STORY_OBJECT_ROLE)) || (or&IN_SECONDARY_QUOTE_ROLE); // embedded story will only be set in quote, but an embedded story is set in the past, so it is equivalent to out of quote.
	bool inQuestion=(flags&cWordMatch::flagInQuestion)!=0;
	bool inPStatement=(flags&cWordMatch::flagInPStatement)!=0;
	bool inInfinitivePhrase=(flags&cWordMatch::flagInInfinitivePhrase)!=0 && !(or&SENTENCE_IN_ALT_REL_ROLE);
	bool inRelClause=(or&SENTENCE_IN_REL_ROLE)!=0;
	//if (inRelClause && accompanyingRolePP(where))
	//	lplog(LOG_INFO,L"%d:pp connected to rel detected!",where);
	bool includeThoughInRelClause=inRelClause && m[where].principalWhereAdjectivalPosition<0 && 
			 m[where].queryForm(relativizerForm)<0 && m[where].queryForm(demonstrativeDeterminerForm)<0 &&
			 !(or&THINK_ENCLOSING_ROLE) && !(or&POSSIBLE_ENCLOSING_ROLE) && (or&FOCUS_EVALUATED) && !accompanyingRolePP(where);
	bool isAdjective=m[where].principalWhereAdjectivalPosition>=0 || (m[where].word->second.inflectionFlags&(SINGULAR_OWNER|PLURAL_OWNER)) ||
	  (m[where].flags&cWordMatch::flagNounOwner);
	// in the answer/appeal/letter/missive
	if (notPhysicallyPresentByMissive(where)) return false;
	bool notPhysicallyPresentObject=(m[where].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN) && 
		   ((m[where].getObject()>=0 && objects[m[where].getObject()].neuter && !objects[m[where].getObject()].isAgent(true)) || isInternalBodyPart(where));
	// a statement of state (he was) is not certain of physical presence
	// Conrad was undoubtedly the tenant of the house .
	/* Needs more work */
	vector <cLocalFocus>::iterator lsi;
	if ((or&(IN_PRIMARY_QUOTE_ROLE|IN_SECONDARY_QUOTE_ROLE))==0 && m[where].getRelVerb()>=0 && m[m[where].getRelVerb()].getMainEntry()->first==L"am" && (or&SUBJECT_ROLE) &&
			m[where].getObject()>=0 && objects[m[where].getObject()].objectClass==NAME_OBJECT_CLASS &&
			((lsi=in(m[where].getObject()))==localObjects.end() || !lsi->physicallyPresent) &&
		  m[m[where].getRelVerb()].getRelObject()>=0 && m[m[m[where].getRelVerb()].getRelObject()].getObject()>=0 && 
			((lsi=in(m[m[m[where].getRelVerb()].getRelObject()].getObject()))==localObjects.end() || !lsi->physicallyPresent))
	{
		// ignore POV objects
		if (currentSpeakerGroup>=speakerGroups.size() || speakerGroups[currentSpeakerGroup].povSpeakers.find(m[where].getObject())==speakerGroups[currentSpeakerGroup].povSpeakers.end())
		{
			// ignore descriptions
			// Mr. Julius P. Hersheimmer was a great deal younger than either Tommy or Tuppence had pictured him.
			int wo;
			if (m[where].getRelVerb()>=0 && (wo=m[m[where].getRelVerb()].getRelObject())>=0 && m[wo].endObjectPosition>=0 && m[wo].endObjectPosition<(signed)m.size() && m[m[wo].endObjectPosition].pma.queryPatternDiff(L"__ADJECTIVE",L"A")==-1)
			{
				return false;
			}
		}
	}
	if (isAdjective)
	{
		// ACCEPT: she stopped at a baker's.  
		// REJECT: she stopped at Boris's.
		// REJECT: she stopped at Boris's bakery.
		// REJECT: she understood the baker's agitation.
		// REJECT: she understood Boris's agitation.
		bool isUnresolvable=unResolvablePosition(beginObjectPosition);
    if (or&(PLACE_OBJECT_ROLE|MOVEMENT_PREP_OBJECT_ROLE) && isUnresolvable)
			isAdjective=false;
		// (117)||| (118)A (119)man's (120)voice (121)beside (122)her (123)made 
		// Ben's voice made her jump.
		// (250)Her (251)grave (252)eyes (253)met (254)his (grave is counted as an object)
		// Ben's statue's eyes followed her everywhere. (statue has flagNounOwner set)
		bool interveningObject=false;
		for (int owner=where+1; owner<at && !interveningObject; owner++)
			interveningObject=m[owner].getObject()>=0 && (objects[m[owner].getObject()].male || objects[m[owner].getObject()].female || (m[at].flags&cWordMatch::flagNounOwner));
		if (!interveningObject && m[at].getObject()>=0 && objects[m[at].getObject()].objectClass==BODY_OBJECT_CLASS)
			isAdjective=false;
		// (1328)||| (1329)Tommy (1330)sat (1331)down (1332)opposite (1333)her (1334). (1335)His (1336)bared (1337)head 
		// At 254: (250)She (253)met (254)his (255)inquiringly (256).
		if (at==where && (m[where].queryWinnerForm(personalPronounAccusativeForm)>=0 || m[where].queryWinnerForm(possessivePronounForm)>=0))
			isAdjective=false;
		//  Tommy's next procedure was to make a call at South Audley Mansions . 
		if (at!=where && (m[at].objectRole&IS_OBJECT_ROLE) && 
			  (m[at].word->first==L"procedure" || m[at].word->first==L"step" || m[at].word->first==L"activity" || m[at].word->first==L"act"))
			isAdjective=false;
	}
	/*
	int whereOwningVerb;
	if (inInfinitivePhrase && m[where].getRelVerb()>=0 && (whereOwningVerb=m[m[where].getRelVerb()].previousCompoundPartObject)>=0 && isSpecialVerb(whereOwningVerb))
	{
		or&=~(NONPAST_OBJECT_ROLE|NONPRESENT_OBJECT_ROLE);
		int tsSense=m[whereOwningVerb].verbSense;
		if ((tsSense!=VT_PAST && tsSense!=VT_EXTENDED+ VT_PAST && tsSense!=VT_PASSIVE+ VT_PAST && tsSense!=VT_PASSIVE+ VT_PAST+VT_EXTENDED))
			or|=NONPAST_OBJECT_ROLE;
		if ((tsSense!=VT_PRESENT && tsSense!=VT_EXTENDED+ VT_PRESENT && tsSense!=VT_PASSIVE+ VT_PRESENT && tsSense!=VT_PASSIVE+ VT_PRESENT+VT_EXTENDED))
			or|=NONPRESENT_OBJECT_ROLE;
		lplog(LOG_RESOLUTION,L"Infinitive verb @%d is owned by verb @%d.",m[where].getRelVerb(),whereOwningVerb);
	}
	*/
	// She[tuppence] addressed it[pencil] to LOCATIONTommy at his[tommy] club
	//   Mailing it to someone means that someone is NOT there
	// The MOVE_OBJECTlatter[faust] brought Tommy's mind[tommy] back to Mr . Brown again
	//   moving something that is not physical
	// He[prime minister] took up his[prime minister] conversation with Mr . Carter at the point it[conversation] had broken off . 
	//   EXCEPT when the something is a conversation
	if ((or&PREP_OBJECT_ROLE) && !(or&NO_PP_PREP_ROLE) && im->relPrep>=0 && m[im->relPrep].getRelVerb()>=0)
	{
		int whereVerb=m[im->relPrep].getRelVerb();
		if (isVerbClass(whereVerb,L"send-11.1-1") || !isPhysicalActionVerb(whereVerb))
			return false;
		int wo=m[whereVerb].getRelObject();
		if (wo>=0 && m[wo].getObject()>=0 && (lsi=in(m[wo].getObject()))!=localObjects.end() && !lsi->physicallyPresent &&
			  (m[wo].word->second.flags&cSourceWordInfo::notPhysicalObjectByWN))
		{
			set <wstring> synonyms;
  		getSynonyms(L"conversation",synonyms,ADJ);
			if (m[wo].word->first!=L"conversation" && synonyms.find(m[wo].word->first)==synonyms.end())
				return false;
		}
	}
	// neither of them spoke.
	if ((or&PREP_OBJECT_ROLE) && m[where].relPrep>0 && m[m[where].relPrep].word->first==L"of" && m[m[where].relPrep-1].queryWinnerForm(pronounForm)>=0)
		or&=~PREP_OBJECT_ROLE;
	return !notPhysicallyPresentObject &&
			// even though this could be an unresolvable object even with the OUTSIDE_QUOTE_NONPAST_OBJECT_ROLE flag,
			// unresolvables are given a critical role boost which should not be given to OUTSIDE_QUOTE_NONPAST_OBJECT_ROLE objects.
			// (did not strike her as a man who would be afraid of death!)
			!(or&NOT_OBJECT_ROLE) && 
			(ignoreTense || ((inQuote || !(or&NONPAST_OBJECT_ROLE)) &&	(!inQuote || !(or&NONPRESENT_OBJECT_ROLE)))) &&
			!isAdjective &&
			// NOT the photograph of 'Jane Finn' but keep 'I went with Pat.' /  I[julius] was lying in bed[bed] with a hospital nurse, and a little black - bearded man 
			(!(or&PREP_OBJECT_ROLE) || !(or&NO_PP_PREP_ROLE)) &&
			 // also this object cannot be in a question or a probability statement (if a duck was a man) or a relative clause
			 !inQuestion && !inInfinitivePhrase && !inPStatement && (!inRelClause || includeThoughInRelClause);
}

// a clerk three rats (don't resolve because this indicates the first time the object has been mentioned)
// but NOT Ben was a big man. and not where A is used as an initial.
// this also means that any word which is ONLY a quantifier or numeralCardinal will not be resolved at all.
// this where is set to the BEGINNING of the object, not the principalWhere.
bool cSource::unResolvablePosition(int where)
{
	LFS
		if (where < 0)
			return false;
  vector <cWordMatch>::iterator im=m.begin()+where;
	int at=im->principalWherePosition;
	if (at<0) at=im->principalWhereAdjectivalPosition;
	if (at<0) return false;
  return 
	  (m[at].objectRole&UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE) ||
		 im->queryWinnerForm(relativizerForm)>=0 ||
	 (im->word->first==L"a" && where+1<(signed)m.size() && (im+1)->word->first!=L"." && (m[at].endObjectPosition-m[at].beginObjectPosition>1)) || // not A. Carter
		im->word->first==L"an" || im->word->first==L"another" || im->word->first==L"some" || 
		// relative clause must be attached to this instance of the object
		// not "the" unless relative clause attached
		 (m[at].getObject()>=0 && at<objects[m[at].getObject()].whereRelativeClause && 
		  m[m[at].endObjectPosition].word->first!=L"," && // allow only restrictive relative clauses
		  objects[m[at].getObject()].objectClass!=NAME_OBJECT_CLASS && im->word->first==L"the") || 
			// some had been saved.  / NOT 'all three' were there.
     (im->queryWinnerForm(quantifierForm)>=0 && im->word->first!=L"all") ||
     im->queryWinnerForm(numeralCardinalForm)>=0;
}

// eliminate an object that exactly matches another object in localObjects
// but which cannot match it because it is an unresolvable object
// a hospital nurse
bool cSource::eliminateExactMatchingLocalObject(int where) 
{ LFS
	vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
	for (; lsi!=lsiEnd; lsi++)
	{
		if (objects[lsi->om.object].objectClass==objects[m[where].getObject()].objectClass && 
			  objects[lsi->om.object].end-objects[lsi->om.object].begin==objects[m[where].getObject()].end-objects[m[where].getObject()].begin)
		{
			bool allMatched=true;
			for (int I=objects[lsi->om.object].begin,J=objects[m[where].getObject()].begin; I<objects[lsi->om.object].end && allMatched; I++,J++)
				allMatched=(m[I].word==m[J].word);
			if (allMatched)
			{
			  if (debugTrace.traceSpeakerResolution)
				{
			    wstring tmpstr;
					lplog(LOG_RESOLUTION,L"%06d:erased localObject %s because it matches a currently occurring unresolvable object",
					  where,objectString(lsi->om,tmpstr,true).c_str());
				}
				if (currentSpeakerGroup<speakerGroups.size() && currentEmbeddedSpeakerGroup>=0 && 
						((unsigned)currentEmbeddedSpeakerGroup)<speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size() &&
						speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.find(lsi->om.object)!=speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.end())
				{
					speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.erase(lsi->om.object);
					speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.insert(m[where].getObject());
					if (debugTrace.traceSpeakerResolution)
					{
						wstring tmpstr,tmpstr2,tmpstr3;
						lplog(LOG_RESOLUTION,L"%06d:replaced object %s with object %s in embeddedSpeakerGroup %s because it matches a currently occurring unresolvable object",
							where,objectString(lsi->om,tmpstr,true).c_str(),objectString(m[where].getObject(),tmpstr2,true).c_str(),toText(speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup],tmpstr3));
					}
				}
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr;
					lplog(LOG_RESOLUTION,L"%06d:Removed %s from localObjects",where,objectString(lsi->om.object,tmpstr,true).c_str());
				}
				localObjects.erase(lsi);
				return true;
			}
		}
	}
	return false;
}

void cSource::unMatchObjects(int where,vector <cOM> &objectMatches,bool identifiedAsSpeaker)
{ LFS
  for (vector <cOM>::iterator omi=m[where].objectMatches.begin(); omi!=m[where].objectMatches.end(); )
  {
    if (in(omi->object,objectMatches)!=objectMatches.end())
      omi++;
		else
		{
			// erase location from *omi
			vector <cObject::cLocation>::iterator wloc=find(objects[omi->object].locations.begin(),objects[omi->object].locations.end(),cObject::cLocation(where));
			if (wloc!=objects[omi->object].locations.end())
				objects[omi->object].locations.erase(wloc);
			if (identifiedAsSpeaker)
				objects[omi->object].numIdentifiedAsSpeaker--;
			else
				objects[omi->object].numEncounters--;
			omi=m[where].objectMatches.erase(omi);
		}
  }
}

void cSource::unMatchObjects(int where,vector <int> &restrictObjects,bool identifiedAsSpeaker)
{ LFS
  for (vector <cOM>::iterator omi=m[where].objectMatches.begin(); omi!=m[where].objectMatches.end(); )
  {
    if (find(restrictObjects.begin(),restrictObjects.end(),omi->object)!=restrictObjects.end())
      omi++;
		else
		{
			// erase location from *omi
			vector <cObject::cLocation>::iterator wloc=find(objects[omi->object].locations.begin(),objects[omi->object].locations.end(),cObject::cLocation(where));
			if (wloc!=objects[omi->object].locations.end())
				objects[omi->object].locations.erase(wloc);
			if (identifiedAsSpeaker)
				objects[omi->object].numIdentifiedAsSpeaker--;
			else
				objects[omi->object].numEncounters--;
			omi=m[where].objectMatches.erase(omi);
		}
  }
}

bool cSource::intersect(int where,set <int> &speakers,bool &allIn,bool &oneIn)
{ LFS
  if (m[where].objectMatches.size())
    intersect(m[where].objectMatches,speakers,allIn,oneIn);
  else
    oneIn=allIn=(speakers.find(m[where].getObject())!=speakers.end());
	return oneIn;
}

bool cSource::intersect(int where,vector <cOM> &speakers,bool &allIn,bool &oneIn)
{ LFS
  if (m[where].objectMatches.size())
    intersect(m[where].objectMatches,speakers,allIn,oneIn);
  else
    oneIn=allIn=(in(m[where].getObject(),speakers)!=speakers.end());
	return oneIn;
}

bool cSource::intersect(int where,vector <int> &objs,bool &allIn,bool &oneIn)
{ LFS
  if (m[where].objectMatches.size())
    intersect(m[where].objectMatches, objs,allIn,oneIn);
  else
    oneIn=allIn=(find(objs.begin(), objs.end(),m[where].getObject())!= objs.end());
	return oneIn;
}

bool cSource::intersect(vector <cOM> &matches,set <int> &speakers,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
  for (unsigned int I=0; I<matches.size(); I++)
    if (speakers.find(matches[I].object)!=speakers.end())
      oneIn=true;
    else
      allIn=false;
	return oneIn;
}

bool cSource::intersect(set <int> &speakers,vector <cOM> &matches,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
	for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
    if (in(*si,matches)!=matches.end())
      oneIn=true;
    else
      allIn=false;
	return oneIn;
}

bool cSource::intersect(vector <int> &speakers,vector <cOM> &matches,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
	for (vector <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
    if (in(*si,matches)!=matches.end())
      oneIn=true;
    else
      allIn=false;
	return oneIn;
}

// are any or all matches in speakers?
bool cSource::intersect(vector <int> &matches,set <int> &speakers,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
  for (unsigned int I=0; I<matches.size(); I++)
    if (speakers.find(matches[I])!=speakers.end())
      oneIn=true;
    else
      allIn=false;
	return oneIn;
}

// are any or all matches in speakers?
bool cSource::intersect(vector <int> &o1,vector <int> &o2,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
	for (vector<int>::iterator oi=o1.begin(),oiEnd=o1.end(); oi!=oiEnd; oi++)
    if (find(o2.begin(),o2.end(),*oi)!=o2.end())
      oneIn=true;
    else
      allIn=false;
	return oneIn;
}

// are any or all speakers in matches?
bool cSource::intersect(set <int> &speakers,vector <int> &matches,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
	for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
    if (find(matches.begin(),matches.end(),*si)!=matches.end())
      oneIn=true;
    else
      allIn=false;
	return oneIn;
}

// are any or all speakers in matches?
bool cSource::intersect(set <int> &speakers,set <int> &matches,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
	for (set <int>::iterator si=speakers.begin(),siEnd=speakers.end(); si!=siEnd; si++)
    if (matches.find(*si)!=matches.end())
      oneIn=true;
    else
      allIn=false;
	return oneIn;
}

bool cSource::intersect(vector <cOM> &m1,vector <cOM> &m2,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
  for (unsigned int I=0; I<m1.size(); I++)
    if (in(m1[I].object,m2)!=m2.end())
      oneIn=true;
    else
      allIn=false;
  return oneIn;
}

bool cSource::intersect(vector <tIWMM> &m1,vector <tIWMM> &m2,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
  for (unsigned int I=0; I<m1.size(); I++)
    if (find(m2.begin(),m2.end(),m1[I])!=m2.end())
      oneIn=true;
    else
      allIn=false;
  return oneIn;
}

bool cSource::intersect(vector <tIWMM> &m1,wchar_t **a,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
  for (unsigned int I=0; I<m1.size(); I++)
	  for (unsigned int J=0; a[J]; J++)
			if (Words.query(a[J])==m1[I])
	      oneIn=true;
		  else
			  allIn=false;
  return oneIn;
}

bool cSource::intersect(vector <cOM> &m1,vector <int> &m2,bool &allIn,bool &oneIn)
{ LFS
  allIn=true;
  oneIn=false;
  for (unsigned int I=0; I<m1.size(); I++)
    if (find(m2.begin(),m2.end(),m1[I].object)!=m2.end())
      oneIn=true;
    else
      allIn=false;
  return oneIn;
}

int cSource::preferWordOrder(int wordOrderSensitiveModifier,vector <int> &locations)
{ LFS
	// wchar_t *wordOrderWords[]={L"other",L"another",L"second",L"first",L"third",L"former",L"latter",L"that",L"this",L"two",L"three",NULL};
	switch (wordOrderSensitiveModifier)
	{
	case 3: // first
		{
			int minLocation=-1,which=-1;
			for (unsigned int I=0; I<locations.size(); I++)
				if (minLocation<0 || minLocation>locations[I])
				{
					minLocation=locations[I];
			    which=I;
				}
			return which;
		}
	case 5: // former
	case 0: // other requires that we have two of these items
		if (locations.size()!=2)
			return -1;
		// so now return the one which was not referenced last
		if (locations[0]<locations[1])
			return 0;
		else
			return 1;
		break;
	case 1:
		// another refers to something that has not been referred to previously
		return -2;
	case 2: // second
	case 6: // latter
		if (locations.size()!=2)
			return -1;
	case 7: // that
	case 8: // this
	case 4: // third
		// so now return the one which WAS referenced last
		int maxLocation=-1,which=-1;
		for (unsigned int I=0; I<locations.size(); I++)
			if (maxLocation<locations[I])
			{
				maxLocation=locations[I];
		    which=I;
			}
		return which;
	}
	return -1;
}

// if subject and object are pronouns and one is single and the other is plural, 
//   override computed saliences and substitute speakerGroup info
bool cSource::mixedPluralityInSameSentence(int where)
{ LFS
	int object=m[where].getObject();
	if (object<0) return false;
	int objectClass=objects[object].objectClass;
  if (objectClass!=PRONOUN_OBJECT_CLASS &&
      objectClass!=REFLEXIVE_PRONOUN_OBJECT_CLASS &&
      objectClass!=RECIPROCAL_PRONOUN_OBJECT_CLASS)
		return false;
	int whereOtherObject;
	if ((whereOtherObject=m[where].getRelObject())<0 && (whereOtherObject=m[where].relSubject)<0)
		return false;
	int otherObject=m[whereOtherObject].getObject();
	if (otherObject<0)
		return false;
	int otherObjectClass=objects[otherObject].objectClass;
	if (otherObjectClass!=PRONOUN_OBJECT_CLASS &&
			otherObjectClass!=REFLEXIVE_PRONOUN_OBJECT_CLASS &&
			otherObjectClass!=RECIPROCAL_PRONOUN_OBJECT_CLASS)
	{
		int otherObjectMaxEnd=m[whereOtherObject].pma.findMaxLen()+whereOtherObject;
		bool successfullyFoundLocalPrepObject=false;
		// attempt to search out any objects of prepositions within the immediate noun phrase
		for (int I=whereOtherObject; I<otherObjectMaxEnd && !isEOS(I) && !(m[I].objectRole&SUBJECT_ROLE) && m[I].word!=Words.sectionWord && !successfullyFoundLocalPrepObject; I++)
			successfullyFoundLocalPrepObject=(m[I].objectRole&PREP_OBJECT_ROLE)!=0 && (otherObject=m[I].getObject())>=0 && 
				  ((otherObjectClass=objects[otherObject].objectClass)==PRONOUN_OBJECT_CLASS || otherObjectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS || otherObjectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS);
		if (!successfullyFoundLocalPrepObject)
			return false;
	}
	return objects[object].plural ^ objects[otherObject].plural;
}

#define MIXED_PLURALITY_SALIENCE_BOOST 2000
// if there are subgroups for the current speakerGroup
// and there are both plural and singular pronouns used in this sentence
// then if this is a singular pronoun increase localObjects matching the singular division of the speakerGroup salience by 1000 
// if this is a plural pronoun increase localObjects matching the plural division of the speakerGroup salience by 1000 
void cSource::mixedPluralityUsageSubGroupEnhancement(int where)
{ LFS
	if (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].groupedSpeakers.size())
	{
		int I;
		// back up to beginning of sentence
		for (I=where; I>=0 && !isEOS(I); I--);
		if (I==0) return;
		int beginSentence=I;
		for (I=where; I<(signed)m.size() && !isEOS(I); I++);
		if (I==m.size()) return;
		int endSentence=I;
		int singularPronounSeen=0,pluralPronounSeen=0;
		for (I=beginSentence; I<endSentence; I++)
		{
			int o=m[I].getObject();
			if (o== cObject::eOBJECTS::OBJECT_UNKNOWN_MALE || o== cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE ||
				  (o>=0 && (objects[o].male || objects[o].female) && !objects[o].plural && 
					 (objects[o].objectClass==PRONOUN_OBJECT_CLASS || objects[o].objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS || 
					  objects[o].objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS)))
				singularPronounSeen++;
			else if (o== cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL || (o>=0 && objects[o].plural))
				pluralPronounSeen++;
		}
		if (singularPronounSeen==0 || pluralPronounSeen==0)
			return;
		int o=m[where].getObject();
		singularPronounSeen=pluralPronounSeen=0;
		if (o== cObject::eOBJECTS::OBJECT_UNKNOWN_MALE || o== cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE || (o>=0 && (objects[o].male || objects[o].female) && !objects[o].plural))
			singularPronounSeen++;
		else if (o== cObject::eOBJECTS::OBJECT_UNKNOWN_PLURAL || (o>=0 && objects[o].plural))
			pluralPronounSeen++;
		if (!((singularPronounSeen==0) ^ (pluralPronounSeen==0)))
			return;
	  vector <cSpeakerGroup>::iterator csg=speakerGroups.begin()+currentSpeakerGroup;
		// boost all objects in currentSpeakerGroup not belonging to subgroup
		bool previousSpeakerGroup=csg->groupedSpeakers.size()>0;
		bool promoteNewSpeakers=((singularPronounSeen && previousSpeakerGroup) || (pluralPronounSeen && !previousSpeakerGroup));
		vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end();
		wstring tmpstr;
		for (; lsi!=lsiEnd; lsi++)
		{
			if (csg->speakers.find(lsi->om.object)!=csg->speakers.end())
			{
				bool promote=(csg->groupedSpeakers.find(lsi->om.object)!=csg->groupedSpeakers.end()) ^ promoteNewSpeakers;
				lsi->om.salienceFactor+=(promote) ? MIXED_PLURALITY_SALIENCE_BOOST : -MIXED_PLURALITY_SALIENCE_BOOST;
				itos(L"SINGPLUR[+",(promote) ? MIXED_PLURALITY_SALIENCE_BOOST : -MIXED_PLURALITY_SALIENCE_BOOST,lsi->res,L"]");
			}
		}
	}
}

void cSource::processSubjectCataRestriction(int where,int subjectCataRestriction)
{ LFS
	if (m[where].objectMatches.size()==1 && subjectCataRestriction>=0 && m[subjectCataRestriction].objectMatches.size()>1)
	{
		vector <cOM>::iterator omi;
		if ((omi=in(m[where].objectMatches[0].object,m[subjectCataRestriction].objectMatches))!=m[subjectCataRestriction].objectMatches.end())
		{
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr,tmpstr2;
				lplog(LOG_RESOLUTION,L"%06d:erased object %d:%s from subject %d:%s in subject cata restriction.",
					where,where,objectString(m[where].objectMatches[0],tmpstr,true).c_str(),subjectCataRestriction,objectString(m[subjectCataRestriction].objectMatches,tmpstr2,true).c_str());
			}
			m[subjectCataRestriction].objectMatches.erase(omi);
		}
		// subjectCataRestriction is for the first position in a possible multiple subject.
		if (m[subjectCataRestriction].objectRole&MPLURAL_ROLE)
		{
			for (unsigned int I=subjectCataRestriction+1; (m[I].objectRole&(MPLURAL_ROLE|SUBJECT_ROLE)); I++)
			  if (m[I].getObject()!=-1)
				{
					processSubjectCataRestriction(where,I);
					break;
				}
		}
	}
}

// scan before the quote within the containing paragraph
// if the word before beginQuote is a . or EOS, and if the word before has a match to _S1,
//   return a speaker with scanSpeakerAfter, with tillEndOfSentence=true, and endQuote=the beginning of _S1.
// if previousParagraph is set, this indicates the speaker came from the subject of the previous paragraph ending with a :
int cSource::speakerBefore(int beginQuote,bool &previousParagraph)
{ LFS
	if (beginQuote<2) return -1;
	previousParagraph=false;
	int where=beginQuote-1,tmpwhere=(beginQuote>2) ? beginQuote-2-m[beginQuote-2].maxMatch+1 : -1,quoteEnd=-1,I,preference=-1;
	int element=(tmpwhere>=0) ? queryPattern(tmpwhere,L"__S1",quoteEnd) : -1;
	bool previousWordPunctuation=(wcschr(L"?!.,:",m[beginQuote-1].word->first[0])!=NULL);
  // maxMatch calculated to match the end of the pattern match, so we need to add 1. beginQuote-2-m[beginQuote-2].maxMatch+1
  // IF the quote is at the beginning of a sentence AND the preceeding pattern is a sentence which ends BEFORE the quote begins THEN search for speaker
	// this is not necessarily the lastBeginS1, as maxMatch can also match a larger structure, such as MS1.
  if (tmpwhere>=0 && previousWordPunctuation && element!=-1 && pema[element].end+tmpwhere<beginQuote)
		// sanity check - no conjunctions or infinitive phrases from beginQuote to this position.
    preference=pema[element].begin+tmpwhere;
	if (tmpwhere>=0 && previousWordPunctuation && element==-1 && (element=queryPattern(tmpwhere,L"_MS1",quoteEnd))!=-1 && pema[element].end+tmpwhere<beginQuote)
		tmpwhere+=pema[element].begin;
	else
	{
		if (m[where].word==Words.sectionWord) 
		{
			where--;
			previousParagraph=(m[where].word->first==L":");
		}
		if (previousParagraph && m[where-1].queryForm(thinkForm)>=0 && m[where-1].relSubject>=0 && m[m[where-1].relSubject].getObject()>=0 && 
				(objects[m[m[where-1].relSubject].getObject()].isAgent(true) ||
				 m[m[where-1].relSubject].word->first==L"voice" || m[m[where-1].relSubject].word->first==L"who"))
			return m[m[where-1].relSubject].beginObjectPosition-1;
		tmpwhere=(m[where].word->first==L":" || m[where].queryForm(thinkForm)>=0) ? where-m[where-1].maxMatch : beginQuote-2;
	}
	bool S1found=false;
	for (I=beginQuote-2; I>=tmpwhere && I>=0 && !S1found; I--)
	{
		if (S1found=m[I].pma.findMaxLen(L"__S1",element) && m[I].pma[element].len+I<=beginQuote && m[I].pma[element].len+I>=where)
			break;
		if (S1found=m[I].pma.findMaxLen(L"_REL1",element) && m[I].pma[element].len+I<=beginQuote && m[I].pma[element].len+I>=where &&
			  m[I].getRelVerb()>=0 && m[m[I].getRelVerb()].queryForm(thinkForm)>=0 && m[I].word->first==L"who")
			break;
		if (S1found=m[I].pma.findMaxLen(L"_MS1",element) && m[I].pma[element].len+I<=beginQuote && m[I].pma[element].len+I>=where)
		{
			int subjectTag=-1;
			// if contains SUBJECT, return SUBJECT-1.
			if ((((patterns[m[I].pma[element].getPattern()]->includesOnlyDescendantsAllOfTagSet)&((__int64)1<<subjectVerbRelationTagSet))!=0))
			{
		    vector < vector <cTagLocation> > tagSets;
				tagSets.clear();
				if (startCollectTags(debugTrace.traceRelations,subjectVerbRelationTagSet,where,m[I].pma[element].pemaByPatternEnd,tagSets,true,false,L"verb relation - speaker before")>0)
				{
					for (unsigned int J=0; J<tagSets.size() && subjectTag<0; J++) 
						if ((subjectTag=findOneTag(tagSets[J],L"SUBJECT",-1))>=0)
							I=tagSets[J][subjectTag].sourcePosition-1;
				}
			}
			// now find the last _S1 that is owned by _MS1 and return __S1-1.
			if (subjectTag>=0) break;
			S1found=false;
			for (I=beginQuote-2; I>=tmpwhere && I>=0 && !S1found; I--)
				if (S1found=m[I].pma.findMaxLen(L"__S1",element) && m[I].pma[element].len+I<=beginQuote)
					break;
			if (S1found || I<0) break;
		}
		if (S1found=m[I].pma.findMaxLen(L"_VERB_BARE_INF",element) && m[I].pma[element].len+I<=beginQuote && m[I].pma[element].len+I>=where)
			break;
	}
	if (!S1found && preference==-1) return -1;
	if (preference<0 || (I>=0 && preference<I && m[I].getRelVerb()>=0 && isVerbClass(m[I].getRelVerb(),L"say")))
		preference=I;
	int lastLargestSType=preference; // see note above as to lastLargestSType
	if (m[lastLargestSType].objectRole&MPLURAL_ROLE) return -1; // if subject is plural this is not the speaker.
	return lastLargestSType-1;
}

wchar_t *onlyThink[]={ L"thought", NULL };
void cSource::setSameAudience(int whereVerb,int speakerObjectPosition,int &audienceObjectPosition)
{ LFS
	if (audienceObjectPosition!=-1 || whereVerb<0 || speakerObjectPosition<0) return;
	for (unsigned int I=0; onlyThink[I]; I++)
		if (m[whereVerb].word->first==onlyThink[I]) 
		{
			audienceObjectPosition=speakerObjectPosition;
			return;
		}
}

/* CASES:
"No names, please.  I'm known as Mr. Carter here.  Well, now"--he looked from one to the other--"who's going to tell me the story?"
Boris remarked, glancing up at the clock: "You are early.

*/
// if a paragraph goes by and there are no quotations, set previousSpeakers and beforePrevious to unknown
// if inserted ", don't change speaker, but start a new section dedicated to that speaker.

// if no match of these strings immediately before or after in the same paragraph, and the previous quotation immediately
// preceded the current one {A}:
//   If reached the end of the " and there is just a paragraph, and the previous quotation or this one did not end in a ?, {B}
//      Assign the speaker before the previous speaker.  {C}
//   If reached the end of the ", there is just a paragraph, and previous quotation or this one DID end in a ?, {D}
//      assign the speaker before the previous speaker to this current quotation. {E}
//   otherwise, look for any person or pronouns in the sentence after the quote in the same paragraph.  if there is none,
//     switch to the speaker before the previous speaker. {E}
// if no match of these strings immediately before or after in the same paragraph and the previous quotations did not immediately
//   precede the current one: {F}
//   if the previous paragraph ended in a :, scan current and previous sentence for speaker string matches.  If not found,
//   assign both the previous speaker and the speaker before that (but probably it is the previous speaker)

// When speaker is resolved, assign current speaker, and move backwards, assigning the previous immediate quotations {G}
// with the opposite speakers until resolved or there are no more paragraphs containing quotations.
// till end of sentence or end of paragraph - which also must be after the end of a sentence.
int cSource::scanForSpeaker(int where,bool &definitelySpeaker,bool &crossedSectionBoundary,int &audienceObjectPosition)
{ LFS
  if ((unsigned)where+1>=m.size()) return -1;
  definitelySpeaker=true;
	crossedSectionBoundary=false;
  vector <cWordMatch>::iterator im=m.begin()+where+1;
  int element,end=-1,speakerObjectPosition=-1;
	bool speakerSet=false,atSectionWord=false;
  while (im->word->first==L"," || im->word->first==L"--" || im->word->second.query(dashForm)>=0)
  {
    im++; // "No names, please.  Well, now"--he looked from one to the other--"who's going to tell me the story?"
    where++;
  }
	int vbi=-1,endVBI=-1;
	bool extendedSayVerb=false;
	bool thinkSayVerb=false;
	if (where+2<(signed)m.size())
		thinkSayVerb=((vbi=im->pma.queryPattern(L"_VERB_BARE_INF",endVBI))!=-1 && m[where+2].principalWherePosition>=0) ?
			// heard him say // as in "Tuppence lingered a moment longer by the door which she[tuppence] had carefully neglected to close , and heard him[boris,albert] say : "
			(m[where+2].endObjectPosition>=0 && m[where+2].endObjectPosition<(signed)m.size() && m[m[where+2].endObjectPosition].queryForm(thinkForm)>=0)
			: 
			// said Ann.
			(im->queryForm(thinkForm)>=0 || 
			 // also include 'put in' / broke 'out' - only allowed with their particular particles
			 (extendedSayVerb=(m[where+2].word->first==L"in" && m[where+1].word->first==L"put")) ||
		   (extendedSayVerb=(m[where+2].word->first==L"out" && m[where+1].word->first==L"broke")));
	else
		thinkSayVerb=im->queryForm(thinkForm)>=0;
  if ((element=im->pma.queryPattern(L"_VERBREL1",end))!=-1 || vbi!=-1)
  {
		if (vbi!=-1) 
			end=where+endVBI+1;
		else
			end+=where+1;
    speakerObjectPosition=where+2;
    for (im=m.begin()+speakerObjectPosition; speakerObjectPosition<end; im++,speakerObjectPosition++)
    {
			if (im->principalWherePosition<0 || m[im->principalWherePosition].getObject()<0) continue;
      // verbrel1 indicates a verb followed by a noun.  However, if this is a past tense verb,
      //    it will also be counted as an adjective, as part of a _NOUN.  If this is so, reject the whole thing.
      if (thinkSayVerb || objects[m[im->principalWherePosition].getObject()].isAgent(true) || isVoice(im->principalWherePosition))
      {
        // scan for audience "to"
        int saveAudienceObjectPosition=audienceObjectPosition,ao; // audienceObjectPosition may have been previously set by hailed speakers
        for (audienceObjectPosition=speakerObjectPosition+1,im++; audienceObjectPosition<end; im++,audienceObjectPosition++)
        {
          if (im->word->first==L"to" && audienceObjectPosition+1<end && (ao=m[audienceObjectPosition+1].getObject())!= cObject::eOBJECTS::UNKNOWN_OBJECT &&
						  (ao<0 || objects[ao].isAgent(true)))
          {
            audienceObjectPosition++;
            return speakerObjectPosition;
          }
        }
        audienceObjectPosition=saveAudienceObjectPosition;
				setSameAudience(where+1,speakerObjectPosition,audienceObjectPosition);
        return speakerObjectPosition;
      }
    }
    im=m.begin()+where+1;
		speakerSet=true;
  }
  if (im->word==Words.sectionWord)
  {
		crossedSectionBoundary=true;
    im++; // "I beg your pardon."  A man's voice - Secret Adversary source
		//im++;
    where++;//=2;
    if ((unsigned)where+1>=m.size())
      return -1;
		atSectionWord=true;
  }
  // This is if the quote is absorbed by a quoted noun, so that the whole sentence becomes an _S1, eliminating the _VERBREL1.
  else if ((element=im->pma.queryPattern(L"_VERBPAST",end))!=-1 || (im->queryWinnerForm(verbForm)>=0 && (im->word->second.inflectionFlags&VERB_PAST)==VERB_PAST))
  {
		speakerSet=true;
    if (end<0) end=1;
    speakerObjectPosition=end+where+1;
		if (extendedSayVerb)
			speakerObjectPosition++;
    if (m[speakerObjectPosition].principalWherePosition>=0 && (thinkSayVerb ||
      objects[m[m[speakerObjectPosition].principalWherePosition].getObject()].isAgent(true) ||
      isVoice(m[speakerObjectPosition].principalWherePosition)))
    {
      // scan for audience "to"
      int saveAudienceObjectPosition=audienceObjectPosition,ao; // audienceObjectPosition may have been previously set by hailed speakers
      for (audienceObjectPosition=speakerObjectPosition+1,im++; audienceObjectPosition<end; im++,audienceObjectPosition++)
      {
        if (im->word->first==L"to" && audienceObjectPosition+1<end && (ao=m[m[audienceObjectPosition+1].principalWherePosition].getObject())!=cObject::eOBJECTS::UNKNOWN_OBJECT && 
					  (ao<0 || objects[ao].isAgent(true)))
        {
          audienceObjectPosition++;
          return speakerObjectPosition;
        }
      }
      audienceObjectPosition=saveAudienceObjectPosition;
			setSameAudience(where+1,speakerObjectPosition,audienceObjectPosition);
      return speakerObjectPosition;
    }
  }
  if (im->principalWherePosition<0) return -1;
	int verbPosition;
	thinkSayVerb=(verbPosition=m[im->principalWherePosition].getRelVerb())>=0 && verbPosition+1<(signed)m.size() &&
							 (m[verbPosition].queryForm(thinkForm)>=0 || 
								// also include 'broke in', and 'put in'
							  (m[verbPosition+1].word->first==L"in" && (m[verbPosition].word->first==L"put" || m[verbPosition].word->first==L"broke")));
  speakerObjectPosition=where+1;
  // he muttered. - the verb for this pattern _S1 must not have any objects!  Also must be a simple past tense, for primary quotes!
	// She[tuppence] looked appealingly at Sir James , who replied gravely :
	bool isAgent = m[im->principalWherePosition].getObject()>=0 && (objects[m[im->principalWherePosition].getObject()].isAgent(true) ||
		  isVoice(im->principalWherePosition) || m[im->principalWherePosition].word->first==L"who");
  if (!thinkSayVerb && !isAgent) return -1;
  definitelySpeaker=false;
	if (im->flags&cWordMatch::flagIgnoreAsSpeaker) 
	{
		if (crossedSectionBoundary || objects[m[im->principalWherePosition].getObject()].neuter) return -1;
		setSameAudience(verbPosition,speakerObjectPosition,audienceObjectPosition);
		if (definitelySpeaker=m[speakerObjectPosition].getRelObject()>=0 && isVoice(m[speakerObjectPosition].getRelObject()))
			speakerObjectPosition=m[m[speakerObjectPosition].getRelObject()].beginObjectPosition;
		return speakerObjectPosition;
	}
  if (im->pma.findMaxLen(L"__S1",element)==true && (patterns[im->pma[element].getPattern()]->differentiator[0]=='1' || patterns[im->pma[element].getPattern()]->differentiator[0]=='4'))
  {
		if (atSectionWord) return -1;
    vector < vector <cTagLocation> > tagSets;
    int nextObjectTag=-1,vTag=-1;
    if (startCollectTags(debugTrace.traceVerbObjects,verbObjectsTagSet,where+1,im->pma.content[element].pemaByPatternEnd,tagSets,true,false,L"verb objects - scan for speaker")>0)
      for (unsigned int J=0; J<tagSets.size(); J++)
      {
				if (debugTrace.traceVerbObjects)
					printTagSet(LOG_RESOLUTION,L"VO",J,tagSets[J],where+1,im->pma.content[element].pemaByPatternEnd);
				// also must not have a relative clause starting with 'that'
        if (getVerb(tagSets[J],vTag))
        {
          verbPosition=tagSets[J][vTag].sourcePosition;
					int pwsp=speakerObjectPosition;
					if (m[speakerObjectPosition].principalWherePosition>=0) pwsp=m[speakerObjectPosition].principalWherePosition;
					if (m[pwsp].getRelVerb()<0 || verbPosition!=m[pwsp].getRelVerb())
						lplog(LOG_RESOLUTION,L"speaker verb disagreement! %d:speakerObjectPosition[PW]=%d: relVerb=%d vs collectTag verbPosition %d.",where,pwsp,m[pwsp].getRelVerb(),verbPosition);
					int objectPosition,ao=-1;
					if (verbPosition+1<(int)m.size() && m[verbPosition+1].queryWinnerForm(adverbForm)>=0) verbPosition++;
					if (verbPosition+1<(int)m.size() && m[verbPosition+1].word->first==L"that") return -1;
					// he[julius] said in a low voice to Tuppence 
          if (verbPosition+1<(int)m.size() && m[verbPosition+1].queryWinnerForm(prepositionForm)>=0 && m[verbPosition+1].word->first!=L"to" && 
						     m[verbPosition+2].principalWherePosition>=0)
						for (end=m[m[verbPosition+2].principalWherePosition].endObjectPosition; verbPosition<end && m[verbPosition+1].word->first!=L"to"; verbPosition++);
					// Tuppence rose to her feet
          if (verbPosition+1<(int)m.size() && m[verbPosition+1].word->first==L"to" && (objectPosition=m[verbPosition+2].principalWherePosition)>=0 &&
						  (thinkSayVerb || (ao=m[objectPosition].getObject())<0 || objects[ao].objectClass!=BODY_OBJECT_CLASS) && 
								(ao<0 || objects[ao].isAgent(true)))
					{
						// audienceObjectPosition if already set is a HAIL.  verbPosition+2 is specific, but may be him or her, so it may be gender ambiguous.
						// “[tommy:julius] I[tommy] say , Hersheimmer ” -- Tommy turned to him[henry] -- “[tommy:julius] Tuppence has gone off sleuthing on her[tuppence] own . ”
						if (audienceObjectPosition>=0 && (ao=m[audienceObjectPosition].getObject())>=0 &&
							  objects[ao].objectClass==NAME_OBJECT_CLASS && m[objectPosition].queryWinnerForm(personalPronounAccusativeForm)>=0 &&
							  objects[ao].matchGender(objects[m[objectPosition].getObject()]))
						{
							m[objectPosition].objectMatches.clear();
							m[objectPosition].flags|=cWordMatch::flagObjectResolved;
							m[objectPosition].objectMatches.push_back(cOM(ao,SALIENCE_THRESHOLD));
							objects[ao].locations.push_back(objectPosition);
							objects[ao].updateFirstLocation(objectPosition);
							wstring tmpstr;
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"%06d:audience/hail match %d:%s",objectPosition,audienceObjectPosition,objectString(m[audienceObjectPosition].getObject(),tmpstr,true).c_str());
						}
						else if (audienceObjectPosition<0 || ao<0 || objects[m[audienceObjectPosition].getObject()].objectClass!=NAME_OBJECT_CLASS || m[verbPosition+2].queryWinnerForm(reflexivePronounForm)>=0)
		          audienceObjectPosition=verbPosition+2;
					}
        }
        definitelySpeaker|=findTag(tagSets[J],L"OBJECT",nextObjectTag)<0;
      }
		speakerSet=true;
  }
  if (!speakerSet && !(definitelySpeaker=thinkSayVerb && isAgent)) return -1;
	setSameAudience(verbPosition,speakerObjectPosition,audienceObjectPosition);
	return speakerObjectPosition;
}

void cSource::addCataSpeaker(int where,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool definitelySpeaker)
{ LFS
  int element,speakerObjectPosition=-1,speakerObject;
  // skip past verb
  vector <cWordMatch>::iterator im=m.begin()+where+2;
  if ((element=im->pma.queryPattern(L"_VERBPAST",speakerObjectPosition))!=-1)
    speakerObjectPosition+=where+1;
  else
    speakerObjectPosition=where+2;
	speakerObjectPosition=m[speakerObjectPosition].principalWherePosition;
  if (m[speakerObjectPosition].objectMatches.size() || (speakerObject=m[speakerObjectPosition].getObject())<0)
    return;
  genderedBodyPartSubstitution(where,speakerObject);
  // we don't care about adjectival objects
  resolveObject(speakerObjectPosition,definitelySpeaker,false,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
}

// where==beginQuote
// objectsToSet=m[where].audienceObjectMatches
// int fromPosition=audienceObjectPosition
void cSource::setSpeakerMatchesFromPosition(int where,vector <cOM> &objectsToSet,int fromPosition,wchar_t *fromWhere,unsigned __int64 flag)
{ LFS
  if (m[fromPosition].objectMatches.size())
    objectsToSet=m[fromPosition].objectMatches;
  else if (m[fromPosition].getObject()>=0)
  {
    objectsToSet.clear();
    objectsToSet.push_back(cOM(m[fromPosition].getObject(),SALIENCE_THRESHOLD));
  }
	if (flag&(cWordMatch::flagSpecifiedResolveAudience|cWordMatch::flagDefiniteResolveSpeakers))
	{
		int os=(m[fromPosition].objectMatches.size()==1) ? m[fromPosition].objectMatches[0].object : m[fromPosition].getObject();
		vector <cLocalFocus>::iterator lsi=in(os);
		if (lsi!=localObjects.end() && lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent<0)
			lsi->whereBecamePhysicallyPresent=fromPosition;
	}
  for (vector <cOM>::iterator nmi=objectsToSet.begin(),nmiEnd=objectsToSet.end(); nmi!=nmiEnd; nmi++)
    objects[nmi->object].speakerLocations.insert(where);
  if (debugTrace.traceSpeakerResolution)
	{
	  wstring tmpstr,tmpstr2;
		bool audience=(flag&(cWordMatch::flagForwardLinkAlternateResolveAudience|cWordMatch::flagHailedResolveAudience|cWordMatch::flagSpecifiedResolveAudience|cWordMatch::flagFromLastDefiniteResolveAudience|cWordMatch::flagGenderIsAmbiguousResolveAudience|cWordMatch::flagMostLikelyResolveAudience))!=0;
		lplog(LOG_RESOLUTION,L"%06d:Set %s to %d:%s (%s) with flags=%s",where,(audience) ? L"audience":L"speaker",
			 fromPosition,objectString(objectsToSet,tmpstr,true).c_str(),fromWhere,speakerResolutionFlagsString(flag,tmpstr2).c_str());
	}
  m[where].flags|=flag;
}

// objectsSet=objects at matchWhere.
void cSource::setObjectsFromMatchedAtPosition(int where,vector <cOM> &objectsSet,int matchWhere,bool identifiedAsSpeaker,unsigned __int64 flag)
{ LFS
  if (m[matchWhere].objectMatches.size())
    objectsSet=m[matchWhere].objectMatches;
  else
  {
    objectsSet.clear();
    objectsSet.push_back(cOM(m[matchWhere].getObject(),SALIENCE_THRESHOLD));
  }
  wstring tmpstr,tmpstr2;
  if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:SOMP Set %s to:%s (%s)",abs(where),(identifiedAsSpeaker) ? L"audience":L"speaker",objectString(objectsSet,tmpstr,true).c_str(),speakerResolutionFlagsString(flag,tmpstr2).c_str());
}

// remove all elements from removeFrom that are also in m[matchWhere].objectMatches.
bool cSource::removeMatchedFromObjects(int where,vector <cOM> &removeFrom,int matchWhere,bool identifiedAsSpeaker)
{ LFS
  if (where<0) return false;
  wstring tmpstr;
	bool objectsRemoved=false;
  vector <cOM>::iterator si,ti,tiEnd;
  if (m[matchWhere].objectMatches.size())
  {
    for (ti=m[matchWhere].objectMatches.begin(),tiEnd=m[matchWhere].objectMatches.end(); ti!=tiEnd; ti++)
    {
      if ((si=in(ti->object,removeFrom))!=removeFrom.end())
      {
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_RESOLUTION,L"%06d:erased speaker %s in matched speaker resolution.",where,objectString(*si,tmpstr,true).c_str());
        if (identifiedAsSpeaker)
        {
          objects[si->object].numIdentifiedAsSpeaker--;
          objects[si->object].numIdentifiedAsSpeakerInSection--;
          unsigned int I;
          for (I=0; I<localObjects.size(); I++)
            if (localObjects[I].om.object==si->object)
              localObjects[I].numIdentifiedAsSpeaker--;
        }
        removeFrom.erase(si);
				objectsRemoved=true;
      }
    }
  }
  else
  {
    if (objectsRemoved=(si=in(m[matchWhere].getObject(),removeFrom))!=removeFrom.end())
    {
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_RESOLUTION,L"%06d:erased speaker %s in matched speaker resolution (2).",where,objectString(*si,tmpstr,true).c_str());
      removeFrom.erase(si);
    }
  }
	return objectsRemoved && removeFrom.size()==1;
}

/*
0 speaker
1 notspeaker
2 speaker
3 notspeaker
4 speaker
5 notspeaker
6 speaker
*/
void cSource::resolveSpeakersUsingPreviousSubject(int where)
{ LFS
	// find speaker group with the first unresolvedSpeaker in it
  if (unresolvedSpeakers.empty()) return;
	int firstUnresolved=abs(unresolvedSpeakers[0]),sg;
	for (sg=currentSpeakerGroup; sg>=0 && (firstUnresolved<speakerGroups[sg].sgBegin || firstUnresolved>=speakerGroups[sg].sgEnd); sg--);
  if (sg<0) return;
  vector <int> speakersFound;
  for (unsigned int I=0; I<subjectsInPreviousUnquotedSection.size(); I++)
    if (speakerGroups[sg].speakers.find(subjectsInPreviousUnquotedSection[I])!=speakerGroups[currentSpeakerGroup].speakers.end())
      speakersFound.push_back(subjectsInPreviousUnquotedSection[I]);
	// in case speakerGroups is slightly inaccurate
  if (speakersFound.empty() && sg>0) 
    for (unsigned int I=0; I<subjectsInPreviousUnquotedSection.size(); I++)
	    if (speakerGroups[sg-1].speakers.find(subjectsInPreviousUnquotedSection[I])!=speakerGroups[sg-1].speakers.end())
		    speakersFound.push_back(subjectsInPreviousUnquotedSection[I]);
  wstring tmpstr;
	bool oneIn,allIn;
	if (intersect(whereSubjectsInPreviousUnquotedSection,speakerGroups[sg].povSpeakers,allIn,oneIn) && (m[firstUnresolved].flags&cWordMatch::flagGenderIsAmbiguousResolveSpeakers) &&
		  m[firstUnresolved].objectMatches.size()==1)
		return;
  if (speakersFound.size()!=1) 
	{
		if (m[whereSubjectsInPreviousUnquotedSection].getObject()>=0 && !objects[m[whereSubjectsInPreviousUnquotedSection].getObject()].plural && debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:ZXZ Previous subject of %d:%s not usable for unresolved speaker starting at %d.",where,whereSubjectsInPreviousUnquotedSection,objectString(subjectsInPreviousUnquotedSection,tmpstr).c_str(),firstUnresolved);
		// if no speakers found, or there is already only one choice for the first unresolved position, or the subjects are derived for a plural pronoun (like they), reject.
		if (speakersFound.empty() || m[firstUnresolved].objectMatches.size()==1 || (m[whereSubjectsInPreviousUnquotedSection].getObject()>=0 && objects[m[whereSubjectsInPreviousUnquotedSection].getObject()].plural)) return;
		speakersFound.erase(speakersFound.begin()+1,speakersFound.end());
	}
	// if only one speaker and unresolved quote is immediately after the subject,
	// and speaker has not been resolved more reliably, and number of speakers in speaker group > 2,
	// and if it would be blocked (alternate resolve speaker already set) or 
	//     unresolvedSpeakers[urs]<0 (indicating matches already set so resolveSpeakersByAlternationForwards will skip it)
	//   override alternately set speaker.
	int urs=0,immediatelyAfterSubject=firstUnresolved,sgAt; 
	vector <int>::iterator dni=lower_bound(definitelyIdentifiedAsSpeakerInSpeakerGroups.begin(),definitelyIdentifiedAsSpeakerInSpeakerGroups.end(),immediatelyAfterSubject);
	definitelyIdentifiedAsSpeakerInSpeakerGroups.insert(dni,immediatelyAfterSubject);
	// get speaker group at the point right after the subject
	for (sgAt=0; sgAt<(signed)speakerGroups.size() && speakerGroups[sgAt].sgEnd<immediatelyAfterSubject; sgAt++);
	if (speakersFound.size()==1 && 
			!((m[immediatelyAfterSubject].flags)&cWordMatch::flagDefiniteResolveSpeakers) &&
			sgAt<(signed)speakerGroups.size() && (speakerGroups[sgAt].speakers.size())>2 &&
			((m[immediatelyAfterSubject].flags&(cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers|cWordMatch::flagEmbeddedStoryResolveSpeakers)) ||
			unresolvedSpeakers[urs]<0) &&
			(!((m[immediatelyAfterSubject].flags)&cWordMatch::flagSpecifiedResolveAudience) || m[m[immediatelyAfterSubject].audiencePosition].getObject()!=speakersFound[0]))
	{
		// verify that if a speaker was defined, that the gender of the speaker match the gender of speakersFound.
		if (m[immediatelyAfterSubject].speakerPosition>=0 && !objects[m[m[immediatelyAfterSubject].speakerPosition].getObject()].matchGender(objects[speakersFound[0]]))
		{
			wstring tmpstr2;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:ZXZ RESOLVESPEAKERS override of %s over %s rejected due to different gender",
			        where,objectString(speakersFound,tmpstr).c_str(),objectString(m[m[immediatelyAfterSubject].speakerPosition].getObject(),tmpstr2,true).c_str());
			return;
		}
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:ZXZ RESOLVESPEAKERS override using previous subject of %s",where,objectString(speakersFound,tmpstr).c_str());
		setObjectsFromMatchedAtPosition(immediatelyAfterSubject,m[immediatelyAfterSubject].objectMatches,objects[speakersFound[0]].originalLocation,false,cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers);
		removeMatchedFromObjects(immediatelyAfterSubject,m[immediatelyAfterSubject].audienceObjectMatches,objects[speakersFound[0]].originalLocation,false);
		if (m[immediatelyAfterSubject].audienceObjectMatches.empty())
		{
      for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
        if (in(*si,m[immediatelyAfterSubject].objectMatches)==m[immediatelyAfterSubject].objectMatches.end())
          m[immediatelyAfterSubject].audienceObjectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
			m[immediatelyAfterSubject].flags|=cWordMatch::flagAudienceFromSpeakerGroupResolveAudience;
		}
	  for (vector <cOM>::iterator nmi=m[immediatelyAfterSubject].objectMatches.begin(),nmiEnd=m[immediatelyAfterSubject].objectMatches.end(); nmi!=nmiEnd; nmi++)
			objects[nmi->object].speakerLocations.insert(immediatelyAfterSubject);
		m[immediatelyAfterSubject].flags|=cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers;
		if (m[immediatelyAfterSubject].audienceObjectMatches.size()==1 && m[immediatelyAfterSubject].audienceObjectMatches[0].object==speakersFound[0])
		{
			// get any other agreement speaker in currentSpeakerGroup except for current speaker
			m[immediatelyAfterSubject].audienceObjectMatches.clear();
			for (set <int>::iterator si=speakerGroups[sgAt].speakers.begin(); si!=speakerGroups[sgAt].speakers.end(); si++)
				if (*si!=speakersFound[0])
				{
					m[immediatelyAfterSubject].audienceObjectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
					objects[*si].speakerLocations.insert(immediatelyAfterSubject);
		      for (int forwardLink=m[immediatelyAfterSubject].getQuoteForwardLink(); forwardLink>=0; forwardLink=m[forwardLink].getQuoteForwardLink())
						objects[*si].speakerLocations.insert(forwardLink);
					if (m[immediatelyAfterSubject].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers)
						for (int embeddedStoryLink=m[immediatelyAfterSubject].nextQuote; embeddedStoryLink>=0 && (m[embeddedStoryLink].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers) && (m[embeddedStoryLink].speakerPosition<0 || (m[embeddedStoryLink].flags&cWordMatch::flagGenderIsAmbiguousResolveSpeakers)); embeddedStoryLink=m[embeddedStoryLink].nextQuote)
						{
							objects[*si].speakerLocations.insert(embeddedStoryLink);
							if (m[embeddedStoryLink].speakerPosition>=0)
								objects[*si].speakerLocations.insert(m[embeddedStoryLink].speakerPosition);
				      for (int forwardLink=m[embeddedStoryLink].getQuoteForwardLink(); forwardLink>=0; forwardLink=m[forwardLink].getQuoteForwardLink())
							{
								objects[*si].speakerLocations.insert(forwardLink);
								if (m[embeddedStoryLink].speakerPosition>=0)
									objects[*si].speakerLocations.insert(m[embeddedStoryLink].speakerPosition);
							}
						}
				}
			m[immediatelyAfterSubject].flags|=cWordMatch::flagAlternateResolveForwardFromLastSubjectAudience;
		}
    for (int forwardLink=m[immediatelyAfterSubject].getQuoteForwardLink(); forwardLink>=0; forwardLink=m[forwardLink].getQuoteForwardLink())
    {
			setSpeakerMatchesFromPosition(where,m[forwardLink].objectMatches,immediatelyAfterSubject,L"override using previous subject",m[immediatelyAfterSubject].flags|cWordMatch::flagForwardLinkResolveSpeakers);
			m[forwardLink].audienceObjectMatches=m[immediatelyAfterSubject].audienceObjectMatches;
    }
		if (m[immediatelyAfterSubject].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers)
			for (int embeddedStoryLink=m[immediatelyAfterSubject].nextQuote; embeddedStoryLink>=0 && (m[embeddedStoryLink].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers) && (m[embeddedStoryLink].speakerPosition<0 || (m[embeddedStoryLink].flags&cWordMatch::flagGenderIsAmbiguousResolveSpeakers)); embeddedStoryLink=m[embeddedStoryLink].nextQuote)
			{
				setSpeakerMatchesFromPosition(where,m[embeddedStoryLink].objectMatches,immediatelyAfterSubject,L"override using previous subject",m[immediatelyAfterSubject].flags|cWordMatch::flagForwardLinkResolveSpeakers);
				if (m[embeddedStoryLink].speakerPosition>=0)
					setSpeakerMatchesFromPosition(where,m[m[embeddedStoryLink].speakerPosition].objectMatches,immediatelyAfterSubject,L"override using previous subject",m[immediatelyAfterSubject].flags|cWordMatch::flagForwardLinkResolveSpeakers);
				m[embeddedStoryLink].audienceObjectMatches=m[immediatelyAfterSubject].audienceObjectMatches;
				for (int forwardLink=m[embeddedStoryLink].getQuoteForwardLink(); forwardLink>=0; forwardLink=m[forwardLink].getQuoteForwardLink())
				{
					setSpeakerMatchesFromPosition(where,m[forwardLink].objectMatches,immediatelyAfterSubject,L"override using previous subject",m[immediatelyAfterSubject].flags|cWordMatch::flagForwardLinkResolveSpeakers);
					if (m[forwardLink].speakerPosition>=0)
						setSpeakerMatchesFromPosition(where,m[m[forwardLink].speakerPosition].objectMatches,immediatelyAfterSubject,L"override using previous subject",m[immediatelyAfterSubject].flags|cWordMatch::flagForwardLinkResolveSpeakers);
					m[forwardLink].audienceObjectMatches=m[immediatelyAfterSubject].audienceObjectMatches;
				}
			}
		reEvaluateHailedObjects(immediatelyAfterSubject,false);
	}
	resolveSpeakersByAlternationForwards(where,objects[speakersFound[0]].originalLocation,true);
}

void cSource::setAudience(int where,int whereQuote,int currentSpeakerWhere)
{ LFS
	wstring tmpstr;
	// scan quote for audience - if supposed audience is included in the quote, then reject
	bool audienceFoundInQuote=false,allIn,oneIn;
	// if audienceObjectMatches is empty, then this may be the only chance to fill it.
	if (m[whereQuote].audienceObjectMatches.size()>0 || speakerGroups[currentSpeakerGroup].speakers.size()>2)
	{
		for (int ai=whereQuote; ai<m[whereQuote].endQuote && !audienceFoundInQuote; ai++)
			audienceFoundInQuote=m[ai].getObject()>=0 && !(m[ai].objectRole&(HAIL_ROLE|PP_OBJECT_ROLE)) && (m[ai].objectRole&FOCUS_EVALUATED) &&
					(intersect(ai,m[currentSpeakerWhere].objectMatches,allIn,oneIn) || in(m[currentSpeakerWhere].getObject(),ai)) &&
					// don't count cases where the audience is a name and the object@ai is not a name
					// this is to prevent inadvertent audience cancellations when objects other than names match names
					// You[tuppence] are such young things[tuppence] , both[tommy,tuppence] of you[tuppence] . I[mr] shouldn't like anything to happen to you[tuppence] . ”
					(m[currentSpeakerWhere].getObject()<0 || 
					 objects[m[currentSpeakerWhere].getObject()].objectClass!=NAME_OBJECT_CLASS ||
					 objects[m[ai].getObject()].objectClass==NAME_OBJECT_CLASS);
	}
	if (audienceFoundInQuote)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:set audience matches rejected because audience found in quote @%d",where,whereQuote);
	}
	else if (!(m[whereQuote].flags&(cWordMatch::flagSpecifiedResolveAudience|cWordMatch::flagForwardLinkAlternateResolveAudience)) ||
			m[whereQuote].audienceObjectMatches.size()!=1)
	{
		setSpeakerMatchesFromPosition(whereQuote,m[whereQuote].audienceObjectMatches,currentSpeakerWhere,L"audience forward link",m[where].flags|cWordMatch::flagForwardLinkAlternateResolveAudience);
		if (m[whereQuote].objectMatches.size()!=1)
			subtract(m[whereQuote].objectMatches,m[whereQuote].audienceObjectMatches);
	}
	else if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:added audience (5) rejected %s because audience already set",whereQuote,objectString(m[whereQuote].audienceObjectMatches,tmpstr,true).c_str());
}

void cSource::resolveAudienceByAlternation(int currentSpeakerWhere,bool definitelySpeaker,int urs,bool forwards)
{ LFS
  wstring tmpstr;
	unsigned __int64 flagAlternation=(forwards) ? cWordMatch::flagAlternateResolveForwardFromLastSubjectAudience : cWordMatch::flagAlternateResolveBackwardFromDefiniteAudience; // reject alternate only if going backward, not forward from last subject
  int ursWhere=abs(unresolvedSpeakers[urs]);
  // reject this resolution if removing the speaker would result in no speakers at all
  if (m[ursWhere].objectMatches.size()!=1 || 
		  (definitelySpeaker && (m[ursWhere].flags&(cWordMatch::flagMostLikelyResolveAudience|
				cWordMatch::flagDefiniteResolveSpeakers|cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers|cWordMatch::flagAlternateResolveForwardAfterDefinite|
				cWordMatch::flagFromLastDefiniteResolveAudience|cWordMatch::flagSpecifiedResolveAudience|cWordMatch::flagAlternateResolveBackwardFromDefiniteAudience|
				cWordMatch::flagFromPreviousHailResolveSpeakers|cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers))==cWordMatch::flagMostLikelyResolveAudience) ||
      (m[ursWhere].objectMatches[0].object!=m[currentSpeakerWhere].getObject() &&
       in(m[ursWhere].objectMatches[0].object,m[currentSpeakerWhere].objectMatches)==m[currentSpeakerWhere].objectMatches.end()))
  {
		// scan quote for audience - if supposed audience is included in the quote, then reject
		bool audienceFoundInQuote=false,propagateForward=false,allIn,oneIn,audienceIsSpeaker=false;
		for (int ai=ursWhere; ai<m[ursWhere].endQuote && !audienceFoundInQuote; ai++)
			audienceFoundInQuote=m[ai].getObject()>=0 && !(m[ai].objectRole&(HAIL_ROLE|PP_OBJECT_ROLE)) && (m[ai].objectRole&FOCUS_EVALUATED) &&
				  in(m[ai].getObject(),m[currentSpeakerWhere].objectMatches)!=m[currentSpeakerWhere].objectMatches.end();
		if (audienceFoundInQuote)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:set audience matches rejected because audience found in quote",ursWhere);
		}
		else if (audienceIsSpeaker=intersect(m[ursWhere].objectMatches,m[currentSpeakerWhere].objectMatches,allIn,oneIn) && allIn)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:set audience matches rejected because audience is speaker",ursWhere);
		}
		else if (propagateForward=!(m[ursWhere].flags&(flagAlternation|cWordMatch::flagAlternateResolveForwardAfterDefinite|
							cWordMatch::flagFromLastDefiniteResolveAudience|cWordMatch::flagSpecifiedResolveAudience|
							cWordMatch::flagFromPreviousHailResolveSpeakers)) ||
						m[ursWhere].audienceObjectMatches.empty() ||
						(m[ursWhere].audienceObjectMatches.size()>1 && 
						 intersect(currentSpeakerWhere,m[ursWhere].audienceObjectMatches,allIn,oneIn)))
		{
			setObjectsFromMatchedAtPosition(unresolvedSpeakers[urs],m[ursWhere].audienceObjectMatches,currentSpeakerWhere,true,flagAlternation);
			if (m[ursWhere].audienceObjectMatches.size()==1 && m[ursWhere].objectMatches.size()>1)
				subtract(m[ursWhere].audienceObjectMatches[0].object,m[ursWhere].objectMatches);
			reEvaluateHailedObjects(abs(unresolvedSpeakers[urs]),true);
		}
		else if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:set audience matches rejected because alternate already set",ursWhere);
		if (!audienceIsSpeaker && (!(m[ursWhere].flags&(cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers|cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers)) || (m[ursWhere].objectMatches.size()!=1)))
		{
      propagateForward|=removeMatchedFromObjects(abs(unresolvedSpeakers[urs]),m[ursWhere].objectMatches,currentSpeakerWhere,true);
			if (m[ursWhere].speakerPosition>=0 && m[m[ursWhere].speakerPosition].objectMatches.size()>1)
	      removeMatchedFromObjects(m[ursWhere].speakerPosition,m[m[ursWhere].speakerPosition].objectMatches,currentSpeakerWhere,true);	  
		}
		else if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:remove from matches rejected because alternate already set",ursWhere);
		if (definitelySpeaker)
			m[ursWhere].flags|=flagAlternation;
		reEvaluateHailedObjects(ursWhere,false);
		if (propagateForward)
		{
			for (int forwardLink=m[ursWhere].getQuoteForwardLink(); forwardLink>=0; forwardLink=m[forwardLink].getQuoteForwardLink())
				setAudience(ursWhere,forwardLink,currentSpeakerWhere);
			if (m[ursWhere].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers)
			{
				unsigned __int64 embeddedStoryType=m[ursWhere].flags&(cWordMatch::flagSecondEmbeddedStory|cWordMatch::flagFirstEmbeddedStory);
				for (int embeddedStoryLink=m[ursWhere].nextQuote; embeddedStoryLink>=0 && 
					(m[embeddedStoryLink].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers) && 
					embeddedStoryType==(m[embeddedStoryLink].flags&(cWordMatch::flagSecondEmbeddedStory|cWordMatch::flagFirstEmbeddedStory)) &&
					(m[embeddedStoryLink].speakerPosition<0 || (m[embeddedStoryLink].flags&cWordMatch::flagGenderIsAmbiguousResolveSpeakers)); embeddedStoryLink=m[embeddedStoryLink].nextQuote)
				{
					setAudience(ursWhere,embeddedStoryLink,currentSpeakerWhere);
		      for (int forwardLink=m[embeddedStoryLink].getQuoteForwardLink(); forwardLink>=0; forwardLink=m[forwardLink].getQuoteForwardLink())
					  setAudience(ursWhere,forwardLink,currentSpeakerWhere);
				}
			}
		}
  }
  else if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:speaker %s REJECTED for deletion from objectMatches (and set audienceObjectMatches)",ursWhere,
			(m[currentSpeakerWhere].objectMatches.size()) ? objectString(m[currentSpeakerWhere].objectMatches,tmpstr,false).c_str() : objectString(m[currentSpeakerWhere].getObject(),tmpstr,false).c_str());
  if (m[ursWhere].objectMatches.size()<=1 && definitelySpeaker && 
		  (currentSpeakerGroup>=speakerGroups.size() || speakerGroups[currentSpeakerGroup].speakers.size()<=2))
		m[ursWhere].flags|=cWordMatch::flagAlternateResolutionFinishedSpeakers;
}

bool cSource::matchedObjectsEqual(vector <cOM> &match1,vector <cOM> &match2)
{ LFS
	if (match1.size()!=match2.size()) return false;
	for (vector <cOM>::iterator omi=match1.begin(),omiEnd=match1.end(); omi!=omiEnd; omi++)
		if (in(omi->object,match2)==match2.end()) return false;
	return true;
}

bool cSource::resolveMatchesByAlternation(int currentSpeakerWhere,bool definitelySpeaker,int urs,bool forwards)
{ LFS
  wstring tmpstr;
  int ursWhere=abs(unresolvedSpeakers[urs]),o;
	bool notSpeakersResolved=false;
	if (!(m[ursWhere].flags&(cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers|cWordMatch::flagAlternateResolveForwardAfterDefinite|cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers)) || (m[ursWhere].objectMatches.size()!=1))
	{
		if (m[ursWhere].audienceObjectMatches.size()==1 &&
		    (m[ursWhere].audienceObjectMatches[0].object==m[currentSpeakerWhere].getObject() ||
			   in(m[ursWhere].audienceObjectMatches[0].object,m[currentSpeakerWhere].objectMatches)!=m[currentSpeakerWhere].objectMatches.end()))
		{
			// if marked cWordMatch::flagMostLikelyResolveSpeakers, and speakerGroup > 1 speaker with matching gender, allow this, hoping that the reresolve would solve it.
			bool allIn,oneIn;
			int genderAgreement=0;
			if ((m[ursWhere].flags&cWordMatch::flagMostLikelyResolveSpeakers) && speakerGroups[currentSpeakerGroup].speakers.size()>2)
				for (set<int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
					genderAgreement+=(objects[*si].matchGender(objects[m[ursWhere].audienceObjectMatches[0].object])) ? 1 : 0;
			if (genderAgreement<=1)
			{
				bool fromLikely=(m[ursWhere].flags&(cWordMatch::flagMostLikelyResolveSpeakers|cWordMatch::flagMostLikelyResolveAudience))==(cWordMatch::flagMostLikelyResolveSpeakers|cWordMatch::flagMostLikelyResolveAudience);
				// only pure most likely, going forwards and the previous connected quote does not have the same speaker
				bool overrideLikely=fromLikely && m[ursWhere].previousQuote>=0 && forwards && !(m[m[ursWhere].previousQuote].flags&cWordMatch::flagLastContinuousQuote) && 
					  !intersect(currentSpeakerWhere,m[m[ursWhere].previousQuote].objectMatches,allIn,oneIn);
				if (!overrideLikely)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:set of speaker %s REJECTED %s because the result would leave objectMatches=audienceObjectMatches",ursWhere,objectString(m[currentSpeakerWhere].getObject(),tmpstr,true).c_str(),
								(fromLikely) ? L"from likely":L"");
					return false;
				}
				vector <cOM> om=m[ursWhere].objectMatches;
				m[ursWhere].objectMatches=m[ursWhere].audienceObjectMatches;
				m[ursWhere].audienceObjectMatches=om;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Flipped on forwards alternation because objects to set audience=objects already matched (overrideLikely)",ursWhere);
			}
		}
		// the below example removes the possibility for an absolute rule about speakers not being allowed to say their own name.
		// “[tuppence:tommy] I[tuppence] have got it[list,thing] . how ishas that for clever little Tuppence ? ” 
		//if (!matchedObjectsEqual(m[ursWhere].objectMatches,m[currentSpeakerWhere].objectMatches) && scanQuoteForObject(ursWhere,currentSpeakerWhere))
		//{
		//	if (t.traceSpeakerResolution)
		//		lplog(LOG_RESOLUTION,L"%06d:set of speaker %s REJECTED because the quote contains a reference to it",ursWhere,objectString(m[currentSpeakerWhere].getObject(),tmpstr,true).c_str());
		//	return false;
		//}
		if (forwards && m[ursWhere].audiencePosition>=0 && intersect(currentSpeakerWhere,m[ursWhere].audiencePosition))
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Reject resolveForwardFromLastSubject because audience is set and conflicts.",ursWhere);
			return false;
		}
		if (!(m[ursWhere].flags&cWordMatch::flagFromPreviousHailResolveSpeakers) ||
			   // previous hail could have been ambiguous
			  (m[ursWhere].objectMatches.size()!=1 && 
				 ((m[currentSpeakerWhere].objectMatches.empty() && in(m[currentSpeakerWhere].getObject(),m[ursWhere].objectMatches)!=m[ursWhere].objectMatches.end()) ||
				  (m[currentSpeakerWhere].objectMatches.size()==1 && in(m[currentSpeakerWhere].objectMatches[0].object,m[ursWhere].objectMatches)!=m[ursWhere].objectMatches.end()))))
		{
			if (m[ursWhere].speakerPosition<0 || m[m[ursWhere].speakerPosition].getObject()<0 || 
				  (m[currentSpeakerWhere].objectMatches.empty() && objects[m[currentSpeakerWhere].getObject()].matchGender(objects[m[m[ursWhere].speakerPosition].getObject()])) ||
					(m[currentSpeakerWhere].objectMatches.size()==1 && objects[m[currentSpeakerWhere].objectMatches[0].object].matchGender(objects[m[m[ursWhere].speakerPosition].getObject()])))
				setObjectsFromMatchedAtPosition(ursWhere,m[ursWhere].objectMatches,currentSpeakerWhere,false,(forwards) ? cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers : cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers);
			else
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Reject resolveForwardFromLastSubject because speaker gender conflicts.",ursWhere);
				return false;
			}
		}
		if (definitelySpeaker)
			m[ursWhere].flags|=(forwards) ? cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers : cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers;
		notSpeakersResolved=reEvaluateHailedObjects(ursWhere,true);
		for (int forwardLink=m[ursWhere].getQuoteForwardLink(); forwardLink>=0; forwardLink=m[forwardLink].getQuoteForwardLink())
			setSpeakerMatchesFromPosition(forwardLink,m[forwardLink].objectMatches,currentSpeakerWhere,L"resolve speakers forward Link",m[ursWhere].flags|cWordMatch::flagForwardLinkAlternateResolveSpeakers);
		for (unsigned int I=0; I<m[ursWhere].objectMatches.size(); I++)
		{
			objects[o=m[ursWhere].objectMatches[I].object].numIdentifiedAsSpeaker++;
			objects[o].numIdentifiedAsSpeakerInSection++;
			if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection+objects[o].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
				sections[section].speakerObjects.push_back(cOM(o,SALIENCE_THRESHOLD));
			vector <cLocalFocus>::iterator lsi=in(o);
			if (lsi!=localObjects.end())
				lsi->numIdentifiedAsSpeaker++; 
		}
	  if (m[ursWhere].audienceObjectMatches.size()<=1 && definitelySpeaker && 
			  (currentSpeakerGroup>=speakerGroups.size() || speakerGroups[currentSpeakerGroup].speakers.size()<=2))
			m[ursWhere].flags|=cWordMatch::flagAlternateResolutionFinishedSpeakers;
	}
	else if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:set matches rejected because alternate already set",ursWhere);
  // reject this resolution if removing the speaker from counterspeakers would result in no counterspeakers at all
	if (m[ursWhere].audienceObjectMatches.size()!=1 ||
      (m[ursWhere].audienceObjectMatches[0].object!=m[currentSpeakerWhere].getObject() &&
       in(m[ursWhere].audienceObjectMatches[0].object,m[currentSpeakerWhere].objectMatches)==m[currentSpeakerWhere].objectMatches.end()))
	{
		if (!(m[ursWhere].flags&cWordMatch::flagAlternateResolveBackwardFromDefiniteAudience) || (m[ursWhere].audienceObjectMatches.size()!=1))
			notSpeakersResolved|=removeMatchedFromObjects(ursWhere,m[ursWhere].audienceObjectMatches,currentSpeakerWhere,false);
		else if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:remove from not matches rejected because alternate already set",ursWhere);
	}
  else if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:speaker %s REJECTED for deletion from audienceObjectMatches (and set objectMatches)",ursWhere,
          objectString(m[currentSpeakerWhere].getObject(),tmpstr,true).c_str());
	return notSpeakersResolved;
}

struct sFlagString {
	unsigned __int64 flag;
	wchar_t *s;
} RSFlagStrings[] = {
	{ cWordMatch::flagAlternateResolveForwardAfterDefinite, L"AlternateForwardAfterDefinite " },

	{ cWordMatch::flagFromPreviousHailResolveSpeakers, L"FromPreviousHail " },
	{ cWordMatch::flagMostLikelyResolveSpeakers, L"MostLikely " },
	{ cWordMatch::flagGenderIsAmbiguousResolveSpeakers, L"GenderAmbiguous " },
	{ cWordMatch::flagForwardLinkAlternateResolveSpeakers, L"ForwardLinkAlternate " },
	{ cWordMatch::flagForwardLinkResolveSpeakers, L"ForwardLink " },
	{ cWordMatch::flagDefiniteResolveSpeakers, L"Definite " },
	{ cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers, L"AlternateForwardFromLastSubject " },
	{ cWordMatch::flagAlternateResolveBackwardFromDefiniteSpeakers, L"AlternateBackwardFromDefinite " },
	{ cWordMatch::flagAlternateResolutionFinishedSpeakers, L"AlternateFinished " },
	{ cWordMatch::flagEmbeddedStoryResolveSpeakers, L"EmbeddedStory " },
	{ 0,NULL },
	{ cWordMatch::flagMostLikelyResolveAudience, L"MostLikely " },
	{ cWordMatch::flagGenderIsAmbiguousResolveAudience, L"GenderAmbiguous " },
	{ cWordMatch::flagForwardLinkResolveAudience, L"ForwardLink " },
	{ cWordMatch::flagForwardLinkAlternateResolveAudience, L"ForwardLinkAlternate " },
	{ cWordMatch::flagFromLastDefiniteResolveAudience, L"FromLastDefinite " },
	{ cWordMatch::flagSpecifiedResolveAudience, L"Audience " },
	{ cWordMatch::flagHailedResolveAudience, L"Hailed " },
	{ cWordMatch::flagAlternateResolveForwardFromLastSubjectAudience, L"AlternateForwardFromLastSubject " },
	{ cWordMatch::flagAlternateResolveBackwardFromDefiniteAudience, L"AlternateBackwardFromDefinite " },
	{ cWordMatch::flagAudienceFromSpeakerGroupResolveAudience, L"AudienceFromSpeakerGroup " },
	{ 0,NULL }
};

wstring cSource::speakerResolutionFlagsString(__int64 flags,wstring &tmpstr)
{ LFS
	tmpstr.clear();
	unsigned int I;
	for (I=0; RSFlagStrings[I].s; I++)
		if (flags&RSFlagStrings[I].flag)
			tmpstr+=RSFlagStrings[I].s;
	tmpstr+=L"//";
	for (I++; RSFlagStrings[I].s; I++)
		if (flags&RSFlagStrings[I].flag)
			tmpstr+=RSFlagStrings[I].s;
	return tmpstr;
}

void cSource::printUnresolvedLocation(int urs)
{ LFS
	int ursWhere=abs(unresolvedSpeakers[urs]);
	bool finished=(m[ursWhere].flags&cWordMatch::flagAlternateResolutionFinishedSpeakers)!=0,notMatched=(unresolvedSpeakers[urs]<0); // objectsNotMatched is unresolved;
	wstring tmpstr,tmpstr2,tmpstr3;
	lplog(LOG_RESOLUTION,L"%06d:--%s %s--  %s//%s %s",ursWhere,(finished) ? L"finished":L"unresolved",(notMatched) ? L"audience":L"speaker",
					objectString(m[ursWhere].objectMatches,tmpstr,true).c_str(),objectString(m[ursWhere].audienceObjectMatches,tmpstr2,true).c_str(),
					speakerResolutionFlagsString(m[ursWhere].flags,tmpstr3).c_str());
}

void cSource::resolveSpeakersByAlternationForwards(int where,int currentSpeakerWhere,bool definitelySpeaker)
{ LFS
  wstring tmpstr,tmpstr2;
  if (debugTrace.traceSpeakerResolution)
  {
		lplog(LOG_RESOLUTION,L"%06d:RESOLVESPEAKERS alternating speaker forwards of %d:%s (%s)",where,whereSubjectsInPreviousUnquotedSection,objectString(m[currentSpeakerWhere].getObject(),tmpstr,false).c_str(),(definitelySpeaker) ? L"definite speaker":L"undefinite");
    for (int urs=0; urs<(signed)unresolvedSpeakers.size(); urs++)
			printUnresolvedLocation(urs);
  }
	// when currentSpeakerWhere is processed, a counterSpeaker may become uniquely established as well.
	int counterSpeakerEstablished=-1,counterSpeakerEstablishedPosition=-1;
	vector <cOM> objectsSet;
	if (m[currentSpeakerWhere].objectMatches.size())
    objectsSet=m[currentSpeakerWhere].objectMatches;
  else
    objectsSet.push_back(cOM(m[currentSpeakerWhere].getObject(),SALIENCE_THRESHOLD));
  for (int urs=0; urs<(signed)unresolvedSpeakers.size(); urs++)
  {
		int ursWhere=abs(unresolvedSpeakers[urs]);
		// check to see whether this differs from more reliable checks:
		if (!(m[ursWhere].flags&cWordMatch::flagAlternateResolutionFinishedSpeakers))
		{
			if ((m[ursWhere].flags&(cWordMatch::flagDefiniteResolveSpeakers|cWordMatch::flagAudienceFromSpeakerGroupResolveAudience|cWordMatch::flagEmbeddedStoryResolveSpeakers)) &&
				  m[ursWhere].objectMatches.size()<=1)
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Stopped forwards alternation [MATCH] on flags: %s",ursWhere,speakerResolutionFlagsString(m[ursWhere].flags,tmpstr).c_str());
				break;
			}
			// if the next quote is set definitely to the same speaker on this quote as we are setting alternatively, reject.
			if (m[ursWhere].nextQuote>=0 && (m[m[ursWhere].nextQuote].flags&cWordMatch::flagDefiniteResolveSpeakers) &&
				  m[m[ursWhere].nextQuote].objectMatches.size()==1 && 
					m[m[ursWhere].nextQuote].objectMatches[0].object==m[currentSpeakerWhere].getObject())
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Stopped forwards alternation [MATCH] because next definite speaker matches current",ursWhere);
				break;
			}
			if (counterSpeakerEstablishedPosition>=0)
				resolveAudienceByAlternation(counterSpeakerEstablishedPosition,definitelySpeaker,urs,true);
			//if (matchedObjectsEqual(objectsSet,m[ursWhere].audienceObjectMatches))
			//	break;
			if (resolveMatchesByAlternation(currentSpeakerWhere,definitelySpeaker,urs,true))
			{
				// this resolution led to another resolution by notMatches
				// is the next speaker unresolved?
				counterSpeakerEstablished=ursWhere;
			}
		}
		else if (urs==0 && m[ursWhere].objectMatches.size()==1 && objectsSet.size()==1 && m[ursWhere].objectMatches[0].object!=objectsSet[0].object)
		{
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Stopped forwards alternation [MATCH] because first speaker %s doesn't match last subject %s",ursWhere,objectString(m[ursWhere].objectMatches[0].object,tmpstr,true).c_str(),objectString(objectsSet[0].object,tmpstr2,true).c_str());
			break;
		}
		urs++;
		if (urs<(signed)unresolvedSpeakers.size() && !(m[ursWhere=abs(unresolvedSpeakers[urs])].flags&cWordMatch::flagAlternateResolutionFinishedSpeakers))
		{
			if (counterSpeakerEstablishedPosition>=0)
			{
				if (m[ursWhere].flags&moreReliableMatchedFlags)
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Stopped forwards alternation [MATCH counter speaker] on flags: %s",ursWhere,speakerResolutionFlagsString(m[ursWhere].flags,tmpstr).c_str());
					counterSpeakerEstablished=counterSpeakerEstablishedPosition=-1;
				}
				else
					resolveMatchesByAlternation(counterSpeakerEstablishedPosition,definitelySpeaker,urs,true);
			}
			if (matchedObjectsEqual(objectsSet,m[ursWhere].objectMatches))
			{
				if ((m[ursWhere].flags&(cWordMatch::flagMostLikelyResolveAudience|cWordMatch::flagMostLikelyResolveSpeakers))==(cWordMatch::flagMostLikelyResolveAudience|cWordMatch::flagMostLikelyResolveSpeakers))
				{
					vector <cOM> om=m[ursWhere].objectMatches;
					m[ursWhere].objectMatches=m[ursWhere].audienceObjectMatches;
					m[ursWhere].audienceObjectMatches=om;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Flipped on forwards alternation because objects to set audience=objects already matched",ursWhere);
				}
			  else
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Stopped forwards alternation because objects to set audience=objects already set as matched",ursWhere);
					break;
				}
			}
			if (counterSpeakerEstablished>=0 && counterSpeakerEstablishedPosition<0)
			{
				// is a counterSpeaker needed?
				if (m[ursWhere].objectMatches.size()!=1)
				{
					m[ursWhere].objectMatches=m[counterSpeakerEstablished].audienceObjectMatches;
					for (vector <cOM>::iterator nmi=m[ursWhere].objectMatches.begin(),nmiEnd=m[ursWhere].objectMatches.end(); nmi!=nmiEnd; nmi++)
						objects[nmi->object].speakerLocations.insert(ursWhere);
					if (debugTrace.traceSpeakerResolution)
					{
						lplog(LOG_RESOLUTION,L"%06d:set speaker to %s (from %d)",ursWhere,objectString(m[ursWhere].objectMatches,tmpstr,true).c_str(),counterSpeakerEstablished);
					}
					m[ursWhere].flags|=cWordMatch::flagAlternateResolveForwardFromLastSubjectSpeakers;
					counterSpeakerEstablishedPosition=ursWhere;
				}
				else
					counterSpeakerEstablished=-1;
			}
			bool lastMatch=(m[ursWhere].flags&moreReliableNotMatchedFlags)!=0;
			// give possibility of removing audience from speaker
			resolveAudienceByAlternation(currentSpeakerWhere,definitelySpeaker,urs,true);
			if (lastMatch) // previous call set reliable flag, so must set lastMatch before call
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Stopped forwards alternation [audience] on flags: %s",ursWhere,speakerResolutionFlagsString(m[ursWhere].flags,tmpstr).c_str());
				break;
			}
		}
  }
  // don't push one more speaker back because this call is not called for every unresolved speaker
}

// {G}
// requires a list of unresolvedSpeakers that
// 1. have no other text between them (no unquoted text)
// 2. do not include any quotes that have been extended
// each entry in this list is an integer pair pointing to the m array offset for each "
// these entries are in text-forward order
void cSource::resolvePreviousSpeakersByAlternationBackwards(int where,int currentSpeakerWhere,bool definitelySpeaker)
{ LFS
  wstring tmpstr,tmpstr2;
  if (debugTrace.traceSpeakerResolution)
  {
		if (m[currentSpeakerWhere].objectMatches.size())
			lplog(LOG_RESOLUTION,L"%06d:RESOLVESPEAKERS alternating speaker backwards of %d:%s (%s)",where,currentSpeakerWhere,objectString(m[currentSpeakerWhere].objectMatches,tmpstr,false).c_str(),(definitelySpeaker) ? L"definite speaker":L"undefinite");
		else
			lplog(LOG_RESOLUTION,L"%06d:RESOLVESPEAKERS alternating speaker backwards of %d:%s (%s)",where,currentSpeakerWhere,objectString(m[currentSpeakerWhere].getObject(),tmpstr,false).c_str(),(definitelySpeaker) ? L"definite speaker":L"undefinite");
    for (int urs=unresolvedSpeakers.size()-1; urs>=0; urs--)
			printUnresolvedLocation(urs);
  }
	__int64 inEmbeddedStory=m[where].flags&(cWordMatch::flagEmbeddedStoryResolveSpeakers|cWordMatch::flagSecondEmbeddedStory|cWordMatch::flagFirstEmbeddedStory);
  for (int urs=unresolvedSpeakers.size()-1; urs>=0; )
  {
		for (; urs>=0 && inEmbeddedStory && !(m[abs(unresolvedSpeakers[urs])].flags&cWordMatch::flagAlternateResolutionFinishedSpeakers) && 
					 (m[abs(unresolvedSpeakers[urs])].flags&(cWordMatch::flagEmbeddedStoryResolveSpeakers|cWordMatch::flagSecondEmbeddedStory|cWordMatch::flagFirstEmbeddedStory))==inEmbeddedStory; urs--)
			resolveMatchesByAlternation(currentSpeakerWhere,definitelySpeaker,urs,false);
		if (urs<0) break;
    //      assign speaker (S) to all even previous unknown objects.
    //      subtract speaker (S) from all odd previous unknown objects. (1 before, 3 before, etc)
    if (!(m[abs(unresolvedSpeakers[urs])].flags&cWordMatch::flagAlternateResolutionFinishedSpeakers))
			resolveAudienceByAlternation(currentSpeakerWhere,definitelySpeaker,urs,false);
		inEmbeddedStory=m[abs(unresolvedSpeakers[urs])].flags&(cWordMatch::flagEmbeddedStoryResolveSpeakers|cWordMatch::flagSecondEmbeddedStory|cWordMatch::flagFirstEmbeddedStory);
		urs--;
		if (urs<0) break;
		for (; urs>=0 && inEmbeddedStory && !(m[abs(unresolvedSpeakers[urs])].flags&cWordMatch::flagAlternateResolutionFinishedSpeakers) && 
					 (m[abs(unresolvedSpeakers[urs])].flags&(cWordMatch::flagEmbeddedStoryResolveSpeakers|cWordMatch::flagSecondEmbeddedStory|cWordMatch::flagFirstEmbeddedStory))==inEmbeddedStory; urs--)
			resolveAudienceByAlternation(currentSpeakerWhere,definitelySpeaker,urs,false);
		if (urs<0) break;
    if (unresolvedSpeakers[urs]>=0 && !(m[abs(unresolvedSpeakers[urs])].flags&cWordMatch::flagAlternateResolutionFinishedSpeakers))
			resolveMatchesByAlternation(currentSpeakerWhere,definitelySpeaker,urs,false);
		inEmbeddedStory=m[abs(unresolvedSpeakers[urs])].flags&(cWordMatch::flagEmbeddedStoryResolveSpeakers|cWordMatch::flagSecondEmbeddedStory|cWordMatch::flagFirstEmbeddedStory);
		urs--;
  }
  // push one more speaker back (flip speakers, so that the next time the previousSpeakers or currentSpeaker is resolved,
  //   we can remember the history.
  // even if there are no previous unresolved speakers, the present speaker NOT matched may not be known accurately.
  if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:set unresolvedSpeaker position %d to %d",where,unresolvedSpeakers.size(),(definitelySpeaker) ? -where:where);
	unresolvedSpeakers.push_back((definitelySpeaker) ? -where:where);
}

bool cSource::quotationsImmediatelyBefore(int beginQuote)
{ LFS
  if (!beginQuote) return false;
  vector <cWordMatch>::iterator im;
  for (im=m.begin()+beginQuote-1; im!=m.begin() && im->word==Words.sectionWord; im--);
  return im!=m.begin() && im->queryForm(quoteForm)>=0;
}

void cSource::displayQuoteContext(unsigned int begin, unsigned int end)
{
	LFS
	wstring qc = L"   ";
  for (unsigned int I=begin; I<end && I<m.size(); I++)
  {
    wchar_t temp[100];
		wsprintf(temp, L"(%d)", I);
		qc += temp;
		getOriginalWord(I, qc, true);
		qc += L" ";
    if ((I-begin)%10==0 && I!=begin) qc+=L"\n   ";
  }
  lplog(LOG_INFO,L"%s",qc.c_str());
}

// salience comparison for speakers - so these must appear out of quote
// put inQuote speakers last
bool salienceCompare(const cLocalFocus &lhs,const cLocalFocus &rhs)
{ LFS
	if (lhs.occurredOutsidePrimaryQuote && !rhs.occurredOutsidePrimaryQuote)
		return true;
	if (!lhs.occurredOutsidePrimaryQuote && rhs.occurredOutsidePrimaryQuote)
		return false;
  return lhs.om.salienceFactor>rhs.om.salienceFactor;
}

// resolved gendered objects that are HAIL object in quotes may not be resolved correctly.
// these objects cannot resolve to the speaker (but the speaker isn't known at the time)
// so, when a speaker is set (SS), scan the quote for HAILED gendered objects (HGO).
//   if the audience is definitely set, set the HAILED gendered object to this audience object.
//   if the audience object is not definitively set, and speakergroups are available,
//     for each speaker in speakergroup, 
//       if not SS, and matching sex and number, then add speaker to matches of HGO.
//     if only one match for HGO,
//       set audienceObject to match.
bool cSource::reEvaluateHailedObjects(int beginQuote,bool setMatched)
{ LFS
  bool reEvaluationOccurred=false;
	wstring tmpstr;
	int o,cSG=currentSpeakerGroup;
	while (((unsigned)cSG)<speakerGroups.size() && cSG>=0 && speakerGroups[cSG].sgBegin>beginQuote) cSG--;
	while (((unsigned)cSG)<speakerGroups.size() && cSG>=0 && speakerGroups[cSG].sgEnd<beginQuote) cSG++;
  for (int I=beginQuote; I<m[beginQuote].endQuote; I++)
	{
    if ((o=m[I].getObject())>=0 && (m[I].objectRole&HAIL_ROLE) &&
			  // 'sir', not 'archdeacon' - if 'archdeacon' were allowed, matching just by sex would not be sufficient
				((objects[o].objectClass==NAME_OBJECT_CLASS && objects[o].name.justHonorific() && objects[o].name.hon->second.query(L"pinr")>=0) ||
				 objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
         objects[o].objectClass==BODY_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
				 objects[o].objectClass==META_GROUP_OBJECT_CLASS))
		{
			if (!setMatched && m[beginQuote].audienceObjectMatches.size()>0 && objects[m[beginQuote].audienceObjectMatches[0].object].matchGender(objects[o]) && 
				  m[beginQuote].audienceObjectMatches[0].object!=o)
			{
				m[I].objectMatches.clear();
				for (vector <cOM>::iterator aom=m[beginQuote].audienceObjectMatches.begin(),aomEnd=m[beginQuote].audienceObjectMatches.end(); aom!=aomEnd; aom++)
					if (objects[aom->object].matchGender(objects[o]))
					{
						m[I].objectMatches.push_back(*aom);
						objects[aom->object].locations.push_back(I);
						objects[aom->object].updateFirstLocation(I);
				}
				reEvaluationOccurred=m[I].objectMatches.size()==1;
			}
			else if (cSG>=0 && cSG<(signed)speakerGroups.size())
			{
				m[I].objectMatches.clear();
				for (set <int>::iterator si=speakerGroups[cSG].speakers.begin(),siEnd=speakerGroups[cSG].speakers.end(); si!=siEnd; si++)
					if (in(*si,m[beginQuote].objectMatches)==m[beginQuote].objectMatches.end() && objects[*si].matchGender(objects[o]))
						m[I].objectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
				if (m[I].objectMatches.size()==1)
				{
					setSpeakerMatchesFromPosition(beginQuote,m[beginQuote].audienceObjectMatches,I,L"Unique reresolve of gender HAIL object audience",cWordMatch::flagHailedResolveAudience);
					reEvaluationOccurred=true;
				}
			}
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Reresolve of gender HAIL object resulted in %s",I,objectString(m[I].objectMatches,tmpstr,true).c_str());
		}
		/* re-evaluate meta group objects dependent on a first or second person owner? - not right now...
		if (o>=0 && objects[o].objectClass==META_GROUP_OBJECT_CLASS && 
		    objects[o].getOwnerWhere()>=0 && (m[objects[o].getOwnerWhere()].word->second.inflectionFlags&(FIRST_PERSON|SECOND_PERSON))!=0 &&
			 // if identity relationship created a match, don't rematch
			 (m[where].objectMatches.empty() || 
			 (!(m[where].objectRole&RE_OBJECT_ROLE) && (m[where].objectRole&(SUBJECT_ROLE|IS_OBJECT_ROLE|SUBJECT_PLEONASTIC_ROLE))!=(SUBJECT_ROLE|IS_OBJECT_ROLE))) &&
			 m[beginQuote].objectMatches.size()==1)
		{
			
		}
		*/
	}
	return reEvaluationOccurred;
}

void cSource::boostRecentSpeakers(int where,int beginQuote,int speakersConsidered,int speakersMentionedInLastParagraph,bool getMostLikelyAudience)
{ LFS
	vector <cLocalFocus>::iterator lsi;
	// consider who spoke most recently, but not if at least two speakers have been mentioned in the last paragraph
	if (speakerGroups[currentSpeakerGroup].speakers.size()>2 && speakersConsidered>2 && previousPrimaryQuote>=0 && speakersMentionedInLastParagraph<=1 &&
		  m[previousPrimaryQuote].objectMatches.size()==1 && m[previousPrimaryQuote].audienceObjectMatches.size()==1)
	{
		//if (getMostLikelyAudience)
		//{
			lsi=in(m[previousPrimaryQuote].objectMatches[0].object);
			if (lsi!=localObjects.end() && lsi->om.salienceFactor>=500)
			{
				lsi->om.salienceFactor+=15000;
				lsi->res+=L"+RECENT[+15000]";
			}
		//}
		//else
		//{
			lsi=in(m[previousPrimaryQuote].audienceObjectMatches[0].object);
			if (lsi!=localObjects.end() && lsi->om.salienceFactor>=500)
			{
				lsi->om.salienceFactor+=15000;
				lsi->res+=L"+RECENT[+15000]";
			}
		//}
		m[where].flags|=cWordMatch::flagResolvedByRecent;
	}
	if (speakerGroups[currentSpeakerGroup].speakers.size()>2 && speakersConsidered>1 && 
		  previousPrimaryQuote>=0 && m[previousPrimaryQuote].nextQuote==beginQuote && !(m[previousPrimaryQuote].flags&cWordMatch::flagLastContinuousQuote) &&
		  m[previousPrimaryQuote].objectMatches.size()==1 && m[previousPrimaryQuote].audienceObjectMatches.size()==1)
	{
		if (getMostLikelyAudience)
		{
			lsi=in(m[previousPrimaryQuote].objectMatches[0].object);
			if (lsi!=localObjects.end() && lsi->om.salienceFactor>=500)
			{
				lsi->om.salienceFactor+=30000;
				lsi->res+=L"+RECENT2[+30000]";
			}
		}
		else
		{
			lsi=in(m[previousPrimaryQuote].audienceObjectMatches[0].object);
			if (lsi!=localObjects.end() && lsi->om.salienceFactor>=500)
			{
				lsi->om.salienceFactor+=30000;
				lsi->res+=L"+RECENT2[+30000]";
			}
		}
		m[where].flags|=cWordMatch::flagResolvedByRecent;
	}
	if (speakerGroups[currentSpeakerGroup].speakers.size()>2 && speakersConsidered>2 && previousPrimaryQuote>=0 && m[previousPrimaryQuote].nextQuote==beginQuote &&
		  m[previousPrimaryQuote].objectMatches.size()==2 && (m[previousPrimaryQuote].flags&cWordMatch::flagResolvedByRecent))
	{
		lsi=in(m[previousPrimaryQuote].objectMatches[0].object);
		if (lsi!=localObjects.end() && lsi->om.salienceFactor>=500)
		{
			lsi->om.salienceFactor+=10000;
			lsi->res+=L"+RECENT3[+10000]";
		}
		lsi=in(m[previousPrimaryQuote].objectMatches[1].object);
		if (lsi!=localObjects.end() && lsi->om.salienceFactor>=500)
		{
			lsi->om.salienceFactor+=10000;
			lsi->res+=L"+RECENT3[+10000]";
		}
		m[where].flags|=cWordMatch::flagResolvedByRecent;
	}
}

//    assign the two most common localObjects (or two objects positively identified), and remove the previous objects.
//    assign the result back to previousSpeakers.
void cSource::getMostLikelySpeakers(unsigned int beginQuote,unsigned int endQuote,int lastDefiniteSpeaker,
                                   bool previousSpeakersUncertain,int wherePreviousLastSubjects,vector <int> &previousLastSubjects,
																	 int rejectObjectPosition,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb)
{ LFS
	tIWMM secondaryQuoteOpenWord=Words.gquery(L"‘"),secondaryQuoteCloseWord=Words.gquery(L"’");
	// if there are more than two speakers in the current speakerGroup, and
	// (all previousSpeakers were not groupedSpeakers, and beforePreviousSpeakers also were not in groupedSpeakers, OR
  //  all previousSpeakers were in groupedSpeakers, and beforePreviousSpeakers also were all in groupedSpeakers)
	// skip picking a new set of speakers and simply swap beforePreviousSpeakers and previousSpeakers.
  vector <cLocalFocus>::iterator lsi;
	// if object position to be excluded from current speaker (for example, a previous HAIL position) matches everything in the speakerGroup, cancel
	// it, as it will end up rejecting all speakers.
	if (rejectObjectPosition>=0 && currentSpeakerGroup<speakerGroups.size() && m[rejectObjectPosition].objectMatches.size()==speakerGroups[currentSpeakerGroup].speakers.size())
		rejectObjectPosition=lastDefiniteSpeaker;
  wstring tmpstr,tmpstr2;
  bool atLeastOne=false,objectsRejected=false,getMostLikelyAudience=(rejectObjectPosition==beginQuote);
  int saveBlockedObject=-1;
	// verify speaker objects in local salience
  if (usePIS && currentSpeakerGroup<speakerGroups.size())
  {
    set <int>::iterator pISO=speakerGroups[currentSpeakerGroup].speakers.begin(),pISOEnd=speakerGroups[currentSpeakerGroup].speakers.end();
    for (; pISO!=pISOEnd; pISO++)
    {
      if ((lsi=in(*pISO))==localObjects.end())
      {
        localObjects.push_back(cLocalFocus(*pISO,false,false,false,true));
        if (debugTrace.traceSpeakerResolution)
          lplog(LOG_RESOLUTION,L"%06d:%d Add speakerGroup's %s to local focus",beginQuote,currentSpeakerGroup,objectString(*pISO,tmpstr,false).c_str());
      }
      else // make sure a speaker which was previously only encountered in a quote is now considered for out of quotes
        lsi->occurredOutsidePrimaryQuote=true;
    }
  }
  vector <cLocalFocus>::iterator lsiEnd=localObjects.end();
	int speakersConsidered=0,speakersMentionedInLastParagraph=0;
  for (lsi=localObjects.begin(); lsi!=lsiEnd; lsi++)
  {
    lsi->res.clear();
		lsi->numMatchedAdjectives=0;
    if (!lsi->occurredOutsidePrimaryQuote) continue;
    lsi->om.salienceFactor=-1;
    if (usePIS && (speakerGroups[currentSpeakerGroup].speakers.find(lsi->om.object)==speakerGroups[currentSpeakerGroup].speakers.end() ||
			             find(speakerGroups[currentSpeakerGroup].observers.begin(),speakerGroups[currentSpeakerGroup].observers.end(),lsi->om.object)!=speakerGroups[currentSpeakerGroup].observers.end()))
      continue;
    unsigned int I;
    // reject names defined in current quote - "Tommy, old bean!" is unlikely to be said by Tommy.
		// unless it has speaker role [over the phone] - "Tommy here.  Get over here quick!"
    for (I=beginQuote; I<endQuote; I++)
		{
			// skip secondary quotes
			if (m[I].word==secondaryQuoteOpenWord) 
			{
				while (m[I].word!=secondaryQuoteCloseWord && I<endQuote) I++;
				continue;
			}
			if (m[I].getObject()==lsi->om.object)
			{
				if ((m[I].objectRole&IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE) && !getMostLikelyAudience)
				{
					lsi->om.salienceFactor+=10000;
					lsi->res+=L"+SELF_REFER_SPEAKER[+10000]";
					speakersConsidered=-1;
				}
				else if ((m[I].objectRole&IN_QUOTE_REFERRING_AUDIENCE_ROLE) && getMostLikelyAudience)
				{
					lsi->om.salienceFactor+=10000;
					lsi->res+=L"+REFER_AUDIENCE[+10000]";
					speakersConsidered=-1;
				}
				// can't be an appositive / that man, Boris Something, 
				else if (!(m[I].objectRole&RE_OBJECT_ROLE))
					break;
			}
		}
    if (I!=endQuote && (!(m[I].objectRole&(IN_QUOTE_REFERRING_AUDIENCE_ROLE|HAIL_ROLE)) || !getMostLikelyAudience)) 
    {
      if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Found in-quote %s%s.",I,objectString(lsi->om.object,tmpstr,true).c_str(),m[I].roleString(tmpstr2).c_str());
      // if speakers are uncertain, and there are only two speakers and one is disallowed by the above rule then
      // only one speaker will be returned, which will not be enough to fill in the audience which on subsequent elimination will result in UNKNOWN
      if (rejectObjectPosition<0 && !beforePreviousSpeakers.size() && !getMostLikelyAudience) // previous speaker uncertain so add
        saveBlockedObject=lsi->om.object;
      continue;
    }
    if (rejectObjectPosition>=0 && m[rejectObjectPosition].objectMatches.size()<=1 &&  
      (lsi->om.object==m[rejectObjectPosition].getObject() || 
			 in(lsi->om.object,m[rejectObjectPosition].objectMatches)!=m[rejectObjectPosition].objectMatches.end()))
    {
      objectsRejected=true;
      continue;
    }
    vector <cObject>::iterator o=objects.begin()+lsi->om.object;
    if (!o->male && !o->female && o->objectClass!=BODY_OBJECT_CLASS) continue;
    if (lsi->notSpeaker) continue;
    atLeastOne=true;
    lsi->om.salienceFactor=lsi->numDefinitelyIdentifiedAsSpeaker*SALIENCE_THRESHOLD+lsi->numIdentifiedAsSpeaker*300+lsi->numEncounters*200;
    if (o->objectClass==NAME_OBJECT_CLASS) lsi->om.salienceFactor+=500;
    else if (o->objectClass==GENDERED_GENERAL_OBJECT_CLASS) lsi->om.salienceFactor+=300;
    else if (o->objectClass==BODY_OBJECT_CLASS) lsi->om.salienceFactor+=300;
    else if (o->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS) lsi->om.salienceFactor+=500;
    else if (o->objectClass==GENDERED_DEMONYM_OBJECT_CLASS) lsi->om.salienceFactor+=500;
    else if (o->objectClass==META_GROUP_OBJECT_CLASS) lsi->om.salienceFactor+=500;
    if (o->plural) lsi->om.salienceFactor-=300;
    if (lsi->lastWhere>=0 && (m[lsi->lastWhere].objectRole&MPLURAL_ROLE)) lsi->om.salienceFactor-=300;
    if (lsi->previousWhere>=0 && (m[lsi->previousWhere].objectRole&MPLURAL_ROLE)) lsi->om.salienceFactor-=300;
    // unquoted only (speaker evaluation)
    lsi->om.salienceFactor+=(3-((lsi->getUnquotedAge()>8) ? 8 : lsi->getUnquotedAge()))*100; // -500 to +300
    if (lsi->getUnquotedPreviousAge()>=0)
      lsi->om.salienceFactor+=(3-((lsi->getUnquotedPreviousAge()>3) ? 3 : lsi->getUnquotedPreviousAge()))*100; // 0 to +300
    if (I!=endQuote && getMostLikelyAudience) 
		{
      lsi->om.salienceFactor+=10000;
			lsi->res+=L"+IN_QUOTE[+10000]";
			speakersConsidered=-1;
		}
		if (lsi->om.salienceFactor>=500 && speakersConsidered>=0)
		{
			if ((previousPrimaryQuote>=0 && lsi->lastWhere>m[previousPrimaryQuote].endQuote) && 
				  (m[previousPrimaryQuote].nextQuote!=beginQuote || (m[previousPrimaryQuote].flags&cWordMatch::flagLastContinuousQuote)))
				speakersMentionedInLastParagraph++;
			speakersConsidered++;
		}
  }
  if (objectsRejected && speakersConsidered>0)
		speakersConsidered++;
	boostRecentSpeakers(beginQuote,beginQuote,speakersConsidered,speakersMentionedInLastParagraph,getMostLikelyAudience);
  sort(localObjects.begin(),localObjects.end(),salienceCompare);
  if (debugTrace.traceSpeakerResolution)
  {
    if (previousSpeakers.size())
      lplog(LOG_RESOLUTION,L"%06d:Most Likely previous speakers are: %s",beginQuote,objectString(previousSpeakers,tmpstr).c_str());
    else
      lplog(LOG_RESOLUTION,L"%06d:No previous speakers.",beginQuote);
    if (objectsRejected)
			lplog(LOG_RESOLUTION,L"%06d:Rejecting %s %d:%s [%s]",beginQuote,(getMostLikelyAudience) ? L"audience" : L"lastDefiniteSpeaker",rejectObjectPosition,objectString(m[rejectObjectPosition].getObject(),tmpstr,true).c_str(),objectString(m[rejectObjectPosition].objectMatches,tmpstr,true).c_str());
    printLocalFocusedObjects(beginQuote,GENDERED_GENERAL_OBJECT_CLASS);
  }
  if ((localObjects.size() && localObjects[0].om.salienceFactor>=0) || lastDefiniteSpeaker!=-1)
  {
		vector <int> saveBeforePreviousSpeakers=beforePreviousSpeakers;
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:Changing beforePreviousSpeakers from %s to %s",beginQuote,
			      objectString(beforePreviousSpeakers,tmpstr).c_str(),objectString(previousSpeakers,tmpstr2).c_str());
		// preferredSpeaker is normally the speaker who spoke two quotes ago (beforePreviousSpeakers).  However, this may be overridden by a defined audience of the last quote.
		int preferredSpeaker=(beforePreviousSpeakers.size()==1) ? beforePreviousSpeakers[0] : -1;
		if (previousPrimaryQuote>=0 && !(m[previousPrimaryQuote].flags&cWordMatch::flagLastContinuousQuote) && 
			  (m[previousPrimaryQuote].flags&(cWordMatch::flagSpecifiedResolveAudience|cWordMatch::flagHailedResolveAudience)) && 
			  m[previousPrimaryQuote].audienceObjectMatches.size()==1 && m[previousPrimaryQuote].audienceObjectMatches[0].object!=preferredSpeaker)
			preferredSpeaker=m[previousPrimaryQuote].audienceObjectMatches[0].object;
		if (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.find(preferredSpeaker)==speakerGroups[currentSpeakerGroup].speakers.end())
			preferredSpeaker=-1;
    beforePreviousSpeakers=previousSpeakers;
		for (unsigned int I=0; I<beforePreviousSpeakers.size(); I++)
			if (objects[beforePreviousSpeakers[I]].getOwnerWhere()>=0 && m[objects[beforePreviousSpeakers[I]].getOwnerWhere()].getObject()>=0 &&
				  find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),m[objects[beforePreviousSpeakers[I]].getOwnerWhere()].getObject())==beforePreviousSpeakers.end())
				beforePreviousSpeakers.push_back(m[objects[beforePreviousSpeakers[I]].getOwnerWhere()].getObject());
    previousSpeakers.clear();
    atLeastOne=false;
    if (localObjects.size() && localObjects[0].om.salienceFactor>=0)
    {
      // pick top 2 because it is unlikely that more than two people are in a conversation
      int highestSalience=localObjects[0].om.salienceFactor;
      previousSpeakersUncertain=previousSpeakersUncertain && beforePreviousSpeakers.size()!=1;
      for (lsi=localObjects.begin(); lsi!=lsiEnd && lsi->om.salienceFactor==highestSalience; lsi++)
      {
				if (lsi->occurredOutsidePrimaryQuote && lsi->physicallyPresent && (previousSpeakersUncertain ||
          find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),lsi->om.object)==beforePreviousSpeakers.end()))
        {
          previousSpeakers.push_back(lsi->om.object);
          if (objects[lsi->om.object].firstLocation<(int)beginQuote) atLeastOne=true; // don't count objects from future - could become replaced with objects that are already rejected
        }
      }
      if (lsi!=lsiEnd && lsi->om.salienceFactor>=0 && previousSpeakers.size()<2 && 
				  (!objectsRejected || !atLeastOne || highestSalience/2<lsi->om.salienceFactor || previousSpeakers.empty()))
      {
        highestSalience=lsi->om.salienceFactor;
        for (; lsi!=lsiEnd && lsi->om.salienceFactor==highestSalience; lsi++)
          if (lsi->occurredOutsidePrimaryQuote && (previousSpeakersUncertain ||
            find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),lsi->om.object)==beforePreviousSpeakers.end()))
            previousSpeakers.push_back(lsi->om.object);
      }
			if ((lsi=in(preferredSpeaker))!=lsiEnd && lsi->om.salienceFactor>=0)
        previousSpeakers.push_back(lsi->om.object);
    }
		if (wherePreviousLastSubjects>=0 && previousSpeakers.size()>1)
		{
			for (unsigned int pls=0; pls<previousLastSubjects.size(); pls++)
			{
				if (objects[previousLastSubjects[pls]].numDefinitelyIdentifiedAsSpeaker && 
					  find(previousSpeakers.begin(),previousSpeakers.end(),previousLastSubjects[pls])==previousSpeakers.end() &&
						(m[beginQuote].audiencePosition<0 || !in(previousLastSubjects[pls],m[beginQuote].audiencePosition)))
				{
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Add previous subject %s (at %d) to previous speakers (%s)",beginQuote,objectString(previousLastSubjects[pls],tmpstr,false).c_str(),wherePreviousLastSubjects,objectString(previousSpeakers,tmpstr2).c_str());
					previousSpeakers.push_back(previousLastSubjects[pls]);
				}
			}
		}
		if (preferredSpeaker>=0 && find(previousSpeakers.begin(),previousSpeakers.end(),preferredSpeaker)!=previousSpeakers.end())
		{
			previousSpeakers.clear();
			previousSpeakers.push_back(preferredSpeaker);
		}
		if (previousSpeakers.size()==1 && saveBeforePreviousSpeakers.size()==1 && saveBeforePreviousSpeakers[0]==previousSpeakers[0] && 
			  (!previousSpeakersUncertain || setFlagAlternateResolveForwardAfterDefinite))
		{
			setFlagAlternateResolveForwardAfterDefinite=true;
	    if (debugTrace.traceSpeakerResolution)
		    lplog(LOG_RESOLUTION,L"%06d:Set speakers to flagAlternateResolveForwardAfterDefinite",beginQuote);
		}
		else
			setFlagAlternateResolveForwardAfterDefinite=false;
		if (previousSpeakers.size()>1)
		{
			int speakerNotPP=0,save=-1; // cannot use lsi->physicallyPresent
			for (int ps = 0; ps < (signed)previousSpeakers.size(); ps++)
			{
				lsi = in(previousSpeakers[ps]);
				if (lsi!=localObjects.end() && lsi->lastWhere < 0 && lsi->numEncounters == 0)
				{
					speakerNotPP++;
					save = ps;
				}
			}
			if (speakerNotPP==1)
			{
		    if (debugTrace.traceSpeakerResolution)
				  lplog(LOG_RESOLUTION,L"%06d:Erased non present previous speaker %s because a present speaker exists",beginQuote,objectString(previousSpeakers[save],tmpstr,true).c_str());
				previousSpeakers.erase(previousSpeakers.begin()+save);
			}
		}
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:Set previous speakers to: %s",beginQuote,objectString(previousSpeakers,tmpstr).c_str());
  }
  else if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:All speakers rejected.  Set previous speakers unchanged: %s",beginQuote,objectString(previousSpeakers,tmpstr).c_str());
  // if inside the quote a GENDERED object is hailed, and the intersection of gendered object matches and previousSpeakers
  // (which is actually the current speaker for the quote being processed)
  // is NOT NULL, reResolveObject.
	// this object is used in the further resolution of the quote.  Therefore it cannot be a generic object like pinr, because
	// that would attempt to use the currentSpeaker information which this routine is trying to gain more information to set more accurately
	// by resaving saveBlockedObject.
  bool allIn,oneIn;
	int o;
  for (unsigned int I=beginQuote; I<endQuote; I++)
    if ((o=m[I].getObject())>=0 && (m[I].objectRole&HAIL_ROLE) &&
				((objects[o].objectClass==NAME_OBJECT_CLASS && objects[o].name.justHonorific() && m[I].queryForm(L"pinr")<0) || 
         objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
         objects[o].objectClass==BODY_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
         objects[o].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS) &&
         intersect(m[I].objectMatches,previousSpeakers,allIn,oneIn))
    {
       m[I].flags&=~cWordMatch::flagObjectResolved;
       resolveObject(I,true,true,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,true,false);
       if (m[I].objectMatches.size()==1 && saveBlockedObject<0)
       {
         saveBlockedObject=m[I].objectMatches[0].object;
         if (debugTrace.traceSpeakerResolution)
           lplog(LOG_RESOLUTION,L"%06d:Reresolve of gender HAIL object %s resulted in previousSpeaker",beginQuote,objectString(saveBlockedObject,tmpstr,false).c_str());
       }
    }
  // saveBlockedObject is from a NAME hailed from within the quote
  if (saveBlockedObject>=0)
  {
    if (find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),saveBlockedObject)==beforePreviousSpeakers.end())
    {
      beforePreviousSpeakers.push_back(saveBlockedObject); // this prevents NotMatches from being null
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_RESOLUTION,L"%06d:Most Likely added %s to before previous speakers",beginQuote,objectString(saveBlockedObject,tmpstr,false).c_str());
    }
  }
}

// scan in quote for a speaker which is referenced at the beginning:
// this is an example where objects in a quote are referenced directly outside of it.
// "Tommy, old bean!"
// "Emma, pick that up."
// only if there are no local speakers (at the beginning of a section, for example)
// OR if the "name" is a gendered singular noun
// skip secondary quotes (could also use secondary quote info later)
int cSource::scanForSpeakers(int begin,int end,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,unsigned __int64 roleFlag)
{ LFS
  unsigned int s;
  for (s=0; s<localObjects.size() && !localObjects[s].numIdentifiedAsSpeaker; s++);
  if (s==localObjects.size()) return -1;
  int inQuoteSpeakerFound=-1,o;
	tIWMM secondaryQuoteOpenWord=Words.gquery(L"‘"),secondaryQuoteCloseWord=Words.gquery(L"’");
  for (int I=begin+1; I<end; I++)
  {
		if (m[I].word==secondaryQuoteOpenWord) 
		{
			while (m[I].word!=secondaryQuoteCloseWord && I<end) I++;
			continue;
		}
    if ((m[I].objectRole&roleFlag) && m[I].getObject()>=0)
    {
			wstring tmpstr,tmpstr2;
			vector <cLocalFocus>::iterator lsi;
      resolveObject(I,true,true,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
      o=m[I].getObject();
      if ((!objects[o].male && !objects[o].female && objects[o].neuter) || objects[o].plural)
			{
				m[I].objectRole&=~roleFlag;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Removed %s flag. Object %s was the wrong type (1).",I,(roleFlag==HAIL_ROLE) ? L"HAIL":L"SELF_REFERRING",objectString(o,tmpstr2,true).c_str());
				if ((lsi=in(o))!=localObjects.end()) 
				{
				  cLocalFocus::setSalienceAgeMethod(true,false,objectToBeMatchedInQuote,quoteIndependentAge);
					lsi->occurredInPrimaryQuote=true;
					lsi->resetAge(objectToBeMatchedInQuote);
					// gosh gee
					if (m[I].queryForm(interjectionForm)>=0)
						lsi->increaseAge(10);
				}
        continue;
			}
			bool allIn,oneIn;
			if (currentSpeakerGroup<speakerGroups.size() && intersect(speakerGroups[currentSpeakerGroup].speakers,m[I].objectMatches,allIn,oneIn) && allIn)
				continue;
			if (currentSpeakerGroup<speakerGroups.size() && intersect(I,speakerGroups[currentSpeakerGroup].speakers,allIn,oneIn))
			{
				inQuoteSpeakerFound=I;
				continue;
			}
			if (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.find(o)==speakerGroups[currentSpeakerGroup].speakers.end() &&
				  (objects[o].objectClass!=NAME_OBJECT_CLASS || !objects[o].name.justHonorific() || objects[o].name.hon->second.query(L"pinr")<0) &&
					(objects[o].getOwnerWhere()<0 || !(m[objects[o].getOwnerWhere()].word->second.inflectionFlags&FIRST_PERSON))) // my friend
			{
				bool aliasFound=false;
				for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(); si!=speakerGroups[currentSpeakerGroup].speakers.end(); si++)
					if (aliasFound=find(objects[*si].aliases.begin(),objects[*si].aliases.end(),o)!=objects[*si].aliases.end())
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION,L"%06d:Imposed object %s was an alias for the speaker object %s.",I,(roleFlag==HAIL_ROLE) ? L"HAIL":L"SELF_REFERRING",objectString(*si,tmpstr2,true).c_str());
						break;
					}
				if (!aliasFound) 
				{
					m[I].objectRole&=~roleFlag;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Removed %s flag. Object %s was the wrong type (2).",I,(roleFlag==HAIL_ROLE) ? L"HAIL":L"SELF_REFERRING",objectString(o,tmpstr2,true).c_str());
					if ((lsi=in(o))!=localObjects.end()) 
					{
					  cLocalFocus::setSalienceAgeMethod(true,false,objectToBeMatchedInQuote,quoteIndependentAge);
						lsi->occurredInPrimaryQuote=true;
						lsi->resetAge(objectToBeMatchedInQuote);
						if (lsi->whereBecamePhysicallyPresent==I)
						{
							lsi->physicallyPresent=false;
							lsi->whereBecamePhysicallyPresent=-1;
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION,L"%06d:%s cancelled physically present",I,objectString(lsi->om,tmpstr,true).c_str());
						}
						// gosh gee
						if (m[I].queryForm(interjectionForm)>=0)
							lsi->increaseAge(10);
					}
					continue;
				}
			}
      if (debugTrace.traceSpeakerResolution)
      {
        wstring roleStr;
				lplog(LOG_RESOLUTION,L"%06d:Found %s speaker %s.",I,(roleFlag&HAIL_ROLE) ? L"hailed" : L"meta",objectString(o,tmpstr,false).c_str());
      }
      inQuoteSpeakerFound=I;
    }
  }
  return inQuoteSpeakerFound;
}

bool cSource::genderedBodyPartSubstitution(int where,int &speakerObject)
{ LFS
  int at=objects[speakerObject].originalLocation;
  // if 'at' field of speakerObject is a body part, and owner gendered, substitute owner for speaker. (A man's voice)
  // the last adjectival object in the list before the actual speakerObject must be picked (Her father's voice - is not her's but her father's)
  int lastGenderedObjectAt=-1;
  for (int genderedObjectAt=objects[speakerObject].begin; genderedObjectAt<at; genderedObjectAt++)
    if (m[genderedObjectAt].getObject()>=0 && (objects[m[genderedObjectAt].getObject()].male || objects[m[genderedObjectAt].getObject()].female))
      lastGenderedObjectAt=genderedObjectAt;
  // objects with -2 are possessive determiners (his, her, its)
    else if (m[genderedObjectAt].getObject()< cObject::eOBJECTS::UNKNOWN_OBJECT &&
      ((m[genderedObjectAt].word->second.inflectionFlags&FEMALE_GENDER)==FEMALE_GENDER ||
      (m[genderedObjectAt].word->second.inflectionFlags&  MALE_GENDER)==  MALE_GENDER) &&
      m[genderedObjectAt].objectMatches.size())
      lastGenderedObjectAt=genderedObjectAt;
  if (lastGenderedObjectAt>=0)
  {
    wstring tmpstr,tmpstr2;
    int substituteObject=m[lastGenderedObjectAt].getObject();
    if (substituteObject<0 || objects[substituteObject].objectClass!=NAME_OBJECT_CLASS)
    {
      // search for names or gendered objects, preferring names
      vector <cOM>::iterator omi,omEnd=m[lastGenderedObjectAt].objectMatches.end();
      for (omi=m[lastGenderedObjectAt].objectMatches.begin(); omi!=omEnd; omi++)
        if (objects[omi->object].objectClass==NAME_OBJECT_CLASS)
        {
          substituteObject=omi->object;
          break;
        }
        else if ((objects[omi->object].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
                  objects[omi->object].objectClass==BODY_OBJECT_CLASS ||
                  objects[omi->object].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
                  objects[omi->object].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
                  objects[omi->object].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
                  objects[omi->object].objectClass==META_GROUP_OBJECT_CLASS) &&
                 substituteObject<0)
          substituteObject=omi->object;
    }
    if (substituteObject>=0 && (objects[substituteObject].objectClass==NAME_OBJECT_CLASS ||
      objects[substituteObject].objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
      objects[substituteObject].objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
      objects[substituteObject].objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
      objects[substituteObject].objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
      objects[substituteObject].objectClass==META_GROUP_OBJECT_CLASS))
    {
      if (debugTrace.traceSpeakerResolution)
        lplog(LOG_RESOLUTION,L"Substituted speaker %s for body part %s.",objectString(substituteObject,tmpstr,false).c_str(),objectString(speakerObject,tmpstr,false).c_str());
      replaceObject(substituteObject,speakerObject,L"genderedBodyPartSubstitution");
      if (section<sections.size())
        replaceObject(where,substituteObject,speakerObject,sections[section].preIdentifiedSpeakerObjects,L"SECTION",sections[section].begin,sections[section].endHeader,L"genderedBodyPartSubstitution");
      //if (currentSpeakerGroup<speakerGroups.size())
      //  replaceObject(substituteObject,speakerObject,speakerGroups[currentSpeakerGroup].speakers,"SPEAKERGROUP",currentSpeakerGroup,"genderedBodyPartSubstitution");
      if (objects[substituteObject].objectClass==NAME_OBJECT_CLASS)
        m[at].flags|=cWordMatch::flagObjectResolved;
      speakerObject=substituteObject;
      return true;
    }
  }
  return false;
}

bool cSource::replaceObjectInSection(int where,int replacementObject,int objectToBeReplaced,wchar_t *fromWhere)
{ LFS
	if (replacementObject==objectToBeReplaced) return false;
  if (section>=sections.size() || 
		  objects[objectToBeReplaced].objectClass==PRONOUN_OBJECT_CLASS ||
		  objects[objectToBeReplaced].objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS ||
			objects[objectToBeReplaced].objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS ||
			(objects[objectToBeReplaced].objectClass==NAME_OBJECT_CLASS && 
			 objects[objectToBeReplaced].name.justHonorific() && 
			 objects[objectToBeReplaced].name.hon->second.query(L"pinr")>=0)) 
	{
		if (objects[objectToBeReplaced].objectClass==NAME_OBJECT_CLASS)
			moveNyms(where,replacementObject,objectToBeReplaced,fromWhere);
		return false;
	}
	if (repeatReplaceObjectInSectionPosition==where && repeatReplaceObjectInSectionFromWhere!=fromWhere)
		return false;
	repeatReplaceObjectInSectionPosition=where;
	repeatReplaceObjectInSectionFromWhere=fromWhere;
  // also look for all locations and speakerLocations in this
  if (debugTrace.traceSpeakerResolution)
  {
    wstring tmpstr,tmpstr2;
    lplog(LOG_RESOLUTION,L"%06d:Substituted object %s for object %s [BEGIN] (%s)",
      where,objectString(replacementObject,tmpstr,false).c_str(),objectString(objectToBeReplaced,tmpstr2,false).c_str(),fromWhere);
  }
  int beginLimit=(int)sections[section].begin,untilLimit=where;
  if (currentSpeakerGroup<speakerGroups.size())
  {
    beginLimit=speakerGroups[currentSpeakerGroup].sgBegin;
    untilLimit=speakerGroups[currentSpeakerGroup].sgEnd;
  }
  // also substitute in section and speakerGroups up till the end of the section
	bool replacedObject=false;
  replacedObject|=replaceObject(replacementObject,objectToBeReplaced,sections[section].definiteSpeakerObjects,fromWhere);
  replacedObject|=replaceObject(replacementObject,objectToBeReplaced,sections[section].speakerObjects,fromWhere);
  replacedObject|=replaceObject(replacementObject,objectToBeReplaced,sections[section].objectsSpokenAbout,fromWhere);
  replacedObject|=replaceObject(replacementObject,objectToBeReplaced,sections[section].objectsInNarration,fromWhere);
  if (section<sections.size())
    replacedObject|=replaceObject(where,replacementObject,objectToBeReplaced,sections[section].preIdentifiedSpeakerObjects,L"SECTION",sections[section].begin,sections[section].endHeader,fromWhere);
  if (currentSpeakerGroup<speakerGroups.size())
	{
		// replace object in previous speaker groups
		int sg=currentSpeakerGroup;
		while (sg>=0 && speakerGroups[sg].sgBegin>=(int)sections[section].begin && 
			     (replaceObjectInSpeakerGroup(where,replacementObject,objectToBeReplaced,sg,L"SPEAKERGROUP(2)") ||
					  speakerGroups[sg].speakers.find(replacementObject)!=speakerGroups[sg].speakers.end() ||
						speakerGroups[sg].speakers.size()==1))  // skip over speakerGroups that have only narrative where the speakerGroup has been pared to nothing
		  sg--;
		if (sg>=0 && speakerGroups[sg].fromNextSpeakerGroup.find(objectToBeReplaced)!=speakerGroups[sg].fromNextSpeakerGroup.end())
		{
			replaceObjectInSpeakerGroup(where,replacementObject,objectToBeReplaced,sg,L"SPEAKERGROUP(3)");
			sg--;
		}
		unsigned int tillsg=min(currentSpeakerGroup+1,speakerGroups.size()),lastSG=tillsg;
		int endSection=(section+1<sections.size()) ? sections[section+1].begin : m.size();
		for ( ; tillsg<speakerGroups.size() && speakerGroups[tillsg].sgEnd<=endSection; tillsg++)
			if (replaceObjectInSpeakerGroup(where,replacementObject,objectToBeReplaced,tillsg,L"SPEAKERGROUP(4)"))
			{
				if (speakerGroups[tillsg].speakers.size()<=1 && speakerGroups[tillsg].conversationalQuotes>1 && tillsg)
				{
					speakerGroups[tillsg].speakers.insert(speakerGroups[tillsg-1].speakers.begin(),speakerGroups[tillsg-1].speakers.end());
					if (debugTrace.traceSpeakerResolution)
					{
						wstring tmpstr,tmpstr2;
						lplog(LOG_RESOLUTION,L"%06d:Speakers from previous group %s imported because replacement in conversational speakerGroup reduced speakers to 1 resulting in %s (%s)",
									where,toText(speakerGroups[tillsg-1],tmpstr),toText(speakerGroups[tillsg],tmpstr2),fromWhere);
					}
				}
				lastSG=tillsg;
			}
		tillsg=lastSG;
		if (sg<(int)currentSpeakerGroup) sg++;
    beginLimit=speakerGroups[sg].sgBegin;
		untilLimit=(tillsg<speakerGroups.size()) ? speakerGroups[tillsg].sgBegin : m.size();
		if (objects[replacementObject].lastSpeakerGroup<(int)tillsg-1) objects[replacementObject].lastSpeakerGroup=tillsg-1;
		for (; sg<=(int)tillsg && sg<(int)speakerGroups.size(); sg++)
		{
			speakerGroups[sg].previousSubsetSpeakerGroup=-1;
			speakerGroups[sg].groupedSpeakers.clear();
			speakerGroups[sg].singularSpeakers.clear();
			subtract(speakerGroups[sg].speakers,speakerGroups[sg].groupedSpeakers,speakerGroups[sg].singularSpeakers); 
			determinePreviousSubgroup(where,sg,&speakerGroups[sg]);
			determineSubgroupFromGroups(speakerGroups[sg]);
			if (speakerGroups[sg].saveNonNameObject==objectToBeReplaced)
				speakerGroups[sg].saveNonNameObject=replacementObject;
			if (speakerGroups[sg].observers.size() && speakerGroups[sg].speakers.size()<=2 && speakerGroups[sg].conversationalQuotes)
			{
				speakerGroups[sg].povSpeakers=speakerGroups[sg].observers;
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr;
					lplog(LOG_RESOLUTION,L"%06d:Eliminated observers in sg %s (replaceObjectInSection %s)",where,toText(speakerGroups[sg],tmpstr),fromWhere);
				}
				int sgi=sg+1;
				if (sgi<(signed)speakerGroups.size() && speakerGroups[sgi].observers==speakerGroups[sg].observers)
				{
					int mo=*speakerGroups[sg].observers.begin();
					// analyze next speaker group to see whether the loss of observer status affects the next one:
					// speaker part of previous speakerGroup and conversationalQuotes
					bool isPreviousSpeaker=sgi && speakerGroups[sgi-1].speakers.find(mo)!=speakerGroups[sgi-1].speakers.end() && speakerGroups[sgi-1].conversationalQuotes;
					bool allIn,oneIn,currentSpeakerGroupSuperGroupToPrevious=sgi && intersect(speakerGroups[sgi-1].speakers,speakerGroups[sgi].speakers,allIn,oneIn) && allIn;
					// if there is more than one conversational quote, and there is only one definitively named speaker, and there are more than two speakers, and
					//   the speaker was not an observer in a pervious group, then speaker is not an observer (because we don't know whether the other speaker is actually an observer)
					bool uncertainObserver=speakerGroups[sgi].conversationalQuotes && speakerGroups[sgi].dnSpeakers.size()<=1 && speakerGroups[sgi].speakers.size()>2;
					// if the speaker was not an observer in previous group and dnSpeakers is empty, then speaker is not an observer
					if (speakerGroups[sgi].dnSpeakers.empty() || (isPreviousSpeaker && currentSpeakerGroupSuperGroupToPrevious) || uncertainObserver) 
					{
						speakerGroups[sgi].povSpeakers=speakerGroups[sgi].observers;
						speakerGroups[sgi].observers.clear();
						if (debugTrace.traceSpeakerResolution)
						{
							wstring tmpstr;
							lplog(LOG_RESOLUTION,L"%06d:Eliminated observers (depended on previous sg) in sg %s (replaceObjectInSection %s)",where,toText(speakerGroups[sg],tmpstr),fromWhere);
						}
					}
				}
				speakerGroups[sg].observers.clear();
			}
		}
	}
  vector <cObject::cLocation>::iterator li,liEnd;
	wstring locations;
  for (li=objects[objectToBeReplaced].locations.begin(),liEnd=objects[objectToBeReplaced].locations.end(); li!=liEnd; li++)
    if (li->at>=beginLimit && li->at<=untilLimit)
		{
			replaceObject(replacementObject,objectToBeReplaced,m[li->at].objectMatches,li);
			if (debugTrace.traceSpeakerResolution) itos(L" ",li->at,locations,L"");
		}
  // also look for all locations and speakerLocations in this
  if (debugTrace.traceSpeakerResolution)
  {
    wstring tmpstr,tmpstr2;
    lplog(LOG_RESOLUTION,L"%06d:Substituted object %s for object %s in locations [%d-%d](%s) (%s)",
      where,objectString(replacementObject,tmpstr,false).c_str(),objectString(objectToBeReplaced,tmpstr2,false).c_str(),
        beginLimit,untilLimit,locations.c_str(),fromWhere);
  }
  set <int>::iterator olsl,olslEnd;
  for (olsl=objects[objectToBeReplaced].speakerLocations.begin(),olslEnd=objects[objectToBeReplaced].speakerLocations.end(); olsl!=olslEnd; olsl++)
    if (*olsl>=beginLimit && *olsl<=untilLimit)
    {
      replaceObject(replacementObject,objectToBeReplaced,m[*olsl].objectMatches,*olsl);
      objects[replacementObject].speakerLocations.insert(*olsl);
      replaceObject(replacementObject,objectToBeReplaced,m[*olsl].audienceObjectMatches,*olsl);
    }
	// push a match unto the object itself, unless it is already there
	if (objects[objectToBeReplaced].begin>=beginLimit && in(replacementObject,m[objects[objectToBeReplaced].originalLocation].objectMatches)==m[objects[objectToBeReplaced].originalLocation].objectMatches.end())
	{
		m[objects[objectToBeReplaced].originalLocation].objectMatches.push_back(cOM(replacementObject,SALIENCE_THRESHOLD));
		objects[replacementObject].locations.push_back(cObject::cLocation(objects[objectToBeReplaced].originalLocation));
		objects[replacementObject].updateFirstLocation(objects[objectToBeReplaced].originalLocation);
	}	
	// replace in localObjects
  replaceObject(replacementObject,objectToBeReplaced,fromWhere);
	objects[replacementObject].aliases.insert(objects[replacementObject].aliases.end(),objects[objectToBeReplaced].aliases.begin(),objects[objectToBeReplaced].aliases.end());
	vector <int>::iterator ai=find(objects[replacementObject].aliases.begin(),objects[replacementObject].aliases.end(),replacementObject),aiEnd;
	if (ai!=objects[replacementObject].aliases.end()) objects[replacementObject].aliases.erase(ai);
	for (ai=objects[objectToBeReplaced].aliases.begin(),aiEnd=objects[objectToBeReplaced].aliases.end(); ai!=aiEnd; ai++)
	{
		if (*ai==replacementObject || *ai==objectToBeReplaced) continue;
		objects[*ai].aliases.push_back(replacementObject);
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr,tmpstr2;
			if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:Object %s gained alias %s (3).",where,objectString(*ai,tmpstr,true).c_str(),objectString(replacementObject,tmpstr2,true).c_str());
		}
	}
  if (debugTrace.traceSpeakerResolution && objects[objectToBeReplaced].aliases.size())
  {
    wstring tmpstr,tmpstr2;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:Object %s gained alias %s (4).",where,objectString(replacementObject,tmpstr,true).c_str(),objectString(objects[objectToBeReplaced].aliases,tmpstr2).c_str());
	}
	moveNyms(where,replacementObject,objectToBeReplaced,fromWhere);
	return replacedObject;
}

int cSource::getBodyObject(int o)
{ LFS
  if (objects[o].objectClass==BODY_OBJECT_CLASS && objects[o].getOwnerWhere()>=0)
  {
      if (m[objects[o].getOwnerWhere()].objectMatches.size()==1)
        return m[objects[o].getOwnerWhere()].objectMatches[0].object;
      else if (m[objects[o].getOwnerWhere()].objectMatches.empty())
        return m[objects[o].getOwnerWhere()].getObject();
	}
	return -1;
}

unsigned int cSource::numMatchingGenderInSpeakerGroup(int o)
{ LFS
	unsigned int nmg=0;
	for (set <int>::iterator s=speakerGroups[currentSpeakerGroup].speakers.begin(),sEnd=speakerGroups[currentSpeakerGroup].speakers.end(); s!=sEnd; s++)
		if ((objects[*s].male && objects[o].male) || (objects[*s].female && objects[o].female))
			nmg++;
	return nmg;
}

// definitelySpeaker is from scanForSpeaker.  It is set to true unless the speaker was detected after the
// quote in an _S1 pattern with a verb with a nonpast tense, or the verb had one or more objects.
void cSource::imposeSpeaker(int beginQuote,int endQuote,int &lastDefiniteSpeaker,int speakerObjectAt,bool definitelySpeaker,
                           int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb,bool previousSpeakersUncertain,int audienceObjectPosition,vector <int> &lastUnquotedSubjects, int whereLastUnquotedSubjects)
{ LFS
  wstring tmpstr,tmpstr2,tmpstr3;
  if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%d-%d:Imposing speaker at %d definitelySpeaker=%s last=%d lastBeginS1=%d previousSpeakersUncertain=%s audienceObjectPosition=%d lastUnquotedSubjects=%s@%d",
          beginQuote,endQuote,speakerObjectAt,(definitelySpeaker) ? L"true":L"false",lastDefiniteSpeaker,lastBeginS1,(previousSpeakersUncertain) ? L"true":L"false",
          audienceObjectPosition,objectString(lastUnquotedSubjects,tmpstr).c_str(),whereLastUnquotedSubjects);
	// if subjects in previous unquoted section usable for immediate resolution, and last unquoted subjects position exists and is not plural,
	//   set last subject preference=true for all local objects outside primary quotes which match the last subjects.
	// last subject preference used in chooseBest
	if (subjectsInPreviousUnquotedSectionUsableForImmediateResolution && (whereLastUnquotedSubjects<0 || m[whereLastUnquotedSubjects].getObject()<0 || !objects[m[whereLastUnquotedSubjects].getObject()].plural))
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
	    if (lsi->occurredOutsidePrimaryQuote)
				lsi->lastSubject=(find(lastUnquotedSubjects.begin(),lastUnquotedSubjects.end(),lsi->om.object)!=lastUnquotedSubjects.end());
  // resolve object at speakerObjectAt
  resolveObject(speakerObjectAt,definitelySpeaker,false,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
	// if all beforePreviousSpeakers are in the speaker object, then delete all of the beforePreviousSpeakers from the speakerObject position.
  bool allIn,oneIn;
	if (m[speakerObjectAt].objectMatches.size()>1 && beforePreviousSpeakers.size()<m[speakerObjectAt].objectMatches.size() &&
		  intersect(beforePreviousSpeakers,m[speakerObjectAt].objectMatches,allIn,oneIn) && allIn)
		unMatchObjects(speakerObjectAt,beforePreviousSpeakers,true);
  // also resolve the principal object the speakerObjectAt is related to, if it is a part of an object
  if (m[speakerObjectAt].principalWherePosition>=0)
  {
		speakerObjectAt=m[speakerObjectAt].principalWherePosition;
		if (m[speakerObjectAt].getObject()>=0 && objects[m[speakerObjectAt].getObject()].getOwnerWhere()>=0)
		  resolveObject(objects[m[speakerObjectAt].getObject()].getOwnerWhere(),definitelySpeaker,false,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
    resolveObject(speakerObjectAt,definitelySpeaker,false,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
  }
	// remove observers
  if (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].observers.size())
	{
		for (int I=0; I<(signed)m[speakerObjectAt].objectMatches.size(); )
			if (find(speakerGroups[currentSpeakerGroup].observers.begin(),speakerGroups[currentSpeakerGroup].observers.end(),m[speakerObjectAt].objectMatches[I].object)!=speakerGroups[currentSpeakerGroup].observers.end())
				m[speakerObjectAt].objectMatches.erase(m[speakerObjectAt].objectMatches.begin()+I);
			else
				I++;
	}
	// reset lastSubject preference
  for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
    lsi->lastSubject=false;
  int speakerObject=m[speakerObjectAt].getObject(); // just in case object is replaced during resolveObject
	// are any speakers in the speakerGroup using this as an alias?  If so, replace with actual speaker.
	if (currentSpeakerGroup<speakerGroups.size())
    for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(); si!=speakerGroups[currentSpeakerGroup].speakers.end(); si++)
			if (find(objects[*si].aliases.begin(),objects[*si].aliases.end(),speakerObject)!=objects[*si].aliases.end() && objects[*si].firstLocation>=0)
			{
		    if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Imposed object %s was an alias for the speaker object %s.",beginQuote,objectString(speakerObject,tmpstr,true).c_str(),objectString(*si,tmpstr2,true).c_str());
				speakerObject=*si;
				speakerObjectAt=objects[speakerObject].firstLocation;
				break;
			}
			//else if (t.traceSpeakerResolution) // unnecessary
			//	lplog(LOG_RESOLUTION,L"%06d:Imposed object %s not found in aliases %s for the speaker object %s.",beginQuote,
			//	  objectString(speakerObject,tmpstr,true).c_str(),objectString(objects[*si].aliases,tmpstr2).c_str(),objectString(*si,tmpstr3,true).c_str());
	// resolve audience
  int audienceObject=-1;
  if (audienceObjectPosition>=0)
  {
    resolveObject(audienceObjectPosition,definitelySpeaker,false,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
    if (m[audienceObjectPosition].principalWherePosition>=0)
    {
			audienceObjectPosition=m[audienceObjectPosition].principalWherePosition;
      resolveObject(audienceObjectPosition,definitelySpeaker,false,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,true,false,false);
    }
		if (m[audienceObjectPosition].objectMatches.size()==1)
			audienceObject=m[audienceObjectPosition].objectMatches[0].object;
		else
			audienceObject=m[audienceObjectPosition].getObject(); // just in case object is replaced during resolveObject
  }
	// set speaker and audience.  If the speaker is definitively specified, but the speaker object is a gendered pronoun, for which there is more than one matching gender, then flag as ambiguous.
  if (beginQuote>=0)
  {
		__int64 flag=cWordMatch::flagMostLikelyResolveSpeakers;
		if (definitelySpeaker)
		{
			int o=m[speakerObjectAt].getObject(),begin=m[speakerObjectAt].beginObjectPosition;
			flag=cWordMatch::flagDefiniteResolveSpeakers;
			// if speaker is definite (that is if the quote is specifically attributed - he said, she said etc.
			// but the speaker is a pronoun that is ambiguous in that there is more than one speaker associated with that gender
			// if speakers are ambiguous and there is no hint from the previous subject
			// but this subject must be perfectly generic (no adjectives for a gendered object to indicate preference)
			if (o>=0 && (objects[o].objectClass==PRONOUN_OBJECT_CLASS || 
				  (objects[o].objectClass==GENDERED_GENERAL_OBJECT_CLASS && begin>=0 && m[begin].word->first==L"the" && (m[speakerObjectAt].endObjectPosition-begin)==2)) &&
				  (objects[o].male ^ objects[o].female) &&
					//speakerGroups[currentSpeakerGroup].speakers.size()>2 &&
					(!subjectsInPreviousUnquotedSectionUsableForImmediateResolution || subjectsInPreviousUnquotedSection.size()!=1 ||
				   m[speakerObjectAt].objectMatches.size()!=1 || m[speakerObjectAt].objectMatches[0].object!=subjectsInPreviousUnquotedSection[0]) &&
					 numMatchingGenderInSpeakerGroup(o)>1)
				flag=cWordMatch::flagGenderIsAmbiguousResolveSpeakers|cWordMatch::flagMostLikelyResolveSpeakers;
		}
		setSpeakerMatchesFromPosition(beginQuote,m[beginQuote].objectMatches,speakerObjectAt,L"speaker",flag);
    // set NOT speaker to matches of audienceObjectPosition (if set)
    if (audienceObjectPosition>=0)
		{
			flag=cWordMatch::flagSpecifiedResolveAudience;
			int o=m[audienceObjectPosition].getObject();
			// if audience is a hail, and there is more than one match and the number of matching gendered objects in the speaker group > the number of audience objects, mark as ambiguous
			if ((m[audienceObjectPosition].objectRole&HAIL_ROLE) && m[audienceObjectPosition].objectMatches.size()>1 && (objects[o].male ^ objects[o].female) &&
				  numMatchingGenderInSpeakerGroup(o)>m[audienceObjectPosition].objectMatches.size())
				flag=cWordMatch::flagGenderIsAmbiguousResolveAudience|cWordMatch::flagMostLikelyResolveAudience;
      setSpeakerMatchesFromPosition(beginQuote,m[beginQuote].audienceObjectMatches,audienceObjectPosition,L"audiencePosition (2)",flag);
		}
  }
	// if speaker/audience is definite and not a pronoun, set the speaker and audience as definite in counters, definitive speaker groups, section speakers, local focus
  if (definitelySpeaker)
  {
    if (speakerObject<0)
    {
      lplog(LOG_RESOLUTION,L"%06d:Definite speaker is not an object!",speakerObjectAt);
      return;
    }
    if (objects[speakerObject].objectClass!=REFLEXIVE_PRONOUN_OBJECT_CLASS &&
        objects[speakerObject].objectClass!=PRONOUN_OBJECT_CLASS &&
        objects[speakerObject].objectClass!=RECIPROCAL_PRONOUN_OBJECT_CLASS)
    {
      // if object is not a name and has other matches, prefer the name match
      if (objects[speakerObject].objectClass!=NAME_OBJECT_CLASS && m[speakerObjectAt].objectMatches.size())
      {
        int replacementObject=-1,I;
        for (I=0; I<(signed)m[speakerObjectAt].objectMatches.size(); I++)
          if (objects[m[speakerObjectAt].objectMatches[I].object].objectClass==NAME_OBJECT_CLASS)
          {
            if (replacementObject!=-1) break;
            replacementObject=m[speakerObjectAt].objectMatches[I].object;
          }
        // more than one potential replacement object - select a preIdentifiedSpeakerObject
        if (replacementObject>=0 && I!=m[speakerObjectAt].objectMatches.size() && currentSpeakerGroup<speakerGroups.size())
        {
          // if at least one of the matches is in predefinedSpeakers, remove
          //   remove from predefinedSpeakers and also from localObjects
          replacementObject=-1;
          for (I=0; I<(signed)m[speakerObjectAt].objectMatches.size(); I++)
            if (speakerGroups[currentSpeakerGroup].speakers.find(m[speakerObjectAt].objectMatches[I].object)!=speakerGroups[currentSpeakerGroup].speakers.end())
						{
	            if (replacementObject!=-1) break;
							replacementObject=m[speakerObjectAt].objectMatches[I].object;
						}
        }
				// even if it is not a NAME_OBJECT_CLASS, replace.
				if (m[speakerObjectAt].objectMatches.size()==1)
				{
					replacementObject=m[speakerObjectAt].objectMatches[0].object;
          I=(signed)m[speakerObjectAt].objectMatches.size();
				}		 
        if (replacementObject>=0 && I==m[speakerObjectAt].objectMatches.size())
        {
					for (int replacementLoop=0; m[objects[replacementObject].originalLocation].objectMatches.size()==1 && replacementLoop<1000; replacementLoop++)
						replacementObject=m[objects[replacementObject].originalLocation].objectMatches[0].object;
          replaceObjectInSection(speakerObjectAt,replacementObject,speakerObject,L"imposeSpeaker");
          speakerObject=replacementObject;
        }
      }
			// set speaker object counters, definite speaker groups, section speaker objects, make sure object in local focus.
      objects[speakerObject].numDefinitelyIdentifiedAsSpeaker++;
			if (speakerObject!=audienceObject)
				definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(speakerObjectAt);
      objects[speakerObject].numDefinitelyIdentifiedAsSpeakerInSection++;
      if (objects[speakerObject].numDefinitelyIdentifiedAsSpeakerInSection+objects[speakerObject].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
        sections[section].speakerObjects.push_back(cOM(speakerObject,SALIENCE_THRESHOLD));
			vector <cLocalFocus>::iterator lsi;
      if (pushObjectIntoLocalFocus(speakerObjectAt, speakerObject, true, false, false, false, L"local speaker (2)", lsi)) 
				lsi->numDefinitelyIdentifiedAsSpeaker++;
    }
    if (audienceObject>=0 &&
        objects[audienceObject].objectClass!=REFLEXIVE_PRONOUN_OBJECT_CLASS &&
        objects[audienceObject].objectClass!=RECIPROCAL_PRONOUN_OBJECT_CLASS &&
        objects[audienceObject].objectClass!=PRONOUN_OBJECT_CLASS)
    {
			if (speakerObject!=audienceObject)
				definitelyIdentifiedAsSpeakerInSpeakerGroups.push_back(audienceObjectPosition);
      objects[audienceObject].numDefinitelyIdentifiedAsSpeaker++;
      objects[audienceObject].numDefinitelyIdentifiedAsSpeakerInSection++;
      if (objects[audienceObject].numDefinitelyIdentifiedAsSpeakerInSection+objects[audienceObject].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
        sections[section].speakerObjects.push_back(cOM(audienceObject,SALIENCE_THRESHOLD));
			vector <cLocalFocus>::iterator lsi;
      if (pushObjectIntoLocalFocus(audienceObjectPosition, audienceObject, true, false, false, false, L"local audience speaker (3)",lsi)) 
				lsi->numDefinitelyIdentifiedAsSpeaker++;
    }
  }
	// (CMREADME39)
  if (beginQuote>=0)
  {
    // resolve previous uncertain matches
    resolvePreviousSpeakersByAlternationBackwards(beginQuote,speakerObjectAt,definitelySpeaker);
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:Changing beforePreviousSpeakers from %s to %s (2)",beginQuote,
			      objectString(beforePreviousSpeakers,tmpstr).c_str(),objectString(previousSpeakers,tmpstr2).c_str());
    beforePreviousSpeakers=previousSpeakers;
    wchar_t *howDerived=L"";
    // set NOT speaker to matches of audienceObjectPosition (if set)
    if (audienceObjectPosition>=0)
		{
			__int64 flag=cWordMatch::flagSpecifiedResolveAudience;
			int o=m[audienceObjectPosition].getObject();
			if ((m[audienceObjectPosition].objectRole&HAIL_ROLE) && m[audienceObjectPosition].objectMatches.size()>1 && (objects[o].male ^ objects[o].female) &&
				  numMatchingGenderInSpeakerGroup(o)>m[audienceObjectPosition].objectMatches.size())
				flag=cWordMatch::flagGenderIsAmbiguousResolveAudience|cWordMatch::flagMostLikelyResolveAudience;
      setSpeakerMatchesFromPosition(beginQuote,m[beginQuote].audienceObjectMatches,audienceObjectPosition,howDerived=L"audiencePosition (2)",flag);
		}
		// if a speaker is definitely identified, that is the lastDefiniteSpeaker for the next statement.
		// but if the lastDefiniteSpeaker is the same speaker as the present speaker, then reject the lastDefiniteSpeaker.
    else if (lastDefiniteSpeaker>=0)
		{
			if (!intersect(m[beginQuote].objectMatches,m[lastDefiniteSpeaker].objectMatches,allIn,oneIn))
			{
				setSpeakerMatchesFromPosition(beginQuote,m[beginQuote].audienceObjectMatches,lastDefiniteSpeaker,howDerived=L"lastDefiniteSpeaker",cWordMatch::flagFromLastDefiniteResolveAudience);
				if (unresolvedSpeakers.size())
					unresolvedSpeakers[unresolvedSpeakers.size()-1]=-beginQuote; // objectsNotMatched may be more definitely resolved later...
			}
		}
    if (*howDerived==0)
    {
      if (unresolvedSpeakers.size())
        unresolvedSpeakers[unresolvedSpeakers.size()-1]=-beginQuote; // objectsNotMatched should be resolved later...
      // this is just in case a writer decides to have (apparently) the same person say something twice in a sqlrow
      // the NotMatches would be blank, but perhaps this is OK...perhaps the person is talking to themselves.
      if (previousSpeakers.size()==1 && m[beginQuote].objectMatches.size()==1 && previousSpeakers[0]==m[beginQuote].objectMatches[0].object)
        previousSpeakers.clear();
      // If a defined speaker comes right after a paragraph without a quote, the previousSpeakers is cleared.
      howDerived=L"previous speakers";
      if (previousSpeakers.empty())
      {
        howDerived=L"no previous speakers";
        vector <int> noPreviousSubjects;
        getMostLikelySpeakers(beginQuote,endQuote,lastDefiniteSpeaker,previousSpeakersUncertain,-1,noPreviousSubjects,beginQuote,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb);
				//listenerAssociationPreference();
        if (unresolvedSpeakers.size()>0)
          unresolvedSpeakers[unresolvedSpeakers.size()-1]=-beginQuote;
				m[beginQuote].flags|=cWordMatch::flagMostLikelyResolveAudience;
			}
			else if (previousSpeakers.size()!=1)  
				m[beginQuote].flags|=cWordMatch::flagMostLikelyResolveAudience;
			else
				m[beginQuote].flags|=cWordMatch::flagAlternateResolveBackwardFromDefiniteAudience; // will block other alternate speakers
			vector <int> bodyObjects;
			int o;
			for (unsigned int I=0; I<m[beginQuote].objectMatches.size(); I++)
				if ((o=getBodyObject(m[beginQuote].objectMatches[I].object))>=0)
					bodyObjects.push_back(o);
			// name intro "Tuppence," asked Bill - making this HAIL is too expansive, resulting in many mistakes
			if (find(previousSpeakers.begin(),previousSpeakers.end(),m[beginQuote+1].getObject())!=previousSpeakers.end() && 
				  m[m[beginQuote+1].endObjectPosition].word->first==L"," && m[m[beginQuote+1].endObjectPosition+1].word->first==L"”")
			{
				if (m[beginQuote+1].objectRole&HAIL_ROLE)
				{
					m[beginQuote+1].objectRole|=HAIL_ROLE;
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:Acquired HAIL role (2).",beginQuote+1);
				}
				previousSpeakers.clear();
				previousSpeakers.push_back(m[beginQuote+1].getObject());
				beforePreviousSpeakers=previousSpeakers;
        howDerived=L"no previous speakers - intro suggested";
			}
      for (unsigned s=0; s<previousSpeakers.size(); s++)
      {
				if (((o=getBodyObject(previousSpeakers[s]))<0 || 
					  in(o,m[beginQuote].objectMatches)==m[beginQuote].objectMatches.end()) &&
						find(bodyObjects.begin(),bodyObjects.end(),previousSpeakers[s])==bodyObjects.end() && 
            in(previousSpeakers[s],m[beginQuote].objectMatches)==m[beginQuote].objectMatches.end() &&
						in(previousSpeakers[s],m[beginQuote].audienceObjectMatches)==m[beginQuote].audienceObjectMatches.end())
        {
          m[beginQuote].audienceObjectMatches.push_back(cOM(previousSpeakers[s],SALIENCE_THRESHOLD));
          objects[previousSpeakers[s]].speakerLocations.insert(beginQuote);
        }
      }
	    if (debugTrace.traceSpeakerResolution)
		    lplog(LOG_RESOLUTION,L"%06d:set audience matches [%s] (1) to %s",beginQuote,howDerived,objectString(m[beginQuote].audienceObjectMatches,tmpstr,true).c_str());
    }
    previousSpeakers.clear();
    for (unsigned int I=0; I<m[beginQuote].objectMatches.size(); I++)
    {
      int o;
      previousSpeakers.push_back(o=m[beginQuote].objectMatches[I].object);
      objects[o].numIdentifiedAsSpeaker++;
			if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[o].getSubType()>=0)
				lplog(LOG_RESOLUTION,L"%06d:Removing place designation (9) from object %s.",beginQuote,objectString(o,tmpstr,false).c_str());
			objects[o].resetSubType(); // not a place
			objects[o].isNotAPlace=true;
      objects[o].numIdentifiedAsSpeakerInSection++;
      if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection+objects[o].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
        sections[section].speakerObjects.push_back(cOM(o,SALIENCE_THRESHOLD));
      vector <cLocalFocus>::iterator lsi=in(o);
      if (lsi!=localObjects.end())
        lsi->numIdentifiedAsSpeaker++;
    }
    if (definitelySpeaker) 
		{
			lastDefiniteSpeaker=beginQuote; 
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:Set last definite speaker to %d.",beginQuote,beginQuote);
		}
		else
			lastDefiniteSpeaker=-1;
    printSpeakerQueue(endQuote);
  }
}

void cSource::addSpeakerObjects(int position,bool toMatched,int where,vector <int> speakers,__int64 resolutionFlag)
{ LFS
	bool pushed=false;
  for (unsigned int s=0; s<speakers.size(); s++)
  {
    if (pushed=toMatched && in(speakers[s],m[position].objectMatches)==m[position].objectMatches.end())
      m[position].objectMatches.push_back(cOM(speakers[s],SALIENCE_THRESHOLD)); 
    else if (pushed=!toMatched && in(speakers[s],m[position].audienceObjectMatches)==m[position].audienceObjectMatches.end())
      m[position].audienceObjectMatches.push_back(cOM(speakers[s],SALIENCE_THRESHOLD));
		// position is quote, so locations push not needed
		if (pushed)
			objects[speakers[s]].speakerLocations.insert(position);
  }
	m[position].flags|=resolutionFlag;
  if (pushed && debugTrace.traceSpeakerResolution)
  {
    wstring tmpstr,tmpstr2,tmpstr3;
    lplog(LOG_RESOLUTION,L"%06d:added %s match of %s [added %s to %s]",where,(toMatched) ? L"speaker":L"audience",objectString(speakers,tmpstr).c_str(),speakerResolutionFlagsString(resolutionFlag,tmpstr2).c_str(),speakerResolutionFlagsString(m[position].flags,tmpstr3).c_str());
  }
}

void cSource::imposeMostLikelySpeakers(unsigned int beginQuote,int &lastDefiniteSpeaker,int audienceObjectPosition)
{ LFS
  bool previousSpeakersEmpty;
  if (previousSpeakersEmpty=previousSpeakers.empty())
    previousSpeakers=beforePreviousSpeakers;
	bool allIn,oneIn;
	int addResolveFlag=0;
	wstring tmpstr,tmpstr2;
	// check if this speaker is set because it is hailed in the immediately preceding quote.  If so, increase the certainty of the match.
	if (!previousSpeakersEmpty && previousPrimaryQuote!=-1 && (m[previousPrimaryQuote].flags&cWordMatch::flagSpecifiedResolveAudience))
	{
		// make sure the previousPrimaryQuote is immediately before beginQuote
		unsigned int endOfParagraph=m[previousPrimaryQuote].endQuote;
		while (endOfParagraph<m.size() && m[endOfParagraph].word!=Words.sectionWord) endOfParagraph++;
		if (endOfParagraph+1<m.size() && endOfParagraph+1==beginQuote && intersect(m[previousPrimaryQuote].audienceObjectMatches,previousSpeakers,allIn,oneIn) && allIn)
		{
			addResolveFlag=cWordMatch::flagFromPreviousHailResolveSpeakers;
			if (m[previousPrimaryQuote].audienceObjectMatches.size()<previousSpeakers.size())
			{
			  if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION,L"%06d:Narrowed speaker to from %s to hailed speaker %s.",beginQuote,
						objectString(previousSpeakers,tmpstr).c_str(),objectString(m[beginQuote].audienceObjectMatches,tmpstr2,true).c_str());
				previousSpeakers.clear();
				for (unsigned int aom=0; aom<m[previousPrimaryQuote].audienceObjectMatches.size(); aom++)
					previousSpeakers.push_back(m[previousPrimaryQuote].audienceObjectMatches[aom].object);
			}
		}
	}
  // set objectMatches at beginQuote to previousSpeakers
	if (setFlagAlternateResolveForwardAfterDefinite) addResolveFlag|=cWordMatch::flagAlternateResolveForwardAfterDefinite;
	addSpeakerObjects(beginQuote,true,beginQuote,previousSpeakers,cWordMatch::flagMostLikelyResolveSpeakers|addResolveFlag);
  for (unsigned s=0; s<previousSpeakers.size(); s++)
  {
    int o=previousSpeakers[s];
    objects[o].numIdentifiedAsSpeaker++;
		if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[o].getSubType()>=0)
			lplog(LOG_RESOLUTION,L"%06d:Removing place designation (10) from object %s.",beginQuote,objectString(o,tmpstr,false).c_str());
		objects[o].resetSubType(); // not a place
		objects[o].isNotAPlace=true;
    objects[o].numIdentifiedAsSpeakerInSection++;
    if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection+objects[o].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
      sections[section].speakerObjects.push_back(cOM(o,SALIENCE_THRESHOLD));
    unsigned int J;
    for (J=0; J<localObjects.size(); J++)
      if (localObjects[J].om.object==o)
        localObjects[J].numIdentifiedAsSpeaker++;
  }
  if (previousSpeakersEmpty)
    previousSpeakers.clear();
	// if object position to be excluded from current speaker (for example, a previous HAIL position) matches everything in the speakerGroup, cancel
	// it, as it will end up rejecting all speakers.
  if (audienceObjectPosition>=0 && (currentSpeakerGroup>=speakerGroups.size() || m[audienceObjectPosition].objectMatches.size()<speakerGroups[currentSpeakerGroup].speakers.size()))
	{
    setSpeakerMatchesFromPosition(beginQuote,m[beginQuote].audienceObjectMatches,audienceObjectPosition,L"audienceObjectPosition",cWordMatch::flagSpecifiedResolveAudience);
		if (m[beginQuote].objectMatches.size()==1)
			subtract(m[beginQuote].objectMatches[0].object,m[beginQuote].audienceObjectMatches);
		if (previousPrimaryQuote!=(unsigned int)-1 && m[previousPrimaryQuote].nextQuote==beginQuote && 
			  !(m[previousPrimaryQuote].flags&(cWordMatch::flagFromPreviousHailResolveSpeakers|cWordMatch::flagDefiniteResolveSpeakers)) &&
				m[beginQuote].audienceObjectMatches.size()==1 &&
				m[previousPrimaryQuote].objectMatches.size()>1 && in(m[beginQuote].audienceObjectMatches[0].object,m[previousPrimaryQuote].objectMatches)!=m[previousPrimaryQuote].objectMatches.end())
		{
			m[previousPrimaryQuote].objectMatches.clear();
			m[previousPrimaryQuote].objectMatches.push_back(cOM(m[beginQuote].audienceObjectMatches[0].object,SALIENCE_THRESHOLD));
			if (m[previousPrimaryQuote].audienceObjectMatches.size()>1)
			{
				subtract(m[beginQuote].audienceObjectMatches[0].object,m[previousPrimaryQuote].audienceObjectMatches);
				if (m[previousPrimaryQuote].audienceObjectMatches.size()==1 && m[beginQuote].objectMatches.size()>1 && 
					  in(m[previousPrimaryQuote].audienceObjectMatches[0].object,m[beginQuote].objectMatches)!=m[beginQuote].objectMatches.end())
				{
					m[beginQuote].objectMatches.clear();
					m[beginQuote].objectMatches.push_back(cOM(m[previousPrimaryQuote].audienceObjectMatches[0].object,SALIENCE_THRESHOLD));
				  if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION,L"%06d:set speaker to %s// (6) from definite audience@%d",beginQuote,
							objectString(m[beginQuote].objectMatches,tmpstr,true).c_str(),audienceObjectPosition);
				}
			}
		  if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:set speaker to %s//%s (5) from definite audience@%d in next quote",previousPrimaryQuote,
					objectString(m[previousPrimaryQuote].objectMatches,tmpstr,true).c_str(),objectString(m[previousPrimaryQuote].audienceObjectMatches,tmpstr2,true).c_str(),audienceObjectPosition);
		}
	}
  else if (lastDefiniteSpeaker>=0)
    setSpeakerMatchesFromPosition(beginQuote,m[beginQuote].audienceObjectMatches,lastDefiniteSpeaker,L"definite previous speaker",
			(m[lastDefiniteSpeaker].flags&cWordMatch::flagDefiniteResolveSpeakers) ? cWordMatch::flagFromLastDefiniteResolveAudience : (cWordMatch::flagGenderIsAmbiguousResolveSpeakers|cWordMatch::flagMostLikelyResolveAudience));
  else if (beforePreviousSpeakers.size())
	{
		vector <int>::iterator audienceInQuote;
		// exclude audiences that are mentioned in the quote, if there are more than 2 speakers
		if (currentSpeakerGroup<speakerGroups.size() && speakerGroups[currentSpeakerGroup].speakers.size()>2 && m[lastOpeningPrimaryQuote].getRelObject()>=0 &&
			 (m[lastOpeningPrimaryQuote].flags&cWordMatch::flagQuoteContainsSpeaker) && 
			 (audienceInQuote=find(beforePreviousSpeakers.begin(),beforePreviousSpeakers.end(),m[m[lastOpeningPrimaryQuote].getRelObject()].getObject()))!=beforePreviousSpeakers.end())
			beforePreviousSpeakers.erase(audienceInQuote);
		if ((addResolveFlag&cWordMatch::flagAlternateResolveForwardAfterDefinite) && intersect(previousSpeakers,beforePreviousSpeakers,allIn,oneIn) && currentSpeakerGroup<speakerGroups.size())
		{
			beforePreviousSpeakers.clear();
			for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
				if (find(previousSpeakers.begin(),previousSpeakers.end(),*si)==previousSpeakers.end())
					beforePreviousSpeakers.push_back(*si);
		}
    addSpeakerObjects(beginQuote,false,beginQuote,beforePreviousSpeakers,cWordMatch::flagMostLikelyResolveAudience);
	}
  else // if no history, no definite current speaker - guess!
    m[beginQuote].audienceObjectMatches=m[beginQuote].objectMatches;
	// restrict audience position to audience in quote, if more tightly defined.
	if (audienceObjectPosition>=0 && currentSpeakerGroup<speakerGroups.size() && m[audienceObjectPosition].objectMatches.size()>=m[beginQuote].audienceObjectMatches.size())
		unMatchObjects(audienceObjectPosition,m[beginQuote].audienceObjectMatches,false);
  if (debugTrace.traceSpeakerResolution)
  {
    lplog(LOG_RESOLUTION,L"%06d:set speaker to %s//%s (4) most likely assignment",beginQuote,
      objectString(m[beginQuote].objectMatches,tmpstr,true).c_str(),objectString(m[beginQuote].audienceObjectMatches,tmpstr2,true).c_str());
    lplog(LOG_RESOLUTION,L"%06d:set unresolvedSpeaker position %d to %d",beginQuote,unresolvedSpeakers.size(),beginQuote);
  }
  lastDefiniteSpeaker=-1;
  if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:Reset last definite speaker.",beginQuote);
  unresolvedSpeakers.push_back(beginQuote);
}

int maxAges[]=
{ 
	0, // NULL class
	3, // PRONOUN_OBJECT_CLASS
  3, // REFLEXIVE_PRONOUN_OBJECT_CLASS
  3, // RECIPROCAL_PRONOUN_OBJECT_CLASS
  50, // NAME_OBJECT_CLASS
  10, // GENDERED_GENERAL_OBJECT_CLASS
  3, // BODY_OBJECT_CLASS
  10, // GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS
  10, // GENDERED_DEMONYM_OBJECT_CLASS
  3, // NON_GENDERED_GENERAL_OBJECT_CLASS
  3, // NON_GENDERED_BUSINESS_OBJECT_CLASS
  3, // PLEONASTIC_OBJECT_CLASS
  3, // VERB_OBJECT_CLASS
  3, // NON_GENDERED_NAME_OBJECT_CLASS
  10, // META_GROUP_OBJECT_CLASS
  10 // GENDERED_RELATIVE_OBJECT_CLASS
};


void cSource::ageSpeaker(int where,bool inPrimaryQuote,bool inSecondaryQuote,vector <cLocalFocus>::iterator &lfi,int amount)
{ LFS
	// don't age speakers in primary speaker group if in story
	if ((m[where].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE) && currentSpeakerGroup<speakerGroups.size() &&
		  speakerGroups[currentSpeakerGroup].speakers.find(lfi->om.object)!=speakerGroups[currentSpeakerGroup].speakers.end())
	{
    lfi++;
		return;
	}
  lfi->increaseAge(inPrimaryQuote,inSecondaryQuote,amount);
  int tmp,tmp2,tmp3,maxAge=maxAges[objects[lfi->om.object].objectClass];
	// the following age by # of adjectives modification did not result in any significant improvement
	// name objects accumulate adjectives and already last quite long and are matched differently, so don't make them last longer
	//if (objects[lfi->om.object].objectClass!=NAME_OBJECT_CLASS)
		// make objects that have been highly specified (those with more adjectives) last longer
	//	maxAge*=max(min(objects[lfi->om.object].associatedAdjectives.size(),4),1);
	if (lfi->getTotalAge()>maxAge &&
      (tmp=objects[lfi->om.object].numEncountersInSection)<2 &&
      (tmp2=objects[lfi->om.object].numIdentifiedAsSpeakerInSection)==0 &&
      (tmp3=objects[lfi->om.object].numDefinitelyIdentifiedAsSpeakerInSection)==0)
  {
    if (debugTrace.traceSpeakerResolution)
    {
      wstring tmpstr;
      lplog(LOG_RESOLUTION,L"%06d:Removed %s from localObjects",where,objectString(lfi->om.object,tmpstr,true).c_str());
    }
    lfi=localObjects.erase(lfi);
  }
  else
    lfi++;
}

void cSource::ageSpeaker(int where,vector <cLocalFocus>::iterator &lfi,int amount)
{ LFS
  lfi->increaseAge(amount);
  int tmp,tmp2,tmp3;
  if (lfi->getTotalAge()>maxAges[objects[lfi->om.object].objectClass] &&
      (tmp=objects[lfi->om.object].numEncountersInSection)<2 &&
      (tmp2=objects[lfi->om.object].numIdentifiedAsSpeakerInSection)==0 &&
      (tmp3=objects[lfi->om.object].numDefinitelyIdentifiedAsSpeakerInSection)==0)
  {
    if (debugTrace.traceSpeakerResolution)
    {
      wstring tmpstr;
      lplog(LOG_RESOLUTION,L"%06d:Removed %s from localObjects",where,objectString(lfi->om.object,tmpstr,true).c_str());
    }
    lfi=localObjects.erase(lfi);
  }
  else
    lfi++;
}

bool cSource::followObjectChain(int &o)
{ LFS
  int oo=o;
	set <int> keepPastObjects;
	keepPastObjects.insert(o);
  while (objects[o].replacedBy>=0)
	{
		if (keepPastObjects.find(o=objects[o].replacedBy)==keepPastObjects.end()) 
			keepPastObjects.insert(o);
		else
		{
			wstring tmpstr;
			lplog(LOG_ERROR,L"Recursion hit on object %s (1).",objectString(o,tmpstr,true).c_str());
			objects[o].replacedBy=-1;
		}
	}
  return oo!=o;
}

// any mention of object either at the location or matched to a location
int cSource::locationBefore(int object,int where)
{ LFS
  int maxLocation=-1;
  for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end();
       loc!=locEnd; loc++)
     if (loc->at<where && loc->at>maxLocation) maxLocation=loc->at;
  return (maxLocation!=-1) ? maxLocation : objects[object].originalLocation;
}

// any mention of object at the location 
int cSource::atBefore(int object,int where)
{ LFS
  int maxLocation=-1;
  for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end();
       loc!=locEnd; loc++)
     if (loc->at<where && loc->at>maxLocation && m[loc->at].getObject()==object) maxLocation=loc->at;
  return (maxLocation!=-1) ? maxLocation : objects[object].originalLocation;
}

// if all occurrences are in questions, probability statements or are marked RE, reject.
bool cSource::anyAcceptableLocations(int where,int object)
{ LFS
  for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end();
       loc!=locEnd; loc++)
     if (loc->at<where && !(m[loc->at].flags&(cWordMatch::flagInQuestion|cWordMatch::flagInPStatement)) && !(m[loc->at].objectRole&RE_OBJECT_ROLE))
				return true;
  return false;
}

// used in cases where we want to avoid counting objects that have been resolved from a future speaker group
// a guest is guessed to be Boris from future information, but Boris is not actually mentioned, so this would lead to incorrect metagroup resolve
// a group joiner object is one example of an object which is obtained from the future
int cSource::locationBeforeExceptFuture(int object,int where)
{ LFS
  int maxLocation=-1;
  for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end();
       loc!=locEnd; loc++)
     if (loc->at<where && loc->at>maxLocation && !isGroupJoiner(m[loc->at].word)) maxLocation=loc->at;
  return (maxLocation!=-1) ? maxLocation : objects[object].originalLocation;
}

// any physically present mention of object either at the location or matched to a location > where
int cSource::ppLocationBetween(int object,int min,int max)
{ LFS
	bool physicallyEvaluated;
  for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end(); loc!=locEnd; loc++)
     if (loc->at<=max && loc->at>=min && physicallyPresentPosition(loc->at,m[loc->at].beginObjectPosition,physicallyEvaluated,false) && physicallyEvaluated) 
			 return loc->at;
  return -1;
}

// count only those locations which match objects that match the given plurality of the object to be matched against
int cSource::locationBefore(int object,int where,bool plural)
{ LFS
  int maxLocation=-1,o;
  for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end();
       loc!=locEnd; loc++)
	{
		if ((o=m[loc->at].getObject())>=0 && plural!=objects[o].plural) continue;
		if (o<0 && plural!=((m[loc->at].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER))!=0)) continue;
    if (loc->at<where && loc->at>maxLocation) maxLocation=loc->at;
	}
  return (maxLocation!=-1) ? maxLocation : objects[object].originalLocation;
}

int cSource::locationBefore(int object,int where,bool plural,bool notInQuestion)
{ LFS
  int maxLocation=-1,o;
  for (vector <cObject::cLocation>::iterator loc=objects[object].locations.begin(),locEnd=objects[object].locations.end();
       loc!=locEnd; loc++)
	{
		if ((o=m[loc->at].getObject())>=0 && plural!=objects[o].plural) continue;
		if (o<0 && plural!=((m[loc->at].word->second.inflectionFlags&(PLURAL|PLURAL_OWNER))!=0)) continue;
		if (notInQuestion && (m[loc->at].flags&cWordMatch::flagInQuestion)) continue;
    if (loc->at<where && loc->at>maxLocation) maxLocation=loc->at;
	}
  return (maxLocation!=-1) ? maxLocation : objects[object].originalLocation;
}

bool frequencyCompare(const cOM &lhs,const cOM &rhs)
{ LFS
  return lhs.salienceFactor>rhs.salienceFactor;
}

void cSource::processNextSection(int I,int nextSection)
{ LFS
  wstring tmpstr;
  if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:______________________END SECTION: %s___________________________",I,phraseString(sections[nextSection].begin,sections[nextSection].endHeader,tmpstr,true).c_str());
  for (vector <cOM>::iterator mo=sections[nextSection].objectsInNarration.begin(); mo!=sections[nextSection].objectsInNarration.end(); )
  {
    if (objects[mo->object].eliminated || !objects[mo->object].numEncountersInSection || objects[mo->object].numSpokenAboutInSection || objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection || objects[mo->object].numIdentifiedAsSpeakerInSection)
		{
      objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
      mo=sections[nextSection].objectsInNarration.erase(mo);
		}
    else
    {
      mo->salienceFactor=objects[mo->object].numEncountersInSection;
      objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
      mo++;
    }
  }
  sort(sections[nextSection].objectsInNarration.begin(),sections[nextSection].objectsInNarration.end(),frequencyCompare);
  for (vector <cOM>::iterator mo=sections[nextSection].objectsSpokenAbout.begin(); mo!=sections[nextSection].objectsSpokenAbout.end(); )
  {
    if (objects[mo->object].eliminated)
      followObjectChain(mo->object);
    if (!objects[mo->object].numSpokenAboutInSection || objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection || objects[mo->object].numIdentifiedAsSpeakerInSection)
		{
      objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
      mo=sections[nextSection].objectsSpokenAbout.erase(mo);
		}
    else
    {
      mo->salienceFactor=objects[mo->object].numSpokenAboutInSection;
      objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
      mo++;
    }
  }
  sort(sections[nextSection].objectsSpokenAbout.begin(),sections[nextSection].objectsSpokenAbout.end(),frequencyCompare);
  for (vector <cOM>::iterator mo=sections[nextSection].speakerObjects.begin(); mo!=sections[nextSection].speakerObjects.end(); )
  {
    if (objects[mo->object].eliminated)
      followObjectChain(mo->object);
    if (!objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection && !objects[mo->object].numIdentifiedAsSpeakerInSection)
		{
      objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
      mo=sections[nextSection].speakerObjects.erase(mo);
		}
    else if (objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection)
    {
      sections[nextSection].definiteSpeakerObjects.push_back(cOM(mo->object,objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection));
      objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
      mo=sections[nextSection].speakerObjects.erase(mo);
    }
    else
    {
      mo->salienceFactor=objects[mo->object].numIdentifiedAsSpeakerInSection;
      objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
      mo++;
    }
  }
  sort(sections[nextSection].speakerObjects.begin(),sections[nextSection].speakerObjects.end(),frequencyCompare);
  if (debugTrace.traceSpeakerResolution)
  {
    for (vector <cOM>::iterator mo=sections[nextSection].speakerObjects.begin(); mo!=sections[nextSection].speakerObjects.end(); mo++)
      if (!objects[mo->object].neuter && !objects[mo->object].plural)
        lplog(LOG_RESOLUTION,L"SPEAKER %s (%d occurrences)",objectString(mo->object,tmpstr,false).c_str(),mo->salienceFactor);
    for (vector <cOM>::iterator mo=sections[nextSection].objectsSpokenAbout.begin(); mo!=sections[nextSection].objectsSpokenAbout.end(); mo++)
      if (objects[mo->object].objectClass==NAME_OBJECT_CLASS)
        lplog(LOG_RESOLUTION,L"SPOKENABOUT %s (%d occurrences)",objectString(mo->object,tmpstr,false).c_str(),mo->salienceFactor);
    for (vector <cOM>::iterator mo=sections[nextSection].objectsInNarration.begin(),moEnd=sections[nextSection].objectsInNarration.end(); mo!=moEnd; mo++)
      if (objects[mo->object].objectClass==NAME_OBJECT_CLASS)
        lplog(LOG_RESOLUTION,L"NARRATION %s (%d occurrences)",objectString(mo->object,tmpstr,false).c_str(),mo->salienceFactor);
    if (nextSection+1<(signed)sections.size())
      lplog(LOG_RESOLUTION,L"%06d:____________________BEGIN SECTION: %s___________________________",I,phraseString(I,sections[nextSection+1].endHeader,tmpstr,true).c_str());
  }
}

void cSource::clearNextSection(int I,int nextSection)
{ LFS
  wstring tmpstr;
  if (debugTrace.traceSpeakerResolution)
    lplog(LOG_RESOLUTION,L"%06d:______________________END SECTION: %s___________________________",I,phraseString(sections[nextSection].begin,sections[nextSection].endHeader,tmpstr,true).c_str());
  for (vector <cOM>::iterator mo=sections[nextSection].objectsInNarration.begin(); mo!=sections[nextSection].objectsInNarration.end(); mo++)
    objects[mo->object].numEncountersInSection=0;
	sections[nextSection].objectsInNarration.clear();
  for (vector <cOM>::iterator mo=sections[nextSection].objectsSpokenAbout.begin(); mo!=sections[nextSection].objectsSpokenAbout.end(); mo++)
  {
    if (objects[mo->object].eliminated)
      followObjectChain(mo->object);
    objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=0;
  }
	sections[nextSection].objectsSpokenAbout.clear();
  for (vector <cOM>::iterator mo=sections[nextSection].speakerObjects.begin(); mo!=sections[nextSection].speakerObjects.end(); mo++)
  {
    if (objects[mo->object].eliminated)
      followObjectChain(mo->object);
    objects[mo->object].numEncountersInSection=objects[mo->object].numSpokenAboutInSection=objects[mo->object].numIdentifiedAsSpeakerInSection=objects[mo->object].numDefinitelyIdentifiedAsSpeakerInSection=0;
  }
	sections[nextSection].speakerObjects.clear();
	sections[nextSection].definiteSpeakerObjects.clear();
  if (debugTrace.traceSpeakerResolution)
  {
    if (nextSection+1<(signed)sections.size())
      lplog(LOG_RESOLUTION,L"%06d:____________________BEGIN SECTION: %s___________________________",I,phraseString(I,sections[nextSection+1].endHeader,tmpstr,true).c_str());
  }
}

bool sectionCompareBySpeakerMatches(cSource::cSection lhs, cSource::cSection rhs)
{ LFS
  if (lhs.speakersMatched+lhs.speakersNotMatched>0 && rhs.speakersMatched+rhs.speakersNotMatched>0)
    return lhs.speakersMatched*100/(lhs.speakersMatched+lhs.speakersNotMatched) > rhs.speakersMatched*100/(rhs.speakersMatched+rhs.speakersNotMatched);
  if (lhs.speakersMatched+lhs.speakersNotMatched>0) return true;
  return false;
}

void cSource::printSectionStatistics(void)
{ LFS
  vector <cSection>::iterator s=sections.begin();
  sort(sections.begin(),sections.end(),sectionCompareBySpeakerMatches);
  for (unsigned int I=0; I<sections.size(); I++,s++)
  {
    wstring sectionHeader;
    for (unsigned int is=s->begin; is<s->endHeader; is++)
      if (m[is].word!=Words.sectionWord)
        sectionHeader+=m[is].word->first+L" ";
    if (s->counterSpeakersMatched+s->counterSpeakersNotMatched)
      lplog(LOG_RESOLUTION,L"%-30.30s: %04d/%04d out of %04d %03d%%/%03d%%",sectionHeader.c_str(),
						s->speakersMatched,s->counterSpeakersMatched,s->speakersMatched+s->speakersNotMatched,
						s->speakersMatched*100/(s->speakersMatched+s->speakersNotMatched),s->counterSpeakersMatched*100/(s->counterSpeakersMatched+s->counterSpeakersNotMatched));
  }
  if (counterSpeakersMatched+counterSpeakersNotMatched)
    lplog(LOG_RESOLUTION,L"%04d/%04d out of %04d %03d%%/%03d%% speakersMatched/counterSpeakersMatched",
					speakersMatched,counterSpeakersMatched,speakersMatched+speakersNotMatched,
					speakersMatched*100/(speakersMatched+speakersNotMatched),counterSpeakersMatched*100/(counterSpeakersMatched+counterSpeakersNotMatched));
}

void cSource::mergeFocusResolution(int o,int I,vector <int> &lastSubjects,bool &clearBeforeSet)
{ LFS
	bool fromPreviousSection=false;
  if (currentSpeakerGroup<speakerGroups.size() && 
		  (find(speakerGroups[currentSpeakerGroup].speakers.begin(),speakerGroups[currentSpeakerGroup].speakers.end(),o)!=speakerGroups[currentSpeakerGroup].speakers.end() ||
			(fromPreviousSection=(currentSpeakerGroup>0 && find(speakerGroups[currentSpeakerGroup-1].speakers.begin(),speakerGroups[currentSpeakerGroup-1].speakers.end(),o)!=speakerGroups[currentSpeakerGroup-1].speakers.end()))))
  {
	  wstring tmpstr,tmpstr2;
    if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d: PIS POST speaker %s%s %s inserted (section %d) ZXZ inserted into lastSubjects",I,(fromPreviousSection) ? L" PS":L"",objectString(o,tmpstr,false).c_str(),m[I].roleString(tmpstr2).c_str(),section);
    if (section<sections.size()) sections[section].preIdentifiedSpeakerObjects.insert(o);
    if (clearBeforeSet)
      lastSubjects.clear();
    lastSubjects.push_back(o);
		clearBeforeSet=false;
  }
}

// REF:SUBGROUPS
// speaker groups are not enough to identify who is speaking.
// when one group meets another, and one is of a different size, there is a preference for
//   underspecified singular pronouns to match the speaker group that has one member, and
//   underspecified plural pronouns to match the speaker group that has more than one member.
// for example:
//   SG1 - speaker group having n>1 members (a member 'N', all of members are 'NS')
//   SUPER_SG1 - speaker group after SG1 having all of SG1 plus 1 member (which is MEM_ADD)
//   MEM_ADD - member added to SG1 to make SUPER_SG1
//
//   SG2 - speaker group having one member (which is MEM_ADD)
//   SUPER_SG2 - speaker group after SG2 having all of SG2 plus n>1 members (a member 'N', all of members 'NS')
//   MEM_ADD - the sole member in SG2
// if matched to members 'OS[Old Speakers]' and MEM_ADD:
//   IN QUOTES:
//   'we'/'us'/'our'/'ours'/'ourselves' if speaker is also part of 'OS', then match ONLY to 'OS'.
//      If speaker is MEM_ADD, then age all the speakers (ageSinceLastSpeakerGroup should be set to disable this subgrouping)
//   'you'/'your'/'yours'/'yourself'/'yourselves' if speaker is also part of 'OS', then match ONLY to MEM_ADD.
//      If speaker is MEM_ADD, then match ONLY to 'OS'.
//   NOT IN QUOTES:
//   'they'/'them'/'their'/'theirs'/'themselves'  match ONLY to 'OS'
//   'he'/'him'/'his'/'himself' or 'she'/'her'/'hers'/'herself' match ONLY to MEM_ADD
// restrictToAlreadyMatched - because the only match may be from a single speaker
//   if false, add all other members of the group - don't restrict the choice to the matches already there.
void cSource::preferSubgroupMatch(int where,int objectClass,int inflectionFlags,bool inQuote,int speakerObject,bool restrictToAlreadyMatched)
{ LFS
  vector <cWordMatch>::iterator im=m.begin()+where;
	int o;
  if ((o=im->getObject())== cObject::eOBJECTS::UNKNOWN_OBJECT || currentSpeakerGroup==0 ||
      (o>=0 &&
       objectClass!=PRONOUN_OBJECT_CLASS &&
       objectClass!=REFLEXIVE_PRONOUN_OBJECT_CLASS &&
       objectClass!=RECIPROCAL_PRONOUN_OBJECT_CLASS &&
			 objectClass!=GENDERED_GENERAL_OBJECT_CLASS))
    return;
	cSpeakerGroup syntheticCurrentSpeakerGroup;
	cSpeakerGroup *csg;
	if (currentSpeakerGroup>=speakerGroups.size())
	{
		if (speakerGroups[currentSpeakerGroup-1].groupedSpeakers.empty()) return;
		// create synthetic current speaker group
		wstring tmpstr,tmpstr2,tmpstr3,tmpstr4;
		syntheticCurrentSpeakerGroup=tempSpeakerGroup;
	  csg=&syntheticCurrentSpeakerGroup;
		for (unsigned int I=0; I<nextNarrationSubjects.size(); I++) csg->speakers.insert(nextNarrationSubjects[I]);
		csg->speakers.insert(speakerGroups[currentSpeakerGroup-1].speakers.begin(),speakerGroups[currentSpeakerGroup-1].speakers.end());
		csg->groupedSpeakers=speakerGroups[currentSpeakerGroup-1].groupedSpeakers;
		determinePreviousSubgroup(where,currentSpeakerGroup,csg);
		if (csg->groupedSpeakers.size()==csg->speakers.size())
		{
			csg->groupedSpeakers.clear();
			determineSubgroupFromGroups(syntheticCurrentSpeakerGroup);
		}
		subtract(csg->speakers,csg->groupedSpeakers,csg->singularSpeakers);
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:preferSubgroupMatch: tempSpeakerGroup=%s. previous group=%s. nextNarrationSubjects=%s. synthetic speaker group=%s",
				where,toText(tempSpeakerGroup,tmpstr),toText(speakerGroups[currentSpeakerGroup-1],tmpstr3),objectString(nextNarrationSubjects,tmpstr2).c_str(),toText(*csg,tmpstr4));
	}
	else
  // matched to any groups?
		csg=&speakerGroups[currentSpeakerGroup];
  if (csg->groupedSpeakers.empty()) return;
  if (restrictToAlreadyMatched)
  {
    if (im->objectMatches.size()<=1) return;
    bool matchedGrouped=false,matchedSingular=false;
    for (vector <cOM>::iterator omi=im->objectMatches.begin(),omiEnd=im->objectMatches.end(); omi!=omiEnd; omi++)
      if (csg->speakers.find(omi->object)!=csg->speakers.end())
      {
        if (csg->singularSpeakers.find(omi->object)!=csg->singularSpeakers.end())
          matchedSingular=true;
        else
          matchedGrouped=true;
      }
    if (!matchedSingular || !matchedGrouped) return;
  }
  //int inflectionFlags=im->word->second.inflectionFlags;
  bool plural=(inflectionFlags&PLURAL)==PLURAL || (inflectionFlags&PLURAL_OWNER)==PLURAL_OWNER;
	bool male=o== cObject::eOBJECTS::OBJECT_UNKNOWN_MALE || o== cObject::eOBJECTS::OBJECT_UNKNOWN_MALE_OR_FEMALE || (o>=0 && objects[o].male);
	bool female=o== cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE || o== cObject::eOBJECTS::OBJECT_UNKNOWN_MALE_OR_FEMALE || (o>=0 && objects[o].female);
  bool matchOnlyGroupedSpeakers=false,matchOnlySingularSpeakers=false;
  bool firstPerson=(inflectionFlags&FIRST_PERSON)!=0,secondPerson=(inflectionFlags&SECOND_PERSON)!=0,thirdPerson=(inflectionFlags&THIRD_PERSON)!=0 || (o>=0 && objectClass==GENDERED_GENERAL_OBJECT_CLASS);
  if (inQuote)
  {
    if (csg->speakers.find(speakerObject)==csg->speakers.end()) return;
    bool speakerInSingularSpeakers=(csg->singularSpeakers.find(speakerObject)!=csg->singularSpeakers.end());
    if (plural)
    {
      matchOnlyGroupedSpeakers=(firstPerson && !speakerInSingularSpeakers) || (secondPerson && !firstPerson && speakerInSingularSpeakers);
    }
    else if (secondPerson) // "yourself"
    {
      if (speakerInSingularSpeakers) 
				matchOnlyGroupedSpeakers=true;
			else
				matchOnlySingularSpeakers=true;
    }
  }
  else
  {
    if (plural && !firstPerson) matchOnlyGroupedSpeakers=true;
    if (!plural && thirdPerson) matchOnlySingularSpeakers=true;
  }
  if (matchOnlyGroupedSpeakers)
  {
		for (vector <cOM>::iterator omi=im->objectMatches.begin(); omi!=im->objectMatches.end(); )
		if (csg->singularSpeakers.find(omi->object)!=csg->singularSpeakers.end())
				omi=im->objectMatches.erase(omi);
			else
				omi++;
		for (set <int>::iterator si=csg->groupedSpeakers.begin(),siEnd=csg->groupedSpeakers.end(); si!=siEnd; si++)
			if (in(*si,im->objectMatches)==im->objectMatches.end() && ((male && female) || (objects[*si].male==male && objects[*si].female==female)))
			{
				im->objectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
		    objects[*si].locations.push_back(cObject::cLocation(where));
		    objects[*si].updateFirstLocation(where);
			}
  }
  else if (matchOnlySingularSpeakers && csg->singularSpeakers.size())
  {
		// save, in case none of the singularSpeakers match the gender of the matched pronoun
		vector <cOM> saveObjectMatches=im->objectMatches;
		csg->singularSpeakers.clear();
		subtract(csg->speakers,csg->groupedSpeakers,csg->singularSpeakers); 
		for (vector <cOM>::iterator omi=im->objectMatches.begin(); omi!=im->objectMatches.end(); )
			if (csg->singularSpeakers.find(omi->object)==csg->singularSpeakers.end())
					omi=im->objectMatches.erase(omi);
			else
				omi++;
		// add plural matches from localObjects
		if (im->objectMatches.empty())
		{
		  vector <cLocalFocus>::iterator lsi;
			cOM om(-1,-10000);
			for (set <int>::iterator si=csg->singularSpeakers.begin(),siEnd=csg->singularSpeakers.end(); si!=siEnd; si++)
				if ((lsi=in(*si))!=localObjects.end() && om.salienceFactor<lsi->om.salienceFactor)
				  om=lsi->om;
			if (om.object>=0 && ((male && female) || (objects[om.object].male==male && objects[om.object].female==female)))
			{
				im->objectMatches.push_back(om);
		    objects[om.object].locations.push_back(cObject::cLocation(where));
		    objects[om.object].updateFirstLocation(where);
			}
		}
		if (im->objectMatches.empty())
			im->objectMatches=saveObjectMatches;
	}
	else
		return;
  if (debugTrace.traceObjectResolution || debugTrace.traceSpeakerResolution)
  {
    wstring tmpstr,tmpstr2,tmpstr3,tmpstr4;
    lplog(LOG_RESOLUTION,L"%06d:%s: ALL=%s GROUPED=%s SINGULAR=%s FINAL=%s.",
			where,(matchOnlyGroupedSpeakers) ? L"matchOnlyGroupedSpeakers":L"matchOnlySingularSpeakers",
			objectString(csg->speakers,tmpstr).c_str(),objectString(csg->groupedSpeakers,tmpstr2).c_str(),
			objectString(csg->singularSpeakers,tmpstr3).c_str(),
         objectString(im->objectMatches,tmpstr4,true).c_str());
  }
}

// if a character suddenly references a new person in the middle of a conversation, and that person matches a person already
// in the speaker group exclusively, then replace the one in the speaker group with the hailed speaker.
bool cSource::matchNewHail(int where,vector <cSpeakerGroup>::iterator sg,int o)
{ LFS
	set<int>::iterator pm=sg->speakers.end();
	for (set<int>::iterator s=sg->speakers.begin(),sEnd=sg->speakers.end(); s!=sEnd; s++)
		if (potentiallyMergable(where,objects.begin()+o,objects.begin()+*s,false,false))
		{
			if (pm!=sg->speakers.end()) // more than one?  return false.
				return false;
			pm=s;
		}
	if (pm==sg->speakers.end()) return false; // none? return false.
	vector <cObject>::iterator object=objects.begin()+o;
	if (objects[o].objectClass!=NAME_OBJECT_CLASS)
		replaceObjectWithObject(where,object,*pm,L"newHailExclusiveMatch");
	return true;
}

// old code:
		  //((im->objectRole&~ID_SENTENCE_TYPE)==SUBJECT_ROLE || 
			// (im->objectRole&~ID_SENTENCE_TYPE)==(SUBJECT_ROLE|MPLURAL_ROLE))
			// ) // must not be an subject of a subsidiary either (no &)
// in secondary quotes, inPrimaryQuote=false
void cSource::accumulateSubjects(int I,int o,bool inPrimaryQuote,bool inSecondaryQuote,int &whereSubject,bool &erasePreviousSubject,vector <int> &lastSubjects)
{ LFS
	bool isNotPhysicallyPresent=false,hasFocus=isFocus(I,inPrimaryQuote,inSecondaryQuote,o,isNotPhysicallyPresent,lastSubjects.empty());
	if (((hasFocus && inPrimaryQuote) || ((m[I].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE)) && inSecondaryQuote)) && 
		  (m[I].objectRole&IN_EMBEDDED_STORY_OBJECT_ROLE) && !isNotPhysicallyPresent && o>=0 && currentEmbeddedSpeakerGroup>=0)
	{
	  vector <cSpeakerGroup>::iterator esg=speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.begin()+currentEmbeddedSpeakerGroup;
		// if HAIL, and not already in embedded speaker group, and not a name, try to match the object to a particular object already in a speaker group.
		if ((m[I].objectRole&(HAIL_ROLE|IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE)) && inSecondaryQuote && esg->speakers.find(o)==esg->speakers.end() &&
			  objects[o].objectClass!=NAME_OBJECT_CLASS && matchNewHail(I,esg,o))
			return;
		// detect whether o is a body object, where any speaker is the owner.  If so, skip insertion.
		if (objects[o].objectClass==BODY_OBJECT_CLASS)
		{
			if (objects[o].getOwnerWhere()>=0)
				for (unsigned int omi=0; omi<m[objects[o].getOwnerWhere()].objectMatches.size(); omi++)
					esg->speakers.insert(m[objects[o].getOwnerWhere()].objectMatches[omi].object);
		}
		else
			esg->speakers.insert(o);
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr,tmpstr2;
			lplog(LOG_RESOLUTION,L"%06d: Inserted embedded subject %s into embedded speakergroup %s.",I,objectString(o,tmpstr,true).c_str(),toText(speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup],tmpstr2));
		}
	}
	__int64 or=m[I].objectRole;
  if (hasFocus && ((or&SUBJECT_ROLE) || lastSubjects.empty() || whereSubject==I) && 
		  !(or&(NO_ALT_RES_SPEAKER_ROLE|IN_EMBEDDED_STORY_OBJECT_ROLE)))
	{
		mergeFocusResolution(o,I,lastSubjects,erasePreviousSubject);
		whereSubject=I;
	}
	// whatever the subject resolved to must have been unacceptable.  However, it is a new subject so at least erase the old one.
	else if (!inPrimaryQuote && erasePreviousSubject && (m[I].objectRole&SUBJECT_ROLE) && m[I].getObject()>=0 && objects[m[I].getObject()].objectClass==PRONOUN_OBJECT_CLASS)
	{
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr;
			lplog(LOG_RESOLUTION,L"%06d: ZXZ Erased last subjects %s.",I,objectString(lastSubjects,tmpstr).c_str());
		}
    lastSubjects.clear();
		erasePreviousSubject=false;
		whereSubject=-1;
	}
}

// if previousParagraph is set, then don't set NO_ALT_RES_SPEAKER_ROLE because this is used to ignore this 
//   object for subject purposes, which is used to set the speakers of subsequent paragraphs.  In the
//   cases of NO_ALT_RES_SPEAKER_ROLE, we want to ignore it, but in the case of previousParagraph, the object sets
//   the speaker of the NEXT paragraph, so this is what we want.
int cSource::determineSpeaker(int beginQuote,int endQuote,bool inPrimaryQuote,bool noSpeakerAfterward,bool &definitelySpeaker,int &audienceObjectPosition)
{ LFS
  bool definitelyAfterSpeaker=true,previousParagraph=false,crossedSectionBoundary=false; // checkCataSpeaker=false,
  int speakerPosition=-1,beforeLocation=speakerBefore(beginQuote,previousParagraph),afterSpeakerPosition=-1; // unknown
  if (beforeLocation>=0)
    speakerPosition=scanForSpeaker(beforeLocation,definitelySpeaker,crossedSectionBoundary,audienceObjectPosition);
  if (!noSpeakerAfterward)
    afterSpeakerPosition=scanForSpeaker(endQuote,definitelyAfterSpeaker,crossedSectionBoundary,audienceObjectPosition);
	// if both directions have plausible speakers with verbs not having objects
	// prefer the after speaker if the quote is preceded by a sentence end
	if (speakerPosition>0 && afterSpeakerPosition>0 && definitelyAfterSpeaker && definitelySpeaker && beginQuote && isEOS(beginQuote-1))
	  definitelySpeaker=false;
	if (speakerPosition<0 || (afterSpeakerPosition>=0 && definitelyAfterSpeaker && !definitelySpeaker))
	{
		speakerPosition=afterSpeakerPosition;
		definitelySpeaker=definitelyAfterSpeaker;
	}
	// should this be considered a subject of a sentence to be used as an alternative resolution speaker?
	// if it should NOT, set to NO_ALT_RES_SPEAKER_ROLE.
	//crossedSectionBoundary=false;
	if (speakerPosition>=0 && m[speakerPosition].principalWherePosition>=0) 
	{
		if (inPrimaryQuote)
		{
			m[m[speakerPosition].principalWherePosition].objectRole|=PRIMARY_SPEAKER_ROLE; 
			if (!previousParagraph && !crossedSectionBoundary) 
				m[m[speakerPosition].principalWherePosition].objectRole|=NO_ALT_RES_SPEAKER_ROLE; 
		}
		else
			m[m[speakerPosition].principalWherePosition].objectRole|=SECONDARY_SPEAKER_ROLE; 
		m[beginQuote].speakerPosition=(m[speakerPosition].principalWherePosition>=0) ? m[speakerPosition].principalWherePosition : speakerPosition;
	}
	if (audienceObjectPosition>=0)
		m[beginQuote].audiencePosition=audienceObjectPosition;
	// cata speaker
	if (speakerPosition<0 && endQuote+2<(signed)m.size() && m[endQuote+1].word==Words.sectionWord && m[endQuote+2].principalWherePosition>=0 && 
		  isVoice(m[endQuote+2].principalWherePosition))
	{
		speakerPosition=endQuote+2;
 	  definitelySpeaker=true;
	}
	return speakerPosition;
}

bool cSource::isAgentObject(int object)
{ LFS
	vector <cObject>::iterator o=objects.begin()+object;
	return o->objectClass==PRONOUN_OBJECT_CLASS ||
		  o->objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS ||
			o->objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS ||
			(o->objectClass==NAME_OBJECT_CLASS && ((o->male ^ o->female) || o->PISDefinite || o->numIdentifiedAsSpeaker)) ||
			o->objectClass==GENDERED_GENERAL_OBJECT_CLASS ||
			o->objectClass==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS ||
			o->objectClass==GENDERED_DEMONYM_OBJECT_CLASS ||
			o->objectClass==GENDERED_RELATIVE_OBJECT_CLASS ||
			o->objectClass==META_GROUP_OBJECT_CLASS;
}

bool cSource::hasAgentObjectOwner(int where,int &ownerWhere)
{ LFS
	return m[where].getObject()>=0 && 
					 (ownerWhere=objects[m[where].getObject()].getOwnerWhere())>=0 && 
					 ((m[ownerWhere].getObject()>=0 && isAgentObject(m[ownerWhere].getObject())) ||
					 (m[ownerWhere].getObject()== cObject::eOBJECTS::OBJECT_UNKNOWN_MALE || m[ownerWhere].getObject()== cObject::eOBJECTS::OBJECT_UNKNOWN_FEMALE || m[ownerWhere].getObject()== cObject::eOBJECTS::OBJECT_UNKNOWN_MALE_OR_FEMALE));
}

void cSource::checkInfinitivePhraseForLocation(vector <cTagLocation> &tagSet,bool locationTense)
{ LFS
	int nextTag=-1;
	int iverbTag=findTag(tagSet,L"IVERB",nextTag);
	if (iverbTag<0) return;
	tIWMM subjectWord=wNULL;
	int subjectTag=findOneTag(tagSet,L"SUBJECT",-1),subjectObject=-1,whereSubjectObject=-1;
	if (!resolveTag(tagSet,subjectTag,subjectObject,whereSubjectObject,subjectWord)) return;
	// find verb of IVERB to associate with verb
	vector < vector <cTagLocation> > iTagSets;
	if (startCollectTagsFromTag(debugTrace.traceRelations,iverbTagSet,tagSet[iverbTag],iTagSets,-1,true, false, L"check infinitive phrase for location")>0)
	for (unsigned int K=0; K<iTagSets.size(); K++)
	{
		if (debugTrace.traceRelations)
			::printTagSet(LOG_RESOLUTION,L"IVERB",K,iTagSets[K]);
		accumulateLocation(tagSet[iverbTag].sourcePosition,iTagSets[K],subjectObject,locationTense);
	}
}

void cSource::preparePrepMap(void)
{ LFS
	if (prepTypesMap.size()) return;
	for (unsigned int I=0; prepRelations[I].prep; I++)
		prepTypesMap[prepRelations[I].prep]=prepRelations[I].prepRelationType;
	unordered_map <wstring,int>::iterator pmi,pmiEnd;
	for (unsigned int I=0; prepEquivalents[I].prep; I++)
		if ((pmi=prepTypesMap.find(prepEquivalents[I].equivalent))!=prepTypesMap.end())
			prepTypesMap[prepEquivalents[I].prep]=pmi->second;
	// set which prepositions may indicate movement
	set <int> movementPrepTypes;
	int movementPrepTypesArray[]={ tprNEAR,tprX,tprIN,tprAT,tprY,tprSPAT,tprFROM,tprTO }; // tprFOR? set out briskly for home
	for (unsigned int I=0; I<sizeof(movementPrepTypesArray)/sizeof(movementPrepTypesArray[0]); I++)
		movementPrepTypes.insert(movementPrepTypesArray[I]);
	tIWMM p;
	for (pmi=prepTypesMap.begin(),pmiEnd=prepTypesMap.end(); pmi!=pmiEnd; pmi++)
		if (movementPrepTypes.find(pmi->second)!=movementPrepTypes.end() && (p=Words.query(pmi->first))!=Words.end())
			p->second.flags|=cSourceWordInfo::prepMoveType;
}

// "P","PREPOBJECT","VERBSUBOBJECT"
int cSource::evaluatePrepObjectRelation(vector <cTagLocation> &tagSet,int &pIndex,tIWMM &prepWord,int &object,int &wherePrepObject,tIWMM &objectWord)
{ LFS
	// find preposition
	int tag,nextTag=-1;
	if ((tag=findTag(tagSet,L"P",nextTag))<0 || !tagIsCertain(tagSet[tag].sourcePosition)) return -1;
	prepWord=m[tagSet[tag].sourcePosition].word;
	pIndex=prepWord->second.index;
	nextTag=-1;
	// find PREPOBJECT
	if ((tag=findTag(tagSet,L"PREPOBJECT",nextTag))<0) return -1;
	//findObject(tagSet[tag],wherePrepObject);
	resolveTag(tagSet,tag,object,wherePrepObject,objectWord);
	return 0;
}

// accumulate (Subject,verb,preposition,object) OR
//            (Subject,verb,object,preposition,object)
// Subject must be a speaker
// where object is a Proper Noun OR a neuter object
// run prep through prepEquivalents.  Prep must be in:
// tprNEAR,tprX,tprIN,tprAT,tprY,tprSPAT,tprFROM,tprTO
// this tagSet is from subjectVerbRelationTagSet
// this is only called if the verbTense is narrative and true.
void cSource::accumulateLocation(int where,vector <cTagLocation> &tagSet,int subjectObject,bool locationTense)
{ LFS
	if (accumulateLocationLastLocation==where) return;
	wstring reason;
	int nextTag=-1,verbTagIndex;
	// get preposition and subobject
	int prepTag=findTag(tagSet,L"PREP",nextTag);
	if (prepTag<0) return;
	vector < vector <cTagLocation> > tagSets;
	cTagLocation ts=tagSet[prepTag],nts=ts;
	if (m[ts.sourcePosition].objectRole&SUBJECT_ROLE)
		return; // no prepositions in subjects are meaningful for this application
	tIWMM prepWord=wNULL,subObjectWord=wNULL;
	int pIndex,subobject=-1,movementPrepType=-1,embeddedPrepTag=-1,wherePrepObject=-1;
	while (true)
	{
		if (startCollectTagsFromTag(debugTrace.traceRelations,prepTagSet,ts,tagSets,-1,true, false, L"accumulate location")>0)
			for (unsigned int K=0; K<tagSets.size(); K++)
			{
				if (debugTrace.traceRelations)
					::printTagSet(LOG_RESOLUTION,L"PREP",K,tagSets[K]);
				subobject=-1;
				if (evaluatePrepObjectRelation(tagSets[K],pIndex,prepWord,subobject,wherePrepObject,subObjectWord)>=0 && (prepWord->second.flags&cSourceWordInfo::prepMoveType)!=0)
					movementPrepType=1;
				if (movementPrepType>=0) break;
				nextTag=-1;
				if (embeddedPrepTag<0 && ((embeddedPrepTag=findTag(tagSets[K],L"PREP",nextTag))>=0))
					nts=tagSets[K][embeddedPrepTag];
			}
			if (movementPrepType>=0 || embeddedPrepTag<0) break;
			// otherwise start over again if a subsidiary PREP was found
			tagSets.clear();
			ts=nts;
			embeddedPrepTag=-1;
	}
	if (!locationTense)
		reason+=L" tense not valid";
	if (movementPrepType<0)
		reason+=L" prepType not movement";
	if (subobject<0) 
		reason=L" no subobject found";
	if (subjectObject<0)
	{
		tIWMM subjectWord=wNULL;
		int subjectTag=findOneTag(tagSet,L"SUBJECT",-1),whereSubjectObject;
		if (!resolveTag(tagSet,subjectTag,subjectObject,whereSubjectObject,subjectWord)) 
			reason+=L" no subject found";
	}
	//bool subjectInSpeakers=speakerGroups[currentSpeakerGroup].speakers.find(subjectObject)!=speakerGroups[currentSpeakerGroup].speakers.end();
	if ((verbTagIndex=findOneTag(tagSet,L"V_OBJECT",-1))<0) 
		reason+=L" no verb found";
	nextTag=-1;
	int directObjectTag=-1,indirectObjectTag=findTag(tagSet,L"OBJECT",directObjectTag);
	// hObjects are for use with _VERB_BARE_INF - I make/made you approach him, where there is only a relationship between the subject (I) and (you)
	int hObjectTag=findOneTag(tagSet,L"HOBJECT",-1);
	if (hObjectTag>=0)
	{
		directObjectTag=hObjectTag;
		indirectObjectTag=-1;
	}
	// I gave the book  - book is direct object
	// I gave you the book - book is still direct object
	if (directObjectTag<0 && indirectObjectTag>=0)
	{
		directObjectTag=indirectObjectTag;
		indirectObjectTag=-1;
	}
	wstring tmpstr,tmpstr2,tmpstr3,tmpstr4;
	if (reason.length())
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:SV survivor (1) %s %s %s %s %s",where,reason.c_str(),
						objectString(subjectObject,tmpstr,true).c_str(),(verbTagIndex<0) ? L"":m[tagSet[verbTagIndex].sourcePosition].word->first.c_str(),
			(prepWord!=wNULL) ? prepWord->first.c_str() : L"",objectString(subobject,tmpstr2,false).c_str());
		if (movementPrepType>=0 && wherePrepObject>=0)
			m[wherePrepObject].objectRole|=MOVEMENT_PREP_OBJECT_ROLE;
		return;
	}
	if (objects[subobject].plural) reason+=L" plural";
	if (objects[subobject].objectClass==NAME_OBJECT_CLASS && 
		(objects[subobject].PISDefinite || objects[subobject].numIdentifiedAsSpeaker || (objects[subobject].male ^ objects[subobject].female)))
		reason+=L" name (sex/speaker)";
	if (objects[subobject].objectClass!=BODY_OBJECT_CLASS && objects[subobject].objectClass!=NAME_OBJECT_CLASS && objects[subobject].objectClass!=NON_GENDERED_NAME_OBJECT_CLASS)
	{
		if (!objects[subobject].neuter) reason+=L" not neuter";
		if (!m[wherePrepObject].isPhysicalObject()) reason+=L" not physical object";
	}
	if (m[wherePrepObject].word->second.timeFlags&T_UNIT) reason+=L" time unit";
	// don't allow "a, an" or quantifiers (all, every, etc)
	// may allow "a" or "an" in the future if statistics can be collected on the principal noun 
	vector <cWordMatch>::iterator im=m.begin()+objects[subobject].begin;
	// allow England
	if (objects[subobject].objectClass!=NAME_OBJECT_CLASS && 
		im->word->first!=L"the" && im->queryWinnerForm(demonstrativeDeterminerForm)<0 && im->queryWinnerForm(possessiveDeterminerForm)<0) 
		reason+=L" no determiner";
	// preposition occurs before any object (or there is no object)
	if (directObjectTag<0 || tagSet[prepTag].sourcePosition<tagSet[directObjectTag].sourcePosition)
	{
		if (subjectObject<0 || !isAgentObject(subjectObject)) 
			reason+=L" subject not agent";
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:AL%s SVPO %s %s %s %s %s",where,(reason.empty()) ? L"VALID":L"",reason.c_str(),
						objectString(subjectObject,tmpstr,true).c_str(),m[tagSet[verbTagIndex].sourcePosition].word->first.c_str(),
						prepWord->first.c_str(),objectString(subobject,tmpstr2,true).c_str());
		accumulateLocationLastLocation=where;
		if (wherePrepObject>=0)
		{
			if (reason.empty())
				m[wherePrepObject].objectRole|=PLACE_OBJECT_ROLE;
			m[wherePrepObject].objectRole|=(movementPrepType<0) ? NON_MOVEMENT_PREP_OBJECT_ROLE:MOVEMENT_PREP_OBJECT_ROLE;
		}
		return;
	}
	int directObject=-1,whereDirectObject=-1,indirectObject=-1,whereIndirectObject=-1;
	tIWMM directObjectWord=wNULL,indirectObjectWord=wNULL;
	if (resolveTag(tagSet,directObjectTag,directObject,whereDirectObject,directObjectWord) && directObject>=0)
	{
		if (!isAgentObject(directObject)) 
			reason+=L" direct object not agent";
		// preposition occurs before any object (or there is no object)
		if (indirectObjectTag<0 || tagSet[prepTag].sourcePosition<tagSet[indirectObjectTag].sourcePosition)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:AL%s SVOPO %s %s %s %s %s %s",where,(reason.empty()) ? L"VALID":L"",reason.c_str(),
							objectString(subjectObject,tmpstr,true).c_str(),m[tagSet[verbTagIndex].sourcePosition].word->first.c_str(),
							objectString(directObject,tmpstr3,true).c_str(),
							prepWord->first.c_str(),objectString(subobject,tmpstr2,true).c_str());
			accumulateLocationLastLocation=where;
			if (wherePrepObject>=0)
			{
				if (reason.empty())
					m[wherePrepObject].objectRole|=PLACE_OBJECT_ROLE;
				m[wherePrepObject].objectRole|=(movementPrepType<0) ? NON_MOVEMENT_PREP_OBJECT_ROLE:MOVEMENT_PREP_OBJECT_ROLE;
			}
			return;
		}
		if (resolveTag(tagSet,indirectObjectTag,indirectObject,whereIndirectObject,indirectObjectWord) && indirectObject>=0)
		{
			if (!isAgentObject(indirectObject)) 
				reason+=L" indirect object not agent";
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:AL%s SVOOPO %s %s %s %s %s %s",where,(reason.empty()) ? L"VALID":L"",reason.c_str(),
							objectString(subjectObject,tmpstr,true).c_str(),m[tagSet[verbTagIndex].sourcePosition].word->first.c_str(),
							objectString(directObject,tmpstr3,true).c_str(),objectString(indirectObject,tmpstr4,true).c_str(),
							prepWord->first.c_str(),objectString(subobject,tmpstr2,true).c_str());
			accumulateLocationLastLocation=where;
			if (wherePrepObject>=0)
			{
				if (reason.empty())
					m[wherePrepObject].objectRole|=PLACE_OBJECT_ROLE;
				m[wherePrepObject].objectRole|=(movementPrepType<0) ? NON_MOVEMENT_PREP_OBJECT_ROLE:MOVEMENT_PREP_OBJECT_ROLE;
			}
			return;
		}
	}
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:SV survivor (2) %s %s %s %s %s %s",where,reason.c_str(),
					objectString(subjectObject,tmpstr,true).c_str(),m[tagSet[verbTagIndex].sourcePosition].word->first.c_str(),
					objectString(directObject,tmpstr3,true).c_str(),objectString(indirectObject,tmpstr4,true).c_str(),
					prepWord->first.c_str(),objectString(subobject,tmpstr2,true).c_str());
}

// is this OK?  there is only one object slot, so we are being opportunistic
// in not checking if the object occupies the entire tag...
int cSource::findObject(cTagLocation &tag,int &objectPosition)
{ LFS
	if ((objectPosition=m[tag.sourcePosition].principalWherePosition)<0)
		objectPosition=m[tag.sourcePosition].principalWhereAdjectivalPosition;
	// either this or that
	if (objectPosition<0 && tag.len>1)
		objectPosition=m[tag.sourcePosition+1].principalWherePosition;
	if (objectPosition<0 && m[tag.sourcePosition].getObject()<0 && tag.len>1 && 
		  m[tag.sourcePosition+1].getObject()>=0) 
		objectPosition=tag.sourcePosition+1;
	//int object=m[objectPosition=tag.sourcePosition].principalWherePosition;
	//if (object<0) object=m[tag.sourcePosition].principalWhereAdjectivalPosition;
	//vector <cWordMatch>::iterator im=m.begin()+tag.sourcePosition,imEnd=m.begin()+tag.sourcePosition+tag.len;
	//for (; im!=imEnd && im->getObject()!=object; im++,objectPosition++);
	//return (im!=imEnd && im->getObject()!=UNKNOWN_OBJECT && im->endObjectPosition-im->beginObjectPosition<=tag.len) ? object : -1;
	if (objectPosition<0) return -1;
	return m[objectPosition].getObject();
}

void cSource::assignMetaQueryAudience(int beginQuote,int previousQuote,int primaryObject,int secondaryObject,int secondaryTag,vector <cTagLocation> &tagSet)
{ LFS
	wstring tmpstr,tmpstr2,tmpstr3;
	if (secondaryObject<0 || objects[secondaryObject].objectClass!=NAME_OBJECT_CLASS || 
		  (objects[secondaryObject].name.justHonorific() && m[tagSet[secondaryTag].sourcePosition].queryForm(L"pinr")>=0))
	{
		// if there is no previous quote but there are only two speakers in the current speaker group and the primary is one of them,
		// then the other speaker in the speaker group must be the secondary speaker.
		if (previousQuote<0 && secondaryTag>=0 && currentSpeakerGroup<speakerGroups.size() && 
				speakerGroups[currentSpeakerGroup].speakers.size()==2 && speakerGroups[currentSpeakerGroup].speakers.find(primaryObject)!=speakerGroups[currentSpeakerGroup].speakers.end())
		{
			for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
				if (*si!=primaryObject)
					m[beginQuote].audienceObjectMatches.push_back(cOM(secondaryObject=*si,SALIENCE_THRESHOLD));
		}
		else if (previousQuote>=0 && m[previousQuote].objectMatches.size())
		{
			m[beginQuote].audienceObjectMatches=m[previousQuote].objectMatches;
      if (debugTrace.traceSpeakerResolution)
	      lplog(LOG_RESOLUTION,L"%06d:beginQuote=%d previousQuote=%d secondaryObject=%s pqo=%s",beginQuote,beginQuote,previousQuote,objectString(secondaryObject,tmpstr,true).c_str(),objectString(m[previousQuote].objectMatches[0].object,tmpstr2,true).c_str());
			if (secondaryObject>=0 && secondaryObject!=m[previousQuote].objectMatches[0].object)
			{
				int whereSecondaryObject;
				findObject(tagSet[secondaryTag],whereSecondaryObject);
				m[whereSecondaryObject].objectMatches.push_back(cOM(secondaryObject=m[previousQuote].objectMatches[0].object,SALIENCE_THRESHOLD));
				objects[secondaryObject].locations.push_back(whereSecondaryObject);
			  m[whereSecondaryObject].flags|=cWordMatch::flagObjectResolved;
			}
		}
		else if (secondaryTag>=0)
			m[beginQuote].audienceObjectMatches.push_back(cOM(secondaryObject,SALIENCE_THRESHOLD));
	}
	else
		m[beginQuote].audienceObjectMatches.push_back(cOM(secondaryObject,SALIENCE_THRESHOLD));
	for (unsigned int J=0; J<m[beginQuote].audienceObjectMatches.size(); J++)
		objects[m[beginQuote].audienceObjectMatches[J].object].speakerLocations.insert(beginQuote);
}

// determine if it is an answer.
// If so, set name of speaker to IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE, and set to objectMatches of beginQuote.
// set lastSpeaker to audience (audienceObjectMatches of beginQuote)
// if the speakers in currentSpeakerGroup =2, and the speaker other than the audience does not match the IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE,
//   create an alias for the speaker.
bool cSource::processMetaSpeakerQueryAnswer(int beginQuote,int previousQuote,int lastQuery)
{ LFS
	vector < vector<cTagLocation> > tagSets;
	int element=m[lastQuery].pma.queryPattern(L"_META_SPEAKER_QUERY");
	if (element==-1 || startCollectTags(debugTrace.traceNameResolution,metaNameEquivalenceTagSet,lastQuery,m[lastQuery].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd,tagSets,true,true,L"name equivalence - meta speaker query answer [query]")==0)
		return false;
	wstring tmpstr,tmpstr2,tmpstr3;
	tIWMM aboutWord=wNULL;
	int whereAboutObject=-1,aboutObject= cObject::eOBJECTS::UNKNOWN_OBJECT,aboutTag;
	for (unsigned int I=0; I<tagSets.size(); I++)
	{
		if (debugTrace.traceNameResolution)
			printTagSet(LOG_RESOLUTION,L"MSQ",I,tagSets[I],lastQuery,m[lastQuery].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd);
		if ((aboutTag=findOneTag(tagSets[I],L"NAME_ABOUT",-1))>=0)
			aboutObject=m[whereAboutObject=tagSets[I][aboutTag].sourcePosition].getObject();
	}
	tagSets.clear();
	element=m[beginQuote+1].pma.queryPattern(L"_META_SPEAKER_QUERY_RESPONSE");
	if (element==-1 || startCollectTags(debugTrace.traceNameResolution,metaNameEquivalenceTagSet,beginQuote+1,m[beginQuote+1].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd,tagSets,true,true, L"name equivalence - meta speaker query answer [response]")==0)
		return false;
	int whereAboutResponseObject=-1,aboutResponseObject=-1,aboutResponseTag=-1;
	for (unsigned int I=0; I<tagSets.size(); I++)
	{
		if (debugTrace.traceNameResolution)
			printTagSet(LOG_RESOLUTION,L"MSQR",I,tagSets[I],beginQuote+1,m[beginQuote+1].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd);
		if ((aboutResponseTag=findOneTag(tagSets[I],L"NAME_ABOUT",-1))>=0)
			aboutResponseObject=m[whereAboutResponseObject=tagSets[I][aboutResponseTag].sourcePosition].getObject();
		int primaryTag=findOneTag(tagSets[I],L"NAME_PRIMARY",-1),secondaryTag=findOneTag(tagSets[I],L"NAME_SECONDARY",-1);
		if (primaryTag<0) continue;
		tIWMM secondaryWord=wNULL;
		int secondaryObject=-1,wherePrimaryObject,primaryObject=findObject(tagSets[I][primaryTag],wherePrimaryObject),whereSecondaryObject=-1;
		resolveTag(tagSets[I],secondaryTag,secondaryObject,whereSecondaryObject,secondaryWord);
		if (primaryObject<0) continue;
		if (debugTrace.traceRole)
			lplog(LOG_ROLE,L"%06d:Removed HAIL role (processMetaSpeakerQueryAnswer).",wherePrimaryObject);
		m[wherePrimaryObject].objectRole&=~HAIL_ROLE;
		m[wherePrimaryObject].objectRole|=SUBJECT_ROLE|UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE|FOCUS_EVALUATED;
		bool speakerName=false,audienceName=false,thirdPersonName=false;
		if (aboutResponseObject!= cObject::eOBJECTS::UNKNOWN_OBJECT)
		{
			if (m[whereAboutResponseObject].word->second.inflectionFlags&FIRST_PERSON) 
				speakerName=true;
			else if (m[whereAboutResponseObject].word->second.inflectionFlags&SECOND_PERSON) 
				audienceName=true;
			else 
				thirdPersonName=true;
			whereAboutObject=whereAboutResponseObject;
			aboutObject=aboutResponseObject;
		}
		else if (aboutObject!= cObject::eOBJECTS::UNKNOWN_OBJECT)
		{
			if (m[whereAboutObject].word->second.inflectionFlags&SECOND_PERSON)
				speakerName=true;
			else if (m[whereAboutObject].word->second.inflectionFlags&FIRST_PERSON) // although this would surely be rare...  "what is my name?" "who am I?"
				audienceName=true;
			else 
				thirdPersonName=true;
		}
		if (speakerName)
		{
			m[wherePrimaryObject].objectRole|=IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE;
			if (previousQuote>=0 && m[previousQuote].audienceObjectMatches.size()==1)
				aboutObject=m[previousQuote].audienceObjectMatches[0].object;
		}
		else if (audienceName)
		{
			m[wherePrimaryObject].objectRole|=IN_QUOTE_REFERRING_AUDIENCE_ROLE;
			if (previousQuote>=0 && m[previousQuote].objectMatches.size()==1)
				aboutObject=m[previousQuote].objectMatches[0].object;
		}
		else if (thirdPersonName)
		{
			if (m[whereAboutObject].objectMatches.size())
				aboutObject=m[whereAboutObject].objectMatches[0].object;
			else
				aboutObject=m[whereAboutObject].getObject();
		}
		// assign alias?
		if (primaryObject!=aboutObject)
		{
			equivocateObjects(beginQuote,aboutObject,primaryObject);
			primaryObject=aboutObject;
		}
		if (speakerName && primaryObject>=0)
		{
			m[beginQuote].objectMatches.push_back(cOM(primaryObject,SALIENCE_THRESHOLD));
		  objects[primaryObject].speakerLocations.insert(beginQuote);
			m[beginQuote].flags|=cWordMatch::flagDefiniteResolveSpeakers;
		}
    if (debugTrace.traceSpeakerResolution)
      lplog(LOG_RESOLUTION,L"%06d:beginQuote=%d previousQuote=%d primaryObject=%s secondaryObject=%s aboutObject=%s",I,beginQuote,previousQuote,objectString(primaryObject,tmpstr,true).c_str(),objectString(secondaryObject,tmpstr2,true).c_str(),objectString(aboutObject,tmpstr2,true).c_str());
		assignMetaQueryAudience(beginQuote,previousQuote,primaryObject,secondaryObject,secondaryTag,tagSets[I]);
		if (secondaryObject>=0)
			m[beginQuote].flags|=cWordMatch::flagSpecifiedResolveAudience;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION,L"%06d:MSQA  %s//%s %s",beginQuote,
					objectString(m[beginQuote].objectMatches,tmpstr,true).c_str(),objectString(m[beginQuote].audienceObjectMatches,tmpstr2,true).c_str(),
					speakerResolutionFlagsString(m[beginQuote].flags,tmpstr3).c_str());
		return true;
	}
	return false;
}

bool cSource::matchAliases(int where,int object,set <int> &objs)
{ LFS
	for (set <int>::iterator oi= objs.begin(),oEnd= objs.end();oi!=oEnd; oi++)
		if (matchAliases(where,object,*oi))
			return true;
	return false;
}

bool cSource::matchAliases(int where,int object,vector <cOM> &objs)
{ LFS
	for (vector <cOM>::iterator oi= objs.begin(),oEnd= objs.end(); oi!=oEnd; oi++)
		if (matchAliases(where,object,oi->object))
			return true;
	return false;
}

bool cSource::matchAliases(int where,int object,int aliasObject)
{ LFS
	if (object<=1 || aliasObject<=1)
		return false;
	if (matchAlias(where,aliasObject,object))
		return true;
	for (vector <int>::iterator ai=objects[object].aliases.begin(),aEnd=objects[object].aliases.end(); ai!=aEnd; ai++)
		if (matchAlias(where,aliasObject,*ai))
			return true;
	return false;
}

// my cousin == Peter Marshall's cousin?
bool cSource::matchAliasLocation(int where,int &objectLocation, int &aliasLocation)
{ LFS
	while (objectLocation<(signed)m.size() && m[objectLocation].getObject()== cObject::eOBJECTS::UNKNOWN_OBJECT)
		objectLocation++;
	while (aliasLocation<(signed)m.size() && m[aliasLocation].getObject()== cObject::eOBJECTS::UNKNOWN_OBJECT)
		aliasLocation++;
	if (objectLocation==(signed)m.size() || aliasLocation==(signed)m.size())
		return false;
	//for (unsigned int I=0; I<m[objectLocation].objectMatches.size(); I++)
	//	objects[m[objectLocation].objectMatches[I].object].lsiOffset=localObjects.begin();
	if (m[objectLocation].getObject()>=0)
		objects[m[objectLocation].getObject()].lsiOffset=localObjects.begin();
	bool foundMatch=false;
	// allowing more than one objectMatches allows too loose an association to form an alias,
	// therefore allowing a replacement to succeed that should not be allowed.
	// do not use an objectMatch that occurs in the future (resolveSpeakers would encounter objects that have already
	//   been matched by identifySpeakerGroups)
	if (aliasLocation<where && m[aliasLocation].objectMatches.size()==1 && objects[m[aliasLocation].objectMatches[0].object].lsiOffset!=cNULL)
		foundMatch=true;
	if (m[aliasLocation].getObject()>=0)
		foundMatch|=(objects[m[aliasLocation].getObject()].lsiOffset!=cNULL);
	//for (unsigned int I=0; I<m[objectLocation].objectMatches.size(); I++)
	//	objects[m[objectLocation].objectMatches[I].object].lsiOffset=NULL;
	if (m[objectLocation].getObject()>=0)
		objects[m[objectLocation].getObject()].lsiOffset=cNULL;
	if (!foundMatch) return false;
	objectLocation=m[objectLocation].endObjectPosition;
	aliasLocation=m[aliasLocation].endObjectPosition;
	return true;
}

bool cSource::matchAlias(int where,int object, int aliasObject)
{ LFS
	if (object==aliasObject) return true;
	int al=objects[aliasObject].begin,ol=objects[object].begin;
	for (; ol<objects[object].end && al<objects[aliasObject].end; )
		if (!matchAliasLocation(where,ol,al)) return false;
	// at the end both objects must reach the end 12/5/2007
	return (ol==objects[object].end && al==objects[aliasObject].end);
}

// resolve grouped objects that depend on the speakers point-of-view
// these objects cannot wait for resolveFirstSecondPronouns because they 
// summon objects from other places that will be used subsequently
// “[julius:tuppence] He[tommy] gave me[julius] a call . Over the phone . 
// Told me[julius] to get a move on , and hustle . Said he[tommy] was trailing two crooks . ”
void cSource::resolveQuotedPOVObjects(int lastOpeningQuote,int lastClosingQuote)
{ LFS
	if (m[lastOpeningQuote].objectMatches.empty()) return; // no quoted point of view
	vector <cOM> objectMatches;
	for (int I=lastOpeningQuote; I<lastClosingQuote; I++)
	{
		objectMatches.clear();
		resolveMetaGroupTwo(I,true,objectMatches);
		if (objectMatches.size())
		{
			eliminateBodyObjectRedundancy(I,objectMatches);
			unMatchObjects(I,objectMatches,false);
			for (vector <cOM>::iterator om=objectMatches.begin(),omEnd=objectMatches.end(); om!=omEnd; om++)
			{
				// even if object is already on stack, update other fields associated with the localSpeaker.
				vector <cLocalFocus>::iterator lsi;
				if (!pushObjectIntoLocalFocus(I, om->object, false, false, true, false, L"MG (2)", lsi)) continue;
				// if cata speaker was invoked, an object may be matched by itself because it was already on the localObjects stack.
				if (m[I].getObject()!=om->object)
				{
					pushLocalObjectOntoMatches(I,lsi,L"MG (2)");
					if (objects[m[I].getObject()].objectClass==NAME_OBJECT_CLASS)
						lsi->om.salienceFactor=SALIENCE_THRESHOLD;
				}
			}
		}
	}
}

void cSource::setEmbeddedStorySpeaker(int where,int &lastDefiniteSpeaker)
{ LFS
	wstring tmpstr;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:impose embeddedStory speaker %d:%s",where,m[where].embeddedStorySpeakerPosition,
					(m[m[where].embeddedStorySpeakerPosition].objectMatches.empty()) ? 
					  objectString(m[m[where].embeddedStorySpeakerPosition].getObject(),tmpstr,true).c_str() : 
	          objectString(m[m[where].embeddedStorySpeakerPosition].objectMatches,tmpstr,true).c_str());
	previousSpeakers.clear();
	vector <cOM> objectMatches=m[m[where].embeddedStorySpeakerPosition].objectMatches;
	if (objectMatches.empty())
		objectMatches.push_back(cOM(m[m[where].embeddedStorySpeakerPosition].getObject(),SALIENCE_THRESHOLD));
	for (unsigned int I=0; I<objectMatches.size(); I++)
	{
		int o;
		previousSpeakers.push_back(o=objectMatches[I].object);
		objects[o].numIdentifiedAsSpeaker++;
		if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[o].getSubType()>=0)
			lplog(LOG_RESOLUTION,L"%06d:Removing place designation (11) from object %s.",where,objectString(o,tmpstr,false).c_str());
		objects[o].resetSubType(); // not a place
		objects[o].isNotAPlace=true;
		objects[o].numIdentifiedAsSpeakerInSection++;
		if (objects[o].numDefinitelyIdentifiedAsSpeakerInSection+objects[o].numIdentifiedAsSpeakerInSection==1 && section<sections.size())
			sections[section].speakerObjects.push_back(cOM(o,SALIENCE_THRESHOLD));
		vector <cLocalFocus>::iterator lsi=in(o);
		if (lsi!=localObjects.end())
			lsi->numIdentifiedAsSpeaker++;
	}
	lastDefiniteSpeaker=where; 
	setSpeakerMatchesFromPosition(where,m[where].objectMatches,m[where].embeddedStorySpeakerPosition,L"embeddedStorySpeaker",cWordMatch::flagEmbeddedStoryResolveSpeakers);
	// set NOT speaker to the rest of the speaker group
	vector <cLocalFocus>::iterator lsi;
  for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
    if (in(*si,m[where].objectMatches)==m[where].objectMatches.end() && (lsi=in(*si))!=localObjects.end() && lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent>=0)
		{
      m[where].audienceObjectMatches.push_back(cOM(*si,SALIENCE_THRESHOLD));
			objects[*si].speakerLocations.insert(where);
		}
	m[where].flags|=cWordMatch::flagAudienceFromSpeakerGroupResolveAudience;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION,L"%06d:Set last definite speaker to %d.",where,where);
}

void cSource::ageIntoNewSpeakerGroup(int where)
{ LFS
	speakerGroupTransition(where,currentSpeakerGroup,true);
  currentSpeakerGroup++;
	currentEmbeddedSpeakerGroup=-1;
	vector <cSpeakerGroup>::iterator sg=speakerGroups.begin()+currentSpeakerGroup;
	if (currentSpeakerGroup+1<speakerGroups.size() && eraseAliasesAndReplacementsInSpeakerGroup(sg,true) && sg->speakers.size()==1 && 
			// if a replacement occurs, reducing it from 2 to 1 (and so a non-self conversation is not possible)
			(sg+1)->speakers.size()>1 && (sg+1)->speakers.find(*sg->speakers.begin())!=(sg+1)->speakers.end() && sg->conversationalQuotes>1)
	{
		if (currentEmbeddedSpeakerGroup>=0) 
		{
			speakerGroups[currentSpeakerGroup+1].embeddedSpeakerGroups=speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:current embedded speaker group reset avoidance extension.",where);
		}
		(sg+1)->sgBegin=sg->sgBegin;
		(sg+1)->conversationalQuotes+=sg->conversationalQuotes;
		(sg+1)->dnSpeakers.insert(sg->dnSpeakers.begin(),sg->dnSpeakers.end());
		lplog(LOG_RESOLUTION,L"speakerGroup erased: %d",sg-speakerGroups.begin());
		int numSG = (int)(sg - speakerGroups.begin());
		for (auto &tsg : speakerGroups)
			if (tsg.previousSubsetSpeakerGroup >= numSG)
				tsg.previousSubsetSpeakerGroup--;
		for (cObject &o : objects)
		{
			if (o.getFirstSpeakerGroup() >= numSG)
				o.setFirstSpeakerGroup(o.getFirstSpeakerGroup() - 1);
			if (o.lastSpeakerGroup >= numSG)
				o.lastSpeakerGroup--;
		}
		speakerGroups.erase(sg);
	}
	speakerGroupTransition(where,currentSpeakerGroup,false);
  for (vector <cLocalFocus>::iterator lfi=localObjects.begin(); lfi!=localObjects.end(); )
  {
    if (find(speakerGroups[currentSpeakerGroup].speakers.begin(),speakerGroups[currentSpeakerGroup].speakers.end(),lfi->om.object)!=speakerGroups[currentSpeakerGroup].speakers.end())
    {
      lfi->resetAgeBeginSection(find(speakerGroups[currentSpeakerGroup].observers.begin(),speakerGroups[currentSpeakerGroup].observers.end(),lfi->om.object)!=speakerGroups[currentSpeakerGroup].observers.end());
      lfi++;
    }
    else if (objects[lfi->om.object].objectClass==NAME_OBJECT_CLASS && !lfi->numEncounters && !lfi->numIdentifiedAsSpeaker)
		{
			if (debugTrace.traceSpeakerResolution)
			{
				wstring tmpstr;
				lplog(LOG_RESOLUTION,L"%06d:Removed %s from localObjects",where,objectString(lfi->om.object,tmpstr,true).c_str());
			}
      lfi=localObjects.erase(lfi); // if NAME, ageSpeaker will never delete this, so if not in speakerGroup, then remove
		}
    else
		{
			// if the last location of this object was physically present and in the last paragraph and matched a meta group object, don't mark as non-present.
			// this is to correct a mistake in speaker groups, where meta group objects aren't processed more accurately.
			int beginEntirePosition=-1;
			if (lfi->lastWhere>=0)
			{
				beginEntirePosition=m[lfi->lastWhere].beginObjectPosition; // if this is an adjectival object 
				if (m[lfi->lastWhere].flags&cWordMatch::flagAdjectivalObject) 
					for (; beginEntirePosition>=0 && m[beginEntirePosition].principalWherePosition<0; beginEntirePosition--);
			}
			bool physicallyEvaluated=false;
			if (objects[lfi->om.object].objectClass==NAME_OBJECT_CLASS && (lfi->getTotalAge()>3 || lfi->lastWhere<0 ||
				  !physicallyPresentPosition(lfi->lastWhere,beginEntirePosition,physicallyEvaluated,false) || !physicallyEvaluated) &&
					// no physically present locations in the last speaker group were recorded
					(lfi->getTotalAge()>10 || (currentSpeakerGroup && ppLocationBetween(lfi->om.object,speakerGroups[currentSpeakerGroup-1].sgBegin,where)<0)))
			{
				if (debugTrace.traceSpeakerResolution)
				{
					wstring tmpstr;
					lplog(LOG_RESOLUTION,L"%06d:Made %d:%s [AGE:%d,%d] PP=%s PE=%s not physically present",where,
						lfi->lastWhere,objectString(lfi->om.object,tmpstr,true).c_str(),lfi->getTotalAge(),(currentSpeakerGroup>0) ? ppLocationBetween(lfi->om.object,speakerGroups[currentSpeakerGroup-1].sgBegin,where) : -1,
						 (physicallyPresentPosition(lfi->lastWhere,beginEntirePosition,physicallyEvaluated,false)) ? L"true" : L"false",
						 (physicallyEvaluated) ? L"true" : L"false");
				}
				lfi->physicallyPresent=false;
			}
			lfi->numEncounters=lfi->numIdentifiedAsSpeaker=lfi->numDefinitelyIdentifiedAsSpeaker=0;
			lfi++;
		}
	}
	for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
	{
		objects[*si].lastSpeakerGroup=currentSpeakerGroup;
		objects[*si].ageSinceLastSpeakerGroup=0; // not used in this section yet
	}
}

void cSource::resetObjects(void)
{ LFS
	// erase associated adjectives and nouns from possibly incorrectly matched objects
	wstring tmpstr;
	for (unsigned int o=0; o<objects.size(); o++)
	{
		if ((objects[o].male || objects[o].female) && !objects[o].eliminated && 
			  !objects[o].numIdentifiedAsSpeaker && !objects[o].numDefinitelyIdentifiedAsSpeaker && !objects[o].PISSubject && !objects[o].PISHail && !objects[o].PISDefinite &&
				objects[o].objectClass==NAME_OBJECT_CLASS && objects[o].name.hon==wNULL)
		{
			if (objects[o].getSubType()>=0)
			{
				objects[o].male=objects[o].female=false;
				objects[o].neuter=true;
				if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
					lplog(LOG_RESOLUTION,L"%06d:Making place %s neuter.",objects[o].originalLocation,objectString(o,tmpstr,false).c_str());
			}
			else if (m[objects[o].originalLocation].word->second.timeFlags&T_UNIT)
			{
				objects[o].male=objects[o].female=false;
				objects[o].neuter=true;
				if (debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution)
					lplog(LOG_RESOLUTION,L"%06d:Making time unit %s neuter.",objects[o].originalLocation,objectString(o,tmpstr,false).c_str());
			}
		}
		if ((debugTrace.traceSpeakerResolution || debugTrace.traceObjectResolution) && objects[o].genderNarrowed && objects[o].objectClass!=NON_GENDERED_NAME_OBJECT_CLASS)
			lplog(LOG_RESOLUTION,L"%06d:Unnarrowing %s [%d].",objects[o].originalLocation,objectString(o,tmpstr,true).c_str(),o);
		objects[o].clear(sourceType==WEB_SEARCH_SOURCE_TYPE || sourceType==WIKIPEDIA_SOURCE_TYPE || sourceType==INTERACTIVE_SOURCE_TYPE);
		addAssociatedNounsFromTitle(o);
	}
	for (int objectLastTense=0; objectLastTense<VERB_HISTORY; objectLastTense++)
		lastVerbTenses[objectLastTense].clear();
  section=0;
  usePIS=true;
  currentSpeakerGroup=0;
	currentTimelineSegment=0;
	currentEmbeddedSpeakerGroup=-1;
	currentEmbeddedTimelineSegment=-1;
	if (currentSpeakerGroup<speakerGroups.size())
		for (set <int>::iterator si=speakerGroups[currentSpeakerGroup].speakers.begin(),siEnd=speakerGroups[currentSpeakerGroup].speakers.end(); si!=siEnd; si++)
		{
			objects[*si].lastSpeakerGroup=currentSpeakerGroup;
			objects[*si].ageSinceLastSpeakerGroup=0; // not used in this section yet
		}
  unresolvedSpeakers.clear();
  localObjects.clear();
	subjectsInPreviousUnquotedSection.clear();
	// re-associate the objects - initialize the adjectives from the objects themselves
	vector <cWordMatch>::iterator im=m.begin(),imend=m.end();
	unsigned int I=0;
	for (; im!=imend; im++,I++)
	{
		// brushed past Julius - leads to Julius not being considered 'young'
		// if adjectives accumulate on names, then there is an opportunity for an adjective to accumulate that
		//   disagrees with other adjectives that, in names, accumulate in different ways than being accompanied
		//   directly with the noun (young John), as apposed to 'John was young'.
		int o=m[I].getObject();
		if (o>=0) 
		{
			accumulateAdjectives(I);
			if (objects[o].objectClass==NAME_OBJECT_CLASS && objects[o].associatedNouns.empty())
				addDefaultGenderedAssociatedNouns(o);
			objects[o].setGenericAge(m);
		}
    im->flags&=~cWordMatch::flagObjectResolved;
	}
}

bool cSource::setSecondaryQuoteString(int I,vector <int> &secondaryQuotesResolutions)
{ LFS
	bool definitelySpeaker=false,noTextBeforeOrAfter=false,noSpeakerAfterward=false;
	// ignore noSpeakerAfterward and return value for secondaryQuotes
	quotedString(lastOpeningSecondaryQuote,I,noTextBeforeOrAfter,noSpeakerAfterward);
  int audienceObjectPosition=-1,sentences=0,secondarySpeaker=determineSpeaker(lastOpeningSecondaryQuote,I,false,false,definitelySpeaker,audienceObjectPosition);
	if (secondarySpeaker<0)
		for (int J=lastOpeningSecondaryQuote+1; J<I && sentences<2; J++)
		{
			sentences+=(isEOS(J)) ? 1 : 0;
			int len;
			sentences+=(((m[J].pma.queryPattern(L"__S1",len)!=-1) || (m[J].pma.queryPattern(L"_Q2",len)!=-1)) && J+len<I) ? 2 : 0;  // sentence must be fully contained in secondary quote
		}
	#ifdef LOG_QUOTATIONS
		wstring str;
		for (int K=lastOpeningSecondaryQuote-5; K<I+5; K++)
			str+=m[K].word->first+L" ";
	#endif
	if ((secondarySpeaker>=0 || noTextBeforeOrAfter || sentences>=2) && (!noTextBeforeOrAfter || I-lastOpeningSecondaryQuote!=2))
  {
	  secondaryQuotesResolutions.push_back(lastOpeningSecondaryQuote);
	  secondaryQuotesResolutions.push_back(I);
	  secondaryQuotesResolutions.push_back(secondarySpeaker);
	  secondaryQuotesResolutions.push_back(audienceObjectPosition);
		#ifdef LOG_QUOTATIONS
			lplog(L"%d-%d NQS %s",lastOpeningSecondaryQuote,I,str.c_str());
		#endif
		return false;
  }
	else
	{
    m[lastOpeningSecondaryQuote].flags|=cWordMatch::flagQuotedString;
		#ifdef LOG_QUOTATIONS
			lplog(L"%d-%d QS %s",lastOpeningSecondaryQuote,I,str.c_str());
		#endif
		return true;
	}
}

bool cSource::replaceAliasesAndReplacements(set <int> &so)
{ LFS
	wstring tmpstr,tmpstr2,tmpstr3;
	int recur=0;
	bool replacementsMade=false;
	for (set <int>::iterator s=so.begin(); s!=so.end(); recur++)
	{
		if (recur>=50)
		{
			lplog(LOG_ERROR,L"Recursion hit on object %s (2)!",objectString(*s,tmpstr,true).c_str());
			objects[*s].replacedBy=-1;
			break;
		}
		bool erased=false;
		if (objects[*s].replacedBy>=0)
		{
			int tmp=*s;
			so.erase(s);
			followObjectChain(tmp);
			so.insert(tmp);
			s=so.begin();
			erased=true;
		}
		for (vector <int>::iterator alias=objects[*s].aliases.begin(),aliasEnd=objects[*s].aliases.end(); alias!=aliasEnd; alias++)
      if (so.find(*alias)!=so.end())
			{
				so.erase(*alias);
				s=so.begin();
				erased=true;
				break;
			}
    if (!erased) s++;
		else
			replacementsMade=true;
	}
	return replacementsMade;
}

bool cSource::replaceSubsequentMatches(set <int> &so,int sgEnd)
{ LFS
	wstring tmpstr,tmpstr2,tmpstr3;
	int recur=0;
	bool replacementsMade=false;
	set <int> replacedObjects;
	for (set <int>::iterator s=so.begin(); s!=so.end(); )
	{
		int at=atBefore(*s,sgEnd);
		if (at>=0 && m[at].objectMatches.size() && in(*s,m[at].objectMatches)==m[at].objectMatches.end() && 
			  replacedObjects.find(*s)==replacedObjects.end())
		{
			//lplog(LOG_ERROR,L"new: %d: replacing %s with %s",at,objectString(*s,tmpstr,true).c_str(),whereString(at,tmpstr2,true).c_str());
			if (recur++>=(signed)so.size()*2)
			{
				lplog(LOG_ERROR,L"%d:Recursion hit on object %s (3) - %d!",sgEnd,objectString(*s,tmpstr,true).c_str(),recur);
				objects[*s].replacedBy=-1;
				break;
			}
			replacedObjects.insert(*s);
			so.erase(s);
			bool rm=false;
			for (unsigned int omi=0; omi<m[at].objectMatches.size(); omi++)
			{
				if (so.find(m[at].objectMatches[omi].object)==so.end())
				{
					so.insert(m[at].objectMatches[omi].object);
					//lplog(LOG_ERROR,L"new: %d: Insertion on object %s (3) - %d!",at,objectString(m[at].objectMatches[omi].object,tmpstr,true).c_str(),recur);
					replacementsMade=rm=true;
				}
			}
			s=so.begin();
		}
		else
			s++;
	}
	return replacementsMade;
}

bool cSource::eraseAliasesAndReplacementsInSpeakerGroup(vector <cSpeakerGroup>::iterator sg,bool eraseSubsequentMatches)
{ LFS
	bool replacementsMade;
	if (replacementsMade=replaceAliasesAndReplacements(sg->speakers))
	{
		replaceAliasesAndReplacements(sg->groupedSpeakers);
		replaceAliasesAndReplacements(sg->singularSpeakers);
		replaceAliasesAndReplacements(sg->povSpeakers);
		replaceAliasesAndReplacements(sg->dnSpeakers);
	}
	if (eraseSubsequentMatches)
	{
		replaceSubsequentMatches(sg->speakers,sg->sgEnd);
		replaceSubsequentMatches(sg->groupedSpeakers,sg->sgEnd);
		replaceSubsequentMatches(sg->singularSpeakers,sg->sgEnd);
		replaceSubsequentMatches(sg->povSpeakers,sg->sgEnd);
		replaceSubsequentMatches(sg->dnSpeakers,sg->sgEnd);
	}
	if (sg->observers.size() && sg->speakers.size()<=2 && sg->conversationalQuotes)
	{
		sg->povSpeakers=sg->observers;
		sg->observers.clear();
		if (debugTrace.traceSpeakerResolution)
		{
			wstring tmpstr;
			lplog(LOG_RESOLUTION,L"%06d:Eliminated observers in sg %s (eraseAliasesAndReplacementsInSpeakerGroup)",sg->sgBegin,toText(*sg,tmpstr));
		}
	}
	return replacementsMade;
}

void cSource::eraseAliasesAndReplacementsInEmbeddedSpeakerGroups(void)
{ LFS
  for (vector <cSpeakerGroup>::iterator sg=speakerGroups.begin(),sgEnd=speakerGroups.end(); sg!=sgEnd; sg++)
	  for (vector <cSpeakerGroup>::iterator esg=sg->embeddedSpeakerGroups.begin(),esgEnd=sg->embeddedSpeakerGroups.end(); esg!=esgEnd; esg++)
			eraseAliasesAndReplacementsInSpeakerGroup(esg,false);
}

/*
letters
type 1
  BEGIN: start sentence: DEAR _NAME_ , EOS
	END: “[tuppence:tommy] _NAME_ . ”
type 2
BEGIN: a sentence mentioning a book, telegram, note, wire, letter etc as an object.
END: a sentence ending in a dash and a _NAME_


10497: SPEAKER: letter: 
	BEGIN: “[tommy:tuppence] DEAR SIR , ” 
	END: Yours truly , “[tommy:tuppence] A . CARTER . ”
10750: letter
	BEGIN: “[tuppence:tommy] DEAR SIR[mr] , ” 
	END: Yours truly , “[tuppence:tommy] JULIUS P . HERSHEIMMER . ” 
27263: SPEAKER: another letter - completely not resolved correctly.
	BEGIN: “[julius:tuppence] DEAR MISS TUPPENCE , ” 
	END:  Your sincere friend[boris] , “[tuppence:julius] MR . CARTER . ”
64805: SPEAKER: letter
	BEGIN: “Unresolved[tommy,henry:julius] DEAR JULIUS , ” 
	END: Yours affectionately , “Unresolved[tommy,henry:julius] TUPPENCE . ”
65147: TYPE 2 SPEAKER: letter (telegram) from Tuppence to Tommy
	BEGIN:  ESTABTommy disentangled it[room,ball] and smoothed out the telegram . 
	END: Come at once , Moat House , Ebury , Yorkshire , great developments -- TOMMY . ” 
67862: TYPE 2 letter
	BEGIN:  Later in the day[day] MOVETommy received a wire : 
	END: “Unresolved[tommy] Join me[tommy] Manchester Midland Hotel . Important news -- JULIUS . ” 
67974: TYPE 2 letter
	BEGIN: MOVEHe[julius] handed the telegraph form to the other[julius] . Tommy's eyes[tommy] opened as he[tommy,julius] read : 
	END: “UnresolvedES [tommy] Jane Finn found . Come Manchester Midland Hotel immediately -- PEEL EDGERTON . ” 
77211: another letter
	BEGIN: “[prime minister:mr] DEAR MR . CARTER , ” 
	END: Yours , etc . , “[mr:prime minister] THOMAS BERESFORD . ” 
79192: another letter
  BEGIN: “[julius:julius] DEAR HERSHEIMMER , ” 
	END: Yours , “[julius:julius] TOMMY BERESFORD . ” 
81645: another letter
  BEGIN: “[tommy:carter] DEAR TOMMY[carter] , ” 
	END: Yours , “Unresolved[tommy:tommy,carter] TWOPENCE . ” 

*/

bool cSource::detectLetterAsObject(int where,int &lastLetterBegin)
{ LFS
	wchar_t *letterWords[]={ L"letter",L"note",L"telegram",L"wire",L"book",L"missive", NULL };
	wstring word=m[where].word->first;
	for (int I=0; letterWords[I]; I++)
		if (word==letterWords[I])
		{
			lastLetterBegin=lastOpeningPrimaryQuote;
			if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
			{
				wstring tmpstr2;
				lplog(LOG_RESOLUTION,L"%06d:letter (1) detected possible mention at lastLetterBegin=%d",where,lastLetterBegin);
			}
			return true;
		}
	return false;
}

//  BEGIN: “[tommy:carter] DEAR TOMMY[carter] , ” 
void createLetterIntroPatterns(void)
{ LFS
  //cPattern *p=NULL;
  cPattern::create(L"_LETTER_BEGIN{_IGNORE}",L"1",
    1,L"quotes",OPEN_INFLECTION,1,1,
		2,L"adjective|dear",L"adjective|dearest",0,1,1,
		1,L"_NAME{NAME_PRIMARY}",0,1,1,
		2,L",",L":",0,0,1,
    1,L"quotes",CLOSE_INFLECTION,1,1,
    0);
}

// called at an opening primary quote
//  BEGIN: “[tommy:carter] DEAR TOMMY[carter] , ” 
// also can be set if the speaker verb is "read"
int cSource::letterDetectionBegin(int where,int &whereLetterTo,int &lastLetterBegin)
{ LFS
	if (m[where].word->first!=L"“") return -1;
	if (m[where].speakerPosition>=0 && m[m[where].speakerPosition].getRelVerb()>=0)
	{
		wchar_t *readWords[]={ L"read", NULL };
		wstring verb=m[m[m[where].speakerPosition].getRelVerb()].word->first;
		for (int I=0; readWords[I]; I++)
			if (verb==readWords[I])
			{
				whereLetterTo=0;
				lastLetterBegin=where;
				if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
					lplog(LOG_RESOLUTION,L"%06d:letter detection begin (2) detected beginning at lastLetterBegin=%d",where,lastLetterBegin);
				break;
			}
	}
	int element=-1,maxLen=-1;
  vector < vector <cTagLocation> > tagSets;
	if (where<(signed)m.size() && (element=m[where].pma.queryPattern(L"_LETTER_BEGIN",maxLen))!=-1 && 
		  startCollectTags(true,metaSpeakerTagSet,where,m[where].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd,tagSets,true,true,L"letter detection")>0)
	{
    for (unsigned int J=0; J<tagSets.size(); J++)
    {
      if (debugTrace.traceNameResolution)
        printTagSet(LOG_RESOLUTION,L"LDB",J,tagSets[J],where,m[where].pma[element&~cMatchElement::patternFlag].pemaByPatternEnd);
			int primaryTag;
			int wherePrimary=tagSets[J][primaryTag=findOneTag(tagSets[J],L"NAME_PRIMARY",-1)].sourcePosition;
			if (tagSets[J][primaryTag].len>1 && m[wherePrimary].principalWherePosition>=0) // make sure to bypass any adjectives
				wherePrimary=m[wherePrimary].principalWherePosition;
			int oc=(m[wherePrimary].getObject()>=0) ? objects[m[wherePrimary].getObject()].objectClass : -1;
			// must be a name or gendered object
			if (oc==NAME_OBJECT_CLASS || oc==GENDERED_GENERAL_OBJECT_CLASS || oc==BODY_OBJECT_CLASS || oc==GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || 
					oc==GENDERED_DEMONYM_OBJECT_CLASS || oc==META_GROUP_OBJECT_CLASS || oc==GENDERED_RELATIVE_OBJECT_CLASS)
			{
				lastLetterBegin=where;
				wstring tmpstr;
				if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
					lplog(LOG_RESOLUTION,L"%06d:letter detection begin (3) detected beginning at lastLetterBegin=%d written to object %s",where,lastLetterBegin,whereString(wherePrimary,tmpstr,true).c_str());
				return whereLetterTo=wherePrimary;
			}
    }
	}
	return -1;
}

bool cSource::letterDetectionEnd(int where,int whereLetterTo,int lastLetterBegin)
{ LFS
	int endType=-1;
	// 	END: Yours , “Unresolved[tommy:tommy,carter] TWOPENCE . ” 
	// Sincerely, Tommy 
	if (whereLetterTo<0)
	{
		wchar_t *letterEndWords[]={ L"yours",L"sincerely",L"love",L"always", NULL };
		wstring word=m[where].word->first;
		for (int I=0; letterEndWords[I]; I++)
			if (word==letterEndWords[I])
			{
				if (m[where+1].word->first==L",")
				{
					whereLetterTo=0;
					endType=2;
					if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
						lplog(LOG_RESOLUTION,L"%06d:letter detection end (1) detected end at %d",where,where);
				}
				break;
			}
	}
	int maxLen=-1,setSpeakers=-1; // element=-1,
	if (whereLetterTo>=0 && m[where].word->first==L"“" && m[where+1].pma.queryPattern(L"_NAME",maxLen)!=-1 && 
		  ((m[where+1+maxLen].word->first==L"." && m[where+2+maxLen].word->first==L"”") || m[where+1+maxLen].word->first==L"”"))
	{
		endType=3;
		setSpeakers=where+1;
	}
	// twopence is not a recognized name (it was not seen before or after in the text)
	// 	END: Yours , “Unresolved[tommy:tommy,carter] TWOPENCE . ” 
	if (whereLetterTo>=0 && m[where].word->first==L"“" && (m[where+1].flags&cWordMatch::flagAllCaps) &&
		  ((m[where+2].word->first==L"." && m[where+3].word->first==L"”") || m[where+2].word->first==L"”"))
	{
		// leads to doubles - future investigation
		//if (m[where+1].getObject()>=0)
		//{
		//	objects[m[where+1].getObject()].objectClass=NAME_OBJECT_CLASS;
		//	objects[m[where+1].getObject()].name.any=m[where+1].word;
		//}
		endType=4;
		setSpeakers=where+1;
	}
	// END: Come at once , Moat House , Ebury , Yorkshire , great developments -- TOMMY . ” 
	if (setSpeakers<0 && (m[where].objectRole&IN_PRIMARY_QUOTE_ROLE) && m[where].queryWinnerForm(dashForm)>=0 && where+1<(signed)m.size() && m[where+1].pma.queryPattern(L"_NAME",maxLen)!=-1)
	{
		endType=5;
		setSpeakers=where+1;
	}
	if (setSpeakers>=0 && whereLetterTo<0 && lastLetterBegin<0)
	{
		// search for local recent physically present 'letter' object (physicallyPresent, not inQuotes, age<3)
		for (vector <cLocalFocus>::iterator lsi=localObjects.begin(),lsiEnd=localObjects.end(); lsi!=lsiEnd; lsi++)
			if (lsi->occurredOutsidePrimaryQuote && lsi->getTotalAge()<3 && lsi->physicallyPresent && 
					objects[lsi->om.object].neuter && detectLetterAsObject(objects[lsi->om.object].originalLocation,lastLetterBegin))
				break;
	}
	if (setSpeakers>=0 && (lastLetterBegin>=0 || whereLetterTo>=0 || endType==2))
	{
		int letterFrom=m[where+1].getObject(),letterTo=-1;
		// Your sincere friend, Mr. Carter
		int closingResolution=where-1;
		if (m[closingResolution].word->first==L"“") closingResolution--;
		if (m[closingResolution].word->first==L"," && m[closingResolution-1].queryForm(friendForm)>=0 && 
			  m[closingResolution-1].getObject()>=0 && m[closingResolution-1].objectMatches.size()>0)
		{
			vector <int> restrictObjects;
			unMatchObjects(closingResolution-1,restrictObjects,false);
      objects[letterFrom].locations.push_back(closingResolution-1);
			m[closingResolution-1].objectMatches.push_back(cOM(letterFrom,SALIENCE_THRESHOLD));
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"%06d:letter detection closing salutation rematched",where);
		}
		if (m[where+1].objectMatches.size())
			letterFrom=m[where+1].objectMatches[0].object;
		if (letterFrom>=0 && objects[letterFrom].getSubType()>=0) 
		{
			wstring tmpstr,tmpstr2;
			if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
				lplog(LOG_RESOLUTION,L"%06d:letter detection end (%d) rejected end %d-%d: %d:%s//%d:%s",
							where,endType,lastLetterBegin,where,where+1,objectString(letterFrom,tmpstr,true).c_str(),whereLetterTo,objectString(letterTo,tmpstr2,true).c_str());
			return false;
		}
		if (whereLetterTo>0)
		{
			letterTo=m[whereLetterTo].getObject();
			if (m[whereLetterTo].objectMatches.size() && m[lastLetterBegin].objectMatches.size())
			{
				m[whereLetterTo].objectMatches.clear();
				letterTo=m[lastLetterBegin].objectMatches[0].object;
				m[whereLetterTo].objectMatches.push_back(cOM(letterTo,SALIENCE_THRESHOLD));
			}
		}
		bool toAllSpeakers=whereLetterTo>=0 && (m[whereLetterTo].objectMatches.empty() && (m[whereLetterTo].word->second.flags&cSourceWordInfo::genericGenderIgnoreMatch) &&
			  currentSpeakerGroup<speakerGroups.size() && letterTo==m[whereLetterTo].getObject());
		wstring tmpstr,tmpstr2;
		if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
			lplog(LOG_RESOLUTION,L"%06d:letter detection end (%d) detected end %d-%d: %d:%s//%d:%s",
		        where,endType,lastLetterBegin,where,where+1,objectString(letterFrom,tmpstr,true).c_str(),whereLetterTo,objectString(letterTo,tmpstr2,true).c_str());
		if (letterFrom>=0 || letterTo>=0)
			for (int letterQuote=lastLetterBegin; letterQuote<=where && letterQuote>=0; letterQuote=m[letterQuote].nextQuote)
			{
				for (int linkQuote=letterQuote; linkQuote>=0; linkQuote=m[linkQuote].getQuoteForwardLink())
				{
					if (letterFrom>=0)
					{
						m[linkQuote].objectMatches.clear();
						m[linkQuote].objectMatches.push_back(cOM(letterFrom,SALIENCE_THRESHOLD));
					}
					if (letterTo>=0)
					{
						m[linkQuote].audienceObjectMatches.clear();
						vector <cLocalFocus>::iterator lsi;
						if (!toAllSpeakers)
							m[linkQuote].audienceObjectMatches.push_back(cOM(letterTo,SALIENCE_THRESHOLD));
						else
							for (set <int>::iterator s=speakerGroups[currentSpeakerGroup].speakers.begin(); s!=speakerGroups[currentSpeakerGroup].speakers.end(); s++)
							{
								if ((lsi=in(*s))!=localObjects.end() && lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent<lastLetterBegin)
									m[linkQuote].audienceObjectMatches.push_back(cOM(*s,SALIENCE_THRESHOLD));
							}
					}
					if (debugTrace.traceNameResolution || debugTrace.traceObjectResolution)
						lplog(LOG_RESOLUTION,L"%06d:matches set@%d from %d",where,linkQuote,letterQuote);
					m[linkQuote].flags|=cWordMatch::flagDefiniteResolveSpeakers|cWordMatch::flagSpecifiedResolveAudience;
				}
			}
		return true;
	}
	return false;
}

// "27 Carshalton Gardens," said Tuppence, referring to the address.
// [NO] 'Pray, madam,' said Mr Dorrit, referring to the handbill again,
// [NO] 'She did that all right,' he said, referring to the chair.
// "Will he let me?" asked the young fiddler, again referring to the teacher.
// "He is certainly a strange man," said she, referring to the surgeon.
// "Now what will you do with them?" asked Vi, referring to the letters.
// "Well, if he is a handy boy like that," said Aunt Jo, referring to the colored boy
// "Is that bill real?" asked one boy, referring to the money.
// "Such big ones!" Nan exclaimed, referring to the luscious red strawberries
// [NO]"I think I fell asleep," said he, referring to the accident.
// "It's in the guide-book," said Golenishtchev, referring to the palazzo Vronsky had taken.
// "But he must be a Freemason," said he, referring to the abbe whom he had met that evening.
void cSource::resolveMetaReference(int speakerPosition,int quotePosition,int lastBeginS1,int lastRelativePhrase,int lastQ2,int lastVerb)
{ LFS
	if (speakerPosition+6>=(signed)m.size() || m[speakerPosition+1].word->first!=L"," || m[speakerPosition+2].word->first!=L"referring" || 
		  m[speakerPosition+3].word->first!=L"to" || m[speakerPosition+4].word->first!=L"the") return;
	if (m[speakerPosition+4].principalWherePosition<0 || m[m[speakerPosition+4].principalWherePosition].getObject()<0) return;
	int whereReferredToObject=m[speakerPosition+4].principalWherePosition,referredToObject,whereAddObject;
	m[whereReferredToObject].flags&=~cWordMatch::flagObjectResolved;
	resolveObject(whereReferredToObject,true,true,false,lastBeginS1,lastRelativePhrase,lastQ2,lastVerb,false,true,false);
	if (m[whereReferredToObject].objectMatches.size()>0)
		referredToObject=m[whereReferredToObject].objectMatches[0].object;
	else
		referredToObject=m[whereReferredToObject].getObject();
	// if a single noun between quotes
	int addObject;
	if (m[whereAddObject=quotePosition+1].principalWherePosition>=0)
		whereAddObject=m[quotePosition+1].principalWherePosition;
	if (m[whereAddObject].getObject()>=0 && m[m[whereAddObject].endObjectPosition].word->first==L"," && 
		m[m[whereAddObject].endObjectPosition+1].queryForm(quoteForm)>=0)
	{
		if (m[whereAddObject].objectMatches.size()>0)
			addObject=m[whereAddObject].objectMatches[0].object;
		else
			addObject=m[whereAddObject].getObject();
		// match addObject [27 Carshalton Gardens] against [the address]
		if (m[whereReferredToObject].objectMatches.size()>1)
		{
			m[whereReferredToObject].objectMatches.push_back(cOM(addObject,SALIENCE_THRESHOLD));
			objects[addObject].locations.push_back(cObject::cLocation(whereReferredToObject));
			objects[addObject].updateFirstLocation(whereReferredToObject);
			return;
		}
	}
	else
	{
		// "Will he let me?" asked the young fiddler, again referring to the teacher.
		// "Now what will you do with them?" asked Vi, referring to the letters.
		// scan between quotes (which must only be one sentence in length) for subject and object
		int whereSubject=-1,whereObject=-1;
		for (int I=quotePosition; I<m[quotePosition].endQuote; I++)
		{
			if ((m[I].objectRole&SUBJECT_ROLE) && m[I].getObject()>=0 && m[I].getRelVerb()>=0)
			{
				if (whereSubject>=0) return; // ambiguous
				whereSubject=I;
			}
			if ((m[I].objectRole&OBJECT_ROLE) && m[I].getObject()>=0 && m[I].getRelVerb()>=0 && m[I].relSubject==whereSubject)
			{
				if (whereObject>=0) return; // ambiguous
				whereObject=I;
			}
		}
		if (whereSubject<0 && whereObject>=0)
			whereAddObject=whereObject;
		else if (whereSubject>=0 && whereObject<0)
			whereAddObject=whereSubject;
		else if (whereSubject>=0 && whereObject>=0)
		{
			if (m[whereSubject].objectMatches.size()>0)
				addObject=m[whereSubject].objectMatches[0].object;
			else
				addObject=m[whereSubject].getObject();
			bool subjectMatches=objects[referredToObject].matchGenderIncludingNeuter(objects[addObject]);
			if (m[whereObject].objectMatches.size()>0)
				addObject=m[whereObject].objectMatches[0].object;
			else
				addObject=m[whereObject].getObject();
			bool objectMatches=objects[referredToObject].matchGenderIncludingNeuter(objects[addObject]);
			if (subjectMatches && objectMatches) return; // ambiguous
			if (subjectMatches) whereAddObject=whereSubject;
			else if (objectMatches) whereAddObject=whereObject;
			else return;
		}
		else
			return;
		if (m[whereAddObject].objectMatches.size()>0)
			addObject=m[whereAddObject].objectMatches[0].object;
		else
			addObject=m[whereAddObject].getObject();
	}
	if ((objects[addObject].objectClass==NAME_OBJECT_CLASS || objects[addObject].objectClass==NON_GENDERED_NAME_OBJECT_CLASS) && 
		  objects[referredToObject].objectClass==NON_GENDERED_GENERAL_OBJECT_CLASS)
	{
		replaceObjectInSection(speakerPosition,addObject,referredToObject,L"resolveMetaReference");
	}
	else if (objects[referredToObject].objectClass==PRONOUN_OBJECT_CLASS || 
		       objects[referredToObject].objectClass==REFLEXIVE_PRONOUN_OBJECT_CLASS || 
					 objects[referredToObject].objectClass==RECIPROCAL_PRONOUN_OBJECT_CLASS)
	{
		m[whereReferredToObject].objectMatches.push_back(cOM(addObject,SALIENCE_THRESHOLD));
		objects[addObject].locations.push_back(cObject::cLocation(whereReferredToObject));
		objects[addObject].updateFirstLocation(whereReferredToObject);
	}
	else
		objects[referredToObject].aliases.push_back(addObject);
}

// this routine prints the most important resolutions first (speakerGroup, gendered, then everything else)
void cSource::printResolutionCheck(vector <int> &badSpeakers)
{
	LFS
		wstring tmpstr1, tmpstr2, tmpstr3;
	vector <cWordMatch>::iterator im = m.begin(), imend = m.end();
	remove("rescheck.lplog");
	for (currentSpeakerGroup = 0; currentSpeakerGroup < speakerGroups.size(); currentSpeakerGroup++)
	{
		if (m[speakerGroups[currentSpeakerGroup].sgBegin].t.traceSpeakerResolution || (speakerGroups[currentSpeakerGroup].sgEnd > 0 && m[speakerGroups[currentSpeakerGroup].sgEnd - 1].t.traceSpeakerResolution))
			lplog(LOG_RESCHECK, L"%06d:%s", speakerGroups[currentSpeakerGroup].sgBegin, objectString(speakerGroups[currentSpeakerGroup].speakers, tmpstr1).c_str());
	}
	currentSpeakerGroup = 0;
	currentTimelineSegment = 0;
	currentEmbeddedSpeakerGroup = -1;
	currentEmbeddedTimelineSegment = -1;
	// print only non-neuter and non-quantifier resolutions
	for (int I = 0; im != imend; im++, I++)
	{
		if (m[I].t.traceSpeakerResolution)
		{
			if ((m[I].getObject() >= 0 && objects[m[I].getObject()].neuter && !objects[m[I].getObject()].male && !objects[m[I].getObject()].female) ||
				m[I].queryWinnerForm(quantifierForm) >= 0)
				continue;
			if (m[I].getObject() != -1)
			{
				if (m[I].objectMatches.size())
					lplog(LOG_RESCHECK, L"%06d:%s (%s)", I, objectString(m[I].getObject(), tmpstr1, true).c_str(), objectSortedString(m[I].objectMatches, tmpstr2).c_str());
				else
					lplog(LOG_RESCHECK, L"%06d:%s", I, objectString(m[I].getObject(), tmpstr1, true).c_str());
			}
			else if (m[I].objectMatches.size() || m[I].audienceObjectMatches.size())
				lplog(LOG_RESCHECK, L"%06d:(%s//%s)", I, objectSortedString(m[I].objectMatches, tmpstr2).c_str(), objectSortedString(m[I].audienceObjectMatches, tmpstr3).c_str());
			if (m[I].word->first == L"“" && !(m[I].flags&cWordMatch::flagQuotedString))
			{
				if (m[I].objectMatches.size() != 1)
					lplog(LOG_RESCHECK, L"%06d:Invalid Speaker", I);
				if (m[I].audienceObjectMatches.size() == 0 || (m[I].audienceObjectMatches.size() >= speakerGroups[currentSpeakerGroup].speakers.size() && speakerGroups[currentSpeakerGroup].speakers.size() > 1))
					lplog(LOG_RESCHECK, L"%06d:Invalid Audience", I);
				if (m[I].objectMatches.size() != 1 || m[I].audienceObjectMatches.size() == 0 || (m[I].audienceObjectMatches.size() >= speakerGroups[currentSpeakerGroup].speakers.size() && speakerGroups[currentSpeakerGroup].speakers.size() > 1))
					badSpeakers.push_back(I);
			}
		}
		while (currentSpeakerGroup + 1 < speakerGroups.size() && speakerGroups[currentSpeakerGroup + 1].sgBegin == I) currentSpeakerGroup++;
	}
	// print only neuter and quantifier resolutions
	im = m.begin();
	for (int I = 0; im != imend; im++, I++)
	{
		if (m[I].t.traceSpeakerResolution)
		{
			if ((m[I].getObject() >= 0 && !objects[m[I].getObject()].container &&
				objects[m[I].getObject()].neuter && !objects[m[I].getObject()].male && !objects[m[I].getObject()].female) ||
				m[I].queryWinnerForm(quantifierForm) >= 0)
			{
				if (m[I].objectMatches.size())
					lplog(LOG_RESCHECK, L"%d:%s (%s)", I, objectString(m[I].getObject(), tmpstr1, true).c_str(), objectSortedString(m[I].objectMatches, tmpstr2).c_str());
				else if (m[I].getObject() != -1)
					lplog(LOG_RESCHECK, L"%d:%s", I, objectString(m[I].getObject(), tmpstr1, true).c_str());
			}
		}
	}
}

void cSource::processEndOfPrimaryQuoteRS(int where, int lastSentenceEndBeforeAndNotIncludingCurrentQuote,
	int lastBeginS1, int lastRelativePhrase, int lastQ2, int lastVerb, int &lastQuotedString, unsigned int &quotedObjectCounter,int &lastDefiniteSpeaker,int &lastClosingPrimaryQuote,
	int &paragraphsSinceLastSubjectWasSet,int wherePreviousLastSubjects, 
	bool &inPrimaryQuote, bool &immediatelyAfterEndOfParagraph, bool &quotesSeenSinceLastSentence,bool &previousSpeakersUncertain,
	vector <int> &previousLastSubjects)
{
	inPrimaryQuote = false;
	lastClosingPrimaryQuote = where;
	wstring tmpstr,tmpstr2;
	// if the previous end quote was inserted, and this end quote was also inserted, then this quote has the same speakers as the last
	bool previousEndQuoteInserted = previousPrimaryQuote >= 0 && m[previousPrimaryQuote].endQuote >= 0 && (m[m[previousPrimaryQuote].endQuote].flags&cWordMatch::flagInsertedQuote) != 0 && (m[where].flags&cWordMatch::flagInsertedQuote) != 0;
	// immediatelyAfterEndOfParagraph is set if there has been an end of paragraph seen since the last close quote.
	bool noTextBeforeOrAfter, quotedStringSeen = false, multipleQuoteInSentence = (previousPrimaryQuote > lastSentenceEndBeforeAndNotIncludingCurrentQuote), noSpeakerAfterward = false, audienceInSubQuote = false, audienceFromSpeakerGroup = false;
	if (!(quotedStringSeen = quotedString(lastOpeningPrimaryQuote, where, noTextBeforeOrAfter, noSpeakerAfterward)))
	{
		if (!(m[lastOpeningPrimaryQuote].flags&cWordMatch::flagQuotedString) &&
			(multipleQuoteInSentence || !immediatelyAfterEndOfParagraph || previousEndQuoteInserted))
		{
			addSpeakerObjects(lastOpeningPrimaryQuote, true, lastOpeningPrimaryQuote, previousSpeakers, m[lastOpeningPrimaryQuote].flags | cWordMatch::flagForwardLinkResolveSpeakers);
			if (previousPrimaryQuote >= 0)
			{
				m[lastOpeningPrimaryQuote].audienceObjectMatches = m[previousPrimaryQuote].audienceObjectMatches;
				bool allIn, oneIn;
				// (CMREADME34)
				// scan quote for audience - if supposed audience is included in the quote, then reject
				for (int ai = lastOpeningPrimaryQuote; ai < m[lastOpeningPrimaryQuote].endQuote; ai++)
					if (m[ai].getObject() >= 0 && !(m[ai].objectRole&(HAIL_ROLE | PP_OBJECT_ROLE | IN_QUOTE_REFERRING_AUDIENCE_ROLE)) && (m[ai].objectRole&FOCUS_EVALUATED) &&
						intersect(ai, m[lastOpeningPrimaryQuote].audienceObjectMatches, allIn, oneIn) &&
						(m[where].queryForm(pronounForm) >= 0 || m[where].queryForm(indefinitePronounForm) >= 0)) // invalidObject
					{
						vector <cOM> audienceObjectMatches = m[lastOpeningPrimaryQuote].audienceObjectMatches;
						m[lastOpeningPrimaryQuote].audienceObjectMatches.clear();
						vector <cLocalFocus>::iterator lsi;
						for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].speakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].speakers.end(); si != siEnd; si++)
							if (in(*si, m[lastOpeningPrimaryQuote].objectMatches) == m[lastOpeningPrimaryQuote].objectMatches.end() && *si != m[ai].getObject() &&
								(lsi = in(*si)) != localObjects.end() && lsi->physicallyPresent && lsi->whereBecamePhysicallyPresent >= 0)
								m[lastOpeningPrimaryQuote].audienceObjectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, L"%06d:audience speaker of %s corrected to %s [audience appears in quote@%d].", lastOpeningPrimaryQuote, objectString(audienceObjectMatches, tmpstr, true).c_str(), objectString(m[lastOpeningPrimaryQuote].audienceObjectMatches, tmpstr2, true).c_str(), ai);
						m[lastOpeningPrimaryQuote].flags |= cWordMatch::flagSpecifiedResolveAudience;
						for (unsigned int J = 0; J < m[lastOpeningPrimaryQuote].audienceObjectMatches.size(); J++)
							objects[m[lastOpeningPrimaryQuote].audienceObjectMatches[J].object].speakerLocations.insert(lastOpeningPrimaryQuote);
						if (!(m[previousPrimaryQuote].flags&cWordMatch::flagSpecifiedResolveAudience) &&
							!intersect(lastOpeningPrimaryQuote, m[previousPrimaryQuote].audienceObjectMatches, allIn, oneIn))
						{
							if (debugTrace.traceSpeakerResolution)
								lplog(LOG_RESOLUTION, L"%06d:original audience %s corrected [audience appears in forwardLink quote].", previousPrimaryQuote, objectString(m[previousPrimaryQuote].audienceObjectMatches, tmpstr, true).c_str());
							m[previousPrimaryQuote].audienceObjectMatches = m[lastOpeningPrimaryQuote].audienceObjectMatches;
							m[previousPrimaryQuote].flags |= cWordMatch::flagSpecifiedResolveAudience;
							for (vector <cOM>::iterator aomi = m[previousPrimaryQuote].audienceObjectMatches.begin(), aomiEnd = m[previousPrimaryQuote].audienceObjectMatches.end(); aomi != aomiEnd; aomi++)
								objects[aomi->object].speakerLocations.insert(previousPrimaryQuote);
						}
						break;
					}
				// (CMREADME35)
				// possible different person talking to?
				if (!immediatelyAfterEndOfParagraph)
				{
					int audienceObjectPosition = scanForSpeakers(lastOpeningPrimaryQuote, where, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, HAIL_ROLE | IN_QUOTE_REFERRING_AUDIENCE_ROLE);
					bool definitelySpeaker;
					int speakerPosition = determineSpeaker(lastOpeningPrimaryQuote, where, true, noSpeakerAfterward, definitelySpeaker, audienceObjectPosition), lateSpeaker = -1;
					// the boots of Albert
					if (speakerPosition >= 0 && m[speakerPosition].getObject() >= 0 && objects[m[speakerPosition].getObject()].objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
						objects[m[speakerPosition].getObject()].getOwnerWhere() >= 0 && m[objects[m[speakerPosition].getObject()].getOwnerWhere()].getObject() >= 0 &&
						(objects[m[objects[m[speakerPosition].getObject()].getOwnerWhere()].getObject()].male || objects[m[objects[m[speakerPosition].getObject()].getOwnerWhere()].getObject()].female))
						speakerPosition = objects[m[speakerPosition].getObject()].getOwnerWhere();
					if (speakerPosition < 0)
						speakerPosition = scanForSpeakers(lastOpeningPrimaryQuote, where, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE);
					resolveMetaReference(speakerPosition, lastOpeningPrimaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb);
					if (speakerPosition >= 0 && previousSpeakers.size() > 1 &&
						((m[speakerPosition].objectMatches.empty() && find(previousSpeakers.begin(), previousSpeakers.end(), lateSpeaker = m[speakerPosition].getObject()) != previousSpeakers.end()) ||
						(m[speakerPosition].objectMatches.size() == 1 && find(previousSpeakers.begin(), previousSpeakers.end(), lateSpeaker = m[speakerPosition].objectMatches[0].object) != previousSpeakers.end())))
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, L"%06d:late speaker of %s clarifies original speaker(s) %s.", where, objectString(lateSpeaker, tmpstr, true).c_str(), objectString(m[lastOpeningPrimaryQuote].objectMatches, tmpstr2, true).c_str());
						m[lastOpeningPrimaryQuote].objectMatches.clear();
						m[lastOpeningPrimaryQuote].objectMatches.push_back(cOM(lateSpeaker, SALIENCE_THRESHOLD));
						for (unsigned int J = 0; J < m[lastOpeningPrimaryQuote].objectMatches.size(); J++)
							objects[m[lastOpeningPrimaryQuote].objectMatches[J].object].speakerLocations.insert(lastOpeningPrimaryQuote);
					}
					else if (lateSpeaker >= 0)
					{
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, L"%06d:late speaker of %s conflicts with original speaker(s) %s.", where, objectString(lateSpeaker, tmpstr, true).c_str(), objectString(m[lastOpeningPrimaryQuote].objectMatches, tmpstr2, true).c_str());
					}
					if (audienceObjectPosition >= 0 && m[audienceObjectPosition].getObject() >= 0)
					{
						audienceInSubQuote = true;
						resolveObject(audienceObjectPosition, false, false, false, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, false, false, false);
						setSpeakerMatchesFromPosition(lastOpeningPrimaryQuote, m[lastOpeningPrimaryQuote].audienceObjectMatches, audienceObjectPosition, L"audienceObjectPositionSubQuote(2)", cWordMatch::flagSpecifiedResolveAudience);
						subtract(m[audienceObjectPosition].getObject(), m[lastOpeningPrimaryQuote].objectMatches);
						if (intersect(m[lastOpeningPrimaryQuote].audienceObjectMatches, m[lastOpeningPrimaryQuote].objectMatches, allIn, oneIn) && !allIn)
							subtract(m[lastOpeningPrimaryQuote].audienceObjectMatches, m[lastOpeningPrimaryQuote].objectMatches);
					}
					// if audience not found, and previous quote had an audienceobject where the subject was talking to himself,
					// and speaker is found, then cancel the assumption that the speaker is talking to himself.
					else if (speakerPosition >= 0 && m[previousPrimaryQuote].audienceObjectMatches.size() && m[previousPrimaryQuote].objectMatches.size() &&
						m[previousPrimaryQuote].audienceObjectMatches[0].object == m[previousPrimaryQuote].objectMatches[0].object && currentSpeakerGroup < speakerGroups.size())
					{
						audienceFromSpeakerGroup = true;
						m[lastOpeningPrimaryQuote].audienceObjectMatches.clear();
						for (set <int>::iterator si = speakerGroups[currentSpeakerGroup].speakers.begin(), siEnd = speakerGroups[currentSpeakerGroup].speakers.end(); si != siEnd; si++)
							if (in(*si, m[previousPrimaryQuote].objectMatches) == m[previousPrimaryQuote].objectMatches.end())
								m[lastOpeningPrimaryQuote].audienceObjectMatches.push_back(cOM(*si, SALIENCE_THRESHOLD));
						m[lastOpeningPrimaryQuote].flags |= cWordMatch::flagAudienceFromSpeakerGroupResolveAudience;
					}
					// resolve the primary link with help from the forwardLink audience
					// if definite (audienceInSubQuote) and only one entity and previous is not one entity and not definitely resolved
					if (audienceInSubQuote &&
						m[lastOpeningPrimaryQuote].audienceObjectMatches.size() == 1 && m[previousPrimaryQuote].audienceObjectMatches.size() != 1 &&
						!(m[previousPrimaryQuote].flags&cWordMatch::flagSpecifiedResolveAudience))
					{
						// if the objectMatches of the primaryLink includes the audience, delete the audience
						subtract(m[lastOpeningPrimaryQuote].audienceObjectMatches[0].object, m[previousPrimaryQuote].objectMatches);
						wstring tmpstr3;
						setSpeakerMatchesFromPosition(previousPrimaryQuote, m[previousPrimaryQuote].audienceObjectMatches, audienceObjectPosition, L"audienceObjectPositionSubQuote", cWordMatch::flagSpecifiedResolveAudience);
						if (intersect(m[previousPrimaryQuote].audienceObjectMatches, m[previousPrimaryQuote].objectMatches, allIn, oneIn) && !allIn)
							subtract(m[previousPrimaryQuote].audienceObjectMatches, m[previousPrimaryQuote].objectMatches);
						if (debugTrace.traceSpeakerResolution)
							lplog(LOG_RESOLUTION, L"%06d:Resolve by definite audience backtrack from forwardLink [%s] resolving %d:%s to %d:%s",
								lastOpeningPrimaryQuote, speakerResolutionFlagsString(m[previousPrimaryQuote].flags, tmpstr).c_str(),
								lastOpeningPrimaryQuote,
								objectString(m[lastOpeningPrimaryQuote].audienceObjectMatches, tmpstr2, true).c_str(),
								previousPrimaryQuote,
								objectString(m[previousPrimaryQuote].audienceObjectMatches, tmpstr3, true).c_str());
					}
				}
				// (CMREADME36)
				for (unsigned int J = 0; J < m[lastOpeningPrimaryQuote].audienceObjectMatches.size(); J++)
					objects[m[lastOpeningPrimaryQuote].audienceObjectMatches[J].object].speakerLocations.insert(lastOpeningPrimaryQuote);
				if (previousPrimaryQuote != lastOpeningPrimaryQuote)
				{
					m[previousPrimaryQuote].setQuoteForwardLink(lastOpeningPrimaryQuote); // resolve unknown speakers
					m[lastOpeningPrimaryQuote].quoteBackLink = previousPrimaryQuote; // resolve unknown speakers
				}
				else
					lplog(LOG_ERROR, L"%06d:previousPrimaryQuote and lastOpeningPrimaryQuote are the same (2)!", previousPrimaryQuote);
				if (m[previousPrimaryQuote].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers)
					m[lastOpeningPrimaryQuote].flags |= cWordMatch::flagEmbeddedStoryResolveSpeakers;
				if (debugTrace.traceSpeakerResolution)
				{
					wstring reason;
					if (multipleQuoteInSentence) reason = L"multipleQuoteInSentence ";
					if (!immediatelyAfterEndOfParagraph) reason += L"!immediatelyAfterEndOfParagraph ";
					if (audienceInSubQuote) reason += L"audienceInSubQuote ";
					if (audienceFromSpeakerGroup) reason += L"audienceFromSpeakerGroup ";
					if (previousEndQuoteInserted) reason += L"inserted quote ";
					lplog(LOG_RESOLUTION, L"%06d:set audience matches (2) to %s - forward linked from %d - %s", lastOpeningPrimaryQuote, objectString(m[lastOpeningPrimaryQuote].audienceObjectMatches, tmpstr, true).c_str(), previousPrimaryQuote, reason.c_str());
				}
			}
		}
		else
		{
		  // (CMREADME37)
			int lastDefinedOpenEmbeddedSpeakerGroup = currentEmbeddedSpeakerGroup, ldsg = currentSpeakerGroup;
			if (lastDefinedOpenEmbeddedSpeakerGroup < 0 && ldsg>0 && speakerGroups[ldsg - 1].embeddedSpeakerGroups.size() > 0)
				lastDefinedOpenEmbeddedSpeakerGroup = speakerGroups[ldsg = ldsg - 1].embeddedSpeakerGroups.size() - 1;
			if (lastDefinedOpenEmbeddedSpeakerGroup >= 0 && !(m[lastOpeningPrimaryQuote].flags&(cWordMatch::flagEmbeddedStoryResolveSpeakers | cWordMatch::flagEmbeddedStoryResolveSpeakersGap)) &&
				speakerGroups[ldsg].embeddedSpeakerGroups[lastDefinedOpenEmbeddedSpeakerGroup].sgEnd <= 0 && previousPrimaryQuote >= 0)
			{
				int forwardLink = previousPrimaryQuote;
				while (m[forwardLink].getQuoteForwardLink() > forwardLink) forwardLink = m[forwardLink].getQuoteForwardLink();
				speakerGroups[ldsg].embeddedSpeakerGroups[lastDefinedOpenEmbeddedSpeakerGroup].sgEnd = m[forwardLink].endQuote;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%d-%d:[%d,%d]mark end of embedded speaker group:%s",
						lastOpeningPrimaryQuote, where, ldsg, lastDefinedOpenEmbeddedSpeakerGroup,
						toText(speakerGroups[ldsg].embeddedSpeakerGroups[lastDefinedOpenEmbeddedSpeakerGroup], tmpstr));
				currentEmbeddedSpeakerGroup = -1;
				currentEmbeddedTimelineSegment = -1;
			}
			cLocalFocus::setSalienceAgeMethod(false, false, objectToBeMatchedInQuote, quoteIndependentAge);
			int audienceObjectPosition = scanForSpeakers(lastOpeningPrimaryQuote, where, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, HAIL_ROLE | IN_QUOTE_REFERRING_AUDIENCE_ROLE);
			bool definitelySpeaker = false;
			int speakerPosition = determineSpeaker(lastOpeningPrimaryQuote, where, true, noSpeakerAfterward, definitelySpeaker, audienceObjectPosition), w, w2;
			// the boots of Albert
			if (speakerPosition >= 0 && (w = m[speakerPosition].principalWherePosition) >= 0 &&
				m[w].getObject() >= 0 && objects[m[w].getObject()].objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
				(w2 = objects[m[w].getObject()].getOwnerWhere()) >= 0 && m[w2].getObject() >= 0 &&
				(objects[m[w2].getObject()].male || objects[m[w2].getObject()].female))
				speakerPosition = w2;
			if (speakerPosition < 0)
				definitelySpeaker |= (speakerPosition = scanForSpeakers(lastOpeningPrimaryQuote, where, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, IN_QUOTE_SELF_REFERRING_SPEAKER_ROLE)) >= 0;
			// If there is more than one speaker, and if the speaker position object is not found in the current speaker group, invalidate speaker position.
			if (speakerPosition >= 0 && m[speakerPosition].principalWherePosition >= 0 && m[m[speakerPosition].principalWherePosition].getObject() >= 0 &&
				speakerGroups[currentSpeakerGroup].speakers.size() > 1 &&
				objects[m[m[speakerPosition].principalWherePosition].getObject()].objectClass == NON_GENDERED_GENERAL_OBJECT_CLASS &&
				speakerGroups[currentSpeakerGroup].speakers.find(m[m[speakerPosition].principalWherePosition].getObject()) == speakerGroups[currentSpeakerGroup].speakers.end())
				speakerPosition = -1;
			// (CMREADME38)
			if (speakerPosition >= 0)
			{
				resolveMetaReference(speakerPosition, lastOpeningPrimaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb);
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:XXZ subjectsInPreviousUnquotedSection=%s", where, objectString(subjectsInPreviousUnquotedSection, tmpstr).c_str());
				imposeSpeaker(lastOpeningPrimaryQuote, where, lastDefiniteSpeaker, speakerPosition, definitelySpeaker, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, previousSpeakersUncertain, audienceObjectPosition, subjectsInPreviousUnquotedSection, whereSubjectsInPreviousUnquotedSection);
				previousSpeakersUncertain = (previousSpeakers.size() != 1 && !(m[speakerPosition].word->second.inflectionFlags&PLURAL));
			}
			else
			{
				if ((m[lastOpeningPrimaryQuote].flags&cWordMatch::flagEmbeddedStoryResolveSpeakers) && m[lastOpeningPrimaryQuote].embeddedStorySpeakerPosition >= 0)
					setEmbeddedStorySpeaker(lastOpeningPrimaryQuote, lastDefiniteSpeaker);
				else if (noTextBeforeOrAfter)
				{
					getMostLikelySpeakers(lastOpeningPrimaryQuote, where, lastDefiniteSpeaker, previousSpeakersUncertain,
						(paragraphsSinceLastSubjectWasSet) ? -1 : wherePreviousLastSubjects, previousLastSubjects, (audienceObjectPosition >= 0) ? audienceObjectPosition : lastDefiniteSpeaker, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb);
					imposeMostLikelySpeakers(lastOpeningPrimaryQuote, lastDefiniteSpeaker, audienceObjectPosition);
				}
				else
					quotedStringSeen = true;
				previousSpeakersUncertain = true;
			}
			if (audienceObjectPosition >= 0 && m[audienceObjectPosition].getObject() >= 0)
			{
				beforePreviousSpeakers.clear();
				beforePreviousSpeakers.push_back(m[audienceObjectPosition].getObject()); // this prevents NOTmatches from being null
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:Impose added audience %s to before previous speakers", where, objectString(m[audienceObjectPosition].getObject(), tmpstr, false).c_str());
			}
		}
		immediatelyAfterEndOfParagraph = false;
		if (paragraphsSinceLastSubjectWasSet >= 0) paragraphsSinceLastSubjectWasSet++;
	}
	// (CMREADME40)
	if (lastQuotedString > lastSentenceEndBeforeAndNotIncludingCurrentQuote)
		quotedStringSeen = true;
	// remove any objects referring to a quote which is not a quoted string
	for (; quotedObjectCounter < objects.size(); quotedObjectCounter++)
		if (objects[quotedObjectCounter].begin == lastOpeningPrimaryQuote)
		{
			if (debugTrace.traceObjectResolution)
			{
				lplog(LOG_RESOLUTION, L"%06d:object %s eliminated because it includes a primary quote",
					lastOpeningPrimaryQuote, objectString(quotedObjectCounter, tmpstr, false).c_str());
			}
			objects[quotedObjectCounter].eliminated = true;
			for (vector <cSection>::iterator is = sections.begin(), isEnd = sections.end(); is != isEnd; is++)
				is->preIdentifiedSpeakerObjects.erase(quotedObjectCounter);
		}
		else if (objects[quotedObjectCounter].begin > lastOpeningPrimaryQuote)
			break;
	quotesSeenSinceLastSentence = true;
	if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION, L"%06d:(3) quotesSeenSinceLastSentence set to true", where);
	previousPrimaryQuote = lastOpeningPrimaryQuote;
	resolveQuotedPOVObjects(lastOpeningPrimaryQuote, where);
}

void cSource::processEndOfSentenceRS(int where, 
	int &questionSpeakerLastSentence, int &questionSpeaker, bool &currentIsQuestion,
	int &lastBeginS1, int &lastRelativePhrase, int &lastQ2, int &lastClosingPrimaryQuote,
	int &paragraphsSinceLastSubjectWasSet, int &wherePreviousLastSubjects,
	bool &inPrimaryQuote, bool &inSecondaryQuote,bool &quotesSeenSinceLastSentence,
	int whereSubject,vector <int> &lastSubjects,vector <int> &previousLastSubjects,
	int &lastSentenceMetaSpeakerQuery,int &currentMetaSpeakerQuery, int &currentMetaWhereQuery,
	bool &endOfSentence,bool &transitionSinceEOS,bool &accumulateMultipleSubjects,
	int &uqPreviousToLastSentenceEnd, int &uqLastSentenceEnd, int &lastSentenceEnd,
	int lastSectionWord, unsigned int &agingStructuresSeen)
{
	wstring tmpstr,tmpstr2;
	if (inPrimaryQuote)
	{
		lastSentenceMetaSpeakerQuery = currentMetaSpeakerQuery;
		currentMetaSpeakerQuery = -1;
	}
	lastBeginS1 = -1;
	lastRelativePhrase = -1;
	lastQ2 = -1;
	endOfSentence = true;
	transitionSinceEOS = false;
	int whereLastObject = -1;
	// Process unquoted previous and last sentence ends to scan for space relations and meta where query patterns
	bool anySpaceRelation = false;
	if (isEOS(uqPreviousToLastSentenceEnd)) uqPreviousToLastSentenceEnd++; // skip the last sentence's EOS.
	if (m[uqPreviousToLastSentenceEnd].queryForm(quoteForm) >= 0)
		uqPreviousToLastSentenceEnd++; // skip the quote at the end of last sentence.
	if (m[uqPreviousToLastSentenceEnd].word == Words.sectionWord)
		uqPreviousToLastSentenceEnd++; // skip the separator.
	if (m[uqPreviousToLastSentenceEnd].queryForm(quoteForm) >= 0)
		uqPreviousToLastSentenceEnd++; // skip the quote at the start of this sentence.
	for (int J = uqPreviousToLastSentenceEnd; J < uqLastSentenceEnd; J++)
	{
		detectSpaceRelation(J, where, lastSubjects); // so that space relations use disambiguated objects
		detectSpaceLocation(J, lastBeginS1);
		evaluateMetaWhereQuery(J, false, currentMetaWhereQuery); // how to use currentIsQuestion when this is set below?
		if (m[J].getObject() >= 0) whereLastObject = J;
		anySpaceRelation |= (m[J].spaceRelation);
	}
	// the inQuestion flag is only set as the first word of the question or any object
	// if the sentence is one word or one object, make space relation one word.
	if (!anySpaceRelation && uqLastSentenceEnd - uqPreviousToLastSentenceEnd > 0 && (uqLastSentenceEnd - uqPreviousToLastSentenceEnd <= 2 || whereLastObject >= 0))
	{
		if (whereLastObject < 0) whereLastObject = uqPreviousToLastSentenceEnd;
		cSpaceRelation lastSR(whereLastObject, -1, -1, -1, -1, -1, -1, -1, -1, stNORELATION, false, false, -1, -1, true);
		if (lastOpeningPrimaryQuote > uqLastSentenceEnd)
			lastSR.tft.lastOpeningPrimaryQuote = m[lastOpeningPrimaryQuote].previousQuote;
		else
			lastSR.tft.lastOpeningPrimaryQuote = lastOpeningPrimaryQuote;
		bool comparesr(const cSpaceRelation &s1, const cSpaceRelation &s2);
		vector <cSpaceRelation>::iterator location = lower_bound(spaceRelations.begin(), spaceRelations.end(), lastSR, comparesr);
		spaceRelations.insert(location, lastSR);
	}
	uqPreviousToLastSentenceEnd = uqLastSentenceEnd;
	uqLastSentenceEnd = where;
	lastSentenceEnd = where;
	// reset subjectsInPreviousUnquotedSectionUsableForImmediateResolution
	accumulateMultipleSubjects = false;
	if (!inPrimaryQuote && !inSecondaryQuote && subjectsInPreviousUnquotedSectionUsableForImmediateResolution)
	{
		subjectsInPreviousUnquotedSectionUsableForImmediateResolution = false;
		if (debugTrace.traceSpeakerResolution && subjectsInPreviousUnquotedSection.size())
			lplog(LOG_SG | LOG_RESOLUTION, L"%06d:%02d Cancelling subjectsInPreviousUnquotedSectionUsableForImmediateResolution", where, section);
	}
	// use questions to enhance the identification of speakers
	if (!inSecondaryQuote || (where + 1 < (signed)m.size() && m[where + 1].word->first == L"’"))
		setQuestion(m.begin() + where, inPrimaryQuote, questionSpeakerLastSentence, questionSpeaker, currentIsQuestion);
	else
		setSecondaryQuestion(m.begin() + where);
	if (!agingStructuresSeen)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:%02d     aging speakers (%s) EOS", where, section, (inPrimaryQuote) ? L"inQuote" : L"outsideQuote");
		for (vector <cLocalFocus>::iterator lfi = localObjects.begin(); lfi != localObjects.end();)
			ageSpeaker(where, inPrimaryQuote, inSecondaryQuote, lfi, 1);
	}
	else if (debugTrace.traceSpeakerResolution)
		lplog(LOG_RESOLUTION, L"%06d:%02d     NOT aging speakers (%s) EOS - %d agingStructuresSeen", where, section, (inPrimaryQuote) ? L"inQuote" : L"outsideQuote", agingStructuresSeen);
	agingStructuresSeen = 0;
	// save subjects and clear current subjects
	if (!inPrimaryQuote && !inSecondaryQuote)
	{
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:ZXZ set previousLastSubjects=%s@%d (previous value %s@%d). Cleared lastSubjects",
				where, objectString(lastSubjects, tmpstr).c_str(), whereSubject, objectString(previousLastSubjects, tmpstr).c_str(), wherePreviousLastSubjects);
		previousLastSubjects = lastSubjects;
		wherePreviousLastSubjects = whereSubject;
		paragraphsSinceLastSubjectWasSet = 0;
		lastSubjects.clear();
	}
	if (inPrimaryQuote && ((unsigned)currentSpeakerGroup) < speakerGroups.size() && currentEmbeddedSpeakerGroup >= 0 &&
		((unsigned)currentEmbeddedSpeakerGroup) < speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size())
	{
		// age out secondary speaker groups
		for (vector <cLocalFocus>::iterator lsi = localObjects.begin(), lsiEnd = localObjects.end(); lsi != lsiEnd; lsi++)
		{
			if (lsi->getAge(false, objectToBeMatchedInQuote, quoteIndependentAge) >= 10 && speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.find(lsi->om.object) != speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.end())
			{
				// find next ending secondaryquote after last Where 
				int J = lsi->lastWhere, begin = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin;
				for (; J >= begin && (m[J].word->first != L"“" || m[J].nextQuote < 0); J--);
				if (J >= begin && (m[m[J].nextQuote].flags&(cWordMatch::flagEmbeddedStoryResolveSpeakers | cWordMatch::flagEmbeddedStoryResolveSpeakersGap)))
				{
					int getForwardLinkedEnd = J;
					while (m[getForwardLinkedEnd].getQuoteForwardLink() >= 0) getForwardLinkedEnd = m[getForwardLinkedEnd].getQuoteForwardLink();
					speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.push_back(speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup]);
					speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup++].sgEnd = m[getForwardLinkedEnd].endQuote;
					speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin = m[J].nextQuote;
					speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd = -2;
					speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].speakers.erase(lsi->om.object);
					// also erase any speakers discovered before the begin point (they belong to the next embedded speaker group)
					for (set<int>::iterator s = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup - 1].speakers.begin(),
						sEnd = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup - 1].speakers.end();
						s != sEnd;)
					{
						int lb = locationBefore(*s, speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup - 1].sgEnd);
						if (lb < 0 || lb >= speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup - 1].sgEnd)
							speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup - 1].speakers.erase(s++);
						else
							s++;
					}
					if (debugTrace.traceSpeakerResolution)
						lplog(LOG_RESOLUTION | LOG_SG, L"%d:Aged out %s. Q[%d %d %d] Created another embeddedSpeakerGroup %s.", where, objectString(lsi->om, tmpstr, true).c_str(), J, m[J].endQuote, m[J].nextQuote, toText(speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup], tmpstr2));
				}
			}
		}
	}
	// lastDefiniteSpeaker processing - look ahead - is this NOT the last sentence in the paragraph?
	// All examples assume sectionWord occurs immediately after example.
	// A. don't set lastDefiniteSpeaker.  most likely speakers already sets lastDefiniteSpeaker to -1. (inPrimaryQuote=true)
	//   “[mr:miss] at eleven[eleven] o'clock . ” 
	// B. don't set lastDefiniteSpeaker since this speaker is definite. (lastBeginS1<lastClosedQuote) inPrimaryQuote==false
	//   "My time is my own," replied Mr. Beresford magnificently.  
	// C. don't set lastDefiniteSpeaker, since this speaker is definite. (lastBeginS1>lastClosedQuote, but 
	//   "What about the colonies?" she suggested.
	// D. lastDefiniteSpeaker never set since definitelySpeaker was false (poured has objects - dregs is an object)
	//   "let us[miss,tommy] drink to success. " she[miss] poured some cold dregs of tea[tea] into the two cups .
	if (where + 3 < (signed)m.size() && m[where + 1].word != Words.sectionWord)
	{
		quotesSeenSinceLastSentence = inPrimaryQuote;
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:(EOS)quotesSeenSinceLastSentence=%s", where, (quotesSeenSinceLastSentence) ? L"true" : L"false");
	}
	else
	{
		quotesSeenSinceLastSentence = (lastClosingPrimaryQuote > lastSectionWord);
		if (debugTrace.traceSpeakerResolution)
			lplog(LOG_RESOLUTION, L"%06d:quotesSeenSinceLastSentence=%s not set (end of paragraph)", where, (quotesSeenSinceLastSentence) ? L"true" : L"false");
	}
}

// identifies all speakers of quotes.  Also identifies all possible objects for anaphora resolution.
//
// accumulate all speakers in localObjects.  Dump localObjects on a section header.
// if unable to identify speaker,
//    assign previous speaker to beforePrevious,
//    assign the two most common localObjects (or two speakers positively identified), and remove the previous speakers.
//    assign speaker to previous speaker (done in previous step)
//    add index to unknown speakers.
// if able to identify speaker (S),
//    add speaker (S) to local speaker.  Increment numEncounters and set identified=true. (done in findSpeaker)
//      assign speaker (S) to all even previous unknown speakers.
//      subtract speaker (S) from all odd previous unknown speakers. (1 before, 3 before, etc)
//    assign previous speaker to beforePrevious,
//    assign speaker (S) to previous speaker
// reject as possible speakers not positively identified:
//   1. pronouns and Quantifiers (Some, All)
//   2. any noun having adjectives that are pronouns
//   3. strip off any preposition phrases or such
void cSource::resolveSpeakers(vector <int> &secondaryQuotesResolutions)
{ LFS 
	resetObjects();
  bool quotesSeen=false,quotesSeenSinceLastSentence=false,endOfSentence=false,immediatelyAfterEndOfParagraph=true,inPrimaryQuote=false,inSecondaryQuote=false,previousSpeakersUncertain=true,accumulateMultipleSubjects=false;
	bool currentIsQuestion=false,transitionSinceEOS=false;
	setFlagAlternateResolveForwardAfterDefinite=false; // no definite speakers
  lastOpeningPrimaryQuote=-1;
	lastQuote=-1;
	previousPrimaryQuote=-1;
	lastOpeningSecondaryQuote=-1;
	int beginSection=0,lastBeginS1=0,lastDefiniteSpeaker=-1,lastSentenceEnd=0,uqLastSentenceEnd=0,uqPreviousToLastSentenceEnd=0,lastQuotedString=-1,currentMetaSpeakerQuery=-1,lastSentenceMetaSpeakerQuery=-1,currentMetaWhereQuery=-1;
  int lastClosingPrimaryQuote=-1,lastSectionWord=-1,lastRelativePhrase=-1,lastQ2=-1,lastVerb=-1;
	int questionSpeaker=-1,questionSpeakerLastParagraph=-1,questionSpeakerLastSentence=-1,whereFirstSubjectInParagraph=-1;
  int lastSentenceEndBeforeAndNotIncludingCurrentQuote=-1,lastProgressPercent=-1;
	int whereLetterTo=-1,lastLetterBegin=-1;
	/* last subject */
	// subjectsInPreviousUnquotedSection is used in imposeSpeaker to set lastSubject bool flag in each localObject occurring outside quotes.
	//    also used in resolveSpeakersUsingPreviousSubject at the break after a section with no quotes in it.  
	//    after a section with no quotes in it, subjectsInPreviousUnquotedSection is set by previousLastSubjects.  
	//    It is cleared at the beginning of a section.
	// previousLastSubjects is used in getMostLikelySpeakers - if there is already two or more speakers, all previousLastSubjects are added as well.
	//    It is set to lastSubjects after the end of a sentence not in a quote, and cleared at the end of a section. (only if paragraphsSinceLastSubjectWasSet==0)
	// lastSubjects is cleared at the end of a section and after setting previousLastSubjects after the end of a sentence not in a quote.
	//    It is set by the subjects of every sentence not in a quote.
  int wherePreviousLastSubjects=-1,whereSubject=-1,paragraphsSinceLastSubjectWasSet=-1;
	whereSubjectsInPreviousUnquotedSection=0;
	primaryLocationLastPosition=-1;
  vector <int> lastSubjects,previousLastSubjects;
	/* last subject */
  unsigned int quotedObjectCounter=0,agingStructuresSeen=0;
	speakerGroupsEstablished=true;
	subjectsInPreviousUnquotedSectionUsableForImmediateResolution=false;
	int endMetaResponse=-1;
	spaceRelationsIdEnd=spaceRelations.size();
	if (!speakerGroups.size())
	{
		cSpeakerGroup entireSpeakerGroup;
		entireSpeakerGroup.sgBegin=0;
		entireSpeakerGroup.sgEnd=m.size();
		entireSpeakerGroup.speakers.insert(0);
		entireSpeakerGroup.povSpeakers.insert(0);
		entireSpeakerGroup.observers.insert(1);
		speakerGroups.push_back(entireSpeakerGroup);
	}
	wstring tmpstr,tmpstr2;
	for (int I = 0; I<m.size() && !exitNow; I++)
	{
		if ((int)(I * 100 / m.size())>lastProgressPercent)
		{
			lastProgressPercent = (int)I * 100 / m.size();
			wprintf(L"PROGRESS: %03d%% speakers resolved with %d seconds elapsed \r", lastProgressPercent, clocksec());
		}
		if (m[I].skipResponse >= 0)
		{
			endMetaResponse = m[I].skipResponse;
			subjectsInPreviousUnquotedSection.clear();
			int oc;
			if (m[I].relSubject >= 0 && m[m[I].relSubject].getObject() >= 0 && ((oc = objects[m[m[I].relSubject].getObject()].objectClass) == NAME_OBJECT_CLASS ||
				oc == GENDERED_GENERAL_OBJECT_CLASS || oc == BODY_OBJECT_CLASS || oc == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS || oc == GENDERED_DEMONYM_OBJECT_CLASS ||
				oc == META_GROUP_OBJECT_CLASS || oc == GENDERED_RELATIVE_OBJECT_CLASS))
			{
				whereSubjectsInPreviousUnquotedSection = m[I].relSubject;
				subjectsInPreviousUnquotedSection.push_back(m[whereSubjectsInPreviousUnquotedSection].getObject());
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d-%06d:Detected meta-response [subject %d:%s].", I, m[I].skipResponse, whereSubjectsInPreviousUnquotedSection, whereString(whereSubjectsInPreviousUnquotedSection, tmpstr, true).c_str());
			}
		}
		debugTrace.traceObjectResolution = m[I].t.traceObjectResolution ^ flipTOROverride;
		debugTrace.traceNameResolution = m[I].t.traceNameResolution ^ flipTNROverride;
		debugTrace.traceSpeakerResolution = m[I].t.traceSpeakerResolution || TSROverride;
		logCache = m[I].logCache;
		if (m[I].pma.queryPattern(L"_REL1") != -1)
			lastRelativePhrase = I;
		if (m[I].pma.queryPattern(L"_Q2") != -1)
			lastQ2 = I;
		if (m[I].hasVerbRelations)
			lastVerb = I;
		if (m[I].pma.queryPattern(L"__S1") != -1 && endMetaResponse < I)
		{
			lastBeginS1 = I;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:%02d     aging speakers (%s) BeginS1", I, section, (inPrimaryQuote) ? L"inQuote" : L"outsideQuote");
			for (vector <cLocalFocus>::iterator lfi = localObjects.begin(); lfi != localObjects.end();)
				ageSpeaker(I, inPrimaryQuote, inSecondaryQuote, lfi, 1);
			agingStructuresSeen++;
			accumulateMultipleSubjects = false;
		}
		if (inPrimaryQuote && m[I].pma.queryPattern(L"_META_SPEAKER_QUERY") != -1)
			currentMetaSpeakerQuery = I;
		evaluateMetaWhereQuery(I, inPrimaryQuote, currentMetaWhereQuery);
		associateNyms(I); // (ASSOC)
		associatePossessions(I);
		adjustHailRoleDuringScan(I);
		if (m[I].getObject() >= 0 || m[I].getObject() < cObject::eOBJECTS::UNKNOWN_OBJECT)
			resolveObject(I, (m[I].objectRole&PRIMARY_SPEAKER_ROLE) != 0, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, (m[I].objectRole&PRIMARY_SPEAKER_ROLE) != 0, false, false);
		if (m[I].objectMatches.size() == 1 && m[I].getObject() >= 0)
			moveNyms(I, m[I].objectMatches[0].object, m[I].getObject(), L"resolveSpeakers");
		identifyMetaNameEquivalence(I, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb);
		identifyMetaSpeaker(I, inPrimaryQuote | inSecondaryQuote);
		identifyAnnounce(I, inPrimaryQuote | inSecondaryQuote);
		identifyMetaGroup(I, inPrimaryQuote, inSecondaryQuote, lastBeginS1, lastRelativePhrase, lastQ2, lastVerb);
		letterDetectionBegin(I, whereLetterTo, lastLetterBegin);
		if (letterDetectionEnd(I, whereLetterTo, lastLetterBegin))
			continue;
		vector<cSpaceRelation>::iterator sr;
		if (m[I].spaceRelation && (m[I].objectRole&(IN_SECONDARY_QUOTE_ROLE | IN_PRIMARY_QUOTE_ROLE)) == 0 && (sr = findSpaceRelation(I))!=spaceRelations.end() && (sr = findSpaceRelation(I))->where == I &&
			sr->tft.timeTransition)
		{
			vector <cLocalFocus>::iterator lsi, llsi;
			int numPPSpeakers = 0, keepPPSpeakerWhere = sr->whereSubject;
			bool allIn, oneIn;
			// if there is only one PP speaker, and whereSubject is not a speaker, and spaceRelation is not a TIME type, then set where to the PP speaker.
			for (set<int>::iterator i = speakerGroups[currentSpeakerGroup].speakers.begin(), iEnd = speakerGroups[currentSpeakerGroup].speakers.end(); i != iEnd; i++)
				if ((lsi = in(*i)) != localObjects.end() && lsi->physicallyPresent)
				{
					numPPSpeakers++;
					llsi = lsi;
				}
			if ((sr->whereSubject < 0 || !intersect(sr->whereSubject, speakerGroups[currentSpeakerGroup].speakers, allIn, oneIn)) && numPPSpeakers == 1)
				keepPPSpeakerWhere = llsi->whereBecamePhysicallyPresent;
			ageTransition(I, true, transitionSinceEOS, sr->tft.duplicateTimeTransitionFromWhere, keepPPSpeakerWhere, lastSubjects, L"TSR 3");
		}
		// Wikipedia
		if (sourceType != PATTERN_TRANSFORM_TYPE)
			identifyISARelation(I, true);
		// CMREADME30
		if (!(m[I].objectRole&(FOCUS_EVALUATED | HAIL_ROLE)) && (m[I].getObject() >= 0 && objects[m[I].getObject()].objectClass == NAME_OBJECT_CLASS &&
			m[I].beginObjectPosition > 0 && (m[m[I].beginObjectPosition - 1].queryWinnerForm(conjunctionForm) >= 0 || m[m[I].beginObjectPosition - 1].queryForm(quoteForm) >= 0) &&
			m[I].endObjectPosition<(signed)m.size() && isEOS(m[I].endObjectPosition)) && objects[m[I].getObject()].numDefinitelyIdentifiedAsSpeaker>0)
		{
			m[I].objectRole |= OBJECT_ROLE | SUBJECT_ROLE; // mark for use with localRoleBoost
			vector <cLocalFocus>::iterator lsi;
			if ((lsi = in(m[I].getObject())) != localObjects.end() && lsi->lastWhere == I)
				m[I].objectRole |= UNRESOLVABLE_FROM_IMPLICIT_OBJECT_ROLE; // mark for use with localRoleBoost
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Set object %s to OBJ/SUBJ role (ALONE) [%d].", I, objectString(m[I].getObject(), tmpstr, true).c_str(), (lsi != localObjects.end()) ? lsi->lastWhere : -1);
		}
		if (I < endMetaResponse) continue;
		// CMREADME31
		if (!currentIsQuestion)
		{
			bool erasePreviousSubject = (m[I].objectRole != (SUBJECT_ROLE | MPLURAL_ROLE) || !accumulateMultipleSubjects);
			if (m[I].objectMatches.empty())
				accumulateSubjects(I, m[I].getObject(), inPrimaryQuote, inSecondaryQuote, whereSubject, erasePreviousSubject, lastSubjects);
			for (unsigned int om = 0; om < m[I].objectMatches.size(); om++)
				accumulateSubjects(I, m[I].objectMatches[om].object, inPrimaryQuote, inSecondaryQuote, whereSubject, erasePreviousSubject, lastSubjects);
			accumulateMultipleSubjects = (m[I].objectRole == (SUBJECT_ROLE | MPLURAL_ROLE));
			if (whereFirstSubjectInParagraph == -1 && whereSubject == I)
			{
				whereFirstSubjectInParagraph = whereSubject;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:QXQ whereFirstSubjectInParagraph set to %d.", I, whereFirstSubjectInParagraph);
			}
		}
		// CMREADME32
		if (m[I].word->first == L"lptable" || m[I].word->first == L"lpendcolumn")
		{
			localObjects.clear();
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:cleared local objects (%s)", I, m[I].word->first.c_str());
		}
		// CMREADME33
		if (m[I].word->first == L"“")
		{
			if (m[I].flags&cWordMatch::flagEmbeddedStoryBeginResolveSpeakers)
			{
				int lastDefinedOpenEmbeddedSpeakerGroup = currentEmbeddedSpeakerGroup, ldsg = currentSpeakerGroup;
				if (lastDefinedOpenEmbeddedSpeakerGroup<0 && ldsg>0 && speakerGroups[ldsg - 1].embeddedSpeakerGroups.size() > 0)
					lastDefinedOpenEmbeddedSpeakerGroup = speakerGroups[ldsg = ldsg - 1].embeddedSpeakerGroups.size() - 1;
				if (lastDefinedOpenEmbeddedSpeakerGroup >= 0 && ldsg >= 0 &&
					speakerGroups[ldsg].embeddedSpeakerGroups[lastDefinedOpenEmbeddedSpeakerGroup].sgEnd <= 0)
				{
					// the only way this point is reached is if there is a story that is made up of continuous quotes BUT
					// switches speakers (or appeared to at speaker identification time)
					int forwardLink = speakerGroups[ldsg].embeddedSpeakerGroups[lastDefinedOpenEmbeddedSpeakerGroup].sgBegin;
					while (m[forwardLink].getQuoteForwardLink() > forwardLink) forwardLink = m[forwardLink].getQuoteForwardLink();
					speakerGroups[ldsg].embeddedSpeakerGroups[lastDefinedOpenEmbeddedSpeakerGroup].sgEnd = m[forwardLink].endQuote;
				}
				speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.push_back(cSpeakerGroup());
				currentEmbeddedSpeakerGroup = speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups.size() - 1;
				speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgBegin = I;
				speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].sgEnd = -3;
				speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup].section = speakerGroups[currentSpeakerGroup].section;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:mark start of embedded speaker group[%d,%d]:%s",
					I, currentSpeakerGroup, currentEmbeddedSpeakerGroup, toText(speakerGroups[currentSpeakerGroup].embeddedSpeakerGroups[currentEmbeddedSpeakerGroup], tmpstr));
			}
			if (!(m[I].flags&cWordMatch::flagQuotedString))
			{
				lastSentenceEndBeforeAndNotIncludingCurrentQuote = lastSentenceEnd;
				inPrimaryQuote = true;
				quotesSeen = quotesSeenSinceLastSentence = true;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:quotesSeen=quotesSeenSinceLastSentence=true", I);
				int previousOpeningQuote = lastOpeningPrimaryQuote;
				lastOpeningPrimaryQuote = I;
				if (lastSentenceMetaSpeakerQuery >= 0 && processMetaSpeakerQueryAnswer(lastOpeningPrimaryQuote, previousOpeningQuote, lastSentenceMetaSpeakerQuery)) continue;
			}
			else
				lastQuotedString = I;
		}
		else if (m[I].word->first == L"”" && lastOpeningPrimaryQuote >= 0 && !inPrimaryQuote)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:End of quoted string", I);
		}
		else if (m[I].word->first == L"”" && lastOpeningPrimaryQuote >= 0)
		{
			processEndOfPrimaryQuoteRS(I, lastSentenceEndBeforeAndNotIncludingCurrentQuote,
				lastBeginS1, lastRelativePhrase, lastQ2, lastVerb, lastQuotedString, quotedObjectCounter, lastDefiniteSpeaker, lastClosingPrimaryQuote,
				paragraphsSinceLastSubjectWasSet, wherePreviousLastSubjects,
				inPrimaryQuote, immediatelyAfterEndOfParagraph, quotesSeenSinceLastSentence, previousSpeakersUncertain,
				previousLastSubjects);
		}
		else if (m[I].word->first == L"‘")
		{
			lastOpeningSecondaryQuote = I;
			if (!(m[I].flags&cWordMatch::flagQuotedString))
			{
				inSecondaryQuote = true;
				inPrimaryQuote = false;
			}
		}
		else if (m[I].word->first == L"’" && inSecondaryQuote)
		{
			inSecondaryQuote = false;
			inPrimaryQuote = (lastOpeningPrimaryQuote >= 0);
			setSecondaryQuoteString(I, secondaryQuotesResolutions);
			resolveQuotedPOVObjects(lastOpeningSecondaryQuote, I);
		}
		// (CMREADME41)
		// if end of sentence, or next word is a double 'next line' BUT the current word is not itself a 'next line' or a comma or inside of a chapter heading
		else if (isEOS(I) || (I + 1 < ((signed)m.size()) && m[I + 1].word == Words.sectionWord && m[I].word != Words.sectionWord && m[I].word->first != L"," && (sections.empty() || I >= (int)sections[section].endHeader)))
		{
			processEndOfSentenceRS(I,
				questionSpeakerLastSentence, questionSpeaker, currentIsQuestion,
				lastBeginS1, lastRelativePhrase, lastQ2, lastClosingPrimaryQuote,
				paragraphsSinceLastSubjectWasSet, wherePreviousLastSubjects,
				inPrimaryQuote, inSecondaryQuote, quotesSeenSinceLastSentence,
				whereSubject, lastSubjects, previousLastSubjects,
				lastSentenceMetaSpeakerQuery, currentMetaSpeakerQuery, currentMetaWhereQuery,
				endOfSentence, transitionSinceEOS, accumulateMultipleSubjects,
				uqPreviousToLastSentenceEnd, uqLastSentenceEnd, lastSentenceEnd,
				lastSectionWord, agingStructuresSeen);
		}
		// (CMREADME42)
		else if (m[I].word == Words.sectionWord)
		{
			if (m[I].flags & 1)
			{
				localObjects.clear();
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:cleared local objects", I);
			}
			endOfSentence = false;
			immediatelyAfterEndOfParagraph = true;
			accumulateMultipleSubjects = false;
			lastSectionWord = I;
			subjectsInPreviousUnquotedSectionUsableForImmediateResolution = false;
			if (!quotesSeenSinceLastSentence)
			{
				lastLetterBegin = -1;
				whereLetterTo = -1;
				setFlagAlternateResolveForwardAfterDefinite = false; // no definite speakers
				if (lastOpeningPrimaryQuote >= 0)
					m[lastOpeningPrimaryQuote].flags |= cWordMatch::flagLastContinuousQuote;
				lastDefiniteSpeaker = -1;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:Reset last definite speaker.", I);
				if (subjectsInPreviousUnquotedSection.size() && usePIS && currentSpeakerGroup < speakerGroups.size() && unresolvedSpeakers.size())
				{
					bool subjectDefinitelyResolved;
					if (questionSpeakerLastParagraph >= 0)
						correctBySpeakerInversionIfQuestion(I,whereFirstSubjectInParagraph);
					resolveSpeakersUsingPreviousSubject(I);
					if (questionSpeakerLastParagraph >= 0)
					{
						questionAgreement(I, whereFirstSubjectInParagraph, questionSpeakerLastParagraph, m[questionSpeakerLastParagraph].objectMatches, subjectDefinitelyResolved, false, L"QXQ");
						questionAgreement(I, whereFirstSubjectInParagraph, questionSpeakerLastParagraph, m[questionSpeakerLastParagraph].audienceObjectMatches, subjectDefinitelyResolved, true, L"QXQ");
					}
				}
				else
				{
					bool subjectDefinitelyResolved;
					if (questionSpeakerLastParagraph >= 0 && !questionAgreement(I, whereFirstSubjectInParagraph, questionSpeakerLastParagraph, m[questionSpeakerLastParagraph].objectMatches, subjectDefinitelyResolved, false, L"QXQNOSUBJECT"))
					{
						// correct?
					}
				}
				unresolvedSpeakers.clear();
				previousSpeakers.clear();
				beforePreviousSpeakers.clear();
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:cleared all unresolvedSpeaker positions (1)", I);
				subjectsInPreviousUnquotedSection = previousLastSubjects;
				whereSubjectsInPreviousUnquotedSection = wherePreviousLastSubjects;
				subjectsInPreviousUnquotedSectionUsableForImmediateResolution = true;
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:ZXZ set subjectsInPreviousUnquotedSection=previousLastSubjects=%s whereSubjectsInPreviousUnquotedSection=%d", I, objectString(subjectsInPreviousUnquotedSection, tmpstr).c_str(), whereSubjectsInPreviousUnquotedSection);
			}
			if (!inPrimaryQuote)
			{
				quotesSeenSinceLastSentence = false;
				quotesSeen = false;
			}
			questionSpeakerLastParagraph = questionSpeakerLastSentence;
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:QXQ whereFirstSubjectInParagraph,questionSpeakerLastSentence reset from %d,%d.", I, whereFirstSubjectInParagraph, questionSpeakerLastSentence);
			questionSpeakerLastSentence = -1;
			whereFirstSubjectInParagraph = -1;
		}
		// (CMREADME43)
		if (section == 0 && section < sections.size() && I == sections[0].begin)
		{
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:____________________BEGIN SECTION: %s___________________________", I, phraseString(I, sections[section].endHeader, tmpstr, true).c_str());
			beginSection = I;
			// if the speaker group does not span the section
			if (currentSpeakerGroup + 1 < speakerGroups.size() &&
				(speakerGroups[currentSpeakerGroup].sgBegin >= I || speakerGroups[currentSpeakerGroup].sgEnd <= (signed)sections[section + 1].endHeader))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:%02d     aging speakers End Of Section", I, section);
				for (vector <cLocalFocus>::iterator lfi = localObjects.begin(); lfi != localObjects.end();)
					if (speakerGroups[currentSpeakerGroup + 1].speakers.find(lfi->om.object) == speakerGroups[currentSpeakerGroup + 1].speakers.end())
						ageSpeaker(I, lfi, EOS_AGE);
					else
						lfi++;
			}
		}
		if (section + 1 < sections.size() && I == sections[section + 1].begin)
		{
			if (!quotesSeenSinceLastSentence &&
				subjectsInPreviousUnquotedSection.size() && usePIS && currentSpeakerGroup < speakerGroups.size() && unresolvedSpeakers.size())
				resolveSpeakersUsingPreviousSubject(I);
			lastDefiniteSpeaker = -1;
			setFlagAlternateResolveForwardAfterDefinite = false; // no definite speakers
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:Reset last definite speaker.", I);
			unresolvedSpeakers.clear();
			subjectsInPreviousUnquotedSection.clear();
			whereSubjectsInPreviousUnquotedSection = -1;
			whereSubject = wherePreviousLastSubjects = -1;
			previousLastSubjects.clear();
			lastSubjects.clear();
			if (debugTrace.traceSpeakerResolution)
				lplog(LOG_RESOLUTION, L"%06d:ZXZ reset subjectsInPreviousUnquotedSection, previousLastSubjects, lastSubjects etc", I);
			// if the speaker group does not span the section
			if (currentSpeakerGroup + 1 < speakerGroups.size() &&
				(speakerGroups[currentSpeakerGroup].sgBegin >= I || speakerGroups[currentSpeakerGroup].sgEnd <= (signed)sections[section + 1].endHeader))
			{
				if (debugTrace.traceSpeakerResolution)
					lplog(LOG_RESOLUTION, L"%06d:%02d     aging speakers End Of Section", I, section);
				for (vector <cLocalFocus>::iterator lfi = localObjects.begin(); lfi != localObjects.end();)
					if (speakerGroups[currentSpeakerGroup + 1].speakers.find(lfi->om.object) == speakerGroups[currentSpeakerGroup + 1].speakers.end())
						ageSpeaker(I, lfi, EOS_AGE);
					else
						lfi++;
			}
			section++;
			processNextSection(I, section - 1);
			beginSection = I;
			lastSense = -1;
		}
		while (currentSpeakerGroup + 1 < speakerGroups.size() && I == speakerGroups[currentSpeakerGroup + 1].sgBegin)
			ageIntoNewSpeakerGroup(I);
	}
	// (CMREADME44)
	int whereLastObject=-1;
	bool anySpaceRelation=false;
	for (int J=uqPreviousToLastSentenceEnd; J<uqLastSentenceEnd; J++)
	{
		detectSpaceRelation(J,m.size(),lastSubjects); // so that space relations use disambiguated objects
		detectSpaceLocation(J,lastBeginS1);
		evaluateMetaWhereQuery(J,false,currentMetaWhereQuery); // how to use currentIsQuestion when this is set below?
		if (m[J].getObject()>=0) whereLastObject=J;
		anySpaceRelation|=(m[J].spaceRelation) ;
	} 
	// the inQuestion flag is only set as the first word of the question or any object
	// if the sentence is one word or one object, make space relation one word.
	if (!anySpaceRelation && uqLastSentenceEnd-uqPreviousToLastSentenceEnd>0 && (uqLastSentenceEnd-uqPreviousToLastSentenceEnd<=2 || whereLastObject>=0))
	{
		if (whereLastObject<0) whereLastObject=uqPreviousToLastSentenceEnd;
		cSpaceRelation sr(whereLastObject,-1,-1,-1,-1,-1,-1,-1,-1,stNORELATION,false,false,-1,-1,true);
		if (lastOpeningPrimaryQuote>uqLastSentenceEnd)
			sr.tft.lastOpeningPrimaryQuote=m[lastOpeningPrimaryQuote].previousQuote;
		else
			sr.tft.lastOpeningPrimaryQuote=lastOpeningPrimaryQuote;
		bool comparesr( const cSpaceRelation &s1, const cSpaceRelation &s2 );
		vector <cSpaceRelation>::iterator location = lower_bound(spaceRelations.begin(), spaceRelations.end(), sr, comparesr);
		spaceRelations.insert(location,sr);
	}
  if (section<sections.size())
    processNextSection(m.size(),section);
	eraseAliasesAndReplacementsInEmbeddedSpeakerGroups();
  wprintf(L"PROGRESS: 100%% speakers resolved with %d seconds elapsed \n",clocksec());
  if (debugTrace.traceSpeakerResolution || TSROverride)
  {
    lplog(LOG_SG,L"SPEAKER GROUPS [LIST]");
    for (unsigned int I=0; I<speakerGroups.size(); I++)
		{
			translateBodyObjects(speakerGroups[I]);
      lplog(LOG_SG,L"%d: %s",I,toText(speakerGroups[I],tmpstr));
			for (unsigned int J=0; J<speakerGroups[I].embeddedSpeakerGroups.size(); J++)
				lplog(LOG_SG,L"   %d: %s",J,toText(speakerGroups[I].embeddedSpeakerGroups[J],tmpstr));
		}
  }
  for (vector <cSection>::iterator is=sections.begin(),isEnd=sections.end(); is!=isEnd; is++)
  {
    for (vector <cOM>::iterator mo=is->speakerObjects.begin(); mo!=is->speakerObjects.end(); mo++)
      followObjectChain(mo->object);
    for (vector <cOM>::iterator mo=is->objectsSpokenAbout.begin(); mo!=is->objectsSpokenAbout.end(); mo++)
      followObjectChain(mo->object);
    for (vector <cOM>::iterator mo=is->objectsInNarration.begin(); mo!=is->objectsInNarration.end(); mo++)
      followObjectChain(mo->object);
  }
}