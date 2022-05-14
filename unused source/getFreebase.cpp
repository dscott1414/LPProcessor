#include <windows.h>
#include <io.h>
#include "word.h"
#include "source.h"
#include <fcntl.h>
#include "sys/stat.h"
#include "direct.h"
#include "time.h"
#include <errno.h>
#include <Winhttp.h>
#include <functional>
//#include <locale>
#include "profile.h"
#include "mysql.h"
#include "ontology.h"
#include <string>
bool myquery(MYSQL *mysql,wchar_t *q,bool allowFailure=false);
bool myquery(MYSQL *mysql,wchar_t *q,MYSQL_RES * &result,bool allowFailure=false);

extern "C"
{ 
#include "yajl_tree.h"
}

int getFreebasePath(int where,wstring webAddress,wstring &buffer,wstring epath,bool forceWebReread)
{ LFS
	//int timer=clock(); 	
	int bw=-1;
	while ((bw=epath.find_first_of(L"/*?\"<>|,"))!=wstring::npos)
		epath[bw]=L'_';
	wstring filePathOut,headers;
	int retValue=getWebPath(where,webAddress,buffer,epath,L"dbPediaCache",filePathOut,headers,0,false,true, forceWebReread);
	if (buffer.find(L"Uh oh! You found a bug!")!=wstring::npos)
	{
		_wremove(filePathOut.c_str());
		buffer.clear();
		lplog(LOG_WHERE|LOG_WIKIPEDIA,L"FreeBase API bug with URL:\n%s",webAddress.c_str());
		return -1;
	}
	if (buffer.find(L"The requested URL could not be retrieved")!=wstring::npos)
	{
		_wremove(filePathOut.c_str());
		buffer.clear();
		lplog(LOG_WHERE|LOG_WIKIPEDIA,L"FreeBase URL could not be retrieved with URL:\n%s",webAddress.c_str());
		return -1;
	}
	if (buffer.find(L"Freebase API service")!=wstring::npos)
	{
		_wremove(filePathOut.c_str());
		buffer.clear();
		retValue=getWebPath(where,webAddress,buffer,epath,L"dbPediaCache",filePathOut,headers,0,false,true,forceWebReread);
		//if (buffer.find(L"Freebase API service ")!=wstring::npos)
		//	lplog(LOG_FATAL_ERROR|LOG_WHERE|LOG_WIKIPEDIA,L"FreeBase API suspended with URL:\n%s",webAddress.c_str());
	}
	return retValue;
}

        struct {
            const char **keys; /*< Array of keys */
            yajl_val *values; /*< Array of values. */
            size_t len; /*< Number of key-value-pairs. */
        } object;
        struct {
            yajl_val *values; /*< Array of elements. */
            size_t len; /*< Number of elements. */
        } array;

void printJsonNode(int recursionLevel,yajl_val node)
{ LFS
	switch (node->type)
	{
	case yajl_t_string:
		printf("%*sstring[%s]\n",recursionLevel*2," ",node->u.string);
		break;
	case yajl_t_number:
		printf("%*snumber[%s]\n",recursionLevel*2," ",node->u.number.r);
		break;
	case yajl_t_object:
		for (unsigned int I=0; I<node->u.object.len; I++)
		{
			printf("%*sobjectKey[%s]\n",recursionLevel*2," ",node->u.object.keys[I]);
			printJsonNode(recursionLevel+1,node->u.object.values[I]);
		}
		break;
	case yajl_t_array:
		for (unsigned int I=0; I<node->u.array.len; I++)
			printJsonNode(recursionLevel+1,node->u.array.values[I]);
		break;
	case yajl_t_true:
		printf("%*strue\n",recursionLevel*2," ");
		break;
	case yajl_t_false:
		printf("%*sfalse\n", recursionLevel * 2, " ");
		break;
	case yajl_t_any:
		printf("%*sany\n", recursionLevel * 2, " ");
		break;
	case yajl_t_null:
		printf("%*snull\n", recursionLevel * 2, " ");
		break;
	}
}

// https://www.googleapis.com/freebase/v1/search?query=BND&spell=always&exact=false&prefixed=true
int Ontology::lookupInFreebaseSuggest(wstring freebaseObject,vector <cTreeCat *> &rdfTypes)
{ LFS
	replace(freebaseObject.begin(),freebaseObject.end(),L'_',L' ');
	wstring lobject=freebaseObject;
	transform (lobject.begin (), lobject.end (), lobject.begin (), (int(*)(int)) tolower);
	wstring matchid=L"/en/"+lobject;
	wstring buffer,freeBaseMetaWebQueryWebAddress=L"https://www.googleapis.com/freebase/v1/search?query="+freebaseObject+L"&spell=always&exact=false&prefixed=true";
	if (getFreebasePath(-1,freeBaseMetaWebQueryWebAddress,buffer,freebaseObject+L"_FreeBase.json")<0)
    return -1;
	string fileData;
	wTM(buffer,fileData);
	dbs ndbs;
	dbPediaOntologyCategoryList[SEPARATOR]=ndbs;
	char errbuf[1024];
	bool isFound=false;

  /* null plug buffers */
  errbuf[0] = 0;
	/* we have the whole config file in memory.  let's parse it ... */
	yajl_val node = yajl_tree_parse((const char *) fileData.c_str(), errbuf, sizeof(errbuf));

  /* parse error handling */
  if (node == NULL) {
		wstring filename=freebaseObject+L"_FreeBase.json";
		lplog(LOG_ERROR,L"FreeBase parse error (4) from %s: %S\n%s",filename.c_str(), errbuf,buffer.c_str());
    return -1;
  }

  lplog(LOG_WHERE,L"FreeBase (4): %s", buffer.c_str());
  /* ... and extract a nested value from the config file */
	const char * path[] = { "result", (const char *) 0 };
	yajl_val v4 = yajl_tree_get(node, path, yajl_t_array);
	if (v4!=NULL)
	{
		for (unsigned int i3=0; i3<v4->u.array.len; i3++)
		{
			yajl_val v5=v4->u.array.values[i3];
			if (v5->type==yajl_t_object)
			{
				const char * idpath[] = { "id", (const char *) 0 };
				const char * namepath[] = { "name", (const char *) 0 };
				yajl_val vid = yajl_tree_get(v5, idpath, yajl_t_string);
				yajl_val vname = yajl_tree_get(v5, namepath, yajl_t_string);
				wstring id,name,description;
				if (vid!=NULL && YAJL_IS_STRING(vid))
				{
					mTW(YAJL_GET_STRING(vid),id);
				}
				if (vname!=NULL && YAJL_IS_STRING(vname))
				{
					mTW(YAJL_GET_STRING(vname),name);
				  yajl_tree_free(node);
					return lookupInFreebase(name,rdfTypes);
				}
			}
		}
  }
	if (!isFound)
		lplog(LOG_WIKIPEDIA,L"FB:%s:not found",freebaseObject.c_str());
  return 0;
}

//inline int (isUnderline)(int c) { return c==L'_'; }
// properties name type
// download site: https://developers.google.com/freebase/data
// isolate name, type and description properties
// the maximum field lengths are printed out below
// 1301 seconds elapsed (89073828692 bytes read)
//totalWrittenTypes=93680521
//maxPropertyLength=4165
//maxNameLength=4096
//maxIDLength=719
//errors=52853504
//music.release_track:12041800 12%
//music.recording:8753663 9%
//music.single:6168320 6%
//book.book_edition:3403211 3%
//media_common.cataloged_instance:3208072 3%
//type.namespace:2755262 2%
//book.isbn:2667812 2%
//people.person:2634702 2%
//type.content:2521978 2%
//book.written_work:2466975 2%

//totalWrittenTypes = 125020688
//maxPropertyLength = 57934
//maxNameLength = 4086
//maxIDLength = 745
//errors = 118933
//common.notable_for:28053142 - 11 %
//music.release_track : 13805905 11 %
//music.recording : 9967967 7 %
//music.single : 7216885 5 %
//people.person : 3048153 2 %
//type.namespace : 2764735 2 %
//book.isbn : 2667819 2 %
//type.content : 2525549 2 %
//measurement_unit.dated_integer : 2490536 1 %
//book.written_work : 1947658 1 %

/*
  // 21845 is the maximum allowed for varchar 

 
 ns:m.0hcr6  key:en  "sgt_peppers_lonely_hearts_club_band".
ns:m.0hcr6  ns:common.topic.article ns:m.0hcrj.
ns:m.0hcrj  ns:common.document.text "Sgt. Pepper's Lonely Hearts Club Band (often shortened to Sgt. Pepper) is the eighth studio album by the English rock band The Beatles
ns:m.0hcr6  ns:type.object.key      "/en/sgt_peppers_lonely_hearts_club_band".
ns:m.0hcr6  ns:type.object.name     "Sgt. Pepper's Lonely Hearts Club Band"@en.
ns:m.0hcr6  ns:type.object.type     ns:award.award_nominated_work.
ns:m.0hcr6  ns:type.object.type     ns:award.award_winning_work.
ns:m.0hcr6  ns:type.object.type     ns:base.recordingstudios.studio_album.
ns:m.0hcr6  ns:type.object.type     ns:m.06vwzp1.
ns:m.0hcr6  ns:type.object.type     ns:music.album.
ns:m.0hcrj  ns:type.object.type     ns:common.document.
ns:m.0hcr6  ns:type.object.type     ns:common.topic.
*/

// fields also appear to have duplicates with rdf:
// ns:common.topic.article links description with main key
void regularize(string &lobject)
{ LFS
	transform (lobject.begin(), lobject.end(), lobject.begin(), (int(*)(int)) tolower);
	replace(lobject.begin(),lobject.end(),'_',' ');
	replace(lobject.begin(),lobject.end(),'+',' ');
	replace(lobject.begin(),lobject.end(),'|',' ');
	string slo2;
	for (unsigned int I=0; I<lobject.size(); I++)
		if (lobject[I]!=' ' || I==0 || lobject[I-1]!=' ')
			slo2+=lobject[I];
	lobject=slo2;
}

#define BUFFER_LEN 65536
void Source, ::reduceLocalFreebase(wchar_t *path,wchar_t *filename)
{ LFS
	char *fields[] = {
		"<http://rdf.freebase.com/ns/type.object.type>",
		"<http://rdf.freebase.com/ns/type.object.name>",
		"<http://rdf.freebase.com/ns/common.topic.description>",
		"<http://rdf.freebase.com/key/key.en>",
		"<http://rdf.freebase.com/ns/common.topic.article>",
		"<http://rdf.freebase.com/ns/common.document.text>",
		"<http://rdf.freebase.com/ns/common.topic.alias>",
		"<http://rdf.freebase.com/key/key.wikipedia.en>",
		"<http://rdf.freebase.com/ns/people.person.profession>"
		, NULL };
	char *fieldsCompressed[] = { "{T}", "{N}", "{D}", "{K}", "{L}", "{D}", "{A}", "{W}", "{P}", NULL };
	map <string, __int64> propertyStatisticsMap;
	wchar_t fullPath[1024];
  wsprintf(fullPath,L"%s\\%s",path,filename);
	FILE *fp=_wfopen(fullPath,L"rb");
	if (!fp) return;
	wstring fouts=fullPath+wstring(L".out");
	FILE *fout=_wfopen(fouts.c_str(),L"w+");
	if (!fout) 
	{
		fclose(fp);
		return;
	}
	char buffer[BUFFER_LEN];
	__int64 totalInputLength=_filelengthi64(_fileno(fp));
	__int64 whereInFile=0;
	int lastProgressPercent=-1;
	unsigned int maxPropertyLength = 0, maxNameLength = 0, maxIDLength = 0, maxDescriptionLength = 0, errors = 0, numAliases=0;
	string lastid,accumulatedProperties,name,description;
	vector <string> keys,aliases;
	char *id=buffer;
	if( setvbuf( fp, NULL, _IOFBF, 20000000 ) != 0 )
    printf( "Incorrect type or size of buffer.\n" );
	if( setvbuf( fout, NULL, _IOFBF, 20000000 ) != 0 )
    printf( "Incorrect type or size of buffer.\n" );
	while (fgets(buffer,BUFFER_LEN,fp))
	{
    if ((int)(whereInFile*100/totalInputLength)>lastProgressPercent)
    {
      lastProgressPercent=(int)(whereInFile*100/totalInputLength);
			wchar_t consoleTitle[1024];
      wsprintf(consoleTitle,L"PROGRESS: %03d%% lines read with %d seconds elapsed (%I64d bytes read)",lastProgressPercent,clocksec(),whereInFile);
			SetConsoleTitle(consoleTitle);
    }
		int buflen=strlen(buffer);
		while (buflen>2 && buffer[buflen-2]!='.' && buflen<BUFFER_LEN)
		{
			fgets(buffer+buflen-1,BUFFER_LEN-buflen,fp);
			int buflen2=strlen(buffer);
			if (buflen==buflen2) break;
			buflen=buflen2;
		}
		whereInFile+=buflen;
		char *chi=strchr(buffer,'\t');
		if (!chi)
			continue;
		*chi=0;
		if (lastid!=id)
		{
			if (accumulatedProperties[0] || description[0])
			{
				int cutOff=0;
				if (!strncmp(lastid.c_str(),"ns:",3))
					cutOff=3;
				if (name.empty())
					name=lastid.c_str()+cutOff;
				if (name.length()>127 || lastid.length()>127)
				{
					//printf("%s|%s\n",name.c_str(),lastid.c_str()+cutOff);
					errors++;
				}
				else
				{
					string lname=name;
					regularize(lname);
					if (lname!=name)
						accumulatedProperties+="{N}"+name;
					for (unsigned int I=0; I<keys.size(); I++)
					{
						regularize(keys[I]);
						replace(keys[I].begin(),keys[I].end(),' ','_');
						fprintf(fout,"%s|%s|%s|%s%s\n",lname.c_str(),keys[I].c_str(),lastid.c_str()+cutOff,accumulatedProperties.c_str(),description.c_str());
					}
					if (keys.empty())
						fprintf(fout,"%s||%s|%s%s\n",lname.c_str(),lastid.c_str()+cutOff,accumulatedProperties.c_str(),description.c_str());
					if (lastid.size()>0)
						for (unsigned int I = 0; I<aliases.size(); I++, numAliases++)
						{
							regularize(aliases[I]);
							fprintf(fout,"%s||%s|\n",aliases[I].c_str(),lastid.c_str()+cutOff);
						}
				}
				maxPropertyLength=max(maxPropertyLength,accumulatedProperties.length());
				maxNameLength=max(maxNameLength,name.length());
				maxDescriptionLength=max(maxDescriptionLength,description.length());
				maxIDLength=max(maxIDLength,lastid.length());
				lastid=id;
			}
			accumulatedProperties.clear();
			name.clear();
			description.clear();
			keys.clear();
			aliases.clear();
			lastid=id;
		}
		char *ftype=chi+1;
		chi=strchr(ftype,'\t');
		if (!chi)
			continue;
		*chi=0;
		for (int I=0; fields[I]; I++)
			if (!strcmp(ftype,fields[I]))
			{
				char *fproperty=chi+1;
				if (buffer[buflen-1]=='\n')
					buflen--;
				if (buffer[buflen - 1] == '.')
					buflen--;
				if (buffer[buflen - 1] == '\t')
					buflen--;
				buffer[buflen] = 0;
				if (!strcmp(fproperty,"<http://rdf.freebase.com/ns/common.topic>") || !strcmp(fproperty,"<http://rdf.freebase.com/ns/common.document>"))
					break;
				if (fproperty[0]=='"')
				{
					char *endQuote=strrchr(fproperty+1,'"');
					if (!endQuote || (endQuote[1]!=0 && (endQuote[1]!=L'@' || endQuote[2]!=L'e' || endQuote[3]!=L'n')))
						break;
					fproperty++;
					*endQuote=0;
				}
				int cutOff=0;
				if (!strncmp(fproperty, "ns:", 3))
					cutOff = 3;
				if (!strncmp(fproperty, "<http://rdf.freebase.com/ns/", strlen("<http://rdf.freebase.com/ns/")))
				{
					cutOff = strlen("<http://rdf.freebase.com/ns/");
					if (fproperty[strlen(fproperty) - 1] == '>')
						fproperty[strlen(fproperty) - 1] = 0;
				}
				if (I==1)
					name=fproperty+cutOff;
				else if (I==2 || I==5)
				{
					description=fieldsCompressed[I];
					description+=fproperty+cutOff;
				}
				else if (I==3) // KEY
				{
					keys.push_back(fproperty);
				}
				else if (I==6)
				{
					for (char *l=fproperty+cutOff; *l; l++)
						*l=tolower(*l);
					aliases.push_back(fproperty+cutOff);
				}
				else
				{
					if (I==0)
					{
						map <string,__int64>::iterator psmi=propertyStatisticsMap.find(fproperty+cutOff);
						if (psmi!=propertyStatisticsMap.end())
							psmi->second++;
						else
							propertyStatisticsMap[fproperty+cutOff]=1;
					}
					if (I==7)
						aliases.push_back(fproperty + cutOff);
					accumulatedProperties += fieldsCompressed[I];
					accumulatedProperties+=fproperty+cutOff;
				}
				break;
			}
	}
	if (ferror(fp))
		perror( "Read error" );
	struct ltint
	{
		bool operator()(__int64 i1, __int64 i2) const
		{
			return i1>i2;
		}
	};

	map <__int64,string,ltint> psOrderMap; 
	__int64 total=0;
	for (map <string,__int64>::iterator psmi=propertyStatisticsMap.begin(),psmiEnd=propertyStatisticsMap.end(); psmi!=psmiEnd; psmi++)
	{
		if (psmi->second>100000)
			psOrderMap[psmi->second]=psmi->first;
		total+=psmi->second;
	}
	int top10=0;
	printf("\ntotalWrittenTypes=%I64d\nmaxPropertyLength=%d\nmaxNameLength=%d\nmaxIDLength=%d\nerrors=%d\naliases=%d",total,maxPropertyLength,maxNameLength,maxIDLength,errors,numAliases);
	for (map <__int64,string>::iterator psmi=psOrderMap.begin(),psmiEnd=psOrderMap.end(); psmi!=psmiEnd && top10<10; psmi++,top10++)
		printf("%s:%I64d %I64d%%\n",psmi->second.c_str(),psmi->first,((__int64)psmi->first*100)/total);
	fclose(fp);
	fclose(fout);
	initializeDatabaseHandle(mysql, L"localhost", alreadyConnected);
	if (!myquery(&mysql, L"drop table freebaseProperties")) return;
	if (!myquery(&mysql,L"create table freebaseProperties  ( name varchar(128) NOT NULL,INDEX name_ind (name),k varchar(64) NOT NULL,INDEX k_ind (k),id varchar(128) NOT NULL,INDEX id_ind (id),properties varchar(18000) NOT NULL ) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin")) return ;
	wstring sqlFullStatement=L"LOAD DATA INFILE '";
	for (int I=0; fullPath[I]; I++)
		if (fullPath[I]=='\\')
			sqlFullStatement+=L"\\\\";
		else
			sqlFullStatement+=fullPath[I];
	sqlFullStatement+=L".out' INTO TABLE freebaseProperties FIELDS TERMINATED BY '|' LINES TERMINATED BY '\\r\\n'";
	if (!myquery(&mysql,(wchar_t *)sqlFullStatement.c_str())) 
		return ;
}

bool wholeWord(char *buffer,char *pattern)
{ LFS
	char *ch=strstr(buffer,pattern);
	if (!ch) return false;
	char *endch=ch+strlen(pattern);
	return isspace(*endch) || *endch=='.';
}

/*
 * The memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
void *memstr(const char *haystack, size_t hlen, const char *needle, size_t nlen)
{ LFS
    int needle_first;
    const char *p = haystack;
    size_t plen = hlen;

    if (!nlen)
        return NULL;

    needle_first = *(unsigned char *)needle;

    while (plen >= nlen && (p = (char *)memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen))
            return (void *)p;

        p++;
        plen = hlen - (p - haystack);
    }

    return NULL;
}


// "Hearts Club Band"
// ns:m.0hcr6  key:en  "sgt_peppers_lonely_hearts_club_band".
// ns:common.document   ns:type.type.instance   ns:m.0hcrj.
// ns:m.0hcrj  ns:common.document.text "Sgt. Pepper's Lonely Hearts Club Band (often shortened to Sgt. Pepper) is the eighth studio album by the English rock band The Beatles, released on 1"@en.
#define SCAN_BUF_LEN 200000000
void scanStringInFile(wchar_t *filename,char *testItem)
{ LFS 
	int fp=_wopen(filename,_O_RDONLY|O_BINARY,0);
	if (fp<0) 
	{
    perror( "Open failed on input file" );
		return;
	}
	char *buffer=(char *)malloc(SCAN_BUF_LEN),*pattern=testItem;
	__int64 totalInputLength=_filelengthi64(fp),whereInFile=0;
	int lastProgressPercent=-1,patlen=strlen(pattern);
	for (; _read(fp,buffer,SCAN_BUF_LEN)>0; whereInFile+=SCAN_BUF_LEN)
	{
    if ((int)(whereInFile*100/totalInputLength)>lastProgressPercent)
    {
      lastProgressPercent=(int)(whereInFile*100/totalInputLength);
			wchar_t consoleTitle[1024];
      wsprintf(consoleTitle,L"PROGRESS: %03d%% lines read with %d seconds elapsed (%I64d bytes read)",lastProgressPercent,clocksec(),whereInFile);
			SetConsoleTitle(consoleTitle);
    }
		char *ch=buffer,*nextch;
		while ((ch-buffer)<SCAN_BUF_LEN && (nextch=(char *)memstr(ch,SCAN_BUF_LEN-(ch-buffer),pattern,patlen)))
		{
			ch=nextch+patlen;
			if (isspace(*ch) || *ch=='.')
			{
				char *begin=ch;
				while (begin>buffer && *begin!='\n' && *begin)
					begin--;
				char *end=strchr(ch,'\n');
				if (!end)
					break;
				*end=0;
				printf("%12I64d:%s\n",whereInFile+(begin-buffer),begin+1);
				ch=end+1;
			}
		}
	}
	close(fp);
}

int tospace(int c) {
  return (c>=0 && isspace(c)) ? ' ':c;
}

void computePhraseSimilarity(vector <wstring> &va,vector <wstring> &vb,unsigned int &ai,unsigned int &similaritySum)
{ LFS
	unsigned int maxSimilaritySum=0,maxAI=0;
	for (unsigned int bi=0; bi<vb.size(); bi++)
	{
		if (vb[bi]==va[ai])
		{
			unsigned int aai=ai,bbi=bi,subSimilaritySum=0;
			for (; bbi<vb.size(); bbi++)
			{
				if (va[aai]!=vb[bbi])
				{
					if (va[aai]!=vb[bbi+1])
						break;
					subSimilaritySum-=50;
					bbi++;
				}
				subSimilaritySum+=100;
				aai++;
			}
			if (maxSimilaritySum<subSimilaritySum)
			{
				maxSimilaritySum=subSimilaritySum;
				maxAI=aai;
			}
		}
	}
	if (maxAI>ai+3)
	{
		ai=maxAI;
		similaritySum+=maxSimilaritySum;
	}
	else
	{
		similaritySum+=maxSimilaritySum/2;
		ai++;
	}
}

// for each word of a (aw):
//    is bw[3] or bw+1[2] or bw+2[1] the same (if so bw2)? search 20 words beyond (if so bw3)
//    if not, is bw2[3] or bw2+1[2] or bw2+2[1] the same (if so bw2) search 20 words beyond (if so bw3)
//    if (match for bw2 or bw3, 
int computeSimilarity(string &a,int &numWordsA,string &b,int &numWordsB,sTrace &t)
{ LFS
	// separate a and b into words and also regarding punctuation
	if (a==b)
		return 100;
	if (a.empty() || b.empty())
		return 0;
	wstring wa;
	mTW(a,wa);
	wchar_t *bookBuffer=(wchar_t *)wa.c_str();
	__int64 bufferLen=wa.length(),bufferScanLocation=0;
	wstring sWord;
	vector <wstring> va,vb;
	int nounOwner=0,result=0;
	while (result!=PARSE_EOF && !exitNow)
	{
		result=Words.readWord(bookBuffer,bufferLen,bufferScanLocation,sWord,nounOwner,false,false,t);
		if (sWord.length())
			va.push_back(sWord);
	}
	wstring wb;
	mTW(b,wb);
	bookBuffer=(wchar_t *)wb.c_str();
	bufferLen=wb.length(),bufferScanLocation=0;
	result=0;
	while (result!=PARSE_EOF && !exitNow)
	{
		result=Words.readWord(bookBuffer,bufferLen,bufferScanLocation,sWord,nounOwner,false,false,t);
		if (sWord.length())
			vb.push_back(sWord);
	}
	unsigned int similaritySum=0;
	numWordsA=va.size();
	numWordsB=vb.size();
	for (unsigned int ai=0; ai<va.size(); )
	{
		computePhraseSimilarity(va,vb,ai,similaritySum);
	}
	return similaritySum/min(va.size(),vb.size());
}

void Ontology::compareFreebaseWebToFreebaseDescriptionRDL()
{ LFS
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	wchar_t original[4096];
	int descriptionsProcessed=0,descriptionFailures=0;
	printf("\n");
	sTrace t;
	for (char f='a'; f<='z'; f++)
		for (char f2='a'; f2<='z'; f2++)
		{
			_snwprintf(original,4096,L"%s\\dbPediaCache\\%c\\%c\\*_FreeBaseTopic.json",CACHEDIR,f,f2);
			hFind = FindFirstFile(original, &FindFileData);
			if (hFind == INVALID_HANDLE_VALUE) 
			{
				wprintf (L"FindFirstFile failed on directory %s (%d)\r", original, (int)GetLastError());
				continue;
			}
			do
			{
				if (FindFileData.cFileName[0]=='.' || wcsstr(FindFileData.cFileName,L"UTF8")) continue;
				_snwprintf(original,4096,L"%s\\dbPediaCache\\%c\\%c\\%s",CACHEDIR,f,f2,FindFileData.cFileName);
				wchar_t buffer[65536];
				int actualLen=-1;
				getPath(original,buffer,65536,actualLen);
				if (wcsstr(buffer,L"description"))
				{
					string fileData;
					wTM(buffer,fileData);
					char errbuf[1024];
					errbuf[0] = 0;
					yajl_val node = yajl_tree_parse((const char *) fileData.c_str(), errbuf, sizeof(errbuf));
					if (node == NULL) 
						continue;
					/* ... and extract a nested value from the config file */
					wchar_t id[1024];
					wcscpy(id,L"/en/");
					// cFileName='_aag_FreeBaseTopic.json'
					wcscpy(id+4,FindFileData.cFileName+1);
					id[wcslen(id)-strlen("_FreeBaseTopic.json")]=0;
					// id='/en/aag'
					string utf8id;
					wTM(id,utf8id);
					const char * path[] = { utf8id.c_str(), "result", "description", (const char *) 0 };
					yajl_val v4 = yajl_tree_get(node, path, yajl_t_string);
					string descriptionFromWeb;
					if (v4!=NULL)
					{
						string tdw=YAJL_GET_STRING(v4);
						// fancy way of getting rid of spaces but doesn't compile with STLPort 5.2.1
						//descriptionFromWeb.erase(std::find_if(descriptionFromWeb.rbegin(), descriptionFromWeb.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), descriptionFromWeb.end());
						// painful slow method
						for (unsigned int I=0; I<tdw.size(); I++)
							if (!isspace(tdw[I]))
								descriptionFromWeb+=tdw[I];
        	}
					yajl_tree_free(node);
					string descriptionFromDB;
					wTM(Ontology::getFBDescription(L"",id+4),descriptionFromDB);
					descriptionsProcessed++;
					// ... is appended when the description is too long
					int wherePeriods; 
					if ((wherePeriods=descriptionFromWeb.find("..."))!=string::npos && wherePeriods==descriptionFromWeb.length()-3)
						descriptionFromWeb.erase(wherePeriods);
					// eliminate spaces as they are also different from the web and the original db entry
					transform (descriptionFromWeb.begin (), descriptionFromWeb.end (), descriptionFromWeb.begin(), (int(*)(int)) tospace);
					transform (descriptionFromDB.begin (), descriptionFromDB.end (), descriptionFromDB.begin(), (int(*)(int)) tospace);
					if (descriptionFromDB.find(descriptionFromWeb)!=0)
					{
						int numWordsWeb=0,numWordsDB=0;
						int cs=computeSimilarity(descriptionFromWeb,numWordsWeb,descriptionFromDB,numWordsDB,t);
						if (cs<20)
						{
							descriptionFailures++;
							printf("%d%% (%d out of %d) failures:%S:Non-matching description:%d\nweb [%04d words]:%s[%zu]\n\ndb  [%04d words]:%s[%zu]\n",
								descriptionFailures*100/descriptionsProcessed,descriptionFailures,descriptionsProcessed,id,cs,
								numWordsWeb,descriptionFromWeb.c_str(),descriptionFromWeb.length(),numWordsDB,descriptionFromDB.c_str(),descriptionFromDB.length());
						}
					}
				}
			} while (FindNextFile(hFind, &FindFileData) != 0);
			FindClose(hFind);
		}
}

