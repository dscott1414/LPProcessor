#pragma warning(disable : 4786 ) // disable warning C4786
#include <windows.h>
#include <io.h>
#include "word.h"
#include "source.h"
#include <stdlib.h>
#include "wn.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "vcXML.h"
#include "profile.h"
#include "wininet.h"
#include "internet.h"

bool myquery(MYSQL *mysql, wchar_t *q, MYSQL_RES * &result, bool allowFailure = false);
bool myquery(MYSQL *mysql, wchar_t *q, bool allowFailure = false);
bool wordNetInitialized = false; // initialized
unordered_map <wstring,wstring> mostCommonSynonymMap; // initialized
unordered_map <wstring,wstring> synonymMap; // synonyms that are left out of WordNet // initialized
unordered_map <wstring,wstring> synonymDeletionMap; // initialized

set <int> offsets; // initialized
extern unordered_map <wstring,int> levinVerbToClassSectionMap; // dictionary initialized 
extern unordered_map <int,wstring> levinClassSectionNames; // dictionary initialized 
extern unordered_map <wstring,set <int> > vbNetVerbToClassMap;
int numVbNetClassFound=0,numOneSenseVbNetClassFound=0,numMultiSenseVbNetClassFound=0,numVbNetClassMultiSenseNotFound=0,verbsMappedToVerbNet=0;

bool cacheOrderedHyperNyms=true; // initialized
unordered_map<wstring, vector < vector <string> > > orderedHyperNymsMap; // protected with orderedHyperNymsMapSRWLock
unordered_map<wstring, int > orderedHyperNymsNumMap; // protected with orderedHyperNymsMapSRWLock
// used for agentiveNominalizations 
unordered_map <wstring,set < wstring > > nounVerbMap; // initialized
void printEntry(sDefinition d);

int extractWordsFromSynset(char *word,SynsetPtr synset_ptr,int recur,vector <set <wstring> > &words,bool ignoreTopLevel,sTrace &t)
{ LFS
	if (!synset_ptr) return 0;
	/*
    for (int I=0; I<synset_ptr -> ptrcount; I++)
		{
			wchar_t *synTypes[]={ "","ANTPTR","HYPERPTR","HYPOPTR","ENTAILPTR","SIMPTR","ISMEMBERPTR","ISSTUFFPTR","ISPARTPTR","HASMEMBERPTR","HASSTUFFPTR","HASPARTPTR",
				   "MERONYM","HOLONYM","CAUSETO","PPLPTR","SEEALSOPTR","PERTPTR","ATTRIBUTE","VERBGROUP","DERIVATION","CLASSIFICATION","CLASS",
					 "SYNS","FREQ","FRAMES","COORDS","RELATIVES","HMERONYM","HHOLONYM","WNGREP","OVERVIEW" };
			wprintf(L"%d:%s\n",I,synTypes[synset_ptr->ptrtyp[I]]);
		}
	  wprintf(L"%s\n",FmtSynset(synset_ptr, 1));
	*/
	int numWords=0;
	if (recur || !ignoreTopLevel)
	{
		set <wstring> sense;
		//if (recur && synset_ptr -> wcount && t.traceSpeakerResolution) lplogNR(LOG_WORDNET,L"  =>");
		for (int I = 0; I < synset_ptr -> wcount; I++)
		{
			if (!strcmp(word,synset_ptr -> words[I])) continue;
			numWords++;
			char synword[WORDBUF];
			strcpy(synword,synset_ptr -> words[I]);
			strsubst(synword, '_', ' ');
			strlwr(synword);
			wstring w;
			sense.insert(mTW(synword,w));
			//if (t.traceSpeakerResolution)
				//lplogNR(LOG_WORDNET,L"%S%c",synset_ptr -> words[I],(I==synset_ptr -> wcount-1)? L' ':L',');
		}
		words.push_back(sense);
		if (!recur && synset_ptr -> wcount && t.traceSpeakerResolution) lplog(LOG_WORDNET,L"");
	}
	int tmpNumWords;
	if ((tmpNumWords=extractWordsFromSynset(word,synset_ptr->ptrlist,recur+1,words,ignoreTopLevel,t)) && !recur && t.traceSpeakerResolution) 
		lplog(LOG_WORDNET,L"");
  numWords+=extractWordsFromSynset(word,synset_ptr->nextss,recur,words,ignoreTopLevel,t)+tmpNumWords;
	return numWords;
}

int extractWordsFromSynset(char *word, SynsetPtr synset_ptr, int recur, set <wstring> &words, bool ignoreTopLevel, sTrace &t)
{
	vector <set <wstring> > wordsBySense;
	extractWordsFromSynset(word, synset_ptr, recur, wordsBySense, ignoreTopLevel, t);
	for (int I = 0; I < wordsBySense.size(); I++)
		words.insert(wordsBySense[I].begin(), wordsBySense[I].end());
	return words.size();
}

void addToWordNet()
{ LFS
	synonymMap[L"professor"]=L"teacher";
	synonymDeletionMap[L"book"]=L"album"; // allowing book and album to be synonyms would be mixing the two largest categories of named items
	synonymDeletionMap[L"detail"]=L"specialty"; // this is not correct - leads to 'full detail' = 'Krugman's specialty'
	synonymDeletionMap[L"person"]=L"party"; // this is not correct - leads to 'full detail' = 'Krugman's specialty'
}

void initWordNet()
{ LFS
	if (!wordNetInitialized)
	{
		if (wninit()<0)
			lplog(LOG_FATAL_ERROR,L"WordNet failed initialization!");
		wordNetInitialized=true;
		addToWordNet();
	}
}

bool checkexist(char *word,int wordClass)
{ LFS
	initWordNet();
  if (getindex(word,wordClass)) return true;
  for (char *morphword = morphstr(word, wordClass); morphword; morphword = morphstr(NULL, wordClass))
    if (getindex(morphword,wordClass)) return true;
  return false;
}

bool checkexist(char *word)
{ LFS
  return checkexist(word,NOUN) || checkexist(word,VERB) || checkexist(word,ADJ) || checkexist(word,ADV);
}

// checks to see that all words in WordNet are defined in
int WordClass::wordCheck(void)
{ LFS
	initWordNet();
  //SynsetPtr sp=findtheinfo_ds("entity", NOUN, HYPOPTR, ALLSENSES);
  //if (sp)
  //  printSynsetStruct(0,"entity",-1,-2,sp,false);
  //wprintf(L"offsets=%d.",offsets.size());
  tIWMM w=begin(),wEnd=end();
  int combinations=0,unknown=0,notInWordNet=0,derivations=0,numWords=0,word=0;
  for (; w!=wEnd; w++) numWords++;
  for (w=begin(); w!=wEnd; w++)
  {
    word++;
    if ((word&15)==15) wprintf(L"%07d out of %07d\r",word,numWords);
    //SynsetPtr sp=findtheinfo_ds((char *)w->first.c_str(), NOUN, HYPERPTR, ALLSENSES);
    //if (sp)
    //  printSynsetStruct(0,(char *)w->first.c_str(),0,2,sp,true);
    lplog(L"Word %s has no mainEntry!",w->first.c_str());
    if (w->second.mainEntry==wNULL &&
        (w->second.query(nounForm)>=0 ||
         w->second.query(verbForm)>=0 ||
         w->second.query(adjectiveForm)>=0 ||
         w->second.query(adverbForm)>=0))
      lplog(L"Word %s has no mainEntry!",w->first.c_str());
    if (w->first[1] && w->first[0]>='a' && w->first[0]<='z')
    {
      if (w->second.isUnknown())
        unknown++;
      else if (!w->second.isRareWord() && !(w->second.flags&tFI::queryOnLowerCase) && !checkexist((char *)w->first.c_str()))
      {
        tFI *fi=&w->second;
        if (fi->query(COMBINATION_FORM_NUM)<0)
        {
          if (fi->mainEntry!=wNULL)
            derivations++;
          else
          {
            notInWordNet++;
            //wprintf(L"\nWord %s was not found in WordNet - (%d) ",w->first.c_str(),fi->usagePatterns[tFI::TRANSFER_COUNT]);
            //for (unsigned int f=0; f<fi->count; f++)
            //  wprintf(L"%s ",fi->Form(f)->name.c_str());
          }
        }
        else
          combinations++;
      }
    }
  }
  lplog(LOG_FATAL_ERROR,L"combinations=%d unknown=%d derivations=%d notInWordNet=%d.",combinations,unknown,derivations,notInWordNet);
  return 0;
}

// set categories of words based on WordNet to be used with time and place
int setWordNetCategoryBits(void)
{ LFS
	initWordNet();
  return 0;
}

// Thesaurus.com gives better results than WordNet (checked for professor and columnist)
// thesaurus changed - its new entries are:
// http://thesaurus.com/browse/columnist?posFilter=noun
// the old entries (which we parse) are:
// http://thesaurus.com/t2opt/out?desturl=browse/columnist&posFilter=noun
// also better than www.synonyms.net: http://www.synonyms.net/synonym/columnist or 
// Big Huge Thesaurus: key 78c2b0a82a3c06236622bb4f8158ead9 (http://words.bighugelabs.com/api/2/78c2b0a82a3c06236622bb4f8158ead9/'word'/json)
void scrapeOldThesaurus(wstring word,set <wstring> &synonyms, int synonymType,bool forceWebReread)
{ LFS
	wstring webAddress=L"http://thesaurus.com/t2opt/out?desturl=browse/"+word+L"&posFilter=",epath=word+L".thesaurus.txt",filePathOut,buffer,cSynonymType,match,headers;
	switch (synonymType)
	{
		case NOUN: webAddress+=L"noun"; break;
		case ADJ: webAddress+=L"adjective";  break; // may not work! 
		case VERB: webAddress+=L"verb"; break;
		case ADV: webAddress+=L"adverb"; break; // may not work!
		default:;
	}
	int space,lastNewLine=1000;
	while ((space=webAddress.find(' '))!=wstring::npos) 
		webAddress[space]='+';
	while ((space=epath.find(' '))!=wstring::npos) 
		epath[space]='+';
	Internet::getWebPath(-1,webAddress,buffer,epath,L"webSearchCache",filePathOut,headers,synonymType+1,true,true, forceWebReread);
	size_t beginPos=buffer.find(L"Main Entry:",0);
	if (beginPos==wstring::npos) 
		return;
	beginPos+=wcslen(L"Main Entry:");
	size_t endPos=1000000,tmpPos;
	wchar_t *endStr[]={L"Main Entry:",L"Roget's 21st Century Thesaurus",L"Adjective Finder",L"Synonym Collection",L"Search another word",L"Antonyms:", L"* = informal/non-formal usage",NULL };
	for (int I=0; endStr[I]!=NULL; I++)
		if ((tmpPos=buffer.find(endStr[I],beginPos))!=wstring::npos && tmpPos<endPos)
			endPos=tmpPos;
	if (endPos==wstring::npos)
		return;
	match=buffer.substr(beginPos,endPos-beginPos);
	bool noMainEntryMatch=(match.find(word)==wstring::npos),addressEncountered=false;
	size_t pos=match.find(L"Synonyms:");
	if (pos!=wstring::npos)
	{
		wstring s;
		for (pos+=wcslen(L"Synonyms:"); pos<(signed)match.length(); pos++)
			if (iswalpha(match[pos]) || match[pos]==L'\'')
				s+=match[pos];
			else if (match[pos]==L' ')
			{
				if (!s.empty()) s+=match[pos];
			}
			else if (match[pos]==L',')
			{
				while (s.length()>0 && iswspace(s[s.length()-1]))
					s.erase(s.length()-1);
				transform (s.begin (), s.end (), s.begin (), (int(*)(int)) tolower);
				if (s.length()>0)
				{
					if (s.length()>=WORDBUF)
						lplog(LOG_WHERE|LOG_ERROR,L"Synonym of %s (%s) is too long \n%s.",word.c_str(),s.c_str(),match.c_str());
					else
						synonyms.insert(s);
				}
				s.clear();
			}
			else if (match[pos-1]!=L',' && match[pos]==13 && match[pos+1]==10 && 
				(iswupper(match[pos+2]) || iswdigit(match[pos+2]) || !iswalpha(match[pos+2]))) // Ads start with unpredictable strings, but always capitalized, after a newline.
			{
				break;
			}
			else if (match[pos]==13 && match[pos+1]==10)
				lastNewLine=s.length();
			else if (addressEncountered=match[pos]==L'.' && pos>3 && match[pos-1]==L'w' && match[pos-2]==L'w' && match[pos-3]==L'w')
				break;
		transform (s.begin (), s.end (), s.begin (), (int(*)(int)) tolower);
		if (s.length()>0)
		{
			if ((s.length()>=64 || addressEncountered) && lastNewLine<(signed)s.length())
				s=s.substr(0,lastNewLine);
			if (s.length()>=64)
				lplog(LOG_WHERE|LOG_ERROR,L"Synonym of %s (%s) is too long \n%s.",word.c_str(),s.c_str(),match.c_str());
			else
				synonyms.insert(s);
		}
	}
	extern int logSynonymDetail;
	if (noMainEntryMatch && synonyms.find(word)==synonyms.end() && logSynonymDetail>0)
		lplog(LOG_WHERE,L"%s itself not found in synonyms [%s].",word.c_str(),setString(synonyms,buffer,L"|").c_str());
}

void scrapeNewThesaurus(wstring word, int synonymType, vector <sDefinition> &vd)
{
	int space;
	while ((space = word.find('_')) != wstring::npos)
		word[space] = ' ';
	wstring webAddress = L"http://thesaurus.com/browse/" + word + L"?posfilter=", epath = word + L".thesaurus.txt", filePathOut, buffer, cSynonymType, match, headers;
	switch (synonymType)
	{
	case NOUN: webAddress += L"noun"; break;
	case ADJ: webAddress += L"adjective";  break; // may not work! 
	case VERB: webAddress += L"verb"; break;
	case ADV: webAddress += L"adverb"; break; // may not work!
	default:;
	}
	//int lastNewLine = 1000;
	while ((space = webAddress.find(' ')) != wstring::npos)
		webAddress[space] = '+';
	while ((space = epath.find(' ')) != wstring::npos)
		epath[space] = '+';
	Internet::getWebPath(-1, webAddress, buffer, epath, L"webSearchCache", filePathOut, headers, synonymType, false, true);
	if (buffer.find(L"<li id=\"words-gallery-no-results\">no thesaurus results</li>") != wstring::npos ||
		buffer.find(L"there's not a match") != wstring::npos)
		return;
	size_t beginPos = 0;
	wstring spaceTest;
	if (firstMatch(buffer, L"<strong>0</strong>", L"<span>Synonyms found <span class=\"headword\">", beginPos, spaceTest, false) != wstring::npos)
		return;
	while (true)
	{
		sDefinition d;
		wTM(word, d.mainEntry);
		wstring beginString = L"<div class=\"synonym-description\">", endString = L"</div>", description, synonymList;
		size_t endPos;
		beginPos = buffer.find(beginString);
		if (beginPos != wstring::npos && (endPos = buffer.find(endString, beginPos + beginString.length())) != wstring::npos)
		{
			description = buffer.substr(beginPos + beginString.length(), endPos - beginPos - beginString.length());
			buffer.erase(beginPos, endPos - beginPos);
			/*
				<div class="synonym-description">
				<em class="txt">verb</em>
				<strong class="ttl">dig and search</strong>
				</div>
				*/
			wstring wordTypeStr;
			beginPos = 0;
			if (nextMatch(description, L"<em class=\"txt\">", L"</em>", beginPos, wordTypeStr, false))
				lplog(LOG_FATAL_ERROR, L"Can't find wordType");
			wTM(wordTypeStr, d.wordType);
			wstring primarySynonym;
			beginPos = 0;
			if (nextMatch(description, L"<strong class=\"ttl\">", L"</strong>", beginPos, primarySynonym, false))
				lplog(LOG_FATAL_ERROR, L"Can't find short description");
			string ps;
			d.primarySynonyms.push_back(wTM(primarySynonym, ps));
			beginString = L"<div class=\"relevancy-list\">";
			beginPos = buffer.find(beginString, beginPos);
			if (beginPos != wstring::npos && (endPos = buffer.find(endString, beginPos + beginString.length())) != wstring::npos)
			{
				synonymList = buffer.substr(beginPos + beginString.length(), endPos - beginPos - beginString.length());
				buffer.erase(beginPos, endPos - beginPos);
				wstring synonymEntry;
				beginPos = 0;
				while (!nextMatch(synonymList, L"<li", L"</li>", beginPos, synonymEntry, false))
				{
					/*
					<li >
					<a href="http://thesaurus.com/browse/embed"
					data-id="1"
					data-category="{&quot;name&quot;: &quot;relevant-3&quot;, &quot;color&quot;: &quot;#fcbb45&quot;}"
					data-complexity="2"
					data-length="2">
					<span class="text">embed</span>
					<span class="star inactive">star</span>
					</a>
					</li>
					*/
					size_t bp = 0;
					wstring complexity, length, synonym;
					if (nextMatch(synonymEntry, L"data-complexity=\"", L"\"", bp, complexity, false))
						lplog(LOG_FATAL_ERROR, L"Can't find complexity");
					bp = 0;
					if (nextMatch(synonymEntry, L"data-length=\"", L"\"", bp, length, false))
						lplog(LOG_FATAL_ERROR, L"Can't find length");
					bp = 0;
					if (nextMatch(synonymEntry, L"<span class=\"text\">", L"</span>", bp, synonym, false))
						lplog(LOG_FATAL_ERROR, L"Can't find synonym entry");
					synonym += L"|" + complexity + L"|" + length;
					string ss;
					d.accumulatedSynonyms.push_back(wTM(synonym, ss));
				}
			}
			else
			{
				lplog(LOG_FATAL_ERROR, L"Thesaurus has no synonyms");
			}
			size_t nextBeginPos = buffer.find(beginString);
			beginString = L"<section class=\"container-info antonyms\" >";
			endString = L"</section>";
			beginPos = buffer.find(beginString);
			// antonym list for next synonym
			if (!(beginPos >= nextBeginPos && nextBeginPos != wstring::npos) &&
			   beginPos != wstring::npos && (endPos = buffer.find(endString, beginPos + beginString.length())) != wstring::npos)
			{
				wstring antonymList = buffer.substr(beginPos + beginString.length(), endPos - beginPos - beginString.length());
				buffer.erase(beginPos, endPos - beginPos);
				wstring antonymEntry;
				beginPos = 0;
				while (!nextMatch(antonymList, L"<li", L"</li>", beginPos, antonymEntry, false))
				{
					/*
					<li >
					<a href="http://thesaurus.com/browse/embed"
					data-id="1"
					data-category="{&quot;name&quot;: &quot;relevant-3&quot;, &quot;color&quot;: &quot;#fcbb45&quot;}"
					data-complexity="2"
					data-length="2">
					<span class="text">embed</span>
					<span class="star inactive">star</span>
					</a>
					</li>
					*/
					size_t bp = 0;
					wstring complexity, length, antonym;
					if (nextMatch(antonymEntry, L"data-complexity=\"", L"\"", bp, complexity, false))
						lplog(LOG_FATAL_ERROR, L"Can't find complexity");
					bp = 0;
					if (nextMatch(antonymEntry, L"data-length=\"", L"\"", bp, length, false))
						lplog(LOG_FATAL_ERROR, L"Can't find length");
					if (nextMatch(antonymEntry, L"<span class=\"text\">", L"</span>", bp, antonym, false))
						lplog(LOG_FATAL_ERROR, L"Can't find antonym entry");
					antonym += L"|" + complexity + L"|" + length;
					string as;
					d.accumulatedAntonyms.push_back(wTM(antonym, as));
				}
			}
			vd.push_back(d);
		}
		else
		{
			if (vd.empty())
			{
				//lplog(LOG_FATAL_ERROR, L"Thesaurus has no description");
				//printf("\nThesaurus has no description for %S (%S)\n", word.c_str(), filePathOut.c_str());
				break;
			}
			else
				break;
		}
	}
}

void split(string str, vector <string> &words, char *splitch);

bool getSynonymsFromDB(MYSQL mysql,wstring word, vector < set <wstring> > &synonyms, int synonymType)
{
	bool entriesAdded = false;
	wstring query = L"select primarySynonyms, accumulatedSynonyms from thesaurus where mainEntry = '";
	query += word + L"' and ";
	// thesaurus mappings
	// "adj"=1, "adv"=2, "prep"=4, "pron"=8, "conj"=16, "det"=32, "interj"=64, "n"=128, "v"=256, NULL };
	if (synonymType == 1) // NOUN
		query += L"(wordType&128)=128";
	else if (synonymType == 2) // VERB
		query += L"(wordType&256)=256";
	else if (synonymType == 3) // ADJ
		query += L"(wordType&1)=1";
	else if (synonymType == 4) // ADV
		query += L"(wordType&2)=2";
	if (!myquery(&mysql, L"LOCK TABLES thesaurus READ")) return false;
	MYSQL_RES *result = NULL;
	MYSQL_ROW sqlrow;
	if (myquery(&mysql, (wchar_t *)query.c_str(), result))
	{
		if ((sqlrow = mysql_fetch_row(result)) != NULL)
		{
			string primarySynonyms = (sqlrow[0] == NULL) ? "" : sqlrow[0];
			string properties = (sqlrow[1] == NULL) ? "" : sqlrow[1];
			int lastBegin = 0;
			set <wstring> sense;
			for (int s = 0; s < properties.size(); s++)
				if (properties[s] == ';')
				{
					wstring wtmp;
					mTW(properties.substr(lastBegin, s - lastBegin), wtmp);
					if (wtmp[wtmp.length() - 1] == L'*')
						wtmp.erase(wtmp.length() - 1);
					sense.insert(wtmp);
					lastBegin = s + 1;
					entriesAdded = true;
				}
			vector <string> ps;
			split(primarySynonyms, ps, ";");
			for (int psi = 0; psi < ps.size(); psi++)
			{
				wstring tmpstr;
				if (ps[psi].find(" ") == string::npos)
					sense.insert(mTW(ps[psi], tmpstr));
				else
				{
					int wo;
					if ((wo=ps[psi].find(" or ")) != string::npos)
					{
						vector <string> words;
						split(ps[psi], words, " ");
						if (words.size() == 3 && words[1] == "or")
						{
							sense.insert(mTW(words[0], tmpstr));
							sense.insert(mTW(words[2], tmpstr));
						}
					}
				}
			}
			if (sense.size()>0)
				synonyms.push_back(sense);
		}
		mysql_free_result(result);
	}
	myquery(&mysql, L"UNLOCK TABLES");
	return entriesAdded;
}

unordered_map <wstring,vector < set <wstring> > > internalSynonymMap[4];
void Source::getSynonyms(wstring word, set <wstring> &synonyms, int synonymType)
{
	vector <set <wstring> > synonymsSenses;
	getSynonyms(word, synonymsSenses, synonymType);
	for (int s = 0; s < synonymsSenses.size(); s++)
		synonyms.insert(synonymsSenses[s].begin(), synonymsSenses[s].end());
}

void Source::getSynonyms(wstring word, vector <set <wstring> > &synonyms, int synonymType)
{
	LFS
		// check if word is legal
		for (int I = 0; I<word.length(); I++)
			if (!iswalpha(word[I]) && word[I] != L'_') // two words has a _ in it
				return;
	if (word.find(L"http")!=wstring::npos)
		return;
	unordered_map <wstring, vector < set <wstring> > >::iterator smi = internalSynonymMap[synonymType].find(word);
	if (smi != internalSynonymMap[synonymType].end())
	{
		synonyms = smi->second;
		return;
	}
	initWordNet();
	string sWord;
	SynsetPtr sp = findtheinfo_ds(wTM(word, sWord), synonymType, SIMPTR, ALLSENSES);

	extractWordsFromSynset(wTM(word, sWord), sp, 0, synonyms, false, debugTrace);
	unordered_map <wstring, wstring>::iterator si = synonymMap.find(word);
	if (si != synonymMap.end())
	{
		for (int I = 0; I < synonyms.size(); I++)
			synonyms[I].insert(si->second);
	}
	if (iswalpha(word[0]))
	{
		if (!getSynonymsFromDB(mysql, word, synonyms, synonymType))
		{
			vector <sDefinition> d;
			scrapeNewThesaurus(word, synonymType, d);
			for (int n = 0; n < d.size(); n++)
			{
				set <wstring> sense;
				for (int I = 0; I < d[n].accumulatedSynonyms.size(); I++)
				{
					wstring tmp;
					mTW(d[n].accumulatedSynonyms[I], tmp);
					transform(tmp.begin(), tmp.end(), tmp.begin(), (int(*)(int)) tolower);
					wchar_t chopSense = tmp.find('|');
					if (chopSense != wstring::npos)
						tmp.erase(chopSense);
					sense.insert(tmp);
				}
				synonyms.push_back(sense);
			}
		}
	}
	si = synonymDeletionMap.find(word);
	if (si != synonymDeletionMap.end())
		for (int I = 0; I < synonyms.size(); I++)
			synonyms[I].erase(si->second);
	internalSynonymMap[synonymType][word] = synonyms;
}

void Source::getWordNetSynonymsOnly(wstring word, vector <set <wstring> > &synonyms, int synonymType)
{
	LFS
	initWordNet();
	string sWord;
	SynsetPtr sp = findtheinfo_ds(wTM(word, sWord), synonymType, SIMPTR, ALLSENSES);
	extractWordsFromSynset(wTM(word, sWord), sp, 0, synonyms, false, debugTrace);
}


void getAntonyms(wstring word,set <wstring> &antonyms,sTrace &t)
{ LFS
	initWordNet();
	string sWord;
  SynsetPtr sp=findtheinfo_ds(wTM(word,sWord), ADJ, ANTPTR, ALLSENSES);
	extractWordsFromSynset(wTM(word,sWord),sp,0,antonyms,true,t);
}

int getFamiliarity(wstring word,bool isAdjective)
{ LFS
	initWordNet();
	string sWord;
	IndexPtr index=index_lookup(wTM(word,sWord), (isAdjective) ? ADJ : NOUN);
	if (index==NULL) return 0;
	return index->sense_cnt;
}

int getHighestFamiliarity(wstring word)
{ LFS
	if (word.empty()) 
		return -1;
	initWordNet();
	int maxFamiliarity=-1;
	string sWord;
	char *w=wTM(word,sWord);
	if (!*w)
		return -1;
	IndexPtr index=index_lookup(w, ADJ);
	if (index!=NULL) 
		maxFamiliarity=index->sense_cnt;
	if ((index=index_lookup(w, NOUN))!=NULL)
		maxFamiliarity=max(maxFamiliarity,index->sense_cnt);
	if ((index=index_lookup(w, VERB))!=NULL)
		maxFamiliarity=max(maxFamiliarity,index->sense_cnt);
	if ((index=index_lookup(w, ADV))!=NULL)
		maxFamiliarity=max(maxFamiliarity,index->sense_cnt);
	return maxFamiliarity;
}

void getHyperNyms(SynsetPtr sp,vector < set <string> > &objects,bool avoidCapitalizedNouns,bool print)
{ LFS
	for (; sp; sp=sp->nextss)
	{
		set <string> oneSenseObjects;
		SynsetPtr o=sp;
		while (o->ptrlist)
		{
			int index=o->ptrlist->hereiam;
			o=read_synset(NOUN,index,0);
			for (int w=0; w<o->wcount; w++) 
				if (!avoidCapitalizedNouns || islower(o->words[w][0]))
				{
					oneSenseObjects.insert(o->words[w]);
					if (print)
						printf("%s|",o->words[w]);
				}
			o->ptrlist = traceptrs_ds(o, o->searchtype=HYPERPTR, NOUN,0);
		}
		if (print)
			printf("\n");
		if (oneSenseObjects.size())
			objects.push_back(oneSenseObjects);
	}
}

void getOrderedHyperNyms(SynsetPtr sp,vector < vector <string> > &objects,bool avoidCapitalizedNouns,bool print)
{ LFS
	for (; sp; sp=sp->nextss)
	{
		vector <string> oneSenseObjects;
		SynsetPtr o=sp;
		while (o->ptrlist)
		{
			if (print)
				printf("\n  ");
			oneSenseObjects.push_back("");
			int index=o->ptrlist->hereiam;
			o=read_synset(NOUN,index,0);
			for (int w=0; w<o->wcount; w++) 
				if (!avoidCapitalizedNouns || islower(o->words[w][0]))
				{
					oneSenseObjects.push_back(o->words[w]);
					if (print)
						printf("%s|",o->words[w]);
				}
			o->ptrlist = traceptrs_ds(o, o->searchtype=HYPERPTR, NOUN,0);
		}
		if (print)
			printf("\n");
		if (oneSenseObjects.size())
			objects.push_back(oneSenseObjects);
	}
}

bool initHyperNym(wstring word,SynsetPtr &sp)
{ LFS
	initWordNet();
	string sWord;
  sp=findtheinfo_ds(wTM(word,sWord), NOUN, HYPERPTR, ALLSENSES);
  if (!sp)
	{
		char *mword;
		for (mword = morphstr(wTM(word,sWord), NOUN); mword; mword = morphstr(NULL, NOUN))
			if (sp=findtheinfo_ds(mword, NOUN, HYPERPTR, ALLSENSES)) break;
	}
	return sp!=NULL;
}

bool hasHyperNym(SynsetPtr sp,string MBCSHyperNum,bool &found)
{ LFS
	int numSenses=0,numHypernymFound=0;
	for (; sp; sp=sp->nextss,numSenses++)
	{
		SynsetPtr o=sp;
		while (o->ptrlist)
		{
			int index=o->ptrlist->hereiam;
			o=read_synset(NOUN,index,0);
			for (int w=0; w<o->wcount; w++) 
				if (MBCSHyperNum==o->words[w]) 
				{
					numHypernymFound++;
					found=true;
					break;
				}
			o->ptrlist = traceptrs_ds(o, o->searchtype=HYPERPTR, NOUN,0);
		}
	}
	return numSenses==numHypernymFound;
}

bool hasHyperNym(wstring word,wstring hyperNym,bool &found)
{ LFS
  SynsetPtr sp;
	initHyperNym(word,sp);
	string MBCSHyperNym;
	wTM(hyperNym,MBCSHyperNym);
	return hasHyperNym(sp,MBCSHyperNym,found);
}

void splitMultiWord(string MBCSMultiWord,vector <wstring> &words)
{ LFS
  wstring w;
	splitMultiWord(mTW(MBCSMultiWord,w),words);
}

void splitMultiWord(wstring multiWord,vector <wstring> &words)
{ LFS
	words.clear();
  wstring word;
  for (unsigned I=0; I<multiWord.length(); I++)
    if (multiWord[I]==' ' || multiWord[I]=='_')
    {
      if (!word.length()) continue;
      words.push_back(word);
      word.erase();
    }
    else
      word+=multiWord[I];
  if (word.length())
    words.push_back(word);
}

/*
static struct {
    char *option;		// user's search request 
    int search;			// search to pass findtheinfo() 
    int pos;			// part-of-speech to pass findtheinfo() 
    int helpmsgidx;		// index into help message table 
    char *compactLabel;		// text for search header message 
} *optptr, optlist[] = {
    { "-synsa", SIMPTR,	ADJ, 0, "Similarity" },
    { "-antsa", ANTPTR,	ADJ, 1, "Antonyms" },
    { "-perta", PERTPTR, ADJ, 0, "Pertainyms" },
    { "-attra", ATTRIBUTE, ADJ, 2, "Attributes" },
    { "-domna", CLASSIFICATION, ADJ, 3, "Domain" },
    { "-domta", CLASS, ADJ, 4, "Domain Terms" },
    { "-famla", FREQ, ADJ, 5, "Familiarity" },
    { "-grepa", WNGREP, ADJ, 6, "Grep" },

    { "-synsn", HYPERPTR, NOUN, 0, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
    { "-antsn", ANTPTR,	NOUN, 2, "Antonyms" },
    { "-coorn", COORDS, NOUN, 3, "Coordinate Terms (sisters)" },
    { "-hypen", -HYPERPTR, NOUN, 4, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
    { "-hypon", HYPOPTR, NOUN, 5, "Hyponyms" },
    { "-treen", -HYPOPTR, NOUN, 6, "Hyponyms" },
    { "-holon", HOLONYM, NOUN, 7, "Holonyms" },
    { "-sprtn", ISPARTPTR, NOUN, 7, "Part Holonyms" },
    { "-smemn", ISMEMBERPTR, NOUN, 7, "Member Holonyms" },
    { "-ssubn", ISSTUFFPTR, NOUN, 7, "Substance Holonyms" },
    { "-hholn",	-HHOLONYM, NOUN, 8, "Holonyms" },
    { "-meron", MERONYM, NOUN, 9, "Meronyms" },
    { "-subsn", HASSTUFFPTR, NOUN, 9, "Substance Meronyms" },
    { "-partn", HASPARTPTR, NOUN, 9, "Part Meronyms" },
    { "-membn", HASMEMBERPTR, NOUN, 9, "Member Meronyms" },
    { "-hmern", -HMERONYM, NOUN, 10, "Meronyms" },
    { "-nomnn", DERIVATION, NOUN, 11, "Derived Forms" },
    { "-derin", DERIVATION, NOUN, 11, "Derived Forms" },
    { "-domnn", CLASSIFICATION, NOUN, 13, "Domain" },
    { "-domtn", CLASS, NOUN, 14, "Domain Terms" },
    { "-attrn", ATTRIBUTE, NOUN, 12, "Attributes" },
    { "-famln", FREQ, NOUN, 15, "Familiarity" },
    { "-grepn", WNGREP, NOUN, 16, "Grep" },

    { "-synsv", HYPERPTR, VERB, 0, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
    { "-simsv", RELATIVES, VERB, 1, "Synonyms (Grouped by Similarity of Meaning)" },
    { "-antsv", ANTPTR, VERB, 2, "Antonyms" },
    { "-coorv", COORDS, VERB, 3, "Coordinate Terms (sisters)" },
    { "-hypev", -HYPERPTR, VERB, 4, "Synonyms/Hypernyms (Ordered by Estimated Frequency)" },
    { "-hypov", HYPOPTR, VERB, 5, "Troponyms (hyponyms)" },
    { "-treev", -HYPOPTR, VERB, 5, "Troponyms (hyponyms)" },
    { "-tropv", -HYPOPTR, VERB, 5, "Troponyms (hyponyms)" },
    { "-entav", ENTAILPTR, VERB, 6, "Entailment" },
    { "-causv", CAUSETO, VERB, 7, "\'Cause To\'" },
    { "-nomnv", DERIVATION, VERB, 8, "Derived Forms" },
    { "-deriv", DERIVATION, VERB, 8, "Derived Forms" },
    { "-domnv", CLASSIFICATION, VERB, 10, "Domain" },
    { "-domtv", CLASS, VERB, 11, "Domain Terms" },
    { "-framv", FRAMES, VERB, 9, "Sample Sentences" },
    { "-famlv", FREQ, VERB, 12, "Familiarity" },
    { "-grepv", WNGREP, VERB, 13, "Grep" },

    { "-synsr", SYNS, ADV, 0, "Synonyms" },
    { "-antsr", ANTPTR, ADV, 1, "Antonyms" },
    { "-pertr", PERTPTR, ADV, 0, "Pertainyms" },
    { "-domnr", CLASSIFICATION, ADV, 2, "Domain" },
    { "-domtr", CLASS, ADV, 3, "Domain Terms" },
    { "-famlr", FREQ, ADV, 4, "Familiarity" },
    { "-grepr", WNGREP, ADV, 5, "Grep" },

    { "-over", OVERVIEW, ALL_POS, -1, "Overview" },
    { NULL, 0, 0, 0, NULL }
};
*/
bool addCoords(wchar_t *word,vector <tmWS > &objects,int wnClass,wchar_t *preferredSense,int &numFirstSense,set <string> &ignoreSenses,bool print)
{ LFS
	initWordNet();
	string MBCSWord;
	wTM(word,MBCSWord);
	string MBCSPreferredSense;
	if (preferredSense) 
		wTM(preferredSense,MBCSPreferredSense);
	int originalSize=objects.size();
	numFirstSense=0;
	vector <wstring> words;
	IndexPtr idx = index_lookup((char *)MBCSWord.c_str(),wnClass);
	if (!idx) return false;
	for (int sense = 0; sense < idx->off_cnt; sense++) 
	{
		if (sense==1) numFirstSense=objects.size();
		bool foundSense=false,ignoreSense=false;
		SynsetPtr synptr = read_synset(wnClass, idx->offset[sense], idx->wd);
		for (int w=0; w<synptr->wcount; w++) 
		{
			if (!strcmp(MBCSWord.c_str(),synptr->words[w]))
				continue;
			if (preferredSense && !strcmp(MBCSPreferredSense.c_str(),synptr->words[w]))
				foundSense=true;
			if (ignoreSense=ignoreSenses.size() && ignoreSenses.find(synptr->words[w])!=ignoreSenses.end())
			{
				if (print)
					printf("%s ** IGNORED SENSE found **\n",synptr->words[w]); 
				break;
			}
			else if (print)
				printf("%s\n",synptr->words[w]); 
			splitMultiWord(synptr->words[w],words);
			objects.push_back(words);
		}
		if (ignoreSense) continue;
		for(int i = 0; i < synptr->ptrcount; i++) 
		{
			if((synptr->ptrtyp[i] == HYPERPTR || synptr->ptrtyp[i] == INSTANCE) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword))) 
			{
				SynsetPtr cursyn = read_synset(wnClass, synptr->ptroff[i], "");
				for (int w=0; w<cursyn->wcount; w++) 
				{
					if (preferredSense && !strcmp(MBCSPreferredSense.c_str(),cursyn->words[w]))
						foundSense=true;
					if (print)
						printf("%s\n",cursyn->words[w]); 
					splitMultiWord(cursyn->words[w],words);
					objects.push_back(words);
				}
				for (int j = 0; j < cursyn->ptrcount; j++) 
				{
					if (cursyn->ptrtyp[j] == HYPOPTR ||	cursyn->ptrtyp[j] == INSTANCES)
					{
						SynsetPtr cs2=read_synset(wnClass, cursyn->ptroff[j], "");
						for (int w=0; w<cs2->wcount; w++) 
						{
							if (print)
								printf("%s\n",cs2->words[w]); 
							splitMultiWord(cs2->words[w],words);
							objects.push_back(words);
						}
				  }
				}
			}
		}
		if (preferredSense)
		{
			if (!foundSense)
				objects.erase(objects.begin()+originalSize,objects.end());
			else
				return objects.size()>0;
		}
	}
	return objects.size()>0;
}

bool addOneSenseCoords(wchar_t *word,set < wstring > &objects,int wnClass,int &numSense)
{ LFS
	initWordNet();
	string MBCSWord;
	wTM(word,MBCSWord);
	numSense=0;
	IndexPtr idx = index_lookup((char *)MBCSWord.c_str(),wnClass);
	if (!idx) return false;
	numSense=idx->off_cnt;
	for (int sense = 0; sense < idx->off_cnt; sense++) 
	{
		SynsetPtr synptr = read_synset(wnClass, idx->offset[sense], idx->wd);
		wstring tw;
		for (int w=0; w<synptr->wcount; w++) 
			if (strcmp(MBCSWord.c_str(),synptr->words[w]))
				objects.insert(mTW(synptr->words[w],tw));
		for(int i = 0; i < synptr->ptrcount; i++) 
		{
			if((synptr->ptrtyp[i] == HYPERPTR || synptr->ptrtyp[i] == INSTANCE) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword))) 
			{
				SynsetPtr cursyn = read_synset(wnClass, synptr->ptroff[i], "");
				for (int w=0; w<cursyn->wcount; w++) 
					objects.insert(mTW(cursyn->words[w],tw));
				for (int j = 0; j < cursyn->ptrcount; j++) 
				{
					if (cursyn->ptrtyp[j] == HYPOPTR ||	cursyn->ptrtyp[j] == INSTANCES)
					{
						SynsetPtr cs2=read_synset(wnClass, cursyn->ptroff[j], "");
						for (int w=0; w<cs2->wcount; w++) 
							objects.insert(mTW(cs2->words[w],tw));
				  }
				}
			}
		}
	}
	return objects.size()>0;
}

void recurseHyponym(int index,int depth,vector <tmWS > &objects,char *preferredSense,bool &foundSense,set <string> &ignoreSenses,bool print)
{ LFS
	vector <wstring> words;
	SynsetPtr cursyn = read_synset(NOUN, index, "");
	if (print && cursyn->wcount) printf("%*s==>",depth*2," ");
	bool ignoreSense=false;
	for (int w=0; w<cursyn->wcount; w++) 
	{
		if (preferredSense && !strcmp(preferredSense,cursyn->words[w]))
			foundSense=true;
		if (ignoreSense=ignoreSenses.size() && ignoreSenses.find(cursyn->words[w])!=ignoreSenses.end())
		{
			if (print)
				printf("%s (IGNORED RECURSIVE SENSE found **)",cursyn->words[w]); 
			break;
		}
		else if (print)
			printf("%s,",cursyn->words[w]); 
		splitMultiWord(cursyn->words[w],words);
		objects.push_back(words);
	}
	if (print && cursyn->wcount) wprintf(L"\n"); 
	if (ignoreSense) return;
	for (int k = 0; k < cursyn->ptrcount; k++) 
		if (cursyn->ptrtyp[k] == HYPOPTR ||	cursyn->ptrtyp[k] == INSTANCES)
			recurseHyponym(cursyn->ptroff[k],depth+1,objects,preferredSense,foundSense,ignoreSenses,print);
}

bool addHyponyms(wchar_t *word,vector <tmWS > &objects,wchar_t *preferredSense,set <string> &ignoreSenses,bool print)
{ LFS
	initWordNet();
	string MBCSPreferredSense,MBCSWord;
	wTM(word,MBCSWord);
	if (preferredSense) wTM(preferredSense,MBCSPreferredSense);
	int originalSize=objects.size();
	vector <wstring> words;
	IndexPtr idx = index_lookup((char *)MBCSWord.c_str(),NOUN);
  if (!idx)
	{
		char *mword;
		for (mword = morphstr((char *)MBCSWord.c_str(), NOUN); mword; mword = morphstr(NULL, NOUN))
			if (idx = index_lookup(mword,NOUN)) break;
		if (!idx) return false;
		MBCSWord=mword;
	}
	for (int sense = 0; sense < idx->off_cnt; sense++) 
	{
		bool foundSense=false,ignoreSense=false;
		SynsetPtr synptr = read_synset(NOUN, idx->offset[sense], idx->wd);
		if (print && synptr->wcount) printf("PRINCIPAL:"); 
		for (int w=0; w<synptr->wcount; w++) 
		{
			if (!strcmp(MBCSWord.c_str(),synptr->words[w]))
				continue;
			if (preferredSense && !strcmp((char *)MBCSPreferredSense.c_str(),synptr->words[w]))
				foundSense=true;
			if (ignoreSense=ignoreSenses.size() && ignoreSenses.find(synptr->words[w])!=ignoreSenses.end())
			{
				if (print)
					printf("%s (IGNORED SENSE found **)",synptr->words[w]); 
				break;
			}
			else if (print)
				printf("%s,",synptr->words[w]); 
			splitMultiWord(synptr->words[w],words);
			objects.push_back(words);
		}
		if (print && synptr->wcount) printf("\n"); 
		if (ignoreSense) continue;
		for(int i = 0; i < synptr->ptrcount; i++) 
		{
			if((synptr->ptrtyp[i] == HYPOPTR || synptr->ptrtyp[i] == INSTANCES) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword))) 
			{
				recurseHyponym(synptr->ptroff[i],1,objects,(char *)MBCSPreferredSense.c_str(),foundSense,ignoreSenses,print);
			}
		}
		if (preferredSense)
		{
			if (!foundSense)
				objects.erase(objects.begin()+originalSize,objects.end());
			else
				return objects.size()>0;
		}
	}
	return objects.size()>0;
}

void recurseHyponym(int index,int depth,set <wstring> &objects,bool print)
{ LFS
	vector <wstring> words;
  wstring tw;
	SynsetPtr cursyn = read_synset(NOUN, index, "");
	if (print && cursyn->wcount) printf("%*s==>",depth*2," ");
	for (int w=0; w<cursyn->wcount; w++) 
		if (!strchr(cursyn->words[w],' ') && !strchr(cursyn->words[w],'_'))
			objects.insert(mTW(cursyn->words[w],tw));
	if (print && cursyn->wcount) printf("\n");
	for (int k = 0; k < cursyn->ptrcount; k++) 
		if (cursyn->ptrtyp[k] == HYPOPTR ||	cursyn->ptrtyp[k] == INSTANCES)
			recurseHyponym(cursyn->ptroff[k],depth+1,objects,print);
}

bool addHyponyms(wchar_t *word,set <wstring> &objects,bool print)
{ LFS
	initWordNet();
	string MBCSWord;
	wTM(word,MBCSWord);
	//int originalSize=objects.size();
	vector <wstring> words;
	IndexPtr idx = index_lookup((char *)MBCSWord.c_str(),NOUN);
  if (!idx)
	{
		char *mword;
		for (mword = morphstr((char *)MBCSWord.c_str(), NOUN); mword; mword = morphstr(NULL, NOUN))
			if (idx = index_lookup(mword,NOUN)) break;
		if (!idx) return false;
		MBCSWord=mword;
	}
	for (int sense = 0; sense < idx->off_cnt; sense++) 
	{
		//bool foundSense=false;
		SynsetPtr synptr = read_synset(NOUN, idx->offset[sense], idx->wd);
		wstring stw;
		for (int w=0; w<synptr->wcount; w++) 
		{
			if (!strcmp(MBCSWord.c_str(),synptr->words[w]))
				continue;
			if (!strchr(synptr->words[w],' ') && !strchr(synptr->words[w],'_'))
				objects.insert(mTW(synptr->words[w],stw));
		}
		if (print && synptr->wcount) printf("\n");
		for(int i = 0; i < synptr->ptrcount; i++) 
		{
			if((synptr->ptrtyp[i] == HYPOPTR || synptr->ptrtyp[i] == INSTANCES) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword))) 
			{
				recurseHyponym(synptr->ptroff[i],1,objects,print);
			}
		}
	}
	return objects.size()>0;
}

wstring getMostCommonSynonym(wstring in,wstring &out,bool isNoun,bool isVerb,bool isAdjective,bool isAdverb,
														 SynsetPtr &sp,IndexPtr &index,set <wstring> &synonyms,int &initialFamiliarity,int &highestFamiliarity,sTrace &t)
{ LFS
unordered_map <wstring,wstring>::iterator smi;
	if ((smi=mostCommonSynonymMap.find(in))==mostCommonSynonymMap.end())
	{
		initWordNet();
		out=in;
		int wnClass=-1;
		if (isNoun) wnClass=NOUN;
		else if (isVerb) wnClass=VERB;
		else if (isAdjective) wnClass=ADJ;
		else if (isAdverb) wnClass=ADV;
		if (wnClass>=0) 
		{
			string inStr;
			sp=findtheinfo_ds(wTM(in,inStr), wnClass, SIMPTR, ALLSENSES);
			index=NULL;
			if (sp) 
			{
				extractWordsFromSynset(wTM(in,inStr),sp,0,synonyms,false,t);
				index=index_lookup(wTM(in,inStr), wnClass);
				if (index) 
				{
					initialFamiliarity=highestFamiliarity=index->sense_cnt;
					for (set <wstring>::iterator si=synonyms.begin(),siEnd=synonyms.end(); si!=siEnd; si++)
					{
						IndexPtr synonymIndex=index_lookup(wTM(*si,inStr), wnClass);
						if (synonymIndex && synonymIndex->sense_cnt>highestFamiliarity)
						{
							highestFamiliarity=synonymIndex->sense_cnt;
							out=*si;
						}
					}
				}
			}
		}
		mostCommonSynonymMap[in]=out;
	}
	else
		out=(smi)->second;
	return out;
}

wstring getMostCommonSynonym(wstring in,wstring &out,bool isNoun,bool isVerb,bool isAdjective,bool isAdverb,sTrace &t)
{ LFS
  SynsetPtr sp;
	IndexPtr index;
	set <wstring> synonyms;
	int initialFamiliarity;
	int highestFamiliarity;
  return getMostCommonSynonym(in,out,isNoun,isVerb,isAdjective,isAdverb,sp,index,synonyms,initialFamiliarity,highestFamiliarity,t);
}

wstring getIsKindOf(wstring in,int &highestFamiliarity)
{ LFS
	initWordNet();
	wstring out=in;
	string inStr;
	IndexPtr idx = index_lookup(wTM(in,inStr),NOUN);
	if (!idx || idx->off_cnt>1) return L"";
	//int initialFamiliarity=highestFamiliarity=idx->sense_cnt;
	for (int sense = 0; sense < idx->off_cnt; sense++) 
	{
		SynsetPtr synptr = read_synset(NOUN, idx->offset[sense], idx->wd);
		for (int w=0; w<synptr->wcount; w++) 
		{
			IndexPtr index=index_lookup(synptr->words[w], NOUN);
			if (index && index->sense_cnt>highestFamiliarity)
			{
				mTW(synptr->words[w],out);
				highestFamiliarity=index->sense_cnt;
			}
		}
		for(int i = 0; i < synptr->ptrcount; i++) 
		{
			if((synptr->ptrtyp[i] == HYPERPTR || synptr->ptrtyp[i] == INSTANCE) &&
				((synptr->pfrm[i] == 0) || (synptr->pfrm[i] == synptr->whichword))) 
			{
				SynsetPtr cursyn = read_synset(NOUN, synptr->ptroff[i], "");
				for (int w=0; w<cursyn->wcount; w++) 
				{
					IndexPtr index=index_lookup(cursyn->words[w], NOUN);
					if (index && index->sense_cnt>highestFamiliarity)
					{
						mTW(cursyn->words[w],out);
						highestFamiliarity=index->sense_cnt;
					}
				}
			}
		}
	}
	return out;
}

bool stripEndingIfFound(wstring &in,wchar_t *ending,wchar_t *replace,int &inflectionFlags)
{ LFS
	if (in.length()>wcslen(ending) && !wcscmp(in.c_str()+in.length()-wcslen(ending),ending))
	{
		wstring save=in;
		save.erase(save.length()-wcslen(ending),wcslen(ending));
		save+=replace;
		tIWMM w=Words.query(save);
		if (w!=Words.end())
		{
			in=save;
			inflectionFlags=w->second.inflectionFlags;
			return true;
		}
	}
	return false;
}

void deriveMainEntry(int where,int fromWhere,wstring &in,int &inflectionFlags,bool isVerb,bool isNoun,wstring &lastNounNotFound,wstring &lastVerbNotFound)
{ LFS
	if (isVerb && !(inflectionFlags&VERB_PRESENT_FIRST_SINGULAR))
	{
		if (in==lastVerbNotFound) return;
		if (in==L"ishas" || in==L"wouldhad" || in==L"ishasdoes" || in==L"should") return;
		if (inflectionFlags&VERB_PRESENT_PARTICIPLE) 
		{
			if (!stripEndingIfFound(in,L"ing",L"",inflectionFlags) && !stripEndingIfFound(in,L"ing",L"e",inflectionFlags) &&
				!stripEndingIfFound(in,L"nning",L"n",inflectionFlags))
				stripEndingIfFound(in,L"rring",L"r",inflectionFlags);
		}
		else if (inflectionFlags&VERB_PAST) 
		{
			if (!stripEndingIfFound(in,L"ied",L"y",inflectionFlags) && !stripEndingIfFound(in,L"ed",L"",inflectionFlags))
				stripEndingIfFound(in,L"ed",L"e",inflectionFlags);
		}
		tIWMM me;
		if ((inflectionFlags&VERB_INFLECTIONS_MASK) && (me=Words.gquery(in)->second.mainEntry)!=wNULL &&
			  me->first!=in && (me->second.inflectionFlags&VERB_PRESENT_FIRST_SINGULAR))
		{
			in=me->first;
			inflectionFlags=me->second.inflectionFlags;
		}
		if ((inflectionFlags&VERB_INFLECTIONS_MASK) && !(inflectionFlags&VERB_PRESENT_FIRST_SINGULAR))
		{
			wstring sFlags;
			tIWMM meError=Words.gquery(in);
			if (!(meError->second.flags&tFI::mainEntryErrorNoted))
				lplog(LOG_DICTIONARY,L"%06d:%s irregular verb [mainEntry %s] (%s,%d).",where,in.c_str(),
					(meError->second.mainEntry!=wNULL) ? meError->second.mainEntry->first.c_str() : L"",
						inflectionFlagsToStr(inflectionFlags&VERB_INFLECTIONS_MASK,sFlags),fromWhere);
			meError->second.flags|=tFI::mainEntryErrorNoted;
			lastVerbNotFound=in;
		}
	}
	if (isNoun && !(inflectionFlags&SINGULAR))
	{
		if (in==lastNounNotFound) return;
		if (inflectionFlags&PLURAL) 
			stripEndingIfFound(in,L"s",L"",inflectionFlags);
		tIWMM me;
		if (!(inflectionFlags&SINGULAR) && (inflectionFlags&NOUN_INFLECTIONS_MASK) && (me=Words.gquery(in)->second.mainEntry)!=wNULL &&
			  me->first!=in && (me->second.inflectionFlags&SINGULAR))
		{
			in=me->first;
			inflectionFlags=me->second.inflectionFlags;
		}
		if (!(inflectionFlags&SINGULAR) && (inflectionFlags&NOUN_INFLECTIONS_MASK))
		{
			wstring sFlags;
			if (in==L"many" || in==L"various") return;
			lplog(LOG_DICTIONARY,L"%06d:%s irregular noun not found [mainEntry %s] (%s,%d).",where,in.c_str(),
				(Words.gquery(in)->second.mainEntry!=wNULL) ? Words.gquery(in)->second.mainEntry->first.c_str() : L"",
						inflectionFlagsToStr(inflectionFlags&NOUN_INFLECTIONS_MASK,sFlags),fromWhere);
			lastNounNotFound=in;
		}
	}
}

void scanCoordObjects(wstring &original,wstring &cdstr,set <wstring> &objects,int wnClass,int &highestCoordFamiliarity,wstring &coordFamiliarity,bool &proposedSubstitution,bool &oneSenseVbNetClassFound)
{ LFS
	wstring tmp;
unordered_map <wstring, set<int> >::iterator inlvtoCi=vbNetVerbToClassMap.end();
	for (set <wstring>::iterator oi=objects.begin(),oiEnd=objects.end(); oi!=oiEnd; oi++)
	{
		cdstr+=*oi;
		string oiStr;
		IndexPtr index=index_lookup(wTM(*oi,oiStr), wnClass);
		if (index) 
		{
			cdstr+=L"["+itos(index->sense_cnt,tmp)+L"]";
			if (index->sense_cnt>highestCoordFamiliarity)
			{
				highestCoordFamiliarity=index->sense_cnt;
				coordFamiliarity=*oi;
				proposedSubstitution=true;
			}
			if (wnClass==VERB) 
			{
				oneSenseVbNetClassFound|=(inlvtoCi=vbNetVerbToClassMap.find(*oi))!=vbNetVerbToClassMap.end();
				if (inlvtoCi!=vbNetVerbToClassMap.end())
				{
					vbNetVerbToClassMap[original].insert(inlvtoCi->second.begin(),inlvtoCi->second.end());
					// lplog(LOG_TIME,L"mapped %s",original.c_str());
				}
			}
		}
		cdstr+=L"|";
	}
	if (cdstr.length())
		cdstr.erase(cdstr.length()-1);
}

bool inEveryGroup(string word,vector < set <string> > &objects)
{ LFS
	for (vector < set <string> >::iterator oi=objects.begin(),oiEnd=objects.end(); oi!=oiEnd; oi++)
		if (oi->find(word)==oi->end())
			return false;
	return true;
}

bool readHyperNymCache(wstring &in,vector < set <string> > &objects)
{ LFS
	if (in==L"con") in=L"_con_"; // prevent Windows redirection
	wstring path=wstring(CACHEDIR)+L"\\wordNetCache\\"+in;
  IOHANDLE fd=_wopen(path.c_str(),O_RDWR|O_BINARY);
  if (fd<0) return false;
  void *buffer;
  int bufferlen=filelength(fd);
  buffer=(void *)tmalloc(bufferlen+10);
  ::read(fd,buffer,bufferlen);
  close(fd);
  int where=0,n;
  if (!copy(n,buffer,where,bufferlen)) return false;
	for (int I=0; I<n; I++)
	{
		set <string> tobjects;
		if (!copy(tobjects,buffer,where,bufferlen))
	      ::lplog(LOG_FATAL_ERROR,L"Buffer overrun encountered in read buffer at location %d (limit=%d).",where,bufferlen);
		objects.push_back(tobjects);
	}
  tfree(bufferlen,buffer);
	return true;
}

bool readHyperNymCache(wstring &in,vector < vector <string> > &objects)
{ LFS
	if (in==L"con") in=L"_con_"; // prevent Windows redirection
	wstring path=wstring(CACHEDIR)+L"\\wordNetCache\\orderedHyperNyms_"+in;
  IOHANDLE fd=_wopen(path.c_str(),O_RDWR|O_BINARY);
  if (fd<0) return false;
  void *buffer;
  int bufferlen=filelength(fd);
  buffer=(void *)tmalloc(bufferlen+10);
  ::read(fd,buffer,bufferlen);
  close(fd);
  int where=0,n;
  if (!copy(n,buffer,where,bufferlen)) return false;
	objects.reserve(n);
	for (int I=0; I<n; I++)
	{
		vector <string> tobjects;
		if (!copy(tobjects,buffer,where,bufferlen))
	      ::lplog(LOG_FATAL_ERROR,L"Buffer overrun encountered in read buffer at location %d (limit=%d).",where,bufferlen);
		objects.push_back(tobjects);
	}
  tfree(bufferlen,buffer);
	return true;
}

#define MAX_BUF 102400
bool writeHyperNymCache(wstring &in,vector < set <string> > &objects)
{ LFS
	if (in==L"con") in=L"_con_"; // prevent Windows redirection
	wstring path=wstring(CACHEDIR)+L"\\wordNetCache\\"+in;
  int fd=_wopen(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD | _S_IWRITE),where=0;
  if (fd<0) return false;
  char buffer[MAX_BUF];
  if (!copy(buffer,(int)objects.size(),where,MAX_BUF)) return false;
	for (unsigned int I=0; I<objects.size(); I++)
		if (!copy(buffer,objects[I],where,MAX_BUF)) return false;
  write(fd,buffer,where);
  close(fd);
	return true;
}

bool writeHyperNymCache(wstring &in,vector < vector <string> > &objects)
{ LFS
	if (in==L"con") in=L"_con_"; // prevent Windows redirection
	wstring path=wstring(CACHEDIR)+L"\\wordNetCache\\orderedHyperNyms_"+in;
  int fd=_wopen(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD | _S_IWRITE),where=0;
  if (fd<0) return false;
  char buffer[MAX_BUF];
  if (!copy(buffer,(int)objects.size(),where,MAX_BUF)) return false;
	for (unsigned int I=0; I<objects.size(); I++)
		if (!copy(buffer,objects[I],where,MAX_BUF)) return false;
  write(fd,buffer,where);
  close(fd);
	return true;
}

// L"communication" includes such things as screaming, etc.
// L"measure" includes milkshake and other things which are measured
// L"attribute" includes all diseases
// L"relation" includes all familial relations (husband)
// L"group" includes all groups like bikers, rockers, skinheads
// L"set" includes tormentor and radio
void analyzeNounClass(int where,int fromWhere,wstring in,int inflectionFlags,bool &measurableObject,bool &notMeasurableObject,bool &grouping,sTrace &t,wstring &lastNounNotFound,wstring &lastVerbNotFound)
{ LFS
	initWordNet();
	deriveMainEntry(where,fromWhere,in,inflectionFlags,false,true,lastNounNotFound,lastVerbNotFound);
	vector < set <string> > objects;
	if (!readHyperNymCache(in,objects))
	{
	  SynsetPtr sp;
		if (!initHyperNym(in,sp)) return;
		getHyperNyms(sp,objects,true,false);
		writeHyperNymCache(in,objects);
	}
	if (inEveryGroup("physical_object",objects) || inEveryGroup("physical_entity",objects))
		measurableObject=true;
	if ((inEveryGroup("psychological_feature",objects) && !inEveryGroup("cognitive_state",objects)) || inEveryGroup("abstraction",objects))
	{
		notMeasurableObject=true;
		measurableObject=false;
	}
	else 
	{
		bool found=true;
		for (vector < set <string> >::iterator oi=objects.begin(),oiEnd=objects.end(); oi!=oiEnd; oi++)
			if (oi->find("process")==oi->end() && oi->find("abstraction")==oi->end())
				found=false;
		if (found)
		{
			measurableObject=false;
			notMeasurableObject=true;
			if (t.traceSpeakerResolution)
				lplog(LOG_RESOLUTION,L"mcs %s: physical=%s notPhysical=%s",
					in.c_str(),(measurableObject) ? L"true":L"false",(notMeasurableObject) ? L"true":L"false");
		}
	}
	grouping=inEveryGroup("grouping",objects);
/*
	int initialFamiliarity=-1,highestFamiliarity=-1,numSense=0,wnClass=VERB;
  wstring synonym,cdstr,coordFamiliarity,tmp;
	set <wstring> synonyms,objects;
  SynsetPtr sp=NULL;
	if (index) 
	{
		initialFamiliarity=highestFamiliarity=index->sense_cnt;
	}
	int highestCoordFamiliarity=initialFamiliarity;
	getMostCommonSynonym(in,synonym,false,true,false,false,sp,index,synonyms,initialFamiliarity,highestFamiliarity,t);
	if (synonyms.size())
	{
		int kindFamiliarity;
		wstring kindOf=getIsKindOf(in,kindFamiliarity);
		if (kindFamiliarity>highestFamiliarity)
		{
			synonym=kindOf;
			highestFamiliarity=kindFamiliarity;
			//lplog(LOG_RESOLUTION,L"%s is a kind of %s.",in.c_str(),synonym.c_str());
		}
	}
*/
	if (!measurableObject && !notMeasurableObject && t.traceSpeakerResolution)
		lplog(LOG_WORDNET,L"%d:mcs %s: physical=%s notPhysical=%s",where,
				in.c_str(),(measurableObject) ? L"true":L"false",(notMeasurableObject) ? L"true":L"false");
}

void getAllHyperNyms(wstring in,vector < set <string> > &kindOfObjects)
{ LFS
	initWordNet();
	if (!readHyperNymCache(in,kindOfObjects))
	{
	  SynsetPtr sp;
		if (initHyperNym(in,sp)) 
			getHyperNyms(sp,kindOfObjects,true,false);
		writeHyperNymCache(in,kindOfObjects);
	}
}

void getAllOrderedHyperNyms(wstring in,vector < vector <string> > &kindOfObjects)
{ LFS
	if (cacheOrderedHyperNyms)
	{
		AcquireSRWLockShared(&orderedHyperNymsMapSRWLock);
		unordered_map<wstring, int >::iterator ohnmi;
	  if ((ohnmi=orderedHyperNymsNumMap.find(in))==orderedHyperNymsNumMap.end())
			orderedHyperNymsNumMap[in]=1;
		else
		{
			(*ohnmi).second++;
			kindOfObjects=orderedHyperNymsMap[in];
			ReleaseSRWLockShared(&orderedHyperNymsMapSRWLock);
			return;
		}
		ReleaseSRWLockShared(&orderedHyperNymsMapSRWLock);
	}
	initWordNet();
	if (!readHyperNymCache(in,kindOfObjects))
	{
	  SynsetPtr sp;
		if (initHyperNym(in,sp)) 
			getOrderedHyperNyms(sp,kindOfObjects,true,false);
		writeHyperNymCache(in,kindOfObjects);
	}
	if (cacheOrderedHyperNyms)
	{
		AcquireSRWLockExclusive(&orderedHyperNymsMapSRWLock);
		orderedHyperNymsMap[in]=kindOfObjects;
		ReleaseSRWLockExclusive(&orderedHyperNymsMapSRWLock);
	}
}

bool inWordNetClass(int where,wstring in,int inflectionFlags,string group,wstring &lastNounNotFound,wstring &lastVerbNotFound)
{ LFS
	initWordNet();
	deriveMainEntry(where,35,in,inflectionFlags,false,true,lastNounNotFound,lastVerbNotFound);
	vector < set <string> > objects;
	getAllHyperNyms(in,objects);
	for (vector < set <string> >::iterator oi=objects.begin(),oiEnd=objects.end(); oi!=oiEnd; oi++)
		if (oi->find(group)!=oi->end())
			return true;
	return false;
}

void analyzeVerbNetClass(int where,wstring in,wstring &proposedSubstitute,int &numIrregular,int inflectionFlags,sTrace &t,wstring &lastNounNotFound,wstring &lastVerbNotFound)
{ LFS
	initWordNet();
	deriveMainEntry(where,37,in,inflectionFlags,true,false,lastNounNotFound,lastVerbNotFound);
  SynsetPtr sp=NULL;
	set <wstring> synonyms,objects;
	int initialFamiliarity=-1,highestFamiliarity=-1,numSense=0,wnClass=VERB;
  wstring synonym,cdstr,coordFamiliarity,tmp;
	string inStr;
	IndexPtr index=index_lookup(wTM(in,inStr), wnClass);
	if (index) 
	{
		initialFamiliarity=highestFamiliarity=index->sense_cnt;
		numSense=index->off_cnt;
	}
	unordered_map <wstring, set<int> >::iterator inlvtoCi=vbNetVerbToClassMap.find(in);
	if (inlvtoCi!=vbNetVerbToClassMap.end())
	{
		numVbNetClassFound++;
	}
	else
	{
		int highestCoordFamiliarity=initialFamiliarity;
		bool proposedSubstitution=false,oneSenseVbNetClassFound=false,multiSenseVbNetClassFound=false;
		getMostCommonSynonym(in,synonym,false,true,false,false,sp,index,synonyms,initialFamiliarity,highestFamiliarity,t);
		if (numSense==1)
		{
			if (synonym!=in) 
			{
				oneSenseVbNetClassFound|=(inlvtoCi=vbNetVerbToClassMap.find(synonym))!=vbNetVerbToClassMap.end();
				if (inlvtoCi!=vbNetVerbToClassMap.end())
				{
					vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(),inlvtoCi->second.end());
					//lplog(LOG_TIME,L"mapped %s (2)",in.c_str());
				}
			}
			if (!oneSenseVbNetClassFound)
				for (set <wstring>::iterator si=synonyms.begin(),siEnd=synonyms.end(); si!=siEnd; si++)
					if ((inlvtoCi=vbNetVerbToClassMap.find(*si))!=vbNetVerbToClassMap.end())
					{
						oneSenseVbNetClassFound=true;
						synonym=*si;
						vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(),inlvtoCi->second.end());
						//lplog(LOG_TIME,L"mapped %s (5)",in.c_str());
					}
			if (!oneSenseVbNetClassFound)
			{
				addOneSenseCoords((wchar_t *)in.c_str(),objects,wnClass,numSense);
				scanCoordObjects(in,cdstr,objects,wnClass,highestCoordFamiliarity,coordFamiliarity,proposedSubstitution,oneSenseVbNetClassFound);
			}
		}
		vector <wstring> multiSenseMatchingSynonyms;
		if (numSense>1)
		{
			if (synonym!=in) 
			{
				multiSenseVbNetClassFound|=(inlvtoCi=vbNetVerbToClassMap.find(synonym))!=vbNetVerbToClassMap.end();
				if (inlvtoCi!=vbNetVerbToClassMap.end())
				{
					vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(),inlvtoCi->second.end());
					//lplog(LOG_TIME,L"mapped %s (3)",in.c_str());
				}
			}
			if (!multiSenseVbNetClassFound)
			{
				if (synonyms.empty())
				{
					addOneSenseCoords((wchar_t *)in.c_str(),objects,wnClass,numSense);
					scanCoordObjects(in,cdstr,objects,wnClass,highestCoordFamiliarity,coordFamiliarity,proposedSubstitution,multiSenseVbNetClassFound);
				}
				else
					for (set <wstring>::iterator si=synonyms.begin(),siEnd=synonyms.end(); si!=siEnd; si++)
						if ((inlvtoCi=vbNetVerbToClassMap.find(*si))!=vbNetVerbToClassMap.end())
						{
							multiSenseMatchingSynonyms.push_back(*si);
							vbNetVerbToClassMap[in].insert(inlvtoCi->second.begin(),inlvtoCi->second.end());
							//lplog(LOG_TIME,L"mapped %s (4)",in.c_str());
							multiSenseVbNetClassFound=true;
						}
			}
			else
				multiSenseMatchingSynonyms.push_back(synonym);
		}
		proposedSubstitution=synonym!=in;
		wstring inName,outName;
		if (oneSenseVbNetClassFound)
			numOneSenseVbNetClassFound++;
		if (multiSenseVbNetClassFound)
			numMultiSenseVbNetClassFound++;
		if (!oneSenseVbNetClassFound && !multiSenseVbNetClassFound)
		{
			wstring sFlags,multiSenseSynonym;
			inflectionFlagsToStr(inflectionFlags&VERB_INFLECTIONS_MASK,sFlags);
			for (set <wstring>::iterator si=synonyms.begin(),siEnd=synonyms.end(); si!=siEnd; si++)
				multiSenseSynonym+=L" "+*si;
			if (t.traceSpeakerResolution)
			{
			if (inflectionFlags&VERB_PRESENT_FIRST_SINGULAR)
				lplog(LOG_WORDNET,L"%d:%s vbNet class not found [mainEntry %s] %s - %s.",1+numVbNetClassMultiSenseNotFound++,in.c_str(),
							(Words.gquery(in)->second.mainEntry!=wNULL) ? Words.gquery(in)->second.mainEntry->first.c_str() : L"",
							((inflectionFlags&VERB_INFLECTIONS_MASK)==VERB_PRESENT_FIRST_SINGULAR) ? L"" : sFlags.c_str(),multiSenseSynonym.c_str());
			else
				lplog(LOG_WORDNET,L"%d:%s irregular class not found",1+numIrregular++,in.c_str());
			}
		}
	}
	if (in!=synonym)
		proposedSubstitute=synonym;	
	else if (in!=coordFamiliarity)
		proposedSubstitute=coordFamiliarity;
}

int Source::initializeNounVerbMapping(void)
{ LFS
	wchar_t *path=L"source\\lists\\nounVerbMapping";
	initWordNet();
	int nvfd=_wopen(path,O_RDWR|O_BINARY);
	if (nvfd>=0)
	{
		int bufferlen=filelength(nvfd),where=0;
		void *buffer=(void *)tmalloc(bufferlen+10);
		::read(nvfd,buffer,bufferlen);
		close(nvfd);
		int numMappings;
		if (!copy(numMappings,buffer,where,bufferlen)) return -1;
		for (int I=0; I<numMappings; I++)
		{
			wstring noun;
			set <wstring> verbs;
			if (!copy(noun,buffer,where,bufferlen)) return -1;
			if (!copy(verbs,buffer,where,bufferlen)) return -1;
			nounVerbMap[noun]=verbs;
		}
		tfree(bufferlen,buffer);
		return 0;
	}
	int readWikiNominalizations(MYSQL &mysql, unordered_map <wstring,set < wstring > > &agentiveNominalizations);
	readWikiNominalizations(mysql,nounVerbMap);

	char noun[1024];
	FILE *nounfp=fopen("F:\\Program Files (x86)\\WordNet\\2.1\\dict\\index.noun","r");
	while (fgets(noun,1024,nounfp))
	{
		char *ch=strchr(noun,' ');
		if (ch) *ch=0;
		IndexPtr idx = NULL;
		int offsetcnt=0;
		unsigned long senseOffsets[MAXSENSE];

		for (int i = 0; i < MAXSENSE; i++)
			senseOffsets[i] = 0;

		char *cnoun=noun;
		set <wstring> verbs;
		while ((idx = getindex(cnoun, NOUN)) != NULL) 
		{
			cnoun=NULL;
			/* Go through all of the searchword's senses in the database and perform the search requested. */
			for (int sense = 0; sense < idx->off_cnt; sense++) 
			{
				int skipit=0;
				/* Determine if this synset has already been done with a different spelling. If so, skip it. */
				for (int i = 0; i < offsetcnt && !skipit; i++) 
				{
					if (senseOffsets[i] == idx->offset[sense])
						skipit = 1;
				}
				if (skipit != 1) 
				{
					senseOffsets[offsetcnt++] = idx->offset[sense];
					SynsetPtr cursyn = read_synset(NOUN, idx->offset[sense], idx->wd);
					for (int i = 0; i < cursyn->ptrcount; i++) 
					{
						if ((cursyn->ptrtyp[i] == DERIVATION) && (cursyn->pfrm[i] == cursyn->whichword) && cursyn->ppos[i]==VERB) 
						{
							SynsetPtr cursyn2 = read_synset(cursyn->ppos[i], cursyn->ptroff[i], "");
							wstring tsynw;
							verbs.insert(mTW(cursyn2->words[cursyn->pto[i]-1],tsynw));
							free_synset(cursyn2);
						}
					}
					free_synset(cursyn);
				} /* end if (skipit) */
			} /* end for (sense) */
			free_index(idx);
		} /* end while (idx) */
		if (!verbs.empty())
		{
			wstring wnoun;
			mTW(noun, wnoun);
			nounVerbMap[wnoun]=verbs;
		}
	}
	fclose(nounfp);
	int where=0,nounVerbMapSize=nounVerbMap.size();
	nvfd=_wopen(path,O_CREAT|O_RDWR|O_BINARY,_S_IREAD | _S_IWRITE);
	if (nvfd<0) 
	{
		lplog(LOG_ERROR,L"ERROR:Unable to open %s - %S. (1)",path,_sys_errlist[errno]);
		return false;
	}
	char buffer[MAX_BUF];
	if (!copy(buffer,nounVerbMapSize,where,MAX_BUF)) return false;
	for (unordered_map <wstring,set < wstring > >::iterator nvi=nounVerbMap.begin(),nviEnd=nounVerbMap.end(); nvi!=nviEnd; nvi++)
	{
		if (!copy(buffer,nvi->first,where,MAX_BUF)) return -1;
		if (!copy(buffer,nvi->second,where,MAX_BUF)) return -1;
		if (where>MAX_BUF-1024)
		{
			if (::write(nvfd,buffer,where)<0) return -1;
			where=0;
		}
	}
	if (where)
		::write(nvfd,buffer,where);
	close(nvfd);
	return 0;
}

