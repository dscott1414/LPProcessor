/*
	DBCreateSQLSchema.cpp - CREATE TABLE / CREATE DATABASE and seed-source generators

	Overview:
		createDatabase() is the one-shot installer: connect (or create schema
		`lp`), then CREATE the forms/sources/words/wordForms/wordFrequency/
		noRDFTypes tables and the object/relation family.  The other functions
		are CREATE TABLE helpers for thesaurus/groups/locations/time, plus
		INSERT generators that seed `sources` from the BNC index, NewsBank day
		numbers, the tests\\ directory, and QA parse-request paths.

	Pipeline position:
		Run once from main when the schema is missing.  generate*Sources is also
		called from that path.  Thesaurus/group/location/time CREATEs are
		orphaned (author notes they "do not exist within the database").

	Key entry points:
		- createDatabase() - full install.
		- createObjectTables / createRelationTables / createTimeRelationTables.
		- generateBNCSources / generateNewsBankSources / generateTestSources.
		- generateParseRequestSources / deleteGeneratedParseRequests.
		- writeThesaurusEntry / createThesaurusTables.
		- insertWordRelationTypes.

	Dependencies:
		MySQL via envConfig.h's getDBUser()/getDBPassword().  source\\lists\\bookSources.sql
		is slurped into the sources table.  BNC index at
		LMAINDIR\\BNC-world\\doc\\Source\\bncIndex.xml.

	Notes / gotchas:
		- createDatabase connects with the same getDBUser()/getDBPassword() call
			initializeDatabaseHandle() uses.
		- Several CREATE TABLE strings were syntactically invalid (trailing
			commas; FK column names that did not match the column; duplicate
			index names; FKs to tables that were created later, or never
			created at all).  createTimeRelationTables, createObjectTables, and
			createRelationTables have all been fixed; see the comments on each.
			createGroupTables's two remaining defects (no index on the column
			its children FK to, and an FK naming a groups.id column that does
			not exist) are now fixed as well; createLocationTables and
			createThesaurusTables were re-checked and are valid as they stand.
			All three are still uncalled from anywhere in the tree (author
			note: "these do not exist within the database").
		- generateBNCSources allocates actualLen+sizeof(lpchar_t) bytes and
			NUL-terminates at buffer[actualLen/sizeof(lpchar_t)] - in range.
		- writeThesaurusEntry builds the statement in a lpwstring (no fixed-size
			stack buffer), escaping each list entry before appending it.
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
#include "profile.h"
#include "mysqldb.h"
#include "QuestionAnswering.h"

bool checkFull(MYSQL* mysql, lpchar_t* qt, size_t& len, bool flush, lpchar_t* qualifier);

//maxSynonymAccumulatedSize = 552
//maxAntonymAccumulatedSize = 106
//maxPrimarySynonymAccumulatedSize = 86
//maxConceptSize = 35

// CREATE TABLE thesaurus (mainEntry, wordType bitmap, synonym/antonym/concept
// blobs).  Returns -1 if the CREATE fails (table-already-exists is FATAL
// unless the caller used allowFailure).
int cSource::createThesaurusTables()
{
	if (!myquery(&mysql, u"CREATE TABLE thesaurus ("
		u"mainEntry CHAR(32) CHARACTER SET utf8mb4 NOT NULL, INDEX me_ind (mainEntry), "
		u"wordType SMALLINT UNSIGNED NOT NULL,"
		u"primarySynonyms CHAR(100) CHARACTER SET utf8mb4 NOT NULL, "
		u"accumulatedSynonyms TEXT(1000) CHARACTER SET utf8mb4 NOT NULL, "
		u"accumulatedAntonyms CHAR(120) CHARACTER SET utf8mb4 NOT NULL, "
		u"concepts CHAR(40) CHARACTER SET utf8mb4 NOT NULL)"))
		return -1;
	return 0;
}

/*
typedef struct {
string mainEntry;
string wordType;
vector <string> primarySynonyms;
vector <string> accumulatedSynonyms, accumulatedAntonyms;
vector <int> concepts;
vector <string> rest;
} sDefinition;

*/
// Append each narrow string as "value;" into a wide SQL literal, escaped.
static void appendEscapedList(lpwstring& qt, const vector <string>& values)
{
	lpwstring wide;
	for (unsigned int I = 0; I < values.size(); I++)
	{
		mTW(values[I], wide);
		qt += escaped(wide) + u";";
	}
}

// INSERT one thesaurus row.  wordType is packed as bits from the wt[]
// tokens found in d.wordType (pron/conj/interj suppress n; adv suppresses v).
// Returns myquery's result.
int cSource::writeThesaurusEntry(sDefinition& d)
{
	const char* wt[] = { "adj", "adv", "prep", "pron", "conj", "det", "interj", "n", "v", NULL };
	int w, wtTotal = 0;
	for (int I = 0; wt[I]; I++)
		if ((w = d.wordType.find(wt[I])) != string::npos)
			wtTotal += 1 << I;
	if (wtTotal & ((1 << 3) | (1 << 4) | (1 << 6)))
		wtTotal &= ~(1 << 7);
	if (wtTotal & (1 << 1))
		wtTotal &= ~(1 << 8);
	// Built in a lpwstring: the synonym/antonym lists are unbounded and used to be
	// wcscat'd into a 4096 lpchar_t stack buffer.
	lpwstring qt, mainEntry, tmp;
	mTW(d.mainEntry, mainEntry);
	qt = u"INSERT INTO thesaurus VALUES (\"" + escaped(mainEntry) + u"\"," + itos(wtTotal, tmp) + u",\"";
	appendEscapedList(qt, d.primarySynonyms);
	qt += u"\",\"";
	appendEscapedList(qt, d.accumulatedSynonyms);
	qt += u"\",\"";
	appendEscapedList(qt, d.accumulatedAntonyms);
	qt += u"\",\"";
	for (unsigned int I = 0; I < d.concepts.size(); I++)
		qt += itos(d.concepts[I], tmp) + u";";
	qt += u"\")";
	return myquery(&mysql, qt.c_str());
}

// CREATE groups / groupMapTo / subGroups.  Still unused (author note: "these do
// not exist within the database"), but no longer broken-if-run: two defects that
// would have made this fail on the first call are fixed.
//   1. `groups` declared groupId with no index on it, so both child tables'
//      FOREIGN KEY (...) REFERENCES groups(groupId) would be rejected -- InnoDB
//      requires the referenced column to be the first column of an index on the
//      parent.  A non-unique index is the right kind here: a group has one row
//      per fromWord, so groupId is deliberately NOT unique in this table.
//   2. subGroups' second FK referenced groups(id); `groups` has no `id` column at
//      all.  It now references groups(groupId), which is what it meant.
// Reasoned from the schema, not executed -- there is no MySQL server in this
// environment to run it against, and nothing calls this function.
int cSource::createGroupTables(void)
{
	LFS
		if (!myquery(&mysql, u"CREATE TABLE groups (groupId int(11) unsigned NOT NULL, INDEX g_ind (groupId), "
			u"typeId SMALLINT UNSIGNED NOT NULL, INDEX t_ind(typeId), "
			u"fromWordId INT UNSIGNED NOT NULL, INDEX fw_ind (fromWordId), FOREIGN KEY (fromWordId) REFERENCES words(id), "
			u"ts TIMESTAMP)")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE groupMapTo (groupId INT UNSIGNED NOT NULL, INDEX g_ind (groupId), FOREIGN KEY (groupId) REFERENCES groups(groupId), "
		u"toWordId INT UNSIGNED NOT NULL, INDEX tw_ind (toWordId), FOREIGN KEY (toWordId) REFERENCES words(id), "
		u"ts TIMESTAMP)")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE subGroups (groupId INT UNSIGNED NOT NULL, INDEX g_ind (groupId), FOREIGN KEY (groupId) REFERENCES groups(groupId), "
		u"subgroupId INT UNSIGNED NOT NULL, INDEX sg_ind (subgroupId), FOREIGN KEY (subgroupId) REFERENCES groups(groupId), "
		u"UNIQUE INDEX gsg_ind (groupId,subgroupId))")) return -1;
	return 0;
}

// CREATE locations / locationObjectAssociation.  Unused (author note).
// thse are not used and do not exist within the database
int cSource::createLocationTables(void)
{
	LFS
		/*
			locations table:
			location id (auto-generated)
			sourceId smallint
			where int
			generic location id (key to locations table) - park/room/building etc
		*/
		if (!myquery(&mysql, u"CREATE TABLE locations ("
			u"id int(11) unsigned NOT NULL auto_increment unique, sourceId smallint NOT NULL, w int NOT NULL, "
			u"genericLocation INT UNSIGNED, FOREIGN KEY (genericLocation) REFERENCES locations(id))")) return -1;
	/*
		locationObjectAssociation table:
		location id (key to locations table)
		word (representing noun), physically present at said location
		specific location where (sourceId + where)
	*/
	if (!myquery(&mysql, u"CREATE TABLE locationObjectAssociation ("
		u"locationId INT UNSIGNED NOT NULL, FOREIGN KEY (locationId) REFERENCES locations(id), "
		u"wordId INT UNSIGNED NOT NULL, INDEX w_ind (wordId), FOREIGN KEY (wordId) REFERENCES words(id), "
		u"sourceId smallint NOT NULL, "
		u"w int NOT NULL)")) return -1;
	return 0;
}

// CREATE objects / objectLocations / objectWordMap.  objects has no `id`
// column (rows are keyed by (sourceId,objectNum) - see cSource::flushObjects,
// which REPLACE INTOs it positionally with no id value), so objectLocations
// cannot FK a single-column objects.id; the FK is dropped, keeping the plain
// index.  objectLocations itself is never populated anywhere in the tree
// (flushObjects's comment: "objectLocations is not written").
// objectWordMap.orderOrType is a name-part enum when objects.name is set.
int cSource::createObjectTables(void)
{
	LFS
		// the name bit will be set so that objectWordMap orderOrType will be defined as a type,
		// hon,hon2,hon3; first; middle,middle2; last; suffix; any;
		if (!myquery(&mysql, u"CREATE TABLE objects ("
			u"sourceId INT NOT NULL, "
			u"objectNum INT NOT NULL, "
			u"objectClass INT NOT NULL, "
			u"numEncounters INT NOT NULL, "
			u"numIdentifiedAsSpeaker INT NOT NULL, "
			u"nickName INT NOT NULL, "
			u"ownerWhere INT NOT NULL, "
			u"firstSpeakerGroup INT NOT NULL, "
			u"profession INT NOT NULL, "
			u"ts TIMESTAMP, "
			u"identified BIT NOT NULL, plural BIT NOT NULL, male BIT NOT NULL, female BIT NOT NULL, neuter BIT NOT NULL, "
			u"common BIT NOT NULL, name BIT NOT NULL)")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE objectLocations ("
		u"objectId INT UNSIGNED NOT NULL, INDEX oi_ind (objectId), "
		u"sourceId INT UNSIGNED NOT NULL, INDEX s_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id), "
		u"at INT)")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE objectWordMap ("
		u"sourceId INT NOT NULL, "
		u"objectNum INT NOT NULL, "
		u"wordId INT UNSIGNED NOT NULL, INDEX w_ind (wordId), FOREIGN KEY (wordId) REFERENCES words(id), "
		u"orderOrType TINYINT NOT NULL)")) return -1;
	return 0;
}

// CREATE timeGroups / timeGroupMembers / timeRelationTypes / timeGroupRelations
// (in that order - timeGroupMembers and timeGroupRelations FK timeGroups(id),
// so timeGroups must exist first: InnoDB rejects a FOREIGN KEY to a table
// that has not been created yet).  Never called anywhere in the tree today,
// but kept buildable rather than left broken.
// Fixed: timeGroupMembers indexed/FKed `relationId`, but the column is
// wordRelationId; timeGroupRelations had a trailing comma before `)`.
int cSource::createTimeRelationTables(void)
{
	LFS
		// timeGroups:
		if (!myquery(&mysql, u"CREATE TABLE timeGroups (id int(11) unsigned NOT NULL auto_increment unique, "
			u"speakerId INT UNSIGNED NOT NULL, INDEX si_ind (speakerId), FOREIGN KEY (speakerId) REFERENCES objects(id), "
			u"sourceId INT UNSIGNED NOT NULL, INDEX s_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id))")) return -1;
	// timeGroupMembers: timeGroupId foreign key to timeGroups, objectId, wordRelationId
	if (!myquery(&mysql, u"CREATE TABLE timeGroupMembers ("
		u"timeGroupId INT UNSIGNED NOT NULL, INDEX tgi_ind (timeGroupId), FOREIGN KEY (timeGroupId) REFERENCES timeGroups(id), "
		u"objectId INT UNSIGNED NOT NULL, INDEX oi_ind (objectId), FOREIGN KEY (objectId) REFERENCES objects(id), "
		u"wordRelationId INT UNSIGNED NOT NULL, INDEX r_ind (wordRelationId), FOREIGN KEY (wordRelationId) REFERENCES wordRelations(id))")) return -1;
	// timeRelationTypes: {BEFORE|AFTER|ON_OR_BEFORE|ON_OR_AFTER|LESS_THAN|MORE_THAN|
	//                    EQUAL_OR_LESS|EQUAL_OR_MORE|START|MID|END|APPROX}
	if (!myquery(&mysql, u"CREATE TABLE timeRelationTypes (id int(11) unsigned NOT NULL auto_increment unique, "
		u"type VARCHAR(256) CHARACTER SET utf8mb4 NOT NULL)")) return -1;
	// timeGroupRelations:
	if (!myquery(&mysql, u"CREATE TABLE timeGroupRelations (id int(11) unsigned NOT NULL auto_increment unique, "
		u"timeRelationTypeId INT UNSIGNED NOT NULL, INDEX tri_ind (timeRelationTypeId), FOREIGN KEY (timeRelationTypeId) REFERENCES timeRelationTypes(id), "
		u"timeGroupId INT UNSIGNED NOT NULL, INDEX tgi_ind (timeGroupId), FOREIGN KEY (timeGroupId) REFERENCES timeGroups(id), "
		u"timeGroup2Id INT UNSIGNED NOT NULL, INDEX tgi2_ind (timeGroup2Id), FOREIGN KEY (timeGroup2Id) REFERENCES timeGroups(id)"
		u")")) return -1;
	return 0;
}

// Larry loves Mary.
// Larry object:
//   RelationsWithObjects VerbWithSubjectWord
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is wordId to loves
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <cRelation> cr:
//         index is objectId to Mary
//         count++;
// Mary object:
//   RelationsWithObjects VerbWithDirectWord
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is wordId to loves
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <cRelation> cr:
//         index is objectId to Larry
//         count++;
// loves word:
//   verb=mainEntry love (of loves)
//   RelationsWithObjects SubjectWordWithVerb
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is objectId to Larry
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <cRelation> cr:
//         index is objectId to Mary
//         count++;
//   RelationsWithObjects DirectWordWithVerb
//     totalCount++;
//     add to vector tRelationWithObject o:
//       index is objectId to Mary
//       count++;
//       time is time of sentence
//       sense is "present tense, active"
//       add to vector <cRelation> cr:
//         index is objectId to Larry
//         count++;

// CREATE objectRelations, wordRelations, wordrelationsmemory (MEMORY),
// multiWordRelations, prepPhraseMultiWordRelations, relationsFlow.
// Fixed: wordrelationsmemory had a trailing comma before `)` and a unique key
// missing toWordId; multiWordRelations redeclared secondaryObjectLocal_ind
// (and the secondaryVerb FK) instead of covering nextSecondaryObjectLocal /
// secondaryVerb; relationsFlow FKed a relationsTributary table that does not
// exist anywhere in the schema, and its sourceId column FKed relationsFlow
// itself instead of sources(id).  This function is called from
// createDatabase(), so these bugs previously made a from-scratch schema
// bootstrap abort here every time.
int cSource::createRelationTables(void)
{
	LFS
		if (!myquery(&mysql, u"CREATE TABLE objectRelations (id int(11) unsigned NOT NULL auto_increment unique, "
			u"fromObjectId INT UNSIGNED NOT NULL, INDEX fo_ind (fromObjectId), FOREIGN KEY (fromObjectId) REFERENCES objects(id), "
			u"toObjectId INT UNSIGNED NOT NULL, INDEX to_ind (toObjectId), FOREIGN KEY (toObjectId) REFERENCES objects(id), "
			u"totalCount INT NOT NULL, "
			u"typeId INT UNSIGNED NOT NULL, INDEX t_ind(typeId), "
			u"ts TIMESTAMP)")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE wordRelations ("
		u"id int(11) unsigned NOT NULL auto_increment unique, "
		u"sourceId smallint(5) unsigned NOT NULL, "
		u"lastWhere int NOT NULL, "
		u"fromWordId INT UNSIGNED NOT NULL DEFAULT '0', INDEX fw_ind (fromWordId), FOREIGN KEY (fromWordId) REFERENCES words(id), "
		u"toWordId INT UNSIGNED NOT NULL DEFAULT '0', INDEX tw_ind (toWordId), FOREIGN KEY (toWordId) REFERENCES words(id), "
		u"typeId SMALLINT UNSIGNED NOT NULL DEFAULT '0', "
		u"UNIQUE INDEX uw_ind (fromWordId,toWordId,typeId), " // must be kept for ON DUPLICATE KEY UPDATE logic in flushWordRelations
		u"totalCount INT NOT NULL DEFAULT '0', "
		u"ts TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, INDEX ts_ind (ts)) DELAY_KEY_WRITE=1")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE wordrelationsmemory ("
		u"id int(11) unsigned NOT NULL AUTO_INCREMENT,"
		u"sourceId smallint(5) unsigned NOT NULL,"
		u"lastWhere int(10) unsigned NOT NULL,"
		u"fromWordId int(10) unsigned NOT NULL DEFAULT '0',"
		u"toWordId int(10) unsigned NOT NULL DEFAULT '0',"
		u"typeId smallint(5) unsigned NOT NULL DEFAULT '0',"
		u"totalCount int(11) NOT NULL DEFAULT '0',"
		u"UNIQUE KEY id (id),"
		// must match wordRelations' own (fromWordId,toWordId,typeId) unique key: main.cpp's
		// WRMemoryCheck bulk-inserts every row of wordRelations into this table with a plain
		// INSERT (no IGNORE/ON DUPLICATE), so a narrower key here (fromWordId,typeId) would
		// collide on the first fromWordId that has more than one toWordId of the same type
		// and abort the insert.
		u"UNIQUE KEY uw_ind (fromWordId,toWordId,typeId),"
		u"KEY fw_ind (fromWordId),"
		u"KEY tw_ind (toWordId)"
		u") ENGINE = MEMORY AUTO_INCREMENT = 36258188 DEFAULT CHARSET = latin1 DELAY_KEY_WRITE = 1;")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE multiWordRelations ("
		u"id int(11) unsigned NOT NULL auto_increment unique, "
		u"sourceId INT SIGNED NOT NULL DEFAULT -1, INDEX sourceId_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id), "
		u"w INT SIGNED NOT NULL DEFAULT -1, " // where is a reserved word so renamed to 'w'
		u"sentenceNum INT SIGNED NOT NULL DEFAULT -1, "
		u"narrativeNum INT SIGNED NOT NULL DEFAULT -1, "
		u"speaker INT SIGNED NOT NULL DEFAULT -1, "
		u"audience INT SIGNED NOT NULL DEFAULT -1, "
		u"referencingEntityLocal INT SIGNED NOT NULL DEFAULT -1, INDEX referencingEntity_ind (referencingEntityLocal), FOREIGN KEY (referencingEntityLocal) REFERENCES words(id), "
		u"referencingEntityMatched INT SIGNED NOT NULL DEFAULT -1, "
		u"referencingVerb INT SIGNED NOT NULL DEFAULT -1,FOREIGN KEY (referencingVerb) REFERENCES words(id), "
		u"subjectAdjectiveMatchedOwner INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (subjectAdjectiveMatchedOwner) REFERENCES objects(id), "
		u"subjectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (subjectAdjective) REFERENCES words(id), "
		u"subjectAdjective2 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (subjectAdjective2) REFERENCES words(id), "
		u"subjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX subject_ind (subjectLocal), FOREIGN KEY (subjectLocal) REFERENCES words(id), "
		u"subjectMatched INT SIGNED NOT NULL DEFAULT -1, "
		u"verb INT SIGNED NOT NULL DEFAULT -1,FOREIGN KEY (verb) REFERENCES words(id), "
		u"adverb INT SIGNED DEFAULT -1,FOREIGN KEY (adverb) REFERENCES words(id), "
		u"objectAdjectiveMatchedOwner INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjectiveMatchedOwner) REFERENCES objects(id), "
		u"objectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjective) REFERENCES words(id), "
		u"objectAdjective2 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjective2) REFERENCES words(id), "
		u"objectAdjective3 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (objectAdjective3) REFERENCES words(id), "
		u"objectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX object_ind (objectLocal), FOREIGN KEY (objectLocal) REFERENCES words(id), "
		u"objectMatched INT SIGNED NOT NULL DEFAULT -1, "
		u"nextObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX nextObjectLocal_ind (nextObjectLocal), FOREIGN KEY (nextObjectLocal) REFERENCES words(id), "
		u"nextObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
		u"secondaryVerb INT SIGNED NOT NULL DEFAULT -1,FOREIGN KEY (secondaryVerb) REFERENCES words(id), "
		u"secondaryObjectAdjectiveMatchedOwner INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (secondaryObjectAdjectiveMatchedOwner) REFERENCES objects(id), "
		u"secondaryObjectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (secondaryObjectAdjective) REFERENCES words(id), "
		u"secondaryObjectAdjective2 INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (secondaryObjectAdjective2) REFERENCES words(id), "
		u"secondaryObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX secondaryObjectLocal_ind (secondaryObjectLocal), FOREIGN KEY (secondaryObjectLocal) REFERENCES words(id), "
		u"secondaryObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
		u"nextSecondaryObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX nextSecondaryObjectLocal_ind (nextSecondaryObjectLocal), FOREIGN KEY (nextSecondaryObjectLocal) REFERENCES words(id), "
		u"nextSecondaryObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
		u"relationType INT SIGNED NOT NULL DEFAULT -1, "
		u"objectSubType INT SIGNED NOT NULL DEFAULT -1, "
		u"timeProgression INT SIGNED NOT NULL DEFAULT -1, "
		u"flags BIGINT SIGNED NOT NULL DEFAULT -1) DELAY_KEY_WRITE=1")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE prepPhraseMultiWordRelations ("
		u"sourceId INT SIGNED NOT NULL DEFAULT -1, INDEX sourceId_ind (sourceId), FOREIGN KEY (sourceId) REFERENCES sources(id), "
		u"w INT SIGNED NOT NULL DEFAULT -1, " // where is a reserved word so renamed to 'w'
		u"nearestVerb INT SIGNED DEFAULT NULL, INDEX nearestVerb_ind (nearestVerb), FOREIGN KEY (nearestVerb) REFERENCES words(id), "
		u"nearestObject INT SIGNED DEFAULT NULL, INDEX nearestObject_ind (nearestObject), FOREIGN KEY (nearestObject) REFERENCES words(id), "
		u"principalWord INT SIGNED DEFAULT NULL, INDEX principalWord_ind (principalWord), FOREIGN KEY (principalWord) REFERENCES words(id), "
		u"immediatePrincipalWord INT SIGNED DEFAULT NULL, INDEX immediatePrincipalWord_ind (immediatePrincipalWord), FOREIGN KEY (immediatePrincipalWord) REFERENCES words(id), "
		u"prep INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (prep) REFERENCES words(id), "
		u"prepObjectAdjectiveMatchedOwner int(11) NOT NULL DEFAULT -1, "
		u"prepObjectAdjective INT SIGNED NOT NULL DEFAULT -1, FOREIGN KEY (prepObjectAdjective) REFERENCES words(id), "
		u"prepObjectLocal INT SIGNED NOT NULL DEFAULT -1, INDEX prepObjectLocal_ind (prepObjectLocal), FOREIGN KEY (prepObjectLocal) REFERENCES words(id), "
		u"prepObjectMatched INT SIGNED NOT NULL DEFAULT -1, "
		u"prepObjectSubType INT SIGNED NOT NULL DEFAULT -1, "
		u"flags INT SIGNED NOT NULL DEFAULT -1) DELAY_KEY_WRITE=1")) return -1;
	if (!myquery(&mysql, u"CREATE TABLE relationsFlow (id int(11) unsigned NOT NULL auto_increment unique, "
		// subject or object (objectId)
		u"objectId INT UNSIGNED, INDEX o_ind (objectId), FOREIGN KEY (objectId) REFERENCES objects(id), "
		// relationIndex
		u"relationId INT UNSIGNED NOT NULL, INDEX r_ind (relationId), FOREIGN KEY (relationId) REFERENCES wordRelations(id), "
		// { T_SEQUENTIAL, T_ON, T_AFTER, T_PRESENT,T_VAGUE, T_RECURRING etc }
		u"timeRelationType SMALLINT UNSIGNED NOT NULL, "
		// day  (year > season > month > week > day (date) > afternoon > hour > minute (time) > second...
		u"capacity SMALLINT UNSIGNED NOT NULL, "
		// reference time
		u"timeExpressionId INT UNSIGNED NOT NULL, FOREIGN KEY (timeExpressionId) REFERENCES relationsFlow(id), "
		// other generic relations in the same expression - no relationsTributary table exists anywhere
		// in the schema, so this can only be a plain column (dropping the dangling FK) until one is added
		u"relationsTributaryId INT UNSIGNED NOT NULL, "
		u"sequenceId INT UNSIGNED," // # - after timeExpression (increases by 10 to allow for insertions, or none if simultaneous
		u"sourceId INT UNSIGNED NOT NULL, FOREIGN KEY (sourceId) REFERENCES sources(id), "
		u"ts TIMESTAMP)")) return -1;
	return 0;
}

// Parse a BNC-world bncIndex.xml-like file and INSERT one sources row per
// <doc> whose <genre> starts with 'W'.  Reads the file as raw lpchar_t data
// (actualLen bytes) into a buffer of actualLen+sizeof(lpchar_t) bytes and
// NUL-terminates at the lpchar_t offset, in range.  Returns 0 on success,
// negative NET_ERR-like codes on I/O or parse failure.
int generateBNCSources(MYSQL& mysql, lpwstring indexFile) // note this is slightly modified from the original
{
	LFS
		int startTime = clock();
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_strcpy(qt, u"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
	size_t len = lp_strlen(qt);
	// Batch B5: POSIX open/fstat/read replace CreateFile/GetFileSize/ReadFile.
	int hFile = lp_wopen(indexFile, O_RDONLY);
	if (hFile < 0)
	{
		lpwstring bcct;
		if (errno != ERROR_PATH_NOT_FOUND)
			lplog(LOG_FATAL_ERROR, u"GetPath cannot open BNC sources path %s - %s", indexFile.c_str(), getLastErrorMessage(bcct));
		return -17;
	}
	struct stat indexStatus;
	unsigned int actualLen = (fstat(hFile, &indexStatus) == 0) ? (unsigned int)indexStatus.st_size : 0;
	if (actualLen <= 0 || (actualLen % sizeof(lpchar_t)) != 0)
	{
		lplog(LOG_ERROR, u"ERROR:filelength of file %s yields an invalid filelength (%d).", indexFile.c_str(), actualLen);
		::close(hFile);
		return -18;
	}
	lpchar_t* buffer = (lpchar_t*)tmalloc(actualLen + sizeof(lpchar_t));
	if (::read(hFile, buffer, actualLen) != (ssize_t)actualLen)
	{
		lplog(LOG_ERROR, u"ERROR:read error of file %s.", indexFile.c_str());
		::close(hFile);
		return -19;
	}
	buffer[actualLen / sizeof(lpchar_t)] = 0;
	::close(hFile);
	int where = 0, returnCode = -20;// , numWords = 0;
	while (true)
	{
		lpchar_t* docStart = lp_strstr(buffer + where, u"<doc>");
		if (!docStart)
		{
			returnCode = 0;
			break;
		}
		returnCode = -20;
		lpchar_t* docEnd = lp_strstr(docStart, u"</doc>");
		if (!docEnd) break;
		*docEnd = 0;
		lpchar_t* id = lp_strstr(docStart, u"<idno>");
		if (!id) break;
		id += lp_strlen(u"<idno>");
		lpchar_t* idEnd = lp_strstr(id, u"</idno>");
		if (!idEnd) break;
		*idEnd = 0;
		lpchar_t* genre = lp_strstr(idEnd + 1, u"<genre>");
		if (!genre) break;
		where = (docEnd - buffer) + lp_strlen(u"</doc>");
		if (genre[lp_strlen(u"<genre>")] != 'W')
			continue;
		len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u"(%d, \"%s\", \"\", 0),", cSource::BNC_SOURCE_TYPE, id);
		if (!checkFull(&mysql, qt, len, false, NULL)) return -20;
	}
	if (!checkFull(&mysql, qt, len, true, NULL)) return -21;
	if (logDatabaseDetails)
		lplog(u"Inserting BNC sources took %d seconds.", (clock() - startTime) / CLOCKS_PER_SEC);
	tfree(actualLen + sizeof(lpchar_t), buffer); // match the tmalloc(actualLen + sizeof(lpchar_t)) above
	return returnCode;
}

// INSERT one NEWS_BANK_SOURCE_TYPE row per day from Unix day 2557..7056
// (1977-01-01-ish through ~1989) with path newsbank\\<year>\\<daynum>.txt.
int generateNewsBankSources(MYSQL& mysql)
{
	LFS
		int startTime = clock();
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_strcpy(qt, u"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
	size_t len = lp_strlen(qt);
	for (unsigned int I = 2557; I < 7057; I++)
	{
		time_t timer = I * 24 * 3600;
		struct tm* day = gmtime(&timer);
		len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u"(%d, \"newsbank\\%d\\%I64d.txt\", \"\", 0),", cSource::NEWS_BANK_SOURCE_TYPE, day->tm_year + 1900, timer / (24 * 3600));
		if (!checkFull(&mysql, qt, len, false, NULL)) return -1;
	}
	if (!checkFull(&mysql, qt, len, true, NULL)) return -1;
	if (logDatabaseDetails)
		lplog(u"Inserting NewsBank sources took %d seconds.", (clock() - startTime) / CLOCKS_PER_SEC);
	return 0;
}

// INSERT the canned tests\\<name>.txt sources (agreement, date-time-number,
// Nameres, ...) as TEST_SOURCE_TYPE with start ~~BEGIN.
int generateTestSources(MYSQL& mysql)
{
	LFS
		int startTime = clock();
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_strcpy(qt, u"INSERT INTO sources (sourceType, path, start, repeatStart) VALUES ");
	size_t len = lp_strlen(qt);
	const lpchar_t* testSources[] = { u"agreement",u"date-time-number",u"lappinl",u"modification",u"Nameres",u"pattern matching",u"resolution",u"time",u"timeExpressions",u"usage",
											 u"verb object",NULL };
	for (unsigned int I = 0; testSources[I]; I++)
	{
		len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u"(%d, \"tests\\\\%s.txt\", \"~~BEGIN\", 1),", cSource::TEST_SOURCE_TYPE, testSources[I]);
		if (!checkFull(&mysql, qt, len, false, NULL)) return -1;
	}
	if (!checkFull(&mysql, qt, len, true, NULL)) return -1;
	if (logDatabaseDetails)
		lplog(u"Inserting Test sources took %d seconds.", (clock() - startTime) / CLOCKS_PER_SEC);
	return 0;
}

// INSERT one REQUEST_TYPE source for a QA web-search hit.  pathInCache is
// stripped of the TEXTDIR prefix and escapeStr'd; fullWebPath is used as
// title and is NOT escaped (lp_wsprintf into 16384 lpchar_t).
int generateParseRequestSources(MYSQL& mysql, vector <cQuestionAnswering::cSearchSource>::iterator pri)
{
	lpchar_t qt[16384];
	lpwstring pathInCache = pri->pathInCache;
	pathInCache = pathInCache.substr(lp_strlen(TEXTDIR) + 1, pathInCache.length() - lp_strlen(TEXTDIR) - 1);
	escapeStr(pathInCache);
	lp_wsprintf(qt, u"INSERT INTO sources (sourceType, etext, path, start, repeatStart, author, title, processing, processed) VALUES (%d,\"%s\",\"%s\",\"\",0,\"\",\"%s\",NULL,NULL)",
		cSource::REQUEST_TYPE, (pri->isSnippet) ? u"snippet" : u"full", pathInCache.c_str(), pri->fullWebPath.c_str());
	return myquery(&mysql, qt);
}

// DELETE every REQUEST_TYPE row from sources (the ephemeral QA parses).
int deleteGeneratedParseRequests(MYSQL& mysql)
{
	lpchar_t qt[1024];
	lp_wsprintf(qt, u"delete from sources where sourceType=%d", cSource::REQUEST_TYPE);
	return myquery(&mysql, qt);
}

// INSERT (id, getRelStr(id)) for firstRelationType..numRelationWOTypes-1
// into wordRelationType.  Called only when the CREATE of that table fails
// (i.e. it already exists) - a surprising control flow.
int cSource::insertWordRelationTypes(void)
{
	LFS
		lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_strcpy(qt, u"INSERT INTO wordRelationType VALUES ");
	size_t len = lp_strlen(qt);
	for (unsigned int I = firstRelationType; I < numRelationWOTypes; I++)
	{
		len += lp_snprintf(qt + len, QUERY_BUFFER_LEN - len, u"(%u, \"%s\"),", I, getRelStr(I));
		if (!checkFull(&mysql, qt, len, false, NULL)) return -1;
	}
	if (!checkFull(&mysql, qt, len, true, NULL)) return -1;
	return 0;
}

// If initializeDatabaseHandle succeeds the schema already exists: close and
// return 0.  Otherwise, if the error was ER_BAD_DB_ERROR, reconnect without
// a schema (same getDBUser()/getDBPassword() credentials), CREATE DATABASE lp,
// then CREATE the core tables and seed sources from bookSources.sql + BNC/
// NewsBank/tests. Returns 0 or -1; most CREATE failures are FATAL first.
int cSource::createDatabase(const lpchar_t* server)
{
	LFS
		if (!initializeDatabaseHandle(mysql, server, alreadyConnected))
		{
			mysql_close(&mysql);
			return 0;
		}
	string sqlStr;
	if (mysql_errno(&mysql) != ER_BAD_DB_ERROR || mysql_real_connect(&mysql, wTM(server, sqlStr), getDBUser().c_str(), getDBPassword().c_str(), NULL, 0, NULL, 0) == NULL)
	{
		lplog(LOG_FATAL_ERROR, u"Failed to connect to MySQL - %S", mysql_error(&mysql));
		//int err=mysql_errno(&mysql);
		return -1;
	}
	lpchar_t qt[2 * QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"CREATE DATABASE %s character set utf8mb4", LDBNAME);
	if (!myquery(&mysql, qt)) return -1;
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"USE %s", LDBNAME);
	if (!myquery(&mysql, qt)) return -1;
	// create forms table
	if (!myquery(&mysql, u"CREATE TABLE forms ("
		u"id int(11) unsigned NOT NULL auto_increment unique, "
		u"name CHAR(32) CHARACTER SET utf8mb4 NOT NULL, "
		u"shortName CHAR(32) CHARACTER SET utf8mb4 NOT NULL, "
		u"inflectionsClass CHAR(12) CHARACTER SET utf8mb4 NOT NULL, "
		u"hasInflections BIT NOT NULL, "
		u"isTopLevel BIT NOT NULL, "
		u"properNounSubClass BIT NOT NULL, "
		u"blockProperNounRecognition BIT NOT NULL,"
		u"ts TIMESTAMP)")) return -1;
	// create sources table
	if (!myquery(&mysql, u"CREATE TABLE sources "
		u"(id int(11) unsigned NOT NULL auto_increment unique, "
		u"sourceType TINYINT(4) NOT NULL, "
		u"etext VARCHAR (10) CHARACTER SET utf8mb4,"
		u"path VARCHAR (1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,"
		u"start VARCHAR(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL, "
		u"repeatStart INT NOT NULL, "
		u"author VARCHAR (128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
		u"title VARCHAR (1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
		u"date DATETIME,"
		u"numSentences INT, matchedSentences INT, numWords INT, numUnknown INT, numUnmatched INT, numOvermatched INT,"
		u"numQuotations INT, quotationExceptions INT, numTicks INT, numPatternMatches INT, "
		u"sizeInBytes INT, numWordRelations INT, numMultiWordRelations INT, "
		u"processing BIT, processed BIT, "
		u"proc2 INT, " // other processing steps
		u"duplicateId INT," // if not null, this source should be skipped because it is a duplicate of another source (id)
		u"lastProcessedTime TIMESTAMP DEFAULT 0, " // not set to current timestamp or updated automatically
		u"ts TIMESTAMP," // specifying nothing is the same as DEFAULT CURRENT_TIMESTAMP and ON UPDATE CURRENT_TIMESTAMP
		u"KEY `EtextIndex` (`etext`),"
		u"KEY `nsi` (`sourceType`,`start`,`processed`)"
		u") DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_bin"))
	{
		lplog(LOG_FATAL_ERROR, u"Failed to create sources - %S", mysql_error(&mysql));
		return -1;
	}
	int actualLen;
	getPath(u"source\\lists\\bookSources.sql", qt, 2 * QUERY_BUFFER_LEN, actualLen);
	qt[actualLen] = 0;
	lpwstring source = qt;
	//mysql_real_escape_string(&mysql, qt, source.c_str(), actualLen);
	if (!myquery(&mysql, qt))
	{
		lplog(LOG_FATAL_ERROR, u"Failed to populate sources with books - %S", mysql_error(&mysql));
		return -1;
	}
	generateBNCSources(mysql, lpwstring(LMAINDIR) + u"\\BNC-world\\doc\\Source\\bncIndex.xml");
	generateNewsBankSources(mysql);
	generateTestSources(mysql);
	// create wordRelations types table (for convenience)
	if (!myquery(&mysql, u"CREATE TABLE wordRelationType (id int(11) unsigned NOT NULL unique, type CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL)"))
		insertWordRelationTypes();
	// create words table
	if (!myquery(&mysql, u"CREATE TABLE words (id int(11) unsigned NOT NULL auto_increment unique, "
		u"word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT '',"
		u"inflectionFlags INT UNSIGNED NOT NULL default '0',"
		u"flags INT NOT NULL default '0',"
		u"timeFlags INT NOT NULL default '0'," // INDEX word_ind (word) dropped (increased index space by more than 1000 times)
		u"mainEntryWordId INT UNSIGNED NULL, INDEX me_ind (mainEntryWordId), "
		u"derivationRules INT DEFAULT 0, "
		u"sourceId INT UNSIGNED DEFAULT NULL, INDEX s_ind (sourceId), " //FOREIGN KEY (sourceId) REFERENCES sources(id),
		u"ts TIMESTAMP, "
		u"INDEX ts_ind (ts)"
		u") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"))
	{
		lplog(LOG_FATAL_ERROR, u"Failed to create words table - %S", mysql_error(&mysql));
		return -1;
	}
	// create wordForms table
	if (!myquery(&mysql, u"CREATE TABLE wordForms ("
		u"wordId INT UNSIGNED NOT NULL, "
		u"formId INT UNSIGNED NOT NULL, "
		u"count INT NOT NULL, "
		u"ts TIMESTAMP, "
		u"INDEX wId_ind (wordId), " // FOREIGN KEY (wordId) REFERENCES words(id), "
		u"INDEX fId_ind (formId), "
		u"INDEX ts_ind (ts), "
		u"UNIQUE INDEX uw_ind (wordId,formId)"))
	{
		lplog(LOG_FATAL_ERROR, u"Failed to create wordForms - %S", mysql_error(&mysql));
		return -1;
	}
	if (!myquery(&mysql, u"ALTER TABLE wordForms DELAY_KEY_WRITE = 1")) return -1;
	// words unknown processing
	if (!myquery(&mysql, u"CREATE TABLE wordFrequency ("
		u"word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT '',"
		u"totalFrequency       INT NOT NULL default '0', "
		u"unknownFrequency     INT NOT NULL default '0', "
		u"capitalizedFrequency INT NOT NULL default '0', "
		u"allCapsFrequency     INT NOT NULL default '0', "
		u"lastSourceId INT UNSIGNED DEFAULT NULL, INDEX s_ind (lastSourceId), "
		u"nonEuropeanWord BIT "
		u") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin ENGINE = INNODB"))  // must use INNODB because of per row locking required by multiple processes 
	{
		lplog(LOG_FATAL_ERROR, u"Failed to create wordsFrequency table - %S", mysql_error(&mysql));
		return -1;
	}
	if (!myquery(&mysql, u"CREATE TABLE noRDFTypes ("
		u"word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT ''"
		u") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"))
	{
		lplog(LOG_FATAL_ERROR, u"Failed to create noRDFTypes table - %S", mysql_error(&mysql));
		return -1;
	}
	if (createObjectTables() < 0)
	{
		lplog(LOG_FATAL_ERROR, u"Failed to create object tables - %S", mysql_error(&mysql));
		return -1;
	}
	if (createRelationTables() < 0)
	{
		lplog(LOG_FATAL_ERROR, u"Failed to create relation tables - %S", mysql_error(&mysql));
		return -1;
	}
	return 0;
}

