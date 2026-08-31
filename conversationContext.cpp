/*
	conversationContext.cpp - unfinished conversation-span detector (cSource::identifyConversations)

	Overview:
		Intended to walk the quote chain (firstQuote / nextQuote) and group quotes
		that share a speaker-group and at least two participants into
		"conversations", then build a three-level coherence map (exact word,
		WordNet/VerbNet synonym, word-relation).  Only the grouping predicates are
		sketched; the maps are allocated and never filled, and nothing is stored
		on cSource.

	Pipeline position:
		Would run after resolveSpeakers (stage 7).  Not called from the current
		main loop (the adjacency-pair comments are a design note, not code).

	Key entry points:
		- identifyConversations() - walk quotes; currently a stub that only
			counts conversations locally.

	Key data structures / globals:
		- cCohereInfo - intended (lastLocation, age, occurrence) value type for
			the coherence maps; members are private (class default) and unused.

	Notes / gotchas:
		- The "share at least two people" test copy-pastes the same
			intersect(...) condition twice, so the second clause is dead.  The
			obvious intent was to also compare against previousQuote's audience.
		- Embedded-story quotes (flagEmbeddedStoryResolveSpeakers without Begin)
			are skipped.
		- currentSpeakerGroup is a cSource member; this mutates it as a cursor.
*/
#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include "math.h"
#include "profile.h"

// Per-word coherence accumulator (unused).  Members are private and there is
// no constructor, so a default-constructed instance has uninitialized ints.
class cCohereInfo
{
	int lastLocation;
	int age;
	int occurrence;
};

// Walk the quote chain and count multi-party conversations (quotes whose
// speaker and audience object-match lists are not equal, and that do not
// share two participants with the previous quote / speaker group).
// The coherence maps below are never populated; the function has no
// observable effect besides advancing currentSpeakerGroup.
// this is to identify all conversations in the source and to trace all nouns/verbs/adjectives/adverbs through exact match, synonyms, and relations.
// a conversation is currently defined as being between more than one person.
void cSource::identifyConversations()
{
	LFS
		int numQuotes = 0, numQuotesInConversations = 0, numConversations = 0;
	currentSpeakerGroup = 0;
	bool allIn, oneIn;
	for (int I = firstQuote; I >= 0; I = m[I].nextQuote, numQuotes++)
	{
		// is this an extended embedded story?  if so, skip.
		if ((m[I].flags & cWordMatch::flagEmbeddedStoryResolveSpeakers) && !(m[I].flags & cWordMatch::flagEmbeddedStoryBeginResolveSpeakers))
			continue;
		// does this quote refer to more than one person (speaker and audience)?  if not, skip, this is not a conversation between two or more people.
		if (m[I].audienceObjectMatches.empty() || m[I].objectMatches.empty() || (intersect(m[I].audienceObjectMatches, m[I].objectMatches, allIn, oneIn) && allIn))
			continue;
		numQuotesInConversations++;
		// did the last quote and current quote belong to a different speaker group?  if so, this is a different conversation.
		if (currentSpeakerGroup + 1 < speakerGroups.size() && speakerGroups[currentSpeakerGroup + 1].sgBegin < I)
		{
			while (currentSpeakerGroup + 1 < speakerGroups.size() && speakerGroups[currentSpeakerGroup + 1].sgBegin < I)
				currentSpeakerGroup++;
			numConversations++;
			continue;
		}
		// did the last quote and current quote share at least two people? if not, this is a different conversation.
		int previousQuote = m[I].previousQuote;
		if (previousQuote >= 0 &&
			!(intersect(m[I].objectMatches, m[previousQuote].objectMatches, allIn, oneIn) || intersect(m[I].audienceObjectMatches, m[previousQuote].audienceObjectMatches, allIn, oneIn)))
		{
			numConversations++;
			continue;
		}
		// compute adjacency pair
		// question-answer 				
		// greeting-greeting 				
		// leave taking-leave taking
		// congratulations-thanks		
		// complaint-excuse/remedy/Denial/Apology
		// offer/invitation/request-acceptance/denial/rejection		
		// apology-acceptance
		// inform-acknowledge
		//
		// a. noun/verb/adj/adv - exact match?
		// b. synonyms through verbnet/wordnet
		// c. relations

		// first level map word 
		map<tIWMM, cCohereInfo> coherenceWords;
		// second level map noun/adjective/adverb synonyms and verb verbNet classes
		map<tIWMM, cCohereInfo> coherenceSecondary;
		// third level map 
		// this maps verb/noun/adj/adv relations
		// rich - millionaire/ rich - money

	}
	//lplog(L"numQuotes=%d. numQuotesInConversations=%d. numConversations=%d.",numQuotes,numQuotesInConversations,numConversations);
}