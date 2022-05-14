bool spaceSame(wstring sWord,wstring option)
{
	if (sWord.find(L" ")==wstring::npos)
		return option.find(L"%2B")==wstring::npos;
	return option.find(L"%2B")!=wstring::npos;
}

int processInflections(wstring sWord,wstring mainEntry,wstring Form,wstring sInflections,vector<wstring> &allInflections)
{
	wchar_t *begin,*end;
	wstring inflections;
	bool boldUsed=false;
	if (Form!=L"verb") allInflections.push_back(mainEntry);
	if (!sInflections.length())
	{
		int where;
		if (Form==L"noun") sInflections=L"<b>-s</b>";
		else if (Form==L"adjective" || Form==L"adverb") sInflections=L"<b>-er/-est</b>";
		else if ((where=Form.find(L"verb"))!=wstring::npos && (where==0 || Form[where-1]==' ')) sInflections=L"<b>-ed/-ing/-s</b>";
		else return 0;
	}
	for (int ch=0; sWord[ch]; ch++) sWord[ch]=towlower(sWord[ch]);
	begin=(wchar_t *)sInflections.c_str();
	while (true)
	{
		wchar_t *savech=begin;
		begin=wcsstr(begin,L"<b>");
		// the following if added because of incorrect processing of word 'losing' - also was before the 'losing' entry and so should be ignored.
		wchar_t *nextEntry;
		if ((nextEntry=wcsstr(savech,L"\\;"))!=NULL && nextEntry<begin)
			savech=nextEntry;
		if (begin) boldUsed=true;
		if (!begin && (boldUsed || !(begin=wcsstr(savech,L"<i>")))) break;
		begin+=wcslen(L"<b>");
		end=wcsstr(begin,L"</b>");
		if (!end && !(end=wcsstr(begin,L"</i>"))) break;
		*end=0;
		inflections=begin;
		removeDots(inflections);
		int where=inflections.find(L'&');
		int ch;
		if (where>=0 && swscanf(inflections.c_str()+where,L"&#%d;",&ch)==1)
		{
			inflections[where]=ch;
			inflections[where+1]=0;
		}
		int a;
		for (a=0; a<sizeof(alternates)/sizeof(wchar_t *); a++)
			if (wcsstr(savech,alternates[a])) break;
		if (a<sizeof(alternates)/sizeof(wchar_t *))
		{
			if (equivalentIfIgnoreDashSpaceCase(sWord,inflections))
			{
				if (allInflections.size())  allInflections.pop_back();
			}
			else
			{
				wchar_t *tmp;
				if (tmp=wcschr(end+1,';')) end=tmp+1;  // separate entries added because of incorrect processing of "catch"
				begin=end+1;
				continue;
			}
		}
		int pluralpos;
		if (Form==L"noun" && (pluralpos=inflections.find(L"plural"))!=wstring::npos)
		{
			wstring plural=inflections.substr(pluralpos+1,inflections.find(' ',pluralpos+2)-1);
			allInflections.push_back(plural);
			return 0;
		}
		if (inflections.find('/')==wstring::npos && inflections[0]!='-')
		{
			allInflections.push_back(inflections);
		}
		else
		{
			getInflection(sWord,Form,mainEntry,inflections,allInflections);
			if (Form==L"verb") allInflections.push_back(mainEntry); // MainEntry is present participle first singular
			return 0;
		}
		begin=end+1;
	}
	if (Form==L"verb")
	{
		allInflections.push_back(mainEntry); // MainEntry is present participle first singular
	}
	return 0;
}

// build%5B1%2Cverb%5D%3D459446%3B
// build  [1  ,verb  ]  =459446  ;
int parseOption(wstring option,wstring &newWord,wstring &form,wstring &jump)
{
	int pos2=option.find(L"%5B"),pos3=wstring::npos;
	if ((pos3=option.find(L"%3D"))==wstring::npos)
	{
		lplog(LOG_DICTIONARY,L"ERROR:Incorrect format of option wstring %s.",option.c_str());
		return PARSE_OPTION_FAILED; // incorrect format - no =
	}
	jump=option.substr(0,pos3);
	form=L"";
	if (pos2==wstring::npos)
	{
		newWord=jump;
		return 0;
	}
	newWord=option.substr(0,pos2);
	if ((pos2=option.find(L"%2C"))==wstring::npos) return 0;
	if ((pos3=option.find(L"%5D"))==wstring::npos) return 0;
	form=option.substr(pos2+wcslen(L"%2C"),pos3-pos2-wcslen(L"%2C"));
	int copy=0,original;
	for (original=0; form[original]; )
		if (wcsncmp(form.c_str()+original,L"%2B",3))
			form[copy++]=form[original++];
		else
		{
			form[copy++]=L' ';
			original+=3;
		}
		if (copy!=original) form.erase(copy,original-copy);
		return 0;
}

int findMainEntry(wstring &match,wstring &mainEntry)
{
	wstring temp,temp2;
	while (takeLastMatch(match,L"<a href=\"javascript:popWin",L"</a>",temp2,false)==0);
	takeLastMatch(match,L"bgcolor=\"white\"",L"<br>",temp2,false);
	//if (false) lplog(L"%s\n\n",match.c_str());
	wstring audio;
	takeLastMatch(match,L"Main Entry:\09",L"<br>",temp,false);
	takeLastMatch(temp,L"<a href",L"</a>",audio,false);
	takeLastMatch(temp,L"<b>",L"</b>",mainEntry,false);
	removeDots(mainEntry);
	int where=mainEntry.find(L'&');
	int ch;
	if (where>=0 && swscanf(mainEntry.c_str()+where,L"&#%d;",&ch)==1)
	{
		mainEntry[where]=ch;
		mainEntry[where+1]=0;
	}
	takeLastMatch(mainEntry,L"<sup>",L"</sup>",temp2,true);
	return 0;
}

bool WordClass::processInflectedFunction(wstring sWord,wstring mainEntry,wstring temp,wstring Form,tIWMM &iWord,int sourceId)
{
	if (mainEntry!=sWord) return false;
	int inflection;
	if (wcsstr(temp.c_str(),L"adjective"))
	{
		if (wcsstr(Form.c_str(),L"comparative of")) inflection=ADJECTIVE_COMPARATIVE;
		else if (wcsstr(Form.c_str(),L"superlative of")) inflection=ADJECTIVE_SUPERLATIVE;
		else return false;
		Form=L"adjective";
	}
	else if (wcsstr(temp.c_str(),L"adverb"))
	{
		if (wcsstr(Form.c_str(),L"comparative of")) inflection=ADVERB_COMPARATIVE;
		else if (wcsstr(Form.c_str(),L"superlative of")) inflection=ADVERB_SUPERLATIVE;
		else return false;
		Form=L"adverb";
	}
	else if (Form==L"verb past")
	{
		inflection=VERB_PAST;
		Form=L"verb";
	}
	else return false;
	checkAdd(L"if",iWord,sWord,0,Form,inflection,0,mainEntry,sourceId);
	return true;
}

wchar_t *namedForms[]={
	L"<i>past</i>",L"<i>past part</i>",L"<i>present part</i>",L"<i>present first singular</i>",L"<i>second singular</i>",
	L"<i>third singular</i>",L"<i>plural</i>",L"<i>past first & third singular<i>"
};

bool WordClass::processNamedForms(wstring sWord,wstring mainEntry,wstring Form,tIWMM &iWord,wstring sInflections,int sourceId)
{
	if (Form!=L"verb") return false;
	wchar_t *begin=(wchar_t *)sInflections.c_str(),*end;
	bool namedFormFound=false;
	wstring inflections;
	while (true)
	{
		wchar_t *savech=begin;
		begin=wcsstr(begin,L"<b>");
		if (!begin) break;
		begin+=wcslen(L"<b>");
		end=wcsstr(begin,L"</b>");
		if (!end) break;
		*end=0;
		int a;
		for (a=0; a<sizeof(namedForms)/sizeof(wchar_t *); a++)
			if (wcsstr(savech,namedForms[a])) break;
		if (a==sizeof(namedForms)/sizeof(wchar_t *))
		{
			begin=end+1;
			continue;
		}
		inflections=begin;
		removeDots(inflections);
		int inflection=VERB_PAST;
		switch (a)
		{
		case 0: inflection=VERB_PAST; break;
		case 1: inflection=VERB_PAST_PARTICIPLE; break;
		case 2: inflection=VERB_PRESENT_PARTICIPLE; break;
		case 3: inflection=VERB_PRESENT_FIRST_SINGULAR; break;
		case 4: begin=end+1; continue; // used for the subject "you"
		case 5: inflection=VERB_PRESENT_THIRD_SINGULAR; break;
		case 6: inflection=VERB_PRESENT_PLURAL; break;
		case 7: inflection=VERB_PAST; break;
		}
		wstring inflectionName;
		if (equivalentIfIgnoreDashSpaceCase(sWord,inflections))
			checkAdd(L"PNF",iWord,sWord,0,Form,inflection,0,mainEntry,sourceId);
		else
			lplog(LOG_DICTIONARY,L"word %s (PNF): Detected Inflection%s (%s) (Form %s) definitionEntry %s",
			sWord.c_str(),getInflectionName(inflection,verbInflectionMap,inflectionName),inflections.c_str(),Form.c_str(),mainEntry.c_str());
		namedFormFound=true;
		begin=end+1;
	}
	return namedFormFound;
}

void WordClass::processEndForm(tIWMM &iWord,wstring sWord,wstring match,wstring mainEntry,int sourceId)
{
	wstring lastDitch;
	// take the longest wstring from <br>- <b> to the last </i> before encountering <br> or 0
	while (true)
	{
		int till=0,pos=match.find(L"<br>- <b>",till);
		if (pos==wstring::npos) break;
		till=pos+wcslen(L"<br>- <b>");
		while (match[till] && wcsncmp(match.c_str()+till,L"<br>",wcslen(L"<br>"))) till++;
		lastDitch=match.substr(pos,till-pos);
		match.erase(pos,till-pos);
		wstring possibleWord,forms,form;
		while (!takeLastMatch(lastDitch,L"<i>",L"</i>",form,false))
			forms+=L" "+form;
		while (!takeLastMatch(lastDitch,L"<b>",L"</b>",possibleWord,false)) 
		{
			removeDots(possibleWord);
			while (possibleWord[possibleWord.length()-1]==' ') possibleWord.erase(possibleWord.length()-1,1);
			// possibleWord (derivationBase) ornithischian, mainEntry ornithischia, sWord ornithischians
			if (forms.find(form=L"adjective")!=wstring::npos)
				inflectionMatch(form,iWord,sWord,possibleWord,sourceId,mainEntry);
			if (forms.find(form=L"adverb")!=wstring::npos)
				inflectionMatch(form,iWord,sWord,possibleWord,sourceId,mainEntry);
			if (forms.find(form=L"noun")!=wstring::npos)
				inflectionMatch(form,iWord,sWord,possibleWord,sourceId,mainEntry);
			int pos;
			if ((pos=forms.find(form=L"verb"))!=wstring::npos && (pos==0 || forms[pos-1]!=L'd'))
				inflectionMatch(form,iWord,sWord,possibleWord,sourceId,mainEntry);
		}
	}
}

bool WordClass::reduceDanielWebsterPage(wstring sWord,wstring &buffer)
{
	wchar_t *input=wcsdup(buffer.c_str());
	int len=buffer.length();
	if ((wcsstr(input,L"The word you've entered isn't in the ")) ||
		(wcsstr(input,L"No entries found that match")))
	{
		if (!unknownWDWords.size())
			readUnknownWords(unknownWD,unknownWDWords);
		vector <wstring>::iterator ip=lower_bound(unknownWDWords.begin(),unknownWDWords.end(),sWord);
		wcslwr((wchar_t *)sWord.c_str());
		unknownWDWords.insert(ip,sWord);
		changedWords=true;
		free(input);
		return false;
	}
	eliminate(input,len,L"<script language=\"javascript\"",L"</script>",false);
	eliminate(input,len,L"<!-- Begin hat -->",L"<!-- End hat -->",false);
	eliminate(input,len,L"<!-- Begin logo",L"<!-- End logo, title -->",false);
	eliminate(input,len,L"<map name=\"tabsmap2\">",L"</map>",false);
	eliminate(input,len,L"<map name=\"refsmap\">",L"</map>",false);
	eliminate(input,len,L"<!-- Footer Links",L"<!-- Copyright -->",false);
	while (eliminate(input,len,L"<img",L">",false));
	while (eliminate(input,len,L"<IMG",L">",false));
	eliminate(input,len,L"<!-- End results area",L"</table>",false);
	eliminate(input,len,L"<em>",L"</em>",false);
	eliminate(input,len,L"<head>",L"</head>",false);
	eliminate(input,len,L"bgcolor",L"cellspacing=\"0\"",false);
	while (eliminate(input,len,L"<td",L">",false));
	while (eliminate(input,len,L"</td>"));
	while (eliminate(input,len,L"</tr>"));
	while (eliminate(input,len,L"<tr>"));
	//while (eliminate(input,len,"<font size=",">",false));
	eliminate(input,len,L"To select an entry, click on it. Click 'Go' if one entry is listed.");
	eliminate(input,len,L"Double-click any word in the entry to see its definition.");
	eliminate(input,len,L"Citation format for this entry:");
	eliminate(input,len,L"Merriam-Webster, 2002.");
	eliminate(input,len,L"http://unabridged.merriam-webster.com");
	//while (eliminate(input,len,"</font>"));
	//eliminate(input,len,"<a href=\"unabridged",">",false);
	eliminate(input,len,L"(<script",L"src=\"/date.js\"></script>)",false);
	eliminate(input,len,L"Click",L"Lists",false);

	unsigned int newlen=0;
	for (int I=0; I<len; I++)
	{
		if ((input[I+0]==9 || input[I+0]==10) && (input[I+1]==9 || input[I+1]==10))
		{
			I++;
			continue;
		}
		input[newlen++]=input[I];
	}
	len=newlen;
	input[len]=0;
	buffer=input;
	free(input);
	return true;
}

void WordClass::reduceDanielWebsterListwordPage(wstring &buffer)
{
	wchar_t *input=wcsdup(buffer.c_str());
	int len=buffer.length();
	eliminate(input,len,L"<script language=\"javascript\"",L"</script>",false);
	eliminate(input,len,L"<!-- Begin hat -->",L"<!-- End hat -->",false);
	eliminate(input,len,L"<!-- Begin logo",L"<!-- End logo, title -->",false);
	eliminate(input,len,L"<map name=\"tabsmap2\">",L"</map>",false);
	eliminate(input,len,L"<map name=\"refsmap\">",L"</map>",false);
	eliminate(input,len,L"<!-- Footer Links",L"<!-- Copyright -->",false);
	while (eliminate(input,len,L"<img",L">",false));
	while (eliminate(input,len,L"<IMG",L">",false));
	eliminate(input,len,L"<!-- End results area",L"</table>",false);
	eliminate(input,len,L"<em>",L"</em>",false);
	eliminate(input,len,L"<head>",L"</head>",false);
	while (eliminate(input,len,L"<td",L">",false));
	while (eliminate(input,len,L"</td>"));
	while (eliminate(input,len,L"</tr>"));
	while (eliminate(input,len,L"<tr>"));
	//while (eliminate(input,len,"<font",">",false));
	eliminate(input,len,L"To select an entry, click on it. Click 'Go' if one entry is listed.");
	eliminate(input,len,L"Double-click any word in the entry to see its definition.");
	eliminate(input,len,L"Citation format for this entry:");
	eliminate(input,len,L"Merriam-Webster, 2002.");
	eliminate(input,len,L"http://unabridged.merriam-webster.com");
	//while (eliminate(input,len,"</font>"));
	eliminate(input,len,L"(<script",L"src=\"/date.js\"></script>)",false);
	eliminate(input,len,L"bgcolor=\"#000066\"");
	eliminate(input,len,L"bgcolor=\"#FFFFFF\"");
	eliminate(input,len,L"bgcolor=\"white\"");
	unsigned int newlen=0;
	for (int I=0; I<len; I++)
	{
		if ((input[I+0]==9 || input[I+0]==10) && (input[I+1]==9 || input[I+1]==10))
		{
			I++;
			continue;
		}
		input[newlen++]=input[I];
	}
	len=newlen;
	input[len]=0;
	buffer=input;
	free(input);
}


int WordClass::parseMerriamWebsterDictionaryPage(wstring buffer,tIWMM &iWord,wstring sWord,wstring desiredForm,vector <wstring> &pastPages,bool recursive,int sourceId)
{
	wstring match,lastOptionsList,mainEntry,Form;
	wstring temp,temp2,alternateForm,alternate,alternateTemp;
	int ret;
	while (takeLastMatch(buffer,L"<table ",L"</table>",match,false)==0)
	{
		if (log_net) lplog(L"******\n%s\n****",match.c_str());
		if (match.find(L"save log-in information")!=wstring::npos)
			lplog(LOG_FATAL_ERROR,L"Merriam Webster dictionary subscription is not valid on this machine (or log-in required).");
		if (match.find(L"Main Entry:")!=wstring::npos)
		{
			wstring mainEntry;
			if (lastOptionsList.empty()) takeLastMatch(match,L"<input type=hidden name=list value=",L">",lastOptionsList,false);
			if (ret=processMainEntryBlock(iWord,sWord,match,desiredForm,pastPages,mainEntry,sourceId)) return ret;
			if (lastOptionsList.length() && !recursive) // && forms.size()<=1) // don't do an option list within an option list
				if (ret=processOptionsList(iWord,sWord,match,lastOptionsList,pastPages,sourceId)) return ret;
			//if (iWord==WMM.end() || !iWord->second.formsSize())
			//{
				processEndForm(iWord,sWord,match,mainEntry,sourceId);
			//}
			return 0;
		}
		else if ((match.find(L"The word you've entered isn't in the ")!=wstring::npos) ||
			(match.find(L"No entries found that match")!=wstring::npos))
		{
			//if capitalized, make a proper noun (this was at the beginning of a sentence)
			return WORD_NOT_FOUND;
		}
		else if (iWord==WMM.end())
			takeLastMatch(match,L"<input type=hidden name=list value=",L">",lastOptionsList,false);
	}
	removeRedundantSpace(buffer);
	lplog(LOG_DICTIONARY,L"ERROR MW:** Word %s resulted in an unparsable page.",sWord.c_str());
	if (extraDetailedLogging)
		lplog(LOG_DICTIONARY,L"ERROR MW:** page:\n%s\n",buffer.c_str());
	return UNPARSABLE_PAGE;
}

int WordClass::processAlternateForms(wstring sWord,wstring mainEntry,wstring form,tIWMM &iWord,int sourceId)
{
	if (form.find(L"plural noun")!=wstring::npos || form.find(L"noun plural")!=wstring::npos)
		return checkAdd(L"ALTF",iWord,sWord,0,L"noun",PLURAL,0,sWord,sourceId);
	if (form==L"verb past")
		return checkAdd(L"ALTF",iWord,sWord,0,L"verb",VERB_PAST,0,sWord,sourceId);
	if (form==L"verbal auxiliary")
		return checkAdd(L"ALTF",iWord,sWord,0,L"verbal_auxiliary",0,0,sWord,sourceId);
	if (form.find(L"suffix")!=wstring::npos)
	{
		lplog(LOG_DICTIONARY,L"Suffix form encountered %s (ignored) in options processing of word %s ...",form.c_str(),sWord.c_str());
		return 0;
	}
	if (form.find(L"abbreviation or noun")!=wstring::npos)// lplog
	{
		checkAdd(L"ALTF",iWord,sWord,0,L"noun",SINGULAR,0,sWord,sourceId);
		return checkAdd(L"ALTF",iWord,sWord,0,L"abbreviation",0,0,sWord,sourceId);
	}
	if (form.find(L"adjective & adverb")!=wstring::npos || // rare
		form.find(L"adjective (or adverb)")!=wstring::npos || // crescendo
		form.find(L"adverb (or adjective)")!=wstring::npos)// agate , below
	{
		checkAdd(L"ALTF",iWord,sWord,0,L"adjective",ADJECTIVE_NORMATIVE,0,sWord,sourceId);
		return checkAdd(L"ALTF",iWord,sWord,0,L"adverb",ADVERB_NORMATIVE,0,sWord,sourceId);
	}
	if (form.find(L"adjective or noun")!=wstring::npos || // auto
		form.find(L"noun or adjective")!=wstring::npos)// nip
	{
		checkAdd(L"ALTF",iWord,sWord,0,L"adjective",ADJECTIVE_NORMATIVE,0,sWord,sourceId);
		return checkAdd(L"ALTF",iWord,sWord,0,L"noun",SINGULAR,0,sWord,sourceId);
	}
	if (form.find(L"adjective%2Ccomparative of")!=wstring::npos)// worse
		return checkAdd(L"ALTF",iWord,sWord,0,L"adjective",ADJECTIVE_COMPARATIVE,0,sWord,sourceId);
	if (form.find(L"adverb%2Ccomparative of")!=wstring::npos)// worse
		return checkAdd(L"ALTF",iWord,sWord,0,L"adverb",ADVERB_COMPARATIVE,0,sWord,sourceId);
	if (form.find(L"noun or verb")!=wstring::npos)// bike
	{
		checkAdd(L"ALTF",iWord,sWord,0,L"noun",SINGULAR,0,sWord,sourceId);
		return checkAdd(L"ALTF",iWord,sWord,0,L"verb",VERB_PRESENT_FIRST_SINGULAR,0,sWord,sourceId);
	}
	if (form.find(L"definite article")!=wstring::npos)// ta
		return checkAdd(L"ALTF",iWord,sWord,0,L"determiner",0,0,sWord,sourceId);
	if (form.find(L"indefinite article")!=wstring::npos)// some
		return checkAdd(L"ALTF",iWord,sWord,0,L"quantifier",0,0,sWord,sourceId);
	if (form.find(L"noun%2Cplural in construction")!=wstring::npos)// few
		return checkAdd(L"ALTF",iWord,sWord,0,L"noun",PLURAL,0,sWord,sourceId);
	if (form.find(L"noun%2Csingular or plural in construction")!=wstring::npos)// needy
		return checkAdd(L"ALTF",iWord,sWord,0,L"noun",PLURAL|SINGULAR,0,sWord,sourceId);
	if (form.find(L"pronoun%2Cplural in construction")!=wstring::npos)// divers
		return checkAdd(L"ALTF",iWord,sWord,0,L"pronoun",PLURAL,0,sWord,sourceId);
	if (form.find(L"pronoun%2Csingular or plural in construction")!=wstring::npos ||// most
		form.find(L"pronoun%2Csometimes plural in construction")!=wstring::npos)// other
		return checkAdd(L"ALTF",iWord,sWord,0,L"pronoun",PLURAL,0,sWord,sourceId);
	if (form.find(L"verb imperative")!=wstring::npos)// coop
		return checkAdd(L"ALTF",iWord,sWord,0,L"verb",VERB_PRESENT_FIRST_SINGULAR,0,sWord,sourceId);
	return -1;
}

void transformList(bool mainEntryHasSpace,bool mainEntryHasDash,wstring oldList,wstring &newList,bool medical)
{
	wstring option;
	bool hasSpace=false,hasDash=false,pastBracket=false,firstEntryHit=false;
	for (unsigned int I=1; I<oldList.size()-1; I++) // oldList starts with " and ends with "
		switch (oldList[I])
	{
		case '+':option+=L"%2B"; break;
		case ' ':option+=L"%2B"; if (!pastBracket) hasSpace=true; break;
		case '[':option+=L"%5B"; pastBracket=true; break;
		case ',':option+=L"%2C"; break;
		case ']':option+=L"%5D"; break;
		case '=':option+=L"%3D"; break;
		case '-':option+=L"-"; if (!pastBracket) hasDash=true; break;
		case ';':option+=L"%3B";
			if (newList.length()+option.length()>1024) break; // limitation of WinInet path?
			if (firstEntryHit && (medical || (!(hasSpace ^ mainEntryHasSpace) &&	!(hasDash ^ mainEntryHasDash))))
				newList+=option;
			//else
			//  LOGFULL("Rejecting option %s.",option.c_str());
			firstEntryHit=true;
			option=L"";
			hasSpace=false;
			hasDash=false;
			pastBracket=false;
			break;
		default: option+=oldList[I];
	}
	if (option.length())
	{
		if (newList.length()+option.length()>1024) return; // limitation of WinInet path?
		if (medical || (!(hasSpace ^ mainEntryHasSpace) &&	!(hasDash ^ mainEntryHasDash)))
			newList+=option;
		//else
		//  LOGFULL("Rejecting option %s.",option.c_str());
	}
	if (newList.length()>3 && newList.substr(newList.length()-3,3)==L"%3B")
		newList.erase(newList.length()-3,3);
}

int WordClass::processOptionsList(tIWMM &iWord,wstring sWord,wstring match,wstring lastOptionsList,vector <wstring> &pastPages,int sourceId)
{
	if (log_net) lplog(L"******\n%s\n****",match.c_str());
	wstring firstMainEntry;
	wstring list;
	bool medical=(match.find(L"<input type=hidden name=book value=Medical>")!=wstring::npos);
	bool collegiate=(match.find(L"<input type=hidden name=book value=Dictionary>")!=wstring::npos);
	transformList((sWord.find(L" ")!=wstring::npos),(sWord.find(L"-")!=wstring::npos),lastOptionsList,list,medical);
	/*
	int nextpos=lastOptionsList.find(L"<",temppos=formpos+wcslen("<option>"));
	if (nextpos==wstring::npos) break;
	wstring option=lastOptionsList.substr(temppos,nextpos-temppos-1);
	int pos2=option.find('['),pos3=wstring::npos,pos4=wstring::npos;
	if (pos2!=wstring::npos) pos3=option.find(',',pos2+1);
	if (pos3!=wstring::npos) pos4=option.find(',',pos3+1); // may have an instruction like ",plural in construction" (sWord:few)
	if (pos4!=wstring::npos &&
	processInstruction(iWord,sWord,option.substr(0,pos2),option.substr(pos3+1,pos4-pos3-1),option.substr(pos4+1,option.find(']',pos4+1)-pos4-1),Forms,Inflections))
	{
	startpos=formpos+wcslen("<option>");
	continue;
	}
	if (pos4==wstring::npos) pos4=option.find(']',pos3+1);
	if (pos2!=wstring::npos && pos3!=wstring::npos && pos4!=wstring::npos)
	{
	if (!firstMainEntry[0]) firstMainEntry=option.substr(0,pos2);
	//else if (firstMainEntry!=option.substr(0,pos2) && sWord!=option.substr(0,pos2)) break;
	wstring newWord=option.substr(0,pos2);
	wstring aForm=option.substr(pos3+1,pos4-pos3-1);
	*/
	for (int startpos=0; startpos!=wstring::npos; startpos=list.find(L"%3B",startpos))
	{
		if (startpos>0) startpos+=wcslen(L"%3B"); // beginning of next entry
		int nextpos=list.find(L"%3B",startpos);
		wstring option,newWord,form,jump;
		if (nextpos==wstring::npos)
			option=list.substr(startpos,list.length()-startpos);
		else
			option=list.substr(startpos,nextpos-startpos);
		if (!option.length()) break;
		if (parseOption(option,newWord,form,jump))
		{
			lplog(LOG_DICTIONARY,L"ERROR:Incorrect option wstring %s at position %d-%d in wstring %s.",option.c_str(),startpos,nextpos,list.c_str());
			continue;
		}
		//lplog(L"%d: %s",webAddress.length(),webAddress.c_str());
		if (!medical && (!spaceSame(sWord,newWord) || towlower(sWord[0])!=towlower(newWord[0]))) continue; // sWord must have same # of spaces as option
		if (medical && newWord.find(sWord)==wstring::npos && sWord.find(newWord)==wstring::npos) continue;
		wstring webAddress;
		wstring bookType=L"Third";
		if (medical)
			bookType=L"Medical";
		if (collegiate)
			bookType=L"Dictionary";
		webAddress=L"http://unabridged.merriam-webster.com/cgi-bin/unabridged?srcho=va&hdwd="+sWord+L"&listword="+sWord+L"&book="+bookType+L"&jump="+jump+L"&list="+list;
		if (find(pastPages.begin(),pastPages.end(),webAddress)!=pastPages.end()) continue;
		pastPages.push_back(webAddress);
		wstring buffer;
		if (form==L"verb" || form==L"noun" || form==L"adjective" || form==L"adverb" ||
			form==L"transitive verb" || form==L"intransitive verb" || form==L"")
		{
			int ret;
			if (ret=getWebsterDictionaryPage(webAddress,L"",buffer,true)) continue;
			if (ret=parseMerriamWebsterDictionaryPage(buffer,iWord,sWord,form,pastPages,true,sourceId))
			{
				lplog(LOG_DICTIONARY,L"Option processing of the option %s failed.",newWord.c_str());
				continue;
			}
		}
		else if (form.find(L"plural noun")!=wstring::npos || form.find(L"noun plural")!=wstring::npos)
		{
			if (equivalentIfIgnoreDashSpaceCase(sWord,newWord)) checkAdd(L"pol",iWord,sWord,0,L"noun",PLURAL,0,sWord,sourceId);
		}
		else if (form==L"verb past")
		{
			if (equivalentIfIgnoreDashSpaceCase(sWord,newWord)) checkAdd(L"pol",iWord,sWord,0,L"verb",VERB_PAST,0,sWord,sourceId);
		}
		else if (form==L"verbal auxiliary" || form==L"interjection" || form==L"preposition" || form==L"pronoun" || form==L"conjunction" || form==L"abbreviation")
		{
			if (form==L"verbal auxiliary") form=L"verbal_auxiliary";
			if (newWord==sWord) checkAdd(L"pol",iWord,sWord,0,form,0,0,sWord,sourceId);
		}
		else if (form.find(L"suffix")!=wstring::npos)
		{
			lplog(LOG_DICTIONARY,L"Suffix form encountered %s (ignored) in options processing of word %s (%s)...",form.c_str(),sWord.c_str(),newWord.c_str());
		}
		else
		{
			if (equivalentIfIgnoreDashSpaceCase(sWord,newWord))
			{
				if (form.find(L"abbreviation or noun")!=wstring::npos)// lplog
				{
					checkAdd(L"pol",iWord,sWord,0,L"noun",SINGULAR,0,sWord,sourceId);
					checkAdd(L"pol",iWord,sWord,0,L"abbreviation",0,0,sWord,sourceId);
				}
				else if (form.find(L"adjective & adverb")!=wstring::npos || // rare
					form.find(L"adjective (or adverb)")!=wstring::npos || // crescendo
					form.find(L"adverb (or adjective)")!=wstring::npos)// agate , below
				{
					checkAdd(L"pol",iWord,sWord,0,L"adjective",ADJECTIVE_NORMATIVE,0,sWord,sourceId);
					checkAdd(L"pol",iWord,sWord,0,L"adverb",ADVERB_NORMATIVE,0,sWord,sourceId);
				}
				else if (form.find(L"adjective or noun")!=wstring::npos || // auto
					form.find(L"noun or adjective")!=wstring::npos)// nip
				{
					checkAdd(L"pol",iWord,sWord,0,L"adjective",ADJECTIVE_NORMATIVE,0,sWord,sourceId);
					checkAdd(L"pol",iWord,sWord,0,L"noun",SINGULAR,0,sWord,sourceId);
				}
				else if (form.find(L"adjective%2Ccomparative of")!=wstring::npos)// worse
					checkAdd(L"pol",iWord,sWord,0,L"adjective",ADJECTIVE_COMPARATIVE,0,sWord,sourceId);
				else if (form.find(L"adverb%2Ccomparative of")!=wstring::npos)// worse
					checkAdd(L"pol",iWord,sWord,0,L"adverb",ADVERB_COMPARATIVE,0,sWord,sourceId);
				else if (form.find(L"noun or verb")!=wstring::npos)// bike
				{
					checkAdd(L"pol",iWord,sWord,0,L"noun",SINGULAR,0,sWord,sourceId);
					checkAdd(L"pol",iWord,sWord,0,L"verb",VERB_PRESENT_FIRST_SINGULAR,0,sWord,sourceId);
				}
				else if (form.find(L"definite article")!=wstring::npos)// ta
					checkAdd(L"pol",iWord,sWord,0,L"determiner",0,0,sWord,sourceId);
				else if (form.find(L"indefinite article")!=wstring::npos)// some
					checkAdd(L"pol",iWord,sWord,0,L"quantifier",0,0,sWord,sourceId);
				else if (form.find(L"noun%2Cplural in construction")!=wstring::npos)// few
				{
					checkAdd(L"pol",iWord,sWord,0,L"noun",PLURAL,0,sWord,sourceId);
				}
				else if (form.find(L"noun%2Csingular or plural in construction")!=wstring::npos)// needy
				{
					checkAdd(L"pol",iWord,sWord,0,L"noun",PLURAL|SINGULAR,0,sWord,sourceId);
				}
				else if (form.find(L"pronoun%2Cplural in construction")!=wstring::npos)// divers
				{
					checkAdd(L"pol",iWord,sWord,0,L"pronoun",PLURAL,0,sWord,sourceId);
				}
				else if (form.find(L"pronoun%2Csingular or plural in construction")!=wstring::npos)// most
					//form.find(L"pronoun%2Csometimes plural in construction")!=wstring::npos)// other is NOT actually plural - others is plural!
				{
					checkAdd(L"pol",iWord,sWord,0,L"pronoun",PLURAL,0,sWord,sourceId);
				}
				else if (form.find(L"verb imperative")!=wstring::npos)// coop
				{
					checkAdd(L"pol",iWord,sWord,0,L"verb",VERB_PRESENT_FIRST_SINGULAR,0,sWord,sourceId);
				}
				else
					lplog(LOG_DICTIONARY,L"ERROR:Unrecognized form %s in options processing of word %s (%s)...\n",form.c_str(),sWord.c_str(),newWord.c_str());
			}
		}
	}
	return 0;
}

int findFunction(wstring sInflection,wstring &Form)
{
	wchar_t *lookFor[]={L"intransitive verb",L"transitive verb",L"noun",L"verb",L"adjective",L"adverb",NULL};
	takeLastMatch(sInflection,L"<i>",L"</i>",Form,false);
	for (int I=0; lookFor[I]; I++) if (!wcscmp(Form.c_str(),lookFor[I])) return 0;
	Form=L"";
	return -1;
}

int WordClass::processMainEntryBlock(tIWMM &iWord,wstring sWord,wstring match,wstring desiredForm,vector <wstring> &pastPages,wstring &mainEntry,int sourceId)
{
	bool medical=(match.find(L"<input type=hidden name=book value=Medical>")!=wstring::npos);
	vector <wstring> variantMainEntries;
	wstring Form,temp,sInflection,inflectionName;
	bool inflection_pp=false,inflection_pts=false,inflection_pss=false,inflection_p=false,inflection_ppp=false,inflection_prp=false;
	bool inflection_c=false,inflection_s=false,noun_p=false,noun_s=false;
	findMainEntry(match,mainEntry);
	takeLastMatch(match,L"Inflected Form(s):",L"<br>",sInflection,false);
	findFunction(sInflection,Form);
	if (takeLastMatch(match,L"Variant(s):\09",L"<br>",temp,false)==0)
	{
		wstring variantMainEntry;
		while (!takeLastMatch(temp,L"<b>",L"</b>",variantMainEntry,false))
		{
			removeDots(variantMainEntry);
			variantMainEntries.push_back(variantMainEntry);
		}
	}
	if (takeLastMatch(match,L"Function:\09",L"<br>",temp,false)==0 || Form.length())
	{
		if (Form.empty()) takeLastMatch(temp,L"<i>",L"</i>",Form,false);
		if (Form==L"prefix" || Form==L"combining form")
			Form=L"adjective"; // not a word, but still kind of a unit of meaning klepto, narco, etc. see prefixes in initializeDictionary
		if (Form.find(L"noun")!=wstring::npos) Form=L"noun";
		//      if (Form.find(L"verb")!=wstring::npos)) Form=L"verb";
		if (desiredForm.length() && Form!=desiredForm)
			lplog(LOG_DICTIONARY,L"ERROR:Parsing word %s, parent thought form was %s, but child page returned a form of %s.",sWord.c_str(),desiredForm.c_str(),Form.c_str());
		generateInflections(sWord,mainEntry,Form,iWord,sInflection,temp,sourceId,medical);
		for (unsigned int ve=0; ve<variantMainEntries.size(); ve++) generateInflections(sWord,variantMainEntries[ve],Form,iWord,sInflection,temp,sourceId,medical);
		return 0;
	}
	if (takeLastMatch(match,L"<i>transitive verb",L"</i>",temp,false)==0)
	{
		Form=L"transitive verb";
		generateInflections(sWord,mainEntry,Form,iWord,sInflection,temp,sourceId,medical);
		for (unsigned int ve=0; ve<variantMainEntries.size(); ve++) generateInflections(sWord,variantMainEntries[ve],Form,iWord,sInflection,temp,sourceId,medical);
		return 0;
	}
	if (takeLastMatch(match,L"<i>intransitive verb",L"</i>",temp,false)==0)
	{
		Form=L"intransitive verb";
		generateInflections(sWord,mainEntry,Form,iWord,sInflection,temp,sourceId,medical);
		for (unsigned int ve=0; ve<variantMainEntries.size(); ve++) generateInflections(sWord,variantMainEntries[ve],Form,iWord,sInflection,temp,sourceId,medical);
		return 0;
	}
	match=match+sInflection+L"</a>"; // add inflection back in in case it contains inflection hints
	//<i>variant of</i><a href=L"unabridged?book=Third&va=make"><font size=L"-1">MAKE</font></a>
	//<i>variant of</i><<a href=L"unabridged-tb?book=Third&va=sup"><font size=L"-1">SUP</font></a>>1</sup><a href=L"unabridged-tb?book=Third&va=mead"><font size=L"-1">MEAD</font></a>
	//<i>present third singular of</i>
	//<i>past of</i>
	//<<a href=L"unabridged-tb?book=Third&va=sup"><font size=L"-1">SUP</font></a>>1</sup>
	//<i>British variant of</i><a href=L"collegiate?book=Dictionary&va=harmonize"><font size=L"-1">HARMONIZE</font></a>
	takeLastMatch(match,L"<<a href=L",L"</sup>",temp,false);
	if (takeLastMatch(match,L"variant of</i>",L"</a>",temp,false)==0)
	{
		wstring variant;
		if (takeLastMatch(temp,L"<font size=\"-1\">",L"</font>",variant,false)==0)
		{
			wcslwr((wchar_t *)variant.c_str());
			tIWMM iWordVariant=query(variant);
			if (iWordVariant==WMM.end() || (iWordVariant->second.flags&tFI::queryOnAnyAppearance))
			{
				wstring buffer;
				int ret;
				// prevent infinite loops by having two undefined variants refer to each other.
				if (query(sWord)==WMM.end())
					pair < tIWMM, bool > pr=WMM.insert(tWFIMap(sWord,tFI()));
				if (ret=getWebsterDictionaryPage(variant,L"",buffer,false)) return ret;
				if (parseMerriamWebsterDictionaryPage(buffer,iWordVariant,variant,L"",pastPages,false,sourceId) || iWordVariant==WMM.end()) return 0;
				iWordVariant->second.flags&=~tFI::queryOnAnyAppearance;
			}
			bool added;
			if (iWordVariant->second.query(UNDEFINED_FORM_NUM)<0)
				iWord=addCopy(sWord,iWordVariant,added);
			else
				lplog(LOG_DICTIONARY,L"Rejected unknown word %s.",iWordVariant->first.c_str());
			return 0;
		}
	}
	else if ((inflection_ppp=(takeLastMatch(match,L"<i>past and past participle of</i>",L"</a>",temp,false)==0)) ||
		(inflection_pts=(takeLastMatch(match,L"<i>present third singular of</i>",L"</a>",temp,false)==0)) ||
		(inflection_pts=(takeLastMatch(match,L"<i>third singular of</i>",L"</a>",temp,false)==0)) ||
		(inflection_pts=(takeLastMatch(match,L"<i>present 3rd singular of</i>",L"</a>",temp,false)==0)) ||
		(inflection_pss=(takeLastMatch(match,L"<i>present second singular of</i>",L"</a>",temp,false)==0)) ||
		(inflection_p=(takeLastMatch(match,L"<i>past of",L"</a>",temp,false)==0)) ||
		(inflection_p=(takeLastMatch(match,L"<i>past second singular of",L"</a>",temp,false)==0)) ||
		(inflection_p=(takeLastMatch(match,L"<i>nonstandard past of",L"</a>",temp,false)==0)) ||
		(inflection_p=(takeLastMatch(match,L"<i>archaic past of</i>",L"</a>",temp,false)==0)) ||
		(inflection_p=(takeLastMatch(match,L"<i>now dialect past of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past part & nonstandard past of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past part & South past of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past & chiefly dialect past part of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past part or dialect past of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past or dialect past part of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past or chiefly dialect past part of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past or nonstandard past part of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past or obs past part of</i>",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past and nonstandard past part of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past & archaic past part of",L"</a>",temp,false)==0)) ||
		(inflection_ppp=(takeLastMatch(match,L"<i>past tense and dialect past part of",L"</a>",temp,false)==0)) ||
		(inflection_pp=(takeLastMatch(match,L"<i>past part of",L"</a>",temp,false)==0)) ||
		(inflection_pp=(takeLastMatch(match,L"<i>past part. of",L"</a>",temp,false)==0)) ||
		(inflection_prp=(takeLastMatch(match,L"<i>present part of",L"</a>",temp,false)==0)) ||
		(inflection_prp=(takeLastMatch(match,L"<i>present part. of",L"</a>",temp,false)==0)))
	{
		int inflection=0;
		if (inflection_ppp) inflection=VERB_PAST_PARTICIPLE; // and VERB_PAST
		else if (inflection_pts) inflection=VERB_PRESENT_THIRD_SINGULAR;
		else if (inflection_pss) inflection=VERB_PRESENT_SECOND_SINGULAR;
		else if (inflection_p) inflection=VERB_PAST;
		else if (inflection_pp) inflection=VERB_PAST_PARTICIPLE;
		else if (inflection_prp) inflection=VERB_PRESENT_PARTICIPLE;
		// get definition entry temp like <a href=L"unabridged?book=Third&va=wood"><font size=L"-1">WOOD</font>

		if (equivalentIfIgnoreDashSpaceCase(sWord,mainEntry))
		{
			wstring definitionEntry;
			if (takeLastMatch(temp,L"<font size=\"-1\">",L"</font>",definitionEntry,false)!=0)
				lplog(LOG_DICTIONARY,L"processMainEntryBlock verb inflection pattern not found! with word %s.",sWord.c_str());
			else
			{
				Form=L"verb";
				checkAdd(L"piv",iWord,mainEntry,0,Form,inflection,0,definitionEntry,sourceId);
				if (inflection_ppp) checkAdd(L"piv",iWord,mainEntry,0,Form,VERB_PAST,0,definitionEntry,sourceId);
			}
		}
		else
		{
			lplog(LOG_DICTIONARY,L"MainEntry %s: Detected Inflection (piv)%s (%s) (Form %s)",mainEntry.c_str(),getInflectionName(inflection,verbInflectionMap,inflectionName),sWord.c_str(),Form.c_str());
			if (inflection_ppp)
				lplog(LOG_DICTIONARY,L"MainEntry %s: Detected Inflection (piv)%s (%s) (Form %s)",mainEntry.c_str(),getInflectionName(VERB_PAST,verbInflectionMap,inflectionName),sWord.c_str(),Form.c_str());
		}
	}
	else if ((noun_p=(takeLastMatch(match,L"<i>plural of</i>",L"</a>",temp,false)==0)) ||
		(noun_s=(takeLastMatch(match,L"<i>singular of</i>",L"</a>",temp,false)==0)))
	{
		int inflection=(noun_p) ? PLURAL:SINGULAR;
		Form=L"noun";
		if (equivalentIfIgnoreDashSpaceCase(sWord,mainEntry))
			checkAdd(L"pn",iWord,mainEntry,0,Form,inflection,0,sWord,sourceId);
		else
			lplog(LOG_DICTIONARY,L"MainEntry %s: Detected Inflection (pn)%s (%s) (Form %s)",mainEntry.c_str(),getInflectionName(inflection,nounInflectionMap,inflectionName),sWord.c_str(),Form.c_str());
	}
	else if ((inflection_c=(takeLastMatch(match,L"<i>comparative of</i>",L"</a>",temp,false)==0)) ||
		(inflection_s=(takeLastMatch(match,L"<i>superlative of</i>",L"</a>",temp,false)==0)))
	{
		int inflection=0;
		if (inflection_c && Form==L"adjective") inflection=ADJECTIVE_COMPARATIVE;
		if (inflection_c && Form==L"adverb") inflection=ADVERB_COMPARATIVE;
		if (inflection_c && !Form.length()) inflection=ADJECTIVE_COMPARATIVE;
		if (inflection_s && Form==L"adjective") inflection=ADJECTIVE_SUPERLATIVE;
		if (inflection_s && Form==L"adverb") inflection=ADVERB_SUPERLATIVE;
		if (inflection_s && !Form.length()) inflection=ADJECTIVE_SUPERLATIVE;
		if (!Form.length()) Form=L"adjective";
		if (equivalentIfIgnoreDashSpaceCase(sWord,mainEntry))
			checkAdd(L"pa",iWord,mainEntry,0,Form,inflection,0,sWord,sourceId);
		else
			lplog(LOG_DICTIONARY,L"MainEntry %s: Detected Inflection (pa)%s (%s) (Form %s)",mainEntry.c_str(),getInflectionName(inflection,(Form==L"adjective") ? adjectiveInflectionMap:adverbInflectionMap,inflectionName),sWord.c_str(),Form.c_str());
	}
	else if (match.find(L"code word for the letter")!=wstring::npos)
	{
		checkAdd(L"cw",iWord,mainEntry,0,L"symbol",0,0,sWord,sourceId);
	}
	else
	{
		takeLastMatch(match,L"Pronunciation:",L"</a>",temp,false);  //'Citation format' was removed from cache entries
		if (temp.find(L"<i>synonym of</i>")==wstring::npos &&
			temp.find(L"<i>variant of")==wstring::npos &&
			temp.find(L"an international radiotelephone signal word")==wstring::npos &&
			match.find(L"<option>")==wstring::npos &&
			match.find(L"<i>possessive of</i>")==wstring::npos && // his, mine
			match.find(L"<i>Merriam-Webster's Collegiate Dictionary, Eleventh Edition</i>")==wstring::npos)
		{
			lplog(LOG_DICTIONARY,L"ERROR:Inflection processing of word %s in match block (sub block %s):",sWord.c_str(),temp.c_str());
			lplog(LOG_DICTIONARY,L"%s",match.c_str());
			lplog(LOG_DICTIONARY,L"\n");
		}
		if (match.find(L"<option>")==wstring::npos)
			return WORD_NOT_FOUND;
		//return INFLECTION_PROCESSING_FAILED;
	}
	return 0;
}

bool WordClass::processInstruction(tIWMM &iWord,wstring sWord,wstring newWord,wstring aForm,wstring instruction,int sourceId)
{
	int inflection=0;
	if (wcsstr(instruction.c_str(),L"comparative of"))
	{
		if (aForm==L"adjective") inflection=ADJECTIVE_COMPARATIVE;
		if (aForm==L"adverb") inflection=ADVERB_COMPARATIVE;
	}
	else if (wcsstr(instruction.c_str(),L"superlative of"))
	{
		if (aForm==L"adjective") inflection=ADJECTIVE_SUPERLATIVE;
		if (aForm==L"adverb") inflection=ADVERB_SUPERLATIVE;
	}
	else
		return false;
	wstring inflectionName;
	if (equivalentIfIgnoreDashSpaceCase(sWord,newWord))
		checkAdd(L"i",iWord,newWord,0,aForm,inflection,0,sWord,sourceId);
	else
		lplog(LOG_DICTIONARY,L"MainEntry(i) %s: Detected Inflection%s (%s) (Form %s)",newWord.c_str(),getInflectionName(inflection,(aForm==L"adjective") ? adjectiveInflectionMap:adverbInflectionMap,inflectionName),sWord.c_str(),aForm.c_str());
	return true;
}

int WordClass::generateInflections(wstring sWord,wstring mainEntry,wstring sForm,tIWMM &iWord,wstring sInflection,wstring temp,int sourceId,bool medical)
{
	if (processNamedForms(sWord,mainEntry,sForm,iWord,sInflection,sourceId)) return 0;
	if (processInflectedFunction(sWord,mainEntry,temp,sForm,iWord,sourceId)) return 0;
	vector <wstring> allInflections;
	if (sForm==L"intransitive verb" || sForm==L"transitive verb") sForm=L"verb";
	processInflections(sWord,mainEntry,sForm,sInflection,allInflections);
	if (processAlternateForms(sWord,mainEntry,sForm,iWord,sourceId)==0) return 0;
	checkAddInflections(allInflections,iWord,sWord,sForm,mainEntry,sourceId,medical);
	return 0;
}

#define MAX_LEN 2048
int WordClass::getWebsterDictionaryPage(wstring sWord,wstring form,wstring &buffer,bool wordIsAddress)
{
	wchar_t webAddress[MAX_LEN];
	wchar_t path[MAX_LEN];
	if (!unknownWDWords.size())
		readUnknownWords(unknownWD,unknownWDWords);
	if (!wordIsAddress && binary_search(unknownWDWords.begin(),unknownWDWords.end(),sWord))
	{
		buffer=L"<table >No entries found that match</table>";
		return 0;
	}
	if (wordIsAddress)
	{
		_snwprintf(path,MAX_LEN-10,L"%s\\pagecache\\_%s.txt",cacheDir,wcsstr(sWord.c_str(),L"listword="));
		wchar_t *ch=wcsstr(path,L"&list=");
		if (ch) *ch=0;
		convertIllegalChars(path+wcslen(cacheDir)+wcslen(L"\\pagecache\\"));
		distributeToSubDirectories(path,wcslen(cacheDir)+wcslen(L"\\pagecache\\"),false);
	}
	else
	{
		_snwprintf(path,MAX_LEN-10,L"%s\\pagecache\\_%s.%s.txt",cacheDir,sWord.c_str(),form.c_str());
		convertIllegalChars(path+wcslen(cacheDir)+wcslen(L"\\pagecache\\"));
		distributeToSubDirectories(path,wcslen(cacheDir)+wcslen(L"\\pagecache\\"),false);
	}
	wcscat(path,L".UTF8");
	wchar_t cBuffer[MAX_BUF];
	int actualLenInBytes,ret;
	if (!getPath(path,(void *)cBuffer,MAX_BUF,actualLenInBytes))
	{
		cBuffer[actualLenInBytes/sizeof(cBuffer[0])]=0;
		buffer=cBuffer;
		if (buffer.find(L"save log-in information")!=wstring::npos)
		{
			_wremove(path);
			return getWebsterDictionaryPage(sWord,form,buffer,wordIsAddress);
		}
		if (buffer.find(L"can be found in <i>Merriam-Webster's Collegiate Dictionary",0)==wstring::npos && 
				buffer.find(L"can be found in <i>Merriam-Webster's Medical Dictionary",0)==wstring::npos)
			return 0;
	}
	else
	{
		if (wordIsAddress)
			_snwprintf(webAddress,MAX_LEN,L"%s",sWord.c_str());
		else
		{
			_snwprintf(webAddress,MAX_LEN,L"http://unabridged.merriam-webster.com/cgi-bin/unabridged?va=%s",sWord.c_str());
			if (form.length())
				_snwprintf(webAddress+wcslen(webAddress),MAX_LEN-wcslen(webAddress),L"&fl=%s",form.c_str());
		}
		if (ret=readPage(webAddress,buffer)) return ret;
	}
	// if redirected to the collegiate dictionary...
	// <b>achilles</b> can be found in <i>Merriam-Webster's Collegiate Dictionary, Eleventh Edition</i>.  <a href="/cgi-bin/collegiate?book=Dictionary&va=achilles">View the complete definition</a>. 	</form>	<br><br><font face="Arial" size="-1"><font color="000066"></font><br>	<br>	"achilles ." .   .</font><br>	<br>&nbsp;	&nbsp;	</table>	</table>	<br>
	if (buffer.find(L"can be found in <i>Merriam-Webster's Collegiate Dictionary",0)!=wstring::npos)
	{
		// <a href="/cgi-bin/collegiate?book=Dictionary&va=achilles">View the complete definition</a>. 	</form>	<br><br><font face="Arial" size="-1"><font color="000066"></font><br>	<br>	"achilles ." .   .</font><br>	<br>&nbsp;	&nbsp;	</table>	</table>	<br>
		// http://unabridged.merriam-webster.com/cgi-bin/collegiate?book=Dictionary&va=achilles
		wstring match;
		if (!takeLastMatch(buffer,L"<a href=\"/cgi-bin/collegiate?book=Dictionary&va=",L"\">",match,false))
		{
			_snwprintf(webAddress,MAX_LEN,L"http://unabridged.merriam-webster.com/cgi-bin/collegiate?book=Dictionary&va=%s",match.c_str());
			buffer.clear();
			if (ret=readPage(webAddress,buffer)) return ret;
		}
	}
	// if redirected to the medical dictionary...
	// <b>asthmaticus</b> can be found in <i>Merriam-Webster's Medical Dictionary</i>.  <a href="/cgi-bin/medical?book=Medical&va=asthmaticus">View the complete definition</a>. 	</form>	<br><br><font face="Arial" size="-1"><font color="000066"></font><br>	<br>	"asthmaticus ." .   .</font><br>	<br>&nbsp;	&nbsp;	</a>
	if (buffer.find(L"can be found in <i>Merriam-Webster's Medical Dictionary",0)!=wstring::npos)
	{
		// http://unabridged.merriam-webster.com/cgi-bin/medical?book=Medical&va=asthmaticus
		wstring match;
		if (!takeLastMatch(buffer,L"<a href=\"/cgi-bin/medical?book=Medical&va=",L"\">",match,false))
		{
			_snwprintf(webAddress,MAX_LEN,L"http://unabridged.merriam-webster.com/cgi-bin/medical?book=Medical&va=%s",match.c_str());
			buffer.clear();
			if (ret=readPage(webAddress,buffer)) return ret;
		}
	}
	if (buffer.find(L"To enter the subscription area requires the user name and password",0)!=wstring::npos)
	{
		return -1;
	}
	bool writeFile=true;
	if (wordIsAddress)
		reduceDanielWebsterListwordPage(buffer);
	else
		writeFile=reduceDanielWebsterPage(sWord,buffer);
	int fd=_wopen(path,O_CREAT|O_RDWR|O_BINARY,_S_IREAD | _S_IWRITE );
	if (fd<0)
	{
		path[wcslen(cacheDir)+wcslen(L"\\pagecache\\")+1]=0;
		_wmkdir(path);
		path[wcslen(cacheDir)+wcslen(L"\\pagecache\\")+1]='\\';
		path[wcslen(cacheDir)+wcslen(L"\\pagecache\\")+3]=0;
		_wmkdir(path);
		path[wcslen(cacheDir)+wcslen(L"\\pagecache\\")+3]='\\';
		if ((fd=_wopen(path,O_CREAT|O_RDWR|O_BINARY,_S_IREAD | _S_IWRITE ))<0)
		{
			lplog(LOG_ERROR,L"ERROR:Cannot create path %s - %S (1).",path,sys_errlist[errno]);
			return GETPAGE_CANNOT_CREATE;
		}
	}
	//LOGNET("Writing web page cache file %s.",path);
	if (writeFile)
		_write(fd,buffer.c_str(),buffer.length()*sizeof(buffer[0]));
	_close(fd);
	return 0;
}
