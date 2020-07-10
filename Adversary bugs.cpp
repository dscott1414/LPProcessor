/*
'where' - incorrectly flagged statements
000215:ESTAB:S[mans]V[was]->AT[a state] PERTAINS TO:mans
ESTABHe[man] was evidently in a state of overmastering fear
000259:looking is not recorded as an object, yet is not an acceptable verb
He[man] stood looking at her[girl] with a kind of desperate irresolution
001377:ESTAB:S[His brown suit{OWNER:lieutenant mr thomas beresford }]V[near]->AT[the end] PERTAINS TO:His brown suit{OWNER:lieutenant mr thomas beresford }
His[tommy] brown suit was well cut , but perilously ESTABnear the end of its[end] tether . 
002459:ESTAB:V[go]->AT[home] PERTAINS TO:
// go home is an infinitive phrase
003432:ESTAB:S[credit entries a book - keeping That]V[fired]->AT[miss tuppence ] PERTAINS TO:
ESTABthey[entries,a,keeping,that...] fired me[tuppence] out 
003902:ESTAB:S[that]V[strike]->AT[lieutenant mr thomas beresford ] PERTAINS TO:
How would ESTABthat strike you[tommy] if you[tommy] read it[pay,hire,that] ?
003913:ESTAB:S[that pay hire]V[strike]->AT[lieutenant mr thomas beresford ] PERTAINS TO:
ESTABIt[that,pay,hire] would strike me[tommy] as either being a hoax , or else written by a lunatic .
004322:STAY:S[the words]V[remained] PERTAINS TO:
the words hovering on the tip of her[tuppence] tongue[tuppence] STAYremained unspoken 
005334:missed:
then opened the ESTABdoor[door] and stood aside to let her[tuppence] pass in . 
005909:missed:
Details about your[tuppence] past life in England
006797: missed:
treatment of HOBJECT - picked up as subject, but doesn't have the correct role. Probably shouldn't have relObject set?
008285: missed:
they sat in state surrounded by the many hors d'oeuvre of Tuppence's dreams.
061321: Mr. Carter is physically present 
ESTABhe[tommy] was in the presence of the man who did not here[here] go by the name[name] of “QS Mr . Carter . ” 
081012: Albert is physically present ('that worthy' refers to Albert)
that worthy made his appearance
099863: The Young Adventurers (Tuppence and Tommy) are introduced returning in a taxi. (new location), thus Jane and Julius are no longer present.

1887: brass hats is neuter - not recognized as an acceptable subject
20885: aboard - dangling preposition of 'movement' type - search for relation PrepWithPWord order by totalCount
select w.word,wr.totalCount from wordrelations wr, words w fromWordId=26323 and typeId=27 and w.id=wr.toWordId order by wr.totalCount limit 200;
then check for each object whether it is in the localObjects list, if so, suggest as possible missing object.
Julius is now aboard the train. the train is moving and so Julius is also... So delete him from possible
speakers.
23897: should be 'this visitor;, speaking but there is a problem with the speakerGroup ('where')
24227: extra speaker group - ('where')
26986: Julius should not be recorded as physically present, so he should not be in the speaker group... line 768 isFocus
27085: 'he' should be Julius, but he is in the speaker group. Consider a 'SelfAudience' flag set in identifySpeakerGroups to use in speakerResolution
28659: a smart young woman should be removed by now ('where') change in position by POV (28257)
29317: This is not resolved correctly because the speakergroup is wrong 'where' - POV Tuppence has moved to the kitchen!
29668-30453 - Tuppence should be an observer
31404: Should resolve to James ('where')
36139: FRIDAY and Saturday passed... After time has passed, all objects should become not PP. 
36242: Should refer to Tommy, not Julius - Julius should not be PP.
37292: we should not include master - master should be edgerton ('where')
39094: Should not contain the butler
55709: lift should not be a verb
46522: the others should be Julius and James
46635: 'she' is Tuppence only because she is incorrectly classified as an observer ('where')
46773: He should be undefined - not James (speaker group should be extended by 'where')
46837: the three should be all three in the speaker group
47010: to James, not to Tuppence ('where')
47252: the other should be brisk doctor, incorrectly resolved because the speaker group is not set correctly ('where')
50031: grouping is not correct ('where');
50294: Should be Julius, not Dr. ('where')
50785: 'where' - James strode away
52097: 'Tuppence went upstairs' - POV means Julius has left
52871: 'Soho' is a place and if it is marked such, and neutral, then it will help Tommy be matched to 52893:He
53342: He should be Conrad, but the speaker group is not correct (should also include 'a tall man') - 'where'
54131: Incorrect grouping causes bunch of errors - ('where') 
54553: This should be 'a tall man', but he is not in the speaker group 'where'
54416: Grouping is wrong - Tommy shouldn't be grouped
55020: Conrad really should be in the speakerGroup ('where')
55874: speakerGroup incorrectly created - ('where')
56434: Conrad should not be in speakergroup - so Conrad matching 'he' should not have 3rdPersonSpeaker -10000 penalty
57363: Tommy is 'musing' to himself - ('where')
57393: Annette ('where')
57600: The girl (should be Annette) is not mentioned in this speakerGroup and so not given high priority. ('where')
58247: Number 14 -> Tommy, not tommy to Annette ('where' - because Annette has left), also Conrad has entered. Also context.
58701: two men should be Conrad and Number 14, but new speakerGroup blocks this - ('where')
59431: Two men - should be Conrad and Number 14 - enhanced metagroup matching
58168: He should be "Mr. Brown". This is because "He was abroad" is seen as physically present ('where')
58693: the two - 'where'
60319: the men should be Conrad and the German
61832: Annette should be marked not physically present, so she is not reintroduced to the speaker group by 'sign of Annette'
66754: They should be Julius and Tommy
69231: the two - James and Julius
75268: Julius has left
75941: Tuppence is not actually physically present - Compound object list 75941 75943 becomes physically present
77854: tuppence
79272: Everyone has left besides Julius
84283: the two girls are tuppence and Jane
84634: the two figures should be tuppence and Jane
85988: girls should be Tuppence and Jane
88186: Tuppence should not be in this story!
89245: why is this story suddenly also to two other people?
89439: French is not a person!
90156: Why does the story keep accumulating more and more people?
90430: sometimes 'page' or 'pages' is not an occupation...
90862: story has an interruption from St. James
91274: 'specialist' is an occupation
91664: papers should not match to a gendered object
91928: Tuppence is talking only to James

97939: letter read by Mr. Carter, to all
BEGIN: He[mr] opened the book[book] , and turned the thin pages . 
END: Mr . Carter shut the book[book] . 

------------------------------------------------------
meta group mismatches
2707: first: should be the first listed in the list right before (now not matched).
8194: the other [room]
13552: they should not be resolved, so the girl can be Jane
16071: another man should be matched to subsequent references - pick up loose nouns! man should not be mg!
16171: Half a second is misinterpreted as a metagroup object (second)
19332: "two" should be two 'words' - use local syntax clue
18930: metagroup not Tommy
19331: or two [word]
19567: metagroup not resolved to Boris because the previous sentence is incorrectly resolved to Tommy - perhaps with weight tweaking, this might be resolved.
19612: the latter is not resolved correctly 
20071: the two should be Boris and Whittington
20143: 'another' should be taxi
20410: another ally is broken... [Julius]
20483: another man should be matched to Boris (except that it is 'another man here')
21048: the other should be Boris
22406: extend meta pattern to recogize 'first man' as 'city clerk'
23586: meta 'another' should be another 'plan'
23720: meta 'the other' should be the other 'door'
24118: our colleague should be Mr. Potter
25204: two men should be Boris and Whittington, but POV speaker is not set correctly 
26339: another 'servant'
27446: word or two of advice - two should be 'word'
28586: the lift-boy here should be Albert. This was previously matched to 'a small lift-boy' which was not removed from localObjects as quickly as Albert.
29056: the other woman should be Mrs. Vandermeyer
29685: the other shouldn't be 'Boris'
299871: the other should be Tuppence
35176: all and 'em shouldn't be matched at all
35193: these crooks should be Boris and Whittington
37002: both should be derived from you - as in 'you both'
39164: two should be cable - enhance 'two' meta object to include the pattern 'NOUN or two'
39847: the two men should be Julius and James - context
42424: a friend of mine should be Tommy
42544: the beautiful face should be Mrs. Vandermeyer
42868, 42876, 42895: meta-object this friend - should resolve to Julius
44178: the others should be everyone except for Julius and Rita
44561: them should be Julius and Tuppence - use L&L also
48877: shock (from 'one')
50320: Should be James, not Julius (latter, meta processing)
51417: another as in one another 
51455: another should be 'time' -- sometime or another
53166: the others - matched through remembering where Tommy is and his former audience
53180: one face - should be Number One, not Conrad
53748: the others (same as 53166)
54130: speakerGroup grouping is incorrect - Tommy should not be grouped (also 'where')
55984: the one [room]
56851: the latter - breakfast
59252: all others - should be all other [questions]
61826: the latter should be Mr. Carter
68014: that lawyer chap - should be Peel Edgerton - see misparse
68239: your acquaintance - should be Julius
68307: other should be edgerton not Julius
68740: lady and Jane Finn should be equivocated - but lady is a subject with two verb phrases
70031: you both
70525,70554: Julius and Tommy
75637: latter should be Mr. Carter
78295: the new-comer should be St. James
78402: us should be Mr. Carter and prime minister - a group since the prime minister didn't leave, and James is new
82136: latter should be Julius
84662: The smaller of the two should at least reference 'Two figures, hastily huddled in cloaks'
84694, 84742: Another man is NOT George
86120: incorrect grouping - Jane and Tuppence should be grouped together
88216: somehow or 'other' should not be matched to anything

misparses:
4330: nice manners should implement DEBUG 2
6434: mouse is a misparse
11300: know is a verb
17583: her is not recognized (because of quotes)
17900: lives has a mainEntry of life...
18994: misparse - no Q1
19425: sitting directly behind Whittington - have a definitive MOVE_PREP overrule (or add to) a contact or "special" verb
21526: parsing issue - _PP as SUBJECT
22169: comrade is not parsed as a HAIL - not Tommy
24046: misparse - Irish should settle, not a growing disposition
25904: Should be Annie, by META_SPEAKER_RESPONSE, but ' ouse is misparsed
25913: Many is the time - misparsed leads to 'Many' having no corresponding object
Incorrect parsing leads to incorrect attribution of secondary quote which leads to incorrect resolution of annie
(when they are talking about Rita, not Annie)
28116: Cook is being missed because of a VERBREL1 being parsed
29153: Unknown how to parse this - she would have been pleased had the guest proved to be a total stranger.
29417: herself is not resolved to anything (should be Tuppence) - because this is not an _S1
30176: England should be a place (neuter)
31361: misparse leads to IS object confusion
31934: here any
33720: medical man is not matched to 'a little black-bearded man' because of RULE 5, in the same multiple object, but RULE 5 does not apply...
39703: INFPSUB contains too many OBJECTs
41333: misparse 'off her guard' should be a prepositional phrase...
42056: a flush crept over her face - because of particle usage incorrectly reduces cost of creep having an object
43002: her is incorrectly a direct object - Mrs . Vandemeyer gazed round her fearfully .
44444: half-ashamed of herself, is not being parsed as INTRO_S1
62596: Julius is not detected as the subject, because of 'pump-handled' - He[julius] strode across , and pump - handled Tommy's hand[tommy] with what seemed to the latter quite unnecessary vigour .
66566: son-in-law
69219: Sir James's not Sir James ishas
72317: Is there any need to hurry? 'there' is parsed as an adverb, leaving 'any' and 'need' as the subject and object
74696: _PP should be 'until he' without penalty for the 'he' being an object
75423: Tommy's mind is a new __C1__S1[2]
75626- not parsed

4909: SPEAKER: Incorrect audience (should be Tommy) special 'wrote'
12722: SPEAKER: Should be Mr. Carter, not Tuppence. Tuppence nodded is not necessarily acceptable as a subject indicating the speaker of the next paragraph
15913: SPEAKER: Should be Julius, not Tommy speaking...
15996: SPEAKER: questionInversion conflict with SPEAKER: 36789
23952: audience should be to 'this visitor', and split between this visitor and Boris (2nd sentence)
24005: SPEAKER: a new voice should be assigned to Mr. Potter
24176: SPEAKER: sibilant tones should be associated with 'one'
24561: SPEAKER: 'Number 14' is introduced again by purely context within the quotes, and should be attributed as speaker.
29668: SPEAKER: Should be Vandermeyer (Tuppence should be observer)
36705: SPEAKER: consider allowing question/next subject in paragraph agreement to be processed even if
the question is not the last in its quote.
36789: SPEAKER: questionInversion conflict with SPEAKER: 15996
37587: SPEAKER: audience should be Tuppence (same as forward link, but because of alternate backwards resolution blows through a para with one sentence, it isn't.
37980: SPEAKER: split hail
37992: SPEAKER: because of split hail this is not being resolved to Julius
43467: SPEAKER: Sir James to Tuppence, NOT Rita
46327: SPEAKER: speaker guess is wrong
46420: SPEAKER: Tuppence, not Marguerite
46429: SPEAKER: Should be Tuppence talking to Rita, not Rita to Tuppence
47730: SPEAKER: split audience
48375,48418: SPEAKER: Julius should be detected from an extension of lastSubject
The little man[dr] shifted his[dr] benevolent glance[dr] to the excited young American[julius] .
52273: SPEAKER: a HAIL speaker that never speaks and is never referred to as doing anything PP - he is not actually a HAIL!
53063: SPEAKER: Should be Tommy again, to himself
53955: SPEAKER: kill him! is directed toward every one, not Tommy and so him refers to Tommy (split speaker)
54300: SPEAKER: wrong - the man is 'a tall man', not Boris
55558: SPEAKER: should be a tall man, not Tommy. This is because lastSubject is wrong.
55875, 55883, 55891: SPEAKER: Incorrect because of incorrect usage of a speaker that has not spoken before resolving backwards
57431: Tommy is talking to Conrad
58261: 14 is talking to Conrad
59968: Annette turns to Tommy.
60036: Annette should be talking to Tommy. Tommy has the only subject in the paragraph before and after, so context?
60106: Annette is talking to other people besides Tommy, who she just left ('where')
60186: Annette is talking to 'The German'
60452: Annette's voice is the last thing mentioned before the quote
60508: Annette 'words'
61255: Tommy is not talking to anyone
61462: disallow LastSubject when subject is the wrong tense or in a relative phrase? - when Tommy had finished 
61507: meta equivalence - You[mr] say you[mr] have recognized Number[number] 1 to be Kramenin ?
62148: resolved incorrectly from last subject - Tommy is correct (context)
62446: Annette, not Tuppence (context)
62873,62888: SPEAKER: Should be Tommy/Julius, not between the waiter and Tommy.
63192: SPEAKER - why is the question-response item not used to prevent Tommy?
63930: secondary quote improperly turned into a quoted string
65018, 65073: audience should be Henry, not Julius
65083: SPEAKER: explicit split hail
65739: SPEAKER: a porter, to whom Tommy addressed himself: (special pattern where the audience is 'a porter')
65848: SPEAKER: Tommy talking to the porter- context 
66384: workman is speaking, but this is only context
69421: audience should be Tommy
69884: Should be Tommy talking to James: context
70417: Should be Tommy talking to James - incorrect definite attribution caused by --
70583: split hail - actually talking to everyone else (and mentioning everyone else)
70992: Jane is talking to Julius
71042: Julius is replying to Jane
71165: Jane is talking to Julius, not Tommy talking to girl
71201: Julius is talking to Jane
72126: James speaking, determined by a meta speaker pattern outside quotes. It was Sir James who spoke .
72314: audience is James , not Julius
72548: Julius is talking to James. Confusion caused by subject in previous paragraph.
74794: Julius is speaking. 'Not so Julius' - not recognized as a subject.
78163: incorrect hail, separated by --
78315: audience should be James
81252: name in speaker - should ignore if two speakers and second quote in same paragraph and the same speaker as first quote
83023: speaker self-reference not detected: 'Even I -- Kramenin! -- ...'
83239: speaker self-reference note detected: 'all[life,revolutionists,question,chance,...] will end happily for little Julius .'
84366: Julius is talking to George, the chauffeur - The chauffeur is also the lastSubject
84616: the other fool is Tuppence
85386: Julius talking to Tuppence, not Julius talking to Annette
86243: 'both of you' should refer to Jane & Tuppence
87217: King's Cross is not a person a person's name is not owned by someone else
87345: Tuppence to Jane (Jane is still physically present)
87530: Tuppence to Jane
87959: James to Jane, not Jane to 'my child'
92049: Why is the audience suddenly 'Jane'?
92247: Jane is not the speaker - James is the speaker - fooled by lastsubject. Could override by meta object 'speaker'
92420: the girl is Jane, not Tuppence
93221: Tommy is not there!
85180: split hail
95792: dead man should be James - context
95907: that great man - James
96555: girl's should be Jane
96623: Julius told him/accused him - because this has an object it is not marked definite, but should be, and him should be marked as audience!
96943: why is she 'mother'?
97223: he should be James
97250: two should be Julius and James
97440,97567: James
99791: why is Beresford not resolved?

Manual quote-quote: 9972(9957) [QUOTE-QUOTE], 13571(13566) [QUOTE-QUOTE], (18763)18757
1568:fifth daughter of arch-deacon - recognized as an RE_OBJECT, but not being equivocated to "miss Prudence Cowley"
2317:she should be "your mother"
3230,3265: them should be people
3731: Two Young Adventurers are Tommy and Tuppence (context, or perhaps because the expression is in secondary quotes)
4430: his eyes can match 'their' in 'their glance', but it would be even better to match their glance against Whittington
4467: the young gentleman should match to Tommy
6072: What I am willing to pay for' should be a subject
6262: should resolve to Archdeacon Cowley
7250: match a HAIL object to another object if it is a nonMatchedObject for the quote? [not done] "That'll do, Brown. You can go."
7269: clerk
7571: lady should still be Tuppence
7611, 7613, 7616: context the driver is saying something not in quotes, but it is from his POV
8683: should be Whittington
10010: Y.A. Should not be Albert or Annette
12500: Mr. Carter - subject in previous sentence is an object
14748: semantic issue
15031: 'my cousin' = Jane Finn should be a persistent object!
15325: her is not resolved
15927: her should be Jane - recognize Scotland Yard to be a neuter (plural)
15992: Should be Julius, not Tommy
16089: 'inspector Brown'
16111: nouns don't have -- in the middle of them - increase cost of these nouns.
16177: 'inspector Brown'
18810: them should be 'things'
18919: Whittington, not Boris
18939: should also obey 'is,of' Whittington
19749: Should be Tommy
19770: the voice should not preferentially match to 'the voices' (incorrect nym matching)
19823: this shouldn't be resolved to Tommy.
20093: Should be Whittington
20294: he should be Tommy ('which' should be recognized as mixed plurality?)
20567: Should be tommy, not julius
20704: should only be Tommy's shoulder
21352: Possibly L&L
21761: doorkeeper - new profession?
22224: 'they' is referring to a nationality (Huns - or Germans)
22714: Bearded man should match 'a tall man' with a sharply pointed beard
23840: This visitor is incorrectly discarded due to 'an Irish Sinn feiner' being closer, but this is an alias for 'This visitor' anyway
gov'nor - a new profession?
24346: my friend should force adding speakerGroup, or something?
25263: the name " Rita " should be a special case of a name - not recognized as Rita
25877: should be Rita by context
26995: his cousin should be Jane Finn (relative relations)
27585: friend?
27838: your cousin should be matched to nothing (relative relation resolution)
28018: A smart young woman should be Annie (28194)
28194: If someone calls "Annie!" most likely the next subject in a non-quote or the next speaker is 'Annie'.
The man Tommy followed (29221) - match
18910 (command with Tuppence) - 19066, 20105, 20310, 20477, 20971
28026 is resolved to Annie, but 28207 isn't?
28586: the lift boy should be resolved to Albert
28968: the daughters should not resolve to Rita! Just Tuppence (relative relations)
29279: - Boris=the visitor, beforePreviousSpeakers not being used by imposeSpeaker...
31829: Should be Julius
35295: him should only be Julius (only PP should be allowed)
36184: they should be Tuppence and Tommy, not Tuppence and Julius
35948, 35979: 'She' is referring to 'the enormous car'
36565: Should refer to James, not Julius.
37042: here is not Tuppence
37416: She is a bad lot - the change to stop getting adjectives from neutral objects to gendered objects broke this
37457: “[tuppence:julius] I[tuppence] think perhaps I[tuppence] wouldhad better tell you[julius] the whole story , Sir James . I[tuppence] have a sort of feeling that[sort] you[julius] wouldhad know in a minute if I[tuppence] didn't tell the truth , and so you[julius] might as well know all[truth,minute,sort] about it[truth,minute,sort] from the beginning . What do you[julius] think , Julius ? ”
two hails in the same sentence - split hail
37650: him should be Mr. Carter (cata match)
37587: The lastSubject from the skipResponse should not override the mostLikely NOT speaker set previously.
38147: one person - cata res to Rita
38320: can determine only from context within the quote
38643: The audience could be deduced from the first subject of the next paragraph after a question (QXQ subject conflict)
38868: Rita
39022: He should be James
40112: She should be Tuppence, not Rita (context)
41214: the VERBREL should still limit its object L&L against the subject of the previous __S1
41828: her eyes should be Tuppence's eyes, not Rita's. Subject-object connection for body objects?
42112: should be Rita (only)
42797, 43052, 43086, 43094 should resolve to Mr. Brown
43270: Should just be Rita. But because Tuppence is not in a group, Tuppence's salience is not decreased from the match in 43266 (they)
in the final resolveSpeakers pass, because the speakerGroup (which used to have groupedSpeakers containing Tuppence) is recalculated and in the recalculation
in determineSubgroupFromGroups, groups = the size of the speakerGroup, so groupedSpeakers is zeroed.
43338,43356: woman should be Rita, not Tuppence (context?)
43468: Rita, not Tuppence (because of incorrect 43466)
43772: bright boy of yours should be Albert
43977: his should be Brown
43998: Should be Rita
44051: should be [only] Rita - from RE_OBJECT Rita, her eyes half-open,
44152: James should not be an observer
44237: context - should be Rita had surprised Tuppence
44590: boy should be Albert
44672, 44708: us should be Julius and James
44944: his should be Julius, not Albert
44956: A preference for Julius for speaker should be turned into only Julius (previous subject in same paragraph)
45016: pooh-pooh causes 'her' to not be parsed correctly
45094: improper grouping - these speaker groups should have no grouping.
45128, 45166, 45174: Mrs. Vandermeyer (speaking about an event in the past, 'her' has no antecedent in local salience)
45337: she should be Jane, not one
45562: The first part should be an answer to Tuppence, the second part is addressed to Julius
46240: prisoner should be Rita
46292,46308,46367:she should be prisoner (Rita)
47828: her should be Mrs. Vandermeyer - a question did not make the name appear physically present
48928: He should be Julius. Grouping is applied incorrectly and makes it the Dr.
49912: The young lady is NOT Rita. It is Janet.
50006: a warning glance from James, should be James, not Dr. Hall.
50099: Should be nurse, not Tuppence
50118: should be only Janet
50665: A pity came into his eyes - 'came' introducing a subject related to the object
51167: 'we' talking about James and the speaker. James is not PP.
52187: your one chance - chance should be owned by 'your' not by 'one'
52269: Tommy, Tommy is not a hail - Tommy doesn't appear outside quotes
52908: EQUIVALENCE RELATION:
He[tommy] recognized it[voice] at once for that of the bearded and efficient German[man] ,
52994: 'They' should not be matched to a singular POV - create group of two
53092: should be to Tommy's lips - context
53154: other [side]
53262,53330: Conrad (the evil-faced doorkeeper) - context - referring to bearded man and 'the man'
53563: Dear old Conrad - Conrad still hasn't said anything
53618: several different phrases before quote, but the one behind the primary has 'said', which should be lastWordOrSimplifiedRDFTypesFoundInTitleSynonyms.
53650, 53652: Conrad - context
53801: captor - new profession?
53813: the existence of 'we' and 'you' in the same sentence as subject and object should suggest another group
53980: his eyes should be [man], but it is neither object nor subject. Perhaps a new rule for RE_OBJECT, with non evaluated body objects after a speaker and ,?
54106: who is the 'working man'?
54110: Comrades should be the group other than Tommy
54402: my good fellow should be a HAIL
54455: He should be Tommy, as well as most of the rest of the paragraph
54749: Not the last subject, but a previous subject, could be used to make choice less ambiguous
54907: Audience suggestion "Made a sign 'to Conrad'" - if audience is ambiguous, take this as a hint, even if the verb has an object (a sign)
55800: waved to Conrad - another correct suggestion of the audience
56594: he should be Tommy - context
56829: tommy is talking to himself - conversations with no reply
57318: Annette shook her head should not count as a strong subject for alternateResolutionForwards (perhaps after a question?)
57341: they shouldn't be resolved at all...
57432: to Conrad - context
57460: Thomas is talking to Conrad (context)
57512: Conrad to Tommy (the man - POV is Thomas - should be Conrad, not Tommy)
57648: Conrad and Tommy should not be grouped, regardless of 'they' match at 58143.
57764: they and following theys should not resolve to anything
58110: The girl should be Annette, not Jane (gender-age matching).
58175: Grouping - annette and Conrad should be grouped together
58362: you and us, etc, should use POV or grouping to better disambiguate
58883: Conrad -> Annette (context)
59698: a small hand should be Annette
59802: Thomas should not be an observer
59681: Hail in question not processed correctly
60863: pursuers should be a gendered role object
60974: attach another kind of relative clause - his[tommy] two pursuers , of whom the German[man] was one 
61265: that should be the portait of kremanin, who should be equivocated to 'Number 1'
61336: a meta pattern announcing presence - he[tommy] was in the presence of the man who did not here[here] go by the name[name] of “QS Mr . Carter .
61451: Mr. Carter's face - context
62074: Tommy's hair - context
62106, 62124, 62132: not Annette, Tuppence - see salienceNPP
63120: should be Julius - subject in sentence of same paragraph as speaker should match
64018: Should be Julius - salienceNPP
65166: they should be Tommy and Julius
66754: They should be Julius and Tommy - why isn't this grouped?
66959: Tommy should be more salient
67087 the result was the same - 'same' should only be matched with 'result'
67309: they should be Tommy and Julius (verb in sentence should be used with gendered subjects)
67382: Tuppence should not be matched to Tommy & Julius (IS_OBJECT should not be used in an infinitive phrase which is not a BE phrase)
67970: should only be Tommy.
68228: Should be Tommy, not James
68470-68696: Talking about Jane Finn, not Tuppence
68740: should be part of a META_NAME_EQUIVALENCE
68992, 68998: James, not Tommy
69394, 69407, 69412: Tommy, not Edgerton
69482,504,535,: the girl should be Annette
70828: Should be Jane (context)
70871: the girl (should be Jane)
71082: Mother is incorrectly resolved to Mother Greenbank
71449: children should not be resolved to Tommy
72416: tone should be James's tone.
73863: little William, though a subject, and physically present, should not be a speakers because William is a gun.
75801: Tuppence
76496: she was a nurse! - she should be Jane but it is being matched NYM to nurse's as in 'a nurse's kit'
77928: A half smile (Prime Minister)
77932: other's (Prime Minister)
78230, 78244, 78245, 78251, 78263, 78281 - contextually referring to James, not 'body'
78552: photographer is cross linked with photograph because of an identity relationship between the neuter object and Annette
78886: They should be Julius and Tommy (introduction of elements from historical context)
79005: that American chap - Julius - but who is he really?
81576: Should be Tuppence only - the incorrect resolution of carter means carter is improperly asserted as being female
83368: Temporary POV switch ' The Russian[kramenin] believed him[julius] . Corrupt himself , he[julius] believed implicitly in the power of money .'
83528: Jane Finn lost salience but should still match 'the girl'
83606: the other girl should be Tuppence, not "the one you" - which matches 'you' to Kramenin
84498: POV problem. Kramenin is the POV, but suddenly it switches to Whittington seeing (84505)
84819: him should be 'Another man', not Whittington, although that's possible
85014: them should be Whittington etc, not Tommy (grouping?)
85101: only Kremanin, not Kremanin and George
85953: them should not include Tommy - mixed singular/plural pronoun
86193: split hail leads to 'you' being Jane, not Tommy
87436: taxi man should not resolve to James
88346, 88368: them should be 'papers'
88370: most of what followed (most of) should not be resolved
88641: they should be 'men' or men and Mrs Vandermeyer
88727: two of the advertising pages - 'two' should be pages
92634: his cousin should match to Jane
93363: he should be Brown/Julius (context)
098919:the three 
098986:the American (aunt miss cousin jane finn )
099015:the other boy{WO:other} (prime minister )


*/