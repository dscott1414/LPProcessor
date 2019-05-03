#pragma warning(disable : 4786 4503 4996) // disable warning C4786
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
#include <io.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <set>
using namespace std;

wchar_t *getLastErrorMessage() 
{ 
	static wchar_t msg[10000];
  DWORD dw = GetLastError(); 
  FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM,NULL,dw,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPTSTR) &msg,10000, NULL );
	size_t len;
	if (msg[len=wcslen(msg)-1]=='\n') msg[len]=0;
	return msg;
}

int getPath(const char *pathname,char *buffer,int maxlen,int &actualLen)
{
	HANDLE hFile = CreateFileA(pathname,    // file to open
										GENERIC_READ,          // open for reading
										FILE_SHARE_READ,       // share for reading
										NULL,                  // default security
										OPEN_EXISTING,         // existing file only
										FILE_ATTRIBUTE_NORMAL, // normal file
										NULL);                 // no attr. template
	 
	if (hFile == INVALID_HANDLE_VALUE) 
	{ 
		if (GetLastError()!=ERROR_PATH_NOT_FOUND && GetLastError()!=ERROR_FILE_NOT_FOUND)  
			printf("GetPath cannot open path %s - %S",pathname,getLastErrorMessage());
		return -1;
	}
	actualLen=GetFileSize(hFile,NULL);
	if (actualLen<=0)
	{
		printf("ERROR:filelength of file %s yields an invalid filelength (%d).",pathname,actualLen);
		CloseHandle(hFile);
		return -1;
	}
	if (actualLen+1>=maxlen) 
	{
		printf("ERROR:filelength of file %s (%d) is greater than the maximum allowed (%d).",pathname,actualLen+1,maxlen);
		CloseHandle(hFile);
		return -1;
	}
	DWORD lenRead=0;
	if (!ReadFile(hFile,buffer,actualLen,&lenRead,NULL) || actualLen!=lenRead)
	{
		printf("ERROR:read error of file %s.",pathname);
		CloseHandle(hFile);
		return -1;
	}
	buffer[actualLen]=0;
	CloseHandle(hFile);
	return 0;
}

#define MAX_BUF_LEN 10000000

char buffer[MAX_BUF_LEN];
typedef map <string,int>::iterator tIBNC;
typedef pair <string, int> tWFIMap;
map<string,int> alltypes,lptypes;

void findAllTypesMatchingWord(char *path,char *pattern)
{
  struct _finddata_t bnc_file;
  intptr_t hFile;
  strcat(path,"\\");
  size_t olen=strlen(path);
  strcat(path,"*.");
  if( (hFile = _findfirst( path, &bnc_file )) != -1L )
   {
      do 
      {
        if (bnc_file.name[0]!='.')
        {
          strcpy(path+olen,bnc_file.name);
          if (bnc_file.attrib&_A_SUBDIR)
            findAllTypesMatchingWord(path,pattern);
          else
          {
            printf("%s\r",path);
            int actualLen;
            getPath(path,buffer,MAX_BUF_LEN,actualLen);
            int where=0;
            strlwr(buffer);
            while (where<actualLen-(signed)strlen(pattern))
            {
              if (!strncmp(buffer+where,pattern,strlen(pattern)))
              {
                int tmp=where;
                while (buffer[tmp]!='<' && tmp) tmp--;
                buffer[where]=0;
    			      pair < tIBNC, bool > pr=alltypes.insert(tWFIMap(buffer+tmp+3,0));
                if (!pr.second)
                  pr.first->second++;
              }
              where++;
            }
          }
        }
      } while( _findnext( hFile, &bnc_file ) == 0 );
      _findclose( hFile );
  }
}

void findAllWordsMatchingType(char *path,char *pattern)
{
  struct _finddata_t bnc_file;
  intptr_t hFile;
  strcat(path,"\\");
  size_t olen=strlen(path);
  strcat(path,"*.");
  if( (hFile = _findfirst( path, &bnc_file )) != -1L )
   {
      do 
      {
        if (bnc_file.name[0]!='.')
        {
          strcpy(path+olen,bnc_file.name);
          if (bnc_file.attrib&_A_SUBDIR)
            findAllWordsMatchingType(path,pattern);
          else
          {
            printf("%s\r",path);
            int actualLen;
            getPath(path,buffer,MAX_BUF_LEN,actualLen);
            int where=0;
            strlwr(buffer);
            while (where<actualLen-(signed)strlen(pattern))
            {
              if (!strncmp(buffer+where,pattern,strlen(pattern)))
              {
                size_t tmp=where+strlen(pattern);
                while (!isspace(buffer[tmp]) && buffer[tmp]!='<') tmp++;
                buffer[tmp]=0;
    			      pair < tIBNC, bool > pr=alltypes.insert(tWFIMap(buffer+where+strlen(pattern),0));
                if (!pr.second)
                  pr.first->second++;
              }
              where++;
            }
          }
        }
      } while( _findnext( hFile, &bnc_file ) == 0 );
      _findclose( hFile );
  }
}

struct
{
  char *tag;
  char *sForm;
} tagList[]=
{
  // Total number of wordclass tags in the BNC basic tagset = 57, plus 4 punctuation tags
  {"aj0","adjective"}, // adjective (general or positive) (e.g. good, old, beautiful)
  {"ajc","adjective"}, // comparative adjective (e.g. better, older)
  {"ajs","adjective"}, // superlative adjective (e.g. best, oldest)
  {"at0","determiner//demonstrative_determiner//possessive_determiner//interrogative_determiner//quantifier"}, // article (e.g. the, a, an, no, every, )
  {"av0","adverb//predeterminer//time//abbreviation//time_abbreviation//date_abbreviation"}, // general adverb: an adverb not subclassified as avp or avq (see below) (e.g. often, well, longer (adv.), furthest. or abbreviations like ie, i.e. ibid etc
  {"avp","adverb"}, // adverb particle (e.g. up, off, out)
  {"avq","relativizer//interrogative_pronoun"}, // wh-adverb (e.g. when, where, how, why, wherever)
  {"cjc","coordinator//conjunction"}, // coordinating conjunction (e.g. and, or, but) - 'but' should not be a coordinator (2/2/2007) jack but jill went up the hill.
  {"cjs","conjunction//coordinator//preposition//relativizer//adverb"}, // subordinating conjunction (e.g. although, when)
  {"cjt","relativizer"}, // the subordinating conjunction that
  {"crd","numeral_cardinal//numeral_ordinal//number//roman_numeral//date//time"}, // cardinal number (e.g. one, 3, fifty-five, 3609, '90s)
  {"dps","possessive_determiner"}, // possessive determiner-pronoun (e.g. your, their, his)
  {"dt0","pronoun//quantifier//demonstrative_determiner//predeterminer//adjective"}, // general determiner-pronoun: i.e. a determiner-pronoun which is not a dtq or an at0.
  {"dtq","relativizer//interrogative_determiner//interrogative_pronoun"}, // wh-determiner-pronoun (e.g. which, what, whose, whichever)
  {"ex0",NULL}, // existential there, i.e. there occurring in the there is ... or there are ... construction
  {"itj","interjection"}, // interjection or other isolate (e.g. oh, yes, mhm, wow)
  {"nn0","noun"}, // common noun, neutral for number (e.g. aircraft, data, committee)
  {"nn1","noun"}, // singular common noun (e.g. pencil, goose, time, revelation)
  {"nn2","noun"}, // plural common noun (e.g. pencils, geese, times, revelations)
  {"np0","proper noun"}, // proper noun (e.g. london, michael, mars, ibm)
  {"ord","numeral_ordinal"}, // ordinal numeral (e.g. first, sixth, 77th, last) .
  {"pni",NULL}, // indefinite pronoun (e.g. none, everything, one [as pronoun], nobody)
  {"pnp","personal_pronoun_nominative//personal_pronoun//possessive_pronoun//personal_pronoun_accusative"}, // personal pronoun (e.g. i, you, them, ours)
  {"pnq",NULL}, // wh-pronoun (e.g. who, whoever, whom)
  {"pnx",NULL}, // reflexive pronoun (e.g. myself, yourself, itself, ourselves)
  {"pos","combine"}, // the possessive or genitive marker 's or '
  {"prf",NULL}, // the preposition of
  {"prp","preposition"}, // preposition (except for of) (e.g. about, at, in, on, on behalf of, with)
  {"pul",NULL}, // punctuation: left bracket - i.e. ( or [
  {"pun",NULL}, // punctuation: general separating mark - i.e. . , ! , : ; - or ?
  {"puq",NULL}, // punctuation: quotation mark - i.e. ' or \"
  {"pur",NULL}, // punctuation: right bracket - i.e. ) or ]
  {"to0","to//preposition"}, // infinitive marker to
  {"unc",NULL}, // unclassified items which are not appropriately considered as items of the english lexicon.
  {"vbb","is"}, // the present tense forms of the verb be, except for is, 's: i.e. am, are, 'm, 're and be [subjunctive or imperative]
  {"vbd","is"}, // the past tense forms of the verb be: was and were
  {"vbg",NULL}, // the -ing form of the verb be: being
  {"vbi",NULL}, // the infinitive form of the verb be: be
  {"vbn",NULL}, // the past participle form of the verb be: been
  {"vbz","is"}, // the -s form of the verb be: is, 's
  {"vdb","does"}, // the finite base form of the verb do: do
  {"vdd","does"}, // the past tense form of the verb do: did
  {"vdg","does"}, // the -ing form of the verb do: doing
  {"vdi",NULL}, // the infinitive form of the verb do: do
  {"vdn","does"}, // the past participle form of the verb do: done
  {"vdz","does"}, // the -s form of the verb do: does, 's
  {"vhb","have"}, // the finite base form of the verb have: have, 've
  {"vhd","have"}, // the past tense form of the verb have: had, 'd
  {"vhg","have"}, // the -ing form of the verb have: having
  {"vhi","have"}, // the infinitive form of the verb have: have
  {"vhn","have"}, // the past participle form of the verb have: had
  {"vhz","have"}, // the -s form of the verb have: has, 's
  {"vm0","modal_auxiliary//future_modal_auxiliary//verbverb"}, // modal auxiliary verb (e.g. will, would, can, could, 'll, 'd) or "let",l"dare"
  {"vvb","verbverb//verb//think"}, // the finite base form of lexical verbs (e.g. forget, send, live, return) [including the imperative and present subjunctive]
  {"vvd","verbverb//verb//think"}, // the past tense form of lexical verbs (e.g. forgot, sent, lived, returned)
  {"vvg","verbverb//verb//think"}, // the -ing form of lexical verbs (e.g. forgetting, sending, living, returning)
  {"vvi","verbverb//verb//think"}, // the infinitive form of lexical verbs (e.g. forget, send, live, return)
  {"vvn","verbverb//verb//think"}, // the past participle form of lexical verbs (e.g. forgotten, sent, lived, returned)
  {"vvz","verbverb//verb//think"}, // the -s form of lexical verbs (e.g. forgets, sends, lives, returns)
  {"xx0","combine"}, // the negative particle not or n't
  {"zz0",NULL}, // alphabetical symbols (e.g. a, a, b, b, c, d)
	{NULL,NULL}
  };
/*
<w PRP-AVP>in <
<w PRP>in <
  {"AJ0","NN1"},
  {"AJ0","VVD"},
  {"AJ0","VVG"},
  {"AJ0","VVN"},
  {"AV0","AJ0"},
  {"AVP","PRP"},
  {"AVQ","CJS"},
  {"CJS","AVQ"},
  {"CJS","PRP"},
  {"CJT","DT0"},
  {"CRD","PNI"},
  {"DT0","CJT"},
  {"NN1","AJ0"},
  {"NN1","NP0"},
  {"NN1","VVB"},
  {"NN1","VVG"},
  {"NN2","VVZ"},
  {"NP0","NN1"},
  {"PNI","CRD"},
  {"PRP","AVP"},
  {"PRP","CJS"},
  {"VVB","NN1"},
  {"VVD","AJ0"},
  {"VVD","VVN"},
  {"VVG","AJ0"},
  {"VVG","NN1"},
  {"VVN","AJ0"},
  {"VVN","VVD"},
  {"VVZ","NN2"}
*/
#define FIND_TYPE "<w dt0>"
#define FIND_WORD ">sit"
int main( int argc, char *argv[] )
{
  char path[1024];
  strcpy(path,"F:\\lp\\BNC-World\\texts");
  findAllTypesMatchingWord(path,FIND_WORD);
  //findAllWordsMatchingType(path,FIND_TYPE);
  printf("\n");
  __int64 total=0;
  for (tIBNC b=alltypes.begin(),e=alltypes.end(); b!=e; b++)
  {
    if (b->first.find('-')==string::npos && b->second)
		{
			int found=-1;
			for (unsigned int t=0; tagList[t].tag && found<0; t++)
				if (b->first==tagList[t].tag)
					found=t;
			printf("%s:%d %s\n",b->first.c_str(),b->second,(found<0 || !tagList[found].sForm) ? "" : tagList[found].sForm);
			if (found>=0 && tagList[found].sForm)
			{
				if (lptypes.find(tagList[found].sForm)==lptypes.end())
					lptypes[tagList[found].sForm]=b->second;
				else
					lptypes[tagList[found].sForm]+=b->second;
			}
		}
    total+=b->second;
  }
	printf("----------------------\n");
  for (tIBNC b=lptypes.begin(),e=lptypes.end(); b!=e; b++)
		printf("%s:%d\n",b->first.c_str(),b->second);
  printf("total=%I64d",total);
  char tryx[1024];
  gets_s(tryx);
}