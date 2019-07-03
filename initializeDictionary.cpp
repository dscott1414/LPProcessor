#include <windows.h>
#include "Winhttp.h"
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "word.h"
#include "names.h"
#include "time.h" // clock
#include "wn.h"
#include "profile.h"
unordered_map <int,wstring> levinClassSectionToVerbMap; // initialized
unordered_map <wstring,int> levinVerbToClassSectionMap; // dictionary initialized 
unordered_map <int,wstring> levinClassSectionNames; // dictionary initialized 

tIWMM WordClass::gquery(wstring sWord)
{ LFS
	tIWMM iWord=WMM.find(sWord);
	if (iWord==end())
		lplog(LOG_FATAL_ERROR,L"FATAL ERROR: word %s not found.",sWord.c_str());
	return iWord;
}

tIWMM WordClass::predefineWord(const wchar_t *word,int flags)
{ LFS
  unsigned int iForm=FormsClass::addNewForm(word,word,false);
	handleExtendedParseWords((wchar_t *)word);
	bool added;
	return addNewOrModify(NULL,word,flags,iForm,0,0,L"",-1,added);
}

int WordClass::predefineWords(wchar_t *words[],wstring sForm,wstring shortForm,int flags,bool properNounSubClass)
{ LFS
  unsigned int iForm=FormsClass::addNewForm(sForm,shortForm,false,properNounSubClass);
	for (int I=0; words[I]; I++)
	{
		handleExtendedParseWords(words[I]);
		bool added;
		addNewOrModify(NULL,words[I],flags,iForm,0,0,L"",-1,added);
	}
	return iForm;
}

int WordClass::predefineWords(Inflections words[],wstring sForm,wstring shortName,wstring inflectionsClass,int flags,bool properNounSubClass)
{ LFS
	size_t aForm=FormsClass::createForm(sForm,shortName,true,inflectionsClass,properNounSubClass);
	for (int I=0; words[I].word[0]; I++)
	{
		handleExtendedParseWords(words[I].word);
		bool added;
		addNewOrModify(NULL, words[I].word,flags,aForm,words[I].inflection,0,L"",-1,added);
		wchar_t *quote;
		if (quote = wcschr(words[I].word, L'\''))
		{
			*quote = L'ʼ';
			handleExtendedParseWords(words[I].word);
			addNewOrModify(NULL, words[I].word, flags, aForm, words[I].inflection, 0, L"", -1, added);
			*quote = L'\'';
		}
	}
	return 0;
}

int WordClass::predefineWords(InflectionsRoot words[],wstring sForm,wstring shortName,wstring inflectionsClass,int flags,bool properNounSubClass)
{ LFS
	unsigned int aForm=FormsClass::createForm(sForm,shortName,true,inflectionsClass,properNounSubClass);
	for (int I=0; words[I].word[0]; I++)
	{
		handleExtendedParseWords(words[I].word);
		bool added;
		addNewOrModify(NULL, words[I].word,flags,aForm,words[I].inflection,0,words[I].mainEntry,-1,added);
		// check for possible spaces and add dashes if only one space
		bool containsSpaces=false;
		for (unsigned int J=0; words[I].word[J]; J++)
			if (words[I].word[J]==' ')
			{
				containsSpaces=true;
				words[I].word[J]='-';
			}
		if (containsSpaces)
		{
			handleExtendedParseWords(words[I].word);
			addNewOrModify(NULL, words[I].word,flags,aForm,words[I].inflection,0,words[I].mainEntry,-1,added);
		}
	}
	return 0;
}

int WordClass::predefineVerbsFromFile(wstring sForm,wstring shortName,wchar_t *path,int flags)
{ LFS
	wchar_t *inflectionsClass=L"verb";
	bool properNounSubClass=false;
	FILE *tf=_wfopen(path,L"rb");
	if (!tf)
		lplog(LOG_FATAL_ERROR,L"verb list %s is missing.",path);
	unsigned int aForm=FormsClass::createForm(sForm,shortName,true,inflectionsClass,properNounSubClass);
	while (true)
	{
		wchar_t firstSingular[1024],past[1024],presentParticiple[1024],thirdSingular[1024];
		if (fwscanf(tf,L"%s %s %s %s",firstSingular,past,presentParticiple,thirdSingular)!=4)
			break;
		if (firstSingular[0]==65279) // BOM
			wcscpy(firstSingular,firstSingular+1);
		handleExtendedParseWords(firstSingular);
		handleExtendedParseWords(past);
		handleExtendedParseWords(presentParticiple);
		handleExtendedParseWords(thirdSingular);
		bool added;
		addNewOrModify(NULL, firstSingular,flags,aForm,VERB_PRESENT_FIRST_SINGULAR,0,firstSingular,-1,added);
		addNewOrModify(NULL, past,flags,aForm,VERB_PAST,0,firstSingular,-1,added);
		addNewOrModify(NULL, presentParticiple,flags,aForm,VERB_PRESENT_PARTICIPLE,0,firstSingular,-1,added);
		addNewOrModify(NULL, thirdSingular,flags,aForm,VERB_PRESENT_THIRD_SINGULAR,0,firstSingular,-1,added);
	}
	fclose(tf);
	return 0;
}

bool WordClass::readVerbClasses(void)
{ LFS
	wchar_t *path;
	FILE *tf=_wfopen(path=L"source\\lists\\levinVerbClasses.txt",L"rb");
	if (!tf)
		lplog(LOG_FATAL_ERROR,L"verb class list %s is missing.",path);
	wchar_t line[1024];
	// acclaim : 2.10, 2.13.1, 2.13.2, 2.13.3, 33 
	while (fgetws(line,1023,tf))
	{
		if (wcschr(line,'/')) continue;
		wchar_t *ch=wcschr(line,L':'),*ch2,*period;
		if (!ch) 
		{
			fclose(tf);
			return false;
		}
		ch[-1]=0;
		wstring verb=line;
		ch+=2;
		while (true)
		{
			if (ch2=wcschr(ch,L',')) *ch2=0;
			int classSection=0;
			while (true)
			{
				if (period=wcschr(ch,L'.')) *period=0;
				classSection+=_wtoi(ch)<<24;
				if (!period) break;
				if (period=wcschr(ch=period+1,L'.')) *period=0;
				classSection+=_wtoi(ch)<<16;
				if (!period) break;
				if (period=wcschr(ch=period+1,L'.')) *period=0;
				classSection+=_wtoi(ch)<<8;
				if (!period) break;
				if (period=wcschr(ch=period+1,L'.')) *period=0;
				classSection+=_wtoi(ch);
				if (!period) break;
			}
			levinClassSectionToVerbMap[classSection]=verb;
			levinVerbToClassSectionMap[verb]=classSection;
			if (!ch2) break;
			ch=ch2+2;
		}
	}
	fclose(tf);
	return true;
}

bool WordClass::readVerbClassNames(void)
{ LFS
	wchar_t *path;
	FILE *tf=_wfopen(path=L"source\\lists\\levinVerbCategories.txt",L"rb");
	if (!tf)
		lplog(LOG_FATAL_ERROR,L"verb class category list %s is missing.",path);
	wchar_t line[1024];
	//  01234567890
	//   01.1.2.1  Causative/Inchoative Alternation
	while (fgetws(line,1023,tf))
	{
		if (wcsstr(line,L"//")) continue;
		wchar_t *ch,*period,*cutoff;
		if (ch=wcschr(line,13)) *ch=0;
		if (cutoff=wcschr(line+11,L'\"')) *cutoff=0;
		if (cutoff=wcsstr(line+11,L" verbs")) *cutoff=0;
		wstring verbClassName=line+11;
		line[10]=0;
		ch=line+1;
		int classSection=0;
		while (true)
		{
			if (period=wcschr(ch,L'.')) *period=0;
			classSection+=_wtoi(ch)<<24;
			if (!period) break;
			if (period=wcschr(ch=period+1,L'.')) *period=0;
			classSection+=_wtoi(ch)<<16;
			if (!period) break;
			if (period=wcschr(ch=period+1,L'.')) *period=0;
			classSection+=_wtoi(ch)<<8;
			if (!period) break;
			if (period=wcschr(ch=period+1,L'.')) *period=0;
			classSection+=_wtoi(ch);
			if (!period) break;
		}
		levinClassSectionNames[classSection]=verbClassName;
	}
	fclose(tf);
	return true;
}

int WordClass::addProperNamesFile(wstring path)
{ LFS
	FILE *fp=_wfopen(path.c_str(),L"rb");
	if (!fp) return -1;
	wchar_t line[1024];
	fgetws(line,1024,fp);
	fgetws(line,1024,fp);
	fgetws(line,1024,fp);
	//Rank,Male,Number,Female,Number
	// 9,Edward,7428,Mildred,5800
	while (fgetws(line,1024,fp))
	{
		int rank;
		wchar_t *ch=line;
		while (*ch!=',' && *ch) ch++;
		if (!*ch) continue;
		*ch=0;
		rank=_wtoi(line);
		wchar_t *savech=++ch;
		while (*ch!=',' && *ch) ch++;
		if (!*ch) continue;
		*ch=0;
		//wstring maleName=savech;
		bool added;
		int flags=(query(savech)==end()) ? tFI::queryOnLowerCase : 0;
		addNewOrModify(NULL, savech,flags,PROPER_NOUN_FORM_NUM/*MALE_GENDER+(rank<<8)+year*/,MALE_GENDER|MALE_GENDER_ONLY_CAPITALIZED,0,savech,-1,added);
		savech=++ch;
		while (*ch!=',' && *ch) ch++;
		if (!*ch) continue;
		*ch=0;
		savech=++ch;
		while (*ch!=',' && *ch) ch++;
		if (!*ch) continue;
		*ch=0;
		//wstring femaleName=savech;
		flags=(query(savech)==end()) ? tFI::queryOnLowerCase : 0;
		addNewOrModify(NULL, savech,flags,PROPER_NOUN_FORM_NUM/*FEMALE_GENDER+(rank<<8)+year*/,FEMALE_GENDER|FEMALE_GENDER_ONLY_CAPITALIZED,0,savech,-1,added);
	}
	fclose(fp);
	return 0;
}

#define MAX_BUF 32000

int WordClass::writeWord(tIWMM iWord, void *buffer, int &where, int limit)
{
	LFS
	if (!copy(buffer, iWord->first, where, limit))
		return -1;
	iWord->second.write(buffer,where,limit);
	return 0;
}

int WordClass::readFormsCache(char *buffer,int bufferlen,int &numReadForms)
{ LFS
	int where=0;
	unsigned int numForms;
	if (!copy(numForms,buffer,where,bufferlen)) return -1;
	numReadForms=numForms;
	for (unsigned int iForm=0; iForm<numForms; iForm++)
	{
		wstring name,shortName,inflectionsClass;
		if (!copy(name,buffer,where,bufferlen)) return -1;
		if (!copy(shortName,buffer,where,bufferlen)) return -1;
		if (!copy(inflectionsClass,buffer,where,bufferlen)) return -1;
		short hasInflections,properNounSubClass,isTopLevel,isIgnore,isVerbForm,blockProperNounRecognition,formCheck;
		if (!copy(hasInflections,buffer,where,bufferlen)) return -1;
		if (!copy(properNounSubClass,buffer,where,bufferlen)) return -1;
		if (!copy(isTopLevel,buffer,where,bufferlen)) return -1;
		if (!copy(isIgnore,buffer,where,bufferlen)) return -1;
		if (!copy(isVerbForm,buffer,where,bufferlen)) return -1;
		if (!copy(blockProperNounRecognition,buffer,where,bufferlen)) return -1;
		if (!copy(formCheck,buffer,where,bufferlen)) return -1;
		//lplog(L"%d:hasInflections=%d form=%s shortForm=%s inflectionsClass=%s",where,(int)hasInflections,name.c_str(),shortName.c_str(),inflectionsClass.c_str());
		//int f=FormsClass::createForm(name,shortName,hasInflections!=0,inflectionsClass,properNounSubClass!=0);
		if (FormsClass::findForm(name)<0)
		{
			Forms.push_back(new FormClass(-1,name,shortName,inflectionsClass,hasInflections!=0,properNounSubClass!=0,isTopLevel!=0,isIgnore!=0,isVerbForm!=0,blockProperNounRecognition!=0,formCheck!=0));
			FormsClass::formMap[name]=Forms.size()-1;
		}
	}
	return where;
}

int WordClass::writeFormsCache(int fd)
{ LFS
	int where,numForms=(int)Forms.size(),len=(int)sizeof(numForms);
	::write(fd,&numForms,sizeof(numForms));
	if (logDatabaseDetails)
		lplog(L"%d:writing %d forms.",len,numForms);
	int I=0;
	for (vector <FormClass *>::iterator fc=Forms.begin(),fcend=Forms.end(); fc!=fcend; fc++,I++)
	{
		char buffer[MAX_BUF];
		where=0;
		//lplog(L"%d:%d:wrote form %s.",len,I,(*fc)->name.c_str());
		(*fc)->write(buffer,where,MAX_BUF);
		::write(fd,buffer,where);
		len+=where;
	}
	return len;
}

bool disqualify(wstring sWord)
{ LFS
	if (sWord.length()>MAX_WORD_LENGTH)
		return true;
	if (sWord[sWord.length()-1]==' ')
	{
		lplog(LOG_ERROR,L"Word '%s' has a space at the end... Rejected (4).",sWord.c_str());
		return true;
	}
	if (sWord==L"--" || sWord==L"." || sWord==L"...")
		return false;
	int dashes=0;
	for (unsigned I=0; I<sWord.length(); I++)
		if (WordClass::isDash(sWord[I])) dashes++;
	if (dashes>2) return true;
	if (sWord.find('.')==wstring::npos)
		return false;
	// word may only have periods if it is a real abbreviation X.X.X.X. ...
	// all other abbreviations must have already been introduced through initializeDictionary
	for (const wchar_t *w=sWord.c_str(); *w; w+=2)
	{
		if (*w<0 || !iswalpha(*w))
			return true;
		if (!w[1] || w[1]!='.')
			return true;
	}
	return false;
}

int WordClass::readWords(wstring oPath, int sourceId, bool disqualifyWords)
{
	LFS
	oPath += L".wordCacheFile";
	int fd=_wopen(oPath.c_str(),O_RDONLY|O_BINARY);
	if (fd<0) return -1;
	char *buffer;
	int bufferlen=filelength(fd);
	buffer=(char *)tmalloc(bufferlen+10);
	::read(fd,buffer,bufferlen);
	close(fd);
	int where=0,numReadForms;
	where=readFormsCache(buffer,bufferlen,numReadForms);
	if (where<0) return -1;
	// for preferVerbPresentParticiple contained in the tFI constructor
	if (nounForm==-1) nounForm=FormsClass::gFindForm(L"noun");
	if (adjectiveForm<0) adjectiveForm=FormsClass::gFindForm(L"adjective");
	if (adverbForm<0) adverbForm=FormsClass::gFindForm(L"adverb");
	if (prepositionForm<0) prepositionForm=FormsClass::gFindForm(L"preposition");
	FormsClass::changedForms=false;
	wstring sWord;
	tIWMM iWord=wNULL;
	vector <wstring> mainEntries;
	vector <tIWMM> entries;
	bool rejectionTitleIsPrinted = false;
	//int numPreps=0;
	while (where<bufferlen)
	{
		//int saveWhere=where;
		if (!copy(sWord,buffer,where,bufferlen)) return -1;
		wstring sME;
		if (disqualifyWords && disqualify(sWord))
		{
			if (!rejectionTitleIsPrinted)
			{
				lplog(LOG_ERROR, L"These words from word cache file %s are rejected:", oPath.c_str());
				rejectionTitleIsPrinted = true;
			}
			lplog(LOG_ERROR, L"    %s", sWord.c_str());
			tFI(buffer,where,bufferlen,sME,sourceId); // advance 'where' in buffer
			continue;
		}
		//lplog(LOG_INFO, L"TEMP %s", sWord.c_str());
		if ((iWord=WMM.find(sWord))!=WMM.end())
		{
			if (iWord->second.updateFromDisk(buffer,where,bufferlen,sME))
			{
				entries.push_back(iWord);
				mainEntries.push_back(sME);
				#ifdef LOG_WORD_FLOW
					lplog(L"Updated word %s.",sWord.c_str());
				#endif
        for (unsigned int K=0; K<iWord->second.formsSize(); K++)
        {
          if (iWord->second.forms()[K]>=(unsigned)numReadForms)
            lplog(LOG_FATAL_ERROR,L"Word %s has illegal form #%d (out of max %d)!",sWord.c_str(),iWord->second.forms()[K],numReadForms);
          
        }
			}
			#ifdef LOG_WORD_FLOW
				else
					lplog(L"Rejected word %s.",sWord.c_str());
			#endif
		}
		else
		{
			iWord=WMM.insert(tWFIMap(sWord,tFI(buffer,where,bufferlen,sME,sourceId))).first;
			entries.push_back(iWord);
			mainEntries.push_back(sME);
			handleExtendedParseWords((wchar_t *)sWord.c_str());
      for (unsigned int K=0; K<iWord->second.formsSize(); K++)
      {
        if (iWord->second.forms()[K]>=(unsigned)numReadForms)
          lplog(LOG_FATAL_ERROR,L"Word %s has illegal form #%d (out of max %d)!",sWord.c_str(),iWord->second.forms()[K],numReadForms);
        
      }
		}
	}
	for (unsigned int I=0; I<mainEntries.size(); I++)
		if (mainEntries[I].length())
		{
			iWord=WMM.find(mainEntries[I]);
			if (iWord==end())
			{
				entries[I]->second.flags|=tFI::queryOnAnyAppearance;
				lplog(LOG_ERROR,L"MainEntry %s not found in memory for word %s!",mainEntries[I].c_str(),entries[I]->first.c_str());
			}
			else
			{
				entries[I]->second.mainEntry=iWord;
				if (iWord!=wNULL)
					mainEntryMap[iWord->first].push_back(entries[I]);
			}
		}
	tfree(bufferlen+10,buffer);
	tIWMM w=begin(),wEnd=end();
	for (w=begin(); w!=wEnd; w++)
		if (w->second.mainEntry==wNULL && iswalpha(w->first[0]) && 
				(w->second.query(nounForm)>=0 ||
				 w->second.query(verbForm)>=0 ||
				 w->second.query(adjectiveForm)>=0 ||
				 w->second.query(adverbForm)>=0))
		{
			if (logDetail)
				lplog(LOG_ERROR,L"Word %s has no mainEntry after reading wordcache!",w->first.c_str());
			w->second.flags|=tFI::queryOnAnyAppearance;
			w->second.mainEntry=w;
		}
	return 0;
}

int WordClass::addGenderedNouns(wchar_t *genPath,int defaultInflectionFlags,int wordForm)
{ LFS
	FILE *fgen=_wfopen(genPath,L"rb"); // binary mode reads unicode
	if (!fgen) return -1;
	wchar_t noun[101];
	while (fgetws(noun,100,fgen))
	{
		if (noun[0]==0xFEFF) // detect BOM
			memcpy(noun,noun+1,(wcslen(noun+1)+1)*sizeof(noun[0]));
		if (noun[wcslen(noun)-1]=='\n') noun[wcslen(noun)-1]=0;
		if (noun[wcslen(noun)-1]=='\r') noun[wcslen(noun)-1]=0; // in binary mode, cr/lf is not translated
		bool addWordNetSearch=false,hypo=false;
		wchar_t *preferredSense=NULL,*ch;
		if ((hypo=(ch=wcsstr(noun,L" +HYPO"))!=NULL) || (ch=wcsstr(noun,L" +COORDS")))
		{
			addWordNetSearch=true;
			*ch=0;
			wchar_t *chsense=wcschr(ch+1,'('),*chsense2=wcschr(ch+1,')');
			if (chsense && chsense2)
			{
				*chsense2=0;
				preferredSense=chsense+1;
			}
		}
		while (noun[0] && isspace(noun[wcslen(noun)-1]))
			noun[wcslen(noun)-1]=0;
		handleExtendedParseWords(noun);
		bool added;
		tIWMM word=query(noun);
		if (word==end())
			addNewOrModify(NULL, noun,tFI::queryOnAnyAppearance,wordForm,defaultInflectionFlags,0,noun,-1,added);
		else
		{
			int inflectionFlags=word->second.inflectionFlags;
			if (!(inflectionFlags&(MALE_GENDER|FEMALE_GENDER))) inflectionFlags|=defaultInflectionFlags;
			addNewOrModify(NULL, noun,0,wordForm,inflectionFlags,0,noun,-1,added);
		}
		if (addWordNetSearch)
		{
			vector <tmWS > objects;
			set <string> ignoreCategories;
			int numFirstSense;
			if (hypo)	addHyponyms(noun,objects,preferredSense,ignoreCategories,false);
			else addCoords(noun,objects,NOUN,preferredSense,numFirstSense,ignoreCategories,false);
			for (unsigned int I=0; I<objects.size(); I++)
			{
				// toss multiword professions for now
				if (objects[I].ws.size()==1)
				{
					handleExtendedParseWords((wchar_t *)objects[I].ws[0].c_str());
					word=query(objects[I].ws[0].c_str());
					if (word==end())
						addNewOrModify(NULL, objects[I].ws[0].c_str(),tFI::queryOnAnyAppearance,wordForm,defaultInflectionFlags,0,objects[I].ws[0].c_str(),-1,added);
					else
					{
						int inflectionFlags=word->second.inflectionFlags;
						if (!(inflectionFlags&(MALE_GENDER|FEMALE_GENDER))) inflectionFlags|=defaultInflectionFlags;
						addNewOrModify(NULL, objects[I].ws[0].c_str(),tFI::queryOnAnyAppearance,wordForm,inflectionFlags,0,objects[I].ws[0].c_str(),-1,added);
					}
				}
			}
		}
	}
	fclose(fgen);
	return 0;
}

// every line is a nation followed by a person associated with that nation (noun,adjective)separated by a comma
// ; He is from Afghanistan.  He is an Afghan.  He is Afghani.
int WordClass::addDemonyms(wchar_t *demPath)
{ LFS
	demonymForm=FormsClass::createForm(L"demonym",L"de",true,L"noun",true);
	FILE *fdem=_wfopen(demPath,L"rb"); // binary mode reads unicode
	if (!fdem) 
	{
		lplog(LOG_FATAL_ERROR,L"demonym list file %s not found.",demPath);
		return -1;
	}
	wchar_t demonym[101];
	while (fgetws(demonym,100,fdem))
	{
		if (demonym[0]==0xFEFF) // detect BOM
			memcpy(demonym,demonym+1,(wcslen(demonym+1)+1)*sizeof(demonym[0]));
		if (demonym[wcslen(demonym)-1]=='\n') demonym[wcslen(demonym)-1]=0;
		if (demonym[wcslen(demonym)-1]=='\r') demonym[wcslen(demonym)-1]=0; // in binary mode, cr/lf is not translated
		wcslwr(demonym);
		if (demonym[0]==L';') continue;
		wchar_t *nounDemonym=wcschr(demonym,L',');
		if (!nounDemonym) continue;
		int inflectionFlags=0;
		wchar_t *adjectiveDemonym=wcschr(++nounDemonym,L',');
		if (!adjectiveDemonym) continue;
		*adjectiveDemonym=0;
		wchar_t *kind=wcschr(++adjectiveDemonym,L',');
		bool male=false,female=false,plural=false;
		while (kind) 
		{
			*kind=0;
			kind++;
			switch (*kind)
			{
			case L'm':male=true; break; 
			case L'f':female=true; break; 
			case L'p':plural=true; break; 
			}
			kind=wcschr(kind+1,L',');
		}
		if (male) inflectionFlags=MALE_GENDER; 
		if (female) inflectionFlags=FEMALE_GENDER; 
		if (!female && !male) inflectionFlags|=MALE_GENDER|FEMALE_GENDER;
		inflectionFlags|=(plural) ? PLURAL : SINGULAR;
		int len=wcslen(nounDemonym)-1;
		while (len>1 && nounDemonym[len]==' ') len--;
		if (nounDemonym[len+1]==' ') 
			nounDemonym[len+1]=0;
		len=wcslen(adjectiveDemonym)-1;
		while (len>1 && adjectiveDemonym[len]==' ') len--;
		if (adjectiveDemonym[len+1]==' ') 
			adjectiveDemonym[len+1]=0;
		handleExtendedParseWords(nounDemonym);
		bool added;
		addNewOrModify(NULL, nounDemonym,tFI::queryOnAnyAppearance,demonymForm,inflectionFlags,0,nounDemonym,-1,added);
		addNewOrModify(NULL, nounDemonym,tFI::queryOnAnyAppearance,nounForm,inflectionFlags,0,nounDemonym,-1,added);
		if (wcscmp(nounDemonym,adjectiveDemonym))
		{
			addNewOrModify(NULL, adjectiveDemonym,tFI::queryOnAnyAppearance,demonymForm,MALE_GENDER|FEMALE_GENDER,0,adjectiveDemonym,-1,added);
			addNewOrModify(NULL, adjectiveDemonym,tFI::queryOnAnyAppearance,adjectiveForm,MALE_GENDER|FEMALE_GENDER,0,adjectiveDemonym,-1,added);
			addNewOrModify(NULL, adjectiveDemonym,tFI::queryOnAnyAppearance,nounForm,inflectionFlags|PLURAL&~SINGULAR,0,adjectiveDemonym,-1,added);
		}
		else
			addNewOrModify(NULL, adjectiveDemonym,tFI::queryOnAnyAppearance,adjectiveForm,0,0,adjectiveDemonym,-1,added);
	}
	fclose(fdem);
	return 0;
}

//     bool added;
//    words.push_back(addNewOrModify(mwords[I],tFI::queryOnAnyAppearance,PROPER_NOUN_FORM_NUM,0,0,mwords[I],-1,added));
bool WordClass::addPlaces(wstring pPath,vector <tmWS > &objects)
{ LFS
	FILE *fp=_wfopen(pPath.c_str(),L"rb"); // binary mode reads unicode
	if (!fp) return false;
	int numFirstSense;
	wchar_t place[1024];
	while (fgetws(place,1020,fp))
	{
		if (place[0]==0xFEFF) // detect BOM
			memcpy(place,place+1,(wcslen(place+1)+1)*sizeof(place[0]));
		int len=wcslen(place);
		if (len>=1 && place[0]==L';') continue;
		if (place[len-1]=='\n') place[--len]=0;
		if (place[len-1]=='\r') place[--len]=0; // in binary mode, cr/lf is not translated
		while (place[len-1]==' ') place[--len]=0;
		set <string> ignoreCategories;
		bool addWordNetSearch=false;
		wchar_t *preferredSense=NULL,*ch,*ch2;
		bool hypo=false,print=false;
		if (print=(ch=wcsstr(place,L" +print"))!=NULL)
			*ch=0;
		while (((ch=wcsrchr(place,L'-'))!=NULL || (ch = wcsrchr(place, L'—')) != NULL) && ch!=place)
		{
			string ic;
			ignoreCategories.insert(wTM(ch+1,ic));
			if (print)
				printf("IGNORING: [%lS]\n",ch+1);
			while (ch!=place && *(ch-1)==' ') ch--;
			*ch=0;
		}
		if ((hypo=(ch=wcsstr(place,L" +HYPO"))!=NULL) || (ch=wcsstr(place,L" +COORDS")))
		{
			addWordNetSearch=true;
			*ch=0;
			wchar_t *chsense=wcschr(ch+1,'('),*chsense2=wcschr(ch+1,')');
			if (chsense && chsense2)
			{
				*chsense2=0;
				preferredSense=chsense+1;
			}
		}
		wcslwr(place);
		// if has () other than for WORDNET sense, remove
		ch=wcschr(place,'(');
		ch2=wcschr(place,')');
		if (ch && ch2)
		{
			while (ch>place && ch[-1]>=0 && iswspace(ch[-1])) ch--;
			*ch=0;
		}
		// if has one comma, register name before comma, and content after comma appended
		ch=wcsrchr(place,',');
		if (ch)
			*ch=0;
		vector <wstring> words;
		if (wcschr(place,' ')!=NULL)
		{
			if (addWordNetSearch)
			{
				if (hypo)	addHyponyms(place,objects,preferredSense,ignoreCategories,print);
				else addCoords(place,objects,NOUN,preferredSense,numFirstSense,ignoreCategories,print);
			}
			splitMultiWord(place,words);
			objects.push_back(tmWS(words));
			continue;
		}
		handleExtendedParseWords(place);
		if (ch)
		{
			ch++;
			while (*ch==' ') ch++;
			if (*ch)
			{
				if (addWordNetSearch)
				{
					if (hypo)	addHyponyms(place,objects,preferredSense,ignoreCategories,print);
					else addCoords(place,objects,NOUN,preferredSense,numFirstSense,ignoreCategories,print);
				}
				splitMultiWord(ch,words);
				words.push_back(place);
				objects.push_back(words);
			}
		}
		if (isDash(place[0]))
		{
			for (unsigned int I=0; I<objects.size(); I++)
				if (objects[I].ws[objects[I].ws.size()-1]==place+1)
					objects.erase(objects.begin()+I);
			continue;
		}
		words.clear();
		words.push_back(place);
		if (addWordNetSearch)
		{
			if (hypo)	addHyponyms(place,objects,preferredSense,ignoreCategories,print);
			else addCoords(place,objects,NOUN,preferredSense,numFirstSense,ignoreCategories,print);
		}
		objects.push_back(words);
	}
	fclose(fp);
	return true;
}

void WordClass::addNickNames(wchar_t *filePath)
{ LFS
	FILE *nf=_wfopen(filePath,L"rb"); // binary mode reads unicode
	if (!nf)
		lplog(LOG_FATAL_ERROR,L"ERROR:Unable to open %s.  (2)",filePath);
	wchar_t names[1024];
	int equivalenceClass=0;
	while (fgetws(names,1024,nf))
	{
		if (names[0]==0xFEFF) // detect BOM
			memcpy(names,names+1,(wcslen(names+1)+1)*sizeof(names[0]));
		typedef pair <wstring, int> tNickPair;
		wchar_t seps[] = L" ,", *token;
		for (token = wcstok( names, seps ); token != NULL; token = wcstok( NULL, seps ))
		{
			if (token[wcslen(token)-1]=='\n') token[wcslen(token)-1]=0;
			if (token[wcslen(token)-1]=='\r') token[wcslen(token)-1]=0; // in binary mode, cr/lf is not translated
			if (!token[0]) continue;
			wcslwr(token);
			//lplog(L"Inserted %s with class %d.",token,equivalenceClass);
			nicknameEquivalenceMap.insert(tNickPair(token,equivalenceClass));
		}
		equivalenceClass++;
	}
	fclose(nf);
}

bool WordClass::readWordsOfMultiWordObjects(vector < vector < tmWS > > &multiWordStrings,vector < vector < vector <tIWMM> > > &multiWordObjects)
{ LFS
	for (int pp=0; OCSubTypeStrings[pp]; pp++)
	{
		vector < tmWS > placeWords;
		if (!addPlaces(wstring(L"source\\lists\\places\\")+OCSubTypeStrings[pp]+L".txt",placeWords)) 
			return false;
		multiWordStrings.push_back(placeWords);
		vector < vector <tIWMM> > multiWordObjectsCategory;
		multiWordObjects.push_back(multiWordObjectsCategory);
	}
	return true;
}

// multiWordStrings is read once from txt files in Source initialization.
// vector < tmWS > multiWordStrings;
// after each readWithLock (after each book adds more words)
// the multiWordStrings vector is scanned and any more objects which have all the words defined are moved to the multiWordObjects array,
//   and the entry is removed from the multiWordStrings array
void WordClass::addMultiWordObjects(vector < vector < tmWS > > &multiWordStrings,vector < vector < vector <tIWMM> > > &multiWordObjects)
{ LFS 
	int multiWordObjectType=0,numStringsScanned=0,numStringsEntered=0;
	for (vector < vector < tmWS > >::iterator mwsi=multiWordStrings.begin(),mwsiEnd=multiWordStrings.end(); mwsi!=mwsiEnd; mwsi++,multiWordObjectType++)
		for (vector < tmWS >::iterator mwsii=mwsi->begin(); mwsii!=mwsi->end(); numStringsScanned++,mwsii++)
			if (!mwsii->taken)
			{
				vector <tIWMM> multiWordObject;
				tIWMM w=WMM.end();
				for (vector <wstring>::iterator mwsiii=mwsii->ws.begin(),mwsiiiEnd=mwsii->ws.end(); mwsiii!=mwsiiiEnd; mwsiii++)
					if ((w=query(*mwsiii))!=WMM.end())
						multiWordObject.push_back(w);
					else
						break;
				if (w!=WMM.end())
				{
					multiWordObjects[multiWordObjectType].push_back(multiWordObject);
					for (vector <tIWMM>::iterator wi=multiWordObject.begin(),wiEnd=multiWordObject.end(); wi!=wiEnd; wi++)
					{
						tFI *twi=&(*wi)->second;
						if (twi->query(adjectiveForm)>=0 || twi->query(nounForm)>=0 || twi->query(PROPER_NOUN_FORM_NUM)>=0)
						{
							twi->relatedSubTypes.push_back(multiWordObjectType);
							twi->relatedSubTypeObjects.push_back(multiWordObjects[multiWordObjectType].size()-1);
							if (twi->mainEntry!=wNULL && twi->mainEntry!=Words.end() && twi->mainEntry!=(*wi))
							{
								twi->mainEntry->second.relatedSubTypes.push_back(multiWordObjectType);
								twi->mainEntry->second.relatedSubTypeObjects.push_back(multiWordObjects[multiWordObjectType].size()-1);
							}
						}
					}
					//mwsii=mwsi->erase(mwsii);
					mwsii->taken=true;
					numStringsEntered++;
				}
			}
		//lplog(LOG_RESOLUTION,L"(%d/%d) multiWordObjects entered.",numStringsEntered,numStringsScanned);
}

	wchar_t *roman_numeral[] = {
		L"i",L"ii",L"iii",L"iv",L"v",L"vi",L"vii",L"viii",L"ix",
		L"x",L"xi",L"xii",L"xiii",L"xiv",L"xv",L"xvi",L"xvii",L"xviii",L"xix",
		L"xx",L"xxi",L"xxii",L"xxiii",L"xxiv",L"xxv",L"xxvi",L"xxvii",L"xxviii",L"xxix",
		L"xxx",L"xxxi",L"xxxii",L"xxxiii",L"xxxiv",L"xxxv",L"xxxvi",L"xxxvii",L"xxxviii",L"xxxix",
		L"xl",L"xli",L"xlii",L"xliii",L"xliv",L"xlv",L"xlvi",L"xlvii",L"xlviii",L"xlix",
		L"l",L"li",L"lii",L"liii",L"liv",L"lv",L"lvi",L"lvii",L"lviii",L"lix",
		L"lx",L"lxi",L"lxii",L"lxiii",L"lxiv",L"lxv",L"lxvi",L"lxvii",L"lxviii",L"lxix",
		L"lxx",L"lxxi",L"lxxii",L"lxxiii",L"lxxiv",L"lxxv",L"lxxvi",L"lxxvii",L"lxxviii",L"lxxix",
		NULL};

int WordClass::createWordCategories()
{ LFS
	changedWords=true;
	inCreateDictionaryPhase=true;
	FormsClass::createForm(UNDEFINED_FORM,UNDEFINED_SHORT_FORM,false,L"",false);
	FormsClass::createForm(SECTION_FORM,SECTION_SHORT_FORM,true,L"",false);
	FormsClass::createForm(COMBINATION_FORM,COMBINATION_SHORT_FORM,true,L"",false);
	FormsClass::createForm(PROPER_NOUN_FORM,PROPER_NOUN_SHORT_FORM,false,L"noun",false);
	FormsClass::createForm(NUMBER_FORM,NUMBER_SHORT_FORM,false,L"",false);
	// add special section word
	bool added;
	addNewOrModify(NULL,wstring(L"|||"),tFI::topLevelSeparator,SECTION_FORM_NUM,0,0,L"",-1,added);
	sectionWord=WMM.begin();
	Inflections honorific[] = {{L"mister",MALE_GENDER},{L"missus",FEMALE_GENDER},{L"miss",FEMALE_GENDER},
		{L"monsieur",MALE_GENDER},{L"madame",FEMALE_GENDER},{L"mademoiselle",FEMALE_GENDER},{L"sir",MALE_GENDER},{L"ma'am",FEMALE_GENDER},{L"lord",MALE_GENDER},
		{L"lady",FEMALE_GENDER},{L"m'm",FEMALE_GENDER},{NULL,0}};
	predefineWords(honorific,L"honorific",L"hon",L"noun",tFI::queryOnAnyAppearance,false);
	Forms[Forms.size()-1]->blockProperNounRecognition=true; // any words having this form will never be considered as proper nouns
	// honorifics that can resolve like gendered nouns without a previous introduction of a character
	//   that has this as part of his/her name.  This is an honorific that is not a title or occupation (like deacon)
	//   these honorifics can still be used similarly to occupations ('what does the little lady like today?')
	//   but they are not occupations in that they can be resolved to any name which has a gender match and
	//   they do not require an 'introduction' of a character with this honorific in their name (unlike
	//   the honorific 'archdeacon' as in 'the archdeacon' or 'Archdeacon Cowley'.
	//   Previous Introduction Not Required
	Inflections pinr[] = {{L"mister",MALE_GENDER},{L"mr",MALE_GENDER},{L"missus",FEMALE_GENDER},{L"miss",FEMALE_GENDER},
		{L"monsieur",MALE_GENDER},{L"madame",FEMALE_GENDER},{L"mademoiselle",FEMALE_GENDER},{L"sir",MALE_GENDER},{L"ma'am",FEMALE_GENDER},//{L"lord",MALE_GENDER},
		{L"lady",FEMALE_GENDER},{L"m'm",FEMALE_GENDER},{NULL,0}};
	predefineWords(pinr,L"pinr",L"pinr",L"noun",false,false);
	Inflections honorific_abbreviation[] = {{L"mr",MALE_GENDER},{L"mrs",FEMALE_GENDER},{L"dr",MALE_GENDER|FEMALE_GENDER},
		{L"rev",MALE_GENDER|FEMALE_GENDER},{L"sen",MALE_GENDER|FEMALE_GENDER},{L"ms",FEMALE_GENDER},{L"st",MALE_GENDER|FEMALE_GENDER},{L"m",MALE_GENDER},{NULL,0}};
	predefineWords(honorific_abbreviation,L"honorific_abbreviation",L"hon_abb",L"noun",0,false);
	Forms[Forms.size()-1]->blockProperNounRecognition=true; // any words having this form will never be considered as proper nouns
	Inflections honorificFull[] = {{L"mother",FEMALE_GENDER},{L"lady",FEMALE_GENDER},{L"aunt",FEMALE_GENDER},{L"sister",FEMALE_GENDER},
		{L"grandmother",FEMALE_GENDER},{L"duchess",FEMALE_GENDER},{L"queen",FEMALE_GENDER},{L"princess",FEMALE_GENDER},{L"baroness",FEMALE_GENDER},
		{L"mistress",FEMALE_GENDER},{L"marchioness",FEMALE_GENDER},{L"countess",FEMALE_GENDER},{L"viscountess",FEMALE_GENDER},
		{L"father",MALE_GENDER},{L"uncle",MALE_GENDER},{L"grandfather",MALE_GENDER},
		{L"king",MALE_GENDER},{L"duke",MALE_GENDER},{L"prince",MALE_GENDER},{L"baron",MALE_GENDER},{L"marquis",MALE_GENDER},
		{L"earl",MALE_GENDER},{L"viscount",MALE_GENDER},{L"count",MALE_GENDER},{L"cousin",MALE_GENDER|FEMALE_GENDER},
		{L"inspector",MALE_GENDER|FEMALE_GENDER},{NULL,0}};
	predefineWords(honorificFull,L"honorific",L"hon",L"noun",tFI::queryOnAnyAppearance,false);
	Inflections militaryTitle[] = {
				// army/marines/air force officers
				{L"general",MALE_GENDER},{L"lieutenant general",MALE_GENDER},{L"major general",MALE_GENDER},{L"brigadier general",MALE_GENDER},
				{L"colonal",MALE_GENDER},{L"lieutenant colonal",MALE_GENDER},{L"major",MALE_GENDER},{L"captain",MALE_GENDER},
				{L"lieutenant",MALE_GENDER},

				// navy/coast guard officers
				{L"admiral",MALE_GENDER},{L"vice admiral",MALE_GENDER},{L"rear admiral",MALE_GENDER},
				{L"captain",MALE_GENDER},{L"commander",MALE_GENDER},{L"lieutenant commander",MALE_GENDER},{L"ensign",MALE_GENDER},

				// army/marines/navy/coast guard warrant officers
				{L"chief warrant officer",MALE_GENDER},{L"warrant officer",MALE_GENDER},

				// enlisted army
				{L"sargeant major",MALE_GENDER},{L"master sargeant",MALE_GENDER},{L"sargeant",MALE_GENDER},
				{L"staff sargeant",MALE_GENDER},{L"corporal",MALE_GENDER},{L"private",MALE_GENDER},

				// enlisted marines
				{L"gunnery sargeant",MALE_GENDER},{L"lance corporal",MALE_GENDER},

				// enlisted air force
				{L"chief master sargeant",MALE_GENDER},{L"senior master sargeant",MALE_GENDER},{L"master sargeant",MALE_GENDER},{L"technical sargeant",MALE_GENDER},
				{L"senior airman",MALE_GENDER},{L"airman",MALE_GENDER},

				// enlisted navy/coast guard
				{L"master chief petty officer",MALE_GENDER},{L"senior chief petty officer",MALE_GENDER},{L"chief petty officer",MALE_GENDER},
				{L"petty officer",MALE_GENDER},{L"seaman",MALE_GENDER},

				{NULL,0}};
	predefineWords(militaryTitle,L"honorific",L"hon",L"noun",tFI::queryOnAnyAppearance,false);
	Inflections religiousTitle[] = {
				{L"deacon",MALE_GENDER},{L"archdeacon",MALE_GENDER},{L"elder",MALE_GENDER},
				{L"priest",MALE_GENDER},{L"priestess",FEMALE_GENDER},{L"bishop",MALE_GENDER},{L"archbishop",MALE_GENDER},
				{L"minister",MALE_GENDER},{L"reverend",MALE_GENDER},{L"chaplain",MALE_GENDER},
				{L"vicar",MALE_GENDER},{L"canon",MALE_GENDER},{L"warden",MALE_GENDER},
				{L"father",MALE_GENDER},{L"monsignor",MALE_GENDER},{L"monseigneur",MALE_GENDER},
		{L"cardinal",MALE_GENDER},{L"pastor",MALE_GENDER},{L"rector",MALE_GENDER},
				{L"eminence",MALE_GENDER},{L"excellence",MALE_GENDER},{L"pope",MALE_GENDER},{L"emperor",MALE_GENDER},
				{NULL,0}};
	predefineWords(religiousTitle,L"honorific",L"hon",L"noun",tFI::queryOnAnyAppearance,false);
	Inflections governmentTitle[] = {
				{L"president",MALE_GENDER},{L"prime minister",MALE_GENDER},{L"vice president",MALE_GENDER},
				{L"speaker of the house",MALE_GENDER},{L"senator",MALE_GENDER},{L"governor",MALE_GENDER},{L"mayor",MALE_GENDER},
				{L"attorney general",MALE_GENDER},{L"chief of staff",MALE_GENDER},{L"national security advisor",MALE_GENDER},
				{L"chief",MALE_GENDER},{L"chieftess",FEMALE_GENDER},{L"ambassador",MALE_GENDER|FEMALE_GENDER},
				{NULL,0}};
	predefineWords(governmentTitle,L"honorific",L"hon",L"noun",tFI::queryOnAnyAppearance,false);
	Inflections brackets[] = {{L"{",OPEN_INFLECTION},{L"}",CLOSE_INFLECTION},
														{L"(",OPEN_INFLECTION},{L")",CLOSE_INFLECTION},
														{L"<",OPEN_INFLECTION},{L">",CLOSE_INFLECTION},
														{L"[",OPEN_INFLECTION},{L"]",CLOSE_INFLECTION},{NULL,0}};
	predefineWords(brackets,L"brackets",L"brackets",L"brackets");
	// "\"" is mapped to {L"“",OPEN_INFLECTION},{L"”",CLOSE_INFLECTION}
	// "'" is mapped to {L"‘",OPEN_INFLECTION},{L"’",CLOSE_INFLECTION}
	Inflections quotes[] = {{L"\"",0},{L"'",0},{L"`",OPEN_INFLECTION},
													{L"‘",OPEN_INFLECTION},{L"’",CLOSE_INFLECTION},{L"“",OPEN_INFLECTION},{L"”",CLOSE_INFLECTION},{NULL,0}};
	predefineWords(quotes,L"quotes",L"quotes",L"quotes");
	wchar_t *dash[] = {L"-",L"—",L"–",L"--",NULL};                          predefineWords(dash,L"dash",L"dash");
	wchar_t *punctuation[]={L".",L"·",L"...",L";",L":",L"@",L"#",L"$",L"%",L"^",L"&",L"*",L"--",L"+",L"=",L"_",L"|",L"‚",L",",L"/",L"~",L"…",L"│",NULL};
	for (wchar_t **p=punctuation; *p; p++) predefineWord(*p);
	wchar_t *letter[] = {L"a",L"b",L"c",L"d",L"e",L"f",L"g",L"h",L"i",L"j",L"k",L"l",L"m",L"n",L"o",L"p",L"q",L"r",L"s",L"t",
		L"u",L"v",L"w",L"x",L"y",L"z",NULL};
	predefineWords(letter,L"letter",L"let");
	if (letterForm<0) letterForm=FormsClass::gFindForm(L"letter");
	Forms[letterForm]->inflectionsClass=L"noun";
	// abbreviations - see abbreviations note below
	Inflections measurement_abbreviation[] = {
		{L"lbs",PLURAL},{L"mo",SINGULAR},{L"mos",PLURAL},  {L"cm",SINGULAR},{L"bt",SINGULAR},{L"cc",SINGULAR},
		{L"kg",SINGULAR},{L"km",SINGULAR},{L"kw",SINGULAR},
		{L"lb",SINGULAR},{L"ft",SINGULAR|PLURAL},{L"oz",SINGULAR|PLURAL},{L"in",SINGULAR|PLURAL},
		{L"mg",SINGULAR},{L"ml",SINGULAR},{L"mm",SINGULAR},{L"mpg",SINGULAR},{L"mph",SINGULAR},
		{L"oz",SINGULAR},{L"ppm",SINGULAR},{L"rpm",SINGULAR},{L"tbsp",SINGULAR},{L"tsp",SINGULAR},
		{NULL,0}};
	predefineWords(measurement_abbreviation,L"measurement_abbreviation",L"meas_abb",L"noun");
	// abbreviations - see abbreviations note below
	Inflections street_address_abbreviation[] = {
		{L"st",SINGULAR},{L"av",SINGULAR},{L"ave",SINGULAR},{L"dr",SINGULAR},{L"rd",SINGULAR},{L"pk",SINGULAR}, // streets (NewsBank)
		{NULL,0}};
	predefineWords(street_address_abbreviation,L"street_address_abbreviation",L"sa_abb",L"noun",0,true);
	Inflections street_address[] = {
		{L"street",SINGULAR},{L"avenue",SINGULAR},{L"drive",SINGULAR},{L"road",SINGULAR},{L"pike",SINGULAR}, // streets (NewsBank)
		{NULL,0}};
	predefineWords(street_address,L"street_address",L"sa",L"noun",tFI::queryOnAnyAppearance,true);
	Inflections business[] = {{L"incorporated",SINGULAR},{L"limited",SINGULAR},{L"corporation",SINGULAR},{L"company",SINGULAR},{NULL,0}};
	predefineWords(business,L"business",L"b",L"noun",0,true);
	// abbreviations - see abbreviations note below
	Inflections business_abbreviation[] = {{L"inc",SINGULAR},{L"ltd",SINGULAR},{L"corp",SINGULAR},{L"co",SINGULAR},{NULL,0}};
	predefineWords(business_abbreviation,L"business_abbreviation",L"b_abb",L"noun",0,true);
	// abbreviations - see abbreviations note below
	Inflections abbreviation[] = {
		{L"appt",SINGULAR},{L"art",SINGULAR},{L"isbn",SINGULAR},{L"ibid",SINGULAR},{L"ln",SINGULAR},
		{L"i.e",SINGULAR},{L"ie",SINGULAR},{L"etc",SINGULAR},{L"e.g",SINGULAR},{L"eg",SINGULAR},{L"circa",SINGULAR},{L"c",SINGULAR},
		{L"pc",SINGULAR},{L"dna",SINGULAR},{L"rna",SINGULAR},{L"pcb",SINGULAR},{L"pc",SINGULAR},{L"ph",SINGULAR},{L"pcb",SINGULAR},
		{L"no",SINGULAR},{L"ok",SINGULAR},{L"o.k",SINGULAR},{L"etc",SINGULAR},{L"p",SINGULAR},{L"a.b.c",SINGULAR},{L"v.a.d",SINGULAR}, // a.b.c. from Secret Adversary
		{NULL,0}};
	predefineWords(abbreviation,L"abbreviation",L"abb",L"noun",0,true);
	// abbreviations - see abbreviations note below
	Inflections pagenumber[] = {
		{L"pp",SINGULAR},{L"p",SINGULAR},{L"fig",SINGULAR},
		{NULL,0}};
	predefineWords(pagenumber,L"pnum",L"pnum");
	// abbreviations - see abbreviations note below
	Inflections time_abbreviation[] = {{L"p.m",SINGULAR},{L"a.m",SINGULAR},{L"pm",SINGULAR},{L"am",SINGULAR},{NULL,0}};
	predefineWords(time_abbreviation,L"time_abbreviation",L"tabb",L"noun");
	// abbreviations - see abbreviations note below
	Inflections date_abbreviation[] = {{L"a.d",SINGULAR},{L"b.c",SINGULAR},{L"ad",SINGULAR},{L"bc",SINGULAR},{NULL,0}};
	predefineWords(date_abbreviation,L"date_abbreviation",L"dabb",L"noun");
	// abbreviations - no abbreviation can end with a period because
	//   then the period would be hidden from the parser.  Because no period would be seen as a distinct
	//   word by the parser, and if the period would also stand for the end of the sentence, as well as the
	//   end of the abbreviation, no abbreviations can end with a period, but instead
	//   almost all abbreviations have their own _PATTERNS which add an optional period at the end.

	wchar_t *interjection[] = {L"aha",L"my",L"um",L"er",L"ah",L"eh",L"gosh",L"ouch",L"huh",L"aah",L"phoo",L"gee",L"heh",L"eek",L"ahem",L"whoa",L"lordy",L"begad",L"hmm",L"ssh",L"avast",
													L"mm",L"mmm",L"hm",L"good-bye",L"h'm",L"o",L"no",L"yes",L"hush",L"aye",L"bravo",L"nay",L"ach",L"yea",L"yeah",
							L"abudah",L"ahh",L"amen",L"ar",L"aw",L"burmah",L"cor",L"daaah",L"damn",L"doggone",L"ee",L"euch",L"farewell",L"femgliah",L"gah",
							L"gees",L"glug",L"goddammit",L"goddamn",L"golly",L"hah",L"hallelujah",L"hey",L"hic",L"hooray",L"howdy",L"hoy",L"hum",L"hurrah",
							L"jeez",L"mmmm",L"na",L"nah",L"nope",L"och",L"oi",L"oo",L"phew",L"pop",L"pow",L"siah",L"slam",L"ta",L"tara",L"uh",L"waaaaah",
							L"wham",L"whee",L"whoop",L"whoosh",L"yep",L"yo",L"yuk",L"yup",L"shush",L"daah",L"dah",L"naaah",L"naw",L"nope",L"oy",
							L"allo",L"ay",L"bah",L"ciao",L"cripes",L"eureka",L"forsooth",L"goodbye",L"hello",L"hiya",L"hullo",L"hurray",L"oh",
							L"olah",L"ooh",L"oooh",L"pah",L"presto",L"shh",L"shucks",L"umm",L"ya",L"yuck",L"zounds",L"alas",L"ha",L"hurrah",L"hurray",
							L"hoo",L"sorry",L"lord", // L"how" - removed because / Empire State of Mind: How Jay-Z Went from Street Corner to Corner Office 'How' matches __INTRO_N and prevents _RELQ
							NULL};
	predefineWords(interjection,L"interjection",L"inter",tFI::queryOnAnyAppearance,false);
	wchar_t *trademark[] = {L"benzadrine",NULL};
	predefineWords(trademark,L"trademark",L"tm");
	wchar_t *symbol[] = {L"he",NULL};
	predefineWords(symbol,L"symbol",L"sym");
	Inflections pronoun[] = {{L"yonder",SINGULAR|PLURAL|NEUTER_GENDER},{L"there",SINGULAR|PLURAL|NEUTER_GENDER},
		 {L"both",PLURAL|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		 {L"either",SINGULAR|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		 {L"neither",SINGULAR|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},{L"any",SINGULAR|PLURAL|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		 {L"all",SINGULAR|PLURAL|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},{L"another",SINGULAR|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		 {L"other",SINGULAR|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},{L"each",SINGULAR|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		 {L"less",SINGULAR},{L"more",SINGULAR|PLURAL},
		 {L"least",SINGULAR|PLURAL},{L"most",SINGULAR|PLURAL},
		 {L"such",SINGULAR|PLURAL},{NULL,0}};
	predefineWords(pronoun,L"pronoun",L"pn",L"noun",tFI::queryOnAnyAppearance,false);
	wchar_t *determiner[] = {L"the",L"a",L"an",L"th'",L"another",NULL};
	predefineWords(determiner,L"determiner",L"det");
	Forms[Forms.size()-1]->blockProperNounRecognition=true; // any words having this form will never be considered as proper nouns
	Inflections demonstrative_determiner[] = {
				{L"this",SINGULAR|NEUTER_GENDER},{L"that",SINGULAR|NEUTER_GENDER},{L"these",PLURAL|NEUTER_GENDER},{L"those",PLURAL|NEUTER_GENDER},{NULL,0}};
	predefineWords(demonstrative_determiner,L"demonstrative_determiner",L"dem_det",L"noun");
	Inflections possessive_determiner[] = {
		{L"my",SINGULAR_OWNER|FEMALE_GENDER|MALE_GENDER|FIRST_PERSON},{L"our",PLURAL_OWNER|FEMALE_GENDER|MALE_GENDER|FIRST_PERSON|SECOND_PERSON},
		{L"your",SINGULAR_OWNER|PLURAL_OWNER|FEMALE_GENDER|MALE_GENDER|SECOND_PERSON},
		{L"her",SINGULAR_OWNER|FEMALE_GENDER|THIRD_PERSON},{L"his",SINGULAR_OWNER|MALE_GENDER|THIRD_PERSON},
		{L"its",SINGULAR_OWNER|NEUTER_GENDER|THIRD_PERSON},{L"their",PLURAL_OWNER|FEMALE_GENDER|MALE_GENDER|NEUTER_GENDER|THIRD_PERSON},
		//{L"that's",SINGULAR_OWNER|NEUTER_GENDER}, // interferes with that's processing
		{L"thy",SINGULAR|PLURAL|FEMALE_GENDER|MALE_GENDER|SECOND_PERSON},{NULL,0}};
	predefineWords(possessive_determiner,L"possessive_determiner",L"pos_det",L"noun");
	wchar_t *interrogative_determiner[] = {L"what",L"which",L"whose",L"whatever",L"whichever",L"whosoever",L"whoever",L"whomever",L"whereever",L"whenever",NULL};
	predefineWords(interrogative_determiner,L"interrogative_determiner",L"int_det");
	// http://www.wwnorton.com/write/waor.htm
	// Some of the oil has been cleaned up.
	// Some of the problems have been solved.
	Inflections quantifier[] = {{L"all",SINGULAR|PLURAL},{L"some",SINGULAR|PLURAL},{L"much",SINGULAR|PLURAL},{L"many",PLURAL},{L"few",PLURAL},{L"each",SINGULAR},{L"every",PLURAL},{L"several",PLURAL},{NULL,0}};
	predefineWords(quantifier,L"quantifier",L"quant",L"noun",tFI::queryOnAnyAppearance,false);

	Inflections personal_pronoun_nominative[] = {
		{L"one",SINGULAR|MALE_GENDER|FEMALE_GENDER|THIRD_PERSON},
		{L"i",SINGULAR|FIRST_PERSON|MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED},{L"we",PLURAL|FIRST_PERSON|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{NULL,0}};
	predefineWords(personal_pronoun_nominative,L"personal_pronoun_nominative",L"pers_pron_nom",L"noun");
	Inflections personal_pronoun_accusative[] = {
		{L"me",SINGULAR|FIRST_PERSON|MALE_GENDER|FEMALE_GENDER},{L"us",PLURAL|FIRST_PERSON|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{L"him",SINGULAR|MALE_GENDER|THIRD_PERSON},{L"her",SINGULAR|FEMALE_GENDER|THIRD_PERSON},
		{L"them",PLURAL|THIRD_PERSON|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		{L"em",PLURAL|THIRD_PERSON|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		{L"'em",PLURAL|THIRD_PERSON|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},{NULL,0}};
	predefineWords(personal_pronoun_accusative,L"personal_pronoun_accusative",L"pers_pron_acc",L"noun");
	Inflections personal_pronoun[] = {
		{L"you",SINGULAR|PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},{L"ye",SINGULAR|PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{L"ya",SINGULAR|PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{L"yer",SINGULAR|PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},{L"youse",SINGULAR|PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{L"he",SINGULAR|MALE_GENDER|THIRD_PERSON},{L"she",SINGULAR|FEMALE_GENDER|THIRD_PERSON},
		{L"it",SINGULAR|NEUTER_GENDER|THIRD_PERSON},
		{L"they",PLURAL|THIRD_PERSON|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},
		{L"thee",SINGULAR|PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},{L"thees",PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{L"thou",SINGULAR|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},{NULL,0}};
	predefineWords(personal_pronoun,L"personal_pronoun",L"pers_pron",L"noun");
	Inflections reflexive_pronoun[]={
		{L"myself",SINGULAR|FIRST_PERSON},{L"ourselves",PLURAL|FIRST_PERSON|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{L"yourself",SINGULAR|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},{L"yourselves",PLURAL|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},{L"himself",SINGULAR|MALE_GENDER|THIRD_PERSON},
		{L"herself",SINGULAR|FEMALE_GENDER|THIRD_PERSON},{L"itself",SINGULAR|NEUTER_GENDER|THIRD_PERSON},
		{L"themselves",PLURAL|THIRD_PERSON|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},{NULL,0}};
	predefineWords(reflexive_pronoun,L"reflexive_pronoun",L"refl_pron",L"noun");
	Inflections possessive_pronoun[] = {
		{L"mine",SINGULAR|FIRST_PERSON},{L"ours",PLURAL|FIRST_PERSON|SECOND_PERSON|MALE_GENDER|FEMALE_GENDER},
		{L"yours",SINGULAR|PLURAL|SECOND_PERSON},{L"thine",SINGULAR|PLURAL|SECOND_PERSON},
		{L"his",SINGULAR|MALE_GENDER|THIRD_PERSON},{L"hers",SINGULAR|FEMALE_GENDER|THIRD_PERSON}, // {L"it's",SINGULAR|NEUTER_GENDER},
		{L"theirs",PLURAL|THIRD_PERSON|MALE_GENDER|FEMALE_GENDER},{L"everybodies",PLURAL|MALE_GENDER|FEMALE_GENDER},
		{L"somebodies",SINGULAR|MALE_GENDER|FEMALE_GENDER},{L"nobodies",SINGULAR|MALE_GENDER|FEMALE_GENDER},{NULL,0}};
		// removed - interfering with contraction processing no one's could be 'no one is'
		//{L"everyone's",PLURAL},{L"someone's",SINGULAR},{L"no one's",SINGULAR},
	predefineWords(possessive_pronoun,L"possessive_pronoun",L"pos_pron",L"noun",tFI::queryOnAnyAppearance);
	wchar_t *reciprocal_pronoun[]={L"each other",L"one another",NULL};
	predefineWords(reciprocal_pronoun,L"reciprocal_pronoun",L"recip_pron");
	Inflections indefinite_pronoun[]={ // agreement is singular ownership is fuzzy
			{L"everybody",PLURAL|MALE_GENDER|FEMALE_GENDER},{L"everyone",SINGULAR|MALE_GENDER|FEMALE_GENDER},{L"every one",SINGULAR|MALE_GENDER|FEMALE_GENDER},
			{L"people",PLURAL|MALE_GENDER|FEMALE_GENDER},{L"person",SINGULAR|MALE_GENDER|FEMALE_GENDER},
			{L"others",PLURAL|MALE_GENDER|FEMALE_GENDER},{L"other",SINGULAR|MALE_GENDER|FEMALE_GENDER},
			{L"everything",PLURAL|NEUTER_GENDER},{L"somebody",SINGULAR|MALE_GENDER|FEMALE_GENDER},
			{L"someone",SINGULAR|MALE_GENDER|FEMALE_GENDER},{L"something",SINGULAR|NEUTER_GENDER},
			{L"anybody",SINGULAR|MALE_GENDER|FEMALE_GENDER},{L"anyone",SINGULAR|MALE_GENDER|FEMALE_GENDER},
			{L"anything",SINGULAR|NEUTER_GENDER},{L"nobody",SINGULAR|MALE_GENDER|FEMALE_GENDER},
			{L"no one",SINGULAR|MALE_GENDER|FEMALE_GENDER},{L"nothing",SINGULAR|NEUTER_GENDER},
			{L"crowd",PLURAL|MALE_GENDER|FEMALE_GENDER},
			{NULL,0}};
	predefineWords(indefinite_pronoun,L"indefinite_pronoun",L"indef_pron",L"noun",0);
	// 	who, whom, whose, which, that, what, whoever, whomever, whichever, whatever - ,L"wherever",L"whenever"
	wchar_t *interrogative_pronoun[]={L"who",L"whom",L"what",L"which",L"whoever",L"whomever",L"whatever",L"whichever",NULL};
	predefineWords(interrogative_pronoun,L"interrogative_pronoun",L"inter_pron");

	wchar_t *preposition[] = {L"as",L"into",L"against",L"along",L"anigh",L"at",L"atop",L"between",L"betwixt",L"bout",
		L"by",L"circa",L"concerning",L"considering",L"contra",L"cum",L"during",L"enduring",L"fae",L"for",L"forby",
		L"fro",L"in",L"in-between",L"maugre",L"minus",L"next to",L"notwithstanding",L"of",L"de",L"on",L"outen",L"per",L"plus",L"qua",
		L"sans",L"thorough",L"through",L"till",L"times",L"to",L"touching",L"underneath",L"unlike",L"until",L"upon",
		L"via",L"without",
		L"about",L"above",L"across",L"after",L"again",L"alongside",L"around",L"aside",L"aslant",L"astraddle",L"astride",
		L"athwart",L"before",L"behind",L"below",L"beneath",L"beside",L"beyond",L"but",L"ere",L"except",L"fromward",
		L"in between",L"nearby",L"o'er",L"or",L"since",L"than",L"throughout",L"toward",L"towards",
		L"under",L"upward",L"within",
		// these words must be specified as the dictionary lookup gets too many forms (>8)
		L"ere",
		NULL};
	predefineWords(preposition,L"preposition",L"prep",tFI::queryOnAnyAppearance,false);
	wchar_t *verbalPreposition[] = {L"including",NULL};
	predefineWords(verbalPreposition,L"verbalPreposition",L"vprep",tFI::queryOnAnyAppearance,false);
	wchar_t *preposition2[] = {L"with",NULL}; // this is also defined as an adverb and as a noun which are very rare usages
	predefineWords(preposition2,L"preposition",L"prep",0,false);
	Inflections negation_verb_contraction[] = {{L"daresn't",VERB_PRESENT_FIRST_SINGULAR},{L"dasn't",VERB_PRESENT_FIRST_SINGULAR},
			{L"dunno",VERB_PRESENT_FIRST_SINGULAR},{NULL,0}};//{L"oughtn't",VERB_PRESENT_THIRD_SINGULAR},
	//{L"didn't",VERB_PAST},{L"doesn't",VERB_PAST_THIRD_SINGULAR},{L"don't",VERB_PRESENT_FIRST_SINGULAR},{L"don't",VERB_PRESENT_PLURAL},NULL};
	predefineWords(negation_verb_contraction,L"negation_verb_contraction",L"neg_vb_cont",L"verb");

	wchar_t *conjunction[] = {L"and",L"as if",L"but",L"however",L"if",L"lest",L"nor",L"or",L"than",L"unless",L"as",L"before",L"after", 
		L"cos",L"even",L"given",L"immediately",L"insofar",L"whereupon", // BNCC
		L"an'", // short for 'and'
		L"o'", // short for 'of'
		L"whencesoever",L"whenever",L"whensoever",L"whereas",L"wheresoever",L"whiles",L"whilst",L"&",L"wherever", // "--" removed There may be a risk--if I've been followed.
		// these words must be specified as the dictionary lookup gets too many forms (>8)
		L"how",L"ere",
		NULL};
	predefineWords(conjunction,L"conjunction",L"conj",tFI::queryOnAnyAppearance,false);
	wchar_t *modal_auxiliary[]={L"can",L"canst",L"could",L"couldst",L"may",L"mayst",L"might",L"must",L"should",L"shouldst",L"would",L"wouldst",L"wouldhad",L"ought",L"aught",NULL};
	predefineWords(modal_auxiliary,L"modal_auxiliary",L"mod_aux",tFI::queryOnAnyAppearance,false);
	wchar_t *future_modal_auxiliary[]={L"shall",L"will",L"wilt",L"shalt",NULL};
	predefineWords(future_modal_auxiliary,L"future_modal_auxiliary",L"fut_mod_aux",tFI::queryOnAnyAppearance,false);
	wchar_t *negation_modal_auxiliary[]={L"can't",L"mayn't",L"mustn't",L"shouldn't",L"couldn't",L"wouldn't",L"cannot",L"mightn't",L"oughtn't",NULL};
		//"can not",L"may not",L"must not",L"should not",L"could not",L"would not",L"might not",NULL};
	predefineWords(negation_modal_auxiliary,L"negation_modal_auxiliary",L"neg_mod_aux");
	wchar_t *negation_future_modal_auxiliary[]={L"won't",L"shan't",NULL}; // "will not",L"shall not",
	predefineWords(negation_future_modal_auxiliary,L"negation_future_modal_auxiliary",L"neg_fut_mod_aux");

	wchar_t *numeral_cardinal[] = {
		L"zero",L"naught",L"one",L"two",L"three",L"four",L"five",L"six",L"seven",L"eight",L"nine",L"ten",L"eleven",
		L"ones",L"twos",L"threes",L"fours",L"fives",L"sixes",L"sevens",L"eights",L"nines",L"tens",L"elevens",
		L"twelve",L"dozen",L"dozens",L"thirteen",L"fourteen",L"fifteen",L"sixteen",L"seventeen",L"eighteen",L"nineteen",L"umpteen",L"gross",
		L"twenty",L"twenties",L"thirty",L"thirties",L"forty",L"forties",L"fifty",L"fifties",L"sixty",L"sixties",L"seventy",L"seventies",L"eighty",L"eighties",L"ninety",L"nineties",
		L"hundred",L"hundreds",L"thousand",L"thousands",L"million",L"millions",L"billion",L"billions",L"trillion",L"trillions",NULL};
	predefineWords(numeral_cardinal,L"numeral_cardinal",L"card");
	predefineWords(roman_numeral,L"roman_numeral",L"roman");
	wchar_t *numeral_ordinal[] = {L"first",L"second",L"seconds",L"third",L"thirds",L"fourth",L"fourths",L"fifth",L"fifths",
		L"sixth",L"sixths",L"seventh",L"sevenths",L"eighth",L"eighths",L"ninth",L"ninths",L"tenth",L"tenths",
		L"eleventh",L"elevenths",L"twelfth",L"twelveths",L"twelves",L"thirteenth",L"thirteenths",L"fourteenth",L"fourteenths",L"fifteenth",L"fifteenths",
		L"sixteenth",L"sixteenths",L"seventeenth",L"seventeenths",L"eighteenth",L"eighteenths",L"nineteenth",L"nineteenths",
		L"twentieth",L"twentieths",L"thirtieth",L"thirtieths",L"fortieth",L"fortieths",L"fiftieth",L"fiftieths",L"sixtieth",L"sixtieths",L"seventieth",L"seventieths",L"eightieth",L"eightieths",
		L"ninetieth",L"nintieths",L"hundredth",L"hundredths",L"thousandth",L"thousandths",L"millionth",L"millionths",L"billionth",L"billionths",L"trillionth",L"trillionths",
		L"1st",L"2nd",L"3rd",L"4th",L"5th",L"6th",L"7th",L"8th",L"9th",L"10th",
		L"11th",L"12th",L"13th",L"14th",L"15th",L"16th",L"17th",L"18th",L"19th",L"20th",
		L"21st",L"22nd",L"23rd",L"24th",L"25th",L"26th",L"27th",L"28th",L"29th",L"30th",
		L"31st",L"32nd",L"33rd",L"34th",L"35th",L"36th",L"37th",L"38th",L"39th",L"40th",
		L"first",L"last",L"next",L"umpteenth",L"nth",NULL}; // BNC
	predefineWords(numeral_ordinal,L"numeral_ordinal",L"ord");
	wchar_t *inserts[] = {L"ouch",L"right-o",L"good-bye",NULL}; // chapter 14 LGSWE
	predefineWords(inserts,L"inserts",L"ins");
	wchar_t *polite_inserts[] = {L"please",L"pray",NULL}; // chapter 14 LGSWE // "thank you" removed (decreased matches)
	predefineWords(polite_inserts,L"polite_inserts",L"pi",tFI::queryOnAnyAppearance,false);
	wchar_t *predeterminer[] = {L"all",L"half",L"double",L"both",L"once",L"twice",L"thrice",NULL};
	predefineWords(predeterminer,L"predeterminer",L"predeterminer",tFI::queryOnAnyAppearance,false);
	wchar_t *sNoQuery[] = {L"and",L"you",L"p.o.",L"!",L"?",NULL};
	for (wchar_t **s=sNoQuery; *s; s++) predefineWord(*s);
	wchar_t *specials[] = {L"ex",L"there",L"to",L"only",L"but",L"also",L"thank",L"box",L"which",L"what",L"whose",L"how",L"both",L"van",L"von",NULL};
	for (wchar_t **s=specials; *s; s++) predefineWord(*s,tFI::queryOnAnyAppearance);
	// not is registered as a (noun, adjective, adverb and preposition, but it is very rarely anything but an adverb)
	// never is an adverb
	// no is an adverb, adjective or noun (very rare)
	wchar_t *specials3[] = {L"not",L"no",L"never",L"p",L"m",L"le",L"de",L"f",L"c",L"k",L"o",NULL};  //p is the British abbreviation for pence
	for (wchar_t **s=specials3; *s; s++) predefineWord(*s,0);

	// of late, he is not there. // _PP only takes nouns as objects.
	InflectionsRoot noun[] = {{L"desk",SINGULAR,L"desk"},{L"desks",PLURAL,L"desk"},{L"eardrop",SINGULAR,L"eardrop"},{L"eardrops",PLURAL,L"eardrop"},
											{L"as",SINGULAR,L"as"},{L"art",SINGULAR,L"art"},{L"arts",PLURAL,L"art"},
											{L"to-day",SINGULAR,L"to-day"},{L"to-morrow",SINGULAR,L"to-morrow"},{L"to-night",SINGULAR,L"to-night"},
											{L"hanger-on",SINGULAR,L"hanger-on"},{L"hangers-on",PLURAL,L"hanger-on"},{L"man-of-war",SINGULAR,L"man-of-war"},
											{L"in-law",SINGULAR,L"in-law"},{L"in-laws",PLURAL,L"in-law"},
											{L"jack-of-all-trades",SINGULAR,L"jack-of-all-trades"},
											{L"late",SINGULAR,L"late"},{L"time",SINGULAR,L"time"},{L"times",PLURAL,L"time"},{L"for ever",SINGULAR,L"for ever"},
											{L"veronal",SINGULAR,L"veronal"},{L"lookout",SINGULAR,L"lookout"},{L"o'clock",SINGULAR,L"o'clock"},
											// these words must be specified as the dictionary lookup gets too many forms (>8)
											{L"air",SINGULAR,L"air"},{L"airs",PLURAL,L"air"},
											{L"bit",SINGULAR,L"bit"},{L"bits",PLURAL,L"bit"},
											{L"ear",SINGULAR,L"ear"},{L"ears",PLURAL,L"ear"},
											{L"fat",SINGULAR,L"fat"},{L"fats",PLURAL,L"fat"},
											{L"but",SINGULAR,L"but"},{L"buts",PLURAL,L"but"},
											{L"how",SINGULAR,L"how"},{L"hows",PLURAL,L"how"},
											{L"what",SINGULAR,L"what"},{L"whats",PLURAL,L"what"},
																						{L"whereabouts",SINGULAR,L"whereabouts"},
											{NULL,0}};
	predefineWords(noun,L"noun",L"n",L"noun",tFI::queryOnAnyAppearance);
	InflectionsRoot adjective[] = {{L"little",ADJECTIVE_NORMATIVE,L"little"},{L"littler",ADJECTIVE_COMPARATIVE,L"little"},
											{L"littlest",ADJECTIVE_SUPERLATIVE,L"little"},{L"just",ADJECTIVE_NORMATIVE,L"just"},
											{L"yonder",ADJECTIVE_NORMATIVE,L"yonder"},{L"thorough",ADJECTIVE_NORMATIVE,L"thorough"},
											{L"would-be",ADJECTIVE_NORMATIVE,L"would-be"},{L"towards",ADJECTIVE_NORMATIVE,L"towards"},
											{L"art",ADJECTIVE_NORMATIVE,L"art"},{L"slicked-back",ADJECTIVE_NORMATIVE,L"slicked-back"},
											{L"bad",ADJECTIVE_NORMATIVE,L"bad"},{L"worse",ADJECTIVE_COMPARATIVE,L"bad"},{L"worst",ADJECTIVE_SUPERLATIVE,L"bad"},
											{L"good",ADJECTIVE_NORMATIVE,L"good"},{L"better",ADJECTIVE_COMPARATIVE,L"good"},{L"best",ADJECTIVE_SUPERLATIVE,L"good"},
											{L"state-of-the-art",ADJECTIVE_NORMATIVE,L"state-of-the-art"},
											{L"op-ed",ADJECTIVE_NORMATIVE,L"op-ed"},
											// these words must be specified as the dictionary lookup gets too many forms (>8)
											{L"fat",ADJECTIVE_NORMATIVE,L"fat"},
											{L"what",ADJECTIVE_NORMATIVE,L"what"},
											{L"but",ADJECTIVE_NORMATIVE,L"but"},
											{L"next",ADJECTIVE_NORMATIVE,L"next"},
											{NULL,0}};
	predefineWords(adjective,L"adjective",L"adj",L"adjective",tFI::queryOnAnyAppearance);
	InflectionsRoot verb[] = {
							{L"showed",VERB_PAST,L"show"},{L"shown",VERB_PAST_PARTICIPLE,L"show"},{L"showing",VERB_PRESENT_PARTICIPLE,L"show"},
							{L"shows",VERB_PRESENT_THIRD_SINGULAR,L"show"},{L"show",VERB_PRESENT_FIRST_SINGULAR,L"show"},
							{L"cut out",VERB_PAST,L"cut out"},{L"cut out",VERB_PAST_PARTICIPLE,L"cut out"},{L"cutting out",VERB_PRESENT_PARTICIPLE,L"cut out"},
							{L"cuts out",VERB_PRESENT_THIRD_SINGULAR,L"cut out"},{L"cut out",VERB_PRESENT_FIRST_SINGULAR,L"cut out"},
							{L"withdrew",VERB_PAST,L"withdraw"},{L"foresaw",VERB_PAST,L"foresee"},
							{L"withdrawn",VERB_PAST_PARTICIPLE,L"withdraw"},{L"foresaw",VERB_PAST_PARTICIPLE,L"foreseen"},
							{L"misunderstood",VERB_PAST|VERB_PAST_PARTICIPLE,L"misunderstand"},
							{L"dunno",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR,L"dunno"},
							{L"done",VERB_PAST_PARTICIPLE,L"do"},
							{L"lasted",VERB_PAST,L"last"},{L"lasted",VERB_PAST_PARTICIPLE,L"last"},{L"lasting",VERB_PRESENT_PARTICIPLE,L"last"}, // last is also a number - which is marked as nonquery
							{L"lasts",VERB_PRESENT_THIRD_SINGULAR,L"last"},{L"last",VERB_PRESENT_FIRST_SINGULAR,L"last"},
							// the rest are from BNC
							{L"rebuilt",VERB_PAST,L"rebuild"},{L"rebuilt",VERB_PAST_PARTICIPLE,L"rebuild"}, // rebuilt is not displayed as an m-w entry correctly
							{L"capitalise",VERB_PRESENT_FIRST_SINGULAR,L"capitalise"},{L"fax",VERB_PRESENT_FIRST_SINGULAR,L"fax"},{L"fritz",VERB_PRESENT_FIRST_SINGULAR,L"fritz"},
							{L"agonised",VERB_PAST,L"agonise"},{L"individualised",VERB_PAST,L"individualise"},
							{L"overshot",VERB_PAST,L"overshoot"},{L"oversized",VERB_PAST,L"oversize"},{L"oversold",VERB_PAST,L"oversell"},{L"recombined",VERB_PAST,L"recombine"},{L"spasmed",VERB_PAST,L"spasm"},{L"underpaid",VERB_PAST,L"underpay"},{L"unwound",VERB_PAST,L"unwind"},{L"overfed",VERB_PAST,L"overfeed"},
							{L"airbrushes",VERB_PRESENT_THIRD_SINGULAR,L"airbrush"},{L"bringeth",VERB_PRESENT_THIRD_SINGULAR,L"bring"},{L"catalyses",VERB_PRESENT_THIRD_SINGULAR,L"catalyse"},{L"changeth",VERB_PRESENT_THIRD_SINGULAR,L"change"},{L"goeth",VERB_PRESENT_THIRD_SINGULAR,L"go"},
							{L"interwoven",VERB_PAST_PARTICIPLE,L"interweave"},{L"overseen",VERB_PAST_PARTICIPLE,L"oversee"},{L"overgrown",VERB_PAST_PARTICIPLE,L"overgrow"},{L"outdone",VERB_PAST_PARTICIPLE,L"outdo"},
							{L"hypothesising",VERB_PRESENT_PARTICIPLE,L"hypothesise"},{L"labouring",VERB_PRESENT_PARTICIPLE,L"labour"},{L"outsourcing",VERB_PRESENT_PARTICIPLE,L"outsource"},{L"scrutinising",VERB_PRESENT_PARTICIPLE,L"scrutinise"},{L"sleepwalking",VERB_PRESENT_PARTICIPLE,L"sleepwalk"},{L"snorkelling",VERB_PRESENT_PARTICIPLE,L"snorkel"},{L"underlying",VERB_PRESENT_PARTICIPLE,L"underlie"},
							{L"slew",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR,L"slue"},{L"slewing",VERB_PRESENT_PARTICIPLE,L"slue"},{L"slewed",VERB_PAST,L"slue"},
							{L"slough",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_THIRD_SINGULAR,L"slue"},{L"sloughing",VERB_PRESENT_PARTICIPLE,L"slue"},{L"sloughed",VERB_PAST,L"slue"},
							// these words must be specified as the dictionary lookup gets too many forms (>8)
							{L"air",VERB_PRESENT_FIRST_SINGULAR,L"air"},
							{L"bite",VERB_PRESENT_FIRST_SINGULAR,L"bite"},{L"bit",VERB_PAST,L"bite"},
							{L"ear",VERB_PRESENT_FIRST_SINGULAR,L"ear"},
							{L"fat",VERB_PRESENT_FIRST_SINGULAR,L"fat"},
							{L"wot",VERB_PRESENT_FIRST_SINGULAR,L"wot"},
							{NULL,0}};
	predefineWords(verb,L"verb",L"v",L"verb",tFI::queryOnAnyAppearance);
	InflectionsRoot adverb[] = {
				{L"as",ADVERB_NORMATIVE,L"as"},{L"there",ADVERB_NORMATIVE,L"there"},{L"little",ADVERB_NORMATIVE,L"little"},
				{L"just",ADVERB_NORMATIVE,L"just"},{L"rather",ADVERB_NORMATIVE,L"rather"},{L"ruther",ADVERB_NORMATIVE,L"ruther"},
				{L"yonder",ADVERB_NORMATIVE,L"yonder"},{L"for ever",ADVERB_NORMATIVE,L"for ever"},{L"however",ADVERB_NORMATIVE,L"however"},
				{L"bad",ADVERB_NORMATIVE,L"bad"},{L"worse",ADVERB_COMPARATIVE,L"bad"},{L"worst",ADVERB_SUPERLATIVE,L"bad"},
				{L"good",ADVERB_NORMATIVE,L"good"},{L"better",ADVERB_COMPARATIVE,L"good"},{L"best",ADVERB_SUPERLATIVE,L"good"},
				{L"even",ADVERB_NORMATIVE,L"even"},{L"grimly",ADVERB_NORMATIVE,L"grim"}, // not in m-w!
				{L"backwards",ADVERB_NORMATIVE,L"backward"}, // not in m-w!
				{L"afterwards",ADVERB_NORMATIVE,L"afterward"}, // not in m-w!
				// these words must be specified as the dictionary lookup gets too many forms (>8)
				{L"what",ADVERB_NORMATIVE,L"what"},
				{L"but",ADVERB_NORMATIVE,L"but"},
				{L"how",ADVERB_NORMATIVE,L"how"},
				{L"ere",ADVERB_NORMATIVE,L"ere"},
				{L"worldwide",ADVERB_NORMATIVE,L"worldwide"},
				{NULL,0}};
	predefineWords(adverb,L"adverb",L"adv",L"adverb",tFI::queryOnAnyAppearance);
	InflectionsRoot adverb2[] = {{L"this",ADVERB_NORMATIVE,L"this"},{L"that",ADVERB_NORMATIVE,L"that"},{L"next",ADVERB_NORMATIVE,L"next"},{L"first",ADVERB_NORMATIVE,L"first"},{NULL,0}};
	predefineWords(adverb2,L"adverb",L"adv",L"adverb");  // do not allow discovery of this or that because they are also m-w pronouns and adjectives, which are
																										// already included in the patterns as determiners
	InflectionsRoot is[] = {
							{L"am",VERB_PRESENT_FIRST_SINGULAR,L"am"}, // this must be first (mainEntry without queryOnAppearance)
							{L"is",VERB_PRESENT_THIRD_SINGULAR,L"am"},{L"ishas",VERB_PRESENT_THIRD_SINGULAR,L"ishas"},
							{L"ishasdoes",VERB_PRESENT_THIRD_SINGULAR,L"ishasdoes"},
							{L"are",VERB_PRESENT_PLURAL,L"am"},
							{L"art",VERB_PRESENT_PLURAL,L"am"},
							{L"was",VERB_PAST,L"am"},{L"been",VERB_PAST_PARTICIPLE,L"am"},
							{L"were",VERB_PAST_PLURAL,L"am"},
							{L"wert",VERB_PAST_PLURAL,L"am"},
							{NULL,0}};
	predefineWords(is,L"is",L"is",L"verb");
							//{L"is not",VERB_PRESENT_THIRD_SINGULAR},{L"am not",VERB_PRESENT_FIRST_SINGULAR},{L"are not",VERB_PRESENT_PLURAL},
							//{L"was not",VERB_PAST_PARTICIPLE},{L"was not",VERB_PAST},{L"were not",VERB_PAST_PLURAL},
	InflectionsRoot is_negation[] = {
							{L"isn't",VERB_PRESENT_THIRD_SINGULAR,L"am"},{L"ain't",VERB_PRESENT_FIRST_SINGULAR,L"am"},
							{L"amn't",VERB_PRESENT_FIRST_SINGULAR,L"am"},{L"an't",VERB_PRESENT_FIRST_SINGULAR,L"am"},
							{L"is't",VERB_PRESENT_THIRD_SINGULAR,L"am"},
							{L"aren't",VERB_PRESENT_PLURAL,L"am"},{L"wasn't",VERB_PAST,L"am"},{L"wasn't",VERB_PAST_PARTICIPLE,L"am"},
							{L"weren't",VERB_PAST_PLURAL,L"am"},{NULL,0}};
	predefineWords(is_negation,L"is_negation",L"is_neg",L"verb");
	predefineWord(L"be");
	predefineWord(L"being",tFI::queryOnAnyAppearance);
	predefineWord(L"been");
	InflectionsRoot have[] = {
							{L"have",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL,L"have"}, // this must be first (mainEntry without queryOnAppearance)
							{L"had",VERB_PAST,L"have"},{L"wouldhad",VERB_PAST,L"wouldhad"},{L"had",VERB_PAST_PARTICIPLE,L"have"},{L"wouldhad",VERB_PAST_PARTICIPLE,L"wouldhad"},
							{L"having",VERB_PRESENT_PARTICIPLE,L"have"},
							{L"has",VERB_PRESENT_THIRD_SINGULAR,L"have"},{L"hast",VERB_PRESENT_SECOND_SINGULAR,L"have"},
							{L"hath",VERB_PRESENT_THIRD_SINGULAR,L"have"},
							{L"ishas",VERB_PRESENT_THIRD_SINGULAR,L"have"},{L"ishasdoes",VERB_PRESENT_THIRD_SINGULAR,L"have"},{NULL,0}};
	predefineWords(have,L"have",L"have",L"verb");
	InflectionsRoot have_negation[] = {
							{L"haven't",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL,L"have"},// this must be first (mainEntry without queryOnAppearance)
							{L"havent",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL,L"have"},// this must be first (mainEntry without queryOnAppearance)
							{L"hadn't",VERB_PAST,L"haven't"},{L"hain't",VERB_PRESENT_FIRST_SINGULAR,L"have"},
							{L"hain't",VERB_PRESENT_THIRD_SINGULAR,L"have"},
							{L"han't",VERB_PRESENT_FIRST_SINGULAR,L"have"},{L"han't",VERB_PRESENT_THIRD_SINGULAR,L"have"},
							{L"hasn't",VERB_PRESENT_THIRD_SINGULAR,L"have"},
							{L"haven't",VERB_PRESENT_THIRD_SINGULAR,L"have"},
							//{L"have not",VERB_PRESENT_FIRST_SINGULAR},{L"had not",VERB_PRESENT_FIRST_SINGULAR},
							{NULL,0}};
	predefineWords(have_negation,L"have_negation",L"have_neg",L"verb");
	InflectionsRoot does[] = {
							{L"do",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL,L"do"},// this must be first (mainEntry without queryOnAppearance)
							{L"did",VERB_PAST,L"do"},{L"done",VERB_PAST_PARTICIPLE,L"do"},{L"doing",VERB_PRESENT_PARTICIPLE,L"do"},
							{L"does",VERB_PRESENT_THIRD_SINGULAR,L"do"},{L"dost",VERB_PRESENT_SECOND_SINGULAR,L"do"},
							{L"doth",VERB_PRESENT_THIRD_SINGULAR,L"do"},
							{L"ishasdoes",VERB_PRESENT_THIRD_SINGULAR,L"ishasdoes"},{NULL,0}};
	predefineWords(does,L"does",L"does",L"verb");
	InflectionsRoot does_negation[] = {
							{L"don't",VERB_PRESENT_FIRST_SINGULAR|VERB_PRESENT_PLURAL,L"do"},// this must be first (mainEntry without queryOnAppearance)
							{L"didn't",VERB_PAST,L"do"},{L"doesn't",VERB_PRESENT_THIRD_SINGULAR,L"do"},
							{L"doesn't",VERB_PRESENT_THIRD_SINGULAR,L"do"},
							//,{L"did not",VERB_PAST},{L"does not",VERB_PRESENT_THIRD_SINGULAR},{L"do not",VERB_PRESENT_FIRST_SINGULAR},
							{NULL,0}};
	predefineWords(does_negation,L"does_negation",L"does_neg",L"verb");
	// introduces relative clauses
		// relative pronouns - who, which, whose, whom
		// relative adverbs - when, where, why

		// Subject and object pronouns cannot be distinguished by their forms - who, which are used for subject and object pronouns.
		//   You can, however, distinguish them as follows:
		//   If the relative pronoun is followed by a verb, the relative pronoun is a subject pronoun. Subject pronouns must always be used.
		//     the apple which is lying on the table
		//   If the relative pronoun is not followed by a verb (but by a noun or pronoun), the relative pronoun is an object pronoun. Object pronouns can be dropped in defining relative clauses, which are then called Contact Clauses.
		//     the apple (which) George lay on the table
	// who, whom, whose, which, that, what, whoever, whoever, whomever, whichever, whatever
	wchar_t *relativizer[] = {L"who",L"which",L"whom",L"whose", // "that" removed - doesn't necessarily fit with all the other relativizers - doesn't necessarily start a question
																															// whose shoes? which shoes? NOT 'that shoe'
														L"where",L"when",L"why",L"whence",L"whereabouts",L"whereby",L"whereat",L"whensoever",L"wherefore",L"whereon",L"whereto",L"wherewith",
														L"what",L"how",L"ow",L"whither",
												 NULL}; // BNCC
	predefineWords(relativizer,L"relativizer",L"rel");
	// eliminated complementizer 12/14/2006 - what has too many forms and complementizer and startquestion are somewhat redundant
	// replaced complementizer with interrogative_determiner
	//wchar_t *complementizer[] = {L"whoever",L"whichever",L"whomever",L"whereever",L"whenever",L"what",NULL};
	//predefineWords(complementizer,L"complementizer",L"comp");
	// eliminated startquestion - replaced with relativizer 12/14/2006
	//wchar_t *startquestion[] = {L"who",L"which",L"whom",L"whose",L"where",L"when",L"why",L"what",L"how",L"wherein",L"whereof",L"whither",L"whereabouts",NULL};
	//predefineWords(startquestion,L"startquestion",L"sq");
	// help run him out of town!
	// help define the status.
	// dare run three miles!
	// rather run two miles.
	// go steal it, if you want.
	// come show me the house.
	wchar_t *verbal_auxiliary[] = {L"rather",L"ruther",L"dare",L"dares",L"help",L"helps",L"go",L"come",L"need",L"needs",NULL};
	predefineWords(verbal_auxiliary,L"verbal_auxiliary",L"vb_aux",tFI::queryOnAnyAppearance,false);
	wchar_t *past_verbal_auxiliary[] = {L"dared",L"helped",L"needed",NULL};
	predefineWords(past_verbal_auxiliary,L"past_verbal_auxiliary",L"past_vb_aux",tFI::queryOnAnyAppearance,false);
	wchar_t *sectionheader[] = {L"book",L"chapter",L"part",L"prologue",L"epilogue",L"volume",NULL};
	predefineWords(sectionheader,L"sectionheader",L"sh",tFI::queryOnAnyAppearance,false);
	// defined by Daniel Webster
	wchar_t *servicemark[] = {L"automat",L"bake-off",L"college board",L"laundromat",L"planned parenthood",L"soap box derby",L"comsat",L"grammy",L"rolfing",L"super bowl",NULL};
	predefineWords(servicemark,L"service mark",L"sm");
	predefineVerbsFromFile(L"think",L"think",L"source\\lists\\thinksayVerbs.txt",0);
	predefineVerbsFromFile(L"istate",L"istate",L"source\\lists\\internalStateVerbs.txt",0);
	wchar_t *coordinator[] = {L"and",L"or",L"nor",L"an'",L"plus",NULL};  // He and I / David and Jane, etc. ('but' removed) 2/2/2007
	predefineWords(coordinator,L"coordinator",L"coord");
		// you need not define the parameters for me. 2/5/2007
	InflectionsRoot verbverb[] = {
		{L"dare",VERB_PRESENT_FIRST_SINGULAR,L"dare"},{L"dared",VERB_PAST,L"dare"},{L"daring",VERB_PRESENT_PARTICIPLE,L"dare"},{L"dares",VERB_PRESENT_THIRD_SINGULAR,L"dare"},
		{L"rather",VERB_PRESENT_FIRST_SINGULAR,L"rather"},{L"rathered",VERB_PAST,L"rather"},{L"ruther",VERB_PAST,L"rather"},{L"rathering",VERB_PRESENT_PARTICIPLE,L"rather"},{L"rathers",VERB_PRESENT_THIRD_SINGULAR,L"rather"},
		{L"make",VERB_PRESENT_FIRST_SINGULAR,L"make"},{L"made",VERB_PAST,L"make"},{L"making",VERB_PRESENT_PARTICIPLE,L"make"},{L"makes",VERB_PRESENT_THIRD_SINGULAR,L"make"},
		{L"let",VERB_PRESENT_FIRST_SINGULAR,L"let"},{L"let",VERB_PAST,L"let"},{L"letting",VERB_PRESENT_PARTICIPLE,L"let"},{L"lets",VERB_PRESENT_THIRD_SINGULAR,L"let"},
		{L"help",VERB_PRESENT_FIRST_SINGULAR,L"help"},{L"helped",VERB_PAST,L"help"},{L"helping",VERB_PRESENT_PARTICIPLE,L"help"},{L"helps",VERB_PRESENT_THIRD_SINGULAR,L"help"},
		{L"need",VERB_PRESENT_FIRST_SINGULAR,L"need"},{L"needed",VERB_PAST,L"need"},{L"needing",VERB_PRESENT_PARTICIPLE,L"need"},{L"needs",VERB_PRESENT_THIRD_SINGULAR,L"need"},
		// feeling, hearing, seeing, watching, telling, having...
		{L"feel",VERB_PRESENT_FIRST_SINGULAR,L"feel"},{L"felt",VERB_PAST,L"feel"},      {L"feeling",VERB_PRESENT_PARTICIPLE,L"feel"},  {L"feels",VERB_PRESENT_THIRD_SINGULAR,L"feel"},
		{L"hear",VERB_PRESENT_FIRST_SINGULAR,L"hear"},{L"heard",VERB_PAST,L"hear"},     {L"hearing",VERB_PRESENT_PARTICIPLE,L"hear"},  {L"hears",VERB_PRESENT_THIRD_SINGULAR,L"hear"},
		{L"see",VERB_PRESENT_FIRST_SINGULAR,L"see"},{L"saw",VERB_PAST,L"see"},          {L"seeing",VERB_PRESENT_PARTICIPLE,L"see"},    {L"sees",VERB_PRESENT_THIRD_SINGULAR,L"see"},
		{L"watch",VERB_PRESENT_FIRST_SINGULAR,L"watch"},{L"watched",VERB_PAST,L"watch"},{L"watching",VERB_PRESENT_PARTICIPLE,L"watch"},{L"watches",VERB_PRESENT_THIRD_SINGULAR,L"watch"},
		{L"tell",VERB_PRESENT_FIRST_SINGULAR,L"tell"},{L"told",VERB_PAST,L"tell"},      {L"telling",VERB_PRESENT_PARTICIPLE,L"tell"},  {L"tells",VERB_PRESENT_THIRD_SINGULAR,L"tell"},
		{NULL,0}};
	predefineWords(verbverb,L"verbverb",L"verbverb",L"verb",tFI::queryOnAnyAppearance);
	//InflectionsRoot verbverb2[] = {
	 // {L"have",VERB_PRESENT_FIRST_SINGULAR,L"have"},{L"had",VERB_PAST,L"have"},{L"having",VERB_PRESENT_PARTICIPLE,L"have"},{L"has",VERB_PRESENT_THIRD_SINGULAR,L"have"},
	 // {NULL,0}};
	//predefineWords(verbverb2,L"verbverb",L"verbverb",L"verb");
	wchar_t *specials2[] = {L"if",L"then",L"more",L"than",L"so",L"number",L"of",L"as",L"like",NULL};
	for (wchar_t **s=specials2; *s; s++) predefineWord(*s,tFI::queryOnAnyAppearance);

	commonProfessionForm=FormsClass::createForm(L"commonProfession",L"cp",true,L"noun",false);
	moneyForm = FormsClass::createForm(L"money", L"m", true, L"noun", false);
	webAddressForm = FormsClass::createForm(L"webAddress", L"wa", true, L"noun", false);
	addGenderedNouns(L"source\\lists\\commonProfessions.txt", SINGULAR | MALE_GENDER | FEMALE_GENDER, commonProfessionForm);
	addGenderedNouns(L"source\\lists\\commonProfessionsPlural.txt",PLURAL|MALE_GENDER|FEMALE_GENDER,commonProfessionForm); // only exist in plural form [staff]
	addGenderedNouns(L"source\\lists\\commonProfessionsFemale.txt",SINGULAR|FEMALE_GENDER,commonProfessionForm);
	gquery(L"woman")->second.remove(commonProfessionForm);
	gquery(L"hand")->second.remove(commonProfessionForm);
	friendForm=FormsClass::createForm(L"friend",L"fr",true,L"noun",false);
	addGenderedNouns(L"source\\lists\\friends.txt",SINGULAR|MALE_GENDER|FEMALE_GENDER,friendForm);

	// source: http://geography.about.com/library/weekly/aa030900a.htm
	addDemonyms(L"source\\lists\\demonyms.txt");
 
	/* add Proper Noun categories */
	createTimeCategories(false);
	wchar_t *telephoneNumbers[]={L"388-1976",NULL};
	predefineWords(telephoneNumbers,L"telephone_number",L"telenum",0,false);

	// this is placed here to avoid renumbering forms
	inCreateDictionaryPhase=false;
	if (nounForm==-1) nounForm=FormsClass::gFindForm(L"noun");
	addGenderedNouns(L"source\\lists\\singularFemaleGender.txt",SINGULAR|FEMALE_GENDER,nounForm);
	addGenderedNouns(L"source\\lists\\pluralFemaleGender.txt",PLURAL|FEMALE_GENDER,nounForm);
	addGenderedNouns(L"source\\lists\\singularMaleGender.txt",SINGULAR|MALE_GENDER,nounForm);
	addGenderedNouns(L"source\\lists\\pluralMaleGender.txt",PLURAL|MALE_GENDER,nounForm);
	extern wchar_t *stateVerbs[];
	for (unsigned int I=0; stateVerbs[I]; I++)
		gquery(stateVerbs[I])->second.flags|=tFI::stateVerb;
	extern wchar_t *possibleStateVerbs[];
	for (unsigned int I=0; possibleStateVerbs[I]; I++)
		gquery(possibleStateVerbs[I])->second.flags|=tFI::possibleStateVerb;
	wchar_t *nameListPaths[]={
		L"source\\lists\\Names\\Names1990.csv",L"source\\lists\\Names\\Names1980.csv",L"source\\lists\\Names\\Names1970.csv",L"source\\lists\\Names\\Names1960.csv",L"source\\lists\\Names\\Names1950.csv",
		L"source\\lists\\Names\\Names1940.csv",L"source\\lists\\Names\\Names1930.csv",L"source\\lists\\Names\\Names1920.csv",L"source\\lists\\Names\\Names1910.csv",L"source\\lists\\Names\\Names1900.csv",NULL
	};
	for (int nlp=0; nameListPaths[nlp]; nlp++)
		addProperNamesFile(nameListPaths[nlp]);
	{
		// The letters also should not be included as names (proper nouns)
		wchar_t anyLetter[3];
		for (wchar_t l='a'; l<='z'; l++)
		{
			anyLetter[0]=l;
			anyLetter[1]=0;
			tIWMM q=Words.gquery(anyLetter);
			if (q->second.query(PROPER_NOUN_FORM_NUM)>=0)
				q->second.remove(PROPER_NOUN_FORM_NUM);
			anyLetter[1]='.';
			anyLetter[2]=0;
			remove(anyLetter);
					//lplog(L"removed Prop Noun from %c.",letter[0]);
		}
	}
	// a list of words that tend to be prefixes used with dashes.  Dashes are separated out into separate words, so we want these words to stand alone.
	// these are statistically significant words judged from the BNC
	InflectionsRoot dashed_prefixes[] = {
		{L"neo",ADVERB_NORMATIVE,L"neo"},{L"full",ADVERB_NORMATIVE,L"full"},{L"off",ADVERB_NORMATIVE,L"off"},{L"light",ADVERB_NORMATIVE,L"light"},{L"ultra",ADVERB_NORMATIVE,L"ultra"},
		{L"micro",ADVERB_NORMATIVE,L"micro"},{L"sun",ADVERB_NORMATIVE,L"sun"},{L"pseudo",ADVERB_NORMATIVE,L"pseudo"},{L"white",ADVERB_NORMATIVE,L"white"},{L"hard",ADVERB_NORMATIVE,L"hard"},
		{L"air",ADVERB_NORMATIVE,L"air"},{L"back",ADVERB_NORMATIVE,L"back"},{L"un",ADVERB_NORMATIVE,L"un"},{L"black",ADVERB_NORMATIVE,L"black"},{L"much",ADVERB_NORMATIVE,L"much"},
		{L"quasi",ADVERB_NORMATIVE,L"quasi"},{L"counter",ADVERB_NORMATIVE,L"counter"},{L"euro",ADVERB_NORMATIVE,L"euro"},{L"inter",ADVERB_NORMATIVE,L"inter"},
		{L"ever",ADVERB_NORMATIVE,L"ever"},{L"de",ADVERB_NORMATIVE,L"de"},{L"hand",ADVERB_NORMATIVE,L"hand"},{L"single",ADVERB_NORMATIVE,L"single"},{L"co",ADVERB_NORMATIVE,L"co"},
		{L"mini",ADVERB_NORMATIVE,L"mini"},{L"pro",ADVERB_NORMATIVE,L"pro"},{L"in",ADVERB_NORMATIVE,L"in"},{L"multi",ADVERB_NORMATIVE,L"multi"},{L"semi",ADVERB_NORMATIVE,L"semi"},
		{L"sub",ADVERB_NORMATIVE,L"sub"},{L"mid",ADVERB_NORMATIVE,L"mid"},{L"post",ADVERB_NORMATIVE,L"post"},{L"pre",ADVERB_NORMATIVE,L"pre"},{L"ex",ADVERB_NORMATIVE,L"ex"},
		{L"re",ADVERB_NORMATIVE,L"re"},{L"anti",ADVERB_NORMATIVE,L"anti"},{L"non",ADVERB_NORMATIVE,L"non"},{NULL,0}};
	predefineWords(dashed_prefixes,L"adverb",L"adv",L"adverb",tFI::queryOnAnyAppearance);
	predefineWords(dashed_prefixes,L"adjective",L"adj",L"adjective",tFI::queryOnAnyAppearance);
	predefineHolidays();

	wchar_t *particles[]={ L"apart", L"about", L"across", L"along", L"around", L"aside", L"away", L"back", L"by", L"down", 
												 L"forth", L"forward", L"home", L"in", L"off", L"on", L"out", L"over", L"past", L"round", L"through", L"under", L"up",NULL };
	predefineWords(particles,L"particle",L"pa");
	PPN=predefineWord(L"__ppn__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	TELENUM=predefineWord(L"__telenum__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	NUM=predefineWord(L"__num__"); // number used for relations
	DATE=predefineWord(L"__date__"); // date used for relations.
	TIME=predefineWord(L"__time__"); // time used for relations.
	LOCATION=predefineWord(L"__location__"); // location used for relations.
	wchar_t *specials4[] = {L"b",L"his",L"her",L"or",L"eg",L"e.g.",L"the",L"less",L"once",L"twice",NULL};  //b is billion
	for (wchar_t **s=specials4; *s; s++) predefineWord(*s,0);
	// these are words which result in too many forms (>8), have been defined fully and should not be overwritten by caches.
	wchar_t *seal[]={L"air",L"bit",L"but",L"how",L"what",NULL};
	for (unsigned int I=0; seal[I]; I++)
		gquery(seal[I])->second.flags&=~tFI::queryOnAnyAppearance;
	return 0;
}

bool string_compare(const wstring& s1, const wstring& s2)
{ LFS
		return s1 < s2 ? 1 : 0;
}

void WordClass::readUnknownWords(wchar_t *fileName,vector <wstring> &unknownWords)
{ LFS
	int fd=_wopen(fileName,O_RDWR);
	if (fd<0)
		lplog(LOG_FATAL_ERROR,L"FATAL:%s unknown word list does not exist.",fileName);
	unsigned int len=filelength(fd);
	wchar_t *buffer=(wchar_t *)tmalloc(len);
	::read(fd,buffer,len);
	close(fd);
	len/=sizeof(buffer[0]);
	unknownWords.reserve(500000);
	//bool sorted=true,duplicates=false;
	unsigned int lastWord=0;
	while (iswspace(buffer[lastWord])) lastWord++;
	//int wordBeforeThat=-1;
	for (unsigned int I=lastWord; I<len; I++)
		if (buffer[I]==10 || buffer[I]==13)
	{
			buffer[I]=0;
			while (buffer[I+1]>=0 && iswspace(buffer[I+1])) I++;
			//strlwr(buffer+lastWord);
			if (wcslen(buffer+lastWord)<2)
			{
				//wordBeforeThat=lastWord;
				lastWord=I+1;
				continue;
			}
			//if (unknownWords[unknownWords.size()-1]!=buffer+lastWord)
			unknownWords.push_back(buffer+lastWord);
			//else
			//  duplicates=true;
			//if (sorted && unknownWords.size()>1)
			//  sorted=unknownWords[unknownWords.size()-2]<=unknownWords[unknownWords.size()-1];
			//wordBeforeThat=lastWord;
			lastWord=I+1;
	}
	tfree(len,buffer);
	//if (!sorted)
	//  sort(unknownWords.begin(),unknownWords.end(),string_compare);
	//if (!sorted || duplicates)
	//  writeUnknownWords(fileName,unknownWords);
}

void WordClass::writeUnknownWords(wchar_t *fileName,vector <wstring> &unknownWords)
{ LFS
	if (!unknownWords.size()) return;
	wchar_t tmp[1024];
	wsprintf(tmp,L"%s",fileName);
	FILE *words=_wfopen(tmp,(appendToUnknownWordsMode) ? L"ab" : L"wb");
	if (!words)
		lplog(LOG_FATAL_ERROR,L"FATAL:%s Cannot open unknown word list.",tmp);
	for (unsigned int I=0; I<unknownWords.size(); I++)
	{
		if (unknownWords[I][0]>='0' && unknownWords[I][0]<='9') // time, date or number not considered unknown.
			continue;
		// if next word matches except for subtraction of last letter, remove word.
		if (unknownWords[I]==unknownWords[I+1].substr(0,unknownWords[I].length()))
			continue;
		fwprintf(words,L"%s\n",unknownWords[I].c_str());
	}
	fclose(words);
	if (appendToUnknownWordsMode) unknownWords.clear();
}

WordClass::WordClass(void)
{ LFS
	appendToUnknownWordsMode=false;
	tFI::allocated=20000;
	tFI::formsArray=(unsigned int *)tmalloc(tFI::allocated*(sizeof(*tFI::formsArray)));
	tFI::fACount=0;
		tFI::uniqueNewIndex=1; // use to insure every word has a unique index, even though it hasn't been
	minimumLastWordWrittenClockDiff=240;
	changedWords=inCreateDictionaryPhase=false;
	lastReadfromDBTime=-1;
	idToMap=NULL;
	idsAllocated=0;
	disinclinationRecursionCount=0;
}

bool tFI::toLowestCost(int form)
{ LFS
	int hf;
	if ((hf=query(form))>=0)
	{
		int flc=getLowestCost();
		usagePatterns[hf]=usagePatterns[flc];
		usageCosts[hf]=usageCosts[flc];
		return true;
	}
	return false;
}

bool tFI::setCost(int form,int cost)
{ LFS
	int hf;
	if ((hf=query(form))>=0)
	{
		usagePatterns[hf]=255*cost/4;
		usageCosts[hf]=cost;
		return true;
	}
	return false;
}

void WordClass::initializeChangeStateVerbs()
{ LFS
	InflectionsRoot changeState[] = {
		{L"start",VERB_PRESENT_FIRST_SINGULAR,L"start"},			    {L"started",VERB_PAST,L"start"},					{L"starting",VERB_PRESENT_PARTICIPLE,L"start"},					{L"starts",VERB_PRESENT_THIRD_SINGULAR,L"start"},
		{L"begin",VERB_PRESENT_FIRST_SINGULAR,L"begin"},			    {L"began",VERB_PAST,L"begin"},						{L"beginning",VERB_PRESENT_PARTICIPLE,L"begin"},				{L"begins",VERB_PRESENT_THIRD_SINGULAR,L"begin"},
		{L"commence",VERB_PRESENT_FIRST_SINGULAR,L"commence"},		{L"commenced",VERB_PAST,L"commence"},			{L"commencing",VERB_PRESENT_PARTICIPLE,L"commence"},		{L"commences",VERB_PRESENT_THIRD_SINGULAR,L"commence"},
		{L"initiate",VERB_PRESENT_FIRST_SINGULAR,L"initiate"},		{L"initiated",VERB_PAST,L"initiate"},			{L"initiating",VERB_PRESENT_PARTICIPLE,L"initiate"},		{L"initiates",VERB_PRESENT_THIRD_SINGULAR,L"initiate"},
		{L"stop",VERB_PRESENT_FIRST_SINGULAR,L"stop"},						{L"stopped",VERB_PAST,L"stop"},						{L"stopping",VERB_PRESENT_PARTICIPLE,L"stop"},					{L"stops",VERB_PRESENT_THIRD_SINGULAR,L"stop"},
		{L"halt",VERB_PRESENT_FIRST_SINGULAR,L"halt"},						{L"halted",VERB_PAST,L"halt"},						{L"halting",VERB_PRESENT_PARTICIPLE,L"halt"},						{L"halts",VERB_PRESENT_THIRD_SINGULAR,L"halt"},
		{L"conclude",VERB_PRESENT_FIRST_SINGULAR,L"conclude"},		{L"concluded",VERB_PAST,L"conclude"},			{L"concluding",VERB_PRESENT_PARTICIPLE,L"conclude"},		{L"concludes",VERB_PRESENT_THIRD_SINGULAR,L"conclude"},
		{L"discontinue",VERB_PRESENT_FIRST_SINGULAR,L"discontinue"},{L"discontinued",VERB_PAST,L"discontinue"},{L"discontinuing",VERB_PRESENT_PARTICIPLE,L"discontinue"},  {L"discontinues",VERB_PRESENT_THIRD_SINGULAR,L"discontinue"},
		{L"close",VERB_PRESENT_FIRST_SINGULAR,L"close"},					{L"closed",VERB_PAST,L"close"},						{L"closing",VERB_PRESENT_PARTICIPLE,L"close"},					{L"closes",VERB_PRESENT_THIRD_SINGULAR,L"close"},
		{L"cease",VERB_PRESENT_FIRST_SINGULAR,L"cease"},					{L"ceased",VERB_PAST,L"cease"},						{L"ceasing",VERB_PRESENT_PARTICIPLE,L"cease"},					{L"ceases",VERB_PRESENT_THIRD_SINGULAR,L"cease"},
		{L"quit",VERB_PRESENT_FIRST_SINGULAR,L"quit"},						{L"quit",VERB_PAST,L"quit"},							{L"quitting",VERB_PRESENT_PARTICIPLE,L"quit"},					{L"quits",VERB_PRESENT_THIRD_SINGULAR,L"quit"},
		{L"interrupt",VERB_PRESENT_FIRST_SINGULAR,L"interrupt"},	{L"interrupted",VERB_PAST,L"interrupt"},	{L"interrupting",VERB_PRESENT_PARTICIPLE,L"interrupt"},	{L"interrupts",VERB_PRESENT_THIRD_SINGULAR,L"interrupt"},
		{L"suspend",VERB_PRESENT_FIRST_SINGULAR,L"suspend"},			{L"suspended",VERB_PAST,L"suspend"},			{L"suspending",VERB_PRESENT_PARTICIPLE,L"suspend"},			{L"suspends",VERB_PRESENT_THIRD_SINGULAR,L"suspend"},
		{L"pause",VERB_PRESENT_FIRST_SINGULAR,L"pause"},					{L"paused",VERB_PAST,L"pause"},						{L"pausing",VERB_PRESENT_PARTICIPLE,L"pause"},					{L"pauses",VERB_PRESENT_THIRD_SINGULAR,L"pause"},
		{L"finish",VERB_PRESENT_FIRST_SINGULAR,L"finish"},				{L"finished",VERB_PAST,L"finish"},				{L"finishing",VERB_PRESENT_PARTICIPLE,L"finish"},				{L"finishes",VERB_PRESENT_THIRD_SINGULAR,L"finish"},
		{L"end",VERB_PRESENT_FIRST_SINGULAR,L"end"},							{L"ended",VERB_PAST,L"end"},							{L"ending",VERB_PRESENT_PARTICIPLE,L"end"},							{L"ends",VERB_PRESENT_THIRD_SINGULAR,L"end"},
		{L"terminate",VERB_PRESENT_FIRST_SINGULAR,L"terminate"},	{L"terminated",VERB_PAST,L"terminate"},		{L"terminating",VERB_PRESENT_PARTICIPLE,L"terminate"},	{L"terminates",VERB_PRESENT_THIRD_SINGULAR,L"terminate"},
		{L"complete",VERB_PRESENT_FIRST_SINGULAR,L"complete"},		{L"completed",VERB_PAST,L"complete"},			{L"completing",VERB_PRESENT_PARTICIPLE,L"complete"},		{L"completes",VERB_PRESENT_THIRD_SINGULAR,L"complete"},
		{L"conclude",VERB_PRESENT_FIRST_SINGULAR,L"conclude"},		{L"concluded",VERB_PAST,L"conclude"},			{L"concluding",VERB_PRESENT_PARTICIPLE,L"conclude"},		{L"concludes",VERB_PRESENT_THIRD_SINGULAR,L"conclude"},
		{L"resume",VERB_PRESENT_FIRST_SINGULAR,L"resume"},				{L"resumed",VERB_PAST,L"resume"},					{L"resuming",VERB_PRESENT_PARTICIPLE,L"resume"},				{L"resumes",VERB_PRESENT_THIRD_SINGULAR,L"resume"},
		{L"continue",VERB_PRESENT_FIRST_SINGULAR,L"continue"},		{L"continued",VERB_PAST,L"continue"},			{L"continuing",VERB_PRESENT_PARTICIPLE,L"continue"},		{L"continues",VERB_PRESENT_THIRD_SINGULAR,L"continue"},
		{L"recommence",VERB_PRESENT_FIRST_SINGULAR,L"recommence"},{L"recommenced",VERB_PAST,L"recommence"},	{L"recommencing",VERB_PRESENT_PARTICIPLE,L"recommence"},{L"recommences",VERB_PRESENT_THIRD_SINGULAR,L"recommence"},
		{L"renew",VERB_PRESENT_FIRST_SINGULAR,L"renew"},					{L"renewed",VERB_PAST,L"renew"},					{L"renewing",VERB_PRESENT_PARTICIPLE,L"renew"},					{L"renews",VERB_PRESENT_THIRD_SINGULAR,L"renew"},
		{L"restart",VERB_PRESENT_FIRST_SINGULAR,L"restart"},			{L"restarted",VERB_PAST,L"restart"},			{L"restarting",VERB_PRESENT_PARTICIPLE,L"restart"},			{L"restarts",VERB_PRESENT_THIRD_SINGULAR,L"restart"},
		{NULL,0}};
	predefineWords(changeState,L"changeState",L"changeState",L"verb",0);
}

void WordClass::initialize()
{ LFS
	wprintf(L"Initializing dictionary...                  \r");
	addNickNames(L"source\\lists\\maleNicknames.txt");
	addNickNames(L"source\\lists\\femaleNicknames.txt");
	// avoid looking these common forms up...
	if (accForm<0) accForm=FormsClass::gFindForm(L"personal_pronoun_accusative");
	if (adverbForm<0) adverbForm=FormsClass::gFindForm(L"adverb");
	if (adjectiveForm<0) adjectiveForm=FormsClass::gFindForm(L"adjective");
	if (commaForm<0) commaForm=FormsClass::gFindForm(L",");
	if (conjunctionForm<0) conjunctionForm=FormsClass::gFindForm(L"conjunction");
	if (demonstrativeDeterminerForm<0) demonstrativeDeterminerForm=FormsClass::gFindForm(L"demonstrative_determiner");
	if (determinerForm<0) determinerForm=FormsClass::gFindForm(L"determiner");
	if (doesForm<0) doesForm=FormsClass::gFindForm(L"does");
	if (doesNegationForm<0) doesNegationForm=FormsClass::gFindForm(L"does_negation");
	if (honorificForm<0) honorificForm=FormsClass::gFindForm(L"honorific");
	if (indefinitePronounForm<0) indefinitePronounForm=FormsClass::gFindForm(L"indefinite_pronoun");
	if (reciprocalPronounForm<0) reciprocalPronounForm=FormsClass::gFindForm(L"reciprocal_pronoun");
	if (pronounForm<0) pronounForm=FormsClass::gFindForm(L"pronoun");
	if (nomForm<0) nomForm=FormsClass::gFindForm(L"personal_pronoun_nominative");
	if (nounForm==-1) nounForm=FormsClass::gFindForm(L"noun");
	if (numeralCardinalForm<0) numeralCardinalForm=FormsClass::gFindForm(L"numeral_cardinal");
	if (numeralOrdinalForm<0) numeralOrdinalForm=FormsClass::gFindForm(L"numeral_ordinal");
	if (romanNumeralForm<0) romanNumeralForm=FormsClass::gFindForm(L"roman_numeral");
	if (possessiveDeterminerForm<0) possessiveDeterminerForm=FormsClass::gFindForm(L"possessive_determiner");
	if (interrogativeDeterminerForm<0) interrogativeDeterminerForm=FormsClass::gFindForm(L"interrogative_determiner");
	if (possessivePronounForm<0) possessivePronounForm=FormsClass::gFindForm(L"possessive_pronoun");  // mine, ours etc.
	if (periodForm<0) periodForm=FormsClass::gFindForm(L".");
	if (quantifierForm<0) quantifierForm=FormsClass::gFindForm(L"quantifier");
	if (quoteForm<0) quoteForm=FormsClass::gFindForm(L"quotes");
	if (dashForm<0) dashForm=FormsClass::gFindForm(L"dash");
	if (reflexiveForm<0) reflexiveForm=FormsClass::gFindForm(L"reflexive_pronoun"); // myself, himself
	if (verbForm<0) verbForm=FormsClass::gFindForm(L"verb");
	if (thinkForm<0) thinkForm=FormsClass::gFindForm(L"think");
	//if (internalStateForm<0) internalStateForm=FormsClass::gFindForm(L"internalState");
	if (dateForm<0) dateForm=FormsClass::gFindForm(L"date");
	if (timeForm<0) timeForm=FormsClass::gFindForm(L"time");
	if (telephoneNumberForm<0) telephoneNumberForm=FormsClass::gFindForm(L"telephone_number");
	if (coordinatorForm<0) coordinatorForm=FormsClass::gFindForm(L"coordinator");
	if (verbverbForm<0) verbverbForm=FormsClass::gFindForm(L"verbverb");
	if (abbreviationForm<0) abbreviationForm=FormsClass::gFindForm(L"abbreviation");
	if (sa_abbForm<0) sa_abbForm=FormsClass::gFindForm(L"street_address_abbreviation");
	numberForm=NUMBER_FORM_NUM;
	if (beForm<0) beForm=FormsClass::gFindForm(L"be");
	if (haveForm<0) haveForm=FormsClass::gFindForm(L"have");
	if (haveNegationForm<0) haveNegationForm=FormsClass::gFindForm(L"have_negation");
	if (doForm<0) doForm=FormsClass::gFindForm(L"does");
	if (doNegationForm<0) doNegationForm=FormsClass::gFindForm(L"does_negation");
	if (interjectionForm<0) interjectionForm=FormsClass::gFindForm(L"interjection");
	if (personalPronounForm<0) personalPronounForm=FormsClass::gFindForm(L"personal_pronoun");
	if (letterForm<0) letterForm=FormsClass::gFindForm(L"letter");
	if (isForm<0) isForm=FormsClass::gFindForm(L"is");
	if (isNegationForm<0) isNegationForm=FormsClass::gFindForm(L"is_negation");
	if (prepositionForm<0) prepositionForm=FormsClass::gFindForm(L"preposition");
	if (telenumForm<0) telenumForm=FormsClass::gFindForm(L"telephone_number");
	if (bracketForm<0) bracketForm=FormsClass::gFindForm(L"brackets");
	if (toForm<0) toForm=FormsClass::gFindForm(L"to");
	if (futureModalAuxiliaryForm<0) futureModalAuxiliaryForm=FormsClass::gFindForm(L"future_modal_auxiliary");
	if (negationModalAuxiliaryForm<0) negationModalAuxiliaryForm=FormsClass::gFindForm(L"negation_modal_auxiliary");
	if (negationFutureModalAuxiliaryForm<0) negationFutureModalAuxiliaryForm=FormsClass::gFindForm(L"negation_future_modal_auxiliary");
	if (modalAuxiliaryForm<0)  modalAuxiliaryForm=FormsClass::gFindForm(L"modal_auxiliary");
	if (relativizerForm<0)  relativizerForm=FormsClass::gFindForm(L"relativizer");
	if (honorificAbbreviationForm<0) honorificAbbreviationForm=FormsClass::gFindForm(L"honorific_abbreviation");
	if (businessForm<0)  businessForm=FormsClass::gFindForm(L"business");
	if (demonymForm<0)  demonymForm=FormsClass::gFindForm(L"demonym");
	//if (relativeForm<0)  relativeForm=FormsClass::gFindForm(L"relative");
	if (commonProfessionForm<0) commonProfessionForm=FormsClass::gFindForm(L"commonProfession");
	if (friendForm<0) friendForm=FormsClass::gFindForm(L"friend");
	if (moneyForm<0) moneyForm = FormsClass::gFindForm(L"money");
	if (webAddressForm<0) webAddressForm = FormsClass::gFindForm(L"webAddress");
	if (internalStateForm<0) internalStateForm = FormsClass::gFindForm(L"internalState");
	if (particleForm<0) particleForm=FormsClass::gFindForm(L"particle");
	if (relativeForm<0) relativeForm=FormsClass::gFindForm(L"relative");
	if (monthForm < 0) monthForm = FormsClass::gFindForm(L"month");

	// DO NOT REMOVE - this section filled with Daniel Webster dictionary problems
	removeFlag(L"man",PLURAL); // Webster has it also defined as a plural noun (very rare)
	removeFlag(L"sir",PLURAL); 
	removeFlag(L"other",PLURAL); 
	removeFlag(L"ear",PLURAL); 
	removeFlag(L"lives",SINGULAR); 
	removeFlag(L"spoke",VERB_PRESENT_FIRST_SINGULAR);
	removeFlag(L"rose",VERB_PRESENT_FIRST_SINGULAR);
	removeFlag(L"one",FIRST_PERSON|SECOND_PERSON);
	removeFlag(L"sat",VERB_PRESENT_FIRST_SINGULAR);
	removeFlag(L"bleed",VERB_PAST_PLURAL);
	addFlag(L"bleed",VERB_PRESENT_FIRST_SINGULAR);
	addFlag(L"overspread",VERB_PAST|VERB_PAST_PARTICIPLE);
	// written is incorrectly entered in Websters
	removeFlag(L"written",VERB_PAST);
	// come has rare usage in Websters
	removeFlag(L"come",VERB_PAST);
	// hold is incorrectly entered in Websters
	removeFlag(L"hold",VERB_PAST);
	// letters is incorrectly entered...
	removeFlag(L"letters",SINGULAR);
	// will is not an adverb or adjective!
	tIWMM q=gquery(L"will");
	q->second.remove(adjectiveForm);
	q->second.remove(adverbForm);
	// might is not a verb - it is only an auxiliary
	gquery(L"might")->second.remove(verbForm);
	// may is not a verb - it is only an auxiliary
	gquery(L"may")->second.remove(verbForm);
	// was is a plural of wa - exceedingly rare usage
	gquery(L"was")->second.remove(nounForm);
	// my as an interjection is OK, except that the parser "ignores" interjections, so this common adjective is absorbed into inappropriate
	// patterns like _ADVERB.  If the parser would not need to ignore interjections, then this line can be removed.
	gquery(L"my")->second.remove(interjectionForm);
	// 'the' is never an adverb
	gquery(L"the")->second.remove(adverbForm);
	// 'the' is never a preposition
	gquery(L"the")->second.remove(prepositionForm);
	// 'of' is never a verbal auxiliary
	gquery(L"of")->second.remove(L"verbal_auxiliary");
	gquery(L"through")->second.remove(L"noun");
	gquery(L"knowledge")->second.remove(verbForm);
	// 'do' is a verb but it is already included in the patterns
	gquery(L"do")->second.remove(verbForm);
	// 'to' is kind of an adverb but most of the time it is not
	gquery(L"to")->second.remove(adverbForm);
	// 'in' is already a measurement abbreviation
	gquery(L"in")->second.remove(abbreviationForm);
	// 'in' is not a verb
	gquery(L"in")->second.remove(verbForm);
	// something is not a verb!
	gquery(L"something")->second.remove(verbForm);
	// 'but' is NOT a pronoun!
	gquery(L"but")->second.remove(pronounForm);
	// 'but' could be used as 'butt' out, but it is too rare
	gquery(L"but")->second.remove(verbForm);
	// 'but' could be used as 'butt' out, but it is too rare
	// but is a coordinator for BNC only
	gquery(L"but")->second.remove(coordinatorForm);
	// at might have a pronoun form but that is very rare for such a common word
	gquery(L"at")->second.remove(pronounForm);
	// at only has conjunction because of 'at meaning that, which is really an abbreviation
	gquery(L"at")->second.remove(conjunctionForm); 
	// ear is not a pronoun!
	gquery(L"ear")->second.remove(pronounForm);
	// planning is not a noun (although it can be used as such)
	gquery(L"planning")->second.remove(nounForm);
	// 'here' when acting as an adverb should actually be a noun anaphor
	tIWMM here=Words.query(L"here");
	if (here!=Words.end()) here->second.remove(L"adverb");
	// i is very rarely a proper noun
	gquery(L"i")->second.remove(PROPER_NOUN_FORM_NUM);
	// 'being' is NOT a conjunction - that is marked with top-level and then all verb relative clauses can start after it
	// which would be wrong
	gquery(L"being")->second.remove(conjunctionForm);
	// beauty is not a verb 
	gquery(L"beauty")->second.remove(verbForm);
  gquery(L"bad")->second.remove(verbForm);
	gquery(L"ought")->second.remove(pronounForm);
	gquery(L"have")->second.remove(verbverbForm);
	gquery(L"had")->second.remove(verbverbForm);
	gquery(L"having")->second.remove(verbverbForm);
		gquery(L"as")->second.remove(nounForm); // as is alternate singular form of "ass"
		gquery(L"as")->second.remove(pronounForm); // as is technically a pronoun but is very rarely used in the same way as other pronouns
		gquery(L"as")->second.flags&=~(tFI::queryOnLowerCase|tFI::queryOnAnyAppearance);
	// feet is the plural of foot (an extremely rare usage is also included in MW with 'foot' as plural also)
	gquery(L"feet")->second.mainEntry=gquery(L"foot");
	gquery(L"women")->second.mainEntry=gquery(L"woman");
	gquery(L"rose")->second.mainEntry=gquery(L"rise");
	gquery(L"cutting")->second.mainEntry=gquery(L"cut");
	gquery(L"speaking")->second.mainEntry=gquery(L"speak");
	gquery(L"trying")->second.mainEntry=gquery(L"try");
	gquery(L"becoming")->second.mainEntry=gquery(L"become");
	gquery(L"gentlemen")->second.mainEntry=gquery(L"gentleman");
	gquery(L"drew")->second.mainEntry=gquery(L"draw");
	gquery(L"slewed")->second.mainEntry=gquery(L"slew");
	gquery(L"paid")->second.mainEntry=gquery(L"pay");
	gquery(L"produced")->second.mainEntry=gquery(L"produce");
	gquery(L"tried")->second.mainEntry=gquery(L"try");
	gquery(L"hurried")->second.mainEntry=gquery(L"hurry");
	gquery(L"carried")->second.mainEntry=gquery(L"carry");
	gquery(L"thrown")->second.mainEntry=gquery(L"throw");
	gquery(L"slid")->second.mainEntry=gquery(L"slide");
	gquery(L"hid")->second.mainEntry=gquery(L"hide");
	gquery(L"woke")->second.mainEntry=gquery(L"wake");
	gquery(L"hidden")->second.mainEntry=gquery(L"hide");
	gquery(L"aged")->second.mainEntry=gquery(L"age");
	gquery(L"pictured")->second.mainEntry=gquery(L"picture");
	gquery(L"acquired")->second.mainEntry=gquery(L"acquire");
	gquery(L"published")->second.mainEntry=gquery(L"publish");
	gquery(L"modelled")->second.mainEntry=gquery(L"model");
	gquery(L"hemmed")->second.mainEntry=gquery(L"hem");
	gquery(L"leaned")->second.mainEntry=gquery(L"lean");
	gquery(L"letters")->second.mainEntry=gquery(L"letter");
	gquery(L"ditches")->second.mainEntry=gquery(L"ditch");
	gquery(L"fairies")->second.mainEntry=gquery(L"fairy");
	gquery(L"geese")->second.mainEntry=gquery(L"goose");
	gquery(L"lilies")->second.mainEntry=gquery(L"lily");
	//gquery(L"jackknives")->second.mainEntry=gquery(L"jackknife");
	gquery(L"mice")->second.mainEntry=gquery(L"mouse");
	gquery(L"mistresses")->second.mainEntry=gquery(L"mistress");
	gquery(L"mercies")->second.mainEntry=gquery(L"mercy");
	gquery(L"lives")->second.mainEntry=gquery(L"live");
	gquery(L"drowned")->second.mainEntry=gquery(L"drown");
	gquery(L"worried")->second.mainEntry=gquery(L"worry");
	gquery(L"satisfied")->second.mainEntry=gquery(L"satisfy");
	gquery(L"studied")->second.mainEntry=gquery(L"study");
	gquery(L"keeping")->second.mainEntry=gquery(L"keep");
	gquery(L"killing")->second.mainEntry=gquery(L"kill");
	gquery(L"dying")->second.mainEntry=gquery(L"die");
	gquery(L"bringing")->second.mainEntry=gquery(L"bring");
	gquery(L"buying")->second.mainEntry=gquery(L"buy");
	gquery(L"occupied")->second.mainEntry=gquery(L"occupy");
	gquery(L"countesses")->second.mainEntry=gquery(L"countess");
	gquery(L"grandchildren")->second.mainEntry=gquery(L"grandchild");
	gquery(L"kings")->second.mainEntry=gquery(L"king");
	gquery(L"ladies")->second.mainEntry=gquery(L"lady");
	gquery(L"leashes")->second.mainEntry=gquery(L"leash");
	//gquery(L"messieurs")->second.mainEntry=gquery(L"messieur");
	//gquery(L"nanobots")->second.mainEntry=gquery(L"nanobot");
	gquery(L"navies")->second.mainEntry=gquery(L"navy");
	gquery(L"rosebushes")->second.mainEntry=gquery(L"bush");
	gquery(L"scarves")->second.mainEntry=gquery(L"scarf");
	gquery(L"sisters")->second.mainEntry=gquery(L"sister");
	gquery(L"snuffboxes")->second.mainEntry=gquery(L"box");
	//gquery(L"subdelegations")->second.mainEntry=gquery(L"subdelegation");
	gquery(L"teeth")->second.mainEntry=gquery(L"tooth");
	gquery(L"waitresses")->second.mainEntry=gquery(L"waitress");
	gquery(L"wives")->second.mainEntry=gquery(L"wife");
	//gquery(L"bivouacking")->second.mainEntry=gquery(L"bivouack");
	gquery(L"omitting")->second.mainEntry=gquery(L"omit");
	gquery(L"businessmen")->second.mainEntry=gquery(L"man");
	gquery(L"saves")->second.mainEntry=gquery(L"save");
	gquery(L"bucks")->second.mainEntry=gquery(L"buck");
	gquery(L"torments")->second.mainEntry=gquery(L"torment");
	//gquery(L"militarymen")->second.mainEntry=gquery(L"man");
	gquery(L"bled")->second.mainEntry=gquery(L"bleed");
	gquery(L"dotted")->second.mainEntry=gquery(L"dot");
	gquery(L"repaid")->second.mainEntry=gquery(L"repay");
	gquery(L"countermarches")->second.mainEntry=gquery(L"march");
	//gquery(L"reestablishes")->second.mainEntry=gquery(L"establish");
	//gquery(L"outvying")->second.mainEntry=gquery(L"vie");
	gquery(L"few")->second.remove(verbForm);
	gquery(L"gov'nor")->second.remove(verbForm);
	gquery(L"bear")->second.remove(verbForm);
	gquery(L"more")->second.remove(verbForm);
	gquery(L"just")->second.remove(verbForm);
	gquery(L"poor")->second.remove(verbForm);
	gquery(L"able")->second.remove(verbForm);
	gquery(L"m.p.")->second.remove(verbForm);
	gquery(L"much")->second.remove(verbForm);
	gquery(L"c.i.d.")->second.remove(verbForm);
	gquery(L"gott")->second.remove(verbForm);
	gquery(L"trading")->second.mainEntry=gquery(L"trade");
	gquery(L"sitting")->second.mainEntry=gquery(L"sit");
	gquery(L"rattling")->second.mainEntry=gquery(L"rattle");
	gquery(L"jarred")->second.mainEntry=gquery(L"jar");
	gquery(L"sitting")->second.mainEntry=gquery(L"sit");
	gquery(L"panning")->second.mainEntry=gquery(L"pan");
	gquery(L"emboldened")->second.mainEntry=gquery(L"embolden");
	gquery(L"laid")->second.mainEntry=gquery(L"lay");
	gquery(L"girls")->second.mainEntry=gquery(L"girl");
	gquery(L"waiters")->second.mainEntry=gquery(L"waiter");
	gquery(L"burning")->second.mainEntry=gquery(L"burn");
	gquery(L"bluffing")->second.mainEntry=gquery(L"bluff");
	gquery(L"employed")->second.mainEntry=gquery(L"employ");
	gquery(L"pinning")->second.mainEntry=gquery(L"pin");
	gquery(L"polishing")->second.mainEntry=gquery(L"polish");
	gquery(L"terrified")->second.mainEntry=gquery(L"terrify");
	gquery(L"barbecued")->second.mainEntry=gquery(L"barbecue");
	gquery(L"rung")->second.mainEntry=gquery(L"ring");

	// bore should never have been made a "think" verb
	gquery(L"bear")->second.remove(thinkForm);
	gquery(L"bore")->second.remove(thinkForm);
	gquery(L"bored")->second.remove(thinkForm);
	gquery(L"bears")->second.remove(thinkForm);
	gquery(L"or")->second.remove(prepositionForm);
	gquery(L"or")->second.remove(adjectiveForm);
	gquery(L"hand")->second.remove(commonProfessionForm); // included by WordNet employee +HYPO
	gquery(L"daughters")->second.mainEntry=gquery(L"daughter");
	gquery(L"husbands")->second.mainEntry=gquery(L"husband");
	gquery(L"fathers")->second.mainEntry=gquery(L"father");
	gquery(L"seconds")->second.mainEntry=gquery(L"second");
	gquery(L"can't")->second.mainEntry=gquery(L"can");
	gquery(L"couldn't")->second.mainEntry=gquery(L"could");
	gquery(L"shouldn't")->second.mainEntry=gquery(L"should");
	gquery(L"hadn't")->second.mainEntry=gquery(L"have");
	gquery(L"oughtn't")->second.mainEntry=gquery(L"ought");
	gquery(L"won't")->second.mainEntry=gquery(L"will");
	gquery(L"convey")->second.remove(internalStateForm);
	gquery(L"conveys")->second.remove(internalStateForm);
	gquery(L"conveyed")->second.remove(internalStateForm);
	gquery(L"conveying")->second.remove(internalStateForm);
	gquery(L"janet")->second.remove(verbForm);
	gquery(L"janet")->second.remove(nounForm);
	gquery(L"janet")->second.remove(adjectiveForm);
	gquery(L"janet")->second.remove(COMBINATION_FORM_NUM);
	gquery(L"arrange")->second.remove(thinkForm);
	gquery(L"arranges")->second.remove(thinkForm);
	gquery(L"arranged")->second.remove(thinkForm);
	gquery(L"arranging")->second.remove(thinkForm);
	int timeUnitForm=FormsClass::gFindForm(L"timeUnit");
	addFlag(L"what",PLURAL);  // 'what' was inadvertently set to singular before
	gquery(L"wick")->second.remove(timeUnitForm);
	Forms[honorificAbbreviationForm]->blockProperNounRecognition=true; // any words having this form will never be considered as proper nouns
	wchar_t *topLevelForms[]=
				 {L",",L";",L"--",L":",L".",L"*",L"!",L"?",L"...",L"&",L"/",L"#",L"=",L"|",L"│", // added "|" 4/11/2006 for BNC
			L"brackets",L"dash",L"quotes",L"numeral_ordinal",L"roman_numeral",L"sectionheader", // deleted 'not' 10/26/2006
			L"conjunction",L"coordinator",L"interjection",L"relativizer",L"inserts",NULL}; // expandable forms
	for (int form=0; topLevelForms[form]; form++)
	{
		int f=FormsClass::gFindForm(topLevelForms[form]);
		Forms[f]->isTopLevel=true;
	}
	int commonForms[]= 
			{ reflexiveForm,nomForm,accForm,conjunctionForm,demonstrativeDeterminerForm,possessiveDeterminerForm,interrogativeDeterminerForm,
		indefinitePronounForm,reciprocalPronounForm,pronounForm,thinkForm,relativeForm,
		determinerForm,doesForm,doesNegationForm,possessivePronounForm,quantifierForm,coordinatorForm,
		beForm,haveForm,haveNegationForm,doForm,doNegationForm,interjectionForm,personalPronounForm,
		isForm,isNegationForm,prepositionForm,toForm,relativizerForm,particleForm,
		doForm,doNegationForm,modalAuxiliaryForm,futureModalAuxiliaryForm,negationModalAuxiliaryForm,negationFutureModalAuxiliaryForm,NULL};
	for (int form = 0; commonForms[form]; form++)
		Forms[commonForms[form]]->isCommonForm = true;

	// numeralCardinalForm, numeralOrdinalForm, romanNumeralForm, quantifierform, dateForm,timeForm, telephoneNumberForm, moneyForm, and webAddressForm
	// are not closed, but they can be positively identified so they do not have to be written in the cache file
	vector<int> nonCachedForms = { commaForm, periodForm ,reflexiveForm	,nomForm	,accForm	,quoteForm	,dashForm	,bracketForm	,conjunctionForm,
		demonstrativeDeterminerForm	,possessiveDeterminerForm	,interrogativeDeterminerForm	,indefinitePronounForm	,reciprocalPronounForm,
		pronounForm	,numeralCardinalForm	,numeralOrdinalForm	,romanNumeralForm	,honorificForm	,honorificAbbreviationForm	,relativeForm	,
		determinerForm,doesForm	,doesNegationForm	,possessivePronounForm	,quantifierForm	,dateForm	,timeForm	,telephoneNumberForm	,
		coordinatorForm,numberForm	,beForm	,haveForm	,haveNegationForm	,doForm	,doNegationForm	,
		personalPronounForm	,letterForm	,isForm	,isNegationForm	, telenumForm	,sa_abbForm	,toForm,relativizerForm	,
		moneyForm	,particleForm	,webAddressForm	,doForm	,doNegationForm	,monthForm	,letterForm,modalAuxiliaryForm	,futureModalAuxiliaryForm	,
		negationModalAuxiliaryForm	,negationFutureModalAuxiliaryForm };
	for (int form: nonCachedForms)
		Forms[form]->isNonCachedForm = true;

	// "quotes" removed 6/24 because it was causing NOUN to match double nouns across ".
	// example:_NOUN [my dear child , " interrupted tuppence]
	wchar_t *ignoreForms[]=
			{L"dash",L"interjection",L"/",L"^",L"|",L"│",NULL}; // bracketing with "/" sometimes used as emphasis (took out "--", 8/30/2005) added "|" 4/11/2006 for BNC

	for (tIWMM iWord=Words.WMM.begin(),iWordEnd=Words.WMM.end(); iWord!=iWordEnd; iWord++)
		iWord->second.flags&=~tFI::ignoreFlag;
	for (int form=0; ignoreForms[form]; form++)
	{
		int f=FormsClass::gFindForm(ignoreForms[form]);
		Forms[f]->isIgnore=true;
	}
	for (tIWMM iWord=Words.WMM.begin(),iWordEnd=Words.WMM.end(); iWord!=iWordEnd; iWord++)
	{
		iWord->second.removeIllegalForms();
		iWord->second.setIgnore();
		iWord->second.setTopLevel();
	}
	int verbForms[]={ verbForm,verbverbForm,isForm,isNegationForm,haveForm,haveNegationForm,doesForm,doesNegationForm,
		modalAuxiliaryForm,negationModalAuxiliaryForm,futureModalAuxiliaryForm,negationFutureModalAuxiliaryForm,beForm,thinkForm,-1 };
	for (int form=0; verbForms[form]>=0; form++)
		Forms[verbForms[form]]->verbForm=true;
	gquery(L"i")->second.flags&=~tFI::topLevelSeparator; // don't make "I" top level, even though it is also a roman numeral!
	sectionWord->second.flags|=tFI::topLevelSeparator;
	PPN=gquery(L"__ppn__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	TELENUM=gquery(L"__telenum__"); // personal proper noun used for relations with pronouns or gendered proper nouns.
	NUM=gquery(L"__num__"); // number used for relations.
	DATE=gquery(L"__date__");
	TIME=gquery(L"__time__");
	predefineWord(L"__location__"); // location used for relations.
	LOCATION=gquery(L"__location__");
	TABLE=predefineWord(L"lpTABLE"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	END_COLUMN=predefineWord(L"lpENDCOLUMN"); // used to end each column string which is extracted from <table> and table-like constructions in HTML
	END_COLUMN_HEADERS=predefineWord(L"lpENDCOLUMNHEADERS"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	MISSING_COLUMN=predefineWord(L"lpMISSINGCOLUMN"); // used to start the table section which is extracted from <table> and table-like constructions in HTML
	// set usageCosts for 'think' verbs to be equal to the cost of verb usage.
	// see similar routine for regularizing usage cost of nouns - usageCostToNoun
	for (tIWMM iWord=Words.WMM.begin(),iWordEnd=Words.WMM.end(); iWord!=iWordEnd; iWord++)
	{
		iWord->second.flags&=~(tFI::physicalObjectByWN|tFI::notPhysicalObjectByWN|tFI::uncertainPhysicalObjectByWN);
		iWord->second.costEquivalentSubClass(commonProfessionForm,nounForm);
		iWord->second.costEquivalentSubClass(thinkForm,verbForm);
		iWord->second.costEquivalentSubClass(verbverbForm,verbForm);
		if ((iWord->second.inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST))==(VERB_PRESENT_FIRST_SINGULAR|VERB_PAST) &&
      iWord->second.mainEntry!=iWord && iWord->second.mainEntry!=wNULL &&
			(iWord->second.mainEntry->second.inflectionFlags&(VERB_PRESENT_FIRST_SINGULAR))==(VERB_PRESENT_FIRST_SINGULAR))
		{
			//lplog(L"past,present confused for verb %s (mainEntry %s).",iWord->first.c_str(),iWord->second.mainEntry->first.c_str());
			removeFlag(iWord->first,VERB_PRESENT_FIRST_SINGULAR);
		}
		// reset these counts, which are more relevant on a per-source basis
		iWord->second.usagePatterns[tFI::PROPER_NOUN_USAGE_PATTERN]=iWord->second.usagePatterns[tFI::LOWER_CASE_USAGE_PATTERN]=0;
		iWord->second.numProperNounUsageAsAdjective=0;
		// make honorifics not costly (honorifics are not reflected out of BNC properly so they are underweighted)
		iWord->second.toLowestCost(honorificForm);
		if (!iWord->second.toLowestCost(demonstrativeDeterminerForm) && iWord->second.query(relativizerForm)<0 && iWord->first!=L"there")
			iWord->second.toLowestCost(pronounForm);
		if (iWord->second.costEquivalentSubClass(indefinitePronounForm,nounForm))
		{
			iWord->second.remove(nounForm);
			iWord->second.flags&=~(tFI::queryOnLowerCase|tFI::queryOnAnyAppearance);
		}
		// two knocks followed in succession (two could also be a noun, knocks could be a verb, so two must be marked as plural!)
		if (iWord->second.query(numeralCardinalForm)>=0)
		{
			if (iWord->first!=L"one")
				iWord->second.inflectionFlags|=PLURAL;
			iWord->second.inflectionFlags|=MALE_GENDER|FEMALE_GENDER; // Number Fourteen, please close the door. / The Two
		}
		// Brandy vs brandy / The brandy[rita] brought the colour back to her[rita] white cheeks[rita] , and revived her[rita] in a marvellous fashion .
		if ((iWord->second.inflectionFlags&(MALE_GENDER|FEMALE_GENDER)) && iWord->second.query(PROPER_NOUN_FORM_NUM)>=0 && 
				iWord->second.query(nounForm)>=0 && iWord->second.query(honorificForm)<0)
		{
			if (iWord->second.inflectionFlags&MALE_GENDER)
			{
				iWord->second.inflectionFlags&=~MALE_GENDER;
				iWord->second.inflectionFlags|=MALE_GENDER_ONLY_CAPITALIZED;
			}
			if (iWord->second.inflectionFlags&FEMALE_GENDER)
			{
				iWord->second.inflectionFlags&=~FEMALE_GENDER;
				iWord->second.inflectionFlags|=FEMALE_GENDER_ONLY_CAPITALIZED;
			}
			//lplog(LOG_RESOLUTION,L"%s will only be gendered if capitalized.",iWord->first.c_str());
		}
	}
	gquery(L"tell")->second.usagePatterns[tFI::VERB_HAS_2_OBJECTS]=255;
	gquery(L"tell")->second.usageCosts[tFI::VERB_HAS_2_OBJECTS]=0;
	gquery(L"descend")->second.usagePatterns[tFI::VERB_HAS_1_OBJECTS]=255;
	gquery(L"descend")->second.usageCosts[tFI::VERB_HAS_1_OBJECTS]=0;
	gquery(L"wish")->second.usagePatterns[tFI::VERB_HAS_2_OBJECTS]=255; // I wish you some figgy pudding
	gquery(L"wish")->second.usageCosts[tFI::VERB_HAS_2_OBJECTS]=0;
	gquery(L"get")->second.usagePatterns[tFI::VERB_HAS_2_OBJECTS]=127; // to get her a taxi
	gquery(L"get")->second.usageCosts[tFI::VERB_HAS_2_OBJECTS]=2;
	gquery(L"nurse")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; 
	gquery(L"nurse")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"turn")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; 
	gquery(L"turn")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"rap")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; 
	gquery(L"rap")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"speed")->second.usagePatterns[tFI::VERB_HAS_1_OBJECTS]=0;
	gquery(L"speed")->second.usageCosts[tFI::VERB_HAS_1_OBJECTS]=4;
	gquery(L"other")->second.toLowestCost(indefinitePronounForm);
	gquery(L"last")->second.toLowestCost(adjectiveForm);
	gquery(L"last")->second.toLowestCost(adverbForm);
	gquery(L"few")->second.toLowestCost(quantifierForm);
	gquery(L"spring")->second.toLowestCost(verbForm);
	gquery(L"dove")->second.setCost(verbForm,3);
	gquery(L"mind")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; 
	gquery(L"mind")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"walk")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; 
	gquery(L"walk")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"repair")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; // He wanted to repair to the Gallery.
	gquery(L"repair")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"step")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; 
	gquery(L"step")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"side")->second.usagePatterns[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=0; 
	gquery(L"side")->second.usageCosts[tFI::SINGULAR_NOUN_HAS_NO_DETERMINER]=4;
	gquery(L"situation")->second.flags|=tFI::notPhysicalObjectByWN;
	gquery(L"hope")->second.flags|=tFI::notPhysicalObjectByWN;
	gquery(L"mind")->second.flags|=tFI::notPhysicalObjectByWN;
	Forms[indefinitePronounForm]->inflectionsClass=L"noun";
	Forms[letterForm]->inflectionsClass=L"noun";
	Forms[honorificForm]->inflectionsClass=L"noun";
	addGenderedNouns(L"source\\lists\\commonProfessions.txt",SINGULAR|MALE_GENDER|FEMALE_GENDER,commonProfessionForm);
	addGenderedNouns(L"source\\lists\\newCommonProfessions.txt",SINGULAR|MALE_GENDER|FEMALE_GENDER,commonProfessionForm);
	addGenderedNouns(L"source\\lists\\commonProfessionsPlural.txt",PLURAL|MALE_GENDER|FEMALE_GENDER,commonProfessionForm); // only exist in plural form [staff]
	gquery(L"woman")->second.remove(commonProfessionForm);
	gquery(L"hand")->second.remove(commonProfessionForm);
	predefineVerbsFromFile(L"think",L"think",L"source\\lists\\newThinkSayVerbs.txt",0);
	Inflections governmentTitle[] = {
				{L"ambassador",MALE_GENDER|FEMALE_GENDER},
				{NULL,0}};
	predefineWords(governmentTitle,L"honorific",L"hon",L"noun",tFI::queryOnAnyAppearance,false);
	/***** begin remove section when refresh of DB is done 
		// for each timeUnit, etc., make sure that the usageCost of that = that of the equivalent noun.
	/*
	addGenderedNouns(L"source\\lists\\newCommonProfessions.txt",SINGULAR|MALE_GENDER|FEMALE_GENDER,commonProfessionForm);
	predefineVerbsFromFile(L"internalState",L"internalState",L"source\\lists\\newInternalStateVerbs.txt",0);
	Inflections honorificFull[] = {{L"count",MALE_GENDER},{L"inspector",MALE_GENDER|FEMALE_GENDER},{NULL,0}};
	predefineWords(honorificFull,L"honorific",L"hon",L"",0,false);
	wchar_t *particles[]={ L"apart", L"about", L"across", L"along", L"around", L"aside", L"away", L"back", L"by", L"down", 
												 L"forth", L"forward", L"home", L"in", L"off", L"on", L"out", L"over", L"past", L"round", L"through", L"under", L"up",NULL };
	particleForm=predefineWords(particles,L"particle",L"pa");
	wchar_t *relatives[]={ L"mother", L"father", L"sister", L"brother", L"sibling", L"son", L"daughter",
												 L"grandmother", L"grandfather", L"grandson", L"granddaughter",
												 L"wife", L"husband", L"spouse",
												 L"niece", L"nephew", L"uncle", L"aunt", 
												 L"grandniece", L"grandnephew", L"granduncle", L"grandaunt", 
												 L"brother-in-law", L"sister-in-law", L"father-in-law", L"mother-in-law", L"daughter-in-law", L"son-in-law",
												 L"cousin", L"coworker", L"relation", L"relative", L"parent", L"child",
												 NULL };
	relativeForm=predefineWords(relatives,L"relative",L"re");
	wchar_t *interjection[] = {L"sorry",L"lord",NULL};
	predefineWords(interjection,L"interjection",L"inter");
	*/
	/***** end remove section when refresh of DB is done ****/
	Inflections pronoun[] = {{L"rest",PLURAL|MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER},{NULL,0}};
	predefineWords(pronoun,L"pronoun",L"pn",L"noun",tFI::queryOnAnyAppearance,false);
	Inflections indefinite_pronoun[]={ {L"every one",SINGULAR|MALE_GENDER|FEMALE_GENDER},{NULL,0}};
	predefineWords(indefinite_pronoun,L"indefinite_pronoun",L"indef_pron",L"noun",0);
	gquery(L"every one")->second.remove(UNDEFINED_FORM_NUM);
	gquery(L"every one")->second.remove(L"abbreviation");
	gquery(L"every one")->second.remove(L"adjective");
	gquery(L"every one")->second.remove(L"verb");
	gquery(L"every one")->second.remove(L"adverb");
	gquery(L"every one")->second.remove(pronounForm);
	Inflections titles[]={ {L"priestess",FEMALE_GENDER},{L"chief",MALE_GENDER},{L"chieftess",FEMALE_GENDER},{L"mayor",MALE_GENDER},{L"monseigneur",MALE_GENDER},{L"emperor",MALE_GENDER},{NULL,0}};
	predefineWords(titles,L"honorific",L"hon",L"noun",0,false);
	Inflections honorific[] = {{L"mademoiselle",FEMALE_GENDER},{NULL,0}};
	predefineWords(honorific,L"honorific",L"hon",L"noun",0,false);
	Inflections pinr[] = {{L"mademoiselle",FEMALE_GENDER},{NULL,0}};
  predefineWords(pinr,L"pinr",L"pinr",L"noun",false,false);
	Inflections indefinite_pronoun2[]={ // agreement is singular ownership is fuzzy
			{L"crowd",PLURAL|MALE_GENDER|FEMALE_GENDER},
			{NULL,0}};
	predefineWords(indefinite_pronoun2,L"indefinite_pronoun",L"indef_pron",L"noun",0);
	Inflections quantifier[] = {{L"several",PLURAL},{NULL,0}};
	predefineWords(quantifier,L"quantifier",L"quant",L"noun",0,false);
	InflectionsRoot adverb2[] = {{L"next",ADVERB_NORMATIVE,L"next"},{L"first",ADVERB_NORMATIVE,L"first"},{L"in total",ADVERB_NORMATIVE,L"in total"},{NULL,0}}; // when did he first run?
	predefineWords(adverb2,L"adverb",L"adv",L"adverb");  
	InflectionsRoot adjective[] = {{L"state-of-the-art",ADJECTIVE_NORMATIVE,L"state-of-the-art"},{L"next",ADJECTIVE_NORMATIVE,L"next"},{L"op-ed",ADJECTIVE_NORMATIVE,L"op-ed"},{NULL,0}};
	predefineWords(adjective,L"adjective",L"adj",L"adjective");  
	Inflections honorific_abbreviation[] = {{L"sen",MALE_GENDER|FEMALE_GENDER},{NULL,0}};
	predefineWords(honorific_abbreviation,L"honorific_abbreviation",L"hon_abb",L"noun",0,false);
	InflectionsRoot noun_abbreviation[] = {{L"ph.d.",SINGULAR,L"ph.d."},{NULL,0}};
	predefineWords(noun_abbreviation,L"noun",L"n",L"noun");
	wchar_t *det_another[]={ L"another",NULL };
	predefineWords(det_another,L"determiner",L"det");
	addDemonyms(L"source\\lists\\newdemonyms.txt");
	gquery(L"englishmen")->second.mainEntry=gquery(L"englishman");
	gquery(L"men")->second.mainEntry=gquery(L"man");
	gquery(L"burning")->second.mainEntry=gquery(L"burn");
	InflectionsRoot verbverb[] = {
		{L"feel",VERB_PRESENT_FIRST_SINGULAR,L"feel"},{L"felt",VERB_PAST,L"feel"},      {L"feeling",VERB_PRESENT_PARTICIPLE,L"feel"},  {L"feels",VERB_PRESENT_THIRD_SINGULAR,L"feel"},
		{NULL,0}};
	predefineWords(verbverb,L"verbverb",L"verbverb",L"verb",0);
	Inflections abbreviation[] = {
		{L"isbn",SINGULAR},
		{NULL,0}};
	predefineWords(abbreviation,L"abbreviation",L"abb",L"noun",0,true);
	initializeChangeStateVerbs();

	gquery(L"felt")->second.flags&=~(tFI::queryOnLowerCase|tFI::queryOnAnyAppearance);
	removeFlag(L"felt",VERB_PRESENT_FIRST_SINGULAR);
	wchar_t *addAllGender[]={L"both",L"either",L"neither",L"any",L"all",L"another",L"other",L"each",NULL};
	for (unsigned int I=0; addAllGender[I]; I++) 
		addFlag(addAllGender[I],MALE_GENDER|FEMALE_GENDER|NEUTER_GENDER);
	wchar_t *addGender[]={L"me",L"my",L"our",L"your",L"thy",L"you",L"ye",L"ya",L"yer",L"youse",L"thee",L"thees",L"thou",L"yourself",L"yourselves",NULL};
	for (unsigned int I=0; addGender[I]; I++) 
		addFlag(addGender[I],MALE_GENDER|FEMALE_GENDER);
	addFlag(L"i",MALE_GENDER_ONLY_CAPITALIZED|FEMALE_GENDER_ONLY_CAPITALIZED);
	addFlag(L"their",FEMALE_GENDER|MALE_GENDER|NEUTER_GENDER);
	addFlag(L"couple",PLURAL);
	wchar_t *genericGender[]={ L"man", L"fellow", L"gentleman", L"sir", L"chap", L"woman", L"lady", L"madam", L"girl", L"miss",L"missus",L"boy",NULL };
	for (unsigned int I=0; genericGender[I]; I++)
		Words.gquery(genericGender[I])->second.flags|=tFI::genericGenderIgnoreMatch;
	wchar_t *genericAgeGender[]={ L"child", L"baby", L"husband", L"wife",L"father", L"mother",NULL };
	for (unsigned int I=0; genericAgeGender[I]; I++)
		Words.gquery(genericAgeGender[I])->second.flags|=tFI::genericAgeGender;
	// 'that' may not be a relativizer... it doesn't necessarily start a question...
	gquery(L"that")->second.remove(relativizerForm);
	Forms[determinerForm]->blockProperNounRecognition=true; // any words having this form will never be considered as proper nouns
	Forms[honorificAbbreviationForm]->blockProperNounRecognition=true; // any words having this form will never be considered as proper nouns
	createTimeCategories(true);
	gquery(L"sat")->second.flags&=~(tFI::queryOnLowerCase|tFI::queryOnAnyAppearance);
	readVerbClasses();
	readVerbClassNames();
	readVBNet();
	predefineHolidays();
	printf("Finished initializing dictionary...\r");
}

