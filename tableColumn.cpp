#include <windows.h>
#include <io.h>
#include "word.h"
#include "ontology.h"
#include "source.h"
#include "QuestionAnswering.h"
#include "profile.h"
#include "wn.h"

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

// remove a domain like film from the accumulated common association map.  This is because Wikipedia is really good at accumulating types of this sort, which biases trying to find a common association.
void cColumn::removeDomainFromAccumulatedRDFTypesMap(const wchar_t* domainAssociations[])
{
	for (int ma = 0; domainAssociations[ma][0]; ma++)
	{
		unordered_map < wstring, cAssociationType >::iterator catmi = accumulatedRDFTypesMap.find(domainAssociations[ma]);
		if (catmi != accumulatedRDFTypesMap.end())
			catmi->second.frequency = 0;
	}
}

// accumulate all rdf types of all entries of all rows in a column.
void cColumn::accumulateColumnRDFTypes(cSource* wikipediaSource, wstring tableName, unordered_set <wstring>& titleSynonyms, bool keepMusicDomain, bool keepFilmDomain, bool onlyPreferred, bool fileCaching)
{
	for (int row = 0; row < rows.size(); row++)
	{
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
			if (!onlyPreferred || rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms)
				rows[row].entries[entry].accumulateEntryRDFTypes(wikipediaSource, tableName, row, entry, titleSynonyms, accumulatedRDFTypesMap, fileCaching);
	}
	const wchar_t* musicAssociations[] = { L"single", L"recording", L"music", L"release", L"album", L"" };
	if (!keepMusicDomain)
		removeDomainFromAccumulatedRDFTypesMap(musicAssociations);
	const wchar_t* filmAssociations[] = { L"film", L"" };
	if (!keepFilmDomain)
		removeDomainFromAccumulatedRDFTypesMap(filmAssociations);
}

// accumulate RDF types from one entry.  
// the RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap map takes all the RDF types associated with a given sequence of words (adaptiveWhere to adaptiveWhere+numWords) and 
//   returns a list of words with confidences estimating how much the RDF type the word is simplified from is associated with the sequence of words.
// take the list of words simplified from RDF types which are associated with the entry, and accumulate the ones with highest confidence into the accumulatedRDFTypesMap for the column.
void cColumn::cEntry::accumulateEntryRDFTypes(cSource* wikipediaSource, wstring tableName, int row, int entry, unordered_set <wstring>& titleSynonyms, unordered_map < wstring, cAssociationType >& accumulatedRDFTypesMap, bool fileCaching)
{
	queryAssociationsMatched = 0;
	titleAssociationsMatched = 0;
	unordered_map <wstring, int > RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
	wikipediaSource->getAssociationMapMaster(adaptiveWhere, numWords, RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap, TEXT(__FUNCTION__), fileCaching);
	for (unordered_map <wstring, int >::iterator ri = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.begin(), riEnd = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.end(); ri != riEnd; ri++)
	{
		if (logTableCoherenceDetail)
			lplog(LOG_WHERE, L"Processing table %s: rowIndex %d entry %d accumulateEntryRDFTypes RDF type %s", tableName.c_str(), row, entry, ri->first.c_str());
		if (ri->second == 1) // only if confidence is 1
		{
			unordered_map < wstring, cAssociationType >::iterator cmi = accumulatedRDFTypesMap.find(ri->first);
			if (cmi == accumulatedRDFTypesMap.end())
			{
				accumulatedRDFTypesMap[ri->first].frequency = 1;
				unordered_set <wstring >::iterator wami;
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
void cColumn::getMostCommonRDFTypes(const wchar_t* when, wstring tableName)
{
	mostCommonAssociationTypeSet.clear();
	numCommonObjectAssociations = 0;
	for (unordered_map < wstring, cAssociationType >::iterator cmi = accumulatedRDFTypesMap.begin(), cmiEnd = accumulatedRDFTypesMap.end(); cmi != cmiEnd; cmi++)
	{
		if (logTableCoherenceDetail)
			lplog(LOG_WHERE, L"Processing table %s: getMostCommonRDFTypes RDF type coherency %s numRows= %d RDFType=%s Frequency=%d", tableName.c_str(),
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
		lplog(LOG_WHERE, L"Processing table %s: table coherency %s numRows= %d numCommonObjectAssociations=%d queryAssociationsMatched=%d titleAssociationsMatched=%d", tableName.c_str(),
			when, rows.size(), numCommonObjectAssociations, queryAssociationsMatched, titleAssociationsMatched);
	if (mostCommonAssociationTypeSet.size() > 0 && rows.size() > 0)
		mostCommonRatio = ((mostCommonAssociationTypeSet.begin()))->frequency * 100 / rows.size();
	logColumn(LOG_WHERE, when, tableName);
}

void cColumn::zeroColumnAccumulatedRDFTypes()
{
	accumulatedRDFTypesMap.clear();
}

int cColumn::getSumOfAllFullyConfidentRDFTypeFrequencies(cSource* wikipediaSource, int row, int entry, int& maxOfAllFullyConfidentRDFTypeFrequencies, wstring& fullyConfidentSimplifiedRDFTypeWithMaximumFrequency, bool fileCaching)
{
	unordered_map <wstring, int > RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
	wikipediaSource->getAssociationMapMaster(rows[row].entries[entry].adaptiveWhere, rows[row].entries[entry].numWords, RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap, TEXT(__FUNCTION__), fileCaching);
	int sumOfAllFullyConfidentRDFTypeFrequencies = 0;
	maxOfAllFullyConfidentRDFTypeFrequencies = 0;
	for (unordered_map <wstring, int >::iterator ri = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.begin(), riEnd = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.end(); ri != riEnd; ri++)
	{
		if (ri->second == 1) // only use RDFTypes with a certain confidence
		{
			unordered_map < wstring, cAssociationType >::iterator cmi = accumulatedRDFTypesMap.find(ri->first);
			if (cmi == accumulatedRDFTypesMap.end())
				lplog(LOG_FATAL_ERROR, L"association for word %s not found in common columnIndex map!", ri->first.c_str());
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
int cColumn::calculateColumnRDFTypeCoherence(cSource* wikipediaSource, cColumn::cEntry titleEntry, wstring tableName, bool fileCaching)
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
				wstring fullyConfidentSimplifiedRDFTypeWithMaximumFrequencyInEntry;
				int maxOfAllFullyConfidentRDFTypeFrequenciesInEntry = 0, sumOfAllFullyConfidentRDFTypeFrequenciesInEntry;
				sumOfAllFullyConfidentRDFTypeFrequenciesInEntry = getSumOfAllFullyConfidentRDFTypeFrequencies(wikipediaSource, row, entry, maxOfAllFullyConfidentRDFTypeFrequenciesInEntry, fullyConfidentSimplifiedRDFTypeWithMaximumFrequencyInEntry, fileCaching);
				if (!maxOfAllFullyConfidentRDFTypeFrequenciesInEntry)
				{
					maxOfAllFullyConfidentRDFTypeFrequenciesInEntry = rows.size();
					wikipediaSource->phraseString(titleEntry.begin, titleEntry.begin + titleEntry.numWords, fullyConfidentSimplifiedRDFTypeWithMaximumFrequencyInEntry, true);
				}
				wstring tmpstr;
				wikipediaSource->phraseString(rows[row].entries[entry].begin, rows[row].entries[entry].begin + rows[row].entries[entry].numWords, tmpstr, false);
				if (logQuestionDetail)
					lplog(LOG_WHERE, L"Processing table %s: table coherency %s rowIndex=%d entry=%d associationValue=%d maxFrequency=%d[%s]", tableName.c_str(), tmpstr.c_str(),
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
			lplog(LOG_WHERE, L"Processing table %s: table coherence=%d%% [%d/%d]", tableName.c_str(), coherencyPercentage, numRowsWhereSimplifiedRDFTypesFound, rows.size());
	}
	return coherencyPercentage;
}

bool cColumn::testTitlePreference(cSource* wikipediaSource, wstring tableName, unordered_set <wstring>& titleSynonyms, bool fileCaching)
{
	if (titleSynonyms.empty())
		return false;
	// if title exists, and is positively associated with 75% or more of the selected words in each row, make those the preferredEntries.
	for (int row = 0; row < rows.size(); row++)
	{
		// accumulate RDFTypeSimplifiedToWordFoundInTitleSynonyms and lastWordFoundInTitleSynonyms for the next test
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
		{
			unordered_map <wstring, int > RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap;
			wikipediaSource->getAssociationMapMaster(rows[row].entries[entry].adaptiveWhere, rows[row].entries[entry].numWords, RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap, TEXT(__FUNCTION__), fileCaching);
			if (RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.size() > 0)
				rows[row].numSimplifiedRDFTypesFoundForRow++;
			wstring confidentSimplifiedRDFTypes, simplifiedRDFTypes;
			for (unordered_map <wstring, int >::iterator ri = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.begin(), riEnd = RDFTypeSimplificationToWordAssociationWithObject_toConfidenceMap.end(); ri != riEnd; ri++)
			{
				if (ri->second == 1)
					confidentSimplifiedRDFTypes += L" '" + ri->first + L"'";
				else
					simplifiedRDFTypes += L" '" + ri->first + L"'";
				if (titleSynonyms.find(ri->first) != titleSynonyms.end())
				{
					if (logQuestionDetail)
					{
						wstring tmpstr;
						lplog(LOG_WHERE, L"Processing table %s: rowIndex=%d[%d] titleSynonyms=%s simplifiedRDFType=%s", tableName.c_str(), row, entry, setString(titleSynonyms, tmpstr, L" ").c_str(), ri->first.c_str());
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
				lplog(LOG_WHERE, L"Processing table %s: rowIndex=%d[%d] table coherency %d:lastWord=%d:%s matchesTitle=%s", tableName.c_str(), row, entry,
					rows[row].entries[entry].adaptiveWhere, lastWord, wikipediaSource->m[lastWord].word->first.c_str(), (rows[row].entries[entry].lastWordFoundInTitleSynonyms) ? L"true" : L"false");
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
			wstring tmpstr3;
			wikipediaSource->phraseString(rows[row].entries[entry].begin, rows[row].entries[entry].begin + rows[row].entries[entry].numWords, tmpstr3, false);
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"Processing table %s: rowIndex=%d[%d] table coherency of entry %d:%s %s with simplifiedRDFTypes=%s", tableName.c_str(), row, entry, rows[row].entries[entry].adaptiveWhere, tmpstr3.c_str(), (rows[row].entries[entry].lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms) ? L"lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms!" : L"", rows[row].entries[entry].simplifiedRDFTypes.c_str());
		}
		if (rows[row].numSimplifiedRDFTypesFoundForRow > 0)
			numRowsWhereSimplifiedRDFTypesFound++;
	}
	lplog(LOG_WHERE, L"Processing table %s: title preference: %d entries preferred in %d[out of %d] rows.", tableName.c_str(), numRowsWhereLastWordOrSimplifiedRDFTypesFoundInTitleSynonyms, numRowsWhereSimplifiedRDFTypesFound, rows.size());
	return numRowsWhereLastWordOrSimplifiedRDFTypesFoundInTitleSynonyms > numRowsWhereSimplifiedRDFTypesFound * 3 / 4;
}

void cColumn::setRowPreference(cSource* wikipediaSource, wstring tableName, bool fileCaching)
{
	for (int row = 0; row < rows.size(); row++)
	{
		int preferredEntry = -1;
		int maxOfMaxFrequency = -1;
		int maxAccumulatedAssociationValue = -1;
		wstring maxOfMaxAssociation;
		for (int entry = 0; entry < rows[row].entries.size(); entry++)
		{
			wstring maxAssociation;
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
			wstring tmpstr;
			wikipediaSource->phraseString(rows[row].entries[preferredEntry].begin, rows[row].entries[preferredEntry].begin + rows[row].entries[preferredEntry].numWords, tmpstr, false);
			if (logQuestionDetail)
				lplog(LOG_WHERE, L"Processing table %s: table coherency setRowPreference %s rowIndex=%d preferredEntry=%d maxOfMaxFrequency=%d[%s] maxAssociationValue=%d", tableName.c_str(), tmpstr.c_str(),
					row, preferredEntry, maxOfMaxFrequency, maxOfMaxAssociation.c_str(), maxAccumulatedAssociationValue);
		}
	}
}

bool cColumn::determineColumnRDFTypeCoherency(cSource* wikipediaSource, cColumn::cEntry titleEntry, unordered_set <wstring>& titleSynonyms, wstring tableName, bool keepMusicDomain, bool keepFilmDomain, bool fileCaching)
{
	int sumMaxEntries = 0;
	for (int row = 0; row < rows.size(); row++)
		sumMaxEntries += rows[row].entries.size();
	if (sumMaxEntries / rows.size() > 3) // if there are more than 3 entries, the number of possible combinations is too large to make sure it is actually a valid list.
	{
		if (logQuestionDetail)
			lplog(LOG_WHERE, L"Processing table %s: table coherency averageEntrySize=%d", tableName.c_str(), sumMaxEntries / rows.size());
		//return false; // TEMP DEBUG
	}
	// accumulate all the types of all the entries in the column together into accumulatedRDFTypesMap
	vector <int> noPreferences;
	accumulateColumnRDFTypes(wikipediaSource, tableName, titleSynonyms, keepMusicDomain, keepFilmDomain, false, fileCaching);
	getMostCommonRDFTypes(L"BEFORE", tableName);
	// prefer the entry in each row of each column that matches with the most common types OR title
	if (!testTitlePreference(wikipediaSource, tableName, titleSynonyms, fileCaching))
		setRowPreference(wikipediaSource, tableName, fileCaching);
	zeroColumnAccumulatedRDFTypes();
	// accumulate all the types of ONLY the lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms entries in the table together
	accumulateColumnRDFTypes(wikipediaSource, tableName, titleSynonyms, keepMusicDomain, keepFilmDomain, true, fileCaching);
	getMostCommonRDFTypes(L"AFTER", tableName);
	if (calculateColumnRDFTypeCoherence(wikipediaSource, titleEntry, tableName, fileCaching) < 90)
		return true;
	return true;
}


void cColumn::logColumn(int logType, const wchar_t* when, wstring tableName)
{
	if (!logTableCoherenceDetail)
		return;
	if (rows.size() > 2 || (rows.size() > 1 && matchedHeader))
		lplog(logType, L"Processing table %s: table coherency %s numRows=%d numDefinite=%d maxFrequency=%d mostCommonRatio=%d%% matchedHeader=%s "
			L"table coherency [%d out of %d:%d%%] "
			L"table query associations matched=[%d out of %d:%d%%] "
			L"table title associations matched=[%d out of %d:%d%%]",
			tableName.c_str(), when, (int)rows.size(), numDefinite, gMaxFrequency, mostCommonRatio, (matchedHeader) ? L"true" : L"false",
			numCommonObjectAssociations, (int)accumulatedRDFTypesMap.size(), (int)(numCommonObjectAssociations * 100 / ((accumulatedRDFTypesMap.size()) ? accumulatedRDFTypesMap.size() : 1)),
			queryAssociationsMatched, numCommonObjectAssociations, queryAssociationsMatched * 100 / ((numCommonObjectAssociations) ? numCommonObjectAssociations : 1),
			titleAssociationsMatched, numCommonObjectAssociations, titleAssociationsMatched * 100 / ((numCommonObjectAssociations) ? numCommonObjectAssociations : 1));
	for (set < cWordFrequencyMatch >::iterator si = mostCommonAssociationTypeSet.begin(), siEnd = mostCommonAssociationTypeSet.end(); si != siEnd; si++)
	{
		lplog(logType, L"Processing table %s: table coherency %d:%s [queryMatch=%s,titleMatch=%s]", tableName.c_str(), si->frequency, si->word.c_str(), (si->queryObjectMatch) ? L"true" : L"false", (si->titleObjectMatch) ? L"true" : L"false");
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
	wstring simplifiedRDFTypes;
*/
void cColumn::cEntry::logEntry(int logType, const wchar_t* tableName, int row, int entryIndex, cSource* source)
{
	wstring tmp;
	if (row < 0)
		lplog(logType, L"Processing table %s: TITLE entry %d:%s matchedToQuery=%d synonymsMatchedToQuery=%d", tableName, adaptiveWhere, source->phraseString(begin, begin + numWords, tmp, false).c_str(),
			matchedQuestionObject.size(), synonymMatchedQuestionObject.size());
	else
		lplog(logType, L"Processing table %s: rowIndex %d[%d] entry %d:%s%s matchedToQuery=%d synonymsMatchedToQuery=%d", tableName, row, entryIndex, adaptiveWhere, source->phraseString(begin, begin + numWords, tmp, false).c_str(),
			(lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms) ? L" [lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms]" : L"", matchedQuestionObject.size(), synonymMatchedQuestionObject.size());
}

wstring cColumn::cEntry::sprint(cSource* source, wstring& buffer)
{
	wstring phrase, whereStr, matchedQuestionObjectStr, synonymMatchedQuestionObjectStr;
	source->phraseString(begin, begin + numWords, phrase, false);
	itos(adaptiveWhere, whereStr);
	itos(matchedQuestionObject.size(), matchedQuestionObjectStr);
	itos(synonymMatchedQuestionObject.size(), synonymMatchedQuestionObjectStr);
	return buffer = whereStr + L":" + phrase + L" [# matched question object=" + matchedQuestionObjectStr + L" # synonym matched question object=" + matchedQuestionObjectStr + L"]";
}

const wchar_t* wikiInvalidTableEntries[] = {
	L"All articles with dead external links", L"All articles with unsourced statements", L"All articles with specifically marked weasel - worded phrases",
	L"All articles to be expanded",	L"Wikipedia articles with VIAF identifiers",	L"Wikipedia articles with LCCN identifiers",	L"Wikipedia articles with ISNI identifiers",
	L"Wikipedia articles with GND identifiers",	L"Wikipedia articles with BNF identifiers",	L"Wikipedia articles with Musicbrainz identifiers",
	L"Commons category template with no category set", L"All Wikipedia articles in need of updating", L"Coordinates on Wikidata", L"Commons category without a link on Wikidata",
	L"Pages containing cite templates with deprecated parameters", L"All articles needing additional references", L"Articles with unsourced statements",
	L"Articles with dead external links", NULL, // links
	L"Create account", L"Log in", NULL, // personalTools
	L"Article", L"Talk", NULL, // nameSpaces
	L"Read", L"Edit", L"View source", L"View history", NULL, // views
	L"Main page", L"Contents", L"Donate to Wikipedia", L"Featured content", L"Current events", L"Random article", L"Wikimedia Shop", NULL, // main
	L"Help", L"About Wikipedia", L"Community portal", L"Recent changes", L"Contact page", NULL, // help
	L"What links here", L"Related changes", L"Upload file", L"Special pages", L"Permanent link", L"Page information", L"Wikidata item", L"Cite this page", NULL, // whatLinksHere
	L"Create a book", L"Download as PDF", L"Printable version", NULL, // output
	L"Dansk", L"Deutsch", L"Español", L"Esperanto", L"Français", L"Italiano", L"Nederlands", L"Polski", L"Svenska", L"Íslenska", L"Scots", L"Română", L"Português", L"Edit links", NULL, NULL }; // languages
unordered_map <wstring, int> wikiInvalidTableEntriesMap;
vector <int> expectedNumEntries;
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
	wstring entry;
	for (int I = beginEntry; I + 1 < wikipediaSource->m.size() && wikipediaSource->m[I + 1].word != Words.END_COLUMN && wikipediaSource->m[I + 1].word != Words.TABLE; I++)
	{
		wikipediaSource->getOriginalWord(I, entry, true);
		entry += L" ";
	}
	if (entry.length())
		entry.erase(entry.length() - 1);
	unordered_map <wstring, int>::iterator itemi;
	if ((itemi = wikiInvalidTableEntriesMap.find(entry)) != wikiInvalidTableEntriesMap.end())
	{
		wikiColumns[itemi->second]++;
		lplog(LOG_WHERE, L"Processing table %s:INVALID entry %d:%s [FOUND INVALID WIKI:%s]", num.c_str(), beginEntry, entry.c_str(), itemi->first.c_str());
		return true;
	}
	if (entry.find(L"Wikipedia") != wstring::npos || entry.find(L"Articles needing additional references") != wstring::npos || entry.find(L"Articles containing potentially dated statements") != wstring::npos)
	{
		wikiColumns[0]++;
		return true;
	}
	if (wikiColumns[0] > 2 && entry.find(L"Articles") != wstring::npos)
	{
		wikiColumns[0]++;
		return true;
	}
	for (int I = 0; wikiInvalidTableEntries[I]; I++)
		if (entry.find(wikiInvalidTableEntries[I]) != wstring::npos)
		{
			wikiColumns[0]++;
			return true;
		}
	return false;
}

bool cSourceTable::getTableFromSource(int I, int whereQuestionTypeObject, cSource* wikipediaSource, cSource* questionSource)
{
	wstring tmpstr, tmpstr2;
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
			lplog(LOG_WHERE, L"Processing table %s: of source %s: header columnIndex %d:%s found=%s.", num.c_str(), wikipediaSource->sourcePath.c_str(),
				numColumnHeaders, wikipediaSource->phraseString(beginColumnHeader, I - 3, tmpstr, false).c_str(), wikipediaSource->phraseString(headerEntry.begin, headerEntry.begin + headerEntry.numWords, tmpstr2, false).c_str());
	}
	I += 3;
	if (logTableDetail)
		lplog(LOG_WHERE, L"Processing table %s: of source %s: %d columns found [matching columnIndex #%d]", num.c_str(), wikipediaSource->sourcePath.c_str(), columns.size(), columnHeaderMatchTitle);
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
					else if (wikipediaSource->m[I].pma.queryPattern(L"_DATE") != -1)
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
									lplog(LOG_WHERE, L"Processing table %s: of source %s: REJECTED [missing principalWherePosition] rowIndex %d columnData %d %s.", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
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
									lplog(LOG_WHERE, L"Processing table %s: of source %s: rowIndex %d columnData %d data found=%s object[%d-%d][%d] numPrepositions=%d", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
										numColumn, wikipediaSource->phraseString(wikipediaSource->m[I].beginObjectPosition, wikipediaSource->m[I].beginObjectPosition + numWords, tmpstr2, false).c_str(), wikipediaSource->m[I].beginObjectPosition, wikipediaSource->m[I].endObjectPosition, I, numPrepositions);
								else
									lplog(LOG_WHERE, L"Processing table %s: of source %s: rowIndex %d data found=%s object[%d-%d][%d] numPrepositions=%d", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
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
					lplog(LOG_WHERE, L"Processing table %s: of source %s: rowIndex %d %sFINISHED:columnData %d:%s.", num.c_str(), wikipediaSource->sourcePath.c_str(), row, (invalidEntry) ? L"INVALID " : L"",
						numColumn, wikipediaSource->phraseString(beginColumn, I - 1, tmpstr, false).c_str());
				else
					lplog(LOG_WHERE, L"Processing table %s: of source %s: rowIndex %d %sFINISHED:%s.", num.c_str(), wikipediaSource->sourcePath.c_str(), row, (invalidEntry) ? L"INVALID " : L"",
						wikipediaSource->phraseString(beginColumn, I - 1, tmpstr, false).c_str());
				I++; // skip period
			}
			else
			{
				int beginColumn = I + 1;
				// skip column
				for (; I < wikipediaSource->m.size() && wikipediaSource->m[I].word != Words.END_COLUMN && wikipediaSource->m[I].word != Words.TABLE; I++);
				if (logTableDetail)
					lplog(LOG_WHERE, L"Processing table %s: of source %s: rowIndex %d columnData %d:%s SKIPPED.", num.c_str(), wikipediaSource->sourcePath.c_str(), row,
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
		lplog(LOG_WHERE, L"Processing table %s: Incomplete REJECTED table?", num.c_str());
	return true;
}

bool coherentTitle(int begin, int end, cSource* wikipediaSource)
{
	for (int I = begin; I < end; I++)
		if (wikipediaSource->m[I].word->first == L"index" || wikipediaSource->m[I].word->first == L"contents" || wikipediaSource->m[I].word->first == Words.TOC_HEADER->first)
			return false;
	return true;
}

//   for each table with a table header, does the table header match the questionTypeObject or its synonyms?
//     if not, and the table has column headers, does any column header match the questionTypeObject or its synonyms?
//    if matched, feed the table or only the selected column into propertyValues.
cSourceTable::cSourceTable(int& I, int whereQuestionTypeObject, cSource* wikipediaSource, cSource* questionSource, bool fileCaching)
{
	LFS
		wstring tmpstr, tmpstr2, whereQuestionTypeObjectString;
	columnHeaderMatchTitle = -1;
	// table number .
	int tableNum = I + 1;
	num = wikipediaSource->m[tableNum].word->first;
	source = wikipediaSource;
	I += 3;
	if (logTableDetail)
		lplog(LOG_WHERE, L"Processing table %s: BEGIN of source %s: looking for %d:%s.", num.c_str(), wikipediaSource->sourcePath.c_str(), whereQuestionTypeObject, questionSource->whereString(whereQuestionTypeObject, whereQuestionTypeObjectString, false).c_str());
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
		lplog(LOG_WHERE, L"Processing table %s: REJECTED, all columns invalid", num.c_str());
		return;
	}
	tableTitleEntry.coherentTable = coherentTitle(tableTitleEntry.begin, tableTitleEntry.begin + tableTitleEntry.numWords, wikipediaSource) && !tableTitleEntry.tableOfContentsFlag;
	if (tableTitleEntry.coherentTable)
	{
		unordered_set <wstring> titleSynonyms;
		for (int ti = tableTitleEntry.begin; ti < tableTitleEntry.begin + tableTitleEntry.numWords; ti++)
		{
			wikipediaSource->getSynonyms(wikipediaSource->m[ti].getMainEntry()->first, titleSynonyms, NOUN);
			titleSynonyms.insert(wikipediaSource->m[ti].getMainEntry()->first);
		}
		titleSynonyms.insert(wikipediaSource->m[tableTitleEntry.begin].getMainEntry()->first);
		//if (logTableDetail)
		{
			lplog(LOG_WHERE, L"Processing table %s: of source %s: title synonyms for %s:%s",
				num.c_str(), wikipediaSource->sourcePath.c_str(), wikipediaSource->phraseString(tableTitleEntry.begin, tableTitleEntry.begin + tableTitleEntry.numWords, tmpstr, false).c_str(), setString(titleSynonyms, tmpstr, L" ").c_str());
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
			lplog(LOG_WHERE, L"Processing table %s: END", num.c_str());
	}
	else
	{
		lplog(LOG_WHERE, L"Processing table %s: REJECTED, title not coherent.  Marking all columns incoherent", num.c_str());
		for (vector <cColumn>::iterator ci = columns.begin(), ciEnd = columns.end(); ci != ciEnd; ci++)
			ci->coherencyPercentage = 0;
	}
}

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


cColumn::cEntry cColumn::scanColumnEntry(int whereQuestionTypeObject, cSource* wikipediaSource, cSource* questionSource, int& where, bool& matchFound, wstring tableName)
{
	LFS
		cEntry columnEntry;
	columnEntry.begin = where;
	columnEntry.adaptiveWhere = where;
	wstring tmpstr;
	for (; where < wikipediaSource->m.size() && wikipediaSource->m[where].word != Words.END_COLUMN; where++)
	{
		if (wikipediaSource->m[where].word == Words.TOC_HEADER)
			columnEntry.tableOfContentsFlag = true;
		if (wikipediaSource->m[where].getObject() >= 0)
		{
			int ownerWhere;
			if (wikipediaSource->isDefiniteObject(where, L"CHILD MATCHED", ownerWhere, false))
			{
				// we do this test but this was not accurate for 'Awards' matching 'prizes'
				lplog(LOG_WHERE, L"Processing table %s: %s is a definite object (continuing)", tableName.c_str(), wikipediaSource->whereString(where, tmpstr, false).c_str());
			}
			if (matchFound = wikipediaSource->m[where].getMainEntry() == questionSource->m[whereQuestionTypeObject].getMainEntry())
			{
				if (logTableDetail)
					// we do this test but this was not accurate for 'Awards' matching 'prizes'
					lplog(LOG_WHERE, L"Processing table %s: %d:%s matches %d:%s.", tableName.c_str(), where, wikipediaSource->m[where].getMainEntry()->first.c_str(), whereQuestionTypeObject, questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str());
				columnEntry.matchedQuestionObject.push_back(where);
			}
			unordered_set <wstring> childSynonyms;
			wikipediaSource->getSynonyms(wikipediaSource->m[where].getMainEntry()->first, childSynonyms, NOUN);
			if (matchFound = childSynonyms.find(questionSource->m[whereQuestionTypeObject].getMainEntry()->first) != childSynonyms.end() ||
				hasHyperNym(wikipediaSource->m[where].getMainEntry()->first, questionSource->m[whereQuestionTypeObject].getMainEntry()->first, matchFound, false))
			{
				if (logTableDetail)
				{
					// we do this test but this was not accurate for 'Awards' matching 'prizes'
					wstring tmpstr3;
					if (childSynonyms.find(questionSource->m[whereQuestionTypeObject].getMainEntry()->first) != childSynonyms.end())
						lplog(LOG_WHERE, L"Processing table %s: synonyms of %d:%s (%s) matched %d:%s.", tableName.c_str(), where, wikipediaSource->m[where].getMainEntry()->first.c_str(), setString(childSynonyms, tmpstr3, L" ").c_str(), whereQuestionTypeObject, questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str());
					if (hasHyperNym(wikipediaSource->m[where].getMainEntry()->first, questionSource->m[whereQuestionTypeObject].getMainEntry()->first, matchFound, false))
						lplog(LOG_WHERE, L"Processing table %s: %d:%s has a hypernym of %d:%s.", tableName.c_str(), where, wikipediaSource->m[where].getMainEntry()->first.c_str(), whereQuestionTypeObject, questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str());
				}
				columnEntry.synonymMatchedQuestionObject.push_back(where);
			}
			if (logSynonymDetail)
				lplog(LOG_WHERE, L"Processing table %s: TSYM %s is not found in %s (rejected)", tableName.c_str(), questionSource->m[whereQuestionTypeObject].getMainEntry()->first.c_str(), setString(childSynonyms, tmpstr, L"|").c_str());
		}
	}
	columnEntry.numWords = where - columnEntry.begin;
	if (wikipediaSource->m[where - 1].word->first == L".")
		columnEntry.numWords--;
	where += 2;
	return columnEntry;
}

bool cSourceTable::analyzeTitle(unsigned int where, int& numWords, int& numPrepositions, wstring tableName, cSource* wikipediaSource)
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
			wikipediaSource->m[I].word->first[0] == L',' || wikipediaSource->m[I].word->first[0] == L':' || wikipediaSource->m[I].word->first[0] == L';' || wikipediaSource->m[I].queryForm(determinerForm) >= 0 || wikipediaSource->m[I].queryForm(numeralOrdinalForm) >= 0 || wikipediaSource->m[I].queryForm(prepositionForm) >= 0 || wikipediaSource->m[I].queryForm(coordinatorForm) >= 0);
		I++, numWords++)
	{
		if (wikipediaSource->m[I].queryForm(prepositionForm) >= 0)
			numPrepositions++;
		if (wikipediaSource->m[I].getObject() > 0)
		{
			numWords += (wikipediaSource->m[I].endObjectPosition - 1 - I);
			I = wikipediaSource->m[I].endObjectPosition - 1; // skip objects and periods associated with abbreviations and names
		}
		if (wikipediaSource->m[I].word->first[0] == L',')
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
			wstring tmpstr, tmpstr2;
			lplog(LOG_WHERE, L"Processing table %s: %d:title %s rejected words after comma resulting in %s", tableName.c_str(), where, wikipediaSource->phraseString(where, where + numWords, tmpstr, true, L" ").c_str(), wikipediaSource->phraseString(where, firstComma, tmpstr2, true, L" ").c_str());
		}
		numWords = firstComma - where;
	}
	while (numWords > 1 && (wikipediaSource->m[where + numWords - 1].word->first[0] == L',' || wikipediaSource->m[where + numWords - 1].queryForm(determinerForm) >= 0 || wikipediaSource->m[where + numWords - 1].queryForm(prepositionForm) >= 0 || wikipediaSource->m[where + numWords - 1].queryForm(coordinatorForm) >= 0))
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

