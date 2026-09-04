/*
	syntacticRelationGroups.cpp - cSyntacticRelationGroup (one SVO / location /
	                              question clause) plus SRG print/serialize helpers.

	Overview:
		A cSyntacticRelationGroup is the clause-level record built after
		pattern matching: where the controller/subject/verb/object/preps sit
		in m[], the semantic relationType, tense/flow flags (cTimeFlowTense),
		and question-answering extras.  This TU owns the constructors, the
		on-disk serialize/deserialize pair, source-index remapping used when
		a child source is spliced into a question, and the debug printers
		that dump an SRG as a labelled S/V/O string.  Adjective/adverb
		extractors (getWSAdjective, getOSAdjective, getMSAdverb, ...) pull
		modifiers off an object span or, when the object is missing, off the
		words immediately after the verb (typical of "How old is X?").

	Pipeline position:
		Stage 5+ (relations / time / QA).  Groups themselves are created in
		semanticRelations.cpp / timeRelations.cpp; this file is the type's
		implementation and the print/prep helpers those stages call.

	Key entry points:
		- cSyntacticRelationGroup constructors - live, cache, and remapped
		- write() / convertFlags() / convertToFlags() - binary cache
		- printSRG() - LOG_WHERE / LOG_QCHECK dump of one group
		- getAllPreps() / checkInsertPrep() - walk the relPrep chain
		- getSRIMinMax() - print window around the group
		- wrti() / wchr() / gmo() - word-string helpers for printers

	Key data structures / globals:
		- SRIDebugCounter - monotonically increasing id stamped into QCHECK
		  logs (process-wide, not reset per source)
		- NULLWORD (187) - sentinel lexicon index meaning "no adjective /
		  adverb / profession"

	Notes / gotchas:
		- wrti() returns tmpstr.c_str(); the caller must keep tmpstr alive
		  for the duration of the use (printSRG does this with a stack of
		  named wstrings).
		- write() always calls convertFlags(false,false,false,0): isQuestion /
		  inPrimaryQuote / inSecondaryQuote / questionFlags are convertFlags
		  parameters only, not cSyntacticRelationGroup member fields, so there
		  is no real per-SRG value being discarded here — quote/question
		  status is recovered from cWordMatch::flagInQuestion on the word
		  instead. skip and changeStateAdverb (the two bits packed right after
		  those three) are real member fields and both round-trip correctly:
		  write() packs the live values and the cache ctor's convertToFlags
		  unpacks them without being overwritten afterward.
		- The remapping ctor sets o = -1 and does not copy timeInfo,
		  description or tft.presType from the source SRG (those are
		  lpwstring/vector and default to empty, which is intentional --
		  positions were remapped into a different source, so old
		  presType/description text would not apply). changeStateAdverb,
		  speakerContinuation, printMin/printMax and the nonSemantic* match
		  flags are plain bool/int, not copied either, and are now explicitly
		  reset (previously left indeterminate).
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "time.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "vcXML.h"
#include "profile.h"
#include "mysqldb.h"
int SRIDebugCounter = 0;
#define NULLWORD 187

// Lexicon string at source position `where`, or u"" if where < 0.
const lpchar_t *cSource::wchr(int where)
{
	LFS
		return (where < 0) ? u"" : m[where].word->first.c_str();
}

// End of the object spanning `wo`, or wo itself if there is no wider object.
// Negative wo is returned unchanged (used as a printMax "missing" sentinel).
int cSource::gmo(int wo)
{
	LFS
		if (wo < 0) return wo;
	return (m[wo].endObjectPosition > wo) ? m[wo].endObjectPosition : wo;
}

// Format "[id whereString]" or "[id (whereString) class]" into tmpstr.
// Returns tmpstr.c_str() — caller must keep tmpstr alive.  Out-of-range
// where writes "Illegal!" into tmpstr.  where < 0 returns u"".
const lpchar_t *cSource::wrti(int where, const lpchar_t * id, lpwstring &tmpstr, bool shortFormat)
{
	LFS
	if (where < 0) return u"";
	if (where >= m.size())
	{
		tmpstr = u"Illegal!";
		return tmpstr.c_str();
	}
	lpwstring ws;
	whereString(where, ws, shortFormat);
	tIWMM w = fullyResolveToClass(where);
	lpwstring lcws = ws, lcw = w->first;
	std::transform(lcws.begin(), lcws.end(), lcws.begin(), ::tolower);
	std::transform(lcw.begin(), lcw.end(), lcw.begin(), ::tolower);
	if (w == wNULL || w->second.index < 0 || lcws.find(lcw) != lpwstring::npos)
		tmpstr = u"[" + lpwstring(id) + u" " + ws + u"]";
	else
		tmpstr = u"[" + lpwstring(id) + u" (" + ws + u") " + w->first + u"]";
	return tmpstr.c_str();
}

// get all prepositions, discard any prepositions that have attached objects as their head (subjects or objects of a verb) that are not the wo in question.
// Krugman earned his B.A. in economics from Yale University summa cum laude in 1974 and his PhD from the Massachusetts Institute of Technology (MIT) in 1977.
// please also see http://aclweb.org/anthology-new/J/J06/J06-3002.pdf
// 
// Collect the relPrep chains hanging off the verb, the group's wherePrep /
// whereSecondaryPrep, and (if wo >= 0) the object itself.  Drops preps whose
// attached object is not `wo` (see checkInsertPrep).  relPrep is a next-link;
// a repeated wp aborts the walk.
void cSource::getAllPreps(cSyntacticRelationGroup* srg, set <int> &relPreps, int wo)
{
	LFS
		if (srg->whereVerb >= 0 && srg->wherePrep != m[srg->whereVerb].relPrep)
			for (int wp = m[srg->whereVerb].relPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
				if (checkInsertPrep(relPreps, wp, wo) < 0)
					break;
	for (int wp = srg->wherePrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
		if (checkInsertPrep(relPreps, wp, wo) < 0)
			break;
	for (int wp = srg->whereSecondaryPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
		if (checkInsertPrep(relPreps, wp, wo) < 0)
			break;
	if (wo >= 0 && m[wo].relPrep >= 0)
	{
		for (int wp = m[wo].relPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
			if (checkInsertPrep(relPreps, wp, wo) < 0)
				break;
	}
}

// Append "prep adj* wherePrepObject: [PO ...]" (and up to 10 compound
// POC parts) onto ps.  No-op if wherePrep or its relObject is missing.
void cSource::prepPhraseToString(int wherePrep, lpwstring &ps)
{
	LFS
		if (wherePrep < 0) return;
	int wherePrepObject = m[wherePrep].getRelObject();
	if (wherePrepObject < 0) return;
	lpwstring tmpstr1, tmpstr2,ws;
	ps += wchr(wherePrep) + getWOSAdjective(wherePrepObject, tmpstr1) + u" " + getWSAdjective(wherePrepObject, 0) + u" " + getWSAdjective(wherePrepObject, 1) + u" " + itos(wherePrepObject,ws) + u":" + wrti(wherePrepObject, u"PO", tmpstr2);
	// compound nouns
	int compoundCount = 0;
	while (wherePrep >= 0 && wherePrepObject >= 0 && (wherePrepObject = m[wherePrepObject].nextCompoundPartObject) >= 0 && compoundCount++ < 10)
		ps += wchr(wherePrep) + getWOSAdjective(wherePrepObject, tmpstr1) + u" " + getWSAdjective(wherePrepObject, 0) + u" " + getWSAdjective(wherePrepObject, 1) + u" " + itos(wherePrepObject, ws) + u":" + wrti(wherePrepObject, u"POC", tmpstr2);
}

// Insert wp into relPreps unless already present or wp is past m[].
// When wo >= 0, also require nextQuote < 0 or nextQuote == wo so only
// preps attached to that object are kept.  Returns -1 to stop the walk,
// 0 to continue.
int cSource::checkInsertPrep(set <int> &relPreps, int wp, int wo)
{
	// wp is signed; every current caller already guards wp>=0 in its loop
	// condition, but compare against a same-signedness bound here so this
	// stays correct (rather than relying on unsigned wraparound) if that
	// ever changes.
	if (wp < 0 || wp >= (int)m.size() || relPreps.find(wp) != relPreps.end())
		return -1;
	if (wo < 0 || m[wp].nextQuote < 0 || m[wp].nextQuote == wo) // get only the prepositions attached to the wo
		relPreps.insert(wp);
	return 0;
}

// Lexicon index of a GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS at `object`,
// or of its single objectMatch.  NULLWORD (187) if object < 0 or not an
// occupation/role.
int cSource::getProfession(int object)
{
	LFS
		if (object < 0) return NULLWORD;
	int where = m[object].principalWherePosition;
	if (where < 0) return NULLWORD;
	if (objects[object].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS)
		return m[where].word->second.index;
	if (m[where].objectMatches.size() == 1 && objects[object = m[where].objectMatches[0].object].objectClass == GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS &&
		objects[object].originalLocation >= 0)
		return m[objects[object].originalLocation].word->second.index;
	return NULLWORD;
}

// Fill srg->printMin / printMax with the source-position window covering
// controller, subject, verb, object, secondary object and every prep in
// the wherePrep / whereSecondaryPrep chains (skipping transformedPrep).
void cSource::getSRIMinMax(cSyntacticRelationGroup* srg)
{
	LFS
		srg->printMin = 10000;
	srg->printMax = -1;
	if (srg->whereControllingEntity >= 0)
	{
		srg->printMin = min(srg->printMin, srg->whereControllingEntity);
		if (m[srg->whereControllingEntity].getObject() >= 0)
			srg->printMin = min(srg->printMin, m[srg->whereControllingEntity].beginObjectPosition);
	}
	if (srg->whereSubject >= 0)
	{
		srg->printMin = min(srg->printMin, srg->whereSubject);
		if (m[srg->whereSubject].getObject() >= 0)
			srg->printMin = min(srg->printMin, m[srg->whereSubject].beginObjectPosition);
	}
	if (srg->whereVerb >= 0) srg->printMin = min(srg->printMin, srg->whereVerb);
	if (srg->whereObject >= 0) srg->printMin = min(srg->printMin, srg->whereObject);

	srg->printMax = max(srg->printMax, gmo(srg->whereControllingEntity));
	srg->printMax = max(srg->printMax, gmo(srg->whereSubject));
	srg->printMax = max(srg->printMax, srg->whereVerb);
	srg->printMax = max(srg->printMax, gmo(srg->whereObject));
	if (srg->whereObject >= 0) srg->printMax = max(srg->printMax, gmo(m[srg->whereObject].nextCompoundPartObject));
	if (srg->whereVerb >= 0) srg->printMax = max(srg->printMax, m[srg->whereVerb].getRelVerb());
	if (srg->whereVerb >= 0 && m[srg->whereVerb].getRelVerb() >= 0) srg->printMax = max(srg->printMax, gmo(m[m[srg->whereVerb].getRelVerb()].getRelObject()));
	set <int> relPreps;
	for (int wp = srg->wherePrep; wp >= 0 && wp < (int)m.size(); wp = m[wp].relPrep)
	{
		if (relPreps.find(wp) != relPreps.end())
			break;
		relPreps.insert(wp);
	}
	for (int wp = srg->whereSecondaryPrep; wp >= 0 && wp + 1 < (int)m.size(); wp = m[wp].relPrep)
	{
		if (relPreps.find(wp) != relPreps.end())
			break;
		relPreps.insert(wp);
	}
	for (set <int>::iterator rp = relPreps.begin(), rpEnd = relPreps.end(); rp != rpEnd; rp++)
	{
		if (*rp != srg->transformedPrep)
			srg->printMin = min(srg->printMin, *rp);
		if (m[*rp].getRelObject() >= 0) srg->printMin = min(srg->printMin, m[*rp].getRelObject());
		if (*rp != srg->transformedPrep)
			srg->printMax = max(srg->printMax, *rp);
		srg->printMax = max(srg->printMax, gmo(m[*rp].getRelObject()));
		if (m[*rp].getRelObject() >= 0) srg->printMax = max(srg->printMax, gmo(m[m[*rp].getRelObject()].nextCompoundPartObject));
	}
	if (srg->whereQuestionType >= 0) srg->printMin = min(srg->printMin, srg->whereQuestionType);
}

// numOrder-th non-object adjective/noun/determiner inside the object span
// at `where` (acceptableAdjective && !acceptableObjectPosition).  Empty
// if where < 0 or the span is a single token.
lpwstring cSource::getWSAdjective(int where, int numOrder)
{
	LFS
		if (where < 0) return u"";
	int object = m[where].getObject();
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1 && where > 0)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
			if (acceptableAdjective(I) && !acceptableObjectPosition(I))
			{
				if (numOrder == 0)
					return m[I].word->first;
				else
					numOrder--;
			}
	}
	return u"";
}

// Object-id of the first adjectival object inside `where`, or — if where < 0 —
// of a post-verbal adjectival object (whereVerb+1, or +2 if +1 is an adverb).
// Returns -1 if none.
int cSource::getOSAdjective(int whereVerb, int where)
{
	LFS
		if (where >= 0)
			return getOSAdjective(where);
	if (whereVerb < 0) return -1;
	if (whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 1))
		return (m[whereVerb + 1].getObject() >= 0) ? m[whereVerb + 1].getObject() : m[whereVerb + 1].objectMatches[0].object;
	if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && m[whereVerb + 2].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 2))
		return (m[whereVerb + 2].getObject() >= 0) ? m[whereVerb + 2].getObject() : m[whereVerb + 2].objectMatches[0].object;
	return -1;
}

// whereString of the first adjectival object at `where`, else (if where < 0)
// of the post-verbal adjectival object next to whereVerb.  Writes into tmpstr.
lpwstring cSource::getWOSAdjective(int whereVerb, int where, lpwstring &tmpstr)
{
	LFS
		getWOSAdjective(where, tmpstr);
	if (tmpstr.empty() && where < 0 && whereVerb >= 0)
	{
		if (whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 1))
			return whereString(whereVerb + 1, tmpstr, false);
		if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && m[whereVerb + 2].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(whereVerb + 2))
			return whereString(whereVerb + 2, tmpstr, false);
	}
	return tmpstr;
}

// Lexicon index of the numOrder-th non-object adjective at `where`, or —
// if where < 0 and numOrder == 0 — of a post-verbal adjective / _Q2-I
// "What is X short for?" complement.  NULLWORD if none.
int cSource::getMSAdjective(int whereVerb, int where, int numOrder)
{
	LFS
		if (where >= 0)
			return getMSAdjective(where, numOrder);
	if (numOrder != 0) return NULLWORD;
	int i;
	// How old is Darrell Hammond?
	//if (where<0 && whereVerb>=0 && (m[whereVerb].flags&cWordMatch::flagInQuestion) && m[whereVerb].relSubject<0 &&
	//	  m[whereVerb-1].queryWinnerForm(adjectiveForm)>=0 && m[whereVerb-1].getObject()<0)
	//	return ((i=m[whereVerb-1].word->second.index)<0) ? NULLWORD : i;
	if (whereVerb < 0 || (whereVerb + 1 >= where && where >= 0)) return NULLWORD;
	int maxEnd, q2IElement = queryPattern(whereVerb, u"_Q2", maxEnd);
	if (q2IElement >= 0 && patterns[pema[q2IElement].getParentPattern()]->differentiator == u"I" && m[whereVerb].relSubject >= 0 && (i = m[m[whereVerb].relSubject].endObjectPosition) >= 0 &&
		(m[i].queryWinnerForm(adjectiveForm) >= 0 || (m[i].queryWinnerForm(quoteForm) >= 0 && i + 1 < (signed)m.size() && m[i = i + 1].queryWinnerForm(adjectiveForm) >= 0))) // What is "WWE" short for?
		return ((i = m[i].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb + 1 < (signed)m.size() && acceptableAdjective(whereVerb + 1) && !acceptableObjectPosition(whereVerb + 1))
		return ((i = m[whereVerb + 1].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && acceptableAdjective(whereVerb + 2) && !acceptableObjectPosition(whereVerb + 2))
		return ((i = m[whereVerb + 2].word->second.index) < 0) ? NULLWORD : i;
	return NULLWORD;
}

// Word string of the numOrder-th non-object adjective at `where`, falling
// back to a post-verbal / _Q2-I adjective when where < 0 and numOrder == 0.
lpwstring cSource::getWSAdjective(int whereVerb, int where, int numOrder, lpwstring &tmpstr)
{
	LFS
		tmpstr = getWSAdjective(where, numOrder);
	if (numOrder != 0)
		return tmpstr;
	if (tmpstr.empty() && where < 0 && whereVerb >= 0)
	{
		int maxEnd, i, q2IElement = queryPattern(whereVerb, u"_Q2", maxEnd);
		if (q2IElement >= 0 && patterns[pema[q2IElement].getParentPattern()]->differentiator == u"I" && m[whereVerb].relSubject >= 0 && (i = m[m[whereVerb].relSubject].endObjectPosition) >= 0 &&
			(m[i].queryWinnerForm(adjectiveForm) >= 0 || (m[i].queryWinnerForm(quoteForm) >= 0 && i + 1 < (signed)m.size() && m[i = i + 1].queryWinnerForm(adjectiveForm) >= 0))) // What is "WWE" short for?
			return m[i].word->first;
		if (whereVerb + 1 < (signed)m.size() && acceptableAdjective(whereVerb + 1) && !acceptableObjectPosition(whereVerb + 1))
			return m[whereVerb + 1].word->first;
		if (whereVerb + 2 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0 && acceptableAdjective(whereVerb + 2) && !acceptableObjectPosition(whereVerb + 2))
			return m[whereVerb + 2].word->first;
	}
	return tmpstr;
}

// Lexicon index of an adverb immediately after or before whereVerb.
// If changeStateAdverb, also accepts a T_START/STOP/FINISH/RESUME time
// word immediately before the verb.  NULLWORD / -1 if none.
int cSource::getMSAdverb(int whereVerb, bool changeStateAdverb)
{
	LFS
		int i = -1;
	if (whereVerb >= 0 && whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0)
		return ((i = m[whereVerb + 1].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb > 0 && m[whereVerb - 1].queryWinnerForm(adverbForm) >= 0)
		return ((i = m[whereVerb - 1].word->second.index) < 0) ? NULLWORD : i;
	if (whereVerb > 0 && changeStateAdverb)
	{
		int timeFlag = (m[whereVerb - 1].word->second.timeFlags & 31);
		if (timeFlag == T_START || timeFlag == T_STOP || timeFlag == T_FINISH || timeFlag == T_RESUME)
			return ((i = m[whereVerb - 1].word->second.index) < 0) ? NULLWORD : i;
	}
	return i;
}

// Word string of an adverb next to whereVerb (prefers before, then after).
// changeStateAdverb=true also accepts a T_START/STOP/FINISH/RESUME time
// word immediately before the verb, same filter as getMSAdverb.
const lpchar_t *cSource::getWSAdverb(int whereVerb, bool changeStateAdverb)
{
	LFS
		if (whereVerb > 0 && m[whereVerb - 1].queryWinnerForm(adverbForm) >= 0)
			return m[whereVerb - 1].word->first.c_str();
	if (whereVerb >= 0 && whereVerb + 1 < (signed)m.size() && m[whereVerb + 1].queryWinnerForm(adverbForm) >= 0)
		return m[whereVerb + 1].word->first.c_str();
	if (whereVerb > 0 && changeStateAdverb)
	{
		int timeFlag = (m[whereVerb - 1].word->second.timeFlags & 31);
		if (timeFlag == T_START || timeFlag == T_STOP || timeFlag == T_FINISH || timeFlag == T_RESUME)
			return m[whereVerb - 1].word->first.c_str();
	}
	return u"";
}

// True if `where` has objectMatches, or a multi-token / non-general /
// repeatedly-encountered object.  Used to tell "real" object adjectives
// from ordinary modifiers.
bool cSource::acceptableObjectPosition(int where)
{
	LFS
		return m[where].objectMatches.size() > 0 ||
		(m[where].getObject() >= 0 &&
		(m[where].endObjectPosition - m[where].beginObjectPosition > 1 || objects[m[where].getObject()].objectClass != NON_GENDERED_GENERAL_OBJECT_CLASS || objects[m[where].getObject()].numEncounters > 1));
}

// True if `where` won as adjective, noun, __ADJECTIVE, demonstrative /
// possessive determiner or quantifier — anything that can modify a noun.
bool cSource::acceptableAdjective(int where)
{
	LFS
		return m[where].queryWinnerForm(adjectiveForm) >= 0 || m[where].queryWinnerForm(nounForm) >= 0 || m[where].pma.queryPattern(u"__ADJECTIVE") != -1 ||
		m[where].queryWinnerForm(demonstrativeDeterminerForm) >= 0 || m[where].queryWinnerForm(possessiveDeterminerForm) >= 0 || m[where].queryWinnerForm(quantifierForm) >= 0;
}

// return the first adjective that is an object
// Object-id of that first adjectival object inside the span at `where`,
// or -1.  Prefers getObject() over objectMatches[0].
int cSource::getOSAdjective(int where)
{
	LFS
		if (where < 0) return -1;
	int object = m[where].getObject();
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1 && where > 0)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
			if (m[I].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(I))
				return (m[I].getObject() >= 0) ? m[I].getObject() : m[I].objectMatches[0].object;
	}
	return -1;
}

// whereString of the first adjectival object inside the span at `where`.
// Empty if where < 0 or none found.  Writes into tmpstr.
lpwstring cSource::getWOSAdjective(int where, lpwstring &tmpstr)
{
	LFS
		if (where < 0) return u"";
	int object = m[where].getObject();
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1 && where > 0)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
			if (m[I].principalWhereAdjectivalPosition >= 0 && acceptableObjectPosition(I))
				return whereString(I, tmpstr, false);
	}
	return u"";
}

// return the first adjective that is not an object
// Lexicon index of the numOrder-th such adjective inside the object span
// at `where`.  NULLWORD if where < 0 or none remain.
int cSource::getMSAdjective(int where, int numOrder)
{
	LFS
		if (where < 0) return NULLWORD;
	int object = m[where].getObject(), index;
	if (object >= 0 && m[where].endObjectPosition - m[where].beginObjectPosition > 1)
	{
		for (int I = m[where].beginObjectPosition; I < m[where].endObjectPosition - 1; I++)
		{
			if (acceptableAdjective(I) && !acceptableObjectPosition(I))
			{
				if (numOrder == 0)
					return ((index = m[I].word->second.index) < 0) ? NULLWORD : index;
				else
					numOrder--;
			}
		}
	}
	return NULLWORD;
}

// Overload: render prep `ps` via prepPhraseToString then call the lpwstring
// printSRG.  s/ws/wo are sentence / subject / object positions for the dump.
void cSource::printSRG(lpwstring logPrefix, cSyntacticRelationGroup* srg, int s, int ws, int wo, int ps, bool overWrote, int matchSum, lpwstring matchInfo, int logDestination)
{
	LFS
		lpwstring tmpstr;
	prepPhraseToString(ps, tmpstr);
	printSRG(logPrefix, srg, s, ws, wo, tmpstr, overWrote, matchSum, matchInfo, logDestination);
}

// One-line LOG dump of an SRG: relationType, controller, S/V/O adjectives,
// secondary object, next objects and the prep string.  matchSum >= 0 stamps
// SRIDebugCounter into the line (QCHECK omits the counter).  The format
// string's 34 specifiers match the 34 arguments below one-for-one.
void cSource::printSRG(lpwstring logPrefix, cSyntacticRelationGroup* srg, int s, int ws, int wo, lpwstring ps, bool overWrote, int matchSum, lpwstring matchInfo, int logDestination)
{
	LFS
		if (srg == NULL)
			return;
	lpwstring tmpstr, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6, tmpstr7, tmpstr8, tmpstr9, tmpstr10, tmpstr11, tmpstr12, tmpstr14;
	bool inQuestion = (srg->whereSubject >= 0 && (m[srg->whereSubject].flags&cWordMatch::flagInQuestion));
	inQuestion |= (srg->whereObject >= 0 && (m[srg->whereObject].flags&cWordMatch::flagInQuestion));
	//if (SRIDebugCounter==464)
	//{
	//	extern int logQuestionProfileTime,logSynonymDetail,logTableDetail,equivalenceLogDetail,logDetail;
	//	logQuestionProfileTime=logSynonymDetail=logTableDetail=equivalenceLogDetail=logDetail=1;
	//}
	if (matchSum >= 0)
	{
		itos(SRIDebugCounter++, tmpstr12);
		itos(matchSum, tmpstr14);
		if (logDestination&LOG_QCHECK)
			tmpstr14 = u"[" + tmpstr14 + u" " + matchInfo + u"]";
		else
			tmpstr14 = u"[" + tmpstr12 + u":" + tmpstr14 + u" " + matchInfo + u"]";
	}
	else if (s >= 0)
	{
		itos(s, tmpstr12);
		tmpstr14 = u"S" + tmpstr12;
	}
	const lpchar_t *tmp1 = 0, *tmp2 = 0, *tmp3 = 0, *tmp4 = 0, *tmp5 = 0, *tmp6 = 0;
	bool shortFormat = (logDestination&LOG_QCHECK) != 0;
	const lpchar_t *f1 = u"%s:%06d:%s%s%s %s %s%s %s %s %s %s %d:%s%s [V %d:%s]%s%s%s %s %s %d:%s%s%s%s%s%s%s%s%s%s%s";
	lplog(logDestination, f1, 
		logPrefix.c_str(), // 1
		srg->where, // 2
		tmpstr14.c_str(), // 3
		(srg->whereQuestionType < 0 && srg->relationType != stLOCATION && srg->relationType != -stLOCATION && srg->relationType != stNORELATION && inQuestion) ? u"***" : u"", //4
		(overWrote) ? u" OVERWRITE" : u"", // 5
		relationString(srg->relationType).c_str(),  // 6
		(srg->whereQuestionType >= 0) ? m[srg->whereQuestionType].word->first.c_str() : u"", // 7
		wrti(srg->whereControllingEntity, u"controller", tmpstr, shortFormat),  // 8
		(srg->whereControllingEntity < 0) ? u"" : wchr(m[srg->whereControllingEntity].getRelVerb()), // 9
		getWOSAdjective(ws, tmpstr2).c_str(), // 10
		getWSAdjective(ws, 0).c_str(), // 11
		getWSAdjective(ws, 1).c_str(), // 12
		ws, // 13
		wrti(ws, u"S", tmpstr3, shortFormat), // 14
		(srg->tft.negation) ? u"[NOT]" : u"", // 15
		srg->whereVerb, // 16
		wchr(srg->whereVerb), // 17
		tmp1 = getWSAdverb(srg->whereVerb, srg->changeStateAdverb), // 18
		tmp2 = getWOSAdjective(srg->whereVerb, wo, tmpstr4).c_str(), // 19
		tmp3 = getWSAdjective(srg->whereVerb, wo, 0, tmpstr5).c_str(), // 20
		tmp4 = getWSAdjective(srg->whereVerb, wo, 1, tmpstr6).c_str(), // 21
		tmp5 = getWSAdjective(srg->whereVerb, wo, 2, tmpstr7).c_str(), // 22
		wo, // 23
		tmp6 = wrti(wo, u"O", tmpstr8, shortFormat), // 24
		wchr(srg->whereSecondaryVerb), // 25
		getWOSAdjective(srg->whereSecondaryObject, tmpstr9).c_str(), // 26
		getWSAdjective(srg->whereSecondaryObject, 0).c_str(), // 27
		getWSAdjective(srg->whereSecondaryObject, 1).c_str(), // 28
		wrti(srg->whereSecondaryObject, u"O2", tmpstr10, shortFormat), // 29
		wrti(srg->whereNextSecondaryObject, u"nextObject2", tmpstr11, shortFormat), // 30
		(srg->objectSubType >= 0) ? OCSubTypeStrings[srg->objectSubType] : u"", // 31
		(srg->whereObject < 0) ? u"" : wrti(m[srg->whereObject].relNextObject, u"nextObject", tmpstr12, shortFormat), // 32
		ps.c_str(), //33
		(inQuestion) ? u"?" : u"."); // 34
}

// Live constructor: fill the SVO / location slots from the caller, zero the
// tense-flow / QA / print fields.  tft.presType (a lpwstring) is left at its
// default empty value; semanticRelations.cpp builds it up with += later.
cSyntacticRelationGroup::cSyntacticRelationGroup(int _where, int _o, int _whereControllingEntity, int _whereSubject, int _whereVerb, int _wherePrep, int _whereObject,
	int _wherePrepObject, int _movingRelativeTo, int _relationType,
	bool _genderedEntityMove, bool _genderedLocationRelation, int _objectSubType, int _prepObjectSubType, bool _physicalRelation)
{
	where = _where;
	o = _o;
	whereControllingEntity = _whereControllingEntity;
	whereSubject = _whereSubject;
	whereVerb = _whereVerb;
	wherePrep = _wherePrep;
	whereObject = _whereObject;
	wherePrepObject = _wherePrepObject;
	whereMovingRelativeTo = _movingRelativeTo;
	relationType = _relationType;
	genderedEntityMove = _genderedEntityMove;
	genderedLocationRelation = _genderedLocationRelation;
	objectSubType = _objectSubType;
	prepObjectSubType = _prepObjectSubType;
	physicalRelation = _physicalRelation;
	establishingLocation = false;
	futureLocation = false;
	tft.speakerCommand = false;
	tft.speakerQuestionToAudience = false;
	tft.significantRelation = false;
	tft.beyondLocalSpace = false; // movement beyond a room or a local area
	tft.story = false; // also includes future stories
	tft.timeTransition = false;
	tft.nonPresentTimeTransition = false;
	tft.duplicateTimeTransitionFromWhere = -1;
	tft.beforePastHappening = false;
	tft.pastHappening = false;
	tft.presentHappening = false;
	tft.futureHappening = false;
	tft.futureInPastHappening = false;
	tft.negation = false;
	agentLocationRelationSet = false;
	timeInfoSet = false;
	isConstructedRelative = false;
	prepositionUncertain = false;
	tft.lastOpeningPrimaryQuote = -1;
	nextSPR = -1;
	whereSecondaryVerb = -1;
	whereSecondaryObject = -1;
	whereNextSecondaryObject = -1;
	whereSecondaryPrep = -1;
	whereQuestionType = -1;
	questionType = 0;
	sentenceNum = -1;
	subQuery = false;
	skip = false;
	changeStateAdverb = false;
	nonSemanticObjectTotalMatch = false;
	nonSemanticPrepositionObjectTotalMatch = false;
	nonSemanticSecondaryObjectTotalMatch = false;
	nonSemanticSubjectTotalMatch = false;
	printMax = -1;
	printMin = -1;
	speakerContinuation = false;
	timeProgression = -1;
	whereQuestionTypeObject = -1;
	transformedPrep = -1;
	associatedPattern = NULL;
	mapPatternAnswer = NULL;
	mapPatternQuestion = NULL;
}

// Identity on the ten "where / o / relationType" slots only — tense, QA
// and secondary-verb fields are ignored.
bool operator != (const cSyntacticRelationGroup &lhs, const cSyntacticRelationGroup &rhs)
{
	return lhs.where != rhs.where ||
		lhs.o != rhs.o ||
		lhs.whereControllingEntity != rhs.whereControllingEntity ||
		lhs.whereSubject != rhs.whereSubject ||
		lhs.whereVerb != rhs.whereVerb ||
		lhs.wherePrep != rhs.wherePrep ||
		lhs.whereObject != rhs.whereObject ||
		lhs.wherePrepObject != rhs.wherePrepObject ||
		lhs.whereMovingRelativeTo != rhs.whereMovingRelativeTo ||
		lhs.relationType != rhs.relationType;
}

// Same ten-slot identity as operator !=.  Dual definitions exist in
// semanticRelations.h; keep them in lockstep.
bool operator == (const cSyntacticRelationGroup &lhs, const cSyntacticRelationGroup &rhs)
{
	return lhs.where == rhs.where &&
		lhs.o == rhs.o &&
		lhs.whereControllingEntity == rhs.whereControllingEntity &&
		lhs.whereSubject == rhs.whereSubject &&
		lhs.whereVerb == rhs.whereVerb &&
		lhs.wherePrep == rhs.wherePrep &&
		lhs.whereObject == rhs.whereObject &&
		lhs.wherePrepObject == rhs.wherePrepObject &&
		lhs.whereMovingRelativeTo == rhs.whereMovingRelativeTo &&
		lhs.relationType == rhs.relationType;
}

// True if `this` is the same clause as z except that this may fill in an
// o / wherePrep / wherePrepObject that z still has as "missing" (< 0).
bool cSyntacticRelationGroup::canUpdate(cSyntacticRelationGroup &z)
{
	return where == z.where &&
		(o == z.o || (z.o < 0 && o>0)) &&
		whereControllingEntity == z.whereControllingEntity &&
		whereSubject == z.whereSubject &&
		whereVerb == z.whereVerb &&
		(wherePrep == z.wherePrep || (z.wherePrep < 0 && wherePrep>0)) &&
		whereObject == z.whereObject &&
		(wherePrepObject == z.wherePrepObject || (z.wherePrepObject < 0 && wherePrepObject>0)) &&
		whereMovingRelativeTo == z.whereMovingRelativeTo &&
		relationType == z.relationType;
}

// Deserialize from the source-cache buffer at offset w (advanced on
// success).  error is set on a short read.  convertToFlags restores skip
// and changeStateAdverb from the packed word (they are not overwritten
// afterward).  QA / print / nonSemantic* fields are reset, not read.
cSyntacticRelationGroup::cSyntacticRelationGroup(char *buffer, int &w, unsigned int total, bool &error)
{
	if (error = !copy(where, buffer, w, total)) return;
	if (error = !copy(o, buffer, w, total)) return;
	if (error = !copy(whereControllingEntity, buffer, w, total)) return;
	if (error = !copy(whereSubject, buffer, w, total)) return;
	if (error = !copy(whereVerb, buffer, w, total)) return;
	if (error = !copy(wherePrep, buffer, w, total)) return;
	if (error = !copy(whereObject, buffer, w, total)) return;
	if (error = !copy(wherePrepObject, buffer, w, total)) return;
	if (error = !copy(whereSecondaryVerb, buffer, w, total)) return;
	if (error = !copy(whereSecondaryObject, buffer, w, total)) return;
	if (error = !copy(whereSecondaryPrep, buffer, w, total)) return;
	if (error = !copy(whereNextSecondaryObject, buffer, w, total)) return;
	if (error = !copy(whereMovingRelativeTo, buffer, w, total)) return;
	if (error = !copy(relationType, buffer, w, total)) return;
	if (error = !copy(objectSubType, buffer, w, total)) return;
	if (error = !copy(prepObjectSubType, buffer, w, total)) return;
	if (error = !copy(timeProgression, buffer, w, total)) return;
	if (error = !copy(questionType, buffer, w, total)) return;
	if (error = !copy(whereQuestionType, buffer, w, total)) return;
	if (error = !copy(sentenceNum, buffer, w, total)) return;
	// flags
	int64_t flags;
	if (error = !copy(flags, buffer, w, total)) return;
	convertToFlags(flags);
	if (error = !copy(tft.lastOpeningPrimaryQuote, buffer, w, total)) return;
	if (error = !copy(tft.duplicateTimeTransitionFromWhere, buffer, w, total)) return;
	if (error = !copy(tft.presType, buffer, w, total)) return;
	if (error = !copy(description, buffer, w, total)) return;
	if (error = !copy(nextSPR, buffer, w, total)) return;
	int count;
	if (error = !copy(count, buffer, w, total)) return;
	subQuery = false;
	timeInfo.reserve(count);
	while (count-- && !error && w < (signed)total)
		timeInfo.emplace_back(buffer, w, total, error);
	nonSemanticObjectTotalMatch = false;
	nonSemanticPrepositionObjectTotalMatch = false;
	nonSemanticSecondaryObjectTotalMatch = false;
	nonSemanticSubjectTotalMatch = false;
	transformedPrep = -1;
	printMax = -1;
	printMin = -1;
	speakerContinuation = false;
	whereQuestionTypeObject = -1;
	associatedPattern=NULL; // used only with question answering, particularly with verifying transformed questions
	mapPatternAnswer = NULL;
	mapPatternQuestion = NULL;

}

// Range-check every source-position and o.  Returns 0 if OK, or 400+
// identifying the first bad field.  wherePrep may be -2 (special "unset
// prep" used by some location relations); other positions allow -1.
int cSyntacticRelationGroup::sanityCheck(int maxSourcePosition, int maxObjectIndex)
{
	if (where < 0 || where >= maxSourcePosition) return 400;
	if (o < cObject::eOBJECTS::OBJECT_UNKNOWN_ALL || o >= maxObjectIndex) return 401;
	if (whereControllingEntity < -1 || whereControllingEntity >= maxSourcePosition) return 402;
	if (whereSubject < -1 || whereSubject >= maxSourcePosition) return 403;
	if (whereVerb < -1 || whereVerb >= maxSourcePosition) return 404;
	if (wherePrep < -2 || wherePrep >= maxSourcePosition) return 405;
	if (whereObject < -1 || whereObject >= maxSourcePosition) return 406;
	if (wherePrepObject < -1 || wherePrepObject >= maxSourcePosition) return 407;
	if (whereSecondaryVerb < -1 || whereSecondaryVerb >= maxSourcePosition) return 408;
	if (whereSecondaryObject < -1 || whereSecondaryObject >= maxSourcePosition) return 409;
	if (whereSecondaryPrep < -1 || whereSecondaryPrep >= maxSourcePosition) return 410;
	if (whereNextSecondaryObject < -1 || whereNextSecondaryObject >= maxSourcePosition) return 411;
	if (whereMovingRelativeTo < -1 || whereMovingRelativeTo >= maxSourcePosition) return 412;
	if (relationType >= stLAST) return 413;
	//if (objectSubType >= OCSubType::NUM_SUBTYPES) return false;
	//if (prepObjectSubType >= OCSubType::NUM_SUBTYPES) return false;
	if (whereQuestionType < -1 || whereQuestionType >= maxSourcePosition) return 414;
	return 0;
}

// Unpack the packed flag word written by convertFlags.  The first three
// bits (inSecondaryQuote / inPrimaryQuote / isQuestion) are discarded —
// write() always persists them as 0.  The top 6 questionFlags bits are
// never unpacked.
void cSyntacticRelationGroup::convertToFlags(int64_t flags)
{
	/*bool inSecondaryQuote=flags&1; */flags >>= 1;
	/*bool inPrimaryQuote=flags&1;*/ flags >>= 1;
	/*bool isQuestion=flags&1;*/ flags >>= 1;
	changeStateAdverb = flags & 1; flags >>= 1;
	skip = flags & 1; flags >>= 1;
	physicalRelation = flags & 1; flags >>= 1;
	timeInfoSet = flags & 1; flags >>= 1;
	agentLocationRelationSet = flags & 1; flags >>= 1;
	tft.negation = flags & 1; flags >>= 1;
	tft.futureInPastHappening = flags & 1; flags >>= 1;
	tft.futureHappening = flags & 1; flags >>= 1;
	tft.presentHappening = flags & 1; flags >>= 1;
	tft.pastHappening = flags & 1; flags >>= 1;
	tft.beforePastHappening = flags & 1; flags >>= 1;
	tft.nonPresentTimeTransition = flags & 1; flags >>= 1;
	tft.timeTransition = flags & 1; flags >>= 1;
	tft.story = flags & 1; flags >>= 1;
	tft.beyondLocalSpace = flags & 1; flags >>= 1;
	tft.significantRelation = flags & 1; flags >>= 1;
	tft.speakerQuestionToAudience = flags & 1; flags >>= 1;
	tft.speakerCommand = flags & 1; flags >>= 1;
	speakerContinuation = flags & 1; flags >>= 1;
	futureLocation = flags & 1; flags >>= 1;
	establishingLocation = flags & 1; flags >>= 1;
	genderedLocationRelation = flags & 1; flags >>= 1;
	genderedEntityMove = flags & 1;
	// 6 questionFlags bits (see convertFlags)
}

// Pack gendered/location/tense/skip/changeStateAdverb plus the three
// quote/question bools and questionFlags<<6 into the on-disk flag word.
int64_t cSyntacticRelationGroup::convertFlags(bool isQuestion, bool inPrimaryQuote, bool inSecondaryQuote, int64_t questionFlags)
{
	int64_t flags = (questionFlags << 6);
	flags |= (genderedEntityMove) ? 1 : 0; flags <<= 1;
	flags |= (genderedLocationRelation) ? 1 : 0; flags <<= 1;
	flags |= (establishingLocation) ? 1 : 0; flags <<= 1;
	flags |= (futureLocation) ? 1 : 0; flags <<= 1;
	flags |= (speakerContinuation) ? 1 : 0; flags <<= 1;
	flags |= (tft.speakerCommand) ? 1 : 0; flags <<= 1;
	flags |= (tft.speakerQuestionToAudience) ? 1 : 0; flags <<= 1;
	flags |= (tft.significantRelation) ? 1 : 0; flags <<= 1;
	flags |= (tft.beyondLocalSpace) ? 1 : 0; flags <<= 1;
	flags |= (tft.story) ? 1 : 0; flags <<= 1;
	flags |= (tft.timeTransition) ? 1 : 0; flags <<= 1;
	flags |= (tft.nonPresentTimeTransition) ? 1 : 0; flags <<= 1;
	flags |= (tft.beforePastHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.pastHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.presentHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.futureHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.futureInPastHappening) ? 1 : 0; flags <<= 1;
	flags |= (tft.negation) ? 1 : 0; flags <<= 1;
	flags |= (agentLocationRelationSet) ? 1 : 0; flags <<= 1;
	flags |= (timeInfoSet) ? 1 : 0; flags <<= 1;
	flags |= (physicalRelation) ? 1 : 0; flags <<= 1;
	flags |= (skip) ? 1 : 0; flags <<= 1;
	flags |= (changeStateAdverb) ? 1 : 0; flags <<= 1;
	flags |= (isQuestion) ? 1 : 0; flags <<= 1;
	flags |= (inPrimaryQuote) ? 1 : 0; flags <<= 1;
	flags |= (inSecondaryQuote) ? 1 : 0;
	return flags;
}

// Serialize in the same field order as the buffer ctor.  Always writes
// convertFlags(false,false,false,0); questions are recovered from
// cWordMatch::flagInQuestion, not from these bits.  Returns false if the
// copy helper ran out of room.
bool cSyntacticRelationGroup::write(void *buffer, int &w, int limit)
{
	if (!copy(buffer, where, w, limit)) return false;
	if (!copy(buffer, o, w, limit)) return false;
	if (!copy(buffer, whereControllingEntity, w, limit)) return false;
	if (!copy(buffer, whereSubject, w, limit)) return false;
	if (!copy(buffer, whereVerb, w, limit)) return false;
	if (!copy(buffer, wherePrep, w, limit)) return false;
	if (!copy(buffer, whereObject, w, limit)) return false;
	if (!copy(buffer, wherePrepObject, w, limit)) return false;
	if (!copy(buffer, whereSecondaryVerb, w, limit)) return false;
	if (!copy(buffer, whereSecondaryObject, w, limit)) return false;
	if (!copy(buffer, whereSecondaryPrep, w, limit)) return false;
	if (!copy(buffer, whereNextSecondaryObject, w, limit)) return false;
	if (!copy(buffer, whereMovingRelativeTo, w, limit)) return false;
	if (!copy(buffer, relationType, w, limit)) return false;
	if (!copy(buffer, objectSubType, w, limit)) return false;
	if (!copy(buffer, prepObjectSubType, w, limit)) return false;
	if (!copy(buffer, timeProgression, w, limit)) return false;
	if (!copy(buffer, questionType, w, limit)) return false;
	if (!copy(buffer, whereQuestionType, w, limit)) return false;
	if (!copy(buffer, sentenceNum, w, limit)) return false;
	// flags
	int64_t flags = convertFlags(false, false, false, 0); // while on disk, questions can be queried by the flag cWordMatch::inQuestion
	if (!copy(buffer, flags, w, limit)) return false;
	if (!copy(buffer, tft.lastOpeningPrimaryQuote, w, limit)) return false;
	if (!copy(buffer, tft.duplicateTimeTransitionFromWhere, w, limit)) return false;
	if (!copy(buffer, tft.presType, w, limit)) return false;
	if (!copy(buffer, description, w, limit)) return false;
	if (!copy(buffer, nextSPR, w, limit)) return false;
	unsigned int count = timeInfo.size();
	if (!copy((void *)buffer, count, w, limit)) return false;
	for (unsigned int I = 0; I < count; I++)
		if (!timeInfo[I].write(buffer, w, limit)) return false;
	return true;
}

// Remap a source position through sourceIndexMap (used when a child
// source is copied into a question).  originalVal < 0 is copied as-is
// (values < -1 are logged as illegal).  Unmapped positives are left
// unchanged and logged.  Returns true only on a successful map hit.
bool cSyntacticRelationGroup::adjustValue(int& val, int originalVal, lpwstring valString, unordered_map <int, int>& sourceIndexMap)
{
	if (originalVal < 0)
	{
		if (originalVal <-1)
			lplog(LOG_WHERE|LOG_ERROR, u"Illegal value for translating %s - %d.", valString.c_str(), originalVal);
		val = originalVal;
		return false;
	}
	if (sourceIndexMap.find(originalVal) != sourceIndexMap.end())
	{
		if (sourceIndexMap[originalVal] != originalVal)
			lplog(LOG_WHERE, u"Translated %s from %d to %d.", valString.c_str(), originalVal, sourceIndexMap[originalVal]);
		val = sourceIndexMap[originalVal];
		return true;
	}
	else
	{
		lplog(LOG_WHERE, u"Unable to translate %s of %d.", valString.c_str(), originalVal);
		val = originalVal;
	}
	return false;
}

// Copy srg with every source position rewritten through sourceIndexMap.
// o is forced to -1; timeInfo, description, tft.presType, changeStateAdverb,
// speakerContinuation and the nonSemantic* flags are not copied.
cSyntacticRelationGroup::cSyntacticRelationGroup(cSyntacticRelationGroup *srg, unordered_map <int, int> &sourceIndexMap)
{
	o = -1;
	adjustValue(where, srg->where, u"where", sourceIndexMap);
	adjustValue(whereControllingEntity, srg->whereControllingEntity, u"whereControllingEntity", sourceIndexMap);
	adjustValue(whereSubject, srg->whereSubject, u"whereSubject", sourceIndexMap);
	adjustValue(whereVerb, srg->whereVerb, u"whereVerb", sourceIndexMap);
	adjustValue(wherePrep, srg->wherePrep, u"wherePrep", sourceIndexMap);
	adjustValue(whereObject, srg->whereObject, u"whereObject", sourceIndexMap);
	adjustValue(wherePrepObject, srg->wherePrepObject, u"wherePrepObject", sourceIndexMap);
	adjustValue(whereMovingRelativeTo, srg->whereMovingRelativeTo, u"whereMovingRelativeTo", sourceIndexMap);
	adjustValue(whereSecondaryVerb, srg->whereSecondaryVerb, u"whereSecondaryVerb", sourceIndexMap);
	adjustValue(whereSecondaryObject, srg->whereSecondaryObject, u"whereSecondaryObject", sourceIndexMap);
	adjustValue(whereNextSecondaryObject, srg->whereNextSecondaryObject, u"whereNextSecondaryObject", sourceIndexMap);
	adjustValue(whereSecondaryPrep, srg->whereSecondaryPrep, u"whereSecondaryPrep", sourceIndexMap);
	adjustValue(whereQuestionType, srg->whereQuestionType, u"whereQuestionType", sourceIndexMap);
	adjustValue(whereQuestionTypeObject, srg->whereQuestionTypeObject, u"whereQuestionTypeObject", sourceIndexMap);
	for (int wo : srg->whereQuestionInformationSourceObjects)
	{
		int whereQuestionInformationSourceObject;
		if (adjustValue(whereQuestionInformationSourceObject, wo, u"whereQuestionInformationSourceObject", sourceIndexMap))
			whereQuestionInformationSourceObjects.insert(sourceIndexMap[wo]);
	}
	adjustValue(transformedPrep, srg->transformedPrep, u"transformedPrep", sourceIndexMap);

	relationType = srg->relationType;
	genderedEntityMove = srg->genderedEntityMove;
	genderedLocationRelation = srg->genderedLocationRelation;
	objectSubType = srg->objectSubType;
	prepObjectSubType = srg->prepObjectSubType;
	physicalRelation = srg->physicalRelation;
	prepositionUncertain = srg->prepositionUncertain;
	tft.speakerCommand = srg->tft.speakerCommand;
	tft.speakerQuestionToAudience = srg->tft.speakerQuestionToAudience;
	tft.significantRelation = srg->tft.significantRelation;
	tft.beyondLocalSpace = srg->tft.beyondLocalSpace;
	tft.story = srg->tft.story;
	tft.timeTransition = srg->tft.timeTransition;
	tft.nonPresentTimeTransition = srg->tft.nonPresentTimeTransition;
	tft.duplicateTimeTransitionFromWhere = srg->tft.duplicateTimeTransitionFromWhere;
	tft.beforePastHappening = srg->tft.beforePastHappening;
	tft.pastHappening = srg->tft.pastHappening;
	tft.presentHappening = srg->tft.presentHappening;
	tft.futureHappening = srg->tft.futureHappening;
	tft.futureInPastHappening = srg->tft.futureInPastHappening;
	tft.negation = srg->tft.negation;
	tft.lastOpeningPrimaryQuote = srg->tft.lastOpeningPrimaryQuote;
	establishingLocation = srg->establishingLocation;
	futureLocation = srg->futureLocation;
	agentLocationRelationSet = srg->agentLocationRelationSet;
	timeInfoSet = srg->timeInfoSet;
	isConstructedRelative = srg->isConstructedRelative;
	nextSPR = srg->nextSPR;
	questionType = srg->questionType;
	sentenceNum = srg->sentenceNum;
	subQuery = srg->subQuery;
	skip = srg->skip;
	timeProgression = srg->timeProgression;
	associatedPattern = srg->associatedPattern; // used only with question answering, particularly with verifying transformed questions
	mapPatternAnswer = srg->mapPatternAnswer;
	mapPatternQuestion = srg->mapPatternQuestion;
	// Not copied from srg (see the comment above): description, tft.presType
	// and timeInfo are lpwstring/vector and safely default to empty/empty, but
	// these six were plain bool/int and were left truly indeterminate --
	// initialize them explicitly instead of inheriting stack garbage.
	printMin = -1;
	printMax = -1;
	speakerContinuation = false;
	changeStateAdverb = false;
	nonSemanticSubjectTotalMatch = false;
	nonSemanticObjectTotalMatch = false;
	nonSemanticSecondaryObjectTotalMatch = false;
	nonSemanticPrepositionObjectTotalMatch = false;
}
