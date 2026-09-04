/*
	conversationContext.cpp - unfinished conversation-span detector (cSource::identifyConversations)

	Overview:
		Walks the quote chain (firstQuote / nextQuote) and counts quotes that
		share a speaker-group and at least two participants as
		"conversations".  The three-level coherence map (exact word,
		WordNet/VerbNet synonym, word-relation) and the adjacency-pair
		classification described in the comments below were never
		implemented - only the grouping predicates are; nothing is stored on
		cSource and the counts are local to the function (see the trailing
		commented-out lplog).

	Pipeline position:
		Runs after resolveSpeakers (stage 7): main.cpp calls
		source.identifyConversations() once per source, right after
		resolveFirstSecondPersonPronouns/resolveSpeakers.

	Key entry points:
		- identifyConversations() - walk quotes; currently a stub that only
			counts conversations locally.

	Notes / gotchas:
		- The "share at least two people" test compares current vs previous
			quote's objectMatches (speakers) in one clause and current vs
			previous quote's audienceObjectMatches in the other (previously both
			clauses copy-pasted the objectMatches comparison, so the audience
			was never consulted; fixed).
		- Embedded-story quotes (flagEmbeddedStoryResolveSpeakers without Begin)
			are skipped.
		- currentSpeakerGroup is a cSource member; this mutates it as a cursor.
*/
// Batch B5: the Win32-only includes that used to head this file (windows.h and
// friends) are gone; these are what the code below actually needs on macOS.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "time.h"
#include "math.h"
#include "profile.h"

// Walk the quote chain and count multi-party conversations (quotes whose
// speaker and audience object-match lists are not equal, and that do not
// share two participants with the previous quote / speaker group).  The
// counts are local; the function has no observable effect besides
// advancing currentSpeakerGroup.
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
		//
		// intended: a first-level map of word -> (lastLocation, age, occurrence) for
		// exact matches, a second-level map for noun/adjective/adverb synonyms and verb
		// verbNet classes, and a third-level map for verb/noun/adj/adv relations
		// (rich - millionaire / rich - money).  None of the three maps were ever
		// implemented, so the never-populated cCohereInfo-keyed locals that used to
		// sit here were removed as dead code; this comment records the design intent.
	}
	//lplog(u"numQuotes=%d. numQuotesInConversations=%d. numConversations=%d.",numQuotes,numQuotesInConversations,numConversations);
}
