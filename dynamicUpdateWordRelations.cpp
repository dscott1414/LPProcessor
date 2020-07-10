#include <windows.h>
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "source.h"
#include "time.h"
#include "vcXML.h"
#include "profile.h"

//
//  this is for dyncaliically updating word relations in DB which sources were being processed, which is no longer supported.
//  word relations should not be updating in DB normally because the source is in testing so word relations will be biased toward the weights of the word relations being tested.
//  It is a much better idea to periodically update word relation weights based on testing the entire corpus before, collecting and updating weights based on the most conservative
//  sentence parses (lowest cost) and then temporarily updating the word relations then testing the corpus statistics again and verifying that the parsing has improved (possibly by 
//  comparing against Stanford)
//

int Source::attachAdjectiveRelation(vector <tTagLocation> &tagSet, int whereObject)
{
	LFS
		int begin = m[whereObject].beginObjectPosition, end = m[whereObject].endObjectPosition;
	tIWMM modifiedWord = resolveToClass(whereObject);
	int nextAdjectiveTag = -1, adjectiveTag = findTagConstrained(tagSet, L"ADJ", nextAdjectiveTag, begin, end);
	while (adjectiveTag >= 0 && tagIsCertain(tagSet[adjectiveTag].sourcePosition))
	{
		tIWMM adjectiveWord = m[tagSet[adjectiveTag].sourcePosition].word;
		if (m[tagSet[adjectiveTag].sourcePosition].isPPN())
			adjectiveWord = Words.PPN;
		// not delayed because the arguments are not subject to enhanced resolution (not an object)
		addRelations(tagSet[adjectiveTag].sourcePosition, modifiedWord, adjectiveWord, WordWithAdjective);
		adjectiveTag = nextAdjectiveTag;
		nextAdjectiveTag = findTagConstrained(tagSet, L"ADJ", nextAdjectiveTag, begin, end);
	}
	return 0;
}

int Source::attachAdverbRelation(vector <tTagLocation> &tagSet, int verbTagIndex, tIWMM verbWord)
{
	LFS
		int nextAdverbTag = -1, adverbTag = findTagConstrained(tagSet, L"ADV", nextAdverbTag, tagSet[verbTagIndex]);
	tIWMM adverbWord;
	if (adverbTag >= 0 && tagIsCertain(tagSet[adverbTag].sourcePosition) && (adverbWord = resolveToClass(tagSet[adverbTag].sourcePosition)) != wNULL)
	{
		// not delayed because the arguments are not subject to enhanced resolution (not an object)
		addRelations(tagSet[adverbTag].sourcePosition, verbWord, adverbWord, VerbWithAdverb);
		if (nextAdverbTag >= 0 && tagIsCertain(tagSet[adverbTag].sourcePosition) && (adverbWord = resolveToClass(tagSet[nextAdverbTag].sourcePosition)) != wNULL)
			addRelations(tagSet[nextAdverbTag].sourcePosition, verbWord, adverbWord, VerbWithAdverb);
	}
	return 0;
}

void Source::resolveWordRelations()
{
	LFS
		unsigned int sni = 0;
	//logCache=0;
	for (int I = 0; I < (int)delayedWordRelations.size(); I += 4)
	{
		int where = delayedWordRelations[I];
		int fromWhere = delayedWordRelations[I + 1];
		int toWhere = delayedWordRelations[I + 2];
		int relationType = delayedWordRelations[I + 3];
		if (I > 4 &&
			where == delayedWordRelations[I - 4] &&
			fromWhere == delayedWordRelations[I - 3] &&
			toWhere == delayedWordRelations[I - 2] &&
			relationType == delayedWordRelations[I - 1])
			continue;
		//lplog(L"where=%d from=%d to=%d rt=%d",where,fromWhere,toWhere,relationType);
		while (sni + 1 < subNarratives.size() && subNarratives[sni + 1] <= where)
			sni++;
		// if not in narrative but in quotes, narrativeNum=-1
		// if in narrative and in quotes, narrativeNum=sni
		// if not in quotes, narrativeNum=0
		switch (relationType)
		{
		case PrepWithPWord:
			addRelations(where, m[fromWhere].word, fullyResolveToClass(toWhere), relationType);
			break;
		case AWordWithPrep:
		case WordWithPrep:
			addRelations(where, fullyResolveToClass(fromWhere), m[toWhere].word, relationType);
			break;
		case DirectWordWithIndirectWord:
			addRelations(where, fullyResolveToClass(fromWhere), fullyResolveToClass(toWhere), relationType);
			break;
		case VerbWithPrep:
			addRelations(where, m[fromWhere].getVerbME(fromWhere, 13, lastNounNotFound, lastVerbNotFound), m[toWhere].word, VerbWithPrep);
			break;
		case VerbWithIndirectWord:
		case VerbWithDirectWord:
		case NotVerbWithSubjectWord:
		case VerbWithSubjectWord:
			unsigned __int64 or = m[toWhere].objectRole;
			int narrativeNum = (or &(IN_PRIMARY_QUOTE_ROLE | IN_SECONDARY_QUOTE_ROLE)) ? -1 : 0;
			if (or &IN_EMBEDDED_STORY_OBJECT_ROLE) narrativeNum = (signed)sni;
			int subject = -1;
			if (m[fromWhere].relSubject >= 0)
			{
				subject = m[m[fromWhere].relSubject].getObject();
				if (m[m[fromWhere].relSubject].objectMatches.size() > 0)
					subject = m[m[fromWhere].relSubject].objectMatches[0].object;
			}
			tFI::cRMap::tIcRMap r = addRelations(where, m[fromWhere].getVerbME(fromWhere, 1, lastNounNotFound, lastVerbNotFound), fullyResolveToClass(toWhere), relationType);
			bool physicallyEvaluated;
			int pp = (toWhere >= 0 && m[toWhere].getObject() >= 0 && m[toWhere].beginObjectPosition >= 0 && m[m[toWhere].beginObjectPosition].principalWherePosition >= 0 &&
				(physicallyPresentPosition(toWhere, physicallyEvaluated) && physicallyEvaluated));
			relationHistory.push_back(cRelationHistory(r, toWhere, narrativeNum, subject, pp));
			break;
		}
	}
	delayedWordRelations.clear();
}

void Source::recordVerbTenseRelations(int where, int sense, int subjectObject, int whereVerb)
{
	LFS
		// just present, past or future
		int pureTense = (getSimplifiedTense(sense) & 7) / 2;
	// skip should/could verbs
	if (subjectObject >= 0 && !(sense&(VT_POSSIBLE | VT_VERB_CLAUSE)))
	{
		cLastVerbTenses *lastVerbTense = objects[subjectObject].lastVerbTenses;
		for (int objectLastTense = 0; objectLastTense < VERB_HISTORY; objectLastTense++, lastVerbTense++)
		{
			if (lastVerbTense->lastTense != pureTense ||
				lastVerbTense->lastVerb < 0 ||
				abs(whereVerb - lastVerbTense->lastVerb) < 200)
				break;
			// not delayed because the arguments are two verbs
			addRelations(where, m[whereVerb].getVerbME(where, 4, lastNounNotFound, lastVerbNotFound), m[lastVerbTense->lastVerb].getVerbME(where, 20, lastNounNotFound, lastVerbNotFound), VerbWithNext1MainVerbSameSubject + objectLastTense * 2);
		}
		lastVerbTense = objects[subjectObject].lastVerbTenses;
		memmove(lastVerbTense + 1, lastVerbTense, (VERB_HISTORY - 1) * sizeof(lastVerbTense[0]));
		lastVerbTense[0].lastVerb = whereVerb;
		lastVerbTense[0].lastTense = pureTense;
		// must have a subject, though doesn't have to match.
		for (int objectLastTense = 0; objectLastTense < VERB_HISTORY; objectLastTense++)
		{
			if (lastVerbTenses[objectLastTense].lastTense != pureTense ||
				lastVerbTenses[objectLastTense].lastVerb < 0 ||
				abs(whereVerb - lastVerbTenses[objectLastTense].lastVerb) < 200)
				break;
			// not delayed because the arguments are two verbs
			addRelations(where, m[whereVerb].getVerbME(where, 5, lastNounNotFound, lastVerbNotFound), m[lastVerbTenses[objectLastTense].lastVerb].getVerbME(where, 6, lastNounNotFound, lastVerbNotFound), VerbWithNext1MainVerb + objectLastTense * 2);
		}
		memmove(lastVerbTenses + 1, lastVerbTenses, (VERB_HISTORY - 1) * sizeof(lastVerbTenses[0]));
		lastVerbTenses[0].lastVerb = whereVerb;
		lastVerbTenses[0].lastTense = pureTense;
	}
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
tFI::cRMap::tIcRMap Source::addRelations(int where, tIWMM from, tIWMM to, int relationType)
{
	LFS
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
		from->second.changedSinceLastWordRelationFlush = true;
	to->second.changedSinceLastWordRelationFlush = true;
	if (debugTrace.traceRelations)
		lplog(L"%06d:TRQQQ %s:%s -> %s", where, getRelStr(relationType), from->first.c_str(), to->first.c_str());
	to->second.addRelation(where, getComplementaryRelationship(relationType), from);
	return from->second.addRelation(where, relationType, to);
}

void Source::addDelayedWordRelations(int where, int fromWhere, int toWhere, int relationType)
{
	LFS
		// word relations are no longer updated in the database dynamically.  But this code in the future may be used to update the relations periodically from a large mass of texts.
		delayedWordRelations.push_back(where);
	delayedWordRelations.push_back(fromWhere);
	delayedWordRelations.push_back(toWhere);
	delayedWordRelations.push_back(relationType);
}

