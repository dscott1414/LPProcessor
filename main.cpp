#include <windows.h>
#define _WINSOCKAPI_ /* Prevent inclusion of winsock.h in windows.h */
#include "io.h"
#include "winhttp.h"
#include "word.h"
#include "source.h"
#include "time.h"
#include <fcntl.h>
#include "bncc.h"
#include "mysql.h"
#include <direct.h>
#include <sys/stat.h>
#include "ontology.h"
#include <crtdbg.h>
	extern "C" {
#include <yajl_tree.h>
	}
#include "getMusicBrainz.h"
#include "profile.h"
#include <Dbghelp.h>
#include "mysqldb.h"
#include "internet.h"
#include "stacktrace.h"

// needed for _STLP_DEBUG - these must be set to a legal, unreachable yet never changing value
unordered_map <wstring,tFI> static_wordMap;
tIWMM wNULL=static_wordMap.begin();
map<tIWMM, tFI::cRMap::tRelation, Source::wordMapCompare> static_tIcMap;
tFI::cRMap::tIcRMap tNULL=(tFI::cRMap::tIcRMap)static_tIcMap.begin();
vector <cLocalFocus> static_cLocalFocus;
vector <cLocalFocus>::iterator cNULL=static_cLocalFocus.begin();
vector <Source::cSpeakerGroup> static_cSpeakerGroup;
vector <Source::cSpeakerGroup>::iterator sgNULL = (vector <Source::cSpeakerGroup>::iterator)static_cSpeakerGroup.begin();
vector <WordMatch> static_wm;
vector <WordMatch>::iterator wmNULL=static_wm.begin();
set<int> static_setInt;
set<int>::iterator sNULL= static_setInt.begin();

// profiling
__int64 cProfile::cb;
__int64 cProfile::accumulatedOverheadTime=0;
unordered_map <string ,__int64 > cProfile::counterMap;
unordered_map <string ,int > cProfile::counterNumMap;
unordered_map <string,cProfile::CP> cProfile::timeMapTotal;
__int64 cProfile::totalCount=0;
bool cProfile::lockInitialized=false;
string cProfile::functionPath;
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::timeSetCompare> cProfile::timeSort; // sort map by time taken by function
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::memorySetCompare> cProfile::memorySort; // sort map by memory allocated by function
set <unordered_map <string,cProfile::CP>::iterator ,cProfile::countSetCompare> cProfile::countSort; // sort map by number of times function is called
__int64 cProfile::mySQLTotalTime=0;
struct _RTL_SRWLOCK cProfile::networkTimeSRWLock;
int cProfile::totalInternetTimeWaitBandwidthControl;
__int64 cProfile::accumulationNetworkProfileTimer;
__int64 cProfile::accumulateOnlyNetTimer;
__int64 cProfile::lastNetworkTimePrinted;
__int64 cProfile::accumulateNetworkTimeCount;
int cProfile::lastNetClock;
unordered_map < wstring, __int64 > cProfile::netAndSleepTimes,cProfile::onlyNetTimes,cProfile::numTimesPerURL;


typedef long long (FAR WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType, CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

void createMinidump(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
	HMODULE mhLib = ::LoadLibrary(L"dbghelp.dll");
	MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(mhLib, "MiniDumpWriteDump");
	wchar_t corePath[1024];
	wsprintf(corePath, L"%s\\core.dmp",LMAINDIR);
	HANDLE  hFile = ::CreateFile(corePath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

	_MINIDUMP_EXCEPTION_INFORMATION ExInfo;
	ExInfo.ThreadId = ::GetCurrentThreadId();
	ExInfo.ExceptionPointers = apExceptionInfo;
	ExInfo.ClientPointers = FALSE;

	pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL);
	::CloseHandle(hFile);
}

void printStackTrace()
{
	std::stringstream buff;
	buff << ":  General Software Fault! \n";
	buff << "\n";

	std::vector<dbg::StackFrame> stack = dbg::stack_trace();
	buff << "Callstack: \n";
	for (unsigned int i = 0; i < stack.size(); i++)
	{
		buff << "0x" << std::hex << stack[i].address << ": " << stack[i].name << "(" << std::dec << stack[i].line << ") in " << stack[i].module << "\n";
	}
	::lplog(LOG_FATAL_ERROR, L"%S", buff.str().c_str());
}

LONG WINAPI unhandled_handler(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
	createMinidump(apExceptionInfo);
	printStackTrace();
	return EXCEPTION_CONTINUE_SEARCH;
}

int acquireList(wchar_t *filename)
{ 
	LFS
	FILE *listfile=_wfopen(filename,L"rb"); // binary mode reads unicode
	if (listfile)
	{
		wchar_t url[2048],*path;
		int total=0;
		while (fgetws(url,2047,listfile))
		{
			if (url[0]==0xFEFF) // detect BOM
				memcpy(url,url+1,wcslen(url+1));
			if (url[wcslen(url)-1]==L'\n') url[wcslen(url)-1]=0;
			if (url[wcslen(url)-1]==L'\r') url[wcslen(url)-1]=0;
			wchar_t *ch=wcschr(url,L'|');
			*ch=0;
			path=ch+1;
			wchar_t *period=wcsrchr(url,L'.');
			if (period) wcscat(path,period);
			wstring buffer;
			int destfile=_wopen(path,O_RDWR|O_BINARY|O_CREAT);
			if (destfile)
			{
				if (Internet::readBinaryPage(url,destfile,total))
					lplog(LOG_ERROR,L"error retrieving %s.",url);
				close(destfile);
			}
			else
				lplog(LOG_ERROR,L"error opening %s.",path);
			wprintf(L"%s - total bytes = %dMB.\n",path,total/1024/1024);
			Sleep(5000);
		}
		fclose(listfile);
	}
	return 0;
}

/*
int reportInfo(WMISupport &provider)
{ LFS
static __int64 lastVirtualBytes=0,lastWorkingSet=0;

IWbemClassObject *resourceNameInstance;
bool status=provider.getResourceObject(L"Win32_PerfFormattedData_PerfProc_Process.Name='lp'",resourceNameInstance);
__int64 percentProcessorTime,percentUserTime,percentPrivilegedTime,elapsedTime,virtualBytes,workingSet;
status = provider.getWMIKey(resourceNameInstance,L"ElapsedTime",elapsedTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentProcessorTime",percentProcessorTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentUserTime",percentUserTime);
status = provider.getWMIKey(resourceNameInstance,L"PercentPrivilegedTime",percentPrivilegedTime);
status = provider.getWMIKey(resourceNameInstance,L"VirtualBytes",virtualBytes);
status = provider.getWMIKey(resourceNameInstance,L"WorkingSet",workingSet);
lplog(L"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
wprintf(L"elapsed time=%I64d (%I64d,%I64d,%I64d) virtualBytes=%I64d workingSet=%I64d\n",
elapsedTime,percentProcessorTime,percentUserTime,percentPrivilegedTime,virtualBytes-lastVirtualBytes,workingSet-lastWorkingSet);
lastVirtualBytes=virtualBytes;
lastWorkingSet=workingSet;
return 0;
}
*/

bool exitNow=false,exitEventually=false;

BOOL WINAPI ConsoleHandler(DWORD CEvent)
{ LFS
	if (exitEventually) exitNow=true;
	exitEventually=true;
	switch(CEvent)
	{
	case CTRL_C_EVENT:
		wprintf(L"\nCTRL+C received! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_BREAK_EVENT:
		wprintf(L"\nCTRL+Break received! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_CLOSE_EVENT:
		wprintf(L"\nClose received! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_LOGOFF_EVENT:
		wprintf(L"\nUser is logging off! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	case CTRL_SHUTDOWN_EVENT:
		exitNow=true;
		wprintf(L"\nSystem is shutting down! Interrupting %s...\n",(exitNow) ? L"immediately" : L"at end of this source");
		break;
	}
	return TRUE;
}

/*	

Amazon customer reviews:
http://www.amazon.com/gp/community-content-search/results?ie=UTF8&flatten=1&query=10%20amp&search-alias=community-reviews

1. finish processing american gutenberg and australian gutenberg
2. process definitions and create word relations with definition flag
3. finish using word relations for parsing speed-up and evaluation
4. Also fix word relations not found problem when writing word relation history
5. Use this to convert from
http://gill.readingroo.ms/1%20-%20GILL'S%20LIBRARY/Fiction-Biographies/
http://manual.calibre-ebook.com/faq.html#what-formats-does-app-support-conversion-to-from

1. PURPOSE - the purpose of the first conversation in the Adversary is to transfer the papers
2. DESIRE - Characters say 'I want to make money', I'd like to go to the park (etc)
3. SENTIMENT - Today I am happy.

1. record location of every relation in a separate table
each location can also be specific and generic (St. James Park is specific, park is generic)
2. create a linked list of narrative verbs throughout each narrative
3. divided into sections defined by changes in time and/or place
4. where the narrative linked list leads backwards into the start of the section
5. and each section has an associated time and place
6. and each section is linked to the speakergroup.
7. each section should also have associated generic objects, such that each location has associated generic objects (chair, desk, tree, etc)

look up a relation throughout all sources.
filter by generic place (park/kitchen/etc) not specific place
filter by previous action/future action

generic action cause/effect
for every entry in wordRelationHistory for a given wordRelation (and gwhere=given where)
query wordRelationHistory where where>gwhere-10 and where<gwhere [PAST]
OR query wordRelationHistory where where<gwhere+10 and where>gwhere [FUTURE]
and narrativeNum matches sort by frequency

object restrained action cause/effect
for every entry in wordRelationHistory for a given wordRelation (and gwhere=given where, and gobject=given associated object id)
query wordRelationHistory where where>gwhere-10 and where<gwhere and object id match [PAST]
OR query wordRelationHistory where where<gwhere+10 and where>gwhere and object id match [FUTURE]
sort by frequency

1. NOW at time expression timetransition check: 65091
2. adjust location tracking by time, to create a full timeline with location
3. [ sentence drives adjacency pair drives next sentence linked by relations]
identify all conversation pairs by type
relations
sentences
adjacency pairs
4. Question/Answer training - trace through all answers to questions from corpus
Determine all structures that are an answer to them. Analyze structure types and matching relations between the question and answer.
a. Who
b. When
c. Where
d. What
e. Why
f. How

Also, go by Google News. Get all articles about a person. 
Analyze all articles to confirm they are about the same person. 
Analyze whether the article is positive or negative about the person
Where they are
When they are
What are they doing

TODO: 
multiple processes:
central decision
hunger
curiousity
secondary relations correlator (thinking about what was just said)
alternate goals resolution mechanism
current central conversation theme 
alternate conversation themes

the use of language is driven by results!
study how things move in space - have a goal, scan for the goal being achieved and move backwards to find the language and context that enabled that action.

drive understanding through structure determined by adjacency pairs
at first, limit understanding to physical verbs and fundamental relations (work, play, etc)

constantly build a model of your audience
relations
where and when
locate the audience in context of the stories

drivers of conversation - 
inform - non-opinion - driven by 'theme' or common topic or summary (money/Mother)
goal -driven conversation - how do we get this accomplished?
biographies/biographies show how one person lived. This will help give a person-context/image of the other.
search then in biographies (Gutenberg LoC Class = CT (151 books match)) for this particular person/as many context clues as given
then project forward in time to search for goal (stated by other speaker?)
find love
make lots of money
become famous
will have to search also for synonyms/meanings
will also have to drive question/answer adjacency pairs
find happiness? http://kidshealth.org/kid/feeling/index.html
inform with opinion - (editorials?)
first we must establish opinions to defend or persuade with
we must measure how much the other person agrees/disagrees with our opinion and 
then based on which part of the opinion
counter it
feeling based conversation
mad/sad/glad/fear/shame/hurt - how to understand feelings?
type of conversation (inform/fix problem/persuasion) goal


different types of conversation (making some one else's internal state more consonant with yours):
persuasion: argument, debate
making some one else's internal state more consonant with yours:
why the U.S. should have more nuclear plants
why we should spend more money
Expressing agreement
Expressing criticism
Expressing support
Responding to criticism

fix problem - request for information or opinion
how to get more money [for us or for me], a common goal or a self-related or other-related goal
Asking a question
Disclosing personal information
Soliciting help
Soliciting comments
Putting out a wanted ad
Calling for action - Rallying support - Recruiting people - Giving a shout-out

inform - 
about 'dry' issue like economics (teach) or 
how to build a train
Answering a question
Making an observation
Acknowledging receipt of information
Advertising something or Giving a heads-up
Offering an opinion
Making a suggestion
Augmenting a previous post
internal issue like 'why I am mad at you'. 'I am sorry'.
Expressing surprise
Showing dismay
discuss without goal about common topic ("house", "children", "sports", "weather", "hobby", etc)
Making a joke


refresh relations in DB
analyze conversation and flow using 
current/historical internal/external environment for each person and narrator
1. INFORMATION - what information does each person know?
2. What new information can be provided and still maintain coherency?

moods - happy/sad/mad etc
corpus derived (laughed/cried/etc)
motivations money/power/etc



1. check before/after etc being used as conjunctions rather than prepositions
2. treat better: during (for time expressions that are META, whose prep objects don't involve 
time, or involving time conjunctions like when and after.
3. cut down the WordCache for wikipedia entries
4. single quote

1. Partners in Crime
2. N or M
3. BY the pricking of my thumbs
asher/lascarides - Logics of conversation
connectives - yet/hence/but
implicatures
amy ate some of the chocolate
also means amy did not eat all of the chocolate


eliminate these from Wikipedia search:
number -
anything with a , or a [ or a {
anything with an odd # of quotes
anything with the second word being a time unit, or a pronoun
action - hailing a taxi. Does that mean, if encountered at the end of an sg, that the character got into the car? Most likely.
go to lunch? going to dinner - look for places mentioned?
introduce location by activity - party, lunch, dinner etc.
phrases indicating future action
handle postProcessLinkedTimeExpressions.
in quote time expressions, either command
4611:call upon me tomorrow morning at 11 o'clock
7278: Come to - morrow at the same time
8771: Whittington was in a hurry to get rid of you[tuppence] this morning
9362: What are you[tuppence] doing this afternoon ?
10765: you[tuppence] would call round somewhere about lunch - time
16142: you[julius] don't know the name of the man who came this morning
17447: MOVE_OBJECT I[tuppence] got his[carter] reply this morning - in quote
31019: And to - day is Friday!
31919: - hadn't been here since Wednesday
32031: I[julius] haven't had one darned word from him[tommy] since we[julius,tuppence] parted at the depot on Wednesday .
35596: I[julius] shall be round in the car in half an hour . 
38750: I[james] will call upon her[marguerite] about ten o'clock
39169: I[tuppence] shall meet you[julius] at the Ritz at seven .
47883: She[marguerite] took an overdose of chloral last night 
47916: she[marguerite] was found dead this morning .
49831: To - day is Monday
27652 - morning post
Commands
involving an activity location:
07841:Let us go to Lunch. 
09415:Let us do dinner and a show
10525: you[tommy,tuppence] could call and see me[carter] at the above address[carshalton] at eleven o'clock to - morrow morning
involving a future intention:
00819:Let us get out of it. "EXIT" GENDERED FULL
09558:Come up with me, Tuppence
09704:Come on, Tuppence.
Other phrases [ WHEN ]
future intention at a time:
4129:I must return to my suite at the hostel. [ UNKNOWN FUTURE ]
9126:I shall go alone tomorrow. [ TOMORROW ]
10106:He had been bound ... to repair to the National Gallery, where his colleague would meet him at ten o'clock.
future intention tied to an action:
9186:When I come out I shan't speak to you [ I come out ]
9212:when he comes out of the building I shall drop a handkerchief [ He comes out of the building ]
future intention in an iterative (story-like) fashion
10611:ushered into the presence of Mr. Carter. [ AFTER previous statement ]
10686:I rejoin you in the road outside [ AFTER previous statement ]
10695:we proceed to the next address... [ AFTER previous statement ]
Questions
Where shall we meet? METAWQ Piccadilly Tube station
You followed me here?
Where to? METAWQ Paris.
A pensionnat? METAWQ Madame Columbier in the Avenue de Neuilly
How about the Ritz?
I go where?
??
You would go in the character of my ward
I prefer the Picadilly.
rendezvous and place and address are both 'where' anaphors

Also, collect phrases for maintenance of current time line.

verify place for each gendered object throughout book
includes future and past objects, which also implies time relations
current: 63 - why is 'others' matched to children?
do 'new' location for an object - add speakergroup/aging
'He finished painting in the gallery'
'Back at the Ritz, he looked through the pictures'
improve meta object resolution
collect and organize comments
analyze through entire corpus
generate new conversation based on stat
add logic/motivation
add front-end

transcript sources
http://www.texaslegacy.org/m/transcripts/index.html
http://www.pbs.org/wgbh/nova/transcripts/
http://www.pbs.org/nbr/site/onair/transcripts/archive/
http://www.pbs.org/newshour/bb/
news.google.com/archives

She was going to have not taken a bath for three days.
St. James - should be masculine (st.'s last name is always gender dominant)
St. James shouldn't match because it is an adjective inside of another NAME which is a place!
single noun with determiner - accumulate in BNC or when every other word has a relation
verb objects - accumulate BNC when no objects after active verb, one object or two objects when one is a pronoun
accumulate non-BNC without infinitive phrases (is 'to' a prep or not)
Why - an account of the character's wants and desires and teh subsequent fulfillment thereof
include mental states
context analysis (conversation)
http://www.idiomconnection.com/
www.phrases.org.uk
http://www.idiomsite.com/
http://www.idioms-today.com/
extend thinksay and internal verbs through verbNet - mark counts of how many in each class
implement Paice & Husk for pleonastic it
so anaphora - 
(1) “And with complete premeditation [they] resolved that His
Imperial Majesty Haile Selassie should be strangled because he was
head of the feudal system.” He was so strangled on Aug. 26, 1975,
in his bed most cruelly. (Chicago Tribune 12/15/94)
SYNTACTIC FORM AND DISCOURSE ACCESSIBILITY 3
(2) As an imperial statute the British North America Act could be
amended only by the British Parliament, which did so on several
occasions. (Groliers Encyclopedia)
zero anaphora - find the red blocks and stack up three
He found three blocks and she did also.
REMEMBER COPULAR VERBS 5.5 p.435 "to be" verbs
copular relations - 
He is the driver.
He was voted the driver.
He became the driver.
He was appointed the president.
He was named the doctor.
Indefinite anaphora - 
George bought some chocolates
A few are left.
attributes
NEWS corpora - changes in anaphora
Sophia Loren ....
The actress ...

if last name is resolved as a noun (not unknown), is not matched as speaker, and first name has no sex (not preidentified as a girl or boy name)
it is a NAME_OTHER object

example of how true reading is so heavily based on prediction and context that very little
information is needed (the first and last letters, and the rest out of order) to read:

"Cna yuo raed tihs? Olny 55 plepoe out of 100 can.

"i cdnuolt blveiee taht I cluod aulaclty uesdnatnrd waht I was rdanieg.
The phaonmneal pweor of the hmuan mnid, aoccdrnig to a rscheearch at
Cmabrigde Uinervtisy, it dseno't mtaetr in waht oerdr the ltteres in a
wrod are, the olny iproamtnt tihng is taht the frsit and lsat ltteer
be in the rghit pclae. The rset can be a taotl mses and you can sitll
raed it whotuit a pboerlm. Tihs is bcuseae the huamn mnid deos not
raed ervey lteter by istlef, but the wrod as a wlohe. Azanmig huh?
yaeh and I awlyas tghuhot slpeling was ipmorantt!"
does the verb 'to be' take any adverb?

does not include:
verbs attended by recurring time: ""

verbpart relation (bring him up)
remember adjectives, make different proper noun adjectives incompatible "Danver's Park, Coor's Park"
Jane as Tuppence should not be a name
clean up all present verbs still in narration

GIVER-RECEIVER MODEL
once time-flow is established for each speaker object
create giver receiver arrays, each entry has:
speaker object: Nancy
object: (animate object) Bill
role: (role from Nancy to Bill) Boss/Unknown
giver array
a noun: time #, Nancy gave him money
verb: time #, Nancy said to Bill: "Spy on Rachel. Steal the jewel.".
receiver array
a noun: time #, Nancy took the jewel.
verb: time #, Bill said to Nancy: "Now give me money."
object: Bart
role: Mother
giver array
a noun: time #, Nancy gave Bart a hug.
verb: time #, Nancy said to Bart: "Go to school".
receiver array
a noun: time #, Bart gave Nancy a kiss.

// don't attempt to estimate a verb tense appearing in the second position after another verb - this
// requires more sophisticated processing to get the tense of the verb immediately before this one - TODO
// incorporate verb tense I am going to do this, I am running to finish this, etc

make sure mainEntry isn't used to cross word-class boundries.
spoke noun mainEntry is speak - verb. Wrong!

include EVAL in costing for markChildren
~~~Willing to do anything, go anywhere. check 'willing'

update wordforms set count=50000 where wordid=30 and formid=62; // making sure 'but' is also a conjunction
update wordforms set count=7000 where wordid=7367 and formid=103; // making sure 'willing' is also a verb participle
James's is MALE and FEMALE? (4263)
ALSO count the embedded relative clause in __APPNOUN[2] as a relative clause, with appropriate costs and statistics
correct statistics of object relative clauses (IVOS)
*TEST also if the second object is sufficiently far away from the verb, it should incur a graduated cost as well.

word relations ratio
for each word relation present in sentence, wrr=(frequency of wr)/(all wr for word)
swrr=add wrr for each word in sentence/num words in sentence
when, who, where relations so that subjects can be tagged with time, person or place

do INFP phrases after verbs mean the verb has no more objects other than the INFP itself?

Why are there numbers in wordRelations?
also for a generic company (associated with all company names)
also for a generic geographic entity

verbs accepting two objects:
"accord","advance","afford","allot","allow","apportion","ask","assemble","assign","award","bake","bar","bear","begrudge","bequeath","bet","bid",
"blend","boil","brew","bring","build","buy","call","carve","cash","cast","catch","charter","chisel","chuck","churn","clean","clear","compile",
"cook","crochet","cut","dance","deal","deny","design","develop","dig","do","draw","drop","earn","embroider","ensure","envy","excuse","fashion",
"fetch","fill","fix","flash","fling","fold","forbid","foretell","forge","forgive","forward","fry","gain","gather","get","give","grant","grill",
"grind","grow","grudge","guarantee","hack","hammer","hand","hardboil","hatch","hire","hit","hum","iron","keep","knit","lay","lead","lease","leave",
"lend","light","loan","lose","mail","make","match","mint","mix","mold","occasion","offer","open","order","owe","paint","pass","pay","phone","pick",
"play","pluck","poach","pound","pour","prepare","procure","promise","pull","reach","read","recite","refuse","reimburse","remit","rent","reserve",
"roll","run","save","scramble","sculpt","secure","sell","send","set","sew","shape","shoot","show","sing","slaughter","softboil","spare","spin",
"steal","stitch","strike","take","teach","telegraph","tell","throw","tip","toss","vote","vouchsafe","wager","wash","weave","whistle","whittle",
"will","win","wire","wish","write",

prepositional verbs
barring bating concerning considering
disconcerning during enduring excusing
failing following induring inpending
lacking notwithstanding passing pending
regarding respecting saving selfregarding
touching transpassing unwanting wanting
barring excepting bating excepting

parse this sentence:
In the neighbourhood are: Alatri is divide into the following rioni (quarters): Chiappitto, Pacciano, Porpuro, Valle Santa Maria, Carvarola, 
Capranica, Fontana Vecchia, Maddalena, Piedimonte, Madonna delle Grazie, Melegranate, Montecapraro, Vignola, Valle Carchera, Montesantangelo, Montelarena, 
Pezza, Allegra, Basciano, Pignano, Castello, Collefreddo, Madonna del Pianto, Montelungo, Montereo, Monte San Marino, Pezzelle, Preturo, Sant'Antimo, 
San Valentino, Vallecupa, Vallefredda, Valle Pantano, Vallesacco, Valle S.Matteo, Villa Magna, Cassiano, Castagneto, Fraschette, Seritico, Santa Caterina, 
Vicero, Aiello, Canarolo, Collelavena, Costa San Vincenzo, Maranillo, Cavariccio, Colletraiano, Imbratto, Piano, S. Colomba, Scopigliette, Cucuruzzavolo, 
le Grotte, Magione, Mole Santa Maria, San Pancrazio, Vallemiccina, Sant'Emidio, Canale, Prati Giuliani, Quarticciolo, Quarti di Tecchiena, Tecchiena, 
Campello, Mole Bisleti, Cuione, Fontana Santo Stefano, Fontana Sistiliana, Frittola, S. Manno, Arillette, Collecuttrino, Colle del Papa, Laguccio, 
Montelena, Quercia d'Orlando, San Mattia, Carano, Fontana Scurano, Magliano, Cellerano, Fiume, Fiura, Fontana Santa, Riano, Abbadia, Case Paolone, 
Fontana Sambuco, Gaudo, Intignano, Colleprata. 

enhance agreement with the examples in comment before evaluateSubjectVerbAgreement.
when she thought of him, she yelled "How!". (speaker related speech not in a conversation)
abb as pertains to plural measurements restricted 10 oz 11 in, etc.
fix multi-subject relations
test relations with examples from both grammar books (clear up usage of REL1, REL2 and VERBREL1, VERBREL2)

create process to detect misspelled words and replace them with the pointers to correctly spelled words
and also replace rarely used numbers with ranges 'over 100 and under 200' 'over 1000 and under 1000000' 'many' etc.
verify objects, verify object roles, 'Some' should be plural only, it should NOT be male or FEMALE.
enhance groups by grouping together only people who react warmly to each other (not growling, or saying 'I hate you' etc).

?fix 'to' verb vs 'to' noun, so that 'to' (single noun without determiner) is of greater cost, and encourages INFP.
fix VERBREL as an object for a prepositional phrase - EXCEPT where preposition is 'to'.
limit links to objects in relatedObjects array to main noun for GENERAL_OBJECT_CLASSes

more information sources:
check all words against WordNet and make sure we have all the words WordNet 2.1 does
(add company form test if Proper_Noun - uncertain as to implementation)
http://www.hoovers.com/free/search/simple/xmillion/index.xhtml?query_string=General+Motors&which=company&page=1
look for: Hoover's Company Name Matches
www.time.com (all archives are open and free)

open issues:
incorporate internal body parts as clues to set internal state objects
Correct abbreviation discovery by using abbreviation rules delineated in computational linguistics book:
abbreviations usually don't have any vowels in them followed by lower case words, or punctuation
France should be a neuter object!
addCoords and addHyponyms could also extract gender tags
Incorporate ANC (Second Release) http://americannationalcorpus.org/SecondRelease/encoding.html
populate new word relation WordWithPrepObjectWord
What [makes of vehicles] participated in the Challenge?

think patterns could be replaced by a tag procedure like agree which increases the cost of a pattern if the verb is not a think verb but has an _S1 as object
correct words that have spaces split by a new line are not recognized as being in the same word
add parents to patterns to have reduceParent look for parents of a child pattern in log n, not the child pattern in order n
if a pattern has a child which has all the tags for a particular tagset, then block all patterns for that child from that tagset. This
prevents a parent pattern from checking for a tagset when the child would also check for it. This is good for a noun - determiner usage,
but may not be optimal for agreement patterns.
add gendered animals (cow, bull)
add gendered objects (ship, countries)
add plural to collective nouns (team, committee)
create occupational subclass: educational - 1st-grader, middle-schooler, junior, under-graduate...
create occupational subclass: political - democratic, republican, independent...
appearance (already taken care of by associatedAdjectives): shorty, dwarf
create occupational subclass: class/caste - peasant, bourgousie
racial/regional (but not country): aborigine, indian, african, asian, european - add to demonyms
by hobby/activity other than occupation: bicyclist, runner, 
attempt to combine pronoun definite selections with definite Proper_Noun's of the same gender and number.
attribute three-quarters, three-fifths to number
correct can't, don't etc contractions
keep female/male names as probabilities - so Robert has only a 1 % chance of being female.
meta language: the adjective "old" didn't describe them.
handle very long dashed words (by splitting them): kowabunga-mutant-ninja-turtle-most-unfortunately-named-clothing-company-of-the-century

/* feedback mechanism
run through BNC, noting correct class types. save in winnerForms

test BNC pattern violation and general BNC sentence parse correctness - eliminate multiple parses on one sentence as much as possible
run through matching process.
run eliminateLoserPatterns
for each position in sentence:
if no preferred forms, next position.
for beginPEMAPosition of m[position] till endPEMAPosition
if pema position returns isChildPattern false
if form is incorrect:
add all parents to 'incorrect' array. add last parent to incorrectLastParent if lastParent has isTopLevelMatch true for this position
if form is correct:
add all parents to 'correct' array.
if there are no parents in the correct array (unmatchable++), or incorrect array (correctlyMatched++), return.
for each parent in incorrect array that matches a parent in the correct array by begin, end, and parentElement,
increase the incorrectly matching child cost patternElementIndex by one.
if there are no matches, increase cost of each incorrectLastParent by one.
clear pma, pema
rerun

efficiency:
use Low Fragmentation heap by using HeapSetInformation
try not clearing TagSetMap in collectTags!
will use HANDLER interface to MyISAM, instead of SQL... faster?
ALSO think about converting forms to bit fields:
eliminating wordForms table, storing forms per word as a bit field within words table
make patterns more efficient:
{ LFS
computer 'first' of each pattern so that a set of patterns beginning with each form is created
compute first form and last form of each pattern
compute first and last strictly non-mandatory form of each pattern
collect all patterns which include a pattern - use for
for each pattern
for each element in pattern
for each index in pattern
if pattern index:
if first of pattern and lastSet has common members, log problem.
if form index:
if lastSet has form in it, log problem.
add up all the forms and the lasts of all the patterns of all the previous elements until a mandatory element is reached.
if the intersection of the first of the pattern and the set
compute overmatched by form and by pattern
}

debug hints:
use Win32 windbg
use application verifier
use this sympath:
.sympath c:\windows\system32;SRV*c:\localsymbols*http://msdl.microsoft.com/download/symbols;SRV*C:\WebSymb*http://msdl.microsoft.com/download/symbols
*/
int overallTime;
int initializeCounter(void);
void freeCounter(void);
void reportMemoryUsage(void);
int getInterviewTranscript();
int getTwitterEntries(wchar_t *filter);
bool TSROverride=false,flipTOROverride=false,flipTNROverride=false,logMatchedSentences=false,logUnmatchedSentences=false;

void no_memory () {
	lplog(LOG_FATAL_ERROR,L"Out of memory (new/STL allocation).");
	exit (1);
}

void setConsoleWindowSize(int width,int height)
{
	HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);      // Get Handle 
	_COORD coord;
	coord.X = max(200, width);
	coord.Y = max(6000, height);
	if (!SetConsoleScreenBufferSize(Handle, coord))            // Set Buffer Size 
		printf("Cannot set console buffer info to (%d,%d) (%d) %s\n", coord.X, coord.Y, (int)GetLastError(), LastErrorStr());

	//CONSOLE_SCREEN_BUFFER_INFOEX  csbiInfo;
	//csbiInfo.cbSize = sizeof(csbiInfo);
	// Get the current screen buffer size and window position. 
	//if (!GetConsoleScreenBufferInfoEx(Handle, &csbiInfo))
	//	printf("Cannot get console buffer info (%d) %s\n", (int)GetLastError(), LastErrorStr());
	//_COORD maxSize = GetLargestConsoleWindowSize(Handle);

	//printf("buffer sizex=%d buffer sizey=%d buffer cursor=(%d,%d) \nattributeflags=%d buffer window=(top=%d,left=%d,bottom=%d,right=%d) \nmax given buf=(%d,%d) max absolute=(%d,%d) popupAttributes=%d fullScreen=%d\n", 
	//	csbiInfo.dwSize.X, csbiInfo.dwSize.Y, csbiInfo.dwCursorPosition.X, csbiInfo.dwCursorPosition.Y,
	//	(int)csbiInfo.wAttributes, csbiInfo.srWindow.Top, csbiInfo.srWindow.Left, csbiInfo.srWindow.Bottom, csbiInfo.srWindow.Right,
	//	csbiInfo.dwMaximumWindowSize.X, csbiInfo.dwMaximumWindowSize.Y,
	//	maxSize.X,maxSize.Y,
	//	(int)csbiInfo.wPopupAttributes,(int)csbiInfo.bFullscreenSupported);

	//height = min(height, csbiInfo.dwMaximumWindowSize.Y);
	//width = min(width, csbiInfo.dwMaximumWindowSize.X);


	_SMALL_RECT Rect;
	Rect.Top = 0;
	Rect.Left = 0;
	Rect.Bottom = height - 1;
	Rect.Right = width - 1;

	if (!SetConsoleWindowInfo(Handle, TRUE, &Rect))            // Set Window Size 	SMALL_RECT srctWindow;
		printf("Cannot set console window info to (top=%d,left=%d,bottom=%d,right=%d) (%d) %s\n", 
			Rect.Top, Rect.Left, Rect.Bottom, Rect.Right,
			(int)GetLastError(), LastErrorStr());
}

int createLPProcess(int numProcess, HANDLE &processHandle, DWORD &processId, wchar_t *commandPath, wchar_t *processParameters)
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.wShowWindow = true;
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE | STARTF_USECOUNTCHARS | STARTF_USESHOWWINDOW;
	si.dwX = 60;
	si.dwY = 180 * numProcess;
	si.dwXSize = 300;
	si.dwYSize = 500;
	si.dwXCountChars = 180;
	si.dwYCountChars = 3000;
	si.wShowWindow = SW_SHOWNOACTIVATE; // don't continuously hijack focus
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcess(commandPath,
		processParameters, // Command line
		NULL, // Process handle not inheritable
		NULL, // Thread handle not inheritable
		FALSE, // Set handle inheritance to FALSE
		CREATE_NEW_CONSOLE,
		NULL, // Use parent's environment block
		NULL, // Use parent's starting directory 
		&si, // Pointer to STARTUPINFO structure
		&pi) // Pointer to PROCESS_INFORMATION structure
		)
	{
		wchar_t cwd[1024];
		printf("CreateProcess of %S failed (%d) %s in %S.\n", processParameters, (int)GetLastError(), LastErrorStr(), _wgetcwd(cwd, 1024));
		return -1;
	}
	processHandle = pi.hProcess;
	processId = pi.dwProcessId;
	return 0;
}

int getNumSourcesProcessed(Source &source, int &numSourcesProcessed, __int64 &wordsProcessed, __int64 &sentencesProcessed)
{
	MYSQL_RES * result;
	if (!myquery(&source.mysql, L"LOCK TABLES sources WRITE")) return -1;
	if (myquery(&source.mysql, L"select COUNT(id), SUM(numWords), SUM(numSentences) from sources where sourceType = 2 and processed IS not NULL and processing IS NULL and start != '**SKIP**' and start != '**START NOT FOUND**'",result))
	{
		MYSQL_ROW sqlrow = NULL;
		if (sqlrow = mysql_fetch_row(result))
		{
			numSourcesProcessed = atoi(sqlrow[0]);
			wordsProcessed = atol(sqlrow[1]);
			sentencesProcessed = atoi(sqlrow[2]);
		}
		mysql_free_result(result);
	}
	if (!myquery(&source.mysql, L"UNLOCK TABLES")) return -1;
	return 0;
}

// https://stackoverflow.com/questions/813086/can-i-send-a-ctrl-c-sigint-to-an-application-on-windows/1179124
// Inspired from http://stackoverflow.com/a/15281070/1529139
// and http://stackoverflow.com/q/40059902/1529139
bool signalCtrl(DWORD dwProcessId, DWORD dwCtrlEvent)
{
	bool success = false;
	DWORD thisConsoleId = GetCurrentProcessId();
	// Leave current console if it exists
	// (otherwise AttachConsole will return ERROR_ACCESS_DENIED)
	bool consoleDetached = (FreeConsole() != FALSE);

	if (AttachConsole(dwProcessId) != FALSE)
	{
		// Add a fake Ctrl-C handler for avoid instant kill is this console
		// WARNING: do not revert it or current program will be also killed
		SetConsoleCtrlHandler(nullptr, true);
		success = (GenerateConsoleCtrlEvent(dwCtrlEvent, 0) != FALSE);
		FreeConsole();
	}

	if (consoleDetached)
	{
		// Create a new console if previous was deleted by OS
		if (AttachConsole(thisConsoleId) == FALSE)
		{
			int errorCode = GetLastError();
			if (errorCode == 31) // 31=ERROR_GEN_FAILURE
			{
				AllocConsole();
			}
		}
	}
	return success;
}

int startProcesses(Source &source, int processKind, int step, int beginSource, int endSource, Source::sourceTypeEnum processSourceType, int maxProcesses, int numSourcesPerProcess,
	bool forceSourceReread, bool sourceWrite, bool sourceWordNetRead, bool sourceWordNetWrite,bool makeCopyBeforeSourceWrite,bool parseOnly, wstring specialExtension)
{
	LFS
	chdir("source");
	bool sentBreakSignals = false;
	int startTime = clock();
	HANDLE *handles = (HANDLE *)calloc(maxProcesses, sizeof(HANDLE));
	int numProcesses = 0, errorCode = 0,numSourcesProcessedOriginally =0;
	__int64 wordsProcessedOriginally = 0, sentencesProcessedOriginally = 0;
	getNumSourcesProcessed(source, numSourcesProcessedOriginally, wordsProcessedOriginally, sentencesProcessedOriginally);
	int numSourcesLeft = source.getNumSources(true);
	maxProcesses = min(maxProcesses, numSourcesLeft);
	wstring tmpstr;
	while (!errorCode)
	{
		unsigned int nextProcessIndex = numProcesses;
		if (numProcesses == maxProcesses)
		{
			nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, false, 1000 * 60 * 5, false);
			if (nextProcessIndex == WAIT_FAILED)
			{
				if (!numProcesses)
					break;
				lplog(LOG_FATAL_ERROR, L"WaitForMultipleObjectsEx failed with error %s", getLastErrorMessage(tmpstr));
			}
			numSourcesLeft = 0;
			int numSourcesProcessedNow = 0;
			__int64 wordsProcessedNow = 0, sentencesProcessedNow = 0;
			getNumSourcesProcessed(source, numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
			int processingSeconds=(clock() - startTime) / CLOCKS_PER_SEC;
			wchar_t consoleTitle[1500];
			numSourcesProcessedNow -= numSourcesProcessedOriginally;
			wordsProcessedNow -= wordsProcessedOriginally;
			sentencesProcessedNow -= sentencesProcessedOriginally;
			wsprintf(consoleTitle, L"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].", 
				numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds/3600, (processingSeconds % 3600)/60, processingSeconds % 60, numSourcesProcessedNow*3600/processingSeconds, wordsProcessedNow *3600/processingSeconds);
			SetConsoleTitle(consoleTitle);

			if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
				continue;
			if (nextProcessIndex < WAIT_OBJECT_0 + numProcesses) // nextProcessIndex >= WAIT_OBJECT_0 && 
			{
				nextProcessIndex -= WAIT_OBJECT_0;
				CloseHandle(handles[nextProcessIndex]);
				printf("\nClosing process %d", nextProcessIndex);
			}
			if (nextProcessIndex >= WAIT_ABANDONED_0 && nextProcessIndex < WAIT_ABANDONED_0 + numProcesses)
			{
				nextProcessIndex -= WAIT_ABANDONED_0;
				printf("\nClosing process %d [abandoned]", nextProcessIndex);
				CloseHandle(handles[nextProcessIndex]);
			}
		}
		int id, repeatStart, prId;
		wstring start, path, encoding, etext, author, title, pathInCache;
		bool result=true;
		if (processKind == 1 && numSourcesLeft > 0)
			numSourcesLeft--;
		else
		{
			switch (processKind)
			{
			case 0:result = source.getNextUnprocessedParseRequest(prId, pathInCache); break;
			case 1:result = source.getNextUnprocessedSource(beginSource, endSource, false, id, path, encoding, start, repeatStart, etext, author, title); break;
			case 2:result = source.anymoreUnprocessedForUnknown(step); break;
			default:result = false; break;
			}
		}
		if (!result)
		{
			if (numProcesses == maxProcesses)
			{
				memmove(handles + nextProcessIndex, handles + nextProcessIndex + 1, (maxProcesses - nextProcessIndex - 1) * sizeof(handles[0]));
				numProcesses--;
			}
			if (numProcesses)
			{
				printf("\nNo more processes to be created. %d processes left to wait for.", numProcesses);
				while (numProcesses)
				{
					nextProcessIndex = WaitForMultipleObjectsEx(numProcesses, handles, false, 1000 * 60 * 3, false);
					if (nextProcessIndex == WAIT_FAILED)
						lplog(LOG_FATAL_ERROR, L"\nWaitForMultipleObjectsEx failed with error %s", getLastErrorMessage(tmpstr));
					int numSourcesProcessedNow = 0;
					__int64 wordsProcessedNow = 0, sentencesProcessedNow = 0;
					getNumSourcesProcessed(source, numSourcesProcessedNow, wordsProcessedNow, sentencesProcessedNow);
					int processingSeconds = (clock() - startTime) / CLOCKS_PER_SEC;
					wchar_t consoleTitle[1500];
					numSourcesProcessedNow -= numSourcesProcessedOriginally;
					wordsProcessedNow -= wordsProcessedOriginally;
					sentencesProcessedNow -= sentencesProcessedOriginally;
					wsprintf(consoleTitle, L"sources=%06d:sentences=%06I64d:words=%08I64d in %02d:%02d:%02d [%d sources/hour] [%I64d words/hour].",
						numSourcesProcessedNow, sentencesProcessedNow, wordsProcessedNow, processingSeconds / 3600, (processingSeconds % 3600) / 60, processingSeconds % 60, numSourcesProcessedNow * 3600 / processingSeconds, wordsProcessedNow * 3600 / processingSeconds);
					lplog(LOG_INFO | LOG_ERROR, L"%s", consoleTitle);
					SetConsoleTitle(consoleTitle);
					if (nextProcessIndex == WAIT_IO_COMPLETION || nextProcessIndex == WAIT_TIMEOUT)
						continue;
					if (nextProcessIndex < WAIT_OBJECT_0 + numProcesses) // nextProcessIndex >= WAIT_OBJECT_0 && 
					{
						nextProcessIndex -= WAIT_OBJECT_0;
						CloseHandle(handles[nextProcessIndex]);
						printf("\nClosing process %d", nextProcessIndex);
					}
					if (nextProcessIndex >= WAIT_ABANDONED_0 && nextProcessIndex < WAIT_ABANDONED_0 + numProcesses)
					{
						nextProcessIndex -= WAIT_ABANDONED_0;
						printf("\nClosing process %d [abandoned]", nextProcessIndex);
						CloseHandle(handles[nextProcessIndex]);
					}
					memmove(handles + nextProcessIndex, handles + nextProcessIndex + 1, (maxProcesses - nextProcessIndex - 1) * sizeof(handles[0]));
					numProcesses--;
				}
			}
			break;
		}
		if (exitNow || exitEventually)
		{
			printf("\nSending break signals to children...\n");
			if (!sentBreakSignals)
			{
				for (int p = 0; p < numProcesses; p++)
				{
					int pid = GetProcessId(handles[p]);
					signalCtrl(pid, CTRL_C_EVENT);
				}
				sentBreakSignals = true;
			}
		}
		else
		{
			HANDLE processHandle = 0;
			DWORD processId = 0;
			wchar_t processParameters[1024];
			switch (processKind)
			{
			case 0:
				wsprintf(processParameters, L"ParseAllSourcesx64\\lp.exe -ParseRequest \"%s\" -cacheDir %s %s%s%s%s%s%s%s%s-log %s.%d", pathInCache.c_str(), CACHEDIR,
					(forceSourceReread) ? L"-forceSourceReread " : L"",
					(sourceWrite) ? L"-SW " : L"",
					(sourceWordNetRead) ? L"-SWNR " : L"",
					(sourceWordNetWrite) ? L"-SWNW " : L"",
					(parseOnly) ? L"-parseOnly " : L"",
					(makeCopyBeforeSourceWrite) ? L"-MCSW " : L"",
					(logMatchedSentences) ? L"-logMatchedSentences " : L"",
					(logUnmatchedSentences) ? L"-logUnmatchedSentences " : L"",
					specialExtension.c_str(),
					nextProcessIndex);
				if (errorCode = createLPProcess(nextProcessIndex, processHandle, processId, L"ParseAllSourcesx64\\lp.exe", processParameters) < 0)
					break;
				break;
			case 1:
				wsprintf(processParameters, L"ParseAllSourcesx64\\lp.exe -book 0 + -BC 0 -cacheDir %s %s%s%s%s%s%s%s%s-numSourceLimit %d -log %s.%d", CACHEDIR,
					(forceSourceReread) ? L"-forceSourceReread " : L"",
					(sourceWrite) ? L"-SW " : L"",
					(sourceWordNetRead) ? L"-SWNR " : L"",
					(sourceWordNetWrite) ? L"-SWNW " : L"",
					(parseOnly) ? L"-parseOnly " : L"",
					(makeCopyBeforeSourceWrite) ? L"-MCSW " : L"",
					(logMatchedSentences) ? L"-logMatchedSentences " : L"",
					(logUnmatchedSentences) ? L"-logUnmatchedSentences " : L"",
					numSourcesPerProcess,
					specialExtension.c_str(),
					nextProcessIndex);
				if (specialExtension.length() > 0)
				{
					wcscat(processParameters, L" -specialExtension ");
					wcscat(processParameters, specialExtension.c_str());
				}
				if (errorCode = createLPProcess(nextProcessIndex, processHandle, processId, L"ParseAllSourcesx64\\lp.exe", processParameters) < 0)
					break;
				break;
			case 2:
				wsprintf(processParameters, L"x64\\StanfordAllSources\\CorpusAnalysis.exe -step %d -numSourceLimit %d -log %s.%d", CACHEDIR, step, numSourcesPerProcess, specialExtension.c_str(),nextProcessIndex);
				if (errorCode = createLPProcess(nextProcessIndex, processHandle, processId, L"x64\\StanfordAllSources\\CorpusAnalysis.exe", processParameters) < 0)
					break;
				break;
			default: break;
			}
			handles[nextProcessIndex] = processHandle;
			if (numProcesses < maxProcesses)
				numProcesses++;
			printf("\nCreated process %d:%d", nextProcessIndex, (int)processId);
		}
	}
	if (processSourceType != Source::REQUEST_TYPE)
	{
		freeCounter();
		_exit(0); // fast exit
	}
	free(handles);
	chdir("..");
	return 0;
}

SRWLOCK rdfTypeMapSRWLock,mySQLTotalTimeSRWLock,totalInternetTimeWaitBandwidthControlSRWLock,mySQLQueryBufferSRWLock,orderedHyperNymsMapSRWLock;
void createLocks(void)
{ LFS
	InitializeSRWLock(&rdfTypeMapSRWLock);
	InitializeSRWLock(&mySQLTotalTimeSRWLock);
	InitializeSRWLock(&totalInternetTimeWaitBandwidthControlSRWLock);
	InitializeSRWLock(&mySQLQueryBufferSRWLock);
}

int WRMemoryCheck(MYSQL mysql)
{
	int numRowsOnDisk = 0, numRowsInMemory = 0;
	MYSQL_ROW sqlrow;
	MYSQL_RES *result = NULL;
	if (!myquery(&mysql, L"LOCK TABLES wordRelationsMemory WRITE,wordRelations READ"))
		return -1;
	if (!myquery(&mysql, L"SELECT COUNT(sourceId) from wordRelationsMemory", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsInMemory = atoi(sqlrow[0]);
	else
		return -1;
	if (numRowsInMemory > 0)
	{
		myquery(&mysql, L"UNLOCK TABLES");
		return 0;
	}
	if (!myquery(&mysql, L"SELECT COUNT(sourceId) from wordRelations", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsOnDisk = atoi(sqlrow[0]);
	printf("Loading wordrelations into memory...\n");
	if (!myquery(&mysql, L"insert into wordrelationsmemory select id, sourceId, lastWhere, fromWordId, toWordId, typeId, totalCount from wordrelations"))
		return -1;
	printf("Finished wordrelations into memory...\n");
	if (!myquery(&mysql, L"SELECT COUNT(sourceId) from wordRelationsMemory", result))
		return -1;
	if ((sqlrow = mysql_fetch_row(result)) != NULL)
		numRowsInMemory = atoi(sqlrow[0]);
	else
		return -1;
	myquery(&mysql, L"UNLOCK TABLES");
	return (numRowsInMemory == numRowsOnDisk) ? 0 : -1;
}

//SleepConditionVariableSRW	Sleeps on the specified condition variable and releases the specified lock as an atomic operation.
//TryAcquireSRWLockExclusive	Attempts to acquire a slim reader/writer (SRW) lock in exclusive mode. If the call is successful, the calling thread takes ownership of the lock.
//TryAcquireSRWLockShared	Attempts to acquire a slim reader/writer (SRW) lock in shared mode. If the call is successful, the calling thread takes ownership of the lock.

// -test timeExpressions -retry -BC 0 -cacheDir M:\caches -forceSourceReread -parseOnly -SW -SWNR -SWNW 
// -test tokenization -BC 0 -cacheDir M:\caches -SW -SWNR -SWNW -forceSourceReread -retry
// -test thatParsing -BC 0 -cacheDir M:\caches -SW -SWNR -SWNW -forceSourceReread -retry
// parse one gutenberg book repeatedly:
// -book 3537 3538 -BC 0 -cacheDir M:\caches -SW -SWNR -SWNW -forceSourceReread -numSourcesPerProcess 15 -retry -parseOnly
// -book 0 -BC 0 -retry -cacheDir M:\caches -SR -SW -SWNR -SWNW -TNMS
// parse gutenberg books all at once parcelled out (parseOnly):
// -book 0 + -BC 0 -cacheDir M:\caches -SWNR -SWNW -SW -MCSW -logMatchedSentences -logUnmatchedSentences -numSourcesPerProcess 15 -forceSourceReread -parseOnly -retry -mp 8
// do TREC:
// -Interactive 0 + -BC 0 -cacheDir J:\caches -SR -SW -SWNR -SWNW -TNMS
void redistributeFilesAlphabetically(wchar_t *dir);
int numSourceLimit = 0;
int wmain(int argc,wchar_t *argv[])
{
	// Create a dump file whenever this program crashes (only on windows)
	SetUnhandledExceptionFilter(unhandled_handler);
	wstring testbuffer;
	//int readPageFromSolr(const wchar_t *queryParams, wstring &buffer);
	//readPageFromSolr(L"{ params: { q: \"*:*\",rows:1 } }", testbuffer);
	//if (!testbuffer.empty())
	//	return 0;
	bool viterbiTest = false;
	setConsoleWindowSize(100, 8);
	cProfile profile("");
	createLocks();
	wchar_t dir[1024];
	GetCurrentDirectoryW(1024,dir);
	set_new_handler(no_memory);
	chdir("..");
	overallTime=clock();
	// test gutenberg by generating usage statistics
	//if (argc>1 && !wcscmp(argv[1],L"-tg"))
	//{
	//	Source source2(L"localhost",0,false,true,true);
	//	source2.testStartCode();
	//	return 0;
	//}
	SetConsoleCtrlHandler(ConsoleHandler, true);
	if (remove("wcheck.lplog")==ERROR_SHARING_VIOLATION) return -1;
	initializeCounter();
	//void extractFromWiktionary(wchar_t *f);
	//extractFromWiktionary(LMAINDIR+L"\\Linguistics information\\TEMP-E20120211.tsv");
	/* test over */
	if (argc>2 && !_wcsicmp(argv[1],L"-acquireMovieList"))
		return acquireList(argv[2]);
	if (argc>2 && !_wcsicmp(argv[1],L"-acquireInterviewTranscript"))
		return getInterviewTranscript();
	if (argc==2 && wcsstr(argv[1],L"-acquireTwitter"))
		return getTwitterEntries(argv[1]);
	int numCommandLineParameters=argc;
	wchar_t *sourceHost=L"localhost";
	cacheDir=CACHEDIR;
	bool resetAllSource=false,resetProcessingFlags=false,generateFormStatistics=false,retry=false;
	bool forceSourceReread=false,sourceWrite=false,sourceWordNetRead=false,sourceWordNetWrite=false,parseOnly=false, makeCopyBeforeSourceWrite=false;
	int numSourcesPerProcess = 5; 
	wstring specialExtension;
	for (int I=0; I<argc; I++)
	{
		if (!_wcsicmp(argv[I],L"-server") && I<argc-1)
			sourceHost=argv[++I];
		else if (!_wcsicmp(argv[I],L"-log") && I<argc-1)
			logFileExtension=argv[++I];
		else if (!_wcsicmp(argv[I], L"-mp") && I < argc - 1)
			multiProcess = _wtoi(argv[++I]);
		else if (!_wcsicmp(argv[I], L"-numSourcesPerProcess") && I < argc - 1)
			numSourcesPerProcess = _wtoi(argv[++I]);
		else if (!_wcsicmp(argv[I],L"-numSourceLimit") && I<argc-1)
			numSourceLimit=_wtoi(argv[++I]);
		else if (!_wcsicmp(argv[I],L"-cacheDir") && I<argc-1)
			cacheDir=argv[++I];
		else if (!_wcsicmp(argv[I],L"-resetAllSource"))
			resetAllSource=true;
		else if (!_wcsicmp(argv[I],L"-resetProcessingFlags"))
			resetProcessingFlags=true;
		else if (!_wcsicmp(argv[I],L"-generateFormStatistics"))
			generateFormStatistics=true;
		else if (!_wcsicmp(argv[I],L"-retry"))
			retry=true;
		else if (!_wcsicmp(argv[I],L"-parseOnly"))
			parseOnly=true;
		else if ((!_wcsicmp(argv[I],L"-LC") || !_wcsicmp(argv[I],L"-logCache")) && I<argc-1)
			logCache=_wtoi(argv[I+1]);
		else if ((!_wcsicmp(argv[I],L"-BC")) && I<argc-1) // bandwidth control
			Internet::bandwidthControl=_wtoi(argv[I+1]);
		else if (!_wcsicmp(argv[I],L"-logMatchedSentences"))
			logMatchedSentences=true;
		else if (!_wcsicmp(argv[I],L"-logUnmatchedSentences"))
			logUnmatchedSentences=true;
		else if (!_wcsicmp(argv[I],L"-TSRO"))
			TSROverride=true;
		else if (!_wcsicmp(argv[I],L"-fTOR"))
			flipTOROverride=true;
		else if (!_wcsicmp(argv[I],L"-fTNR"))
			flipTNROverride=true;
		else if (!_wcsicmp(argv[I], L"-forceSourceReread"))
			forceSourceReread = true;
		else if (!_wcsicmp(argv[I], L"-SW"))
			sourceWrite = true;
		else if (!_wcsicmp(argv[I], L"-MCSW"))
			makeCopyBeforeSourceWrite = true;
		else if (!_wcsicmp(argv[I],L"-SWNR"))
			sourceWordNetRead=true;
		else if (!_wcsicmp(argv[I], L"-SWNW"))
			sourceWordNetWrite = true;
		else if (!_wcsicmp(argv[I], L"-specialExtension"))
			specialExtension = argv[++I];
		else
			continue;
		numCommandLineParameters=min(numCommandLineParameters,I);
	}
	/*
	vector <mbInfoRecordingType> mbRecordingsTypes;
	vector <mbInfoReleaseType> mbReleasesTypes;
	vector <mbInfoArtistType> mbArtistsTypes;
	getArtists(L"artist",L"Jay-Z",mbArtistsTypes);
	getArtists(L"compactLabel",L"Roc-A-Fella Records",mbArtistsTypes);
	getReleases(L"compactLabel",L"Roc-A-Fella Records",mbReleasesTypes);
	getRecordings(L"artist",L"Jay-Z",mbRecordingsTypes);
	*/
	vector <int> badSpeakers;
	int sourceArgs = -1;
	enum Source::sourceTypeEnum sourceType= Source::NO_SOURCE_TYPE;
	const wchar_t *where;
	for (int I = 1; I < numCommandLineParameters - 1; I++)
	{
		wstring arg = argv[I];
		std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
		if ((where = wcsstr(L"1-test 2-book 3-newsbank 4-bnc 5-script 6-websearch 7-wikipedia 8-interactive 9-parserequest", arg.c_str())))
		{
			sourceType = (enum Source::sourceTypeEnum)(where[-1] - '0');
			sourceArgs = I;
			break;
		}
	}
	if (sourceArgs==-1)
		lplog(LOG_FATAL_ERROR,L"Source type not found.");
	if (_waccess(cacheDir,0)<0)
		lplog(LOG_FATAL_ERROR,L"Cache directory %s does not exist!",cacheDir);
	Source source(sourceHost,sourceType,generateFormStatistics,multiProcess>0,true);
	if (multiProcess > 0 || numSourceLimit == 0) // controller or a single process not under control
		WRMemoryCheck(source.mysql);

	if (resetAllSource) source.resetAllSource();
	if (resetProcessingFlags) source.resetProcessingFlags();
	source.initializeNounVerbMapping();
	unsigned int totalQuotations=0,quotationExceptions=0,unknownCount=0;
	// build thesaurus 
	//int getThesaurus();
	//getThesaurus();
	//source.createThesaurusTables();
	//extern vector <sDefinition> thesaurus;
	//for (int I = 0; I < thesaurus.size(); I++)
	//	source.writeThesaurusEntry(thesaurus[I]);
	// synonym testing
	//void testThesaurus();
	//testThesaurus();
	//vector <set <wstring> > synonyms;
	//source.getWordNetSynonymsOnly(L"car", synonyms, 1);
	//for (int I = 0; I < synonyms.size(); I++)
		//for (set<wstring>::iterator ss = synonyms[I].begin(), ssEnd = synonyms[I].end(); ss != ssEnd; ss++)
			//printf("%d:%S\n", I, ss->c_str());
	if (multiProcess==0)
		initializePatterns();
	unordered_map <int, vector < vector <tTagLocation> > > emptyMap;
	source.pemaMapToTagSetsByPemaByTagSet.reserve(desiredTagSets.size());
	for (unsigned int ts=0; ts<desiredTagSets.size(); ts++)
		source.pemaMapToTagSetsByPemaByTagSet.push_back(emptyMap);
	if (sourceType == Source::sourceTypeEnum::PATTERN_TRANSFORM_TYPE)
	{
		wchar_t consoleTitle[1500];
#ifdef _DEBUG
		wchar_t dflag = L'D';
#else
		wchar_t dflag = L'R';
#endif
			wsprintf(consoleTitle, L"[%c] %s...", dflag,argv[sourceArgs+1]);
		_putws(consoleTitle);
		lplog(LOG_INFO | LOG_ERROR, L"%s\n", consoleTitle);
		SetConsoleTitle(consoleTitle);
		source.unlockTables();
		Words.addMultiWordObjects(source.multiWordStrings, source.multiWordObjects);
		Source *requestedSource;
		return source.processPath(argv[sourceArgs+1], requestedSource, Source::WEB_SEARCH_SOURCE_TYPE, 1, false);
	}
	int globalTotalUnmatched=0,globalOverMatchedPositionsTotal=0,numWords=0;
	if (iswdigit(argv[sourceArgs+1][0]))
	{
		int beginSource=_wtoi(argv[sourceArgs + 1]),endSource;
		endSource = (argv[sourceArgs + 2][0] == '+') ? -1 : ((iswdigit(argv[sourceArgs + 2][0])) ? _wtoi(argv[sourceArgs + 2]) : beginSource + 1);
		if (retry)
		{
			wprintf(L"Resetting sources...               \r");
			source.resetSource(beginSource, endSource);
		}
		if (multiProcess>0)
		{
			HWND consoleWindowHandle = GetConsoleWindow();
			SetWindowPos(consoleWindowHandle, HWND_NOTOPMOST, 900, 0, 700, 180, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
			startProcesses(source, 1,0,beginSource, endSource, sourceType, multiProcess, numSourcesPerProcess, forceSourceReread, sourceWrite, sourceWordNetRead, sourceWordNetWrite,makeCopyBeforeSourceWrite,parseOnly,specialExtension);
			return 0;
		}
		wprintf(L"Getting number of sources to process...               \r");
		int numSources = source.getNumSources(false);
		int numSourcesProcessed=0,pid= GetCurrentProcessId();
		while (!exitNow && !exitEventually && (numSourceLimit==0 || numSourcesProcessed++<numSourceLimit))
		{
			int sourceId, repeatStart;
			wstring path, encoding, etext, author, title, start;
			wprintf(L"Getting number of sources left...               \r");
			int numSourcesLeft = source.getNumSources(true);
			if (!source.getNextUnprocessedSource(beginSource, endSource, true, sourceId, path, encoding, start, repeatStart, etext, author, title))
				break;
			path.insert(0, L"\\");
			path=path.insert(0,TEXTDIR);
			wchar_t consoleTitle[1500];
			wsprintf(consoleTitle,L"[%03d:%03d-%03d:%03d%%]PID%05d %s '%s'...",sourceId,beginSource,numSources, (numSources-numSourcesLeft)*100/numSources,pid, (start == L"**SKIP**" || start == L"**START NOT FOUND**") ? L"Skipping":L"",title.c_str());
			_putws(consoleTitle);
			lplog(LOG_INFO|LOG_ERROR,L"%s\n", consoleTitle);
			SetConsoleTitle(consoleTitle);
			source.unlockTables();
			if (start == L"**SKIP**")
				continue;
			Words.addMultiWordObjects(source.multiWordStrings,source.multiWordObjects);
			wstring rt1,rt2;
			int ret=0;
			bool parsedOnly = false;
			if (forceSourceReread || !source.readSource(path,false, parsedOnly, true,true, specialExtension))
			{
				unknownCount=0;
				switch (sourceType)
				{
				case Source::TEST_SOURCE_TYPE:
				case Source::GUTENBERG_SOURCE_TYPE:
				case Source::WIKIPEDIA_SOURCE_TYPE:
				case Source::INTERACTIVE_SOURCE_TYPE:
				case Source::WEB_SEARCH_SOURCE_TYPE:
				case Source::NEWS_BANK_SOURCE_TYPE:
					if ((ret=source.tokenize(title,etext,path,encoding,start,repeatStart,unknownCount))<0)
					{
						lplog(LOG_ERROR,L"ERROR:Unable to parse %s - %d (start=%s, repeatStart=%d).",path.c_str(),ret,start.c_str(),repeatStart);
						continue;
					}
					quotationExceptions=source.doQuotesOwnershipAndContractions(totalQuotations);
					break;
				case Source::BNC_SOURCE_TYPE:
				{
					source.beginClock = clock();
					bncc bnc;
					bnc.process(source, sourceId, path);
					source.adjustWords();
					unknownCount = bnc.unknownCount;
					break;
				}
				case Source::NO_SOURCE_TYPE:
				case Source::SCRIPT_SOURCE_TYPE:
				case Source::PATTERN_TRANSFORM_TYPE:
				case Source::REQUEST_TYPE:
				default: break;
				}
				//int cap=source.m.capacity();
				source.m.shrink_to_fit(); // C++ 11 only
				//int cap2=source.m.capacity();
				source.sourceId = sourceId;
				int totalUnmatched = source.printSentences(true, unknownCount, quotationExceptions, totalQuotations, globalOverMatchedPositionsTotal);
				if (totalUnmatched < 0)
					lplog(LOG_FATAL_ERROR, L"Cannot print sentences.");
				globalTotalUnmatched+=totalUnmatched;
			}
			else
			{
				lplog(LOG_INFO,L"%s already parsed.",path.c_str());
				source.m.shrink_to_fit(); // C++ 11 only
				source.printSentencesCheck(false);
			}
			numWords+=source.m.size();
			//source.accumulateNewPatterns();
			//source.printAccumulatedPatterns();
			sourceWordNetWrite=(!sourceWordNetRead || !source.readWNMaps(path)) && sourceWordNetWrite;
			source.addWNExtensions();
			// noun/verb class analysis (debugging)
			//bool measurableObject,notMeasurableObject,grouping;
			//analyzeNounClass(0,L"fish",0,measurableObject,notMeasurableObject,grouping,t);
			//wstring proposedSubstitute;
			//int inflectionFlags=0;
			//bool isNoun=false,isVerb=true,isAdjective=false,isAdverb=false;
			//analyzeSense(false,L"draft",proposedSubstitute,numIrregular,inflectionFlags,isNoun,isVerb,isAdjective,isAdverb);
			puts("");
			source.identifyObjects();
			vector <int> secondaryQuotesResolutions;
			source.analyzeWordSenses();
			source.narrativeIsQuoted = sourceType != Source::GUTENBERG_SOURCE_TYPE;
			source.syntacticRelations();
			lplog();
			if (sourceWrite)
			{
				source.write(path, false, makeCopyBeforeSourceWrite, specialExtension);
				source.writeWords(path, specialExtension);
				source.writePatternUsage(path, true);
			}
			if (parseOnly || viterbiTest)
			{
				if (viterbiTest)
				{
					void testViterbiFromSource(Source &source);
					testViterbiFromSource(source);
				}
				source.clearSource();
				if (source.updateWordUsageCostsDynamically)
					WordClass::resetUsagePatternsAndCosts(source.debugTrace);
				else
					WordClass::resetCapitalizationAndProperNounUsageStatistics(source.debugTrace);
				if (!exitNow) source.signalFinishedProcessingSource(sourceId);
				continue;
			}
			//source.printVerbFrequency();
			source.identifySpeakerGroups(); 
			source.resolveSpeakers(secondaryQuotesResolutions);
			source.resolveFirstSecondPersonPronouns(secondaryQuotesResolutions);
			source.printObjects();
			source.resolveWordRelations();
			if (sourceWrite && !source.write(path,true, false,specialExtension))
				lplog(LOG_FATAL_ERROR,L"buffer overrun");
			source.printResolutionCheck(badSpeakers);
			source.logSpaceCheck();
			source.identifyConversations();
			//source.printResolutions(badSpeakers,false);
			if (sourceWordNetWrite)
				source.writeWNMaps(path);
			if (source.debugTrace.traceSpeakerResolution)
			{
				source.printTenseStatistics(L"Narrator",source.narratorTenseStatistics,source.numTotalNarratorVerbTenses);
				source.printTenseStatistics(L"Speaker",source.speakerTenseStatistics,source.numTotalSpeakerVerbTenses);
				source.printTenseStatistics(L"Narrator All Tenses",source.narratorFullTenseStatistics,source.numTotalNarratorFullVerbTenses);
				source.printTenseStatistics(L"Speaker All Tenses",source.speakerFullTenseStatistics,source.numTotalSpeakerFullVerbTenses);
				if (source.debugTrace.traceSpeakerResolution)
					source.printSectionStatistics();
			}
			lplog();
			if (source.sourceInPast=source.sourceType==Source::INTERACTIVE_SOURCE_TYPE)
				source.matchBasicElements(parseOnly,false);
			if (!exitNow) source.signalFinishedProcessingSource(sourceId);
			source.clearSource();
			if (source.updateWordUsageCostsDynamically)
				WordClass::resetUsagePatternsAndCosts(source.debugTrace);
			else
				WordClass::resetCapitalizationAndProperNounUsageStatistics(source.debugTrace);
		}
#ifdef LOG_PATTERNS
		cPattern::printPatternStatistics();
#endif
		if (numWords)
		{
			wprintf(L"\n%d milliseconds elapsed (%d words, %d unmatched (%5.2f%%) %d overmatched (%5.2f%%)",
				(int)((clock()-overallTime)/(CLOCKS_PER_SEC/1000)),numWords,
				globalTotalUnmatched,(float)globalTotalUnmatched*100/numWords,globalOverMatchedPositionsTotal,(float)globalOverMatchedPositionsTotal*100/numWords);
			lplog(L"%d milliseconds elapsed (%d words, %d unmatched (%5.2f%%) %d overmatched (%5.2f%%)",
				(int)((clock()-overallTime)/(CLOCKS_PER_SEC/1000)),numWords,
				globalTotalUnmatched,(float)globalTotalUnmatched*100/numWords,globalOverMatchedPositionsTotal,(float)globalOverMatchedPositionsTotal*100/numWords);
			lplog();
		}
		else
			lplog(L"%d milliseconds elapsed. No words processed.",(clock()-overallTime)/(CLOCKS_PER_SEC/1000));
	}
	else
	{
		wstring path=L"tests\\"+ std::wstring(argv[sourceArgs + 1]) +L".txt";
		wstring start=L"~~BEGIN",title,etext,encoding=L"NOT FOUND";
		if (argv[sourceArgs + 2][0] == L'~')
			start = argv[sourceArgs + 2];
		int repeatStart=1;
		bool parsedOnly;
		if (forceSourceReread || !source.readSource(path, false, parsedOnly, true, true, specialExtension))
		{
			if (source.tokenize(title,etext,path,encoding, start,repeatStart,unknownCount)<0)
				exit(0);
			lplog();
			source.write(path,false, false,specialExtension);
		}
		quotationExceptions=source.doQuotesOwnershipAndContractions(totalQuotations);
		globalTotalUnmatched+=source.printSentences(false,unknownCount,quotationExceptions,totalQuotations,globalOverMatchedPositionsTotal);
		puts("");
		source.identifyObjects();
		vector <int> secondaryQuotesResolutions;
		source.analyzeWordSenses();
		source.narrativeIsQuoted = true;
		source.syntacticRelations();
		source.testSyntacticRelations();
		if (parseOnly || viterbiTest)
		{
			if (viterbiTest)
			{
				void testViterbiFromSource(Source &source);
				testViterbiFromSource(source);
			}
			source.write(path, true, false,specialExtension);
			source.writeWords(path, specialExtension);
		}
		else
		{
			source.identifySpeakerGroups();
			source.resolveSpeakers(secondaryQuotesResolutions);
			source.resolveFirstSecondPersonPronouns(secondaryQuotesResolutions);
			source.printObjects();
			source.write(path, true, false,specialExtension);
			source.printResolutionCheck(badSpeakers);
			source.logSpaceCheck();
			lplog();
		}
	}
	freeCounter();
	cProfile::lfprint(profile);
	_exit(0); // fast exit
}