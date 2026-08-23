/*
	dbQuerySearch.cpp - thin QA wrappers that dispatch ownership / fact search to MusicBrainz

	Overview:
		Two cQuestionAnswering methods that look like generic DB lookups but
		immediately forward to the MusicBrainz-specific implementations
		(matchOwnershipDbMusicBrainz / dbSearchMusicBrainz).  Kept as a separate
		TU so a later generic DB backend can be swapped in without touching QA.

	Pipeline position:
		Stage 8.  Called from question answering when the question is an
		ownership / "what did X record" style fact.

	Key entry points:
		- matchOwnershipDbQuery() - true if parentSRG's ownership can be answered.
		- dbSearchForQuery() - fill answerSRGs from the DB; true if any hit.

	Notes / gotchas:
		- Both bodies are one-line forwards.  The MusicBrainz implementations
			live in getMusicBrainz.cpp (out of this assignment).
*/
#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <winsock.h>
#include "Winhttp.h"
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "time.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "profile.h"
#include "QuestionAnswering.h"

#include <typeinfo>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
using namespace std;

// True if parentSRG describes an ownership relation that MusicBrainz can
// answer (artist-recording, etc.).  derivation is an in/out trace string.
bool cQuestionAnswering::matchOwnershipDbQuery(cSource* questionSource, wchar_t* derivation, cSyntacticRelationGroup* parentSRG)
{
	LFS
		return matchOwnershipDbMusicBrainz(questionSource, derivation, parentSRG);
}

// Search the (MusicBrainz) DB for answers to parentSRG and append them to
// answerSRGs.  Returns true if at least one answer was added.
bool cQuestionAnswering::dbSearchForQuery(cSource* questionSource, wchar_t* derivation, cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs)
{
	LFS
		return dbSearchMusicBrainz(questionSource, derivation, parentSRG, answerSRGs);
}