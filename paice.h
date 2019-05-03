#pragma warning (disable: 4503)
#pragma warning (disable: 4996)
#undef _STLP_USE_EXCEPTIONS // STLPORT 4.6.1
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
class Stemmer
{

public:
	class tSuffixRule
	{
	public:
		wstring text; /* To return stemmer output */
		wstring keystr; /* Key wstring,ie,suffix to remove */
		wstring repstr; /* wstring  to replace deleted letters */
		wstring form;
		int inflection;
		int rulenum; /* Line number of rule in rule list file */
		bool intact; /* Boolean-must word be intact? */
		bool cont; /* Boolean-continue with another rule? */
		bool protect; /* Boolean-protect this ending? */
		intArray trail;
		tSuffixRule()
		{
		}
		~tSuffixRule()
		{
		}
		tSuffixRule(const tSuffixRule &rhs)
		{
			text = rhs.text; /* To return stemmer output */
			keystr = rhs.keystr; /* Key wstring,ie,suffix to remove */
			repstr = rhs.repstr; /* wstring  to replace deleted letters */
			form = rhs.form;
			inflection = rhs.inflection;
			rulenum = rhs.rulenum; /* Line number of rule in rule list file */
			intact = rhs.intact; /* Boolean-must word be intact? */
			cont = rhs.cont; /* Boolean-continue with another rule? */
			protect = rhs.protect;  /* Boolean-protect this ending? */
			trail = rhs.trail;
		}
	};

	typedef struct {
		wstring keystr; /* Key wstring,ie,suffix to remove */
		wstring repstr; /* wstring  to replace deleted letters */
		int rulenum; /* Line number of rule in rule list file */
	} tPrefixRule;

	static int findLastFormInflection(vector <tSuffixRule> rulesUsed, vector <tSuffixRule>::iterator &r, wstring &form, int &inflection);
	static int stem(MYSQL mysql, wstring s, vector <tSuffixRule> &rulesUsed, intArray &trail, int addRule);
	~Stemmer();
	static boolean isWordDBUnknown(MYSQL mysql, wstring word);

private:
	static vector <tSuffixRule> stemRules;
	static vector <tPrefixRule> prefixRules;
	static int applyStemRule(wstring sWord, tSuffixRule rule, vector <tSuffixRule> &rulesUsed, intArray trail);
	static int applyPrefixRule(MYSQL mysql, tPrefixRule r, vector <tSuffixRule> &rulesUsed, int originalSize, wstring sWord);
	static int readStemRules(void);
	static int readPrefixRules(void);
	static int getInflectionNum(wchar_t const *inflection);
	static int stripPrefix(MYSQL mysql, wstring s, vector <tSuffixRule> &rulesUsed);

};