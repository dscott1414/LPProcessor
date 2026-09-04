/*
	tableColumn.cpp - Wikipedia-table walk and RDF-type coherence scoring for QA

	Overview:
		cSourceTable walks a Wikipedia cSource from a Words.TABLE sentinel,
		building cColumn/cRow/cEntry from the synthetic END_COLUMN /
		END_COLUMN_HEADERS tokens tokenize.cpp left in m[].  Each column is then
		scored: accumulate every cell's simplified RDF types, prefer cells whose
		type (or last word) hits the title's WordNet synonyms, re-accumulate,
		and compute coherencyPercentage.  Wiki chrome (languages, "View history",
		VIAF identifiers, ...) is rejected via wikiInvalidTableEntries.

	Pipeline position:
		Stage 8.  cQuestionAnswering::addTables() constructs one cSourceTable per
		TABLE token; surviving tables become wikiQuestionTypeObjectAnswers.

	Key entry points:
		- cSourceTable::cSourceTable() - parse one table starting at I (advanced).
		- getTableFromSource() - headers + cells.
		- scanColumnEntry() - one header/title cell, matching the question type.
		- determineColumnRDFTypeCoherency() - the two-pass RDF score.
		- addTables() - driver.

	Notes / gotchas:
		- determineColumnRDFTypeCoherency now returns false for a >3
			entries/row average or a <90% coherence score (both reject paths
			were previously disabled - "TEMP DEBUG" - so the caller never
			dropped a column for incoherence).  Behaviour-changing: incoherent
			Wikipedia table columns are rejected as QA answers again.
		- cEntry::sprint now formats synonymMatchedQuestionObject.size() with
			its own synonymMatchedQuestionObjectStr (previously copy-pasted
			matchedQuestionObjectStr, so the synonym count was never shown).
		- testTitlePreference uses `=` in `if (x = (titleSynonyms.find(...)))`
			intentionally (assignment-in-condition, same idiom as source.h).
		- getTableFromSource always returns true; invalid tables are marked via
			invalidColumn / empty entries, not the return code.
		- wikiInvalidTableEntries is a NULL-separated list of chrome groups;
			isEntryInvalid builds a map on first call.
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
#include "QuestionAnswering.h"
#include "profile.h"
#include "wn.h"

// Zero the per-column counters.  gMaxFrequency starts at 1 so a later
// ratio divide cannot /0 before any types are accumulated.  rows and the
// RDF maps are left empty (their default ctors).
cColumn::cColumn()
{
	numDefinite = 0;
	numNumerical = 0;
	numPunctuation = 0;
	gMaxFrequency = 1;
	mostCommonRatio = 0;
	numCommonObjectAssociations = 0;
	invalidEntries = 0;
	emptyEntries = 0;
	matchedHeader = false;
	queryAssociationsMatched = 0;
	titleAssociationsMatched = 0;
	numRowsWhereLastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = 0;
	numRowsWhereSimplifiedRDFTypesFound = 0;
}

/* RDF type column analysis section*/

// Zero the frequency of every domainAssociations[] key in
// accumulatedRDFTypesMap (the entries stay, but they drop out of
// getMostCommonRDFTypes).  domainAssociations is a NULL-terminated list
// whose last element is u"".
// remove a domain like film from the accumulated common association map.  This is because Wikipedia is really good at accumulating types of this sort, which biases trying to find a common association.
void cColumn::removeDomainFromAccumulatedRDFTypesMap(const lpchar_t* domainAssociations[])
{
	for (int ma = 0; domainAssociations[ma][0]; ma++)
	{
		unordered_map < lpwstring, cAssociationType >::iterator catmi = accumulatedRDFTypesMap.find(domainAssociations[ma]);
		if (catmi != accumulatedRDFTypesMap.end())
			catmi->second.frequency = 0;
	}
}

// accumulate all rdf types of all entries of all rows in a column.
void cColumn::accumulateColumnRDFTypes(cSource* wikipediaSource, lpwstring tableName, unordered_set <lpwstring>& titleSynonyms, bool keepMusicDomain, bool keepFilmDomain, bool onlyPreferred, bool fileCaching)
{
	for (int row = 0; row < rows.size(); row++)
	{
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
			if (!onlyPreferred || rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms)
				rows[row].entries[entry].accumulateEntryRDFTypes(wikipediaSource, tableName, row, entry, titleSynonyms, accumulatedRDFTypesMap, fileCaching);
	}
	const lpchar_t* musicAssociations[] = { u"single", u"recording", u"music", u"release", u"album", u"" };
	if (!keepMusicDomain)
		removeDomainFromAccumulatedRDFTypesMap(musicAssociations);
	const lpchar_t* filmAssociations[] = { u"film", u"" };
	if (!keepFilmDomain)
		removeDomainFromAccumulatedRDFTypesMap(filmAssociations);
}

// accumulate RDF types from one entry.  
// the RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap map takes all the RDF types associated with a given sequence of words (adaptiveWhere to adaptiveWhere+numWords) and 
//   returns a list of words with confidences estimating how much the RDF type the word is simplified from is associated with the sequence of words.
// take the list of words simplified from RDF types which are associated with the entry, and accumulate the ones with highest confidence into the accumulatedRDFTypesMap for the column.
void cColumn::cEntry::accumulateEntryRDFTypes(cSource* wikipediaSource, lpwstring tableName, int row, int entry, unordered_set <lpwstring>& titleSynonyms, unordered_map < lpwstring, cAssociationType >& accumulatedRDFTypesMap, bool fileCaching)
{
	queryAssociationsMatched = 0;
	titleAssociationsMatched = 0;
	unordered_map <lpwstring, int > RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
	wikipediaSource->getAssociationMapMaster(adaptiveWhere, numWords, RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap, LP_TEXT(__func__), fileCaching);
	for (unordered_map <lpwstring, int >::iterator ri = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.begin(), riEnd = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.end(); ri != riEnd; ri++)
	{
		if (logTableCoherenceDetail)
			lplog(LOG_WHERE, u"Processing table %s: rowIndex %d entry %d accumulateEntryRDFTypes RDF type %s", tableName.c_str(), row, entry, ri->first.c_str());
		if (ri->second == 1) // only if confidence is 1
		{
			unordered_map < lpwstring, cAssociationType >::iterator cmi = accumulatedRDFTypesMap.find(ri->first);
			if (cmi == accumulatedRDFTypesMap.end())
			{
				accumulatedRDFTypesMap[ri->first].frequency = 1;
				unordered_set <lpwstring >::iterator wami;
				if (accumulatedRDFTypesMap[ri->first].titleObjectMatch = (wami = titleSynonyms.find(ri->first)) != titleSynonyms.end())
					titleAssociationsMatched++;
			}
			else
				cmi->second.frequency++;
		}
	}
}

// after accumulating RDF types from every entry from every row in the column, further accumulate them into a set, which is sorted by frequency, then alphabetically by word 
// output in mostCommonAssociationTypeSet, which is the set of all the simplified RDF types, their frequency in the column, whether the RDF type matched something in the query, and whether the RDF type matched something in the title of the column.
void cColumn::getMostCommonRDFTypes(const lpchar_t* when, lpwstring tableName)
{
	mostCommonAssociationTypeSet.clear();
	numCommonObjectAssociations = 0;
	for (unordered_map < lpwstring, cAssociationType >::iterator cmi = accumulatedRDFTypesMap.begin(), cmiEnd = accumulatedRDFTypesMap.end(); cmi != cmiEnd; cmi++)
	{
		if (logTableCoherenceDetail)
			lplog(LOG_WHERE, u"Processing table %s: getMostCommonRDFTypes RDF type coherency %s numRows= %d RDFType=%s Frequency=%d", tableName.c_str(),
				when, rows.size(), cmi->first.c_str(), cmi->second.frequency);
		if (cmi->second.frequency > rows.size() - 1)
		{
			numCommonObjectAssociations++;
			mostCommonAssociationTypeSet.insert(cWordFrequencyMatch(cmi->first, cmi->second.frequency, cmi->second.queryObjectMatch, cmi->second.titleObjectMatch));
			queryAssociationsMatched++;
			if (cmi->second.titleObjectMatch)
				titleAssociationsMatched++;
		}
		else if (cmi->second.titleObjectMatch && cmi->second.frequency > rows.size() / 2)
		{
			mostCommonAssociationTypeSet.insert(cWordFrequencyMatch(cmi->first, cmi->second.frequency, false, cmi->second.titleObjectMatch));
			titleAssociationsMatched++;
		}
	}
	if (logTableCoherenceDetail)
		lplog(LOG_WHERE, u"Processing table %s: table coherency %s numRows= %d numCommonObjectAssociations=%d queryAssociationsMatched=%d titleAssociationsMatched=%d", tableName.c_str(),
			when, rows.size(), numCommonObjectAssociations, queryAssociationsMatched, titleAssociationsMatched);
	if (mostCommonAssociationTypeSet.size() > 0 && rows.size() > 0)
		mostCommonRatio = ((mostCommonAssociationTypeSet.begin()))->frequency * 100 / rows.size();
	logColumn(LOG_WHERE, when, tableName);
}

// Drop accumulatedRDFTypesMap so the second (preferred-cell-only) pass
// starts clean.  mostCommonAssociationTypeSet is not cleared here.
void cColumn::zeroColumnAccumulatedRDFTypes()
{
	accumulatedRDFTypesMap.clear();
}

// Sum accumulatedRDFTypesMap frequencies for this cell's confidence==1 RDF
// types; also return the type with the highest frequency via the out-params.
// FATAL if a confidence-1 type is missing from the column map (should have
// been accumulated already).
int cColumn::getSumOfAllFullyConfidentRDFTypeFrequencies(cSource* wikipediaSource, int row, int entry, int& maxOfAllFullyConfidentRDFTypeFrequencies, lpwstring& fullyConfidentSimplifiedRDFTypeWithMaximumFrequency, bool fileCaching)
{
	unordered_map <lpwstring, int > RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
	wikipediaSource->getAssociationMapMaster(rows[row].entries[entry].adaptiveWhere, rows[row].entries[entry].numWords, RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap, LP_TEXT(__func__), fileCaching);
	int sumOfAllFullyConfidentRDFTypeFrequencies = 0;
	maxOfAllFullyConfidentRDFTypeFrequencies = 0;
	for (unordered_map <lpwstring, int >::iterator ri = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.begin(), riEnd = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.end(); ri != riEnd; ri++)
	{
		if (ri->second == 1) // only use RDFTypes with a certain confidence
		{
			unordered_map < lpwstring, cAssociationType >::iterator cmi = accumulatedRDFTypesMap.find(ri->first);
			if (cmi == accumulatedRDFTypesMap.end())
				lplog(LOG_FATAL_ERROR, u"association for word %s not found in common columnIndex map!", ri->first.c_str());
			sumOfAllFullyConfidentRDFTypeFrequencies += cmi->second.frequency;
			if (maxOfAllFullyConfidentRDFTypeFrequencies < cmi->second.frequency)
			{
				maxOfAllFullyConfidentRDFTypeFrequencies = cmi->second.frequency;
				fullyConfidentSimplifiedRDFTypeWithMaximumFrequency = cmi->first;
			}
		}
	}
	return sumOfAllFullyConfidentRDFTypeFrequencies;
}

// singling out only the entries that are marked lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms, accumulate the confident RDF type frequencies and divide by the number of rows, thus arriving at a coherence percentage.
int cColumn::calculateColumnRDFTypeCoherence(cSource* wikipediaSource, cColumn::cEntry titleEntry, lpwstring tableName, bool fileCaching)
{
	int sumOfAllFullyConfidentRDFTypeFrequenciesInOnlyPreferredColumnEntries = 0, sumOfMaxOfAllFullyConfidentRDFTypeFrequenciesInOnlyPreferredColumnEntries = 0;
	// for each row
	for (int row = 0; row < rows.size(); row++)
	{
		// for each entry in each row
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
		{
			// for each entry in each row
			if (rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms)
			{
				lpwstring fullyConfidentSimplifiedRDFTypeWithMaximumFrequencyInEntry;
				int maxOfAllFullyConfidentRDFTypeFrequenciesInEntry = 0, sumOfAllFullyConfidentRDFTypeFrequenciesInEntry;
				sumOfAllFullyConfidentRDFTypeFrequenciesInEntry = getSumOfAllFullyConfidentRDFTypeFrequencies(wikipediaSource, row, entry, maxOfAllFullyConfidentRDFTypeFrequenciesInEntry, fullyConfidentSimplifiedRDFTypeWithMaximumFrequencyInEntry, fileCaching);
				if (!maxOfAllFullyConfidentRDFTypeFrequenciesInEntry)
				{
					maxOfAllFullyConfidentRDFTypeFrequenciesInEntry = rows.size();
					wikipediaSource->phraseString(titleEntry.begin, titleEntry.begin + titleEntry.numWords, fullyConfidentSimplifiedRDFTypeWithMaximumFrequencyInEntry, true);
				}
				lpwstring tmpstr;
				wikipediaSource->phraseString(rows[row].entries[entry].begin, rows[row].entries[entry].begin + rows[row].entries[entry].numWords, tmpstr, false);
				if (logQuestionDetail)
					lplog(LOG_WHERE, u"Processing table %s: table coherency %s rowIndex=%d entry=%d associationValue=%d maxFrequency=%d[%s]", tableName.c_str(), tmpstr.c_str(),
						row, entry, sumOfAllFullyConfidentRDFTypeFrequenciesInEntry, maxOfAllFullyConfidentRDFTypeFrequenciesInEntry, fullyConfidentSimplifiedRDFTypeWithMaximumFrequencyInEntry.c_str());
				sumOfAllFullyConfidentRDFTypeFrequenciesInOnlyPreferredColumnEntries += sumOfAllFullyConfidentRDFTypeFrequenciesInEntry;
				sumOfMaxOfAllFullyConfidentRDFTypeFrequenciesInOnlyPreferredColumnEntries += maxOfAllFullyConfidentRDFTypeFrequenciesInEntry;
			}
		}
	}
	if (rows.size())
	{
		if (numRowsWhereSimplifiedRDFTypesFound < rows.size() && numRowsWhereSimplifiedRDFTypesFound>0)
			coherencyPercentage = 100 * sumOfMaxOfAllFullyConfidentRDFTypeFrequenciesInOnlyPreferredColumnEntries / (numRowsWhereSimplifiedRDFTypesFound * numRowsWhereSimplifiedRDFTypesFound);
		else
			coherencyPercentage = 100 * sumOfMaxOfAllFullyConfidentRDFTypeFrequenciesInOnlyPreferredColumnEntries / (rows.size() * rows.size());
		if (logQuestionDetail)
			lplog(LOG_WHERE, u"Processing table %s: table coherence=%d%% [%d/%d]", tableName.c_str(), coherencyPercentage, numRowsWhereSimplifiedRDFTypesFound, rows.size());
	}
	return coherencyPercentage;
}

// For every cell, count simplified RDF types that sit in titleSynonyms and
// whether the cell's last word is itself a title synonym.  Mark
// lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms on the winning cells of
// each row.  Returns true if >= 75% of rows that have any RDF types also
// have a title-synonym hit (the caller then skips setRowPreference).
bool cColumn::testTitlePreference(cSource* wikipediaSource, lpwstring tableName, unordered_set <lpwstring>& titleSynonyms, bool fileCaching)
{
	if (titleSynonyms.empty())
		return false;
	// if title exists, and is positively associated with 75% or more of the selected words in each row, make those the preferredEntries.
	for (int row = 0; row < rows.size(); row++)
	{
		// accumulate RDFTypeSimplifiedToWordFoundInTitleSynonyms and lastWordFoundInTitleSynonyms for the next test
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
		{
			unordered_map <lpwstring, int > RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
			wikipediaSource->getAssociationMapMaster(rows[row].entries[entry].adaptiveWhere, rows[row].entries[entry].numWords, RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap, LP_TEXT(__func__), fileCaching);
			if (RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.size() > 0)
				rows[row].numSimplifiedRDFTypesFoundForRow++;
			lpwstring confidentSimplifiedRDFTypes, simplifiedRDFTypes;
			for (unordered_map <lpwstring, int >::iterator ri = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.begin(), riEnd = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.end(); ri != riEnd; ri++)
			{
				if (ri->second == 1)
					confidentSimplifiedRDFTypes += u" '" + ri->first + u"'";
				else
					simplifiedRDFTypes += u" '" + ri->first + u"'";
				if (titleSynonyms.find(ri->first) != titleSynonyms.end())
				{
					if (logQuestionDetail)
					{
						lpwstring tmpstr;
						lplog(LOG_WHERE, u"Processing table %s: rowIndex=%d[%d] titleSynonyms=%s simplifiedRDFType=%s", tableName.c_str(), row, entry, setString(titleSynonyms, tmpstr, u" ").c_str(), ri->first.c_str());
					}
					rows[row].entries[entry].RDFTypeSimplifiedToWordFoundInTitleSynonyms++;
				}
			}
			rows[row].entries[entry].simplifiedRDFTypes = (confidentSimplifiedRDFTypes.empty()) ? simplifiedRDFTypes : confidentSimplifiedRDFTypes;
			if (rows[row].entries[entry].RDFTypeSimplifiedToWordFoundInTitleSynonyms > rows[row].maxTitleFound)
				rows[row].maxTitleFound = rows[row].entries[entry].RDFTypeSimplifiedToWordFoundInTitleSynonyms;
			int lastWord = wikipediaSource->m[rows[row].entries[entry].adaptiveWhere].endObjectPosition - 1;
			if (rows[row].entries[entry].lastWordFoundInTitleSynonyms = (titleSynonyms.find(wikipediaSource->m[lastWord].getMainEntry()->first) != titleSynonyms.end()))
				rows[row].numLastWordsFoundInTitleSynonymsInRow++;
			if (logQuestionDetail)
				lplog(LOG_WHERE, u"Processing table %s: rowIndex=%d[%d] table coherency %d:lastWord=%d:%s matchesTitle=%s", tableName.c_str(), row, entry,
					rows[row].entries[entry].adaptiveWhere, lastWord, wikipediaSource->m[lastWord].word->first.c_str(), (rows[row].entries[entry].lastWordFoundInTitleSynonyms) ? u"true" : u"false");
		}
		// accumulate numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow for the next test
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
		{
			// the maximum number of title synonyms matched for this entry (out of all entries for this row) or last word found in title synonyms
			if (rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = (rows[row].maxTitleFound > 0 && rows[row].entries[entry].RDFTypeSimplifiedToWordFoundInTitleSynonyms >= rows[row].maxTitleFound) || rows[row].entries[entry].lastWordFoundInTitleSynonyms)
				rows[row].numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow++;
		}
		if (rows[row].numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow > 0)
			numRowsWhereLastWordOrSimplifiedRDFTypesFoundInTitleSynonyms++;
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
		{
			// it is more likely that the row belongs to the category suggested by the title if the last word of the entry is in the title synonyms, rather than the RDF types.
			if (rows[row].numLastWordOrSimplifiedRDFTypesFoundInTitleSynonymsInRow > 1 && rows[row].numLastWordsFoundInTitleSynonymsInRow > 0)
				rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = rows[row].entries[entry].lastWordFoundInTitleSynonyms;
			lpwstring tmpstr3;
			wikipediaSource->phraseString(rows[row].entries[entry].begin, rows[row].entries[entry].begin + rows[row].entries[entry].numWords, tmpstr3, false);
			if (logQuestionDetail)
				lplog(LOG_WHERE, u"Processing table %s: rowIndex=%d[%d] table coherency of entry %d:%s %s with simplifiedRDFTypes=%s", tableName.c_str(), row, entry, rows[row].entries[entry].adaptiveWhere, tmpstr3.c_str(), (rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms) ? u"lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms!" : u"", rows[row].entries[entry].simplifiedRDFTypes.c_str());
		}
		if (rows[row].numSimplifiedRDFTypesFoundForRow > 0)
			numRowsWhereSimplifiedRDFTypesFound++;
	}
	lplog(LOG_WHERE, u"Processing table %s: title preference: %d entries preferred in %d[out of %d] rows.", tableName.c_str(), numRowsWhereLastWordOrSimplifiedRDFTypesFoundInTitleSynonyms, numRowsWhereSimplifiedRDFTypesFound, rows.size());
	return numRowsWhereLastWordOrSimplifiedRDFTypesFoundInTitleSynonyms > numRowsWhereSimplifiedRDFTypesFound * 3 / 4;
}

// Fallback when the title did not prefer cells: mark the one cell per row
// whose confidence-1 RDF types have the highest column frequency.
void cColumn::setRowPreference(cSource* wikipediaSource, lpwstring tableName, bool fileCaching)
{
	for (int row = 0; row < rows.size(); row++)
	{
		int preferredEntry = -1;
		int maxOfMaxFrequency = -1;
		int maxAccumulatedAssociationValue = -1;
		lpwstring maxOfMaxAssociation;
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
		{
			lpwstring maxAssociation;
			int accumulatedAssociationValue, maxFrequency;
			accumulatedAssociationValue = getSumOfAllFullyConfidentRDFTypeFrequencies(wikipediaSource, row, entry, maxFrequency, maxAssociation, fileCaching);
			if (maxFrequency > maxOfMaxFrequency || (maxFrequency == maxOfMaxFrequency && accumulatedAssociationValue > maxAccumulatedAssociationValue))
			{
				maxAccumulatedAssociationValue = accumulatedAssociationValue;
				preferredEntry = entry;
				maxOfMaxFrequency = maxFrequency;
				maxOfMaxAssociation = maxAssociation;
			}
		}
		if (preferredEntry >= 0)
		{
			rows[row].entries[preferredEntry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms = true;
			lpwstring tmpstr;
			wikipediaSource->phraseString(rows[row].entries[preferredEntry].begin, rows[row].entries[preferredEntry].begin + rows[row].entries[preferredEntry].numWords, tmpstr, false);
			if (logQuestionDetail)
				lplog(LOG_WHERE, u"Processing table %s: table coherency setRowPreference %s rowIndex=%d preferredEntry=%d maxOfMaxFrequency=%d[%s] maxAssociationValue=%d", tableName.c_str(), tmpstr.c_str(),
					row, preferredEntry, maxOfMaxFrequency, maxOfMaxAssociation.c_str(), maxAccumulatedAssociationValue);
		}
	}
}

// Two-pass coherence: accumulate all cells, prefer title-matching (or
// frequency-matching) cells, re-accumulate only those, compute
// coherencyPercentage.  Rejects (returns false) a column that averages more
// than 3 entries/row (too many combinations to trust as a valid list) or
// whose final coherence score is under 90%.  Both reject paths used to be
// disabled ("TEMP DEBUG"), so every Wikipedia table column was kept as a QA
// answer regardless of coherence; re-enabled here.
bool cColumn::determineColumnRDFTypeCoherency(cSource* wikipediaSource, cColumn::cEntry titleEntry, unordered_set <lpwstring>& titleSynonyms, lpwstring tableName, bool keepMusicDomain, bool keepFilmDomain, bool fileCaching)
{
	int sumMaxEntries = 0;
	for (int row = 0; row < rows.size(); row++)
		sumMaxEntries += rows[row].entries.size();
	if (sumMaxEntries / rows.size() > 3) // if there are more than 3 entries, the number of possible combinations is too large to make sure it is actually a valid list.
	{
		if (logQuestionDetail)
			lplog(LOG_WHERE, u"Processing table %s: table coherency averageEntrySize=%d", tableName.c_str(), sumMaxEntries / rows.size());
		return false;
	}
	// accumulate all the types of all the entries in the column together into accumulatedRDFTypesMap
	vector <int> noPreferences;
	accumulateColumnRDFTypes(wikipediaSource, tableName, titleSynonyms, keepMusicDomain, keepFilmDomain, false, fileCaching);
	getMostCommonRDFTypes(u"BEFORE", tableName);
	// prefer the entry in each row of each column that matches with the most common types OR title
	if (!testTitlePreference(wikipediaSource, tableName, titleSynonyms, fileCaching))
		setRowPreference(wikipediaSource, tableName, fileCaching);
	zeroColumnAccumulatedRDFTypes();
	// accumulate all the types of ONLY the lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms entries in the table together
	accumulateColumnRDFTypes(wikipediaSource, tableName, titleSynonyms, keepMusicDomain, keepFilmDomain, true, fileCaching);
	getMostCommonRDFTypes(u"AFTER", tableName);
	if (calculateColumnRDFTypeCoherence(wikipediaSource, titleEntry, tableName, fileCaching) < 90)
		return false;
	return true;
}


// Dump column-level coherence counters and mostCommonAssociationTypeSet
// when logTableCoherenceDetail is on.  'when' is "BEFORE" / "AFTER".
void cColumn::logColumn(int logType, const lpchar_t* when, lpwstring tableName)
{
	if (!logTableCoherenceDetail)
		return;
	if (rows.size() > 2 || (rows.size() > 1 && matchedHeader))
		lplog(logType, u"Processing table %s: table coherency %s numRows=%d numDefinite=%d maxFrequency=%d mostCommonRatio=%d%% matchedHeader=%s "
			u"table coherency [%d out of %d:%d%%] "
			u"table query associations matched=[%d out of %d:%d%%] "
			u"table title associations matched=[%d out of %d:%d%%]",
			tableName.c_str(), when, (int)rows.size(), numDefinite, gMaxFrequency, mostCommonRatio, (matchedHeader) ? u"true" : u"false",
			numCommonObjectAssociations, (int)accumulatedRDFTypesMap.size(), (int)(numCommonObjectAssociations * 100 / ((accumulatedRDFTypesMap.size()) ? accumulatedRDFTypesMap.size() : 1)),
			queryAssociationsMatched, numCommonObjectAssociations, queryAssociationsMatched * 100 / ((numCommonObjectAssociations) ? numCommonObjectAssociations : 1),
			titleAssociationsMatched, numCommonObjectAssociations, titleAssociationsMatched * 100 / ((numCommonObjectAssociations) ? numCommonObjectAssociations : 1));
	for (set < cWordFrequencyMatch >::iterator si = mostCommonAssociationTypeSet.begin(), siEnd = mostCommonAssociationTypeSet.end(); si != siEnd; si++)
	{
		lplog(logType, u"Processing table %s: table coherency %d:%s [queryMatch=%s,titleMatch=%s]", tableName.c_str(), si->frequency, si->word.c_str(), (si->queryObjectMatch) ? u"true" : u"false", (si->titleObjectMatch) ? u"true" : u"false");
	}
}

/*
	int RDFTypeSimplifiedToWordFoundInTitleSynonyms;
	bool lastWordFoundInTitleSynonyms;
	bool lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms;
	bool tableOfContentsFlag;
	int queryAssociationsMatched;
	int titleAssociationsMatched;
	vector <int> matchedQuestionObject;
	vector <int> synonymMatchedQuestionObject;
	lpwstring simplifiedRDFTypes;
*/
// One-line dump of this cell.  row<0 means this is the table title.
void cColumn::cEntry::logEntry(int logType, const lpchar_t* tableName, int row, int entryIndex, cSource* source)
{
	lpwstring tmp;
	if (row < 0)
		lplog(logType, u"Processing table %s: TITLE entry %d:%s matchedToQuery=%d synonymsMatchedToQuery=%d", tableName, adaptiveWhere, source->phraseString(begin, begin + numWords, tmp, false).c_str(),
			matchedQuestionObject.size(), synonymMatchedQuestionObject.size());
	else
		lplog(logType, u"Processing table %s: rowIndex %d[%d] entry %d:%s%s matchedToQuery=%d synonymsMatchedToQuery=%d", tableName, row, entryIndex, adaptiveWhere, source->phraseString(begin, begin + numWords, tmp, false).c_str(),
			(lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms) ? u" [lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms]" : u"", matchedQuestionObject.size(), synonymMatchedQuestionObject.size());
}

// Format "adaptiveWhere:phrase [# matched ... # synonym matched ...]" into
// buffer.
lpwstring cColumn::cEntry::sprint(cSource* source, lpwstring& buffer)
{
	lpwstring phrase, whereStr, matchedQuestionObjectStr, synonymMatchedQuestionObjectStr;
	source->phraseString(begin, begin + numWords, phrase, false);
	itos(adaptiveWhere, whereStr);
	itos(matchedQuestionObject.size(), matchedQuestionObjectStr);
	itos(synonymMatchedQuestionObject.size(), synonymMatchedQuestionObjectStr);
	return buffer = whereStr + u":" + phrase + u" [# matched question object=" + matchedQuestionObjectStr + u" # synonym matched question object=" + synonymMatchedQuestionObjectStr + u"]";
}

const lpchar_t* wikiInvalidTableEntries[] = {
	u"All articles with dead external links", u"All articles with unsourced statements", u"All articles with specifically marked weasel - worded phrases",
	u"All articles to be expanded",	u"Wikipedia articles with VIAF identifiers",	u"Wikipedia articles with LCCN identifiers",	u"Wikipedia articles with ISNI identifiers",
	u"Wikipedia articles with GND identifiers",	u"Wikipedia articles with BNF identifiers",	u"Wikipedia articles with Musicbrainz identifiers",
	u"Commons category template with no category set", u"All Wikipedia articles in need of updating", u"Coordinates on Wikidata", u"Commons category without a link on Wikidata",
	u"Pages containing cite templates with deprecated parameters", u"All articles needing additional references", u"Articles with unsourced statements",
	u"Articles with dead external links", NULL, // links
	u"Create account", u"Log in", NULL, // personalTools
	u"Article", u"Talk", NULL, // nameSpaces
	u"Read", u"Edit", u"View source", u"View history", NULL, // views
	u"Main page", u"Contents", u"Donate to Wikipedia", u"Featured content", u"Current events", u"Random article", u"Wikimedia Shop", NULL, // main
	u"Help", u"About Wikipedia", u"Community portal", u"Recent changes", u"Contact page", NULL, // help
	u"What links here", u"Related changes", u"Upload file", u"Special pages", u"Permanent link", u"Page information", u"Wikidata item", u"Cite this page", NULL, // whatLinksHere
	u"Create a book", u"Download as PDF", u"Printable version", NULL, // output
	u"Dansk", u"Deutsch", u"Español", u"Esperanto", u"Français", u"Italiano", u"Nederlands", u"Polski", u"Svenska", u"Íslenska", u"Scots", u"Română", u"Português", u"Edit links", NULL, NULL }; // languages
unordered_map <lpwstring, int> wikiInvalidTableEntriesMap;
vector <int> expectedNumEntries;
// True if the text of the cell starting at beginEntry is Wikipedia chrome
// (languages, "View history", VIAF, "Articles with dead external links",
// ...).  On first call, builds wikiInvalidTableEntriesMap from the
// NULL-separated wikiInvalidTableEntries groups.  wikiColumns[group] is
// incremented so a later pass can see a consistent chrome category.
bool cSourceTable::isEntryInvalid(int beginEntry, vector <int>& wikiColumns, cSource* wikipediaSource)
{
	if (wikiInvalidTableEntriesMap.empty())
	{
		int tableNum = 0;
		for (int I = 0; wikiInvalidTableEntries[I]; I++, tableNum++)
		{
			int en = I;
			for (int J = I; wikiInvalidTableEntries[J]; J++, I++)
				wikiInvalidTableEntriesMap[wikiInvalidTableEntries[J]] = tableNum;
			expectedNumEntries.push_back(I - en);
		}
	}
	// get current entry
	// before END_COLUMN is always a period, which we also skip (I+1)
	lpwstring entry;
	for (int I = beginEntry; I + 1 < wikipediaSource->m.size() && wikipediaSource->m[I + 1].word != Words.END_COLUMN && wikipediaSource->m[I + 1].word != Words.TABLE; I++)
	{
		wikipediaSource->getOriginalWord(I, entry, true);
		entry += u" ";
	}
	if (entry.length())
		entry.erase(entry.length() - 1);
	unordered_map <lpwstring, int>::iterator itemi;
	if ((itemi = wikiInvalidTableEntriesMap.find(entry)) != wikiInvalidTableEntriesMap.end())
	{
		wikiColumns[itemi->second]++;
		lplog(LOG_WHERE, u"Processing table %s:INVALID entry %d:%s [FOUND INVALID WIKI:%s]", num.c_str(), beginEntry, entry.c_str(), itemi->first.c_str());
		return true;
	}
	if (entry.find(u"Wikipedia") != lpwstring::npos || entry.find(u"Articles needing additional references") != lpwstring::npos || entry.find(u"Articles containing potentially dated statements") != lpwstring::npos)
	{
		wikiColumns[0]++;
		return true;
	}
	if (wikiColumns[0] > 2 && entry.find(u"Articles") != lpwstring::npos)
	{
		wikiColumns[0]++;
		return true;
	}
	for (int I = 0; wikiInvalidTableEntries[I]; I++)
		if (entry.find(wikiInvalidTableEntries[I]) != lpwstring::npos)
		{
			wikiColumns[0]++;
			return true;
		}
	return false;
}

// From source position I (just after the title), read column headers up to
// END_COLUMN_HEADERS, then cells up to the next TABLE.  Only the column
// whose header matched the question (or every column if there are no
// headers) is materialized as objects/entries; others are skipped.
// Always returns true; sets invalidColumn / emptyEntries on the columns.
bool cSourceTable::getTableFromSource(int I, int whereQuestionTypeObject, cSource* wikipediaSource, cSource* questionSource)
{
	lpwstring tmpstr, tmpstr2;
	int row = 0;
	vector <int> wikiInvalidColumnsEntryCount;
	bool matchFound;
	// header columns (each column header ends with ., and the column header section ends with __end_column_headers__)
	for (int numColumnHeaders = 0; I < wikipediaSource->m.size() && wikipediaSource->m[I].word != Words.END_COLUMN_HEADERS; numColumnHeaders++)
	{
		columns.push_back(cColumn());
		int beginColumnHeader = I;
		cColumn::cEntry headerEntry = cColumn::scanColumnEntry(whereQuestionTypeObject, wikipediaSource, questionSource, I, matchFound, num);
		if (headerEntry.matchedQuestionObject.size() > 0 || headerEntry.synonymMatchedQuestionObject.size() > 0)
		{
			columnHeaderMatchTitle = numColumnHeaders;
			columns[numColumnHeaders].matchedHeader = true;
		}
		columnHeaders.push_back(headerEntry);
		if (logTableDetail)
			lplog(LOG_WHERE, u"Processing table %s: of source %s: header columnIndex %d:%s found=%s.", num.c_str(), wikipediaSource->sourcePath.c_str(),
				numColumnHeaders, wikipediaSource->phraseString(beginColumnHeader, I - 3, tmpstr, false).c_str(), wikipediaSource->phraseString(headerEntry.begin, headerEntry.begin + headerEntry.numWords, tmpstr2, false).c_str());
	}
	I += 3;
	if (logTableDetail)
		lplog(LOG_WHERE, u"Processing table %s: of source %s: %d columns found [matching columnIndex #%d]", num.c_str(), wikipediaSource->sourcePath.c_str(), columns.size(), columnHeaderMatchTitle);
	// column going across (row order) (each column ends with .).  If there is no column header, there is only one column.
	if (columnHeaders.size() == 0)
		columns.push_back(cColumn());
	wikiInvalidColumnsEntryCount.resize(10);
	for (; I + 1 < wikipediaSource->m.size() && wikipediaSource->m[I + 1].word != Words.TABLE; row++)
	{
		for (int numColumn = 0; numColumn < columns.size(); numColumn++)
		{
			//int emptyEntries = 0;
			if (numColumn == columnHeaderMatchTitle || columnHeaders.empty())
			{
				int beginColumn = I + 1;
				bool invalidEntry = isEntryInvalid(beginColumn, wikiInvalidColumnsEntryCount, wikipediaSource);
				vector <cColumn::cEntry> entries;
				bool isObject;
				for (; I < wikipediaSource->m.size() && wikipediaSource->m[I].word != Words.END_COLUMN && wikipediaSource->m[I].word != Words.TABLE;)
				{
					vector <cTreeCat*> rdfTypes;
					if (wikipediaSource->m[I].principalWherePosition >= 0 && wikipediaSource->m[I].getObject() < 0 && wikipediaSource->m[wikipediaSource->m[I].principalWherePosition].getObject() >= 0 && I < wikipediaSource->m[I].principalWherePosition)
						I = wikipediaSource->m[I].principalWherePosition;
					if (iswdigit(wikipediaSource->m[I].word->first[0]))
					{
						columns[numColumn].numNumerical++;
						if (wikipediaSource->m[I].endObjectPosition > (signed)I)
							I = wikipediaSource->m[I].endObjectPosition;
						else
							I++;
					}
					else if (wikipediaSource->isEOS(I))
					{
						columns[numColumn].numPunctuation++;
						if (wikipediaSource->m[I].endObjectPosition > (signed)I)
							I = wikipediaSource->m[I].endObjectPosition;
						else
							I++;
					}
					else if (wikipediaSource->m[I].pma.queryPattern(u"_DATE") != -1)
					{
						if (wikipediaSource->m[I].endObjectPosition > (signed)I)
							I = wikipediaSource->m[I].endObjectPosition;
						else
							I++;
					}
					else if ((isObject = wikipediaSource->m[I].getObject() >= 0 && !(wikipediaSource->m[I].flags & cWordMatch::flagAdjectivalObject)) ||
						(wikipediaSource->m[I].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || ((wikipediaSource->m[I].flags & cWordMatch::flagFirstLetterCapitalized)) || (wikipediaSource->m[I].flags & cWordMatch::flagAllCaps)))
					{
						int numWords = 0, numPrepositions = 0;
						columns[numColumn].numDefinite++;
						bool accumulate = false;
						if (!isObject)
						{
							if (wikipediaSource->m[I].principalWherePosition < 0 && analyzeTitle(I, numWords, numPrepositions, num, wikipediaSource) &&
								// reject ISBN and all common words (may be modified later)
								!(numWords == 1 && (wikipediaSource->m[I].queryForm(abbreviationForm) >= 0 || wikipediaSource->m[I].word->second.isCommonWord())))
							{
								wikipediaSource->createObject(cObject(NAME_OBJECT_CLASS, cName(), I, I + numWords, 0, 0, 0, false, false, true, false, false));
								accumulate = true;
							}
							else
							{
								if (wikipediaSource->m[I].principalWherePosition < 0)
									lplog(LOG_WHERE, u"Processing table %s: of source %s: REJECTED [missing principalWherePosition] rowIndex %d columnData %d %s.", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
										numColumn, wikipediaSource->phraseString(I, I + 1, tmpstr2, false).c_str());
								I++;
							}
						}
						else if (wikipediaSource->m[I].endObjectPosition - wikipediaSource->m[I].beginObjectPosition > 1 ||
							(wikipediaSource->m[I].queryForm(abbreviationForm) < 0 && !wikipediaSource->m[I].word->second.isCommonWord())) // reject ISBN and all common words (may be modified later)
						{
							analyzeTitle(wikipediaSource->m[I].beginObjectPosition, numWords, numPrepositions, num, wikipediaSource);
							if (wikipediaSource->m[I].endObjectPosition - wikipediaSource->m[I].beginObjectPosition > numWords)
								numWords = wikipediaSource->m[I].endObjectPosition - wikipediaSource->m[I].beginObjectPosition;
							accumulate = true;
						}
						else
						{
							if (wikipediaSource->m[I].endObjectPosition > (signed)I)
								I = wikipediaSource->m[I].endObjectPosition;
							else
								I++;
						}
						if (accumulate)
						{
							entries.push_back(cColumn::cEntry(wikipediaSource->m[I].beginObjectPosition, I, numWords));
							if (logTableDetail)
							{
								if (columns.size() > 1)
									lplog(LOG_WHERE, u"Processing table %s: of source %s: rowIndex %d columnData %d data found=%s object[%d-%d][%d] numPrepositions=%d", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
										numColumn, wikipediaSource->phraseString(wikipediaSource->m[I].beginObjectPosition, wikipediaSource->m[I].beginObjectPosition + numWords, tmpstr2, false).c_str(), wikipediaSource->m[I].beginObjectPosition, wikipediaSource->m[I].endObjectPosition, I, numPrepositions);
								else
									lplog(LOG_WHERE, u"Processing table %s: of source %s: rowIndex %d data found=%s object[%d-%d][%d] numPrepositions=%d", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
										wikipediaSource->phraseString(wikipediaSource->m[I].beginObjectPosition, wikipediaSource->m[I].beginObjectPosition + numWords, tmpstr2, false).c_str(), wikipediaSource->m[I].beginObjectPosition, wikipediaSource->m[I].endObjectPosition, I, numPrepositions);
							}
							if (numWords <= 0 || wikipediaSource->m[I].beginObjectPosition + numWords < I + 1)
								I++;
							else
								I = wikipediaSource->m[I].beginObjectPosition + numWords;
						}
					}
					else
					{
						I++;
					}
				}
				columns[numColumn].rows.push_back(cColumn::cRow(entries));
				if (invalidEntry)
					columns[numColumn].invalidEntries++;
				if (entries.empty() || entries.size() == 1 && !entries[0].numWords)
				{
					columns[numColumn].emptyEntries++;
					invalidEntry = true;
				}
				if (columns.size() > 1)
					lplog(LOG_WHERE, u"Processing table %s: of source %s: rowIndex %d %sFINISHED:columnData %d:%s.", num.c_str(), wikipediaSource->sourcePath.c_str(), row, (invalidEntry) ? u"INVALID " : u"",
						numColumn, wikipediaSource->phraseString(beginColumn, I - 1, tmpstr, false).c_str());
				else
					lplog(LOG_WHERE, u"Processing table %s: of source %s: rowIndex %d %sFINISHED:%s.", num.c_str(), wikipediaSource->sourcePath.c_str(), row, (invalidEntry) ? u"INVALID " : u"",
						wikipediaSource->phraseString(beginColumn, I - 1, tmpstr, false).c_str());
				I++; // skip period
			}
			else
			{
				int beginColumn = I + 1;
				// skip column
				for (; I < wikipediaSource->m.size() && wikipediaSource->m[I].word != Words.END_COLUMN && wikipediaSource->m[I].word != Words.TABLE; I++);
				if (logTableDetail)
					lplog(LOG_WHERE, u"Processing table %s: of source %s: rowIndex %d columnData %d:%s SKIPPED.", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
						numColumn, wikipediaSource->phraseString(beginColumn, I - 1, tmpstr, false).c_str());
				I++; // skip period
			}
		}
	}
	for (int c = 0; c < columns.size(); c++)
		columns[c].invalidColumn = columns[c].rows.empty() ||
		columns[c].emptyEntries == columns[c].rows.size();
	if (columns.size() == 1 &&
		(columns[0].invalidColumn = columns[0].rows.empty() ||
			columns[0].invalidEntries > 6 ||
			((columns[0].invalidEntries + columns[0].emptyEntries) * 100 / columns[0].rows.size() > 80)))
	{
		int maximumIncorrectEntries = -1;
		for (int J = 0; J < wikiInvalidColumnsEntryCount.size(); J++)
			maximumIncorrectEntries = max(maximumIncorrectEntries, wikiInvalidColumnsEntryCount[J]);
		if (maximumIncorrectEntries < columns[0].invalidEntries) // they are split among different invalid categories - so this is inconsistent
			columns[0].invalidColumn = false;
	}
	if (columns.size() == 1 && !columns[0].invalidColumn && columns[0].invalidEntries)
		lplog(LOG_WHERE, u"Processing table %s: Incomplete REJECTED table?", num.c_str());
	return true;
}

// False if the title span contains "index", "contents", or TOC_HEADER
// (a table-of-contents table is not a fact table).
bool coherentTitle(int begin, int end, cSource* wikipediaSource)
{
	for (int I = begin; I < end; I++)
		if (wikipediaSource->m[I].word->first == u"index" || wikipediaSource->m[I].word->first == u"contents" || wikipediaSource->m[I].word->first == Words.TOC_HEADER->first)
			return false;
	return true;
}

// Parse the table whose TABLE token is at I (I is advanced past the table).
// Fills columns / tableTitleEntry / num.  On a coherent title, runs
// determineColumnRDFTypeCoherency per column, clearing the rows of any
// column it rejects.
//   for each table with a table header, does the table header match the questionTypeObject or its synonyms?
//     if not, and the table has column headers, does any column header match the questionTypeObject or its synonyms?
//    if matched, feed the table or only the selected column into propertyValues.
cSourceTable::cSourceTable(int& I, int whereQuestionTypeObject, cSource* wikipediaSource, cSource* questionSource, bool fileCaching)
{
	LFS
		lpwstring tmpstr, tmpstr2, whereQuestionTypeObjectString;
	columnHeaderMatchTitle = -1;
	// table number .
	int tableNum = I + 1;
	num = wikipediaSource->m[tableNum].word->first;
	source = wikipediaSource;
	I += 3;
	if (logTableDetail)
		lplog(LOG_WHERE, u"Processing table %s: BEGIN of source %s: looking for %d:%s.", num.c_str(), wikipediaSource->sourcePath.c_str(), whereQuestionTypeObject, questionSource->whereString(whereQuestionTypeObject, whereQuestionTypeObjectString, false).c_str());
	bool matchFound = false;
	// process title of table.
	// header . lpendcolumn .
	// a title name should not be counted as a possible answer - (Gene is the column header in the table in article Usher Syndrome (wikipedia))
	tableTitleEntry = cColumn::scanColumnEntry(whereQuestionTypeObject, wikipediaSource, questionSource, I, matchFound, num);
	if (I + 4 >= wikipediaSource->m.size()) return;
	if (logTableDetail)
		tableTitleEntry.logEntry(LOG_WHERE, num.c_str(), -1, -1, wikipediaSource);
	// a missing column in a row is marked with __missing_column__
	getTableFromSource(I, whereQuestionTypeObject, wikipediaSource, questionSource);
	bool invalidTable = true;
	for (int c = 0; invalidTable && c < columns.size(); c++)
		if (!columns[c].invalidColumn)
			invalidTable = false;
	if (invalidTable)
	{
		lplog(LOG_WHERE, u"Processing table %s: REJECTED, all columns invalid", num.c_str());
		return;
	}
	tableTitleEntry.coherentTable = coherentTitle(tableTitleEntry.begin, tableTitleEntry.begin + tableTitleEntry.numWords, wikipediaSource) && !tableTitleEntry.tableOfContentsFlag;
	if (tableTitleEntry.coherentTable)
	{
		unordered_set <lpwstring> titleSynonyms;
		for (int ti = tableTitleEntry.begin; ti < tableTitleEntry.begin + tableTitleEntry.numWords; ti++)
		{
			wikipediaSource->getSynonyms(wikipediaSource->m[ti].getMainEntry()->first, titleSynonyms, NOUN);
			titleSynonyms.insert(wikipediaSource->m[ti].getMainEntry()->first);
		}
		titleSynonyms.insert(wikipediaSource->m[tableTitleEntry.begin].getMainEntry()->first);
		//if (logTableDetail)
		{
			lplog(LOG_WHERE, u"Processing table %s: of source %s: title synonyms for %s:%s",
				num.c_str(), wikipediaSource->sourcePath.c_str(), wikipediaSource->phraseString(tableTitleEntry.begin, tableTitleEntry.begin + tableTitleEntry.numWords, tmpstr, false).c_str(), setString(titleSynonyms, tmpstr, u" ").c_str());
		}
		// ****************************************
		// assess how coherent the table is.  Can we guess a common type for each column of the table?
		bool isAnyColumnCoherent = false;
		for (vector <cColumn>::iterator ci = columns.begin(), ciEnd = columns.end(); ci != ciEnd; ci++)
		{
			if (ci->rows.size() <= 1 || !ci->determineColumnRDFTypeCoherency(wikipediaSource, tableTitleEntry, titleSynonyms, num, false, false, fileCaching))
				ci->rows.clear();
			else
				isAnyColumnCoherent = true;
		}
		tableTitleEntry.coherentTable = isAnyColumnCoherent;
		if (logTableDetail)
			lplog(LOG_WHERE, u"Processing table %s: END", num.c_str());
	}
	else
	{
		lplog(LOG_WHERE, u"Processing table %s: REJECTED, title not coherent.  Marking all columns incoherent", num.c_str());
		for (vector <cColumn>::iterator ci = columns.begin(), ciEnd = columns.end(); ci != ciEnd; ci++)
			ci->coherencyPercentage = 0;
	}
}

// Walk wikipediaSource for Words.TABLE and append every table that has at
// least one non-invalid column (single-column tables also need >1 row).
// fileCaching is the QA member, not a parameter.
void cQuestionAnswering::addTables(cSource* questionSource, int whereQuestionTypeObject, cSource* wikipediaSource, vector < cSourceTable >& wikiTables)
{
	LFS
		for (int where = 0; where < (signed)wikipediaSource->m.size(); where++)
			if (wikipediaSource->m[where].word == Words.TABLE)
			{
				cSourceTable wikiTable(where, whereQuestionTypeObject, wikipediaSource, questionSource, fileCaching);
				if (wikiTable.columns.size() > 0 && ((wikiTable.columns.size() > 1 || !wikiTable.columns[0].invalidColumn) && (wikiTable.columns.size() > 1 || (wikiTable.columns.size() == 1 && wikiTable.columns[0].rows.size() > 1))))
					wikiTables.push_back(wikiTable);
			}
}


// Scan from 'where' to END_COLUMN, recording cells whose mainEntry equals
// (or is a synonym/hypernym of) the question's type-object.  Advances where
// past the trailing " . END_COLUMN . " (where += 2 after the loop).
// matchFound is overwritten on every object (last one wins).
cColumn::cEntry cColumn::scanColumnEntry(int whereQuestionTypeObject, cSource* wikipediaSource, cSource* questionSource, int& where, bool& matchFound, lpwstring tableName)
{
	LFS
		cEntry columnEntry;
	columnEntry.begin = where;
	columnEntry.adaptiveWhere = where;
	lpwstring tmpstr;
	for (; where < wikipediaSource->m.size() && wikipediaSource->m[where].word != Words.END_COLUMN; where++)
	{
		if (wikipediaSource->m[where].word == Words.TOC_HEADER)
			columnEntry.tableOfContentsFlag = true;
		if (wikipediaSource->m[where].getObject() >= 0)
		{
			int ownerWhere;
			if (wikipediaSource->isDefiniteObject(where, u"CHILD MATCHED", ownerWhere, false))
			{
				// we do this test but this was not accurate for 'Awards' matching 'prizes'
				lplog(LOG_WHERE, u"Processing table %s: %s is a definite object (continuing)", tableName.c_str(), wikipediaSource->whereString(where, tmpstr, false).c_str());
			}
			if (matchFound = wikipediaSource->m[where].getMainEntry() == questionSource->m[whereQuestionTypeObject].getMainEntry())
			{
				if (logTableDetail)
					// we do this test but this was not accurate for 'Awards' matching 'prizes'
					lplog(LOG_WHERE, u"Processing table %s: %d:%s matches %d:%s.", tableName.c_str(), where, wikipediaSource->m[where].getMainEntry()->first.c_str(), whereQuestionTypeObject, questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str());
				columnEntry.matchedQuestionObject.push_back(where);
			}
			unordered_set <lpwstring> childSynonyms;
			wikipediaSource->getSynonyms(wikipediaSource->m[where].getMainEntry()->first, childSynonyms, NOUN);
			if (matchFound = childSynonyms.find(questionSource->m[whereQuestionTypeObject].getMainEntry()->first) != childSynonyms.end() ||
				hasHyperNym(wikipediaSource->m[where].getMainEntry()->first, questionSource->m[whereQuestionTypeObject].getMainEntry()->first, matchFound, false))
			{
				if (logTableDetail)
				{
					// we do this test but this was not accurate for 'Awards' matching 'prizes'
					lpwstring tmpstr3;
					if (childSynonyms.find(questionSource->m[whereQuestionTypeObject].getMainEntry()->first) != childSynonyms.end())
						lplog(LOG_WHERE, u"Processing table %s: synonyms of %d:%s (%s) matched %d:%s.", tableName.c_str(), where, wikipediaSource->m[where].getMainEntry()->first.c_str(), setString(childSynonyms, tmpstr3, u" ").c_str(), whereQuestionTypeObject, questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str());
					if (hasHyperNym(wikipediaSource->m[where].getMainEntry()->first, questionSource->m[whereQuestionTypeObject].getMainEntry()->first, matchFound, false))
						lplog(LOG_WHERE, u"Processing table %s: %d:%s has a hypernym of %d:%s.", tableName.c_str(), where, wikipediaSource->m[where].getMainEntry()->first.c_str(), whereQuestionTypeObject, questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str());
				}
				columnEntry.synonymMatchedQuestionObject.push_back(where);
			}
			if (logSynonymDetail)
				lplog(LOG_WHERE, u"Processing table %s: TSYM %s is not found in %s (rejected)", tableName.c_str(), questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str(), setString(childSynonyms, tmpstr, u"|").c_str());
		}
	}
	columnEntry.numWords = where - columnEntry.begin;
	if (wikipediaSource->m[where - 1].word->first == u".")
		columnEntry.numWords--;
	where += 2;
	return columnEntry;
}

// Measure a capitalized title-like run starting at 'where' (proper nouns,
// determiners, prepositions, ordinals, commas).  Truncates at a comma that
// is not followed by a conjunction.  Returns false only if the run is empty
// (then numWords is forced to 1).  Used to decide whether to createObject
// a NAME_OBJECT_CLASS for an unmatched capitalized cell.
bool cSourceTable::analyzeTitle(unsigned int where, int& numWords, int& numPrepositions, lpwstring tableName, cSource* wikipediaSource)
{
	LFS
		numWords = 0;
	// check if from begin to end there is only capitalized words except for determiners or prepositions - 
	// What Do We Need to Know About the International Monetary System? (Paul Krugman)
	// be sensitive to breaks, like / Fundación De Asturias [7047][name][N][F:Fundación M1:De L:Asturias ][region] ( Spain ) , Prince of Asturias Awards in Social Sciences[7051-7062].
	bool allCapitalized = true;
	numPrepositions = 0;
	int lastComma = -1, lastConjunction = -1, firstComma = -1;
	for (unsigned int I = where; I < wikipediaSource->m.size() && wikipediaSource->m[I].word != Words.END_COLUMN && wikipediaSource->m[I].word != Words.TABLE && allCapitalized && !wikipediaSource->isEOS(I) && wikipediaSource->m[I].queryForm(bracketForm) < 0 &&
		(wikipediaSource->m[I].queryForm(PROPER_NOUN_FORM_NUM) >= 0 || (wikipediaSource->m[I].flags & cWordMatch::flagFirstLetterCapitalized) || (wikipediaSource->m[I].flags & cWordMatch::flagAllCaps) ||
			wikipediaSource->m[I].word->first[0] == u',' || wikipediaSource->m[I].word->first[0] == u':' || wikipediaSource->m[I].word->first[0] == u';' || wikipediaSource->m[I].queryForm(determinerForm) >= 0 || wikipediaSource->m[I].queryForm(numeralOrdinalForm) >= 0 || wikipediaSource->m[I].queryForm(prepositionForm) >= 0 || wikipediaSource->m[I].queryForm(coordinatorForm) >= 0);
		I++, numWords++)
	{
		if (wikipediaSource->m[I].queryForm(prepositionForm) >= 0)
			numPrepositions++;
		if (wikipediaSource->m[I].getObject() > 0)
		{
			numWords += (wikipediaSource->m[I].endObjectPosition - 1 - I);
			I = wikipediaSource->m[I].endObjectPosition - 1; // skip objects and periods associated with abbreviations and names
		}
		if (wikipediaSource->m[I].word->first[0] == u',')
		{
			lastComma = I;
			if (firstComma<0 || lastConjunction>firstComma)
				firstComma = I;
		}
		if (wikipediaSource->m[I].queryForm(conjunctionForm) >= 0)
			lastConjunction = I;
	}
	if (lastComma >= 0 && lastConjunction != lastComma + 1 && lastConjunction != lastComma + 2)
	{
		if (logQuestionDetail)
		{
			lpwstring tmpstr, tmpstr2;
			lplog(LOG_WHERE, u"Processing table %s: %d:title %s rejected words after comma resulting in %s", tableName.c_str(), where, wikipediaSource->phraseString(where, where + numWords, tmpstr, true, u" ").c_str(), wikipediaSource->phraseString(where, firstComma, tmpstr2, true, u" ").c_str());
		}
		numWords = firstComma - where;
	}
	while (numWords > 1 && (wikipediaSource->m[where + numWords - 1].word->first[0] == u',' || wikipediaSource->m[where + numWords - 1].queryForm(determinerForm) >= 0 || wikipediaSource->m[where + numWords - 1].queryForm(prepositionForm) >= 0 || wikipediaSource->m[where + numWords - 1].queryForm(coordinatorForm) >= 0))
		numWords--;
	if (wikipediaSource->m[where].endObjectPosition > (int)where + numWords)
		numWords = wikipediaSource->m[where].endObjectPosition - where;
	if (numWords == 0)
	{
		numWords = 1;
		return false;
	}
	return true;
}

