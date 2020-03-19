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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include "word.h"
#include "profile.h"
#include "paice.h"
#include "mysqldb.h"
#define MINSTEMSIZE 3
#include <sstream>

#define maxlinelength 1024 /* Maximum length of line read from file */
enum states { s_notapply,s_stop,s_continue };
vector <Stemmer::tSuffixRule> Stemmer::stemRules;
vector <Stemmer::tPrefixRule> Stemmer::prefixRules;
unordered_set<int> Stemmer::unacceptableCombinationForms;

/* * * APPLYRULE()  * * * * * * * */
int Stemmer::applyStemRule(wstring word,tSuffixRule rule,vector <tSuffixRule> &rulesUsed,intArray trail)
{ LFS
  /* Apply the rule r to word,leaving results in word.Return stop,continue */
  /* or notapply as appropriate */

  if(rulesUsed.size() && rule.intact) /* If it should be the first rule applied but isn't... */
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
  if ((stemlen < 0) || (stemlen + rule.repstr.length() < MINSTEMSIZE) || rule.keystr!=word.substr(stemlen,word.length()-stemlen)) /* AZ change */
    return s_notapply;

  /* If ending matches key string.. */
  if (rule.protect) return s_stop; /* If it is protected,then stop */

  /* Before replacing, exclude terminal state markers from the
    replacement string length count to see if stem will be valid.
    It is more efficient to check here than before the match. */
  int rl=rule.repstr.length();
  if (rl >= 2 && iswdigit(rule.repstr[rl-1]) && iswdigit(rule.repstr[rl-2]))  rl-=2;
  if (stemlen + rl < MINSTEMSIZE ) return s_notapply;

  /* Replace matching keystr with repstr. */
  rule.text=word.substr(0,stemlen)+rule.repstr;
  rule.trail=trail;
  rulesUsed.push_back(rule);
  #ifdef LOG_DICTIONARY
  lplog(L"rule #%d applied to %s resulting in %s trail=%s.",rule.rulenum,word.c_str(),rule.text.c_str(),trail.concatToString().c_str());
  #endif
  return (rule.cont) ? s_continue:s_stop;/* If continue flag is set,return cont */
}

int Stemmer::getInflectionNum(wchar_t const *inflection)
{ LFS
  if (!inflection[0]) return 0;
  int temp=0;
  const wchar_t *ch;
  tInflectionMap *inflectionMaps[]={nounInflectionMap,verbInflectionMap,adjectiveInflectionMap,adverbInflectionMap};
  for (int map=0; map<4; map++)
    for (int I=0; inflectionMaps[map][I].num>=0; I++)
    {
      wchar_t *name=inflectionMaps[map][I].name;
      if ((ch=wcsstr(inflection,name)) && (*(ch+wcslen(name))==L' ' || !*(ch+wcslen(name))))
        temp|=inflectionMaps[map][I].num;
    }
  return temp;
}

int Stemmer::readStemRules(void)
{ LFS
  /* Format is: keystr,repstr,flags where keystr and repstr are strings,and */
  /* flags are:"protect","intact","continue" (without the inverted commas in the actual file).  */

  FILE *fp=_wfopen(L"source\\lists\\suffixRules.txt",L"rb");
  if (!fp) 
	{
		lplog(LOG_FATAL_ERROR,L"Suffix file not found.");
		return NO_SUFFIX_RULES_FILE;
	}
  wchar_t s[maxlinelength];
  tSuffixRule temp;
  int line;
  /* Read a line at a time until eof */
  for (line=1; fgetws(s,maxlinelength,fp); line++)
  {
		if (s[0]==0xFEFF) // detect BOM
			memcpy(s,s+1,wcslen(s+1));
    if ((s[0] == L';') || (s[0] == L'\r') || (s[0] == L'\n') || (s[0] == L' '))
      continue;
	  wchar_t savecopy[maxlinelength];
		wcscpy(savecopy,s);
    wchar_t *ch=wcschr(s,L','),*savech;
    if (!ch) 
		{
		  fclose(fp);
			lplog(LOG_FATAL_ERROR,L"Error parsing (0) suffix rule on line %d: %s",line,savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
    *ch=0;
    temp.keystr = s; /* Copy key string into the rule struct */
    if (!(ch=wcschr(savech=ch+1,L','))) 
		{
		  fclose(fp);
			lplog(LOG_FATAL_ERROR,L"Error parsing (1) suffix rule on line %d: %s",line,savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
    *ch=0;
    temp.repstr = savech;
    if (!(ch=wcschr(savech=ch+1,L','))) 
		{
		  fclose(fp);
			lplog(LOG_FATAL_ERROR,L"Error parsing (2) suffix rule on line %d: %s",line,savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
    *ch=0;
    temp.form = savech;
    if (!(ch=wcschr(savech=ch+1,L','))) 
		{
		  fclose(fp);
			lplog(LOG_FATAL_ERROR,L"Error parsing (3) suffix rule on line %d: %s",line,savecopy);
			return SUFFIX_RULES_PARSE_ERROR;
		}
    *ch=0;
    temp.inflection = getInflectionNum(savech);
    if ((ch=wcschr(savech=ch+1,L';'))) *ch=0;
    temp.protect = wcsstr(savech,L"protect")!=NULL;
    temp.intact = wcsstr(savech,L"intact")!=NULL;
    temp.cont = wcsstr(savech,L"continue")!=NULL;
    temp.rulenum = line; /* Line number of rule in file */
    /* Check replacement string for special 2-digit markers  */
    int rl = temp.repstr.length();
    if (rl > 1 && iswdigit(temp.repstr[rl-1]) && iswdigit(temp.repstr[rl-2]) && temp.cont == false)
      lplog(L"** WARNING ** ReadRules: State marker may require continue:line %d\n",line);
    stemRules.push_back(temp);
  }
  fclose(fp);
  return 0;
}

int Stemmer::readPrefixRules(void)
{ LFS
  /* Format is: keystr,repstr where keystr and repstr are strings */

  FILE *fp=_wfopen(L"source\\lists\\prefixRules.txt",L"rb");
  if (!fp) return NO_PREFIX_RULES_FILE;
  wchar_t s[maxlinelength];
  tPrefixRule temp;
  int line;
  /* Read a line at a time until eof */
  for (line=1; fgetws(s,maxlinelength,fp); line++)
  {
		if (s[0]==0xFEFF) // detect BOM
			memcpy(s,s+1,wcslen(s+1));
    if ((s[0] == ';') || (s[0] == '\r') || (s[0] == '\n') || (s[0] == ' '))
      continue;
    wchar_t *ch=wcschr(s,L',');
    if (!ch) 
		{
		  fclose(fp);
			return PREFIX_RULES_PARSE_ERROR;
		}
    *ch=0;
    temp.keystr = s; /* Copy key string into the rule struct */
    temp.repstr = ch+1;
    temp.rulenum = line;
    prefixRules.push_back(temp);
  }
  fclose(fp);
  return 0;
}

bool sortRuleGreater(Stemmer::tSuffixRule a, Stemmer::tSuffixRule b)
{ LFS
  if (a.trail.count==b.trail.count)
    return a.text.length() > b.text.length();
  return a.trail.count < b.trail.count;
}

int Stemmer::stem(MYSQL mysql, wstring word, vector <tSuffixRule> &rulesUsed,intArray &trail,int addRule)
{ LFS
  int state=s_continue;
  int ret;
  if (!stemRules.size() && (ret=readStemRules())<0) return ret;
  if (addRule>=0) trail.push_back(addRule);
  for (unsigned int r=0; r<stemRules.size(); r++)
    if (state=applyStemRule(word,stemRules[r],rulesUsed,trail)==s_continue)
      stem(mysql, rulesUsed[rulesUsed.size()-1].text,rulesUsed,trail,stemRules[r].rulenum);
  if (addRule>=0) return rulesUsed.size();
  if (ret=stripPrefix(mysql, word, rulesUsed)) 
		return ret;
  sort(rulesUsed.begin(),rulesUsed.end(),sortRuleGreater);
  return rulesUsed.size();
}

bool Stemmer::isWordDBUnknown(MYSQL mysql,wstring word)
{
	tIWMM iWord = Words.query(word);
	if (iWord != Words.end() && iWord->second.query(UNDEFINED_FORM_NUM) >= 0)
		return true;
	if (!myquery(&mysql, L"LOCK TABLES words w READ,wordForms wf READ")) return true;
	wchar_t qt[QUERY_BUFFER_LEN_OVERFLOW];
	_snwprintf(qt, QUERY_BUFFER_LEN, L"select COUNT(*) from words w,wordForms wf where w.id=wf.wordId and word=\"%s\" and wf.formId=%d", word.c_str(), UNDEFINED_FORM_NUM+1); // always add one when referring to DB formId
	MYSQL_RES *result = NULL;
	MYSQL_ROW sqlrow;
	if (!myquery(&mysql, qt, result))
	{
		myquery(&mysql, L"UNLOCK TABLES");
		return true;
	}
	int count=1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		count = atoi(sqlrow[0]);
	mysql_free_result(result);
	if (!myquery(&mysql, L"UNLOCK TABLES"))
		return true;
	return count > 0;
}

int Stemmer::applyPrefixRule(MYSQL mysql, tPrefixRule r,vector <tSuffixRule> &rulesUsed,int originalSize,wstring word)
{ LFS
  // word must have sufficient length over the prefix as well as matching it over the prefix length.
  if (word.length()<=r.keystr.length()+2 || r.keystr!=word.substr(0,r.keystr.length())) return 0;
  tSuffixRule tmp;
  tmp.text=word.substr(r.keystr.length(),word.length()-r.keystr.length());
	// does this word exist and is known?
	if (isWordDBUnknown(mysql,tmp.text))
		return 0;
  tmp.keystr=r.keystr;
  tmp.repstr=r.repstr;
  tmp.rulenum=-r.rulenum;
  tmp.form=L"PREVIOUS";
  rulesUsed.push_back(tmp);
  for (int ru=0; ru<originalSize; ru++)
  {
    if (rulesUsed[ru].text.length()<=r.keystr.length()+2) continue; // won't leave any word left!
    tmp.text=rulesUsed[ru].text.substr(r.keystr.length(),rulesUsed[ru].text.length()-r.keystr.length());
    tmp.keystr=r.keystr;
    tmp.repstr=r.repstr;
    tmp.rulenum=-r.rulenum;
    tmp.trail=rulesUsed[ru].trail;
    tmp.trail.add(rulesUsed[ru].rulenum);
    tmp.form=L"PREVIOUS";
    rulesUsed.push_back(tmp);
  }
  return 0;
}

// prefixes are not nestable
// apply all prefixes to all rulesUsed
int Stemmer::stripPrefix(MYSQL mysql, wstring word, vector <tSuffixRule> &rulesUsed)
{ LFS
  if (!prefixRules.size() && readPrefixRules()<0) return -1;
  int originalSize=rulesUsed.size();
  for (unsigned int r=0; r<prefixRules.size(); r++)
    applyPrefixRule(mysql,prefixRules[r],rulesUsed,originalSize,word);
  return 0;
}

int Stemmer::findLastFormInflection(vector <tSuffixRule> rulesUsed, vector <tSuffixRule>::iterator &r, wstring &form, int &inflection)
{
	LFS
		form = L"ORIGINAL";
	if (r->form != L"PREVIOUS")
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
		if (stemRules[sr].form != L"PREVIOUS")
		{
			form = stemRules[sr].form;
			inflection = stemRules[sr].inflection;
		}
	}
	return 0;
}

Stemmer::~Stemmer()
{
	LFS
	stemRules.clear();
	prefixRules.clear();
}

bool Stemmer::wordIsNotUnknownAndOpen(tIWMM iWord,bool log)
{
	if (unacceptableCombinationForms.empty())
	{
		unacceptableCombinationForms.insert(UNDEFINED_FORM_NUM);
		unacceptableCombinationForms.insert(commaForm);
		unacceptableCombinationForms.insert(periodForm);
		unacceptableCombinationForms.insert(reflexivePronounForm);
		unacceptableCombinationForms.insert(nomForm);
		unacceptableCombinationForms.insert(accForm);
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
			lplog(LOG_DICTIONARY, L"WordPosMAP %s is a %s.", iWord->first.c_str(), Forms[*ucf]->name.c_str());
			return false;
		}
	return (iWord->second.query(verbForm) >= 0) || (iWord->second.query(nounForm) >= 0) || (iWord->second.query(adverbForm) >= 0) || (iWord->second.query(adjectiveForm) >= 0);
}

