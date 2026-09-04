/*
	paice.h - Paice/Husk stemmer (cStemmer) used to recover a word's mainEntry and inflection

	Overview:
		A C++ port of the 1994 Paice/Husk suffix stemmer, later extended by Zamora
		(MINSTEMSIZE, 2-digit state markers) and then by this project with prefix
		stripping against the words table and a closed-class reject list
		(wordIsNotUnknownAndOpen).  Rules are loaded once from
		source\\lists\\suffixRules.txt and prefixRules.txt.

	Pipeline position:
		Called from cWord::attemptDisInclination / parseWord during lexicon lookup
		(initialization and tokenize) when a surface form is not already in Words.

	Key entry points:
		- stem() - apply every suffix rule (recursively on 'continue'), then prefixes.
		- findLastFormInflection() - walk a result's trail to recover form + inflection.
		- isWordDBUnknown() - true if the candidate stem is UNDEFINED in memory or DB.
		- wordIsNotUnknownAndOpen() - false if the word carries a closed-class form.

	Key data structures / globals:
		- cSuffixRule - one suffixRules.txt line: keystr/repstr/form/inflection/flags/trail.
		- tPrefixRule - one prefixRules.txt line: keystr/repstr/rulenum.
		- stemRules / prefixRules - loaded lazily on first stem() / stripPrefix().
		- unacceptableCombinationForms - closed-class form ids filled on first use.

	Notes / gotchas:
		- trail is a cIntArray of rule line numbers (negative for prefixes) recording
			which rules produced a given candidate; encode/decode on cIntArray packs
			these into a 30-bit int elsewhere.
		- stem() is recursive and each frame has an LFS, so PROFILE builds explode
			functionPath on long suffix chains.
		- findLastFormInflection takes rulesUsed by value (full copy).
*/
//#pragma warning (disable: 4503)
//#pragma warning (disable: 4996)
//#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
using namespace std;
#include "mysql.h"
#include <stdio.h>
//#include "intarray.h"

#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
class cStemmer
{

public:
	class cSuffixRule
	{
	public:
		lpwstring text; /* To return stemmer output */
		lpwstring keystr; /* Key lpwstring,ie,suffix to remove */
		lpwstring repstr; /* lpwstring  to replace deleted letters */
		lpwstring form;
		int inflection;
		int rulenum; /* Line number of rule in rule list file */
		bool intact; /* Boolean-must word be intact? */
		bool cont; /* Boolean-continue with another rule? */
		bool protect; /* Boolean-protect this ending? */
		cIntArray trail;
		// Zero inflection/flags; rulenum=-1 (not yet bound to a file line).
		cSuffixRule()
		{
			inflection=0;
			rulenum=-1; /* Line number of rule in rule list file */
			intact=false; /* Boolean-must word be intact? */
			cont=false; /* Boolean-continue with another rule? */
			protect=false; /* Boolean-protect this ending? */
		}
		/*~cSuffixRule()
		{
		}
		
		cSuffixRule(const cSuffixRule &rhs)
		{
			text = rhs.text; // To return stemmer output 
			keystr = rhs.keystr; // Key lpwstring,ie,suffix to remove 
			repstr = rhs.repstr; // lpwstring  to replace deleted letters 
			form = rhs.form;
			inflection = rhs.inflection;
			rulenum = rhs.rulenum; // Line number of rule in rule list file 
			intact = rhs.intact; // Boolean-must word be intact? 
			cont = rhs.cont; // Boolean-continue with another rule? 
			protect = rhs.protect;  // Boolean-protect this ending? 
			trail = rhs.trail;
		}
	*/
	};

	typedef struct {
		lpwstring keystr; /* Key lpwstring,ie,suffix to remove */
		lpwstring repstr; /* lpwstring  to replace deleted letters */
		int rulenum; /* Line number of rule in rule list file */
	} tPrefixRule;

	static int findLastFormInflection(vector <cSuffixRule> rulesUsed, vector <cSuffixRule>::iterator &r, lpwstring &form, int &inflection);
	static size_t stem(MYSQL mysql, lpwstring s, vector<cSuffixRule>& rulesUsed, cIntArray& trail, int addRule);
	// Clears the process-wide stemRules/prefixRules vectors (not usually needed -
	// they are static and live for the process).
	~cStemmer();
	static bool isWordDBUnknown(MYSQL mysql, lpwstring word);
	static bool wordIsNotUnknownAndOpen(tIWMM iWord, bool log);

private:
	static unordered_set<int> unacceptableCombinationForms;
	static vector <cSuffixRule> stemRules;
	static vector <tPrefixRule> prefixRules;
	static int applyStemRule(lpwstring sWord, cSuffixRule rule, vector <cSuffixRule> &rulesUsed, cIntArray trail);
	static int applyPrefixRule(MYSQL mysql, tPrefixRule r, vector <cSuffixRule> &rulesUsed, int originalSize, lpwstring sWord);
	static int readStemRules(void);
	static int readPrefixRules(void);
	static int getInflectionNum(lpchar_t const *inflection);
	static int stripPrefix(MYSQL mysql, lpwstring s, vector <cSuffixRule> &rulesUsed);
};