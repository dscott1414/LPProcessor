/*
	paice.cpp - Paice/Husk suffix/prefix stemmer (cStemmer) implementation

	Overview:
		Loads suffixRules.txt / prefixRules.txt on first use, then stem() tries
		every suffix rule against the surface form.  A 'continue' rule recurses
		on the replacement.  After suffixes, stripPrefix() tries each prefix
		against the original word and against every suffix-produced candidate,
		keeping a prefix strip only when the remainder is not UNDEFINED in the
		lexicon/DB.  Results are sorted (fewer trail steps, then longer text).

	Pipeline position:
		cWord::attemptDisInclination / parseWord, during lexicon lookup.

	Key entry points:
		- applyStemRule() - one rule vs one word; returns s_notapply/s_stop/s_continue.
		- stem() - drive the search; returns rulesUsed.size() or a NET_ERR code.
		- stripPrefix() / applyPrefixRule() - non-nestable prefix pass.
		- readStemRules() / readPrefixRules() - parse the lists files.
		- findLastFormInflection() - recover form/inflection from a result's trail.
		- isWordDBUnknown() / wordIsNotUnknownAndOpen() - filters.

	Dependencies:
		source\\lists\\suffixRules.txt (key,rep,form,inflection,flags)
		source\\lists\\prefixRules.txt (key,rep)
		words / wordForms tables (isWordDBUnknown).

	Notes / gotchas:
		- stem()'s loop tests `applyStemRule(...) == s_continue` directly now; it used
			to read `if (state = applyStemRule(...) == s_continue)`, an `=`/`==`
			precedence trap that stored a 0/1 bool into an otherwise-unread `state`
			local.  The `if` always evaluated correctly either way (removing the dead
			assignment does not change behaviour) - only the dead store is gone.
		- BOM stripping uses wmemmove(s, s+1, lp_strlen(s+1)+1) in both readStemRules
			and readPrefixRules (readPrefixRules used to use an overlapping
			byte-count memcpy that corrupted the rest of the line; fixed to match
			readStemRules).
		- isWordDBUnknown escapes `word` (escaped()/escapeStr()) before
			interpolating it into the double-quoted literal.
		- LOG_DICTIONARY in applyStemRule calls trail.concatToString() with no
			argument; that path does not compile if the #define is on.
		- findLastFormInflection copies rulesUsed by value.
*/
/* Paice/Husk Stemmer Program 1994-5,by Andrew Stark   Started 12-9-94. */
/* 7/30/2003 - Antonio Zamora:
 - allowed comment lines starting with semicolon in rules,
 - improved diagnostic handling for rules,
 - replaced acceptable() subroutine with fixed-length MINSTEMSIZE,
 - introduced trailing 2-digit strings as state markers in rules,
 - added rule display and debugging option,
 - modified applyrule() by prescreening rules to return "not applicable"
	 for matches that would create unacceptably short stems,
 - added return code to readrules().
*/
/* 8/10/2003 - AZ - modified stem() to return the rule trace */
/* 10/10/2004 - DS - modified program to use more C++ */

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
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include "word.h"
#include "source.h" // batch B5: escaped(), previously reached via another header
#include "profile.h"
#include "paice.h"
#include "mysqldb.h"
#define MINSTEMSIZE 3
#include <sstream>

#define maxlinelength 1024 /* Maximum length of line read from file */
enum states { s_notapply, s_stop, s_continue };
vector <cStemmer::cSuffixRule> cStemmer::stemRules;
vector <cStemmer::tPrefixRule> cStemmer::prefixRules;
unordered_set<int> cStemmer::unacceptableCombinationForms;

// Try one suffix rule against 'word'.  Returns s_notapply if the suffix
// does not match, the word is not intact when required, or the result would
// be shorter than MINSTEMSIZE (3); s_stop if protect or the rule does not
// continue; s_continue if the caller should recurse on rule.text.
// On apply, pushes a copy of 'rule' (with text/trail filled) onto rulesUsed.
// 'rule' and 'trail' are taken by value.
/* * * APPLYRULE()  * * * * * * * */
int cStemmer::applyStemRule(lpwstring word, cSuffixRule rule, vector <cSuffixRule>& rulesUsed, cIntArray trail)
{
	LFS
		/* Apply the rule r to word,leaving results in word.Return stop,continue */
		/* or notapply as appropriate */

		if (rulesUsed.size() && rule.intact) /* If it should be the first rule applied but isn't... */
			return s_notapply; /* ..then rule fails */

	int stemlen = word.length() - rule.keystr.length(); /* Find where suffix  should start */

	/* AZ - the following change speeds up the process:
	1) Avoid matching rules for which the suffix is longer than the word.
	2) Avoid applying rules that would result in unacceptably short stems.
		The second condition  makes it possible to apply rules of
		smaller length that are substrings of another rule.
		E.g, given MINSTEMSIZE = 3 and the rules:
			tions,?,stop
			ions,?,stop
			s,?,stop
		For the word "actions"  'tions' would have matched and generated
			the 2 letter stem "ac".  However, since this is smaller than
			MINSTEMSIZE, the rule is considered not applicable and the next
			rule, 'ions' will apply and generate "act")
		For "lions", 'ions' won't apply, but 's' applies to generate 'lion'
	*/
	if ((stemlen < 0) || (stemlen + rule.repstr.length() < MINSTEMSIZE) || rule.keystr != word.substr(stemlen, word.length() - stemlen)) /* AZ change */
		return s_notapply;

	/* If ending matches key string.. */
	if (rule.protect) return s_stop; /* If it is protected,then stop */

	/* Before replacing, exclude terminal state markers from the
		replacement string length count to see if stem will be valid.
		It is more efficient to check here than before the match. */
	int rl = rule.repstr.length();
	if (rl >= 2 && iswdigit(rule.repstr[rl - 1]) && iswdigit(rule.repstr[rl - 2]))  rl -= 2;
	if (stemlen + rl < MINSTEMSIZE) return s_notapply;

	/* Replace matching keystr with repstr. */
	rule.text = word.substr(0, stemlen) + rule.repstr;
	rule.trail = trail;
	rulesUsed.push_back(rule);
#ifdef LOG_DICTIONARY
	lplog(u"rule #%d applied to %s resulting in %s trail=%s.", rule.rulenum, word.c_str(), rule.text.c_str(), trail.concatToString().c_str());
#endif
	return (rule.cont) ? s_continue : s_stop;/* If continue flag is set,return cont */
}

// OR together every InflectionTypes bit whose name occurs as a whole word
// in 'inflection' (space- or NUL-terminated), searching the four
// noun/verb/adjective/adverb maps.  Returns 0 if inflection is empty.
int cStemmer::getInflectionNum(lpchar_t const* inflection)
{
	LFS
		if (!inflection[0]) return 0;
	int temp = 0;
	const lpchar_t* ch;
	tInflectionMap* inflectionMaps[] = { nounInflectionMap,verbInflectionMap,adjectiveInflectionMap,adverbInflectionMap };
	for (int map = 0; map < 4; map++)
		for (int I = 0; inflectionMaps[map][I].num >= 0; I++)
		{
			const lpchar_t* name = inflectionMaps[map][I].name;
			if ((ch = lp_strstr(inflection, name)) && (*(ch + lp_strlen(name)) == u' ' || !*(ch + lp_strlen(name))))
				temp |= inflectionMaps[map][I].num;
		}
	return temp;
}

// Parse source\\lists\\suffixRules.txt into stemRules.  Line format:
// keystr,repstr,form,inflection,flags  with optional ;comment.  BOM (U+FEFF)
// is stripped with wmemmove (see file header).  Returns 0, or
// NO_SUFFIX_RULES_FILE / SUFFIX_RULES_PARSE_ERROR (both FATAL first).
int cStemmer::readStemRules(void)
{
	LFS
		/* Format is: keystr,repstr,flags where keystr and repstr are strings,and */
		/* flags are:"protect","intact","continue" (without the inverted commas in the actual file).  */

		FILE* fp = lp_wfopen(u"source\\lists\\suffixRules.txt", "rb");
	if (!fp)
	{
		lplog(LOG_FATAL_ERROR, u"Suffix file not found.");
		return NO_SUFFIX_RULES_FILE;
	}
	lpchar_t s[maxlinelength];
	cSuffixRule temp;
	int line;
	/* Read a line at a time until eof */
	for (line = 1; lp_fgetws(s, maxlinelength, fp); line++)
	{
		if (s[0] == 0xFEFF) // detect BOM
			// Batch B2: wmemmove has no char16_t equivalent; std::memmove works directly
			// once scaled by sizeof(lpchar_t) (it's already alignment/overlap-safe raw
			// memory move, exactly what wmemmove itself is specified in terms of).
			std::memmove(s, s + 1, (lp_strlen(s + 1) + 1) * sizeof(lpchar_t));
		if ((s[0] == u';') || (s[0] == u'\r') || (s[0] == u'\n') || (s[0] == u' '))
			continue;
		lpchar_t savecopy[maxlinelength];
		lp_strcpy(savecopy, s);
		lpchar_t* ch = lp_strchr(s, u','), * savech;
		if (!ch)
		{
			fclose(fp);
			lplog(LOG_FATAL_ERROR, u"Error parsing (0) suffix rule on line %d: %s", line, savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
		*ch = 0;
		temp.keystr = s; /* Copy key string into the rule struct */
		if (!(ch = lp_strchr(savech = ch + 1, u',')))
		{
			fclose(fp);
			lplog(LOG_FATAL_ERROR, u"Error parsing (1) suffix rule on line %d: %s", line, savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
		*ch = 0;
		temp.repstr = savech;
		if (!(ch = lp_strchr(savech = ch + 1, u',')))
		{
			fclose(fp);
			lplog(LOG_FATAL_ERROR, u"Error parsing (2) suffix rule on line %d: %s", line, savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
		*ch = 0;
		temp.form = savech;
		if (!(ch = lp_strchr(savech = ch + 1, u',')))
		{
			fclose(fp);
			lplog(LOG_FATAL_ERROR, u"Error parsing (3) suffix rule on line %d: %s", line, savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
		*ch = 0;
		temp.inflection = getInflectionNum(savech);
		if ((ch = lp_strchr(savech = ch + 1, u';'))) *ch = 0;
		temp.protect = lp_strstr(savech, u"protect") != NULL;
		temp.intact = lp_strstr(savech, u"intact") != NULL;
		temp.cont = lp_strstr(savech, u"continue") != NULL;
		temp.rulenum = line; /* Line number of rule in file */
		/* Check replacement string for special 2-digit markers  */
		int rl = temp.repstr.length();
		if (rl > 1 && iswdigit(temp.repstr[rl - 1]) && iswdigit(temp.repstr[rl - 2]) && temp.cont == false)
			lplog(u"** WARNING ** ReadRules: State marker may require continue:line %d\n", line);
		stemRules.push_back(temp);
	}
	fclose(fp);
	return 0;
}

// Parse source\\lists\\prefixRules.txt (keystr,repstr) into prefixRules.
// Missing file is a silent NO_PREFIX_RULES_FILE (not FATAL, unlike suffixes).
int cStemmer::readPrefixRules(void)
{
	LFS
		/* Format is: keystr,repstr where keystr and repstr are strings */

		FILE* fp = lp_wfopen(u"source\\lists\\prefixRules.txt", "rb");
	if (!fp) return NO_PREFIX_RULES_FILE;
	lpchar_t s[maxlinelength];
	tPrefixRule temp;
	int line;
	/* Read a line at a time until eof */
	for (line = 1; lp_fgetws(s, maxlinelength, fp); line++)
	{
		if (s[0] == 0xFEFF) // detect BOM
			// Batch B2: wmemmove has no char16_t equivalent; std::memmove works directly
			// once scaled by sizeof(lpchar_t) (it's already alignment/overlap-safe raw
			// memory move, exactly what wmemmove itself is specified in terms of).
			std::memmove(s, s + 1, (lp_strlen(s + 1) + 1) * sizeof(lpchar_t));
		if ((s[0] == ';') || (s[0] == '\r') || (s[0] == '\n') || (s[0] == ' '))
			continue;
		lpchar_t* ch = lp_strchr(s, u',');
		if (!ch)
		{
			fclose(fp);
			return PREFIX_RULES_PARSE_ERROR;
		}
		*ch = 0;
		temp.keystr = s; /* Copy key string into the rule struct */
		temp.repstr = ch + 1;
		temp.rulenum = line;
		prefixRules.push_back(temp);
	}
	fclose(fp);
	return 0;
}

// Sort key for stem() results: fewer trail steps first, then longer text.
// Taken by value (two full cSuffixRule copies per comparison).
bool sortRuleGreater(cStemmer::cSuffixRule a, cStemmer::cSuffixRule b)
{
	LFS
		if (a.trail.count == b.trail.count)
			return a.text.length() > b.text.length();
	return a.trail.count < b.trail.count;
}

// Load suffix rules if needed, try every rule, recurse on s_continue.
// addRule>=0: this is a recursive frame - push addRule onto trail and
// return rulesUsed.size() without prefix-stripping or sorting.
// addRule<0 (the public call): then stripPrefix, sort, return size.
// Returns a negative NET_ERR if the rules file is missing/corrupt.
size_t cStemmer::stem(MYSQL mysql, lpwstring word, vector<cSuffixRule>& rulesUsed, cIntArray& trail, int addRule)
{
	LFS
		int ret;
	if (!stemRules.size() && (ret = readStemRules()) < 0) return ret;
	if (addRule >= 0) trail.push_back(addRule);
	for (unsigned int r = 0; r < stemRules.size(); r++)
		if (applyStemRule(word, stemRules[r], rulesUsed, trail) == s_continue)
			stem(mysql, rulesUsed[rulesUsed.size() - 1].text, rulesUsed, trail, stemRules[r].rulenum);
	if (addRule >= 0) return rulesUsed.size();
	if (ret = stripPrefix(mysql, word, rulesUsed))
		return ret;
	sort(rulesUsed.begin(), rulesUsed.end(), sortRuleGreater);
	return rulesUsed.size();
}

// True if 'word' is already in Words with UNDEFINED_FORM, or if the DB has
// a words/wordForms row for it with formId UNDEFINED_FORM_NUM+1 (DB is
// 1-based).  Also returns true on LOCK/query failure (conservative: treat
// as unknown so the prefix is rejected).  word is escaped before being
// interpolated into the double-quoted literal.
bool cStemmer::isWordDBUnknown(MYSQL mysql, lpwstring word)
{
	tIWMM iWord = Words.query(word);
	if (iWord != Words.end() && iWord->second.query(UNDEFINED_FORM_NUM) >= 0)
		return true;
	if (!myquery(&mysql, u"LOCK TABLES words w READ,wordForms wf READ")) return true;
	lpchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	lp_snprintf(qt, QUERY_BUFFER_LEN, u"select COUNT(*) from words w,wordForms wf where w.id=wf.wordId and word=\"%s\" and wf.formId=%d", escaped(word).c_str(), UNDEFINED_FORM_NUM + 1); // always add one when referring to DB formId
	MYSQL_RES* result = NULL;
	MYSQL_ROW sqlrow;
	if (!myquery(&mysql, qt, result))
	{
		myquery(&mysql, u"UNLOCK TABLES");
		return true;
	}
	int count = 1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		count = atoi(sqlrow[0]);
	mysql_free_result(result);
	if (!myquery(&mysql, u"UNLOCK TABLES"))
		return true;
	return count > 0;
}

// If 'word' starts with r.keystr and the remainder is a known open-class
// word, push a prefix-stripped cSuffixRule (rulenum = -r.rulenum, form
// PREVIOUS) and also strip the same prefix from rulesUsed[0..originalSize).
// Prefixes are not nested.  Always returns 0.
int cStemmer::applyPrefixRule(MYSQL mysql, tPrefixRule r, vector <cSuffixRule>& rulesUsed, int originalSize, lpwstring word)
{
	LFS
		// word must have sufficient length over the prefix as well as matching it over the prefix length.
		if (word.length() <= r.keystr.length() + 2 || r.keystr != word.substr(0, r.keystr.length())) return 0;
	cSuffixRule tmp;
	tmp.text = word.substr(r.keystr.length(), word.length() - r.keystr.length());
	// does this word exist and is known?
	if (isWordDBUnknown(mysql, tmp.text))
		return 0;
	tmp.keystr = r.keystr;
	tmp.repstr = r.repstr;
	tmp.rulenum = -r.rulenum;
	tmp.form = u"PREVIOUS";
	rulesUsed.push_back(tmp);
	for (int ru = 0; ru < originalSize; ru++)
	{
		if (rulesUsed[ru].text.length() <= r.keystr.length() + 2) continue; // won't leave any word left!
		tmp.text = rulesUsed[ru].text.substr(r.keystr.length(), rulesUsed[ru].text.length() - r.keystr.length());
		tmp.keystr = r.keystr;
		tmp.repstr = r.repstr;
		tmp.rulenum = -r.rulenum;
		tmp.trail = rulesUsed[ru].trail;
		tmp.trail.add(rulesUsed[ru].rulenum);
		tmp.form = u"PREVIOUS";
		rulesUsed.push_back(tmp);
	}
	return 0;
}

// Load prefix rules if needed, then applyPrefixRule for every prefix against
// 'word' and the current rulesUsed snapshot.  Returns -1 if the prefix file
// is missing; 0 otherwise.
// prefixes are not nestable
// apply all prefixes to all rulesUsed
int cStemmer::stripPrefix(MYSQL mysql, lpwstring word, vector <cSuffixRule>& rulesUsed)
{
	LFS
		if (!prefixRules.size() && readPrefixRules() < 0) return -1;
	size_t originalSize = rulesUsed.size();
	for (unsigned int r = 0; r < prefixRules.size(); r++)
		applyPrefixRule(mysql, prefixRules[r], rulesUsed, originalSize, word);
	return 0;
}

// Walk r->trail from the end, skipping negative (prefix) ids, and copy the
// last non-PREVIOUS stemRules[].form/inflection into the out-params.  If r
// itself is not PREVIOUS, start from r.  rulesUsed is unused (passed by
// value).  Always returns 0.
int cStemmer::findLastFormInflection(vector <cSuffixRule> rulesUsed, vector <cSuffixRule>::iterator& r, lpwstring& form, int& inflection)
{
	LFS
		form = u"ORIGINAL";
	if (r->form != u"PREVIOUS")
	{
		form = r->form;
		inflection = r->inflection;
	}
	for (int t = r->trail.size() - 1; t >= 0; t--)
	{
		if (r->trail[t] < 0) continue;
		unsigned int sr = 0;
		for (sr = 0; sr < stemRules.size(); sr++)
			if (stemRules[sr].rulenum == r->trail[t])
				break;
		if (stemRules[sr].form != u"PREVIOUS")
		{
			form = stemRules[sr].form;
			inflection = stemRules[sr].inflection;
		}
	}
	return 0;
}

// Clear the process-wide rule vectors.  Instantiating a cStemmer just to
// destroy it is the only way to unload the rules.
cStemmer::~cStemmer()
{
	LFS
		stemRules.clear();
	prefixRules.clear();
}

// False if iWord carries any closed-class form (pronoun, determiner, numeral,
// modal, punctuation, ... - the set is filled once).  True only if it also
// has verb, noun, adverb, or adjective.  log writes the rejecting form.
bool cStemmer::wordIsNotUnknownAndOpen(tIWMM iWord, bool log)
{
	if (unacceptableCombinationForms.empty())
	{
		unacceptableCombinationForms.insert(UNDEFINED_FORM_NUM);
		unacceptableCombinationForms.insert(commaForm);
		unacceptableCombinationForms.insert(periodForm);
		unacceptableCombinationForms.insert(reflexivePronounForm);
		unacceptableCombinationForms.insert(nomForm);
		unacceptableCombinationForms.insert(personalPronounAccusativeForm);
		unacceptableCombinationForms.insert(quoteForm);
		unacceptableCombinationForms.insert(dashForm);
		unacceptableCombinationForms.insert(bracketForm);
		unacceptableCombinationForms.insert(conjunctionForm);
		unacceptableCombinationForms.insert(demonstrativeDeterminerForm);
		unacceptableCombinationForms.insert(possessiveDeterminerForm);
		unacceptableCombinationForms.insert(interrogativeDeterminerForm);
		unacceptableCombinationForms.insert(indefinitePronounForm);
		unacceptableCombinationForms.insert(reciprocalPronounForm);
		unacceptableCombinationForms.insert(pronounForm);
		unacceptableCombinationForms.insert(numeralCardinalForm);
		unacceptableCombinationForms.insert(numeralOrdinalForm);
		unacceptableCombinationForms.insert(romanNumeralForm);
		unacceptableCombinationForms.insert(honorificForm);
		unacceptableCombinationForms.insert(honorificAbbreviationForm);
		unacceptableCombinationForms.insert(relativeForm);
		unacceptableCombinationForms.insert(determinerForm);
		unacceptableCombinationForms.insert(doesForm);
		unacceptableCombinationForms.insert(doesNegationForm);
		unacceptableCombinationForms.insert(possessivePronounForm);
		unacceptableCombinationForms.insert(quantifierForm);
		unacceptableCombinationForms.insert(dateForm);
		unacceptableCombinationForms.insert(timeForm);
		unacceptableCombinationForms.insert(telephoneNumberForm);
		unacceptableCombinationForms.insert(coordinatorForm);
		unacceptableCombinationForms.insert(abbreviationForm);
		unacceptableCombinationForms.insert(numberForm);
		unacceptableCombinationForms.insert(beForm);
		unacceptableCombinationForms.insert(haveForm);
		unacceptableCombinationForms.insert(haveNegationForm);
		unacceptableCombinationForms.insert(doForm);
		unacceptableCombinationForms.insert(doNegationForm);
		unacceptableCombinationForms.insert(interjectionForm);
		unacceptableCombinationForms.insert(personalPronounForm);
		unacceptableCombinationForms.insert(letterForm);
		unacceptableCombinationForms.insert(isForm);
		unacceptableCombinationForms.insert(isNegationForm);
		unacceptableCombinationForms.insert(prepositionForm);
		unacceptableCombinationForms.insert(telenumForm);
		unacceptableCombinationForms.insert(sa_abbForm);
		unacceptableCombinationForms.insert(toForm);
		unacceptableCombinationForms.insert(relativizerForm);
		unacceptableCombinationForms.insert(moneyForm);
		unacceptableCombinationForms.insert(particleForm);
		unacceptableCombinationForms.insert(webAddressForm);
		unacceptableCombinationForms.insert(doForm);
		unacceptableCombinationForms.insert(doNegationForm);
		unacceptableCombinationForms.insert(monthForm);
		unacceptableCombinationForms.insert(letterForm);
		unacceptableCombinationForms.insert(modalAuxiliaryForm);
		unacceptableCombinationForms.insert(futureModalAuxiliaryForm);
		unacceptableCombinationForms.insert(negationModalAuxiliaryForm);
		unacceptableCombinationForms.insert(negationFutureModalAuxiliaryForm);
	}
	unordered_set<int>::iterator ucf;
	for (unsigned int f = 0; f < iWord->second.formsSize(); f++)
		if ((ucf = unacceptableCombinationForms.find(iWord->second.forms()[f])) != unacceptableCombinationForms.end())
		{
			if (log)
				lplog(LOG_DICTIONARY, u"WordPosMAP %s is a %s.", iWord->first.c_str(), Forms[*ucf]->name.c_str());
			return false;
		}
	return (iWord->second.query(verbForm) >= 0) || (iWord->second.query(nounForm) >= 0) || (iWord->second.query(adverbForm) >= 0) || (iWord->second.query(adjectiveForm) >= 0);
}

